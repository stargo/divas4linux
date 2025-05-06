
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include "diva.h"
#include "pc_maint.h"
#include "xdi_msg.h"
#include "diva_dma.h"
#include "dlist.h"
#include "cfg_types.h"
#include <poll.h>
#include "di_defs.h"
#include "pc.h"
#include "divasync.h"

#define DIVA_SDRAM_WRITE_BLOCK_SZ	(64*1024)
extern int card_number;

typedef int (*divas_get_card_properti_proc_t)(dword card, int fd, void* data);
typedef struct _diva_get_card_property_proc_t {
	const char* name;
	divas_get_card_properti_proc_t get_proc;
	int length;
} diva_get_card_property_proc_t;

static void sig_handler (int sig_nr) { }

static int divas_get_vidi_info (dword card, int fd, void* data);
static int divas_get_vidi_mode (dword card, int fd, void* data);
static int divas_get_clock_data (dword card, int fd, void* data);
static int divas_get_hardware_info_struct (dword card, int fd, void* data);
static int divas_get_card_serial_number (dword card, int fd, void* data);
static int divas_get_card_xlog	(dword card, int fd, void* data);
static int divas_get_pci_hw_config(dword card, int fd, void* str);
static int divas_get_card_state	(dword card, int fd, void* data);
static int divas_get_software_features (dword card, int fd, void* data);
static int card_number_relates_to_handle (int use_ioctl, int card_number, dword handle);
static int divas_get_plx_read (dword card, int fd, void* data);
static int divas_get_io_device_read (dword card, int fd, void* data);
static int divas_get_plx_write (dword card, int fd, void* data);
static int divas_get_io_device_write (dword card, int fd, void* data);
static int divas_get_clock_interrupt_control (dword card, int fd, void* data);
static int divas_get_clock_interrupt_data (dword card, int fd, void* data);

static diva_get_card_property_proc_t card_properties[] = {
{"clock_interrupt_data", divas_get_clock_interrupt_data, 255 },
{"clock_interrupt_control", divas_get_clock_interrupt_control,  255 },
{"plx_read",      divas_get_plx_read,  255 },
{"io_read",      divas_get_io_device_read,  255 },
{"plx_write",     divas_get_plx_write, 255 },
{"io_write",      divas_get_io_device_write,  255 },
{"vidi_info",     divas_get_vidi_info, sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t)},
{"vidi_mode",     divas_get_vidi_mode, sizeof(dword)},
{"clock_data",    divas_get_clock_data, sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t)},
{"hw_info_struct",divas_get_hardware_info_struct,	60},
{"serial number", divas_get_card_serial_number,		64},
{"xlog",					divas_get_card_xlog,					sizeof(struct mi_pc_maint)},
{"pci hw config",	divas_get_pci_hw_config,			8*11+5},
{"card state",		divas_get_card_state,					64},
{"software features",	divas_get_software_features, 32},
{ 0,              0,                            0,}
};

static int diva_divas_uses_ioctl;

dword
divas_open_driver (int card_number)
{
	int fd;

  if (((fd = open("/dev/Divas",  O_RDWR|O_NONBLOCK)) > 0) &&
			flock (fd, LOCK_EX) == 0 &&
      card_number_relates_to_handle(0, card_number, fd)         ) {
		flock (fd, LOCK_UN);
    diva_divas_uses_ioctl = 0;
    return ((dword)fd);
  }
  else
  if (((fd = open("/proc/net/isdn/eicon/divas",  O_RDWR|O_NONBLOCK)) > 0) &&
      card_number_relates_to_handle(1, card_number, fd)         ) {
    diva_divas_uses_ioctl = 1;
    return ((dword)fd);
  }
  else
  if (((fd = open("/dev/DivasP", O_RDWR|O_NONBLOCK)) > 0) &&
      card_number_relates_to_handle(0, card_number, fd)         ) {
    diva_divas_uses_ioctl = 0;
    return ((dword)fd);
  }
  else
  if (((fd = open("/proc/net/isdn/eicon/divasp", O_RDWR|O_NONBLOCK)) > 0) &&
      card_number_relates_to_handle(1, card_number, fd)         ) {
    diva_divas_uses_ioctl = 1;
    return ((dword)fd);
  }
  else
  if (((fd = open("/dev/DivasC", O_RDWR|O_NONBLOCK)) > 0) &&
      card_number_relates_to_handle(0, card_number, fd)         ) {
    diva_divas_uses_ioctl = 0;
    return ((dword)fd);
  }
  else
  if (((fd = open("/proc/net/isdn/eicon/divasc", O_RDWR|O_NONBLOCK)) > 0) &&
      card_number_relates_to_handle(1, card_number, fd)         ) {
    diva_divas_uses_ioctl = 1;
    return ((dword)fd);
  }
  return (DIVA_INVALID_FILE_HANDLE);
}

