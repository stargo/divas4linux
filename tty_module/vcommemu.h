
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

#ifndef VCOMMEMU__H
#define VCOMMEMU__H

/*#include "pshpack1.h" */

#define PORT_FUNCTION_TYPE_

/*typedef BOOL (__cdecl * PORT_FUNCTION)();*/
#if defined (BLAH)
typedef struct _PortFunctions {
	BOOL (__cdecl *pPortSetCommState)();	/* ptr to PortSetState */
	BOOL (__cdecl *pPortGetCommState)();	/* ptr to PortGetState */
	BOOL (__cdecl *pPortSetup)();		/* ptr to PortSetup */
	BOOL (__cdecl *pPortTransmitChar)();	/* ptr to PortTransmitChar */
	BOOL (__cdecl *pPortClose)();		/* ptr to PortClose */
	BOOL (__cdecl *pPortGetQueueStatus)();	/* ptr to PortGetQueueStatus */
	BOOL (__cdecl *pPortClearError)();	/* ptr to PortClearError */
	BOOL (__cdecl *pPortSetModemStatusShadow)(); /* ptr to
					        PortSetModemStatusShadow */
	BOOL (__cdecl *pPortGetProperties)();	/* ptr to PortGetProperties */
	BOOL (__cdecl *pPortEscapeFunction)();	/* ptr to PortEscapeFunction */
	BOOL (__cdecl *pPortPurge)();		/* ptr to PortPurge */
	BOOL (__cdecl *pPortSetEventMask)();	/* ptr to PortSetEventMask */
	BOOL (__cdecl *pPortGetEventMask)();	/* ptr to PortGetEventMask */
	BOOL (__cdecl *pPortWrite)();		/* ptr to PortWrite */
	BOOL (__cdecl *pPortRead)();		/* ptr to PortRead */
	BOOL (__cdecl *pPortEnableNotification)(); /* ptr to PortEnableNotification */
	BOOL (__cdecl *pPortSetReadCallBack)(); /* ptr to PortSetReadCallBack */
	BOOL (__cdecl *pPortSetWriteCallBack)(); /* ptr to PortSetWriteCallBack */
	BOOL (__cdecl *pPortGetModemStatus)();	/* ptr to PortGetModemStatus */

	BOOL (__cdecl *pPortGetCommConfig)();	/* ptr to PortGetCommConfig */
	BOOL (__cdecl *pPortSetCommConfig)();	/* ptr to PortSetCommConfig */
	BOOL (__cdecl *pPortGetError)();	/* ptr to PortGetError (win32 style)*/
	BOOL (__cdecl *pPortDeviceIOCtl)();	/* ptr to PortDeviceIOCtl */
		
} PortFunctions;
#endif

typedef struct COMMTIMEOUTS {
	dword ReadIntervalTimeout;
	dword ReadTotalTimeoutMultiplier;
	dword ReadTotalTimeoutConstant;
	dword WriteTotalTimeoutMultiplier;
	dword WriteTotalTimeoutConstant;
} COMMTIMEOUTS, *LPCOMMTIMEOUTS;

typedef struct _PortData {
        word PDLength;			/* sizeof (PortData) */
        word PDVersion;			/* version of struct */
        /*PortFunctions *PDfunctions;*/	/* Points to a list of functions
					   supported by	the port driver */
        dword PDNumFunctions;		/* Highest ordinal of supported
					   function */
        dword dwLastError;		/* what was the error
					   for the last operation */
        dword dwClientEventMask;	/* event mask set by client */
        dword lpClientEventNotify;	/* address set by client for
					   event notification */
	dword lpClientReadNotify;	/* adress set by client for
					   read threshold notification */
	dword lpClientWriteNotify;	/* address set by client for
					   write threshold notification */
	dword ClientRefData;		/* Client's reference data */

	dword dwWin31Req;		/* Used for WIN3.1 specific reasons */
	dword dwClientEvent;		/* Event to send to client */
	dword dwCallerVMId;		/* Used for supporting all VMs */

	dword dwDetectedEvents;		/* mask of detected and
					   enabled events */
    dword dwCommError;		/* non-zero if I/O error. */
	byte bMSRShadow;		/* the shadow of Modem Status
					   Register */
    word wFlags;			/* flags for the port */
	byte LossByte;			/* For COMM ports 1-4 VCD flags this
					   byte telling port driver that it
					   has lost the port */

	byte* QInAddr;			/* Address of the queue */
    dword QInSize;
    byte* QOutAddr;			/* Address of the queue */
    dword QOutSize;			/* Length of queue in bytes */
	dword QInCount;			/* # of bytes currently in queue */
	dword QInGet;			/* Offset into q to get bytes from */
	dword QInPut;			/* Offset into q to put bytes in */
	dword QOutCount;		/* Number of bytes currently in q */
	dword QOutGet;			/* Offset into q to get bytes from */
	dword QOutPut;			/* Offset into q to put bytes in */

	dword ValidPortData;		/* For checking validity etc. */

	dword lpLoadHandle;		/* load handle of the owner VxD */
	COMMTIMEOUTS cmto;		/* Commtimeouts struct */

	dword lpReadRequestQueue;	/* Pointer to pending Read requests */
	dword lpWriteRequestQueue;	/* Pointer to pending Write requests */

	dword dwLastReceiveTime;	/* Time of last reception of data */
		
	dword dwReserved1;		/* Reserved */
	dword dwReserved2;		/* Reserved */
} PortData;

