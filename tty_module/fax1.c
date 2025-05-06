
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
# include <linux/types.h>
# include <asm/string.h>
#endif
#include "port.h"
#include "atp_if.h"
#include "queue_if.h"
#include "t30.h"
#include "debuglib.h"

extern int sprintf (char*, const char*, ...);
extern void faxRsp (struct ISDN_PORT *P, byte *Text);
#if defined(UNIX)
static int portXmit_ (ISDN_PORT *P, byte *Data, word sizeData, word FrameType)
{ return (portXmit (P, Data, sizeData, FrameType) ? sizeData : 0); }
#define portXmit(P,Data,sizeData,FrameType) portXmit_(P,Data,sizeData,FrameType)
#endif
/*
 * Although the DSP code supports HDLC with all modulations,
 * HDLC support at speeds > 300 bps is for further studies.
 */
#define	FAX1_HDLC_AT_300_BAUD_ONLY	0
#define FAX1_V34FAX					1

/*
 * Miscellaneous times and counts
 * DSP timer unit is 125 microseconds, thus one millisecond is 8 DSP units
 * The AT+FTS/AT+FRS (Silence) unit is 10 milliseconds and thus 80 DSP units
 */
#define MILLISECONDS_UNTIL_CARRIER(p)	60000	/* ATS7=60	*/
#define MILLISECONDS_UNTIL_XMIT			5000	/* five sec	*/
#define MILLISECONDS_AFTER_CARRIER(p)	1400	/* ATS10=14	*/
#define MILLISECONDS_PER_FRAME_300(n)	((dword)((n + 3) * 27))
#define MIN_TX_RECONFIGURE_DELAY		75

/*
 * What the timer must do when it fires
 */
#define	FAX1_TIMER_TASK_NONE			0
#define FAX1_TIMER_TASK_NO_CARRIER		1
#define FAX1_TIMER_TASK_NO_XMIT			2
#define FAX1_TIMER_TASK_FRAME_SENT		3
#define FAX1_TIMER_TASK_DTE_INACTIVITY	4
#ifdef FAX1_V34FAX
#define FAX1_TIMER_TASK_CLEARDOWN		5
#define FAX1_TIMER_TASK_V8_INDICATE_CNG	6
#endif /* FAX1_V34FAX */

/*
 * Basic states of the machine, always IDLE except when the port is
 * connected with (P->Mode == P_MODE_FAX &&
 * (P->Class == P_CLASS_FAX1) || P->Class == P_CLASS_FAX10))
 */
#define	FAX1_STATE_IDLE		0
#define	FAX1_STATE_UP		1

/*
 * Phases of a modulation
 */
#define	FAX1_PHASE_IDLE		0
#define	FAX1_PHASE_DCD		1
#define	FAX1_PHASE_CTS		2
#define	FAX1_PHASE_XMIT		3
#define	FAX1_PHASE_RECV		4
#define	FAX1_PHASE_TERM		5
#ifdef FAX1_V34FAX
#define	FAX1_PHASE_TOV8		6
#define	FAX1_PHASE_V8		7
#define	FAX1_PHASE_TOCTRL	8
#define	FAX1_PHASE_CTRL		9
#define	FAX1_PHASE_TOPRI	10
#define	FAX1_PHASE_PRI		11
#define	FAX1_PHASE_TODISC	12
#define	FAX1_PHASE_DISC		13
#endif /* FAX1_V34FAX */

/*
 * Final bit in control field of HDLC frame
 */
#define	FAX1_FLAG_FINAL		0x10	/* poll/final bit in HDLC control field	*/

/*
 * Magic characters and bits in data transmissions
 */
#define	FAX1_CHAR_DLE		0x10	/* escape and transparency character	*/
#define	FAX1_CHAR_ETX		0x03	/* end of frame							*/
#define	FAX1_CHAR_SUB		0x1a	/* double escape in data				*/

#define _DLE_ETX_			"\x10\x03"
#define _DLE_DLE_ETX_		"\x10\x10\x03"

#ifdef FAX1_V34FAX

#define V34FAX_HDLC_PREAMBLE_FLAGS		2
#define V34FAX_MARK_BITS_TO_SEND		40
#define V34FAX_MARK_BITS_TO_DETECT		40

#define V8_CATEGORY_CALL_FUNCTION		0x01
#define V8_CATEGORY_MODULATION_MODES	0x05
#define V8_CATEGORY_V90_AVAILABILITY	0x07
#define V8_CATEGORY_PROTOCOLS			0x0a
#define V8_CATEGORY_PSTN_ACCESS			0x0d
#define V8_CATEGORY_NON_STANDARD		0x0f
#define V8_CATEGORY_INFO_MASK			0xe0
#define V8_EXTENSION_PATTERN			0x10
#define V8_EXTENSION_INFO_MASK			0xc7

#define V8_CALLF0_0						0x00
#define V8_CALLF0_H324					0x20
#define V8_CALLF0_V18					0x40
#define V8_CALLF0_T101					0x60
#define V8_CALLF0_T30_NORMAL			0x80
#define V8_CALLF0_T30_POLLING			0xa0
#define V8_CALLF0_DATA					0xc0
#define V8_CALLF0_EXTENDED				0xe0

#define V8_MODN0_V90					0x20
#define V8_MODN0_V34					0x40
#define V8_MODN0_V34HDX					0x80

#define V8_MODN1_V32BIS					0x01
#define V8_MODN1_V22BIS					0x02
#define V8_MODN1_V17					0x04
#define V8_MODN1_V29					0x40
#define V8_MODN1_V27TER					0x80

#define V8_MODN2_V26TER					0x01
#define V8_MODN2_V26BIS					0x02
#define V8_MODN2_V23					0x04
#define V8_MODN2_V23HDX					0x40
#define V8_MODN2_V21					0x80

#define V8_PCM0_V90APCM					0x20
#define V8_PCM0_V90DPCM					0x40

#define V8_PROT0_V42					0x20
#define V8_PROT0_EXTENDED				0xe0

#define V8_ACCESS0_CALL_DCE_CELLULAR	0x20
#define V8_ACCESS0_ANSWER_DCE_CELLULAR	0x40
#define V8_ACCESS0_DIGITAL				0x80

#define FAX1_CHAR_DC1		0x51
#define FAX1_CHAR_DC3		0x53
#define FAX1_CHAR_FERR		0x07
#define FAX1_CHAR_EOT		0x04
#define FAX1_CHAR_PRI		0x6b
#define FAX1_CHAR_CTRL		0x6d
#define FAX1_CHAR_PPH		0x6c
#define FAX1_CHAR_RTN		0x6a
#define FAX1_CHAR_MARK		0x68
#define FAX1_CHAR_RTNC		0x69
#define FAX1_CHAR_C12		0x6e
#define FAX1_CHAR_C24		0x6f
#define FAX1_CHAR_P24		0x70
#define FAX1_CHAR_P48		0x71
#define FAX1_CHAR_P72		0x72
#define FAX1_CHAR_P96		0x73
#define FAX1_CHAR_P120		0x74
#define FAX1_CHAR_P144		0x75
#define FAX1_CHAR_P168		0x76
#define FAX1_CHAR_P192		0x77
#define FAX1_CHAR_P216		0x78
#define FAX1_CHAR_P240		0x79
#define FAX1_CHAR_P264		0x7a
#define FAX1_CHAR_P288		0x7b
#define FAX1_CHAR_P312		0x7c
#define FAX1_CHAR_P336		0x7d

#define _DLE_FERR_			"\x10\x07"
#define _DLE_EOT_			"\x10\x04"
#define _DLE_PRI_			"\x10\x6b"
#define _DLE_CTRL_			"\x10\x6d"
#define _DLE_RTNC_			"\x10\x69"

static byte ControlRateToCharTable[] =
{
	FAX1_CHAR_C24,
	FAX1_CHAR_C12,
	FAX1_CHAR_C24
};

static byte PrimaryRateToCharTable[] =
{
	FAX1_CHAR_P336,
	FAX1_CHAR_P24,
	FAX1_CHAR_P48,
	FAX1_CHAR_P72,
	FAX1_CHAR_P96,
	FAX1_CHAR_P120,
	FAX1_CHAR_P144,
	FAX1_CHAR_P168,
	FAX1_CHAR_P192,
	FAX1_CHAR_P216,
	FAX1_CHAR_P240,
	FAX1_CHAR_P264,
	FAX1_CHAR_P288,
	FAX1_CHAR_P312,
	FAX1_CHAR_P336
};

#endif /* FAX1_V34FAX */

#define SHORT_TRN			CLASS1_RECONFIGURE_SHORT_TRAIN_FLAG
#define ECHO_PROT			CLASS1_RECONFIGURE_ECHO_PROTECT_FLAG

static struct
{
	byte	Value;
	word	Code;
} ModulationTable [] =
{
	{   3, CLASS1_RECONFIGURE_V21_CH2                       },	/* V.21 channel 2 300 bps	*/
	{  24, CLASS1_RECONFIGURE_V27_2400|ECHO_PROT            },	/* V.27ter 2400 bps			*/
	{  48, CLASS1_RECONFIGURE_V27_4800|ECHO_PROT            },	/* V.27ter 4800 bps			*/
	{  72, CLASS1_RECONFIGURE_V29_7200                      },	/* V.29    7200 bps			*/
	{  73, CLASS1_RECONFIGURE_V17_7200|ECHO_PROT            },	/* V.17    7200 bps long	*/
	{  74, CLASS1_RECONFIGURE_V17_7200|ECHO_PROT|SHORT_TRN  },	/* V.17    7200 bps short	*/
	{  96, CLASS1_RECONFIGURE_V29_9600                      },	/* V.29    9600 bps			*/
	{  97, CLASS1_RECONFIGURE_V17_9600|ECHO_PROT            },	/* V.17    9600 bps long	*/
	{  98, CLASS1_RECONFIGURE_V17_9600|ECHO_PROT|SHORT_TRN  },	/* V.17    9600 bps short	*/
	{ 121, CLASS1_RECONFIGURE_V17_12000|ECHO_PROT           },	/* V.17   12000 bps long	*/
	{ 122, CLASS1_RECONFIGURE_V17_12000|ECHO_PROT|SHORT_TRN },	/* V.17   12000 bps short	*/
	{ 145, CLASS1_RECONFIGURE_V17_14400|ECHO_PROT           },	/* V.17   14400 bps long	*/
	{ 146, CLASS1_RECONFIGURE_V17_14400|ECHO_PROT|SHORT_TRN }	/* V.17   14400 bps short	*/
};

static word PreambleFlagTable[] =
{
	0,					/* CLASS1_RECONFIGURE_IDLE											*/
	32,					/* CLASS1_RECONFIGURE_V25		(1sec - 15%) * 300Bit/s / 8Bit/Flag	*/
    32,					/* CLASS1_RECONFIGURE_V21_CH2	(1sec - 15%) * 300Bit/s / 8Bit/Flag	*/
	(200 * 24) / 800,	/* CLASS1_RECONFIGURE_V27_2400	200ms * 2400Bit/s / 8Bit/Flag		*/
	(200 * 48) / 800,	/* CLASS1_RECONFIGURE_V27_4800	200ms * 4800Bit/s / 8Bit/Flag		*/
	(200 * 72) / 800,	/* CLASS1_RECONFIGURE_V29_7200	200ms * 7200Bit/s / 8Bit/Flag		*/
	(200 * 96) / 800,	/* CLASS1_RECONFIGURE_V29_9600	200ms * 9600Bit/s / 8Bit/Flag		*/
	(200 * 120) / 800,	/* CLASS1_RECONFIGURE_V33_12000	200ms * 12000Bit/s / 8Bit/Flag		*/
	(200 * 144) / 800,	/* CLASS1_RECONFIGURE_V33_14400	200ms * 14400Bit/s / 8Bit/Flag		*/
	(200 * 72) / 800,	/* CLASS1_RECONFIGURE_V17_7200	200ms * 7200Bit/s / 8Bit/Flag		*/
	(200 * 96) / 800,	/* CLASS1_RECONFIGURE_V17_9600	200ms * 9600Bit/s / 8Bit/Flag		*/
	(200 * 120) / 800,	/* CLASS1_RECONFIGURE_V17_12000	200ms * 12000Bit/s / 8Bit/Flag		*/
	(200 * 144) / 800	/* CLASS1_RECONFIGURE_V17_14400	200ms * 14400Bit/s / 8Bit/Flag		*/
};

/*
 * Reconfigure packet format
 */
#pragma pack(1)

typedef struct {
	byte	Type ;					/* CLASS1_RECONFIGURE_REQUEST			*/
	byte	DelayLSB, DelayMSB ;	/* reconfigure delay (in 8kHz samples)	*/
	byte	CodeLSB, CodeMSB ;		/* reconfigure code						*/
	byte	FlagsLSB, FlagsMSB ;	/* # of reconfigure hdlc preamble flags	*/
} RECONFIG ;

#ifdef FAX1_V34FAX

typedef struct {
	byte	Type ;					/* CLASS1_REQUEST_V34FAX_SETUP			*/
	byte	FlagsLSB, FlagsMSB ;
	byte	PrefCtrlRateLSB, PrefCtrlRateMSB ;
	byte	MinPrimTxRateLSB, MinPrimTxRateMSB ;
	byte	MaxPrimTxRateLSB, MaxPrimTxRateMSB ;
	byte	MinPrimRxRateLSB, MinPrimRxRateMSB ;
	byte	MaxPrimRxRateLSB, MaxPrimRxRateMSB ;
	byte	ModulationsLSB, ModulationsMSB ;
} V34FAX_SETUP ;

#endif /* FAX1_V34FAX */
#pragma pack()

static byte *fax1Phase (byte Phase)
{
	static byte tmp[16] ;
	switch ( Phase )
	{
	case FAX1_PHASE_IDLE:	return ("IDLE") ;
	case FAX1_PHASE_DCD :	return ("DCD ") ;
	case FAX1_PHASE_CTS :	return ("CTS ") ;
	case FAX1_PHASE_XMIT:	return ("XMIT") ;
	case FAX1_PHASE_RECV:	return ("RECV") ;
	case FAX1_PHASE_TERM:	return ("TERM") ;
#ifdef FAX1_V34FAX
	case FAX1_PHASE_TOV8:	return ("TOV8") ;
	case FAX1_PHASE_V8:		return ("V8  ") ;
	case FAX1_PHASE_TOCTRL:	return ("TOCT") ;
	case FAX1_PHASE_CTRL:	return ("CTRL") ;
	case FAX1_PHASE_TOPRI:	return ("TOPR") ;
	case FAX1_PHASE_PRI:	return ("PRI ") ;
	case FAX1_PHASE_TODISC:	return ("TODI") ;
	case FAX1_PHASE_DISC:	return ("DISC") ;
#endif /* FAX1_V34FAX */
	}
	(void) sprintf (tmp, "ILL_PHASE(%02x)", Phase) ;
	return ( tmp ) ;
}

static byte *fax1CtrlInd (byte Type)
{
	static byte tmp[16] ;
	switch ( Type )
	{
	case CLASS1_INDICATION_SYNC:		return ("SYNC   ") ;
	case CLASS1_INDICATION_DCD_OFF:		return ("DCD_OFF") ;
	case CLASS1_INDICATION_DCD_ON:		return ("DCD_ON ") ;
	case CLASS1_INDICATION_CTS_OFF:		return ("CTS_OFF") ;
	case CLASS1_INDICATION_CTS_ON:		return ("CTS_ON ") ;
	case CLASS1_INDICATION_SIGNAL_TONE:	return ("TONEDET") ;
#ifdef FAX1_V34FAX
	case CLASS1_INDICATION_CI_DETECTED:	return ("CI_DET ") ;
#endif /* FAX1_V34FAX */
	}
	(void) sprintf (tmp, "ILL_CTRL(%02x)", Type) ;
	return ( tmp ) ;
}

