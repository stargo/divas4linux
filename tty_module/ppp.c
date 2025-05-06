
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

# include "sys_if.h"
# include "ppp_if.h"
# include "debuglib.h"

/* fast frame check sequence from RFC 1662 */

static word fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

# define PPPINITFCS16 0xffff
# define PPPGOODFCS16 0xf0b8

static word pppfcs16(word fcs, byte *cp, word len)
{
	while (len--) fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
	return (fcs);
}

# pragma pack(1)

typedef struct CFG_OPT {	/* configuration option element		*/
	byte	Option;		/* option type				*/
	byte	Length;		/* of whole option element		*/
#if defined(UNIX)
	byte	Data[1];	/* arbitrary length info		*/
#else
	byte	Data[0];	/* arbitrary length info		*/
#endif
} CFG_OPT;

# define	LCP_OPT_ACCM	  2
# define	LCP_OPT_ACCM_LEN  6	/* 2 + sizeof (dword)		*/
# define	IPCP_OPT_ADDRS	  1	/* obsoleted by OPT_ADDR	*/
# define	IPCP_OPT_ADDR	  3
# define	IPCP_OPT_ADDR_LEN 6	/* 2 + sizeof (dword)		*/

typedef struct CFG_HDR {	/* LCP/IPCP configuration frame		*/
	/* Address & Control may be compressed (omitted) in IPCP frames	*/
	byte	Address;	/* 0xff					*/
	byte	Control;	/* 0x03					*/
	/* Protocol is always noncompressed in control protocol frames	*/
	byte	Protocol[2];	/* 0xc021 (LCP) / 0x8021 (IPCP)		*/
# define	MAX_CFG_HDR_LEN	4 
	byte	Type;
# define	CFG_REQ		1       /* Configure-Request		*/
# define	CFG_ACK		2	/* Configure-Ack		*/
# define	CFG_NAK		3	/* Configure-Nak		*/
# define	CFG_REJ		4	/* Configure-Reject		*/
# define	CFG_TRM_REQ	5	/* Terminate-Request		*/
# define	CFG_TRM_ACK	6	/* Terminate-Ack		*/
# define	CFG_CODE_REJ	7	/* Code-Reject			*/
# define	CFG_PROT_REJ	8	/* Protocol-Reject (LCP only)	*/
# define	CFG_ECHO_REQ	9	/* Echo-Request (LCP only)	*/
# define	CFG_ECHO_REP	10	/* Echo-Reply (LCP only)	*/
# define	CFG_DISCARD_REQ	11	/* Discard-Request (LCP only)	*/
# define	CFG_IDENT	12	/* Identification (LCP only)	*/
# define	CFG_TIME_REM	13	/* Time-Remaining (LCP only)	*/
# define	CFG_RESET_REQ	14	/* Reset-Request (CCP only)	*/
# define	CFG_RESET_REP	15	/* Reset-Reply (CCP only)	*/
# define	IPCP_CFG_MAX	7	/* max IPCP type (Code-Reject)	*/
# define	LCP_CFG_MAX	13	/* max LCP type (Time-Remaining)*/
	byte	Identifier;	/* unique request number		*/
	byte	Length[2];	/* of whole packet - MAX_CFG_HDR_LEN	*/
# define	MIN_CFG_OPT_LEN	4	/* i.e. no byte of options	*/
#if defined(UNIX)
	byte	Options[1];	/* arbitrary no of options		*/
#else
	byte	Options[0];	/* arbitrary no of options		*/
#endif
} CFG_HDR;

# pragma pack()

/* the usual byte sex problems */

#ifdef ntohs
#undef ntohs
#endif
# define ntohs(p)	( (word) ((((byte *)(p))[0] << 8) + ((byte *)(p))[1])  )

#ifdef ntohl
#undef ntohl
#endif
# define ntohl(p)	( (dword)((ntohs((p)) << 16) + ntohs((byte *)(p) + 2)) )

