
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
#include <stdlib.h>

#include "st_ifc.h"
#include "idi_ifs.h"
#include "os.h"
#include "pc.h"
#include "um_xdi.h"
#include "man_defs.h"
#define CARDTYPE_H_WANT_DATA 1
#include "cardtype.h"

#define MODEM_PARSE_ENTRIES  16 /* amount of variables of interest */
#define FAX_PARSE_ENTRIES    35 /* amount of variables of interest */
#define LINE_PARSE_ENTRIES   17 /* amount of variables of interest */
#define STAT_PARSE_ENTRIES   200 /* amount of variables of interest */
#define CHANNEL_B1_STAT_PARSE_ENTRIES 6
#define CHANNEL_B2_STAT_PARSE_ENTRIES 6

#define LINE_PARSE_ENTRIES_MAX_MISS   2 /* amount of variables which might be missing */


#define LINE_ID_ISDN		"B"
#define LINE_ID_ANALOG	"Line-"

char* LINE_ID[] = { LINE_ID_ISDN, LINE_ID_ANALOG };

#define DIVA_RESOURCE_PATH               "Info\\Resource"
#define DIVA_RESOURCE_PATH_LENGTH        (sizeof(DIVA_RESOURCE_PATH)-1)
#define DIVA_RESOURCE_PATH_PREFIX        DIVA_RESOURCE_PATH"\\"
#define DIVA_RESOURCE_PATH_PREFIX_LENGTH (sizeof(DIVA_RESOURCE_PATH_PREFIX)-1)

#define DIVA_LAW_PATH        "Config\\B-Layer1\\u-Law"
#define DIVA_LAW_PATH_LENGTH (sizeof(DIVA_LAW_PATH)-1)

#define DIVALOG_DBG
/* #define SUPER_TRACE_DEBUG	*/
/* #define DIVA_DEBUG_VAR    */

#ifdef SUPER_TRACE_DEBUG
#define DBG_PRINT(__x__) do { fprintf __x__ ; fflush(logfile); } while(0)
FILE * logfile;
#else
#define DBG_PRINT(__x__) do { } while(0)
#endif

/*
	LOCAL FUNCTIONS
	*/
static int SuperTraceLibraryFinit (void* hLib);
static void*	SuperTraceGetHandle (void* hLib);
static int SuperTraceMessageInput (void* hLib);
static int SuperTraceSetAudioTap  (void* hLib, int Channel, int on);
static int SuperTraceSetBChannel  (void* hLib, int Channel, int on);
static int SuperTraceSetDChannel  (void* hLib, int on);
static int SuperTraceSetInfo      (void* hLib, int on);
static int SuperTraceClearCall (void* hLib, int Channel);
static int SuperTraceGetOutgoingCallStatistics (void* hLib);
static int SuperTraceGetIncomingCallStatistics (void* hLib);
static int SuperTraceGetLayer1Statistics (void* hLib);
static int SuperTraceGetModemStatistics (void* hLib);
static int SuperTraceGetFaxStatistics (void* hLib);
static int SuperTraceGetBLayer1Statistics (void* hLib);
static int SuperTraceGetBLayer2Statistics (void* hLib);
static int SuperTraceGetDLayer1Statistics (void* hLib);
static int SuperTraceGetDLayer2Statistics (void* hLib);
static int DivaSTraceGetBLayer1ChannelStatistics (void* hLib, int channel);
static int DivaSTraceGetBLayer2ChannelStatistics (void* hLib, int channel);
static int DivaSTraceInterfaceIdentify (void* hLib, int on);
static int DivaSTraceInterfaceDisableTrace (void* hLib);
static int DivaSTraceInterfaceGetNrChannels (void* hLib);
static int DivaSTraceSetInterfaceFeatures (void* hLib, diva_strace_ifc_features_t features);
static int SuperTraceGetResourceInfo (void* hLib);
static int SuperTraceUpdateTime (void* hLib);

#if defined(DIVA_DEBUG_VAR)
static void diva_print_var (diva_man_var_header_t* pVar);
#endif

/*
	LOCAL FUNCTIONS
	*/
static int ScheduleNextTraceRequest (diva_strace_context_t* pLib);
static int process_idi_event (diva_strace_context_t* pLib,
															diva_man_var_header_t* pVar);
static int process_idi_info  (diva_strace_context_t* pLib,
															diva_man_var_header_t* pVar);
static int diva_modem_event (diva_strace_context_t* pLib, int Channel);
static int diva_fax_event   (diva_strace_context_t* pLib, int Channel);
static int diva_line_event (diva_strace_context_t* pLib, int Channel);
static int diva_modem_info (diva_strace_context_t* pLib,
														int Channel,
														diva_man_var_header_t* pVar);
static int diva_fax_info   (diva_strace_context_t* pLib,
														int Channel,
														diva_man_var_header_t* pVar);
static int diva_channel_b1_stat_info (diva_strace_context_t* pLib,
																			int Channel,
																			diva_man_var_header_t* pVar);
static int diva_channel_b2_stat_info (diva_strace_context_t* pLib,
																			int Channel,
																			diva_man_var_header_t* pVar);
static int diva_line_info  (diva_strace_context_t* pLib,
														int Channel,
														diva_man_var_header_t* pVar);
static int diva_ifc_config     (diva_strace_context_t* pLib,
																diva_man_var_header_t* pVar);
static int diva_ifc_info     (diva_strace_context_t* pLib,
																diva_man_var_header_t* pVar);
static int diva_ifc_statistics (diva_strace_context_t* pLib,
																diva_man_var_header_t* pVar);
static int diva_temperature (diva_strace_context_t* pLib,
 														 diva_man_var_header_t* pVar);
static diva_man_var_header_t* get_next_var (diva_man_var_header_t* pVar);
static diva_man_var_header_t* find_var (diva_man_var_header_t* pVar,
																				const char* name);
static int locate_var (const diva_man_var_header_t* pVar, const char* name, int name_length);
static int diva_strace_read_int  (diva_man_var_header_t* pVar, int* var);
static int diva_strace_read_uint (diva_man_var_header_t* pVar, dword* var);
static int diva_strace_read_asz  (diva_man_var_header_t* pVar, char* var);
static int diva_strace_read_asc  (diva_man_var_header_t* pVar, char* var);
static int diva_strace_read_ie   (diva_man_var_header_t* pVar,
																	diva_trace_ie_t* var);
static void diva_create_parse_table (diva_strace_context_t* pLib);
static void diva_create_channel_b1_stat_parse_table (diva_strace_context_t* pLib, int Channel);
static void diva_create_channel_b2_stat_parse_table (diva_strace_context_t* pLib, int Channel);
static void diva_trace_error (diva_strace_context_t* pLib,
															int error, const char* file, int line);
#if 0
static void diva_print_var_name (const char* label,
																 diva_man_var_header_t* pVar);
#endif
static void diva_trace_notify_user (diva_strace_context_t* pLib,
																		int Channel,
																		int notify_subject);
static int diva_trace_read_variable (diva_man_var_header_t* pVar,
																		 void* variable);

static int diva_init_cardtype (diva_strace_context_t* pLib, dword ct);

/*
	Initialize the library and return context
	of the created trace object that will represent
	the IDI adapter.
	Return 0 on error.
	*/
diva_strace_library_interface_t* DivaSTraceLibraryCreateInstance (int Adapter,
											const diva_trace_library_user_interface_t* user_proc) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)malloc(sizeof(*pLib));
	int i;

	if (!pLib) {
		return (0);
	}
	memset (pLib, 0x00, sizeof(*pLib));

	pLib->Adapter  = Adapter;

	/*
		Set up Library Interface
		*/
	pLib->instance.hLib                                = pLib;
	pLib->instance.DivaSTraceLibraryFinit              = SuperTraceLibraryFinit;
	pLib->instance.DivaSTraceMessageInput              = SuperTraceMessageInput;
	pLib->instance.DivaSTraceGetHandle                 = SuperTraceGetHandle;
	pLib->instance.DivaSTraceSetAudioTap               = SuperTraceSetAudioTap;
	pLib->instance.DivaSTraceSetBChannel               = SuperTraceSetBChannel;
	pLib->instance.DivaSTraceSetDChannel               = SuperTraceSetDChannel;
	pLib->instance.DivaSTraceSetInfo                   = SuperTraceSetInfo;
	pLib->instance.DivaSTraceGetOutgoingCallStatistics = \
																			SuperTraceGetOutgoingCallStatistics;
	pLib->instance.DivaSTraceGetIncomingCallStatistics = \
																			SuperTraceGetIncomingCallStatistics;
	pLib->instance.DivaSTraceGetLayer1Statistics = \
																			SuperTraceGetLayer1Statistics;
	pLib->instance.DivaSTraceGetModemStatistics        = \
																			SuperTraceGetModemStatistics;
	pLib->instance.DivaSTraceGetFaxStatistics          = \
																			SuperTraceGetFaxStatistics;
	pLib->instance.DivaSTraceGetBLayer1Statistics      = \
																			SuperTraceGetBLayer1Statistics;
	pLib->instance.DivaSTraceGetBLayer2Statistics      = \
																			SuperTraceGetBLayer2Statistics;
	pLib->instance.DivaSTraceGetDLayer1Statistics      = \
																			SuperTraceGetDLayer1Statistics;
	pLib->instance.DivaSTraceGetDLayer2Statistics      = \
																			SuperTraceGetDLayer2Statistics;
	pLib->instance.DivaSTraceClearCall                 = SuperTraceClearCall;
	pLib->instance.DivaSTraceInterfaceIdentify         = DivaSTraceInterfaceIdentify;
	pLib->instance.DivaSTraceGetBLayer1ChannelStatistics = DivaSTraceGetBLayer1ChannelStatistics;
	pLib->instance.DivaSTraceGetBLayer2ChannelStatistics = DivaSTraceGetBLayer2ChannelStatistics;

	pLib->instance.DivaSTraceInterfaceDisableTrace  = DivaSTraceInterfaceDisableTrace;
	pLib->instance.DivaSTraceInterfaceGetNrChannels = DivaSTraceInterfaceGetNrChannels;
	pLib->instance.DivaSTraceSetInterfaceFeatures   = DivaSTraceSetInterfaceFeatures;
	pLib->instance.DivaSTraceGetResourceInfo        = SuperTraceGetResourceInfo;
	pLib->instance.DivaSTraceUpdateTime             = SuperTraceUpdateTime;

	if (user_proc) {
		pLib->user_proc_table.user_context      = user_proc->user_context;
		pLib->user_proc_table.notify_proc       = user_proc->notify_proc;
		pLib->user_proc_table.trace_proc        = user_proc->trace_proc;
		pLib->user_proc_table.error_notify_proc = user_proc->error_notify_proc;
	}

	if ((pLib->hAdapter = SuperTraceOpenAdapter (Adapter)) == \
																								DIVA_OS_INVALID_HANDLE) {
		free(pLib);
		return (0);
	}

	if (!(pLib->Channels = SuperTraceGetNumberOfChannels (pLib->hAdapter))) {
		SuperTraceCloseAdapter  (pLib->hAdapter);
		free(pLib);
		return (0);
	}
	if ((pLib->instance.adapter_serial_number = SuperTraceGetAdapterSerialNumber (pLib->hAdapter)) ==
		                                                                      0xffffffff) {
		SuperTraceCloseAdapter  (pLib->hAdapter);
		free(pLib);
		return (0);
	}

	if (SuperTraceGetAdapterName (pLib->hAdapter,
																&pLib->instance.adapter_name[0],
																sizeof(pLib->instance.adapter_name)) < 0) {
		SuperTraceCloseAdapter  (pLib->hAdapter);
		free(pLib);
		return (0);
	}

	/*
		Calculate amount of parte table entites necessary to translate
		information from all events of onterest
		*/
	pLib->parse_entries = (MODEM_PARSE_ENTRIES + FAX_PARSE_ENTRIES + \
												 STAT_PARSE_ENTRIES + \
												 CHANNEL_B1_STAT_PARSE_ENTRIES + \
												 CHANNEL_B2_STAT_PARSE_ENTRIES + \
												 LINE_PARSE_ENTRIES + 1) * pLib->Channels;
	pLib->parse_table = (diva_strace_path2action_t*)malloc (\
											pLib->parse_entries * sizeof(diva_strace_path2action_t));

	pLib->Interface.pConfig = &pLib->InterfaceConfig;
	pLib->Interface.pConfig->channels = pLib->Channels;

  pLib->line_id = LINE_ID[0];  /* "Line-" or "B", defaults to "B" */

	for (i = 0; i < 31; i++) {
		pLib->lines[i].pInterface       = &pLib->Interface;
		pLib->lines[i].pInterfaceStat   = &pLib->InterfaceStat;
	}

	if ((!pLib->parse_table) || SuperTraceASSIGN (pLib->hAdapter, pLib->buffer)) {
		SuperTraceCloseAdapter  (pLib->hAdapter);
		free (pLib);
		return (0);
	}
	pLib->req_busy = 1;
	pLib->rc_ok    = ASSIGN_OK;

