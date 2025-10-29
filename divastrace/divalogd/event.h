
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

#ifndef __DIVA_IDI_LOG_EVENT_H__
#define __DIVA_IDI_LOG_EVENT_H__

void diva_log_error_proc (void* user_context,
                          diva_strace_library_interface_t* hLib,
                          int Adapter,
                          int error,
                          const char* file,
                          int line);
void diva_log_notify_proc (void* user_context,
                           diva_strace_library_interface_t* hLib,
                           int Adapter,
                           diva_trace_line_state_t* channel,
                           int notify_subject);
void diva_log_trace_proc (void* user_context,
                          diva_strace_library_interface_t* hLib,
                          int Adapter,
                          void* xlog_buffer);

void diva_log_cleanup_adapter (void* user_context, int adapter);
void diva_log_cleanup_adapter_strace(int adapter);

int diva_log_is_channel_active (int adapter, int channel);
int diva_log_get_adapter_type (int adapter);
int diva_log_get_cardtype (int adapter);
int diva_log_get_channel_count (int adapter);
void * diva_log_get_adapter_config_ptr (int adapter);

#endif
