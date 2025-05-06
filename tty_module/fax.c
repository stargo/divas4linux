
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
#include "platform.h"
#include "di_defs.h"
#endif

#include "port.h"
#include "fax_if.h"
#include "atp_if.h"
#if defined(UNIX)
#include "fax.h"
#endif
#include "t30.h"
#include "debuglib.h"

extern ISDN_PORT_DRIVER PortDriver;
extern int sprintf (char*, const char*, ...);

static struct
{
	byte                    Val;
	char *                  Text;
} ClassTable[] =
{
	{ P_CLASS_DATA,  "0"   },
	{ P_CLASS_FAX1,  "1"   },
	{ P_CLASS_FAX10, "1.0" },
	{ P_CLASS_FAX2,  "2"   },
//	{ P_CLASS_FAX20, "2.0" },
//	{ P_CLASS_FAX21, "2.1" },
//	{ P_CLASS_VOICE, "8"   }
};

void faxRsp (ISDN_PORT *P, byte *Text)
{	/* wrap plain text responses in CR-LF sequences */
    byte Response[80];

    sprintf (Response, "\r\n%s\r\n", Text) ;
    DBG_TRC(("[%p:%s] faxRsp(%d)", P->Channel, P->Name, str_len(Response)))
    DBG_BLK((Response, str_len(Response)))
    atRspStr (P, Response);
}

int genCmdMI (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* query modem manufacturer */
#if defined(UNIX)
    faxRsp (P, "Eicon Technology Corp.");
#else
    extern char company[];	/* from date.c */
    faxRsp (P, company);
#endif
    return R_OK ;
}

int genCmdMM (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* query modem model */
#if defined(UNIX)
    faxRsp (P, "DIVA Unix ISDN Modem");
#else
    extern char primary[];	/* from date.c */
    faxRsp (P, primary);
#endif
	return R_OK ;
}

int genCmdMR (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* query modem revision */
#if defined(UNIX)
    faxRsp (P, "Version 1.0");
#else
    extern char build[];	/* from date.c */
    faxRsp (P, build);
#endif
	return R_OK ;
}

/****************************************************************************\
* faxCmdCLASS                                                                *
*                                                                            *
* +FCLASS command                                                            *
\****************************************************************************/
static int faxCmdCLASS (ISDN_PORT *P, int Req, int Val, byte **Str)
{/* +FCLASS command */
	char                    Class[32];
	byte			Mode;
	char *                  p;
	int                     i;

	switch ( Req )
	{
	case FAX_REQ_VAL:	/* +FCLASS?  - query current class		*/
        i = sizeof(ClassTable) / sizeof(ClassTable[0]);
        do
        {
            i--;
        } while ((i != 0) && (ClassTable[i].Val != P->Class));

        faxRsp (P, ClassTable[i].Text) ;
		break ;

	case FAX_REQ_RNG:	/* +FCLASS=? - query supported classes	*/
	    p = Class;
        for (i = 0; i < sizeof(ClassTable) / sizeof(ClassTable[0]); i++)
        {
		    if (P->At.ClassSupport & (1 << ClassTable[i].Val))
            {
                if (p != Class)
                    *(p++) = ',';
                p += sprintf (p, "%s", ClassTable[i].Text);
            }
        }
        if (p == Class)
			return R_ERROR ;
        *p = '\0';
        faxRsp (P, Class) ;
		break ;

	case FAX_REQ_SET:	/* +FCLASS=x - set class			 	*/
        i = 0;
        p = *Str;
        while ( (i < sizeof(Class) - 1) && *p && (*p != ';') )
        {
            Class[i++] = *(p++) ;
        }
        Class[i] = '\0';
        i = 0;
		while ((i < sizeof(ClassTable) / sizeof(ClassTable[0]))
		    && (str_cmp (ClassTable[i].Text, Class) != 0))
        {
            i++;
        }
		if (i == sizeof(ClassTable) / sizeof(ClassTable[0]))
			return R_ERROR ;
		*Str = p ;

		if( !(P->At.ClassSupport & (1 << ClassTable[i].Val)) )
			return R_ERROR ;

		Mode = P_MODE_MODEM;
		P->Class = ClassTable[i].Val;
		switch ( P->Class )
		{
			case P_CLASS_DATA:  break ;
			case P_CLASS_FAX1:
			case P_CLASS_FAX10: Mode = P_MODE_FAX; fax1SetClass (P) ; break ;
			case P_CLASS_FAX2:  Mode = P_MODE_FAX; fax2SetClass (P) ; break ;
		}
		P->Mode = P->At.f.Mode = Mode ;
		if (P->Mode == P_MODE_FAX)
		{
			P->FlowOpts = ((P->At.f.FlowOpts == AT_FLOWOPTS_BOTH) ||
			               (P->At.f.FlowOpts == AT_FLOWOPTS_RTSCTS)) ?
			                AT_FLOWOPTS_BOTH : AT_FLOWOPTS_XONXOFF;
		}
		else
		{
			P->FlowOpts = P->At.f.FlowOpts;
		}

		break ;
	}

	return R_OK ;
}

