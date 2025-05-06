
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
#include "s4bri.h"
#include "os.h"
#include "diva_cfg.h"
#include <dlist.h>
#include <xdi_msg.h>

#include <s_4bri.c>

extern int card_ordinal;
extern int logical_adapter;
extern int logical_adapter_separate_config;
extern int card_number;

extern diva_entity_link_t* diva_cfg_adapter_handle;

static void diva_4bri_configure_pri_protocol (PISDN_ADAPTER IoAdapter);

int divas_4bri_create_image (int revision,
                            byte* sdram,
                            int* p_fd,
                            int* dsp_fd,
														int fpga_fd,
													  const char* ProtocolName,
														int protocol_id_0,
														int protocol_id_1,
														int protocol_id_2,
														int protocol_id_3,
														dword* protocol_features,
                            int tasks,
														int reentrant_protocol) {
	PISDN_ADAPTER IoAdapter, Slave;
	PISDN_ADAPTER adapter_array[4];
	ADAPTER_LIST_ENTRY	quadro_list;
	dword offset;
	int i, protocol_ids[4];
  int factor = (tasks == 1) ? 1 : 2;
	dword          code, FileLength;
	byte           *File;
	dword card_bar = cfg_get_card_bar (2);
  int vidi_init_ok = 0;
	dword vidi_mode;

	vidi_mode = diva_cfg_get_vidi_mode ();


	DBG_LOG(("vidi mode: %u", vidi_mode))

	if (diva_cfg_get_set_vidi_state (vidi_mode) != 0) {
		vidi_mode = 0;
		DBG_ERR(("failed to update vidi mode state"))
	}

	DBG_LOG(("vidi state: '%s'", vidi_mode != 0 ? "on" : "off"))

	protocol_ids[0] = protocol_id_0;
	protocol_ids[1] = protocol_id_1;
	protocol_ids[2] = protocol_id_2;
	protocol_ids[3] = protocol_id_3;

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
		Slave->reentrant = (byte)reentrant_protocol;
		if (revision) {
			if (reentrant_protocol != 0) {
				offset					= Slave->ControllerNumber * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE);
			} else {
				offset					= Slave->ControllerNumber * (MQ2_MEMORY_SIZE >> 2);
			}
			Slave->dma_map  = (void*)(unsigned long)(card_number+i);
		} else {
			if (reentrant_protocol != 0) {
				offset					= Slave->ControllerNumber * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE);
			} else {
				offset					= Slave->ControllerNumber * (MQ_MEMORY_SIZE >> 2);
			}
		}
		Slave->Address	= &sdram[offset];
		Slave->ram			= &sdram[offset];
		Slave->sdram_bar = card_bar;

		Slave->Properties = CardProperties[card_ordinal];
		Slave->protocol_id = protocol_ids[i];
		Slave->cardType = card_ordinal;
		Slave->a.io = Slave;
		Slave->ANum = card_number + i;

		quadro_list.QuadroAdapter[i] = Slave;
	}
	IoAdapter = adapter_array[0];
 	IoAdapter->QuadroList = &quadro_list;
	if (revision) {
		prepare_qBri2_functions (IoAdapter);
	} else {
		prepare_qBri_functions (IoAdapter);
	}

	/*
		Set up file handles
		*/
	for (i = 0; i < tasks; i++) {
		char tmp[4];
		sprintf (tmp, "p%d", i);
		diva_set_named_value (tmp,	(dword)p_fd[i]);
		strcpy (adapter_array[i]->protocol_name, tmp);
	}
  if (card_ordinal == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI) {
    /* no DSPs and no SoftDSP */
  } else if (card_ordinal == CARDTYPE_DIVASRV_B_2F_PCI) {
		diva_set_named_value (DIVA_BRI2F_SDP_1_NAME,	(dword)dsp_fd[0]);
		diva_set_named_value (DIVA_BRI2F_SDP_2_NAME,	(dword)dsp_fd[1]);
	} else {
		diva_set_named_value (DSP_TELINDUS_FILE,	(dword)dsp_fd[0]);
	}
	diva_set_named_value ("fpga_code",	(dword)fpga_fd);

	File = qBri_check_FPGAsrc (IoAdapter, "fpga_code", &FileLength, &code);
	if (cfg_get_Diva4BRIDisableFPGA ()) {
		IoAdapter->fpga_features = 0;
	}

	if (File) {
		xdiFreeFile (File) ;
	}
	for (i = 0; i < tasks; i++) {
		adapter_array[i]->fpga_features = IoAdapter->fpga_features;
	}

	if (reentrant_protocol != 0) {
		qBri_reentrant_protocol_load (IoAdapter);
	} else {
 		int i, controller = tasks ;

		do {
			controller-- ;
			i = 0 ;
			while (IoAdapter->QuadroList->QuadroAdapter[i]->ControllerNumber !=
							controller ) i++ ;
			/*
 				calculate base address for instance
				*/
			Slave          = IoAdapter->QuadroList->QuadroAdapter[i] ;
			offset         = Slave->ControllerNumber*(IoAdapter->MemorySize>>factor);
			Slave->Address = &IoAdapter->Address[offset] ;
			Slave->ram     = &IoAdapter->ram[offset] ;

			if ( !qBri_protocol_load (IoAdapter, Slave) )
				return (0) ;
			} while (controller != 0) ;
	}

  if (card_ordinal == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI) {

  } else if (card_ordinal == CARDTYPE_DIVASRV_B_2F_PCI) {
		byte* link_addr = 0;
		if (dsp_fd[0] >= 0) {
			link_addr = qBri_sdp_load (IoAdapter, DIVA_BRI2F_SDP_1_NAME, link_addr);
		}
		if (dsp_fd[1] >= 0) {
			link_addr = qBri_sdp_load (IoAdapter, DIVA_BRI2F_SDP_2_NAME, link_addr);
		}
		if (!link_addr) {
			qBri_sdp_load (IoAdapter, 0, 0);
		}
    
  } else {
		if (reentrant_protocol != 0) {
	  	if (!qBri_reentrant_telindus_load (IoAdapter)) {
				for (i = 0; i < tasks; i++) { free (adapter_array[i]); }
		  	return (0) ;
			}

			for (i = 1; i < tasks; i++) {
				Slave                 = IoAdapter->QuadroList->QuadroAdapter[i];
				Slave->features       = IoAdapter->features;
				Slave->InitialDspInfo = IoAdapter->InitialDspInfo;
			}
		} else {
	  	if (!qBri_telindus_load (IoAdapter)) {
				for (i = 0; i < tasks; i++) { free (adapter_array[i]); }
		  	return (0);
			}
		}
	}

	for (i = 0; i < tasks; i++) {
		diva_4bri_configure_pri_protocol (adapter_array[i]);
		if (logical_adapter_separate_config) {
			logical_adapter++;
		}
	}

	for (i = 0; i < tasks; i++) {
  	Slave = IoAdapter->QuadroList->QuadroAdapter[i] ;
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
		if (IoAdapter->reentrant != 0) {
			Slave->ram += MP_SHARED_RAM_OFFSET;
		} else {
			Slave->ram += ((IoAdapter->MemorySize >> factor) - MQ_SHARED_RAM_SIZE);
		}
  	memset (Slave->ram, '\0', 256) ;
  	diva_configure_protocol (Slave, MP_SHARED_RAM_SIZE);
	}

	*protocol_features = IoAdapter->features;

	for (i = 0; i < tasks; i++) { free (adapter_array[i]); }

	return (0);
}

