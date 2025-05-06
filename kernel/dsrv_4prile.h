
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

#ifndef __DIVA_XDI_DSRV_4PRI_L_E_INC__
#define __DIVA_XDI_DSRV_4PRI_L_E_INC__

#define DIVA_L_MP_SHUFFLE(__x__) (__x__) /* (((__x__) & ~0x3U) | (((__x__) & 0x1U) << 1) | (((__x__) & 0x2U) >> 1)) */

#define DIVA_4PRILE_FPGA_ROM_REGISTER_OFFSET         0x20300U
#define DIVA_4PRILE_FPGA_DOORBELL_REGISTER_OFFSET    0x20400U

#define DIVA_4PRIE_TDM_CONTROL_BASE_OFFSET 0x20000U /* 0x27000 */

#define DIVA_4PRIE_TDM_CONTROL_REGISTER_OFFSET(__PhysicalDevice__) (DIVA_4PRIE_TDM_CONTROL_BASE_OFFSET+0x1000U*(__PhysicalDevice__))

#define DIVA_4PRILE_FALC_ISR_MASK 0x01U
#define DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(__PhysicalDevice__)  (0x20410U + ((__PhysicalDevice__) * 0x10U)) /* Bit 0 - Falc */
#define DIVA_4PRILE_DMA_MASK_REGISTER_OFFSET  0x20490U
#define DIVA_4PRILE_TASK_IMASK_REGISTER_OFFSET(__PhysicalDevice__) (0x20510U + ((__PhysicalDevice__) * 0x10U)) /* Bit 0 - Falc */

#define DIVA_L_MP_FLASH_ADDRESS_REGISTER_OFFSET 0x20800
#define DIVA_L_MP_FLASH_DATA_REGISTER_OFFSET    0x20810
#define DIVA_L_MP_FLASH_CONTROL_REGISTER_OFFSET 0x20820

int diva_4prile_connect_interrupt (PISDN_ADAPTER IoAdapter, int (*isr_proc)(void*), void* isr_proc_context);
void diva_os_prepare_4prile_functions (PISDN_ADAPTER IoAdapter);
void diva_4prile_read_serial_number (PISDN_ADAPTER IoAdapter);

#endif