int
divas_close_driver (dword handle)
{
	if (handle != DIVA_INVALID_FILE_HANDLE) {
		close ((int)handle);
		return (0);
	}
	return (-1);
}

/* --------------------------------------------------------------------------
   Verifies if opened handle accepts card number
   returns 0 if not accepted, returns 1 if accepted
   -------------------------------------------------------------------------- */
static int
card_number_relates_to_handle (int use_ioctl, int card_number, dword handle)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return 0;
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_GET_CARD_ORDINAL;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (use_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		divas_close_driver (handle);
  		return 0;
  	}
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
  		divas_close_driver (handle);
  		return 0;
  	}
  }
	memset (&msg, 0x00, sizeof(msg));

  if (use_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
  		divas_close_driver (handle);
  		return 0;
  	}
  } else {
    if (read ((int)handle, &msg, sizeof(msg)) != sizeof(dword)) {
  		divas_close_driver (handle);
  		return 0;
    }
  }
	return 1;
}

int divas_get_card (int card_number)
{
  dword handle = divas_open_driver (card_number);
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    return (-1);
  }
  if (diva_divas_uses_ioctl == 0) {
    if (flock ((int)handle, LOCK_EX) != 0) {
      divas_close_driver (handle);
      return (-1);
    }
  }

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card_number;
  msg.command = DIVA_XDI_UM_CMD_GET_CARD_ORDINAL;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  } else {
   if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  }
  memset (&msg, 0x00, sizeof(msg));

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
      divas_close_driver (handle);
      return (-1);
    }
  } else {
    if (read ((int)handle, &msg, sizeof(msg)) != sizeof(dword)) {
      divas_close_driver (handle);
      return (-1);
    }
  }
  divas_close_driver (handle);
  return ((int)(*(dword*)&msg));
}

int divas_get_revision_id (int card_number)
{
  dword handle = divas_open_driver (card_number);
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    return  -1;
  }
  if (!diva_divas_uses_ioctl) {
    if (flock ((int)handle, LOCK_EX) != 0) {
      divas_close_driver (handle);
      return  -1;
    }
  }

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card_number;
  msg.command = DIVA_XDI_UM_CMD_GET_PCI_REVISION_ID;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl) {
    if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      divas_close_driver (handle);
      return -1;
    }
  } else {
   if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
      divas_close_driver (handle);
      return -1;
    }
  }
  memset (&msg, 0x00, sizeof(msg));

  if (diva_divas_uses_ioctl) {
    if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != 1) {
      divas_close_driver (handle);
      return -1;
    }
  } else {
    if (read ((int)handle, &msg, sizeof(msg)) != 1) {
      divas_close_driver (handle);
      return -1;
    }
  }
  divas_close_driver (handle);
  return (int)(*(byte*)&msg);
}

/* --------------------------------------------------------------------------
		Recovery different card propertoes
	 -------------------------------------------------------------------------- */

int
divas_get_card_properties (int card_number,
                           const char* name,
                           char* buffer,
                           int   size)
{
	int i, ret;
	dword handle;

	for (i = 0; card_properties[i].name; i++) {
		if (!strcmp (name, card_properties[i].name)) {
			if (card_properties[i].length > size) {
				return (-1);
			}
			if (!buffer) {
				return (card_properties[i].length);
			}
			if ((handle = divas_open_driver (card_number)) == DIVA_INVALID_FILE_HANDLE) {
				return (-1);
			}
			if (diva_divas_uses_ioctl == 0) {
				if (flock ((int)handle, LOCK_EX) != 0) {
					divas_close_driver (handle);
					return (-1);
				}
			}

			ret = (*(card_properties[i].get_proc))(card_number, (int)handle, buffer);

			divas_close_driver (handle);
			return (ret);
		}
	}

	return (-1);
}


static int
divas_get_vidi_mode (dword card, int fd, void* data) {
	diva_xdi_um_cfg_cmd_t msg;
	diva_xdi_io_cmd iomsg;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_SET_VIDI_MODE;

	msg.command_data.vidi_mode.vidi_mode = *(dword*)data;

	iomsg.length = sizeof(msg);
	iomsg.cmd = (void*)&msg;

	if (diva_divas_uses_ioctl != 0) {
		if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
			*(dword*)data = 0;
			return (-1);
		}
	} else {
		if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
			*(dword*)data = 0;
			return (-1);
		}
	}

	return (sizeof(dword));
}

/* --------------------------------------------------------------------------
    Return vidi info struct in buffer (parameter)
   -------------------------------------------------------------------------- */
