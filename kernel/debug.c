
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

#define __DIVA_MAINT_IMPLEMENTATION__
#include "platform.h"
#include "pc.h"
#include "di_defs.h"
#include "debug_if.h"
#include "divasync.h"
#include "kst_ifc.h"
#include "maintidi.h"
#include "man_defs.h"
#include "cfg_types.h"
#include "cfg_notify.h"

/*
  LOCALS
  */
#define DBG_MAGIC (0x47114711L)
#define MAX_TMP_BUFFER_STRING 4096

static void DI_register (void *arg);
static void DEBUGLIB_CALLING_CONVENTION DI_deregister (pDbgHandle hDbg);
static void DI_format (int do_lock, word id, int type, char *format, va_list argument_list);
static void DEBUGLIB_CALLING_CONVENTION DI_format_locked   (word id, int type, char *format, va_list argument_list);
static void DEBUGLIB_CALLING_CONVENTION DI_format_old (word id, char *format, va_list ap) { }
static void DEBUGLIB_CALLING_CONVENTION DiProcessEventLog (unsigned short id, unsigned long msgID, va_list ap) { }
static void single_p (byte * P, word * PLength, byte Id);
static void diva_maint_xdi_cb (ENTITY* e);
static word SuperTraceCreateReadReq (byte* P, const char* path);
static int diva_mnt_cmp_nmbr (const char* nmbr);
static void diva_free_dma_descriptor (IDI_CALL request, int nr);
static int diva_get_dma_descriptor (IDI_CALL request, dword *dma_magic);
static void _diva_set_driver_dbg_mask (const byte* name, dword name_length, dword mask);
void diva_mnt_internal_dprintf (dword drv_id, dword type, char* p, ...);

static dword MaxDumpSize = 4*1024;
static dword MaxXlogSize = 4*1024;
static char  TraceFilter[DIVA_MAX_SELECTIVE_FILTER_LENGTH+1];
static int TraceFilterIdent   = -1;
static int TraceFilterChannel = -1;
static char* maint_tmp_buffer_base;
static volatile int maint_tmp_buffer_length;
static int diva_core_saved;
static IDI_CALL xdi_adapter_request; /* Request function of adapter used to save debug/trace buffer in the external memory */
static int discard_maint_idi_trace = 1;

#define DIVA_SYSTEM_INFO_MESSAGE "System message in interrupt context"

typedef struct _diva_maint_client {
  dword       sec;
  dword       usec;
  pDbgHandle  hDbg;
  char        drvName[128];
  dword       dbgMask;
  dword       last_dbgMask;
  IDI_CALL    request;
  _DbgHandle_ Dbg;
  int         logical;
  int         channels;
  diva_strace_library_interface_t* pIdiLib;
  BUFFERS     XData;
  char        xbuffer[2048+512];
  byte*       pmem;
  int         request_pending;
  int         dma_handle;
  int         adapter_type;
} diva_maint_client_t;
static diva_maint_client_t clients[MAX_DESCRIPTORS];

static void diva_change_management_debug_mask (diva_maint_client_t* pC, dword old_mask);

static void diva_maint_error (void* user_context,
                              diva_strace_library_interface_t* hLib,
                              int Adapter,
                              int error,
                              const char* file,
                              int line);
static void diva_maint_state_change_notify (void* user_context,
                                            diva_strace_library_interface_t* hLib,
                                            int Adapter,
                                            diva_trace_line_state_t* channel,
                                            int notify_subject);
static void diva_maint_trace_notify (void* user_context,
                                     diva_strace_library_interface_t* hLib,
                                     int Adapter,
                                     void* xlog_buffer,
                                     int length);



typedef struct MSG_QUEUE {
	dword	Size;		/* total size of queue (constant)	*/
	byte	*Base;		/* lowest address (constant)		*/
	byte	*High;		/* Base + Size (constant)		*/
	byte	*Head;		/* first message in queue (if any)	*/
	byte	*Tail;		/* first free position			*/
	byte	*Wrap;		/* current wraparound position 		*/
	dword	Count;		/* current no of bytes in queue		*/
} MSG_QUEUE;

typedef struct MSG_HEAD {
	volatile dword	Size;		/* size of data following MSG_HEAD	*/
#define	MSG_INCOMPLETE	0x8000	/* ored to Size until queueCompleteMsg 	*/
} MSG_HEAD;

#define queueCompleteMsg(p) do{ ((MSG_HEAD *)p - 1)->Size &= ~MSG_INCOMPLETE; }while(0)
#define queueCount(q)	((q)->Count)
#define MSG_NEED(size) \
	( (sizeof(MSG_HEAD) + size + sizeof(dword) - 1) & ~(sizeof(dword) - 1) )

static void queueInit (MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer) {
	Q->Size = sizeBuffer;
	Q->Base = Q->Head = Q->Tail = Buffer;
	Q->High = Buffer + sizeBuffer;
	Q->Wrap = 0;
	Q->Count= 0;
}

static byte *queueAllocMsg (MSG_QUEUE *Q, word size) {
	/* Allocate 'size' bytes at tail of queue which will be filled later
   * directly whith callers own message header info and/or message.
   * An 'alloced' message is marked incomplete by oring the 'Size' field
   * whith MSG_INCOMPLETE.
   * This must be reset via queueCompleteMsg() after the message is filled.
   * As long as a message is marked incomplete queuePeekMsg() will return
   * a 'queue empty' condition when it reaches such a message.  */

	MSG_HEAD *Msg;
	word need = MSG_NEED(size);

	if (Q->Tail == Q->Head) {
		if (Q->Wrap || need > Q->Size) {
			return(0); /* full */
		}
		goto alloc; /* empty */
	}

	if (Q->Tail > Q->Head) {
		if (Q->Tail + need <= Q->High) goto alloc; /* append */
		if (Q->Base + need > Q->Head) {
			return (0); /* too much */
		}
		/* wraparound the queue (but not the message) */
		Q->Wrap = Q->Tail;
		Q->Tail = Q->Base;
		goto alloc;
	}

	if (Q->Tail + need > Q->Head) {
		return (0); /* too much */
	}

alloc:
	Msg = (MSG_HEAD *)Q->Tail;

	Msg->Size = size | MSG_INCOMPLETE;

	Q->Tail	 += need;
	Q->Count += size;



	return ((byte*)(Msg + 1));
}

static void queueFreeMsg (MSG_QUEUE *Q) {
/* Free the message at head of queue */

	word size = ((MSG_HEAD *)Q->Head)->Size & ~MSG_INCOMPLETE;

	Q->Head  += MSG_NEED(size);
	Q->Count -= size;

	if (Q->Wrap) {
		if (Q->Head >= Q->Wrap) {
			Q->Head = Q->Base;
			Q->Wrap = 0;
		}
	} else if (Q->Head >= Q->Tail) {
		Q->Head = Q->Tail = Q->Base;
	}
}

static byte *queuePeekMsg (MSG_QUEUE *Q, word *size) {
	/* Show the first valid message in queue BUT DONT free the message.
   * After looking on the message contents it can be freed queueFreeMsg()
   * or simply remain in message queue.  */

	MSG_HEAD *Msg = (MSG_HEAD *)Q->Head;

	if (((byte *)Msg == Q->Tail && !Q->Wrap) ||
	    (Msg->Size & MSG_INCOMPLETE)) {
		return (0);
	} else {
		*size = (word)(Msg->Size);
		return ((byte *)(Msg + 1));
	}
}

/*
  Message queue header
  */
static MSG_QUEUE*          dbg_queue = 0;
static byte*               dbg_base  = 0;
static dword               dbg_base_length = 0;
static byte*               initial_dbg_base  = 0;
static dword               initial_dbg_base_length = 0;
static int                 external_dbg_queue = 0;
static diva_os_spin_lock_t dbg_q_lock;
static diva_os_spin_lock_t dbg_adapter_lock;
static diva_os_spin_lock_t timestamp_lock;
static int                 dbg_q_busy = 0;
static volatile dword      dbg_sequence = 0;
static dword               start_sec;
static dword               start_usec;

/*
	CfgLib
	*/
static diva_cfg_lib_access_ifc_t* cfg_lib_ifc;
static void* diva_cfg_lib_notification_handle;
static IDI_CALL dadapter_request;
static int diva_maint_init_complete;

typedef struct _diva_maint_dbg_mask_cfg {
	byte  name[24]; /* driver name */
	dword value;
	dword name_length;
} diva_maint_dbg_mask_cfg_t;

#define DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE "dbg\\"
#define DIVA_MAINT_DRIVER_MANAGEMENT_IDI_TRACE "management_idi_trace"
static diva_maint_dbg_mask_cfg_t dbg_info[32];

#define DIVA_MAINT_DRIVER_TIMESTAMP_ADAPTER "timestampAdapter"
static int timestampAdapter = -1;
static IDI_CALL timestampAdapterRequest;
static volatile dword* adapterTimestamp;

static void diva_os_get_timestamp (dword* sec, dword* usec) {

	if (diva_maint_init_complete) {
		diva_os_spin_lock_magic_t old_irql;

		diva_os_enter_spin_lock (&timestamp_lock, &old_irql, "timestamp");
		if (adapterTimestamp != 0) {
			dword timestamp = *adapterTimestamp;
			diva_os_leave_spin_lock (&timestamp_lock, &old_irql, "timestamp");
			*sec  = timestamp / 500;
			*usec = (timestamp % 500)*2000;;
			return;
		}
		diva_os_leave_spin_lock (&timestamp_lock, &old_irql, "timestamp");
	}

	diva_os_get_time (sec, usec);
}

static diva_maint_dbg_mask_cfg_t* get_dbg_info (const byte* name, dword name_length) {
	dword i = 0;

	if (name_length == 0)
		name_length = (dword)strlen((char*)name);

	if ((name_length = MIN(sizeof(dbg_info[0].name), name_length)) != 0) {
		for (i = 0; i < sizeof(dbg_info)/sizeof(dbg_info[0]); i++) {
			if (dbg_info[i].name_length == name_length &&
					memcmp (dbg_info[i].name, name, name_length) == 0) {
				return (&dbg_info[i]);
			}
		}
	}

	return (0);
}

static diva_maint_dbg_mask_cfg_t* get_dbg_info_from_client_name (const byte* name) {
	dword i, name_length = (dword)strlen((char*)name);

	if (name_length != 0) {
		for (i = 0; i < sizeof(dbg_info)/sizeof(dbg_info[0]); i++) {
			if (dbg_info[i].name_length != 0 && dbg_info[i].name_length <= name_length &&
				memcmp (dbg_info[i].name, name, dbg_info[i].name_length) == 0) {

				return (&dbg_info[i]);
			}
		}
	}

	return (0);
}

static diva_maint_dbg_mask_cfg_t* get_free_dbg_info (const byte* name, dword name_length) {
	dword i = 0;

	if (name_length == 0)
		name_length = (dword)strlen((char*)name);

	if ((name_length = MIN(sizeof(dbg_info[0].name), name_length)) != 0) {
		for (i = 0; i < sizeof(dbg_info)/sizeof(dbg_info[0]); i++) {
			if (dbg_info[i].name_length == 0) {
				memcpy (dbg_info[i].name, name, name_length);
				dbg_info[i].name_length = name_length;

				return (&dbg_info[i]);
			}
		}
	}

	return (0);
}

static int diva_maint_parse_variable_name (const byte* variable,
																					 const byte* template,
																					 dword template_length,
																					 pcbyte* key,
																					 dword*  key_length,
																					 pcbyte* name,
																					 dword*  name_length) {
	const byte* variable_name;
	dword variable_name_length;

	if (!cfg_lib_ifc || 
			(*(cfg_lib_ifc->diva_cfg_lib_get_name_ident_proc))(variable,
																												 &variable_name,
																												 &variable_name_length) != DivaCfgLibOK) {
		return (-1);
	}

  if (variable_name_length > template_length &&
      (template_length == 0 || memcmp (variable_name, template, template_length) == 0)) {
    const byte* p = variable_name + template_length;
    dword len;

    if (*p != '\\') {
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
          return (0);
        }
      }
    }
  }

	return (-1);
}

