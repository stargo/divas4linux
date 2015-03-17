
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
# include "fax_if.h"
# include "atp_if.h"
# include "isdn_if.h"
# include "fax2.h"
# include "sys_if.h"
# include "debuglib.h"
# include "t30.h"
# include "fax_stat.h"

void faxRsp (struct ISDN_PORT *P, byte *Text);
extern int sprintf (char*, const char*, ...);
extern int global_options;

/*********************************************************************(GEI)**\
*                                                                            *
*                                   macros                                   *
*                                                                            *
\****************************************************************************/

#define USE_FAX_FULL_DEBUG 0
#if USE_FAX_FULL_DEBUG
    #define DBG_MAX_BSIZE       1024
    #define DBG_WRITE_CNT       10000
    #define DBG_RECEIVE_FIRST   0
    #define DBG_RECEIVE_LAST    10000
    #define DBG_BIT_CNT         0
#endif

#define HACK_CMD_K              "AT+FK\r"

#define	MIN_SCANLINE	(5)
#define	MSG_OVERHEAD	(sizeof(MSG_HEAD) + sizeof(word))
#define	MAX_SCANLINES	(2500)
#define	BUF_OVERHEAD	(2048)

/*********************************************************************(GEI)**\
*                                                                            *
*                              type definitions                              *
*                                                                            *
\****************************************************************************/

typedef struct OBJBUFFER
{
    BYTE *                  Data;
    unsigned int            Size;
    unsigned int            Len;
    BYTE *                  Next;
}
OBJBUFFER, *POBJBUFFER;


/*********************************************************************(GEI)**\
*                                                                            *
*                                 prototypes                                 *
*                                                                            *
\****************************************************************************/

static WORD
fax2InRcvDATA
(
	ISDN_PORT *             P,
    BYTE **                 Data,
    WORD                    sizeData,
    BYTE *                  Stream,
    WORD                    sizeStream
);

static VOID
fax2InRcvEDATA
(
	ISDN_PORT *             P,
    BYTE **                 Data,
    WORD                    sizeData,
    BYTE *                  Stream,
    WORD                    sizeStream
);

void fax2InRcvNDISC (ISDN_PORT* P, byte* Data, word length);

static VOID
fax2OutRcvEDATA
(
	ISDN_PORT *             P,
    BYTE **                 Data,
    WORD                    sizeData,
    BYTE *                  Stream,
    WORD                    sizeStream
);

static VOID
fax2OutRcvX_CONNECT_ACK
(
	ISDN_PORT *             P,
    BYTE **                 Data,
    WORD                    sizeData,
    BYTE *                  Stream,
    WORD                    sizeStream
);

static void
fax2RcvEop
(
	ISDN_PORT *             P,
    T30_INFO *              ptPaT30Info
);

static void
fax2OutBufSend
(
    ISDN_PORT *             P,
    POBJBUFFER              OutBuf
);

static int
fax2Xmit
(
    ISDN_PORT *             P,
	byte *                  Data,
    word                    sizeData,
    word                    FrameType
);

static void
fax2ParamStrToBuf
(
    ISDN_PORT *             P,
    BYTE **                 ppcPaCmd,
    BYTE *                  pbPaParam
);

static void
fax2ParamBufToStr
(
    BYTE *                  pbPaParam,
    char *                  pcPaBuffer
);

static void
fax2Capabilities
(
    BYTE *                  pbPaParam,
    char *                  pcPaBuffer,
    UINT                    uPaBufferSize
);

static VOID
InitDocuheader
(
    BYTE *                  pbPaParam,
    PSFF_DOCU_HEADER        ptPaDh
);

static VOID
InitPageheader
(
		ISDN_PORT* P,
    BYTE *                  pbPaParam,
    PSFF_PAGE_HEADER        ptPaPh
);

static VOID
InitPageheaderEnd
(
    PSFF_PAGE_HEADER        ptPaPh
);

static VOID
T30InfoSet
(
	ISDN_PORT *             P,
    T30_INFO *              ptPaT30Info
);

static void
UpdateT30Info
(
	ISDN_PORT *             P,
    T30_INFO *              ptPaT30Info,
    BYTE *                  pbPaParam
);

static void
UpdatePTSInfo
(
	ISDN_PORT *             P,
	T30_INFO *              ptPaT30Info,
	const char* label
);

static void
fax2RxEnqueue
(
    ISDN_PORT *             P,
    byte *                  Data,
    word                    sizeData,
    BOOL                    fScanLineData
);

static void fax2CreateHeaderLine (ISDN_PORT *P,
																	T30_INFO *ptPaT30Info,
																	byte*    length,
																	byte*    start,
																	byte     max_length);

static void fax2Drop (ISDN_PORT *P, byte Cause) ;

static const char* fax_phase_name (byte phase);
static const char* fax_code_name  (byte code);

/*********************************************************************(GEI)**\
*                                                                            *
*                                 variables                                  *
*                                                                            *
\****************************************************************************/

static BYTE                 Cl2Eol [] = { 0x00, 0x80 };
static BYTE                 Cl2Rtc [] = { 0x00, 0x08, 0x80, 0x00, 0x08, 0x80,
                                          0x00, 0x08, 0x80, 0x00, 0x08, 0x80 };
static BYTE                 Cl2Eop [] = { DLE, ETX };

static BYTE                 Cl2ToSff_DF [] =
{
    SFF_DF_1D_HUFFMANN,
    SFF_DF_1D_HUFFMANN,
    SFF_DF_1D_HUFFMANN,
    SFF_DF_1D_HUFFMANN
};

static WORD                 Cl2ToSff_WD [] =
{
    SFF_WD_A4,
    SFF_WD_B4,
    SFF_WD_A3,
    SFF_WD_A5,
    SFF_WD_A6
};

static WORD                 Cl2ToSff_WD_R16 [] =
{
    SFF_WD_A4_R16,
    SFF_WD_B4_R16,
    SFF_WD_A3_R16,
    SFF_WD_A5_R16,
    SFF_WD_A6_R16
};


static BYTE                 Cl2ToIdi_WD [] =
{
	T30_RECPROP_BIT_PAGE_WIDTH_A4,
	T30_RECPROP_BIT_PAGE_WIDTH_B4,
	T30_RECPROP_BIT_PAGE_WIDTH_A3,
	T30_RECPROP_BIT_PAGE_WIDTH_A4,
	T30_RECPROP_BIT_PAGE_WIDTH_A4
};

static BYTE                 IdiToCl2_WD [] =
{
    CL2_WD_1728,
    CL2_WD_2048,
    CL2_WD_2432
};

static BYTE                 Cl2ToIdi_BR [] =
{
    IDI_BR_24, /* 0 */
    IDI_BR_48, /* 1 */
    IDI_BR_72, /* 2 */
    IDI_BR_96, /* 3 */
    IDI_BR_120, /* 4 */
    IDI_BR_144, /* 5 */

/*
	V.34 rates
	*/
    IDI_BR_168, /* 6 */
    IDI_BR_192, /* 7 */
    IDI_BR_216, /* 8 */
    IDI_BR_240, /* 9 */
    IDI_BR_264, /* A */
    IDI_BR_288, /* B */
    IDI_BR_312, /* C */
    IDI_BR_336  /* D */
};

static BYTE                 IdiToCl2_BR [] =
{
    CL2_BR_96,
    CL2_BR_24,
    CL2_BR_48,
    CL2_BR_72,
    CL2_BR_96,
    CL2_BR_120,
    CL2_BR_144,

/*
	V.34 rates
	*/
    CL2_BR_144, /* CL2_BR_168 */
    CL2_BR_144, /* CL2_BR_192 */
    CL2_BR_144, /* CL2_BR_216 */
    CL2_BR_144, /* CL2_BR_240 */
    CL2_BR_144, /* CL2_BR_264 */
    CL2_BR_144, /* CL2_BR_288 */
    CL2_BR_144, /* CL2_BR_312 */
    CL2_BR_144, /* CL2_BR_336 */
};

static BYTE                 IdiToCl2_BR_v34 [] =
{
    CL2_BR_96,
    CL2_BR_24,
    CL2_BR_48,
    CL2_BR_72,
    CL2_BR_96,
    CL2_BR_120,
    CL2_BR_144,

/*
	V.34 rates
	*/
    CL2_BR_168,
    CL2_BR_192,
    CL2_BR_216,
    CL2_BR_240,
    CL2_BR_264,
    CL2_BR_288,
    CL2_BR_312,
    CL2_BR_336
};


static BYTE                 Cl2ToIdi_VR [] =
{
    IDI_VR_NORMAL,
    IDI_VR_FINE,
    IDI_VR_SUPER_FINE,
    IDI_VR_SUPER_FINE, /* 3 is illegal */
    IDI_VR_ULTRA_FINE,
};

static BYTE                 IdiToCl2_VR [] =
{
    CL2_VR_NORMAL,
    CL2_VR_FINE,
    CL2_VR_SUPER_FINE,
    CL2_VR_SUPER_FINE, /* 3 is illegal */
    CL2_VR_ULTRA_FINE,
};

static BYTE CL2VRValues [] = {
    CL2_VR_NORMAL,
    CL2_VR_FINE,
    CL2_VR_SUPER_FINE,
    CL2_VR_ULTRA_FINE
};

static BYTE                 CopyQualityToPTS [] =
{
    1,
    1,
    3,
    2
};

static BYTE                 CL2MaxValues [] =
{
    CL2_VR_FINE,
    CL2_BR_144,
    CL2_WD_2432,
    CL2_LN_A4,
    CL2_DF_1D,
    CL2_EC_ECM256,
    CL2_BF_OFF,
    CL2_ST_40_40
};

static BYTE IdiToCl2_ST [] =
{
		CL2_ST_0_0,   /* T30_MIN_SCANLINE_TIME_00_00_00 */
		CL2_ST_5_5,   /* T30_MIN_SCANLINE_TIME_05_05_05 */
		CL2_ST_10_5,  /* T30_MIN_SCANLINE_TIME_10_05_05 */
		CL2_ST_10_10, /* T30_MIN_SCANLINE_TIME_10_10_10 */
		CL2_ST_20_10, /* T30_MIN_SCANLINE_TIME_20_10_10 */
		CL2_ST_20_20, /* T30_MIN_SCANLINE_TIME_20_20_20 */
		CL2_ST_40_20, /* T30_MIN_SCANLINE_TIME_40_20_20 */
		CL2_ST_40_40, /* T30_MIN_SCANLINE_TIME_40_40_40 */
		CL2_ST_0_0,   /* T30_MIN_SCANLINE_TIME_RES_8     */
		CL2_ST_0_0,   /* T30_MIN_SCANLINE_TIME_RES_9     */
		CL2_ST_0_0,   /* T30_MIN_SCANLINE_TIME_RES_10    */
		CL2_ST_10_10, /* T30_MIN_SCANLINE_TIME_10_10_05  */
		CL2_ST_20_10, /* T30_MIN_SCANLINE_TIME_20_10_05  */
		CL2_ST_20_20, /* T30_MIN_SCANLINE_TIME_20_20_10  */
		CL2_ST_40_20, /* T30_MIN_SCANLINE_TIME_40_20_10  */
		CL2_ST_40_40  /* T30_MIN_SCANLINE_TIME_40_40_20  */
};

static BYTE CL2ToIdi_ST[] =
{
		T30_MIN_SCANLINE_TIME_00_00_00, /* CL2_ST_0_0   */
		T30_MIN_SCANLINE_TIME_05_05_05, /* CL2_ST_5_5   */
		T30_MIN_SCANLINE_TIME_10_05_05, /* CL2_ST_10_5  */
		T30_MIN_SCANLINE_TIME_10_10_10, /* CL2_ST_10_10 */
		T30_MIN_SCANLINE_TIME_20_10_10, /* CL2_ST_20_10 */
		T30_MIN_SCANLINE_TIME_20_20_20, /* CL2_ST_20_20 */
		T30_MIN_SCANLINE_TIME_40_20_20, /* CL2_ST_40_20 */
		T30_MIN_SCANLINE_TIME_40_40_40  /* CL2_ST_40_40 */
};