/* Standard error codes set in dwLastError. */

#define	IE_BADID	-1	 /* invalid or unsupported device */
#define	IE_OPEN		-2	 /* Device already open */
#define	IE_NOPEN	-3	 /* Device not open */
#define	IE_MEMORY	-4	 /* unable to allocate queues */
#define	IE_DEFAULT	-5	 /* error in default params */
#define	IE_INVALIDSERVICE -6	 /* port driver doesn't support this service*/
#define	IE_HARDWARE	-10	 /* hardware not present */
#define	IE_BYTESIZE	-11	 /* illegal byte size */
#define	IE_BAUDRATE	-12	 /* unsupported baud rate */
#define	IE_EXTINVALID	-20	 /* unsupported extended function */
#define	IE_INVALIDPARAM	-21	 /* Parameters are wrong */
#define	IE_TRANSMITCHARFAILED 0x4000 /* TransmitChar failed */

/* Events that can be set in dwClientEventMask */

#define	EV_RXCHAR	0x00000001	 /* Any Character received */
#define	EV_RXFLAG	0x00000002	 /* Received certain character */
#define	EV_TXEMPTY	0x00000004	 /* Transmitt Queue Empty */
#define	EV_CTS		0x00000008	 /* CTS changed state */
#define	EV_DSR		0x00000010	 /* DSR changed state */
#define	EV_RLSD		0x00000020	 /* RLSD changed state */
#define	EV_BREAK	0x00000040	 /* BREAK received */
#define	EV_ERR		0x00000080	 /* Line status error occurred */
#define	EV_RING		0x00000100	 /* Ring signal detected */
#define	EV_PERR		0x00000200	 /* Printer error occured */
#define	EV_CTSS		0x00000400	 /* CTS state */
#define	EV_DSRS		0x00000800	 /* DSR state */
#define	EV_RLSDS	0x00001000	 /* RLSD state */
#define	EV_RingTe	0x00002000	 /* Ring Trailing Edge Indicator */
#define	EV_TXCHAR	0x00004000	 /* Any character transmitted */
#define	EV_DRIVER	0x00008000	 /* Driver specific event */
#define	EV_UNAVAIL	0x00010000	 /* Acquired port has been stolen */
#define	EV_AVAIL	0x00020000	 /* stolen port has been released */


/* Error Flags for dwCommError */

#define	CE_RXOVER	0x00000001		/* Receive Queue overflow */
#define	CE_OVERRUN	0x00000002		/* Receive Overrun Error */
#define	CE_RXPARITY	0x00000004		/* Receive Parity Error */
#define	CE_FRAME	0x00000008		/* Receive Framing error */
#define	CE_BREAK	0x00000010		/* Break Detected */
#define	CE_CTSTO	0x00000020		/* CTS Timeout */
#define	CE_DSRTO	0x00000040		/* DSR Timeout */
#define	CE_RLSDTO	0x00000080		/* RLSD Timeout */
#define	CE_TXFULL	0x00000100		/* TX Queue is full */
#define	CE_PTO		0x00000200		/* LPTx Timeout */
#define	CE_IOE		0x00000400		/* LPTx I/O Error */
#define	CE_DNS		0x00000800		/* LPTx Device not selected */
#define	CE_OOP		0x00001000		/* LPTx Out-Of-Paper */
#define	CE_Unused1	0x00002000		/* unused */
#define	CE_Unused2	0x00004000		/* unused */
#define	CE_MODE		0x00008000		/* Requested mode unsupported*/

//
// Modem status flags
//

#define	MS_CTS_ON	0x0010
#define	MS_DSR_ON	0x0020
#define	MS_RING_ON	0x0040
#define	MS_RLSD_ON	0x0080
#define	MS_Modem_Status	0x00F0


/* flags for wFlags of PortData */

#define	Event_Sent	0x0001	/* has an event been set for COMM.DRV ? */
#define	Event_Sent_Bit	0x0000	/* bit in the flag word */
#define	TimeOut_Error	0x0002	/* Operation was abandoned due to timeout */
#define	TimeOut_Error_Bit 0x0001 /* its bit field */
#define	Flush_In_Progress 0x0004 /* FlushFileBuffers is in progress */
#define	Flush_In_Progress_Bit 0x0002 /* its bit */
#define	TxQueuesSet	0x0008	/* Non-zero xmit queue exists */
#define	TxQueuesSetBit	0x0003	/* its bit */
#define	CloseComm_In_Progress	0x0010	/* CloseComm is in progress */
#define	CloseComm_In_Progress_Bit 0x0004 /* its bit */
#define Spec_Timeouts_Set	0x0020	/* Special timeouts have been set */
#define	Spec_Timeouts_Set_Bit	0x0005	/* its bit */

typedef struct _DCB {
	dword DCBLength;		/* sizeof (DCB) */
	dword BaudRate ;		/* Baudrate at which running */
        dword BitMask;			/* flag dword */
        dword XonLim;			/* Transmit X-ON threshold */
        dword XoffLim;			/* Transmit X-OFF threshold */
        word wReserved;			/* reserved */
        byte ByteSize;			/* Number of bits/byte, 4-8 */
	byte Parity;			/* 0-4=None,Odd,Even,Mark,Space */
	byte StopBits;			/* 0,1,2 = 1, 1.5, 2 */
	char XonChar;			/* Tx and Rx X-ON character */
	char XoffChar;			/* Tx and Rx X-OFF character */
	char ErrorChar;			/* Parity error replacement char */
	char EofChar;			/* End of Input character */
	char EvtChar1;			/* special event character */
	char EvtChar2;			/* Another special event character */
	byte bReserved;			/* reserved */
	dword RlsTimeout;		/* Timeout for RLSD to be set */
	dword CtsTimeout;		/* Timeout for CTS to be set */
	dword DsrTimeout;		/* Timeout for DSR to be set */
	dword TxDelay;			/* Amount of time between chars */
} _DCB;

/*  Comm Baud Rate indices : allowable values for BaudRate */

#define	CBR_110		0x0000FF10
#define	CBR_300		0x0000FF11
#define	CBR_600		0x0000FF12
#define	CBR_1200	0x0000FF13
#define	CBR_2400	0x0000FF14
#define	CBR_4800	0x0000FF15
#define	CBR_9600	0x0000FF16
#define	CBR_14400	0x0000FF17
#define	CBR_19200	0x0000FF18

/* 0x0000FF19,0x00000FF1A are reserved */

#define	CBR_38400	0x0000FF1B

/* 0x0000FF1C, 0x0000FF1D, 0x0000FF1E are reserved */

#define	CBR_56000	0x0000FF1F

/* 0x0000FF20, 0x0000FF21, 0x0000FF22 are reserved */

#define	CBR_128000	0x0000FF23

/* 0x0000FF24, 0x0000FF25, 0x0000FF26 are reserved */

#define	CBR_256000	0x0000FF27

/* Flags for Bitmask */

#define	fBinary		0x00000001	/* Binary mode */
#define	fRTSDisable	0x00000002	/* Disable RTS */
#define fParity		0x00000004	/* Perform parity checking */
#define	fOutXCTSFlow	0x00000008	/* Output handshaking using CTS */
#define	fOutXDSRFlow	0x00000010	/* Output handshaking using DSR */
#define	fEnqAck		0x00000020	/* ENQ/ACK software handshaking */
#define	fEtxAck		0x00000040	/* ETX/ACK software handshaking */
#define	fDTRDisable	0x00000080	/* Disable DTR */

#define	fOutX		0x00000100	/* Output X-ON/X-OFF */
#define	fInX		0x00000200	/* Input X-ON/X-OFF */
#define	fPErrChar	0x00000400	/* Parity error replacement active */
#define	fNullStrip	0x00000800	/* Null stripping */
#define	fCharEvent	0x00001000	/* Character event */
#define	fDTRFlow	0x00002000	/* Input handshaking using DTR */
#define	fRTSFlow	0x00004000	/* Output handshaking using RTS */
#define	fWin30Compat	0x00008000	/* Maintain Compatiblity */

