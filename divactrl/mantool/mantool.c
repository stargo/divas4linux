
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
#include "pc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include "os.h"
#include "um_xdi.h"

#define MAX_CHAINED_IND 8
static unsigned short controller = 1;
static int controller_set = 0;
static int binary = 0;
static FILE         * LogStream=0;
static int entity = -1;

typedef struct _man_rcv_data_s {
	word length;
	byte P[4096];
	byte Ind;
} man_rcv_data_s_t;
static man_rcv_data_s_t man_rcv_data;

/* ----------------------------------------------------------------------
		LOCALS
	 ---------------------------------------------------------------------- */
typedef void (*diva_idi_ind_proc_t)(diva_um_idi_ind_hdr_t* pInd);
static diva_idi_ind_proc_t indication_proc = 0;

static void print_usage (void);
static void display_legend (void);
typedef int (*man_operation_proc_t)(const char* name,
																		const char* value);
static man_operation_proc_t man_operation_proc = 0;
static const char* value;
static const char* name;
static char _name[256];
static int monitor_mode          = 0;
static const char* i_trace       = "";
static const char* o_trace       = "";
static const char* trace_file    = 0;
static int modem_state_trace     = 0;
static int fax_state_trace       = 0;
static int channel_state_trace   = 0;

static int exclusive_access      = 0;
static int wd_timeout            = 0;
static int recursive_read        = 0;

static void name2unix (char* name);
static int IdiMessageInput (diva_um_idi_ind_hdr_t* pInd, int length);
static int matool_write (const char* name, const char* value);
static int matool_read (const char* name, const char* value);
static int matool_read_main (const char* name, const char* value, int decode);
static int matool_monitor (const char* name, const char* value);
static int matool_execute (const char* name, const char* value);

static word man_create_read_req (byte* P, const char* path);
static void AssignManagementId (int unhide);
static void RemoveManagementId (void);
static void diva_man_display_message_context (byte* msg, int length, char* tmp,
																						  char* alt_path);
static char* diva_man_decorate_name (char* tmp, int path_length);
static void single_p(byte * P, word * PLength, byte Id);
static byte* diva_man_get_next_entry (byte* msg);
static int diva_man_copy_entries(byte* msg, byte* buf, int buflen);

static int trace_channel = 0;
dword      ignore_trace = 0;

static char man_print_tmp_buffer[4096];
static char diva_man_path[256];
static byte* diva_man_decode_variable (byte* msg, int length, char* tmp,
																			 char* alt_path);
static man_rcv_data_s_t* diva_man_wait_ind (int entity);

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

static const byte* diva_man_test_get_idi_variable_info (const char*);
static void diva_man_test_request (byte Req, byte* data, word data_length);
static byte diva_man_wait_rc (void);
static const byte* diva_man_test_get_variable_info (const char*, const byte*,
																										const char*);
static void diva_waitkey (void);
static const char* print_time (void);
static void sig_handler (int sig_nr) { }
static void wd_sig_handler (int sig_nr) { exit(1); }
static int display_descriptor_list (int list_names);
static int display_adapter_info (int adapter);

/* ----------------------------------------------------------------------
    MANTOOL MAIN
   ---------------------------------------------------------------------- */
int mantool_main (int argc, char** argv) {
	struct sigaction act, org, org1;
	int i, ret, unhide  = 0;

	if (argc < 2) {
		print_usage();
		exit (1);
	}

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'b':
					binary = 1;
					break;

				case 'm':
				case 'g':
				case 'a':
					if (man_operation_proc && man_operation_proc != matool_monitor) {
						if (!binary) {
							printf ("I: monitor operation ignored\n");
						}
						break;
					}
					man_operation_proc = matool_monitor;
					value = name = " ";
					switch (argv[i][1]) {
						case 'a':
							modem_state_trace = 1;
							break;

						case 'g':
							fax_state_trace = 1;
							break;

						default:
							channel_state_trace = 1;
					}
					break;

				case 'i': /* Trace Nr for incoming calls */
					if (*o_trace || *i_trace) {
						printf ("I: can't trace multiple destinations, trace ignored\n");
						break;
					}
					i_trace = &argv[i][2];
					break;

				case 'o':	/* Trace Nr for outgoing calls */
					if (*o_trace || *i_trace) {
						printf ("I: can't trace multiple destinations, trace ignored\n");
						break;
					}
					o_trace = &argv[i][2];
					break;

				case 'f':
					trace_file = &argv[i][2];
					break;

				case 'c':
		       		if ((!(argv[i+1])) || (!(controller = (unsigned short)atoi(&argv[i+1][0])))) {
						 print_usage();
						 exit(1);
					}
					controller_set = 1;
					if (!binary) {
       			printf("I: adapter %d selected\n", controller);
					}
					break;
				case 'w':
					if (man_operation_proc) {
						if (!binary) {
							printf ("I: write operation ignored\n");
						}
						break;
					}
					man_operation_proc = matool_write;
					name = _name;
					strcpy (_name, &argv[i][2]);
					name2unix (_name);
					if ((value		= strstr (_name, "=")) == 0) {
						print_usage ();
						exit (1);
					}
					*(char*)value++ = 0;
					break;
				case 'r':
					if (man_operation_proc) {
						if (!binary) {
							printf ("I: read operation ignored\n");
						}
						break;
					}
					man_operation_proc = matool_read;
					name		= &argv[i][2];
					if (!*name)
						name = "\\"; /* Read Root */
					value		= " ";
					strcpy (_name, name);
					name = _name;
					name2unix (&_name[1]);
					break;
				case 'e':
					if (man_operation_proc) {
						if (!binary) {
							printf ("I: execute operation ignored\n");
						}
						break;
					}
					man_operation_proc = matool_execute;
					name		= &argv[i][2];
					value		= " ";
					strcpy (_name, name);
					name = _name;
					name2unix (_name);
					break;

				case 'E':
					exclusive_access = (!strcmp (&argv[i][1], "Exclusive"));
					break;

				case 'W':
					wd_timeout = (!strcmp (&argv[i][1], "WDog"));
					break;

				case 'R':
					recursive_read = (!strcmp(&argv[i][1], "Recursive"));
					break;

				case 'l':
					display_legend ();
					return (0);

				case 'u':
					unhide = 1; // display hidden variables
					break;

				case '?':
				case 'h':
					print_usage ();
					return (0);

				case 'L':
					return (display_descriptor_list(0));
				case 'F':
					if (controller_set == 0) {
						return (display_descriptor_list(1));
					} else {
						return (display_adapter_info (controller));
					}
			}

		}
	}

	if ((*i_trace || *o_trace) && (!trace_file)) {
		print_usage ();
		exit (1);
	}

	if (!man_operation_proc || !value || !name || !*value || !*name) {
		print_usage ();
		exit (1);
	}
	if (!binary) {
 printf ("Management Interface B-Channel State Trace utility for Diva cards\n");
 printf ("BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
 printf ("Copyright (c) 1991-2007 Dialogic\n");
		if (exclusive_access) {
 printf ("Exclusive interface access\n");
		} else {
 printf ("\n");
		}
	}

	AssignManagementId (unhide);

	if (exclusive_access) {
		act.sa_handler = sig_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_NOMASK;
		sigaction (SIGALRM, &act, &org);
		alarm (10);

		if (flock (entity, LOCK_EX) == (-1)) {
			if (!binary) {
				if (errno == EINTR) {
					printf ("A: operation timeout\n");
				} else {
					printf ("A: can't lock user mode IDI[%d], errno=%d\n", (int)controller, errno);
				}
			}
			alarm (0);
			sigaction (SIGALRM, &org, 0);
			exit (1);
		}
		alarm (0);
		sigaction (SIGALRM, &org, 0);
	}
	if (wd_timeout) {
		act.sa_handler = wd_sig_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_NOMASK;
		sigaction (SIGALRM, &act, &org);

		act.sa_handler = sig_handler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_NOMASK;
		sigaction (SIGKILL, &act, &org1);

		alarm (20);
	}

	ret = (*man_operation_proc)(name, value);

	RemoveManagementId ();

	if (wd_timeout) {
		alarm (0);
		sigaction (SIGALRM, &org,  0);
		sigaction (SIGKILL, &org1, 0);
	}

	return (ret);
}

