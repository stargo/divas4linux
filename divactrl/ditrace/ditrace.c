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
#define __DIVA_INCLUDE_DITRACE_PARAMETERS__
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <errno.h>
#define __NO_STANDALONE_DBG_INC__
#include "platform.h"
#include "debuglib.h"
#include "di_defs.h"
#include "man_defs.h"
#include "debug_if.h"
#include "xdi_msg.h"
#include "dbgioctl.h"
#include "ditrace.h"
#include <xdi_vers.h>
#include <string.h>
#include <signal.h>

extern byte dbg_capi_nspi_use_b3_protocol;
extern word o;

extern void display_q931_message (char*, void*);
 
extern int dbg_rtp_filter_ssrc;
extern dword dbg_rtp_ssrc;
extern int dbg_rtp_filter_payload;
extern dword dbg_rtp_payload;
extern dword dbg_rtp_direction;
 
/*
  LOCALS
  */
#if !defined(DIVA_BUILD)
#define DIVA_BUILD "local"
#endif

static void do_dprintf (char* fmt, ...);
static int ditrace_process_cmd_line (int argc, char** argv);
extern int diva_decode_message (diva_dbg_entry_head_t* pmsg, int length);
static int diva_write_binary_ring_buffer (dword buffer_size, int do_poll);
extern int diva_read_binary_ring_buffer (char *filename);
extern int diva_trace_init();
static void usage(char *name);
extern int add_file (byte** , const char* file, int limit);
extern int add_string_to_header (byte** data,
																 const char* subject,
																 const char* data_str,
																 int limit);
extern unsigned int trace_compress (unsigned char* src, unsigned int src_length, unsigned char* dst);
extern void queueInit (MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer);
extern void add_string_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, dword id, const char* str);
extern void add_file_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, const char* name);
extern void sig_handler (int sig_nr);
extern int term_signal;
extern void queueInit (MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer);
extern byte *queueAllocMsg (MSG_QUEUE *Q, word size);
extern void queueFreeMsg (MSG_QUEUE *Q);
extern byte *queuePeekMsg (MSG_QUEUE *Q, word *size);



static void fill_id_list (const char* id_array);

FILE* OutStream;
int use_mlog_time_stamp = 0;
int silent_operation = 0;
int debug = 0;
dword ppp_analysis_mask;
dword vsig_analysis_mask;
dword rtp_analysis_mask;

int no_headers;
dword b_channel_mask;
int no_dchannel;
int no_strings;

#define DIVA_MAINT_IDS 255
int id_list[DIVA_MAINT_IDS];
int max_id_index = 0;
dword decoded_trace_events_mask = 0;
static int search_events;

static struct {
	const char* name;
	dword       event;
} trace_events[] = {
 /*
  Driver events
  */
 { "DRV_LOG",             0x00000001 }, /* always worth mentioning */
 { "DRV_FTL",             0x00000002 }, /* always sampled error    */
 { "DRV_ERR",             0x00000004 }, /* any kind of error       */
 { "DRV_TRC",             0x00000008 }, /* verbose information     */
 { "DRV_XLOG",            0x00000010 }, /* old xlog info           */
 { "DRV_MXLOG",           0x00000020 }, /* maestra xlog info       */
 { "DRV_EVL",             0x00000080 }, /* eventlog message        */
 { "DRV_REG",             0x00000100 }, /* init/configuration read */
 { "DRV_MEM",             0x00000200 }, /* memory management       */
 { "DRV_SPL",             0x00000400 }, /* event/spinlock handling */
 { "DRV_IRP",             0x00000800 }, /* OS I/O request handling */
 { "DRV_TIM",             0x00001000 }, /* timer/watchdog handling */
 { "DRV_BLK",             0x00002000 }, /* raw data block contents */
 { "DRV_TAPI",            0x00010000 }, /* debug TAPI interface    */
 { "DRV_NDIS",            0x00020000 }, /* debug NDIS interface    */
 { "DRV_CONN",            0x00040000 }, /* connection handling     */
 { "DRV_STAT",            0x00080000 }, /* trace state machines    */
 { "DRV_SEND",            0x00100000 }, /* trace raw xmitted data  */
 { "DRV_RECV",            0x00200000 }, /* trace raw received data */
 { "DRV_PRV0",            0x01000000 }, /* trace messages          */
 { "DRV_PRV1",            0x02000000 }, /* trace messages          */
 { "DRV_PRV2",            0x04000000 }, /* trace messages          */
 { "DRV_PRV3",            0x08000000 }, /* trace messages          */

 /*
  Adapter events
  */
 { "CARD_STRINGS",        0x00000001 }, /* All trace messages from the card */
 { "CARD_DCHAN",          0x00000002 }, /* All D-channel relater trace messages */
 { "CARD_MDM_PROGRESS",   0x00000004 }, /* Modem progress events */
 { "CARD_FAX_PROGRESS",   0x00000008 }, /* Fax progress events */
 { "CARD_IFC_STAT",       0x00000010 }, /* Interface call statistics */
 { "CARD_MDM_STAT",       0x00000020 }, /* Global modem statistics   */
 { "CARD_FAX_STAT",       0x00000040 }, /* Global call statistics    */
 { "CARD_LINE_EVENTS",    0x00000080 }, /* Line state events */
 { "CARD_IFC_EVENTS",     0x00000100 }, /* Interface/L1/L2 state events */
 { "CARD_IFC_BCHANNEL",   0x00000200 }, /* B-Channel trace for all channels */
 { "CARD_IFC_AUDIO",      0x00000400 }, /* Audio Tap trace for all channels */

 /*
  General events
  */
 { "NONE",                0x00000000 }, /* deactivate all events   */

 { 0, 0 }
};

