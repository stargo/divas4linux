/*
 *
  Copyright (c) Dialogic, 2009.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
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

#define DIVA_UM_IDI_OS_DEFINES 1

#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "dlist.h"
#include "dqueue.h"
#include "adapter.h"
#include "entity.h"
#include "um_xdi.h"
#include "um_idi.h"
#include "debuglib.h"
#include "divasync.h"

#define DIVAS_MAX_XDI_ADAPTERS	64
#define DIVA_LICENSE_RESPONSE_VARIABLE "Info\\License\\Verify"

int diva_user_mode_idi_create_adapter (const DESCRIPTOR* d, int adapter_nr);
/* --------------------------------------------------------------------------
		IMPORTS
   -------------------------------------------------------------------------- */
extern void diva_os_wakeup_read              (void* os_context);
extern void diva_os_wakeup_close             (void* os_context);
/* --------------------------------------------------------------------------
		LOCALS
   -------------------------------------------------------------------------- */
static diva_entity_queue_t	adapter_q;
static diva_entity_queue_t  system_delayed_cleanup_q;
static diva_entity_queue_t	free_mem_q;
static diva_os_spin_lock_t adapter_lock;

static diva_um_idi_adapter_t* diva_um_idi_find_adapter (dword nr);
static void cleanup_adapter (diva_um_idi_adapter_t* a);
static void cleanup_entity (divas_um_idi_entity_t* e, int free_memory);
static int diva_user_mode_idi_adapter_features (diva_um_idi_adapter_t* a,
                                                diva_um_idi_adapter_features_t* features);
static int process_idi_request (divas_um_idi_entity_t* e,
                                diva_um_idi_req_hdr_t* req);
static int process_idi_rc (divas_um_idi_entity_t* e,	byte rc);
static int process_idi_ind (divas_um_idi_entity_t* e, byte ind);
static int write_return_code (divas_um_idi_entity_t* e, byte rc);
static int read_combi_ind (divas_um_idi_entity_t* e);
static void diva_um_idi_dummy_request(ENTITY* e) { }
static int diva_user_mode_idi_get_descriptor_list (byte* mem, int max_length);
static void diva_um_free_unused_memory (void);
static int diva_um_idi_proxy_read (void* entity,
																	 void* os_handle,
																	 void* dst,
																	 int max_length,
																	 divas_um_idi_copy_to_user_fn_t cp_fn);
static int diva_um_idi_proxy_write (void* entity,
																		void* os_handle,
																		const void* src,
																		int length,
																		divas_um_idi_copy_from_user_fn_t cp_fn);
static void diva_um_proxy_user_request (divas_um_idi_proxy_t* pE, ENTITY* e);
static void diva_um_idi_destroy_proxy(void* context);
static word diva_copy_xdescr (byte* dst, ENTITY* e, int max_length);
static int diva_user_mode_is_proxy_adapter (IDI_CALL request);

/* --------------------------------------------------------------------------
    DIDD request function management
   -------------------------------------------------------------------------- */
typedef struct _diva_um_proxy_request_array {
  int busy;
  IDI_CALL  request;
	IDI_CALL  local_request;
	divas_um_idi_proxy_t* pE;
} diva_um_proxy_request_array_t;

#define declare_um_proxy_request(__x__) static void um_proxy_request_##__x__(ENTITY* e); static void um_proxy_local_request_##__x__(ENTITY* e)

#define um_proxy_request_fn(__x__) \
static void um_proxy_request_##__x__(ENTITY *e) { \
  if (requests[__x__].busy) \
		diva_os_diva_um_proxy_user_request (diva_um_proxy_user_request, requests[__x__].pE, e); } \
static void um_proxy_local_request_##__x__(ENTITY *e) { \
  if (requests[__x__].busy) \
		diva_um_proxy_user_request (requests[__x__].pE, e); }

#define um_proxy_request(__x__) um_proxy_request_##__x__, um_proxy_local_request_##__x__

declare_um_proxy_request(0);
declare_um_proxy_request(1);
declare_um_proxy_request(2);
declare_um_proxy_request(3);
declare_um_proxy_request(4);
declare_um_proxy_request(5);
declare_um_proxy_request(6);
declare_um_proxy_request(7);
declare_um_proxy_request(8);
declare_um_proxy_request(9);
declare_um_proxy_request(10);
declare_um_proxy_request(11);
declare_um_proxy_request(12);
declare_um_proxy_request(13);
declare_um_proxy_request(14);
declare_um_proxy_request(15);
declare_um_proxy_request(16);
declare_um_proxy_request(17);
declare_um_proxy_request(18);
declare_um_proxy_request(19);
declare_um_proxy_request(20);
declare_um_proxy_request(21);
declare_um_proxy_request(22);
declare_um_proxy_request(23);
declare_um_proxy_request(24);
declare_um_proxy_request(25);
declare_um_proxy_request(26);
declare_um_proxy_request(27);
declare_um_proxy_request(28);
declare_um_proxy_request(29);
declare_um_proxy_request(30);
declare_um_proxy_request(31);

static diva_um_proxy_request_array_t requests[32] = {
{ 0, um_proxy_request(0),  0 },
{ 0, um_proxy_request(1),  0 },
{ 0, um_proxy_request(2),  0 },
{ 0, um_proxy_request(3),  0 },
{ 0, um_proxy_request(4),  0 },
{ 0, um_proxy_request(5),  0 },
{ 0, um_proxy_request(6),  0 },
{ 0, um_proxy_request(7),  0 },
{ 0, um_proxy_request(8),  0 },
{ 0, um_proxy_request(9),  0 },
{ 0, um_proxy_request(10), 0 },
{ 0, um_proxy_request(11), 0 },
{ 0, um_proxy_request(12), 0 },
{ 0, um_proxy_request(13), 0 },
{ 0, um_proxy_request(14), 0 },
{ 0, um_proxy_request(15), 0 },
{ 0, um_proxy_request(16), 0 },
{ 0, um_proxy_request(17), 0 },
{ 0, um_proxy_request(18), 0 },
{ 0, um_proxy_request(19), 0 },
{ 0, um_proxy_request(20), 0 },
{ 0, um_proxy_request(21), 0 },
{ 0, um_proxy_request(22), 0 },
{ 0, um_proxy_request(23), 0 },
{ 0, um_proxy_request(24), 0 },
{ 0, um_proxy_request(25), 0 },
{ 0, um_proxy_request(26), 0 },
{ 0, um_proxy_request(27), 0 },
{ 0, um_proxy_request(28), 0 },
{ 0, um_proxy_request(29), 0 },
{ 0, um_proxy_request(30), 0 },
{ 0, um_proxy_request(31), 0 }
};

um_proxy_request_fn(0)
um_proxy_request_fn(1)
um_proxy_request_fn(2)
um_proxy_request_fn(3)
um_proxy_request_fn(4)
um_proxy_request_fn(5)
um_proxy_request_fn(6)
um_proxy_request_fn(7)
um_proxy_request_fn(8)
um_proxy_request_fn(9)
um_proxy_request_fn(10)
um_proxy_request_fn(11)
um_proxy_request_fn(12)
um_proxy_request_fn(13)
um_proxy_request_fn(14)
um_proxy_request_fn(15)
um_proxy_request_fn(16)
um_proxy_request_fn(17)
um_proxy_request_fn(18)
um_proxy_request_fn(19)
um_proxy_request_fn(20)
um_proxy_request_fn(21)
um_proxy_request_fn(22)
um_proxy_request_fn(23)
um_proxy_request_fn(24)
um_proxy_request_fn(25)
um_proxy_request_fn(26)
um_proxy_request_fn(27)
um_proxy_request_fn(28)
um_proxy_request_fn(29)
um_proxy_request_fn(30)
um_proxy_request_fn(31)

static diva_os_spin_lock_t request_array_lock;


/* --------------------------------------------------------------------------
		Proxy entity
   -------------------------------------------------------------------------- */
typedef struct _diva_um_idi_proxy_entity {
  diva_entity_link_t link; /* Used to maintain list of entities */
  diva_entity_link_t req_link; /* Used to maintain list of active requests */
  ENTITY* e;
  int request_pending;
  int assign_pending;
	int remove_pending;
  struct {
    diva_um_idi_proxy_idi_req_t hdr;
    byte data[2152];
  } req;
} diva_um_idi_proxy_entity_t;

/* --------------------------------------------------------------------------
		MAIN
   -------------------------------------------------------------------------- */
int
diva_user_mode_idi_init (void)
{
	DESCRIPTOR d;

  diva_q_init(&adapter_q);
  diva_q_init(&free_mem_q);
  diva_os_initialize_spin_lock (&adapter_lock, "init");
  diva_os_initialize_spin_lock (&request_array_lock, "init");

  /*
    Create system adapter.
    System adapter is used to process requests which are
    not related to certain idi adapter
		*/
	memset(&d, 0x00, sizeof(d));
	d.request = diva_um_idi_dummy_request;
	diva_user_mode_idi_create_adapter (&d, DIVA_UM_IDI_SYSTEM_ADAPTER_NR);

  return (0);
}

/* --------------------------------------------------------------------------
		Copy adapter features to user supplied buffer
   -------------------------------------------------------------------------- */
static int
diva_user_mode_idi_adapter_features (diva_um_idi_adapter_t* a,
			diva_um_idi_adapter_features_t* features)
{
  IDI_SYNC_REQ sync_req;

  if ((a) && (a->d.request)){
    features->type           = a->d.type;
    features->features       = a->d.features;
    features->channels       = a->d.channels;
    memset(features->name, 0, sizeof(features->name));

    sync_req.GetName.Req = 0;
    sync_req.GetName.Rc = IDI_SYNC_REQ_GET_NAME;
    (*(a->d.request))((ENTITY *)&sync_req);
    strncpy(features->name, sync_req.GetName.name, sizeof(features->name)-1);

    sync_req.GetSerial.Req = 0;
    sync_req.GetSerial.Rc = IDI_SYNC_REQ_GET_SERIAL;
    sync_req.GetSerial.serial = 0;
    (*(a->d.request))((ENTITY *)&sync_req);
    features->serial_number = sync_req.GetSerial.serial;
  }

  return ((a) ? 0 : -1);
}

static int diva_user_mode_idi_get_descriptor_list (byte* mem, int max_length) {
	diva_um_idi_ind_hdr_t* pind = (diva_um_idi_ind_hdr_t*)mem;
	diva_um_idi_adapter_t* a;
	dword data_length = 0;
	dword* p = 0;

	if (mem != 0) {
		pind->type					= DIVA_UM_IDI_IND_DESCRIPTOR_LIST;
		pind->hdr.list.number_of_descriptors = 0;
		pind->data_length   = 0;
		p = (dword*)&pind[1];
		max_length -= sizeof(*pind);
	}

	a = (diva_um_idi_adapter_t*)diva_q_get_head (&adapter_q);
	while (a != 0 && max_length >= sizeof(*p)) {
		if (a->adapter_nr != DIVA_UM_IDI_SYSTEM_ADAPTER_NR &&
				a->d.request != 0 &&
				a->d.request != diva_um_idi_dummy_request) {
			if (mem != 0) {
				*p++ = (dword)a->adapter_nr;
				pind->hdr.list.number_of_descriptors++;
				pind->data_length += sizeof(*p);
				max_length -= sizeof(*p);
			}
			data_length += sizeof(*p);
		}
		a = (diva_um_idi_adapter_t*)diva_q_get_next (&a->link);
	}

	return (sizeof(*pind)+data_length);
}

/* --------------------------------------------------------------------------
		REMOVE ADAPTER
   -------------------------------------------------------------------------- */
void diva_user_mode_idi_remove_adapter (int adapter_nr) {
	diva_um_idi_adapter_t* a;
	diva_os_spin_lock_magic_t old_irql;

	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "create_adapter");

	a = (diva_um_idi_adapter_t*)diva_q_get_head (&adapter_q);

  while (a) {
    if(a->adapter_nr == adapter_nr) {
      diva_q_remove (&adapter_q, &a->link);
      cleanup_adapter (a);
      DBG_LOG (("DIDD: del adapter(%d)", a->adapter_nr))
			diva_q_add_tail (&free_mem_q, &a->link);
			break;
    }
    a = (diva_um_idi_adapter_t*)diva_q_get_next (&a->link);
  }

	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "create_adapter");

	diva_um_free_unused_memory ();
}

/* --------------------------------------------------------------------------
		CALLED ON DRIVER EXIT (UNLOAD)
   -------------------------------------------------------------------------- */
void
diva_user_mode_idi_finit (void)
{
	diva_um_idi_adapter_t* a = (diva_um_idi_adapter_t*)diva_q_get_head (&adapter_q);

	while (a) {
		diva_q_remove (&adapter_q, &a->link);
		cleanup_adapter (a);
		DBG_LOG (("DIDD: del adapter(%d)", a->adapter_nr))
		diva_os_free (0, a);
		a = (diva_um_idi_adapter_t*)diva_q_get_head (&adapter_q);
	}

	diva_um_free_unused_memory ();

	diva_os_destroy_spin_lock (&adapter_lock, "unload");
	diva_os_destroy_spin_lock (&request_array_lock, "unload");
}

/* -------------------------------------------------------------------------
		CREATE AND INIT IDI ADAPTER
	 ------------------------------------------------------------------------- */
int diva_user_mode_idi_create_adapter (const DESCRIPTOR* d, int adapter_nr) {
	diva_os_spin_lock_magic_t old_irql;
	diva_um_idi_adapter_t* a =
		(diva_um_idi_adapter_t*)diva_os_malloc (0, sizeof(diva_um_idi_adapter_t));
	int i;
	
	if (!a) {
		return (-1);
	}
	memset(a, 0x00, sizeof(*a));

	a->d = *d;
	a->adapter_nr = adapter_nr;

	if ((i = diva_user_mode_is_proxy_adapter (d->request)) >= 0)
		a->d.request = requests[i].local_request;

	DBG_LOG (("DIDD_ADD A(%d), type:%02x, features:%04x, channels:%d, %s",
					 adapter_nr, a->d.type, a->d.features, a->d.channels,
					 (diva_user_mode_is_proxy_adapter (a->d.request) >= 0) ? "user mode" : ""))

	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "create_adapter");
	diva_q_add_tail (&adapter_q, &a->link);
	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "create_adapter");

	return (0);
}

static int diva_user_mode_is_proxy_adapter (IDI_CALL request) {
	int i;

	for (i = 0; i < sizeof(requests)/sizeof(requests[0]); i++) {
		if (request == requests[i].request || request == requests[i].local_request)
			return (i);
	}

	return (-1);
}

