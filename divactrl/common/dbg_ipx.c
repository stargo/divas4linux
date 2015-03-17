
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
IPXCPoptions (unsigned char *data, long len, char *line)
{
 int   cp, opt_len ;
 char *add_on ;
/*
 * interpret complete buffer contents
 */
 while ( len >= 2 )
 {
  add_on  = strchr (line, '\0') ;
  opt_len = (int)data[1] ;
  if ( (opt_len < 2) || (opt_len > len) )
   return ;
  switch (data[0])
  {
  case 1:
   if ( opt_len == 6 )
   {
    sprintf (add_on, "\n        - IPX-Network-Number %d.%d.%d.%d",
             data[2], data[3], data[4], data[5]) ;
   }
   break ;
  case 2:
   if ( opt_len == 8 )
   {
    sprintf (add_on,
    "\n        - IPX-Node-Number %02x.%02x.%02x.%02x.%02x.%02x",
    data[2], data[3], data[4], data[5], data[6], data[7]) ;
   }
   break ;
  case  3:
   if ( opt_len >= 4 )
   {
    cp = ((int)data[2] << 8) | (int)data[3] ;
    switch (cp)
    {
    case 0x0002:
     strcpy (add_on, "\n        - Telebit Compressed IPX") ;
     break ;
    case 0x0235:
     strcpy (add_on, "\n        - Shiva Compressed NCP/IPX") ;
     break ;
    }
   }
   break ;
  case 4:
   if ( opt_len >= 4 )
   {
    cp = ((int)data[2] << 8) | (int)data[3] ;
    switch (cp)
    {
    case 0x0000:
     strcpy (add_on, "\n        - No routing protocol required") ;
     break ;
    case 0x0002:
     strcpy (add_on, "\n        - Novell RIP/SAP required") ;
     break ;
    case 0x0004:
     strcpy (add_on, "\n        - Novell NLSP required") ;
     break ;
    }
   }
   break ;
  case 5:
   if ( opt_len >= 3 )
   {
    memcpy (add_on, &data[2], opt_len) ;
    add_on[opt_len - 2] = '\0' ;
   }
   break ;
  case 6:
   if ( opt_len == 2 )
   {
    strcpy (add_on, "\n        - IPX-Configuration-Complete") ;
   }
   break ;
  default:
   break ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseIPXCP (unsigned char *data, long len, char *line)
{
 int ipxcp_req, ipxcp_id, ipxcp_len ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 ipxcp_req = (int)data[0] ;
 ipxcp_id  = (int)data[1] ;
 ipxcp_len = ((int)data[2] << 8) | (int)data[3] ;
 if ( len != ipxcp_len )
  return ;
 data      += 4 ;
 ipxcp_len -= 4 ;
/*
 * Analyse IPXCP packet code
 */
 switch (ipxcp_req)
 {
 case  1:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Configure-Request",
           ipxcp_id, ipxcp_len) ;
  IPXCPoptions (data, ipxcp_len, line) ;
  break ;
 case  2:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Configure-Ack",
           ipxcp_id, ipxcp_len) ;
  IPXCPoptions (data, ipxcp_len, line) ;
  break ;
 case  3:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Configure-Nak",
           ipxcp_id, ipxcp_len) ;
  IPXCPoptions (data, ipxcp_len, line) ;
  break ;
 case  4:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Configure-Reject",
           ipxcp_id, ipxcp_len) ;
  IPXCPoptions (data, ipxcp_len, line) ;
  break ;
 case  5:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Terminate-Request",
           ipxcp_id, ipxcp_len) ;
  break ;
 case  6:
  sprintf (line, "\n  IPX : id %2d, len %2d ==> Terminate-Ack",
           ipxcp_id, ipxcp_len) ;
  break ;
 case  7:
  sprintf (line, "\n IPX  : id %2d, len %2d ==> Code-Reject",
           ipxcp_id, ipxcp_len) ;
  line = strchr (line, '\0') ;
  analyseIPXCP (data, ipxcp_len, line) ;
  break ;
 default:
  break ;
 }
}
