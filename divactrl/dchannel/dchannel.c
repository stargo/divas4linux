
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
#include "../mantool/os.h"
#include "../load/diva.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "pc.h"
#include "dimaint.h"
#include "um_xdi.h"
#include "man_defs.h"

/*
  IMPORTS
  */
extern FILE* VisualStream;
extern FILE* DebugStream;
extern word maxout;
extern word xlog(byte *stream, XLOG *buffer);

/*
  LOCALS
  */
/*
  usage and command line processing
  */
struct _diva_cmd_line_parameter;
struct _dchannel_options;
static void dchannel_usage_options (const char* tab);
static void dchannel_usage_controller (void);
static void usage_option (const char* tab, const struct _dchannel_options* opt);
static struct _diva_cmd_line_parameter* find_dchannel_entry (const char* c);
static int dchannel_process_cmd_line (int argc, char** argv);

/*
  management interface operation
  */
static void B_TraceEventOff (void);
static void B_TraceSize (unsigned short size);
static void DEventsON (word mask);
static void L1Event (int on);
static void L2Event (int on);

/*
  private
  */
static int use_xlog_format = 0;
static int execute_user_script (int layer, int state);
static int create_pid_file (int c, int create);
static const char* monitor_file = 0;
static char card_name[64];
static char card_serial[64];
static int report_to_syslog = 0;
static int   entity=-1;
static byte  __tx_data[2048+512];
static diva_um_idi_req_hdr_t* tx_req = (diva_um_idi_req_hdr_t*)&__tx_data[0];
static byte* tx_data;

static byte  rx_data[2048+512];

static unsigned short basic_state;
static unsigned short quit=0;
static unsigned short controller = 1;

static void IdiMessageInput (diva_um_idi_ind_hdr_t* pInd);

static void add_p(byte * P, word * PLength, byte Id, byte * w);
static void single_p(byte * P, word * PLength, byte Id);
static void AssignManagementId (void);
static diva_um_idi_ind_hdr_t*
diva_man_wait_msg_interruptible (byte* buffer,
                                 int length,
                                 int interruptible);
static diva_um_idi_ind_hdr_t* diva_man_wait_msg (byte* buffer, int length);
static void SelectTraceDir (void);

static void RemoveManagementId (void);
static void dchannel_help (void);

static void TraceEventOn (void);
static void TraceEventOff (void);

typedef struct _diva_man_var_header {
  byte   escape;
  byte   length;
  byte   management_id;
  byte   type;
  byte   attribute;
  byte   status;
  byte   value_length;
  byte	 path_length;
} diva_man_var_header_t;

enum {
     REQUEST_ROOT,
     CD_TRACE,
     EVENT_TRACE_ON,
     OPERATING,
     REMOVING
     };

static void
dchannel_help (void)
{
  fprintf(VisualStream, "Usage: divactrl dchannel controller [options]\n\n");
  dchannel_usage_controller ();
  dchannel_usage_options ("  ");
}

   /*******************************************************************/
   /*                  dchannel main()                                */
   /*******************************************************************/
typedef struct _diva_cmd_line_parameter {
  const char* ident;
  int found;
  int string;
  int value;
  int length;
  dword min;
  dword max;
  dword  vi;
  char vs[256];
} diva_cmd_line_parameter_t;

