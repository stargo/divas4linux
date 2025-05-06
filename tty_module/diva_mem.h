
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

#ifndef __DIVA_PRIVATE_MMEORY_POOL__
#define __DIVA_PRIVATE_MMEORY_POOL__

# include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)
# include <linux/malloc.h>
#else
# include <linux/slab.h>
#endif

# include <linux/kernel.h>
# include <linux/types.h>
# include <linux/timer.h>
# include <linux/mm.h>
# include <linux/major.h>
# include <linux/string.h>
# include <linux/vmalloc.h>

#endif