static void register_timestamp (IDI_CALL request) {
	diva_os_spin_lock_magic_t old_irql;
	int registered;

	diva_os_enter_spin_lock (&timestamp_lock, &old_irql, "timestamp");
	registered = adapterTimestamp != 0;
	diva_os_leave_spin_lock (&timestamp_lock, &old_irql, "timestamp");
	if (registered != 0)
		return;

	if (request == 0) {
		DESCRIPTOR*   t        = diva_os_malloc (0, sizeof(*t)*MAX_DESCRIPTORS);
		IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));

		if (t != 0 && sync_req != 0) {
			int x;

			memset (t, 0x00, sizeof(*t)*MAX_DESCRIPTORS);
			memset (sync_req, 0x00, sizeof(*sync_req));
			diva_os_read_descriptor_array (t, sizeof(*t)*MAX_DESCRIPTORS);

			for (x = 0; x < MAX_DESCRIPTORS; x++) {
				if (t[x].channels && (t[x].type < IDI_VADAPTER) && t[x].features &&
						t[x].request && (t[x].type != IDI_MADAPTER)) {
					sync_req->xdi_logical_adapter_number.Req = 0;
					sync_req->xdi_logical_adapter_number.Rc = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
					sync_req->xdi_logical_adapter_number.info.logical_adapter_number = 0;
					(*(t[x].request))((ENTITY *)sync_req);
					if (sync_req->xdi_logical_adapter_number.info.logical_adapter_number == (dword)timestampAdapter) {
						request = t[x].request;
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

	if (request != 0) {
		IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));

		if (sync_req != 0) {
			dword* tmp = 0;

			sync_req->diva_xdi_clock_control_command_req.Req = 0;
			sync_req->diva_xdi_clock_control_command_req.Rc  = IDI_SYNC_REQ_XDI_CLOCK_CONTROL;
			memset (&sync_req->diva_xdi_clock_control_command_req.info,
							0x00,
							sizeof(sync_req->diva_xdi_clock_control_command_req.info));
			sync_req->diva_xdi_clock_control_command_req.info.command                      = 2;
			sync_req->diva_xdi_clock_control_command_req.info.data.on.port                 = 0;
			sync_req->diva_xdi_clock_control_command_req.info.data.on.isr_callback         = 0;
			sync_req->diva_xdi_clock_control_command_req.info.data.on.isr_callback_context = &tmp;

			(*request)((ENTITY*)sync_req);
			diva_os_enter_spin_lock (&timestamp_lock, &old_irql, "timestamp");
			adapterTimestamp = tmp;
			diva_os_leave_spin_lock (&timestamp_lock, &old_irql, "timestamp");

			diva_os_free (0, sync_req);
		}
	}
}

static void unregister_timestamp (void) {
	diva_os_spin_lock_magic_t old_irql;
	timestampAdapterRequest = 0;
	diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql, "timestamp");
	adapterTimestamp = 0;
	diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql, "timestamp");
}

/*
	Configuration is passed using simple instance and following structure:
	dbg/driver name/value
	Variable: dword
	*/
static int diva_maint_cfg_changed (void* context, const byte* message, const byte* instance) {
	diva_os_spin_lock_magic_t old_irql;
	int init_complete = diva_maint_init_complete;

	if (!cfg_lib_ifc) 
		return -1;

	if (init_complete != 0) {
		diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "cfg_changed");
	}

	if (message == 0 && instance != 0) {
		const byte* var, *key, *name;
		dword key_length, name_length;
		dword i = 0;

		for (i = 0; i < sizeof(dbg_info)/sizeof(dbg_info[0]); i++) {
			dbg_info[i].name_length = 0;
		}

		var = (*(cfg_lib_ifc->diva_cfg_storage_find_variable_proc))
                                              (cfg_lib_ifc, instance,
																							(byte*)DIVA_MAINT_DRIVER_MANAGEMENT_IDI_TRACE,
																							sizeof(DIVA_MAINT_DRIVER_MANAGEMENT_IDI_TRACE)-1);
		if (var != 0) {
			byte v;

			if ((*(cfg_lib_ifc->diva_cfg_lib_read_byte_value_proc))(var, &v) == DivaCfgLibOK) {
				discard_maint_idi_trace = (v != 0);
			}
		}

		var = (*(cfg_lib_ifc->diva_cfg_storage_find_variable_proc))
                                              (cfg_lib_ifc, instance,
																							(byte*)DIVA_MAINT_DRIVER_TIMESTAMP_ADAPTER,
																							sizeof(DIVA_MAINT_DRIVER_TIMESTAMP_ADAPTER)-1);
		if (var != 0) {
			if ((*(cfg_lib_ifc->diva_cfg_lib_read_int_value_proc))(var, &timestampAdapter) == DivaCfgLibOK) {
				if (init_complete) {
					register_timestamp(0);
				}
			} else {
				timestampAdapter = -1;
				if (init_complete) {
					unregister_timestamp ();
				}
			}
		}



		var = 0;
		while ((var = (*(cfg_lib_ifc->diva_cfg_storage_enum_variable_proc))(cfg_lib_ifc,
                                        instance, var,
                                        DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE,
                                        sizeof(DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE)-1,
                                        &key, &key_length,
                                        &name, &name_length)) != 0) {
			if (name_length == sizeof("value")-1 && memcmp (name, "value", sizeof("value")-1) == 0) {
				diva_maint_dbg_mask_cfg_t* info = get_dbg_info (key, (byte)key_length);

				if (info == 0) {
					info = get_free_dbg_info (key, (byte)key_length);
				}
				if (info != 0) {
					if ((*(cfg_lib_ifc->diva_cfg_lib_read_dword_value_proc))(var, &info->value) == DivaCfgLibOK) {
						info->value &= 0x7fffffff;
					} else {
						info->value = 0x197;
					}
				}
			}
		}
	} else if (message != 0 && instance == 0) {
		diva_cfg_lib_notification_context_t* message_info = (diva_cfg_lib_notification_context_t*)message;

		if (message_info->notification_type == DivaCfgVariableChanged) {
      const byte* name, *key = 0;
      dword name_length, key_length;

			{
				const byte* variable_name;
				dword variable_name_length;
				byte v;

				if ((*(cfg_lib_ifc->diva_cfg_lib_get_name_ident_proc))(
																										message_info->info.variable_changed.variable,
																										&variable_name,
																										&variable_name_length) == DivaCfgLibOK) {
					if (variable_name_length == sizeof(DIVA_MAINT_DRIVER_MANAGEMENT_IDI_TRACE)-1 &&
							memcmp (variable_name, DIVA_MAINT_DRIVER_MANAGEMENT_IDI_TRACE, variable_name_length) == 0) {
						if ((*(cfg_lib_ifc->diva_cfg_lib_read_byte_value_proc))(
										message_info->info.variable_changed.variable, &v) == DivaCfgLibOK) {
							discard_maint_idi_trace = (v != 0);
						}
					}
				}
			}

			if (diva_maint_parse_variable_name (message_info->info.variable_changed.variable,
																					DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE,
																					sizeof(DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE)-1,
																					&key,
																					&key_length,
																					&name,
																					&name_length) == 0 &&
					name_length == sizeof("value")-1 && memcmp (name, "value", sizeof("value")-1) == 0) {
				diva_maint_dbg_mask_cfg_t* info = get_dbg_info (key, (byte)key_length);

				if (info == 0) {
					info = get_free_dbg_info (key, (byte)key_length);
				}
				if (info != 0) {
					if ((*(cfg_lib_ifc->diva_cfg_lib_read_dword_value_proc))(
											message_info->info.variable_changed.variable, &info->value) == DivaCfgLibOK) {
						info->value &= 0x7fffffff;
					} else {
						info->value = 0x197;
					}
				}
			}
		} else if (message_info->notification_type == DivaCfgVariableRemoved) {
      const byte* name, *key = 0;
      dword name_length, key_length;
      if (diva_maint_parse_variable_name (message_info->info.variable_changed.variable,
                                          DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE,
                                          sizeof(DIVA_MAINT_DRIVER_DEBUG_MASK_TEMPLATE)-1,
                                          &key,
                                          &key_length,
                                          &name,
                                          &name_length) == 0 &&
					name_length == sizeof("value")-1 && memcmp (name, "value", sizeof("value")-1) == 0) {
        diva_maint_dbg_mask_cfg_t* info = get_dbg_info (key, (byte)key_length);

				if (info != 0) {
					info->name_length = 0;
				}
			}
		}
	}

	if (init_complete != 0) {
		dword i = 0;

		diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "cfg_changed");

		for (i = 0; i < sizeof(dbg_info)/sizeof(dbg_info[0]); i++) {
			if (dbg_info[i].name_length != 0) {
				_diva_set_driver_dbg_mask (dbg_info[i].name, dbg_info[i].name_length, dbg_info[i].value);
			}
		}
	}

	return (0);
}

void diva_maint_connect_to_cfg_lib (void* _dadapter) {
	IDI_CALL dadapter = (IDI_CALL)_dadapter;

	if (cfg_lib_ifc == 0 && dadapter != 0) {
		IDI_SYNC_REQ* sync_req = (IDI_SYNC_REQ*)diva_os_malloc (0, sizeof(*sync_req));
		if (!sync_req)
			return;

		dadapter_request = dadapter;

		memset (sync_req, 0x00, sizeof(*sync_req));
		sync_req->didd_get_cfg_lib_ifc.e.Rc = IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC;
		(*dadapter_request)((ENTITY*)sync_req);
		if (sync_req->didd_get_cfg_lib_ifc.e.Rc == 0xff) {
			if ((cfg_lib_ifc = sync_req->didd_get_cfg_lib_ifc.info.ifc) != 0) {

				/*
					Register notification callback
					*/
				diva_cfg_lib_notification_handle =
              (*(cfg_lib_ifc->diva_cfg_lib_cfg_register_notify_callback_proc))(
                                                          diva_maint_cfg_changed,
                                                          &dadapter_request,
                                                          TargetMAINTDrv);
			}
		}
		diva_os_free(0, sync_req);
	}
}

void diva_maint_disconnect_cfg_lib (void) {
  if (cfg_lib_ifc != 0) {
    if (diva_cfg_lib_notification_handle != 0) {
      (*(cfg_lib_ifc->diva_cfg_lib_cfg_remove_notify_callback_proc))(diva_cfg_lib_notification_handle);
      diva_cfg_lib_notification_handle = 0;
    }
    (*(cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(cfg_lib_ifc);
    cfg_lib_ifc = 0;
  }

	dadapter_request = 0;
}

/*
	INTERFACE:
    Initialize run time queue structures.
    base:    base of the message queue
    length:  length of the message queue
    do_init: perfor queue reset
    init_runtime: init OS objects

    return:  zero on sucess, -1 on error
  */
static int diva_maint_internal_init (byte* base, unsigned long length, int do_init, int init_runtime) {
  if ((dbg_queue != 0 && init_runtime != 0) || base == 0  || length < (4096*4)) {
    return (-1);
  }

  if (init_runtime != 0) {
    TraceFilter[0]     =  0;
    TraceFilterIdent   = -1;
    TraceFilterChannel = -1;
  }

  dbg_base = base;
  dbg_base_length = (dword)length;

  if (init_runtime != 0)
    diva_os_get_timestamp (&start_sec, &start_usec);

  *(dword*)base  = (dword)DBG_MAGIC; /* Store Magic */
  base   += sizeof(dword);
  length -= sizeof(dword);

  *(dword*)base = 2048; /* Extension Field Length */
  base   += sizeof(dword);
  length -= sizeof(dword);

  strcpy (base, "KERNEL MODE BUFFER\n");
  base   += 2048;
  length -= 2048;

	*(dword*)base = MAX_TMP_BUFFER_STRING; /* Temporary buffer */
  base   += sizeof(dword);
  length -= sizeof(dword);
	*base   = 0; /* Clean buffer */
	maint_tmp_buffer_base = (char*)base;
	base   += MAX_TMP_BUFFER_STRING;
	length -= MAX_TMP_BUFFER_STRING;

  *(dword*)base = 0; /* Terminate extension */
  base   += sizeof(dword);
  length -= sizeof(dword);

  *(void**)base  =  (void*)(base+sizeof(void*)); /* Store Base  */
  base   += sizeof(void*);
  length -= sizeof(void*);

  dbg_queue = (MSG_QUEUE*)base;
  queueInit (dbg_queue, base + sizeof(MSG_QUEUE), length - sizeof(MSG_QUEUE) - 512);
  external_dbg_queue = 0;

  if (!do_init) {
    external_dbg_queue = 1; /* memory was located on the external device */
  }

  if (init_runtime != 0) {
    if (diva_os_initialize_spin_lock (&dbg_q_lock, "dbg_init")) {
      dbg_queue = 0;
      dbg_base = 0;
      external_dbg_queue = 0;
      return (-1);
    }
    if (diva_os_initialize_spin_lock (&dbg_adapter_lock, "dbg_init")) {
      diva_os_destroy_spin_lock(&dbg_q_lock, "dbg_init");
      dbg_queue = 0;
      dbg_base = 0;
      external_dbg_queue = 0;
      return (-1);
		}
		if (diva_os_initialize_spin_lock (&timestamp_lock, "dbg_init")) {
			diva_os_destroy_spin_lock(&dbg_q_lock, "dbg_init");
      diva_os_destroy_spin_lock(&dbg_adapter_lock, "dbg_init");
			dbg_queue = 0;
			dbg_base = 0;
			external_dbg_queue = 0;
			return (-1);
    }
  }

  return (0);
}

/*
	INTERFACE:
    Initialize run time queue structures.
    base:    base of the message queue
    length:  length of the message queue
    do_init: perfor queue reset

    return:  zero on sucess, -1 on error
  */
int diva_maint_init (byte* base, unsigned long length, int do_init) {
  return (diva_maint_internal_init (base, length, do_init, 1));
}

/*
	INTERFACE:
    Initialize run time queue structures.
    base:    base of the message queue
    length:  length of the message queue
    do_init: perfor queue reset

    return:  zero on sucess, -1 on error
  */
int diva_maint_init_and_connect_to_cfg_lib (byte* base, unsigned long length, int do_init, void* dadapter) {
	int ret;

	diva_maint_connect_to_cfg_lib (dadapter);

	ret = diva_maint_internal_init (base, length, do_init, 1);

	if (ret == 0) {
		diva_maint_init_complete = 1;
		if (timestampAdapter >= 0)
			register_timestamp (0);
	} else {
		diva_maint_disconnect_cfg_lib ();
	}

	return (ret);
}

/*
  INTERFACE:
    Finit at unload time
    return address of internal queue or zero if queue
    was external
  */
void* diva_maint_finit (void) {
  void* ret = (void*)dbg_base;
  int i;

	diva_maint_init_complete = 0;
	diva_maint_disconnect_cfg_lib ();

	maint_tmp_buffer_length = 0;
  maint_tmp_buffer_base = 0;
  dbg_queue = 0;
  dbg_base  = 0;

  if (ret) {
    diva_os_destroy_spin_lock(&dbg_q_lock, "dbg_finit");
    diva_os_destroy_spin_lock(&dbg_adapter_lock, "dbg_finit");
    diva_os_destroy_spin_lock(&timestamp_lock, "dbg_finit");
  }

  if (external_dbg_queue) {
    ret = 0;
  }
  external_dbg_queue = 0;

	if (initial_dbg_base != 0) {
		ret = initial_dbg_base;
	}

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].pmem) {
      diva_os_free (0, clients[i].pmem);
    }
  }

  return (ret);
}

