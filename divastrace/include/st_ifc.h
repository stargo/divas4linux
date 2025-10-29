
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

#ifndef __DIVA_EICON_TRACE_API__
#define __DIVA_EICON_TRACE_API__

#define DIVA_TRACE_LINE_TYPE_LEN 64
#define DIVA_TRACE_IE_LEN        64
#define DIVA_TRACE_INFO_LEN      128
#define DIVA_TRACE_FAX_PRMS_LEN  128

#define DIVA_MAX_ADAPTERS        128
#define DIVA_MAX_LINES					 32

#define DIVA_MAX_ADAPTER_NAME_LEN 128

#define DIVA_FAX_FEATURE_FINE				0x0001
#define DIVA_FAX_FEATURE_ECM				0x0002
#define DIVA_FAX_FEATURE_ECM_64			0x0004
#define DIVA_FAX_FEATURE_2D					0x0008
#define DIVA_FAX_FEATURE_T6					0x0010
#define DIVA_FAX_FEATURE_POLLING		0x0040
#define DIVA_FAX_FEATURE_V34				0x1000

#define DIVA_MODEM_FEATURE_V42			0x0002
#define DIVA_MODEM_FEATURE_V42_LAPM	0x0004

#define DIVA_ADAPTER_TYPE_BRI			0x0000
#define DIVA_ADAPTER_TYPE_PRI			0x0001
#define DIVA_ADAPTER_TYPE_ANALOG		0x0002
#define DIVA_ADAPTER_TYPE_HM			0x0003
#define DIVA_ADAPTER_TYPE_SOFTIP		0x0004
#define DIVA_ADAPTER_TYPE_SOFTIP_SRV		0x0005

struct _diva_resource_info_entry;

typedef struct _diva_trace_ie {
	byte length;
	byte data[DIVA_TRACE_IE_LEN];
} diva_trace_ie_t;

/*
	Structure used to represent "State\\BX\\Modem" directory
	to user.
	*/
typedef struct _diva_trace_modem_state {
	dword	ChannelNumber;

	dword	Event;

	dword	Norm;

	dword Options; /* Options received from Application */

	dword	TxSpeed;
	dword	RxSpeed;

	dword RoundtripMsec;

	dword SymbolRate;

	int		RxLeveldBm;
	int		EchoLeveldBm;

	dword	SNRdb;
	dword MAE;

	dword LocalRetrains;
	dword RemoteRetrains;
	dword LocalResyncs;
	dword RemoteResyncs;

	dword DiscReason;

} diva_trace_modem_state_t;

/*
	Representation of "State\\BX\\FAX" directory
	*/
typedef struct _diva_trace_fax_state {
	dword	ChannelNumber;
	dword Event;
	dword Page_Counter;
	dword Features;
	char Station_ID[DIVA_TRACE_FAX_PRMS_LEN];
	char Subaddress[DIVA_TRACE_FAX_PRMS_LEN];
	char Password[DIVA_TRACE_FAX_PRMS_LEN];
	dword Speed;
	dword Resolution;
	dword Paper_Width;
	dword Paper_Length;
	dword Scanline_Time;
	dword Disc_Reason;
	dword	dummy;
} diva_trace_fax_state_t;

/*
  Structure holding Adapter Config and Info.
  */
typedef struct _diva_ifc_config {
  dword	type;      /* PRI / BRI / Analog */
  dword	channels;  /* number of channels */
  dword	protocol;  /* as reported by Config\DChannel\Protocol */
  dword	NTmode;    /* NT/TE */
  dword StableL2;  /* Stable Layer 2 */
  dword	TEI;       /* SPID-1\TEI to detect PointToPoint vs. PointToMultipoint */
  dword	cardtype;  /* as reported by Config\CardType */
  dword	InterfaceNr;  /* for Softip-Adapter*/
  dword BoardRevision;
  dword SubFunction;
  dword SubDevice;
  char ProtocolBuild[DIVA_TRACE_INFO_LEN];
  char DSPCodeBuild[DIVA_TRACE_INFO_LEN];
  dword	alarm_red;
  dword	alarm_yellow;
  dword	alarm_blue;
  dword PRI;
  dword PCIDMA;
  dword analogChannels;
  char  law;
  dword serial_nr;
} diva_ifc_config_t;


