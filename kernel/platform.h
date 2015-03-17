/* $Id: platform.h,v 1.35 2003/12/05 18:45:05 armin Exp $
 *
 * platform.h
 * 
 *
 * Copyright 2000-2009  by Armin Schindler (armin@melware.de)
 * Copyright 2000-2009  Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007  Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */


#ifndef	__PLATFORM_H__
#define	__PLATFORM_H__

#define CONFIG_CAPI_EICON 1
#define CONFIG_ISDN_DIVAS 1
#define CONFIG_ISDN_DIVAS_BRIPCI 1
#define CONFIG_ISDN_DIVAS_PRIPCI 1
#define CONFIG_ISDN_DIVAS_ANALOG 1
#define CONFIG_ISDN_DIVAS_DIVACAPI 1
#define CONFIG_ISDN_DIVAS_USERIDI 1
#define CONFIG_ISDN_DIVAS_MAINT 1

#if !defined(DIVA_BUILD)
#define DIVA_BUILD "local"
#endif

#include <linux/module.h>
#include <linux/version.h>
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

#define DIVA_INCLUDE_DISCONTINUED_HARDWARE 1
/* #define DIVA_XDI_AER_SUPPORT 1 */


#define OSCONST
#include "cardtype.h"

#define DIVA_INIT_FUNCTION  __init
#define DIVA_EXIT_FUNCTION  __exit

#define DIVA_USER_MODE_CARD_CONFIG 1
#define	USE_EXTENDED_DEBUGS 1

#define MAX_ADAPTER     64

#define DIVA_ISTREAM 1

#define MEMORY_SPACE_TYPE  0
#define PORT_SPACE_TYPE    1


#include <linux/string.h>

#ifndef	byte
#define	byte   u8
#endif

#ifndef	word
#define	word   u16
#endif

#ifndef	dword
#define	dword  u32
#endif

#ifndef	qword
#define	qword  u64
#endif

#ifndef int32
#define int32 s32
#endif

typedef const byte* pcbyte;

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

#ifndef	NULL
#define	NULL	((void *) 0)
#endif

#ifndef	MIN
#define MIN(a,b)	((a)>(b) ? (b) : (a))
#endif

#ifndef	MAX
#define MAX(a,b)	((a)>(b) ? (a) : (b))
#endif

#ifndef	far
#define far
#endif

#ifndef	_pascal
#define _pascal
#endif

#ifndef	_loadds
#define _loadds
#endif

#ifndef	_cdecl
#define _cdecl
#endif

#if !defined(DIM)
#define DIM(array)  (sizeof (array)/sizeof ((array)[0]))
#endif

#define DIVA_INVALID_FILE_HANDLE  ((dword)(-1))

#define DIVAS_CONTAINING_RECORD(address, type, field) \
        ((type *)((char*)(address) - (char*)(&((type *)0)->field)))
#define DIVAS_OFFSET_OF(type, field) ((dword)&(((type*)0)->field))
#define DIVAS_SIZE_OF(type, field) (sizeof(((type*)0)->field))

extern int sprintf(char *, const char*, ...);

typedef void* LIST_ENTRY;

typedef char    DEVICE_NAME[64];
typedef struct _ISDN_ADAPTER   ISDN_ADAPTER;
typedef struct _ISDN_ADAPTER* PISDN_ADAPTER;

typedef void (* DIVA_DI_PRINTF) (unsigned char *, ...);
extern DIVA_DI_PRINTF dprintf;
#include "debuglib.h"

#define dtrc(p) DBG_PRV0(p)
#define dbug(a,p) DBG_PRV1(p)


typedef struct e_info_s E_INFO ;

typedef char diva_os_dependent_devica_name_t[64];
typedef void* PDEVICE_OBJECT;

struct _diva_os_soft_isr;
struct _diva_os_timer;
struct _ISDN_ADAPTER;

void diva_log_info(unsigned char *, ...);

/*
**  XDI DIDD Interface
*/
void diva_xdi_didd_register_adapter (int card);
void diva_xdi_didd_remove_adapter (int card);

/*
** memory allocation
*/
#define DIVA_OS_MALLOC_ATOMIC 0x80
#define DIVA_MIN_VMALLOC (4*1024+1)

static __inline__ void* diva_os_malloc (unsigned long flags, unsigned long size)
{
	void *ret = NULL;

	if (size != 0) {
    if ((flags & DIVA_OS_MALLOC_ATOMIC) != 0) {
			ret = kmalloc (size, GFP_ATOMIC);
		} else {
			if (size < DIVA_MIN_VMALLOC) {
				ret = kmalloc(size+sizeof(int), GFP_KERNEL);
				if (ret != NULL) {
					((int *) ret)[0] = 0;
					ret = (void *)(((byte *) ret) + sizeof(int));
				}
			}
			if ((size >= DIVA_MIN_VMALLOC) || (ret == NULL)) {
				ret = (void *) vmalloc((unsigned int) (size+sizeof(int)));
				if (ret != NULL) {
					((int *) ret)[0] = 1;
					ret = (void *)(((byte *) ret) + sizeof(int));
				}
			}
		}
	}

	return (ret);
}

