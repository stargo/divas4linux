
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

#if defined(LINUX)
# include <linux/version.h>
# include <linux/types.h>
# include <linux/kernel.h>
# include <linux/fs.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)
# include <linux/malloc.h>
#else
# include <linux/slab.h>
#endif

# include <linux/tty.h>
# include <linux/tty_flip.h>
# include <linux/major.h>
# include <linux/mm.h>
# include <asm/string.h>
# include <asm/uaccess.h>
# include <asm/errno.h>
# include <asm/segment.h>


#undef N_DATA


#endif /* LINUX */        

# include "port.h"
# include "pc.h"
# include "serial.h"
# include "sys_if.h"	/* wrapped system functions	*/
# include "atp_if.h"	/* AT command handler interface	*/
# include "rna_if.h"	/* RNA data handler interface	*/
# include "btx_if.h"	/* BTX data handler interface	*/
# include "fax_if.h"	/* FAX data handler interface	*/
# include "log_if.h"	/* logging interface	     	*/
# include "queue_if.h"	/* queue interface		*/
# include "di_defs.h"
# include "vcommemu.h"
# include "debuglib.h"

#if defined(LINUX)
extern word rnaRecv (ISDN_PORT *, byte **, word , byte *, word);
extern word btxRecv (ISDN_PORT *, byte **, word , byte *, word);
extern word faxRecv (ISDN_PORT *, byte **, word , byte *, word , word);
extern void faxTxEmpty (ISDN_PORT *);
extern word rnaWrite (ISDN_PORT *, byte **, word , byte *, word );
extern word btxWrite (ISDN_PORT *, byte **, word , byte *, word );
extern word faxWrite (ISDN_PORT *,byte **, word ,byte *, word , word *);
#endif /* LINUX */   

extern int Channel_Count;

int Win95 = 0;
extern void diva_os_didd_read (DESCRIPTOR* d, int* length);
extern void atInit (ISDN_PORT *P, int NewPort, byte *Address, int Profile);

/* Global driver data is centralized in this structure */
ISDN_PORT_DRIVER PortDriver =
{
	0,	/* Isdn  - not attached to ISDN machine	*/
	0,	/* Ready - not completely initialized	*/
} ;

#if defined(WINNT) || defined(UNIX)
#define VXD_PUSHAD
#define VXD_POPAD
#endif /* WINNT */

static byte EvenParity [128] = {
	0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
	0x88, 0x09, 0x0a, 0x8b, 0x0c, 0x8d, 0x8e, 0x0f,
	0x90, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17,
	0x18, 0x99, 0x9a, 0x1b, 0x9c, 0x1d, 0x1e, 0x9f,
	0xa0, 0x21, 0x22, 0xa3, 0x24, 0xa5, 0xa6, 0x27,
	0x28, 0xa9, 0xaa, 0x2b, 0xac, 0x2d, 0x2e, 0xaf,
	0x30, 0xb1, 0xb2, 0x33, 0xb4, 0x35, 0x36, 0xb7,
	0xb8, 0x39, 0x3a, 0xbb, 0x3c, 0xbd, 0xbe, 0x3f,
	0xc0, 0x41, 0x42, 0xc3, 0x44, 0xc5, 0xc6, 0x47,
	0x48, 0xc9, 0xca, 0x4b, 0xcc, 0x4d, 0x4e, 0xcf,
	0x50, 0xd1, 0xd2, 0x53, 0xd4, 0x55, 0x56, 0xd7,
	0xd8, 0x59, 0x5a, 0xdb, 0x5c, 0xdd, 0xde, 0x5f,
	0x60, 0xe1, 0xe2, 0x63, 0xe4, 0x65, 0x66, 0xe7,
	0xe8, 0x69, 0x6a, 0xeb, 0x6c, 0xed, 0xee, 0x6f,
	0xf0, 0x71, 0x72, 0xf3, 0x74, 0xf5, 0xf6, 0x77,
	0x78, 0xf9, 0xfa, 0x7b, 0xfc, 0x7d, 0x7e, 0xff,
};
static byte ByteSizeMasks [] = {
	0x7f, 0x7f, 0x7f, 0x7f, 0x0f, 0x1f, 0x3f, 0x7f
};

static void portStrip (ISDN_PORT *P, byte *Data, dword sizeData)
{
/* strip received data bytes (assumes P->DCB.Bytesize < 8) */
	byte Mask;
	for (Mask = ByteSizeMasks[P->DCB.ByteSize];
	     sizeData--; *Data++ &= Mask) ;
}

static void portParity (ISDN_PORT *P, byte *Data, dword sizeData)
{
/* strip data bytes to send and add parity (assumes P->DCB.Bytesize < 8) */

	byte Mask;
	for (Mask = ByteSizeMasks[P->DCB.ByteSize]; sizeData--; Data++) {
		switch (P->DCB.Parity) {
		default: /* 0=None,3=Mark,4=Space */
			*Data &= Mask;
			break;
		case 1 : /* odd	*/
			*Data = EvenParity[*Data & Mask] ^ 0x80;
			break;
		case 2 : /* even */
			*Data = EvenParity[*Data & Mask];
			break;
		}
	}
}

static void portTxUnTimeout (ISDN_PORT *P)
{
/*
 * don't clear TxOvershot here because this function is called
 * from AT command processing (ATH, ATZ ...) too and clearing
 * the TXOvershot flag would result in a missing notification.
 */
	P->TxFlow	= 0;
	P->TxDeferTime  = 0;
	sysCancelTimer (&P->TxDeferTimer);
	P->sizeTxCompile = 0;
	sysCancelTimer (&P->TxCompileTimer);
}

static void portRxUnTimeout (ISDN_PORT *P)
{
	sysCancelTimer (&P->RxNotifyTimer);
}

void portUnTimeout (ISDN_PORT *P)
{
	portRxUnTimeout (P);
	portTxUnTimeout (P);
}

void portSetMsr (ISDN_PORT *P, byte Mask, byte Value)
{
	byte Msr; dword Event;

	Msr = *P->pMsrShadow;

	Msr &= ~Mask;
	Msr |= Value;

	if (Msr == *P->pMsrShadow) return;

	*P->pMsrShadow = Msr | (Mask >> 4);

	switch (Mask)
	{
	case MS_RLSD_ON : Event = EV_RLSD | EV_RLSDS  ; break;
	case MS_RING_ON : Event = EV_RING | EV_RingTe ; break;
	case MS_DSR_ON  : Event = EV_DSR  | EV_DSRS   ; break;
	case MS_CTS_ON  : Event = EV_CTS  | EV_CTSS   ; break;
	default		: return;
	}

	*P->pDetectedEvents &= ~Event;
	P->ModemStateEvents &= ~Event;

	if (Value == 0) {
			Event &= ~(EV_RLSDS | EV_RingTe | EV_DSRS | EV_CTSS);
	}

	*P->pDetectedEvents |= Event;
	P->ModemStateEvents |= Event & ~(EV_RLSD | EV_RING | EV_DSR | EV_CTS);
}

void portEvNotify (ISDN_PORT *P)
{
/* Notify client if there is some event he wants to know about.	*/
/* Because VCOMM sometimes polls 'P->DetectedEvents' we always	*/
/* must clear flagged but currently not enabled events.		*/
/* Then we call the notification callback (if any).		*/

	if ((*P->pDetectedEvents &= P->EvNotify.Trigger) && P->EvNotify.Proc &&
	    (*P->pDetectedEvents & ~(EV_RLSDS|EV_RingTe|EV_DSRS|EV_CTSS))) {
		if (++(P->NotifyLock) <= 1) {
			LOG_FNC(("[%s] EvNfy: Ev=%x Msr=%x",
				  P->Name, *P->pDetectedEvents, *P->pMsrShadow));
			/* this callback is known to mangle registers	*/
			/* which crashes when we optimize the code	*/
			(*P->EvNotify.Proc) (P, P->EvNotify.Data,
					     	CN_EVENT, *P->pDetectedEvents);
		}
		--(P->NotifyLock);
	}

}

static void portRxNotify (ISDN_PORT *P) {
/* notify client if there is data waiting in input queue */


	if (P->RxNotify.Proc && !sysTimerPending (&P->RxNotifyTimer) &&
	    ( (P->PortData.QInCount >= P->RxNotify.Trigger) ||
	      (P->RxNotifyAny && P->PortData.QInCount)	       )  ) {
		if (++(P->NotifyLock) <= 1) {
			/* this callback seems to be clean but... */
			(*P->RxNotify.Proc) (P, P->RxNotify.Data,
						CN_RECEIVE, 0);
		}
		--(P->NotifyLock);
	}

}

void portTxNotify (ISDN_PORT *P) {
	sysScheduleDpc (&P->TxRestartWriteDpc);
}

static void portTxNotify_in_dpc (ISDN_PORT *P)
{
/* notify client if there is room in output queue again */

	if (P->TxNotify.Proc &&
			((P->TxOvershot && P->PortData.QOutCount < P->TxNotify.Trigger) ||
				(queueCount (&P->TxQueue) < 256))) {
		if (++(P->NotifyLock) <= 1) {
			P->TxOvershot = 0;
			/* this callback seems to be clean but... */
			(*P->TxNotify.Proc) (P, P->TxNotify.Data,
						CN_TRANSMIT, 0);
		}
		--(P->NotifyLock);
	}

	if (!queueCount (&P->TxQueue)) {
		P->PortData.QOutCount = 0; /* just to be sure */
		*P->pDetectedEvents |= EV_TXEMPTY;
		portEvNotify (P);
	}

}

