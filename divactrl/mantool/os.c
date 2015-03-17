/*------------------------------------------------------------------------------
 *
 * (c) COPYRIGHT 1999-2007       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be
 * copied, transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *------------------------------------------------------------------------------*/
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

    sprintf (name, "/proc/net/eicon/adapter%d/idi", adapter_nr);
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