static BYTE ReverseTable[0x100] =
{
    0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
    0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
    0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
    0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
    0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
    0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
    0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
    0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
    0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
    0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
    0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
    0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
    0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
    0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
    0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
    0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/* ------------------------------------------------------------------------
		(re)initialize CLASS2 specific data (AT&F, AT&Z processing)
	 ------------------------------------------------------------------------ */
void fax2Init (struct ISDN_PORT *P) {

	/*--- init phase and error ---------------------------------------(GEI)-*/

	P->At.Fax2.Phase = CL2_PHASE_IDLE;
	P->At.Fax2.Error = 0;

	/*--- init local modem and session parameter (DCC, DIS, DCS) -----(GEI)-*/

	if (global_options & DIVA_FAX_ALLOW_HIRES) {
		byte i;
		P->At.Fax2.DCC [CL2_PARAM_VR] = 0;
		for (i = 0; i < sizeof(CL2VRValues)/sizeof(CL2VRValues[0]); i++) {
			P->At.Fax2.DCC [CL2_PARAM_VR] |= CL2VRValues [i];
		}
	} else {
		P->At.Fax2.DCC [CL2_PARAM_VR]  = CL2_VR_FINE;
	}


	if ((global_options & DIVA_FAX_FORCE_V34)
	 && (P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport &
		     (1L << CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL))) {
		P->At.Fax2.DCC [CL2_PARAM_BR]  = CL2_BR_336;
	} else if (P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport &
		     (1L << CLASS1_RECONFIGURE_V17_14400)){
		P->At.Fax2.DCC [CL2_PARAM_BR]  = CL2_BR_144;
	} else {
		P->At.Fax2.DCC [CL2_PARAM_BR]  = CL2_BR_96;
	}

	P->At.Fax2.DCC [CL2_PARAM_WD]  = CL2_WD_2432;

#if defined(UNIX)
	P->At.Fax2.DCC [CL2_PARAM_LN]  = CL2_LN_UNLIM;
#else
	P->At.Fax2.DCC [CL2_PARAM_LN]  = CL2_LN_A4;
#endif
	P->At.Fax2.DCC [CL2_PARAM_DF]  = CL2_DF_1D;
	P->At.Fax2.DCC [CL2_PARAM_EC]  = CL2_EC_ECM256;
	P->At.Fax2.DCC [CL2_PARAM_BF]  = CL2_BF_OFF;
	P->At.Fax2.DCC [CL2_PARAM_ST]  = CL2_ST_0_0;

	mem_cpy (P->At.Fax2.DIS, P->At.Fax2.DCC, CL2_PARAM_LEN);

#if defined(UNIX)
	mem_cpy (P->At.Fax2.DCS, P->At.Fax2.DCC, CL2_PARAM_LEN);
#else
	mem_zero (P->At.Fax2.DCS, CL2_PARAM_LEN);
#endif

 	/*--- init local station id --------------------------------------(GEI)-*/

	mem_set (P->At.Fax2.ID, ' ', sizeof (P->At.Fax2.ID) - 1);
	P->At.Fax2.ID [sizeof (P->At.Fax2.ID) - 1]  = '\0';

	/*--- init bit order ---------------------------------------------(GEI)-*/

	P->At.Fax2.ReverseBitOrder = 0;

	/*--- switch receive capability off ------------------------------(GEI)-*/

	//P->At.fFaxListen  = FALSE;

	/*--- switch polling off -----------------------------------------(GEI)-*/

	P->At.Fax2.AllowIncPollingRequest = 0;
	P->At.Fax2.PollingRequestActive   = 0;
	P->At.Fax2.RcvSEP_PWD_delivered   = 0;

	/*
		Reset page header
		*/
	P->At.Fax2.page_header[0] = 0;

	/*
		Reset date and time
		*/
		P->At.Fax2.year    = 0;
		P->At.Fax2.month   = 0;
		P->At.Fax2.day     = 0;
		P->At.Fax2.hour    = 0;
		P->At.Fax2.minute  = 0;
		P->At.Fax2.second  = 0;

	/*--- set t30_info structure -------------------------------------(GEI)-*/

	T30InfoSet (P, &P->At.Fax2.T30Info);

}

void fax2Finit (ISDN_PORT *P)
{
}

/*********************************************************************(JFR)**\
*                                                                            *
*   Class 2 Command handlers - the commands are assumed to be valid			 *
*                                                                            *
\****************************************************************************/

void fax2SetClass (ISDN_PORT *P)
{/* +FCLASS=2 */
	P->At.Fax2.Phase = CL2_PHASE_IDLE;
}

int fax2CmdDCC (ISDN_PORT *P, int Req, int Val, byte **Str) {
	char Response[80];

	switch ( Req ) {
		case FAX_REQ_RNG:	/* +FDCC=? - query modem capabilities */
			fax2Capabilities (P->At.Fax2.DCC, Response, sizeof(Response));
			faxRsp (P, Response);
			break;

		case FAX_REQ_SET:	/* +FDCC=3,2,... - set modem capabilities */
			fax2ParamStrToBuf (P, Str, P->At.Fax2.DCC);
			if (global_options & DIVA_FAX_FORCE_ECM) {
				P->At.Fax2.DCC [CL2_PARAM_EC]  = CL2_EC_ECM256;
			}
			if (global_options & DIVA_FAX_FORCE_V34 &&
				(!(global_options & DIVA_FAX_ALLOW_V34_CODES)) &&
				(P->At.Fax2.DCC [CL2_PARAM_BR] >= CL2_BR_144)) {
				P->At.Fax2.DCC [CL2_PARAM_BR]  = CL2_BR_336;
			}
			mem_cpy (P->At.Fax2.DIS, P->At.Fax2.DCC, CL2_PARAM_LEN);
			T30InfoSet (P, &P->At.Fax2.T30Info);
			break;
	}

	return R_OK;
}

int fax2CmdDIS (ISDN_PORT *P, int Req, int Val, byte **Str) {
	char Response[80];

	switch ( Req ) {
		case FAX_REQ_VAL:	/* +FDIS? - query current DIS settings */
			fax2ParamBufToStr (P->At.Fax2.DIS, Response);
			faxRsp (P, Response);
			break;

		case FAX_REQ_RNG:	/* +FDIS=? - query modem capabilities */
			fax2Capabilities (P->At.Fax2.DCC, Response, sizeof(Response));
			faxRsp (P, Response);
			break;

		case FAX_REQ_SET:	/* +FDIS=3,2,... - set modem capabilities */
			fax2ParamStrToBuf (P, Str, P->At.Fax2.DIS);
			if (global_options & DIVA_FAX_FORCE_ECM) {
				P->At.Fax2.DIS [CL2_PARAM_EC]  = CL2_EC_ECM256;
			}
			if (global_options & DIVA_FAX_FORCE_V34 &&
					(!(global_options & DIVA_FAX_ALLOW_V34_CODES)) &&
					(P->At.Fax2.DCC [CL2_PARAM_BR] >= CL2_BR_144)) {
				P->At.Fax2.DCC [CL2_PARAM_BR]  = CL2_BR_336;
			}
			T30InfoSet (P, &P->At.Fax2.T30Info);
			break;
	}

	return R_OK;
}

/*
	Set date and time
	year,month,day,hour,minute,second,zone,delta

	zone and delta should be zero
	*/
int fax2CmdTD (ISDN_PORT *P, int Req, int Val, byte **Str) {
	int value, year, month, day, hour, minute, second;
	byte* start = *Str;

	year = atNumArg (&start);
	if (year < 1970 || year > 65535 || *start != ',') {
		return (R_ERROR);
	}
	start++;

	month = atNumArg (&start);
	if (month < 1 || month > 12 || *start != ',') {
		return (R_ERROR);
	}
	start++;
	
	day = atNumArg (&start);
	if (day < 1 || day > 31 || *start != ',') {
		return (R_ERROR);
	}
	start++;

	hour = atNumArg (&start);
	if (hour < 0 || hour > 23 || *start != ',') {
		return (R_ERROR);
	}
	start++;

	minute = atNumArg (&start);
	if (minute < 0 || minute > 59 || *start != ',') {
		return (R_ERROR);
	}
	start++;

	second = atNumArg (&start);
	if (second < 0 || second > 59 || *start != ',') {
		return (R_ERROR);
	}
	start++;

	value = atNumArg (&start);
	if (value != 0 || *start != ',') {
		/*
			Zone not supported
			*/
		return (R_ERROR);
	}
	start++;
	
	value = atNumArg (&start);
	if (value != 0) {
		/*
			dts delta not supported
			*/
		return (R_ERROR);
	}

	*Str = start;

	P->At.Fax2.year    = (word)year;
	P->At.Fax2.month   = (byte)month;
	P->At.Fax2.day     = (byte)day;
	P->At.Fax2.hour    = (byte)hour;
	P->At.Fax2.minute  = (byte)minute;
	P->At.Fax2.second  = (byte)second;

	return (R_OK);
}

/*
	Set Fax Page header
	*/
int fax2CmdPH (ISDN_PORT *P, int Req, int Val, byte **Str) {
	byte* start = *Str, *end;
	int length;

	while (*start && *start != ',') {
		start++;
	}
	if (*start == 0 || start[1] != '"') {
		return (R_ERROR);
	}
	start += 2;
	end=start;

	while (*end && *end != '"') {
		end++;
	}
	if (*end == 0) {
		return (R_ERROR);
	}

	if ((length = end - start) >= sizeof(P->At.Fax2.page_header)) {
		return (R_ERROR);
	}

	*Str = end + 1;

	if (end != start) {
		mem_cpy (P->At.Fax2.page_header, start, length);
	}
	P->At.Fax2.page_header[length] = 0;

	DBG_TRC(("[%p:%s] set page header: %s",
					P->Channel,
					P->Name,
					&P->At.Fax2.page_header[0]))

	return (R_OK);
}

int fax2CmdLID (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* +FLID="...." - set local ID */
    byte *p, *q;
    unsigned i;

	p = *Str;


	if ( (*p++ != '"') || !(q = (byte*)str_chr((char*)p, '"'))
	  || ((i = q - p) >= sizeof(P->At.Fax2.ID)) )
		return R_ERROR;

	*Str = q + 1;

	mem_zero (&P->At.Fax2.ID[0], sizeof(P->At.Fax2.ID)) ;

	if ( i > 0 )
		mem_cpy (&P->At.Fax2.ID[0], p, i) ;
	else
		mem_set (&P->At.Fax2.ID[0], ' ', sizeof(P->At.Fax2.ID) - 1) ;

	T30InfoSet (P, &P->At.Fax2.T30Info);

	return R_OK ;
}

int fax2CmdBOR (ISDN_PORT *P, int Req, int Val, byte **Str)
{
    char Response[80];

	switch ( Req )
	{
	case FAX_REQ_VAL:	/* +FBOR? - query current bitorder */
        faxRsp (P, P->At.Fax2.ReverseBitOrder? "1" : "0");
        break ;
	case FAX_REQ_RNG :  /* +FBOR=? - query range */
        fax2Capabilities (P->At.Fax2.DCC, Response, sizeof(Response));
        faxRsp (P, Response);
		break;
	case FAX_REQ_SET:	/* +FBOR=0/1 - set bitorder */
        P->At.Fax2.ReverseBitOrder = (Val & 1);
		break ;
    }
	return R_OK ;
}

int fax2CmdLPL (ISDN_PORT *P, int Req, int Val, byte **Str) {

	switch (Req) {
		case FAX_REQ_VAL:	/* +FLPL? - query current polling state */
			faxRsp (P, P->At.Fax2.AllowIncPollingRequest ? "1" : "0");
			break ;
		case FAX_REQ_SET:	/* +FLPL=0/1 - set bitorder */
			P->At.Fax2.AllowIncPollingRequest = (Val & 1);
			break ;
	}

	return R_OK ;
}

int fax2CmdCR (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* +FCR=0/1 - set listem mode */

	switch ( Req )
	{
	case FAX_REQ_VAL:	
    	faxRsp (P, "1");
        break ;
	case FAX_REQ_RNG :  /* +FCR=? - query range */
        faxRsp (P, "(0,1)");
		break;
    }
	return R_OK ;
}

int fax2CmdDT (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* +FDT - transmit data */
		struct {
			T30_INFO tLoT30Info;
			byte header_line[T30_MAX_HEADER_LINE_LENGTH];
		} info;

    if (P->At.Fax2.Phase == CL2_PHASE_B)
    {
        /*--- send station id to IDI ---------------------------------(GEI)-*/

        T30InfoSet (P, &info.tLoT30Info);
				fax2CreateHeaderLine (P,
															&info.tLoT30Info,
															&info.tLoT30Info.head_line_len,
															((byte*)&info) + sizeof(info.tLoT30Info),
															T30_MAX_HEADER_LINE_LENGTH);
        fax2Xmit (P,
									(byte*)&info.tLoT30Info,
									sizeof(info.tLoT30Info) + info.tLoT30Info.head_line_len,
									N_EDATA);

        return R_VOID ;
    }

    if (P->At.Fax2.Phase == CL2_PHASE_D)
    {
        /*--- enter phase C ------------------------------------------(GEI)-*/

        P->At.Fax2.Phase  		= CL2_PHASE_C;
    	P->port_u.Fax2.uCntEols   	= 0;
    	P->port_u.Fax2.uCntEolsV  	= 0;

        faxRsp (P, "CONNECT");
        portFaxXon (P);	/* respect port DCB flow control settings */

        /*--- switch to data mode ------------------------------------(JFR)-*/

        P->State = P_DATA;

        return R_VOID ;
    }

    DBG_FTL(("[%p:%s] fax2CmdDT: invalid phase", P->Channel, P->Name))
    return R_ERROR ;
}

int fax2CmdDR (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* +FDR - receive data */
    T30_INFO tLoT30Info;
    char     abRespond [32];

    if (P->At.Fax2.Phase == CL2_PHASE_B)
    {
        /*--- cause sending the CFR frame ----------------------------(GEI)-*/

        T30InfoSet (P, &tLoT30Info);

        fax2Xmit (P, (BYTE *) &tLoT30Info, sizeof (tLoT30Info), N_EDATA);

        /*--- send responds '+FCFR' ----------------------------------(GEI)-*/

        faxRsp (P, "+FCFR");

        /*--- send responds '+FDCS:' ---------------------------------(GEI)-*/

       	str_cpy (abRespond, "+FDCS: ");
       	fax2ParamBufToStr (P->At.Fax2.DCS, abRespond + str_len (abRespond));
       	faxRsp (P, abRespond);

        /*--- enter phase C ------------------------------------------(GEI)-*/

        P->At.Fax2.Phase         = CL2_PHASE_C;
        P->port_u.Fax2.uPrevObject    = OBJECT_DOCU;
        P->port_u.Fax2.uNextObject    = OBJECT_DOCU;
    	P->port_u.Fax2.uCntEols   	= 0;
    	P->port_u.Fax2.uCntEolsV  	= 0;

        /*--- send responds 'CONNECT' --------------------------------(GEI)-*/

       	faxRsp (P, "CONNECT");

        /*--- switch to data mode ------------------------------------(JFR)-*/

        P->State = P_DATA;

		return R_VOID ;
    }

    if (P->At.Fax2.Phase == CL2_PHASE_D)
    {
        /*--- cause sending the MCF frame ----------------------------(GEI)-*/

        T30InfoSet (P, &tLoT30Info);

        fax2Xmit (P, (BYTE *) &tLoT30Info, sizeof (tLoT30Info), N_EDATA);

        /*--- enter phase C, if next page will be received -----------(GEI)-*/

        if (P->port_u.Fax2.bET == 0)
        {
            /*--- enter phase C --------------------------------------(GEI)-*/

            P->At.Fax2.Phase         = CL2_PHASE_C;
			P->port_u.Fax2.uCntEols   	= 0;
			P->port_u.Fax2.uCntEolsV  	= 0;

            /*--- send responds 'CONNECT' ----------------------------(GEI)-*/

            faxRsp (P, "CONNECT");

            /*--- switch to data mode --------------------------------(JFR)-*/

            P->State = P_DATA;
        }

        /*--- enter phase C, if next document will be received -------(GEI)-*/
        else
        if (P->port_u.Fax2.bET == 1)
        {
            /*--- enter phase C --------------------------------------(GEI)-*/

            P->At.Fax2.Phase     = CL2_PHASE_C;
			P->port_u.Fax2.uCntEols   = 0;
			P->port_u.Fax2.uCntEolsV  = 0;
        }

        /*--- enter phase E, if fax session is terminating -----------(GEI)-*/
        else
        if (P->port_u.Fax2.bET == 2)
        {
            /*--- enter phase E --------------------------------------(GEI)-*/

            P->At.Fax2.Phase    = CL2_PHASE_E;
        }

		return R_VOID ;
    }

    DBG_FTL(("[%p:%s] fax2CmdDR: invalid phase", P->Channel, P->Name))
    return R_ERROR ;
}

int fax2CmdET (ISDN_PORT *P, int Req, int Val, byte **Str)
{
    SFF_PAGE_HEADER tLoPh;

    if ( P->At.Fax2.Phase != CL2_PHASE_D )
		return R_ERROR;

    switch ( P->port_u.Fax2.bET = (BYTE) Val )
	{
    case 0: /*--- send pageheader ------------------------------------(GEI)-*/

        InitPageheader (P, P->At.Fax2.DCS, &tLoPh);
        fax2Xmit (P, (BYTE *) &tLoPh, sizeof (tLoPh), N_DATA);
        break;

    case 1: /*--- send pageheader (new document/parameter change) ----(GEI)-*/

        InitPageheader (P, P->At.Fax2.DCS, &tLoPh);
        tLoPh.ResVert = 0xff;   /* non-standard, initiates parameter change */
        fax2Xmit (P, (BYTE *) &tLoPh, sizeof (tLoPh), N_DATA);
        break;

    case 2: /*--- send pageheader (document end) ---------------------(GEI)-*/

        InitPageheaderEnd (&tLoPh);
        fax2Xmit (P, (BYTE *) &tLoPh, sizeof (tLoPh), N_DATA);
        break;

    default:
        return R_ERROR ;
    }

	return R_VOID ;
}

int fax2CmdK (ISDN_PORT *P, int Req, int Val, byte **Str)
{
    if (P->At.Fax2.Phase == CL2_PHASE_IDLE || P->At.Fax2.Phase == CL2_PHASE_E)
        return R_ERROR ;

    /*--- send hangup response, reset variables and drop connection --------*/

    fax2Drop (P, CAUSE_SHUT);

    return R_VOID ;	/* don't send an OK response */
}

/****************************************************************************/

T30_INFO *fax2Connect (ISDN_PORT *P)
{
	/* Called by atDial() or atAnswer() to get the FAX parameters (if any).	*/
	/* If the connection succeeds fax2Up() will be called next but there is	*/
	/* no callback for a failing connection.								*/

	if (P->At.Fax2.AllowIncPollingRequest) {
		DBG_TRC(("[%p:%s] fax2Connect Allow Inc. polling Request", P->Channel, P->Name))

		P->At.Fax2.T30Info.control_bits_low  |= (byte)(T30_CONTROL_BIT_ACCEPT_POLLING);
		P->At.Fax2.T30Info.control_bits_high |= (byte)(T30_CONTROL_BIT_ACCEPT_POLLING >> 8);

		if (global_options & DIVA_FAX_FORCE_SEP_SUB_PWD) { /* polling */
			word features = T30_CONTROL_BIT_ACCEPT_SUBADDRESS  |
											T30_CONTROL_BIT_ACCEPT_SEL_POLLING |
											T30_CONTROL_BIT_ACCEPT_PASSWORD;
			P->At.Fax2.T30Info.control_bits_low  |= (byte)(features & 0xff);
			P->At.Fax2.T30Info.control_bits_high |= (byte)(features >> 8);
			DBG_TRC(("[%p:%s] fax2Connect Allow SEP/PWD", P->Channel, P->Name))
		}
	} else if ((P->Direction == P_CONN_IN) &&
			(global_options & DIVA_FAX_FORCE_SEP_SUB_PWD)) { /* normal receive option */
		word features = T30_CONTROL_BIT_ACCEPT_SUBADDRESS  |
										T30_CONTROL_BIT_ACCEPT_PASSWORD;
		P->At.Fax2.T30Info.control_bits_low  |= (byte)(features & 0xff);
		P->At.Fax2.T30Info.control_bits_high |= (byte)(features >> 8);
		DBG_TRC(("[%p:%s] fax2Connect Allow SUB/PWD", P->Channel, P->Name))
	}

	return ( &P->At.Fax2.T30Info );
}

void fax2Up (ISDN_PORT *P)
{
	/* Called by portUp(), i.e an inbound or outbound call was successfully	*/
	/* established and the B-channel is ready.								*/

	if ( P->Direction == P_CONN_OUT )
	{
		/*
		 * To be prepared for some FAX programs which send any scan line whith
		 * a single write we must hack around here and try to increase the send
		 * buffer size for outbound calls.
		 * We must be prepared to queue as much minimum length scanlines as fit
		 * in the user requested buffer size (but not more than the maximum
		 * number of scanlines per fine resolution page).
		 * Further we have to accept slightly more data as fits in the user
		 * requested buffer cause fax2Write() may change the size of data
		 * coming via PortWrite(), thus we add some more bytes to the computed
		 * amount.
		 * The trick here is, that the queue limit for the apps remains
		 * virtually the same (QOutSize not changed).
		 */
		byte* TxQueue;
		dword TxSize ;
		TxSize = ((P->PortData.QOutSize / MIN_SCANLINE) + 1) * MSG_OVERHEAD ;
		if (TxSize > MAX_SCANLINES * MSG_OVERHEAD)
			TxSize = MAX_SCANLINES * MSG_OVERHEAD ;
		TxSize += P->PortData.QOutSize + BUF_OVERHEAD ;
		if (TxSize > MAX_TX_QUEUE)
			TxSize = MAX_TX_QUEUE ;
		TxQueue = 0 ; /* force realloc */
		if (portSetQueue (P, &TxQueue, &TxSize, &P->TxBuffer, 'W') )
		{
			queueInit (&P->TxQueue, TxQueue, TxSize);
			portInitQOut (P, TxQueue, P->PortData.QOutSize);
		}
	}

    /*--- enter phase A ----------------------------------------------(GEI)-*/

    P->At.Fax2.Phase  = CL2_PHASE_A;

    /*--- debug ------------------------------------------------------(GEI)-*/

    //P->port_u.Fax2.uBitCount  = DBG_BIT_CNT;

    /*--- init session variable --------------------------------------(GEI)-*/

    P->port_u.Fax2.uLineLen       = 0;
    P->port_u.Fax2.dLineData      = 0;
	P->port_u.Fax2.uLineDataLen   = 0;
    P->port_u.Fax2.bDle           = 0;
    P->port_u.Fax2.fNullByteExist = FALSE;
    P->port_u.Fax2.uCntBlocks     = 0;
    P->port_u.Fax2.uLargestBlock  = 0;
    P->port_u.Fax2.uLargestLine   = 0;
	P->port_u.Fax2.nPageCount     = 0;
	P->port_u.Fax2.nRespTxEmpty   = R_VOID;

    /*--- init remote modem parameter (DIS) --------------------------(GEI)-*/

    P->port_u.Fax2.abDISRemote [CL2_PARAM_VR]  = CL2_VR_NORMAL;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_BR]  = CL2_BR_24;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_WD]  = CL2_WD_1728;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_LN]  = CL2_LN_A4;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_DF]  = CL2_DF_1D;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_EC]  = CL2_EC_OFF;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_BF]  = CL2_BF_OFF;
    P->port_u.Fax2.abDISRemote [CL2_PARAM_ST]  = CL2_ST_0_0;

    /*--- init remote station id -------------------------------------(GEI)-*/

    P->port_u.Fax2.acIDRemote [0]  = '\0';

