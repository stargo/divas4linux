
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

#include "platform2.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/
extern void analysePAP   (unsigned char *data, long len, char *line) ;
extern void analyseCHAP  (unsigned char *data, long len, char *line) ;
extern void analyseCCP   (unsigned char *data, long len, char *line) ;
extern void analyseIPCP  (unsigned char *data, long len, char *line) ;
extern void analyseNBFCP (unsigned char *data, long len, char *line) ;
extern void analyseCBCP  (unsigned char *data, long len, char *line) ;

/*****************************************************************************/
static void ipAnalyse(const byte* p, int len, char* line);
/*****************************************************************************/
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
/******************************************************************************/
static void
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
	ipAnalyse(&data[1], len-1, &line[strlen(line)]);
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
  strcat (line, "\n  Multi-Link") ;
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
static void
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
  strcat (line, "\n  Novell IPX Control Protocol") ;
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
AUTHoption (unsigned char *data, char *line)
{
 char *add_on ;
 long  prot ;
 add_on = strchr (line, '\0') ;
 prot = (data[2] << 8) | data[3] ;
 switch (prot)
 {
 case 0xC023:
  if ( data[1] == 4 )
  {
   strcpy (add_on, "\n        - Authentification-Protocol ==> PAP") ;
  }
  break ;
 case 0xC027:
  strcpy (add_on, "\n        - Authentification-Protocol ==> SPAP") ;
  break ;
 case 0xC223:
  if ( data[1] != 5 )
   break ;
  if ( data[4] == 5 )
  {
   strcpy (add_on,
           "\n        - Authentification-Protocol ==> CHAP-MD5") ;
  }
  else
  {
   sprintf (add_on,
            "\n        - Authentification-Protocol ==> CHAP-%d",
            data[4]) ;
  }
  break ;
 case 0xC281:
  strcpy (add_on, "\n        - Proprietary Authentification Protocol") ;
  break ;
 default:
  sprintf (add_on, "\n        - Authentification-Protocol ==> 0x%04lX",
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
  if ( opt_len > len )
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
   AUTHoption (data, add_on) ;
   break ;
  case 4:
   sprintf (add_on, "\n        - Quality-Protocol 0x%02X%02X",
            data[2], data[3]) ;
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
  default:
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
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Code-Reject",
           frame_id, frame_len) ;
  break ;
 case  8:
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Protocol-Reject",
           frame_id, frame_len) ;
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
  sprintf (add_on, "\n  LCP : id %2d, len %2d ==> Discard-Request",
           frame_id, frame_len) ;
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
analyseSPAP (unsigned char *data, long len, char *line)
{
 char *add_on ;
/*
 * Check length of buffer against length of frame
 */
 if ( len < 2 )
  return ;
 add_on = strchr (line, '\0') ;
 strcpy (add_on, "\n  SPAP: Shive Password Authentification Protocol") ;
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
 strcpy (add_on, "\n  PpAP: Proprietary Authentification Protocol") ;
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
 strcpy (add_on, "\n  PNAP: Proprietary Node ID Authentification Protocol") ;
}
/*****************************************************************************/
static const char* ip_protocol_name(byte protocol) {
	static char tmp[24];

	switch (protocol) {
		case	1:	return ("ICMP	internet control message protocol");
		case	2:	return ("IGMP	internet group management protocol");
		case	3:	return ("GGP gateway-gateway protocol");
		case	4:	return ("IP-ENCAP	IP encapsulated in IP (officially ``IP'')");
		case	5:	return ("ST	datagram mode");
		case	6:	return ("TCP transmission control protocol");
		case	7:	return ("CBT");
		case	8:	return ("EGP exterior gateway protocol");
		case	9:	return ("IGP any private interior gateway");
		case	10:	return ("BBN-RCC-MON BBN RCC Monitoring");
		case	11:	return ("NVP-II Network Voice Protocol");
		case	12:	return ("PUP PARC universal packet protocol");
		case	13:	return ("ARGUS");
		case	14:	return ("EMCON");
		case	15:	return ("XNET Cross Net Debugger");
		case	16:	return ("CHAOS");
		case	17:	return ("UDP user datagram protocol");
		case	18:	return ("MUX Multiplexing protocol");
		case	19:	return ("DCN-MEAS DCN Measurement Subsystems");
		case	20:	return ("HMP host monitoring protocol");
		case	21:	return ("PRM packet radio measurement protocol");
		case	22:	return ("XNS-IDP Xerox NS IDP");
		case	23:	return ("TRUNK-1");
		case	24:	return ("TRUNK-2");
		case	25:	return ("LEAF-1");
		case	26:	return ("LEAF-2");
		case	27:	return ("RDP reliable datagram protocol");
		case	28:	return ("IRTP Internet Reliable Transaction Protocol");
		case	29:	return ("ISO-TP4 ISO Transport Protocol Class 4");
		case	30:	return ("NETBLT Bulk Data Transfer Protocol");
		case	31:	return ("MFE-NSP MFE Network Services Protocol");
		case	32:	return ("MERIT-INP MERIT Internodal Protocol");
		case	33:	return ("SEP Sequential Exchange Protocol");
		case	34:	return ("3PC Third Party Connect Protocol");
		case	35:	return ("IDPR Inter-Domain Policy Routing Protocol");
		case	36:	return ("XTP Xpress Tranfer Protocol");
		case	37:	return ("DDP Datagram Delivery Protocol");
		case	38:	return ("IDPR-CMTP IDPR Control Message Transport Protocol");
		case	39:	return ("TP++ Transport Protocol");
		case	40:	return ("IL Transport Protocol");
		case	41:	return ("IPv6");
		case	42:	return ("SDRP Source Demand Routing Protocol");
		case	43:	return ("IPv6-Route Routing Header for IPv6");
		case	44:	return ("IPv6-Frag Fragment Header for IPv6");
		case	45:	return ("IDRP Inter-Domain Routing Protocol");
		case	46:	return ("RSVP Resource ReSerVation Protocol");
		case	47:	return ("GRE Generic Routing Encapsulation");
		case	48:	return ("MHRP Mobile Host Routing Protocol");
		case	49:	return ("BNA");
		case	50:	return ("IPv6-Crypt Encryption Header for IPv6");
		case	51:	return ("IPv6-Auth Authentication Header for IPv6");
		case	52:	return ("I-NLSP Integrated Net Layer Security TUBA");
		case	53:	return ("SWIPE IP with Encryption");
		case	54:	return ("NARP NBMA Address Resolution Protocol");
		case	55:	return ("MOBILE IP Mobility");
		case	56:	return ("TLSP Transport Layer Security Protocol");
		case	57:	return ("SKIP");
		case	58:	return ("IPv6-ICMP ICMP for IPv6");
		case	59:	return ("IPv6-NoNxt No Next Header for IPv6");
		case	60:	return ("IPv6-Opts Destination Options for IPv6");
		case	61:	return ("Any host internal protocol");
		case	62:	return ("CFTP");
		case	63:	return ("Any local network");
		case	64:	return ("SAT-EXPAK SATNET and Backroom EXPAK");
		case	65:	return ("KRYPTOLAN");
		case	66:	return ("RVD MIT Remote Virtual Disk Protocol");
		case	67:	return ("IPPC Internet Pluribus Packet Core");
		case	68:	return ("Any distributed file system");
		case	69:	return ("SAT-MON SATNET Monitoring");
		case	70:	return ("VISA Protocol");
		case	71:	return ("IPCV Internet Packet Core Utility");
		case	72:	return ("CPNX Computer Protocol Network Executive");
		case	73:	return ("CPHB Computer Protocol Heart Beat");
		case	74:	return ("WSN  Wang Span Network");
		case	75:	return ("PVP  Packet Video Protocol");
		case	76:	return ("BR-SAT-MON Backroom SATNET Monitoring");
		case	77:	return ("SUN-ND SUN ND PROTOCOL-Temporary");
		case	78:	return ("WB-MON WIDEBAND Monitoring");
		case	79:	return ("WB-EXPAK WIDEBAND EXPAK");
		case	80:	return ("ISO-IP ISO Internet Protocol");
		case	81:	return ("VMTP Versatile Message Transport");
		case	82:	return ("SECURE-VMTP SECURE-VMTP");
		case	83:	return ("VINES");
		case	84:	return ("TTP");
		case	85:	return ("NSFNET-IGP");
		case	86:	return ("DGP Dissimilar Gateway Protocol");
		case	87:	return ("TCF");
		case	88:	return ("EIGRP Enhanced Interior Routing Protocol (Cisco)");
		case	89:	return ("OSPFIGP Open Shortest Path First IGP");
		case	90:	return ("Sprite-RPC Sprite RPC Protocol");
		case	91:	return ("LARP Locus Address Resolution Protocol");
		case	92:	return ("MTP Multicast Transport Protocol");
		case	93:	return ("AX.25 Frames");
		case	94:	return ("IPIP Yet another IP encapsulation");
		case	95:	return ("MICP Mobile Internetworking Control Protocol");
		case	96:	return ("SCC-SP Semaphore Communications Sec. Pro.");
		case	97:	return ("ETHERIP Ethernet-within-IP Encapsulation");
		case	98:	return ("ENCAP Yet Another IP encapsulation");
		case	99:	return ("Any private encryption scheme");
		case	100:	return ("GMTP");
		case	101:	return ("IFMP Ipsilon Flow Management Protocol");
		case	102:	return ("PNNI PNNI over IP");
		case	103:	return ("PIM Protocol Independent Multicast");
		case	104:	return ("ARIS");
		case	105:	return ("SCPS");
		case	106:	return ("QNX");
		case	107:	return ("A/N Active Networks");
		case	108:	return ("IPComp IP Payload Compression Protocol");
		case	109:	return ("SNP Sitara Networks Protocol");
		case	110:	return ("Compaq-Peer Compaq Peer Protocol");
		case	111:	return ("IPX-in-IP IPX in IP");
		case	112:	return ("VRRP Virtual Router Redundancy Protocol");
		case	113:	return ("PGM Reliable Transport Protocol");
		case	114:	return ("Any 0-hop protocol");
		case	115:	return ("L2TP Layer Two Tunneling Protocol");
		case	116:	return ("DDX D-II Data Exchange");
		case	117:	return ("IATP Interactive Agent Transfer Protocol");
		case	118:	return ("STP Schedule Transfer");
		case	119:	return ("SRP SpectraLink Radio Protocol");
		case	120:	return ("UTI");
		case	121:	return ("SMP Simple Message Protocol");
		case	122:	return ("SM");
		case	123:	return ("PTP Performance Transparency Protocol");
		case	124:	return ("ISIS ISIS over IPv4");
		case	125:	return ("FIRE");
		case	126:	return ("CRTP Combat Radio Transport Protocol");
		case	127:	return ("CRUDP Combat Radio User Datagram");
		case	128:	return ("SSCOPMCE");
		case	129:	return ("IPLT");
		case	130:	return ("SPS Secure Packet Shield");
		case	131:	return ("PIPE Private IP Encapsulation within IP");
		case	132:	return ("SCTP Stream Control Transmission Protocol");
		case	133:	return ("FC Fibre Channel");
		case	134:	return ("RSVP-E2E-IGNORE");
		case	255: return ("Reserved");

		default:
			sprintf (tmp, "unknown: %d", protocol);
			return (&tmp[0]);
	}
}
/*****************************************************************************/
static void ipAnalyse(const byte* p, int len, char* line) {
	int hlen = (*p & 0x0f) * 4;

	if (((*p & 0xf0) >> 4) != 4) {
		sprintf (line, "\n  wrong protocol: %d", (*p & 0xf0) >> 4);
		return;
	}
	strcat (line, "\n   IP");

	if (hlen < 20) {
		sprintf (&line[strlen(line)], " error, too small header: %d", hlen);
		return;
	}
	if (len < hlen) {
		strcat (line, " truncated");
		return;
	}

	p++;

	{
		byte tos;
		word tot_len;
		word id;
		word offset;
		byte ttl;
		byte protocol;
		word csum;

		tos       = *p++;
		tot_len   = ntohs(READ_WORD(p)); p += 2;
		id        = ntohs(READ_WORD(p)); p += 2;
		offset    = ntohs(READ_WORD(p)); p += 2;
		ttl       = *p++;
		protocol  = *p++;
		csum      = ntohs(READ_WORD(p)); p += 2;

		if (protocol == 17 && len >= 24) {
			sprintf (&line[strlen(line)],
 " tos:%d tot_len:%d id:%d offset:%d ttl:%d\n   protocol:'%s' csum:%04x\n   %d.%d.%d.%d:%d > %d.%d.%d.%d:%d",
						 tos, tot_len, id, offset, ttl, ip_protocol_name(protocol), csum,
							p[0],   p[1],   p[2],   p[3],   ntohs(READ_WORD(&p[3+4+1])),
							p[0+4], p[1+4], p[2+4], p[3+4], ntohs(READ_WORD(&p[3+4+1+2])));
		} else {
			sprintf (&line[strlen(line)],
			" tos:%d tot_len:%d id:%d offset:%d ttl:%d\n   protocol:'%s' csum:%04x\n   %d.%d.%d.%d > %d.%d.%d.%d",
						 tos, tot_len, id, offset, ttl, ip_protocol_name(protocol), csum,
							p[0],   p[1],   p[2],   p[3],
							p[0+4], p[1+4], p[2+4], p[3+4]);
		}
	}
}
/*****************************************************************************/
void
__pppAnalyse (FILE *out, unsigned char *data, unsigned long data_len, unsigned long real_frame_length)
{
 unsigned char *buffer = data ;
 char           line[1024] ;
 long           len = data_len ;
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
 line[0] = '\0' ;


/*
 * Check for consistence with ISO 3309 extension mechanism for address field
 */
 if ((len >= 4) && ((buffer[0] ^ buffer[1]) & 1))
 {
  switch (buffer[0])
  {
  case 0x00:
   analyseProto (&buffer[1], len - 1, &line[0]) ;
   break ;
  case 0x02:
   analyseSomething (&buffer[1], len - 1, &line[0]) ;
   break ;
  case 0x80:
   analyseCntrl (&buffer[1], len - 1, &line[0]) ;
   break ;
  case 0xC0:
   switch (buffer[1])
   {
   case 0x21:
    analyseLCP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x23:
    analysePAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x25:
    analyseLQR (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x27:
    analyseSPAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x29:
    analyseCBCP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x81:
    analyse_CCP (&buffer[2], len - 2, &line[0]) ;
    break ;
   default:
    break ;
   }
   break ;
  case 0xC2:
   switch (buffer[1])
   {
   case 0x23:
    analyseCHAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x6F:
    analyseSBAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   case 0x81:
    analysePpAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   default:
    break ;
   }
   break ;
  case 0xC4:
   switch (buffer[1])
   {
   case 0x81:
    analysePNAP (&buffer[2], len - 2, &line[0]) ;
    break ;
   default:
    break ;
   }
   break ;
  default:
   break ;
  }
 } else if ((buffer[0] % 2) != 0) {
	char* output_line = &line[0];
	analyseProto (&buffer[0], len, line);
	output_line += strlen(output_line);

	switch (buffer[0]) {
		case 0x21:
			ipAnalyse(&buffer[1], real_frame_length-1, line);
			break;
	}
 }
/*
 * Output raw data
 */
 if ( line[0] )
 {
  fputs (&line[0], out) ;
 }

 {
  long i;

	fprintf (out, "\n%ld bytes", data_len) ;
  for ( i = 0 ; i < (long)real_frame_length ; i += 16 )
  {
   putAsc (out, &data[i], i, real_frame_length - i) ;
  }
 }

 fprintf (out, "\n\n") ;
}

