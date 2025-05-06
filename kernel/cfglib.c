
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
#include "dlist.h"
#include "cfg_types.h"
#include "cfglib.h"
#include "cfg_notify.h"
#include "di_defs.h"
#include "pc.h"
#include "divasync.h"
#include "parser.h"
#include "drv_man.h"

#ifndef CARDTYPE_H_WANT_DATA
#define CARDTYPE_H_WANT_DATA 0
#endif

#ifndef CARDTYPE_H_WANT_IDI_DATA
#define CARDTYPE_H_WANT_IDI_DATA 0
#endif

#ifndef CARDTYPE_H_WANT_RESOURCE_DATA
#define CARDTYPE_H_WANT_RESOURCE_DATA 0
#endif

#ifndef CARDTYPE_H_WANT_FILE_DATA
#define CARDTYPE_H_WANT_FILE_DATA 0
#endif

#include "cardtype.h"

#define DIVA_CFG_LIB_MAX_INITIAL_RESPONSE_LENGTH (16*1024)
#define DIVA_CFG_LIB_RESPONSE_LENGTH_INCREMENT   (4*1024)

#define DIVA_CFG_TIMER_RESOLUTION 20 /* mSec */
#define DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT (500/DIVA_CFG_TIMER_RESOLUTION)

#define DIVA_CFG_READONLY_MANAGEMENT_INFO "/readonly"


typedef struct _diva_cfg_lib_management_ifc_state {
	volatile int status;
	volatile int message_nr;
	volatile IDI_CALL request;
  void (*internal_request)(void* context, ENTITY* e);
  void* internal_request_context;
	int	xdi_adapter;
	const byte* instance;
	const byte* variable; /* Set if only one specific variable is to be updated */
	int free_instance;
	const byte* var;
	int state;
	int	rc_pending;
	dword script_offset;
	int error;
	const byte* write_var_name;
	byte        write_var_name_length;
	ENTITY e;
  BUFFERS XData;
  BUFFERS RData;
	byte xdata[270];
	byte rdata[271];

	const byte* enum_prefix;
	byte				enum_prefix_length;
	const byte* enum_name;
	dword				enum_data;
	dword				enum_nr;
	int         final_enum_nr;

	byte        var_name_buffer[270];
} diva_cfg_lib_management_ifc_state_t;

typedef struct _diva_cfg_lib_notify_client {
	diva_entity_link_t                      link;
	diva_cfg_lib_cfg_notify_callback_proc_t proc;
	void*                                   context;
  dword                                   owner;
	diva_entity_queue_t											pending_data_q;
} diva_cfg_lib_notify_client_t;

typedef struct _diva_cfg_pending_notify_header {
	diva_entity_link_t link;
	dword              response_location;
	dword              data_length;
} diva_cfg_pending_notify_header_t;

/*
	LOCALS
	*/
static int diva_cfg_lib_notify_pending (void);
static byte diva_cfg_create_read_req (diva_cfg_lib_management_ifc_state_t* pE);
const byte* diva_cfg_find_next_var_with_management_info (const byte* instance,
																												 const byte* var);
static int diva_cfg_create_write_req (diva_cfg_lib_management_ifc_state_t* pE);
static IDI_CALL diva_cfg_get_request_function (diva_section_target_t target,
																							 int* adapter_type,
																							 int instance_by_name,
																							 const byte* instance_ident,
																							 dword instance_ident_length,
																							 dword instance_nr);
static const byte* diva_cfg_get_variable_data (const byte* value, dword* data_length);
static int diva_cfg_lib_check_user_mode_adapter (IDI_CALL request);
static void diva_cfg_lib_apply_named_variables (diva_cfg_lib_management_ifc_state_t* state);
static diva_cfg_lib_instance_t* cfg_lib;

/*
	Create and initialize CFGLib instance
	*/
int diva_cfg_lib_init (void) {
	if (!(cfg_lib = diva_os_malloc (0, sizeof(*cfg_lib)))) {
		DBG_FTL(("CfgLib: Failed to allocate %d bytes for CFGLib instance", sizeof(*cfg_lib)))
		return (-1);
	}
	memset (cfg_lib, 0x00, sizeof(*cfg_lib));

	diva_q_init (&cfg_lib->notify_q);

	if (diva_os_initialize_spin_lock (&cfg_lib->notify_lock, "init")) {
		diva_os_free (0, cfg_lib);
		cfg_lib = 0;
		DBG_FTL(("CfgLib: Failed to initialize notify lock"))
		return (-1);
	}

	return (0);
}

/*
	Process configuration change request
	INPUT:
		Configuration file
	RETURN:
		zero on success
		negative error value in case of error
	*/
int diva_cfg_lib_write (const char* data) {
	diva_os_spin_lock_magic_t old_irql;
	char* response_buffer;
	int ret = 0;
	int free_response_buffer = 0;

	if (!cfg_lib) {
		DBG_FTL(("CfgLib: Write failed, library not initialized"))
		return (-1);
	}

	if (diva_cfg_lib_notify_pending()) {
		DBG_FTL(("CfgLib: Notification response pending"))
		return (-1);
	}

	if (!(response_buffer = diva_os_malloc (0, DIVA_CFG_LIB_MAX_INITIAL_RESPONSE_LENGTH))) {
		DBG_FTL(("CfgLib: Failed to allocate response buffer"))
		return (-1);
	}

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");
	if (cfg_lib->return_data) {
		ret = -1;
	} else {
		cfg_lib->return_data = response_buffer;
		cfg_lib->return_data[0]         = 0;
		cfg_lib->return_data_length     = 1;
		cfg_lib->max_return_data_length = DIVA_CFG_LIB_MAX_INITIAL_RESPONSE_LENGTH;
	}
	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

	if (ret < 0) {
		diva_os_free (0, response_buffer);
		DBG_FTL(("CfgLib: Configuration update failed: response pending"))
		return (-1);
	}

	cfg_lib->src = data;
	ret = diva_cfg_lib_read_input (cfg_lib);
	cfg_lib->src = 0;

	if (cfg_lib->data) {
		if (!cfg_lib->storage) {
			/*
				Initial configuration update
				*/
			diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "initial write");
			cfg_lib->storage         = cfg_lib->data;
			diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "initial write");

			free_response_buffer     = 1;
		} else {
			/*
				Configuration update
				*/
			if (!(ret = diva_cfg_compare (cfg_lib, cfg_lib->storage, cfg_lib->data))) {
				void* org_mem;

				diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "update");
				org_mem          = (void*)cfg_lib->storage;
				cfg_lib->storage = cfg_lib->data;
				diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "update");

				if (org_mem) {
					diva_os_free (0, (byte*)org_mem);
				}
			}
		}
	}

	cfg_lib->data            = 0;
	cfg_lib->data_length     = 0;
	cfg_lib->max_data_length = 0;

	if (ret || free_response_buffer) {
		diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		response_buffer = cfg_lib->return_data;
		cfg_lib->return_data = 0;

		diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		diva_os_free (0, response_buffer);
	} else {
		if (cfg_lib->return_data && !diva_cfg_lib_notify_pending()) {
      diva_cfg_lib_call_notify_callback (TargetCFGLibDrv, 0, 0, 0);
    }
	}

	return (ret);
}

/*
	Read configuration change acknowledge
	Return:
		amount of bytes written to user buffer on success
		negative error value in case of error
	*/
int diva_cfg_lib_read (char** data, int max_length) {
	diva_os_spin_lock_magic_t old_irql;
	int length;

	if (!cfg_lib->return_data) {
		DBG_FTL(("CfgLib: Failed to read response: buffer empty"))
		return (-1);
	}
	if (cfg_lib->return_data_length > (dword)max_length) {
		DBG_ERR(("CfgLib: Failed to read response: buffer empty too small"))
		return (-2);
	}

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

	*data = (char*)cfg_lib->return_data;
	length = (int)cfg_lib->return_data_length;
	cfg_lib->return_data = 0;
	cfg_lib->return_data_length = 0;

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

	return (length);
}

int diva_cfg_get_read_buffer_length (void) {
  return ((int)cfg_lib->return_data_length);
}

/*
	Shut down CFGLib instance and free CFGLib memory
	*/
void diva_cfg_lib_finit (void) {
	if (cfg_lib) {
		if (cfg_lib->return_data) {
			diva_os_free (0, cfg_lib->return_data);
		}
		if (cfg_lib->data) {
			diva_os_free (0, cfg_lib->data);
		}
		if (cfg_lib->storage) {
			diva_os_free (0, (void*)cfg_lib->storage);
		}

		diva_os_destroy_spin_lock (&cfg_lib->notify_lock, 0);

		diva_os_free (0, cfg_lib);
		cfg_lib = 0;
	}
}

static int diva_cfg_find_notify (const void* what, const diva_entity_link_t* p) {
	diva_cfg_lib_notify_client_t* handle = (diva_cfg_lib_notify_client_t*)p;
	dword* id = (dword*)what;

	return (!(handle->owner == *id));
}

void* diva_cfg_lib_cfg_register_notify_callback (diva_cfg_lib_cfg_notify_callback_proc_t proc,
																								 void* context,
																								 dword owner) {
	diva_cfg_lib_notify_client_t* handle = diva_os_malloc (0, sizeof(*handle));
	const byte* powner;
	int add = 1;

	if (handle != 0) {
    diva_os_spin_lock_magic_t old_irql;

		memset (handle, 0x00, sizeof(*handle));
		handle->proc    = proc;
		handle->context = context;
		handle->owner   = owner;
		diva_q_init (&handle->pending_data_q);

		diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		if (diva_q_find (&cfg_lib->notify_q, &owner, diva_cfg_find_notify)) {
			add = 0;
		} else {
			diva_q_add_tail (&cfg_lib->notify_q, &handle->link);
		}

		diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		if (!add) {
			diva_os_free (0, handle);
			DBG_ERR(("CfgLib: Configuration owner for %lu already registered", owner))
			handle = 0;
		} else {
			DBG_LOG(("CfgLib: Registered notification for owner %lu", owner))

			if ((powner = diva_cfg_parser_get_owner_configuretion (cfg_lib, owner))) {
				/*
					Provide initial configuration to application
					Return value is ignored at this location, supposed that problem is
					handled by application self.
					*/
				const byte* section = 0;

				while ((section = diva_cfg_get_next_instance (powner, section))) {
					diva_cfg_lib_call_notify_callback (owner, section, get_max_length(section), 0);
				}
			}
		}
	}

	return (handle);
}