static int cmp_adapter_nr (const void* what, const diva_entity_link_t* p) {
	diva_um_idi_adapter_t* a = (diva_um_idi_adapter_t*)p;
	dword* nr = (dword*)what;

	DBG_TRC (("find_adapter: (%d)-(%d)", *nr, a->adapter_nr))

	return (*nr != (dword)a->adapter_nr);
}

/* ------------------------------------------------------------------------
			Find adapter by Adapter number
   ------------------------------------------------------------------------ */
static diva_um_idi_adapter_t* diva_um_idi_find_adapter (dword nr) {
	return (diva_um_idi_adapter_t*)diva_q_find (&adapter_q,
                                                    (void*)&nr,
                                                    cmp_adapter_nr);
}

/* ------------------------------------------------------------------------
		Return number of adapters in system
   ------------------------------------------------------------------------ */
int diva_um_idi_nr_of_adapters (void) {
	return (diva_q_get_nr_of_entries (&adapter_q));
}

/* ------------------------------------------------------------------------
		Cleanup this adapter and cleanup/delete all entities assigned
		to this adapter
		Called unter protection of adapter spin lock
   ------------------------------------------------------------------------ */
static void cleanup_adapter (diva_um_idi_adapter_t* a) {
  divas_um_idi_entity_t *e, *me, *le;
	int adapter_nr = a->adapter_nr;

	while ((e = (divas_um_idi_entity_t*)diva_q_get_head(&a->delayed_cleanup_q)) != 0) {
		diva_q_remove (&a->delayed_cleanup_q, &e->link);
		diva_q_add_tail (&system_delayed_cleanup_q, &e->link);
	}

  e = (divas_um_idi_entity_t*)diva_q_get_head(&a->entity_q);
	while (e != 0) {
		diva_q_remove (&a->entity_q, &e->link);
    cleanup_entity (e, 0);
    if (e->os_context) {
		  diva_os_wakeup_read (e->os_context);
		  diva_os_wakeup_close (e->os_context);
    }
		me = e;
		e = (divas_um_idi_entity_t*)diva_q_get_head(&a->entity_q);

		if (me->entity_type == DivaUmIdiEntityTypeMaster) {
			diva_entity_queue_t* q[2];
			int i;
			me->multiplex.master.state = DivaUmIdiMasterOutOfService;
			q[0] = &me->multiplex.master.active_multiplex_q;
			q[1] = &me->multiplex.master.idle_multiplex_q;

			for (i = 0; i < sizeof(q)/sizeof(q[0]); i++) {
				while ((le = (divas_um_idi_entity_t*)diva_q_get_head (q[i])) != 0) {
					diva_q_remove (q[i], &le->link);
					le->multiplex.logical.xdi_entity = 0;
					cleanup_entity (le, 0);
          if (le->os_context != 0) {
						diva_os_wakeup_read (le->os_context);
						diva_os_wakeup_close (le->os_context);
					}
				}
			}
			DBG_TRC (("A(%d) me:%p removed, adapter removed", adapter_nr, me))
			diva_q_add_tail (&free_mem_q, &me->link);
		}
	}

  memset(&a->d, 0x00, sizeof(DESCRIPTOR));
}

/* ------------------------------------------------------------------------
		Cleanup, but NOT delete this entity
   ------------------------------------------------------------------------ */
static void cleanup_entity (divas_um_idi_entity_t* e, int free_memory) {
  e->os_ref   = 0;
  e->status   = 0;
  e->adapter  = 0;
  e->e.Id     = 0;
  e->rc_count = 0;

  e->status |= DIVA_UM_IDI_REMOVED;
	e->status |= DIVA_UM_IDI_REMOVE_PENDING;
	e->cbuffer_length = 0;
	e->cbuffer_pos    = 0;

	if (free_memory != 0 && e->entity_type != DivaUmIdiEntityTypeMaster) {
		diva_data_q_finit	(&e->data);
		diva_data_q_finit	(&e->rc);
	}
}

/* ------------------------------------------------------------------------
		Create ENTITY, link it to the adapter and remove pointer to entity
   ------------------------------------------------------------------------ */
void* divas_um_idi_create_entity (dword adapter_nr, void* file) {
  divas_um_idi_entity_t* e, *master_entity = 0, *entity_to_free = 0;
  diva_um_idi_adapter_t* a;
  diva_os_spin_lock_magic_t old_irql;
	int license_server  = (adapter_nr & 0x4000) != 0;
  int create_master   = (adapter_nr > 100) | license_server;
	int add_master_to_q = 1;

	if (adapter_nr == DIVA_UM_IDI_SYSTEM_ADAPTER_NR) {
		license_server  = 0;
		create_master   = 0;
	} else {
		adapter_nr &= ~(0x8000 | 0x4000);
	}

	if (create_master != 0) {
		if ((master_entity = (divas_um_idi_entity_t*)diva_os_malloc (0, sizeof(*master_entity))) == 0)
			return (0);

		memset (master_entity, 0x00, sizeof(*master_entity));
		
		master_entity->entity_type = DivaUmIdiEntityTypeMaster;
		master_entity->multiplex.master.type =
			(license_server == 0) ? DivaUmIdiMasterTypeIdi : DivaUmIdiMasterTypeLicense;
	}

	if ((e = (divas_um_idi_entity_t*)diva_os_malloc (0, sizeof(*e)))) {
		memset(e, 0x00, sizeof(*e));
		if (!(e->os_context = diva_os_malloc (0, diva_os_get_context_size ()))) {
			DBG_ERR (("E(%08x) no memory for os context", e))
			diva_os_free (0, e);
			return (0);
		}
		memset(e->os_context, 0x00, diva_os_get_context_size ());
    e->entity_type =
			create_master == 0 ? DivaUmIdiEntityTypeManagement : DivaUmidiEntityTypeLogical;

		if ((diva_data_q_init (&e->data, 2048+512, 16))) {
			DBG_ERR (("E(%08x) no memory for data q", e))
			diva_os_free (0, e->os_context);
			diva_os_free (0, e);
			if (master_entity != 0) {
				diva_os_free (0, master_entity);
			}
			return (0);
		}
		if ((diva_data_q_init (&e->rc, 512, 2))) {
			DBG_ERR (("E(%08x) no memory for RC", e))
			diva_data_q_finit	(&e->data);
			diva_os_free (0, e->os_context);
			diva_os_free (0, e);
			if (master_entity != 0) {
				diva_os_free (0, master_entity);
			}
			return (0);
		}

    diva_os_enter_spin_lock (&adapter_lock, &old_irql, "create_entity");

		/*
			Look for Adapter requested
			*/
		if ((a = diva_um_idi_find_adapter (adapter_nr)) == 0 || a->nr_delayed_cleanup_entities > DIVA_UM_IDI_MAX_TIMEOUTS) {
			/*
				No adapter was found, or this adapter was removed
				*/
      diva_os_leave_spin_lock (&adapter_lock, &old_irql, "create_entity");

			if (a == 0) {
				DBG_TRC (("A: no adapter(%d)", adapter_nr))
			} else {
				DBG_ERR (("A: adapter(%d) not responding", adapter_nr))
			}

			cleanup_entity (e, 1);
			diva_os_free (0, e->os_context);
			diva_os_free (0, e);
			if (master_entity != 0) {
				diva_os_free (0, master_entity);
			}
			return (0);
		}

		e->os_ref = file; /* link to os handle */

    /*
      Check is master entity is available
			*/
		if (create_master != 0) {
			divas_um_idi_entity_t* m_e = (divas_um_idi_entity_t*)diva_q_get_head(&a->entity_q);

			while (m_e != 0) {
				if (m_e->entity_type == DivaUmIdiEntityTypeMaster &&
						m_e->multiplex.master.type == master_entity->multiplex.master.type &&
						m_e->multiplex.master.state < DivaUmIdiMasterRemovePending) {
					break;
				}
				m_e = (divas_um_idi_entity_t*)diva_q_get_next(&m_e->link);
			}

			if (m_e != 0) {
				entity_to_free = master_entity;
				master_entity  = m_e;
				add_master_to_q = 0;
			}
		}

		if (create_master == 0) {
			e->adapter = a;   /* link to adapter   */
			diva_q_add_tail (&a->entity_q, &e->link); /* link from adapter */
		} else {
			master_entity->adapter = a; /* link to adapter */
			e->multiplex.logical.xdi_entity = master_entity;
			diva_q_add_tail (&master_entity->multiplex.master.idle_multiplex_q, &e->link);
			if (add_master_to_q != 0) {
				DBG_TRC (("A(%d) create master:%p", adapter_nr, master_entity))
				diva_q_add_tail (&a->entity_q, &master_entity->link); /* link from adapter */
			}
		}

    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "create_entity");

		DBG_TRC (("A(%d), create E(%p) master:%p", adapter_nr, e, master_entity))
	} else if (master_entity != 0) {
    diva_os_free (0, master_entity);
  }

	if (entity_to_free != 0) {
		diva_os_free (0, entity_to_free);
	}

	return (e);
}

/* -----------------------------------------------------------------------
    Create user mode proxy
   ----------------------------------------------------------------------- */
void* divas_um_idi_create_proxy (void* file) {
	divas_um_idi_proxy_t* e = (divas_um_idi_proxy_t*)diva_os_malloc (0, sizeof(*e));

	if (e != 0) {
		memset (e, 0x00, sizeof(*e));
		if ((e->os_context = diva_os_malloc (0, diva_os_get_context_size ())) != 0) {
			memset(e->os_context, 0x00, diva_os_get_context_size ());
			e->entity_function = DivaUmIdiProxy;
			e->state           = DivaUmIdiProxyIdle;
			e->os_ref          = file;
			e->req_nr          = -1;

			diva_os_initialize_spin_lock (&e->req_lock, "init");
			diva_os_initialize_spin_lock (&e->callback_lock, "init");

			DBG_LOG(("um add new proxy %p", e))
		} else {
			diva_os_free (0, e);
		}
	}

	return (e);
}

/* -----------------------------------------------------------------------
    Destroy user mode proxy
   ----------------------------------------------------------------------- */
static void diva_um_idi_destroy_proxy(void* context) {
	divas_um_idi_proxy_t* pE = (divas_um_idi_proxy_t*)context;
	diva_os_spin_lock_magic_t old_irql;

	if (pE != 0) {
		if (pE->state == DivaUmIdiProxyRegistered) {
			diva_um_idi_proxy_entity_t* pU;
			diva_entity_queue_t* q[2];
			diva_entity_queue_t  free_q;
			diva_entity_link_t* link;
			int i;

			if (pE->req_nr >= 0) {
				IDI_SYNC_REQ sync_req;

				sync_req.didd_remove_adapter.e.Req = 0;
				sync_req.didd_remove_adapter.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER;
				sync_req.didd_remove_adapter.info.p_request = requests[pE->req_nr].request;
				(*(pE->dadapter_request))((ENTITY*)&sync_req);

				diva_os_enter_spin_lock (&request_array_lock, &old_irql, "release");
				requests[pE->req_nr].busy = 0;
				requests[pE->req_nr].pE   = 0;
				diva_os_leave_spin_lock (&request_array_lock, &old_irql, "release");
			}

			diva_q_init (&free_q);
			q[0] = &pE->entity_q;
			q[1] = &pE->assign_q;

			diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "release");

			pE->state = DivaUmIdiProxyIdle;

			for (i = 0; i < sizeof(q)/sizeof(q[0]); i++) {
				while ((pU = (diva_um_idi_proxy_entity_t*)diva_q_get_head	(q[i])) != 0) {
					diva_q_remove   (q[i], &pU->link);
					diva_q_add_tail (&free_q, &pU->link);
				}
			}

			while ((link = diva_q_get_head (&pE->req_q)) != 0) {
				diva_q_remove (&pE->req_q, link);
			}

			memset (pE->entity_map, 0x00, sizeof(pE->entity_map));

			diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "release");

			while ((link = diva_q_get_head (&free_q)) != 0) {
				diva_q_remove (&free_q, link);
				diva_os_free (0, link);
				DBG_TRC(("um(%d) remove %p", pE->logical_adapter_number, link))
			}
		}

		DBG_LOG(("um(%d) release %p", pE->logical_adapter_number, pE))

		diva_os_free (0, pE);
	}
}

/* ------------------------------------------------------------------------
		Unlink entity and free memory 
   ------------------------------------------------------------------------ */
int divas_um_idi_delete_entity (int adapter_nr, void* entity) {
  divas_um_idi_entity_t* e, *me_to_free = 0;
  diva_um_idi_adapter_t* a;
  diva_os_spin_lock_magic_t old_irql;
	int timeout = 0, me_timeout = 0;
	

  if (!(e = (divas_um_idi_entity_t*)entity))
    return(-1);

	if (e->entity_function != DivaUmIdiEntity) {
		diva_um_idi_destroy_proxy(entity);
		return (0);
	}

	if (e->entity_type != DivaUmIdiEntityTypeManagement) {
		divas_um_idi_entity_t* me;

  	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "delete_entity");

		if ((me = e->multiplex.logical.xdi_entity) != 0) {
			if (e->multiplex.logical.on_active_q != 0) {
				if (me->multiplex.master.state == DivaUmIdiMasterResponsePending &&
						e == (divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q)) {
					me->multiplex.master.state = DivaUmIdiMasterResponsePendingCancelled;
				}
				diva_q_remove (&me->multiplex.master.active_multiplex_q, &e->link);
			} else {
				diva_q_remove (&me->multiplex.master.idle_multiplex_q, &e->link);
			}

			if (diva_q_get_head(&me->multiplex.master.active_multiplex_q) == 0 &&
					diva_q_get_head(&me->multiplex.master.idle_multiplex_q)   == 0) {
				if (me->multiplex.master.state == DivaUmIdiMasterAssigned && me->adapter != 0) {
					me->e.Req = REMOVE;
					me->e.X->PLength  = 1;
					me->e.X->P			  = "";
					me->multiplex.master.state = DivaUmIdiMasterRemovePending;
					me->rc_count = 1;
					me->status |= DIVA_UM_IDI_REMOVED;
					me->status |= DIVA_UM_IDI_REMOVE_PENDING;

					DBG_TRC(("A(%d) me:%p last le removed, start remove", me->adapter->adapter_nr, me))

					(*(me->adapter->d.request))(&me->e);
				} else if (me->multiplex.master.state == DivaUmIdiMasterIdle ||
									 me->multiplex.master.state == DivaUmIdiMasterOutOfService ||
									 me->adapter == 0) {
					if (me->adapter != 0) {
						me_timeout = (me->status & DIVA_UM_IDI_TIMEOUT) != 0;
						diva_q_remove (&me->adapter->entity_q, &me->link);
						if (me_timeout != 0) {
							me->status |= DIVA_UM_IDI_ME_TIMEOUT;
							diva_q_add_tail (&me->adapter->delayed_cleanup_q, &me->link);
							me->adapter->nr_delayed_cleanup_entities++;
						}
					}
					me_to_free = me;
				}
			}
		}

  	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "delete_entity");
	} else {
  	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "delete_entity");
  	if ((a = e->adapter)) {
			timeout = (e->status & DIVA_UM_IDI_TIMEOUT) != 0;
			diva_q_remove (&a->entity_q, &e->link);
			if (timeout != 0) {
				diva_q_add_tail (&e->adapter->delayed_cleanup_q, &e->link);
				a->nr_delayed_cleanup_entities++;
			}
  	}
  	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "delete_entity");
	}

  diva_um_idi_stop_wdog (entity);

	if (timeout == 0) {
		cleanup_entity (e, 1);
		diva_os_free (0, e->os_context);
		memset(e, 0x00, sizeof(*e));
		diva_os_free (0, e);
		DBG_TRC (("A(%d) remove E:%08x", adapter_nr, e))
	} else {
		DBG_ERR (("A(%d) delayed remove E:%08x", adapter_nr, e))
	}

	if (me_to_free != 0) {
		if (me_timeout == 0) {
			DBG_TRC (("A(%d) me:%p removed, in idle state", adapter_nr, me_to_free))
			diva_os_free (0, me_to_free);
		} else {
			DBG_TRC (("A(%d) me:%p delayed remove, in idle state", adapter_nr, me_to_free))
		}
	}

  return (0);
}