static int
divas_get_vidi_info (dword card, int fd, void* data)
{
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_INIT_VIDI;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }

  memset (data, 0x00, sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t));
  iomsg.length = sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t);
  iomsg.cmd    = data;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t)) {
      return (-1);
    }
  } else {
    if (read (fd, data, sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t)) !=
                              sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t)) {
      return (-1);
    }
  }

  return 1;
}

dword divas_read_bar_dword (dword handle, byte bar, dword offset, int* info) {
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	dword dst[2];
	int tmp;

	if (info == 0)
		info = &tmp;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_BAR_ACCESS;
	msg.command_data.bar_access_data.bar = 0;
	msg.command_data.bar_access_data.access_type = DIVA_BAR_ACCESS_TYPE_READ | DIVA_BAR_ACCESS_TYPE_SINGLE;
	msg.command_data.bar_access_data.length = 4;
	msg.command_data.bar_access_data.offset = offset;

	iomsg.length = sizeof(msg);
	iomsg.cmd    = (void*)&msg;

	if (diva_divas_uses_ioctl != 0) {
		if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
			*info |= -1;
			return (0xffffffff);
		}
	} else {
		if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
			*info |= -1;
			return (0xffffffff);
		}
	}

	iomsg.length = 8;
	iomsg.cmd = (void *) dst;

	if (diva_divas_uses_ioctl != 0) {
		if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != 8) {
			*info |= -1;
			return (0xffffffff);
		}
	} else {
		if (read ((int)handle, dst, 8) != 8) {
			*info |= -1;
			return (0xffffffff);
		}
	}


	if (dst[0] != 4) {
		*info |= -1;
		return (0xffffffff);
	}

	*info = 0;

	return (dst[1]);
}

int divas_write_bar_dword (dword handle, byte bar, dword offset, dword data) {
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	dword dst[2];

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_BAR_ACCESS;
	msg.command_data.bar_access_data.bar = 0;
	msg.command_data.bar_access_data.access_type = DIVA_BAR_ACCESS_TYPE_WRITE | DIVA_BAR_ACCESS_TYPE_SINGLE;
	msg.command_data.bar_access_data.length = 4;
	msg.command_data.bar_access_data.offset = offset;
	msg.command_data.bar_access_data.data   = data;

	iomsg.length = sizeof(msg);
	iomsg.cmd    = (void*)&msg;

	if (diva_divas_uses_ioctl != 0) {
		if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
			return (-1);
		}
	} else {
		if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
			return (-1);
		}
	}

	iomsg.length = 8;
	iomsg.cmd = (void *) dst;

	if (diva_divas_uses_ioctl != 0) {
		if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != 8) {
			return (-1);
		}
	} else {
		if (read ((int)handle, dst, 8) != 8) {
			return (-1);
		}
	}


	return ((dst[0] == 0) ? 0 : -1);
}


static int divas_get_plx_read (dword card, int fd, void* data) {
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	int ret;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_READ_WRITE_PLX_REGISTER;
	msg.command_data.plx_register.write  = 0;
	msg.command_data.plx_register.length = 4;
	msg.command_data.plx_register.offset = (byte)atoi((char*)data);
	msg.command_data.plx_register.value  = 0;


	*(char*)data = 0;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }


  memset (&msg, 0x00, sizeof(msg));
  iomsg.length = sizeof(msg.command_data.plx_register);
  iomsg.cmd    = &msg.command_data.plx_register;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(msg.command_data.plx_register)) {
      return (-1);
    }
  } else {
    if (read (fd, &msg.command_data.plx_register, sizeof(msg.command_data.plx_register)) !=
                              sizeof(msg.command_data.plx_register)) {
      return (-1);
    }
  }

	switch (msg.command_data.plx_register.length) {
		case 1:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%02x",
											msg.command_data.plx_register.offset,
											(byte)msg.command_data.plx_register.value);
			break;

		case 2:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%04x",
											msg.command_data.plx_register.offset,
											(word)msg.command_data.plx_register.value);
			break;

		default:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%08x",
											msg.command_data.plx_register.offset,
											msg.command_data.plx_register.value);
			break;
	}

	return (ret);
}

static int divas_get_io_device_read (dword card, int fd, void* data) {
	dword offset = (dword)atoi((char*)data) & ~3U;
	int err = 0, ret = -1;
	dword v = divas_read_bar_dword (fd, 0, offset, &err);

	if (err == 0) {
		ret = snprintf ((char*)data, 0xff, "IO[%08x]=%08x", offset, v);
	}

	return (ret);
}

