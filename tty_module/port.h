
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
 *
  File Revision :    2.1
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

# ifndef PORT___H
# define PORT___H

#if !defined(UNIX)
# include "typedefs.h"	/* basic type definitions	*/
#else
# include "platform.h"
#include "vcommemu.h"

#endif

# include "isdn_if.h"	/* the isdn - port interface	*/
# include "queue_if.h"	/* the queue handler interface	*/
# include "sys_if.h"	/* system funcs and structs	*/
#if defined(UNIX)
# include "rna_if.h"	/* the RNA handler interface	*/
# include "btx_if.h"	/* the BTX handler interface	*/
# include "fax_if.h"	/* the FAX handler interface	*/
#else
# include "rna_if.h"	/* the RNA handler interface	*/
# include "btx_if.h"	/* the BTX handler interface	*/
# include "fax_if.h"	/* the FAX handler interface	*/
#endif
# define LISTEN_SELECTIVELY	0	/* Listen only if ATS0=0 was set */

# define DiPort_DeviceName	"DiPort"
# define DiPort_ModemID		"Eicon ISDN Modem"

# define MAX_PORTS		255

# define MAX_DEVICE_NAME	9	/*  8 + \0	*/
# define MAX_PORT_NAME		16	/* 15 + \0	*/
# define MAX_ID_STRING		48	/* 47 + \0	*/
# define MAX_AT_COMMAND		64	/* 63 + \0	*/
# define MIN_TX_QUEUE		(4  * 1024)
# define MAX_TX_QUEUE		(64 * 1024)
# define MIN_RX_QUEUE		(4  * 1024)
# define MAX_RX_QUEUE		(64 * 1024)
# define MAX_V8_CI_LENGTH	8
# define MAX_V8_CF_VALUES	8
# define MAX_V8_PROT_VALUES	8
# define MAX_V8_CM_LENGTH	8
# define MAX_V8_JM_LENGTH	8

/*
 * An ISDN_PORT structure is allocated when a port is opened via
 * PortOpen() and remembered in an ISDN_PORT_DESC structure.
 * An array of ISDN_PORT_DESC structures is maintained in the
 * ISDN_PORT_DRIVER structure.
 */

/*	AT command processor profile embedded in AT_DATA		*/

