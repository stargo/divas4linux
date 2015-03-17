/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "platform.h"
#include "dlist.h"
#include "cfg_types.h"
#include "cfglib.h"
#include "parser.h"

static
const byte* diva_cfg_lib_find_owner (diva_section_target_t target,
																		 const byte* src,
																		 dword max_length);
typedef void (*diva_cfg_enum_cback_proc_t)(void* ctxt, const byte* start, const byte* value);
static int diva_cfg_lib_enum_owner (const byte* src,
														 dword max_length,
														 diva_cfg_enum_cback_proc_t cback_proc,
														 void* context);
static int diva_cfg_lib_enum_instance (const byte* src,
																dword max_length,
																diva_cfg_enum_cback_proc_t cback_proc,
																void* context);
static void diva_cfg_lib_enum_cfg_section (const byte* src,
																		diva_cfg_enum_cback_proc_t cback_proc,
																		void* context);
static const byte* diva_cfg_get_next_owner    (const byte* src, const byte* current_owner);


static void diva_cfg_enum_cfg_sections_cback (void* ctxt, const byte* start, const byte* value);

static int diva_snprint_owner (char* dst, const byte* owner, int limit);
static int diva_snprint_instance (char* dst, const byte* instance, int limit);
static int diva_snprint_ident (char* dst, const byte* value, int limit, diva_vie_id_t id, int fmt);
static int diva_snprint_value (char* dst, const byte* value, int limit, int fmt);
static void print_full_value_name (const char* label,
																	 const byte* owner,
																	 const byte* instance,
																	 const byte* value);

static diva_section_target_t diva_get_owner_ident (const byte* owner);
static diva_hot_update_operation_result_t diva_update_hotplug_variable_using_callback (const byte* owner,
																																				const byte* instance,
																																				const byte* variable);

static int diva_cfg_lib_write_variable_update_result (const byte* value,
																											diva_hot_update_operation_result_t result);

static void print_position (diva_cfg_lib_instance_t* pLib);

static int diva_cfg_lib_verify_storage (const byte* storage);

diva_vie_tags_t* diva_cfg_find_tag_by_name (diva_cfg_lib_instance_t* pLib) {
	diva_vie_tags_t* vie_attributes = diva_cfg_get_vie_attributes();
	dword i, vie_attributes_length  = diva_cfg_get_vie_attributes_length();

	if (pLib->src[pLib->position] != '<') {
		DBG_ERR(("diva_cfg_find_tag_by_name, missing '<'"))
		return (0);
	}
	pLib->position++;

	for (i = 0; i < vie_attributes_length; i++) {
		if (!strncmp(&pLib->src[pLib->position], vie_attributes[i].tag, strlen(vie_attributes[i].tag))) {
			pLib->position += strlen(vie_attributes[i].tag);
			pLib->position += diva_cfg_ignore_unused (&pLib->src[pLib->position]);
		
			if (vie_attributes[i].attribute) {
				const char* tag_name = vie_attributes[i].tag;
				if (strncmp(&pLib->src[pLib->position], "id=\"", 4)) {
					DBG_ERR(("diva_cfg_find_tag_by_name, missing attribute for '%s' tag", vie_attributes[i].tag))
					return (0);
				}
				pLib->position += 4;
				for (i = 0; i < vie_attributes_length; i++) {
					if (!strncmp (vie_attributes[i].tag, tag_name, strlen(tag_name)) &&
							!strncmp (&pLib->src[pLib->position],
												vie_attributes[i].attribute,
												strlen(vie_attributes[i].attribute))) {
						pLib->position += strlen(vie_attributes[i].attribute);
						if (pLib->src[pLib->position] != '\"') {
							return (0);
						}
						pLib->position++;
						pLib->position += diva_cfg_ignore_unused (&pLib->src[pLib->position]);
						if (pLib->src[pLib->position] != '>') {
							return (0);
						}
						pLib->position++;
						return (&vie_attributes[i]);
					}
				}
				return (0);

			} else if (pLib->src[pLib->position] == '>') {
				pLib->position++;
				return (&vie_attributes[i]);
			} else {
				DBG_ERR(("diva_cfg_find_tag_by_name, format error by processing of '%s' tag", vie_attributes[i].tag))
				return (0);
			}
		}
	}

	return (0);
}


/*
	Read configuration file.
	Return zero on success
	Return -1 on error
	*/
int diva_cfg_lib_read_input (diva_cfg_lib_instance_t* pLib) {
	if (pLib->data) {
		DBG_ERR(("Failed: parser busy"))
		return (-1);
	}

	/*
		Calculate amount of memory necessary to save
		configuration file
		*/
	for (pLib->position = to_next_tag (pLib->src),
			 pLib->level = 0,
			 pLib->data = 0,
			 pLib->data_length = 0,
			 pLib->max_data_length = 0;
			 pLib->src[pLib->position] != 0;
			 pLib->position += to_next_tag (&pLib->src[pLib->position])) {
		diva_vie_tags_t* tag;
		if (!(tag = diva_cfg_find_tag_by_name (pLib))) {
			if (pLib->src[pLib->position]) {
				DBG_ERR(("Invalid tag at offset %lu", pLib->position))
				print_position (pLib);
				return (-1);
			} else {
				break;
			}
		}
		if (!(*(tag->read_proc))(pLib, tag)) {
			DBG_ERR(("Read error for tag '%s' at offset %lu", tag->tag, pLib->position))
			print_position (pLib);
			return (-1);
		}
	}
	DBG_REG(("check input: read %lu, wanted:%lu bytes", pLib->position, pLib->data_length))

	if (!(pLib->data = diva_os_malloc (0, pLib->data_length + 1))) {
		DBG_FTL(("Failed to alloc %lu bytes", pLib->data_length + 1))
		return (-1);
	}

	/*
		Now read data to storage
		*/
	pLib->position        = 0;
	pLib->max_data_length = pLib->data_length;
	for (pLib->position = to_next_tag (pLib->src),
			 pLib->level = 0,
			 pLib->data_length = 0;
			 pLib->src[pLib->position] != 0;
			 pLib->position += to_next_tag (&pLib->src[pLib->position])) {
		diva_vie_tags_t* tag;
		if (!(tag = diva_cfg_find_tag_by_name (pLib))) {
			if (pLib->src[pLib->position]) {
				DBG_ERR(("Invalid tag at offset %lu", pLib->position))
				print_position (pLib);
				if (pLib->data != 0)
					diva_os_free (0, pLib->data);
				pLib->data = 0;
				return (-1);
			} else {
				break;
			}
		}
		if (!(*(tag->read_proc))(pLib, tag)) {
			DBG_ERR(("Read error for tag '%s' at offset %lu", tag->tag, pLib->position))
			print_position (pLib);
			if (pLib->data != 0)
				diva_os_free (0, pLib->data);
			pLib->data = 0;
			return (-1);
		}
	}
	DBG_REG(("read input: read %lu, used:%lu, available:%lu bytes",
						pLib->position, pLib->data_length, pLib->max_data_length))


	if (pLib->data != 0 && diva_cfg_lib_verify_storage (pLib->data) != 0) {
		diva_os_free(0, pLib->data);
		pLib->data = 0;
		return (-1);
	}

	return (0);
}

