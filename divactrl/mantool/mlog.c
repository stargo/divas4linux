
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
#include "os.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "pc.h"
#include "dimaint.h"
#include "um_xdi.h"
#include "man_defs.h"

extern word xlog(byte *stream, XLOG *buffer);

static int   entity=-1;
static byte  __tx_data[2048+512];
static diva_um_idi_req_hdr_t* tx_req = (diva_um_idi_req_hdr_t*)&__tx_data[0];
static byte* tx_data;

static byte  rx_data[2048+512];
extern word maxout;

unsigned short key;
unsigned short BTrace=1;
unsigned short STrace=1;
unsigned short HistoryPresent=0;
unsigned short TriggeredEvents=0;
unsigned short HistoryLength=0;
unsigned short L1Trace = 1;
unsigned short DTrace = 1;
unsigned short MTrace  = 1;
unsigned short NTrace = 1;
unsigned short TTrace = 1;
unsigned short BMask = 0;
unsigned short AudioMask = 0;
unsigned short EyeMask = 0;
unsigned short LineSideTrace = 0;
unsigned short TimeslotTrace = 0;

unsigned short ETrace=1;
unsigned short basic_state;
unsigned short quit=0;
unsigned short EPrint=1;
unsigned short controller = 1;
unsigned long  TrcEvent;
FILE         * LogStream=0;
char         LogFileName[32];

unsigned long DbgFileWrap = (unsigned long)(-1);
unsigned long DbgFileLength = 0;

unsigned short Key_Procedure(unsigned short);
unsigned short HandleToIndex(unsigned short);

static void IdiMessageInput (diva_um_idi_ind_hdr_t* pInd);

static void add_p(byte * P, word * PLength, byte Id, byte * w);
static void single_p(byte * P, word * PLength, byte Id);
static void AssignManagementId (void);
static void diva_mlog_init_rb (void);
static diva_um_idi_ind_hdr_t* diva_man_wait_msg (byte* buffer, int length);
static void ReadTracePath (void);
static void SelectTraceDir (void);
static void TraceEventOn (void);
static void TraceEventOff (void);
static void StringEventOn (void);
static void S_TraceEventOff (void);
static void B_TraceSize (unsigned short size);
static void L1EventsOFF (void);
static void DEventsOFF (void);
static void DEventsON (void);
static void ModemEventsOFF (void);
static void NetEventsOFF (void);
#if 0
static void L1EventsON (void);
static void ModemEventsON (void);
static void NetEventsON (void);
#endif
static void DebugTracesOFF (void);
static void DebugTracesON (void);
static diva_um_idi_ind_hdr_t* diva_man_wait_msg_and_cmd (byte* buffer,
																												 int length);

static void ReadInitVars (void);

static void TriggeredEventOn (void);
/** \note unused
static void TriggeredEventOff (void);
*/


static void StartHistory (void);
static void StopHistory (void);
static void ClearHistory (void);

static void RemoveManagementId (void);
static void events(void);
static void mlog_help (void);
static void diva_waitkey (void);
static void rotate_ring_buffer_files (void);

static FILE* VisualStream;

static word std_trc_mask = TM_D_CHAN | TM_L_LAYER | TM_N_LAYER | TM_DL_ERR |
                    			 TM_LAYER1  | TM_C_COMM  | TM_M_DATA | TM_STRING;
static dword b_trace_mask = 0xffffffff;
static dword a_trace_mask = 0x00000000;
static dword eye_trace_mask = 0x00000000;
static void trace_set_b_channel (dword channel,
                                 unsigned short line_side_trace, unsigned short timeslot_trace);
static void trace_set_a_channel (dword channel,
                                 unsigned short timeslot_trace);
static void trace_set_eye_channel (dword channel,
                                   unsigned short timeslot_trace);
static int do_wait = 0;
static int generate_time_stamp = 0;
static int zero_time_stamp     = 0;

enum {
     REQUEST_ROOT,
     CD_TRACE,
     EVENT_TRACE_ON,
     OPERATING,
     REMOVING
     };

