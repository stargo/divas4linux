
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
#include "pc.h"
#include "debuglib.h"
#include "di_defs.h"
#include "divasync.h"
#include "dadapter.h"
/* --------------------------------------------------------------------------
  Adapter array change notification framework
   -------------------------------------------------------------------------- */
typedef struct _didd_adapter_change_notification {
 didd_adapter_change_callback_t callback;
 void IDI_CALL_ENTITY_T *    context;
} didd_adapter_change_notification_t, \
 * IDI_CALL_ENTITY_T pdidd_adapter_change_notification_t;
#define DIVA_DIDD_MAX_NOTIFICATIONS 256
static didd_adapter_change_notification_t\
              NotificationTable[DIVA_DIDD_MAX_NOTIFICATIONS];
/* --------------------------------------------------------------------------
  Array to held adapter information
   -------------------------------------------------------------------------- */
static DESCRIPTOR  HandleTable[NEW_MAX_DESCRIPTORS];
dword Adapters = 0; /* Number of adapters */
/* --------------------------------------------------------------------------
  Access to CFGLib interface
   -------------------------------------------------------------------------- */
#if defined(__DIVA_CFGLIB_IMPLEMENTATION__)
extern void* diva_cfg_lib_get_cfg_interface(void);
#endif
/* --------------------------------------------------------------------------
  Shadow IDI_DIMAINT
  and 'shadow' debug stuff
   -------------------------------------------------------------------------- */
static void no_printf (unsigned char * format, ...) { }
DIVA_DI_PRINTF dprintf = no_printf;
static DESCRIPTOR  MAdapter =  {IDI_DIMAINT, /* Adapter Type */
                0x00,     /* Channels */
                0x0000,    /* Features */
                (IDI_CALL)no_printf};
/* --------------------------------------------------------------------------
  DAdapter. Only IDI clients with buffer, that is huge enough to
  get all descriptors will receive information about DAdapter
  { byte type, byte channels, word features, IDI_CALL request }
   -------------------------------------------------------------------------- */
static void IDI_CALL_LINK_T diva_dadapter_request (ENTITY IDI_CALL_ENTITY_T *);
static DESCRIPTOR  DAdapter =  {IDI_DADAPTER, /* Adapter Type */
                0x00,     /* Channels */
                0x0000,    /* Features */
                diva_dadapter_request };
/* --------------------------------------------------------------------------
  LOCALS
   -------------------------------------------------------------------------- */
static dword diva_register_adapter_callback (\
                   didd_adapter_change_callback_t callback,
                   void IDI_CALL_ENTITY_T* context);
static void diva_remove_adapter_callback (dword handle);
static void diva_notify_adapter_change (DESCRIPTOR* d, int removal);
static diva_os_spin_lock_t didd_spin;
/* --------------------------------------------------------------------------
  Should be called as first step, after driver init
  -------------------------------------------------------------------------- */
void diva_didd_load_time_init (void) {
 memset (&HandleTable[0], 0x00, sizeof(HandleTable));
 memset (&NotificationTable[0], 0x00, sizeof(NotificationTable));
 diva_os_initialize_spin_lock (&didd_spin, "didd");
}
/* --------------------------------------------------------------------------
  Should be called as last step, if driver does unload
  -------------------------------------------------------------------------- */
void diva_didd_load_time_finit (void) {
 diva_os_destroy_spin_lock (&didd_spin, "didd");
}
/* --------------------------------------------------------------------------
  Called in order to register new adapter in adapter array
  return adapter handle (> 0) on success
  return -1 adapter array overflow
  -------------------------------------------------------------------------- */
