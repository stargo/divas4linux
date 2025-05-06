
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
#include "dlist.h"

/*
**  Initialize linked list
*/

void
diva_q_init (diva_entity_queue_t* q)
{
  q->head = q->tail = 0;
}

/*
**  Remove element from linked list
*/
void
diva_q_remove (diva_entity_queue_t* q, diva_entity_link_t* what)
{
  if(!what->prev) {
    if ((q->head = what->next)) {
      q->head->prev = 0;
    } else {
      q->tail = 0;
    }
  } else if (!what->next) {
    q->tail = what->prev;
    q->tail->next = 0;
  } else {
    what->prev->next = what->next;
    what->next->prev = what->prev;
  }
  what->prev = what->next = 0;
}

/*
**  Add element to the tail of linked list
*/
void
diva_q_add_tail (diva_entity_queue_t* q, diva_entity_link_t* what)
{
  what->next = 0;
  if (!q->head) {
    what->prev = 0;
    q->head = q->tail = what;
  } else {
    what->prev = q->tail;
    q->tail->next = what;
    q->tail = what;
  }
}

/*
**  Add element to the linked list after a specified element
*/
void diva_q_insert_after (diva_entity_queue_t* q, diva_entity_link_t* prev, diva_entity_link_t* what)
{
  diva_entity_link_t *next;
  
  if((0 == prev) || (0 == (next=diva_q_get_next(prev)))){
    diva_q_add_tail(q,what);
    return;
  }
  what->prev  = prev;
  what->next  = next;
  prev->next  = what;
  next->prev  = what;
}

/*
**  Add element to the linked list before a specified element
*/
void diva_q_insert_before (diva_entity_queue_t* q, diva_entity_link_t* next, diva_entity_link_t* what)
{
  diva_entity_link_t *prev;
  
  if(!next){
    diva_q_add_tail(q,what);
    return;
  }
  if(0 == (prev=diva_q_get_prev(next))){/*next seems to be the head*/
    q->head=what;
    what->prev=0;
    what->next=next;
    next->prev=what;
  }else{ /*not the head*/
    what->prev  = prev;
    what->next  = next;
    prev->next  = what;
    next->prev  = what;
  }
}

diva_entity_link_t* diva_q_find (const diva_entity_queue_t* q, const void* what,
             diva_q_cmp_fn_t cmp_fn)
{
  diva_entity_link_t* diva_q_current = q->head;

  while (diva_q_current) {
    if (!(*cmp_fn)(what, diva_q_current)) {
      break;
    }
    diva_q_current = diva_q_current->next;
  }

  return (diva_q_current);
}

diva_entity_link_t*
diva_q_get_head (const diva_entity_queue_t* q)
{
  return (q->head);
}

diva_entity_link_t*
diva_q_get_tail (const diva_entity_queue_t* q)
{
  return (q->tail);
}

diva_entity_link_t*
diva_q_get_next	(const diva_entity_link_t* what)
{
  return ((what) ? what->next : 0);
}

diva_entity_link_t*
diva_q_get_prev	(const diva_entity_link_t* what)
{
  return ((what) ? what->prev : 0);
}

int
diva_q_get_nr_of_entries (const diva_entity_queue_t* q)
{
  int i = 0;
  const diva_entity_link_t* diva_q_current = q->head;

  while (diva_q_current) {
    i++;
    diva_q_current = diva_q_current->next;
  }

  return (i);
}

