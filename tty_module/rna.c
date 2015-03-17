
/*
 *
  Copyright (c) Dialogic(R), 2009.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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

#if defined(UNIX)
# include "platform.h" 
#else
# include "typedefs.h"
#endif
# include "port.h"

# include "rna_if.h"
# include "ppp_if.h"
# include "debuglib.h"

/*
 * the old RAS magics
 */
# define SYN		0x16
# define SOH_BCAST	0x01
# define SOH_DEST	0x02
# define SOH_TYPE	0x80
# define SOH_COMPRESS	0x40
# define SOH_XONXOF	0x20
# define ETX		0x03

/*
 * The old RAS checksum uses the CCITT polynomial x**0 + x**5 + x**12 + x**16.
 * This is the same polynomial as it is used for the PPP checksum but the PPP
 * checksum is generated starting whith a crc value of 0xffff and the least
 * significant bit feeded first whilst the old RAS checksum starts whith a
 * crc value of 0 and feeds the most significant bit first.
 */

static word rasCRC (word crc, unsigned char *block, word size)
{
static word crctab[256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
        };
	while ( size-- )
		crc = (crc << 8) ^ crctab[ (crc >> 8) ^ *block++ ];
	return crc;
}

static int rasAsyncFrame (byte *Frame, word sizeFrame)
{
	return ((sizeFrame >= 10)					&&
		(Frame[0] == SYN)					&&
		(Frame[1] == SOH_BCAST || Frame[1] == SOH_DEST)		&&
		(((Frame[2] << 8) + Frame[3]) == (sizeFrame - 7))	&&
		(Frame[4] == 0xf0)					&&
		((Frame[5] == 0xf0) || (Frame[5] == 0xf1))		&&
		(Frame[sizeFrame - 3] == ETX)				 );
}

static int rasSyncFrame (byte *Frame, word sizeFrame)
{
	return ((sizeFrame >= 3)			&&
	        (Frame[0] == 0xf0)			&&
		(Frame[1] == 0xf0 || Frame[1] == 0xf1)	 );
}

