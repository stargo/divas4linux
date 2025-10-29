
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
#include "os.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#ifdef __DIVA_OS_IMPLEMENT_GET_TIME_INFO__
#include <sys/time.h>
#include <time.h>
int diva_os_time_info_cfg_sync = 0;
#endif
#include "um_xdi.h"

#define DEBUG_OS
#undef DEBUG_OS 

static int diva_idi_uses_ioctl;

void* diva_os_open_adapter (int adapter_nr) {
	char name[512];
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
			close (fd);
#if defined(DEBUG_OS)
			printf ("A: can't open %s, errno=%d\n", name, errno);
#endif
      return ((void*)DIVA_OS_INVALID_HANDLE);
    }
  } else {
    sprintf (name, "/proc/net/isdn/eicon/adapter%d/idi", adapter_nr);
    if ((fd = open( name, O_RDWR | O_NONBLOCK)) < 0) {
#if defined(DEBUG_OS)
			printf ("A: can't open %s, errno=%d\n", name, errno);
#endif
      return ((void*)DIVA_OS_INVALID_HANDLE);
    }

    diva_idi_uses_ioctl = 1;
  }

	return ((void*)(long)fd);
}

int diva_os_read_descriptor_list (void* request_data,
                                  int request_data_length,
                                  void* indication_data,
                                  int indication_data_length) {
	void* handle = diva_os_open_adapter (0);
	int ret = -1;

	if (handle != 0) {
		if (diva_os_put_req (handle, request_data, request_data_length) >= 0) {
			memset (indication_data, 0x00, indication_data_length);
			if (diva_os_get_message (handle, indication_data, indication_data_length) >= 0) {
				ret = 0;
			}
		}
		diva_os_close_adapter (handle);
	}

	return (ret);
}

int diva_os_close_adapter (void* handle) {
	int fd = (int)(long)handle;
	if (fd >= 0) {
		close (fd);
		return(0);
	}
	return(-1);
}

/*
	Write messsage and return amount ob bytes we wrote or -1
	on error
	*/
int diva_os_put_req (void* handle, const void* data, int length) {
	int ret, fd;
	diva_um_io_cmd xcmd;
	
	fd = (int)(long)handle;
	xcmd.data = (void *)data;
	xcmd.length = length;

  if (diva_idi_uses_ioctl != 0) {
	  ret = ioctl(fd, DIVA_UM_IDI_IO_CMD_WRITE, (ulong) &xcmd);
  } else {
		ret = write (fd, data, length);
  }
	
	return (ret);
}

/*
	Read message and return amoung of bytes we got or -1 on error
	*/
int diva_os_get_message (void* handle, void* data, int max_length) {
	int ret, fd;
	diva_um_io_cmd xcmd;

	fd = (int)(long)handle;
	xcmd.data = data;
	xcmd.length = max_length;

  if (diva_idi_uses_ioctl != 0) {
    ret = ioctl(fd, DIVA_UM_IDI_IO_CMD_READ, (ulong) &xcmd);
  } else {
		ret = read(fd, data, max_length);
  }
	
	return (ret);
}

/*
	We use conver adapter handle from his internal representation to
	the presentation used by OS, that can be used in misc. primitives
	used to wait (sleep) until message is available (device is ready).
	*/
void* diva_os_convert_idi_handle_to_waitable_object (void* handle) {
	return (handle);
}

#ifdef __DIVA_OS_IMPLEMENT_GET_TIME_INFO__
/*
		t_sec - set to UTS time in sec
		t_sec_fractional - set to fraction of secound in 1/4294967295 units
		tz_offset - set to TZ offset in sec
		dst_active - set to une if DST is active
		synchronized - set to true if system is synchronized to global time source
	*/
int diva_os_get_time_info (dword* t_sec, dword* t_sec_fractional, dword* tz_offset, int* dst_active, int* synchronized) {
	struct timeval t_v;
	struct tm* t_m;

	if (gettimeofday (&t_v, 0) != 0)
		return (-1);

	if ((t_m = localtime (&t_v.tv_sec)) == 0)
		return (-1);

	if (t_sec != 0) {
		*t_sec = t_v.tv_sec;
	}

	if (t_sec_fractional != 0) {
		*t_sec_fractional = t_v.tv_usec * 4294U /* 4294.967295 */;
	}

	if (tz_offset != 0) {
		*tz_offset = (dword)t_m->tm_gmtoff;
	}

	if (dst_active != 0) {
		*dst_active = t_m->tm_isdst > 0;
	}

	if (synchronized != 0) {
		*synchronized = diva_os_time_info_cfg_sync;
	}

	return (0);
}
#endif

int diva_os_report_error_backtrace(char *fmt, ...) {
	return(0);
}