void portRxEnqueue (ISDN_PORT *P, byte *Data, word sizeData, int frame)
{
/* enqueue data in receive queue */

	byte *Place;

	if (!Data || !sizeData) return;

	/* some clients (for example hyperterm) use this as receive trigger */

	*P->pLastReceiveTime = sysTimerTicks();

	/* Some clients (for example RNA, WinCim) ask for these events	*/
	/* but for RNA the normal receive indication is sufficient.	*/
	/* Other clients use them to trigger receive lights and thus we	*/
	/* give them what they want.					*/

	if (P->Mode != P_MODE_RNA) {
		*P->pDetectedEvents |= EV_RXCHAR;
		if (P->EvNotify.Trigger & EV_RXFLAG) {
			/* must scan for flag char */
			byte *p; dword i;
			for (p = Data, i = sizeData; i--; p++) {
		    		if (*p && (*p == P->DCB.EvtChar1 ||
					   *p == P->DCB.EvtChar2  )) {
					*P->pDetectedEvents |= EV_RXFLAG;
					break;
				}
			}
		}
	}

	switch (frame) {

	case 1 : /* respect frame boundaries */

		if (!queueWriteMsg (&P->RxQueue, Data, sizeData)) {
			DBG_TRC(("I: at %d overrun\n", __LINE__))
			goto overrun;
		}
		break;

	default: /* treat as byte stream */

		if ((P->PortData.QInCount += sizeData) > P->PortData.QInSize) {
			DBG_TRC(("I: at %d overrun, QInSize=%d, QInCount=%d, sizeData=%d\n",
				__LINE__, P->PortData.QInSize, P->PortData.QInCount, sizeData))
			P->PortData.QInCount -= sizeData;

overrun:
			P->PortData.dwCommError |= CE_RXOVER;
			DBG_FTL(("[%p:%s] RxEnq: %d overrun", P->Channel, P->Name, sizeData));
			break;
		}

		Place = P->PortData.QInAddr + P->PortData.QInPut;

		if (P->PortData.QInPut + sizeData > P->PortData.QInSize) {
			/* wraparound, copy tail here, head as usual */
			P->PortData.QInPut =
				(dword) sizeData -
				(P->PortData.QInSize - P->PortData.QInPut);
			sizeData -= (word) P->PortData.QInPut;
			mem_cpy (P->PortData.QInAddr,
				 Data + sizeData, P->PortData.QInPut);
			if (P->DCB.ByteSize < 8)
				portStrip (P, P->PortData.QInAddr,
					      P->PortData.QInPut);
		} else if (P->PortData.QInPut + sizeData ==
							 P->PortData.QInSize) {
			P->PortData.QInPut = 0;
		} else {
			P->PortData.QInPut += sizeData;
		}

		mem_cpy (Place, Data, sizeData);
		if (P->DCB.ByteSize < 8) portStrip (P, Place, sizeData);
	}

	portEvNotify (P);
	portRxNotify (P);
}

void diva_port_add_pending_data (void* C, void* hP, int count) {
	ISDN_PORT *P;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C ||
	    (P->State != P_DATA && P->Mode != P_MODE_FAX) )
		return;

	P->rx_queued_in_event_queue += count;
	if (P->rx_queued_in_event_queue < 0) {
		P->rx_queued_in_event_queue = 0;
	}

}

static int portWant (void *C, void *hP, word sizeData)
{
	ISDN_PORT *P;


	if (!(P = (ISDN_PORT *) hP) || P->Channel != C ||
	    (P->State != P_DATA && P->Mode != P_MODE_FAX) ||
			(!P->RxBuffer.Buffer)) {
		return (-1);	/* don't want, discard */
	}
#ifdef ACCOUNT_FOR_RTS_XOFF_FLOW
	if ((P->RxRtsFlow
	  && ((P->FlowOpts == AT_FLOWOPTS_RTSCTS) || (P->FlowOpts == AT_FLOWOPTS_BOTH)))
	 || (P->RxXoffFlow
	  && ((P->FlowOpts == AT_FLOWOPTS_XONXOFF) || (P->FlowOpts == AT_FLOWOPTS_BOTH))))
	{
		return (0);
	}
#endif
	sizeData += P->rx_queued_in_event_queue;

	if (P->Mode == P_MODE_FRAME) {
		sizeData += 64;
		return (queueSpace (&P->RxQueue, sizeData));
	} else if (P->Mode == P_MODE_FAX) {
		return ((P->PortData.QInCount == 0)
		     || (P->PortData.QInCount + 2 * sizeData + 256 <= P->PortData.QInSize));
	} else {
		return (P->PortData.QInCount + sizeData <= P->PortData.QInSize);
	}
}

static int portRecv (void *C, void *hP,
		     byte *Data, word sizeData, word FrameType)
{
	ISDN_PORT *P;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C ||
			!P->RxBuffer.Buffer) {
		DBG_ERR(("<<recv>> link?"));
		return (0);
	}

	if (P->State != P_DATA && P->Mode != P_MODE_FAX) {
		DBG_TRC(("[%p:%s] <<recv>> %s ?", P->Channel, P->Name, atState (P->State)))
		return (0);
	}

	if (!Data || !sizeData) {
		DBG_TRC(("[%p:%s] <<Recv>> args?", P->Channel, P->Name))
		return (1);
	}

	switch (P->Mode) {
	case P_MODE_RNA  :
		sizeData = rnaRecv (P, &Data, sizeData, P->Stream, sizeof(P->Stream));
		break;
	case P_MODE_BTX  :
		sizeData = btxRecv (P, &Data, sizeData, P->Stream, sizeof(P->Stream));
		break;
	case P_MODE_FAX  :
		sizeData = faxRecv (P, &Data, sizeData,
					P->Stream, sizeof(P->Stream), FrameType);
		break;
	case P_MODE_MODEM:
	case P_MODE_VOICE:
	case P_MODE_FRAME:
	default:
		break;
	}

	if (sizeData) {
		portRxEnqueue (P, Data, sizeData, (P->Mode == P_MODE_FRAME));
	}

	return (1);
}

void _cdecl portSetCommDcb (ISDN_PORT *P, _DCB *pDCB, dword ActionMask)
{
	if (ActionMask & fBitMask)	P->DCB.BitMask	  = pDCB->BitMask;
	if (ActionMask & fBaudRate)	P->DCB.BaudRate	  = pDCB->BaudRate;
	if (ActionMask & fXonLim)	P->DCB.XonLim	  = pDCB->XonLim;
	if (ActionMask & fXoffLim)	P->DCB.XoffLim	  = pDCB->XoffLim;
	if (ActionMask & fLCR) {
					P->DCB.ByteSize   = pDCB->ByteSize;
					P->DCB.Parity	  = pDCB->Parity;
					P->DCB.StopBits   = pDCB->StopBits;
	}
	if (ActionMask & fXonChar)	P->DCB.XonChar	  = pDCB->XonChar;
	if (ActionMask & fXoffChar)	P->DCB.XoffChar   = pDCB->XoffChar;
	if (ActionMask & fErrorChar)	P->DCB.ErrorChar  = pDCB->ErrorChar;
	if (ActionMask & fEofChar)	P->DCB.EofChar	  = pDCB->EofChar;
	if (ActionMask & fEvtChar1)	P->DCB.EvtChar1   = pDCB->EvtChar1;
	if (ActionMask & fEvtChar2)	P->DCB.EvtChar2   = pDCB->EvtChar2;
	if (ActionMask & fRlsTimeout)	P->DCB.RlsTimeout = pDCB->RlsTimeout;
	if (ActionMask & fCtsTimeout)	P->DCB.CtsTimeout = pDCB->CtsTimeout;
	if (ActionMask & fDsrTimeout)	P->DCB.DsrTimeout = pDCB->DsrTimeout;
	if (ActionMask & fTxDelay)	P->DCB.TxDelay	  = pDCB->TxDelay;

#if !defined(UNIX)
	DBG_CONN(("[%p:%s] SetDcb: f=%x", P->Channel, P->Name, ActionMask));
	DBG_BLK(((char *)&P->DCB, sizeof (P->DCB)));
#endif
}

int _cdecl PortSetCommState (ISDN_PORT *P, _DCB *pDCB, dword ActionMask)
{
	portSetCommDcb (P, pDCB, ActionMask);

	if ((ActionMask & fBitMask) && (P->DCB.BitMask & fDTRDisable))
		atDtrOff (P);

	P->PortData.dwLastError = 0;
	return (TRUE);
}

int portSetQueue (
	ISDN_PORT *P, byte** Buffer, dword *Size, QUEUE_BUF *B, byte which)
{
	if (!*Size) {
		/* use the preallocated buffer */
		*Buffer = B->Buffer;
		*Size  = B->Size;
	} else if (!*Buffer) {
		/* use the preallocated buffer or expand it (never shrink) */
		if (*Size > B->Size) {
			void *hPages = B->hPages;
#if 0
			/* DBG_TRC(("MEM: alloc %s at %d", __FILE__, __LINE__)) */
			if (!(*Buffer = sysPageRealloc(Size,&hPages))) {
				DBG_FTL(("[%p:%s] SetQ: %c %d nomem", P->Channel, P->Name, which, *Size))
				return(0);
			}
#else
		/*
			We can not re-alloc here - in the interrupt context. Also we will easy
      leave the size as before.
			In case of problem it will be necessary to convert this memory to the
      non-paged pool.
			Currently we easy try to allocate MAX TX instead of MIX TX on begin
			*/
			*Size = B->Size;
			*Buffer = B->Buffer;
#endif
			B->hPages = hPages;
			B->Buffer = *Buffer;
			B->Size	  = *Size;
		}
		*Buffer = B->Buffer;
		*Size	= B->Size;
	}
	return (1);
}

void portInitQIn (ISDN_PORT *P, byte *Stream, dword sizeStream)
{
	P->PortData.QInAddr  = Stream;
	P->PortData.QInSize  = sizeStream;
	P->PortData.QInCount = P->PortData.QInGet = P->PortData.QInPut = 0;
}

