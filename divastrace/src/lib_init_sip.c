
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
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "st_ifc.h"
#include "idi_ifs.h"
#include "os.h"
#include "pc.h"
#include "um_xdi.h"
#include "man_defs.h"

#define COMMON_PARSE_ENTRIES  6
#define INFO_PARSE_ENTRIES    5
#define STATE_PARSE_ENTRIES   9
#define STATE_SDP_PARSE_ENTRIES   5


#define DIVALOG_DBG
/* #define SUPER_TRACE_DEBUG */
/* #define DIVA_DEBUG_VAR    */

#ifdef SUPER_TRACE_DEBUG
#define DBG_PRINT(__x__) do { fprintf __x__ ; fflush(logfile); } while(0)
FILE * logfile = stdout;
#else
#define DBG_PRINT(__x__) do { } while(0)
#endif

/*
	PUBLIC FUNCTIONS
	*/
static int SuperTraceSipLibraryFinit (void* hLib);
static void*	SuperTraceSipGetHandle (void* hLib);
static int SuperTraceSipMessageInput (void* hLib);
static int DivaSTraceSipInterfaceGetNrChannels (void* hLib);
static int DivaSTraceSipGetCallData (void* hSipLib, void* hAdapterLib, int Adapter, int Channel, dword CallReference);
static int DivaSTraceSipGetHeartBeat (void* hSipLib);
static int DivaSTraceSipGetRegistrar (void* hSipLib);
#if defined(DIVA_DEBUG_VAR)
static void diva_print_var (diva_man_var_header_t* pVar);
#endif

/*
	LOCAL FUNCTIONS
	*/
static int ScheduleNextTraceRequest (diva_strace_sip_context_t* pLib);
static int process_idi_event (diva_strace_sip_context_t* pLib,
															diva_man_var_header_t* pVar);
static int process_idi_info  (diva_strace_sip_context_t* pLib,
															diva_man_var_header_t* pVar);
static int diva_sip_adapter_info  (diva_strace_sip_context_t* pLib,
														int Adapter,
														diva_man_var_header_t* pVar);
static int diva_sip_channel_info  (diva_strace_sip_context_t* pLib,
														int Adapter, int Channel,
														diva_man_var_header_t* pVar);
static int diva_sip_channel_sdp_info  (diva_strace_sip_context_t* pLib,
														int Adapter, int Channel,
														diva_man_var_header_t* pVar);

static int diva_sip_registrar_info  (diva_strace_sip_context_t* pLib,
														diva_man_var_header_t* pVar);
static diva_man_var_header_t* get_next_var (diva_man_var_header_t* pVar);
static diva_man_var_header_t* find_var (diva_man_var_header_t* pVar,
																				const char* name);
static int diva_strace_read_int  (diva_man_var_header_t* pVar, int* var);
static int diva_strace_read_uint (diva_man_var_header_t* pVar, dword* var);
static int diva_strace_read_asz  (diva_man_var_header_t* pVar, char* var);
static int diva_strace_read_asc  (diva_man_var_header_t* pVar, char* var);
static int diva_strace_read_ie   (diva_man_var_header_t* pVar,
																	diva_trace_ie_t* var);
static void diva_create_sip_parse_table (diva_strace_sip_context_t* pLib);
static void diva_trace_error (diva_strace_sip_context_t* pLib,
															int error, const char* file, int line);
#if 0
static void diva_print_var_name (const char* label,
																 diva_man_var_header_t* pVar);
#endif
static void diva_trace_notify_user (diva_strace_sip_context_t* pLib,
																		int Channel,
																		int notify_subject);
static int diva_trace_read_variable (diva_man_var_header_t* pVar,
																		 void* variable);

/*
 # Softip Adapter initialisation
 */
/*
	Initialize the library and return context
	of the created trace object that will represent
	the IDI adapter.
	Return 0 on error.
	*/
