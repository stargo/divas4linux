
/*
 *
  Copyright (c) Dialogic(R), 2009.
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

# ifndef ISDN___H
# define ISDN___H

#if !defined(UNIX)
# include "typedefs.h"	/* basic type definitions	*/
#endif

#if defined(UNIX)
#include "platform.h"
#include "dimaint.h"
#include "di_defs.h"
#	define  IDI_CALL_TYPE
#endif

# include "pc.h"	/* IDI message constants	*/
#	define DAD	CPN	/* named so in IDI spec	*/

# include "queue_if.h"	/* the queue handler interface	*/
# include "isdn_if.h"	/* the isdn - port interface	*/
# include "sys_if.h"	/* the system interface		*/
# include "log_if.h"	/* the logging interface	*/

/* if we stay resident once loaded we don't need the quiesce code	*/
# ifndef TRY_TO_QUIESCE
# define TRY_TO_QUIESCE 0
# endif

typedef struct ISDN_CHANNEL {

	struct ISDN_ADAPTER *Adapter;	/* backlink to adapter info	*/
	IDI_CALL	RequestFunc;	/* request function		*/
	word		Features;	/* hardware features of channel	*/
	byte		Id[20];		/* adapter_channel-port id	*/
#define			ISDN_CHANNEL_ID_LEN	3	/* -port id loc	*/
	byte		EscUid[11];	/* trace ID on the card set with sig req */

	void		*Port;		/* associated upper layer port	*/

	byte 		State;		/* overall channel state	*/
# define		C_DOWN		0	/* unusable		*/
# define		C_INIT		1	/* preparing for use	*/
# define		C_IDLE		2	/* free to be used	*/
# define		C_CALL_DIAL	3	/* outbound call pend.	*/
# define		C_CALL_OFFER	4	/* inbound call offered	*/
						/* await accept/answer	*/
# define		C_CALL_ACCEPT	5	/* inbound call accep-	*/
						/* ted, alert sent	*/
# define		C_CALL_ANSWER	6	/* inbound call answered*/
# define		C_CALL_UP	7	/* call connected	*/
# define		C_CALL_DISC_X	8	/* HANGUP sent for call	*/

# define		C_DATA_CONN_X	9	/* N_CONNECT sent	*/
# define		C_DATA_WAIT 	10	/* wait for modem conn	*/
# define		C_DATA_XFER 	11	/* data transfer phase	*/
# define		C_DATA_DISC_X	12	/* data connection reset*/

	byte		NetUp;		/* remember early N_CONNECT_IND	*/
	byte		MdmUp;		/* remember early N_UDATA_IND for modem */
	ISDN_CONN_INFO MdmInfo; /* connect info to remember last DCD in case of dalayer CALL_IND */

	byte		Age;		/* incremented each time the	*/
					/* channel is going idle	*/
	byte		DetProt;	/* detect HDLC based protocols	*/
	byte		ReqProt;	/* requested protocol		*/
	byte		Prot;		/* used data exchange protocol	*/
	byte 		Baud;		/* baud rate for rate adaption	*/
	byte		NetFC;		/* last Net.Rc was OK_FC	*/

	ENTITY		Sig;		/* Signalling entity		*/
	BUFFERS		SigXdesc;	/*	Xmit buffer descriptor.	*/

	ENTITY		Net;		/* Network layer entity		*/
	BUFFERS		NetXdesc;	/*	Xmit buffer descriptor.	*/
	BUFFERS		NetRdesc;	/*	Recv buffer descriptor	*/
					/* (NL indications and data are	*/
					/*  queued in common adapter q.)*/

	sys_TIMER	UpTimer;	/* timer to delay Up indication	*/

	MSG_QUEUE	Xqueue;		/* Queued Sig and Net requests.	*/
					/*	The 'Parms' portion of	*/
					/*	the request on head of 	*/
					/*	queue is used as xmit 	*/
					/*	buffer.			*/

	MSG_QUEUE	Squeue;		/* Queued Sig and Net requests.	*/
	dword			RcNotifyFlag;
	int       net_busy;
	int				sig_busy;
	int       delay_hangup;
	int				net_disc_ind_pending;
	int				net_rc;
	int 			in_assign;
	int				sig_hup_rnr;
	int				seen_hup_ind;
	int				sig_hup_sent;
	int				net_disc_ok;
	int				sig_remove_sent;
	int 			channel_assigned;
  int       rx_dma_ref_count;
  int       rx_dma_handle;
  dword     rx_dma_magic;

	sys_DPC		put_req_dpc;

 /*
		Parameters necessary for the B-channel protocol detections, that
    uses now different set of parameters and apply original parameters
    only after the real protocol was detected
    */
	ISDN_CONN_PARMS original_conn_parms;

	/*
		Variable that is indicating that transparent channel
		based protocol detection is used
		*/
	int	transparent_detect;

	diva_soft_hdlc_t hdlc_detector;
	soft_v110_state  v110_detector[DIVA_NR_OF_V110_DETECTORS];
	L2               piafs_detector[DIVA_NR_OF_PIAFS_DETECTORS];
	int							 last_detected_v110_rate;

	dword	transparent_detection_timeout;

	/*
		Points to the data located in the bit transparent detector
		if any data shoudl be sent to host after protocol was changed
		*/
	byte* detected_packet;
	word	detected_packet_length;

	byte  RequestedDisconnectCause;
	byte  ReceivedDisconnectCause;

} ISDN_CHANNEL;
#define IDI_DIALER_EVENT_DATA_RC	0x00000001
#define IDI_DIALER_EVENT_CONN_RC	0x00000002

