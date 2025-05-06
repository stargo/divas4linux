
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#if defined(CONFIG_DEVFS_FS)
#include <linux/devfs_fs_kernel.h>
#endif
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/poll.h>
#include <linux/kmod.h>

#include "platform.h"
#include "dlist.h"
#include "di_defs.h"
#include "divasync.h"
#include "cfg_types.h"
#include "cfg_notify.h"
#include "hsi_interface.h"
#include "debuglib.h"

MODULE_DESCRIPTION("Kernel driver for Dialogic Diva cards");
MODULE_AUTHOR("Dialogic");
MODULE_LICENSE("GPL");

/*
	IMPORTS
	*/
extern void DIVA_DIDD_Read (void *, int);

/*
	LOCALS
	*/
struct _diva_hsi_info;
static void no_printf(unsigned char *fmt, ...) { }
static void didd_callback(struct _diva_hsi_info* pHsi, DESCRIPTOR* adapter, int removal);
static int diva_hsi_cfg_changed (void* context, const byte* message, const byte* instance);
static int diva_hsi_activate (struct _diva_hsi_info* pHsi);
static int diva_hsi_deactivate (struct _diva_hsi_info* pHsi);
static void stop_dbg (struct _diva_hsi_info *pHsi);
static void diva_hsi_wakeup_read (struct _diva_hsi_info *pHsi);
static ssize_t diva_hsi_read  (struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t diva_hsi_write (struct file *file, const char *buf, size_t count, loff_t *ppos);
static unsigned int diva_hsi_poll (struct file *file, poll_table * wait);
static int diva_hsi_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int diva_hsi_open (struct inode *inode, struct file *file);
static int diva_hsi_release(struct inode *inode, struct file *file);
static int diva_hsi_bridge_init (struct _diva_hsi_info* pHsi, const HSI_BRIDGE_INIT_INPUT* info);
static int diva_hsi_bridge_stop (struct _diva_hsi_info* pHsi);
static int diva_hsi_enable (struct _diva_hsi_info* pHsi);
static int diva_hsi_disable (struct _diva_hsi_info* pHsi);
static int diva_hsi_check_response (struct _diva_hsi_info* pHsi);

DIVA_DI_PRINTF dprintf = no_printf;

static struct file_operations diva_hsi_fops = {
	.owner   = THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.llseek  = no_llseek,
#endif
	.read    = diva_hsi_read,
	.write   = diva_hsi_write,
	.poll    = diva_hsi_poll,
	.ioctl   = diva_hsi_ioctl,
	.open    = diva_hsi_open,
	.release = diva_hsi_release
};

typedef struct _diva_hsi_info {
	struct semaphore diva_hsi_lock;
	int      active;
	int      initialized;
	int      release;
	
	dword    logical_adapter_number; /* Logical adapter number */
	dword    adapter_port;           /* Port number, starting with zero */
	IDI_CALL adapter_request;
	IDI_SYNC_REQ req;
	DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

	unsigned int bridgeId; /* Unique ID assigned to Bridge Device by KMBC */
	HSI_ISR_CALLBACK isr_proc;
	dword nr_hsi_buffers;
	dword current_hsi_buffer;

	dword notify_handle;
	DESCRIPTOR DAdapter;
	DESCRIPTOR MAdapter;

	diva_cfg_lib_access_ifc_t* cfg_lib_ifc;
	void* diva_cfg_lib_notification_handle;

	int major;

	diva_entity_queue_t response_q;

	wait_queue_head_t   response_wait_q;

	rwlock_t              isr_spin_lock;
	struct tasklet_struct isr_tasklet;

	volatile dword count;
} diva_hsi_info_t;

typedef struct _diva_hsi_device_info {
	int      major;
	atomic_t state;
} diva_hsi_device_info_t;

diva_hsi_device_info_t diva_hsi_device;

typedef struct _diva_hsi_response {
	diva_entity_link_t link;
	dword              length;
  dword              cmd;
  union {
		HSI_BRIDGE_INIT_OUTPUT         init;
		HSI_BRIDGE_STOP_OUTPUT         stop;
		HSI_BRIDGE_INTR_ENABLE_OUTPUT  enable;
		HSI_BRIDGE_INTR_DISABLE_OUTPUT disable;
  } data;
} diva_hsi_response_t;

#define DEVNAME                                   "DivasHSI"
#define DIVA_HSI_LOGICAL_ADAPTER_NR_VARIABLE_NAME "Cfg\\LogicalAdapterNr"

/*
	Adapter insertion/removal notification
	*/
static void didd_callback (diva_hsi_info_t* pHsi, DESCRIPTOR * adapter, int removal) {
	if (adapter->type == IDI_DIMAINT) {
		if (removal == 0) {
			memcpy(&pHsi->MAdapter, adapter, sizeof(pHsi->MAdapter));
			dprintf = (DIVA_DI_PRINTF) pHsi->MAdapter.request;
			DbgRegister("DIVAS_HSI", "8.5", 0x197);
		} else {
			stop_dbg(pHsi);
		}
		return;
	}

	down (&pHsi->diva_hsi_lock);

	if (removal == 0) {
		if (adapter->request != 0 && adapter->channels != 0 && adapter->features != 0 &&
				adapter->type != IDI_MADAPTER && adapter->type < IDI_VADAPTER) {
			pHsi->req.xdi_logical_adapter_number.Req = 0;
			pHsi->req.xdi_logical_adapter_number.Rc  = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
			pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number = 0;
			(*(adapter->request))((ENTITY*)&pHsi->req);
			if (pHsi->logical_adapter_number == pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number) {
				pHsi->adapter_request = adapter->request;
				DBG_LOG(("add adapter %d", pHsi->logical_adapter_number))
				if (pHsi->active != 0) {
					diva_hsi_activate(pHsi);
				}
			}
		}
	} else {
		if (adapter->request != 0 && adapter->request == pHsi->adapter_request) {
			DBG_LOG(("remove adapter %d", pHsi->logical_adapter_number))
			if (pHsi->active != 0) {
				diva_hsi_deactivate(pHsi);
			}
			pHsi->adapter_request = 0;
		}
	}

	up(&pHsi->diva_hsi_lock);

}

static void diva_xdi_clock_isr_proc (const void* context, dword count) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)context;
	unsigned long flags;

  read_lock_irqsave(&pHsi->isr_spin_lock, flags);

	if (likely(pHsi->active != 0)) {
		if (unlikely(pHsi->isr_proc == (HSI_ISR_CALLBACK)0xabcdUL)) {
			pHsi->count = count;
			tasklet_schedule(&pHsi->isr_tasklet);
		} else {
			pHsi->current_hsi_buffer = (pHsi->current_hsi_buffer + 1) & 0x000000ffU;
			if (pHsi->current_hsi_buffer >= pHsi->nr_hsi_buffers)
				pHsi->current_hsi_buffer = 0;

			count |= ((pHsi->current_hsi_buffer << 24) & 0xff000000U);
			(*(pHsi->isr_proc)) (pHsi->bridgeId, (int)count);
		}
	}

  read_unlock_irqrestore(&pHsi->isr_spin_lock, flags);
}

