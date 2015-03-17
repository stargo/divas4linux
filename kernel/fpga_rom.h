
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
#ifndef __DIVA_FPGA_ROM_H__  
#define __DIVA_FPGA_ROM_H__
void diva_show_fpga_rom_features (void* context,
                                  dword address,
                                  volatile dword* dummy_read_address,
                                  dword* hardware_features_address,
                                  dword* fpga_features_address,
                                  word (*dprintf_proc)(char *, ...),
                                  dword (*read_fpga_register_proc)(void*, dword),
                                  dword (*dummy_read_proc)(void*));
#define DIVA_FPGA_HARDWARE_FEATURE_TEMPERATURE_SENSOR_SUPPORT (1U << 0)
#define DIVA_FPGA_HARDWARE_FEATURE_HW_TIMER                   (1U << 7)
#endif  