#ifdef htons
#undef htons
#endif
# define htons(p,w)	{ ((byte *)(p))[0] = (byte)((w) >> 8);	\
			  ((byte *)(p))[1] = (byte)(w);		}

#ifdef htonl
#undef htonl
#endif
# define htonl(p,d)	{ htons((p), (word)((d) >> 16));	\
			  htons((byte *)(p) + 2, (word)(d));	}

int pppSyncLcpFrame (byte *Data, word sizeData)
{
/* check if data is a LCP frame in sync framing */

	return ((sizeData >= sizeof (CFG_HDR))				&&
#if defined(UNIX)
		(*((dword *) Data) == PPP_LCP_TAG)		&&
#else
		(*((UNALIGNED dword *) Data) == PPP_LCP_TAG)		&&
#endif
	        (((CFG_HDR *) Data)->Type <= LCP_CFG_MAX)		&&
	        ((ntohs (((CFG_HDR *) Data)->Length) + MAX_CFG_HDR_LEN)
		 <= sizeData)						  );
}

int pppAsyncLcpFrame (byte *Data, word sizeData)
{
/* check if data is an LCP frame in async framing */

	static byte p1[] = { 0x7e, 0x7d, 0xdf, 0x7d, 0x23, 0xc0, 0x21 };
	static byte p2[] = { 0x7e, 0xff, 0x7d, 0x23, 0xc0, 0x21 };
	static byte p3[] = { 0x7e, 0xff, 0x03, 0xc0, 0x21 };

	/* first try the well known patterns */

	if ((sizeData > sizeof (p1) && !mem_cmp (Data, p1, sizeof (p1))) ||
	    (sizeData > sizeof (p2) && !mem_cmp (Data, p2, sizeof (p2))) ||
	    (sizeData > sizeof (p3) && !mem_cmp (Data, p3, sizeof (p3)))	 )
		return (1);	/* rather sure */

	/* and now the unsafe candidates which send only a few bytes at once */

	return ((sizeData && Data[0] == 0x7e && sizeData <= sizeof (p3)) ? -1 :
									    0 );
}

byte *pppFindOpt (byte *Opt, byte *endOpts, byte OptId)
{ /* find a certain option in the option field of a LCP or IPCP frame */

	for ( ;
	     (Opt <= endOpts - 2) &&
	     (Opt + ((CFG_OPT *) Opt)->Length <= endOpts);
	     Opt += ((CFG_OPT *) Opt)->Length) {
		if (((CFG_OPT *) Opt)->Option == OptId) return (Opt);
	}
	return (0);	/* not found */
}

void pppInit (PPP_CONFIG *ppp, dword RxACCM, int RxFlags,
			       dword TxACCM, int TxFlags)
{
/* initialize the LCP tracking control fields */

	ppp->RxTrack.Flags	 = (byte) RxFlags;
	ppp->RxTrack.caughtReq	 =
	ppp->RxTrack.caughtRej	 = 0;
	ppp->RxTrack.AckACCM     =
	ppp->RxTrack.DefACCM 	 = RxACCM;

	ppp->TxTrack.Flags	 = (byte) TxFlags;
	ppp->TxTrack.caughtReq	 =
	ppp->TxTrack.caughtRej	 = 0;
	ppp->TxTrack.AckACCM	 =
	ppp->TxTrack.DefACCM	 = TxACCM;
}