/* --------------------------------------------------------------------------
		Called by application to read data from IDI
	 -------------------------------------------------------------------------- */
int diva_um_idi_read (void* entity,
                      void* os_handle,
                      void* dst,
                      int max_length,
                      divas_um_idi_copy_to_user_fn_t cp_fn) {
	divas_um_idi_entity_t* e;
	diva_um_idi_adapter_t* a;
	const void* data;
	int length, ret = 0;
	diva_um_idi_data_queue_t* q;
  diva_os_spin_lock_magic_t old_irql;

	if ((e = (divas_um_idi_entity_t*)entity) == 0)
		return (-1);

	if (e->entity_function != DivaUmIdiEntity)
		return (diva_um_idi_proxy_read(entity, os_handle, dst, max_length, cp_fn));


  diva_os_enter_spin_lock (&adapter_lock, &old_irql, "read");


  if (e->entity_type != DivaUmIdiEntityTypeManagement &&
			e->multiplex.logical.xdi_entity == 0) {
		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "read");
		DBG_ERR(("E(%08x) read failed - me removed", e))
		return (-1);
	}

	a = e->entity_type == DivaUmIdiEntityTypeManagement ?
						e->adapter : e->multiplex.logical.xdi_entity->adapter;

  if (a == 0 ||
      (e->status & DIVA_UM_IDI_REMOVE_PENDING) != 0 ||
      (e->status & DIVA_UM_IDI_REMOVED) != 0) {
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "read");
	  DBG_ERR(("E(%08x) read failed - adapter removed", e))
    return (-1);
  }

	DBG_TRC(("A(%d) E(%08x) read(%d)", a->adapter_nr, e, max_length))

	/*
		Try to read return code first
		*/
	data	= diva_data_q_get_segment4read	(&e->rc);
	q			=	&e->rc;

	/*
		No return codes available, read indications now
		*/
	if (!data) {
		if (!(e->status & DIVA_UM_IDI_RC_PENDING)) {
			DBG_TRC (("A(%d) E(%08x) read data", a->adapter_nr, e))
			read_combi_ind (e);
			data	= diva_data_q_get_segment4read	(&e->data);
			q			=	&e->data;
		}
	} else {
		e->status &= ~DIVA_UM_IDI_RC_PENDING;
		DBG_TRC (("A(%d) E(%08x) read rc", a->adapter_nr, e))
	}
		
	if (data) {
		if ((length = diva_data_q_get_segment_length (q)) > max_length) {
			/*
				Not enough space to read message
				*/
			DBG_ERR (("A: A(%d) E(%08x) read small buffer", a->adapter_nr, e, ret))
      diva_os_leave_spin_lock (&adapter_lock, &old_irql, "read");
			return (-2);
		}
		/*
			Copy it to user, this function does access ONLY locked an verified
			memory, also we can access it witch spin lock held
			*/
	
		if ((ret = (*cp_fn) (os_handle, dst, data, length)) >= 0) {
			/*
				Acknowledge only if read was successfull
				*/
			diva_data_q_ack_segment4read	(q);
			read_combi_ind (e);
		}
	}


  DBG_TRC (("A(%d) E(%08x) read=%d", a->adapter_nr, e, ret))

  diva_os_leave_spin_lock (&adapter_lock, &old_irql, "read");

	return (ret);
}

static int diva_um_idi_proxy_read (void* entity,
																	 void* os_handle,
																	 void* dst,
																	 int max_length,
																	 divas_um_idi_copy_to_user_fn_t cp_fn) {
	divas_um_idi_proxy_t* pE = (divas_um_idi_proxy_t*)entity;
	diva_os_spin_lock_magic_t old_irql;
	int ret = -1, nr_pending_requests;
	ENTITY** pending_requests_q;

	if (pE->entity_function != DivaUmIdiProxy)
		return (-1);

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
	nr_pending_requests = pE->nr_pending_requests;
	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

	if (nr_pending_requests != 0 &&
			(pending_requests_q = (ENTITY**)diva_os_malloc (0, sizeof(pE->pending_requests_q))) != 0) {
		int i;

		diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
		if ((nr_pending_requests = pE->nr_pending_requests) != 0) {
			pE->nr_pending_requests = 0;
			memcpy (pending_requests_q, pE->pending_requests_q, nr_pending_requests*sizeof(pE->pending_requests_q[0]));
		}
		diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

		for (i = 0; i < nr_pending_requests; i++) {
			ENTITY* e = pending_requests_q[i];

			if ((e->Id & 0x1f) == 0) {
				/*
					ASSIGN request
					*/
				diva_um_idi_proxy_entity_t* pU = 0;
				int create_entity;

				diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
				if ((create_entity = pE->nr_entities + 1 <= pE->max_entities) != 0)
					pE->nr_entities++;
				diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

				if (create_entity != 0)
					pU = (diva_um_idi_proxy_entity_t*)diva_os_malloc (0, sizeof(*pU));

				if (pU != 0) {
					memset (pU, 0x00, sizeof(*pU));
					if ((pU->req.hdr.data_length = diva_copy_xdescr (pU->req.data,
																													 e,
																													 sizeof(pU->req.data))) == 0xffff) {
						diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
						pE->nr_entities--;
						diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");
						diva_os_free (0, pU);
						e->complete = 0xff;
						e->Rc       = ASSIGN_RC | WRONG_COMMAND;
						e->RcCh     = e->ReqCh;
						e->No       = 0;
						(*(e->callback))(e);
					} else {
						pU->e = e;
						pU->req.hdr.Id  = pU->e->Id;
						pU->req.hdr.Req = pU->e->Req;
						pU->req.hdr.Ch  = pU->e->ReqCh;

						DBG_TRC(("um(%d) create %p", pE->logical_adapter_number, pU))

						diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");

						pU->request_pending = 1;
						pU->assign_pending = 1;
						diva_q_add_tail (&pE->assign_q, &pU->link);
						diva_q_add_tail (&pE->req_q,    &pU->req_link);

						diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");
					}
				} else {
					diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
					pE->nr_entities--;
					diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");
					e->complete = 0xff;
					e->Rc       = ASSIGN_RC | OUT_OF_RESOURCES;
					e->RcCh     = e->ReqCh;
					(*(e->callback))(e);
				}
			} else {
				diva_um_idi_proxy_entity_t* pU;

				diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
				if ((pU = pE->entity_map[e->Id]) != 0) {
					if (pU->e != e) {
						DBG_ERR(("um(%d) wrong Id:%02x", pE->logical_adapter_number, e->Id))
						pU = 0;
					} else if (pU->request_pending != 0 || pU->remove_pending != 0) {
						DBG_ERR(("um(%d) Id:%02x request pending", pE->logical_adapter_number, pU->e->Id))
						pU = 0;
						e = 0;
					}
				} else {
					DBG_ERR(("um(%d) unknown Id:%02x", pE->logical_adapter_number, e->Id))
				}
				if (pU != 0) {
					if ((pU->req.hdr.data_length =
								diva_copy_xdescr (pU->req.data, e, sizeof(pU->req.data))) == 0xffff) {
						pU->req.hdr.data_length = 0;
						pU                      = 0;
					}
					if (pU != 0) {
						pU->req.hdr.Id  = e->Id;
						pU->req.hdr.Req = e->Req;
						pU->req.hdr.Ch  = e->ReqCh;
						pU->request_pending = 1;
						diva_q_add_tail (&pE->req_q,    &pU->req_link);
					}
				}
				diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

				if (pU == 0 && e != 0) {
					e->complete = 0xff;
					e->Rc = WRONG_ID;
					e->RcCh = 0;
					(*(e->callback))(e);
				}
			}
		}

		diva_os_free (0, pending_requests_q);
	}

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
	if (pE->state == DivaUmIdiProxyRegistered) {
		diva_entity_link_t* link = diva_q_get_head(&pE->req_q);
		if (link != 0) {
			diva_um_idi_proxy_entity_t* pU = DIVAS_CONTAINING_RECORD(link, diva_um_idi_proxy_entity_t, req_link);

			if (max_length >= (int)sizeof(pU->req.hdr)+pU->req.hdr.data_length) {
				if ((ret = (*cp_fn) (os_handle, dst, &pU->req, sizeof(pU->req.hdr)+pU->req.hdr.data_length)) >= 0) {
					if (pU->req.hdr.Req == REMOVE)
						pU->remove_pending = 1;
					diva_q_remove(&pE->req_q, link);
					pU->request_pending = 0;
				} else
					ret = -2;
			} else
				ret = -2;
		} else
			ret = 0;
	}
	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

	return (ret);
}

static void diva_um_free_unused_memory (void) {
	diva_entity_link_t* mem;
	divas_um_idi_entity_t* e;
  diva_os_spin_lock_magic_t old_irql;

  diva_os_enter_spin_lock (&adapter_lock, &old_irql, "write");

	while ((e = (divas_um_idi_entity_t*)diva_q_get_head(&system_delayed_cleanup_q)) != 0) {
		diva_q_remove (&system_delayed_cleanup_q, &e->link);

		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");

		if ((e->status & DIVA_UM_IDI_ME_TIMEOUT) == 0) {
			cleanup_entity (e, 1);
			if (e->os_context != 0) {
				diva_os_free (0, e->os_context);
			}
			memset(e, 0x00, sizeof(*e));
			DBG_ERR (("complete delayed remove E:%08x", e))
		} else {
			DBG_ERR (("complete delayed me remove E:%08x", e))
		}
		diva_os_free (0, e);

		diva_os_enter_spin_lock (&adapter_lock, &old_irql, "write");
	}

	while ((mem = diva_q_get_head(&free_mem_q)) != 0) {
		diva_q_remove (&free_mem_q, mem);

		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
		diva_os_free (0, mem);
		DBG_TRC(("Free mem:%p", mem))
		diva_os_enter_spin_lock (&adapter_lock, &old_irql, "write");
	}

	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
}

int diva_um_idi_write (void* entity,
                       void* os_handle,
                       const void* src,
                       int length,
                       divas_um_idi_copy_from_user_fn_t cp_fn) {
	divas_um_idi_entity_t* e;
	diva_um_idi_adapter_t* a;
	diva_um_idi_req_hdr_t* req;
	void* data;
	int ret = 0;
  diva_os_spin_lock_magic_t old_irql;

	diva_um_free_unused_memory ();

  if ((e = (divas_um_idi_entity_t*)entity) == 0)
		return (-1);

	if (e->entity_function != DivaUmIdiEntity)
		return (diva_um_idi_proxy_write (entity, os_handle, src, length, cp_fn));


  diva_os_enter_spin_lock (&adapter_lock, &old_irql, "write");

	if (e->entity_type == DivaUmIdiEntityTypeManagement) {
		if ((a = e->adapter) == 0) {
			diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
			DBG_ERR(("E(%08x) write failed - adapter removed", e))
			return (-1);
		}
	} else {
		if (e->multiplex.logical.xdi_entity == 0 ||
				(a = e->multiplex.logical.xdi_entity->adapter) == 0 ||
				e->multiplex.logical.xdi_entity->multiplex.master.state >= DivaUmIdiMasterRemovePending) {
			diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
			DBG_ERR(("E(%08x) le write failed - me removed", e))
			return (-1);
		}
	}

	if ((e->status & DIVA_UM_IDI_REMOVE_PENDING) != 0 ||
			(e->status & DIVA_UM_IDI_REMOVED) != 0) {
		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
		DBG_ERR(("E(%08x) write failed - adapter removed", e))
		return (-1);
	}

  DBG_TRC(("A(%d) E(%08x) write(%d)", a->adapter_nr, e, length))

  if ((length < sizeof (*req)) || (length > sizeof (e->buffer))) {
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
    return (-2);
  }

  if (e->status & DIVA_UM_IDI_RC_PENDING) {
    DBG_ERR(("A: A(%d) E(%08x) rc pending", a->adapter_nr, e))
              diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
    return (-1); /* should wait for RC code first */
  }

	/*
			Copy function does access only locked verified memory,
			also it can be called with spin lock held
		*/
	if ((ret = (*cp_fn)(os_handle, e->buffer, src, length)) < 0) {
		DBG_TRC(("A: A(%d) E(%08x) write error=%d", a->adapter_nr, e, ret))
                diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
		return (ret);
	}

	req = (diva_um_idi_req_hdr_t*)&e->buffer[0];

	switch (req->type) {
		case DIVA_UM_IDI_GET_FEATURES: {
			DBG_TRC(("A(%d) get_features", a->adapter_nr))
			if (!(data = diva_data_q_get_segment4write	(&e->data, sizeof(diva_um_idi_ind_hdr_t)))) {
				DBG_ERR(("A(%d) get_features, no free buffer", a->adapter_nr))
				diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
				return (0);
			}
			diva_user_mode_idi_adapter_features (a,
												&(((diva_um_idi_ind_hdr_t*)data)->hdr.features));
			((diva_um_idi_ind_hdr_t*)data)->type = DIVA_UM_IDI_IND_FEATURES;
			((diva_um_idi_ind_hdr_t*)data)->data_length = 0;
			diva_data_q_ack_segment4write	(data);

      diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");

			diva_os_wakeup_read (e->os_context);
		} break;

		case DIVA_UM_IDI_GET_DESCRIPTOR_LIST: {
			int length = diva_user_mode_idi_get_descriptor_list (0, 0xffff);

			DBG_TRC(("A(%d) get descriptor list"))

			if (!(data = diva_data_q_get_segment4write	(&e->data, length))) {
				DBG_ERR(("A(%d) get descriptor list, no free buffer", a->adapter_nr))
				diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
				return (0);
			}
			(void)diva_user_mode_idi_get_descriptor_list(data, length);
			diva_data_q_ack_segment4write	(data);

      diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");

			diva_os_wakeup_read (e->os_context);
		} break;

		case DIVA_UM_IDI_REQ:
		case DIVA_UM_IDI_REQ_MAN:
		case DIVA_UM_IDI_REQ_SIG:
		case DIVA_UM_IDI_REQ_NET:
			DBG_TRC(("A(%d) REQ(%02d)-(%02d)-(%08x)", \
							a->adapter_nr, req->Req, req->ReqCh,
							req->type & DIVA_UM_IDI_REQ_TYPE_MASK))
			switch (process_idi_request (e, req)) {
				case -1:
          diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
					return (-1);
				case -2:
          diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
					diva_os_wakeup_read (e->os_context);
					break;
				default:
          diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
					break;
			}
			break;

		default:
      diva_os_leave_spin_lock (&adapter_lock, &old_irql, "write");
			return (-1);
	}

	DBG_TRC(("A(%d) E(%08x) write=%d", a->adapter_nr, e, ret))

	return (ret);
}

