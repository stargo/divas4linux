
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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

#include "diva_mem.h"


void* diva_mem_malloc (unsigned int length) {
	if (length) {
		void* ret;
		ret = ((void*)vmalloc ((unsigned int)length));
		/* DBG_TRC(("DIVA alloc=%p", ret)) */
		return (ret);
	}
	return (0);
}

void diva_mem_free (void* ptr) {
	if (ptr) {
		/* DBG_TRC(("DIVA free=%p", ptr)) */
		vfree (ptr);
	}
}

