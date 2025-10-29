
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

#ifndef __DIVA_TRACE_API_OS_H__
#define __DIVA_TRACE_API_OS_H__

void* diva_os_open_adapter  (int   adapter_nr);
int   diva_os_read_descriptor_list (void* request_data,
                                    int request_data_length,
                                    void* indication_data,
                                    int indication_data_length);
int   diva_os_close_adapter (void* handle);
int   diva_os_put_req       (void* handle, const void* data, int length);
int   diva_os_get_message   (void* handle, void* data, int max_length);
void* diva_os_convert_idi_handle_to_waitable_object (void* handle);
int diva_os_report_error_backtrace(char *fmt, ...);

#endif