int faxCmdIT (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	byte Buf[32];
	byte *p;
	byte t, a;
	int i;

	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FIT? query inactivity timeout value    */
		sprintf (Buf, "%d,%d",
		              P->At.InactivityTimeout,
		              P->At.InactivityAction);
        faxRsp (P, Buf);
		break;

	case FAX_REQ_RNG:	/* +FIT=? query inactivity timeout range   */
        faxRsp (P, "(0-255),(0,1)");
        break;

	case FAX_REQ_SET:	/* +FIT=t,a set inactivity timeout         */
		p = *Str;
		t = 0;
		a = 0;
		i = atNumArg (&p);
		if ((i < 0) || (i > 255) || (*p != ','))
			return (R_ERROR);
		t = (byte) i;
		p++;
		i = atNumArg (&p);
		if ((i < 0) || (i > 1))
			return (R_ERROR);
		a = (byte) i;
		*Str = p;
        P->At.InactivityTimeout = t;
        P->At.InactivityAction = a;
		break;
	}
	return R_OK ;
}

int faxCmdLO (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	byte Buf[16];

	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FLO? query current flow control option */
		switch (P->At.f.FlowOpts)
		{
		case AT_FLOWOPTS_RTSCTS:
			P->At.FlowControlOption = P_FLOW_CONTROL_OPTION_CTS_RTS;
			break;
		case AT_FLOWOPTS_XONXOFF:
		case AT_FLOWOPTS_TRANSP:
		case AT_FLOWOPTS_BOTH:
			P->At.FlowControlOption = P_FLOW_CONTROL_OPTION_XON_XOFF;
			break;
		default:
			P->At.FlowControlOption = P_FLOW_CONTROL_OPTION_NONE;
		}
		sprintf (Buf, "%d", P->At.FlowControlOption);
		faxRsp (P, Buf);
		break;

	case FAX_REQ_RNG:	/* +FLO=? query flow control support       */
		faxRsp (P, "(0,1,2)");
		break;

	case FAX_REQ_SET:	/* +FLO=n set flow control option          */
		P->At.FlowControlOption = (byte) Val;
		switch (P->At.FlowControlOption)
		{
		case P_FLOW_CONTROL_OPTION_CTS_RTS:
			P->At.f.FlowOpts = AT_FLOWOPTS_RTSCTS;
			break;
		case P_FLOW_CONTROL_OPTION_XON_XOFF:
			P->At.f.FlowOpts = AT_FLOWOPTS_XONXOFF;
			break;
		default:
			P->At.f.FlowOpts = AT_FLOWOPTS_OFF;
		}
		break;
	}
	return R_OK ;
}

int faxCmdPR (ISDN_PORT *P, int Req, int Val, byte **Str)
{
	byte Buf[16];

	switch (Req)
	{
	case FAX_REQ_VAL:	/* +FPR? query serial port rate            */
		sprintf (Buf, "%d", P->At.SerialPortRate);
        faxRsp (P, Buf);
		break;

	case FAX_REQ_RNG:	/* +FPR=? query serial port rate support   */
        faxRsp (P, "(0,1,2,4,8,10,18)");
        break;

	case FAX_REQ_SET:	/* +FPR=n set serial port rate             */
        P->At.SerialPortRate = (byte) Val;
		break;
	}
	return R_OK ;
}

/* Command description flag bits */