static void AssignManagementId (int unhide) {
	byte rc;
  byte lli[] = {
    0x19, 0x01, 0x41
  };

  if (unhide) lli[2] |= 0x02; // set nohide flag
	if ((entity = diva_open_adapter ((int)controller)) < 0) {
		if (!binary) {
			printf ("A: can't open user mode IDI[%d], errno=%d\n",
							(int)controller, errno);
		}
		exit (1);
	}
	diva_man_test_request (ASSIGN, lli, sizeof(lli));
	if ((rc = diva_man_wait_rc ()) != ASSIGN_OK) {
		if (!binary) {
			printf ("A: Can't ASIGN, RC=%02x\n", rc);
		}
		exit (1);
	}
}

static void RemoveManagementId (void) {
	if (entity != -1) {
		diva_close_adapter (entity);
		entity = -1;
	}
}

static void display_legend (void) {
	printf ("Type:\n");
	printf ("  MI_DIR:           dir-\n");
	printf ("  MI_EXECUTE:       fn -\n");
	printf ("  MI_ASCIIZ:        asz-\n");
	printf ("  MI_ASCII:         asc-\n");
	printf ("  MI_NUMBER:        hex-\n");
	printf ("  MI_TRACE:         trc-\n");
	printf ("  MI_INT:           int-\n");
	printf ("  MI_UINT:          uit-\n");
	printf ("  MI_TRACE:         trc-\n");
	printf ("  MI_HINT:          hit-\n");
	printf ("  MI_HSTR:          hs -\n");
	printf ("  MI_BOOLEAN:       bol-\n");
	printf ("  MI_IP_ADDRESS:    ip -\n");
	printf ("  MI_BITFLD:        bit-\n");
	printf ("  MI_SPID_STATE:    sst-\n");
	printf ("  UNK:              ----\n");
	diva_waitkey();
	printf ("Attribute:\n");
	printf ("  W  - writable, locked or protected\n");
	printf ("  w  - writable, locked or protected\n");
	printf ("  e  - event, cleared\n");
	printf ("  E  - event, set\n");
	printf ("  l  - locked\n");
	printf ("  p  - protected\n");
}

static void diva_waitkey (void) {
	printf \
	(" ---------------------- please press RETURN to continue ----------------------\n");
  getchar();
}