static void diva_4bri_configure_pri_protocol (PISDN_ADAPTER IoAdapter) {

	IoAdapter->serialNo               = diva_cfg_get_serial_number ();
	diva_cfg_get_hw_info_struct ((char*)IoAdapter->hw_info, sizeof(IoAdapter->hw_info));

	if (diva_cfg_adapter_handle) {
		if (!(IoAdapter->cfg_lib_memory_init = diva_cfg_get_ram_init (&IoAdapter->cfg_lib_memory_init_length))) {
			DBG_ERR(("CfgLib ram init section missing"))
		}
		return;
	}

	IoAdapter->tei							= cfg_get_tei();
	IoAdapter->nt2							= cfg_get_nt2();
	IoAdapter->DidLen 					= cfg_get_didd_len();
	IoAdapter->WatchDog					= cfg_get_watchdog();
	IoAdapter->Permanent				= cfg_get_permanent();
	IoAdapter->StableL2 				=	cfg_get_stable_l2();
	IoAdapter->NoOrderCheck 		=	cfg_get_no_order_check();
	IoAdapter->LowChannel				= cfg_get_low_channel();
	IoAdapter->ProtVersion 			=	cfg_get_prot_version();
	IoAdapter->crc4							= cfg_get_crc4();
	IoAdapter->NoHscx30         = cfg_get_interface_loopback ();
	IoAdapter->nosig						= cfg_get_nosig();

	cfg_get_spid_oad  (1+IoAdapter->ControllerNumber*2, &IoAdapter->Oad1[0]);
	cfg_get_spid_osa  (1+IoAdapter->ControllerNumber*2, &IoAdapter->Osa1[0]);
	cfg_get_spid			(1+IoAdapter->ControllerNumber*2, &IoAdapter->Spid1[0]);
	cfg_get_spid_oad  (2+IoAdapter->ControllerNumber*2, &IoAdapter->Oad2[0]);
	cfg_get_spid_osa  (2+IoAdapter->ControllerNumber*2, &IoAdapter->Osa2[0]);
	cfg_get_spid			(2+IoAdapter->ControllerNumber*2, &IoAdapter->Spid2[0]);

	IoAdapter->ModemGuardTone					=	cfg_get_ModemGuardTone();
	IoAdapter->ModemMinSpeed					= cfg_get_ModemMinSpeed();
	IoAdapter->ModemMaxSpeed 					=	cfg_get_ModemMaxSpeed();
	IoAdapter->ModemOptions						=	cfg_get_ModemOptions();
	IoAdapter->ModemOptions2					= cfg_get_ModemOptions2();
	IoAdapter->ModemNegotiationMode		= cfg_get_ModemNegotiationMode();
	IoAdapter->ModemModulationsMask		= cfg_get_ModemModulationsMask();
	IoAdapter->ModemTransmitLevel			= cfg_get_ModemTransmitLevel();
	IoAdapter->ModemCarrierWaitTimeSec= cfg_get_ModemCarrierWaitTime ();
	IoAdapter->ModemCarrierLossWaitTimeTenthSec =
                                      cfg_get_ModemCarrierLossWaitTime ();
	IoAdapter->FaxOptions 						=	cfg_get_FaxOptions();
	IoAdapter->FaxMaxSpeed						=	cfg_get_FaxMaxSpeed();
	IoAdapter->Part68LevelLimiter     =	cfg_get_Part68LevelLimiter();

	IoAdapter->L1TristateOrQsig				= cfg_get_L1TristateQSIG();

	IoAdapter->ForceLaw								= cfg_get_forced_law();
	IoAdapter->GenerateRingtone       = cfg_get_RingerTone();

	IoAdapter->UsEktsNumCallApp       = cfg_get_UsEktsCachHandles();
	IoAdapter->UsEktsFeatAddConf      = cfg_get_UsEktsBeginConf();
	IoAdapter->UsEktsFeatRemoveConf   = cfg_get_UsEktsDropConf();
	IoAdapter->UsEktsFeatCallTransfer = cfg_get_UsEktsCallTransfer();
	IoAdapter->UsEktsFeatMsgWaiting   = cfg_get_UsEktsMWI();
	IoAdapter->ForceVoiceMailAlert    = cfg_get_UsForceVoiceAlert ();
	IoAdapter->DisableAutoSpid        = cfg_get_UsDisableAutoSPID ();

	IoAdapter->QsigDialect            = cfg_get_QsigDialect();

	IoAdapter->PiafsLinkTurnaroundInFrames = cfg_get_PiafsRtfOff ();

	IoAdapter->QsigFeatures           = cfg_get_QsigFeatures ();

	IoAdapter->BriLayer2LinkCount     = cfg_get_BriLinkCount ();

	IoAdapter->SupplementaryServicesFeatures = cfg_get_SuppSrvFeatures ();
	IoAdapter->FaxV34Options          = cfg_get_V34FaxOptions ();
	IoAdapter->DisabledDspMask        = cfg_get_DisabledDspsMask ();
	IoAdapter->AlertToIn20mSecTicks   = cfg_get_AlertTimeout ();
	IoAdapter->ModemEyeSetup          = cfg_get_ModemEyeSetup ();
	IoAdapter->CCBSRelTimer           = cfg_get_CCBSReleaseTimer ();
	IoAdapter->DiscAfterProgress      = cfg_get_DiscAfterProgress ();
}

void diva_os_set_qBri_functions (PISDN_ADAPTER IoAdapter) {
}

void diva_os_set_qBri2_functions (PISDN_ADAPTER IoAdapter) {
}
