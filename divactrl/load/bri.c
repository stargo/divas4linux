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
#include "bri.h"
#include "os.h"
#include "diva_cfg.h"
#include <dlist.h>

/*
	Emulation of I/O based BRI card hardware
	*/
static byte* bri_sdram;
static word bri_hi;
static word bri_lo;
static void* bri_addr_hi;
static void* bri_addr_lo;
static void* bri_addr_data;
static int bri_debug_io;
static dword bri_max_io_addr;

extern diva_entity_link_t* diva_cfg_adapter_handle;

#include <s_bri.c>
extern int card_ordinal;
extern int card_number;

static void diva_configure_bri_protocol (PISDN_ADAPTER IoAdapter);

/* ------------------------------------------------------------------------
		Does create image in the allocated memory
		return:
						>	 0: operation failed
						<= 0: success, ofset to start download
	 ------------------------------------------------------------------------ */
int divas_bri_create_image (byte* sdram,
														int prot_fd,
														int dsp_fd,
														const char* protocol_name,
														int protocol_id,
														dword* protocol_features,
														dword* max_download_address) {
	PISDN_ADAPTER IoAdapter = malloc (sizeof(*IoAdapter));

	bri_sdram       = 0;
	bri_hi          = 0;
	bri_lo          = 0;
	bri_addr_hi     = 0;
	bri_addr_lo     = 0;
	bri_addr_data   = 0;
	bri_debug_io    = 0;
	bri_max_io_addr = 0;

	if (!IoAdapter) {
		return (-1);
	}
	memset (IoAdapter, 0x00, sizeof(*IoAdapter));

	diva_set_named_value ("p0", (dword)prot_fd);
	strcpy (IoAdapter->protocol_name, "p0");
	diva_set_named_value (DSP_TELINDUS_FILE,	(dword)dsp_fd);
	diva_set_named_value (DSP_QSIG_TELINDUS_FILE, (dword)dsp_fd);

	IoAdapter->Properties = CardProperties[card_ordinal];
	IoAdapter->protocol_id = protocol_id;
	IoAdapter->cardType = (word)card_ordinal;
	IoAdapter->ram = sdram;
	IoAdapter->a.io = IoAdapter;
	IoAdapter->ANum = card_number;

	IoAdapter->port	= (byte*)0x100;
	bri_sdram				=	sdram;

	bri_addr_hi   = IoAdapter->port + \
        ((IoAdapter->Properties.Bus == BUS_PCI) ? M_PCI_ADDRH : ADDRH);
	bri_addr_lo   = IoAdapter->port + ADDR ;
	bri_addr_data = IoAdapter->port + DATA ;

	prepare_maestra_functions (IoAdapter);

	diva_configure_bri_protocol (IoAdapter);

	if (!bri_protocol_load (IoAdapter)) {
		return (-2);
	}

	if (!bri_telindus_load (IoAdapter, DSP_TELINDUS_FILE)) {
		return (-3) ;
	}

	bri_max_io_addr += 8;
	if (bri_max_io_addr & 0x00000001) {
		bri_max_io_addr++;
	}
	DBG_LOG(("Max I/O Address=%08x", bri_max_io_addr))
	*max_download_address = bri_max_io_addr;

	outpp  (bri_addr_hi, SHAREDM_SEG);
	outppw (bri_addr_lo, 0x1e);
	outpp (bri_addr_data, 0);
	outpp (bri_addr_data, 0);

	diva_configure_protocol (IoAdapter, 0xffffffff);

	*protocol_features = IoAdapter->features;

	free (IoAdapter);

	return (0);
}