void portInitQOut (ISDN_PORT *P, byte *Stream, dword sizeStream)
{
	P->PortData.QOutAddr  = Stream;
	P->PortData.QOutSize  = sizeStream;
	P->PortData.QOutCount = P->PortData.QOutGet = P->PortData.QOutPut = 0;
}

int _cdecl PortSetModemStatusShadow (ISDN_PORT *P, byte *pMSRShadow)
{
	LOG_FNC(("[%s] SetMsrShadow: %d", P->Name, !!pMSRShadow));

	if (!pMSRShadow) {
		P->pMsrShadow = &P->PortData.bMSRShadow;
	} else {
		*pMSRShadow   = *P->pMsrShadow;
		P->pMsrShadow = pMSRShadow;
	}

	P->PortData.dwLastError = 0;
	return(TRUE);
}

int _cdecl PortEscapeFunction (
	ISDN_PORT *P, long lFunc, long InData, dword *OutData)
{
	dword LastReceiveTime;

	switch (lFunc) {

/* 1 */	case SETXOFF:		/* Simulate XOFF received		*/
		LOG_SIG(("[%s] Esc: XOFF", P->Name));
		P->TxFlow = 1;
		break;
/* 2 */	case SETXON:		/* Simulate XON received		*/
		LOG_SIG(("[%s] Esc: XON", P->Name));
		P->TxFlow = 0;
		break;
/* 3 */	case SETRTS:		/* Set RTS high				*/
		LOG_SIG(("[%s] Esc: RTS+", P->Name));
		P->RxRtsFlow = 0;
		break;
/* 4 */	case CLRRTS:		/* Set RTS low				*/
		LOG_SIG(("[%s] Esc: RTS-", P->Name));
		P->RxRtsFlow = 1;
		break;
/* 5 */	case SETDTR:		/* Set DTR high				*/
		LOG_SIG(("[%s] Esc: DTR+", P->Name));
		break;
/* 6 */	case CLRDTR:		/* Set DTR low (mostly drop connection)	*/
		LOG_SIG(("[%s] Esc: DTR-", P->Name));
		atDtrOff (P);
		break;
/* 7 */	case RESETDEV:		/* Reset device if possible		*/
#	ifdef Not_Vxd
/* 8 */	case GETLPTMAX:		/* Get maximum LPT supported (reserved)	*/
/* 9 */	case GETCOMMAX:		/* Get maximum COM supported (reserved) */
#	endif /* Not_Vxd */
		goto not_handled;

/* 10 */case GETCOMBASEIRQ:	/* Get COM base and IRQ			*/
/* 11 */case GETCOMBASEIRQ1:	/* same for compatibility		*/
		LOG_SIG(("[%s] Esc: IRQ", P->Name));
		if (OutData) *OutData = 0;	/* no IRQ, no IOPort */
		break;

/* 12 */case SETBREAK:		/* Set break condition			*/
/* 13 */case CLEARBREAK:	/* Clear break condition		*/
# ifdef Not_Vxd
/* 14 */case GETPORTHANDLE:	/* Get handle for Port (not to support) */
/* 15 */case GETEXTENDEDFNADDR:	/* Get the address of extended funcs	*/
# endif /* Not_Vxd */
		goto not_handled;

/* 16 */case CLRTIMERLOGIC:	/* Clear the timer logic of the driver	*/
		LOG_SIG(("[%s] Esc: TIM-", P->Name));
		P->RxNotifyAny = 0;
		break;

/* 17 */case GETDEVICEID:	/* Get the device ID of the device	*/
/* 18 */case SETECPADDRESS:	/* Set ECP channel address		*/
		goto not_handled;

/* 19 */case SETUPDATETIMEADDR:	/* Set address of update field for	*/
				/* last time a char was received	*/
		LOG_SIG(("[%s] Esc: UPD%c", P->Name, InData? '+' : '-'));
		LastReceiveTime = *P->pLastReceiveTime;
		P->pLastReceiveTime = InData? (dword *) InData :
					       &P->PortData.dwLastReceiveTime;
		*P->pLastReceiveTime = P->PortData.QInCount ?
				       LastReceiveTime : 0;
		break;

/* 20 */case IGNOREERRORONREADS:/* Ignore pending IO errors on reads	*/
		LOG_SIG(("[%s] Esc: ERR-", P->Name));
		break;	/* don't have this problem */

/* 21 */case ENABLETIMERLOGIC:	/* Re-enable timer logic code		*/
		LOG_SIG(("[%s] Esc: TIM+", P->Name));
		P->RxNotifyAny = 1;
		portRxNotify (P);
		break;

/* 22 */case IGNORESELECT:	/* Ignore select bit			*/

	default:
	not_handled:
		LOG_FNC(("[%s] Esc: %d ?", P->Name, lFunc));
		break;
	}

	portEvNotify (P);

	P->PortData.dwLastError = 0;
	return(TRUE);
}

int _cdecl PortPurge (ISDN_PORT *P, dword RecQueueFlag)
{
 	LOG_FNC(("[%s] Purge: %c", P->Name, RecQueueFlag ? 'R' : 'W'));

	if (RecQueueFlag) {
		portRxUnTimeout (P);
		queueReset (&P->RxQueue);
		portInitQIn (P, P->PortData.QInAddr,
				 P->PortData.QInSize);
	} else {
		portTxUnTimeout (P);
		queueReset (&P->TxQueue);
		portInitQOut (P, P->PortData.QOutAddr,
				  P->PortData.QOutSize);
		portTxNotify (P);
	}

	P->PortData.dwLastError = 0;
	return(TRUE);
}

int _cdecl PortSetEventMask (ISDN_PORT *P, dword dwMask, dword *pEvents)
{
	byte Msr; dword Events;

	Msr     = *P->pMsrShadow;
	Events  = *P->pDetectedEvents;
	Events &= ~(EV_RLSDS | EV_RingTe | EV_DSRS | EV_CTSS);
	Events |= P->ModemStateEvents;
	Events &= P->EvNotify.Trigger;

	LOG_SIG(("[%s] SetEvt: M=%x E=%x Msr=%x", P->Name, dwMask, Events, Msr));

	P->EvNotify.Trigger = dwMask;
	P->pDetectedEvents = pEvents? pEvents : &P->PortData.dwDetectedEvents;

	/* The following is a special hack for incoming connections.	*/
	/*								*/
	/* When we get an 'up' indication we set RLSD status and delta	*/
	/* bit in the MSR and try to indicate this event.		*/
	/* Unfortunately 'EvNotify.Trigger' is mostly 0 when answering	*/
	/* an incoming call and thus the event is lost.			*/
	/*								*/
	/* By checking the MSR delta bits we can see if the MSR events	*/
	/* were consumed already and if not we indicate them again now	*/
	/* (portEvNotify() clears out again any event not in mask).	*/

	if (Msr & (MS_RLSD_ON >> 4)) Events |= EV_RLSD ;
	if (Msr & (MS_RING_ON >> 4)) Events |= EV_RING ;
	if (Msr & (MS_DSR_ON >> 4))  Events |= EV_DSR ;
	if (Msr & (MS_CTS_ON >> 4))  Events |= EV_CTS ;

	*P->pDetectedEvents = Events;

	portEvNotify (P);

	P->PortData.dwLastError = 0;
	return(TRUE);
}

#if 0
int _cdecl PortGetEventMask (ISDN_PORT *P, dword dwMask, dword *pEvents)
{
	dword Events;

	Events = *P->pDetectedEvents;
	Events &= ~(EV_RLSDS | EV_RingTe | EV_DSRS | EV_CTSS);
	Events |= P->ModemStateEvents;
	Events &= P->EvNotify.Trigger;

	LOG_FNC(("[%s] GetEvt: M=%x E=%x", P->Name, dwMask, Events));

	if (pEvents) *pEvents = (Events & dwMask);

	/* Clear all events in question from event shadow to prevent 	*/
	/* multiple indications but leave the modem status line bits.	*/

	Events &= ~dwMask | (EV_RLSDS | EV_RingTe | EV_DSRS | EV_CTSS);
	*P->pDetectedEvents = Events ;

	/* modem status change queries need special handling !		*/

	if (Events & (dwMask & ( EV_RLSD | EV_RING | EV_DSR | EV_CTS ))) {
		if (Events & EV_RLSD) *P->pMsrShadow &= ~(MS_RLSD_ON >> 4);
		if (Events & EV_RING) *P->pMsrShadow &= ~(MS_RING_ON >> 4);
		if (Events & EV_DSR)  *P->pMsrShadow &= ~(MS_DSR_ON >> 4);
		if (Events & EV_CTS)  *P->pMsrShadow &= ~(MS_CTS_ON >> 4);
	}

	P->PortData.dwLastError = 0;
	return(TRUE);
}
#endif

static int portTxEnq (ISDN_PORT *P, byte *Data, word sizeData, word FrameType)
{ /* save FrameType and Data in transmit queue, increment QOutCount */
  /* and remember if QOutCount has reached the send trigger now	    */

	byte *place;

	place = queueAllocMsg (&P->TxQueue, (word)(sizeof(word) + sizeData));
	if (place) {
		*((word *)place) = FrameType;
		mem_cpy (place + sizeof(word), Data, sizeData);
		queueCompleteMsg (place);
		P->PortData.QOutCount += sizeData;
		return TRUE;
	} else {
		/* DBG_TRC(("[%p:%s] TxEnq: Q full (%ld/%ld-%ld %ld)",
			  P->Channel, P->Name, sizeData,
			  P->PortData.QOutCount, queueCount (&P->TxQueue), (long)(P->TxQueue.Size))) */
		return FALSE;
	}
}

static void portTxDeq (ISDN_PORT *P, word sizeData)
{ /* remove first message from transmit queue, decrement QOutCount */

	queueFreeMsg (&P->TxQueue);
	if (queueCount (&P->TxQueue) && P->PortData.QOutCount > sizeData) {
		P->PortData.QOutCount -= sizeData;
	} else {
		P->PortData.QOutCount = 0;
		if (P->Mode == P_MODE_FAX) {
			faxTxEmpty (P); /* give em a chance to send OK on EOP */
		}
	}
}

