/*------------------------------------------------------------------------------
 *
 * (c) COPYRIGHT 1999-2007       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be
 * copied, transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *------------------------------------------------------------------------------*/
/* --------------------------------------------------------------------------
		This file wraps arpoud classic kernel mode procedure to be able
		to call same code in user mode
	 -------------------------------------------------------------------------- */
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "pri3.h"
#include "os.h"
#include "diva_cfg.h"
#include <di_defs.h>
#include <cardtype.h>
#include <dlist.h>
#include <xdi_msg.h>

#include <s_pri3.c>
PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
extern int card_ordinal;
extern int card_number;

extern diva_entity_link_t* diva_cfg_adapter_handle;

void diva_configure_pri_v3_protocol (PISDN_ADAPTER IoAdapter);

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
divas_pri_v3_create_image (int rev_2,
                           byte* sdram,
                           int prot_fd,
                           int dsp_fd,
                           const char* protocol_name,
                           int protocol_id,
                           dword* protocol_features)
{
	PISDN_ADAPTER IoAdapter = malloc (sizeof(*IoAdapter));
	dword card_bar = cfg_get_card_bar (2);
  int vidi_init_ok = 0;
	dword vidi_mode;

	if (!IoAdapter) {
		return (-1);
	}
	memset (IoAdapter, 0x00, sizeof(*IoAdapter));

	vidi_mode = diva_cfg_get_vidi_mode ();

	if (diva_cfg_get_set_vidi_state (vidi_mode) != 0) {
		vidi_mode = 0;
	}

	DBG_LOG(("vidi state: '%s'", vidi_mode != 0 ? "on" : "off"))

	IoAdapter->host_vidi.vidi_active = vidi_mode != 0;

	diva_set_named_value ("p0", (dword)prot_fd);
	strcpy (IoAdapter->protocol_name, "p0");
	diva_set_named_value (DSP_TELINDUS_FILE,	(dword)dsp_fd);

	IoAdapter->Properties = CardProperties[card_ordinal];
	IoAdapter->Channels   = IoAdapter->Properties.Channels;
	IoAdapter->protocol_id = protocol_id;
	IoAdapter->cardType = (word)card_ordinal;
	IoAdapter->ram = sdram;
	IoAdapter->sdram_bar = card_bar;
	IoAdapter->a.io = IoAdapter;
	IoAdapter->dma_map = (void*)(unsigned long)card_number;
	IoAdapter->ANum = card_number;
	prepare_pri_v3_functions (IoAdapter);

	diva_configure_pri_v3_protocol (IoAdapter);

	if (!pri_v3_protocol_load (IoAdapter)) {
		return (-2);
	}

	if (!pri_telindus_load (IoAdapter)) {
		return (-3) ;
	}

	if (vidi_mode != 0) {
		diva_xdi_um_cfg_cmd_data_init_vidi_t host_vidi;

		memset (&host_vidi, 0x00, sizeof(host_vidi));
		if (diva_cfg_get_vidi_info_struct (0, &host_vidi) != 0) {
			DBG_ERR(("A(%d) failed to initialize vidi", card_number))
			vidi_init_ok = -1;
		} else {
			diva_set_vidi_values (IoAdapter, &host_vidi);
		}
	}

	IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
	memset (IoAdapter->ram, 0x00, 256) ;
	diva_configure_protocol (IoAdapter, 0xffffffff);

	*protocol_features = IoAdapter->features;

	free (IoAdapter);

	return (0);
}

void
diva_configure_pri_v3_protocol (PISDN_ADAPTER IoAdapter)
{
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
	IoAdapter->BChMask  				= cfg_get_b_ch_mask();
	IoAdapter->StableL2 				=	cfg_get_stable_l2();
	IoAdapter->NoOrderCheck 		=	cfg_get_no_order_check();
	IoAdapter->LowChannel				= cfg_get_low_channel();
	IoAdapter->NoHscx30         = (cfg_get_fractional_flag () |
			cfg_get_interface_loopback ());
	IoAdapter->ProtVersion 			=	cfg_get_prot_version();
	IoAdapter->crc4							= cfg_get_crc4();
	IoAdapter->nosig						= cfg_get_nosig();

	if (IoAdapter->protocol_id == PROTTYPE_RBSCAS) {
		cfg_adjust_rbs_options();
	}

	cfg_get_spid_oad  (1, &IoAdapter->Oad1[0]);
	cfg_get_spid_osa  (1, &IoAdapter->Osa1[0]);
	cfg_get_spid			(1, &IoAdapter->Spid1[0]);
	cfg_get_spid_oad  (2, &IoAdapter->Oad2[0]);
	cfg_get_spid_osa  (2, &IoAdapter->Osa2[0]);
	cfg_get_spid			(2, &IoAdapter->Spid2[0]);

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

	IoAdapter->L1TristateOrQsig       = cfg_get_L1TristateQSIG();

	IoAdapter->ForceLaw               = cfg_get_forced_law();
	IoAdapter->GenerateRingtone       = cfg_get_RingerTone();
	IoAdapter->QsigDialect            = cfg_get_QsigDialect();

	IoAdapter->PiafsLinkTurnaroundInFrames = cfg_get_PiafsRtfOff ();

	IoAdapter->QsigFeatures           = cfg_get_QsigFeatures ();


	IoAdapter->SupplementaryServicesFeatures = cfg_get_SuppSrvFeatures ();
	IoAdapter->R2Dialect              = cfg_get_R2Dialect ();
	IoAdapter->R2CtryLength           = cfg_get_R2CtryLength ();
	IoAdapter->R2CasOptions           = cfg_get_R2CasOptions ();
	IoAdapter->FaxV34Options          = cfg_get_V34FaxOptions ();
	IoAdapter->DisabledDspMask        = cfg_get_DisabledDspsMask ();
	IoAdapter->AlertToIn20mSecTicks   = cfg_get_AlertTimeout ();
	IoAdapter->ModemEyeSetup          = cfg_get_ModemEyeSetup ();
	IoAdapter->CCBSRelTimer           = cfg_get_CCBSReleaseTimer ();
	IoAdapter->AniDniLimiter[0]       = cfg_get_AniDniLimiter_1 ();
	IoAdapter->AniDniLimiter[1]       = cfg_get_AniDniLimiter_2 ();
	IoAdapter->AniDniLimiter[2]       = cfg_get_AniDniLimiter_3 ();
	IoAdapter->DiscAfterProgress      = cfg_get_DiscAfterProgress ();

	IoAdapter->TxAttenuation          = cfg_get_TxAttenuation();
}

void diva_os_prepare_pri_v3_functions (PISDN_ADAPTER IoAdapter) {

}