/*
	Structure used to represent Interface State in the abstract
	and interface/D-channel protocol independent form.
	*/
typedef struct _diva_trace_interface_state {
    diva_ifc_config_t * pConfig;
	char Layer1[DIVA_TRACE_LINE_TYPE_LEN];
	char Layer2[DIVA_TRACE_LINE_TYPE_LEN];
	dword InitialTemperature;
	dword MaxTemperature;
	dword MinTemperature;
	dword Temperature;
	dword DSPState;
	const struct _diva_resource_info_entry* Resource;
} diva_trace_interface_state_t;

typedef struct _diva_incoming_call_statistics {
	dword Calls;
	dword Connected;
	dword User_Busy;
	dword Call_Rejected;
	dword Wrong_Number;
	dword Incompatible_Dst;
	dword Out_of_Order;
	dword Ignored;
	dword Abandoned;
	dword No_Channel_Avail;
} diva_incoming_call_statistics_t;

typedef struct _diva_outgoing_call_statistics {
	dword Calls;
	dword Connected;
	dword User_Busy;
	dword No_Answer;
	dword Wrong_Number;
	dword Call_Rejected;
	dword Other_Failures;
	dword Abandoned;
	dword No_Channel_Avail;
} diva_outgoing_call_statistics_t;

typedef struct _diva_layer1_statistics {
	dword Framing_Errors;
	dword CRC4_Errors;
	dword Frame_Slips;
} diva_layer1_statistics_t;


typedef struct _diva_modem_call_statistics {
	dword Disc_Normal;
	dword Disc_Unspecified;
	dword Disc_Busy_Tone;
	dword Disc_Congestion;
	dword Disc_Carr_Wait;
	dword Disc_Trn_Timeout;
	dword Disc_Incompat;
	dword Disc_Frame_Rej;
	dword Disc_V42bis;
} diva_modem_call_statistics_t;

typedef struct _diva_fax_call_statistics {
	dword Disc_Normal;
	dword Disc_No_Energy;
	dword Disc_Peer_No_FAX;
	dword Disc_Not_Ident;
	dword Disc_No_Response;
	dword Disc_Retries;
	dword Disc_Unexp_Msg;
	dword Disc_No_Polling;
	dword Disc_Training;
	dword Disc_Unexpected;
	dword Disc_Application;
	dword Disc_Incompat;
	dword Disc_No_Command;
	dword Disc_Long_Msg;
	dword Disc_Supervisor;
	dword Disc_SUB_SEP_PWD;
	dword Disc_Invalid_Msg;
	dword Disc_Page_Coding;
	dword Disc_App_Timeout;
	dword Disc_Unexp_V21;
	dword Disc_Mark_React;
	dword Disc_Trn_Timeout;
	dword Disc_Prim_CTS_ON;
	dword Disc_Turnaroundp;
	dword Disc_V8_Incomp;
	dword Disc_Peer_ECM_Bug;
	dword Disc_Below_Speed;
	dword Disc_Overhead_Ex;
	dword Disc_Unspecified;
	dword TX_Pages_Total;
	dword TX_Pages_Retrain;
	dword TX_Pages_Reject;
	dword RX_Pages_Total;
	dword RX_Pages_Retrain;
	dword RX_Pages_Reject;
} diva_fax_call_statistics_t;

typedef struct _diva_prot_statistics {
	dword X_Frames;
	dword X_Bytes;
	dword X_Errors;
	dword R_Frames;
	dword R_Bytes;
	dword R_Errors;
} diva_prot_statistics_t;

typedef struct _diva_ifc_statistics {
	diva_incoming_call_statistics_t	inc;
	diva_outgoing_call_statistics_t outg;
	diva_layer1_statistics_t        layer1;
	diva_modem_call_statistics_t    mdm;
	diva_fax_call_statistics_t      fax;
	diva_prot_statistics_t          b1;
	diva_prot_statistics_t          b2;
	diva_prot_statistics_t          d1;
	diva_prot_statistics_t          d2;
} diva_ifc_statistics_t;

/*
	Structure used to represent "State\\BX" directory
	to user.
	*/