/*
  IMPORTS
  */
extern FILE* VisualStream;
extern FILE* DebugStream;
extern void monitorLogs (void *inputFile, char *buf, unsigned long *bufSize) ;
extern void infoAnalyse (FILE *out, DbgMessage *msg) ;

extern void	pppAnalyse  (FILE *out, DbgMessage *msg) ;
extern void __pppAnalyse (FILE *out, unsigned char *data, unsigned long data_len, unsigned long real_frame_length);

extern void	tapiFilter  (FILE *out, DbgMessage *msg) ;
extern void dilogAnalyse (FILE *out, DbgMessage *msg, int DilogFormat) ;
extern void xlogAnalyse (FILE *out, DbgMessage *msg) ;
extern word xlog(byte *stream, void * buffer);

extern void sig_handler (int sig_nr);
extern int term_signal;

#define MAINT_DRIVER_NAME "/proc/net/eicon/maint"
#define NEW_MAINT_DRIVER_NAME "/dev/DivasMAINT"
int diva_maint_uses_ioctl;

int diva_open_maint_driver (void) {
	int fd;

	if ((fd = open (NEW_MAINT_DRIVER_NAME, O_RDWR | O_NONBLOCK)) < 0) {
		fd = open (MAINT_DRIVER_NAME, O_RDWR | O_NONBLOCK);
		diva_maint_uses_ioctl = 1;
	} else {
		diva_maint_uses_ioctl = 0;
	}

	return (fd);
}

diva_cmd_line_parameter_t* ditrace_find_entry (const char* c) {
	int i;

	for (i = 0; ditrace_parameters[i].ident; i++) {
		if (!strcmp(ditrace_parameters[i].ident, c)) {
			return (&ditrace_parameters[i]);
		}
	}

	return (0);
}

int ditrace_list_drivers (char* out, int length) {
  int i, fd = diva_open_maint_driver ();
  byte data[512];
  dword* pcmd = (dword*)&data[0];
  dword hours, min, sec, msec;

  if (fd < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  if (out && length > 1) {
    int i;
    char tmp[256];

    *out++ = '\n';
    length--;
    for (i = 0; ((i < 53) && (length > 1)); length--, i++) {
      *out++ = '-';
    }
    if (length > 1) {
      *out++ = '\n';
      length--;
    }
	  i = sprintf (tmp, "  ditrace, BUILD (%s[%s]-%s-%s)\n",
             DIVA_BUILD, diva_xdi_common_code_build, __DATE__, __TIME__);
    if (length > (i+1)) {
      strcpy (out, tmp);
      length-=i;
      out+= i;
		}
    for (i = 0; ((i < 53) && (length > 1)); length--, i++) {
      *out++ = '-';
    }
    if (length > 1) {
      *out++ = '\n';
      length--;
    }
    *out = 0;
  }
  if (!out) {
    fprintf (OutStream, "\n -----------------------------------------------------\n");
	  fprintf (OutStream,
             "  ditrace, BUILD (%s[%s]-%s-%s)\n",
             DIVA_BUILD, diva_xdi_common_code_build, __DATE__, __TIME__);
    fprintf (OutStream, " ------------------------------------------------------\n");
  }

  for (i = 1; ; i++) {
    pcmd[0]      = DITRACE_CMD_GET_DRIVER_INFO;
    pcmd[1]      = i; /* driver number */

    if (diva_maint_uses_ioctl != 0) {
      if (ioctl (fd,
                 DIVA_XDI_IO_CMD_WRITE_MSG,
                 (ulong)pcmd) <= 0) {
        break;
      }
    } else {
			if (write (fd, &data[0], sizeof(data)) <= 0) {
				break;
			}
    }

    if (!data[0]) {
      continue;
    }

    sec  = (data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24));
    msec = (data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24));

    hours = sec/3600;
    min   = (sec - hours*3600)/60;
    sec   = sec - hours*3600 - min*60;

    if (out) {
      char tmp [256];
      int len = sprintf (tmp, "  %2d:%02d:%02d.%03d %02d - %s\n",
                         hours, min, sec, msec, (byte)i, &data[9]);
      if (length >= (len+1)) {
        strcpy (out, tmp);
        length -= len;
        out    += len;
      }
    } else {
      fprintf (OutStream, "  %2d:%02d:%02d.%03d %02d - %s\n",
               hours, min, sec, msec, (byte)i, &data[9]);
    }
  }

  close (fd);

  if (out) {
    for (i = 0; ((i < 53) && (length > 1)); length--, i++) {
      *out++ = '-';
    }
    if (length > 1) {
      *out++ = '\n';
      length--;
    }
    *out = 0;
  } else {
    fprintf (OutStream, " ------------------------------------------------------\n");
  }

  return (0);
}

