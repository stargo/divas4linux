
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

#if !defined(LINUX) /* { */
#include <windows.h>
#endif
/* } */
#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include "st_ifc.h"
#include "dlist.h"
#include "diva_log.h"
#include "record.h"
#include "usage.h"
#include "syslog.h"
#include "idi_defs.h"
#include "os.h"


/*
	LOCALS
	*/
static void diva_update_line_state (FILE** Log,
																		int Adapter,
																		int Ch,
																		const diva_trace_line_state_t* info,
																		const char* adapter_name,
																		dword adapter_serial_number);
static void diva_update_sip_state (FILE** Log,
																		int Adapter,
																		int Ch,
																		const diva_sip_channel_info_t* info,
																		const char* adapter_name,
																		dword adapter_serial_number);
static void diva_update_modem_state (FILE** Log,
																		 int Adapter,
																		 int Ch,
																		 const diva_trace_line_state_t* info,
																		 const char* adapter_name,
																		 dword adapter_serial_number);
static void diva_update_fax_state (FILE** Log,
																	 int Adapter,
																	 int Ch,
																	 const diva_trace_line_state_t* info,
																	 const char* adapter_name,
																	 dword adapter_serial_number);
static void diva_update_channel_statistics (FILE** Log,
																						int Adapter,
																						int Ch,
																						const diva_trace_line_state_t* info,
																						const char* adapter_name,
																						dword adapter_serial_number);

static void diva_update_registrar_state(FILE** Log, diva_sip_info_t* sip_config);

int diva_combine_sip_ifc_stats (user_context_t* user_context, int Adapter);

typedef struct _diva_line_state_map {
	int state;
	const char* idi_name;
	const char* pretty_name;
} diva_line_state_map_t;

static diva_line_state_map_t line_state_map[] = {
{ 0,  "Idle",          " - " },
{ 1,  "Connected-In",  "IN " },
{ 2,  "Connected-Out", "OUT" },
{ -1,  0,              " - " }
};

static diva_log_record_t state[DIVA_MAX_ADAPTERS][DIVA_MAX_LINES];
static diva_log_ifc_info_t ifc_state[DIVA_MAX_ADAPTERS];


/* internal helper function
	 used to check if eventlog msg should be written
	 */
int diva_eventlog_check(char * oldstate, char * newstate, int cardtype) {
	int i=0;

	if ( strcmp(oldstate,newstate) /* || oldstate[0]==0 KWu: removed because there should be no reason, caused all up entries to be written twice */) {
		if( newstate[0] == 0 ) return (0); /* ignore when new state undefined */
	  if (cardtype == DIVA_ADAPTER_TYPE_ANALOG) {
			//printf("diva_eventlog_check: %s -> %s\n", oldstate, newstate);
			if (strstr(newstate, "Down") || strstr(oldstate, "Down")) {
				i = 1;
			}  
			return (i);
		}

		/* Ignore "Lost Framing", "Syncronized", "Hook Off" and "Idle" after "Hook Off": Dont generate traps*/
		if (!strstr(newstate, "Lost Framing")) {
			if (!strstr(newstate, "Syncronized")) {
				if (!strstr(newstate, "Hook Off")) {   /* for analog cards */
					if (!( strstr(newstate, "Idle") && strstr(oldstate, "Hook Off"))) { /* for analog cards */
						i=1;
					} else {
						// DBG_SNMPX((logfile,"diva_trap_check: ignore \"Idle\" after \"Hook Off\"\n"));
					}
				} else {
					// DBG_SNMPX((logfile,"diva_trap_check: ignore \"Hook Off\"\n"));
				}
			} else {
				// DBG_SNMPX((logfile,"diva_trap_check: ignore \"Syncronized\"\n"));
			}
		} else {
			// DBG_SNMPX((logfile,"diva_trap_check: ignore \"Lost Framing\"\n"));
		}

		if ((i == 1) && (strstr(newstate, "Down") == 0)) {
	    i=2;
		}

	}
	return(i);
}


/* local helper */
static int cmp_adapter_nr (const void* what, const diva_entity_link_t* p) {
	diva_adapter_t* pA = (diva_adapter_t*)p;
	int nr = *(const int*)what;
	return (nr != pA->adapter_nr);
}

/*
	Handles internal library error. One of possible errors is the
	trace event buffer overflow. In case it happens we need to re-read
	all state directories on appropriate adapter.
	All other errors to be considered as fatal and should cause application
	exit.
	*/
