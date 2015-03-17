
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

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
//#include <linux/module.h>
//#include <linux/init.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/vmalloc.h>
#include <linux/sched.h>

#if !defined(__KERNEL_VERSION_GT_2_4__)

#include <linux/isdn.h>
#include <linux/isdnif.h>
#include <linux/isdn_compat.h>
#include "divainclude/platform.h"

#else

#include "divainclude/platform.h"

#endif

#include "di_defs.h"
#include "divasync.h"

typedef void (*DIVA_DIDD_Read_t)(DESCRIPTOR *, int);
extern DIVA_DIDD_Read_t DIVA_DIDD_Read_fn;
extern void diva_adapter_change (IDI_CALL request, int insert);
extern int diva_mtpx_adapter_detected;

static DESCRIPTOR MAdapter;
static DESCRIPTOR DAdapter;
static dword notify_handle;

void no_printf (unsigned char *, ...);

DIVA_DI_PRINTF dprintf = no_printf;
#ifndef MAX_DESCRIPTORS
#define MAX_DESCRIPTORS 140
#endif
static void didd_callback(void *context, DESCRIPTOR* adapter, int removal);
static char* DRIVERRELEASE = DIVA_BUILD;
#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)

int connect_didd(void) {
  int x = 0;
  int dadapter = 0;
  IDI_SYNC_REQ req;
  static DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

  if (!DIVA_DIDD_Read_fn) {
		return (0);
	}

  DIVA_DIDD_Read_fn(DIDD_Table, sizeof(DIDD_Table));

  for (x = 0; x < MAX_DESCRIPTORS; x++)
  {
    if (DIDD_Table[x].type == IDI_DADAPTER)
    {  /* DADAPTER found */
      dadapter = 1;
      memcpy(&DAdapter, &DIDD_Table[x], sizeof(DAdapter));
      req.didd_notify.e.Req = 0;
      req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY;
      req.didd_notify.info.callback = didd_callback;
      req.didd_notify.info.context = 0;
      DAdapter.request((ENTITY *)&req);
      if (req.didd_notify.e.Rc != 0xff)
        return(0);
      notify_handle = req.didd_notify.info.handle;
    }
    else if (DIDD_Table[x].type == IDI_DIMAINT)
    {  /* MAINT found */
      memcpy(&MAdapter, &DIDD_Table[x], sizeof(DAdapter));
      dprintf = (DIVA_DI_PRINTF)MAdapter.request;
      DbgRegister("TTY", DRIVERRELEASE, DBG_DEFAULT);
    }
    else if ((DIDD_Table[x].type > 0) &&
             (DIDD_Table[x].type < 16))
    {  /* IDI Adapter found */
    }
  }
  return(dadapter);
}

static void didd_callback(void *context, DESCRIPTOR* adapter, int removal) {
  if (adapter->type == IDI_MADAPTER) {
		if (removal) {
    	DBG_ERR(("Notification about IDI_MADAPTER change ! Oops."));
		}
    return;
	} else if (adapter->type == IDI_DADAPTER) {
    DBG_ERR(("Notification about IDI_DADAPTER change ! Oops."));
    return;
  } else if (adapter->type == IDI_DIMAINT) {
    if (removal)
    {
      DbgDeregister();
      memset(&MAdapter, 0, sizeof(MAdapter));
      dprintf = no_printf;
    }
    else
    {
      memcpy(&MAdapter, adapter, sizeof(MAdapter));
      dprintf = (DIVA_DI_PRINTF)MAdapter.request;
      DbgRegister("TTY", DRIVERRELEASE, DBG_DEFAULT);
    }
  } else if ((adapter->type > 0) && (adapter->type < 16)) {
		/*
			IDI Adapter
			*/
		if (!diva_mtpx_adapter_detected) {
    	diva_adapter_change (adapter->request, !removal);
		}
  }
  return;
}

void disconnect_didd(void) {
  if (DIVA_DIDD_Read_fn) {
  	IDI_SYNC_REQ req;

  	req.didd_notify.e.Req = 0;
  	req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
  	req.didd_notify.info.handle = notify_handle;
  	DAdapter.request((ENTITY *)&req);
    DbgDeregister();
  }
}

void diva_os_add_adapter_descriptor (DESCRIPTOR* d, int operation) {
  IDI_SYNC_REQ req;

  if (operation) { /* Add    */
    req.didd_remove_adapter.e.Req = 0;
    req.didd_add_adapter.e.Rc = IDI_SYNC_REQ_DIDD_ADD_ADAPTER;
    req.didd_add_adapter.info.descriptor = (void *)d;
    DAdapter.request((ENTITY *)&req);
  } else {         /* Remove */
    req.didd_remove_adapter.e.Req = 0;
    req.didd_remove_adapter.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER;
    req.didd_remove_adapter.info.p_request = d->request;
    DAdapter.request((ENTITY *)&req);
  }
}

void no_printf (unsigned char * x ,...)
{
  /* dummy debug function */
}

IDI_CALL diva_get_dadapter_request(void) {
	return (DAdapter.request);
}

void DbgDeregisterH (_DbgHandle_ *pDebugHandle);
#include "debuglib.c"
