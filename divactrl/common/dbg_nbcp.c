
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
NBCPoptions (unsigned char *data, long len, char *line, int req)
{
 int   i, opt_len ;
 char *add_on, *opt_class ;
/*
 * Check length of buffer against length of frame
 */
 while ( (len >= 4) && ((opt_len = (int)data[1]) <= len) )
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
   strcpy (add_on, "\n        - Name-Projection") ;
   add_on += strlen (add_on) ;
   if ( req )
   {
    for ( i = 2 ; i < opt_len ; i += 17 )
    {
     add_on += sprintf (add_on, "\n          -- %s Name",
                   (data[i + 16] == 1 ? "Unique" : "Group ")) ;
     add_on = copyField (&data[i], 15, add_on) ;
    }
   }
   else
   {
    for ( i = 2 ; i < opt_len ; i += 17 )
    {
     add_on += sprintf (add_on, "\n          --") ;
     add_on = copyField (&data[i], 15, add_on) ;
     switch (data[i + 16])
     {
     case 0x00:
      strcat (add_on, " Successfully added") ;
      break ;
     case 0x0D:
      strcat (add_on, " Duplicate name in local table") ;
      break ;
     case 0x0E:
      strcat (add_on, " Name table full") ;
      break ;
     case 0x15:
      strcat (add_on, " Name not fount") ;
      break ;
     case 0x16:
      strcat (add_on, " Name in use on remote NetBIOS") ;
      break ;
     case 0x19:
      strcat (add_on, " Name conflict detected") ;
      break ;
     case 0x30:
      strcat (add_on, " Name defined by another environment") ;
      break ;
     case 0x35:
      strcat (add_on, " Required system resources exhausted") ;
      break ;
     default:
      sprintf (add_on, " Undefined result 0x%02X",
               data[i + 16]) ;
      break ;
     }
     add_on += strlen (add_on) ;
    }
   }
   break ;
  case 2:
   if ( opt_len > 8 )
   {
    strcpy (add_on, "\n        - Peer Information for ") ;
    (void)copyField (&data[8], opt_len - 8, add_on) ;
   }
   else
   {
    strcpy (add_on, "\n        - Peer Information") ;
   }
   add_on += strlen (add_on) ;
   switch ((data[2] << 8) | data[3])
   {
   case 1:  case 3:  case 5:  case 7:
    opt_class = "Reserved ..." ;
    break ;
   case 2:
    opt_class = "PPP NetBIOS Gateway Server" ;
    break ;
   case 4:
    opt_class = "PPP Local Access Only Server" ;
    break ;
   case 6:
    opt_class = "PPP NBF Bridge" ;
    break ;
   case 8:
    opt_class = "PPP End-System" ;
    break ;
   default:
    opt_class = "Undefined ..." ;
    break ;
   }
   add_on += sprintf (add_on,
             "\n          -- Class '%s' Version 0x%04X / 0x%04X",
             opt_class, (data[4] << 8) | data[5],
             (data[6] << 8) | data[7]) ;
   break ;
  case  3:
   if ( opt_len == 5 )
   {
    sprintf (add_on,
        "\n        - Multicast-Filtering - Period %d - Prior %d",
        (data[2] << 8) | data[3], data[4]) ;
   }
   break ;
   case 4:
    strcat (add_on, "\n        - IEEE-MAC-Address-Required") ;
    break ;
  default:
   sprintf (add_on, "\n        - Unknown NBFCP Option 0x%02X",
            data[0]) ;
   break ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseNBFCP (unsigned char *data, long len, char *line)
{
 int   nbfcp_id, nbfcp_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 nbfcp_id  = (int)data[1] ;
 nbfcp_len = (data[2] << 8) | data[3] ;
 add_on    = strchr (line, '\0') ;
 if ( len != nbfcp_len )
  return ;
/*
 * Analyse NBFCP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Configure-Request",
           nbfcp_id, nbfcp_len) ;
  NBCPoptions (data + 4, nbfcp_len - 4, add_on, 1) ;
  break ;
 case  2:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Configure-Ack",
           nbfcp_id, nbfcp_len) ;
  NBCPoptions (data + 4, nbfcp_len - 4, add_on, 1) ;
  break ;
 case  3:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Configure-Nak",
           nbfcp_id, nbfcp_len) ;
  NBCPoptions (data + 4, nbfcp_len - 4, add_on, 0) ;
  break ;
 case  4:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Configure-Reject",
           nbfcp_id, nbfcp_len) ;
  NBCPoptions (data + 4, nbfcp_len - 4, add_on, 0) ;
  break ;
 case  5:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Terminate-Request",
           nbfcp_id, nbfcp_len) ;
  break ;
 case  6:
  sprintf (add_on, "\n  NBCP: id %2d, len %2d ==> Terminate-Ack",
           nbfcp_id, nbfcp_len) ;
  break ;
 case  7:
  sprintf (add_on, "\n NBCP : id %2d, len %2d ==> Code-Reject",
           nbfcp_id, nbfcp_len) ;
  add_on = strchr (line, '\0') ;
  analyseNBFCP (data + 4, nbfcp_len - 4, add_on) ;
  break ;
 default:
  break ;
 }
}
