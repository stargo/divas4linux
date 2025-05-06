
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

#ifndef __DIVA_OS_SYS_MAN_H__
#define __DIVA_OS_SYS_MAN_H__

#include "debuglib.h"
#include "sys_if.h"

#if !defined(memcpy)
#define memcpy mem_cpy
#endif
#if !defined(strlen)
#define strlen str_len
#endif
#if !defined(strcpy)
#define strcpy str_cpy
#endif
#if !defined(memset)
#define memset mem_set
#endif

struct _diva_os_soft_isr;
typedef struct _diva_os_soft_isr {
	char dpc_thread_name[24];
	sys_DPC dpc;
	void (*dpc_proc)(struct _diva_os_soft_isr* psoft_isr, void* Context);
	void* context;
} diva_os_soft_isr_t;

static void man_drv_sys_callback (void *context) {
	diva_os_soft_isr_t* pDpc = (diva_os_soft_isr_t*)context;
	(*(pDpc->dpc_proc))(pDpc, pDpc->context);
}


static int diva_os_initialize_soft_isr (diva_os_soft_isr_t* pDpc, 
																				void (*dpc_proc)(diva_os_soft_isr_t*,void*),
																				void* context) {
	pDpc->dpc_proc = dpc_proc;
	pDpc->context  = context;
	sysInitializeDpc (&pDpc->dpc, man_drv_sys_callback, pDpc);

	return (0);
}


static void diva_os_remove_soft_isr (diva_os_soft_isr_t* pDpc) {
	sysCancelDpc   (&pDpc->dpc);
}

#define diva_os_schedule_soft_isr(__x__) sysScheduleDpc(&((__x__)->dpc))

int diva_os_initialize_spin_lock (diva_os_spin_lock_t* lock, void * unused);
void diva_os_destroy_spin_lock   (diva_os_spin_lock_t* lock, void * unused);
void diva_os_enter_spin_lock (diva_os_spin_lock_t* lock,
															diva_os_spin_lock_magic_t* old_irql,
															void* dbg);
void diva_os_leave_spin_lock (diva_os_spin_lock_t* lock,
															diva_os_spin_lock_magic_t* old_irql,
															void* dbg);
#endif