#ifdef SUPER_TRACE_DEBUG
//  logfile=fopen("c:\\strace.txt","a");
  logfile = stdout;
	DBG_PRINT((logfile,"initialized strace interface on adapter %d -------------------\n",Adapter));
#endif

	pLib->time_update_state = diva_os_get_time_info (0, 0, 0, 0, 0) == 0 ? 1 : -1;
	pLib->dst_state = 2; /* trigger time information update */

	return ((diva_strace_library_interface_t*)pLib);
}

const dword* DivaSTraceLibraryGetDescriptorList (void) {
	return (SuperTraceReadDescriptorList());
}

static int SuperTraceLibraryFinit (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
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

static void*	SuperTraceGetHandle (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	return (SuperTraceGetWaitableObject (pLib->hAdapter));
}

/*
	After library handle object is gone in signaled state
	this function should be called and will pick up incoming
	IDI messages
	This function should process messages until no messages is available more.
	*/
static int SuperTraceMessageInput (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
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

					/*
						Auto-detect amount of events/channels and features
					*/
					if (pLib->time_information_update_state == 1) {
						pLib->time_information_update_state = -1;
						ignore = 1;
					} else if (pLib->time_update_state == 2) {
						pLib->time_update_state = -1;
						ignore = 1;
					} else if (pLib->law_state == 1) {
						pLib->law_state = -1;
					} else if (pLib->resource_info_state == 1) {
						pLib->resource_info_state = -1;
						ignore = 1;
					} else if (pLib->identify_start == 2) {
						pLib->identify_start = 0;
						ignore = 1;
					} else if (pLib->identify_stop == 2) {
						pLib->identify_stop = 0;
						ignore = 1;
					} else if (pLib->temperature_event == 1) {
						pLib->temperature_event = 2;
						ignore = 1;
					} else if (pLib->general_info_event == 1) {
					  pLib->general_info_event = 2;
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
						pLib->general_config_event = 8; /* end */
						ignore = 1;
					} else if (pLib->general_b_ch_event == 1) {
						pLib->general_b_ch_event = 2;
						ignore = 1;
					} else if (pLib->general_fax_event == 1) {
						pLib->general_fax_event = 2;
						ignore = 1;
					} else if (pLib->general_mdm_event == 1) {
						pLib->general_mdm_event = 2;
						ignore = 1;
					} else if ((pLib->ChannelsTraceActive < pLib->Channels) && pLib->ChannelsTraceActive) {
						pLib->ChannelsTraceActive = pLib->Channels;
						ignore = 1;
					} else if (pLib->ModemTraceActive < pLib->Channels) {
						pLib->ModemTraceActive = pLib->Channels;
						ignore = 1;
					} else if (pLib->FaxTraceActive < pLib->Channels) {
						pLib->FaxTraceActive = pLib->Channels;
						ignore = 1;
					} else if (pLib->audio_trace_init == 2) {
						ignore = 1;
						pLib->audio_trace_init = 1;
					} else if (pLib->l2_trace == 1) {
						ignore = 1;
						pLib->l2_trace = 3; /* ignore missing State\Layer2 No1 directory */
					}

					if (!ignore) {
	  			  // printf("lib_init: !ignore\n");
	  			  DBG_PRINT((logfile,"lib_init: !ignore\n"));
						return (-1); /* request failed */
					}
				} else {
					if (pLib->time_information_update_state == 1) {
						pLib->time_information_update_state = 0;
					} else if (pLib->time_update_state == 2) {
						pLib->time_update_state = 0;
					} else if (pLib->law_state == 1) {
						pLib->law_state = 2;
					} else if (pLib->resource_info_state == 1) {
						pLib->resource_info_state = 2;
					} else if (pLib->identify_start == 2) {
						pLib->identify_start = 0;
					} else if (pLib->identify_stop == 2) {
						pLib->identify_stop = 0;
					} else if (pLib->temperature_event == 1) {
						pLib->temperature_event = 2;
					} else if (pLib->general_cardtype_event == 1) {
					  pLib->general_cardtype_event = 2; /* read Config/CardType */
					} else if (pLib->general_info_event == 1) {
					  pLib->general_info_event = 2;   /* read Info */
					} else if (pLib->general_config_event == 1) {
					  pLib->general_config_event = 2; /* read Config */
					} else if (pLib->general_config_event == 3) {
					  pLib->general_config_event = 4; /* read Config\DChannel\Protocol */
					} else if (pLib->general_config_event == 5) {
					  pLib->general_config_event = 6; /* read Config\SPID-1\TEI */
					} else if (pLib->general_config_event == 7) {
					  pLib->general_config_event = 8; /* read StableL2 */
					} else if (pLib->general_b_ch_event == 1) {
						pLib->ChannelsTraceActive = pLib->Channels;
						pLib->general_b_ch_event = 2;
					} else if (pLib->general_fax_event == 1) {
						pLib->general_fax_event = 2;
						pLib->FaxTraceActive = pLib->Channels;
						pLib->channel_b1_b2_statistics_available = 1;
					} else if (pLib->general_mdm_event == 1) {
						pLib->general_mdm_event = 2;
						pLib->ModemTraceActive = pLib->Channels;
					}
				}

				if (pLib->audio_trace_init == 2) {
					pLib->audio_trace_init = 1;
				}
				pLib->rc_ok = 0xff; /* default OK after assign was done */
				if ((ret = ScheduleNextTraceRequest(pLib))) {
				  //printf("lib_init: ScheduleNextTraceRequest returned error %d\n",ret);
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
										//printf("lib_init: process_idi_info returned error\n");
										DBG_PRINT((logfile,"lib_init: process_idi_info returned error\n"));
										return (-1);
									}
									break;

								case MAN_EVENT_IND:
									if (process_idi_event (pLib, (diva_man_var_header_t*)p)) {
										//printf("lib_init: process_idi_event returned error\n");
										DBG_PRINT((logfile,"lib_init: process_idi_event returned error\n"));
										return (-1);
									}
									break;

								case MAN_TRACE_IND:
									if (pLib->trace_on == 1) {
										/*
											Ignore first trace event that is result of
											EVENT_ON operation
											*/
										pLib->trace_on++;
									} else {
										/*
											Delivery XLOG buffer to application
											*/
										if (pLib->user_proc_table.trace_proc) {
											(*(pLib->user_proc_table.trace_proc))(\
																		pLib->user_proc_table.user_context,
																		&pLib->instance, pLib->Adapter,
																		(void*)&(((MI_XLOG_HDR *)p))->code);
										}
									}
									break;

								default:
									//printf("lib_init: inner default branch\n");
									DBG_PRINT((logfile,"lib_init: inner default branch\n"));
									return (-1);
							}
							p += (this_ind_length+1);
							total_length -= (4 + this_ind_length);
						}
					} break;

					case MAN_INFO_IND:
						if (process_idi_info (pLib, (diva_man_var_header_t*)&pInd[1])) {
	  		      //printf("lib_init: process_idi_info returned error\n");
	  		      DBG_PRINT((logfile,"lib_init: process_idi_info returned error\n"));
							return (-1);
						}
						break;

					case MAN_EVENT_IND:
						if (process_idi_event (pLib, (diva_man_var_header_t*)&pInd[1])) {
	  		      //printf("lib_init: process_idi_event returned error\n");
	  		      DBG_PRINT((logfile,"lib_init: process_idi_event returned error\n"));
							return (-1);
						}
						break;

					case MAN_TRACE_IND:
						if (pLib->trace_on == 1) {
							/*
								Ignore first trace event that is result of
								EVENT_ON operation
								*/
							pLib->trace_on++;
						} else {
							/*
								Delivery XLOG buffer to application
								*/
							if (pLib->user_proc_table.trace_proc) {
								(*(pLib->user_proc_table.trace_proc))(\
																		pLib->user_proc_table.user_context,
																		&pLib->instance, pLib->Adapter,
																		(void*)&(((MI_XLOG_HDR *)&pInd[1])->code));
							}
						}
						break;

					default:
    	      //printf("lib_init: inner default branch\n");
    	      DBG_PRINT((logfile,"lib_init: inner default branch\n"));
						return (-1);
				}
				break;

			default:
    	  //printf("lib_init: outer default branch\n");
    	  DBG_PRINT((logfile,"lib_init: outer default branch\n"));
				return (-1);
		}
	}
  if (!one_read) {
	  //printf("lib_init: !one_read\n");
	  DBG_PRINT((logfile,"lib_init: !one_read\n"));
    return (-1);
  }

	if ((ret = ScheduleNextTraceRequest(pLib))) {
	  //printf("lib_init: outer ScheduleNextTraceRequest returned error %d\n",ret);
	  DBG_PRINT((logfile,"lib_init: outer ScheduleNextTraceRequest returned error %d\n",ret));
		return (-1);
	}

	return (ret);
}

/*
	Internal state machine responsible for scheduling of requests
	*/
