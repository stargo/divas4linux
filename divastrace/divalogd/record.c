
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
#endif /* } */
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

static dword log_record_count = 0;
static dword total_connection_nr = 0;

static const char* call_record_fields_description = "# FIELDS: DIRECTION,CALL TIME,DURATION(U:M:S),REMOTE ADDR/SUBADDR,LOCAL ADDR/SUBADDR,PROTOCOL,APPLICATION,ADAPTER,B-CHANNEL,MDM MIN RX,MDM MAX RX,MDM MIN TX,MDM MAX TX,MDM RETRAINS,FAX MIN,FAX MAX,FAX PAGES,FAX ID,FAX FEATURES,ADAPTER NAME,ADAPTER SERIAL NUMBER,CONNECTION NR,TOTAL CONNECTION NR,L1XFrames,L1XBytes,L1XErrors,L1RFrames,L1RBytes,L1RErrors,L2XFrames,L2XBytes,L2XErrors,L2RFrames,L2RBytes,L2RErrors,CHARGES,DISC CAUSE,IP,PORT,IPM1,RPM1,LPM1,TRANSPORT,MEDIA1,CODEC1,CR,SDP/SESSION_ID,SDP/EMAIL_ADDR,SDP/PHONE_NR,SDP/MEDIA_NAME,SDP/CONN_INFO,ABANDONED CALL";
static const char* call_log_fields_description = "# FIELDS: DIRECTION, STATE(ONHOOK/OFFHOOK), TIME, REMOTE ADDR/SUBADDR,LOCAL ADDR/SUBADDR,PROTOCOL,APPLICATION,ADAPTER,B-CHANNEL,DISC CAUSE,IP,PORT,IPM1,RPM1,LPM1,TRANSPORT,MEDIA1,CODEC1,CR,SDP/SESSION_ID,SDP/EMAIL_ADDR,SDP/PHONE_NR,SDP/MEDIA_NAME,SDP/CONN_INFO,ABANDONED CALL";
/*
	Write log file header
	DIRECTION, CONNECTED AT , ONLINE TIME,
  REMOTE ADDRESS, LOCAL ADDRESS, PROTOCOL, APPLICATION,
  ADAPTER, B-CHANNEL, ..., DISC CAUSE
	*/
int diva_write_log_header (FILE** Log) {
	time_t t    = time(0);

	if (Log[0]) {
		if (fprintf (Log[0],
								 "# Diva Log daemon started at %s",
								 asctime (localtime (&t))) <= 0) {
			return (-1);
		}
		if (fprintf (Log[0], "%s\n", call_record_fields_description) <= 0) {
			return (-1);
		}

		fflush (Log[0]);
	}

	if (Log[1]) {
		if (fprintf (Log[1],
								 "# Diva Call Log daemon started at %s",
								 asctime (localtime (&t))) <= 0) {
			return (-1);
		}
		if (fprintf (Log[1], "%s\n", call_log_fields_description) <= 0) {
			return (-1);
		}

		fflush (Log[1]);
	}

	return (0);
}

int diva_write_log_suffix (FILE** Log) {
	time_t t = time(0);

	if (Log[0]) {
		if (fprintf (Log[0],
								 "# Diva Log daemon terminated at %s",
								 asctime (localtime (&t))) <= 0) {
			return (-1);
		}
		fflush (Log[0]);
	}
	if (Log[1]) {
		if (fprintf (Log[1],
								 "# Diva Call Log daemon terminated at %s",
								 asctime (localtime (&t))) <= 0) {
			return (-1);
		}
		fflush (Log[1]);
	}

	return (0);
}

/* big endian to dotted quad string */
char * ip2str(dword * ip_addr, char *ip_string) {
	char * tmp = ip_string;
	int i;
	byte *ip_byte = (byte *)ip_addr;
	sprintf(tmp,"%d.%d.%d.%d",*(ip_byte+3),*(ip_byte+2),*(ip_byte+1),*(ip_byte));
	return(ip_string);
}