static byte *fax1DataInd (byte Type)
{
	static byte tmp[16] ;
	switch ( Type )
	{
	case CLASS1_INDICATION_DATA:				return ("DATA") ;
	case CLASS1_INDICATION_FLUSHED_DATA:		return ("FLUSHED_DATA") ;
	case CLASS1_INDICATION_HDLC_DATA:			return ("HDLC") ;
	case CLASS1_INDICATION_ABORTED_HDLC_DATA:	return ("ABORTED_HDLC") ;
	case CLASS1_INDICATION_WRONG_CRC_HDLC_DATA:	return ("WRONG_CRC_HDLC") ;
	case CLASS1_INDICATION_FLUSHED_HDLC_DATA:	return ("FLUSHED_HDLC") ;
	}
	(void) sprintf (tmp, "ILL_DATA(%02x)", Type) ;
	return ( tmp ) ;
}

static void fax1CancelTimer (ISDN_PORT *P)
{
	P->port_u.Fax1.TimerTask = FAX1_TIMER_TASK_NONE ;
	(void) sysCancelTimer (&P->port_u.Fax1.Timer) ;
}

static void fax1ScheduleTimer (ISDN_PORT *P, byte Task, dword MSec)
{
	(void) sysCancelTimer (&P->port_u.Fax1.Timer) ;
	P->port_u.Fax1.TimerTask = Task ;
	(void) sysScheduleTimer (&P->port_u.Fax1.Timer, MSec) ;
}

static void fax1InactivityTimer (ISDN_PORT *P)
{
	if ( P->At.InactivityTimeout != 0 )
	{
		(void) sysCancelTimer (&P->port_u.Fax1.Timer) ;
		P->port_u.Fax1.TimerTask = FAX1_TIMER_TASK_DTE_INACTIVITY ;
		(void) sysScheduleTimer (&P->port_u.Fax1.Timer,
		                         1000L * P->At.InactivityTimeout) ;
	}
}

static void fax1RspString (ISDN_PORT *P, byte *RspString)
{
	faxRsp (P, RspString) ;
	atRspFlush (P) ;
}

static void fax1RspNumber (ISDN_PORT *P, int RspNumber)
{
	atRsp (P, RspNumber) ;
	atRspFlush (P) ;
}

static void fax1EnqIdle (ISDN_PORT *P)
{
	int    written;
	static RECONFIG Idle =
	{
		CLASS1_RECONFIGURE_REQUEST,
		0 & 0xff, 															/* DelayLSB	*/
		0 >> 8,																/* DelayMSB	*/
	 	(CLASS1_RECONFIGURE_SYNC_FLAG | CLASS1_RECONFIGURE_IDLE) & 0xff,	/* CodeLSB	*/
	 	(CLASS1_RECONFIGURE_SYNC_FLAG | CLASS1_RECONFIGURE_IDLE) >> 8,		/* CodeMSB	*/
	    0 & 0xff,															/* FlagsLSB	*/
	    0 >> 8,																/* FlagsMSB	*/
	};

	written = portXmit(P, (byte *)&Idle, sizeof(Idle), N_UDATA);
	if ( written == sizeof(Idle) )
	{
		(P->port_u.Fax1.ReconfigPending)++ ;
		(P->port_u.Fax1.SyncPending)++ ;
		P->port_u.Fax1.SyncTime = 0 ;
	}
	/* ???: no flow control check */
}

#ifdef FAX1_V34FAX

static byte fax1FindV8Octet (byte *Info, byte Length, byte Category, byte Offset)
{
	byte i;

	i = 0;
	while ((i < Length) && ((Info[i] & ~V8_CATEGORY_INFO_MASK) != Category))
		i++;
	if (i + Offset >= Length)
		return (Length);
	if (Offset != 0)
	{
		do
		{
			i++;
			if ((Info[i] & ~V8_EXTENSION_INFO_MASK) != V8_EXTENSION_PATTERN)
				return (Length);
		} while (--Offset != 0);
	}
	return (i);
}

static void fax1RspV8CallMenu (ISDN_PORT *P)
{
	byte Buf[80];
	byte pos;
	int i;

	if (P->At.Fax1.V34Fax)
	{
		pos = 0;
		P->At.V8.CMBuffer[pos++] =
			(P->port_u.Fax1.V34FaxSource) ?
			V8_CATEGORY_CALL_FUNCTION | V8_CALLF0_T30_POLLING :
			V8_CATEGORY_CALL_FUNCTION | V8_CALLF0_T30_NORMAL;
		P->At.V8.CMBuffer[pos++] = V8_CATEGORY_MODULATION_MODES | V8_MODN0_V34HDX;
		P->At.V8.CMBuffer[pos++] = V8_EXTENSION_PATTERN |
			V8_MODN1_V17 | V8_MODN1_V29 | V8_MODN1_V27TER;
		P->At.V8.CMBuffer[pos++] = V8_EXTENSION_PATTERN;
		P->At.V8.CMLength = pos;
		i = sprintf (Buf, "+A8M:");
		for (pos = 0; pos < P->At.V8.CMLength; pos++)
			i += sprintf (&Buf[i], "%02X", P->At.V8.CMBuffer[pos]);
	}
	else
	{
		P->At.V8.CMLength = 0;
		i = sprintf (Buf, "+A8M:0");
	}
    faxRsp (P, Buf);
}

static void fax1RspV8JointMenu (ISDN_PORT *P)
{
	byte Buf[80];
	byte pos;
	int i;

	if (P->At.Fax1.V34Fax)
	{
		pos = 0;
		P->At.V8.JMBuffer[pos++] =
			(P->port_u.Fax1.V34FaxSource) ?
			V8_CATEGORY_CALL_FUNCTION | V8_CALLF0_T30_NORMAL :
			V8_CATEGORY_CALL_FUNCTION | V8_CALLF0_T30_POLLING;
		P->At.V8.JMBuffer[pos++] = V8_CATEGORY_MODULATION_MODES | V8_MODN0_V34HDX;
		P->At.V8.JMBuffer[pos++] = V8_EXTENSION_PATTERN |
			V8_MODN1_V17 | V8_MODN1_V29 | V8_MODN1_V27TER;
		P->At.V8.JMBuffer[pos++] = V8_EXTENSION_PATTERN;
		P->At.V8.JMLength = pos;
		i = sprintf (Buf, "+A8M:");
		for (pos = 0; pos < P->At.V8.JMLength; pos++)
			i += sprintf (&Buf[i], "%02X", P->At.V8.JMBuffer[pos]);
	}
	else
	{
		P->At.V8.JMLength = 0;
		i = sprintf (Buf, "+A8M:0");
	}
    faxRsp (P, Buf);
}

static void fax1RspF34 (ISDN_PORT *P)
{
	byte Buf[32];
	byte *p;

	p = Buf;
	p += sprintf (p, "+F34:%d,%d",
	                 P->port_u.Fax1.V34FaxPrimaryRateDiv2400,
	                 P->port_u.Fax1.V34FaxControlTxRateDiv1200);
	if (P->port_u.Fax1.V34FaxControlRxRateDiv1200 != P->port_u.Fax1.V34FaxControlTxRateDiv1200)
	{
		p += sprintf (p, ",%d",
		                 P->port_u.Fax1.V34FaxControlRxRateDiv1200);
	}
	*p = '\0';
    faxRsp (P, Buf);
}

static void fax1V34EscapeRate (ISDN_PORT *P, byte ControlChannel, byte *Prefix)
{
	byte Buf[32];
	byte *p, *q;

	p = Buf;
	if (Prefix != NULL)
	{
		q = Prefix;
		while (*q != '\0')
			*p++ = *q++;
	}
	*(p++) = FAX1_CHAR_DLE;
	*(p++) = ControlChannel ? FAX1_CHAR_CTRL : FAX1_CHAR_PRI;
	*(p++) = FAX1_CHAR_DLE;
	*(p++) = PrimaryRateToCharTable[P->port_u.Fax1.V34FaxPrimaryRateDiv2400];
	if (ControlChannel)
	{
		*(p++) = FAX1_CHAR_DLE;
		*(p++) = ControlRateToCharTable[P->port_u.Fax1.V34FaxControlTxRateDiv1200];
		if (P->port_u.Fax1.V34FaxControlRxRateDiv1200 != P->port_u.Fax1.V34FaxControlTxRateDiv1200)
		{
			*(p++) = FAX1_CHAR_DLE;
			*(p++) = ControlRateToCharTable[P->port_u.Fax1.V34FaxControlRxRateDiv1200];
		}
	}
	if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
	{
		*(p++) = CLASS1_INDICATION_DATA ;	/* dummy to fill type field */

		if ( !queueWriteMsg (&P->port_u.Fax1.RxQueue, Buf, (word)(p - Buf)) )
			DBG_FTL(("[%p:%s] fax1V34EscapeRate: overrun", P->Channel, P->Name));
	}
	else
	{
		portRxEnqueue( P, Buf, (word)(p - Buf), 0/*stream mode*/ ) ;
	}
}

static int fax1SendV34FaxSetup (ISDN_PORT *P)
{
	V34FAX_SETUP V34FaxSetup;
	word w;
	int written;

	V34FaxSetup.Type				= CLASS1_REQUEST_V34FAX_SETUP ;
	w = P->At.Fax1.V34Fax ? P->At.Fax1.V34FaxFlags : 0;
	V34FaxSetup.FlagsLSB			= (byte)(w & 0xff) ;
	V34FaxSetup.FlagsMSB			= (byte)(w >> 8) ;
	w = (word)(P->At.Fax1.V34FaxPrefControlRateDiv1200 * 1200) ;
	V34FaxSetup.PrefCtrlRateLSB		= (byte)(w & 0xff) ;
	V34FaxSetup.PrefCtrlRateMSB		= (byte)(w >> 8) ;
	w = (word)(P->At.Fax1.V34FaxMinPrimaryRateDiv2400 * 2400) ;
	V34FaxSetup.MinPrimTxRateLSB	= (byte)(w & 0xff) ;
	V34FaxSetup.MinPrimTxRateMSB	= (byte)(w >> 8) ;
	w = (word)(P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 * 2400) ;
	V34FaxSetup.MaxPrimTxRateLSB	= (byte)(w & 0xff) ;
	V34FaxSetup.MaxPrimTxRateMSB	= (byte)(w >> 8) ;
	w = (word)(P->At.Fax1.V34FaxMinPrimaryRateDiv2400 * 2400) ;
	V34FaxSetup.MinPrimRxRateLSB	= (byte)(w & 0xff) ;
	V34FaxSetup.MinPrimRxRateMSB	= (byte)(w >> 8) ;
	w = (word)(P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 * 2400) ;
	V34FaxSetup.MaxPrimRxRateLSB	= (byte)(w & 0xff) ;
	V34FaxSetup.MaxPrimRxRateMSB	= (byte)(w >> 8) ;
	w = 0;
	if ((P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport) &
	    (1L << CLASS1_RECONFIGURE_V27_4800))
	{
		w |= CLASS1_V34FAX_ENABLED_MODULATION_V27;
	}
	if ((P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport) &
	    (1L << CLASS1_RECONFIGURE_V29_9600))
	{
		w |= CLASS1_V34FAX_ENABLED_MODULATION_V29;
	}
	if ((P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport) &
	    (1L << CLASS1_RECONFIGURE_V33_14400))
	{
		w |= CLASS1_V34FAX_ENABLED_MODULATION_V33;
	}
	if ((P->At.FaxTxModulationSupport & P->At.FaxRxModulationSupport) &
	    (1L << CLASS1_RECONFIGURE_V17_14400))
	{
		w |= CLASS1_V34FAX_ENABLED_MODULATION_V17;
	}
	V34FaxSetup.ModulationsLSB		= (byte)(w & 0xff) ;
	V34FaxSetup.ModulationsMSB		= (byte)(w >> 8) ;

	written = portXmit (P, (byte *)&V34FaxSetup, sizeof(V34FaxSetup), N_UDATA);
	if (written != sizeof(V34FaxSetup))
		return FALSE ;
	return TRUE ;
}

static int fax1V34FaxReconfig (ISDN_PORT *P, word Mod)
{
	RECONFIG	Reconfig ;
	int written;

	P->port_u.Fax1.ReconfigRequest		= TRUE ;
	P->port_u.Fax1.ReconfigModulation	= Mod ;
	P->port_u.Fax1.ReconfigDelay			= 0 ;
	P->port_u.Fax1.ReconfigFlags			= 0 ;
	if (Mod & CLASS1_RECONFIGURE_HDLC_FLAG)
	{
		switch (Mod & CLASS1_RECONFIGURE_PROTOCOL_MASK)
		{
		case CLASS1_RECONFIGURE_V34_CONTROL_CHANNEL:
			P->port_u.Fax1.ReconfigFlags = V34FAX_HDLC_PREAMBLE_FLAGS;
			break;
		case CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL:
			if (Mod & CLASS1_RECONFIGURE_TX_FLAG)
				P->port_u.Fax1.ReconfigFlags = 60 * P->port_u.Fax1.V34FaxPrimaryRateDiv2400;
			break;
		case CLASS1_RECONFIGURE_V8:
			break;
		default:
			if (Mod & CLASS1_RECONFIGURE_TX_FLAG)
			{
				P->port_u.Fax1.ReconfigFlags = PreambleFlagTable[
					Mod & CLASS1_RECONFIGURE_PROTOCOL_MASK] ;
			}
			break;
		}
	}
	if ( P->port_u.Fax1.SyncPending == 0 )
	{
		Reconfig.Type		= CLASS1_RECONFIGURE_REQUEST ;
		Reconfig.DelayLSB	= (byte)((0 * 8) & 0xff) ;
		Reconfig.DelayMSB	= (byte)((0 * 8) >> 8) ;
		Reconfig.CodeLSB	= (byte)(P->port_u.Fax1.ReconfigModulation & 0xff) ;
		Reconfig.CodeMSB	= (byte)(P->port_u.Fax1.ReconfigModulation >> 8) ;
		Reconfig.FlagsLSB	= (byte)(P->port_u.Fax1.ReconfigFlags & 0xff) ;
		Reconfig.FlagsMSB	= (byte)(P->port_u.Fax1.ReconfigFlags >> 8) ;

		written = portXmit (P, (byte *)&Reconfig, sizeof(Reconfig), N_UDATA);
		if (written != sizeof(Reconfig))
		{
			P->port_u.Fax1.ReconfigRequest = FALSE ;
			return FALSE ;
		}
	}
	(P->port_u.Fax1.ReconfigPending)++ ;
	P->port_u.Fax1.ReconfigRequest = FALSE ;
	return TRUE ;
}

