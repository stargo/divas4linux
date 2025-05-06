
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
static void
copyField (unsigned char *data, int len, char *line)
{
 int i, ch ;
 strcat (line, " - '") ;
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
}
/*****************************************************************************/
void
analyseCHAP (unsigned char *data, long len, char *line)
{
 int   chap_id, chap_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 chap_id = (int)data[1] ;
 chap_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != chap_len )
  return ;
/*
 * Analyse CHAP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  CHAP: id %2d, len %2d ==> Challenge",
           chap_id, chap_len) ;
  if ( chap_len >= 7 )
  {
   chap_len = data[4] + 5 ;
   if ( chap_len < len )
   {
    (void)copyField (&data[chap_len], len - chap_len, add_on) ;
   }
  }
  break ;
 case  2:
  sprintf (add_on, "\n  CHAP: id %2d, len %2d ==> Response",
           chap_id, chap_len) ;
  if ( chap_len >= 7 )
  {
   chap_len = data[4] + 5 ;
   if ( chap_len < len )
   {
    (void)copyField (&data[chap_len], len - chap_len, add_on) ;
   }
  }
  break ;
 case  3:
  sprintf (add_on, "\n  CHAP: id %2d, len %2d ==> Success",
           chap_id, chap_len) ;
  if ( chap_len > 4 )
  {
   (void)copyField (&data[4], chap_len - 4, add_on) ;
  }
  break ;
 case  4:
  sprintf (add_on, "\n  CHAP: id %2d, len %2d ==> Failure",
           chap_id, chap_len) ;
  if ( chap_len > 4 )
  {
   (void)copyField (&data[4], chap_len - 4, add_on) ;
  }
  break ;
 default:
  break ;
 }
}
