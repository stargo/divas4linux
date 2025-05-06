
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
#include "debug_if.h"
#include "um_dbg.h"

extern char *DRIVERRELEASE_MNT;

#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)

extern void DIVA_DIDD_Read(void *, int);

static dword notify_handle;
static DESCRIPTOR DAdapter;
static DESCRIPTOR MAdapter;
static DESCRIPTOR MaintDescriptor =
    { IDI_DIMAINT, 0, 0, (IDI_CALL) diva_maint_prtComp };

extern void *diva_os_malloc_tbuffer(unsigned long flags,
				    unsigned long size);
extern void diva_os_free_tbuffer(unsigned long flags, void *ptr);
extern int diva_os_copy_to_user(void *os_handle, void *dst,
				const void *src, int length);
extern int diva_os_copy_from_user(void *os_handle, void *dst,
				  const void *src, int length);

static int maint_um_dbg_read_write_data (void** info, const void* buf, int count);
static int maint_um_dbg_write_string    (void** info, const void* buf, int count);

static void no_printf(unsigned char *x, ...)
{
	/* dummy debug function */
}
DIVA_DI_PRINTF dprintf = no_printf;

#include "debuglib.c"

/*
 *  DIDD callback function
 */
static void didd_callback(void *context, DESCRIPTOR * adapter,
			   int removal)
{
	if (adapter->type == IDI_DADAPTER) {
		DBG_ERR(("cb: Change in DAdapter ? Oops ?."));
	} else if (adapter->type == IDI_DIMAINT) {
		if (removal) {
      DbgDeregister();
      memset(&MAdapter, 0, sizeof(MAdapter));
      dprintf = no_printf;
		} else {
			memcpy(&MAdapter, adapter, sizeof(MAdapter));
			dprintf = (DIVA_DI_PRINTF) MAdapter.request;
			DbgRegister("MAINT", DRIVERRELEASE_MNT, DBG_DEFAULT);
		}
	} else if ((adapter->type > 0) && (adapter->type < 16)) {
		if (removal) {
			diva_mnt_remove_xdi_adapter(adapter);
		} else {
			diva_mnt_add_xdi_adapter(adapter);
		}
	}
}

static IDI_CALL DIVA_INIT_FUNCTION get_dadapter_request (void) {
	DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];
	int x = 0;

	DIVA_DIDD_Read(DIDD_Table, sizeof(DIDD_Table));

	for (x = 0; x < MAX_DESCRIPTORS; x++) {
		if (DIDD_Table[x].type == IDI_DADAPTER) {	/* DADAPTER found */
			return (DIDD_Table[x].request);
		}
	}

	return (0);
}

/*
 * connect to didd
 */
static int DIVA_INIT_FUNCTION connect_didd(void)
{
	int x = 0;
	int dadapter = 0;
	IDI_SYNC_REQ req;
	DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

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
			if (req.didd_notify.e.Rc != 0xff)
				return (0);
			notify_handle = req.didd_notify.info.handle;
			/* Register MAINT (me) */
			req.didd_add_adapter.e.Req = 0;
			req.didd_add_adapter.e.Rc =
			    IDI_SYNC_REQ_DIDD_ADD_ADAPTER;
			req.didd_add_adapter.info.descriptor =
			    (void *) &MaintDescriptor;
			DAdapter.request((ENTITY *) & req);
			if (req.didd_add_adapter.e.Rc != 0xff)
				return (0);
		} else if ((DIDD_Table[x].type > 0)
			   && (DIDD_Table[x].type < 16)) {
			diva_mnt_add_xdi_adapter(&DIDD_Table[x]);
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
	DAdapter.request((ENTITY *) & req);

	req.didd_remove_adapter.e.Req = 0;
	req.didd_remove_adapter.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER;
	req.didd_remove_adapter.info.p_request =
	    (IDI_CALL) MaintDescriptor.request;
	DAdapter.request((ENTITY *) & req);
}

/*
 * read/write maint
 */