#define	fDsrSensitivity	0x00010000	/* DSR sensitivity */
#define	fTxContinueOnXoff 0x00020000	/* Continue Tx when Xoff sent */
#define	fDtrEnable	0x00040000	/* Enable DTR on device open */
#define	fAbortOnError	0x00080000	/*abort all reads and writes on error*/
#define	fRTSEnable	0x00100000	/* enable RTS on device open */
#define	fRTSToggle	0x00200000	/* iff bytes in Q, set RTS high */

/* Allowable parity values */

#define	NOPARITY	0
#define	ODDPARITY	1
#define	EVENPARITY	2
#define	MARKPARITY	3
#define	SPACEPARITY	4

/* Allowable stopbits */

#define	ONESTOPBIT	0
#define	ONE5STOPBITS	1
#define	TWOSTOPBITS	2

typedef	unsigned short WCHAR;

typedef struct _COMMPROP {
	word   wPacketLength;		/* length of property structure
					   in bytes */
	word   wPacketVersion;		/* version of this structure */
	dword  dwServiceMask;		/* Bitmask indicating services
					   provided */
	dword  dwReserved1;		/* reserved */
	dword  dwMaxTxQueue;		/* Max transmit queue size.
					   0 => not used */
	dword  dwMaxRxQueue;		/* Max receive queue size.
					   0 => not used */
	dword  dwMaxBaud;		/* maximum baud supported */
	dword  dwProvSubType;		/* specific COMM provider type */
	dword  dwProvCapabilities;	/* flow control capabilities */
	dword  dwSettableParams;	/* Bitmask indicating params
					   that can be set. */
	dword  dwSettableBaud;		/* Bitmask indicating baud rates
					   that can be set*/
	word   wSettableData;		/* bitmask indicating # of data bits
					   that can be set*/
	word   wSettableStopParity;	/* bitmask indicating allowed
					   stopbits and parity checking */
	dword  dwCurrentTxQueue;	/* Current size of transmit queue
					   0 => unavailable */
	dword  dwCurrentRxQueue;	/* Current size of receive queue
					   0 => unavailable */
	dword  dwProvSpec1;		/* Used iff clients have
					   intimate knowledge of format */
	dword  dwProvSpec2;		/* Used iff clients have intimate
					   knowledge of format */
	/*wchar  wcProvChar[1];		*//* Used iff clients have intimate \						   knowledge of format */
    byte   wcProvChar[1];
	word	filler;			/* To make it multiple of 4 */
	
} _COMMPROP;

//
// Serial provider type.
//

#define SP_SERIALCOMM    ((dword)0x00000001)

//
// Provider SubTypes
//

#define PST_UNSPECIFIED      ((dword)0x00000000)
#define PST_RS232            ((dword)0x00000001)
#define PST_PARALLELPORT     ((dword)0x00000002)
#define PST_RS422            ((dword)0x00000003)
#define PST_RS423            ((dword)0x00000004)
#define PST_RS449            ((dword)0x00000005)
#define PST_MODEM            ((dword)0x00000006)
#define PST_FAX              ((dword)0x00000021)
#define PST_SCANNER          ((dword)0x00000022)
#define PST_NETWORK_BRIDGE   ((dword)0x00000100)
#define PST_LAT              ((dword)0x00000101)
#define PST_TCPIP_TELNET     ((dword)0x00000102)
#define PST_X25              ((dword)0x00000103)

//
// Provider capabilities flags.
//

#define PCF_DTRDSR        ((dword)0x0001)
#define PCF_RTSCTS        ((dword)0x0002)
#define PCF_RLSD          ((dword)0x0004)
#define PCF_PARITY_CHECK  ((dword)0x0008)
#define PCF_XONXOFF       ((dword)0x0010)
#define PCF_SETXCHAR      ((dword)0x0020)
#define PCF_TOTALTIMEOUTS ((dword)0x0040)
#define PCF_INTTIMEOUTS   ((dword)0x0080)
#define PCF_SPECIALCHARS  ((dword)0x0100)
#define PCF_16BITMODE     ((dword)0x0200)

//
// Comm provider settable parameters.
//