static void fax1V34Cleardown (ISDN_PORT *P)
{
	int   written;
	byte ReqBuf[4];

	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_TODISC:
		fax1EnqIdle (P);
 		fax1ScheduleTimer (P, FAX1_TIMER_TASK_CLEARDOWN,
		                      1000) ;
		P->State = P_ESCAPE ;
		P->port_u.Fax1.Phase = FAX1_PHASE_DISC;
		break;

	case FAX1_PHASE_DISC:
		fax1CancelTimer (P);
		ReqBuf[0] = CLASS1_REQUEST_CLEARDOWN;
		written = portXmit (P, ReqBuf, 1, N_UDATA) ;
		/* ???: no flow control check */
		portRxEnqueue (P, _DLE_EOT_, 2, 0/*stream mode*/) ;
		fax1RspNumber (P, R_OK) ;
		fax1InactivityTimer (P) ;
		break;

	default:
		P->port_u.Fax1.ReconfigRequest = FALSE;
 		fax1ScheduleTimer (P, FAX1_TIMER_TASK_CLEARDOWN,
		                      1000) ;
		P->State = P_ESCAPE ;
		P->port_u.Fax1.Phase = FAX1_PHASE_TODISC;
		if ((P->port_u.Fax1.SyncPending == 0) && !P->port_u.Fax1.V34FaxControl)
		{
			fax1EnqIdle (P);
			P->port_u.Fax1.Phase = FAX1_PHASE_DISC;
		}
	}
}

#endif /* FAX1_V34FAX */

static int fax1Silence (
	ISDN_PORT *P, int Req, int Val, byte *Cmd)
{
	int Res = R_OK ;

	switch ( Req )
	{
	case FAX_REQ_SET:
		if ( P->At.Fax1.State != FAX1_STATE_UP )
		{
			DBG_FTL(("[%p:%s] fax1+F%s: state is not 'up'", P->Channel, P->Name, Cmd))
			Res = R_ERROR ;
			break ;
		}
		if ( !P->Channel )
		{
			DBG_FTL(("[%p:%s] fax1+F%s: not connected", P->Channel, P->Name, Cmd))
			Res = R_ERROR ;
			break ;
		}
		/* add value to already remembered value, respond OK */
		/* command value is given in 10ms units              */
		P->port_u.Fax1.TxRxDelay += (word)(Val * 10) ;
		break ;
	case FAX_REQ_VAL:
		/* no current value, respond OK as seen with a rockwell modem */
		break ;
	case FAX_REQ_RNG:
		/* respond whith valid range and an OK */
		fax1RspString (P, (byte *)"0-255") ;
		break ;
	}
	fax1InactivityTimer (P) ;
	return (Res) ;
}

static int fax1QueryMod (ISDN_PORT *P, int Req, byte *Cmd)
{
	byte Buf[80] ;
	byte *p;
	int i;

	if ( !(P->At.ClassSupport & ((1 << P_CLASS_FAX1) | (1 << P_CLASS_FAX10))) )
	{
		DBG_FTL(("[%p:%s] fax1+F%s: class 1 not supported", P->Channel, P->Name, Cmd))
		return R_ERROR ;
	}
	p = Buf;
	if ( Req == FAX_REQ_VAL )
	{	/* always respond 3 as seen with a rockwell modem	*/
		p += sprintf (p, "%d", 3) ;
	}
	else
	{	/* return the modulations available for this board	*/
		for (i = 0; i < sizeof(ModulationTable) / sizeof(ModulationTable[0]); i++)
		{
			if ( TRUE
#if	FAX1_HDLC_AT_300_BAUD_ONLY
			  && (((Cmd[1] != 'h') && (Cmd[1] != 'H'))
			   || (ModulationTable[i].Value == 3))
#endif /* FAX1_HDLC_AT_300_BAUD_ONLY */
			  && (((Cmd[0] != 't') && (Cmd[0] != 'T'))
			   || (P->At.FaxTxModulationSupport & (1L <<
			       (ModulationTable[i].Code & CLASS1_RECONFIGURE_PROTOCOL_MASK))))
			  && (((Cmd[0] == 't') || (Cmd[0] == 'T'))
			   || (P->At.FaxRxModulationSupport & (1L <<
			       (ModulationTable[i].Code & CLASS1_RECONFIGURE_PROTOCOL_MASK)))) )
			{
				if (p != Buf)
					*(p++) = ',';
				p += sprintf (p, "%d", ModulationTable[i].Value);
			}
		}
	}
	*p = '\0';
	fax1RspString (P, Buf) ;
	return R_OK ;
}

static int fax1XlateMod (ISDN_PORT *P, byte *Cmd, int Val, word *pMod)
{
	int i;
	word Mod;

	if ( P->At.Fax1.State != FAX1_STATE_UP )
	{
		DBG_FTL(("[%p:%s] fax1+F%s: state is not 'up'", P->Channel, P->Name, Cmd))
    	return FALSE ;
	}
	if ( !P->Channel )
	{
		DBG_FTL(("[%p:%s] fax1+F%s: not connected", P->Channel, P->Name, Cmd))
    	return FALSE ;
	}
	Mod = 0;
	if ((Cmd[0] == 't') || (Cmd[0] == 'T'))
		Mod |= CLASS1_RECONFIGURE_TX_FLAG;
	if ((Cmd[1] == 'h') || (Cmd[1] == 'H'))
		Mod |= CLASS1_RECONFIGURE_HDLC_FLAG;
	i = 0;
	while ((i < sizeof(ModulationTable) / sizeof(ModulationTable[0]))
	    && (ModulationTable[i].Value != Val))
	{
		i++;
	}
	if ( (i == sizeof(ModulationTable) / sizeof(ModulationTable[0]))
#if	FAX1_HDLC_AT_300_BAUD_ONLY
	  || ((Mod & CLASS1_RECONFIGURE_HDLC_FLAG) && (Val != 3))
#endif /* FAX1_HDLC_AT_300_BAUD_ONLY */
	  || ((Mod & CLASS1_RECONFIGURE_TX_FLAG)
	   && !(P->At.FaxTxModulationSupport & (1L <<
	        (ModulationTable[i].Code & CLASS1_RECONFIGURE_PROTOCOL_MASK))))
	  || (!(Mod & CLASS1_RECONFIGURE_TX_FLAG)
	   && !(P->At.FaxRxModulationSupport & (1L <<
	        (ModulationTable[i].Code & CLASS1_RECONFIGURE_PROTOCOL_MASK)))) )
	{
		DBG_FTL(("[%p:%s] fax1+F%s: %d illegal", P->Channel, P->Name, Cmd, Val))
		return FALSE ;
	}
	Mod |= ModulationTable[i].Code;
	*pMod = Mod;
	return TRUE ;
}

static void fax1Deconfig (ISDN_PORT *P)
{
	fax1CancelTimer (P) ;
	queueReset (&P->port_u.Fax1.RxQueue) ;
	P->port_u.Fax1.FrameLen 			= 0 ;
	P->port_u.Fax1.TxRxDelay			= 0 ;
	P->port_u.Fax1.Phase				= FAX1_PHASE_IDLE ;
	P->port_u.Fax1.TimerTask			= FAX1_TIMER_TASK_NONE ;
	P->port_u.Fax1.TxDlePending		= FALSE ;
	P->port_u.Fax1.RxDlePending		= FALSE ;
	P->port_u.Fax1.ReconfigRequest	= FALSE ;

	P->State = P_ESCAPE ;
}

static int fax1SendReconfig (ISDN_PORT *P)
{
	RECONFIG	Reconfig ;
	dword		Delay, CompensatedDelay ;
	dword		Now ;
	int         written ;

	Delay = P->port_u.Fax1.ReconfigDelay ;
	if ( P->port_u.Fax1.ReconfigModulation )
	{
		if ( (P->port_u.Fax1.ReconfigModulation & CLASS1_RECONFIGURE_TX_FLAG)
		 &&  (Delay < MIN_TX_RECONFIGURE_DELAY) )
		{
			Delay = MIN_TX_RECONFIGURE_DELAY ;
		}
		if ( P->port_u.Fax1.SyncTime )
		{
			Now = sysTimerTicks () ;
			CompensatedDelay = (((int32)(P->port_u.Fax1.SyncTime + Delay - Now)) > 0) ?
				(P->port_u.Fax1.SyncTime + Delay - Now) : 0 ;
			DBG_FTL(("[%p:%s] fax1 delay: %ld %ld %ld %ld",
				P->Channel, P->Name,
				(long) Delay,
				(long) CompensatedDelay,
				(long)(P->port_u.Fax1.SyncTime),
				(long) Now))
			if (CompensatedDelay < Delay)
			{
				Delay = CompensatedDelay ;
			}
		}
	}

	Reconfig.Type		= CLASS1_RECONFIGURE_REQUEST ;
	Reconfig.DelayLSB	= (byte)((Delay * 8) & 0xff) ;
	Reconfig.DelayMSB	= (byte)((Delay * 8) >> 8) ;
	Reconfig.CodeLSB	= (byte)(P->port_u.Fax1.ReconfigModulation & 0xff) ;
	Reconfig.CodeMSB	= (byte)(P->port_u.Fax1.ReconfigModulation >> 8) ;
	Reconfig.FlagsLSB	= (byte)(P->port_u.Fax1.ReconfigFlags & 0xff) ;
	Reconfig.FlagsMSB	= (byte)(P->port_u.Fax1.ReconfigFlags >> 8) ;

	written = portXmit (P, (byte *)&Reconfig, sizeof(Reconfig), N_UDATA);
	if (written != sizeof(Reconfig))
		return FALSE ;

	(P->port_u.Fax1.ReconfigPending)++ ;
	P->port_u.Fax1.ReconfigRequest = FALSE ;
	return TRUE ;
}

static int fax1Reconfig (ISDN_PORT *P, word Mod, word Delay)
{

	/* clean up any pending stuff */

	fax1Deconfig (P) ;

	P->port_u.Fax1.ReconfigRequest		= TRUE ;
	P->port_u.Fax1.ReconfigModulation	= Mod ;
	P->port_u.Fax1.ReconfigDelay			= Delay ;
	if ( (Mod & CLASS1_RECONFIGURE_HDLC_FLAG)
	  && (Mod & CLASS1_RECONFIGURE_TX_FLAG) )
	{
		P->port_u.Fax1.ReconfigFlags = PreambleFlagTable[
			Mod & CLASS1_RECONFIGURE_PROTOCOL_MASK] ;
	}
	else
	{
		P->port_u.Fax1.ReconfigFlags = 0 ;
	}
	if ( P->port_u.Fax1.SyncPending == 0 )
	{
		if ( !fax1SendReconfig (P) )
		{
			P->port_u.Fax1.ReconfigRequest = FALSE ;
			fax1InactivityTimer (P) ;
			return R_ERROR ;
		}
	}
	if ( (Mod & CLASS1_RECONFIGURE_PROTOCOL_MASK) == CLASS1_RECONFIGURE_V25 )
	{
		/* adjust to a modulation as expected in further +FRH/+FTH	*/
		Mod = CLASS1_RECONFIGURE_V21_CH2 | (Mod & ~CLASS1_RECONFIGURE_PROTOCOL_MASK);
	}
	P->port_u.Fax1.Modulation = Mod ;

	if ( Mod & CLASS1_RECONFIGURE_TX_FLAG )
	{
		P->port_u.Fax1.Phase = FAX1_PHASE_CTS ;
	}
	else
	{
		P->port_u.Fax1.Phase = FAX1_PHASE_DCD ;
		fax1ScheduleTimer (P, FAX1_TIMER_TASK_NO_CARRIER,
		                      MILLISECONDS_UNTIL_CARRIER(P)) ;
	}

	P->State = P_DATA ;

	return R_VOID ;
}

static int fax1AbortRecv (ISDN_PORT *P)
{
	switch ( P->port_u.Fax1.Phase )
	{
#ifdef FAX1_V34FAX
	case FAX1_PHASE_TOCTRL:
	case FAX1_PHASE_CTRL:
	case FAX1_PHASE_TOPRI:
	case FAX1_PHASE_PRI:
		atRspFlush (P) ;
		portRxEnqueue (P, _DLE_EOT_, 2, 0/*stream mode*/) ;
		return (1) ;

	case FAX1_PHASE_V8 :
		atRspFlush (P) ;
		fax1CancelTimer (P) ;
		P->State = P_ESCAPE ;
		return (1) ;

#endif /* FAX1_V34FAX */
	case FAX1_PHASE_DCD :
	case FAX1_PHASE_RECV:
		atRspFlush (P) ;
		if ( P->port_u.Fax1.RxDlePending )
		{
			P->port_u.Fax1.RxDlePending = FALSE ;
			portRxEnqueue (P, _DLE_DLE_ETX_, 3, 0/*stream mode*/) ;
		}
		else
		{
			portRxEnqueue (P, _DLE_ETX_, 2, 0/*stream mode*/) ;
		}
		fax1Deconfig (P) ;
		return (1) ;
	}
	return (0) ;
}

static void _cdecl fax1Timer (ISDN_PORT *P)
{
	byte TimerTask;

	TimerTask = P->port_u.Fax1.TimerTask;
	P->port_u.Fax1.TimerTask = FAX1_TIMER_TASK_NONE ;
	switch ( TimerTask )
	{
	case FAX1_TIMER_TASK_NO_CARRIER:
		DBG_FTL(("[%p:%s] fax1Timer: miss DCD_ON", P->Channel, P->Name))
		fax1Deconfig (P) ;
		fax1RspNumber (P, R_NO_CARRIER) ;
		fax1InactivityTimer (P) ;
		break ;

	case FAX1_TIMER_TASK_NO_XMIT:
		DBG_FTL(("[%p:%s] fax1Timer: miss XMIT, abort", P->Channel, P->Name))
		fax1Deconfig (P) ;
		P->At.Fax1.State = FAX1_STATE_IDLE ;
    	atDrop (P, CAUSE_SHUT);
		fax1RspNumber (P, R_ERROR) ;
		break ;

	case FAX1_TIMER_TASK_FRAME_SENT:
		DBG_PRV1(("[%p:%s] fax1Timer: send CONNECT", P->Channel, P->Name))
		fax1RspNumber (P, R_CONNECT_300) ;
		fax1InactivityTimer (P) ;
		break ;

	case FAX1_TIMER_TASK_DTE_INACTIVITY:
		DBG_FTL(("[%p:%s] fax1Timer: DTE inactivity", P->Channel, P->Name))
		fax1AbortRecv (P) ;
		fax1Deconfig (P) ;
		P->At.Fax1.State = FAX1_STATE_IDLE ;
    	atDrop (P, CAUSE_SHUT);
		fax1RspNumber (P, R_ERROR) ;
		if (P->At.InactivityAction == 0)
		{
			P->Class = P_CLASS_DATA;
			P->Mode = P_MODE_MODEM;
		}
		break ;

#ifdef FAX1_V34FAX
	case FAX1_TIMER_TASK_CLEARDOWN:
		fax1V34Cleardown (P);
		break;

	case FAX1_TIMER_TASK_V8_INDICATE_CNG:
        faxRsp (P, "+A8C:1");
 		fax1ScheduleTimer (P, FAX1_TIMER_TASK_V8_INDICATE_CNG,
		                      3500) ;
		break;
#endif /* FAX1_V34FAX */

	case FAX1_TIMER_TASK_NONE:
	default:
		break ;
	}
}

int fax1CmdTS (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	return (
		fax1Silence (P, Req, Val, "TS"));
}

int fax1CmdRS (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	return (
		fax1Silence (P, Req, Val, "RS"));
}

int fax1CmdTH (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	word Mod ;

	if ( Req != FAX_REQ_SET )
		return (fax1QueryMod (P, Req, "TH")) ;

	if ( !fax1XlateMod (P, "TH", Val, &Mod ) )
	{
		fax1InactivityTimer (P) ;
		return R_ERROR ;
	}
	return ( fax1Reconfig (P, Mod, P->port_u.Fax1.TxRxDelay) ) ;

	/* R_ERROR for errors, R_VOID if we wait for carrier now */
}

