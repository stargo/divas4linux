
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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
#include "cfg_types.h"
#include "cfg_notify.h"
#include "cfg_ifc.h"
#include "dlist.h"

typedef struct _diva_um_instance {
	diva_entity_link_t link;
	const byte* instance_data;
} diva_um_instance_t;

typedef struct _diva_um_cfg_lib_instance {
	diva_cfg_lib_access_ifc_t ifc;
	int initialized;
	diva_entity_queue_t instance_q;
	diva_cfg_lib_cfg_notify_callback_proc_t callback_proc;
	void* callback_context;
} diva_um_cfg_lib_instance_t;

static diva_um_cfg_lib_instance_t diva_um_cfg_lib_instance;

void diva_um_get_callback_proc (diva_cfg_lib_access_ifc_t* ifc,
																diva_cfg_lib_cfg_notify_callback_proc_t* proc,
																void** context) {
	diva_um_cfg_lib_instance_t* instance = (diva_um_cfg_lib_instance_t*)ifc;

	*proc = instance->callback_proc;
	*context = instance->callback_context;
}

static diva_um_instance_t* find_instance (diva_um_cfg_lib_instance_t* instance,
																					diva_vie_id_t ident_type,
																					const byte* ident,
																					dword ident_length,
																					dword ident_nr);

static void diva_cfg_lib_free_cfg_interface(diva_cfg_lib_access_ifc_t* ifc) {
	diva_um_cfg_lib_instance_t* instance = (diva_um_cfg_lib_instance_t*)ifc;
	diva_um_instance_t* instance_data    = (diva_um_instance_t*)diva_q_get_head(&instance->instance_q);

	if (instance->initialized) {
		while ((instance_data    = (diva_um_instance_t*)diva_q_get_head(&instance->instance_q)) != 0) {
			diva_q_remove   (&instance->instance_q, &instance_data->link);
			diva_os_free (0, (void*)instance_data->instance_data);
			diva_os_free (0, instance_data);
		}
		memset (instance, 0x00, sizeof(*instance));
	}
}

void* diva_cfg_lib_cfg_register_notify_callback (diva_cfg_lib_cfg_notify_callback_proc_t callback_proc,
																								 void* callback_context,
																								 dword owner) {
	diva_um_cfg_lib_instance_t* instance = &diva_um_cfg_lib_instance;

	if (instance->initialized != 0 && instance->callback_proc == 0 && callback_proc != 0) {
		diva_um_instance_t* instance_data;

		instance->callback_proc = callback_proc;
		instance->callback_context = callback_context;

		for (instance_data = (diva_um_instance_t*)diva_q_get_head(&instance->instance_q);
				 instance_data != 0;
				 instance_data = (diva_um_instance_t*)diva_q_get_next(&instance_data->link)) {
			/*
				Call callback function but ignore response in case of initial callback
				*/
			(*(instance->callback_proc))(instance->callback_context, 0, instance_data->instance_data);
		}

		return (&instance->callback_proc);
	}

	return (0);
}

int diva_cfg_lib_cfg_remove_notify_callback (void* handle) {
	diva_um_cfg_lib_instance_t* instance = &diva_um_cfg_lib_instance;

	if (handle != 0 && instance->initialized != 0 && instance->callback_proc != 0 &&
			handle == (void*)&instance->callback_proc) {
		instance->callback_proc = 0;
		instance->callback_context = 0;

		return (0);
	}

	return (-1);
}

/*
  Function always works on locked storage or on read only copy
  of instance data. No lock is necessary.
  */