static diva_cmd_line_parameter_t dchannel_parameters[] = {
{ "c",                      0, 0, 1,    0, 1,       1024, 0, {0, 0}},
{ "monitor",                0, 1, 1,  255, 0,          0, 0, {0, 0}},
{ "dmonitor",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "l1off",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "l2off",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "xlog",                   0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "Silent",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "syslog",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "Debug",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "?",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "h",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{  0,                       0, 0, 0,    0, 0,          0, 0, {0, 0}}
};

typedef struct _dchannel_options {
  const char*			key;
  const char*     text[64];
} dchannel_options_t;

static dchannel_options_t dchannel_options [] = {
{ "-?, -h",      {"display this help screen",
                  0}
},
{ "-monitor command",
                 {"start in the background and monitor interface",
                  "state change (Layer1/Layer2 if not turned off).",
                  "On change execute user supplied application",
                  "'command' with following paraneters:",
                  "Argument 1: controller number",
                  "Argument 2: source of event",
                  "            1 - Layer 1",
                  "            2 - Layer 2",
                  "Argument 3: current state",
                  "            0 - not connected (DOWN, CONNECTING,",
                  "                DISCONNECTING)",
                  "            1 - connected (UP)",
                  0}
},
{ "-dmonitor", {"decode D-channel messages",
                0}
},
{ "-l1off",      {"turn monitoring of Layer1 status change",
                  "(turned on by default)",
                  0}
},
{ "-l2off",      {"turn monitoring of Layer2 status change",
                  "(turned on by default)",
                  0}
},
{ "-xlog",       {"use Eicon 'xlog' output format",
                  0}
},
{ "-syslog",     {"report to syslog",
                   0}
},
{"-Silent",      {"silent operation mode, do not display messages",
                   0}
},
{"-Debug",       {"view debug messages",
                  0}
},
/* ------------------------------------------------------------------------ */
{ 0, {0}}
};

static void do_hup (int sig) {
  quit = 1;
}

int
dchannel_main (int argc, char** argv)
{
	diva_cmd_line_parameter_t* e;
  diva_um_idi_ind_hdr_t* pInd;
  word trc_mask = TM_D_CHAN;
  int l1_on = 1, l2_on = 2, dmonitor = 0, monitor = 0;
  int card_ordinal;
  char property[64];

  VisualStream = stdout;
  DebugStream  = stderr;
  maxout = 2048;
  dchannel_process_cmd_line (argc, argv);
  tx_data = (byte*)&tx_req[1];

  if (find_dchannel_entry ("syslog")->found) {
    report_to_syslog = 1;
  }
  if (find_dchannel_entry ("syslog")->found ||
      find_dchannel_entry ("monitor")->found) {
    find_dchannel_entry ("Silent")->found = 1;
  }

  if (find_dchannel_entry ("?")->found) {
    dchannel_help();
    exit (0);
  }

  use_xlog_format = find_dchannel_entry ("xlog")->found;

  e = find_dchannel_entry ("Silent");
  if (e->found) {
    if (!(VisualStream = fopen ("/dev/null", "w"))) {
      return (1);
    }
  } else {
    VisualStream = stdout;
  }
	e = find_dchannel_entry ("c");
  if (e->found) {
    controller = e->vi;
  } else {
    dchannel_help();
    exit (1);
  }
	if (find_dchannel_entry ("l1off")->found) {
    l1_on = 0;
  }
	if (find_dchannel_entry ("l2off")->found) {
    l2_on = 0;
  }
  if (find_dchannel_entry ("dmonitor")->found) {
    dmonitor = 1;
	  trc_mask |= TM_C_COMM;
	  trc_mask |= TM_DL_ERR;
	  trc_mask |= TM_LAYER1;
  }
  if ((e = find_dchannel_entry ("monitor"))->found) {
    monitor = 1;
    trc_mask &= ~TM_D_CHAN;
    monitor_file = &e->vs[0];
  }

	if (monitor && !l1_on && !l2_on && !dmonitor) {
    fprintf (VisualStream,"A: Layer1 or Layer2 or D-monitor trace\n");
    fprintf (VisualStream,"should be on in order to start D-channel monitor\n");
    exit (1);
  }

  if ((card_ordinal = divas_get_card (controller)) < 0) {
    fprintf (VisualStream, "A: can't get card type\n");
    exit (1);
  }
  strcpy (card_name, CardProperties[card_ordinal].Name);
	if (divas_get_card_properties (controller,
																 "serial number",
																 card_serial,
																 sizeof(card_serial)) <= 0) {
		strcpy (card_serial, "???");
	}
  if (divas_get_card_properties (controller,
                                 "card state",
                                 property,
                                 sizeof(property)) <= 0) {
    strcpy (property, "unknown");
  }
  if (!strcmp (property, "ready") || !strcmp (property, "trapped")) {
    fprintf (VisualStream,
             "A: can't access card, card state is '%s'\n", property);
    exit (1);
  }
  if (report_to_syslog || monitor) {
    char tmp_ident[64];
    sprintf (tmp_ident, "A(%d) %s SN:%s ", controller, card_name, card_serial);
    openlog (tmp_ident, LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  }

  if (monitor) {
    if (daemon (0,0)) {
      syslog (LOG_ERR, " [%ld] %s", (long)getpid(), "D-channel monitor failed");
      exit (1);
    } else {
     if (create_pid_file (controller, 1)) {
       syslog (LOG_NOTICE, " [%ld] %s", (long)getpid(), "D-channel monitor failed, can not create pid file");
       exit (1);
     }
     syslog (LOG_NOTICE, " [%ld] %s", (long)getpid(), "D-channel monitor started");
    }
  }

	signal (SIGHUP, do_hup);
	signal (SIGTERM, do_hup);
	signal (SIGABRT, do_hup);
	signal (SIGQUIT, do_hup);
	signal (SIGINT, do_hup);

  AssignManagementId ();
  basic_state = REQUEST_ROOT;
  basic_state = CD_TRACE;
  SelectTraceDir ();
  B_TraceEventOff ();
  B_TraceSize (maxout);
  basic_state = EVENT_TRACE_ON;
  if (l1_on && monitor) {
    L1Event (1);
  }
  if (l2_on && monitor) {
    L2Event (1);
  }
	if (trc_mask) {
    DEventsON (trc_mask);
    TraceEventOn ();
  }
  basic_state = OPERATING;
  SelectTraceDir ();

  while (!quit) {
    if ((pInd = diva_man_wait_msg_interruptible (rx_data,
                                                 sizeof(rx_data), 1))) {
      if (pInd->type == DIVA_UM_IDI_IND) {
        IdiMessageInput (pInd);
      }
    } else {
      break;
    }
  }

  /*
    Cleanup intarface before close
    */
	if (trc_mask) {
    DEventsON (0);
    TraceEventOff ();
  }
  if (l1_on && monitor) {
    L1Event (0);
  }
  if (l2_on && monitor) {
    L2Event (0);
  }
  SelectTraceDir ();
  RemoveManagementId ();
  if (report_to_syslog || monitor) {
    if (monitor) {
      syslog (LOG_NOTICE," [%ld] %s", (long)getpid(),"D-channel monitor terminated");
      create_pid_file (controller, 0);
    }
    closelog();
  }
	signal (SIGHUP,  SIG_DFL);
	signal (SIGTERM, SIG_DFL);
	signal (SIGABRT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGINT,  SIG_DFL);

  return 0;
}

static void
IdiMessageInput (diva_um_idi_ind_hdr_t* pInd)
{
  static byte tmp_xlog_buffer[4096*2];
  byte* p = &tmp_xlog_buffer[0];
  word tmp_xlog_pos;
  static int l1_state = 1;
  static int l2_state = 1;

  switch (pInd->hdr.ind.Ind) {
    case MAN_INFO_IND:
      break;

    case MAN_EVENT_IND: {
      diva_man_var_header_t* var_info = (diva_man_var_header_t*)&pInd[1];
      if ((var_info->path_length == 12) &&
           !strncmp((char*)(&var_info->path_length+1), "State\\Layer1",12)) {
        int val=!strncmp((char*)&var_info->path_length+1+var_info->path_length,
                         "Activated", 9);
        if (l1_state && !val &&
            !strncmp ((char*)&var_info->path_length+1+var_info->path_length,
                      "Syncronized", 11)) {
          val = 1;
        }
        if (val != l1_state) {
          l1_state = val;
          if (report_to_syslog || execute_user_script (1, val)) {
            syslog (val ? LOG_NOTICE : LOG_WARNING,
                    "L1: %s", val ? "UP" : "DOWN");
          }
        }
      } else if ((var_info->path_length == 16) &&
                  !strncmp((char*)(&var_info->path_length+1), "State\\Layer2 No1", 16)) {
        int val;
        switch (*((char*)&var_info->path_length+1+var_info->path_length)) {
          case 1:
          case 5:
            val = 1;
            break;
          default:
            val = 0;
        }
        if (val != l2_state) {
          l2_state = val;
          if (report_to_syslog || execute_user_script (2, val)) {
            syslog (val ? LOG_NOTICE : LOG_WARNING,
                    "L2: %s", val ? "UP" : "DOWN");
          }
        }
      }
    } break;

    case MAN_TRACE_IND:
      switch (basic_state) {
        case OPERATING:
          tmp_xlog_buffer[0] = 0;
          tmp_xlog_pos = xlog(&tmp_xlog_buffer[0],
                              (XLOG*)&(((MI_XLOG_HDR *)&pInd[1])->code));
          tmp_xlog_buffer[tmp_xlog_pos] = 0;
          if (!use_xlog_format && (tmp_xlog_buffer[0] == 'D') &&
              (tmp_xlog_buffer[1] == '-')) {
            if ((p = (byte*)strstr ((char*)tmp_xlog_buffer, ") "))) {
              p -= 3;
              if (p >= &tmp_xlog_buffer[0]) {
                memcpy (p, "DTRC:", 5);
              } else {
                p = &tmp_xlog_buffer[0];
              }
            } else {
              p = &tmp_xlog_buffer[0];
            }
          }
          if (report_to_syslog) {
            syslog (LOG_INFO, "%s", p);
          }
          fprintf(VisualStream, "%s", p);
          fflush (VisualStream);
          break;
      }
      break;

    default:
    break;
  }
}

static void
RemoveManagementId (void)
{
  if (entity != -1) {
    diva_close_adapter (entity);
    entity = -1;
  }
}

static diva_um_idi_ind_hdr_t*
diva_man_wait_msg (byte* buffer, int length) {
  return (diva_man_wait_msg_interruptible (buffer, length, 0));
}

static diva_um_idi_ind_hdr_t*
diva_man_wait_msg_interruptible (byte* buffer, int length, int interruptible)
{
  /* blocks until message available */
  if (interruptible) {
    if (diva_wait_idi_message_interruptible (entity, 1)) {
      return (0);
    }
  } else {
    diva_wait_idi_message (entity);
  }

  if (diva_get_message (entity, buffer, length) <= 0) {
    printf ("A: can't get message, errno=%d\n", errno);
    exit (3);
  }

  return ((diva_um_idi_ind_hdr_t*)buffer);
}

static void
AssignManagementId (void)
{
  unsigned short i = 0;
 diva_um_idi_ind_hdr_t* pInd;

 if ((entity = diva_open_adapter ((int)controller)) < 0) {
      printf ("A: can't open user mode IDI[%d], errno=%d\n",
				(int)controller, errno);
      exit (1);
  }

  /*
    ASSIGN a management entity
  */
  single_p(tx_data,&i,0); /* End Of Message */

  tx_req->type				= DIVA_UM_IDI_REQ_MAN;
  tx_req->Req					=	ASSIGN;
  tx_req->ReqCh				= 0;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    printf ("A: can't ASSIGN, errno=%d\n", errno);
    exit (1);
  }

  for (;;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (pInd->type == DIVA_UM_IDI_IND_RC) {
      break;
    }
  }

  if (pInd->hdr.rc.Rc != 0xef) {
    printf ("A: ASSIGN failed, RC=%02x\n", (byte)(pInd->hdr.rc.Rc));
    exit (2);
  }
}

static void
SelectTraceDir (void)
{
  unsigned short i=0;
  int wait_ind, wait_rc;
  diva_um_idi_ind_hdr_t* pInd;

  add_p(tx_data,&i,ESC, (byte*)"\x0b\x80\x00\x00\x00\x00\x05Trace");
  single_p(tx_data,&i,0); /* End Of Message */

  tx_req->Req = MAN_READ;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    printf ("A: error in SelectTraceDir, MAN_READ errno=%d\n", errno);
    exit (1);
  }

  for (wait_ind = 1, wait_rc = 1; wait_ind || wait_rc;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
      wait_rc = 0;
      if (pInd->hdr.rc.Rc != 0xff) {
        printf ("A: SelectTraceDir, MAN_READ Rc=%02x", (byte)pInd->hdr.rc.Rc);
        exit (2);
      }
      continue;
    }
    if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
      if (pInd->hdr.ind.Ind == MAN_INFO_IND) {
        wait_ind = 0;
      }
    }
  }
}