typedef struct AT_CONFIG {

	byte	Id;		/* assigned profile Id			*/
	byte	Mode;		/* working mode				*/
# define	P_MODE_NONE   	  0	/* not defined			*/
# define	P_MODE_MIN   	  1	/* min settable mode		*/
# define	P_MODE_MODEM	  1	/* normal modem 		*/
# define	P_MODE_FAX 	  2	/* FAX modem			*/
# define	P_MODE_VOICE 	  3	/* voice modem			*/
# define	P_MODE_RNA	  4	/* RNA connections	 	*/
# define	P_MODE_BTX 	  5	/* BTX over ISDN		*/
# define	P_MODE_FRAME	  6	/* frame oriented I/O		*/
# define	P_MODE_MAX 	  6	/* max settable mode		*/
# define	P_MODE_HOOK 	  7	/* hooked UART serial driver	*/
	byte	Prot;		/* configured ISDN protocol (isdn_if.h)	*/
	byte	Service;	/* ISDN service for outgoing calls	*/
	byte	ServiceAdd;	/* additional service information	*/
	byte	PlanDest;	/* numbering plan for destination addr	*/
	byte	PlanOrig;	/* numbering plan for originating addr	*/
	byte	Baud;		/* baud rate for rate adaption		*/
	byte	Data;		/* switch to data mode after call setup	*/
	byte	Echo;		/* echo input data			*/
	byte	Quiet;		/* don't send responses 		*/
	byte	Verbose;	/* send verbose responses		*/
	byte	Progress;	/* kind of call progress messages	*/
	byte	DtrOpts;	/* data terminal ready options		*/
	byte	FlowOpts;	/* data terminal flow control options	*/
# define AT_FLOWOPTS_OFF          0
# define AT_FLOWOPTS_OLD_RTSCTS   1
# define AT_FLOWOPTS_OLD_XONXOFF  2
# define AT_FLOWOPTS_RTSCTS       3
# define AT_FLOWOPTS_XONXOFF      4
# define AT_FLOWOPTS_TRANSP       5
# define AT_FLOWOPTS_BOTH         6
# define AT_FLOWOPTS_MAX          6
	byte	ComOpts;	/* communications mode options		*/
	byte	ConFormat;	/* analog connect message format (\V)	*/
	byte	_1;			/* reserved for more AT commands		*/
	byte	_2;			/* 			=== ditto ===				*/
	byte	_3;			/* 			=== ditto ===				*/
	byte	RspDelay;	/* delay AT responses 'RspDelay' millis	*/
	byte	RnaFraming;	/* configured RNA framing method	*/
# define	P_FRAME_NONE	  0	/* don't touch data		*/
# define	P_FRAME_SYNC	  1	/* peer expects sync framing	*/
# define	P_FRAME_ASYNC	  2	/* peer expects async framing	*/
# define	P_FRAME_RAS_SYNC  3	/* peer expects sync framing	*/
# define	P_FRAME_RAS_ASYNC 4	/* peer expects async framing	*/
# define	P_FRAME_DET	  5	/* try to detect		*/
# define	P_FRAME_MAX	  5	/* max defined method		*/
	/* Bit-Transparent mode for protocol detection */
	byte		BitTDetect; /* BIT_T_DETECT_... */
	/* registers		*/
# define	AT_MAX_REGS 	  3
	byte	Regs[AT_MAX_REGS];

	diva_modem_options_t modem_options;
	byte	S27;
	byte	S51;
	byte  Presentation_and_Screening;

	byte  S253;
#define S254_ATH_IN_RING 0x01
#define S254_ENABLE_TIES 0x02
	byte  S254;

	byte  cid_setting;
  byte  port2adapter_binding;
  byte  at_escape_option;

	byte user_bc[64];
	byte user_llc[64];
	byte user_orig_subadr [ISDN_MAX_SUBADDR];

	int	o_line;
	int	i_line;

	int o_channel;
	int	i_channel;

} AT_CONFIG;

/*	AT command processor data embedded in ISDN_PORT			*/