/*
  INTERFACE:
    Return amount of messages in debug queue
  */
dword diva_dbg_q_length (void) {
	return (dbg_queue ? queueCount(dbg_queue)	: 0);
}

/*
  INTERFACE:
    Lock messafe queue and return the pointer to the first
    entry.
  */
diva_dbg_entry_head_t* diva_maint_get_message (word* size,
                                               diva_os_spin_lock_magic_t* old_irql) {
  diva_dbg_entry_head_t*     pmsg = 0;

  diva_os_enter_spin_lock (&dbg_q_lock, old_irql, "read");
  if (dbg_q_busy) {
    diva_os_leave_spin_lock (&dbg_q_lock, old_irql, "read_busy");
    return (0);
  }
  dbg_q_busy = 1;

  if (!(pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, size))) {
    dbg_q_busy = 0;
    diva_os_leave_spin_lock (&dbg_q_lock, old_irql, "read_empty");
  }

  return (pmsg);
}

/*
  INTERFACE:
    acknowledge last message and unlock queue
  */
void diva_maint_ack_message (int do_release,
                             diva_os_spin_lock_magic_t* old_irql) {
	if (!dbg_q_busy) {
		return;
	}
	if (do_release) {
		queueFreeMsg (dbg_queue);
	}
	dbg_q_busy = 0;
  diva_os_leave_spin_lock (&dbg_q_lock, old_irql, "read_ack");
}


/*
  INTERFACE:
    PRT COMP function used to register
    with MAINT adapter or log in compatibility
    mode in case older driver version is connected too
  */
void diva_maint_prtComp (char *format, ...) {
  void    *hDbg;
  va_list ap;

  if (!format)
    return;

  va_start(ap, format);

  /*
    register to new log driver functions
   */
  if ((format[0] == 0) && ((unsigned char)format[1] == 255)) {
    hDbg = va_arg(ap, void *); /* ptr to DbgHandle */
    DI_register (hDbg);
  }

  va_end (ap);
}

static void DI_register (void *arg) {
  diva_os_spin_lock_magic_t old_irql;
  dword sec, usec;
  pDbgHandle  	hDbg ;
  int id, free_id = -1, best_id = 0;

  diva_os_get_timestamp (&sec, &usec);

	hDbg = (pDbgHandle)arg ;
  /*
    Check for bad args, specially for the old obsolete debug handle
    */
  if ((hDbg == NULL) ||
      ((hDbg->id == 0) && (((_OldDbgHandle_ *)hDbg)->id == -1)) ||
      (hDbg->Registered != 0)) {
		return ;
  }

  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "register");

  for (id = 1; id < (sizeof(clients)/sizeof(clients[0])); id++) {
    if (clients[id].hDbg == hDbg) {
      /*
        driver already registered
        */
      diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");
      return;
    }
    if (clients[id].hDbg) { /* slot is busy */
      continue;
    }
    free_id = id;
    if (!strcmp (clients[id].drvName, hDbg->drvName)) {
      /*
        This driver was already registered with this name
        and slot is still free - reuse it
        */
      best_id = 1;
      break;
    }
    if (!clients[id].hDbg) { /* slot is busy */
      break;
    }
  }

  if (free_id != -1) {
		diva_maint_dbg_mask_cfg_t* info = get_dbg_info_from_client_name (hDbg->drvName);
    diva_dbg_entry_head_t* pmsg = 0;
    int len;
    char tmp[256];
    word size;

		if (info != 0) {
			hDbg->dbgMask = info->value;
		}

    /*
      Register new driver with id == free_id
      */
    clients[free_id].hDbg = hDbg;
    clients[free_id].sec  = sec;
    clients[free_id].usec = usec;
    strcpy (clients[free_id].drvName, hDbg->drvName);

    clients[free_id].dbgMask = hDbg->dbgMask;
    if (best_id) {
				hDbg->dbgMask |= clients[free_id].last_dbgMask;
    } else {
      clients[free_id].last_dbgMask = 0;
    }

    hDbg->Registered = DBG_HANDLE_REG_NEW ;
    hDbg->id         = (byte)free_id;
    hDbg->dbg_end    = DI_deregister;
    hDbg->dbg_prt    = DI_format_locked;
    hDbg->dbg_ev     = DiProcessEventLog;
    hDbg->dbg_irq    = DI_format_locked;
    if (hDbg->Version > 0) {
      hDbg->dbg_old  = DI_format_old;
    }
    hDbg->next       = (pDbgHandle)DBG_MAGIC;

    /*
      Log driver register, MAINT driver ID is '0'
      */
    len = sprintf (tmp, "DIMAINT - drv # %d = '%s' registered",
                   free_id, hDbg->drvName);

    while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                        (word)(len+1+sizeof(*pmsg))))) {
      if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
        queueFreeMsg (dbg_queue);
      } else {
        break;
      }
    }

    if (pmsg) {
      pmsg->sequence    = dbg_sequence++;
      pmsg->time_sec    = sec;
      pmsg->time_usec   = usec;
      pmsg->facility    = MSG_TYPE_STRING;
      pmsg->dli         = DLI_LOG;
      pmsg->drv_id      = 0; /* id 0 - DIMAINT */
      pmsg->di_cpu      = 0;
      pmsg->data_length = len+1;

      memcpy (&pmsg[1], tmp, len+1);
		  queueCompleteMsg (pmsg);
      diva_maint_wakeup_read();
    }
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");
}

static void DEBUGLIB_CALLING_CONVENTION
DI_deregister (pDbgHandle hDbg) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  dword sec, usec;
  int i;
  word size;
  byte* pmem = 0;

  diva_os_get_timestamp (&sec, &usec);

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "read");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "read");

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].hDbg == hDbg) {
      diva_dbg_entry_head_t* pmsg;
      char tmp[256];
      int len;

      clients[i].hDbg = 0;

      hDbg->id       = -1;
      hDbg->dbgMask  = 0;
      hDbg->dbg_end  = 0;
      hDbg->dbg_prt  = 0;
      hDbg->dbg_irq  = 0;
      if (hDbg->Version > 0)
        hDbg->dbg_old = 0;
      hDbg->Registered = 0;
      hDbg->next     = 0;

      if (clients[i].pIdiLib) {
        (*(clients[i].pIdiLib->DivaSTraceLibraryFinit))(clients[i].pIdiLib->hLib);
        clients[i].pIdiLib = 0;

        pmem = clients[i].pmem;
        clients[i].pmem = 0;
      }

      /*
        Log driver register, MAINT driver ID is '0'
        */
      len = sprintf (tmp, "DIMAINT - drv # %d = '%s' de-registered",
                     i, hDbg->drvName);

      while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                        (word)(len+1+sizeof(*pmsg))))) {
        if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
          queueFreeMsg (dbg_queue);
        } else {
          break;
        }
      }

      if (pmsg) {
        pmsg->sequence    = dbg_sequence++;
        pmsg->time_sec    = sec;
        pmsg->time_usec   = usec;
        pmsg->facility    = MSG_TYPE_STRING;
        pmsg->dli         = DLI_LOG;
        pmsg->drv_id      = 0; /* id 0 - DIMAINT */
        pmsg->di_cpu      = 0;
        pmsg->data_length = len+1;

        memcpy (&pmsg[1], tmp, len+1);
  		  queueCompleteMsg (pmsg);
        diva_maint_wakeup_read();
      }

      break;
    }
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "read_ack");
  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "read_ack");

  if (pmem) {
    diva_os_free (0, pmem);
  }
}

static void DEBUGLIB_CALLING_CONVENTION DI_format_locked (unsigned short id,
                       int type,
                       char *format,
                       va_list argument_list) {
  DI_format (1, id, type, format, argument_list);
}


/*
	Used internally to insert string messages in the existing stream of data

	Returns amount of bytes written
	*/
static int diva_mnt_insert_message (dword sec, dword usec, const char* message, int message_length) {
	diva_dbg_entry_head_t* pmsg;
	dword length;
	word size;

	if (message_length == 0)
		message_length = strlen(message);

	message_length = MIN(MSG_FRAME_MAX_SIZE,message_length);

	length = (dword)(message_length + sizeof(*pmsg) + 1);

	while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                                        (word)length))) {
		if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
			queueFreeMsg (dbg_queue);
		} else {
			break;
		}
	}
	if ( !pmsg ) {
		/* Something is lost */
		return ( 0 ) ;
	}

	pmsg->sequence    = dbg_sequence++;
	pmsg->time_sec    = sec;
	pmsg->time_usec   = usec;
	pmsg->facility    = MSG_TYPE_STRING;
	pmsg->dli         = DLI_FTL;
	pmsg->drv_id      = 0x1fffffff; /* system message id */
	pmsg->di_cpu      = 0;
	pmsg->data_length = length - sizeof(*pmsg);

	memcpy (&pmsg[1], message, pmsg->data_length - 1);

	((char*)&pmsg[1])[pmsg->data_length] = 0;

	queueCompleteMsg (pmsg);

	return (message_length);
}

static void diva_maint_recovery_tmp_buffer (dword sec, dword usec) {
  if (maint_tmp_buffer_length != 0) {
		int length;

		do {
			length = maint_tmp_buffer_length;
			diva_mnt_insert_message (sec, usec, "System message in interrupt context", 0);
			diva_mnt_insert_message (sec, usec, maint_tmp_buffer_base, length);
			diva_mnt_insert_message (sec, usec, "-----------------------------------", 0);
		} while (length != maint_tmp_buffer_length);

		maint_tmp_buffer_length = 0;
  }
}

#if 1
void
DI_append(unsigned short id, int type, int facility, char *log, unsigned long length) {
	diva_os_spin_lock_magic_t old_irql ;
	dword                     sec, usec ;
	diva_dbg_entry_head_t    *pmsg = 0;
	word                      size ;

	diva_os_get_timestamp (&sec, &usec);
    diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "append");

	while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
	                                                       (word)length+sizeof(*pmsg)))) {
		if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
			queueFreeMsg (dbg_queue) ;
		} else {
			break ;
		}
	}
	if ( pmsg ) { ;
		pmsg->sequence    = dbg_sequence++ ;
		pmsg->time_sec    = sec ;
		pmsg->time_usec   = usec ;
		pmsg->facility    = facility ;
		pmsg->dli         = type ; /* DLI_XXX */
		pmsg->drv_id      = id ;   /* driver MAINT id */
		pmsg->di_cpu      = 0 ;
		pmsg->data_length = length ;

		memcpy (&pmsg[1], log, pmsg->data_length);
		queueCompleteMsg (pmsg);
	}

	if (queueCount(dbg_queue)) {
		diva_maint_wakeup_read();
	}

	diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "append");

}
#endif

