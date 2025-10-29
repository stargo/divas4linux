
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

#ifndef _DIVA_IDI_LOG_RECORD_BASE_H__
#define _DIVA_IDI_LOG_RECORD_BASE_H__

#define DIVA_SUPPORTED_ADAPTERS MIN((MAXIMUM_WAIT_OBJECTS-1),DIVA_MAX_ADAPTERS)
#define MAX_VALNAM_LEN   128
#define SEP_SIZE 4
#define DIVA_SUPPORTED_CHANNELS 8

int diva_rotate_log_file (FILE** pLog);
int diva_write_channel_state (FILE** pLog, int adapter_number, int line_number, const char* data);
int diva_write_interface_state (FILE** Log, int adapter_number, int channel_number, const char* data);
int diva_write_sip_registrar_state (FILE** pLog, const char* data);
int diva_write_adapter_resource (FILE** Log, int adapter_number, const char* data);

/*
	User IPC processing function,
	returns number of messages processed or -1 on error

	mode:
		1 - normal operation
		2 - shutdown in process
		3 - fast shutdown in process
	*/
typedef int (*diva_modules_user_ipc_proc_t)(void* context, int mode);

typedef void (*diva_modules_error_proc_t)(const char* fmt, ...);

/*
	Describes actrual state of the line
	*/
typedef struct _diva_log_record {
	dword       adapter_number; /* Adapter number   */
	dword       line_number;    /* B-channel number */
  const char* line_state_name;

	time_t			off_hook_time;

  /*
		Coded as Address/Subaddress
		*/
	char        localaddress [2*DIVA_TRACE_LINE_TYPE_LEN+1];
	char        remoteaddress[2*DIVA_TRACE_LINE_TYPE_LEN+1];
	/*
		Application that used this B-channel
		*/
	char        application[DIVA_TRACE_LINE_TYPE_LEN];
  /*
		Used B-Channel protocol, coded as L1/L2/L3
		*/
	char        protocol[3*DIVA_TRACE_LINE_TYPE_LEN+2];
	/*
		Charges accumulated by this B-channel
		*/
	dword       charges;
	/*
		Charges for last call
		*/
	dword       last_charges;
	/*
		Overall amount of calls for statistic reason
		*/
	dword       call_number;

	/* Last Disconnect Cause */
	dword				LastDisconnectCause;
	dword				AbandonedCallin;
	dword				AbandonedCallout;

  /*
    Modem Section
    */
  dword max_rx_speed;
  dword min_rx_speed;
  dword max_tx_speed;
  dword min_tx_speed;
  dword retrains;

	/*
		Fax Section
		*/
  dword max_fax_speed;
  dword min_fax_speed;
  dword pages;
  dword fax_features;
	char  FaxID[DIVA_TRACE_FAX_PRMS_LEN];

	dword connection_nr;

	/*
		Channel L1 Stats
		*/
	dword b1_x_frames;
	dword b1_x_bytes;
	dword b1_x_errors;
	dword b1_r_frames;
	dword b1_r_bytes;
	dword b1_r_errors;

	/*
		Channel L2 Stats
		*/
	dword b2_x_frames;
	dword b2_x_bytes;
	dword b2_x_errors;
	dword b2_r_frames;
	dword b2_r_bytes;
	dword b2_r_errors;

	/*
		Sip specifics
		*/
	diva_sip_channel_info_t sip;

} diva_log_record_t;

typedef struct _diva_log_ifc_info {
	diva_trace_interface_state_t state[DIVA_SUPPORTED_CHANNELS];
	diva_ifc_statistics_t        statistics;
} diva_log_ifc_info_t;

typedef struct _diva_log_softip_map {
  int first_softip_adapter;
  int softip_adapter_map[DIVA_SUPPORTED_ADAPTERS+1];
} diva_log_softip_map_t;

typedef struct _user_context {
  FILE* Log[2];
  diva_entity_queue_t* adapters;
  diva_log_softip_map_t softip_adapters;
} user_context_t;
    
typedef struct _diva_adapter {
  diva_entity_link_t link;
  int    adapter_nr; /* Logical adapter number */
  int    board_nr;   /* Board directory (HMP) */
  int    board_adapter_nr; /* Number of the Adapter at this Board (HMP) */
  time_t suspended;  /* If true then adapter is suspended */
  time_t timestamp;  /* Time stamp of last received message */
  time_t stat_update; /* Statistics update timer */
  diva_strace_library_interface_t* hLib;
  diva_trace_library_user_interface_t user;
  int   diva_strace_initialised; /* set to 1 after first statistics update,
                                  set to 2 after creation of adapter directory */
  int   polling_mode;
  int   error;
  int   ifc_available;
  int   adapter_type;
  int   channels;
  HKEY  AdapterKey[8];
  HKEY  ChannelsKey[8];
  HKEY  InfoKey[8];
  HKEY  RegistrarKey; /* only for softip */
  HKEY  BoardKey; /* for Board directory (HMP) */
} diva_adapter_t;


typedef struct {
  int value;
  char name[MAX_VALNAM_LEN];
}file_cont_t, *p_file_cont_t;

typedef struct VAR_LIST_ELEMENT var_list_ele_t;

struct VAR_LIST_ELEMENT{
  var_list_ele_t *  prev;
  var_list_ele_t *  next;
  int  touch;
  char name[MAX_VALNAM_LEN];
  };

typedef struct {
  var_list_ele_t * list_head;
  int  touch;
  char path[MAX_VALNAM_LEN];
}key_ele_t, *p_key_ele_t;

typedef struct _diva_conf{
  key_ele_t  ConfKey;
  key_ele_t  PortKey[8];
 }diva_conf_t;

typedef struct stat_conf{
  int  update_count;
  diva_conf_t * boardconf;
 }stat_conf_t;

int diva_eventlog_layer1 (FILE** Log, int adapter_number, int channel_number, diva_log_ifc_info_t* pInfo);
    
#endif