static dword print_tag (byte* src) {
	diva_vie_tags_t* vie_attributes = diva_cfg_get_vie_attributes();
	dword vie_attributes_length     = diva_cfg_get_vie_attributes_length();
	dword i, used;
	dword id = vle_to_dword (src, &used);

	src += used;

	for (i = 0; i < vie_attributes_length; i++) {
		if (id == (dword)vie_attributes[i].id) {
			used += (*(vie_attributes[i].dump))(&vie_attributes[i], src);
			return (used);
		}
	}

	DBG_ERR(("print_tag: unknown tag"))

	return (0);
}

/*
	Dump and decode the context of the CFGLib storage
	*/
void diva_cfg_lib_dump_storage (const byte* src, dword data_length) {
	byte* data   = (byte*)src;
	dword i, length = data_length;

	DBG_REG(("-------------------------------------------"))
	DBG_REG(("dump length: %lu bytes", length))

	for (i = 0; i < length;) {
		dword pos;
		if (!(pos = print_tag (&data[i]))) {
			break;
		}
		i += pos;
	}

	DBG_REG(("-------------------------------------------"))
}

typedef struct _diva_find_owner_context {
	diva_section_target_t target;
	const byte* start;
} diva_find_owner_context_t;


static void diva_cmd_find_owner_cback (void* context, const byte* start, const byte* value) {
	diva_find_owner_context_t* ctxt = (diva_find_owner_context_t*)context;

	if (!ctxt->start) {
		dword length, used, total_length = 0;

		if (vle_to_dword (start, &used) != Tlie) {
			DBG_FTL(("%s, %d", __FILE__, __LINE__))
			return;
		}

		/*
			Check owner
			*/
		if (vle_to_dword (value, &length) != VieOwner) {
			DBG_FTL(("%s, %d", __FILE__, __LINE__))
			return;
		}
		value += length;
		if (vle_to_dword (value, &length) != ctxt->target) {
			return;
		}

		ctxt->start   = start;
		start        += used;
		total_length += used;
		length        = vle_to_dword (start, &used);
		total_length += used + length;
	}
}

/*
	Return pointer to begin of configuration section
	*/
static
const byte* diva_cfg_lib_find_owner (diva_section_target_t target,
																		 const byte* src,
																		 dword max_length) {
	diva_find_owner_context_t data;

	data.target = target;
	data.start  = 0;

	diva_cfg_lib_enum_owner (src, max_length, diva_cmd_find_owner_cback, &data);

	return (data.start);
}

/*
	Call user provided function for every owner element
	found in the tree
	*/
static int diva_cfg_lib_enum_owner (const byte* src,
														 dword max_length,
														 diva_cfg_enum_cback_proc_t cback_proc,
														 void* context) {
	dword id, used, tlie_length, position = 0;
	diva_vie_tags_t* tag;

	if ((vle_to_dword (&src[position], &used) != Tlie) || !used || (used >= max_length)) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	position   += used;
	max_length -= used;

	if (!(tlie_length = vle_to_dword (&src[position], &used)) || !used || (used >= max_length)) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	position   += used;
	max_length -= used;
	if (tlie_length > max_length) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	max_length = tlie_length;

	for (max_length = tlie_length; max_length != 0;) {
		id = vle_to_dword (&src[position], &used);
		if (!(tag = find_tag_from_id (id)) || !used || (used >= max_length)) {
			DBG_FTL(("%s : %d", __FILE__, __LINE__))
			return (-1);
		}
		if (tag->id == Tlie) {
			diva_cfg_lib_enum_owner (&src[position], max_length, cback_proc, context);
		} else if (tag->id == VieOwner) {
			(*cback_proc)(context, src, &src[position]);
		}
		if (!(used = (*(tag->skip))(&src[position], max_length)) || (used > max_length)) {
			DBG_FTL(("%s : %d", __FILE__, __LINE__))
			return (-1);
		}
		position   += used;
		max_length -= used;
	}

	return (0);
}

/*
	Call user provided function for every instance element
	found in the tree
	*/