/*
	Configuration is provided using logical adapter number.
	This is necessary to convert the configuration to logical adapter
	number and port number, where logical adapter number is the adapter
	number of the first port
	*/
static int diva_get_adapter_port (struct _diva_hsi_info* pHsi) {
	int ret = -ENODEV;

	pHsi->req.xdi_logical_adapter_number.Req = 0;
	pHsi->req.xdi_logical_adapter_number.Rc  = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
	pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number = 0;
	pHsi->req.xdi_logical_adapter_number.info.controller             = 0;
	pHsi->req.xdi_logical_adapter_number.info.total_controllers      = 0;
	(*(pHsi->adapter_request))((ENTITY*)&pHsi->req);
	if (pHsi->req.xdi_logical_adapter_number.info.total_controllers != 0) {
		if (pHsi->req.xdi_logical_adapter_number.info.controller != 0) {
			dword controllers = pHsi->req.xdi_logical_adapter_number.info.total_controllers;
			dword controller  = pHsi->req.xdi_logical_adapter_number.info.controller;
			dword serial_number;
			word  cardtype;
			int x;

			pHsi->req.GetSerial.Req = 0;
			pHsi->req.GetSerial.Rc = IDI_SYNC_REQ_GET_SERIAL;
			pHsi->req.GetSerial.serial = 0;
			(*(pHsi->adapter_request))((ENTITY*)&pHsi->req);
			serial_number = (pHsi->req.GetSerial.serial & 0x00ffffffff);

			pHsi->req.GetCardType.Req          = 0;
			pHsi->req.GetCardType.Rc           = IDI_SYNC_REQ_GET_CARDTYPE;
			pHsi->req.GetCardType.cardtype = 0;
			(*(pHsi->adapter_request))((ENTITY*)&pHsi->req);
			if ((cardtype = pHsi->req.GetCardType.cardtype) != 0) {
				/*
					Find the adapter with same card type, same number of controllers
					and with controller set to zero
					*/
				memset (pHsi->DIDD_Table, 0x00, sizeof(pHsi->DIDD_Table));
				DIVA_DIDD_Read (pHsi->DIDD_Table, sizeof(pHsi->DIDD_Table));

				for (x = 0; x < MAX_DESCRIPTORS; x++) {
					if (pHsi->DIDD_Table[x].request  != 0 &&
							pHsi->DIDD_Table[x].channels != 0 &&
							pHsi->DIDD_Table[x].features != 0 &&
							pHsi->DIDD_Table[x].type     != IDI_MADAPTER &&
							pHsi->DIDD_Table[x].type < IDI_VADAPTER) {
						dword nr;
						pHsi->req.xdi_logical_adapter_number.Req = 0;
						pHsi->req.xdi_logical_adapter_number.Rc  = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
						pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number = 0;
						pHsi->req.xdi_logical_adapter_number.info.controller             = 0;
						pHsi->req.xdi_logical_adapter_number.info.total_controllers      = 0;
						(*(pHsi->DIDD_Table[x].request))((ENTITY*)&pHsi->req);
						if ((nr = pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number) != 0 &&
								pHsi->req.xdi_logical_adapter_number.info.controller == 0 &&
								pHsi->req.xdi_logical_adapter_number.info.total_controllers == controllers) {
							pHsi->req.GetSerial.Req = 0;
							pHsi->req.GetSerial.Rc = IDI_SYNC_REQ_GET_SERIAL;
							pHsi->req.GetSerial.serial = 0;
							(*(pHsi->DIDD_Table[x].request))((ENTITY*)&pHsi->req);
							if ((pHsi->req.GetSerial.serial & 0x00ffffffff) == serial_number) {
								pHsi->req.GetCardType.Req          = 0;
								pHsi->req.GetCardType.Rc           = IDI_SYNC_REQ_GET_CARDTYPE;
								pHsi->req.GetCardType.cardtype = 0;
								(*(pHsi->DIDD_Table[x].request))((ENTITY*)&pHsi->req);
								if (pHsi->req.GetCardType.cardtype == cardtype) {
									DBG_LOG(("map %u to %u:%u", pHsi->logical_adapter_number, nr, controller))
									pHsi->adapter_request = pHsi->DIDD_Table[x].request;
									pHsi->adapter_port    = controller;
									pHsi->logical_adapter_number = nr;
									ret = 0;
									break;
								}
							}
						}
					}
				}
			}
		} else {
			pHsi->adapter_port = 0;
			ret = 0;
		}
	}

	return (ret);
}



