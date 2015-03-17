
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

# ifndef QUEUE_IF___H
# define QUEUE_IF___H

typedef struct MSG_QUEUE {
	dword	Size;		/* total size of queue (constant)	*/
	byte	*Base;		/* lowest address (constant)		*/
	byte	*High;		/* Base + Size (constant)		*/
	byte	*Head;		/* first message in queue (if any)	*/
	byte	*Tail;		/* first free position			*/
	byte	*Wrap;		/* current wraparound position 		*/
	dword	Count;		/* current no of bytes in queue		*/
  void* t;        /* align structure to sizeof(void*) */
} MSG_QUEUE;

typedef union MSG_HEAD {
  volatile dword Size;		/* size of data following MSG_HEAD	*/
  void*          t; /* align union to sizeof(void*) */
#define	MSG_INCOMPLETE	0x8000	/* ored to Size until queueCompleteMsg 	*/
} MSG_HEAD;

void queueInit		(MSG_QUEUE *Q, byte *Buffer, dword sizeBuffer);
void queueReset		(MSG_QUEUE *Q);
dword queueGetFree  (MSG_QUEUE *Q, dword SplitSize, dword *pSplitCount);
int  queueSpace		(MSG_QUEUE *Q, word size);
byte *queueAllocMsg	(MSG_QUEUE *Q, word size);
int  queueWriteMsg	(MSG_QUEUE *Q, byte *data, word size);
byte *queuePeekMsg	(MSG_QUEUE *Q, word *size);
void queueFreeMsg	(MSG_QUEUE *Q);
byte *queueFreeTail	(MSG_QUEUE *Q, word *size);

# define queueCompleteMsg(p) { ((MSG_HEAD *)p - 1)->Size &= ~MSG_INCOMPLETE; }

# define queueCount(q)	((q)->Count)

# endif /* QUEUE_IF___H */