word pppTrackLcp (PPP_CONFIG *ppp, byte **Data, word sizeData, byte From)
{
/*
 * PPP over ISDN has a default ACCM of 0x00000000 and mostly doesn't negotiate
 * the ACCM.
 * PPP over asyncronous lines has a default ACCM of 0xffffffff and mostly
 * negotiates the ACCM to reduce the set of control chars to escape.
 *
 * Using an ACCM of oxffffffff to explode sync frames will cause a lot of
 * overhead and thus we have to track the ACCM negotion and to remove or to
 * add a suitable ACCM in LCP config packets.
 */
	CFG_HDR		*Cfg;
	LCP_TRACK	*Track;
	byte		*OptAccm, *endOpts, Pref, func[16], caughtReq;
	word		sizeOpts;
	dword		ACCM;

	Cfg = (CFG_HDR *) *Data;

	if (!pppSyncLcpFrame(*Data, sizeData) ||
	    Cfg->Type < CFG_REQ || Cfg->Type > CFG_REJ) {
		/* not interesting, pass as is */
	    	return (sizeData);
	}

	func[0] = From;
	str_cpy ((byte*)func + 1, (byte*)"pppLcp");
	str_cpy (func + 7, (Cfg->Type == CFG_REQ)? "REQ" :
			  (Cfg->Type == CFG_ACK)? "ACK" :
			  (Cfg->Type == CFG_NAK)? "NAK" : "REJ");

	sizeOpts = ntohs (Cfg->Length);
	endOpts	 = (byte *) Cfg + (MAX_CFG_HDR_LEN + sizeOpts);

	if ((OptAccm = pppFindOpt (Cfg->Options, endOpts, LCP_OPT_ACCM))) {
		if (((CFG_OPT *) OptAccm)->Length != LCP_OPT_ACCM_LEN)
			return (sizeData);
		ACCM = ntohl (((CFG_OPT *) OptAccm)->Data);
		DBG_NDIS(("%s: ACCM=%lx", func, ACCM));
	} else {
		ACCM = 0;
		DBG_NDIS(("%s: ACCM=none", func));
	}

	switch (Cfg->Type) {

	case CFG_REQ:

		Track = (From == '>')? &ppp->TxTrack : &ppp->RxTrack;

		Track->caughtReq = 0;

		if (OptAccm) {

			if (!(Track->Flags & LCP_FLAG_DEL_ACCM))
				break; /* not our problem */

			/* save original request, remove the ACCM from	*/
			/* request and lets hope the peer accepts it	*/

			if (sizeData > sizeof (Track->Req)) {
				DBG_NDIS(("%s: can't del ACCM", func));
				break;
			}
			mem_cpy (Track->Req, Cfg, sizeData);
			mem_cpy (ppp->ModCfg, Cfg, OptAccm - (byte *) Cfg);
			mem_cpy (ppp->ModCfg + (OptAccm - (byte *) Cfg),
				OptAccm + LCP_OPT_ACCM_LEN,
				endOpts - (OptAccm + LCP_OPT_ACCM_LEN));
			sizeOpts -= LCP_OPT_ACCM_LEN;
			htons (((CFG_HDR *) ppp->ModCfg)->Length, sizeOpts);

			*Data = ppp->ModCfg;
		  	sizeData -= LCP_OPT_ACCM_LEN;

			DBG_NDIS(("%s: ACCM=none (del)", func));

			Track->caughtReq = LCP_FLAG_DEL_ACCM;
			Track->SavACCM   = ACCM;

			break;
		}

		if (!(Track->Flags & LCP_FLAG_ADD_ACCM))
			break; /* not our problem */

		/* save original request, add a null or the previously	*/
		/* nak'ed ACCM to request (hopefuly accepted by peer)	*/

		if (sizeData > sizeof (Track->Req) - LCP_OPT_ACCM_LEN) {
			DBG_NDIS(("%s: can't add ACCM", func));
			break;
		}
		mem_cpy (Track->Req, Cfg, sizeData);
		mem_cpy (ppp->ModCfg, Cfg, sizeData);
		OptAccm = ppp->ModCfg + sizeData;
		((CFG_OPT *)OptAccm)->Option = LCP_OPT_ACCM;
		((CFG_OPT *)OptAccm)->Length = LCP_OPT_ACCM_LEN;
	        ACCM = Track->caughtRej ? Track->SavACCM : 0;
		htonl (((CFG_OPT *) OptAccm)->Data, ACCM);
		htons (((CFG_HDR *) ppp->ModCfg)->Length,
			sizeOpts + LCP_OPT_ACCM_LEN);

		*Data	  = ppp->ModCfg;	
		sizeData += LCP_OPT_ACCM_LEN;

		DBG_NDIS(("%s: ACCM=%lx (add)", func, ACCM));

		Track->caughtReq = LCP_FLAG_ADD_ACCM;

		break;

	case CFG_ACK:

		if (From == '>') {
			Track = &ppp->RxTrack; Pref = 'R';
		} else {
			Track = &ppp->TxTrack; Pref = 'T';
		}

		caughtReq = Track->caughtReq;

		Track->caughtReq = Track->caughtRej = 0;

		if (!caughtReq) {
			/* negotiated whithout us */
			if (OptAccm) {
				DBG_NDIS(("%s: %cxACCM=%lx (ack)", func,
					      Pref, ACCM));
				Track->AckACCM = ACCM;
			} else {
				DBG_NDIS(("%s: %cxACCM=%lx (def)", func,
					      Pref, Track->DefACCM));
				Track->AckACCM = Track->DefACCM;
			}
			break;
		}

		if (OptAccm) {
			/* should be the ACCM we added to the caught req */
			DBG_NDIS(("%s: %cxACCM=%lx (add)", func, Pref, ACCM));
			Track->AckACCM = ACCM;
		} else if (caughtReq == LCP_FLAG_DEL_ACCM) {
			DBG_NDIS(("%s: %cxACCM=%lx (del)", func,
				      Pref, Track->SavACCM));
			Track->AckACCM = Track->SavACCM;
		} else {
			DBG_NDIS(("%s: %cxACCM=%lx (def)", func,
				      Pref, Track->DefACCM));
			Track->AckACCM = Track->DefACCM;
		}

		/* return the saved cfg req instead of ack */

		((CFG_HDR *) Track->Req)->Type = CFG_ACK;

		sizeData = MAX_CFG_HDR_LEN +
			   ntohs (((CFG_HDR *) Track->Req)->Length);
		*Data = Track->Req;

		break;

	case CFG_NAK:

		Track = (From == '>') ? &ppp->RxTrack : &ppp->TxTrack;

		caughtReq = Track->caughtReq;

		Track->caughtReq = 0;

		if (!OptAccm || caughtReq != LCP_FLAG_ADD_ACCM)
			break; /* not our problem */

		/* see if there is more than the ACCM	*/

		if (OptAccm != Cfg->Options ||
		    endOpts != OptAccm + LCP_OPT_ACCM_LEN) {

			/* Nice, it's more and thus we can remove the	*/
			/* ACCM we added and propagate on all other	*/
			/* options. The ACCM coming whith this Nak is	*/
			/* saved and added to the next Request.		*/

			Track->SavACCM = ACCM; Track->caughtRej = 1;

			mem_cpy (ppp->ModCfg, Cfg, OptAccm - (byte *) Cfg);
			mem_cpy (ppp->ModCfg + (OptAccm - (byte *) Cfg),
				OptAccm + LCP_OPT_ACCM_LEN,
				endOpts - (OptAccm + LCP_OPT_ACCM_LEN));
			sizeOpts -= LCP_OPT_ACCM_LEN;
			htons (((CFG_HDR *) ppp->ModCfg)->Length, sizeOpts);

			*Data = ppp->ModCfg;
		  	sizeData -= LCP_OPT_ACCM_LEN;

			DBG_NDIS(("%s: change", func));

			break;
		}

		/* Shit, it's only the ACCM and we should generate a	*/
		/* new Req here from the saved Req which contains the	*/
		/* ACCM just received.					*/
		/* For now we take it easy, clear the 'ADD_ACCM' flag,	*/
		/* set the 'DEL_ACCM' flag and ignore the NAK.		*/
		/* This should result in a new Configure Request after	*/
		/* after some timeout period (hopefuly).		*/

		DBG_NDIS(("%s: discard", func));

		Track->Flags = LCP_FLAG_DEL_ACCM;

		return (0);	/* forget this NAK */

	case CFG_REJ:

		Track = (From == '>') ? &ppp->RxTrack : &ppp->TxTrack;

		caughtReq = Track->caughtReq;

		Track->caughtReq = 0;

		if (!OptAccm) break; /* not our problem */

		/* If an ACCM is rejected it should not be sent again.	*/
		/* Thus we clear the 'ADD_ACCM' flag (if any), set the	*/
		/* 'DEL_ACCM' flag and pass on the Reject.		*/
		/* This should result in a new Configure Request.	*/

		Track->Flags = LCP_FLAG_DEL_ACCM;

		break;
	}

	return (sizeData);
}

