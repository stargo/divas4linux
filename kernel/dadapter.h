
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

#ifndef __DIVA_DIDD_DADAPTER_INC__
#define __DIVA_DIDD_DADAPTER_INC__
void diva_didd_load_time_init (void);
void diva_didd_load_time_finit (void);
int diva_didd_add_descriptor (DESCRIPTOR* d);
int diva_didd_remove_descriptor (IDI_CALL request);
int diva_didd_read_adapter_array (DESCRIPTOR* buffer, int length);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
int diva_get_clock_data (dword* bus_addr_lo,
                         dword* bus_addr_hi,
                         dword* length,
                         void** addr);
#else
int diva_get_clock_data (dword* bus_addr_lo,
                         dword* bus_addr_hi,
                         dword* length,
                         void** addr,
                         void*  pci_dev_handle);
int diva_unmap_clock_data (dword bus_addr_lo,
                           dword bus_addr_hi,
                           void* pci_dev_handle);
#endif
#define OLD_MAX_DESCRIPTORS     16
#define NEW_MAX_DESCRIPTORS     140
#endif
