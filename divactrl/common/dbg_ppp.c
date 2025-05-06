
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
#include "dbgioctl.h"
/******************************************************************************/
extern void analyseLink  (unsigned char *data, long len, char *line) ;
extern void analysePAP   (unsigned char *data, long len, char *line) ;
extern void analyseSPAP  (unsigned char *data, long len, char *line) ;
extern void analyseCHAP  (unsigned char *data, long len, char *line) ;
extern void analyseCCP   (unsigned char *data, long len, char *line) ;
extern void analyseIPCP  (unsigned char *data, long len, char *line) ;
extern void analyseNBFCP (unsigned char *data, long len, char *line) ;
extern void analyseCBCP  (unsigned char *data, long len, char *line) ;
extern void analyseIPXCP (unsigned char *data, long len, char *line) ;
extern void analyseMLPPP (unsigned char *data, long len, char *line) ;
extern void analyseBACP  (unsigned char *data, long len, char *line) ;
extern void analyseBAP   (unsigned char *data, long len, char *line) ;
extern void analyseESHP  (unsigned char *data, long len, char *line) ;
/******************************************************************************/
static void
putAsc (FILE *out, unsigned char *data, int addr, int len)
{
 char buffer[80] ;
 int  i, j, ch ;
 if ( len > 16 )
  len = 16 ;
 if ( len <= 0 )
  return ;
 memset (&buffer[0], ' ', sizeof(buffer)) ;
 sprintf (&buffer[0], "\n  0x%04lx  ", (long)addr) ;
 for ( i = 0, j = 11 ; i < len ; ++i, j += 3 )
 {
  sprintf ((char *)&buffer[(i / 4) + j], "%02X", data[i]) ;
  buffer[(i / 4) + j + 2] = (char)' ' ;
  ch = data[i] & 0x007F ;
  if ( (ch < ' ') || (ch == 0x7F) )
   buffer[63 + i] = (char)'.' ;
  else
   buffer[63 + i] = (char)ch ;
 }
 buffer[79] = '\0' ;
 fputs (&buffer[0], out) ;
}
/*****************************************************************************/
void
analyseProto (unsigned char *data, long len, char *line)
{
/*
 * Check length of buffer against length of frame
 */
 if ( len < 1 )
  return ;
 switch (data[0])
 {
 case 0x01:
  strcat (line, "\n  Padding Protocol") ;
  break ;
 case 0x03: case 0x05: case 0x07: case 0x09: case 0x0B:
 case 0x0D: case 0x0F: case 0x11: case 0x13: case 0x15:
 case 0x17: case 0x19: case 0x1B: case 0x1D: case 0x1F:
  strcat (line, "\n  reserved (transparency inefficient)") ;
  break ;
 case 0x21:
  strcat (line, "\n  Internet Protocol") ;
  break ;
 case 0x23:
  strcat (line, "\n  OSI Network Layer") ;
  break ;
 case 0x25:
  strcat (line, "\n  Xerox NS IDP") ;
  break ;
 case 0x27:
  strcat (line, "\n  DECnet Phase IV") ;
  break ;
 case 0x29:
  strcat (line, "\n  Appletalk") ;
  break ;
 case 0x2B:
  strcat (line, "\n  Novell IPX") ;
  break ;
 case 0x2D:
  strcat (line, "\n  Van Jacobsen Compressed TCP/IP") ;
  break ;
 case 0x2F:
  strcat (line, "\n  Van Jacobsen Uncompressed TCP/IP") ;
  break ;
 case 0x31:
  strcat (line, "\n  Bridging PDU") ;
  break ;
 case 0x33:
  strcat (line, "\n  Stream Protocol (ST-II)") ;
  break ;
 case 0x35:
  strcat (line, "\n  Banyan Vines") ;
  break ;
 case 0x39:
  strcat (line, "\n  AppleTalk EDDP") ;
  break ;
 case 0x3B:
  strcat (line, "\n  AppleTalk SmartBuffered") ;
  break ;
 case 0x3D:
  analyseMLPPP (data + 1, len - 1, line) ;
  break ;
 case 0x3F:
  strcat (line, "\n  NETBIOS Framing") ;
  break ;
 case 0x41:
  strcat (line, "\n  Cisco Systems") ;
  break ;
 case 0x43:
  strcat (line, "\n  Ascom Timeplex") ;
  break ;
 case 0x45:
  strcat (line, "\n  Fujitsu Link Backup and Load Balancing (LBLB)") ;
  break ;
 case 0x47:
  strcat (line, "\n  DCA Remote Lan") ;
  break ;
 case 0x49:
  strcat (line, "\n  Serial Data Transport Protocol (PPP-SDTP)") ;
  break ;
 case 0x4B:
  strcat (line, "\n  SNA over 802.2") ;
  break ;
 case 0x4D:
  strcat (line, "\n  SNA") ;
  break ;
 case 0x4F:
  strcat (line, "\n  IP6 Header Compression") ;
  break ;
 case 0x6F:
  strcat (line, "\n  Stampede Bridging") ;
  break ;
 case 0x7D:
  strcat (line, "\n  reserved (Control Escape)          [RFC 1661]") ;
  break ;
 case 0x7F:
  strcat (line, "\n  reserved (compression inefficient  [RFC 1662]") ;
  break ;
 case 0xCF:
  strcat (line, "\n  reserved (PPP NLPID)") ;
  break ;
 case 0xFB:
  strcat (line, "\n  compression on single link in multilink group") ;
  break ;
 case 0xFD:
  strcat (line, "\n  1st choice compression") ;
  break ;
 case 0xFF:
  strcat (line, "\n  reserved (compression inefficient)") ;
  break ;
 default:
  break ;
 }
}
/*****************************************************************************/
static void
analyseSomething (unsigned char *data, long len, char *line)
{
/*
 * Check length of buffer against length of frame
 */
 if ( len < 1 )
  return ;
 switch (data[0])
 {
 case 0x01:
  strcat (line, "\n  802.1d Hello Packets") ;
  break ;
 case 0x03:
  strcat (line, "\n  IBM Source Routing BPDU") ;
  break ;
 case 0x05:
  strcat (line, "\n  DEC LANBridge100 Spanning Tree") ;
  break ;
 case 0x31:
  strcat (line, "\n  Luxcom") ;
  break ;
 case 0x33:
  strcat (line, "\n  Sigma Network Systems") ;
  break ;
 default:
  break ;
 }
}
/*****************************************************************************/
void
analyseCntrl (unsigned char *data, long len, char *line)
{
/*
 * Check length of buffer against length of frame
 */
 if ( len < 1 )
  return ;
 switch (data[0])
 {
 case 0x01: case 0x7D: case 0xCF: case 0xFF:
 case 0x03: case 0x05: case 0x07: case 0x09: case 0x0B:
 case 0x0D: case 0x0F: case 0x11: case 0x13: case 0x15:
 case 0x17: case 0x19: case 0x1B: case 0x1D: case 0x1F:
  strcat (line, "\n  Not Used - reserved           [RFC 1661]") ;
  break ;
 case 0x21:
  analyseIPCP (data + 1, len - 1, line) ;
  break ;
 case 0x23:
  strcat (line, "\n  OSI Network Layer Control Protocol") ;
  break ;
 case 0x25:
  strcat (line, "\n  Xerox NS IDP Control Protocol") ;
  break ;
 case 0x27:
  strcat (line, "\n  DECnet Phase IV Control Protocol") ;
  break ;
 case 0x29:
  strcat (line, "\n  Appletalk Control Protocol") ;
  break ;
 case 0x2B:
  analyseIPXCP (data + 1, len - 1, line) ;
  break ;
 case 0x2D: case 0x2F: case 0x39: case 0x3B:
  strcat (line, "\n  reserved") ;
  break ;
 case 0x31:
  strcat (line, "\n  Bridging NCP") ;
  break ;
 case 0x33:
  strcat (line, "\n  Stream Protocol Control Protocol") ;
  break ;
 case 0x35:
  strcat (line, "\n  Banyan Vines Control Protocol") ;
  break ;
 case 0x3D:
  strcat (line, "\n  Multi-Link Control Protocol") ;
  break ;
 case 0x3F:
  analyseNBFCP (data + 1, len - 1, line) ;
  break ;
 case 0x41:
  strcat (line, "\n  Cisco Systems Control Protocol") ;
  break ;
 case 0x43:
  strcat (line, "\n  Ascom Timeplex") ;
  break ;
 case 0x45:
  strcat (line, "\n  Fujitsu LBLB Control Protocol") ;
  break ;
 case 0x47:
  strcat (line, "\n  DCA Remote Lan Network Control Protocol (RLNCP)") ;
  break ;
 case 0x49:
  strcat (line, "\n  Serial Data Control Protocol (PPP-SDCP)") ;
  break ;
 case 0x4B:
  strcat (line, "\n  SNA over 802.2 Control Protocol") ;
  break ;
 case 0x4D:
  strcat (line, "\n  SNA Control Protocol") ;
  break ;
 case 0x4F:
  strcat (line, "\n  IP6 Header Compression Control Protocol") ;
  break ;
 case 0x6F:
  strcat (line, "\n  Stampede Bridging Control Protocol") ;
  break ;
 case 0xFB:
  strcat (line, "\n  compression on single link in multilink group control") ;
  break ;
 case 0xFD:
  analyseCCP (data + 1, len - 1, line) ;
  break ;
 default:
  break ;
 }
}
/*****************************************************************************/
static void
authOptsSPAP (unsigned char *data, unsigned long len, char *add_on)
{
 dword mask = 0 ;
 strcpy (add_on, "\n        - Authentication-Protocol ==> SPAP") ;
 add_on += strlen (add_on) ;
 switch (len)
 {
 default:
 case 8:
  mask = (unsigned long)data[7] ;
 case 7: /* ignore missing bytes in this mask */
  mask |= (unsigned long)(data[6] << 8) ;
 case 6:
  mask |= (unsigned long)(data[5] << 16) ;
 case 5:
  mask |= (unsigned long)(data[4] << 24) ;
  sprintf (add_on, "\n          -- Capability Bitmask 0x%08X", mask) ;
  add_on += strlen (add_on) ;
 case 4:
  sprintf (add_on, "\n          -- SPAP Version 0x%02X%02X",
           data[2], data[3]) ;
  add_on += strlen (add_on) ;
 case 3:
 case 2:
  sprintf (add_on, "\n          -- Padding %d", data[1]) ;
  add_on += strlen (add_on) ;
 case 1:
  sprintf (add_on, "\n          -- Encryption 0x%02X", data[0]) ;
 case 0:
  break ;
 }
}
/*****************************************************************************/
static void
authOptsCHAP (unsigned char *data, unsigned long len, char *add_on)
{
 if ( len == 1 )
 {
  if ( data[0] == 5 )
  {
   strcpy (add_on,
           "\n        - Authentication-Protocol ==> CHAP-MD5") ;
   return ;
  }
  sprintf (add_on,
           "\n        - Authentication-Protocol ==> CHAP-%d", data[0]) ;
 }
}
/*****************************************************************************/
static void
AUTHoption (unsigned char *data, unsigned long len, char *line)
{
 char *add_on ;
 dword  prot ;
 add_on = strchr (line, '\0') ;
 prot = (data[2] << 8) | data[3] ;
 switch (prot)
 {
 case 0xC023:
  if ( len == 4 )
  {
   strcpy (add_on, "\n        - Authentication-Protocol ==> PAP") ;
  }
  break ;
 case 0xC027:
  authOptsSPAP (data + 4, len - 4, add_on) ;
  break ;
 case 0xC223:
  authOptsCHAP (data + 4, len - 4, add_on) ;
  break ;
 case 0xC281:
  strcpy (add_on, "\n        - Proprietary Authentication Protocol") ;
  break ;
 default:
  sprintf (add_on, "\n        - Authentication-Protocol ==> 0x%04X",
           prot) ;
  break ;
 }
}
/*****************************************************************************/
static void
LCPoptions (unsigned char *data, long len, char *line)
{
 int   opt_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 while ( len >= 2 )
 {
  opt_len = (int)data[1] ;
  if ( (opt_len < 2) || (opt_len > len) )
   return ;
  add_on = strchr (line, '\0') ;
/*
 * Analyse option packet code
 */
  switch (data[0])
  {
  case 1:
   sprintf (add_on, "\n        - Maximum-Receive-Unit %d",
            (data[2] << 8) | data[3]) ;
   break ;
  case 2:
   if ( opt_len == 6 )
   {
    sprintf (add_on, "\n        - Async-CC-Map 0x%08X",
             (data[2] << 24) | (data[3] << 16)
           | (data[4] <<  8) |  data[5]      ) ;
   }
   else
   {
    strcpy (add_on, "\n        - Async-CC-Map illegal") ;
   }
   break ;
  case 3:
   AUTHoption (data, opt_len, add_on) ;
   break ;
  case 4:
   if ( opt_len == 4 )
   {
    sprintf (add_on, "\n        - Quality-Protocol 0x%04X",
             (data[2] << 8) + data[3]) ;
    break ;
   }
   if ( opt_len == 8 )
   {
    sprintf (add_on,
             "\n        - Quality-Protocol 0x%04X - Period %ld ms",
             (data[2] << 8) + data[3], (long)((data[4] << 24)
             + (data[5] << 16) + (data[6] << 8) + data[7]) * 10) ;
   }
   break ;
  case 5:
   sprintf (add_on, "\n        - Magic-Number 0x%02X%02X%02X%02X",
            data[2], data[3], data[4], data[5]) ;
   break ;
  case 7:
   strcpy (add_on, "\n        - Protocol-Field-Compression") ;
   break ;
  case 8:
   strcpy (add_on, "\n        - Addr-and-Control-Field-Compression") ;
   break ;
  case 9:
   sprintf (add_on, "\n        - FCS-Alternatives ==> %s FCS",
            (data[2] == 1 ? "Null"
                          : (data[2] == 2 ? "CCITT 16-Bit"
                                          : "CCITT 32-Bit")) ) ;
   break ;
  case 10:
   strcpy (add_on, "\n        - Self-Describing-Pad") ;
   break ;
  case 11:
   strcpy (add_on, "\n        - Numbered-Mode") ;
   break ;
  case 12:
   strcpy (add_on, "\n        - Multi-Link-Procedure") ;
   break ;
  case 13:
   add_on += sprintf (add_on, "\n        - Callback ==> ") ;
   switch (data[2])
   {
   case 0:
    strcpy (add_on, "Location determined by user authentication") ;
    break ;
   case 1:
    strcpy (add_on, "Dialing string = ") ;
    add_on += strlen (add_on) ;
    memcpy (add_on, &data[3], opt_len - 3) ;
    add_on += (opt_len - 3) ;
    *add_on = '\0' ;
    break ;
   case 2:
    strcpy (add_on, "Location Identifier = ") ;
    add_on += strlen (add_on) ;
    memcpy (add_on, &data[3], opt_len - 3) ;
    add_on += (opt_len - 3) ;
    *add_on = '\0' ;
    break ;
   case 3:
    strcpy (add_on, "E.164 Number") ;
    break ;
   case 4:
    strcpy (add_on, "X.500 distinguished name") ;
    break ;
   case 6:
    strcpy (add_on, "Location is determined during CBCP negotiation") ;
    break ;
   default:
    break ;
   }
   break ;
  case 14:
   strcpy (add_on, "\n        - Connect-Time") ;
   break ;
  case 15:
   strcpy (add_on, "\n        - Compound-Frames") ;
   break ;
  case 16:
   strcpy (add_on, "\n        - Nominal-Data-Encapsulation") ;
   break ;
  case 17:
   sprintf (add_on, "\n        - Multilink-MRRU %d",
            (data[2] << 8) | data[3]) ;
   break ;
  case 18:
   strcpy (add_on, "\n        - Multilink-Short-Sequence-Number-Header-Format") ;
   break ;
  case 19:
   strcpy (add_on, "\n        - Multilink-Endpoint-Discriminator") ;
   add_on += strlen (add_on) ;
   switch (data[2])
   {
   case 0:
    strcpy (add_on, "\n          -- Null Class") ;
    break ;
   case 1:
    strcpy (add_on, "\n          -- Locally Assigned Address") ;
    break ;
   case 2:
    sprintf (add_on, "\n          -- IP-Address %d.%d.%d.%d",
             data[3], data[4], data[5], data[6]) ;
    break ;
   case 3:
    sprintf (add_on, "\n          -- IEEE 802.1 MAC Addr %02X.%02X.%02X.%02X.%02X.%02X",
             data[3], data[4], data[5],
             data[6], data[7], data[8]) ;
    break ;
   case 4:
    strcpy (add_on, "\n          -- PPP Magic-Number Block") ;
    break ;
   case 5:
    strcpy (add_on, "\n          -- Public Switched Network Directory Number") ;
    break ;
   default:
    break ;
   }
   break ;
  case 20:
   sprintf (add_on, "\n        - Proprietary                [KEN]") ;
   break ;
  case 21:
   sprintf (add_on, "\n        - DCE-Identifier       [SCHNEIDER]") ;
   break ;
  case 23:
   if ( opt_len == 4 )
   {
    sprintf (add_on, "\n        - BAP Link Discriminator 0x%04X",
             (data[2] << 8) + data[3]) ;
   }
   break ;
  default:
   sprintf (add_on, "\n        - Unknown Option 0x%X, len %d",
            data[0], opt_len) ;
   break ;
  }
  data += opt_len ;
  len -= opt_len ;
 }
}
/*****************************************************************************/
static void
analyseLCP (unsigned char *data, long len, char *line)
{
 int   frame_id, frame_len ;
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 4 )
  return ;
 frame_id = (int)data[1] ;
 frame_len = (data[2] << 8) | data[3] ;
 add_on = strchr (line, '\0') ;
 if ( len != frame_len )
  return ;