static int diva_um_idi_proxy_add_adapter (divas_um_idi_proxy_t* pE,
																					void* os_handle,
																					const void* src,
																					int length,
																					divas_um_idi_copy_from_user_fn_t cp_fn) {

	diva_os_spin_lock_magic_t old_irql;
	diva_um_idi_proxy_req_t req;
	DESCRIPTOR* t;
	IDI_SYNC_REQ* sync_req;
	IDI_CALL request = 0;
	int ret, req_nr = -1, i, already_installed = 0;

	if (length != sizeof(req)) {
		return (-1);
	}

	if (cp_fn != 0) {
		if ((ret = (*cp_fn)(os_handle, &req, src, length)) < 0)
			return (ret);
	} else {
		memcpy (&req, src, length);
	}

	if (req.Id != DIVA_UM_IDI_REQ_TYPE_REGISTER_ADAPTER ||
			req.logical_adapter_number == 0 || req.descriptor_type == 0 || req.features == 0 ||
			req.name_length == 0 || req.name_length > sizeof(req.name) || req.name[0] == 0) {
		if (req.Id == DIVA_UM_IDI_REQ_TYPE_REGISTER_ADAPTER) {
			req.name[sizeof(req.name)-1] = 0;
			DBG_ERR(("um(%d) failed to register adapter", req.logical_adapter_number))
		} else {
			DBG_ERR(("um(%d) wrong request:%08x", req.logical_adapter_number, req.Id))
		}

		return (-1);
	}

	diva_os_enter_spin_lock (&request_array_lock, &old_irql, "requests");
	for (i = 0; i < sizeof(requests)/sizeof(requests[0]); i++)
		if (requests[i].busy == 0) {
			requests[i].busy = 1;
			req_nr = i;
			break;
		}
	diva_os_leave_spin_lock (&request_array_lock, &old_irql, "requests");

	if (req_nr < 0) {
		DBG_ERR(("um(%d) no free request", req.logical_adapter_number))
		return (0);
	}

		/*
			Register adapter
			*/
	t = (DESCRIPTOR*)diva_os_malloc (0, MAX_DESCRIPTORS*sizeof(*t));
	sync_req = (IDI_SYNC_REQ*)diva_os_malloc (0, sizeof(*sync_req));

	if (t != 0 && sync_req != 0) {
		int x;

		memset (t, 0x00, MAX_DESCRIPTORS*sizeof(*t));
		memset (sync_req, 0x00, sizeof(*sync_req));
		diva_os_read_descriptor_array (t, MAX_DESCRIPTORS*sizeof(*t));

		/*
			Check if adapter already installed
			*/
		for (x = 0; (already_installed == 0 && x < MAX_DESCRIPTORS); x++) {
			if (t[x].request != 0 && t[x].type != IDI_DADAPTER && t[x].type != IDI_DIMAINT) {
				sync_req->xdi_logical_adapter_number.Req = 0;
				sync_req->xdi_logical_adapter_number.Rc = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
				sync_req->xdi_logical_adapter_number.info.logical_adapter_number = 0;
				sync_req->xdi_logical_adapter_number.info.controller             = 0;
				sync_req->xdi_logical_adapter_number.info.total_controllers      = 0;
				(*(t[x].request))((ENTITY *)sync_req);
				already_installed =
					(sync_req->xdi_logical_adapter_number.info.logical_adapter_number == req.logical_adapter_number);

				if (already_installed == 0) {
					sync_req->GetName.Req = 0;
					sync_req->GetName.Rc = IDI_SYNC_REQ_GET_NAME;
					sync_req->GetName.name[0] = 0;
					(*(t[x].request))((ENTITY*)sync_req);

					already_installed = sync_req->GetName.name[0] != 0                     &&
															strlen(sync_req->GetName.name) == strlen(pE->name) &&
															memcmp(sync_req->GetName.name, pE->name, strlen(pE->name)) == 0;
				}
			}
		}

		for (x = 0; (already_installed == 0 && request == 0 && x < MAX_DESCRIPTORS); x++) {
			if (t[x].type == IDI_DADAPTER) {
				request = t[x].request;
			}
		}
	}

	if (t != 0)
		diva_os_free (0, t);
	t = 0;

	if (already_installed == 0 && request != 0) {
		DESCRIPTOR d;

		d.type     = (byte)req.descriptor_type;
		d.channels = (byte)req.channels;
		d.features = (word)req.features;
		d.request  = requests[req_nr].request;

		pE->logical_adapter_number = req.logical_adapter_number;
		pE->controller             = req.controller;
		pE->total_controllers      = req.total_controllers;
		pE->max_entities           = req.max_entities;
		pE->adapter_features       = req.features;
		pE->serial_number          = req.serial_number;
		pE->card_type              = req.card_type;

		if (pE->max_entities == 0 || pE->max_entities > 0xff)
			pE->max_entities = 0xff;

		memcpy (pE->name, req.name, MIN(sizeof(pE->name),sizeof(req.name)));
		pE->name[MIN((sizeof(pE->name)-1), req.name_length)] = 0;

		pE->dadapter_request = request;

		sync_req->didd_remove_adapter.e.Req = 0;
		sync_req->didd_add_adapter.e.Rc = IDI_SYNC_REQ_DIDD_ADD_ADAPTER;
		sync_req->didd_add_adapter.info.descriptor = (void *)&d;

		diva_os_enter_spin_lock (&request_array_lock, &old_irql, "requests");

		requests[req_nr].pE = pE;

		diva_os_leave_spin_lock (&request_array_lock, &old_irql, "requests");

		pE->req_nr = req_nr;
		pE->state = DivaUmIdiProxyRegistered;

		(*request)((ENTITY *)sync_req);
		if (sync_req->didd_add_adapter.e.Rc == 0xff) {
			ret = sizeof(req);
			req_nr = -1;
			DBG_LOG(("um(%d) add adapter %p type:%02x, ch:%d, features:%04x, nr:%d req:%d",
								pE->logical_adapter_number, pE, d.type, d.channels, d.features, pE->req_nr))
		} else {
			pE->req_nr = -1;
			pE->state = DivaUmIdiProxyIdle;
			pE->logical_adapter_number = 0;
			pE->name[0] = 0;
			ret = 0;
		}
	} else
		ret = 0;

	if (sync_req != 0)
		diva_os_free (0, sync_req);

	if (req_nr >= 0) {
		diva_os_enter_spin_lock (&request_array_lock, &old_irql, "requests");
		requests[req_nr].pE   = 0;
		requests[req_nr].busy = 0;
		diva_os_leave_spin_lock (&request_array_lock, &old_irql, "requests");
	}

	return (ret);
}

static void diva_um_idi_proxy_copy_ind_to_user (ENTITY* e, const byte* src, int length) {
  int i = 0;
  int to_copy;

  while ((i < e->RNum) && length) {
    if ((to_copy = MIN(e->R[i].PLength, length))) {
      if (e->R[i].P) {
        memcpy (e->R[i].P, src, to_copy);
      }
      e->R[i].PLength = (word)to_copy;
      length -= to_copy;
      src    += to_copy;
    }
    i++;
  }

  if ((e->RNum = (byte)i) == 0)
    e->R->PLength = 0;
}


static int diva_um_idi_proxy_ind (divas_um_idi_proxy_t* pE,
																	const diva_um_idi_proxy_idi_rc_or_ind_t* hdr,
																	int max_length) {
	diva_os_spin_lock_magic_t old_irql;
	diva_um_idi_proxy_entity_t* pU;
	byte Id = (byte)hdr->Id;
	int ret = -1;

	if (max_length != hdr->data_length)
		return (-1);

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "rc");

	if ((pU = pE->entity_map[Id]) == 0) {
		ret = -1;
	} else if (pU->remove_pending != 0 || pU->e == 0) {
		pU = 0;
		ret = (hdr->data_length + sizeof(*hdr));
	}

	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "rc");

	if (pU != 0) {
		ENTITY* e = pU->e;
		PBUFFER RBuffer;

		e->Ind      = (byte)hdr->RcInd;
		e->IndCh    = (byte)hdr->Ch;
		e->complete = (hdr->data_length <= sizeof(RBuffer.P));
		e->RLength  = hdr->data_length;
		e->RNum     = 0;
		e->RNR      = 0;
		if ((RBuffer.length = MIN(hdr->data_length, sizeof(RBuffer.P)))) {
			memcpy (RBuffer.P, &hdr[1], RBuffer.length);
		}
		e->RBuffer = (DBUFFER*)&RBuffer;
		(*(e->callback))(e);

		if (e->RNR == 1) {
			ret = 0; 
		} else if (e->RNum == 0 || e->RNR == 2) {
			ret = (hdr->data_length + sizeof(*hdr));
		} else {
			e->Ind      = (byte)hdr->RcInd;
			e->complete = 2;
			diva_um_idi_proxy_copy_ind_to_user (e, (const byte*)&hdr[1], hdr->data_length);
			(*(e->callback))(e);
			ret = (hdr->data_length + sizeof(*hdr));
		}
	}

	return (ret);
}

static int diva_um_idi_proxy_rc (divas_um_idi_proxy_t* pE,
																 const diva_um_idi_proxy_idi_rc_or_ind_t* hdr) {
	diva_os_spin_lock_magic_t old_irql;
	diva_um_idi_proxy_entity_t* pU;
	byte Id = (byte)hdr->Id;
	int ret = -1, remove_complete = 0;
	ENTITY* e = 0;

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "rc");
	if ((pU = pE->entity_map[Id]) != 0) {
		if ((hdr->data_length & DIVA_UM_IDI_PROXY_RC_TYPE_REMOVE_COMPLETE) != 0) {
			pE->entity_map[Id] = 0;
			remove_complete = 1;
			if (pU->assign_pending != 0)
				diva_q_remove(&pE->assign_q, &pU->link);
			else
				diva_q_remove(&pE->entity_q, &pU->link);
			if (pU->request_pending != 0)
				diva_q_remove(&pE->req_q, &pU->req_link);
			if (pE->nr_entities != 0)
				pE->nr_entities--;
		}
	}
	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "rc");

	if (pU != 0) {
		if ((e = pU->e) != 0) {
			e->complete = 0xff;
			if (remove_complete != 0) {
				e->Id   = 0;
				e->Rc   = 0xff;
				e->RcCh = 0;
			} else {
				e->Rc   = (byte)hdr->RcInd;
				e->RcCh = (byte)hdr->Ch;
			}
			ret = sizeof(*hdr);
			(*(e->callback))(e);
		}
	} else if ((((byte)hdr->RcInd) & 0xf0) == ASSIGN_RC) { /* Process assign Rc */
		byte Rc = (byte)hdr->RcInd;

		diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "rc");
		if ((pU = (diva_um_idi_proxy_entity_t*)diva_q_get_head(&pE->assign_q)) != 0 &&
				pU->assign_pending != 0 && pU->request_pending == 0 && pU->e != 0) {
			diva_q_remove(&pE->assign_q, &pU->link);
			pU->assign_pending = 0;
			e = pU->e;
			e->complete = 0xff;
			e->Id   = Id;
			e->Rc   = Rc;
			e->RcCh = (byte)hdr->RcInd;

			if (Rc == ASSIGN_OK) {
				pE->entity_map[Id] = pU;
				diva_q_add_tail (&pE->entity_q, &pU->link);
			} else {
				remove_complete = 1;
				if (pE->nr_entities != 0)
					pE->nr_entities--;
			}
		} else {
			DBG_ERR(("um(%d) unknown assign rc Id:%02x, Rc:%02x",
							pE->logical_adapter_number, (byte)hdr->Id, (byte)hdr->RcInd))
		}
		diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "rc");

		ret = sizeof(*hdr);
		if (e != 0)
			(*(e->callback))(e);

	} else {
		DBG_ERR(("um(%d) unknown rc Id:%02x, Rc:%02x",
							pE->logical_adapter_number, (byte)hdr->Id, (byte)hdr->RcInd))
	}

	if (remove_complete != 0 && pU != 0) {
		DBG_TRC(("um(%d) remove %p", pE->logical_adapter_number, pU))
		diva_os_free (0, pU);
	}

	return (ret);
}

static int diva_um_idi_proxy_write (void* entity,
																		void* os_handle,
																		const void* src,
																		int length,
																		divas_um_idi_copy_from_user_fn_t cp_fn) {
	divas_um_idi_proxy_t* pE = (divas_um_idi_proxy_t*)entity;
	int ret = length;

	if (pE->entity_function != DivaUmIdiProxy)
		return (-1);

	if (pE->state == DivaUmIdiProxyIdle) {
		ret = diva_um_idi_proxy_add_adapter (pE, os_handle, src, length, cp_fn);
	} else {
		if (length >= sizeof(diva_um_idi_proxy_idi_rc_or_ind_t)) {
			diva_um_idi_proxy_idi_rc_or_ind_t* hdr, hdr_data;

			if (cp_fn != 0) {
				if (length == sizeof(*hdr))
					hdr = &hdr_data;
				else
					hdr = diva_os_malloc (0, length);

				if (hdr != 0)
					ret = (*cp_fn)(os_handle, hdr, src, length);
				else
					ret = 0;
			} else {
				hdr = (diva_um_idi_proxy_idi_rc_or_ind_t*)src;
			}

			if (ret > 0) {
				if ((hdr->data_length & 0x8000) != 0)
					ret = diva_um_idi_proxy_rc (pE, hdr);
				else
					ret = diva_um_idi_proxy_ind (pE, hdr, length-sizeof(*hdr));
			}

			if (hdr != (diva_um_idi_proxy_idi_rc_or_ind_t*)src && hdr != &hdr_data && hdr != 0) {
				diva_os_free (0, hdr);
			}
		} else {
			ret = -1;
		}
	}

	return (ret);
}