word pppTrackIpcp (PPP_CONFIG *ppp, byte **Data, word sizeData, byte From)
{
	CFG_HDR		*Cfg;
	LCP_TRACK	*Track;
	byte		*OptAddr, *endOpts, func[16], caughtReq, caughtRej;
	word		sizeOpts, Adj;
	dword		TRACK_PPP_ADDR;

	if (**Data == 0xff) {
		Cfg = (CFG_HDR *) *Data; Adj = 0;
	} else {
		Cfg = (CFG_HDR *) (*Data - 2); Adj = 2;
	}

	sizeOpts = ntohs (Cfg->Length);
	endOpts	 = (byte *) Cfg + (MAX_CFG_HDR_LEN + sizeOpts);

	if (sizeOpts < MIN_CFG_OPT_LEN || endOpts > *Data + sizeData ||
	    Cfg->Type < CFG_REQ || Cfg->Type > CFG_REJ) {
		/* not interesting, pass as is */
	    	return (sizeData);
	}

	func[0] = From;
	str_cpy ((byte*)func + 1, (byte*)"pppIpcp");
	str_cpy ((byte*)func + 8, (Cfg->Type == CFG_REQ)? (byte*)"REQ" :
			  (Cfg->Type == CFG_ACK)? (byte*)"ACK" :
			  (Cfg->Type == CFG_NAK)? (byte*)"NAK" : (byte*)"REJ");

	if ((OptAddr = pppFindOpt (Cfg->Options, endOpts, IPCP_OPT_ADDR)) ||
	    (OptAddr = pppFindOpt (Cfg->Options, endOpts, IPCP_OPT_ADDRS)) ) {
		if (((CFG_OPT *) OptAddr)->Length < IPCP_OPT_ADDR_LEN)
			return (sizeData);
		TRACK_PPP_ADDR = ntohl (((CFG_OPT *) OptAddr)->Data);
		DBG_NDIS(("%s: ADDR=%lx", func, TRACK_PPP_ADDR));
	} else {
		TRACK_PPP_ADDR = 0;
		DBG_NDIS(("%s: ADDR=none", func));
	}

	if (!((Cfg->Type == CFG_REQ) ^ (From == '<'))) return (sizeData);

	Track = &ppp->TxTrack;
	caughtReq = Track->caughtReq;
	caughtRej = Track->caughtRej;
	Track->caughtReq = Track->caughtRej = 0;

	switch (Cfg->Type) {

	case CFG_REQ:

		if (!TRACK_PPP_ADDR) break; /* not our problem */

		/* save original request */

		if (sizeData > sizeof (Track->Req)) {
			DBG_NDIS(("%s: can't save req", func));
			break;
		}
		mem_cpy (Track->Req, *Data, sizeData);

		if ((Track->Flags & IPCP_FLAG_NUL_CFG) || caughtRej) {
			/* remember that we modified req */
			Track->caughtReq = 2;
			/* send an empty request */
			mem_cpy (ppp->ModCfg, *Data, sizeData);
			htons (((CFG_HDR *) (ppp->ModCfg - Adj))->Length, 4);
			*Data = ppp->ModCfg;
	  		sizeData = sizeof (CFG_HDR) - Adj;
			DBG_NDIS(("%s: nul req", func));
		} else {
			/* remember that we saved initial req */
			Track->caughtReq = 1;
		}
		break;

	case CFG_ACK:

		if (caughtReq != 2) break; /* not our problem */

		/* return the saved cfg req instead of ack */

		DBG_NDIS(("%s: nul rep", func));

		((CFG_HDR *) (Track->Req - Adj))->Type = CFG_ACK;

		*Data = Track->Req;
		sizeData = MAX_CFG_HDR_LEN - Adj +
			   ntohs (((CFG_HDR *) (Track->Req - Adj))->Length);

		break;

	case CFG_REJ:

		if (TRACK_PPP_ADDR && caughtReq == 1 &&
		    sizeOpts ==
		    ntohs (((CFG_HDR *) (Track->Req - Adj))->Length)) {
		    	/* everything including nonzero address is rejected, */
			/* smells strongly like NetGW, let's try to patch    */
			Track->caughtRej = 1;
		}
		/* fall through */

	case CFG_NAK:

		Track->Flags &= ~IPCP_FLAG_NUL_CFG;
		break;
	}

	return (sizeData);
}