static diva_cfg_lib_return_type_t
diva_cfg_storage_read_instance_cfg_var(struct _diva_cfg_lib_access_ifc* ifc,
																			 const byte* instance_data,
																			 const char* name,
																			 diva_cfg_lib_value_type_t value_type,
																			 dword* data_length,
																			 void*  dst) {
	dword var_length;
  const byte* pvar = diva_cfg_find_named_variable (instance_data, (const byte*)name,
																									 strlen(name), &var_length);
	if (pvar) {
		dword max_var_length = *data_length;
		byte* pdata = (byte*)dst;

		if (pdata == 0) {
			*data_length = var_length;
			return (DivaCfgLibOK);
		}

		if (max_var_length == 0)
			return (DivaCfgLibParameter);

		if (value_type == DivaCfgLibValueTypeASCIIZ)
			var_length++; /* include trailing zero */

		*data_length = var_length;

		if (max_var_length < var_length)
			return (DivaCfgLibBufferTooSmall);

		switch (value_type) {
			case DivaCfgLibValueTypeASCIIZ:
				if (var_length - 1)
					memcpy (pdata, pvar, var_length - 1);
				pdata[var_length - 1] = 0;
				break;

			case DivaCfgLibValueTypeBinaryString:
				memcpy (pdata, pvar, var_length);
				break;

			case DivaCfgLibValueTypeBool:
			case DivaCfgLibValueTypeSigned:
			case DivaCfgLibValueTypeUnsigned:
				if (var_length <= 4) {
					dword val, i;

					for (i = 0, val = 0; i < var_length; i++) {
						val |= pvar[i] << i*8;
					}
					if (value_type == DivaCfgLibValueTypeBool)
						val = val != 0;

					switch (max_var_length) {
						case 1:
							pdata[0] = (byte)val;
							break;
						case 2:
							WRITE_WORD(pdata, ((word)val));
							break;
						case 4:
							WRITE_DWORD(pdata, val);
							break;
						default:
							return (DivaCfgLibWrongType);
					}
				}
				break;

			default:
				return (DivaCfgLibWrongType);
		}

		return (DivaCfgLibOK);
	}

	return (DivaCfgLibNotFound);
}

static diva_cfg_lib_return_type_t
diva_cfg_storage_get_adapter_cfg_info (struct _diva_cfg_lib_access_ifc* ifc,
                                       const   byte* instance_data,
                                       dword   offset_after,
                                       dword*  ram_offset,
                                       pcbyte* data,
                                       dword*  data_length) {
  const byte* section = 0;
  dword section_offset;

  while ((section = diva_cfg_get_next_ram_init_section (instance_data, section)) != 0) {
    section_offset = diva_cfg_get_ram_init_offset (section);
    if (offset_after == 0 || section_offset > offset_after) {
      *ram_offset  = section_offset;
      *data_length = diva_cfg_get_ram_init_value (section,data);

      return (DivaCfgLibOK);
    }
  }

  return (DivaCfgLibNotFound);
}

/*
	Enumerate variables which math specified template
	*/
const byte* diva_cfg_storage_enum_variable (struct _diva_cfg_lib_access_ifc* ifc,
																						const   byte* instance_data,
																						const   byte* current_var,
																						const   byte* _template,
																						dword   template_length,
																						pcbyte* key,
																						dword*  key_length,
																						pcbyte* name,
																						dword*  name_length) {
	const byte* element, *variable_name;
	dword used, variable_name_length;

	while ((current_var = diva_cfg_get_next_cfg_section (instance_data, current_var)) != 0) {
		if (diva_cfg_get_section_type (current_var) == VieNamedVarName) {
			if ((element = diva_cfg_get_section_element (current_var, VieNamedVarName)) != 0) {
				(void)vle_to_dword (element, &used);
				variable_name_length = diva_get_bs (element+used, &variable_name);

				if (variable_name_length > template_length &&
						(template_length == 0 || memcmp (variable_name, _template, template_length) == 0)) {
					const byte* p = variable_name + template_length;
					dword len;

					if (template_length == 0) {
						if (key)
							*key = variable_name;
						if (key_length)
							*key_length = variable_name_length;
						if (name)
							*name = variable_name;
						if (name_length)
							*name_length = variable_name_length;
						return (current_var);
					} else if (*p != '\\') {
						for (len = 0; len + template_length < variable_name_length; len++) {
							if (p[len] == '\\') {
								if(key != 0)
									*key = p;
								if (key_length != 0)
									*key_length = len;
								if (name != 0)
									*name = &p[len+1];
								if (name_length)
									*name_length = variable_name_length - template_length - len - 1;
								return (current_var);
							}
						}
					}
				}
			}
		}
	}

	return (0);
}