static void
TraceEventOn (void)
{
  unsigned short i = 0;
  int wait_rc, wait_ind;
  diva_um_idi_ind_hdr_t* pInd;

  add_p(tx_data,&i,ESC,(byte*)"\x16\x80\x00\x00\x00\x00\x10Trace\\Log Buffer");
  single_p(tx_data,&i,0); /* End Of Message */

  tx_req->Req = MAN_EVENT_ON;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    printf ("A: error in TraceEventOn, MAN_EVENT_ON errno=%d\n", errno);
    exit (1);
  }

  for (wait_ind = 1, wait_rc = 1; wait_ind || wait_rc;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
      wait_rc = 0;
      if (pInd->hdr.rc.Rc != 0xff) {
        printf ("A: TraceEventOn, MAN_EVENT_ON Rc=%02x", (byte)pInd->hdr.rc.Rc);
        exit (2);
      }
      continue;
    }
    if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
      if (pInd->hdr.ind.Ind == MAN_TRACE_IND) {
        wait_ind = 0;
      }
    }
  }
}

static void
TraceEventOff (void)
{
  unsigned short i = 0;
  int wait_rc;
  diva_um_idi_ind_hdr_t* pInd;

  add_p (tx_data,&i,ESC,(byte*)"\x16\x80\x00\x00\x00\x00\x10Trace\\Log Buffer");
  single_p (tx_data,&i,0); /* End Of Message */

  tx_req->Req = MAN_EVENT_OFF;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    printf ("A: error in TraceEventOff, MAN_EVENT_OFF errno=%d\n", errno);
    exit (1);
  }

  for (wait_rc = 1; wait_rc;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
      wait_rc = 0;
      if (pInd->hdr.rc.Rc != 0xff) {
        printf ("A: TraceEventOff, MAN_EVENT_OFF Rc=%02x",
		(byte)pInd->hdr.rc.Rc);
        exit (2);
      }
    }
  }
}

