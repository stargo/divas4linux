
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

#ifndef __DIVA_MALLOC_H__
#define __DIVA_MALLOC_H__

/* Constants */
#define MALLOC_FLAG_MEMZERO    0x0008  /* Set to 8 to be compatible with Windows' HEAP_ZERO_MEMORY constant */




/* Function declarations */
#if defined(__cplusplus)
extern "C" {
#endif

void diva_malloc_init(void);

void *diva_os_malloc(dword flags, dword memSize);
void diva_os_free(dword flags, void *p);

/* Functions to be used with memory check */
void diva_malloc_emergency_free(void);
void diva_malloc_mem_set_static(void);
void diva_malloc_memstat_print(void);    /* Print memory usage statistics to Diva Trace */
void diva_malloc_checkdeleted(void);
void diva_malloc_checkactive(void); 
dword diva_malloc_mem_bytes_current(void);
dword diva_malloc_mem_bytes_max(void);
dword diva_malloc_mem_bytes_static(void);
dword diva_malloc_mem_blocks_current(void);
dword diva_malloc_mem_blocks_max(void);
dword diva_malloc_mem_blocks_static(void);

void diva_malloc_deinit(void);

#if defined(__cplusplus)
}
#endif


#endif

/* end of file */

