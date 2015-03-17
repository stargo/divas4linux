/* $Id: diddfunc.c,v 1.14 2003/08/25 10:06:37 schindler Exp $
 *
 * DIDD Interface module for Dialogic active cards.
 * 
 * Functions are in dadapter.c 
 * 
 * Copyright 2002-2009 by Armin Schindler (mac@melware.de) 
 * Copyright 2002-2009 Cytronics & Melware (info@melware.de)
 * Copyright 2002-2007 Dialogic
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include "platform.h"
#include "di_defs.h"
#include "dadapter.h"
#include "divasync.h"
#include "cfg_types.h"
#include "cfg_ifc.h"

#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)


extern void DIVA_DIDD_Read(void *, int);
extern char *DRIVERRELEASE_DIDD;
static dword notify_handle;
static DESCRIPTOR _DAdapter;

/*
 * didd callback function
 */
static void didd_callback(void *context, DESCRIPTOR * adapter,
			   int removal)
{
	if (adapter->type == IDI_DADAPTER) {
		DBG_ERR(("Notification about IDI_DADAPTER change ! Oops."))
		return;
	} else if (adapter->type == IDI_DIMAINT) {
		if (removal) {
			DbgDeregister();
		} else {
			DbgRegister("DIDD", DRIVERRELEASE_DIDD, DBG_DEFAULT);
		}
	} else if (adapter->channels != 0       &&
						 adapter->type < IDI_VADAPTER &&
						 adapter->features != 0       &&
						 adapter->request) {
		if (removal == 0) {
			diva_cfg_lib_notify_adapter_insertion(adapter->request);
		}
	}
}

/* -------------------------------------------------------------------------
  Portable debug Library
  ------------------------------------------------------------------------- */
#include "debuglib.c"
/* ------------------------------------------------------------------------- */

/*
 * connect to didd
 */
static int DIVA_INIT_FUNCTION connect_didd(void)
{
	int x = 0;
	int dadapter = 0;
	IDI_SYNC_REQ req;
	static DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

	DIVA_DIDD_Read(DIDD_Table, sizeof(DIDD_Table));

	for (x = 0; x < MAX_DESCRIPTORS; x++) {
		if (DIDD_Table[x].type == IDI_DADAPTER) {	/* DADAPTER found */
			dadapter = 1;
			memcpy(&_DAdapter, &DIDD_Table[x], sizeof(_DAdapter));
			req.didd_notify.e.Req = 0;
			req.didd_notify.e.Rc =
			    IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY;
			req.didd_notify.info.callback = (void *)didd_callback;
			req.didd_notify.info.context = 0;
			_DAdapter.request((ENTITY *) & req);
			if (req.didd_notify.e.Rc != 0xff)
				return (0);
			notify_handle = req.didd_notify.info.handle;
		} else if (DIDD_Table[x].type == IDI_DIMAINT) {	/* MAINT found */
			DbgRegister("DIDD", DRIVERRELEASE_DIDD, DBG_DEFAULT);
		}
	}
	return (dadapter);
}

/*
 * disconnect from didd
 */
static void DIVA_EXIT_FUNCTION disconnect_didd(void)
{
	IDI_SYNC_REQ req;

	req.didd_notify.e.Req = 0;
	req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
	req.didd_notify.info.handle = notify_handle;
	_DAdapter.request((ENTITY *) & req);
}

/*
 * init
 */
int DIVA_INIT_FUNCTION diddfunc_init(void)
{
	diva_didd_load_time_init();

	if (!connect_didd()) {
		DBG_ERR(("init: failed to connect to DIDD."))
		diva_didd_load_time_finit();
		return (0);
	}
	return (1);
}

/*
 * finit
 */
void DIVA_EXIT_FUNCTION diddfunc_finit(void)
{
	DbgDeregister();
	disconnect_didd();
	diva_didd_load_time_finit();
}