int fax1CmdRH (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	word Mod ;
	byte *Frame ;
	word sizeFrame ;

	if ( Req != FAX_REQ_SET )
		return (fax1QueryMod (P, Req, "RH")) ;

	if ( !fax1XlateMod (P, "RH", Val, &Mod ) )
	{
		fax1InactivityTimer (P) ;
		return R_ERROR ;
	}
	if ( (Mod != P->port_u.Fax1.Modulation)
	  || ((P->port_u.Fax1.Phase != FAX1_PHASE_DCD)
	   && (P->port_u.Fax1.Phase != FAX1_PHASE_RECV)) )
	{
		return ( fax1Reconfig (P, Mod, P->port_u.Fax1.TxRxDelay) ) ;

		/* R_ERROR for errors, R_VOID if we wait for carrier now */
	}

	Frame = queuePeekMsg (&P->port_u.Fax1.RxQueue, &sizeFrame) ;

	if ( !Frame )
	{
		if ( P->port_u.Fax1.Phase == FAX1_PHASE_RECV )
			fax1RspNumber (P, R_CONNECT_300) ;
		P->State = P_DATA ;		/* give a chance to cancel	*/
		return R_VOID ;			/* response sent already	*/
	}

	sizeFrame-- ;
	switch ( Frame[sizeFrame] )
	{
	case CLASS1_INDICATION_HDLC_DATA:

		fax1RspNumber (P, R_CONNECT_300) ;
		portRxEnqueue (P, Frame, sizeFrame, 0/*stream mode*/) ;
		queueFreeMsg (&P->port_u.Fax1.RxQueue) ;
		fax1RspNumber (P, R_OK) ;
		break ;

	case CLASS1_INDICATION_WRONG_CRC_HDLC_DATA:

		fax1RspNumber (P, R_CONNECT_300) ;
		portRxEnqueue (P, Frame, sizeFrame, 0/*stream mode*/) ;
		queueFreeMsg (&P->port_u.Fax1.RxQueue) ;
		fax1RspNumber (P, R_ERROR) ;
		break ;

	case CLASS1_INDICATION_FLUSHED_HDLC_DATA:

		fax1RspNumber (P, R_ERROR) ;
		fax1Deconfig (P) ;
		break ;
	}

	P->State = P_ESCAPE ;	/* need +FRH before next frame	*/
	fax1InactivityTimer (P) ;
	return R_VOID ;			/* response sent already	*/
}

int fax1CmdTM (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	word Mod ;

	if ( Req != FAX_REQ_SET )
		return (fax1QueryMod (P, Req, "TM")) ;

	if ( !fax1XlateMod (P, "TM", Val, &Mod ) )
	{
		fax1InactivityTimer (P) ;
		return R_ERROR ;
	}
	return ( fax1Reconfig (P, Mod, P->port_u.Fax1.TxRxDelay) ) ;

	/* R_ERROR for errors, R_VOID if we wait for carrier now */
}

int fax1CmdRM (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	word Mod ;

	if ( Req != FAX_REQ_SET )
		return (fax1QueryMod (P, Req, "RM")) ;

	if ( !fax1XlateMod (P, "RM", Val, &Mod ) )
	{
		fax1InactivityTimer (P) ;
		return R_ERROR ;
	}
	return ( fax1Reconfig (P, Mod, P->port_u.Fax1.TxRxDelay) ) ;

	/* R_ERROR for errors, R_VOID if we wait for carrier now */
}

int fax1CmdAR (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FAR? query adaptive reception control state */
        faxRsp (P, P->At.Fax1.AdaptiveReceptionControl ? "1" : "0");
		break;

	case FAX_REQ_RNG:	/* +FAR=? query adaptive reception control support */
        faxRsp (P, "(0,1)");
        break;

	case FAX_REQ_SET:	/* +FAR=n set adaptive reception control state */
        P->At.Fax1.AdaptiveReceptionControl = (Val != 0);
		break;
	}
	return R_OK ;
}

int fax1Cmd34 (ISDN_PORT *P, int Req, int Val, byte **Str)
{	/* +F34=[maxp][,[minp][,prevc][,maxp2][,minp2]] */
#ifdef FAX1_V34FAX
	byte Buf[16];
	byte *p;
	byte maxp, minp, prefc, maxp2, minp2;
	int i;

	switch (Req)
	{
	case FAX_REQ_VAL:	/* +F34? query V.34 FAX setup              */
		sprintf (Buf, "+F34: %d,%d,%d,%d,%d",
		              P->At.Fax1.V34FaxMaxPrimaryRateDiv2400,
		              P->At.Fax1.V34FaxMinPrimaryRateDiv2400,
		              P->At.Fax1.V34FaxPrefControlRateDiv1200,
		              P->At.Fax1.V34FaxMaxReceiveRateDiv2400,
		              P->At.Fax1.V34FaxMinReceiveRateDiv2400);
        faxRsp (P, Buf);
		break;

	case FAX_REQ_RNG:	/* +F34=? query V.34 FAX support           */
        faxRsp (P, "+F34: (0-14),(0-14),(0-2),(0-14),(0-14)");
        break;

	case FAX_REQ_SET:	/* +F34=maxp,minp,... set V.34 FAX setup   */
		p = *Str;
		maxp = 0;
		minp = 0;
		prefc = 0;
		maxp2 = 0;
		minp2 = 0;
		i = atNumArg (&p);
		if (i > 14)
			return (R_ERROR);
		if (i >= 0)
			maxp = (byte) i;
		if (*p == ',')
		{
			p++;
			i = atNumArg (&p);
			if (i > 14)
				return (R_ERROR);
			if (i >= 0)
				minp = (byte) i;
			if (*p == ',')
			{
				p++;
				i = atNumArg (&p);
				if (i > 2)
					return (R_ERROR);
				if (i >= 0)
					prefc = (byte) i;
				if (*p == ',')
				{
					p++;
					i = atNumArg (&p);
					if (i > 14)
						return (R_ERROR);
					if (i >= 0)
						maxp2 = (byte) i;
					if (*p == ',')
					{
						p++;
						i = atNumArg (&p);
						if (i > 14)
							return (R_ERROR);
						if (i >= 0)
							minp2 = (byte) i;
					}
				}
			}
		}
		*Str = p;
		if (minp > maxp)
			minp = maxp;
		if (minp2 > maxp2)
			minp2 = maxp2;
        P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = maxp;
        P->At.Fax1.V34FaxMinPrimaryRateDiv2400 = minp;
        P->At.Fax1.V34FaxPrefControlRateDiv1200 = prefc;
        P->At.Fax1.V34FaxMaxReceiveRateDiv2400 = maxp2;
        P->At.Fax1.V34FaxMinReceiveRateDiv2400 = minp2;
		if (P->At.Fax1.State == FAX1_STATE_UP)
		{
			if (P->port_u.Fax1.Phase == FAX1_PHASE_V8)
			{
				P->At.Fax1.V34Fax = TRUE;
			}
			else
			{
				if (!P->At.Fax1.V34Fax)
				{
					DBG_FTL(("[%p:%s] fax1F34: up but not V.34 FAX mode", P->Channel, P->Name))
					return R_ERROR ;
				}
				fax1SendV34FaxSetup (P);
			}
		}
		else
		{
			P->At.Fax1.V34Fax = TRUE;
		}
		break;
	}
	return R_OK ;
#else /* FAX1_V34FAX */
	return R_ERROR ;
#endif /* FAX1_V34FAX */
}

int fax1CmdCL (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	byte Buf[16];

	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FCL? query carrier loss timeout value  */
		sprintf (Buf, "%d", P->At.Fax1.CarrierLossTimeout);
        faxRsp (P, Buf);
		break;

	case FAX_REQ_RNG:	/* +FCL=? query carrier loss timeout range */
        faxRsp (P, "(0-255)");
        break;

	case FAX_REQ_SET:	/* +FCL=n set carrier loss timeout         */
        P->At.Fax1.CarrierLossTimeout = (byte) Val;
		break;
	}
	return R_OK ;
}

int fax1CmdDD (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FDD? query current double escape state */
        faxRsp (P, P->At.Fax1.DoubleEscape ? "1" : "0");
		break;

	case FAX_REQ_RNG:	/* +FDD=? query double escape support      */
        faxRsp (P, "(0,1)");
        break;

	case FAX_REQ_SET:	/* +FDD=n set double escape state          */
        P->At.Fax1.DoubleEscape = (Val != 0);
		break;
	}
	return R_OK ;
}

void fax1Init (ISDN_PORT *P)
{	/* (re)set CLASS1 specific data to initial state (AT&F, AT&Z processing)*/
	if ( P->At.Fax1.State == FAX1_STATE_UP )
	{
		fax1CancelTimer (P) ;
		P->At.Fax1.State = FAX1_STATE_IDLE ;
	}
#ifdef FAX1_V34FAX
	P->At.V8.OrgNegotiationMode = P_V8_ORG_DCE_CONTROLLED;
	P->At.V8.AnsNegotiationMode = P_V8_ANS_DCE_CONTROLLED;
	P->At.V8.CILength = 0;
	P->At.V8.V8bisNegotiationMode = 0;
	P->At.V8.AcceptCallFunctionCount = 0;
	P->At.V8.AcceptProtocolCount = 0;
	P->At.V8.CMLength = 0;
	P->At.V8.JMLength = 0;
	P->At.Fax1.AdaptiveReceptionControl = FALSE;
	P->At.Fax1.CarrierLossTimeout = 0;
	P->At.Fax1.DoubleEscape = FALSE;
	P->At.Fax1.V34Fax = FALSE;
	P->At.Fax1.V34FaxFlags = 0;
	P->At.Fax1.V34FaxMinPrimaryRateDiv2400 = 0;
	P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 0;
	P->At.Fax1.V34FaxPrefControlRateDiv1200 = 0;
	P->At.Fax1.V34FaxMinReceiveRateDiv2400 = 0;
	P->At.Fax1.V34FaxMaxReceiveRateDiv2400 = 0;
#endif /* FAX1_V34FAX */
}

void fax1Finit (ISDN_PORT *P)
{
	if ( P->At.Fax1.State == FAX1_STATE_UP )
	{
		fax1CancelTimer (P) ;
		P->At.Fax1.State = FAX1_STATE_IDLE ;
	}
}

void fax1SetClass (ISDN_PORT *P)
{	/* called when an AT+FCLASS=1 command was detected */
	if ( P->At.Fax1.State == FAX1_STATE_UP )
	{
		DBG_FTL(("[%p:%s] fax1SetClass: Still UP", P->Channel, P->Name))
	}
}

T30_INFO *fax1Connect (ISDN_PORT *P)
{	/* Called by atDial() or atAnswer() to get the FAX parameters (if any).	*/
	/* If the connection succeeds fax1Up() will be called next but there is	*/
	/* no callback for a failing connection.								*/
	static T30_INFO fake ;
	fake.atf = IDI_ATF_CLASS1 ;	/* that's all IDI must know */
	return ( &fake ) ;
}

void fax1Up (ISDN_PORT *P)
{
#ifdef FAX1_V34FAX
	byte pos;
#endif /* FAX1_V34FAX */

	/* Called by PortUp(), i.e an inbound or outbound call was successfully	*/
	/* established and the B-channel is ready.								*/

	/* Set the basic state of the machine to UP, let the app do T.30 stuff	*/

	P->At.Fax1.State = FAX1_STATE_UP ;

	/* Initialize the temporary data structures in the shared data area	*/

	mem_zero (&P->port_u.Fax1, sizeof(P->port_u.Fax1)) ;
	queueInit (&P->port_u.Fax1.RxQueue,
			   &P->port_u.Fax1.FrameBuf[0], sizeof(P->port_u.Fax1.FrameBuf)) ;
	sysInitializeTimer (&P->port_u.Fax1.Timer, (sys_CALLBACK)fax1Timer, P) ;

	/* Set up the modulation for the initial negotiation phase.		*/
	/* In Originating mode we receive, in answer mode we send first	*/

#ifdef FAX1_V34FAX
	if ( P->Direction == P_CONN_OUT )
	{
		if (P->At.V8.OrgNegotiationMode == P_V8_ORG_DISABLED)
		{
			P->At.Fax1.V34Fax = FALSE;
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		|
									CLASS1_RECONFIGURE_HDLC_FLAG,
									0) ;
		}
		else
		{
			if ((P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_CONTROLLED)
			 || (P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_WITH_INDICATIONS))
			{
				if (P->At.Fax1.V34Fax)
				{
					pos = fax1FindV8Octet (P->At.V8.CIBuffer, P->At.V8.CILength,
					                       V8_CATEGORY_CALL_FUNCTION, 0);
					if ((pos < P->At.V8.CILength)
					 && ((P->At.V8.CIBuffer[pos] & V8_CATEGORY_INFO_MASK) == V8_CALLF0_T30_POLLING))
					{
						P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
					}
					else
					{
						P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY;
					}
					fax1SendV34FaxSetup (P);
				}
				(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		|
										CLASS1_RECONFIGURE_HDLC_FLAG,
										0) ;
				if (P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_WITH_INDICATIONS)
				{
			        faxRsp (P, "+A8A:1");
				}
			}
			else
			{
		        faxRsp (P, "+A8A:1");
				P->port_u.Fax1.Phase = FAX1_PHASE_V8;
				fax1RspNumber (P, R_OK) ;
				P->State = P_ESCAPE ;
				fax1InactivityTimer (P) ;
			}
		}
	}
	else
	{
		if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DISABLED)
		{
			P->At.Fax1.V34Fax = FALSE;
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
									CLASS1_RECONFIGURE_HDLC_FLAG |
									CLASS1_RECONFIGURE_TX_FLAG,
									0) ;
		}
		else if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DTE_SEND_SILENCE)
		{
	 		fax1ScheduleTimer (P, FAX1_TIMER_TASK_V8_INDICATE_CNG,
			                      300) ;
			P->port_u.Fax1.Phase = FAX1_PHASE_V8;
			P->State = P_DATA ;		/* expecting to be canceled	*/
		}
		else
		{
			if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DTE_SEND_ANS)
			{
				P->At.Fax1.V34Fax = FALSE;
			}
			else
			{
				if (P->At.Fax1.V34Fax)
				{
					P->At.Fax1.V34FaxFlags = 0;
					for (pos = 0; pos < P->At.V8.AcceptCallFunctionCount; pos++)
					{
						switch (P->At.V8.AcceptCallFunctionTable[pos])
						{
						case P_V8_CALL_FUNCTION_T30_NORMAL:
							P->At.Fax1.V34FaxFlags |= CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY;
							break;
						case P_V8_CALL_FUNCTION_T30_POLLING:
							P->At.Fax1.V34FaxFlags |= CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
							break;
						}
					}
					if (P->At.Fax1.V34FaxFlags == 0)
					{
						P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY |
						                         CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
					}
					fax1SendV34FaxSetup (P);
				}
			}
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
									CLASS1_RECONFIGURE_HDLC_FLAG |
									CLASS1_RECONFIGURE_TX_FLAG,
									0) ;
		}
	}
#else /* FAX1_V34FAX */
	if ( P->Direction == P_CONN_OUT )
	{
		(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		|
								CLASS1_RECONFIGURE_HDLC_FLAG,
								0) ;
	}
	else
	{
		(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
								CLASS1_RECONFIGURE_HDLC_FLAG |
								CLASS1_RECONFIGURE_TX_FLAG,
								0) ;
	}
#endif /* FAX1_V34FAX */
}