static int ScheduleNextTraceRequest (diva_strace_context_t* pLib) {
	char name[64];
	int ret = 0;
	int i;

	if (pLib->req_busy) {
		return (0);
	}

	if (pLib->wait_for_ind) {
		return (0);
	}

  if (!pLib->general_cardtype_event) {
    /* detect analog board upon presence of "Line Event" variable */
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Config\\CardType", pLib->buffer))) {
      return (-1);
    }
    pLib->general_cardtype_event = 1;
    pLib->req_busy = 1;
    pLib->wait_for_ind = 1; /* set flow control */
    return (0);
  }

  if (!pLib->general_config_event) {
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Config", pLib->buffer))) {
      return (-1);
    }
    pLib->general_config_event = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->general_config_event==2) { /* If Dchannel protocol not found under "Config\Protocol" try in Config\Dchannel\Protocol*/
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Config\\DChannel\\Protocol", pLib->buffer))) {
      return (-1);
    }
    pLib->general_config_event = 3;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->general_config_event==4) {
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Config\\SPID-1\\TEI", pLib->buffer))) {
      return (-1);
    }
    pLib->general_config_event = 5;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->general_config_event==6) {
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Config\\Layer2\\L2", pLib->buffer))) {
      return (-1);
    }
    pLib->general_config_event = 7;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->general_b_ch_event) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
      if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, "State\\B Event", pLib->buffer))) {
        return (-1);
      }
    } else {
      if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, "State\\Line Event", pLib->buffer))) {
        return (-1);
      }
    }
    pLib->general_b_ch_event = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->general_fax_event) {
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, "State\\FAX Event", pLib->buffer))) {
      return (-1);
    }
    pLib->general_fax_event = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->general_mdm_event) {
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, "State\\Modem Event", pLib->buffer))) {
      return (-1);
    }
    pLib->general_mdm_event = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->general_info_event) {
    if ((ret = SuperTraceReadRequest(pLib->hAdapter, "Info", pLib->buffer))) {
      return (-1);
    }
    pLib->general_info_event = 1;
    pLib->req_busy = 1;
    return (0);
  }

 if (pLib->law_state >= 0) {
    if (pLib->law_state == 0 || pLib->law_req != pLib->law_ack) {
      if (SuperTraceReadRequest (pLib->hAdapter, DIVA_LAW_PATH, pLib->buffer)) {
        pLib->law_state = -1;
        return (-1);
      }
      if (pLib->law_state == 0) {
        pLib->law_state = 1;
      }
      pLib->law_ack = pLib->law_req;
      pLib->req_busy = 1;
      return (0);
    }
  }

  if (pLib->ChannelsTraceActive < pLib->Channels) {
    pLib->ChannelsTraceActive++;
    sprintf (name, "State\\%s%d\\Line", pLib->line_id, pLib->ChannelsTraceActive);
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
      pLib->ChannelsTraceActive--;
      return (-1);
    }
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->ModemTraceActive < pLib->Channels) {
    pLib->ModemTraceActive++;
    sprintf (name, "State\\%s%d\\Modem\\Event", pLib->line_id, pLib->ModemTraceActive);
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
      pLib->ModemTraceActive--;
      return (-1);
    }
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->FaxTraceActive < pLib->Channels) {
    pLib->FaxTraceActive++;
    sprintf (name, "State\\%s%d\\FAX\\Event", pLib->line_id, pLib->FaxTraceActive);
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
      pLib->FaxTraceActive--;
      return (-1);
    }
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->trace_mask_init) {
    word tmp = 0x0000;
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\Event Enable",
                            &tmp,
                            0x87, /* MI_BITFLD */
                            sizeof(tmp))) {
      return (-1);
    }
    pLib->trace_mask_init = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->audio_trace_init) {
    dword tmp = 0x00000000;
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\AudioCh# Enable",
                            &tmp,
                            0x87, /* MI_BITFLD */
                            sizeof(tmp))) {
      return (-1);
    }
    pLib->audio_trace_init = 2;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->bchannel_init) {
    dword tmp = 0x00000000;
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\B-Ch# Enable",
                            &tmp,
                            0x87, /* MI_BITFLD */
                            sizeof(tmp))) {
      return (-1);
    }
    pLib->bchannel_init = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->trace_length_init) {
    word tmp = 512;
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\Max Log Length",
                            &tmp,
                            0x82, /* MI_UINT */
                            sizeof(tmp))) {
      return (-1);
    }
    pLib->trace_length_init = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->trace_on) {
    if (SuperTraceTraceOnRequest (pLib->hAdapter,
                                  "Trace\\Log Buffer",
                                  pLib->buffer)) {
      return (-1);
    }
    pLib->trace_on = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->trace_event_mask != pLib->current_trace_event_mask) {
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\Event Enable",
                            &pLib->trace_event_mask,
                            0x87, /* MI_BITFLD */
                            sizeof(pLib->trace_event_mask))) {
      return (-1);
    }
    pLib->current_trace_event_mask = pLib->trace_event_mask;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->audio_tap_mask != pLib->current_audio_tap_mask) {
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\AudioCh# Enable",
                            &pLib->audio_tap_mask,
                            0x87, /* MI_BITFLD */
                            sizeof(pLib->audio_tap_mask))) {
      return (-1);
    }
    pLib->current_audio_tap_mask = pLib->audio_tap_mask;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->bchannel_trace_mask != pLib->current_bchannel_trace_mask) {
    if (SuperTraceWriteVar (pLib->hAdapter,
                            pLib->buffer,
                            "Trace\\B-Ch# Enable",
                            &pLib->bchannel_trace_mask,
                            0x87, /* MI_BITFLD */
                            sizeof(pLib->bchannel_trace_mask))) {
      return (-1);
    }
    pLib->current_bchannel_trace_mask = pLib->bchannel_trace_mask;
    pLib->req_busy = 1;
    return (0);
  }

  if (!pLib->trace_events_down) {
    if (SuperTraceTraceOnRequest (pLib->hAdapter,
                                  "Events Down",
                                  pLib->buffer)) {
      return (-1);
    }
    pLib->trace_events_down = 1;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
    if (!pLib->l1_trace) {
      if (SuperTraceTraceOnRequest (pLib->hAdapter,
                                    "State\\Layer1",
                                    pLib->buffer)) {
        return (-1);
      }
      pLib->l1_trace = 1;
      pLib->req_busy = 1;
      return (0);
    }

    if (!pLib->l2_trace) {
      if (SuperTraceTraceOnRequest (pLib->hAdapter,
                                    "State\\Layer2 No1",
                                    pLib->buffer)) {
        return (-1);
      }
      pLib->l2_trace = 1;
      pLib->req_busy = 1;
      return (0);
    }

    /* set alarm events */
    if (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_PRI) {
      if ((pLib->alarms_trace & 3) == 0) {
        if (SuperTraceTraceOnRequest (pLib->hAdapter, "State\\Red Alarm", pLib->buffer)) {
          return (-1);
        }
        pLib->alarms_trace |= 1L ;
        pLib->req_busy = 1;
        return (0);
      }
      if ((pLib->alarms_trace & 12) == 0) {
        if (SuperTraceTraceOnRequest (pLib->hAdapter, "State\\Yellow Alarm", pLib->buffer)) {
          return (-1);
        }
        pLib->alarms_trace |= 4L ;
        pLib->req_busy = 1;
        return (0);
      }
      if ((pLib->alarms_trace & 48) == 0) {
        if (SuperTraceTraceOnRequest (pLib->hAdapter, "State\\Blue Alarm", pLib->buffer)) {
          return (-1);
        }
        pLib->alarms_trace |= 16L ;
        pLib->req_busy = 1;
        return (0);
      }
    }

  } else { /* analog adapter */
    if (pLib->l1_trace < pLib->Channels) {
      sprintf (name, "State\\Line-%d\\LineState", pLib->l1_trace + 1);
      if (SuperTraceTraceOnRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->l1_trace++;
  		pLib->req_busy = 1;
      return (0);
    }
  }

	if (!pLib->temperature_event) {
		pLib->temperature_event = 1;
		if (SuperTraceTraceOnRequest (pLib->hAdapter,
																	"Info\\Temperature",
																	pLib->buffer)) {
			return (-1);
		}
		pLib->req_busy = 1;
		return (0);
	}

	if (pLib->temperature_read_state == 1) {
		if (SuperTraceReadRequest (pLib->hAdapter, "Info", pLib->buffer)) {
			return (-1);
		}
		pLib->temperature_read_state = 2;
		pLib->req_busy = 1;
		return (0);
	}

  for (i = 0; i < 31; i++) {
    if (pLib->pending_line_status & (1L << i)) {
      sprintf (name, "State\\%s%d", pLib->line_id, i+1);
      if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->pending_line_status &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->pending_modem_status & (1L << i)) {
      sprintf (name, "State\\%s%d\\Modem", pLib->line_id, i+1);
      if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->pending_modem_status &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->pending_fax_status & (1L << i)) {
      sprintf (name, "State\\%s%d\\FAX", pLib->line_id, i+1);
      if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->pending_fax_status &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->clear_call_command & (1L << i)) {
      sprintf (name, "State\\%s%d\\Clear Call", pLib->line_id, i+1);
      if (SuperTraceExecuteRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->clear_call_command &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->channel_b1_stat & (1L << i)) {
      sprintf (name, "State\\%s%d\\L1 Stats", pLib->line_id, i+1);
      if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->channel_b1_stat &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->channel_b2_stat & (1L << i)) {
      sprintf (name, "State\\%s%d\\L2 Stats", pLib->line_id, i+1);
      if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
        return (-1);
      }
      pLib->channel_b2_stat &= ~(1L << i);
      pLib->req_busy = 1;
      return (0);
    }
  }

  if (pLib->outgoing_ifc_stats) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {

      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\Outgoing Calls",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->outgoing_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    } else {
    	/* Analog Adapter collects call stats per line */
		  for (i = 0; i < pLib->Channels; i++) {
        if (pLib->outgoing_ifc_stats & (1L << i)) {
          sprintf (name, "State\\Line-%d\\L3 Stats\\Outgoing Calls", i+1);
          if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
            return (-1);
          }
          pLib->outgoing_ifc_stats &= ~(1L << i);
          pLib->req_busy = 1;
          return (0);
        }
      }
    }
  }

  if (pLib->incoming_ifc_stats) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
	    if (SuperTraceReadRequest (pLib->hAdapter,
  	                             "Statistics\\Incoming Calls",
    	                           pLib->buffer)) {
      	return (-1);
      }
      pLib->incoming_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    } else {
    	/* Analog Adapter collects call stats per line */
		  for (i = 0; i < pLib->Channels; i++) {
        if (pLib->incoming_ifc_stats & (1L << i)) {
          sprintf (name, "State\\Line-%d\\L3 Stats\\Incoming Calls", i+1);
          if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
            return (-1);
          }
          pLib->incoming_ifc_stats &= ~(1L << i);
          pLib->req_busy = 1;
          return (0);
        }
      }
    }
  }

  if (pLib->l1_ifc_stats) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {

      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\Layer1",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->l1_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    } else {
      /* Analog Adapter - nothing to read */
      pLib->l1_ifc_stats = 0;
      pLib->req_busy = 0;
      return (0);
    }
  }

  
  if (pLib->modem_ifc_stats) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {

      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\Modem",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->modem_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    } else {
      	/* Analog Adapter collects call stats per line */
	  	  for (i = 0; i < pLib->Channels; i++) {
          if (pLib->modem_ifc_stats & (1L << i)) {
            sprintf (name, "Statistics\\Line-%d\\Modem", i+1);
            if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
              return (-1);
            }
            pLib->modem_ifc_stats &= ~(1L << i);
            pLib->req_busy = 1;
            return (0);
          }
        }
    }
    
    
    
  }

  if (pLib->fax_ifc_stats) {
    if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\FAX",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->fax_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    } else {
      	/* Analog Adapter collects call stats per line */
        for (i = 0; i < pLib->Channels; i++) {
          if (pLib->fax_ifc_stats & (1L << i)) {
            sprintf (name, "Statistics\\Line-%d\\FAX", i+1);
            if (SuperTraceReadRequest (pLib->hAdapter, name, pLib->buffer)) {
              return (-1);
            }
            pLib->fax_ifc_stats &= ~(1L << i);
            pLib->req_busy = 1;
            return (0);
          }
        }
    }
  }

  if (pLib->b1_ifc_stats) {
    if (SuperTraceReadRequest (pLib->hAdapter,
                               "Statistics\\B-Layer1",
                               pLib->buffer)) {
      return (-1);
    }
    pLib->b1_ifc_stats = 0;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->b2_ifc_stats) {
    if (SuperTraceReadRequest (pLib->hAdapter,
                               "Statistics\\B-Layer2",
                               pLib->buffer)) {
      return (-1);
    }
    pLib->b2_ifc_stats = 0;
    pLib->req_busy = 1;
    return (0);
  }

  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
    if (pLib->d1_ifc_stats) {
      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\D-Layer1",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->d1_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->d2_ifc_stats) {
      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\D-Layer2",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->d2_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    }
    if (pLib->l1_ifc_stats) {
      if (SuperTraceReadRequest (pLib->hAdapter,
                                 "Statistics\\Layer1",
                                 pLib->buffer)) {
        return (-1);
      }
      pLib->l1_ifc_stats = 0;
      pLib->req_busy = 1;
      return (0);
    }
	}

	if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
    if (!pLib->IncomingCallsCallsActive) {
      pLib->IncomingCallsCallsActive = 1;
      sprintf (name, "%s", "Statistics\\Incoming Calls\\Calls");
      if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
        pLib->IncomingCallsCallsActive = 0;
        return (-1);
      }
      pLib->req_busy = 1;
      return (0);
    }
    if (!pLib->IncomingCallsConnectedActive) {
      pLib->IncomingCallsConnectedActive = 1;
      sprintf (name, "%s", "Statistics\\Incoming Calls\\Connected");
      if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
        pLib->IncomingCallsConnectedActive = 0;
        return (-1);
      }
      pLib->req_busy = 1;
      return (0);
    }
  	if (!pLib->OutgoingCallsCallsActive) {
	    pLib->OutgoingCallsCallsActive = 1;
	    sprintf (name, "%s", "Statistics\\Outgoing Calls\\Calls");
	    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
  	 	  pLib->OutgoingCallsCallsActive = 0;
    	 	return (-1);
			}
  	  pLib->req_busy = 1;
    	  return (0);
	  }
	  if (!pLib->OutgoingCallsConnectedActive) {
  	  pLib->OutgoingCallsConnectedActive = 1;
	    sprintf (name, "%s", "Statistics\\Outgoing Calls\\Connected");
	 	  if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
  	    pLib->OutgoingCallsConnectedActive = 0;
    	  return (-1);
	    }
  	  pLib->req_busy = 1;
	    return (0);
	  }
  } else { /* pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_ANALOG */
		for (i = 0; i < pLib->Channels; i++) {
			if (~(pLib->IncomingCallsCallsActive) & (1L << i)) {
				sprintf (name, "State\\Line-%d\\L3 Stats\\Incoming Calls\\Calls", i+1);
        if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
          return (-1);
        }
				pLib->IncomingCallsCallsActive |= (1L << i);
        pLib->req_busy = 1;
        return (0);
			}
			if (~pLib->IncomingCallsConnectedActive & (1L << i)) {
				sprintf (name, "State\\Line-%d\\L3 Stats\\Incoming Calls\\Connected", i+1);
        if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
          return (-1);
        }
				pLib->IncomingCallsConnectedActive |= (1L << i);
        pLib->req_busy = 1;
        return (0);
			}
			if (~pLib->OutgoingCallsCallsActive & (1L << i)) {
				sprintf (name, "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Calls", i+1);
        if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
          return (-1);
        }
				pLib->OutgoingCallsCallsActive |= (1L << i);
        pLib->req_busy = 1;
        return (0);
			}
			if (~pLib->OutgoingCallsConnectedActive & (1L << i)) {
				sprintf (name, "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Connected", i+1);
        if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
          return (-1);
        }
				pLib->OutgoingCallsConnectedActive |= (1L << i);
        pLib->req_busy = 1;
        return (0);
			}
  	}
  }
	if (pLib->identify_start == 1) {
		pLib->identify_start = 2;
		sprintf (name, "%s", "Info\\IdentifyStart");
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
			pLib->identify_start = 0;
			return (-1);
		}
    pLib->req_busy = 1;
		return (0);
	}
	if (pLib->identify_stop == 1) {
		pLib->identify_stop = 2;
		sprintf (name, "%s", "Info\\IdentifyStop");
    if ((ret = SuperTraceTraceOnRequest(pLib->hAdapter, name, pLib->buffer))) {
			pLib->identify_stop = 0;
			return (-1);
		}
    pLib->req_busy = 1;
		return (0);
	}

	if (pLib->resource_info_state >= 0) {
		if (pLib->resource_info_state == 0 || pLib->resource_info_req != pLib->resource_info_ack) {
			if (SuperTraceReadRequest (pLib->hAdapter, DIVA_RESOURCE_PATH, pLib->buffer)) {
				pLib->resource_info_state = -1;
				return (-1);
			}
			if (pLib->resource_info_state == 0) {
				pLib->resource_info_state = 1;
			}
			pLib->resource_info_ack = pLib->resource_info_req;
			pLib->req_busy = 1;
			return (0);
		}
	}

	if (pLib->time_information_update_state == 0 && pLib->time_update_state == 1) {
		dword tz_offset;
		int   dst_state;
		int   sync_state;

		if (diva_os_get_time_info (0, 0, &tz_offset, &dst_state, &sync_state) == 0) {
			if (pLib->tz_offset  != tz_offset ||
					pLib->dst_state  != dst_state ||
					pLib->sync_state != sync_state) {
				byte data[8];

				data[0] = (byte)((dst_state != 0) | ((sync_state != 0) << 1));
				data[1] = 0;
				data[2] = 0;
				data[3] = 0;
				data[4] = (byte)tz_offset;
				data[5] = (byte)(tz_offset >> 8);
				data[6] = (byte)(tz_offset >> 16);
				data[7] = (byte)(tz_offset >> 24);

				if (SuperTraceWriteVar (pLib->hAdapter,
															pLib->buffer,
															"Info\\Time\\Information",
															data,
															0x84, /* MI_HSTR */
															sizeof(data))) {
					return (-1);
				} else {
					pLib->tz_offset  = tz_offset;
					pLib->dst_state  = dst_state;
					pLib->sync_state = sync_state;
					pLib->time_information_update_state = 1;
					pLib->req_busy = 1;
					return (0);
				}
			}
		} else {
			pLib->time_information_update_state = -1;
			pLib->time_update_state             = -1;
		}
	}

	if (pLib->time_update_state == 1) {
		dword t_sec, t_sec_fract;

		if (diva_os_get_time_info (&t_sec, &t_sec_fract, 0, 0, 0) == 0) {
			byte data[8];

			data[0] = (byte)t_sec_fract;
			data[1] = (byte)(t_sec_fract >> 8);
			data[2] = (byte)(t_sec_fract >> 16);
			data[3] = (byte)(t_sec_fract >> 24);
			data[4] = (byte)t_sec;
			data[5] = (byte)(t_sec >> 8);
			data[6] = (byte)(t_sec >> 16);
			data[7] = (byte)(t_sec >> 24);

			if (SuperTraceWriteVar (pLib->hAdapter,
															pLib->buffer,
															"Info\\Time\\Seconds",
															data,
															0x84, /* MI_HSTR */
															sizeof(data))) {
				pLib->time_update_state = 0;
				return (-1);
			} else {
				pLib->time_update_state = 2;
				pLib->req_busy = 1;
				return (0);
			}
		} else {
			pLib->time_update_state = -1;
		}
	}

	return (0);
}

