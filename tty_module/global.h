
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

#if !defined(_GLOBAL_H)
#define _GLOBAL_H

#include "unix.h"

#if defined(_INKERNEL)
#include "externs.h"
#if !defined(UNIXWARE)
extern void EtdK_printf(char *, ...);
extern void EtdK_sprintf(char *,char *, ...);
#endif
extern void EtdP_lock_fn(fn_t *,void *);
#define _i_sync(a,b,c) EtdP_lock_fn(b,c)
#endif /*unix.h*/

#define _cdecl
#define _fastcall
#define far
#define _pascal
#define _loadds
#define IO_ADDRESS word
#define bcopy bcopy

#endif  /*GLOBAL_H*/