static void
single_p(byte * P, word * PLength, byte Id)
{
  P[(*PLength)++] = Id;
}

/*------------------------------------------------------------------*/
/* append a multiple byte information element to an IDI messages    */
/*------------------------------------------------------------------*/
static void
add_p(byte * P, word * PLength, byte Id, byte * w)
{
  P[(*PLength)++] = Id;
  memcpy(&P[*PLength], w, w[0]+1);
  *PLength +=w[0]+1;
}

/*
   OPTION USAGE FUNCTIONS
   */
static void dchannel_usage_controller (void) {
  fprintf (VisualStream, " controller, mandatory:\n");
  fprintf (VisualStream, "  -c n:    select controller number n\n");
  fprintf (VisualStream, "           default: 1\n");
  fprintf (VisualStream, "\n");
}

static void dchannel_usage_options (const char* tab) {
  const dchannel_options_t* opt = &dchannel_options [0];

  fprintf (VisualStream, " options, optional:\n");

  while (opt->key) {
    usage_option (tab, opt++);
  }
  fprintf (VisualStream, "\n");
}

static void usage_option (const char* tab, const dchannel_options_t* opt) {
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

/*
   COMMAND LINE PROCESSING
   */
static int dchannel_process_cmd_line (int argc, char** argv) {
	int i;
	char* p;
	diva_cmd_line_parameter_t* e;
	diva_cmd_line_parameter_t* silent = find_dchannel_entry("Silent");


	for (i = 0; i < argc; i++) {
		p = argv[i];
		if (*p++ == '-') {
			if (!(e = find_dchannel_entry (p))) {
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

static diva_cmd_line_parameter_t* find_dchannel_entry (const char* c) {
	int i;

	for (i = 0; dchannel_parameters[i].ident; i++) {
		if (!strcmp(dchannel_parameters[i].ident, c)) {
			return (&dchannel_parameters[i]);
		}
	}

	return (0);
}

/*
  MANAGEMENT INTERFACE OPERATIONS
  */
static void B_TraceEventOff (void) {
	unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,
					(byte*)"\x1c\x80\x00\x00\x00\x04\x12Trace\\B-Ch# Enable\x00\x00\x00\x00");
	single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    fprintf (VisualStream,
             "A: error in S_TraceEventOff, MAN_WRITE errno=%d\n",
             errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
        fprintf (VisualStream,
                 "A: B_TraceEventOff, MAN_WRITE Rc=%02x",
                 (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void B_TraceSize (unsigned short size) {
	word i = 0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;
	unsigned char * _fty =\
							(byte*)"\x1c\x80\x82\x01\x00\x02\x14Trace\\Max Log Length\x1e\x00";
	unsigned char fty[64];

	memcpy (fty, _fty, sizeof(fty));
	fty[27] =	LOBYTE(size);
	fty[28] =	HIBYTE(size);
  add_p(tx_data,&i,ESC,fty);
  single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    fprintf (VisualStream,
             "A: error in B_TraceSize, MAN_WRITE errno=%d\n",
             errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
        fprintf (VisualStream,
                 "A: B_TraceSize, MAN_WRITE Rc=%02x",
                 (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void DEventsON (word mask) {
  unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,
				 (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(mask);
  tx_data[i-1] = HIBYTE(mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    fprintf (VisualStream,
		         "A: error in DEventsON, MAN_WRITE errno=%d\n",
             errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
        fprintf (VisualStream,
                 "A: DEventsON, MAN_WRITE Rc=%02x",
                 (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void L1Event (int on) {
  unsigned short i = 0;
  int wait_rc;
  diva_um_idi_ind_hdr_t* pInd;

  add_p (tx_data,&i,ESC,(byte*)"\x12\x80\x00\x00\x00\x00\x0cState\\Layer1");
  single_p (tx_data,&i,0); /* End Of Message */

  tx_req->Req = on ? MAN_EVENT_ON : MAN_EVENT_OFF;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    fprintf (VisualStream,
             "A: error in L1Event, %s errno=%d\n",
             on ? "MAN_EVENT_ON" : "MAN_EVENT_OFF",
             errno);
    exit (1);
  }

  for (wait_rc = 1; wait_rc;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
      wait_rc = 0;
      if (pInd->hdr.rc.Rc != 0xff) {
        fprintf (VisualStream,
                 "A: error in L1Event, %s Rc=%02x\n",
                 on ? "MAN_EVENT_ON" : "MAN_EVENT_OFF",
		             (byte)pInd->hdr.rc.Rc);
        exit (2);
      }
    }
  }
}

static void L2Event (int on) {
  unsigned short i = 0;
  int wait_rc;
  diva_um_idi_ind_hdr_t* pInd;

  add_p (tx_data,&i,ESC,(byte*)"\x16\x80\x00\x00\x00\x00\x10State\\Layer2 No1");
  single_p (tx_data,&i,0); /* End Of Message */

  tx_req->Req = on ? MAN_EVENT_ON : MAN_EVENT_OFF;
  tx_req->data_length = (dword)i;

  if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
    fprintf (VisualStream,
             "A: error in L2Event, %s errno=%d\n",
             on ? "MAN_EVENT_ON" : "MAN_EVENT_OFF",
             errno);
    exit (1);
  }

  for (wait_rc = 1; wait_rc;) {
    pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
    if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
      wait_rc = 0;
      if (pInd->hdr.rc.Rc != 0xff) {
        fprintf (VisualStream,
                 "A: error in L2Event, %s Rc=%02x\n",
                 on ? "MAN_EVENT_ON" : "MAN_EVENT_OFF",
		             (byte)pInd->hdr.rc.Rc);
        exit (2);
      }
    }
  }
}

static int execute_user_script (int layer, int state) {
	int status;
  char cmd_string[512];

  if (!monitor_file) {
    syslog (LOG_ERR, "%s", "User notification failed");
    return (-1);
  }

  sprintf (cmd_string, "%s %d %d %d", monitor_file, controller, layer, state);
  status = system (cmd_string);
  if (status != -1) {
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
        return (0);
      } else {
        syslog (LOG_ERR, "%s%d", "User notification failed, error=", WEXITSTATUS(status));
      }
    }
  } else {
    syslog (LOG_ERR, "%s%d", "User notification failed, errno=", errno);
    return (-1);
  }

	syslog (LOG_ERR, "%s", "User notification failed");

  return (-1);
}

static int create_pid_file (int c, int create) {
  char name[64];

  sprintf (name, "/var/run/divasdchanneld%d.pid", c);

  if (create) {
    FILE* fs;
    if ((fs = fopen (name, "w"))) {
      fprintf (fs, "%ld\n", (long int)getpid());
      fflush (fs);
      fclose (fs);
    } else {
      return (-1);
    }
  } else {
    unlink (name);
  }

  return (0);
}