/*
	Activate rate interrupt
	*/
static int diva_hsi_activate (struct _diva_hsi_info* pHsi) {
  int ret;

	if ((ret = diva_get_adapter_port (pHsi)) != 0) {
		return (ret);
	}

	pHsi->req.diva_xdi_clock_control_command_req.Req = 0;
  pHsi->req.diva_xdi_clock_control_command_req.Rc  = IDI_SYNC_REQ_XDI_CLOCK_CONTROL;
  memset (&pHsi->req.diva_xdi_clock_control_command_req.info,
          0x00,
          sizeof(pHsi->req.diva_xdi_clock_control_command_req.info));
  pHsi->req.diva_xdi_clock_control_command_req.info.command                      = 1;
  pHsi->req.diva_xdi_clock_control_command_req.info.data.on.port                 = pHsi->adapter_port;
  pHsi->req.diva_xdi_clock_control_command_req.info.data.on.isr_callback         = diva_xdi_clock_isr_proc;
  pHsi->req.diva_xdi_clock_control_command_req.info.data.on.isr_callback_context = pHsi;

  (*(pHsi->adapter_request))((ENTITY*)&pHsi->req);

  if (pHsi->req.diva_xdi_clock_control_command_req.Rc == 0xff) {
    DBG_LOG(("hsi interrupt activated"))
    ret = 0;
  } else {
    DBG_LOG(("hsi interrupt activation failed"))
    ret = -EIO;
  }

  return (ret);
}

/*
	Deactivate rate interrupt
	*/
static int diva_hsi_deactivate (struct _diva_hsi_info* pHsi) {
	int ret;

	pHsi->req.diva_xdi_clock_control_command_req.Req = 0;
	pHsi->req.diva_xdi_clock_control_command_req.Rc  = IDI_SYNC_REQ_XDI_CLOCK_CONTROL;
	memset (&pHsi->req.diva_xdi_clock_control_command_req.info,
					0x00,
					sizeof(pHsi->req.diva_xdi_clock_control_command_req.info));
	pHsi->req.diva_xdi_clock_control_command_req.info.command = 0;
	pHsi->req.diva_xdi_clock_control_command_req.info.data.off.port = pHsi->adapter_port;

	(*(pHsi->adapter_request))((ENTITY*)&pHsi->req);

	if (pHsi->req.diva_xdi_clock_control_command_req.Rc == 0xff) {
		DBG_LOG(("hsi interrupt deactivated"))
		ret = 0;
	} else {
		DBG_LOG(("hsi interrupt deactivation failed"))
		ret = -EIO;
	}

	return (ret);
}

