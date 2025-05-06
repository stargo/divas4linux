
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

#include <platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * make "dimaint.h" working
 */
#include "dimaint.h"
#include "dbgioctl.h"
/******************************************************************************/
extern void capiAnalyse (FILE *out, XLOG *msg) ;
/******************************************************************************/
static void
putAsc (FILE *out, unsigned char *data, int addr, int len)
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
dilogAnalyse (FILE *out, DbgMessage *msg, int DilogFormat)
{
 XLOG          *xp ;
 char           direction, tmp[MSG_FRAME_MAX_SIZE] ;
 dword          type, framelen, headlen, datalen, dumplen, i ;
 int            channel ;
/*
 * Check length of traditional xlog format
 */
 switch (msg->size)
 {
 case 0:
  fputs ("- malformed empty xlog format\n", out) ;
  return ;
 case 1:
  fprintf (out, "- malformed xlog format: 0x%02X\n",
           msg->data[0]) ;
  return ;
 case 2:
  fprintf (out, "- malformed xlog format: 0x%02X 0x%02X\n",
           msg->data[0], msg->data[1]) ;
  return ;
 default:
  break ;
 }
 xp   = (XLOG *)&msg->data[0] ;
 type = xp->code & 0x00FF ;
/*
 * Log B-channel traffic here, pass any other stuff to Xlog()
 */
 if ( (msg->size >= 4) && ((type == 1) || (type == 2)) )
 {
  direction = (type == 1) ? 'X' : 'R' ;
  channel   = (signed char)((xp->code >> 8) & 0x00ff) ;
  framelen  = xp->info.l1.length ;
  headlen   = (unsigned long)&((XLOG *)0)->info.l1.i[0] ;
  datalen   = ( msg->size > headlen ) ? (msg->size - headlen) : 0 ;
  if ( datalen > framelen )
    datalen = framelen ;
  if ( channel >= 0 )
   fprintf (out, "B%d-%c(%03d) ", channel, direction, framelen) ;
  else
   fprintf (out, "B-%d-%c(%03d) ", ~channel, direction, framelen) ;
  if ( datalen == 0 )
  {
   fputs (" 0 bytes\n", out) ;
   return ;
  }
  dumplen = datalen ;
  if ( dumplen > 28 )
    dumplen = 28 ;
  if ( DilogFormat )
  {
   for ( i = 0; i < dumplen; i++ )
    fprintf (out, "%02X ", xp->info.l1.i[i]) ;
   if ( framelen > dumplen )
    fputs ("cont", out) ;
   fputs ("\n", out) ;
   return ;
  }
/*
 * Output raw data
 */
  fprintf (out, "%d bytes", dumplen) ;
  for ( i = 0 ; i < dumplen ; i += 16 )
  {
   putAsc (out, &xp->info.l1.i[i], i, dumplen - i) ;
  }
  fputs ("\n", out) ;
  return ;
 }
/*
 * use traditional dilog interpreter
 */
 if ( type )
 {
  /* sorry, they don't respect size of data */
  datalen = (msg->size < sizeof(tmp)) ?
             msg->size : sizeof(tmp) - 1 ;
  memcpy (&tmp[0], &msg->data[0], datalen) ;
  memset (&tmp[datalen], '\0', sizeof(tmp) - datalen) ;
  if ((type == 0x80) || (type == 0x81))
  {
   capiAnalyse (out, (XLOG *)&tmp[0]) ;
  }
  else
  {
   xlog_stream (out, (XLOG *)&tmp[0]) ;
  }
  return ;
 }
/*
 * Output raw data
 */
 fprintf (out, "%d bytes", msg->size) ;
 for ( i = 0 ; i < msg->size ; i += 16 )
 {
  putAsc (out, &msg->data[i], i, msg->size - i) ;
 }
 fprintf (out, "\n") ;
}
/*****************************************************************************/
#include <pshpack1.h>
/*
    'struct mlog' should be packed without any padding
 */
struct mlog
{ word  code ;
 word  timeh ;
 word  timel ;
 char   buffer[256] ;
} ;
#include <poppack.h>
static byte        tmpBuf[2200] ;
static DbgMessage *dLogMsg = (DbgMessage *)&tmpBuf[0] ;
void
xlogAnalyse (FILE *out, DbgMessage *msg)
{
 dword        msec, sec, proc ;
 struct mlog *mwork = (struct mlog *)&msg->data[0] ;
 msec = (((dword )mwork->timeh) << 16) + mwork->timel ;
 sec  = msec / 1000 ;
 proc = mwork->code >> 8 ;
 if ( proc > 0 )
 {
  fprintf (out, "%lu:%04ld:%03ld - P(%d) ",
           (unsigned long)sec / 3600, (unsigned long)sec % 3600, (unsigned long)msec % 1000, proc) ;
 }
 else
 {
  fprintf (out, "%lu:%04ld:%03ld - ",
           (unsigned long)sec / 3600, (unsigned long)sec % 3600, (unsigned long)msec % 1000) ;
 }
 switch (mwork->code & 0xFF)
 {
 case 1:
  fprintf (out, "%s\n", mwork->buffer) ;
  break ;
 case 2:
  dLogMsg->size = msg->size - 6 ;
  memcpy (&dLogMsg->data[0], &mwork->buffer[0], dLogMsg->size) ;
  dilogAnalyse (out, dLogMsg, 1) ;
  break ;
 default:
  fprintf (out, "unknown message type 0x%02x\n", mwork->code & 0xFF) ;
  break ;
 }
}
