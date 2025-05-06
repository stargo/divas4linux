
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
#include <errno.h>
#include "um_xdi.h"
#include "os.h"

#define DEBUG_OS
#undef DEBUG_OS 

extern void mlog_process_stdin_cmd (const char* cmd);

static int diva_idi_uses_ioctl;

int diva_open_adapter (int adapter_nr) {
	int fd ;

  if ((fd = open ("/dev/DivasIDI", O_RDWR | O_NONBLOCK)) >= 0) {
    diva_idi_uses_ioctl = 0;
    /*
      First write access is used to bind to specified adapter
      */
    if (write(fd, &adapter_nr, sizeof(adapter_nr)) != sizeof(adapter_nr)) {
      /*
         Adapter not found
         */
      return (DIVA_INVALID_FILE_HANDLE);
    }
  } else {
    char name[512];

    sprintf (name, "/proc/net/isdn/eicon/adapter%d/idi", adapter_nr);
    if ((fd = open( name, O_RDWR | O_NONBLOCK)) < 0) {
#if defined(DEBUG_OS)
      printf ("A: can't open %s, errno=%d\n", name, errno);
#endif
      return (DIVA_INVALID_FILE_HANDLE);
    }

    diva_idi_uses_ioctl = 1;
  }

  return ((dword)fd);
}

int diva_close_adapter (int handle) {
	if (handle != DIVA_INVALID_FILE_HANDLE){
		close ((int)handle);
		return(0);
	}
#if defined(DEBUG_OS)
			printf ("A: close_adapter, errno=%d\n", errno);
#endif
	return(-1);
}

int diva_put_req (int handle, const void* data, int length) {
	int ret;
  diva_um_io_cmd xcmd;

  xcmd.data = (void *)data;
  xcmd.length = length;
	
  if (diva_idi_uses_ioctl != 0) {
	  if ((ret = ioctl(handle, DIVA_UM_IDI_IO_CMD_WRITE, (ulong) &xcmd)) < 0) {
#if defined(DEBUG_OS)
		  printf ("A: put_req, errno=%d\n", errno);
#endif
	  }
  } else {
    if ((ret = write (handle, data, length)) < 0) {
#if defined(DEBUG_OS)
		  printf ("A: put_req, errno=%d\n", errno);
#endif
    }
  }

	return (ret);
}

int diva_get_message (int handle, void* data, int max_length) {
	int ret;
  diva_um_io_cmd xcmd;

  xcmd.data = data;
  xcmd.length = max_length;
	
  if (diva_idi_uses_ioctl != 0) {
    if ((ret = ioctl(handle, DIVA_UM_IDI_IO_CMD_READ, (ulong) &xcmd)) < 0) {
#if defined(DEBUG_OS)
      printf ("A: get_message, errno=%d\n", errno);
#endif
    }
  } else {
    if ((ret = read(handle, data, max_length)) < 0) {
#if defined(DEBUG_OS)
      printf ("A: get_message, errno=%d\n", errno);
#endif
    }
  }

	return (ret);
}

int diva_wait_idi_message (int entity) {
  return (diva_wait_idi_message_interruptible (entity, 0));
}

int diva_wait_idi_message_interruptible (int entity, int interruptible) {
	fd_set readfds;
	int ret;

	for (;;) {
		memset (&readfds, 0x00, sizeof(readfds));

		FD_SET (entity, &readfds);
		ret = select (entity+1, &readfds, 0, 0, 0);
		if (ret < 0) {
			if ((errno == EINTR) && !interruptible)
				continue;
			else
				return (-1);
		}

		if (FD_ISSET(entity, &readfds)) {
			return (0);
		}
	}
}

unsigned long GetTickCount (void) {
	return (0);
}

void diva_wait_idi_message_and_stdin (int entity) {
	fd_set readfds;
	int ret;
	char tmp[256];
	int n;
	char* p;

	for (;;) {
		memset (&readfds, 0x00, sizeof(readfds));

		FD_SET (0,  &readfds);
		FD_SET (entity, &readfds);
		ret = select (entity+1, &readfds, 0, 0, 0);
		if (ret < 0) {
			if (errno == EINTR) 
				continue;
			else
				return ;
		}

		if (FD_ISSET(0, &readfds)) {
			n = read (0, tmp, sizeof(tmp)-1);
			tmp[n] = 0;
			if ((p = strstr (tmp, "\r"))) *p = 0;
			if ((p = strstr (tmp, "\n"))) *p = 0;
			mlog_process_stdin_cmd (&tmp[0]);
		}

		if (FD_ISSET(entity, &readfds)) {
			return ;
		}
	}
}