# define PAR		0x80	/* parameter command						*/
# define ACT		0x40	/* action command							*/
# define OPERATIONS	0x1c	/* operations permitted for this command	*/
# define SET		0x10	/* set new value							*/
# define VAL		0x08	/* query current value						*/
# define RNG		0x04	/* query all possible values				*/
# define PROPERTIES	0x03	/* basic command properties					*/
# define NUM		0x02	/* has numeric arg (possibly empty)			*/
# define EOL		0x01	/* cmd must be last command of line			*/
# define CL0		(1 << P_CLASS_DATA)		/* class 0 (data modem)		*/
# define CL1		(1 << P_CLASS_FAX1)		/* class 1 command			*/
# define CL2		(1 << P_CLASS_FAX2)		/* class 2 command			*/
# define CL10		(1 << P_CLASS_FAX10)	/* class 1.0 command		*/
# define CL20		(1 << P_CLASS_FAX20)	/* class 2.0 command		*/
# define CL21		(1 << P_CLASS_FAX21)	/* class 2.1 command		*/
# define CL1X		(CL1 + CL10)
# define CL2X		(CL2 + CL20 + CL21)
# define CLXX		(CL0 + CL1X + CL2X)

typedef struct
{
	char *                  Name;
	FAX_CMD                 Handler;
	byte                    Class;
	byte                    Flags;
	byte                    Low;
	byte                    High;
} CMD_TABLE ;

static CMD_TABLE Commands[] =
{
	{ "class", faxCmdCLASS, CLXX, PAR + SET + VAL + RNG             ,  0,   2 },
	{ "ts",    fax1CmdTS  , CL1X, ACT + SET + VAL + RNG + NUM + EOL ,  0, 255 },
	{ "rs",    fax1CmdRS  , CL1X, ACT + SET + VAL + RNG + NUM + EOL ,  0, 255 },
	{ "th",    fax1CmdTH  , CL1X, ACT + SET + VAL + RNG + NUM + EOL ,  3, 146 },
	{ "rh",    fax1CmdRH  , CL1X, ACT + SET + VAL + RNG + NUM + EOL ,  3, 146 },
	{ "tm",    fax1CmdTM  , CL1X, ACT + SET + VAL + RNG + NUM + EOL , 24, 146 },
	{ "rm",    fax1CmdRM  , CL1X, ACT + SET + VAL + RNG + NUM + EOL , 24, 146 },
	{ "34",    fax1Cmd34  , CL10, PAR + SET + VAL + RNG             ,  0,   0 },
	{ "ar",    fax1CmdAR  , CL10, PAR + SET + VAL + RNG + NUM       ,  0,   1 },
	{ "cl",    fax1CmdCL  , CL10, PAR + SET + VAL + RNG + NUM       ,  0, 255 },
	{ "dd",    fax1CmdDD  , CL10, PAR + SET + VAL + RNG + NUM       ,  0,   1 },
	{ "it",    faxCmdIT   , CLXX, PAR + SET + VAL + RNG             ,  0,   0 },
	{ "mi",    genCmdMI   , CLXX, PAR       + VAL                   ,  0,   0 },
	{ "mm",    genCmdMM   , CLXX, PAR       + VAL                   ,  0,   0 },
	{ "mr",    genCmdMR   , CLXX, PAR       + VAL                   ,  0,   0 },
	{ "lo",    faxCmdLO   , CLXX, PAR + SET + VAL + RNG + NUM       ,  0,   2 },
	{ "pr",    faxCmdPR   , CLXX, PAR + SET + VAL + RNG + NUM       ,  0, 255 },
	{ "axerr", 0          , CL2 , PAR                               ,  0,   0 },
	{ "bor",   fax2CmdBOR , CL2 , PAR + SET + VAL       + NUM       ,  0,   1 },
	{ "lpl",   fax2CmdLPL , CL2 , PAR + SET + VAL       + NUM       ,  0,   1 },
	{ "buf",   0          , CL2 , PAR                               ,  0,   0 },
	{ "bug",   0          , CL2 , PAR                               ,  0,   0 },
	{ "cr",    fax2CmdCR  , CL2 , PAR + SET + VAL + RNG + NUM       ,  0,   1 },
	{ "dcc",   fax2CmdDCC , CL2 , PAR + SET + VAL + RNG             ,  0,   0 },
	{ "dcs",   0          , CL2 , PAR                               ,  0,   0 },
	{ "dis",   fax2CmdDIS , CL2 , PAR + SET + VAL + RNG             ,  0,   0 },
	{ "dr",    fax2CmdDR  , CL2 , ACT                         + EOL ,  0,   0 },
	{ "dt",    fax2CmdDT  , CL2 , ACT                         + EOL ,  0,   0 },
	{ "et",    fax2CmdET  , CL2 , ACT + SET             + NUM       ,  0,   9 },
	{ "faa",   0          , CL2 , PAR                               ,  0,   0 },
/* Added in aa - should 'faa' be there - maybe not */
	{ "aa",    0          , CL2 , PAR                               ,  0,   0 },
	{ "k",     fax2CmdK   , CL2 , ACT                               ,  0,   0 },
	{ "lid",   fax2CmdLID , CL2 , PAR + SET                         ,  0,   0 },
	{ "mfr",   genCmdMI   , CL2 , PAR       + VAL                   ,  0,   0 },
	{ "mdl",   genCmdMM   , CL2 , PAR       + VAL                   ,  0,   0 },
	{ "nsf",   0          , CL2 , PAR                               ,  0,   0 },
	{ "phcto", 0          , CL2 , PAR                               ,  0,   0 },
	{ "pts",   0          , CL2 , PAR                               ,  0,   0 },
	{ "rev",   genCmdMR   , CL2 , PAR       + VAL                   ,  0,   0 },
	{ "cq",    0          , CL2 , PAR                               ,  0,   0 },
	{ "td",    fax2CmdTD  , CL2 , PAR + SET                         ,  0,   0 },
	{ "ph",    fax2CmdPH  , CL2 , PAR + SET                         ,  0,   0 },
/* Added in for VSI fax stuff - stops ERROR being received */
	{ "rel",   0          , CL2 , ACT                               ,  0,   0 }
} ;

