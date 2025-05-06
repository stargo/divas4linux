
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

# ifndef SERIAL___H
# define SERIAL___H

/*
 * serial port driver interface definitions
 */

# include "divareq.h"

/* query current state of card and modem */

void  serState (ISDN_PORT *P, byte *Function);

/* the serial port intercept functions */

int   serOpen (ISDN_PORT *P, byte *State);
void  serInit (ISDN_PORT *P);
void  serClose (ISDN_PORT *P);
void  serIsdnUp (ISDN_PORT *P);
void  serIsdnBusy (ISDN_PORT *P);
void  serIsdnDown (ISDN_PORT *P);
int   _cdecl serWrite (ISDN_PORT *P,
		       byte *Data, dword sizeData, dword *sizeWritten);
int   _cdecl serRead (ISDN_PORT *P,
		      byte *Buffer, dword Wanted, dword *pGiven);
int   _cdecl serDeferred (ISDN_PORT *P, SERIAL_STATE *State);

/* the real serial port functions */

int  _cdecl ser_Open (ISDN_PORT *P);
int  _cdecl ser_Interrupt (void *Context);
int  _cdecl ser_Close (ISDN_PORT *P);

int  _cdecl ser_SetCommState (ISDN_PORT *P, _DCB *pDCB, dword ActionMask);
int  _cdecl ser_GetQueueStatus (ISDN_PORT *P, _COMSTAT *pCOMSTAT);
int  _cdecl ser_ClearError (ISDN_PORT *P, _COMSTAT *pCOMSTAT, dword *pError);
int  _cdecl ser_EscapeFunction (ISDN_PORT *P,
			        long lFunc, long InData, dword *OutData);
int  _cdecl ser_Purge (ISDN_PORT *P, dword RecQueueFlag);
int  _cdecl ser_SetEventMask (ISDN_PORT *P, dword dwMask, dword *pEvents);
int  _cdecl ser_GetEventMask (ISDN_PORT *P, dword dwMask, dword *pEvents);
int  _cdecl ser_Write (ISDN_PORT *P,
		      byte *Data, dword sizeData, dword *sizeWritten);
int  _cdecl ser_TransmitChar (ISDN_PORT *P, byte Char);
int  _cdecl ser_Read (ISDN_PORT *P, byte *Buffer, dword Wanted, dword *pGiven);
int  _cdecl ser_EnableNotification (
			ISDN_PORT *P,
			void (*EvNotifyProc)(ISDN_PORT *, long, long, long),
			long ReferenceData);
int  _cdecl ser_SetReadCallBack (
			ISDN_PORT *P,
			dword RxTrigger,
			void (*RxNotifyProc)(ISDN_PORT *, long, long, long),
			dword ReferenceData);
int  _cdecl ser_SetWriteCallBack(
			ISDN_PORT *P,
			dword TxTrigger,
			void (*TxNotifyProc)(ISDN_PORT *, dword, long, long),
			dword ReferenceData);
int  _cdecl ser_GetModemStatus (ISDN_PORT *P, dword *pModemStatus);

/*
 * The following structs are needed by the Win32 specific port functions
 * PortGetCommConfig() and PortSetCommConfig().
 * They are defined in vcommw32.inc but not in vcomm.h and there is no
 * vcommw32.h file in the Win95DDK inc32 directory.
 * Official definitions for these structures are in winbase.h (Win32SDK)
 * but it's hard to include winbase.h in the Win95DDK enviroment.
 */

typedef struct W32DCB {
	dword DCBlength;	/* sizeof(DCB)				*/
	dword BaudRate;		/* Baudrate at which running		*/
# if 1	/* sorry, the compiler did not understand these bitfields	*/
	dword BitMask;		/* Flags, see below			*/
# else
	dword fBinary: 1;		/* Binary Mode (skip EOF check)	*/
	dword fParity: 1;		/* Enable parity checking	*/
	dword fOutxCtsFlow:1;		/* CTS handshaking on output	*/
	dword fOutxDsrFlow:1;		/* DSR handshaking on output	*/
	dword fDtrControl:2;		/* DTR Flow control		*/
	dword fDsrSensitivity:1;	/* DSR Sensitivity		*/
	dword fTXContinueOnXoff: 1;	/* Continue TX when Xoff sent	*/
	dword fOutX: 1;			/* Enable output X-ON/X-OFF	*/
	dword fInX: 1;			/* Enable input X-ON/X-OFF	*/
	dword fErrorChar: 1;		/* Enable Err Replacement	*/
	dword fNull: 1;			/* Enable Null stripping	*/
	dword fRtsControl:2;		/* Rts Flow control		*/
	dword fAbortOnError:1;		/* Abort reads/writes on Error	*/
	dword fDummy2:17;		/* Reserved			*/
# endif /* 1 */
	word wReserved;			/* Not currently used		*/
	word XonLim;			/* Transmit X-ON threshold	*/
	word XoffLim;			/* Transmit X-OFF threshold	*/
	byte ByteSize;			/* Number of bits/byte, 4-8	*/
	byte Parity;			/* 0-4=None,Odd,Even,Mark,Space	*/
	byte StopBits;			/* 0,1,2 = 1, 1.5, 2		*/
	char XonChar;			/* Tx and Rx X-ON character	*/
	char XoffChar;			/* Tx and Rx X-OFF character	*/
	char ErrorChar;			/* Error replacement char	*/
	char EofChar;			/* End of Input character	*/
	char EvtChar;			/* Received Event character	*/
	word wReserved1;		/* Fill for now (sizeof == 28)	*/
} W32DCB;

typedef struct W32COMMCONFIG {
	dword	dwSize;			/* Size of the entire struct	*/
	word	wVersion;		/* version of the structure	*/
	word	wReserved;		/* alignment			*/
	W32DCB	dcb;			/* device control block		*/
	dword	dwProviderSubType;	/* ordinal for provider-defined	*/
					/* data structure format	*/
	dword	dwProviderOffset;	/* Offset of provider-specific	*/
					/* data field in bytes from the	*/
					/* start of this structure	*/
	dword	dwProviderSize;		/* size of provider-specific	*/
					/* data field			*/
	word	wcProviderData[1];	/* provider-specific data	*/
	word	filler;			/* Fill for now (sizeof == 52)	*/
} W32COMMCONFIG;

# define W32COMMCONFIG_MIN_SIZE \
	( (dword)&(((W32COMMCONFIG *)0)->dcb) + sizeof(W32DCB) )

# endif /* SERIAL___H */