static void mlog_help (void) {
   fprintf (VisualStream,
	  "Management Interface Trace/Log utility for Diva Server adapters\n");
   fprintf (VisualStream, "BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
   fprintf (VisualStream, "Copyright (c) 1991-2007 Dialogic\n\n");
   fprintf (VisualStream,
           "===============================================================\n");
  fprintf (VisualStream, "  Press 'q' to quit\n");
  fprintf (VisualStream, "  Press 'b' to toggle B-channel traces\n");
  fprintf (VisualStream, "  Press 'f' to change the B-channel trace buffer size\n");
  fprintf (VisualStream, "  Press '#' to select B-channels\n");
  fprintf (VisualStream, "  Press 'A' to select audio samples\n");
  fprintf (VisualStream, "  Press 'Y' to select eye patterns\n");
  fprintf (VisualStream, "  Press 'j' to toggle media side tracing\n");
  fprintf (VisualStream, "  Press 'k' to toggle NULL PLCI tracing\n");
  fprintf (VisualStream, "  Press 's' to toggle string traces\n");
  fprintf (VisualStream, "  Press 'd' to toggle D-channel traces\n");
  if (HistoryPresent) {
	  fprintf (VisualStream, "  Press 'o' to StartHistory\n");
	  fprintf (VisualStream, "  Press 'g' to StopHistory\n");
  }
  fprintf (VisualStream, "  Press 'h' for help\n");
	fprintf (VisualStream, "  Use -o command line option to out\n");
	fprintf (VisualStream, "         data to the stdout and control\n");
	fprintf (VisualStream, "         stream to stderr\n");
	fprintf (VisualStream, "  Use -b command line option to turn\n");
	fprintf (VisualStream, "         the B-channel info OFF\n");

	if (do_wait) {
		diva_waitkey ();
	}

	fprintf (VisualStream, "  Use -lxxx command line option to set\n");
	fprintf (VisualStream, "         the length of B-channel traces to xxx bytes\n");
	fprintf (VisualStream, "  Use -#xxx command line option to set\n");
	fprintf (VisualStream, "         B-channel trace mask (as hex value)\n");
	fprintf (VisualStream, "  Use -Axxx command line option to set\n");
	fprintf (VisualStream, "         audio sample trace mask (as hex value)\n");
	fprintf (VisualStream, "  Use -Yxxx command line option to set\n");
	fprintf (VisualStream, "         EYE pattern trace mask (as hex value)\n");
	fprintf (VisualStream, "  Use -j command line option to enable\n");
	fprintf (VisualStream, "         the media side info\n");
	fprintf (VisualStream, "  Use -k command line option to enable\n");
	fprintf (VisualStream, "         the NULL PLCI info\n");
	fprintf (VisualStream, "  Use -s command line option to turn\n");
	fprintf (VisualStream, "         the string info off\n");
	fprintf (VisualStream, "  Use -d command line option to turn\n");
	fprintf (VisualStream, "         the D-channel info off\n");
	fprintf (VisualStream, "  Use -t command line option to turn\n");
	fprintf (VisualStream, "         the debug traces off\n");
	fprintf (VisualStream, "  Use -wXXX to write output in one ring buffer\n");
	fprintf (VisualStream, "         with length XXX and 4 segments:\n");
	fprintf (VisualStream, "         mlog.0 mlog.1 mlog.2 and mlog.3\n");

	if (do_wait) {
		diva_waitkey ();
	}

	fprintf (VisualStream, "  Use -rXXX to read output from ring buffer and\n");
	fprintf (VisualStream, "         write it to file XXX\n");
	fprintf (VisualStream, "  Use -c X command line option to select\n");
	fprintf (VisualStream, "         adapter nr. x\n");
	fprintf (VisualStream, "  Use -h command line option to display this\n");
	fprintf (VisualStream, "         help screen in human readable form and exit\n");
}

   /*******************************************************************/
   /*                  Mlog Main()                                    */
   /*******************************************************************/

int mlog_main (int argc, char** argv) {
	unsigned short arg;
	int b_len       = 0;

	tx_data = (byte*)&tx_req[1];
	VisualStream = stdout;

   for(arg=1; arg<argc; arg++) /* set streams first */
   {
		 if ((argv[arg][0] == '-') && (argv[arg][1] == 'o')) {
       LogStream = stdout;
       VisualStream = stderr;
   fprintf (LogStream,
					  "Management Interface Trace/Log utility for EICON DIVA adapters\n");
   fprintf (LogStream, "BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
   fprintf (LogStream, "Copyright (c) 1991-2007 Dialogic\n\n");
   fprintf (LogStream,
           "===============================================================\n");
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 's')) {
			STrace = 0;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'b')) {
			BTrace = 0;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'l')) {
			b_len = atoi(&argv[arg][2]);
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == '#')) {
			BMask = 1;
			sscanf(&argv[arg][2], "%x", &b_trace_mask);
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'A')) {
			AudioMask = 1;
			sscanf(&argv[arg][2], "%x", &a_trace_mask);
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'Y')) {
			EyeMask = 1;
			sscanf(&argv[arg][2], "%x", &eye_trace_mask);
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'j')) {
			LineSideTrace = 1;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'k')) {
			TimeslotTrace = 1;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'n')) {
			NTrace = 0; /* Networking Interface */
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'm')) {
			MTrace  = 0;  /* Modem Debug Info */
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == '1')) {
			L1Trace = 0;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'd')) {
			DTrace = 0;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 't')) {
			TTrace = 0;
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'w')) {
			DbgFileWrap = atoi(&argv[arg][2]);
			if (DbgFileWrap < 128) {
				DbgFileWrap = 128;
			} else if (DbgFileWrap > 1024*1024) {
				DbgFileWrap = 1024*1024;
			}
			DbgFileWrap *= 1024 / 4;  /* 4 files */
		 } else if ((argv[arg][0] == '-') && (argv[arg][1] == 'r')) {
			/*
					Do RB reconstruction
				*/
			int i;
			const char* dest_file = &argv[arg][2];
			FILE*	dest = fopen (dest_file, "w");
			char  seg_name[24];
			FILE* seg;
			char  tmp_buffer[32*1024];

			printf ("Begin data extraction ...\n");
			if (!dest) {
				printf ("E: can't create output file '%s'. ERRNO=%d\n",
								dest_file, errno);
				exit(1);
			}
			for (i=0;i<4;i++) {
				sprintf (seg_name, "mlog.%d", 3-i);
				printf ("  process segment %s\n", seg_name);

				if (!(seg = fopen (seg_name, "r"))) {
					fclose (dest);
					unlink (dest_file);
					printf ("E: can't open segment file '%s'. ERRNO=%d\n",
									seg_name, errno);
					exit(1);
				}
				while (!feof(seg)) {
					int len = fread (tmp_buffer, 1, sizeof(tmp_buffer), seg);
					if (len < 0) {
						fclose (dest);
						fclose (seg);
						unlink (dest_file);
						printf ("E: can't read segment file '%s'. ERRNO=%d\n",
										seg_name, errno);
						exit(1);
					}
					if (!len) continue;
					if (fwrite (tmp_buffer, 1, len, dest) != (unsigned int)len) {
						fclose (dest);
						fclose (seg);
						unlink (dest_file);
						printf ("E: can't read segment file '%s'. ERRNO=%d\n",
										seg_name, errno);
						exit(1);
					}
				}
				fclose (seg);
			}


			fflush (dest);
			fclose (dest);
			printf ("data was sucessful extracted to file '%s'\n", dest_file);

			exit(0);
		 } else if
		 ((argv[arg][0] == '-') && ((argv[arg][1] == 'h') || (argv[arg][1] == '?'))) {
			do_wait = 1;
			mlog_help();
			exit (0);
		 }
	}

   for(arg=1; arg<argc; arg++)
   {
	 if ((argv[arg][0] == '-') && (argv[arg][1] == 'c'))
     {
       if ((!argv[arg+1]) || (!(controller = atoi(&argv[arg+1][0])))){
		   mlog_help();
		   exit(1);
	   }
       fprintf(VisualStream, "  Adapter %d selected\n", controller);
     }
   }

   fprintf (VisualStream, "Management Interface Trace utility for Diva cards Version 1.0\nCopyright (c) 1991 - 2007 Dialogic\n\n");

   mlog_help ();

   AssignManagementId ();

   basic_state = REQUEST_ROOT;
   
   diva_mlog_init_rb ();
   ReadInitVars();
   
   if (HistoryPresent) {
	   TriggeredEventOn();
   }
   
   if (HistoryPresent==0 || TriggeredEvents==0) {
     ETrace = 0;
     if (HistoryPresent) {
	     printf("History Active\n");
     }
   } else {
		trace_set_a_channel (0xffffffff, TimeslotTrace);
		trace_set_eye_channel (0xffffffff, TimeslotTrace);
       
		TraceEventOn();
    ETrace = 1;
    if (HistoryLength>0) {
    	while ( HistoryLength>0) {
    		diva_um_idi_ind_hdr_t* pInd = diva_man_wait_msg (rx_data, sizeof(rx_data));
				if (pInd->type == DIVA_UM_IDI_IND) {
					IdiMessageInput (pInd);
				}
			}
			ClearHistory();
		}
		trace_set_a_channel (0x0, TimeslotTrace);
		trace_set_eye_channel (0x0, TimeslotTrace);
   }

		ReadTracePath ();
		basic_state = CD_TRACE;
		SelectTraceDir ();
		basic_state = EVENT_TRACE_ON;
		basic_state = OPERATING;
		SelectTraceDir ();

		if (STrace) {
			StringEventOn ();
		} else {
			std_trc_mask &= ~TM_STRING;
		}
		if (!BTrace) {
			trace_set_b_channel (0, LineSideTrace, TimeslotTrace);
		} else if (b_len) {
		maxout = b_len;
		if (maxout > 2048) maxout = 2048;
			B_TraceSize(maxout);
		}
		if (!DTrace) {
			DEventsOFF();
		}
		if (!L1Trace) {
			L1EventsOFF();
		}
		if (!MTrace) {
			ModemEventsOFF();
		}
		if (!NTrace) {
			NetEventsOFF();
		}
		if (!TTrace) {
			DebugTracesOFF();
		}
		if ((b_trace_mask != 0xffffffff) || LineSideTrace || TimeslotTrace || !BTrace) {
			trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
		}
		if ((a_trace_mask != 0x00000000) || TimeslotTrace) {
			trace_set_a_channel (a_trace_mask, TimeslotTrace);
		}
		if ((eye_trace_mask != 0x00000000) || TimeslotTrace) {
			trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
		}

   /*******************************************************************/
   /*  Run the application and wait for end                           */
   /*******************************************************************/
	while (!quit) {
		diva_um_idi_ind_hdr_t* pInd =\
								diva_man_wait_msg_and_cmd (rx_data, sizeof(rx_data));
		if (pInd->type == DIVA_UM_IDI_IND) {
			IdiMessageInput (pInd);
		}
	}

	TraceEventOff ();
	RemoveManagementId ();

	return 0;
}