static word diva_copy_xdescr (byte* dst, ENTITY* e, int max_length) {
  dword i = 0;
  word length = 0;

  while (i < e->XNum) {
    if (e->X[i].PLength && e->X[i].P) {
      if ((length + e->X[i].PLength) > max_length)
				return (0xffff);
      memcpy (&dst[length], &e->X[i].P[0], e->X[i].PLength);
      length += e->X[i].PLength;
    }
    i++;
  }

  return (length);
}

/*
	Request function
	*/
static void diva_um_proxy_user_request (divas_um_idi_proxy_t* pE, ENTITY* e) {
	diva_os_spin_lock_magic_t old_irql;

	if (e->Req == 0) {
		IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ*)e ;

		switch (e->Rc) {
			case IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER: {
				diva_xdi_get_logical_adapter_number_s_t *pI = &syncReq->xdi_logical_adapter_number.info;

				pI->logical_adapter_number = pE->logical_adapter_number;
				pI->controller             = pE->controller;
				pI->total_controllers      = pE->total_controllers;
			} break;

			case IDI_SYNC_REQ_GET_NAME:
				strcpy (&syncReq->GetName.name[0], pE->name);
				break;

			case IDI_SYNC_REQ_GET_SERIAL:
				syncReq->GetSerial.serial = pE->serial_number;
				break;

			case IDI_SYNC_REQ_GET_CARDTYPE:
				syncReq->GetCardType.cardtype = pE->card_type;
				break;

			case IDI_SYNC_REQ_GET_FEATURES:
				syncReq->GetFeatures.features = (word)pE->adapter_features;
				break;

			case IDI_SYNC_REQ_GET_INTERNAL_REQUEST_PROC:
				syncReq->diva_get_internal_request.Rc = 0xff;
				syncReq->diva_get_internal_request.info.request_proc =
								(void (*)(void*,ENTITY*))diva_um_proxy_user_request;
				syncReq->diva_get_internal_request.info.context = pE;
				break;
		}

		return;
	}

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req");
	if (pE->nr_pending_requests < sizeof(pE->pending_requests_q)/sizeof(pE->pending_requests_q[0])) {
		e->No = 1;
		pE->pending_requests_q[pE->nr_pending_requests++] = e;
		diva_os_wakeup_read (pE->os_context);
		e = 0;
	}
	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req");

	if (e != 0) {
		e->No = 0;
		e->Rc = ASSIGN_RC | OUT_OF_RESOURCES;
		DBG_ERR(("um(%d) ignore request, no free resources", pE->logical_adapter_number))
	}
}

/* --------------------------------------------------------------------------
			CALLBACK FROM XDI
	 -------------------------------------------------------------------------- */
DIVA_UM_IDI_ENTITY_CALLBACK_TYPE
void diva_um_idi_xdi_callback (ENTITY* entity) {
	divas_um_idi_entity_t* e = DIVAS_CONTAINING_RECORD(entity,
                                   divas_um_idi_entity_t, e); 
  diva_os_spin_lock_magic_t old_irql;
	int call_wakeup = 0;
	int do_lock = e->entity_type == DivaUmIdiEntityTypeManagement;

	if (do_lock) {
  	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
	}

	if ((e->status & DIVA_UM_IDI_TIMEOUT) != 0) {
		DBG_ERR(("E(%p) response timeout", e))
		if (e->e.complete == 255) {
			e->e.Rc = 0;
		} else {
			e->e.RNum = 0;
			e->e.RNR =  2;
			e->e.Ind = 0;
		}
		if (do_lock) {
    	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
		}
		return;
	}

	if (e->e.complete == 255) {
		if (!(e->status & DIVA_UM_IDI_REMOVE_PENDING)) {
      diva_um_idi_stop_wdog (e);
    }
		if ((call_wakeup = process_idi_rc (e, e->e.Rc))) {
			if (e->rc_count) {
				e->rc_count--;
			}
		}
		e->e.Rc = 0;

		if (do_lock) {
    	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
		}

		if (call_wakeup) {
			diva_os_wakeup_read (e->os_context);
			diva_os_wakeup_close (e->os_context);
		}
	} else {
		if (e->status & DIVA_UM_IDI_REMOVE_PENDING) {
			e->e.RNum = 0;
			e->e.RNR =  2;
		} else {
			call_wakeup = process_idi_ind (e, e->e.Ind);
		}
		e->e.Ind = 0;
		if (do_lock) {
    	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
		}
		if (call_wakeup) {
			diva_os_wakeup_read (e->os_context);
		}
	}
}

static word create_management_request (byte* P, const char* name) {
  byte var_length;
  byte* plen;

  var_length = (byte)strlen (name);

  *P++ = ESC;
  plen = P++;
  *P++ = 0x80; /* MAN_IE */
  *P++ = 0x00; /* Type */
  *P++ = 0x00; /* Attribute */
  *P++ = 0x00; /* Status */
  *P++ = 0x00; /* Variable Length */
  *P++ = var_length;
  memcpy (P, name, var_length);
  P += var_length;
  *plen = var_length + 0x06;

  return ((word)(var_length + 0x08));
}

static void send_init_license(divas_um_idi_entity_t* me) {

	DBG_TRC(("A(%d) me:%p le:%p init license",
						me->adapter->adapter_nr,
						me,
						diva_q_get_head(&me->multiplex.master.active_multiplex_q)))

  me->rc_count = 1;
  me->multiplex.master.state         = DivaUmIdiMasterResponsePending;
  me->multiplex.master.license_state = DivaUmIdiLicenseInit;
  me->status |= DIVA_UM_IDI_RC_PENDING;
  me->cbuffer_length = 0;
  me->cbuffer_pos  = 0;

  me->e.Req        = MAN_EXECUTE;
  me->e.ReqCh      = 0;
  me->e.X->PLength = create_management_request (me->buffer, "Info\\License\\Init");
  me->e.X->P       = me->buffer;

  (*(me->adapter->d.request))(&me->e);
}

/*
	Returns number of bytes written
	*/
static byte write_license_data (divas_um_idi_entity_t* me) {
	byte to_write = 0;

  if (me->cbuffer_length != 0) {
		const char* name = "Info\\License\\Write";
		int name_length = strlen(name);
		byte* P = &me->buffer[0];

		to_write = MIN(((byte)me->cbuffer_length), 100);

		DBG_TRC(("A(%d) me:%p le:%p write license, offset=%d length=%d",
							me->adapter->adapter_nr,
							me,
							diva_q_get_head(&me->multiplex.master.active_multiplex_q),
							me->cbuffer_pos, to_write))
		DBG_BLK((&me->cbuffer[me->cbuffer_pos], to_write))

		me->rc_count = 1;
		me->multiplex.master.state         = DivaUmIdiMasterResponsePending;
		me->multiplex.master.license_state = DivaUmIdiLicenseWrite;
		me->status |= DIVA_UM_IDI_RC_PENDING;

		*P++ = ESC;
		*P++ = to_write+1+name_length+6; /* Element length */
		*P++ = 0x80; /* MAN_IE */
		*P++ = 0x05; /* Type MI_NUMBER */
		*P++ = 0x00; /* Attribute */
		*P++ = 0x00; /* Status */
		*P++ = to_write+1; /* Value Length */
		*P++ = (byte)name_length; /* Path Length */
		memcpy (P, name, name_length);
		P += name_length;
		*P++ = to_write;
		memcpy (P, &me->cbuffer[me->cbuffer_pos], to_write);
		me->cbuffer_length -= to_write;
		me->cbuffer_pos    += to_write;

		me->e.Req        = MAN_WRITE;
		me->e.ReqCh      = 0;
		me->e.X->PLength = to_write+1+name_length+8;
		me->e.X->P       = me->buffer;

		(*(me->adapter->d.request))(&me->e);
	}

	return (to_write);
}


static void read_license_response (divas_um_idi_entity_t* me) {
	DBG_TRC(("A(%d) me:%p le:%p read license response",
						me->adapter->adapter_nr,
						me,
						diva_q_get_head(&me->multiplex.master.active_multiplex_q)))

  me->rc_count = 2;
  me->multiplex.master.state         = DivaUmIdiMasterResponsePending;
  me->multiplex.master.license_state = DivaUmIdiLicenseWaitResponse;
  me->status |= DIVA_UM_IDI_RC_PENDING;
  me->cbuffer_length = 0;
  me->cbuffer_pos  = 0;

  me->e.Req        = MAN_READ;
  me->e.ReqCh      = 0;
	me->e.X->PLength = create_management_request (me->buffer, "Info\\License");
  me->e.X->P       = me->buffer;

  (*(me->adapter->d.request))(&me->e);
}

static void delivery_license_response(divas_um_idi_entity_t* me,
																			divas_um_idi_entity_t* le) {
	const byte* msg = &me->cbuffer[0];
	word length = 0, i = 0, msg_length;
	PBUFFER r;

	if (msg[0] != ESC) {
		if (me->cbuffer_length > 11 && *msg == MAN_INFO_IND &&
				READ_WORD(&msg[1])+3 < me->cbuffer_length) {
			i = 3;
			me->cbuffer_length = READ_WORD(&msg[1])+3;
		}
	}

	me->cbuffer[me->cbuffer_length+i] = 0;


	while (i < me->cbuffer_length - 8 && msg[i] == ESC) {
		msg_length = msg[i+1]+2;
		if (i+msg_length <= me->cbuffer_length && msg_length > 8 && msg[i+3] == 5 && msg[i+7] != 0) {
			if ((msg[i+7] == sizeof(DIVA_LICENSE_RESPONSE_VARIABLE) ||
					msg[i+7] == sizeof(DIVA_LICENSE_RESPONSE_VARIABLE) + 1) &&
					memcmp (&msg[i+8], DIVA_LICENSE_RESPONSE_VARIABLE, sizeof(DIVA_LICENSE_RESPONSE_VARIABLE)-1) == 0) {
				if (length < sizeof(me->buffer)) {
					memcpy (&me->buffer[length], &msg[i+8+msg[i+7]+1], msg[i+8+msg[i+7]]);
					length += msg[i+8+msg[i+7]];
				}
			}
		}
		i += msg_length;
	}

	me->cbuffer_length = 0;

	if (length == 0)
		length = 512;

	le->e.Rc    = 0;
	le->e.Ind   = (byte)MAN_INFO_IND;
	le->e.IndCh = 0;
	le->e.complete = (length <= sizeof(r.P));
	le->e.RLength  = (word)length;
	le->e.RNum     = 0;
	le->e.RNR      = 0;

	if ((r.length = (word)MIN(length, sizeof(r.P))) != 0)
		memcpy (r.P, me->buffer, r.length);

	if ((le->status & DIVA_UM_IDI_REMOVE_PENDING) != 0) {
		diva_um_idi_stop_wdog (le);
	}

	if (le->rc_count != 0)
		le->rc_count--;

	le->status &= ~DIVA_UM_IDI_RC_PENDING;

	le->e.RBuffer = (DBUFFER*)&r;
	(*(le->e.callback))(&le->e);

	if (le->e.RNR == 0 && le->e.RNum != 0 && le->e.R->PLength >= length) {
		memcpy (le->e.R->P, me->buffer, length);
		le->e.R->PLength = (word)length;
		le->e.RNum = 1;
		le->e.Ind   = (byte)MAN_INFO_IND;
		le->e.complete = 2;
		(*(le->e.callback))(&le->e);
	}

	le->e.Ind   = 0;
}