static __inline__ void  diva_os_free   (unsigned long flags, void* ptr)
{
	int i;
	if (ptr != 0) {
    if ((flags & DIVA_OS_MALLOC_ATOMIC) != 0) {
			kfree (ptr);
		} else {
			ptr = (void *)((byte *) ptr - sizeof(int));
			i = ((int *) ptr)[0];
			if (i==0) {
				kfree(ptr);
			} else {
				vfree(ptr);
			}
		}
	}
}

/*
** use skbuffs for message buffer
*/
typedef struct sk_buff diva_os_message_buffer_s;
diva_os_message_buffer_s *diva_os_alloc_message_buffer(unsigned long size, void **data_buf);
void diva_os_free_message_buffer(diva_os_message_buffer_s *dmb);
#define DIVA_MESSAGE_BUFFER_LEN(x) x->len
#define DIVA_MESSAGE_BUFFER_DATA(x) x->data

/*
** mSeconds waiting
*/
static __inline__ void diva_os_sleep(dword mSec)
{
	if (mSec != 0xffffffff) {
		unsigned long timeout = HZ * mSec / 1000 + 1;

		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(timeout);
	} else {
		yield();
	}
}
static __inline__ void diva_os_wait(dword mSec)
{
	mdelay(mSec);
}

/*
**  PCI Configuration space access
*/
void PCIwrite (byte bus, byte func, int offset, void* data, int length, void* pci_dev_handle);
void PCIread (byte bus, byte func, int offset, void* data, int length, void* pci_dev_handle);

/*
**  I/O Port utilities
*/
int diva_os_register_io_port (void *adapter, int register, unsigned long port,
				unsigned long length, const char* name);
/*
**  I/O port access abstraction
*/
byte inpp (void*);
word inppw (void*);
void inppw_buffer (void*, void*, int);
void outppw (void*, word);
void outppw_buffer (void* , void*, int);
void outpp (void*, word);

/*
**  IRQ 
*/
typedef struct _diva_os_adapter_irq_info {
        unsigned int irq_nr;
        int  registered;
        char irq_name[24];
} diva_os_adapter_irq_info_t;
int diva_os_register_irq (void* context, unsigned int irq, const char* name, int msi);
void diva_os_remove_irq (void* context, unsigned int irq);

#define diva_os_in_irq() in_irq()
#define diva_os_spinlocks_available() (irqs_disabled() == 0)

/*
**  Spin Lock framework
*/
typedef long diva_os_spin_lock_magic_t;
typedef spinlock_t diva_os_spin_lock_t;
static __inline__ int diva_os_initialize_spin_lock (spinlock_t *lock, void * unused) { \
  spin_lock_init (lock); return(0); }
static __inline__ void diva_os_enter_spin_lock (diva_os_spin_lock_t* a, \
                              diva_os_spin_lock_magic_t* old_irql, \
                              void* dbg) { spin_lock_bh(a); }
static __inline__ void diva_os_leave_spin_lock (diva_os_spin_lock_t* a, \
                              diva_os_spin_lock_magic_t* old_irql, \
                              void* dbg) { spin_unlock_bh(a); }

#define diva_os_destroy_spin_lock(a,b) do { } while(0)

/*
**  Deffered processing framework
*/
typedef int (*diva_os_isr_callback_t)(struct _ISDN_ADAPTER*);
typedef void (*diva_os_soft_isr_callback_t)(struct _diva_os_soft_isr* psoft_isr, void* context);

typedef struct _diva_os_soft_isr {
  void* object;
  diva_os_soft_isr_callback_t callback;
  void* callback_context;
  char dpc_thread_name[24];
} diva_os_soft_isr_t;

int diva_os_initialize_soft_isr (diva_os_soft_isr_t* psoft_isr, diva_os_soft_isr_callback_t callback, void*   callback_context);
int diva_os_schedule_soft_isr (diva_os_soft_isr_t* psoft_isr);
int diva_os_cancel_soft_isr (diva_os_soft_isr_t* psoft_isr);
void diva_os_remove_soft_isr (diva_os_soft_isr_t* psoft_isr);

/*
  Get time service
  */
void diva_os_get_time (dword* sec, dword* usec);

/*
**  atomic operation, fake because covered by os abstraction layer
*/
typedef int diva_os_atomic_t;
static int __inline__
diva_os_atomic_increment(diva_os_atomic_t* pv)
{
  *pv = *pv + 1;
  return (*pv);
}
static int __inline__
diva_os_atomic_decrement(diva_os_atomic_t* pv)
{
  *pv = *pv - 1;
  return (*pv);
}

/* 
**  CAPI SECTION
*/
#define NO_CORNETN
#define IMPLEMENT_DTMF 1
#define IMPLEMENT_LINE_INTERCONNECT2 1
#define IMPLEMENT_ECHO_CANCELLER 1
#define IMPLEMENT_RTP 1
#define IMPLEMENT_T38 1
#define IMPLEMENT_FAX_SUB_SEP_PWD 1
#define IMPLEMENT_V18 1
#define IMPLEMENT_DTMF_TONE 1
#define IMPLEMENT_PIAFS 1
#define IMPLEMENT_FAX_PAPER_FORMATS 1
#define IMPLEMENT_VOWN 1
#define IMPLEMENT_CAPIDTMF 1
#define IMPLEMENT_FAX_NONSTANDARD 1
#define VSWITCH_SUPPORT 1
#define IMPLEMENT_SS7 1