static int diva_cfg_find_notify_handle (const void* what, const diva_entity_link_t* p) {
	diva_cfg_lib_notify_client_t* user_handle = (diva_cfg_lib_notify_client_t*)what;
	diva_cfg_lib_notify_client_t* handle = (diva_cfg_lib_notify_client_t*)p;

	return (!(handle == user_handle));
}

/*
	Move all messaged from src queue to dst queue
	*/
static void move_messages (diva_entity_queue_t* src, diva_entity_queue_t* dst) {
	diva_entity_link_t* msg;

	while ((msg = diva_q_get_head (src))) {
		diva_q_remove   (src, msg);
		diva_q_add_tail (dst, msg);
	}
}

int diva_cfg_lib_cfg_remove_notify_callback   (void* handle) {
	int ret = -1, response_pending = diva_cfg_lib_notify_pending ();
	dword owner = 0;

	if (handle) {
		diva_cfg_lib_notify_client_t* user_handle = (diva_cfg_lib_notify_client_t*)handle;
    diva_os_spin_lock_magic_t old_irql;
		diva_entity_queue_t free_mem_q;

		diva_q_init (&free_mem_q);

		diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		if (diva_q_find (&cfg_lib->notify_q, handle, diva_cfg_find_notify_handle)) {
			owner = user_handle->owner;
			diva_q_remove   (&cfg_lib->notify_q, &user_handle->link);
			diva_q_add_tail (&free_mem_q,        &user_handle->link);
			move_messages (&user_handle->pending_data_q, &free_mem_q);

      if (user_handle->owner == (dword)TargetCFGLibDrv) {
				if (cfg_lib->return_data) {
					diva_q_add_tail (&free_mem_q, (diva_entity_link_t*)cfg_lib->return_data);
					cfg_lib->return_data = 0;
					cfg_lib->return_data_length = 0;
				}
				/*
					Cancel configuration response for all applications.
					*/
				{
					diva_cfg_lib_notify_client_t* appl_handle =
								(diva_cfg_lib_notify_client_t*)diva_q_get_head (&cfg_lib->notify_q);
					diva_cfg_pending_notify_header_t* hdr;

					while (appl_handle) {
						hdr = (diva_cfg_pending_notify_header_t*)diva_q_get_head (&appl_handle->pending_data_q);
						while (hdr) {
							hdr->response_location = 0;
							hdr = (diva_cfg_pending_notify_header_t*)diva_q_get_next (&hdr->link);
						}
						appl_handle = (diva_cfg_lib_notify_client_t*)diva_q_get_next(&appl_handle->link);
					}
				}
      }
		}

		diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

		/*
			Free memory
			*/
		{
			diva_entity_link_t* msg;

			while ((msg = diva_q_get_head (&free_mem_q))) {
				diva_q_remove (&free_mem_q, msg);
				diva_os_free  (0, msg);
			}
		}
		if (user_handle) {
			if ((owner != (dword)TargetCFGLibDrv) && cfg_lib->return_data &&
					response_pending && !diva_cfg_lib_notify_pending()) {
				diva_cfg_lib_call_notify_callback (TargetCFGLibDrv, 0, 0, 0);
			}
			DBG_LOG(("CfgLib: Removed notification callback for owner %lu", owner))
			ret = 0;
		} else {
			DBG_FTL(("CfgLib: Failed to remove notification callback"))
		}
	}

	return (ret);
}

/*
	Notifycation function and inform user about changes.
	This function is called only in case hot update procedure failed
	*/
diva_hot_update_operation_result_t diva_cfg_lib_call_notify_callback (dword owner,
																																			const byte* instance,
																																			dword instance_data_length,
																																			dword response_position) {
	volatile diva_cfg_lib_cfg_notify_callback_proc_t proc = 0;
	diva_cfg_lib_notify_client_t* handle = 0;
	diva_hot_update_operation_result_t ret;
	diva_os_spin_lock_magic_t old_irql;
	volatile void* context = 0;
	diva_cfg_pending_notify_header_t* hdr;
	int	call_notify = 1;

	if (instance && (owner == (dword)TargetCFGLibDrv)) {
		return (HotUpdateOK);
	}

	if (owner == (dword)TargetXdiAdapter || owner == (dword)TargetSoftIPAdapter) {
		/*
			Translate data before pass notification to XDI adapter
			*/
		if (!(hdr = (diva_cfg_pending_notify_header_t*)diva_cfg_translate_xdi_data (sizeof(*hdr),
																																								instance,
																																								&instance_data_length))) {
			return (HotUpdateFailedNotPossible);
		} else {
			DBG_TRC(("CfgLib: Adapter notify(instance_data_length):"))
			DBG_BLK(((void*)&hdr[1], instance_data_length-sizeof(hdr[0])))
  		hdr->data_length = instance_data_length - sizeof(hdr[0]);
		}
	} else if (!(hdr = (diva_cfg_pending_notify_header_t*)diva_os_malloc (0,
																										 sizeof(*hdr)+instance_data_length))) {
		return (HotUpdateFailedNotPossible);
	} else {
  	hdr->data_length = instance_data_length;
		memcpy (&hdr[1], instance, instance_data_length);
	}

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

	if ((handle = (diva_cfg_lib_notify_client_t*)diva_q_find (&cfg_lib->notify_q,
																													 &owner,
																													 diva_cfg_find_notify))) {
		proc    = handle->proc;
		context = handle->context;

		call_notify = (diva_q_get_head	(&handle->pending_data_q) == 0);
		diva_q_add_tail (&handle->pending_data_q, &hdr->link);
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "add notify");

	if (handle) {
		if (call_notify) {
			int remove_notification = 0;

			DBG_REG(("CfgLib: Call notify cback for owner %x", owner))
			switch ((*proc)((void*)context, 0, (byte*)&hdr[1])) {
				case 0:
					remove_notification = 1;
					ret = HotUpdateOK;
					break;

				case 1:
					/*
						Acknowledge delayed
						*/
					ret = HotUpdatePending;
					hdr->response_location = response_position;
					break;

				default:
					remove_notification = 1;
					ret = HotUpdateFailedNotPossible;
					break;
			}

			if (remove_notification) {
				diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "del notify data");
				diva_q_remove (&handle->pending_data_q, &hdr->link);
				diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "del notify data");
				diva_os_free (0, hdr);
			}
		} else {
			/*
				Acknowledge delayed
				*/
			ret = HotUpdateOK;
			hdr->response_location = response_position;
			DBG_LOG(("CfgLib: Notify queue for %x not empty, response position=%d", owner, response_position))
		}
	} else {
		diva_os_free (0, hdr);
		ret = HotUpdateFailedOwnerNotFound;
	}

	return (ret);
}

/*
	Call notification function which provides partial configuration update notification
	*/
diva_hot_update_operation_result_t diva_cfg_lib_call_variable_notify_callback (dword owner,
																																							 int instance_by_name,
																																							 const byte* instance_ident,
																																							 dword instance_ident_length,
																																							 dword instance_nr,
																																							 const byte* variable,
																																							 int changed) {
	diva_hot_update_operation_result_t ret = HotUpdateFailedOwnerNotFound;
	volatile diva_cfg_lib_cfg_notify_callback_proc_t proc = 0;
	diva_cfg_lib_notify_client_t* handle;
	diva_os_spin_lock_magic_t old_irql;
	volatile void* context = 0;
	int data_queue_empty = 0;

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "var change");

	if ((handle = (diva_cfg_lib_notify_client_t*)diva_q_find (&cfg_lib->notify_q,
																													 &owner,
																													 diva_cfg_find_notify))) {
		proc    = handle->proc;
		context = handle->context;

		data_queue_empty = (diva_q_get_head	(&handle->pending_data_q) == 0);
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "var change");

	if (proc) {
		if (data_queue_empty != 0) {
			diva_cfg_lib_notification_context_t nfy;

			nfy.notification_type = changed ? DivaCfgVariableChanged : DivaCfgVariableRemoved;
			nfy.info.variable_changed.instance_by_name      = instance_by_name;
			nfy.info.variable_changed.instance_ident        = instance_ident;
			nfy.info.variable_changed.instance_ident_length = instance_ident_length;
			nfy.info.variable_changed.instance_nr           = instance_nr;
			nfy.info.variable_changed.variable              = variable;

			switch ((*proc)((void*)context, (void*)&nfy, 0)) {
				case 0:
					ret = HotUpdateOK;
					break;
				case 1:
					DBG_ERR(("CfgLib: OS abstraction layer error at %d", __LINE__))
					ret = HotUpdateFailedNotPossible;
					break;
				default:
					ret = HotUpdateFailedNotPossible;
					break;
			}
		} else {
			ret = HotUpdateOK;
			DBG_LOG(("CfgLib: Variable change: notify queue for %x not empty", owner))
		}
	}

	return (ret);
}


/*
	Returns true if notification for one of
	applications still pending
	*/
static int diva_cfg_lib_notify_pending (void) {
	diva_cfg_lib_notify_client_t* handle;
	diva_os_spin_lock_magic_t old_irql;
	int ret = 0;

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "check pending data");

	for (handle = (diva_cfg_lib_notify_client_t*)diva_q_get_head (&cfg_lib->notify_q);
			 ((handle != 0) && (ret == 0));
			 handle = (diva_cfg_lib_notify_client_t*)diva_q_get_next (&handle->link)) {
		ret = (diva_q_get_head	(&handle->pending_data_q) != 0);
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "check pending data");

	return (ret);
}

int add_string_to_response_buffer (const char* p) {
	dword length = strlen(p) + 1;

	if ((length + cfg_lib->return_data_length) >= cfg_lib->max_return_data_length) {
		char* response_buffer;
		cfg_lib->max_return_data_length += DIVA_CFG_LIB_RESPONSE_LENGTH_INCREMENT;

		if (!(response_buffer = diva_os_malloc (0, cfg_lib->max_return_data_length))) {
			DBG_FTL(("CfgLib: Failed to realloc response buffer"))
			return (-1);
		}
		memcpy (response_buffer, cfg_lib->return_data, cfg_lib->return_data_length);
		diva_os_free (0, cfg_lib->return_data);
		cfg_lib->return_data = response_buffer;
	}


	memcpy (&cfg_lib->return_data[cfg_lib->return_data_length-1], p, (int)length);
	cfg_lib->return_data_length += (length-1);

	return (0);
}

dword get_response_buffer_position (void) {
	return ((cfg_lib->return_data && cfg_lib->return_data_length) ?
								(cfg_lib->return_data_length-1) : 0);
}

int diva_cfg_lib_response_open_owner  (dword owner) {
	char buffer[64];

	diva_snprintf (buffer, sizeof(buffer), "<tlie>\n\t<vie id=\"owner\">%lx</vie>\n", owner);

	return (add_string_to_response_buffer (&buffer[0]));
}