DIVA_UM_IDI_ENTITY_CALLBACK_TYPE
void diva_um_idi_license_xdi_callback (ENTITY* entity) {
	divas_um_idi_entity_t* me = DIVAS_CONTAINING_RECORD(entity, divas_um_idi_entity_t, e);
  diva_os_spin_lock_magic_t old_irql;

	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "xdi_callback");


	if ((me->status & DIVA_UM_IDI_TIMEOUT) != 0) {
		DBG_ERR(("E(%p) response timeout", me))
		if (me->e.complete == 255) {
			me->e.Rc = 0;
		} else {
			me->e.RNum = 0;
			me->e.RNR =  2;
			me->e.Ind = 0;
		}
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
		return;
	}

	if (me->e.complete == 255) {
		me->status &= ~DIVA_UM_IDI_RC_PENDING;
		if (me->multiplex.master.state == DivaUmIdiMasterAssignPending) {
			divas_um_idi_entity_t* le;

			me->multiplex.master.state = me->e.Rc == ASSIGN_OK ?
																			DivaUmIdiMasterAssigned : DivaUmIdiMasterOutOfService;

			me->rc_count = 0;
			me->status   = 0;

			DBG_TRC(("A(%d) me:%p license assign:%s",
								me->adapter->adapter_nr, me, me->e.Rc == ASSIGN_OK ? "ok" : "failed"))

			while ((le = (divas_um_idi_entity_t*)diva_q_get_head(
															&me->multiplex.master.active_multiplex_q)) != 0) {
				diva_q_remove   (&me->multiplex.master.active_multiplex_q, &le->link);
				le->multiplex.logical.on_active_q = 0;
				diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &le->link);

				le->e.Id = 2;
				le->e.Rc = me->e.Rc;
				le->e.complete = 255;
				(*(le->e.callback))(&le->e);
			}
		} else if (me->multiplex.master.state == DivaUmIdiMasterResponsePending) {
			divas_um_idi_entity_t* le =
				(divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q);

			if (me->e.Rc == OK) {
				if (--me->rc_count == 0) {
					me->multiplex.master.state = DivaUmIdiMasterAssigned;
					switch (me->multiplex.master.license_state) {
						case DivaUmIdiLicenseIdle:
							send_init_license(me);
							break;
						case DivaUmIdiLicenseInit:
							if (((diva_um_idi_req_hdr_t*)&le->buffer[0])->data_length != 0)
								memcpy (me->cbuffer,
												&le->buffer[sizeof(diva_um_idi_req_hdr_t)],
												((diva_um_idi_req_hdr_t*)&le->buffer[0])->data_length);
							me->cbuffer_length = ((diva_um_idi_req_hdr_t*)&le->buffer[0])->data_length;
							me->cbuffer_pos    = 0;
						case DivaUmIdiLicenseWrite:
							if (write_license_data(me) == 0) {
								read_license_response(me);
							}
							break;
						case DivaUmIdiLicenseWaitResponse:
							diva_q_remove   (&me->multiplex.master.active_multiplex_q, &le->link);
							le->multiplex.logical.on_active_q = 0;
							diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &le->link);
							delivery_license_response(me, le);
							me->multiplex.master.license_state = DivaUmIdiLicenseIdle;
							me->cbuffer_length = 0;
							break;

						default:
							break;
					}
				}
			} else {
				me->multiplex.master.license_state = DivaUmIdiLicenseIdle;
				me->multiplex.master.state = DivaUmIdiMasterAssigned;
				diva_q_remove   (&me->multiplex.master.active_multiplex_q, &le->link);
				le->multiplex.logical.on_active_q = 0;
				diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &le->link);

				me->rc_count = 0;
				le->e.Rc = me->e.Rc;
				le->e.complete = 255;
				(*(le->e.callback))(&le->e);
			}
		} else if (me->multiplex.master.state == DivaUmIdiMasterResponsePendingCancelled) {
			if (me->e.Rc == OK)
				me->rc_count--;
			else
				me->rc_count = 0;

			if (me->rc_count == 0) {
				me->multiplex.master.state = DivaUmIdiMasterAssigned;
				me->multiplex.master.license_state = DivaUmIdiLicenseIdle;
			}
		} else if (me->multiplex.master.state == DivaUmIdiMasterRemovePending) {
			diva_q_remove (&me->adapter->entity_q, &me->link);
			diva_q_add_tail (&free_mem_q, &me->link);
			DBG_TRC (("E(%08x) me ls remove complete", me))
			me = 0;
		}
		if (me != 0)
			me->e.Rc = 0;
	} else {
		if (me->e.complete != 2) {
			if (me->multiplex.master.state != DivaUmIdiMasterResponsePending) {
				me->e.RNum = 0;
				me->e.RNR  = 2;
				if (me->rc_count != 0 && --me->rc_count == 0 &&
						me->multiplex.master.state == DivaUmIdiMasterResponsePendingCancelled)
					me->multiplex.master.state = DivaUmIdiMasterAssigned;
			} else {
				me->e.RNum       = 1;
				me->e.R->P       = &me->cbuffer[0];
				me->e.R->PLength = (word)sizeof(me->cbuffer) - 1;
			}
		} else {
			if (me->rc_count != 0 && --me->rc_count == 0 &&
					me->multiplex.master.state == DivaUmIdiMasterResponsePendingCancelled) {
				me->multiplex.master.state = DivaUmIdiMasterAssigned;
				me->cbuffer_length = 0;
			}

			if (me->multiplex.master.state == DivaUmIdiMasterResponsePending && me->rc_count == 0) {
				divas_um_idi_entity_t* le =
					(divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q);
				me->multiplex.master.state = DivaUmIdiMasterAssigned;
				me->cbuffer_length = me->e.R->PLength;
				diva_q_remove   (&me->multiplex.master.active_multiplex_q, &le->link);
				le->multiplex.logical.on_active_q = 0;
				diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &le->link);
				delivery_license_response(me, le);
				me->multiplex.master.license_state = DivaUmIdiLicenseIdle;
				me->cbuffer_length = 0;
			}
		}
		me->e.Ind = 0;
	}

	if (me != 0 && me->multiplex.master.state == DivaUmIdiMasterAssigned) {
		if (diva_q_get_head(&me->multiplex.master.active_multiplex_q) != 0 &&
				me->multiplex.master.license_state == DivaUmIdiLicenseIdle) {
			send_init_license(me);
		} else if (diva_q_get_head(&me->multiplex.master.active_multiplex_q) == 0 &&
							 diva_q_get_head(&me->multiplex.master.idle_multiplex_q) == 0) {
			me->e.Req = REMOVE;
			me->e.X->PLength  = 1;
			me->e.X->P        = "";
			me->multiplex.master.state = DivaUmIdiMasterRemovePending;
			me->rc_count = 1;
			me->status |= DIVA_UM_IDI_REMOVED;
			me->status |= DIVA_UM_IDI_REMOVE_PENDING;

			DBG_TRC(("A(%d) me:%p no ls entities, start remove", me->adapter->adapter_nr, me))
			(*(me->adapter->d.request))(&me->e);
		}
	}

	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
}

static int check_chained_indication (const byte* src) {
	if (src[0] == 0x7f && /* ESC element */
			src[2] == 0x80 && /* man info element */
			src[3] == 0x82 && /* MI_UINT */
			(src[4] & 0x04) != 0  && /* is an ESC variable */
			src[7] == 9    && /* path len */
			memcmp((char*)&src[8], "ind_count", 9) == 0 &&
			src[6] == 1    /* length of variable */) {
		return (src[17]);
	}

	return (0);
}

static int check_chained_combi_indication (const byte* cbuffer, word cbuffer_length) {
	int ind_count = 0;
	word cbuffer_pos = 0, this_ind_length;

  while (cbuffer_length > 3 && cbuffer[cbuffer_pos] != 0) {
    /*
      Byte 0   - Ind
      Byte 1,2 - data length
      */
    this_ind_length = (word)cbuffer[cbuffer_pos+1] | ((word)cbuffer[cbuffer_pos+2] << 8);
    if (this_ind_length + 4 > cbuffer_length) {
      /*
        Indication too long
        */
			return (0);
    }
		if (cbuffer[cbuffer_pos] == MAN_INFO_IND) {
			if (ind_count == 0) {
				ind_count = check_chained_indication (&cbuffer[cbuffer_pos]);
			} else {
				ind_count--;
			}
		}
    cbuffer_pos += (this_ind_length+4);
    cbuffer_length -= (this_ind_length+4);
  }

	return (ind_count);
}

DIVA_UM_IDI_ENTITY_CALLBACK_TYPE
void diva_um_idi_master_xdi_callback (ENTITY* entity) {
	divas_um_idi_entity_t* me = DIVAS_CONTAINING_RECORD(entity, divas_um_idi_entity_t, e); 
  diva_os_spin_lock_magic_t old_irql;
	int me_removed  = 0;
	byte delivery_to_user = 0;

	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "xdi_callback");

	if ((me->status & DIVA_UM_IDI_TIMEOUT) != 0) {
		DBG_ERR(("E(%p) response timeout", me))
		if (me->e.complete == 255) {
			me->e.Rc = 0;
		} else {
			me->e.RNum = 0;
			me->e.RNR  = 2;
			me->e.Ind  = 0;
		}
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
		return;
	}

	if (me->e.complete == 255) {
		if (me->multiplex.master.state == DivaUmIdiMasterAssignPending) {
			divas_um_idi_entity_t* le;

			me->multiplex.master.state = me->e.Rc == ASSIGN_OK ?
																			DivaUmIdiMasterAssigned : DivaUmIdiMasterOutOfService;
			
			me->rc_count = 0;
			me->status   = 0;

			DBG_TRC(("A(%d) me:%p assign:%s", me->adapter->adapter_nr, me, me->e.Rc == ASSIGN_OK ? "ok" : "failed"))

			while ((le = (divas_um_idi_entity_t*)diva_q_get_head(
															&me->multiplex.master.active_multiplex_q)) != 0) {
				diva_q_remove   (&me->multiplex.master.active_multiplex_q, &le->link);
				le->multiplex.logical.on_active_q = 0;
				diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &le->link);

				le->e.Id = 2;
				le->e.Rc = me->e.Rc;
				le->e.complete = 255;
				(*(le->e.callback))(&le->e);
			}

		} else if (me->multiplex.master.state == DivaUmIdiMasterResponsePending ||
				me->multiplex.master.state == DivaUmIdiMasterResponsePendingCancelled) {

			DBG_TRC(("A(%d) me:%p Rc:%02x", me->adapter->adapter_nr, me, me->e.Rc))

			if (me->e.Rc == OK)
				me->rc_count--;
			else
				me->rc_count = 0;
			me->status &= ~DIVA_UM_IDI_RC_PENDING;

			if (me->rc_count == 0) {
				if (me->multiplex.master.state == DivaUmIdiMasterResponsePending)
					delivery_to_user = me->e.Rc;
				me->multiplex.master.state = DivaUmIdiMasterAssigned;
				me->status &= ~DIVA_UM_IDI_ME_CHAINED_IND_PENDING;
			}
		} else if (me->multiplex.master.state == DivaUmIdiMasterRemovePending) {
			me_removed = 1;
		}
		me->e.Rc = 0;
	} else {
		if (me->e.complete != 2) {
			if (me->multiplex.master.state != DivaUmIdiMasterResponsePending) {
				me->e.RNR  = 2;
				me->e.RNum = 0;
				if (--me->rc_count == 0) {
					me->multiplex.master.state = DivaUmIdiMasterAssigned;
					me->status &= ~DIVA_UM_IDI_ME_CHAINED_IND_PENDING;
				}
			} else {
        me->e.RNum       = 1;
        me->e.R->P       = &me->cbuffer[0];
        me->e.R->PLength = (word)sizeof(me->cbuffer);
        me->cbuffer_pos  = me->e.Ind;
			}
		} else {
			if (me->multiplex.master.state != DivaUmIdiMasterResponsePending) {
        me->cbuffer_pos  = 0;
				me->cbuffer_length = 0;
				if (--me->rc_count == 0)
					me->multiplex.master.state = DivaUmIdiMasterAssigned;
			} else {
				me->cbuffer_length = me->e.R->PLength + 1;

				if ((me->status & DIVA_UM_IDI_ME_CHAINED_IND_PENDING) == 0) {
					int chained_ind_count = 0;

					if (me->cbuffer_pos == MAN_INFO_IND) {
						chained_ind_count = check_chained_indication (&me->cbuffer[0]);
					} else if (me->cbuffer_pos == MAN_COMBI_IND) {
						chained_ind_count = check_chained_combi_indication (&me->cbuffer[0], me->e.R->PLength);
					}

					if (chained_ind_count != 0) {
						DBG_TRC(("A(%d) me:%p chained ind:%d", me->adapter->adapter_nr, me, chained_ind_count))
						me->rc_count += chained_ind_count;
						me->status |= DIVA_UM_IDI_ME_CHAINED_IND_PENDING;
						delivery_to_user = OK;
					}
				} else {
					delivery_to_user = OK;
				}

				if (--me->rc_count == 0) {
					me->multiplex.master.state = DivaUmIdiMasterAssigned;
					delivery_to_user = OK;
					me->status &= ~DIVA_UM_IDI_ME_CHAINED_IND_PENDING;
				}
			}
		}
		me->e.Ind  = 0;
	}

	if (delivery_to_user != 0) {
		divas_um_idi_entity_t* e_usr =
				(divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q);

		if ((me->status & DIVA_UM_IDI_ME_CHAINED_IND_PENDING) == 0) {
			diva_q_remove   (&me->multiplex.master.active_multiplex_q, &e_usr->link);
			e_usr->multiplex.logical.on_active_q = 0;
			diva_q_add_tail (&me->multiplex.master.idle_multiplex_q, &e_usr->link);

			e_usr->e.Ind = 0;
			e_usr->e.Rc  = delivery_to_user;
			e_usr->e.complete = 255;
			(*(e_usr->e.callback))(&e_usr->e);
		}
		if (delivery_to_user == OK && me->cbuffer_length != 0) {
			PBUFFER r;

			me->cbuffer_length--;
			e_usr->e.Rc    = 0;
			e_usr->e.Ind   = (byte)me->cbuffer_pos;
			e_usr->e.IndCh = 0;
      e_usr->e.complete = (me->cbuffer_length <= sizeof(r.P));
      e_usr->e.RLength  = (word)me->cbuffer_length;
      e_usr->e.RNum     = 0;
      e_usr->e.RNR      = 0;

			if ((r.length = (word)MIN(me->cbuffer_length, sizeof(r.P))) != 0)
				memcpy (r.P, me->cbuffer, r.length);

      e_usr->e.RBuffer = (DBUFFER*)&r;
			(*(e_usr->e.callback))(&e_usr->e);

			if (e_usr->e.RNR == 0 && e_usr->e.RNum != 0 && e_usr->e.R->PLength >= me->cbuffer_length) {
				if (me->cbuffer_length == 0) {
					e_usr->e.RNum = 0;
					e_usr->e.R->PLength = 0;
				} else {
					memcpy (e_usr->e.R->P, me->cbuffer, me->cbuffer_length);
					e_usr->e.R->PLength = (word)me->cbuffer_length;
					e_usr->e.RNum = 1;
				}

				e_usr->e.Ind   = (byte)me->cbuffer_pos;
				e_usr->e.complete = 2;
				(*(e_usr->e.callback))(&e_usr->e);
				e_usr->e.Ind   = 0;
			}
		}
		me->cbuffer_length = 0;
	}

	if (me_removed != 0) {
		diva_q_remove (&me->adapter->entity_q, &me->link);
		diva_q_add_tail(&free_mem_q, &me->link);
		DBG_TRC (("E(%08x) me remove complete", me))
	} else if (me->rc_count == 0 && me->multiplex.master.state == DivaUmIdiMasterAssigned) {
		divas_um_idi_entity_t* req_e =
				(divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q);
		diva_um_idi_req_hdr_t* me_req = (diva_um_idi_req_hdr_t*)&me->buffer;

		if (req_e == 0) {
			if (diva_q_get_head(&me->multiplex.master.idle_multiplex_q) == 0) {
				me->e.Req = REMOVE;
				me->e.X->PLength  = 1;
				me->e.X->P			  = "";
				me->multiplex.master.state = DivaUmIdiMasterRemovePending;
				me->rc_count = 1;
				me->status |= DIVA_UM_IDI_REMOVED;
				me->status |= DIVA_UM_IDI_REMOVE_PENDING;

				DBG_TRC(("A(%d) me:%p no entities, start remove", me->adapter->adapter_nr, me))
				(*(me->adapter->d.request))(&me->e);
			}
		} else {
			memcpy (me->buffer,
							req_e->buffer,
							sizeof(*me_req)+((diva_um_idi_req_hdr_t*)&req_e->buffer[0])->data_length);
			me->multiplex.master.state = DivaUmIdiMasterResponsePending;

			me->e.Req        = (byte)me_req->Req;
			me->e.ReqCh      = (byte)me_req->ReqCh;
			me->e.X->PLength = (word)me_req->data_length;
			me->e.X->P        = (byte*)&me_req[1];

			me->rc_count = 1;
			me->status |= DIVA_UM_IDI_RC_PENDING;
			me->status &= ~DIVA_UM_IDI_ME_CHAINED_IND_PENDING;
			me->rc_count += (me->e.Req == MAN_READ);
			me->cbuffer_length = 0;

			DBG_SEND((me->e.X->P, MIN(me->e.X->PLength, 255)))

			(*(me->adapter->d.request))(&me->e);
		}
	}

	diva_os_leave_spin_lock (&adapter_lock, &old_irql, "xdi_callback");
}

