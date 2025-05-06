
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

#ifndef __DIVA_XDI_OS_PART_H__
#define __DIVA_XDI_OS_PART_H__


int divasa_xdi_driver_entry(void);
void divasa_xdi_driver_unload(void);
void *diva_driver_add_card(void *pdev, unsigned long CardOrdinal, int msi, int dac_disabled);
void diva_driver_remove_card(void *pdiva, int* msi);
int diva_driver_stop_card_no_io (void* pdiva);
void diva_restart_hotplug_wait_q (void);

typedef int (*divas_xdi_copy_to_user_fn_t) (void *os_handle, void *dst,
					    const void *src, int length);

typedef int (*divas_xdi_copy_from_user_fn_t) (void *os_handle, void *dst,
					      const void *src, int length);

int diva_xdi_read(void *adapter, void *os_handle, void *dst,
		  int max_length, divas_xdi_copy_to_user_fn_t cp_fn);

int diva_xdi_write(void *adapter, void *os_handle, const void *src,
		   int length, divas_xdi_copy_from_user_fn_t cp_fn);

void *diva_xdi_open_adapter(void *os_handle, const void *src,
			    int length,
			    divas_xdi_copy_from_user_fn_t cp_fn);

void diva_xdi_close_adapter(void *adapter, void *os_handle);

void diva_os_shedule_user_mode_event (dword adapter_nr);

typedef enum _diva_user_mode_helper_event {
	DivaUserModeHelperEventAdapterInserted,
	DivaUserModeHelperEventAdapterInsertedNoCfg
} diva_user_mode_helper_event_t;
void diva_os_notify_user_mode_helper (diva_user_mode_helper_event_t event, dword cardType, dword serialNumber);

int diva_xdi_write_direct (void *os_handle, const void *src, int length, divas_xdi_copy_from_user_fn_t cp_fn);

#endif
