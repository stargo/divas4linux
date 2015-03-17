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
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <errno.h>
#define __NO_STANDALONE_DBG_INC__
#include "platform.h"
#include "memorymap.h"
#include "platform.h"
#include "debuglib.h"
#include "di_defs.h"
#include "man_defs.h"
#include "debug_if.h"
#include "dbgioctl.h"
// #include <string.h>
// #include <signal.h>

#include "ditrace.h"

extern unsigned int trace_compress (unsigned char* src, unsigned int src_length, unsigned char* dst);
extern unsigned int trace_expand (unsigned char* src, unsigned int src_length, unsigned char* dst);
extern word maxout;
extern word o;

extern int dbg_rtp_filter_ssrc;
extern dword dbg_rtp_ssrc;
extern int dbg_rtp_filter_payload;
extern dword dbg_rtp_payload;
extern dword dbg_rtp_direction;

extern void display_q931_message (char*, void*);

int diva_decode_message (diva_dbg_entry_head_t* pmsg, int length);
int diva_read_binary_ring_buffer (char *filename);
int add_file (byte** , const char* file, int limit);
int add_string_to_header (byte** data,
																 const char* subject,
																 const char* data_str,
																 int limit);
int diva_trace_init ();
static int check_id (int id);

/*
  IMPORTS
  */
extern FILE* VisualStream;
extern FILE* DebugStream;
extern void monitorLogs (void *inputFile, char *buf, unsigned long *bufSize) ;
extern void infoAnalyse (FILE *out, DbgMessage *msg) ;

extern void	pppAnalyse  (FILE *out, DbgMessage *msg) ;
extern void __pppAnalyse (FILE *out, unsigned char *data, unsigned long data_len, unsigned long real_frame_length);

extern void	rtpAnalyse  (FILE *out, DbgMessage *msg, int rx) ;

extern void	tapiFilter  (FILE *out, DbgMessage *msg) ;
extern void dilogAnalyse (FILE *out, DbgMessage *msg, int DilogFormat) ;
extern void xlogAnalyse (FILE *out, DbgMessage *msg) ;
extern word xlog(byte *stream, void * buffer);

extern diva_cmd_line_parameter_t* ditrace_find_entry (const char* c);
extern int ditrace_list_drivers (char* out, int length);
extern int diva_open_maint_driver (void);

extern FILE* OutStream;
extern int use_mlog_time_stamp;
extern int silent_operation;
extern dword ppp_analysis_mask;
extern dword vsig_analysis_mask;
extern dword rtp_analysis_mask;

extern int no_headers;
extern dword b_channel_mask;
extern int no_dchannel;
extern int no_strings;

#define DIVA_MAINT_IDS 255
extern int id_list[DIVA_MAINT_IDS];
extern int max_id_index;
extern dword decoded_trace_events_mask;

extern int debug;

static char mnemonic[] = "LFETX   RMSITB  tncsxr  0123    ";
static const char* selective_trace_pattern = "Selective Trace ON (2048) for Ch=";
static int selective_trace_pattern_length;
static const char* selective_trace_off_pattern = "Selective Trace OFF for Ch=";
static int selective_trace_off_pattern_length;
static const char* trace_lencth_cmd_pattern = "*** MAINT TRACELENGTH";
static int trace_lencth_cmd_pattern_length;
static int DilogFormat = 0;
int term_signal = 0;

#define DIVA_TRACE_HEADER_TITLE "\n# HEADER SUBJECT: "

extern int diva_maint_uses_ioctl;

/******************************************************************************/
static int check_b_channel (const char* p) {
	int ch_value = 0;

	if (p && p[0] == '[') {
		if (p[2] == ',') {
			p += 3;
			if (*p == '*')
				return (0);
			ch_value = *p - '0';
		} else if (p[3] == ',') {
			p += 4;
			ch_value = *p - '0';
		}
		if (ch_value > 0) {
			if (p[2] == ']') {
				ch_value = ch_value * 10 + p[1] - '0';
			}
			if ((b_channel_mask & (1 << (ch_value-1))) == 0)
				return (-1);
		}
	}

	return (0);
}

