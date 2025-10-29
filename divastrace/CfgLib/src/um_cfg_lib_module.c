
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
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

#include "platform.h"
#include "debug.h"
#include "dlist.h"
#include "io_user_ifc.h"
#include "cfg_types.h"
#include "cfg_notify.h"
#include "cfg_ifc.h"
#ifdef BOARDCONF
#include "os.h"
#endif //BOARDCONF

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <errno.h>



struct _diva_um_cfg_lib_module;
struct _diva_um_cfg_lib_io_device;

typedef struct _diva_um_cfg_lib_io_device {
	diva_io_object_user_ifc_t ifc;
	void                 *context;
	struct _diva_um_cfg_lib_module* cfg_lib;

	/*
		Private variables
		*/
	struct _diva_um_cfg_lib_module* cfg_lib_module;
	int fd;
	int initialized;
	int error;

} diva_um_cfg_lib_io_device_t;

typedef struct _diva_um_cfg_lib_module {
  diva_io_object_user_enum_ifc_t ifc;
  int                            created;
  diva_um_cfg_lib_io_device_t    cfg_lib_device;
  int                            use_ioctl;
	struct _diva_cfg_lib_access_ifc* cfg_lib_ifc;
	void (*diva_cfg_lib_free_cfg_interface_proc)(struct _diva_cfg_lib_access_ifc* ifc);
	dword owner;
} diva_um_cfg_lib_module_t;

static diva_um_cfg_lib_module_t diva_cfg_lib_module;

static int diva_create_cfg_lib_device (diva_um_cfg_lib_module_t* pM, dword owner);
static int diva_um_cfg_lib_process_device_messages (diva_um_cfg_lib_module_t* pM,
                                                    diva_um_cfg_lib_io_device_t* pI);
static int diva_um_cfg_lib_device_ready (struct _diva_io_object_user_ifc* hIfc,
                                      int readin, int write, int status, int error);

static void diva_um_cfg_lib_free_cfg_interface (struct _diva_cfg_lib_access_ifc* ifc) {
	diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (pI->error == 0) {
		dbg_msg(DBG_ERROR, "CfgLib error at %d", __LINE__);
		pI->error = __LINE__;
	}
}

static int diva_cfg_lib_ipc (struct _diva_io_object_user_enum_ifc* enum_ifc) {
	return (0);
}

static int diva_cfg_lib_free_io_ifc (struct _diva_io_object_user_enum_ifc* enum_ifc) {
	diva_um_cfg_lib_module_t* pM = (diva_um_cfg_lib_module_t*)enum_ifc->user_context;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (pM->created != 0 && pI->initialized != 0 && pI->error != 0) {
		(*(pI->ifc.release_proc))(&pI->ifc);
	}

	return (0);
}

static int diva_cfg_lib_nr_io_devices (struct _diva_io_object_user_enum_ifc* enum_ifc) {
	diva_um_cfg_lib_module_t* pM = (diva_um_cfg_lib_module_t*)enum_ifc->user_context;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	int ret = (pM->created != 0 && pI->initialized != 0 && pI->error == 0);

	return (ret);
}

static diva_io_object_user_ifc_t* diva_cfg_lib_enum_io_devices (struct _diva_io_object_user_enum_ifc* enum_ifc,
																														 diva_io_object_user_ifc_t* ident) {
	diva_um_cfg_lib_module_t* pM = (diva_um_cfg_lib_module_t*)enum_ifc->user_context;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (pM->created != 0 && pI->initialized != 0 && pI->error == 0 && ident == 0) {
		return (&pI->ifc);
	}

	return (0);
}

static int diva_cfg_lib_module_ifc_release (struct _diva_io_object_user_enum_ifc* enum_ifc) {
	diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;

	if (pM->created != 0) {
		(*(pM->ifc.shutdown_complete))(&pM->ifc);
		pM->created = 0;
	}

	return (0);
}

static int diva_module_start_shutdown (struct _diva_io_object_user_enum_ifc* enum_ifc) {
  diva_um_cfg_lib_module_t* pM = (diva_um_cfg_lib_module_t*)enum_ifc->user_context;
  diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

  if (pM->created != 0 && pI->initialized != 0) {
    (*(pI->ifc.release_proc))(&pI->ifc);
		(*(pM->diva_cfg_lib_free_cfg_interface_proc))(pM->cfg_lib_ifc);
		pM->cfg_lib_ifc = 0;
  }

  return (0);
}

static int diva_cfg_lib_module_shutdown_complete (struct _diva_io_object_user_enum_ifc* enum_ifc) {
	diva_um_cfg_lib_module_t* pM = (diva_um_cfg_lib_module_t*)enum_ifc->user_context;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (pM->created != 0 && pI->initialized != 0) {
		(*(pI->ifc.release_proc))(&pI->ifc);
		(*(pM->diva_cfg_lib_free_cfg_interface_proc))(pM->cfg_lib_ifc);
		pM->cfg_lib_ifc = 0;
	}

	return (0);
}

