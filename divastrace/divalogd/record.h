
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

#ifndef __DIVA_LOG_RECORD_H__
#define __DIVA_LOG_RECORD_H__

struct _diva_log_ifc_info;
struct _diva_resource_info_entry;

int diva_write_log_header (FILE** Log);
int diva_write_log_suffix (FILE** Log);
const char* diva_get_call_record_description(void);
const char* diva_get_interface_state_description (void);
const char* diva_get_interface_info_description (void);
const char* diva_get_registrar_description (void);
const char* diva_get_resource_description(void);


int diva_write_log_record (FILE** Log,
													 diva_log_record_t* pState,
													 const char* adapter_name,
													 dword adapter_serial_number);
int diva_write_call_log_record (FILE* Log, diva_log_record_t* pState, int online);
int diva_update_channel_state (FILE** pLog,
															 diva_log_record_t* pState,
															 const char* adapter_name,
															 dword adapter_serial_number);
int diva_update_ifc_state (FILE** pLog,
													 int adapter_number,
													 int channel_number,
													 struct _diva_log_ifc_info* pInfo);

int diva_write_registrar_record (FILE** Log, diva_sip_info_t* sip_config);

int diva_update_adapter_resource (FILE** Log,
																	int adapter_number,
																	const struct _diva_resource_info_entry* resource);

#define DIVA_MAX_RECORDS_PER_FILE 10000

#endif

