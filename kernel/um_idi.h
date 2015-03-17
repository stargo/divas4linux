/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
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
/* $Id: um_idi.h,v 1.1.2.2 2002/10/02 14:38:38 armin Exp $ */

#ifndef __DIVA_USER_MODE_IDI_CORE_H__
#define __DIVA_USER_MODE_IDI_CORE_H__


/*
  interface between UM IDI core and OS dependent part
  */
int diva_user_mode_idi_init(void);
void diva_user_mode_idi_finit(void);
int diva_um_idi_nr_of_adapters(void);
void *divas_um_idi_create_entity(dword adapter_nr, void *file);
int divas_um_idi_delete_entity(int adapter_nr, void *entity);

typedef int (*divas_um_idi_copy_to_user_fn_t) (void *os_handle,
					       void *dst,
					       const void *src,
					       int length);
typedef int (*divas_um_idi_copy_from_user_fn_t) (void *os_handle,
						 void *dst,
						 const void *src,
						 int length);

int diva_um_idi_read(void *entity,
		     void *os_handle,
		     void *dst,
		     int max_length, divas_um_idi_copy_to_user_fn_t cp_fn);

int diva_um_idi_write(void *entity,
		      void *os_handle,
		      const void *src,
		      int length, divas_um_idi_copy_from_user_fn_t cp_fn);

int diva_user_mode_idi_ind_ready(void *entity, void *os_handle);
void *diva_um_id_get_os_context(void *entity);
int diva_os_get_context_size(void);
int divas_um_idi_entity_assigned(void *entity);
int divas_um_idi_entity_start_remove(void *entity);

void diva_um_idi_start_wdog(void *entity);
void diva_um_idi_stop_wdog(void *entity);

void* divas_um_idi_create_proxy(void* file);

void divas_um_notify_timeout (void* p_os);

#endif