static void print_usage (void) {
 printf ("Management Interface B-Channel State Trace utility for Diva cards\n");
 printf ("BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
 printf ("Copyright (c) 1991-2007 Dialogic\n\n");

	printf (" -c XXX        - to select controller XXX\n");
	printf (" -b            - do not print error and info messages,\n");
	printf ("                 do not wait for user input\n");
	printf (" -r\"XXX\"       - to read variable XXX or directory XXX\n");
	printf (" -e\"XXX\"       - to execute variable\n");
	printf (" -w\"XXX=YYY[,MMM]\" - to write YYY to variable XXX with mask MMM\n");
	printf (" -Recursive    - to read context of current directory and all they\n");
	printf ("                 subdirectories\n");
	printf (" -l            - to display legend\n");
	printf (" -L            - to display descriptor list\n");
	printf (" -F            - to display list of adapter names and features\n");
	printf (" -u            - unhide variables with hidden attribute\n");
	printf (" -h or -?      - to display this help screen\n");

	printf ("\n");

	printf (" -m            - to turn monitor modus on:\n");
	printf ("                 Monitor and out the state changes of\n");
	printf ("                 B-channels.\n");
	printf ("                 Can be used for accounting/monitoring.\n");
	printf (" -a            - to turn analog modem monitor modus on:\n");
	printf ("                 Monitor and out the state changes of\n");
	printf ("                 all modem instances.\n");
	printf (" -g            - to turn fax G3 monitor modus on:\n");
	printf ("                 Monitor and out the state changes of\n");
	printf ("                 all fax instances.\n");
	printf (" -Exclusive    - Obtain exclusive access to adapter using\n");
	printf ("                 file lock.\n");
	printf (" -WDog         - activate watchdog.\n");

	printf ("\n");
	diva_waitkey ();
	printf ("\n\n");
	printf ("Syntax:\n");
	printf (" divactrl mantool [EXAMPLE] \n\n");
	printf ("Examples:\n");
	printf (" -r                            - "\
			 													"read root of management directory\n");
	printf (" -r\"State\\B1\"                  - "\
																					"read State\\B1 directory\n");
	printf (" -r\"State\\B1\\Line\"             - "\
																				"read State\\B1\\Line variable\n");
	printf (" -e\"State\\B1\\Clear Call\"       - "\
							"call function State\\B1\\Clear Call\n");
	printf (" -w\"Config\\NT-2=1\"             - "\
							"write TRUE to variable Config\\NT-2\n");
	printf (" -w\"Config\\NT-2=0\"             - "\
							"write FALSE to variable Config\\NT-2\n");
	printf (" -w\"Config\\SPID-1\\SPID=380001\" - "\
							"change SPID\n");
	printf (" -m -a -g > log.txt\n");
	printf (" -m > log.txt                  - "\
					"start in monitor modus:\n"\
					"                                 "\
					"write all information about incoming\n"\
					"                                 "\
					"and outgoing calls in the file log.txt\n"\
					"                                 "\
					"the information in the log.txt\n"\
					"                                 "\
					"does contain timestamps plus\n"\
					"                                 "\
					"available information about the calls,\n"\
					"                                 "\
					"and can be used for accounting\n");
	printf ("\n");
}

static int matool_read (const char* name, const char* value) {
	if (recursive_read) {
		printf ("\nRecursive read: [%s]", name);
	}

	return (matool_read_main (name, value, 1));
}

static int matool_read_main (const char* name, const char* value, int decode) {
	word length = 0;
	char tmp = 0;
	byte _data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&_data[0];
	byte* xdata = (byte*)&pReq[1];
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0];
	byte* rdata = (byte*)&pInd[1];
  int ind_count, ie_data_len, ind_len;
  byte ie_buffer[MAX_CHAINED_IND * 2560];
	int len;

	if (!strcmp(name, "\\")) { /* Read ROOT */
		name = &tmp;
	}
	strcpy (diva_man_path, name);

	length = man_create_read_req (xdata, name);
  single_p(xdata,&length,0);    /* End Of Message */

	pReq->type  = DIVA_UM_IDI_REQ_MAN;
	pReq->Req   = MAN_READ;
	pReq->ReqCh = 0;
	pReq->data_length = (dword)length;

	if (diva_put_req (entity, pReq, sizeof(*pReq)+length) < 0) {
		if (!binary) {
			printf ("A: can't write MAN_READ");
		}
		return (1);
	}

	diva_wait_idi_message (entity); /* blocks until message available */

	if ((len = diva_get_message (entity, _data, sizeof(_data))) <= 0) {
		if (!binary) {
			printf ("A: RC length=%d\n", len);
		}
		return (2);
	}
	_data[len] = 0; /* terminate in any case */

	if (pInd->type != DIVA_UM_IDI_IND_RC) {
		if (!binary) {
			printf ("A: false message type=%d, should=%d\n",
							(int)pInd->type, DIVA_UM_IDI_IND_RC);
		}
		return (3);
	}
	if (pInd->hdr.rc.Rc != 0xff) {
		if (!binary) {
			printf ("A: RC=%02x\n", (byte)pInd->hdr.rc.Rc);
		}
		return (4);
	}

	if (decode) {
    ind_count = 1;  // wait for at least one indication
    ie_data_len = 0;
    while (ind_count--) {
      diva_wait_idi_message (entity); /* blocks until message available */

      if ((len = diva_get_message (entity, _data, sizeof(_data))) <= 0) {
        if (!binary) {
          printf ("A: RC length=%d\n", (int)len);
        }
        return (2);
      }
      _data[len] = 0; /* terminate in any case */

      if (pInd->type != DIVA_UM_IDI_IND) {
        if (!binary) {
          printf ("A: false message type=%d, should=%d\n",
                  (int)pInd->type, DIVA_UM_IDI_IND);
        }
        return (3);
      }

      if (pInd->hdr.ind.Ind != MAN_INFO_IND) {
        if (!binary) {
          printf ("A: false indication type=%d, should=%d\n",
                  (byte)pInd->hdr.ind.Ind, MAN_INFO_IND);
        }
        return (3);
      }

      // check for chained indication and set counter accordingly
      if ((rdata[0] == 0x7f) && // ESC element
          (rdata[2] == 0x80) && // man info element
          (rdata[3] == 0x82) && // MI_UINT
          (rdata[4] &0x04  ) && // is an ESC variable
          (rdata[7] == 9   ) && // path len
          !strncmp((char*)&rdata[8], "ind_count", 9) &&
          (rdata[6] == 1   )    // length of variable
                                ) {
        ind_count = rdata[17];
        if (ind_count > MAX_CHAINED_IND) {
          if (!binary) {
            printf ("A: too many chained indications (%d). Maximum is %d\n",
                    ind_count, MAX_CHAINED_IND);
            return (3);
          }
        }
        continue;
      }

      if (!(ind_len = diva_man_copy_entries(rdata,
                                 &ie_buffer[ie_data_len],
                                 sizeof(ie_buffer) - ie_data_len))) {
        if (!binary) {
          printf ("A: chained indication buffer overflow (%d).\n",
                  (int)sizeof(ie_buffer));
          return (3);
        }
      }
      ie_data_len += ind_len;

    } // while (ind_count)

    ie_buffer[ie_data_len]=0;

    diva_man_display_message_context (ie_buffer, ie_data_len,
                                      man_print_tmp_buffer, 0);

    if (recursive_read) {
      byte* var = ie_buffer;

      do {
        if ((var[0] == 0x7F) && (var[3] == 0x01)) {
          byte* msg = var+7;
          byte path_length = *msg++;
          char tmp[271];
          int ret;

          memcpy (tmp, msg, path_length);
          tmp[path_length] = 0;

          printf ("--------dir-[%s]", tmp);

          if ((ret = matool_read_main (tmp, value, decode))) {
            return (ret);
          }
        }
      } while ((var = diva_man_get_next_entry (var)));
    }
	}

	return (0);
}

static word man_create_read_req (byte* P, const char* path) {
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

	return (var_length + 0x08);
}

static void single_p(byte * P, word * PLength, byte Id) {
  P[(*PLength)++] = Id;
}

/*------------------------------------------------------------------*/
/* append a multiple byte information element to an IDI messages    */
/*------------------------------------------------------------------*/
void add_p(byte * P, word * PLength, byte Id, byte * w)
{
  P[(*PLength)++] = Id;
  memcpy(&P[*PLength], w, w[0]+1);
  *PLength +=w[0]+1;
}


/* ---------------------------------------------------
			Display message context to the buffer
	 --------------------------------------------------- */
static void diva_man_display_message_context (byte* msg,
																							int length,
																							char* tmp, char* alt_path) {
	int line = 0;
	printf ("\n");
	while (*msg == 0x7F) {
		msg = diva_man_decode_variable (msg, length, tmp, alt_path);
		printf ("%s\n", tmp);
		if (!binary && !(++line % 20)) {
			diva_waitkey ();
		}
	}
	printf ("\n");
	fflush (0);
}

static byte* diva_man_get_next_entry (byte* msg) {
	while (*msg == 0x7F) {
		byte* start = msg + 2;
		int msg_length = *(msg+1);

		return (start+msg_length);
	}

	return (0);
}

static int diva_man_copy_entries(byte* msg, byte* buf, int buflen) {
  int len = 0;
  byte *start = msg;

  for (len=0; msg[0] == 0x7F; msg = &msg[2+msg[1]]) {
    len += 2+msg[1];
  }
  if (len > buflen) return 0;
  memcpy(buf, start, len);
	return len;
}

/* ---------------------------------------------------
			Display message context to the buffer
	 --------------------------------------------------- */