/*
	Timeout for the bit transparent protocol detection procedure in minutes.
  If no protocol will be detected in this time then link will be disconnected.
	*/
#define DIVA_TRANSPARENT_DETECTION_TO_MIN 5

# define BASE_POINTER(type,name,addr) \
	((type *)((byte *)(addr) - (byte *)(&((type *)0)->name)))

typedef struct ISDN_MSG {
	ISDN_CHANNEL	*Channel;	/* source or destination	*/
	byte		Age;		/* to detect orphaned messages	*/
	byte		Kind;		/* kind of message		*/
# define		ISDN_SIG_REQ	   1	/* req from upper layer	*/
# define		ISDN_SIG_RC	   2	/* rc from lower layer	*/
# define		ISDN_SIG_IND	   3	/* ind from lower layer	*/
# define		ISDN_NET_CTL_REQ   4	/* req from upper layer	*/
# define		ISDN_NET_DATA_REQ  5	/* req from upper layer	*/
# define		ISDN_NET_RC	   6	/* rc from lower layer	*/
# define		ISDN_NET_FC	   7	/* flow control rc (FC)	*/
# define		ISDN_NET_CTL_IND   8	/* ind from lower layer	*/
# define		ISDN_NET_DATA_IND  9	/* ind from lower layer	*/
# define		ISDN_NET_DATA_GO  10	/* rc past flow control	*/
	byte		Type;		/* type of request/indication	*/
	byte		Rc;		/* return code for SIG_RC msg	*/
	byte		Parms[1]; /* please align to dword !	*/
} ISDN_MSG;

typedef struct ISDN_ADAPTER {

	byte		State;		/* adapter state		*/
#	define		A_DOWN	0
#	define		A_INIT	1
#	define		A_RUNS	2

	byte		Type;		/* hardware type		*/
	word		Features;	/* hardware features		*/
	IDI_CALL	RequestFunc;	/* request function		*/

	dword		numChannels;	/* no of channels		*/
	ISDN_CHANNEL	*Channels;	/* per B-channel information	*/
	ISDN_CHANNEL	*lastChannel;	/* == Channels + numChannels	*/

	sys_DPC		EventDpc;	/* event processing function	*/
	volatile int		EventLock;	/* protect event handling	*/
#	define		A_PROCESSING_EVENTS	0x01
#	define		A_SENDING_REQUESTS	0x02

	MSG_QUEUE	Events;		/* return codes, signalling and	*/
					/* data indications from lower	*/
					/* layer			*/

} ISDN_ADAPTER;

typedef struct ISDN_MACHINE {

	byte		State;		/* machine state		*/
#	define		M_DOWN	0
#	define		M_INIT	1
#	define		M_RUNS	2
#	define		M_SHUT	3

	byte		Listen;		/* listen for incoming calls	*/
	word		Features;	/* sum of all adapters features	*/
	word		Capabilities;	/* special capabilities		*/
	word		w_align;	/* fill up for long alignment	*/

	struct ISDN_INDICATIONS *Port;	/* set by isdnBind()		*/

# if TRY_TO_QUIESCE
	dword		QuiesceCount;	/* quiesce count		*/
	int		QuiescePending;	/* quiesce in progress		*/
	sys_TIMER	QuiesceTimer;	/* timer for quiescing		*/
	sys_EVENT	QuiesceEvent;	/* event for quiescing		*/
# endif /* TRY_TO_QUIESCE */

	dword		sizeMemory;	/* total size of memory needed	*/
	byte		*Memory;	/* base of allocated memory	*/
	void		*hMemory;	/* Win3x handle for 'Memory'	*/

	dword		numAdapters;	/* total no of adapters		*/
	ISDN_ADAPTER	*Adapters;	/* per adapter information	*/
	ISDN_ADAPTER	*lastAdapter;	/* Adapters + numAdapters	*/

	dword		numChannels;	/* total no of channels		*/
	ISDN_CHANNEL	*Channels;	/* per B-channel information	*/
	ISDN_CHANNEL	*lastChannel;	/* Channels + numChannels	*/

} ISDN_MACHINE;