diva_strace_library_sip_interface_t* DivaSTraceLibrarySipCreateInstance (const diva_trace_library_user_interface_t* user_proc) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)malloc(sizeof(*pLib));
	int i,j;
	if (!pLib) {
		return (0);
	}
	memset (pLib, 0x00, sizeof(*pLib));

	pLib->Adapter  = 1007;

	/*
		Set up Library Interface
		*/
	pLib->instance.hLib                                = pLib;
	pLib->instance.DivaSTraceSipLibraryFinit              = SuperTraceSipLibraryFinit;
	pLib->instance.DivaSTraceSipMessageInput              = SuperTraceSipMessageInput;
	pLib->instance.DivaSTraceSipGetHandle                 = SuperTraceSipGetHandle;
	pLib->instance.DivaSTraceSipInterfaceGetNrChannels = DivaSTraceSipInterfaceGetNrChannels;
	pLib->instance.DivaSTraceSipGetCallData						 = DivaSTraceSipGetCallData;
	pLib->instance.DivaSTraceSipGetHeartBeat						 = DivaSTraceSipGetHeartBeat;
	pLib->instance.DivaSTraceSipGetRegistrar						 = DivaSTraceSipGetRegistrar;
	if (user_proc) {
		pLib->user_proc_table.user_context      = user_proc->user_context;
		pLib->user_proc_table.notify_proc       = user_proc->notify_proc;
		pLib->user_proc_table.trace_proc        = user_proc->trace_proc;
		pLib->user_proc_table.error_notify_proc = user_proc->error_notify_proc;
	}

	if ((pLib->hAdapter = SuperTraceOpenAdapter (pLib->Adapter)) == \
																								DIVA_OS_INVALID_HANDLE) {
		free(pLib);
		return (0);
	}

	/*
		Calculate amount of parte table entites necessary to translate
		information from all events of onterest
		*/
	pLib->parse_entries = COMMON_PARSE_ENTRIES \
												+ INFO_PARSE_ENTRIES * DIVA_SIP_MAX_ADAPTERS \
												+ (STATE_PARSE_ENTRIES+STATE_SDP_PARSE_ENTRIES) * DIVA_SIP_MAX_CHANNELS \
												+ 1 ;
	pLib->parse_table = (diva_strace_path2action_t*)malloc (\
											pLib->parse_entries * sizeof(diva_strace_path2action_t));

	if ((!pLib->parse_table) || SuperTraceASSIGN (pLib->hAdapter, pLib->buffer)) {
		SuperTraceCloseAdapter  (pLib->hAdapter);
		free (pLib);
		return (0);
	}
	pLib->req_busy = 1;
	pLib->rc_ok    = ASSIGN_OK;

	pLib->general_config_event = 0;

	diva_create_sip_parse_table(pLib);

	for (i = 0; i < DIVA_SIP_MAX_ADAPTERS; i++) {
		for (j = 1; j <= DIVA_SIP_MAX_CHANNELS / DIVA_SIP_MAX_ADAPTERS; j++) {
			int ch_id = i*30 + (j-1);
			pLib->Channels[ch_id].interface_nr=(byte)i;
			pLib->Channels[ch_id].channel_nr=(byte)j;
		}
	}

#ifdef SUPER_TRACE_DEBUG
	DBG_PRINT((logfile,"initialized strace interface on softip service  -------------------\n"));
#endif

	return ((diva_strace_library_sip_interface_t*)pLib);
}

static int SuperTraceSipLibraryFinit (void* hLib) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hLib;
	if (pLib) {
		if (pLib->parse_table) {
			free (pLib->parse_table);
		}
		if (pLib->hAdapter) {
			SuperTraceCloseAdapter  (pLib->hAdapter);
		}
		free(pLib);
		return (0);
	}
	return (-1);
}

static void*	SuperTraceSipGetHandle (void* hLib) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hLib;
	return (SuperTraceGetWaitableObject (pLib->hAdapter));
}

/*
	After library handle object is in signalled state
	this function should be called and will pick up incoming
	IDI messages
	This function should process messages until no messages is available more.
	*/
