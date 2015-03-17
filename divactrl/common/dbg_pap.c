
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
static char *
copyField (unsigned char *data, int len, char *line)
{
 int i, ch ;
 for ( i = 0 ; i < len ; ++i, ++line )
 {
  ch = data[i] & 0x007F ;
  if ( (ch < ' ') || (ch == 0x7F) )
   *line = (char)'.' ;
  else
   *line = (char)ch ;
 }
 *line = '\0' ;
 return (line) ;
}
/*****************************************************************************/
void
analysePAP (unsigned char *data, long len, char *line)
{
 int   pap_id, pap_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 pap_id  = (int)data[1] ;
 pap_len = (int)((data[2] << 8) | data[3]) ;
 add_on  = strchr (line, '\0') ;
 if ( len != pap_len )
  return ;
/*
 * Analyse PAP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  PAP : id %2d, len %2d ==> Authenticate-Req",
           pap_id, pap_len) ;
  if ( len >= 6 )
  {
   strcat (add_on, "\n      - Peer-Id : ") ;
   add_on += strlen (add_on) ;
   pap_len = (int)data[4] ;
   add_on = copyField (&data[5], pap_len, add_on) ;
   strcat (add_on, "\n      - Password: ") ;
   add_on += strlen (add_on) ;
   pap_len += 5 ;
   (void)copyField (&data[pap_len + 1], (int)data[pap_len], add_on) ;
  }
  return ;
 case  2:
  sprintf (add_on, "\n  PAP : id %2d, len %2d ==> Authenticate-Ack",
           pap_id, pap_len) ;
  break ;
 case  3:
  sprintf (add_on, "\n  PAP : id %2d, len %2d ==> Authenticate-Nak",
           pap_id, pap_len) ;
  break ;
 default:
  return ;
 }
/*
 * special handling for bay routers, no msg-length field for Ack/Nak
 */
 if ( len > 5 )
 {
  strcat (add_on, "\n      - ") ;
  add_on += strlen (add_on) ;
  pap_len = (int)data[4] ;
  if ( pap_len == (len - 5) )
  {
   (void)copyField (&data[5], pap_len, add_on) ;
  }
  else
  {
   pap_len = (len - 4) ;
   (void)copyField (&data[4], pap_len, add_on) ;
  }
 }
}