# if 0
    /* seems to be not a good idea because some Fax Programs begin to poll  */
    /* the input queue heavily as soon as they see +FCON !		            */
    /*--- send response '+FCON' --------------------------------------(JFR)-*/
    faxRsp (P, "+FCON");
# endif

	P->port_u.Fax2.remote_speed = 0;
}

/****************************************************************************/

int fax2Delay (ISDN_PORT *P, byte *Response, word sizeResponse, dword *RspDelay)
{
	/* Called by atRspQueue() just before a Response is entered to the pen-	*/
	/* ding response queue or to the receive queue.							*/
	/* Here we have the chance to adjust the delay to be used used for this	*/
	/* response by changing the value passed indirectly via '*RspDelay'.	*/
	/* The function return value defines the kind of delay handling:		*/
	/*  == 0  -	default response delay handling								*/
	/*  != 0  -	all pending responses are flushed und the current response	*/
	/*			is delayed by the value written to '*RspDelay'.				*/
	/*																		*/
	/* We delay all responses in FAX ESCAPE mode by some ms because most of	*/
	/* the braindamaged fax applications expect a delay here.				*/
	/* CONNECT responses for outbound calls get a larger delay to account	*/
	/* for the line turnaround time needed after the CFR or MCF frame was	*/
	/* received. For the old t30 implementation we assumed 250 ms after CFR */
	/* and 1000 ms after MCF, now we assume 250 ms in both cases and defer	*/
	/* the response by 200 ms. This way the application will not start to	*/
	/* send data a long time before there is a chance to transmit data.		*/
	/* Further we set the baud rate at which IDI should be feeded when we	*/
	/* get a CONNECT response. This is mainly to simulate real FAX modem	*/
	/* behavior and thus to keep the animatinons of some apps working.		*/

	if ( P->State != P_ESCAPE )
		return (0) ;

	if ( !*RspDelay )
	{
#if 0 /* VXD */
		*RspDelay = 10 ;
#endif
#if defined(WINNT) || defined(UNIX)
		*RspDelay = 25 ;	/* to overcome the minimum timer interval */
#endif
	}

	if ( P->Direction != P_CONN_OUT
	  || mem_cmp (Response, "\r\nCONNECT\r\n", sizeof("\r\nCONNECT\r\n") - 1) )
		return (0) ;

	*RspDelay = 200 ; // P->port_u.Fax2.nPageCount ? 800/*MCF*/ : 200/*CFR*/
	P->Defer  = 0;

	return (1);
}

/****************************************************************************/

