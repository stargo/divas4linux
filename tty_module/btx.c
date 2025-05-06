
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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

#if defined(UNIX)
# include "platform.h"
#else
# include "typedefs.h"
#endif
# include "port.h"

# include "btx_if.h"
# include "debuglib.h"

/* Attention: headers & tags are defined in little endian order */

# define BTX_HDR_SCREEN		0x0001		/* 0100		*/
# define BTX_HDR_TD_MORE	0x0104		/* 0401		*/
# define BTX_HDR_TD_LAST	0x0004		/* 0400		*/

# define BTX_TAG_END_PAGE	0x1141421f	/* 1f424111	*/

# define BTX_STATE_SCREEN	0	/* screen data		*/
# define BTX_STATE_TD_IN_PUSH	1	/* save xparent data	*/
# define BTX_STATE_TD_IN_FLUSH	2	/* flush xparent data	*/
# define BTX_STATE_TD_IN_POP	3	/* submit xparent data	*/

# define ASC_STX	0x02
# define ASC_ETX	0x03
# define ASC_EOT	0x04
# define ASC_ENQ	0x05
# define ASC_DLE	0x10
# define ASC_NAK	0x15
# define ASC_ETB	0x17

static byte EOT_DLE_ENQ [] = { ASC_EOT, ASC_DLE, ASC_ENQ };
static byte DLE_EOT	[] = { ASC_DLE, ASC_EOT };

/* fast frame check sequence stolen from RFC 1662 */

static word crctab[256] = {
	0x0000,	0xc0c1,	0xc181,	0x0140,	0xc301,	0x03c0,	0x0280,	0xc241,
	0xc601,	0x06c0,	0x0780,	0xc741,	0x0500,	0xc5c1,	0xc481,	0x0440,
	0xcc01,	0x0cc0,	0x0d80,	0xcd41,	0x0f00,	0xcfc1,	0xce81,	0x0e40,
	0x0a00,	0xcac1,	0xcb81,	0x0b40,	0xc901,	0x09c0,	0x0880,	0xc841,
	0xd801,	0x18c0,	0x1980,	0xd941,	0x1b00,	0xdbc1,	0xda81,	0x1a40,
	0x1e00,	0xdec1,	0xdf81,	0x1f40,	0xdd01,	0x1dc0,	0x1c80,	0xdc41,
	0x1400,	0xd4c1,	0xd581,	0x1540,	0xd701,	0x17c0,	0x1680,	0xd641,
	0xd201,	0x12c0,	0x1380,	0xd341,	0x1100,	0xd1c1,	0xd081,	0x1040,
	0xf001,	0x30c0,	0x3180,	0xf141,	0x3300,	0xf3c1,	0xf281,	0x3240,
	0x3600,	0xf6c1,	0xf781,	0x3740,	0xf501,	0x35c0,	0x3480,	0xf441,
	0x3c00,	0xfcc1,	0xfd81,	0x3d40,	0xff01,	0x3fc0,	0x3e80,	0xfe41,
	0xfa01,	0x3ac0,	0x3b80,	0xfb41,	0x3900,	0xf9c1,	0xf881,	0x3840,
	0x2800,	0xe8c1,	0xe981,	0x2940,	0xeb01,	0x2bc0,	0x2a80,	0xea41,
	0xee01,	0x2ec0,	0x2f80,	0xef41,	0x2d00,	0xedc1,	0xec81,	0x2c40,
	0xe401,	0x24c0,	0x2580,	0xe541,	0x2700,	0xe7c1,	0xe681,	0x2640,
	0x2200,	0xe2c1,	0xe381,	0x2340,	0xe101,	0x21c0,	0x2080,	0xe041,
	0xa001,	0x60c0,	0x6180,	0xa141,	0x6300,	0xa3c1,	0xa281,	0x6240,
	0x6600,	0xa6c1,	0xa781,	0x6740,	0xa501,	0x65c0,	0x6480,	0xa441,
	0x6c00,	0xacc1,	0xad81,	0x6d40,	0xaf01,	0x6fc0,	0x6e80,	0xae41,
	0xaa01,	0x6ac0,	0x6b80,	0xab41,	0x6900,	0xa9c1,	0xa881,	0x6840,
	0x7800,	0xb8c1,	0xb981,	0x7940,	0xbb01,	0x7bc0,	0x7a80,	0xba41,
	0xbe01,	0x7ec0,	0x7f80,	0xbf41,	0x7d00,	0xbdc1,	0xbc81,	0x7c40,
	0xb401,	0x74c0,	0x7580,	0xb541,	0x7700,	0xb7c1,	0xb681,	0x7640,
	0x7200,	0xb2c1,	0xb381,	0x7340,	0xb101,	0x71c0,	0x7080,	0xb041,
	0x5000,	0x90c1,	0x9181,	0x5140,	0x9301,	0x53c0,	0x5280,	0x9241,
	0x9601,	0x56c0,	0x5780,	0x9741,	0x5500,	0x95c1,	0x9481,	0x5440,
	0x9c01,	0x5cc0,	0x5d80,	0x9d41,	0x5f00,	0x9fc1,	0x9e81,	0x5e40,
	0x5a00,	0x9ac1,	0x9b81,	0x5b40,	0x9901,	0x59c0,	0x5880,	0x9841,
	0x8801,	0x48c0,	0x4980,	0x8941,	0x4b00,	0x8bc1,	0x8a81,	0x4a40,
	0x4e00,	0x8ec1,	0x8f81,	0x4f40,	0x8d01,	0x4dc0,	0x4c80,	0x8c41,
	0x4400,	0x84c1,	0x8581,	0x4540,	0x8701,	0x47c0,	0x4680,	0x8641,
	0x8201,	0x42c0,	0x4380,	0x8341,	0x4100,	0x81c1,	0x8081,	0x4040
};