void diva_log_error_proc (void* user_context,
													diva_strace_library_interface_t* hLib,
													int Adapter,
													int error,
													const char* file,
													int line) {
	syslog (LOG_ERR,
					"internal error:%d at Adapter:%d, file:%s, line:%d\n",
					error, Adapter, file, line);
}
#if 0
int getAllSubAdapters(FILE** Log, int Adapter, int adapterSubIndex[], int max_adapter)
{
  int subAdapterIndex = 0;
  if(diva_ifc_initialised(Log, Adapter)) {
    if(ifc_state[Adapter].state[0].pConfig) {
      /* first get cardtype and serial of adapter */  
      dword cardtype  = ifc_state[Adapter].state[0].pConfig->cardtype;
      dword serial_nr = ifc_state[Adapter].state[0].pConfig->serial_nr &0x00FFFFFF;
      /* and now loop over the ifcstate array */
      int adapterIndex = 0;
      for(adapterIndex = 0; adapterIndex < max_adapter; adapterIndex++) {
        if(   ( ifc_state[adapterIndex].state[0].pConfig)
           && ( ifc_state[adapterIndex].state[0].pConfig->cardtype               == cardtype)
           &&((ifc_state[adapterIndex].state[0].pConfig->serial_nr &0x00FFFFFF)  == serial_nr)) {
          if(diva_ifc_initialised(Log, adapterIndex)) {
            adapterSubIndex[subAdapterIndex] = adapterIndex;
            subAdapterIndex++;
          }
        }
      }
      adapterSubIndex[subAdapterIndex]=max_adapter+1;  
    }
  }
  return(subAdapterIndex);  
}
#endif

/*
	We are interested only in one type 2 types of events:
	1 - Line state change event
	2 - Interface state change event. This allows indication of connection
			loss due to Layer 1/Layer 2 problems
	*/
