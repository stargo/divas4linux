
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

#include "platform.h"
#include "debuglib.h"
#include "v110.h"

typedef struct _diva_v110_irate_sequence {
	const char* name;
	dword	speed;
	byte	e_rate;
	divaV110Rate v110_rate;
	byte         idi_rate;
} diva_v110_irate_sequence_t;

diva_v110_irate_sequence_t v110_irate8K[] = {
	{ "600",    600,   0x40, divaV110Rate600,    1 },
	{ "1200",   1200,  0x20, divaV110Rate1200,   2 },
	{ "4800",   4800,  0x30, divaV110Rate4800,   4 },
	{ "2400",   2400,  0xff, divaV110Rate2400,   3 } /* default for irate==8K */
};

diva_v110_irate_sequence_t v110_irate16K[] = {
	{ "9600",   9600,  0xff, divaV110Rate9600,   5 } /* default for irate==16K */
};

diva_v110_irate_sequence_t v110_irate32K[] = {
	{ "19200",  19200, 0xff, divaV110Rate19200,  6  } /* default for irate=32K */
};

diva_v110_irate_sequence_t v110_irate64K[] = {
	{ "38400",  38400, 0xff, divaV110Rate38400,  7  } /* default for irate=64K */
};

typedef struct _diva_v110_detector_rate {
	const char*  name;
	divaV110Rate v110_base_rate;
	diva_v110_irate_sequence_t* sequence;
} diva_v110_detector_rate_t;

static diva_v110_detector_rate_t diva_v110_detector_setup[DIVA_NR_OF_V110_DETECTORS+1] = {
	{ "8K",  divaV110Rate2400,  v110_irate8K  },
	{ "16K", divaV110Rate9600,  v110_irate16K },
	{ "32K", divaV110Rate19200, v110_irate32K },
	{ "64K", divaV110Rate38400, v110_irate64K },
	{ 0,     0,                 0             }
};

#define POSITIVE_OF_A_ONE       0x05   /* positive compensation of a one */
#define POSITIVE_OF_A_ZERO      0x04   /* positive compensation of a zero */
#define NEGATIVE_COMPENSAT      0x06   /* negative compensation */        

/*
	IMPORTS
	*/
extern char* get_port_name (void* hP);
#define port_name(__x__)  ((__x__) ? get_port_name(__x__) : "?")
extern void* mem_set (void* s1, int c, unsigned long len);

/*
	LOCALS
	*/
static byte *do_deframe(soft_v110_state *pstate, byte* pdata, dword len, dword* removed);
static diva_v110_irate_sequence_t* do_v110 (soft_v110_state *pstate, byte *pframe);

