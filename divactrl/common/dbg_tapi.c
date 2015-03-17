
/*
 *
  Copyright (c) Eicon Networks, 2005.
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
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
#include <stdlib.h>
#include <string.h>
#include "dbgioctl.h"
/******************************************************************************/
extern void oidFilter (FILE *out, char *msgText) ;
/******************************************************************************/
static char *
CallStateString (unsigned long CST)
{
 switch (CST)
 {
 case 0x00000001:
  return ("LINE_CALL_STATE__IDLE") ;
 case 0x00000002:
  return ("LINE_CALL_STATE__OFFERING") ;
 case 0x00000004:
  return ("LINE_CALL_STATE__ACCEPTED") ;
 case 0x00000008:
  return ("LINE_CALL_STATE__DIALTONE") ;
 case 0x00000010:
  return ("LINE_CALL_STATE__DIALING") ;
 case 0x00000020:
  return ("LINE_CALL_STATE__RINGBACK") ;
 case 0x00000040:
  return ("LINE_CALL_STATE__BUSY") ;
 case 0x00000080:
  return ("LINE_CALL_STATE__SPECIALINFO") ;
 case 0x00000100:
  return ("LINE_CALL_STATE__CONNECTED") ;
 case 0x00000200:
  return ("LINE_CALL_STATE__PROCEEDING") ;
 case 0x00000400:
  return ("LINE_CALL_STATE__ONHOLD") ;
 case 0x00000800:
  return ("LINE_CALL_STATE__CONFERENCED") ;
 case 0x00001000:
  return ("LINE_CALL_STATE__ONHOLDPENDCONF") ;
 case 0x00002000:
  return ("LINE_CALL_STATE__ONHOLDPENDTRANSFER") ;
 case 0x00004000:
  return ("LINE_CALL_STATE__DISCONNECTED") ;
 case 0x00008000:
  return ("LINE_CALL_STATE__UNKNOWN") ;
/*
 * some private states within TAPI disconnect machine
 */
 case 0x10000000:
  return ("LINE_CALL_STATE__HANGUP") ;
 case 0x20000000:
  return ("LINE_CALL_STATE__DROP") ;
 default:
  return ("LINE_CALL_STATE__???") ;
 }
}
/******************************************************************************/
static char *
CallFeatureString (unsigned long LCF)
{
 switch (LCF)
 {
 case 0x00000001:
  return ("LINE_CALL_FEATURE__ACCEPT") ;
 case 0x00000002:
  return ("LINE_CALL_FEATURE__ADDTOCONF") ;
 case 0x00000004:
  return ("LINE_CALL_FEATURE__ANSWER") ;
 case 0x00000008:
  return ("LINE_CALL_FEATURE__BLINDTRANSFER") ;
 case 0x00000010:
  return ("LINE_CALL_FEATURE__COMPLETECALL") ;
 case 0x00000020:
  return ("LINE_CALL_FEATURE__COMPLETETRANSF") ;
 case 0x00000040:
  return ("LINE_CALL_FEATURE__DIAL") ;
 case 0x00000080:
  return ("LINE_CALL_FEATURE__DROP") ;
 case 0x00000100:
  return ("LINE_CALL_FEATURE__GATHERDIGITS") ;
 case 0x00000200:
  return ("LINE_CALL_FEATURE__GENERATEDIGITS") ;
 case 0x00000400:
  return ("LINE_CALL_FEATURE__GENERATETONE") ;
 case 0x00000800:
  return ("LINE_CALL_FEATURE__HOLD") ;
 case 0x00001000:
  return ("LINE_CALL_FEATURE__MONITORDIGITS") ;
 case 0x00002000:
  return ("LINE_CALL_FEATURE__MONITORMEDIA") ;
 case 0x00004000:
  return ("LINE_CALL_FEATURE__MONITORTONES") ;
 case 0x00008000:
  return ("LINE_CALL_FEATURE__PARK") ;
 case 0x00010000:
  return ("LINE_CALL_FEATURE__PREPAREADDCONF") ;
 case 0x00020000:
  return ("LINE_CALL_FEATURE__REDIRECT") ;
 case 0x00040000:
  return ("LINE_CALL_FEATURE__REMOVEFROMCONF") ;
 case 0x00080000:
  return ("LINE_CALL_FEATURE__SECURECALL") ;
 case 0x00100000:
  return ("LINE_CALL_FEATURE__SENDUSERUSER") ;
 case 0x00200000:
  return ("LINE_CALL_FEATURE__SETCALLPARAMS") ;
 case 0x00400000:
  return ("LINE_CALL_FEATURE__SETMEDIACONTROL") ;
 case 0x00800000:
  return ("LINE_CALL_FEATURE__SETTERMINAL") ;
 case 0x01000000:
  return ("LINE_CALL_FEATURE__SETUPCONF") ;
 case 0x02000000:
  return ("LINE_CALL_FEATURE__SETUPTRANSFER") ;
 case 0x04000000:
  return ("LINE_CALL_FEATURE__SWAPHOLD") ;
 case 0x08000000:
  return ("LINE_CALL_FEATURE__UNHOLD") ;
 case 0x10000000:
  return ("LINE_CALL_FEATURE__RELEASEUSERUSERINFO") ;
 case 0x20000000:
  return ("LINE_CALL_FEATURE__SETTREATMENT") ;
 case 0x40000000:
  return ("LINE_CALL_FEATURE__SETQOS") ;
 case 0x80000000:
  return ("LINE_CALL_FEATURE__SETCALLDATA") ;
 default:
  return ("LINE_CALL_FEATURE__???") ;
 }
}
/******************************************************************************/
static char *
CallInfoString (unsigned long CIS)
{
 switch (CIS)
 {
 case 0x00000001:
  return ("LINE_CALL_INFO_STATE__OTHER") ;
 case 0x00000002:
  return ("LINE_CALL_INFO_STATE__DEVSPECIFIC") ;
 case 0x00000004:
  return ("LINE_CALL_INFO_STATE__BEARERMODE") ;
 case 0x00000008:
  return ("LINE_CALL_INFO_STATE__RATE") ;
 case 0x00000010:
  return ("LINE_CALL_INFO_STATE__MEDIAMODE") ;
 case 0x00000020:
  return ("LINE_CALL_INFO_STATE__APPSPECIFIC") ;
 case 0x00000040:
  return ("LINE_CALL_INFO_STATE__CALLID") ;
 case 0x00000080:
  return ("LINE_CALL_INFO_STATE__RELATEDCALLID") ;
 case 0x00000100:
  return ("LINE_CALL_INFO_STATE__ORIGIN") ;
 case 0x00000200:
  return ("LINE_CALL_INFO_STATE__REASON") ;
 case 0x00000400:
  return ("LINE_CALL_INFO_STATE__COMPLETIONID") ;
 case 0x00000800:
  return ("LINE_CALL_INFO_STATE__NUMOWNERINCR") ;
 case 0x00001000:
  return ("LINE_CALL_INFO_STATE__NUMOWNERDECR") ;
 case 0x00002000:
  return ("LINE_CALL_INFO_STATE__NUMMONITORS") ;
 case 0x00004000:
  return ("LINE_CALL_INFO_STATE__TRUNK") ;
 case 0x00008000:
  return ("LINE_CALL_INFO_STATE__CALLERID") ;
 case 0x00010000:
  return ("LINE_CALL_INFO_STATE__CALLEDID") ;
 case 0x00020000:
  return ("LINE_CALL_INFO_STATE__CONNECTEDID") ;
 case 0x00040000:
  return ("LINE_CALL_INFO_STATE__REDIRECTIONID") ;
 case 0x00080000:
  return ("LINE_CALL_INFO_STATE__REDIRECTINGID") ;
 case 0x00100000:
  return ("LINE_CALL_INFO_STATE__DISPLAY") ;
 case 0x00200000:
  return ("LINE_CALL_INFO_STATE__USERUSERINFO") ;
 case 0x00400000:
  return ("LINE_CALL_INFO_STATE__HIGHLEVELCOMP") ;
 case 0x00800000:
  return ("LINE_CALL_INFO_STATE__LOWLEVELCOMP") ;
 case 0x01000000:
  return ("LINE_CALL_INFO_STATE__CHARGINGINFO") ;
 case 0x02000000:
  return ("LINE_CALL_INFO_STATE__TERMINAL") ;
 case 0x04000000:
  return ("LINE_CALL_INFO_STATE__DIALPARAMS") ;
 case 0x08000000:
  return ("LINE_CALL_INFO_STATE__MONITORMODES") ;
 case 0x10000000:
  return ("LINE_CALL_INFO_STATE__TREATMENT") ;
 case 0x20000000:
  return ("LINE_CALL_INFO_STATE__QOS") ;
 case 0x40000000:
  return ("LINE_CALL_INFO_STATE__CALLDATA") ;
 default:
  return ("LINE_CALL_INFO_STATE__???") ;
 }
}
/******************************************************************************/
static char *
CallPrivString (unsigned long LCP)
{
 switch (LCP)
 {
 case 0x00000001:
  return ("LINE_CALL_PRIVILEGE__NONE") ;
 case 0x00000002:
  return ("LINE_CALL_PRIVILEGE__MONITOR") ;
 case 0x00000004:
  return ("LINE_CALL_PRIVILEGE__OWNER") ;
 default:
  return ("LINE_CALL_PRIVILEGE__???") ;
 }
}
/******************************************************************************/
static char *
DevStateString (unsigned long DST)
{
 switch (DST)
 {
 case 0x00000001:
  return ("LINE_DEV_STATE__OTHER") ;
 case 0x00000002:
  return ("LINE_DEV_STATE__RINGING") ;
 case 0x00000004:
  return ("LINE_DEV_STATE__CONNECTED") ;
 case 0x00000008:
  return ("LINE_DEV_STATE__DISCONNECTED") ;
 case 0x00000010:
  return ("LINE_DEV_STATE__MSGWAITON") ;
 case 0x00000020:
  return ("LINE_DEV_STATE__MSGWAITOFF") ;
 case 0x00000040:
  return ("LINE_DEV_STATE__INSERVICE") ;
 case 0x00000080:
  return ("LINE_DEV_STATE__OUTOFSERVICE") ;
 case 0x00000100:
  return ("LINE_DEV_STATE__MAINTENANCE") ;
 case 0x00000200:
  return ("LINE_DEV_STATE__OPEN") ;
 case 0x00000400:
  return ("LINE_DEV_STATE__CLOSE") ;
 case 0x00000800:
  return ("LINE_DEV_STATE__NUMCALLS") ;
 case 0x00001000:
  return ("LINE_DEV_STATE__NUMCOMPLETIONS") ;
 case 0x00002000:
  return ("LINE_DEV_STATE__TERMINALS") ;
 case 0x00004000:
  return ("LINE_DEV_STATE__ROAMMODE") ;
 case 0x00008000:
  return ("LINE_DEV_STATE__BATTERY") ;
 case 0x00010000:
  return ("LINE_DEV_STATE__SIGNAL") ;
 case 0x00020000:
  return ("LINE_DEV_STATE__DEVSPECIFIC") ;
 case 0x00040000:
  return ("LINE_DEV_STATE__REINIT") ;
 case 0x00080000:
  return ("LINE_DEV_STATE__LOCK") ;
 case 0x00100000:
  return ("LINE_DEV_STATE__CAPSCHANGE") ;
 case 0x00200000:
  return ("LINE_DEV_STATE__CONFIGCHANGE") ;
 case 0x00400000:
  return ("LINE_DEV_STATE__TRANSLATECHANGE") ;
 case 0x00800000:
  return ("LINE_DEV_STATE__COMPLCANCEL") ;
 case 0x01000000:
  return ("LINE_DEV_STATE__REMOVED") ;
 default:
  return ("LINE_DEV_STATE__???") ;
 }
}
/******************************************************************************/
static char *
AddrStateString (unsigned long AST)
{
 switch (AST)
 {
 case 0x00000001:
  return ("LINE_ADDRESS_STATE__OTHER") ;
 case 0x00000002:
  return ("LINE_ADDRESS_STATE__DEVSPECIFIC") ;
 case 0x00000004:
  return ("LINE_ADDRESS_STATE__INUSEZERO") ;
 case 0x00000008:
  return ("LINE_ADDRESS_STATE__INUSEONE") ;
 case 0x00000010:
  return ("LINE_ADDRESS_STATE__INUSEMANY") ;
 case 0x00000020:
  return ("LINE_ADDRESS_STATE__NUMCALLS") ;
 case 0x00000040:
  return ("LINE_ADDRESS_STATE__FORWARD") ;
 case 0x00000080:
  return ("LINE_ADDRESS_STATE__TERMINALS") ;
 case 0x00000100:
  return ("LINE_ADDRESS_STATE__CAPSCHANGE") ;
 default:
  return ("LINE_ADDRESS_STATE__???") ;
 }
}
/******************************************************************************/
static char *
AddrFeatureString (unsigned long LAF)
{
 switch (LAF)
 {
 case 0x00000001:
  return ("LINE_ADDR_FEATURE__FORWARD") ;
 case 0x00000002:
  return ("LINE_ADDR_FEATURE__MAKECALL") ;
 case 0x00000004:
  return ("LINE_ADDR_FEATURE__PICKUP") ;
 case 0x00000008:
  return ("LINE_ADDR_FEATURE__SETMEDIACONTROL") ;
 case 0x00000010:
  return ("LINE_ADDR_FEATURE__SETTERMINAL") ;
 case 0x00000020:
  return ("LINE_ADDR_FEATURE__SETUPCONF") ;
 case 0x00000040:
  return ("LINE_ADDR_FEATURE__UNCOMPLETECALL") ;
 case 0x00000080:
  return ("LINE_ADDR_FEATURE__UNPARK") ;
 case 0x00000100:
  return ("LINE_ADDR_FEATURE__PICKUPHELD") ;
 case 0x00000200:
  return ("LINE_ADDR_FEATURE__PICKUPGROUP") ;
 case 0x00000400:
  return ("LINE_ADDR_FEATURE__PICKUPDIRECT") ;
 case 0x00000800:
  return ("LINE_ADDR_FEATURE__PICKUPWAITING") ;
 case 0x00001000:
  return ("LINE_ADDR_FEATURE__FORWARDFWD") ;
 case 0x00002000:
  return ("LINE_ADDR_FEATURE__FORWARDDND") ;
 default:
  return ("LINE_ADDR_FEATURE__???") ;
 }
}
/******************************************************************************/
static char *
BearerModeString (unsigned long LBM)
{
 switch (LBM)
 {
 case 0x00000001:
  return ("LINE_BEARER_MODE__VOICE") ;
 case 0x00000002:
  return ("LINE_BEARER_MODE__SPEECH") ;
 case 0x00000004:
  return ("LINE_BEARER_MODE__MULTIUSE") ;
 case 0x00000008:
  return ("LINE_BEARER_MODE__DATA") ;
 case 0x00000010:
  return ("LINE_BEARER_MODE__ALTSPEECHDATA") ;
 case 0x00000020:
  return ("LINE_BEARER_MODE__NONCALLSIGNALING") ;
 case 0x00000040:
  return ("LINE_BEARER_MODE__PASSTHROUGH") ;
 case 0x00000080:
  return ("LINE_BEARER_MODE__RESTRICTEDDATA") ;
 default:
  return ("LINE_BEARER_MODE__???") ;
 }
}
/******************************************************************************/
static char *
DisconnectModeString (unsigned long LDM)
{
 switch (LDM)
 {
 case 0x00000001:
  return ("LINE_DISCONNECT_MODE__NORMAL") ;
 case 0x00000002:
  return ("LINE_DISCONNECT_MODE__UNKNOWN") ;
 case 0x00000004:
  return ("LINE_DISCONNECT_MODE__REJECT") ;
 case 0x00000008:
  return ("LINE_DISCONNECT_MODE__PICKUP") ;
 case 0x00000010:
  return ("LINE_DISCONNECT_MODE__FORWARDED") ;
 case 0x00000020:
  return ("LINE_DISCONNECT_MODE__BUSY") ;
 case 0x00000040:
  return ("LINE_DISCONNECT_MODE__NOANSWER") ;
 case 0x00000080:
  return ("LINE_DISCONNECT_MODE__BADADDRESS") ;
 case 0x00000100:
  return ("LINE_DISCONNECT_MODE__UNREACHABLE") ;
 case 0x00000200:
  return ("LINE_DISCONNECT_MODE__CONGESTION") ;
 case 0x00000400:
  return ("LINE_DISCONNECT_MODE__INCOMPATIBLE") ;
 case 0x00000800:
  return ("LINE_DISCONNECT_MODE__UNAVAIL") ;
 case 0x00001000:
  return ("LINE_DISCONNECT_MODE__NODIALTONE") ;
 case 0x00002000:
  return ("LINE_DISCONNECT_MODE__NUMBERCHANGED") ;
 case 0x00004000:
  return ("LINE_DISCONNECT_MODE__OUTOFORDER") ;
 case 0x00008000:
  return ("LINE_DISCONNECT_MODE__TEMPFAILURE") ;
 case 0x00010000:
  return ("LINE_DISCONNECT_MODE__QOSUNAVAIL") ;
 case 0x00020000:
  return ("LINE_DISCONNECT_MODE__BLOCKED") ;
 case 0x00040000:
  return ("LINE_DISCONNECT_MODE__DONOTDISTURB") ;
 case 0x00080000:
  return ("LINE_DISCONNECT_MODE__CANCELLED") ;
 default:
  return ("LINE_DISCONNECT_MODE__???") ;
 }
}
/******************************************************************************/
static char *
MediaModeString (unsigned long LMM)
{
 switch (LMM)
 {
 case 0x00000002:
  return ("LINE_MEDIA_MODE__UNKNOWN") ;
 case 0x00000004:
  return ("LINE_MEDIA_MODE__INTERACTIVEVOICE") ;
 case 0x00000008:
  return ("LINE_MEDIA_MODE__AUTOMATEDVOICE") ;
 case 0x00000010:
  return ("LINE_MEDIA_MODE__DATAMODEM") ;
 case 0x00000020:
  return ("LINE_MEDIA_MODE__G3FAX") ;
 case 0x00000040:
  return ("LINE_MEDIA_MODE__TDD") ;
 case 0x00000080:
  return ("LINE_MEDIA_MODE__G4FAX") ;
 case 0x00000100:
  return ("LINE_MEDIA_MODE__DIGITALDATA") ;
 case 0x00000200:
  return ("LINE_MEDIA_MODE__TELETEX") ;
 case 0x00000400:
  return ("LINE_MEDIA_MODE__VIDEOTEX") ;
 case 0x00000800:
  return ("LINE_MEDIA_MODE__TELEX") ;
 case 0x00001000:
  return ("LINE_MEDIA_MODE__MIXED") ;
 case 0x00002000:
  return ("LINE_MEDIA_MODE__ADSI") ;
 case 0x00004000:
  return ("LINE_MEDIA_MODE__VOICEVIEW") ;
 default:
  return ("LINE_MEDIA_MODE__???") ;
 }
}
/******************************************************************************/
static char *
IsdnEventString (unsigned long ev_type)
{
 switch (ev_type & ~0x08000000)
 {
 case 0x00000001:
  return ("ISDN_EVENT_DIALTONE") ;
 case 0x00000002:
  return ("ISDN_EVENT_DIALING") ;
 case 0x00000004:
  return ("ISDN_EVENT_PROCEEDING") ;
 case 0x00000008:
  return ("ISDN_EVENT_ALERT") ;
 case 0x00000010:
  return ("ISDN_EVENT_CHARGING") ;
 case 0x00000020:
  return ("ISDN_EVENT_CHARGETICK") ;
 case 0x00000040:
  return ("ISDN_EVENT_DATE") ;
 case 0x00000080:
  return ("ISDN_EVENT_RING") ;
 case 0x00000100:
  return ("ISDN_EVENT_CONNECT") ;
 case 0x00000200:
  return ("ISDN_EVENT_DISCONNECT") ;
 case 0x00000400:
  return ("ISDN_EVENT_IDLE") ;
 case 0x00000800:
  return ("ISDN_EVENT_REASSIGN_HW") ;
 case 0x00001000:
  return ("ISDN_EVENT_SEND_DONE") ;
 case 0x00002000:
  return ("ISDN_EVENT_RECV_DONE") ;
 case 0x00003000:
  return ("ISDN_EVENT_TRANSFER") ;
 case 0x000037FF:
  return ("ISDN_EVENTS_ALL") ;
 default:
  return ("ISDN_EVENT_???") ;
 }
}
/******************************************************************************/
static char *
IsdnCauseString (unsigned long cause)
{
 switch (cause & ~0x02000000)
 {
 case 0x00000000:
  return ("ISDN_CAUSE_OK") ;
 case 0x00000001:
  return ("ISDN_CAUSE_UNKNOWN") ;
 case 0x00000002:
  return ("ISDN_CAUSE_NORMAL") ;
 case 0x00000004:
  return ("ISDN_CAUSE_REJECT") ;
 case 0x00000008:
  return ("ISDN_CAUSE_IGNORE") ;
 case 0x00000010:
  return ("ISDN_CAUSE_BUSY") ;
 case 0x00000020:
  return ("ISDN_CAUSE_NO_ANSWER") ;
 case 0x00000040:
  return ("ISDN_CAUSE_BAD_ADDRESS") ;
 case 0x00000080:
  return ("ISDN_CAUSE_UNREACHABLE") ;
 case 0x00000100:
  return ("ISDN_CAUSE_HARDWARE") ;
 case 0x00000200:
  return ("ISDN_CAUSE_INCOMPATIBLE") ;
 case 0x00000400:
  return ("ISDN_CAUSE_UNAVAIL") ;
 case 0x00000800:
  return ("ISDN_CAUSE_CONGESTION") ;
 case 0x00010000:
  return ("ISDN_CAUSE_ASSIGN_OK") ; // IAL internal
 case 0x00020000:
  return ("ISDN_CAUSE_ASSIGN_KO") ; // IAL internal
 case 0x00030000:
  return ("ISDN_CAUSE_REMOVE_OK") ; // IAL internal
 case 0x00040000:
  return ("ISDN_CAUSE_REMOVE_KO") ; // IAL internal
 case 0x00050000:
  return ("ISDN_CAUSE_E_AGAIN") ;  // IAL internal
 case 0x00060000:
  return ("ISDN_CAUSE_E_RETRY") ;  // IAL internal
 case 0x00008000:
  return ("ISDN_CAUSE_OUT_OF_SERVICE") ;
 case 0x00090000:
  return ("ISDN_CAUSE_CABLE") ;  // IAL internal
 case 0x000A0000:
  return ("ISDN_CAUSE_SWITCH") ;  // IAL internal
 case 0x00100000:
  return ("ISDN_CAUSE_FAILURE") ;
 case 0x00200000:
  return ("ISDN_CAUSE_TIMEOUT") ;
 case 0x00400000:
  return ("ISDN_CAUSE_SHUTDOWN") ;
 default:
  return ("ISDN_CAUSE_???") ;
 }
}
/*****************************************************************************/
static char *
OidSubstitute (unsigned long oid)
{
 static char buffer[64] ;
 sprintf (&buffer[0], "OID_0x%08lX_", oid) ;
 return (&buffer[0]) ;
}
/*****************************************************************************/
typedef char*  (* identFunc)(unsigned long) ;
typedef struct
{ char     *abbrev ;
 long      abSize ;
 identFunc ident ;
} tapiIdent ;
static tapiIdent tapiIdentTab[] =
{ { "OID_0x"       ,  6, OidSubstitute},
 { "AST_0x"       ,  6, AddrStateString},
 { "CST_0x"       ,  6, CallStateString},
 { "CIS_0x"       ,  6, CallInfoString},
 { "DST_0x"       ,  6, DevStateString},
 { "LDM_0x"       ,  6, DisconnectModeString},
 { "LAF_0x"       ,  6, AddrFeatureString},
 { "LBM_0x"       ,  6, BearerModeString},
 { "LCF_0x"       ,  6, CallFeatureString},
 { "LCP_0x"       ,  6, CallPrivString},
 { "LMM_0x"       ,  6, MediaModeString},
 { "ISDN_EV_0x"   , 10, IsdnEventString},
 { "ISDN_CAUSE_0x", 13, IsdnCauseString},
 { ((void*)0)            ,  0, ((void*)0) }
} ;
/*****************************************************************************/
void
tapiFilter (FILE *out, DbgMessage *msg)
{
 char          *buffer, *outPtr, *ptr1, *ptr2 ;
 char           inputLine[MSG_FRAME_MAX_SIZE] ;
 char           outputLine[MSG_FRAME_MAX_SIZE] ;
 unsigned long  val ;
 tapiIdent     *tid ;
/*
 * copy input line and append a terminating zero
 */
 memcpy (&inputLine[0], &msg->data[0], msg->size) ;
 memset (&outputLine[0], '\0', 100) ;
 inputLine[msg->size] = '\0' ;
 buffer = &inputLine[0] ;
 outPtr = &outputLine[0] ;
/*
 * scan string for '\`' pairs
 */
 while ( (ptr1 = strchr (buffer, '`')) != ((void*)0)  )
 {
  ptr2 = strchr (ptr1 + 1, '`') ;
  if ( !ptr2 )
  {
   break ;
  }
  *ptr1++ = '\0' ;
  *ptr2++ = '\0' ;
/*
 * scan string for all possible constants
 */
  for ( tid = &tapiIdentTab[0] ; tid->abbrev ; ++tid )
  {
   if ( !strncmp (ptr1, tid->abbrev, tid->abSize) )
   {
    val = strtoul (&ptr1[tid->abSize], ((void*)0) , 16) ;
    ptr1 = (* tid->ident)(val) ;
    break ;
   }
  }
  strcpy (outPtr, buffer) ;
  strcat (outPtr, ptr1) ;
  outPtr += strlen (outPtr) ;
  buffer = ptr2 ;
 }
/*
 * call oidFilter for compatibility reasons
 */
 if ( buffer[0] )
 {
  strcat (outPtr, buffer) ;
 }
 oidFilter (out, &outputLine[0]) ;
}
