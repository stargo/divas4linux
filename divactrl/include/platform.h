
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
  Copyright (c) Dialogic(R), 2004-2017
  Copyright 2000-2003 by Armin Schindler (mac@melware.de)
  Copyright 2000-2003 Cytronics & Melware (info@melware.de)

 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of Adapters.
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

#ifndef	__PLATFORM_H__
#define	__PLATFORM_H__

#define DIVA_INCLUDE_DISCONTINUED_HARDWARE 1
#define DIVA_XDI_VIRTUAL_FLAT_MEMORY 1

#define OSCONST
#include "cardtype.h"
#include <stdint.h>

#define __inline inline

typedef struct {  
unsigned long LowPart;  
long HighPart;  
} LARGE_INTEGER;  

#define	USE_EXTENDED_DEBUGS

#define MAX_ADAPTER     32

#define IMPLEMENT_DTMF 1
#define IMPLEMENT_LINE_INTERCONNECT 1
#define IMPLEMENT_ECHO_CANCELLER 1
#define IMPLEMENT_RTP 0
#define NO_CORNETN
#define IMPLEMENT_T38 0

#define MEMORY_SPACE_TYPE  0
#define PORT_SPACE_TYPE    1

#define DIVA_IDI_RX_DMA 1

#ifndef	byte
#define	byte   unsigned char 
#endif

#ifndef	word
#define	word   unsigned short
#endif

#ifndef	dword
#define	dword  unsigned int
#endif

#ifndef int32
#define int32  int
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

typedef void* LIST_ENTRY;

typedef char    DEVICE_NAME[64];
typedef struct _ISDN_ADAPTER   ISDN_ADAPTER;
typedef struct _ISDN_ADAPTER* PISDN_ADAPTER;

typedef void (* DIVA_DI_PRINTF) (char *, ...);
extern DIVA_DI_PRINTF __diva_dprintf;

#ifndef __NO_STANDALONE_DBG_INC__
#define DBG_TEST(func,args) __diva_dprintf args ;

#define DBG_LOG(args)  DBG_TEST(LOG, args)
#define DBG_FTL(args)  DBG_TEST(FTL, args)
#define DBG_ERR(args)  DBG_TEST(ERR, args)
#define DBG_TRC(args)  DBG_TEST(TRC, args)
#define DBG_MXLOG(args)  DBG_TEST(MXLOG, args)
#define DBG_FTL_MXLOG(args) DBG_TEST(FTL_MXLOG, args)
#define DBG_EVL(args)  DBG_TEST(EVL, args)
#define DBG_REG(args)  DBG_TEST(REG, args)
#define DBG_MEM(args)  DBG_TEST(MEM, args)
#define DBG_SPL(args)  DBG_TEST(SPL, args)
#define DBG_IRP(args)  DBG_TEST(IRP, args)
#define DBG_TIM(args)  DBG_TEST(TIM, args)
#define DBG_BLK(args)  DBG_TEST(BLK, args)
#define DBG_TAPI(args)  DBG_TEST(TAPI, args)
#define DBG_NDIS(args)  DBG_TEST(NDIS, args)
#define DBG_CONN(args)  DBG_TEST(CONN, args)
#define DBG_STAT(args)  DBG_TEST(STAT, args)
#define DBG_SEND(args)  DBG_TEST(SEND, args)
#define DBG_RECV(args)  DBG_TEST(RECV, args)
#define DBG_PRV0(args)  DBG_TEST(PRV0, args)
#define DBG_PRV1(args)  DBG_TEST(PRV1, args)
#define DBG_PRV2(args)  DBG_TEST(PRV2, args)
#define DBG_PRV3(args)  DBG_TEST(PRV3, args)
#endif

typedef struct e_info_s E_INFO ;

typedef char diva_os_dependent_devica_name_t[64];
typedef void* PDEVICE_OBJECT;

struct _diva_os_soft_isr;
struct _diva_os_timer;
struct _ISDN_ADAPTER;

void diva_log_info(unsigned char *, ...);

#define DIVA_OS_MALLOC_ATOMIC 0x80

/*
** memory allocation
*/
void* diva_os_malloc (unsigned long flags, unsigned long size);
void    diva_os_free     (unsigned long flags, void* ptr);

/*
** mSeconds waiting
*/
void diva_os_wait(dword mSec);
void diva_os_sleep(dword mSec);

/*
**  I/O port access abstraction
*/
byte inpp (void*);
word inppw (void*);
void inppw_buffer (void*, void*, int);
void outppw (void*, word);
void outppw_buffer (void* , void*, int);
void outpp (void*, word);

extern word ntohs (word);