void portTxPurge (ISDN_PORT *P)
{ /* remove all Data in transmit queue, clear QOutCount */
	portTxUnTimeout (P);
	queueReset (&P->TxQueue);
	faxTxEmpty (P); /* give em a chance to send OK on EOP */
	P->PortData.QOutCount = 0;

	/* DBG_TRC(("I: portTxPurge")) */

	portTxNotify (P);
}

int portXmit (ISDN_PORT *P, byte *Data, word sizeData, word FrameType)
{
	/* If ISDN signalled a flow control condition or if we have to	*/
	/* defer the next packet or if we did split the previous frame,	*/
	/* we simply append this message to queue and wait until the	*/
	/* next transmission is triggered anyway.			*/

	word split;

	split = (P->Split == 0) ? ISDN_MAX_FRAME : P->Split;

	if (P->TxFlow || P->TxDeferTime || queueCount (&P->TxQueue)) {
		/* first write all frags except last (if requested),	*/
		/* then the last (or only one)				*/
		/* BUGBUG: 'BytesWritten' returned by PortWrite() are	*/
		/* mostly incorrect in case of queue overrun !		*/
		for ( ;
		     split && sizeData > split &&
		     portTxEnq (P, Data, split, FrameType);
		     sizeData -= split, Data += split)
				;
		return (portTxEnq (P, Data, sizeData, FrameType));
	}

	/* We are ready to send this frame to ISDN */

	if (split && sizeData > split) {
		/* first write all frags to queue, then try to pass the	*/
		/* first frag from queue to ISDN			*/
		for ( ; ; ) {
			if (!portTxEnq (P, Data,
					   (word)((split < sizeData) ?
					     	   split : sizeData   ),
					    FrameType			)) {
				/* DBG_TRC(("I: Can't portTxEnq, also PURGE")) */
				portTxPurge (P);
				return FALSE;
			}
			if (sizeData > split) {
				sizeData -= split;
				Data	 += split;
			} else {
				break ;
			}
		}
		/* try to send the first frag now */
		Data = queuePeekMsg (&P->TxQueue, &sizeData);
		sizeData -= sizeof(word);
		switch (PortDriver.Isdn->Xmit (P, P->Channel,
						  Data + sizeof(word),
						  sizeData, FrameType)) {
		case  0 : /* hard error, clear our send queue */
			/* DBG_TRC(("I: Port TX ERROR!!!")) */
			portTxPurge (P);
			return FALSE;
		case -1 : /* temporary overload, try later */
			return TRUE;
		default : /* first frag passed to ISDN, remove from Q */
			portTxDeq (P, sizeData);
			break;
		}
	} else {
		switch (PortDriver.Isdn->Xmit (P, P->Channel,
						  Data, sizeData, FrameType)) {
		case  0 : /* hard error */
			/* DBG_TRC(("I: Port TX ERROR!!!")) */
			return FALSE;
		case -1 : /* temporary overload */
			return (portTxEnq (P, Data, sizeData, FrameType));
		default : /* alright */
			break;
		}
	}

	if (P->Defer && FrameType == N_DATA) {
		/* Compute the time when next frame should be sent but	*/
		/* make it a little bit shorter cause the timers fire	*/
		/* too late in most cases (i don't worry about a timer	*/
		/* wraparound here, a frame which is sent too early is	*/
		/* not really critical).				*/
		P->TxDeferTime = sysTimerTicks() + (sizeData*8000)/P->Defer;
	}

	/* don't notify here, we come from PortWrite() and it's done there */

	return(TRUE);
}

static void _cdecl portTxCompileTimer (ISDN_PORT *P)
{
	if (P->sizeTxCompile) {
		/* DBG_TRC(("I: Port TX Compile (%d)", P->sizeTxCompile)) */
 		if (portXmit (P, P->TxCompile, (word) P->sizeTxCompile,
			     N_DATA /* FrameType - normal data */))
		{
 			P->sizeTxCompile = 0;
		}
	}
}

int PortDoCollectAsync (ISDN_PORT* P) {
	return (P && (P->State == P_DATA) && (P->Mode == P_MODE_RNA) &&
					(P->port_u.Rna.Framing == P_FRAME_SYNC));
}

int _cdecl PortWrite (
	ISDN_PORT *P, byte *Data, dword sizeData, dword *sizeWritten)
{
	byte *p ; word FrameType ;
	dword n, i, split ;

	if (!P || P->State == P_IDLE) return (0);

	P->PortData.dwLastError = 0;

	if (!Data || !sizeData || !sizeWritten) {
		DBG_FTL(("[%p:%s] Write: args?", P->Channel, P->Name));
		return FALSE;
	}
	if (!P->TxBuffer.Buffer) {
		return (0);
	}

	*sizeWritten = sizeData;


	P->LastRwTick = PortDriver.Ticks;

	/* Even if we do not queue any byte we must act here as	*/
	/* if we did queue any byte before sending it and thus	*/
	/* set the overshot flag if QOutCont + sizeData reaches	*/
	/* the xmit trigger ('HyperTerminal' depends on it) !	*/

	if (P->TxNotify.Trigger &&
	    P->PortData.QOutCount + sizeData >= P->TxNotify.Trigger) {
		P->TxOvershot = 1;
	}

	if (P->State != P_DATA) {
		/* should be a Hayes modem command */
		atWrite (P, Data, (word) sizeData);
		portTxNotify (P);
		return TRUE;
	}

	/* do whatever necessary before transmitting data */

	FrameType = N_DATA; /* assume normal data frame */

	if ((!P->At.f.at_escape_option) && atEscape (P, Data, (word) sizeData)) {
		portTxPurge (P);
		sizeData = 0;
	} else {
	switch (P->Mode) {
	case P_MODE_FRAME:
	case P_MODE_VOICE:
		/* pass data as is */
		break;
	case P_MODE_RNA	 :
			sizeData = rnaWrite(P, &Data, (word) sizeData,
													P->data_Frame, sizeof (P->data_Frame));
		break;
	case P_MODE_BTX:
		/* first check for command mode escapes, then for framing */
		if ((P->At.f.at_escape_option) && atEscape (P, Data, (word) sizeData)) {
			portTxPurge (P);
			sizeData = 0;
		} else {
			sizeData = btxWrite(P, &Data, (word) sizeData,
					       P->data_Frame, sizeof (P->data_Frame));
		}
		break;
	case P_MODE_FAX:
		/* don't pass more data as fits in the frame buf to prevent */
		/* unhandled queue overruns when faxWrite calls portXmit(). */
		for ( *sizeWritten = 0 ; sizeData ; )
		{
			i = (sizeData < sizeof(P->data_Frame)) ? sizeData :
							 sizeof(P->data_Frame) ;
#if 0 /* WINNT */
			/* try to buffer only the minimum amount of data */
			if ( P->Class == P_CLASS_FAX1 )
			{
				if ( queueCount (&P->TxQueue) )
				{
					LOG_NET(("[%s] Write: Q busy", P->Name))
					sizeData = 0 ;
					break ;
				}
				i = (sizeData < FAX_MAX_SCANLINE)?
					 sizeData : FAX_MAX_SCANLINE  ;
			}
#endif /* WINNT */


			if ( !queueSpace (&P->TxQueue, (word)i) )
			{
				/* DBG_ERR(("A: %s at %d", __FILE__, __LINE__)) */
				goto TxFull ;
 			}

			p = Data ; /* 'p' may be changed by faxWrite */

			if ( ((n = faxWrite (P, &p, (word)i,
					    P->data_Frame, sizeof(P->data_Frame), &FrameType))
			       == FAX_WRITE_TXFULL)
			  || ( n && !portXmit (P, p, (word)n, FrameType) ) )
			{
				/* DBG_ERR(("A: %s at %d", __FILE__, __LINE__)) */
				goto TxFull ;
			}

 			Data         += i ;
			sizeData     -= i ;
			*sizeWritten += i ;
		}
		break;
	case P_MODE_MODEM:
	default		 :
		/* first check for command mode escapes */
		if ((P->At.f.at_escape_option) && atEscape (P, Data, (word) sizeData)) {
			/* stop compiling and any output */
			portTxPurge (P);
			sizeData = 0;
			break;
		}
		split = (P->Split == 0) ? ISDN_MAX_FRAME : P->Split;
		if (sizeData == 1 && 0)
		{
			/* and now try to compile a larger frame */
			n = queueGetFree (&(P->TxQueue), split + sizeof(word), &i);
			n = ((n > sizeof(word)) ? n - sizeof(word) : 0) + i * split;
			if (n > sizeof(P->TxCompile))
				n = sizeof(P->TxCompile);
			if (n > split)
				n = split;
			if (P->sizeTxCompile + sizeData < n)
			{
				P->TxCompile[(P->sizeTxCompile)++] = *Data;
				if (P->DCB.ByteSize < 8) {
					portParity (P,
						    P->TxCompile + (P->sizeTxCompile - 1),
						    1);
				}
				if (!sysTimerPending(&P->TxCompileTimer)) {
					sysStartTimer (&P->TxCompileTimer, 10/*msec*/);
				}
				sizeData = 0;	/* delay until timer fires */
			}
		}
		if (sizeData != 0)
		{
			*sizeWritten = 0;
			if (P->sizeTxCompile != 0)
			{
				mem_cpy (P->data_Frame, P->TxCompile, P->sizeTxCompile);
				n = (sizeData <= split - P->sizeTxCompile) ?
				    sizeData : split - P->sizeTxCompile;
				mem_cpy (P->data_Frame + P->sizeTxCompile, Data, n);
				if (P->DCB.ByteSize < 8)
				{
					portParity (P,
						    P->data_Frame + P->sizeTxCompile,
						    n);
				}
				if (!portXmit (P, P->data_Frame,
					          (word)(P->sizeTxCompile + n),
						  FrameType))
				{
					/* DBG_ERR(("A: %s at %d", __FILE__, __LINE__)) */
					goto TxFull ;
				}
				sizeData -= n;
				Data += n;
				*sizeWritten += n;
				P->sizeTxCompile = 0;
				sysCancelTimer (&P->TxCompileTimer);
			}
			while (sizeData != 0)
			{
				n = (sizeData <= split) ?
				    sizeData : split;
				mem_cpy (P->data_Frame, Data, n);
				if (P->DCB.ByteSize < 8)
					portParity (P, P->data_Frame, n);
				if (!portXmit (P, P->data_Frame,
					          (word) n,
						  FrameType))
				{
					/* DBG_ERR(("A: %s at %d", __FILE__, __LINE__)) */
					goto TxFull ;
				}
				sizeData -= n;
				Data += n;
				*sizeWritten += n;
			}
		}
	}
  }

	if (sizeData && !portXmit (P, Data, (word) sizeData, FrameType))
	{
		*sizeWritten = 0 ;
TxFull:
		/* DBG_ERR(("A: Tx is FULL !!!\n")) */
		P->PortData.dwCommError |= CE_TXFULL ;
	}

	/* notify if the xmit queue is below trigger or empty */

	portTxNotify (P) ;

	return TRUE ;
}

