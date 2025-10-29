
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

#ifndef __DIVA_EICON_TRACE_IDI_IFC_H__
#define __DIVA_EICON_TRACE_IDI_IFC_H__

void* SuperTraceOpenAdapter   (int AdapterNumber);
const dword* SuperTraceReadDescriptorList (void);
int   SuperTraceCloseAdapter  (void* AdapterHandle);
int   SuperTraceWrite         (void* AdapterHandle,
                               const void* data, int length);
int   SuperTraceRead          (void* AdapterHandle, void* data, int max_length);
void* SuperTraceGetWaitableObject (void* AdapterHandle);
int   SuperTraceReadRequest   (void* AdapterHandle,const char* name,byte* data);
word  SuperTraceCreateReadReq (byte* P, const char* path);
int   SuperTraceGetNumberOfChannels (void* AdapterHandle);
unsigned int SuperTraceGetAdapterSerialNumber (void* AdapterHandle);
int   SuperTraceGetAdapterName (void* AdapterHandle, char* data, int max_length);
int   SuperTraceASSIGN        (void* AdapterHandle, byte* data);
int   SuperTraceTraceOnRequest(void* hAdapter, const char* name, byte* data);
int   SuperTraceWriteVar (void* AdapterHandle,
												byte* data,
										 		const char* name,
										 		void* var,
										 		byte type,
										 		byte var_length);
int   SuperTraceExecuteRequest (void* AdapterHandle,
																const char* name,
																byte* data);

typedef struct _diva_strace_path2action {
	char               path[64]; /* Full path to variable            */
	void*							 variable; /* Variable that will receive value */
} diva_strace_path2action_t;

typedef struct _diva_strace_context {
	diva_strace_library_interface_t	instance;

	int   Adapter;
	void* hAdapter;

	int Channels;
	int	req_busy;
	int	wait_for_ind;

	byte buffer[2048*2+1024+512];

	char * line_id;

	int general_config_event;
	int general_info_event;
	int resource_info_req;
	int resource_info_ack;
	int resource_info_state;

	int law_req;
	int law_ack;
	int law_state;
	char law;

  int general_b_ch_event;
  int general_fax_event;
  int general_mdm_event;
  int general_cardtype_event;

  int temperature_event;
  int temperature_read_state;

	int identify_start;
	int identify_stop;

	int initial_cfg_complete_event;

	byte	rc_ok;

	/*
		Initialization request state machine
		*/
	int ChannelsTraceActive;
	int ModemTraceActive;
	int FaxTraceActive;
	int IncomingCallsCallsActive;
	int IncomingCallsConnectedActive;
	int OutgoingCallsCallsActive;
	int OutgoingCallsConnectedActive;

	int trace_mask_init;
	int audio_trace_init;
	int bchannel_init;
	int trace_length_init;
	int	trace_on;
	int trace_events_down;
	int l1_trace;
	int l2_trace;
	int alarms_trace;

	int channel_b1_b2_statistics_available;

	/*
		Trace\Event Enable
		*/
	word trace_event_mask;
	word current_trace_event_mask;

	dword audio_tap_mask;
	dword current_audio_tap_mask;

	dword bchannel_trace_mask;
	dword current_bchannel_trace_mask;


	diva_trace_line_state_t lines[31];

	int	parse_entries;
	int	cur_parse_entry;
	diva_strace_path2action_t* parse_table;

	diva_trace_library_user_interface_t user_proc_table;

	int line_parse_entry_first[31];
	int line_parse_entry_last[31];

	int modem_parse_entry_first[31];
	int modem_parse_entry_last[31];

	int fax_parse_entry_first[31];
	int fax_parse_entry_last[31];

	int channel_b1_parse_entry_first[31];
	int channel_b1_parse_entry_last[31];

	int channel_b2_parse_entry_first[31];
	int channel_b2_parse_entry_last[31];

	int statistic_parse_first;
	int statistic_parse_last;

	int config_parse_first;
	int config_parse_last;

	int info_parse_first;
	int info_parse_last;

	int mdm_statistic_parse_first;
	int mdm_statistic_parse_last;

	int fax_statistic_parse_first;
	int fax_statistic_parse_last;

	dword	line_init_event;
	dword	modem_init_event;
	dword	fax_init_event;
	dword	analog_init_event;

	dword	pending_line_status;
	dword	pending_modem_status;
	dword	pending_fax_status;

	dword clear_call_command;

	int outgoing_ifc_stats;
	int incoming_ifc_stats;
	int l1_ifc_stats;
	int modem_ifc_stats;
	int fax_ifc_stats;
	int b1_ifc_stats;
	int b2_ifc_stats;
	int d1_ifc_stats;
	int d2_ifc_stats;

	dword channel_b1_stat;
	dword channel_b2_stat;

	diva_trace_interface_state_t Interface;
	diva_ifc_statistics_t				 InterfaceStat;
	diva_ifc_config_t						 InterfaceConfig;

	int time_update_state;
	int time_information_update_state;
	dword tz_offset;
	int   dst_state;
	int   sync_state;
} diva_strace_context_t;

#define DIVA_SIP_MAX_CHANNELS		120
#define DIVA_SIP_MAX_ADAPTERS		  4

typedef struct _diva_strace_sip_context {
	diva_strace_library_sip_interface_t	instance;

	int   Adapter;
	void* hAdapter;

	int AdapterMap[DIVA_SIP_MAX_ADAPTERS];
	int channelcount;
	int	req_busy;
	int	wait_for_ind;

	/* call lookup state variables */
	void* hAdapterLib;
	word search_cr;
	int search_active;
	int search_adapter;
	int search_channel;

	byte buffer[2048*2+1024+512];

	int general_config_event;
	int heart_beat;
	int get_registrar;
	byte	rc_ok;

	/*
		Initialization request state machine
		*/

	int	parse_entries;
	int	cur_parse_entry;
	diva_strace_path2action_t* parse_table;

	diva_trace_library_user_interface_t user_proc_table;

	int info_parse_entry_first[DIVA_SIP_MAX_ADAPTERS];
	int info_parse_entry_last[DIVA_SIP_MAX_ADAPTERS];
	int state_parse_entry_first[DIVA_SIP_MAX_CHANNELS];
	int state_parse_entry_last[DIVA_SIP_MAX_CHANNELS];
	int state_sdp_parse_entry_first[DIVA_SIP_MAX_CHANNELS];
	int state_sdp_parse_entry_last[DIVA_SIP_MAX_CHANNELS];

	int config_parse_first;
	int config_parse_last;

	int registrar_parse_first;
	int registrar_parse_last;

	dword	line_init_event;
	dword	pending_line_status;

	diva_sip_info_t Config;
	diva_sip_ifc_info_t Interface[DIVA_SIP_MAX_ADAPTERS];
	diva_sip_channel_info_t Channels[DIVA_SIP_MAX_CHANNELS];
} diva_strace_sip_context_t;

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

#endif