word rnaRecv (
	ISDN_PORT *P, byte **Data, word sizeData, byte *Stream, word sizeStream)
{
/* Called from portRecv() to do the necessary frame conversions.	*/
/*									*/
/* We need a caller supplied buffer for frame conversion to permit	*/
/* the use of a stack buffer for this purpose.				*/
/*									*/
/* If we do a frame conversion we change '*Data' to point to 'Stream',	*/
/* thus the conversion is transparent for the caller.			*/
/*									*/
/* The stream buffer must be sufficient for a complete exploded frame.	*/

	byte *Frame; dword ACCM;

	Frame = *Data;

	switch (P->port_u.Rna.Framing) {

	case P_FRAME_DET: /* check for framing in the beginning	*/

		if (pppSyncLcpFrame (Frame, sizeData)) {
			DBG_CONN(("[%p:%s] <<recv>> PPP_SYNC", P->Channel, P->Name));
			P->port_u.Rna.Framing = P_FRAME_SYNC;
			goto pppRecvSync;
		}
		if (rasSyncFrame (Frame, sizeData)) {
			DBG_CONN(("[%p:%s] <<recv>> RAS_SYNC", P->Channel, P->Name));
			P->port_u.Rna.Framing = P_FRAME_RAS_SYNC;
			goto rasRecvSync;
		}

		if (pppAsyncLcpFrame (Frame, sizeData)) {
			DBG_CONN(("[%p:%s] <<recv>> PPP_ASYNC", P->Channel, P->Name));
			P->port_u.Rna.Framing = P_FRAME_ASYNC;
			break;
		}
		if (rasAsyncFrame (Frame, sizeData)) {
			DBG_CONN(("[%p:%s] <<recv>> RAS_ASYNC", P->Channel, P->Name));
			P->port_u.Rna.Framing = P_FRAME_RAS_ASYNC;
			break;
		}

 		DBG_CONN(("[%p:%s] <<recv>> unknown framing", P->Channel, P->Name));
		P->port_u.Rna.Framing = P_FRAME_NONE;

		break;

	case P_FRAME_SYNC:	/* explode to async frame */
pppRecvSync:
		/* For normal traffic we must use the negotiated ACCM	*/
		/* cause the receiver is obliged to discard all chars	*/
		/* which are flagged in the ACCM and come non escaped.	*/
		/* For Lcp packets we always use the maximum ACCM.	*/

		ACCM = PPP_RX_ACCM(&P->port_u.Rna.Ppp);

		/* Look for PPP control protocol frames, must have at	*/
		/* least 6 bytes for 'protocol(2) + type(1) + id(1) +	*/
		/* size (2)', 2 more bytes if HLDC header is present.	*/

		if (sizeData >= 6) {
			byte *Prot = Frame;
			if (*(( word *) Prot) == PPP_HDLC_TAG)
				Prot += 2;
		     	if ((*Prot >= 0x80) &&
			    (*Prot == 0x80 || *Prot == 0xC0 || *Prot == 0xC2)) {
				goto PppControl;
			}
		}

		/* not a PPP control protocol frame, pass on */

		goto explode;

PppControl:	/* PPP control protocol frame, check for patches */

		if (*((dword *) Frame) == PPP_LCP_TAG) {
			ACCM = 0xffffffffL;
			if (P->port_u.Rna.Patches & PPP_PATCH_PASS_LCP) {
				/* don't touch frame */
				goto explode;
			}
			if (!(sizeData = pppTrackLcp (&P->port_u.Rna.Ppp,
						      &Frame, sizeData, '<'))){
				/* frame was caught */
		    		break;
			}
			goto explode;
		}

		if (P->port_u.Rna.Patches & PPP_PATCH_PASS_IPCP) {
			/* no IPCP interventions */
			goto explode;
		}

		if (*(( dword *) Frame) == PPP_IPCP_TAG_4 ||
		    *(( word *)  Frame) == PPP_IPCP_TAG_2   ) {
			if (!(sizeData = pppTrackIpcp (&P->port_u.Rna.Ppp,
						       &Frame, sizeData, '<'))){
				/* frame was caught */
		    		break;
			}
		}

explode:
		P->port_u.Rna.Explode.State		= EXP_START;
		P->port_u.Rna.Explode.baseFrame	=
		P->port_u.Rna.Explode.Frame		= Frame;
		P->port_u.Rna.Explode.sizeFrame	= sizeData;
		P->port_u.Rna.Explode.baseStream	=
		P->port_u.Rna.Explode.Stream		= Stream;
		P->port_u.Rna.Explode.sizeStream	= sizeStream;

		if ((sizeData = pppExplode (&P->port_u.Rna.Explode, ACCM)) &&
		    P->port_u.Rna.Explode.State == EXP_START /* complete */  ) {
		    	*Data = Stream;
		} else {
			sizeData = 0;
			DBG_FTL(("[%p:%s] <<recv>> PPP_SYNC - cnv err", P->Channel, P->Name));
		}

		break;

	case P_FRAME_RAS_SYNC:
rasRecvSync:
		if (rasSyncFrame (*Data, sizeData) &&
		    ((sizeData + 7) <= sizeStream)  ) {
			word crc;
			Stream[0] = SYN;
			Stream[1] = SOH_DEST;
			Stream[2] = (byte) (sizeData >> 8);
			Stream[3] = (byte) (sizeData);
			mem_cpy (&Stream[4], *Data, sizeData);
			Stream[4 + sizeData] = ETX;
			crc = rasCRC (0, &Stream[1], (word)(sizeData + 4));
			Stream[4 + sizeData + 1] = (byte) (crc);
			Stream[4 + sizeData + 2] = (byte) (crc >> 8);
			*Data	  = Stream;
			sizeData += 7;
		} else {
			DBG_FTL(("[%p:%s] <<recv>> RAS_SYNC - bad frame", P->Channel, P->Name));
		}

		break;

	case P_FRAME_ASYNC:
	case P_FRAME_RAS_ASYNC:
	case P_FRAME_NONE:
	default:
		/* don't touch */
		break;
	}

	return (sizeData);
}