static int diva_cfg_lib_enum_instance (const byte* src,
																dword max_length,
																diva_cfg_enum_cback_proc_t cback_proc,
																void* context) {
	dword id, used, tlie_length, position = 0;
	diva_vie_tags_t* tag;

	if ((vle_to_dword (&src[position], &used) != Tlie) || !used || (used >= max_length)) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	position   += used;
	max_length -= used;

	if (!(tlie_length = vle_to_dword (&src[position], &used)) || !used || (used >= max_length)) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	position   += used;
	max_length -= used;
	if (tlie_length > max_length) {
		DBG_FTL(("%s : %d", __FILE__, __LINE__))
		return (-1);
	}
	max_length = tlie_length;

	for (max_length = tlie_length; max_length != 0;) {
		id = vle_to_dword (&src[position], &used);
		if (!(tag = find_tag_from_id (id)) || !used || (used >= max_length)) {
			DBG_FTL(("%s : %d", __FILE__, __LINE__))
			return (-1);
		}
		if (tag->id == Tlie) {
			diva_cfg_lib_enum_instance (&src[position], max_length, cback_proc, context);
		} else if ((tag->id == VieInstance) || (tag->id == VieInstance2)) {
			(*cback_proc)(context, src, &src[position]);
		}
		if (!(used = (*(tag->skip))(&src[position], max_length)) || (used > max_length)) {
			DBG_FTL(("%s : %d", __FILE__, __LINE__))
			return (-1);
		}
		position   += used;
		max_length -= used;
	}

	return (0);
}

typedef struct _find_instance {
	dword nr;
	const byte* name;
	dword name_length;
	const byte* instance;
} find_instance_t;

static void diva_cmd_find_instance_cback (void* context, const byte* start, const byte* value) {
	find_instance_t *ctxt = (find_instance_t*)context;

	if (!ctxt->instance) {
		dword length, used, total_length = 0;

		if (vle_to_dword (start, &used) != Tlie) {
			DBG_FTL(("%s, %d", __FILE__, __LINE__))
			return;
		}

		/*
			Check owner
			*/
		switch (vle_to_dword (value, &length)) {
			case VieInstance:
				if (ctxt->name || (vle_to_dword (&value[length], &length) != ctxt->nr)) {
					return;
				}
				break;

			case VieInstance2:
				if (ctxt->name) {
					dword len;

					value += length;
					len = vle_to_dword (value, &length);
					value += length;
					if (ctxt->name_length != len) {
						return;
					}
					if (memcmp (ctxt->name, value, len)) {
						return;
					}
				} else {
					return;
				}
				break;

			default:
				DBG_FTL(("%s, %d", __FILE__, __LINE__))
				return;
		}

		ctxt->instance   = start;
		start        += used;
		total_length += used;
		length        = vle_to_dword (start, &used);
		total_length += used + length;
	}
}

/*
	Find section that address required instance

	In case name is non zero then look by name in
	opposite case look by number.
	*/
const byte* diva_cfg_lib_find_instance (dword nr,
																				const byte* name,
																				dword name_length,
																				const byte* src,
																				dword max_length) {
	find_instance_t info;

	info.nr          = nr;
	info.name        = name;
	info.name_length = name_length;
	info.instance    = 0;

	diva_cfg_lib_enum_instance (src,
															max_length,
															diva_cmd_find_instance_cback,
															&info);

	return (info.instance);
}

typedef struct _enum_section_owner_context {
	const byte* current_owner;
	const byte* owner;
} enum_section_owner_context_t;

static void diva_cmd_enum_owner_cback (void* context, const byte* start, const byte* value) {
	enum_section_owner_context_t* ctxt = (enum_section_owner_context_t*)context;

	if (!ctxt->owner) {
		if (start > ctxt->current_owner) {
			ctxt->owner = start;
		}
	}
}

/*
	Enumerate section owners
	*/
static const byte* diva_cfg_get_next_owner (const byte* src, const byte* current_owner) {
	enum_section_owner_context_t info;
	dword used, length, max_length = 0;

	if (vle_to_dword (src, &used) != Tlie) {
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
		return (0);
	}
	max_length += used;
	if (!(length = vle_to_dword (&src[used], &used))) {
		return (0);
	}
	max_length += used + length;

	info.current_owner = current_owner;
	info.owner         = 0;

	diva_cfg_lib_enum_owner (src, max_length, diva_cmd_enum_owner_cback, &info);

	return (info.owner);
}

typedef struct _enum_section_instance_context {
	const byte* current_instance;
	const byte* instance;
} enum_section_instance_context_t;

static void diva_cmd_enum_instance_cback (void* context, const byte* start, const byte* value) {
	enum_section_instance_context_t* ctxt = (enum_section_instance_context_t*)context;

	if (!ctxt->instance) {
		if (start > ctxt->current_instance) {
			ctxt->instance = start;
		}
	}
}

/*
	Enumerate instances
	*/
const byte* diva_cfg_get_next_instance (const byte* src, const byte* current_section) {
	enum_section_instance_context_t info;
	dword used, length, max_length = 0;

	if (vle_to_dword (src, &used) != Tlie) {
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
		return (0);
	}
	max_length += used;
	if (!(length = vle_to_dword (&src[used], &used))) {
		return (0);
	}
	max_length += used + length;

	info.current_instance = current_section;
	info.instance         = 0;

	diva_cfg_lib_enum_instance (src, max_length, diva_cmd_enum_instance_cback, &info);

	return (info.instance);
}

typedef struct _cfg_compare_instance {
	diva_cfg_lib_instance_t* pLib;
	const byte* org_owner;
	dword				org_length;
	const byte* new_owner;   /* Points to entire owner section */
	dword       new_owner_length;
	const byte* owner_ident; /* Identifies owner */
} cfg_compare_instance_t;

typedef struct _diva_ucf_cmp_cfg_sections_context {
	diva_cfg_lib_instance_t* pLib;
	const byte*							 org_instance;
	dword										 org_instance_max_length;

  const byte*              new_owner;
  const byte*              owner_ident;
  const byte*              new_instance;
  const byte*              instance_ident;

	int                      update_result;

	int											 changes_detected;
} diva_ucf_cmp_cfg_sections_context_t;

