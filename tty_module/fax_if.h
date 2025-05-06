
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

#ifndef _FAX_IF_H_
#define _FAX_IF_H_

#if !defined(UNIX)
#include "typedefs.h"
#endif

/* the fax command handler and the request types */
#if !defined(UNIX)
typedef int (* FAX_CMD) (struct ISDN_PORT *P, int Req, int Val, byte **Str);
#endif

#define FAX_REQ_NUL        -1	/* self contained command		*/
#define FAX_REQ_SET         0	/* set new value				*/
#define FAX_REQ_VAL         1	/* query current value			*/
#define FAX_REQ_RNG         2	/* query all possible values	*/

/* some ugly constants */

#define FAX_MAX_SCANLINE	4096
#define FAX_MAX_WRITE_SCANLINE 256

#define	FAX_WRITE_TXFULL ((word)-1)	/* transmit queue full return value */
/* functions used by all FAX modules */
#if !defined(UNIX)
void faxRsp (struct ISDN_PORT *P, byte *Text);

/* the common callout fuctions */

void             faxInit     (struct ISDN_PORT *P);
void             faxFinit    (struct ISDN_PORT *P);
void             faxSetClassSupport (struct ISDN_PORT *P);
int              faxPlusF    (struct ISDN_PORT *P, byte **Arg);
struct T30_INFO *faxConnect  (struct ISDN_PORT *P);
int              faxUp       (struct ISDN_PORT *P);
int              faxDelay    (struct ISDN_PORT *P,
							  byte *Response, word sizeResponse,
							  dword *RspDelay);
int              faxHangup   (struct ISDN_PORT *P);
word             faxRecv     (struct ISDN_PORT *P,
                              byte **Data, word sizeData,
                              byte *Stream, word sizeStream, word FrameType);
void             faxTxEmpty  (struct ISDN_PORT *P);
word             faxWrite    (struct ISDN_PORT *P,
                              byte **Data, word sizeData,
                              byte *Frame, word sizeFrame, word *FrameType);

/* the class1 implementations */

void             fax1Init     (struct ISDN_PORT *P);
void             fax1Finit    (struct ISDN_PORT *P);
void             fax1SetClass (struct ISDN_PORT *P);
struct T30_INFO *fax1Connect  (struct ISDN_PORT *P);
void             fax1Up       (struct ISDN_PORT *P);
int              fax1Delay    (struct ISDN_PORT *P,
							   byte *Response, word sizeResponse,
							   dword *RspDelay);
int              fax1Hangup   (struct ISDN_PORT *P);
word             fax1Write	  (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Frame, word sizeFrame, word *FrameType);
void             fax1TxEmpty  (struct ISDN_PORT *P);
word             fax1Recv     (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Stream, word sizeStream, word FrameType);
int              fax1Online   (struct ISDN_PORT *P);
int              fax1V8Setup  (struct ISDN_PORT *P);
int              fax1V8Menu   (struct ISDN_PORT *P);

int  fax1CmdTS    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRS    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdTH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdTM    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdRM    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdAR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdCL    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1CmdDD    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax1Cmd34    (struct ISDN_PORT *P, int Req, int Val, byte **Str);

/* the class2 implementations */

void             fax2Init     (struct ISDN_PORT *P);
void             fax2Finit    (struct ISDN_PORT *P);
void             fax2SetClass (struct ISDN_PORT *P);
struct T30_INFO *fax2Connect  (struct ISDN_PORT *P);
void             fax2Up       (struct ISDN_PORT *P);
int              fax2Delay    (struct ISDN_PORT *P,
							   byte *Response, word sizeResponse,
							   dword *RspDelay);
int              fax2Hangup   (struct ISDN_PORT *P);
word             fax2Write    (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Frame, word sizeFrame, word *FrameType);
void             fax2TxEmpty  (struct ISDN_PORT *P);
word             fax2Recv     (struct ISDN_PORT *P,
                               byte **Data, word sizeData,
                               byte *Stream, word sizeStream, word FrameType);

