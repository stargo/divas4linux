
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2004-2017
  Copyright 2000-2003 by Armin Schindler (mac@melware.de)
  Copyright 2000-2003 Cytronics & Melware (info@melware.de)

 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of Adapters.
 *
  File Revision :    2.1
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
#include "debug.h"
#include "cfg_types.h"
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
#include "dlist.h"
#include "cfglib.h"
#include "parser.h"
#include "pc_init.h"
#endif

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__ /* { */
static const byte* find_pcinit_section (const byte* ref, const byte* instance);
static const byte* find_ram_section    (const byte* ref, const byte* instance);
static const byte* find_var_section    (const byte* ref, const byte* instance);
static const byte* find_image_section  (const byte* ref, const byte* instance);
static const byte* find_binding_section (const byte* ref, const byte* instance) { return (0); }

static int cmp_pcinit_section (const byte* ref, const byte* instance);
static int cmp_ram_section    (const byte* ref, const byte* instance);
static int cmp_var_section    (const byte* ref, const byte* instance);
static int cmp_image_section  (const byte* ref, const byte* instance);
static int cmp_binding_section  (const byte* ref, const byte* instance) { return (1); }
static dword diva_read_bs (const char* src, byte* dst, dword* length);
static dword str_to_vle   (const char* src, byte* vle, dword* length);
static dword dword_to_vle (dword value, byte* vle, dword max_length);
static byte diva_cfg_get_cfg_line_number (const byte* src);
#endif /* } */

/*
	Optional and required elements
	*/
static diva_vie_id_t pcinit_required[]    = { ViePcInitValue,    VieLlast };
static diva_vie_id_t pcinit_optional[]    = { VieParameterLine,  VieLlast };
static diva_vie_id_t raminit_required[]   = { VieRamInitValue,   VieRamInitOperation, VieLlast };
static diva_vie_id_t raminit_optional[]   = { VieParameterLine,  VieLlast };

static diva_vie_id_t namedvar_required[]  = { VieNamedVarValue,  VieLlast };
static diva_vie_id_t binding_required[]   = { VieBindingEnable,  VieLlast };
static diva_vie_id_t imagetype_required[] = { VieImageName,      VieLlast };
static diva_vie_id_t imagetype_optional[] = { VieImageArchive,   VieImageData,  VieLlast };
static diva_vie_id_t binding_optional[]   = { VieBindingService, VieBindingHLC, VieBindingLLC,
																							VieBindingRangeStart, VieBindingRangeEnd, VieBindingBindTo,
																							VieBindingBindTo2, VieBindingRangeStartCiPN,
																							VieBindingRangeEndCiPN,
																							VieBindingRangeStartSub, VieBindingRangeEndSub,
																							VieBindingRangeStartCiPNSub, VieBindingRangeEndCiPNSub,
																							VieLlast };

/*
	Management interface value 'VieManagementInfo' is optional for every section
	so it is not necessary to include it in every list with optional
	parameters.
	Moreover management interface processing information is not considered
	by configuration change check procedure, also it is better do not include
	this information in any if lists below.
	*/
static diva_cfg_section_t sections[] = {
  { ViePcInitname,         "PCINIT",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		find_pcinit_section,  cmp_pcinit_section,
#endif
		pcinit_required,     pcinit_optional },
  { VieRamInitOffset,      "RAM",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		find_ram_section,     cmp_ram_section,
#endif
		raminit_required,    raminit_optional },
  { VieNamedVarName,       "VAR",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		find_var_section,     cmp_var_section,
#endif
		namedvar_required,   0 },
	{ VieImageType,          "IMAGE",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		find_image_section,   cmp_image_section,
#endif
		imagetype_required,  imagetype_optional },
	{ VieBindingDirection,   "BINDING",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		find_binding_section, cmp_binding_section,
#endif
		binding_required,    binding_optional }
};

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__ /* { */
static dword read_vie_vle (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag);
static dword read_vie_bs  (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag);
static dword read_tlie    (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag);
static dword dump_tlie    (struct _diva_vie_tags* tag, byte* src);
static dword dump_vie_vle (struct _diva_vie_tags* tag, byte* src);
static dword dump_vie_bs  (struct _diva_vie_tags* tag, byte* src);
static int close_tag (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag);
#endif /* } */
static dword skip_tlie    (const byte* src, dword max_length);
static dword skip_vie_vle (const byte* src, dword max_length);
static dword skip_vie_bs  (const byte* src, dword max_length);

/*
	Attributes for VIE elements

	Shorter elements should follow longer elements.
	*/
static diva_vie_tags_t vie_attributes[] ={
	{ "tlie",  Tlie,     0,
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_tlie,    dump_tlie,
#endif
		skip_tlie, 0, 0 },
	{ "vie",  VieOwner, "owner",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieInstance2, "instance2",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0,  1 },
	{ "vie", VieInstance, "instance",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", ViePcInitname, "pcinitname",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", ViePcInitValue, "pcinitvalue",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1  },
	{ "vie", VieRamInitOffset, "raminitoffset",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieRamInitOperation, "raminitoperation",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieRamInitValue, "raminitvalue",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieImageType, "imagetype",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieImageName, "imagename",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1  },
	{ "vie", VieImageArchive, "imagearchive",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieImageData,    "imagedata",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieNamedVarName, "namedvarname",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieNamedVarValue, "namedvarvalue",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieManagementInfo, "managementinfo",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },


	{ "vie", VieBindingRangeStartCiPNSub, "bindingrangestartcipnsub",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeEndCiPNSub,   "bindingrangeendcipnsub",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeStartCiPN,    "bindingrangestartcipn",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeStartSub,     "bindingrangestartsub",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeEndCiPN,      "bindingrangeendcipn",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeEndSub,       "bindingrangeendsub",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingServiceMask,       "bindingservicemask",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingRangeStart,        "bindingrangestart",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingDirection,         "bindingdirection",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieBindingRangeEnd,          "bindingrangeend",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingService,           "bindingservice",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingEnable,            "bindingenable",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieBindingBindTo2,           "bindingto2",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1  },
	{ "vie", VieBindingHLC,               "bindinghlc",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingLLC,               "bindingllc",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingBC,                "bindingbc",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_bs,  dump_vie_bs,
#endif
		skip_vie_bs, 0, 1 },
	{ "vie", VieBindingBindTo,            "bindingto",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 },
	{ "vie", VieParameterLine,            "parameterline",
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		read_vie_vle, dump_vie_vle,
#endif
		skip_vie_vle, 1, 0 }
};

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
diva_vie_tags_t* diva_cfg_get_vie_attributes(void) {
	return (&vie_attributes[0]);
}
int diva_cfg_get_vie_attributes_length(void) {
	return (sizeof(vie_attributes)/sizeof(vie_attributes[0]));
}
#endif


/*
	Get length of TLIE section
	*/
dword get_max_length (const byte* src) {
	dword max_length = 0, length, used;

	if (!src || (vle_to_dword (src, &used) != Tlie)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
#endif
		return (0);
	}
	max_length += used;
	if (!(length = vle_to_dword (&src[used], &used))) {
		return (0);
	}
	max_length += used + length;

	return (max_length);
}