#define IMPLEMENT_LINE_INTERCONNECT  0
#define IMPLEMENT_MARKED_OK_AFTER_FC 1

#define DIVA_IDI_RX_DMA 1

#if !defined(__i386__)
#define READ_WORD(w) ( ((byte *)(w))[0] + \
                      (((byte *)(w))[1]<<8) )

#define READ_DWORD(w) ( ((byte *)(w))[0] + \
                       (((byte *)(w))[1]<<8) + \
                       (((byte *)(w))[2]<<16) + \
                       (((byte *)(w))[3]<<24) )

#define WRITE_WORD(b,w) do{ ((byte*)(b))[0]=(byte)(w); \
                          ((byte*)(b))[1]=(byte)((w)>>8); }while(0)

#define WRITE_DWORD(b,w) do{ ((byte*)(b))[0]=(byte)(w); \
                           ((byte*)(b))[1]=(byte)((w)>>8); \
                           ((byte*)(b))[2]=(byte)((w)>>16); \
                           ((byte*)(b))[3]=(byte)((w)>>24); }while(0)
#else
#define READ_WORD(w) (*(word *)(w))
#define READ_DWORD(w) (*(dword *)(w))
#define WRITE_WORD(b,w) do{ *(word *)(b)=(w); }while(0)
#define WRITE_DWORD(b,w) do{ *(dword *)(b)=(w); }while(0)
#endif

#ifdef BITS_PER_LONG
 #if BITS_PER_LONG > 32 
  #define PLATFORM_GT_32BIT
  /* #define ULongToPtr(x) (void *)(unsigned long)(x) */
 #endif
#endif

#undef N_DATA
#undef ADDR

#define diva_os_dump_file_t char
#define diva_os_board_trace_t char
#define diva_os_dump_file(__x__) do { } while(0)

#define PtrToUlong(__x__)  ((unsigned long)(__x__))
#define PtrToLong(__x__)   ((long)(__x__))
#define ULongToPtr(__x__)  ((byte*)(unsigned long)(__x__))
#define NumToPtr(__x__)  ((byte*)(unsigned long)(__x__))
#define PtrToUshort(__x__) ((word)(unsigned long)(__x__))
#define PtrToInt(__x__)    ((int)(long)(__x__))
#define IntToPtr(__x__)    ((void*)(long)(__x__))

#define DIVA_XDI_CFG_LIB_ONLY 1

#define DIVA_ADAPTER_TEST_SUPPORTED 1
#define  __DIVA_CFGLIB_IMPLEMENTATION__ 1

#define MAX_DESCRIPTORS 140

#define SETPROTOCOLLOAD 1

long strtol( const char *s, char **stop, unsigned int base);
int diva_snprintf (char* dst, int length, const char* format, ...);

void diva_os_read_descriptor_array (void *, int);

#define DIVA_XDI_USES_TASKLETS 1

#if defined(DIVA_UM_IDI_OS_DEFINES) /* { */
#define DIVA_UM_IDI_SYSTEM_ADAPTER_NR 0
#define DIVA_UM_IDI_ENTITY_CALLBACK_TYPE static
struct entity_s;
DIVA_UM_IDI_ENTITY_CALLBACK_TYPE void diva_um_idi_xdi_callback (struct entity_s* e);
DIVA_UM_IDI_ENTITY_CALLBACK_TYPE void diva_um_idi_master_xdi_callback (struct entity_s* e);
DIVA_UM_IDI_ENTITY_CALLBACK_TYPE void diva_um_idi_license_xdi_callback (struct entity_s* e);

static __inline__ void diva_um_idi_xdi_os_callback (struct entity_s* e) {
	diva_um_idi_xdi_callback (e);
}

static __inline__ void diva_um_idi_master_xdi_os_callback  (struct entity_s* e) {
	diva_um_idi_master_xdi_callback (e);
}

static __inline__ void diva_um_idi_license_xdi_os_callback (struct entity_s* e) {
	diva_um_idi_license_xdi_callback (e);
}

struct _divas_um_idi_proxy;
struct entity_s;

static __inline__ void diva_os_diva_um_proxy_user_request (void (*fn)(struct _divas_um_idi_proxy*, struct entity_s*),
																													 struct _divas_um_idi_proxy* pE,
																													 struct entity_s* e) {
	(*fn)(pE, e);
}

#endif /* } */

/* #define DIVA_EICON_CAPI 1 */
#define MODULDRIVERDEBUG 0

#define DEBUGLIB_CALLING_CONVENTION

static __inline__ int diva_os_save_file (dword location, const char* name, const void* data, dword data_length) {
  return (-1);
}

#endif	/* __PLATFORM_H__ */