int faxPlusF (ISDN_PORT *P, byte **Arg)
{/* generic fax command handler */
	CMD_TABLE *             Cmd;
	byte *                  p;
	int			            Val;
	int                     Req;
	int                     Rc;
	int                     i;

	Rc = R_OK ;

	p = *Arg ;

	for ( Cmd = &Commands[0] ; ; Cmd++ )
	{
		if ( Cmd >= &Commands[sizeof(Commands)/sizeof(Commands[0])] )
		{
            DBG_FTL(("[%p:%s] faxCmd: unknown", P->Channel, P->Name));
            return R_ERROR ;
		}
		i = str_len ((char *)(Cmd->Name)) ;
		if ( mem_i_cmp (Cmd->Name, p, i) == 0 )
		{
			p += i ;	/* skip command */
			break;
		}
	}

	if ( !Cmd->Handler )
		goto skip;

	if ( Cmd->Flags & ACT )
	{
		if ( !(Cmd->Class & (1 << P->Class)) )
		{
            DBG_FTL(("[%p:%s] faxCmd: wrong class %d", P->Channel, P->Name, P->Class));
            return R_ERROR ;
		}
	}
	else if ( Cmd->Flags & PAR )
	{
		if ( !(Cmd->Class & P->At.ClassSupport) )
		{
            DBG_FTL(("[%p:%s] faxCmd: unsupported", P->Channel, P->Name));
		}
	}

	Val = -1 ;
	if ( !(Cmd->Flags & OPERATIONS) )
	{
		Req = FAX_REQ_NUL ;	/* self contained command */
		if ( *p && ((*p != ';') || (Cmd->Flags & EOL)) )
			goto illegal ;
	}
	else switch ( *p++ )
	{
	case '?':	/* query current value */
		Req = FAX_REQ_VAL ;
		if ( !(Cmd->Flags & VAL) || (*p && (*p != ';')) )
			goto illegal ;
		break ;

	case '=':	/* query range or set value */
		if ( *p == '?' )
		{
			p++;
			Req = FAX_REQ_RNG ;	/* query range */
			if ( !(Cmd->Flags & RNG) || (*p && (*p != ';')) )
				goto illegal ;
		}
		else
		{
			Req = FAX_REQ_SET ;	/* set value */
			if ( !(Cmd->Flags & SET) )
				goto illegal ;
			if ( Cmd->Flags & NUM )
			{/* pure numeric argument */
				Val = atNumArg (&p) ;
				if ( (*p && ((*p != ';') || (Cmd->Flags & EOL)))
				  || ((unsigned)Val < Cmd->Low)
				  || ((unsigned)Val > Cmd->High) )
				{
					goto illegal ;
				}
			}
		}
		break ;

	default :	/* neither '=' nor '?', surely an error */
		goto illegal ;
	}

	switch ( Rc = Cmd->Handler (P, Req, Val, &p) )
	{
	default		:
		return (Rc) ;

	case R_OK	:
	case R_VOID	:
   		if ( *p && (*p != ';') )
			DBG_FTL(("[%p:%s] faxCmd: ambiguous", P->Channel, P->Name));
		break ;
	}

	/* advance to next command and send an OK response */
skip:
   	while ( *p && (*p != ';') ) p++ ;
    while ( *p == ';' ) p++ ;

	*Arg = p ;
	return Rc ;

illegal:
	DBG_FTL(("[%p:%s] faxCmd: illegal", P->Channel, P->Name));
	return R_ERROR ;
}

