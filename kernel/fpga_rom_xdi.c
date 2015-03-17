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
#include "platform.h"
#include "debuglib.h"
#include "fpga_rom.h"

/*
	Necessary to provide va versions of debug functions the the future
	*/
static word diva_fpga_rom_dprintf (char* format, ...) {
	if ((MYDRIVERDEBUGHANDLE.dbgMask) & (unsigned long)DL_LOG) {
		pMyDriverDebugHandle = &MYDRIVERDEBUGHANDLE;
		if (pMyDriverDebugHandle->dbg_prt) {
			va_list ap;

			va_start (ap, format);
			(pMyDriverDebugHandle->dbg_prt)(pMyDriverDebugHandle->id, DLI_LOG, format, ap);
			va_end (ap);
		}
	}

	return (0);
}

static word diva_fpga_rom_no_dprintf (char *format, ...) {
	return (0);
}

static dword diva_xdi_read_fpga_register (void* context, dword address) {
	return ((*(volatile dword*)(((byte*)context) + address)));
}

static dword diva_xdi_dummy_read (void* context) {
	return (0);
}

void diva_xdi_show_fpga_rom_features (const byte* fpga_mem_address) {
	dword tmp = 0;

	diva_show_fpga_rom_features ((void*)fpga_mem_address,
															 0,
															 &tmp,
															 0,
															 0,
															 diva_fpga_rom_dprintf,
															 diva_xdi_read_fpga_register,
															 diva_xdi_dummy_read);
}

dword diva_xdi_decode_fpga_rom_features (const byte* fpga_mem_address) {
	dword tmp = 0, ret = 0;

	diva_show_fpga_rom_features ((void*)fpga_mem_address,
															 0,
															 &tmp,
															 0,
															 &ret,
															 diva_fpga_rom_no_dprintf,
															 diva_xdi_read_fpga_register,
															 diva_xdi_dummy_read);

	return (ret);
}