int fax1V8Setup (ISDN_PORT *P)
{
#ifdef FAX1_V34FAX
	byte pos;

	if ( P->At.Fax1.State != FAX1_STATE_UP )
		return R_OK;

	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_V8:
		if ( P->Direction == P_CONN_OUT )
		{
			DBG_FTL(("[%p:%s] fax1V8Setup: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
			return R_ERROR;
		}
		else
		{
			if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DISABLED)
			{
				P->At.Fax1.V34Fax = FALSE;
				fax1SendV34FaxSetup (P);
				(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
										CLASS1_RECONFIGURE_HDLC_FLAG |
										CLASS1_RECONFIGURE_TX_FLAG,
										0) ;
			}
			else if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DTE_SEND_SILENCE)
			{
		 		fax1ScheduleTimer (P, FAX1_TIMER_TASK_V8_INDICATE_CNG,
				                      300) ;
				P->port_u.Fax1.Phase = FAX1_PHASE_V8;
				P->State = P_DATA ;		/* expecting to be canceled	*/
			}
			else
			{
				if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DTE_SEND_ANS)
				{
					P->At.Fax1.V34Fax = FALSE;
				}
				else
				{
					if (P->At.Fax1.V34Fax)
					{
						P->At.Fax1.V34FaxFlags = 0;
						for (pos = 0; pos < P->At.V8.AcceptCallFunctionCount; pos++)
						{
							switch (P->At.V8.AcceptCallFunctionTable[pos])
							{
							case P_V8_CALL_FUNCTION_T30_NORMAL:
								P->At.Fax1.V34FaxFlags |= CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY;
								break;
							case P_V8_CALL_FUNCTION_T30_POLLING:
								P->At.Fax1.V34FaxFlags |= CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
								break;
							}
						}
						if (P->At.Fax1.V34FaxFlags == 0)
						{
							P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY |
							                         CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
						}
					}
				}
				fax1SendV34FaxSetup (P);
				(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
										CLASS1_RECONFIGURE_HDLC_FLAG |
										CLASS1_RECONFIGURE_TX_FLAG,
										0) ;
			}
			return R_VOID;
		}
		break;

	default:
		DBG_FTL(("[%p:%s] fax1V8Setup: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
		return R_ERROR;
	}
#endif /* FAX1_V34FAX */
	return R_OK;
}

int fax1V8Menu (ISDN_PORT *P)
{
#ifdef FAX1_V34FAX
	byte pos;

	if ( P->At.Fax1.State != FAX1_STATE_UP )
		return R_OK;

	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_V8:
		if ( P->Direction == P_CONN_OUT )
		{
			if (P->At.Fax1.V34Fax)
			{
				pos = fax1FindV8Octet (P->At.V8.CMBuffer, P->At.V8.CMLength,
				                       V8_CATEGORY_CALL_FUNCTION, 0);
				if (pos >= P->At.V8.CMLength)
				{
					P->At.Fax1.V34Fax = FALSE;
				}
				else
				{
					switch (P->At.V8.CMBuffer[pos] & V8_CATEGORY_INFO_MASK)
					{
					case V8_CALLF0_T30_POLLING:
						P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_POLLING_CAPABILITY;
						break;
					case V8_CALLF0_T30_NORMAL:
						P->At.Fax1.V34FaxFlags = CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY;
						break;
					default:
						P->At.Fax1.V34Fax = FALSE;
					}
				}
				pos = fax1FindV8Octet (P->At.V8.CMBuffer, P->At.V8.CMLength,
				                       V8_CATEGORY_MODULATION_MODES, 0);
				if ((pos >= P->At.V8.CMLength)
				 || !(P->At.V8.CMBuffer[pos] & V8_MODN0_V34HDX))
				{
					P->At.Fax1.V34Fax = FALSE;
				}
			}
			fax1SendV34FaxSetup (P);
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		|
									CLASS1_RECONFIGURE_HDLC_FLAG,
									0) ;
			return R_VOID;
		}
		else
		{
			DBG_FTL(("[%p:%s] fax1V8Menu: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
			return R_ERROR;
		}
		break;

	case FAX1_PHASE_TOCTRL:
	case FAX1_PHASE_CTRL:
	case FAX1_PHASE_TOPRI:
	case FAX1_PHASE_PRI:
		if ( P->Direction == P_CONN_OUT )
		{
			DBG_FTL(("[%p:%s] fax1V8Menu: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
			return R_ERROR;
		}
		break;

	default:
		DBG_FTL(("[%p:%s] fax1V8Menu: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
		return R_ERROR;
	}
#endif /* FAX1_V34FAX */
	return R_OK;
}

int fax1Online (ISDN_PORT *P)
{
#ifdef FAX1_V34FAX
	byte *Frame ;
	word sizeFrame ;

	if ( P->At.Fax1.State != FAX1_STATE_UP )
		return R_OK;

	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_V8:
		P->At.Fax1.V34Fax = FALSE;
		fax1SendV34FaxSetup (P);
		if ( P->Direction == P_CONN_OUT )
		{
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		|
									CLASS1_RECONFIGURE_HDLC_FLAG,
									0) ;
		}
		else
		{
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V25		 |
									CLASS1_RECONFIGURE_HDLC_FLAG |
									CLASS1_RECONFIGURE_TX_FLAG,
									0) ;
		}
		return R_VOID ;

	case FAX1_PHASE_TOCTRL:
	case FAX1_PHASE_CTRL:
	case FAX1_PHASE_TOPRI:
	case FAX1_PHASE_PRI:
        faxRsp (P, "+A8J:1");
		fax1RspF34 (P);
		fax1RspNumber (P, R_CONNECT_300) ;
		P->State = P_DATA ;
		while ((Frame = queuePeekMsg (&P->port_u.Fax1.RxQueue, &sizeFrame)))
		{
			sizeFrame--;
			portRxEnqueue (P, Frame, sizeFrame, 0/*stream mode*/) ;
			queueFreeMsg (&P->port_u.Fax1.RxQueue) ;
		}
		return R_VOID ;

	case FAX1_PHASE_XMIT:
		if ( P->Direction == P_CONN_OUT )
		{
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V21_CH2	|
									CLASS1_RECONFIGURE_HDLC_FLAG,
									0) ;
		}
		else
		{
			fax1RspNumber (P, R_CONNECT_300) ;
			P->State = P_DATA ;
			fax1ScheduleTimer (P, FAX1_TIMER_TASK_NO_XMIT,
			                      MILLISECONDS_UNTIL_XMIT);
			fax1InactivityTimer (P) ;
		}
		return R_VOID ;

	case FAX1_PHASE_RECV:
		if ( P->Direction == P_CONN_OUT )
		{
			fax1RspNumber (P, R_CONNECT_300) ;
			P->State = P_DATA ;
			Frame = queuePeekMsg (&P->port_u.Fax1.RxQueue, &sizeFrame) ;
			if ( Frame )
			{
				sizeFrame-- ;
				portRxEnqueue (P, Frame, sizeFrame, 0/*stream mode*/) ;
				queueFreeMsg (&P->port_u.Fax1.RxQueue) ;
				switch ( Frame[sizeFrame] )
				{
				case CLASS1_INDICATION_HDLC_DATA:
					fax1RspNumber (P, R_OK) ;
					break ;

				case CLASS1_INDICATION_WRONG_CRC_HDLC_DATA:
					fax1RspNumber (P, R_ERROR) ;
					break ;

				case CLASS1_INDICATION_FLUSHED_HDLC_DATA:
					fax1RspNumber (P, R_ERROR) ;
					fax1Deconfig (P) ;
					break ;
				}
				P->State = P_ESCAPE ;	/* need +FRH before next frame	*/
				fax1InactivityTimer (P) ;
			}
		}
		else
		{
			(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V21_CH2	 |
									CLASS1_RECONFIGURE_HDLC_FLAG |
									CLASS1_RECONFIGURE_TX_FLAG,
									0) ;
		}
		return R_VOID ;

	default:
		DBG_FTL(("[%p:%s] fax1Online: wrong phase %s", P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
		return R_ERROR;
	}
#endif /* FAX1_V34FAX */
	return R_OK;
}

int fax1Delay (ISDN_PORT *P, byte *Response, word sizeResponse, dword *RspDelay)
{
	/* Called by atRspQueue() just before a Response is entered to the pen-	*/
	/* ding response queue or to the receive queue.							*/
	/* Here we have the chance to adjust the delay to be used used for this	*/
	/* response by changing the value passed indirectly via '*RspDelay'.	*/
	/* The function return value defines the kind of delay handling:		*/
	/*  == 0  -	default response delay handling								*/
	/*  != 0  -	all pending responses are flushed und the current response	*/
	/*			is delayed by the value written to '*RspDelay'.				*/

	return (0) ;	/* no need to change anything */
}

int fax1Hangup (ISDN_PORT *P)
{
	/* Called by atDtrOff() or PortDown() if a successful connection which	*/
	/* was indicated via fax1Up() before is disconnected again.				*/

	if ( P->At.Fax1.State == FAX1_STATE_UP )
	{
		DBG_CONN(("[%p:%s] fax1Hangup: phase=%s",
				  P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
   		if ( fax1AbortRecv (P) )
			DBG_CONN(("[%p:%s] fax1Hangup: aborted recv", P->Channel, P->Name))
		fax1CancelTimer (P) ;
	}
	P->At.Fax1.State = FAX1_STATE_IDLE ;

	/* ???:  not any time it is correct to send a NO CARRIER response.
	         We have to check the current state and send the corresponding
	         response. */
	return (1) ;	/* we lost the carrier*/
}

static void fax1StopXmit (ISDN_PORT *P)
{
	DBG_CONN(("[%p:%s] fax1StopXmit: Phase %s -> TERM, Config=IDLE",
			  P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
	fax1Deconfig (P) ;
	fax1EnqIdle (P) ;
	P->port_u.Fax1.Phase = FAX1_PHASE_TERM ;
}

word fax1WriteHdlc (ISDN_PORT *P, byte **Data, word sizeData)
{
	byte *pData ;
	int   written;
#ifdef FAX1_V34FAX
	byte ReqBuf[4];
#endif /* FAX1_V34FAX */

	for ( pData = *Data ; sizeData-- ; pData++ )
	{
		if ( P->port_u.Fax1.TxDlePending )
		{
			P->port_u.Fax1.TxDlePending = FALSE ;

#ifdef FAX1_V34FAX
			if (P->At.Fax1.V34Fax)
			{
				switch (*pData)
				{
				case FAX1_CHAR_DC1: /* Xon */
					P->RxXoffFlow = FALSE;
					break;
				case FAX1_CHAR_DC3: /* Xoff */
					P->RxXoffFlow = TRUE;
					break;
				case FAX1_CHAR_ETX:
					written = portXmit (P, P->port_u.Fax1.FrameBuf,
					                       P->port_u.Fax1.FrameLen, N_DATA) ;
					/* ???: no flow control check */
					if ((P->port_u.Fax1.Phase == FAX1_PHASE_CTRL) && !P->port_u.Fax1.V34FaxMark)
					{
						if (!P->port_u.Fax1.V34FaxSource
						 && (P->port_u.Fax1.FrameLen >= 3)
						 && (P->port_u.Fax1.FrameBuf[0] == T4_ECM_ADDRESS_GSTN)
						 && (P->port_u.Fax1.FrameBuf[1] == (T4_ECM_CONTROL|T4_ECM_FINAL_FRAME_BIT))
						 && (((P->port_u.Fax1.FrameLen == 3) && (P->port_u.Fax1.FrameBuf[2] == T30_CFR))
						  || ((P->port_u.Fax1.FrameLen == 3) && (P->port_u.Fax1.FrameBuf[2] == T30_MCF))
						  || ((P->port_u.Fax1.FrameLen >= 3) && (P->port_u.Fax1.FrameBuf[2] == T30_PPR))
						  || ((P->port_u.Fax1.FrameLen == 3) && (P->port_u.Fax1.FrameBuf[2] == T30_CTR))
						  || ((P->port_u.Fax1.FrameLen == 3) && (P->port_u.Fax1.FrameBuf[2] == T30_ERR))))
						{
							ReqBuf[0] = CLASS1_REQUEST_DETECT_MARK;
							ReqBuf[1] = V34FAX_MARK_BITS_TO_DETECT & 0xff;
							ReqBuf[2] = V34FAX_MARK_BITS_TO_DETECT >> 8;
							written = portXmit (P, ReqBuf, 3, N_UDATA) ;
							P->port_u.Fax1.V34FaxMark = TRUE;
						}
						else if (P->port_u.Fax1.V34FaxSource
						 && (P->port_u.Fax1.FrameLen >= 3)
						 && (P->port_u.Fax1.FrameBuf[0] == T4_ECM_ADDRESS_GSTN)
						 && (P->port_u.Fax1.FrameBuf[1] == (T4_ECM_CONTROL|T4_ECM_FINAL_FRAME_BIT))
						 && (P->port_u.Fax1.FrameBuf[2] == T30_DTC))
						{
							ReqBuf[0] = CLASS1_REQUEST_DETECT_MARK;
							ReqBuf[1] = V34FAX_MARK_BITS_TO_DETECT & 0xff;
							ReqBuf[2] = V34FAX_MARK_BITS_TO_DETECT >> 8;
							written = portXmit (P, ReqBuf, 3, N_UDATA) ;
							P->port_u.Fax1.V34FaxMark = TRUE;
							DBG_FTL(("[%p:%s] fax1WriteHdlc: [DTC] cannot turnaround %s",
							         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
							fax1V34Cleardown (P);
						}
					}
					P->port_u.Fax1.FrameLen = 0 ;
					break;
				case FAX1_CHAR_EOT:
					if (P->port_u.Fax1.V34FaxSource && P->port_u.Fax1.V34FaxControl)
					{
						ReqBuf[0] = CLASS1_REQUEST_SEND_MARK;
						ReqBuf[1] = V34FAX_MARK_BITS_TO_SEND & 0xff;
						ReqBuf[2] = V34FAX_MARK_BITS_TO_SEND >> 8;
						written = portXmit (P, ReqBuf, 3, N_UDATA) ;
						/* ???: no flow control check */
						P->port_u.Fax1.V34FaxMark = TRUE;
					}
					fax1V34Cleardown (P);
					break;
				case FAX1_CHAR_PRI:
					if (!P->port_u.Fax1.V34FaxSource)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><PRI> from recipient", P->Channel, P->Name)) ;
						break;
					}
					if (P->port_u.Fax1.Phase != FAX1_PHASE_CTRL)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><PRI> in bad phase %s",
						         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
						break;
					}
					if (P->port_u.Fax1.V34FaxControl)
					{
						ReqBuf[0] = CLASS1_REQUEST_SEND_MARK;
						ReqBuf[1] = V34FAX_MARK_BITS_TO_SEND & 0xff;
						ReqBuf[2] = V34FAX_MARK_BITS_TO_SEND >> 8;
						written = portXmit (P, ReqBuf, 3, N_UDATA) ;
						/* ???: no flow control check */
						P->port_u.Fax1.V34FaxMark = TRUE;
					}
					else
					{
						(void) fax1V34FaxReconfig (P, CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL |
						                              CLASS1_RECONFIGURE_HDLC_FLAG |
						                              CLASS1_RECONFIGURE_TX_FLAG) ;
					}
					P->port_u.Fax1.Phase = FAX1_PHASE_TOPRI;
					break;
				case FAX1_CHAR_CTRL:
					if (!P->port_u.Fax1.V34FaxSource)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><CTRL> from recipient", P->Channel, P->Name)) ;
						break;
					}
					if (P->port_u.Fax1.Phase != FAX1_PHASE_PRI)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><CTRL> in bad phase %s",
						         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
						break;
					}
					(void) fax1V34FaxReconfig (P, CLASS1_RECONFIGURE_V34_CONTROL_CHANNEL |
					                              CLASS1_RECONFIGURE_HDLC_FLAG |
					                              CLASS1_RECONFIGURE_TX_FLAG) ;
					P->port_u.Fax1.Phase = FAX1_PHASE_TOCTRL;
					break;
				case FAX1_CHAR_PPH:
					fax1SendV34FaxSetup (P);
					break;
				case FAX1_CHAR_RTN:
					break;
				case FAX1_CHAR_MARK:
					if (P->port_u.Fax1.Phase != FAX1_PHASE_CTRL)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><MARK> in bad phase %s",
						         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
						break;
					}
					ReqBuf[0] = CLASS1_REQUEST_SEND_MARK;
					ReqBuf[1] = V34FAX_MARK_BITS_TO_SEND & 0xff;
					ReqBuf[2] = V34FAX_MARK_BITS_TO_SEND >> 8;
					written = portXmit (P, ReqBuf, 3, N_UDATA) ;
					/* ???: no flow control check */
					P->port_u.Fax1.V34FaxMark = TRUE;
					DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><MARK> turnaround polling not supported %s",
					         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
					fax1V34Cleardown (P);
					break;
				case FAX1_CHAR_RTNC:
					if (P->port_u.Fax1.Phase != FAX1_PHASE_CTRL)
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: <DLE><RTNC> in bad phase %s",
						         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase))) ;
						break;
					}
					ReqBuf[0] = CLASS1_REQUEST_RETRAIN;
					written = portXmit (P, ReqBuf, 1, N_UDATA) ;
					/* ???: no flow control check */
					break;
				case FAX1_CHAR_C12:
					P->At.Fax1.V34FaxPrefControlRateDiv1200 = 1;
					break;
				case FAX1_CHAR_C24:
					P->At.Fax1.V34FaxPrefControlRateDiv1200 = 2;
					break;
				case FAX1_CHAR_P24:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 1;
					break;
				case FAX1_CHAR_P48:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 2;
					break;
				case FAX1_CHAR_P72:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 3;
					break;
				case FAX1_CHAR_P96:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 4;
					break;
				case FAX1_CHAR_P120:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 5;
					break;
				case FAX1_CHAR_P144:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 6;
					break;
				case FAX1_CHAR_P168:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 7;
					break;
				case FAX1_CHAR_P192:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 8;
					break;
				case FAX1_CHAR_P216:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 9;
					break;
				case FAX1_CHAR_P240:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 10;
					break;
				case FAX1_CHAR_P264:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 11;
					break;
				case FAX1_CHAR_P288:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 12;
					break;
				case FAX1_CHAR_P312:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 13;
					break;
				case FAX1_CHAR_P336:
					P->At.Fax1.V34FaxMaxPrimaryRateDiv2400 = 14;
					break;
				case FAX1_CHAR_SUB:
					if ( P->port_u.Fax1.FrameLen > sizeof(P->port_u.Fax1.FrameBuf) - 2 )
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: overrun", P->Channel, P->Name))
						return (0);
					}
					P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = FAX1_CHAR_DLE ;
					P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = FAX1_CHAR_DLE ;
					break;
				case FAX1_CHAR_DLE:
					if ( P->port_u.Fax1.FrameLen >= sizeof(P->port_u.Fax1.FrameBuf) )
					{
						DBG_FTL(("[%p:%s] fax1WriteHdlc: overrun", P->Channel, P->Name))
						return (0);
					}
					P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = FAX1_CHAR_DLE ;
					break;
				}
		        if (P->At.Fax1.V34FaxMinPrimaryRateDiv2400 > P->At.Fax1.V34FaxMaxPrimaryRateDiv2400)
					P->At.Fax1.V34FaxMinPrimaryRateDiv2400 = P->At.Fax1.V34FaxMaxPrimaryRateDiv2400;
				continue;
			}
#endif /* FAX1_V34FAX */

			if ( *pData == FAX1_CHAR_ETX )
			{

				/* Frame complete - must send it here, and not past return	*/
				/* because we must drop carrier when sending final frame.	*/
				/* An empty frame stops the carrier.						*/

				if ( P->port_u.Fax1.FrameLen == 0 )
				{
					fax1StopXmit (P) ; /* "OK" response is sent after CTS_OFF */
					return (0) ;
				}

				written = portXmit (P, P->port_u.Fax1.FrameBuf,
				              P->port_u.Fax1.FrameLen, N_DATA) ;
				/* ???: no flow control check */

				if ( (P->port_u.Fax1.FrameLen >= 2)
				  && (P->port_u.Fax1.FrameBuf[1] & FAX1_FLAG_FINAL) )
				{
					fax1StopXmit (P) ; /* "OK" response is sent after CTS_OFF */
					return (0) ;
				}

				if ((P->port_u.Fax1.Modulation & CLASS1_RECONFIGURE_PROTOCOL_MASK) ==
				    CLASS1_RECONFIGURE_V21_CH2)
				{
					/* delay CONNECT until frame is sent */

					fax1ScheduleTimer (P, FAX1_TIMER_TASK_FRAME_SENT,
					                      MILLISECONDS_PER_FRAME_300(P->port_u.Fax1.FrameLen)) ;
				}
				else
				{
					DBG_PRV1(("[%p:%s] fax1WriteHdlc: send CONNECT", P->Channel, P->Name))
					fax1RspNumber (P, R_CONNECT_300) ;
					fax1InactivityTimer (P) ;
				}
				P->port_u.Fax1.FrameLen = 0 ;
				return (0) ;
			}
			if ( *pData != FAX1_CHAR_DLE )
			{
				if (P->Class == P_CLASS_FAX10)
				{
					switch (*pData)
					{
					case FAX1_CHAR_SUB:
						if ( P->port_u.Fax1.FrameLen > sizeof(P->port_u.Fax1.FrameBuf) - 2 )
						{
							DBG_FTL(("[%p:%s] fax1WriteHdlc: overrun", P->Channel, P->Name))
							return (0);
						}
						P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = FAX1_CHAR_DLE ;
						P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = FAX1_CHAR_DLE ;
						break;
					}
				}
				/* discard DLE-nonDLE sequences */
				continue ;
			}
		}
		else if ( *pData == FAX1_CHAR_DLE )
		{
			P->port_u.Fax1.TxDlePending = TRUE ;
			continue ;	/* remember DLE  */
		}

		/* Wow, we can try to save this byte in the frame buffer	*/

		if ( P->port_u.Fax1.FrameLen >= sizeof(P->port_u.Fax1.FrameBuf) )
		{
			DBG_FTL(("[%p:%s] fax1WriteHdlc: overrun", P->Channel, P->Name))
			return (0);
		}
		P->port_u.Fax1.FrameBuf[P->port_u.Fax1.FrameLen++] = *pData ;
	}

	/* Data buffered now, indicate that there is nothing to send   */

	return (0) ;
}

