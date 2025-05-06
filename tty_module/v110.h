
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

#ifndef __DIVA_SOFT_V110_H__
#define __DIVA_SOFT_V110_H__

typedef struct FRAMER_STATE_T {
  word   bit_ctr;
  word   parity_ctr;
  byte   state;
  byte   bit_stuffing_ctr;
} FRAMER_STATE;


typedef struct _soft_v110_state {
	void* C;
	void* P;
	int cfg;
	byte v110_rate;
	byte v110_irate;
  byte v110_cfg_bits;
	byte v110_cfg_stop;
	byte v110_cfg_parity;
	byte v110_cfg_mode;           /* sync. or async. mode        */
	byte v110_phase;              /* indicate current phase      */
	dword detect_delay;           /* delay counter               */
	byte rx_deframe_state;        /* current deframe state       */
  byte v110_rx_frame[10];       /* buffer of current rx frame  */
	byte v110_rx_frame_pos;       /* current write position      */
	dword rx_deframe_len;         /* bits in deframe window      */
	dword rx_deframe_wnd;         /* deframe window              */
	byte frame_sync_ctr;          /* number of correct frames    */

  FRAMER_STATE      rx_framer_state;         /* receive framer state        */
	dword             rx_wnd_len;              /* detection window length     */
	dword             rx_frm_wnd;              /* detection window            */

	dword							detector_counter;
} soft_v110_state;

/* V.110 transfer rates */
#define V110_RATE_600          1
#define V110_RATE_1200         2
#define V110_RATE_2400         3
#define V110_RATE_7200         4
#define V110_RATE_14400        5
#define V110_RATE_28800        6
#define V110_RATE_4800         7
#define V110_RATE_9600         8
#define V110_RATE_19200        9
#define V110_RATE_38400        10
#define V110_RATE_12000        11
#define V110_RATE_24000        12
#define V110_RATE_48000        13
#define V110_RATE_56000        14
#define V110_RATE_1200_75      15
#define V110_RATE_75_1200      16

/* RA2 intermediate rates */
#define V110_IRATE_8K          1
#define V110_IRATE_16K         2
#define V110_IRATE_32K         4
#define V110_IRATE_64K         8



typedef enum {
	divaV110_8_DataBits = 8,
	divaV110_7_DataBits = 7,
	divaV110_6_DataBits = 6,
	divaV110_5_DataBits = 5
} divaV110DataBits;

typedef enum {
	divaV110_1_StopBit  = 1,
	divaV110_2_StopBits = 2,
} divaV110StopBits;

typedef enum {
	divaV110ParityNone = 0,
	divaV110ParityOdd  = 1 | 0x80,
	divaV110ParityEven = 2 | 0x80,
	divaV110ParityOne  = 3 | 0x80
} divaV110Parify;

#define SOFT_V110_MODE_ASYNC   1
#define SOFT_V110_MODE_SYNC    2
typedef enum {
	divaV110ModeAsync = SOFT_V110_MODE_ASYNC,
	divaV110ModeSync  = SOFT_V110_MODE_SYNC
} divaV110Mode;

typedef enum {
	divaV110Rate24000   = V110_RATE_24000,
	divaV110Rate600     =  V110_RATE_600,
	divaV110Rate1200    =  V110_RATE_1200,
	divaV110Rate2400    =  V110_RATE_2400,
	divaV110Rate4800    =  V110_RATE_4800,
	divaV110Rate9600    =  V110_RATE_9600,
	divaV110Rate19200   =  V110_RATE_19200,
	divaV110Rate38400   =  V110_RATE_38400,
	divaV110Rate48000   =  V110_RATE_48000,
	divaV110Rate56000   =  V110_RATE_56000,
	divaV110Rate7200    =  V110_RATE_7200,
	divaV110Rate14400   =  V110_RATE_14400,
	divaV110Rate28800   =  V110_RATE_28800,
	divaV110Rate12000   =  V110_RATE_12000,
	divaV110Rate1200_75 =  V110_RATE_1200_75,
	divaV110Rate75_1200 =  V110_RATE_75_1200
} divaV110Rate;


/* bytes of data to be received before starting the machine. For this time
   as much 0xff will be sent, as data was received */
#define V110_DETECTION_DELAY   (10*1024)

#define DIVA_NR_OF_V110_DETECTORS 4


/* the V.110 state machine */
#define SOFTV110_PHASE_INIT    0
#define SOFTV110_PHASE_IDLE    1
#define SOFTV110_PHASE_SYNC    2
#define SOFTV110_PHASE_TOLINE  3
#define SOFTV110_PHASE_DATA    4
#define SOFTV110_PHASE_DISC    5

#define FRM_STATE_ZERO_OCTETT   10
#define FRM_STATE_SYNC          11

#define DIVA_V110_NEED_FRAMES_TO_DETECT 10

void diva_init_v110_detector (void* C, void* P, soft_v110_state* pdetectors);
int diva_v110_detector_process (soft_v110_state* pdetectors, byte* pData, dword length,
																dword* /* OUT */ pdetected_rate,
																int*  last_detected_v110_rate);

#endif

