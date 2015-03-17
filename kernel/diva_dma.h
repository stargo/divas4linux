
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
#ifndef __DIVA_DMA_MAPPING_IFC_H__
#define __DIVA_DMA_MAPPING_IFC_H__
typedef struct _diva_dma_map_entry  diva_dma_map_entry_t;
struct _diva_dma_map_entry* diva_alloc_dma_map (void* os_context, int nentries, int first);
void diva_init_dma_map_entry (struct _diva_dma_map_entry* pmap,
                              int nr, void* virt, unsigned long phys,
                              void* addr_handle);
void diva_init_dma_map_entry_hi (struct _diva_dma_map_entry* pmap,
                                 int nr, unsigned long phys_hi);
int diva_alloc_dma_map_entry (struct _diva_dma_map_entry* pmap);
void diva_free_dma_map_entry (struct _diva_dma_map_entry* pmap, int entry);
void diva_get_dma_map_entry (struct _diva_dma_map_entry* pmap, int nr,
                             void** pvirt, unsigned long* pphys);
void diva_get_dma_map_entry_hi (struct _diva_dma_map_entry* pmap, int nr, unsigned long* pphys_hi);
void diva_free_dma_mapping (struct _diva_dma_map_entry* pmap, int last);
void diva_reset_dma_mapping (struct _diva_dma_map_entry* pmap);
int  diva_nr_free_dma_entries (struct _diva_dma_map_entry* pmap);
/*
  Functionality to be implemented by OS wrapper
  and running in process context
  */
void diva_init_dma_map (void* hdev,
                        struct _diva_dma_map_entry** ppmap,
                        int nentries);
void diva_init_dma_map_pages (void* hdev,
                        struct _diva_dma_map_entry** ppmap,
                        int nentries, int pages);
void diva_free_dma_map (void* hdev,
                        struct _diva_dma_map_entry* pmap);
void diva_free_dma_map_pages (void* hdev,
                        struct _diva_dma_map_entry* pmap, int pages);
void* diva_get_entry_handle (struct _diva_dma_map_entry* pmap, int nr);
#define DIVA_DMA_CARD_ID_TYPE(__card_id__) ((byte)(((dword)(__card_id__)) >> 28))
#define DIVA_DMA_CARD_ID(__card_id__)      ((((dword)(__card_id__)) & ~0xf0000000) - 1)
#define DIVA_DMA_CREATE_CARD_ID(__card_id_type__,__card_id__) \
  ((((dword)((__card_id__)+1)) & ~0xf0000000) | ((((dword)(__card_id_type__)) & 0x0000000f) << 28))
#define DIVA_DMA_CARD_ID_SERVER  0x00
#define DIVA_DMA_CARD_ID_SOFT_IP 0x01
void *diva_dma_bus_to_virtual_address (dword card_id,
                                       dword bus_address_low,
                                       dword bus_address_high,
                                       dword *entry_index);
#endif