static int SuperTraceSipMessageInput (void* hLib) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hLib;
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)pLib->buffer;
	int ret = 0;
  int	one_read = 0;

	while (SuperTraceRead(pLib->hAdapter,pLib->buffer,sizeof(pLib->buffer)) > 0) {
    one_read = 1;
		switch (pInd->type) {
			case DIVA_UM_IDI_IND_RC:
				pLib->req_busy = 0;
				if (pInd->hdr.rc.Rc != pLib->rc_ok) {
					int ignore = 0;

					if (pLib->get_registrar == 2) {
						ignore = 1;
					} else if (pLib->general_config_event == 1) {
						pLib->general_config_event = 2;
						ignore = 1;
					} else if (pLib->general_config_event == 3) {
						pLib->general_config_event = 4;
						ignore = 1;
					} else if (pLib->general_config_event == 5) {
						pLib->general_config_event = 6;
						ignore = 1;
					} else if (pLib->general_config_event == 7) {
						pLib->general_config_event = 8;
						ignore = 1;
					}

					if (!ignore) {
	  			  DBG_PRINT((logfile,"lib_init: !ignore\n"));
						return (-1); /* request failed */
					}
				} else {
					//DBG_PRINT((logfile,"Adaptermap: 1=%d 2=%d 3=%d 4=%d\n",pLib->AdapterMap[0], pLib->AdapterMap[1], pLib->AdapterMap[2], pLib->AdapterMap[3]);
				}

				pLib->rc_ok = 0xff; /* default OK after assign was done */
				if ((ret = ScheduleNextTraceRequest(pLib))) {
				  DBG_PRINT((logfile,"lib_init: ScheduleNextTraceRequest returned error %d\n",ret));
					return (-1);
				}
				break;

			case DIVA_UM_IDI_IND:
				//DBG_PRINT((logfile,"lib_init: pInd->hdr.ind.Ind %d\n",(int)(pInd->hdr.ind.Ind)));
        pLib->wait_for_ind = 0; /* cancel flow control */
				switch (pInd->hdr.ind.Ind) {
					case MAN_COMBI_IND: {
						byte* p = (byte*)&pInd[1];
						int total_length    = (int)pInd->data_length;
						word  this_ind_length;
						byte Ind;

						while (total_length > 3 && *p) {
							Ind = *p++;
							this_ind_length = (word)p[0] | ((word)p[1] << 8);
							p += 2;

							switch (Ind) {
								case MAN_INFO_IND:
									if (process_idi_info (pLib, (diva_man_var_header_t*)p)) {
										DBG_PRINT((logfile,"lib_init: process_idi_info returned error\n"));
										return (-1);
									}
									break;

								case MAN_EVENT_IND:
									if (process_idi_event (pLib, (diva_man_var_header_t*)p)) {
										DBG_PRINT((logfile,"lib_init: process_idi_event returned error\n"));
										return (-1);
									}
									break;

								case MAN_TRACE_IND:
                  /*
                    Deliver XLOG buffer to application
                    */
#if 0
                  if (pLib->user_proc_table.trace_proc) {
                    (*(pLib->user_proc_table.trace_proc))(\
                                  pLib->user_proc_table.user_context,
                                  &pLib->instance, pLib->Adapter,
                                  (void*)&(((MI_XLOG_HDR *)&pInd[1])->code));
                  }
#endif
									break;

								default:
									DBG_PRINT((logfile,"lib_init: inner default branch\n"));
									return (-1);
							}
							p += (this_ind_length+1);
							total_length -= (4 + this_ind_length);
						}
					} break;

					case MAN_INFO_IND:
					  {
              diva_man_var_header_t* pVar = (diva_man_var_header_t*)&pInd[1];
#if 0
              while (pVar) {
#endif
                if (process_idi_info (pLib, pVar)) {
                  DBG_PRINT((logfile,"lib_init: process_idi_info returned error\n"));
                  return (-1);
                }
#if 0
                pVar = get_next_var(pVar);
              }
#endif
						}
						break;

					case MAN_EVENT_IND:
						if (process_idi_event (pLib, (diva_man_var_header_t*)&pInd[1])) {
	  		      DBG_PRINT((logfile,"lib_init: process_idi_event returned error\n"));
							return (-1);
						}
						break;

					case MAN_TRACE_IND:
            /*
              Delivery XLOG buffer to application
              */
            if (pLib->user_proc_table.trace_proc) {
#if 0
              (*(pLib->user_proc_table.trace_proc))(\
                                  pLib->user_proc_table.user_context,
                                  &pLib->instance, pLib->Adapter,
                                  (void*)&(((MI_XLOG_HDR *)&pInd[1])->code));
#endif
            }
						break;

					default:
    	      DBG_PRINT((logfile,"lib_init: inner default branch\n"));
						return (-1);
				}
				break;

			default:
    	  DBG_PRINT((logfile,"lib_init: outer default branch\n"));
				return (-1);
		}
	}
  if (!one_read) {
	  DBG_PRINT((logfile,"lib_init: !one_read\n"));
    return (-1);
  }

	if ((ret = ScheduleNextTraceRequest(pLib))) {
	  DBG_PRINT((logfile,"lib_init: outer ScheduleNextTraceRequest returned error %d\n",ret));
		return (-1);
	}

	return (ret);
}

