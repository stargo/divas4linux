
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

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "analog.h"
#include "os.h"
#include "diva_cfg.h"
#include <di_defs.h>
#include <cardtype.h>
#include <dlist.h>

#include <xdi_msg.h>

#include <s_analog.c>
PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
extern int card_ordinal;
extern int card_number;

extern diva_entity_link_t* diva_cfg_adapter_handle;

extern byte * qBri_check_FPGAsrc (PISDN_ADAPTER IoAdapter, char *FileName, dword *Length, dword *code);

void diva_configure_analog_protocol (PISDN_ADAPTER IoAdapter);

typedef struct _diva_named_value {
	char	ident[24];
	dword value;
} diva_named_value_t;

/* ------------------------------------------------------------------------
		Does create image in the allocated memory
		return:
						>	 0: operation failed
						<= 0: success, ofset to start download
	 ------------------------------------------------------------------------ */
int
divas_analog_create_image (int instances,
													 byte* sdram,
													 int protocol_fd,
													 int dsp_fd,
													 int fpga_fd,
													 const char* ProtocolName,
													 dword* protocol_features)
{
	PISDN_ADAPTER IoAdapter = malloc (sizeof(*IoAdapter));
	dword card_bar = cfg_get_card_bar (2), code, FileLength;
	byte *File;
	int vidi_init_ok = 0;
	dword vidi_mode;

	if (!IoAdapter) {
		return (-1);
	}

	vidi_mode = diva_cfg_get_vidi_mode ();

	DBG_LOG(("vidi mode: %u", vidi_mode))

	if (diva_cfg_get_set_vidi_state (vidi_mode) != 0) {
		vidi_mode = 0;
		DBG_ERR(("failed to update vidi mode state"))
	}

	DBG_LOG(("vidi state: '%s'", vidi_mode != 0 ? "on" : "off"))

	memset (IoAdapter, 0x00, sizeof(*IoAdapter));

	diva_set_named_value ("p0", (dword)protocol_fd);
	strcpy (IoAdapter->protocol_name, "p0");
	diva_set_named_value (DSP_TELINDUS_FILE,	(dword)dsp_fd);
	diva_set_named_value ("fpga_code",	(dword)fpga_fd);

	IoAdapter->Properties = CardProperties[card_ordinal];
	IoAdapter->host_vidi.vidi_active = vidi_mode != 0;
	IoAdapter->Channels   = IoAdapter->Properties.Channels;
	IoAdapter->protocol_id = 34;
	IoAdapter->cardType = (word)card_ordinal;
	IoAdapter->ram = sdram;
	IoAdapter->sdram_bar = card_bar;
	IoAdapter->a.io = IoAdapter;
	prepare_analog_functions (IoAdapter);
	IoAdapter->dma_map = (void*)(unsigned long)card_number;
	IoAdapter->ANum = card_number;

	File = qBri_check_FPGAsrc (IoAdapter, "fpga_code", &FileLength, &code);
	if (cfg_get_Diva4BRIDisableFPGA ()) {
		IoAdapter->fpga_features = 0;
	} else {
		IoAdapter->fpga_features |= PCINIT_FPGA_PLX_ACCESS_SUPPORTED ;
	}
	if (File) {
		xdiFreeFile (File) ;
	}

	if (vidi_mode != 0) {
		diva_xdi_um_cfg_cmd_data_init_vidi_t host_vidi;

		memset (&host_vidi, 0x00, sizeof(host_vidi));
		if (diva_cfg_get_vidi_info_struct (0, &host_vidi) != 0) {
			DBG_ERR(("A(%d) failed to initialize vidi", card_number))
			vidi_init_ok = -1;
		}
		diva_set_vidi_values (IoAdapter, &host_vidi);
	}

	diva_configure_analog_protocol (IoAdapter);

	if (!analog_protocol_load (IoAdapter)) {
		return (-2);
	}

	if (!analog_telindus_load (IoAdapter)) {
		return (-3) ;
	}

	IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
	memset (IoAdapter->ram, 0x00, 256) ;
	diva_configure_protocol (IoAdapter, 0xffffffff);

	*protocol_features = IoAdapter->features;

	free (IoAdapter);

	return (0);
}

void
diva_configure_analog_protocol (PISDN_ADAPTER IoAdapter)
{
	IoAdapter->serialNo               = diva_cfg_get_serial_number ();

	diva_cfg_get_hw_info_struct ((char*)IoAdapter->hw_info, sizeof(IoAdapter->hw_info));

	if (diva_cfg_adapter_handle) {
		if (!(IoAdapter->cfg_lib_memory_init = diva_cfg_get_ram_init (&IoAdapter->cfg_lib_memory_init_length))) {
			DBG_ERR(("CfgLib ram init section missing"))
		}
		return;
	}

	IoAdapter->ProtVersion 			= 0x80 | 34;

	IoAdapter->ModemGuardTone         =	cfg_get_ModemGuardTone();
	IoAdapter->ModemMinSpeed          = cfg_get_ModemMinSpeed();
	IoAdapter->ModemMaxSpeed          =	cfg_get_ModemMaxSpeed();
	IoAdapter->ModemOptions           =	cfg_get_ModemOptions();
	IoAdapter->ModemOptions2          = cfg_get_ModemOptions2();
	IoAdapter->ModemNegotiationMode   = cfg_get_ModemNegotiationMode();
	IoAdapter->ModemModulationsMask   = cfg_get_ModemModulationsMask();
	IoAdapter->ModemTransmitLevel     = cfg_get_ModemTransmitLevel();
	IoAdapter->ModemCarrierWaitTimeSec= cfg_get_ModemCarrierWaitTime ();
	IoAdapter->ModemCarrierLossWaitTimeTenthSec =
                                      cfg_get_ModemCarrierLossWaitTime ();
	IoAdapter->FaxOptions             =	cfg_get_FaxOptions();
	IoAdapter->FaxMaxSpeed            =	cfg_get_FaxMaxSpeed();
	IoAdapter->Part68LevelLimiter     =	cfg_get_Part68LevelLimiter();

	IoAdapter->ForceLaw               = cfg_get_forced_law();


	IoAdapter->FaxV34Options          = cfg_get_V34FaxOptions ();
	IoAdapter->DisabledDspMask        = cfg_get_DisabledDspsMask ();
	IoAdapter->ModemEyeSetup          = cfg_get_ModemEyeSetup ();
}

void diva_os_prepare_analog_functions (PISDN_ADAPTER IoAdapter) {
}