word fax1Write (ISDN_PORT *P,
                byte **Data, word sizeData,
                byte *Frame, word sizeFrame, word *FrameType)
{
	byte *pData, *pFrame, *qFrame ;
	int   written;
	BOOL  fAbort;

	if ( P->At.Fax1.State != FAX1_STATE_UP )
	{
		DBG_FTL(("[%p:%s] fax1Write: bad state", P->Channel, P->Name))
    	return (0); /* forget it */
	}

	/* stop the NO_XMIT timer set up after each CONNECT response */

	fax1CancelTimer (P) ;

	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_XMIT:
#ifdef FAX1_V34FAX
	case FAX1_PHASE_TOCTRL:
	case FAX1_PHASE_CTRL:
	case FAX1_PHASE_TOPRI:
	case FAX1_PHASE_PRI:
#endif /* FAX1_V34FAX */
		break;

	default:
		fAbort = FALSE;
		for ( pData = *Data ; sizeData-- ; pData++ )
		{
			switch (*pData)
			{
			case 0x11: /* Xon */
				P->RxXoffFlow = FALSE;
				break;
			case 0x13: /* Xoff */
				P->RxXoffFlow = TRUE;
				break;
			default:
				fAbort = TRUE;
			}
		}
		if ( fAbort )
		{
	   		if ( fax1AbortRecv (P) )
			{
				DBG_CONN(("[%p:%s] fax1Write: abort recv, phase=%s",
				         P->Channel, P->Name, fax1Phase(P->port_u.Fax1.Phase)))
				fax1RspNumber (P, R_OK) ;
			} else {
				DBG_FTL(("[%p:%s] fax1Write: bad phase=%s",
				         P->Channel, P->Name, fax1Phase(P->port_u.Fax1.Phase)))
			}
			fax1InactivityTimer (P) ;
		}
		return (0); /* forget it */
	}
	fax1InactivityTimer (P) ;

	if ( P->port_u.Fax1.Modulation & CLASS1_RECONFIGURE_HDLC_FLAG )
		return (fax1WriteHdlc (P, Data, sizeData));

	/* Normal stream mode */

	DBG_PRV1(("[%p:%s] fax1Write: got %d", P->Channel, P->Name, sizeData))

	pFrame = Frame ; qFrame = Frame + sizeFrame ;

	for ( pData = *Data ; sizeData-- ; pData++ )
	{
		if ( P->port_u.Fax1.TxDlePending )
		{
			P->port_u.Fax1.TxDlePending = FALSE ;

			if ( *pData == FAX1_CHAR_ETX )
			{
				/* Send complete - must send it here, and not past return	*/
				/* because we must drop carrier now							*/

				sizeData =  (word)(pFrame - Frame) ;
				if ( sizeData )
				{
					DBG_PRV1(("[%p:%s] fax1Write: put %d", P->Channel, P->Name, sizeData))
					written = portXmit (P, Frame, sizeData, N_DATA);
					/* ???: no flow control check */
				}

				DBG_CONN(("[%p:%s] fax1Write: DLE-ETX", P->Channel, P->Name))

				fax1StopXmit (P) ;

				return (0) ;
			}
			if ( *pData != FAX1_CHAR_DLE )
			{
				if (P->Class == P_CLASS_FAX10)
				{
					switch (*pData)
					{
					case FAX1_CHAR_SUB:
						if ( pFrame > qFrame + 2 )
						{
							DBG_PRV1(("[%p:%s] fax1Write: put %d", P->Channel, P->Name, pFrame - Frame))
							written = portXmit (P, Frame, (word)(pFrame - Frame), N_DATA) ;
							/* ???: no flow control check */
							pFrame = Frame ;
						}
						*(pFrame++) = FAX1_CHAR_DLE ;
						*(pFrame++) = FAX1_CHAR_DLE ;
						break;
					}
				}
				/* discard DLE-nonDLE sequences */
				continue ;
			}
		}
		else if ( *pData == FAX1_CHAR_DLE )
		{
			P->port_u.Fax1.TxDlePending = TRUE ;
				continue ;	/* remember DLE  */
		}

		/* Copy this byte to the frame buffer	*/

		if ( pFrame >= qFrame )
		{
			DBG_PRV1(("[%p:%s] fax1Write: put %d", P->Channel, P->Name, pFrame - Frame))
			written = portXmit (P, Frame, (word)(pFrame - Frame), N_DATA) ;
			/* ???: no flow control check */
			pFrame = Frame ;
		}

		*(pFrame++) = *pData ;
	}

	/* Send stripped copy of data in 'Frame' now */

	DBG_PRV1(("[%p:%s] fax1Write: put %d", P->Channel, P->Name, (pFrame - Frame)))

	*Data		= Frame ;
	*FrameType	= N_DATA ;

	return ( (word)(pFrame - Frame) ) ;
}

void fax1TxEmpty (ISDN_PORT *P)
{
	/* Called everytime when the port driver transmit queue goes empty.	*/
	/* This is mainly for CLASS2 to give em a chance to send OK on EOP.	*/
}

static word fax1AddEscape (ISDN_PORT *P, byte *pData, word sizeData,
	byte *Stream, word sizeStream, byte *RspString)
{
	byte *pStream, *qStream ;
	int n ;

	/* blow up the escapes */

	pStream = Stream ;
	qStream = Stream + sizeStream ;

	while ( sizeData-- )
	{
		if ( P->port_u.Fax1.RxDlePending )
		{
			P->port_u.Fax1.RxDlePending = FALSE ;
			if ( pStream >= qStream )
			{
				DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
				return (0) ;
			}
            if (P->At.Fax1.DoubleEscape && (*pData == FAX1_CHAR_DLE))
            {
				pData++ ;
				*(pStream++) = FAX1_CHAR_SUB ;
				continue ;
			}
			*(pStream++) = FAX1_CHAR_DLE ;
		}
		if ( pStream >= qStream )
		{
			DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
			return (0) ;
		}
		if ( (*(pStream++) = *(pData++)) == FAX1_CHAR_DLE )
		{
			P->port_u.Fax1.RxDlePending = TRUE ;
		}
	}

	if ( RspString != NULL )
	{
		/* complete DLE-DLE for transparency and append response */
		n = str_len (RspString) ;
		if ( pStream + 1 + n > qStream )
		{
			DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
			return (0) ;
		}
		if ( P->port_u.Fax1.RxDlePending )
		{
			P->port_u.Fax1.RxDlePending = FALSE ;
			*(pStream++) = FAX1_CHAR_DLE ;
		}
		if ( n != 0 )
		{
			memcpy (pStream, RspString, n) ;
			pStream += n ;
		}
	}

	return ( (word)(pStream - Stream) ) ;
}

static void fax1EscapeEnqueue (ISDN_PORT *P, byte *pData, word sizeData,
	byte *Stream, word sizeStream, byte *RspString, byte Type)
{
	byte *pStream, *qStream ;
	int n ;

	/* blow up the escapes */

	pStream = Stream ;
	qStream = Stream + sizeStream ;

	while ( sizeData-- )
	{
		if ( P->port_u.Fax1.RxDlePending )
		{
			P->port_u.Fax1.RxDlePending = FALSE ;
			if ( pStream >= qStream )
			{
				DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
				return ;
			}
            if (P->At.Fax1.DoubleEscape && (*pData == FAX1_CHAR_DLE))
            {
				pData++ ;
				*(pStream++) = FAX1_CHAR_SUB ;
				continue ;
			}
			*(pStream++) = FAX1_CHAR_DLE ;
		}
		if ( pStream >= qStream )
		{
			DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
			return ;
		}
		if ( (*(pStream++) = *(pData++)) == FAX1_CHAR_DLE )
		{
			P->port_u.Fax1.RxDlePending = TRUE ;
		}
	}

	/* complete DLE-DLE for transparency and append response */
	n = (RspString == NULL) ? 0 : str_len (RspString) ;
	if ( pStream + 1 + n + 1 > qStream )
	{
		DBG_FTL(("[%p:%s] fax1Recv: overrun", P->Channel, P->Name))
		return ;
	}
	if ( RspString != NULL )
	{
		if ( P->port_u.Fax1.RxDlePending )
		{
			P->port_u.Fax1.RxDlePending = FALSE ;
			*(pStream++) = FAX1_CHAR_DLE ;
		}
		if ( n != 0 )
		{
			memcpy (pStream, RspString, n) ;
			pStream += n ;
		}
	}
	*(pStream++) = Type ;

	if ( !queueWriteMsg (&P->port_u.Fax1.RxQueue, Stream, (word)(pStream - Stream)) )
		DBG_FTL(("[%p:%s] fax1EscapeEnqueue: %s overrun", P->Channel, P->Name, fax1DataInd (Type)));
}

