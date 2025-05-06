
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
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

#ifndef __DIVA_UM_OS_API_H__
#define __DIVA_UM_OS_API_H__

#ifdef __cplusplus
extern "C" {
#endif

dword divas_open_driver (int card_number);
int divas_close_driver (dword handle);
int divas_get_card (int card_number);
int divas_get_revision_id (int card_number);
int divas_get_card_properties (int card_number,
                               const char* name,
                               char* buffer,
                               int   size);
int divas_start_bootloader (int card_number);
int divas_ram_write (int card_number,
                     int ram_number,
                     const void* buffer,
                     dword offset,
                     dword length);
int divas_start_adapter (int card_number, dword start_address, dword features);
int divas_stop_adapter (int card_number);
int divas_write_fpga (int card_number,
                      int fpga_number,
                      const void* buffer,
                      dword length);
int divas_ram_read (int card_number,
                    int ram_number,
                    void* buffer,
                    dword offset,
                    dword length);
int divas_set_protocol_features (int card_number, dword features);
int divas_memory_test (int card_number, dword test_cmd);
dword divas_read_bar_dword (dword handle, byte bar, dword offset, int* info);
int divas_write_bar_dword (dword handle, byte bar, dword offset, dword data);
int divas_set_protocol_code_version (int card_number, dword version);

#ifdef __cplusplus
}
#endif

#endif