static word btxCRC16 (register word crc, register byte *cp, register word len) {
       while (len--) crc = (crc >> 8) ^ crctab[(crc ^ *cp++) & 0xff];
       return (crc);
}

word btxRecv (
	ISDN_PORT *P, byte **Data, word sizeData, byte *Stream, word sizeStream)
{
/* Called from portRecv() to do the necessary frame conversions.	*/
/*									*/
/* Stream is a caller supplied stack buffer which may be used for frame	*/
/* conversions.								*/
/*									*/
/* If we do a frame conversion we change '*Data' to point to 'Stream',	*/
/* thus the conversion is transparent for the caller.			*/

	word Hdr;

	if (sizeData <= 2) {
		DBG_FTL(("[%p:%s] <<recv>> btx, no data", P->Channel, P->Name))
		return (0);
	}


	Hdr = *(( word *) *Data);

	sizeData -= 2;
	*Data	 += 2;

	switch (Hdr) {
	case BTX_HDR_SCREEN:
	case BTX_HDR_TD_MORE:
	case BTX_HDR_TD_LAST:
		break;
	default:
		DBG_FTL(("[%p:%s] <<recv>> btx, bad hdr %x", P->Channel, P->Name, Hdr));
		break;
	}

	switch (P->port_u.Btx.State) {

	case BTX_STATE_SCREEN	:

		switch (Hdr) {

		case BTX_HDR_SCREEN:
		default:

			goto pass;	/* pass data unchanged */

		case BTX_HDR_TD_MORE:
		case BTX_HDR_TD_LAST:

			P->port_u.Btx.State = BTX_STATE_TD_IN_PUSH;
			P->port_u.Btx.sizeHold =
			P->port_u.Btx.sizeData =
			P->port_u.Btx.nextData = 0;
		}

		/* fall trough to save transparent data */

	case BTX_STATE_TD_IN_PUSH:

		switch (Hdr) {

		case BTX_HDR_TD_LAST:
		case BTX_HDR_TD_MORE:

			if (P->port_u.Btx.sizeData+sizeData > P->port_u.Btx.sizePage) {
				DBG_FTL(("[%p:%s] <<recv>> btx, page overrun", P->Channel, P->Name));
				P->port_u.Btx.State = (Hdr == BTX_HDR_TD_MORE) ?
						  BTX_STATE_TD_IN_FLUSH	  :
						  BTX_STATE_SCREEN;
			} else {
				mem_cpy (P->port_u.Btx.Page + P->port_u.Btx.sizeData,
					*Data, sizeData);
				P->port_u.Btx.sizeData += sizeData;
				if (Hdr == BTX_HDR_TD_LAST) {
					/* invite to receive first block */
					P->port_u.Btx.State = BTX_STATE_TD_IN_POP;
					P->port_u.Btx.nextAck = 0;
					portRxEnqueue (P, EOT_DLE_ENQ,
						       sizeof (EOT_DLE_ENQ), 0);
				}
			}
			return (0);

		case BTX_HDR_SCREEN:
		default:

			DBG_FTL(("[%p:%s] <<recv> btx, unexp. (push)", P->Channel, P->Name));
			P->port_u.Btx.State = BTX_STATE_SCREEN;
			goto pass;
		}

	case BTX_STATE_TD_IN_POP:

		switch (Hdr) {

		case BTX_HDR_SCREEN:
			if (P->port_u.Btx.sizeHold + sizeData >
			    sizeof (P->port_u.Btx.Hold)	  ) {
				DBG_FTL(("[%p:%s] <<recv>> btx, hold overrun", P->Channel, P->Name));
			} else {
				mem_cpy (P->port_u.Btx.Hold + P->port_u.Btx.sizeHold,
					*Data, sizeData);
				P->port_u.Btx.sizeHold += sizeData;
			}
			break;

		case BTX_HDR_TD_MORE:
		case BTX_HDR_TD_LAST:
		default:
			DBG_FTL(("[%p:%s] <<recv>> btx - unexp. (pop)", P->Channel, P->Name));
		}

		return (0);

	case BTX_STATE_TD_IN_FLUSH:

		switch (Hdr) {

		case BTX_HDR_SCREEN:
		default:
			P->port_u.Btx.State = BTX_STATE_SCREEN;
			goto pass;
		case BTX_HDR_TD_LAST:
			P->port_u.Btx.State = BTX_STATE_SCREEN;
		case BTX_HDR_TD_MORE:
			break;
		}

		return (0);
	}

pass:
	if (sizeData >= sizeof(BTX_TAG_END_PAGE) &&
	    *(( dword *) (*Data + sizeData - sizeof(BTX_TAG_END_PAGE)))
	    == BTX_TAG_END_PAGE) {
	    	mem_cpy (Stream, *Data, sizeData);
	    	Stream[sizeData] = ASC_EOT;
		*Data = Stream;
		sizeData++;
	}

	return (sizeData);
}