static void fax1CtsDcdOn (ISDN_PORT *P, byte *pData, word sizeData)
{
	int Match;
	byte Type;
#ifdef FAX1_V34FAX
	dword d;
	word w;
#endif /* FAX1_V34FAX */

	Match	= FALSE;
	Type	= pData[0] ;

#ifdef FAX1_V34FAX
	if ( P->At.Fax1.V34Fax )
	{
		if ((sizeData >= 24)
		 && ((pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
		  || (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_PRIMARY)))
		{
			d = pData[12] | (((dword)(pData[13])) << 8) |
				(((dword)(pData[14])) << 16) | (((dword)(pData[15])) << 24);
			if (d != 0)
			{
				P->port_u.Fax1.V34FaxSource = TRUE;
				P->port_u.Fax1.V34FaxPrimaryRateDiv2400 = (byte)(d / 2400);
			}
			else
			{
				d = pData[16] | (((dword)(pData[17])) << 8) |
					(((dword)(pData[18])) << 16) | (((dword)(pData[19])) << 24);
				if (d != 0)
				{
					P->port_u.Fax1.V34FaxSource = FALSE;
					P->port_u.Fax1.V34FaxPrimaryRateDiv2400 = (byte)(d / 2400);
				}
			}
			w = pData[20] | (pData[21] << 8);
			P->port_u.Fax1.V34FaxControlTxRateDiv1200 = (byte)(w / 1200);
			w = pData[22] | (pData[23] << 8);
			P->port_u.Fax1.V34FaxControlRxRateDiv1200 = (byte)(w / 1200);
			if (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
			{
				P->port_u.Fax1.V34FaxControl = TRUE;
			}
			else
			{
				P->port_u.Fax1.V34FaxControl = FALSE;
				P->port_u.Fax1.V34FaxMark = FALSE;
			}
			switch (P->port_u.Fax1.Phase)
			{
			case FAX1_PHASE_TOV8:
			case FAX1_PHASE_V8:
			case FAX1_PHASE_TODISC:
			case FAX1_PHASE_DISC:
				break;

			case FAX1_PHASE_TOCTRL:
				if (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
				{
					if (Type == CLASS1_INDICATION_DCD_ON)
					{
						fax1V34EscapeRate (P, TRUE, NULL);
						P->port_u.Fax1.Phase = FAX1_PHASE_CTRL;
					}
					Match = TRUE;
				}
				break;

			case FAX1_PHASE_CTRL:
				if (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
				{
					if (Type == CLASS1_INDICATION_DCD_ON)
					{
						fax1V34EscapeRate (P, TRUE, _DLE_RTNC_);
					}
					Match = TRUE;
				}
				break;

			case FAX1_PHASE_TOPRI:
				if (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
				{
#if 0
					if (Type == CLASS1_INDICATION_DCD_ON)
					{
						fax1V34EscapeRate (P, TRUE, _DLE_RTNC_);
					}
#endif
					Match = TRUE;
				}
				else
				{
					if ((P->port_u.Fax1.V34FaxSource && (Type == CLASS1_INDICATION_CTS_ON))
					 || (!P->port_u.Fax1.V34FaxSource && (Type == CLASS1_INDICATION_DCD_ON)))
					{
						fax1V34EscapeRate (P, FALSE, NULL);
						P->port_u.Fax1.Phase = FAX1_PHASE_PRI;
						Match = TRUE;
					}
				}
				break;

			case FAX1_PHASE_PRI:
				break;

			default:
				if (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL)
				{
					if (Type == CLASS1_INDICATION_DCD_ON)
					{
						if ( P->Direction == P_CONN_OUT )
						{
							if ((P->At.V8.OrgNegotiationMode == P_V8_ORG_DISABLED)
							 || (P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_CONTROLLED)
							 || (P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_WITH_INDICATIONS))
							{
								if (P->At.V8.OrgNegotiationMode == P_V8_ORG_DCE_WITH_INDICATIONS)
								{
									fax1RspV8JointMenu (P);
							        faxRsp (P, "+A8J:1");
								}
								fax1RspF34 (P);
								fax1RspNumber (P, R_CONNECT_300) ;
								fax1V34EscapeRate (P, TRUE, NULL);
							}
							else
							{
								fax1RspV8JointMenu (P);
								fax1RspNumber (P, R_OK) ;
								P->State = P_ESCAPE ;
								fax1V34EscapeRate (P, TRUE, NULL);
								fax1InactivityTimer (P) ;
							}
						}
						else
						{
							if ((P->At.V8.AnsNegotiationMode == P_V8_ANS_DISABLED)
							 || (P->At.V8.AnsNegotiationMode == P_V8_ANS_DCE_CONTROLLED)
							 || (P->At.V8.AnsNegotiationMode == P_V8_ANS_DCE_WITH_INDICATIONS))
							{
								if (P->At.V8.AnsNegotiationMode == P_V8_ANS_DCE_WITH_INDICATIONS)
								{
									fax1RspV8CallMenu (P);
							        faxRsp (P, "+A8J:1");
								}
								fax1RspF34 (P);
								fax1RspNumber (P, R_CONNECT_300) ;
								fax1V34EscapeRate (P, TRUE, NULL);
							}
							else
							{
								fax1RspV8CallMenu (P);
								fax1RspNumber (P, R_OK) ;
								P->State = P_ESCAPE ;
								fax1V34EscapeRate (P, TRUE, NULL);
								fax1InactivityTimer (P) ;
							}
						}
						P->port_u.Fax1.Phase = FAX1_PHASE_CTRL;
					}
					Match = TRUE;
				}
				break;
			}
			if (!Match)
			{
				DBG_FTL(("[%p:%s] fax1CtsDcdOn: unexpected %s %s in %s %s",
				         P->Channel, P->Name,
						 (pData[3] == CLASS1_CONNECTED_NORM_V34FAX_CONTROL) ? "CTRL" : "PRI",
				         fax1CtrlInd (Type), fax1Phase (P->port_u.Fax1.Phase),
				         P->port_u.Fax1.V34FaxSource ? "SRC" : "RCV"));
			}
			return;
		}
		else
		{
			DBG_CONN(("[%p:%s] fax1CtsDcdOn: fallback %s in %s",
			         P->Channel, P->Name, fax1CtrlInd (Type), fax1Phase (P->port_u.Fax1.Phase)));
			P->port_u.Fax1.V34FaxMark = FALSE;
			P->port_u.Fax1.V34FaxControl = FALSE;
			P->At.Fax1.V34Fax = FALSE;
			queueReset (&P->port_u.Fax1.RxQueue) ;
			fax1SendV34FaxSetup (P);
			switch (P->port_u.Fax1.Phase)
			{
			case FAX1_PHASE_TOV8:
			case FAX1_PHASE_V8:
			case FAX1_PHASE_TODISC:
			case FAX1_PHASE_DISC:
				return;

			case FAX1_PHASE_TOCTRL:
			case FAX1_PHASE_CTRL:
			case FAX1_PHASE_TOPRI:
			case FAX1_PHASE_PRI:
				if (P->State == P_DATA)
				{
					portRxEnqueue (P, _DLE_EOT_, 2, 0/*stream mode*/) ;
					if (Type == CLASS1_INDICATION_DCD_ON)
					{
						/* start receiving, enqueue IDLE reconfiguration already	*/
						/* here to get the FLUSHED*DATA message asap after DCD_OFF.	*/
						P->port_u.Fax1.Modulation = CLASS1_RECONFIGURE_V21_CH2 |
						                       CLASS1_RECONFIGURE_HDLC_FLAG;
						P->port_u.Fax1.Phase = FAX1_PHASE_RECV ;
						fax1CancelTimer (P) ;
						fax1EnqIdle (P) ;
						if (P->At.f.Verbose)
							faxRsp (P, "+FCERROR") ;
						else
							faxRsp (P, "+F4") ;
					}
					else
					{
						fax1StopXmit (P);
						fax1RspNumber (P, R_OK) ;
					}
					P->State = P_ESCAPE ;	/* need +FRH before next frame	*/
					fax1InactivityTimer (P) ;
					return;
				}
				if (Type == CLASS1_INDICATION_DCD_ON)
				{
					/* start receiving, enqueue IDLE reconfiguration already	*/
					/* here to get the FLUSHED*DATA message asap after DCD_OFF.	*/
					P->port_u.Fax1.Modulation = CLASS1_RECONFIGURE_V21_CH2 |
					                       CLASS1_RECONFIGURE_HDLC_FLAG;
					P->port_u.Fax1.Phase = FAX1_PHASE_DCD ;
				}
				else
				{
					P->port_u.Fax1.Modulation = CLASS1_RECONFIGURE_V21_CH2   |
					                       CLASS1_RECONFIGURE_HDLC_FLAG |
					                       CLASS1_RECONFIGURE_TX_FLAG;
					P->port_u.Fax1.Phase = FAX1_PHASE_CTS ;
				}
			default:
				if ( P->Direction == P_CONN_OUT )
				{
					if ((P->At.V8.OrgNegotiationMode != P_V8_ORG_DISABLED)
					 && (P->At.V8.OrgNegotiationMode != P_V8_ORG_DCE_CONTROLLED))
					{
						fax1RspV8JointMenu (P);
						if (P->At.V8.OrgNegotiationMode != P_V8_ORG_DCE_WITH_INDICATIONS)
						{
							if (P->port_u.Fax1.Phase == FAX1_PHASE_DCD)
							{
								P->port_u.Fax1.Phase = FAX1_PHASE_RECV ;
								fax1CancelTimer (P) ;
								fax1EnqIdle (P) ;
							}
							else
							{
								P->port_u.Fax1.Phase = FAX1_PHASE_XMIT ;
							}
							fax1RspNumber (P, R_OK) ;
							P->State = P_ESCAPE ;	/* need ATO	*/
							fax1InactivityTimer (P) ;
							return;
						}
					}
				}
				else
				{
					if ((P->At.V8.AnsNegotiationMode != P_V8_ANS_DISABLED)
					 && (P->At.V8.AnsNegotiationMode != P_V8_ANS_DCE_CONTROLLED))
					{
						fax1RspV8CallMenu (P);
						if (P->At.V8.AnsNegotiationMode != P_V8_ANS_DCE_WITH_INDICATIONS)
						{
							if (P->port_u.Fax1.Phase == FAX1_PHASE_DCD)
							{
								P->port_u.Fax1.Phase = FAX1_PHASE_RECV ;
								fax1CancelTimer (P) ;
								fax1EnqIdle (P) ;
							}
							else
							{
								P->port_u.Fax1.Phase = FAX1_PHASE_XMIT ;
							}
							fax1RspNumber (P, R_OK) ;
							P->State = P_ESCAPE ;	/* need ATO	*/
							fax1InactivityTimer (P) ;
							return;
						}
					}
				}
			}
			P->State = P_DATA ;
		}
	}
#endif /* FAX1_V34FAX */
	switch (P->port_u.Fax1.Phase)
	{
	case FAX1_PHASE_CTS:
		if (Type == CLASS1_INDICATION_CTS_ON)
		{
			P->port_u.Fax1.Phase = FAX1_PHASE_XMIT ;
			fax1RspNumber (P, R_CONNECT_300) ;
			fax1ScheduleTimer (P, FAX1_TIMER_TASK_NO_XMIT,
			                      MILLISECONDS_UNTIL_XMIT);
			fax1InactivityTimer (P) ;
			Match = TRUE;
		}
		break;

	case FAX1_PHASE_XMIT:
		if (Type == CLASS1_INDICATION_CTS_ON)
		{
			Match = TRUE;
		}
		break;

	case FAX1_PHASE_DCD:
		if (Type == CLASS1_INDICATION_DCD_ON)
		{
			/* start receiving, enqueue IDLE reconfiguration already	*/
			/* here to get the FLUSHED*DATA message asap after DCD_OFF.	*/
			P->port_u.Fax1.Phase = FAX1_PHASE_RECV ;
			fax1CancelTimer (P) ;
			fax1EnqIdle (P) ;
			fax1RspNumber (P, R_CONNECT_300) ;
			Match = TRUE;
		}
		break;
	}
	if (!Match)
	{
		DBG_FTL(("[%p:%s] fax1CtsDcdOn: unexpected %s in %s",
		         P->Channel, P->Name, fax1CtrlInd (Type), fax1Phase (P->port_u.Fax1.Phase)))
	}
}

#ifdef FAX1_V34FAX

static void fax1DcdOff (ISDN_PORT *P)
{

	if ( P->At.Fax1.V34Fax )
	{
		P->port_u.Fax1.V34FaxControl = FALSE;
		switch (P->port_u.Fax1.Phase)
		{
		case FAX1_PHASE_CTRL:
			if (!P->port_u.Fax1.V34FaxSource)
			{
				if (P->port_u.Fax1.V34FaxMark)
				{
					(void) fax1V34FaxReconfig (P, CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL |
					                              CLASS1_RECONFIGURE_HDLC_FLAG |
					                              CLASS1_RECONFIGURE_SYNC_FLAG) ;
					P->port_u.Fax1.Phase = FAX1_PHASE_TOPRI;
				}
			}
			break;

		case FAX1_PHASE_TOPRI:
			if (P->port_u.Fax1.V34FaxSource)
			{
				if (P->port_u.Fax1.V34FaxMark)
				{
					(void) fax1V34FaxReconfig (P, CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL |
					                              CLASS1_RECONFIGURE_HDLC_FLAG |
					                              CLASS1_RECONFIGURE_TX_FLAG) ;
				}
			}
			break;

		case FAX1_PHASE_PRI:
			if (!P->port_u.Fax1.V34FaxSource)
			{
				(void) fax1V34FaxReconfig (P, CLASS1_RECONFIGURE_V34_CONTROL_CHANNEL |
				                              CLASS1_RECONFIGURE_HDLC_FLAG |
				                              CLASS1_RECONFIGURE_SYNC_FLAG) ;
				P->port_u.Fax1.Phase = FAX1_PHASE_TOCTRL;
			}
			break;

		case FAX1_PHASE_TODISC:
			fax1V34Cleardown (P);
			break;
		}
		P->port_u.Fax1.V34FaxMark = FALSE;
	}
}

#endif /* FAX1_V34FAX */

static void fax1Kick (ISDN_PORT *P, byte *pData, word sizeData)
{
	byte Type ;

	if ( sizeData < sizeof(byte) )
	{
		DBG_FTL(("[%p:%s] fax1Kick: bad len %d < 1", P->Channel, P->Name, sizeData))
		return ;
	}

	Type = pData[0] ;

	DBG_CONN(("[%p:%s] fax1Kick: Phase=%s Ind=%s",
			  P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase), fax1CtrlInd (Type)))

	switch ( Type )
	{
	case CLASS1_INDICATION_DCD_ON:

		if ( (P->port_u.Fax1.ReconfigPending == 0) && !P->port_u.Fax1.ReconfigRequest )
		{
			fax1CtsDcdOn (P, pData, sizeData) ;
		}
		break ;

	case CLASS1_INDICATION_DCD_OFF:

		/* stay in receive mode until DSP receive queue is emptied,	*/
		/* this is indicated by a FLUSHED*DATA message later		*/
		/* forced by the the IDLE reconfig enqueued on DCD_ON.		*/
#ifdef FAX1_V34FAX
		if ( (P->port_u.Fax1.ReconfigPending == 0) && !P->port_u.Fax1.ReconfigRequest )
		{
			fax1DcdOff (P) ;
		}
#endif /* FAX1_V34FAX */
		break ;

	case CLASS1_INDICATION_CTS_ON:

		if ( (P->port_u.Fax1.ReconfigPending == 0) && !P->port_u.Fax1.ReconfigRequest )
		{
			fax1CtsDcdOn (P, pData, sizeData) ;
		}
		break ;

	case CLASS1_INDICATION_CTS_OFF:

		/* we do not expect CTS OFF during transmission	*/
		/* since it's half duplex.						*/
		break ;

	case CLASS1_INDICATION_SYNC:

		if ( P->port_u.Fax1.SyncPending != 0 )
		{
			if ( --(P->port_u.Fax1.SyncPending) == 0 )
			{
				P->port_u.Fax1.SyncTime = sysTimerTicks () ;
				if ( P->port_u.Fax1.SyncTime == 0 )
					P->port_u.Fax1.SyncTime = 1 ;
				DBG_FTL(("[%p:%s] fax1 sync: %ld",
					P->Channel, P->Name, (long)(P->port_u.Fax1.SyncTime)))
				if ( P->port_u.Fax1.ReconfigRequest )
					fax1SendReconfig (P) ;
#ifdef FAX1_V34FAX
				else if (P->port_u.Fax1.TimerTask == FAX1_TIMER_TASK_CLEARDOWN)
					fax1V34Cleardown (P);
#endif /* FAX1_V34FAX */
			}
		}
		break ;

	case CLASS1_INDICATION_SIGNAL_TONE:

        if ( pData[1] == CLASS1_SIGNAL_FAX_FLAGS )
		{
			if ( (P->port_u.Fax1.ReconfigPending == 0) && !P->port_u.Fax1.ReconfigRequest )
			{
				DBG_CONN(("[%p:%s] fax1Kick: V.21 ch2 detected in %s",
				         P->Channel, P->Name, fax1Phase (P->port_u.Fax1.Phase)))
#ifdef FAX1_V34FAX
				if ( P->At.Fax1.V34Fax )
				{
					P->port_u.Fax1.V34FaxMark = FALSE;
					P->port_u.Fax1.V34FaxControl = FALSE;
					P->At.Fax1.V34Fax = FALSE;
					queueReset (&P->port_u.Fax1.RxQueue) ;
					fax1SendV34FaxSetup (P);
					switch (P->port_u.Fax1.Phase)
					{
					case FAX1_PHASE_TOV8:
					case FAX1_PHASE_V8:
					case FAX1_PHASE_TODISC:
					case FAX1_PHASE_DISC:
						break;

					case FAX1_PHASE_TOCTRL:
					case FAX1_PHASE_CTRL:
					case FAX1_PHASE_TOPRI:
					case FAX1_PHASE_PRI:
						if (P->State == P_DATA)
						{
							portRxEnqueue (P, _DLE_EOT_, 2, 0/*stream mode*/) ;
							if (P->At.f.Verbose)
								faxRsp (P, "+FCERROR") ;
							else
								faxRsp (P, "+F4") ;
							P->State = P_ESCAPE ;	/* need +FRH before next frame	*/
							fax1InactivityTimer (P) ;
						}
						break;
					}
				}
				else
#endif /* FAX1_V34FAX */
				{
					if ( (P->port_u.Fax1.Phase == FAX1_PHASE_DCD)
					  && ((P->port_u.Fax1.Modulation & CLASS1_RECONFIGURE_PROTOCOL_MASK) !=
					      CLASS1_RECONFIGURE_V21_CH2) )
					{
						if ( P->At.Fax1.AdaptiveReceptionControl )
						{
							(void) fax1Reconfig (P, CLASS1_RECONFIGURE_V21_CH2	|
													CLASS1_RECONFIGURE_HDLC_FLAG,
													0) ;
					        faxRsp (P, "+FRH:3");
						}
						else
						{
							fax1Deconfig (P) ;
							if (P->At.f.Verbose)
								faxRsp (P, "+FCERROR") ;
							else
								faxRsp (P, "+F4") ;
							fax1InactivityTimer (P) ;
						}
					}
				}
			}
		}
		break ;

#ifdef FAX1_V34FAX
	case CLASS1_INDICATION_CI_DETECTED:
		break ;
#endif /* FAX1_V34FAX */
	}
}

word fax1Recv (ISDN_PORT *P,
               byte **Data, word sizeData,
               byte *Stream, word sizeStream, word FrameType)
{
	static byte FakeDcdOn[] = { CLASS1_INDICATION_DCD_ON };
	byte *pData, *Name, *Phase, *RspString, Type, Reconfig ;

	pData = *Data ;		/* *Data comes in and is */
	*Data = Stream ;	/* converted to Stream	 */

	if ( P->At.Fax1.State != FAX1_STATE_UP )
	{
		DBG_FTL(("[%p:%s] fax1Recv: bad state", P->Channel, P->Name))
    	return (0); /* forget it */
	}

	switch ( FrameType )
	{
	default:

		DBG_FTL(("[%p:%s] fax1Recv: bad IDI type %02x", P->Channel, P->Name, FrameType))
    	return (0) ;	/* forget it */

	case N_EDATA:	/* modulation phase indications	*/

		fax1Kick (P, pData, sizeData) ;
		return (0) ;	/* handled anyway */

	case N_DATA :	/* normal control or page data	*/

		break ;	/* this is the only case handled here */
	}

	sizeData--;
	Type	= pData[sizeData] ;
	Name	= fax1DataInd (Type) ;
	Phase	= fax1Phase (P->port_u.Fax1.Phase) ;

	if ( P->port_u.Fax1.ReconfigPending != 0 )
	{
		Reconfig = P->port_u.Fax1.ReconfigPending ;
		if ( (Type == CLASS1_INDICATION_FLUSHED_DATA)
		  || (Type == CLASS1_INDICATION_FLUSHED_HDLC_DATA) )
		{
			(P->port_u.Fax1.ReconfigPending)-- ;
		}
		if ( (Reconfig > 1)
	  	  || ((P->port_u.Fax1.Phase != FAX1_PHASE_RECV)
	  	   && (P->port_u.Fax1.Phase != FAX1_PHASE_TERM)) )
		{
			DBG_PRV1(("[%p:%s] fax1Recv: drop %s in %s reconfig %d",
			        P->Channel, P->Name, Name, Phase, Reconfig))
			if ( (P->port_u.Fax1.ReconfigPending == 0)
			  && !P->port_u.Fax1.ReconfigRequest
			  && (P->port_u.Fax1.Phase == FAX1_PHASE_CTS)
			  && !(P->port_u.Fax1.Modulation & CLASS1_RECONFIGURE_HDLC_FLAG) )
			{
				P->port_u.Fax1.Phase = FAX1_PHASE_XMIT ;
				fax1RspNumber (P, R_CONNECT_300) ;
				fax1ScheduleTimer (P, FAX1_TIMER_TASK_NO_XMIT,
				                      MILLISECONDS_UNTIL_XMIT);
				fax1InactivityTimer (P) ;
			}
			return (0) ;	/* forget it */
		}
	}
	if ( P->port_u.Fax1.ReconfigRequest )
	{
		DBG_PRV1(("[%p:%s] fax1Recv: drop %s in %s reconfig req",
		        P->Channel, P->Name, Name, Phase))
		return (0) ;	/* forget it */
	}

#ifdef FAX1_V34FAX
	if (P->At.Fax1.V34Fax)
	{
		if ( (P->port_u.Fax1.Phase != FAX1_PHASE_CTRL)
		  && ((P->port_u.Fax1.Phase != FAX1_PHASE_PRI) || P->port_u.Fax1.V34FaxSource) )
		{
			DBG_PRV1(("[%p:%s] fax1Recv: drop %s in %s", P->Channel, P->Name, Name, Phase))
			return (0) ;	/* forget it */
		}

		switch ( Type )
		{
		case CLASS1_INDICATION_ABORTED_HDLC_DATA:

			DBG_CONN(("[%p:%s] fax1Recv: %s", P->Channel, P->Name, Name))
			return (0) ; /* forget it */

		case CLASS1_INDICATION_HDLC_DATA:

			/* Discard the trailing byte with the count of HDLC preamble flags,	*/
			sizeData--;
			if ( sizeData < 5 )
			{
				DBG_FTL(("[%p:%s] fax1Recv: %s len %d < 5", P->Channel, P->Name, Name, sizeData))
				return (0) ; /* forget it */
			}
			DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
			if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
			{
				DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'%s'", P->Channel, P->Name, sizeData, Name))
				fax1EscapeEnqueue (P, pData, sizeData, Stream, sizeStream, _DLE_ETX_, Type) ;
				return (0) ;
			}
			DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>", P->Channel, P->Name, sizeData))
			return (fax1AddEscape (P, pData, sizeData, Stream, sizeStream, _DLE_ETX_)) ;

		case CLASS1_INDICATION_WRONG_CRC_HDLC_DATA:

			/* Discard the trailing byte with the count of HDLC preamble flags,	*/
			sizeData--;
			DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
			if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
			{
				DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><FERR>'%s'", P->Channel, P->Name, sizeData, Name))
				fax1EscapeEnqueue (P, pData, sizeData, Stream, sizeStream, _DLE_FERR_, Type) ;
				return (0) ;
			}
			DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><FERR>", P->Channel, P->Name, sizeData))
			return (fax1AddEscape (P, pData, sizeData, Stream, sizeStream, _DLE_FERR_)) ;

		case CLASS1_INDICATION_FLUSHED_HDLC_DATA:

			DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
			return (0) ; /* forget it */
		}
		DBG_FTL(("[%p:%s] fax1Recv: %s", P->Channel, P->Name, Name))
		return (0) ;	/* forget it */
	}
#endif /* FAX1_V34FAX */

	if ( (P->port_u.Fax1.Phase != FAX1_PHASE_DCD)
	  && (P->port_u.Fax1.Phase != FAX1_PHASE_RECV) )
	{
		DBG_PRV1(("[%p:%s] fax1Recv: drop %s in %s", P->Channel, P->Name, Name, Phase))
		if ( (P->port_u.Fax1.Phase == FAX1_PHASE_TERM)
		  && ((Type == CLASS1_INDICATION_FLUSHED_DATA)
		   || (Type == CLASS1_INDICATION_FLUSHED_HDLC_DATA)) )
		{
			fax1Deconfig (P) ;
			fax1RspNumber (P, R_OK) ;
			fax1InactivityTimer (P) ;
		}
		return (0) ;	/* forget it */
	}

	switch ( Type )
	{
	default:

		DBG_FTL(("[%p:%s] fax1Recv: %s", P->Channel, P->Name, Name))
		return (0) ;	/* forget it */

	case CLASS1_INDICATION_DATA:

		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
			fax1CtsDcdOn (P, FakeDcdOn, sizeof(FakeDcdOn)) ;
		DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
		return (fax1AddEscape (P, pData, sizeData, Stream, sizeStream, NULL)) ;

	case CLASS1_INDICATION_FLUSHED_DATA:

		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
		{
			DBG_PRV1(("[%p:%s] fax1Recv: unexpected %s in %s", P->Channel, P->Name, Name, Phase))
			return (0) ;	/* forget it */
		}
		DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
		fax1Deconfig (P) ;
		RspString = (P->At.f.Verbose)? _DLE_ETX_"\r\nNO CARRIER\r\n" : _DLE_ETX_"3\r" ;
		DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'NO_CARRIER'", P->Channel, P->Name, sizeData))
		fax1InactivityTimer (P) ;
		return (fax1AddEscape (P, pData, sizeData, Stream, sizeStream, RspString)) ;

	case CLASS1_INDICATION_ABORTED_HDLC_DATA:

		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
			fax1CtsDcdOn (P, FakeDcdOn, sizeof(FakeDcdOn)) ;
		DBG_CONN(("[%p:%s] fax1Recv: %s", P->Channel, P->Name, Name))
		return (0) ; /* forget it */

	case CLASS1_INDICATION_HDLC_DATA:

		/* Discard the trailing byte with the count of HDLC preamble flags,	*/
		sizeData--;
		if ( sizeData < 5 )
		{
			DBG_FTL(("[%p:%s] fax1Recv: %s len %d < 5", P->Channel, P->Name, Name, sizeData))
			return (0) ; /* forget it */
		}
		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
			fax1CtsDcdOn (P, FakeDcdOn, sizeof(FakeDcdOn)) ;
		DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
		if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
		{
			DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'%s'", P->Channel, P->Name, sizeData, Name))
			fax1EscapeEnqueue (P, pData, sizeData, Stream, sizeStream, _DLE_ETX_, Type) ;
			return (0) ;
		}
		DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'OK'", P->Channel, P->Name, sizeData))
		RspString = (P->At.f.Verbose)? _DLE_ETX_"\r\nOK\r\n" : _DLE_ETX_"0\r" ;
		break ;

	case CLASS1_INDICATION_WRONG_CRC_HDLC_DATA:

		/* Discard the trailing byte with the count of HDLC preamble flags,	*/
		sizeData--;
		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
			fax1CtsDcdOn (P, FakeDcdOn, sizeof(FakeDcdOn)) ;
		DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
		if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
		{
			DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'%s'", P->Channel, P->Name, sizeData, Name))
			fax1EscapeEnqueue (P, pData, sizeData, Stream, sizeStream, _DLE_ETX_, Type) ;
			return (0) ;
		}
		DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'ERROR'", P->Channel, P->Name, sizeData))
		RspString = (P->At.f.Verbose)? _DLE_ETX_"\r\nERROR\r\n" : _DLE_ETX_"4\r" ;
		break ;

	case CLASS1_INDICATION_FLUSHED_HDLC_DATA:

		if ( P->port_u.Fax1.Phase == FAX1_PHASE_DCD )
		{
			DBG_PRV1(("[%p:%s] fax1Recv: unexpected %s in %s", P->Channel, P->Name, Name, Phase))
			return (0) ;	/* forget it */
		}
		DBG_PRV1(("[%p:%s] fax1Recv: %s len %d", P->Channel, P->Name, Name, sizeData))
		if ( queueCount(&P->port_u.Fax1.RxQueue) || (P->State == P_ESCAPE) )
		{
			DBG_CONN(("[%p:%s] fax1Recv: <%d>'%s'", P->Channel, P->Name, 0, Name))
			fax1EscapeEnqueue (P, pData, 0, Stream, sizeStream, _DLE_ETX_, Type) ;
			return (0) ;
		}
		fax1Deconfig (P) ;
		DBG_CONN(("[%p:%s] fax1Recv: <%d><DLE><ETX>'ERROR'", P->Channel, P->Name, 0))
		RspString = (P->At.f.Verbose)? _DLE_ETX_"\r\nERROR\r\n" : _DLE_ETX_"4\r" ;
		fax1InactivityTimer (P) ;
		return (fax1AddEscape (P, pData, 0, Stream, sizeStream, RspString)) ;
	}

	/* Here we have an HDLC frame ready to be passed upstream.			*/
	/* insert an additional DLE before each native DLE as for any other	*/
	/* received data and append DLE-ETX and finally the response string	*/

	sizeData = fax1AddEscape (P, pData, sizeData, Stream, sizeStream, RspString) ;
	if ( !sizeData )
		return (0) ; /* buffer overrun, forget it */

	/* no need here to ask for the FINAL frame because HDLC receiver is	*/
	/* not stopped until the sender drops carrier and thus it makes no	*/
	/* sense to request reconfiguration or so.							*/

	P->State = P_ESCAPE ;		/* need +FRH before passing next frame	*/
	fax1InactivityTimer (P) ;
	return (sizeData) ;			/* pass this frame to the application	*/
}