/*
	Check is some variables was removed from the original instance.
	Only named variables are affected by this operation

	Returns true if some of variables was removed and hot update notification
	for this variable was rejected
	*/
static int diva_cmd_detect_removed_variables (const byte* owner,
																							const byte* instance,
																							const byte* ref,
																							int* changes_detected) {
	const byte* var = 0, *variable_name, *instance_ident = 0;
	dword used, variable_name_length, data_length, instance_ident_length = 0;
	dword instance_nr = 0, position = 0, owner_ident, value;
	int ret = 0, instance_by_name;
	int adapter = 0;

	(void)vle_to_dword (owner, &used);
	owner_ident = vle_to_dword (&owner[used], &used);

	adapter = ((TargetXdiAdapter    == ((diva_section_target_t)owner_ident)) ||
						 (TargetSoftIPAdapter == ((diva_section_target_t)owner_ident)));

	(void)vle_to_dword (ref, &used); /* tlie tag */
	position += used;
	vle_to_dword (ref+position, &used); /* total length */
	position += used;

	value = vle_to_dword (ref+position, &used); /* instance handle type */
	position += used;

	if (value == VieInstance2) {
		instance_by_name = 1;
		instance_ident_length = diva_get_bs  (ref+position, &instance_ident);
	} else {
		instance_by_name = 0;
		instance_nr = vle_to_dword (ref+position, &used);
	}

	while ((var = diva_cfg_storage_enum_variable (0, ref, var, 0, 0, 0, 0,
																			&variable_name, &variable_name_length)) != 0) {
		if (diva_cfg_find_named_variable (instance,
																			variable_name, variable_name_length,
																			&data_length) == 0) {
			if (diva_cfg_get_section_element (var, VieManagementInfo) != 0) {
				if (adapter == 0) {
					ret |= (diva_cfg_lib_call_variable_notify_callback (owner_ident,
																											instance_by_name,
																											instance_ident,
																											instance_ident_length,
																											instance_nr,
																											var,
																											0) != HotUpdateOK);
				} else {
					*changes_detected = 1;
				}
			} else {
				ret = 1;
			}
		}
	}

	return (ret);
}

/*
	Write named variables to management interface of XDI adapter
	*/
static int diva_cmd_update_named_xdi_variables (const byte* owner,
																								const byte* instance) {
	diva_section_target_t target;
	dword used;
	int ret = 0;

	(void)vle_to_dword (owner, &used);
	target = (diva_section_target_t)vle_to_dword (&owner[used], &used);

	if (target == TargetXdiAdapter || target == TargetSoftIPAdapter) {
		ret = diva_cfg_lib_update_named_xdi_variables (instance);
	}

	return (ret);
}

/*
	Called for every instance found in the owner section found
	in new configuration data.

	start points now to instance section in new configuration data
	*/