static int divas_get_io_device_write (dword card, int fd, void* data) {
	int err = 0, ret = -1;
	char* start = (char*)data, *next;
	dword offset, value;

	offset = (dword)strtoul(start, &next, 0);
	if (next == start || next == 0 || *next != ':') {
		return (-1);
	}
	start = next + 1;
	value = (dword)strtoul (start, &next, 0);
	if (next == start || next == 0) {
		return (-1);
	}

	offset &= ~3U;

	if (divas_write_bar_dword (fd, 0, offset, value) != 0) {
		return (-1);
	}

	value = divas_read_bar_dword (fd, 0, offset, &err);

	if (err == 0) {
		ret = snprintf ((char*)data, 0xff, "IO[%08x]=%08x", offset, value);
	}

	return (ret);
}

static int divas_get_plx_write (dword card, int fd, void* data) {
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	int ret;
	char* start = (char*)data, *next;
	dword offset;
	dword value;
	dword width = 4;

	offset = (dword)strtoul(start, &next, 0);
	if (next == start || next == 0 || *next != ':') {
		return (-1);
	}
	start = next + 1;
	value = (dword)strtoul (start, &next, 0);
	if (next == start || next == 0) {
		return (-1);
	}
	if (*next != 0) {
		if (*next == ':') {
			start = next + 1;
			width = (dword)strtoul (start, &next, 0);
			if (next == start || next == 0 || *next != 0) {
				return (-1);
			}
		} else {
			return (-1);
		}
	}


  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_READ_WRITE_PLX_REGISTER;
	msg.command_data.plx_register.write  = 1;
	msg.command_data.plx_register.length = (byte)width;
	msg.command_data.plx_register.offset = (byte)offset;
	msg.command_data.plx_register.value  = value;

	*(char*)data = 0;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }


  memset (&msg, 0x00, sizeof(msg));
  iomsg.length = sizeof(msg.command_data.plx_register);
  iomsg.cmd    = &msg.command_data.plx_register;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(msg.command_data.plx_register)) {
      return (-1);
    }
  } else {
    if (read (fd, &msg.command_data.plx_register, sizeof(msg.command_data.plx_register)) !=
                              sizeof(msg.command_data.plx_register)) {
      return (-1);
    }
  }

	switch (msg.command_data.plx_register.length) {
		case 1:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%02x",
											msg.command_data.plx_register.offset,
											(byte)msg.command_data.plx_register.value);
			break;

		case 2:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%04x",
											msg.command_data.plx_register.offset,
											(word)msg.command_data.plx_register.value);
			break;

		default:
			ret = snprintf ((char*)data, 0xff, "PLX[%02x]=%08x",
											msg.command_data.plx_register.offset,
											msg.command_data.plx_register.value);
			break;
	}

	return (ret);
}

static int divas_get_clock_interrupt_control (dword card, int fd, void* data) {
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_CONTROL;
  msg.command_data.clock_interrupt_control.command = (byte)atoi((char*)data);

  *(char*)data = 0;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }

	strcpy ((char*)data, "OK");
	return (strlen((char*)data));
}

static int divas_get_clock_interrupt_data (dword card, int fd, void* data) {
	diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t clock_interrupt_data;
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_DATA;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }

	*(char*)data = 0;
	memset (&clock_interrupt_data, 0x00, sizeof(clock_interrupt_data));
  iomsg.length = sizeof(clock_interrupt_data);
  iomsg.cmd    = &clock_interrupt_data;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd,
               DIVA_XDI_IO_CMD_READ_MSG,
               (ulong)&iomsg) != sizeof(clock_interrupt_data)) {
      return (-1);
    }
  } else {
    if (read (fd, &clock_interrupt_data, sizeof(clock_interrupt_data)) !=
                              sizeof(clock_interrupt_data)) {
      return (-1);
    }
  }

	sprintf (data, "%s:%u clock:%u errors:%u",
					 clock_interrupt_data.state != 0 ? "active" : "deactivated",
					 clock_interrupt_data.state,
					 clock_interrupt_data.clock,
					 clock_interrupt_data.errors);

  return 1;
}

/* --------------------------------------------------------------------------
    Return clock data struct in buffer (parameter)
   -------------------------------------------------------------------------- */
static int
divas_get_clock_data (dword card, int fd, void* data)
{
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)card;
  msg.command = DIVA_XDI_UM_CMD_GET_CLOCK_MEMORY_INFO;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      return (-1);
    }
  }

  memset (data, 0x00, sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t));
  iomsg.length = sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t);
  iomsg.cmd    = data;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd,
               DIVA_XDI_IO_CMD_READ_MSG,
               (ulong)&iomsg) != sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t)) {
      return (-1);
    }
  } else {
    if (read (fd, data, sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t)) !=
                              sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t)) {
      return (-1);
    }
  }

  return 1;
}

/* --------------------------------------------------------------------------
		Return hardware info struct in buffer (parameter)
	 -------------------------------------------------------------------------- */
