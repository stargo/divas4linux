
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

#ifndef __DIVA_XDI_DSRV_4PRIE_INC__
#define __DIVA_XDI_DSRV_4PRIE_INC__

#define DIVA_4PRIE_REAL_SDRAM_SIZE (128*1024*1024)

void diva_os_prepare_4prie_functions (PISDN_ADAPTER IoAdapter);

#define DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET 0x200E4

#define DIVA_4PRIE_FS_DSP_RESET_REGISTER_OFFSET 0x3000
#define DIVA_4PRIE_FS_DSP_RESET_BITS            0x00ffffff

#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET 0x3004
#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT  (1U << 27)
#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT  (1U << 28)
#define DIVA_4PRIE_FS_CPU_RESET_REGISTER_FPGA_RESET_BITS (0xfU << 20)

#define DIVA_4PRIE_FS_INTERRUPT_CONTROL_REGISTER 0x7050
#define DIVA_4PRIE_FS_INTERRUPT_CONTROL_REGISTER_INTERRUPT_REGISTER_BIT (1U << 31)

#define DIVA_CUSTOM_CONTROL_REGISTER_OFFSET 0x5000
#define DIVA_CUSTOM_DEVICE_RST_N (1U << 0)
#define DIVA_CUSTOM_DEVICE_CS_N  (1U << 1)
#define DIVA_CUSTOM_DEVICE_ADV_N (1U << 2)
#define DIVA_CUSTOM_DEVICE_WR_N  (1U << 3)
#define DIVA_CUSTOM_DEVICE_OE_N  (1U << 4)

#define DIVA_CUSTOM_LOW_ADDRESS_REGISTER    0x5004
#define DIVA_CUSTOM_HI_ADDRESS_REGISTER     0x5008
#define DIVA_CUSTOM_WRITE_DATA_REGISTER     0x500C
#define DIVA_CUSTOM_READ_DATA_REGISTER      0x5010

#endif

