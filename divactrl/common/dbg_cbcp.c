
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
/*****************************************************************************/
static char *
copyField (unsigned char *data, int len, char *line)
{
 int i, ch ;
 strcat (line, " '") ;
 line += strlen (line) ;
 for ( i = 0 ; i < len ; ++i, ++line )
 {
  ch = data[i] & 0x007F ;
  if ( (ch < ' ') || (ch == 0x7F) )
   *line = (char)'.' ;
  else
   *line = (char)ch ;
 }
 *line++ = '\'' ;
 *line = '\0' ;
 return (line) ;
}
/*****************************************************************************/
static void
CBCPoptions (unsigned char *data, long len, char *line)
{
 int   i, l, opt_len ;
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
  case 1:
   strcpy (add_on, "\n        - No callback") ;
   break ;
  case 2:
   strcpy (add_on, "\n        - Callback to user specified #") ;
   break ;
  case 3:
   strcpy (add_on, "\n        - Callback to pre-specified #") ;
   break ;
  case 4:
   strcpy (add_on, "\n        - Callback to any of a list of #") ;
   break ;
  default:
   sprintf (add_on, "\n        - Unknown CBCP Option 0x%02X",
            data[0]) ;
   break ;
  }
  if ( opt_len > 2 )
  {
   add_on += strlen (add_on) ;
   add_on += sprintf (add_on, "\n          -- Callback delay %d",
                      data[2]) ;
  }
  for ( i = 3 ; i < opt_len ; )
  {
   if ( data[i] == 1 )
   {
    add_on += sprintf (add_on, "\n          -- PSTN/ISDN Addr ==>") ;
   }
   else
   {
    add_on += sprintf (add_on, "\n          -- Addr Type 0x%02X ==>",
                       data[i]) ;
   }
   l = (int)strlen ((char*)&data[i + 1]) ;
   add_on = copyField (&data[i + 1], l, add_on) ;
   i += (1 + l + 1) ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseCBCP (unsigned char *data, long len, char *line)
{
 int   cbcp_id, cbcp_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 cbcp_id = (int)data[1] ;
 cbcp_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != cbcp_len )
  return ;
/*
 * Analyse CBCP packet code
 */
 switch (data[0])
 {
 case  1:
  add_on += sprintf (add_on,
                     "\n  CBCP: id %2d, len %2d ==> Callback Request",
                     cbcp_id, cbcp_len) ;
  CBCPoptions (&data[4], cbcp_len - 4, add_on) ;
  break ;
 case  2:
  add_on += sprintf (add_on,
                     "\n  CBCP: id %2d, len %2d ==> Callback Response",
                     cbcp_id, cbcp_len) ;
  CBCPoptions (&data[4], cbcp_len - 4, add_on) ;
  break ;
 case  3:
  add_on += sprintf (add_on,
                     "\n  CBCP: id %2d, len %2d ==> Callback Ack",
                     cbcp_id, cbcp_len) ;
  CBCPoptions (&data[4], cbcp_len - 4, add_on) ;
  break ;
 default:
  break ;
 }
}
