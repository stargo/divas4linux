
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
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

#ifndef __OS__H__
#define __OS__H__

int diva_open_adapter (int adapter_nr);
int diva_close_adapter (int handle);
int diva_put_req (int handle, const void* data, int length);
int diva_get_message (int handle, void* data, int max_length);
int diva_wait_idi_message (int entity);
unsigned long GetTickCount (void);
void diva_wait_idi_message_and_stdin (int entity);
int diva_wait_idi_message_interruptible (int entity, int interruptible);

#define LOBYTE(__x__) ((byte)(__x__))
#define HIBYTE(__x__) ((byte)(((word)(__x__)) >> 8))

#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif
