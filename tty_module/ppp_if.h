
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

# ifndef PPP_IF___H
# define PPP_IF___H

/*
 * PPP interface definitions
 */

#if !defined(UNIX)
# include "typedefs.h"
#endif
#if !defined (byte)
#define byte unsigned char
#endif
#if !defined (word)
#define word unsigned short
#endif
#if !defined (dword)
#define dword u32
#endif


# define PPP_PATCH_PASS_LCP		0x80	/* default is track	*/
# define PPP_PATCH_KEEP_TX_ACCM		0x40	/* default is del	*/
# define PPP_PATCH_KEEP_RX_ACCM		0x20	/* default is add	*/
# define PPP_PATCH_NUL_TX_CFG		0x08	/* force patch of IPCP	*/
# define PPP_PATCH_PASS_IPCP		0x04	/* default is track	*/

# define PPP_HDLC_TAG	0x03ff
# define PPP_LCP_TAG	0x21c003ff
# define PPP_IPCP_TAG_4	0x218003ff
# define PPP_IPCP_TAG_2	0x2180

/* static parameter structure for the PPP stream conversion functions	*/

typedef struct PPP_STREAM {
	byte	State;		/* current state			*/
				/* Explode states			*/
# define	EXP_START	  0	/* start of frame		*/
# define	EXP_AGAIN	  1	/* stream full (frame)		*/
# define	EXP_CHECK	  2	/* stream full (checksum)	*/
				/* Implode states			*/
# define	IMP_START	  0	/* start of frame		*/
# define	IMP_MORE	  1	/* need more from stream	*/
# define	IMP_ESCAPE	  2	/* need more, last byte was ESC	*/
# define	IMP_SCAN	  3	/* scan for next frame		*/	
	byte	Check;		/* do checksum for imploded frame	*/
	byte	Checksum[2];	/* saved frame check sequence		*/
	byte	*baseFrame;	/* base of frame buffer			*/
	byte	*Frame;		/* current position in frame		*/
	word	maxFrame;	/* size of frame buffer (unused)	*/
	word	sizeFrame;	/* remaining/total bytes (EXP/IMP)	*/
	byte	*baseStream;	/* base of stream buffer		*/
	byte	*Stream;	/* current position in stream		*/
	word	maxStream;	/* size of stream buffer (unused)	*/
	word	sizeStream;	/* remaining/total size (IMP/EXP)	*/
} PPP_STREAM;

/*
 * internal data structures which must be allocated by other components
 */

# define LCP_FRAME_MAX	96

typedef struct LCP_TRACK {
	byte	Flags;		/* track control flags			*/
# define	LCP_FLAG_ADD_ACCM    0x01 /* add ACCM to config req	*/
# define	LCP_FLAG_DEL_ACCM    0x02 /* del ACCM from config req	*/
# define	IPCP_FLAG_NUL_CFG    0x10 /* del all from 1st cfg req	*/
	byte	caughtReq;	/* caught a configure REQ		*/
	byte	caughtRej;	/* caught some NAK or REJ info		*/
	byte	Req[LCP_FRAME_MAX]; /* saved original configure REQ	*/
	dword	SavACCM;	/* saved ACCM from config REQ or NAK	*/ 
	dword	AckACCM;	/* ACCM from config ACK			*/
	dword	DefACCM;	/* default ACCM				*/ 
} LCP_TRACK;

typedef struct PPP_CONFIG {

	LCP_TRACK TxTrack;	/* used to track xmit configuration	*/
	LCP_TRACK RxTrack;	/* used to track recv configuration	*/

	byte	ModCfg[LCP_FRAME_MAX]; /* modified LCP config frame	*/

} PPP_CONFIG;

# define PPP_TX_ACCM(_ppp_) (((PPP_CONFIG *)_ppp_)->TxTrack.AckACCM)
# define PPP_RX_ACCM(_ppp_) (((PPP_CONFIG *)_ppp_)->RxTrack.AckACCM)

int	pppSyncLcpFrame  (byte *Data, word sizeData);
int	pppAsyncLcpFrame (byte *Data, word sizeData);
word	pppImplode	 (PPP_STREAM *S, dword ACCM);
word	pppExplode	 (PPP_STREAM *S, dword ACCM);
void	pppInit		 (PPP_CONFIG *ppp, dword RxACCM, int RxFlags,
					   dword TxACCM, int TxFlags);
word	pppTrackLcp	 (PPP_CONFIG *ppp,
			  byte **Data, word sizeData, byte What);
word	pppTrackIpcp	 (PPP_CONFIG *ppp,
			  byte **Data, word sizeData, byte What);

# endif /* PPP_IF___H */