/*
 * Analyse LCP packet code
 */
 switch (data[0])
 {
 case  1:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Configure-Request",
           frame_id, frame_len) ;
  LCPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  2:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Configure-Ack",
           frame_id, frame_len) ;
  LCPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  3:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Configure-Nak",
           frame_id, frame_len) ;
  LCPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  4:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Configure-Reject",
           frame_id, frame_len) ;
  LCPoptions (data + 4, len - 4, add_on) ;
  break ;
 case  5:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Terminate-Request",
           frame_id, frame_len) ;
  break ;
 case  6:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Terminate-Ack",
           frame_id, frame_len) ;
  break ;
 case  7:
  sprintf (add_on, "\n LCP  : id %2d, len %2d ==> Code-Reject",
           frame_id, frame_len) ;
  add_on = strchr (line, '\0') ;
  analyseLCP (data + 4, len - 4, add_on) ;
  break ;
 case  8:
  sprintf (add_on, "\n LCP  : id %2d, len %2d ==> Protocol-Reject",
           frame_id, frame_len) ;
  data += 4 ;
  len -= 4 ;
  switch (data[0])
  {
  case 0x00:
   ++data ;
   --len ;
  default:
   if ( data[0] == 0x3D ) /* only accept MLPPP frames */
   {
    analyseProto (data, len, add_on) ;
   }
   break ;
  case 0x02:
   analyseSomething (&data[1], len - 1, add_on) ;
   break ;
  case 0x80:
   analyseCntrl (&data[1], len - 1, add_on) ;
   break ;
  case 0xC0:
  case 0xC2:
  case 0xC4:
   analyseLink (data, len, add_on) ;
   break ;
  }
  break ;
 case  9:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Echo-Request",
           frame_id, frame_len) ;
  break ;
 case 10:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Echo-Reply",
           frame_id, frame_len) ;
  break ;
 case 11:
  analyseESHP (data, frame_len, add_on) ;
  break ;
 case 12:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Identification (LCP)",
           frame_id, frame_len) ;
  break ;
 case 13:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Time-Remaining (LCP)",
           frame_id, frame_len) ;
  break ;
 case 14:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Reset-Request (CCP)",
           frame_id, frame_len) ;
  break ;
 case 15:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Reset-Reply (CCP)",
           frame_id, frame_len) ;
  break ;
 default:
  break ;
 }
}
/*****************************************************************************/
static void
analyseLQR (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  LQR : Link Quality Report") ;
}
/*****************************************************************************/
static void
analyse_CCP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  _CCP: Container Control Protocol") ;
}
/*****************************************************************************/
static void
analyseSBAP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  SBAP: Stampede Bridging Authorization Protocol") ;
}
/*****************************************************************************/
static void
analysePpAP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  PpAP: Proprietary Authentication Protocol") ;
}
/*****************************************************************************/
static void
analysePNAP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  PNAP: Proprietary Node ID Authentication Protocol") ;
}
/*****************************************************************************/
void
analyseLink (unsigned char *buffer, long len, char *line)
{
 switch (buffer[0])
 {
 case 0xC0:
  switch (buffer[1])
  {
  case 0x21:
   analyseLCP (&buffer[2], len - 2, line) ;
   break ;
  case 0x23:
   analysePAP (&buffer[2], len - 2, line) ;
   break ;
  case 0x25:
   analyseLQR (&buffer[2], len - 2, line) ;
   break ;
  case 0x27:
   analyseSPAP (&buffer[2], len - 2, line) ;
   break ;
  case 0x29:
   analyseCBCP (&buffer[2], len - 2, line) ;
   break ;
  case 0x2B:
   analyseBACP (&buffer[2], len - 2, line) ;
   break ;
  case 0x2D:
   analyseBAP (&buffer[2], len - 2, line) ;
   break ;
  case 0x81:
   analyse_CCP (&buffer[2], len - 2, line) ;
   break ;
  default:
   break ;
  }
  break ;
 case 0xC2:
  switch (buffer[1])
  {
  case 0x23:
   analyseCHAP (&buffer[2], len - 2, line) ;
   break ;
  case 0x6F:
   analyseSBAP (&buffer[2], len - 2, line) ;
   break ;
  case 0x81:
   analysePpAP (&buffer[2], len - 2, line) ;
   break ;
  default:
   break ;
  }
  break ;
 case 0xC4:
  switch (buffer[1])
  {
  case 0x81:
   analysePNAP (&buffer[2], len - 2, line) ;
   break ;
  default:
   break ;
  }
 }
}
/*****************************************************************************/
void
pppAnalyse (FILE *out, DbgMessage *msg, int real_message_length)
{
 unsigned char *buffer = &msg->data[0] ;
 char           line[1024] ;
 long           i, len = msg->size ;
/*
 * strip PPP framing
 */
 if ( (len >= 2) && !memcmp (buffer, "\xFF\x03", 2) )
 {
  buffer += 2 ;
  len -= 2 ;
 }
 else
 {
  if ( (len >= 3) && !memcmp (buffer, "\xFF\x7D\x23", 3) )
  {
   buffer += 3 ;
   len -= 3 ;
  }
  else
  {
   if ( (len >= 4) && !memcmp (buffer, "\x7D\xDF\x7D\x23", 4) )
   {
    buffer += 4 ;
    len -= 4 ;
   }
  }
 }
/*
 * Check for consistence with ISO 3309 extension mechanism for address field
 */
 line[0] = '\0' ;
 if ( (len >= 4) && ((buffer[0] ^ buffer[1]) & 1) )
 {
  switch (buffer[0])
  {
  case 0x00:
   ++buffer ;
   --len ;
  default:
   if ( buffer[0] == 0x3D ) /* only accept MLPPP frames */
   {
    analyseProto (buffer, len, &line[0]) ;
   }
   break ;
  case 0x02:
   analyseSomething (&buffer[1], len - 1, &line[0]) ;
   break ;
  case 0x80:
   analyseCntrl (&buffer[1], len - 1, &line[0]) ;
   break ;
  case 0xC0:
  case 0xC2:
  case 0xC4:
   analyseLink (buffer, len, &line[0]) ;
   break ;
  }
 }
/*
 * Output resulting text and raw data (if not supressed)
 */
 if ( msg->id == 0 && msg->type == 0 )
 {
  if ( line[0] != '\0' )
  {
   fputs (&line[0], out) ;
  }
 }
 else
 {
  fprintf (out, "%d bytes", msg->size) ;
  fputs (&line[0], out) ;
  for ( i = 0 ; i < (long)msg->size ; i += 16 )
  {
   putAsc (out, &msg->data[i], i, msg->size - i) ;
  }
 }
 fputs ("\n", out) ;
}
/******************************************************************************/
void
infoAnalyse (FILE *out, DbgMessage *msg)
{
 unsigned long i ;
 fprintf (out, "%d bytes", msg->size) ;
 for ( i = 0 ; i < msg->size ; i += 16 )
 {
  putAsc (out, &msg->data[i], i, msg->size - i) ;
 }
 fprintf (out, "\n") ;
}