static
const byte* diva_cfg_storage_find_variable (struct _diva_cfg_lib_access_ifc* ifc,
                                            const byte* instance_data,
                                            const byte* name,
                                            dword name_length) {
  const byte* section = 0, *element, *variable_name;
  dword used, variable_name_length;

  while ((section = diva_cfg_get_next_cfg_section (instance_data, section)) != 0) {
    if (diva_cfg_get_section_type (section) == VieNamedVarName) {
      if ((element = diva_cfg_get_section_element (section, VieNamedVarName)) != 0) {
        (void)vle_to_dword (element, &used);
        variable_name_length = diva_get_bs (element+used, &variable_name);

        if (variable_name_length == name_length &&
            memcmp(variable_name, name, variable_name_length) == 0) {
          return (section);
        }
      }
    }
  }

  return (0);
}

/*
  Retrieve data section of named variable
  */
static const byte* diva_cfg_lib_get_named_var_data(const byte* variable, dword* data_length) {
  const byte* element = diva_cfg_get_section_element (variable, VieNamedVarValue);
  const byte* data = 0;

  if (element != 0) {
    dword used;
    (void)vle_to_dword (element, &used);
    *data_length = diva_get_bs  (element+used, &data);
  }

  return (data);
}

#define diva_cfg_implement_read_value(__type__) static diva_cfg_lib_return_type_t \
  diva_cfg_lib_read_##__type__##_value ( const byte* variable, \
                                        __type__*   value) { \
  dword data_length; \
  const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length); \
  if (data != 0) { \
    if (data_length <= sizeof(__type__)) { \
      memset (value, 0x00, sizeof(*value)); \
      memcpy (value, data, data_length);    \
      return (DivaCfgLibOK); \
    } \
    return (DivaCfgLibBufferTooSmall); \
  } \
  return (DivaCfgLibWrongType); \
}

diva_cfg_implement_read_value(byte)
diva_cfg_implement_read_value(char)
diva_cfg_implement_read_value(short)
diva_cfg_implement_read_value(word)
diva_cfg_implement_read_value(int)
diva_cfg_implement_read_value(dword)

static diva_cfg_lib_return_type_t diva_cfg_lib_read_64bit_value (const byte* variable,
                                                                 void* value) {
  dword data_length;
  const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

  if (data != 0) {
    if (data_length <= 8) {
      memset (value, 0x00, 8);
      memcpy (value, data, data_length);
      return (DivaCfgLibOK);
    }
    return (DivaCfgLibBufferTooSmall);
  }

  return (DivaCfgLibWrongType);
}

/*
  Read binary string value
  */
static diva_cfg_lib_return_type_t diva_cfg_lib_read_binary_string (const byte* variable,
                                                                   void* value,
                                                                   dword* length,
                                                                   dword max_length) {
  dword data_length;
  const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

  if (data != 0) {
    if (data_length <= max_length) {
      memset (value, 0x00, max_length);
      memcpy (value, data, data_length);
      *length = data_length;
      return (DivaCfgLibOK);
    }
    return (DivaCfgLibBufferTooSmall);
  }

  return (DivaCfgLibWrongType);
}

/*
  Read ascii zero terminated string
  Max length includes terminating zero
  */
static diva_cfg_lib_return_type_t diva_cfg_lib_read_asciiz_string (const byte* variable,
                                                                   char* value,
                                                                   dword max_length) {
  dword data_length;
  const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

  if (data != 0) {
    int zero_present = (data_length && data[data_length-1] == 0);
    dword length = zero_present ? data_length : (data_length+1);

    if (length <= max_length) {
      memset (value, 0x00, max_length);
      memcpy (value, data, data_length);
      return (DivaCfgLibOK);
    }
    return (DivaCfgLibBufferTooSmall);
  }
  return (DivaCfgLibWrongType);
}

/*
  Allows access to variable data
  */
static const byte* diva_cfg_lib_get_variable_data (const byte* variable, dword* length) {
  return (diva_cfg_lib_get_named_var_data(variable,length));
}

/*
  Retrieve information about section ident
  */