void faxSetClassSupport (ISDN_PORT *P)
{
	unsigned short          uCapabilities;
	byte                    bClass;
	byte                    b;
	byte                    bSupported;
	unsigned long           u;

	/* quick and dirty hack to get the available fax support:				*/
	/* a card with fax and modem capabilities is assumed to have the faster	*/
	/* DSP which is capable to run the TELINDUS code.						*/

	/*--- get adapter capabilities -------------------------------------------*/
	uCapabilities = PortDriver.Isdn->Caps();

	/*--- set modulation support ---------------------------------------------*/
	switch(uCapabilities & (ISDN_CAP_FAX3 + ISDN_CAP_MODEM))
	{
	case (ISDN_CAP_FAX3) :
		P->At.FaxTxModulationSupport = CLASS1_SUPPORTED_PROTOCOLS_TX_DIVA ;
		P->At.FaxRxModulationSupport = CLASS1_SUPPORTED_PROTOCOLS_RX_DIVA ;
		break ;
	case (ISDN_CAP_FAX3 + ISDN_CAP_MODEM) :
		P->At.FaxTxModulationSupport = CLASS1_SUPPORTED_PROTOCOLS_TX_DIVAPRO ;
		P->At.FaxRxModulationSupport = CLASS1_SUPPORTED_PROTOCOLS_RX_DIVAPRO ;
		break ;
	default:
		P->At.FaxTxModulationSupport = 0 ;
		P->At.FaxRxModulationSupport = 0 ;
		break ;
	}

	/*--- set supported classes ----------------------------------------------*/
	P->At.ClassSupport = 0;
	for( u=0; u < sizeof(ClassTable)/sizeof(ClassTable[0]); u++ )
	{
		bClass = 1 << ClassTable[u].Val;

		if(    ( bClass & (CL1X | CL2X) )
		   && !( uCapabilities & ISDN_CAP_FAX3 ) )
		{	continue;
		}

		if( bClass & (CL0 + CL1X + CL2X) )
			P->At.ClassSupport |= bClass;
	}
	P->At.ClassSupport |= CL0;   // Class Data Modem is always supported

	// Since fax application expect that they can switch between Class Data
	// and Class Fax we have always offer the Class Data, also if only fax
	// is allowed.

	/*--- set current class --------------------------------------------------*/
	bClass = 1 << P->Class;

	if( !( P->At.ClassSupport & bClass ) )
	{
		if(  bClass & (CL1X | CL2X) ) // if cur class == fax class
			bSupported = (byte)(P->At.ClassSupport & (CL1X | CL2X));
		else
			bSupported = (byte)(P->At.ClassSupport & CL0);

		if( !bSupported )
		{	if ( P->At.ClassSupport & CL0 )
				bSupported = CL0;
			else
				bSupported = P->At.ClassSupport;
		}
		for( bClass=1; bClass; bClass <<= 1 )
		{	if( bClass & bSupported )
				break;
		}

		if ( !bClass )
			bClass = CL0;
	}

	for( P->Class=0, b=1; b && !(bClass & b); P->Class++, b<<=1 )
		;
	if( !b )
		P->Class = P_CLASS_DATA;
}