static void _cdecl portTxDeferTimer (ISDN_PORT *P)
{
	byte *Data; word sizeData;


	if ((Data = queuePeekMsg (&P->TxQueue, &sizeData))) {
		sizeData -= sizeof(word);
		switch (PortDriver.Isdn->Xmit (P, P->Channel,
						  Data + sizeof(word),
						  sizeData, *((word *)Data)) ) {
		case  0 : /* hard error, forget pending data */
			portTxPurge (P);
			break;
		case -1 : /* temporary overload, try later again */
			break;
		default : /* alright, free this message, wait for next 'Go' */
			portTxDeq (P, sizeData);
			P->TxDeferTime = sysTimerTicks() +
					 (sizeData*8000)/P->Defer;
			break;
		}
		portTxNotify (P);
	} else {
		P->TxDeferTime = 0;
	}
}

static void portFlowOutX (ISDN_PORT *P, byte Off, byte *func)
{
	byte *Xchar;

	/* it's surely not so bad to toggle CTS in MSR shadow	*/

	if (Off) {
		Xchar = (byte*)"\x13"; /*Xoff*/
		portSetMsr (P, MS_CTS_ON, 0/*off*/);
		/* DBG_TRC(("I: CTS OFF")) */
	} else {
		Xchar = (byte*)"\x11"; /*Xon*/
		portSetMsr (P, MS_CTS_ON, MS_CTS_ON/*on*/);
		/* DBG_TRC(("I: CTS ON")) */
	}

	/* In case of Xoff/Xon output flow control we remember	*/
	/* the flow control state but don't put anything to the	*/
	/* recv queue, nevertheless we must indicate a receive	*/
	/* (see serial.vxd sample code).			*/
	/* If flow control is not requested we queue the Xchar.	*/

#if defined(OLD_WAY)
	if (P->DCB.BitMask & fOutX) {
		P->TxFlow = Off;
		*P->pLastReceiveTime = sysTimerTicks();
		*P->pDetectedEvents |= EV_RXCHAR;
		portEvNotify (P);
	} else {
#endif
		/* both portRxEnqueue() and atRspStr() do portEvNotify() */
		if (P->State == P_DATA)
			portRxEnqueue (P, Xchar, 1, 0);
		else
			atRspStr (P, Xchar);
#if defined(OLD_WAY)
	}
#endif
}

void portFaxXon (ISDN_PORT *P)
{ /* A FAX modem sends Xon when it is ready to send data. */
  /* As far as i know this depends not on AT&K setting.   */
  /* To clone the serial driver behavior we must respect  */
  /* the port DCB flow control settings here.		  */

	if (P->DCB.BitMask & fOutX)
	{
		portFlowOutX (P, 0 /*Xon*/, (byte*)"FaxXon");
	}
	else
	{
    		atRspStr (P, (byte*)"\x11");
	}
}

static void portTxFlow (ISDN_PORT *P, byte Off)
{ /* Here we try to clone the normal modem Xoff/Xon flow */
  /* control and thus we do it only for AT&K4/5/6	 */

	if ((P->FlowOpts >= AT_FLOWOPTS_XONXOFF) && (P->TxFlow ^ Off))
		portFlowOutX (P, Off, (byte*)"TxFlow");
}

/*
 * UNIX Specific - used for controling the flow control
 * settings instead of having to do AT&Kx
 */
byte portGetFlow (ISDN_PORT *P)
{
		return (P->FlowOpts);
}


static void portStop (void *C, void *hP)
{
	ISDN_PORT *P;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C) {
		DBG_FTL(("<<stop>> link ?"));
		return;
	}

	if (P->State != P_DATA && P->State != P_ESCAPE) {
		DBG_FTL(("[%p:%s] <<stop>> %s ?", P->Channel, P->Name, atState (P->State)));
		return;
	}

	/* DBG_TRC(("I: PortStop")) */

	portTxFlow (P, 1 /*Xoff*/);
}

static void portGo (void *C, void *hP)
{ /* able to send the next message */
	ISDN_PORT *P;
	byte *Data; word sizeData;

	if (!(P = (ISDN_PORT *) hP) || P->Channel != C) {
		DBG_FTL(("<<go>> link ?"));
		return;
	}

	if (P->State != P_DATA && P->State != P_ESCAPE) {
		DBG_FTL(("[%p:%s] <<go>> %s ?", P->Channel, P->Name, atState (P->State)));
		return;
	}


	/* If flow control was set, clean it up now */

	portTxFlow (P, 0 /*Xon*/);

	/* If we have to defer the next xmit, set up the timer now */

	if (P->TxDeferTime) {
		dword Defer, Now;
		if (sysTimerPending(&P->TxDeferTimer))
			return;
		if ( ((Now = sysTimerTicks())        < P->TxDeferTime) &&
		     ((Defer = P->TxDeferTime - Now) > 1	     ) &&
		     ( Defer < 2000				     )    )
			sysStartTimer (&P->TxDeferTimer, Defer);
		else
			portTxDeferTimer (P);
		return;
	}

	/* No further constraints, try to send the next frame */

	if ((Data = queuePeekMsg (&P->TxQueue, &sizeData))) {
		sizeData -= sizeof(word);
		switch (PortDriver.Isdn->Xmit (P, P->Channel,
						  Data + sizeof(word),
						  sizeData, *((word *)Data)) ) {
		case  0 : /* hard error, forget pending data */
			portTxPurge (P);
			break;
		case -1 : /* temporary overload, try later again */
			break;
		default : /* alright, free this message, wait for next 'Go' */
			portTxDeq (P, sizeData);
			break;
		}
#if 0 /* VXD */
		/* as it was working the whole time before WINNT */
		portTxNotify (P);
#endif /* VXD */
	}
#if defined(WINNT) || defined(UNIX)
	/* because of possibly pending writes we need always a kick */
	portTxNotify (P);
#endif /* WINNT */

}

static void _cdecl portRxRestartRead (ISDN_PORT *P) {
	if ((P->State == P_IDLE) || !P->RxNotify.Proc) {
		return;
	}

	if (++(P->NotifyLock) <= 1) {
		/* this callback seems to be clean but... */
		if (P->RxNotify.Proc) {
		 (*P->RxNotify.Proc) (P, P->RxNotify.Data, CN_RECEIVE, 1);
		}
	}
	--(P->NotifyLock);
}

static void _cdecl portTxNotifyTimer (ISDN_PORT *P) {
	if (P && P->State != P_IDLE && P->TxNotify.Proc) {
		 (*P->TxNotify.Proc) (P, P->TxNotify.Data, CN_TRANSMIT, 0);
	}
}

static void portTxRestartWrite (ISDN_PORT* P) {
	if ((P->State == P_IDLE) || !P->TxNotify.Proc) {
		return;
	}
	portTxNotify_in_dpc (P);
}

static void _cdecl portRxNotifyTimer (ISDN_PORT *P) {
	if (P && P->State != P_IDLE) {
		portRxNotify (P);
	}
}

int PortReadSize(ISDN_PORT *P)
{
dword dwReadSize;
	if (!P || P->State == P_IDLE) return (0);

    dwReadSize = P->PortData.QInCount ?
	       P->PortData.QInCount : queueCount (&P->RxQueue);

	return (dwReadSize);
}