/* big-/little endian conversion*/
word port2word(word * port) {
  return( (*(byte*)(port) + (*(byte*)(port+1)<<8)) );
}

static int print_log_record (char* buffer,
														 const char* timestamp,
														 time_t ours,
														 time_t min,
														 time_t sec,
														 diva_log_record_t* pState,
														 const char* adapter_name,
														 dword adapter_serial_number,
														 int max_length) {
	char fax_features[256];
	int ret;
        char ipStr[16], ipm1Str[16];

	fax_features[0] = 0;
	if (pState->fax_features & DIVA_FAX_FEATURE_FINE) {
		strcat (fax_features, "[Fine]");
	}
	if (pState->fax_features & DIVA_FAX_FEATURE_ECM) {
		strcat (fax_features, "[ECM]");
	} else if (pState->fax_features & DIVA_FAX_FEATURE_ECM_64) {
		strcat (fax_features, "[ECM 64]");
	}
	if (pState->fax_features & DIVA_FAX_FEATURE_T6) {
		strcat (fax_features, "[T6]");
	} else if (pState->fax_features & DIVA_FAX_FEATURE_2D) {
		strcat (fax_features, "[2D]");
	}
	if (pState->fax_features & DIVA_FAX_FEATURE_V34) {
		strcat (fax_features, "[V.34]");
	}
	if (pState->fax_features & DIVA_FAX_FEATURE_POLLING) {
		strcat (fax_features, "[Polling]");
	}

	ret = snprintf (buffer, max_length - 2,
					 "%s,%s,%lu:%lu:%lu,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,'%s','%s','%s',%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%04x,'%s',%d,'%s',%d,%d,'%s','%s','%s',%04x,'%s','%s','%s','%s','%s','%u'",
					 pState->line_state_name,
					 timestamp,
					 (unsigned long)ours, (unsigned long)min, (unsigned long)sec,
					 pState->remoteaddress,
					 pState->localaddress,
					 pState->protocol,
					 pState->application,
					 (int)pState->adapter_number,
					 (int)pState->line_number,
					 pState->min_rx_speed,
					 pState->max_rx_speed,
					 pState->min_tx_speed,
					 pState->max_tx_speed,
					 pState->retrains,
					 pState->min_fax_speed,
					 pState->max_fax_speed,
					 pState->pages,
					 pState->FaxID,
					 fax_features,
					 adapter_name,
					 adapter_serial_number,
					 pState->connection_nr,
					 total_connection_nr,
					 pState->b1_x_frames,
					 pState->b1_x_bytes,
					 pState->b1_x_errors,
					 pState->b1_r_frames,
					 pState->b1_r_bytes,
					 pState->b1_r_errors,
					 pState->b2_x_frames,
					 pState->b2_x_bytes,
					 pState->b2_x_errors,
					 pState->b2_r_frames,
					 pState->b2_r_bytes,
					 pState->b2_r_errors,
					 pState->last_charges,
					 (dword)pState->LastDisconnectCause,
					 /* sip fields */
					 ip2str(&pState->sip.ip, ipStr),
					 port2word(&pState->sip.port),
					 ip2str(&pState->sip.ipm1,ipm1Str),
					 port2word(&pState->sip.rpm1),
					 port2word(&pState->sip.lpm1),
					 pState->sip.transport,
					 pState->sip.media1,
					 pState->sip.codec1,
					 pState->sip.cr,
					 pState->sip.sdp.session_id,
					 pState->sip.sdp.email_addr,
					 pState->sip.sdp.phone_nr,
					 pState->sip.sdp.media_name,
					 pState->sip.sdp.conn_info,
					 pState->AbandonedCallin||pState->AbandonedCallout
					 );

	if (ret <= 0) {
		*buffer = 0;
		diva_syslog_record ("ERROR: Record lost due to too small buffer");
		return (-1);
	}

	buffer[max_length-1]=0;

	return (ret);
}

/*
	Write call log record after call completion
	*/
