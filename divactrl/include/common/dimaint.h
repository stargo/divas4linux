
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

#ifndef _DIMAINT_INCLUDE_  
#define _DIMAINT_INCLUDE_
#include "platform.h"
/*----- XLOG Trace event codes -----*/
#define TRC_B_X           1
#define TRC_B_R           2
#define TRC_D_X           3
#define TRC_D_R           4
#define TRC_SIG_EVENT_COM 5
#define TRC_LL_IND        6
#define TRC_LL_REQ        7
#define TRC_DEBUG         8
#define TRC_MDL_ERROR     9
#define TRC_UTASK_PC      10
#define TRC_PC_UTASK      11
#define TRC_X_X           12
#define TRC_X_R           13
#define TRC_N_IND         14
#define TRC_N_REQ         15
#define TRC_TASK          16
#define TRC_IO_REQ        18
#define TRC_IO_CON        19
#define TRC_L1_STATE      20
#define TRC_SIG_EVENT_DEF 21
#define TRC_CAUSE         22
#define TRC_EYE           23
#define TRC_STRING        24
#define TRC_AUDIO         25
#define TRC_REG_DUMP      50
#define TRC_CAPI20_A      128
#define TRC_CAPI20_B      129
#define TRC_CAPI11_A      130
#define TRC_CAPI11_B      131
#define TRC_N_RC          201
#define TRC_N_RNR         202
#define TRC_XDI_REQ       220
#define TRC_XDI_RC        221
#define TRC_XDI_IND       222
/*----- Layer 1 state XLOG codes -----*/
#define  L1XLOG_SYNC_LOST       1
#define  L1XLOG_SYNC_GAINED     2
#define  L1XLOG_DOWN            3
#define  L1XLOG_UP              4
#define  L1XLOG_ACTIVATION_REQ  5
#define  L1XLOG_PS_DOWN         6
#define  L1XLOG_PS_UP           7
#define  L1XLOG_FC1             8
#define  L1XLOG_FC2             9
#define  L1XLOG_FC3             10
#define  L1XLOG_FC4             11
struct l1s {
  short length;
  unsigned char i[22];
};
struct l2s {
  short code;
  short length;
  unsigned char i[20];
};
union par {
  char text[42];
  struct l1s l1;
  struct l2s l2;
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
  byte buffer[255];
};
typedef void (      * DI_PRINTF)(byte   *, ...);
#endif  
