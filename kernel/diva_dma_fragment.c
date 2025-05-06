
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

#include "platform.h"
#include "dlist.h"
#include "diva_dma.h"
#include "diva_dma_fragment.h"

/*
 * Assumes following map of DMA entries
 * BITS [0...7]  - 8 Bit nr of map entry
 * BITS [8...10] - 3 Bit fragment number
 * BITS [15]     - set to 1 if uses fragment map
 */
typedef struct _diva_dma_fragment_map_entry {
  diva_entity_link_t link;
  word dma_map_entry;
  byte dma_map_entry_fragments;
} diva_dma_fragment_map_entry_t;

typedef struct _diva_dma_fragment_map {
  struct _diva_dma_map_entry* dma_map;
  diva_dma_fragment_map_entry_t* dma_fragment_map_entries;
  diva_entity_queue_t free_q;
  diva_entity_queue_t available_q;
} diva_dma_fragment_map_t;

struct _diva_dma_fragment_map* diva_init_dma_fragment_map (struct _diva_dma_map_entry* dma_map, int nr_entries /* 1 ... 30 */) {
  diva_dma_fragment_map_t* fragment_map;

  if (dma_map == 0 || nr_entries < 1 || nr_entries > 30) {
    DBG_ERR(("%s %p %d", __FUNCTION__, dma_map, nr_entries))
    return (0);
  }

  fragment_map = diva_os_malloc (0, sizeof(*fragment_map));

  if (fragment_map != 0) {
    fragment_map->dma_fragment_map_entries = diva_os_malloc (0, sizeof(diva_dma_fragment_map_entry_t)*nr_entries);
    if (fragment_map->dma_fragment_map_entries != 0) {
      int i;

      fragment_map->dma_map = dma_map;
      diva_q_init (&fragment_map->free_q);
      diva_q_init (&fragment_map->available_q);

      for (i = 0; i < nr_entries; i++) {
        diva_q_add_tail (&fragment_map->free_q, &fragment_map->dma_fragment_map_entries[i].link);
        fragment_map->dma_fragment_map_entries[i].dma_map_entry = 0xffff;
        fragment_map->dma_fragment_map_entries[i].dma_map_entry_fragments = 0;
      }

      return (fragment_map);
    }
    diva_os_free (0, fragment_map);
  }

  return (0);
}


int diva_dma_fragment_map_alloc (struct _diva_dma_fragment_map* fragment_map,
                                 struct _diva_dma_map_entry* dma_map,
                                 dword length,
                                 diva_os_spin_lock_t* data_spin_lock) {

  if (unlikely(length > DIVA_DMA_FRAGMENT_SIZE || fragment_map == 0)) {
    diva_os_spin_lock_magic_t irql;
    int entry_nr;

    diva_os_enter_spin_lock (data_spin_lock, &irql, "alloc");
    entry_nr = diva_alloc_dma_map_entry (dma_map);
    diva_os_leave_spin_lock (data_spin_lock, &irql, "alloc");

    return (entry_nr);
  }

  if (fragment_map != 0) {
    diva_entity_link_t* link = diva_q_get_head (&fragment_map->available_q);
    word nr;

    if (link != 0) {
      diva_dma_fragment_map_entry_t* map_entry = DIVAS_CONTAINING_RECORD(link, diva_dma_fragment_map_entry_t, link);

      if (map_entry->dma_map_entry_fragments != 0xff) {
        word i;

        for (i = 0; i < 8; i++) {
          if ((map_entry->dma_map_entry_fragments & (1U << i)) == 0) {
            map_entry->dma_map_entry_fragments |= (1U << i);
            nr = (word)(map_entry - &fragment_map->dma_fragment_map_entries[0]) | i << 8 | 1U << 15;
            if (map_entry->dma_map_entry_fragments == 0xff) {
              diva_q_remove   (&fragment_map->available_q, &map_entry->link);
              diva_q_add_tail (&fragment_map->available_q, &map_entry->link);
            }
#if 0
            DBG_TRC(("[%p] dma fragment: %04x dma:%u map:%u segment:%u",
										fragment_map,
										nr,
										map_entry->dma_map_entry,
										(word)(map_entry - &fragment_map->dma_fragment_map_entries[0]),
										i))
#endif

            return (nr);
          }
        }
      }
    }

    link = diva_q_get_head (&fragment_map->free_q);
    if (link != 0) {
      diva_os_spin_lock_magic_t irql;
      diva_dma_fragment_map_entry_t* map_entry = DIVAS_CONTAINING_RECORD(link, diva_dma_fragment_map_entry_t, link);
      int entry_nr;

      diva_os_enter_spin_lock (data_spin_lock, &irql, "alloc");
      entry_nr = diva_alloc_dma_map_entry (fragment_map->dma_map);
      diva_os_leave_spin_lock (data_spin_lock, &irql, "alloc");

      if (entry_nr >= 0) {
        diva_q_remove (&fragment_map->free_q, &map_entry->link);
				link = diva_q_get_head (&fragment_map->available_q);
        diva_q_insert_before (&fragment_map->available_q, link, &map_entry->link);
        map_entry->dma_map_entry = (word)entry_nr;
        map_entry->dma_map_entry_fragments = 1;
				nr = (word)(map_entry - &fragment_map->dma_fragment_map_entries[0]) | 1U << 15;
#if 0
        DBG_TRC(("[%p] new dma fragment: %04x dma:%u map:%u segment:%u",
								 fragment_map,
                 nr,
                 map_entry->dma_map_entry,
                 (word)(map_entry - &fragment_map->dma_fragment_map_entries[0]),
                 0))
				DBG_LOG(("[%p] fragments in use: %d", fragment_map, diva_q_get_nr_of_entries (&fragment_map->available_q)))
#endif

        return (nr);
      }
    }
  }

  return (-1);
}

