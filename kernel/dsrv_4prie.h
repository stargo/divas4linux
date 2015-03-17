/*
 *
  Copyright (c) Dialogic, 2008.
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
#ifndef __DIVA_XDI_DSRV_4PRIE_INC__
#define __DIVA_XDI_DSRV_4PRIE_INC__

#define DIVA_4PRIE_REAL_SDRAM_SIZE (64*1024*1024)

void diva_os_prepare_4prie_functions (PISDN_ADAPTER IoAdapter);

#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET 0x3004
#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT (1U << 27)
#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT (1U << 28)

#endif

