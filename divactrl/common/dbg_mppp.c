
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
extern void analyseProto (unsigned char *data, long len, char *line) ;
extern void analyseCntrl (unsigned char *data, long len, char *line) ;
extern void analyseLink  (unsigned char *data, long len, char *line) ;
/*****************************************************************************/
void
analyseMLPPP (unsigned char *buffer, long len, char *line)
{
 long  mp_id, mp_len = len ;
/*
 * Check length of buffer against length of frame
 */
 if ( mp_len < 4 )
  return ;
 if ( buffer[1] )
 {
  mp_id = (long)((buffer[0] & 0x0F) << 8)
        | (long)buffer[1] ;
  buffer += 2 ;
  mp_len -= 2 ;
 }
 else
 {
  mp_id = ((long)buffer[1] << 16)
        | ((long)buffer[2] <<  8)
        |  (long)buffer[3] ;
  buffer += 4 ;
  mp_len -= 4 ;
 }
 if ( mp_len < 4 )
  return ;
/*
 * Analyse MLPPP packet code
 */
 switch (buffer[0])
 {
 case 0x02:
  return ;
 case 0x80:
  sprintf (line, "\nML_PPP: id %2ld, len %2ld", mp_id, len) ;
  analyseCntrl (&buffer[1], mp_len - 1, strchr (line, '\0')) ;
  return ;
 case 0xC0:
 case 0xC2:
 case 0xC4:
  sprintf (line, "\nML_PPP: id %2ld, len %2ld", mp_id, len) ;
  analyseLink (buffer, mp_len, strchr (line, '\0')) ;
  return ;
 case 0x00:
  ++buffer ;
  --mp_len ;
 default:
  break ;
 }
}