static int process_idi_event (diva_strace_context_t* pLib,
															diva_man_var_header_t* pVar) {
	const char* path = (char*)&pVar->path_length+1;
	char name[64];
	int i, ret;

//  printf("process idi event for adapter %d: path \"%s\" ifc_type: %d\n",pLib->Adapter, path, pLib->InterfaceConfig.type);
//  DBG_PRINT((logfile,"process idi event for adapter %d: path \"%s\" ifc_type: %d\n",pLib->Adapter, path, pLib->InterfaceConfig.type));

	if (!strncmp(((pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) ? "State\\B Event" : "State\\Line Event"),
	               path, pVar->path_length)) {
    dword ch_id;
    if (!diva_trace_read_variable (pVar, &ch_id)) {
      if (!pLib->line_init_event && !pLib->pending_line_status) {
        for (i = 1; i <= pLib->Channels; i++) {
          diva_line_event(pLib, i);
        }
        return (0);
      } else if (ch_id && ch_id <= (dword)pLib->Channels) {
        /*
          Check if event is new combi event with directory
          context inside of the event
          */
        sprintf (name, "State\\%s%d\\Framing", pLib->line_id, ch_id);
        if (find_var (pVar, name)) {
          /*
            Process new style combi event
            */
          ret = process_idi_info  (pLib, get_next_var (pVar));
          DBG_PRINT((logfile,"process idi event: new style process_idi_info returned %d\n",ret));
        } else {
          /*
            Process regular event
            */
          ret = diva_line_event(pLib, (int)ch_id);
          DBG_PRINT((logfile,"process idi event: diva_line_event returned %d\n",ret));
        }
        return (ret);
      }
      return (0);
    }
    DBG_PRINT((logfile,"process idi event: State\\B/Line- Event returns error\n"));
    return (-1);
  }

	if (!strncmp("State\\FAX Event", path, pVar->path_length)) {
    dword ch_id;
    if (!diva_trace_read_variable (pVar, &ch_id)) {
      if (!pLib->pending_fax_status && !pLib->fax_init_event) {
        for (i = 1; i <= pLib->Channels; i++) {
          diva_fax_event(pLib, i);
        }
        return (0);
      } else if (ch_id && ch_id <= (dword)pLib->Channels) {
        sprintf (name, "State\\%s%d\\FAX\\Features", pLib->line_id, ch_id);
        if (find_var (pVar, name)) {
          return (process_idi_info  (pLib, get_next_var (pVar)));
        } else {
          return (diva_fax_event(pLib, (int)ch_id));
        }
      }
      return (0);
    }
    return (-1);
  }

	if (!strncmp("State\\Modem Event", path, pVar->path_length)) {
    dword ch_id;
    if (!diva_trace_read_variable (pVar, &ch_id)) {
      if (!pLib->pending_modem_status && !pLib->modem_init_event) {
        for (i = 1; i <= pLib->Channels; i++) {
          diva_modem_event(pLib, i);
        }
        return (0);
      } else if (ch_id && ch_id <= (dword)pLib->Channels) {
        sprintf (name, "State\\%s%d\\Modem\\Norm", pLib->line_id, ch_id);
        if (find_var (pVar, name)) {
          return (process_idi_info  (pLib, get_next_var (pVar)));
        } else {
          return (diva_modem_event(pLib, (int)ch_id));
        }
      }
      return (0);
    }
    return (-1);
  }

	/*
		First look for Line Event
		*/
	for (i = 1; i <= pLib->Channels; i++) {
		sprintf (name, "State\\%s%d\\Line", pLib->line_id, i);
		if (find_var (pVar, name)) {
			return (diva_line_event(pLib, i));
		}
	}

	/*
		Look for Moden Progress Event
		*/
	for (i = 1; i <= pLib->Channels; i++) {
		sprintf (name, "State\\%s%d\\Modem\\Event", pLib->line_id, i);
		if (find_var (pVar, name)) {
			return (diva_modem_event (pLib, i));
		}
	}

	/*
		Look for Fax Event
		*/
	for (i = 1; i <= pLib->Channels; i++) {
		sprintf (name, "State\\%s%d\\FAX\\Event", pLib->line_id, i);
		if (find_var (pVar, name)) {
			return (diva_fax_event (pLib, i));
		}
	}

	/*
		Notification about loss of events
		*/
	if (!strncmp("Events Down", path, pVar->path_length)) {
		if (pLib->trace_events_down == 1) {
			pLib->trace_events_down = 2;
		} else {
      DBG_PRINT((logfile,"process idi event: ERROR - Events Down\n"));
			diva_trace_error (pLib, 1, "Events Down", 0);
		}
		return (0);
	}

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

	if (strncmp("Info\\Temperature", path, pVar->path_length) == 0) {
		if (pLib->temperature_read_state == 0) {
			pLib->temperature_read_state = 1;
		}
		return (0);
	}

  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) {
    /* BRI, PRI and SoftIP adapters */
    if (!strncmp("State\\Layer1", path, pVar->path_length)) {
			diva_strace_read_asz  (pVar, &pLib->lines[0].pInterface->Layer1[0]);
			if (pLib->l1_trace == 1) {
				pLib->l1_trace = 2;
			} else {
				diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
			}
			return (0);
		}
    if (!strncmp("State\\Layer2 No1", path, pVar->path_length)) {
      char* tmp = &pLib->lines[0].pInterface->Layer2[0];
      dword l2_state=0;
      diva_strace_read_uint (pVar, &l2_state);

      switch (l2_state) {
        case 0:
          strcpy (tmp, "Idle");
          break;
        case 1:
          strcpy (tmp, "Layer2 UP");
          break;
        case 2:
          strcpy (tmp, "Layer2 Disconnecting");
          break;
        case 3:
          strcpy (tmp, "Layer2 Connecting");
          break;
        case 4:
          strcpy (tmp, "SPID Initializing");
          break;
        case 5:
          strcpy (tmp, "SPID Initialised");
          break;
        case 6:
          strcpy (tmp, "Layer2 Connecting");
          break;

        case  7:
          strcpy (tmp, "Auto SPID Stopped");
          break;

        case  8:
          strcpy (tmp, "Auto SPID Idle");
          break;

        case  9:
          strcpy (tmp, "Auto SPID Requested");
          break;

        case  10:
          strcpy (tmp, "Auto SPID Delivery");
          break;

        case 11:
          strcpy (tmp, "Auto SPID Complete");
          break;

        default:
          sprintf (tmp, "U:%d", (int)l2_state);
      }
      if (pLib->l2_trace == 1) {
        pLib->l2_trace = 2;
      } else {
        diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
      }
      return (0);
    }
    if (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_PRI) {
    	if (!strncmp("State\\Red Alarm", path, pVar->path_length)) {
        diva_strace_read_uint (pVar, &pLib->lines[0].pInterface->pConfig->alarm_red);
        if ((pLib->alarms_trace & 3) == 1) {
          pLib->alarms_trace |= 2;
      	} else {
        	diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
      	}
      	return (0);
    	}
    	if (!strncmp("State\\Yellow Alarm", path, pVar->path_length)) {
        diva_strace_read_uint (pVar, &pLib->lines[0].pInterface->pConfig->alarm_yellow);
        if ((pLib->alarms_trace & 12) == 4) {
          pLib->alarms_trace |=8;
      	} else {
        	diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
      	}
      	return (0);
    	}
    	if (!strncmp("State\\Blue Alarm", path, pVar->path_length)) {
        diva_strace_read_uint (pVar, &pLib->lines[0].pInterface->pConfig->alarm_blue);
        if ((pLib->alarms_trace & 48) == 16) {
          pLib->alarms_trace |= 32;
      	} else {
        	diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
      	}
      	return (0);
    	}
    }


  } else {

    /* Analog adapter: Layer 1 is found in State\\Line-x\LineState */
    for (i = 1; i <= pLib->Channels; i++) {
      sprintf (name, "State\\Line-%d\\LineState", i);
			if (!strncmp(name, path, pVar->path_length)) {
        pLib->lines[i-1].pInterface->Layer1[0] = 'L';
        pLib->lines[i-1].pInterface->Layer1[1] = 'i';
        pLib->lines[i-1].pInterface->Layer1[2] = 'n';
        pLib->lines[i-1].pInterface->Layer1[3] = 'e';
        pLib->lines[i-1].pInterface->Layer1[4] = ' ';
        pLib->lines[i-1].pInterface->Layer1[5] = '-';
        pLib->lines[i-1].pInterface->Layer1[6] = ' ';
        pLib->lines[i-1].pInterface->Layer1[7] = (char)(i+'0');
        pLib->lines[i-1].pInterface->Layer1[8] = ' ';
        pLib->lines[i-1].pInterface->Layer1[9] =  0;
				diva_strace_read_asz (pVar, &pLib->lines[i-1].pInterface->Layer1[9]);
				diva_trace_notify_user (pLib, i-1, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
				return (0);
			}
      sprintf (name, "State\\Line-%d\\L3 Stats\\Incoming Calls\\Calls", i);
			if (!strncmp(name, path, pVar->path_length)) {
				diva_strace_read_uint (pVar, &pLib->lines[i-1].pInterfaceStat->inc.Calls);
				diva_trace_notify_user (pLib, i-1, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
				return (0);
			}
      sprintf (name, "State\\Line-%d\\L3 Stats\\Incoming Calls\\Connected", i);
			if (!strncmp(name, path, pVar->path_length)) {
				diva_strace_read_uint (pVar, &pLib->lines[i-1].pInterfaceStat->inc.Connected);
				diva_trace_notify_user (pLib, i-1, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
				return (0);
			}
      sprintf (name, "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Calls", i);
			if (!strncmp(name, path, pVar->path_length)) {
				diva_strace_read_uint (pVar, &pLib->lines[i-1].pInterfaceStat->outg.Calls);
				diva_trace_notify_user (pLib, i-1, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
				return (0);
			}
      sprintf (name, "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Connected", i);
			if (!strncmp(name, path, pVar->path_length)) {
				diva_strace_read_uint (pVar, &pLib->lines[i-1].pInterfaceStat->outg.Connected);
				diva_trace_notify_user (pLib, i-1, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
				return (0);
			}
    }
  }

	if (!strncmp("Statistics\\Incoming Calls\\Calls", path, pVar->path_length) ||
			!strncmp("Statistics\\Incoming Calls\\Connected", path, pVar->path_length)) {
		return (SuperTraceGetIncomingCallStatistics (pLib));
	}

	if (!strncmp("Statistics\\Outgoing Calls\\Calls", path, pVar->path_length) ||
			!strncmp("Statistics\\Outgoing Calls\\Connected", path, pVar->path_length)) {
		return (SuperTraceGetOutgoingCallStatistics (pLib));
	}

	if (!strncmp("Statistics\\Layer1\\Framing Errors", path, pVar->path_length) ||
			!strncmp("Statistics\\Layer1\\CRC4 Errors", path, pVar->path_length) ||
			!strncmp("Statistics\\Layer1\\Frame Slips", path, pVar->path_length)) {
		return (SuperTraceGetLayer1Statistics (pLib));
	}

  DBG_PRINT((logfile,"process idi event: Function returns error\n"));
	return (-1);
}

static int diva_line_event (diva_strace_context_t* pLib, int Channel) {
	pLib->pending_line_status |= (1L << (Channel-1));
	return (0);
}

static int diva_modem_event (diva_strace_context_t* pLib, int Channel) {
	pLib->pending_modem_status |= (1L << (Channel-1));
	return (0);
}

static int diva_fax_event (diva_strace_context_t* pLib, int Channel) {
	pLib->pending_fax_status |= (1L << (Channel-1));
	return (0);
}

/*
	Process INFO indications that arrive from the card
	Uses path of first I.E. to detect the source of the
	indication
	*/
static int process_idi_info  (diva_strace_context_t* pLib,
															diva_man_var_header_t* pVar) {
	const char* path;
	char name[64];
	int i, len;

	if (pVar == 0) {
		return (-1);
	}

	path = (char*)&pVar->path_length+1;

//  printf("process idi info: path \"%s\" Adapter: %d \n",path, pLib->Adapter);
#ifdef SUPER_TRACE_DEBUG
//  DBG_PRINT((logfile,"process idi info: path \"%s\" \n",path));
#endif

	/*
		Initialize adapter-type before anything else happens
	*/
	if (pLib->general_cardtype_event == 2) {
		if (!strncmp(path, "Config\\CardType", sizeof("Config\\CardType")-1)) {
			dword ct;
			if (diva_trace_read_variable (pVar, &ct))  {
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1); /* error */
			}
			diva_init_cardtype(pLib, ct);
			if(pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_HM) {
				pLib->general_cardtype_event=3;
				return(0);
			}
			diva_create_parse_table (pLib);
			pLib->general_cardtype_event=3;
			return(0);
		}
	}
	
	if(pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_HM) {
		return(0);
	}
	/*
		First look for Modem Status Info
		*/
	for (i = pLib->Channels; i > 0; i--) {
		len = sprintf (name, "State\\%s%d\\Modem", pLib->line_id, i);
		if (!strncmp(name, path, len)) {
			return (diva_modem_info (pLib, i, pVar));
		}
	}

	/*
		Look for Fax Status Info
		*/
	for (i = pLib->Channels; i > 0; i--) {
		len = sprintf (name, "State\\%s%d\\FAX", pLib->line_id, i);
		if (!strncmp(name, path, len)) {
			return (diva_fax_info (pLib, i, pVar));
		}
	}

	/*
		Look for channel layer 1 statistics
		*/
	for (i = pLib->Channels; i > 0; i--) {
		len = sprintf (name, "State\\%s%d\\L1 Stats", pLib->line_id, i);
		if (!strncmp(name, path, len)) {
			return (diva_channel_b1_stat_info (pLib, i, pVar));
		}
	}

	/*
		Look for channel layer 2 statistics
		*/
	for (i = pLib->Channels; i > 0; i--) {
		len = sprintf (name, "State\\%s%d\\L2 Stats", pLib->line_id, i);
		if (!strncmp(name, path, len)) {
			return (diva_channel_b2_stat_info (pLib, i, pVar));
		}
	}

	if (!diva_ifc_statistics (pLib, pVar)) {
		return (0);
	}

	/*
		Look for Line Status Info
		*/
	for (i = pLib->Channels; i > 0; i--) {
		len = sprintf (name, "State\\%s%d", pLib->line_id, i);
		if (!strncmp(name, path, len)) {
			return (diva_line_info (pLib, i, pVar));
		}
	}

  i = diva_ifc_config (pLib, pVar);
	if (i==-1) {
		return (-1); /* error occured */
	} else if (i) {
	  return (0);  /* found variable */
	}              /* else fall through */

  i = diva_ifc_info (pLib, pVar);
	if (i==-1) {
		return (-1); /* error occured */
	} else if (i) {
	  diva_temperature(pLib, pVar);
	  return (0);  /* found variable */
	}              /* else fall through */


	return (-1);
}

static int diva_temperature (diva_strace_context_t* pLib,
                             diva_man_var_header_t* pVar) {
  diva_man_var_header_t* cur;
  int found = 0;

  if ((cur = find_var (pVar, "Info\\InitialTemperature")) != 0) {
    diva_strace_read_uint (cur, &pLib->lines[0].pInterface->InitialTemperature);
    found = 1;
  }
  if ((cur = find_var (pVar, "Info\\MaxTemperature")) != 0) {
    diva_strace_read_uint (cur, &pLib->lines[0].pInterface->MaxTemperature);
    found = 1;
  }
  if ((cur = find_var (pVar, "Info\\MinTemperature")) != 0) {
    diva_strace_read_uint (cur, &pLib->lines[0].pInterface->MinTemperature);
    found = 1;
  }
  if ((cur = find_var (pVar, "Info\\Temperature")) != 0) {
    diva_strace_read_uint (cur, &pLib->lines[0].pInterface->Temperature);
    found = 1;
  }

  if (found != 0) {
		if (pLib->initial_cfg_complete_event != 0) {
			diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_TEMPERATURE_CHANGE);
		}
    pLib->temperature_read_state = 0;
    return (0);
  }

  return (-1);
}

/*
	MODEM INSTANCE STATE UPDATE

	Update Modem Status Information and issue notification to user,
	that will inform about change in the state of modem instance, that is
	associuated with this channel
	*/
static int diva_modem_info (diva_strace_context_t* pLib,
														int Channel,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, nr = Channel - 1;

	for (i  = pLib->modem_parse_entry_first[nr];
			 i <= pLib->modem_parse_entry_last[nr]; i++) {

		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_init: ERROR -3 in diva_modem_info adapter %d\n", pLib->Adapter));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
  		DBG_PRINT((logfile,"lib_init: ERROR -2 in diva_modem_info adapter %d\n", pLib->Adapter));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}

	/*
		We do not use first event to notify user - this is the event that is
		generated as result of EVENT ON operation and is used only to initialize
		internal variables of application
		*/
	if (pLib->modem_init_event & (1L << nr)) {
		diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE);
	} else {
		pLib->modem_init_event |= (1L << nr);
	}

	return (0);
}

static int diva_channel_b1_stat_info (diva_strace_context_t* pLib,
																			int Channel,
																			diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, nr = Channel - 1;

	for (i  = pLib->channel_b1_parse_entry_first[nr];
			 i <= pLib->channel_b1_parse_entry_last[nr]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_init: ERROR -3 in diva_channel_b1_stat_info adapter %d\n", pLib->Adapter));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
  		DBG_PRINT((logfile,"lib_init: ERROR -2 in diva_channel_b1_stat_info adapter %d\n", pLib->Adapter));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}

	diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE);

	return (0);
}