word btxWrite (
	ISDN_PORT *P, byte **Data, word sizeData, byte *Frame, word sizeFrame)
{
/* Called from PortWrite() to do the necessary frame conversions.	*/
/*									*/
/* Frame is a caller supplied stack buffer which may be used for frame	*/
/* conversions.								*/
/*									*/
/* If we do a frame conversion we change '*Data' to point to 'Stream',	*/
/* thus the conversion is transparent for the caller.			*/

	byte	*This, *Last, *baseFrame, savCC, endCC;
	word	sizeChunk, Checksum;

	This = *Data;

	switch (P->port_u.Btx.State) {

	case BTX_STATE_SCREEN:

		/* The maximum HDLC framesize is 132 bytes, 6 bytes are	*/
		/* wasted for headers (X75,T70,BTX); thus we must split	*/
		/* all data in 126 byte chunks (normal BTX apps never	*/
		/* send more than 126 byte at once, but if BTX is used	*/
		/* via RNA as an internet service provider we get more).*/

		*(( word *) Frame) = BTX_HDR_SCREEN;

		while (sizeData)
		{
			sizeChunk = (sizeData < 126) ? sizeData : 126;
			mem_cpy (Frame + 2, This, sizeChunk);


			if (!portXmit (P, Frame, (word)(2+sizeChunk), N_DATA))
				break;

			This	 += sizeChunk;
			sizeData -= sizeChunk;
		}
		break ;

	case BTX_STATE_TD_IN_PUSH:
	case BTX_STATE_TD_IN_FLUSH:

		DBG_FTL(("[%p:%s] Write: btx, unexp. (push)", P->Channel, P->Name));
		return (0);

	case BTX_STATE_TD_IN_POP:

		if (!(P->port_u.Btx.nextAck & 1)) {
			if (*This != ASC_DLE) goto badack;
			P->port_u.Btx.nextAck++;
			if (!--sizeData) return (0);	/* wait for 0 || 1 */
			This++;
		}

		if (*This != '0' + ((P->port_u.Btx.nextAck >> 1) & 1) || --sizeData){
badack:
			DBG_FTL(("[%p:%s] Write: btx, bad ack (pop)", P->Channel, P->Name));
			portRxEnqueue (P, DLE_EOT, sizeof (DLE_EOT), 0);
			return (0);
		}

		P->port_u.Btx.nextAck++;

		/* get size of next chunk to present */

		sizeChunk = ((P->port_u.Btx.sizeData - P->port_u.Btx.nextData) > 256) ?
			    256 : (P->port_u.Btx.sizeData - P->port_u.Btx.nextData);

		/* send final EOT if this transparent block finished */

		if (!sizeChunk) {
			P->port_u.Btx.State = BTX_STATE_SCREEN;
			portRxEnqueue (P, EOT_DLE_ENQ, 1, 0);
			if (P->port_u.Btx.sizeHold) {
				portRxEnqueue (P, P->port_u.Btx.Hold,
						  P->port_u.Btx.sizeHold, 0);
			}
			return (0);
		}

		/* present the next chunk */

		This = P->port_u.Btx.Page + P->port_u.Btx.nextData;
		P->port_u.Btx.nextData += sizeChunk;
		Last = P->port_u.Btx.Page + P->port_u.Btx.nextData;

		if (P->port_u.Btx.nextData >= P->port_u.Btx.sizeData) {
			P->port_u.Btx.nextData = P->port_u.Btx.sizeData = 0;
			endCC = ASC_ETX;
		} else {
			endCC = ASC_ETB;
		}

		/* save byte past current chunk, append ETB/ETX	to cur-	*/
		/* chunk, compute checksum (including ETB/ETX), restore	*/
		/* saved byte and construct the frame			*/

		savCC = *Last;
		*Last = endCC;
		Checksum = btxCRC16 (0, This, (word) (sizeChunk + 1));
		*Last = savCC;

		baseFrame = Frame;

		*Frame++ = ASC_DLE; *Frame++ = ASC_STX;

		while (This < Last) {
			if ((*Frame++ = *This++) == ASC_DLE) *Frame++ = ASC_DLE;
		}

		*Frame++ = ASC_DLE; *Frame++ = endCC;
		*Frame++ = (byte) Checksum; *Frame++ = (byte) (Checksum >> 8);

		portRxEnqueue (P, baseFrame, (word) (Frame - baseFrame), 0);
	}

	return (0);
}

int btxUp (ISDN_PORT *P)
{
/* Called by portUp(), i.e. an inbound or outbound call was success-	*/
/* fully established and the B-channel protocol connection is up.	*/
/* P->Mode  == P_MODE_BTX						*/
/* P->State == P_DATA							*/

	P->port_u.Btx.State = BTX_STATE_SCREEN;
	P->port_u.Btx.sizePage = BTX_MAX_PAGE;
	/* generate response */
	return (1);
}