int _cdecl PortRead (
	ISDN_PORT *P, byte *Buffer, dword Wanted, dword *pGiven)
{
	byte *Place; word sizeFrame;
	dword Give;

	if (!P || P->State == P_IDLE) {
		*pGiven = 0;
		return (TRUE);
	}

	P->PortData.dwLastError = 0;

	Give = P->PortData.QInCount ?
	       P->PortData.QInCount : queueCount (&P->RxQueue);
 
	if (!Wanted || !Give) {
#if !defined(UNIX)
#if 1	/* don't log heavily polling apps */
		if (logMask & LOG_PAR_)
#endif
 		LOG_FNC(("[%s] Read: %d(%d) ", P->Name, Wanted, Give))
#endif
		*pGiven = 0;
		return TRUE;
	}


	P->LastRwTick = PortDriver.Ticks;

	if (!P->PortData.QInCount) {
		/* must be frame mode, return each frame separately */
		if ((Place = queuePeekMsg (&P->RxQueue, &sizeFrame))) {
			if (Wanted < sizeFrame) {
				DBG_TRC(("I: Frame Buffer Overflow"))
			}
			if (Wanted > sizeFrame) Wanted = sizeFrame;
			mem_cpy (Buffer, Place, Wanted);
			*pGiven = Wanted;
			queueFreeMsg (&P->RxQueue);
		} else {
			*pGiven = 0;
		}
		return TRUE;
	}

	if (Give > Wanted) {
		Give = Wanted;
		/*
		 * If P->Block is set we try to repair the ?HyperTerminal?
		 * problems when working at large speed (see the comment in
		 * 'portComStat' above).
		 */
		if (P->State == P_DATA &&
		    Wanted < P->Block && !sysTimerPending(&P->RxNotifyTimer)) {
			sysStartTimer (&P->RxNotifyTimer, P->Wait? P->Wait : 2);
		}
	}

	*pGiven = Give;

	P->PortData.QInCount -= Give;
	Place = P->PortData.QInAddr + P->PortData.QInGet;

	if (P->PortData.QInGet + Give > P->PortData.QInSize) {
		/* wraparound, copy tail first	*/
		P->PortData.QInGet = (dword) Give -
				     (P->PortData.QInSize - P->PortData.QInGet);
		Give -= P->PortData.QInGet;
		mem_cpy (Buffer + Give,
			 P->PortData.QInAddr, P->PortData.QInGet);
	} else if (P->PortData.QInGet + Give == P->PortData.QInSize) {
		P->PortData.QInGet = 0;
	} else {
		P->PortData.QInGet += Give;
	}

	mem_cpy (Buffer, Place, Give);
	return TRUE;
}

int _cdecl PortEnableNotification (ISDN_PORT *P,
																	 void (*EvNotifyProc)(void*, void*, long, long),
																	 void* ReferenceData)
{
	if ((P->EvNotify.Proc = EvNotifyProc)) {
		LOG_FNC(("[%s] SetECb: M=%x", P->Name, P->EvNotify.Trigger));
		P->EvNotify.Data = ReferenceData;
		portEvNotify (P);
	} else {
		LOG_FNC(("[%s] SetECb: off", P->Name));
	}

	P->PortData.dwLastError = 0;
	return(TRUE);
}


int _cdecl PortSetReadCallBack (
	ISDN_PORT *P,
	dword RxTrigger,
	void (*RxNotifyProc)(void*, void*, long, long),
	void* ReferenceData)
{
	if (((int) RxTrigger) < 0 || !RxNotifyProc) {
		LOG_FNC(("[%s] SetRCb: off", P->Name));
		P->RxNotify.Proc = 0;
		P->RxNotify.Trigger = (dword) -1; /* not evaluated if Proc=0 */
	} else {
		LOG_FNC(("[%s] SetRCb: T=%ld", P->Name, RxTrigger));
		P->RxNotify.Proc = RxNotifyProc;
		P->RxNotify.Data = ReferenceData;
		P->RxNotify.Trigger = (RxTrigger < P->PortData.QInSize) ?
				       RxTrigger : P->PortData.QInSize  ;
	}

	/* As i was taught by the 'serial' sample code resetting the	*/
	/* 'LastReceiveTime' seems to be necessary cause otherwise some	*/
	/* applications (for example Amaris BTX) fail cause of an early	*/
	/* read timeout generated by VCOMM.				*/
	/* As a side effect the permanent polling of input queue seen	*/
	/* with 'HyperTerminal' before disappeared now.		*/

	if (!P->PortData.QInCount) *P->pLastReceiveTime = 0;

	/* We are too fast for Win3x RAS, they set the threshold mostly */
	/* _after_ they had written some command but the echo what they */
	/* want to get signalled is already there			*/
	/* But..., at least one app (PROXITEL) runs in a loop this way.	*/
	/* Because this app is needed in some Win95 project, we fix it	*/
	/* for now by calling portRxNotify() only in Win3x.		*/

	if (P->State != P_DATA && !Win95) portRxNotify (P);

	P->PortData.dwLastError = 0;
	return(TRUE);
}

int _cdecl PortSetWriteCallBack(ISDN_PORT *P, dword TxTrigger,
																void(*TxNotifyProc)(void*, void*, long, long),
																void* ReferenceData )
{
	if ((long) TxTrigger <= 0 || !TxNotifyProc) {
		LOG_FNC(("[%s] SetWCb: off", P->Name));
		P->TxNotify.Proc = 0;
		P->TxNotify.Trigger = 0;
		P->TxOvershot = 0;
	} else {
		LOG_FNC(("[%s] SetWCb: T=%ld", P->Name, TxTrigger));
		P->TxNotify.Proc = TxNotifyProc;
		P->TxNotify.Data = ReferenceData;
		P->TxNotify.Trigger = (TxTrigger < P->PortData.QOutSize) ?
				       TxTrigger : P->PortData.QOutSize  ;
		P->TxOvershot = (P->PortData.QOutCount >= P->TxNotify.Trigger);
	}

	P->PortData.dwLastError = 0;
	return(TRUE);
}

static void SetPortDefaults (ISDN_PORT *P)
{
	P->EvNotify.Proc	= 0;
	P->EvNotify.Trigger	= 0;
	P->RxNotify.Proc	= 0;
	P->RxNotify.Trigger	= (dword)-1;
	P->TxNotify.Proc	= 0;
	P->TxNotify.Trigger	= 0;

	P->pMsrShadow		= &P->PortData.bMSRShadow;
	P->pDetectedEvents	= &P->PortData.dwDetectedEvents;
	P->pLastReceiveTime	= &P->PortData.dwLastReceiveTime;
	P->ModemStateEvents	= EV_CTSS | EV_DSRS;

}

static int _cdecl portStealCallback (void* Context, int Request)
{
	return FALSE;	/* nobody should steal us a port */
}

int _cdecl PortClose (ISDN_PORT *P)
{
	ISDN_PORT_DESC	*D;
	byte	Name[MAX_PORT_NAME];
	int		DontFree, Detach;

	sysCancelTimer (&P->TxNotifyTimer);


	str_cpy ((char*)Name, (char*)P->Name);	/* needed for good bye message	*/


	for (D = PortDriver.Ports; P != D->Port;){
		if (P->State == P_IDLE || ++D >= PortDriver.lastPort) {
			return (FALSE);
		}
	}

	/* Normally we would like to detach from ISDN when a port is	*/
	/* closed because the Port data could be freed freed now.	*/
	/* But the stupid WinFax Lite closes and opens the port before	*/
	/* it answers an inbound call, thus we don't detach COM ports	*/
	/* in P_OFFERED state.						*/
	/* Finally.. after some bad experiences with OSR2 we think it's	*/
	/* better never to free the Port data because either VCOMM or	*/
	/* UNIMODEM or both cannot forget a port fast enough and thus	*/
	/* sometimes crash terribly !					*/

	Detach	 = 1 ;	/* always detach from ISDN on close and consequently */
	DontFree = 1 ;	/* free Port data (nobody except us knows about it)  */


	atFinit (P, Detach);

	P->State = P_IDLE;



	/* Win3x VCOMM crashes when the Port data is freed and some COM	*/
	/* based programs (WinFax 4.0, WinFax Lite) expect that the mo-	*/
	/* dem settings hold after a close, thus we don't free the Port	*/
	/* data of COMn ports !						*/
	/* To be sure against crashes in this case, we must reset the	*/
	/* varios shadow pointers as well as any notification functions */
	/* and prepare a small input queue for unsolicited responses.	*/

	if (DontFree) {
		SetPortDefaults (P);

		PortPurge (P, 0);
		PortPurge (P, 1);

		P->HoldState = Detach ? P_COMMAND : P_OFFERED;
		P->State = P_IDLE;

	} else {
		/* no problem to free the port data	*/
		sysPageFree (P->hPages);
		D->Port = 0;
	}

	PortDriver.curPorts--;


	return(TRUE);
}

char* get_port_name (void* hP) {
	ISDN_PORT* P = (ISDN_PORT*)hP;
	return ((P != 0) ? (char*)P->Name : "?");
}
static char *portName (void *C, void *hP)
{ /* return the name of the port associated to the channel */
	return ( hP? (char *)((ISDN_PORT *)hP)->Name : "?" ) ;
}


/*
 *  System Control Message Processors
 *
 *	vcomm.h defines all entries in the PortFunctions structure and
 *	the PortInitialize function as:  BOOL (*pPort<function>)();
 *	The evaluation of this specification depends on the -Gz flag of
 *	the compiler (_cdecl if -Gz not set, _stdcall if -Gz set) but the
 *	functions are expected to be _cdecl.
 *	Thus we cast pointers to such functions according to the current
 *	-Gz setting using the PORT_FUNCTION typedef below.
 *
 *  vcommemu.h which is used instead of vcomm.h for the WinNT version
 *	specifies the functions exactly and exports a PORT_FUNCTION typedef.
 */

#ifndef PORT_FUNCTION_TYPE_
typedef BOOL (* PORT_FUNCTION)();
#endif

ISDN_PORT* ttyFindPort (int nr) {
	if (nr && (nr < (Channel_Count+1))) {
		ISDN_PORT_DESC	*D;
		char Name[24];

		sprintf (Name, "%s%d", DiPort_DeviceName, nr);

		for (D = PortDriver.Ports; str_i_cmp ((char*)D->Name, (char*)Name); ) {
			if (++D >= PortDriver.lastPort) {
				return (0);
			}
		}
		return (D->Port);
	}

	return (0);
}