typedef struct _diva_trace_line_state {
	dword	ChannelNumber;

	char Line[DIVA_TRACE_LINE_TYPE_LEN];

	char Framing[DIVA_TRACE_LINE_TYPE_LEN];

	char Layer2[DIVA_TRACE_LINE_TYPE_LEN];
	char Layer3[DIVA_TRACE_LINE_TYPE_LEN];

	char RemoteAddress[DIVA_TRACE_LINE_TYPE_LEN];
	char RemoteSubAddress[DIVA_TRACE_LINE_TYPE_LEN];

	char LocalAddress[DIVA_TRACE_LINE_TYPE_LEN];
	char LocalSubAddress[DIVA_TRACE_LINE_TYPE_LEN];

	diva_trace_ie_t	call_BC;
	diva_trace_ie_t	call_HLC;
	diva_trace_ie_t	call_LLC;

	dword Charges;

	dword CallReference;

	dword LastDisconnectCause;
	dword AbandonedCallin;
	dword AbandonedCallout;

	char UserID[DIVA_TRACE_LINE_TYPE_LEN];

	diva_trace_modem_state_t modem;
	diva_trace_fax_state_t   fax;

	diva_trace_interface_state_t* pInterface;

	diva_ifc_statistics_t*				pInterfaceStat;

	diva_prot_statistics_t				L1_Stats;
	diva_prot_statistics_t				L2_Stats;
} diva_trace_line_state_t;

#define DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE				('l')
#define DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE			('m')
#define DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE				('f')
#define DIVA_SUPER_TRACE_INTERFACE_CHANGE					('i')
#define DIVA_SUPER_TRACE_TEMPERATURE_CHANGE       ('T')
#define DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE				('s')
#define DIVA_SUPER_TRACE_NOTIFY_MDM_STAT_CHANGE   ('M')
#define DIVA_SUPER_TRACE_NOTIFY_FAX_STAT_CHANGE   ('F')
#define DIVA_SUPER_TRACE_NOTIFY_SIP_CHANGE				('S')
#define DIVA_SUPER_TRACE_NOTIFY_SIP_REGISTRAR  ('r')
#define DIVA_SUPER_TRACE_NOTIFY_RESOURCE_UPDATE   ('R')

struct _diva_strace_library_interface;
typedef void (*diva_trace_channel_state_change_proc_t)(void* user_context,
							struct _diva_strace_library_interface* hLib,
							int Adapter,
							diva_trace_line_state_t* channel, int notify_subject);
typedef void (*diva_trace_channel_trace_proc_t)(void* user_context,
							struct _diva_strace_library_interface* hLib,
							int Adapter, void* xlog_buffer);
typedef void (*diva_trace_error_proc_t)(void* user_context,
							struct _diva_strace_library_interface* hLib,
							int Adapter,
							int error, const char* file, int line);

/*
	This structure creates interface from user to library
	*/
typedef struct _diva_trace_library_user_interface {
	void*																		user_context;
	diva_trace_channel_state_change_proc_t	notify_proc;
	diva_trace_channel_trace_proc_t					trace_proc;
	diva_trace_error_proc_t									error_notify_proc;
} diva_trace_library_user_interface_t;

/*
	Interface from Library to User
	*/
typedef int   (*DivaSTraceLibraryFinit_proc_t)(void* hLib);
typedef int   (*DivaSTraceMessageInput_proc_t)(void* hLib);
typedef void*	(*DivaSTraceGetHandle_proc_t)(void* hLib);

/*
	Turn Audio Tap trace on/off
	Channel should be in the range 1 ... Number of Channels
	*/
typedef int (*DivaSTraceSetAudioTap_proc_t)(void* hLib, int Channel, int on);

/*
	Turn B-channel trace on/off
	Channel should be in the range 1 ... Number of Channels
	*/
typedef int (*DivaSTraceSetBChannel_proc_t)(void* hLib, int Channel, int on);

/*
	Turn	D-channel (Layer1/Layer2/Layer3) trace on/off
		Layer1 - All D-channel frames received/sent over the interface
						 inclusive Layer 2 headers, Layer 2 frames and TEI management frames
		Layer2 - Events from LAPD protocol instance with SAPI of signalling protocol
		Layer3 - All D-channel frames addressed to assigned to the card TEI and
						 SAPI of signalling protocol, and signalling protocol events.
	*/
typedef int (*DivaSTraceSetDChannel_proc_t)(void* hLib, int on);

