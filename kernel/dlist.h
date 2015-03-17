/*****************************************************************************
 *
 * (c) COPYRIGHT 2005-2008       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be copied,
 * transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *----------------------------------------------------------------------------*/

#ifndef __DIVA_LINK_H__
#define __DIVA_LINK_H__

struct _diva_entity_link;
typedef struct _diva_entity_link {
	struct _diva_entity_link* prev;
	struct _diva_entity_link* next;
} diva_entity_link_t;

typedef struct _diva_entity_queue {
	diva_entity_link_t* head;
	diva_entity_link_t* tail;
} diva_entity_queue_t;

typedef int (*diva_q_cmp_fn_t)(const void* what,
                               const diva_entity_link_t*);

void diva_q_remove   (diva_entity_queue_t* q, diva_entity_link_t* what);
void diva_q_add_tail (diva_entity_queue_t* q, diva_entity_link_t* what);
void diva_q_insert_after (diva_entity_queue_t* q, diva_entity_link_t* prev, diva_entity_link_t* what);
void diva_q_insert_before (diva_entity_queue_t* q, diva_entity_link_t* next, diva_entity_link_t* what);
diva_entity_link_t* diva_q_find (const diva_entity_queue_t* q,
                                 const void* what, diva_q_cmp_fn_t cmp_fn);

diva_entity_link_t* diva_q_get_head	(const diva_entity_queue_t* q);
diva_entity_link_t* diva_q_get_tail	(const diva_entity_queue_t* q);
diva_entity_link_t* diva_q_get_next	(const diva_entity_link_t* what);
diva_entity_link_t* diva_q_get_prev	(const diva_entity_link_t* what);
int diva_q_get_nr_of_entries (const diva_entity_queue_t* q);
void diva_q_init (diva_entity_queue_t* q);

#endif