int diva_cfg_lib_response_close_owner (void) {
	return (add_string_to_response_buffer ("</tlie>\n\n"));
}

int diva_cfg_lib_response_open_instance (dword instance) {
	char buffer[64];
	diva_snprintf (buffer, sizeof(buffer), "\t<tlie>\n\t\t<vie id=\"instance\">%lx</vie>\n", instance);

	return (add_string_to_response_buffer (&buffer[0]));
}

int diva_cfg_lib_response_open_instance_by_name (const byte* name, dword name_length) {
	int max_length = (int)(4*name_length + 64);
	char* buffer = diva_os_malloc (0, max_length);
	int length = 0, i, is_ascii = 1;

	if (!buffer) {
		DBG_FTL(("CfgLib: Failed to alloc response buffer"));
		return (-1);
	}

	length += diva_snprintf (&buffer[length], max_length - length, "%s", "\t<tlie>\n\t\t<vie id=\"instance2\">");

	for (i = 0; ((i < (int)name_length) && is_ascii); i++) {
		is_ascii = ((name[i] >= 0x20) && (name[i] <= 0x7e) && name[i] != '<' && name[i] != '>');
	}

	length += diva_snprintf (&buffer[length], max_length - length, "%s", is_ascii ? "<as>" : "<bs>");

	for (i = 0; i < (int)name_length; i++) {
		if (is_ascii) {
			length += diva_snprintf (&buffer[length], max_length - length, "%c", (char)name[i]);
		} else {
			length += diva_snprintf (&buffer[length], max_length - length, "%s%02x", i ? "," : "", name[i]);
		}
	}

	length += diva_snprintf (&buffer[length], max_length - length, "%s", is_ascii ? "</as>" : "</bs>");
	length += diva_snprintf (&buffer[length], max_length - length, "%s", "</vie>\n");

	i = add_string_to_response_buffer (buffer);

	diva_os_free (0, buffer);

	return (i);
}

int diva_cfg_lib_response_close_instance (void) {
	return (add_string_to_response_buffer ("\t</tlie>\n"));
}

int diva_cfg_lib_open_response (void) {
	return (add_string_to_response_buffer ("<tlie>\n\n"));

}

int diva_cfg_lib_close_response (void) {
	return (add_string_to_response_buffer ("\n\n</tlie>\n\n"));
}

const byte* diva_cfg_lib_get_storage (void) {
	return (cfg_lib ? cfg_lib->storage : 0);
}

/*
	Acknowledge pending notification
	*/
void diva_cfg_lib_ack_owner_data (dword owner, dword response) {
	diva_cfg_pending_notify_header_t* hdr = 0;
	diva_cfg_lib_notify_client_t* handle;
	diva_os_spin_lock_magic_t old_irql;

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "ack data");

	if ((handle = (diva_cfg_lib_notify_client_t*)diva_q_find (&cfg_lib->notify_q,
																														&owner,
																														diva_cfg_find_notify))) {
		if ((hdr = (diva_cfg_pending_notify_header_t*)diva_q_get_head (&handle->pending_data_q))) {
			diva_q_remove (&handle->pending_data_q, &hdr->link);
			if (hdr->response_location != 0) {
				char* response_location = &cfg_lib->return_data[hdr->response_location];

				if (cfg_lib->return_data && cfg_lib->return_data_length &&
						(response_location > cfg_lib->return_data) &&
						(response_location < &cfg_lib->return_data[cfg_lib->return_data_length]) &&
						(response_location[-1] == '>')) {
					response_location[0] = (char)((response >= (dword)HotUpdateLastApplicationResponse) ?
														(dword)HotUpdateInvalidApplicationResponse : response) + '0';
				} else {
					DBG_FTL(("CfgLib: Ack pending notification logic for %d", (int)owner))
				}
			}
		}
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "ack notify");


	if (hdr) {
		diva_os_free (0, hdr);
		if (cfg_lib->return_data && !diva_cfg_lib_notify_pending()) {
			/*
				Last notification response collected,
				provide response to configuration application
				*/
      diva_cfg_lib_call_notify_callback (TargetCFGLibDrv, 0, 0, 0);
    }
	}
}

int diva_cfg_lib_get_owner_data (byte** data, dword owner) {
	diva_cfg_pending_notify_header_t* hdr = 0;
	diva_cfg_lib_notify_client_t* handle;
	diva_os_spin_lock_magic_t old_irql;
	int ret = -1;

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "ack data");

	if ((handle = (diva_cfg_lib_notify_client_t*)diva_q_find (&cfg_lib->notify_q,
																														&owner,
																														diva_cfg_find_notify))) {
		hdr = (diva_cfg_pending_notify_header_t*)diva_q_get_head (&handle->pending_data_q);
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "ack notify");

	if (hdr) {
		*data = (byte*)&hdr[1];
		ret = (int)hdr->data_length;
	}

	return (ret);
}

/*
  Free CFGLib interface instance
	*/
static void diva_cfg_lib_free_cfg_interface(diva_cfg_lib_access_ifc_t* ifc) {
}

/*
  Function always works on locked storage or on read only copy
  of instance data. No lock is necessary.
  */
static diva_cfg_lib_return_type_t
diva_cfg_storage_read_instance_cfg_var(struct _diva_cfg_lib_access_ifc* ifc,
																			 const byte* instance_data,
																			 const char* name,
																			 diva_cfg_lib_value_type_t value_type,
																			 dword* data_length,
																			 void*  dst) {
	dword var_length;
  const byte* pvar = diva_cfg_find_named_variable (instance_data, name,
																									 strlen(name), &var_length);
	if (pvar) {
		dword max_var_length = *data_length;
		byte* pdata = (void*)dst;

		if (pdata == 0) {
			*data_length = var_length;
			return (DivaCfgLibOK);
		}

		if (max_var_length == 0)
			return (DivaCfgLibParameter);

		if (value_type == DivaCfgLibValueTypeASCIIZ)
			var_length++; /* include trailing zero */

		*data_length = var_length;

		if (max_var_length < var_length)
			return (DivaCfgLibBufferTooSmall);

		switch (value_type) {
			case DivaCfgLibValueTypeASCIIZ:
				if (var_length - 1)
					memcpy (pdata, pvar, var_length - 1);
				pdata[var_length - 1] = 0;
				break;

			case DivaCfgLibValueTypeBinaryString:
				memcpy (pdata, pvar, var_length);
				break;

			case DivaCfgLibValueTypeBool:
			case DivaCfgLibValueTypeSigned:
			case DivaCfgLibValueTypeUnsigned:
				if (var_length <= 4) {
					dword val, i;

					for (i = 0, val = 0; i < var_length; i++) {
						val |= pvar[i] << i*8;
					}
					if (value_type == DivaCfgLibValueTypeBool)
						val = val != 0;

					switch (max_var_length) {
						case 1:
							pdata[0] = (byte)val;
							break;
						case 2:
							WRITE_WORD(pdata, ((word)val));
							break;
						case 4:
							WRITE_DWORD(pdata, val);
							break;
						default:
							return (DivaCfgLibWrongType);
					}
				}
				break;

			default:
				return (DivaCfgLibWrongType);
		}

		return (DivaCfgLibOK);
	}

	return (DivaCfgLibNotFound);
}

/*
	Return information about image
	*/
static diva_cfg_lib_return_type_t
diva_cfg_storage_get_image_info (struct _diva_cfg_lib_access_ifc* ifc,
                                 const byte* instance_data,
                                 diva_cfg_image_type_t requested_image_type,
                                 pcbyte* image_name,
                                 dword*  image_name_length,
                                 pcbyte* archive_name,
                                 dword*  archive_name_length) {

	if (diva_cfg_get_image_info (instance_data,
															 requested_image_type,
															 image_name,
															 image_name_length,
															 archive_name,
															 archive_name_length) == 0) {
		return (DivaCfgLibOK);
	}

	return (DivaCfgLibNotFound);
}

static diva_cfg_lib_return_type_t
diva_cfg_storage_get_adapter_cfg_info (struct _diva_cfg_lib_access_ifc* ifc,
																			 const   byte* instance_data,
																			 dword   offset_after,
																			 dword*  ram_offset,
																			 pcbyte* data,
																			 dword*  data_length) {
	const byte* section = 0;
	dword section_offset;

	while ((section = diva_cfg_get_next_ram_init_section (instance_data, section)) != 0) {
		section_offset = diva_cfg_get_ram_init_offset (section);
		if (offset_after == 0 || section_offset > offset_after) {
			*ram_offset  = section_offset;
			*data_length = diva_cfg_get_ram_init_value (section,data);

			return (DivaCfgLibOK);
		}
	}

	return (DivaCfgLibNotFound);
}

/*
	Enumerate variables which math specified template
	*/
const byte* diva_cfg_storage_enum_variable (struct _diva_cfg_lib_access_ifc* ifc,
																						const   byte* instance_data,
																						const   byte* current_var,
																						const   byte* template,
																						dword   template_length,
																						pcbyte* key,
																						dword*  key_length,
																						pcbyte* name,
																						dword*  name_length) {
	const byte* element, *variable_name;
	dword used, variable_name_length;

	while ((current_var = diva_cfg_get_next_cfg_section (instance_data, current_var)) != 0) {
		if (diva_cfg_get_section_type (current_var) == VieNamedVarName) {
			if ((element = diva_cfg_get_section_element (current_var, VieNamedVarName)) != 0) {
				(void)vle_to_dword (element, &used);
				variable_name_length = diva_get_bs (element+used, &variable_name);

				if (variable_name_length > template_length &&
						(template_length == 0 || memcmp (variable_name, template, template_length) == 0)) {
					const byte* p = variable_name + template_length;
					dword len;

					if (template_length == 0) {
						if (key)
							*key = variable_name;
						if (key_length)
							*key_length = variable_name_length;
						if (name)
							*name = variable_name;
						if (name_length)
							*name_length = variable_name_length;
						return (current_var);
					} else if (*p != '\\') {
						for (len = 0; len + template_length < variable_name_length; len++) {
							if (p[len] == '\\') {
								if(key != 0)
									*key = p;
								if (key_length != 0)
									*key_length = len;
								if (name != 0)
									*name = &p[len+1];
								if (name_length)
									*name_length = variable_name_length - template_length - len - 1;
								return (current_var);
							}
						}
					}
				}
			}
		}
	}

	return (0);
}

