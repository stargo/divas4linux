/*****************************************************************************
 *
 * (c) COPYRIGHT 2005-2008       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be copied,
 * transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *----------------------------------------------------------------------------*/



/* -------------------------------------------------  
This LOG.C does output to BUFFER  
-------------------------------------------------- */  
#include "platform.h"  
/*****************************************************************************
 *
 * File:           log.c
 * Component:      Maint
 * Description:    xlog interpretation
 *
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include "cau_1tr6.h"
#include "cau_q931.h"
#include "pc.h"
int diva_convert_be2le = 0;
word diva_word2le (word w);
/*----------------------------------------------------------------*/
/* Prints into a buffer instead into a file if PRT2BUF is defined */
  #define FILE byte
  #define fprintf o=o+(word)sprintf
  #define Out (char*)&stream[o]
word o;  /* index to buffer, not used if printed to file */
/*----------------------------------------------------------------*/
/* Define max byte count of a message to print before "cont"      */
/* variable can be set externally to change this behaviour        */
word maxout = 30;
#define LOBYTE(w)  ((byte)(w))
#define HIBYTE(w)  ((byte)(((word)(w) >> 8) & 0xFF))
struct l1s {
  short length;
  unsigned char i[22];
};
struct l2s {
  short code;
  short length;
  unsigned char i[20];
};
struct nu_info_s {
  short code;
  short Id;
  short info;
  short info1;
};
union par {
  char text[42];
  struct l1s l1;
  struct l2s l2;
  struct nu_info_s nu_info;
};
typedef struct xlog_s XLOG;
struct xlog_s {
  short code;
  union par info;
};
typedef struct log_s LOG;
struct log_s {
  word length;
  word code;
  word timeh;
  word timel;
  byte buffer[80];
};
#define ZERO_BASED_INDEX_VALID(x,t) ((unsigned)x < sizeof(t)/sizeof(t[0]))
#define ONE_BASED_INDEX_VALID(x,t) ((unsigned)x <= sizeof(t)/sizeof(t[0]))
static char *ll_name[13] = {
  "LL_UDATA",
  "LL_ESTABLISH",
  "LL_RELEASE",
  "LL_DATA",
  "LL_LOCAL",
  "LL_REMOTE",
  "",
  "LL_TEST",
  "LL_MDATA",
  "LL_BUDATA",
  "",
  "LL_XID",
  "LL_XID_R"
};
static char *ns_name[13] = {
  "N-MDATA",
  "N-CONNECT",
  "N-CONNECT ACK",
  "N-DISC",
  "N-DISC ACK",
  "N-RESET",
  "N-RESET ACK",
  "N-DATA",
  "N-EDATA",
  "N-UDATA",
  "N-BDATA",
  "N-DATA ACK",
  "N-EDATA ACK"
};
static char * l1_name[] = {
  "SYNC_LOST",
  "SYNC_GAINED",
  "L1_DOWN",
  "L1_UP",
  "ACTIVATION_REQ",
  "PS1_DOWN",
  "PS1_UP",
  "L1_FC1",
  "L1_FC2",
  "L1_FC3",
  "L1_FC4"
};
static struct {
  byte code;
  char * name;
} capi20_command[] = {
  { 0x01, "ALERT" },
  { 0x02, "CONNECT" },
  { 0x03, "CONNECT_ACTIVE" },
  { 0x04, "DISCONNECT" },
  { 0x05, "LISTEN" },
  { 0x08, "INFO" },
  { 0x41, "SELECT_B" },
  { 0x80, "FACILITY" },
  { 0x82, "CONNECT_B3" },
  { 0x83, "CONNECT_B3_ACTIVE" },
  { 0x84, "DISCONNECT_B3" },
  { 0x86, "DATA_B3" },
  { 0x87, "RESET_B3" },
  { 0x88, "CONNECT_B3_T90_ACTIVE" },
  { 0xd4, "PORTABILITY" }, /* suspend/resume */
  { 0xff, "MANUFACTURER" }
};
static struct {
  byte code;
  char * name;
} capi11_command[] = {
  { 0x01, "RESET_B3" },
  { 0x02, "CONNECT" },
  { 0x03, "CONNECT_ACTIVE" },
  { 0x04, "DISCONNECT" },
  { 0x05, "LISTEN" },
  { 0x06, "GET_PARMS" },
  { 0x07, "INFO" },
  { 0x08, "DATA" },
  { 0x09, "CONNECT_INFO" },
  { 0x40, "SELECT_B2" },
  { 0x80, "SELECT_B3" },
  { 0x81, "LISTEN_B3" },
  { 0x82, "CONNECT_B3" },
  { 0x83, "CONNECT_B3_ACTIVE" },
  { 0x84, "DISCONNECT_B3" },
  { 0x85, "GET_B3_PARMS" },
  { 0x86, "DATA_B3" },
  { 0x87, "HANDSET" },
};
static struct {
  byte code;
  char * name;
} capi20_subcommand[] = {
  { 0x80, "REQ" },
  { 0x81, "CON" },
  { 0x82, "IND" },
  { 0x83, "RES" }
};
static struct {
  byte code;
  char * name;
} capi11_subcommand[] = {
  { 0x00, "REQ" },
  { 0x01, "CON" },
  { 0x02, "IND" },
  { 0x03, "RES" }
};
#define PD_MAX 4
static struct {
  byte code;
  char * name;
} pd[PD_MAX] = {
  { 0x08, "Q.931  " },
  { 0x0c, "CORNET " },
  { 0x40, "DKZE   " },
  { 0x41, "1TR6   " }
};
#define MT_MAX 34
static struct {
  byte code;
  char * name;
} mt[MT_MAX] = {
  { 0x01, "ALERT" },
  { 0x02, "CALL_PROC" },
  { 0x03, "PROGRESS" },
  { 0x05, "SETUP" },
  { 0x07, "CONN" },
  { 0x0d, "SETUP_ACK" },
  { 0x0f, "CONN_ACK" },
  { 0x20, "USER_INFO" },
  { 0x21, "SUSP_REJ" },
  { 0x22, "RESUME_REJ" },
  { 0x24, "HOLD_REQ" },
  { 0x25, "SUSP" },
  { 0x26, "RESUME" },
  { 0x28, "HOLD_ACK" },
  { 0x30, "HOLD_REJ" },
  { 0x31, "RETRIEVE_REQ" },
  { 0x33, "RETRIEVE_ACK" },
  { 0x37, "RETRIEVE_REJ" },
  { 0x2d, "SUSP_ACK" },
  { 0x2e, "RESUME_ACK" },
  { 0x45, "DISC" },
  { 0x46, "RESTART" },
  { 0x4d, "REL" },
  { 0x4e, "RESTART_ACK" },
  { 0x5a, "REL_COM" },
  { 0x60, "SEGMENT" },
  { 0x62, "FACILITY" },
  { 0x64, "REGISTER" },
  { 0x6d, "GINFO" },
  { 0x6e, "NOTIFY" },
  { 0x75, "STATUS_ENQ" },
  { 0x79, "CONGESTION_CTRL" },
  { 0x7d, "STATUS" },
  { 0x7b, "INFO" }
};
#define IE_MAX 41
static struct {
  word code;
  char * name;
} ie[IE_MAX] = {
  { 0x00a1, "Sending complete" },
  { 0x00a0, "MORE" },
  { 0x00b0, "Congestion level" },
  { 0x00d0, "Repeat indicator" },
  { 0x0000, "SMSG" },
  { 0x0004, "Bearer Capability" },
  { 0x0008, "Cause" },
  { 0x000c, "Connected Address" },
  { 0x0010, "Call Identity" },
  { 0x0014, "Call State" },
  { 0x0018, "Channel Id" },
  { 0x001a, "Advice of Charge" },
  { 0x001c, "Facility" },
  { 0x001e, "Progress Indicator" },
  { 0x0020, "NSF" },
  { 0x0027, "Notify Indicator" },
  { 0x0028, "Display" },
  { 0x0029, "Date" },
  { 0x002c, "Key " },
  { 0x0034, "Signal" },
  { 0x003a, "SPID" },
  { 0x003b, "Endpoint Id"  },
  { 0x004c, "Connected Number" },
  { 0x006c, "Calling Party Number" },
  { 0x006d, "Calling Party Subaddress" },
  { 0x0070, "Called Party Number" },
  { 0x0071, "Called Party Subaddress" },
  { 0x0074, "Redirecting Number" },
  { 0x0076, "Redirection Number" },
  { 0x0078, "TNET" },
  { 0x0079, "Restart Indicator" },
  { 0x007c, "LLC" },
  { 0x007d, "HLC" },
  { 0x007e, "User User Info" },
  { 0x007f, "ESC" },
  { 0x051a, "Advice of Charge" },
  { 0x061a, "Advice of Charge" },
  { 0x0601, "Service Indicator" },
  { 0x0602, "Charging Info" },
  { 0x0603, "Date" },
  { 0x0607, "Called Party State" }
};
static char * sig_state[] = {
  "Null state",
  "Call initiated",
  "Overlap sending",
  "Outgoing call proceeding",
  "Call delivered",
  "",
  "Call present",
  "Call received",
  "Connect request",
  "Incoming call proceeding",
  "Active",
  "Disconnect request",
  "Disconnect indication",
  "",
  "",
  "Suspend request",
  "",
  "Resume request",
  "",
  "Release request",
  "",
  "",
  "",
  "",
  "",
  "Overlap receiving"
};
static char * sig_timeout[] = {
  "",
  "T303",
  "",
  "",
  "",
  "",
  "",
  "",
  "T313",
  "",
  "T309",
  "T305",
  "",
  "",
  "",
  "T319",
  "",
  "T318",
  "",
  "T308",
  "",
  "",
  "",
  "",
  "",
  "T302"
};
struct msg_s {
  word code;
  word length;
  byte info[1000];
} message = {0,0,{0}};
/* MIPS CPU exeption context structure */
struct xcptcontext {
    dword       sr;
    dword       cr;
    dword       epc;
    dword       vaddr;
    dword       regs[32];
    dword       mdlo;
    dword       mdhi;
    dword       reseverd;
    dword       xclass;
}xcp;
/* MIPS CPU exception classes */
#define XCPC_GENERAL    0
#define XCPC_TLBMISS    1
#define XCPC_XTLBMISS   2
#define XCPC_CACHEERR   3
#define XCPC_CLASS      0x0ff
#define XCPC_USRSTACK   0x100
#define XCPTINTR        0
#define XCPTMOD         1
#define XCPTTLBL        2
#define XCPTTLBS        3
#define XCPTADEL        4
#define XCPTADES        5
#define XCPTIBE         6
#define XCPTDBE         7
#define XCPTSYS         8
#define XCPTBP          9
#define XCPTRI          10
#define XCPTCPU         11
#define XCPTOVF         12
#define XCPTTRAP        13
#define XCPTVCEI        14
#define XCPTFPE         15
#define XCPTCP2         16
#define XCPTRES17       17
#define XCPTRES18       18
#define XCPTRES19       19
#define XCPTRES20       20
#define XCPTRES21       21
#define XCPTRES22       22
#define XCPTWATCH       23
#define XCPTRES24       24
#define XCPTRES25       25
#define XCPTRES26       26
#define XCPTRES27       27
#define XCPTRES28       28
#define XCPTRES29       29
#define XCPTRES30       30
#define XCPTVCED        31
#define NXCPT           32
/* exception classes */
#define XCPC_GENERAL    0
#define XCPC_TLBMISS    1
#define XCPC_XTLBMISS   2
#define XCPC_CACHEERR   3
#define XCPC_CLASS      0x0ff
#define XCPC_USRSTACK   0x100
static char hex_digit_table[0x10] =
  {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static char rev_hex_digit_table[0x10] =
  {'0','8','4','C','2','A','6','E','1','9','5','D','3','B','7','F'};
/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/
word xlog(FILE * stream,XLOG * buffer);
static void xlog_dump_buffer (FILE *stream, byte *buf, word len, char *tmp);
void xlog_sig(FILE * stream, struct msg_s * message);
void call_failed_event(FILE * stream, struct msg_s * message, word code);
void display_q931_message(FILE * stream, struct msg_s * message);
void display_ie(FILE * stream, byte pd, word w, byte * ie);
void spid_event(FILE * stream, struct msg_s * message, word code);
static void rc2str (byte rc, char* p);
static void encode_sig_req (byte req, char* p, int is_req);
static void encode_man_req (byte req, char* p, int is_req);
word xlog(FILE *stream, XLOG * buffer)
{
  word i;
  word n;
  word code;
  word lim;
  word Id;
  word new_msg_code   = 0;
  word new_msg_length = 0;
  byte nl_sec;
  char *p;
  static char tmp[256];
  o = 0;
  buffer->code = diva_word2le (buffer->code);
  if((byte)buffer->code==21 || (byte)buffer->code==22){
    new_msg_code = buffer->code;
    new_msg_length = 0;
  }
  switch((byte)buffer->code) {
  case 1:
    n = diva_word2le(buffer->info.l1.length);
    if(((short)(buffer->code)) >= 0) fprintf(Out,"B%d-X(%03d)",buffer->code>>8,n);
    else fprintf(Out,"B-%d-X(%03d)",~(buffer->code>>8),n);
    xlog_dump_buffer (stream, buffer->info.l1.i, n, tmp);
    break;
  case 2:
    n = diva_word2le(buffer->info.l1.length);
    if(((short)(buffer->code)) >= 0) fprintf(Out,"B%d-R(%03d)",buffer->code>>8,n);
    else fprintf(Out,"B-%d-R(%03d)",~(buffer->code>>8),n);
    xlog_dump_buffer (stream, buffer->info.l1.i, n, tmp);
    break;
  case 3:
    n = diva_word2le(buffer->info.l1.length);
    fprintf(Out,"D-X(%03d)",n);
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 4:
    n = diva_word2le(buffer->info.l1.length);
    fprintf(Out,"D-R(%03d)",n);
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 5:
    n = diva_word2le(buffer->info.l2.length);
    fprintf(Out,"SIG-EVENT(%03d)%04X -",n,diva_word2le(buffer->info.l2.code));
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l2.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 6:
    code = diva_word2le(buffer->info.l2.code);
    if(code && code <= 13)
      fprintf(Out,"%s IND",ll_name[code-1]);
    else
      fprintf(Out,"UNKNOWN LL IND");
    break;
  case 7:
    code = diva_word2le(buffer->info.l2.code);
    if(code && code <= 13)
      fprintf(Out,"%s REQ",ll_name[code-1]);
    else
      fprintf(Out,"UNKNOWN LL REQ");
    break;
  case 8:
    n = diva_word2le(buffer->info.l2.length);
    fprintf(Out,"DEBUG%04X -",diva_word2le(buffer->info.l2.code));
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l2.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 9:
    *(word*)&tmp[0] = diva_word2le(*(word*)&buffer->info.text[0]);
    fprintf(Out,"MDL-ERROR(%s)", &tmp[0]);
    break;
  case 10:
    fprintf(Out,"UTASK->PC(%02X)",diva_word2le(buffer->info.l2.code));
    break;
  case 11:
    fprintf(Out,"PC->UTASK(%02X)",diva_word2le(buffer->info.l2.code));
    break;
  case 12:
    n = diva_word2le(buffer->info.l1.length);
    fprintf(Out,"X-X(%03d)",n);
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 13:
    n = diva_word2le(buffer->info.l1.length);
    fprintf(Out,"X-R(%03d)",n);
    for(i=0;i<n && i<maxout;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    if(n>i) fprintf(Out," cont");
    break;
  case 14:
    code = diva_word2le(buffer->info.nu_info.code)-1;
    Id   = diva_word2le(buffer->info.nu_info.Id);
    nl_sec = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
    if ((byte)(code+1) == N_COMBI_IND) {
      fprintf(Out,"[%02x] %s IND Id:%02x, Ch:%02x", nl_sec, "N-CDATA",
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else if((code &0x0f)<=12)
      fprintf(Out,"[%02x] %s IND Id:%02x, Ch:%02x", nl_sec,
              ns_name[code &0x0f],
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    else
      fprintf(Out,"[%02x] N-UNKNOWN NS IND(%02x) Id:%02x, Ch:%02x", nl_sec,
              (byte)(code+1),
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    break;
  case 15:
    code   = diva_word2le(buffer->info.nu_info.code)-1;
    Id     = diva_word2le(buffer->info.nu_info.Id);
    nl_sec = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
    if (((byte)Id) == NL_ID) {
      fprintf(Out,"[%02x] %s REQ Id:%s, Ch:%02x", nl_sec, "N-ASSIGN",
              "NL_ID", HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else if ((byte)(code+1) == REMOVE) {
      fprintf(Out,"[%02x] %s REQ Id:%02x, Ch:%02x", nl_sec, "N-REMOVE",
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else if ((byte)(code+1) == N_XON) {
      fprintf(Out,"[%02x] %s REQ Id:%02x, Ch:%02x", nl_sec, "N-XON",
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else if ((byte)(code+1) == UREMOVE) {
      fprintf(Out,"[%02x] %s REQ Id:%02x, Ch:%02x", nl_sec, "N-UREMOVE",
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else if((code & 0x0f)<=12) {
      fprintf(Out,"[%02x] %s REQ Id:%02x, Ch:%02x", nl_sec, ns_name[code &0x0f],
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else {
      fprintf(Out,"[%02x] N-UNKNOWN NS REQ(%02x) Id:%02x, Ch:%02x", nl_sec,
              (byte)(code+1),
              (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    }
    break;
  case 16:
    fprintf(Out,"TASK %02i: %s",
            diva_word2le(buffer->info.l2.code),buffer->info.l2.i);
    break;
  case 18:
    code = diva_word2le(buffer->info.l2.code);
    fprintf(Out,"IO-REQ %02x",code);
    break;
  case 19:
    code = diva_word2le(buffer->info.l2.code);
    fprintf(Out,"IO-CON %02x",code);
    break;
  case 20:
    code = diva_word2le(buffer->info.l2.code);
    if ( ZERO_BASED_INDEX_VALID(code,l1_name) )
      fprintf(Out, "%s", l1_name[code]);
    else
      fprintf(Out, "UNKNOWN L1 STATE (0x%04x)", code);
    break;
  case 21:
    if (!(buffer->code & 0x0100)
     || (((word)(buffer->code)) == ((word)(message.code | 0x0100))))
    {
      if (!(buffer->code & 0x0100) && (buffer->code & 0xfe00))
        message.length = 0;
      for(i=0;i<(word)diva_word2le(buffer->info.l2.length);i++)
        message.info[message.length++] = buffer->info.l2.i[i];
      if(diva_word2le(buffer->info.l2.code)) {
        switch(diva_word2le(buffer->info.l2.code) &0xff) {
        case 1:
          xlog_sig(stream,&message);
          break;
        default:
          fprintf(Out,"MSG%04X -",diva_word2le(buffer->info.l2.code));
          for(i=0;i<message.length;i++)
            fprintf(Out," %02X",message.info[i]);
          break;
        }
      }
      else {
        new_msg_length = message.length;
        fprintf(Out,"MORE");
      }
    }
    else {
      new_msg_code &= 0x00ff;
      fprintf(Out,"MORE");
    }
    break;
  case 22:
    if (!(buffer->code & 0x0100)
     || (((word)(buffer->code)) == ((word)(message.code | 0x0100))))
    {
      if (!(buffer->code & 0x0100) && (buffer->code & 0xfe00))
        message.length = 0;
      for(i=0;i<(word)diva_word2le(buffer->info.l2.length);i++)
        message.info[message.length++] = buffer->info.l2.i[i];
      if(diva_word2le(buffer->info.l2.code)) {
        switch(diva_word2le(buffer->info.l2.code) &0xff) {
        case 1:
          call_failed_event(stream, &message, diva_word2le(buffer->info.l2.code));
          break;
        case 2:
          spid_event(stream, &message, diva_word2le(buffer->info.l2.code));
          break;
        default:
          fprintf(Out,"EVENT%04X -",diva_word2le(buffer->info.l2.code));
          for(i=0;i<message.length;i++)
            fprintf(Out," %02X",message.info[i]);
          break;
        }
      }
      else {
        new_msg_length = message.length;
        fprintf(Out,"MORE");
      }
    }
    else {
      new_msg_code &= 0x00ff;
      fprintf(Out,"MORE");
    }
    break;
  case 23:
    p = tmp;
    for(i=0; i<(word)diva_word2le(buffer->info.l1.length); i+=2) {
      *(p++) = ' ';
      *(p++) = hex_digit_table[buffer->info.l1.i[i+1] >> 4];
      *(p++) = hex_digit_table[buffer->info.l1.i[i+1] & 0xf];
      *(p++) = hex_digit_table[buffer->info.l1.i[i] >> 4];
      *(p++) = hex_digit_table[buffer->info.l1.i[i] & 0xf];
    }
    *p = '\0';
    if(((short)(buffer->code)) >= 0)
      fprintf(Out,"[*,%d] EYE%s",buffer->code>>8,tmp);
    else
      fprintf(Out,"[*,-%d] EYE%s",~(buffer->code>>8),tmp);
    break;
  case 24:
    fprintf(Out, "%s", buffer->info.text);
    break;
  case 25:
    n = diva_word2le(buffer->info.l1.length);
    if(((short)(buffer->code)) >= 0)
      fprintf(Out,"Audio%d (%03d)",buffer->code>>8,n);
    else
      fprintf(Out,"Audio-%d (%03d)",~(buffer->code>>8),n);
    if (n != 0)
    {
      i = 0;
      lim = 0;
      do
      {
        fprintf(Out,"\n  (%04d) - ",i);
        lim = (n - lim > 20) ? lim + 20 : n;
        p = tmp;
        do
        {
          *(p++) = ' ';
          *(p++) = rev_hex_digit_table[buffer->info.l1.i[i+1] & 0xf];
          *(p++) = rev_hex_digit_table[buffer->info.l1.i[i+1] >> 4];
          *(p++) = rev_hex_digit_table[buffer->info.l1.i[i] & 0xf];
          *(p++) = rev_hex_digit_table[buffer->info.l1.i[i] >> 4];
          i += 2;
        } while (i < lim);
        *p = '\0';
        if(((short)(buffer->code)) >= 0)
          fprintf(Out,"[*,%d] SAMPLE[]%s",buffer->code>>8,tmp);
        else
          fprintf(Out,"[*,-%d] SAMPLE[]%s",~(buffer->code>>8),tmp);
      } while (i < n);
    }
    break;
  case 50:
    //--- register dump comes in two blocks, each 80 Bytes long. Print
    //--- content after second part has been arrived
    if (0 == ((byte)(buffer->code>>8)))  // first part
    {
      for (i=0; i<20; i++)
        ((dword   *)(&xcp.sr))[i] = ((dword   *)&buffer->info.text)[i];
      fprintf(Out,"Register Dump part #1 received.");
      break;  // wait for next
    }
    for (i=0; i<20; i++)
      ((dword   *)(&xcp.sr))[i+20] = ((dword   *)&buffer->info.text)[i];
    fprintf(Out,"Register Dump part #2 received, printing now:\n");
    fprintf(Out,"CPU Exception Class: %04x Code:",(word)xcp.xclass);
    switch ((xcp.cr &0x0000007c) >> 2)
    {
      case XCPTINTR: fprintf(Out,"interrupt                  "); break;
      case XCPTMOD:  fprintf(Out,"TLB mod  /IBOUND           "); break;
      case XCPTTLBL: fprintf(Out,"TLB load /DBOUND           "); break;
      case XCPTTLBS: fprintf(Out,"TLB store                  "); break;
      case XCPTADEL: fprintf(Out,"Address error load         "); break;
      case XCPTADES: fprintf(Out,"Address error store        "); break;
      case XCPTIBE:  fprintf(Out,"Instruction load bus error "); break;
      case XCPTDBE:  fprintf(Out,"Data load/store bus error  "); break;
      case XCPTSYS:  fprintf(Out,"Syscall                    "); break;
      case XCPTBP:   fprintf(Out,"Breakpoint                 "); break;
      case XCPTCPU:  fprintf(Out,"Coprocessor unusable       "); break;
      case XCPTRI:   fprintf(Out,"Reverd instruction         "); break;
      case XCPTOVF:  fprintf(Out,"Overflow                   "); break;
      case 23:       fprintf(Out,"WATCH                      "); break;
      default:       fprintf(Out,"Unknown Code               "); break;
    }
    fprintf(Out,"\n");
    fprintf(Out,"sr  = %08lx\t",(unsigned long)xcp.sr);
    fprintf(Out,"cr  = %08lx\t",(unsigned long)xcp.cr);
    fprintf(Out,"epc = %08lx\t",(unsigned long)xcp.epc);
    fprintf(Out,"vadr= %08lx\n",(unsigned long)xcp.vaddr);
    fprintf(Out,"zero= %08lx\t",(unsigned long)xcp.regs[0]);
    fprintf(Out,"at  = %08lx\t",(unsigned long)xcp.regs[1]);
    fprintf(Out,"v0  = %08lx\t",(unsigned long)xcp.regs[2]);
    fprintf(Out,"v1  = %08lx\n",(unsigned long)xcp.regs[3]);
    fprintf(Out,"a0  = %08lx\t",(unsigned long)xcp.regs[4]);
    fprintf(Out,"a1  = %08lx\t",(unsigned long)xcp.regs[5]);
    fprintf(Out,"a2  = %08lx\t",(unsigned long)xcp.regs[6]);
    fprintf(Out,"a3  = %08lx\n",(unsigned long)xcp.regs[7]);
    fprintf(Out,"t0  = %08lx\t",(unsigned long)xcp.regs[8]);
    fprintf(Out,"t1  = %08lx\t",(unsigned long)xcp.regs[9]);
    fprintf(Out,"t2  = %08lx\t",(unsigned long)xcp.regs[10]);
    fprintf(Out,"t3  = %08lx\n",(unsigned long)xcp.regs[11]);
    fprintf(Out,"t4  = %08lx\t",(unsigned long)xcp.regs[12]);
    fprintf(Out,"t5  = %08lx\t",(unsigned long)xcp.regs[13]);
    fprintf(Out,"t6  = %08lx\t",(unsigned long)xcp.regs[14]);
    fprintf(Out,"t7  = %08lx\n",(unsigned long)xcp.regs[15]);
    fprintf(Out,"s0  = %08lx\t",(unsigned long)xcp.regs[16]);
    fprintf(Out,"s1  = %08lx\t",(unsigned long)xcp.regs[17]);
    fprintf(Out,"s2  = %08lx\t",(unsigned long)xcp.regs[18]);
    fprintf(Out,"s3  = %08lx\n",(unsigned long)xcp.regs[19]);
    fprintf(Out,"s4  = %08lx\t",(unsigned long)xcp.regs[20]);
    fprintf(Out,"s5  = %08lx\t",(unsigned long)xcp.regs[21]);
    fprintf(Out,"s6  = %08lx\t",(unsigned long)xcp.regs[22]);
    fprintf(Out,"s7  = %08lx\n",(unsigned long)xcp.regs[23]);
    fprintf(Out,"t8  = %08lx\t",(unsigned long)xcp.regs[24]);
    fprintf(Out,"t9  = %08lx\t",(unsigned long)xcp.regs[25]);
    fprintf(Out,"k0  = %08lx\t",(unsigned long)xcp.regs[26]);
    fprintf(Out,"k1  = %08lx\n",(unsigned long)xcp.regs[27]);
    fprintf(Out,"gp  = %08lx\t",(unsigned long)xcp.regs[28]);
    fprintf(Out,"sp  = %08lx\t",(unsigned long)xcp.regs[29]);
    fprintf(Out,"s8  = %08lx\t",(unsigned long)xcp.regs[30]);
    fprintf(Out,"ra  = %08lx\n",(unsigned long)xcp.regs[31]);
    fprintf(Out,"mdlo= %08lx\t",(unsigned long)xcp.mdlo);
    fprintf(Out,"mdhi= %08lx\n",(unsigned long)xcp.mdhi);
    break;
  case 128:
  case 129:
    n = buffer->info.l1.length;
    if(buffer->code==128) fprintf(Out,"CAPI20_PUT(%03d) ",n);
    else fprintf(Out,"CAPI20_GET(%03d) ",n);
    fprintf(Out,"APPL %04x ",*(word *)&buffer->info.l1.i[0]);
    fprintf(Out,"%04x:%08lx ",*(word *)&buffer->info.l1.i[4],
                               (unsigned long)*(dword *)&buffer->info.l1.i[6]);
    for(i=0;i<15 && buffer->info.l1.i[2]!=capi20_command[i].code;i++);
    if(i<15) fprintf(Out,"%s ",capi20_command[i].name);
    else fprintf(Out,"CMD(%x) ",buffer->info.l1.i[2]);
    for(i=0;i<4 && buffer->info.l1.i[3]!=capi20_subcommand[i].code;i++);
    if(i<4) fprintf(Out,"%s",capi20_subcommand[i].name);
    else fprintf(Out,"SUB(%x)",buffer->info.l1.i[3]);
    fprintf(Out,"\n                                              ");
    for(i=10;i<n-2;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    break;
  case 130:
  case 131:
    n = buffer->info.l1.length;
    if(buffer->code==130) fprintf(Out,"CAPI11_PUT(%03d) ",n);
    else fprintf(Out,"CAPI11_GET(%03d) ",n);
    fprintf(Out,"APPL %04x ",*(word *)&buffer->info.l1.i[0]);
    fprintf(Out,"%04x:",*(word *)&buffer->info.l1.i[4]);
    for(i=0;i<18 && buffer->info.l1.i[2]!=capi11_command[i].code;i++);
    if(i<18) fprintf(Out,"%s ",capi11_command[i].name);
    else fprintf(Out,"CMD(%x) ",buffer->info.l1.i[2]);
    for(i=0;i<4 && buffer->info.l1.i[3]!=capi11_subcommand[i].code;i++);
    if(i<4) fprintf(Out,"%s",capi11_subcommand[i].name);
    else fprintf(Out,"SUB(%x)",buffer->info.l1.i[3]);
    fprintf(Out,"\n                                              ");
    for(i=6;i<n-2;i++)
      fprintf(Out," %02X",buffer->info.l1.i[i]);
    break;
  case 201: /* Log Return Code */
    Id   = diva_word2le(buffer->info.nu_info.Id);
    nl_sec = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
    code = (byte)diva_word2le(buffer->info.nu_info.code);
    sprintf (tmp, "[%02x] N-RC=", nl_sec);
    p = &tmp[strlen(tmp)];
    rc2str ((byte)code, p);
    p = &tmp[strlen(tmp)];
    if (((byte)Id) == NL_ID + GL_ERR_ID) {
      sprintf (p, " Id:%s, Ch:%02x",
               "NL_ID+GL_ERR_ID", HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    } else {
      sprintf (p, " Id:%02x, Ch:%02x",
               (byte)Id, HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    }
    fprintf(Out,"%s", tmp);
    break;
  case 202:
    Id   = diva_word2le(buffer->info.nu_info.Id);
    code = diva_word2le(buffer->info.nu_info.code);
    nl_sec = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
    fprintf(Out,"[%02x] N-RNR Id:%02x, Ch:%02x",
            nl_sec, (byte)Id, HIBYTE(code));
    break;
  case 220: /* log XDI request */
    code = diva_word2le(buffer->info.nu_info.info) - 1;
    sprintf (tmp, "XDI[%02x] ",
      HIBYTE(diva_word2le(buffer->info.nu_info.code)));
    p = &tmp[strlen(tmp)];
    switch (HIBYTE(diva_word2le(buffer->info.nu_info.info))) {
      case NL_ID: /* NL REQ */
        if (((byte)(code+1) == ASSIGN) &&
            (((byte)diva_word2le(buffer->info.nu_info.Id)) == NL_ID)) {
          strcpy (p,"N-ASSIGN");
        } else if ((byte)(code+1) == REMOVE) {
          strcpy (p, "N-REMOVE");
        } else if ((byte)(code+1) == N_XON) {
          strcpy (p, "N-XON");
        } else if ((byte)(code+1) == UREMOVE) {
          strcpy (p, "N-UREMOVE");
        } else if((code & 0x0f)<=12) {
          strcpy (p, ns_name[code &0x0f]);
        } else {
          sprintf (p, "N-UNKNOWN NS(%02x)",
                   (byte)diva_word2le(buffer->info.nu_info.info));
        }
        break;
      case DSIG_ID: /* SIG REQ */
        Id = (byte)diva_word2le(buffer->info.nu_info.Id);
        if (Id == DSIG_ID) {
          strcpy (p, "S-ASSIGN");
        } else {
          encode_sig_req ((byte)diva_word2le(buffer->info.nu_info.info),
                          p, 1);
        }
        break;
      case MAN_ID:
        encode_man_req ((byte)diva_word2le(buffer->info.nu_info.info), p, 1);
        break;
      default:
        sprintf (p, "?-(%02x)",
                 (byte)diva_word2le(buffer->info.nu_info.info));
        break;
    }
    strcat (tmp, " REQ Id:");
    p = &tmp[strlen(tmp)];
    Id = (byte)diva_word2le(buffer->info.nu_info.Id);
    switch (Id) {
      case DSIG_ID:
        strcpy (p, "DSIG_ID");
        break;
      case NL_ID:
        strcpy (p, "NL_ID");
        break;
      case BLLC_ID:
        strcpy (p, "BLLC_ID");
        break;
      case TASK_ID:
        strcpy (p, "TASK_ID");
        break;
      case TIMER_ID:
        strcpy (p, "TIMER_ID");
        break;
      case TEL_ID:
        strcpy (p, "TEL_ID");
        break;
      case MAN_ID:
        strcpy (p, "MAN_ID");
        break;
      default:
        sprintf (p, "%02x", Id);
        break;
    }
    p = &tmp[strlen(tmp)];
    sprintf (p, ", Ch:%02x, A(%02x)",
             HIBYTE(diva_word2le(buffer->info.nu_info.Id)),
             (byte)diva_word2le(buffer->info.nu_info.code));
    fprintf(Out,"%s", tmp);
    break;
  case 221:
    { /* XDI RC code log */
      byte A  = LOBYTE(diva_word2le(buffer->info.nu_info.code));
      byte s  = HIBYTE(diva_word2le(buffer->info.nu_info.code));
      byte id = LOBYTE(diva_word2le(buffer->info.nu_info.Id));
      byte ch = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
      byte rc = LOBYTE(diva_word2le(buffer->info.nu_info.info));
      byte t  = HIBYTE(diva_word2le(buffer->info.nu_info.info));
      byte cb = LOBYTE(diva_word2le(buffer->info.nu_info.info1));
      Id = id;
      sprintf (tmp, "XDI[%02x] ", s);
      p = &tmp[strlen(tmp)];
      switch (t) {
        case NL_ID: /* NL REQ */
          strcpy (p, "N-");
          break;
        case DSIG_ID: /* SIG REQ */
          strcpy (p, "S-");
          break;
        case MAN_ID:
          strcpy (p, "MAN-");
          break;
      }
      strcat (p, "RC=");
      p = &tmp[strlen(tmp)];
      if (rc == READY_INT) {
        strcat (p, "READY_INT");
      } else {
        rc2str (rc, p);
      }
      p = &tmp[strlen(tmp)];
      if (id == NL_ID + GL_ERR_ID) {
        sprintf (p, " Id:%s, Ch:%02x", "NL_ID+GL_ERR_ID", ch);
      } else if (id == DSIG_ID + GL_ERR_ID) {
        sprintf (p, " Id:%s, Ch:%02x", "DSIG_ID+GL_ERR_ID", ch);
      } else if (id == MAN_ID + GL_ERR_ID) {
        sprintf (p, " Id:%s, Ch:%02x", "MAN_ID+GL_ERR_ID", ch);
      } else {
        sprintf (p, " Id:%02x, Ch:%02x", id, ch);
      }
      switch (cb) {
        case 0: /* delivery */
          strcat (tmp, " DELIVERY");
          break;
        case 1: /* callback */
          strcat (tmp, " CALLBACK");
          break;
        case 2: /* assign */
          strcat (tmp, " ASSIGN");
          break;
      }
      p = &tmp[strlen(tmp)];
      sprintf (p, " A(%02x)", A);
      fprintf(Out,"%s", tmp);
    } break;
  case 222:
    {/* XDI xlog IND */
      byte A   = LOBYTE(diva_word2le(buffer->info.nu_info.code));
      byte s   = HIBYTE(diva_word2le(buffer->info.nu_info.code));
      byte id  = LOBYTE(diva_word2le(buffer->info.nu_info.Id));
      byte ch  = HIBYTE(diva_word2le(buffer->info.nu_info.Id));
      byte ind = LOBYTE(diva_word2le(buffer->info.nu_info.info));
      byte t   = HIBYTE(diva_word2le(buffer->info.nu_info.info));
      byte rnr = LOBYTE(diva_word2le(buffer->info.nu_info.info1));
      byte val = HIBYTE(diva_word2le(buffer->info.nu_info.info1));
      Id = id;
      sprintf (tmp, "XDI[%02x] ", s);
      p = &tmp[strlen(tmp)];
      switch (t) {
        case NL_ID: /* NL REQ */
          if (ind == N_COMBI_IND) {
            strcpy (p, "N-CDATA");
          } else if (((ind-1) & 0x0f)<=12) {
            strcpy (p, ns_name[(ind-1) &0x0f]);
          } else {
            sprintf (p, "N-UNKNOWN NS(%02x)", ind);
          }
          break;
        case DSIG_ID: /* SIG REQ */
          encode_sig_req (ind, p, 0);
          break;
        case MAN_ID:
          encode_man_req (ind, p, 0);
          break;
        default:
          sprintf (p, "?-(%02x)", ind);
          break;
      }
      strcat (tmp, " IND Id:");
      p = &tmp[strlen(tmp)];
      switch (id) {
        case DSIG_ID:
          strcpy (p, "DSIG_ID");
          break;
        case NL_ID:
          strcpy (p, "NL_ID");
          break;
        case BLLC_ID:
          strcpy (p, "BLLC_ID");
          break;
        case TASK_ID:
          strcpy (p, "TASK_ID");
          break;
        case TIMER_ID:
          strcpy (p, "TIMER_ID");
          break;
        case TEL_ID:
          strcpy (p, "TEL_ID");
          break;
        case MAN_ID:
          strcpy (p, "MAN_ID");
          break;
        default:
          sprintf (p, "%02x", id);
          break;
      }
      p = &tmp[strlen(tmp)];
      sprintf (p, ", Ch:%02x ", ch);
      p = &tmp[strlen(tmp)];
      switch (val) {
        case 0:
          strcpy (p, "DELIVERY");
          break;
        case 1:
          sprintf (p, "RNR=%d", rnr);
          break;
        case 2:
          strcpy (p, "RNum=0");
          break;
        case 3:
          strcpy (p, "COMPLETE");
          break;
      }
      p = &tmp[strlen(tmp)];
      sprintf (p, " A(%02x)", A);
      fprintf(Out,"%s", tmp);
    } break;
  }
  if((byte)buffer->code==21 || (byte)buffer->code==22){
    message.code = new_msg_code;
    message.length = new_msg_length;
  }
  fprintf(Out,"\n");
  return o;
}
static void xlog_dump_buffer (FILE *stream, byte *buf, word len, char *tmp)
{
  word i, lim;
  char *p;
  if (len != 0)
  {
    i = 0;
    if (maxout > 30)
    {
      lim = 0;
      do
      {
        fprintf(Out,"\n  (%04d) - ",i);
        lim = (len - lim > 20) ? lim + 20 : len;
        if (lim > maxout)
          lim = maxout;
        p = tmp;
        do
        {
          *(p++) = ' ';
          *(p++) = hex_digit_table[buf[i] >> 4];
          *(p++) = hex_digit_table[buf[i] & 0xf];
          i++;
        } while (i < lim);
        *p = '\0';
        fprintf(Out,"%s",tmp);
      } while ((i < len) && (i < maxout));
    }
    else
    {
      p = tmp;
      do
      {
        *(p++) = ' ';
        *(p++) = hex_digit_table[buf[i] >> 4];
        *(p++) = hex_digit_table[buf[i] & 0xf];
        i++;
      } while ((i < len) && (i < maxout));
      *p = '\0';
      fprintf(Out,"%s",tmp);
    }
    if (i < len)
      fprintf(Out," cont");
  }
}
void xlog_sig(FILE * stream, struct msg_s * message)
{
  word n;
  word i;
  byte msg;
  n = message->length;
  msg = TRUE;
  *(word *)&message->info[0] = diva_word2le(*(word *)&message->info[0]);
  switch(*(word *)&message->info[0]) {
  case 0x0000:
    fprintf(Out,"SIG-x(%03i)",n-4);
    break;
  case 0x0010:
  case 0x0013:
    fprintf(Out,"SIG-R(%03i)",n-4);
    break;
  case 0x0020:
    fprintf(Out,"SIG-X(%03i)",n-4);
    break;
  default:
    fprintf(Out,"SIG-EVENT %04X",*(word *)&message->info[0]);
    msg = FALSE;
    break;
  }
  if(n>4)
  {
   for(i=0;i<n-4; i++)
  fprintf(Out," %02X",message->info[4+i]);
   fprintf(Out,"\n");
  }
  if(msg) display_q931_message(stream, message);
}
void call_failed_event(FILE * stream, struct msg_s * message, word code)
{
  fprintf(Out, "EVENT: Call failed in State '%s'\n", sig_state[code>>8]);
  *(word *)&message->info[0] = diva_word2le(*(word *)&message->info[0]);
  switch(*(word *)&message->info[0]) {
  case 0x0000:
  case 0x0010:
  case 0x0020:
    display_q931_message(stream, message);
    break;
  case 0xff00:
    fprintf(Out,"                     ");
    fprintf(Out,"Signaling Timeout %s",sig_timeout[code>>8]);
    break;
  case 0xffff:
    fprintf(Out,"                     ");
    fprintf(Out,"Link disconnected");
    switch(message->info[4]) {
    case 8:
      fprintf(Out,", Layer-1 error (cable or NT)");
      break;
    case 9:
      fprintf(Out,", Layer-2 error");
      break;
    case 10:
      fprintf(Out,", TEI error");
      break;
    }
    break;
  }
}
void display_q931_message(FILE * stream, struct msg_s * message)
{
  word i;
  word cr;
  word p;
  word w;
  word wlen;
  byte codeset,lock;
  fprintf(Out,"                     ");
  cr = 0;
        /* read one byte call reference */
  if(message->info[5]==1) {
    cr = message->info[6] &0x7f;
  }
        /* read two byte call reference */
  if(message->info[5]==2) {
    cr = message->info[7];
    cr += (message->info[6] &0x7f)<<8;
  }
        /* read call reference flag */
  if(message->info[5]) {
    if(message->info[6] &0x80) cr |=0x8000;
  }
  for(i=0;i<PD_MAX && message->info[4]!=pd[i].code;i++);
  /* one Byte CR */
  if (message->info[5]==1)
  {
    if(i<PD_MAX) fprintf(Out,"%sCR%02x ",pd[i].name,message->info[6]);
    else fprintf(Out,"PD(%02xCR%02x)  ",message->info[4],message->info[6]);
  }
  else
  {
    if(i<PD_MAX) fprintf(Out,"%sCR%04x ",pd[i].name,cr);
    else fprintf(Out,"PD(%02xCR%04x)  ",message->info[4],cr);
  }
  p = 6+message->info[5];
  for(i=0;i<MT_MAX && message->info[p]!=mt[i].code;i++);
  if(i<MT_MAX) fprintf(Out,"%s",mt[i].name);
  else fprintf(Out,"MT%02x",message->info[p]);
  p++;
  codeset = 0;
  lock = 0;
  while(p<message->length) {
    fprintf(Out,"\n                            ");
      /* read information element id and length                   */
    w = message->info[p];
    if(w & 0x80) {
      w &=0xf1;
      wlen = 1;
    }
    else {
      wlen = message->info[p+1]+2;
    }
    if(lock & 0x80) lock &=0x7f;
    else codeset = lock;
    if((w&0xf0)==0x90) {
      codeset = message->info[p];
      if(codeset &8) fprintf(Out,"SHIFT %02x",codeset &7);
      else fprintf(Out,"SHIFT LOCK %02x",codeset &7);
      if(!(codeset & 0x08)) lock = (byte)(codeset & 7);
      codeset &=7;
      lock |=0x80;
    }
    else {
      w |= (codeset<<8);
      for(i=0;i<IE_MAX && w!=ie[i].code;i++);
      if(i<IE_MAX) fprintf(Out,"%s",ie[i].name);
      else fprintf(Out,"%01x/%02x ",codeset,(byte)w);
      if((p+wlen) > message->length) {
        fprintf(Out,"IE length error");
      }
      else {
        if(wlen>1) display_ie(stream,message->info[4],w,&message->info[1+p]);
      }
    }
      /* check if length valid (not exceeding end of packet)      */
    p=p+(word)wlen;
  }
}
void display_ie(FILE * stream, byte pd, word w, byte * ie)
{
  word i;
  switch(w) {
  case 0x08:
    for(i=0;i<ie[0];i++) fprintf(Out," %02x",ie[1+i]);
    switch(pd) {
    case 0x08:
      for(i=0; i<ie[0] && !(ie[1+i]&0x80); i++);
      fprintf(Out, " '%s'",cau_q931[ie[2+i]&0x7f]);
      break;
    case 0x41:
      if(ie[0]) fprintf(Out, " '%s'",cau_1tr6[ie[1]&0x7f]);
      else fprintf(Out, " '%s'",cau_1tr6[0]);
      break;
    }
    break;
  case 0x0c:
  case 0x6c:
  case 0x6d:
  case 0x70:
  case 0x71:
    for(i=0; i<ie[0] && !(ie[1+i]&0x80); i++)
      fprintf(Out, " %02x",ie[1+i]);
    fprintf(Out, " %02x",ie[1+i++]);
    fprintf(Out," '");
    for( ; i<ie[0]; i++) fprintf(Out, "%c",ie[1+i]);
    fprintf(Out,"'");
    break;
  case 0x2c:
  case 0x603:
    fprintf(Out," '");
    for(i=0 ; i<ie[0]; i++) fprintf(Out, "%c",ie[1+i]);
    fprintf(Out,"'");
    break;
  case 0x1c: /* Facility */
    for(i=0;i<ie[0];i++) fprintf(Out," %02x",ie[1+i]);
    break;
  default:
    for(i=0;i<ie[0];i++) fprintf(Out," %02x",ie[1+i]);
    break;
  }
}
void spid_event(FILE * stream, struct msg_s * message, word code)
{
  word i;
  switch(code>>8) {
  case 1:
    fprintf(Out, "EVENT: SPID rejected");
    break;
  case 2:
    fprintf(Out, "EVENT: SPID accepted");
    break;
  case 3:
    fprintf(Out, "EVENT: Timeout, resend SPID");
    break;
  case 4:
    fprintf(Out, "EVENT: Layer-2 failed, resend SPID");
    break;
  case 5:
    fprintf(Out, "EVENT: Call attempt on inactive SPID");
    break;
  }
  fprintf(Out, " '");
  for(i=0;i<message->info[0];i++) fprintf(Out,"%c",message->info[1+i]);
  fprintf(Out, "'");
}
static void rc2str (byte rc, char* p) {
  if ((rc & 0xf0) == 0xf0) {
    switch (rc) {
      case OK_FC:
        strcpy (p, "OK_FC");
        break;
      case READY_INT:
        strcpy (p, "READY_INT");
        break;
      case TIMER_INT:
        strcpy (p, "TIMER_INT");
        break;
      case OK:
        strcpy (p, "OK");
        break;
      default:
        sprintf (p, "%02x", rc);
        break;
    }
    return;
  }
  if (rc & 0xe0) {
    switch (rc) {
      case ASSIGN_RC:
        strcpy (p, "ASSIGN_RC");
        break;
      case ASSIGN_OK:
        strcpy (p, "ASSIGN_OK");
        return;
      default:
        sprintf (p, "%02x", rc);
        return;
    }
  }
  strcat (p, " | ");
  p = p+strlen(p);
  switch (rc & 0x1f) {
    case UNKNOWN_COMMAND:
      strcpy (p, "UNKNOWN_COMMAND");
      break;
    case WRONG_COMMAND:
      strcpy (p, "WRONG_COMMAND");
      break;
    case WRONG_ID:
      strcpy (p, "WRONG_ID");
      break;
    case WRONG_CH:
      strcpy (p, "WRONG_CH");
      break;
    case UNKNOWN_IE:
      strcpy (p, "UNKNOWN_IE");
      break;
    case WRONG_IE:
      strcpy (p, "WRONG_IE");
      break;
    case OUT_OF_RESOURCES:
      strcpy (p, "OUT_OF_RESOURCES");
      break;
    case N_FLOW_CONTROL:
      strcpy (p, "N_FLOW_CONTROL");
      break;
    case ASSIGN_RC:
      strcpy (p, "ASSIGN_RC");
      break;
    case ASSIGN_OK:
      strcpy (p, "ASSIGN_OK");
      break;
    default:
      sprintf (p, "%02x", rc);
      break;
  }
}
static void encode_sig_req (byte code, char* p, int is_req) {
  strcpy (p, "S-");
  p = p+strlen(p);
  switch (code) {
    case CALL_REQ:      /* 1 call request                             */
      /* CALL_CON          1 call confirmation                        */
      if (is_req) {
        strcpy(p, "CALL_REQ");
      } else {
        strcpy(p, "CALL_CON");
      }
      break;
    case LISTEN_REQ:    /* 2  listen request                           */
      /* CALL_IND          2  incoming call connected                  */
      if (is_req) {
        strcpy (p, "LISTEN_REQ");
      } else {
        strcpy (p, "CALL_IND");
      }
      break;
    case HANGUP:        /* 3  hangup request/indication                */
      strcpy(p, "HANGUP");
      break;
    case SUSPEND:       /* 4  call suspend request/confirm             */
      strcpy (p, "SUSPEND");
      break;
    case RESUME:        /* 5  call resume request/confirm              */
      strcpy (p, "RESUME");
      break;
    case SUSPEND_REJ:   /* 6  suspend rejected indication              */
      strcpy (p, "SUSPEND_REJ");
      break;
    case USER_DATA:     /* 8  user data for user to user signaling     */
      strcpy (p, "USER_DATA");
      break;
    case CONGESTION:    /* 9  network congestion indication            */
      strcpy (p, "CONGESTION");
      break;
    case INDICATE_REQ:  /* 10 request to indicate an incoming call     */
      /* INDICATE_IND      10 indicates that there is an incoming call */
      if (is_req) {
        strcpy (p, "INDICATE_REQ");
      } else {
        strcpy (p, "INDICATE_IND");
      }
      break;
    case CALL_RES:      /* 11 accept an incoming call                  */
      strcpy (p, "CALL_RES");
      break;
    case CALL_ALERT:    /* 12 send ALERT for incoming call             */
      strcpy (p, "CALL_ALERT");
      break;
    case INFO_REQ:      /* 13 INFO request                             */
      /* INFO_IND          13 INFO indication                          */
      if (is_req) {
        strcpy (p, "INFO_REQ");
      } else {
        strcpy (p, "INFO_IND");
      }
      break;
    case REJECT:        /* 14 reject an incoming call                  */
      strcpy (p, "REJECT");
      break;
    case RESOURCES:     /* 15 reserve B-Channel hardware resources     */
      strcpy (p, "RESOURCES");
      break;
    case TEL_CTRL:      /* 16 Telephone control request/indication     */
      strcpy (p, "TEL_CTRL");
      break;
    case STATUS_REQ:    /* 17 Request D-State (returned in INFO_IND)   */
      strcpy (p, "STATUS_REQ");
      break;
    case FAC_REG_REQ:   /* 18 connection idependent fac registration   */
      strcpy (p, "FAC_REG_REQ");
      break;
    case FAC_REG_ACK:   /* 19 fac registration acknowledge             */
      strcpy (p, "FAC_REG_ACK");
      break;
    case FAC_REG_REJ:   /* 20 fac registration reject                  */
      strcpy (p, "FAC_REG_REJ");
      break;
    case CALL_COMPLETE: /* 21 send a CALL_PROC for incoming call       */
      strcpy (p, "CALL_COMPLETE");
      break;
    case FACILITY_REQ:  /* 25 Q.931 connection independent fac req     */
      strcpy (p, "FACILITY_REQ");
      break;
    case FACILITY_IND:  /* 26 Q.931 connection independent fac ind     */
      strcpy (p, "FACILITY_IND");
      break;
    case SIG_CTRL:      /* 29 Control for signalling hardware          */
      strcpy (p, "SIG_CTRL");
      break;
    case DSP_CTRL:      /* 30 Control for DSPs                         */
      strcpy (p, "DSP_CTRL");
      break;
    case LAW_REQ:       /* 31 Law config request for (returns info_i)  */
      strcpy (p, "LAW_REQ");
      break;
    case S_SERVICE:     /* 41 Send/Receive Supplementary Service       */
      strcpy (p, "S_SERVICE");
      break;
    case S_SERVICE_REJ: /* 42 Reject Supplementary Service indication  */
      strcpy (p, "S_SERVICE_REJ");
      break;
    case S_SUPPORTED:   /* 43 Req/Ind to get Supported Services        */
      strcpy (p, "S_SUPPORTED");
      break;
    case STATUS_ENQ:    /* 44 Req to send the D-ch request if !state0  */
      strcpy (p, "STATUS_ENQ");
      break;
    case SSEXT_REQ:     /* 48 Supplem.Serv./QSIG specific request      */
      strcpy (p, "SSEXT_REQ");
      break;
    case SSEXT_IND:     /* 49 Supplem.Serv./QSIG specific indication   */
      strcpy (p, "SSEXT_IND");
      break;
    case VSWITCH_REQ:   /* 65 communication between protocol and       */
      strcpy (p, "VSWITCH_REQ");
      break;
    case VSWITCH_IND:   /* 66 capifunctions for D-CH-switching         */
      strcpy (p, "VSWITCH_IND");
      break;
    case MWI_POLL:      /* 67 Message Waiting Status Request fkt       */
      strcpy (p, "MWI_POLL");
      break;
    case CALL_PEND_NOTIFY:/* 68 notify capi to set new listen          */
      strcpy (p, "CALL_PEND_NOTIFY");
      break;
    case UREMOVE:
      strcpy (p, "UREMOVE");
      break;
    case REMOVE:
      strcpy (p, "REMOVE");
      break;
    default:
      sprintf (p, "UNKNOWN(%02x)", code);
      break;
  }
}
static void encode_man_req (byte code, char* p, int is_req) {
  strcpy (p, "M-");
  p = p+strlen(p);
  if (is_req)
  {
    switch (code) {
      case ASSIGN:      /* 1 */
        strcpy (p, "ASSIGN");
        break;
      case MAN_READ:    /* 2 */
        strcpy (p, "MAN_READ");
        break;
      case MAN_WRITE:   /* 3 */
        strcpy (p, "MAN_WRITE");
        break;
      case MAN_EXECUTE: /* 4 */
        strcpy (p, "MAN_EXECUTE");
        break;
      case MAN_EVENT_ON:/* 5 */
        strcpy (p, "MAN_EVENT_ON");
        break;
      case MAN_EVENT_OFF:/*6 */
        strcpy (p, "MAN_EVENT_OFF");
        break;
      case MAN_LOCK:    /* 7 */
        strcpy (p, "MAN_LOCK");
        break;
      case MAN_UNLOCK:  /* 8 */
        strcpy (p, "MAN_UNLOCK");
        break;
      case REMOVE:
        strcpy (p, "REMOVE");
        break;
      case UREMOVE:
        strcpy (p, "UREMOVE");
        break;
      default:
        sprintf (p, "UNKNOWN(%02x)", code);
        break;
    }
  }
  else
  {
    switch (code) {
      case N_MDATA: /* 1 */
        strcpy (p, "MAN_MDATA");
        break;
      case MAN_INFO_IND:  /* 2 */
        strcpy (p, "MAN_INFO_IND");
        break;
      case MAN_EVENT_IND: /* 3 */
        strcpy (p, "MAN_EVENT_IND");
        break;
      case MAN_TRACE_IND: /* 4 */
        strcpy (p, "MAN_TRACE_IND");
        break;
      case MAN_COMBI_IND: /* 9 */
        strcpy (p, "MAN_COMBI_IND");
        break;
  
      default:
        sprintf (p, "UNKNOWN(%02x)", code);
        break;
    }
  }
}
#define MAKEWORD(a, b)      ((word)(((byte)(a)) | ((word)((byte)(b))) << 8))
#define LOBYTE(w)           ((byte)(w))
#define HIBYTE(w)           ((byte)(((word)(w) >> 8) & 0xFF))
word diva_word2le (word w) {
  if (diva_convert_be2le) {
    return (MAKEWORD(HIBYTE(w), LOBYTE(w)));
  }
  return (w);
}