/*
 * Codings for the "B-channel HW" octet in the CAI info element
 *
 * B1_HDLC 		64 KBit/s with HDLC framing
 * B1_XPARENT		64 KBit/s bit-transparent operation with byte framing
 *			from the network
 * B1_V110_a		V.110 rate adaption in asynchronous operation with
 *			start/stop framing
 * B1_V110_s		V.110 rate adaption in synchronous operation with
 *			HDLC framing
 * B1_T30		T.30 modem functionality for FAX Group 3 operation
 * B1_HDLC_inv		64 KBit/s inverted operation with HDLC framing
 * B1_HDLC_56		56 KBit/s rate adaptation with HDLC framing
 * B1_MODEM_a		Asyncronous modem 14400 Bit/s
 * B1_MODEM_s		Syncronous modem 14400 Bit/s
 * B1_EXT_0		External device 0 (for example DIVA-mobile modem)
 * B1_EXT_1		External device 1 (another source of data stream)
 */

# define B1_HDLC		0x05
# define B1_EXT_0		0x06
# define B1_EXT_1		0x07
# define B1_HDLC_56		0x08
# define B1_XPARENT		0x09
# define B1_V110_s		0x0c	/* x08 in message.c (CB says 0x0c) */
# define B1_V110_a		0x0d
# define B1_T30	   	   	0x10
# define B1_HDLC_inv		0x00	/* from message.c (CB says NOIY) */
# define B1_MODEM_a		0x11
# define B1_MODEM_s		0x12
# define B1_PIAFS     35

/*
 * Codings for the "layer 2 protocol" octet in the LLC info element
 * (unfortunately coding is different for incoming and outgoing calls)
 *
 * B2_X75SLP		X.75 SLP protocol (a.k.a. ISO 7776)
 * B2_X75SLPV42		X.75 SLP, V42bis protocol (a.k.a. ISO 7776 with V.42bis)
 * B2_XPARENT		Transparent Layer 2
 * B2_SDLC	     	SDLC protocol
 * B2_LAPD		LAPD according to Q.921 for D-channel X.25
 * B2_T30		T.30 modem for FAX Group 3
 * B2_PPP    		Point-to-Point Protocol
 * B2_XPARENT_NOERR	Transparent operation (ignoring framing errors
 *			from Layer 1)
 */

# define B2_NONE		0x00	/* external device, no protocol */
# define B2_X75SLP		0x01
# define B2_XPARENT_o		0x02
# define B2_XPARENT_i		0x03
# define B2_SDLC		0x04
# define B2_LAPD		0x06
# define B2_T30_o		0x02
# define B2_T30_i		0x03
# define B2_V120		0x08	/* brandnew feature */
# define B2_V42_o		0x0a
# define B2_V42_i		0x09
# define B2_PPP			0x00	/* unspecified */
# define B2_XPARENT_NOERR	0x00	/* unspecified */
# define B2_X75SLPV42		12
# define B2_PIAFS_OUT   29
# define B2_PIAFS_IN    29

/*
 * Codings for the "layer 3 protocol" octet in the LLC info element
 *
 * B3_XPARENT		Transparent Layer 3
 * B3_T90NL		T90NL
 * B3_ISO_8208		ISO 8208 (a.k.a. X.25 DTE-DTE)
 * B3_X25_DCE		X.25 DCE
 * B3_T30		T.30 modem for FAX Group 3 operation
 */

# define B3_NONE		0x00	/* external device, no protocol */
# define B3_XPARENT		0x04
# define B3_T90NL		0x03
# define B3_ISO_8208		0x02
# define B3_X25_DCE	   	0x02
# define B3_T30			0x06