static int diva_channel_b2_stat_info (diva_strace_context_t* pLib,
																			int Channel,
																			diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, nr = Channel - 1;

	for (i  = pLib->channel_b2_parse_entry_first[nr];
			 i <= pLib->channel_b2_parse_entry_last[nr]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}

	diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE);

	return (0);
}

static int diva_fax_info (diva_strace_context_t* pLib,
													int Channel,
													diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, nr = Channel - 1;

	for (i  = pLib->fax_parse_entry_first[nr];
			 i <= pLib->fax_parse_entry_last[nr]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
  		  DBG_PRINT((logfile,"lib_init: ERROR -3 in diva_fax_info adapter %d\n", pLib->Adapter));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
 		  DBG_PRINT((logfile,"lib_init: ERROR -2 in diva_fax_info adapter %d\n", pLib->Adapter));
			diva_trace_error (pLib, -2 , __FILE__, __LINE__);
			return (-1);
		}
	}

	/*
		We do not use first event to notify user - this is the event that is
		generated as result of EVENT ON operation and is used only to initialize
		internal variables of application
		*/
	if (pLib->fax_init_event & (1L << nr)) {
		diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE);
	} else {
		pLib->fax_init_event |= (1L << nr);
	}

	return (0);
}

/*
	LINE STATE UPDATE
	Update Line Status Information and issue notification to user,
	that will inform about change in the line state.
	*/
