
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

#if !defined(_PLATFORM_H_)
#define _PLATFORM_H_
#if defined(UNIX)
#include "global.h"
#endif


#ifndef DEC_UNIX

#define byte unsigned char
#define word unsigned short
#define dword unsigned int
/** ISW 18/07/1996 - dont redefine these **/
#if !defined(_AIX) && !defined(UNIXWARE)
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)>(b) ? (b) : (a))
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#endif
#endif

typedef void (*ENTRY)(void);

typedef const byte* pcbyte;

/*------------------------------------------------------------------*/
/* configuration parameters                                         */
/*------------------------------------------------------------------*/

#if defined(FAR_BUFFER) || defined(CW)
#define MAX_PC          64
#define MAX_LL          128
#else
#define MAX_PC          16
#define MAX_LL          10
#endif

#define MAX_TEL         2
#define MAX_DL          4
#define FC_LEVEL        150

#if defined(OS2)
#define OSTATIC static
#define OFAR far
#else
#define OSTATIC
#define OFAR
#endif

#if defined(VXD)
#undef far
#undef _pascal
#undef _loadds

#define _fastcall
#define far
#define _pascal
#define _loadds

typedef unsigned size_t;
#define bcopy(p2,p1,n)         memcpy(p1,p2,n)
#define IO_ADDRESS word

#define REF_PARAMETER(p)
#endif

#if defined(WINNT)
#define _cdecl
#define _fastcall
#define far
#define _pascal
#define _loadds
#define bcopy(p2,p1,n)         memcpy(p1,p2,n)
#define IO_ADDRESS unsigned long
#endif

#if defined(MIPS)

#define _cdecl
#define _fastcall
#define far
#define _pascal
#define _loadds

#define READ_WORD(w) (word)( ((byte far *)(w))[0] + \
                            (((byte far *)(w))[1]<<8) )

#define READ_DWORD(w) (dword)( ((byte far *)(w))[0] + \
                              (((byte far *)(w))[1]<<8) + \
                              (((byte far *)(w))[2]<<16) + \
                              (((byte far *)(w))[3]<<24) )

#define WRITE_WORD(b,w) { (b)[0]=(byte)w; (b)[1]=(byte)(w>>8); }

#define WRITE_DWORD(b,w) { (b)[0]=(byte)w; \
                           (b)[1]=(byte)(w>>8); \
                           (b)[2]=(byte)(w>>16); \
                           (b)[3]=(byte)(w>>24); }

#define bcopy(p2,p1,n)         memcpy(p1,p2,n)

#elif defined(MIPS_EB)

#define _cdecl
#define _fastcall
#define far
#define _pascal
#define _loadds

#define READ_WORD(w) (word)( ((byte far *)(w))[1] + \
                            (((byte far *)(w))[0]<<8) )

#define READ_DWORD(w) (dword)( ((byte far *)(w))[3] + \
                              (((byte far *)(w))[2]<<8) + \
                              (((byte far *)(w))[1]<<16) + \
                              (((byte far *)(w))[0]<<24) )

#define WRITE_WORD(b,w) { (b)[1]=(byte)w; (b)[0]=(byte)(w>>8); }

#define WRITE_DWORD(b,w) { (b)[3]=(byte)w; \
                           (b)[2]=(byte)(w>>8); \
                           (b)[1]=(byte)(w>>16); \
                           (b)[0]=(byte)(w>>24); }

#define bcopy(p2,p1,n)         memcpy(p1,p2,n)

#elif ( defined(WINNT) && defined(_PPC_) ) || defined(_AIX)

#define READ_WORD(w) (word)( ((byte far *)(w))[0] + \
                            (((byte far *)(w))[1]<<8) )

#define READ_DWORD(w) (dword)( ((byte far *)(w))[0] + \
                              (((byte far *)(w))[1]<<8) + \
                              (((byte far *)(w))[2]<<16) + \
                              (((byte far *)(w))[3]<<24) )

#define WRITE_WORD(b,w) { (b)[0]=(byte)w; (b)[1]=(byte)(w>>8); }

#define WRITE_DWORD(b,w) { (b)[0]=(byte)w; \
                           (b)[1]=(byte)(w>>8); \
                           (b)[2]=(byte)(w>>16); \
                           (b)[3]=(byte)(w>>24); }

#else

#if defined(__KERNEL_VERSION_GT_2_4__) /* { */

#define READ_WORD(w) (word)( ((byte *)(w))[0] + \
                            (((byte *)(w))[1]<<8) )

#define READ_DWORD(w) (dword)( ((byte *)(w))[0] + \
                              (((byte *)(w))[1]<<8) + \
                              (((byte *)(w))[2]<<16) + \
                              (((byte *)(w))[3]<<24) )

#define WRITE_WORD(b,w) { (b)[0]=(byte)w; (b)[1]=(byte)(w>>8); }

#define WRITE_DWORD(b,w) { (b)[0]=(byte)w; \
                           (b)[1]=(byte)(w>>8); \
                           (b)[2]=(byte)(w>>16); \
                           (b)[3]=(byte)(w>>24); }

#else /* } { */

#define READ_WORD(w) (*(word far *)(w))
#define READ_DWORD(w) (*(dword far *)(w))
#define WRITE_WORD(b,w) { *(word far *)(b)=w; }
#define WRITE_DWORD(b,w) { *(dword far *)(b)=w; }

#endif /* } */

#endif

#define PtrToLong(__x__) ((long)(__x__))
#define IntToPtr(__x__)  ((void*)(long)(__x__))
#define PtrToInt(__x__)  ((int)(long)(__x__))

#if 0
/**
 **
 ** New macro to perform action conditionally whether it is a BIG_ENDIAN
 ** or LITTLE_ENDIAN architecture.
 **
 ** The idea is to allow for modifications for BIG_ENDIAN machines
 ** without modifying the existing LITTLE_ENDIAN code
**/

#if BYTE_ORDER == BIG_ENDIAN
#define	ONBIGLITTLE(a,b)	a
#else
#define	ONBIGLITTLE(a,b)	b
#endif
#endif

/*........................................
  . DEC: CT: Wed Jul 17 16:53:46         .
  . Include DEC Platform Specific        .
  ........................................*/

#ifdef DEC_UNIX
#include <dec_plat.h>
#endif

#if !defined(IO_ADDRESS)
#define IO_ADDRESS word
#endif

#if !defined(bcopy)
void bcopy(byte far * src, byte far * dest, word length);
#endif

#if !defined(clear)
void clear(byte * buffer, word length);
#endif

#if !defined(alloc)
#define alloc(a) 0
#endif

#if !defined(REF_PARAMETER)
#define REF_PARAMETER(p) (p)=(p)
#endif

/** ISW 18/07/1996 - include AIX utilities file **/
#if defined(_AIX)
#include "utilaix.h"
#endif

#include "dlist.h"

typedef void* diva_os_spin_lock_t;
typedef unsigned long diva_os_spin_lock_magic_t;


#if defined(__KERNEL_VERSION_GT_2_4__)
#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,25)
#define CONFIG_PREEMPT_BKL=y
#endif
#if defined(CONFIG_PREEMPT_BKL) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#define DIVA_USES_MUTEX 1
#endif
#endif


#endif /** _PLATFORM_H_ **/