int diva_decode_message (diva_dbg_entry_head_t* pmsg, int length) {
  byte* data = (byte*)&pmsg[1];
  dword DL_index, DT_index, DLI_name;
  static dword sequence = 0;
  dword hours, min, sec, msec;
  DbgMessage* dbg_msg;
  char msg_data [4*MSG_FRAME_MAX_SIZE+sizeof(*dbg_msg)+1];
	int message_too_long = 0;

  dbg_msg = (DbgMessage*)msg_data;
  DLI_name = DLI_NAME(pmsg->dli) ;	/* numeric debug level	*/
  DL_index = DL_INDEX(pmsg->dli) ;	/* position of mask bit */
  DT_index = DT_INDEX(pmsg->dli) ;	/* basic message type	*/

  if (pmsg->facility != MSG_TYPE_MLOG) {
    if (DL_index >= 32) {
      fprintf (DebugStream, "I: corrupted message, index=%d\n", DL_index);
      return (-1);
    }

    if (DLI_name == DLI_XLOG) {
      switch (data[1]) {
        case 0x01:	/* B-X */
          DL_index = DL_SEND;
          break;
        case 0x02:	/* B-R */
  				DL_index = DL_RECV;
          break;

        case 0x03:	/* D-X */
        case 0x04:	/* D-R */
        case 0x05:	/* SIG_EVENT */
        case 0x15:	/* xlog_sig */
        case 0x16:	/* event */
          DL_index = DL_CONN;
          break ;

        case 0x06:	/* low layer indications     */
        case 0x07:	/* low layer requests      s */
        case 0x0E:	/* network layer indications */
        case 0x0F:	/* network layer requests    */
          DL_index = DL_STAT;
          break;

        case 0x09:	/* MDL-ERROR */
          DL_index = DL_ERR;
          break ;

        case 0x14:	/* L1-Up, ... */
          DL_index = DL_LOG;
          break ;

        case 0x17:	/* EYE */
          DL_index = DL_PRV0;
          break;

        default:
          DL_index = DL_TRC ; break ;
      }
    }
  }

  switch (pmsg->facility) {
    case MSG_TYPE_DRV_ID:
    case MSG_TYPE_FLAGS:
      break;

    default:
      if (sequence > pmsg->sequence) {
        fprintf (OutStream, "sequence mismatch %d / %d\n",
                 sequence, pmsg->sequence);
      } else {
        if ((++sequence) < pmsg->sequence) {
          fprintf (OutStream, "==> missing sequence %d - %d\n",
                   sequence, pmsg->sequence - 1) ;
        }
      }
      sequence = pmsg->sequence;
  }

	if (check_id (pmsg->drv_id)) {
		return (0);
	}

	if (decoded_trace_events_mask) {
		dword mask = (pmsg->dli >> 8) & 0xff;
		if (mask > 4) {
			mask--;
		}
		if (!(mask & decoded_trace_events_mask)) {
			return (0);
		}
	}

  sec  = pmsg->time_sec;
  msec = pmsg->time_usec/1000;

  hours = sec/3600;
  min   = (sec - hours*3600)/60;
  sec   = sec - hours*3600 - min*60;



  if (pmsg->facility == MSG_TYPE_MLOG) {
    MI_XLOG_HDR *TrcData;
    byte tmp_xlog_buffer[8*4096];
    word tmp_xlog_pos;
		byte* b_channel_data;
		word message_code;

    TrcData = (MI_XLOG_HDR *)&pmsg[1];
		b_channel_data = (byte*)&TrcData->code;
		message_code = *(word*)b_channel_data;

		if (no_dchannel != 0) {
			switch ((byte)message_code) {
				case  3:
				case  4:
				case  5:
				case  9:
				case 20:
				case 21:
				case 22:
					return (0);
			}
		}

		if (((byte)message_code) == 24 && no_strings != 0) {
			return (0);
		}

		if (b_channel_mask != 0) {
			switch ((byte)message_code) {
				case 1: /* B-X */
				case 2: /* B-R */
				case 23: /* EYE */
				case 25: /* SAMPLE */
					if ((message_code & 0xff00) != 0 &&
							(b_channel_mask & (1 << ((byte)((message_code >> 8) - 1)))) == 0)
						return (0);
					break;
				case 24:
					if (check_b_channel ((char*)(b_channel_data+2)) != 0)
						return (0);
					break;
			}
		}

    if (use_mlog_time_stamp) {
      sec  = TrcData->time/1000;
      msec = TrcData->time - sec*1000;
      hours = sec/3600;
      min   = (sec - hours*3600)/60;
      sec   = sec - hours*3600 - min*60;
    }
    fprintf (OutStream, "%2d:%02d:%02d.%03d ", hours, min, sec, msec);
    fprintf (OutStream, "%c %X ", '-', (int)pmsg->drv_id) ;

    tmp_xlog_buffer[0] = 0;
    tmp_xlog_pos = xlog(&tmp_xlog_buffer[0], (void*)&TrcData->code);
    tmp_xlog_buffer[tmp_xlog_pos] = 0;
    fprintf (OutStream, "%s", &tmp_xlog_buffer[0]);
    fflush (OutStream);
		if (ppp_analysis_mask & PPP_ANALYSE_BCHANNEL) {

			if (((byte)message_code) == 1 /* B-X */ || ((byte)message_code) == 2 /* B-R */) {
				union {
					DbgMessage ppp_msg;
					byte data[sizeof(DbgMessage)+2048+512];
				} ppp_data;
				word total_length = *(word*)&b_channel_data[2];
				byte* ppp_frame = &b_channel_data[4];

				ppp_data.ppp_msg.id = 1;
				ppp_data.ppp_msg.size = MIN(maxout,total_length);
				memcpy (&ppp_data.ppp_msg.data[0], ppp_frame, ppp_data.ppp_msg.size);
				if (total_length > maxout) {
					memset (&ppp_data.ppp_msg.data[ppp_data.ppp_msg.size], 0x00, total_length - maxout);
				}
				ppp_data.ppp_msg.size = total_length;
				__pppAnalyse (OutStream, &ppp_data.ppp_msg.data[0], ppp_data.ppp_msg.size, maxout);
			}

		}
    return (0);
  }

  if (no_strings != 0 && pmsg->facility == MSG_TYPE_STRING) {
		return (0);
	}

  if ((pmsg->facility == MSG_TYPE_STRING) && pmsg->drv_id == 0x7fffffff) {
    fwrite  (&pmsg[1], 1, (word)(length-sizeof(*pmsg) - 1), OutStream);
    fprintf (OutStream, "%s", "\n");
    return (0);
  }

  /*
    Output time stamp
    */
	fprintf (OutStream, "%2d:%02d:%02d.%03d ", hours, min, sec, msec);

  	/*
	    append message type and driver id for regular messages
 	   */
  if ((pmsg->facility != MSG_TYPE_BINARY) && pmsg->drv_id &&
      (DLI_name != DLI_XLOG) && (DLI_name != DLI_MXLOG)) {
    fprintf (OutStream, "%c %X ", mnemonic[DL_index], (int)pmsg->drv_id) ;
	}

  /*
    Construct trace message and pass to decoder
    */
  dbg_msg->NTtime.LowPart	 = 0;
  dbg_msg->NTtime.HighPart = 0;
  dbg_msg->size	= (word)(length-sizeof(*pmsg));
  if (pmsg->facility == MSG_TYPE_STRING) {
    dbg_msg->size--;
  }
  dbg_msg->ffff	= 0xffff ;
  dbg_msg->id		= (word)pmsg->drv_id;
  dbg_msg->type	= (pmsg->dli | pmsg->facility);
  dbg_msg->seq	= pmsg->sequence;
	if (dbg_msg->size > MSG_FRAME_MAX_SIZE) {
    fprintf (OutStream, "message too long (%d > %d)\n{\n", dbg_msg->size, MSG_FRAME_MAX_SIZE);
		dbg_msg->size = MIN(4*MSG_FRAME_MAX_SIZE, dbg_msg->size);
		message_too_long = 1;
		infoAnalyse (OutStream, dbg_msg);
	}
  memcpy (&dbg_msg->data[0], data, dbg_msg->size);

  /*
    convert message body
    */
    switch (pmsg->facility) {
      case MSG_TYPE_STRING:
        if (selective_trace_pattern_length < (int)dbg_msg->size) {
          if (memcmp(&dbg_msg->data[0],
                     selective_trace_pattern,
                     selective_trace_pattern_length) == 0) {
            maxout = 2048;
          }
        }
        if (selective_trace_off_pattern_length < (int)dbg_msg->size) {
          if (memcmp(&dbg_msg->data[0],
                     selective_trace_off_pattern,
                     selective_trace_off_pattern_length) == 0) {
            maxout = 30;
          }
        }
        if (trace_lencth_cmd_pattern_length < (int)dbg_msg->size) {
          if (memcmp(&dbg_msg->data[0],
                     trace_lencth_cmd_pattern,
                     trace_lencth_cmd_pattern_length) == 0) {
						word new_length = atoi((char*)&dbg_msg->data[trace_lencth_cmd_pattern_length]);
						if (new_length <= 3000 && new_length >= 4) {
							maxout = new_length;
						} else {
							maxout = 30;
						}
          }
				}

        tapiFilter (OutStream, dbg_msg) ;
        break;

       case MSG_TYPE_BINARY:
         switch (DLI_name) {
           case DLI_XLOG:
             dilogAnalyse (OutStream, dbg_msg, DilogFormat);
             break;
           case DLI_MXLOG:
             xlogAnalyse (OutStream, dbg_msg);
             break;
           case DLI_BLK:
           case DLI_RECV:
           case DLI_SEND:
             if (!(dbg_rtp_filter_ssrc != 0 && dbg_rtp_filter_payload != 0)) {
             infoAnalyse (OutStream, dbg_msg);
             }
             if (ppp_analysis_mask & PPP_ANALYSE_DATA) {
               pppAnalyse (OutStream, dbg_msg);
             }
             if ((vsig_analysis_mask & VSIG_ANALYSE_DATA) != 0 &&
                 dbg_msg->size > 4 && dbg_msg->size < 512 &&
                 dbg_msg->data[0] == 0x08 && dbg_msg->data[1] == 0x02) {
               byte tmp[16*1024];
               struct {
                 word code;
                 word length;
                 byte info[1000];
               } msg;
               memcpy (&msg.info[4], &dbg_msg->data[0], dbg_msg->size);
               msg.length = (word)(dbg_msg->size + 4);
               tmp[0] = 0;
               o = 0;
               display_q931_message ((char*)tmp, &msg);
               if (tmp[0] != 0)
                fprintf (OutStream, "%s\n", tmp);
						 }
						 if ((DLI_name == DLI_RECV || DLI_name ==  DLI_SEND) && (rtp_analysis_mask & RTP_ANALYSE_DATA) != 0) {
               if (dbg_rtp_direction == 0 || dbg_rtp_direction == 3 ||
                   (DLI_name == DLI_RECV && dbg_rtp_direction == 1) || (DLI_name == DLI_SEND && dbg_rtp_direction == 2)) {
                 rtpAnalyse (OutStream, dbg_msg, DLI_name == DLI_RECV);
							 }
						 }
             break ;
           default:
             pppAnalyse (OutStream, dbg_msg);
             break ;
         }
         break ;

      default:
        fprintf (OutStream,
                 "unknown message type 0x%x, fatal happens !\n",
                 DT_index) ;
  }

	if (message_too_long != 0) {
		fprintf (OutStream, "}\n");
	}

  fflush (OutStream) ;

  return (0);
}