/*
	Converts VLE variable to dword
	'pos' receives the amount of bytes read
	from input stream
	*/
dword vle_to_dword (const byte* vle, dword* pos) {
	dword v = 0, i = 0;

	*pos = 0;

	do {
		v |= (*vle & 0x7f) << i++ * 7;
		(*pos)++;
	} while (*vle++ & 0x80);


	return (v);
}

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__ /* { */

/*
	Find PCINIT section using reference PCINIT section
	This is only one element we wants to compare: ViePcInitname
	*/
static const byte* find_pcinit_section (const byte* ref, const byte* instance) {
	const byte* name = diva_cfg_get_section_element (ref, ViePcInitname);
	const byte* section = 0;

	if (name) {
		dword used;
		dword new_pcinit, new_cfg_line;

		(void)vle_to_dword (name, &used);
		new_pcinit = vle_to_dword (name + used, &used);

		if ((name = diva_cfg_get_section_element (ref, VieParameterLine))) {
			(void)vle_to_dword (name, &used);
			new_cfg_line = vle_to_dword (name + used, &used);
		} else {
			new_cfg_line = 0;
		}

		while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
			dword org_cfg_line;

			if ((name = diva_cfg_get_section_element (section, VieParameterLine))) {
				(void)vle_to_dword (name, &used);
				org_cfg_line = vle_to_dword (name + used, &used);
			} else {
				org_cfg_line = 0;
			}

			if ((org_cfg_line == new_cfg_line) &&
					(name = diva_cfg_get_section_element (section, ViePcInitname))) {
				dword org_pcinit;
				(void)vle_to_dword (name, &used);
				org_pcinit = vle_to_dword (name + used, &used);

				if (org_pcinit == new_pcinit) {
					break;
				}
			}
		}
	}

	return (section);
}

/*
	Find RAM init section. To identify this type configuration section this is necessary to process
  as follows:
  1 - Find RAM section with same offset
  2 - Check if RAM operation is compatible
	*/