int diva_write_log_record (FILE** pLog,
													 diva_log_record_t* pState,
													 const char* adapter_name,
													 dword adapter_serial_number) {
	char buffer[4096];
	time_t t    = time(0);
	time_t dt   = (time_t)difftime (t, pState->off_hook_time);
	time_t ours = (dt/3600);
	time_t min  = (dt - ours*3600)/60;
	time_t sec  = dt - ours*3600 - min*60;
	char tmp[128], *p;

	FILE* Log = *pLog;

	strcpy (tmp, asctime (localtime (&pState->off_hook_time)));
	if ((p = strstr(tmp, "\n"))) {
		*p = 0;
	}
	if ((p = strstr(tmp, "\r"))) {
		*p = 0;
	}

	if (!ours && !min && !sec) {
		sec = 1;
	}

	pState->connection_nr++;
	total_connection_nr++;

	/*
		STATE NAME, CALL TIME , CALL DURATION, REMOTE ADDRESS, LOCAL ADDRESS,
		PROTOCOL, APPLICATION, ADAPTER, B_CHANNEL
		*/

	if (print_log_record (buffer,
										tmp, /* Timestamp */
										ours,
										min,
										sec,
										pState,
										adapter_name,
										adapter_serial_number,
										sizeof(buffer)) <= 0) {
		return (-1);
	}

	if (fprintf (Log, "%s\n", buffer) <= 0) {
		return (-1);
	}

	fflush (Log);

	if (++log_record_count >= DIVA_MAX_RECORDS_PER_FILE) {
		log_record_count = 0;
		if (diva_rotate_log_file (pLog)) {
			return (-1);
		}
	}

	return (0);
}

/*
	Write call state record after change in the call state
	*/
int diva_write_call_log_record (FILE* Log, diva_log_record_t* pState, int online) {
	time_t t    = time(0);
	char tmp[128], *p;

	strcpy (tmp, asctime (localtime (&t)));
	if ((p = strstr(tmp, "\n"))) {
		*p = 0;
	}
	if ((p = strstr(tmp, "\r"))) {
		*p = 0;
	}

	/*
		DIRECTION, STATE, TIME, REMOTE ADDR/SUBADDR, LOCAL ADDR/SUBADDR, PROTOCOL,
		APPLICATION, ADAPTER,B-CHANNEL,DISC CAUSE
		*/

	if (fprintf (Log,
					 "%s,%s,%s,%s,%s,%s,%s,%d,%d,%04x\n",
					 pState->line_state_name,
					 online ? "OFFHOOK" : "ONHOOK",
					 tmp,
					 pState->remoteaddress,
					 pState->localaddress,
					 pState->protocol,
					 pState->application,
					 (int)pState->adapter_number,
					 (int)pState->line_number,
					 (dword)pState->LastDisconnectCause ) <= 0) {
		return (-1);
	}

	fflush (Log);

	return (0);
}

/*
	Write volatile information about channel state
	*/
int diva_update_channel_state (FILE** Log,
															 diva_log_record_t* pState,
															 const char* adapter_name,
															 dword adapter_serial_number) {
	int ret;

	if (pState->line_state_name) {
		char buffer[4096];

		time_t t    = time(0);
		time_t dt   = (time_t)difftime (t, pState->off_hook_time);
		time_t ours = (dt/3600);
		time_t min  = (dt - ours*3600)/60;
		time_t sec  = dt - ours*3600 - min*60;
		char tmp[128], *p;

		strcpy (tmp, asctime (localtime (&pState->off_hook_time)));
		if ((p = strstr(tmp, "\n"))) {
			*p = 0;
		}
		if ((p = strstr(tmp, "\r"))) {
			*p = 0;
		}

		if (!ours && !min && !sec) {
			sec = 1;
		}

		if (print_log_record (buffer,
											tmp, /* Timestamp */
											ours,
											min,
											sec,
											pState,
											adapter_name,
											adapter_serial_number,
											sizeof(buffer)) <= 0) {
			return (-1);
		}

		ret = diva_write_channel_state (Log, (int)pState->adapter_number, (int)pState->line_number, &buffer[0]);
	} else {
		ret = diva_write_channel_state (Log, (int)pState->adapter_number, (int)pState->line_number, 0);
	}

	return (ret);
}