void add_string_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, dword id, const char* str);
void add_file_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, const char* name);

#define queueCount(q)	((q)->Count)
#define MSG_NEED(size) \
	( (sizeof(MSG_HEAD) + size + sizeof(dword) - 1) & ~(sizeof(dword) - 1) )

void queueInit (MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer) {
	Q->Size = sizeBuffer;
	Q->Base = Q->Head = Q->Tail = Buffer;
	Q->High = Buffer + sizeBuffer;
	Q->Wrap = 0;
	Q->Count= 0;
}

byte *queueAllocMsg (MSG_QUEUE *Q, word size) {
	/* Allocate 'size' bytes at tail of queue which will be filled later
   * directly whith callers own message header info and/or message.
   * An 'alloced' message is marked incomplete by oring the 'Size' field
   * whith MSG_INCOMPLETE.
   * This must be reset via queueCompleteMsg() after the message is filled.
   * As long as a message is marked incomplete queuePeekMsg() will return
   * a 'queue empty' condition when it reaches such a message.  */

	MSG_HEAD *Msg;
	word need = MSG_NEED(size);

	if (Q->Tail == Q->Head) {
		if (Q->Wrap || need > Q->Size) {
			return(0); /* full */
		}
		goto alloc; /* empty */
	}

	if (Q->Tail > Q->Head) {
		if (Q->Tail + need <= Q->High) goto alloc; /* append */
		if (Q->Base + need > Q->Head) {
			return (0); /* too much */
		}
		/* wraparound the queue (but not the message) */
		Q->Wrap = Q->Tail;
		Q->Tail = Q->Base;
		goto alloc;
	}

	if (Q->Tail + need > Q->Head) {
		return (0); /* too much */
	}

alloc:
	Msg = (MSG_HEAD *)Q->Tail;

	Msg->Size = size | MSG_INCOMPLETE;

	Q->Tail	 += need;
	Q->Count += size;



	return ((byte*)(Msg + 1));
}