void fax2TxEmpty (ISDN_PORT *P)
{
	/*--- do nothing, if fax session isn't active --------------------(GEI)-*/

    if (P->At.Fax2.Phase == CL2_PHASE_IDLE || P->port_u.Fax2.nRespTxEmpty == R_VOID)
    {
        return;
    }

	/*--- send respond 'OK' --------------------------------------(GEI)-*/

	atRsp (P, P->port_u.Fax2.nRespTxEmpty);

	P->port_u.Fax2.nRespTxEmpty  = R_VOID;

	/*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

WORD fax2Write (ISDN_PORT *P, BYTE **Data, WORD sizeData,
                              BYTE *Frame, WORD sizeFrame, WORD *FrameType)
{
    OBJBUFFER               InBuf;
    BYTE                    bInData;
	BYTE					bInMask;
    OBJBUFFER               OutBuf;
    OBJBUFFER               LineBuf;
	DWORD					dLineData;
	UINT        			uLineDataLen;
    BYTE                    bByte;
    BYTE                    Event;
#       define EVENT_NONE       0
#       define EVENT_EOD        1
#       define EVENT_EOL        2
#       define EVENT_EOP        3
#       define EVENT_CMD_K      4


	/* Simply ignore any data written in data mode when receiving a fax.	*/
	/* This is not completely correct because only XON's should come here.	*/

    if ( P->Direction != P_CONN_OUT )
	{
		InBuf.Data     = *Data;
		InBuf.Size     = sizeData;
		InBuf.Len      = 0;
		InBuf.Next     = InBuf.Data;
		while (InBuf.Len < InBuf.Size)
		{
			switch (*InBuf.Next)
			{
			case 0x11: /* Xon */
				P->RxXoffFlow = FALSE;
				break;
			case 0x13: /* Xoff */
				P->RxXoffFlow = TRUE;
				break;
			}
			InBuf.Next++;
			InBuf.Len++;
		}
		return (0) ;
	}

	if ( P->At.Fax2.Phase != CL2_PHASE_C )
    {
        DBG_FTL(("[%p:%s] fax2Write: not in send phase C", P->Channel, P->Name))
        return (0);
    }

	/* Check if there is enough room left in send queue because we cannot	*/
	/* handle a queue full condition when collecting partial lines.			*/

	if (!queueSpace(&P->TxQueue, (word)(sizeData + P->port_u.Fax2.uLineLen))) {
 		DBG_ERR(("[%p:%s] fax2Write: Q full", P->Channel, P->Name))
		return (FAX_WRITE_TXFULL);
    }

	/* Limit the size of the frame buffer to the max scanline size.			*/
	/* Thus the frames generated here are small enough to trigger dequeues	*/
	/* from the xmit q in the right intervals (MS Fax on Win3x cancels the	*/
	/* session if the xmit q stays full for more than 1/2 second).			*/

    if (sizeFrame < FAX_MAX_WRITE_SCANLINE) {
        DBG_FTL(("[%p:%s] fax2Write: framebuf too small", P->Channel, P->Name))
        return (0);
    }
    sizeFrame = FAX_MAX_WRITE_SCANLINE;

    /*--- statistics -------------------------------------------------(GEI)-*/

    P->port_u.Fax2.uCntBlocks++;
#if defined(LINUX)
    P->port_u.Fax2.uLargestBlock  = (P->port_u.Fax2.uLargestBlock > sizeData) ? P->port_u.Fax2.uLargestBlock : sizeData;
#else
    P->port_u.Fax2.uLargestBlock  = max (P->port_u.Fax2.uLargestBlock, sizeData);
#endif

    /*--- debug ------------------------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
    DBG_TRC(("fax2Write (%d) { %d+%d+%d", P->port_u.Fax2.uCntBlocks,
        P->port_u.Fax2.uLineLen, P->port_u.Fax2.uLineDataLen, sizeData))

    if (P->port_u.Fax2.uCntBlocks <= DBG_WRITE_CNT)
    {
#if USE_LOG_BLK
		if (DBG_MASK(DL_TRC|DL_BLK)){
        	DBG_BLK((*Data, ((sizeData<DBG_MAX_BSIZE)?sizeData:DBG_MAX_BSIZE)))
		}
#endif
    }
#endif
    /*--- init InBuffer ----------------------------------------------(GEI)-*/

    InBuf.Data     = *Data;
    InBuf.Size     = sizeData;
    InBuf.Len      = 0;
    InBuf.Next     = InBuf.Data;

    bInData        = 0;
    bInMask        = 0;

    /*--- init LineBuffer ---------------------------------------------(GEI)-*/

    LineBuf.Data     = P->port_u.Fax2.abLine;
    LineBuf.Size     = sizeof (P->port_u.Fax2.abLine);
    LineBuf.Len      = P->port_u.Fax2.uLineLen;
    LineBuf.Next     = LineBuf.Data + LineBuf.Len;

	dLineData        = P->port_u.Fax2.dLineData;
	uLineDataLen     = P->port_u.Fax2.uLineDataLen;

    /*--- init OutBuffer ---------------------------------------------(GEI)-*/

    OutBuf.Data     = Frame;
    OutBuf.Size     = sizeFrame;
    OutBuf.Len      = 0;
    OutBuf.Next     = OutBuf.Data;

    /*--- init event -------------------------------------------------(GEI)-*/

    Event  = EVENT_NONE;

    /*--- set event, if given data is the cmd AT+FK ------------------(GEI)-*/

    /* this is a special hack for Microsoft Fax At Work                     */

    if
    (
        InBuf.Size == sizeof (HACK_CMD_K) - 1
        &&
        mem_i_cmp (InBuf.Data, HACK_CMD_K, InBuf.Size) == 0
    )
    {
        DBG_ERR(("[%p:%s] got cmd AT+FK as data, cancel session", P->Channel, P->Name))

        Event  = EVENT_CMD_K;
    }

    /*--- copy all lines of buffer -----------------------------------(GEI)-*/

    for (;;)
    {
        /*--- break loop, if cmd AT+FK found -------------------------(GEI)-*/

        /* this is a special hack for Microsoft Fax At Work                 */

        if (Event == EVENT_CMD_K)
        {
            break;
        }

        /*--- copy all bytes of line ---------------------------------(GEI)-*/

        for (;;)
        {
            /*--- copy all bits of byte ------------------------------(GEI)-*/

            /*... get next byte, if no more bits available ...........(GEI).*/

            if (bInMask == 0)
            {
                /*--- break, if end of data --------------------------(GEI)-*/

                if (InBuf.Len >= InBuf.Size)
                {
                    Event = EVENT_EOD;
                    break;
                }

                /*--- process DLE byte, if exist ---------------------(GEI)-*/

                if (P->port_u.Fax2.bDle != DLE && *InBuf.Next == DLE)
                {
                    P->port_u.Fax2.bDle  = DLE;
                    InBuf.Next++;
                    InBuf.Len++;

                    if (InBuf.Len >= InBuf.Size)
                    {
                        Event = EVENT_EOD;
                        break;
                    }
                }

                if (P->port_u.Fax2.bDle == DLE)
                {
                    P->port_u.Fax2.bDle  = 0;

                    if (*InBuf.Next == ETX)
                    {
                        Event  = EVENT_EOP;
						break;
                    }
					else
                    if (*InBuf.Next == DLE)
					{
						// do nothing
					}
					else
                    {
                        DBG_ERR(("[%p:%s] unknown DLE escape %02x found", P->Channel, P->Name, *InBuf.Next))

                        InBuf.Next++;
                        InBuf.Len++;

                        if (InBuf.Len >= InBuf.Size)
                        {
                            Event = EVENT_EOD;
                            break;
                        }
												continue;
                    }
                }

                /*--- get next byte ----------------------------------(GEI)-*/

                InBuf.Len++;

                bInData  = *InBuf.Next++;
                bInMask  = (P->At.Fax2.ReverseBitOrder) ? 0x80 : 0x01;
            }

            /*... copy all bits ......................................(GEI).*/

            while (bInMask)
            {
                /*--- shift current bit into OutData (highest bit) ---(GEI)-*/

                dLineData >>= 1;
                uLineDataLen++;

                if (bInData & bInMask)
                {
                    dLineData |= 0x80000000;
                }

                /*--- debug ------------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
                if (P->port_u.Fax2.uBitCount > 0)
                {
                    P->port_u.Fax2.uBitCount--;

                    DBG_TRC(("BitTrace: ID=%02x,IM=%02x,LD=%08lx,LL=%d",
                        bInData, bInMask, dLineData, uLineDataLen))
                }
#endif
                /*--- next bit from InBuffer -------------------------(GEI)-*/

                if (P->At.Fax2.ReverseBitOrder)
                {
                    bInMask >>= 1;
                }
                else
                {
                    bInMask <<= 1;
                }

                /*--- look for end of line (EOL) ---------------------(GEI)-*/

                if
                (
                    uLineDataLen >= T4_EOL_BITSIZE
                    &&
                    (dLineData & T4_EOL_MASK_DWORD) == T4_EOL_DWORD
                )
                {
                    /*--- set event to end of line -------------------(GEI)-*/

                   	Event = EVENT_EOL;

                    /*--- process exist data bits --------------------(GEI)-*/

                    if (uLineDataLen > T4_EOL_BITSIZE)
                    {
                        /*--- extract byte ---------------------------(GEI)-*/

                        bByte
                        =
                        (BYTE)
                        (
                            (dLineData & ~T4_EOL_MASK_DWORD)
                            >>
                            (DWORD_BITSIZE  - uLineDataLen)
                        );

                        /*--- handle leading null bytes before eol ---(GEI)-*/

                        if (bByte == 0)
                        {
                            if (! P->port_u.Fax2.fNullByteExist)
                            {
                                P->port_u.Fax2.uNullBytesPos   = LineBuf.Len;
                                P->port_u.Fax2.fNullByteExist  = TRUE;
                            }
                        }
                        else
                        {
                            P->port_u.Fax2.fNullByteExist  = FALSE;
                        }

                        /*--- put byte into LineBuffer ---------------(GEI)-*/

                        if (LineBuf.Len < LineBuf.Size)
                        {
                            *LineBuf.Next++  = bByte;
                            LineBuf.Len++;
                        }

                        /*--- debug ----------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
                        if (P->port_u.Fax2.uBitCount > 0)
                        {
                            DBG_TRC(("BitTrace: EOL: Byte=%02x,LD=%08lx,LL=%d",
                                bByte, dLineData, uLineDataLen))
                        }
#endif
                    }
                    else
                    {
                        /*--- debug ----------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
                        if (P->port_u.Fax2.uBitCount > 0){
                            DBG_TRC(("BitTrace: EOL"))
						}
#endif
                    }

                    uLineDataLen  = 0;

                    break;
                }

                /*--- put byte in OutBuffer, if entire it extracted --(GEI)-*/

                if (uLineDataLen >= T4_EOL_BITSIZE + BYTE_BITSIZE)
                {
                    /*--- extract byte -------------------------------(GEI)-*/

                    bByte
                    =
                    (BYTE)
                    (
                        (dLineData & ~T4_EOL_MASK_DWORD)
                        >>
                        (DWORD_BITSIZE - T4_EOL_BITSIZE - BYTE_BITSIZE)
                    );

                    dLineData    &= T4_EOL_MASK_DWORD;
                    uLineDataLen  = T4_EOL_BITSIZE;

                    /*--- handle leading null bytes before eol -------(GEI)-*/

                    if (bByte == 0)
                    {
                        if (! P->port_u.Fax2.fNullByteExist)
                        {
                            P->port_u.Fax2.uNullBytesPos   = LineBuf.Len;
                            P->port_u.Fax2.fNullByteExist  = TRUE;
                        }
                    }
                    else
                    {
                        P->port_u.Fax2.fNullByteExist  = FALSE;
                    }

                    /*--- put byte into LineBuffer -------------------(GEI)-*/

                    if (LineBuf.Len < LineBuf.Size)
                    {
                        *LineBuf.Next++  = bByte;
                        LineBuf.Len++;
                    }

                    /*--- debug --------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
                    if (P->port_u.Fax2.uBitCount > 0)
                    {
                        DBG_TRC(("BitTrace: Byte=%02x,LD=%08lx,LL=%d",
                            bByte, dLineData, uLineDataLen))
                    }
#endif
                }

                /*--- next bit ---------------------------------------(GEI)-*/
            }

            /*--- break copy line loop, if an event occur ------------(GEI)-*/

            if (Event != EVENT_NONE)
            {
                break;
            }

            /*--- next byte ------------------------------------------(GEI)-*/
        }

        /*--- break copy all lines loop, if not eol found ------------(GEI)-*/

        if (Event != EVENT_EOL && Event != EVENT_EOP)
        {
            break;
        }

		/*--- flush LineBuffer, if it is not empty -------------------(GEI)-*/

        if (Event == EVENT_EOP && uLineDataLen > 0)
		{
			dLineData  >>= DWORD_BITSIZE - uLineDataLen;
			uLineDataLen  = 0;

			while (dLineData != 0)
			{
                bByte = (BYTE) dLineData;
				dLineData  >>= BYTE_BITSIZE;

				if (bByte == 0)
				{
					if (! P->port_u.Fax2.fNullByteExist)
					{
						P->port_u.Fax2.uNullBytesPos   = LineBuf.Len;
						P->port_u.Fax2.fNullByteExist  = TRUE;
					}
				}
				else
				{
					P->port_u.Fax2.fNullByteExist  = FALSE;
				}

				if (LineBuf.Len < LineBuf.Size)
				{
					*LineBuf.Next++  = bByte;
					LineBuf.Len++;
				}
			}
        }

        /*--- statistics ---------------------------------------------(GEI)-*/
#if defined(LINUX)
        P->port_u.Fax2.uLargestLine  = (P->port_u.Fax2.uLargestLine > LineBuf.Len)? P->port_u.Fax2.uLargestLine : LineBuf.Len;
#else                                    
        P->port_u.Fax2.uLargestLine  = max (P->port_u.Fax2.uLargestLine, LineBuf.Len);
#endif

        /*--- remove leading null bytes before eol -------------------(GEI)-*/

        if (P->port_u.Fax2.fNullByteExist)
        {
            if (P->port_u.Fax2.uNullBytesPos == 0)
            {
                LineBuf.Len  = 0;
            }
            else
            {
                LineBuf.Len  = P->port_u.Fax2.uNullBytesPos + 1;
            }
        }

        /*--- put line into OutBuffer, if line data available --------(GEI)-*/

        if (LineBuf.Len > 0)
        {
			/*--- increment count of scanlines with data -------------(GEI)-*/

			P->port_u.Fax2.uCntEols++;

			/*--- send OutBuffer, if it hasn't enough space for ------(GEI)-*/
			/*    current line                                              */

			if (OutBuf.Len + LineBuf.Len + SFF_LEN_FLD_SIZE > OutBuf.Size)
			{
				fax2OutBufSend (P, &OutBuf);
			}

			/*--- copy line into OutBuffer (use SFF-format) ----------(GEI)-*/

			if (LineBuf.Len <= 216)
			{
				*OutBuf.Next++  = (BYTE) LineBuf.Len;
				OutBuf.Len++;
			}
			else
			{
				*OutBuf.Next++             = 0;
				*OutBuf.Next++  = (BYTE) LineBuf.Len;
				*OutBuf.Next++  = (BYTE)(LineBuf.Len >> 8);

				OutBuf.Len  += 3;
			}

			mem_cpy (OutBuf.Next, LineBuf.Data, LineBuf.Len);

			OutBuf.Next  += LineBuf.Len;
			OutBuf.Len   += LineBuf.Len;
        }
		else
		{
			/*--- increment count of scanlines without data ----------(GEI)-*/

			if (Event == EVENT_EOL) P->port_u.Fax2.uCntEolsV++;
		}

        /*--- empty LineBuffer ---------------------------------------(GEI)-*/

        LineBuf.Len   = 0;
        LineBuf.Next  = LineBuf.Data;

        P->port_u.Fax2.fNullByteExist  = FALSE;

		/*--- break loop, if event is 'EndOfPage' --------------------(GEI)-*/

        if (Event == EVENT_EOP) break;

        /*--- init event ---------------------------------------------(GEI)-*/

        Event  = EVENT_NONE;

        /*--- next line ----------------------------------------------(GEI)-*/
    }

    /*--- switch on event --------------------------------------------(GEI)-*/

    if (Event == EVENT_EOP)
    {
        /*--- debug --------------------------------------------------(GEI)-*/

        DBG_TRC((
            "[%p:%s] fax2Write (%d): EOP, EOLs=%d(%d), BlockMax=%d, LineMax=%d",
			P->Channel, P->Name,
            P->port_u.Fax2.uCntBlocks,
            P->port_u.Fax2.uCntEols,
            P->port_u.Fax2.uCntEolsV,
			P->port_u.Fax2.uLargestBlock,
			P->port_u.Fax2.uLargestLine
        ))

        /*--- enter phase D ------------------------------------------(GEI)-*/

        P->At.Fax2.Phase  = CL2_PHASE_D;
        P->State         = P_ESCAPE;

        /*--- let AT response handler know about current page --------(GEI)-*/

		P->port_u.Fax2.nPageCount++;

        /*--- send respond 'OK' --------------------------------------(GEI)-*/

		if (queueCount(&P->TxQueue) == 0)
		{
        	atRsp (P, R_OK);
		}
		else
		{
			P->port_u.Fax2.nRespTxEmpty  = R_OK;
		}
    }
    else
    if (Event == EVENT_CMD_K)
    {
        /*--- debug --------------------------------------------------(GEI)-*/

        DBG_CONN((
            "[%p:%s] fax2Write (%d): CANCEL, EOLs=%d(%d), BlockMax=%d, LineMax=%d",
            P->Channel, P->Name,
			P->port_u.Fax2.uCntBlocks,
            P->port_u.Fax2.uCntEols,
            P->port_u.Fax2.uCntEolsV,
            P->port_u.Fax2.uLargestBlock,
            P->port_u.Fax2.uLargestLine
        ))

        /*--- clear out and line buffer ------------------------------(GEI)-*/

        OutBuf.Len      = 0;
        OutBuf.Next     = OutBuf.Data;
        LineBuf.Len     = 0;
	    uLineDataLen    = 0;

		/*--- send hangup response, reset variables and drop connection ----*/

		fax2Drop (P, CAUSE_SHUT);
    }

    /*--- send OutBuffer ---------------------------------------------(GEI)-*/

    if (OutBuf.Len > 0)
    {
        fax2OutBufSend (P, &OutBuf);
    }

    /*--- save buffer data -------------------------------------------(GEI)-*/

    P->port_u.Fax2.uLineLen       = LineBuf.Len;
	P->port_u.Fax2.dLineData      = dLineData;
	P->port_u.Fax2.uLineDataLen   = uLineDataLen;

    /*--- debug ------------------------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
    if (P->port_u.Fax2.uCntBlocks <= DBG_WRITE_CNT)
    {
#if USE_LOG_BLK
		if (DBG_MASK(DL_TRC|DL_BLK)){
        	DBG_TRC(("LineBuf:"))
        	DBG_BLK((LineBuf.Data, ((LineBuf.Len<DBG_MAX_BSIZE)?LineBuf.Len:DBG_MAX_BSIZE)))
		}
#endif
        DBG_TRC(("dLineData %08lx", dLineData))
    }
    DBG_TRC(("fax2Write (%d) } %d+%d+%d",
        P->port_u.Fax2.uCntBlocks,
        P->port_u.Fax2.uLineLen,
        P->port_u.Fax2.uLineDataLen,
        sizeData - InBuf.Len
    ))
#endif

    /*--- undefine macros --------------------------------------------(GEI)-*/

    #undef EVENT_NONE
    #undef EVENT_EOD
    #undef EVENT_EOL
    #undef EVENT_EOP
    #undef EVENT_CMD_K

    /*--- return -----------------------------------------------------(GEI)-*/

	return (0);
}

/****************************************************************************/

WORD fax2Recv (ISDN_PORT *P,
			   BYTE **Data, WORD sizeData,
			   BYTE *Stream, WORD sizeStream, WORD FrameType)
{
    /*--- switch on call direction -----------------------------------(GEI)-*/

    if (P->Direction == P_CONN_IN)
    {
        /*--- switch on frame type -----------------------------------(GEI)-*/

        switch (FrameType)
        {

        /*... page data ..............................................(GEI).*/

        case N_DATA:

            /*--- process data ---------------------------------------(GEI)-*/

            sizeData
            =
            fax2InRcvDATA
            (
	            P,
                Data,
                sizeData,
                Stream,
                sizeStream
            );

            break;

        /*... info data (dialog with T30 protocol of IDI) ............(GEI).*/

        case N_EDATA:

            /*--- process data ---------------------------------------(GEI)-*/

            fax2InRcvEDATA
            (
	            P,
                Data,
                sizeData,
                Stream,
                sizeStream
            );

            /*--- return without data --------------------------------(GEI)-*/

            sizeData  = 0;

            break;

        /*... all other frame types (invalid frames) .................(GEI).*/

        default:

            /*--- return without data --------------------------------(GEI)-*/

            sizeData  = 0;

            break;
        }
    }
    else
    {
        /*--- switch on frame type -----------------------------------(GEI)-*/

        switch (FrameType)
        {

        /*... info data (dialog with T30 protocol of IDI) ............(GEI).*/

        case N_EDATA:

            /*--- process data ---------------------------------------(GEI)-*/

            fax2OutRcvEDATA
            (
	            P,
                Data,
                sizeData,
                Stream,
                sizeStream
            );

            /*--- return without data --------------------------------(GEI)-*/

            sizeData  = 0;

            break;

        /*... from port driver generate info data ....................(GEI).*/

        case ISDN_FRAME_CTL | N_CONNECT_ACK:

            /*--- process data ---------------------------------------(GEI)-*/

            fax2OutRcvX_CONNECT_ACK
            (
	            P,
                Data,
                sizeData,
                Stream,
                sizeStream
            );

            /*--- return without data --------------------------------(GEI)-*/

            sizeData  = 0;

            break;

        /*... all other frame types (invalid frames) .................(GEI).*/

        default:

            /*--- return without data --------------------------------(GEI)-*/

            sizeData  = 0;

            break;
        }
    }

    /*--- return with new datasize -----------------------------------(GEI)-*/

	return (sizeData);
}

/****************************************************************************/

static WORD
fax2InRcvDATA (ISDN_PORT *P,
               BYTE **Data, WORD sizeData, BYTE *Stream, WORD sizeStream)
{
    OBJBUFFER               InBuf;
    OBJBUFFER               OutBuf;
    OBJBUFFER               LineBuf;
    UINT                    uLength;
    UINT                    uObjectSize;
    UINT                    uObjHeadLen;
    UINT                    uObjDataLen;
    BYTE                    bRecordtype;
    BYTE                    bPageHeaderLen;
    BYTE                    Event;
#       define EVENT_NONE       0
#       define EVENT_NEEDDATA   1

    /*--- statistics -------------------------------------------------(GEI)-*/

    P->port_u.Fax2.uCntBlocks++;
#if defined(LINUX)
    P->port_u.Fax2.uLargestBlock  = (P->port_u.Fax2.uLargestBlock > sizeData)? P->port_u.Fax2.uLargestBlock : sizeData;
#else              
    P->port_u.Fax2.uLargestBlock  = max (P->port_u.Fax2.uLargestBlock, sizeData);

#endif

    /*--- debug ------------------------------------------------------(GEI)-*/

#if USE_FAX_FULL_DEBUG
    if (P->port_u.Fax2.uCntBlocks >= DBG_RECEIVE_FIRST && P->port_u.Fax2.uCntBlocks <= DBG_RECEIVE_LAST)
    {
        DBG_TRC(("fax2InRcvDATA (%d) { (%d+)%d", P->port_u.Fax2.uCntBlocks,
            P->port_u.Fax2.uLineLen, sizeData))

#if USE_LOG_BLK
		if (DBG_MASK(DL_TRC|DL_BLK)) {
			DBG_TRC(("Line:"))
			DBG_BLK((P->port_u.Fax2.abLine, P->port_u.Fax2.uLineLen))
			DBG_TRC(("Data:"))
			DBG_BLK((*Data, ((sizeData<DBG_MAX_BSIZE)?sizeData:DBG_MAX_BSIZE)))
		}
#endif
    }
#endif

    /*--- init InBuffer ----------------------------------------------(GEI)-*/

    InBuf.Data     = *Data;
    InBuf.Size     = sizeData;
    InBuf.Len      = 0;
    InBuf.Next     = InBuf.Data;

    /*--- init LineBuffer ---------------------------------------------(GEI)-*/

    LineBuf.Data     = P->port_u.Fax2.abLine;
    LineBuf.Size     = sizeof (P->port_u.Fax2.abLine);
    LineBuf.Len      = P->port_u.Fax2.uLineLen;
    LineBuf.Next     = LineBuf.Data + LineBuf.Len;

    /*--- init OutBuffer ---------------------------------------------(GEI)-*/

    OutBuf.Data     = Stream;
    OutBuf.Size     = sizeStream;
    OutBuf.Len      = 0;
    OutBuf.Next     = OutBuf.Data;

    /*--- init event -------------------------------------------------(GEI)-*/

    Event  = EVENT_NONE;

    /*--- process all received data ----------------------------------(GEI)-*/

    while (Event == EVENT_NONE)
    {
        /*--- switch on next receiving object ------------------------(GEI)-*/

        switch (P->port_u.Fax2.uNextObject)
        {

        /*... next receiving object is the document header ...........(GEI).*/

        case OBJECT_DOCU:

            /*--- wait on more data, if not enough available ---------(GEI)-*/

            uLength  = LineBuf.Len + (InBuf.Size - InBuf.Len);

            if (uLength < sizeof (SFF_DOCU_HEADER))
            {
                Event  = EVENT_NEEDDATA;
                break;
            }

            /*--- set object size ------------------------------------(GEI)-*/

            uObjectSize  = sizeof (SFF_DOCU_HEADER);

            /*--- overread document header ---------------------------(GEI)-*/

            uLength  = uObjectSize;

            if (LineBuf.Len < uLength)
            {
                uLength     -= LineBuf.Len;
                LineBuf.Len  = 0;
                LineBuf.Next = LineBuf.Data;

                InBuf.Len   += uLength;
                InBuf.Next  += uLength;
            }
            else
            {
                LineBuf.Len  -= uLength;
                LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                mem_cpy (LineBuf.Data, LineBuf.Data + uLength, LineBuf.Len);
            }

            /*--- next receiving object is Page ----------------------(GEI)-*/

            P->port_u.Fax2.uPrevObject  = OBJECT_DOCU;
            P->port_u.Fax2.uNextObject  = OBJECT_PAGE;

            /*--- process next object --------------------------------(GEI)-*/

            break;

        /*... next receiving object is the page header ...............(GEI).*/

        case OBJECT_PAGE:
            /*--- wait on more data, if not enough available ---------(GEI)-*/

            uLength  = LineBuf.Len + (InBuf.Size - InBuf.Len);

            if (uLength < 2)
            {
                Event  = EVENT_NEEDDATA;
                break;
            }

            /*-- get page header length ------------------------------(GEI)-*/

            /* will be need for end of document detection                   */

            while (LineBuf.Len < 2)
            {
                *LineBuf.Next++  = *InBuf.Next++;
                LineBuf.Len++;
                InBuf.Len++;
            }

            bPageHeaderLen  = *(LineBuf.Data + 1);

            /*--- set object size ------------------------------------(GEI)-*/

            uObjectSize
            =
            (bPageHeaderLen == 0) ? 2 : sizeof (SFF_PAGE_HEADER);

            if (uLength < uObjectSize)
            {
                Event  = EVENT_NEEDDATA;
                break;
            }

            /*--- overread page header -------------------------------(GEI)-*/

            uLength  = uObjectSize;

            if (LineBuf.Len < uLength)
            {
                uLength     -= LineBuf.Len;
                LineBuf.Len  = 0;
                LineBuf.Next = LineBuf.Data;

                InBuf.Len   += uLength;
                InBuf.Next  += uLength;
            }
            else
            {
                LineBuf.Len  -= uLength;
                LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                mem_cpy (LineBuf.Data, LineBuf.Data + uLength, LineBuf.Len);
            }

            /*--- next receiving object is Line ----------------------(GEI)-*/

            P->port_u.Fax2.uPrevObject  = OBJECT_PAGE;
            P->port_u.Fax2.uNextObject  = OBJECT_LINE;

            /*--- process next object --------------------------------(GEI)-*/

            break;

        /*... next receiving object is a scan line ...................(GEI).*/

        case OBJECT_LINE:

            /*--- compute length of available data -------------------(GEI)-*/

            uLength  = LineBuf.Len + (InBuf.Size - InBuf.Len);

            /*--- wait on more data, if not enough available ---------(GEI)-*/

            if (uLength < 1)
            {
                Event  = EVENT_NEEDDATA;
                break;
            }

            /*--- copy recordtype into line buffer -------------------(GEI)-*/

            while (LineBuf.Len < 1)
            {
                *LineBuf.Next++  = *InBuf.Next++;
                LineBuf.Len++;
                InBuf.Len++;
            }

            /*-- get recordtype --------------------------------------(GEI)-*/

            bRecordtype  = *LineBuf.Data;

            /*--- switch on recordtype -------------------------------(GEI)-*/

            if (bRecordtype == 0)  /* recortdtype pixel row (2 byte length) */
            {
                /*--- set header length of record --------------------(GEI)-*/

                uObjHeadLen  = 3;

                /*--- wait on more data, if not enough available -----(GEI)-*/

                if (uLength < uObjHeadLen)
                {
                    Event  = EVENT_NEEDDATA;
                    break;
                }

                /*--- copy header into line buffer -------------------(GEI)-*/

                while (LineBuf.Len < uObjHeadLen)
                {
                    *LineBuf.Next++  = *InBuf.Next++;
                    LineBuf.Len++;
                    InBuf.Len++;
                }

                /*--- set object size --------------------------------(GEI)-*/

                uObjDataLen  = (*(LineBuf.Data + 2) << 8) |
                                *(LineBuf.Data + 1);
                uObjectSize  = uObjHeadLen + uObjDataLen;
            }
            else                   /* recortdtype pixel row (1 byte length) */
            if (bRecordtype >= 1 && bRecordtype <= 216)
            {
                /*--- set header length of record --------------------(GEI)-*/

                uObjHeadLen  = 1;

                /*--- set object size --------------------------------(GEI)-*/

                uObjDataLen  = bRecordtype;
                uObjectSize  = uObjHeadLen + uObjDataLen;
            }
            else                                 /* recortdtype empty lines */
            if (bRecordtype >= 217 && bRecordtype <=253)
            {
                /*--- set header length of record --------------------(GEI)-*/

                uObjHeadLen  = 1;

                /*--- set object size --------------------------------(GEI)-*/

                uObjDataLen  = 0;
                uObjectSize  = uObjHeadLen + uObjDataLen;

                /*--- debug ------------------------------------------(GEI)-*/

                DBG_TRC(("[%p:%s] fax2InRcvDATA: obj line %d rcved",
                         P->Channel, P->Name, bRecordtype))

                /*--- overread byte ----------------------------------(GEI)-*/

                LineBuf.Len--;
                LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                mem_cpy (LineBuf.Data, LineBuf.Data + 1, LineBuf.Len);

                break;
            }
            else                                 /* recordtype page header */
            if (bRecordtype == 254)
            {
                /*--- next receiving object is Page ------------------(GEI)-*/

                P->port_u.Fax2.uPrevObject  = OBJECT_LINE;
                P->port_u.Fax2.uNextObject  = OBJECT_PAGE;

                break;
            }
            else                             /* recordtype user information */
            {
                /*--- set header length of record --------------------(GEI)-*/

                uObjHeadLen  = 2;

                /*--- wait on more data, if not enough available -----(GEI)-*/

                if (uLength < uObjHeadLen)
                {
                    Event  = EVENT_NEEDDATA;
                    break;
                }

                /*--- copy header into line buffer -------------------(GEI)-*/

                while (LineBuf.Len < uObjHeadLen)
                {
                    *LineBuf.Next++  = *InBuf.Next++;
                    LineBuf.Len++;
                    InBuf.Len++;
                }

                /*--- set object size --------------------------------(GEI)-*/

                uObjDataLen  = *(LineBuf.Data + 1);
                uObjectSize  = uObjHeadLen + uObjDataLen;

                /*--- switch on length -------------------------------(GEI)-*/

                if (uObjDataLen == 0)   /* illegal line coding */
                {
                    /*--- overread the two bytes ---------------------(GEI)-*/

                    LineBuf.Len  -= uObjHeadLen;
                    LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                    mem_cpy
                    (
                        LineBuf.Data,
                        LineBuf.Data + uObjHeadLen,
                        LineBuf.Len
                    );

                    break;
                }
                else                    /* user information */
                {
                    /*--- wait on more data, if not enough available -(GEI)-*/

                    if (uLength < uObjectSize)
                    {
                        Event  = EVENT_NEEDDATA;
                        break;
                    }

                    /*--- overread user information ------------------(GEI)-*/

                    uLength  = uObjectSize;

                    if (LineBuf.Len < uLength)
                    {
                        uLength     -= LineBuf.Len;
                        LineBuf.Len  = 0;
                        LineBuf.Next = LineBuf.Data;

                        InBuf.Len   += uLength;
                        InBuf.Next  += uLength;
                    }
                    else
                    {
                        LineBuf.Len  -= uLength;
                        LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                        mem_cpy
                        (
                            LineBuf.Data,
                            LineBuf.Data + uLength,
                            LineBuf.Len
                        );
                    }
                }

                break;
            }

            /*--- overread line header -------------------------------(GEI)-*/

            uLength = uObjHeadLen;

            if (LineBuf.Len < uLength)
            {
                uLength     -= LineBuf.Len;
                LineBuf.Len  = 0;
                LineBuf.Next = LineBuf.Data;

                InBuf.Len   += uLength;
                InBuf.Next  += uLength;
            }
            else
            {
                LineBuf.Len  -= uLength;
                LineBuf.Next  = LineBuf.Data + LineBuf.Len;

                mem_cpy
                (
                    LineBuf.Data,
                    LineBuf.Data + uLength,
                    LineBuf.Len
                );
            }

            /*--- do not accumulate line data - might become too big -(GEI)-*/

            P->port_u.Fax2.uObjDataLen = uObjDataLen;
            P->port_u.Fax2.uNextObject = OBJECT_LINE_DATA;

            /*--- enqueue end of line sequence into receive queue ----(GEI)-*/

            fax2RxEnqueue (P, Cl2Eol, sizeof (Cl2Eol), TRUE);

            break;

        case OBJECT_LINE_DATA:

            /*--- compute length of available data -------------------(GEI)-*/

            uLength  = LineBuf.Len + (InBuf.Size - InBuf.Len);

            /*--- enqueue data of linebuffer into receive queue ------(GEI)-*/

            if (uLength > P->port_u.Fax2.uObjDataLen)
            {
                uLength = P->port_u.Fax2.uObjDataLen;
            }
            P->port_u.Fax2.uObjDataLen -= uLength;

            if (LineBuf.Len != 0)
            {
                fax2RxEnqueue
                (
                    P,
                    LineBuf.Data,
                    (WORD)(LineBuf.Len),
                    TRUE
                );
            }

            uLength  -= LineBuf.Len;

            LineBuf.Len   = 0;
            LineBuf.Next  = LineBuf.Data;

            /*--- enqueue data of inbuffer into receive queue --------(GEI)-*/

            if (uLength > 0)
            {
                fax2RxEnqueue (P, InBuf.Next, (WORD) uLength, TRUE);

                InBuf.Len   += uLength;
                InBuf.Next  += uLength;
            }

            if (P->port_u.Fax2.uObjDataLen != 0)
            {
                Event  = EVENT_NEEDDATA;
                break;
            }

            /*--- count lines ----------------------------------------(GEI)-*/

            P->port_u.Fax2.uCntEols++;

            P->port_u.Fax2.uNextObject = OBJECT_LINE;

            break;

        } /* end of switch (P->port_u.Fax2.uWaiOnObject) */

    } /* end of while (Event == EVENT_NONE) */

    /*--- copy data from inbuffer to linebuffer ----------------------(GEI)-*/

    if (InBuf.Len < InBuf.Size)
    {
        uLength  = InBuf.Size - InBuf.Len; /* len of remaining data */

        mem_cpy (LineBuf.Next, InBuf.Next, uLength);

        LineBuf.Len  += uLength;
    }

    /*--- save buffer data -------------------------------------------(GEI)-*/

    P->port_u.Fax2.uLineLen  = LineBuf.Len;

    /*--- undefine macros --------------------------------------------(GEI)-*/

    #undef EVENT_NONE
    #undef EVENT_NEEDDATA

    /*--- return -----------------------------------------------------(GEI)-*/

    return (0);
}

/* ------------------------------------------------------------------------
		Receive EDATA from IDI, incoming fax receiving direction
   ------------------------------------------------------------------------ */
static VOID fax2InRcvEDATA (ISDN_PORT *P,
														BYTE **Data,
														WORD sizeData,
														byte *Stream,
														word sizeStream) {
	T30_INFO* ptLoT30Info;
	byte*     faxParam;
    BYTE      abRespond [32];

	/*--- intro ------------------------------------------------------(GEI)-*/

	ptLoT30Info  = (T30_INFO *) (*Data);

	/*--- debug ------------------------------------------------------(GEI)-*/

	DBG_TRC(("[%p:%s] fax2InRcvEDATA: Phase:%s, Code:%s",
							 P->Channel, P->Name,
							 fax_phase_name(P->At.Fax2.Phase),
							 fax_code_name(ptLoT30Info->code)))

#if USE_FAX_FULL_DEBUG
    DBG_TRC(("fax2InRcvEDATA: Phase %d, Code %02x", P->At.Fax2.Phase,
        ptLoT30Info->code))
#if USE_LOG_BLK
	if (DBG_MASK(DL_TRC|DL_BLK)) {
    	DBG_BLK((*Data, sizeData))
	}
#endif
#endif

	/*--- switch on phase and event ----------------------------------(GEI)-*/
	if ((P->At.Fax2.Phase == CL2_PHASE_A) &&
			(ptLoT30Info->code == IDI_EDATA_DTC)) {
		/*
			Received polling request from opposite side,
			prepare document transmission
			*/
		byte* TxQueue;
		dword TxSize ;
		TxSize = ((P->PortData.QOutSize / MIN_SCANLINE) + 1) * MSG_OVERHEAD ;
		if (TxSize > MAX_SCANLINES * MSG_OVERHEAD)
			TxSize = MAX_SCANLINES * MSG_OVERHEAD ;
		TxSize += P->PortData.QOutSize + BUF_OVERHEAD ;
		if (TxSize > MAX_TX_QUEUE)
			TxSize = MAX_TX_QUEUE ;
		TxQueue = 0 ; /* force realloc */
		if (portSetQueue (P, &TxQueue, &TxSize, &P->TxBuffer, 'W')) {
			queueInit (&P->TxQueue, (byte *) TxQueue, TxSize);
			portInitQOut (P, (byte *)TxQueue, P->PortData.QOutSize);
		}

		/*
			Update information from CIG and DTC fremes
			*/
		UpdateT30Info (P, ptLoT30Info, P->port_u.Fax2.abDISRemote);

		/*
			Send FCON
			*/
		faxRsp (P, "+FCON");

		if (global_options & DIVA_FAX_FORCE_SEP_SUB_PWD) {
			int   t30_length  = sizeof(T30_INFO) + ptLoT30Info->head_line_len;
			int   data_length = sizeData - t30_length;
			byte* data_start  = *Data + t30_length;

			if (data_length > 1) { /* Look for SEP */
				int length = *data_start;
				data_start++;
				data_length--;

				if (length) {
					str_cpy (Stream, "+FPA:");
					mem_cpy (&Stream[5], data_start, length);
					Stream[length+5] = 0;
					DBG_TRC(("[%p:%s] detected SEP<%s>", P->Channel, P->Name, &Stream[5]))
					data_length -= length;
					data_start  += length;
					faxRsp (P, Stream);
				}

				if (data_length > 1) { /* Look for PWD */
					length = *data_start;
					data_start++;
					data_length--;

					if (length) {
						str_cpy (Stream, "+FPW:");
						mem_cpy (&Stream[5], data_start, length);
						Stream[length+5] = 0;
						DBG_TRC(("[%p:%s] detected PWD<%s>", P->Channel, P->Name, &Stream[5]))
						data_length -= length;
						data_start  += length;
						faxRsp (P, Stream);
					}
				}
			}
		}

		/*
			Send +FCIG and polling side ID
			*/
		sprintf (Stream, (byte*)"+FCIG:%s" , P->port_u.Fax2.acIDRemote);
		faxRsp (P, Stream);

		/*
			Send DTC
			*/
		str_cpy ((char*)Stream, "+FDTC: ");
		fax2ParamBufToStr (P->port_u.Fax2.abDISRemote,(char*)(Stream + str_len ((char*)Stream)));
		faxRsp (P, Stream);

		/*
			Send OK
			*/

		atRsp (P, R_OK);

		/*
			now switch to the transmit mode
			*/
		P->Direction = P_CONN_OUT;
		P->At.Fax2.Phase  = CL2_PHASE_B;
		P->At.Fax2.PollingRequestActive = 1;

	} else if ((P->At.Fax2.Phase == CL2_PHASE_A) &&
			(ptLoT30Info->code == IDI_EDATA_TRAIN_OK)) {
	/*... received training ok of 1st document .......................(GEI).*/
	/*--- process getting t30 data -------------------------------(GEI)-*/

		UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);

	/*--- send response '+FCON' ----------------------------------(GEI)-*/

		faxRsp (P, "+FCON");

	/*
		process pre-message negotiations, if allowed
		*/

		if ((!P->At.Fax2.RcvSEP_PWD_delivered) &&
				(global_options & DIVA_FAX_FORCE_SEP_SUB_PWD)) {
			int   t30_length  = sizeof(T30_INFO) + ptLoT30Info->head_line_len;
			int   data_length = sizeData - t30_length;
			byte* data_start  = *Data + t30_length;

			P->At.Fax2.RcvSEP_PWD_delivered = 1; /* delivery only one time */

			if (data_length > 1) { /* Look for SUB */
				int length = *data_start;
				data_start++;
				data_length--;

				if (length) {
					str_cpy (Stream, "+FSA:");
					mem_cpy (&Stream[5], data_start, length);
					Stream[length+5] = 0;
					DBG_TRC(("[%p:%s] detected SUB<%s>", P->Channel, P->Name, &Stream[5]))
					data_length -= length;
					data_start  += length;
					faxRsp (P, Stream);
				}

				if (data_length > 1) { /* Look for PWD */
					length = *data_start;
					data_start++;
					data_length--;

					if (length) {
						str_cpy (Stream, "+FPW:");
						mem_cpy (&Stream[5], data_start, length);
						Stream[length+5] = 0;
						DBG_TRC(("[%p:%s] detected PWD<%s>", P->Channel, P->Name, &Stream[5]))
						data_length -= length;
						data_start  += length;
						faxRsp (P, Stream);
					}
				}
			}
		}

	/*--- send responds '+FTSI:' ---------------------------------(GEI)-*/

		sprintf (Stream, (byte*)"+FTSI:%s" , P->port_u.Fax2.acIDRemote);
		faxRsp (P, Stream);

	/*--- send responds '+FDCS:' ---------------------------------(GEI)-*/

		str_cpy ((char*)Stream, "+FDCS: ");
		faxParam = P->At.Fax2.DCS;
		fax2ParamBufToStr (faxParam ,(char*) Stream + str_len ((char*)Stream));
		faxRsp (P, Stream);

	/*--- send responds 'OK' -------------------------------------(GEI)-*/

		atRsp (P, R_OK);

	/*--- enter phase B ------------------------------------------(GEI)-*/

		P->At.Fax2.Phase  = CL2_PHASE_B;

	/*... received training ok of another document ...................(GEI).*/
	} else if ((P->At.Fax2.Phase == CL2_PHASE_B) &&
						 (ptLoT30Info->code == IDI_EDATA_TRAIN_OK)) {
	/*--- process getting t30 data -------------------------------(GEI)-*/

		UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);

	/*--- send responds '+FDCS:' ---------------------------------(GEI)-*/

		str_cpy ((char*)Stream, "+FDCS: ");
		fax2ParamBufToStr (P->At.Fax2.DCS, (char*)(Stream + str_len ((char*)Stream)));
		faxRsp (P, Stream);

		/*--- send responds 'OK' -------------------------------------(GEI)-*/

		atRsp (P, R_OK);

    /*... received end  of page ......................................(GEI).*/
	} else if (P->At.Fax2.Phase == CL2_PHASE_C) {
		/*--- switch on code -----------------------------------------(GEI)-*/

		switch (ptLoT30Info->code) {
			case IDI_EDATA_MPS:
				P->port_u.Fax2.bET  = 0;
				fax2RcvEop (P, ptLoT30Info);
				break;

			case IDI_EDATA_EOM:
				P->port_u.Fax2.bET  = 1;
				fax2RcvEop (P, ptLoT30Info);
				break;

			case IDI_EDATA_EOP:
				P->port_u.Fax2.bET  = 2;
				fax2RcvEop (P, ptLoT30Info);
				break;

			case IDI_EDATA_DCS:
				/*
					Receiverd DCS in the phase C, also some thing
					gone wrong and session provess with fallback
					to phase B, but we stille here and wait until
					IDI_EDATA_TRAIN_OK
					*/
				DBG_TRC(("[%p:%s] W: fax2InRcvEDATA: DCS, wait for TRAIN_OK",
										 P->Channel, P->Name))
				UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);
				break;

			case IDI_EDATA_TRAIN_OK: {
				T30_INFO tLoT30Info;

				DBG_TRC(("[%p:%s] W: fax2InRcvEDATA: TRAIN_OK, send spoofed +FDR",
										 P->Channel, P->Name))

				UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);
				T30InfoSet (P, &tLoT30Info);
				fax2Xmit (P, (BYTE *) &tLoT30Info, sizeof (tLoT30Info), N_EDATA);

				/*--- if EOM report parameters and enter data state ------(GEI)-*/

				if (P->port_u.Fax2.bET == 1)
				{
					/*--- send responds '+FCFR' --------------------------(GEI)-*/

					faxRsp (P, "+FCFR");

					/*--- send responds '+FDCS:' -------------------------(GEI)-*/

					str_cpy (abRespond, "+FDCS: ");
					fax2ParamBufToStr (P->At.Fax2.DCS, abRespond + str_len (abRespond));
					faxRsp (P, abRespond);

					/*--- send responds 'CONNECT' ------------------------(GEI)-*/

					faxRsp (P, "CONNECT");

					/*--- switch to data mode ----------------------------(JFR)-*/

					P->State = P_DATA;
				}
			} break;

			default:
				DBG_TRC(("[%p:%s] W: fax2InRcvEDATA: Phase:C, Code:%02x unhandled",
										 P->Channel, P->Name, ptLoT30Info->code))
		}
	} else {
		DBG_TRC(("[%p:%s] W: fax2InRcvEDATA: Phase:?, Code:%02x unhandled",
								 P->Channel, P->Name, ptLoT30Info->code))
	}

	/*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

static VOID
fax2OutRcvEDATA (ISDN_PORT *P,
 				 BYTE **Data, WORD sizeData, BYTE *Stream, WORD sizeStream)
{
    T30_INFO *              ptLoT30Info;
    BYTE                    abRespond [32];
    
    /*--- intro ------------------------------------------------------(GEI)-*/

    ptLoT30Info  = (T30_INFO *) (*Data);

    /*--- debug ------------------------------------------------------(GEI)-*/
		DBG_TRC(("[%p:%s] fax2OutRcvEDATA: Phase:%s, Code:%s",
								 P->Channel, P->Name,
								 fax_phase_name(P->At.Fax2.Phase),
								 fax_code_name(ptLoT30Info->code)))