static void IdiMessageInput (diva_um_idi_ind_hdr_t* pInd) {
  MI_XLOG_HDR *TrcData;
  unsigned long sec, hour;
  static byte tmp_xlog_buffer[4096*4];
  word tmp_xlog_pos;
  int length;
  byte *ie;
  char name[64];

	switch (pInd->hdr.ind.Ind) {
		case MAN_INFO_IND:
			break;

		case MAN_TRACE_IND:
			switch (basic_state) {
			  case REQUEST_ROOT:
				case OPERATING:
					TrcEvent++;
					events();
					if (LogStream) {
						TrcData = (MI_XLOG_HDR *)&pInd[1];
						if (!TrcData->time) {
							if (generate_time_stamp || (zero_time_stamp++ > 3)) {
								TrcData->time = GetTickCount ();
								generate_time_stamp = 1;
							}
						} else {
							generate_time_stamp = 0;
							zero_time_stamp     = 0;
						}
						sec  = TrcData->time/1000;
						hour = sec/3600;
						if (VisualStream == stderr) {
							fprintf(LogStream, "[C:%d] %u:%04u:%03u - ",
											controller,
											(unsigned short)hour, (unsigned short)(sec%3600),
											(unsigned short)(TrcData->time%1000)    );
						} else {
							fprintf(LogStream, "[C:%d] %u:%04u:%03u - ",
											controller,
											(unsigned short)hour, (unsigned short)(sec%3600),
											(unsigned short)(TrcData->time%1000)    );
						}
            tmp_xlog_buffer[0] = 0;
            tmp_xlog_pos = xlog(&tmp_xlog_buffer[0], (XLOG *)&TrcData->code);
            tmp_xlog_buffer[tmp_xlog_pos] = 0;
            fprintf(LogStream, "%s", &tmp_xlog_buffer[0]);
            if (!strcmp("A: TRIGGER USER STOP!\n", (char*)&tmp_xlog_buffer[0])) {
            	HistoryLength = 0;
            }
            fflush(0);
				}
        break;
      }
      break;
    case MAN_EVENT_IND:
				length = pInd->data_length;
				ie = ((byte *) &pInd->data_length)+4;
				while ((length>0) && (*ie++ == 127)) {
					byte ie_length = *ie++;
					length -= ie_length;
					strncpy(name, (char*)(ie+6), *(ie+5));
					name[*(ie+5)] = '\0';
					if (!strcmp("Trace\\History\\Triggered Event", name)) {
						TriggeredEvents = *(ie+6+*(ie+5));
						if (TriggeredEvents == 1) {
							printf("History stopped, restarting\n");
				      ReadInitVars();
				   		trace_set_b_channel (0xffffffff, LineSideTrace, TimeslotTrace);
				   		trace_set_a_channel (0xffffffff, TimeslotTrace);
				      trace_set_eye_channel (0xffffffff, TimeslotTrace);
						  if (ETrace != 0) {
					      TraceEventOff();
							}
						  TraceEventOn();
							while (HistoryLength>0) {
								 diva_um_idi_ind_hdr_t* pInd = diva_man_wait_msg (rx_data, sizeof(rx_data));
								 if (pInd->type == DIVA_UM_IDI_IND) {
									 IdiMessageInput (pInd);
								 }
							 }
							trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
							trace_set_a_channel (a_trace_mask, TimeslotTrace);
							trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
				      TraceEventOff();
			  	    ETrace = 0;
			  	    			if (HistoryPresent) {
								ClearHistory();
								StartHistory();
							}
						}
					}
					ie += ie_length;
				}	
      break;

    default:
    break;
  }

	if (LogStream && (LogStream != stdout) &&
			(DbgFileWrap != ((unsigned long)(-1)))) {
		DbgFileLength = ftell (LogStream);
		if (DbgFileLength >= DbgFileWrap) {
			rotate_ring_buffer_files ();
			DbgFileLength = 0;
		}
	}
}