static byte* diva_man_decode_variable (byte* msg, int length, char* tmp,
																			 char* alt_path) {
	byte* start = msg + 2;
	int msg_length = *(msg+1);
	int var_length, path_length;
	byte type, attrib, status;
	int diva_man_path_length;

	if (alt_path) {
		diva_man_path_length = strlen (alt_path);
	} else  {
		diva_man_path_length = strlen (diva_man_path);
	}

	if (diva_man_path_length) /* remove trailing back slash */
		diva_man_path_length++;

	msg     += 3; /* Jmp to type */
	type = *msg++;
	attrib = *msg++;
	status = *msg++;

	*tmp++= '-';
	if (attrib & 0x01) { /* WRITE ??? */
		if ((status & 0x01) || (status & 0x04)) {
			*tmp++= 'W';
		} else {
			*tmp++= 'w';
		}
	} else {
		*tmp++= '-';
	}
	if (attrib & 0x02) { /* EVENT ??? */
		if (status & 0x02) { /* ON ??? */
			*tmp++= 'E';
		} else {
			*tmp++= 'e';
		}
	} else {
		*tmp++= '-';
	}
	if (status & 0x01) { /* LOCKED ??? */
		*tmp++= 'l';
	} else {
		*tmp++= '-';
	}
	if (status & 0x04) { /* PROTECTED ??? */
		*tmp++= 'p';
	} else {
		*tmp++= '-';
	}
	if (attrib & 0x08) { /* HIDDEN ??? */
	  *tmp++= 'h';
	} else {
		*tmp++= '-';
	}
	if (attrib & 0x10) { /* REMOTE ??? */
	  *tmp++= 'r';
	} else {
		*tmp++= '-';
	}
	*tmp++= '-';
	var_length = *msg++;
	path_length = *msg++;

	if (path_length > diva_man_path_length) {
		msg += diva_man_path_length;
		path_length -= diva_man_path_length;
	}
	/* ------------------------------------------------------- */

	if 				(type == 0x01 /* MI_DIR */) {
		strcpy (tmp, "dir-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		tmp -= 3;


	} else if (type == 0x02 /* MI_EXECUTE */) {
		strcpy (tmp, "fn -");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		tmp -= 3;


	} else if (type == 0x03 /* MI_ASCIIZ */) {
		strcpy (tmp, "asz-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		var_length = strlen ((char*)msg);
		memcpy (tmp, msg, var_length);
		tmp += var_length;



	} else if (type == 0x04 /* MI_ASCII */) {
		int o = 0;
		strcpy (tmp, "asc-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		if (*msg) {
			if (msg[1] >= ' ' && msg[1] < 127) {
				memcpy (tmp, msg+1,*msg);
			} else {
				memcpy (tmp, msg+2,*msg-1);
				o = 1;
			}
		}
		var_length = *msg + 1;
		tmp += var_length - 1 - o; /* strip leading length byte */
	} else if (type == 0x05 /* MI_NUMBER */) {
		strcpy (tmp, "hex-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		{
			int length = *msg++;
			if (length) {
				while (length--) {
					tmp += sprintf (tmp, "%02x", *msg++);
					if (length) {
						*tmp++ =  ' ';
					}
				}
			} else {
				strcpy (tmp, "null");
				tmp += 4;
			}
		}

	} else if (type == 0x06 /* MI_TRACE */) {
		strcpy (tmp, "trc-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		tmp -= 3;


	} else if (type == 0x81 /* MI_INT */) {
		strcpy (tmp, "int-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 1:
				tmp += sprintf (tmp, "%d", *(signed char*)msg);
				break;
			case 2:
				tmp += sprintf (tmp, "%d", (short)READ_WORD(msg));
				break;
			default:
				tmp += sprintf (tmp, "%ld", (long)(int32)READ_DWORD(msg));
		}

	} else if (type == 0x82 /* MI_UINT */) {
		strcpy (tmp, "uit-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 1:
				tmp += sprintf (tmp, "%u", *(byte*)msg);
				break;
			case 2:
				tmp += sprintf (tmp, "%u", READ_WORD(msg));
				break;
			default:
				tmp += sprintf (tmp, "%lu", (unsigned long)READ_DWORD(msg));
		}
	} else if (type == 0x83 /* MI_HINT */) {
		strcpy (tmp, "hit-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 1:
				tmp += sprintf (tmp, "0x%02x", *(byte*)msg);
				break;
			case 2:
				tmp += sprintf (tmp, "0x%04x", READ_WORD(msg));
				break;
			case 4:
				tmp += sprintf (tmp, "0x%08lx", (unsigned long)READ_DWORD(msg));
				break;
			default:
				tmp += sprintf (tmp, "0x%lx", (unsigned long)READ_DWORD(msg));
		}

	} else if (type == 0x84 /* MI_HSTR */) {
		strcpy (tmp, "hs -");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		if (!var_length) {
			strcpy (tmp, "null");
			tmp += 4;
		} else {
			int length = var_length;
			while (length--) {
				tmp += sprintf (tmp, "%02x", *msg++);
			}
		}

	} else if (type == 0x85 /* MI_BOOLEAN */) {
		dword val;

		strcpy (tmp, "bol-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 1:
				val = *(byte*)msg;
				break;
			case 2:
				val = READ_WORD(msg);
				break;
			default:
				val = READ_DWORD(msg);
		}

		if (val) {
			memcpy (tmp, "TRUE", 4);
			tmp += 4;
		} else {
			memcpy (tmp, "FALSE", 5);
			tmp += 5;
		}

	} else if (type == 0x86 /* MI_IP_ADDRESS */) {
		strcpy (tmp, "ip -");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 2:
				tmp += sprintf (tmp, "%d", (((word)((byte*)msg)[0]) << 8) | ((word)((byte*)msg)[1]));
				break;
			case 4:
				tmp += sprintf (tmp, "%u.%u.%u.%u", ((byte*)msg)[3], ((byte*)msg)[2], ((byte*)msg)[1], ((byte*)msg)[0]);
				break;
		}

	} else if (type == 0x87 /* MI_BITFLD */) {
		dword val;
		int i;
		strcpy (tmp, "bit-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (var_length) {
			case 1:
				val = (*(byte*)msg) << 24;
				break;
			case 2:
				val = (READ_WORD(msg)) << 16;
				break;
			default:
				val = READ_DWORD(msg);
		}

		for (i=0; i < (8*var_length); i++) {
			if (val & 0x80000000) {
				*tmp++ = '1';
			} else {
				*tmp++ = '0';
			}
			val = val << 1;


			if (i < ((8*var_length)-1)) {
				if ((!((i+1)%4)) && i)
					*tmp ++= ':';
			}
		}
	} else if (type == 0x88 /* MI_SPID_STATE */) {
		strcpy (tmp, "sst-");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);

		switch (*(byte*)msg) {
			case 0:
				strcpy (tmp, "Idle                ");
				break;
			case 1:
				strcpy (tmp, "Layer2 UP           ");
				break;
			case 2:
				strcpy (tmp, "Layer2 Disconnecting");
				break;
			case 3:
				strcpy (tmp, "Layer2 Connecting   ");
				break;
			case 4:
				strcpy (tmp, "SPID Initializing   ");
				break;
			case 5:
				strcpy (tmp, "SPID Initialised    ");
				break;
			case 6:
				strcpy (tmp, "Layer2 Connecting   ");
				break;

			case  7:
				strcpy (tmp, "Auto SPID Stopped   ");
				break;

			case  8:
				strcpy (tmp, "Auto SPID Idle      ");
				break;

			case  9:
				strcpy (tmp, "Auto SPID Requested ");
				break;

			case  10:
				strcpy (tmp, "Auto SPID Delivery  ");
				break;

			case 11:
				strcpy (tmp, "Auto SPID Complete  ");
				break;

			default:
				strcpy (tmp, "---                 ");
		}
		tmp += 20;

	} else { /* UNKNORN TYPE */
		strcpy (tmp, "----");
		tmp+= 4;
		*tmp++= '[';

		memcpy (tmp, msg, path_length);
		msg+= path_length;
		tmp+= path_length;
		tmp = diva_man_decorate_name (tmp, path_length);
		tmp -= 3;
	}
	*tmp = 0;

	return (start+msg_length);
}

static char* diva_man_decorate_name (char* tmp, int path_length) {
	*tmp++ = ' ';
	while (path_length++ < 27) {
		*tmp++ = '.';
	}
	strcpy (tmp, "] = ");

	return (tmp + 4);
}

/* -------------------------------------------------------------
		WRITE VARIABLE
	 ------------------------------------------------------------- */
static int matool_write (const char* prms, const char* value) {
	byte _data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&_data[0];
	byte* xdata = (byte*)&pReq[1];
	/* diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0]; */
	/* byte* rdata = (byte*)&pInd[1]; */

	byte* man_x_data = xdata;
	const diva_man_var_header_t* var_info;
	byte rc;
	word i, j;
	char c;
	word length = 0;
	int _signed = 0;
	dword out_val;
	dword out_mask = 0xffffffff;
	byte* data_ptr;
	const char* fmt = "%d,%d";
	int str_type = 0;

	diva_man_path[0] = 0;

	if ((var_info = (const diva_man_var_header_t*)\
							diva_man_test_get_idi_variable_info (prms)) != 0) {
			if (var_info->attribute & 0x01 /* MI_WRITE*/) {
				switch (var_info->type) {
					case 0x81: /* MI_INT */
						_signed = 1;
						break;
					case 0x82: /* MI_UINT */
						break;
					case 0x83: /* MI_HINT */
					case 0x87: /* MI_BITFLD */
						fmt = "%x,%x";
						break;
					case 0x84: /* MI_HSTR */
						str_type = 0x84;
						break;
					case 0x85:
						break;
					case 0x03:
						str_type = 0x03;
						break;
					case 0x04:
						str_type = 0x04;
						break;

					default:
						if (!binary) {
							printf ("I: write of type %02x not implemented\n",
											 var_info->type);
						}
						return (1);
				}
				if (!str_type) {
					sscanf (value, fmt, &out_val, &out_mask);
					data_ptr = ((byte*)var_info) + var_info->length + 2\
					           - var_info->value_length;
				} else if (str_type == 0x84) {
					if (value[0] == 't')
					{
						length = 8;
					}
					else
					{
						length = 0;
						i = 0;
						while (value[i] != '\0')
						{
							if (isxdigit (value[i]))
								length++;
							i++;
						}
						length /= 2;
					}
					if (length > var_info->value_length)
						length = var_info->value_length;
					data_ptr = ((byte*)var_info) + 8 + var_info->path_length;
				} else {
					length = MIN((word)(var_info->value_length - 1), strlen(value));
					data_ptr = ((byte*)var_info) + 8 + var_info->path_length;
				}

				if (str_type == 0x03) {
					memcpy (data_ptr, value, length+1);
					((diva_man_var_header_t*)var_info)->value_length = (byte)(length+1);
					length = 8+length+1+ var_info->path_length;
					((diva_man_var_header_t*)var_info)->length = (byte)(length - 2);
					memcpy (man_x_data, var_info, length);
				} else if (str_type == 0x04) {
					*data_ptr = (byte)length;
					memcpy (data_ptr+1, value, length);
					((diva_man_var_header_t*)var_info)->value_length = (byte)(length+1);
					length = 8+length+1+ var_info->path_length;
					((diva_man_var_header_t*)var_info)->length = (byte)(length - 2);
					memcpy (man_x_data, var_info, length);
				} else if (str_type == 0x84) {
					if (value[0] == 't')
					{
						time_t t;

						time (&t);
						data_ptr[0] = 0;
						data_ptr[1] = 0;
						data_ptr[2] = 0;
						data_ptr[3] = 0;
						data_ptr[4] = (byte) t;
						data_ptr[5] = (byte)(t >> 8);
						data_ptr[6] = (byte)(t >> 16);
						data_ptr[7] = (byte)(t >> 24);
					}
					else
					{
						i = 0;
						j = 0;
						while (j != length)
						{
							while (!isxdigit (value[i]))
								i++;
							c = value[i];
							data_ptr[j] = (byte)((isdigit (c) ? c - '0' : tolower (c) - 'a' + 10) << 4);
							i++;
							while (!isxdigit (value[i]))
								i++;
							c = value[i];
							data_ptr[j] |= (byte)(isdigit (c) ? c - '0' : tolower (c) - 'a' + 10);
							i++;
							j++;
						}
					}
					((diva_man_var_header_t*)var_info)->value_length = (byte) length;
					length = 8+length+ var_info->path_length;
					((diva_man_var_header_t*)var_info)->length = length - 2;
					memcpy (man_x_data, var_info, length);
				} else {
					switch (var_info->value_length) {
						case 1:
							*data_ptr = (byte)((*data_ptr & ~out_mask) | out_val);
							break;
						case 2:
							WRITE_WORD(data_ptr, (word)((READ_WORD(data_ptr) & ~out_mask) | out_val));
							break;
						case 4:
							WRITE_DWORD(data_ptr, (dword)((READ_DWORD(data_ptr) & ~out_mask) | out_val));
							break;

						default:
							if (!binary) {
								printf ("A: Can't write, invalid variable length\n");
							}
							return (1);
					}
					memcpy (man_x_data, var_info, var_info->length+2);
					length = var_info->length+2;
				}
  			single_p(xdata,&length,0);    /* End Of Message */
				diva_man_test_request (MAN_WRITE, man_x_data, length);
				if ((rc = diva_man_wait_rc ()) != 0xff) {
					if (!binary) {
						printf ("A: Can't write, RC=%02x\n", rc);
					}
					return (1);
				}
				return (0);
			} else {
				if (!binary) {
					printf ("A: Can't write, not writable\n");
				}
			}
	} else {
			if (!binary) {
				printf ("A: Can't write, variable not found\n");
			}
	}

	return (1);
}

static diva_um_idi_ind_hdr_t* diva_man_wait_msg (char* buffer, int* length) {
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)buffer;
	int len = *length;

	diva_wait_idi_message (entity); /* blocks until message available */
	if ((len = diva_get_message (entity, pInd, len)) <= 0) {
		printf ("A: can't get message, errno=%d\n", errno);
		exit (5);
	}
	buffer[len] = 0; /* terminate in any case */

	*length = len;

	return (pInd);
}