static
const byte* diva_cfg_storage_find_variable (struct _diva_cfg_lib_access_ifc* ifc,
																						const byte* instance_data,
																						const byte* name,
																						dword name_length) {
	const byte* section = 0, *element, *variable_name;
	dword used, variable_name_length;

	while ((section = diva_cfg_get_next_cfg_section (instance_data, section)) != 0) {
		if (diva_cfg_get_section_type (section) == VieNamedVarName) {
			if ((element = diva_cfg_get_section_element (section, VieNamedVarName)) != 0) {
				(void)vle_to_dword (element, &used);
				variable_name_length = diva_get_bs (element+used, &variable_name);

				if (variable_name_length == name_length &&
						memcmp(variable_name, name, variable_name_length) == 0) {
					return (section);
				}
			}
		}
	}

	return (0);
}

/*
	Retrieve data section of named variable
	*/
static const byte* diva_cfg_lib_get_named_var_data(const byte* variable, dword* data_length) {
	const byte* element = diva_cfg_get_section_element (variable, VieNamedVarValue);
	const byte* data = 0;

	if (element != 0) {
		dword used;
		(void)vle_to_dword (element, &used);
		*data_length = diva_get_bs  (element+used, &data);
	}

	return (data);
}

#define diva_cfg_implement_read_value(__type__) static diva_cfg_lib_return_type_t \
	diva_cfg_lib_read_##__type__##_value ( const byte* variable, \
																				__type__*   value) { \
	dword data_length; \
	const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length); \
	if (data != 0) { \
		if (data_length <= sizeof(__type__)) { \
			memset (value, 0x00, sizeof(*value)); \
			memcpy (value, data, data_length);    \
			return (DivaCfgLibOK); \
		} \
		return (DivaCfgLibBufferTooSmall); \
	} \
	return (DivaCfgLibWrongType); \
}

diva_cfg_implement_read_value(byte)
diva_cfg_implement_read_value(char)
diva_cfg_implement_read_value(short)
diva_cfg_implement_read_value(word)
diva_cfg_implement_read_value(int)
diva_cfg_implement_read_value(dword)

static diva_cfg_lib_return_type_t diva_cfg_lib_read_64bit_value (const byte* variable,
																																 void* value) {
	dword data_length;
	const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

	if (data != 0) {
		if (data_length <= 8) {
			memset (value, 0x00, 8);
			memcpy (value, data, data_length);
			return (DivaCfgLibOK);
		}
		return (DivaCfgLibBufferTooSmall);
	}

	return (DivaCfgLibWrongType);
}

/*
	Read binary string value
	*/
static diva_cfg_lib_return_type_t diva_cfg_lib_read_binary_string (const byte* variable,
																																	 void* value,
																																	 dword* length,
																																	 dword max_length) {
	dword data_length;
	const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

	if (data != 0) {
		if (data_length <= max_length) {
			memset (value, 0x00, max_length);
			memcpy (value, data, data_length);
			*length = data_length;
			return (DivaCfgLibOK);
		}
		return (DivaCfgLibBufferTooSmall);
	}

	return (DivaCfgLibWrongType);
}

/*
	Read ascii zero terminated string
	Max length includes terminating zero
	*/
static diva_cfg_lib_return_type_t diva_cfg_lib_read_asciiz_string (const byte* variable,
																																   char* value,
																																   dword max_length) {
	dword data_length;
	const byte* data = diva_cfg_lib_get_named_var_data(variable, &data_length);

	if (data != 0) {
		int zero_present = (data_length && data[data_length-1] == 0);
		dword length = zero_present ? data_length : (data_length+1);

		if (length <= max_length) {
			memset (value, 0x00, max_length);
			memcpy (value, data, data_length);
			return (DivaCfgLibOK);
		}
		return (DivaCfgLibBufferTooSmall);
	}
	return (DivaCfgLibWrongType);
}

/*
	Allows access to variable data
	*/
static const byte* diva_cfg_lib_get_variable_data (const byte* variable, dword* length) {
	return (diva_cfg_lib_get_named_var_data(variable,length));
}

/*
	Returns pointer to the start of the section and the section length
	*/
static const byte* diva_mtpx_get_section (diva_section_target_t target,
																					int instance_by_name,
																					const byte* instance_ident,
																					dword instance_ident_length,
																					dword instance_nr,
																					dword* instance_length) {
	const byte* owner;

	if ((owner = diva_cfg_parser_get_owner_configuretion (cfg_lib, target)) != 0) {
		dword owner_length = get_max_length(owner);
		if (owner_length != 0) {
			const byte* instance = diva_cfg_lib_find_instance (instance_by_name ? 0 : instance_nr,
																												 instance_by_name ? instance_ident : 0,
																												 instance_by_name ? instance_ident_length : 0,
																												 owner, owner_length);
			if (instance != 0) {
				dword length = get_max_length(instance);
				if (length != 0) {
					*instance_length = length;
					return (instance);
				}
			}
		}
	}

	return (0);
}

/*
	Provides access to instance data

	1. Take speen lock and calculate amount of memory necessary
	2. Reslease spin lock and alloc all necessary memory
	3. Take spin lock and check if instance data fits in the
		 allocated memory area. If not then release spin lock,
		 free allocated mamory and repeat the operation
	4. Copy instance data to the allocated mmeory and release
		 spin lock
	5. Translate data in case instance owned by XDI adapter
	*/
static const byte* diva_cfg_lib_get_instance (struct _diva_cfg_lib_access_ifc* ifc,
																							diva_section_target_t target,
																							int instance_by_name,
																							const byte* instance_ident,
																							dword instance_ident_length,
																							dword instance_nr) {
	diva_os_spin_lock_magic_t old_irql;
	dword mem_length;
	byte* mem = 0;
	const byte* instance;
	dword instance_length;

	if (cfg_lib == 0 || cfg_lib->storage == 0)
		return (0);

	for (;;) {
		if (mem)
			diva_os_free (0, mem);
		mem_length = 0;
		mem = 0;

		diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "get instance");
		instance = diva_mtpx_get_section (target, instance_by_name, instance_ident,
																 instance_ident_length, instance_nr, &mem_length);
		diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "get instance");

		if (instance == 0 || mem_length == 0 || (mem = diva_os_malloc (0, mem_length+sizeof(void*))) == 0)
			return (0);

		memset (mem, 0x00, mem_length+sizeof(void*));


		diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "get instance");
		instance = diva_mtpx_get_section (target, instance_by_name, instance_ident,
																	 instance_ident_length, instance_nr, &instance_length);

		if (instance != 0 && instance_length != 0 && instance_length <= mem_length) {
			memcpy (mem, instance, instance_length);
		}
		diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "get instance");

		if (instance != 0 && instance_length != 0 && instance_length <= mem_length) {
			if (target == TargetXdiAdapter || target == TargetSoftIPAdapter) {
				dword xdi_length;
				byte* xdi = diva_cfg_translate_xdi_data (0, mem, &xdi_length);
				if (xdi == 0) {
					diva_os_free (0, mem);
					return (0);
				} else {
					DBG_TRC(("CfgLib: instance len=%d xdi_instance_len=%d", instance_length, get_max_length(xdi)))
					diva_os_free (0, mem);
					mem = xdi;
				}
			}
			return (mem);
		}
	}

	return (0);
}

/*
	Release instance data
	*/
static void diva_cfg_lib_release_instance (struct _diva_cfg_lib_access_ifc* ifc, const byte* instance) {
	if (instance)
		diva_os_free (0, (byte*)instance);
}

/*
	Retrieve information about section ident
	*/
static diva_cfg_lib_return_type_t
diva_cfg_lib_get_instance_ident (const byte* instance,
																 diva_vie_id_t* ident_type,
																 pcbyte* ident,
																 dword*  ident_length,
																 dword*  ident_nr) {
	dword used, value, position = 0;

	(void)vle_to_dword (instance, &used); /* tlie tag */
	position += used;
	vle_to_dword (instance+position, &used); /* total length */
	position += used;

	value = vle_to_dword (instance+position, &used); /* instance handle type */
	position += used;

	if (value == VieInstance2) {
		*ident_type = VieInstance2;
		if (ident && ident_length) {
			*ident_length = diva_get_bs  (instance+position, ident);
			if (ident_nr)
				*ident_nr = 0;
			return (DivaCfgLibOK);
		}
		return (DivaCfgLibWrongType);
	} else if (value == VieInstance) {
		*ident_type = VieInstance;
		if (ident_nr != 0) {
			*ident_nr = vle_to_dword (instance+position, &used);
			if (ident_length)
				*ident_length = 0;
			if (ident)
				*ident        = 0;
			return (DivaCfgLibOK);
		}
		return (DivaCfgLibWrongType);
	}

	return (DivaCfgLibNotFound);
}

/*
	Retrieve information about variable ident
	*/
static diva_cfg_lib_return_type_t
diva_cfg_lib_get_name_ident (const byte* variable, pcbyte* ident, dword* ident_length) {
	const byte* name = diva_cfg_get_section_element (variable, VieNamedVarName);

	if (name != 0) {
		dword used;

		(void)vle_to_dword (name, &used);
		*ident_length = diva_get_bs (name+used, ident);

		return (DivaCfgLibOK);
	}

	return (DivaCfgLibWrongType);
}

/*
	Read configuration value from configuration storage
	*/
static diva_cfg_lib_return_type_t
diva_cfg_storage_read_cfg_var (struct _diva_cfg_lib_access_ifc* ifc,
															 diva_section_target_t owner,
															 dword instance_by_nr,
															 byte* instance_by_name,
															 int   instance_by_name_length,
															 const char* name,
															 diva_cfg_lib_value_type_t value_type,
															 dword* data_length,
															 void* pdata) {
	diva_os_spin_lock_magic_t old_irql;
	diva_cfg_lib_return_type_t ret;
	const byte* powner, *instance;
	dword max_length;

	if (!cfg_lib) {
		DBG_FTL(("CfgLib: diva_cfg_storage_read_cfg_var failed, library not initialized"))
		return (DivaCfgLibNotFound);
	}

	diva_os_enter_spin_lock (&cfg_lib->notify_lock, &old_irql, "read cfg var");

	if ( (cfg_lib->storage != 0) &&
      (powner = diva_cfg_parser_get_owner_configuretion (cfg_lib, owner)) != 0 &&
			(max_length = get_max_length (powner)) != 0 &&
			(instance = diva_cfg_lib_find_instance (instance_by_nr,
																							instance_by_name,
																							instance_by_name_length,
																							powner,
																							max_length)) != 0) {
		ret = diva_cfg_storage_read_instance_cfg_var (ifc, instance, name,
																									value_type, data_length,
																									pdata);
	} else {
		ret = DivaCfgLibNotFound;
	}

	diva_os_leave_spin_lock (&cfg_lib->notify_lock, &old_irql, "read cfg var");

	return (ret);
}