static void diva_cmd_compare_instance_cback (void* context, const byte* start, const byte* value) {
	const byte* instance_ident = value;
	cfg_compare_instance_t* info = (cfg_compare_instance_t*)context;
	dword used, section_type = vle_to_dword (value, &used);
	dword instance_name_length = 0, instance_nr = 0;
	const byte* instance_name  = 0, *org_instance;
	int changes_detected = 0;

	if (info->pLib->parse_error) {
		return;
	}

	value += used;
	if (section_type == VieInstance) {
		instance_nr = vle_to_dword (value, &used);
	} else if (section_type == VieInstance2) {
		instance_name_length = vle_to_dword (value, &used);
		instance_name = value + used;
	} else {
		DBG_FTL(("Wrong section type: %lu (%s,%d)", section_type, __FILE__, __LINE__))
		info->pLib->parse_error = 1;
		return;
	}

	if (info->org_owner) {
		org_instance = diva_cfg_lib_find_instance (instance_nr,
																							 instance_name,
																							 instance_name_length,
																							 info->org_owner,
																							 info->org_length);
	} else {
		org_instance = 0;
		changes_detected = 1;
	}
	if (instance_name) {
		DBG_REG((" Start to compare %sVieInstance2 name length: %lu",
							org_instance ? "" : "New ", instance_name_length))
		DBG_BLK(((byte*)instance_name, instance_name_length))
	} else {
		DBG_REG((" Start to compare %sVieInstance: %lu", org_instance ? "" : "New ", instance_nr))
	}

	if (org_instance) {
		dword max_org_instance_length = get_max_length (org_instance);
		diva_ucf_cmp_cfg_sections_context_t ctxt;
		diva_hot_update_operation_result_t update_result;

		/*
			Compare context of every found configuration parameter
			*/

		ctxt.pLib                    = info->pLib;
		ctxt.org_instance            = org_instance;
		ctxt.org_instance_max_length = max_org_instance_length;

    ctxt.new_owner      = info->new_owner;
		ctxt.owner_ident    = info->owner_ident;
		ctxt.new_instance   = start;
		ctxt.instance_ident = instance_ident;

		ctxt.update_result  = 0;

		ctxt.changes_detected = 0;

		if (instance_name) {
			if (diva_cfg_lib_response_open_instance_by_name (instance_name, instance_name_length)) {
				info->pLib->parse_error = 1;
				return;
			}
		} else {
			if (diva_cfg_lib_response_open_instance (instance_nr)) {
				info->pLib->parse_error = 1;
				return;
			}
		}

		diva_cfg_lib_enum_cfg_section (start, diva_cfg_enum_cfg_sections_cback, &ctxt);

		update_result = (ctxt.update_result != 0) ? (int)HotUpdateFailedNotPossible : 0;

		if (add_string_to_response_buffer ("\t\t<vie id=\"response\">")) {
			info->pLib->parse_error = 1;
			return;
    }

		/*
			Check for removed variables
			*/
		if (diva_cmd_detect_removed_variables (info->owner_ident, start, org_instance, &changes_detected) != 0) {
			ctxt.update_result = 1;
		}

		/*
			Write all named variables to management interface of XDI adapter
			if any changes in the adapter configuration
			*/
		changes_detected |= ctxt.changes_detected;

		if (changes_detected != 0 && diva_cmd_update_named_xdi_variables (info->owner_ident, start) != 0) {
			ctxt.update_result = -1;
		}

		if (ctxt.update_result) {
			/*
				Update using external interface failed.
				Try to notify application using configuration change notification.
				*/
			update_result = diva_cfg_lib_call_notify_callback (diva_get_owner_ident (ctxt.owner_ident),
																												 start,
																												 get_max_length (start),
																												 get_response_buffer_position());
		}

		/*
			Write cumulative result of update operation
			*/
		{
			byte buffer[24];

			diva_snprintf (&buffer[0], sizeof(buffer), "%d</vie>\n", (int)update_result);

			if (add_string_to_response_buffer (&buffer[0])) {
				info->pLib->parse_error = 1;
				return;
			}
		}

		if (diva_cfg_lib_response_close_instance ()) {
			info->pLib->parse_error = 1;
			return;
		}
	} else {
		diva_ucf_cmp_cfg_sections_context_t ctxt;
		diva_hot_update_operation_result_t update_result;

		/*
			Process new instance
			*/
		if (instance_name) {
			if (diva_cfg_lib_response_open_instance_by_name (instance_name, instance_name_length)) {
				info->pLib->parse_error = 1;
				return;
			}
		} else {
			if (diva_cfg_lib_response_open_instance (instance_nr)) {
				info->pLib->parse_error = 1;
				return;
			}
		}

		ctxt.pLib                    = info->pLib;
		ctxt.org_instance            = 0;
		ctxt.org_instance_max_length = 0;

    ctxt.new_owner      = info->new_owner;
		ctxt.owner_ident    = info->owner_ident;
		ctxt.new_instance   = start;
		ctxt.instance_ident = instance_ident;

		ctxt.update_result  = 0;

		ctxt.changes_detected = 0;

		diva_cfg_lib_enum_cfg_section (start, diva_cfg_enum_cfg_sections_cback, &ctxt);

		update_result = (ctxt.update_result != 0) ? (int)HotUpdateFailedNotPossible : 0;

		if (add_string_to_response_buffer ("\t\t<vie id=\"response\">")) {
			info->pLib->parse_error = 1;
			return;
    }
		if (diva_cmd_update_named_xdi_variables (info->owner_ident, start) != 0) {
			ctxt.update_result = -1;
		}

		if (ctxt.update_result) {
			/*
				Update using external interface failed.
				Try to notify application using configuration change notification.
				*/
			update_result = diva_cfg_lib_call_notify_callback (diva_get_owner_ident (ctxt.owner_ident),
																												 start,
																												 get_max_length (start),
																												 get_response_buffer_position());
		}

		/*
			Write cumulative result of update operation
			*/
		{
			byte buffer[24];

			diva_snprintf (&buffer[0], sizeof(buffer), "%d</vie>\n", update_result);

			if (add_string_to_response_buffer (&buffer[0])) {
				info->pLib->parse_error = 1;
				return;
			}
		}

		if (diva_cfg_lib_response_close_instance ()) {
			info->pLib->parse_error = 1;
			return;
		}
	}
}

typedef struct _cfg_compare_owner {
	diva_cfg_lib_instance_t* pLib;
	const byte* new;
	dword new_length;
	const byte* org;
	dword org_length;
} cfg_compare_owner_t;

/*
	Routine called for all owners that are was found in new configuration file.
	Routine will try to find the owner in existing configuration file.
	In case like owner was found then context of both sections is to be compared.
	In case like owner was not found then hot update procedure is invoked fo this
	owner.
	*/
static void diva_cmd_compare_owner_cback (void* context, const byte* start, const byte* value) {
	const byte* owner_ident = value;
	cfg_compare_owner_t* info = (cfg_compare_owner_t*)context;
	const char* org_owner;
	diva_section_target_t id;
	dword used;

	if (info->pLib->parse_error) {
		return;
	}

	/*
		Get owner ID
		*/
	(void)vle_to_dword (value, &used);
	id = (diva_section_target_t)vle_to_dword (&value[used], &used);

	/*
		Look for same owner inside of original configuration section
		*/
	if ((org_owner = diva_cfg_lib_find_owner (id, info->org, info->org_length))) {
		/*
			Found, necessary to compare context of both sections (e.g. instances)
			*/
		cfg_compare_instance_t instance_info;
		dword new_owner_length = get_max_length(start), org_length = get_max_length(org_owner);

		DBG_REG(("Start to compare section owned by %d", id))

		instance_info.pLib             = info->pLib;
		instance_info.org_owner        = org_owner;
		instance_info.org_length       = org_length;
		instance_info.new_owner        = start;
		instance_info.new_owner_length = new_owner_length;
		instance_info.owner_ident      = owner_ident;

		if (diva_cfg_lib_response_open_owner (id)) {
			info->pLib->parse_error = 1;
			return;
		}

		diva_cfg_lib_enum_instance (start, new_owner_length, diva_cmd_compare_instance_cback, &instance_info);

		if (diva_cfg_lib_response_close_owner ()) {
			info->pLib->parse_error = 1;
			return;
		}
	} else {
		cfg_compare_instance_t instance_info;
		dword new_owner_length = get_max_length(start);

		/*
			Not found, this is new section. This is necessary to apply hot update
			to entire section
			*/

		if (diva_cfg_lib_response_open_owner (id)) {
			info->pLib->parse_error = 1;
			return;
		}

		DBG_REG(("New section owned by %d", id))

		instance_info.pLib             = info->pLib;
		instance_info.org_owner        = 0;
		instance_info.org_length       = 0;
		instance_info.new_owner        = start;
		instance_info.new_owner_length = new_owner_length;
		instance_info.owner_ident      = owner_ident;

		diva_cfg_lib_enum_instance (start, new_owner_length, diva_cmd_compare_instance_cback, &instance_info);

		if (diva_cfg_lib_response_close_owner ()) {
			info->pLib->parse_error = 1;
			return;
		}
	}
}