/*
	Internal state machine responsible for scheduling of requests
	*/
static int ScheduleNextTraceRequest (diva_strace_sip_context_t* pLib) {
	char name[64];
	int i;
	int ret = 0;

	if (pLib->req_busy) {
		return (0);
	}

	if (pLib->wait_for_ind) {
		return (0);
	}

  i = pLib->general_config_event;
  if ((i < 7) && !(i & 0x1)) {
    sprintf (name, "Adapter%d\\Info", i/2);
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, name, pLib->buffer))) {
      return (-1);
    }
    pLib->general_config_event = i+1;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->search_active==1) {
    sprintf (name, "Adapter%d\\State\\B%d", pLib->search_adapter, pLib->search_channel);
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, name, pLib->buffer))) {
      return (-1);
    }
    pLib->search_active=2;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->search_active==3) {
    sprintf (name, "Adapter%d\\State\\B%d\\SDP", pLib->search_adapter, pLib->search_channel);
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, name, pLib->buffer))) {
      return (-1);
    }
    pLib->search_active=4;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->heart_beat==1) {
		sprintf (name, "Config\\callinstrategy");
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, name, pLib->buffer))) {
      return (-1);
    }
    pLib->heart_beat=2;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->get_registrar==1) {
		sprintf (name, "State\\registrar\\registry_server 1");
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, name, pLib->buffer))) {
      return (-1);
    }
    pLib->get_registrar=2;
    pLib->req_busy = 1;
    return (0);
  }

  return (0);
}

static int process_idi_event (diva_strace_sip_context_t* pLib,
															diva_man_var_header_t* pVar) {
	/* UM sources do not support events yet. This is a placeholder for future extensions. */

	const char* path = (char*)&pVar->path_length+1;

	DBG_PRINT((logfile,"process idi event for adapter %d: path \"%s\" ifc_type: %d\n",pLib->Adapter, path));

	if (!strncmp("Indication Queue", path, pVar->path_length)) {
	  /*
	    Indication from virtual event "Indication Queue" indicates
	    that all indications were discarded from application queue.
	    The only way to handle this condition is to restart the
	    Management Interface instance
	    */
		DBG_PRINT((logfile,"process idi event: ERROR - Indication Queue Flushed\n"));
		diva_trace_error (pLib, 1, "Indication Queue Flushed", 0);
		return (-1);
	}

	DBG_PRINT((logfile,"process idi event: Function returns error\n"));
	return (-1);
}

/*
	Process INFO indications that arrive from the card
	Uses path of first I.E. to detect the source of the
	indication
	*/
static int process_idi_info  (diva_strace_sip_context_t* pLib,
															diva_man_var_header_t* pVar) {
	const char* path;
	char name[64];
	int i, len;

	if (pVar == 0) {
		return (-1);
	}

	path = (char*)&pVar->path_length+1;

#ifdef SUPER_TRACE_DEBUG
	memcpy (name, path, pVar->path_length);
	name[pVar->path_length]=0;
  DBG_PRINT((logfile,"process idi info: path \"%s\" Adapter: %d \n", name, pLib->Adapter));
#endif

	/* process bchannel data: step 2 read sdp directory */
	if (pLib->search_active == 4) {
		len = sprintf (name, "Adapter%d\\State\\B%d\\SDP", pLib->search_adapter, pLib->search_channel);
		if (!strncmp(name, path, len)) {
			if (!diva_sip_channel_sdp_info (pLib, pLib->search_adapter, pLib->search_channel, pVar)) {
			  pLib->search_active = 0;
			  return (0);
			}
		}
	}

	/* process bchannel data: step 1 - read directory and compare call reference*/
	len = sprintf (name, "Adapter%d\\State\\B%d", pLib->search_adapter, pLib->search_channel);
	if (!strncmp(name, path, len)) {
		if (!diva_sip_channel_info (pLib, pLib->search_adapter, pLib->search_channel, pVar)) {
			if (pLib->search_active == 2) {
				if (1 || (pLib->search_cr == pLib->Channels[ (pLib->search_adapter*30 + pLib->search_channel - 1) ].cr)) {
				  /* this is my call, read sdp directory and call notify function */
				  pLib->search_active = 3;
				}
			}
		  return (0);
		}
	}

	/* Process heartbeat request */
	len = sprintf (name, "Config\\callinstrategy");
	if (!strncmp(name, path, len)) {
		pLib->heart_beat = 0;
		return(0);
	}

        if (pLib->get_registrar==2) {
	  len = sprintf (name, "State\\registrar\\registry_server 1");
	  if (!strncmp(name, path, len)) {
		if (!diva_sip_registrar_info(pLib, pVar)) {
		  	pLib->get_registrar = 0;
		  	return(0);
		}
	  }
        }


  /* Initialize adapter-map */
  i = pLib->general_config_event;
  if ((i <= 7) && (i & 1)) {
		if (!diva_sip_adapter_info  (pLib, i >> 1, pVar)) {
      pLib->general_config_event++;
		  return (0);
		}
  }

  DBG_PRINT((logfile,"process_idi_info (sip): no variable found\n"));
	return (-1);
} /* process_idi_info */