static int
divas_get_hardware_info_struct (dword card, int fd, void* data)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_GET_HW_INFO_STRUCT;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		return (-1);
  	}
  } else {
	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
  		return (-1);
  	}
  }
	memset (data, 0xff, 60);
  iomsg.length = 60;
  iomsg.cmd    = data;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
		  return (-1);
	  }
  } else {
	  if (read (fd, data, 60) != 60) {
		  return (-1);
	  }
  }

	return 1;
}

/* --------------------------------------------------------------------------
		Return serial number of the card as ASCII string
	 -------------------------------------------------------------------------- */
static int
divas_get_card_serial_number (dword card, int fd, void* data)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_GET_SERIAL_NR;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		return (-1);
  	}
  } else {
	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
  		return (-1);
  	}
  }
	memset (&msg, 0x00, sizeof(msg));

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
		  return (-1);
	  }
  } else {
	  if (read (fd, &msg, sizeof(msg)) != sizeof(dword)) {
		  return (-1);
	  }
  }

	return (sprintf ((char*)data, "%d", *(dword*)&msg)+1);
}

static int
divas_get_pci_hw_config (dword card, int fd, void* str)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	dword data[9];

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_GET_PCI_HW_CONFIG;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  return (-1);
	  }
  } else {
	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
		  return (-1);
	  }
  }

	memset (&data, 0x00, sizeof(data));

  iomsg.length = sizeof(data);
  iomsg.cmd = (void *) &data;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(data)) {
		  return (-1);
	  }
  } else {
	  if (read (fd, data, sizeof(data)) != sizeof(data)) {
		  return (-1);
	  }
  }

	return (sprintf ((char*)str,
		"0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%02x",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6],
		data[7], data[8])+1);
}

/* --------------------------------------------------------------------------
		Issue one XLOG request and read one XLOG buffer from the card
	 -------------------------------------------------------------------------- */
static int
divas_get_card_xlog (dword card, int fd, void* data)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	int lock_fd = -1;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_READ_XLOG_ENTRY;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  return (-1);
	  }
  } else {
		char lock_file_name[128];

		snprintf (lock_file_name, sizeof(lock_file_name), "%s.%d", "/var/lock/Diva", card);
		lock_file_name[sizeof(lock_file_name)-1] = 0;
		lock_fd = open (lock_file_name, O_CREAT | O_RDWR, __S_IREAD|__S_IWRITE);

		if (lock_fd >= 0) {
			flock (fd, LOCK_UN);
			if (flock (lock_fd, LOCK_EX) != 0) {
				if (flock (fd, LOCK_EX) != 0) {
					close (lock_fd);
					return (-1);
				}
			}
		}

	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
		  return (-1);
	  }
  }

	memset (data, 0x00, sizeof(struct mi_pc_maint));

  iomsg.length = sizeof(struct mi_pc_maint);
  iomsg.cmd = data;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(struct mi_pc_maint)) {
			if (lock_fd >= 0)
				close (lock_fd);
		  return (-1);
	  }
  } else {
	  if (read (fd, data, sizeof(struct mi_pc_maint)) != sizeof(struct mi_pc_maint)) {
			if (lock_fd >= 0)
				close (lock_fd);
		  return (-1);
	  }
  }

	if (lock_fd >= 0)
		close (lock_fd);

	return (sizeof(struct mi_pc_maint));
}

void*
diva_os_malloc (unsigned long flags, unsigned long size)
{
	void* ret = malloc (size);

	if (ret) {
		memset (ret, 0x00, size);
	}

	return (ret);
}

void
diva_os_free (unsigned long flags, void* ptr)
{
	if (ptr) {
		free (ptr);
	}
}

/* --------------------------------------------------------------------------
		START BOOTLOADER
	 -------------------------------------------------------------------------- */
int
divas_start_bootloader (int card_number)
{
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_RESET_ADAPTER;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		divas_close_driver (handle);
  		return (-1);
  	}
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
  		divas_close_driver (handle);
  		return (-1);
  	}
  }

	divas_close_driver (handle);

	return (0);
}

/* --------------------------------------------------------------------------
		WRITE CARD MEMORY
	 -------------------------------------------------------------------------- */