static void DI_format (int do_lock,
                       unsigned short id,
                       int type,
                       char *format,
                       va_list ap) {
  diva_os_spin_lock_magic_t old_irql;
  dword sec, usec;
  diva_dbg_entry_head_t* pmsg = 0;
  dword length;
  word size;
  static char fmtBuf[MSG_FRAME_MAX_SIZE+sizeof(*pmsg)+1];
  char          *data;
  unsigned short code;

  if ( diva_os_in_irq() || (diva_os_spinlocks_available() == 0) ) {
		const char* info;

		if (format) {
			if (*format) {
				info = format;
			} else {
				info = "binary trace message";
			}
		} else {
			info = "0 trace message";
		}

		if (maint_tmp_buffer_length == 0) {
			memcpy (&maint_tmp_buffer_base[0], info, MIN(strlen(info), (MAX_TMP_BUFFER_STRING - 1)));
			maint_tmp_buffer_length = strlen(info);
		}

    return;
  }

	if (format == 0)
		return;

  if (maint_tmp_buffer_length == 0) {
		if ((TraceFilter[0] != 0) && ((TraceFilterIdent < 0) || (TraceFilterChannel < 0))) {
			return;
		}
	}

  diva_os_get_timestamp (&sec, &usec);

  if (do_lock) {
    diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "format");
  }

  if (maint_tmp_buffer_length != 0) {
		diva_maint_recovery_tmp_buffer (sec, usec);
	}

  switch (type) {
  case DLI_MXLOG :
  case DLI_BLK :
  case DLI_SEND:
  case DLI_RECV:
    if (!(length = va_arg(ap, unsigned long))) {
      break;
    }
    if (length > MaxDumpSize) {
      length = MaxDumpSize;
    }
    while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                (word)length+sizeof(*pmsg)))) {
      if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
        queueFreeMsg (dbg_queue);
      } else {
        break;
      }
    }
    if (pmsg) {
      memcpy (&pmsg[1], format, length);
      pmsg->sequence    = dbg_sequence++;
      pmsg->time_sec    = sec;
      pmsg->time_usec   = usec;
      pmsg->facility    = MSG_TYPE_BINARY ;
      pmsg->dli         = type; /* DLI_XXX */
      pmsg->drv_id      = id;   /* driver MAINT id */
      pmsg->di_cpu      = 0;
      pmsg->data_length = length;
      queueCompleteMsg (pmsg);
    }
		break;

  case DLI_XLOG: {
    byte* p;
    data    = va_arg(ap, char*);
    code    = (unsigned short)va_arg(ap, unsigned int);
    length	= (unsigned long) va_arg(ap, unsigned int);

    if (length > MaxXlogSize)
      length = MaxXlogSize;

    /*
      Inore management interface indications from entities
      which are owned by MAINT driver.
      */
    if (((code == 220 && length >= sizeof(word)*3) ||
				(code >  220 && code <= 222 && length >= sizeof(word)*4)) &&
				(discard_maint_idi_trace != 0 ||
        (id < sizeof(clients)/sizeof(clients[0]) &&
        clients[id].hDbg != 0 &&
        clients[id].drvName[0] == 'D' &&
        clients[id].drvName[1] == 'I' &&
        clients[id].drvName[2] == 'V' &&
        clients[id].drvName[3] == 'A' &&
        clients[id].drvName[4] == 'S'))) {
      word LogInfo[4];

      memcpy (&LogInfo[0], data, sizeof(LogInfo));
      if (((byte)(LogInfo[2] >> 8)) == MAN_ID) {
        byte xdi_adapter = (byte)LogInfo[0];
        byte xdi_id      = (byte)LogInfo[1];
        int i, skip;

        for (i = 0, skip = discard_maint_idi_trace; skip == 0 && i < sizeof(clients)/sizeof(clients[0]); i++) {
          if (clients[i].hDbg != 0 && clients[i].request != 0 &&
              clients[i].logical == xdi_adapter &&
              clients[i].pIdiLib != 0 && clients[i].pIdiLib->hLib != 0 &&
      ((ENTITY*)(*(clients[i].pIdiLib->DivaSTraceGetHandle))(clients[i].pIdiLib->hLib))->Id == xdi_id)
          skip = 1;
        }

        if (skip != 0)
          break;
      }
    }

    while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                  (word)length+sizeof(*pmsg)+2))) {
      if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
        queueFreeMsg (dbg_queue);
      } else {
        break;
      }
    }
    if (pmsg) {
      p = (byte*)&pmsg[1];
      p[0] = (char)(code) ;
      p[1] = (char)(code >> 8) ;
      if (data && length) {
        memcpy (&p[2], &data[0], length) ;
      }
      length += 2 ;

      pmsg->sequence    = dbg_sequence++;
      pmsg->time_sec    = sec;
      pmsg->time_usec   = usec;
      pmsg->facility    = MSG_TYPE_BINARY ;
      pmsg->dli         = type; /* DLI_XXX */
      pmsg->drv_id      = id;   /* driver MAINT id */
      pmsg->di_cpu      = 0;
      pmsg->data_length = length;
      queueCompleteMsg (pmsg);
    }
  } break;

  case DLI_LOG :
  case DLI_FTL :
  case DLI_ERR :
  case DLI_TRC :
  case DLI_REG :
  case DLI_MEM :
  case DLI_SPL :
  case DLI_IRP :
  case DLI_TIM :
  case DLI_TAPI:
  case DLI_NDIS:
  case DLI_CONN:
  case DLI_STAT:
  case DLI_PRV0:
  case DLI_PRV1:
  case DLI_PRV2:
  case DLI_PRV3:
    if ((length = (unsigned long)vsprintf (&fmtBuf[0], format, ap)) > 0) {
      length += (sizeof(*pmsg)+1);

      while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                                          (word)length))) {
        if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
          queueFreeMsg (dbg_queue);
        } else {
          break;
        }
      }
	if ( !pmsg ) break ;	/* BSOD occured */

      pmsg->sequence    = dbg_sequence++;
      pmsg->time_sec    = sec;
      pmsg->time_usec   = usec;
      pmsg->facility    = MSG_TYPE_STRING;
      pmsg->dli         = type; /* DLI_XXX */
      pmsg->drv_id      = id;   /* driver MAINT id */
      pmsg->di_cpu      = 0;
      pmsg->data_length = length - sizeof(*pmsg);

      memcpy (&pmsg[1], fmtBuf, pmsg->data_length);
		  queueCompleteMsg (pmsg);
    }
    break;

  } /* switch type */


  if (queueCount(dbg_queue)) {
    diva_maint_wakeup_read();
  }

  if (do_lock) {
    diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "format");
  }
}

/*
  Write driver ID and driver revision to callers buffer
  */
int diva_get_driver_info (dword id, byte* data, int data_length) {
  diva_os_spin_lock_magic_t old_irql;
  byte* p = data;
  int to_copy;

  if (!data || !id || (data_length < 17) ||
      (id >= (sizeof(clients)/sizeof(clients[0])))) {
    return (-1);
  }

  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "driver info");

  if (clients[id].hDbg) {
    *p++ = 1;
    *p++ = (byte)clients[id].sec; /* save secounds */
    *p++ = (byte)(clients[id].sec >>  8);
    *p++ = (byte)(clients[id].sec >> 16);
    *p++ = (byte)(clients[id].sec >> 24);

    *p++ = (byte)(clients[id].usec/1000); /* save msecounds */
    *p++ = (byte)((clients[id].usec/1000) >>  8);
    *p++ = (byte)((clients[id].usec/1000) >> 16);
    *p++ = (byte)((clients[id].usec/1000) >> 24);

    data_length -= 9;

    if ((to_copy = (int)MIN((int)strlen(clients[id].drvName), data_length-1))) {
      memcpy (p, clients[id].drvName, to_copy);
      p += to_copy;
      data_length -= to_copy;
      if ((data_length >= 4) && clients[id].hDbg->drvTag[0]) {
        *p++ = '(';
        data_length -= 1;
        if ((to_copy = (int)MIN((int)strlen(clients[id].hDbg->drvTag), data_length-2))) {
          memcpy (p, clients[id].hDbg->drvTag, to_copy);
          p += to_copy;
          data_length -= to_copy;
          if (data_length >= 2) {
            *p++ = ')';
            data_length--;
          }
        }
      }
    }
  }
  *p++ = 0;

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "driver info");

  return ((int)(p - data));
}

int diva_get_driver_dbg_mask (dword id, byte* data) {
  diva_os_spin_lock_magic_t old_irql;
  int ret = -1;

  if (!data || !id || (id >= (sizeof(clients)/sizeof(clients[0])))) {
    return (-1);
  }
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "driver info");

  if (clients[id].hDbg) {
    ret = 4;
    *data++= (byte)(clients[id].hDbg->dbgMask);
    *data++= (byte)(clients[id].hDbg->dbgMask >>  8);
    *data++= (byte)(clients[id].hDbg->dbgMask >> 16);
    *data++= (byte)(clients[id].hDbg->dbgMask >> 24);
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "driver info");

  return (ret);
}

static void _diva_set_driver_dbg_mask (const byte* name, dword name_length, dword mask) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
	dword id, old_mask;
	int found = 0;

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "dbg mask");


	for (id = 0; id < sizeof(clients)/sizeof(clients[0]); id++) {
		if (clients[id].hDbg != 0 &&
				strlen(clients[id].drvName) >= name_length &&
				memcmp (clients[id].drvName, name, name_length) == 0) {
			found = 1;
			old_mask = clients[id].hDbg->dbgMask;
			mask &= 0x7fffffff;
			clients[id].hDbg->dbgMask = mask;
			clients[id].last_dbgMask = (clients[id].hDbg->dbgMask | clients[id].dbgMask);
			diva_change_management_debug_mask (&clients[id], old_mask);
		}
	}

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "dbg mask");

	if (found != 0) {
		for (id = 0; id < sizeof(clients)/sizeof(clients[0]); id++) {
			if (clients[id].hDbg != 0 && clients[id].request_pending) {
				clients[id].request_pending = 0;
				(*(clients[id].request))((ENTITY*)(*(clients[id].pIdiLib->DivaSTraceGetHandle))
																														(clients[id].pIdiLib->hLib));
			}
		}
	}

  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");
}

int diva_set_driver_dbg_mask (dword id, dword mask) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  int ret = -1;


  if (!id || (id >= (sizeof(clients)/sizeof(clients[0])))) {
    return (-1);
  }

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "dbg mask");

  if (clients[id].hDbg) {
		diva_maint_dbg_mask_cfg_t* info = get_dbg_info_from_client_name (clients[id].drvName);
    dword old_mask = clients[id].hDbg->dbgMask;
    mask &= 0x7fffffff;
    clients[id].hDbg->dbgMask = mask;
		if (info != 0) {
			info->value = mask;
		}
    clients[id].last_dbgMask = (clients[id].hDbg->dbgMask | clients[id].dbgMask);
    ret = 4;
    diva_change_management_debug_mask (&clients[id], old_mask);
  }


  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "dbg mask");

  if (clients[id].request_pending) {
    clients[id].request_pending = 0;
    (*(clients[id].request))((ENTITY*)(*(clients[id].pIdiLib->DivaSTraceGetHandle))(clients[id].pIdiLib->hLib));
  }

  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");

  return (ret);
}

static int diva_get_idi_adapter_info (IDI_CALL request,
                                      dword* serial,
                                      dword* logical,
                                      dword* adapter_type,
                                      byte** address,
                                      dword* memory_length) {
  IDI_SYNC_REQ* sync_req = diva_os_malloc (0, sizeof(*sync_req));
  int ret = -1;

  if (sync_req != 0) {
    sync_req->xdi_logical_adapter_number.Req = 0;
    sync_req->xdi_logical_adapter_number.Rc = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
    sync_req->xdi_logical_adapter_number.info.logical_adapter_number = 0;
    (*request)((ENTITY*)sync_req);
    *logical = sync_req->xdi_logical_adapter_number.info.logical_adapter_number;

    sync_req->GetSerial.Req = 0;
    sync_req->GetSerial.Rc = IDI_SYNC_REQ_GET_SERIAL;
    sync_req->GetSerial.serial = 0;
    (*request)((ENTITY*)sync_req);
    *serial = sync_req->GetSerial.serial;

    sync_req->GetCardType.Req = 0;
    sync_req->GetCardType.Rc = IDI_SYNC_REQ_GET_CARDTYPE;
    sync_req->GetCardType.cardtype = 0 ;
    (*request)((ENTITY*)sync_req);
    *adapter_type = (dword)sync_req->GetCardType.cardtype;

		/*
			Check if user mode adapter
			*/
		sync_req->xdi_sdram_bar.Req      = 0;
		sync_req->xdi_sdram_bar.Rc       = IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR;
		sync_req->xdi_sdram_bar.info.bar = 0x00000001;
		(*request)((ENTITY*)sync_req);
		if (sync_req->xdi_sdram_bar.info.bar == 0x00000001) {
			sync_req->xdi_sdram_bar.Req      = 0;
			sync_req->xdi_sdram_bar.Rc       = IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR;
			sync_req->xdi_sdram_bar.info.bar = 0xabcd1234;
			(*request)((ENTITY*)sync_req);
			ret = (sync_req->xdi_sdram_bar.info.bar != 0xabcd1234) ? 0 : (-1);
		} else {
			ret = 0;
		}

    sync_req->diva_get_adapter_debug_storage_address.Req = 0;
    sync_req->diva_get_adapter_debug_storage_address.Rc  = IDI_SYNC_REQ_GET_ADAPTER_DEBUG_STORAGE_ADDRESS;
    sync_req->diva_get_adapter_debug_storage_address.info.storage_start  = 0;
    sync_req->diva_get_adapter_debug_storage_address.info.storage_length = 0;
    (*request)((ENTITY*)sync_req);
    *address = (byte*)sync_req->diva_get_adapter_debug_storage_address.info.storage_start;
    *memory_length = sync_req->diva_get_adapter_debug_storage_address.info.storage_length;

    diva_os_free (0, sync_req);
  }

  return (ret);
}

/*
  Register XDI adapter as MAINT compatible driver
  */