static diva_cfg_lib_return_type_t
diva_cfg_lib_get_instance_ident (const byte* instance,
                                 diva_vie_id_t* ident_type,
                                 pcbyte* ident,
                                 dword*  ident_length,
                                 dword*  ident_nr) {
  dword used, value, position = 0;

  (void)vle_to_dword (instance, &used); /* tlie tag */
  position += used;
  vle_to_dword (instance+position, &used); /* total length */
  position += used;

  value = vle_to_dword (instance+position, &used); /* instance handle type */
  position += used;

  if (value == VieInstance2) {
    *ident_type = VieInstance2;
    if (ident && ident_length) {
      *ident_length = diva_get_bs  (instance+position, ident);
      if (ident_nr)
        *ident_nr = 0;
      return (DivaCfgLibOK);
    }
    return (DivaCfgLibWrongType);
  } else if (value == VieInstance) {
    *ident_type = VieInstance;
    if (ident_nr != 0) {
      *ident_nr = vle_to_dword (instance+position, &used);
      if (ident_length)
        *ident_length = 0;
      if (ident)
        *ident        = 0;
      return (DivaCfgLibOK);
    }
    return (DivaCfgLibWrongType);
  }

  return (DivaCfgLibNotFound);
}

static const byte* diva_cfg_lib_get_instance (struct _diva_cfg_lib_access_ifc* ifc,
																							diva_section_target_t target,
																							int instance_by_name,
																							const byte* instance_ident,
																							dword instance_ident_length,
																							dword instance_nr) {
	diva_um_cfg_lib_instance_t* instance = (diva_um_cfg_lib_instance_t*)ifc;

	if (instance->initialized != 0) {
		diva_um_instance_t* saved_instance_data =
																	find_instance (instance,
																								 instance_by_name != 0 ? VieInstance2 : VieInstance,
																								 instance_ident,
																								 instance_ident_length,
																								 instance_nr);
		if (saved_instance_data != 0) {
			dword length = get_max_length (saved_instance_data->instance_data);
			const byte* instance_data = (byte*)diva_os_malloc (0, length + sizeof(void*));

			if (instance_data != 0) {
				memset ((void*)instance_data, length, sizeof(void*));
				memcpy ((void*)instance_data, saved_instance_data->instance_data, length);

				return (instance_data);
			}
		}
	}

	return (0);
}

static void diva_cfg_lib_release_instance (struct _diva_cfg_lib_access_ifc* ifc, const byte* instance) {
	if (instance != 0) {
		diva_os_free (0, (void*)instance);
	}
}

/*
  Retrieve information about variable ident
  */
static diva_cfg_lib_return_type_t
diva_cfg_lib_get_name_ident (const byte* variable, pcbyte* ident, dword* ident_length) {
  const byte* name = diva_cfg_get_section_element (variable, VieNamedVarName);

  if (name != 0) {
    dword used;

    (void)vle_to_dword (name, &used);
    *ident_length = diva_get_bs (name+used, ident);

    return (DivaCfgLibOK);
  }

  return (DivaCfgLibWrongType);
}

static const byte* diva_cfg_lib_copy_instance (const byte* instance) {
	dword length = get_max_length(instance);
	byte* mem = (byte*)diva_os_malloc (0, length+sizeof(void*));

	if (mem != 0) {
		memset (&mem[length], 0x00, sizeof(void*));
		memcpy (mem, instance, length);
	}

	return (mem);
}

/* -----------------------------------------------------------
		INTERFACE: provides CfgLib interface to user
   ----------------------------------------------------------- */