static void softv110_init (void*  hChannel,
										void*	 hPort,
										int    cfg,
										soft_v110_state *pstate,
										divaV110Mode		 mode,
										divaV110Rate		 rate,
										divaV110DataBits data_bits,
										divaV110StopBits stop_bits,
										divaV110Parify   parity) {
	mem_set(pstate, 0, sizeof(*pstate));

	pstate->C = hChannel;
	pstate->P = hPort;
	pstate->cfg = cfg;

	pstate->v110_rate = rate;
	if ((pstate->v110_cfg_mode = mode) == divaV110ModeAsync) {
		pstate->v110_cfg_bits   = data_bits;
		pstate->v110_cfg_stop   = stop_bits;
		pstate->v110_cfg_parity = parity;
	}

	switch(pstate->v110_rate) {
		case V110_RATE_600:    
		case V110_RATE_1200:   
		case V110_RATE_2400:   
		case V110_RATE_4800:   pstate->v110_irate = V110_IRATE_8K;    break;
		case V110_RATE_7200: 
		case V110_RATE_9600:   pstate->v110_irate = V110_IRATE_16K;   break;
		case V110_RATE_12000: 
		case V110_RATE_14400: 
		case V110_RATE_19200:  pstate->v110_irate = V110_IRATE_32K;   break;
		case V110_RATE_24000:  
		case V110_RATE_28800:  
		case V110_RATE_38400:  pstate->v110_irate = V110_IRATE_64K;   break;
      
		default:
			DBG_ERR(("V110 Setup: unsupported rate (%02x)", pstate->v110_rate))
			return;
	}

	pstate->v110_phase    = SOFTV110_PHASE_IDLE;    /* reset state machine          */
	/*
		This amount of data will by-pass the v110 detector, to prevent triggering of the other
		detectors (HDLC).
		After this interval we still have 2 mechanism to prevent occasitional triggres:
		The frame should be received DIVA_V110_NEED_FRAMES_TO_DETECT times w/o los of sync
		*/
	pstate->detect_delay  = V110_DETECTION_DELAY;
	pstate->detector_counter = 0;

  /* reset V.110 frame detection */
  pstate->rx_deframe_state  = FRM_STATE_ZERO_OCTETT; /* reset frame detection state  */
  pstate->v110_rx_frame_pos = 0;                     /* empty deframe windows        */
  pstate->rx_deframe_len    = 0;                     /* empty V.110 frame buffer     */
  pstate->frame_sync_ctr    = 0;                     /* reset frame counter          */
  pstate->rx_deframe_wnd    = 0;                     /* v.110 frame detection window */
  
  /* reset rx framer (start/stop bit detection) */
  mem_set(&pstate->rx_framer_state, 0, sizeof(FRAMER_STATE));
  pstate->rx_frm_wnd = 0;           /* start/stop bit detection wnd */
  pstate->rx_wnd_len = 0;           /* detection window length     */


	DBG_TRC(("v110 init channel(r:%d m:%d d:%d s:%d p:%d)",
					 pstate->v110_rate, pstate->v110_cfg_mode,
					 pstate->v110_cfg_bits,
					 pstate->v110_cfg_stop,
					 pstate->v110_cfg_parity))
}

static diva_v110_irate_sequence_t* softv110_decode_process (soft_v110_state *pstate,
																														byte* pdata, dword length) {
	dword removed = 0;
  byte *pframe;
	diva_v110_irate_sequence_t* detected = 0;

	while (length) {
		switch (pstate->v110_phase) {
			case SOFTV110_PHASE_IDLE:
				removed = MIN(pstate->detect_delay, length);
				pstate->detect_delay -= removed;
				if (!pstate->detect_delay) {
					pstate->v110_phase = SOFTV110_PHASE_SYNC;
				}
				break;

			case SOFTV110_PHASE_DATA:
				if ((pframe = do_deframe(pstate, pdata, length, &removed))) {
          detected = do_v110(pstate, pframe);
				}
				break;

			default:
				if ((pframe = do_deframe(pstate, pdata, length, &removed))) {
          detected = do_v110(pstate, pframe);
				}
		}
		length -= removed;
    pdata += removed;
	}

	return (detected);
}