#if USE_FAX_FULL_DEBUG
    DBG_TRC(("fax2OutRcvEDATA: Phase %d, Code %d", P->At.Fax2.Phase,
        ptLoT30Info->code))
#if USE_LOG_BLK
	if (DBG_MASK(DL_TRC|DL_BLK)) {
    	//DBG_BLK((*Data, sizeData))
	}
#endif
#endif

    /*--- switch on phase --------------------------------------------(GEI)-*/

    if (P->At.Fax2.Phase == CL2_PHASE_A)
    {
        /*--- process getting t30 data -------------------------------(GEI)-*/

        UpdateT30Info (P, ptLoT30Info, P->port_u.Fax2.abDISRemote);

        /*--- send response '+FCON' ----------------------------------(GEI)-*/

        faxRsp (P, "+FCON");

        /*--- send responds '+FCSI:' ---------------------------------(GEI)-*/
    
        sprintf (Stream, (byte*)"+FCSI:\"%s\"" , P->port_u.Fax2.acIDRemote);
        faxRsp (P, Stream);

        /*--- send responds '+FDIS:' ---------------------------------(GEI)-*/

        str_cpy ((char*)Stream, "+FDIS: ");
        fax2ParamBufToStr (P->port_u.Fax2.abDISRemote,(char*)(Stream + str_len ((char*)Stream)));
        faxRsp (P, Stream);

        /*--- send responds 'OK' -------------------------------------(GEI)-*/

        atRsp (P, R_OK);

        /*--- enter phase B ------------------------------------------(GEI)-*/

        P->At.Fax2.Phase  = CL2_PHASE_B;
    } else if ((P->At.Fax2.Phase == CL2_PHASE_B) &&
               (ptLoT30Info->code == IDI_EDATA_TRAIN_OK)) {

        if (P->At.Fax2.PollingRequestActive == 1)
        {
            UpdateT30Info (P, ptLoT30Info, P->port_u.Fax2.abDISRemote);
            fax2OutRcvX_CONNECT_ACK (P, Data, sizeData, Stream, sizeStream);
        }
        else
        {
            /*--- process getting t30 data ---------------------------(GEI)-*/

            UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);

            /*--- send responds '+FDCS:' -----------------------------(GEI)-*/

            str_cpy (Stream, "+FDCS: ");
            fax2ParamBufToStr (P->At.Fax2.DCS, Stream + sizeof ("+FDCS: ") - 1);
            faxRsp (P, Stream);

            /*--- enter phase C --------------------------------------(GEI)-*/

            P->At.Fax2.Phase  		= CL2_PHASE_C;
            P->port_u.Fax2.uCntEols   	= 0;
            P->port_u.Fax2.uCntEolsV  	= 0;

            /*--- send respond 'CONNECT' and send XON ----------------(GEI)-*/

            faxRsp (P, "CONNECT");
            portFaxXon (P);	/* respect port DCB flow control settings */

            /*--- switch to data mode --------------------------------(JFR)-*/

            P->State = P_DATA;
        }

    } else if (P->At.Fax2.Phase == CL2_PHASE_D) {

        if (ptLoT30Info->code == IDI_EDATA_MCF)
        {
            /*--- send responds '+FPTS:' -----------------------------(GEI)-*/

            UpdatePTSInfo (P, ptLoT30Info, "fax2OutRcvEDATA, PHASE_D, MCF");

            sprintf (abRespond, "+FPTS: %d", P->port_u.Fax2.bPTS);

            faxRsp (P, abRespond);

            /*--- switch on value of last ET cmd ---------------------(GEI)-*/

            switch (P->port_u.Fax2.bET)
            {
            /*... new page ...........................................(GEI).*/

            case 0:

                /*--- comment ----------------------------------------(GEI)-*/

                /* stay in phase D, wait on cmd '+FDT' */

                /*--- send responds 'OK' -----------------------------(GEI)-*/

                atRsp (P, R_OK);

                break;

            /*... new document/parameter change ......................(GEI).*/

            case 1:

                /* stay in phase D, wait for the IDI_EDATA_DIS */

                break;

            /*... session end and all other ..........................(GEI).*/

            case 2:
            default:

    		/*--- send hangup response, reset variables and drop connection */

                fax2Drop (P, CAUSE_DROP);

                break;
            }
        }
        else if (ptLoT30Info->code == IDI_EDATA_DIS)
        {
            /*--- enter phase B --------------------------------------(GEI)-*/

            P->At.Fax2.Phase  = CL2_PHASE_B;

            /*--- send responds 'OK' ---------------------------------(GEI)-*/

            atRsp (P, R_OK);
        }
    }

    /*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

static VOID
fax2OutRcvX_CONNECT_ACK (ISDN_PORT *P,
  						 BYTE **Data, WORD sizeData,
						 BYTE *Stream, WORD sizeStream)
{
    BYTE                    abLoBuffer
                            [
                                sizeof (SFF_DOCU_HEADER)
                                +
                                sizeof (SFF_PAGE_HEADER)
                            ];
    T30_INFO *              ptLoT30Info;
    SFF_PAGE_HEADER *       ptLoPh;

    /*--- intro ------------------------------------------------------(GEI)-*/

    ptLoT30Info  = (T30_INFO *) (*Data);

    /*--- debug ------------------------------------------------------(GEI)-*/

