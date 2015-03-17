
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
static void
CCPoptions (unsigned char *data, long len, char *line)
{
 int   i, opt_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 while ( (len >= 2) && ((opt_len = (int)data[1]) <= len) )
 {
  if ( opt_len < 2 )
   return ;
  add_on = strchr (line, '\0') ;
/*
 * analyse compression mode
 */
  switch (data[0])
  {
  case  0:
   if ( data[1] > 2 )
   {
    sprintf (add_on,
             "\n        - OUI ==> %02X%02X%02X - Subtype 0x%02X",
             data[2], data[3], data[4], data[5]) ;
   }
   else
   {
    sprintf (add_on, "\n        - OUI ==> no Subtype/Values") ;
   }
   break ;
  case  1: case  2:
   sprintf (add_on, "\n        - Predictor type %d", data[0]) ;
   break ;
  case  3:
   strcpy (add_on, "\n        - Puddle Jumper") ;
   break ;
  case 16:
   strcpy (add_on, "\n        - Hewlett-Packard PPC") ;
   break ;
  case 17:
   strcpy (add_on, "\n        - Stac Electronics LZS") ;
   break ;
  case 18:
   sprintf (add_on, "\n        - Microsoft PPC") ;
   break ;
  case 19:
   strcpy (add_on, "\n        - Gandalf FZA") ;
   break ;
  case 20:
   strcpy (add_on, "\n        - V.42bis compression") ;
   break ;
  case 21:
   strcpy (add_on, "\n        - BSD LZW Compress") ;
   break ;
  default:
   sprintf (add_on,
            "\n        - Unknown Compression Type 0x%02X/len %d",
            data[0], data[1]) ;
   break ;
  }
/*
 * add optional values
 */
  if ( (data[0] > 0) && (opt_len > 2) && (opt_len <= 10) )
  {
   strcat (add_on, " ==> values:") ;
   add_on += strlen (add_on) ;
   for ( i = 2 ; i < opt_len ; ++i )
   {
    add_on += sprintf (add_on, " 0x%02X", data[i]) ;
   }
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseCCP (unsigned char *data, long len, char *line)
{
 int   ccp_id, ccp_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 ccp_id = (int)data[1] ;
 ccp_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != ccp_len )
  return ;
/*
 * Analyse CCP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Configure-Request",
           ccp_id, ccp_len) ;
  CCPoptions (data + 4, ccp_len - 4, add_on) ;
  break ;
 case  2:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Configure-Ack",
           ccp_id, ccp_len) ;
  CCPoptions (data + 4, ccp_len - 4, add_on) ;
  break ;
 case  3:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Configure-Nak",
           ccp_id, ccp_len) ;
  CCPoptions (data + 4, ccp_len - 4, add_on) ;
  break ;
 case  4:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Configure-Reject",
           ccp_id, ccp_len) ;
  CCPoptions (data + 4, ccp_len - 4, add_on) ;
  break ;
 case  5:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Terminate-Request",
           ccp_id, ccp_len) ;
  break ;
 case  6:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Terminate-Ack",
           ccp_id, ccp_len) ;
  break ;
 case  7:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Code-Reject",
           ccp_id, ccp_len) ;
  break ;
 case 14:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Reset-Request",
           ccp_id, ccp_len) ;
  break ;
 case 15:
  sprintf (add_on, "\n  CCP : id %2d, len %2d ==> Reset-Reply",
           ccp_id, ccp_len) ;
  break ;
 default:
  break ;
 }
}