static void RemoveManagementId (void) {
	if (entity != -1) {
		diva_close_adapter (entity);
		entity = -1;
	}
}

static diva_um_idi_ind_hdr_t* diva_man_wait_msg (byte* buffer, int length) {

	diva_wait_idi_message (entity); /* blocks until message available */
	if (diva_get_message (entity, buffer, length) <= 0) {
		printf ("A: can't get message, errno=%d\n", errno);
		exit (3);
	}

	return ((diva_um_idi_ind_hdr_t*)buffer);
}

static void AssignManagementId (void) {
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


static void ReadTracePath (void) {
	unsigned short i=0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC, (byte*)"\x06\x80\x00\x00\x00\x00\x00");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_READ;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in ReadTracePath, MAN_READ errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1, wait_ind = 1; wait_rc || wait_ind;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: ReadTracePath, MAN_READ Rc=%02x", (byte)pInd->hdr.rc.Rc);
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

static void ReadInitVars (void) {
	unsigned short i=0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;
	char name[64];
	byte *ie;
	byte ie_length;

	add_p(tx_data,&i,ESC,(byte*)"\x13\x80\x00\x00\x00\x00\x0DTrace\\History");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_READ;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in ReadInitVars, MAN_READ errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1, wait_ind = 1; wait_rc || wait_ind;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				break;
			} else {
				HistoryPresent = 1;
			}
			continue;
		}
		if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
			if (pInd->hdr.ind.Ind == MAN_INFO_IND) {
				int length = pInd->data_length;
				ie = ((byte *) &pInd->data_length)+4;
				while ((length>0) && (*ie++ == 127)) {
					ie_length = *ie++;
					length -= ie_length;
					strncpy(name, (char*)ie+6, *(ie+5));
					name[*(ie+5)] = '\0';
					if (!strcmp("Trace\\History\\History Length", name)) {
						HistoryLength = *(ie+6+*(ie+5));
					} else if (!strcmp("Trace\\History\\Triggered Event", name)) {
						TriggeredEvents = *(ie+6+*(ie+5));
					}
					ie += ie_length;
				}	
				wait_ind = 0;
			}
		}
	}
}