static const byte* diva_cfg_lib_copy_instance (const byte* instance) {
	dword length = get_max_length(instance);
	byte* mem = diva_os_malloc (0, length+sizeof(void*));

	if (mem != 0) {
		memset (&mem[length], 0x00, sizeof(void*));
		memcpy (mem, instance, length);
	}

	return (mem);
}

void* diva_cfg_lib_get_cfg_interface (void) {
	static diva_cfg_lib_access_ifc_t ifc =
                   { sizeof(ifc),
                     0, /* version */
                     0, /* storage */
                     0, /* context */
                     diva_cfg_lib_free_cfg_interface,
                     diva_cfg_storage_read_cfg_var,
                     diva_cfg_lib_cfg_register_notify_callback,
                     diva_cfg_lib_cfg_remove_notify_callback,
                     diva_cfg_storage_read_instance_cfg_var,
                     diva_cfg_storage_get_image_info,
                     diva_cfg_storage_get_adapter_cfg_info,
                     diva_cfg_storage_enum_variable,
                     diva_cfg_storage_find_variable,
                     diva_cfg_lib_read_char_value,
                     diva_cfg_lib_read_byte_value,
                     diva_cfg_lib_read_short_value,
                     diva_cfg_lib_read_word_value,
                     diva_cfg_lib_read_int_value,
                     diva_cfg_lib_read_dword_value,
                     diva_cfg_lib_read_64bit_value,
										 diva_cfg_lib_read_binary_string,
										 diva_cfg_lib_read_asciiz_string,
                     diva_cfg_lib_get_variable_data,
                     diva_cfg_lib_get_instance,
                     diva_cfg_lib_release_instance,
                     diva_cfg_lib_get_instance_ident,
                     diva_cfg_lib_get_name_ident,
                     diva_cfg_lib_copy_instance };

	if (cfg_lib != 0 && cfg_lib->storage != 0) {
		return (&ifc);
  }

  return (0);
}

/*
	Management interface access state machine.
	The purpose of this state machine is to apply all named variables
	which contains management interface path to management interface of the
	xdi adapter.

	- If request function is provided then this is necessary to find instance
		using the request function.
	- If instance is given then this is necessary to find request using instance
		data
	- If instance and variable is given then this is necessary to find request function
		using instance data and apply only the provided variable (supposed variable
		provides management interface info).


	*/

static void diva_cfg_send_mgnt_request (diva_cfg_lib_management_ifc_state_t* pE,
																				byte Req, byte xlength, int use_internal_request) {
	pE->e.Req        = Req;
	pE->e.XNum       = 1;
  pE->e.X          = &pE->XData;
  pE->e.X->PLength = xlength;
  pE->e.X->P       = &pE->xdata[0];
	pE->e.RNR        = 0;

	pE->rc_pending = 1;

  if (use_internal_request != 0 && pE->internal_request != 0) {
		(*(pE->internal_request))(pE->internal_request_context, &pE->e);
  } else {
   (*(pE->request))(&pE->e);
  }
}

/*
	Enumerate over named variables with management interface information
	*/
const byte* diva_cfg_find_next_var_with_management_info (const byte* instance,
																												 const byte* var) {
	while ((var = diva_cfg_storage_enum_variable (0, instance, var, 0, 0, 0, 0, 0, 0)) != 0) {
		const byte* info;

		if ((info = diva_cfg_get_section_element (var, VieManagementInfo)) != 0) {
			const byte* management_info;
			dword used;

			(void)vle_to_dword (info, &used);
			used = (byte)diva_get_bs  (info+used, &management_info);

			if (used != (sizeof(DIVA_CFG_READONLY_MANAGEMENT_INFO)-1) ||
					memcmp(management_info, DIVA_CFG_READONLY_MANAGEMENT_INFO, sizeof(DIVA_CFG_READONLY_MANAGEMENT_INFO)-1) != 0) {
				return (var);
			}
		}
	}

	return (0);
}

static byte diva_cfg_create_read_req (diva_cfg_lib_management_ifc_state_t* pE) {
	const byte* info = diva_cfg_get_section_element (pE->variable ? pE->variable : pE->var, VieManagementInfo);
	byte* P = &pE->xdata[0], *plen;
	const byte* variable_name, *management_info;
	dword used, management_info_length;
	word	length;
	byte variable_name_length;

	pE->write_var_name = 0;
	pE->write_var_name_length = 0;

	*P = 0;
	(void)vle_to_dword (info, &used);
	management_info_length = (byte)diva_get_bs  (info+used, &management_info);

	if (management_info[0] == (byte)'/') {
		char tmp[3];
		dword i = pE->script_offset + 1;
		char* end = 0, entry_type;
		byte entry_length;

		if (i >= management_info_length) { pE->script_offset = 0; return (0); }
		tmp[0] = (char)management_info[i++];
		if (i >= management_info_length) { pE->script_offset = 0; return (0); }
		tmp[1] = (char)management_info[i++];
		if (i >= management_info_length) { pE->script_offset = 0; return (0); }
		tmp[2] = 0;
		entry_length = (byte)strtol(&tmp[tmp[0] == '0' ? 1 : 0], &end, 16);

		DBG_REG(("CfgLib: Entry length = %s : %d", tmp, entry_length))

		if (entry_length < 2 || (i+entry_length) > management_info_length) {
			pE->script_offset = 0;
			pE->error = __LINE__;
			return (0);
		}
		entry_type = (char)management_info[i++];
		if (entry_type == 'E' || entry_type == 'W') {
			variable_name        = &management_info[i];
			variable_name_length = (byte)(entry_length - 1);
			pE->script_offset    = i + entry_length - 2;
		} else {
			pE->script_offset = 0;
			pE->error = __LINE__;
			return (0);
		}
	} else {
		pE->script_offset = 0;
		variable_name = management_info;
		variable_name_length = (byte)management_info_length;
	}

	/*
		Apply enumeration if any
		*/
	if (pE->final_enum_nr > 0) {
		int i;

		for (i = 0; i < variable_name_length; i++) {
			if (variable_name[i] == '@')
				break;
		}
		if (i > 0 && i < variable_name_length) {
			int len = i;

			memcpy (&pE->var_name_buffer[0], &variable_name[0], i);
			len += diva_snprintf (&pE->var_name_buffer[i],
														sizeof(pE->var_name_buffer)-20,
														"%d", pE->final_enum_nr);
			i++;
			memcpy (&pE->var_name_buffer[len], &variable_name[i], variable_name_length - i);
			len += variable_name_length - i;

			variable_name        = &pE->var_name_buffer[0];
			variable_name_length = (byte)len;
		}
	}


	pE->write_var_name        = variable_name;
	pE->write_var_name_length = (byte)variable_name_length;

	DBG_TRC(("CfgLib: read management interface variable"))
	DBG_BLK(((void*)variable_name, variable_name_length))


	*P++ = ESC;
	plen = P++;
	*P++ = 0x80; /* MAN_IE */
	*P++ = 0x00; /* Type */
	*P++ = 0x00; /* Attribute */
	*P++ = 0x00; /* Status */
	*P++ = 0x00; /* Variable Length */
	*P++ = variable_name_length;
	memcpy (P, variable_name, variable_name_length);
	P += variable_name_length;
	*plen = variable_name_length + 0x06;
  *P = 0;

	length = variable_name_length + 0x09;

	pE->rdata[0] = 0;

	return ((byte)length);
}

static const byte* diva_cfg_get_variable_data (const byte* value, dword* data_length) {
	diva_vie_id_t id = diva_cfg_get_section_type (value);
	diva_cfg_section_t* ref;
	dword used;
	static diva_vie_id_t idents [] = { VieNamedVarValue, VieRamInitValue, ViePcInitValue };
	const byte* element, *variable_data;
	int i;

	if ((id <= IeNotDefined) || (id >= VieLlast) || (!(ref = identify_section (id)))) {
		return (0);
	}

	for (i = 0; i < sizeof(idents)/sizeof(idents[0]); i++) {
		if ((element = diva_cfg_get_section_element (value, idents[i]))) {
			(void)vle_to_dword (element, &used);
			*data_length = diva_get_bs  (element+used, &variable_data);
			return (variable_data);
		}
	}

	return (0);
}

static int diva_cfg_create_write_req (diva_cfg_lib_management_ifc_state_t* pE) {
	const diva_man_var_header_t* rVar = (diva_man_var_header_t*)&pE->rdata[0];
	diva_man_var_header_t* pVar = (diva_man_var_header_t*)&pE->xdata[0];
	dword variable_value_length = 0, variable_name_length;
	const byte* variable_name;
	const byte* variable_value = diva_cfg_get_variable_data (
															pE->variable ? pE->variable : pE->var, &variable_value_length);
	byte max_value_length = rVar->value_length, *P;
	word length;
	int man_execute = 0;


	variable_name = pE->write_var_name;
	variable_name_length = (byte)pE->write_var_name_length;

	pE->write_var_name        = 0;
	pE->write_var_name_length = 0;

	if (variable_name == 0 || variable_name_length == 0) {
		pE->error = __LINE__;
		return (-1);
	}

	if (rVar->type == 0x02) {
		/*
			MI_EXECUTE
			*/
		man_execute = 1;
		variable_value_length = 0;
	} else if ((rVar->attribute & 0x01) != 0) {
		if ((rVar->type == 0x03) || (rVar->type == 0x04)) {
			if ((variable_value_length+1) > max_value_length) {
				pE->error = __LINE__;
				return (-1);
			}
		} else if (variable_value_length > max_value_length) {
			pE->error = __LINE__;
			return (-1);
		}
	} else {
		pE->error = __LINE__;
		return (-1);
	}

	memcpy (pVar, rVar, sizeof(*pVar));

	memcpy (pVar, rVar, sizeof(*pVar));
	memcpy (&pVar->path_length + 1, variable_name, variable_name_length);
	P = &pVar->path_length + 1 + variable_name_length;

	if (man_execute == 0) {
		if (pVar->type == 0x03 /* MI_ASCIIZ */) {
			memcpy (P, variable_value, variable_value_length);
			P += variable_value_length;
			*P++ = 0;
			variable_value_length++;
		} else if (pVar->type == 0x04 /* MI_ASCII */ || pVar->type == 0x05 /* MI_NUMBER */) {
			*P++ = (byte)variable_value_length;
			memcpy (P, variable_value, variable_value_length);
			P += variable_value_length;
			variable_value_length++;
		} else if (diva_cfg_check_management_variable_value_type (pVar->type) > 0) {
			/*
				Unsigned variable
				*/
			byte i;

			for (i = 0; i < max_value_length; i++) {
				if (i < variable_value_length) {
					*P++ = variable_value[i];
				} else {
					*P++ = 0;
				}
			}
			variable_value_length = max_value_length;

			memcpy (P, variable_value, variable_value_length);
			P += variable_value_length;
		} else if (diva_cfg_check_management_variable_value_type (pVar->type) == 0) {
			/*
				Signed variable
				*/
			dword src;

			switch (variable_value_length) {
				case 1:
					src = (signed char)variable_value[0];
					break;

				case 2:
					src = (short)(variable_value[0] | variable_value[1] << 8);
					break;

				case 4:
					src = (int)(variable_value[0]       | variable_value[1] << 8 |
											variable_value[2] << 16 | variable_value[3] << 24);
					break;

				default:
					pE->error = __LINE__;
					return (-1);
			}

			switch (max_value_length) {
				case 1:
					*P++ = (byte)src;
					break;

				case 2:
					*P++ = (byte)src;
					*P++ = (byte)(src >> 8);
					break;

				case 4:
					*P++ = (byte)src;
					*P++ = (byte)(src >>  8);
					*P++ = (byte)(src >> 16);
					*P++ = (byte)(src >> 24);
					break;

				default:
					pE->error = __LINE__;
					return (-1);
			}

			variable_value_length = max_value_length;
		} else {
			pE->error = __LINE__;
			return (-1);
		}
	}

	pVar->length       = variable_value_length + variable_name_length + 6;
	pVar->value_length = (byte)variable_value_length;
	*(&pVar->path_length + 1 + variable_name_length + variable_value_length) = 0;
	length = pVar->length + 3;

	pE->rdata[0] = 0;

	return (length);
}