/************************************************************************/
/*									*/
/* Types of information elements					*/
/*									*/
/* The first byte of an information element indicates the element type.	*/
/* Single-octet elements have the extension bit (bit 1) set to 1,	*/
/* the type is coded in bits 2-4 and the info is coded in bits 5-8.	*/
/* Multi-octet elements have the extension bit (bit 1) set to 0,	*/
/* the type is coded in bits 2-8, the second byte indicates the number	*/
/* of the following information octets.					*/
/*									*/
/************************************************************************/

#define INFO_ELEM_SINGLE_OCTET(_type)	(_type & 0x80)
#define INFO_ELEM_SINGLE_INFO(_type)	(_type & 0x0f)
#define INFO_ELEM_TYPE(_type)	       ((_type & 0x80) ? (_type & 0xf0) : _type)

#define	CODESET(_type) (_type & 0x07)

/*  C O D E S E T  0	*/

/* single-octet element types	*/

#define	INFO_ELEM_RESERVED			0x80
#define	INFO_ELEM_SHIFT				0x90	/**/
#define	INFO_ELEM_MORE_INFORMATION		0xA0
#define	INFO_ELEM_CONGESTION_LEVEL		0xB0

/* multi-octet element types	*/

#define	INFO_ELEM_BEARER_CAPABILITIES		0x06	/**/
#define	INFO_ELEM_CAUSE				0x08	/**/
#define	INFO_ELEM_CONNECTED_ADDRESS		0x0C
#define	INFO_ELEM_CALL_IDENTITY			0x10	/**/
#define	INFO_ELEM_CHANNEL_IDENTIFICATION	0x18	/**/
#define	INFO_ELEM_LOGICAL_LINK_IDENTIFIER	0x19	/**/
#define	INFO_ELEM_NETWORK_SPECIFIC_FACS		0x20
#define	INFO_ELEM_DISPLAY			0x28
#define	INFO_ELEM_KEYPAD_INFORMATION		0x2C	/**/
#define	INFO_ELEM_ORIGINATION_ADDRESS		0x6C	/**/
#define	INFO_ELEM_ORIGINATION_SUBADDRESS	0x6D	/**/
#define	INFO_ELEM_DESTINATION_ADDRESS		0x70
#define	INFO_ELEM_CALLED_PARTY_NUMBER		0x70	/**/
#define	INFO_ELEM_DESTINATION_SUBADDRESS	0x71	/**/
#define	INFO_ELEM_MESSAGE_TYPE			0x7A	/**/
#define	INFO_ELEM_LOW_LAYER_CAPABILITY		0x7C	/**/
#define	INFO_ELEM_HIGH_LAYER_CAPABILITY		0x7D	/**/
#define	INFO_ELEM_USER_USER_INFORMATION		0x7E	/**/

#define	INFO_ELEM_ESCAPE			0x7F	/*private*/

/* C O D E S E T  6	*/

#define	INFO_ELEM_SERVICE_INDICATOR		0x01	/**/
#define	INFO_ELEM_CHARGING_INFORMATION		0x02	/**/
#define	INFO_ELEM_DATE				0x03	/**/
#define	INFO_ELEM_FAC_SELECT			0x05
#define	INFO_ELEM_FAC_STATUS			0x06
#define	INFO_ELEM_CALLED_PARTY_STATE		0x07	/**/
#define	INFO_ELEM_ADDITIONAL_ATTRIBUTES		0x08

/************************************************************************/
/*								 	*/
/* Q931 network causes							*/
/*									*/
/* Causes which are defined in some other recommendation(s)		*/
/* but not in Q931 are tagged whith an empty comment			*/
/*									*/
/************************************************************************/

#define Q931_CAUSE_MASK	0x7F

/*	0x0_, 0x1_ normal events					*/

#define Q931_CAUSE_UnallocatedNumber_B_TinNr				0x01
#define Q931_CAUSE_NoRouteToSpecifiedTransitNetwork			0x02
#define Q931_CAUSE_NoRouteToDestination					0x03
#define Q931_CAUSE_SendSpecialInformationTone				0x04/**/
#define Q931_CAUSE_MisdialledTrunkPrefix				0x05/**/
#define Q931_CAUSE_ChannelUnacceptable					0x06
#define Q931_CAUSE_CallAwardedAndBeingDeliveredInAnEstablishedChannel	0x07
#define Q931_CAUSE_NormalCallClearing					0x10
#define Q931_CAUSE_UserBusy						0x11
#define Q931_CAUSE_NoUserResponding					0x12
#define Q931_CAUSE_NoAnswerFromUser_UserAlerted				0x13
#define Q931_CAUSE_CallRejected						0x15
#define Q931_CAUSE_NumberChanged					0x16
#define Q931_CAUSE_NonSelectedUserClearing				0x1A
#define Q931_CAUSE_DestinationOutOfOrder				0x1B
#define Q931_CAUSE_AddressIncomplete_InvalidNumberFormat		0x1C
#define Q931_CAUSE_FacilityRejected					0x1D
#define Q931_CAUSE_ResponseTo_STATUS_ENQUIRY				0x1E
#define Q931_CAUSE_Normal_Unspecified					0x1F

