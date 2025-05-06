
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
#include "s4pri.h"
#include "os.h"
#include "diva_cfg.h"
#include <dlist.h>
#include <xdi_msg.h>

#include <s_4pri.c>

/*
	LOCALS
	*/
static void diva_configure_qpri_protocol (PISDN_ADAPTER IoAdapter);

/*
	IMPORTS
	*/
extern int card_ordinal;
extern int logical_adapter;
extern int card_number;
extern diva_entity_link_t* diva_cfg_adapter_handle;

/*
	Create 4PRI SDRAM image
	*/
int divas_4pri_create_image (byte* sdram, /* SDRAM image */
                             int prot_fd,
                             int dsp_fd,
														 dword* protocol_features,
                             int tasks,
                             int pcie_fs) {
	PISDN_ADAPTER IoAdapter, Slave;
	PISDN_ADAPTER adapter_array[8];
	ADAPTER_LIST_ENTRY	quadro_list;
	dword card_bar = pcie_fs == 0 ? cfg_get_card_bar (2) :  cfg_get_card_bar (0);
	dword offset;
	int i;
  int vidi_init_ok = 0;
	dword vidi_mode;

	memset (adapter_array, 0x00, sizeof(adapter_array));

	vidi_mode = diva_cfg_get_vidi_mode ();


	if (diva_cfg_get_set_vidi_state (vidi_mode) != 0) {
		vidi_mode = 0;
	}

	DBG_LOG(("vidi state: '%s'", vidi_mode != 0 ? "on" : "off"))

	for (i = 0; i < tasks; i++) {
		if (!(adapter_array[i] = malloc (sizeof(*IoAdapter)))) {
			for (i = 0; i < tasks; i++) {
				if (adapter_array[i]) { free (adapter_array[i]); }
			}
			return (-1);
		} else {
			memset (adapter_array[i], 0x00, sizeof(*IoAdapter));
			adapter_array[i]->tasks = tasks;
		}
	}

	for (i = 0; i < tasks; i++) {
		Slave = adapter_array[i];
		Slave->ControllerNumber = i;
		Slave->host_vidi.vidi_active = vidi_mode != 0;
		Slave->dma_map = (void*)(unsigned long)(card_number+i);
		offset         = Slave->ControllerNumber * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE);
		Slave->ANum    = card_number + i;

		Slave->Address   = &sdram[offset];
		Slave->ram       = &sdram[offset];
		Slave->sdram_bar = card_bar;

		Slave->Properties  = CardProperties[card_ordinal];
		Slave->protocol_id = 0xffffffff;
   	Slave->cardType    = card_ordinal;
		Slave->a.io        = Slave;

		quadro_list.QuadroAdapter[i] = Slave;
	}
	IoAdapter             = adapter_array[0];
	IoAdapter->QuadroList = &quadro_list;

	prepare_4pri_functions (IoAdapter);

	diva_set_named_value ("p0", (dword)prot_fd);
	strcpy (IoAdapter->protocol_name, "p0");
	diva_set_named_value ("bfdload.bin", (dword)dsp_fd);
	strcpy (&IoAdapter->AddDownload[0], "bfdload.bin");

	for (i = 0; i < tasks; i++) {
		Slave = IoAdapter->QuadroList->QuadroAdapter[i];
		Slave->xconnectExportMode = MIN(DivaXconnectExportModeMax,diva_nr_li_descriptors);
	}

	for (i = 0; i < tasks; i++) {
		Slave = IoAdapter->QuadroList->QuadroAdapter[i];
		diva_configure_qpri_protocol (Slave);
		logical_adapter++;
	}

	if (!qpri_protocol_load (IoAdapter)) {
		for (i = 0; i < tasks; i++) {
			if (adapter_array[i]) { free (adapter_array[i]); }
		}
		return (-2);
	}
	if (!qpri_dsp_load (IoAdapter)) {
		for (i = 0; i < tasks; i++) {
			if (adapter_array[i]) { free (adapter_array[i]); }
		}
		return (-3);
	}

	for (i = 1; i < tasks; i++) {
		Slave                 = IoAdapter->QuadroList->QuadroAdapter[i];
		Slave->features       = IoAdapter->features;
		Slave->InitialDspInfo = IoAdapter->InitialDspInfo;
	}

	for (i = 0; i < tasks; i++) {
		Slave       = IoAdapter->QuadroList->QuadroAdapter[i];

		if (vidi_mode != 0) {
			diva_xdi_um_cfg_cmd_data_init_vidi_t host_vidi;

			memset (&host_vidi, 0x00, sizeof(host_vidi));
			if (diva_cfg_get_vidi_info_struct (i, &host_vidi) != 0) {
				DBG_ERR(("A(%d) failed to initialize vidi", card_number+i))
				vidi_init_ok = -1;
				break;
			}
			diva_set_vidi_values (Slave, &host_vidi);
		}

		Slave->ram += MP_SHARED_RAM_OFFSET;
  	memset (Slave->ram, '\0', 256);
		diva_configure_protocol (Slave, MP_SHARED_RAM_SIZE);
	}

	*protocol_features = IoAdapter->features;

	for (i = 0; i < tasks; i++) {
		if (adapter_array[i]) { free (adapter_array[i]); }
	}

  if (vidi_init_ok < 0)
    return (-1);

	return (0);
}

static void diva_configure_qpri_protocol (PISDN_ADAPTER IoAdapter) {
	IoAdapter->serialNo               = diva_cfg_get_serial_number ();
	diva_cfg_get_hw_info_struct ((char*)IoAdapter->hw_info, sizeof(IoAdapter->hw_info));

	if (diva_cfg_adapter_handle) {
		if (!(IoAdapter->cfg_lib_memory_init = diva_cfg_get_ram_init (&IoAdapter->cfg_lib_memory_init_length))) {
			DBG_ERR(("CfgLib ram init section missing"))
		}
	} else {
		DBG_ERR(("Missing CFGLib handle"))
	}
}

void diva_os_prepare_4pri_functions (PISDN_ADAPTER IoAdapter) {
}