ISDN_PORT * _cdecl PortOpen (
	byte  *Name,	/* name of port to open	*/
	dword *Error,	/* specific error code	*/
	byte  *Address, /* Acpt address for this Port */
	int    Profile) /* Profile for this Port */
{
	static _DCB MyDcb = {
	sizeof (_DCB), 	/* DCBLength	- sizeof (DCB) */
	115200,		/* BaudRate	- port Baudrate */
        fBinary,	/* BitMask	- flag DWORD */
	(dword)-1L,	/* XonLim	- Transmit X-ON threshold */
	(dword)-1L,	/* XoffLim	- Transmit X-OFF threshold */
        0,		/* wReserved	- reserved */
        8,		/* ByteSize	- Number of bits/byte, 4-8 */
	NOPARITY,	/* Parity	- 0-4=None,Odd,Even,Mark,Space */
	ONESTOPBIT,	/* StopBits	- 0,1,2 = 1, 1.5, 2 */
	0x11,		/* XonChar	- Tx and Rx X-ON character */
	0x13,		/* XoffChar	- Tx and Rx X-OFF character */
	0,		/* ErrorChar	- Parity error replacement char */
	0,		/* EofChar	- End of Input character */
	0,		/* EvtChar1	- special event character */
	0,		/* EvtChar2	- Another special event character */
	0,		/* bReserved	- reserved */
	0L,		/* RlsTimeout	- Timeout for RLSD to be set */
	0L,		/* CtsTimeout	- Timeout for CTS to be set */
	0L,		/* DsrTimeout	- Timeout for DSR to be set */
	0L		/* TxDelay	- Amount of time between chars */
	};
	ISDN_PORT_DESC	*D;
	ISDN_PORT	*P = NULL;
	int		NewPort;
	byte		State;
	void		*hPortPages;
	void		*hRxPages,	*hTxPages;
	byte		*RxBuffer,	*TxBuffer;
	dword		sizeRxBuffer,	sizeTxBuffer;
	dword		hPort;
	byte		NiceName[MAX_PORT_NAME];
	int		i ;

	for ( i = 0 ; ; i++ )
	{ /* beautify the port name */
		NiceName[i] = Name[i] ;
		if ( NiceName[i] == 0 )
			break ;
		if ( NiceName[i] >= 'A' && NiceName[i] <= 'Z' )
			NiceName[i] |= ' ' ;
	}

 	LOG_SIG(("[%s] Open: (%d) { ", NiceName, PortDriver.curPorts));

	/* Gets matches the Port Name */
	for (D = PortDriver.Ports; str_i_cmp ((char*)D->Name, (char*)Name); ) {
		if (++D >= PortDriver.lastPort) {
			DBG_FTL(("[%s] Open: unknown", NiceName));
			*Error = (dword) IE_BADID;
			return (0);
		}
	}

	if (!PortDriver.Isdn) {
		DBG_FTL(("[%s] Open: no ISDN", NiceName));
		*Error = (dword) IE_HARDWARE;
		return (0);
	}

	if ((P = D->Port)) {
		if (P->State != P_IDLE) {
			DBG_FTL(("[%s] Open: already open", NiceName));
			*Error = (dword) IE_OPEN;	
			return (0);
		}
		hPortPages  = P->hPages;
		State	    = P->HoldState ;
		NewPort = 0;	/* don't change current modem settings */
	} else {
		dword fake = sizeof (ISDN_PORT);
		/* DBG_TRC(("MEM: alloc %s at %d", __FILE__, __LINE__)) */
		if (!(P = (ISDN_PORT *) sysPageAlloc(&fake, &hPortPages))) {
			DBG_FTL(("[%s] Open: nomem", NiceName));
			*Error = (dword) IE_MEMORY;
			return (0);
		}
		State   = P_COMMAND;
		NewPort = 1;	/* use default initial modem settings */
		/* DBG_TRC(("I: ALLOC")) */
	}


	hPort	     = 0;
	hRxPages = RxBuffer     = P->RxBuffer.Buffer;
	hTxPages = TxBuffer     = P->TxBuffer.Buffer;

	sizeRxBuffer = MIN_RX_QUEUE;
	sizeTxBuffer = MAX_TX_QUEUE /* + ISDN_MAX_FRAME*/;

	if (!RxBuffer) {
		RxBuffer = diva_mem_malloc (sizeRxBuffer+512);
	}
	if (!TxBuffer) {
		TxBuffer = diva_mem_malloc (sizeTxBuffer+512);
	}
	if (!TxBuffer || !RxBuffer) {
		goto failure;
	}

	str_cpy ((char*)P->Name, (char*)NiceName) ;

	/* initialize VCOMM PortData structure */

	mem_zero (&P->PortData, sizeof (P->PortData));
	P->PortData.PDLength		= sizeof (*P);
	P->PortData.PDVersion		= 1;
	//P->PortData.PDfunctions		= &MyPortFunctions ;
	P->PortData.PDNumFunctions	= Win95 ? 22 : 19;
	P->PortData.bMSRShadow		= MS_DSR_ON | MS_CTS_ON ;
	//P->PortData.ValidPortData	= 'SMTF';/*don't know why, see serial*/
	/* our send Q is a frame queue, thus we lie about the size */
	portInitQOut (P, TxBuffer, MIN_TX_QUEUE);
	/* our recv Q is (mostly) a byte queue managed in PortData */
	portInitQIn (P, RxBuffer, sizeRxBuffer);

	/* initialize DCB structure */

	P->DCB = MyDcb;

	/* initialize the common part of port structure */

	SetPortDefaults (P);

	/* initialize the multiple used part of port structure */

	mem_zero (&P->port_u, sizeof (P->port_u));

	/* initialize isdn specific part of port structure */

	P->hPages		= hPortPages;
	P->hPort		= hPort;
	P->LastRwTick		= 0;
	P->LastUsed		= 0;
	P->NotifyLock		= 0;
	P->RxBuffer.hPages	= hRxPages;
	P->RxBuffer.Buffer	= RxBuffer;
	P->RxBuffer.Size	= sizeRxBuffer;
	queueInit (&P->RxQueue, RxBuffer, sizeRxBuffer);
	sysInitializeTimer (&P->TxNotifyTimer, (sys_CALLBACK)portTxNotifyTimer, P);
	sysInitializeTimer (&P->RxNotifyTimer, (sys_CALLBACK)portRxNotifyTimer, P);
	sysInitializeDpc (&P->RxRestartReadDpc, (sys_CALLBACK)portRxRestartRead, P);
	sysInitializeDpc (&P->TxRestartWriteDpc, (sys_CALLBACK)portTxRestartWrite, P);
	P->RxNotifyAny		= 0;
	P->RxRtsFlow            = 0;
	P->RxXoffFlow           = 0;

	P->TxFlow		= 0;
	P->TxOvershot		= 0;

	P->TxBuffer.hPages	= hTxPages;
	P->TxBuffer.Buffer	= TxBuffer;
	P->TxBuffer.Size	= sizeTxBuffer;
	queueInit (&P->TxQueue, TxBuffer, sizeTxBuffer);
	P->TxDeferTime		= 0;
	sysInitializeTimer (&P->TxDeferTimer, (sys_CALLBACK)portTxDeferTimer, P);
	P->sizeTxCompile	= 0;
	sysInitializeTimer (&P->TxCompileTimer, (sys_CALLBACK)portTxCompileTimer, P);

	/* initialize AT command processor data and initial state */

#if defined(UNIX)
	atInit (P, 1 /* NewPort HERE 2*/ , Address, Profile);
#else
	atInit (P, NewPort);
#endif

	D->Port = P;

 	P->State = State;

	PortDriver.curPorts++;


	return (P);

failure:
	if (hPort)    D->ContentionHandler (RELEASE_RESOURCE, hPort,
					    (dword) portStealCallback, 0, 0) ;
	if (RxBuffer) sysPageFree (hRxPages);
	if (TxBuffer) sysPageFree (hTxPages);
	if (!D->Port) sysPageFree (hPortPages);

    /*Memory free function */
	return (0);
}

static void _cdecl portWatchdog (void *context)
{
	ISDN_PORT_DESC	*D;
	ISDN_PORT	*P;
	unsigned long old_irql;

	if ( !PortDriver.Ready )
		return ;

	PortDriver.Ticks++;

	for (D = PortDriver.Ports; D < PortDriver.lastPort; D++) {

		if (!(P = D->Port)) continue;

		switch (P->State) {
		case P_CONNECTED:
		case P_DATA	:
		case P_ESCAPE	:
			if (P->At.RwTimeout &&
			    ((dword)(PortDriver.Ticks - P->LastRwTick)) > P->At.RwTimeout) {
				DBG_TRC(("[%p:%s] Watchdog: %d sec idle, drop",
				  P->Channel, P->Name,((dword)(PortDriver.Ticks - P->LastRwTick))*10))
				atDrop (P, CAUSE_SHUT);
				atRsp (P, R_NO_CARRIER);
			}
			break;

		case P_DIALING:
			/*
				Call establishement timeout
				*/
			if ((P->Mode == P_MODE_FAX) && (P->At.f.modem_options.s7 >= 10)) {
				int val = P->At.f.modem_options.s7/10;

				if (!P->LastAlertTick) {
					P->LastAlertTick = PortDriver.Ticks;
				} else if (((dword)(PortDriver.Ticks - P->LastAlertTick)) > val) {
					DBG_TRC(("[%p:%s] Outgoing call Watchdog: %d sec, drop",
					P->Channel, P->Name, ((dword)(PortDriver.Ticks - P->LastAlertTick))*10))
					atDrop (P, CAUSE_SHUT);
					atRsp (P, R_NO_CARRIER);
				}
      }
			break;

		case P_OFFERED	:
		case P_DISCONN	:
		case P_COMMAND	:
		case P_ANSWERED	:
			break;
		case P_IDLE	:
		default		:
			break;
		}
	}
	(void) sysStartTimer (&PortDriver.Watchdog, P_TO_TICK * 1000);
}