/*
	INTERFACE
	Compare two configurations, apply hot plug procedure to every changed variable
	*/
int diva_cfg_compare (diva_cfg_lib_instance_t* pLib, const byte* org, const byte* new) {
	dword org_max_length = get_max_length(org), new_max_length = get_max_length(new);
	cfg_compare_owner_t info;

	if (!(org_max_length && new_max_length)) {
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
		return (-1);
	}

	pLib->parse_error       = 0;

	info.pLib = pLib;
	info.new  = new;
	info.new_length = new_max_length;
	info.org  = org;
	info.org_length = org_max_length;

	if (diva_cfg_lib_open_response ()) {
		pLib->parse_error = 1;
	} else {
		diva_cfg_lib_enum_owner (new, new_max_length, diva_cmd_compare_owner_cback, &info);

		if (pLib->parse_error == 0) {
			if (diva_cfg_lib_close_response ()) {
				pLib->parse_error = 1;
			}
		}
	}

	return (pLib->parse_error ? (-1) : 0);
}

/*
	Call user provided cback function for all found in
	current section configuration elements
	*/
static void diva_cfg_lib_enum_cfg_section (const byte* src,
																	 diva_cfg_enum_cback_proc_t cback_proc,
																	 void* context) {
	const byte* section = 0;

	while ((section = diva_cfg_get_next_cfg_section (src, section))) {
		(*cback_proc)(context, section, 0);
	}
}

/*
	This function is called for every found inside of new instance data
	configuration section
	*/
static void diva_cfg_enum_cfg_sections_cback (void* context, const byte* start, const byte* value) {
	diva_ucf_cmp_cfg_sections_context_t* info = (diva_ucf_cmp_cfg_sections_context_t*)context;
	diva_vie_id_t id = diva_cfg_get_section_type (start);
	diva_cfg_section_t* ref;
	const byte* org;
	int apply_changes = 1;
	diva_hot_update_operation_result_t operation_result = HotUpdateNoChangesDetected;

	if ((id <= IeNotDefined) || (id >= VieLlast) || (!(ref = identify_section (id)))) {
		DBG_ERR(("Ignore cfg section (%d) %s at %d", id, __FILE__, __LINE__))
		return;
	}
	if (id == VieBindingDirection) {
		/*
			Bindings are always updated on demand
			*/
		print_full_value_name ("Binding:", info->owner_ident, info->instance_ident, start);
		return;
	}

	if (info->org_instance) {
		if ((org = (*(ref->find))(start, info->org_instance))) {
			apply_changes = (*(ref->cmp))(start, org);
		}
	} else {
		apply_changes = 1;
	}

	if (apply_changes) {
		info->changes_detected = 1;
		print_full_value_name ("Changed:",     info->owner_ident, info->instance_ident, start);
		operation_result = diva_update_hotplug_variable (info->owner_ident, info->instance_ident, start);

		switch (operation_result) {
			case HotUpdateOK:
			case HotUpdateNoChangesDetected:
				break;
			default:
				operation_result = diva_update_hotplug_variable_using_callback (info->owner_ident, info->instance_ident, start);
		}

	} else {
		print_full_value_name ("Not changed:", info->owner_ident, info->instance_ident, start);
	}

	switch (operation_result) {
		case HotUpdateOK:
		case HotUpdateNoChangesDetected:
			break;

		default:
			info->update_result = 1;
	}

	if (diva_cfg_lib_write_variable_update_result (start, operation_result)) {
		info->pLib->parse_error = 1;
	}
}

static diva_section_target_t diva_get_owner_ident (const byte* owner) {
	dword used, id;

	(void)vle_to_dword (owner, &used);
	id = (diva_section_target_t)vle_to_dword (&owner[used], &used);

	return ((diva_section_target_t)id);
}

static int diva_snprint_owner (char* dst, const byte* owner, int limit) {
	dword used, id;

	(void)vle_to_dword (owner, &used);
	id = (diva_section_target_t)vle_to_dword (&owner[used], &used);

	return (diva_snprintf (dst, limit, "owner(%lu)", id));
}

static int diva_snprint_instance (char* dst, const byte* instance, int limit) {
	dword used;

	if (vle_to_dword (instance, &used) == VieInstance) {
		return (diva_snprintf (dst, limit, "instance(%lu)", vle_to_dword (instance + used, &used)));
	} else {
		const byte* data;
		dword data_length = diva_get_bs  (instance+used, &data);
		int written;

		written = diva_snprintf (dst, limit, "instance2(");
		limit  -= written;
		dst    += written;
		if (limit > (int)data_length) {
			memcpy (dst, data, data_length);
			limit   -= data_length;
			dst     += data_length;
			written += data_length;
		}
		return (written + diva_snprintf (dst, limit, ")"));
	}
}