/*
	Get overall card statistics
	*/
typedef int (*DivaSTraceGetOutgoingCallStatistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetIncomingCallStatistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetLayer1Statistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetModemStatistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetFaxStatistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetBLayer1Statistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetBLayer2Statistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetDLayer1Statistics_proc_t)(void* hLib);
typedef int (*DivaSTraceGetDLayer2Statistics_proc_t)(void* hLib);

/*
	Get channel statistics
	*/
typedef int (*DivaSTraceGetBLayer1ChannelStatistics_proc_t)(void* hLib, int channel);
typedef int (*DivaSTraceGetBLayer2ChannelStatistics_proc_t)(void* hLib, int channel);

/*
	Call control
	*/
typedef int (*DivaSTraceClearCall_proc_t)(void* hLib, int Channel);

/*
	Interface Identify
	*/
typedef int (*DivaSTraceInterfaceIdentify_proc_t)(void* hLib, int on);

/*
  Disable trace messages
  */
typedef int (*DivaSTraceInterfaceDisableTrace_proc_t)(void* hLib);

typedef int (*DivaSTraceInterfaceGetNrChannels_proc_t)(void* hLib);

typedef enum {
	DivaSTraceFeatureGlobalStatisticsPolling = 1
} diva_strace_ifc_features_t;

typedef int (*DivaSTraceSetInterfaceFeatures_proc_t)(void* hLib, diva_strace_ifc_features_t features);

typedef int (*DivaSTraceGetResourceInfo_proc_t)(void* hLib);

typedef int (*DivaSTraceUpdateTime_proc_t)(void* hLib);


typedef struct _diva_strace_library_interface {
	void* hLib;
	DivaSTraceLibraryFinit_proc_t DivaSTraceLibraryFinit;
	DivaSTraceMessageInput_proc_t DivaSTraceMessageInput;
	DivaSTraceGetHandle_proc_t    DivaSTraceGetHandle;
	DivaSTraceSetAudioTap_proc_t  DivaSTraceSetAudioTap;
	DivaSTraceSetBChannel_proc_t  DivaSTraceSetBChannel;
	DivaSTraceSetDChannel_proc_t  DivaSTraceSetDChannel;
	DivaSTraceSetDChannel_proc_t  DivaSTraceSetInfo;
	DivaSTraceGetOutgoingCallStatistics_proc_t DivaSTraceGetOutgoingCallStatistics;
	DivaSTraceGetIncomingCallStatistics_proc_t DivaSTraceGetIncomingCallStatistics;
	DivaSTraceGetLayer1Statistics_proc_t  DivaSTraceGetLayer1Statistics;
	DivaSTraceGetModemStatistics_proc_t   DivaSTraceGetModemStatistics;
	DivaSTraceGetFaxStatistics_proc_t     DivaSTraceGetFaxStatistics;
	DivaSTraceGetBLayer1Statistics_proc_t DivaSTraceGetBLayer1Statistics;
	DivaSTraceGetBLayer2Statistics_proc_t DivaSTraceGetBLayer2Statistics;
	DivaSTraceGetDLayer1Statistics_proc_t DivaSTraceGetDLayer1Statistics;
	DivaSTraceGetDLayer2Statistics_proc_t DivaSTraceGetDLayer2Statistics;
	DivaSTraceClearCall_proc_t            DivaSTraceClearCall;
	DivaSTraceInterfaceIdentify_proc_t    DivaSTraceInterfaceIdentify;
	DivaSTraceGetBLayer1ChannelStatistics_proc_t DivaSTraceGetBLayer1ChannelStatistics;
	DivaSTraceGetBLayer2ChannelStatistics_proc_t DivaSTraceGetBLayer2ChannelStatistics;
	DivaSTraceInterfaceDisableTrace_proc_t       DivaSTraceInterfaceDisableTrace;
	DivaSTraceInterfaceGetNrChannels_proc_t      DivaSTraceInterfaceGetNrChannels;

	/*
		Fields filled by 'DivaSTraceLibraryCreateInstance'
    Application is free to change the context of these fields
    for own purposes (that can be necessary to decorate
    the name or the serial number).
    */
	unsigned int                  adapter_serial_number;
  char                          adapter_name[DIVA_MAX_ADAPTER_NAME_LEN];
  const char *                  adapter_display_name;

	DivaSTraceSetInterfaceFeatures_proc_t DivaSTraceSetInterfaceFeatures;
	DivaSTraceGetResourceInfo_proc_t      DivaSTraceGetResourceInfo;
  DivaSTraceUpdateTime_proc_t           DivaSTraceUpdateTime;
} diva_strace_library_interface_t;

