
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
