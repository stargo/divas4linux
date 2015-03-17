
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
static void
authReqOpts (unsigned char *data, int len, char *add_on)
{
 int code, oLen ;
 while ( len >= 2 )
 {
  add_on += strlen (add_on) ;
  code = (int)data[0] ;
  oLen = (int)(data[1] - 2) ;
  data += 2 ;
  len  -= 2 ;
  if ( (oLen < 0) || (oLen > len) )
   return ;
  switch (code)
  {
  case  1: /* Username */
   strcat (add_on, "\n      - Username: ") ;
   add_on += strlen (add_on) ;
   add_on = copyField (data, oLen, add_on) ;
   break ;
  case  2: /* Password */
   strcat (add_on, "\n      - Password: ") ;
   add_on += strlen (add_on) ;
   add_on = copyField (data, oLen, add_on) ;
   break ;
  case  7: /* Resuming Virtual Connection */
   sprintf (add_on, "\n      - Resuming Virtual Connection") ;
   break ;
  case 11: /* Capability Bitmask */
   sprintf (add_on, "\n      - Capability Bitmask 0x%02X%02X",
            data[0], data[1]) ;
   break ;
  }
  data += oLen ;
  len  -= oLen ;
 }
}
/*****************************************************************************/
static void
commonOptions (unsigned char *data, int len, char *add_on)
{
 int code, oLen ;
 while ( len >= 2 )
 {
  add_on += strlen (add_on) ;
  code = (int)data[0] ;
  oLen = (int)(data[1] - 2) ;
  data += 2 ;
  len  -= 2 ;
  if ( (oLen <= len) && (oLen > 4) )
  {
   strcat (add_on, "\n      - ") ;
   add_on += strlen (add_on) ;
   add_on = copyField (data, oLen, add_on) ;
  }
  data += oLen ;
  len  -= oLen ;
 }
}
/*****************************************************************************/
void
analyseSPAP (unsigned char *data, long len, char *line)
{
 int   spap_id, spap_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 spap_id  = (int)data[1] ;
 spap_len = (int)((data[2] << 8) | data[3]) ;
 add_on   = strchr (line, '\0') ;
 if ( len < spap_len )
  return ;
/*
 * Analyse SPAP packet code
 */
 switch (data[0])
 {
 case  1: /* Old Authenticate-Request (obsolete) */
  break ;
 case  2: /* Authenticate-Ack */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Authenticate-Ack",
           spap_id, spap_len) ;
  break ;
 case  3: /* Authenticate-Callback */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Authenticate-Callback",
           spap_id, spap_len) ;
  break ;
 case  4: /* Authenticate-Nak */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Authenticate-Nak",
           spap_id, spap_len) ;
  break ;
 case  5: /* Request-Authentication */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Request-Authentication",
           spap_id, spap_len) ;
  break ;
 case  6: /* Authenticate-Request */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Authenticate-Req",
           spap_id, spap_len) ;
  add_on += strlen (add_on) ;
  authReqOpts (data + 4, spap_len - 4, add_on) ;
  break ;
 case  7: /* Change Password */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Change Password",
           spap_id, spap_len) ;
  break ;
 case  8: /* Alert */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Alert",
           spap_id, spap_len) ;
  break ;
 case  9: /* Alert-Ack */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Alert-Ack",
           spap_id, spap_len) ;
  break ;
 case 10: /* MCCP Call-Request */
 case 11: /* MCCP Callback-Request */
 case 12: /* MCCP Ack */
 case 13: /* MCCP Nak */
  break ;
 case 14: /* Dialog-Request */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Dialog-Request",
           spap_id, spap_len) ;
  commonOptions (data + 4, spap_len - 4, add_on) ;
  break ;
 case 15: /* Dialog-Response */
  sprintf (add_on, "\n  SPAP: id %2d, len %2d ==> Dialog-Response",
           spap_id, spap_len) ;
  commonOptions (data + 4, spap_len - 4, add_on) ;
  break ;
 default:
  break ;
 }
}