static void diva_cfg_async_update (ENTITY* e) {
	diva_cfg_lib_management_ifc_state_t* pE =
			DIVAS_CONTAINING_RECORD(e, diva_cfg_lib_management_ifc_state_t, e);
	int read_next_variable = 0;

	pE->message_nr++;

	switch (pE->state) {
		case 0: /* Assign complete */
			if (e->Rc == ASSIGN_OK) {
				byte length = diva_cfg_create_read_req(pE);
				e->Rc = 0;
				pE->state = 1;
				pE->rc_pending = 0;
				if (length != 0) {
					diva_cfg_send_mgnt_request (pE, MAN_READ, length, 1);
				} else {
					read_next_variable = 1;
				}
			} else {
				DBG_ERR(("CfgLib: management entity assign failed"))
				pE->status = -1;
			}
			break;

		case 1:
			if (e->complete == 255) {
				if (e->Rc != 0xff) {
					read_next_variable = 1;
					pE->error = __LINE__;
					DBG_ERR(("CfgLib: failed to read management interface variable"))
				}
				pE->rc_pending = 0;
				e->Rc = 0;
			} else {
				e->Ind = 0;
				if (e->complete != 2) {
					if (e->RLength > (sizeof(pE->rdata)-1)) {
						DBG_ERR(("CfgLib: management interface variable too long"))
						pE->error = __LINE__;
						e->RNum = 0;
						e->RNR  = 2;
						if (pE->rc_pending == 0)
							read_next_variable = 1;
						else
							pE->state = 3;
					} else {
						e->RNum       = 1;
						e->R          = &pE->RData;
						e->R->P       = &pE->rdata[0];
						e->R->PLength = sizeof(pE->rdata) - 1;
						e->RNR        = 0;
					}
				} else {
					pE->rdata[e->R->PLength] = 0;
				}
			}
			if (pE->rc_pending == 0 && pE->rdata[0] == ESC) {
				byte var_type = ((diva_man_var_header_t*)&pE->rdata[0])->type;
				int write_data_length = diva_cfg_create_write_req(pE);

				if (write_data_length > 0) {
					pE->state = 2;
					diva_cfg_send_mgnt_request (pE,
																			(var_type == 0x02) ? MAN_EXECUTE : MAN_WRITE,
																			(byte)write_data_length, 1);
				} else {
					DBG_ERR(("CfgLib: wrong type of management interface variable"))
					pE->error = __LINE__;
					read_next_variable = 1;
				}
			}
			break;

		case 2:
			if (e->complete == 255) {
				if (e->Rc != 0xff) {
					DBG_ERR(("CfgLib: failed to write management interface variable"))
					pE->error = __LINE__;
				}
				read_next_variable = 1;
				pE->rc_pending = 0;
				e->Rc = 0;
			}
			break;

		case 3:
			if (e->complete == 255) {
				read_next_variable = 1;
				e->Rc = 0;
				pE->rc_pending = 0;
				pE->state = 1;
			}
			break;

		case -1: /* Remove complete */
			if (e->complete == 255 && e->Id == 0) {
				if ((pE->rc_pending = e->Rc) != 0xff) {
					pE->error = __LINE__;
				}
				pE->status = 1;
				e->Rc = 0;
			}
			break;
	}

	if (read_next_variable != 0) {
		byte length = 0;

		if (pE->variable != 0) {
			while (pE->script_offset != 0 && length == 0) {
				length = diva_cfg_create_read_req(pE);
			}
		} else {
			int next_var = 0;

			do {
				length = 0;

				while (pE->script_offset != 0 && length == 0) {
					length = diva_cfg_create_read_req(pE);
				}

				if (next_var != 0 && pE->script_offset == 0 && length == 0) {
					length = diva_cfg_create_read_req(pE);
				}

				next_var = 1;
			} while (length == 0 && (pE->var = diva_cfg_find_next_var_with_management_info (pE->instance, pE->var)) != 0);
		}
		if (length != 0) {
			pE->state    = 1;
			diva_cfg_send_mgnt_request (pE, MAN_READ, length, 1);
		} else {
			pE->xdata[0] =  0;
			pE->state    = -1;
			diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
		}
	}
}

static void diva_read_management_instance_ident (diva_cfg_lib_management_ifc_state_t* pE) {
	byte* P = &pE->xdata[0], *plen;
	byte variable_name_length = pE->enum_prefix_length, length;

	pE->enum_nr++;

	*P++ = ESC;
	plen = P++;
	*P++ = 0x80; /* MAN_IE */
	*P++ = 0x00; /* Type */
	*P++ = 0x00; /* Attribute */
	*P++ = 0x00; /* Status */
	*P++ = 0x00; /* Variable Length */
	P++;         /* variable_name_length */

	memcpy (P, pE->enum_prefix, variable_name_length);
	variable_name_length += (byte)diva_snprintf (&P[variable_name_length],
                                           sizeof(pE->xdata)-variable_name_length-10,
                                           "%d\\II", pE->enum_nr);
	P[-1] = variable_name_length;
	P += variable_name_length;
	*plen = variable_name_length + 0x06;
  *P = 0;

	length = variable_name_length + 0x09;

	pE->rdata[0] = 0;

	diva_cfg_send_mgnt_request (pE, MAN_READ, length, 1);
}

static int diva_check_enum_instance (diva_cfg_lib_management_ifc_state_t* pE) {
	const diva_man_var_header_t* pVar = (diva_man_var_header_t*)&pE->rdata[0];
	byte* ptr = (byte*)&pVar->path_length;

	if (pVar->escape != ESC)
		return (-1);

	ptr += (pVar->path_length + 1);

	if        (pE->enum_name != 0 && pVar->type == 0x03 /* MI_ASCIIZ */) {
		if (pE->enum_data == (pVar->value_length - 1) &&
				memcmp (ptr, pE->enum_name, pE->enum_data) == 0)
			return (0);
	} else if (pE->enum_name == 0 && pVar->type == 0x82 /* MI_UINT   */) {
		switch (pVar->value_length) {
			case 1:
				if (pE->enum_data == (dword)ptr[0])
					return (0);
				break;
			case 2:
				if (pE->enum_data == (dword)READ_WORD(ptr))
					return (0);
				break;
			case 4:
				if (pE->enum_data == (dword)READ_DWORD(ptr))
					return (0);
				break;
		}
	}

	return (-1);
}

static void diva_cfg_async_enum (ENTITY* e) {
	diva_cfg_lib_management_ifc_state_t* pE =
			DIVAS_CONTAINING_RECORD(e, diva_cfg_lib_management_ifc_state_t, e);
	int schedule_read = 0;
	byte Rc;

	pE->message_nr++;

	switch (pE->state) {
		case 0:
			pE->rc_pending = 0;
			Rc = e->Rc;
			e->Rc = 0;
			if (Rc == ASSIGN_OK) {
				schedule_read = 1;
			} else {
				pE->state  = -1;
				pE->error  = __LINE__;
				pE->status = -1;
				return;
			}
			break;

		case 1:
			if (e->complete == 255) {
				pE->rc_pending = 0;
				Rc = e->Rc;
				e->Rc          = 0;
				if (Rc == 0xff) {
					pE->state = 2;
				} else {
					pE->error = __LINE__;
					pE->state = -1;
					pE->xdata[0] =  0;
					diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
				}
			} else {
				e->Ind = 0;
				if (e->complete != 2) {
					if (e->RLength > (sizeof(pE->rdata)-1)) {
						pE->error = __LINE__;
						e->RNum = 0;
						e->RNR  = 2;
						pE->state = 3;
					} else {
						e->RNum       = 1;
						e->R          = &pE->RData;
						e->R->P       = &pE->rdata[0];
						e->R->PLength = sizeof(pE->rdata) - 1;
						e->RNR        = 0;
					}
				} else {
					pE->rdata[e->R->PLength] = 0;
					pE->state = 3;
				}
			}
			break;

		case 2:
			e->Ind = 0;
			if (e->complete != 2) {
				if (e->RLength > (sizeof(pE->rdata)-1)) {
					pE->error = __LINE__;
					pE->state = -1;
					pE->xdata[0] =  0;
					diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
				} else {
					e->RNum       = 1;
					e->R          = &pE->RData;
					e->R->P       = &pE->rdata[0];
					e->R->PLength = sizeof(pE->rdata) - 1;
					e->RNR        = 0;
				}
			} else {
				pE->rdata[e->R->PLength] = 0;

				if (diva_check_enum_instance(pE) == 0) {
					pE->final_enum_nr = (int)pE->enum_nr;
					pE->state = -1;
					pE->xdata[0] =  0;
					diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
				} else {
					schedule_read = 1;
				}
			}
			break;

		case 3:
			if (e->complete == 255) {
				pE->rc_pending = 0;
				Rc = e->Rc;
				e->Rc = 0;

				if (pE->error != 0 || Rc != 0xff) {
					pE->error = __LINE__;
					pE->state = -1;
					pE->xdata[0] =  0;
					diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
				} else {
					if (diva_check_enum_instance(pE) == 0) {
						pE->final_enum_nr = (int)pE->enum_nr;
						pE->state = -1;
						pE->xdata[0] =  0;
						diva_cfg_send_mgnt_request (pE, REMOVE, 0, 1);
					} else {
						schedule_read = 1;
					}
				}
			}
			break;

		case -1:
			e->Rc = 0;
			if (e->complete == 255 && e->Id == 0) {
				pE->status = pE->final_enum_nr;
				return;
			}
			break;
	}

	if (schedule_read) {
		pE->state = 1;
		diva_read_management_instance_ident(pE);
	}
}