/* --------------------------------------------------------------------------
			WAIT TIMM RC CODE WAS RECEIVED
	 -------------------------------------------------------------------------- */
static byte diva_man_wait_rc (void) {
	int len;
	byte _data[2048+512];
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0];

	for (;;) {
		diva_wait_idi_message (entity); /* blocks until message available */

		if ((len = diva_get_message (entity, _data, sizeof(_data))) <= 0) {
			if (!binary) {
				printf ("A: RC length=%d\n", len);
			}
			return (1);
		}
		_data[len] = 0; /* terminate in any case */

		if (pInd->type == DIVA_UM_IDI_IND_RC) {
			break;
		}

		if (indication_proc) {
			(*indication_proc)(pInd);
		}

	}

	return ((byte)(pInd->hdr.rc.Rc));
}

static const byte* diva_man_test_get_idi_variable_info (const char* name) {
	byte _data[2048+512];
	byte* man_x_data = &_data[0];
	man_rcv_data_s_t* pdata;
	word length = 0;
	byte rc;

	strcpy (man_print_tmp_buffer, diva_man_path);
	if (man_print_tmp_buffer[0])
		strcat (man_print_tmp_buffer, "\\");
	strcat (man_print_tmp_buffer, name);

	length = man_create_read_req (man_x_data, man_print_tmp_buffer);
 	single_p(&_data[0],&length,0);    // End Of Message
	diva_man_test_request (MAN_READ, man_x_data, length);
	if ((rc = diva_man_wait_rc ()) == 0xff) {
		for (;;) {
			pdata = diva_man_wait_ind (entity);
			pdata->P[pdata->length] = 0;

			if (pdata->Ind == MAN_INFO_IND) break;
		}

		return (diva_man_test_get_variable_info (name, pdata->P, 0));
	}

	return (0);
}