word pppImplode (PPP_STREAM *S, dword ACCM)
{
/* Gather complete PPP frames from a stream in so called HDLC like framing,
 * i.e. remove frame marks, stuffing and checksum.
 * This function is prepared to scan a contiguous stream, i.e. the input data
 * is not expected to contain a complete frame, but the frame buffer must be
 * big enough for the largest possible complete frame.
 *
 * Begin of processing:
 *
 *	Parameters in PPP_STREAM:
 *
 * 		State		IMP_START
 *		baseStream	begin of stream buffer
 *		Stream		begin of stream buffer
 *		sizeStream	size of data in stream buffer
 *		baseFrame	begin of frame buffer
 *		Frame		begin of frame buffer
 *		sizeFrame	size of frame buffer
 *
 *	Processing:
 *
 *	- Skip all leading frame marks
 *	- Implode data from stream buffer to frame buffer until the end of
 *	  frame is detected or all data in stream buffer is processed.
 *	- Set 'State' to indicate if the frame buffer is a complete frame
 *
 *	  # IMP_START  - frame complete
 *	  # IMP_MORE   - frame incomplete, expect more in stream 
 *	  # IMP_ESCAPE - frame incomplete, expect more in stream,
 *			 last byte seen in stream was PPP-ESCAPE
 *	  # IMP_SCAN   - stream out of sync, no frame so far,
 *			 scan for next frame in stream
 *
 *	- The size of imploded data in frame buffer (if any) is returned.
 *	  'Stream'	points to the next byte to process in stream buffer
 *	  'sizeStream'	contains the number of bytes remaining in stream buffer
 *	  'Frame'	points to the current position in frame buffer
 *
 * Actions of caller after return:
 *
 *	if (value > 0 && 'State' == IMP_START) {
 *		The frame buffer contains a complete frame now !
 *		Empty the frame buffer or allocate a new frame buffer and
 *		set 'baseFrame', 'Frame' and 'sizeFrame' accordingly.
 *	} else if (value == 0) {
 *		This case normally needs no handling !
 *		The frame buffer contains no valid data and thus it 
 *		will be reused as is.
 *		The stream buffer is empty and must be filled again (see below).
 *	}
 *
 *	if ('sizeStream' == 0) {
 *		The stream buffer is empty !
 *		Fill up the stream buffer or get another filled stream buffer
 *		and set 'baseStream', 'Stream' and 'sizeStream' accordingly.
 *	}
 *
 *	Call pppImplode() again
 *
 *	Other values in PPP_STREAM than given above must not be changed cause
 *	otherwise sync is lost !!!
 *
 *	To restart everything from scratch set all parameters in PPP_STREAM
 *	again (see "Begin of processing" above).
 *
 */
	word	sizeStream, sizeFrame;
	byte	*Stream, *Frame, *endFrame;
	byte	c;

	Stream		= S->Stream;
	sizeStream	= S->sizeStream;
	Frame		= S->Frame;
	endFrame	= S->baseFrame + S->sizeFrame;

again: /* reenter here after a framing error if there is more data in stream */

	switch (S->State) {

	case IMP_SCAN:	/* scan for next frame mark */

		for ( ; sizeStream && *Stream != 0x7e; Stream++, sizeStream--) ;

		if (!sizeStream) goto done;

		S->State = IMP_START;

	case IMP_START:	/* skip all frame marks	*/

		for ( ; sizeStream && *Stream == 0x7e; Stream++, sizeStream--) ;

		if (!sizeStream) goto done;

		S->State = IMP_MORE;

		break;

	case IMP_ESCAPE:	/* next valid character must be escaped */

		for ( ;
		     sizeStream && *Stream < 0x20 && (ACCM & (1 << *Stream));
		     Stream++, sizeStream--)
		     	; /* discard masked control characters */

		if (!sizeStream) goto done;

		if ((c = *Stream++) == 0x7e) {
			S->State = IMP_START;
			goto broken;	/* early frame termination */
		}

		if (Frame >= endFrame) {
			S->State = IMP_SCAN;
			goto overrun;
		}

		*Frame++ = c ^ 0x20;

		S->State = IMP_MORE;

		break;

	case IMP_MORE:	/* implode the data */

		break;
	}

	while (sizeStream--) {

		switch (c = *Stream++) {

		case 0x7e: /* frame end */

			if ((sizeFrame = Frame - S->baseFrame) < 4) {
				DBG_FTL(("Implode: garbled"));
				S->State = IMP_START;
				goto restart;	/* forget this frame */
			}

			if (S->Check &&
			    pppfcs16 (PPPINITFCS16, S->baseFrame, sizeFrame)
			    != PPPGOODFCS16) {
				DBG_FTL(("Implode: bad FCS"));
				S->State = IMP_START;
				goto restart;	/* forget this frame */
			}

			Frame -= sizeof (word);	/* cut off checksum */
			S->State = IMP_START;
			goto done;	/* indicate complete frame */

		case 0x7d: /* unescape */

			for ( ;
			     sizeStream && *Stream < 0x20 &&
			     (ACCM & (1 << *Stream));
			     Stream++, sizeStream-- )
			     	; /* discard masked control characters */

			if (!sizeStream--) {
				S->State = IMP_ESCAPE;
				break;
			}

			if ((c = *Stream++) == 0x7e) {
				S->State = IMP_START;
				goto broken;	/* early frame termination */
			}

			if (Frame >= endFrame) {
				S->State = IMP_SCAN;
				goto overrun;	/* scan for next frame */
			}

			*Frame++ = c ^ 0x20;

			break;

		default	 :

			if (ACCM && c < 0x20 && (ACCM & (1 << c)))
				break; /* discard masked control character */

			if (Frame >= endFrame) {
				S->State = IMP_SCAN;
				goto overrun;	/* scan for next frame */
			}

			*Frame++ = c;

			break;
		}
	}

	/* stream exhausted, frame incomplete */

	sizeStream = 0;
	goto done;

broken:
	DBG_FTL(("Implode: broken"));
	goto restart;
overrun:
	DBG_FTL(("Implode: overrun"));
restart:
	Frame = S->baseFrame;

	if (sizeStream) goto again;	/* scan on in stream buffer */

	/* need more stream data */
done:
	S->Stream	= Stream;
	S->sizeStream	= sizeStream;
	S->Frame	= Frame;

	return (Frame - S->baseFrame);
}

