
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

#ifndef __DIVA_CFG_LIB_INSTANCE_H__
#define __DIVA_CFG_LIB_INSTANCE_H__

struct _diva_cfg_lib_access_ifc;

typedef struct _diva_cfg_lib_instance {
	const byte* storage;


	/*
		Parser state
		*/
	const char* src; /* Pointer to zero terminated input stream */
	dword level;
	dword position;

	byte* data;
	dword data_length;
	dword max_data_length;

	dword parse_error;

	diva_os_spin_lock_t notify_lock;

	diva_entity_queue_t notify_q;

	char* return_data;
  dword return_data_length;
  dword max_return_data_length;

} diva_cfg_lib_instance_t;

diva_hot_update_operation_result_t diva_cfg_lib_call_notify_callback (dword owner,
																																			const byte* instance,
																																			dword instance_data_length,
																																			dword response_position);
diva_hot_update_operation_result_t diva_cfg_lib_call_variable_notify_callback (dword owner,
																																							 int instance_by_name,
																																							 const byte* instance_ident,
																																							 dword instance_ident_length,
																																							 dword instance_nr,
																																							 const byte* variable,
																																							 int changed);
/*
	Functions used to write configuration change response in the response buffer
	*/
int diva_cfg_lib_response_open_owner  (dword owner);
int diva_cfg_lib_response_close_owner (void);
int diva_cfg_lib_response_open_instance (dword instance);
int diva_cfg_lib_response_close_instance (void);
int diva_cfg_lib_response_open_instance_by_name (const byte* name, dword name_length);
int diva_cfg_lib_open_response (void);
int diva_cfg_lib_close_response (void);
int add_string_to_response_buffer (const char* p);
dword get_response_buffer_position (void);
dword to_next_tag (const char* src);
const byte* diva_cfg_storage_enum_variable (struct _diva_cfg_lib_access_ifc* ifc,
																						const   byte* instance_data,
																						const   byte* current_var,
																						const   byte* template,
																						dword   template_length,
																						pcbyte* key,
																						dword*  key_length,
																						pcbyte* name,
																						dword*  name_length);
diva_hot_update_operation_result_t diva_update_hotplug_variable (const byte* owner,
																																 const byte* instance,
																																 const byte* variable);
int diva_cfg_lib_update_named_xdi_variables (const byte* instance);

#endif