/* ************************************************************************ */
/* *                     DEFRAMING & DECODING                             * */
/* ************************************************************************ */
/* ------------------------------------------------------------------------ */
/* Function: do_deframe                                                     */
/* Purpose:  Detect a V.110 frame                                           */
/* Input:    Instance pointer, bit queue containing a stream                */
/* Return:   pointer to a complete V.110 frame                              */
/* ------------------------------------------------------------------------ */
static byte *do_deframe(soft_v110_state *pstate, byte* pdata, dword len, dword* removed) {
  int  bits = 0;
  byte mask=0;
  
  /* set mask and used bits per byte (RA2) */
  switch(pstate->v110_irate) {
    case V110_IRATE_8K:   bits=1; mask = 0x01;     break;
    case V110_IRATE_16K:  bits=2; mask = 0x03;     break;
    case V110_IRATE_32K:  bits=4; mask = 0x0f;     break;
    case V110_IRATE_64K:  bits=8; mask = 0xff;     break;
  }

  *removed = 0;
  while(len > 0) {
    /* fill detection buffer up with the corresponding number of bytes */
    if (pstate->rx_deframe_len <= ((sizeof(dword)*8)-bits)) {
      dword refill=0;
      /* at least the used bits of one byte are free! */
      do {
        refill = ((dword) *pdata) & mask;
        switch(pstate->v110_irate) {
          case V110_IRATE_8K:         
            break;
          case V110_IRATE_16K:
            refill = (refill >> 1) | ( (refill << 1) & 0x02);
            break;
          case V110_IRATE_32K:  
            refill = ( ( (refill<<7) & 0x80) |
                       ( (refill<<5) & 0x40) |
                       ( (refill<<3) & 0x20) |
                       ( (refill<<1) & 0x10) ) >> 4;
            break;
          case V110_IRATE_64K:  
            refill = ( ( (refill<<7) & 0x80) |
                       ( (refill<<5) & 0x40) |
                       ( (refill<<3) & 0x20) |
                       ( (refill<<1) & 0x10) |
                       ( (refill>>7) & 0x01) |
                       ( (refill>>5) & 0x02) |
                       ( (refill>>3) & 0x04) |
                       ( (refill>>1) & 0x08) );            
            break;
        }
        refill <<= ((sizeof(dword)*8)-pstate->rx_deframe_len) /* missing bits */ \
                   - bits;
        pstate->rx_deframe_wnd |= refill;
        pdata++; 
        len--;
        (*removed)++;
        pstate->rx_deframe_len+=bits;

      } while( (pstate->rx_deframe_len < ((sizeof(dword)*8))) && len);
    }
    
    switch(pstate->rx_deframe_state) {
    case FRM_STATE_ZERO_OCTETT: /* search the zero octett */
      if ((pstate->rx_deframe_wnd & 0xff000000) == 0x00) {
        if (pstate->rx_deframe_wnd & 0x00800000) {
          /* We have the leading zero octett and a sync bit.  */
          /* So we can copy and discard the first octett. */
          pstate->v110_rx_frame[0] = 0x00;
          pstate->v110_rx_frame_pos++;
          pstate->rx_deframe_wnd<<= 8;
          pstate->rx_deframe_len-=8;
          pstate->rx_deframe_state++;
        } else {
          /* SYNC bit not found, maybe it is the next one - start again! */
          pstate->rx_deframe_wnd <<=1;
          pstate->rx_deframe_len--;
        }
      } else {
        /* look for the next bit */
          pstate->rx_deframe_wnd <<=1;
          pstate->rx_deframe_len--;
      }
      break;

    case FRM_STATE_SYNC:
      if (pstate->v110_rx_frame_pos <= 8) {
        /* alway look for the next one, too */
        if (pstate->rx_deframe_wnd & 0x80800000) {
          /* So, we can be sure, that we can discard the current byte,
             because it is also no zero octett (=start of frame)!      */
          pstate->v110_rx_frame[pstate->v110_rx_frame_pos++] = (byte) ((pstate->rx_deframe_wnd & 0xff000000) >> 24);
          pstate->rx_deframe_wnd<<=8;
          pstate->rx_deframe_len-=8;
        } else {
          /* we discard the first bit and start again */
          pstate->rx_deframe_wnd<<=1;
          pstate->rx_deframe_len--;

					DBG_TRC(("[%p:%s:%s] failed:",
								pstate->C, port_name(pstate->P), diva_v110_detector_setup[pstate->cfg].name))

					DBG_BLK((pstate->v110_rx_frame, pstate->v110_rx_frame_pos))

          pstate->v110_rx_frame_pos=0;
          pstate->rx_deframe_state=FRM_STATE_ZERO_OCTETT;
					pstate->detector_counter = 0;
        }
      } else {
        /* it is the last octett, so if the SYNC bit is OK, we have a 
           complete frame! */
        if (pstate->rx_deframe_wnd & 0x80000000) {
          /* OK */
          pstate->v110_rx_frame[pstate->v110_rx_frame_pos] = (byte) ((pstate->rx_deframe_wnd & 0xff000000) >> 24);
          pstate->rx_deframe_wnd <<=8;
          pstate->rx_deframe_len-=8;
          pstate->v110_rx_frame_pos=0;
          pstate->rx_deframe_state=FRM_STATE_ZERO_OCTETT;

#if defined (DEBUG_FRAMES_RX)
          dump_frame(pstate, "RX(data frame): ", &pstate->v110_rx_frame[0], 10);
#endif 
					pstate->detector_counter++;
          return(&pstate->v110_rx_frame[0]);
        } else {
          pstate->rx_deframe_wnd<<=1;
          pstate->rx_deframe_len--;
          pstate->v110_rx_frame_pos=0;
          pstate->rx_deframe_state=FRM_STATE_ZERO_OCTETT;
        }

      }
      break;
    default:
			pstate->detector_counter = 0;
      DBG_ERR(("FATAL ERROR: UNKNOWN STATE!!!"))
    }
  }
  return(NULL);
}

