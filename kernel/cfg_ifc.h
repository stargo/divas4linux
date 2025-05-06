
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

#ifndef __DIVA_CFG_LIB_INTERFACE_H__
#define __DIVA_CFG_LIB_INTERFACE_H__

int diva_cfg_lib_init (void);
int diva_cfg_lib_write (const char* data);
int diva_cfg_lib_read (char** data, int max_length);
void diva_cfg_lib_finit (void);

const byte* diva_cfg_get_binding (const byte* storage,
																	diva_section_target_t application,
																	dword	instance_ident,
																	const byte* instance_name,
																	dword instance_name_length,
																	const byte* section);

const byte* diva_cfg_lib_get_storage(void);

int diva_cfg_get_read_buffer_length (void);

int   diva_cfg_lib_get_owner_data (byte** data, dword owner);
void  diva_cfg_lib_ack_owner_data (dword owner, dword response);
void  diva_cfg_lib_notify_adapter_insertion(void* request);

#endif