static int process_logical_idi_request (divas_um_idi_entity_t* le, diva_um_idi_req_hdr_t* le_req) {
	divas_um_idi_entity_t* me = le->multiplex.logical.xdi_entity;
	dword type = le_req->type & DIVA_UM_IDI_REQ_TYPE_MASK;
	byte Req = (byte)le_req->Req;

	if (type != DIVA_UM_IDI_REQ_TYPE_MAN || me == 0 || me->adapter == 0 ||
			me->adapter->d.request == diva_um_idi_dummy_request || le->multiplex.logical.on_active_q != 0) {
		return (-1);
	}

	if (le->e.Id == 0 || le->e.callback == 0) { /* ASSIGN */
		le->e.Req = ASSIGN;
		le->e.Id = 0;
		le->e.Rc = 0;

		le->e.XNum     = 1;
		le->e.RNum     = 1;
		le->e.callback = diva_um_idi_xdi_callback;
		le->e.X        = &le->XData;
		le->e.R        = &le->RData;


		le->rc_count = 1;
		le->status |= DIVA_UM_IDI_RC_PENDING;
		le->status |= DIVA_UM_IDI_ASSIGN_PENDING;

		if (me->multiplex.master.state >= DivaUmIdiMasterRemovePending) {
			/*
				Master entity removed, report error
				Leave on idle queue
				*/
			le->e.Rc = ASSIGN_RC | OUT_OF_RESOURCES;
			le->e.complete = 255;
			(*(le->e.callback))(&le->e);

			return (0);
		}
		if (me->multiplex.master.state > DivaUmIdiMasterAssignPending) {
			/*
				Master entity assigned, report success immediatelly
				Leave on idle queue
				*/
			le->e.Id = 2;
			le->e.Rc = ASSIGN_OK;
      le->e.complete = 255;
			(*(le->e.callback))(&le->e);

			return (0);
		}
		/*
			Master entity not assigned or assign in progress
			Update entity state and move to active queue
			*/
		le->multiplex.logical.on_active_q = 1;
		diva_q_remove   (&me->multiplex.master.idle_multiplex_q,   &le->link);
		diva_q_add_tail (&me->multiplex.master.active_multiplex_q, &le->link);
		diva_um_idi_start_wdog (le);
	} else {
		switch (Req) {
			case MAN_READ:
			case MAN_WRITE:
			case MAN_EXECUTE:
			case MAN_LOCK:
			case MAN_UNLOCK:
				break;

			default:
				return (-1);
		}
		/*
			Other request
			*/
		le->multiplex.logical.on_active_q = 1;
		diva_q_remove   (&me->multiplex.master.idle_multiplex_q,   &le->link);
		diva_q_add_tail (&me->multiplex.master.active_multiplex_q, &le->link);
		diva_um_idi_start_wdog (le);
	}


	if (me->multiplex.master.state == DivaUmIdiMasterIdle) {
		byte options = 0x03;
		diva_um_idi_req_hdr_t* req = (diva_um_idi_req_hdr_t*)&me->buffer[0];
		byte* data = (byte*)&req[1];

		me->e.Req = ASSIGN;
		me->e.ReqCh = 0;
		me->e.Id  = MAN_ID;
		me->e.Rc  = 0;
		me->e.Ind = 0;
		me->cbuffer_length = 0;
		me->cbuffer_pos    = 0;
		data[0] = LLI;
		data[1] = 8;
		data[2] = options | 0x04;
		data[3] = 0;
		data[4] = 0;
		data[5] = 0;
		data[6] = 0;
		data[7] = 0;
		data[8] = (byte)sizeof(me->cbuffer);
		data[9] = (byte)(sizeof(me->cbuffer) >> 8);
		data[10] = 0;
		req->data_length = 11;

    me->e.XNum = 1;
    me->e.RNum = 1;

		if (diva_user_mode_is_proxy_adapter (me->adapter->d.request) >= 0) {
			me->e.callback = (me->multiplex.master.type == DivaUmIdiMasterTypeIdi) ?
												diva_um_idi_master_xdi_callback : diva_um_idi_license_xdi_callback;
		} else {
			me->e.callback = (me->multiplex.master.type == DivaUmIdiMasterTypeIdi) ?
												diva_um_idi_master_xdi_os_callback : diva_um_idi_license_xdi_os_callback;
		}

    me->e.X = &me->XData;
    me->e.R = &me->RData;
		me->e.X->PLength = (word)req->data_length;
		me->e.X->P			 = (byte*)&req[1];

		me->multiplex.master.state = DivaUmIdiMasterAssignPending;

		me->rc_count = 1;
		me->status |= DIVA_UM_IDI_RC_PENDING;
		me->status |= DIVA_UM_IDI_ASSIGN_PENDING;

    (*(me->adapter->d.request))(&me->e);
	} else if (me->multiplex.master.state == DivaUmIdiMasterAssigned) {
		if (me->multiplex.master.type != DivaUmIdiMasterTypeLicense) {
			divas_um_idi_entity_t* req_e =
					(divas_um_idi_entity_t*)diva_q_get_head(&me->multiplex.master.active_multiplex_q);
			diva_um_idi_req_hdr_t* me_req = (diva_um_idi_req_hdr_t*)&me->buffer;
			diva_um_idi_req_hdr_t* req = (diva_um_idi_req_hdr_t*)&req_e->buffer;

			me->multiplex.master.state = DivaUmIdiMasterResponsePending;

			memcpy (me->buffer, req_e->buffer, sizeof(*req) + req->data_length);

			me->e.Req        = (byte)me_req->Req;
			me->e.ReqCh      = (byte)me_req->ReqCh;
			me->e.X->PLength = (word)me_req->data_length;
			me->e.X->P			  = (byte*)&me_req[1];

			me->rc_count = 1;
			me->status |= DIVA_UM_IDI_RC_PENDING;
			me->rc_count += (me->e.Req == MAN_READ);

			DBG_SEND((me->e.X->P, MIN(me->e.X->PLength, 255)))

			(*(me->adapter->d.request))(&me->e);
		} else if (me->multiplex.master.type == DivaUmIdiMasterTypeLicense &&
							 me->multiplex.master.license_state == DivaUmIdiLicenseIdle) {
			send_init_license(me);
		}
	}

	return (0);
}

static int process_idi_request (divas_um_idi_entity_t* e,
                                diva_um_idi_req_hdr_t* req) {
	int assign = 0;
	byte Req = (byte)req->Req;
	dword type = req->type & DIVA_UM_IDI_REQ_TYPE_MASK;

	if (e->entity_type == DivaUmidiEntityTypeLogical) {
		return (process_logical_idi_request (e, req));
	}

	if (e->adapter != 0 && e->adapter->d.request == diva_um_idi_dummy_request) {
		return (-1);
	}

	if (!e->e.Id || !e->e.callback) { /* not assigned */
		if (Req != ASSIGN) {
			DBG_ERR(("A: A(%d) E(%08x) not assigned", e->adapter->adapter_nr, e))
			return (-1); /* NOT ASSIGNED */
		} else {
			switch (type) {
				case DIVA_UM_IDI_REQ_TYPE_MAN: {
					byte* data = (byte*)&req[1];
					byte  options = 0;
					e->application_features = 0;

					if (data[0] == LLI && data[1] != 0) {
						if (e->entity_type == DivaUmIdiEntityTypeManagement && data[1] > 1) {
							if (data[3] & 0x01) {
								e->application_features |= DIVA_UM_IDI_APPLICATION_FEATURE_COMBI_IND;
							}
						}
						options = data[2] & 0x03;
					}
					e->e.Id = MAN_ID;
					e->cbuffer_length = 0;
					e->cbuffer_pos    = 0;
					data[0] = LLI;
					data[1] = 8;
					data[2] = options | 0x04;
					data[3] = 0;
					data[4] = 0;
					data[5] = 0;
					data[6] = 0;
					data[7] = 0;
					data[8] = (byte)sizeof(e->cbuffer);
					data[9] = (byte)(sizeof(e->cbuffer) >> 8);
					data[10] = 0;

					req->data_length = 11;

					DBG_TRC(("A(%d) E(%08x) assign MAN chained:%s unhide:%s combined:on",
										e->adapter->adapter_nr, e,
										options & 0x01 ? "on" : "off",
										options & 0x02 ? "on" : "off"))
				} break;

				case DIVA_UM_IDI_REQ_TYPE_SIG:
					e->e.Id = DSIG_ID;
					DBG_TRC(("A(%d) E(%08x) assign SIG", e->adapter->adapter_nr, e))
					break;

				case DIVA_UM_IDI_REQ_TYPE_NET:
					e->e.Id = NL_ID;
					DBG_TRC(("A(%d) E(%08x) assign NET", e->adapter->adapter_nr, e))
					break;

				default:
					DBG_ERR (("A: A(%d) E(%08x) unknown type=%08x", \
										e->adapter->adapter_nr, e, type))
					return (-1);
			}
		}
		e->e.XNum = 1;
		e->e.RNum = 1;

		if (diva_user_mode_is_proxy_adapter (e->adapter->d.request) >= 0) {
			e->e.callback = diva_um_idi_xdi_callback;
		} else {
			e->e.callback = diva_um_idi_xdi_os_callback;
		}

		e->e.X = &e->XData;
		e->e.R = &e->RData;
		assign = 1;
	}
	e->status			 |= DIVA_UM_IDI_RC_PENDING;
	e->e.Req				= Req;
	e->e.ReqCh			= (byte)req->ReqCh;
	e->e.X->PLength = (word)req->data_length;
	e->e.X->P			  = (byte*)&req[1]; /* Our buffer is safe */

	DBG_TRC(("A(%d) E(%08x) request(%02x-%02x-%02x (%d))", \
					e->adapter->adapter_nr, e, \
					e->e.Id, e->e.Req, e->e.ReqCh, e->e.X->PLength))

	e->rc_count++;

  if (e->adapter && e->adapter->d.request) {
    diva_um_idi_start_wdog (e);
		if (assign == 0 && e->e.X != 0 && e->e.X->PLength != 0) {
			DBG_SEND((e->e.X->P, MIN(e->e.X->PLength, 255)))
		}

    (*(e->adapter->d.request))(&e->e);
  }

	if (assign) {
		if (e->e.Rc == OUT_OF_RESOURCES) {
			/*
				XDI has no entities more, call was not forwarded to the card,
				no callback will be scheduled
				*/
			DBG_ERR(("A: A(%d) E(%08x) XDI out of entities",
							e->adapter->adapter_nr, e))

			e->e.Id		 = 0;
			e->e.ReqCh = 0;
			e->e.RcCh	 = 0;
			e->e.Ind	 = 0;
			e->e.IndCh = 0;
			e->e.XNum	 = 0;
			e->e.RNum	 = 0;
			e->e.callback = 0;
			e->e.X		 = 0;
			e->e.R		 = 0;
			write_return_code (e, ASSIGN_RC | OUT_OF_RESOURCES);
			return (-2);
		} else {
			e->status |= DIVA_UM_IDI_ASSIGN_PENDING;
		}
	}

	return (0);
}

static int process_idi_rc (divas_um_idi_entity_t* e, byte rc) {
	DBG_TRC(("A(%d) E(%08x) rc(%02x-%02x-%02x)",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
					e, e->e.Id, rc, e->e.RcCh))

	if (e->status & DIVA_UM_IDI_ASSIGN_PENDING) {
		e->status &= ~DIVA_UM_IDI_ASSIGN_PENDING;
		if (rc != ASSIGN_OK) {
			DBG_ERR(("A: A(%d) E(%08x) ASSIGN failed",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
					e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr, e))
			e->e.callback = 0;
			e->e.Id = 0;
			e->e.Req = 0;
			e->e.ReqCh = 0;
			e->e.Rc = 0;
			e->e.RcCh = 0;
			e->e.Ind = 0;
			e->e.IndCh = 0;
			e->e.X		 = 0;
			e->e.R		 = 0;
			e->e.XNum	 = 0;
			e->e.RNum	 = 0;
		}
	}
	if ((e->e.Req == REMOVE) && e->e.Id && (rc == 0xff)) {
		DBG_ERR(("A: A(%d) E(%08x)  discard OK in REMOVE",  \
					e->entity_type == DivaUmIdiEntityTypeManagement ?
					e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr, e))
		return (0); /* let us do it in the driver */
	}
	if ((e->e.Req == REMOVE) && (!e->e.Id)) { /* REMOVE COMPLETE */
		e->e.callback = 0;
		e->e.Id = 0;
		e->e.Req = 0;
		e->e.ReqCh = 0;
		e->e.Rc = 0;
		e->e.RcCh = 0;
		e->e.Ind = 0;
		e->e.IndCh = 0;
		e->e.X		 = 0;
		e->e.R		 = 0;
		e->e.XNum	 = 0;
		e->e.RNum	 = 0;
		e->rc_count = 0;
	}
	if ((e->e.Req == REMOVE) && (rc != 0xff)) { /* REMOVE FAILED */
		DBG_ERR(("A: A(%d) E(%08x)  REMOVE FAILED",  \
					e->entity_type == DivaUmIdiEntityTypeManagement ?
					e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr, e))
	}
	write_return_code (e, rc);

	return (1);
}