static int diva_sip_registrar_info  (diva_strace_sip_context_t* pLib,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i;

	for (i = pLib->registrar_parse_first;
			 i <= pLib->registrar_parse_last; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_sip_init: ERROR -3 in diva_sip_registrar_info\n"));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
  		DBG_PRINT((logfile,"lib_sip_init: ERROR -2 in diva_sip_registrar_info\n"));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}
	diva_trace_notify_user (pLib, -1, DIVA_SUPER_TRACE_NOTIFY_SIP_REGISTRAR);
	return (0);
}


static int diva_sip_adapter_info  (diva_strace_sip_context_t* pLib,
														int Adapter,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i;

	for (i  = pLib->info_parse_entry_first[Adapter];
			 i <= pLib->info_parse_entry_last[Adapter]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
				DBG_PRINT((logfile,"lib_sip_init: ERROR -3 in diva_sip_adapter_info for adapter %d\n", Adapter));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
			DBG_PRINT((logfile,"lib_sip_init: ERROR -2 in diva_sip_adapter_info adapter %d\n", Adapter));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}
	return (0);
}


static int diva_sip_channel_info  (diva_strace_sip_context_t* pLib,
														int Adapter, int Channel,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, ch_id = Adapter*30 + Channel - 1;

	for (i  = pLib->state_parse_entry_first[ch_id];
			 i <= pLib->state_parse_entry_last[ch_id]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_sip_init: ERROR -3 in diva_sip_channel_info for adapter %d channel %d\n", Adapter, Channel));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
  		DBG_PRINT((logfile,"lib_sip_init: ERROR -2 in diva_sip_channel_info adapter %d channel %d\n", Adapter, Channel));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}
	return (0);
}


static int diva_sip_channel_sdp_info  (diva_strace_sip_context_t* pLib,
														int Adapter, int Channel,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, ch_id = Adapter*30 + Channel - 1;

	for (i  = pLib->state_sdp_parse_entry_first[ch_id];
			 i <= pLib->state_sdp_parse_entry_last[ch_id]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_sip_init: ERROR -3 in diva_sip_channel_sdp_info for adapter %d channel %d\n", Adapter, Channel));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
  		DBG_PRINT((logfile,"lib_sip_init: ERROR -2 in diva_sip_channel_sdp_info adapter %d channel %d\n", Adapter, Channel));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}
	diva_trace_notify_user (pLib, ch_id, DIVA_SUPER_TRACE_NOTIFY_SIP_CHANGE);
	return (0);
}

/*
	Move position to next variable in the chain
	*/
static diva_man_var_header_t* get_next_var (diva_man_var_header_t* pVar) {
	byte* msg   = (byte*)pVar;
	byte* start;
	int msg_length;

	if (*msg != ESC) return (0);

	start = msg + 2;
	msg_length = *(msg+1);
	msg = (start+msg_length);

	if (*msg != ESC) return (0);

	return ((diva_man_var_header_t*)msg);
}

/*
	Move position to variable with given name
	*/
