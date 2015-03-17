/* $Id: dqueue.h,v 1.1.2.2 2001/02/08 12:25:43 armin Exp $
 *
 * Driver for Dialogic DIVA Server ISDN cards.
 * User Mode IDI Interface
 *
 * Copyright 2000-2003 by Armin Schindler (mac@melware.de)
 * Copyright 2000-2003 Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007 Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
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