int diva_didd_add_descriptor (DESCRIPTOR* d) {
 diva_os_spin_lock_magic_t      irql;
 int i;
 if (d->type == IDI_DIMAINT) {
  if (d->request) {
   MAdapter.request = d->request;
   dprintf = (DIVA_DI_PRINTF)d->request;
   diva_notify_adapter_change (&MAdapter, 0); /* Inserted */
   DBG_TRC (("DIMAINT registered, dprintf=%08x", d->request))
  } else {
   DBG_TRC (("DIMAINT removed"))
   diva_notify_adapter_change (&MAdapter, 1); /* About to remove */
   MAdapter.request = (IDI_CALL)no_printf;
   dprintf = no_printf;
  }
  return (NEW_MAX_DESCRIPTORS);
 }
 for (i = 0; i < NEW_MAX_DESCRIPTORS; i++) {
  diva_os_enter_spin_lock (&didd_spin, &irql, "didd_add");
  if (HandleTable[i].type == 0) {
   memcpy (&HandleTable[i], d, sizeof(*d));
   Adapters++;
   diva_os_leave_spin_lock (&didd_spin, &irql, "didd_add");
   diva_notify_adapter_change (d, 0); /* we have new adapter */
   DBG_TRC (("Add adapter[%d], request=%08x", (i+1), d->request))
   return (i+1);
  }
  diva_os_leave_spin_lock (&didd_spin, &irql, "didd_add");
 }
 DBG_ERR (("Can't add adapter, out of resources"))
 return (-1);
}
/* --------------------------------------------------------------------------
  Called in order to remove one registered adapter from array
  return adapter handle (> 0) on success
  return 0 on success
  -------------------------------------------------------------------------- */
int diva_didd_remove_descriptor (IDI_CALL request) {
 diva_os_spin_lock_magic_t      irql;
 int i;
 if (request == MAdapter.request) {
  DBG_TRC(("DIMAINT removed"))
  dprintf = no_printf;
  diva_notify_adapter_change (&MAdapter, 1); /* About to remove */
  MAdapter.request = (IDI_CALL)no_printf;
  return (0);
 }
 for (i = 0; (Adapters && (i < NEW_MAX_DESCRIPTORS)); i++) {
  if (HandleTable[i].request == request) {
   diva_notify_adapter_change (&HandleTable[i], 1); /* About to remove */
   diva_os_enter_spin_lock (&didd_spin, &irql, "didd_rm");
   memset (&HandleTable[i], 0x00, sizeof(HandleTable[0]));
   Adapters--;
   diva_os_leave_spin_lock (&didd_spin, &irql, "didd_rm");
   DBG_TRC (("Remove adapter[%d], request=%08x", (i+1), request))
   return (0);
  }
 }
 DBG_ERR (("Invalid request=%08x, can't remove adapter", request))
 return (-1);
}
/* --------------------------------------------------------------------------
  Read adapter array
  return 1 if not enough space to save all available adapters
   -------------------------------------------------------------------------- */
int diva_didd_read_adapter_array (DESCRIPTOR* buffer, int length) {
 diva_os_spin_lock_magic_t      irql;
 int src, dst;
 memset (buffer, 0x00, length);
 length /= sizeof(DESCRIPTOR);
 DBG_TRC (("DIDD_Read, space = %d, Adapters = %d", length, Adapters+2))
 
 diva_os_enter_spin_lock (&didd_spin, &irql, "didd_read");
 for (src = 0, dst = 0;
    (Adapters && (src < NEW_MAX_DESCRIPTORS) && (dst < length));
    src++) {
  if (HandleTable[src].type) {
   memcpy (&buffer[dst], &HandleTable[src], sizeof(DESCRIPTOR));
   dst++;
  }
 }
 diva_os_leave_spin_lock (&didd_spin, &irql, "didd_read");
 if (dst < length) {
  memcpy (&buffer[dst], &MAdapter, sizeof(DESCRIPTOR));
  dst++;
 } else {
  DBG_ERR (("Can't write DIMAINT. Array too small"))
 }
 if (dst < length) {
  memcpy (&buffer[dst], &DAdapter, sizeof(DESCRIPTOR));
  dst++;
 } else {
  DBG_ERR (("Can't write DADAPTER. Array too small"))
 }
 DBG_TRC (("Read %d adapters", dst))
 return (dst == length);
}
/* --------------------------------------------------------------------------
  DAdapter request function.
  This function does process only synchronous requests, and is used
  for reception/registration of new interfaces
   -------------------------------------------------------------------------- */