static diva_man_var_header_t* find_var (diva_man_var_header_t* pVar,
																				const char* name) {
	const char* path;

	do {
		if (pVar && (pVar->path_length==strlen(name))) {
		  path = (char*)&pVar->path_length+1;
 		  if (!strncmp (name, path, pVar->path_length)) {
			  break;
		  }
		}
	} while ((pVar = get_next_var (pVar)));

	return (pVar);
}

static void diva_create_info_parse_table  (diva_strace_sip_context_t* pLib,
																					 int Adapter) {
	if ((pLib->cur_parse_entry + INFO_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}
	pLib->info_parse_entry_first[Adapter] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\Info\\adapter_nr", Adapter);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Interface[Adapter].adapter_nr;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\Info\\serial_nr", Adapter);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Interface[Adapter].serial_nr;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\Info\\channels", Adapter);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Interface[Adapter].channels;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\Info\\name", Adapter);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Interface[Adapter].name;

	pLib->info_parse_entry_last[Adapter] = pLib->cur_parse_entry - 1;
}

static void diva_create_state_parse_table (diva_strace_sip_context_t* pLib,
																					 int Adapter, int Channel) {
	int ch_id = Adapter*30 + (Channel-1);

	if ((pLib->cur_parse_entry + STATE_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}
	pLib->state_parse_entry_first[ch_id] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\IP", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].ip;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\PORT", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].port;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\IPM1", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].ipm1;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\RPM1", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].rpm1;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\LPM1", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].lpm1;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\TRANSPORT", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].transport;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\MEDIA1", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].media1;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\CODEC1", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].codec1;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\CR", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].cr;

	pLib->state_parse_entry_last[ch_id] = pLib->cur_parse_entry - 1;
}


static void diva_create_state_sdp_parse_table (diva_strace_sip_context_t* pLib,
																					 int Adapter, int Channel) {
	int ch_id = Adapter*30 + (Channel-1);

	if ((pLib->cur_parse_entry + STATE_SDP_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}
	pLib->state_sdp_parse_entry_first[ch_id] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\SDP\\SESSION_ID", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].sdp.session_id;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\SDP\\EMAIL_ADDR", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].sdp.email_addr;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\SDP\\PHONE_NR", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].sdp.phone_nr;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\SDP\\MEDIA_NAME", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].sdp.media_name;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "Adapter%d\\State\\B%d\\SDP\\CONN_INFO", Adapter, Channel);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLib->Channels[ch_id].sdp.conn_info;

	pLib->state_sdp_parse_entry_last[ch_id] = pLib->cur_parse_entry - 1;
}


static void diva_create_sip_parse_table (diva_strace_sip_context_t* pLib) {
	int i, j;

	/* Attention: Adapters are counted from zero, channels from one */

	/* Adapter Info directory */
	for (i = 0; i < DIVA_SIP_MAX_ADAPTERS; i++) {
		diva_create_info_parse_table (pLib, i);
	}

	/* Adapter State\\Bx directory */
	for (i = 0; i < DIVA_SIP_MAX_ADAPTERS; i++) {
		for (j = 1; j <= DIVA_SIP_MAX_CHANNELS / DIVA_SIP_MAX_ADAPTERS; j++) {
			diva_create_state_parse_table (pLib, i, j);
		}
	}

	/* Adapter State\\Bx\\SDP directory */
	for (i = 0; i < DIVA_SIP_MAX_ADAPTERS; i++) {
		for (j = 1; j <= DIVA_SIP_MAX_CHANNELS / DIVA_SIP_MAX_ADAPTERS; j++) {
			diva_create_state_sdp_parse_table (pLib, i, j);
		}
	}

	/* General Config and Info */
	if ((pLib->cur_parse_entry + COMMON_PARSE_ENTRIES - 1) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}

	pLib->config_parse_first  = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\read_dnmapfile");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
								&pLib->Config.dnmapfile;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\callinstrategy");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
								&pLib->Config.callinstrategy;

	pLib->config_parse_last  = pLib->cur_parse_entry - 1;

	pLib->registrar_parse_first  = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"State\\registrar\\registry_server 1\\officialsipAddr");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
								&pLib->Config.registrar.sipAddr;


	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"State\\registrar\\registry_server 1\\server");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
								&pLib->Config.registrar.server;


	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"State\\registrar\\registry_server 1\\stateflag");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
								&pLib->Config.registrar.stateflag;


	pLib->registrar_parse_last  = pLib->cur_parse_entry - 1;
}


