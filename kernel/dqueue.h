
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

#ifndef _DIVA_USER_MODE_IDI_DATA_QUEUE_H__
#define _DIVA_USER_MODE_IDI_DATA_QUEUE_H__

struct _DIVA_MSG_QUEUE;
typedef struct _diva_um_idi_data_queue {
	struct _DIVA_MSG_QUEUE* pQ;
	int max_length;
} diva_um_idi_data_queue_t;

int diva_data_q_init(diva_um_idi_data_queue_t* q, int max_length, int max_segments);
int diva_data_q_finit(diva_um_idi_data_queue_t * q);
int diva_data_q_get_max_length(const diva_um_idi_data_queue_t * q);
void *diva_data_q_get_segment4write(diva_um_idi_data_queue_t* q, int length);
void diva_data_q_ack_segment4write(void* msg);
const void *diva_data_q_get_segment4read(const diva_um_idi_data_queue_t* q);
int diva_data_q_get_segment_length(const diva_um_idi_data_queue_t * q);
void diva_data_q_ack_segment4read(diva_um_idi_data_queue_t * q);

#endif