void diva_log_notify_proc (void* user_context,
													 diva_strace_library_interface_t* hLib,
													 int Adapter,
													 diva_trace_line_state_t* channel,
													 int notify_subject) {
	FILE** Log = (FILE**)user_context;
	switch (notify_subject) {
		case DIVA_SUPER_TRACE_TEMPERATURE_CHANGE:
			if (channel->pInterfaceStat && channel->pInterface && ((channel->ChannelNumber == 1)  ||
					((channel->pInterface->pConfig) &&
					(channel->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_ANALOG)))) {
				ifc_state[Adapter].state[0].InitialTemperature = channel->pInterface->InitialTemperature;
				ifc_state[Adapter].state[0].MinTemperature     = channel->pInterface->MinTemperature;
				ifc_state[Adapter].state[0].MaxTemperature     = channel->pInterface->MaxTemperature;
				ifc_state[Adapter].state[0].Temperature        = channel->pInterface->Temperature;

				diva_update_ifc_state (Log, Adapter, channel->ChannelNumber, &ifc_state[Adapter]);
			}
			break;

		case DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE:

		  /* if this is a softip call, schedule reading of call related data */
		  if ( channel->pInterface && channel->pInterface->pConfig &&
			     ( channel->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_SOFTIP) ) {
				diva_entity_queue_t* q = (diva_entity_queue_t*)((user_context_t*)user_context)->adapters;
				diva_adapter_t* pA;
				int i=1007;
				if ((pA = (diva_adapter_t*) diva_q_find (q, &i, cmp_adapter_nr))) {
					if (pA->hLib) {
						((diva_strace_library_sip_interface_t*)pA->hLib)->DivaSTraceSipGetCallData (pA->hLib, hLib, channel->pInterface->pConfig->InterfaceNr, (int)channel->ChannelNumber, (dword)channel->CallReference);
					}
				}
			}

			diva_update_line_state (Log,
															Adapter,
															(int)channel->ChannelNumber,
															channel,
															&hLib->adapter_name[0],
															hLib->adapter_serial_number);
			break;

		/* process data retrieved by reading sip service, update channels */
		case DIVA_SUPER_TRACE_NOTIFY_SIP_CHANGE:
			{
		  	diva_sip_channel_info_t* sip_state = (diva_sip_channel_info_t*)channel; /* channel is misused as void* */
				diva_update_sip_state (Log,
																Adapter,
																(int)sip_state->channel_nr,
																sip_state,
																&hLib->adapter_name[0],
																hLib->adapter_serial_number);
			}
			break;

		case DIVA_SUPER_TRACE_NOTIFY_SIP_REGISTRAR:
			{
				diva_sip_info_t* sip_config = (diva_sip_info_t*)channel;
				diva_update_registrar_state(Log,sip_config);
			}
			break;

		case DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE:
			diva_update_modem_state (Log,
															 Adapter,
															 (int)channel->ChannelNumber,
															 channel,
															 &hLib->adapter_name[0],
															 hLib->adapter_serial_number);
			break;

		case DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE:
			diva_update_fax_state (Log,
														 Adapter,
														 (int)channel->ChannelNumber,
														 channel,
														 &hLib->adapter_name[0],
														 hLib->adapter_serial_number);
			break;

		case DIVA_SUPER_TRACE_INTERFACE_CHANGE:
			if ( channel->pInterfaceStat && channel->pInterface &&
					 (channel->ChannelNumber <= DIVA_SUPPORTED_CHANNELS) &&
			     ( (channel->ChannelNumber == 1)  ||
			     ( (channel->pInterface->pConfig) &&
			     ( channel->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_ANALOG)))) {

				/* Check if Eventlog entry should be generated */
				/* compare new state channel->... and saved state ifc_state[]... */
				int eventlogflag=0;
				switch (channel->pInterface->pConfig->type) {
					/* ignore softip adapters */
					case DIVA_ADAPTER_TYPE_SOFTIP:
						break;
					/* check StableL2 for all adapters except ... */
					default:
						if (channel->pInterface->pConfig->StableL2 < 2)	break;
					/* ... analog cards */
					case DIVA_ADAPTER_TYPE_ANALOG:
						eventlogflag = diva_eventlog_check(ifc_state[Adapter].state[channel->ChannelNumber - 1].Layer1, 
																							 channel->pInterface->Layer1, 
																							 channel->pInterface->pConfig->type) != 0;
				}

				ifc_state[Adapter].state[channel->ChannelNumber - 1] = *channel->pInterface;
				ifc_state[Adapter].statistics = *channel->pInterfaceStat;

				if (channel->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_SOFTIP) {
					/*
					  all softip adapter statistics have to be added to the first one
					*/
					int sip_adapter = diva_combine_sip_ifc_stats (user_context, Adapter);
					if (!sip_adapter) {
						sip_adapter=Adapter; /* for startup */
					}
					diva_update_ifc_state (Log, Adapter, channel->ChannelNumber, &ifc_state[sip_adapter]);
				} else {
					diva_update_ifc_state (Log, Adapter, channel->ChannelNumber, &ifc_state[Adapter]);
				}
				if (eventlogflag) {
					printf("Eventlog entry for adapter %d, flag %d\n",Adapter, eventlogflag);
					diva_eventlog_layer1 (Log, Adapter, channel->ChannelNumber, &ifc_state[Adapter]);
				}

			}
			break;
		case DIVA_SUPER_TRACE_NOTIFY_MDM_STAT_CHANGE:
		case DIVA_SUPER_TRACE_NOTIFY_FAX_STAT_CHANGE:
		case DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE:
			{  
				int a = Adapter;
				diva_entity_queue_t* q = (diva_entity_queue_t*)((user_context_t*)user_context)->adapters;
				diva_adapter_t* pA;
				if ((pA = (diva_adapter_t*) diva_q_find (q, &a, cmp_adapter_nr))) {
					pA->stat_update=time(0);
				//	printf("Adapter%x pA->stat_update %x\n", Adapter, (unsigned int)pA->stat_update);
				}

				diva_update_channel_statistics (Log,
																			Adapter,
																			(int)channel->ChannelNumber,
																			channel,
																			&hLib->adapter_name[0],
																			hLib->adapter_serial_number);
		  }
			break;
		case DIVA_SUPER_TRACE_NOTIFY_RESOURCE_UPDATE:
			diva_update_adapter_resource (Log, Adapter, channel->pInterface->Resource);
			break;
	}
}

static void add_ifc_stats(diva_ifc_statistics_t* ifc_a, diva_ifc_statistics_t* ifc_b) {
	dword * p = (dword *)ifc_a;
	dword * q = (dword *)ifc_b;
	while ((byte *) p < (byte *) ifc_a + sizeof(diva_ifc_statistics_t) ) {
		*(p++) += *(q++);
	}
}

int diva_combine_sip_ifc_stats (user_context_t* user_context, int Adapter) {
	diva_log_softip_map_t* sa = &user_context->softip_adapters;
	int i = sa->first_softip_adapter;
	if (i==Adapter) {
		for (++i; sa->softip_adapter_map[i]; i++ ) {
		  add_ifc_stats(&ifc_state[sa->first_softip_adapter].statistics, &ifc_state[i].statistics);
		}
	} else {
	  /* nothing */
	}
	return (sa->first_softip_adapter);
}

