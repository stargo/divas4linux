
/*
 *
  Copyright (c) Dialogic(R), 2009.
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

#if !defined(UNIX)
# include "typedefs.h"
#else
#include "platform.h"
#endif
# include "queue_if.h"
# include "log_if.h"
# include "sys_if.h" 

# define MSG_NEED(size) \
	( (sizeof(MSG_HEAD) + size + sizeof(void*) - 1) & ~(sizeof(void*) - 1) )

void queueInit (MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer)
{
	Q->Size = sizeBuffer;
	Q->Base = Q->Head = Q->Tail = Buffer;
	Q->High = Buffer + sizeBuffer;
	Q->Wrap = 0;
	Q->Count= 0;
}

void queueReset (MSG_QUEUE *Q)
{
	queueInit (Q, Q->Base, Q->Size);
}


dword queueGetFree (MSG_QUEUE *Q, dword SplitSize, dword *pSplitCount)
{
	dword n, s, i, fret = 0;

	if (Q->Tail == Q->Head)
		n = (Q->Wrap) ? 0 : (Q->Size & ~(sizeof(void*) - 1));
	else if (Q->Head > Q->Tail)
		n = Q->Head - Q->Tail;
	else
	{
		n = (Q->High - Q->Tail) & ~(sizeof(void*) - 1);
		if (SplitSize == 0)
		{
			if (((dword)(Q->Head - Q->Base)) > n)
				n = Q->Head - Q->Base;
		}
		else
		{
			s = MSG_NEED (SplitSize);
			i = n % s;
			if (((dword)(Q->Head - Q->Base)) > i)
				n = n - i + (Q->Head - Q->Base);
		}
	}
	if (SplitSize != 0)
	{
		s = MSG_NEED (SplitSize);
		if (pSplitCount)
			*pSplitCount = n / s;
		n %= s;
	}
	fret = ((n > sizeof(void*)) ? n - sizeof(void*) : 0);

	return (fret);
}

int queueSpace (MSG_QUEUE *Q, word size)
{ /* look if 'size' bytes will fit in queue */
	int fret = 0;
	word need;

	need = MSG_NEED(size);
	
	if (Q->Tail == Q->Head) {
		fret = (!Q->Wrap && need <= Q->Size);
		return (fret);
	}

	if (Q->Tail > Q->Head) {
		fret = (Q->Tail + need <= Q->High || Q->Base + need <= Q->Head   );
		return (fret);
	}

	fret = (Q->Tail + need <= Q->Head);
	return (fret);
}

byte *queueAllocMsg (MSG_QUEUE *Q, word size)
{ /* Allocate 'size' bytes at tail of queue which will be filled later
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

int queueWriteMsg (MSG_QUEUE *Q, byte *data, word size)
{ /* Allocate space in queue, copy data and mark the message as complete */

	byte *place;

	if ((place = queueAllocMsg (Q, size))) {
		mem_cpy (place, data, size);
		queueCompleteMsg (place);
		return (1);
	} else {
		return (0);
	}
}

void queueFreeMsg (MSG_QUEUE *Q)
{ /* Free the message at head of queue */

	word size = ((MSG_HEAD *)Q->Head)->Size & ~MSG_INCOMPLETE;

	Q->Head  += MSG_NEED(size);
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

byte *queueFreeTail (MSG_QUEUE *Q, word *size)
{ /* Free all messages except the message at head of queue */

	MSG_HEAD *Msg = (MSG_HEAD *)Q->Head;
	byte* fret = 0;

	if ((byte *)Msg == Q->Tail && !Q->Wrap) {
		return (0);
	}

	*size = Msg->Size & ~MSG_INCOMPLETE;

	Q->Tail  = Q->Head + MSG_NEED(*size);
	Q->Wrap  = 0;
	Q->Count = *size;

	fret = ((byte *)(Msg + 1));

	return (fret);
}

byte *queuePeekMsg (MSG_QUEUE *Q, word *size)
{ /* Show the first valid message in queue BUT DONT free the message.
   * After looking on the message contents it can be freed queueFreeMsg()
   * or simply remain in message queue.  */

	MSG_HEAD *Msg = (MSG_HEAD *)Q->Head;

	if (((byte *)Msg == Q->Tail && !Q->Wrap) ||
	    (Msg->Size & MSG_INCOMPLETE)) {
		return (0);
	} else {
		*size = Msg->Size;
		return ((byte *)(Msg + 1));
	}
}