int
divas_ram_write (int card_number,
                 int ram_number,
                 const void* buffer,
                 dword offset,
                 dword length)
{
	dword handle = divas_open_driver (card_number);
	char tmp[DIVA_SDRAM_WRITE_BLOCK_SZ+sizeof(diva_xdi_um_cfg_cmd_t)];
  diva_xdi_io_cmd iomsg;
	diva_xdi_um_cfg_cmd_t* msg = (diva_xdi_um_cfg_cmd_t*)&tmp[0];
	byte* data = (byte*)&msg[1];
	const byte* src  = (const byte*)buffer;
	dword len;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}

	while (length) {
		len = MIN(DIVA_SDRAM_WRITE_BLOCK_SZ, length);

		memset (msg, 0x00, sizeof(*msg));
		msg->adapter = (dword)card_number;
		msg->command = DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK;
		msg->command_data.write_sdram.ram_number  = ram_number;
		msg->command_data.write_sdram.offset	= offset;
		msg->command_data.write_sdram.length      = len;
		msg->data_length = len;

		memcpy (data, src, len);

    iomsg.length = sizeof(*msg)+len;
    iomsg.cmd = (void *) msg;

    if (diva_divas_uses_ioctl != 0) {
      if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != (sizeof(*msg)+len)) {
			  divas_close_driver (handle);
			  return (-1);
		  }
    } else {
      if (write ((int)handle, msg, sizeof(*msg)+len) != (sizeof(*msg)+len)) {
			  divas_close_driver (handle);
			  return (-1);
		  }
    }

		src		 += len;
		offset += len;
		length -= len;
	}


	divas_close_driver (handle);

	return (0);
}

/*
		Start card at specified address
	*/
int
divas_start_adapter (int card_number, dword start_address, dword features)
{
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_START_ADAPTER;
	msg.command_data.start.offset		= start_address;
	msg.command_data.start.features = features;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  return (-1);
	  }
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
			flock ((int)handle, LOCK_UN);
		  return (-1);
	  }
  }

	divas_close_driver (handle);

	return (0);
}

int
divas_stop_adapter (int card_number)
{
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_STOP_ADAPTER;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  return (-1);
	  }
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
			flock ((int)handle, LOCK_UN);
		  return (-1);
	  }
  }

	divas_close_driver (handle);

	return (0);
}

static int
divas_get_card_state (dword card, int fd, void* data)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	const char* state;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_GET_CARD_STATE;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		return (-1);
  	}
  } else {
	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
  		return (-1);
  	}
  }

	memset (&msg, 0x00, sizeof(msg));

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
		  return (-1);
	  }
  } else {
	  if (read (fd, &msg, sizeof(msg)) != sizeof(dword)) {
		  return (-1);
	  }
  }

	switch (*(dword*)&msg) {
		case 0:
			state = "ready";
			break;

		case 1:
			state = "active";
			break;

		case 2:
			state = "trapped";
			break;

		case 3:
			state = "out of service";
			break;

		default:
			state = "unknown";
	}
	strcpy ((char*)data, state);

	return (strlen((char*)data)+1);
}

static int
divas_get_software_features (dword card, int fd, void* data)
{
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card;
	msg.command = DIVA_XDI_UM_CMD_READ_SOFTWARE_OPTIONS;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
  		return (-1);
  	}
  } else {
	  if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
  		return (-1);
  	}
  }

	memset (&msg, 0x00, sizeof(msg));

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(dword)) {
		  return (-1);
	  }
  } else {
	  if (read (fd, &msg, sizeof(msg)) != sizeof(dword)) {
		  return (-1);
	  }
  }

	sprintf ((char*)data, "%08x", *(dword*)&msg);

	return (strlen((char*)data)+1);
}


int
divas_write_fpga (int card_number,
                  int fpga_number,
                  const void* buffer,
                  dword length)
{
	dword handle = divas_open_driver (card_number);
	char *tmp;
	diva_xdi_um_cfg_cmd_t* msg;
  diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (!(tmp = malloc (sizeof(*msg)+length))) {
		divas_close_driver (handle);
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	msg = (diva_xdi_um_cfg_cmd_t*)tmp;
	memset (msg, 0x00, sizeof(*msg));
	memcpy (&msg[1], buffer, length);

	msg->adapter = (dword)card_number;
	msg->command = DIVA_XDI_UM_CMD_WRITE_FPGA;
	msg->command_data.write_fpga.fpga_number  = fpga_number;
	msg->command_data.write_fpga.image_length = length;

  iomsg.length = sizeof(*msg)+length;
  iomsg.cmd = (void *) msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != (sizeof(*msg)+length)) {
  		free (tmp);
  		divas_close_driver (handle);
  		return (-1);
  	}
  } else {
	  if (write ((int)handle, msg, sizeof(*msg)+length) != (sizeof(*msg)+length)) {
  		free (tmp);
  		divas_close_driver (handle);
  		return (-1);
  	}
  }

	free (tmp);
	divas_close_driver (handle);

	return (0);
}

