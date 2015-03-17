
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
#include "hdlc.h"

/*
	IMPORTS
	*/
extern char* get_port_name (void* hP);
#define port_name(__x__)  ((__x__) ? get_port_name(__x__) : "?")


static word ddal_hdlc_crc_table[0x100] =
{
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static byte ddal_trailing_zeroes_table[0x100] =
{
  8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
  4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};


#define ddal_word_trailing_zeroes(w)\
  (((w) & 0xff) ? ddal_trailing_zeroes_table[(w) & 0xff] : ddal_trailing_zeroes_table[(w) >> 8] + 8)

#define DDAL_HDLC_RX_STATE_SEARCH_FLAG  0
#define DDAL_HDLC_RX_STATE_SEARCH_DATA  1
#define DDAL_HDLC_RX_STATE_DATA         2
#define DDAL_HDLC_CRC_INIT  0xffff
#define DDAL_HDLC_CRC_OK    0xf0b8

static void ddalfax_rx_begin_message (diva_soft_hdlc_t *This, word size) {
	if (sizeof(This->rx_message) < size) {
		DBG_ERR(("Too smal buffer (%s:%d)", __FILE__, __LINE__))
		size = sizeof(This->rx_message);
	}
	This->rx_message_pos = 0;
	This->rx_message_size = size;
}

void diva_init_hdlc_rx_process (diva_soft_hdlc_t *This, void* C, void* P) {
	This->hdlc_rx_resid_data_bits = 0;
	This->hdlc_rx_resid_bit_count = 0;
	This->hdlc_rx_state           = 0;
	This->hdlc_rx_flag_counter    = 0;
	This->hdlc_rx_crc             = 0;
  This->rx_message_size         = 0;
  This->rx_message_pos          = 0;
	This->C                       = C;
	This->P                       = C;
}

byte* diva_hdlc_rx_process (diva_soft_hdlc_t *This, byte hdlc_byte, word* /* OUT */ pframe_length)
{
  word hdlc_eated_bit_count;
  word hdlc_new_data_bits;
  word w, x;

  This->hdlc_rx_resid_data_bits |= ((dword) hdlc_byte) << This->hdlc_rx_resid_bit_count;
  This->hdlc_rx_resid_bit_count += 8;
  do
  {
    hdlc_new_data_bits = (word)(This->hdlc_rx_resid_data_bits);
    switch (This->hdlc_rx_state)
    {
    case DDAL_HDLC_RX_STATE_SEARCH_FLAG:
      if (This->hdlc_rx_resid_bit_count < 16)
        return (0);
      w = hdlc_new_data_bits & 0x7ffe;
      w &= w << 1;
      w &= w << 2;
      w &= w << 2;
      w &= ~hdlc_new_data_bits << 6;
      w &= ~hdlc_new_data_bits >> 1;
      if (w == 0)
        hdlc_eated_bit_count = 9;
      else
      {
        This->hdlc_rx_flag_counter = 0;
        This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_DATA;
        hdlc_eated_bit_count = ddal_word_trailing_zeroes (w) - 3;
      }
      break;

    case DDAL_HDLC_RX_STATE_SEARCH_DATA:
      if (This->hdlc_rx_resid_bit_count < 13)
        return (0);
      if ((hdlc_new_data_bits & 0x1fe0) == 0x0fc0)
      {
        if (This->hdlc_rx_flag_counter < 0x00ff)
          (This->hdlc_rx_flag_counter)++;
        hdlc_eated_bit_count = 8;
        break;
      }
      ddalfax_rx_begin_message (This, DDALFAX_MAX_HDLC_FRAME_LENGTH + 2);
      if (This->rx_message == NULL)
      {
				DBG_ERR(("[%p:%s] hdlc_rx_process no free message buffer (%s:%d)",
								 This->C, port_name(This->P), __FILE__, __LINE__))
        hdlc_eated_bit_count = 0;
        This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_FLAG;
        break;
      }
      This->hdlc_rx_crc = DDAL_HDLC_CRC_INIT;
      This->hdlc_rx_state = DDAL_HDLC_RX_STATE_DATA;
    case DDAL_HDLC_RX_STATE_DATA:
      if (This->hdlc_rx_resid_bit_count < 16)
      {
        if (This->hdlc_rx_resid_bit_count < 13)
          return (0);
        if ((This->hdlc_rx_resid_bit_count > 13)
         && ((hdlc_new_data_bits & 0x3fff) == 0x1f9f))
        {
          hdlc_eated_bit_count = 9;
        }
        else if ((hdlc_new_data_bits & 0x1fe0) == 0x0fc0)
          hdlc_eated_bit_count = 8;
        else
          return (0);
        if (This->hdlc_rx_crc == DDAL_HDLC_CRC_OK) {
					if (This->rx_message_pos > 2) {
						*pframe_length = This->rx_message_pos - 2;
						return (&This->rx_message[0]);
					}
				} else {
					DBG_TRC(("[%p:%s] hdlc_rx_process wrong CRC HDLC data (%s:%d)",
									This->C, port_name(This->P), __FILE__, __LINE__))
        }
        This->hdlc_rx_flag_counter = 0;
        This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_DATA;
        break;
      }
      w = (word) hdlc_new_data_bits;
      w &= w << 1;
      w &= w << 2;
      w &= w << 1;
      if ((w & 0x0fff) == 0)
        hdlc_eated_bit_count = 8;
      else if (w & (w << 1))
      {
        if ((hdlc_new_data_bits & 0x3fff) == 0x1f9f)
        {
          if (This->hdlc_rx_crc == DDAL_HDLC_CRC_OK) {
						if (This->rx_message_pos > 2) {
							*pframe_length = This->rx_message_pos - 2;
							return (&This->rx_message[0]);
						}
					} else {
						DBG_TRC(("[%p:%s] hdlc_rx_process wrong CRC HDLC data (%s:%d)",
										This->C, port_name(This->P), __FILE__, __LINE__))
          }
          hdlc_eated_bit_count = 9;
          This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_DATA;
        }
        else if ((hdlc_new_data_bits & 0x1fe0) == 0x0fc0)
        {
          if (This->hdlc_rx_crc == DDAL_HDLC_CRC_OK) {
						if (This->rx_message_pos > 2) {
							*pframe_length = This->rx_message_pos-2;
							return (&This->rx_message[0]);
						}
					} else {
						DBG_TRC(("[%p:%s] hdlc_rx_process wrong CRC HDLC data (%s:%d)",
										This->C, port_name(This->P), __FILE__, __LINE__))
          }
          hdlc_eated_bit_count = 8;
          This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_DATA;
        }
        else
        {
					// DBG_TRC(("[%p:%s] hdlc_rx_process aborted HDLC data (%s:%d)",
					//				This->C, port_name(This->P), __FILE__, __LINE__))
          hdlc_eated_bit_count = 8;
          This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_FLAG;
        }
        This->hdlc_rx_flag_counter = 0;
        break;
      }
      else
      {
        x = w ^ (w - 1);
        hdlc_new_data_bits += hdlc_new_data_bits & x;
        w &= ~x;
        if ((w & 0x1fff) == 0)
          hdlc_eated_bit_count = 9;
        else
        {
          x = w ^ (w - 1);
          hdlc_new_data_bits += hdlc_new_data_bits & x;
          hdlc_eated_bit_count = 10;
        }
      }
      w = (word)(hdlc_new_data_bits >> (hdlc_eated_bit_count - 3));
      This->hdlc_rx_crc = (This->hdlc_rx_crc >> 8) ^ ddal_hdlc_crc_table[(This->hdlc_rx_crc ^ w) & 0xff];
      if (This->rx_message_pos + 2 < This->rx_message_size)
        This->rx_message[(This->rx_message_pos)++] = (byte) w;
      else
      {
				DBG_TRC(("[%p:%s] hdlc_rx_process too long HDLC frame (%s:%d)",
								This->C, port_name(This->P), __FILE__, __LINE__))
        This->hdlc_rx_state = DDAL_HDLC_RX_STATE_SEARCH_FLAG;
      }
      break;

    default:
      hdlc_eated_bit_count = 0;
    }
    This->hdlc_rx_resid_data_bits >>= hdlc_eated_bit_count;
    This->hdlc_rx_resid_bit_count -= hdlc_eated_bit_count;
  } while (TRUE);

	return (0);
}