static void diva_trace_error (diva_strace_sip_context_t* pLib,
															int error, const char* file, int line) {
	if (pLib->user_proc_table.error_notify_proc) {
		(*(pLib->user_proc_table.error_notify_proc))(
																						pLib->user_proc_table.user_context,
																						0 /** \todo provide own user interface type &pLib->instance */, pLib->Adapter,
																						error, file, line);
	}
}

#if 0
static void diva_print_var_name (const char* label,
																 diva_man_var_header_t* pVar) {
	char name[64];
	memcpy (name, (char*)&pVar->path_length+1, pVar->path_length);
	name[pVar->path_length] = 0;
	printf ("%s, VarName=<%s>\n", label, name);
}
#endif

/*
	Deliver notification to user
	*/
static void diva_trace_notify_user (diva_strace_sip_context_t* pLib,
																		int Channel,
																		int notify_subject) {
	int adapter_nr=0; 
	if (Channel >= 0) {
	  adapter_nr = ((diva_strace_context_t*)((diva_strace_library_interface_t*)pLib->hAdapterLib)->hLib)->Adapter; /* very ugly cast */
	} 
	if (pLib->user_proc_table.notify_proc) {
		(*(pLib->user_proc_table.notify_proc))(pLib->user_proc_table.user_context,
																					 pLib->hAdapterLib,
																					 adapter_nr,
																					 (Channel>-1) ? (diva_trace_line_state_t*)&pLib->Channels[Channel] : (diva_trace_line_state_t*)&pLib->Config, 
																					 notify_subject);
	}
}

/*
	Read variable value to their destination based on the variable type
	*/
static int diva_trace_read_variable (diva_man_var_header_t* pVar,
																		 void* variable) {
  const char * path;

	path = (char*)&pVar->path_length+1;

//  DBG_PRINT((logfile,"lib_init: diva_trace_read_variable length: %d \"%s\" type %02x\n",pVar->path_length,path,pVar->type));

	switch (pVar->type) {
		case 0x03: /* MI_ASCIIZ - syting                               */
			return (diva_strace_read_asz  (pVar, (char*)variable));
		case 0x04: /* MI_ASCII  - string                               */
			return (diva_strace_read_asc  (pVar, (char*)variable));
		case 0x05: /* MI_NUMBER - counted sequence of bytes            */
			return (diva_strace_read_ie  (pVar, (diva_trace_ie_t*)variable));
		case 0x81: /* MI_INT    - signed integer                       */
			return (diva_strace_read_int (pVar, (int*)variable));
		case 0x82: /* MI_UINT   - unsigned integer                     */
			return (diva_strace_read_uint (pVar, (dword*)variable));
		case 0x83: /* MI_HINT   - unsigned integer, hex representetion */
			return (diva_strace_read_uint (pVar, (dword*)variable));
		case 0x85: /* MI_BOOLEAN   - boolean (unsigned integer) */
			return (diva_strace_read_uint (pVar, (dword*)variable));
		case 0x86: /* MI_IP_ADDRESS  - dword big endian (NBO) */
			return (diva_strace_read_uint (pVar, (dword*)variable));
		case 0x87: /* MI_BITFLD - unsigned integer, bit representation */
			return (diva_strace_read_uint (pVar, (dword*)variable));
	}

  DBG_PRINT((logfile,"lib_init: ERROR diva_trace_read_variable: type not found\n",path));

	/*
		This type of variable is not handled, indicate error
		Or one problem in management interface, or in application recodeing
		table, or this application should handle it.
		*/
	return (-1);
}

/*
	Read signed integer to destination
	*/
static int diva_strace_read_int  (diva_man_var_header_t* pVar, int* var) {
	byte* ptr = &pVar->path_length;
	int value;

	ptr += (pVar->path_length + 1);

	switch (pVar->value_length) {
		case 1:
			value = *(char*)ptr;
			break;

		case 2:
			value = (short)READ_WORD(ptr);
			break;

		case 4:
			value = (int)READ_DWORD(ptr);
			break;

		default:
			return (-1);
	}

	*var = value;

	return (0);
}