static void IDI_CALL_LINK_T diva_dadapter_request (\
                      ENTITY IDI_CALL_ENTITY_T *e) {
 IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ *)e ;
 if (e->Req) { /* We do not process it, also return error */
  e->Rc = OUT_OF_RESOURCES;
  DBG_ERR (("Can't process async request, Req=%02x", e->Req))
  return;
 }
 /*
  So, we process sync request
  */
 switch (e->Rc) {
  case IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY: {
   diva_didd_adapter_notify_t* pinfo = &syncReq->didd_notify.info;
   pinfo->handle = diva_register_adapter_callback (\
             (didd_adapter_change_callback_t)pinfo->callback,
             (void IDI_CALL_ENTITY_T *)pinfo->context);
   e->Rc = 0xff;
  } break;
  case IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY: {
   diva_didd_adapter_notify_t* pinfo = &syncReq->didd_notify.info;
   diva_remove_adapter_callback (pinfo->handle);
   e->Rc = 0xff;
  } break;
  case IDI_SYNC_REQ_DIDD_ADD_ADAPTER: {
   diva_didd_add_adapter_t* pinfo = &syncReq->didd_add_adapter.info;
   if (diva_didd_add_descriptor ((DESCRIPTOR*)pinfo->descriptor) < 0) {
    e->Rc = OUT_OF_RESOURCES;
   } else {
    e->Rc = 0xff;
   }
  } break;
  case IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER: {
   diva_didd_remove_adapter_t* pinfo = &syncReq->didd_remove_adapter.info;
   if (diva_didd_remove_descriptor ((IDI_CALL)pinfo->p_request) < 0) {
    e->Rc = OUT_OF_RESOURCES;
   } else {
    e->Rc = 0xff;
   }
  } break;
  case IDI_SYNC_REQ_DIDD_READ_ADAPTER_ARRAY: {
   diva_didd_read_adapter_array_t* pinfo =\
                &syncReq->didd_read_adapter_array.info;
   if (diva_didd_read_adapter_array ((DESCRIPTOR*)pinfo->buffer,
                  (int)pinfo->length)) {
    e->Rc = OUT_OF_RESOURCES;
   } else {
    e->Rc = 0xff;
   }
  } break;
#if defined(__DIVA_CFGLIB_IMPLEMENTATION__)
  case IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC: {
   diva_didd_get_cfg_lib_ifc_t* pinfo =
    &syncReq->didd_get_cfg_lib_ifc.info;
   if ((pinfo->ifc = diva_cfg_lib_get_cfg_interface()) != 0) {
    e->Rc = 0xff;
   } else {
    e->Rc = OUT_OF_RESOURCES;
   }
  } break;
#endif
  case IDI_SYNC_REQ_XDI_GET_CLOCK_DATA: {
    diva_xdi_get_clock_data_t* clock_data = &syncReq->xdi_clock_data.info;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
    if (diva_get_clock_data (&clock_data->bus_addr_lo,
                             &clock_data->bus_addr_hi,
                             &clock_data->length,
                             &clock_data->addr) == 0) {
#else
    if (diva_get_clock_data (&clock_data->bus_addr_lo,
                             &clock_data->bus_addr_hi,
                             &clock_data->length,
                             &clock_data->addr,
                             clock_data->pci_dev_handle) == 0) {
	DBG_LOG(("%s: (%08x:%08x) %p, len:%d dev=%p",
              __FUNCTION__,
	      clock_data->bus_addr_lo,
	      clock_data->bus_addr_hi,
	      clock_data->addr,
	      clock_data->length,
	      clock_data->pci_dev_handle))
#endif
      e->Rc = 0xff;
    } else {
      e->Rc = OUT_OF_RESOURCES;
    }
  } break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  case IDI_SYNC_REQ_XDI_UNMAP_CLOCK_DATA_ADDR: {
    diva_xdi_get_clock_data_t* clock_data = &syncReq->xdi_clock_data.info;

    DBG_LOG(("%s: (%08x:%08x) %p",
              __FUNCTION__, clock_data->bus_addr_lo, clock_data->bus_addr_hi, clock_data->pci_dev_handle))
    if (diva_unmap_clock_data (clock_data->bus_addr_lo,
                           0,
                           clock_data->pci_dev_handle) == 0) {
      e->Rc = 0xff;
    } else {
      e->Rc = OUT_OF_RESOURCES;
    }
  } break;
#endif
  default:
   DBG_ERR (("Can't process sync request, Req=%02x", e->Rc))
   e->Rc = OUT_OF_RESOURCES;
 }
}
/* --------------------------------------------------------------------------
  IDI client does register his notification function
  -------------------------------------------------------------------------- */