#if USE_FAX_FULL_DEBUG
    DBG_TRC(("fax2OutRecvX_CONNECT_ACK: %d", sizeData))
#if USE_LOG_BLK
	if (DBG_MASK(DL_TRC|DL_BLK)) {
    	//DBG_BLK((*Data, sizeData))
	}
#endif
#endif

    /*--- switch on phase --------------------------------------------(GEI)-*/

    if (P->At.Fax2.Phase == CL2_PHASE_B)
    {
        /*--- process getting t30 data -------------------------------(GEI)-*/

        UpdateT30Info (P, ptLoT30Info, P->At.Fax2.DCS);

        /*--- send responds '+FDCS:' ---------------------------------(GEI)-*/

        str_cpy ((char*)Stream, "+FDCS: ");
        fax2ParamBufToStr (P->At.Fax2.DCS, (char*)(Stream + sizeof ("+FDCS: ") - 1));
        faxRsp (P, Stream);

        /*--- send document and page header --------------------------(GEI)-*/

		ptLoPh  = (PSFF_PAGE_HEADER) (abLoBuffer + sizeof (SFF_DOCU_HEADER));

        InitDocuheader (P->At.Fax2.DCS, (PSFF_DOCU_HEADER) abLoBuffer);
        InitPageheader (P, P->At.Fax2.DCS, ptLoPh);

        fax2Xmit (P, abLoBuffer, sizeof (abLoBuffer), N_DATA);

        /*--- enter phase C ------------------------------------------(GEI)-*/

        P->At.Fax2.Phase  		= CL2_PHASE_C;
    	P->port_u.Fax2.uCntEols   	= 0;
    	P->port_u.Fax2.uCntEolsV  	= 0;

        /*--- send respond 'CONNECT' and send XON --------------------(GEI)-*/

        faxRsp (P, "CONNECT");
        portFaxXon (P);	/* respect port DCB flow control settings */

        /*--- switch to data mode ------------------------------------(JFR)-*/

        P->State = P_DATA;
    }

    /*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

void fax2Terminate (ISDN_PORT *P)
{	/* called from faxDrop()or faxHangup(), send responses and clean up */

    byte acRespond [20+1];
		byte original_phase = P->At.Fax2.Phase;
    
    /*--- exit phase C, if is active ---------------------------------(GEI)-*/

    if (P->At.Fax2.Phase == CL2_PHASE_C)
    {
        P->State  = P_ESCAPE;

        if (P->Direction == P_CONN_IN)
        {
            /*--- enqueue RTC sequence into receive queue ------------(GEI)-*/

            fax2RxEnqueue (P, Cl2Rtc, sizeof (Cl2Rtc), TRUE);

            /*--- back to command mode -------------------------------(GEI)-*/

            fax2RxEnqueue (P, Cl2Eop, sizeof (Cl2Eop), FALSE);
        }
        else
        if (P->Direction == P_CONN_OUT)
        {
            atRspStr (P, (byte*)"\x18");
        }
    }

    /*--- enter phase E ----------------------------------------------(GEI)-*/

    P->At.Fax2.Phase  = CL2_PHASE_E;

		diva_update_fax_speed_statistics (P->port_u.Fax2.remote_speed*2400, (word)P->At.Fax2.Error);

		if ((original_phase == CL2_PHASE_A) &&
				(P->Direction == P_CONN_OUT) && (P->At.Fax2.Error == 10)) {
			atRsp (P, R_NO_CARRIER);
		} else {
    /*--- send responds '+FHNG:' -------------------------------------(GEI)-*/
			sprintf (acRespond, (byte*)"+FHNG: %d" , P->At.Fax2.Error);
			faxRsp (P, acRespond);
			atRsp (P, R_OK);
		}

    /*--- enter phase IDLE -------------------------------------------(GEI)-*/

    P->At.Fax2.Phase  = CL2_PHASE_IDLE;

    /*--- reinitialize fax modem -------------------------------------(GEI)-*/

    fax2Init (P);
}

/****************************************************************************/

static void fax2Drop (ISDN_PORT *P, byte Cause)
{	/* Called from different functions in this module to terminate	*/
	/* a connection gracefully.										*/
    fax2Terminate (P);
    atDrop (P, Cause);
}

/****************************************************************************/

int fax2Hangup (ISDN_PORT *P)
{
	/* Called by atDtrOff() or portDown() if a successful connection which	*/
	/* was indicated via fax1Up() before is disconnected.					*/
	/* For an inbound call a N_DISC indication is raised when the DCN frame	*/
	/* was seen, i.e. we come here on any normal termination.				*/
	/* For an outbound call we disconnect ourself on normal termination,	*/
	/* i.e. we come here only on a remote out of phase disconnect and thus	*/
	/* we report an unspecific phase A error with +FHNG: 10 !				*/

	if ( P->Direction == P_CONN_OUT ) {
		if (!P->At.Fax2.Error) {
			P->At.Fax2.Error = 10 ;
		}
	}
    fax2Terminate (P);
    return (0);	/* responses sent already */
}

/****************************************************************************/

