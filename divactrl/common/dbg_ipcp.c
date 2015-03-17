
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
ipcpDumpAddr (char *text, char *add_on, unsigned char *data, int len)
{
 if ( len == 6 )
 {
  sprintf (add_on, "\n        - %s %d.%d.%d.%d",
           text, data[2], data[3], data[4], data[5]) ;
 }
}
/*****************************************************************************/
static void
IPCPoptions (unsigned char *data, long len, char *line)
{
 int   i, opt_len ;
 char *add_on ;
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
  case   1:
   strcpy (add_on, "\n        - IP Addresses (depredated)") ;
   break ;
  case   2:
   if ( (data[2] == 0x00) && (data[3] == 0x2D) )
   {
    strcpy (add_on, "\n        - Van Jacobson Compressed TCP/IP") ;
    add_on += strlen (add_on) ;
   }
   else
   {
    add_on += sprintf (add_on,
                       "\n        - IP compression Protocol 0x%04X",
                       (data[2] << 9) | data[3]) ;
   }
   if ( opt_len > 4 )
   {
    add_on += sprintf (add_on, " - Max-Slot-Id %d", data[4]) ;
   }
   if ( opt_len > 5 )
   {
    add_on += sprintf (add_on, " - Comp-Slot-Id %d", data[5]) ;
   }
   break ;
  case   3:
   ipcpDumpAddr ("IP Address", add_on, data, opt_len) ;
   break ;
  case 129:
   ipcpDumpAddr ("Primary   DNS  Address", add_on, data, opt_len) ;
   break ;
  case 130:
   ipcpDumpAddr ("Primary   NBNS Address", add_on, data, opt_len) ;
   break ;
  case 131:
   ipcpDumpAddr ("Secondary DNS  Address", add_on, data, opt_len) ;
   break ;
  case 132:
   ipcpDumpAddr ("Secondary NBNS Address", add_on, data, opt_len) ;
   break ;
  default:
   add_on += sprintf (add_on, "\n        - Option 0x%02X", data[0]) ;
/*
 * add optional values
 */
   if ( opt_len == 6 )
   {
    ipcpDumpAddr ("Special   IP   Address", add_on, data, opt_len) ;
   }
   else
   {
    add_on += sprintf (add_on, " - Values:") ;
    for ( i = 2 ; i < opt_len ; ++i )
    {
     add_on += sprintf (add_on, " 0x%02X", data[i]) ;
    }
   }
   break ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseIPCP (unsigned char *data, long len, char *line)
{
 int   ipcp_id, ipcp_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 ipcp_id = (int)data[1] ;
 ipcp_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != ipcp_len )
  return ;
/*
 * Analyse IPCP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Configure-Request",
           ipcp_id, ipcp_len) ;
  IPCPoptions (data + 4, ipcp_len - 4, add_on) ;
  break ;
 case  2:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Configure-Ack",
           ipcp_id, ipcp_len) ;
  IPCPoptions (data + 4, ipcp_len - 4, add_on) ;
  break ;
 case  3:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Configure-Nak",
           ipcp_id, ipcp_len) ;
  IPCPoptions (data + 4, ipcp_len - 4, add_on) ;
  break ;
 case  4:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Configure-Reject",
           ipcp_id, ipcp_len) ;
  IPCPoptions (data + 4, ipcp_len - 4, add_on) ;
  break ;
 case  5:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Terminate-Request",
           ipcp_id, ipcp_len) ;
  break ;
 case  6:
  sprintf (add_on, "\n  IPCP: id %2d, len %2d ==> Terminate-Ack",
           ipcp_id, ipcp_len) ;
  break ;
 case  7:
  sprintf (add_on, "\n IPCP : id %2d, len %2d ==> Code-Reject",
           ipcp_id, ipcp_len) ;
  add_on = strchr (line, '\0') ;
  analyseIPCP (data + 4, ipcp_len - 4, add_on) ;
  break ;
 default:
  break ;
 }
}