/*
	Ignore for this application
	*/
void diva_log_trace_proc (void* user_context,
													diva_strace_library_interface_t* hLib,
													int Adapter,
													void* xlog_buffer) {



}

static void diva_update_registrar_state(FILE** Log, diva_sip_info_t* sip_config) {
	diva_sip_info_t* p = sip_config;
	diva_write_registrar_record (Log, p);
}


static void diva_update_sip_state (FILE** Log,
																		int Adapter,
																		int Ch,
																		const diva_sip_channel_info_t* info,
																		const char* adapter_name,
																		dword adapter_serial_number) {
	diva_log_record_t* pState = &state[Adapter][Ch];
	memcpy (&pState->sip, info, sizeof(diva_sip_channel_info_t));

	diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);
}

static void diva_update_line_state (FILE** Log,
																		int Adapter,
																		int Ch,
																		const diva_trace_line_state_t* info,
																		const char* adapter_name,
																		dword adapter_serial_number) {
	diva_log_record_t* pState = &state[Adapter][Ch];
	int i, channel_state_update = 0;
	char * p;

	for (i = 0; line_state_map[i].idi_name; i++) {
		if (!strcmp(line_state_map[i].idi_name, &info->Line[0])) {
			break;
		}
	}

	if (pState->line_state_name && line_state_map[i].state) {
		channel_state_update = 1;
	}
	if (!pState->line_state_name && !line_state_map[i].state) {
		return;
	}

	if (channel_state_update != 0 || pState->line_state_name == 0) {
		/*
			Go Online or update line state information
			*/
		pState->line_state_name = line_state_map[i].pretty_name;
		if (channel_state_update == 0) {
			pState->off_hook_time = time (0);
			pState->last_charges = info->Charges;
		}

		strcpy (pState->localaddress, info->LocalAddress);
		if (info->LocalSubAddress[0]) {
			strcat (pState->localaddress, "/");
			strcat (pState->localaddress, info->LocalSubAddress);
		}

		strcpy (pState->remoteaddress, info->RemoteAddress);
		if (info->RemoteSubAddress[0]) {
			strcat (pState->remoteaddress, "/");
			strcat (pState->remoteaddress, info->RemoteSubAddress);
		}
		/* remove commas from remoteaddress */
		for (p=pState->remoteaddress; *p; p++) {
			while (*p==',') {
				char * q=p;
				while ( (*(q++)=*(q+1)) );
			}
		}

		strcpy (pState->protocol, info->Framing);
		if (strcmp (info->Layer2, "Transparent")) {
			strcat (pState->protocol, "/");
			strcat (pState->protocol, info->Layer2);

			if (strcmp (info->Layer3, "Transparent")) {
				strcat (pState->protocol, "/");
				strcat (pState->protocol, info->Layer3);
			}
		}

		strcpy (pState->application, info->UserID);

		pState->charges = info->Charges;

		pState->adapter_number = (dword)Adapter;
		pState->line_number    = (dword)Ch;

		/* line went up */
		if (channel_state_update == 0) {
			pState->call_number++;
		}

		if (!pState->application[0]) {
			strcpy (pState->application, " - ");
		}

		if (Log[1]) {
			if (diva_write_call_log_record (Log[1], pState, 1)) {
				diva_syslog_record ("ERROR: call log write failed");
			}
		}

		diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);

	} else {
		/*
			Go offline
			*/
		if (pState->charges < info->Charges) {
			pState->charges = info->Charges;
		}

		if (pState->last_charges < info->Charges) {
			pState->last_charges = info->Charges - pState->last_charges;
		} else {
			pState->last_charges = 0;
		}

//		pState->LastDisconnectCause = info->LastDisconnectCause;
//		pState->AbandonedCallin = info->AbandonedCallin;
//		pState->AbandonedCallout = info->AbandonedCallout;

		if (Log[0]) {
			if (diva_write_log_record (&Log[0], pState, adapter_name, adapter_serial_number)) {
				diva_syslog_record ("ERROR: log write failed");
			}
		}
		if (Log[1]) {
			if (diva_write_call_log_record (Log[1], pState, 0)) {
				diva_syslog_record ("ERROR: call log write failed");
			}
		}
		/*
			No clear internal state variables
			*/
		pState->max_rx_speed  = 0;
		pState->min_rx_speed  = 0;
		pState->max_tx_speed  = 0;
		pState->min_tx_speed  = 0;
		pState->retrains      = 0;
		pState->max_fax_speed = 0;
		pState->min_fax_speed = 0;
		pState->pages         = 0;
		pState->fax_features  = 0;
		pState->FaxID[0]      = 0;
		pState->b1_x_frames   = 0;
		pState->b1_x_bytes    = 0;
		pState->b1_x_errors   = 0;
		pState->b1_r_frames   = 0;
		pState->b1_r_bytes    = 0;
		pState->b1_r_errors   = 0;
		pState->b2_x_frames   = 0;
		pState->b2_x_bytes    = 0;
		pState->b2_x_errors   = 0;
		pState->b2_r_frames   = 0;
		pState->b2_r_bytes    = 0;
		pState->b2_r_errors   = 0;

		pState->line_state_name = 0;

		diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);
	}
}