static man_rcv_data_s_t* diva_man_wait_ind (int entity) {
	byte _data[2048+512];
	int len;
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0];

	diva_wait_idi_message (entity); /* blocks until message available */

	if ((len = diva_get_message (entity, _data, sizeof(_data))) <= 0) {
		if (!binary) {
			printf ("A: IND length=%d\n", len);
		}
		exit (1);
	}
	_data[len] = 0; /* terminate in any case */

	if (pInd->type != DIVA_UM_IDI_IND) {
		if (!binary) {
			printf ("A: false message type=%d, should=%d\n",
							(int)pInd->type, DIVA_UM_IDI_IND);
		}
		exit (1);
	}
	man_rcv_data.Ind = (byte)pInd->hdr.ind.Ind;
	man_rcv_data.length = (word)pInd->data_length;
	if (pInd->data_length) {
		memcpy (man_rcv_data.P, &pInd[1], pInd->data_length);
	}
	man_rcv_data.P[pInd->data_length] = 0;

	return (&man_rcv_data);
}

static void diva_man_test_request (byte Req, byte* data, word length) {
	byte _data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&_data[0];
	byte* xdata = (byte*)&pReq[1];

	pReq->type = DIVA_UM_IDI_REQ_MAN;
	pReq->Req = Req;
	pReq->ReqCh = 0;
	pReq->data_length = length;
	if (length) {
		memcpy (xdata, data, length);
	}

	if (diva_put_req (entity, pReq, sizeof(*pReq)+length) < 0) {
		if (!binary) {
			printf ("A: can't write MAN_READ");
		}
		exit (1);
	}
}

static const byte* diva_man_test_get_variable_info (const char* name,
																							const byte* data,
																							const char* alt_path) {
	int name_length = strlen(name);
	int path_length;
	int var_name_length = 0;

	if (alt_path) {
		path_length = strlen (alt_path);
	} else {
		path_length = strlen (diva_man_path);
	}

	if (path_length)
		path_length++;

	while (*data == 0x7F) {
		var_name_length = *(data+7) - path_length;
		if (var_name_length == name_length) {
			if (!strncmp (name, (char*)(data+8+path_length), name_length)) {
				return (data);
			}
		}
		data += (*(data+1)+2);
	}

	return (0); /* Not Found */
}

/* ----------------------------------------------------------------
		EXECUTE FUNCTION
	 ---------------------------------------------------------------- */
static int matool_execute (const char* prms, const char* dummy) {
	byte _data[2048+512];
	byte* man_x_data = &_data[0];
	const diva_man_var_header_t* var_info;
	byte rc;
	word length = 0;

	diva_man_path[0] = 0;


	if ((var_info = (const diva_man_var_header_t*)\
							diva_man_test_get_idi_variable_info (prms)) != 0) {
			if (var_info->type == 0x02 /* MI_EXECUTE */) {
				memcpy (man_x_data, var_info, var_info->length+2);
				length = var_info->length+2;
  			single_p(&_data[0],&length,0);    // End Of Message
				diva_man_test_request (MAN_EXECUTE, man_x_data, length);
				if ((rc = diva_man_wait_rc ()) != 0xff) {
					if (!binary) {
						printf ("A: Can't execute, RC=%02x\n", rc);
					}
					return (1);
				}
				return (0);
			} else {
				if (!binary) {
					printf ("A: Can't execute, not executable\n");
				}
			}
	} else {
			if (!binary) {
				printf ("A: Can't execute, variable not found\n");
			}
	}

	return (1);
}

static void name2unix (char* name) {
	char* ptr = name;

	while (ptr && (ptr = strstr(ptr, "/"))) {
		*ptr++ = '\\';
	}
}