void queueFreeMsg (MSG_QUEUE *Q) {
/* Free the message at head of queue */
	
	word size = ((MSG_HEAD *)Q->Head)->Size & ~MSG_INCOMPLETE;

	Q->Head  += MSG_NEED(size);
	if (debug) {
		fprintf (DebugStream, "MSG_NEED(size) = %d\n", (int)MSG_NEED(size));
	}
	Q->Count -= size;
	if (debug) {
		fprintf (DebugStream, "Count now %0x\n", Q->Count);
	}

	if (Q->Wrap) {
		if (Q->Head >= Q->Wrap) {
			if (debug) {
				fprintf (DebugStream, "Warp activated\n");
			}
			Q->Head = Q->Base;
			Q->Wrap = 0;
		}
	} else if (Q->Head >= Q->Tail) {
		if (debug) {
			fprintf (DebugStream, "Head (%p) >= Tail (%p)\n", Q->Head, Q->Tail);
		}
		Q->Head = Q->Tail = Q->Base;
	}
}

byte *queuePeekMsg (MSG_QUEUE *Q, word *size) {
	/* Show the first valid message in queue BUT DONT free the message.
   * After looking on the message contents it can be freed queueFreeMsg()
   * or simply remain in message queue.  */

	MSG_HEAD *Msg = (MSG_HEAD *)Q->Head;

	if ((byte *)Msg == NULL) {
		if (debug) {
			fprintf (DebugStream, "Msg == NULL, terminating...\n");
		}
		return (0);
	} else if ((byte *)Msg == Q->Tail && !Q->Wrap) {
		if (debug) {
			fprintf (DebugStream, "End of queue reached and warp not activated, terminating...\n");
		}
		return (0);
	} else if (Msg->Size & MSG_INCOMPLETE) {
		if (debug) {
			fprintf (DebugStream, "Msg->Size & MSG_INCOMPLETE, terminating...\n");
		}
		return (0);
	}
	*size = Msg->Size;
	if (debug) {
		fprintf(DebugStream, "Size = %d\n", Msg->Size);
	}
	return ((byte *)(Msg + 1));
}