void diva_init_v110_detector (void* C, void* P, soft_v110_state* pdetectors) {
	int i;

	for (i = 0; diva_v110_detector_setup[i].name; i++) {
		softv110_init (C, P, i,
									 &pdetectors[i],
									 divaV110ModeAsync,
									 diva_v110_detector_setup[i].v110_base_rate,
									 divaV110_8_DataBits,
									 divaV110_1_StopBit,
                   divaV110ParityNone);
	}
}

int diva_v110_detector_process (soft_v110_state* pdetectors,
																byte* pData, dword length,
																dword* pspeed,
																int* last_detected) {
	dword i;
	diva_v110_irate_sequence_t* detected;

	for (i = 0; diva_v110_detector_setup[i].name; i++) {
		if ((detected = softv110_decode_process (&pdetectors[i], pData, length))) {
			if (detected->idi_rate != *last_detected) {
				int j;
				*last_detected = detected->idi_rate;
				for (j = 0; diva_v110_detector_setup[j].name; j++) {
					pdetectors[j].detector_counter  = 0;
				}
			}
			if (pdetectors[i].detector_counter < DIVA_V110_NEED_FRAMES_TO_DETECT) {
				return (0);
			}
			DBG_TRC(("[%p:%s] detected v110 at %s",
							 pdetectors[i].C, port_name(pdetectors[i].P), detected->name))
			*pspeed = detected->speed;
			return (detected->idi_rate);
		}
	}

	return (0);
}


/* ------------------------------------------------------------------------ */
/* Function: do_v110                                                        */
/* Purpose:  Do the V.110 state machine, called each time a complete frame  */
/*           has been detected in the RX data                               */
/* ------------------------------------------------------------------------ */
static diva_v110_irate_sequence_t* do_v110 (soft_v110_state *pstate, byte *pframe) {
  byte e, e_rate = pframe[5] & 0x70, s = 0, x, e_bits;
	int i;

  /* ----------------------------------------- */
  /* | 00 | S1 | S3 | S4 | S6 | S8 | S9 |  0 | */
  /* ----------------------------------------- */
	s |= (pframe[1] & 0x01);    s<<=1;
	s |= (pframe[3] & 0x01);    s<<=1; 
	s |= (pframe[4] & 0x01);    s<<=1;
	s |= (pframe[6] & 0x01);    s<<=1;
	s |= (pframe[8] & 0x01);    s<<=1;
	s |= (pframe[9] & 0x01);    s<<=1;
  x = (pframe[2] & 0x01);

  /* ----------------------------------------- */
  /* | 00 | 00 | 00 | 00 | E4 | E5 | E6 | E7 | */
  /* ----------------------------------------- */
	e_bits = (pframe[5] & 0x0f);

  DBG_TRC(("[%p:%s:%s] detected v110 frame, erate=%02x, s=%02x, x=%02x, e_bits=%02x lookup:%d",
            pstate->C, port_name(pstate->P),
						diva_v110_detector_setup[pstate->cfg].name, e_rate, s, x, e_bits,
						pstate->detector_counter))

	for (i = 0; ; i++) {
		e = diva_v110_detector_setup[pstate->cfg].sequence[i].e_rate;
		if ((e == 0xff) || (e == e_rate)) {
			return (&diva_v110_detector_setup[pstate->cfg].sequence[i]);
		}
	}

	return (0);
}