void diva_dma_fragment_map_get_dma_map_entry (struct _diva_dma_fragment_map* fragment_map,
                                              struct _diva_dma_map_entry* pmap,
                                              word nr, void** pvirt, unsigned long* pphys) {
  if (likely(nr != 0xffff)) {
    if (likely(fragment_map != 0 && (nr & (1U << 15)) != 0)) {
      diva_dma_fragment_map_entry_t* map_entry = &fragment_map->dma_fragment_map_entries[nr & 0xff];
      byte fragment_nr       = (byte)((nr >> 8) & 0x07);
			dword offset = DIVA_DMA_FRAGMENT_SIZE * fragment_nr;
			void* virt;
			unsigned long phys;

			diva_get_dma_map_entry (fragment_map->dma_map, map_entry->dma_map_entry, &virt, &phys);

			*pvirt = ((byte*)virt) + offset;
			*pphys  = phys + offset;
    } else {
      diva_get_dma_map_entry (pmap, nr, pvirt, pphys);
    }
  }
}

void diva_dma_fragment_map_get_dma_map_entry_hi (struct _diva_dma_fragment_map* fragment_map,
                                                 struct _diva_dma_map_entry* pmap,
                                                 word nr, unsigned long* pphys_hi) {
  if (likely(nr != 0xffff)) {
    if (likely(fragment_map != 0 && (nr & (1U << 15)) != 0)) {
      diva_dma_fragment_map_entry_t* map_entry = &fragment_map->dma_fragment_map_entries[nr & 0xff];
      diva_get_dma_map_entry_hi (fragment_map->dma_map, map_entry->dma_map_entry, pphys_hi);
    } else {
      diva_get_dma_map_entry_hi (pmap, nr, pphys_hi);
    }
  }
}

void diva_dma_fragment_map_free  (struct _diva_dma_fragment_map* fragment_map,
                                  struct _diva_dma_map_entry* dma_map,
                                  word nr,
                                  diva_os_spin_lock_t* data_spin_lock) {
  if (likely(nr != 0xffff)) {
    diva_os_spin_lock_magic_t irql;

    if (likely(fragment_map != 0 && (nr & (1U << 15)) != 0)) {
      diva_dma_fragment_map_entry_t* map_entry = &fragment_map->dma_fragment_map_entries[nr & 0xff];
      byte fragment_nr                         = (byte)((nr >> 8) & 0x07);

      map_entry->dma_map_entry_fragments &= ~(1 << fragment_nr);
      diva_q_remove (&fragment_map->available_q, &map_entry->link);
#if 0
			DBG_TRC(("[%p] free dma fragment: %04x, dma:%u map:%u segment:%u", fragment_map, nr, map_entry->dma_map_entry, nr & 0xff, fragment_nr))
#endif
      if (map_entry->dma_map_entry_fragments == 0) {
#if 0
				DBG_LOG(("[%p] fragments in use: %d", fragment_map, diva_q_get_nr_of_entries (&fragment_map->available_q)))
#endif
        diva_os_enter_spin_lock (data_spin_lock, &irql, "free");
        diva_free_dma_map_entry (fragment_map->dma_map, map_entry->dma_map_entry);
        diva_os_leave_spin_lock (data_spin_lock, &irql, "free");
        map_entry->dma_map_entry = 0xffff;
        diva_q_add_tail (&fragment_map->free_q, &map_entry->link);
      } else {
				diva_q_insert_before (&fragment_map->available_q, diva_q_get_head (&fragment_map->available_q), &map_entry->link);
      }
    } else {
      diva_os_enter_spin_lock (data_spin_lock, &irql, "free");
      diva_free_dma_map_entry (dma_map, nr);
      diva_os_leave_spin_lock (data_spin_lock, &irql, "free");
    }
  }
}

void diva_reset_dma_fragment_map (struct _diva_dma_fragment_map* fragment_map) {
  if (fragment_map != 0) {
    diva_entity_link_t* link;

    while ((link = diva_q_get_head (&fragment_map->available_q)) != 0) {
      diva_dma_fragment_map_entry_t* map_entry = DIVAS_CONTAINING_RECORD(link, diva_dma_fragment_map_entry_t, link);

      diva_free_dma_map_entry (fragment_map->dma_map, map_entry->dma_map_entry);
      map_entry->dma_map_entry           = 0xffff;
      map_entry->dma_map_entry_fragments = 0;

      diva_q_remove   (&fragment_map->available_q, &map_entry->link);
      diva_q_add_tail (&fragment_map->free_q, &map_entry->link);
    }
  }
}

void diva_destroy_dma_fragment_map (struct _diva_dma_fragment_map* fragment_map) {
  if (fragment_map != 0) {
    diva_reset_dma_fragment_map (fragment_map);
    diva_os_free (0, fragment_map->dma_fragment_map_entries);
    diva_os_free (0, fragment_map);
  }
}