void sig_handler (int sig_nr) {
	term_signal = 1;
}

int diva_read_binary_ring_buffer (char *filename) {
  byte* rb, *map_base, *org_base;
  MSG_QUEUE dbg_queue_32bit;
  MSG_QUEUE* dbg_queue;
  diva_dbg_entry_head_t* pmsg;
  word size;
  int   header_number = 1, show_jump = 1;
  dword header_length;
	int compression_used = 0;
	byte uncompressed_data[24*1024];
	unsigned int uncompressed_length;
	int main64bit_buffer = 0;

  if ((rb = memorymap(filename)) == (void*)-1) {
    return (-1);
  }

  map_base = rb;
  if (*(dword*)rb != (dword)DBG_MAGIC) {
    fprintf (VisualStream, "ERROR: Invalid File Format\n");
	unmemorymap(map_base);
    return (-1);
  }
  rb += sizeof(dword);

  while ((header_length = *(dword*)rb)) {
    rb += sizeof(dword);

		header_number++;
		if (no_headers == 0) {
			if ((*rb == 0x00) || (*rb == 0xff)) {
				/*
					Extension for the future binary headers
					*/

				fprintf (OutStream, "Ignore binary header Nr:%d\n", header_number-2);
			} else {
				if (header_number == 2) {
					fprintf (OutStream, "%s", (char*)rb);
				} else {
					if (show_jump) {
						show_jump = 0;
						fprintf (OutStream, "%s",
									 "\n\n# ================================================================== \n#\n");
						fprintf (OutStream, "%s",
							"# PLEASE GO DIRECTLY TO 'START TRACE DATA' IF THE SYSTEM DIAGNOSTIC\n");
						fprintf (OutStream, "%s", "# MESSAGES ARE NOT RELEVANT TO YOU.\n");
						fprintf (OutStream, "%s",
									 "#\n# ================================================================== \n\n");
					}
					fprintf (OutStream,
									 "\n# ==================== START TRACE HEADER NR:%d ==================== \n\n",
									 header_number-2);
					if (!compression_used) {
						char* p = strstr ((char*)rb, TRACE_COMPRESSION_TAG);

						if ((compression_used = (p && (*(p+strlen(TRACE_COMPRESSION_TAG)+1) == 0)))) {
							fprintf (OutStream, "%s", "turn decompression on");
						}
					}
					fprintf (OutStream, "%s", (char*)rb);
					fprintf (OutStream,
									 "\n# ====================  END TRACE HEADER NR:%d  ==================== \n\n",
									 header_number-2);
				}
	    }
		}

    rb += header_length;
  }
  rb += sizeof(dword);

	fprintf (OutStream,
					 "\n# ====================    START TRACE DATA     ==================== \n\n");

  if (!(org_base = *(byte**)rb)) {
    fprintf (VisualStream, "ERROR: Invalid File Format\n");
	unmemorymap (map_base);
    return (-1);
  }

  rb += sizeof(void*);
	if (debug) {
		fprintf (DebugStream, "rb now %p\n", rb);
	}

	/*
		Check if this is 64 bit maint buffer
		*/
	if (sizeof(void*) == 4) {
		if (ditrace_find_entry ("64bit")->found == 1) {
			main64bit_buffer = 1;
		} else if (ditrace_find_entry ("no64bit")->found == 0) {
			byte* test_ptr = (byte*)rb;

			if (READ_DWORD(test_ptr) == 0xffffff00) {
				test_ptr += sizeof(dword);
				test_ptr += sizeof(dword);
				if (READ_DWORD(test_ptr) == 0) {
					test_ptr += 2*sizeof(dword);
					if (READ_DWORD(test_ptr) == 0xffffff00) {
						test_ptr += 2*sizeof(dword);
						if (READ_DWORD(test_ptr) == 0xffffff00) {
							test_ptr += 2*sizeof(dword);
							if (READ_DWORD(test_ptr) == 0xffffff00) {
								test_ptr += 2*sizeof(dword);
								if (READ_DWORD(test_ptr) == 0xffffff00) {
									main64bit_buffer = 1;
									fprintf (OutStream,"%s", "INFO: detected 64bit maint buffer.\n\n");
									fflush (OutStream);
								}
							}
						}
					}
				}
			}
		}
	}

	if (main64bit_buffer == 0 || sizeof(void*) > 4) {
		dbg_queue = (MSG_QUEUE*)rb;
	} else {
		byte* q_ptr;

		rb += sizeof(void*);
		q_ptr = (byte*)rb;
		dbg_queue = &dbg_queue_32bit;

		dbg_queue_32bit.Size  = READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.Base  = (byte*)(unsigned long)READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.High  = (byte*)(unsigned long)READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.Head  = (byte*)(unsigned long)READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.Tail  = (byte*)(unsigned long)READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.Wrap  = (byte*)(unsigned long)READ_DWORD(q_ptr); q_ptr += 2*sizeof(dword);
		dbg_queue_32bit.Count = READ_DWORD(q_ptr);
	}

  if (!(dbg_queue->Count && dbg_queue->Count)) {
    fprintf (VisualStream, "INFO: Empty File\n");
	unmemorymap (map_base);
    return (0);
  }

  dbg_queue->Base = rb + ((byte*)(dbg_queue->Base) - (byte*)org_base);
  dbg_queue->High = rb + ((byte*)(dbg_queue->High) - (byte*)org_base);
  dbg_queue->Head = rb + ((byte*)(dbg_queue->Head) - (byte*)org_base);
  dbg_queue->Tail = rb + ((byte*)(dbg_queue->Tail) - (byte*)org_base);
  if (dbg_queue->Wrap) {
    dbg_queue->Wrap = rb + ((byte*)(dbg_queue->Wrap) - (byte*)org_base);
  }
	if (debug) {
		fprintf(DebugStream, "map_base %p, org_base %p, Base %p, High %p, Head %p, Tail %p, Warp %p\n", 
			map_base, org_base, dbg_queue->Base, dbg_queue->High, dbg_queue->Head, dbg_queue->Tail, dbg_queue->Wrap);
	}

  while ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
		if (compression_used) {
			uncompressed_length = trace_expand   ((byte*)pmsg, size, uncompressed_data);
			diva_decode_message ((diva_dbg_entry_head_t*)uncompressed_data, uncompressed_length);
		} else {
			diva_decode_message (pmsg, size);
		}
    queueFreeMsg (dbg_queue);
  }

  unmemorymap(map_base);

  return (0);
}