/* ********************************
  Here goes the SIP service specific stuff
  ********************************* */

/*
  Structure holding per-Adapter Config and Info.
  */
typedef struct _diva_sip_ifc_info {
  dword	adapter_nr; /* logical adapter number of associated protocol code */
  dword	serial_nr;  /* serial number */
  dword	channels;  /* number of channels */
  char name[32];  /* adapter name */
} diva_sip_ifc_info_t;

/*
  Structure holding general service config.
  */
typedef struct _diva_sip_info {
  char license_device[DIVA_TRACE_INFO_LEN]; /* license device */
  char license_host[DIVA_TRACE_INFO_LEN]; /* license host */
  char dnmapfile[DIVA_TRACE_INFO_LEN];
  int callinstrategy;
  struct {
    char sipAddr[DIVA_TRACE_INFO_LEN];
    char server[DIVA_TRACE_INFO_LEN];
    char stateflag[DIVA_TRACE_INFO_LEN];
  } registrar;
} diva_sip_info_t;

/*
  Structure holding channel info.
  */
typedef struct _diva_sip_channel_info {
	byte interface_nr;
	byte channel_nr;
  dword ip;
  word	port;
  dword	ipm1;
  word	rpm1;
  word	lpm1;
  char transport[DIVA_TRACE_INFO_LEN];
  char media1[DIVA_TRACE_INFO_LEN];
  char codec1[DIVA_TRACE_INFO_LEN];
  word	cr; /* call reference */
  struct {
	  char session_id[DIVA_TRACE_INFO_LEN];
	  char email_addr[DIVA_TRACE_INFO_LEN];
	  char phone_nr[DIVA_TRACE_INFO_LEN];
	  char media_name[DIVA_TRACE_INFO_LEN];
	  char conn_info[DIVA_TRACE_INFO_LEN];
  } sdp;
} diva_sip_channel_info_t;

/*
	Resource info entry
	*/
typedef struct _diva_resource_info_entry {
	char  resourceName[128];
	char  resourceType;
	dword resourceStandard;
	char  resourceVisualName[128];
	dword resourceCount;
} diva_resource_info_entry_t;

typedef int (*DivaSTraceSipGetCallData_proc_t)(void* hSipLib, void* hAdapterLib, int Adapter, int Channel, dword CallReference);
typedef int (*DivaSTraceSipGetHeartBeat_proc_t)(void* hSipLib);
typedef int (*DivaSTraceSipGetRegistrar_proc_t)(void* hSipLib);

typedef struct _diva_strace_library_sip_interface {
	void* hLib;
	DivaSTraceLibraryFinit_proc_t DivaSTraceSipLibraryFinit;
	DivaSTraceMessageInput_proc_t DivaSTraceSipMessageInput;
	DivaSTraceGetHandle_proc_t    DivaSTraceSipGetHandle;
	DivaSTraceInterfaceGetNrChannels_proc_t DivaSTraceSipInterfaceGetNrChannels;
	DivaSTraceSipGetCallData_proc_t DivaSTraceSipGetCallData;
	DivaSTraceSipGetHeartBeat_proc_t DivaSTraceSipGetHeartBeat;
	DivaSTraceSipGetRegistrar_proc_t DivaSTraceSipGetRegistrar;
} diva_strace_library_sip_interface_t;

/********
  end sip section
  *********/

#if defined(__cplusplus)
extern "C" {
#endif

/*
	Create and return Library interface
	*/
diva_strace_library_interface_t* DivaSTraceLibraryCreateInstance (int Adapter,
													const diva_trace_library_user_interface_t* user_proc);
diva_strace_library_sip_interface_t* DivaSTraceLibrarySipCreateInstance (const diva_trace_library_user_interface_t* user_proc);

/*
	Return list of descriptors
	*/
const dword* DivaSTraceLibraryGetDescriptorList (void);
#if defined(__cplusplus)
}
#endif

#endif