diva_io_object_user_enum_ifc_t* diva_um_cfg_lib_init (dword owner,
																											struct _diva_cfg_lib_access_ifc** ifc) {
	diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;

	*ifc = 0;

	if (pM->created != 0)
		return (0);

	memset (pM, 0x00, sizeof(*pM));


	if ((pM->cfg_lib_ifc = diva_um_cfg_lib_get_cfg_interface (owner)) == 0) {
		return (0);
	}

	pM->ifc.user_context      = pM;
	pM->ifc.release_proc      = diva_cfg_lib_module_ifc_release;
	pM->ifc.get               = diva_cfg_lib_enum_io_devices;
	pM->ifc.nr                = diva_cfg_lib_nr_io_devices;
	pM->ifc.free_ifc_proc     = diva_cfg_lib_free_io_ifc;
	pM->ifc.ipc_proc          = diva_cfg_lib_ipc;
	pM->ifc.start_shutdown    = diva_module_start_shutdown;
	pM->ifc.shutdown_complete = diva_cfg_lib_module_shutdown_complete;
	pM->owner                 = owner;

	if (pM->created == 0 && diva_create_cfg_lib_device (pM, owner) == 0) {
		pM->created = 1;
	} else {
		(*(pM->cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(pM->cfg_lib_ifc);
		pM->cfg_lib_ifc = 0;
		return (0);
	}

	/*
		Save and replace free interface function
		*/
	pM->diva_cfg_lib_free_cfg_interface_proc = pM->cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc;
	pM->cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc = diva_um_cfg_lib_free_cfg_interface;

	/*
		Read initial configuration from CfgLib
		*/
	{
		int nr = diva_um_cfg_lib_process_device_messages (pM, &pM->cfg_lib_device);

		dbg_msg (DBG_INFO, "CfgLib initialization complete, read %d configuration messages", nr);
	}

	*ifc = pM->cfg_lib_ifc;


	return (&pM->ifc);
}

static int diva_um_cfg_lib_register_owner (dword owner) {
  int fd;

  if ((fd = open ("/dev/DivasDIDD", O_RDWR | O_NONBLOCK)) < 0) {
    fd =  open ("/proc/net/isdn/eicon/divadidd", O_RDWR | O_NONBLOCK);
  }

  if (fd >= 0) {
    if (write(fd, &owner, sizeof(owner)) == sizeof(owner)) {
      return (fd);
    }
  }

  return (-1);
}

static int diva_um_cfg_lib_device_release_proc (struct _diva_io_object_user_ifc* hIfc) {
  diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;
  diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

  if (pI->initialized != 0) {
		close (pI->fd);
		pI->initialized = 0;
  }

  return (0);
}

static DIVA_OS_HANDLE diva_um_cfg_lib_device_get_read_handle (struct _diva_io_object_user_ifc* hIfc) {
  diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;
  diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

  if (pI->initialized != 0 && pI->error == 0) {
    return ((DIVA_OS_HANDLE)pI->fd);
  }

  return (DIVA_OS_HANDLE)(DIVA_OS_INVALID_HANDLE);
}

static DIVA_OS_HANDLE diva_um_cfg_lib_device_no_handle (struct _diva_io_object_user_ifc* hIfc) {
	return (DIVA_OS_HANDLE)(DIVA_OS_INVALID_HANDLE);
}

/*
  Initialize OS independent code and register management interface
  with DIDD
  */
static int diva_create_cfg_lib_device (diva_um_cfg_lib_module_t* pM, dword owner) {
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (pI->initialized == 0) {
		if ((pI->fd = diva_um_cfg_lib_register_owner (owner)) >= 0) {
			/*
				Set up I/O abstraction level interface
				*/
			pI->initialized            = 1;
			pI->cfg_lib_module         = pM;
			pI->ifc.user_context       = pI;
			pI->ifc.release_proc       = diva_um_cfg_lib_device_release_proc;
			pI->ifc.read_handle_proc   = diva_um_cfg_lib_device_get_read_handle;
			pI->ifc.write_handle_proc  = diva_um_cfg_lib_device_no_handle;
			pI->ifc.state_change_proc  = diva_um_cfg_lib_device_no_handle;
			pI->ifc.message_input_proc = diva_um_cfg_lib_device_ready;

			return (0);
		}
	}

  return (-1);
}

static const byte* diva_um_cfg_lib_read_instance_data (int fd) {
  int max_length = 4*1024 + 1, length;
  byte* data = 0;

  for (;;) {
    if (data != 0)
      diva_os_free (0, data);
    if ((data = diva_os_malloc (0, max_length)) == 0)
      return (0);

    if ((length = read (fd, data, max_length-1)) > 0) {
      data[length] = 0;
      return (data);
    }
    if (length <= 0 && errno != EMSGSIZE) {
      diva_os_free (0, data);
      return (0);
    }
    max_length += 1024;
  }

  return (0);
}

static int diva_um_cfg_lib_process_device_messages (diva_um_cfg_lib_module_t* pM,
                                                    diva_um_cfg_lib_io_device_t* pI) {
	int processed_messages = 0;
	const byte* data;

	while (pI->error == 0 && (data = diva_um_cfg_lib_read_instance_data (pI->fd)) != 0) {
		dword ret = diva_um_cfg_lib_message_input (pM->cfg_lib_ifc, data);

		if (write (pI->fd, &ret, sizeof(ret)) == sizeof(ret)) {
			processed_messages++;
		} else {
			dbg_msg(DBG_ERROR, "CfgLib error at %d", __LINE__);
			pI->error = __LINE__;
		}
	}

	return (processed_messages);
}

static int diva_um_cfg_lib_device_ready (struct _diva_io_object_user_ifc* hIfc,
																	 int readin, int write_info, int status, int error) {
	diva_um_cfg_lib_module_t* pM = &diva_cfg_lib_module;
	diva_um_cfg_lib_io_device_t* pI = &pM->cfg_lib_device;

	if (error || write_info || status || !readin || pI->error != 0) {
		if (pI->error == 0) {
			dbg_msg(DBG_ERROR, "CfgLib error at %d", __LINE__);
			pI->error = __LINE__;
		}
		return (-1);
	}

	if (diva_um_cfg_lib_process_device_messages (pM, pI) == 0 || pI->error != 0) {
		dbg_msg(DBG_ERROR, "CfgLib error at %d", __LINE__);
		pI->error = __LINE__;
	}

	return ((pI->error == 0) ? 0 : -1);
}