/*
	Read configuration information
	*/
static int diva_hsi_cfg_changed (void* context, const byte* message, const byte* instance) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)context;
	int ret = -1;

	down (&pHsi->diva_hsi_lock);

	if (message == 0 && instance != 0) {
		const byte* var;
		dword logical_adapter_number;
		int x;

		if ((var = (*(pHsi->cfg_lib_ifc->diva_cfg_storage_find_variable_proc))(pHsi->cfg_lib_ifc,
						instance,
						DIVA_HSI_LOGICAL_ADAPTER_NR_VARIABLE_NAME,
						sizeof(DIVA_HSI_LOGICAL_ADAPTER_NR_VARIABLE_NAME)-1)) != 0) {
			(*(pHsi->cfg_lib_ifc->diva_cfg_lib_read_dword_value_proc))(var, &logical_adapter_number);
			DBG_REG(("adapter: %u", logical_adapter_number))

			pHsi->logical_adapter_number = logical_adapter_number;

			memset(pHsi->DIDD_Table, 0x00, sizeof(pHsi->DIDD_Table));
			DIVA_DIDD_Read(pHsi->DIDD_Table, sizeof(pHsi->DIDD_Table));

			for (x = 0; x < sizeof(pHsi->DIDD_Table)/sizeof(pHsi->DIDD_Table[0]); x++) {
				if (pHsi->DIDD_Table[x].request != 0 && pHsi->DIDD_Table[x].channels != 0 &&
						pHsi->DIDD_Table[x].features != 0 && pHsi->DIDD_Table[x].type != IDI_MADAPTER &&
						pHsi->DIDD_Table[x].type < IDI_VADAPTER) {
					pHsi->req.xdi_logical_adapter_number.Req = 0;
					pHsi->req.xdi_logical_adapter_number.Rc  = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
					pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number = 0;
					(*(pHsi->DIDD_Table[x].request))((ENTITY*)&pHsi->req);
					if (pHsi->logical_adapter_number ==
										pHsi->req.xdi_logical_adapter_number.info.logical_adapter_number) {
						pHsi->adapter_request = pHsi->DIDD_Table[x].request;
						DBG_LOG(("add adapter %d", pHsi->logical_adapter_number))
					}
				}
			}

			if (pHsi->active) {
				diva_hsi_deactivate (pHsi);
				diva_hsi_activate (pHsi);
			}

			ret = 0;
		}
	}

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

/*
	stop debug
	*/
static void stop_dbg (diva_hsi_info_t *pHsi) {
  DbgDeregister();
  memset(&pHsi->MAdapter, 0, sizeof(pHsi->MAdapter));
  dprintf = no_printf;
}


/*
	Connect to DIDD and to CfgLib
	*/