typedef struct AT_DATA {

	AT_CONFIG f;		/* flags and registers from profile	*/

	word	curReg;		/* current register number		*/

	byte	IsdnTrace;	/* flags to control tracing		*/
	byte	RnaPatches;	/* flags to patch RNA idiosyncrasies	*/

	word	MaxFrame;	/* maximum transparent frame length	*/
	word	Wait;		/* wait time for receive notification	*/
	word	Block;		/* respect read blocksize		*/
	word	Defer;		/* defer xmit to this baud rate 	*/
	word	Split;		/* split xmit frames to this size	*/
	word	RwTimeout;	/* inactivity timeout			*/

	dword	Speed;		/* speed to indicate whith 'CONNECT'	*/
	byte	Data;		/* switch to data mode after call setup	*/
	byte	Prot;		/* protocol to set after call setup	*/

	byte	Acpt[ISDN_MAX_NUMBER];	/* accepted destination	*/
	byte	Orig[ISDN_MAX_NUMBER];	/* CiPN	*/
	byte	Orig_1[ISDN_MAX_NUMBER];	/* second CiPN */
	byte	PlanOrig_1;	/* second CiPN numbering plan */
	byte  PresentationAndScreening_1;	/* second CiPN presentation and screening */

	byte	Dest[ISDN_MAX_NUMBER];	/* last dialed address		*/
	byte	Ring[2*ISDN_MAX_NUMBER+4];	/* last ringing address		*/
	byte	RingName[(ISDN_MAX_DISPLAY_IE > ISDN_MAX_PARTY_NAME) ?
	                 ISDN_MAX_DISPLAY_IE : ISDN_MAX_PARTY_NAME];  /* ring calling party name      */
	byte	Last[MAX_AT_COMMAND];	/* last good command line	*/
	byte	Line[MAX_AT_COMMAND];	/* command line buffer		*/
	byte	sizeLine;		/* current # of bytes in buffer	*/
	byte	stateLine;		/* current line scan state	*/

	byte	ClassSupport;			/* modem classes support		*/
	dword	FaxTxModulationSupport;	/* supported TX modulations		*/
	dword	FaxRxModulationSupport;	/* supported RX modulations		*/
	byte	InactivityTimeout;
	byte	InactivityAction;
	byte	SerialPortRate;
# define P_SERIAL_PORT_RATE_AUTO		0
# define P_SERIAL_PORT_RATE_2400		1
# define P_SERIAL_PORT_RATE_4800		2
# define P_SERIAL_PORT_RATE_9600		4
# define P_SERIAL_PORT_RATE_19200		8
# define P_SERIAL_PORT_RATE_38400		10
# define P_SERIAL_PORT_RATE_57600		18
	byte	FlowControlOption;
# define P_FLOW_CONTROL_OPTION_NONE		0
# define P_FLOW_CONTROL_OPTION_XON_XOFF	1
# define P_FLOW_CONTROL_OPTION_CTS_RTS	2

	struct {		/* V.8 negotiation specific data	*/
		byte	OrgNegotiationMode;
# define P_V8_ORG_DISABLED				0
# define P_V8_ORG_DCE_CONTROLLED		1
# define P_V8_ORG_DTE_SEND_CI			2
# define P_V8_ORG_DTE_SEND_CNG			3
# define P_V8_ORG_DTE_SEND_CT			4
# define P_V8_ORG_DTE_SEND_SILENCE		5
# define P_V8_ORG_DCE_WITH_INDICATIONS	6
		byte	AnsNegotiationMode;
# define P_V8_ANS_DISABLED				0
# define P_V8_ANS_DCE_CONTROLLED		1
# define P_V8_ANS_DTE_SEND_ANSAM		2
# define P_V8_ANS_DTE_SEND_SILENCE		3
# define P_V8_ANS_DTE_SEND_ANS			4
# define P_V8_ANS_DCE_WITH_INDICATIONS	5
		byte	CIBuffer[MAX_V8_CI_LENGTH];
		byte	CILength;
		byte	V8bisNegotiationMode;
# define P_V8BIS_DISABLED				0
# define P_V8BIS_DCE_CONTROLLED			1
# define P_V8BIS_DTE_CONTROLLED			2
		word	AcceptCallFunctionTable[MAX_V8_CF_VALUES];
		byte	AcceptCallFunctionCount;
# define P_V8_CALL_FUNCTION_0			0
# define P_V8_CALL_FUNCTION_H324		1
# define P_V8_CALL_FUNCTION_V18			2
# define P_V8_CALL_FUNCTION_T101		3
# define P_V8_CALL_FUNCTION_T30_NORMAL	4
# define P_V8_CALL_FUNCTION_T30_POLLING	5
# define P_V8_CALL_FUNCTION_DATA		6
		word	AcceptProtocolTable[MAX_V8_PROT_VALUES];
		byte	AcceptProtocolCount;
# define P_V8_PROTOCOL_V42				1
		byte	CMBuffer[MAX_V8_CM_LENGTH];
		byte	CMLength;
		byte	JMBuffer[MAX_V8_JM_LENGTH];
		byte	JMLength;
	} V8;

	struct {		/* FAX CLASS 1 modem specific data	*/
		byte	State;
		byte	AdaptiveReceptionControl;
		byte	CarrierLossTimeout;
		byte	DoubleEscape;
		byte	V34Fax ;		/* TRUE if we are in V.34 FAX mode	*/
		word	V34FaxFlags;
		byte	V34FaxMinPrimaryRateDiv2400;
		byte	V34FaxMaxPrimaryRateDiv2400;
		byte	V34FaxPrefControlRateDiv1200;
		byte	V34FaxMinReceiveRateDiv2400;
		byte	V34FaxMaxReceiveRateDiv2400;
	} Fax1;

	struct {		/* FAX CLASS 2 modem specific data	*/
		byte	Phase;
		byte	Error;
		byte	ReverseBitOrder;
		byte  AllowIncPollingRequest;
		byte  PollingRequestActive;
		byte  RcvSEP_PWD_delivered;
		byte	ID[20+1];
		byte	DCC[CL2_PARAM_LEN];
		byte	DIS[CL2_PARAM_LEN];
		byte	DCS[CL2_PARAM_LEN];

		byte  page_header[127]; /* set by +FPH */

		word year;   /* set by +FTD */
		byte month;  /* set by +FTD */
		byte day;    /* set by +FTD */
		byte hour;   /* set by +FTD */
		byte minute; /* set by +FTD */
		byte second; /* set by +FTD */

		T30_INFO	T30Info;
	} Fax2;

} AT_DATA;

