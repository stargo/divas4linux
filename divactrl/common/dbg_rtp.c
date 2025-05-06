
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

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbgioctl.h"

/*****************************************************************************/
int dbg_rtp_filter_ssrc    = 0;
dword dbg_rtp_ssrc         = 0;
int dbg_rtp_filter_payload = 0;
dword dbg_rtp_payload      = 0;
dword dbg_rtp_direction    = 0;
/******************************************************************************/
static int first_packet = 1;
static word expected_seq;
/******************************************************************************/
static void
putAsc (FILE *out, const unsigned char *data, int addr, int len)
{
 char buffer[80] ;
 int  i, j, ch ;
 if ( len > 16 )
  len = 16 ;
 if ( len <= 0 )
  return ;
 memset (&buffer[0], ' ', sizeof(buffer)) ;
 sprintf (&buffer[0], "\n  0x%04lx  ", (long)addr) ;
 for ( i = 0, j = 11 ; i < len ; ++i, j += 3 )
 {
  sprintf ((char *)&buffer[(i / 4) + j], "%02X", data[i]) ;
  buffer[(i / 4) + j + 2] = (char)' ' ;
  ch = data[i] & 0x007F ;
  if ( (ch < ' ') || (ch == 0x7F) )
   buffer[63 + i] = (char)'.' ;
  else
   buffer[63 + i] = (char)ch ;
 }
 buffer[79] = '\0' ;
 fputs (&buffer[0], out) ;
}
/*****************************************************************************/
void
rtpAnalyse (FILE *out, DbgMessage *msg, int real_message_length, int dir)
{
 unsigned char *buffer = &msg->data[0] ;
 char           line[1024] ;
 long           i, len = msg->size ;

 int rtp_v, rtp_p, rtp_x, csrc_count, rtp_m, rtp_pt, addr;
 word rtp_sequence_number;
 dword rtp_timestamp, rtp_ssrc;
 const byte* p;
 dword payload_length;

/*
 * Check length of buffer against length of frame
 */
 if ( len < 12 )
  return ;
 rtp_v = (byte)buffer[0]>>6 & 3;
 if (rtp_v != 2)
	 return ;

 rtp_x      = (buffer[0] >> 4) & 1;
 rtp_p      = (buffer[0] >> 5) & 1;
 csrc_count = buffer[0] & 0x0f;
 rtp_m      = (buffer[1] >> 7) & 1;
 rtp_pt     = buffer[1] & 0x7f;
 rtp_sequence_number = (((word)buffer[2]) << 8) | ((word)buffer[3]);
 rtp_timestamp = (((dword)buffer[4]) << 24) | (((dword)buffer[5]) << 16) | (((dword)buffer[6]) << 8) | ((dword)buffer[7]);
 rtp_ssrc = (((dword)buffer[8]) << 24) | (((dword)buffer[9]) << 16) | (((dword)buffer[10]) << 8) | ((dword)buffer[11]);

 if (dbg_rtp_filter_ssrc != 0 && dbg_rtp_ssrc != rtp_ssrc)
   return;
 if (dbg_rtp_filter_payload != 0 && dbg_rtp_payload != rtp_pt)
   return;

 sprintf (line, "\n  %s RTP : pt %3d, %s%s%s, seq %u/%04x, ts %08x, ssrc 0x%08x",
					dir != 0 ? "RX" : "TX",
					rtp_pt,
					rtp_x != 0 ? "X" : "-",
					rtp_p != 0 ? "P" : "-",
					rtp_m != 0 ? "M" : "-",
					rtp_sequence_number,
					rtp_sequence_number,
					rtp_timestamp,
					rtp_ssrc);

 if (dbg_rtp_filter_ssrc != 0 && dbg_rtp_filter_payload == 0 && (dbg_rtp_direction == 1 || dbg_rtp_direction == 2)) {
   if (first_packet == 0) {
     if (rtp_sequence_number != expected_seq) {
       fputs (&line[0], out) ;
       sprintf (line, ", seq error rcv:%u/%04x != %u/%04x", rtp_sequence_number, rtp_sequence_number, expected_seq, expected_seq);
     }
   } else {
     first_packet = 0;
   }
   expected_seq = rtp_sequence_number;
   expected_seq++;
 }

 fputs (&line[0], out) ;
 p = &buffer[12];
 for (i=0; i<csrc_count; i++) {
   rtp_ssrc = (((dword)p[0]) << 24) | (((dword)p[1]) << 16) | (((dword)p[2]) << 8) | ((dword)p[3]);
   sprintf (line, ", csrc%ld 0x%08x", i, rtp_ssrc);
	 fputs (&line[0], out) ;
 }

 payload_length = msg->size - (p - &buffer[0]);
 sprintf (line, ", plen: %u", payload_length);
 fputs (&line[0], out) ;

 if ((dbg_rtp_filter_ssrc & 0x02) != 0) {
   char name[24];
   FILE* f;

   snprintf (name, sizeof(name)-1, "pt-%03d-%08x.raw", dbg_rtp_payload, dbg_rtp_ssrc);
   name[sizeof(name)-1] = 0;

   f = fopen (name, "ab+");

   if (f != 0) {
     if (fwrite (p, 1, payload_length, f) != payload_length) {
			perror (name);
			fclose (f);
			exit (1);
		 }
     fclose (f);
   } else {
     perror (name);
     exit (1);
   }
 }

 addr = 0;
 while (payload_length != 0) {
   int to_print = MIN(payload_length, 16);
   putAsc (out, p, addr, to_print) ;
   p += to_print;
   addr += to_print;
   payload_length -= to_print;
 }

 fputs ("\n", out) ;
}