int add_string_to_header (byte** data,
																 const char* subject,
																 const char* data_str,
																 int limit) {
	int str_len = strlen(data_str);
	int subject_len = strlen(subject);
	int length = str_len + subject_len + 3 + sizeof(dword) +
								sizeof(DIVA_TRACE_HEADER_TITLE)-1; /* 2*CR + \nSUBJECT: + terminating zero + size */
	byte* p = *data + sizeof(dword);

  length += getpagesize();
  length  = (length/getpagesize())*getpagesize();

	if (length < limit) {
		memcpy (p, DIVA_TRACE_HEADER_TITLE, sizeof(DIVA_TRACE_HEADER_TITLE)-1);
		p += sizeof(DIVA_TRACE_HEADER_TITLE) - 1;
		memcpy (p, subject, subject_len);
		p += subject_len;
		*p++ = '\n';
		*p++ = '\n';
		memcpy (p, data_str, str_len);
		p += str_len;
		*p = 0;

		*(dword*)*data = (dword)(length - sizeof(dword));

		*data += length;

		return (length);

	}

	return (0);
}

int add_file (byte** data, const char* file, int limit) {
	struct stat s;
	int fd;
	int name_len = strlen(file);
	int length = name_len + 3 + sizeof(dword) +
								sizeof(DIVA_TRACE_HEADER_TITLE)-1; /* 2*CR + \nSUBJECT: + terminating zero + size */
	byte* p = *data + sizeof(dword);

	if ((fd = open (file, O_RDONLY)) < 0) {
		return (0);
	}
	if (fstat (fd, &s) < 0) {
		close(fd);
		return(0);
	}

	length += s.st_size;
  length += getpagesize();
  length  = (length/getpagesize())*getpagesize();

	if (length < limit) {
		memcpy (p, DIVA_TRACE_HEADER_TITLE, sizeof(DIVA_TRACE_HEADER_TITLE)-1);
		p += sizeof(DIVA_TRACE_HEADER_TITLE) - 1;
		memcpy (p, file, name_len);
		p += name_len;
		*p++ = '\n';
		*p++ = '\n';
		if (read (fd, p, s.st_size) != s.st_size) {
			return (0);
		}
		p += s.st_size;
		*p = 0;

		*(dword*)*data = (dword)(length - sizeof(dword));

		*data += length;

		return (length);
	}

	return (0);
}