word rnaWrite (
	ISDN_PORT *P, byte **Data, word sizeData, byte *Frame, word sizeFrame)
{
/* Called from PortWrite() to do the necessary frame conversions.	*/
/*									*/
/* We need a caller supplied buffer for frame conversion to permit	*/
/* the use of a stack buffer for this purpose.				*/
/*									*/
/* If we do a frame conversion we change '*Data' to point to 'Frame',	*/
/* thus the conversion is transparent for the caller.			*/
/*									*/
/* The frame buffer must be sufficient for a complete imploded frame.	*/
/*									*/
/* Currently we assume that we get always complete frames from RNA.	*/
/* If this assumption is wrong we must change this function a little	*/
/* bit to deal whith streaming (pppExplode() is prepared for it).	*/

	switch (P->port_u.Rna.Framing) {

	case P_FRAME_DET: /* begin sync if async, adapt to incoming framing */

		if (pppAsyncLcpFrame (*Data, sizeData)) goto pppWriteSync;
		if (rasAsyncFrame (*Data, sizeData))	goto rasWriteSync;

		break;

	case P_FRAME_SYNC:	/* convert to sync  */
pppWriteSync:
		P->port_u.Rna.Implode.State		= IMP_START;
		P->port_u.Rna.Implode.Check		= 0;
		P->port_u.Rna.Implode.baseStream	=
		P->port_u.Rna.Implode.Stream		= *Data;
		P->port_u.Rna.Implode.sizeStream	= sizeData;
		P->port_u.Rna.Implode.baseFrame	=
		P->port_u.Rna.Implode.Frame		= Frame;
		P->port_u.Rna.Implode.sizeFrame	= sizeFrame;

		/* We assume that frames from RNA will never contain	*/
		/* spurious unescaped control characters and thus we	*/
		/* always use the minimum ACCM (i.e. 0) here.		*/

		if ((sizeData = pppImplode (&P->port_u.Rna.Implode, 0L)) &&
		    P->port_u.Rna.Implode.State == IMP_START		      ) {
			*Data = Frame;
		} else {
			/* incomplete frame or frame too large */
			DBG_FTL(("[%p:%s] Write: PPP_SYNC - cnv err", P->Channel, P->Name));
			sizeData = 0;
			break;
		}

		/* Look for PPP control protocol frames, must have at	*/
		/* least 6 bytes for 'protocol(2) + type(1) + id(1) +	*/
		/* size (2)', 2 more bytes if HLDC header is present.	*/
		/* Excuse me for the GoTo's, I want to be fast here.	*/

		if (sizeData >= 6) {
			byte *Prot = Frame;
			if (*(( word *) Prot) == PPP_HDLC_TAG)
				Prot += 2;
		     	if ((*Prot >= 0x80) &&
			    (*Prot == 0x80 || *Prot == 0xC0 || *Prot == 0xC2)) {
				goto PppControl;
			}
		}

		/* not a PPP control protocol frame, pass on */

		break;

PppControl:	/* a PPP control protocol frame, check for patches */

		if (*(( dword *) Frame) == PPP_LCP_TAG) {
			if (P->port_u.Rna.Patches & PPP_PATCH_PASS_LCP) {
				/* don't touch frame */
				break;
			}
			sizeData = pppTrackLcp (&P->port_u.Rna.Ppp,
						Data, sizeData, '>');
			break;
		}

		if (P->port_u.Rna.Patches & PPP_PATCH_PASS_IPCP) {
			/* no IPCP interventions */
			break;
		}

		if (*(( dword *) Frame) == PPP_IPCP_TAG_4 ||
		    *(( word *)  Frame) == PPP_IPCP_TAG_2   ) {
			sizeData = pppTrackIpcp (&P->port_u.Rna.Ppp,
						 Data, sizeData, '>');
			break;
		}

		break;

	case P_FRAME_RAS_SYNC:
rasWriteSync:
		if (rasAsyncFrame (*Data, sizeData)) {
			*Data    += 4;
			sizeData -= 7;
		} else {
			DBG_FTL(("[%p:%s] Write: RAS_SYNC - bad frame", P->Channel, P->Name));
		}

		break;

	case P_FRAME_ASYNC:
	case P_FRAME_RAS_ASYNC:
	case P_FRAME_NONE:
	default:
		/* don't touch */
		break;
	}

	return (sizeData);
}

int rnaUp (ISDN_PORT *P)
{
/* Called by portUp(), i.e. an inbound or outbound call was success-	*/
/* fully established and the B-channel protocol is up.			*/
/* P->Mode  == P_MODE_BTX						*/
/* P->State == P_DATA							*/
/*									*/
/* Now we can initialize the async/sync conversion function.		*/
/*									*/
/* In the beginning we must escape every control character when we	*/
/* explode a sync frame cause this is the initial state for PPP over	*/
/* asyncronous lines (expected by RNA)					*/
/* Thus 0xffffffff is set as the initial receive ACCM.			*/
/*									*/
/* To prevent the overhead of escaping every control character if the	*/
/* peer doesn't sent an ACCM whit it's configure request we add a null 	*/
/* ACCM to such a request by default.					*/
/*									*/
/* For asyncronous frames from RNA we can assume that such frames will	*/
/* never contain spurious unescaped control charecters.			*/
/* Thus 0x00000000 is set as the initial transmit ACCM.			*/
/*									*/
/* Cause some routers (for example Cisco and NetGW) reject an ACCM on	*/
/* syncronous links we remove (but remember) the ACCM from outgoing	*/
/* configure requests by default.					*/

 	pppInit (&P->port_u.Rna.Ppp,
		 0xffffffffL /*RX_ACCM*/,
		 (P->port_u.Rna.Patches & PPP_PATCH_KEEP_RX_ACCM) ?
			0 : LCP_FLAG_ADD_ACCM,
	         0x00000000L /*TX_ACCM*/,
		 ((P->port_u.Rna.Patches & PPP_PATCH_KEEP_TX_ACCM)?
			0 : LCP_FLAG_DEL_ACCM		       ) |
		 ((P->port_u.Rna.Patches & PPP_PATCH_NUL_TX_CFG)  ?
			IPCP_FLAG_NUL_CFG : 0		       )  );
	/* generate response */
	return (1);
}
