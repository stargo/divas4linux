
/*
 *
  Copyright (c) Eicon Networks, 2005.
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
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
/*****************************************************************************/
static  int frame_id, frame_len ;
/*****************************************************************************/
static void
shortholdOptions (unsigned char *data, int len, char *add_on, char *action)
{
 sprintf (add_on, "\n  ESHP: id %2d, len %2d ==> %s",
          frame_id, frame_len, action) ;
 add_on += strlen (add_on) ;
 sprintf (add_on, "\n      - Magic 0x%02X%02X%02X%02X",
          data[4], data[5], data[6], data[7]) ;
}
/*****************************************************************************/
void
analyseESHP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 frame_id  = (int)data[1] ;
 frame_len = (int)((data[2] << 8) | data[3]) ;
 add_on    = strchr (line, '\0') ;
 if ( (len >= 48) && (data[8] == 0xAB) && (data[9] == 0xAB) )
 {
/*
 * Analyse ESHP packet code
 */
  switch (data[10])
  {
  case  1:
   shortholdOptions (data, len, add_on, "Shorthold-Request") ;
   return ;
  case  2:
   shortholdOptions (data, len, add_on, "Shorthold-Ack") ;
   return ;
  case  3:
   shortholdOptions (data, len, add_on, "Resume-Request") ;
   return ;
  case  4:
   shortholdOptions (data, len, add_on, "Resume-Ack") ;
   return ;
  case  5:
   shortholdOptions (data, len, add_on, "Shorthold-Reject") ;
   return ;
  case  6:
   shortholdOptions (data, len, add_on, "Resume-Reject") ;
   return ;
  case  7:
   shortholdOptions (data, len, add_on, "Resume-Reconnect") ;
   return ;
  default:
   break ;
  }
 }
 sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Discard-Request",
          frame_id, frame_len) ;
}