int  fax2CmdBOR   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdLPL   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdCR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDCC   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDIS   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDR    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdDT    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdET    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdK     (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdTD    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdPH    (struct ISDN_PORT *P, int Req, int Val, byte **Str);
int  fax2CmdLID   (struct ISDN_PORT *P, int Req, int Val, byte **Str);
#endif

struct ISDN_PORT;
void fax2RcvNDISC   (struct ISDN_PORT* P, byte* Data, word length);
int fax2DirectionIn (struct ISDN_PORT* P);

/* the temporary class 1 data structure, used only when connected	*/

typedef struct FAX1_DATA
{	sys_TIMER   Timer ;              /* common timer                     */
	MSG_QUEUE   RxQueue ;            /* hdlc frame receive queue         */
	dword       SyncTime ;           /* timestamp of SYNC indication     */
	word        FrameLen ;           /* no of bytes in send frame buffer */
	word        TxRxDelay ;          /* delay before next modulation     */
	word        Modulation ;         /* current modulation requested     */
	byte        ReconfigPending ;    /* enqueued reconfigure cmds        */
	byte        SyncPending ;        /* enqueued reconfigs with SYNC set */
	byte        ReconfigRequest ;    /* reconfigure request pending      */
	word        ReconfigFlags ;      /* number of flags for next reconf. */
	word        ReconfigModulation ; /* modulation for next reconf.      */
	word        ReconfigDelay ;      /* delay for next reconfigure req.  */
	byte        V34FaxSource ;       /* TRUE if we are the source DCE    */
	byte        V34FaxControl ;      /* TRUE if control channel active   */
	byte        V34FaxMark ;         /* TRUE if mark send/detect started */
	byte        V34FaxControlTxRateDiv1200 ;
	byte        V34FaxControlRxRateDiv1200 ;
	byte        V34FaxPrimaryRateDiv2400 ;
	byte        Phase ;              /* phase of modulation reached      */
	byte        TimerTask ;          /* what to do when the timer fires  */
	byte        TxDlePending ;       /* DLE was last in previous data    */
	byte        RxDlePending ;       /* DLE was last in previous data    */
	byte        FrameBuf[512] ;      /* frame I/O buffer                 */
} FAX1_DATA ;

/* the temporary class 2 data structure, used only when connected	*/

#define CL2_PARAM_VR            0
    #define CL2_VR_NORMAL           0x00
    #define CL2_VR_FINE             0x01
    #define CL2_VR_SUPER_FINE       0x02  /* R8 x 15.4 l/mm  */
    #define CL2_VR_ULTRA_FINE       0x04  /* R16 x 15.4 l/mm */
    #define CL2_VR_R8_1540          0x02  /* Class 2.1 */
    #define CL2_VR_R16_1540         0x04  /* Class 2.1 */
    #define CL2_VR_200_100          0x08  /* Class 2.1 */
    #define CL2_VR_200_200          0x10  /* Class 2.1 */
    #define CL2_VR_200_400          0x20  /* Class 2.1 */
    #define CL2_VR_300_300          0x40  /* Class 2.1 */
#define CL2_PARAM_BR            1
    #define CL2_BR_24               0
    #define CL2_BR_48               1  
    #define CL2_BR_72               2
    #define CL2_BR_96               3
    #define CL2_BR_120              4
    #define CL2_BR_144              5
    #define CL2_BR_168              6     /* Class 2.1 */
    #define CL2_BR_192              7     /* Class 2.1 */
    #define CL2_BR_216              8     /* Class 2.1 */
    #define CL2_BR_240              9     /* Class 2.1 */
    #define CL2_BR_264              0x0a  /* Class 2.1 */
    #define CL2_BR_288              0x0b  /* Class 2.1 */
    #define CL2_BR_312              0x0c  /* Class 2.1 */
    #define CL2_BR_336              0x0d  /* Class 2.1 */
#define CL2_PARAM_WD            2
    #define CL2_WD_1728             0
    #define CL2_WD_2048             1
    #define CL2_WD_2432             2
    #define CL2_WD_1216             3
    #define CL2_WD_864              4
#define CL2_PARAM_LN            3
    #define CL2_LN_A4               0
    #define CL2_LN_B4               1
    #define CL2_LN_UNLIM            2
#define CL2_PARAM_DF            4
    #define CL2_DF_1D               0
    #define CL2_DF_2D_READ          1
    #define CL2_DF_2D_UNCOMP        2
    #define CL2_DF_2D_MMR           3
    #define CL2_DF_2D_JBIG          0x04  /* Class 2.1 */
    #define CL2_DF_2D_JBIG_L0       0x08  /* Class 2.1 */
#define CL2_PARAM_EC            5
    #define CL2_EC_OFF              0
    #define CL2_EC_ECM64            1
    #define CL2_EC_ECM256           2
    #define CL2_EC_ECM              1     /* Class 2.1 */
    #define CL2_EC_GROUP3C_HDX      2     /* Class 2.1 */
    #define CL2_EC_GROUP3C_FDX      3     /* Class 2.1 */
#define CL2_PARAM_BF            6
    #define CL2_BF_OFF              0x00
    #define CL2_BF_ON               0x01
    #define CL2_BF_DOCUMENT         0x02  /* Class 2.1 */
    #define CL2_BF_EDIFACT          0x04  /* Class 2.1 */
    #define CL2_BF_BASIC            0x08  /* Class 2.1 */
    #define CL2_BF_CHARACTER        0x10  /* Class 2.1 */
    #define CL2_BF_MIXED            0x20  /* Class 2.1 */
    #define CL2_BF_PROCESSABLE      0x40  /* Class 2.1 */
#define CL2_PARAM_ST            7
    #define CL2_ST_0_0              0
    #define CL2_ST_5_5              1
    #define CL2_ST_10_5             2
    #define CL2_ST_10_10            3
    #define CL2_ST_20_10            4
    #define CL2_ST_20_20            5
    #define CL2_ST_40_20            6
    #define CL2_ST_40_40            7
#define CL2_PARAM_LEN           8

#define CL2_PARAM_JP            8         /* Class 2.1 */
    #define CL2_JP_OFF              0x00  /* Class 2.1 */
    #define CL2_JP_ON               0x01  /* Class 2.1 */
    #define CL2_JP_FULL_COLOR       0x02  /* Class 2.1 */
    #define CL2_JP_PREF_HMTAB       0x04  /* Class 2.1 */
    #define CL2_JP_12_BITS          0x08  /* Class 2.1 */
    #define CL2_JP_NO_SUBSAMPL      0x10  /* Class 2.1 */
    #define CL2_JP_CUST_ILLUM       0x20  /* Class 2.1 */
    #define CL2_JP_CUST_GAMMUT      0x40  /* Class 2.1 */
#define CL2_PARAM_LEN_CLASS21   9

#if defined(UNIX)
#define BYTE unsigned char
#define WORD unsigned short
#define DWORD dword
#define UINT unsigned int
#define BOOL bool_t
#ifdef VOID
#undef VOID
#endif
#define VOID void
#endif

typedef struct FAX2_DATA
{	unsigned int    uLineLen;
    BYTE            abLine [FAX_MAX_SCANLINE]; 
    UINT            uObjDataLen;
    UINT            uLineDataLen;
    DWORD    	    dLineData;
    BYTE            bDle;
    UINT            uNullBytesPos;
    BOOL            fNullByteExist;
    UINT            uCntEols;
    UINT            uCntEolsV;
    UINT            uCntBlocks;
    UINT            uLargestBlock;
    UINT            uLargestLine;
    UINT            uBitCount;
    BYTE            acIDRemote [20+1];
    BYTE            abDISRemote [CL2_PARAM_LEN];
    BYTE            bET;
    BYTE            bPTS;
    UINT            uNextObject;
    UINT            uPrevObject;
    int             nPageCount;     /* needed for CONNECT delay		*/
    int             nRespTxEmpty;   /* needed to defer OK on EOP	*/
		byte						remote_speed;
} FAX2_DATA;

#endif /* _FAX_IF_H_ */