void faxInit (ISDN_PORT *P)
{
	/* Called by atInit() after all common AT processor specific data was	*/
	/* (re)initialized according to a given 'Profile'.						*/
	/* Here we (re)set the FAX class specific AT data to its initial state.	*/
	/* P->Mode  == according to given 'Profile'								*/
	/* P->State == P_COMMAND												*/

	faxSetClassSupport(P) ;

	fax1Init(P) ;
	fax2Init(P) ;
}

void faxFinit (ISDN_PORT *P)
{
	/* Called by atFinit() when the port is closed and thus the AT command 	*/
	/* processor is terminated. It should stop all communication with the	*/
	/* port.																*/

	fax1Finit(P);
	fax2Finit(P);
}

T30_INFO *faxConnect (ISDN_PORT *P)
{
	/* Called by atDial() or atAnswer() to get the FAX parameters (if any).	*/
	/* If the connection succeeds faxUp() will be called next but there is	*/
	/* no callback for a failing connection.								*/

	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10: return (fax1Connect(P)) ;
	case P_CLASS_FAX2:  return (fax2Connect(P)) ;
	}
	return ( (T30_INFO *)0 ) ;
}

int faxUp (ISDN_PORT *P)
{
	/* Called by PortUp(), i.e an inbound or outbound call was successfully	*/
	/* established and the B-channel is ready.								*/
	/* P->Mode  == P_MODE_FAX												*/
	/* P->State == P_CONNECTED												*/

	P->State = P_ESCAPE ;

	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10: fax1Up (P) ; break ;
	case P_CLASS_FAX2:  fax2Up (P) ; break ;
	}
	return 0 ; /* don't send the generic CONNECT nnnn response */
}

int faxDelay (ISDN_PORT *P, byte *Response, word sizeResponse, dword *RspDelay)
{
	/* Called by atRspQueue() just before a Response is entered to the pen-	*/
	/* ding response queue or to the receive queue.							*/
	/* Here we have the chance to adjust the delay to be used used for this	*/
	/* response by changing the value passed indirectly via '*RspDelay'.	*/
	/* The function return value defines the kind of delay handling:		*/
	/*  == 0  -	default response delay handling								*/
	/*  != 0  -	all pending responses are flushed und the current response	*/
	/*			is delayed by the value written to '*RspDelay'.				*/

	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10: return (fax1Delay(P, Response, sizeResponse, RspDelay)) ;
	case P_CLASS_FAX2:  return (fax2Delay(P, Response, sizeResponse, RspDelay)) ;
	}
	return (0) ;
}

int faxHangup (ISDN_PORT *P)
{
	/* Called by atDtrOff() or PortDown() if a successful connection which	*/
	/* was indicated via fax1Up() before is disconnected again.				*/

	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10: return (fax1Hangup(P)) ;
	case P_CLASS_FAX2:  return (fax2Hangup(P)) ;
	}
	return (1) ;	/* generate NO CARRIER response */
}

word faxWrite (struct ISDN_PORT *P,
               byte **Data, word sizeData,
               byte *Frame, word sizeFrame, word *FrameType)
{
	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10:
		return (fax1Write(P, Data, sizeData, Frame, sizeFrame, FrameType)) ;
	case P_CLASS_FAX2:
		return (fax2Write(P, Data, sizeData, Frame, sizeFrame, FrameType)) ;
	}
	return (0) ;
}

void faxTxEmpty (struct ISDN_PORT *P)
{
	/* Called everytime when the port driver transmit queue goes empty.	*/
	/* This is mainly for CLASS2 to give em a chance to send OK on EOP.	*/

	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10: fax1TxEmpty(P) ; break ;
	case P_CLASS_FAX2:  fax2TxEmpty(P) ; break ;
	}
}

word faxRecv (struct ISDN_PORT *P,
              byte **Data, word sizeData,
              byte *Stream, word sizeStream, word FrameType)
{
	switch ( P->Class )
	{
	case P_CLASS_FAX1:
	case P_CLASS_FAX10:
		return (fax1Recv(P, Data, sizeData, Stream, sizeStream, FrameType)) ;
	case P_CLASS_FAX2:
		return (fax2Recv(P, Data, sizeData, Stream, sizeStream, FrameType)) ;
	}
	return (0) ;
}
