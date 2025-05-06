
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

#ifndef __DIVA_PCI_INTERFACE_H__
#define __DIVA_PCI_INTERFACE_H__

struct _diva_os_xdi_adapter;
void *divasa_remap_pci_bar(struct _diva_os_xdi_adapter *a,
                           unsigned long bar,
                           unsigned long area_length);
void divasa_unmap_pci_bar(void *bar);
unsigned int divasa_get_pci_irq(unsigned char bus,
                                unsigned char func, void *pci_dev_handle);
unsigned long divasa_get_pci_bar(unsigned char bus,
                                 unsigned char func,
                                 int bar, void *pci_dev_handle);
int diva_os_get_pci_revision_id(const void* pci_dev_handle, byte* prevId);
byte diva_os_get_pci_bus(void *pci_dev_handle);
byte diva_os_get_pci_func(void *pci_dev_handle);
void* diva_hw_ifc_alloc_dma_page (void* hdev, dword* lo, dword* hi);
void diva_hw_ifc_free_dma_page (void* hdev, void* addr, dword lo, dword hi);

#endif