/*
	Find management interface instance using enumeration procedure
	*/
void diva_cfg_find_management_instance_by_enumeration (diva_cfg_lib_management_ifc_state_t* state) {

	state->final_enum_nr = -1;
	state->state      = 0;
	state->e.callback = diva_cfg_async_enum;
	state->e.Id       = MAN_ID;
	diva_cfg_send_mgnt_request (state, ASSIGN, 0, 0);

	if (state->xdi_adapter != 0 && state->e.No == 0) {
		if (state->free_instance != 0) {
			state->free_instance = 0;
			diva_os_free (0, (void*)state->instance);
			state->instance = 0;
		}
		state->status = -1;
		return;
	}

}

/*
		Apply named variables.
		Instance set - find request function of XDI adapter and update
		Request  set - find XDI adapter configuration instance and update
		Variable and request are set - update only provided variable.
	*/
static void diva_cfg_lib_apply_named_variables (diva_cfg_lib_management_ifc_state_t* state) {
	if (state->request == 0) {
		/*
			Find request function using instance ident
			*/
		const byte* instance_name;
		dword instance_name_length, logical_adapter_nr;
		diva_vie_id_t instance_name_type;

		if (diva_cfg_lib_get_instance_ident (state->instance,
																				 &instance_name_type,
																				 &instance_name,
																				 &instance_name_length,
																				 &logical_adapter_nr) != DivaCfgLibOK) {
			state->status = -1;
			return;
		}

		if ((state->request = diva_cfg_get_request_function (TargetXdiAdapter,
																&state->xdi_adapter,
																instance_name_type == VieInstance2,
																instance_name_type == VieInstance2 ? instance_name : 0,
																instance_name_type == VieInstance2 ? instance_name_length : 0,
																instance_name_type == VieInstance2 ?  0 : logical_adapter_nr)) == 0) {
			state->status = 1;
			return;
		}
	} else if (state->variable == 0) {
		/*
			Find instance using request function
			*/
		IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));
		dword logical_adapter_nr;
		char* xdi_adapter_name;
		unsigned long cardtype;

		if (sync_req == 0) {
			if (sync_req != 0)
				diva_os_free (0, sync_req);
			state->status = -1;
			return;
		}

		memset (sync_req, 0x00, sizeof(*sync_req));

		sync_req->xdi_logical_adapter_number.Req = 0;
 		sync_req->xdi_logical_adapter_number.Rc = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
		sync_req->xdi_logical_adapter_number.info.logical_adapter_number = 0;
		(*(state->request))((ENTITY *)sync_req);
		logical_adapter_nr = sync_req->xdi_logical_adapter_number.info.logical_adapter_number;

		sync_req->GetCardType.Req = 0;
		sync_req->GetCardType.Rc  = IDI_SYNC_REQ_GET_CARDTYPE;
		sync_req->GetCardType.cardtype = 0;
		(*(state->request))((ENTITY *)sync_req);
		cardtype = sync_req->GetCardType.cardtype;

		sync_req->GetName.Req     = 0;
		sync_req->GetName.Rc      = IDI_SYNC_REQ_GET_NAME;
		sync_req->GetName.name[0] = 0;
		(*(state->request))((ENTITY *)sync_req);
		xdi_adapter_name = (char*)&sync_req->GetName.name[0];

		if ((state->instance = diva_cfg_lib_get_instance (0,
								((cardtype != CARDTYPE_DIVASRV_SOFTIP_V20) && (cardtype != CARDTYPE_DIVASRV_HOST_MEDIA_BOARD)) ?
								TargetXdiAdapter : TargetSoftIPAdapter,
																											1, xdi_adapter_name,
																											strlen(xdi_adapter_name), 0)) == 0) {
			state->instance = diva_cfg_lib_get_instance (0,
								((cardtype != CARDTYPE_DIVASRV_SOFTIP_V20) && (cardtype != CARDTYPE_DIVASRV_HOST_MEDIA_BOARD)) ?
								TargetXdiAdapter : TargetSoftIPAdapter,
																									 0, 0, 0, logical_adapter_nr);
		}

		diva_os_free (0, sync_req);

		if (state->instance == 0) {
			state->status = -1;
			return;
		}

		state->free_instance = 1;
	}

	if (state->variable == 0) {
		if ((state->var = diva_cfg_find_next_var_with_management_info (state->instance, 0)) == 0) {
			if (state->free_instance != 0)
				diva_os_free (0, (void*)state->instance);
			state->free_instance = 0;
			state->instance      = 0;
			state->status        = 1;
			return;
		}
	}

	/*
		Now instance handle and the request function both are available
		and named variables can be applied to management interface
		*/
	memset (&state->e, 0x00, sizeof(state->e));
	state->state      = 0;
	state->e.callback = diva_cfg_async_update;
	state->e.Id       = MAN_ID;
	diva_cfg_send_mgnt_request (state, ASSIGN, 0, 0);

	if (state->xdi_adapter != 0 && state->e.No == 0) {
		if (state->free_instance != 0) {
			state->free_instance = 0;
			diva_os_free (0, (void*)state->instance);
			state->instance = 0;
		}
		state->status = -1;
		return;
	}
}

int diva_cfg_lib_update_named_xdi_variables (const byte* instance) {
	diva_cfg_lib_management_ifc_state_t* pE =
		(diva_cfg_lib_management_ifc_state_t*)diva_os_malloc (0, sizeof(*pE));
	int ret = -1;

	if (pE != 0) {
		int i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT, message_nr = 0;

		memset (pE, 0x00, sizeof(*pE));
		pE->instance = instance;

		diva_cfg_lib_apply_named_variables (pE);

		while (pE->status == 0 && i--) {
			if (pE->message_nr != message_nr) {
				message_nr = pE->message_nr;
				i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT;
			}
			diva_os_sleep(DIVA_CFG_TIMER_RESOLUTION);
		}

		if (pE->free_instance)
			diva_os_free (0, (char*)pE->instance);

		if (pE->status == 1 && pE->error == 0) {
			ret = 0;
			DBG_REG(("CfgLib: updated named variables after configuration change"))
		} else if (pE->status == 0) {
			DBG_ERR(("CfgLib: management interface operation timeout"))
		} else if (pE->error != 0) {
			DBG_ERR(("CfgLib: management interface error at %d", pE->error))
		}
		diva_os_free (0, pE);
	}

	return (ret);
}

static IDI_CALL diva_cfg_get_request_function (diva_section_target_t target,
																							 int* xdi_adapter,
																							 int instance_by_name,
																							 const byte* instance_ident,
																							 dword instance_ident_length,
																							 dword instance_nr) {
	IDI_CALL request = 0;

	*xdi_adapter = 0;

	if (target == TargetXdiAdapter    ||
			target == TargetSoftIPAdapter ||
			target == TargetCapiDrv       ||
			target == TargetMtpxMngt      ||
			target == TargetMtpxAdapter   ||
			target == TargetComDrv        ||
			target == TargetSoftIPControl ||
			target == TargetSipControl    ||
			target == TargetMTP3          ||
			target == TargetISUP          ||
			target == TargetDiva_IP_HMP) {
		DESCRIPTOR* t = (DESCRIPTOR*)diva_os_malloc (0, 2*MAX_DESCRIPTORS*sizeof(*t));
		IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));

		if (t != 0 && sync_req != 0) {
			int x;

			memset (t, 0x00, 2*MAX_DESCRIPTORS*sizeof(*t));
			memset (sync_req, 0x00, sizeof(*sync_req));
			diva_os_read_descriptor_array (t, MAX_DESCRIPTORS*sizeof(*t));

			for (x = 0; x < MAX_DESCRIPTORS; x++) {
				if (t[x].type == IDI_DADAPTER) {
					IDI_CALL dadapter_request = t[x].request;

					for (x = 0; x < MAX_DESCRIPTORS; x++) {
						if (t[x].type == 0 && t[x].request == 0) {
							sync_req->didd_read_adapter_array.e.Req = 0;
							sync_req->didd_read_adapter_array.e.Rc  = IDI_SYNC_REQ_DIDD_READ_ADAPTER_ARRAY;
							sync_req->didd_read_adapter_array.info.buffer = &t[x];
							sync_req->didd_read_adapter_array.info.length = MAX_DESCRIPTORS*sizeof(*t);
							(*dadapter_request)((ENTITY*)sync_req);
							break;
						}
					}
					break;
				}
      }

			for (x = 0; ((request == 0) && (x < 2*MAX_DESCRIPTORS)); x++) {
				int descriptor_ok = 0;

				switch(target) {
					case TargetXdiAdapter:
					case TargetSoftIPAdapter:
						descriptor_ok = t[x].channels != 0 && t[x].type < IDI_VADAPTER && t[x].features != 0;
						break;
					case TargetCapiDrv:
					case TargetMtpxMngt:
					case TargetComDrv:
					case TargetSoftIPControl:
					case TargetSipControl:
					case TargetMTP3:
					case TargetISUP:
					case TargetDiva_IP_HMP:
						descriptor_ok = t[x].type == IDI_DRIVER && t[x].features != 0;
						break;
					case TargetMtpxAdapter:
						descriptor_ok = t[x].type == IDI_MADAPTER;
						break;
					default:
						break;
				}
				if (descriptor_ok != 0) {
					dword logical_adapter_number;

					sync_req->xdi_logical_adapter_number.Req = 0;
					sync_req->xdi_logical_adapter_number.Rc = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
					sync_req->xdi_logical_adapter_number.info.logical_adapter_number = 0;
					(*(t[x].request))((ENTITY *)sync_req);
					logical_adapter_number = sync_req->xdi_logical_adapter_number.info.logical_adapter_number;

					switch (target) {
						case TargetXdiAdapter:
						case TargetSoftIPAdapter:
						case TargetMtpxAdapter:
							if (instance_by_name != 0) {
								char* xdi_adapter_name;

								sync_req->GetName.Req     = 0;
								sync_req->GetName.Rc      = IDI_SYNC_REQ_GET_NAME;
								sync_req->GetName.name[0] = 0;
								(*(t[x].request))((ENTITY *)sync_req);
								xdi_adapter_name = (char*)&sync_req->GetName.name[0];
								if (instance_ident_length == strlen(xdi_adapter_name) &&
										memcmp (instance_ident, xdi_adapter_name, instance_ident_length) == 0) {
									request = t[x].request;
									if (target == TargetXdiAdapter || target == TargetSoftIPAdapter)
										*xdi_adapter = 1;
								}
							} else {
								if (logical_adapter_number == instance_nr) {
									request = t[x].request;
									if (target == TargetXdiAdapter || target == TargetSoftIPAdapter)
										*xdi_adapter = 1;
								}
							}
							break;
						case TargetCapiDrv:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_CAPI) {
								request = t[x].request;
							}
							break;
						case TargetMtpxMngt:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_MTPX_MGNT) {
								request = t[x].request;
							}
							break;
						case TargetComDrv:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_TTY) {
								request = t[x].request;
							}
							break;
					case TargetSoftIPControl:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_SOFTIP) {
								request = t[x].request;
							}
							break;
					case TargetSipControl:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_SIPCTRL) {
								request = t[x].request;
							}
							break;
					case TargetMTP3:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_MTP3) {
								request = t[x].request;
							}
							break;
					case TargetISUP:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_ISUP) {
								request = t[x].request;
							}
							break;
					case TargetDiva_IP_HMP:
							if (logical_adapter_number == DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_DIVA_IP_HMP) {
								request = t[x].request;
							}
							break;

						default:
							break;
					}
				}
			}
		}
		if (t != 0)
			diva_os_free (0, t);
		if (sync_req != 0)
			diva_os_free (0, sync_req);
	}

	return (request);
}