void diva_mnt_add_xdi_adapter (const DESCRIPTOR* d) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  dword sec, usec, logical, serial, org_mask, adapter_type;
  int id, best_id = 0, free_id = -1;
  char tmp[256];
  diva_dbg_entry_head_t* pmsg = 0;
  int len;
  word size;
  byte* pmem = 0;
  dword memory_length;

  diva_os_get_timestamp (&sec, &usec);

  if (diva_get_idi_adapter_info (d->request, &serial, &logical, &adapter_type, &pmem, &memory_length) != 0)
    return;

	if (logical == (dword)timestampAdapter) {
		register_timestamp (d->request);
	}

  if (external_dbg_queue == 0 && xdi_adapter_request == 0 && pmem != 0 && memory_length > 128*1024) {
		volatile dword *signature = (dword*)pmem;
		int saved_maint_buffer = 1;

		if (*signature == (dword)DBG_MAGIC) {
			/*
				Save debug/trace buffer
				*/
			saved_maint_buffer = diva_os_save_file (0 /* default location */,
																							"maint.bin",
																							pmem,
																							memory_length);
		}

    diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "mem");
    diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "mem");

		/*
			Save original debug/trace buffer and request function
			*/
		initial_dbg_base = dbg_base;
		initial_dbg_base_length = dbg_base_length;
		xdi_adapter_request = d->request;

		diva_maint_internal_init (pmem, memory_length, 1, 0);
		diva_mnt_internal_dprintf (0, DLI_LOG, "Use adapter %d memory (%p:%d)",
															 logical, pmem, memory_length);
		if (saved_maint_buffer != 1) {
			diva_mnt_internal_dprintf (0,
																 saved_maint_buffer == 0 ? DLI_LOG : DLI_ERR,
																 "%s MAINT debug/trace buffer",
																 saved_maint_buffer == 0 ? "Saved" : "Failed to save");
		}

    diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "mem");
    diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "mem");

    return;
  }

	if (d->channels == 0 || d->features == 0 || pmem != 0) {
		return;
	}

  if (serial & 0xff000000) {
    sprintf (tmp, "ADAPTER:%d SN:%u-%d",
             (int)logical,
             (unsigned int)serial & 0x00ffffff,
             (byte)(((serial & 0xff000000) >> 24) + 1));
  } else {
    sprintf (tmp, "ADAPTER:%d SN:%u", (int)logical, (unsigned int)serial);
  }

  if (!(pmem = diva_os_malloc (0, DivaSTraceGetMemotyRequirement (d->channels)))) {
    return;
  }
  memset (pmem, 0x00, DivaSTraceGetMemotyRequirement (d->channels));

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "register");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "register");

  for (id = 1; id < (sizeof(clients)/sizeof(clients[0])); id++) {
    if (clients[id].hDbg && (clients[id].request == d->request)) {
      diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");
      diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "register");
      return;
    }
    if (clients[id].hDbg) { /* slot is busy */
      continue;
    }
    if (free_id < 0) {
      free_id = id;
    }
    if (!strcmp (clients[id].drvName, tmp)) {
      /*
        This driver was already registered with this name
        and slot is still free - reuse it
        */
      free_id = id;
      best_id = 1;
      break;
    }
  }

  if (free_id < 0) {
    diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");
    diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "register");
    diva_os_free (0, pmem);
    return;
  }

  id = free_id;
  clients[id].request  = d->request;
  clients[id].request_pending = 0;
  clients[id].hDbg     = &clients[id].Dbg;
  clients[id].sec      = sec;
  clients[id].usec     = usec;

  memcpy (clients[id].drvName, tmp, MIN(sizeof(clients[id].drvName), (strlen(tmp)+1)));
  clients[id].drvName[sizeof(clients[id].drvName)-1] = 0;
  memcpy (clients[id].Dbg.drvName, tmp, MIN(sizeof(clients[id].Dbg.drvName), (strlen(tmp)+1)));
  clients[id].Dbg.drvName[sizeof(clients[id].Dbg.drvName)-1] = 0;

  clients[id].Dbg.drvTag[0] = 0;
  clients[id].logical  = (int)logical;
  clients[id].channels = (int)d->channels;
  clients[id].dma_handle = -1;
  clients[id].adapter_type = (int)adapter_type;

	{
		diva_maint_dbg_mask_cfg_t* info = get_dbg_info_from_client_name (clients[id].Dbg.drvName);

		if (info != 0) {
			clients[id].Dbg.dbgMask    = info->value;
		} else {
			clients[id].Dbg.dbgMask    = 0;
		}
	}
  clients[id].dbgMask        = clients[id].Dbg.dbgMask;
  if (id) {
    clients[id].Dbg.dbgMask |= clients[free_id].last_dbgMask;
  } else {
    clients[id].last_dbgMask = 0;
  }
  clients[id].Dbg.Registered = DBG_HANDLE_REG_NEW;
  clients[id].Dbg.id         = (byte)id;
  clients[id].Dbg.dbg_end    = DI_deregister;
  clients[id].Dbg.dbg_prt    = DI_format_locked;
  clients[id].Dbg.dbg_ev     = DiProcessEventLog;
  clients[id].Dbg.dbg_irq    = DI_format_locked;
  clients[id].Dbg.next       = (pDbgHandle)DBG_MAGIC;

  {
    diva_trace_library_user_interface_t diva_maint_user_ifc = { &clients[id],
																							 diva_maint_state_change_notify,
																							 diva_maint_trace_notify,
																							 diva_maint_error };

    /*
      Attach to adapter management interface
      */
    if ((clients[id].pIdiLib =
               DivaSTraceLibraryCreateInstance ((int)logical, &diva_maint_user_ifc, pmem))) {
      if (((*(clients[id].pIdiLib->DivaSTraceLibraryStart))(clients[id].pIdiLib->hLib))) {
        diva_mnt_internal_dprintf (0, DLI_ERR, "Adapter(%d) Start failed", (int)logical);
        (*(clients[id].pIdiLib->DivaSTraceLibraryFinit))(clients[id].pIdiLib->hLib);
        clients[id].pIdiLib = 0;
      }
    } else {
      diva_mnt_internal_dprintf (0, DLI_ERR, "A(%d) management init failed", (int)logical);
    }
  }

  if (!clients[id].pIdiLib) {
    clients[id].request = 0;
    clients[id].request_pending = 0;
    clients[id].hDbg    = 0;
    diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");
    diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "register");
    diva_os_free (0, pmem);
    return;
  }
	clients[id].pmem = pmem;

  /*
    Log driver register, MAINT driver ID is '0'
    */
  len = sprintf (tmp, "DIMAINT - drv # %d = '%s' registered",
                 id, clients[id].drvName);

  while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                      (word)(len+1+sizeof(*pmsg))))) {
    if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
      queueFreeMsg (dbg_queue);
    } else {
      break;
    }
  }

  if (pmsg) {
    pmsg->sequence    = dbg_sequence++;
    pmsg->time_sec    = sec;
    pmsg->time_usec   = usec;
    pmsg->facility    = MSG_TYPE_STRING;
    pmsg->dli         = DLI_LOG;
    pmsg->drv_id      = 0; /* id 0 - DIMAINT */
    pmsg->di_cpu      = 0;
    pmsg->data_length = len+1;

    memcpy (&pmsg[1], tmp, len+1);
    queueCompleteMsg (pmsg);
    diva_maint_wakeup_read();
  }

  org_mask = clients[id].Dbg.dbgMask;
  clients[id].Dbg.dbgMask = 0;

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "register");

  if (clients[id].request_pending) {
    clients[id].request_pending = 0;
    (*(clients[id].request))((ENTITY*)(*(clients[id].pIdiLib->DivaSTraceGetHandle))(clients[id].pIdiLib->hLib));
  }

  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "register");

	diva_set_driver_dbg_mask (id, org_mask);
}

/*
  De-Register XDI adapter
  */
void diva_mnt_remove_xdi_adapter (const DESCRIPTOR* d) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  dword sec, usec;
  int i;
  word size;
  byte* pmem = 0;

	if (timestampAdapterRequest == d->request) {
		unregister_timestamp ();
	}

  diva_os_get_timestamp (&sec, &usec);

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "read");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "read");

	if (d->request == xdi_adapter_request) {
		/*
			XDI adapter removed, restore original debug/trace buffer
			*/
		diva_maint_internal_init (initial_dbg_base, initial_dbg_base_length, 1, 0);
		xdi_adapter_request = 0;
		initial_dbg_base  = 0;
		initial_dbg_base_length = 0;

		diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "read_ack");
		diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "read_ack");

		return;
	}

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].hDbg && (clients[i].request == d->request)) {
      diva_dbg_entry_head_t* pmsg;
      char tmp[256];
      int len;

      if (clients[i].pIdiLib) {
        (*(clients[i].pIdiLib->DivaSTraceLibraryFinit))(clients[i].pIdiLib->hLib);
        clients[i].pIdiLib = 0;

        pmem = clients[i].pmem;
        clients[i].pmem = 0;
      }

      clients[i].hDbg    = 0;
      clients[i].request_pending = 0;
      if (clients[i].dma_handle >= 0) {
        /*
          Free DMA handle
          */
        diva_free_dma_descriptor (clients[i].request, clients[i].dma_handle);
        clients[i].dma_handle = -1;
      }
      clients[i].request = 0;

      /*
        Log driver register, MAINT driver ID is '0'
        */
      len = sprintf (tmp, "DIMAINT - drv # %d = '%s' de-registered",
                     i, clients[i].Dbg.drvName);

      memset (&clients[i].Dbg, 0x00, sizeof(clients[i].Dbg));

      while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                                        (word)(len+1+sizeof(*pmsg))))) {
        if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
          queueFreeMsg (dbg_queue);
        } else {
          break;
        }
      }

      if (pmsg) {
        pmsg->sequence    = dbg_sequence++;
        pmsg->time_sec    = sec;
        pmsg->time_usec   = usec;
        pmsg->facility    = MSG_TYPE_STRING;
        pmsg->dli         = DLI_LOG;
        pmsg->drv_id      = 0; /* id 0 - DIMAINT */
        pmsg->di_cpu      = 0;
        pmsg->data_length = len+1;

        memcpy (&pmsg[1], tmp, len+1);
  		  queueCompleteMsg (pmsg);
        diva_maint_wakeup_read();
      }

      break;
    }
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "read_ack");
  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "read_ack");

  if (pmem) {
    diva_os_free (0, pmem);
  }
}

/* ----------------------------------------------------------------
     Low level interface for management interace client
   ---------------------------------------------------------------- */
/*
  Return handle to client structure
  */
void* SuperTraceOpenAdapter   (int AdapterNumber) {
  int i;

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].hDbg && clients[i].request && (clients[i].logical == AdapterNumber)) {
      return (&clients[i]);
    }
  }

  return (0);
}

int SuperTraceCloseAdapter  (void* AdapterHandle) {
  return (0);
}