#define SP_PARITY         ((dword)0x0001)
#define SP_BAUD           ((dword)0x0002)
#define SP_DATABITS       ((dword)0x0004)
#define SP_STOPBITS       ((dword)0x0008)
#define SP_HANDSHAKING    ((dword)0x0010)
#define SP_PARITY_CHECK   ((dword)0x0020)
#define SP_RLSD           ((dword)0x0040)

//
// Settable baud rates in the provider.
//

#define BAUD_075          ((dword)0x00000001)
#define BAUD_110          ((dword)0x00000002)
#define BAUD_134_5        ((dword)0x00000004)
#define BAUD_150          ((dword)0x00000008)
#define BAUD_300          ((dword)0x00000010)
#define BAUD_600          ((dword)0x00000020)
#define BAUD_1200         ((dword)0x00000040)
#define BAUD_1800         ((dword)0x00000080)
#define BAUD_2400         ((dword)0x00000100)
#define BAUD_4800         ((dword)0x00000200)
#define BAUD_7200         ((dword)0x00000400)
#define BAUD_9600         ((dword)0x00000800)
#define BAUD_14400        ((dword)0x00001000)
#define BAUD_19200        ((dword)0x00002000)
#define BAUD_38400        ((dword)0x00004000)
#define BAUD_56K          ((dword)0x00008000)
#define BAUD_128K         ((dword)0x00010000)
#define BAUD_USER         ((dword)0x10000000)

//
// Settable Data Bits
//

#define DATABITS_5        ((word)0x0001)
#define DATABITS_6        ((word)0x0002)
#define DATABITS_7        ((word)0x0004)
#define DATABITS_8        ((word)0x0008)
#define DATABITS_16       ((word)0x0010)
#define DATABITS_16X      ((word)0x0020)

//
// Settable Stop and Parity bits.
//

#define STOPBITS_10       ((word)0x0001)
#define STOPBITS_15       ((word)0x0002)
#define STOPBITS_20       ((word)0x0004)
#define PARITY_NONE       ((word)0x0100)
#define PARITY_ODD        ((word)0x0200)
#define PARITY_EVEN       ((word)0x0400)
#define PARITY_MARK       ((word)0x0800)
#define PARITY_SPACE      ((word)0x1000)

typedef struct  _COMSTAT {
	dword BitMask;		/* flags dword */
	dword cbInque;		/* Count of characters in receive queue */
	dword cbOutque;		/* Count of characters in transmit queue */
} _COMSTAT;

/* Flags of BitMask */

#define	fCtsHold	0x00000001	/* Transmit is on CTS hold */
#define	fDsrHold	0x00000002	/* Transmit is on DSR hold */
#define	fRlsdHold	0x00000004	/* Transmit is on RLSD hold */
#define	fXoffHold	0x00000008	/* Received handshake */
#define	fXoffSent	0x00000010	/* Issued handshake */
#define	fEof		0x00000020	/* EOF character found */
#define	fTximmed	0x00000040	/* Character being transmitted */


/* Escape Functions (extended functions). */

#define	Dummy	0		  /* Dummy */
#define	SETXOFF	1		  /* Simulate XOFF received */
#define	SETXON  2		  /* Simulate XON received */
#define	SETRTS	3		  /* Set RTS high */
#define	CLRRTS	4		  /* Set RTS low */
#define	SETDTR	5		  /* Set DTR high */
#define	CLRDTR	6		  /* Set DTR low */
#define	RESETDEV 7		  /* Reset device if possible */

/* These numbers are reserved for compatibility reasons */

#define	GETLPTMAX 8		 /* Get maximum LPT supported */
#define	GETCOMMAX 9		 /* Get maximum COM supported */

#define	GETCOMBASEIRQ	10	 /* Get COM base and IRQ */
#define	GETCOMBASEIRQ1	11	 /* FOR COMPATIBILITY REASONS */

#define	SETBREAK	12	 /* Set break condition */
#define	CLEARBREAK	13	 /* Clear break condition */

/* These too are not available to client VxDs and not implemented by
   port drivers. */

#define	GETPORTHANDLE	14	 /* Get handle for Port */
#define	GETEXTENDEDFNADDR 15	 /* Get the address of extended functions */

#define	CLRTIMERLOGIC	16	/* Clear the timer logic of the port driver */

#define GETDEVICEID	17	/* Get the device ID of the device */
#define SETECPADDRESS	18	/* Set ECP channel address */

#define SETUPDATETIMEADDR	19	/* Set address of update field for
					   last time a char was received */