static void
fax2RcvEop (ISDN_PORT *P, T30_INFO *ptPaT30Info)
{
    BYTE                    abRespond [32];

    /*--- enqueue RTC sequence into receive queue --------------------(GEI)-*/

    fax2RxEnqueue (P, Cl2Rtc, sizeof (Cl2Rtc), TRUE);

    /*--- put end of page sequence into receive queue ----------------(GEI)-*/

    fax2RxEnqueue (P, Cl2Eop, sizeof (Cl2Eop), FALSE);

    /*--- enter phase D ----------------------------------------------(GEI)-*/

    P->At.Fax2.Phase  = CL2_PHASE_D;
    P->State         = P_ESCAPE;

    /*--- send respond '+FPTS' ---------------------------------------(GEI)-*/

    UpdatePTSInfo (P, ptPaT30Info, "fax2RcvEop, PHASE_D");

    sprintf (abRespond, "+FPTS: %d,%d",
             P->port_u.Fax2.bPTS, P->port_u.Fax2.uCntEols);

    faxRsp (P, abRespond);

    /*--- send respond '+FET' ----------------------------------------(GEI)-*/
    sprintf (abRespond, (byte*)"+FET: %d", P->port_u.Fax2.bET);

    faxRsp (P, abRespond);

    /*--- send respond 'OK' ------------------------------------------(GEI)-*/

    atRsp (P, R_OK);

    /*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

static void
fax2OutBufSend (ISDN_PORT *P, POBJBUFFER OutBuf)
{
    /*--- send all complete lines ------------------------------------(GEI)-*/

    fax2Xmit (P, (BYTE *) OutBuf->Data, (WORD) OutBuf->Len, N_DATA);

    /*--- remain or remove uncomplete line ---------------------------(GEI)-*/

    OutBuf->Len     = 0;
    OutBuf->Next    = OutBuf->Data;

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static int
fax2Xmit (ISDN_PORT *P, byte *Data, word sizeData, word FrameType)
{
    /*--- debug ------------------------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
    DBG_TRC(("fax2Xmit %d", sizeData))

    if (P->port_u.Fax2.uCntBlocks <= DBG_WRITE_CNT)
    {
#if USE_LOG_BLK
		if (DBG_MASK(DL_TRC|DL_BLK)) {
        	DBG_BLK((Data, ((sizeData<DBG_MAX_BSIZE)?sizeData:DBG_MAX_BSIZE)))
		}
#endif
    }
#endif
    /*--- send data --------------------------------------------------(GEI)-*/

    return (portXmit (P, Data, sizeData, FrameType));
}

/****************************************************************************/

static void
fax2ParamStrToBuf (ISDN_PORT *P, byte **ppcPaCmd, byte *pbPaParam)
{
    int                     nLoValue;
    unsigned int            uLoI;
    /*char *                  pcLoCmd;*/
     byte *                 pcLoCmd;

    /*--- init variables ---------------------------------------------(GEI)-*/

    pcLoCmd  = *ppcPaCmd;

    /*--- process all parameters -------------------------------------(GEI)-*/

    for (uLoI=0; uLoI < CL2_PARAM_LEN && *pcLoCmd != '\0'; uLoI++)
    {
        /*--- get value ----------------------------------------------(GEI)-*/

				if ((uLoI == CL2_PARAM_BR) &&
						(global_options & DIVA_FAX_ALLOW_V34_CODES) &&
						(global_options & DIVA_FAX_FORCE_V34)) {
					switch (*pcLoCmd) {
						case 'a':
						case 'A':
							nLoValue  = (int)0x0a;
							pcLoCmd++;
							break;
						case 'b':
						case 'B':
							nLoValue  = (int)0x0b;
							pcLoCmd++;
							break;
						case 'c':
						case 'C':
							nLoValue  = (int)0x0c;
							pcLoCmd++;
							break;
						case 'd':
						case 'D':
							nLoValue  = (int)0x0d;
							pcLoCmd++;
							break;
						default:
							nLoValue  = atNumArg (&pcLoCmd);
					}
				} else {
					nLoValue  = atNumArg (&pcLoCmd);
				}

        /*--- assign value, if given ---------------------------------(GEI)-*/

        if (nLoValue >= 0) {
					if ((uLoI == CL2_PARAM_VR) && (global_options & DIVA_FAX_ALLOW_HIRES)) {
						byte i, val = (byte)nLoValue, mask;

						for (i = 0, mask = 0; i < sizeof(CL2VRValues)/sizeof(CL2VRValues[0]); i++) {
								mask |= CL2VRValues [i];
						}
						nLoValue = (val & mask);
					} else if (uLoI == CL2_PARAM_BR) {
						int maxBR;
						if ((global_options & DIVA_FAX_FORCE_V34) && (global_options & DIVA_FAX_ALLOW_V34_CODES)
						 && (P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport &
						     (1L << CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL))) {
							maxBR = CL2_BR_336;
						}
						else if (P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport &
						     (1L << CLASS1_RECONFIGURE_V17_14400)) {
							maxBR = CL2_BR_144;
						}
						else {
							maxBR = CL2_BR_96;
						}
						if (nLoValue > maxBR)
							nLoValue = maxBR ;
					} else {
						if (nLoValue > CL2MaxValues [uLoI])
							nLoValue  = CL2MaxValues [uLoI];
					}
					pbPaParam [uLoI]  = nLoValue;
        }

        /*--- overread all char until comma --------------------------(GEI)-*/

        while (*pcLoCmd != ',' && *pcLoCmd != '\0')
        {
            pcLoCmd++;
        }

        /*--- position ptr after comma -------------------------------(GEI)-*/

        if (*pcLoCmd == ',')
        {
            pcLoCmd++;
        }
    }

    /*--- asign return parameter -------------------------------------(GEI)-*/

    *ppcPaCmd  = pcLoCmd;

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static void
fax2ParamBufToStr (byte *pbPaParam, char *pcPaBuffer)
{
    UINT                    uLoI;
    UINT                    uLoLen;
    byte 	                *ptrsprint;
    byte                    sprintval[30] = "%u";   
    byte                    sprintvalh[30] = "%X";   

    /* convert all parameters to printable format */

    for (uLoI=0, uLoLen=0; uLoI < CL2_PARAM_LEN; uLoI++)
    {
        if (uLoLen > 0)
            pcPaBuffer [uLoLen++] = ',';

				if ((uLoI == CL2_PARAM_BR) &&
						(global_options & DIVA_FAX_ALLOW_V34_CODES) &&
						(global_options & DIVA_FAX_FORCE_V34) &&
						(pbPaParam[uLoI] > 9)) {
        	ptrsprint = sprintvalh;
				} else {
        	ptrsprint = sprintval;
				}

        uLoLen += sprintf ((byte*)&pcPaBuffer[uLoLen], ptrsprint, pbPaParam[uLoI]);
    }
}

/****************************************************************************/

static void
fax2Capabilities (BYTE *pbPaParam, char *pcPaBuffer, UINT uPaBufferSize)
{
    UINT                    uLoI;
    UINT                    uLoLen;
    byte *Val;
    byte *Buf;
    byte pcPaBufx[30] = "(0-%u)";
    byte pcPaBufhx[30] = "(0-%X)";
    byte pcPaBuf[20] = "(0,1)";

   

    /*--- process all parameters -------------------------------------(GEI)-*/

    for (uLoI=0, uLoLen=0; uLoI < CL2_PARAM_LEN; uLoI++)
    {
        /*--- add comma, if param isn't the first --------------------(GEI)-*/

        if (uLoLen != 0)
        {
            pcPaBuffer [uLoLen++]  = ',';
        }

        /*--- switch on param value  ---------------------------------(GEI)-*/

        switch (pbPaParam [uLoI])
        {
        case 0:
					/*-- put constant char into buffer -----------------------(GEI)-*/
					pcPaBuffer [uLoLen++]  = '0';
					break;

        case 1:
          /*-- put constant string into buffer ---------------------(GEI)-*/
          Val = pcPaBuf;
					uLoLen += sprintf ((byte*)&pcPaBuffer[uLoLen], pcPaBuf);
          break;

        default:
					if (uLoI == CL2_PARAM_VR) {
						byte i, val;
						for (i = 1, val = 0; (i < (sizeof(CL2VRValues)/sizeof(CL2VRValues[0]))); i++) {
							if (pbPaParam[uLoI] & CL2VRValues[i]) {
								val |= CL2VRValues[i];
							}
						}
						Buf = pcPaBufhx;
						uLoLen += sprintf ((byte*)&pcPaBuffer[uLoLen], Buf, val);
					} else if (uLoI == CL2_PARAM_BR) {
						byte val = (byte)pbPaParam[uLoI];
						Buf = pcPaBufx;

						if (val > 5) {
							if (global_options & DIVA_FAX_ALLOW_V34_CODES) {
								if (val > 9)
									Buf = pcPaBufhx;
							} else {
								val = 5;
							}
						}
						uLoLen += sprintf ((byte*)&pcPaBuffer[uLoLen], Buf, val);
					} else {
						/*-- put range into buffer -------------------------------(GEI)-*/
						Buf = pcPaBufx;
						uLoLen += sprintf ((byte*)&pcPaBuffer[uLoLen], pcPaBufx, pbPaParam[uLoI]);
					}
          break;
        }
    }

    /*--- append null terminator -------------------------------------(GEI)-*/

    pcPaBuffer [uLoLen]  = '\0';

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static VOID
InitDocuheader (BYTE *pbPaParam, PSFF_DOCU_HEADER ptPaDh)
{
    /*--- init structure with zero -----------------------------------(GEI)-*/

    mem_zero (ptPaDh, sizeof (*ptPaDh));

    /*--- set several values -----------------------------------------(GEI)-*/

    ptPaDh->ID              = 0x66666653;
    ptPaDh->Version         = 0x01;
    ptPaDh->OffFirstPage    = sizeof (SFF_DOCU_HEADER);

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static VOID
InitPageheader (ISDN_PORT* P, BYTE *pbPaParam, PSFF_PAGE_HEADER ptPaPh)
{
    /*--- init structure with zero -----------------------------------(GEI)-*/

    mem_zero (ptPaPh, sizeof (*ptPaPh));

    /*--- set several values -----------------------------------------(GEI)-*/

    ptPaPh->ID          = 254;
    ptPaPh->Length      = sizeof (SFF_PAGE_HEADER) - 2;


		if ((global_options & DIVA_FAX_ALLOW_HIRES) && (pbPaParam [CL2_PARAM_VR] & (~CL2_VR_FINE))) {
			ptPaPh->ResVert     = 0x86; /* 15.4 lines per millimeter */
		} else {
			ptPaPh->ResVert     = pbPaParam [CL2_PARAM_VR];
		}

		if ((global_options & DIVA_FAX_ALLOW_HIRES) && (pbPaParam [CL2_PARAM_VR] & CL2_VR_ULTRA_FINE)) {
			ptPaPh->ResHorz     = 0x86; /* R16, 406 Dpi */
		} else {
			ptPaPh->ResHorz     = 0; /* always 203 dpi */
		}
    ptPaPh->Coding      = Cl2ToSff_DF [pbPaParam [CL2_PARAM_DF]];

		if ((global_options & DIVA_FAX_ALLOW_HIRES) && (pbPaParam [CL2_PARAM_VR] & CL2_VR_ULTRA_FINE)) {
			ptPaPh->LineLength  = Cl2ToSff_WD_R16 [pbPaParam [CL2_PARAM_WD]];
		} else {
			ptPaPh->LineLength  = Cl2ToSff_WD [pbPaParam [CL2_PARAM_WD]];
		}

    ptPaPh->PageLength = 0;

    /*--- return -----------------------------------------------------(GEI)-*/

		DBG_TRC(("[%p:%s] InitPageheader RH:%02x, RV:%02x, LL:%d",
						P->Channel, P->Name, ptPaPh->ResHorz, ptPaPh->ResVert, ptPaPh->LineLength))

    return;
}

/****************************************************************************/

static VOID
InitPageheaderEnd (PSFF_PAGE_HEADER ptPaPh)
{
    /*--- init structure with zero -----------------------------------(GEI)-*/

    mem_zero (ptPaPh, sizeof (*ptPaPh));

    /*--- set several values -----------------------------------------(GEI)-*/

    ptPaPh->ID      = 254;
    ptPaPh->Length  = 0;

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static VOID T30InfoSet (ISDN_PORT *P, T30_INFO *ptPaT30Info) {
	word wLoControlBits = 0;

	/*--- base init --------------------------------------------------(GEI)-*/

	mem_zero (ptPaT30Info, sizeof (*ptPaT30Info));

	ptPaT30Info->atf  = IDI_ATF_MODEM;

	/*--- set values in structure ------------------------------------(GEI)-*/

	ptPaT30Info->rate           = Cl2ToIdi_BR [P->At.Fax2.DIS [CL2_PARAM_BR]];

	if (global_options & DIVA_FAX_ALLOW_HIRES) {
		byte i, val = P->At.Fax2.DIS [CL2_PARAM_VR];

		ptPaT30Info->resolution = 0;

		for (i = 0; (i < sizeof(CL2VRValues)/sizeof(CL2VRValues[0])); i++) {
			if (val & CL2VRValues[i]) {
				ptPaT30Info->resolution |= Cl2ToIdi_VR [CL2VRValues[i]];
			}
		}
	} else {
		ptPaT30Info->resolution     = Cl2ToIdi_VR [P->At.Fax2.DIS [CL2_PARAM_VR]];
	}

	ptPaT30Info->format         = IDI_FORMAT_SFF;

	ptPaT30Info->recording_properties = Cl2ToIdi_WD [P->At.Fax2.DIS [CL2_PARAM_WD]];

	if (P->Direction == P_CONN_OUT) {
		ptPaT30Info->recording_properties &= 0x0f;
		ptPaT30Info->recording_properties |= \
								(byte)(CL2ToIdi_ST[P->port_u.Fax2.abDISRemote [CL2_PARAM_ST]] << 4);
		ptPaT30Info->recording_properties |= \
								(byte)(P->port_u.Fax2.abDISRemote [CL2_PARAM_LN] << 2);
	}

	if (P->At.Fax2.DIS [CL2_PARAM_EC] == CL2_EC_ECM256)
		wLoControlBits |= T30_CONTROL_BIT_ALL_FEATURES;
	else
		if (P->At.Fax2.DIS [CL2_PARAM_EC] == CL2_EC_ECM64)
			wLoControlBits |= T30_CONTROL_BIT_ALL_FEATURES |
												T30_CONTROL_BIT_ECM_64_BYTES;

	if (P->At.Fax2.AllowIncPollingRequest) {
		wLoControlBits |= T30_CONTROL_BIT_ACCEPT_POLLING;

		if (global_options & DIVA_FAX_FORCE_SEP_SUB_PWD) {
			wLoControlBits |= (T30_CONTROL_BIT_ACCEPT_SUBADDRESS  |
												 T30_CONTROL_BIT_ACCEPT_SEL_POLLING |
												 T30_CONTROL_BIT_ACCEPT_PASSWORD);
		}
	} else if ((P->Direction == P_CONN_IN) &&
			(global_options & DIVA_FAX_FORCE_SEP_SUB_PWD)) { /* normal receive option */
		wLoControlBits |= (T30_CONTROL_BIT_ACCEPT_SUBADDRESS  |
											 T30_CONTROL_BIT_ACCEPT_PASSWORD);
	}

	ptPaT30Info->control_bits_low  = (BYTE)(wLoControlBits & 0xff);
	ptPaT30Info->control_bits_high = (BYTE)(wLoControlBits >> 8);

	/*--- set station id in structure --------------------------------(GEI)-*/

	ptPaT30Info->station_id_len  = str_len ((char*)P->At.Fax2.ID);

	mem_cpy (ptPaT30Info->station_id,
					 P->At.Fax2.ID,
					 ptPaT30Info->station_id_len);

	/*
		Header line
		*/
	ptPaT30Info->head_line_len = 0;

	/*--- debug ------------------------------------------------------(GEI)-*/

#if USE_FAX_FULL_DEBUG
#if USE_LOG_BLK
	if (DBG_MASK(DL_TRC|DL_BLK)) {
		DBG_TRC(("T30InfoSet:"))
		DBG_BLK(((byte *)ptPaT30Info, sizeof(*ptPaT30Info)))
	}
#endif
#endif

	/*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

static void UpdateT30Info (ISDN_PORT *P,
													 T30_INFO *ptPaT30Info,
													 BYTE *pbPaParam) {
	word wLoFeatureBits;
	BYTE                    i, bLoWD, vrVal;

	/*----------------------------------------------------------------(GEI)-*/

	if ((global_options & DIVA_FAX_ALLOW_V34_CODES) && (global_options & DIVA_FAX_FORCE_V34)) {
		pbPaParam [CL2_PARAM_BR]  = IdiToCl2_BR_v34 [ptPaT30Info->rate];
	} else {
		pbPaParam [CL2_PARAM_BR]  = IdiToCl2_BR [ptPaT30Info->rate];
	}

	P->port_u.Fax2.remote_speed = IdiToCl2_BR_v34 [ptPaT30Info->rate] + 1;

	bLoWD =   ptPaT30Info->recording_properties;
	bLoWD &=  T30_RECPROP_BIT_PAGE_WIDTH_MASK;
	bLoWD >>= T30_RECPROP_BIT_PAGE_WIDTH_SHIFT;
	pbPaParam [CL2_PARAM_WD] = IdiToCl2_WD [bLoWD];

	for (i = 0, vrVal = 0; (i < sizeof(IdiToCl2_VR)/sizeof(IdiToCl2_VR[0])); i++) {
		if (ptPaT30Info->resolution & IdiToCl2_VR [i]) {
			vrVal |= IdiToCl2_VR [i];
		}
	}
	pbPaParam [CL2_PARAM_VR]  = vrVal;

	pbPaParam [CL2_PARAM_ST]  = IdiToCl2_ST [(ptPaT30Info->recording_properties >> 4) & 0x0f];

	pbPaParam [CL2_PARAM_LN]  = (ptPaT30Info->recording_properties >> 2) & 0x03;

	wLoFeatureBits = (ptPaT30Info->feature_bits_high << 8) |
	                  ptPaT30Info->feature_bits_low;

	if (wLoFeatureBits & T30_FEATURE_BIT_ECM) {
		if (wLoFeatureBits & T30_FEATURE_BIT_ECM_64_BYTES)
    		pbPaParam [CL2_PARAM_EC]  = CL2_EC_ECM64;
		else
    		pbPaParam [CL2_PARAM_EC]  = CL2_EC_ECM256;
	} else
   		pbPaParam [CL2_PARAM_EC]  = CL2_EC_OFF;

	DBG_TRC(("[%p:%s] UpdateT30Info: BR:%d, RES:%d, EC:%d ST:%d LN:%d",
							 P->Channel, P->Name,
							 pbPaParam [CL2_PARAM_BR],
							 pbPaParam [CL2_PARAM_VR],
							 pbPaParam [CL2_PARAM_EC],
							 pbPaParam [CL2_PARAM_ST],
							 pbPaParam [CL2_PARAM_LN]))

	mem_cpy (P->port_u.Fax2.acIDRemote,
					 ptPaT30Info->station_id,
					 ptPaT30Info->station_id_len);

	P->port_u.Fax2.acIDRemote [ptPaT30Info->station_id_len]  = '\0';

	/*--- debug ------------------------------------------------------(GEI)-*/

#if USE_FAX_FULL_DEBUG
#if USE_LOG_BLK
	if (DBG_MASK(DL_TRC|DL_BLK)) {
		DBG_TRC(("UpdateT30Info (T30):"))
		DBG_BLK(((byte *)ptPaT30Info, sizeof (*ptPaT30Info)))
		DBG_TRC(("UpdateT30Info (CL2):"))
		DBG_BLK((pbPaParam, CL2_PARAM_LEN))
	}
#endif
#endif

	/*--- return -----------------------------------------------------(GEI)-*/

	return;
}

/****************************************************************************/

static void
UpdatePTSInfo (ISDN_PORT *P, T30_INFO *ptPaT30Info, const char* label)
{
	WORD                    wLoFeatureBits;
	WORD                    bLoCopyQuality;


    /*----------------------------------------------------------------(GEI)-*/

	wLoFeatureBits = (ptPaT30Info->feature_bits_high << 8) |
	                  ptPaT30Info->feature_bits_low;

	bLoCopyQuality =   wLoFeatureBits;
	bLoCopyQuality &=  T30_FEATURE_COPY_QUALITY_MASK;
	bLoCopyQuality >>= T30_FEATURE_COPY_QUALITY_SHIFT;
	P->port_u.Fax2.bPTS = CopyQualityToPTS [bLoCopyQuality];

	DBG_TRC(("[%p:%s] UpdatePTSInfo(%s) copy features:%04x quality:%04x",
          P->Channel, P->Name, label, wLoFeatureBits, bLoCopyQuality))

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

/****************************************************************************/

static void
fax2RxEnqueue (ISDN_PORT *P, byte *Data, word sizeData, BOOL fWithBitOrder)
{
    UINT                    uInLen;
    UINT                    uOutLen;
    BYTE                    abOutDat [20];
    BYTE                    bData;

    /*--- modify data, if it is scanline data ------------------------(GEI)-*/

    if (fWithBitOrder)
    {
        /*--- process all bytes of given data ------------------------(GEI)-*/

        uInLen      = 0;
        uOutLen     = 0;

        while (uInLen < sizeData)
        {
            /*--- enqueue data, if local buffer is full --------------(GEI)-*/

            /* the buffer must have two byte free space for dle escaping */

            if (uOutLen >= sizeof (abOutDat) - 1)
            {
#if USE_FAX_FULL_DEBUG
                if (   P->port_u.Fax2.uCntBlocks >= DBG_RECEIVE_FIRST
                    && P->port_u.Fax2.uCntBlocks <= DBG_RECEIVE_LAST)
                {
#if USE_LOG_BLK
					if (DBG_MASK(DL_TRC|DL_BLK)) {
                    	DBG_TRC(("fax2RxEnqueue: L=%d", uOutLen))
                    	DBG_BLK((abOutDat, uOutLen))
					}
#endif
                }
#endif

                portRxEnqueue (P, abOutDat, (word) uOutLen, 0);
                uOutLen  = 0;
            }

            /*--- get next byte --------------------------------------(GEI)-*/

            bData  = Data [uInLen++];

            /*--- reverse bitorder, if wanted ------------------------(GEI)-*/

            if (! P->At.Fax2.ReverseBitOrder)
            {
                bData = ReverseTable[bData];
            }

            /*--- put byte into local buffer -------------------------(GEI)-*/

            abOutDat [uOutLen++]  = bData;

            /*--- escape dle, if byte is dle -------------------------(GEI)-*/

            if (bData == DLE)
            {
                abOutDat [uOutLen++]  = DLE;
            }
        }

        /*--- enqueue data, if local buffer is full ------------------(GEI)-*/

        if (uOutLen > 0)
        {
#if USE_FAX_FULL_DEBUG
            if (   P->port_u.Fax2.uCntBlocks >= DBG_RECEIVE_FIRST
                && P->port_u.Fax2.uCntBlocks <= DBG_RECEIVE_LAST)
            {
#if USE_LOG_BLK
				if (DBG_MASK(DL_TRC|DL_BLK)){
    	            DBG_TRC(("fax2RxEnqueue: L=%d", uOutLen))
                	DBG_BLK((abOutDat, uOutLen))
				}
#endif
            }
#endif
            portRxEnqueue (P, abOutDat, (word) uOutLen, 0);
            uOutLen  = 0;
        }
    }
    else
    {
        /*--- enqueue data -------------------------------------------(GEI)-*/
#if USE_FAX_FULL_DEBUG
        if (   P->port_u.Fax2.uCntBlocks >= DBG_RECEIVE_FIRST
            && P->port_u.Fax2.uCntBlocks <= DBG_RECEIVE_LAST)
        {
#if USE_LOG_BLK
			if (DBG_MASK(DL_TRC|DL_BLK)){
            	DBG_TRC(("fax2RxEnqueue: L=%d", sizeData))
            	DBG_BLK((Data, sizeData))
			}
#endif
        }
#endif
        portRxEnqueue (P, Data, sizeData, 0);
    }

    /*--- return -----------------------------------------------------(GEI)-*/

    return;
}

static const char* fax_phase_name (byte phase) {
	static char p[16];

	switch (phase) {
		case CL2_PHASE_IDLE:
			return ("IDLE");
		case CL2_PHASE_A:
			return ("A");
		case CL2_PHASE_B:
			return ("B");
		case CL2_PHASE_C:
			return ("C");
		case CL2_PHASE_D:
			return ("D");
		case CL2_PHASE_E:
			return ("E");
	}

	sprintf (p, "?:%d", phase);

	return (p);
}

static const char* fax_code_name (byte code) {
	static char p[16];

	switch (code) {
		case IDI_EDATA_DIS:
			return ("DIS");
		case IDI_EDATA_FTT:
			return ("FTT");
		case IDI_EDATA_MCF:
			return ("MCF");
		case IDI_EDATA_DCS:
			return ("DCS");
		case IDI_EDATA_TRAIN_OK:
			return ("TRAIN_OK");
		case IDI_EDATA_EOP:
			return ("EOP");
		case IDI_EDATA_MPS:
			return ("MPS");
		case IDI_EDATA_EOM:
			return ("EOM");
		case IDI_EDATA_DTC:
			return ("DTC");
	}

	sprintf (p, "?:%02x", code);

	return (p);
}

/*
	Receive error cause from N_DISC/N_DISC_ACK
	*/
void fax2RcvNDISC (ISDN_PORT* P, byte* Data, word length) {
	if (P && Data && length) {
		if (P->Direction == P_CONN_IN) {
			byte error = 0;

	    switch (P->At.Fax2.Phase) {
				case CL2_PHASE_A: /* establish the connection */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_NO_ENERGY_DETECTED:
							/*
								Silence was detected on the line - e.g. no signal was received from the
								opposite side.
								*/
							error = 3; /* No Loop Current */
							break;
						case T30_ERR_NO_FAX_DETECTED:
						case T30_ERR_V34FAX_V8_INCOMPATIBILITY:
							/*
								Fax code reported that opposite side is not a fax device.
								(No valid frame was received).
								*/
							error = 1; /* Call answered w/o sfull fax hchk */
							break;
						case T30_ERR_NOT_IDENTIFIED:
							/*
								At least one valid fax frame was receiced,
								but exchange of DIS/DCS is failed for unknown reason.
								*/
							error = 1; /* Call answered w/o sfull fax hchk */
							break;
						default:
							error = 1;
							break;
					}
					break;

				case CL2_PHASE_B: /* negotiate session parameter */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_RETRY_NO_PAGE_RECEIVED:
						case T30_ERR_RETRY_NO_DCS_AFTER_EOM:
						case T30_ERR_RETRY_NO_MCF_AFTER_EOM:
						case T30_ERR_SUPERVISORY_TIMEOUT:
						case T30_ERR_TOO_MANY_TRAINS:
						case T30_ERR_NOT_IDENTIFIED:
						case T30_ERR_DROP_BELOW_MIN_SPEED:
						case T30_ERR_MAX_OVERHEAD_EXCEEDED:
							error = 74;
							break;
						case T30_ERR_UNEXPECTED_MESSAGE:
						case T30_ERR_INVALID_COMMAND_FRAME:
						case T30_ERR_TIMEOUT_COMMAND_TOO_LONG:
						case T30_ERR_RETRY_NO_DCS_AFTER_FTT:
						case T30_ERR_RETRY_NO_DCN_AFTER_MCF:
						case T30_ERR_RETRY_NO_DCN_AFTER_RTN:
							error = 72;
							break;
						case T30_ERR_TOO_MANY_REPEATS:
						case T30_ERR_TIMEOUT_RESPONSE_TOO_LONG:
						case T30_ERR_RETRY_NO_DCS_AFTER_MPS:
						case T30_ERR_RETRY_NO_CFR:
						case T30_ERR_RETRY_NO_MCF_AFTER_EOP:
						case T30_ERR_RETRY_NO_MCF_AFTER_MPS:
							error = 71;
							break;
						case T30_ERR_TIMEOUT_NO_COMMAND:
						case T30_ERR_RETRY_NO_COMMAND:
						case T30_ERR_TIMEOUT_NO_RESPONSE:
						case T30_ERR_V34FAX_NO_REACTION_ON_MARK:
							error = 73;
							break;

						default:
							error = 70;
							break;
					}
					break;

				case CL2_PHASE_C: /* transfer one page */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_TOO_LONG_SCAN_LINE:
							error = 91;
							break;
						case T30_ERR_UNSUPPORTED_PAGE_CODING:
						case T30_ERR_INVALID_PAGE_CODING:
						case T30_ERR_INCOMPATIBLE_PAGE_CONFIG:
						case T30_ERR_RECEIVE_CORRUPTED:
						case T30_ERR_V34FAX_KNOWN_ECM_BUG:
							error = 94;
							break;
						case T30_ERR_TIMEOUT_FROM_APPLICATION:
							error = 93;
							break;

						default:
							error = 90;
							break;
					}
					break;
				case CL2_PHASE_D: /* post page exchange */
				case CL2_PHASE_E: /* hang up the connection */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_TIMEOUT_NO_COMMAND:
						case T30_ERR_INVALID_COMMAND_FRAME:
						case T30_ERR_TIMEOUT_COMMAND_TOO_LONG:
							error = 102;
							break;
						case T30_ERR_TIMEOUT_RESPONSE_TOO_LONG:
							error = 101;
							break;

						default:
							error = 100;
							break;
					}
					break;
			}

			DBG_TRC(("[%p:%s] fax2RcvNDISC T30:%d -> FAX2:%d", P->Channel, P->Name, (int)*Data, (int)error))

			P->At.Fax2.Error = error;
		} else if (P->Direction == P_CONN_OUT) {
			byte error = 0;

	    switch (P->At.Fax2.Phase) {
				case CL2_PHASE_A: /* establish the connection */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_NO_ENERGY_DETECTED:
							/*
								Silence was detected on the line - e.g. no signal was received from the
								opposite side.
								*/
							error = 11; /* We can not use "No Loop Current" - the call was issued
							               and it will cause charging */
							break;
						case T30_ERR_NO_FAX_DETECTED:
						case T30_ERR_V34FAX_V8_INCOMPATIBILITY:
							/*
								Fax code reported that opposite side is not a fax device.
								(No valid fax frame was received from opposite side).
								*/
							error = 11; /* No answer or no DIS receiced (T.30 T1 t/o) */
							break;
						case T30_ERR_NOT_IDENTIFIED:
							/*
								At least one valid frame was received, but exchange of DIS/DCS is
								failed for unknown reason.
								*/
							error = 1; /* Call answered w/o sfull fax hchk */
							break;

						default:
							error = 10;
							break;
					}
					break;

				case CL2_PHASE_B: /* negotiate session parameter */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_UNEXPECTED_MESSAGE:
							error = 23;
							break;
						case T30_ERR_TIMEOUT_COMMAND_TOO_LONG:
						case T30_ERR_INVALID_COMMAND_FRAME:
							error = 22;
							break;
						case T30_ERR_V34FAX_TRAINING_TIMEOUT:
						case T30_ERR_V34FAX_UNEXPECTED_V21:
						case T30_ERR_ALL_RATES_FAILED:
						case T30_ERR_TOO_MANY_TRAINS:
						case T30_ERR_DROP_BELOW_MIN_SPEED:
						case T30_ERR_MAX_OVERHEAD_EXCEEDED:
							error = 27;
							break;
						case T30_ERR_INCOMPATIBLE_DIS:
						case T30_ERR_DTC_UNSUPPORTED:
						case T30_ERR_V34FAX_TURNAROUND_POLLING:
							error = 21;
							break;
						case T30_ERR_TIMEOUT_RESPONSE_TOO_LONG:
						case T30_ERR_TIMEOUT_NO_RESPONSE:
							error = 24;
							break;
						case T30_ERR_TOO_MANY_REPEATS:
							error = 25;
							break;
						case T30_ERR_SUPERVISORY_TIMEOUT:
							error = 26;
							break;

						default:
							error = 20;
							break;
					}
					break;

				case CL2_PHASE_C: /* transfer one page */
					switch (*Data) {
						case T30_SUCCESS:
							break;

						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;

						case T30_ERR_TIMEOUT_FROM_APPLICATION:
							error = 43;
							break;
/*
						case T30_ERR_UNSUPPORTED_PAGE_CODING:
							error = 44;
							break;
						case T30_ERR_INVALID_PAGE_CODING:
							error = 45;
							break;
						case T30_ERR_INCOMPATIBLE_PAGE_CONFIG:
							error = 47;
							break;
*/
						default:
							error = 40;
							break;
					}
					break;
				case CL2_PHASE_D: /* post page exchange */
				case CL2_PHASE_E: /* hang up the connection */
					switch (*Data) {
						case T30_SUCCESS:
							break;
						case T30_ERR_APPLICATION_DISC:
							error = 2;
							break;
						case T30_ERR_TIMEOUT_RESPONSE_TOO_LONG:
						case T30_ERR_TIMEOUT_NO_RESPONSE:
							error = 51;
							break;
						case T30_ERR_RETRY_NO_DCS_AFTER_MPS:
						case T30_ERR_RETRY_NO_MCF_AFTER_MPS:
							error = 52;
							break;
						case T30_ERR_RETRY_NO_MCF_AFTER_EOP:
							error = 54;
							break;
						case T30_ERR_RETRY_NO_DCS_AFTER_EOM:
						case T30_ERR_RETRY_NO_MCF_AFTER_EOM:
							error = 56;
							break;

						default:
							error = 50;
							break;
					}
					break;
			}

			if (error) {
				DBG_TRC(("[%p:%s] fax2RcvNDISC T30:%d -> FAX2:%d", P->Channel, P->Name, (int)*Data, (int)error))
				P->At.Fax2.Error = error;
			}
		}
	}
}