static word explode (PPP_STREAM *S, dword ACCM)
{ /* tightly coupled whith pppExplode below */

	byte *Frame, *Stream, *endStream;
	word sizeFrame;
	byte c;

	Frame		= S->Frame;
	sizeFrame	= S->sizeFrame;
	Stream		= S->Stream;

	/* set end of stream below the real end to be sure	*/
	/* that there is always room for trailing frame mark 	*/

	endStream = S->baseStream + S->sizeStream - 2;

	while (sizeFrame--) {

		switch (c = *Frame++) {

		default:	/* 'normal' bytes */
			if (!ACCM || c >= 0x20 || !(ACCM & (1 << c))) {
				if (Stream > endStream) goto incomplete;
				*Stream++ = c;
				break;
			}
			/* fall through to escape */
		case 0x7e: 	/* frame mark */
		case 0x7d:	/* escape */
		case 0x11:	/* XON */
		case 0x13:	/* XOFF */
		case 0x91:	/* XON whith parity */
		case 0x93:	/* XOFF whith parity */
			if (Stream >= endStream) goto incomplete;
			*Stream++ = 0x7d;
			*Stream++ = c ^ 0x20;
		}
	}

	S->Stream = Stream;
	return (1);

incomplete:
	S->Frame	= --Frame;	/* was incremented before check */
	S->sizeFrame	= ++sizeFrame;	/* was decremented before check */
	S->Stream	= Stream;
	return (0);
}