static int connect_didd (diva_hsi_info_t *pHsi) {
	int dadapter = 0;
	int x;

	memset (pHsi->DIDD_Table, 0x00, sizeof(pHsi->DIDD_Table));
	memset (&pHsi->req,       0x00, sizeof(pHsi->req));

  DIVA_DIDD_Read(pHsi->DIDD_Table, sizeof(pHsi->DIDD_Table));

  for (x = 0; x < MAX_DESCRIPTORS; x++) {
    if (pHsi->DIDD_Table[x].type == IDI_DADAPTER) { /* DADAPTER found */
      memcpy(&pHsi->DAdapter, &pHsi->DIDD_Table[x], sizeof(pHsi->DAdapter));
      pHsi->req.didd_notify.e.Req = 0;
      pHsi->req.didd_notify.e.Rc  = IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY;
      pHsi->req.didd_notify.info.callback = (void *)didd_callback;
      pHsi->req.didd_notify.info.context  = pHsi;
      (*(pHsi->DAdapter.request))((ENTITY*)&pHsi->req);
      if (pHsi->req.didd_notify.e.Rc != 0xff) {
        stop_dbg(pHsi);
				break;
      }
      pHsi->notify_handle = pHsi->req.didd_notify.info.handle;
      dadapter = 1;
    } else if (pHsi->DIDD_Table[x].type == IDI_DIMAINT) { /* MAINT found */
      memcpy(&pHsi->MAdapter, &pHsi->DIDD_Table[x], sizeof(pHsi->DAdapter));
      dprintf = (DIVA_DI_PRINTF)pHsi->MAdapter.request;
			DbgRegister("DIVAS_HSI", "8.5", 0x197);
    }
  }

  if (dadapter != 0) {
		/*
			Get CfgLib interface
			*/
		memset (&pHsi->req, 0x00, sizeof(pHsi->req));
		pHsi->req.didd_get_cfg_lib_ifc.e.Rc = IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC;
		(*(pHsi->DAdapter.request))((ENTITY*)&pHsi->req);
		if (pHsi->req.didd_get_cfg_lib_ifc.e.Rc == 0xff &&
				(pHsi->cfg_lib_ifc = pHsi->req.didd_get_cfg_lib_ifc.info.ifc) != 0) {
			/*
				Register notification callback
				*/
			pHsi->diva_cfg_lib_notification_handle =
				(*(pHsi->cfg_lib_ifc->diva_cfg_lib_cfg_register_notify_callback_proc))(
																													diva_hsi_cfg_changed,
																													pHsi,
																													TargetHSI);
			if (pHsi->diva_cfg_lib_notification_handle == 0) {
				DBG_ERR(("failed to register CfgLib notification callback"))
				(*(pHsi->cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(pHsi->cfg_lib_ifc);
				pHsi->cfg_lib_ifc = 0;
			}
		} else {
			DBG_LOG(("failed to retrieve CFGLib interface"))
		}

		if (pHsi->cfg_lib_ifc == 0) {
			pHsi->req.didd_notify.e.Req = 0;
			pHsi->req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
			pHsi->req.didd_notify.info.handle = pHsi->notify_handle;
			(*(pHsi->DAdapter.request))((ENTITY*)&pHsi->req);
			dadapter = 0;
		}
	}

	if (dadapter == 0) {
    stop_dbg (pHsi);
  }

  return ((dadapter != 0) ? 0 : -ENODEV);
}

static ssize_t diva_hsi_read  (struct file *file, char *buf, size_t count, loff_t *ppos) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)file->private_data;
	int ret;

	if (diva_hsi_check_response (pHsi) == 0) {
		if ((file->f_flags & O_NONBLOCK) == 0) {
			ret = wait_event_interruptible (pHsi->response_wait_q,
																			diva_hsi_check_response (pHsi) != 0);
		} else {
			ret = -EAGAIN;
		}
	} else {
		ret = 0;
	}

	if (!(ret < 0)) {
		if (pHsi->release == 0) {
			diva_hsi_response_t* response;

			down (&pHsi->diva_hsi_lock);

			if ((response = (diva_hsi_response_t*)diva_q_get_head (&pHsi->response_q)) != 0) {
				if (count >= response->length) {
					if (copy_to_user(buf, &response->data, response->length) == 0) {
						ret = response->length;
						diva_q_remove (&pHsi->response_q, &response->link);
						diva_os_free (0, response);
					} else {
						ret = -EFAULT;
					}
				} else {
					return (-EMSGSIZE);
				}
			} else {
				ret = -EIO;
			}

			up   (&pHsi->diva_hsi_lock);
		} else {
			ret = -EIO;
		}
	}

	return (ret);
}

static ssize_t diva_hsi_write (struct file *file, const char *buf, size_t count, loff_t *ppos) {
	return (-EINVAL);
}

static unsigned int diva_hsi_poll (struct file *file, poll_table * wait) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)file->private_data;

	poll_wait (file, &pHsi->response_wait_q, wait);

	if (diva_hsi_check_response (pHsi) != 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int diva_hsi_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)file->private_data;
	int ret;

	switch (cmd) {
		case HSI_BRIDGE_INIT_IOCTL: {
			HSI_BRIDGE_INIT_INPUT info;
			if (copy_from_user (&info, (void*)arg, sizeof(HSI_BRIDGE_INIT_INPUT)) == 0) {
				ret = diva_hsi_bridge_init (pHsi, &info);
			} else {
				ret = -EFAULT;
			}
		} break;

		case HSI_BRIDGE_STOP_IOCTL:
			ret = diva_hsi_bridge_stop (pHsi);
			break;

		case HSI_INTR_RATE_ENABLE_IOCTL:
			ret = diva_hsi_enable (pHsi);
			break;

		case HSI_INTR_RATE_DISABLE_IOCTL:
			ret = diva_hsi_disable (pHsi);
			break;

		default:
			ret = -EINVAL;
			break;
	}

	return (ret);
}

static void diva_hsi_tasklet_proc (unsigned long context) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)context;

	DBG_LOG(("clock %u", pHsi->count))
}

