
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

int divas_tty_debug_sig = 1;
int divas_tty_debug_net = 1;
int divas_tty_debug_net_data = 0;

/*
	After N_DISC wait with detach of the
	TTY interface until HANGUP was received.
	This allows to delivery right disconnect cause to the user
	*/
int divas_tty_wait_sig_disc  = 0;

extern void diva_port_add_pending_data (void* C, void* hP, int count);
char* get_port_name (void* hP);
#define port_name(__x__)  ((C) ? get_port_name(C->Port) : "?")

#define TRANSPARENT_DETECT_OFF        0x00
#define TRANSPARENT_DETECT_ON         0x01
#define TRANSPARENT_DETECT_INFO_VALID 0x02

/*****************************************************************************
*
*	COPYRIGHT NOTICE:
*
*	Copyright (C) Eicon Technology Corporation, 1999.
*	This file contains Proprietary Information of
*	Eicon Technology Corporation, and should be treated
*	as confidential.
*
* 	This source file is supplied for the exclusive use with Eicon 
*   Technology Corporation's range of DIVA Server Adapters.
*
* $Archive:   /u/master/esc_source/homer/drv.ser/isdn.c_v  $
* $Workfile:   isdn.c  $
* $Revision:   1.3  $
* $Date:   07 Oct 1999 16:09:14  $
* $Modtime:   07 Oct 1999 15:51:24  $
* $Author:   dhendric  $
*****************************************************************************/

#if defined(LINUX)
# include <linux/version.h>
# include <linux/types.h>
# include <linux/kernel.h>
# include <linux/fs.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)
# include <linux/malloc.h>
#else
# include <linux/slab.h>
#endif

# include <linux/tty.h>
# include <linux/tty_flip.h>
# include <linux/major.h>
# include <linux/mm.h>
# include <asm/string.h>
# include <asm/uaccess.h>
# include <asm/errno.h>
# include <asm/segment.h>

#undef N_DATA

#endif            

#include "platform.h"
#include "hdlc.h"
#include "v110.h"
#include "piafs.h"
#include "isdn.h"
#include "sys_if.h"
#include "ppp_if.h" 
#include "xid.h"
#include "mdm_msg.h"
#include "divasync.h"
#include "debuglib.h"
#include "fax_if.h"

#ifndef HANDLE_EARLY_CONNECT
#define HANDLE_EARLY_CONNECT	1
#endif
#ifndef D_PROTOCOL_DETECTION
#define D_PROTOCOL_DETECTION	1
#endif
#ifndef B_PROTOCOL_DETECTION
#define B_PROTOCOL_DETECTION	1
#define DET_PROT_OFF		0
#define DET_PROT_FIRST_FRAME	1
#define DET_PROT_REASSIGN	2
#endif

#define APPLY_AGE_CHECK	0
#define IDI_does_not_check_XNum_and_Plength	1

static	void	isdnPutReq	(ISDN_CHANNEL *C);
static	void	isdnPutReqDpc	(ISDN_CHANNEL *C);
static	int	isdnSigEnq	(ISDN_CHANNEL *C, byte Type,
				 byte *Parms, word sizeParms);
static	int	isdnNetCtlEnq	(ISDN_CHANNEL *C, byte Type,
				 byte *Parms, word sizeParms);
static int channel_wait_n_disc_ind (ISDN_CHANNEL *C);

#if defined(UNIX)
ISDN_MACHINE	Machine = { M_DOWN /* State */ };
#else
static ISDN_MACHINE	Machine = { M_DOWN /* State */ };
#endif

#define WIDE_ID(__x__) ((__x__.Id == 0xff) ? \
 						((word)((((word)(__x__.No)) << 8) | (word)(__x__.reserved2))) : ((word)(__x__.Id)))

static const char* sig_name (byte code, int is_req);
static const char* net_name (byte req);
extern void diva_tty_os_sleep (dword mSec);
static int diva_tty_shutdown = 0;
static int diva_get_dma_descriptor   (ISDN_CHANNEL* C, dword* dma_magic);
static void diva_free_dma_descriptor (ISDN_CHANNEL* C, int nr);
static int get_fixed_protocol (const char* addr);
extern byte ports_unavailable_cause;
extern int global_options;
extern int diva_mtpx_adapter_detected;
extern int diva_wide_id_detected;


/*
		PIAFS SPECIFIC DEFINITIONS
	*/
extern byte piafs_32K_BC[];
extern byte piafs_64K_BC[];
extern byte piafs_64K_FIX_OrigSa[];
extern byte piafs_64K_VAR_OrigSa[];
extern byte piafs_32K_DLC[];
extern byte piafs_32K_DLC_china[];
extern byte piafs_64K_FIX_DLC[];
extern byte piafs_64K_FIX_DLC_china[];
extern byte piafs_64K_VAR_DLC[];
extern byte piafs_64K_VAR_DLC_china[];

extern unsigned long global_mode;


typedef struct ISDN_FIND_INFO {
	byte Codeset;	/* codeset of 'Type', 'ESC' for escaped */
			/* info elems (IDI private coding)	*/
	byte Type;	/* type of info element			*/
	byte Info;	/* info in case of single octet elems	*/
	byte numOctets;	/* number of info octets		*/
	byte *Octets;	/* info octets				*/
} ISDN_FIND_INFO;

#if 0
static void diva_getname (ISDN_FIND_INFO *I, word Cmd, byte *Name, word sizeName);
#endif

/* ---------------------------------------------------------------------------
		IDI info element parsing
   --------------------------------------------------------------------------- */
typedef struct _diva_mtpx_is_parse {
	byte  wanted_ie;
	byte  wanted_escape_ie;
	int   wanted_ie_length; /* positive - MUST, negative - MAX, zero - IGNORE */
	byte* element_after;    /* I.e. should follow this element */
  byte* data;             /* Will be set to point to the start of the data  */
  byte* ie;               /* Will be set to point to the start of the ie    */
	int   ie_length;        /* Will be set to the total length of the ie      */
	int   data_length;      /* Will be set to the total length of data        */
} diva_mtpx_is_parse_t;
typedef void (*ParseInfoElementCallbackProc_t)(void* callback_context,
																							 byte Type,
																							 byte *IE,
																							 int  IElen,
																							 int SingleOctett);
static int ParseInfoElement (byte *Buf,
														 int  Buflen,
														 ParseInfoElementCallbackProc_t callback,
														 void* callback_context);
static void diva_mtpx_parse_ie_callback (diva_mtpx_is_parse_t* context,
																				 byte  Type,
																				 byte* IE,
																				 int   IElen,
																				 int   SingleOctett);
static void diva_mtpx_parse_sin_callback (diva_mtpx_is_parse_t* context,
																					byte  Type,
																					byte* IE,
																					int   IElen,
																					int   SingleOctett);

/* ---------------------------------------------------------------------------
		Process LAW and PROFILE indication and retrieve data about number of channels
    and about adapter features
   --------------------------------------------------------------------------- */
static dword diva_process_profile_change_notification (byte* data, word length, word* supported_channels);

/* ---------------------------------------------------------------------------
   --------------------------------------------------------------------------- */
typedef struct ISDN_PROT_MAP {
	byte *Name; const char* nice_name; byte Prot; byte b_fill; word Feature;
	byte B2in; byte B2out; byte B3;
	byte Cai[7];
	/*   Cai[0] = size of following info			*/
	/*   Cai[1] = B-channel hardware			*/
	/*   Cai[2] = baud rate (async only)			*/
	/*   Cai[3] = char length, STP, parity, PE (async only) */
	/*   Cai[4] = reserved					*/
	/*   Cai[5] = max. packet length (low byte)		*/
# define	      MAX_PACK_LO (ISDN_MAX_FRAME % 256)
	/*   Cai[6] = max. packet length (high byte)		*/
# define	      MAX_PACK_HI (ISDN_MAX_FRAME / 256)
} ISDN_PROT_MAP;

