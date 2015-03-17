
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

#ifndef __DIVA_HDLC_DETECTOR_H__
#define __DIVA_HDLC_DETECTOR_H__

#define DDALFAX_MAX_HDLC_FRAME_LENGTH (2048+7+2)
typedef struct _diva_soft_hdlc {
	dword hdlc_rx_resid_data_bits;
	word hdlc_rx_resid_bit_count;
	word hdlc_rx_state;
	word hdlc_rx_flag_counter;
	word hdlc_rx_crc;

  byte rx_message[DDALFAX_MAX_HDLC_FRAME_LENGTH+4];
  word rx_message_size;
  word rx_message_pos;

	void* C;
	void* P;
} diva_soft_hdlc_t;

void diva_init_hdlc_rx_process (diva_soft_hdlc_t *This, void* C, void* P);
byte* diva_hdlc_rx_process (diva_soft_hdlc_t *This, byte hdlc_byte, word* /* OUT */ frame_length);


#endif