void add_string_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, dword id, const char* str) {
	diva_dbg_entry_head_t *pmsg;
	int str_length = strlen (str), length = str_length + sizeof(*pmsg) + 1;
	byte *data_buffer = malloc(length);
	byte *pdata;
	byte *compressed_data = malloc(2*length);
	dword compressed_length;
  word size;


	pmsg = (diva_dbg_entry_head_t*)&data_buffer[0];
	pmsg->sequence    = *sequence;
	pmsg->time_sec    = 0;
	pmsg->time_usec   = 0;
	pmsg->facility    = MSG_TYPE_STRING;
	pmsg->dli         = DLI_LOG;
	pmsg->drv_id      = id;
	pmsg->di_cpu      = 0;
	pmsg->data_length = length - sizeof(*pmsg);
 	memcpy (&pmsg[1], str, pmsg->data_length);
	pdata = (byte*)pmsg;


	if ((ditrace_find_entry ("Compress")->found != 0)) {
		compressed_length = trace_compress (pdata, length, &compressed_data[0]);
		pdata = &compressed_data[0];
		length = (int)compressed_length;
	}

  while (!(pmsg = (diva_dbg_entry_head_t*)queueAllocMsg (dbg_queue, (word)length))) {
    if ((pmsg = (diva_dbg_entry_head_t*)queuePeekMsg (dbg_queue, &size))) {
      queueFreeMsg (dbg_queue);
    } else {
      break;
    }
  }

  if (pmsg) {
    memcpy (pmsg, pdata, length);
    queueCompleteMsg (pmsg);
  }
  (*sequence)++;
  free(data_buffer);
  free(compressed_data);
}

