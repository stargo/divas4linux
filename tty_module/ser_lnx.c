
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

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

# include <linux/config.h>
# include <linux/module.h>
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
# include <linux/module.h>
# include <linux/mm.h>
# include <asm/string.h>
# include <asm/uaccess.h>
# include <asm/errno.h>
# include <asm/segment.h>
# include <linux/smp_lock.h>
# include <linux/skbuff.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#include <asm/spinlock.h>
static void local_bh_disable(void) { }
static void local_bh_enable(void) { }
#warning "define local_bd_enable (disable) for older kernel"
#ifndef set_current_state
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
	#define set_current_state(__x__) do{current->state = (__x__);}while(0)
#else
	/* Rel. commit "sched: Change task_struct::state" (Peter Zijlstra, Jun 11 2021) */
	#define set_current_state(__x__) do{WRITE_ONCE(current->__state, (__x__));}while(0)
#endif
#endif
#else
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#endif

#else /* } { */

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/delay.h>
#include <asm/types.h>
#include <asm/io.h>

#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <linux/string.h>

#endif /* } */


#undef N_DATA

# include "ser_ioctl.h"
# include "ser_lnx.h"
# include "kunix.h"
# include "platform.h"
# include "dimaint.h"
# include "di_defs.h"
# include "isdn_if.h"
# include "port.h"
# include "atp_if.h"
# include "sys_if.h"
# include "hdlc.h"
# include "v110.h"
# include "piafs.h"
# include "isdn.h"
# include "vcommemu.h"
# include "debuglib.h"
#include "divamemi.h"

#include "divasync.h"
#include "cfg_types.h"
#include "cfg_notify.h"



#if defined(DIVA_TTY_COUNT_ATOMIC)
#define DIVA_READ_TTY_COUNT(__x__)  atomic_read(&(__x__))
#else
#define DIVA_READ_TTY_COUNT(__x__)  (__x__)
#endif

#if !defined(__KERNEL_VERSION_GT_2_4__)
EXPORT_NO_SYMBOLS;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
#define init_MUTEX_LOCKED(x)	*(x)=MUTEX_LOCKED
#endif

#endif

#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static struct semaphore diva_tty_lock;
#else
static spinlock_t diva_tty_lock;
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,37)
#define init_MUTEX_LOCKED(__x__) sema_init((__x__), 0)
#endif

#else
#warning "Compilation for older kernel: ser_lnx.c"
#endif

int errno = 0;

typedef struct _diva_man_var_header {
  byte   escape;
  byte   length;
  byte   management_id;
  byte   type;
  byte   attribute;
  byte   status;
  byte   value_length;
  byte   path_length;
} diva_man_var_header_t;

typedef struct _diva_capi_mngt_work_item {
  char*  name;
  void*  dst;
  int    size;
} diva_capi_mngt_work_item_t;

typedef struct _diva_capi_mngt_work_entry {
  char* directory_name;
  diva_capi_mngt_work_item_t* work_items;
} diva_capi_mngt_work_entry_t;

typedef struct _diva_capi_mngt_ctxt {
  ENTITY  e;
  volatile int     state;
  BUFFERS XData;
  BUFFERS RData;
  byte    xbuffer[2048+1];
  DESCRIPTOR* d;

  int current_entry;
  diva_capi_mngt_work_entry_t** work_entries;
  int pending_msg;
} diva_capi_mngt_ctxt_t;

typedef void (*EtdM_DIDD_Read_t)(DESCRIPTOR *, int *);
typedef void (*DIVA_DIDD_Read_t)(DESCRIPTOR *, int);
static EtdM_DIDD_Read_t EtdM_DIDD_Read_fn = 0;
DIVA_DIDD_Read_t DIVA_DIDD_Read_fn = 0;
static int __tty_isdn_write (struct tty_struct *tty, char *tx_buffer, int count);
static int diva_tty_read_channel_count (DESCRIPTOR* d, int* VsCAPI);

int diva_mtpx_adapter_detected = 0;
int diva_wide_id_detected      = 0;
atomic_t dpc_init_complete;
extern atomic_t divas_timer_running;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#undef memset
#undef bcopy
#define bcopy(__x__, __y__, __z__) memcpy(__y__,__x__,__z__)
#endif

#define NTTYDEVS 256
#define ETSER_CTRL_DEV 0


#if defined(DIVA_SMP)
	#if defined(DIVA_USES_MUTEX)
		#define DIVA_VERSION_EXTEND "SMP-PMT"
	#else
		#define DIVA_VERSION_EXTEND "SMP"
	#endif
#else
	#if defined(DIVA_USES_MUTEX)
		#define DIVA_VERSION_EXTEND "UNI-PMT"
	#else
		#define DIVA_VERSION_EXTEND "UNI"
	#endif
#endif

#define	DIVA_BUILD_STRING "BUILD ("DIVA_BUILD")-"DIVA_VERSION_EXTEND

#if !defined(__KERNEL_VERSION_GT_2_4__)
#define MAX_DIVA_TTY_MAJORS 2
struct tty_driver eicon_tty_driver[MAX_DIVA_TTY_MAJORS];
static char eicon_driver_names[MAX_DIVA_TTY_MAJORS][32];
#else /* } { */

#define MAX_DIVA_TTY_MAJORS 1
static struct tty_driver* eicon_tty_driver = 0;
static char	*diva_tty_drvname = "Divatty";


#endif /* } */

#if defined(MODULE)
#define eicon_tty_isdn_init init_module
#endif

#define FIRST_MAJOR      (TTY_MAJOR+4)
#define MINORS_PER_MAJOR_DEFAULT 255
int minors_per_major = MINORS_PER_MAJOR_DEFAULT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
static struct tty_port dsport[MINORS_PER_MAJOR_DEFAULT+1];
#endif

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

#define DIVA_MINOR(__x__) (MINOR(((__x__)->device)) + ((MAJOR(((__x__)->device))-FIRST_MAJOR)*minors_per_major))

#else /* } { */

#define DIVA_MINOR(__x__) ((__x__)->index)

#endif /* } */


static int driver_registered[MAX_DIVA_TTY_MAJORS];


/*
	GLOBAL MODULE OPTIONS
	*/
int global_options = 0;
int minors_per_major_init = 0;
char* diva_tty_init = 0;
char diva_tty_init_prm[64];

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
MODULE_PARM(global_options, "i");
MODULE_PARM(minors_per_major_init, "i");
MODULE_PARM(diva_tty_init, "s");
#else /* } { */
#include <linux/moduleparam.h>
module_param(global_options, uint, 0);
module_param(minors_per_major_init, uint, 0);
module_param_string(diva_tty_init, diva_tty_init_prm, sizeof(diva_tty_init_prm), 0);
#endif /* } */

#ifdef MODULE_LICENSE
#if (DIVA_GPL==1)
MODULE_LICENSE("GPL");
#else
MODULE_LICENSE("Eicon Networks");
#endif
#endif


/*
  Global options
*/

int divas_tty_debug_tty_data = 0;


/*
	IMPORTS
	*/
extern void disconnect_didd(void);
extern void do_adapter_shutdown (void);
extern ISDN_MACHINE Machine;
extern ISDN_PORT_DRIVER PortDriver;
extern void create_status_message (ISDN_PORT *P, char* out, int limit);
extern void diva_tty_start_dpc(void);
extern int diva_start_management (int logical_adapter_nr);
extern void diva_stop_management (void);
extern void diva_get_extended_adapter_features (IDI_CALL request);
extern byte ports_unavailable_cause;
extern IDI_CALL diva_get_dadapter_request(void);
extern int divas_tty_debug_sig;
extern int divas_tty_debug_net;
extern int divas_tty_debug_net_data;
extern int divas_tty_wait_sig_disc;

/*
	LOCALS
	*/
static int initialised = 0;
size_t memsize = 0;
int Adapter_Count = 0;
int Channel_Count = 0;
word Desc_Length = 0;
char *mem_ptr = NULL;
ser_dev_t *ser_ctrl = NULL; //Pointer to control device
ser_dev_t *ser_devs = NULL;
static ser_adapter_t *ser_adapter = NULL;
DESCRIPTOR d[140]; //MAX_DESCRIPTORS
DESCRIPTOR flat_descriptor[140];
word flat_descriptor_length = 0;
#if !defined(__KERNEL_VERSION_GT_2_4__)
static int eicon_refcount[MAX_DIVA_TTY_MAJORS];
#endif
static void PortReadReady(void *P, void* RefData, long Type, long Zero);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
static int tty_isdn_chars_in_buffer(struct tty_struct *tty);
#else
static unsigned int tty_isdn_chars_in_buffer(struct tty_struct *tty);
#endif
int PortDoCollectAsync (ISDN_PORT* P);
extern ISDN_PORT_DRIVER PortDriver;
static void diva_setsignals (ser_dev_t* d, int dtr, int rts);
static int process_control_tty_command (char* cmd, char* out, int limit);
static void diva_tty_channel_clear_write_q(register ser_dev_t* sd);
static int diva_tty_read_cfg_value (IDI_SYNC_REQ* sync_req,
                                     int instance,
                                     const char* name,
                                     diva_cfg_lib_value_type_t value_type,
                                     void* data,
                                     dword data_length);
static void diva_tty_read_global_option(IDI_SYNC_REQ* sync_req, const char* name, int mask);
static void diva_tty_read_global_port_option(IDI_SYNC_REQ* sync_req, const char* name, int mask);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
void* memset(void * s, int c, size_t count) {
 char *xs = (char *) s;

 while (count--)
	*xs++ = c;

	return (s);
}
#endif

/*
 *	Define a local default termios struct. All ports will be created
 *	with this termios initially. Basically all it defines is a raw port
 *	at 9600, 8 data bits, 1 stop bit.
 */
static
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
struct termios
#else
struct ktermios
#endif
diva_deftermios = {
	0,
	0,
	(B9600 | CS8 | CREAD | HUPCL | CLOCAL),
	0,
	0,
	INIT_C_CC
};


extern ISDN_PORT * PortOpen( byte *Name, dword *Error, byte *Address,int Profile);
extern int _cdecl PortClose(ISDN_PORT *P);
extern int _cdecl PortRead(ISDN_PORT *,byte *,dword, dword *);
extern int _cdecl PortWrite(ISDN_PORT *,byte *,dword, dword *);
extern int _cdecl PortSetWriteCallBack(ISDN_PORT *P, dword TxTrigger,
																			 void (*TxNotifyProc)(void*, void*, long, long),
																			 void* ReferenceData);
extern int _cdecl PortSetReadCallBack (ISDN_PORT *P, dword RxTrigger,
																			 void (*RxNotifyProc)(void*, void*, long, long),
																			 void* ReferenceData);
extern int _cdecl PortSetModemStatusShadow (ISDN_PORT *, byte *);
extern int _cdecl PortReadSize(ISDN_PORT *);
extern int DiPort_Dynamic_Init (dword, int);
extern int DiPort_Dynamic_Exit (void);
static void diva_set_port_notification (ser_dev_t *sd);
extern void atClearCon (ISDN_PORT *P);
extern int _cdecl PortEscapeFunction ( ISDN_PORT *P, long lFunc, long InData, dword *OutData);

extern int _cdecl PortEnableNotification (ISDN_PORT *P,
													void (*EvNotifyProc)(void*, void*, long, long),
															void* ReferenceData);
int _cdecl PortSetEventMask (ISDN_PORT *P, dword dwMask, dword *pEvents);
static void diva_EvNotifyProc (void*, void* , long, long);
extern int _cdecl PortPurge (ISDN_PORT *P, dword RecQueueFlag);
extern int atHangup (ISDN_PORT *P, byte **Arg);
extern int atReset (ISDN_PORT *P, byte **Arg);
extern void atRsp (ISDN_PORT *P, int Rc);