typedef struct NOTIFY_PROC {
	void	(*Proc)	(void *, void*, long, long);
	void* Data;
	dword	Trigger;
} NOTIFY_PROC;


typedef struct QUEUE_BUF {
	void	*hPages;	/* pageAlloc() handle for 'Buffer'	*/
	byte	*Buffer;	/* buffer address			*/
	dword	Size;		/* total buffer size			*/
} QUEUE_BUF;

typedef struct ISDN_PORT {

	/* The first part of this structure is shared between the isdn	*/
	/* specific code and the code which supports the serial port.	*/
	/* Attention: If anything before and including u.Ser is changed	*/
	/*	      the file serial.inc must be changed accordingly !	*/

	PortData    PortData;	/* shared by VCOMM and port driver	*/
	_DCB	    DCB;	/* the COMM device control block	*/

	NOTIFY_PROC EvNotify;	/* event notify procedure and data	*/
	NOTIFY_PROC RxNotify;	/* receive notify procedure and data	*/
	NOTIFY_PROC TxNotify;	/* transmit notify procedure and data	*/

	dword	   *pDetectedEvents;	/* event shadow			*/
	byte	   *pMsrShadow;		/* modem status shadow		*/
	dword	   *pLastReceiveTime;	/* receive time shadow		*/
	dword	    ModemStateEvents;	/* event bits according to cur- */
					/* rent modem status bits	*/

	union {
		struct {	/* frame mode stream input 		*/
		byte  Stream[512];
		}         Frm;
		RNA_DATA  Rna;	/* RNA control data			*/
		BTX_DATA  Btx;	/* BTX control data			*/
		FAX1_DATA Fax1;	/* FAX class 1 control data		*/
		FAX2_DATA Fax2;	/* FAX class 2 control data		*/
#if defined(UNIX)
	} port_u;
#else
	} u;
#endif

	byte	    Name[MAX_PORT_NAME];
	void	   *hPages;	/* pageAlloc() handle for this struct	*/
	dword	    VMId;	/* VM opening this port			*/
	dword	    hPort;	/* port handle from ACQUIRE_RESOURCE	*/

	dword	    LastRwTick;	/* tick count at last PortRead/Write	*/
	dword	    LastAlertTick;	/* time passed singe outgoing call was issued */
	dword	    LastUsed;	/* unique last used counter		*/
#if defined(WINNT) || defined(UNIX)
#if !defined(UNIX)
	LARGE_INTEGER LastDisc;	/* absolute time of last disconnect	*/
#endif
	ISDN_CONN_PARMS	CallbackParms;	/* parameters of last inbound call		*/
	byte			CallbackValid;	/* callback possible with CallbackParms	*/
	byte			CadValid;	/* Do CAD in connect by incomming call */