static int matool_monitor (const char* dummy, const char* dummy2) {
	int i;
	byte _tx_data[2048+512];
	byte* man_x_data = &_tx_data[0];
	dword ret;
	char name [64];
	int channels = 0;
	diva_man_var_header_t* var_info;
	word length = 0;
	byte rc;
	int indicated_channel = 0;

	if (trace_file) {
		if ((LogStream = fopen(trace_file,"w")) == 0) {
			printf("A: Can't not open trace output file <%s>!\n", trace_file);
			ret = 255;
			goto cleanup;
		}
	}

	if (modem_state_trace) {
		diva_man_path[0] = 0;
		strcpy (name, "State\\Modem Event");
		var_info = (diva_man_var_header_t*)\
												diva_man_test_get_idi_variable_info (name);
		if (var_info) { /* Older cards do not support modem progress */
			length = var_info->value_length;
			var_info->value_length = 0;
			var_info->length -= length;
			length = var_info->length + 2;
			memcpy (man_x_data, var_info, length);
 			single_p(man_x_data,&length,0);    // End Of Message
			diva_man_test_request (MAN_EVENT_ON, man_x_data, length);
			if ((rc = diva_man_wait_rc ()) != 0xff) {
				printf ("A: cant turn event %s ON\n", name);
				ret = 255;
				goto cleanup;
			}
		} else {
			modem_state_trace = 0;
		}
	}
	if (fax_state_trace) {
		diva_man_path[0] = 0;
		strcpy (name, "State\\FAX Event");
		var_info = (diva_man_var_header_t*)\
												diva_man_test_get_idi_variable_info (name);
		if (var_info) { /* Older cards do not support fax progress */
			length = var_info->value_length;
			var_info->value_length = 0;
			var_info->length -= length;
			length = var_info->length + 2;
			memcpy (man_x_data, var_info, length);
 			single_p(man_x_data,&length,0);    // End Of Message
			diva_man_test_request (MAN_EVENT_ON, man_x_data, length);
			if ((rc = diva_man_wait_rc ()) != 0xff) {
				printf ("A: cant turn event %s ON\n", name);
				ret = 255;
				goto cleanup;
			}
		} else {
			fax_state_trace = 0;
		}
	}

	for (i = 1; ((i < 31) && channel_state_trace); i++) {
		diva_man_path[0] = 0;

		sprintf (name, "State\\B%d\\Line", i);
		var_info = (diva_man_var_header_t*)\
												diva_man_test_get_idi_variable_info (name);
		if (!var_info) {
			break;
		}
		length = strlen (((char*)&(var_info->path_length))+\
																		1+var_info->path_length)+1;
		var_info->length -= length;
		length = var_info->length + 2;
		memcpy (man_x_data, var_info, length);
 		single_p(man_x_data,&length,0);    // End Of Message
		diva_man_test_request (MAN_EVENT_ON, man_x_data, length);
		if ((rc = diva_man_wait_rc ()) != 0xff) {
			printf ("A: cant turn event %s ON", name);
			ret = 255;
			goto cleanup;
		}
		channels++;
	}

	if (!channels && channel_state_trace) {
		printf ("A: no B-Channels available\n");
		ret = 255;
		goto cleanup;
	}
 fprintf (stderr, "\n\n");
 fprintf (stderr, "-----------------------------------------------------------------\n");
 fprintf (stderr, "Management Interface B-Channel State Trace utility for Diva cards\n");
 fprintf (stderr, "BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
 fprintf (stderr, "Copyright (c) 1991-2007 Dialogic\n\n");
 if (channels)
 fprintf (stderr, "Channels B1 to B%d available for tracing\n", channels);
 if (modem_state_trace)
		fprintf (stderr, "Modem state trace ON\n");
 if (fax_state_trace)
		fprintf (stderr, "FAX state trace ON\n");
 fprintf (stderr, "-----------------------------------------------------------------\n");
 fprintf (stderr, "\n");
 fprintf (stderr, "\n\n%s, Tracing is running, use Ctrl-C to stop\n\n",
					print_time());

	monitor_mode = 1;
	binary       = 1;

	{ /*
			Flush all incoming indications
			*/
		byte _data[2048+512];
		sleep (2);
		while (diva_get_message (entity, &_data[0], sizeof(_data)) > 0);
	}

	for (;;) {
		byte rx_data[2048+512];
		int len;
		diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)rx_data;
		len = sizeof(rx_data);
		pInd = diva_man_wait_msg ((char*)rx_data, &len);
		if (pInd->type != DIVA_UM_IDI_IND) {
			continue;
		}

		indicated_channel = IdiMessageInput (pInd, len);
		fflush (stdout);

		if (indicated_channel) {
			if (indicated_channel < 100) {
				sprintf (name, "State\\B%d", indicated_channel);
				fflush (stdout);
				matool_read_main (name, value, 0);
			} else if (indicated_channel < 200) { /* Modem State Change */
				sprintf (name, "State\\B%d\\Modem", indicated_channel-100);
				fflush (stdout);
				matool_read_main (name, value, 0);
			} else if (indicated_channel < 300) { /* Fax State Change   */
				sprintf (name, "State\\B%d\\FAX", indicated_channel-200);
				fflush (stdout);
				matool_read_main (name, value, 0);
			}
		}
	}

cleanup: /* Turn all events OFF */
	monitor_mode = 0;
	binary       = 0;

  if (LogStream) {
		fclose (LogStream);
	}

	return (ret != 0);
}

static const char* print_time (void) {
	time_t t;
	char *p;
	static char tmp[128];

	tmp[0] = 0;

	memset (&t, 0x00, sizeof(t));
	time (&t);
	if ((p = ctime(&t)) != 0) {
		strcpy (tmp, p);
		if ((p = strstr (tmp, "\n")) != 0)
			*p = 0;
		if ((p = strstr (tmp, "\r")) != 0)
			*p = 0;
	}

	return (&tmp[0]);
}