void add_file_to_buffer (MSG_QUEUE* dbg_queue, dword* sequence, const char* name) {
	struct stat s;
  dword max_length = 0x1fff;
	int fd;


	if ((fd = open (name, O_RDONLY)) < 0) {
		return;
	}

  add_string_to_buffer (dbg_queue, sequence, 0, name);

	if (fstat (fd, &s) < 0) {
		add_string_to_buffer (dbg_queue, sequence, 0x7fffffff, "Failed to get file length");
		close(fd);
		return;
	}

  while (s.st_size > 0) {
		dword length = MIN(s.st_size, max_length);
		char *data_buffer;
		data_buffer = malloc(length+1);

		if (read (fd, data_buffer, length) == length) {
			data_buffer[length] = 0;
			add_string_to_buffer (dbg_queue, sequence, 0x7fffffff, data_buffer);
		} else {
			add_string_to_buffer (dbg_queue, sequence, 0x7fffffff, "File read error");
		}

		s.st_size -= length;
  }

  close (fd);
}

static int check_id (int id) {
	int i;

	if (no_headers != 0 && ((dword)id) == 0x7fffffff)
		return (-1);

	if (!(max_id_index && id)) {
		return (0);
	}

	for (i = 0; i < max_id_index; i++) {
		if (id == id_list[i]) {
			return (0);
		}
	}

	return (-1);
}



int diva_trace_init () {
 	char *unused;
 	unused = (char *)ditrace_parameters;
 	unused = (char *)ditrace_options;
  selective_trace_pattern_length = strlen (selective_trace_pattern);
  selective_trace_off_pattern_length = strlen (selective_trace_off_pattern);
	trace_lencth_cmd_pattern_length = strlen (trace_lencth_cmd_pattern);

	return (0);
}