static ISDN_PORT_DESC *SavePort (byte *PortName, byte *Func)
{
	ISDN_PORT_DESC *D;

	if ( str_len ((char*)PortName) >= MAX_PORT_NAME ) {
		DBG_FTL(("%s: bad name '%s'", Func, PortName));
		return (0);
	}

	for ( D = PortDriver.Ports ; D < PortDriver.endPorts ; D++ )
	{
		if ( D >= PortDriver.lastPort ) {
			/* save port name in first free entry */
			str_cpy ((char*)D->Name, (char*)PortName);
	      		PortDriver.lastPort++;
			return (D);
		}
		if ( !str_i_cmp ((char*)D->Name, (char*)PortName) )
			return (D);	/* ignore dups */
	}
	return (0);
}

#if 0
static dword ParmKey = 0;
#endif

static void ParmOpen (void)
{
#if !defined(UNIX)
	ParmKey = sysRegOpen (0, ISDN_PORT_DRIVER_PARAMETERS_KEY);
#endif
	return;
}

static int ParmInt (byte *Name, int Default, int LowLimit)
{

#if defined(UNIX)
	return (Default);
#else
	byte Tmp[32]; int Value;
	if (sysRegString (ParmKey, Name, Tmp, sizeof (Tmp))) {
		Value = str_2_int (Tmp);
		if (Value < LowLimit) Value = Default;
	} else {
		Value = Default;
	}
	LOG_PRT(("%s = %d", Name, Value));
	return (Value);
#endif
}

static void ParmStr (byte *Name, byte *Default, byte *Value, dword sizeValue)
{
#if defined(WINNT)
	if (!sysRegString (ParmKey, Name, Value, sizeValue))
		str_cpy (Value, Default);
#endif /* WINNT */

#if defined(UNIX)
		str_cpy ((char*)Value, (char*)Default);
#endif
}

static void ParmClose (void)
{
#if !defined(UNIX)
	sysRegClose (ParmKey);
#endif
	return;
}

static int DiPort_Init_Complete (void)
{
	unsigned long old_irql;

# if LISTEN_SELECTIVELY
	/* Listen only if AT S0=0 is set for some port */
# else /* !LISTEN_SELECTIVELY */
	old_irql = eicon_splimp ();
	PortDriver.Isdn->Listen (1); /* Listen always */
	eicon_splx (old_irql);
# endif /* LISTEN_SELECTIVELY */

	PortDriver.Ready = 1;

	sysInitializeTimer (&PortDriver.Watchdog, portWatchdog, 0);
	old_irql = eicon_splimp ();
	(void) sysStartTimer (&PortDriver.Watchdog, P_TO_TICK * 1000);
	eicon_splx (old_irql);

	return VXD_SUCCESS;
}

int DiPort_Dynamic_Init (dword Dynamic, int NumChannels)
{
/*
 * Process SYS_DYNAMIC_DEVICE_INIT or DEVICE_INIT control message
 *
 * 'Dynamic' == 1 indicates SYS_DYNAMIC_DEVICE_INIT,
 * 'Dynamic' == 0 indicates DEVICE_INIT
 */
	static ISDN_INDICATIONS Indications = {
		portWant,
		portRecv,
		portStop,
		portGo,
		portRing,
		portName,
		portUp,
		portDown,
		portIdle
	};
#if defined(WINNT) || defined(UNIX)
	int	idiMaxCards, idiMaxChannels ;
	byte		PortName[MAX_PORT_NAME - 4 /*reserved for the number*/] ;
	int		NumPorts, FirstPort, i ;
#endif /* WINNT */
	static byte		ComPort[64], *pComPort;
	static byte		DescBuf[sizeof(DESCRIPTOR)*64];
	int			sizeDescBuf;

	/* Remember how we were loaded */

	PortDriver.Dynamic = (byte) Dynamic;

	ParmOpen ();

	PortDriver.Profile  = (byte) 5;
	PortDriver.RspDelay = (byte) ParmInt ((byte*)"RspDelay", 0, 0);
	PortDriver.Fixes    = (dword) ParmInt ((byte*)"Fixes",    0x12, 0);
	PortDriver.RingOn   = (word) ParmInt ((byte*)"RingOn",  2000, 500);
	PortDriver.RingOff  = (word) ParmInt ((byte*)"RingOff", 4000, 500);

	ParmStr ((byte*)"ComPort",   (byte*) "",
		   ComPort, sizeof (ComPort));
	ParmStr ((byte*)"DeviceName", (byte*)DiPort_DeviceName,
		  PortDriver.DeviceName, sizeof (PortDriver.DeviceName));
	ParmStr ((byte*)"ModemID",    (byte*)DiPort_ModemID,
		  PortDriver.ModemID, sizeof (PortDriver.ModemID));

#if defined(WINNT) || defined(UNIX)
	/* DeviceName was used as the basic port name, we prefer PortName now */
	ParmStr ((byte*)"PortName", PortDriver.DeviceName, PortName, sizeof (PortName));
	NumPorts = NumChannels;
	FirstPort =	 ParmInt ((byte*)"FirstPort",    1, 0) ;
	PortDriver.ReuseDelay =
			 ParmInt ((byte*)"ReuseDelay",   5, 1) ;
	PortDriver.CallbackTimeout =
			 ParmInt ((byte*)"CallbackTimeout",   60, 5) ;
	idiMaxCards =	 ParmInt ((byte*)"MaxCards",     8, 1) ;
	idiMaxChannels = ParmInt ((byte*)"MaxChannels",  2, 1) ;
#endif /* WINNT */

	ParmClose () ;


#if defined(WINNT) || defined(UNIX)
	/* Allocate a suitable port descriptor list */
	for ( pComPort = ComPort, i = NumPorts ; *pComPort ; pComPort++ )
	{
		i++ ;
		if ( !(pComPort = (byte*)(str_chr ((char*)pComPort, ','))) )
			break ;
	}
	i *= sizeof(ISDN_PORT_DESC) ;
	/* DBG_TRC(("MEM: alloc %s at %d", __FILE__, __LINE__)) */
	if ( ! (PortDriver.Ports = sysHeapAlloc (i)) )
	{
		DBG_TRC(("Init: out of memory (%d)", i))
		goto failure;
	}
	PortDriver.endPorts = PortDriver.Ports + i ;
#endif /* WINNT || UNIX*/
	/*
	 * Empty the port descriptor list and look for ports to be served.
	 * All ports are remembered in the port descriptor table.
	 * When VCOMM_Register_Port_Driver() calls PortInitialize() we do
	 * a VCOMM_Add_Port() for the requested port(s) to make the port(s)
	 * visible.
	 */

	PortDriver.lastPort = PortDriver.Ports;

	/*
	 * First look for COM ports specified in the 'ComPort' parameter.
	 * These ports are served only if we are loaded as a static driver.
	 * This is always the case in Win3x, but in Win95 it must be forced
	 * via a 'StaticVxD' registry entry which is no longer the default.
	 * The code is left here and not moved to the Win3x branch below
	 * to leave a chance for playing around with ports in Win95.
	 */
#if 0
	for (pComPort = ComPort; ; ) {
        byte *Name;

		for (Name = pComPort; *pComPort && *pComPort != ','; pComPort++)
			;
		if (pComPort > Name) {
			byte savC = *pComPort; *pComPort = '\0';
			SavePort ((byte*)Name, (byte*)"Init");
			*pComPort = savC;
		}
		if (!*pComPort++) break;
	}
#endif

#if defined(WINNT) || defined(UNIX)
	/*
	 * Create as many ports as specified by the NumPorts registry value.
	 * The port name will be built by appending a sequence number to the
	 * PortName registry value, the first number of sequence is defined
	 * by the FirstPort registry value.
	 * Eg. PortName = EiconPort, FirstPort = 10, NumPorts = 2 will create
	 * EiconPort10 and EiconPort11.
	 */
	for ( i = 0 ; i < NumPorts ; i++ )
	{
		sprintf ((byte*)ComPort, (byte*)"%s%d", PortName, FirstPort + i);
		SavePort ((byte*)ComPort, (byte*)"Init");
	}
#endif /* WINNT || UNIX*/

	/* look if we have any port to serve */

	if (!PortDriver.Dynamic && (PortDriver.lastPort <= PortDriver.Ports)) {
		DBG_FTL(("Init: no port"));
		goto failure;
	}

	/*
 	 * Attach to DIDD, ask for all ISDN boards present, allocate the
 	 * necessary data structures and register with the IDI drivers.
	 */

	sizeDescBuf = sizeof (DescBuf);

	// VST: EtdM_DIDD_Read((DESCRIPTOR *)DescBuf, &sizeDescBuf);
	diva_os_didd_read ((DESCRIPTOR *)DescBuf, &sizeDescBuf);
	if ( !(PortDriver.Isdn = isdnBind (&Indications, DescBuf,(word)sizeDescBuf)))
	{
		DBG_ERR(("DIVA_TTY: BIND FAILURE"))
		printk (KERN_ERR "%s: BIND FAILURE=%p\n", __FUNCTION__, PortDriver.Isdn);
		goto failure;
	}

	if (PortDriver.Dynamic)
	{
		DiPort_Init_Complete ();
	}

	return VXD_SUCCESS;

failure:
#if defined(WINNT) || defined(UNIX)
	if ( PortDriver.Ports ) sysHeapFree (PortDriver.Ports) ;
#endif /* WINNT || UNIX */
 	return VXD_FAILURE;
}

int DiPort_Dynamic_Exit (void)
{
	unsigned long old_irql;
	int Ret;

	PortDriver.Ready = 0;
	old_irql = eicon_splimp ();
	sysCancelTimer (&PortDriver.Watchdog);
	eicon_splx (old_irql);

	Ret = VXD_FAILURE;

	if (PortDriver.curPorts)
    {
		DBG_FTL(("Exit: ports busy"));
	} 
	else if (PortDriver.Isdn && (PortDriver.Isdn = isdnBind(0, 0, 0)))
    {
		DBG_FTL(("Exit: isdn busy"));
	} 
	else
    {
		if ( PortDriver.Ports ) sysHeapFree (PortDriver.Ports);
		Ret = VXD_SUCCESS;
	}


	return (Ret);
}