static void SelectTraceDir (void) {
	unsigned short i=0;
	int wait_ind, wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x0b\x80\x00\x00\x00\x00\x05Trace");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_READ;
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

static void TraceEventOn (void) {
	unsigned short i = 0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x16\x80\x00\x00\x00\x00\x10Trace\\Log Buffer");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EVENT_ON;
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

static void TraceEventOff (void) {
	unsigned short i = 0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,(byte*)"\x16\x80\x00\x00\x00\x00\x10Trace\\Log Buffer");
	single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EVENT_OFF;
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
				printf ("A: TraceEventOff, MAN_EVENT_OFF Rc=%02x", \
																							(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void TriggeredEventOn (void) {
	unsigned short i = 0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x23\x80\x00\x00\x00\x00\x1dTrace\\History\\Triggered Event");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EVENT_ON;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in TriggeredEventOn, MAN_EVENT_ON errno=%d\n", errno);
		exit (1);
	}
	for (wait_ind = 1, wait_rc = 1; wait_ind || wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: TriggeredEventsOn, MAN_EXECUTE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
			continue;
		}
		if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
			if (pInd->hdr.ind.Ind == MAN_EVENT_IND) {
				wait_ind = 0;
			}
		}
	}

}

#if 0
static void TriggeredEventOff (void) {
	unsigned short i = 0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,(byte*)"\x23\x80\x00\x00\x00\x00\x1dTrace\\History\\Triggered Event");
	single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EVENT_OFF;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in TriggeredEventOff, MAN_EVENT_OFF errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: TriggeredEventsOff, MAN_EVENT_OFF Rc=%02x", \
																							(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

}
#endif

static void S_TraceEventOff (void) {
	unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask &= ~TM_STRING;
  add_p (tx_data,&i,ESC,
        (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in DebugTracesON, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: StringEventOFF, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;

	add_p (tx_data,&i,ESC,
						(byte*)"\x1d\x80\x00\x00\x00\x04\x13Trace\\Misc Txt Mask\x00\x00\x00\x00");
	single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in S_TraceEventOff, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: S_TraceEventOff, MAN_WRITE Rc=%02x", \
																							(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void L1EventsOFF (void) {
}

static void ModemEventsOFF (void) {
}

static void NetEventsOFF (void) {
}

#if 0
static void L1EventsON (void) {
}

static void ModemEventsON (void) {
}

static void NetEventsON (void) {
}
#endif

void DebugTracesOFF (void) {
  unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask &= ~TM_N_LAYER;

	add_p (tx_data,&i,ESC,
					(byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in S_TraceEventOff, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: S_TraceEventOff, MAN_WRITE Rc=%02x", \
																							(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void DebugTracesON (void) {
	unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask |= TM_N_LAYER;

  add_p (tx_data,&i,ESC,
        (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in DebugTracesON, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: DebugTracesON, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

void StartHistory (void) {
	unsigned short i = 0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x21\x80\x02\x00\x00\x00\x1bTrace\\History\\Start History");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EXECUTE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in StartHistory, MAN_EXECUTE errno=%d\n", errno);
		exit (1);
	}

	for (wait_ind = 1, wait_rc = 1; wait_ind || wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: StartHistory, MAN_EXECUTE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
			continue;
		}
		if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
			if (pInd->hdr.ind.Ind == MAN_EVENT_IND) {
				wait_ind = 0;
			}
		}
	}
}

void StopHistory (void) {
	unsigned short i = 0;
	int wait_rc, wait_ind;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x20\x80\x02\x00\x00\x00\x1aTrace\\History\\Stop History");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EXECUTE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in StopHistory, MAN_EXECUTE errno=%d\n", errno);
		exit (1);
	}

	for (wait_ind = 1, wait_rc = 1; wait_ind || wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: StopHistory, MAN_EXECUTE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
			continue;
		}
		if (wait_ind && (pInd->type == DIVA_UM_IDI_IND)) {
			if (pInd->hdr.ind.Ind == MAN_EVENT_IND) {
				wait_ind = 0;
			}
		}
	}
}

void ClearHistory (void) {
	unsigned short i = 0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p(tx_data,&i,ESC,(byte*)"\x21\x80\x02\x00\x00\x00\x1bTrace\\History\\Clear History");
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req					=	MAN_EXECUTE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in ClearHistory, MAN_EXECUTE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: ClearHistory, MAN_EXECUTE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
			continue;
		}
	}
}

