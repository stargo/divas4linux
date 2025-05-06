
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

#define XIDFIS_GP 0x82        /* ISO standardized 'general purpose' */
/* Data Link Layer Subfield */
#define XIDGI_PN  0x80        /* parameter negotiation group ID */
#define XIDGL_PN  (word)0x13  /* parameter negotiation group length */
#define XIDGI_PPN 0xF0        /* Private para. negotiation group ID */
#define XIDGL_PPN_V42  (word)0x000F  /* Private para. negotiation group length, V.42 */
#define XIDGL_PPN_V120 (word)0x0010  /* Private para. negotiation group length, V.V.120 */
#define XIDGL_PPN_X75  (word)0x0016  /* Private para. negotiation group length, X.75 */
/* parameter negotiation parameter identifier, length + values */
#define XIDPI_HOF  0x03        /* HDLC optional Parameters, Attention: the max length is 10 as seen in L2.h */
#define XIDPL_HOF  0x03        /* parameter length HDLC optional Parameters */
#define XIDPV_HOF0 0x8A        /* para. value, Bit 1-8: Bit 3 - SREJ-procedure req. */ 
#define XIDPV_HOF1 0x89        /* (see:       Bit 9-16: Bit 14 - TEST-procedure req. */ 
#define XIDPV_HOF2 0x00        /* table 11)  Bit 17-24: Bit 17 - Ext.-FCS-Procedure req. */ 
#define XIDPI_MLT  0x05        /* max. length of I-field (N401): TX-Dir. (unit: bits) */
#define XIDPL_MLT  0x02        /* length */
#define XIDPV_LOL  (word)0x0400 /* fixed length of frame which is the local station able to TX */
#define XIDPV_MLT  (word)0x0400 /* maximum possible length */
#define XIDPI_MLR  0x06        /* max. length of I-field (N401): RX-Dir. (unit: bits) */
#define XIDPL_MLR  0x02        /*  */
#define XIDPV_MLR  (word)0x0400 /*  */
#define XIDPI_KTX  0x07        /* window size (k): TX-Dir. (unit: frames) */
#define XIDPL_KTX  0x01        /*  */
#define XIDPV_KTX  0x0F        /*  */
#define XIDPI_KRX  0x08        /* window size (k): RX-Dir. (unit: frames) */
#define XIDPL_KRX  0x01        /*  */
#define XIDPV_KRX  0x0F        /*  */
/* private parameter negotiation parameter identifier */
#define XIDPI_PSI  0x00        /* Parameter set identification */
#define XIDPL_PSI_V42   0x03  /* parameter length Parameter set identification, Attention: the max length is 10 as seen in L2.h */
#define XIDPL_PSI_V120  0x04  /* parameter length Parameter set identification, Attention: the max length is 10 as seen in L2.h */
#define XIDPL_PSI_X75   0x0A  /* parameter length Parameter set identification, Attention: the max length is 10 as seen in L2.h */
#define XIDPV_PSI0_V42  'V'         /* para. value, Byte 1: 'V' (for 'V.42') (in the standard is 'J'?????, but this one works and is used by the other implementations) */
#define XIDPV_PSI1_V42  '4'         /*              Byte 2: '4' */
#define XIDPV_PSI2_V42  '2'         /*              Byte 3: '2' */
#define XIDPV_PSI0_V120 'V'         /* para. value, Byte 1: 'V' (for 'V.120') */
#define XIDPV_PSI1_V120 '1'         /*              Byte 2: '1' */
#define XIDPV_PSI2_V120 '2'         /*              Byte 3: '2' */
#define XIDPV_PSI3_V120 '0'         /*              Byte 4: '0' */
#define XIDPV_PSI0_X75  'C'         /* para. value, Byte 1: 'C' (for 'CAPI20/X75') */
#define XIDPV_PSI1_X75  'A'         /*              Byte 2: 'A' */
#define XIDPV_PSI2_X75  'P'         /*              Byte 3: 'P' */
#define XIDPV_PSI3_X75  'I'         /* para. value, Byte 1: 'I' */
#define XIDPV_PSI4_X75  '2'         /*              Byte 2: '2' */
#define XIDPV_PSI5_X75  '0'         /*              Byte 3: '0' */
#define XIDPV_PSI6_X75  '/'         /* para. value, Byte 1: '/' */
#define XIDPV_PSI7_X75  'X'         /*              Byte 2: 'X' */
#define XIDPV_PSI8_X75  '7'         /*              Byte 3: '7' */
#define XIDPV_PSI9_X75  '5'         /* para. value, Byte 1: '5' */
#define XIDPI_P0   0x01        /* V.42bis: data compression request (P0) */
#define XIDPL_P0   0x01        /* para. length */
#define XIDPV_P0   0x03        /* para. value, compression in both direction */
#define V42BIS_IR_DIR 0x01     /* mask for compression in direction from initiator (originator) to responder (answerer) */
#define V42BIS_RI_DIR 0x02     /* mask for compression in direction from responder (answerer)   to initiator (originator) */
#define XIDPI_P1   0x02        /* V.42bis: number of codewords (P1) */
#define XIDPL_P1   0x02        /* para. length */
#define XIDPV_P1   V42BIS_N2   /* para. value, no codewords */
#define XIDPI_P2   0x03        /* V.42bis: maximum string length (P2) */
#define XIDPL_P2   0x01        /* para. length */
#define XIDPV_P2   V42BIS_N7   /* para. value */
/* User Data Subfield */
#define XIDGI_UD   0xFF        /* group identifier for user data subfield */
                               /* -------> contains no group length; */
                               /* -------> the info is bounded by the FCS */                               

/* MACRO DEFINITIONs */
#define GET_STREAM_WORD(inpStream) ((inpStream[0]<<8) | inpStream[1])/* read word from input stream */

