
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
catASCIIstring (char *add_on, unsigned char *data, int len)
{
 add_on = strchr (add_on, '\0') ;
 *add_on++ = '\'' ;
 memcpy (add_on, data, len) ;
 for ( add_on += len ; add_on[0] == '\0' ; --add_on ) ;
 add_on[1] = '\'' ;
 add_on[2] = '\0' ;
}
/*****************************************************************************/
static void
checkLinkType (unsigned char *data, int len, char *line)
{
 if ( data[0] & 0x01 )
 {
  strcat (line, "\n          -- ISDN") ;
 }
 if ( data[0] & 0x02 )
 {
  strcat (line, "\n          -- X.25") ;
 }
 if ( data[0] & 0x04 )
 {
  strcat (line, "\n          -- Analog") ;
 }
 if ( data[0] & 0x08 )
 {
  strcat (line, "\n          -- Switched Digital (non ISDN)") ;
 }
 if ( data[0] & 0x10 )
 {
  strcat (line, "\n          -- Data Over Voice") ;
 }
}
/*****************************************************************************/
static void
phoneDeltaSubOption (unsigned char *data, long len, char *line)
{
 int   opt_len ;
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
 * analyse Phone Delta suboptions
 */
  switch (data[0])
  {
  case  1:
   strcat (add_on, "\n          -- Unique Digits ...") ;
   catASCIIstring (add_on, data + 2, opt_len - 2) ;
   break ;
  case  2:
   strcat (add_on, "\n          -- Subscriber Number ") ;
   catASCIIstring (add_on, data + 2, opt_len - 2) ;
   break ;
  case  3:
   strcat (add_on, "\n          -- Phone Number Sub-Address ") ;
   catASCIIstring (add_on, data + 2, opt_len - 2) ;
   break ;
  default:
   break ;
  }
  data += opt_len ;
  len  -= opt_len ;
 }
}
/*****************************************************************************/
static void
BAPoptions (unsigned char *data, long len, char *line)
{
 int   opt_len ;
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
 * analyse BAP options
 */
  switch (data[0])
  {
  case  1:
   if ( opt_len >= 4 )
   {
    sprintf (add_on,
             "\n        - Link Type\n          -- Speed %d kBit/s",
             (data[2] << 8) + data[3]) ;
   }
   if ( opt_len >= 5 )
   {
    checkLinkType (data + 4, opt_len - 4, add_on) ;
   }
   break ;
  case  2:
   if ( opt_len >= 5 )
   {
    strcpy (add_on, "\n        - Phone Delta") ;
    phoneDeltaSubOption (data + 2, opt_len - 2, add_on) ;
   }
   break ;
  case  3:
   if ( opt_len == 2 )
   {
    strcpy (add_on, "\n        - No Phone Number needed") ;
   }
   break ;
  case  4:
   strcpy (add_on, "\n        - Reason ") ;
   catASCIIstring (add_on, data + 2, opt_len - 2) ;
   break ;
  case  5:
   if ( opt_len == 4 )
   {
    sprintf (add_on, "\n        - BAP Link Discriminator 0x%04X",
             (data[2] << 8) + data[3]) ;
   }
   break ;
  case  6:
   if ( opt_len == 4 )
   {
    if ( data[2] == 0 )
    {
     strcpy (add_on, "\n        - BAP Call Status OK") ;
    }
    else
    {
     sprintf (add_on,
              "\n        - BAP Call Status 0x%02X - %s",
              data[2], (data[3] ? "Retry" : "No Retry") ) ;
    }
   }
   break ;
  default:
   sprintf (add_on,
            "\n        - Unknown BAP Option 0x%02X/len %d",
            data[0], data[1]) ;
   break ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
static char *
bapRespType (char *prefix, unsigned char respType)
{
 static char tmp[128] ;
 strcpy (&tmp[0], prefix) ;
 strcat (&tmp[0], "-Response") ;
 switch (respType)
 {
 case 0: strcat (&tmp[0], "-Ack") ;      break ;
 case 1: strcat (&tmp[0], "-Nak") ;      break ;
 case 2: strcat (&tmp[0], "-Reject") ;   break ;
 case 3: strcat (&tmp[0], "-Full-Nak") ; break ;
 }
 return (&tmp[0]) ;
}
/*****************************************************************************/
static void
bapReq (char *line, unsigned char *data, int len, char *text)
{
 int bap_id, bap_len ;
 bap_id  = (int)data[1] ;
 bap_len = (data[2] << 8) | data[3] ;
 sprintf (line, "\n  BAP : id %2d, len %2d ==> %s-Request",
          bap_id, bap_len, text) ;
 BAPoptions (data + 4, len - 4, line) ;
}
/*****************************************************************************/
static void
bapResp (char *line, unsigned char *data, int len, char *text)
{
 int bap_id, bap_len ;
 bap_id  = (int)data[1] ;
 bap_len = (data[2] << 8) | data[3] ;
 sprintf (line, "\n  BAP : id %2d, len %2d ==> %s-Response",
          bap_id, bap_len, bapRespType (text, data[4]) ) ;
 BAPoptions (data + 5, len - 5, line) ;
}
/*****************************************************************************/
void
analyseBAP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 add_on = strchr (line, '\0') ;
 if ( len != (long)data[1] )
  return ;
/*
 * Analyse BAP packet type
 */
 switch (data[0])
 {
 case  1:
  bapReq (add_on, data, len, "Call") ;
  break ;
 case  2:
  bapResp (add_on, data, len, "Call") ;
  break ;
 case  3:
  bapReq (add_on, data, len, "Callback") ;
  break ;
 case  4:
  bapResp (add_on, data, len, "Callback") ;
  break ;
 case  5:
  bapReq (add_on, data, len, "Link-Drop-Query") ;
  break ;
 case  6:
  bapResp (add_on, data, len, "Link-Drop-Query") ;
  break ;
 case  7:
  bapReq (add_on, data, len, "Call-Status") ;
  break ;
 case  8:
  bapResp (add_on, data, len, "Call-Status") ;
  break ;
 default:
  break ;
 }
}
/*****************************************************************************/
static void
BACPoptions (unsigned char *data, long len, char *line)
{
 int   opt_len ;
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
 * analyse BACP requests
 */
  switch (data[0])
  {
  case  1:
   if ( opt_len == 6 )
   {
    sprintf (add_on,
             "\n        - Favored Peer Magic # 0x%02X%02X%02X%02X",
             data[2], data[3], data[4], data[5]) ;
    break ;
   }
  default:
   sprintf (add_on,
            "\n        - Unknown BACP Option 0x%02X/len %d",
            data[0], data[1]) ;
   break ;
  }
  data += opt_len ;
  len  -= opt_len ;
 }
}
/*****************************************************************************/
void
analyseBACP (unsigned char *data, long len, char *line)
{
 int   bacp_id, bacp_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 bacp_id = (int)data[1] ;
 bacp_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != bacp_len )
  return ;
/*
 * Analyse BACP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Configure-Request",
           bacp_id, bacp_len) ;
  BACPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  2:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Configure-Ack",
           bacp_id, bacp_len) ;
  BACPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  3:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Configure-Nak",
           bacp_id, bacp_len) ;
  BACPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  4:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Configure-Reject",
           bacp_id, bacp_len) ;
  BACPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  5:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Terminate-Request",
           bacp_id, bacp_len) ;
  break ;
 case  6:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Terminate-Ack",
           bacp_id, bacp_len) ;
  break ;
 case  7:
  sprintf (add_on, "\n  BACP: id %2d, len %2d ==> Code-Reject",
           bacp_id, bacp_len) ;
  break ;
 default:
  break ;
 }
}