#endif

	volatile int	    NotifyLock;	/* to prevent notify recursions		*/

	QUEUE_BUF   RxBuffer;	/* dynamically allocated buffer		*/
	MSG_QUEUE   RxQueue;	/* frame receive queue			*/
	sys_TIMER   RxNotifyTimer;	/* receive notify timer		*/
	sys_TIMER   TxNotifyTimer;	/* receive notify timer		*/
	sys_DPC     RxRestartReadDpc;	/* receive notify timer		*/
	sys_DPC     TxRestartWriteDpc;	/* write notify dpc		*/
	byte	    RxNotifyAny;	/* notify if any byte in queue	*/
	byte	    RxRtsFlow;			/* set if VCOMM stops the ISDN via RTS  */
	byte	    RxXoffFlow;			/* set if VCOMM stops the ISDN via XOFF */

	byte	    HoldState;	/* remembered state at PortClose()	*/

	byte        FlowOpts;           /* data terminal flow control options   */
	byte	    TxFlow;	/* set if we got a stop from ISDN	*/
	byte	    TxOvershot;	/* set if queue count reached trigger	*/
	QUEUE_BUF   TxBuffer;	/* dynamically allocated buffer		*/
	MSG_QUEUE   TxQueue;	/* frame transmit queue			*/
	dword	    TxDeferTime;	/* time when to start next xmit	*/
	sys_TIMER   TxDeferTimer;	/* timer to start next xmit	*/
	byte	    TxCompile[ISDN_MAX_V120_FRAME];
					/* compiles single byte writes	*/
	dword	    sizeTxCompile;	/* number of bytes in buffer	*/
	sys_TIMER   TxCompileTimer;	/* timer to stop compiling	*/

	dword	    EscCount;		/* detected escape characters	*/
	dword	    EscTime;		/* time of last transmission	*/
	sys_TIMER   EscTimer;		/* escape timer			*/
	sys_TIMER   RspTimer;		/* timer to delay AT responses	*/
	MSG_QUEUE   RspQueue;		/* response queue		*/
	byte        RspBuffer[256];	/* response queue buffer	*/
	sys_TIMER   RingTimer;		/* timer for repeated ringing	*/
	sys_DPC			AutoAnswerDpc;	/* deferred automatic answer	*/

	dword	StateTimeout;	/* timeout for network op's		*/
# define	P_TO_TICK	  10	/* any 10 seconds		*/
# define	P_TO_NEVER		(0xFFFFFFFFL)
# define	P_TO_DIALING(now)	(now + 2)
# define	P_TO_OFFERED(now)	(now + 2)
# define	P_TO_ACCEPTED(now)	(now + 2)
# define	P_TO_ANSWERED(now)	(now + 1)
# define	P_TO_DISCONN(now)	(now + 1)

	byte	State;		/* current port state			*/
# define	P_IDLE		  0	/* unused			*/
# define	P_COMMAND	  1	/* processing AT commands	*/
# define	P_DIALING	  2	/* dialing			*/
# define	P_OFFERED	  3	/* offered			*/
# define	P_ANSWERED	  4	/* answered			*/
# define	P_CONNECTED	  5	/* connected			*/
# define	P_DATA		  6	/* transferring data		*/
# define	P_ESCAPE	  7	/* temporary command mode 	*/
# define	P_DISCONN	  8	/* disconnecting		*/

	byte	Direction;	/* direction of current connection	*/
# define	P_CONN_NONE	  0
# define	P_CONN_OUT	  1
# define	P_CONN_IN	  2

	byte	Mode;		/* current modem operating mode		*/

	byte    Class;		/* current modem class			*/
# define	P_CLASS_DATA			0
# define	P_CLASS_FAX1			1
# define	P_CLASS_FAX10			2
# define	P_CLASS_FAX2			3
# define	P_CLASS_FAX20			4
# define	P_CLASS_FAX21			5
# define	P_CLASS_VOICE			6

	word	Wait;		/* wait time for receive notification	*/
	word	Block;		/* respect read blocksize		*/
	word	Defer;		/* defer xmit to this baud rate		*/
	word	Split;		/* split xmit frames to this size	*/

	void	*Channel;	/* assigned ISDN channel		*/

	AT_DATA At;		/* current AT configuration		*/

	int rx_queued_in_event_queue;

	byte dbg_tmp[68];
	byte  data_Frame[ISDN_MAX_FRAME];

	int control_operation_mode;
	int control_response;

	int							last_conn_info_valid;
	ISDN_CONN_INFO	last_conn_info;

	ISDN_CONN_PARMS last_parms; /* saves the connection parms as specified by user */	
	
	byte	  Stream[MAX_RNA_STREAM_BUFFER];
} ISDN_PORT;
extern void diva_port_add_pending_data (void* C, void* hP, int count);