/*
**  IRQ 
*/
typedef struct _diva_os_adapter_irq_info {
        byte irq_nr;
        int  registered;
        char irq_name[24];
} diva_os_adapter_irq_info_t;
int diva_os_register_irq (void* context, byte irq, const char* name);
void diva_os_remove_irq (void* context, byte irq);
void diva_os_i_sync (diva_os_adapter_irq_info_t*, void (*fn)(void*), void*);

/*
**  Spin Lock framework
*/

typedef long diva_os_spin_lock_magic_t;

#define diva_os_spin_lock_t long 
static __inline__ int diva_os_initialize_spin_lock(long *lock, void * unused)
{
  return(0);
}
#define diva_os_destroy_spin_lock(a,b)
void diva_os_enter_spin_lock (diva_os_spin_lock_t* pspin,
                         diva_os_spin_lock_magic_t* pmagic,
                         void* debug);
void diva_os_leave_spin_lock (diva_os_spin_lock_t* pspin,
                         const diva_os_spin_lock_magic_t* pmagic,
                         void* debug);

/*
**  Deffered processing framework
*/
typedef int (*diva_os_isr_callback_t)(struct _ISDN_ADAPTER*);
typedef void (*diva_os_soft_isr_callback_t)(struct _diva_os_soft_isr* psoft_isr, void* context);

typedef struct _diva_os_soft_isr {
  void* object;
  diva_os_soft_isr_callback_t callback;
  void* callback_context;
} diva_os_soft_isr_t;

int diva_os_initialize_soft_isr (diva_os_soft_isr_t* psoft_isr, diva_os_soft_isr_callback_t callback, void*   callback_context);
int diva_os_schedule_soft_isr (diva_os_soft_isr_t* psoft_isr);
int diva_os_cancel_soft_isr (diva_os_soft_isr_t* psoft_isr);
void diva_os_remove_soft_isr (diva_os_soft_isr_t* psoft_isr);

/*
**  Atomic operation framework
*/
#define diva_os_atomic_t long 
long diva_os_atomic_increment(diva_os_atomic_t *);
long diva_os_atomic_decrement(diva_os_atomic_t *);

#if !defined(__i386__)
#define READ_WORD(w) ( ((byte *)(w))[0] + \
                      (((byte *)(w))[1]<<8) )

#define READ_DWORD(w) ( ((byte *)(w))[0] + \
                       (((byte *)(w))[1]<<8) + \
                       (((byte *)(w))[2]<<16) + \
                       (((byte *)(w))[3]<<24) )

#define WRITE_WORD(b,w) { (b)[0]=(byte)(w); \
                          (b)[1]=(byte)((w)>>8); }

#define WRITE_DWORD(b,w) { (b)[0]=(byte)(w); \
                           (b)[1]=(byte)((w)>>8); \
                           (b)[2]=(byte)((w)>>16); \
                           (b)[3]=(byte)((w)>>24); } 
#else
#define READ_WORD(w) (*(word *)(w))
#define READ_DWORD(w) (*(dword *)(w))
#define WRITE_WORD(b,w) { *(word *)(b)=(w); }
#define WRITE_DWORD(b,w) { *(dword *)(b)=(w); }
#endif

int xdiSetProtocol (void*, void*);

unsigned short xlog_stream (void* out, void* buffer);

#define diva_os_dump_file_t char
#define diva_os_board_trace_t char
#define diva_os_dump_file(__x__) do { } while(0)

#define PtrToUlong(__x__)  ((dword      )(unsigned long)(__x__))
#define PtrToLong(__x__)   ((long       )(__x__))
#define ULongToPtr(__x__)  ((byte*      )(unsigned long)(__x__))
#define NumToPtr(__x__)    ((byte*)(unsigned long)(__x__))
#define PtrToUshort(__x__) ((word)(dword)(unsigned long)(__x__))
#define IntToPtr(__x__)    ((byte*      )(unsigned long)(__x__))

#define SETPROTOCOLLOAD 1

#include <string.h>
#include <unistd.h>


#define MAX_DESCRIPTORS 140

void diva_os_read_descriptor_array (void *, int);

/*
**  PCI Configuration space access
*/
static void inline PCIwrite (byte bus, byte func, int offset, void* data, int length, void* pci_dev_handle) {}
static void inline PCIread (byte bus, byte func, int offset, void* data, int length, void* pci_dev_handle)  {}
static inline uint16_t ioread16(const volatile void *addr) { return 0; }
static inline uint32_t ioread32(const volatile void *addr) { return 0;}
static inline void iowrite16(uint16_t value, volatile void *addr) {}
static inline void iowrite32(uint32_t value, volatile void *addr) {}

#endif	/* __PLATFORM_H__ */