int SuperTraceReadRequest (void* AdapterHandle, const char* name, byte* data) {
  diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    byte* xdata = (byte*)&pC->xbuffer[0];
    char tmp = 0;
    word length;

    if (!strcmp(name, "\\")) { /* Read ROOT */
      name = &tmp;
    }
    length = SuperTraceCreateReadReq (xdata, name);
    single_p (xdata, &length, 0); /* End Of Message */

    e->Req        = MAN_READ;
    e->ReqCh      = 0;
    e->X->PLength = length;
    e->X->P			  = (byte*)xdata;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceGetNumberOfChannels (void* AdapterHandle) {
  if (AdapterHandle) {
    diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

		return (pC->channels == 30 ? 31 : pC->channels);
  }

  return (0);
}

int SuperTraceGetAdapterType (void* AdapterHandle) {
  if (AdapterHandle) {
    diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

    return (pC->adapter_type);
  }

  return (0);
}

int SuperTraceASSIGN (void* AdapterHandle, byte* data) {
  diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    IDI_SYNC_REQ* preq;
    char buffer[((sizeof(preq->xdi_extended_features)+4) > sizeof(ENTITY)) ? (sizeof(preq->xdi_extended_features)+4) : sizeof(ENTITY)];
    char features[4];
    word assign_data_length = 1;

    features[0] = 0;
    pC->xbuffer[0] = 0;
    preq = (IDI_SYNC_REQ*)&buffer[0];
    preq->xdi_extended_features.Req = 0;
    preq->xdi_extended_features.Rc  = IDI_SYNC_REQ_XDI_GET_EXTENDED_FEATURES;
    preq->xdi_extended_features.info.buffer_length_in_bytes = sizeof(features);
    preq->xdi_extended_features.info.features = &features[0];

    (*(pC->request))((ENTITY*)preq);

    if ((features[0] & DIVA_XDI_EXTENDED_FEATURES_VALID) &&
        (features[0] & DIVA_XDI_EXTENDED_FEATURE_MANAGEMENT_DMA)) {
      dword rx_dma_magic = 0;
      if ((pC->dma_handle = diva_get_dma_descriptor (pC->request, &rx_dma_magic)) >= 0) {
        pC->xbuffer[0] = LLI;
        pC->xbuffer[1] = 8;
        pC->xbuffer[2] = 0x40;
        pC->xbuffer[3] = (byte)pC->dma_handle;
        pC->xbuffer[4] = (byte)rx_dma_magic;
        pC->xbuffer[5] = (byte)(rx_dma_magic >>  8);
        pC->xbuffer[6] = (byte)(rx_dma_magic >> 16);
        pC->xbuffer[7] = (byte)(rx_dma_magic >> 24);
        pC->xbuffer[8] = (byte)DIVA_MAX_MANAGEMENT_TRANSFER_SIZE;
        pC->xbuffer[9] = (byte)(DIVA_MAX_MANAGEMENT_TRANSFER_SIZE >> 8);
        pC->xbuffer[10] = 0;

        assign_data_length = 11;
      } else {
        pC->xbuffer[0] = LLI;
        pC->xbuffer[1] = 8;
        pC->xbuffer[2] = 0x04;
        pC->xbuffer[3] = 0;
        pC->xbuffer[4] = 0;
        pC->xbuffer[5] = 0;
        pC->xbuffer[6] = 0;
        pC->xbuffer[7] = 0;
        pC->xbuffer[8] = (byte)DIVA_MAX_MANAGEMENT_TRANSFER_SIZE;
        pC->xbuffer[9] = (byte)(DIVA_MAX_MANAGEMENT_TRANSFER_SIZE >> 8);
        pC->xbuffer[10] = 0;

        assign_data_length = 11;
      }
    } else {
      pC->dma_handle = -1;
    }

    e->Id          = MAN_ID;
    e->callback    = diva_maint_xdi_cb;
    e->XNum        = 1;
    e->X           = &pC->XData;
    e->Req         = ASSIGN;
    e->ReqCh       = 0;
    e->X->PLength  = assign_data_length;
    e->X->P        = (byte*)&pC->xbuffer[0];

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceREMOVE (void* AdapterHandle) {
  diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);

    e->XNum        = 1;
    e->X           = &pC->XData;
    e->Req         = REMOVE;
    e->ReqCh       = 0;
    e->X->PLength  = 1;
    e->X->P        = (byte*)&pC->xbuffer[0];
    pC->xbuffer[0] = 0;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceTraceOnRequest(void* hAdapter, const char* name, byte* data) {
  diva_maint_client_t* pC = (diva_maint_client_t*)hAdapter;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    byte* xdata = (byte*)&pC->xbuffer[0];
    char tmp = 0;
    word length;

    if (!strcmp(name, "\\")) { /* Read ROOT */
      name = &tmp;
    }
    length = SuperTraceCreateReadReq (xdata, name);
    single_p (xdata, &length, 0); /* End Of Message */
    e->Req          = MAN_EVENT_ON;
    e->ReqCh        = 0;
    e->X->PLength   = length;
    e->X->P			    = (byte*)xdata;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceTraceOffRequest(void* hAdapter, const char* name, byte* data) {
  diva_maint_client_t* pC = (diva_maint_client_t*)hAdapter;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    byte* xdata = (byte*)&pC->xbuffer[0];
    char tmp = 0;
    word length;

		if (!strcmp(name, "\\")) { /* Read ROOT */
      name = &tmp;
    }
    length = SuperTraceCreateReadReq (xdata, name);
    single_p (xdata, &length, 0); /* End Of Message */
    e->Req          = MAN_EVENT_OFF;
    e->ReqCh        = 0;
    e->X->PLength   = length;
    e->X->P			    = (byte*)xdata;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceWriteVar (void* AdapterHandle,
                        byte* data,
                        const char* name,
                        void* var,
                        byte type,
                        byte var_length) {
  diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    diva_man_var_header_t* pVar = (diva_man_var_header_t*)&pC->xbuffer[0];
    word length = SuperTraceCreateReadReq ((byte*)pVar, name);

    memcpy (&pC->xbuffer[length], var, var_length);
    length += var_length;
    pVar->length += var_length;
    pVar->value_length = var_length;
    pVar->type = type;
    single_p ((byte*)pVar, &length, 0); /* End Of Message */

    e->Req          = MAN_WRITE;
    e->ReqCh			  = 0;
    e->X->PLength   = length;
    e->X->P			    = (byte*)pVar;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

int SuperTraceExecuteRequest (void* AdapterHandle,
                              const char* name,
                              byte* data) {
  diva_maint_client_t* pC = (diva_maint_client_t*)AdapterHandle;

  if (pC && pC->pIdiLib && pC->request) {
    ENTITY* e = (ENTITY*)(*(pC->pIdiLib->DivaSTraceGetHandle))(pC->pIdiLib->hLib);
    byte* xdata = (byte*)&pC->xbuffer[0];
    word length;

    length = SuperTraceCreateReadReq (xdata, name);
    single_p (xdata, &length, 0); /* End Of Message */

    e->Req          = MAN_EXECUTE;
    e->ReqCh			  = 0;
    e->X->PLength   = length;
    e->X->P			    = (byte*)xdata;

    pC->request_pending = 1;

    return (0);
  }

  return (-1);
}

static word SuperTraceCreateReadReq (byte* P, const char* path) {
	byte var_length;
	byte* plen;

	var_length = (byte)strlen (path);

	*P++ = ESC;
	plen = P++;
	*P++ = 0x80; /* MAN_IE */
	*P++ = 0x00; /* Type */
	*P++ = 0x00; /* Attribute */
	*P++ = 0x00; /* Status */
	*P++ = 0x00; /* Variable Length */
	*P++ = var_length;
	memcpy (P, path, var_length);
	P += var_length;
	*plen = var_length + 0x06;

	return ((word)(var_length + 0x08));
}

static void single_p (byte * P, word * PLength, byte Id) {
  P[(*PLength)++] = Id;
}

static void diva_maint_xdi_cb (ENTITY* e) {
  diva_strace_context_t* pLib = DIVAS_CONTAINING_RECORD(e,diva_strace_context_t,e);
  diva_maint_client_t* pC;
  diva_os_spin_lock_magic_t old_irql, old_irql1;


  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "xdi_cb");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "xdi_cb");

  pC = (diva_maint_client_t*)pLib->hAdapter;

  if ((e->complete == 255) || (pC->dma_handle < 0)) {
    if ((*(pLib->instance.DivaSTraceMessageInput))(&pLib->instance)) {
      diva_mnt_internal_dprintf (0, DLI_ERR, "Trace internal library error");
    }
  } else {
    /*
      Process combined management interface indication
      */
    if ((*(pLib->instance.DivaSTraceMessageInput))(&pLib->instance)) {
      diva_mnt_internal_dprintf (0, DLI_ERR, "Trace internal library error (DMA mode)");
    }
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "xdi_cb");


	if (pC->request_pending) {
    pC->request_pending = 0;
    (*(pC->request))(e);
	}

  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "xdi_cb");
}


static void diva_maint_error (void* user_context,
                              diva_strace_library_interface_t* hLib,
                              int Adapter,
                              int error,
                              const char* file,
                              int line) {
	diva_mnt_internal_dprintf (0, DLI_ERR,
                             "Trace library error(%d) A(%d) %s %d", error, Adapter, file, line);
}

static void print_ie (diva_trace_ie_t* ie, char* buffer, int length) {
	int i;

  buffer[0] = 0;

  if (length > 32) {
    for (i = 0; ((i < ie->length) && (length > 3)); i++) {
      sprintf (buffer, "%02x", ie->data[i]);
      buffer += 2;
      length -= 2;
      if (i < (ie->length-1)) {
        strcpy (buffer, " ");
        buffer++;
        length--;
      }
    }
  }
}

static void diva_maint_state_change_notify (void* user_context,
                                            diva_strace_library_interface_t* hLib,
                                            int Adapter,
                                            diva_trace_line_state_t* channel,
                                            int notify_subject) {
  diva_maint_client_t*      pC    = (diva_maint_client_t*)user_context;
  diva_trace_fax_state_t*   fax   = &channel->fax;
  diva_trace_modem_state_t* modem = &channel->modem;
  char tmp[256];

  if (!pC->hDbg) {
    return;
  }

  switch (notify_subject) {
    case DIVA_SUPER_TRACE_NOTIFY_LINE_CHANGE: {
      int view = (TraceFilter[0] == 0);
      /*
        Process selective Trace
        */
      if (channel->Line[0] == 'I' && channel->Line[1] == 'd' &&
          channel->Line[2] == 'l' && channel->Line[3] == 'e') {
        if ((TraceFilterIdent == pC->hDbg->id) && (TraceFilterChannel == (int)channel->ChannelNumber)) {
          (*(hLib->DivaSTraceSetBChannel))(hLib, (int)channel->ChannelNumber, 0);
          (*(hLib->DivaSTraceSetAudioTap))(hLib, (int)channel->ChannelNumber, 0);
					(*(hLib->DivaSTraceSetMaxTraceRecordLength))(hLib, 30);
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG, "Selective Trace OFF for Ch=%d",
                                     (int)channel->ChannelNumber);
          TraceFilterIdent   = -1;
          TraceFilterChannel = -1;
          view = 1;
        }
      } else if (TraceFilter[0] && (TraceFilterIdent < 0) && !(diva_mnt_cmp_nmbr (&channel->RemoteAddress[0]) &&
                                                               diva_mnt_cmp_nmbr (&channel->LocalAddress[0]))) {

        (*(hLib->DivaSTraceSetMaxTraceRecordLength))(hLib, 2048);

        if ((pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_BCHANNEL) != 0) { /* Activate B-channel trace */
          (*(hLib->DivaSTraceSetBChannel))(hLib, (int)channel->ChannelNumber, 1);
        }
        if ((pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_AUDIO) != 0) { /* Activate AudioTap Trace */
          (*(hLib->DivaSTraceSetAudioTap))(hLib, (int)channel->ChannelNumber, 1);
        }

        TraceFilterIdent   = pC->hDbg->id;
        TraceFilterChannel = (int)channel->ChannelNumber;

        if (TraceFilterIdent >= 0) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG, "Selective Trace ON (2048) for Ch=%d",
                                     (int)channel->ChannelNumber);
          view = 1;
        }
      }
      if (view && (pC->hDbg->dbgMask & DIVA_MGT_DBG_LINE_EVENTS)) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Ch     = %d",
                                                                     (int)channel->ChannelNumber);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Status = <%s>", &channel->Line[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Layer1 = <%s>", &channel->Framing[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Layer2 = <%s>", &channel->Layer2[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Layer3 = <%s>", &channel->Layer3[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L RAddr  = <%s>",
                                                                     &channel->RemoteAddress[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L RSAddr = <%s>",
                                                                     &channel->RemoteSubAddress[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L LAddr  = <%s>",
                                                                     &channel->LocalAddress[0]);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L LSAddr = <%s>",
                                                                     &channel->LocalSubAddress[0]);
        print_ie(&channel->call_BC, tmp, sizeof(tmp));
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L BC     = <%s>", tmp);
        print_ie(&channel->call_HLC, tmp, sizeof(tmp));
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L HLC    = <%s>", tmp);
        print_ie(&channel->call_LLC, tmp, sizeof(tmp));
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L LLC    = <%s>", tmp);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L CR     = 0x%x", channel->CallReference);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Disc   = 0x%x",
                                                                    channel->LastDisconnecCause);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT, "L Owner  = <%s>", &channel->UserID[0]);
      }

		} break;

    case DIVA_SUPER_TRACE_NOTIFY_MODEM_CHANGE:
      if (pC->hDbg->dbgMask & DIVA_MGT_DBG_MDM_PROGRESS) {
				{
					int ch = TraceFilterChannel;
					int id = TraceFilterIdent;

					if ((id >= 0) && (ch >= 0) && (id < sizeof(clients)/sizeof(clients[0])) &&
						(clients[id].Dbg.id == (byte)id) && (clients[id].pIdiLib == hLib)) {
						if (ch != (int)modem->ChannelNumber) {
							break;
						}
					} else if (TraceFilter[0] != 0) {
						break;
					}
				}


        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Ch    = %lu",
                                                                     (int)modem->ChannelNumber);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Event = %lu",     modem->Event);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Norm  = %lu",     modem->Norm);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Opts. = 0x%08x",  modem->Options);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Tx    = %lu Bps", modem->TxSpeed);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Rx    = %lu Bps", modem->RxSpeed);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM RT    = %lu mSec",
                                                                     modem->RoundtripMsec);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Sr    = %lu",     modem->SymbolRate);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM Rxl   = %d dBm",  modem->RxLeveldBm);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM El    = %d dBm",  modem->EchoLeveldBm);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM SNR   = %lu dB",  modem->SNRdb);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM MAE   = %lu",     modem->MAE);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM LRet  = %lu",
                                                                     modem->LocalRetrains);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM RRet  = %lu",
                                                                     modem->RemoteRetrains);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM LRes  = %lu",     modem->LocalResyncs);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "MDM RRes  = %lu",
                                                                     modem->RemoteResyncs);
        if (modem->Event == 3) {
          diva_mnt_internal_dprintf(pC->hDbg->id,DLI_STAT,"MDM Disc  =  %lu",    modem->DiscReason);
        }
      }
      if ((modem->Event == 3) && (pC->hDbg->dbgMask & DIVA_MGT_DBG_MDM_STATISTICS)) {
        (*(pC->pIdiLib->DivaSTraceGetModemStatistics))(pC->pIdiLib);
      }
      break;

    case DIVA_SUPER_TRACE_NOTIFY_FAX_CHANGE:
      if (pC->hDbg->dbgMask & DIVA_MGT_DBG_FAX_PROGRESS) {
				{
					int ch = TraceFilterChannel;
					int id = TraceFilterIdent;

					if ((id >= 0) && (ch >= 0) && (id < sizeof(clients)/sizeof(clients[0])) &&
						(clients[id].Dbg.id == (byte)id) && (clients[id].pIdiLib == hLib)) {
						if (ch != (int)fax->ChannelNumber) {
							break;
						}
					} else if (TraceFilter[0] != 0) {
						break;
					}
				}

        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Ch    = %lu",(int)fax->ChannelNumber);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Event = %lu",     fax->Event);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Pages = %lu",     fax->Page_Counter);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Feat. = 0x%08x",  fax->Features);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX ID    = <%s>",    &fax->Station_ID[0]);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Saddr = <%s>",    &fax->Subaddress[0]);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Pwd   = <%s>",    &fax->Password[0]);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Speed = %lu",     fax->Speed);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Res.  = 0x%08x",  fax->Resolution);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Width = %lu",     fax->Paper_Width);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Length= %lu",     fax->Paper_Length);
        diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX SLT   = %lu",     fax->Scanline_Time);
        if (fax->Event == 3) {
          diva_mnt_internal_dprintf(pC->hDbg->id, DLI_STAT, "FAX Disc  = %lu",     fax->Disc_Reason);
        }
      }
      if ((fax->Event == 3) && (pC->hDbg->dbgMask & DIVA_MGT_DBG_FAX_STATISTICS)) {
        (*(pC->pIdiLib->DivaSTraceGetFaxStatistics))(pC->pIdiLib);
      }
      break;

    case DIVA_SUPER_TRACE_INTERFACE_CHANGE:
      if (pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_EVENTS) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT,
                                 "Layer 1 -> [%s]", channel->pInterface->Layer1);
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT,
                                 "Layer 2 -> [%s]", channel->pInterface->Layer2);
      }
      break;

		case DIVA_SUPER_TRACE_TEMPERATURE_CHANGE:
      diva_mnt_internal_dprintf (pC->hDbg->id, DLI_STAT,
                                 "Temperature: initial:%d min:%d max:%d, current:%d",
                                 channel->pInterface->InitialTemperature,
                                 channel->pInterface->MinTemperature,
                                 channel->pInterface->MaxTemperature,
                                 channel->pInterface->Temperature);
			break;

    case DIVA_SUPER_TRACE_NOTIFY_STAT_CHANGE:
      if (pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_STATISTICS) {
        /*
          Incoming Statistices
          */
        if (channel->pInterfaceStat->inc.Calls) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Calls                     =%lu", channel->pInterfaceStat->inc.Calls);
        }
        if (channel->pInterfaceStat->inc.Connected) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Connected                 =%lu", channel->pInterfaceStat->inc.Connected);
        }
        if (channel->pInterfaceStat->inc.User_Busy) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Busy                      =%lu", channel->pInterfaceStat->inc.User_Busy);
        }
        if (channel->pInterfaceStat->inc.Call_Rejected) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Rejected                  =%lu", channel->pInterfaceStat->inc.Call_Rejected);
        }
        if (channel->pInterfaceStat->inc.Wrong_Number) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Wrong Nr                  =%lu", channel->pInterfaceStat->inc.Wrong_Number);
        }
        if (channel->pInterfaceStat->inc.Incompatible_Dst) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Incomp. Dest              =%lu", channel->pInterfaceStat->inc.Incompatible_Dst);
        }
        if (channel->pInterfaceStat->inc.Out_of_Order) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Out of Order              =%lu", channel->pInterfaceStat->inc.Out_of_Order);
        }
        if (channel->pInterfaceStat->inc.Ignored) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Inc Ignored                   =%lu", channel->pInterfaceStat->inc.Ignored);
        }

        /*
          Outgoing Statistices
          */
        if (channel->pInterfaceStat->outg.Calls) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Calls                    =%lu", channel->pInterfaceStat->outg.Calls);
        }
        if (channel->pInterfaceStat->outg.Connected) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Connected                =%lu", channel->pInterfaceStat->outg.Connected);
        }
        if (channel->pInterfaceStat->outg.User_Busy) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Busy                     =%lu", channel->pInterfaceStat->outg.User_Busy);
        }
        if (channel->pInterfaceStat->outg.No_Answer) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg No Answer                =%lu", channel->pInterfaceStat->outg.No_Answer);
        }
        if (channel->pInterfaceStat->outg.Wrong_Number) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Wrong Nr                 =%lu", channel->pInterfaceStat->outg.Wrong_Number);
        }
        if (channel->pInterfaceStat->outg.Call_Rejected) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Rejected                 =%lu", channel->pInterfaceStat->outg.Call_Rejected);
        }
        if (channel->pInterfaceStat->outg.Other_Failures) {
          diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
          "Outg Other Failures           =%lu", channel->pInterfaceStat->outg.Other_Failures);
        }
      }
      break;

    case DIVA_SUPER_TRACE_NOTIFY_MDM_STAT_CHANGE:
      if (channel->pInterfaceStat->mdm.Disc_Normal) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Normal        = %lu", channel->pInterfaceStat->mdm.Disc_Normal);
      }
      if (channel->pInterfaceStat->mdm.Disc_Unspecified) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Unsp.         = %lu", channel->pInterfaceStat->mdm.Disc_Unspecified);
      }
      if (channel->pInterfaceStat->mdm.Disc_Busy_Tone) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Busy Tone     = %lu", channel->pInterfaceStat->mdm.Disc_Busy_Tone);
      }
      if (channel->pInterfaceStat->mdm.Disc_Congestion) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Congestion    = %lu", channel->pInterfaceStat->mdm.Disc_Congestion);
      }
      if (channel->pInterfaceStat->mdm.Disc_Carr_Wait) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Carrier Wait  = %lu", channel->pInterfaceStat->mdm.Disc_Carr_Wait);
      }
      if (channel->pInterfaceStat->mdm.Disc_Trn_Timeout) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Trn. T.o.     = %lu", channel->pInterfaceStat->mdm.Disc_Trn_Timeout);
      }
      if (channel->pInterfaceStat->mdm.Disc_Incompat) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Incompatible  = %lu", channel->pInterfaceStat->mdm.Disc_Incompat);
      }
      if (channel->pInterfaceStat->mdm.Disc_Frame_Rej) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc Frame Reject  = %lu", channel->pInterfaceStat->mdm.Disc_Frame_Rej);
      }
      if (channel->pInterfaceStat->mdm.Disc_V42bis) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "MDM Disc V.42bis       = %lu", channel->pInterfaceStat->mdm.Disc_V42bis);
      }
      break;

    case DIVA_SUPER_TRACE_NOTIFY_FAX_STAT_CHANGE:
      if (channel->pInterfaceStat->fax.Disc_Normal) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Normal        = %lu", channel->pInterfaceStat->fax.Disc_Normal);
      }
      if (channel->pInterfaceStat->fax.Disc_Not_Ident) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Not Ident.    = %lu", channel->pInterfaceStat->fax.Disc_Not_Ident);
      }
      if (channel->pInterfaceStat->fax.Disc_No_Response) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc No Response   = %lu", channel->pInterfaceStat->fax.Disc_No_Response);
      }
      if (channel->pInterfaceStat->fax.Disc_Retries) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Max Retries   = %lu", channel->pInterfaceStat->fax.Disc_Retries);
      }
      if (channel->pInterfaceStat->fax.Disc_Unexp_Msg) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Unexp. Msg.        = %lu", channel->pInterfaceStat->fax.Disc_Unexp_Msg);
      }
      if (channel->pInterfaceStat->fax.Disc_No_Polling) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc No Polling    = %lu", channel->pInterfaceStat->fax.Disc_No_Polling);
      }
      if (channel->pInterfaceStat->fax.Disc_Training) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Training      = %lu", channel->pInterfaceStat->fax.Disc_Training);
      }
      if (channel->pInterfaceStat->fax.Disc_Unexpected) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Unexpected    = %lu", channel->pInterfaceStat->fax.Disc_Unexpected);
      }
      if (channel->pInterfaceStat->fax.Disc_Application) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Application   = %lu", channel->pInterfaceStat->fax.Disc_Application);
      }
      if (channel->pInterfaceStat->fax.Disc_Incompat) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Incompatible  = %lu", channel->pInterfaceStat->fax.Disc_Incompat);
      }
      if (channel->pInterfaceStat->fax.Disc_No_Command) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc No Command    = %lu", channel->pInterfaceStat->fax.Disc_No_Command);
      }
      if (channel->pInterfaceStat->fax.Disc_Long_Msg) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Long Msg.     = %lu", channel->pInterfaceStat->fax.Disc_Long_Msg);
      }
      if (channel->pInterfaceStat->fax.Disc_Supervisor) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Supervisor    = %lu", channel->pInterfaceStat->fax.Disc_Supervisor);
      }
      if (channel->pInterfaceStat->fax.Disc_SUB_SEP_PWD) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc SUP SEP PWD   = %lu", channel->pInterfaceStat->fax.Disc_SUB_SEP_PWD);
      }
      if (channel->pInterfaceStat->fax.Disc_Invalid_Msg) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Invalid Msg.  = %lu", channel->pInterfaceStat->fax.Disc_Invalid_Msg);
      }
      if (channel->pInterfaceStat->fax.Disc_Page_Coding) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Page Coding   = %lu", channel->pInterfaceStat->fax.Disc_Page_Coding);
      }
      if (channel->pInterfaceStat->fax.Disc_App_Timeout) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Appl. T.o.    = %lu", channel->pInterfaceStat->fax.Disc_App_Timeout);
      }
      if (channel->pInterfaceStat->fax.Disc_Unspecified) {
        diva_mnt_internal_dprintf (pC->hDbg->id, DLI_LOG,
        "FAX Disc Unspec.       = %lu", channel->pInterfaceStat->fax.Disc_Unspecified);
      }
      break;
  }
}