static int ditrace_get_driver_mask (int id, int show_events) {
  int fd = diva_open_maint_driver ();
  byte data[4*sizeof(dword)];
  dword* pcmd = (dword*)&data[0];
  dword mask;

  if (fd < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  pcmd[0] = DITRACE_READ_DRIVER_DBG_MASK;
  pcmd[1] = (dword)id;

  if (diva_maint_uses_ioctl != 0) {
    if (ioctl (fd,
               DIVA_XDI_IO_CMD_WRITE_MSG,
               (ulong)pcmd) != 4) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  } else {
    if (write (fd, &data[0], sizeof(data)) != 4) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  }

  mask = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  fprintf (OutStream, "%08x", mask);
	if (show_events != 0) {
		int event, first_event = 1, known_events = 0, unknown_events;

		for (event = 0; trace_events[event].name != 0; event++) {
			known_events |= trace_events[event].event;
		}

		for (event = 0; trace_events[event].name != 0; event++) {
			if ((trace_events[event].event & mask) != 0) {
				const char* name[2] = { trace_events[event].name, 0 };
				dword trace_event = trace_events[event].event;
				int i;

				for (i = event+1; trace_events[i].name != 0; i++) {
					if (trace_events[i].event == trace_event) {
						name[1] = trace_events[i].name;
						break;
					}
				}
				if (name[1] == 0) {
					fprintf (OutStream, "%s%s", (first_event == 0) ? "+" : " - ", name[0]);
				} else {
					fprintf (OutStream, "%s(%s or %s)", (first_event == 0) ? "+" : " - ", name[0], name[1]);
				}
				first_event = 0;
        mask &= ~trace_event;
			}
		}

		unknown_events = (known_events & mask) ^ mask;

		if (unknown_events != 0) {
			fprintf (OutStream, "%s0x%08x", (first_event == 0) ? "+" : " - ", unknown_events);
		}
	}
  fprintf (OutStream, "\n");

  close (fd);

  return (0);
}

static int ditrace_write_driver_mask (int id, dword mask) {
  int fd = diva_open_maint_driver ();
  byte data[4*sizeof(dword)];
  dword* pcmd = (dword*)&data[0];

  if (fd < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  mask &= 0x7fffffff;

  pcmd[0] = DITRACE_WRITE_DRIVER_DBG_MASK;
  pcmd[1] = (dword)id;
  pcmd[2] = mask;

  if (diva_maint_uses_ioctl != 0) {
    if (ioctl (fd,
               DIVA_XDI_IO_CMD_WRITE_MSG,
               (ulong)pcmd) != 4) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  } else {
    if (write (fd, &data[0], sizeof(data)) != 4) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  }

  close (fd);

  return (0);
}

static int ditrace_write_trace_filter (const char* filter) {
  int fd = diva_open_maint_driver ();
  byte data[4*sizeof(dword)+DIVA_MAX_SELECTIVE_FILTER_LENGTH];
  dword* pcmd = (dword*)&data[0];

  if (strlen(filter) > DIVA_MAX_SELECTIVE_FILTER_LENGTH) {
    return (-1);
  }

  if (fd < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  if (!filter || !*filter) {
    filter = "*";
  }

  pcmd[0] = DITRACE_WRITE_SELECTIVE_TRACE_FILTER;
  pcmd[1] = 0; /* ID is always MAINT for this command */
  pcmd[2] = (dword)strlen(filter);
  memcpy (&data[sizeof(*pcmd)*3], filter, pcmd[2]);

  if (diva_maint_uses_ioctl != 0) {
    if (ioctl (fd,
               DIVA_XDI_IO_CMD_WRITE_MSG,
               (ulong)pcmd) != pcmd[2]) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  } else {
    if (write (fd, &data[0], sizeof(data)) != pcmd[2]) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  }

  close (fd);

  return (0);
}

static int ditrace_read_trace_filter (void) {
  int length, fd = diva_open_maint_driver ();
  byte data[4*sizeof(dword)+DIVA_MAX_SELECTIVE_FILTER_LENGTH+1];
  dword* pcmd = (dword*)&data[0];

  if (fd < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  pcmd[0] = DITRACE_READ_SELECTIVE_TRACE_FILTER;
  pcmd[1] = (dword)0; /* always directed to MAINT */
  pcmd[2] = (dword)DIVA_MAX_SELECTIVE_FILTER_LENGTH;

  if (diva_maint_uses_ioctl != 0) {
    if ((length = ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)pcmd)) < 0) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  } else {
    if ((length = write (fd, &data[0], sizeof(data))) < 0) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
  }
  if (length && data[0]) {
    data[length] = 0;
  } else {
    data[0] = '*';
    data[1] = 0;
  }

  fprintf (OutStream, "%s\n", &data[0]);

  close (fd);

  return (0);
}

int ditrace_read_traces (int do_poll) {
  int fd;
  diva_dbg_entry_head_t* pmsg;
  byte data[MSG_FRAME_MAX_SIZE+sizeof(*pmsg)];
  dword* pcmd = (dword*)&data[0];
	struct pollfd fds[2];
  int ret;

  if (ditrace_list_drivers (0,0)) {
    return (-1);
  }

  pmsg = (diva_dbg_entry_head_t*)data;

  if ((fd = diva_open_maint_driver ()) < 0) {
    if (!silent_operation)
      perror ("DITRACE");
    return (-1);
  }

  pcmd[0] = DITRACE_READ_TRACE_ENTRY;
  pcmd[1] = (dword)id_list[0];
  pcmd[2] = sizeof(data);


  for (;;) {

    while ((ret = ((diva_maint_uses_ioctl != 0) ?
                                 ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)pcmd) :
                                 write (fd, data, sizeof(data)))) > sizeof(*pmsg)) {
      if (diva_decode_message (pmsg, ret)) {
        close(fd);
        return (-1);
      }
      pcmd[0] = DITRACE_READ_TRACE_ENTRY;
      pcmd[1] = (dword)id_list[0];
      pcmd[2] = sizeof(data);
    }
    if (ret < 0) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }

    memset (&fds, 0x00, sizeof (fds));
    fds[0].fd     = fd;
    fds[0].events = POLLIN;
    if (!do_poll || (!(poll (fds, 1, -1) > 0)) ||
        (!(fds[0].revents & POLLIN))) {
        break;
    }
  }

  close (fd);

  return (0);
}