static int IdiMessageInput (diva_um_idi_ind_hdr_t* pInd, int length) {
	static int initial_time_print_done = 0;

	if (monitor_mode) {
		if (!initial_time_print_done) {
			initial_time_print_done = 1;
			printf ("B-Channel monitor started, %s\n", print_time ());
		}

  	if				(pInd->hdr.ind.Ind == MAN_INFO_IND) {
			char alt_path[256], *p, *p1;
			int channel = 0;
			diva_man_var_header_t* var_info =\
										(diva_man_var_header_t*)&pInd[1];
			memcpy (alt_path, &var_info->path_length+1, var_info->path_length);
			alt_path[var_info->path_length] = 0;
			if (!strncmp(alt_path, "State\\B",7)) {
				alt_path [5] = 0;
				channel = atoi (&alt_path[7]);
			} else {
				p = p1 = alt_path;
				while (p) {
					if ((p = strstr (p, "\\")) != 0) {
						p1 = p;
						p++;
					}
				}
				*p1 = 0;
			}
			/*
					Check if tracing should be on/off for special numbers
				*/
			if (channel && LogStream) {
				char tmp [128];
				char* value = 0;
				diva_man_var_header_t* var_info;
				sprintf (tmp, "State\\B%d\\Line", channel);
				var_info = (diva_man_var_header_t*)\
					diva_man_test_get_variable_info (tmp, (byte*)&pInd[1], "");
				if (var_info) {
					value = (char*)(&var_info->path_length + 1 + var_info->path_length);
					if (((trace_channel & 0x00FF) == channel) &&
							!(trace_channel & 0x4000) &&
							!strcmp (value, "Idle")) {
						trace_channel = channel | 0x4000; /* turn trace off */
						/* SetEvent (hMonitorEvents[31]); */
					} else if (!trace_channel &&
											*i_trace && !strcmp (value, "Connected-In")) {
						sprintf (tmp, "State\\B%d\\Remote Address", channel);
						var_info = (diva_man_var_header_t*)\
										diva_man_test_get_variable_info (tmp, (byte*)&pInd[1], "");
						if (var_info) {
							value = (char*)(&var_info->path_length + 1 + var_info->path_length);
							if (*value && !strncmp (value+1, i_trace, *value)) {
								trace_channel = channel | 0x8000; /* turn trace on */
								/* SetEvent (hMonitorEvents[31]); */
							}
						}
					} else if (!trace_channel &&
											*o_trace && !strcmp (value, "Connected-Out")) {
						sprintf (tmp, "State\\B%d\\Remote Address", channel);
						var_info = (diva_man_var_header_t*)\
										diva_man_test_get_variable_info (tmp, (byte*)&pInd[1], "");
						if (var_info) {
							value = (char*)(&var_info->path_length + 1 + var_info->path_length);
							if (*value && !strncmp (value+1, o_trace, *value)) {
								trace_channel = channel | 0x8000; /* turn trace on */
								/* SetEvent (hMonitorEvents[31]); */
							}
						}
					}
				}
			}
			diva_man_display_message_context ((byte*)&pInd[1],
																				pInd->data_length,
																				man_print_tmp_buffer, alt_path);
		} else if (pInd->hdr.ind.Ind == MAN_EVENT_IND) {
			char* path;
			int channel = 0;

			diva_man_var_header_t* var_info =\
										(diva_man_var_header_t*)&pInd[1];
			path = (char*)&var_info->path_length+1;
			if (!strncmp (path, "State\\B", 7)) {
				path += 7;
				if ((path[1] < '0') || ((path[1] > '9')))
					path[1] = 0;
				channel = atoi(path);
				printf ("INFO: B%02d -> [%s], %s\n", channel,
							(char*)&var_info->path_length+1+var_info->path_length,
							print_time ());
				if (channel && (channel < 31)) {
					return (channel);
				}
			} else if (!strncmp (path, "State\\Modem Event", var_info->path_length) ||
								 !strncmp (path, "State\\FAX Event", var_info->path_length)) {
				int channel = 0;
				int increment = 100;
				byte* data = &var_info->path_length;

				if (!strncmp (path, "State\\FAX Event", var_info->path_length)) {
					increment = 200;
				}
				data += sizeof(var_info->path_length) + var_info->path_length;
				switch (var_info->value_length) {
					case 1:
						channel = *data;
						break;
					case 2:
						channel = (int)READ_WORD(data);
						break;
					case 4:
						channel = (int)READ_DWORD(data);
						break;
				}
				if (channel) {
					return (channel+increment);
				}
				return (0);
			}
		} else if (pInd->hdr.ind.Ind == MAN_TRACE_IND) {
		}
		return (0);
	} else {
  	if (pInd->hdr.ind.Ind == MAN_INFO_IND) {
				man_rcv_data.length = (word)pInd->data_length;
				man_rcv_data.Ind	  = pInd->hdr.ind.Ind;
				memcpy (man_rcv_data.P, &pInd[1], pInd->data_length);
		} else {
			return (0);
		}
	}

	return (0);
}

static int display_adapter_info (int adapter) {
	byte _data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&_data[0];
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0];
	int len;
	int cardType = 0;

	if ((entity = diva_open_adapter (adapter)) < 0) {
		return (1);
	}

	pReq->type  = DIVA_UM_IDI_GET_FEATURES;
	pReq->Req   = 0;
	pReq->ReqCh = 0;
	pReq->data_length = 0;
	if (diva_put_req (entity, pReq, sizeof(*pReq)) < 0) {
		diva_close_adapter (entity);
		entity = -1;
		return (1);
	}
	memset (pInd, 0x00, sizeof(*pInd));
	if ((len = diva_get_message (entity, _data, sizeof(_data)-1)) <= 0) {
		diva_close_adapter (entity);
		entity = -1;
		return (1);
	}
	if (pInd->type != DIVA_UM_IDI_IND_FEATURES) {
		return (1);
	}
	diva_close_adapter (entity);
	entity = -1;

	{
		int nameLength = strlen(pInd->hdr.features.name)+1;
		if (nameLength+24 < sizeof(pInd->hdr.features.name)) {
			const char* p = &pInd->hdr.features.name[nameLength];

			if (p[0] == 'T' && p[1] == ':') {
				cardType = atoi(&p[2]);
				if (cardType >= CARDTYPE_MAX)
					cardType = 0;
			}
		}
	}

	if (cardType != 0) {
		printf ("name:'%s', type:%02x, features:%04x, channels:%d, nr:%d card type: %d card name: '%s'\n",
						pInd->hdr.features.name,
						pInd->hdr.features.type,
						pInd->hdr.features.features,
						pInd->hdr.features.channels,
						adapter,
						cardType,
						CardProperties[cardType].Name);
	} else {
		printf ("name:'%s', type:%02x, features:%04x, channels:%d, nr:%d\n",
						pInd->hdr.features.name,
						pInd->hdr.features.type,
						pInd->hdr.features.features,
						pInd->hdr.features.channels,
						adapter);
	}

	return (0);
}

static int display_descriptor_list (int list_names) {
	byte _data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&_data[0];
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&_data[0];
	dword i, *p = (dword*)&pInd[1];
	int len;

	if ((entity = diva_open_adapter (0)) < 0) {
		if (!binary) {
			printf ("A: can't open user mode IDI[%d], errno=%d\n", 0, errno);
		}
		return (-1);
	}


	pReq->type  = DIVA_UM_IDI_GET_DESCRIPTOR_LIST;
	pReq->Req   = 0;
	pReq->ReqCh = 0;
	pReq->data_length = 0;

	if (diva_put_req (entity, pReq, sizeof(*pReq)) < 0) {
		if (!binary) {
			printf ("A: can't write GET_DESCRIPTOR_LIST");
		}
		diva_close_adapter (entity);
		entity = -1;
		return (1);
	}

	memset (pInd, 0x00, sizeof(*pInd));

	if ((len = diva_get_message (entity, _data, sizeof(_data)-1)) <= 0) {
		if (!binary) {
			printf ("A: IND length=%d, errno=%d\n", len, errno);
		}
		diva_close_adapter (entity);
		entity = -1;
		return (1);
	}

	if (pInd->type != DIVA_UM_IDI_IND_DESCRIPTOR_LIST) {
		if (!binary)
			printf ("A: wrong IND type=%d\n", pInd->type);
	}
	diva_close_adapter (entity);
	entity = -1;

	if (list_names == 0)
		printf ("%d:{", pInd->hdr.list.number_of_descriptors);

	for (i = 0; i < pInd->hdr.list.number_of_descriptors; i++) {
		if (list_names != 0) {
			(void)display_adapter_info ((int)*p++);
		} else {
			printf ("%s%d", i != 0 ? "," : "", *p++);
		}
	}

	if (list_names == 0)
		printf ("}\n");

	return (0);
}

//void mlog_process_stdin_cmd (const char* cmd) { }