/*
	Perform hot update procedure for this variable.

	owner:    configuration owner
	instance: configuration instance
  value:    configuration variable value and information about hot plug update procedure
	*/
diva_hot_update_operation_result_t diva_update_hotplug_variable (const byte* owner,
																																 const byte* instance,
																																 const byte* variable) {
	diva_hot_update_operation_result_t ret = HotUpdateFailedNotPossible;
	const byte* info = diva_cfg_get_section_element (variable, VieManagementInfo);

	if (info != 0) {
		dword used;
		diva_vie_id_t instance_name_type;
		diva_section_target_t target;
		int xdi_adapter = 0;

		{
			const byte* management_info;
			(void)vle_to_dword (info, &used);
			used = (byte)diva_get_bs  (info+used, &management_info);

			if (used == (sizeof(DIVA_CFG_READONLY_MANAGEMENT_INFO)-1) &&
					memcmp(management_info, DIVA_CFG_READONLY_MANAGEMENT_INFO, sizeof(DIVA_CFG_READONLY_MANAGEMENT_INFO)-1) == 0) {
					return (HotUpdateOK);
			}
    }

		(void)vle_to_dword (owner, &used);
		target = (diva_section_target_t)vle_to_dword (&owner[used], &used);

		instance_name_type = (diva_vie_id_t)vle_to_dword (instance, &used);
		if (instance_name_type == VieInstance2 || instance_name_type == VieInstance) {
			dword instance_name_length = 0, logical_adapter_nr = 0;
			const byte* instance_name = 0;
			IDI_CALL request;

			if (instance_name_type == VieInstance2) {
				instance_name_length = diva_get_bs  (instance+used, &instance_name);
			} else {
				logical_adapter_nr = vle_to_dword (instance + used, &used);
			}

			request = diva_cfg_get_request_function (target,
															&xdi_adapter,
															instance_name_type == VieInstance2,
															instance_name_type == VieInstance2 ? instance_name : 0,
															instance_name_type == VieInstance2 ? instance_name_length : 0,
															instance_name_type == VieInstance2 ? 0 : logical_adapter_nr);
			if (request != 0) {
				diva_cfg_lib_management_ifc_state_t* pE =
									(diva_cfg_lib_management_ifc_state_t*)diva_os_malloc (0, sizeof(*pE));

				if (pE != 0) {
					int i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT, message_nr = 0, enum_nr = -1;
					void (*internal_request)(void* context, ENTITY* e) = 0;
					void* internal_request_context = 0;

          switch (target) {
            case TargetCFGLibDrv:
            case TargetXdiDrv:
            case TargetXdiAdapter:
            case TargetCapiDrv:
            case TargetTapiDrv:
            case TargetWmpDrv:
            case TargetComDrv:
            case TargetMAINTDrv:
            case TargetIOCTLDrv:
            case TargetMtpxMngt:
            case TargetMtpxAdapter:
            case TargetSoftIPAdapter:
              break;

            default: {
              IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));

              if (sync_req != 0) {
                sync_req->diva_get_internal_request.Req = 0;
                sync_req->diva_get_internal_request.Rc  = IDI_SYNC_REQ_GET_INTERNAL_REQUEST_PROC;
                sync_req->diva_get_internal_request.info.request_proc = 0;
                sync_req->diva_get_internal_request.info.context      = 0;
                (*request)((ENTITY*)sync_req);
                if (sync_req->diva_get_internal_request.Rc == 0xff) {
                  internal_request = sync_req->diva_get_internal_request.info.request_proc;
                  internal_request_context = sync_req->diva_get_internal_request.info.context;
                }
                diva_os_free (0, sync_req);
              }
            } break;
          }

					{
						/*
							Lock for enumeration label in the management interface inforation
							element. If found then this is necessary to locate the instance of the
							variable first.
							*/
						const byte* info = diva_cfg_get_section_element (variable, VieManagementInfo);
						const byte* name;
						dword i, info_length;

						(void)vle_to_dword (info, &i);
						info_length = (byte)diva_get_bs  (info+used, &name);

						for (i = 0; i < info_length; i++) {
							if (name[i] == '@')
								break;
						}
						if (i != 0 && i < info_length) {
							memset (pE, 0x00, sizeof(*pE));
							pE->request            = request;
							pE->internal_request   = internal_request;
							pE->internal_request_context = internal_request_context;
							pE->enum_prefix        = name;
							pE->enum_prefix_length = (byte)i;
							pE->xdi_adapter = xdi_adapter;

							if (instance_name_type == VieInstance2) {
								pE->enum_name = instance_name;
								pE->enum_data = instance_name_length;
							} else {
								pE->enum_name = 0;
								pE->enum_data = logical_adapter_nr;
							}

							diva_cfg_find_management_instance_by_enumeration(pE);

							while (pE->status == 0 && i--) {
								if (pE->message_nr != message_nr) {
									message_nr = pE->message_nr;
									i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT;
								}
								diva_os_sleep(DIVA_CFG_TIMER_RESOLUTION);
							}
							if (pE->status > 0 && pE->error == 0) {
								enum_nr = pE->final_enum_nr;
							} else if (pE->status == 0) {
								DBG_ERR(("CfgLib: management interface enum operation timeout"))
							} else {
								DBG_ERR(("CfgLib: management interface enum error at %d", pE->error))
							}
							if (enum_nr <= 0) {
								diva_os_free (0, pE);
								return (ret);
							}
						}
					}

					i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT;
					message_nr = 0;
					memset (pE, 0x00, sizeof(*pE));

					pE->request  = request;
          pE->internal_request = internal_request;
          pE->internal_request_context = internal_request_context;
					pE->variable = variable;
					pE->final_enum_nr = enum_nr;
					pE->xdi_adapter = xdi_adapter;

					diva_cfg_lib_apply_named_variables (pE);

					while (pE->status == 0 && i--) {
						if (pE->message_nr != message_nr) {
							message_nr = pE->message_nr;
							i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT;
						}
						diva_os_sleep(DIVA_CFG_TIMER_RESOLUTION);
					}

					if (pE->status == 1 && pE->error == 0) {
						ret = HotUpdateOK;
					} else if (pE->status == 0) {
						DBG_ERR(("CfgLib: management interface operation timeout"))
					} else if (pE->error != 0) {
						DBG_ERR(("CfgLib: management interface error at %d", pE->error))
					}
					diva_os_free (0, pE);
				}
			}
		}
	}

	return (ret);
}

/**
	\return true if user mode adapter
	*/
static int diva_cfg_lib_check_user_mode_adapter (IDI_CALL request /**< adapter request function */) {
	IDI_SYNC_REQ sync_req;

	sync_req.xdi_sdram_bar.Req      = 0;
	sync_req.xdi_sdram_bar.Rc       = IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR;
	sync_req.xdi_sdram_bar.info.bar = 0x00000001;
	(*request)((ENTITY*)&sync_req);
	if (sync_req.xdi_sdram_bar.info.bar == 0x00000001) {
		sync_req.xdi_sdram_bar.Req      = 0;
		sync_req.xdi_sdram_bar.Rc       = IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR;
		sync_req.xdi_sdram_bar.info.bar = 0xabcd1234;
		(*request)((ENTITY*)&sync_req);
		if (sync_req.xdi_sdram_bar.info.bar == 0xabcd1234) {
			DBG_REG(("detected user mode adapter"))
			return (1);
		}
	}

	return (0);
}

void  diva_cfg_lib_notify_adapter_insertion(void* request) {
	if (request != 0 && diva_cfg_lib_check_user_mode_adapter((IDI_CALL)request) == 0 &&
			cfg_lib != 0 && cfg_lib->storage != 0) {
		diva_cfg_lib_management_ifc_state_t* pE =
			(diva_cfg_lib_management_ifc_state_t*)diva_os_malloc (0, sizeof(*pE));

		if (pE != 0) {
			int i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT, message_nr = 0;

			memset (pE, 0x00, sizeof(*pE));
			pE->request  = (IDI_CALL)request;

			diva_cfg_lib_apply_named_variables (pE);

			while (pE->status == 0 && i--) {
				if (pE->message_nr != message_nr) {
					message_nr = pE->message_nr;
					i = DIVA_CFG_MANAGEMENT_IFC_OPERATION_TIMEOUT;
				}
				diva_os_sleep(DIVA_CFG_TIMER_RESOLUTION);
			}

			if (pE->status == 1 && pE->error == 0) {
				DBG_REG(("CfgLib: updated named variables after adapter start"))
			} else if (pE->status == 0) {
				DBG_ERR(("CfgLib: management interface operation timeout"))
			} else if (pE->error != 0) {
				DBG_ERR(("CfgLib: management interface error at %d", pE->error))
			}

			if (pE->free_instance)
				diva_os_free (0, (void*)pE->instance);

			diva_os_free (0, pE);
		}
	} else {
		DBG_ERR(("CfgLib: adapter insertion notification failed"))
	}
}