const char* diva_get_call_record_description (void) {
	return (call_record_fields_description);
}

static const char* interface_state_fields_description = "# FIELDS: LAYER1 STATE,LAYER2 STATE,INC CALLS,INC CONNECTED,INC USER BUSY,INC CALL REJECTED,INC WRONG NUMBER,INC INCOMPATIBLE DST,INC OUT OF ORDER,INC IGNORED,OUT CALLS,OUT CONNECTED,OUT USER BUSY,OUT NO ANSWER,OUT WRONG NUMBER,OUT CALL REJECTED,OUT OTHER FAILURES,MDM DISC NORMAL,MDM DISC UNSPECIFIED,MDM DISC BUSY TONE,MDM DISC CONGESTION,MDM DISC CARR WAIT,MDM DISC TRN TIMEOUT,MDM DISC INCOMPAT,MDM DISC FRAME REJ,MDM DISC V42BIS,FAX DISC NORMAL,FAX DISC NOT IDENT,FAX DISC NO RESPONSE,FAX DISC RETRIES,FAX DISC UNEXP MSG,FAX DISC NO POLLING,FAX DISC TRAINING,FAX DISC UNEXPECTED,FAX DISC APPLICATION,FAX DISC INCOMPAT,FAX DISC NO COMMAND,FAX DISC LONG MSG,FAX DISC SUPERVISOR,FAX DISC SUB SEP PWD,FAX DISC INVALID MSG,FAX DISC PAGE CODING,FAX DISC APP TIMEOUT,FAX DISC UNSPECIFIED,B1 X FRAMES,B1 X BYTES,B1 X ERRORS,B1 R FRAMES,B1 R BYTES,B1 R ERRORS,B2 X FRAMES,B2 X BYTES,B2 X ERRORS,B2 R FRAMES,B2 R BYTES,B2 R ERRORS,D1 X FRAMES,D1 X BYTES,D1 X ERRORS,D1 R FRAMES,D1 R BYTES,D1 R ERRORS,D2 X FRAMES,D2 X BYTES,D2 X ERRORS,D2 R FRAMES,D2 R BYTES,D2 R ERRORS, INITIAL TEMPERATURE, MIN TEMPERATURE, MAX TEMPERATURE, TEMPERATURE,DSPSTATE,FAX TX PAGES TOTAL,FAX TX PAGES RETRAIN,FAX TX PAGES REJECT,FAX RX PAGES TOTAL,FAX RX PAGES RETRAIN,FAX RX PAGES REJECT,OUT ABANDONED,IN ABANDONED,HARDWARE STATE,OUT NO CHAN AVAIL,IN NO CHAN AVAIL,L1 FRAMING ERRORS,L1 CRC4 ERRORS,L1 FRAME SLIPS";


const char* diva_get_interface_state_description (void) {
	return (interface_state_fields_description);
}

static const char* interface_info_fields_description = "# FIELDS: TYPE, CHANNELS, PROTOCOL, NT MODE, POINTTOPOINT,INTERFACENR,BOARDREVISION,SUBFUNCTION,SUBDEVICE,PROTOCOLBUILD,DSPCODEBUILD,ANALOGCHANNELS,PRI,PCIDMA,ADAPTERTYPE,LAW";

const char* diva_get_interface_info_description (void) {
	return (interface_info_fields_description);
}

static const char* registrar_fields_description = "# FIELDS: sipAddr, server, stateflag";
const char* diva_get_registrar_description (void) {
	return (registrar_fields_description);
}