int maint_read_write(void *buf, int count)
{
	byte data[128];
	dword cmd, id, mask;
	int ret = 0;

	if (count < (3 * sizeof(dword)))
		return (-EFAULT);

	if (diva_os_copy_from_user(NULL, (void *) &data[0],
				   buf, 3 * sizeof(dword))) {
		return (-EFAULT);
	}

	cmd = *(dword *) & data[0];	/* command */
	id = *(dword *) & data[4];	/* driver id */
	mask = *(dword *) & data[8];	/* mask or size */

	switch (cmd) {
	case DITRACE_CMD_GET_DRIVER_INFO:
		if ((ret = diva_get_driver_info(id, data, sizeof(data))) > 0) {
			if ((count < ret) || diva_os_copy_to_user
			    (NULL, buf, (void *) &data[0], ret))
				ret = -EFAULT;
		} else {
			ret = -EINVAL;
		}
		break;

	case DITRACE_READ_DRIVER_DBG_MASK:
		if ((ret = diva_get_driver_dbg_mask(id, (byte *) data)) > 0) {
			if ((count < ret) || diva_os_copy_to_user
			    (NULL, buf, (void *) &data[0], ret))
				ret = -EFAULT;
		} else {
			ret = -ENODEV;
		}
		break;

	case DITRACE_WRITE_DRIVER_DBG_MASK:
		if ((ret = diva_set_driver_dbg_mask(id, mask)) <= 0) {
			ret = -ENODEV;
		}
		break;

    /*
       Filter commans will ignore the ID due to fact that filtering affects
       the B- channel and Audio Tap trace levels only. Also MAINT driver will
       select the right trace ID self
       */
    case DITRACE_WRITE_SELECTIVE_TRACE_FILTER:
      if (!mask) {
        ret = diva_set_trace_filter (1, "*");
      } else if (mask < sizeof(data)) {
        if (copy_from_user((void *)&data[0], (void *)(((byte*)buf)+12), mask)) {
          ret = -EFAULT;
        } else {
          ret = diva_set_trace_filter ((int)mask, data);
        }
      } else {
        ret = -EINVAL;
      }
      break;

    case DITRACE_READ_SELECTIVE_TRACE_FILTER:
      if ((ret = diva_get_trace_filter (sizeof(data), data)) > 0) {
        if (copy_to_user ((void*)buf, (void*)&data[0], ret))
          ret = -EFAULT;
      } else {
        ret = -ENODEV;
      }
      break;

	case DITRACE_READ_TRACE_ENTRY:{
			diva_os_spin_lock_magic_t old_irql;
			word size;
			diva_dbg_entry_head_t *pmsg;
			byte *pbuf;

			if ((pmsg = diva_maint_get_message(&size, &old_irql))) {
				if (size > mask) {
					diva_maint_ack_message(0, &old_irql);
					ret = -EINVAL;
				} else {
					if (!(pbuf = diva_os_malloc_tbuffer(0, size)))
					{
						diva_maint_ack_message(0, &old_irql);
						ret = -ENOMEM;
					} else {
						ret = size;
						memcpy(pbuf, pmsg, size);
						diva_maint_ack_message(1, &old_irql);
						if ((count < size) || diva_os_copy_to_user (NULL, buf,
						     (void *) pbuf, size))
							ret = -EFAULT;
						diva_os_free_tbuffer(0, pbuf);
					}
				}
			} else {
				ret = 0;
			}
		}
		break;

	case DITRACE_READ_TRACE_ENTRYS:{
			diva_os_spin_lock_magic_t old_irql;
			word size;
			diva_dbg_entry_head_t *pmsg;
			byte *pbuf = 0;
			int written = 0;

			if (mask < 4096) {
				ret = -EINVAL;
				break;
			}
			if (!(pbuf = diva_os_malloc_tbuffer(0, mask))) {
				return (-ENOMEM);
			}

			for (;;) {
				if (!(pmsg =
				     diva_maint_get_message(&size, &old_irql))) {
					break;
				}
				if ((size + 8) > mask) {
					diva_maint_ack_message(0, &old_irql);
					break;
				}
				/*
				   Write entry length
				 */
				pbuf[written++] = (byte) size;
				pbuf[written++] = (byte) (size >> 8);
				pbuf[written++] = 0;
				pbuf[written++] = 0;
				/*
				   Write message
				 */
				memcpy(&pbuf[written], pmsg, size);
				diva_maint_ack_message(1, &old_irql);
				written += size;
				mask -= (size + 4);
			}
			pbuf[written++] = 0;
			pbuf[written++] = 0;
			pbuf[written++] = 0;
			pbuf[written++] = 0;

			if ((count < written) || diva_os_copy_to_user(NULL, buf, (void *) pbuf, written)) {
				ret = -EFAULT;
			} else {
				ret = written;
			}
			diva_os_free_tbuffer(0, pbuf);
		}
		break;

	default:
		ret = -EINVAL;
	}
	return (ret);
}