static int diva_line_info  (diva_strace_context_t* pLib,
														int Channel,
														diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, nr = Channel - 1;
	int allowedMissing = LINE_PARSE_ENTRIES_MAX_MISS;
	
	for (i  = pLib->line_parse_entry_first[nr];
			 i <= pLib->line_parse_entry_last[nr]; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
				DBG_PRINT((logfile,"lib_init: ERROR -3 in diva_line_info adapter %d\n", pLib->Adapter));
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
		} else {
			if(allowedMissing == 0) {
				DBG_PRINT((logfile,"lib_init: ERROR -2 in diva_line_info adapter %d\n", pLib->Adapter));
				diva_trace_error (pLib, -2 , __FILE__, __LINE__);
				return (-1);
			}
			else {
				allowedMissing--;
			}
		}
	}

	/*
		We do not use first event to notify user - this is the event that is
		generated as result of EVENT ON operation and is used only to initialize
		internal variables of application

		Exception is is if the line is "online". In this case we have to notify
		user about this condition.
		*/
	if (pLib->line_init_event & (1L << nr)) {
		diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE);
	} else {
		pLib->line_init_event |= (1L << nr);
		if (strcmp (&pLib->lines[nr].Line[0], "Idle")) {
			diva_trace_notify_user (pLib, nr, DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE);
		}
	}

	return (0);
}

/*
	Move position to next vatianle in the chain
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

/*
	Locate variable using path prefix
	*/
static int locate_var (const diva_man_var_header_t* pVar, const char* name, int name_length) {

	if (pVar != 0 && pVar->path_length > (byte)name_length) {
		return (memcmp (&pVar->path_length+1, name, name_length));
	}

	return (-1);
}