static int diva_strace_read_uint (diva_man_var_header_t* pVar, dword* var) {
	byte* ptr = &pVar->path_length;
	dword value;

	ptr += (pVar->path_length + 1);

	switch (pVar->value_length) {
		case 1:
			value = (byte)(*ptr);
			break;

		case 2:
			value = (word)READ_WORD(ptr);
			break;

		case 3:
			value  = (dword)READ_DWORD(ptr);
			value &= 0x00ffffff;
			break;

		case 4:
			value = (dword)READ_DWORD(ptr);
			break;

		default:
			return (-1);
	}

	*var = value;

	return (0);
}

/*
	Read zero terminated ASCII string
	*/
static int diva_strace_read_asz  (diva_man_var_header_t* pVar, char* var) {
	char* ptr = (char*)&pVar->path_length;
	int length;

	ptr += (pVar->path_length + 1);

	if (!(length = pVar->value_length)) {
		length = (int)(strlen (ptr));
	}
	memcpy (var, ptr, length);
	var[length] = 0;

	return (0);
}

/*
	Read counted (with leading length byte) ASCII string
	*/
static int diva_strace_read_asc  (diva_man_var_header_t* pVar, char* var) {
	char* ptr = (char*)&pVar->path_length;

	ptr += (pVar->path_length + 1);
	memcpy (var, ptr+1, *ptr);
	var[(int)*ptr] = 0;

	return (0);
}

/*
		Read one information element - i.e. one string of byte values with
		one length byte in front
	*/
static int  diva_strace_read_ie  (diva_man_var_header_t* pVar,
																	diva_trace_ie_t* var) {
	char* ptr = (char*)&pVar->path_length;

	ptr += (pVar->path_length + 1);

	var->length = *ptr;
	memcpy (&var->data[0], ptr+1, *ptr);

	return (0);
}

static int DivaSTraceSipInterfaceGetNrChannels (void* hLib) {
  diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hLib;

  return (pLib->channelcount);
}

#if 0
/*
  Update General Config and Info
  */
static int diva_ifc_config (diva_strace_sip_context_t* pLib,
																diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, found_var = 0;

  for (i  = pLib->config_parse_first; i <= pLib->config_parse_last; i++) {
    cur = pVar;
	  if (cur && (cur = find_var (cur, pLib->parse_table[i].path))) {
	    found_var |= 1;

	    if (!strncmp(pLib->parse_table[i].path, "Config\\PRI", sizeof("Config\\PRI")-1)) {
	      if (pLib->InterfaceConfig.type >= DIVA_ADAPTER_TYPE_ANALOG) {
	        continue;  /* already set to anything other than BRI or PRI, ignore variable */
	      }
	    }

		  if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
			  diva_trace_error (pLib, -3 , __FILE__, __LINE__);
			  return (-1); /* error */
		  }

	  }
	}
	return (found_var);
}

#endif

static int DivaSTraceSipGetCallData (void* hSipLib, void* hAdapterLib, int Adapter, int Channel, dword CallReference) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hSipLib;

	pLib->hAdapterLib = hAdapterLib;
	pLib->search_cr = (word) CallReference;
	pLib->search_adapter = Adapter;
	pLib->search_channel = Channel;
	pLib->search_active = 1;

	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceSipGetHeartBeat (void* hSipLib) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hSipLib;
	pLib->heart_beat=1;
	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceSipGetRegistrar (void* hSipLib) {
	diva_strace_sip_context_t* pLib = (diva_strace_sip_context_t*)hSipLib;
	pLib->get_registrar=1;
	return (ScheduleNextTraceRequest (pLib));
}

#if defined(DIVA_DEBUG_VAR)
static void diva_print_var (diva_man_var_header_t* pVar) {
	char name[65];
	char* path;
	int length;
	byte* p = (byte*)pVar;

 	printf (" --------------------------------\n");
	do {
		path = (char*)&pVar->path_length+1;
		if ((length = MIN(sizeof(name)-1, pVar->path_length))) {
			memcpy (name, path, length);
			name[length] = 0;
    	printf ("  Var:<%s>, length:%d\n", name, pVar->path_length);
		} else {
    	printf ("  Var: zero length\n");
		}
	} while ((pVar = get_next_var (pVar)));

 	printf (" --------------------------------\n");
}
#endif