static void diva_update_modem_state (FILE** Log,
																		 int Adapter,
																		 int Ch,
																		 const diva_trace_line_state_t* info,
																		 const char* adapter_name,
																		 dword adapter_serial_number) {
	diva_log_record_t* pState       = &state[Adapter][Ch];
	const diva_trace_modem_state_t* modem = &info->modem;

	if (pState->max_rx_speed < modem->RxSpeed) {
		pState->max_rx_speed = modem->RxSpeed;
	}

	if ((!pState->min_rx_speed) || (pState->min_rx_speed > modem->RxSpeed)) {
		pState->min_rx_speed = modem->RxSpeed;
	}

	if (pState->max_tx_speed < modem->TxSpeed) {
		pState->max_tx_speed = modem->TxSpeed;
	}

	if ((!pState->min_tx_speed) || (pState->min_tx_speed > modem->TxSpeed)) {
		pState->min_tx_speed = modem->TxSpeed;
	}

	pState->retrains      = modem->LocalRetrains + modem->RemoteRetrains + \
													modem->LocalResyncs + modem->RemoteResyncs;

	diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);
}

static void diva_update_fax_state (FILE** Log,
																	 int Adapter,
																	 int Ch,
																	 const diva_trace_line_state_t* info,
																	 const char* adapter_name,
																	 dword adapter_serial_number) {
	diva_log_record_t* pState         = &state[Adapter][Ch];
	const diva_trace_fax_state_t* fax = &info->fax;

	if (pState->max_fax_speed < fax->Speed) {
		pState->max_fax_speed = fax->Speed;
	}
	if ((!pState->min_fax_speed) || (pState->min_fax_speed > fax->Speed)) {
		pState->min_fax_speed = fax->Speed;
	}
	pState->pages				  = fax->Page_Counter;
	pState->fax_features |= fax->Features;
	strcpy (pState->FaxID, fax->Station_ID);

	diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);
}

void diva_log_cleanup_adapter (void* user_context, int adapter) {
	FILE** Log = (FILE**)user_context;
	diva_log_record_t* pState;
	int i;

	diva_update_ifc_state (Log, adapter, 1, 0);

	for (i = 0; i < DIVA_MAX_LINES; i++) {
		pState = &state[adapter][i];
		pState->line_state_name = 0;
	}
}

void diva_log_cleanup_adapter_strace(int adapter) {
  if(IDI_IS_ADAPTER_ID(adapter)) {
    ifc_state[adapter].state[0].pConfig = 0;
  }
}

int diva_log_get_adapter_type (int adapter) {
	if(IDI_IS_ADAPTER_ID(adapter)) {
		if(ifc_state[adapter].state[0].pConfig == 0) {
			diva_os_report_error_backtrace("DivaLogD: Try to read Adapter Type when not available (%d:%d)", adapter, __LINE__);
			return(0);
		}
		return ifc_state[adapter].state[0].pConfig->type;
 	}
	else {
		diva_os_report_error_backtrace("DivaLogD: Try to read Adapter Type of adapter of of bounds (%d:%d)", adapter, __LINE__);
		return(0);
	}
}

int diva_log_get_cardtype (int adapter) {
	if(IDI_IS_ADAPTER_ID(adapter)) {
		if(ifc_state[adapter].state[0].pConfig == 0) {
			diva_os_report_error_backtrace("DivaLogD: Try to read Cardtype when not available (%d:%d)", adapter, __LINE__);
			return(0);
		}
		return ifc_state[adapter].state[0].pConfig->cardtype;
	}
	else {
		diva_os_report_error_backtrace("DivaLogD: Try to read Cardtype of adapter of of bounds (%d:%d)", adapter, __LINE__);
		return(0);
	}
}