static int read_combi_ind (divas_um_idi_entity_t* e) {
	word  this_ind_length;
	diva_um_idi_ind_hdr_t* pind;
	int ret = 0;

	if ((e->application_features & DIVA_UM_IDI_APPLICATION_FEATURE_COMBI_IND) != 0) {
		if (e->cbuffer_length > 3) {
			if ((pind = (diva_um_idi_ind_hdr_t*)diva_data_q_get_segment4write (&e->data,
																									e->cbuffer_length+sizeof(*pind))) == 0) {
				return (1);
			}
			pind->type					= DIVA_UM_IDI_IND;
			pind->hdr.ind.Ind		= MAN_COMBI_IND;
			pind->hdr.ind.IndCh = 0;
			pind->data_length   = e->cbuffer_length;
			memcpy (&pind[1], &e->cbuffer[0], e->cbuffer_length);
			diva_data_q_ack_segment4write	(pind);
			e->cbuffer_length = 0;
		}

		if (e->cbuffer_length <= 3) {
			e->cbuffer_length = 0;
			e->cbuffer_pos    = 0;
		}

		return (diva_data_q_get_segment4read(&e->data) != 0);
	}

	while (e->cbuffer_length > 3 && e->cbuffer[e->cbuffer_pos] != 0) {
		/*
			Byte 0   - Ind
			Byte 1,2 - data length
			*/
		this_ind_length = (word)e->cbuffer[e->cbuffer_pos+1] | ((word)e->cbuffer[e->cbuffer_pos+2] << 8);
    if (this_ind_length >= diva_data_q_get_max_length(&e->data) ||
        this_ind_length + 4 > e->cbuffer_length) {
      /*
        Indication too long, also discard entire combined indication
        */
      int i, to_print, org_length = e->cbuffer_length + e->cbuffer_pos - 1;

      DBG_FTL(("IOCTL: partial ind too long Ind:%02x length=%d, pos:%d, len:%d, total:%d",
							 e->cbuffer[e->cbuffer_pos], this_ind_length, e->cbuffer_pos, e->cbuffer_length, org_length))

      i = 0;
      do {
        to_print = MIN(256, org_length - i);
        DBG_BLK((&e->cbuffer[i], to_print))
        i += to_print;
      } while (i < org_length);

      e->cbuffer_length = 0;
      e->cbuffer_pos    = 0;
      break;
    }
		if ((pind = (diva_um_idi_ind_hdr_t*)diva_data_q_get_segment4write (&e->data,
                                                          this_ind_length+sizeof(*pind))) == 0) {
			return (1);
		}
		pind->type					= DIVA_UM_IDI_IND;
		pind->hdr.ind.Ind		= e->cbuffer[e->cbuffer_pos++];

		e->cbuffer_pos += 2;
		pind->hdr.ind.IndCh = 0;
		pind->data_length   = this_ind_length;
		memcpy (&pind[1], &e->cbuffer[e->cbuffer_pos], this_ind_length);
		diva_data_q_ack_segment4write	(pind);
		e->cbuffer_pos += (this_ind_length+1);
		e->cbuffer_length -= (4 + this_ind_length);
		ret = 1;
	}

	if (e->cbuffer_length <= 3 || e->cbuffer[e->cbuffer_pos] == 0) {
		e->cbuffer_length = 0;
		e->cbuffer_pos		= 0;
	}

	return (ret);
}


static int process_idi_ind (divas_um_idi_entity_t* e, byte ind) {
	int do_wakeup = 0;

	if (e->e.complete != 0x02) {
		if (ind == MAN_COMBI_IND) {
			if (e->cbuffer_length != 0) {
				if (read_combi_ind(e))
					do_wakeup = 1;
			}

			if (e->cbuffer_length == 0) {
				e->e.RNum       = 1;
				e->e.R->P			  = &e->cbuffer[0];
				e->e.R->PLength = (word)sizeof(e->cbuffer);
				e->cbuffer_pos  = 0;

				DBG_TRC(("A(%d) E(%08x) cind_1(%02x-%02x-%02x)-[%d-%d]",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
								e, e->e.Id, ind, e->e.IndCh, e->e.RLength, e->e.R->PLength))

			} else {
				e->e.RNum	= 0;
				e->e.RNR	= 1;
				do_wakeup = 1;

				DBG_TRC(("A(%d) E(%08x) cind(%02x-%02x-%02x)-RNR",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
									e, e->e.Id, ind, e->e.IndCh))
			}
		} else {
			diva_um_idi_ind_hdr_t* pind =
					(diva_um_idi_ind_hdr_t*)diva_data_q_get_segment4write (&e->data, e->e.RLength+sizeof(*pind));
			if (pind) {
				e->e.RNum       = 1;
				e->e.R->P			  = (byte*)&pind[1];
				e->e.R->PLength = e->e.RLength;

				DBG_TRC(("A(%d) E(%08x) ind_1(%02x-%02x-%02x)-[%d-%d]",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
								e, e->e.Id, ind, e->e.IndCh, e->e.RLength, e->e.R->PLength))

			} else {
				DBG_TRC(("A(%d) E(%08x) ind(%02x-%02x-%02x)-RNR",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
									e, e->e.Id, ind, e->e.IndCh))
				e->e.RNum	= 0;
				e->e.RNR	= 1;
				do_wakeup = 1;
			}
		}
	} else {
		DBG_RECV((e->e.R->P, MIN(e->e.R->PLength, 255)))

		if (ind == MAN_COMBI_IND) {
			e->cbuffer_length = e->e.R->PLength;

			if (read_combi_ind (e) != 0)
				do_wakeup = 1;

			DBG_TRC(("A(%d) E(%08x) cind(%02x-%02x-%02x)-[%d]",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
								e, e->e.Id, ind, e->e.IndCh, e->e.R->PLength))

		} else {
			diva_um_idi_ind_hdr_t* pind =\
										(diva_um_idi_ind_hdr_t*)(e->e.R->P);

			DBG_TRC(("A(%d) E(%08x) ind(%02x-%02x-%02x)-[%d]", \
					e->entity_type == DivaUmIdiEntityTypeManagement ?
							e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
								e, e->e.Id, ind, e->e.IndCh, e->e.R->PLength))

			pind--;
			pind->type					= DIVA_UM_IDI_IND;
			pind->hdr.ind.Ind		= ind;
			pind->hdr.ind.IndCh = e->e.IndCh;
			pind->data_length   = e->e.R->PLength;
			diva_data_q_ack_segment4write	(pind);
			do_wakeup = 1;
		}
	}

	if ((e->status & DIVA_UM_IDI_RC_PENDING) && diva_data_q_get_segment4read(&e->rc) == 0) {
		do_wakeup = 0;
	}

	if (e->entity_type == DivaUmidiEntityTypeLogical && e->multiplex.logical.on_active_q != 0) {
		do_wakeup = 0;
	}

	return (do_wakeup);
}

/* --------------------------------------------------------------------------
		Write return code to the return code queue of entity
	 -------------------------------------------------------------------------- */
static int	write_return_code (divas_um_idi_entity_t* e, byte rc) {
	diva_um_idi_ind_hdr_t* prc;

	if (!(prc = (diva_um_idi_ind_hdr_t*)diva_data_q_get_segment4write	(&e->rc, sizeof(*prc)))) {
		DBG_ERR (("A: A(%d) E(%08x) rc(%02x) lost",
					e->entity_type == DivaUmIdiEntityTypeManagement ?
					e->adapter->adapter_nr : e->multiplex.logical.xdi_entity->adapter->adapter_nr,
					e, rc))
		e->status &= ~DIVA_UM_IDI_RC_PENDING;
		return (-1);
	}

	prc->type				 = DIVA_UM_IDI_IND_RC;
	prc->hdr.rc.Rc	 = rc;
	prc->hdr.rc.RcCh = e->e.RcCh;
	prc->data_length = 0;
	diva_data_q_ack_segment4write	(prc);
	
	return (0);
}

static int diva_um_idi_proxy_req_ready (void* entity, void* os_handle) {
	divas_um_idi_proxy_t* pE = (divas_um_idi_proxy_t*)entity;
	diva_os_spin_lock_magic_t old_irql;
	int ret = 0;

	if (pE->entity_function != DivaUmIdiProxy)
		return (-1);

	diva_os_enter_spin_lock (&pE->req_lock, &old_irql, "req_ready");
	if (pE->state == DivaUmIdiProxyRegistered) {
		ret = diva_q_get_head(&pE->req_q) != 0 || pE->nr_pending_requests != 0;
	}
	diva_os_leave_spin_lock (&pE->req_lock, &old_irql, "req_ready");

	return (ret);
}


/* --------------------------------------------------------------------------
		Return amount of entries that can be bead from this entity or
		-1 if adapter was removed
	 -------------------------------------------------------------------------- */
int diva_user_mode_idi_ind_ready (void* entity, void* os_handle) {
	divas_um_idi_entity_t* e;
	diva_um_idi_adapter_t* a;
	diva_os_spin_lock_magic_t old_irql;
	int ret;

	if ((e = (divas_um_idi_entity_t*)entity) == 0)
		return(-1);

	if (e->entity_function != DivaUmIdiEntity) {
		ret = diva_um_idi_proxy_req_ready (entity, os_handle);
		return (ret);
	}

	diva_os_enter_spin_lock (&adapter_lock, &old_irql, "ind_ready");

	if (((e->status & DIVA_UM_IDI_REMOVED) != 0) ||
			(e->entity_type != DivaUmIdiEntityTypeManagement &&
			 e->multiplex.logical.xdi_entity == 0)) {
		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "ind_ready");
		return (-1); /* adapter was removed */
	}

	a = e->entity_type == DivaUmIdiEntityTypeManagement ?
				e->adapter : e->multiplex.logical.xdi_entity->adapter;

	if (a == 0 || (e->status & DIVA_UM_IDI_REMOVED) != 0) {
		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "ind_ready");
		return (-1); /* adapter was removed */
	}

	ret = (diva_data_q_get_segment4read(&e->rc) != 0) + (diva_data_q_get_segment4read(&e->data) != 0);

	if ((e->status & DIVA_UM_IDI_RC_PENDING) && diva_data_q_get_segment4read(&e->rc) == 0) {
		ret = 0;
	}

  diva_os_leave_spin_lock (&adapter_lock, &old_irql, "ind_ready");

	return (ret);
}

void* diva_um_id_get_os_context (void* entity) {
	divas_um_idi_entity_t* e = (divas_um_idi_entity_t*)entity;
	void* ret = 0;

	if (e->entity_function == DivaUmIdiEntity) {
		ret = e->os_context;

	} else if (e->entity_function == DivaUmIdiProxy) {
		divas_um_idi_proxy_t* pE = (divas_um_idi_proxy_t*)entity;

		ret = pE->os_context;
	}

	return (ret);
}

int divas_um_idi_entity_assigned (void* entity) {
	divas_um_idi_entity_t* e;
	diva_um_idi_adapter_t* a;
	int ret;
  diva_os_spin_lock_magic_t old_irql;

	if ((e = (divas_um_idi_entity_t*)entity) == 0)
		return (0);

	if (e->entity_function != DivaUmIdiEntity)
		return (0);

  diva_os_enter_spin_lock (&adapter_lock, &old_irql, "assigned?");

	if (e->entity_type != DivaUmIdiEntityTypeManagement) {
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "assigned?");
		return (0);
	}

  if ((a = e->adapter) == 0 || (e->status & DIVA_UM_IDI_REMOVED) != 0) {
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "assigned?");
    return (0);
  }

	e->status |= DIVA_UM_IDI_REMOVE_PENDING;

  ret = (e->e.Id || e->rc_count || (e->status & DIVA_UM_IDI_ASSIGN_PENDING));

	DBG_TRC(("Id:%02x, rc_count:%d, status:%08x", e->e.Id, e->rc_count, e->status))

  diva_os_leave_spin_lock (&adapter_lock, &old_irql, "assigned?");

  return (ret);
}

int divas_um_idi_entity_start_remove (void* entity) {
	divas_um_idi_entity_t* e;
	diva_um_idi_adapter_t* a;
  diva_os_spin_lock_magic_t old_irql;

	if ((e = (divas_um_idi_entity_t*)entity) == 0 || e->entity_type != DivaUmIdiEntityTypeManagement)
		return (0);

  diva_os_enter_spin_lock (&adapter_lock, &old_irql, "start_remove");

	if ((a = e->adapter) == 0 || (e->status & DIVA_UM_IDI_REMOVED) != 0) {
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "start_remove");
    return (0);
  }

  if (e->rc_count) { /* Entity BUSY */
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "start_remove");
    return (1);
  }

  if (!e->e.Id) { /* Remove request was already pending, and arrived now */
    diva_os_leave_spin_lock (&adapter_lock, &old_irql, "start_remove");
    return (0); /* REMOVE was pending */
  }

  /*
    Now send remove request
    */
  e->e.Req	 = REMOVE;
  e->e.ReqCh = 0;

  e->rc_count++;

  DBG_TRC(("A(%d) E(%08x) request(%02x-%02x-%02x (%d))", \
           e->adapter->adapter_nr, e, \
           e->e.Id, e->e.Req, e->e.ReqCh, e->e.X->PLength))

  if (a->d.request)
    (*(a->d.request))(&e->e);

  diva_os_leave_spin_lock (&adapter_lock, &old_irql, "start_remove");

	return (0);
}

void divas_um_notify_timeout (void* p_os) {
	if (p_os != 0) {
	  diva_os_spin_lock_magic_t old_irql;
		divas_um_idi_entity_t* e;
		diva_um_idi_adapter_t* a;
		int found = 0;

		diva_os_enter_spin_lock (&adapter_lock, &old_irql, "timeout");

		for (a = (diva_um_idi_adapter_t*)diva_q_get_head (&adapter_q);
				 a != 0 && found == 0;
				 a = (diva_um_idi_adapter_t*)diva_q_get_next (&a->link)) {
			for (e = (divas_um_idi_entity_t*)diva_q_get_head(&a->entity_q);
					 e != 0 && found == 0;
					 e = (divas_um_idi_entity_t*)diva_q_get_next(&e->link)) {
				if (e->entity_function == DivaUmIdiEntity && e->os_context == p_os) {
					e->status |= DIVA_UM_IDI_TIMEOUT;
					found = 1;
				} else if (e->entity_function == DivaUmIdiEntity &&
									 e->entity_type == DivaUmIdiEntityTypeMaster &&
									 e->multiplex.master.type == DivaUmIdiMasterTypeIdi) {
					divas_um_idi_entity_t* le;
					diva_entity_queue_t* q[2];
					dword i;
					q[0] = &e->multiplex.master.active_multiplex_q;
					q[1] = &e->multiplex.master.idle_multiplex_q;

					for (i = 0; i < sizeof(q)/sizeof(q[0]) && found == 0; i++) {
						for (le = (divas_um_idi_entity_t*)diva_q_get_head (q[i]);
								 le != 0 && found == 0;
								 le = (divas_um_idi_entity_t*)diva_q_get_next (&le->link)) {
							if (le->os_context == p_os) {
								e->status |= DIVA_UM_IDI_TIMEOUT;
								found = 1;
								a->nr_delayed_cleanup_entities++;
							}
						}
					}
				}
			}
		}
		diva_os_leave_spin_lock (&adapter_lock, &old_irql, "timeout");
	}
}