/*	0x2_ resource unavailable					*/

#define Q931_CAUSE_NoCircuit_ChannelAvailable				0x22
#define Q931_CAUSE_NetworkOutOfOrder					0x26
#define Q931_CAUSE_TemporaryFailure					0x29
#define Q931_CAUSE_SwitchingEquipmentCongestion				0x2A
#define Q931_CAUSE_AccessInformationDiscarded				0x2B
#define Q931_CAUSE_RequestedCircuit_ChannelNotAvailable			0x2C
#define Q931_CAUSE_ResourcesUnvailable_Unspecified			0x2F

/* 	0x3_ service or option not available				*/

#define Q931_CAUSE_QualityOfServiceUnavailable				0x31
#define Q931_CAUSE_RequestedFacilityNotSubscribed			0x32
#define Q931_CAUSE_OutgoingCallsBarredWhithinCUG			0x35/**/
#define Q931_CAUSE_IncomingCallsBarredWhithinCUG			0x37/**/
#define Q931_CAUSE_BearerCapabilityNotAuthorized			0x39
#define Q931_CAUSE_BearerCapabilityNotPresentlyAvailable		0x3A
#define Q931_CAUSE_InconsistentDesignatedOutgoingAccessInfo_SubscrClass 0x3E/**/
#define Q931_CAUSE_ServiceOrOptionNotAvailable_Unspecified		0x3F

/*	0x4_ service or option not implemented				*/

#define Q931_CAUSE_BearerCapabilityNotImplemented			0x41
#define Q931_CAUSE_ChannelTypeNotImplemented				0x42
#define Q931_CAUSE_RequestedFacilityNotImplemented			0x45
#define Q931_CAUSE_OnlyRestrictedDigitalInfoBearerCapabilityIsAvailable 0x46
#define Q931_CAUSE_ServiceOrOptionNotImplemented_Unspecified		0x4F

/*	0x5_ invalid message						*/

#define Q931_CAUSE_InvalidCallReferenceValue				0x51
#define Q931_CAUSE_IdentifiedChannelDoesNotExist			0x52
#define Q931_CAUSE_ASuspendedCallExistsButThisCallIdentityDoesNot	0x53
#define Q931_CAUSE_CallIdentityInUse					0x54
#define Q931_CAUSE_NoCallSuspended					0x55
#define Q931_CAUSE_CallHavingTheRequestedCallIdentityHasBeenCleared	0x56
#define Q931_CAUSE_UserNotMemberOfCUG					0x57/**/
#define Q931_CAUSE_IncompatibleDestination				0x58
#define Q931_CAUSE_NonExistentCUG					0x5A/**/
#define Q931_CAUSE_InvalidTransitNetworkSelection			0x5B
#define Q931_CAUSE_InvalidMessage_Unspecified				0x5F

/*	0x6_ protocol error (unknown message)				*/

#define Q931_CAUSE_MandatoryInformationElementIsMissing			0x60
#define Q931_CAUSE_MessageTypeNonExistentOrNotImplemented		0x61
#define Q931_CAUSE_MessageNotCompatibleWhithCallStateOrNonExOrNotImp	0x62
#define Q931_CAUSE_InformationElementNonExistentOrNotImplemented	0x63
#define Q931_CAUSE_InvalidInformationElementContents			0x64
#define Q931_CAUSE_MessageNotCompatibleWhithCallState			0x65
#define Q931_CAUSE_RecoveryOnTimerExpiry				0x66
#define Q931_CAUSE_ParameterNonExistentOrNotImplemented_PassedOn	0x67/**/
#define Q931_CAUSE_InconsistencyInData					0x6E/**/
#define Q931_CAUSE_ProtocolError_Unspecified				0x6F

/*	interworking							*/

#define Q931_CAUSE_InterWorking_Unspecified				0x7F

#define CORNET_CMD_CALLING_PARTY_NAME 0x0007


# endif /* ISDN___H */
