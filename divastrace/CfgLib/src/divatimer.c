
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
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

#include "platform.h"
#include "dlist.h"      /* list management */
#include "divatimer.h"

struct{
  diva_entity_queue_t timer;
  diva_entity_queue_t runouttimer;
}timerqueue;

void diva_timer_init(void)
{ /* first initialization of the timermodule */
  diva_q_init(&timerqueue.timer);
  diva_q_init(&timerqueue.runouttimer);
}

void diva_timer_deinit(void)
{ /* set all timer to runout so it will be signaled */
  timerentry_t *timerentry;

  timerentry=(timerentry_t*)diva_q_get_head(&timerqueue.timer);
  while(timerentry){ /* cleanup user connectiondata */
    timerentry->remainingdeltaclicks=0; /*runs out next click*/
    timerentry=(timerentry_t*)diva_q_get_next(&timerentry->link);
  }
}

void diva_timer_start(timerentry_t *timerentry,dword milliseconds)
{ /* add a new timer */
  timerentry_t *searchtimerentry;
  dword tmpdeltaclicks=0;
  dword deltaclicks;

  deltaclicks=milliseconds/* *?? */;
  /*already in a list*/
  if(timerentry->q) diva_timer_stop(timerentry);
  timerentry->q=&timerqueue.timer;

  /*find location to insert it in the timerlist*/
  searchtimerentry=(timerentry_t*)diva_q_get_head(&timerqueue.timer);
  if(!searchtimerentry){ /*first timer*/
    timerentry->remainingdeltaclicks=deltaclicks;
    diva_q_add_tail(&timerqueue.timer,&timerentry->link);
    return;
  }
  do{
    if((tmpdeltaclicks+searchtimerentry->remainingdeltaclicks)>deltaclicks){
      break; /*found position*/
    }
    tmpdeltaclicks+=searchtimerentry->remainingdeltaclicks;
    searchtimerentry=(timerentry_t*)diva_q_get_next(&searchtimerentry->link);
  }while(searchtimerentry);

  timerentry->remainingdeltaclicks=deltaclicks-tmpdeltaclicks;
  /*insert it in the timerlist*/
  if(!searchtimerentry){ /*last in queue*/
    diva_q_add_tail(&timerqueue.timer,&timerentry->link);
    return;
  }
  /*modify following timerdelta*/
  searchtimerentry->remainingdeltaclicks-=timerentry->remainingdeltaclicks;
  diva_q_insert_before(&timerqueue.timer,&searchtimerentry->link,&timerentry->link);
}

void diva_timer_stop(timerentry_t *timerentry)
{
  timerentry_t *nexttimerentry;

  if(!timerentry || !timerentry->q) return;
  if(timerentry->q==&timerqueue.runouttimer){
    diva_q_remove(&timerqueue.runouttimer,&timerentry->link);
    timerentry->q=0;
    return;
  }
  if(timerentry->q!=&timerqueue.timer) return;
  /*change the time of the following timer*/
  nexttimerentry=(timerentry_t*)diva_q_get_next(&timerentry->link);
  if(nexttimerentry) nexttimerentry->remainingdeltaclicks+=timerentry->remainingdeltaclicks;
  diva_q_remove(&timerqueue.timer,&timerentry->link);
  timerentry->q=0;
}

void diva_timer_klick(dword millisecclicks)
{
  timerentry_t *timerentry;
  dword tmpdeltaclicks;

  timerentry=(timerentry_t*)diva_q_get_head(&timerqueue.timer);
  if(!timerentry) return;
  if(timerentry->remainingdeltaclicks>millisecclicks){ /*no timeout*/
    timerentry->remainingdeltaclicks-=millisecclicks;
    return;
  }
  tmpdeltaclicks=0;
  while(timerentry && (tmpdeltaclicks+timerentry->remainingdeltaclicks)<=millisecclicks){
    tmpdeltaclicks+=timerentry->remainingdeltaclicks;
    /*move entry to the runout queue*/
    diva_q_remove(&timerqueue.timer,&timerentry->link);
    diva_q_add_tail(&timerqueue.runouttimer,&timerentry->link);
    timerentry->q=&timerqueue.runouttimer;
    timerentry=(timerentry_t*)diva_q_get_head(&timerqueue.timer);
  }
  /*set the new timedifference at the first timer*/
  if(timerentry) timerentry->remainingdeltaclicks+=(tmpdeltaclicks-millisecclicks);

  /*signal the runout timer*/
  while(NULL != (timerentry=(timerentry_t*)diva_q_get_head(&timerqueue.runouttimer))){
    diva_q_remove(&timerqueue.runouttimer,&timerentry->link);
    timerentry->q=0;
    timerentry->callb(timerentry); /*signal timer to callback*/
  }
}

