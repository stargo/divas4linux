
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
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

#include "platform.h"
#include "di_defs.h"
#include "divasync.h"
#include "um_xdi.h"
#include "um_idi.h"

#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)

extern char *DRIVERRELEASE_IDI;

extern void DIVA_DIDD_Read(void *, int);
extern int diva_user_mode_idi_create_adapter(const DESCRIPTOR *, int);
extern void diva_user_mode_idi_remove_adapter(int);

static dword notify_handle;
static DESCRIPTOR DAdapter;
static DESCRIPTOR MAdapter;
static atomic_t diva_user_mode_idi_disable_adapter_insertion;

static void no_printf(unsigned char *x, ...)
{
	/* dummy debug function */
}

DIVA_DI_PRINTF dprintf = no_printf;

#include "debuglib.c"

/*
 * stop debug
 */
static void stop_dbg(void)
{
	DbgDeregister();
	memset(&MAdapter, 0, sizeof(MAdapter));
	dprintf = no_printf;
}

/*
 * new card
 */
static void um_new_card(DESCRIPTOR * d)
{
	int adapter_nr = 0;
	IDI_SYNC_REQ sync_req;

  if (atomic_read(&diva_user_mode_idi_disable_adapter_insertion) != 0) {
    return;
  }

	sync_req.xdi_logical_adapter_number.Req = 0;
	sync_req.xdi_logical_adapter_number.Rc =
	    IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;

	d->request((ENTITY *) & sync_req);

	adapter_nr =
	    sync_req.xdi_logical_adapter_number.info.logical_adapter_number;

	if (diva_user_mode_idi_create_adapter(d, adapter_nr)) {
		DBG_ERR(("could not create user mode idi card %d",
			 adapter_nr));
	}
}

/*
 * remove card
 */
static void um_remove_card(DESCRIPTOR * d)
{
	int adapter_nr = 0;
	IDI_SYNC_REQ sync_req;

	sync_req.xdi_logical_adapter_number.Req = 0;
	sync_req.xdi_logical_adapter_number.Rc =
	    IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;

	d->request((ENTITY *) & sync_req);

	adapter_nr =
	    sync_req.xdi_logical_adapter_number.info.logical_adapter_number;

	diva_user_mode_idi_remove_adapter(adapter_nr);
	DBG_LOG(("idi entry removed for card %d", adapter_nr))
}

/*
 * DIDD notify callback
 */
static void didd_callback(void *context, DESCRIPTOR * adapter, int removal)
{
	if (adapter->type == IDI_DADAPTER) {
		DBG_FTL(("Notification about IDI_DADAPTER change ! Oops."));
		return;
	} else if (adapter->type == IDI_DIMAINT) {
		if (removal) {
			stop_dbg();
		} else {
			memcpy(&MAdapter, adapter, sizeof(MAdapter));
			dprintf = (DIVA_DI_PRINTF) MAdapter.request;
			DbgRegister("User IDI", DRIVERRELEASE_IDI, DBG_DEFAULT);
		}
	} else if (((adapter->type > 0) && (adapter->type < 16)) ||
             ((adapter->type == IDI_DRIVER) && (adapter->features & DI_MANAGE)) ||
             (adapter->type == IDI_MADAPTER)) { /* IDI Adapter */
		if (removal) {
			um_remove_card(adapter);
		} else {
			um_new_card(adapter);
		}
	}
}

/*
 * connect DIDD
 */
static int DIVA_INIT_FUNCTION connect_didd(void)
{
	int x = 0;
	int dadapter = 0;
	IDI_SYNC_REQ req;
	DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

  atomic_set(&diva_user_mode_idi_disable_adapter_insertion, 0);

	DIVA_DIDD_Read(DIDD_Table, sizeof(DIDD_Table));

	for (x = 0; x < MAX_DESCRIPTORS; x++) {
		if (DIDD_Table[x].type == IDI_DADAPTER) {	/* DADAPTER found */
			dadapter = 1;
			memcpy(&DAdapter, &DIDD_Table[x], sizeof(DAdapter));
			req.didd_notify.e.Req = 0;
			req.didd_notify.e.Rc =
			    IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY;
			req.didd_notify.info.callback = (void *)didd_callback;
			req.didd_notify.info.context = 0;
			DAdapter.request((ENTITY *) & req);
			if (req.didd_notify.e.Rc != 0xff) {
				stop_dbg();
				return (0);
			}
			notify_handle = req.didd_notify.info.handle;
		} else if (DIDD_Table[x].type == IDI_DIMAINT) {	/* MAINT found */
			memcpy(&MAdapter, &DIDD_Table[x], sizeof(DAdapter));
			dprintf = (DIVA_DI_PRINTF) MAdapter.request;
			DbgRegister("User IDI", DRIVERRELEASE_IDI, DBG_DEFAULT);
		} else if (((DIDD_Table[x].type > 0) && (DIDD_Table[x].type < 16)) ||
               ((DIDD_Table[x].type == IDI_DRIVER) && (DIDD_Table[x].features & DI_MANAGE)) ||
               (DIDD_Table[x].type == IDI_MADAPTER)) {	/* IDI Adapter found */
			um_new_card(&DIDD_Table[x]);
		}
	}

	if (!dadapter) {
		stop_dbg();
	}

	return (dadapter);
}

/*
 *  Disconnect from DIDD
 */
static void DIVA_EXIT_FUNCTION disconnect_didd(void)
{
	IDI_SYNC_REQ req;

  stop_dbg();

	req.didd_notify.e.Req = 0;
	req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
	req.didd_notify.info.handle = notify_handle;
	DAdapter.request((ENTITY *) & req);
}

/*
 * init
 */
int DIVA_INIT_FUNCTION idifunc_init(void)
{
	if (diva_user_mode_idi_init()) {
		DBG_ERR(("init: init failed."));
		return (0);
	}

	if (!connect_didd()) {
		diva_user_mode_idi_finit();
		DBG_ERR(("init: failed to connect to DIDD."));
		return (0);
	}
	return (1);
}

/*
 * finit
 */
void DIVA_EXIT_FUNCTION idifunc_finit(void)
{
  atomic_set(&diva_user_mode_idi_disable_adapter_insertion, 1);
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0)) && \
	!(defined(RHEL_RELEASE_CODE) && LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	flush_scheduled_work();
#endif
	diva_user_mode_idi_finit();
	disconnect_didd();
}