#define IGNOREERRORONREADS	20	/* Ignore pending IO errors on reads*/

#define ENABLETIMERLOGIC	21	/* Re-enable timer logic code */
#define	IGNORESELECT		22	/* Ignore select bit */

#define STARTNONSTDESCAPES	200	/* non standard escapes */

#define	PEEKCHAR		200	/* peek the Rx Q */

/* END OF ESCAPES for ESCAPECOMMFUNCTION */

/* notifications passed in Event of Notification function */

#define	CN_RECEIVE	1	 /* bytes are available in the input queue */
#define	CN_TRANSMIT	2	 /* fewer than wOutTrigger bytes still */
				 /* remain in the output queue waiting */
				 /* to be transmitted. */
#define	CN_EVENT	4	 /* an enabled event has occurred */

/* Masks for relevant fields of DCB */
#define	fBaudRate	0x00000001
#define	fBitMask	0x00000002
#define	fXonLim		0x00000004
#define	fXoffLim	0x00000008
#define	fByteSize	0x00000010
#define	fbParity	0x00000020
#define	fStopBits	0x00000040
#define	fXonChar	0x00000080
#define	fXoffChar	0x00000100
#define	fErrorChar	0x00000200
#define	fEofChar	0x00000400
#define	fEvtChar1	0x00000800
#define	fEvtChar2	0x00001000
#define	fRlsTimeout	0x00002000
#define	fCtsTimeout	0x00004000
#define	fDsrTimeout	0x00008000
#define	fTxDelay	0x00010000
#define	fTimeout	(fRlsTimeout | fDsrTimeout | fCtsTimeout)
#define	fLCR		0x00000070

/*#include "poppack.h"*/
				
/*
 * Contention related equates
 */

#define	MAP_DEVICE_TO_RESOURCE	0
#define	ACQUIRE_RESOURCE	1
#define	STEAL_RESOURCE		2
#define	RELEASE_RESOURCE	3
#define	ADD_RESOURCE		4
#define	REMOVE_RESOURCE		5

#define	MAX_CONTEND_FUNCTIONS	5

#define VXD_SUCCESS 0
#define VXD_FAILURE 1

#define DC_Initialize   1

//
// VCOMM functions
#define VCOMM_Get_Contention_Handler(N) (NULL)
#define VCOMM_Map_Name_To_Resource(N) (NULL)

/*
 * VCOMM and VMM service interface wrappers
 */

word
Get_VMM_Version(
    void
    );


unsigned long	VCOMM_Register_Port_Driver (
			unsigned long PortInitFunc
			);
unsigned long	VCOMM_Add_Port (
			unsigned long RefData,
			unsigned long PortOpenFunc, char *PortName
			);
void		VCOMM_Acquire_Port (
			char *DeviceName, unsigned long PortId,
			unsigned long VMId, unsigned long MustBeZero,
			char *PortName
			);
void		VCOMM_Release_Port (
			unsigned long RefData, unsigned long VMId
			);
void		VCOMM_Map_Win32DCB_To_Ring0 (
			void *Win32DCB, void *Ring0DCB
			);
void		VCOMM_Map_Ring0DCB_To_Win32 (
			void *Ring0DCB, void *Win32DCB
			);
typedef int     (* VCOMM_ContentionHandler) (
			unsigned long Action,
			unsigned long ResourceHandle,
			unsigned long NotifyFunction,
			unsigned long NotifyContext,
			unsigned long Flags
			);

typedef int (_cdecl *PORT_INITFCT) (
	unsigned long Function,		/* always DC_Initialize for now		*/
	unsigned long DeviceNode,	/* zero for non P&P drivers		*/
	unsigned long RefData,		/* to be passed to VCOMM_Add_Port()	*/
	unsigned long Base,			/* IOBase address			*/
	unsigned long Irq,			/* Interrupt number			*/
	unsigned char  *Name );		/* zero for non P&P drivers		*/

typedef void * (_cdecl *PORT_OPENFCT) (
	unsigned char  *Name,	/* name of port to open	*/
	unsigned long VMId,		/* ID of originating VM	*/
	unsigned long *Error);	/* specific error code	*/

//
// Port driver entry points
//
/*int _stdcall DiPort_Dynamic_Init (unsigned long Dynamic);*/
/*int _stdcall DiPort_Dynamic_Exit (void);*/
/*int _stdcall DiPort_Init_Complete (void);*/

#endif /* VCOMMEMU__H */