#if !defined(__KERNEL_VERSION_GT_2_4__)
static struct tty_struct *eicon_table          [MAX_DIVA_TTY_MAJORS][NTTYDEVS];
static struct termios    *eicon_termios        [MAX_DIVA_TTY_MAJORS][NTTYDEVS];
static struct termios    *eicon_termios_locked [MAX_DIVA_TTY_MAJORS][NTTYDEVS];
static char *tty_isdnname[MAX_DIVA_TTY_MAJORS] = { "Divatty0", "Divatty1" };
#endif

int tty_isdn_read(int dev_num) {
	ser_dev_t *sd;
	struct tty_struct *tty;
	dword	dwReadSize=0, sizeGiven = 0;
	int room;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	struct tty_port *port;
#endif

	if (ser_devs[dev_num].dev_state != DEV_OPEN) {
		return (0);
	}
	sd = &ser_devs[dev_num];
	if ((!(tty = sd->ser_ttyp)) || !sd->P || tty->closing) {
		return (0);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	if (!(port = tty->port)) {
		return (0);
	}
#endif


	while ((dwReadSize = PortReadSize(sd->P)) && (!sd->remainder.xoff)) {
#if   LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
		static byte rflag[DIVA_FAST_RBUFFER_SZ];

		room = tty->ldisc.receive_room(tty);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		static byte rflag[DIVA_FAST_RBUFFER_SZ];

		room = tty->receive_room;
#else
		room = tty_buffer_request_room(port, dwReadSize);
#endif
		if (room <= 0) {
			break;
		}
		dwReadSize = MIN(dwReadSize, (dword)room);
		dwReadSize = MIN(dwReadSize, DIVA_FAST_RBUFFER_SZ);

		sizeGiven = 0;
		PortRead (sd->P, sd->rtmp, dwReadSize, &sizeGiven);
		if (!sizeGiven) {
			break;
		}
		if (divas_tty_debug_tty_data > 0) {
			byte* p = (byte*)sd->rtmp;
			dword length = sizeGiven;

			DBG_TRC(("[%d] read %d", dev_num, length))

			do {
				dword to_print = MIN(length, 256);
				DBG_BLK((p, to_print))
				p      += to_print;
				length -= to_print;
			} while (length > 0);
		}
#if   LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
		tty->ldisc.receive_buf (tty, sd->rtmp, rflag, sizeGiven);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
		tty->ldisc.ops->receive_buf (tty, sd->rtmp, rflag, sizeGiven);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
		tty->ldisc->ops->receive_buf (tty, sd->rtmp, rflag, sizeGiven);
#else
		if (sizeGiven >= 1) {
			tty_insert_flip_string(port, sd->rtmp, sizeGiven);
		}
		tty_flip_buffer_push(port);
#endif
	}

	return (0);
}

static void PortWriteReady(void *P, void* pRefData, long Type, long Zero ) {
	long RefData = (long)pRefData;

	if (!ser_devs[RefData].ser_ttyp) {
		return;
	}
	ser_devs[RefData].tx_complete = 1;
}

static void diva_tty_wakeup_write (ser_dev_t *sd) {
	struct tty_struct *tty;

	if ((sd->dev_state != DEV_OPEN) || (!(tty = sd->ser_ttyp)) || tty->closing || (!sd->P)) {
		return;
	}

#if   LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.write_wakeup) {
		(tty->ldisc.write_wakeup)(tty);
	}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc.ops->write_wakeup) {
		(tty->ldisc.ops->write_wakeup)(tty);
	}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) && tty->ldisc->ops && tty->ldisc->ops->write_wakeup) {
		(tty->ldisc->ops->write_wakeup)(tty);
	}
#else
	tty_wakeup(tty);
#endif
	wake_up_interruptible(&tty->write_wait);
}

static void PortReadReady(void* P, void* pRefData, long Type, long Zero) {
	long RefData = (long)pRefData;

	if (!ser_devs[RefData].ser_ttyp) {
		return;
	}

	if (ser_devs[RefData].dev_state != DEV_OPEN)
		return;

	if (ser_devs[RefData].ser_ttyp->closing) {
		return;
	}
	if (!ser_devs[RefData].P) {
		return;
	}


	if (!ser_devs[RefData].remainder.xoff) {
		tty_isdn_read (RefData);
	}
}


static int
tty_isdn_open(struct tty_struct *tty, struct file *filp)
{
	int dev_num;
	ser_dev_t *sd;
	dword porterror=0;
	byte ComPort[64];
	dword RxTrigger = 1;
	dword TxTrigger = 1;
	unsigned long old_irql = eicon_splimp ();

	if( !initialised)
	{
		eicon_splx (old_irql);
		return(-ENOMEM);
	}

	/* get minor number of device being opened */

	if (!(dev_num = DIVA_MINOR(tty))) {
		eicon_splx (old_irql);
		return(0);
	}

	if(ser_devs[dev_num].dev_state == DEV_OPENING) {
		ser_devs[dev_num].dev_open_count++;
#if !defined(__KERNEL_VERSION_GT_2_4__)
		MOD_INC_USE_COUNT;
#endif

		eicon_splx (old_irql);
		return(0);
	}

	if(ser_devs[dev_num].dev_state == DEV_OPEN &&
		ser_devs[dev_num].lock_state == DEV_LOCKED) {
		eicon_splx (old_irql);
		return(-EBUSY);
	}

	if(ser_devs[dev_num].dev_state == DEV_OPEN)
	{
		ser_devs[dev_num].dev_open_count++;
#if !defined(__KERNEL_VERSION_GT_2_4__)
		MOD_INC_USE_COUNT;
#endif
		eicon_splx (old_irql);
		return(0);
	}

	ser_devs[dev_num].dev_state = DEV_OPENING;
	ser_devs[dev_num].idtr = 0;
	ser_devs[dev_num].irts = 0;

	sprintf (ComPort, (byte*)"DiPort%d",dev_num);

	ser_devs[dev_num].dcd_changed_nr				= 0;
	ser_devs[dev_num].remainder.f_length		= 0;
	ser_devs[dev_num].remainder.f_limit		  =\
											sizeof(ser_devs[0].remainder.tx_collect);
	ser_devs[dev_num].signal_dcd 				= 0;
	ser_devs[dev_num].current_dcd_state = 0;

	ser_devs[dev_num].P = \
			PortOpen(ComPort, &porterror,(byte *)&ser_devs[dev_num].Acpt,\
														ser_devs[dev_num].profile);

	if(porterror !=0 || !ser_devs[dev_num].P) {
		ser_devs[dev_num].P         = 0;
		ser_devs[dev_num].dev_state = DEV_CLOSED;
		eicon_splx (old_irql);
		return(-EBUSY);
	}

	ser_devs[dev_num].dev_num = dev_num;
	ser_devs[dev_num].MsrShadow = MS_DSR_ON | MS_CTS_ON;
	PortSetReadCallBack(ser_devs[dev_num].P, RxTrigger, PortReadReady, (void*)(long)dev_num);
	PortSetWriteCallBack(ser_devs[dev_num].P,TxTrigger, PortWriteReady, (void*)(long)dev_num);
	PortSetModemStatusShadow(ser_devs[dev_num].P, &(ser_devs[dev_num].MsrShadow));
	diva_set_port_notification (&ser_devs[dev_num]);

	sd = &ser_devs[dev_num];
	printk(KERN_INFO "%s: [%p:%s] minor=%d\n", __FUNCTION__, sd->P->Channel, sd->P->Name, dev_num);

	tty->driver_data = sd;
	sd->ser_ttyp = tty;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	tty_port_tty_set(tty->port, tty);
#endif

	ser_devs[dev_num].dev_state = DEV_OPEN;
	ser_devs[dev_num].dev_open_count++;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	tty->termios = sd->normaltermios;
#else
	*(tty->termios) = sd->normaltermios;
#endif

#if !defined(__KERNEL_VERSION_GT_2_4__)
	MOD_INC_USE_COUNT;
#endif


	if (diva_tty_init_prm[0]) {
		char buffer[64];
    int length = MIN(62, str_len(&diva_tty_init_prm[0]));

    if (length) {
			mem_cpy (buffer, &diva_tty_init_prm[0], length);
			buffer[length++] = '\r';
			buffer[length] = 0;

		  DBG_TRC(("[%p:%s] TTY_INIT<%s>", sd->P->Channel, sd->P->Name, buffer))

			sd->P->control_operation_mode =  1;
			sd->P->control_response       = -1;
			atWrite (sd->P, buffer, length);
			sd->P->control_operation_mode =  0;
		}
	}

	eicon_splx (old_irql);

	return(0);

}