void DEventsOFF (void) {
  unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask &= ~TM_D_CHAN;
	std_trc_mask &= ~TM_C_COMM;
	std_trc_mask &= ~TM_DL_ERR;
	std_trc_mask &= ~TM_LAYER1;

  add_p (tx_data,&i,ESC,
        (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in DebugTracesOFF, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: DebugTracesOFF, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void DEventsON (void) {
  unsigned short i=0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask |= TM_D_CHAN;
	std_trc_mask |= TM_C_COMM;
	std_trc_mask |= TM_DL_ERR;
	std_trc_mask |= TM_LAYER1;

	add_p (tx_data,&i,ESC,
				 (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in DEventsON, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: DEventsON, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void StringEventOn (void) {
	unsigned short i = 0;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	std_trc_mask |= TM_STRING;

  add_p (tx_data,&i,ESC,
        (byte*)"\x1a\x80\x87\x00\x25\x02\x12Trace\\Event Enable\xff\x00");

  tx_data[i-2] = LOBYTE(std_trc_mask);
  tx_data[i-1] = HIBYTE(std_trc_mask);

  single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in DebugTracesON, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: StringEventON, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;

	add_p (tx_data,&i,ESC,
					(byte*)"\x1d\x80\x00\x00\x00\x04\x13Trace\\Misc Txt Mask\x3f\xff\xff\xff");
	single_p (tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in StringEventON, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: StringEventON, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
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
		printf ("A: error in B_TraceSize, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: B_TraceSize, MAN_WRITE Rc=%02x", (byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void events (void) {
  if(!EPrint) return;

	if (VisualStream != stderr) {
  	fprintf(VisualStream, "\r  Events:");
   	fprintf(VisualStream, " %4ld",TrcEvent);
	}
}


static void trace_set_a_channel (dword channel,
                                 unsigned short timeslot_trace) {
	unsigned short i=0;
	unsigned short j;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,
				 (byte*)"\x23\x80\x00\x00\x00\x08\x15Trace\\AudioTS# Enable\x00\x00\x00\x00\x00\x00\x00\x00");
	if ((channel != 0) && timeslot_trace)
	{
		for (j = 0x23+2 - 8; j < 0x23+2; j++)
			tx_data[j] = 0xff;
	}
	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_a_timeslot, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if ((pInd->hdr.rc.Rc != 0xff) && (channel != 0) && timeslot_trace) {
				printf ("A: trace_set_a_timeslot, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;
	add_p (tx_data,&i,ESC,
				 (byte*)"\x1f\x80\x00\x00\x00\x04\x15Trace\\AudioCh# Enable\x00\x00\x00\x00");

	memcpy (tx_data+0x1f+2-4, &channel, 4);
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_a_channel, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: trace_set_a_channel, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void trace_set_eye_channel (dword channel,
                                   unsigned short timeslot_trace) {
	unsigned short i=0;
	unsigned short j;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,
				 (byte*)"\x21\x80\x00\x00\x00\x08\x13Trace\\EyeTS# Enable\x00\x00\x00\x00\x00\x00\x00\x00");
	if ((channel != 0) && timeslot_trace)
	{
		for (j = 0x21+2 - 8; j < 0x21+2; j++)
			tx_data[j] = 0xff;
	}
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_eye_timeslot, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if ((pInd->hdr.rc.Rc != 0xff) && (channel != 0) && timeslot_trace) {
				printf ("A: trace_set_eye_timeslot, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;
	add_p (tx_data,&i,ESC,
				 (byte*)"\x1d\x80\x00\x00\x00\x04\x13Trace\\EyeCh# Enable\x00\x00\x00\x00");

	memcpy (tx_data+0x1d+2-4, &channel, 4);
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_eye_channel, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: trace_set_eye_channel, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void trace_set_b_channel (dword channel,
                                 unsigned short line_side_trace, unsigned short timeslot_trace) {
	unsigned short i=0;
	unsigned short j;
	int wait_rc;
	diva_um_idi_ind_hdr_t* pInd;

	add_p (tx_data,&i,ESC,
				 (byte*)"\x23\x80\x00\x00\x00\x08\x15Trace\\Line Side B-TS#\x00\x00\x00\x00\x00\x00\x00\x00");
	if ((channel != 0) && line_side_trace && timeslot_trace)
	{
		for (j = 0x23+2 - 8; j < 0x23+2; j++)
			tx_data[j] = 0xff;
	}
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_line_side_b_timeslot, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if ((pInd->hdr.rc.Rc != 0xff) && (channel != 0) && line_side_trace && timeslot_trace) {
				printf ("A: trace_set_line_side_b_timeslot, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;
	add_p (tx_data,&i,ESC,
				 (byte*)"\x1f\x80\x00\x00\x00\x04\x15Trace\\Line Side B-Ch#\x00\x00\x00\x00");
	if ((channel != 0) && line_side_trace)
		memcpy (tx_data+0x1f+2-4, &channel, 4);
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_line_side_b_channel, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if ((pInd->hdr.rc.Rc != 0xff) && (channel != 0) && line_side_trace) {
				printf ("A: trace_set_line_side_b_channel, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;
	add_p (tx_data,&i,ESC,
				 (byte*)"\x20\x80\x00\x00\x00\x08\x12Trace\\B-TS# Enable\x00\x00\x00\x00\x00\x00\x00\x00");
	if ((channel != 0) && timeslot_trace)
	{
		for (j = 0x20+2 - 8; j < 0x20+2; j++)
			tx_data[j] = 0xff;
	}
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_b_timeslot, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if ((pInd->hdr.rc.Rc != 0xff) && (channel != 0) && timeslot_trace) {
				printf ("A: trace_set_b_timeslot, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}

	i = 0;
	add_p (tx_data,&i,ESC,
				 (byte*)"\x1c\x80\x00\x00\x00\x04\x12Trace\\B-Ch# Enable\x00\x00\x00\x00");
	memcpy (tx_data+0x1c+2-4, &channel, 4);
	single_p(tx_data,&i,0); /* End Of Message */

	tx_req->Req = MAN_WRITE;
	tx_req->data_length = (dword)i;

	if (diva_put_req (entity, tx_req, sizeof(*tx_req)+tx_req->data_length) < 0) {
		printf ("A: error in trace_set_b_channel, MAN_WRITE errno=%d\n", errno);
		exit (1);
	}

	for (wait_rc = 1; wait_rc;) {
		pInd  = diva_man_wait_msg (rx_data, sizeof(rx_data));
		if (wait_rc && (pInd->type == DIVA_UM_IDI_IND_RC)) {
			wait_rc = 0;
			if (pInd->hdr.rc.Rc != 0xff) {
				printf ("A: trace_set_b_channel, MAN_WRITE Rc=%02x",
																								(byte)pInd->hdr.rc.Rc);
				exit (2);
			}
		}
	}
}

static void diva_waitkey (void) {
	printf \
	(" ---------------------- please press any key to continue ----------------------\n");
  getchar();
}

/* ----------------------------------------------------------
		The RB consist from 4 segments:
		mlog.0
		mlog.1
		mlog.2
		mlog.3

		Working segment is segment 0. Every tyme if this segment receives
		full this routine is called, and following is done:

		1. fflush (LogStream); fclose (LogStream);
		2. delete mlog.3
		2. rename mlog.2 to mlog.3
		3. rename mlog.1 to mlog.2
		4. rename mlog.0 to mlog.1
		5. create mlog.0 again

	 ---------------------------------------------------------- */
static void rotate_ring_buffer_files (void) {
	if (LogStream) {
		if (fflush (LogStream) < 0) {
			fprintf (VisualStream, "E: can't flush mlog.0. ERRNO=%d\n", errno);
  	  exit(5);
		}
		if (fclose (LogStream) < 0) {
			fprintf (VisualStream, "E: can't close mlog.0. ERRNO=%d\n", errno);
			exit(5);
		}
		LogStream = 0;
	}

	if ((unlink ("mlog.3") < 0) && (errno != ENOENT)) {
		fprintf (VisualStream, "E: can't remove mlog.3. ERRNO=%d\n", errno);
		exit(5);
	}
	if ((rename ("mlog.2", "mlog.3") < 0) && (errno != ENOENT)) {
		fprintf (VisualStream, "E: can't move mlog.2 to mlog.3. ERRNO=%d\n", errno);
		exit (5);
	}
	if ((rename ("mlog.1", "mlog.2") < 0) && (errno != ENOENT)) {
		fprintf (VisualStream, "E: can't move mlog.1 to mlog.2. ERRNO=%d\n", errno);
		exit (5);
	}
	if ((rename ("mlog.0", "mlog.1") < 0) && (errno != ENOENT)) {
		fprintf (VisualStream, "E: can't move mlog.0 to mlog.1. ERRNO=%d\n", errno);
		exit (5);
	}

	if (!(LogStream = fopen("mlog.0", "wb"))) {
		printf("Can't not open mlog.0. ERRNO=%d\n", errno);
		exit (5);
	}
}

static void diva_mlog_init_rb (void) {
	if (!LogStream) {
		const char* open_mode = "w";
		sprintf (LogFileName,"mlog%d.txt",controller);

		if (DbgFileWrap != (unsigned long)(-1)) {
			printf ("  --------     RING BUFFER mode is active     --------\n");
			printf ("  ring buffer segments: mlog.0->mlog.1->mlog.2->mlog.3\n");
			printf ("  segment size: %ld KBytes\n", DbgFileWrap*4/1024);
			rotate_ring_buffer_files ();
			rotate_ring_buffer_files ();
			rotate_ring_buffer_files ();
			rotate_ring_buffer_files ();
			rotate_ring_buffer_files ();
		} else {
			if (!(LogStream = fopen(LogFileName, open_mode))) {
				printf("Can't not open Logfile <%s>!\n", LogFileName);
				exit(4);
			}
		}
   fprintf (LogStream,
	  "Management Interface Trace/Log utility for EICON DIVA adapters\n");
   fprintf (LogStream, "BUILD (%s-%s-%s)\n", DIVA_BUILD, __DATE__, __TIME__);
   fprintf (LogStream, "Copyright (c) 1991-2007 Dialogic\n\n");
   fprintf (LogStream,
           "===============================================================\n");
	}
}

static void single_p(byte * P, word * PLength, byte Id) {
  P[(*PLength)++] = Id;
}

/*------------------------------------------------------------------*/
/* append a multiple byte information element to an IDI messages    */
/*------------------------------------------------------------------*/
static void add_p(byte * P, word * PLength, byte Id, byte * w) {
  P[(*PLength)++] = Id;
  memcpy(&P[*PLength], w, w[0]+1);
  *PLength +=w[0]+1;
}

static diva_um_idi_ind_hdr_t* diva_man_wait_msg_and_cmd (byte* buffer,
																												 int length) {

	diva_wait_idi_message_and_stdin (entity);
	if (diva_get_message (entity, buffer, length) <= 0) {
		printf ("A: can't get message, errno=%d\n", errno);
		exit (3);
	}

	return ((diva_um_idi_ind_hdr_t*)buffer);
}

void mlog_process_stdin_cmd (const char* cmd) {
	switch (cmd[0]) {
		case 't':
		case 'T':
			if (!TTrace) {
				TTrace = 1;
        fprintf(VisualStream, "\r                            \n");
        fprintf(VisualStream, "  Switching Debug events ON   \n");
        events();
				DebugTracesON();
			} else {
				TTrace = 0;
        fprintf(VisualStream, "\r                            \n");
        fprintf(VisualStream, "  Switching Debug events OFF  \n");
        events();
				DebugTracesOFF();
			}
			break;

	  case 'd':
		case 'D':
			if (!DTrace) {
				DTrace = 1;
        fprintf(VisualStream, "\r                            \n");
        fprintf(VisualStream, "  Switching D-Channel events ON   \n");
        events();
				DEventsON();
			} else {
				DTrace = 0;
        fprintf(VisualStream, "\r                            \n");
        fprintf(VisualStream, "  Switching D-Channel events OFF  \n");
        events();
				DEventsOFF();
			}
			break;

	  case 's':
		case 'S':
			if (!STrace) {
				STrace = 1;
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching S events ON   \n");
        events();
				StringEventOn ();
			} else {
				STrace = 0;
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching S events OFF  \n");
        events();
        S_TraceEventOff();
			}
			break;

		case 'h':
		case '?':
      mlog_help ();
			break;

    case 'b':
      if(BTrace)
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching B events OFF\n");
        events();
        BTrace = 0;
        trace_set_b_channel (0, LineSideTrace, TimeslotTrace);
      }
      else
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching B events ON\n");
        events();
        BTrace = 1;
        trace_set_b_channel (0xffffffff, LineSideTrace, TimeslotTrace);
      }
      break;
      
		case 'o':
		case 'O':
			if (HistoryPresent) {
			      fprintf(VisualStream, "\r                        \n");
			      fprintf(VisualStream, "  Starting History\n");
			      events();
			      ClearHistory();
			      StartHistory();
			      if (ETrace != 0) {
				      TraceEventOff();
			  	    ETrace = 0;
				}
			}
      break;
		case 'g':
		case 'G':
			if (HistoryPresent) {
			      fprintf(VisualStream, "\r                        \n");
			      fprintf(VisualStream, "  Stopping History\n");
			      events();
				  if (ETrace != 0) {
				      TraceEventOff();
				}
			      StopHistory();
			      ReadInitVars();
		   		trace_set_b_channel (0xffffffff, LineSideTrace, TimeslotTrace);
		   		trace_set_a_channel (0xffffffff, TimeslotTrace);
			      trace_set_eye_channel (0xffffffff, TimeslotTrace);
				  TraceEventOn();
				  ETrace = 1;
				 while (HistoryLength>0) {
					 diva_um_idi_ind_hdr_t* pInd = diva_man_wait_msg (rx_data, sizeof(rx_data));
					 if (pInd->type == DIVA_UM_IDI_IND) {
						 IdiMessageInput (pInd);
					 }
				 }
				trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
				trace_set_a_channel (a_trace_mask, TimeslotTrace);
				trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
			}
		break;
    case 'F':
    case 'f': {
			word trc_len;
      if ((trc_len = (word)atoi(&cmd[1]))) {
      	EPrint = 0;
      	maxout = trc_len;
      	fprintf(VisualStream,
								"\r                                                   \n");
      	if (maxout > 2048) {
					maxout = 2048;
				}
      	fprintf (VisualStream,
								 "  Using B-channel trace length of %d bytes\n", maxout);
      	B_TraceSize (maxout);
      	EPrint = 1;
			}
     } break;

    case 'e':
    case 'E':
      if(ETrace != 0) {
        fprintf(VisualStream, "\r                            \n");
        fprintf(VisualStream, "  Switching Trace events OFF\n");
        events();
        ETrace = 0;
        TraceEventOff();
      } else {
        fprintf(VisualStream, "\r                           \n");
        fprintf(VisualStream, "  Switching Trace events ON\n");
        ETrace = 1;
        TraceEventOn();
      }
      break;

    case 'j':
      if (LineSideTrace)
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching media side OFF\n");
        events();
        LineSideTrace = 0;
        trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
      }
      else
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching media side ON\n");
        events();
        LineSideTrace = 1;
        trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
      }
      break;

    case 'k':
      if (TimeslotTrace)
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching NULL PLCI OFF\n");
        events();
        TimeslotTrace = 0;
        trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
        trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
        trace_set_a_channel (a_trace_mask, TimeslotTrace);
      }
      else
      {
        fprintf(VisualStream, "\r                        \n");
        fprintf(VisualStream, "  Switching NULL PLCI ON\n");
        events();
        TimeslotTrace = 1;
        trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
        trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
        trace_set_a_channel (a_trace_mask, TimeslotTrace);
      }
      break;

		case '#':
      EPrint = 0;
      fprintf(VisualStream,
							"\r                                                   \n");
			sscanf(&cmd[1], "%x", &b_trace_mask);
      fprintf(VisualStream,
							"  Set the B-channel trace mask to 0x%08x\n", b_trace_mask);
			trace_set_b_channel (b_trace_mask, LineSideTrace, TimeslotTrace);
      EPrint = 1;
      break;

		case 'A':
      EPrint = 0;
      fprintf(VisualStream,
							"\r                                                   \n");
			sscanf(&cmd[1], "%x", &a_trace_mask);
      fprintf(VisualStream,
							"  Set the audio sample trace mask to 0x%08x\n", a_trace_mask);
			trace_set_a_channel (a_trace_mask, TimeslotTrace);
      EPrint = 1;
      break;

		case 'Y':
      EPrint = 0;
      fprintf(VisualStream,
							"\r                                                   \n");
			sscanf(&cmd[1], "%x", &eye_trace_mask);
      fprintf(VisualStream,
							"  Set the audio sample trace mask to 0x%08x\n", eye_trace_mask);
			trace_set_eye_channel (eye_trace_mask, TimeslotTrace);
      EPrint = 1;
      break;

		case 'q':
		case 'Q':
			quit = 1;
			if (ETrace != 0) {
				TraceEventOff ();
			}
			RemoveManagementId ();
			exit (0);
			break;

		default:
			printf ("unknown command <%s>\n", cmd);
	}
}