static void diva_configure_bri_protocol (PISDN_ADAPTER IoAdapter) {

	IoAdapter->serialNo               = diva_cfg_get_serial_number ();

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

	cfg_get_spid_oad  (1, &IoAdapter->Oad1[0]);
	cfg_get_spid_osa  (1, &IoAdapter->Osa1[0]);
	cfg_get_spid			(1, &IoAdapter->Spid1[0]);
	cfg_get_spid_oad  (2, &IoAdapter->Oad2[0]);
	cfg_get_spid_osa  (2, &IoAdapter->Osa2[0]);
	cfg_get_spid			(2, &IoAdapter->Spid2[0]);

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

byte inpp (void* addr) {
	if				(bri_addr_hi == addr) {
		return ((byte)bri_hi);
	} else if (bri_addr_lo == addr) {
		return ((byte)bri_lo);
	} else if (bri_addr_data == addr) {
		dword saddr = (dword)((((dword)bri_hi) << 16) | bri_lo);
		byte* paddr = &bri_sdram[saddr];

		return (*paddr);
	} else {
		DBG_ERR(("A: inpp(%08x)", addr))

		return (0xff);
	}
}

word inppw (void* addr) {
	if				(bri_addr_hi == addr) {
		return (bri_hi);
	} else if (bri_addr_lo == addr) {
		return (bri_lo);
	} else if (bri_addr_data == addr) {
		dword saddr = (dword)((((dword)bri_hi) << 16) | bri_lo);
		word* waddr = (word*)&bri_sdram[saddr];

		saddr += 2;
		bri_lo = (word)saddr;
		bri_hi = (word)(saddr >> 16);

		return (*waddr);
	} else {
		DBG_ERR(("A: inpp(%08x)", addr))

		return (0xff);
	}
}

void outpp (void* addr, word w) {
	if				(bri_addr_hi == addr) {
		bri_hi = (byte)(w);
		if (bri_debug_io) {
			DBG_LOG(("HI=%04x", bri_hi))
	  }
	} else if (bri_addr_lo == addr) {
		bri_lo = (byte)(w);
		if (bri_debug_io) {
			DBG_LOG(("LO=%04x", bri_lo))
	  }
	} else if (bri_addr_data == addr) {
		dword saddr = (dword)((((dword)bri_hi) << 16) | bri_lo);
		byte* paddr = &bri_sdram[saddr];

		if (bri_debug_io) {
			DBG_LOG(("SDRAM[%08x]=%02x", saddr, (byte)w))
		}

		if (saddr >= bri_max_io_addr) {
			bri_max_io_addr = saddr+2;
		}

		if (bri_hi	== SHAREDM_SEG) {
			saddr = BRI_MEMORY_SIZE + bri_lo;
			paddr = (byte*)&bri_sdram[saddr];
		}

		/*
			Byte access uses word increment too
			*/
		saddr += 2;
		bri_lo = (word)saddr;
		bri_hi = (word)(saddr >> 16);

		*paddr = (byte)w;
	} else {
		DBG_ERR(("A: outpp(%08x)", addr))
	}
}

void outppw (void* addr, word p) {
	if				(bri_addr_hi == addr) {
		bri_hi = p;
		if (bri_debug_io) {
			DBG_LOG(("HI=%04x", bri_hi))
	  }
	} else if (bri_addr_lo == addr) {
		bri_lo = p;
		if (bri_debug_io) {
			DBG_LOG(("LO=%04x", bri_lo))
	  }
	} else if (bri_addr_data == addr) {
		dword saddr = (dword)((((dword)bri_hi) << 16) | bri_lo);
		word* waddr = (word*)&bri_sdram[saddr];

		if (bri_debug_io) {
			DBG_LOG(("SDRAM[%08x]=%04x", saddr, p))
		}

		if (saddr >= bri_max_io_addr) {
			bri_max_io_addr = saddr+2;
		}

		if (bri_hi	== SHAREDM_SEG) {
			saddr = BRI_MEMORY_SIZE + bri_lo;
			waddr = (word*)&bri_sdram[saddr];
		}

		saddr += 2;
		bri_lo = (word)saddr;
		bri_hi = (word)(saddr >> 16);

		*waddr = p;
	} else {
		DBG_ERR(("A: outppw(%08x)", addr))
	}
}

void outppw_buffer (void* addr, void* p, int length) {
	word* pw = (word*)p;
	length = (length%2) ? ((length+1)>>1) : (length>>1);

	while (length--) {
		outppw (addr,  *pw++);
	}
}

void diva_os_prepare_maestra_functions (PISDN_ADAPTER IoAdapter) {
}