word pppExplode (PPP_STREAM *S, dword ACCM)
{
/* Expand a complete PPP frame to so called HDLC like framing,
 * i.e. add frame marks, stuffing and checksum.
 *
 * Processing a new frame:
 *
 *	Parameters in PPP_STREAM:
 *
 * 		State		EXP_START
 *		baseFrame	first byte of frame
 *		Frame		first byte of frame
 *		sizeFrame	size of frame
 *		baseStream	begin of result buffer
 *		Stream		begin of result buffer
 *		sizeStream	total size of result buffer
 *
 *	Processing:
 *
 *	- Compute FCS and save it in 'Checksum'
 *	- Expand as much as fits to stream buffer
 *	- set 'State' to indicate if the frame was completely processed
 *	- Return the size of expanded data in stream buffer
 *
 *	  # EXP_START - the full frame and the checksum could be expanded
 *	  # EXP_AGAIN - some of the frame could not be expanded,
 *			'Frame' points to next position in frame,
 *			'sizeFrame' contains the remaining number of bytes
 *	  # EXP_CHECK - some of the checksum could not be expanded,
 *			'Frame' points to next position in 'Checksum',
 *			'sizeFrame' contains the remaining number of bytes
 *
 *	If 'State' is not EXP_START after return, the caller can save away
 *	the resulting stream data from 'baseStream', set all '*Stream' parms
 *	again for the same or a new stream buffer and then call Explode again
 *	whith unchanged 'State' and unchanged '*Frame' parms to expand the
 *	remaing bytes either to given stream buffer.
 *
 * Continue processing of frame or checksum:
 *
 *	Parameters to set in PPP_STREAM:
 *
 *		baseStream	begin of new result buffer
 *		Stream		begin of new result buffer
 *		sizeStream	size of new result buffer
 *
 *	Processing:
 *
 *	- Expand as much as fits to stream buffer
 *	- set 'State' to indicate if the frame was completely processed
 *	- Return the size of expanded data in stream buffer
 *
 *	  # EXP_START - full frame and the checksum are expanded now
 *	  # EXP_AGAIN - see above
 *	  # EXP_CHECK - see above
 *
 *	If 'State' is not EXP_START after return, the caller can continue
 *	processing until the complete frame and the checksum are "streamed".
 */
	word	fcs;

	if (S->State == EXP_START) {
		/* compute the the checksum, append in little endian order */
		fcs   = pppfcs16 (PPPINITFCS16, S->baseFrame, S->sizeFrame);
		fcs  ^= PPPINITFCS16;
		S->Checksum[0] = (byte) fcs;
		S->Checksum[1] = (byte) (fcs >> 8);
		/* put leading frame mark to stream buffer */
		*S->Stream++ = 0x7e;
	}

	if (explode (S, ACCM)) {
		/* this part (frame or checksum) completely done */
		if (S->State != EXP_CHECK) {
			/* switch to explode the checksum now */
			S->State	= EXP_CHECK;
			S->Frame  	= S->Checksum;
			S->sizeFrame	= sizeof (S->Checksum);
			if (explode (S, ACCM)) {
				/* frame completely done now */
				S->State     = EXP_START;
				*S->Stream++ = 0x7e; /* add final frame mark */
			}
		}
	} else {
		/* stream full */
		if (S->State == EXP_START) S->State = EXP_AGAIN;
	}

	return ((word) (S->Stream - S->baseStream));
}
