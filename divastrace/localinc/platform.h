
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
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

#ifndef __DIVA_TRACE_API_PLATFORM_H__
#define __DIVA_TRACE_API_PLATFORM_H__

#include <stdlib.h>
#include <string.h>

typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned int   dword;

#define OSCONST

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

#define TRUE  (1)
#define FALSE (0)

#ifndef MAX
#define MAX(__x__,__y__) (((__x__) > (__y__)) ? (__x__) : (__y__))
#endif
#ifndef MIN
#define MIN(__x__,__y__) (((__x__) > (__y__)) ? (__y__) : (__x__))
#endif

typedef int DIVA_OS_HANDLE;
typedef const byte* pcbyte;
void* diva_os_malloc (dword flags, dword length);
void diva_os_free (dword flags, void* mem);
#define __cdecl
#define far
#define DIVA_OS_INVALID_HANDLE ((void*)-1)
#define DIVA_OS_INVALID_HANDLE_CFG (-1)
#define INVALID_HANDLE_VALUE_CFG DIVA_OS_INVALID_HANDLE_CFG
/*#define DIVA_CFG_LIB_APPLICATION_OWNER 0xf */
#define DIVA_CFG_LIB_APPLICATION_OWNER TargetDivalog

typedef void* HKEY;
typedef int* HANDLE;
#define WINAPI
typedef dword DWORD;
typedef const char* LPTSTR;
typedef int BOOL;
#define INVALID_HANDLE_VALUE DIVA_OS_INVALID_HANDLE
typedef int SERVICE_STATUS;
typedef int SERVICE_STATUS_HANDLE;
typedef word TCHAR;
#define TEXT(__x__) __x__
#define _stricmp strcasecmp

#define MAXIMUM_WAIT_OBJECTS 512

#define CreateEvent(__a__, __b__, __c__, __d__) malloc(sizeof(int))
#define CloseHandle(__a__) free(__a__)

#define HEAP_ZERO_MEMORY 1
#define GetProcessHeap() (1)
static inline void* HeapAlloc(dword __a__, dword __b__, dword __c__) {
	void* ret = malloc((__c__));
	if (ret && ((__b__) & HEAP_ZERO_MEMORY)) {
		memset (ret, 0x00, (__c__));
	}

	return (ret);
}

#define HeapFree(__a__, __b__, __c__) free((__c__))

#define DEFAULT_LOG_FILE_LOG "/var/log/divalog"
#define MAX_PATH 2048
#define SERVICE_START_PENDING 1
#define SERVICE_RUNNING       1
#define NO_ERROR              0

#define DeleteFile(__x__) unlink(__x__)

#define __DIVA_OS_IMPLEMENT_GET_TIME_INFO__ 1
int diva_os_get_time_info (dword* t_sec, dword* t_sec_fractional, dword* tz_offset, int* dst_active, int* synchronized);


#endif