int diva_update_ifc_state (FILE** Log, int adapter_number, int channel_number, diva_log_ifc_info_t* pInfo) {
	char buffer[4096*4];
	int ret, max_length = sizeof (buffer);

	if(pInfo != 0) {
		pInfo->statistics.fax.Disc_Unspecified =  pInfo->statistics.fax.Disc_Unspecified 
							+ pInfo->statistics.fax.Disc_No_Energy
							+ pInfo->statistics.fax.Disc_Peer_No_FAX
							+ pInfo->statistics.fax.Disc_Unexp_V21
							+ pInfo->statistics.fax.Disc_Mark_React
							+ pInfo->statistics.fax.Disc_Trn_Timeout
							+ pInfo->statistics.fax.Disc_Prim_CTS_ON
							+ pInfo->statistics.fax.Disc_Turnaroundp
							+ pInfo->statistics.fax.Disc_V8_Incomp
							+ pInfo->statistics.fax.Disc_Peer_ECM_Bug
							+ pInfo->statistics.fax.Disc_Below_Speed
							+ pInfo->statistics.fax.Disc_Overhead_Ex;
	}
	{
		ret = snprintf (buffer, max_length - 2,
"'%s','%s',%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,'%s',%u,%u,%u,%u,%u",
		pInfo != 0 ? pInfo->state[channel_number-1].Layer1 : "Down",
		pInfo != 0 ? pInfo->state[channel_number-1].Layer2 : "Idle",
		pInfo != 0 ? pInfo->statistics.inc.Calls : 0,
		pInfo != 0 ? pInfo->statistics.inc.Connected : 0,
		pInfo != 0 ? pInfo->statistics.inc.User_Busy : 0,
		pInfo != 0 ? pInfo->statistics.inc.Call_Rejected : 0,
		pInfo != 0 ? pInfo->statistics.inc.Wrong_Number : 0,
		pInfo != 0 ? pInfo->statistics.inc.Incompatible_Dst : 0,
		pInfo != 0 ? pInfo->statistics.inc.Out_of_Order : 0,
		pInfo != 0 ? pInfo->statistics.inc.Ignored : 0,
		pInfo != 0 ? pInfo->statistics.outg.Calls : 0,
		pInfo != 0 ? pInfo->statistics.outg.Connected : 0,
		pInfo != 0 ? pInfo->statistics.outg.User_Busy : 0,
		pInfo != 0 ? pInfo->statistics.outg.No_Answer : 0,
		pInfo != 0 ? pInfo->statistics.outg.Wrong_Number : 0,
		pInfo != 0 ? pInfo->statistics.outg.Call_Rejected : 0,
		pInfo != 0 ? pInfo->statistics.outg.Other_Failures : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Normal : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Unspecified : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Busy_Tone : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Congestion : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Carr_Wait : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Trn_Timeout : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Incompat : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_Frame_Rej : 0,
		pInfo != 0 ? pInfo->statistics.mdm.Disc_V42bis : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Normal : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Not_Ident : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_No_Response : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Retries : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Unexp_Msg : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_No_Polling : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Training : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Unexpected : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Application : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Incompat : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_No_Command : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Long_Msg : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Supervisor : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_SUB_SEP_PWD : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Invalid_Msg : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Page_Coding : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_App_Timeout : 0,
		pInfo != 0 ? pInfo->statistics.fax.Disc_Unspecified : 0,
		pInfo != 0 ? pInfo->statistics.b1.X_Frames : 0,
		pInfo != 0 ? pInfo->statistics.b1.X_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.b1.X_Errors : 0,
		pInfo != 0 ? pInfo->statistics.b1.R_Frames : 0,
		pInfo != 0 ? pInfo->statistics.b1.R_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.b1.R_Errors : 0,
		pInfo != 0 ? pInfo->statistics.b2.X_Frames : 0,
		pInfo != 0 ? pInfo->statistics.b2.X_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.b2.X_Errors : 0,
		pInfo != 0 ? pInfo->statistics.b2.R_Frames : 0,
		pInfo != 0 ? pInfo->statistics.b2.R_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.b2.R_Errors : 0,
		pInfo != 0 ? pInfo->statistics.d1.X_Frames : 0,
		pInfo != 0 ? pInfo->statistics.d1.X_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.d1.X_Errors : 0,
		pInfo != 0 ? pInfo->statistics.d1.R_Frames : 0,
		pInfo != 0 ? pInfo->statistics.d1.R_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.d1.R_Errors : 0,
		pInfo != 0 ? pInfo->statistics.d2.X_Frames : 0,
		pInfo != 0 ? pInfo->statistics.d2.X_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.d2.X_Errors : 0,
		pInfo != 0 ? pInfo->statistics.d2.R_Frames : 0,
		pInfo != 0 ? pInfo->statistics.d2.R_Bytes : 0,
		pInfo != 0 ? pInfo->statistics.d2.R_Errors : 0,
		pInfo != 0 ? (short)pInfo->state[0].InitialTemperature : 0,
		pInfo != 0 ? (short)pInfo->state[0].MinTemperature : 0,
		pInfo != 0 ? (short)pInfo->state[0].MaxTemperature : 0,
		pInfo != 0 ? (short)pInfo->state[0].Temperature : 0,
		pInfo != 0 ? pInfo->state[0].DSPState : 0,
		pInfo != 0 ? pInfo->statistics.fax.TX_Pages_Total : 0,
		pInfo != 0 ? pInfo->statistics.fax.TX_Pages_Retrain : 0,
		pInfo != 0 ? pInfo->statistics.fax.TX_Pages_Reject : 0,
		pInfo != 0 ? pInfo->statistics.fax.RX_Pages_Total : 0,
		pInfo != 0 ? pInfo->statistics.fax.RX_Pages_Retrain : 0,
		pInfo != 0 ? pInfo->statistics.fax.RX_Pages_Reject : 0,
		pInfo != 0 ? pInfo->statistics.outg.Abandoned : 0,
		pInfo != 0 ? pInfo->statistics.inc.Abandoned : 0,
		pInfo != 0 ? "Active" : "Inactive",
		pInfo != 0 ? pInfo->statistics.outg.No_Channel_Avail : 0,
		pInfo != 0 ? pInfo->statistics.inc.No_Channel_Avail : 0,
		pInfo != 0 ? pInfo->statistics.layer1.Framing_Errors : 0,
		pInfo != 0 ? pInfo->statistics.layer1.CRC4_Errors : 0,
		pInfo != 0 ? pInfo->statistics.layer1.Frame_Slips : 0);

		if (ret <= 0) {
			*buffer = 0;
			diva_syslog_record ("ERROR: Record lost due to too small buffer");
			return (-1);
		}

		buffer[max_length-1]=0;
	}

	return (diva_write_interface_state (Log, adapter_number, channel_number, &buffer[0]));
}