const char* get_stripped_fax_id (const char* fax_id, int* length) {
	int id_length      = str_len ((char*)fax_id);
	const char* start  = fax_id;
	const char* end    = &fax_id[(id_length > 0) ? (id_length-1) : 0];

	while (*start && *start == ' ') {
		start++;
	}
	while (*start && end >= start && *end == ' ') {
		end--;
	}
	if (*start && end >= start) {
		*length = (int)(end - start + 1);
	} else {
		start    = " ";
		*length  = 1;
	}

	return (start);
}

/*
	Print header line to buffer

	start points to the start of buffer
	fmt points to the format line
	max_length - output buffer length

	Procedure continues until:
		end of format line is reached
		end of output buffer is reached
		unescaped '\'' characted is reached

	Return amount of characters read from format line
	pages_requested is set to true if pages format was detected
	pwritten is set to amount of bytes written to the ouput buffer
	*/
static int print_page_header (ISDN_PORT* P,
															byte* start,
															char* fmt,
															int max_length,
															int* pages_requested,
															int* pwritten) {
	int written = 0, fmt_length = 0;
	unsigned long int value;
	int flags;
	int dprec;
	int width;
	int prec;
	char sign;
	int ch;
	int n;
	char *cp;
	int realsz;		/* field size expanded by dprec */
	int size;		/* size of converted field or string */
#define DIVA_KPRINTF_BUFSIZE	(4*sizeof(value) * 8 / 3 + 2)
	char buf[DIVA_KPRINTF_BUFSIZE];

#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')
#define	LADJUST		0x004
#define	ZEROPAD		0x080

#define KPRINTF_PUTCHAR(__x__) do { \
																			written++; \
																			if (start && written > max_length) { \
																				*pwritten = written - 1; \
																				return (fmt_length); \
																			} \
																			if (start) { *start++ = (__x__); } else { int c; c = (__x__); } \
																	} while(0)

	for (;;) {
		while (*fmt != '%' && *fmt) {
			if (*fmt == '\'') {
				*pwritten = written;
				return (fmt_length);
			}
			KPRINTF_PUTCHAR(*fmt++);
			fmt_length++;
		}

		if (*fmt == 0) {
			*pwritten = written;
			return (fmt_length);
		}

		/*
			Scip over '%'
			*/
		fmt++;
		fmt_length++;

		/*
			Init
			*/
		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';
		value = 0;

rflag:
	ch = *fmt++;
	fmt_length++;
reswitch:	switch (ch) {
		case ' ':
			if (!sign)
				sign = ' ';
			goto rflag;
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '0':
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
				fmt_length++;
			} while (is_digit(ch));
			width = n;
			goto reswitch;

		case 'D':
		case 'd':
			value = P->At.Fax2.day;
			goto nosign;
		case 'h':
		case 'H':
			value = P->At.Fax2.hour;
			goto nosign;
		case 'i':
		case 'I':
			value = P->At.Fax2.hour >= 12 ? (P->At.Fax2.hour - 12) : (P->At.Fax2.hour + 1);
			goto nosign;
		case 'm':
			value = P->At.Fax2.month;
			goto nosign;
		case 'M':
			value = P->At.Fax2.minute;
			goto nosign;
		case 's':
		case 'S':
			value = P->At.Fax2.second;
			goto nosign;
		case 'y':
			value = P->At.Fax2.year;
			if (value >= 2000) {
				value -= 2000;
			} else {
				value -= 1900;
			}
			goto nosign;
		case 'Y':
			value = P->At.Fax2.year;
			goto nosign;
		case 'P':
			*pages_requested = 1;
			goto ignore;
		case 'p':
			cp = P->At.Fax2.hour >= 12 ? "pm" : "am";
			size = str_len(cp);
			sign = '\0';
			break;
		case 'r':
		case 'R':
			cp = (char*)get_stripped_fax_id (&P->port_u.Fax2.acIDRemote[0], &size);
			break;
		case 't':
		case 'T':
			cp = (char*)get_stripped_fax_id (&P->At.Fax2.ID[0], &size);
			break;
		
nosign:
		sign = '\0';
		if ((dprec = prec) >= 0)
			flags &= ~ZEROPAD;

		cp = buf + DIVA_KPRINTF_BUFSIZE;
		if (value != 0 || prec != 0) {
			while (value >= 10) {
				*--cp = (char)to_char(value % 10);
				value /= 10;
			}
			*--cp = (char)to_char(value);
		}
		size = buf + DIVA_KPRINTF_BUFSIZE - cp;
		break;

	default:
		if (ch == '\0') {
			*pwritten = written;
			return (fmt_length);
		}
		cp = buf;
		*cp = (char)ch;
		size = 1;
		sign = '\0';
		break;
	}

		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;

		if ((flags & (LADJUST|ZEROPAD)) == 0) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

		if (sign)
			KPRINTF_PUTCHAR(sign);

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR('0');
		}

		n = dprec - size;
		while (n-- > 0)
			KPRINTF_PUTCHAR('0');

		while (size--)
			KPRINTF_PUTCHAR(*cp++);
		/* left-adjusting padding (always blank) */
		if (flags & LADJUST) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

ignore:
		*pwritten = (*pwritten != 0) ? 1 : 0;
	}

	*pwritten = written;

	return (fmt_length);
}

/*
	Using current settings and information from ptPaT30Info
	write header line starting at 'start' and store header
	line length to 'length'.
	Header line should not exceed 'max_length'

	The total length in characters depends on the resolution
	and the page width.
	This length is reduced by (T30_HEADER_LINE_LEFT_INDENT + T30_HEADER_LINE_RIGHT_INDENT)
	Finally, in case page nummer and other information should be inserted by adapter
	self length should be reduced by T30_HEADER_LINE_PAGE_INFO_SPACE
	*/
static void fax2CreateHeaderLine (ISDN_PORT *P,
																	T30_INFO *ptPaT30Info,
																	byte*    length,
																	byte*    start,
																	byte     max_length) {
	int font_width_pixels, line_width_pixels, line_width_chars;
#if 0
	const byte* pbPaParam = P->At.Fax2.DCS;
	/*
		This is the correct behavior, but unfortunately DCS is still not valid
		at time header line should be sent to protocol code.
		*/
	if ((global_options & DIVA_FAX_ALLOW_HIRES) && (pbPaParam [CL2_PARAM_VR] & CL2_VR_ULTRA_FINE)) {
		font_width_pixels = T30_R16_1540_OR_400_HEADER_FONT_WIDTH_PIXELS;
		line_width_pixels = Cl2ToSff_WD_R16 [pbPaParam [CL2_PARAM_WD]];

		DBG_LOG(("Line width pixels (ultra fine): %d", line_width_pixels))
	} else {
		font_width_pixels = T30_HEADER_FONT_WIDTH_PIXELS;
		line_width_pixels = Cl2ToSff_WD [pbPaParam [CL2_PARAM_WD]];
		DBG_LOG(("Line width pixels: %d (%d)", line_width_pixels))
	}
#else
	/*
		DCS ist still not valid, also suppose A4 with
		standard resolution.
		*/
	font_width_pixels = T30_HEADER_FONT_WIDTH_PIXELS;
	line_width_pixels = SFF_WD_A4;
#endif

	line_width_chars   = line_width_pixels/font_width_pixels;
	line_width_chars  -= (T30_HEADER_LINE_LEFT_INDENT + T30_HEADER_LINE_RIGHT_INDENT);

	if (line_width_chars > max_length) {
		line_width_chars = max_length;
	}

	if (P->At.Fax2.page_header[0] != 0 && line_width_chars > T30_HEADER_LINE_PAGE_INFO_SPACE) {
		int left_length = 0, left_written = 0;
		int middle_length = 0, middle_written = 0;
		int right_length = 0,  right_written  = 0;
		int line_length = 0, written = 0, pages_requested = 0;

		mem_set (start, ' ', line_width_chars);

		left_length = print_page_header (P,
																		 0,
																		 &P->At.Fax2.page_header[0],
																		 0,
																		 &pages_requested,
																		 &left_written);

		if (P->At.Fax2.page_header[left_length] == '\'') {
			line_length = print_page_header (P,
																	0,
																	&P->At.Fax2.page_header[left_length+1],
																	0,
																	&pages_requested,
																	&written);
			if (P->At.Fax2.page_header[left_length + 1 + line_length] == '\'') {
				middle_length  = line_length;
				middle_written = written;
				right_length  = print_page_header (P,
																		0,
																		&P->At.Fax2.page_header[left_length + middle_length + 2],
																		0,
																		&pages_requested,
																		&right_written);
			} else {
				/*
					Only left and right parts
					*/
				right_length  = line_length;
				right_written = written;
			}
		}
		if (pages_requested == 1) {
			/*
				Format requestd pages count
				Left space for page count. This will signel to protocol code
				that page count should be add to page header.
				*/
			line_width_chars  -= T30_HEADER_LINE_PAGE_INFO_SPACE;
		}

		/*
			Now print head line to buffer
			*/
		if (left_length && left_written) {
			written = 0;
			print_page_header (P,
												 start,
												 &P->At.Fax2.page_header[0],
												 line_width_chars,
												 &pages_requested,
												 &written);
		}
		if (middle_length && middle_written) {
			byte* p;
			middle_written = MIN(middle_written, (line_width_chars - 4));

			p = start + (line_width_chars - middle_written)/2;

			written = 0;
			print_page_header (P,
												 p,
												 &P->At.Fax2.page_header[left_length+1],
												 middle_written,
												 &pages_requested,
												 &written);
		}
		if (right_length && right_written) {
			byte* p;
			byte* fmt_start;

			if (middle_written) {
				fmt_start = &P->At.Fax2.page_header[left_length + middle_length + 2];
			} else {
				fmt_start = &P->At.Fax2.page_header[left_length+1];
			}
			right_written = MIN(right_written, (line_width_chars - 1));

			p = start + (line_width_chars - right_written);

			written = 0;
			print_page_header (P,
												 p,
												 fmt_start,
												 right_written,
												 &pages_requested,
												 &written);
		}

		*length = line_width_chars;
		DBG_TRC(("[%p:%s] page header", P->Channel, P->Name))
		DBG_BLK((start, line_width_chars))
	}
}

int fax2DirectionIn (ISDN_PORT* P) {
	return (P && (P->Direction = P_CONN_IN));
}

/**** end ************************************************************(GEI)**/