static int diva_snprint_ident (char* dst, const byte* value, int limit, diva_vie_id_t id, int fmt) {
	diva_vie_tags_t* vie_attributes = diva_cfg_get_vie_attributes();
	dword vie_attributes_length     = diva_cfg_get_vie_attributes_length();
	const byte* element;
	int written = 0;

	if ((element = diva_cfg_get_section_element (value, id))) {
		dword used;
		int i;

		for (i = 0; i < (int)vie_attributes_length; i++) {
			if (vie_attributes[i].id == id) {
				(void)vle_to_dword (element, &used);
				if (vie_attributes[i].is_read_vle_vie != 0) {
					if (fmt) {
						written += diva_snprintf (&dst[written],
																			limit - written,
																			"\t\t\t<vie id=\"%s\">%lx</vie>\n",
																	 		vie_attributes[i].attribute,
																	 		vle_to_dword (element+used, &used));
					} else {
						written += diva_snprintf (&dst[written], limit - written, ",%s=%lx",
																	 		vie_attributes[i].attribute,
																	 		vle_to_dword (element+used, &used));
					}
				} else if (vie_attributes[i].is_read_vle_bs != 0) {
					const byte* data;
					dword data_length = diva_get_bs  (element+used, &data);
					int label = 0, is_ascii=1;
					if (fmt) {
						written += diva_snprintf (&dst[written],
																			limit - written,
																			"\t\t\t<vie id=\"%s\">",
																			vie_attributes[i].attribute);
					} else {
						written += diva_snprintf (&dst[written], limit - written, ",%s[%lu]={",
																			vie_attributes[i].attribute,
																			data_length);
					}
					{
						/*
							Check if context can be printed as ascii string
							*/
						int i;

						for (i = 0; ((i < (int)data_length) && is_ascii); i++) {
							is_ascii = ((data[i] >= 0x20) && (data[i] <= 0x7e) && data[i] != '<' && data[i] != '>');
						}
					}

					if (fmt) {
						written += diva_snprintf (&dst[written], limit - written, "%s", is_ascii ? "<as>" : "<bs>");
						if (is_ascii) {
							written++;
						}
					}

					if (is_ascii) {
						written--;
						while (((limit - written) > 5) && data_length--) {
							dst[written++] = *data++;
						}
						written += diva_snprintf (&dst[written], limit - written, "");
					} else {
						while (((limit - written) > 5) && data_length--) {
							written += diva_snprintf (&dst[written], limit - written, "%s%02x",
							label++ ? "," : "", *data++);
						}
						if (fmt == 0) {
							written += diva_snprintf (&dst[written], limit - written, "}");
						}
					}

					if (fmt) {
						written += diva_snprintf (&dst[written], limit - written, "%s", is_ascii ? "</as>" : "</bs>");
						written += diva_snprintf (&dst[written], limit - written, "%s", "</vie>\n");
					}
				}

				break;
			}
		}
	}

	return (written);
}

static int diva_snprint_value (char* dst, const byte* value, int limit, int fmt) {
	diva_vie_id_t id = diva_cfg_get_section_type (value);
	diva_cfg_section_t* ref;
	int written = 0, i;
	const byte* element;

	if ((id <= IeNotDefined) || (id >= VieLlast) || (!(ref = identify_section (id)))) {
		if (fmt) {
			return (diva_snprintf (dst, limit, "=NotDefined"));
		} else {
			return (0);
		}
	}

	if (fmt == 0) {
		written += diva_snprintf (&dst[written], limit - written, "%s", ref->name);
	}

	written += diva_snprint_ident (&dst[written], value, limit - written, id, fmt);

	for (i = 0, id = ref->required[i];
			 ((ref->required[i] < VieLlast) && (ref->required[i] > IeNotDefined)); id = ref->required[++i]) {
		if ((element = diva_cfg_get_section_element (value, id))) {
			written += diva_snprint_ident (&dst[written], value, limit - written, id, fmt);
		}
	}

	if (ref->optional) {
		for (i = 0, id = ref->optional[i];
			 	((ref->optional[i] < VieLlast) && (ref->optional[i] > IeNotDefined)); id = ref->optional[++i]) {
			if ((element = diva_cfg_get_section_element (value, id))) {
				written += diva_snprint_ident (&dst[written], value, limit - written, id, fmt);
			}
		}
	}

	return (written);
}

static void print_full_value_name (const char* label,
																	 const byte* owner,
																	 const byte* instance,
																	 const byte* value) {
	byte buffer[512];
	int limit = sizeof(buffer) - 1;
	int	written = 0;

	written += diva_snprintf (&buffer[written], limit-written, "%s", label);
	if (owner) {
		written += diva_snprint_owner (&buffer[written], owner, limit - written);
	}
	if (instance) {
		written += diva_snprint_instance (&buffer[written], instance, limit - written);
	}
	written += diva_snprint_value (&buffer[written], value, limit - written, 0);

	DBG_TRC(("%s", buffer))
}


const byte* diva_cfg_parser_get_owner_configuretion (diva_cfg_lib_instance_t* pLib, dword owner) {
	const byte* powner = 0;

	if (pLib && pLib->storage /* && (((diva_section_target_t)owner) < TargetLast) */) {
		dword max_length = get_max_length (pLib->storage);

		powner = diva_cfg_lib_find_owner ((diva_section_target_t)owner, pLib->storage, max_length);
	}

	return (powner);
}

/*
		Determine type if management interface variable:
		 0  - signed type
		 1  - unsigned type
		-1 - string type
	*/
int diva_cfg_check_management_variable_value_type (byte var_type) {
	static byte signed_types[] = {
		0x81 /* MI_INT */
	};
	static byte unsigned_types[] = {
		0x82, /* MI_UINT */
		0x83, /* MI_HINT */
		0x87, /* MI_BITFLD */
		0x85, /* MI_BOOLEAN */
		0x88 /* MI_SPID_STATE */
	};
	unsigned int i;

	for (i = 0; i < sizeof(signed_types)/sizeof(signed_types[0]); i++) {
		if (var_type == signed_types[i]) {
			return (0);
		}
	}

	for (i = 0; i < sizeof(unsigned_types)/sizeof(unsigned_types[0]); i++) {
		if (var_type == unsigned_types[i]) {
			return (1);
		}
	}

	return (-1);
}

/*
	Update variable using notification callback and partial configuration change notification
	*/