int diva_write_registrar_record (FILE** Log, diva_sip_info_t* sip_config) {
	char data[255];
	sprintf(data,"\"%s\",\"%s\",\"%s\"", sip_config->registrar.sipAddr, sip_config->registrar.server, sip_config->registrar.stateflag);
	diva_write_sip_registrar_state (Log, data);
	return (0);
}

static const char* resource_description = "# SEQUENCE OF STRUCTURES WITH FIELDS representing entries in Info\\Resource management interface directory: ShortResourceName, ResourceType, ResourceStandard, ResourceVisualName, ResourceCount";
const char* diva_get_resource_description (void) {
	return (resource_description);
}

int diva_update_adapter_resource (FILE** Log,
																	int adapter_number,
																	const struct _diva_resource_info_entry* resource) {
	char buffer[255*4*127];
	int length = 0, len;

	for (buffer[0] = 0; resource->resourceName[0] != 0; resource++) {
		len = snprintf (&buffer[length], sizeof(buffer)-length, "%s'%s','%c',%u,'%s',%u",
										length != 0 ? "," : "",
										resource->resourceName,
										resource->resourceType,
										resource->resourceStandard,
										resource->resourceVisualName,
										resource->resourceCount);
		if (len < 0 || len >= ((int)(sizeof(buffer)-length))) {
			return (-1);
		}
		length += len;
	}

	diva_write_adapter_resource (Log, adapter_number, buffer);

	return (0);
}