static ISDN_PROT_MAP protmap[] = {
{((byte *)"HDLC"), "HDLC",      ISDN_PROT_RAW, 0 , 0,
 B2_XPARENT_i	, B2_XPARENT_o	, (B3_XPARENT)	,
 {1, (B1_HDLC)	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"X75",    "X.75",      ISDN_PROT_X75	, 0, 0,
 B2_X75SLP	, B2_X75SLP	, B3_XPARENT	,
 {1, B1_HDLC	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"MODEM",   "",          ISDN_PROT_MDM_s, 0, DI_MODEM,
 B2_XPARENT_i	, B2_XPARENT_o	, B3_XPARENT	,
 {6, B1_MODEM_s	, 6	, 0	, 0	,MAX_PACK_LO,MAX_PACK_HI} },

{(byte *)"MODEM",  "MODEM",     ISDN_PROT_MDM_a, 0, DI_MODEM,
 B2_XPARENT_i	, B2_XPARENT_o	, B3_XPARENT	,
 {6, B1_MODEM_a	, 6	, 0	, 0	,MAX_PACK_LO,MAX_PACK_HI} },

{(byte *)"V110",  "",           ISDN_PROT_V110_s,0, DI_V110,
 B2_XPARENT_i	, B2_XPARENT_o	, B3_XPARENT	,
 {1, B1_V110_s	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"V110",  "V.110",      ISDN_PROT_V110_a,0, DI_V110,
 B2_XPARENT_i	, B2_XPARENT_o	, B3_XPARENT	,
 {6, B1_V110_a	, 0	, 0	, 0	,MAX_PACK_LO,MAX_PACK_HI} },

{(byte *)"VOICE", "",           ISDN_PROT_VOICE,0, 0,
 B2_XPARENT_i	, B2_XPARENT_o	, B3_XPARENT	,
 {6, B1_XPARENT	, 0	, 0	, 0	,/*MAX_PACK_LO*/128,0/*MAX_PACK_HI*/} },

{(byte *)"V120",  "V.120",      ISDN_PROT_V120, 0, DI_V120,
 B2_V120	, B2_V120	, B3_XPARENT	,
 {1, B1_HDLC	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"BTX",   "",           ISDN_PROT_BTX	, 0, 0,
 B2_X75SLP	, B2_X75SLP	, B3_T90NL	,
 {1, B1_HDLC	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"FAX",   "",           ISDN_PROT_FAX	, 0, DI_FAX3,
 B2_T30_i	, B2_T30_o	, B3_T30	,
 {6, B1_T30	, 0	, 0	, 0	,MAX_PACK_LO,MAX_PACK_HI} },

{(byte *)"EXT",   "",           ISDN_PROT_EXT_0, 0, 0,
 B2_NONE	, B2_NONE	, B3_NONE	,
 {1, B1_EXT_0	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"X75",   "X.75v42bis", ISDN_PROT_X75V42	, 0, 0,
 B2_X75SLPV42	, B2_X75SLPV42	, B3_XPARENT	,
 {1, B1_HDLC	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"PIAFS", "PIAFS32K",   ISDN_PROT_PIAFS_32K	, 0, 0,
 B2_PIAFS_OUT	, B2_PIAFS_IN	, B3_XPARENT	,
 {1, B1_PIAFS	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"PIAFS", "PIAFS64KFIX",ISDN_PROT_PIAFS_64K_FIX	, 0, 0,
 B2_PIAFS_OUT	, B2_PIAFS_IN	, B3_XPARENT	,
 {1, B1_PIAFS	, 0	, 0	, 0	, 0	, 0}		},

{(byte *)"PIAFS", "PIAFS64KVAR",ISDN_PROT_PIAFS_64K_VAR	, 0, 0,
 B2_PIAFS_OUT	, B2_PIAFS_IN	, B3_XPARENT	,
 {1, B1_PIAFS	, 0	, 0	, 0	, 0	, 0}		},
};

/* Needed for internal separation of the buggy CLASS 2 from the new CLASS 1 */
#define ISDN_PROT_FAX_CLASS1	(ISDN_PROT_MAX + 1)
#define ISDN_PROT_FAX_CLASS2	(ISDN_PROT_MAX + 2)
/* Needed for internal separation of old and new (V.42 capable) modem code  */
#define ISDN_PROT_MDM_RAW	(ISDN_PROT_MAX + 3)
#define ISDN_PROT_MDM_V42	(ISDN_PROT_MAX + 4)

typedef struct ISDN_CAUSE_MAP {
byte Q931Cause;					byte TapiCause;
} ISDN_CAUSE_MAP;

static ISDN_CAUSE_MAP causemap [] = {

/*	normal events							*/

{Q931_CAUSE_UnallocatedNumber_B_TinNr		, CAUSE_UNREACHABLE},
{Q931_CAUSE_NoRouteToSpecifiedTransitNetwork	, CAUSE_UNREACHABLE},
{Q931_CAUSE_NoRouteToDestination			, CAUSE_UNREACHABLE},
{Q931_CAUSE_SendSpecialInformationTone		, CAUSE_UNKNOWN},
{Q931_CAUSE_MisdialledTrunkPrefix		, CAUSE_UNKNOWN},
{Q931_CAUSE_ChannelUnacceptable			, CAUSE_UNKNOWN},
{Q931_CAUSE_CallAwardedAndBeingDeliveredInAnEstablishedChannel, CAUSE_UNKNOWN},
{Q931_CAUSE_NormalCallClearing			, CAUSE_NORMAL},
{Q931_CAUSE_UserBusy				, CAUSE_BUSY},
{Q931_CAUSE_NoUserResponding			, CAUSE_NOANSWER},
{Q931_CAUSE_NoAnswerFromUser_UserAlerted		, CAUSE_NOANSWER},
{Q931_CAUSE_CallRejected				, CAUSE_REJECT},
{Q931_CAUSE_NumberChanged			, CAUSE_UNREACHABLE},
{Q931_CAUSE_NonSelectedUserClearing		, CAUSE_REMOTE},
{Q931_CAUSE_DestinationOutOfOrder		, CAUSE_OUTOFORDER},
{Q931_CAUSE_AddressIncomplete_InvalidNumberFormat, CAUSE_UNREACHABLE},
{Q931_CAUSE_FacilityRejected			, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_ResponseTo_STATUS_ENQUIRY		, CAUSE_UNKNOWN},
{Q931_CAUSE_Normal_Unspecified			, CAUSE_UNAVAIL},

/*	resource unavailable						*/

{Q931_CAUSE_NoCircuit_ChannelAvailable		, CAUSE_CONGESTION},
{Q931_CAUSE_NetworkOutOfOrder			, CAUSE_CONGESTION},
{Q931_CAUSE_TemporaryFailure			, CAUSE_CONGESTION},
{Q931_CAUSE_SwitchingEquipmentCongestion		, CAUSE_CONGESTION},
{Q931_CAUSE_AccessInformationDiscarded		, CAUSE_CONGESTION},
{Q931_CAUSE_RequestedCircuit_ChannelNotAvailable	, CAUSE_CONGESTION},
{Q931_CAUSE_ResourcesUnvailable_Unspecified	, CAUSE_CONGESTION},

/*	service or option not available					*/

{Q931_CAUSE_QualityOfServiceUnavailable		, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_RequestedFacilityNotSubscribed	, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_OutgoingCallsBarredWhithinCUG	, CAUSE_UNREACHABLE},
{Q931_CAUSE_IncomingCallsBarredWhithinCUG	, CAUSE_UNKNOWN},
{Q931_CAUSE_BearerCapabilityNotAuthorized	, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_BearerCapabilityNotPresentlyAvailable, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_InconsistentDesignatedOutgoingAccessInfo_SubscrClass, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_ServiceOrOptionNotAvailable_Unspecified, CAUSE_INCOMPATIBLE},

/*	service or option not implemented				*/

{Q931_CAUSE_BearerCapabilityNotImplemented	, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_ChannelTypeNotImplemented		, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_RequestedFacilityNotImplemented	, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_OnlyRestrictedDigitalInfoBearerCapabilityIsAvailable, CAUSE_INCOMPATIBLE},
{Q931_CAUSE_ServiceOrOptionNotImplemented_Unspecified, CAUSE_INCOMPATIBLE},

/*	invalid message							*/

{Q931_CAUSE_InvalidCallReferenceValue		, CAUSE_FATAL},
{Q931_CAUSE_IdentifiedChannelDoesNotExist	, CAUSE_FATAL},
{Q931_CAUSE_ASuspendedCallExistsButThisCallIdentityDoesNot, CAUSE_FATAL},
{Q931_CAUSE_CallIdentityInUse			, CAUSE_FATAL},
{Q931_CAUSE_NoCallSuspended			, CAUSE_FATAL},
{Q931_CAUSE_CallHavingTheRequestedCallIdentityHasBeenCleared, CAUSE_FATAL},
{Q931_CAUSE_UserNotMemberOfCUG			, CAUSE_FATAL},
{Q931_CAUSE_IncompatibleDestination		, CAUSE_FATAL},
{Q931_CAUSE_NonExistentCUG			, CAUSE_FATAL},
{Q931_CAUSE_InvalidTransitNetworkSelection	, CAUSE_FATAL},
{Q931_CAUSE_InvalidMessage_Unspecified		, CAUSE_FATAL},

/*	protocol error (unknown message)				*/

{Q931_CAUSE_MandatoryInformationElementIsMissing	, CAUSE_FATAL},
{Q931_CAUSE_MessageTypeNonExistentOrNotImplemented, CAUSE_FATAL},
{Q931_CAUSE_MessageNotCompatibleWhithCallStateOrNonExOrNotImp, CAUSE_FATAL},
{Q931_CAUSE_InformationElementNonExistentOrNotImplemented, CAUSE_FATAL},
{Q931_CAUSE_InvalidInformationElementContents	, CAUSE_FATAL},
{Q931_CAUSE_MessageNotCompatibleWhithCallState	, CAUSE_FATAL},
{Q931_CAUSE_RecoveryOnTimerExpiry		, CAUSE_FATAL},
{Q931_CAUSE_ParameterNonExistentOrNotImplemented_PassedOn, CAUSE_FATAL},
{Q931_CAUSE_InconsistencyInData			, CAUSE_FATAL},
{Q931_CAUSE_ProtocolError_Unspecified		, CAUSE_FATAL},

/*	interworking							*/

{Q931_CAUSE_InterWorking_Unspecified		, CAUSE_UNKNOWN},

/*	the end								*/
{0						, 0}			};


/*
	Call filter
	*/
diva_call_filter_t diva_tty_call_filter;

static void putcai (byte **buf,
										ISDN_PROT_MAP *F,
										byte Baud,
										word MaxFrame,
										diva_modem_options_t* mdm_cfg);

static byte mappedcause(byte Cause)
{
	ISDN_CAUSE_MAP *p;

	if (Cause & CAUSE_MAPPED) return Cause;

	for (p = causemap; p->TapiCause; p++) {
		if (Cause == p->Q931Cause) return (p->TapiCause);
	}
	return (CAUSE_FATAL);
}


static int findinfo (ISDN_FIND_INFO *list, byte *parms, word size)
{
/*
 * Scan 'parms' for the info elems enumerated in 'list'.
 * Return total number of elems found and set the 'list'
 * entries accordingly.
 *
 * The 'numOctets' field in ISDN_INFO indicates the result per elem type:
 *	0	multi-octet elem whithout info,
 *		'Info' is 0, 'Octets' points to a nil string.
 *	1 - 127 multi-octet elem,
 *		'Info' indicates the position of the info element,
 *		'Octets' points to the first octet of info.
 *	128	single-octet elem, 'Info' contains bits 4-8 of elem,
 * 		'Octets' points to a nil string.
 *	255	no such elem in 'parms', 'Octets' points to a nil string.
 */
	ISDN_FIND_INFO *I;
	byte	  typeElem, *curElem, *lastElem;
	byte	  Codeset, Lock;
	int	  numElems;
 

	for (I = list; I->Type; I++) {
		I->Info = 0 ; I->numOctets = 0xFF; I->Octets = (byte *)"" ;
	}
	
    for (numElems = 0, Lock = Codeset = 0,
	     curElem = parms, lastElem = parms + size; curElem < lastElem; ) {

        	/* get element type */
		if (!(typeElem = INFO_ELEM_TYPE (curElem[0]))) {
			if ( curElem[0] || ((curElem + 1) < lastElem) )
			break;
		}

        	/* readjust codeset */
		if (Lock & 0x80) Lock &= 0x7F; else Codeset = Lock;

		/* handle single-octet elems */
		if (INFO_ELEM_SINGLE_OCTET (curElem[0])) {
			if (typeElem == SHIFT) {
				Codeset = curElem[0] & 0x07;
				if (!(curElem[0] & 0x08)) Lock = Codeset;
				Lock |= 0x80;
			} else for (I = list; I->Type; I++) {
				if (I->Codeset != Codeset ||
				    I->Type != typeElem   ||
				    I->numOctets != 0xFF   ) continue;
				I->numOctets = 0x80;
				I->Info = INFO_ELEM_SINGLE_INFO (curElem[0]);
				numElems++;
				break;
			}
			curElem++;
			continue;
		}

		/* handle multi-octet elems */
		if (curElem + 1 >= lastElem		||
		    curElem + 2 + curElem[1] > lastElem	||
		    (typeElem == ESC && curElem[1] < 1)  ) {
			break;
		}
		for (I = list; I->Type; I++) {
			if (typeElem == ESC) {
				if (I->Codeset != ESC || I->Type != curElem[2] || I->numOctets != 0xFF) { 
					continue;
				}
				if ((I->numOctets = (curElem[1] - 1))) {
					I->Octets = curElem + 3;
				}
				I->Info = (byte)++numElems;
				break;
			}
			if (I->Type == typeElem && I->Codeset == Codeset && I->numOctets == 0xFF) {
				if ((I->numOctets = curElem[1])) {
					I->Octets = curElem + 2;
				}
				I->Info = (byte)++numElems;
				break;
			}
		}
		curElem += 2 + curElem[1];
	}

	return (numElems);
}

#if D_PROTOCOL_DETECTION

static byte *findoctet (byte *info, byte length, byte n)
{
/*
 * Find the 'n'th octet in the information part of an info element coded
 * according to ETS 300 102-1 / Q931 and return the address of this octet
 * (or octet group).
 * In the spec octets are numbered from 1 where octet 1 is the type and
 * octet 2 is the length of info.
 * We expect 'info' to point to the 1st information octet, i.e to octet 3.
 */
	byte nOct, *Oct, *lastOct;

	if (n < 3 || !length) return (0);
	for (nOct = 3, Oct = info, lastOct = Oct + length;
	     nOct < n && Oct < lastOct; Oct++) {
	     if (*Oct & 0x80) nOct++;	/* ext bit 1 i.e end of octet group */
	}

	return ((Oct < lastOct) ? Oct : 0);
}

static void evalSIN (ISDN_CHANNEL *C, byte Service, byte ServiceAdd)
{
	static byte speedmap[] = /* maps SIN values to IDI-ASSIGN values */
	{
	/* 00 */	255,	/* 1200/1200	*/
	/* 01 */	255,	/* 1200/75	*/
	/* 02 */	255,	/* 75/1200	*/
	/* 03 */	255,	/* 2400/2400	*/
	/* 04 */	  4,	/* 4800/4800	*/
	/* 05 */	  5,	/* 9600/9600	*/
	/* 06 */	255,	/* 14400/14400	*/
	/* 07 */	  6,	/* 19200/19200	*/
	/* 08 */	  8,	/* 48000/48000	*/
	/* 09 */	  9,	/* 56000/56000	*/
	/* 0A */	  9,	/* 56000/56000	*/
	/* 0B */	255,	/* undefined	*/
	/* 0C */	255,	/* undefined	*/
	/* 0D */	255,	/* undefined	*/
	/* 0E */	255,	/* undefined	*/
	/* 0F */	255,	/* undefined	*/
	} ;

	switch (Service)
	{
	case 0x00: /* Any Service		*/
	case 0x01: /* Telephony Service		*/
	case 0x02: /* a/b Service		*/
	case 0x03: /* X.21 UC 30 Services	*/
	case 0x04: /* FAX G4			*/
	case 0x05: /* Videotex			*/
	case 0x08: /* X.25 Services		*/
	case 0x09: /* Teletex 64		*/
	case 0x0D: /* Remote Control		*/
	case 0x0F: /* Videotex (new)		*/
	default:   /* Undefined Service		*/
		return ;

	case 0x07: /* Digital Data		*/
		break ;
	}

	/* check for digital data protocol and speed */

	switch (ServiceAdd & 0xE0)
	{
	case 0xA0: /* sync mode */

		if ( ServiceAdd & 0x10 )
		{
			C->Prot = ISDN_PROT_V120 ;
			if ( speedmap[ServiceAdd & 0x0F] == 9 )
			{ /* all other speed adaptations are done in V.120 */
				C->Baud = 9 ; /* 56 K */
			}
		}
		else
		{
			C->Baud = speedmap[ServiceAdd & 0x0F] ;
			if ( C->Baud != 9 )
			{ /* they really mean V.110, not simply 56K HDLC */
				C->Prot = ISDN_PROT_V110_s ;
			}
		}
		break ;

	case 0xC0: /* async mode */
	case 0xE0: /* async mode */

		C->Baud = speedmap[ServiceAdd & 0x0F] ;
		if ( C->Baud != 9 )
		{ /* they really mean V.110, not simply 56K HDLC */
			C->Prot = ISDN_PROT_V110_a ;
		}
		break ;
	}
}
static void evalLLC (ISDN_CHANNEL *C, byte *info, byte length,byte *Service, byte *ServiceAdd)
{
	static byte speedmap[] = /* maps BC/LLC values to IDI-ASSIGN values */
	{
	/* 00 */	255,	/* undefined	*/
	/* 01 */	255,	/* 600		*/
	/* 02 */	255,	/* 1200		*/
	/* 03 */	255,	/* 2400		*/
	/* 04 */	255,	/* 3600		*/
	/* 05 */	  4,	/* 4800		*/
	/* 06 */	255,	/* 7200		*/
	/* 07 */	255,	/* 8000		*/
	/* 08 */	  5,	/* 9600		*/
	/* 09 */	255,	/* 14400	*/
	/* 0A */	255,	/* 16000	*/
	/* 0B */	  6,	/* 19200	*/
	/* 0C */	255,	/* 32000	*/
	/* 0D */	  7,	/* 38400	*/
	/* 0E */	  8,	/* 48000	*/
	/* 0F */	  9,	/* 56000	*/
	/* 10 */	255,	/* 64000	*/
	/* 11 */	255,	/* undefined	*/
	/* 12 */	255,	/* undefined	*/
	/* 13 */	255,	/* undefined	*/
	/* 14 */	255,	/* undefined	*/
	/* 15 */	255,	/* undefined	*/
	/* 16 */	255,	/* undefined	*/
	/* 17 */	255,	/* 1200/75	*/
	/* 18 */	255,	/* 75/1200	*/
	/* 19 */	255,	/* 50		*/
	/* 1A */	255,	/* 75		*/
	/* 1B */	255,	/* 110		*/
	/* 1C */	255,	/* 150		*/
	/* 1D */	255,	/* undefined	*/
	/* 1E */	255,	/* 300		*/
	/* 1F */	255,	/* undefined	*/
	} ;
	byte tmp[128], *Octet ;

	mem_cpy (tmp, info, length) ;
	tmp[length] = 0 ;

	/* octet 3 - coding standard */

	if ( !(Octet = findoctet (tmp, length, 3)) )
		return ;

	switch ( Octet[0] & 0x1F )
	{
	case 0x00: /* Speech */
	case 0x10: /* 3.1 kHz audio */
	case 0x18: /* Video */
	default:   /* Undefined transfer capability */

	if ( *Service != 1 )
	{
		*Service    = 1 ;
		*ServiceAdd = 0 ;
	}
	return  ; /* assume analog data */

	case 0x08: /* Unrestricted digital Information */
	case 0x09: /* Restricted digital Information */
	case 0x11: /* Unrestricted data with tones (7 kHz audio) */

		*Service    = 7 ;
		*ServiceAdd = 0 ;
		break ;	/* assume digital data, check for protocol */
	}

	/* octet 5 - user information layer 1 protocol */

	if ( !(Octet = findoctet (tmp, length, 5)) )
		return ; /* end of element */

	switch ( Octet[0] & 0x1F )
	{
	case 0x01: /* V.110 */

		/* octet 5a (sync/async - negotiation - user rate) */

		if ( ((Octet[0] & 0x80) != 0) || (Octet[1] == 0) )
			return ;  /* end of element */

		C->Baud = speedmap[ Octet[1] & 0x1F ] ;

		if ( C->Baud != 9 )
		{	/* they really mean V.110, not simply 56K HDLC */
			C->Prot = (Octet[1] & 0x40)?
				  ISDN_PROT_V110_a : /* async */
				  ISDN_PROT_V110_s ; /* sync  */
		}
		break ;

	case 0x08: /* V.120 */

		C->Prot = ISDN_PROT_V120 ;

		/* octet 5a (sync/async - negotiation - user rate) */

		if ( ((Octet[0] & 0x80) != 0) || (Octet[1] == 0) )
			return ;  /* end of element */

		if ( speedmap[ Octet[1] & 0x1F ] == 9 )
		{ /* all other speed adaptations are done in V.120 */
			C->Baud = 9 ; /* 56 K */
		}
		break ;

	case 0x02: /* u-law */
	case 0x03: /* A-law */

		/* cannot be anything else than analog data */
		if ( *Service != 1 )
		{
			*Service    = 1 ;
			*ServiceAdd = 0 ;
		}

 		return ;

	case 0x04: /* ADPCM 32 kbit/s */
	default:
		break ;
	}
}
#endif /* D_PROTOCOL_DETECTION */

static void set_channel_id (ISDN_CHANNEL *C, int Place)
{

 /* #if defined (UNIX) */
	sprintf(C->Id + Place,(byte*) "%d%02d", (int)(C->Adapter - Machine.Adapters) + 1,
					  (int)(C - C->Adapter->Channels) + 1      );
  /* #endif*/

  /* sprintf (C->Id + Place, (byte *)"%d%02d", (C->Adapter - Machine.Adapters) + 1,
					  (C - C->Adapter->Channels) + 1      ); */
}

static void set_esc_uid (ISDN_CHANNEL *C, char *pName)
{
	int i, l;

	C->EscUid[0] = ESC;
	C->EscUid[1] = 0;
	if (pName != NULL)
	{
		/* Create a short trace ID Pxx from the port name comxx */
		/* and pass it as ESC/UID to the card with every sig    */
		/* request.                                             */
		i = 0;
		l = 0;
		while (pName[l] != '\0')
		{
			if ((pName[l] < '0') || (pName[l] > '9'))
				i = l + 1;
			l++;
		}
		l -= i;
		if (l != 0)
		{
			if (l > sizeof(C->EscUid) - 4)
			{
				i += l - (sizeof(C->EscUid) - 4);
				l = sizeof(C->EscUid) - 4;
			}
			C->EscUid[1] = l + 2;
			C->EscUid[2] = UID;
			C->EscUid[3] = 'P';
			mem_cpy (&C->EscUid[4], &pName[i], l);
		}
	}
}

static void attach_port (ISDN_CHANNEL *C, void *P)
{ 
	/* set port handle and name */
	C->Port = P ;
	set_channel_id (C, sprintf (C->Id,(byte*) "%s-", Machine.Port->Name (C, P)));
	set_esc_uid (C, port_name(C)) ;
}

static void detach_port (ISDN_CHANNEL *C)
{ 
	/* clear port handle and name */
	C->Port = 0 ;
	set_channel_id (C, 0) ;
	set_esc_uid (C, NULL) ;
}

static void link_idle (ISDN_CHANNEL *C)
{ /* indicate idle to upper layer */

	if (C->Port) {
		Machine.Port->Idle (C, C->Port);
		detach_port (C) ;
	}
}

static void link_down (ISDN_CHANNEL *C, byte Cause)
{ /* indicate down to upper layer */

	if (!C->Port)
		return;	/* port lost */
	if (Machine.Port->Down (C, C->Port, mappedcause(Cause), C->ReceivedDisconnectCause))
		return;	/* indicate idle later */
	detach_port (C) ; /* not interested in idle indication */
}

/*
		Issue RESOURCES request that will change the type of assigned
		Layer 1 resources.
	*/
static void resources (ISDN_CHANNEL* C, byte Prot) {
	ISDN_PROT_MAP	*F;
	byte msg [270], *elem = msg;

 	for ( F = protmap ; F->Prot != Prot ; ) {
		if ( ++F >= protmap + sizeof(protmap)/sizeof(protmap[0]) ) {
			DBG_ERR(("A: [%p:%s] resources: bad P=%d", C, port_name(C), Prot))
			return;
		}
	}
	putcai (&elem, F,
					C->Baud,
					C->original_conn_parms.MaxFrame,
					&C->original_conn_parms.modem_options);
	isdnSigEnq (C, RESOURCES, msg, (word) (elem - msg));
}

static void disc_net (ISDN_CHANNEL *C, byte Method) {
	word size;


	DBG_TRC(("[%p:%s] disc_net", C, port_name(C)))


	if (!C->net_busy && (C->Net.Id == NL_ID) && !C->net_rc) {
		queueReset (&C->Xqueue);
	} else {
		if (C->net_busy || C->net_rc) {
			queueFreeTail (&C->Xqueue, &size);
		} else {
			queueReset (&C->Xqueue);
		}
		isdnNetCtlEnq (C, Method, 0, 0);
	}
}

static void disc_link (ISDN_CHANNEL *C, byte Cause, byte reject)
{ /* remove the network entity (if any) and disconnect call */

	C->State = C_CALL_DISC_X;
	disc_net (C, REMOVE);
	
	if (C->RequestedDisconnectCause != 0) {
		byte esc_cause[5] ;

   	esc_cause[0] = ESC;		/* elem type	*/
   	esc_cause[1] = 3;		/* info length	*/
   	esc_cause[2] = CAU;		/* real type 	*/
   	esc_cause[3] = (0x80 | C->RequestedDisconnectCause);
   	esc_cause[4] = 0;		/* 1tr6 cause	*/

		isdnSigEnq (C, ((reject == 0) ? HANGUP : REJECT), esc_cause, sizeof(esc_cause));
	} else {
		isdnSigEnq (C, HANGUP, "", 1);
	}
}

static void shut_link (ISDN_CHANNEL *C, byte Cause)
{ /* forcibly disconnect on lower layer state errors */


	switch (C->State) {
	default		   :
	case C_DOWN	   :
	case C_INIT	   :
	case C_IDLE	   :
		/* should better not happen */
		return;

	case C_CALL_DIAL   :
	case C_CALL_OFFER  :
	case C_CALL_ACCEPT :
	case C_CALL_ANSWER :
	case C_CALL_UP     :
	case C_DATA_CONN_X :
	case C_DATA_WAIT   :
	case C_DATA_XFER   :
		/* remove network layer entity (if any) and disonnect call */
		disc_link (C, Cause, 0);
		break;

	case C_DATA_DISC_X :
	case C_CALL_DISC_X :
		/* should be on the way of disconnecting */
		break;
	}

	/* forcibly detach upper layer (indicate down/idle) */

	link_down (C, Cause);
	link_idle (C);
}

static void s_hup_ind (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* hangup from signaling entity */

	static	ISDN_FIND_INFO	s_hup_info [] = {
		{ (ESC), (CAU), (0), (0), (0) },	/* elem 0 */
		{ (6),   (CIF), (0), (0), (0) },	/* elem 1 */
		{ (0),   (0)  , (0), (0), (0) }	/* stop	  */
	};

	ISDN_FIND_INFO *I, elems [sizeof (s_hup_info) / sizeof (s_hup_info[0])];
	byte CauseQ931, Cause1TR6, Charges[64];

	/* scan the parameters */

	mem_cpy (elems, s_hup_info, sizeof (elems));
	findinfo (elems, Parms, sizeParms);

	I = elems;
	/* 0: esc/cause (private coding)				*/
	/*	byte 4 = Q931 cause					*/
	/*	byte 5 = 1TR6 cause					*/
	if (I->numOctets == 2) {
		CauseQ931 = I->Octets[0] & 0x7f;
		Cause1TR6 = I->Octets[1] & 0x7f;
	} else {
		CauseQ931 = (C->State == C_CALL_DIAL) ?
					  CAUSE_FATAL : CAUSE_UNAVAIL;
		Cause1TR6 = 0;
	}

	I++;
	/* 1: charging info						*/
	/*	byte 2... number of charging units (IA5 text)		*/
	if (I->numOctets > 1 && I->numOctets < sizeof (Charges)) {
		mem_cpy (Charges, I->Octets, I->numOctets);
		Charges [I->numOctets] = 0;
	} else {
		Charges [0] = 0;
	}

	DBG_TRC(("[%p:%s] HANGUP IND State=%d", C, port_name(C), C->State))

	switch (C->State) {
	case C_DATA_CONN_X :
	case C_DATA_WAIT   :
	case C_DATA_XFER   :
		if (C->channel_assigned) {
			C->State = C_DATA_DISC_X;
			disc_net (C, N_DISC);
			break;
		}
	case C_CALL_DIAL  :
	case C_CALL_OFFER :
	case C_CALL_ACCEPT:
	case C_CALL_ANSWER:
	case C_CALL_UP    :
	case C_DATA_DISC_X:
	case C_CALL_DISC_X:

		/* give some info (correct only when setup fails)	*/

		if (CauseQ931 & CAUSE_MAPPED) {
			DBG_TRC(("[%p:%s] hangup: cause=unavail", C, port_name(C)))
		} else {
			DBG_TRC(("[%p:%s] hangup: Q931=%x(%x) 1TR6=%x(%x) charges=%s",
				  C, port_name(C),
				  CauseQ931, CauseQ931 | 0x80,
				  Cause1TR6, Cause1TR6 | 0x80,
				  Charges[0]? (char*)Charges : "unknown"))
		}

		C->State = C_IDLE;
#if APPLY_AGE_CHECK
		C->Age++;
#endif /*APPLY_AGE_CHECK*/

# if TRY_TO_QUIESCE
		/* if shutting down don't touch anything else	*/
	if (Machine.State == M_SHUT)
		{
			DBG_TRC(("[%s] hangup: unblock", C->Id))
			queueReset (&C->Xqueue);
			sysPostEvent (&Machine.QuiesceEvent);
			return;
		}
# endif /* TRY_TO_QUIESCE */

		/* If a network connection was established there should	*/
		/* have been a call to n_disc_ind before and thus the	*/
		/* network layer entity should have been removed.	*/
		/* The disc_net() is mainly for an early failing call	*/
		/* where the network layer entity is already assigned	*/
		/* but not connected.					*/
		/* In case of a locally generated hangup because of an	*/
		/* ISDN layer 1 error n_disc_ind is called just after 	*/
		/* this function.					*/
		/* But this behaviour seems to be buggy and may change	*/
		/* and thus we clean up here without further checks.	*/
		/* "n_disc_ind: state?" is reported later in this case.	*/

		disc_net (C, REMOVE); /* flushes the queue appropriately */

		/* Hangup is not to be acknowlegded, indicate down/idle	*/

		link_down (C, mappedcause (CauseQ931));
		link_idle (C);

		C->State = C_INIT;
		if (!C->sig_remove_sent) {
			isdnSigEnq (C, REMOVE, "", 1);
			C->sig_remove_sent = 1;
		}
		break;


#if 0 /* HACK */
		if (Machine.Listen) {
			isdnSigEnq (C, INDICATE_REQ, "", 1);
		}
#endif
	case C_INIT:
		if (!C->sig_remove_sent) {
			isdnSigEnq (C, REMOVE, "", 1);
			C->sig_remove_sent = 1;
		}
		break;
			
	case C_DOWN	  :
	case C_IDLE	  :
	default		  :
		DBG_TRC(("[%p:%s] s_hup_info, idle net_rc=%d", C, port_name(C), C->net_rc))
		/* should better not happen */
		break;
	}
}

static void n_disc_ind (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* disconnect indication from network layer entity */

	DBG_TRC(("[%p:%s] net_rc=%d", C, port_name(C), C->net_rc))

	switch (C->State) {
	case C_DATA_CONN_X:
	case C_DATA_WAIT  :
	case C_DATA_XFER  :
	case C_DATA_DISC_X: /* DISCONNECT COLLISION */
		/*
		 * Indicate down to upper layer, disconnect call,
		 * wait for completion of call disconnect.
		 *
		 * Cause of problems in disconnect handling whith transparent
		 * layer 2/3 B-channel protocols we don't respond orderly here.
		 * Guntram says it's better to remove the network entitity to
		 * keep the things running and as long as we don't support
		 * multiple X25 connections no problems will arise from this.
		 *
		 * The right action here would be
		 *
		 *	isdnNetCtlEnq (C, N_DISC_ACK, 0, 0);
		 *	isdnNetCtlEnq (C, REMOVE, 0, 0);
		 */

		DBG_TRC(("[%p:%s] n_disc_ind:", C, port_name(C)))

		if ((C->Prot == ISDN_PROT_FAX_CLASS2) && C->Port && Parms && sizeParms) {
			fax2RcvNDISC (C->Port, Parms, sizeParms);
		}

		if (divas_tty_wait_sig_disc == 0 || C->seen_hup_ind != 0) {
			link_down (C, (divas_tty_wait_sig_disc != 0 && C->ReceivedDisconnectCause != 0) ?
																					(C->ReceivedDisconnectCause & 0x7f) : CAUSE_NORMAL);
		} else {
			DBG_TRC(("[%p:%s] n_disc_ind: delayed link down after", C, port_name(C)))
		}
		disc_link (C, CAUSE_NORMAL, 0);

		break;

	case C_DOWN	  :
	case C_INIT	  :
	case C_IDLE	  :
	case C_CALL_DIAL  :
	case C_CALL_OFFER :
	case C_CALL_ACCEPT:
	case C_CALL_ANSWER:
	case C_CALL_UP    :
	case C_CALL_DISC_X:
	/* case C_DATA_DISC_X: */
	default		  :
		/* should better not happen in these states */
		shut_link (C, CAUSE_STATE);
		break;
	}
}

static void n_disc_ack (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* disconnect acknowledge from network layer entity */

	DBG_TRC(("[%p:%s] n_disc_ack, State=%d", C, port_name(C), C->State))
	DBG_TRC(("[%p:%s] net_rc=%d", C, port_name(C), C->net_rc))

	switch (C->State) {
	case C_DATA_DISC_X:
		/* disconnect call now, wait for call disconnect completion */
		disc_link (C, Q931_CAUSE_NormalCallClearing, 0);
		break;

	case C_INIT	  :
		if (!C->Port) {
			disc_net (C, REMOVE);
		}
		break;

	case C_DOWN	  :
	case C_IDLE	  :
	case C_CALL_DIAL  :
	case C_CALL_OFFER :
	case C_CALL_ACCEPT:
	case C_CALL_ANSWER:
	case C_CALL_UP    :
	case C_CALL_DISC_X:
	case C_DATA_CONN_X:
	case C_DATA_WAIT  :
	case C_DATA_XFER  :
	default		  :
		/* should better not happen in these states */
		break;
	}
}

static void put_idi_user_id (byte **buf, char* name) {
	byte* p = *buf;

	if (str_len(name) < 4) {	
		name = "Port?";
	} else {
		name += 2; /* Remove Di */
	}

	p[0] = UID;
	p[1] = str_len(name);
	str_cpy(&p[2],name);
	*buf += (p[1] + 2);

	**buf = 0;
}

static void putinfo (byte **buf, byte type, byte *info)
{ /* Put an info element to '*buf', advance *buf past elem. */

	byte *p = *buf;

	p[0] = type;
	mem_cpy (p + 1, info, *info + 1);
	p += *info + 2;
	*p = 0;	/* don't know why, copied from message.c */

	*buf = p;
}

static void putaddr (byte **buf, byte type, byte coding, byte octet_3a, byte *addr, int esc)
{ /* Put an address (string) info element to '*buf', advance *buf past elem */

	byte *p; int len, e_length = (octet_3a == 0) ? 0 : 1;

	if (!(len = str_len ((char *)addr)))	return;

	p = *buf;

	p[0] = esc != 0 ? ESC : type;
	p[1] = len + 1 /*for coding octet*/ + e_length /* octet 3a */ + (esc != 0);
	if (esc != 0)
		p[2] = type;
	p[2+(esc != 0)] = coding;
	if (e_length != 0) {
		p[3+(esc != 0)] = octet_3a;
	}
	mem_cpy (p + 3 + e_length + (esc != 0), addr, len);

	*buf += len + 3 + e_length + (esc != 0);

	**buf = 0;
}

static void putcai (byte **buf,
										ISDN_PROT_MAP *F,
										byte Baud,
										word MaxFrame,
										diva_modem_options_t* mdm_cfg) {
	/* Put a CAI info element to '*buf', advance *buf past elem */

	byte modem_line_taking;
	word disabled_modem_modulation;
	byte enabled_modem_modulation;
	byte *p = *buf;
	*p++ = CAI;
	mem_cpy (p, F->Cai, sizeof (F->Cai));

	switch (F->Prot) {
		case ISDN_PROT_MDM_s:
		case ISDN_PROT_MDM_a:
			if (mdm_cfg && mdm_cfg->framing_valid) {
				p[3] = mdm_cfg->framing_cai;
			}
			if (mdm_cfg && (mdm_cfg->valid  || mdm_cfg->guard_tone ||
											mdm_cfg->line_taking ||
											mdm_cfg->modulation_options  || mdm_cfg->retrain  ||
											mdm_cfg->reserved_modulation_options ||
											mdm_cfg->reserved_modulation ||
											mdm_cfg->s7 || mdm_cfg->s10 || mdm_cfg->bell ||
											mdm_cfg->fast_connect_mode)) {
				modem_line_taking = mdm_cfg->line_taking;
				if (mdm_cfg->fast_connect_mode == 1) {
					modem_line_taking |= DSP_CAI_MODEM_DISABLE_ANSWER_TONE;
					modem_line_taking |= DSP_CAI_MODEM_DISABLE_CALLING_TONE;
				}

				p[7] = modem_line_taking; p[0]++; /* line taking options */
			}
			if (mdm_cfg && (mdm_cfg->valid  || mdm_cfg->guard_tone ||
											mdm_cfg->modulation_options  || mdm_cfg->retrain  ||
											mdm_cfg->reserved_modulation_options ||
											mdm_cfg->reserved_modulation ||
											mdm_cfg->s7 || mdm_cfg->s10 || mdm_cfg->bell ||
											mdm_cfg->fast_connect_mode)) {
				p[8] = (mdm_cfg->negotiation | mdm_cfg->guard_tone); p[0]++;
			}
			if (mdm_cfg && (mdm_cfg->valid || mdm_cfg->s7 || mdm_cfg->s10 ||
											mdm_cfg->modulation_options || mdm_cfg->retrain ||
											mdm_cfg->reserved_modulation_options ||
											mdm_cfg->reserved_modulation || mdm_cfg->bell ||
											mdm_cfg->fast_connect_mode)) {
				disabled_modem_modulation = mdm_cfg->disabled;
				enabled_modem_modulation = mdm_cfg->enabled;
				if (mdm_cfg->bell /* ATB0 */ && !mdm_cfg->bell_selected) {
					disabled_modem_modulation |=
						 ((DSP_CAI_MODEM_DISABLE_BELL212A << 8) | (DSP_CAI_MODEM_DISABLE_BELL103  << 8));
				}
				if (mdm_cfg->fast_connect_mode && !mdm_cfg->fast_connect_selected) {
					switch (mdm_cfg->fast_connect_mode) {
					case 1:
					case 3:
						enabled_modem_modulation |=
							(DSP_CAI_MODEM_ENABLE_V22FC | DSP_CAI_MODEM_ENABLE_V22BISFC);
						if (disabled_modem_modulation & DSP_CAI_MODEM_DISABLE_V22BIS) {
							if (!(disabled_modem_modulation & DSP_CAI_MODEM_DISABLE_V22))
								enabled_modem_modulation &= ~DSP_CAI_MODEM_ENABLE_V22BISFC;
						} else {
							if (disabled_modem_modulation & DSP_CAI_MODEM_DISABLE_V22)
								enabled_modem_modulation &= ~DSP_CAI_MODEM_ENABLE_V22FC;
						}
						break;
					case 2:
						enabled_modem_modulation |= DSP_CAI_MODEM_ENABLE_V22FC;
						break;
					case 4:
						enabled_modem_modulation |= (DSP_CAI_MODEM_ENABLE_V22FC |
						                             DSP_CAI_MODEM_ENABLE_V22BISFC |
						                             DSP_CAI_MODEM_ENABLE_V29FC);
						if (disabled_modem_modulation & DSP_CAI_MODEM_DISABLE_V22BIS)
							enabled_modem_modulation &= ~DSP_CAI_MODEM_ENABLE_V22BISFC;
						if (disabled_modem_modulation & DSP_CAI_MODEM_DISABLE_V22)
							enabled_modem_modulation &= ~DSP_CAI_MODEM_ENABLE_V22FC;
						break;
					}
				}

				p[9] = (mdm_cfg->modulation_options | mdm_cfg->retrain); p[0]++;/*modem modulation options*/
				p[10] = (byte) (disabled_modem_modulation & 0x00ff);
				p[11] = (byte)((disabled_modem_modulation >> 8) & 0x00ff); p[0] += 2;
				if (mdm_cfg->min_tx || mdm_cfg->max_tx ||
						mdm_cfg->min_rx || mdm_cfg->max_rx ||
						enabled_modem_modulation ||
						mdm_cfg->s7     || mdm_cfg->s10 ||
						mdm_cfg->reserved_modulation_options ||
						mdm_cfg->reserved_modulation) {
					p[12] = enabled_modem_modulation; p[0]++;
					if (mdm_cfg->min_tx || mdm_cfg->max_tx ||
							mdm_cfg->min_rx || mdm_cfg->max_rx ||
							mdm_cfg->s7 || mdm_cfg->s10 ||
							mdm_cfg->reserved_modulation_options ||
							mdm_cfg->reserved_modulation) {
						p[13] = (byte)( mdm_cfg->min_tx     & 0x00ff);
						p[14] = (byte)((mdm_cfg->min_tx>>8) & 0x00ff); p[0] += 2;
						if (mdm_cfg->max_tx || mdm_cfg->min_rx || mdm_cfg->max_rx ||
								mdm_cfg->s7 || mdm_cfg->s10 ||
								mdm_cfg->reserved_modulation_options ||
								mdm_cfg->reserved_modulation) {
							p[15] = (byte)( mdm_cfg->max_tx     & 0x00ff);
							p[16] = (byte)((mdm_cfg->max_tx>>8) & 0x00ff); p[0]+=2;
							if (mdm_cfg->min_rx || mdm_cfg->max_rx ||
									mdm_cfg->s7 || mdm_cfg->s10 ||
									mdm_cfg->reserved_modulation_options ||
									mdm_cfg->reserved_modulation) {
								p[17] = (byte)( mdm_cfg->min_rx     & 0x00ff);
								p[18] = (byte)((mdm_cfg->min_rx>>8) & 0x00ff); p[0]+=2;
								if (mdm_cfg->max_rx || mdm_cfg->s7 || mdm_cfg->s10 ||
										mdm_cfg->reserved_modulation_options ||
										mdm_cfg->reserved_modulation) {
									p[19] = (byte)( mdm_cfg->max_rx     & 0x00ff);
									p[20] = (byte)((mdm_cfg->max_rx>>8) & 0x00ff); p[0]+=2;
									if (mdm_cfg->s7 || mdm_cfg->s10 ||
											mdm_cfg->reserved_modulation_options ||
											mdm_cfg->reserved_modulation) {
										p[21] = 0; p[0]++; /* disable symbol rates  */
										p[22] = 0; p[0]++; /* modem info options    */
										p[23] = 0; p[0]++; /* transmit level adjust */
										p[24] = 0; p[0]++; /* speaker parameters    */
										p[25] = mdm_cfg->s7; p[0]++;
										if (mdm_cfg->s10 ||
												mdm_cfg->reserved_modulation_options ||
												mdm_cfg->reserved_modulation) {
											p[26] = mdm_cfg->s10; p[0]++;
											if (mdm_cfg->reserved_modulation_options ||
													mdm_cfg->reserved_modulation) {
												/*
													Reserved Modem parameters, struct:
													byte  - length
													word  - options
													dword - modulation
													*/
												p[27] = 6; p[0]++;
												p[28] = (byte)( mdm_cfg->reserved_modulation_options       & 0x00ff);
												p[29] = (byte)((mdm_cfg->reserved_modulation_options >> 8) & 0x00ff); p[0] += 2;
												p[30] = (byte)( mdm_cfg->reserved_modulation                    );
												p[31] = (byte)((mdm_cfg->reserved_modulation >>  8) & 0x000000ff);
												p[32] = (byte)((mdm_cfg->reserved_modulation >> 16) & 0x000000ff);
												p[33] = (byte)((mdm_cfg->reserved_modulation >> 24) & 0x000000ff); p[0] += 4;
											}
										}
									}
								}
							}
						}
					}
				}
			}
			break;

	case ISDN_PROT_FAX:
		p[7] = (byte)(0 & 0xff);
		p[8] = (byte)(0 >> 8); p[0] += 2; /* options */
		p[9] = (byte)(0 & 0xff);
		p[10] = (byte)(0 >> 8); p[0] += 2; /* options2 */
		p[11] = (byte)(0 & 0xff);
		p[12] = (byte)(0 >> 8); p[0] += 2; /* min_speed */
		p[13] = (byte)(0 & 0xff);
		p[14] = (byte)(0 >> 8); p[0] += 2; /* max_speed */
		p[15] = (byte)(0 & 0xff);
		p[16] = (byte)(0 >> 8); p[0] += 2; /* reserved_4 */
		p[17] = (byte)(0 & 0xff);
		p[18] = (byte)(0 >> 8); p[0] += 2; /* reserved_5 */
		p[19] = (byte)(0 & 0xff);
		p[20] = (byte)(0 >> 8); p[0] += 2; /* max_overhead_seconds */
		p[21] = (byte)(0 & 0xff);
		p[22] = (byte)(0 >> 8); p[0] += 2; /* disabled_resolutions */
		p[23] = 0; p[0]++; /* max_recording_width */
		p[24] = 0; p[0]++; /* max_recording_length */
		p[25] = 0; p[0]++; /* min_scanline_time */
		p[26] = 0; p[0]++; /* transmit_level_adjust */
		p[27] = 0; p[0]++; /* disabled_symbol_rates_mask */
		p[28] = 0; p[0]++; /* info_options_mask */
		p[29] = mdm_cfg->fax_line_taking; p[0]++; /* line_taking_options */
		p[30] = 0; p[0]++; /* speaker_parameters */
		p[31] = mdm_cfg->s7; p[0]++; /* carrier_wait_time_seconds */
		break;

# if 0
	case ISDN_PROT_V110_s:
		p[2] = (Baud > 9 /*56K*/) ? 9  : Baud;
		break;
# endif
	case ISDN_PROT_V110_a:
		// p[2] = (Baud > 8 /*38K*/) ? 8  : Baud;
		p[2] = Baud;
		if (mdm_cfg && mdm_cfg->framing_valid) {
			p[3] = mdm_cfg->framing_cai;
		}
		DBG_TRC(("v110 baud: %d CAI: %02x", Baud, p[3]))
		break;
	default :
		if (p[1] == B1_HDLC && Baud == 9 /*56K*/) p[1] = B1_HDLC_56;
		break;
	}

	if (MaxFrame && p[0] >= 6) {
		p[5] = MaxFrame % 256;
		p[6] = MaxFrame / 256;
	}

	*buf += 1 + 1 + p[0]; /* type + length + info */
	**buf = 0;

	DBG_TRC(("putcai for %s", F->Name))
	DBG_BLK((p, p[0]+1))
}

static int netassign (ISDN_CHANNEL *C, byte Prot,
     									ISDN_PROT_MAP *F,
											T30_INFO *T30Info,
											int out,
											ISDN_CONN_PARMS *Parms) {
/*  construct the assign message for the network configuration */

	static byte dlc_def [3] = { /* default DLC	*/
		2,		/* info length		*/
		(2138 % 256),	/* max. info (lo byte)	*/
		(2138 / 256) 	/* max. info (hi byte)	*/
	};
	static byte dlc_v120 [7] = {
		6,		/* info length		*/
		(2138 % 256),	/* max. info (lo byte)	*/
		(2138 / 256),	/* max. info (hi byte)	*/
		0x08,		/* address A		*/
		0x01,		/* address B		*/
		127,		/* modulo		*/
		7 		/* Window size		*/
	};
	static byte dlc_mdm_no_ec [11] = {
		10,
		(2138 % 256),
		(2138 / 256),
		3, 1, 7, 7, 0, 0,
		DLC_MODEMPROT_DISABLE_V42_V42BIS |
		DLC_MODEMPROT_DISABLE_MNP_MNP5   |
		DLC_MODEMPROT_DISABLE_V42_DETECT |
    DLC_MODEMPROT_DISABLE_SDLC       |
		DLC_MODEMPROT_DISABLE_COMPRESSION, /* protoocl negotiation options */
		0 /* modem protocol options */
	};
	static byte dlc_mdm_no_ec_sdlc [24] = {
		23,
		(2138 % 256),
		(2138 / 256),
		3, 1, 7, 7, 0, 0,
		DLC_MODEMPROT_DISABLE_V42_V42BIS |
		DLC_MODEMPROT_DISABLE_MNP_MNP5   |
		DLC_MODEMPROT_DISABLE_V42_DETECT |
		DLC_MODEMPROT_DISABLE_COMPRESSION, /* protoocl negotiation options */
		0, /* modem protocol options */
		0, /* modem protocol break configuration */
		0, /* modem protocol application options */
		0, /* modem reserved - struct length */
		9, /* SDLC config - struct length */
		  (2138 % 256),
		  (2138 / 256), /* i.e. field length */
		  0x30, /* Address A */
		  0x00, /* Address B */
      0x07, /* Modulo */
      0x07, /* Window size */
		  0x00,
		  0x00, /* XID length */
		  0x1b /* SDLC Modem options */
	};

	byte msg[270], *elem ;

  if (!C->rx_dma_ref_count) {
    C->rx_dma_handle = diva_get_dma_descriptor (C, &C->rx_dma_magic);
  }

	elem = msg;

	*elem++ = CAI;		/* elem type		*/
	if (C->Sig.Id == 0xff) {
		*elem++ = 2;		/* info length		*/
		*elem++ = C->Sig.reserved2; /* ident lo */
		*elem++ = C->Sig.No;        /* ident hi */
	} else {
		*elem++ = 1;		/* info length		*/
		*elem++ = C->Sig.Id;	/* associated entity	*/
	}

  if (C->rx_dma_handle < 0) {
	*elem++ = LLI;		/* elem type		*/
	*elem++ = 1;		/* info length		*/
	*elem++ = 0x01 | 0x20 | 0x10;		/* don't block (OK_FC) | NO_CANCEL | CMA	*/
  } else {
	  *elem++ = LLI;		/* elem type		*/
	  *elem++ = 6;		/* info length		*/
    /* don't block OK_FC | NO_CANCEL | CMA | RX DMA	*/
	  *elem++ = 0x01 | 0x20 | 0x10 | 0x40;
    *elem++ = (byte)C->rx_dma_handle;
    *elem++ = (byte)C->rx_dma_magic;
    *elem++ = (byte)(C->rx_dma_magic >>  8);
    *elem++ = (byte)(C->rx_dma_magic >> 16);
    *elem++ = (byte)(C->rx_dma_magic >> 24);
    C->rx_dma_ref_count++;
  }

	*elem++ = LLC;		/* elem type		*/
	*elem++ = 2;		/* info length		*/
	*elem++ = out? F->B2out : F->B2in;	/* layer 2 protocol	*/
	*elem++ = F->B3;	/* layer 3 protocol	*/

	C->delay_hangup = 20 /*((F->B2out	==	B2_XPARENT_o)		&& \
										 (F->B2in		==	B2_XPARENT_i)		&& \
										 (F->B3			==	B3_XPARENT))	*	4 */ ;

	C->Prot = C->ReqProt = Prot ;

	switch (F->Prot)
	{
	case ISDN_PROT_MDM_s:
	case ISDN_PROT_MDM_a:
		C->Prot = ISDN_PROT_MDM_RAW ;
		if ( C->Features & DI_V_42  || 1) /* VST */
		{
			elem[-2] = out? B2_V42_o : B2_V42_i ;
			C->Prot = ISDN_PROT_MDM_V42 ;
			if (Parms && (Parms->modem_options.disable_error_control ||
										Parms->modem_options.protocol_options)) {
				if (Parms->modem_options.disable_error_control & DLC_MODEMPROT_DISABLE_SDLC) {
					/*
						In case SDLC is disabled do not provide the SDLC related parameters to the
            protocol code
						*/
					putinfo (&elem, DLC, dlc_mdm_no_ec);
					elem[-2] = Parms->modem_options.disable_error_control;
					elem[-1] = Parms->modem_options.protocol_options;
				} else {
					byte options = 0x03; /* wait CTS/DCD on, indicate CTS/DCD on */

					putinfo (&elem, DLC, dlc_mdm_no_ec_sdlc);
					elem[-15] = Parms->modem_options.disable_error_control;
					elem[-14] = Parms->modem_options.protocol_options;

					if (Parms->modem_options.sdlc_prot_options & SDLC_L2_OPTION_REVERSE_ESTABLISHEMENT) {
						options |= 0x10; /* REVERSE ESTABLISHMENT */
					}
					if (Parms->modem_options.sdlc_prot_options & SDLC_L2_OPTION_SINGLE_DATA_PACKETS) {
						options |= 0x08; /* SINGLE_DATA_PACKETS */
					}
					elem[-1] = options;

					elem[-7] = Parms->modem_options.sdlc_address_A;
				}
			}
		}
		break ;
	case ISDN_PROT_EXT_0:
		return (1) ;	/* no B-channel protocol */
	case ISDN_PROT_X75 :
	case ISDN_PROT_BTX :
		putinfo (&elem, DLC, dlc_def);
		break;
	case ISDN_PROT_FAX :
		putinfo (&elem, DLC, dlc_def);
		*elem++ = NLC;
		*elem++ = sizeof (*T30Info);
		mem_cpy (elem, T30Info, sizeof (*T30Info));
		/* ENTER HACK HACK HACK */
		if (out) ((T30_INFO *) elem)->station_id_len = 0;
		/* LEAVE HACK HACK HACK */
		elem += sizeof (*T30Info);
		break;
	case ISDN_PROT_V120:
		putinfo (&elem, DLC, dlc_v120);
		break;

	case ISDN_PROT_PIAFS_32K:
		if (global_mode & GLOBAL_MODE_CHINA_PIAFS) {
			putinfo (&elem, DLC, piafs_32K_DLC_china);
		} else {
			putinfo (&elem, DLC, piafs_32K_DLC);
		}
		break;

	case ISDN_PROT_PIAFS_64K_FIX:
		if (global_mode & GLOBAL_MODE_CHINA_PIAFS) {
			putinfo (&elem, DLC, piafs_64K_FIX_DLC_china);
		} else {
			putinfo (&elem, DLC, piafs_64K_FIX_DLC);
		}
		break;

	case ISDN_PROT_PIAFS_64K_VAR:
		if (global_mode & GLOBAL_MODE_CHINA_PIAFS) {
			putinfo (&elem, DLC, piafs_64K_VAR_DLC_china);
		} else {
			putinfo (&elem, DLC, piafs_64K_VAR_DLC);
		}
		break;

	default:
		break;
	}

	*elem = 0;	/* just to be sure */
	DBG_TRC(("[%s] %s: net Id=%04x", C->Id, out? "Dial" : "Answer", WIDE_ID(C->Net)))
	DBG_BLK((msg, elem - msg))

	return (isdnNetCtlEnq (C, ASSIGN, msg, (word) (elem - msg)));
}

/*
	queue the basic assign message for the signaling entity
	*/
static int sigassign (ISDN_CHANNEL *C) {
	static byte sigparms [] = {
		KEY, 0x04,  'D',  'I',  'P', '0',
		OAD, 0x01, 0xFD,
		CAI, 0x01, 0xC0,
		SHIFT | 0x08 | 6,  /* non-locking SHIFT, codeset 6 */
		SIN, 0x02, 0x00, 0x00
	};
	static byte sigparms_wide_id [] = {
		KEY, 0x04,  'D',  'I',  'P', '0',
		OAD, 0x01, 0xFD,
		CAI, 0x01, 0xC0,
		SHIFT | 0x08 | 6,  /* non-locking SHIFT, codeset 6 */
		SIN, 0x02, 0x00, 0x00,
		LLI, 0x02, 0x02, (0x04 | 0x02) /* Use wide ID, use profile change notification */
	};

	C->RequestedDisconnectCause = 0;
	C->ReceivedDisconnectCause  = 0;
	if (diva_wide_id_detected) {
		return (isdnSigEnq (C, ASSIGN, sigparms_wide_id, sizeof (sigparms_wide_id)));
	} else {
		return (isdnSigEnq (C, ASSIGN, sigparms, sizeof (sigparms)));
	}
}

/*
 * the external interface
 */

int isdnListen (int Listen)
{ /* prepare for incoming connections */

	ISDN_CHANNEL *C; byte Listening;

	Listening = Machine.Listen;

	if (Listen) {
		Machine.Listen++;
	} else if (Machine.Listen) {
		Machine.Listen--;
	}

	DBG_TRC(("Listen: %d -> %d ", Listening, Machine.Listen))

	if (!((Listening == 0) ^ (Machine.Listen == 0))) {
		/* no fundamental change in listen state */
		return (Machine.Listen);
	}

	for (C = Machine.Channels; C < Machine.lastChannel; C++) {

		if (C->State != C_IDLE)	continue; /* unsuitable state  */
        
		isdnSigEnq (C, (byte) (Listen? INDICATE_REQ : HANGUP), "", 1);
		isdnPutReq (C);
	}

	return (Machine.Listen);
}

/*
	send data to net
	*/
static int isdnXmit (void *P,
										 void *hC,
										 byte *Data,
										 word sizeData,
										 word FrameType) {

	/* The V120 terminal adaption header may be 1 or 2 bytes but	*/
	/* for CompuServe 1 byte is sufficient (but 2 bytes work too)	*/
#	define SIZE_OF_V120_TA_HEADER	1
	static byte Head_V120 [SIZE_OF_V120_TA_HEADER] =
	{
#	if SIZE_OF_V120_TA_HEADER == 1
			0x83  /* Ext,BR ,res,res,C2 ,C1 ,B  ,F   */
#	else /*  SIZE_OF_V120_TA_HEADER != 1 */
			0x03, /* Ext,BR ,res,res,C2 ,C1 ,B  ,F   */
	        	0xf0  /* Ext,DR ,SR ,RR ,res,res,res,res */
#	endif /* SIZE_OF_V120_TA_HEADER == 1 */
	};

	ISDN_CHANNEL *C;
	ISDN_MSG *M;
	byte *Head=0; 
	word sizeHead;


	if (!(C = (ISDN_CHANNEL *) hC) || C->Port != P) {
		return(0);
	}

	/*
		Fax class 2 should send N_EDATA before it will receive N_DISC IND
		*/
		if ((C->State != C_DATA_XFER) || C->sig_hup_rnr) {
			if (!C->net_busy && !C->net_rc && !C->NetFC) {
				isdnPutReq(C);
			}
			if (C->sig_hup_rnr && (C->State == C_DATA_XFER) &&
					(C->Prot == ISDN_PROT_FAX_CLASS2) && C->Port && fax2DirectionIn(C->Port)) {
				DBG_TRC(("[%p:%s] EDATA in DISC (fax class 2 in)", C, port_name(C)))
			} else {
				return(0);
			}
		}

	sizeHead = 0;

	switch (C->Prot) {
	case ISDN_PROT_V120 :
		Head	 = Head_V120;
		sizeHead = sizeof (Head_V120);
		break;
#if defined(WINNT) || defined(UNIX)
	case ISDN_PROT_FAX_CLASS1:
	case ISDN_PROT_FAX_CLASS2:
		/* don't buffer too much but permit for some control data packets */
		if ( queueCount (&C->Xqueue) > 256)
		{
			DBG_TRC(("[%p:%s] Xmit: Q busy (fax class)", C, port_name(C)))
			return (-1);
		}
		break ;
#endif /* WINNT */

	default: /* currently no special handling for the other protocols */
	break;
	}


	if (!(M = (ISDN_MSG *)
		  queueAllocMsg (&C->Xqueue,
		   		 (word) (sizeof (*M) + sizeHead + sizeData)))) {
		DBG_TRC(("[%p:%s] Xmit: Q full", C, port_name(C)))
		if (!C->net_busy && !C->net_rc && !C->NetFC) {
			isdnPutReq(C);
		}
		return (-1);
	}

	M->Channel = C;
	M->Age	   = C->Age;
	M->Kind	   = ISDN_NET_DATA_REQ;
	M->Type	   = (byte) FrameType;	/* the last byte is the IDI type */

	if (sizeHead) mem_cpy (M->Parms, Head, sizeHead);

	mem_cpy (M->Parms + sizeHead, Data, sizeData);

	queueCompleteMsg ((byte *) M);

	if (!C->net_busy && !C->net_rc && !C->NetFC) {
		isdnPutReq(C);
	}


	return (1);
}

static void isdnUpData (ISDN_CHANNEL *C, byte *func, ISDN_CONN_INFO *Info)
{ /* indicate 'Up' for data traffic now */

	DBG_TRC(("[%p:%s] %s: signal Up", C, port_name(C), func))
	C->State = C_DATA_XFER;
	(void) Machine.Port->Up (C, C->Port, Info);

	if (C->transparent_detect && C->detected_packet && C->Adapter && C->Port) {
		/*
			Only for transparent protocols (HDLC, V.110), also we can send data directly
			without any processing (like necessary for V.120)
			*/
		Machine.Port->Recv (C, C->Port, C->detected_packet, C->detected_packet_length, N_DATA);
		C->detected_packet = 0;
	}
}

static void isdnUp (ISDN_CHANNEL *C, byte *func)
{ /* delay 'Up' indication until carrier or protocol is detected */


	if ( C->Prot == ISDN_PROT_MDM_RAW || C->Prot == ISDN_PROT_MDM_V42 ||
			 C->Prot == ISDN_PROT_PIAFS_64K_VAR	||
			 C->Prot == ISDN_PROT_PIAFS_64K_FIX	||
			 C->Prot == ISDN_PROT_PIAFS_32K
	     /* carrier (if any) indicated by an UDATA packet */
#if B_PROTOCOL_DETECTION
	 ||  C->DetProt == DET_PROT_FIRST_FRAME
	     /* indicate up when we know the protocol */
#endif /* B_PROTOCOL_DETECTION */
	) {

		if (C->MdmUp && ((C->Prot == ISDN_PROT_MDM_RAW) || (C->Prot == ISDN_PROT_MDM_V42))) {
			DBG_TRC(("[%p:%s] %s: delayed modem Up", C, port_name(C), func))
			isdnUpData (C, "NetDataInd[DCD]", &C->MdmInfo) ;
		} else {
			DBG_TRC(("[%p:%s] %s: delay Up", C, port_name(C), func))
			C->State = C_DATA_WAIT;
		}

	} else {
		isdnUpData (C, func, 0) ;
	}
}

static byte isdnKindFax (T30_INFO *T30Info, byte *Func)
{
	if (!T30Info) {
		return (ISDN_PROT_DET);
	}
	switch (T30Info->atf)
	{
	case IDI_ATF_CLASS1: return (ISDN_PROT_FAX_CLASS1);
	case IDI_ATF_MODEM:  return (ISDN_PROT_FAX_CLASS2);
	default:
	    DBG_ERR(("%s: bad FAX arg (%d)", Func, T30Info->atf))
	}
	return (ISDN_PROT_DET);
}

/*
 * Outgoing call handling
 */
static void *isdnDial (void *P, ISDN_CONN_PARMS *Parms, T30_INFO *T30Info)
{ /* construct call request and send it to board */

	ISDN_CHANNEL	*C, *lastChannel;
	ISDN_PROT_MAP	*F;
	byte		Prot,msg[270], *elem;
  
	/* check protocol */

	Parms->Cause = CAUSE_FATAL;

 	for ( F = protmap ; F->Prot != Parms->Prot ; )
	{
		if ( ++F >= protmap + sizeof(protmap)/sizeof(protmap[0]) )
	/*	if ( ++F >= protmap + sizeof(protmap) * sizeof(protmap[0]) )*/ 
		{
			DBG_ERR(("A: Dial: bad P=%d", Parms->Prot))
			return (0) ;
		}
	}

	if ( F->Feature && !(Machine.Features & F->Feature) )
	{
		DBG_ERR(("A: Dial: %s (P=%d) not supported", F->Name, F->Prot))
		return (0) ;
	}
	
	Prot = F->Prot ;

	if ( (Prot == ISDN_PROT_FAX)
	  && ((Prot = isdnKindFax (T30Info, (byte *)"Dial")) == ISDN_PROT_DET) ) {
		DBG_ERR(("A: Dial: invalid fax parameters"))
		return (0);
	}

	/* find a suitable adapter to serve this request */
	if ( (Parms->Line != 0) && (Parms->Line <= Machine.numAdapters) )
	{
		C = (Machine.Adapters + (Parms->Line - 1))->Channels ;
		lastChannel = (Machine.Adapters + (Parms->Line - 1))->lastChannel;
	}
	else
	{
		C = Machine.Channels ;
		lastChannel = Machine.lastChannel ;
	}

	for ( ; ; ) {
		word dummy;
		if ((C->State	 == C_IDLE)											&&
				(C->Net.Id == NL_ID)											&&
				(C->Sig.Id != DSIG_ID)										&&
				(!(C->net_busy))													&&
				(!(C->net_rc))														&&
				(!(C->sig_busy))													&&
				(!queuePeekMsg (&C->Xqueue, &dummy))			&&
				(!queuePeekMsg (&C->Squeue, &dummy))			&&
				(!(F->Feature) || (F->Feature & C->Features))) {
			break ;
		}
		if (++C >= lastChannel) {
			DBG_ERR(("A: Dial: no channel"))
			Parms->Cause = CAUSE_AGAIN;
			return (0);
		}
	}

	set_channel_id (C, sprintf (C->Id,(byte*) "%s-", Machine.Port->Name (C, P)));
	set_esc_uid (C, port_name(C)) ;

	/* first place the call */

	elem = msg;

	put_idi_user_id (&elem, get_port_name (P));
	putcai  (&elem, F, Parms->Baud, Parms->MaxFrame, &Parms->modem_options);

	putaddr (&elem, OAD, Parms->PlanOrig, Parms->Presentation_and_Screening, Parms->Orig, 0);
	if (Parms->Orig[0] != 0 && Parms->Orig_1[0] != 0) {
		putaddr (&elem, OAD, Parms->PlanOrig_1, Parms->PresentationAndScreening_1, Parms->Orig_1, 1);
	}
	mem_cpy (&C->original_conn_parms, Parms, sizeof(*Parms));

	if (Parms->OrigSub[0]) {
		elem[0] = OSA;
		memcpy (&elem[1], &Parms->OrigSub[0], Parms->OrigSub[0]+1);
		elem += (Parms->OrigSub[0]+2);
	}

	putaddr (&elem, DAD, Parms->PlanDest, 0, Parms->Dest, 0);
	putaddr (&elem, DSA, 0xFF, 0, Parms->DestSub, 0);

	if (Parms->selected_channel) {
		*elem++ = ESC;
		*elem++ = 0x02;
		*elem++ = CHI;
		*elem++ = ((byte)Parms->selected_channel) | 0x40 /* Exclusive */;
	}

	if ( Parms->Bc[0] )
	{
		elem[0] = BC ;
		mem_cpy (&elem[1], &Parms->Bc[0], Parms->Bc[0] + 1) ;
		elem += Parms->Bc[0] + 2 ;
		if ( Parms->Llc[0] )
		{
			elem[0] = LLC ;
			mem_cpy (&elem[1], &Parms->Llc[0], Parms->Llc[0] + 1) ;
			elem += Parms->Llc[0] + 2 ;
		}
	}
	else
	{
		*elem++ = SHIFT|0x08|6;	/* non-locking SHIFT, codeset 6 */
		*elem++ = SIN;		/* type (in codeset 6) */
		*elem++ = 2;		/* length of info */
		*elem++ = Parms->Service;	/* service */
		*elem++ = Parms->ServiceAdd;	/* service additional */

	}

	DBG_TRC(("[%p:%s] Dial(%s): sig Id=%04x", C, port_name(C), Parms->Dest, WIDE_ID(C->Sig)))
	/*
		DBG_BLK((msg, elem - msg))
		*/

	C->RequestedDisconnectCause = 0;
	C->ReceivedDisconnectCause  = 0;

	if (!isdnSigEnq (C, CALL_REQ, msg, (word) (elem - msg))) {
		DBG_ERR(("A: [%p:%s] ISDN DIAL QUEUE FULL", C, port_name(C)))
		return (0);	/* SNO */
	}

	/* second assign the network layer info,	*/
	/* so it is there when the call is connected	*/

#if B_PROTOCOL_DETECTION
	C->DetProt = DET_PROT_OFF;
#endif /* B_PROTOCOL_DETECTION */

	if (!netassign (C, Prot, F, T30Info, 1 /*out*/, Parms)) {
		DBG_ERR(("A: [%p:%s] Dial: NetASSIGN Failed", C, port_name(C)))
		return (0);	/* SNO */
	}

  
	attach_port (C, P) ;

	C->Baud	 = Parms->Baud;

	C->State = C_CALL_DIAL;
	C->NetUp = 0;
	C->MdmUp = 0;
	C->transparent_detect = 0;

	DBG_TRC(("[%p:%s] Dial, SigId=%04x, NetId=%04x", C, port_name(C), WIDE_ID(C->Sig), WIDE_ID(C->Net)))

	isdnPutReq (C);

	return (C);

	/* wait for CALL_CON */
}

/*
	call connected now, set up B-channel connection
	*/
static void s_call_con (ISDN_CHANNEL *C, byte *Parms, word sizeParms) {

	switch ( C->State )
	{
	case C_CALL_DIAL  :
		break ; /* that's the way I like it */
#if HANDLE_EARLY_CONNECT
	case C_DATA_CONN_X:
	case C_DATA_WAIT  :
	case C_DATA_XFER  :
	case C_CALL_DISC_X:
	case C_DATA_DISC_X:
		if ( C->NetUp )
		{
 			DBG_TRC(("[%p:%s] s_call_con: late ?", C, port_name(C)))
			return ;
		}
		/* fall through */
#endif /* HANDLE_EARLY_CONNECT */
	default:
		DBG_ERR(("A: [%p:%s] s_call_con: state?=%d", C, port_name(C), C->State))
		shut_link (C, CAUSE_STATE) ;
		return;
	}

	if (C->NetUp) {
		/* network entity connected before call completion */
		DBG_TRC(("[%p:%s] NetUp before CALL_CON", C, port_name(C)))
		isdnUp (C, (byte *)"s_call_con");
		return;
	}

	C->State = C_CALL_UP;

	if (C->Prot == ISDN_PROT_EXT_0) {
		/* there is no network layer for external devices */
		isdnUpData (C, (byte *)"s_call_con[EXT]", 0);
		return;
	}

	if (!isdnNetCtlEnq (C, N_CONNECT, 0, 0)) {
		DBG_ERR(("A: [%p:%s] SHUT LINK after N_CONNECT", C, port_name(C)))
		shut_link (C, CAUSE_FATAL);
		return;
	}

	if (C->Prot == ISDN_PROT_FAX_CLASS2) {
		/* Sorry, this is a hack to overcome the odd	*/
		/* CLASS2 behavior. We get T30 info via N_EDATA	*/
		/* before N_CONNECT_ACK (which indicates that	*/
		/* the first page can be sent now) and thus we	*/
		/* indicate Up here and send the N_CONNECT_ACK	*/
		/* as a control data indication (see below).	*/
		isdnUp (C, (byte *)"s_call_con");
	} else {
		/* Wait for N_CONNECT_ACK			*/
		C->State = C_DATA_CONN_X;
	}
}

#if HANDLE_EARLY_CONNECT

static void s_info_ind (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* check info ind for special connect info (France Minitel) containing
	08	----->	Cause info element
	02	----->	Length 2 bytes
	8x	----->	Coding standard CCITT,
				Location variable (x), don't check
	F2	----->	Cause - Data transfer from end-to-end not possible
	1E	----->	Progress Indicator info element
	02	----->	Length 2 bytes
	82	----->	Coding standard CCITT,
				Location - Public network serving the local user
	81	----->	Call is not ISDN type from end to end

  */
	ISDN_FIND_INFO I[3] ;
	
#if 0
	if (C->State == C_DOWN) { /* POSSIBLE ANSWER ON LAW REQ */
		ISDN_FIND_INFO	info [] = {
			(ESC), (PROFILEIE), (0), (0), (0),	/* elem 0 */
			(0),   (0)  , (0), (0), (0)	/* stop	  */
		};

		DBG_TRC(("[%p] get_law (%d)", C, sizeParms))

		if ((findinfo (&info[0], Parms, sizeParms))) {
			C->State = C_INIT;
			DBG_TRC(("[%p] LAW_IND, net_busy=%d, net_rc=%d", C, C->net_busy, C->net_rc))
			isdnSigEnq (C, REMOVE, "", 1);
		} else {
			int i;
			DBG_ERR(("A: [%p] NO LAW ?, net_busy=%d, net_rc=%d", C, C->net_busy, C->net_rc))
			for (i = 0; i < sizeParms; i++) {
				DBG_TRC(("P[%d]=%02x", i, ((byte*)Parms)[i]))
			}
			isdnSigEnq (C, REMOVE, "", 1);
		}
		return ;
	}
#endif

	if ( (C->State != C_CALL_DIAL)
	  || (C->NetUp)
	  || (   (C->Prot != ISDN_PROT_EXT_0)
	      && (C->Prot != ISDN_PROT_MDM_RAW)
	      && (C->Prot != ISDN_PROT_MDM_V42)
	      && (C->Prot != ISDN_PROT_FAX_CLASS1)
	      && (C->Prot != ISDN_PROT_FAX_CLASS2)) )
	{
		return ; /* not interested in this */
	}

	/* scan the parameters */

	mem_zero (&I[0], sizeof(I)) ;
	I[0].Type = 0x08  /* Cause		*/ ;
	I[1].Type = 0x1E  /* Progress Indicator	*/ ;

	findinfo (&I[0], Parms, sizeParms) ;

	if ( (I[0].numOctets < 2)
	  || ((I[0].Octets[0] & 0xF0) != 0x80) || (I[0].Octets[1] != 0xF2)
	  || (I[1].numOctets != 2)
	  || (I[1].Octets[0] != 0x82) || (I[1].Octets[1] != 0x81) )
	{
		return ;	/* not interresting */
	}

	DBG_TRC(("[%p:%s] s_info_ind: early B_CONNECT", C, port_name(C)))
	DBG_TRC(("[%p:%s] Net.Id=%04x, Net.Req=%d", C, port_name(C), WIDE_ID(C->Net), C->Net.Req))

	C->NetUp = 1 ;	/* remember early connect */

	if (C->Prot == ISDN_PROT_EXT_0) {
		/* there is no network layer for external devices */
		isdnUpData (C, (byte *)"s_info_ind[EXT]", 0);
		return;
	}

	if ( !isdnNetCtlEnq (C, N_CONNECT, 0, 0) ) {
		shut_link (C, CAUSE_FATAL);
		return;
	}

	if (C->Prot == ISDN_PROT_FAX_CLASS2) {
		/* a comment about this hack see in s_call_con() above */
		isdnUp (C, (byte *)"s_info_ind");
		/* C_DATA_WAIT or C_DATA_XFER state set by isdnUp() */
	} else {
		/* Wait for N_CONNECT_ACK */
		C->State = C_DATA_CONN_X;
	}
}
#endif /* HANDLE_EARLY_CONNECT */

static void n_conn_ack (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* network layer connected now, indicate up to user */

	if (C->Prot == ISDN_PROT_FAX_CLASS2 &&
	    C->State == C_DATA_XFER && C->Port) {
		/* Sorry, the next CLASS2 hack.	*/
		Machine.Port->Recv (C, C->Port, Parms, sizeParms,
				    ISDN_FRAME_CTL | N_CONNECT_ACK);
		return;
	}

	if (C->State != C_DATA_CONN_X || !C->Port) {
		DBG_ERR(("A: [%p:%s] n_conn_ack: state=%d", C, port_name(C), C->State))
#if 0 /* IGNORE THIS, HACK. HOW  IST IS POSSIBLE? BUT IT HAPPENS !!! */
		shut_link (C, CAUSE_STATE);
#endif
		return;
	}

	isdnUp (C, (byte *)"n_conn_ack");
}

#if 0
/*
 * Incoming call handling
 */

static int getaddr (ISDN_FIND_INFO *I, byte *Number, word sizeNumber,
										int use_type, int one_digit)
{	/*  All address info elements have the same format:		*/
	/*	byte 2 = address type, numbering/addressing plan	*/
	/*	byte 3.. address digits								*/
	/*	We copy the plain address digits only !				*/
	word number_type = 0;
	int written = 0;

	if (I->numOctets && use_type && (!(global_options & DIVA_ISDN_IGNORE_NUMBER_TYPE))) {
		if				((I->Octets[0] & 0x70) == 0x10) { /* international */
   		number_type = 2;
   	} else if	((I->Octets[0] & 0x70) == 0x20) { /* national */
			number_type = 1;
		}
	}
	if (sizeNumber > number_type) {
		sizeNumber -= number_type;
	} else {
		sizeNumber = 0;
	}

	if (I->numOctets > 1 && I->numOctets <= sizeNumber) {
		while (number_type--) {
    	*Number++ = '0';
			written++;
		}

    if (!one_digit) {
		  if ( I->Octets[1] & 0x80) { I->Octets++; I->numOctets--; }
    }

		mem_cpy (&Number[0], I->Octets + 1, I->numOctets - 1);
		Number[I->numOctets - 1] = 0;
		written += (I->numOctets - 1);
	} else {
		Number[0] = 0 ;
	}

	return (written);
}
#endif

/*
	Incoming call present (after INDICATE_REQ), pass on to upper layer
	*/
static void s_ring_ind (ISDN_CHANNEL *C, byte *Info, word sizeInfo) {
	static ISDN_CONN_PARMS Parms ;
	byte		esc_cause[5] ;
	void	       *Port ;
	word dummy = 0;
	int fixed_protocol;
	diva_mtpx_is_parse_t bc_parse_ie, llc_parse_ie, *final_parse_ie = 0, sin_parse_ie, osa_parse_ie;

	if (!(C &&
			(C->State	 == C_IDLE)											&&
			(C->Net.Id == NL_ID)											&&
			(C->Sig.Id != DSIG_ID)										&&
			(!(C->net_busy))													&&
			(!(C->net_rc))														&&
			(!(C->sig_busy))													&&
			(!queuePeekMsg (&C->Xqueue, &dummy))			&&
			(!queuePeekMsg (&C->Squeue, &dummy)))) {
		DBG_ERR(("A: [%p:%s] inc call collision", C, port_name(C)))
		return;
	}

	C->RequestedDisconnectCause = 0;
	C->ReceivedDisconnectCause  = 0;

	/*
		Init parameters
		*/
	mem_zero (&Parms, sizeof(Parms)) ;

	/*
		Set up adapter number
		*/
	Parms.Line = (unsigned char)((C->Adapter - Machine.Adapters) + 1) ;

	/*
		Retrieve DAD
		*/
	{
		diva_mtpx_is_parse_t parse_ie;

		memset (&parse_ie, 0x00, sizeof(parse_ie));
		parse_ie.wanted_ie	= CPN;

		ParseInfoElement (Info,
											sizeInfo,
											(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
											&parse_ie);

		Parms.Dest[0] = 0;

		if (parse_ie.data && (parse_ie.data_length > 1)) {
			while (parse_ie.data_length) {
				if (*parse_ie.data++ & 0x80) {
					parse_ie.data_length--;
					break;
				}
				parse_ie.data_length--;
			}
			if ((parse_ie.data_length = MIN((sizeof(Parms.Dest)-1), parse_ie.data_length))) {
				memcpy (Parms.Dest, parse_ie.data, parse_ie.data_length);
				Parms.Dest[parse_ie.data_length] = 0;
			}
		}
	}

	/*
		Retrieve OAD
		*/
	{
		diva_mtpx_is_parse_t parse_ie;
		byte* first_oad = 0;
		int i;

		Parms.Orig[0]   = 0;
		Parms.Orig_1[0] = 0;

		for (i = 0; i < 2; i++) {
			memset (&parse_ie, 0x00, sizeof(parse_ie));
			parse_ie.wanted_ie	= OAD;
			parse_ie.element_after = first_oad;

			ParseInfoElement (Info,
												sizeInfo,
												(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
												&parse_ie);

			if (parse_ie.data != 0 && parse_ie.data_length > 1) {
				byte* dst = (i == 0)  ? &Parms.Orig[0]     : &Parms.Orig_1[0];
				int limit = ((i == 0) ? sizeof(Parms.Orig) : sizeof(Parms.Orig_1)) - 1;

				first_oad = parse_ie.ie;

				if (!(global_options & DIVA_ISDN_IGNORE_NUMBER_TYPE)) {
					switch (parse_ie.data[0] & 0x70) {
						case 0x10: /* internationsl */
							*dst++ = '0';
							limit--;
						case 0x20: /* national */
							*dst++ = '0';
							limit--;
							break;
					}
				}
				while (parse_ie.data_length) {
					if (*parse_ie.data++ & 0x80) {
						parse_ie.data_length--;
						break;
					}
					parse_ie.data_length--;
				}
				if ((parse_ie.data_length = MIN(limit, parse_ie.data_length))) {
					memcpy (dst, parse_ie.data, parse_ie.data_length);
					dst[parse_ie.data_length] = 0;
				}
			} else  {
				break;
			}
		}
	}

	/*
		Retrieve DSA
		*/
	{
		diva_mtpx_is_parse_t parse_ie;

		memset (&parse_ie, 0x00, sizeof(parse_ie));
		parse_ie.wanted_ie	= DSA;

		ParseInfoElement (Info,
											sizeInfo,
											(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
											&parse_ie);

		Parms.DestSub[0]		= 0;
		Parms.DestSubLength = 0;

		if (parse_ie.data && (parse_ie.data_length > 1)) {
			int length = MIN((sizeof(Parms.DestSub)-1), (parse_ie.data_length - 1));
			memcpy (Parms.DestSub, &parse_ie.data[1], length);
			Parms.DestSub[length] = 0;
			Parms.DestSubLength   = length;
		}
	}

	/*
		Retrieve ESC/CHI
		*/
	{
		diva_mtpx_is_parse_t parse_ie;

		memset (&parse_ie, 0x00, sizeof(parse_ie));
		parse_ie.wanted_ie        = CHI;
		parse_ie.wanted_escape_ie = 0x7f;

		ParseInfoElement (Info,
											sizeInfo,
											(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
											&parse_ie);


		if (parse_ie.data && (parse_ie.data_length != 0)) {
			Parms.selected_channel = parse_ie.data[0] & 0x1f;
		} else {
			Parms.selected_channel = 0;
		}
	}

	/*
		Retrieve OSA
		*/
	memset (&osa_parse_ie, 0x00, sizeof(osa_parse_ie));
	osa_parse_ie.wanted_ie	= OSA;

	ParseInfoElement (Info,
										sizeInfo,
										(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
										&osa_parse_ie);

	Parms.OrigSub[0]		= 0;
	Parms.OrigSubLength = 0;

	if (osa_parse_ie.data && (osa_parse_ie.data_length > 1)) {
		int length = MIN((sizeof(Parms.OrigSub)-1), (osa_parse_ie.data_length - 1));
		memcpy (Parms.OrigSub, &osa_parse_ie.data[1], length);
		Parms.OrigSub[length] = 0;
		Parms.OrigSubLength   = length;
	}

	/*
		Find BC
		*/
	memset (&bc_parse_ie, 0x00, sizeof(bc_parse_ie));
	bc_parse_ie.wanted_ie  = BC;
	ParseInfoElement (Info,
										sizeInfo,
										(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
										&bc_parse_ie);
	/*
		Find LLC
		*/
	memset (&llc_parse_ie, 0x00, sizeof(llc_parse_ie));
	llc_parse_ie.wanted_ie = LLC;
	ParseInfoElement (Info,
										sizeInfo,
										(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
										&llc_parse_ie);

	/*
		Find SIN
		*/
	memset (&sin_parse_ie, 0x00, sizeof(sin_parse_ie));
	ParseInfoElement (Info,
										sizeInfo,
										(ParseInfoElementCallbackProc_t)diva_mtpx_parse_sin_callback,
										&sin_parse_ie);

#if 0
	diva_getname (&elems[RING_INFO_SSEXTIE], CORNET_CMD_CALLING_PARTY_NAME,
	    &Parms.OrigName[0], sizeof(Parms.OrigName));
#endif

	/* service indicator (from 1TR6-indication or generated from DSS1-BC) */

	if (sin_parse_ie.data_length == 2) {
		Parms.Service    = sin_parse_ie.data[0];
		Parms.ServiceAdd = sin_parse_ie.data[1];
	}

	if (global_mode) { /* global_mode */
		C->Prot = global_mode & 0x0000ffff;
		C->Baud = 255 ;			/* maximum rate */

	} else if ((fixed_protocol = get_fixed_protocol(&Parms.Orig[0])) >= 0) {
		C->Prot = (byte)fixed_protocol;
		C->Baud = 255; /* maximum rate */
		DBG_TRC(("[%p:%s] s_ring_ind: Fixed Prot=%d", C, port_name(C), C->Prot))
	} else {						/* !global_mode */
#if D_PROTOCOL_DETECTION

	/* Try to detect the B-channel protocol from either the SIN or	*/
	/* or the BC/LLC info element. Because the IDI alway passes as	*/
	/* well the SIN as the BC element and only one of these is the	*/
	/* original D-channel element, we assume that the first element	*/
	/* is the original one.						*/

	C->Prot = ISDN_PROT_DET ;	/* user defined */
	C->Baud = 255 ;			/* maximum rate */

	if ((sin_parse_ie.data_length == 2) && (bc_parse_ie.data == 0)) {
		evalSIN (C, Parms.Service, Parms.ServiceAdd) ;

		DBG_TRC(("[%p:%s] s_ring_ind: SIN - Prot=%d Baud=%d", C, port_name(C), C->Prot, C->Baud))
	} else {
		int protocol_detected = 0;

		if (!(bc_parse_ie.data && bc_parse_ie.data_length)) {
			bc_parse_ie.data = "";
			bc_parse_ie.data_length = 0;
		}
		
		if (bc_parse_ie.data_length && (bc_parse_ie.data_length < sizeof(Parms.Bc))) {
			Parms.Bc[0] = (byte)bc_parse_ie.data_length;
			mem_cpy (&Parms.Bc[1], bc_parse_ie.data, bc_parse_ie.data_length);
		} else {
			Parms.Bc[0] = 0;
		}
		if (llc_parse_ie.data_length && (llc_parse_ie.data_length < sizeof(Parms.Llc))) {
			Parms.Llc[0] = (byte)llc_parse_ie.data_length;
			mem_cpy (&Parms.Llc[1], llc_parse_ie.data, llc_parse_ie.data_length);
		} else {
			Parms.Llc[0] = 0;
		}


		if (!(llc_parse_ie.data && llc_parse_ie.data_length)) {
			llc_parse_ie.data        = 0;
			llc_parse_ie.data_length = 0;
		}
		
		if (Parms.Bc[0] == piafs_32K_BC[0]) {
			int i;
			for (i = 0; i < piafs_32K_BC[0]; i++) {
				if (Parms.Bc[i+1] != piafs_32K_BC[i+1]) {
					break;
				}
			}
			if (i >= piafs_32K_BC[0]) {
				C->Prot = ISDN_PROT_PIAFS_32K;
				C->Baud = 7;
				DBG_TRC(("[%p:%s] detected PIAFS_32K", C, port_name(C)))
				protocol_detected = 1;
			}
		}
		if (!protocol_detected && (Parms.Bc[0] == piafs_64K_BC[0])) {
			int i;
			for (i = 0; i < piafs_64K_BC[0]; i++) {
				if (Parms.Bc[i+1] != piafs_64K_BC[i+1]) {
					break;
				}
			}
			if (i >= piafs_64K_BC[0]) {
				if (osa_parse_ie.data_length == piafs_64K_FIX_OrigSa[0]) {
					for (i = 0; i < piafs_64K_FIX_OrigSa[0]; i++) {
						if (osa_parse_ie.data[i] != piafs_64K_FIX_OrigSa[i+1]) break;
					}
				}
				if (i >= piafs_64K_FIX_OrigSa[0]) {
					C->Prot = ISDN_PROT_PIAFS_64K_FIX;
					DBG_TRC(("[%p:%s] detected PIAFS_64K fixed rate", C, port_name(C)))
					protocol_detected = 1;
				}
				if (!protocol_detected) {
					if (osa_parse_ie.data_length == piafs_64K_VAR_OrigSa[0]) {
						for (i = 0; i < piafs_64K_VAR_OrigSa[0]; i++) {
							if (osa_parse_ie.data[i] != piafs_64K_VAR_OrigSa[i+1]) break;
						}
					}
					if (i >= piafs_64K_FIX_OrigSa[0]) {
						C->Prot = ISDN_PROT_PIAFS_64K_VAR;
						DBG_TRC(("[%p:%s] detected PIAFS_64K variable rate", C, port_name(C)))
						protocol_detected = 1;
					}
				}
   		}
		}

		if (bc_parse_ie.data_length >= llc_parse_ie.data_length) {
			final_parse_ie = &bc_parse_ie;
		} else {
			final_parse_ie = &llc_parse_ie;
		}

		if (!protocol_detected) {
			evalLLC (C, final_parse_ie->data, final_parse_ie->data_length, &Parms.Service, &Parms.ServiceAdd);
		}

		DBG_TRC(("[%p:%s] s_ring_ind(%s): %s - Prot=%d Baud=%d",
							C, port_name(C), Parms.Dest,
							(final_parse_ie == &bc_parse_ie)? "BC" : "LLC", C->Prot, C->Baud))
	}

#else /* !D_PROTOCOL_DETECTION */

	/* Let the user define the B-Channel protocol, it's mostly not	*/
	/* possible to detect it correctly from SIN or BC/LLC.		*/
	/* The only exception is 56K HDLC which is always indicated by	*/
	/* a BC of 0x8890218f mapped to SIN 7/170 (0x07AA).		*/

	C->Prot	= ISDN_PROT_DET;
	C->Baud	= (Parms.Service == 7 && Parms.ServiceAdd == 170) ? 9 /*56K*/ : 255;

#endif /* D_PROTOCOL_DETECTION */

	} /* end !global_mode */

	Parms.Prot = C->Prot ;
	Parms.Baud = C->Baud ;


#if B_PROTOCOL_DETECTION
	C->DetProt = DET_PROT_OFF;
#endif /* B_PROTOCOL_DETECTION */
	C->State   = C_CALL_OFFER;
	C->NetUp   = 0;
	C->MdmUp = 0;
	C->detected_packet = 0;

	/* indicate call to upper layer */
	Parms.Mode = ISDN_CONN_MODE_DEFAULT ;

	if (( Port = Machine.Port->Ring (C, &Parms) ))
	{
 		attach_port (C, Port) ;

		C->State = C_CALL_ACCEPT;
		isdnSigEnq (C, CALL_ALERT, "", 1);
	    	return;
	}

	/* can't be serviced */

	switch (Parms.Cause) {

	case CAUSE_IGNORE:
	default		 : /* ignore call */
		if (ports_unavailable_cause) {
			/*
				The customer requested to reject the call with one special cause value
				to inform the switch/management about the fact that no application is able
				to process calls on this port
				*/
     	esc_cause[0] = ESC;		/* elem type	*/
     	esc_cause[1] = 3;		/* info length	*/
     	esc_cause[2] = CAU;		/* real type 	*/
     	esc_cause[3] = (0x80 | ports_unavailable_cause);
     	esc_cause[4] = 0;		/* 1tr6 cause	*/
			isdnSigEnq (C, REJECT, esc_cause, sizeof(esc_cause));
		} else {
			isdnSigEnq (C, HANGUP, "", 1);
		}
		break;

	case CAUSE_AGAIN : /* reject call */
            	esc_cause[0] = ESC;		/* elem type	*/
            	esc_cause[1] = 3;		/* info length	*/
            	esc_cause[2] = CAU;		/* real type 	*/
            	esc_cause[3] = 0x80 | Q931_CAUSE_UserBusy;
            	esc_cause[4] = 0;		/* 1tr6 cause	*/
		isdnSigEnq (C, REJECT, esc_cause, sizeof (esc_cause));
		break;
	}

	/* HANGUP or REJECT is acknowledged whith a HANGUP indication,	*/
	/* when this indication arrives we set up the listen again.	*/

	C->State = C_CALL_DISC_X;
}

static int isdnAccept (void *P, void *hC)
{
	ISDN_CHANNEL *C;

	if (!(C = (ISDN_CHANNEL *) hC) || C->Port != P) {
		DBG_ERR(("A: [%p:] Accept: link?, (%p)(%p)", C, P, C ? C->Port : 0))
		return 0;
	}
	if (C->State != C_CALL_OFFER) {
		DBG_ERR(("A: [%p:%s] Accept: state=%d ?", C, port_name(C), C->State))
		return 0;
	}
	if (isdnSigEnq (C, CALL_ALERT, "", 1)) {
		C->State = C_CALL_ACCEPT;
		isdnPutReq(C);
	}
	return 1;

	/* wait for Answer */
}

static int isdnAnswer (void *P, void *hC, word MaxFrame, T30_INFO *T30Info,
											 ISDN_CONN_PARMS*	Parms) {
	ISDN_PROT_MAP	*F;
	ISDN_CHANNEL	*C;
	byte		Prot, msg [270], *elem;
	int             bit_t_detection;
	unsigned char   Service;

	if (!(C = (ISDN_CHANNEL *) hC) || C->Port != P) {
		DBG_ERR(("A: [%p:] Answer: link?, (%p)(%p)", C, P, C ? C->Port : 0))
		return (0);
	}
	if (C->State != C_CALL_OFFER && C->State != C_CALL_ACCEPT) {
		DBG_ERR(("A: [%p:%s] Answer: state=%d ?", C, port_name (C), C->State))
		return (0);
	}

	/*--- set protocol and speed -------------------------------------------*/
	bit_t_detection = (Parms->bit_t_detection != 0);
	C->Prot = Parms->Prot ;
	C->Baud = Parms->Baud ;
	Service = Parms->Service;

	switch (Parms->bit_t_detection) {
		case BIT_T_DETECT_ON:
			/*
				Always use bit transparent detection procedure,
				independent from the signaled protocol
				*/
			C->Prot = ISDN_PROT_DET;
			C->Baud = 255;
			Service = 7;
			DBG_TRC(("[%p:%s] use forced BT detection", C, port_name (C)))
			break;

		case BIT_T_DETECT_ON_D:
			/*
				Use BT detection only if call is not analog modem
				*/
			if (C->Prot != ISDN_PROT_MDM_a) {
				C->Prot = ISDN_PROT_DET;
				C->Baud = 255;
				Service = 7;
				DBG_TRC(("[%p:%s] use forced digital BT detection", C, port_name (C)))
			}
			break;

		case BIT_T_DETECT_ON_S:
			/*
				Use BT detection only if it was not possible to determine the call type based on the
				signaling information
				*/
			DBG_TRC(("[%p:%s] use safe BT detection", C, port_name (C)))
			break;
	}
#if B_PROTOCOL_DETECTION
	if ( (C->Prot == ISDN_PROT_DET)
	  && (Service == 7) && (C->Features & DI_V120) ) {
		C->DetProt = DET_PROT_FIRST_FRAME;
		if (bit_t_detection) {
			DBG_TRC(("[%p:%s] s_ring_ind, use VOICE Detection", C, port_name (C)))
			C->Prot	   = ISDN_PROT_VOICE;
			C->transparent_detect = TRANSPARENT_DETECT_ON;
			diva_init_hdlc_rx_process (&C->hdlc_detector, C, C->Port);
			diva_init_v110_detector (C, C->Port, &C->v110_detector[0]);
			init_piafs_detectors (&C->piafs_detector[0], C, C->Port);
			C->transparent_detection_timeout = 0;
			C->last_detected_v110_rate = -1;
		} else {
			DBG_TRC(("[%p:%s] s_ring_ind, use RAW Detection", C, port_name (C)))
			C->Prot	   = ISDN_PROT_RAW;
			C->transparent_detect = TRANSPARENT_DETECT_OFF;
		}
	}
#endif /* B_PROTOCOL_DETECTION */

	Prot = T30Info ? ISDN_PROT_FAX : C->Prot;

 	for ( F = protmap ; F->Prot != Prot ; )
	{
		if ( ++F >= protmap + sizeof(protmap)/sizeof(protmap[0]) )
		{
			DBG_ERR(("A: [%p:%s] Answer: bad P=%d", C, port_name(C), Prot))
			return (0) ;
		}
	}

	if ( F->Feature && !(C->Features & F->Feature) )
	{
		DBG_TRC(("[%p:%s] Answer: %s (P=%d) not supported", C, port_name (C), F->Name, F->Prot))
		return (0) ;
	}

	if ( F->Prot == ISDN_PROT_FAX )
	{
		if ( (Prot = isdnKindFax (T30Info,(byte *)"Answer")) == ISDN_PROT_DET )
			return (0);

		/* Some fax programs stop receiving if they don't see	*/
		/* data for a longer period (WinFax 4.0 Lite).		*/
		/* Thus we force a data indication any 256 byte if the	*/
		/* 'MaxFrame' is 0 (which would default to 2048).	*/
		/* A nonzero 'MaxFrame' can be set only via AT+iL<n>	*/

		if (MaxFrame == 0) MaxFrame = 256 ;
	}

	/*
		Store information that can be eventually used by the detection protocol
		*/
	mem_cpy (&C->original_conn_parms, Parms, sizeof(*Parms));
	C->original_conn_parms.MaxFrame = MaxFrame;

	/* first assign the network layer info */

	if (!netassign (C, Prot, F, T30Info, 0 /*in*/, Parms)) return (0);

	/* then enqueue call response */

	elem = msg;
	put_idi_user_id (&elem, get_port_name (P));
	putcai (&elem, F, C->Baud, MaxFrame, &Parms->modem_options);

	DBG_TRC(("[%p:%s] Answer: sig Id=%04x", C, port_name (C), WIDE_ID(C->Sig)))
#if 0
	DBG_BLK((msg, elem - msg))
#endif


	if (!isdnSigEnq (C, CALL_RES, msg, (word) (elem - msg))) return (0);

	C->State = C_CALL_ANSWER;

	isdnPutReq (C);

	return (1);

	/* wait for CALL_IND */
}

static void s_call_ind (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* incoming call connected now */


	if (C->State != C_CALL_ANSWER) {
		DBG_ERR(("A: [%p:%s] s_call_ind: state=%d?", C, port_name (C), C->State))
		shut_link (C, CAUSE_STATE);
		return;
	}

	DBG_TRC(("[%p:%s] s_call_ind, state=%d, prot=%d", C, port_name (C), C->State, C->Prot))

	if (C->NetUp) {
		/* network layer already connected (see n_conn_ind below) */
		DBG_TRC(("[%p:%s] NetIsUp?", C, port_name (C)))
		isdnUp (C, (byte *)"s_call_ind");
		return;
	}

	C->State = C_CALL_UP;

	if (C->Prot == ISDN_PROT_EXT_0) {
		/* there is no network layer for external devices */
		isdnUpData (C, (byte *)"s_call_ind[EXT]", 0);
		return;
	}

	/* wait for N_CONNECT */
}

static void n_conn_ind (ISDN_CHANNEL *C, byte *Parms, word sizeParms)
{ /* connect indication from network layer entity */

	/*
	 * Usally we see this indication only after an incoming call was
	 * connected (see above in s_call_ind()).
	 * On long distance calls it may happen that the B-Channel is up
	 * before the D-channel procedure is completed and that the remote
	 * party starts the network connect procedure very soon.
	 * Thus it is possible to get this indication before the D-channel
	 * procedure is completed.
	 * This was seen first when connecting from Germany to CompuServe
	 * in USA whith V.120.
	 */

	if (C->Port) switch (C->State) {
		case C_CALL_DIAL  :
		case C_CALL_ANSWER:
		case C_CALL_UP	  :
		case C_DATA_CONN_X:
		case C_DATA_WAIT  :
		case C_DATA_XFER  :

			if (isdnNetCtlEnq (C, N_CONNECT_ACK, 0, 0)) {
				C->NetUp = 1;
				if (C->State == C_CALL_UP) {
					isdnUp (C, (byte *)"n_conn_ind");
				} else if ( (C->State == C_DATA_WAIT)
				         && (C->DetProt == DET_PROT_REASSIGN) ) {
					ISDN_CONN_INFO Info ;
					C->DetProt = DET_PROT_OFF ;

					if (C->transparent_detect == TRANSPARENT_DETECT_INFO_VALID) {
						mem_cpy (&Info, &C->MdmInfo, sizeof(Info));
						DBG_TRC(("[%p:%s] n_conn_ind, use transparent info", C, port_name(C)))
					} else {
						Info.Prot	  = C->Prot;
						Info.Mode	  = ISDN_CONN_MODE_DEFAULT;
						Info.MaxFrame = (C->Prot == ISDN_PROT_V120) ?
										 ISDN_MAX_V120_FRAME : ISDN_MAX_FRAME ;
						Info.RxSpeed  =
						Info.TxSpeed  = (C->Baud == 9)? 56000 : 64000 ;
						Info.ConNorm  = 0 ;
						Info.ConOpts  = 0 ;
					}

					if (!(C->Prot == ISDN_PROT_PIAFS_64K_VAR	||
			 				 C->Prot == ISDN_PROT_PIAFS_64K_FIX	||
			 				 C->Prot == ISDN_PROT_PIAFS_32K)) {
						isdnUpData (C,
												(C->Prot == ISDN_PROT_V120) ?  "n_conn_ind[V.120 DET]" : "n_conn_ind[DET]",
												&Info);
					}
				}
				return;
			}

		case C_DOWN	  :
		case C_INIT	  :
		case C_IDLE	  :
		case C_CALL_OFFER :
		case C_CALL_ACCEPT:
		case C_CALL_DISC_X:
		case C_DATA_DISC_X:

			break;
	}

	DBG_TRC(("[%p:%s] n_conn_ind: state?", C, port_name(C)))
	shut_link (C, CAUSE_FATAL);
}

static int isdnDrop (void *P, void *hC, byte Cause, byte Detach, byte AdditionalCause)
{ /* handle drop request from upper layer */

	ISDN_CHANNEL *C;

	if (!(C = (ISDN_CHANNEL *) hC) || C->Port != P) {
		DBG_ERR(("A: [%p:] Drop: link? (%p)(%p)", C, P, C ? C->Port : 0))
		return (1);
	}

	if (Detach) {
		DBG_TRC(("[%p:%s] Drop DetachPortDirectly, State=%d Cause=%02x",
							C, port_name(C), C->State, AdditionalCause))
		detach_port (C) ; /* detach port immediately */
	}

	switch (C->State) {
	case C_DOWN	   :
	case C_INIT	   :
	case C_IDLE	   :
		/* nothing to drop */
		DBG_TRC(("[%p:%s] Drop Detach Port on IDLE, nothing to drop", C, port_name(C)))
		detach_port (C) ;
		return (1); /* channel idle */

	case C_DATA_CONN_X :
	case C_DATA_WAIT   :
	case C_DATA_XFER   :
		/*
		 * In case of FAX CLASS2 we _must_ disconnect via N_DISC and
		 * then wait for N_DISC_ACK because the call should be
		 * disconnected only after the T30 disconnect frame was
		 * received by the remote party whith a relatively high
		 * probability (which is indicated by the N_DISC_ACK).
		 *
		 * In case of V.42 modem connections it's better to disconnect
		 * orderly because otherwise the remote modem will not hang up
		 * for a long time because it waits for the layer 2 disconnect.
		 */
		C->RequestedDisconnectCause = AdditionalCause;
		if ((C->Net.Id != NL_ID) && (Cause != CAUSE_SHUT) && (C->channel_assigned)
		  /* && (   C->Prot == ISDN_PROT_FAX_CLASS2
		      || C->Prot == ISDN_PROT_MDM_V42) BLA HACK NEW: do it always!!! */ ) {
			C->State = C_DATA_DISC_X;
			disc_net (C, N_DISC);
			isdnPutReq (C);
			/* wait for N_DISC_ACK, then disconnect call */
			break;
		}
		/*
		 * For all other protocols we don't disconnect the network
		 * layer orderly because there are some problems in disconnect
		 * handling whith transparent layer 2/3 B-channel protocols.
		 * layer orderly .
		 * Guntram says it's better to remove the network entitity to
		 * keep the things running and as long as we don't support
		 * multiple X25 connections no problems will arise from this.
		 * Accordingly we do it brute force, remove the network entity
		 * and disconnect the call (done in disc_link() below).
		 */

	case C_CALL_DIAL   :
	case C_CALL_OFFER  :
	case C_CALL_ACCEPT :
	case C_CALL_ANSWER :
	case C_CALL_UP     :
		/* remove the network entity (if any) and disconnect call */
		C->RequestedDisconnectCause = AdditionalCause;
		disc_link (C, Cause, C->State == C_CALL_ACCEPT);
		isdnPutReq (C);
		break;

	case C_DATA_DISC_X :
	case C_CALL_DISC_X :
		/* let the machine run as expected */
		DBG_TRC(("[%p:%s] DROP: ALREADY DISCONNECTING", C, port_name(C)))
		break;
	}


	return (0);	/* channel not idle yet */
}

static int isdnEnq (ISDN_CHANNEL *C,
										byte Kind,
										byte Type,
										byte *Parms,
										word sizeParms) {
	MSG_QUEUE*	pQ;
	ISDN_MSG *M;
	word sizeEscUid;

	sizeEscUid = 0;
	switch (Kind) {
		case ISDN_SIG_REQ:
			if (C->EscUid[1] > 1) {
				sizeEscUid = C->EscUid[1] + 2;
			}
			pQ = &C->Squeue;
			break;

		default:
			pQ = &C->Xqueue;
	}

	if (!Parms) {
		sizeParms = 0;
	}

	if (!(M=(ISDN_MSG*)queueAllocMsg (pQ, (word)(sizeof(*M) + sizeParms + sizeEscUid)))) {
		return (0);
	}

	M->Channel = C;
	M->Age	   = C->Age;
	M->Kind	   = Kind;
	M->Type	   = Type;
	if (sizeParms) {
		mem_cpy (M->Parms, Parms, sizeParms);
	}
	if (sizeEscUid) {
		mem_cpy (M->Parms + sizeParms, C->EscUid, sizeEscUid);
	}
	queueCompleteMsg ((byte *) M);


	return (1);
}

static int isdnSigEnq (ISDN_CHANNEL *C,
											 byte Type,
											 byte *Parms,
											 word sizeParms) {
	int ret;
#if IDI_does_not_check_XNum_and_Plength
	if ( !Parms || !sizeParms )
	{
		Parms = (byte *)"" ;
		sizeParms = 1 ;
	}
#endif

	if (!(ret = isdnEnq (C, ISDN_SIG_REQ, Type, Parms, sizeParms))) {
		DBG_ERR(("A: [%p:%s] Squeue overrun", C, port_name(C)))
	}

	return (ret);
}

static int isdnNetCtlEnq (ISDN_CHANNEL *C,
													byte Type,
													byte *Parms,
													word sizeParms) {
	int ret;
#if IDI_does_not_check_XNum_and_Plength
	if ( !Parms || !sizeParms )
	{
		Parms = (byte *)"" ;
		sizeParms = 1 ;
	}
#endif
	if (!(ret=isdnEnq (C, ISDN_NET_CTL_REQ, Type, Parms, sizeParms))) {
		DBG_ERR(("A: [%p:%s] Xqueue overrun", C, port_name(C)))
	}

	return (ret);
}

static void isdnPutReqDpc (ISDN_CHANNEL *C) {
	/* sysScheduleDpc (&C->put_req_dpc); */
}

static void isdnPutReq (ISDN_CHANNEL *C) {
	ISDN_ADAPTER *A ;
	ISDN_MSG *M;
	word sizeParms;

	A = C->Adapter ;

	if ((A->EventLock&A_PROCESSING_EVENTS)||(A->EventLock&A_SENDING_REQUESTS)) {
		return ;
	}
	A->EventLock |= A_SENDING_REQUESTS ;

/*
		Processing Signaling Events
	*/
	while ((!C->sig_busy) &&
				 (M=(ISDN_MSG*)queuePeekMsg(&C->Squeue,&sizeParms))) {
		if (M->Channel != C) {
			DBG_ERR(("A: [%p:%s] PutReq: sQ linkage", C, port_name(C)))
			queueFreeMsg (&C->Squeue);
		} else if (M->Kind == ISDN_SIG_REQ) {
			if ((M->Type == REMOVE) && (C->Net.Id != NL_ID)) {
				/*
					Delay remove request till Networking entity was cleaned up
					*/
				break;
			}

			if (divas_tty_debug_sig) {
				if (C->Sig.Id != DSIG_ID) {
					DBG_TRC(("[%p:%s] PutSigReq=%s REQ(%d) Id=%04x",\
											 C, port_name(C),
											 sig_name (M->Type, 1), M->Type, WIDE_ID(C->Sig)))
				} else {
					DBG_TRC(("[%p:%s] PutSigReq=%s REQ(%d) Id=%04x",\
											 C, port_name(C),
											 "S->ASSIGN", M->Type, WIDE_ID(C->Sig)))
				}
			}

			C->Sig.Req = M->Type;
			if (sizeParms -= sizeof (*M)) {
				C->Sig.XNum = 1;
				C->SigXdesc.P = M->Parms;
				C->SigXdesc.PLength = sizeParms;
			} else {
				C->Sig.XNum = 0;
			}
			C->sig_busy = 1;
			C->RequestFunc (&C->Sig); 
			break;
		} else {
			DBG_ERR(("A: [%p:%s] PutReq: sQ garbled",
									 C, port_name(C)))
			queueFreeMsg (&C->Squeue);
		}
	}

/*
		Processing Networking Events
	*/
	M=(ISDN_MSG*)queuePeekMsg(&C->Xqueue,&sizeParms);

	if (M && !C->net_busy && (C->net_rc == 1) && C->NetFC &&
			(M->Kind == ISDN_NET_CTL_REQ) && (M->Type == REMOVE) &&
			!C->net_disc_ind_pending &&
			((C->Net.Req == N_DISC) || (C->Net.Req == N_DISC_ACK))) {
		DBG_ERR(("A: [%p:%s] REMOVE in OK_FC", C, port_name(C)))
		goto fix_net_remove;
	}


	while ((!C->net_busy) && (!C->net_rc) &&
					(M=(ISDN_MSG*)queuePeekMsg(&C->Xqueue,&sizeParms))) {
fix_net_remove:
		if (M->Channel != C) {
			DBG_ERR(("A: [%p:%s] PutReq: xQ linkage",
									 C, port_name(C)))
			queueFreeMsg (&C->Xqueue);
		} else if ((M->Kind == ISDN_NET_CTL_REQ) ||
							 (M->Kind == ISDN_NET_DATA_REQ)) {

			if ((C->Net.Id == NL_ID) && (M->Type != ASSIGN)) {
				queueFreeMsg (&C->Xqueue);
				continue;
			}

			if ((M->Kind == ISDN_NET_DATA_REQ) && C->NetFC) {
				break; /* flow control */
			}
			if (M->Kind == ISDN_NET_CTL_REQ) {
				if (M->Type == N_DISC) {
					C->net_disc_ind_pending = 1;
				}
				if ((M->Type == REMOVE) && C->net_disc_ind_pending) {
					break;
				}
			}

			C->net_rc++;
			if (C->NetFC) {
				C->net_rc++;
			}

			if (divas_tty_debug_net_data ||
					(divas_tty_debug_net && (M->Type != N_DATA))) {
				if (C->Net.Id != NL_ID) {
					DBG_TRC(("[%p:%s] PutNetReq=%s REQ Id=%04x",\
											 C, port_name(C),
											 net_name (M->Type), WIDE_ID(C->Net)))
				} else {
					DBG_TRC(("[%p:%s] PutNetReq=%s REQ Id=%04x",\
											 C, port_name(C),
											 "N-ASSIGN", WIDE_ID(C->Net)))
				}
			}

			if (C->Net.Id == NL_ID) {
				C->in_assign = 1;
			}

			C->Net.Req = M->Type;
			if (sizeParms -= sizeof (*M)) {
				C->Net.XNum = 1;
				C->NetXdesc.P = M->Parms;
				C->NetXdesc.PLength = sizeParms;
			} else {
				C->Net.XNum = 0;
			}
			C->net_busy = 1;
			C->RequestFunc (&C->Net);
			break;
		} else {
			DBG_ERR(("A: PutReq: [%p:%s] xQ garbled", C, port_name(C)))
			queueFreeMsg (&C->Xqueue);
		}
	}

	A->EventLock &= ~A_SENDING_REQUESTS ;
}

/*
	handle indications from signalling entity
	*/
static void isdnSigInd (ISDN_CHANNEL *C, ISDN_MSG *M, word sizeParms) {
	switch	(M->Type) {
		case CALL_CON:   /* outgoing call connected	*/
			s_call_con (C, M->Parms, sizeParms);
			break;

		case INDICATE_IND:   /* incoming call detected		*/
			s_ring_ind (C, M->Parms, sizeParms);
			break;

		case CALL_IND:   /* incoming call connected	*/
			s_call_ind (C, M->Parms, sizeParms);
			break;

		case HANGUP:   /* hangup indication		*/
			s_hup_ind (C, M->Parms, sizeParms);
			break;

		case INFO_IND:   /* INFO indication		*/
#if HANDLE_EARLY_CONNECT
			s_info_ind (C, M->Parms, sizeParms);
#endif /* HANDLE_EARLY_CONNECT */
			break;	/* currently ignored */

	/*
		these indications are not expected and thus ignored
		*/
	case SUSPEND      :   /* call suspend confirm		*/
	case RESUME       :   /* call resume confirm		*/
	case SUSPEND_REJ  :   /* suspend rejected indication	*/
	case USER_DATA    :   /* user to user signaling data	*/
	case CONGESTION   :   /* network congestion indication	*/
	case TEL_CTRL     :   /* Telephone control indication	*/
	case FAC_REG_ACK  :   /* fac registration acknowledge	*/
	case FAC_REG_REJ  :   /* fac registration reject	*/
		DBG_ERR(("A: [%p:%s] SigInd: %x ?", C, port_name(C), M->Type))
		break;
	}
}

/*
		handle the return code for the last started signalling reques
	*/
static void isdnSigRc (ISDN_CHANNEL *C, ISDN_MSG *M) {
	/* Look if this is the Rc for the initial ASSIGN request for	*/
	/* the signalling entity !					*/
	/* If so and if the Rc is ASSIGN_OK set channel State C_IDLE	*/
	/* to indicate that we can start normal work.			*/
	/*								*/
	/* Unfortunately ASSIGN has the same coding as CALL_REQ and	*/
	/* thus it cannot be handled in the switch() below.		*/

	if ((C->State == C_INIT) && (M->Type == ASSIGN) && (M->Rc == ASSIGN_OK)) {
		C->State = C_IDLE;
		if (Machine.Listen) {
			isdnSigEnq (C, INDICATE_REQ, "", 1);
		}
		return;
	}
	/*
		for now we simply ignore any errors
		*/
	if (M->Rc != OK) {
		DBG_ERR(("A: [%p:%s] SigRc=%02x", C, port_name(C), M->Rc))
		s_hup_ind (C, "", 1);
		return;
	}

	switch (M->Type) {
		case REMOVE:
			/*
				final deassign, mark channel, set signal
				*/
			C->sig_remove_sent = 0;
			C->sig_hup_rnr = 0;
			C->seen_hup_ind = 0;
			C->Sig.Id = DSIG_ID;
			C->Sig.ReqCh = 0;
			C->Sig.IndCh = 0;
			C->Sig.RcCh = 0;
			C->State  = C_INIT;
			C->MdmUp = 0;
			C->transparent_detect = 0;
			if (!diva_tty_shutdown) {
				sigassign (C);
			}
# if TRY_TO_QUIESCE
		if (Machine.State == M_SHUT) {
			DBG_TRC(("[%s] SigRc: unblock", C->Id))
			sysPostEvent (&Machine.QuiesceEvent);
		}
# endif /* TRY_TO_QUIESCE */
		break;

	case HANGUP       :   /* hangup request				*/
		break;

	/*
		for now we silently ignore all the other Rc's
		*/
	case CALL_REQ     :   /* call request				*/
	case LISTEN_REQ   :   /* listen request				*/
	case SUSPEND      :   /* call suspend request			*/
	case RESUME       :   /* call resume request			*/
	case USER_DATA    :   /* user data for user to user signaling	*/
	case INDICATE_REQ :   /* request to indicate an incoming call	*/
	case CALL_RES     :   /* accept an incoming call		*/
	case CALL_ALERT   :   /* send ALERT for incoming call		*/
	case INFO_REQ     :   /* INFO request				*/
	case REJECT       :   /* reject an incoming call		*/
	case RESOURCES    :   /* reserve B-Channel hardware resources   */
	case TEL_CTRL     :   /* Telephone control request		*/
	case STATUS_REQ   :   /* Request D-State (returned in INFO_IND)	*/
	case FAC_REG_REQ  :   /* connection idependent fac registration	*/
	case CALL_COMPLETE:   /* send a CALL_PROC for incoming call	*/
	default		  :
		break;
	}
}

#if B_PROTOCOL_DETECTION
/* -------------------------------------------------------------------
    Return:
             0 : X.75
             1 : X.75+V.42
   ------------------------------------------------------------------- */
static int isdnDetX75v42 (ISDN_CHANNEL* C, byte *pXIDFrame, word lenXID) {
  word grpLen;

  if (!lenXID) {
    DBG_TRC(("[%p:%s] Empty XID received!", C, port_name(C)))
    return (0);
  }
  DBG_TRC(("[%p:%s] XID received, length:%d", C, port_name (C), lenXID))
  while(lenXID >= 2)  /* parse XID until there is nothing left */
  {
    lenXID--;
    switch(*pXIDFrame++)
    {
      case XIDGI_PN:   /* parameter negotion group ID found */

        DBG_TRC(("[%s] parse XID - parameter negotion group ID found", C->Id))
        grpLen = GET_STREAM_WORD(pXIDFrame);   /* read  group length */
        if(!grpLen) return (0);

        pXIDFrame += 2;
        lenXID -= 2;
        if(lenXID >= grpLen)  /* is the frame long enough 
                                 (corresp. to the group length) */
        {
          lenXID -= grpLen ;
          pXIDFrame += grpLen ;
        }
        break ;
      case XIDGI_UD: /* user data subfield ID found */

        DBG_TRC(("[%s] parse XID - user data subfield ID found", C->Id))
        grpLen = GET_STREAM_WORD(pXIDFrame);  /* read  group length;
                                           the standard states that 
                                           there is no group length 
                                           in the user data subfield;
                                           it is impl. by TELiNDUS */
        if(!grpLen) return (0);

        pXIDFrame += 2 ;
        lenXID -= 2 ;
        if(lenXID >= grpLen)  /* is the frame long enough 
                                 (corresp. to the group length) */
        {
          lenXID -= grpLen ;
          pXIDFrame +=grpLen;
        }
        break ;
      case XIDGI_PPN: /* private parameter negotion group ID found */

        DBG_TRC(("[%s] parse XID - private parameter negotion group ID found", C->Id))
        return (1); /* FOUND */
      default:
        DBG_TRC(("[%s] parse XID - unknown group ID", C->Id))
        grpLen = GET_STREAM_WORD(pXIDFrame);  /* read  group length;
                                           the standard states that 
                                           there is no group length 
                                           in the user data subfield;
                                           it is impl. by TELiNDUS */
        if(!grpLen) return (0);

        pXIDFrame += 2 ;
        lenXID -= 2 ;
        if(lenXID >= grpLen)  /* is the frame long enough 
                                 (corresp. to the group length) */
        {
          lenXID -= grpLen ;
          pXIDFrame +=grpLen;
				}
        break;
    }
  }

	return (0);
}

static int isdnDetProt (ISDN_CHANNEL *C, byte *Data, word sizeData)
{
	ISDN_PROT_MAP	*F;
	ISDN_CONN_INFO	Info ;
	byte		Prot;

	/*
		Try to detect HDLC Frame
		*/
	if (C->transparent_detect) {
		word i, oLength = 0;
		int detected_idi_prot;
		dword detected_idi_speed, detected_idi_rate;
		byte* pData = Data, *oData = 0;
		for (i = 0; i < sizeData; i++, pData++) {
			if ((oData = diva_hdlc_rx_process (&C->hdlc_detector, *pData, &oLength))) {
				break;
			}
		}
		if (oData) {
			Data = oData;
			sizeData = oLength;
			DBG_TRC(("[%p:%s] HDLC(%d) detected after %ld bytes", C, port_name(C), sizeData,
								C->transparent_detection_timeout))
			DBG_BLK((Data, MIN(sizeData,64)))
		} else if ((detected_idi_prot = piafs_detector_process(&C->piafs_detector[0],
												 Data, sizeData, &detected_idi_speed, &detected_idi_rate))) {
			C->Baud = (byte)detected_idi_rate;
			Prot    = (byte)detected_idi_prot;

			C->transparent_detect = TRANSPARENT_DETECT_INFO_VALID;
			C->MdmInfo.Prot = Prot;
			C->MdmInfo.Mode = C->original_conn_parms.Mode;
			C->MdmInfo.MaxFrame = C->original_conn_parms.MaxFrame;
			C->MdmInfo.RxSpeed  = \
			C->MdmInfo.TxSpeed  = detected_idi_speed;
			C->MdmInfo.ConNorm  = 0 ;
			C->MdmInfo.ConOpts  = 0 ;

			DBG_TRC(("[%p:%s] PIAFS(%d) detected after %ld bytes", C, port_name(C),
								detected_idi_speed, C->transparent_detection_timeout))

			goto reassign ;
		} else {
			dword detected_rate;
			int rate = diva_v110_detector_process (&C->v110_detector[0], Data, sizeData,
																						 &detected_rate,
																						 &C->last_detected_v110_rate);
			if (rate) {
				C->Baud = (byte)rate;
				Prot = ISDN_PROT_V110_a;
				C->transparent_detect = TRANSPARENT_DETECT_INFO_VALID;
				C->MdmInfo.Prot = Prot;
				C->MdmInfo.Mode = C->original_conn_parms.Mode;
				C->MdmInfo.MaxFrame = C->original_conn_parms.MaxFrame;
				C->MdmInfo.RxSpeed  = \
				C->MdmInfo.TxSpeed  = detected_rate;
				C->MdmInfo.ConNorm  = 0 ;
				C->MdmInfo.ConOpts  = 0 ;

				DBG_TRC(("[%p:%s] V110(%d) detected after %ld bytes", C, port_name(C),
								detected_rate, C->transparent_detection_timeout))

				goto reassign ;
			} else {
				C->transparent_detection_timeout += sizeData;
				if (C->transparent_detection_timeout > \
						((DIVA_TRANSPARENT_DETECTION_TO_MIN)*(8*1024*60)+V110_DETECTION_DELAY)) {
					DBG_TRC(("[%p:%s] bittp detection to", C, port_name(C)))
					s_hup_ind (C, "", 1);
				}
				return (1);
			}
		}
	}

	/* check for X.75v42bis XDI */
	if (sizeData > 4) {
		if ((Data[1] == 0xAF) ||
				(Data[1] == (0xAF | 0x10))) {
      if (isdnDetX75v42 (C, &Data[2], (word)(sizeData-2))) {
          DBG_TRC(("[%p:%s] X75V42 Detected", C, port_name(C)))
			    Prot = ISDN_PROT_X75V42 ;
			    goto reassign ;
      }
    }
	}

	/* check for X.75 frame (0x01 0x3F) */
	if ( sizeData >= 2 && Data[0] == 0x01 )
	{
		switch (Data[1])
		{
		case 0x3F:	/* typical X.75 frame */
			Prot = ISDN_PROT_X75 ;
			goto reassign ;
		case 0xAF:	/* proprietary ISDN modem handshake */
			return (-1) ;
		}
	}

	/* check for V.120 frame at 64 kbit/s (0x08 0x01 0x7F) */

	if ( sizeData >= 3 && Data[0] == 0x08 && Data[1] == 0x01 )
	{
		switch (Data[2])
		{
		case 0x7F:	/* typical V.120 frame */
			Prot = ISDN_PROT_V120 ;
			goto reassign ;
		case 0xAF:	/* proprietary ISDN modem handshake */
			return (-1) ;
		}
	}

	if (C->transparent_detect) {
		/*
			In case simple HDLC frame was detected then we change to the HDLC
			from the Transpatent protocol used for the HDLC detection
			*/
		Prot = ISDN_PROT_RAW;
		C->transparent_detect = TRANSPARENT_DETECT_INFO_VALID;

		C->MdmInfo.Prot = ISDN_PROT_RAW;
		C->MdmInfo.Mode = pppSyncLcpFrame (Data, sizeData) ? \
																ISDN_CONN_MODE_SYNCPPP : ISDN_CONN_MODE_DEFAULT;
		C->MdmInfo.MaxFrame = ISDN_MAX_FRAME;
		C->MdmInfo.RxSpeed  =
		C->MdmInfo.TxSpeed  = (C->Baud == 9) ? 56000 : 64000;
		C->MdmInfo.ConNorm  = 0 ;
		C->MdmInfo.ConOpts  = 0 ;

		C->detected_packet        = Data;
		C->detected_packet_length = sizeData;

		goto reassign ;
	}

	/* something different, check if it is sync PPP */

	C->DetProt = DET_PROT_OFF ;

	Info.Prot	  = ISDN_PROT_RAW ;
	Info.Mode	  = pppSyncLcpFrame (Data, sizeData)?
					ISDN_CONN_MODE_SYNCPPP : ISDN_CONN_MODE_DEFAULT ;
	Info.MaxFrame = ISDN_MAX_FRAME ;
	Info.RxSpeed  =
	Info.TxSpeed  = (C->Baud == 9)? 56000 : 64000 ;
	Info.ConNorm  = 0 ;
	Info.ConOpts  = 0 ;
	
	isdnUpData (C, (Info.Mode == ISDN_CONN_MODE_SYNCPPP) ?
					"NetDataInd[PPP]" : "NetDataInd[HDLC]",
				&Info) ;


	return (0) ;

reassign:
	/* remember reassigning */

	C->DetProt = DET_PROT_REASSIGN ;

	/* get the correct network layer settings */

 	for ( F = protmap ; F->Prot != Prot ; F++ ) ;

	/* flush the queue, enqueue the REMOVE for the network entity  */

	disc_net (C, REMOVE) ;

	if (C->transparent_detect) {
		resources (C, Prot);
	}

	/* enqueue new protocol assigment (cannot fail, queue flushed) */

	(void) netassign (C, Prot, F, 0, 0, 0) ;

	/* forget the data frame */

	return (1) ;
}

#endif /* B_PROTOCOL_DETECTION */
/*
	handle data indications from network layer
	*/
static void isdnNetDataInd (ISDN_CHANNEL *C, ISDN_MSG *M, word sizeParms) {
	byte *Data;
	switch	(M->Type & ~(N_Q_BIT | N_M_BIT | N_D_BIT)) {
	case N_UDATA	  :	/* OSI D-UNIT-DATA IND		*/

		if (  C->Port
		  && (C->Prot == ISDN_PROT_MDM_RAW || C->Prot == ISDN_PROT_MDM_V42) )
		{
			/* DCD_ON/OFF or CTS_ON/OFF indication come via UDATA, 	*/
			/* on DCD_ON we signal port up, other indications are	*/
			/* silently discarded.									*/
			/* DCD_ON packet format as defined by TM:				*/
			/*		<word> time of DCD on 							*/
			/*		<byte> connected norm							*/
			/*		<word> connected options						*/
			/*		<dword> connected speed (bit/s, max of tx/rx)	*/
			/*		<word> roundtrip delay (ms)						*/
			/*		<dword> connected speed tx (bit/s)				*/
			/*		<dword> connected speed rx (bit/s)				*/

			if ((!C->MdmUp) && sizeParms && M->Parms[0] == 2/*DCD_ON*/) {
				C->MdmInfo.Prot	  = C->ReqProt ; /* return the original value */
				C->MdmInfo.Mode	  = ISDN_CONN_MODE_DEFAULT ;

				C->MdmInfo.MaxFrame = C->original_conn_parms.MaxFrame;

				if ( sizeParms < 10 )
				{
					C->MdmInfo.RxSpeed =
					C->MdmInfo.TxSpeed = 14400 ; /* a wild guess */
					C->MdmInfo.ConNorm = 0 ;
					C->MdmInfo.ConOpts = 0 ;
				}
				else
				{
					if ( sizeParms < 20 )
					{	/* no seperate rx/tx speed available */
						C->MdmInfo.RxSpeed =
						C->MdmInfo.TxSpeed = *((dword *)&M->Parms[6]) ;
					}
					else
					{
						C->MdmInfo.RxSpeed = *((dword *)&M->Parms[16]) ;
						C->MdmInfo.TxSpeed = *((dword *)&M->Parms[12]) ;
					}
					C->MdmInfo.ConNorm = M->Parms[3] ;
					C->MdmInfo.ConOpts = *((word *)&M->Parms[4]) ;
				}

				C->MdmUp = 1;

				DBG_TRC(("[%p:%s] Modem Norm = %d", C, port_name(C), C->MdmInfo.ConNorm))

				if (C->State == C_DATA_WAIT) {
					isdnUpData (C, "NetDataInd[DCD]", &C->MdmInfo) ;
				}
			}
			break ;
		} else if (C->Prot == ISDN_PROT_PIAFS_64K_VAR	||
			 				 C->Prot == ISDN_PROT_PIAFS_64K_FIX	||
			 				 C->Prot == ISDN_PROT_PIAFS_32K) {
			if ((C->State == C_DATA_WAIT)	&&
					(sizeParms >= 6)					&&
					(M->Parms[0] == 0x80)) {
				ISDN_CONN_INFO Info ;

				Info.Prot	  = C->ReqProt ; /* return the original value */
				Info.Mode	  = ISDN_CONN_MODE_DEFAULT ;
				Info.MaxFrame = ISDN_MAX_FRAME;
				Info.RxSpeed = \
				Info.TxSpeed = READ_WORD(((byte*)&M->Parms[1]));
				Info.ConNorm = (byte)M->Parms[3];
				Info.ConOpts = 0 ;

				DBG_TRC(("[%p:%s] Modem Norm = %d", C, port_name(C), Info.ConNorm))

				isdnUpData (C, "NetDataInd[DCD]", &Info) ;
			}
			break;
		}

	case N_EDATA	  :	/* OSI N-EXPEDITED DATA	IND	*/
	case N_BDATA	  :	/* BROADCAST-DATA IND		*/
	case N_DATA	  :	/* OSI N-DATA IND		*/
		if (!C->Port) {
			DBG_ERR(("A: [%p:?] NetDataInd: port?", C))
			break;
		}
		if (C->State != C_DATA_XFER) {
#if B_PROTOCOL_DETECTION
			if ( C->State == C_DATA_WAIT
			  && C->DetProt == DET_PROT_FIRST_FRAME )
			{
				if ( isdnDetProt (C, M->Parms, sizeParms) )
				{  /* detected either X.75 or V.120, the net  */
				   /* layer has to be reassigned and thus the */
				   /* current frame is simply discarded.      */
					break ;
				}
 				/* no layer 2 protocol detected, an 'up' was  */
				/* indicated to upper layer, pass through the */
				/* frame now				      */
			}
			else
#endif /* B_PROTOCOL_DETECTION */
			{
				DBG_ERR(("A: [%p:%s] NetDataInd: state ? State=%d, Prot=%d",\
											C, port_name (C), C->State, C->DetProt))
				break;
			}
		}

		Data = M->Parms;
		if (C->Prot == ISDN_PROT_V120 && sizeParms) {
			/* strip off terminal adaption header */
			while (--sizeParms && !(*Data++ & 0x80))
				; /* stopped when 'Ext' bit was 1 */
		}
		if (sizeParms) {
			Machine.Port->Recv (C, C->Port,
					    Data, sizeParms, M->Type);
		}
		break;
	}
}

/*
	handle link control indications from network layer
	*/
static void isdnNetCtlInd (ISDN_CHANNEL *C, ISDN_MSG *M, word sizeParms) {
	switch	(M->Type & ~(N_Q_BIT | N_M_BIT | N_D_BIT)) {
		case N_CONNECT:	/* OSI N-CONNECT IND		*/
			C->channel_assigned = 1;
			n_conn_ind (C, M->Parms, sizeParms);
			break;

		case N_CONNECT_ACK:	/* OSI N-CONNECT RES		*/
			C->channel_assigned = 1;
			n_conn_ack (C, M->Parms, sizeParms);
			break;

	case N_DISC	  :	/* OSI N-DISC IND		*/
		C->net_disc_ind_pending = 0;
		C->net_disc_ok = 1;
		C->channel_assigned = 0;
		n_disc_ind (C, M->Parms, sizeParms);
		break;

	case N_DISC_ACK	  :	/* OSI N-DISC RES		*/
		C->net_disc_ind_pending = 0;
		C->net_disc_ok = 1;
		C->channel_assigned = 0;
		n_disc_ack (C, M->Parms, sizeParms);
		break;

	/*
		for now we silently ignore all the other indications
		*/
	case N_RESET	  :	/* OSI N-RESET IND		*/
	case N_RESET_ACK  :	/* OSI N-RESET RES		*/
	case N_DATA_ACK	  : 	/* data ack ind for D-bit proc	*/
	case N_EDATA_ACK  :	/* data	ack ind for INTERRUPT	*/
		break;
	}
}

/*
	 handle the return code indication for the last started operation
	*/
static void isdnNetRc (ISDN_CHANNEL *C, dword event) {
	if (C->Port && (C->State == C_DATA_XFER)) {
		if (event & IDI_DIALER_EVENT_DATA_RC) {
			Machine.Port->Go (C, C->Port);
		} else if ((event & IDI_DIALER_EVENT_CONN_RC) &&
							 (C->Prot == ISDN_PROT_FAX_CLASS1) && !queueCount (&C->Xqueue)) {
			Machine.Port->Go (C, C->Port);
		}
	}
}

/*
	process the events indicated from interrupt
	*/
static void _cdecl isdnEvent (ISDN_ADAPTER *A) {
	ISDN_CHANNEL	*C;
	ISDN_MSG	*M;
	word		sizeParms;

	if (Machine.State == M_DOWN) {
		return ;
	}

	/*
		Prevent callbacks from sending messages to the IDI
		as long as we are in event handling.
		Any queued messages will be sent when we leave
		*/
	if (A->EventLock & A_PROCESSING_EVENTS) {
		return ;
	}
	A->EventLock |= A_PROCESSING_EVENTS ;

	while ((M = (ISDN_MSG*)queuePeekMsg (&A->Events, &sizeParms))) {
		C = M->Channel;
		sizeParms -= sizeof (*M);
		if (!C) {
			DBG_ERR(("A: EvCHANNEL ???"))
			queueFreeMsg (&A->Events) ;
			continue;
		}

		switch (M->Kind) {
			case ISDN_SIG_RC:
				if (divas_tty_debug_sig) {
					if (M->Rc != ASSIGN_OK) {
						DBG_TRC(("[%p:%s] ESigRc=%s Id=%04x Rc=%02x",\
												 C, port_name(C),
												 sig_name (M->Type, 1), WIDE_ID(C->Sig), M->Rc))
					} else {
						DBG_TRC(("[%p:%s] ESigRc=%s Id=%04x Rc=%02x",\
												 C, port_name(C),
												 "S->ASSIGN", WIDE_ID(C->Sig), M->Rc))
					}
				}
				isdnSigRc (C, M);
				break;

			case ISDN_SIG_IND:
				if (divas_tty_debug_sig && (M->Type != INFO_IND)) {
					DBG_TRC(("[%p:%s] ESigInd=%s Id=%04x",\
											 C, port_name(C),
											 sig_name (M->Type, 0), WIDE_ID(C->Sig)))
				}
				isdnSigInd (C, M, sizeParms);
				break;

			case ISDN_NET_CTL_IND:
				if (divas_tty_debug_net_data ||
						(divas_tty_debug_net && (M->Type != N_DATA))) {
					DBG_TRC(("[%p:%s] ENetCtrlInd=%s %s Id=%04x",\
											 C, port_name(C),
											 net_name (M->Type), "IND", WIDE_ID(C->Net)))
				}
				isdnNetCtlInd (C, M, sizeParms);
				break;

			case ISDN_NET_DATA_IND:
				if (divas_tty_debug_net_data ||
						(divas_tty_debug_net && (M->Type != N_DATA))) {
					DBG_TRC(("[%p:%s] ENetDataInd=%s %s Id=%04x",\
											 C, port_name(C),
											 net_name (M->Type), "IND", WIDE_ID(C->Net)))
				}
				isdnNetDataInd (C, M, sizeParms);
				diva_port_add_pending_data (C, C->Port, -1 * sizeParms);
				break;

			default:
				DBG_ERR(("A: [%p:%s] Event: kind=%x ?", C, port_name(C)))
		}
		queueFreeMsg (&A->Events) ;
	}

	A->EventLock &= ~A_PROCESSING_EVENTS;


	for (C = A->Channels; C < A->lastChannel; C++) {
		if (C->RcNotifyFlag) {
			isdnNetRc (C, C->RcNotifyFlag);
			C->RcNotifyFlag = 0;
		}
		isdnPutReq (C);
	}

}

static void IDI_CALL_TYPE sync_isdnSigCallback (ENTITY *E) {
	ISDN_ADAPTER	*A;
	ISDN_CHANNEL	*C;
	ISDN_MSG	*M;
	word		sizeMsg;
	byte Rc;


	C = BASE_POINTER (ISDN_CHANNEL, Sig, E);
	A = C->Adapter;

	if (C->Sig.complete == 255) {
		Rc = C->Sig.Rc;
		if (divas_tty_debug_sig) {
			if (Rc != ASSIGN_OK) {
				DBG_TRC(("[%p:%s] SigRc=%s Id=%04x Rc=%02x",\
										 C, port_name(C),
										 sig_name (C->Sig.Req, 1), WIDE_ID(C->Sig), C->Sig.Rc))
			} else {
				DBG_TRC(("[%p:%s] SigRc=%s Id=%04x Rc=%02x",\
										 C, port_name(C),
										 "S-ASSIGN", WIDE_ID(C->Sig), C->Sig.Rc))
			}
		}
		C->Sig.Rc = 0;
		if (C->sig_busy && queuePeekMsg (&C->Squeue, &sizeMsg))
				queueFreeMsg (&C->Squeue);
		if (C->Sig.Req) {
			sizeMsg = sizeof (*M);
			if (!(M = (ISDN_MSG *) queueAllocMsg (&A->Events, sizeMsg))) {
				DBG_ERR(("A: [%p:%s] eQueue OV SigRc(%02x)", C, port_name(C), C->Sig.Req))
			} else {
				M->Channel = C;
				M->Age	   = C->Age;
				M->Kind	= ISDN_SIG_RC;
				M->Type	= C->Sig.Req;
				M->Rc   = Rc;
				queueCompleteMsg ((byte *) M);
			}
		} else {
			DBG_ERR(("A: [%p:%s] SigRc and !Req, Id=%04x",
									 C, port_name (C), WIDE_ID(C->Sig)))
		}
		C->sig_busy = 0;
	} else {
		if (divas_tty_debug_sig && (C->Sig.Ind != INFO_IND)) {
			DBG_TRC(("[%p:%s] SigInd=%s Id=%04x",\
									 C, port_name (C),
									 sig_name (C->Sig.Ind, 0), WIDE_ID(C->Sig)))
		}
		if (C->Sig.complete != 1) {
			DBG_ERR(("A: [%p:%s] DISCARD_IND", C, port_name(C)))
			C->Sig.RNum = 0;
			C->Sig.RNR  = 2;
			C->Sig.Ind  = 0;
			goto sig_unlock;
		}

		if ((C->Port == 0) && (C->State == C_IDLE) && (C->Sig.Ind == INFO_IND) &&
				(C->Sig.RBuffer->length >= 64)) {
			word channels = 0;
			diva_process_profile_change_notification (C->Sig.RBuffer->P, C->Sig.RBuffer->length, &channels);
		}

		if (!C->seen_hup_ind && (C->Sig.Ind == HANGUP)) {
			diva_mtpx_is_parse_t parse_ie;

			C->seen_hup_ind = 1;
			/*
				Save disconnect cause (ESC/CAU)
				*/
			memset (&parse_ie, 0x00, sizeof(parse_ie));
			parse_ie.wanted_ie				= CAU; /* ESC/CAU */
			parse_ie.wanted_escape_ie = 0x7f;

			ParseInfoElement (C->Sig.RBuffer->P,
												C->Sig.RBuffer->length,
												(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
												&parse_ie);
			if (parse_ie.data != 0 && parse_ie.data_length != 0) {
				C->ReceivedDisconnectCause = parse_ie.data[0] | 0x80;
			}
		}

		if ((C->Sig.Ind == HANGUP) && channel_wait_n_disc_ind (C)) {
			C->Sig.RNum = 0;
			C->Sig.RNR = 1;
			C->Sig.Ind = 0;
			C->sig_hup_rnr = 1;
			DBG_TRC(("[%p:%s] delay HANGUP until N_DISC", C, port_name(C)))
			goto sig_unlock;
		}
		C->sig_hup_rnr = 0;
		sizeMsg = sizeof (*M) + C->Sig.RBuffer->length;
		if (!(M = (ISDN_MSG *) queueAllocMsg (&A->Events, sizeMsg))) {
			C->Sig.RNR  = 1;
		} else {
			M->Channel = C;
			M->Age	   = C->Age;
			M->Kind	= ISDN_SIG_IND;
			M->Type	= C->Sig.Ind;
			if (C->Sig.RBuffer->length) {
				mem_cpy (M->Parms, C->Sig.RBuffer->P, C->Sig.RBuffer->length);
			}
			queueCompleteMsg ((byte *) M);
		}
		C->Sig.RNum = 0;
		C->Sig.Ind = 0;
	}

sig_unlock:
	/* sysScheduleDpc (&A->EventDpc); */
	isdnEvent(A);
	isdnPutReq(C);
}

static void IDI_CALL_TYPE isdnSigCallback (ENTITY *E) {
	unsigned long old_irql = eicon_splimp ();
	sync_isdnSigCallback (E);
	eicon_splx (old_irql);
}


static void IDI_CALL_TYPE sync_isdnNetCallback (ENTITY *E) {
	word	sizeMsg = 0, sizeData= 0;
	ISDN_ADAPTER	*A;
	ISDN_CHANNEL	*C;
	ISDN_MSG			*M;
	byte		NetRc, Kind;
	int		Wanted;

	C = BASE_POINTER (ISDN_CHANNEL, Net, E);
	A = C->Adapter;

	if (C->Net.complete == 255) {

		NetRc = C->Net.Rc;
		if (divas_tty_debug_net_data ||
				(divas_tty_debug_net && (C->Net.Req != N_DATA))) {
			if (NetRc != ASSIGN_OK) {
				DBG_TRC(("[%p:%s] NetRc=%s %s Id=%04x Rc=%02x",\
										 C, port_name(C),
										 net_name (C->Net.Req), "REQ", WIDE_ID(C->Net), C->Net.Rc))
			} else {
				DBG_TRC(("[%p:%s] NetRc=%s %s Id=%04x Rc=%02x",\
										 C, port_name(C),
										 "N-ASSIGN", "REQ", WIDE_ID(C->Net), C->Net.Rc))
			}
		}
		C->Net.Rc = 0;
		if (C->Net.Req == 0) {
			DBG_ERR(("A: [%p:%s] Req==0???", C, port_name(C)))
		}
		if ((C->Net.Req == ASSIGN) && (NetRc == ASSIGN_OK)) {
			word sizeParms;
			if (C->net_busy && queuePeekMsg (&C->Xqueue, &sizeParms))
				queueFreeMsg (&C->Xqueue);
			C->net_busy = 0;
			if (C->net_rc)
				C->net_rc--;
			C->NetFC   = 0;
			C->in_assign = 0;
		} else if (C->in_assign) {
			C->net_busy = 0;
			if (C->net_rc)
				C->net_rc--;
			C->NetFC   = 0;
			C->Net.Id  = NL_ID;
			C->Net.ReqCh = 0;
			C->Net.IndCh = 0;
			C->Net.RcCh = 0;
			C->in_assign = 0;
      if (C->rx_dma_ref_count) {
        C->rx_dma_ref_count--;
        if (!C->rx_dma_ref_count) {
          diva_free_dma_descriptor (C, C->rx_dma_handle);
          C->rx_dma_handle = -1;
          C->rx_dma_magic  = 0;
        }
      }
			DBG_ERR(("A: [%p:%s] ASSIGN FAILED(%02x)", C, port_name (C), NetRc))
			shut_link (C, CAUSE_SHUT);
		} else if ((C->Net.Req == REMOVE) && (!C->Net.Id)) {
			word sizeParms;
			C->NetFC   = 0;
			C->Net.Id  = NL_ID;
			C->Net.ReqCh = 0;
			C->Net.IndCh = 0;
			C->Net.RcCh = 0;
			C->net_disc_ok = 0;
			C->net_disc_ind_pending = 0;
			C->channel_assigned = 0;
			if (C->net_busy && queuePeekMsg (&C->Xqueue, &sizeParms)) {
						queueFreeMsg (&C->Xqueue);
			}
			C->net_busy			= 0;
			C->net_rc				= 0;
      if (C->rx_dma_ref_count) {
        C->rx_dma_ref_count--;
        if (!C->rx_dma_ref_count) {
          diva_free_dma_descriptor (C, C->rx_dma_handle);
          C->rx_dma_handle = -1;
          C->rx_dma_magic  = 0;
        }
      }
		} else if ((C->Net.Req == REMOVE) && C->Net.Id && (NetRc==OK)) {
			DBG_ERR(("[%p:%s] A: IGNORE FAKED RC, Id=%04x", C, port_name (C), WIDE_ID(C->Net)))
			return;
		} else if (NetRc == OK_FC) { /* Flow Control */
			word sizeParms;
			if (C->net_busy && queuePeekMsg (&C->Xqueue, &sizeParms))
				queueFreeMsg (&C->Xqueue);
			C->NetFC = 1;
			if (!((C->net_rc == 1) && C->Net.Req == N_DISC)) {
				C->net_busy = 0;
				if (C->net_rc)
					C->net_rc--;
			}
		} else if (NetRc == OK) { /* We can xmit next request */
			word sizeParms;
			byte org_req = C->Net.Req;
			
			if (C->net_busy && queuePeekMsg (&C->Xqueue, &sizeParms))
				queueFreeMsg (&C->Xqueue);
			C->NetFC = 0;
			C->net_busy = 0;
			if (C->net_rc)
				C->net_rc--;
			
			switch	(org_req & ~(N_Q_BIT | N_M_BIT | N_D_BIT)) {
				case 0:
				case N_DATA:	/* OSI N-DATA REQ		*/
				case N_EDATA:	/* OSI N-EXPEDITED DATA	REQ	*/
				case N_UDATA:	/* OSI D-UNIT-DATA REQ		*/
				case N_BDATA:	/* BROADCAST-DATA REQ		*/
					C->RcNotifyFlag |= IDI_DIALER_EVENT_DATA_RC;
					break;
				case N_CONNECT_ACK:	/* OSI N-CONNECT CON		*/
					C->RcNotifyFlag |= IDI_DIALER_EVENT_CONN_RC;
					break;
				default:
					break;
			}
		} else {
			word sizeParms;
			if (C->net_busy && queuePeekMsg (&C->Xqueue, &sizeParms))
				queueFreeMsg (&C->Xqueue);
			C->NetFC = 0;
			C->net_busy = 0;
			if (C->net_rc) {
				C->net_rc--;
			}
			DBG_ERR(("A: [%p:%s] NET RC=%02x", C, port_name (C), NetRc))
		}
		goto unlock;
	}

	sizeMsg = sizeof (*M);
	M = 0;

	if (C->Net.Ind) {
		if (divas_tty_debug_net_data ||
				(divas_tty_debug_net && (C->Net.Ind != N_DATA))) {
			DBG_TRC(("[%p:%s] NetInd=%s %s Id=%04x",\
									 C, port_name(C),
									 net_name (C->Net.Ind), "IND", WIDE_ID(C->Net)))
		}

		Kind = ISDN_NET_DATA_IND;
		switch ((C->Net.Ind & 0x0F)) {
			case N_DATA:
			case N_EDATA:
			case N_UDATA:
			case N_BDATA:
				switch (C->Net.complete) {
					case 0:
						sizeData = C->Net.RLength;
						break;
					case 1:
						sizeData = C->Net.RBuffer->length;
						break;
					case 2:
						sizeMsg = 0; /* copied to preallocated msg */
						break;
					default:
						C->Net.RNum = 0;
						C->Net.RNR  = 2;
						C->Net.Ind  = 0;
						goto unlock; /* should better not happen */
				}
				if (sizeMsg) {

					if (!C->Port) {
						C->Net.RNum = 0;
						C->Net.RNR  = (byte)2;
						C->Net.Ind  = 0;
						goto unlock;
					}

					if (((Wanted = Machine.Port->Want (C, C->Port, sizeData)) <= 0) &&
							(C->State != C_DATA_WAIT) && ((C->Net.Ind & 0x0F) != N_UDATA)) {
						C->Net.RNum = 0;
						C->Net.RNR  = (byte) (1 - Wanted);
						C->Net.Ind  = 0;
						if (C->seen_hup_ind) {
							C->Net.RNR  = 2;
						}
						goto unlock;
					}
					sizeMsg += sizeData;
				}
				break;

		default:
			Kind = ISDN_NET_CTL_IND;
			if (C->Net.complete != 1) {
				C->Net.RNum = 0;
				C->Net.RNR  = 2;
				C->Net.Ind  = 0;
				goto unlock;
			}
			sizeMsg += C->Net.RBuffer->length;
			break;
		}
	} else {
		goto unlock;
	}

	if (!sizeMsg) {
		/* data ready in previously allocated message */
		M = BASE_POINTER (ISDN_MSG, Parms, C->NetRdesc.P);
	} else if (!(M = (ISDN_MSG *) queueAllocMsg (&A->Events, sizeMsg))) {
		C->Net.Ind = 0;
		C->Net.RNR = 1;
		goto unlock;
	} else {
		/* build the event message */
		M->Channel = C;
		M->Age	   = C->Age;
		switch (M->Kind	= Kind) {
			case ISDN_NET_DATA_IND:
				if (C->Net.complete == 0) {
					/*
						request copying of data and wait for completion callback
						*/
					M->Type	= C->Net.Ind;
					C->NetRdesc.P = M->Parms;
					C->NetRdesc.PLength = C->Net.RLength;
					C->Net.RNum = 1;
					C->Net.Ind = 0;
					diva_port_add_pending_data (C, C->Port, sizeData);
					goto unlock;
				}
				if (C->Net.complete == 1) {
					diva_port_add_pending_data (C, C->Port, sizeData);
				}
			case ISDN_NET_CTL_IND:
				M->Type	= C->Net.Ind;
				mem_cpy (M->Parms, C->Net.RBuffer->P, C->Net.RBuffer->length);
				C->Net.RNum = 0;
				C->Net.Ind = 0;
				break;
		}
	}
	queueCompleteMsg ((byte *) M);

unlock:
	/* sysScheduleDpc (&A->EventDpc); */
	isdnEvent(A);
	isdnPutReq(C);
}

static void IDI_CALL_TYPE isdnNetCallback (ENTITY *E) {
	unsigned long old_irql = eicon_splimp ();
	sync_isdnNetCallback (E);
	eicon_splx (old_irql);
}

# if TRY_TO_QUIESCE
static void _cdecl isdnQuiesceTimer (void *context)
{
	DBG_TRC(("Detach: IDI request timed out "))
	Machine.QuiescePending = 0;
	sysPostEvent (&Machine.QuiesceEvent);
}
# endif /* TRY_TO_QUIESCE */

static int isdnAttach (void *DescTab, word sizeDescTab) {
	DESCRIPTOR *Desc, *lastDesc;
	byte *Memory; 
	dword sizePages;
	ISDN_ADAPTER *A;
	ISDN_CHANNEL *C;

	for (Machine.numAdapters = Machine.numChannels = 0,
	     Desc = (DESCRIPTOR *) DescTab,
	     lastDesc = Desc + sizeDescTab;
	     Desc < lastDesc ;
	     Desc++) {

		if (diva_mtpx_adapter_detected) {
			if (Desc->type != IDI_MADAPTER) {
				continue;
			}
		} else {
			if (Desc->type == 0 || Desc->type >= IDI_VADAPTER) {
				Desc->type = 0;	// don't use, don't scan again 
				continue;
			}
		}
		if (!Desc->channels || !Desc->request) {
    			DBG_TRC(("Attach: bad desc"))
			Desc->type = 0;	// don't use, don't scan again 
			continue;
		}
        
		if (Desc->channels) {
			Machine.Features |= Desc->features ;
			Machine.numAdapters++;
			Machine.numChannels += Desc->channels;
		}
	}

	if (!Machine.numAdapters) {
		DBG_TRC(("Attach: no board"))
		return (0);
	}

	if ( Machine.Features & DI_FAX3 )
		Machine.Capabilities |= ISDN_CAP_FAX3 ;
	if ( Machine.Features & DI_MODEM)
		Machine.Capabilities |= ISDN_CAP_MODEM ;

	/* allocate memory */

# define SIZEOF_XMIT_QUEUE			(512 + ISDN_MAX_FRAME)
# define SIZEOF_XMIT_SIG_QUEUE  (((270*2) + sizeof(void*) - 1) & ~(sizeof(void*) - 1))
# define SIZEOF_RECV_QUEUE			(ISDN_MAX_FRAME * 2)

	Machine.sizeMemory =
	(Machine.numAdapters * sizeof (ISDN_ADAPTER))	+
	(Machine.numChannels * sizeof (ISDN_CHANNEL))	+
	(Machine.numChannels * SIZEOF_XMIT_QUEUE)	+
	(Machine.numChannels * SIZEOF_RECV_QUEUE) +
	(Machine.numChannels * SIZEOF_XMIT_SIG_QUEUE); /* Separate Sig queue */

	sizePages = Machine.sizeMemory;

	if (!(Machine.Memory = sysPageAlloc (&sizePages, &Machine.hMemory))) {	
		DBG_TRC(("isdnAttach:Memory Allocation Failure"))
		return (0);
	}

	/* assign memory and perform all static initializations */

	Memory = Machine.Memory;

	Machine.Adapters	= (ISDN_ADAPTER *) Memory;
	Machine.lastAdapter	= Machine.Adapters + Machine.numAdapters;

	Memory = (byte *) Machine.lastAdapter;

	Machine.Channels	= (ISDN_CHANNEL *) Memory;
	Machine.lastChannel	= Machine.Channels + Machine.numChannels;

	Memory = (byte *) Machine.lastChannel;

	for (Desc = (DESCRIPTOR *) DescTab,
	     A = Machine.Adapters, C = Machine.Channels ;
	     Desc < lastDesc; Desc++) {

		if (diva_mtpx_adapter_detected) {
			if (Desc->type != IDI_MADAPTER) {
				continue;
			}
		} else {
			if ((Desc->type != 0x02) && (Desc->type != 0x04) && (Desc->type != 0x03)) {
				continue;	/* not for me */
			}
		}

		A->State	= A_DOWN;
		A->Type		= Desc->type;
		A->Features	= Desc->features;
		A->RequestFunc	= Desc->request;
		A->numChannels	= Desc->channels;
		A->Channels	= C;
		A->lastChannel	= C + A->numChannels;

		queueInit (&A->Events,Memory,A->numChannels*SIZEOF_RECV_QUEUE);

		Memory += A->numChannels*SIZEOF_RECV_QUEUE;

		for ( ; C < A->lastChannel; C++) {

			C->Adapter	= A;
			C->RequestFunc	= A->RequestFunc;
			C->Features	= A->Features;
			sysInitializeDpc (&C->put_req_dpc, (sys_CALLBACK)isdnPutReqDpc, (void*)C);

			set_channel_id (C, 0) ;
			set_esc_uid (C, NULL) ;

			C->State	= C_DOWN;

			C->Sig.Id	= DSIG_ID;
			C->Sig.callback = isdnSigCallback;
			C->Sig.X	= &C->SigXdesc;

			C->Net.Id	= NL_ID;
			C->Net.callback = isdnNetCallback;
			C->Net.X	= &C->NetXdesc;
			C->Net.R	= &C->NetRdesc;

			queueInit (&C->Xqueue, Memory, SIZEOF_XMIT_QUEUE);
			Memory += SIZEOF_XMIT_QUEUE;
			queueInit (&C->Squeue, Memory, SIZEOF_XMIT_SIG_QUEUE);
			Memory += SIZEOF_XMIT_SIG_QUEUE;
		}

		A++;
	}

	if (A != Machine.lastAdapter || C != Machine.lastChannel ||
	    Memory != Machine.Memory + Machine.sizeMemory) {
	    	DBG_ERR(("Attach: logic?"))
		sysPageFree (Machine.hMemory);
		return (0);
	}

	/* now we try to assign to IDI */

	Machine.State = M_INIT;

# if TRY_TO_QUIESCE
	sysInitializeEvent (&Machine.QuiesceEvent);
	sysInitializeTimer (&Machine.QuiesceTimer, isdnQuiesceTimer, 0);
# endif /* TRY_TO_QUIESCE */

	for (A = Machine.Adapters; A < Machine.lastAdapter; A++) {
		A->State = A_INIT;
		sysInitializeDpc  (&A->EventDpc, (sys_CALLBACK) isdnEvent, A);
		for (C = A->Channels; C < A->lastChannel; C++) { 
			C->State = C_INIT;
			{
				unsigned long old_irql = eicon_splimp ();

				(void) sigassign (C);
				isdnPutReq (C); 

				eicon_splx (old_irql);
			}
			/*
				after ASSIGN_OK we set C->State to C_IDLE	
				*/
		}
		A->State = A_RUNS;
	}
	Machine.State = M_RUNS;
	return (1);
}

static int isdnDetach (void) {
# if TRY_TO_QUIESCE
	ISDN_CHANNEL *C;
	ISDN_ADAPTER *A ;

	if ( Machine.State == M_DOWN )
	{
		DBG_ERR(("Detach: duplicate call"))
		return (1) ;
	}

	DBG_TRC(("Detach: {"))

	/*
	 * The problem here is to quiesce everything before we return to the
	 * caller cause we may have to send more than one request and to await
	 * the result of the first before we can start the next one.
	 *
	 * In a first loop we look if there are active connections or pending
	 * disconnects.
	 * For active connections the disconnect is started, then we must wait
	 * for the end of all disconnects.
	 *
	 * In a second loop we enqueue the remove requests for all network and
	 * signaling entities and wait for completion.
	 */

	Machine.State = M_SHUT ;

	for (Machine.QuiesceCount = 0, C = Machine.Channels;
	     C < Machine.lastChannel; C++) {

		detach_port (C) ;

		switch (C->State) {
		default		   :
		case C_DOWN	   :
		case C_INIT	   :
			/* ready for the next loop */
			continue;

		case C_IDLE	   :
			/* clear listen mode (if any) */
			C->State = C_CALL_DISC_X;
			if (C->RequestedDisconnectCause != 0) {
				byte esc_cause[5] ;

   			esc_cause[0] = ESC;		/* elem type	*/
   			esc_cause[1] = 3;		/* info length	*/
   			esc_cause[2] = CAU;		/* real type 	*/
   			esc_cause[3] = (0x80 | C->RequestedDisconnectCause);
   			esc_cause[4] = 0;		/* 1tr6 cause	*/

				isdnSigEnq (C, HANGUP, esc_cause, sizeof(esc_cause));
			} else {
				isdnSigEnq (C, HANGUP, "", 1);
			}
			isdnPutReq (C);
			break;

		case C_CALL_DIAL   :
		case C_CALL_OFFER  :
		case C_CALL_ACCEPT :
		case C_CALL_ANSWER :
		case C_CALL_UP     :
		case C_DATA_CONN_X :
		case C_DATA_WAIT   :
		case C_DATA_XFER   :
			/* remove network entity (if any) and disconnect call */
			disc_link (C, Q931_CAUSE_NormalCallClearing, 0);
			isdnPutReq (C);
			break;

		case C_DATA_DISC_X :
		case C_CALL_DISC_X :
			/* should be on the way of disconnecting already */
			break;
		}
		Machine.QuiesceCount++;
	}

	if (Machine.QuiesceCount) {

		Machine.QuiescePending =
		sysScheduleTimer (&Machine.QuiesceTimer, 2000 /*two seconds*/);

		while (Machine.QuiescePending) {
			for (C = Machine.Channels;
			     C < Machine.lastChannel; C++) {
				switch (C->State) {
				case C_DOWN	   :
				case C_INIT	   :
				case C_IDLE	   :
					continue; /* as quiet as needed here */
				}
				break;	/* let's block */
			}
			if (C >= Machine.lastChannel) break; /* quiet now */
			DBG_TRC(("[%s] Detach: wait for hangup ", C->Id))
			sysResetEvent (&Machine.QuiesceEvent);
			sysWaitEvent  (&Machine.QuiesceEvent);
		}

		if (Machine.QuiescePending) {
			/* no timeout, i.e. this phase was successful */
			sysCancelTimer (&Machine.QuiesceTimer);
			Machine.QuiescePending = 0;
			Machine.QuiesceCount = 0;
		}
	}

	if (!Machine.QuiesceCount) {

		for (C = Machine.Channels; C < Machine.lastChannel; C++) {
			if (C->State == C_DOWN) continue;
			/* flush the queue as appropriate and */
			/* remove the network entity (if any) */
			disc_net (C, REMOVE);
			if (C->Net.Id != NL_ID) {
				Machine.QuiesceCount++;
			}
			if (C->Sig.Id != DSIG_ID) {
				Machine.QuiesceCount++;
				isdnSigEnq (C, REMOVE, "", 1);
			}
			isdnPutReq (C);
		}

		if (Machine.QuiesceCount) {

			Machine.QuiescePending =
			sysScheduleTimer (&Machine.QuiesceTimer,
					  1000 /*one second*/   );

			while (Machine.QuiescePending) {

				for ( C = Machine.Channels ; C < Machine.lastChannel ; C++ )
				{
					if ( (C->Net.Id != NL_ID)	 /* network entity active	 */
				      || (C->Sig.Id != DSIG_ID)	 /* signalling entity active */
					  || (C->State  != C_DOWN) ) /* return code outstanding	 */
					{
						break ; /* this channel is busy, let's wait */
					}
				}
				if ( C >= Machine.lastChannel )
				{
					break ; /* all channels quiet now */
				}
				prinrk ("[%s] Detach: wait for remove", C->Id);
				sysResetEvent (&Machine.QuiesceEvent) ;
				sysWaitEvent  (&Machine.QuiesceEvent) ;


			}

			if (Machine.QuiescePending) {
				/* this was successful too */
				sysCancelTimer (&Machine.QuiesceTimer);
				Machine.QuiescePending = 0 ;
				Machine.QuiesceCount = 0;
			}
		}
	}

	if (!Machine.QuiesceCount) {
		for ( A = Machine.Adapters ; A < Machine.lastAdapter ; A++ )
			sysCancelDpc (&A->EventDpc) ;
			mem_zero (Machine.Memory, Machine.sizeMemory) ;
			sysPageFree (Machine.hMemory);
			Machine.Memory = 0;
			Machine.State  = M_DOWN;
	}

	DBG_TRC(("Detach: }"))

	return (Machine.QuiesceCount == 0);
# else /* !TRY_TO_QUIESCE */
	return (0);
# endif /* TRY_TO_QUIESCE */
}

static	void isdnCheck (void) { /* to be done */ }
static	void isdnReset (void) { /* to be done */ }

static	word isdnCaps  (void) { return (Machine.Capabilities); }

ISDN_REQUESTS *	isdnBind (
	ISDN_INDICATIONS *Indications, void *DescBuf, word sizeDescBuf)
{
	static ISDN_REQUESTS Requests = {
		isdnXmit,
		isdnDial,
		isdnListen,
		isdnAccept,
		isdnAnswer,
		isdnDrop,
		isdnCheck,
		isdnReset,
		isdnCaps,
	};

	ISDN_REQUESTS *Ret = 0;

	DBG_TRC(("Bind: {"))

	mem_set (&diva_tty_call_filter, 0x00, sizeof(diva_tty_call_filter));

	if (Indications) 
	{
		if (Machine.Port) 
		{
			/*DBG_ERR(("Bind: attach duplicate "))*/
		} else if (!isdnAttach (DescBuf, sizeDescBuf)) 
			{
#if !defined(LINUX)
				   cmn_err(CE_NOTE, "ETSR Bind: att err");
#endif
			} else 
			{
			Machine.Port = Indications;
			Ret = &Requests;
			}
	}
	else 
	{
		if (!Machine.Port) 
		{
		} 	
		else if (!isdnDetach ()) 
		{
#if !defined(LINUX)
        	cmn_err(CE_NOTE, "ETSR Bind: det err");
#endif
		} 	
		else 
		{
			Machine.Port = 0;
		}
	}


	return (Ret);
}

int isdn_get_num_adapters (void) {
	return ((int)Machine.numAdapters);
}

int isdn_get_adapter_channel_count (int adapter_nr) {
	if (Machine.Adapters && adapter_nr && ((adapter_nr - 1) < (int)Machine.numAdapters)) {
		return ((int)(Machine.Adapters[adapter_nr - 1].numChannels));
	}

	return (0);
}

static const char* sig_name (byte code, int is_req) {
	char *p;
 switch (code) {
  case CALL_REQ:    /* 1 call request                             */
     /* CALL_CON          1 call confirmation                        */
   if (is_req) {
    p = "S-CALL_REQ";
   } else {
    p = "S-CALL_CON";
   }
   break;
  case LISTEN_REQ:   /* 2  listen request                           */
   /* CALL_IND      2  incoming call connected                  */
   if (is_req) {
    p = "S-LISTEN_REQ";
   } else {
    p = "S-CALL_IND";
   }
   break;
  case HANGUP:     /* 3  hangup request/indication                */
   p = "S-HANGUP";
   break;
  case SUSPEND:     /* 4  call suspend request/confirm             */
   p = "S-SUSPEND";
   break;
  case RESUME:     /* 5  call resume request/confirm              */
   p = "S-RESUME";
   break;
  case SUSPEND_REJ:   /* 6  suspend rejected indication              */
   p = "S-SUSPEND_REJ";
   break;
  case USER_DATA:    /* 8  user data for user to user signaling     */
   p = "S-USER_DATA";
   break;
  case CONGESTION:    /* 9  network congestion indication            */
   p = "S-CONGESTION";
   break;
  case INDICATE_REQ:  /* 10 request to indicate an incoming call     */
   /* INDICATE_IND    10 indicates that there is an incoming call */
   if (is_req) {
    p = "S-INDICATE_REQ";
   } else {
    p = "S-INDICATE_IND";
   }
   break;
  case CALL_RES:    /* 11 accept an incoming call                  */
   p = "S-CALL_RES";
   break;
  case CALL_ALERT:   /* 12 send ALERT for incoming call             */
   p = "S-CALL_ALERT";
   break;
  case INFO_REQ:    /* 13 INFO request                             */
   /* INFO_IND      13 INFO indication                          */
   if (is_req) {
    p = "S-INFO_REQ";
   } else {
    p = "S-INFO_IND";
   }
   break;
  case REJECT:     /* 14 reject an incoming call                  */
   p = "S-REJECT";
   break;
  case RESOURCES:     /* 15 reserve B-Channel hardware resources     */
   p = "S-RESOURCES";
   break;
  case TEL_CTRL:      /* 16 Telephone control request/indication     */
   p = "S-TEL_CTRL";
   break;
  case STATUS_REQ:    /* 17 Request D-State (returned in INFO_IND)   */
   p = "S-STATUS_REQ";
   break;
  case FAC_REG_REQ:   /* 18 connection idependent fac registration   */
   p = "S-FAC_REG_REQ";
   break;
  case FAC_REG_ACK:   /* 19 fac registration acknowledge             */
   p = "S-FAC_REG_ACK";
   break;
  case FAC_REG_REJ:   /* 20 fac registration reject                  */
   p = "S-FAC_REG_REJ";
   break;
  case CALL_COMPLETE: /* 21 send a CALL_PROC for incoming call       */
   p = "S-CALL_COMPLETE";
   break;
  case FACILITY_REQ:  /* 22 send a Facility Message type             */
   /* FACILITY_IND    22 Facility Message type indication         */
   if (is_req) {
    p = "S-FACILITY_REQ";
   } else {
    p = "S-FACILITY_IND";
   }
   break;
  case SIG_CTRL:      /* 29 Control for signalling hardware          */
   p = "S-SIG_CTRL";
   break;
  case DSP_CTRL:      /* 30 Control for DSPs                         */
   p = "S-DSP_CTRL";
   break;
  case LAW_REQ:       /* 31 Law config request for (returns info_i)  */
   p = "S-LAW_REQ";
   break;
  case UREMOVE:
   p = "S-UREMOVE";
   break;
  case REMOVE:
   p = "S-REMOVE";
   break;
  default:
   p = "S-UNKNOWN";
   break;
 }
	return (p);
}

static const char* net_name (byte code) {
	if (code == 0xff) {
		return ("N_REMOVE");
	}
	switch (code & 0x0f) {
		case N_MDATA:
			return ("N_MDATA");
			break;
		case N_CONNECT:
			return ("N_CONNECT");
			break;
		case N_CONNECT_ACK:
			return ("N_CONNECT_ACK");
			break;
		case N_DISC:
			return ("N_DISC");
			break;
		case N_DISC_ACK:
			return ("N_DISC_ACK");
			break;
		case N_RESET:
			return ("N_RESET");
			break;
		case N_RESET_ACK:
			return ("N_RESET_ACK");
			break;
		case N_DATA:
			return ("N_DATA");
			break;
		case N_EDATA:
			return ("N_EDATA");
			break;
		case N_UDATA:
			return ("N_UDATA");
			break;
		case N_BDATA:
			return ("N_BDATA");
			break;
		case N_DATA_ACK:
			return ("N_DATA_ACK");
			break;
		case N_EDATA_ACK:
			return ("N_EDATA_ACK");
			break;
		default:
			DBG_TRC((" -<%02x>- ", code))
			return ("N-UNKNOWN NS");
	}
}

void print_adapter_status (void) {
	ISDN_CHANNEL	*C, *lastChannel;
	int i = 0;

	C = Machine.Channels ;
	lastChannel = Machine.lastChannel ;

	for ( ; ; ) {
		DBG_TRC(("C:%d -----------------------------------------------", i++))
		DBG_TRC(("[%p:%s], state: %d", C, port_name(C), C->State))
		DBG_TRC(("[%p:%s], Sig:%04x", C, port_name(C), WIDE_ID(C->Sig)))
		DBG_TRC(("[%p:%s], sig_busy:%d", C, port_name(C), C->sig_busy))
		DBG_TRC(("[%p:%s], Net:%04x", C, port_name(C), WIDE_ID(C->Net)))
		DBG_TRC(("[%p:%s], net_busy:%d", C, port_name(C), C->net_busy))
		DBG_TRC(("[%p:%s], net_rc:%d", C, port_name(C), C->net_rc))
		DBG_TRC(("[%p:%s], NetFC:%d", C, port_name(C), C->NetFC))
		DBG_TRC(("[%p:%s], sig_hup_rnr:%d", C, port_name(C), C->sig_hup_rnr))
		DBG_TRC(("[%p:%s], sig_remove_sent:%d", C, port_name(C), C->sig_remove_sent))

		if (++C >= Machine.lastChannel) {
			return;
		}
	}
}

void do_adapter_shutdown (void) {
	ISDN_CHANNEL	*C, *lastChannel;
	unsigned long old_irql;
	dword wait_sec = 10;

	diva_tty_shutdown = 1;

	C = Machine.Channels ;
	lastChannel = Machine.lastChannel ;

	old_irql = eicon_splimp ();

	for ( ; ; ) {
		if (C->Sig.Id != DSIG_ID)	{
			if (C->State == C_IDLE) {
				C->State = C_INIT;
			}
			isdnSigEnq (C, HANGUP, "", 1);
			isdnPutReq	(C);
		}
		if (++C >= Machine.lastChannel) {
			break;
		}
	}

	eicon_splx (old_irql);

	while (wait_sec--) {
		diva_tty_os_sleep (1000);
		C = Machine.Channels ;
		lastChannel = Machine.lastChannel ;

		for (;;) {
			if (C->Sig.Id != DSIG_ID)	{
				break;
			}
			if (++C >= Machine.lastChannel) {
				return; /* All channels removed */
			}
		}
	}
	DBG_ERR(("A: can't remove Sig Id's"))
	print_adapter_status ();
}

static int channel_wait_n_disc_ind (ISDN_CHANNEL *C) {
	if (((C->State == C_DATA_CONN_X) ||
			(C->State == C_DATA_WAIT) ||
			(C->State == C_DATA_XFER)) && C->delay_hangup) {
		C->delay_hangup--;
		return 1;
	}
	return (0);
}

static int diva_get_dma_descriptor (ISDN_CHANNEL* C, dword* dma_magic) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

	pReq->xdi_dma_descriptor_operation.Req = 0;
	pReq->xdi_dma_descriptor_operation.Rc  = \
                                        IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

	pReq->xdi_dma_descriptor_operation.info.operation = \
                                        IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC;
	pReq->xdi_dma_descriptor_operation.info.descriptor_number  = -1;
	pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
	pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  C->RequestFunc ((ENTITY*)pReq);

  if (!pReq->xdi_dma_descriptor_operation.info.operation &&
      (pReq->xdi_dma_descriptor_operation.info.descriptor_number >= 0) &&
      pReq->xdi_dma_descriptor_operation.info.descriptor_magic) {
    DBG_TRC(("[%p:%s] dma_alloc (%d,%08x)",
                 C, port_name(C),
                 pReq->xdi_dma_descriptor_operation.info.descriptor_number,
                 pReq->xdi_dma_descriptor_operation.info.descriptor_magic))

    *dma_magic = pReq->xdi_dma_descriptor_operation.info.descriptor_magic;

    return (pReq->xdi_dma_descriptor_operation.info.descriptor_number);
  } else {
    DBG_TRC(("[%p:%s] dma_alloc failed", C, port_name(C)))
    return (-1);
  }
}

static void diva_free_dma_descriptor (ISDN_CHANNEL* C, int nr) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

  if (nr < 0) {
    return;
  }

	pReq->xdi_dma_descriptor_operation.Req = 0;
	pReq->xdi_dma_descriptor_operation.Rc  = \
                                        IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

	pReq->xdi_dma_descriptor_operation.info.operation = \
                                        IDI_SYNC_REQ_DMA_DESCRIPTOR_FREE;
	pReq->xdi_dma_descriptor_operation.info.descriptor_number  = nr;
	pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
	pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  C->RequestFunc ((ENTITY*)pReq);

  if (!pReq->xdi_dma_descriptor_operation.info.operation) {
    DBG_TRC(("[%p:%s] dma_free(%d)", C, port_name(C), nr))
  } else {
    DBG_ERR(("[%p:%s] dma_free(%d) failed", C, port_name(C), nr))
  }
}

static void local_no_req (ENTITY* e) {

}

/*
  Notification from DADAPTER about adapter start/shutdown
  */
void diva_adapter_change (IDI_CALL request, int insert) {
	unsigned long old_irql = eicon_splimp ();
  ISDN_CHANNEL* C = Machine.Channels ;
	ISDN_MSG msg, *M = &msg;

  for (;;) {
    if ((!insert) && (C->RequestFunc == request)) {
      C->RequestFunc = local_no_req;
      link_down (C, CAUSE_FATAL);
      detach_port (C);
			queueReset (&C->Xqueue);
      queueReset (&C->Squeue);
      if (C->rx_dma_ref_count) {
        C->rx_dma_ref_count = 0;
      	C->RequestFunc = request;
        diva_free_dma_descriptor (C, C->rx_dma_handle);
      	C->RequestFunc = local_no_req;
        C->rx_dma_handle = -1;
        C->rx_dma_magic  = 0;
      }
			C->net_rc = 1; /* to mark channel as unuseable */

			queueReset (&C->Adapter->Events);

			DBG_TRC(("[%p:?] Channel Stopped", C))
		}
    if (insert && (C->RequestFunc == local_no_req) &&
        (C->Adapter->RequestFunc == request)) {
      C->RequestFunc = C->Adapter->RequestFunc;
			C->Sig.Id	= DSIG_ID;
			C->State = C_INIT;

      if (C->rx_dma_ref_count) {
        C->rx_dma_ref_count = 0;
        diva_free_dma_descriptor (C, C->rx_dma_handle);
        C->rx_dma_handle = -1;
        C->rx_dma_magic  = 0;
      }

      M->Rc = OK;
      M->Type = REMOVE;

			C->NetFC   = 0;
			C->Net.Id  = NL_ID;
			C->Net.ReqCh = 0;
			C->Net.IndCh = 0;
			C->Net.RcCh = 0;
			C->net_disc_ok = 0;
			C->net_disc_ind_pending = 0;
			C->channel_assigned = 0;
			C->in_assign = 0;
			C->net_busy			= 0;
			C->sig_busy			= 0;
			C->net_rc				= 0;
      C->Net.Rc  = 0;
      C->Sig.Rc  = 0;
			C->Sig.Ind   = 0;
			C->Net.Ind   = 0;
			C->NetUp   = 0;
			C->MdmUp   = 0;
			C->DetProt = DET_PROT_OFF;
			C->Prot    = 0;
			C->RcNotifyFlag = 0;
			C->transparent_detect = 0;

			DBG_TRC(("[%p:?] Channel Started", C))

			queueReset (&C->Xqueue);
      queueReset (&C->Squeue);
      isdnSigRc (C, M);
			isdnPutReq (C); 
		}
    if (++C >= Machine.lastChannel) {
      eicon_splx (old_irql);
      return; /* All channels removed */
    }
  }

	eicon_splx (old_irql);
}

#if 0
static void diva_getname (ISDN_FIND_INFO *I, word Cmd, byte *Name, word sizeName) {
	byte 	                Len ;
	byte *                  CurElem ;
	word                    CorNetCmd;

	// This information element has the type SSEXTIE, which contain
	// one or more CorNet-N information elements.

	if (I->numOctets < 6 || I->numOctets >= 128)
		return;

	/* scan all CorNet-N info elements */
	CurElem = I->Octets ;
	for (Len=0; Len < I->numOctets; Len += 2+CurElem[1], CurElem += 2+CurElem[1])
	{
		if (   CurElem[0] != SSEXT_IND    // IE type; SSEXT_IND is what we want
		    || CurElem[1] <  4)           // IE length;
		{
			continue ;	/* not interresting */
		}
		CorNetCmd  = ((word)(CurElem[3]) << 8) | CurElem[2] ;
		if (CorNetCmd != Cmd)
			continue ;	/* not interresting */

		if (CurElem[1] - 3 < sizeName)
		{
			mem_cpy(Name, &CurElem[5], CurElem[1] - 3) ;
			Name[CurElem[1]-3] = 0 ;
		}
		break ;
	}
}
#endif

/*
	Call filter used to bind the CPN's to the specified protocols, independent from the
	real detected protocol and independent from the signalled protocol
	*/
const char* diva_tty_isdn_get_protocol_name (int protocol_nr) {
	if (protocol_nr < (sizeof(protmap)/sizeof(protmap[0]))) {
		return (protmap[protocol_nr].nice_name);
	}
	return (0);
}

const diva_call_filter_entry_t* diva_tty_isdn_get_call_filter_entry (int filter_nr) {
	if ((filter_nr < (sizeof(diva_tty_call_filter.entry)/sizeof(diva_tty_call_filter.entry[0]))) &&
			diva_tty_call_filter.n_entries) {
		return (&diva_tty_call_filter.entry[filter_nr]);
	}

	return (0);
}


/*
	CPN:PROTOCOL

	returns (-1) on error or amount of the characters consumed by the function
	*/
int diva_tty_isdn_set_call_filter_entry (const char* cmd) {
	int i, j, k;

	for (i = 0; (cmd[i] && (cmd[i] != ':')); i++);

	if (!cmd[i] || (i >= (ISDN_MAX_NUMBER-1)) || (i < 1)) {
		return (-1);
	}

	if ((str_len ("NONE") <= str_len((char*)&cmd[i+1])) &&
			!mem_i_cmp ("NONE", (char*)&cmd[i+1], str_len ("NONE"))) {

		for (k = 0; k < MAX_CALL_FILTER_ENTRIES; k++) {
			if (diva_tty_call_filter.entry[k].number[0] &&
					!mem_cmp ((char*)&diva_tty_call_filter.entry[k].number[0],
										(char*)cmd, MAX(i, str_len (&diva_tty_call_filter.entry[k].number[0])))) {
				diva_tty_call_filter.entry[k].number[0] = 0;
				if (diva_tty_call_filter.n_entries) {
					diva_tty_call_filter.n_entries--;
				}
				return (i + 1 + str_len ("NONE"));
			}
		}

		return (-1);
	}

	for (j = 0; j < sizeof(protmap)/sizeof(protmap[0]); j++) {
		if (protmap[j].nice_name[0] &&
				(str_len ((char*)protmap[j].nice_name) <= str_len((char*)&cmd[i+1])) &&
				!mem_i_cmp ((char*)protmap[j].nice_name, (char*)&cmd[i+1], str_len ((char*)protmap[j].nice_name))) {

			for (k = 0; k < MAX_CALL_FILTER_ENTRIES; k++) {
				if (diva_tty_call_filter.entry[k].number[0] &&
						!mem_cmp ((char*)&diva_tty_call_filter.entry[k].number[0],
											(char*)cmd, MAX(i, str_len (&diva_tty_call_filter.entry[k].number[0])))) {
					diva_tty_call_filter.entry[k].prot = (byte)j;
					return (i + 1 + str_len ((char*)protmap[j].nice_name));
				}
			}

			for (k = 0; k < MAX_CALL_FILTER_ENTRIES; k++) {
				if (!diva_tty_call_filter.entry[k].number[0]) {
					mem_cpy ((char*)&diva_tty_call_filter.entry[k].number[0], (char*)cmd, i);
					diva_tty_call_filter.entry[k].number[i] = 0;
					diva_tty_call_filter.entry[k].prot = (byte)j;
					diva_tty_call_filter.n_entries++;
					return (i + 1 + str_len ((char*)protmap[j].nice_name));
				}
			}
		}
	}

	return (-1);
}

static int diva_mnt_cmp_nmbr (const char* nmbr, const char* TraceFilter) {
  const char* ref = &TraceFilter[0];
  int ref_len = strlen(&TraceFilter[0]), nmbr_len = strlen(nmbr);

  if (!ref_len || (ref_len > nmbr_len)) {
    return (-1);
  }

  nmbr = nmbr + nmbr_len - 1;
  ref  = ref  + ref_len  - 1;

  while (ref_len--) {
    if (*nmbr-- != *ref--) {
      return (-1);
    }
  }

  return (0);
}

/*
	Based on the address string find the protocol to be used
	for this party
	*/
static int get_fixed_protocol (const char* addr) {
	int k, i = diva_tty_call_filter.n_entries;

	for (k = 0; (i && (k < MAX_CALL_FILTER_ENTRIES)); k++) {
		if (diva_tty_call_filter.entry[k].number[0]) {
			i--;
			if (!diva_mnt_cmp_nmbr (addr, &diva_tty_call_filter.entry[k].number[0]) &&
					(diva_tty_call_filter.entry[k].prot < (sizeof(protmap)/sizeof(protmap[0])))) {
				return (protmap[diva_tty_call_filter.entry[k].prot].Prot);
			}
		}
	}

	return (-1);
}

/* -------------------------------------------------------------------
     Ask XDI about extended features
   ------------------------------------------------------------------- */
void diva_get_extended_adapter_features (IDI_CALL request) {
  IDI_SYNC_REQ* preq;
  ENTITY* e;
  char buffer[((sizeof(preq->xdi_extended_features)+4) > sizeof(ENTITY)) ? \
                            (sizeof(preq->xdi_extended_features)+4) : sizeof(ENTITY)];
  byte features[4];

  preq = (IDI_SYNC_REQ*)&buffer[0];
  e    = (ENTITY*)preq;

  preq->xdi_extended_features.Req = 0;
  preq->xdi_extended_features.Rc  = IDI_SYNC_REQ_XDI_GET_EXTENDED_FEATURES;
  preq->xdi_extended_features.info.buffer_length_in_bytes = sizeof(features);
  preq->xdi_extended_features.info.features = &features[0];

	(*request)(e);

	if (features[0] & DIVA_XDI_EXTENDED_FEATURES_VALID) {
      /*
         Check features located in the byte '0'
         */
    if (features[0] & DIVA_XDI_EXTENDED_FEATURE_WIDE_ID) {
			DBG_LOG(("Adapter supports wide id"))
			diva_wide_id_detected      = 1;
    }
  }
}

/*
	Process law and provile indication from adapter
	*/
static dword diva_process_profile_change_notification (byte* data, word length, word* supported_channels) {
	dword features = 0;

	if (data && length) {
		diva_mtpx_is_parse_t parse_ie;

		memset (&parse_ie, 0x00, sizeof(parse_ie));
		parse_ie.wanted_ie				= PROFILEIE; /* ESC/LAW */
		parse_ie.wanted_escape_ie = 0x7f;

		ParseInfoElement (data,
											length,
											(ParseInfoElementCallbackProc_t)diva_mtpx_parse_ie_callback,
											&parse_ie);
		if (parse_ie.data && (parse_ie.data_length >= 64)) {
      word channels                 =  READ_WORD((&parse_ie.data[ 4-2]));
			dword global_options          = READ_DWORD((&parse_ie.data[ 6-2]));
			dword b1_protocols            = READ_DWORD((&parse_ie.data[10-2]));
			dword b2_protocols            = READ_DWORD((&parse_ie.data[14-2]));
			dword b3_protocols            = READ_DWORD((&parse_ie.data[18-2]));
			dword manufacturer_features   = READ_DWORD((&parse_ie.data[46-2]));
			dword RTP_Payloads            = READ_DWORD((&parse_ie.data[50-2]));
			dword additional_RTP_payloads = READ_DWORD((&parse_ie.data[54-2]));

			*supported_channels = channels;

			DBG_LOG(("LAW and profile indication:"))
			DBG_LOG(("Channels      : %d", channels))
			DBG_LOG(("global_options: %08x", global_options))
			DBG_LOG(("b1 protocols  : %08x", b1_protocols))
			DBG_LOG(("b2 protocols  : %08x", b2_protocols))
			DBG_LOG(("b3 protocols  : %08x", b3_protocols))
			DBG_LOG(("manufacturer  : %08x", manufacturer_features))
			DBG_LOG(("RTP           : %08x", RTP_Payloads))
			DBG_LOG(("Additional RTP: %08x", additional_RTP_payloads))

			if (manufacturer_features & MANUFACTURER_FEATURE_RTP) {
				if (RTP_Payloads || additional_RTP_payloads) {
					features |= PROTCAP_TELINDUS;
				}
			}
			if ((b1_protocols & B1_MODEM_ALL_NEGOTIATE_SUPPORTED) || (b1_protocols & B1_MODEM_ASYNC_SUPPORTED)) {
				if (b2_protocols & B2_MODEM_EC_COMPRESSION_SUPPORTED) {
					features |= PROTCAP_V90D;
					features |= PROTCAP_TELINDUS;
				}
			}
			if (b2_protocols & B2_MODEM_EC_COMPRESSION_SUPPORTED) {
				features |= PROTCAP_V_42;
				features |= PROTCAP_TELINDUS;
			}
			if (b3_protocols & B3_T30_WITH_EXTENSIONS_SUPPORTED) {
				features |= PROTCAP_EXTD_FAX;
				features |= PROTCAP_TELINDUS;
			}
		}
	}

	return (features);
}

/* ----------------------------------------------------------------------
	Info Element Parser
	calls given callback routine for every info element found with 
	the following parameters:
	Type:  first octett of info element
	*IE :  Pointer to start of info element, beginning with 1st octett
	IElen: Overall octett length of info element (IE[IElen] points
	       to first octett after actual info element)
	SingleOctett: is 1 for a single octett element, else 0
	function returns number of found info elements
  ---------------------------------------------------------------------- */
static int ParseInfoElement (byte *Buf,
											int  Buflen,
											ParseInfoElementCallbackProc_t callback,
											void* callback_context) {
  int pos, cnt, len;
  
  for (pos=0, cnt=0; pos < Buflen;)
  {
    // Single Octett Element
    if (*Buf &0x80)
    {
      callback(callback_context, *Buf, Buf, 1 , 1);
      pos++;
      Buf++;
      cnt++;
      continue;
    }

    // Variable length Info Element
		if (*Buf) {
    	len = (int)(Buf[1])+2;
		} else {
    	len = 1;
		}
    callback(callback_context, *Buf, Buf, len, 0);
    pos += len;
    Buf = &Buf[len];
    cnt++;
  }
  return cnt;
}

static void diva_mtpx_parse_ie_callback (diva_mtpx_is_parse_t* context,
																				 byte  Type,
																				 byte* IE,
																				 int   IElen,
																				 int   SingleOctett) {
	diva_mtpx_is_parse_t* pC = (diva_mtpx_is_parse_t*)context;
	int limit = IElen, wanted = IElen;

	if (pC->data) {
		return;
	}
  if (pC->ie != 0) {
    /*
      Already found
      */
    return;
  }
  if (IE <= pC->element_after) {
    /*
      Look for next element
      */
    return;
  }

	if (pC->wanted_ie_length < 0) { /* Length <= wanted */
		limit  = pC->wanted_ie_length * (-1);
	} else if (pC->wanted_ie_length > 0) { /* Length == wanted */
		wanted  = pC->wanted_ie_length;
	}

	if ((Type == 0x7f) && (pC->wanted_escape_ie == 0x7f) && (SingleOctett == 0)) {
		if ((IElen >= 3) && (IE[2] == pC->wanted_ie) && (IElen <= limit) && (wanted == IElen)) {
			/*
	       0   1     2    3
				ESC,LEN+1,TYPE,BYTE0
				*/
			pC->ie = IE;
			pC->ie_length = IElen;
			if ((IElen - 3) > 0) {
				pC->data = &IE[3];
				pC->data_length = IElen - 3;
			}
		}
	} else if ((pC->wanted_escape_ie == 0x00) &&
						 (Type == pC->wanted_ie) && (IElen <= limit) && (wanted == IElen)) {
		/*
       0    1   2
			TYPE,LEN,BYTE0
			*/
		pC->ie = IE;
		pC->ie_length = IElen;

		if ((IElen - 2) > 0) {
			pC->data = &IE[2];
			pC->data_length = IElen - 2;
		}
	}
}

static void diva_mtpx_parse_sin_callback (diva_mtpx_is_parse_t* context,
																					byte  Type,
																					byte* IE,
																					int   IElen,
																					int   SingleOctett) {
	diva_mtpx_is_parse_t* pC = (diva_mtpx_is_parse_t*)context;

	if (pC->data) {
		return;
	}

	if (SingleOctett && (*IE == 0x96)) {
		pC->data_length = 1;
	} else if (SingleOctett && (*IE & 0x90)) {
		pC->data_length = 0;
	} else if ((pC->data_length == 1) && (Type == SIN)) {
		/*
       0    1   2
			TYPE,LEN,BYTE0
			*/
		pC->ie = IE;
		pC->ie_length = IElen;

		if ((IElen - 2) > 0) {
			pC->data = &IE[2];
			pC->data_length = IElen - 2;
		}
	}
}

