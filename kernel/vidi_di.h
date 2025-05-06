
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

#ifndef __DIVA_VIDI_DI_H__
#define __DIVA_VIDI_DI_H__

#include "io.h"

int diva_init_vidi (PISDN_ADAPTER IoAdapter);
void diva_cleanup_vidi (PISDN_ADAPTER IoAdapter);
void vidi_host_pr_out(ADAPTER *a);
byte vidi_host_pr_dpc(ADAPTER *a);
byte vidi_host_test_int (ADAPTER * a);
void vidi_host_clear_int(ADAPTER * a);
void diva_vidi_init_adapter_handle_buffer (diva_vidi_adapter_handle_buffer_t* buf, void* data, int length);
int  diva_vidi_adapter_handle_buffer_put  (diva_vidi_adapter_handle_buffer_t* buf, word handle);
word diva_vidi_adapter_handle_buffer_get  (diva_vidi_adapter_handle_buffer_t* buf);
int vidi_pcm_req (PISDN_ADAPTER IoAdapter, ENTITY *e);

#endif