/*
  Receive trace information from the Management Interface and store it in the
  internal trace buffer with MSG_TYPE_MLOG as is, without any filtering.
  Event Filtering and formatting is done in  Management Interface self.
  */
static void diva_maint_trace_notify (void* user_context,
                                     diva_strace_library_interface_t* hLib,
                                     int Adapter,
                                     void* xlog_buffer,
                                     int length) {
  diva_maint_client_t* pC = (diva_maint_client_t*)user_context;
  diva_dbg_entry_head_t* pmsg;
  word size;
  dword sec, usec;
  int ch = TraceFilterChannel;
  int id = TraceFilterIdent;

  /*
    Selective trace
    */
  if ((id >= 0) && (ch >= 0) && (id < sizeof(clients)/sizeof(clients[0])) &&
      (clients[id].Dbg.id == (byte)id) && (clients[id].pIdiLib == hLib)) {
    const char* p = 0;
    int ch_value = -1;
    MI_XLOG_HDR *TrcData = (MI_XLOG_HDR *)xlog_buffer;

    if (Adapter != clients[id].logical) {
      return; /* Ignore all trace messages from other adapters */
    }

    if (TrcData->code == 24) {
      p = (char*)&TrcData->code;
      p += 2;
    }

    /*
      All L1 messages start as [dsp,ch], so we can filter this information
      and filter out all messages that use different channel
      */
    if (p && p[0] == '[') {
      if (p[2] == ',') {
        p += 3;
        ch_value = *p - '0';
      } else if (p[3] == ',') {
        p += 4;
        ch_value = *p - '0';
      }
      if (ch_value >= 0) {
        if (p[2] == ']') {
          ch_value = ch_value * 10 + p[1] - '0';
        }
        if (ch_value != ch) {
          return; /* Ignore other channels */
        }
      }
    }

	} else if (TraceFilter[0] != 0) {
    return; /* Ignore trace it trace filter is activated, but idle */
  }

  diva_os_get_timestamp (&sec, &usec);

  while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue,
                              (word)length+sizeof(*pmsg)))) {
    if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
      queueFreeMsg (dbg_queue);
    } else {
      break;
    }
  }
  if (pmsg) {
    memcpy (&pmsg[1], xlog_buffer, length);
    pmsg->sequence    = dbg_sequence++;
    pmsg->time_sec    = sec;
    pmsg->time_usec   = usec;
    pmsg->facility    = MSG_TYPE_MLOG;
    pmsg->dli         = pC->logical;
    pmsg->drv_id      = pC->hDbg->id;
    pmsg->di_cpu      = 0;
    pmsg->data_length = length;
    queueCompleteMsg (pmsg);
    if (queueCount(dbg_queue)) {
      diva_maint_wakeup_read();
    }
  }
}


/*
  Convert MAINT trace mask to management interface trace mask/work/facility and
  issue command to management interface
  */