static diva_hot_update_operation_result_t diva_update_hotplug_variable_using_callback (const byte* owner,
																																											 const byte* instance,
																																											 const byte* variable) {
	diva_hot_update_operation_result_t ret = HotUpdateFailedNotPossible;

	if (diva_cfg_get_section_element (variable, VieManagementInfo) != 0) {
		diva_section_target_t target;
		dword used, instance_nr = 0, instance_ident_length = 0;
		const byte *instance_ident = 0;
		int instance_by_name = 0;


		/*
			Retrieve information about section owner
			*/
		(void)vle_to_dword (owner, &used);
		target = (diva_section_target_t)vle_to_dword (&owner[used], &used);

		/*
			Retrieve instance identifier
			*/
		if (vle_to_dword (instance, &used) == VieInstance) {
			instance_nr   = vle_to_dword (instance + used, &used);
		} else {
			instance_ident_length = diva_get_bs (instance+used, &instance_ident);
			instance_by_name = 1;
		}

		ret = diva_cfg_lib_call_variable_notify_callback ((dword)target,
																											instance_by_name,
																											instance_ident,
																											instance_ident_length,
																											instance_nr,
																											variable,
																											1);
	}

	return (ret);
}

static int diva_cfg_lib_write_variable_update_result (const byte* value,
																											diva_hot_update_operation_result_t result) {
	diva_vie_id_t id = diva_cfg_get_section_type (value);
	diva_cfg_section_t* ref;
	byte buffer[512];
	int written = 0, limit = sizeof(buffer) - 1;

	if ((id <= IeNotDefined) || (id >= VieLlast) || (!(ref = identify_section (id)))) {
		return (-1);
	}

	if (add_string_to_response_buffer ("\t\t<tlie>\n")) {
		return (-1);
	}

	written += diva_snprint_value (&buffer[written], value, limit - written, 1);

	if (add_string_to_response_buffer (&buffer[0])) {
		return (-1);
	}

	written = 0;
	written += diva_snprintf (&buffer[written],
														limit - written,
														"\t\t\t<vie id=\"response\">%lu</vie>\n",
														(dword)result);

	if (add_string_to_response_buffer (&buffer[0])) {
		return (-1);
	}

	if (add_string_to_response_buffer ("\t\t</tlie>\n")) {
		return (-1);
	}

	return (0);
}

/* ------------------------------------------------------------------------
		Functions used to access bindings
   ------------------------------------------------------------------------ */
/*
	Enumerate bindings

	INPUT:
		storage          - pointer to the storage
		application      - section owner
    instance_ident,
    instance_name,
    instance_length  - instance
    section          - pointer to current binding or zero

  RETURN:
    pointer to first binding section in case section parameter is zero
    pontrer to next binding section
    zero in case no binding section was found or end of list was reached
	*/
const byte* diva_cfg_get_binding (const byte* storage,
																	diva_section_target_t application,
																	dword	instance_ident,
																	const byte* instance_name,
																	dword instance_name_length,
																	const byte* section) {
	dword max_length = get_max_length (storage);
	const byte* owner;
	const byte* binding = 0;

	if ((owner = diva_cfg_lib_find_owner (application, storage, max_length))) {
		dword max_length = get_max_length (owner);
		const byte* instance = diva_cfg_lib_find_instance (instance_ident,
																											 instance_name,
																											 instance_name_length,
																											 owner,
																											 max_length);
		if (instance) {
			while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
				if (diva_cfg_get_section_type (section) == VieBindingDirection) {
					binding = section;
					print_full_value_name ("Found binding:", 0, 0, section);
					break;
				}
			}
		}
	}

	return (binding);
}


/*
	Move stream pointer over unused characters
	*/
dword diva_cfg_ignore_unused (const char* src) {
	dword i;

	for (i = 0;;i++) {
		switch (src[i]) {
			case '\n':
			case '\r':
			case  ' ':
			case '\t':
				break;
			default:
				return (i);
		}
	}

	return (i);
}

dword to_next_tag (const char* src) {
	dword i;

	for (i = 0; ((src[i] != 0) && (src[i] != '<')); i++);

	return (i);
}

/*
	Print data before and after the
	current position
	*/
static void print_position (diva_cfg_lib_instance_t* pLib) {
	dword length = MIN(pLib->position, 256);

	DBG_TRC(("------------"))
	DBG_BLK(((char*)&pLib->src[pLib->position - length], length))

	length = MIN((strlen(pLib->src) - pLib->position), 256);
	DBG_TRC(("|<%c>|", (char)pLib->src[pLib->position]))

	DBG_BLK(((char*)&pLib->src[pLib->position], length))
	DBG_TRC(("------------"))
}

/*
	Verify if all variables contains all mandatory information elements
	returns zero on success
	returns -1 on error
	*/
static int diva_cfg_lib_verify_storage (const byte* storage) {
	dword nr_targets = 0, nr_instances = 0, nr_sections = 0, nr_elements = 0;
	const byte* owner = 0;

	while ((owner = diva_cfg_get_next_owner (storage, owner)) != 0) {
		const byte* instance = 0;

		nr_targets++;

		while ((instance = diva_cfg_get_next_instance (owner, instance)) != 0) {
			const byte* section = 0;

			nr_instances++;

			while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
				diva_vie_id_t id = diva_cfg_get_section_type (section);
				diva_cfg_section_t* section_description;

				nr_sections++;

				if (id == IeNotDefined) {
					DBG_FTL(("Can not identify section"))
					return (-1);
				}

				if ((section_description = identify_section (id)) == 0) {
					DBG_FTL(("Can not identify section, id=%d", id))
					return (-1);
				}
				if (section_description->required != 0) {
					int i;

					for (i = 0; section_description->required[i] != VieLlast; i++) {
						nr_elements++;
						if (diva_cfg_get_section_element (section, section_description->required[i]) == 0) {
							DBG_FTL(("Missing mandatory element, id=%d", section_description->required[i]))
							return (-1);
						}
					}
				}
			}
		}
	}

	DBG_REG(("Verified targets:%d, instances:%d, sections:%d, elements:%d", 
						nr_targets, nr_instances, nr_sections, nr_elements))

	return (0);
}