typedef struct _diva_mtpx_um_client {
	_DbgHandle_ hDbg;
	int (*maint_um_dbg_read_write_proc)(void** info, const void* buf, int count);
} diva_mtpx_um_client_t;

static void dump_dli_data (_DbgHandle_ *hDbg, int dli_name, char* format, ...) {
	va_list ap;

	va_start(ap, format);
	hDbg->dbg_prt (hDbg->id, dli_name, format, ap);
  va_end (ap);
}

/*
  Used by user mode applications to register and to write
  debug messages to MAINT driver
  */
int maint_um_dbg_read_write (void** info, const void* buf, int count) {
	switch ((long)*info) {
		case 0L:
			return (-EINVAL);

		case 1L:
			return (maint_um_dbg_read_write_data (info, buf, count));
	}

	return (((diva_mtpx_um_client_t*)(*info))->maint_um_dbg_read_write_proc (info, buf, count));
}

/*
  Used by user mode applications which supports access to MAINT driver
	to register and to write debug messages to MAINT driver
	*/
static int maint_um_dbg_read_write_data (void** info, const void* buf, int count) {
	void* ident = *info;
	byte cmd;

	if (count == -1 && buf == 0) {
		if (ident != 0 && ident != (void*)1L) {
			diva_mtpx_um_client_t* pC = (diva_mtpx_um_client_t*)ident;
			DbgDeregisterH (&pC->hDbg);
			diva_os_free (0, ident);
			*info = (void*)1L;
		}
		return (0);
	}

	if (count < 5 || ident == 0)
		return (-EINVAL);

	if (diva_os_copy_from_user(NULL, (void *)&cmd, buf, 1))
		return (-EFAULT);

	if (ident == (void*)1L) {
		if (cmd == DIVA_UM_IDI_TRACE_CMD_REGISTER && count > 8) {
			diva_mtpx_um_client_t* pC;
			byte data[128];
			int i;
			size_t __size = sizeof(*pC), *psize = &__size;

			if (diva_os_copy_from_user(NULL, (void*)&data[0], (void*)(((byte*)buf)+1), MIN(count-1,sizeof(data)-1)))
				return (-EFAULT);

			data[sizeof(data)-1] = 0;
			if ((i = strlen ((char*)&data[4])) == 0 || i >= sizeof(data)-2)
				return (-EINVAL);
			i += 5;

			if ((pC = (diva_mtpx_um_client_t*)diva_os_malloc (0, *psize)) != 0) {
				memset (pC, 0x00, sizeof(*pC));
				if (DbgRegisterH (&pC->hDbg, &data[4], &data[i], READ_DWORD(data)) == 0) {
					DbgDeregisterH (&pC->hDbg);
					diva_os_free (0, pC);
					return (-ENOMEM);
				}
				pC->maint_um_dbg_read_write_proc = maint_um_dbg_read_write_data;
				*info = (void*)pC;
				return (count);
			}
			return (-ENOMEM);
		}
		return (-EINVAL);
	}


	if (cmd == DIVA_UM_IDI_TRACE_CMD_WRITE && count > 3) {
		diva_mtpx_um_client_t* pC = (diva_mtpx_um_client_t*)ident;
		byte data[257+2];
		word dli, length;
		int dli_string_format = 0;

		if (diva_os_copy_from_user(NULL, (void*)&data[0], (void*)(((byte*)buf)+1), MIN(count-1, sizeof(data)-1)))
			return (-EFAULT);

		dli = READ_WORD(&data[0]);
		switch (dli) {
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
				dli_string_format = 1;
				break;

			case DLI_BLK :
			case DLI_SEND:
			case DLI_RECV:
			case DLI_MXLOG:
				break;

			default:
				return (-EINVAL);
		}

		length = MIN(count - 3, 256);
		data[length+2] = 0;

		if (dli_string_format != 0) {
			dump_dli_data (&pC->hDbg, dli, "%s", &data[2]);
		} else {
			dump_dli_data (&pC->hDbg, dli, &data[2], length);
		}

		return (count);
	}

	if (cmd == DIVA_UM_IDI_TRACE_CMD_SET_MASK && count == 5) {
		diva_mtpx_um_client_t* pC = (diva_mtpx_um_client_t*)ident;
		byte data[4];

		if (diva_os_copy_from_user(NULL, (void*)&data[0], (void*)(((byte*)buf)+1), 4))
			return (-EFAULT);

		DbgSetLevelH (&pC->hDbg, READ_DWORD(data));

		return (count);
	}

	return (-EINVAL);
}

