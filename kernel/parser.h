/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic Networks range of DIVA Server Adapters.
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
#ifndef __DIVA_CFG_LIB_PARSER_IFC_H__
#define __DIVA_CFG_LIB_PARSER_IFC_H__

typedef struct _diva_man_var_header {
	byte   escape;
	byte   length;
	byte   management_id;
	byte   type;
	byte   attribute;
	byte   status;
	byte   value_length;
	byte	 path_length;
} diva_man_var_header_t;


int diva_cfg_lib_read_input (diva_cfg_lib_instance_t* pLib);
void diva_cfg_lib_dump_storage (const byte* data, dword data_length);
const byte* diva_cfg_get_next_instance (const byte* src, const byte* current_section);
int diva_cfg_compare (diva_cfg_lib_instance_t* pLib, const byte* org, const byte* new);
const byte* diva_cfg_parser_get_owner_configuretion (diva_cfg_lib_instance_t* pLib, dword owner);
dword diva_cfg_ignore_unused (const char* src);
diva_vie_tags_t* diva_cfg_find_tag_by_name (diva_cfg_lib_instance_t* pLib);
const byte* diva_cfg_lib_find_instance (dword nr,
																				const byte* name,
																				dword name_length,
																				const byte* src,
																				dword max_length);
int diva_cfg_check_management_variable_value_type (byte var_type);

#endif

