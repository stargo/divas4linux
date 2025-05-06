
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
  Copyright (c) Dialogic(R), 2004-2017
  Copyright 2000-2003 by Armin Schindler (mac@melware.de)
  Copyright 2000-2003 Cytronics & Melware (info@melware.de)

 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of Adapters.
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