static int diva_write_binary_ring_buffer (dword buffer_size, int do_poll) {
  int fd;
  int rb_fd = fileno (OutStream);
  byte* rb, *p, *map_base;
  MSG_QUEUE* dbg_queue;
  diva_dbg_entry_head_t* pmsg;
  byte data[10*(MSG_FRAME_MAX_SIZE+sizeof(*pmsg))+1];
  dword* pcmd = (dword*)&data[0];
	struct pollfd fds[2];
  int ret;
  int fret = 0;
  word size;
	char vtbl[] = " +*";
  dword events = 0;
  byte* pdata;
  dword cur_length;
	int   header_length = 0;
	struct sigaction act, org[3];
  dword sequence = 0;
	int compress_data = (ditrace_find_entry ("Compress")->found != 0);
	byte compressed_data[24*1024];
	word compressed_length;
	double total_compressed   = 1;
	double total_uncompressed = 1;
	int flush_events = -1;

	search_events = (ditrace_find_entry ("search")->found != 0);

  if (buffer_size < 256) {
    buffer_size = 256;
  }
  buffer_size *= 1024;
  buffer_size += getpagesize();
  buffer_size = (buffer_size/getpagesize())*getpagesize();

  pmsg = (diva_dbg_entry_head_t*)data;

  pcmd[0] = DITRACE_READ_TRACE_ENTRYS;
  pcmd[1] = (dword)0;
  pcmd[2] = sizeof(data);

  if (rb_fd < 0) {
    if (!silent_operation)
      perror ("Output File");
    return (-1);
  }

  if (do_poll)
    fprintf (VisualStream, "==> Create log file ... \n");

  {
    dword i;
    char p[getpagesize()];
    memset (p, 0x00, sizeof(p));

    for (i = 0; i < buffer_size/getpagesize(); i++) {
      if (write (rb_fd, p, sizeof(p)) != sizeof(p)) {
        if (!silent_operation)
          perror ("Output File");
        fclose (OutStream);
        return (-1);
      }
    }
    fflush (0);
  }

  if (!(rb = (byte*)mmap (0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, rb_fd, 0))) {
    if (!silent_operation)
      perror ("Output Buffer");
    fclose (OutStream);
    return (-1);
  }
  map_base = rb;

  /*
    Store Magic
    */
  *(dword*)rb  = (dword)DBG_MAGIC; /* Store Magic */
  rb += sizeof(dword);

  /*
    Create Message Buffer Info Header (up to 2048 Bytes)
    */
  *(dword*)rb = 2048; /* Extension Field Length */
  rb += sizeof(dword);
  if (ditrace_list_drivers ((char*)rb, 2048)) {
    munmap(map_base, buffer_size);
    fclose (OutStream);
    return (-1);
  }
  rb += 2048;

	if (compress_data) {
		header_length += add_string_to_header (&rb, "COMPRESSION TAG", TRACE_COMPRESSION_TAG,
														 		buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
	}

	header_length += add_file (&rb, DATADIR"/divas_cfg.rc",
														 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);

  {
    char name[sizeof("/var/log/divamtpx123412341234.log")+32];
    int i;

    for (i = 0; i < 101; i++) {
      sprintf (name, "/var/log/diva%d.log", i);
      header_length += add_file (&rb, name,
																 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    }
    for (i = 0; i < 101; i++) {
      sprintf (name, "/var/log/diva_dump%d.log", i);
      header_length += add_file (&rb, name,
																 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    }
    for (i = 0; i < 101; i++) {
      sprintf (name, "/var/log/divamgnt%d.log", i);
      header_length += add_file (&rb, name,
																 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    }
    for (i = 101; i < 201; i++) {
      sprintf (name, "/var/log/divamtpx%d.log", i);
      header_length += add_file (&rb, name,
																 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    }
    header_length += add_file (&rb, "/var/log/divacapi.log",
															 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    header_length += add_file (&rb, "/var/log/divatty.log",
															 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
    header_length += add_file (&rb, DATADIR"/httpd/index.html",
															 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);
  }

	if ((buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length) < (16*1024)) {
    munmap(map_base, buffer_size);
    fclose (OutStream);
    return (-1);
	}

  /*
    Terminate Extension List
    */
  *(dword*)rb = 0;
  rb += sizeof(dword);

  /*
    Store Ring Buffer Base
    */
  *(void**)rb  =  (void*)(rb+sizeof(void*)); /* Store Base  */
  rb += sizeof(void*);

  /*
    Create Ring Buffer
    */
  dbg_queue = (MSG_QUEUE*)rb;
  queueInit (dbg_queue, rb + sizeof(MSG_QUEUE),
						 buffer_size - sizeof(MSG_QUEUE) - 512 - 2048 - header_length);

  if ((fd = diva_open_maint_driver ()) < 0) {
    if (!silent_operation)
      perror ("MAINT");
    munmap(map_base, buffer_size);
    fclose (OutStream);
    return (-1);
  }

  if (do_poll)
    fprintf (VisualStream, "==> Read Message Stream, please press 'q' to exit\n");

  pcmd[0] = DITRACE_READ_TRACE_ENTRYS;
  pcmd[1] = (dword)0;
  pcmd[2] = sizeof(data);

	act.sa_handler = sig_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NOMASK;
	sigaction (SIGTERM, &act, &org[0]);
	sigaction (SIGHUP,  &act, &org[1]);
	sigaction (SIGINT,  &act, &org[2]);


  for (;term_signal == 0;) {
    while ((ret = ((diva_maint_uses_ioctl != 0) ?
                                 ioctl (fd, DIVA_XDI_IO_CMD_WRITE_MSG, (ulong)pcmd) :
                                 write (fd, data, sizeof(data)))) > sizeof(*pmsg)) {
      pdata = &data[0];
			if (flush_events > 0) {
				flush_events--;
				if (flush_events == 0) {
					term_signal = 1;
					ret = 0;
				}
			}
      for (; (cur_length = (pdata[0] | (pdata[1] << 8) | (pdata[2] << 16) | (pdata[3] << 24)));) {
        pdata += 4;

        sequence = ((diva_dbg_entry_head_t*)pdata)->sequence;

				if (search_events != 0 && flush_events < 0) {
					diva_dbg_entry_head_t* pmsg = (diva_dbg_entry_head_t*)pdata;

					if (pmsg->facility == MSG_TYPE_STRING) {
						const char* data = (const char*)&pmsg[1];
						int   data_length = cur_length - sizeof(*pmsg);

						if (data_length > sizeof ("exception") && strstr (data, "exception") != 0) {
							flush_events = 100;
						}
					}
				}

				if (compress_data) {
					compressed_length = (word)trace_compress (pdata, cur_length, &compressed_data[0]);

					total_compressed   += compressed_length;
					total_uncompressed += cur_length;
					if ((total_compressed == 0) || (total_uncompressed == 0)) {
						total_compressed   = 1.0;
						total_uncompressed = 1.0;
					}
				} else {
					compressed_length = cur_length;
				}

        while (!(p = (byte*)queueAllocMsg (dbg_queue, compressed_length))) {
          if ((p = (byte*)queuePeekMsg (dbg_queue, &size))) {
            queueFreeMsg (dbg_queue);
          } else {
            break;
          }
        }
        if (p) {
          dword to_flush = (cur_length/getpagesize())*getpagesize();
					if (compress_data) {
          	memcpy (p, &compressed_data[0], compressed_length);
					} else {
          	memcpy (p, pdata, cur_length);
					}
          queueCompleteMsg (p);

          if (cur_length > to_flush) {
            to_flush++;
          }
#if 0
          msync ((void*)((((dword)p)/getpagesize())*getpagesize()), to_flush, MS_ASYNC);
          msync (map_base, getpagesize(), MS_ASYNC);
#endif
        }
        pdata += cur_length;
      }

      pcmd[0] = DITRACE_READ_TRACE_ENTRYS;
      pcmd[1] = (dword)0;
      pcmd[2] = sizeof(data);

      events++;

			if (term_signal != 0) {
				break;
			}
    }
    if (ret < 0) {
      if (!silent_operation)
        perror("DITRACE");
      close (fd);
      return (-1);
    }
    if (!silent_operation && do_poll) {
			if (compress_data) {
      	fprintf (VisualStream, "%c %d, %.1f %% after compression\r",
								 vtbl[events%3], events, (float)total_compressed/total_uncompressed * 100);
			} else {
      	fprintf (VisualStream, "%c %d\r", vtbl[events%3], events);
			}
      fflush (VisualStream);
    }

    memset (&fds, 0x00, sizeof (fds));
    fds[0].fd     = fd;
    fds[0].events = POLLIN;
    if (!silent_operation) {
		  fds[1].fd     = STDIN_FILENO;
		  fds[1].events = POLLIN;
    }
    if (!term_signal && do_poll && (poll (fds, 1 + !silent_operation , -1) > 0)) {
      if (fds[1].revents & POLLIN) {
        char buffer[4];
        int length = read (STDIN_FILENO, buffer, sizeof(buffer)-1);
        if (length && (buffer[0] == 'q')) {
          break;
        }
      }
    } else {
      break;
    }
  }

  sequence++;

	if (compress_data) {
		sprintf ((char*)data, "Total compressed: %e, total uncompressed: %e - %.1f %% after compression",
						 total_compressed,
						 total_uncompressed,
						 (float)total_compressed/total_uncompressed * 100);
		add_string_to_buffer (dbg_queue, &sequence, 0, (char*)data);
	}

  add_string_to_buffer (dbg_queue, &sequence, 0, "Optional information follows using id 0x7fffffff");

	{
    char name[sizeof("/var/log/divamtpx123412341234.log")+32];
		int i = 0;

		for (i = 101; i < 201; i++) {
      sprintf (name, "/var/log/divamtpx%d.log", i);
      add_file_to_buffer (dbg_queue, &sequence, name);
		}
		add_file_to_buffer (dbg_queue, &sequence, "/var/log/divacapi.log");
		add_file_to_buffer (dbg_queue, &sequence, "/var/log/divatty.log");
	}
  add_string_to_buffer (dbg_queue, &sequence, 0, "End of optional information using id 0x7fffffff");

  msync(map_base, buffer_size, MS_SYNC);
  munmap(map_base, buffer_size);
  close (fd);
  fclose (OutStream);

	sigaction (SIGTERM, &org[0], 0);
	sigaction (SIGHUP,  &org[1], 0);
	sigaction (SIGINT,  &org[2], 0);

  return (fret);
}


static void fill_id_list (const char* id_array) {
	int i = 0;
	const char* p = strstr (id_array, ",");

	memset (id_list, 0x00, sizeof(id_list));
	if ((id_list[i] = atoi(id_array))) {
		i++;
	}

	while (p && *p && p[1] && (i < DIVA_MAINT_IDS)) {
		p++;
		if ((id_list[i] = atoi(p))) {
			i++;
		}
		p = strstr (p, ",");
	}
	max_id_index = i;
}

static void do_dprintf (char* fmt, ...) {
	char buffer[256];
	va_list ap;

	buffer[0] = 0;

	va_start(ap, fmt);
	vsprintf (buffer, (char*)fmt, ap);
	va_end(ap);

	fprintf (DebugStream, "%s\n", buffer);
}

int ditrace_main (int argc, char** argv) {
  diva_cmd_line_parameter_t* e;
  
  diva_trace_init();

	diva_dprintf = do_dprintf;
	VisualStream = stdout;
	DebugStream  = VisualStream;
  OutStream    = VisualStream;

  ditrace_process_cmd_line (argc, argv);

	if (ditrace_find_entry ("?")->found ||
	    ditrace_find_entry ("h")->found ||
	    ditrace_find_entry ("-help")->found) {
    usage (argv[0]);
    return (0);
  }
	if (ditrace_find_entry ("UseMlogTime")->found) {
    use_mlog_time_stamp = 1;
  }
	if (ditrace_find_entry ("Silent")->found) {
		if (!(VisualStream = fopen ("/dev/null", "w"))) {
			return (-1);
		}
    silent_operation = 1;
	  DebugStream  = VisualStream;
  }
	if (ditrace_find_entry ("debug")->found) {
		debug = 1;
	}
	if (ditrace_find_entry ("vsig")->found) {
		vsig_analysis_mask |= VSIG_ANALYSE_DATA;
	}
	if (ditrace_find_entry ("rtp")->found) {
		rtp_analysis_mask |= RTP_ANALYSE_DATA;
		dbg_rtp_direction = ditrace_find_entry ("rtp")->vi;
	}
	if (ditrace_find_entry ("pt")->found) {
		dbg_rtp_filter_payload = 1;
		dbg_rtp_payload = ditrace_find_entry ("pt")->vi;
		rtp_analysis_mask |= RTP_ANALYSE_DATA;
	}
	if (ditrace_find_entry ("ssrc")->found) {
		dbg_rtp_filter_ssrc = 1;
		dbg_rtp_ssrc = ditrace_find_entry ("ssrc")->vi;
		rtp_analysis_mask |= RTP_ANALYSE_DATA;

		if (dbg_rtp_filter_payload != 0 && ditrace_find_entry ("savertppayload")->found) {
			char name[24];
			FILE* f;

			snprintf (name, sizeof(name)-1, "pt-%03d-%08x.raw", dbg_rtp_payload, dbg_rtp_ssrc);
			name[sizeof(name)-1] = 0;

			f = fopen (name, "wb");
			if (f != 0) {
				dbg_rtp_filter_ssrc |= 2;
				fclose (f);
			}
		}
	}

  if (ditrace_find_entry ("ppp")->found == 2) {
    ppp_analysis_mask = PPP_ANALYSE_BCHANNEL;
  } else if (ditrace_find_entry ("ppp")->found == 1) {
    const char* data = ditrace_find_entry ("ppp")->vs;
    if (strstr(data, "bchannel") != 0) {
    	ppp_analysis_mask |= PPP_ANALYSE_BCHANNEL;
    }
		if (strstr(data, "data") != 0) {
    	ppp_analysis_mask |= PPP_ANALYSE_DATA;
    }
  }
	no_headers = ditrace_find_entry ("noheaders")->found != 0;
	if (ditrace_find_entry ("channel")->found != 0) {
		b_channel_mask = 1 << (ditrace_find_entry ("channel")->vi - 1);
	}

	if (ditrace_find_entry ("ncpib3protocol")->found != 0) {
		dbg_capi_nspi_use_b3_protocol = (byte)ditrace_find_entry ("ncpib3protocol")->vi;
	}

	no_dchannel = ditrace_find_entry ("nodchannel")->found != 0;
	no_strings  = ditrace_find_entry ("nostrings")->found != 0;
	if (ditrace_find_entry ("-version")->found) {
		fprintf (VisualStream,
						 "ditrace, BUILD (%s[%s]-%s-%s)\n",
             DIVA_BUILD, diva_xdi_common_code_build, __DATE__, __TIME__);
		return (0);
	}
  if ((e = ditrace_find_entry ("File"))->found == 1) {
		if (!(OutStream = fopen (e->vs, "w+"))) {
      if (!silent_operation)
        perror ("Output File");
      return (-1);
    }
  }

  if (ditrace_find_entry ("l")->found) {
    return (ditrace_list_drivers(0,0));
  }

  if ((e = ditrace_find_entry ("d"))->found) {
		fill_id_list (e->vs);
  }
  if (ditrace_find_entry ("events")->found == 2) {
		int i, ret = -1;
		for (i = 0; i < DIVA_MAINT_IDS; i++) {
			if (id_list[i] && (ret = ditrace_get_driver_mask (id_list[i], 1))) {
				return (ret);
			}
		}

		return (ret);
	} else if (ditrace_find_entry ("events")->found == 1) {
		const char* p = ditrace_find_entry ("events")->vs;
		int event;
		dword events = 0;

		for (event = 0; trace_events[event].name != 0; event++) {
			if (strstr(p, trace_events[event].name) != 0)
				events |= trace_events[event].event;
		}
		ditrace_find_entry ("m")->found = 1;
		ditrace_find_entry ("m")->vi    = events;
	}

  if (ditrace_find_entry ("m")->found == 2) {
		int i, ret = -1;

		for (i = 0; i < DIVA_MAINT_IDS; i++) {
			if (id_list[i] && (ret = ditrace_get_driver_mask (id_list[i], 0))) {
				return (ret);
			}
		}

    return (ret);
  }

  if ((ditrace_find_entry ("m")->found == 1) && (ditrace_find_entry ("i")->found == 1)) {
		decoded_trace_events_mask = e->vi;
	} else if ((e = ditrace_find_entry ("m"))->found == 1) {
		int i, ret = -1;

		for (i = 0; i < DIVA_MAINT_IDS; i++) {
			if (id_list[i] && (ret = ditrace_write_driver_mask (id_list[i], e->vi))) {
				return (ret);
			}
		}

		return (ret);
  }
  if ((e = ditrace_find_entry ("f"))->found == 1) {
    return (ditrace_write_trace_filter (&e->vs[0]));
  }
  if ((e = ditrace_find_entry ("f"))->found == 2) {
    return (ditrace_read_trace_filter ());
  }

  if ((e = ditrace_find_entry ("w"))->found == 1) {
    /*
       Write plain binary data to the output file
       */
    if (!ditrace_find_entry ("File")->found) {
      fprintf (VisualStream, "%s\n", "'-File' missing");
      return (-1);
    }
    return (diva_write_binary_ring_buffer(e->vi, ditrace_find_entry("p")->found));
  }
  /*
    Read Binary message file
    */
  if ((e = ditrace_find_entry ("i"))->found == 1) {
    return (diva_read_binary_ring_buffer (e->vs));
  }

  return (ditrace_read_traces(ditrace_find_entry("p")->found));
}

static int ditrace_process_cmd_line (int argc, char** argv) {
	int i;
	char* p;
	diva_cmd_line_parameter_t* e;
	diva_cmd_line_parameter_t* silent = ditrace_find_entry("Silent");


	for (i = 0; i < argc; i++) {
		p = argv[i];
		if (*p++ == '-') {
			if (!(e = ditrace_find_entry (p))) {
				if (!silent->found) {
					fprintf (VisualStream, "A: invalid parameter: (%s)\n", p);
				}
				exit (1);
			}
			if (e->found) {
				if (!silent->found) {
					fprintf (VisualStream, "A: dublicate parameter: (%s)\n", p);
				}
				exit (1);
			}

			e->found = 1;
			if (!e->value) {
				continue;
			}
			if (((i+1) >= argc) || (argv[i+1][0] == '-')) {
			  if (e->value == 2) {
          e->found = 2;
          continue;
        }
				if (!silent->found) {
					fprintf (VisualStream, "A: value missing: (%s)\n", e->ident);
				}
				exit (1);
			}
			p = &argv[i+1][0];
			if (e->string) {
				int len = strlen (p);
				if (len >= e->length) {
					if (!silent->found) {
						fprintf (VisualStream, "A: value too long: (%s)\n", e->ident);
					}
					exit (1);
				}
				strcpy (&e->vs[0], p);
			} else {
				e->vi = strtoul(p, 0, 0);
				if ((e->vi > e->max) || (e->vi < e->min)) {
					if (!silent->found) {
						fprintf (VisualStream, "A: invalid value (%s)\n", e->ident);
					}
					exit (1);
				}
			}
			i++;
		}
	}

	return (0);
}


unsigned short xlog_stream (void* out, void* buffer) {
 byte tmp[16*1024];
  word length = xlog(&tmp[0], buffer);
  tmp[length] = 0;
  fprintf ((FILE*)out, "%s", tmp);

  return (length);
}

static void usage_option (const char* tab, const diva_ditrace_options_t* opt) {
	char buffer[2048];
	int s, j, i;

	strcpy (buffer, tab);
	strcat (buffer, opt->key);
	s = strlen (buffer);
	while (s++ < 22) {
		strcat (buffer, " ");
	}
	j = strlen (buffer) + 2;
	strcat (buffer, ": ");
	strcat (buffer, opt->text[0]);
	fprintf (VisualStream, "%s\n", buffer);

	for (i = 1; (opt->text[i] && (i < 64)); i++) {
		buffer[0] = 0;
		s = 0;
		while (s++ < j) {
			strcat (buffer, " ");
		}
		strcat (buffer, opt->text[i]);
		fprintf (VisualStream, "%s\n", buffer);
	}
}

static void usage_options (const char* tab) {
	const diva_ditrace_options_t* opt = &ditrace_options [0];

	fprintf (VisualStream, " options:\n");

	while (opt->key) {
		usage_option (tab, opt++);
	}
	fprintf (VisualStream, "\n");
}

static void usage(char *name) {
	char* p = strstr(name, "/");
	while (p) {
		name = p+1;
		p = strstr (name, "/");
	}

	fprintf(VisualStream, "Usage: %s options\n\n", name);
	usage_options ("  ");
}

