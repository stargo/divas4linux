
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

#include "port.h"

#define byte unsigned char
#define word unsigned short
#define dword unsigned int


/* device commands */
#define	T_OUTPUT	0
#define	T_TIME		1
#define	T_SUSPEND	2
#define	T_RESUME	3
#define	T_BLOCK		4
#define	T_UNBLOCK	5
#define	T_RFLUSH	6
#define	T_WFLUSH	7
#define	T_BREAK		8
#define	T_INPUT		9
#define T_DISCONNECT	10
#define	T_PARM		11
#define	T_SWTCH		12


/* Internal state */
#define	TIMEOUT	01		/* Delay timeout in progress */
#define	WOPEN	02		/* Waiting for open to complete */
#define	ISOPEN	04		/* Device is open */
#define	TBLOCK	010
#define	CARR_ON	020		/* Software copy of carrier-present */
#define	BUSY	040		/* Output in progress */
#define	OASLP	0100		/* Wakeup when output done */
#define	IASLP	0200		/* Wakeup when input done */
#define	TTSTOP	0400		/* Output stopped by ctl-s */
#define	DIVA_TTY_EXTPROC	01000		/* External processing */
#define	TACT	02000
#define	CLESC	04000		/* Last char escape */
#define	RTO	010000		/* Raw Timeout */
#define	TTIOW	020000
#define	TTXON	040000
#define	TTXOFF	0100000

/* Definitions of device states */
enum device_states {
	DEV_OPENING,
	DEV_OPEN,
	DEV_CLOSED
};

/* Definitions of device states */
enum lock_states {
	DEV_UNLOCKED,
	DEV_LOCKED
};

/* Global structure for Adapters..*/
typedef struct ser_adapter_s {
  byte      type;
  byte      channels;
  word      features;
  void      *request;  //IDI_CALL
  int       length;
  int 		count;
 } ser_adapter_t;

#include "frm.h"

#define DIVA_FAST_RBUFFER_SZ (3000)
  
typedef struct ser_dev_s {

	enum device_states dev_state;/* OPEN, CLOSED */
	enum lock_states lock_state; /* UNLOCKED, LOCKED */
	ulong dev_open_count; 		 /* Number of current opens on device */
	ulong dev_num; 		/* Clone device number */
	unsigned short tty_txcount;        /* Chars in transmit buffer */
	ISDN_PORT *P;		/* All necessary pointer to the ISDN Port struct */
	byte MsrShadow;		/* Shadow - Not used at moment */
	byte Acpt[ISDN_MAX_NUMBER];	/* Associated Number for Devices */
	struct tty_struct *ser_ttyp;
        int profile;

	struct {
		char*  rx_base;
		char* rx_cur;
		int		rx_length;
		int 	xoff;
		int scheduled;

		unsigned char tx_collect[4096];
		int   f_length;
		int		f_limit;
	} remainder;
	diva_check_t check[4];
	int dcd_changed_nr;
	int idtr;
	int irts;
	int signal_dcd;
	int current_dcd_state;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	struct termios	normaltermios;
#else
	struct ktermios	normaltermios;
#endif

	byte rtmp  [DIVA_FAST_RBUFFER_SZ];

  struct sk_buff_head tx_q;
	atomic_t						tx_q_sz;
	int									tx_flow;
	int									tx_complete;
	struct tty_port tport;
} ser_dev_t;

#define DIVA_TTY_TX_Q_FC_LEVEL 1024
#define DIVA_TTY_TX_Q_FC_LEVEL_OFF 256

