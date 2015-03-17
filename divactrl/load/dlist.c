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
 *****************************************************************************/
#include "platform.h"
#include "dlist.h"

/*
**  Initialize linked list
*/

void diva_q_init (diva_entity_queue_t* q)
{
  memset (q, 0x00, sizeof(*q));
}

/*
**  Remove element from linked list
*/
void diva_q_remove (diva_entity_queue_t* q, diva_entity_link_t* what)
{
  if(!what->prev) {
    if (0 != (q->head = what->next)) {
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
void diva_q_add_tail (diva_entity_queue_t* q, diva_entity_link_t* what)
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

diva_entity_link_t* diva_q_get_tail (const diva_entity_queue_t* q)
{
  return (q->tail);
}

diva_entity_link_t* diva_q_get_next	(const diva_entity_link_t* what)
{
  return ((what) ? what->next : 0);
}

diva_entity_link_t* diva_q_get_prev	(const diva_entity_link_t* what)
{
  return ((what) ? what->prev : 0);
}

int diva_q_get_nr_of_entries (const diva_entity_queue_t* q)
{
  int i = 0;
  const diva_entity_link_t* diva_q_current = q->head;

  while (diva_q_current) {
    i++;
    diva_q_current = diva_q_current->next;
  }

  return (i);
}