static const byte* find_ram_section    (const byte* ref, const byte* instance) {
	const byte* name = diva_cfg_get_section_element (ref, VieRamInitOffset);
	const byte* section = 0;

	if (name) {
		dword used, ref_offset, ref_cfg_line;

		(void)vle_to_dword (name, &used);
		ref_offset = vle_to_dword (name+used, &used);

		if ((name = diva_cfg_get_section_element (ref, VieParameterLine))) {
			(void)vle_to_dword (name, &used);
			ref_cfg_line = vle_to_dword (name+used, &used);
		} else {
			ref_cfg_line = 0;
		}

		if ((name = diva_cfg_get_section_element (ref, VieRamInitOperation))) {
			dword ref_operation;

			(void)vle_to_dword (name, &used);
			ref_operation = vle_to_dword (name+used, &used);

			while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
				if ((name = diva_cfg_get_section_element (section, VieRamInitOffset))) {
					dword org_offset, org_cfg_line;

					(void)vle_to_dword (name, &used);
					org_offset = vle_to_dword (name+used, &used);

					if ((name = diva_cfg_get_section_element (section, VieParameterLine))) {
						(void)vle_to_dword (name, &used);
						org_cfg_line = vle_to_dword (name+used, &used);
					} else {
						org_cfg_line = 0;
					}

					if ((org_offset == ref_offset) && (org_cfg_line == ref_cfg_line) &&
							(name = diva_cfg_get_section_element (section, VieRamInitOperation))) {
						dword org_operation;

						(void)vle_to_dword (name, &used);
						org_operation = vle_to_dword (name+used, &used);

						if ((ref_operation == RamInitOperationWrite) || (org_operation == RamInitOperationWrite)) {
							if (ref_operation == org_operation) {
								return (section);
							}
						} else {
							/*
								This is bit/set and bit or bit clear operation
								so this is necessary to compare bit masks
								*/
							dword ref_value_length, org_value_length;
							const byte* ref_value, *org_value;

							if ((name = diva_cfg_get_section_element (ref, VieRamInitValue))) {
								(void)vle_to_dword (name, &used);
								ref_value_length = diva_get_bs (name+used, &ref_value);

								if ((name = diva_cfg_get_section_element (section, VieRamInitValue))) {
									(void)vle_to_dword (name, &used);
									org_value_length = diva_get_bs (name+used, &org_value);

									if ((ref_value_length == org_value_length) &&
											(memcmp (ref_value, org_value, ref_value_length) == 0)) {
										return (section);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return (section);
}

/*
	Variable is identified using variable name (what is binary string)
	*/
static const byte* find_var_section (const byte* ref, const byte* instance) {
	const byte* name = diva_cfg_get_section_element (ref, VieNamedVarName);
	const byte* section = 0;

	if (name) {
		dword used, ref_name_length;
		const byte* ref_name;

		(void)vle_to_dword (name, &used);
		ref_name_length = diva_get_bs (name+used, &ref_name);

		while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
			dword org_name_length;
			const byte* org_name;

			if ((name = diva_cfg_get_section_element (section, VieNamedVarName))) {
				(void)vle_to_dword (name, &used);
				org_name_length = diva_get_bs (name+used, &org_name);

				if ((ref_name_length == org_name_length) &&
						(memcmp (ref_name, org_name, ref_name_length) == 0)) {
					return (section);
				}
			}
		}
	}

	return (section);
}

/*
	Image section contains following variables:

  VieImageType    - VIE, required
	VieImageName    - BS,  required
	VieImageArchive - BS,  optional
	VieImageData    - BS,  optional

	To search for same image this is necessary to look for: VieImageType, VieImageName and VieArchiveName
	*/
static const byte* find_image_section  (const byte* new, const byte* instance) {
	const byte* name = diva_cfg_get_section_element (new, VieImageType);
	const byte* section = 0;

	if (name) {
		dword used, new_type;

		(void)vle_to_dword (name, &used);
		new_type = vle_to_dword (name+used, &used);

		if ((name = diva_cfg_get_section_element (new, VieImageName))) {
			dword new_name_length, new_archive_name_length = 0;
			const byte* new_name, *new_archive_name = 0;

			(void)vle_to_dword (name, &used);
			new_name_length = diva_get_bs  (name+used, &new_name);

			if ((name = diva_cfg_get_section_element (new, VieImageArchive))) {
				(void)vle_to_dword (name, &used);
				new_archive_name_length = diva_get_bs  (name+used, &new_archive_name);
			}

			while ((section = diva_cfg_get_next_cfg_section (instance, section))) {
				if ((name = diva_cfg_get_section_element (section, VieImageType))) {
					dword org_type;

					(void)vle_to_dword (name, &used);
					org_type = vle_to_dword (name+used, &used);

					if ((org_type == new_type) && (name = diva_cfg_get_section_element (section, VieImageName))) {
						dword org_name_length;
						const byte* org_name;

						(void)vle_to_dword (name, &used);
						org_name_length = diva_get_bs  (name+used, &org_name);

						if ((org_name_length == new_name_length) &&
								(memcmp (new_name, org_name, new_name_length) == 0)) {
							dword org_archive_name_length = 0;
							const byte* org_archive_name = 0;

							if ((name = diva_cfg_get_section_element (section, VieImageArchive))) {
								(void)vle_to_dword (name, &used);
								org_archive_name_length = diva_get_bs  (name+used, &org_archive_name);
							}

							if ((new_archive_name == 0) && (org_archive_name == 0)) {
								return (section);
							} else if ((new_archive_name != 0) && (org_archive_name != 0)) {
								if ((new_archive_name_length == org_archive_name_length) &&
										(memcmp (new_archive_name, org_archive_name, new_archive_name_length) == 0)) {
									return (section);
								}
							}
						}
					}
				}
			}
		}
	}

	return (section);
}

/*
	Compare value of both PCINIT sections. Value is provided as
	binary string
	*/
static int cmp_pcinit_section (const byte* new, const byte* org) {
	const byte* new_value = diva_cfg_get_section_element (new, ViePcInitValue);
	const byte* org_value = diva_cfg_get_section_element (org, ViePcInitValue);
	const byte* new_cfg_line = diva_cfg_get_section_element (new, VieParameterLine);
	const byte* org_cfg_line = diva_cfg_get_section_element (org, VieParameterLine);
	dword new_cfg_line_value, org_cfg_line_value;
	dword used;


	if (new_cfg_line) {
		(void)vle_to_dword (new_cfg_line, &used);
		new_cfg_line_value = vle_to_dword (new_cfg_line+used, &used);
	} else {
		new_cfg_line_value = 0;
	}

	if (org_cfg_line) {
		(void)vle_to_dword (org_cfg_line, &used);
		org_cfg_line_value = vle_to_dword (org_cfg_line+used, &used);
	} else {
		org_cfg_line_value = 0;
	}

	if ((org_cfg_line_value == new_cfg_line_value) && new_value && org_value) {
		dword new_data_length, org_data_length;
		const byte* new_data, *org_data;

		(void)vle_to_dword (new_value, &used);
		new_value += used;
		new_data_length = vle_to_dword (new_value, &used);
		new_data = new_value + used;

		(void)vle_to_dword (org_value, &used);
		org_value += used;
		org_data_length = vle_to_dword (org_value, &used);
		org_data = org_value + used;


		return (!((new_data_length == org_data_length) &&
							(memcmp (new_data, org_data, new_data_length) == 0)));
	}

	return (1);
}

/*
	Compare RAM section:
	- Compare operation
	- Compare values
	*/
static int cmp_ram_section    (const byte* new, const byte* org) {
	const byte* new_value     = diva_cfg_get_section_element (new, VieRamInitValue);
	const byte* org_value     = diva_cfg_get_section_element (org, VieRamInitValue);
	const byte* new_operation = diva_cfg_get_section_element (new, VieRamInitOperation);
	const byte* org_operation = diva_cfg_get_section_element (org, VieRamInitOperation);
	const byte* new_cfg_line =  diva_cfg_get_section_element (new, VieParameterLine);
	const byte* org_cfg_line =  diva_cfg_get_section_element (org, VieParameterLine);
	dword new_cfg_line_value, org_cfg_line_value;
	dword used;

	if (new_cfg_line) {
		(void)vle_to_dword (new_cfg_line, &used);
		new_cfg_line_value = vle_to_dword (new_cfg_line+used, &used);
	} else {
		new_cfg_line_value = 0;
	}

	if (org_cfg_line) {
		(void)vle_to_dword (org_cfg_line, &used);
		org_cfg_line_value = vle_to_dword (org_cfg_line+used, &used);
	} else {
		org_cfg_line_value = 0;
	}

	if ((org_cfg_line == new_cfg_line) && new_value && org_value && new_operation && org_operation) {
		dword new_operation_type, org_operation_type, new_length, org_length;
		const byte* new_data, *org_data;

		(void)vle_to_dword (new_operation, &used);
		new_operation_type = vle_to_dword (new_operation+used, &used);

		(void)vle_to_dword (org_operation, &used);
		org_operation_type = vle_to_dword (org_operation+used, &used);

		(void)vle_to_dword (new_value, &used);
		new_length = diva_get_bs (new_value+used, &new_data);

		(void)vle_to_dword (org_value, &used);
		org_length = diva_get_bs (org_value+used, &org_data);


		return (!((org_operation_type == new_operation_type) && (org_length == new_length) &&
							(memcmp (new_data, org_data, new_length) == 0)));
	}

	return (1);
}

/*
	Compare values of both variables
	*/
static int cmp_var_section    (const byte* new, const byte* org) {
	const byte* new_value = diva_cfg_get_section_element (new, VieNamedVarValue);
	const byte* org_value = diva_cfg_get_section_element (org, VieNamedVarValue);

	if (new_value && org_value) {
		dword used, new_data_length, org_data_length;
		const byte* new_data, *org_data;

		(void)vle_to_dword (new_value, &used);
		new_value += used;
		new_data_length = vle_to_dword (new_value, &used);
		new_data = new_value + used;

		(void)vle_to_dword (org_value, &used);
		org_value += used;
		org_data_length = vle_to_dword (org_value, &used);
		org_data = org_value + used;


		return (!((new_data_length == org_data_length) &&
							(memcmp (new_data, org_data, new_data_length) == 0)));
	}

	return (1);
}

/*
	Compare two image sections

	- check is same type
	- check if same name
	- check if same archive (if any)
	- check if same data (if any)

	*/
static int cmp_image_section  (const byte* new, const byte* org) {
	const byte* name, *new_name, *org_name;
	dword used, new_type, org_type, new_name_length, org_name_length;

	if (!(name = diva_cfg_get_section_element (new, VieImageType))) {
		return (-1);
	}
	(void)vle_to_dword (name, &used);
	new_type = vle_to_dword (name+used, &used);

	if (!(name = diva_cfg_get_section_element (org, VieImageType))) {
		return (-1);
	}
	(void)vle_to_dword (name, &used);
	org_type = vle_to_dword (name+used, &used);

	if (org_type != new_type) {
		return (1);
	}

	if (!(name = diva_cfg_get_section_element (new, VieImageName))) {
		return (-1);
	}
	(void)vle_to_dword (name, &used);
	new_name_length = diva_get_bs  (name+used, &new_name);

	if (!(name = diva_cfg_get_section_element (org, VieImageName))) {
		return (-1);
	}
	(void)vle_to_dword (name, &used);
	org_name_length = diva_get_bs  (name+used, &org_name);

	if ((new_name_length != org_name_length) ||
			(memcmp (new_name, org_name, new_name_length) != 0)) {
		return (1);
	}


	/*
		Compare optional information elements
		*/
	{
		int i = 0;
		diva_vie_id_t id[] = { VieImageArchive, VieImageData };

		for (i = 0; i < sizeof(id)/sizeof(id[0]); i++) {
			new_name = 0;
			org_name = 0;

			if ((name = diva_cfg_get_section_element (new, id[i]))) {
				(void)vle_to_dword (name, &used);
				new_name_length = diva_get_bs  (name+used, &new_name);
			}
			if ((name = diva_cfg_get_section_element (org, id[i]))) {
				(void)vle_to_dword (name, &used);
				org_name_length = diva_get_bs  (name+used, &org_name);
			}

			if (new_name || org_name) {
				if (!(new_name && org_name)) {
					return (1);
				}
				if ((new_name_length != org_name_length) || (memcmp (new_name, org_name, new_name_length) != 0)) {
					return (1);
				}
			}
		}
	}

	return (0);
}

#endif /* } */

/*
	Returns pointer to required configuration element
	inside of configuration function
	*/
const byte* diva_cfg_get_section_element (const byte* src, dword required_id) {
	dword used, max_length, id;
	diva_vie_tags_t* tag;

	if (vle_to_dword (src, &used) != Tlie) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
#endif
		return (0);
	}
	src += used;
	if (!(max_length = vle_to_dword (src, &used))) {
		return (0);
	}
	src += used;

	while (max_length) {
		if ((id = vle_to_dword (src, &used)) == required_id) {
			return (src);
		}
		if (!(tag = find_tag_from_id (id))) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
			DBG_FTL(("%s, %d, id=%lu", __FILE__, __LINE__, id))
#endif
			return (0);
		}
		if (!(used = (*(tag->skip))(src, max_length))) {
			return (0);
		}
		src        += used;
		max_length -= used;
	}

	return (0);
}

/*
	Find description of element using
	element id
	*/
diva_vie_tags_t* find_tag_from_id (dword id) {
	dword i;

	for (i = 0; i < sizeof(vie_attributes)/sizeof(vie_attributes[0]); i++) {
		if (id == (dword)vie_attributes[i].id) {
			return (&vie_attributes[i]);
		}
	}

	return (0);
}

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__ /* { */
static dword dump_tlie    (struct _diva_vie_tags* tag, byte* src) {
	dword used;
	dword length = vle_to_dword (src, &used);

	DBG_LOG(("TLIE, length=%lu", length))

	return (used);
}

static dword dump_vie_vle (struct _diva_vie_tags* tag, byte* src) {
	dword used;
	dword value = vle_to_dword (src, &used);

	DBG_LOG((" VIE(%s)=%lu", tag->attribute, value))

	return (used);
}

static dword dump_vie_bs  (struct _diva_vie_tags* tag, byte* src) {
	const byte* data = 0;
	dword used, length = diva_get_bs  (src, &data);

	DBG_LOG((" VIE(%s), length=%lu", tag->attribute, length))
	DBG_BLK(((byte*)data, length))

	used = (dword)(data - src) + length;


	return (used);
}

/*
	Read VIE section with type:
	<vie id='...'>
   <bs>1,2,3,4,5
   </bs>
	</vie>
	*/
static dword read_vie_bs (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag) {
	byte* data = pLib->data ? &pLib->data[pLib->data_length] : 0;
	dword pos, data_length = data ? (pLib->max_data_length - pLib->data_length) : 0;

	/*
		Store elenemt ID
		*/
	/*
		Write element ident
		*/
	if (!(pos = dword_to_vle ((dword)tag->id, data, pLib->max_data_length - pLib->data_length))) {
		DBG_ERR(("Failed to write Tlie Id"))
		return (0);
	}
	pLib->data_length += pos;
	data        = pLib->data ? &pLib->data[pLib->data_length] : 0;
	data_length = data       ? (pLib->max_data_length - pLib->data_length) : 0;

	if (!(pos = diva_read_bs (&pLib->src[pLib->position], data, &data_length))) {
		DBG_ERR(("Failed to write BS"))
		return (0);
	}

	pLib->position    += pos;
	pLib->data_length += data_length;

	if (close_tag (pLib, tag)) {
		return (0);
	}

	return (pos);
}

/*
	Read VIE section with type:
	<vie id='...'>
   Number
	</vie>

	Returns amount of bytes read from input stream or zero
	on error
	*/
static dword read_vie_vle (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag) {
	dword pos, data_length;
	byte* data = pLib->data ? &pLib->data[pLib->data_length] : 0;

	/*
		Write element ident
		*/
	if (!(pos = dword_to_vle ((dword)tag->id, data, pLib->max_data_length - pLib->data_length))) {
		DBG_ERR(("Failed to write Tlie Id"))
		return (0);
	}
	pLib->data_length += pos;
	data = pLib->data ? &pLib->data[pLib->data_length] : 0;

	data_length = pLib->max_data_length - pLib->data_length;
	if (!(pos = str_to_vle (&pLib->src[pLib->position], data, &data_length))) {
		return (0);
	}
	pLib->position    += pos;
	pLib->data_length += data_length;

	if (close_tag (pLib, tag)) {
		return (0);
	}

	return (pos);
}


/*
	Read TLIE
	TLIE always has following format:
	<tlie>
	</tlie>

	RETURN:
		Amount of bytes written to output stream or zero on error

	*/
static dword read_tlie (diva_cfg_lib_instance_t* pLib, struct _diva_vie_tags* tag) {
	byte* data = pLib->data ? &pLib->data[pLib->data_length] : 0;
	byte* tmp_data;
	diva_vie_tags_t* next_tag;
	dword length;
	dword tlie_position;
	dword tlie_length;

	/*
		Write element type
		*/
	if (!(length = dword_to_vle (Tlie, data, pLib->max_data_length - pLib->data_length))) {
		DBG_ERR(("Failed to write Tlie Id"))
		return (0);
	}
	pLib->data_length += length;
	data = pLib->data ? &pLib->data[pLib->data_length] : 0;
	pLib->position    += to_next_tag (&pLib->src[pLib->position]);

	tmp_data      = pLib->data;
	tlie_length   = pLib->data_length;
	tlie_position = pLib->position;
	pLib->data    = 0;

	do {
		if (!(next_tag = diva_cfg_find_tag_by_name (pLib))) {
			DBG_ERR(("Failed to find next tag at offset %lu (%d)", pLib->position, __LINE__))
			return (0);
		}
		if (next_tag->id == Tlie) {
			pLib->level++;
			if (!read_tlie (pLib, next_tag)) {
				DBG_ERR(("Failed to read nested tlie at offset %lu", pLib->position))
				return (-1);
			}
			pLib->level--;
		} else if (!(length = (*(next_tag->read_proc))(pLib, next_tag))) {
			DBG_ERR(("Failed to read '%s' tag at %lu", next_tag->tag, pLib->position))
			return (0);
		}
		pLib->position += to_next_tag (&pLib->src[pLib->position]);
	} while (!((pLib->src[pLib->position] == '<') && (pLib->src[pLib->position + 1] == '/')));


	{
		dword nested_length = pLib->data_length - tlie_length;

		pLib->data          = tmp_data;
		pLib->data_length   = tlie_length;
		data = pLib->data ? &pLib->data[pLib->data_length] : 0;

		/*
			Write stream length
			*/
		if (!(length = dword_to_vle (nested_length, data, pLib->max_data_length - pLib->data_length))) {
			DBG_ERR(("Failed to write Tlie Id"))
			return (0);
		}
		pLib->data_length += length;
		data = pLib->data ? &pLib->data[pLib->data_length] : 0;

		if (pLib->data) {
			/*
				rewind input stream
				*/
			pLib->position = tlie_position;
			do {
				if (!(next_tag = diva_cfg_find_tag_by_name (pLib))) {
					DBG_ERR(("Failed to find next tag at offset %lu (%d)", pLib->position, __LINE__))
					return (0);
				}
				if (next_tag->id == Tlie) {
					pLib->level++;
					if (!read_tlie (pLib, next_tag)) {
						DBG_ERR(("Failed to read nested tlie at offset %lu", pLib->position))
						return (-1);
					}
					pLib->level--;
				} else if (!(length = (*(next_tag->read_proc))(pLib, next_tag))) {
					DBG_ERR(("Failed to read '%s' tag at %lu", next_tag->tag, pLib->position))
					return (0);
				}
				pLib->position += to_next_tag (&pLib->src[pLib->position]);
			} while (!((pLib->src[pLib->position] == '<') && (pLib->src[pLib->position + 1] == '/')));
		} else {
			/*
				Easy update amount of bytes wanted to store this element
				*/
			pLib->data_length += nested_length;
		}
	}


#if 0
	if (0) {
		dword nested_length = pLib->data_length - data_length_tmp;

		pLib->data          = tmp_data;
		pLib->data_length   = data_length_tmp;

		if (pLib->data) {
			pLib->position = position_tmp;
		}

		/*
			Write stream length
			*/
		if (!(length = dword_to_vle (nested_length, data, pLib->max_data_length - pLib->data_length))) {
			DBG_ERR(("Failed to write Tlie Id"))
			return (0);
		}
		pLib->data_length += length;
		data = pLib->data ? &pLib->data[pLib->data_length] : 0;

		if (data) {
			data_length_tmp = pLib->data_length ;
			if (!(length = (*(next_tag->read_proc))(pLib, next_tag))) {
				DBG_ERR(("Failed to read '%s' tag at %lu", next_tag->tag, pLib->position))
				return (0);
			}
		} else {
			pLib->data_length += nested_length;
		}
	}
#endif

	pLib->position += to_next_tag (&pLib->src[pLib->position]);

	if (close_tag (pLib, tag)) {
		return (0);
	}

	return (pLib->data_length);
}

static int close_tag (diva_cfg_lib_instance_t* pLib, diva_vie_tags_t* tag) {
	pLib->position += to_next_tag (&pLib->src[pLib->position]);

	if (pLib->src[pLib->position] != '<') {
		DBG_ERR(("close_tag failed at offset %lu, missing '<'", pLib->position))
		return (-1);
	}
	pLib->position++;
	if (pLib->src[pLib->position] != '/') {
		DBG_ERR(("close_tag failed at offset %lu, missing '/'", pLib->position))
		return (-1);
	}
	pLib->position++;

	if (strncmp(&pLib->src[pLib->position], tag->tag, strlen(tag->tag))) {
		DBG_ERR(("close_tag failed at offset %lu for '%s'", pLib->position, tag->tag))
		return (-1);
	}
	pLib->position += strlen(tag->tag);
	if (pLib->src[pLib->position] != '>') {
		DBG_ERR(("close_tag failed at offset %lu, missing '>'", pLib->position))
		return (-1);
	}
	pLib->position++;

	return (0);
}

diva_cfg_section_t* identify_section (diva_vie_id_t id) {
	int i;

	for (i = 0; i < sizeof(sections)/sizeof(sections[0]); i++) {
		if (sections[i].type == id) {
			return (&sections[i]);
		}
	}

	return (0);
}

/*
	Read boinary string from configuration file to storage
	INPUT:
		<bs>
			0x01,0x02,0x03,0x04,...0x01
		</bs>
		length - maximal amount of bytes that can be written to 'dst'

	OUTPUT:
		dst - used to store binary value in formart:
			VLE - length
			raw binary data
			In case 'dst' is set to zero then no data is stored to
      output buffer and call is used to calculate amount of
      mamory necessary to save tha value to to verify the syntax
      of the configuration file
		length - amount of bytes written to output buffer or zero on error

	RETURN:
		amount of bytes read from configuration file or zero on error


	NOTE:
		Alternative coding can gave format:
		<as>ascii character string</as>

	*/
static dword diva_read_bs (const char* src, byte* dst, dword* length) {
	dword max_length = *length, pos = 0, data_length, written, len;
	int zero_read;
	const char* end_tag = "</bs>";
	int binary_string   = 1;

	*length = 0;

	pos = to_next_tag (src);
	if (strncmp (&src[pos], "<bs>", 4)) {
		if (!strncmp (&src[pos], "<as>", 4)) {
			binary_string = 0;
			end_tag = "</as>";
		} else {
			return (0);
		}
	}
	pos += 4; /* <bs> */


	if (binary_string) {
		/*
			Calculate length of binary string
			*/
		for (data_length = 0;;) {
			dword val;
			char* end = 0;
			pos += diva_cfg_ignore_unused (&src[pos]);
			if ((val = strtol (&src[pos], &end, 16)) > 0xff) {
				return (0);
			}
			if (&src[pos] == end) {
				zero_read = 1;
			} else {
				zero_read = 0;
			}
			pos += (dword)(end - &src[pos]);
			pos += diva_cfg_ignore_unused(&src[pos]);
			if (src[pos] == ',') {
				data_length++;
				pos++;
			} else if (!strncmp(&src[pos], "</bs>", 5)) {
				if (!zero_read) {
					data_length++;
				}
				pos += 5;
				break;
			} else {
				return (0);
			}
		}
	} else {
		/*
			Calculate length of ASCII string
			*/
		for (data_length = 0; src[pos] != 0; pos++) {
			if ((src[pos] == '<')   && (src[pos+1] == '/') &&
					(src[pos+2] == 'a') && (src[pos+3] == 's') && (src[pos+4] == '>')) {
				pos += 5;
				break;
			}
			data_length++;
		}
		if (src[pos] == 0) {
			return (0);
		}
	}

	written = dword_to_vle (data_length, 0, 24) + data_length;

	if (binary_string && dst) {
		written = 0;
		pos = to_next_tag (src) + 4;
		pos += diva_cfg_ignore_unused (&src[pos]);

		if ((len = dword_to_vle (data_length, dst, max_length)) == 0) {
			return (0);
		}
		written    += len;
		for (;;) {
			dword val;
			char* end = 0;

			if (written >= max_length) {
				return (0);
			}
			pos += diva_cfg_ignore_unused (&src[pos]);
			if ((val = strtol (&src[pos], &end, 16)) > 0xff) {
				return (0);
			}
			if (&src[pos] == end) {
				zero_read = 1;
			} else {
				zero_read = 0;
			}
			pos += (dword)(end - &src[pos]);
			pos += diva_cfg_ignore_unused(&src[pos]);
			if (src[pos] == ',') {
				dst[written++] = (byte)val;
				pos++;
			} else if (!strncmp(&src[pos], "</bs>", 5)) {
				if (!zero_read) {
					dst[written++] = (byte)val;
				}
				pos += 5;
				break;
			} else {
				return (0);
			}
		}
	}

	if (!binary_string && dst) {
		written = 0;
		pos = to_next_tag (src) + 4;

		if ((len = dword_to_vle (data_length, dst, max_length)) == 0) {
			return (0);
		}
		written    += len;
		for (;;) {
			if (!src[pos]) {
				return (0);
			}
			if ((src[pos] == '<')   && (src[pos+1] == '/') &&
					(src[pos+2] == 'a') && (src[pos+3] == 's') && (src[pos+4] == '>')) {
				pos += 5;
				break;
			}
			if (written >= max_length) {
				return (0);
			}
			dst[written++] = src[pos++];
		}
	}

	*length = written;

	return (pos);
}

/*
	Convert string variable to VLE information element
		INPUT:
			input stream
			max amount of bytes that can be written via length
		OUTPUT:
			value stored to vle, number of bytes written to length
		RETURN:
			number of bytes read from input stream
	*/
static dword str_to_vle (const char* src, byte* vle, dword* length) {
	dword pos = diva_cfg_ignore_unused (src);
	char* end;
	dword v = (dword)strtol(&src[pos], &end, 16);
	dword max_length = *length;

	pos += (dword)(end - &src[pos]);
	
	if (!(*length = dword_to_vle (v, vle, max_length))) {
		return (0);
	}

	return (pos);
}

/*
	Converts integer variable to VLE
	Return value length (e.g. number of bytes used)
	or zero on error.
	In case 'vle' is set to zero then only amount of bytes
	necessary to encode the values is calculated
	*/
static dword dword_to_vle (dword value, byte* vle, dword max_length) {
	dword pos = 0;

	do {
		if (vle) {
			if (pos >= max_length) {
				return (0);
			}
			vle[pos] = ((byte)value) & 0x7f;
		}
		value = (value >> 7);
		if (value != 0) {
			if (vle) {
				if (pos >= max_length) {
					return (0);
				}
				vle[pos] |= 0x80;
			}
		}
		pos++;
	} while (value);

	return (pos);
}

#endif /* } */

static dword skip_tlie (const byte* src, dword max_length) {
	dword total_used = 0, used;

	(void)vle_to_dword (src, &used);
	src        += used;
	total_used += used;
	if (!used || (total_used >= max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_tlie: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	total_used += vle_to_dword (src, &used);
	total_used += used;
	if (!used || (total_used > max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_tlie: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	return (total_used);
}

static dword skip_vie_vle (const byte* src, dword max_length) {
	dword used, total_used = 0;

	(void)vle_to_dword (src, &used);
	total_used += used;
	src        += used;
	if (!used || (total_used >= max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_vie_vle: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	(void)vle_to_dword (src, &used);
	total_used += used;
	if (!used || (total_used > max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_vie_vle: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	return (total_used);
}

static dword skip_vie_bs  (const byte* src, dword max_length) {
	dword total_used = 0, used;

	(void)vle_to_dword (src, &used);
	src        += used;
	total_used += used;
	if (!used || (total_used >= max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_vie_bs: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	total_used += vle_to_dword (src, &used);
	total_used += used;
	if (!used || (total_used > max_length)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("skip_vie_bs: %lu:%lu", used, max_length))
#endif
		return (0);
	}

	return (total_used);
}

/*
	INPUT:
					Pointer to configuration section

	OUTPUT:
					Type of current configuration section
	
	*/
diva_vie_id_t diva_cfg_get_section_type (const byte* src) {
	int i;

	for (i = 0; i < sizeof(sections)/sizeof(sections[0]); i++) {
		if (diva_cfg_get_section_element (src, sections[i].type)) {
			return (sections[i].type);
		}
	}

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
	DBG_FTL(("Unknown section type"))
#endif

	return (IeNotDefined);
}

/*
	Retrieve contained in BS data:
	INPUT:
		pointer to start of BS element
	OUTPUT:
		pointer to start of BS data field
	RETURN:
		amount of bytes in the BS data field
	*/
dword diva_get_bs  (const byte* src, pcbyte* dst) {
	dword data_length, offset;

	data_length = vle_to_dword (src, &offset);

	*dst = (src + offset);


	return (data_length);
}


/*
	Provide pointer to configuration function
	INPUT:
		src             - pointer to data storage
		current_section - pointer to actual section or zero in case pointer
                      to first section is to be returned
	RETURN:
		pointer to start of section or zero on error
	*/
const byte* diva_cfg_get_next_cfg_section (const byte* src, const byte* current_section) {
	dword id, used, max_length;
	diva_vie_tags_t* tag;
	int skip_current = 0;

	if (vle_to_dword (src, &used) != Tlie) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
#endif
		return (0);
	}
	src += used;
	if (!(max_length = vle_to_dword (src, &used))) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("%s, %d", __FILE__, __LINE__))
#endif
		return (0);
	}
	src += used;

	if ((current_section >= (src + max_length)) ||
			(current_section && (current_section < src))) {
		return (0);
	}

	if (!current_section) {
		current_section = src;
	} else {
		max_length -= (dword)(current_section - src);
		skip_current = 1;
	}

	while (max_length) {
		if ((id = vle_to_dword (current_section, &used)) == Tlie) {
			if (!skip_current) {
				return (current_section);
			}
		}
		skip_current = 0;
		if (!(tag = find_tag_from_id (id))) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
			DBG_FTL(("%s, %d, id=%lu", __FILE__, __LINE__, id))
#endif
			return (0);
		}
		if (!(used = (*(tag->skip))(current_section, max_length))) {
			return (0);
		}
		current_section += used;
		max_length      -= used;
	}

	return (0);
}

/*
	Return next configuration item that belongs to channel.
	Negative channel values advises to provide configuration
	information that not belongs to any channel
	*/
const byte* diva_cfg_get_next_channel_configuration_item (const byte* src,
																													const byte* current_item,
																													int channel) {
	dword id = 0;

	while ((id == 0) && (current_item = diva_cfg_get_next_cfg_section (src, current_item))) {
		switch ((id = diva_cfg_get_section_type (current_item))) {
			case VieBindingDirection:
			case IeNotDefined:
				id = 0;
				break;
		}
	}

	return (current_item);
}

diva_ram_init_operation_t diva_cfg_get_ram_init_operation (const byte* src) {
	const byte* data = diva_cfg_get_section_element (src, VieRamInitOperation);
	diva_ram_init_operation_t ret;
	dword used;

	(void)vle_to_dword (data, &used);
	ret = (diva_ram_init_operation_t)vle_to_dword(data+used, &used);

	if ((ret < RamInitOperationWrite) || (ret >= RamInitOperationLast)) {
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
		DBG_FTL(("Invalid RAM access type"))
#endif
		ret = RamInitOperationNotDefined;
	}

	return (ret);
}

dword diva_cfg_get_ram_init_offset (const byte* src) {
	const byte* data = diva_cfg_get_section_element (src, VieRamInitOffset);
	dword used;

	(void)vle_to_dword (data, &used);
	return (vle_to_dword(data+used, &used));
}

dword diva_cfg_get_ram_init_value (const byte* src, pcbyte* value) {
	const byte* data = diva_cfg_get_section_element (src, VieRamInitValue);
	dword used;

	(void)vle_to_dword (data, &used);
	return (diva_get_bs  (data+used, value));
}

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__ /* { */
/*
	Write RAM init values to memory

	Returns length of used memory area on success
	Returns 0xffffffff on error
	*/
static dword diva_cfg_write_ram_init (byte* dst, const byte* src, dword max_length) {
	const byte* item = 0;
	dword length = 0;

	while ((item = diva_cfg_get_next_channel_configuration_item (src, item, -1))) {
		if (diva_cfg_get_section_type (item) == VieRamInitOffset) {
			const byte* ram_init_data;
			dword ram_init_data_length                = diva_cfg_get_ram_init_value (item, &ram_init_data);
			dword ram_data_offset                     = diva_cfg_get_ram_init_offset    (item);
			diva_ram_init_operation_t ram_access_type = diva_cfg_get_ram_init_operation (item);
			const char* visual_name = 0;

			if (dst) {
				if ((ram_data_offset+ram_init_data_length) > max_length) {
					DBG_ERR(("XDI notify at %d (%ld > %ld)", __LINE__,
									(ram_data_offset+ram_init_data_length), max_length))
					return (0xffffffff);
				} else {
					switch (ram_access_type) {
						case RamInitOperationWrite:
							memcpy (dst+ram_data_offset, ram_init_data, ram_init_data_length);
							visual_name="Write";
							break;

						case RamInitOperationSetBits: {
							dword i, j;

							for (i = ram_data_offset, j = 0; i < (ram_data_offset+ram_init_data_length); i++, j++) {
								dst[i] |= ram_init_data[j];
							}
							visual_name="SetBits";
						} break;

						case RamInitOperationClearBits: {
							dword i, j;

							for (i = ram_data_offset, j = 0; i < (ram_data_offset+ram_init_data_length); i++, j++) {
								dst[i] &= ~(ram_init_data[j]);
							}
							visual_name="ClearBits";
						} break;

						default:
							DBG_ERR(("XDI notify: unknown ram access type"))
							return (0xffffffff);
					}
					DBG_TRC(("Ram%s[%08x][%08x]", visual_name, ram_data_offset, ram_init_data_length))
					DBG_BLK(((byte*)ram_init_data, ram_init_data_length))
				}
			}
			length = MAX(length, (ram_data_offset+ram_init_data_length));
		}
	}

	return (length);
}

static byte diva_cfg_get_pc_init_type (const byte* src) {
	const byte* data = diva_cfg_get_section_element (src, ViePcInitname);
	dword used;

	(void)vle_to_dword (data, &used);
	return ((byte)vle_to_dword(data+used, &used));
}

static byte diva_cfg_get_pc_init_data (const byte* src, pcbyte* value) {
	const byte* data = diva_cfg_get_section_element (src, ViePcInitValue);
	dword used;

	if (data) {
		(void)vle_to_dword (data, &used);
		return ((byte)diva_get_bs  (data+used, value));
	} else {
		return (0);
	}
}

static byte diva_cfg_get_cfg_line_number (const byte* src) {
	const byte* data = diva_cfg_get_section_element (src, VieParameterLine);
	dword used;
	byte ret;

	if (data) {
		(void)vle_to_dword (data, &used);
		ret = (byte)vle_to_dword (data+used, &used);
	} else {
		ret = 0;
	}

	return (ret);
}

/*
	Write RAM init values to memory

	Returns length of used memory area on success
	Returns 0xffffffff on error
	*/
#define PCINIT_OFFSET 224
static dword diva_cfg_write_pcinit (byte* dst, const byte* src, dword max_length) {
	dword offset = PCINIT_OFFSET ;
	const byte* item = 0;
	byte current_cfg_line = 0;
	int write_cfg_line    = 0;

	while ((item = diva_cfg_get_next_channel_configuration_item (src, item, -1))) {
		if (diva_cfg_get_section_type (item) == ViePcInitname) {
			const byte* pc_init_data;
			byte pc_init_data_length = diva_cfg_get_pc_init_data (item, &pc_init_data);
			byte pc_init_type        = diva_cfg_get_pc_init_type (item);
			byte cfg_line            = diva_cfg_get_cfg_line_number (item);

			if (cfg_line != current_cfg_line) {
				write_cfg_line = 1;
				current_cfg_line = cfg_line;
			}
			if (write_cfg_line) {
				if (dst) {
					DBG_TRC(("Insert PCINIT_CFG_LINE_NUMBER=%02x", current_cfg_line))
					if ((offset+2) < max_length) {
						dst[offset++] = PCINIT_CFG_LINE_NUMBER;
						dst[offset++] = 1;
						dst[offset++] = current_cfg_line;
					} else {
						DBG_ERR(("XDI notify at %d", __LINE__))
						return (0xffffffff);
					}
				} else {
					offset++; /* PC INIT TYPE, one BYTE */
					offset++; /* PC INIT DATA LENGTH */
					offset++; /* PC INIT DATA */

				}
			}

			if (dst) {
				if (offset < max_length) {
					dst[offset++] = pc_init_type;
					if ((pc_init_type & 0x80) == 0) {
						DBG_TRC(("PcInit[%02x][%02x]", pc_init_type, pc_init_data_length))
						if (pc_init_data_length) {
							DBG_BLK(((byte*)pc_init_data, pc_init_data_length))
						}
					} else {
						DBG_TRC(("PcInit[%02x]", pc_init_type))
					}

				} else {
					DBG_ERR(("XDI notify at %d", __LINE__))
					return (0xffffffff);
				}
				if ((pc_init_type & 0x80) == 0) {
					if (offset < max_length) {
						dst[offset++] = pc_init_data_length;
					} else {
						DBG_ERR(("XDI notify at %d", __LINE__))
						return (0xffffffff);
					}
					if (pc_init_data_length != 0) {
						if ((offset+pc_init_data_length) < max_length) {
							memcpy (dst+offset, pc_init_data, pc_init_data_length);
							offset += pc_init_data_length;
						} else {
							DBG_ERR(("XDI notify at %d (%d >= %d)", __LINE__, offset+pc_init_data_length, max_length))
							return (0xffffffff);
						}
					}
				}
			} else {
				offset++; /* PC INIT TYPE, one BYTE */
				if ((pc_init_type & 0x80) == 0) {
					offset++; /* PC INIT DATA LENGTH */
					offset += (byte)pc_init_data_length; /* PC INIT DATA */
				}
			}
		}
	}

	if (dst) {
		if (offset < max_length) {
			dst[offset++] = 0;
		} else {
			DBG_ERR(("XDI notify at %d", __LINE__))
			return (0xffffffff);
		}
	} else {
		offset++;
	}

	return (offset);
}

/*
	Return total length of instance data and set pointer to the start of the
	instance data
	*/
static dword diva_cfg_get_instance_data (const byte* src, pcbyte* instance_name_start) {
	dword total_length = 0;
	dword used, instance_type;

	(void)vle_to_dword (src, &used); /* Tlie */
	src += used;
	(void)vle_to_dword (src, &used); /* Total length */
	src += used;

	*instance_name_start = src;

	instance_type = vle_to_dword (src, &used); /* Type of instance */
	total_length += used;
	src          += used;

	if (instance_type == (dword)VieInstance2) {
		total_length += vle_to_dword (src, &used); /* instance name length */
		total_length += used;
	} else {
		(void)vle_to_dword (src, &used); /* instance number */
		total_length += used;
	}

	return (total_length);
}

/*
	Translate configuration change notification to
	xdi adapter configuration memory update
	*/
byte* diva_cfg_translate_xdi_data (dword header_length, const byte* src, dword* data_length) {
	dword max_length = 0, position;
	dword total_ram_section_length = 0, shared_ram_length = 0, instance_ident_length = 0;
	dword total_section_length = 0;
	byte* mem = 0, *hdr = 0;
	const byte* item, *instance_ident_start;

	for (;;) {
		position = dword_to_vle (Tlie, mem, max_length);
		if (mem) { /* total instance length */
			position += dword_to_vle (total_section_length, mem+position, max_length - position);
		} else {
			total_section_length = position;
		}

		/*
			Instance data
			*/
		instance_ident_length = diva_cfg_get_instance_data (src, &instance_ident_start);
		if (mem) {
			if ((position+instance_ident_length) < (max_length-position)) {
				memcpy (mem+position, instance_ident_start, instance_ident_length);
			}
		}
		position += instance_ident_length;

		/*
			Copy sections that can not be written to adapter memory
			*/
		item = 0;
		while ((item = diva_cfg_get_next_channel_configuration_item (src, item, -1))) {
			dword id;
			switch ((id = diva_cfg_get_section_type (item))) {
				case ViePcInitname:
				case VieRamInitOffset:
					break;

				default:
					id = get_max_length (item);
					if (mem) {
						if (id <= (max_length - position)) {
							memcpy (mem+position, item, id);
						} else {
							diva_os_free (0, hdr);
							DBG_ERR(("XDI notify: overflow at %d (%d > %d)", __LINE__, id+position, max_length-position))
							return (0);
						}
					}
					position += id;
					break;
			}
		}
		/*
			Create shared memory image, packaged as adapter memory write
			section at offset zero
			*/
		position += dword_to_vle (Tlie, mem ? mem+position : 0, max_length - position);
		if (mem) {
			position += dword_to_vle (total_ram_section_length, mem ? mem+position : 0, max_length - position);
		} else {
			total_ram_section_length = position;
		}
		position += dword_to_vle (VieRamInitOffset,       mem ? mem+position : 0, max_length - position);
		position += dword_to_vle (0,                      mem ? mem+position : 0, max_length - position);
		position += dword_to_vle (VieRamInitOperation,    mem ? mem+position : 0, max_length - position);
		position += dword_to_vle (RamInitOperationWrite,  mem ? mem+position : 0, max_length - position);
		position += dword_to_vle (VieRamInitValue,        mem ? mem+position : 0, max_length - position);
		if (mem) {
			position += dword_to_vle (shared_ram_length, mem ? mem+position : 0, max_length - position);
		}

		{
			dword ram_length    = diva_cfg_write_ram_init (mem ? mem+position : 0, src, max_length-position);
			dword pcinit_length = diva_cfg_write_pcinit   (mem ? mem+position : 0, src, max_length-position);

			if ((ram_length == 0xffffffff) || (pcinit_length == 0xffffffff)) {
				if (hdr) {
					diva_os_free (0, hdr);
				}
				DBG_ERR(("XDI notify at %d", __LINE__))
				return (0);
			}
			if (mem == 0) {
				shared_ram_length = MAX(ram_length, pcinit_length);
				DBG_TRC(("used ram: %ld, used pcinit: %ld, final length: %ld",
								ram_length, pcinit_length, shared_ram_length))
			} else {
				DBG_TRC(("used ram: %ld, used pcinit: %ld, final length: %ld",
									ram_length, pcinit_length, shared_ram_length))
			}
			position += shared_ram_length;
		}

		if (!mem) {
			position += dword_to_vle (shared_ram_length, 0, max_length);
			total_ram_section_length = position - total_ram_section_length;
			position += dword_to_vle (total_ram_section_length, 0, max_length);
			total_section_length = position - total_section_length;
			DBG_LOG(("TOTAL SECTION LENGTH: %ld\n", total_section_length))
			position += dword_to_vle (total_section_length, 0, max_length);
			max_length = position;

			if (!(hdr = diva_os_malloc (0, max_length+header_length))) {
				DBG_ERR(("XDI notify: failed to alloc %lu bytes", max_length+header_length))
				return (0);
			}
			DBG_TRC(("XDI notify length: %ld", max_length))
			memset (hdr, 0x00, max_length+header_length);
			mem = hdr+header_length;
		} else {
			DBG_TRC(("POSITION: %ld", position))
			break;
		}
	}

	*data_length = (max_length+header_length);

	return (hdr);
}
#endif /* } */

/*
	Provide name of the protocol image
	*/
int diva_cfg_get_image_info (const byte* src,
														 diva_cfg_image_type_t image_type,
														 pcbyte* image_name,
														 dword*  image_name_length,
														 pcbyte* archive_name,
														 dword*  archive_name_length) {
	const byte* item = 0, *element;
	*image_name = *archive_name = 0;
	*image_name_length = *archive_name_length = 0;

	while ((item = diva_cfg_get_next_channel_configuration_item (src, item, -1))) {
		if (diva_cfg_get_section_type (item) == VieImageType) {
			if ((element = diva_cfg_get_section_element (item, VieImageType))) {
				dword used;

				(void)vle_to_dword (element, &used);
				if ((vle_to_dword (element+used, &used) == (dword)image_type)) {
					if ((element = diva_cfg_get_section_element (item, VieImageName))) {
						(void)vle_to_dword (element, &used);
						element += used;
						*image_name_length = vle_to_dword (element, &used);
						*image_name = (element+used);
					} else {
						return (-1);
					}
					if ((element = diva_cfg_get_section_element (item, VieImageArchive))) {
						(void)vle_to_dword (element, &used);
						element += used;
						*archive_name_length = vle_to_dword (element, &used);
						*archive_name = (element+used);
					}

					return (0);
				}
			}
		}
	}

	return (-1);
}

const byte* diva_cfg_get_next_ram_init_section (const byte* src, const byte* section) {
	while ((section = diva_cfg_get_next_channel_configuration_item (src, section, -1))) {
		if (diva_cfg_get_section_type (section) == VieRamInitOffset) {
			return (section);
		}
	}

	return (0);
}

/*
	Find variable with specified name, return pointer to
  data section of the variable and provide the data length
	*/
const byte* diva_cfg_find_named_variable (const byte* src,
																					const byte* variable_name,
																					dword variable_name_length,
																					dword* data_length) {
	const byte* section = 0;

	while ((section = diva_cfg_get_next_channel_configuration_item (src, section, -1))) {
		if (diva_cfg_get_section_type (section) == VieNamedVarName) {
			const byte* name = diva_cfg_get_section_element (section, VieNamedVarName);
			dword used, name_length;

			(void)vle_to_dword (name, &used);
			name_length = diva_get_bs (name+used, &name);

			if ((name_length == variable_name_length) && (!memcmp(name, variable_name, variable_name_length))) {
				if ((name = diva_cfg_get_section_element (section, VieNamedVarValue))) {
					(void)vle_to_dword (name, &used);
					*data_length = diva_get_bs (name+used, &name);
					return (name);
				}
			}
		}
	}


	return (0);
}

/*
	Retrieve named value

	Return zero on success
	Return -1 on error
	*/
int diva_cfg_find_named_value (const byte* src,
															 const byte* variable_name,
															 dword variable_name_length,
															 dword* value) {
	dword data_length;
	const byte* data  = diva_cfg_find_named_variable (src, variable_name, variable_name_length, &data_length);

	if (data) {
		switch (data_length) {
			case 1:
				*value = (dword)*data;
				return (0);
			case 2:
				*value = (dword)READ_WORD(data);
				return (0);
			case 4:
				*value = READ_DWORD(data);
				return (0);
		}
	}

	return (-1);
}


