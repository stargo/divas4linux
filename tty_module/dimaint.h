
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

#ifndef _DIMAINT_INCLUDE_
#define _DIMAINT_INCLUDE_


/*........................................
  . DEC: CT: Fri Jul 19 17:02:16         .
  . Already defined in platform.h        .
  ........................................*/

#if !defined(DEC_UNIX) || (defined(DEC_UNIX) && !defined(_KERNEL)) 

#define byte unsigned char
#define word unsigned short
#define dword unsigned int
#if !defined(UNIXWARE)
#define TRUE 1
#define FALSE 0
#endif

#endif

#if defined(UNIX)
#define DI_GET (80)
#endif

#if defined(WINNT)
#define IOCTL_DIMAINT_LOG        CTL_CODE(0x8001U,6,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_DIMAINT_WRITE_LINE CTL_CODE(0x8001U,8,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define far
#define _loadds
#define pascal
#endif

struct l1s {
  short length;
  unsigned char i[22];
};

struct l2s {
  short code;
  short length;
  unsigned char i[20];
};

union par {
  char text[42];
  struct l1s l1;
  struct l2s l2;
};

typedef struct xlog_s XLOG;
struct xlog_s {
  short code;
  union par info;
};

typedef struct log_s LOG;
struct log_s {
  word length;
  word code;
  word timeh;
  word timel;
  byte buffer[80];
};


/*........................................
  . DEC: CT: Sat Jul 20 15:14:59         .
  . Strings are signed char on DEC       . 
  ........................................*/

#if defined DEC_UNIX
typedef void (* DI_PRINTF)(char *, ...); 

#elif defined(_AIX)
typedef void (_cdecl far _loadds * DI_PRINTF)(char far *, ...);

#elif defined(LINUX)
/*typedef void (_cdecl far _loadds * DI_PRINTF)(char far *, ...);*/

#elif defined(UNIXWARE) || defined(SCO)
typedef void (_cdecl far _loadds * DI_PRINTF)(char [], ...); 
#else
typedef void (_cdecl far _loadds * DI_PRINTF)(byte far *, ...); 
#endif /* UNIXWARE */ 

#endif