int diva_log_get_channel_count (int adapter) {
	if(IDI_IS_ADAPTER_ID(adapter)) {
		if(ifc_state[adapter].state[0].pConfig == 0) {
			diva_os_report_error_backtrace("DivaLogD: Try to read Channel Count when not available (%d:%d)", adapter, __LINE__);
			return(0);
		}
		return ifc_state[adapter].state[0].pConfig->channels;
	}
	else {
		diva_os_report_error_backtrace("DivaLogD: Try to read Channel Count of adapter of of bounds (%d:%d)", adapter, __LINE__);
		return(0);
	}
}

void * diva_log_get_adapter_config_ptr (int adapter) {
	if(IDI_IS_ADAPTER_ID(adapter)) {
		return ifc_state[adapter].state[0].pConfig;
	}
	else {
		diva_os_report_error_backtrace("DivaLogD: Try to read cfg pointer of adapter of of bounds (%d:%d)", adapter, __LINE__);
		return(0);
	}
}
/*
	Returns zero in case channel is active
	Returns 1    in case channel is not active
	Returns -1   in case of error
	*/
int diva_log_is_channel_active (int adapter, int channel) {
	if ((adapter < 0) ||
			(adapter >= DIVA_MAX_ADAPTERS) ||
			(channel < 0) ||
			(channel >= DIVA_MAX_LINES)) {
		return (-1);
	}

	return ((state[adapter][channel].line_state_name != 0) ? 0 : 1);
}

static void diva_update_channel_statistics (FILE** Log,
																						int Adapter,
																						int Ch,
																						const diva_trace_line_state_t* info,
																						const char* adapter_name,
																						dword adapter_serial_number) {
	diva_log_record_t* pState = &state[Adapter][Ch];

	/* PRI/BRI set layer1/2 in channel 1 struct. Analog adapters set this for every channel */
	if ( info->pInterfaceStat && info->pInterface && (Ch <= DIVA_SUPPORTED_CHANNELS) &&
	   ( (Ch == 1) || (info->pInterface->pConfig && (info->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_ANALOG ) ) ) )
	{
		/* compare new state channel->... and saved state ifc_state[]... */
		int eventlogflag = 0;
		eventlogflag = diva_eventlog_check(ifc_state[Adapter].state[Ch - 1].Layer1,info->pInterface->Layer1, info->pInterface->pConfig->type) \
		          				&& (info->pInterface->pConfig->StableL2 == 2);
		ifc_state[Adapter].state[Ch - 1]  = *info->pInterface;
		ifc_state[Adapter].statistics = *info->pInterfaceStat;

		if (info->pInterface->pConfig->type == DIVA_ADAPTER_TYPE_SOFTIP) {
			/* all softip adapter statistics have to be added to the first one */
			user_context_t* user_context = (user_context_t*)Log;
			int sip_adapter = diva_combine_sip_ifc_stats (user_context, Adapter);
			if (!sip_adapter) sip_adapter=Adapter; /* for startup */
			diva_update_ifc_state (Log, Adapter, Ch, &ifc_state[sip_adapter]);
		} else {
			diva_update_ifc_state (Log, Adapter, Ch, &ifc_state[Adapter]);
		}
		if (eventlogflag) {
			printf("Eventlog entry for adapter %d, flag %d\n",Adapter, eventlogflag);
			diva_eventlog_layer1 (Log, Adapter, Ch, &ifc_state[Adapter]);
		}
	}

	if (pState->line_state_name) {
		pState->b1_x_frames   = info->L1_Stats.X_Frames;
		pState->b1_x_bytes    = info->L1_Stats.X_Bytes;
		pState->b1_x_errors   = info->L1_Stats.X_Errors;
		pState->b1_r_frames   = info->L1_Stats.R_Frames;
		pState->b1_r_bytes    = info->L1_Stats.R_Bytes;
		pState->b1_r_errors   = info->L1_Stats.X_Errors;
		pState->b2_x_frames   = info->L2_Stats.X_Frames;
		pState->b2_x_bytes    = info->L2_Stats.X_Bytes;
		pState->b2_x_errors   = info->L2_Stats.X_Errors;
		pState->b2_r_frames   = info->L2_Stats.R_Frames;
		pState->b2_r_bytes    = info->L2_Stats.R_Bytes;
		pState->b2_r_errors   = info->L2_Stats.R_Errors;

		diva_update_channel_state (Log, pState, adapter_name, adapter_serial_number);
	}
}