int
divas_ram_read (int card_number,
                int ram_number,
                void* buffer,
                dword offset,
                dword length)
{
	dword handle = divas_open_driver (card_number);
	dword len;
	diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
	byte* dst = (byte*)buffer;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	while (length) {
		len = MIN(DIVA_SDRAM_WRITE_BLOCK_SZ, length);

		memset (&msg, 0x00, sizeof(msg));
		msg.adapter = (dword)card_number;
		msg.command = DIVA_XDI_UM_CMD_READ_SDRAM;
		msg.command_data.read_sdram.ram_number  = ram_number;
		msg.command_data.read_sdram.offset			= offset;
		msg.command_data.read_sdram.length      = len;

    iomsg.length = sizeof(msg);
    iomsg.cmd = (void *) &msg;

    if (diva_divas_uses_ioctl != 0) {
      if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
        break;
      }
    } else {
      if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
        break;
      }
    }

    iomsg.length = len;
    iomsg.cmd = (void *) dst;

    if (diva_divas_uses_ioctl != 0) {
      if (ioctl ((int)handle, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != len) {
        break;
      }
    } else {
      if (read ((int)handle, dst, len) != len) {
        break;
      }
    }

		length -= len;
		offset += len;
		dst    += len;
	}

	divas_close_driver (handle);

	return ((length == 0) ? 0 : (-1));
}

/*
		Set untranslated protocol features
	*/
int
divas_set_protocol_features (int card_number, dword features)
{
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
	diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_SET_PROTOCOL_FEATURES;
	msg.command_data.features.features = features;

	iomsg.length = sizeof(msg);
	iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  divas_close_driver (handle);
		  return (-1);
	  }
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
		  divas_close_driver (handle);
		  return (-1);
	  }
  }

	divas_close_driver (handle);

	return (0);
}

int divas_set_protocol_code_version (int card_number, dword version) {
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
	diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_SET_PROTOCOL_CODE_VERSION;
	msg.command_data.protocol_code_version.version = version;

	iomsg.length = sizeof(msg);
	iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
	  if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
		  divas_close_driver (handle);
		  return (-1);
	  }
  } else {
	  if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
		  divas_close_driver (handle);
		  return (-1);
	  }
  }

	divas_close_driver (handle);

	return (0);
}

/*
		Set untranslated protocol features
	*/
int
divas_memory_test (int card_number, dword test_cmd)
{
	dword handle = divas_open_driver (card_number);
	diva_xdi_um_cfg_cmd_t msg;
	diva_xdi_io_cmd iomsg;

	if (handle == DIVA_INVALID_FILE_HANDLE) {
		return (-1);
	}
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

	memset (&msg, 0x00, sizeof(msg));
	msg.adapter = (dword)card_number;
	msg.command = DIVA_XDI_UM_CMD_ADAPTER_TEST;
	msg.command_data.test.test_command = test_cmd;

	iomsg.length = sizeof(msg);
	iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl ((int)handle, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  } else {
    if (write ((int)handle, &msg, sizeof(msg)) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  }

	divas_close_driver (handle);

	return (0);
}

static diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t dma_entry = { 0xffffffff, 0, 0 };

int diva_alloc_dma_map_entry (struct _diva_dma_map_entry* pmap) {
  diva_xdi_um_cfg_cmd_t msg;
  diva_xdi_io_cmd iomsg;
  diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t data;
  dword handle;
  int fd;

  dma_entry.nr = 0xffffffff;

  if ((handle = divas_open_driver ((int)(long)pmap)) == DIVA_INVALID_FILE_HANDLE) {
    return (-1);
  }
	if (diva_divas_uses_ioctl == 0) {
		if (flock ((int)handle, LOCK_EX) != 0) {
			divas_close_driver (handle);
			return (-1);
		}
	}

  fd = (int)handle;

  memset (&msg, 0x00, sizeof(msg));
  msg.adapter = (dword)(unsigned long)pmap;
  msg.command = DIVA_XDI_UM_CMD_ALLOC_DMA_DESCRIPTOR;

  iomsg.length = sizeof(msg);
  iomsg.cmd = (void *) &msg;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)&iomsg) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  } else {
    if (write (fd, &msg, sizeof(msg)) != sizeof(msg)) {
      divas_close_driver (handle);
      return (-1);
    }
  }

  memset (&data, 0x00, sizeof(data));

  iomsg.length = sizeof(data);
  iomsg.cmd = (void *) &data;

  if (diva_divas_uses_ioctl != 0) {
    if (ioctl (fd, DIVA_XDI_IO_CMD_READ_MSG, (ulong)&iomsg) != sizeof(data)) {
      divas_close_driver (handle);
      return (-1);
    }
  } else {
    if (read (fd, &data, sizeof(data)) != sizeof(data)) {
      divas_close_driver (handle);
      return (-1);
    }
  }

  dma_entry = data;

  divas_close_driver (handle);

  return ((data.nr != 0xffffffff) ? data.nr : -1);
}