/* global driver data */

typedef struct ISDN_PORT_DESC {

	ISDN_PORT	*Port;
	byte		Name[MAX_PORT_NAME];
	dword		DeviceNode;
	dword		DeviceRefData;
	dword		ResourceHandle;
	VCOMM_ContentionHandler	ContentionHandler;

} ISDN_PORT_DESC;

typedef struct ISDN_PORT_DRIVER {

	ISDN_REQUESTS	*Isdn;		/* attached to ISDN machine	*/
	byte		Ready;		/* loaded and running */
	byte		Dynamic;	/* was dynamically loaded	*/
	byte		Profile;	/* default Open/AT&F profile	*/
	byte		RspDelay;	/* msec to delay AT responses	*/
	dword		Fixes;		/* special hacks		*/
# define		P_FIX_UNBIND_ON_LAST_CLOSE	0x80  /* Win9x  */
# define		P_FIX_DONT_ACQUIRE_PORT		0x40  /* Win9x  */
# define		P_FIX_DETACH_COM_ON_CLOSE	0x20  /* Win9x  */
# define		P_FIX_ACCEPT_ANALOG_CALLS	0x10
# define		P_FIX_DELAY_PORT_REUSE		0x08  /* WinNT	*/
# define		P_FIX_ROUND_ROBIN_RINGS		0x04
# define		P_FIX_SRV_SYNCPPP_ENABLED	0x02
# define 		P_FIX_SRV_CALLBACK_ENABLED  0x01  /* WinNT 	*/
	word		RingOn ;	/* ring on interval in msec	*/
	word		RingOff ;	/* ring off interval in msec	*/
	byte		DeviceName [MAX_DEVICE_NAME];
					/* used for VCOMM_Acquire_Port	*/
	byte		ModemID    [MAX_ID_STRING];
					/* used for ATI(dentification)	*/
	sys_TIMER	Watchdog;	/* global watch timer		*/
	dword		Ticks;		/* watch timer ticks (10 secs)	*/
	dword		Unique;		/* unique age counter		*/
#if defined(WINNT) || defined(UNIX)
	long		ReuseDelay;	/* seconds to delay port reuse	*/
	long		CallbackTimeout; /* max. seconds until callback */
#endif
	dword		curPorts;	/* currently open ports		*/
	ISDN_PORT_DESC	*Ports;		/* start of port table		*/
	ISDN_PORT_DESC	*lastPort;	/* last entry in port table	*/
	ISDN_PORT_DESC	*endPorts;	/* end of port table		*/
} ISDN_PORT_DRIVER;

int	portSetQueue	(ISDN_PORT *P,
			 byte** Buffer, dword *Size, QUEUE_BUF *B, byte which);
void	portInitQIn	(ISDN_PORT *P,
			 byte *Stream, dword sizeStream);
void	portInitQOut	(ISDN_PORT *P,
			 byte *Stream, dword sizeStream);
void	portRxEnqueue	(ISDN_PORT *P,
			 byte *Data, word sizeData, int frame);
void	portEvNotify	(ISDN_PORT *P);
void	portTxNotify	(ISDN_PORT *P);
void	portTxPurge	(ISDN_PORT *P);
int     PortWhatSize(ISDN_PORT *P, byte *Buffer, dword Wanted);
int	portXmit	(ISDN_PORT *P,
			 byte *Data, word sizeData, word FrameType);
void	portFaxXon	(ISDN_PORT *P);
void	portSetMsr	(ISDN_PORT *P, byte Mask, byte Value);
void	portUnTimeout	(ISDN_PORT *P);

struct _diva_mdm_speed_map;
typedef struct _diva_mdm_speed_map {
	int (*check_fn)(dword);
	word*  map;
	struct _diva_mdm_speed_map* lower;
} diva_mdm_speed_map_t;

# endif /* PORT___H */