static void tty_isdn_close(struct tty_struct *tty, struct file *filp) {
	ser_dev_t *devptr = (ser_dev_t *)tty->driver_data;
	unsigned long old_irql = eicon_splimp ();
	ISDN_PORT* P;

	if (!(DIVA_MINOR(tty))) {
		eicon_splx (old_irql);
		return;
	}

	if (!devptr) {
		eicon_splx (old_irql);
		return;
	}

	if (devptr->dev_num==ETSER_CTRL_DEV) {
		ser_ctrl->dev_state=DEV_CLOSED;
		ser_ctrl->dev_num=0;
		eicon_splx (old_irql);
		return;
	}

	if (tty_hung_up_p(filp)) {
#if !defined(__KERNEL_VERSION_GT_2_4__)
		MOD_DEC_USE_COUNT;
#endif
		eicon_splx (old_irql);
		return;
	}


	if ((DIVA_READ_TTY_COUNT(tty->count) == 1) && (devptr->dev_open_count != 1))
		devptr->dev_open_count = 1;
	if (devptr->dev_open_count-- > 1) {
#if !defined(__KERNEL_VERSION_GT_2_4__)
		MOD_DEC_USE_COUNT;
#endif
		eicon_splx (old_irql);
		return;
	}
	devptr->dev_open_count = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	tty->closing = 1;
#endif
	P = devptr->P;

	if (!P) {
		eicon_splx (old_irql);
		return;
	}

	tty_wait_until_sent(tty, 30 * HZ);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	tty_port_tty_set(tty->port, NULL);
#endif

	diva_tty_channel_clear_write_q(devptr);
	devptr->P = 0;
	devptr->signal_dcd  = 0;
	devptr->ser_ttyp = 0;
	PortEnableNotification (P, 0, 0);
	PortSetEventMask (P,       0, 0);
	PortSetReadCallBack(P, 0, 0, (void*)devptr->dev_num);
	PortSetWriteCallBack(P,0, 0, (void*)devptr->dev_num);
	atHangup (P, 0);
	sysCancelDpc   (&P->RxRestartReadDpc);
	sysCancelDpc   (&P->TxRestartWriteDpc);
	sysCancelTimer (&P->TxNotifyTimer);
	PortPurge (P, 0);
	PortPurge (P, 1);
	PortClose (P);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	set_bit(TTY_IO_ERROR, &tty->flags);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	if (tty->ldisc.flush_buffer) {
		(tty->ldisc.flush_buffer)(tty);
	}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
	devptr->normaltermios = *(tty->termios);
#else
	devptr->normaltermios = tty->termios;
#endif

	devptr->dev_state = DEV_CLOSED;
	devptr->remainder.xoff = 0;
	devptr->remainder.f_length = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	tty->closing = 0;
#endif
	devptr->remainder.rx_cur = 0;

#if !defined(__KERNEL_VERSION_GT_2_4__)
	MOD_DEC_USE_COUNT;
#endif

	eicon_splx (old_irql);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,5,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
static ssize_t tty_isdn_write (struct tty_struct *tty,
                           const u_char *tbuf, size_t count)
#else
static int tty_isdn_write (struct tty_struct *tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
                           int from_user,
#endif
                           const u_char *tbuf, int count)
#endif
{
	int dev_num = DIVA_MINOR(tty);
	ser_dev_t* sd;
	struct sk_buff* skb;
	byte* data;

	if (!count) {
		return (0);
	}
	if ((count < 0) || (!tty) || tty->closing) {
		return (-EIO);
	}

	count = (count > 2048) ? 2048 : count;

	sd = &ser_devs[dev_num];

	if (atomic_read(&sd->tx_q_sz) >= DIVA_TTY_TX_Q_FC_LEVEL) {
		diva_tty_start_dpc();
		return (0);
	}
	if (!(skb = alloc_skb (((count < 512) ? (512+16) : (count+16)), GFP_ATOMIC))) {
		return (0);
	}
	if (!(data = skb_put(skb, count))) {
		kfree_skb(skb);
		return (0);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
	if (from_user) {
		copy_from_user ((char*)data, tbuf, count);
	} else {
		memcpy (data, tbuf, count);
	}
#else
	memcpy (data, tbuf, count);
#endif

  if (divas_tty_debug_tty_data > 0) {
		byte* p = (byte*)data;
		dword length = (dword)count, to_print;

		DBG_TRC(("[%d] write %d", dev_num, length))

		do {
			to_print = MIN(length, 256);
			DBG_BLK((p, to_print))
			p      += to_print;
			length -= to_print;
		} while (length > 0);
  }

	skb_queue_tail (&sd->tx_q, skb);

	atomic_add(count, &sd->tx_q_sz);

	diva_tty_start_dpc();

	return (count);
}

static void diva_tty_channel_clear_write_q(register ser_dev_t* sd) {
	struct sk_buff* skb;

	atomic_set(&sd->tx_q_sz, 0);
	while ((skb = skb_dequeue (&sd->tx_q))) {
		kfree_skb(skb);
	}
	sd->tx_flow = 0;
	sd->tx_complete = 0;
	atomic_set(&sd->tx_q_sz, 0);
}

/*
	Remove data from queue and push it to the write routine
	*/
static void diva_tty_process_channel_write(register ser_dev_t* sd) {
	struct sk_buff* skb;
	unsigned long old_irql, written;

	old_irql = eicon_splimp();

	if (!sd->ser_ttyp) {
		diva_tty_channel_clear_write_q(sd);
		eicon_splx (old_irql);
		return;
	}

	while ((skb = skb_dequeue (&sd->tx_q))) {
		if ((written = __tty_isdn_write (sd->ser_ttyp, skb->data, skb->len)) < 0) {
			kfree_skb(skb);
			diva_tty_channel_clear_write_q(sd);
			break;
		} else if (written == 0) {
			sd->tx_flow = 1;
			skb_queue_head(&sd->tx_q, skb);
			break;
		}

		atomic_sub(MIN(skb->len, written), &sd->tx_q_sz);

		if (written >= skb->len) {
			kfree_skb(skb);
		} else {
			skb_pull(skb, written);
			skb_queue_head(&sd->tx_q, skb);
		}
	}

	if (sd->tx_flow && (atomic_read(&sd->tx_q_sz) < DIVA_TTY_TX_Q_FC_LEVEL_OFF)) {
		sd->tx_flow = 0;
		diva_tty_wakeup_write (sd);
	}

	eicon_splx (old_irql);
}

void diva_tty_process_write_dpc (void) {
	register int i;

	for (i = 0; i < (Channel_Count+1); i++) {
		if (skb_queue_len (&ser_devs[i].tx_q)) {
			diva_tty_process_channel_write (&ser_devs[i]);
		} else {

			register unsigned long old_irql = eicon_splimp();
			register ser_dev_t* sd = &ser_devs[i];
			if (sd->ser_ttyp && sd->tx_complete) {
				sd->tx_flow = 0;
				sd->tx_complete = 0;

				diva_tty_wakeup_write (sd);
			}
			eicon_splx (old_irql);

		}
	}
}

static int __tty_isdn_write (struct tty_struct *tty, char *tx_buffer, int count) {
	ser_dev_t *dev_ptr;
	int		total=0, cnt=count;
	int 	dev_num;
	dword	sizeWritten=0;
	int 	written = 0;
	int complete_frame_present = 0;
	int b_limit;

	if (!(dev_num=DIVA_MINOR(tty))) {
		cnt = (cnt > 512) ? 512 : cnt;
		tx_buffer[cnt]=0;
		if (process_control_tty_command(tx_buffer, 0, 0)) {
			count = -EIO;
		}
		return (count);
	}

	/* get out device structure from tty struct */
	if (!(dev_ptr = (ser_dev_t *)tty->driver_data)) {
		return (-EIO);
	}
	if (!dev_ptr->ser_ttyp) {
		return (-EIO);
	}
	if (!(dev_num=DIVA_MINOR(tty))) {
		return (-EIO);
	}
	if (tty->closing) {
		return (-EIO);
	}
	if (!dev_ptr->P || (dev_ptr->dev_state != DEV_OPEN)) {
		return (-EIO);
	}
	if (dev_ptr->P->sizeTxCompile) {
		return (0);
	}
	b_limit = (dev_ptr->P->Mode != P_MODE_FAX) ? 2048 : 512;
	if ((b_limit - dev_ptr->P->PortData.QOutCount - 256) <= 0) {
		return (0);
	}
	if (!queueSpace (&dev_ptr->P->TxQueue, (word)cnt+256)) {
		return (0);
	}
	if (dev_ptr->P->Mode == P_MODE_FAX) {
		if (!queueSpace(&dev_ptr->P->TxQueue, (word)(cnt + 256 + dev_ptr->P->port_u.Fax2.uLineLen))) {
			return (0);
		}
	}
	if (!PortDoCollectAsync (dev_ptr->P)) { /* Stream Mode */
		dev_ptr->remainder.f_length = 0;
		PortWrite(dev_ptr->P, (char*)&tx_buffer[0], cnt, &sizeWritten);
		if (!(total = sizeWritten)) {
			/* DBG_TRC(("[%p:%s] write disc race", dev_ptr->P->Channel, dev_ptr->P->Name)) */
			total = -EIO;
		}
	} else {   /* RNA mode, ASYNC2SYNC conversion necessary */
		const byte* pFrame = &tx_buffer[0];
		byte* collect = dev_ptr->remainder.tx_collect;

		if ((dev_ptr->remainder.f_length == 1) &&
				(*pFrame == 0x7E) && (collect[0] == 0x7E)) {
			dev_ptr->remainder.f_length = 0;
		}

		for (;;) {
			if (!dev_ptr->remainder.f_length) {
				while (cnt) {
					if (*pFrame == 0x7E) {
						break;
					}
					cnt--;
					pFrame++;
					written++;
				}
				if (written) {
					DBG_FTL(("[%p:%s] tty_isdn_write discard %d %d", dev_ptr->P->Channel, dev_ptr->P->Name,
					         cnt + written, written))
				}
				if (!cnt) {
					return (written);
				}
			}
			if ((dev_ptr->remainder.f_length + cnt) > dev_ptr->remainder.f_limit) {
				if (dev_ptr->remainder.f_limit <= dev_ptr->remainder.f_length) {
					DBG_FTL(("[%p:%s] tty_isdn_write overrun %d %d %d %d", dev_ptr->P->Channel, dev_ptr->P->Name,
					         cnt + written, written, dev_ptr->remainder.f_length, dev_ptr->remainder.f_limit))
					dev_ptr->remainder.f_length = 0;
				} else { /* Multiple frames in one write ??? */
					DBG_FTL(("[%p:%s] tty_isdn_write part %d %d %d %d", dev_ptr->P->Channel, dev_ptr->P->Name,
					         cnt + written, written, dev_ptr->remainder.f_length, dev_ptr->remainder.f_limit))
					cnt = dev_ptr->remainder.f_limit - dev_ptr->remainder.f_length;
					break;
				}
			} else {
				break;
			}
		}

		while (cnt) {
			cnt--;
			written++;
			collect[dev_ptr->remainder.f_length++] = *pFrame;
			if ((*pFrame == 0x7E) && (dev_ptr->remainder.f_length > 1)) {
				complete_frame_present = 1;
				break;
			}
			pFrame++;
		}
		total = written;
	}

	if (complete_frame_present) { /* Do it here due to tha fact that PortWrite
																	 will allocate 2048 bytes on the stack
																	 and together with out reassembly buffer
																	 it will be too huge stack usage
																	*/
		complete_frame_present = 0;

		PortWrite (dev_ptr->P,
							 dev_ptr->remainder.tx_collect,
							 dev_ptr->remainder.f_length,
							 &sizeWritten);
		dev_ptr->remainder.tx_collect[0] = 0x7E;
		dev_ptr->remainder.f_length = 1;
		total = written;
	}

	return (total);
}

static int
tty_isdn_ioctl (struct tty_struct *tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
struct file *filp,
#endif
uint cmd, ulong arg) {
	int	dev_num;
	unsigned int ival;
	int rc;
	unsigned long old_irql;
	ser_dev_t* sd;

#if !defined(__KERNEL_VERSION_GT_2_4__)
	if (!tty->device) {
		return (-EIO);
	}
#endif

	if (!(DIVA_MINOR(tty)) && (cmd == 0x3701)) {
		int ret = 0;
		byte buf[128];

#if defined(__KERNEL_VERSION_GT_2_4__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) || \
	(defined(__RHEL_DRM__) && __RHEL_DRM__ >= 5)

		if (!access_ok ((char*)arg, 128)) {
#else
		if (!access_ok (VERIFY_READ, (char*)arg, 128)) {
#endif
#else
		if (verify_area (VERIFY_READ, (char*)arg, 128)) {
#endif
			return (-EFAULT);
		}
		if (copy_from_user (buf, (char*)arg, 128)) {
			return (-EFAULT);
		}

		buf[63] = 0;

		old_irql = eicon_splimp ();
		if ((ret = process_control_tty_command (buf, buf, sizeof(buf)-1)) < 0) {
			ret = (-EIO);
		}
		eicon_splx (old_irql);


		if (ret > 0) {
#if defined(__KERNEL_VERSION_GT_2_4__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) || \
	(defined(__RHEL_DRM__) && __RHEL_DRM__ >= 5)
			if (!access_ok ((char*)arg, 128)) {
#else
			if (!access_ok (VERIFY_WRITE, (char*)arg, 128)) {
#endif
#else
			if (verify_area (VERIFY_WRITE, (char*)arg, 128)) {
#endif
				return (-EFAULT);
			}
			if (copy_to_user((char*)arg, buf, 128)) {
				return (-EFAULT);
			}
		}

		return (ret);
	}

		switch (cmd) {
			case TIOCMGET: {
				uint value = 0, msr;

				old_irql = eicon_splimp ();
				if (
#if !defined(__KERNEL_VERSION_GT_2_4__)
						 !tty->device ||
#endif
						 !(dev_num = DIVA_MINOR(tty)) ||
						 !ser_devs[dev_num].P) {
					eicon_splx (old_irql);
					return (-EIO);
				}
				sd = &ser_devs[dev_num];

				msr = (uint)ser_devs[dev_num].MsrShadow;
				value |= (!sd->idtr) ? TIOCM_DTR : 0;
				value |= (!sd->irts) ? TIOCM_RTS : 0;
				value |= (msr & MS_RLSD_ON) ? TIOCM_CAR : 0;
				value |= (msr & MS_DSR_ON)  ? TIOCM_DSR : 0;
				value |= (msr & MS_CTS_ON)  ? TIOCM_CTS : 0;
				eicon_splx (old_irql);

				return (put_user (value, (uint *)arg));
			}
			case TIOCSERGETLSR: /* Get line status register */
				return (-ENOIOCTLCMD);

		 case TIOCMIWAIT:
				return (-ENOIOCTLCMD);

		case TIOCGICOUNT:
				return (-ENOIOCTLCMD);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
		case TIOCGHAYESESP:
				return (-ENOIOCTLCMD);
#endif

		case TIOCSERCONFIG:
				return (0);

		case TIOCSSERIAL:
				return (-ENOIOCTLCMD);

		case TIOCSERGWILD:
			return put_user(0L, (unsigned long *) arg);

		case TIOCSERSWILD:
#if !defined(__KERNEL_VERSION_GT_2_4__)
			if (!suser())
				return -EPERM;
#endif
			return 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
		case TIOCSHAYESESP:
				return (-ENOIOCTLCMD);
#endif

			case TCSBRK:
				return (-ENOIOCTLCMD);

			case TCSBRKP:
				return (-ENOIOCTLCMD);

			case TIOCMSET: /* CHANGE SIGNALS */
#if defined(__KERNEL_VERSION_GT_2_4__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) || \
	(defined(__RHEL_DRM__) && __RHEL_DRM__ >= 5)
        rc = ((access_ok((void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#else
        rc = ((access_ok(VERIFY_READ, (void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#endif
#else
        rc = verify_area(VERIFY_READ, (void *) arg, sizeof(unsigned int));
#endif
				if (rc == 0) {
					get_user(ival, (unsigned int *) arg);

					old_irql = eicon_splimp ();
					if (
#if !defined(__KERNEL_VERSION_GT_2_4__)
						 !tty->device ||
#endif
						 !(dev_num = DIVA_MINOR(tty)) || \
						 !ser_devs[dev_num].P) {
						eicon_splx (old_irql);
						return (-EIO);
					}
					sd = &ser_devs[dev_num];
					diva_setsignals(sd, ((ival & TIOCM_DTR) ? 1 : 0), \
																		((ival & TIOCM_RTS) ? 1 : 0));
					eicon_splx (old_irql);
				}
				return (rc);

			case TIOCMBIS: /* SET SIGNALS */
#if defined(__KERNEL_VERSION_GT_2_4__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) || \
	(defined(__RHEL_DRM__) && __RHEL_DRM__ >= 5)
        rc = ((access_ok((void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#else
        rc = ((access_ok(VERIFY_READ, (void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#endif
#else
        rc = verify_area(VERIFY_READ, (void *) arg, sizeof(unsigned int));
#endif
			if (rc == 0) {
					get_user(ival, (unsigned int *) arg);

					old_irql = eicon_splimp ();
					if (
#if !defined(__KERNEL_VERSION_GT_2_4__)
							!tty->device ||
#endif
							!(dev_num = DIVA_MINOR(tty)) || \
						 !ser_devs[dev_num].P) {
						eicon_splx (old_irql);
						return (-EIO);
					}
					sd = &ser_devs[dev_num];
					diva_setsignals(sd, ((ival & TIOCM_DTR) ? 1 : -1),
						((ival & TIOCM_RTS) ? 1 : -1));
					eicon_splx (old_irql);
			 }
			 return (rc);

			case TIOCMBIC: /* CLEAR SIGNALS */
#if defined(__KERNEL_VERSION_GT_2_4__)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)) || \
	(defined(__RHEL_DRM__) && __RHEL_DRM__ >= 5)
        rc = ((access_ok((void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#else
        rc = ((access_ok(VERIFY_READ, (void *) arg, sizeof(unsigned int))) == 0) ? -EFAULT : 0;
#endif
#else
        rc = verify_area(VERIFY_READ, (void *) arg, sizeof(unsigned int));
#endif
				if (rc == 0) {
					get_user(ival, (unsigned int *) arg);

					old_irql = eicon_splimp ();
					if (
#if !defined(__KERNEL_VERSION_GT_2_4__)
							!tty->device ||
#endif
							!(dev_num = DIVA_MINOR(tty)) || \
						 !ser_devs[dev_num].P) {
						eicon_splx (old_irql);
						return (-EIO);
					}
					sd = &ser_devs[dev_num];
					diva_setsignals(sd, ((ival & TIOCM_DTR) ? 0 : -1),
								((ival & TIOCM_RTS) ? 0 : -1));
					eicon_splx (old_irql);
				}
				return (rc);


			case TIOCGSOFTCAR:
				rc = put_user(C_CLOCAL(tty) ? 1 : 0, (ulong *) arg);
				return rc;

			case TIOCSSOFTCAR:
				get_user(ival, (ulong *) arg);
				old_irql = eicon_splimp ();
				if (
#if !defined(__KERNEL_VERSION_GT_2_4__)
						!tty->device ||
#endif
						!(dev_num = DIVA_MINOR(tty)) || \
					 !ser_devs[dev_num].P) {
					eicon_splx (old_irql);
					return (-EIO);
				}
				sd = &ser_devs[dev_num];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
				ival = tty->termios.c_cflag = ((tty->termios.c_cflag & ~CLOCAL) | (arg ? CLOCAL : 0));
#else
				ival = tty->termios->c_cflag = ((tty->termios->c_cflag & ~CLOCAL) | (arg ? CLOCAL : 0));
#endif

				if (ival & CLOCAL) {
					sd->signal_dcd = 0;
				} else {
					sd->signal_dcd = 1;
				}
				eicon_splx (old_irql);
				return 0;
		}

	return (-ENOIOCTLCMD);
}

static void tty_isdn_set_termios (struct tty_struct *tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
																	struct termios *old
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
																	const struct ktermios *old
#else
																	struct ktermios *old
#endif
#endif
																	) {
	unsigned long old_irql = eicon_splimp ();
	ser_dev_t *sd = (ser_dev_t *)tty->driver_data;

	if (!tty || !(sd = (ser_dev_t *)tty->driver_data) || !sd->P || tty->closing) {
		eicon_splx (old_irql);
		return;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	if (old && (tty->termios.c_cflag  == old->c_cflag)) {
#else
	if (old && (tty->termios->c_cflag  == old->c_cflag)) {
#endif
		eicon_splx (old_irql);
		return;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	sd->signal_dcd = ((tty->termios.c_cflag  & CLOCAL) == 0);
#else
	sd->signal_dcd = ((tty->termios->c_cflag  & CLOCAL) == 0);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	DBG_TRC(("[%p:%s] DCD DETECT %s, CLOCAL %s",
					 sd->P->Channel, sd->P->Name, sd->signal_dcd ? "ON" : "OFF",
					 (tty->termios.c_cflag  & CLOCAL) ? "SET" : "CLEAN"))
#else
	DBG_TRC(("[%p:%s] DCD DETECT %s, CLOCAL %s",
					 sd->P->Channel, sd->P->Name, sd->signal_dcd ? "ON" : "OFF",
					 (tty->termios->c_cflag  & CLOCAL) ? "SET" : "CLEAN"))
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	if (!(tty->termios.c_cflag & CBAUD)) {
#else
	if (!(tty->termios->c_cflag & CBAUD)) {
#endif
		DBG_TRC(("[%p:%s] CBAUD == B0", sd->P->Channel, sd->P->Name))
		diva_setsignals (sd, 0, 0);
	}

	eicon_splx (old_irql);
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
static int tty_isdn_chars_in_buffer(struct tty_struct *tty) {
#else
static unsigned int tty_isdn_chars_in_buffer(struct tty_struct *tty) {
#endif
	int dev_num;

	if ((!tty) || tty->closing || ((dev_num = DIVA_MINOR(tty)) >= (Channel_Count+1))) {
		return (0);
	}

	return (atomic_read(&ser_devs[dev_num].tx_q_sz));
}

static void tty_isdn_hangup (struct tty_struct* tty) {
	ser_dev_t *sd;
	ISDN_PORT* P;
	unsigned long old_irql;

	if (!tty) {
		return;
	}

	old_irql = eicon_splimp ();

	sd = (ser_dev_t *)tty->driver_data;

	if (tty->closing || !sd ||
			!sd->P ||
			(sd->dev_state != DEV_OPEN)) {
		eicon_splx (old_irql);
		return;
	}

	diva_tty_channel_clear_write_q(sd);
	P = sd->P;
	sd->P = 0;
	PortEnableNotification (P, 0, 0);
	PortSetEventMask (P,       0, 0);
	PortSetReadCallBack(P,0, 0, (void*)sd->dev_num);
	PortSetWriteCallBack(P,0, 0, (void*)sd->dev_num);
	sysCancelDpc	 (&P->RxRestartReadDpc);
	sysCancelDpc   (&P->TxRestartWriteDpc);
	sysCancelTimer (&P->TxNotifyTimer);
	atHangup (P, 0);
	PortPurge (P, 0);
	PortPurge (P, 1);
	PortClose (P);

	set_bit(TTY_IO_ERROR, &tty->flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
	if (tty->ldisc.flush_buffer)
		(tty->ldisc.flush_buffer)(tty);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
//        if (tty->ldisc.ops->flush_buffer)
//                (tty->ldisc.ops->flush_buffer)(tty);
#else
//        if (tty->ldisc->ops && tty->ldisc->ops->flush_buffer)
//                (tty->ldisc->ops->flush_buffer)(tty);
#endif
#endif

	sd->ser_ttyp = 0;
	sd->signal_dcd  = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
		sd->normaltermios = tty->termios;
#else
	if (tty->termios) {
		sd->normaltermios = *(tty->termios);
	}
#endif
	sd->dev_state = DEV_CLOSED;
	sd->remainder.xoff	 = 0;
	sd->remainder.f_length		= 0;
	tty->closing = 0;
	sd->remainder.rx_cur    = 0;
	sd->dev_open_count = 0;

	eicon_splx (old_irql);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0))
static int tty_isdn_write_room(struct tty_struct *tty) {
#else
static unsigned int tty_isdn_write_room(struct tty_struct *tty) {
#endif
	int dev_num, ret;

	if ((!tty) || tty->closing || ((dev_num = DIVA_MINOR(tty)) >= (Channel_Count+1))) {
		return (0);
	}

	ret = DIVA_TTY_TX_Q_FC_LEVEL - atomic_read(&ser_devs[dev_num].tx_q_sz);

	return ((ret < 0) ? 0 : ret);
}

static void tty_isdn_throttle(struct tty_struct *tty) {
#if !defined(DIVA_USES_MUTEX)
	unsigned long old_irql = eicon_splimp ();
#endif
	ser_dev_t *sd = tty->driver_data;

	if (!tty->closing) {
		if (sd && (sd->dev_state == DEV_OPEN) && sd->P) {
			sd->remainder.xoff = 1;
			sysCancelDpc (&sd->P->RxRestartReadDpc);
		}
	}
#if !defined(DIVA_USES_MUTEX)
	eicon_splx (old_irql);
#endif
}

static void tty_isdn_unthrottle(struct tty_struct *tty) {
	ser_dev_t *sd = tty->driver_data;
#if !defined(DIVA_USES_MUTEX)
	unsigned long old_irql = eicon_splimp ();
#endif

	if (!tty->closing) {
		if (sd && (sd->dev_state == DEV_OPEN)) {
			if (sd->ser_ttyp) {
				sd->remainder.xoff = 0;
				if (sd->P) {
					sysScheduleDpc (&sd->P->RxRestartReadDpc);
				}
			}
		}
	}
#if !defined(DIVA_USES_MUTEX)
	eicon_splx (old_irql);
#endif
}

#if defined(__KERNEL_VERSION_GT_2_4__) /* { */
static int diva_tty_tiocmget(struct tty_struct *tty
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
, struct file *file
#endif
) {
	uint value = 0, msr;
	ser_dev_t* sd;
	unsigned long old_irql;
	int dev_num;

	old_irql = eicon_splimp ();
	if (!(dev_num = DIVA_MINOR(tty)) || !ser_devs[dev_num].P) {
		eicon_splx (old_irql);
		return (-EIO);
	}
	sd = &ser_devs[dev_num];

	msr = (uint)ser_devs[dev_num].MsrShadow;
	value |= (!sd->idtr) ? TIOCM_DTR : 0;
	value |= (!sd->irts) ? TIOCM_RTS : 0;
	value |= (msr & MS_RLSD_ON) ? TIOCM_CAR : 0;
	value |= (msr & MS_DSR_ON)  ? TIOCM_DSR : 0;
	value |= (msr & MS_CTS_ON)  ? TIOCM_CTS : 0;
	eicon_splx (old_irql);

	return ((int)value);
}

static int diva_tty_tiocmset(struct tty_struct *tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
struct file *file,
#endif
	unsigned int set, unsigned int clear) {
	ser_dev_t* sd;
	unsigned long old_irql;
	int dev_num;

	old_irql = eicon_splimp ();
	if (!(dev_num = DIVA_MINOR(tty)) || !ser_devs[dev_num].P) {
		eicon_splx (old_irql);
		return (-EIO);
	}
	sd = &ser_devs[dev_num];

	diva_setsignals(sd, ((set   & TIOCM_DTR) ? 1 : -1), ((set   & TIOCM_RTS) ? 1 : -1));
	diva_setsignals(sd, ((clear & TIOCM_DTR) ? 0 : -1), ((clear & TIOCM_RTS) ? 0 : -1));

	eicon_splx (old_irql);

	return 0;
}

#endif /* } */

int eicon_tty_isdn_modem_init(int nr, int minors) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

	memset(&eicon_tty_driver[nr], 0, sizeof(eicon_tty_driver[0]));
	eicon_tty_driver[nr].magic        = TTY_DRIVER_MAGIC;
	eicon_tty_driver[nr].name         = tty_isdnname[nr];
	eicon_tty_driver[nr].major        = FIRST_MAJOR+nr;
	eicon_tty_driver[nr].minor_start  = (nr == 0) ? 0 : 1;
	eicon_tty_driver[nr].num          = (nr == 0) ? (minors + 1) : minors;
	eicon_tty_driver[nr].type         = TTY_DRIVER_TYPE_SERIAL;
	eicon_tty_driver[nr].subtype      = SERIAL_TYPE_NORMAL;
	/* eicon_tty_driver[nr].subtype   = SERIAL_TYPE_NORMAL; */
	eicon_tty_driver[nr].init_termios = diva_deftermios;
	eicon_tty_driver[nr].init_termios.c_cflag =
                                      B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	eicon_tty_driver[nr].flags        = TTY_DRIVER_REAL_RAW;
					/* TTY_DRIVER_RESET_TERMIOS; */
	eicon_tty_driver[nr].refcount     = &eicon_refcount[nr];
	eicon_tty_driver[nr].table        = eicon_table[nr];
	eicon_tty_driver[nr].termios      = eicon_termios[nr];
	eicon_tty_driver[nr].termios_locked =
                                      eicon_termios_locked[nr];

	eicon_tty_driver[nr].open         = tty_isdn_open;
	eicon_tty_driver[nr].close        = tty_isdn_close;
	eicon_tty_driver[nr].write        = tty_isdn_write;
	eicon_tty_driver[nr].put_char     = NULL;
	eicon_tty_driver[nr].flush_chars  = 0;
	eicon_tty_driver[nr].write_room   = tty_isdn_write_room;
	eicon_tty_driver[nr].chars_in_buffer =
                                      tty_isdn_chars_in_buffer;
	eicon_tty_driver[nr].flush_buffer = NULL;
	eicon_tty_driver[nr].ioctl        = tty_isdn_ioctl;
	eicon_tty_driver[nr].throttle     = tty_isdn_throttle;
	eicon_tty_driver[nr].unthrottle   = tty_isdn_unthrottle;
	eicon_tty_driver[nr].set_termios  = tty_isdn_set_termios;
	eicon_tty_driver[nr].stop         = NULL;
	eicon_tty_driver[nr].start        = NULL;
	eicon_tty_driver[nr].hangup       = tty_isdn_hangup;

	sprintf (eicon_driver_names[nr], "Divatty%d", nr);
	eicon_tty_driver[nr].driver_name  = eicon_driver_names[nr];

	if (tty_register_driver(&eicon_tty_driver[nr])) {
		return -1;
	}
#else /* } { */

static struct tty_operations diva_tty_ops = {
	.open = tty_isdn_open,
	.close = tty_isdn_close,
	.write = tty_isdn_write,
	/* .put_char = putchar, */
	/* .flush_chars = stl_flushchars, */
	.write_room = tty_isdn_write_room,
	.chars_in_buffer = tty_isdn_chars_in_buffer,
	.ioctl = tty_isdn_ioctl,
	.set_termios = tty_isdn_set_termios,
	.throttle = tty_isdn_throttle,
	.unthrottle = tty_isdn_unthrottle,
	/* .stop = stop, */
	/* .start = start, */
	.hangup = tty_isdn_hangup,
	/* .flush_buffer = stl_flushbuffer, */
	/* .break_ctl = breakctl, */
	/* .wait_until_sent = waituntilsent, */
	/* .send_xchar = sendxchar, */
	/* .read_proc = readproc, */
	.tiocmget = diva_tty_tiocmget,
	.tiocmset = diva_tty_tiocmset,
};

	if (nr != 0) {
		DBG_ERR(("Wrong major number(%d)", nr))
		return (-1);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	eicon_tty_driver = tty_alloc_driver (minors+1, TTY_DRIVER_REAL_RAW);
	if (IS_ERR(eicon_tty_driver)) {
		DBG_ERR(("tty_alloc_driver(%d) failed", minors+1))
		return (-1);
	}
#else
	if (!(eicon_tty_driver = alloc_tty_driver (minors+1))) {
		DBG_ERR(("alloc_tty_driver(%d) failed", minors+1))
		return (-1);
	}
#endif
	eicon_tty_driver->owner = THIS_MODULE;
	eicon_tty_driver->driver_name  = diva_tty_drvname;
	eicon_tty_driver->name         = "ttyds";
#if defined(CONFIG_DEVFS_FS)
	eicon_tty_driver->devfs_name   = "ttyds/S";
#endif
	eicon_tty_driver->major        = FIRST_MAJOR+nr;
	eicon_tty_driver->minor_start  = 0;
	eicon_tty_driver->type         = TTY_DRIVER_TYPE_SERIAL;
	eicon_tty_driver->subtype      = SERIAL_TYPE_NORMAL;
	eicon_tty_driver->init_termios = diva_deftermios;
	eicon_tty_driver->flags        = TTY_DRIVER_REAL_RAW;
	tty_set_operations(eicon_tty_driver, &diva_tty_ops);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
    {
		int i;

		for (i=0; i<minors+1; i++) {
			tty_port_init(&dsport[i]);
			tty_port_link_device(&dsport[i], eicon_tty_driver, i);
			tty_port_set_initialized(&dsport[i], 1);
		}
	}
#endif

	if (tty_register_driver(eicon_tty_driver)) {
		DBG_ERR(("tty_register_driver failed"))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
		tty_driver_kref_put(eicon_tty_driver);
#else
		put_tty_driver(eicon_tty_driver);
#endif
		eicon_tty_driver = 0;
		return (-1);
	}

#endif /* } */

	driver_registered[nr] = 1;
	return 0;
}

/* Initialises the device array - called from EtSRinit after alloc of mem */
void init_device_array(int NumDevices)
{
	ulong i;

	for( i = 0; i < NumDevices; i++)
	{
		memset(&ser_devs[i], 0, sizeof(ser_dev_t) );
		ser_devs[i].dev_state = DEV_CLOSED;
		ser_devs[i].lock_state = DEV_UNLOCKED;
		ser_devs[i].dev_open_count = 0;
		ser_devs[i].dev_num = i;
		ser_devs[i].profile = 5;
		ser_devs[i].normaltermios = diva_deftermios;

		atomic_set(&ser_devs[i].tx_q_sz, 0);
		skb_queue_head_init(&ser_devs[i].tx_q);
	}

}

extern int connect_didd(void);
static void diva_read_adapter_array (DESCRIPTOR* d, int* length, int register_didd) {

#define DIVA_OS_READ_DIDD_ARRAY() do \
	{ \
		extern void DIVA_DIDD_Read (DESCRIPTOR *, int); \
		int i; \
		DIVA_DIDD_Read_fn=DIVA_DIDD_Read; \
    \
		(*DIVA_DIDD_Read_fn)(d, *length); \
		for (i = 0; i < *length; i++) { \
			if (!d[i].request) break; \
		} \
		*length = i; \
	} while(0)

	memset (d, 0x00, *length);

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

#if ( ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) ) && defined(__i386__) )
	EtdM_DIDD_Read_fn=(EtdM_DIDD_Read_t)get_module_symbol(0,"EtdM_DIDD_Read");
	if (!EtdM_DIDD_Read_fn) {
		EtdM_DIDD_Read_fn=(EtdM_DIDD_Read_t)get_module_symbol("","EtdM_DIDD_Read");
	}
	if (!EtdM_DIDD_Read_fn) {
		DIVA_DIDD_Read_fn=(DIVA_DIDD_Read_t)get_module_symbol(0,"DIVA_DIDD_Read");
		if (!DIVA_DIDD_Read_fn) {
		  DIVA_DIDD_Read_fn=\
               (DIVA_DIDD_Read_t)get_module_symbol("","DIVA_DIDD_Read");
		}
	}

	if (EtdM_DIDD_Read_fn) {
		(*EtdM_DIDD_Read_fn)(d, length);
	} else if (DIVA_DIDD_Read_fn) {
		int i = 0;
		(*DIVA_DIDD_Read_fn)(d, *length);
		for (i = 0; i < *length; i++) {
			if (!d[i].request) break;
		}
		*length = i;
	} else {
		*length = 0;
	}
#else
	DIVA_OS_READ_DIDD_ARRAY();
#endif

#else /* } { */

	DIVA_OS_READ_DIDD_ARRAY();

#endif /* } */

	if (register_didd != 0) {
		connect_didd();
	}

#undef DIVA_OS_READ_DIDD_ARRAY
}

int EtSRinit(void)
{
	unsigned char *tmp_ptr;
	int length = sizeof(d);
	int i = 0, j = 0;

	initialised = 0;

#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ >= 9))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdate-time"
#endif
	DBG_TRC(("EICON TTYDS: %s", DIVA_BUILD_STRING))
	printk (KERN_INFO "EICON TTYDS: %s\n", DIVA_BUILD_STRING);
#if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ >= 9))
#pragma GCC diagnostic pop
#endif

	/*
		Read DIDD table
		*/
	diva_read_adapter_array (d, &length, 1);
	if (!(DIVA_DIDD_Read_fn || EtdM_DIDD_Read_fn)) {
		DBG_ERR(("DIVA_TTY: NO DIDD WAS FOUND"))
		printk (KERN_CRIT "DIVA_TTY: NO DIDD WAS FOUND\n");
		disconnect_didd();
		return (-1);
	}

	if ( length == 0) {
		DBG_ERR(("DIVA_TTY: NO ADAPTER ARRAY"))
		printk (KERN_CRIT "DIVA_TTY: NO ADAPTER ARRAY\n");
		disconnect_didd();
		return (-1);
	}
	Channel_Count = 0;
	Adapter_Count = 0;

	for (i = 0; i < length; i++) {
		if (d[i].type == IDI_MADAPTER) {
			DBG_LOG(("Detected MTPX adapter"))
			diva_mtpx_adapter_detected = 1;
			diva_get_extended_adapter_features (d[i].request);
			break;
		}
	}


	/*
		Initially calculate number of channels
		*/
	if (diva_mtpx_adapter_detected) {

		for(i = 0; i < length; i++) {
			if ((d[i].type == IDI_MADAPTER) && d[i].channels) {
				int VsCAPI = -1;
				int ch = diva_tty_read_channel_count (&d[i], &VsCAPI);
				if (VsCAPI <= 0) {
					if (ch > 0) {
						d[i].channels = (byte)ch;
					}
					Channel_Count += d[i].channels;
					if (Channel_Count > MAX_PORTS) {
						Channel_Count -= d[i].channels;
						break;
					}
					Adapter_Count++;
				} else {
					d[i].channels = 0;
				}
			}
		}
	} else {
		for(i = 0; i < length; i++) {
			if ((d[i].type < 0x80) && d[i].channels) {
				int ch = diva_tty_read_channel_count (&d[i], 0);
				if (ch > 0) {
					d[i].channels = (byte)ch;
				}
				Channel_Count += d[i].channels;
				if (Channel_Count > MAX_PORTS) {
					Channel_Count -= d[i].channels;
					break;
				}
				Adapter_Count ++;
			}
		}
	}

	if (Channel_Count == 0) {
		DBG_ERR(("DIVA_TTY: NO ADAPTER WAS FOUND"))
		printk (KERN_CRIT "DIVA_TTY: NO ADAPTER WAS FOUND\n");
		disconnect_didd();
		return (-1);
	}

	/* printk (KERN_CRIT "DIVA_TTY: %d Channels found\n", Channel_Count); */
	DBG_TRC(("DIVA_TTY: %d Channels found", Channel_Count))
	printk (KERN_INFO "%s: Adapters=%d Channels=%d (didd=%d)\n",
			__FUNCTION__, Adapter_Count, Channel_Count, length);

	/* Calculate the memory required for structures
	* (Channel_Count+1) * sizeof device structure +
	* Adapter_Count * sizeof adapter structure
	*/

	memsize=(Channel_Count+1)*sizeof(ser_dev_t) + Adapter_Count*sizeof(ser_adapter_t);

	if (!(mem_ptr = sysHeapAlloc(memsize+512))) {
		disconnect_didd();
		return(-1);
	}

	memset(mem_ptr, 0, memsize);

	/* Set up the Global Pointers */

	tmp_ptr =(unsigned char *)mem_ptr;
	ser_ctrl =(ser_dev_t *)tmp_ptr;

	ser_devs = (ser_dev_t *)tmp_ptr;

	tmp_ptr += (Channel_Count+1)*sizeof(ser_dev_t);
	tmp_ptr += 128;
	ser_adapter = (ser_adapter_t *)tmp_ptr;

	/*Initialize the Control Structure */

	ser_ctrl->dev_state = DEV_CLOSED;
	ser_ctrl->dev_num = 0;


	/*Initialize the device array */

	init_device_array(Channel_Count+1);

	/* Pass through the DIDD table again filling in the adapter
	 * structures just allocated
	 */

	for(i = 0, j= 0; i < length && j < Adapter_Count; i++)
	{
		int add = 0;
		if (diva_mtpx_adapter_detected) {
			add = (d[i].type == IDI_MADAPTER) && d[i].channels;
		} else {
			add = (d[i].type < 0x80) && d[i].channels;
		}
		if(add) {
			ser_adapter[j].type = d[i].type;
			ser_adapter[j].channels = d[i].channels;
			ser_adapter[j].features = d[i].features;
			ser_adapter[j].request = d[i].request;
			Desc_Length++;
			memcpy (&flat_descriptor[j], &d[i], sizeof (d[0]));
			flat_descriptor_length++;
			DBG_TRC(("DIVA_TTY: T(%02x) A(%02d), Ch:%02d, Req:%p",
              d[i].type, j, d[i].channels, d[i].request))
			printk (KERN_INFO "DIVA_TTY: T(%02x) A(%02d), Ch:%02d, Req:%p\n",
              d[i].type, j, d[i].channels, d[i].request);
			j++;
		}
	}


	/* call Port init stuff passing number of Channels
	 * so it will allocate that number of 'ports'      */

	if (DiPort_Dynamic_Init(1, Channel_Count+1) != VXD_SUCCESS) {
		sysHeapFree(mem_ptr);
		disconnect_didd();
		return(-1);
	}

	/* We are initialised - lets rock .. */
	initialised = 1;

	return (0);
}

int eicon_tty_isdn_init(void) {
	int i, j;
	dword porterror;
	byte ComPort[64];

#if 0
	/*
		Diva TTY does not work without MTPX and without available
		MTPX instances
		*/
	{
		int length = sizeof(d), i;

		memset(d, 0x00, sizeof(d));
		diva_read_adapter_array (d, &length, 0);
		for (i = 0, length = 0; length == 0 && i < sizeof(d)/sizeof(d[0]); i++) {
			length = (d[i].request != 0 && d[i].type == IDI_MADAPTER && d[i].features != 0 && d[i].channels != 0);
		}

		if (length == 0)
			return (-EIO);

		memset(d, 0x00, sizeof(d));
	}
#endif

#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
	init_MUTEX_LOCKED(&diva_tty_lock);
	up (&diva_tty_lock);
#endif
#endif
	atomic_set(&dpc_init_complete, 0);

	memset (&diva_tty_init_prm[0], 0x00, sizeof(diva_tty_init_prm));
	if (diva_tty_init) {
    int length = MIN(62, str_len(diva_tty_init));
		mem_cpy (&diva_tty_init_prm[0], diva_tty_init, length);
		diva_tty_init_prm[length] = 0;
	}

	if ((minors_per_major_init > 0) && (minors_per_major_init <= 255)) {
		minors_per_major = minors_per_major_init;
	}

	DBG_TRC(("I: com_port initialization begin ...\n"))

	if (diva_start_main_dpc ()) {
		DBG_FTL(("A: Can't start system DPC"))
		return (-EIO);
	}

	if (EtSRinit() < 0) {
		printk(KERN_ERR "%s: Initialization failure\n", __FUNCTION__);
		return -EIO;
	}




	DBG_LOG(("MINORS_PER_MAJOR=%d", minors_per_major))
	DBG_LOG(("MAX MAJORS      =%d", MAX_DIVA_TTY_MAJORS))
	DBG_LOG(("NEED MAJORS     =%d",
		Channel_Count/minors_per_major + (Channel_Count%minors_per_major) ? 1 : 0))

	for (i = Channel_Count, j = 0; ((i > 0) && (j < MAX_DIVA_TTY_MAJORS)); i -= MIN(i, minors_per_major), j++) {
		DBG_LOG(("Allocate MAJOR[%d]=%d for %d Minors", j, FIRST_MAJOR+j, MIN(i, minors_per_major)))
		driver_registered[j] = 0;
		if (eicon_tty_isdn_modem_init(j, MIN(i, minors_per_major))) {
			DBG_FTL(("Failed to allocate MAJOR[%d]=%d", j, FIRST_MAJOR+j))
			diva_shutdown_main_dpc ();
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
			for (i = 0; i < MAX_DIVA_TTY_MAJORS; i++) {
				if (driver_registered[i]) {
					tty_unregister_driver(&eicon_tty_driver[i]);
				}
			}
#else /* } { */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0)
			if (tty_unregister_driver(eicon_tty_driver)) {
				DBG_ERR(("Failed to unregister tty driver"))
			}
#else
			tty_unregister_driver(eicon_tty_driver);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
			tty_driver_kref_put(eicon_tty_driver);
#else
			put_tty_driver(eicon_tty_driver);
#endif
			eicon_tty_driver = 0;
#endif /* } */
			return -EIO;
		}
	}


	/* Call EtSRinit here to init adapter & device structs */

	DBG_TRC(("DIVA_TTY: com_port initialization complete"))
	/*
		Allocate all necessary memory, so we do not want to do it later
		*/
	for( i = 1; i < (Channel_Count+1); i++) {
		porterror = 0;
		sprintf (ComPort, (byte*)"DiPort%d", i);
		ser_devs[i].P =\
				PortOpen(ComPort, &porterror,(byte *)&ser_devs[i].Acpt,\
													ser_devs[i].profile);
		if (!porterror && ser_devs[i].P) {
			sysCancelTimer (&ser_devs[i].P->TxNotifyTimer);
			PortClose (ser_devs[i].P);
		}
		ser_devs[i].P = 0;
	}

	atomic_inc(&dpc_init_complete);
	atomic_set(&divas_timer_running, 1);

	diva_start_management (1001);


	/*
		Read Configuration from CFGLib
		*/
	{
		IDI_SYNC_REQ sync_req;

		diva_tty_read_global_option(&sync_req, "GlobalOptions\\FAX_FORCE_ECM", DIVA_FAX_FORCE_ECM);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\FAX_FORCE_V34", DIVA_FAX_FORCE_V34);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\DIVA_FAX_ALLOW_V34_CODES", DIVA_FAX_ALLOW_V34_CODES);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\DIVA_FAX_ALLOW_HIRES", DIVA_FAX_ALLOW_HIRES);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\FAX_FORCE_SEP_SUB_PWD", DIVA_FAX_FORCE_SEP_SUB_PWD);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\DIVA_ISDN_IGNORE_NUMBER_TYPE",
																DIVA_ISDN_IGNORE_NUMBER_TYPE);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\DIVA_ISDN_AT_RSP_IF_RINGING",
																DIVA_ISDN_AT_RSP_IF_RINGING);
		diva_tty_read_global_option(&sync_req, "GlobalOptions\\DIVA_ISDN_DIAL_RR",
																DIVA_ISDN_DIAL_RR);
		diva_tty_read_global_port_option(&sync_req, "GlobalOptions\\ROUND_ROBIN_RINGS",
																P_FIX_ROUND_ROBIN_RINGS);

		diva_tty_read_cfg_value (&sync_req, 0, "GlobalOptions\\Cause",
														 DivaCfgLibValueTypeUnsigned,
														 &ports_unavailable_cause,
														 sizeof(ports_unavailable_cause));

		diva_tty_read_cfg_value (&sync_req, 0, "GlobalOptions\\TTY_INIT",
														 DivaCfgLibValueTypeASCIIZ,
														 &diva_tty_init_prm[0],
														 sizeof(diva_tty_init_prm)-1);

		diva_tty_read_cfg_value (&sync_req, 0, "GlobalOptions\\WaitSigDisc",
														 DivaCfgLibValueTypeBool,
														 &divas_tty_wait_sig_disc,
														 sizeof(divas_tty_wait_sig_disc));

		diva_tty_read_cfg_value (&sync_req, 0, "Debug\\DebugSig",
														 DivaCfgLibValueTypeBool,
														 &divas_tty_debug_sig,
														 sizeof(divas_tty_debug_sig));
		diva_tty_read_cfg_value (&sync_req, 0, "Debug\\DebugNet",
														 DivaCfgLibValueTypeBool,
														 &divas_tty_debug_net,
														 sizeof(divas_tty_debug_net));
		diva_tty_read_cfg_value (&sync_req, 0, "Debug\\DebugNetData",
														 DivaCfgLibValueTypeBool,
														 &divas_tty_debug_net_data,
														 sizeof(divas_tty_debug_net_data));
	}
	return 0;
}


#ifdef MODULE

void cleanup_module (void) {
	ISDN_ADAPTER *A;
	ISDN_PORT_DESC	*D;

	diva_stop_management ();

	do_adapter_shutdown ();
	diva_shutdown_main_dpc ();

/* Ensure that no DPC stuff still hanging around ! Don't want
 * any nasty calls to functions which are being removed from
 * the kernel. Causes those panic'y things.
 */
	for (A = Machine.Adapters; A < Machine.lastAdapter; A++) {
		if ( A->EventDpc.Pending == TRUE ) {
			sysCancelDpc(&A->EventDpc);
		}
	}

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	{
		int i;

		for (i = 0; i < MAX_DIVA_TTY_MAJORS; i++) {
			if (driver_registered[i]) {
				tty_unregister_driver(&eicon_tty_driver[i]);
			}
		}
	}
#else /* } { */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,13,0)
	if (tty_unregister_driver(eicon_tty_driver)) {
		DBG_ERR(("Failed to unregister tty driver"))
	}
#else
	tty_unregister_driver(eicon_tty_driver);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
	tty_driver_kref_put(eicon_tty_driver);
#else
	put_tty_driver(eicon_tty_driver);
#endif
	eicon_tty_driver = 0;
#endif /* } */

	for (D = PortDriver.Ports; D < PortDriver.lastPort; D++) {
		if (D->Port) {
			if (D->Port->RxBuffer.Buffer) {
				sysHeapFree (D->Port->RxBuffer.Buffer);
			}
			if (D->Port->TxBuffer.Buffer) {
				sysHeapFree (D->Port->TxBuffer.Buffer);
			}
			sysHeapFree (D->Port);
		}
	}
	DiPort_Dynamic_Exit();

	sysHeapFree(mem_ptr);

	if (Machine.Memory) {
		sysPageFree (Machine.Memory);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
	{
		int i;

		for (i=0; i<MINORS_PER_MAJOR_DEFAULT; i++) {
			if (tty_port_initialized(&dsport[i])) {
				tty_port_destroy(&dsport[i]);
			}
		}
	}
#endif
	DBG_TRC(("TTY: UNLOAD COMPLETE"))

	disconnect_didd();
}

#endif /* MODULE */

unsigned long eicon_splimp (void) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

	local_bh_disable();
#if defined(DIVA_SMP)
	lock_kernel();
	return (0);
#else
	{
		ulong i;
		save_flags(i);
		cli();
		return (i);
	}
#endif

#else /* } { */

#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    down (&diva_tty_lock);
#else
    spin_lock_bh(&diva_tty_lock);
#endif
#else
	local_bh_disable();
	lock_kernel();
#endif

	return (0);
#endif /* } */
}

void eicon_splx (unsigned long i) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

#if defined(DIVA_SMP)
	unlock_kernel();
#else
	ulong f = (ulong)i;
	restore_flags(f);
#endif
	local_bh_enable();

#else /* } { */
#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
	up (&diva_tty_lock);
#else
    spin_unlock_bh(&diva_tty_lock);
#endif
#else
	unlock_kernel();
	local_bh_enable();
#endif
#endif /* } */
}

static void diva_set_port_notification (ser_dev_t *sd) {
	PortEnableNotification (sd->P, 0, 0);
	PortSetEventMask (sd->P, EV_RLSD, 0);
	PortEnableNotification (sd->P, diva_EvNotifyProc, sd);
}

static void diva_EvNotifyProc (void * Port,
															void* sd_ref,
															long type,
															long events) {
	ser_dev_t* sd = (ser_dev_t*)sd_ref;
  struct tty_struct *tty;

	if (!sd || !(tty = sd->ser_ttyp) || tty->closing ||
			(sd->dev_state != DEV_OPEN) ||
			!sd->P) {
		return;
	}

	if (type == CN_EVENT) {
		if (events & EV_RLSD) {
			uint msr = (uint)sd->MsrShadow;
			msr  = (msr & MS_RLSD_ON) != 0;
			if (sd->current_dcd_state != msr) {
				sd->current_dcd_state = msr;
				DBG_TRC(("[%p:%s] SET DCD %s (%d)",
						sd->P->Channel, sd->P->Name, msr ? "ON" : "OFF", sd->signal_dcd))
				if (sd->P->Mode != P_MODE_FAX) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
					if (!msr && tty->ldisc.flush_buffer) {
						(*(tty->ldisc.flush_buffer))(tty);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
					if (!msr && tty->ldisc.ops->flush_buffer) {
//						(*(tty->ldisc.ops->flush_buffer))(tty);
#else
					if (!msr && tty->ldisc->ops && tty->ldisc->ops->flush_buffer) {
//						(*(tty->ldisc->ops->flush_buffer))(tty);
#endif
#endif
					}
				}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
				lock_kernel();
#endif
				sd->dcd_changed_nr++;
				if (!msr && sd->signal_dcd) {
					tty_hangup (tty);
				}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
				unlock_kernel();
#endif
			}
		}
	}

}



static void diva_setsignals (ser_dev_t* sd, int dtr, int rts) {
	long in_data = 0;
  dword out_data = 0;


	if (!sd || !sd->P || !sd->ser_ttyp || sd->ser_ttyp->closing)
		return;

	switch (dtr) {
		case 0:
			DBG_TRC(("[%p:%s] DTR OFF", sd->P->Channel, sd->P->Name))
			PortEscapeFunction (sd->P, CLRDTR, in_data, &out_data);
			sd->idtr = 1;
			break;
		case 1:
			DBG_TRC(("[%p:%s] DTR ON", sd->P->Channel, sd->P->Name))
			PortEscapeFunction (sd->P, SETDTR, in_data, &out_data);
			sd->idtr = 0;
			break;
	}

	switch (rts) {
		case 0:
			PortEscapeFunction (sd->P, CLRRTS, in_data, &out_data);
			sd->irts = 1;
			break;

		case 1:
		 PortEscapeFunction (sd->P, SETRTS, in_data, &out_data);
			sd->irts = 0;
		 break;
	}
}

void diva_os_didd_read (DESCRIPTOR* dst, int* length) {
	int i;
	int len = *length;

	memset (dst, 0x00, len);
	len /= sizeof (DESCRIPTOR);

	for (i = 0; ((i < len) && (i < flat_descriptor_length)) ; i++) {
		memcpy (&dst[i], &flat_descriptor[i], sizeof (*dst));
	}

	*length = i;
}

#if 0
static void diva_delay(int len) {
	if (len > 0) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(len);
		current->state = TASK_RUNNING;
	}
}
#endif

#if 0
static void diva_waituntilsent(struct tty_struct *tty, int timeout) {
	ser_dev_t* portp;

	unsigned long	tend;

	if (tty == (struct tty_struct *) NULL)
		return;
	portp = tty->driver_data;
	if (!portp)
		return;

	if (!portp->P)
		return;

	if (timeout == 0)
		timeout = HZ;
	tend = jiffies + timeout;

	while (tty_isdn_chars_in_buffer(tty)) {
		if (signal_pending(current))
			break;
		diva_delay(2);
		if (time_after_eq(jiffies, tend))
			break;
	}
}
#endif

void diva_tty_os_sleep (dword mSec) {
	unsigned long timeout = HZ * mSec / 1000 + 1;

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(timeout);
	set_current_state(TASK_RUNNING);
}

/*
	TTY processing function that access TTY device with MINOR 0 and allows
	to pass different control commands to TTY

	command format:
	TTY_NR:command


	*/
static int process_control_tty_command (char* cmd, char* out, int limit) {
	byte* p = (byte*)cmd, *t;
	ser_dev_t *sd;
	int dev;
	int want_no_carrier;

	while (*p && (*p != ':')) {
		p++;
	}
	if (!p || !*p) {
		return (-1);
	}

	t = p;
	while (*t) {
		if ((*t == '\r') || (*t == '\n')) {
			*t = 0;
			break;
		}
		t++;
	}

	*p++ = 0;

	dev = str_2_int (cmd);
	if (!dev || dev > Channel_Count) {
		return (-1);
	}
	sd = &ser_devs[dev];
	if (!sd->ser_ttyp || !sd->P) {
		return (-1);
	}
	if (sd->dev_state != DEV_OPEN) {
		return (-1);
	}
	if (sd->ser_ttyp->closing) {
		return (-1);
	}

	DBG_TRC(("[%p:%s] control (%s)", sd->P->Channel, sd->P->Name, p))
	want_no_carrier = (sd->P->State != P_COMMAND);

	if				(!str_cmp(p, "dcd")) {
		if (want_no_carrier) {
			return (0);
		} else {
			return (-1);
		}
	} else if	(!str_cmp(p, "status") && out) {
		sprintf (out, "DCD %s ", (sd->current_dcd_state) ? "ON" : "OFF");
		create_status_message (sd->P, &out[str_len(out)], limit-strlen(out));
		return (str_len(out));
	} else if	(!str_cmp(p, "open")) {
			return (0);
	} else if	(*p == 'z') {
		p++;
		atHangup (sd->P, 0);
		atReset (sd->P, &p);
		if (want_no_carrier) {
			atRsp (sd->P, R_NO_CARRIER);
		}
	} else if (*p == 'h') {
		atHangup (sd->P, 0);
		if (want_no_carrier) {
			atRsp (sd->P, R_NO_CARRIER);
		}
	} else if (!str_cmp (p, "dtr")) {
		long in_data = 0;
		dword out_data = 0;
		PortEscapeFunction (sd->P, CLRDTR, in_data, &out_data);
	} else if ((*p == 'a') && (p[1] == 't') && !want_no_carrier) {
		p[str_len(p)+1]=0;
		p[str_len(p)]='\r';

		sd->P->control_operation_mode =  1;
		sd->P->control_response       = -1;
		atWrite (sd->P, p, str_len(p));
		sd->P->control_operation_mode =  0;
		if (sd->P->control_response) {
			return (-1);
		}
	} else {
		return (-1);
	}

	return (0);
}

ISDN_PORT* diva_tty_get_port (int i) {
	if (i && (i < (Channel_Count+1))) {
		return (ser_devs[i].P);
	}

	return (0);
}

static void diva_os_sleep (dword mSec) {
  unsigned long timeout = HZ * mSec / 1000 + 1;

  set_current_state(TASK_UNINTERRUPTIBLE);
  schedule_timeout(timeout);
}

static word diva_capi_create_mngt_read_req (byte* P, const char* path) {
  byte var_length;
  byte* plen;

  if ((var_length = (byte)strlen (path)) >= 128) {
    return (0);
  }

  *P++ = ESC;
  plen = P++;
  *P++ = 0x80; /* MAN_IE */
  *P++ = 0x00; /* Type */
  *P++ = 0x00; /* Attribute */
  *P++ = 0x00; /* Status */
  *P++ = 0x00; /* Variable Length */
  *P++ = var_length;
  memcpy (P, path, var_length);
  P += var_length;
  *plen = var_length + 0x06;
  *P++ = 0; /* terminate */

  return ((word)(var_length + 0x09));
}

static void diva_capi_send_mngt_req (diva_capi_mngt_ctxt_t* pE, byte Req, word length, int new_state) {
  ENTITY* e = &pE->e;

  e->XNum        = 1;
  e->X           = &pE->XData;
  e->Req         = Req;
  e->X->PLength  = length;
  e->X->P        = pE->xbuffer;
  pE->state      = new_state;

  (*(pE->d->request))(e);
}

static diva_man_var_header_t* get_next_var (diva_man_var_header_t* pVar) {
  byte* msg   = (byte*)pVar;
  byte* start;
  int msg_length;

  if (*msg != ESC) return (0);

  start = msg + 2;
  msg_length = *(msg+1);
  msg = (start+msg_length);

  if (*msg != ESC) return (0);

  return ((diva_man_var_header_t*)msg);
}

static diva_man_var_header_t* find_var (diva_man_var_header_t* pVar,
                                        const char* name) {
  const char* path;
  int i;

  do {
    path = (char*)&pVar->path_length+1;

    for (i = 0; (name[i] && (i < pVar->path_length) && (name[i] == path[i])); i++);
    if ((i >= pVar->path_length) && (name[i] == 0)) {
      return (pVar);
    }

  } while ((pVar = get_next_var (pVar)));

  return (pVar);
}

static void diva_capi_read_var (const diva_man_var_header_t* pVar, void* dst, int size) {
  if (pVar && dst && size) {
    byte* ptr     = (byte*)&pVar->path_length;
    int   use_int = 0;
    int   v_i     = 0;
    dword v_u     = 0;

    ptr += (pVar->path_length + 1);

    switch (pVar->type) {
      case 0x81: /* MI_INT    - signed integer */
        use_int = 1;
        switch (pVar->value_length) {
          case 1:
            v_i = *(char*)ptr;
            break;
          case 2:
            v_i = (int)READ_WORD(ptr);
            break;
          case 4:
            v_i = (int)READ_DWORD(ptr);
            break;
        }
        break;

      case 0x82: /* MI_UINT   - unsigned integer */
      case 0x83: /* MI_HINT   - unsigned integer, hex representetion */
      case 0x87: /* MI_BITFLD - unsigned integer, bit representation */
        switch (pVar->value_length) {
          case 1:
            v_u = *(byte*)ptr;
            break;
          case 2:
            v_u = READ_WORD(ptr);
            break;
          case 4:
            v_u = READ_DWORD(ptr);
            break;
        }
        break;

      case 0x85: /* MI_BOOLEAN */
        switch (pVar->value_length) {
          case 1:
            v_u = (*(byte*)ptr  != 0);
            break;
          case 2:
            v_u = READ_WORD(ptr) != 0;
            break;
          case 4:
            v_u = READ_DWORD(ptr) != 0;
            break;
        }
        break;

      default:
        DBG_FTL(("Variable type %02x not supported", pVar->type))
        break;
    }

    if (use_int) {
      switch(size) {
        case 1:
          *(char*)dst  = (char)v_i;
          break;
        case 2:
          *(short*)dst = (short)v_i;
          break;
        case 4:
          *(int*)dst   = (int)v_i;
          break;
      }
    } else {
      switch(size) {
        case 1:
          *(byte*)dst   = (byte)v_u;
          break;
        case 2:
          *(word*)dst   = (word)v_u;
          break;
        case 4:
          *(dword*)dst  = (dword)v_u;
          break;
      }
    }
  }
}

static void diva_capi_process_mngt_indication (diva_capi_mngt_ctxt_t* pE) {
  if (!--pE->pending_msg) {

    if (pE->xbuffer[0]) {
      int current_work_entry;
      for (current_work_entry = 0;
           pE->work_entries[pE->current_entry]->work_items[current_work_entry].name;
           current_work_entry++) {
        char* name = pE->work_entries[pE->current_entry]->work_items[current_work_entry].name;
        void* dst  = pE->work_entries[pE->current_entry]->work_items[current_work_entry].dst;
        int   size = pE->work_entries[pE->current_entry]->work_items[current_work_entry].size;

        diva_capi_read_var (find_var ((diva_man_var_header_t*)&pE->xbuffer[0], name), dst, size);
      }
    }

    if (pE->work_entries[++pE->current_entry]) {
      word length;

      pE->pending_msg = 2;
      DBG_LOG(("Read: '%s' directory", pE->work_entries[pE->current_entry]->directory_name))
      length = diva_capi_create_mngt_read_req (pE->xbuffer,
                                               pE->work_entries[pE->current_entry]->directory_name);
      diva_capi_send_mngt_req (pE, MAN_READ, length, pE->state + 1);
    } else {
      pE->xbuffer[0] = 0;
      diva_capi_send_mngt_req (pE, REMOVE, 1, -1);
    }
  }
}

static void diva_capi_mngt_callback (ENTITY* e) {
  diva_capi_mngt_ctxt_t* pE = (diva_capi_mngt_ctxt_t*)e;

  if (e->complete == 255) { /* Return code */
    switch (pE->state) {
      case 1: /* ASSIGN pending */
        if (e->Rc == ASSIGN_OK) {
          word length;
          DBG_LOG(("Read: '%s' directory", pE->work_entries[pE->current_entry]->directory_name))
          pE->pending_msg        = 2;
          length = diva_capi_create_mngt_read_req (pE->xbuffer,
                                                   pE->work_entries[pE->current_entry]->directory_name);
          diva_capi_send_mngt_req (pE, MAN_READ, length, 2);
        } else {
          DBG_FTL(("Management interface ASSIGN failed"))
          pE->state = 0;
        }
        break;

      case -1: /* REMOVE pending */
        if (e->Rc != 0xff) {
          DBG_FTL(("Management interface REMOVE failed (%02x)", e->Rc))
        }
        pE->state = 0;
        break;

      default: /* Assigned, information retrival state */
        if (e->Rc != 0xff) {
          DBG_ERR(("failed to read: '%s' directory (%02x)",
                  pE->work_entries[pE->current_entry]->directory_name, e->Rc))
          pE->pending_msg = 1;
          pE->xbuffer[0]  = 0;
        }
        diva_capi_process_mngt_indication (pE);
        break;
    }
  } else { /* Indication */
    if (pE->state > 1) { /* Assigned, information retrival state */
      if (e->complete != 2) { /* start copy indication */
        e->RNum       = 1;
        e->R          = &pE->RData;
        e->R->P       = &pE->xbuffer[0];
        e->R->PLength = sizeof(pE->xbuffer) - 1;
      } else {
        pE->xbuffer[e->R->PLength] = 0;
        diva_capi_process_mngt_indication (pE);
      }
    } else {
      e->RNum = 0;
      e->RNR  = 2;
    }

    e->Ind = 0;
  }
}
/*
  Read real channel cound from the adapter management interface
  */
static int diva_tty_read_channel_count (DESCRIPTOR* d, int* VsCAPI) {
	int channels = -1;

  if (d && d->request && (d->channels || (d->type == IDI_MADAPTER))) {
    diva_capi_mngt_ctxt_t* pE;

    if ((pE = diva_mem_malloc (sizeof(*pE)))) {
      ENTITY* e = &pE->e;
      int count = 500;
      int prev_state;
      diva_capi_mngt_work_item_t ch_items[] = { { "Info\\Channels", &channels, sizeof(channels) },
                                                { 0, 0, 0 } };
      diva_capi_mngt_work_item_t ch_items_mtpx[] = { { "Info\\Channels", &channels, sizeof(channels) },
																										 { "Info\\VsCAPI",   VsCAPI,   sizeof(*VsCAPI)   },
																										 { 0, 0, 0 } };
      diva_capi_mngt_work_entry_t info_entry = { "Info", ch_items };
      diva_capi_mngt_work_entry_t* entries[] = { &info_entry, 0 };

			if (d->type == IDI_MADAPTER && VsCAPI != 0)
				info_entry.work_items = ch_items_mtpx;

      memset (pE, 0x00, sizeof(*pE));
      pE->d = d;
      pE->work_entries = entries;

      e->Id          = MAN_ID;
      e->callback    = diva_capi_mngt_callback;
      diva_capi_send_mngt_req (pE, ASSIGN, 1, 1);
      prev_state = pE->state;
      diva_os_sleep(10);
      while (pE->state && --count) {
        diva_os_sleep(10);
        if (pE->state != prev_state) {
          prev_state = pE->state;
          count = 500;
        }
      }

      if (pE->state) {
        DBG_FTL(("Management entity removal failed"))
      }

      diva_mem_free (pE);
    }
	}

  return (channels);
}

static int diva_tty_read_cfg_value (IDI_SYNC_REQ* sync_req,
                                     int instance,
                                     const char* name,
                                     diva_cfg_lib_value_type_t value_type,
                                     void* data,
                                     dword data_length)
{
  diva_cfg_lib_return_type_t ret = DivaCfgLibNotFound;
	IDI_CALL request = diva_get_dadapter_request();

  memset (sync_req, 0x00, sizeof(*sync_req));
  sync_req->didd_get_cfg_lib_ifc.e.Rc = IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC;

	if (request) {
		request((ENTITY *)sync_req);
	}

  if (request && sync_req->didd_get_cfg_lib_ifc.e.Rc == 0xff)
  {
    diva_cfg_lib_access_ifc_t* cfg_lib_ifc = sync_req->didd_get_cfg_lib_ifc.info.ifc;

    if (cfg_lib_ifc != 0)
    {
      ret = cfg_lib_ifc->diva_cfg_storage_read_cfg_var_proc(cfg_lib_ifc,
                           TargetComDrv, instance, 0, 0, name,
                           value_type, &data_length, data);

      (*(cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(cfg_lib_ifc);

      if (ret == DivaCfgLibOK) {
        DBG_LOG(("TTY(%d) read %s configuration value", instance, name))
      } else {
        DBG_ERR(("TTY(%d) failed to read %s configuration value", instance, name))
      }
    }
  }

  return ((ret == DivaCfgLibOK) ? 0 : -1);
}

static void diva_tty_read_global_option(IDI_SYNC_REQ* sync_req, const char* name, int mask) {
	byte val = 0;

	if (diva_tty_read_cfg_value (sync_req, 0, name,
															 DivaCfgLibValueTypeBool, &val, sizeof(val)) == 0) {

		if (val != 0) {
			global_options |= mask;
		} else {
			global_options &= ~mask;
		}
	}
}

static void diva_tty_read_global_port_option(IDI_SYNC_REQ* sync_req, const char* name, int mask) {
	byte val = 0;

	if (diva_tty_read_cfg_value (sync_req, 0, name,
															 DivaCfgLibValueTypeBool, &val, sizeof(val)) == 0) {

		if (val != 0) {
			PortDriver.Fixes |= mask;
		} else {
			PortDriver.Fixes &= ~mask;
		}
	}
}



int diva_os_initialize_spin_lock (void** lock, void * unused) {
	spinlock_t* spin_lock = (void*)diva_mem_malloc (sizeof(*spin_lock));

	if (spin_lock != 0) {
		spin_lock_init (spin_lock);
		*lock = (void*)spin_lock;
		return (0);
	}

	return (-1);
}

void diva_os_destroy_spin_lock   (void** lock, void * unused) {
	void* p = *lock;

	if (p != 0) {
		diva_mem_free (p);
	}
}

void diva_os_enter_spin_lock (void** lock,
															diva_os_spin_lock_magic_t* old_irql,
															void* dbg) {
	spinlock_t* sl = (spinlock_t*)*lock;

#if defined(__KERNEL_VERSION_GT_2_4__)
	spin_lock_bh (sl);
#else
	unsigned long flags;

	save_flags(flags);
	cli();
	spin_lock(sl);

	*old_irql = flags;
#endif
}

void diva_os_leave_spin_lock (void** lock,
															diva_os_spin_lock_magic_t* old_irql,
															void* dbg) {
	spinlock_t* sl = (spinlock_t*)*lock;

#if defined(__KERNEL_VERSION_GT_2_4__)
	spin_unlock_bh (sl);
#else
	unsigned long flags = *old_irql;

	spin_unlock(sl);
	restore_flags (flags);
#endif
}

#include "syslnx.c"
