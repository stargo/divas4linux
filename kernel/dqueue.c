
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
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
#include "dqueue.h"

typedef struct _DIVA_MSG_QUEUE {
  dword Size;   /* total size of queue (constant) */
  byte  *Base;    /* lowest address (constant)    */
  byte  *High;    /* Base + Size (constant)   */
  byte  *Head;    /* first message in queue (if any)  */
  byte  *Tail;    /* first free position      */
  byte  *Wrap;    /* current wraparound position    */
  dword Count;    /* current no of bytes in queue   */
} DIVA_MSG_QUEUE;

typedef struct DIVA_MSG_HEAD {
	volatile dword	Size;		/* size of data following MSG_HEAD	*/
#define DIVA_MSG_INCOMPLETE	0x8000	/* ored to Size until queueCompleteMsg 	*/
} DIVA_MSG_HEAD;

#define queueCompleteMsg(p) do{ ((DIVA_MSG_HEAD *)p - 1)->Size &= ~DIVA_MSG_INCOMPLETE; }while(0)
#define queueCount(q) ((q)->Count)
#define DIVA_MSG_NEED(size) \
  ( (sizeof(DIVA_MSG_HEAD) + size + sizeof(dword) - 1) & ~(sizeof(dword) - 1) )

static void queueInit (DIVA_MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer) {
  Q->Size = sizeBuffer;
  Q->Base = Q->Head = Q->Tail = Buffer;
  Q->High = Buffer + sizeBuffer;
  Q->Wrap = 0;
  Q->Count= 0;
}

static byte *queueAllocMsg (DIVA_MSG_QUEUE *Q, word size) {
  /* Allocate 'size' bytes at tail of queue which will be filled later
   * directly whith callers own message header info and/or message.
   * An 'alloced' message is marked incomplete by oring the 'Size' field
   * whith DIVA_MSG_INCOMPLETE.
   * This must be reset via queueCompleteMsg() after the message is filled.
   * As long as a message is marked incomplete queuePeekMsg() will return
   * a 'queue empty' condition when it reaches such a message.  */

  DIVA_MSG_HEAD *Msg;
  word need = DIVA_MSG_NEED(size);

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
  Msg = (DIVA_MSG_HEAD *)Q->Tail;

  Msg->Size = size | DIVA_MSG_INCOMPLETE;

  Q->Tail  += need;
  Q->Count += size;

  return ((byte*)(Msg + 1));
}

static void queueFreeMsg (DIVA_MSG_QUEUE *Q) {
/* Free the message at head of queue */

  word size = ((DIVA_MSG_HEAD *)Q->Head)->Size & ~DIVA_MSG_INCOMPLETE;

  Q->Head  += DIVA_MSG_NEED(size);
  Q->Count -= size;

  if (Q->Wrap) {
    if (Q->Head >= Q->Wrap) {
      Q->Head = Q->Base;
      Q->Wrap = 0;
    }
  } else if (Q->Head >= Q->Tail) {
    Q->Head = Q->Tail = Q->Base;
  }
}

static byte *queuePeekMsg (DIVA_MSG_QUEUE *Q, word *size) {
  /* Show the first valid message in queue BUT DONT free the message.
   * After looking on the message contents it can be freed queueFreeMsg()
   * or simply remain in message queue.  */

  DIVA_MSG_HEAD *Msg = (DIVA_MSG_HEAD *)Q->Head;

  if (((byte *)Msg == Q->Tail && !Q->Wrap) ||
      (Msg->Size & DIVA_MSG_INCOMPLETE)) {
    return (0);
  } else {
    *size = (word)Msg->Size;
    return ((byte *)(Msg + 1));
  }
}

int diva_data_q_init(diva_um_idi_data_queue_t * q, int max_length, int max_segments) {
	dword size = max_length*max_segments+sizeof(DIVA_MSG_QUEUE)+max_segments*sizeof(DIVA_MSG_HEAD);
	DIVA_MSG_QUEUE* pQ = (DIVA_MSG_QUEUE*)diva_os_malloc(0, size+64);

	if (pQ != 0) {
		queueInit (pQ, (byte*)&pQ[1], max_length*max_segments);
		q->pQ = pQ;
		q->max_length = max_length;
	} else {
		q->pQ = 0;
	}

	return ((pQ != 0) ? 0 : -1);
}

int diva_data_q_finit(diva_um_idi_data_queue_t * q) {
	if (q && q->pQ) {
		diva_os_free (0, q->pQ);
		q->pQ = 0;
	}

	return (0);
}

int diva_data_q_get_max_length(const diva_um_idi_data_queue_t* q) {
	return (q->max_length);
}

void *diva_data_q_get_segment4write(diva_um_idi_data_queue_t * q, int length) {
	return (queueAllocMsg (q->pQ, (word)length));
}

void diva_data_q_ack_segment4write(void* msg) {
	queueCompleteMsg(msg);
}

const void *diva_data_q_get_segment4read(const diva_um_idi_data_queue_t* q) {
	word size;

	return (queuePeekMsg (q->pQ, &size));
}

int diva_data_q_get_segment_length(const diva_um_idi_data_queue_t * q) {
	word size = 0;

	queuePeekMsg (q->pQ, &size);

	return (size);
}

void diva_data_q_ack_segment4read(diva_um_idi_data_queue_t * q) {
	queueFreeMsg (q->pQ);
}