void* mntfunc_add_debug_client (const char* name, const char* version) {
	diva_mtpx_um_client_t* pC = (diva_mtpx_um_client_t*)diva_os_malloc (0, sizeof(*pC));

	if (pC != 0) {
		memset (pC, 0x00, sizeof(*pC));
		pC->maint_um_dbg_read_write_proc = maint_um_dbg_write_string;
		if (DbgRegisterH (&pC->hDbg, (char*)name, (char*)version, DL_LOG | DL_FTL | DL_ERR) == 0) {
			diva_os_free (0, pC);
			pC = 0;
		}
	}

	return (pC);
}

static void debug_client_write_string (struct _DbgHandle_* pDbg, const char* format, ...) {
	va_list ap;

	va_start (ap, format);
	(pDbg->dbg_prt)(pDbg->id, DLI_LOG, (char*)format, ap);
	va_end (ap);
}

static int maint_um_dbg_write_string (void** info, const void* buf, int length) {
	diva_mtpx_um_client_t* pC = (diva_mtpx_um_client_t*)*info;
	struct _DbgHandle_ *pDbg  = &pC->hDbg;
	const char* data = (const char*)buf;
	int count = length;

	if (length == -1 && data == 0) {
		DbgDeregisterH (pDbg);
		diva_os_free (0, pC);
		*info = 0;
		return (0);
	}

	if ((pDbg->dbgMask & ((unsigned long)DL_LOG)) != 0) {
		char tmp[255];
		int to_write, i;

		while (length != 0) {
			i = to_write = MIN(length, (sizeof(tmp)-1));
			if (copy_from_user((void *)&tmp[0], (void *)data, to_write)) {
				return (-1);
			}
			while (to_write > 0 && (tmp[to_write-1] == '\n' || tmp[to_write-1] == '\r' || tmp[to_write-1] == 0)) {
				to_write--;
			}
			if (to_write > 0) {
				tmp[to_write] = 0;
				debug_client_write_string (pDbg, "%s", tmp);
			}
			length -= i;
			data   += i;
		}
	}

	return (count);
}

void diva_os_read_descriptor_array (void* t, int length) {
	DIVA_DIDD_Read (t, length);
}

/*
 *  init
 */
int DIVA_INIT_FUNCTION mntfunc_init(int *buffer_length, void **buffer,
				    unsigned long diva_dbg_mem)
{
	if (*buffer_length < 64) {
		*buffer_length = 64;
	}
	if (*buffer_length > 512 && diva_dbg_mem == 0) {
		*buffer_length = 512;
	}
	*buffer_length *= 1024;

	if (diva_dbg_mem) {
		*buffer = (void *) diva_dbg_mem;
	} else {
		while ((*buffer_length >= (64 * 1024))
		       &&
		       (!(*buffer = diva_os_malloc (0, *buffer_length)))) {
			*buffer_length -= 1024;
		}

		if (!*buffer) {
			DBG_ERR(("init: Can not alloc trace buffer"));
			return (0);
		}
	}

	if (diva_maint_init_and_connect_to_cfg_lib (*buffer,
																							*buffer_length,
																							(diva_dbg_mem == 0),
																							get_dadapter_request())) {
		if (!diva_dbg_mem) {
			diva_os_free (0, *buffer);
		}
		DBG_ERR(("init: maint init failed"));
		return (0);
	}

	if (!connect_didd()) {
		DBG_ERR(("init: failed to connect to DIDD."));
		diva_maint_finit();
		if (!diva_dbg_mem) {
			diva_os_free (0, *buffer);
		}
		return (0);
	}
	return (1);
}

/*
 *  exit
 */
void DIVA_EXIT_FUNCTION mntfunc_finit(void)
{
	void *buffer;
	int i = 100;

  DbgDeregister();

	while (diva_mnt_shutdown_xdi_adapters() && i--) {
		diva_os_sleep(10);
	}

	disconnect_didd();

	if ((buffer = diva_maint_finit())) {
		diva_os_free (0, buffer);
	}

  memset(&MAdapter, 0, sizeof(MAdapter));
  dprintf = no_printf;
}
