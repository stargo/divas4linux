
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

#ifndef __DIVA_DMA_FRAGMENT_H__
#define __DIVA_DMA_FRAGMENT_H__

#define DIVA_DMA_FRAGMENT_SIZE 512U

struct _diva_dma_fragment_map;

/*
 * Init DMA fragment map to map to fragment nr_entries dma entries
 */
struct _diva_dma_fragment_map* diva_init_dma_fragment_map (struct _diva_dma_map_entry* dma_map,
                                                           int nr_entries);
int diva_dma_fragment_map_alloc (struct _diva_dma_fragment_map* fragment_map,
                                 struct _diva_dma_map_entry* dma_map,
                                 dword length,
                                 diva_os_spin_lock_t* data_spin_lock);
void diva_dma_fragment_map_free  (struct _diva_dma_fragment_map* fragment_map,
                                  struct _diva_dma_map_entry* dma_map,
                                  word entry_nr,
                                  diva_os_spin_lock_t* data_spin_lock);
void diva_dma_fragment_map_get_dma_map_entry (struct _diva_dma_fragment_map* fragment_map,
                                              struct _diva_dma_map_entry* pmap,
                                              word nr, void** pvirt, unsigned long* pphys);
void diva_dma_fragment_map_get_dma_map_entry_hi (struct _diva_dma_fragment_map* fragment_map,
                                                 struct _diva_dma_map_entry* pmap,
                                                 word nr, unsigned long* pphys_hi);
void diva_reset_dma_fragment_map (struct _diva_dma_fragment_map* fragment_map);
void diva_destroy_dma_fragment_map (struct _diva_dma_fragment_map* fragment_map);

#endif