static int diva_hsi_open (struct inode *inode, struct file *file) {
	int ret = 0;

	if (atomic_inc_and_test(&(diva_hsi_device.state)) == 1) {
		diva_hsi_info_t* pHsi = diva_os_malloc (0, sizeof(*pHsi));

		if (pHsi != 0) {
			memset (pHsi, 0x00, sizeof(*pHsi));
			rwlock_init (&pHsi->isr_spin_lock);
			init_MUTEX (&pHsi->diva_hsi_lock);
			init_waitqueue_head (&pHsi->response_wait_q);
			tasklet_init (&pHsi->isr_tasklet, diva_hsi_tasklet_proc, (unsigned long)pHsi);
			diva_q_init (&pHsi->response_q);

			if (connect_didd (pHsi) == 0) {
				file->private_data = pHsi;
				DBG_TRC(("open"))
			} else {
				tasklet_kill(&pHsi->isr_tasklet);
				diva_os_free (0, pHsi);
				ret = -ENODEV;
			}
		} else {
			ret = -ENOMEM;
		}
	} else {
		ret = -EBUSY;
	}

	if (ret != 0) {
		atomic_add_negative(-1, &(diva_hsi_device.state));
	}

  return (ret);
}

static int diva_hsi_release (struct inode *inode, struct file *file) {
	diva_hsi_info_t* pHsi = (diva_hsi_info_t*)file->private_data;
	diva_hsi_response_t* response;

	down (&pHsi->diva_hsi_lock);

	if (pHsi->active != 0) {
		diva_hsi_deactivate(pHsi);
		pHsi->active = 0;
	}
	pHsi->adapter_request    = 0;
	file->private_data       = 0;

	pHsi->release = 1;

	while ((response = (diva_hsi_response_t*)diva_q_get_head (&pHsi->response_q)) != 0) {
		diva_q_remove (&pHsi->response_q, &response->link);
		diva_os_free (0, response);
	}

	diva_hsi_wakeup_read (pHsi);

	up (&pHsi->diva_hsi_lock);

	(*(pHsi->cfg_lib_ifc->diva_cfg_lib_cfg_remove_notify_callback_proc))(pHsi->diva_cfg_lib_notification_handle);
	(*(pHsi->cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(pHsi->cfg_lib_ifc);

  stop_dbg(pHsi);

  pHsi->req.didd_notify.e.Req = 0;
  pHsi->req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
  pHsi->req.didd_notify.info.handle = pHsi->notify_handle;
  (*(pHsi->DAdapter.request))((ENTITY*)&pHsi->req);

	tasklet_kill(&pHsi->isr_tasklet);

	diva_os_free (0, pHsi);

	DBG_TRC(("close"))

	atomic_add_negative(-1, &(diva_hsi_device.state));

	return (0);
}

static int diva_hsi_bridge_init (diva_hsi_info_t* pHsi, const HSI_BRIDGE_INIT_INPUT* info) {
	HSI_BRIDGE_INIT_INPUT ref;
	int ret;

	down(&pHsi->diva_hsi_lock);

	if (pHsi->initialized == 0) {
		memset (&ref, 0x00, sizeof(ref));
		ref.logicalId  = info->logicalId;
		ref.bridgeId   = info->bridgeId;
		ref.tickRate   = HSI_RATE_2MS;
		ref.startType  = HSI_START_ON_INTR_ENABLE;
		ref.hmpIsrFunc = info->hmpIsrFunc;
		ref.busType    = info->busType;
		ref.lawIdlePattern = info->lawIdlePattern;
		ref.portAllocCode = info->portAllocCode;
		ref.numHsiBuffers = info->numHsiBuffers;
		memcpy (ref.syncPattern,  info->syncPattern, sizeof(ref.syncPattern));

		if (memcmp (&ref, info, sizeof(ref)) == 0) {
			diva_hsi_response_t* response = (diva_hsi_response_t*)diva_os_malloc (0, sizeof(*response));

			if (response != 0) {
				pHsi->bridgeId = info->bridgeId;
				pHsi->isr_proc = info->hmpIsrFunc;
				pHsi->initialized = 1;
				pHsi->nr_hsi_buffers = info->numHsiBuffers;
				pHsi->current_hsi_buffer = 0xffffffff;

				DBG_LOG(("init bridge Id %08x, isr proc:%p", pHsi->bridgeId, pHsi->isr_proc))

				response->length = sizeof(HSI_BRIDGE_INIT_OUTPUT);
				response->cmd    = 0;
				response->data.init.type           = HSI_BRIDGE_INIT_COMPLETION;
				response->data.init.bridgeId       = pHsi->bridgeId;
				response->data.init.txDelay        = 0;
				response->data.init.rxDelay        = 0;
				response->data.init.txSyncDelay    = 0;
				response->data.init.rxSyncDelay    = 0;

				diva_q_add_tail (&pHsi->response_q, &response->link);
				diva_hsi_wakeup_read (pHsi);

				ret = 0;
			} else {
				ret = -ENOMEM;
			}
		} else {
			DBG_ERR(("init wrong value (provided:expected)"))
			if (info->logicalId != ref.logicalId) {
				DBG_ERR(("logicalId %u %u", info->logicalId, ref.logicalId))
			}
			if (info->bridgeId != ref.bridgeId) {
				DBG_ERR(("bridgeId  %u %u", info->bridgeId, ref.bridgeId))
			}
			if (info->numTotalPorts != ref.numTotalPorts) {
				DBG_ERR(("numTotalPorts  %u %u", info->numTotalPorts, ref.numTotalPorts))
			}
			if (info->numVoicePorts != ref.numVoicePorts) {
				DBG_ERR(("numVoicePorts  %u %u", info->numVoicePorts, ref.numVoicePorts))
			}
			if (info->numDataPorts != ref.numDataPorts) {
				DBG_ERR(("numDataPorts  %u %u", info->numDataPorts, ref.numDataPorts))
			}
			if (info->numHsiBuffers != ref.numHsiBuffers) {
				DBG_ERR(("portAllocCode  %u %u", info->numHsiBuffers, ref.numHsiBuffers))
			}
			if (info->tickRate != ref.tickRate) {
				DBG_ERR(("tickRate  %u %u", info->tickRate, ref.tickRate))
			}
			if (info->startType != ref.startType) {
				DBG_ERR(("startType  %u %u", info->startType, ref.startType))
			}
			if (info->busType != ref.busType) {
				DBG_ERR(("busType  %u %u", info->busType, ref.busType))
			}
			if (info->lawIdlePattern != ref.lawIdlePattern) {
				DBG_ERR(("lawIdlePattern  %08x %08x", info->lawIdlePattern, ref.lawIdlePattern))
			}
			if (info->syncTimeslot != ref.syncTimeslot) {
				DBG_ERR(("syncTimeslot  %u %u", info->syncTimeslot, ref.syncTimeslot))
			}
			if (memcmp (info->syncPattern, ref.syncPattern, sizeof(ref.syncPattern)) != 0) {
				DBG_ERR(("syncPattern  %02x%02x%02x%02x %02x%02x%02x%02x",
								info->syncPattern[0], info->syncPattern[1], info->syncPattern[2], info->syncPattern[3],
								ref.syncPattern[0], ref.syncPattern[1], ref.syncPattern[2], ref.syncPattern[3]))
			}
			if (info->hmpIsrFunc != ref.hmpIsrFunc) {
				DBG_ERR(("hmpIsrFunc  %p %p", info->hmpIsrFunc, ref.hmpIsrFunc))
			}
			if (info->hmpSyncComplete != ref.hmpSyncComplete) {
				DBG_ERR(("hmpIsrFunc  %p %p", info->hmpSyncComplete, ref.hmpSyncComplete))
			}
			if (memcmp (info->hostToBoardBuffers, ref.hostToBoardBuffers, sizeof(ref.hostToBoardBuffers)) != 0) {
				int i;

				for (i = 0; i < HSI_MAX_BUFFERS; i++) {
					if (info->hostToBoardBuffers != ref.hostToBoardBuffers) {
						DBG_ERR(("hostToBoardBuffers[%d]  %p %p",
										i, info->hostToBoardBuffers[i], ref.hostToBoardBuffers[i]))
					}
				}

				for (i = 0; i < HSI_MAX_BUFFERS; i++) {
					if (info->boardToHostBuffers != ref.boardToHostBuffers) {
						DBG_ERR(("hostToBoardBuffers[%d]  %p %p",
										i, info->boardToHostBuffers[i], ref.boardToHostBuffers[i]))
					}
				}
			}

			ret = -EINVAL;
		}
	} else {
		ret = -EBUSY;
	}

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

static int diva_hsi_bridge_stop (diva_hsi_info_t* pHsi) {
	int ret;

	down(&pHsi->diva_hsi_lock);

	if (pHsi->initialized != 0) {
		if (pHsi->active == 0) {
			diva_hsi_response_t* response = (diva_hsi_response_t*)diva_os_malloc (0, sizeof(*response));

			if (response != 0) {
				response->length = sizeof(HSI_BRIDGE_STOP_OUTPUT);
				response->cmd    = 0;
				response->data.stop.type     = HSI_BRIDGE_STOP_COMPLETION;
				response->data.stop.bridgeId = pHsi->bridgeId;

				pHsi->bridgeId = 0;
				pHsi->isr_proc = 0;
				pHsi->initialized = 0;

				diva_q_add_tail (&pHsi->response_q, &response->link);
				diva_hsi_wakeup_read (pHsi);

				ret = 0;
			} else {
				ret = -ENOMEM;
			}
		} else {
			ret = -EBUSY;
		}
	} else {
		ret = -EINVAL;
	}

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

static void diva_hsi_wakeup_read (diva_hsi_info_t *pHsi) {
	wake_up_interruptible(&pHsi->response_wait_q);
}

static int diva_hsi_check_response (diva_hsi_info_t* pHsi) {
	int ret;

	down(&pHsi->diva_hsi_lock);

	ret = pHsi->release != 0 || diva_q_get_head (&pHsi->response_q) != 0;

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

static int diva_hsi_enable (diva_hsi_info_t* pHsi) {
	int ret;

	down(&pHsi->diva_hsi_lock);

	if (pHsi->initialized != 0) {
		if (pHsi->active == 0) {
			if (pHsi->adapter_request != 0) {
				diva_hsi_response_t* response = (diva_hsi_response_t*)diva_os_malloc (0, sizeof(*response));

				if (response != 0) {
					if ((ret = diva_hsi_activate (pHsi)) == 0) {
						unsigned long flags;

						write_lock_irqsave(&pHsi->isr_spin_lock, flags);
						pHsi->active = 1;
						write_unlock_irqrestore(&pHsi->isr_spin_lock, flags);

						response->length = sizeof(HSI_BRIDGE_INTR_ENABLE_OUTPUT);
						response->cmd    = 0;
						response->data.enable.type     = HSI_BRIDGE_INTR_ENABLE_COMPLETION;
						response->data.enable.bridgeId = pHsi->bridgeId;
						diva_q_add_tail (&pHsi->response_q, &response->link);
						diva_hsi_wakeup_read (pHsi);
					} else {
						diva_os_free (0, response);
					}
				} else {
					ret = -ENOMEM;
				}
			} else {
				ret = -ENODEV;
			}
		} else {
			ret = -EIO;
		}
	} else {
		ret = -EIO;
	}

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

static int diva_hsi_disable (diva_hsi_info_t* pHsi) {
	int ret;

	down(&pHsi->diva_hsi_lock);

	if (pHsi->initialized != 0) {
		if (pHsi->active != 0) {
			diva_hsi_response_t* response = (diva_hsi_response_t*)diva_os_malloc (0, sizeof(*response));

			if (response != 0) {
				if (pHsi->adapter_request != 0) {
					ret = diva_hsi_deactivate (pHsi);
				} else {
					ret = 0;
				}

				if (ret == 0) {
						unsigned long flags;

						write_lock_irqsave(&pHsi->isr_spin_lock, flags);
            pHsi->active = 0;
						write_unlock_irqrestore(&pHsi->isr_spin_lock, flags);

            response->length = sizeof(HSI_BRIDGE_INTR_DISABLE_OUTPUT);
            response->cmd    = 0;
            response->data.disable.type     = HSI_BRIDGE_INTR_DISABLE_COMPLETION;
            response->data.disable.bridgeId = pHsi->bridgeId;

            diva_q_add_tail (&pHsi->response_q, &response->link);
            diva_hsi_wakeup_read (pHsi);
				} else {
					diva_os_free (0, response);
				}
			} else {
				ret = -ENOMEM;
			}
		} else {
			ret = -EIO;
		}
	} else {
		ret = -EIO;
	}

	up(&pHsi->diva_hsi_lock);

	return (ret);
}

#include "debuglib.c"

/* --------------------------------------------------------------------------
    Driver Load / Startup
   -------------------------------------------------------------------------- */
static int DIVA_INIT_FUNCTION diva_hsi_init (void) {
	int ret = 0;

	atomic_set(&diva_hsi_device.state, -1);

	if ((diva_hsi_device.major = register_chrdev(0, DEVNAME, &diva_hsi_fops)) < 0) {
		ret = -EIO;
	}

	return (ret);
}

/* --------------------------------------------------------------------------
    Driver Unload
   -------------------------------------------------------------------------- */
static void DIVA_EXIT_FUNCTION diva_hsi_exit(void) {
	unregister_chrdev(diva_hsi_device.major, DEVNAME);
}


module_init(diva_hsi_init);
module_exit(diva_hsi_exit);