static dword diva_register_adapter_callback (\
                   didd_adapter_change_callback_t callback,
                   void IDI_CALL_ENTITY_T* context) {
 diva_os_spin_lock_magic_t irql;
 dword i;
 
 for (i = 0; i < DIVA_DIDD_MAX_NOTIFICATIONS; i++) {
  diva_os_enter_spin_lock (&didd_spin, &irql, "didd_nfy_add");
  if (!NotificationTable[i].callback) {
   NotificationTable[i].callback = callback;
   NotificationTable[i].context = context;
   diva_os_leave_spin_lock (&didd_spin, &irql, "didd_nfy_add");
   DBG_TRC(("Register adapter notification[%d]=%08x", i+1, callback))
   return (i+1);
  }
  diva_os_leave_spin_lock (&didd_spin, &irql, "didd_nfy_add");
 }
 DBG_ERR (("Can't register adapter notification, overflow"))
 return (0);
}
/* --------------------------------------------------------------------------
  IDI client does register his notification function
  -------------------------------------------------------------------------- */
static void diva_remove_adapter_callback (dword handle) {
 diva_os_spin_lock_magic_t irql;
 if (handle && ((--handle) < DIVA_DIDD_MAX_NOTIFICATIONS)) {
  diva_os_enter_spin_lock (&didd_spin, &irql, "didd_nfy_rm");
  NotificationTable[handle].callback = 0;
  NotificationTable[handle].context  = 0;
  diva_os_leave_spin_lock (&didd_spin, &irql, "didd_nfy_rm");
  DBG_TRC(("Remove adapter notification[%d]", (int)(handle+1)))
  return;
 }
 DBG_ERR(("Can't remove adapter notification, handle=%d", handle))
}
/* --------------------------------------------------------------------------
  Notify all client about adapter array change
  Does suppose following behavior in the client side:
  Step 1: Redister Notification
  Step 2: Read Adapter Array
  -------------------------------------------------------------------------- */
static void diva_notify_adapter_change (DESCRIPTOR* d, int removal) {
 int i, do_notify;
 didd_adapter_change_notification_t nfy;
 diva_os_spin_lock_magic_t irql;
 for (i = 0; i < DIVA_DIDD_MAX_NOTIFICATIONS; i++) {
  do_notify = 0;
  diva_os_enter_spin_lock (&didd_spin, &irql, "didd_nfy");
  if (NotificationTable[i].callback) {
   memcpy (&nfy, &NotificationTable[i], sizeof(nfy));
   do_notify = 1;
  }
  diva_os_leave_spin_lock (&didd_spin, &irql, "didd_nfy");
  if (do_notify) {
   (*(nfy.callback))(nfy.context, d, removal);
  }
 }
}
/* --------------------------------------------------------------------------
  For all systems, that are linked by Kernel Mode Linker this is ONLY one
  function thet should be exported by this device driver
  IDI clients should look for IDI_DADAPTER, and use request function
  of this adapter (sync request) in order to receive appropriate services:
  - add new adapter
  - remove existing adapter
  - add adapter array notification
  - remove adapter array notification
  (read adapter is redundant in this case)
  INPUT:
   buffer - pointer to buffer that will receive adapter array
   length  - length (in bytes) of space in buffer
  OUTPUT:
   Adapter array will be written to memory described by 'buffer'
   If the last adapter seen in the returned adapter array is
   IDI_DADAPTER or if last adapter in array does have type '0', then
   it was enougth space in buffer to accomodate all available
   adapter descriptors
  *NOTE 1 (debug interface):
   The IDI adapter of type 'IDI_DIMAINT' does register as 'request'
   famous 'dprintf' function (of type DI_PRINTF, please look
   include/debuglib.c and include/debuglib.h) for details.
   So dprintf is not exported from module debug module directly,
   instead of this IDI_DIMAINT is registered.
   Module load order will receive in this case:
    1. DIDD (this file)
    2. DIMAINT does load and register 'IDI_DIMAINT', at this step
      DIDD should be able to get 'dprintf', save it, and
      register with DIDD by means of 'dprintf' function.
    3. any other driver is loaded and is able to access adapter array
      and debug interface
   This approach does allow to load/unload debug interface on demand,
   and save memory, it it is necessary.
  -------------------------------------------------------------------------- */
void IDI_CALL_LINK_T DIVA_DIDD_Read (void IDI_CALL_ENTITY_T * buffer,
                   int length) {
 diva_didd_read_adapter_array (buffer, length);
}