static void diva_create_line_parse_table  (diva_strace_context_t* pLib,
																					 int Channel) {
	diva_trace_line_state_t* pLine = &pLib->lines[Channel];
	int nr = Channel+1;

	if ((pLib->cur_parse_entry + LINE_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}

	pLine->ChannelNumber = nr;

	pLib->line_parse_entry_first[Channel] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Framing", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->Framing[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Line", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->Line[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Layer2", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->Layer2[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Layer3", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->Layer3[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Remote Address", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																								&pLine->RemoteAddress[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Charges", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->Charges;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Last Disc Cause", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																										&pLine->LastDisconnectCause;
	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\AbandonedCallin", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																										&pLine->AbandonedCallin;
	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\AbandonedCallout", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																										&pLine->AbandonedCallout;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\User ID", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->UserID[0];

  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {
    /* these variables are only valid for BRI, PRI and SoftIP adapters */

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\Remote SubAddr", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                                  &pLine->RemoteSubAddress[0];

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\Local Address", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                                  &pLine->LocalAddress[0];

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\Local SubAddr", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                                  &pLine->LocalSubAddress[0];

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\BC", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->call_BC;

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\HLC", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->call_HLC;

    sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
             "State\\B%d\\LLC", nr);
    pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->call_LLC;

		sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
						 "State\\%s%d\\Call Reference", pLib->line_id, nr);
		pLib->parse_table[pLib->cur_parse_entry++].variable = &pLine->CallReference;

	}

	pLib->line_parse_entry_last[Channel] = pLib->cur_parse_entry - 1;
}

static void diva_create_fax_parse_table (diva_strace_context_t* pLib,
																				 int Channel) {
	diva_trace_fax_state_t* pFax = &pLib->lines[Channel].fax;
	int nr = Channel+1;

	if ((pLib->cur_parse_entry + FAX_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}
	pFax->ChannelNumber = nr;

	pLib->fax_parse_entry_first[Channel] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Event", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Event;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Page Counter", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Page_Counter;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Features", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Features;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Station ID", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Station_ID[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Subaddress", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Subaddress[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Password", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Password[0];

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Speed", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Speed;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Resolution", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Resolution;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Paper Width", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Paper_Width;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Paper Length", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Paper_Length;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Scanline Time", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Scanline_Time;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\FAX\\Disc Reason", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pFax->Disc_Reason;

	pLib->fax_parse_entry_last[Channel] = pLib->cur_parse_entry - 1;
}

static void diva_create_modem_parse_table (diva_strace_context_t* pLib,
																					 int Channel) {
	diva_trace_modem_state_t* pModem = &pLib->lines[Channel].modem;
	int nr = Channel+1;

	if ((pLib->cur_parse_entry + MODEM_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}
	pModem->ChannelNumber = nr;

	pLib->modem_parse_entry_first[Channel] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Event", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->Event;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Norm", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->Norm;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Options", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->Options;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\TX Speed", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->TxSpeed;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\RX Speed", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->RxSpeed;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Roundtrip ms", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->RoundtripMsec;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Symbol Rate", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->SymbolRate;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\RX Level dBm", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->RxLeveldBm;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Echo Level dBm", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->EchoLeveldBm;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\SNR dB", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->SNRdb;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\MAE", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->MAE;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Local Retrains", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->LocalRetrains;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Remote Retrains", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->RemoteRetrains;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Local Resyncs", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->LocalResyncs;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Remote Resyncs", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->RemoteResyncs;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\Modem\\Disc Reason", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pModem->DiscReason;

	pLib->modem_parse_entry_last[Channel] = pLib->cur_parse_entry - 1;
}

static void diva_create_parse_table (diva_strace_context_t* pLib) {
	int i;

// printf("Create parse table for Adapter %d (channels = %d); start = %d\n",pLib->Adapter, pLib->Channels, pLib->cur_parse_entry);
	for (i = 0; i < pLib->Channels; i++) {
		diva_create_line_parse_table  (pLib, i);
		diva_create_modem_parse_table (pLib, i);
		diva_create_fax_parse_table   (pLib, i);
		diva_create_channel_b1_stat_parse_table (pLib, i);
		diva_create_channel_b2_stat_parse_table (pLib, i);
	}

	if ((pLib->cur_parse_entry + STAT_PARSE_ENTRIES - 1) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}

	/*
		General Config
		*/

	pLib->config_parse_first  = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\DChannel\\Protocol");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.protocol;

  /* fallback for protocol*/
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\Protocol");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.protocol;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\NT");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.NTmode;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\PRI");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.type;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\SPID-1\\TEI");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.TEI;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\Layer2\\L2");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.StableL2;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\CardType");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.cardtype;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Config\\BChannelCount");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.channels ;

	pLib->config_parse_last  = pLib->cur_parse_entry - 1;
	/*
	  End Config
	*/

	/*
		General Info
		*/

	pLib->info_parse_first  = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\ProtocolBuild");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.ProtocolBuild[0];

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\DSPCodeBuild");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.DSPCodeBuild[0];

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\BoardRevision");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.BoardRevision ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\SubFunction");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.SubFunction ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\SubDevice");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.SubDevice ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\InterfaceNr");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.InterfaceNr ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\PRI");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.PRI ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\PCIDMA");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.PCIDMA ;
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\AnalogChannels");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceConfig.analogChannels ;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Info\\DSPState");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->lines[0].pInterface->DSPState ;

	pLib->info_parse_last  = pLib->cur_parse_entry - 1; 
	/*
	  End Info
	*/


	pLib->statistic_parse_first = pLib->cur_parse_entry;

	/*
		Outgoing Calls
		*/
  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {

		strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
						"Statistics\\Outgoing Calls\\Calls");
		pLib->parse_table[pLib->cur_parse_entry++].variable = \
																			&pLib->InterfaceStat.outg.Calls;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\Connected");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.Connected;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\User Busy");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.User_Busy;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\No Answer");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.No_Answer;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\Wrong Number");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.Wrong_Number;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\No Channel Avail");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.No_Channel_Avail;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\Call Rejected");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.Call_Rejected;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\Other Failures");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.Other_Failures;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Outgoing Calls\\Abandoned");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.outg.Abandoned;

	} else {
    for (i = 0; i < pLib->Channels; i++) {

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"State\\Line-%d\\L3 Stats\\Outgoing Calls\\Calls", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->lines[i].pInterfaceStat->outg.Calls;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"State\\Line-%d\\L3 Stats\\Outgoing Calls\\Connected", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->lines[i].pInterfaceStat->outg.Connected;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\User Busy", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                    &pLib->lines[i].pInterfaceStat->outg.User_Busy;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\No Answer", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
      															&pLib->lines[i].pInterfaceStat->outg.No_Answer;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Wrong Number", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->outg.Wrong_Number;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\No Channel Avail", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->outg.No_Channel_Avail;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Call Rejected", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->outg.Call_Rejected;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Other Failures", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->outg.Other_Failures;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Outgoing Calls\\Abandoned", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->outg.Abandoned;

		}
	}

	/*
		Incoming Calls
		*/
  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {
    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Calls");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Calls;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Connected");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Connected;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\User Busy");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.User_Busy;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Call Rejected");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Call_Rejected;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Wrong Number");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Wrong_Number;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\No Channel Avail");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.No_Channel_Avail;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Incompatible Dst");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Incompatible_Dst;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\No Channel Avail");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.No_Channel_Avail;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Out of Order");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Out_of_Order;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Ignored");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Ignored;

    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Incoming Calls\\Abandoned");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.inc.Abandoned;
  } else {
		for (i = 0; i < pLib->Channels; i++) {
      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Calls", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Calls;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Connected", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Connected;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\User Busy", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.User_Busy;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Call Rejected", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Call_Rejected;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Wrong Number", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Wrong_Number;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Incompatible Dst", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Incompatible_Dst;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\No Channel Avail", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.No_Channel_Avail;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Out of Order", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Out_of_Order;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Ignored", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Ignored;

      sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
              "State\\Line-%d\\L3 Stats\\Incoming Calls\\Abandoned", i+1);
      pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                        &pLib->lines[i].pInterfaceStat->inc.Abandoned;
		}
	}

  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {
    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Layer1\\Framing Errors");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.layer1.Framing_Errors;
    
    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Layer1\\CRC4 Errors");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.layer1.CRC4_Errors;
    
    strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
            "Statistics\\Layer1\\Frame Slips");
    pLib->parse_table[pLib->cur_parse_entry++].variable = \
                                      &pLib->InterfaceStat.layer1.Frame_Slips;

   } 
	
	/*
		Modem Statistics
		*/
  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {
	pLib->mdm_statistic_parse_first = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Normal");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Normal;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Unspecified");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Unspecified;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Busy Tone");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Busy_Tone;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Congestion");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Congestion;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Carr. Wait");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Carr_Wait;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Trn Timeout");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Trn_Timeout;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Incompat.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Incompat;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc Frame Rej.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_Frame_Rej;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\Modem\\Disc V42bis");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.mdm.Disc_V42bis;

	pLib->mdm_statistic_parse_last  = pLib->cur_parse_entry - 1;
  }
  else {
		for (i = 0; i < pLib->Channels; i++) {
			pLib->mdm_statistic_parse_first = pLib->cur_parse_entry;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Normal", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Normal;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Unspecified", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Unspecified;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Busy Tone", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Busy_Tone;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Congestion", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Congestion;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Carr. Wait", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Carr_Wait;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Trn Timeout", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Trn_Timeout;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Incompat.", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Incompat;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc Frame Rej.", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_Frame_Rej;

			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
							"Statistics\\Line-%d\\Modem\\Disc V42bis", i+1);
			pLib->parse_table[pLib->cur_parse_entry++].variable = \
																				&pLib->lines[i].pInterfaceStat->mdm.Disc_V42bis;

			pLib->mdm_statistic_parse_last  = pLib->cur_parse_entry - 1;
    }
    
  }

	/*
		Fax Statistics
		*/
  if (pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG ) {
	pLib->fax_statistic_parse_first = pLib->cur_parse_entry;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Normal");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Normal;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Not Ident.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Not_Ident;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc No Response");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_No_Response;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Retries");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Retries;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Unexp. Msg.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Unexp_Msg;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc No Polling.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_No_Polling;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Training");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Training;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Unexpected");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Unexpected;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Application");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Application;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Incompat.");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Incompat;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc No Command");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_No_Command;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Long Msg");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Long_Msg;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Supervisor");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Supervisor;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc SUB SEP PWD");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_SUB_SEP_PWD;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Invalid Msg");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Invalid_Msg;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Page Coding");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Page_Coding;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc App Timeout");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_App_Timeout;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\Disc Unspecified");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.Disc_Unspecified;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\TX Pages Total");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.TX_Pages_Total;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\TX Pages Retrain");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.TX_Pages_Retrain;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\TX Pages Reject");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.TX_Pages_Reject;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\RX Pages Total");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.RX_Pages_Total;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\RX Pages Retrain");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.RX_Pages_Retrain;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\FAX\\RX Pages Reject");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.fax.RX_Pages_Reject;

	pLib->fax_statistic_parse_last  = pLib->cur_parse_entry - 1;
  }
  else {
  
		for (i = 0; i < pLib->Channels; i++) {
 			pLib->fax_statistic_parse_first = pLib->cur_parse_entry;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Normal", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Normal;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Not Ident.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Not_Ident;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc No Energy", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_No_Energy;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Peer No FAX", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Peer_No_FAX;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc No Response", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_No_Response;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Retries", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Retries;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Unexp. Msg.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Unexp_Msg;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc No Polling.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_No_Polling;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Training", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Training;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\FAX\\Line-%d\\Disc Unexpected", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Unexpected;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Application", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Application;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Incompat.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Incompat;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc No Command", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_No_Command;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Long Msg", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Long_Msg;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Supervisor", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Supervisor;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc SUB SEP PWD", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_SUB_SEP_PWD;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Invalid Msg", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Invalid_Msg;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Page Coding", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Page_Coding;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc App Timeout", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_App_Timeout;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Mark React.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Mark_React;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Trn Timeout", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Trn_Timeout;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Unexp. V.21", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Unexp_V21;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Prim CTS ON", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Prim_CTS_ON;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Turnaroundp", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Turnaroundp;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc V.8 Incomp.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_V8_Incomp;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Peer ECM Bug", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Peer_ECM_Bug;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Below Speed", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Below_Speed;
 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Overhead Ex.", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Overhead_Ex;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\Disc Unspecified", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.Disc_Unspecified;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\TX Pages Total", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.TX_Pages_Total;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\TX Pages Retrain", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.TX_Pages_Retrain;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\TX Pages Reject", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.TX_Pages_Reject;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\RX Pages Total", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.RX_Pages_Total;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\RX Pages Retrain", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.RX_Pages_Retrain;

 			sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
 							"Statistics\\Line-%d\\FAX\\RX Pages Reject", i+1);
 			pLib->parse_table[pLib->cur_parse_entry++].variable = \
 																				&pLib->lines[i].pInterfaceStat->fax.RX_Pages_Reject;

 			pLib->fax_statistic_parse_last  = pLib->cur_parse_entry - 1;
    }
  
  
  }
	/*
		B-Layer1"
		*/
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\X-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.X_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\X-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.X_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\X-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.X_Errors;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\R-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.R_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\R-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.R_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer1\\R-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b1.R_Errors;

	/*
		B-Layer2
		*/
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\X-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.X_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\X-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.X_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\X-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.X_Errors;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\R-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.R_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\R-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.R_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\B-Layer2\\R-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.b2.R_Errors;

	/*
		D-Layer1
		*/
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\X-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.X_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\X-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.X_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\X-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.X_Errors;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\R-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.R_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\R-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.R_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer1\\R-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d1.R_Errors;

	/*
		D-Layer2
		*/
	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\X-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.X_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\X-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.X_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\X-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.X_Errors;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\R-Frames");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.R_Frames;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\R-Bytes");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.R_Bytes;

	strcpy (pLib->parse_table[pLib->cur_parse_entry].path,
					"Statistics\\D-Layer2\\R-Errors");
	pLib->parse_table[pLib->cur_parse_entry++].variable = \
																		&pLib->InterfaceStat.d2.R_Errors;


	pLib->statistic_parse_last  = pLib->cur_parse_entry - 1;
	
}

static void diva_trace_error (diva_strace_context_t* pLib,
															int error, const char* file, int line) {
	if (pLib->user_proc_table.error_notify_proc) {
		(*(pLib->user_proc_table.error_notify_proc))(\
																						pLib->user_proc_table.user_context,
																						&pLib->instance, pLib->Adapter,
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
	Delivery notification to user
	*/
static void diva_trace_notify_user (diva_strace_context_t* pLib,
																		int Channel,
																		int notify_subject) {
	if (pLib->user_proc_table.notify_proc) {
		(*(pLib->user_proc_table.notify_proc))(pLib->user_proc_table.user_context,
																					 &pLib->instance,
																					 pLib->Adapter,
																					 &pLib->lines[Channel],
																					 notify_subject);
	}
}

/*
	Read variable value to they destination based on the variable type
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
	byte* ptr = (byte*)&pVar->path_length;
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
	byte* ptr = (byte*)&pVar->path_length;
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

static int SuperTraceSetAudioTap  (void* hLib, int Channel, int on) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((Channel < 1) || (Channel > pLib->Channels)) {
		return (-1);
	}
	Channel--;

	if (on) {
		pLib->audio_tap_mask |=  (1L << Channel);
	} else {
		pLib->audio_tap_mask &= ~(1L << Channel);
	}

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceSetBChannel  (void* hLib, int Channel, int on) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((Channel < 1) || (Channel > pLib->Channels)) {
		return (-1);
	}
	Channel--;

	if (on) {
		pLib->bchannel_trace_mask |=  (1L << Channel);
	} else {
		pLib->bchannel_trace_mask &= ~(1L << Channel);
	}

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceSetDChannel  (void* hLib, int on) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (on) {
		pLib->trace_event_mask |= (TM_D_CHAN | TM_C_COMM | TM_DL_ERR | TM_LAYER1);
	} else {
		pLib->trace_event_mask &= ~(TM_D_CHAN | TM_C_COMM | TM_DL_ERR | TM_LAYER1);
	}

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceSetInfo (void* hLib, int on) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (on) {
		pLib->trace_event_mask |= TM_STRING;
	} else {
		pLib->trace_event_mask &= ~TM_STRING;
	}

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceClearCall (void* hLib, int Channel) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((Channel < 1) || (Channel > pLib->Channels)) {
		return (-1);
	}
	Channel--;

	pLib->clear_call_command |= (1L << Channel);

	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceInterfaceIdentify (void* hLib, int on) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (on != 0) {
		if (pLib->identify_start == 0)
			pLib->identify_start = 1;

		if (pLib->identify_stop == 1)
			pLib->identify_stop = 0;
	} else {
		if (pLib->identify_stop == 0)
			pLib->identify_stop  = 1;

		if (pLib->identify_start == 1)
			pLib->identify_start = 0;
	}

	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceInterfaceDisableTrace (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (pLib->trace_on == 0 && pLib->trace_length_init == 0) {
		pLib->trace_on = 2;
		pLib->trace_length_init = 1;
		return (0);
	}

	return (-1);
}

static int DivaSTraceInterfaceGetNrChannels (void* hLib) {
  diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

  return (pLib->Channels);
}

/*
  Update General Config
  */
static int diva_ifc_config (diva_strace_context_t* pLib,
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

/*
  Update General Info
  */
static int diva_ifc_info (diva_strace_context_t* pLib,
																diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, found_var = 0;

	if ((cur = find_var (pVar, DIVA_LAW_PATH)) != 0) {
		dword ulaw = 0;
		char v;

		if (diva_trace_read_variable (pVar, &ulaw) == 0) {
			v = (ulaw != 0) ? 'u' : 'a';
		} else {
			v = '?';
		}
		if (v != pLib->law) {
			pLib->law = v;
			pLib->lines[0].pInterface->pConfig->law = v;
			diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_INTERFACE_CHANGE);
		}

		return (1);
	}
	
	if (locate_var (pVar, DIVA_RESOURCE_PATH_PREFIX, DIVA_RESOURCE_PATH_PREFIX_LENGTH) == 0) {
		diva_resource_info_entry_t resources[255];
		int i;

		for (i = 0; pVar != 0 && i < sizeof(resources)/sizeof(resources[0]) - 1; pVar = get_next_var (pVar)) {
			resources[i].resourceCount = 0;
			diva_trace_read_variable (pVar, &resources[i].resourceCount);
			if (resources[i].resourceCount != 0) {
				const char* path = (char*)&pVar->path_length + 1 + DIVA_RESOURCE_PATH_PREFIX_LENGTH;
				const char* p = memchr (path, ':', pVar->path_length - DIVA_RESOURCE_PATH_PREFIX_LENGTH);

				if (p != 0) {
					memcpy (resources[i].resourceName, path, p - path);
					resources[i].resourceName[p - path] = 0;
					resources[i].resourceType = *path;
					resources[i].resourceStandard = atoi(path+1);
					memcpy (resources[i].resourceVisualName,
									p + 2,
									pVar->path_length - DIVA_RESOURCE_PATH_PREFIX_LENGTH - (p - path) - 2);
					resources[i].resourceVisualName[pVar->path_length - DIVA_RESOURCE_PATH_PREFIX_LENGTH - (p - path) - 2] = 0;
					i++;
				}
			}
		}
		resources[i].resourceName[0] = 0;

		pLib->Interface.Resource = resources;
		diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_NOTIFY_RESOURCE_UPDATE);
		pLib->Interface.Resource = 0;

		return (1);
	}

  for (i  = pLib->info_parse_first; i <= pLib->info_parse_last; i++) {
    cur = pVar;
	  if (cur && (cur = find_var (cur, pLib->parse_table[i].path))) {
	    found_var |= 1;

		  if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
			  diva_trace_error (pLib, -3 , __FILE__, __LINE__);
			  return (-1); /* error */
		  }
	  }
	}
	return (found_var);
}

/*
	Parse and update cumulative statistice
	*/
static int diva_ifc_statistics (diva_strace_context_t* pLib,
																diva_man_var_header_t* pVar) {
	diva_man_var_header_t* cur;
	int i, one_updated = 0, mdm_updated = 0, fax_updated = 0;

	for (i  = pLib->statistic_parse_first; i <= pLib->statistic_parse_last; i++) {
		if ((cur = find_var (pVar, pLib->parse_table[i].path))) {
			if (diva_trace_read_variable (cur, pLib->parse_table[i].variable)) {
				diva_trace_error (pLib, -3 , __FILE__, __LINE__);
				return (-1);
			}
			one_updated = 1;
      if ((i >= pLib->mdm_statistic_parse_first) && (i <= pLib->mdm_statistic_parse_last)) {
        mdm_updated = 1;
      }
      if ((i >= pLib->fax_statistic_parse_first) && (i <= pLib->fax_statistic_parse_last)) {
        fax_updated = 1;
      }
		}
	}

	/*
		We do not use first event to notify user - this is the event that is
		generated as result of EVENT ON operation and is used only to initialize
		internal variables of application
		*/
  if (mdm_updated) {
		diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_NOTIFY_MDM_STAT_CHANGE);
  } else if (fax_updated) {
		diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_NOTIFY_FAX_STAT_CHANGE);
  } else if (one_updated) {
    if (pLib->InterfaceConfig.type==DIVA_ADAPTER_TYPE_ANALOG) {
      for (i=0; i<pLib->Channels; i++) {
			  diva_trace_notify_user (pLib, i, DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE);
			}
		} else {
			diva_trace_notify_user (pLib, 0, DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE);
		}
		pLib->initial_cfg_complete_event = 1;
	}

	return (one_updated ? 0 : -1);
}

static int SuperTraceGetOutgoingCallStatistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->outgoing_ifc_stats = (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_ANALOG ? (1L << pLib->Channels) - 1 : 1);

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetIncomingCallStatistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->incoming_ifc_stats = (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_ANALOG ? (1L << pLib->Channels) - 1 : 1);

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetLayer1Statistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
        if(pLib->InterfaceConfig.type != DIVA_ADAPTER_TYPE_ANALOG) { 
	  pLib->l1_ifc_stats =  1;

	  pLib->law_req++;
          return (ScheduleNextTraceRequest (pLib));
        }
	return(0);
}

static int SuperTraceGetModemStatistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->modem_ifc_stats = (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_ANALOG ? (1L << pLib->Channels) - 1 : 1);

	pLib->law_req++;
	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetFaxStatistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->fax_ifc_stats = (pLib->InterfaceConfig.type == DIVA_ADAPTER_TYPE_ANALOG ? (1L << pLib->Channels) - 1 : 1);

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetBLayer1Statistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->b1_ifc_stats = 1;

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetBLayer2Statistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->b2_ifc_stats = 1;

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetDLayer1Statistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->d1_ifc_stats = 1;

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int SuperTraceGetDLayer2Statistics (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;
	pLib->d2_ifc_stats = 1;

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceGetBLayer1ChannelStatistics (void* hLib, int channel) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((channel < 1) || (channel > pLib->Channels)) {
		return (-1);
	}
	channel--;

	if (pLib->channel_b1_b2_statistics_available) {
		pLib->channel_b1_stat |=  (1L << channel);
	}

	pLib->law_req++;


	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceGetBLayer2ChannelStatistics (void* hLib, int channel) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((channel < 1) || (channel > pLib->Channels)) {
		return (-1);
	}
	channel--;

	if (pLib->channel_b1_b2_statistics_available) {
		pLib->channel_b2_stat |=  (1L << channel);
	}

	pLib->law_req++;

	return (ScheduleNextTraceRequest (pLib));
}

static int DivaSTraceSetInterfaceFeatures (void* hLib, diva_strace_ifc_features_t features) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if ((features & DivaSTraceFeatureGlobalStatisticsPolling) != 0) {
		if (pLib->OutgoingCallsCallsActive     != 0 ||
				pLib->OutgoingCallsConnectedActive != 0 ||
				pLib->IncomingCallsCallsActive     != 0 ||
				pLib->IncomingCallsConnectedActive != 0) {
			return (-1);
		}
		features &= ~DivaSTraceFeatureGlobalStatisticsPolling;
		pLib->OutgoingCallsCallsActive     = 0x7fffffff;
		pLib->OutgoingCallsConnectedActive = 0x7fffffff;
		pLib->IncomingCallsCallsActive     = 0x7fffffff;
		pLib->IncomingCallsConnectedActive = 0x7fffffff;
	}

	return ((features == 0) ? 0 : -1);
}

static int SuperTraceGetResourceInfo (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (pLib->resource_info_state > 0 && pLib->resource_info_req == pLib->resource_info_ack) {
		pLib->resource_info_req++;
		return (ScheduleNextTraceRequest (pLib));
	}

	return (0);
}

static int SuperTraceUpdateTime (void* hLib) {
	diva_strace_context_t* pLib = (diva_strace_context_t*)hLib;

	if (pLib->time_update_state == 0) {
		pLib->time_update_state = 1;
		return (0);
	}

	return ((pLib->time_update_state > 0) ? 1 : -1);
}

static void diva_create_channel_b1_stat_parse_table (diva_strace_context_t* pLib, int Channel) {
	diva_prot_statistics_t* pStat = &pLib->lines[Channel].L1_Stats;
	int nr = Channel+1;

	if ((pLib->cur_parse_entry + CHANNEL_B1_STAT_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}

	pLib->channel_b1_parse_entry_first[Channel] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\X-Frames", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Frames;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\X-Bytes", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Bytes;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\X-Errors", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Errors;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\R-Frames", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Frames;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\R-Bytes", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Bytes;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L1 Stats\\R-Errors", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Errors;

	pLib->channel_b1_parse_entry_last[Channel] = pLib->cur_parse_entry - 1;
}

static void diva_create_channel_b2_stat_parse_table (diva_strace_context_t* pLib, int Channel) {
	diva_prot_statistics_t* pStat = &pLib->lines[Channel].L2_Stats;
	int nr = Channel+1;

	if ((pLib->cur_parse_entry + CHANNEL_B2_STAT_PARSE_ENTRIES) >= pLib->parse_entries) {
		diva_trace_error (pLib, -1, __FILE__, __LINE__);
		return;
	}

	pLib->channel_b2_parse_entry_first[Channel] = pLib->cur_parse_entry;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\X-Frames", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Frames;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\X-Bytes", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Bytes;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\X-Errors", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->X_Errors;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\R-Frames", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Frames;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\R-Bytes", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Bytes;

	sprintf (pLib->parse_table[pLib->cur_parse_entry].path,
					 "State\\%s%d\\L2 Stats\\R-Errors", pLib->line_id, nr);
	pLib->parse_table[pLib->cur_parse_entry++].variable = &pStat->R_Errors;

	pLib->channel_b2_parse_entry_last[Channel] = pLib->cur_parse_entry - 1;
}

static int diva_init_cardtype (diva_strace_context_t* pLib, dword ct) {
  int i;

// printf("Init CardType dependent structs for adapter %d\n",pLib->Adapter);
  pLib->InterfaceConfig.cardtype = ct;

  /* set to idi_name if cardtype=0 else point to cardtype.h*/
  pLib->instance.adapter_display_name = (ct ? CardProperties[ct].Name : pLib->instance.adapter_name);

  switch (CardProperties[ct].Card) {
  case CARD_POTS:
  case CARD_POTSV:

			/*
				 lines[0].pInterface = pLib->Interface which resembles PRI/BRI behaviour.
				 The remaining up to 7 interface states are stored in own structures on heap.
				 This is only valid for analog adapters, where each channel has its own
				 layer1/2 state.
				 The same goes for InterfaceStat which is available per line starting from
				 Version DS 8.0.
			*/

			pLib->InterfaceConfig.type = DIVA_ADAPTER_TYPE_ANALOG;
			pLib->line_id = LINE_ID[1];  /* Line- or B */

			for ( i=1; i < pLib->Channels; i++ ) {
				diva_trace_interface_state_t* pS = (diva_trace_interface_state_t*)malloc(sizeof(diva_trace_interface_state_t));
				diva_ifc_statistics_t* pI = (diva_ifc_statistics_t*)malloc(sizeof(diva_ifc_statistics_t));

				if (!pS || !pI ) {
					/* error allocating memory */
					DBG_PRINT((logfile,"lib_init: ERROR allocating heap memory for analog pInterface structs, adapter %d line %d\n", pLib->Adapter, i));
					for (;i>1;) {
						free(pLib->lines[--i].pInterface);
						free(pLib->lines[i].pInterfaceStat);
					}
					return (-1);
				}
				memcpy(pS,&pLib->Interface,sizeof(*pS)); /* init with contents of Interface[0] */
				pLib->lines[i].pInterface = pS;
				memcpy(pI,&pLib->InterfaceStat,sizeof(*pI)); /* init with contents of InterfaceStat[0] */
				pLib->lines[i].pInterfaceStat = pI;
			}
			break;

	case CARD_SOFTIP:
			if(pLib->InterfaceConfig.cardtype == CARDTYPE_DIVASRV_SOFTIP_V20) {
				pLib->InterfaceConfig.type = DIVA_ADAPTER_TYPE_SOFTIP;
			}
			else {
				pLib->InterfaceConfig.type = DIVA_ADAPTER_TYPE_HM;
			}

			break;

	default:
	  /* for bri and pri adapters the type is set using the manpath Config/PRI via parsetable */
	  ;
	}

  return(0);
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