struct _diva_cfg_lib_access_ifc*
diva_um_cfg_lib_get_cfg_interface (dword owner) {
	static diva_cfg_lib_access_ifc_t ifc_ref =
                   { sizeof(ifc_ref),
                     0, /* version */
                     0, /* storage */
                     0, /* context */
                     diva_cfg_lib_free_cfg_interface,
                     0, /* diva_cfg_storage_read_cfg_var */
                     diva_cfg_lib_cfg_register_notify_callback,
                     diva_cfg_lib_cfg_remove_notify_callback,
                     diva_cfg_storage_read_instance_cfg_var,
                     0, /* diva_cfg_storage_get_image_info */
                     diva_cfg_storage_get_adapter_cfg_info,
                     diva_cfg_storage_enum_variable,
                     diva_cfg_storage_find_variable,
                     diva_cfg_lib_read_char_value,
                     diva_cfg_lib_read_byte_value,
                     diva_cfg_lib_read_short_value,
                     diva_cfg_lib_read_word_value,
                     diva_cfg_lib_read_int_value,
                     diva_cfg_lib_read_dword_value,
                     diva_cfg_lib_read_64bit_value,
                     diva_cfg_lib_read_binary_string,
                     diva_cfg_lib_read_asciiz_string,
                     diva_cfg_lib_get_variable_data,
                     diva_cfg_lib_get_instance,
                     diva_cfg_lib_release_instance,
                     diva_cfg_lib_get_instance_ident,
                     diva_cfg_lib_get_name_ident,
                     diva_cfg_lib_copy_instance };
	if (diva_um_cfg_lib_instance.initialized == 0) {
		memset (&diva_um_cfg_lib_instance, 0x00, sizeof(diva_um_cfg_lib_instance));
		memcpy (&diva_um_cfg_lib_instance.ifc, &ifc_ref, sizeof(diva_um_cfg_lib_instance.ifc));
		diva_um_cfg_lib_instance.ifc.cfg_lib_instance_context = &diva_um_cfg_lib_instance;
		diva_um_cfg_lib_instance.initialized = 1;

		return (&diva_um_cfg_lib_instance.ifc);
	}

	return (0);
}

static diva_um_instance_t* find_instance (diva_um_cfg_lib_instance_t* instance,
																					diva_vie_id_t ident_type,
																					const byte* ident,
																					dword ident_length,
																					dword ident_nr) {
	diva_um_instance_t* saved_instance_data;

	for (saved_instance_data = (diva_um_instance_t*)diva_q_get_head(&instance->instance_q);
			 saved_instance_data != 0;
			 saved_instance_data = (diva_um_instance_t*)diva_q_get_next(&saved_instance_data->link)) {
		diva_vie_id_t instance_ident_type = IeNotDefined;
		const byte* instance_ident  = 0;
		dword instance_ident_length = 0;
		dword instance_ident_nr     = 0;

		if (diva_cfg_lib_get_instance_ident (saved_instance_data->instance_data,
																				 &instance_ident_type,
																				 &instance_ident,
																				 &instance_ident_length,
																				 &instance_ident_nr) == DivaCfgLibOK &&
				ident_type == instance_ident_type) {

			if (ident_type == VieInstance) {
				if (ident_nr == instance_ident_nr) {
					return (saved_instance_data);
				}
			} else {
				if (ident_length == instance_ident_length && memcmp (ident, instance_ident, ident_length) == 0) {
					return (saved_instance_data);
				}
			}
		}
	}

	return (0);
}

/*
	INTERFACE: receive information about cfg lib and provide this information to user.
	*/
dword diva_um_cfg_lib_message_input (struct _diva_cfg_lib_access_ifc* ifc,
																	 const byte* instance_data) {
	diva_um_cfg_lib_instance_t* instance = (diva_um_cfg_lib_instance_t*)ifc;
	diva_um_instance_t* saved_instance_data;
	dword ret = HotUpdateFailedOwnerNotFound;
	diva_vie_id_t ident_type = IeNotDefined;
	const byte* ident  = 0;
	dword ident_length = 0;
	dword ident_nr     = 0;

	if (diva_cfg_lib_get_instance_ident (instance_data,
																			 &ident_type,
																			 &ident,
																			 &ident_length,
																			 &ident_nr) != DivaCfgLibOK) {
		diva_os_free (0, (void*)instance_data);
		return (ret);
	}

	if ((saved_instance_data = find_instance (instance, ident_type, ident, ident_length, ident_nr)) != 0) {
		diva_os_free (0, (void*)saved_instance_data->instance_data);
	} else {
		if ((saved_instance_data = (diva_um_instance_t*)diva_os_malloc (0, sizeof(*saved_instance_data))) == 0) {
			diva_os_free (0, (void*)instance_data);
			return (ret);
		}
		memset (saved_instance_data, 0x00, sizeof(*saved_instance_data));
		diva_q_add_tail (&instance->instance_q, &saved_instance_data->link);
	}
	saved_instance_data->instance_data = instance_data;

	if (instance->callback_proc != 0) {
		if ((*(instance->callback_proc))(instance->callback_context, 0, instance_data) != 0)
			ret = HotUpdateFailedNotPossible;
		else
			ret = HotUpdateOK;
	}

	return (ret);
}