/*
  Read adapter configuration from configuration library
  */
int diva_cfg_lib_read_configuration (diva_entity_queue_t* q) {
	int fd;
	diva_entity_link_t* link;
	byte* data = 0;

  if ((fd = open ("/dev/DivasDIDD", O_RDWR | O_NONBLOCK)) < 0) {
    fd =  open ("/proc/net/isdn/eicon/divadidd", O_RDWR | O_NONBLOCK);
  }

	if (fd >= 0) {
		struct pollfd fds[1];
		dword target = TargetXdiAdapter;
		int count, ret;
		struct sigaction act, org;

		act.sa_handler = sig_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_NOMASK;
		sigaction (SIGALRM, &act, &org);
		alarm (10);
		flock (fd, LOCK_EX);
		alarm (0);
		sigaction (SIGALRM, &org, 0);

		if (write(fd, &target, sizeof(target)) != sizeof(target)) {
			count = -1;
		}

		for (count = 0; count >= 0;) {
			memset(&fds[0], 0, sizeof(fds));

			fds[0].fd = fd;
			fds[0].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;

			ret = poll(&fds[0], 1, 0);

			if (ret == 0) {
				break;
			} else if (ret < 0) {
        count = -1;
			} else {
				int max_data_length = 4097;

				if ((link = malloc (sizeof(*link)+max_data_length))) {
					data = (byte*)&link[1];
				}

				while (link && (ret = read (fd, data, max_data_length - 1)) < 0) {
					if (errno == EMSGSIZE) {
						free (link);
						max_data_length += 1024;
						if ((link = malloc (sizeof(*link)+max_data_length))) {
							data = (byte*)&link[1];
						}
					} else {
						free (link);
						link = 0;
					}
				}

				if (link) {
					data[ret] = 0;
					diva_q_add_tail (q, link);
					target = (dword)HotUpdateFailedNotPossible;
					if (write(fd, &target, sizeof(target)) != sizeof(target)) {
						count = -1;
					} else {
						count++;
					}
				} else {
					count = -1;
				}
			}
		}

		close (fd);

		if (count < 0) {
			while ((link = diva_q_get_head (q))) {
				diva_q_remove   (q, link);
				free (link);
			}
		}

		return (count);
	}

	return (-1);
}

void diva_get_dma_map_entry (struct _diva_dma_map_entry* pmap, int nr,
                             void** pvirt, unsigned long* pphys) {
  if (nr == dma_entry.nr) {
    *pphys = (unsigned long)dma_entry.low;
    *pvirt = &dma_entry;
  } else {
    *pphys = 0;
    *pvirt = 0;
  }
}

void diva_get_dma_map_entry_hi (struct _diva_dma_map_entry* pmap, int nr,
                                unsigned long* pphys_hi) {
  if (nr == dma_entry.nr) {
    *pphys_hi = (unsigned long)dma_entry.high;
  } else {
    *pphys_hi = 0;
  }
}

void diva_free_dma_map_entry (struct _diva_dma_map_entry* pmap, int entry) {
}

static void no_dprintf(char* p, ...) {
}

static void dadapter_request (ENTITY* e) {
	if (e->Req == 0) {
		IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ *)e ;

		switch (e->Rc) {
			case IDI_SYNC_REQ_XDI_GET_CLOCK_DATA: {
				diva_xdi_um_cfg_cmd_get_clock_memory_info_t clock_data_info;

				if (divas_get_card_properties (card_number,
																			 "clock_data",
																			 (void*)&clock_data_info,
																			 sizeof(clock_data_info)) > 0) {
					diva_xdi_get_clock_data_t* clock_data = &syncReq->xdi_clock_data.info;

					clock_data->bus_addr_lo = clock_data_info.bus_addr_lo;
					clock_data->bus_addr_hi = clock_data_info.bus_addr_hi;
					clock_data->length      = clock_data_info.length;
					clock_data->addr        = 0;

					e->Rc = 0xff;
				} else {
					e->Rc = OUT_OF_RESOURCES;
				}
			} break;

			default:
				e->Rc = OUT_OF_RESOURCES;
				break;
		}
	}
}

void diva_os_read_descriptor_array (void* t, int length) {
  DESCRIPTOR* d = (DESCRIPTOR*)t;

  memset (t, 0x00, length);

  if (sizeof(*d)*2 <= length) {
    d->request   = (IDI_CALL)no_dprintf;
    d->type      = IDI_DIMAINT;
    d->channels  = 0;
    d->features  = 0;
    d++;
    d->request   = dadapter_request;
    d->type      = IDI_DADAPTER;
    d->channels  = 0;
    d->features  = 0;
  }
}