static void diva_change_management_debug_mask (diva_maint_client_t* pC, dword old_mask) {
  if (pC->request && pC->hDbg && pC->pIdiLib) {
    dword changed;

    if (TraceFilter[0] != 0) {
      pC->hDbg->dbgMask |= DIVA_MGT_DBG_LINE_EVENTS;
    }
    changed = pC->hDbg->dbgMask ^ old_mask;

    if (changed & DIVA_MGT_DBG_TRACE) {
      (*(pC->pIdiLib->DivaSTraceSetInfo))(pC->pIdiLib,
                                          (pC->hDbg->dbgMask & DIVA_MGT_DBG_TRACE) != 0);
    }
    if (changed & DIVA_MGT_DBG_DCHAN) {
      (*(pC->pIdiLib->DivaSTraceSetDChannel))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_DCHAN) != 0);
    }
    if (!TraceFilter[0]) {
      if (changed & DIVA_MGT_DBG_IFC_BCHANNEL) {
        int i, state = ((pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_BCHANNEL) != 0);

        for (i = 0; i < pC->channels; i++) {
          (*(pC->pIdiLib->DivaSTraceSetBChannel))(pC->pIdiLib, i+1, state);
        }
      }
      if (changed & DIVA_MGT_DBG_IFC_AUDIO) {
        int i, state = ((pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_AUDIO) != 0);

        for (i = 0; i < pC->channels; i++) {
          (*(pC->pIdiLib->DivaSTraceSetAudioTap))(pC->pIdiLib, i+1, state);
        }
      }
    }
		if (changed & DIVA_MGT_DBG_HISTORY) {
      (*(pC->pIdiLib->DivaSTraceSetHistory))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_HISTORY) != 0);
	  }
		if (changed & DIVA_MGT_DBG_LINE_EVENTS) {
      (*(pC->pIdiLib->DivaSTraceSetLineState))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_LINE_EVENTS) != 0);
		}
		if (changed & DIVA_MGT_DBG_MDM_PROGRESS) {
      (*(pC->pIdiLib->DivaSTraceSetModemState))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_MDM_PROGRESS) != 0);
		}
		if (changed & DIVA_MGT_DBG_FAX_PROGRESS) {
      (*(pC->pIdiLib->DivaSTraceSetFaxState))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_FAX_PROGRESS) != 0);
		}
		if (changed & DIVA_MGT_DBG_IFC_STATISTICS) {
      (*(pC->pIdiLib->DivaSTraceSetLineStatistics))(pC->pIdiLib,
                                              (pC->hDbg->dbgMask & DIVA_MGT_DBG_IFC_STATISTICS) != 0);
		}
  }
}


void diva_mnt_internal_dprintf (dword drv_id, dword type, char* fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  DI_format (0, (word)drv_id, (int)type, fmt, ap);
  va_end(ap);
}

/*
  Write system information in the trace buffer
  This function can be called from the interrupt context.
  Also in case function is called from the interrupt context
  then write this information in the buffer header and set
  global flag.
	*/
void maint_write_system_info (unsigned long event, const char* info, int length)
{
	if (dbg_base != 0 && maint_tmp_buffer_base != 0) {

		if (info == 0 || length < 0) {
			if (diva_core_saved == 0 && external_dbg_queue == 0 && xdi_adapter_request == 0) {
				/*
					Signal to save MAINT buffer to adapter memory
					*/
				int i;
				IDI_CALL request;
				static IDI_SYNC_REQ sync_req;

				for (i = 0; i < sizeof(clients)/sizeof(clients[0]); i++) {
					if ((request = clients[i].request) != 0 && clients[i].hDbg != 0) {
						sync_req.xdi_save_maint_buffer_operation.Req = 0;
						sync_req.xdi_save_maint_buffer_operation.Rc  =
																	IDI_SYNC_REQ_XDI_SAVE_MAINT_BUFFER_OPERATION;
						sync_req.xdi_save_maint_buffer_operation.info.start   = (void*)dbg_base;
						sync_req.xdi_save_maint_buffer_operation.info.length  = dbg_base_length;
						sync_req.xdi_save_maint_buffer_operation.info.success = -1;
						(*request)((ENTITY*)&sync_req);
						if (sync_req.xdi_save_maint_buffer_operation.info.success == 0) {
							diva_core_saved = 1;
							break;
						}
					}
				}
			}
			return;
		}

		if (diva_os_in_irq() || diva_os_spinlocks_available() == 0) {
			/*
				This is not possible to access spin locks, also
				save information in the temporary buffer at start of the
				trace buffer
				*/
			int   pos  = maint_tmp_buffer_length;
			char* data = &maint_tmp_buffer_base[pos];
			int   to_copy;

			if (length == 0)
				length = strlen(info);

			if ((to_copy = MIN(length, (MAX_TMP_BUFFER_STRING - pos - 1))) < 1) {
/*
				Do not wrap around. In case of multiple exceptions only the first
				exception provides useful information.

				to_copy = MIN(length, (MAX_TMP_BUFFER_STRING - 1));
				data    = maint_tmp_buffer_base;
				pos     = 0;
*/
			} else {
				memcpy (data, info, to_copy);
				maint_tmp_buffer_length = pos + to_copy;
			}
		} else {
			int count;

			if (length == 0)
				length = strlen(info);

			while ((length > 0) && (
             (((char*)info)[length-1] == '\n') ||
             (((char*)info)[length-1] == '\r') ||
             (((char*)info)[length-1] ==  ' ')
            )) { length--; }


			if ((count = length) > 0) {
				diva_os_spin_lock_magic_t old_irql;
				dword sec, usec;

				diva_os_get_timestamp (&sec, &usec);

				diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "system_message");


				while (count > 0 && length > 0) {
					count   = diva_mnt_insert_message (sec, usec, info, length);
					info   += count;
					length -= count;
				}
				if (queueCount(dbg_queue)) {
					diva_maint_wakeup_read();
				}

				diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "system_message");
			}
		}
	}
}

/*
  Shutdown all adapters before driver removal
  */
int diva_mnt_shutdown_xdi_adapters (void) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  int i, fret = 0;
  byte * pmem;


  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    pmem = 0;

    diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "unload");
    diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "unload");

    if (clients[i].hDbg && clients[i].pIdiLib && clients[i].request) {
      if ((*(clients[i].pIdiLib->DivaSTraceLibraryStop))(clients[i].pIdiLib) == 1) {
        /*
          Adapter removal complete
          */
        if (clients[i].pIdiLib) {
          (*(clients[i].pIdiLib->DivaSTraceLibraryFinit))(clients[i].pIdiLib->hLib);
          clients[i].pIdiLib = 0;

          pmem = clients[i].pmem;
          clients[i].pmem = 0;
        }
        clients[i].hDbg    = 0;
        clients[i].request_pending = 0;

        if (clients[i].dma_handle >= 0) {
          /*
            Free DMA handle
            */
          diva_free_dma_descriptor (clients[i].request, clients[i].dma_handle);
          clients[i].dma_handle = -1;
				}
        clients[i].request = 0;
      } else {
        fret = -1;
      }
    }

    diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "unload");
    if (clients[i].hDbg && clients[i].pIdiLib && clients[i].request && clients[i].request_pending) {
      clients[i].request_pending = 0;
      (*(clients[i].request))((ENTITY*)(*(clients[i].pIdiLib->DivaSTraceGetHandle))(clients[i].pIdiLib->hLib));
      if (clients[i].dma_handle >= 0) {
        diva_free_dma_descriptor (clients[i].request, clients[i].dma_handle);
        clients[i].dma_handle = -1;
      }
    }
    diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "unload");

    if (pmem) {
      diva_os_free (0, pmem);
    }
  }

  return (fret);
}

/*
  Set/Read the trace filter used for selective tracing.
  Affects B- and Audio Tap trace mask at run time
  */
int diva_set_trace_filter (int filter_length, const char* filter) {
  diva_os_spin_lock_magic_t old_irql, old_irql1;
  int i, ch, on, client_b_on, client_atap_on;

  diva_os_enter_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");
  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "write_filter");

  if (filter_length <= DIVA_MAX_SELECTIVE_FILTER_LENGTH) {
    memcpy (&TraceFilter[0], filter, filter_length);
    if (TraceFilter[filter_length]) {
      TraceFilter[filter_length] = 0;
    }
    if (TraceFilter[0] == '*') {
      TraceFilter[0] = 0;
    }
  } else {
    filter_length = -1;
  }

  TraceFilterIdent   = -1;
  TraceFilterChannel = -1;

  on = (TraceFilter[0] == 0);

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].hDbg && clients[i].pIdiLib && clients[i].request) {
      client_b_on    = on && ((clients[i].hDbg->dbgMask & DIVA_MGT_DBG_IFC_BCHANNEL) != 0);
      client_atap_on = on && ((clients[i].hDbg->dbgMask & DIVA_MGT_DBG_IFC_AUDIO)    != 0);
      for (ch = 0; ch < clients[i].channels; ch++) {
        (*(clients[i].pIdiLib->DivaSTraceSetBChannel))(clients[i].pIdiLib->hLib, ch+1, client_b_on);
        (*(clients[i].pIdiLib->DivaSTraceSetAudioTap))(clients[i].pIdiLib->hLib, ch+1, client_atap_on);
      }
			(*(clients[i].pIdiLib->DivaSTraceSetMaxTraceRecordLength))(clients[i].pIdiLib->hLib, 30);
			if (on == 0) {
				clients[i].hDbg->dbgMask |= DIVA_MGT_DBG_LINE_EVENTS;
			} else {
				clients[i].hDbg->dbgMask &= ~DIVA_MGT_DBG_LINE_EVENTS;
			}
			(*(clients[i].pIdiLib->DivaSTraceSetLineState))(clients[i].pIdiLib->hLib,
															(clients[i].hDbg->dbgMask & DIVA_MGT_DBG_LINE_EVENTS) != 0);
    }
  }

  for (i = 1; i < (sizeof(clients)/sizeof(clients[0])); i++) {
    if (clients[i].hDbg && clients[i].pIdiLib && clients[i].request && clients[i].request_pending) {
      diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "write_filter");
      clients[i].request_pending = 0;
      (*(clients[i].request))((ENTITY*)(*(clients[i].pIdiLib->DivaSTraceGetHandle))(clients[i].pIdiLib->hLib));
      diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "write_filter");
    }
  }

  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "write_filter");
  diva_os_leave_spin_lock (&dbg_adapter_lock, &old_irql1, "dbg mask");

  return (filter_length);
}

int diva_get_trace_filter (int max_length, char* filter) {
  diva_os_spin_lock_magic_t old_irql;
  int len;

  diva_os_enter_spin_lock (&dbg_q_lock, &old_irql, "read_filter");
  len = strlen (&TraceFilter[0]) + 1;
  if (max_length >= len) {
    memcpy (filter, &TraceFilter[0], len);
  }
  diva_os_leave_spin_lock (&dbg_q_lock, &old_irql, "read_filter");

  return (len);
}

static int diva_dbg_cmp_key (const char* ref, const char* key) {
	while (*key && (*ref++ == *key++));
  return (!*key && !*ref);
}

/*
  In case trace filter starts with "C" character then
  all following characters are interpreted as command.
  Followings commands are available:
  - single, trace single call at time, independent from CPN/CiPN
  */
static int diva_mnt_cmp_nmbr (const char* nmbr) {
  const char* ref = &TraceFilter[0];
  int ref_len = strlen(&TraceFilter[0]), nmbr_len = strlen(nmbr);

  if (ref[0] == 'C') {
    if (diva_dbg_cmp_key (&ref[1], "single")) {
      return (0);
    }
    return (-1);
  }

  if (!ref_len || (ref_len > nmbr_len)) {
    return (-1);
  }

  nmbr = nmbr + nmbr_len - 1;
  ref  = ref  + ref_len  - 1;

  while (ref_len--) {
    if (*nmbr-- != *ref--) {
      return (-1);
    }
  }

  return (0);
}

static int diva_get_dma_descriptor (IDI_CALL request, dword *dma_magic) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

  if (!request) {
    return (-1);
  }

  pReq->xdi_dma_descriptor_operation.Req = 0;
  pReq->xdi_dma_descriptor_operation.Rc = IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

  pReq->xdi_dma_descriptor_operation.info.operation =     IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC;
  pReq->xdi_dma_descriptor_operation.info.descriptor_number  = -1;
  pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
  pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  (*request)((ENTITY*)pReq);

  if (!pReq->xdi_dma_descriptor_operation.info.operation &&
      (pReq->xdi_dma_descriptor_operation.info.descriptor_number >= 0) &&
      pReq->xdi_dma_descriptor_operation.info.descriptor_magic) {
    *dma_magic = pReq->xdi_dma_descriptor_operation.info.descriptor_magic;
    return (pReq->xdi_dma_descriptor_operation.info.descriptor_number);
  } else {
    return (-1);
  }
}

static void diva_free_dma_descriptor (IDI_CALL request, int nr) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

  if (!request || (nr < 0)) {
    return;
  }

  pReq->xdi_dma_descriptor_operation.Req = 0;
  pReq->xdi_dma_descriptor_operation.Rc = IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

  pReq->xdi_dma_descriptor_operation.info.operation = IDI_SYNC_REQ_DMA_DESCRIPTOR_FREE;
  pReq->xdi_dma_descriptor_operation.info.descriptor_number  = nr;
  pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
  pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  (*request)((ENTITY*)pReq);
}

/******************************************************************************/
dword diva_maint_get_bufsize() {
	return (dbg_queue ? dbg_queue->Size : 0) ;
}
/******************************************************************************/
