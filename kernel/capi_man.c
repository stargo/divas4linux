
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
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg.h"
#include "divasync.h"
#include "manage.h"
#include "man_defs.h"
#include "drv_man.h"

extern void diva_os_api_lock   (diva_os_spin_lock_magic_t* old_irql);
extern void diva_os_api_unlock (diva_os_spin_lock_magic_t* old_irql);
extern const DIVA_CAPI_ADAPTER* diva_os_get_adapter(int nr);
extern byte appl_mask_empty (dword *appl_mask_table);
extern void dump_plcis (DIVA_CAPI_ADAPTER *a);
extern APPL *application;
extern byte max_appl;
extern dword diva_capi_ncci_mapping_bug;
extern byte UnMapController (byte);


#ifndef DIVA_BUILD
#include "buildver.h"
#define DIVA_BUILD BUILD
#endif

#define NVer DIVA_BUILD
#define MAN_INTERFACE_VERSION 0x0115
static dword MIFVer = MAN_INTERFACE_VERSION;
#define DIVA_MTPX_MAX_ENUM_DIRECTORIES     999
static int mtpx_man_max_enums = DIVA_MTPX_MAX_ENUM_DIRECTORIES;
static int capi_man_max_plcis = 255;
static int capi_man_max_appls = MAX_APPL;
static int capi_man_max_plci_trace_entries = TRACE_QUEUE_ENTRIES;

/*
	Dump statis for all plci(s)
	*/
static void diva_capi_dump_status (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	const DIVA_CAPI_ADAPTER* a;
	int nr = 0;

	DBG_LOG((" -------------- CAPI STATUS DUMP --------------- "))

	diva_os_api_lock (&irql);

	while ((a = diva_os_get_adapter(nr++))) {
		DBG_LOG((" -------------- CAPI ADAPTER(%x) --------------- ", a->Id))
		dump_plcis ((DIVA_CAPI_ADAPTER*)a);
		DBG_LOG((" ----------------------------------------------- "))
	}

	diva_os_api_unlock (&irql);

	DBG_LOG((" ----------------------------------------------- "))
}

static void *xdi_info_v (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara) ;
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance))) {
		if (cmd == CMD_READVAR) {
			ret = diva_man_read_value_by_offset (pM, (byte*)a, PtrToUlong(context->fPara), context->node);
		} else if (cmd == CMD_WRITEVAR) {
			diva_man_write_value_by_offset (pM, info, (byte*)a, PtrToUlong(context->fPara), context->node);
		}
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static void *xdi_info_capi_controller (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara) ;
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance)) != 0) {
		pM->b_out = UnMapController(a->Id);
		ret = &pM->b_out;
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static void *xdi_info (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara) ;
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance))) {
		if (cmd == CMD_READVAR) {
			switch (*(char*)context->fPara) {
				case 'A': /* Law */
					ret = a->u_law ? "u-Law" : "a-Law";
					break;
			}
		}
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static void* mtpx_xdi_info_v (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int instance  = PtrToInt(context[2].fPara);
	int cfg_entry = PtrToInt(context[1].fPara);
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance)) != 0 &&
			a->mtpx_cfg.cfg != 0 &&
			a->mtpx_cfg.number_of_configuration_entries > (dword)cfg_entry &&
			cmd == CMD_READVAR) {

		ret = diva_man_read_value_by_offset (pM,
																				 (byte*)&a->mtpx_cfg.cfg[cfg_entry],
																				 PtrToUlong(context->fPara),
																				 context->node);
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static void* mtpx_xdi_info_type_name (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int instance  = PtrToInt(context[2].fPara);
	int cfg_entry = PtrToInt(context[1].fPara);
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance)) != 0 &&
			a->mtpx_cfg.cfg != 0 &&
			a->mtpx_cfg.number_of_configuration_entries > (word)cfg_entry &&
			cmd == CMD_READVAR) {
		const char* name[] ={ "PRI", "BRI", "Analog", "SoftIP", "Unknown" };

		ret = (char*)name[MIN(a->mtpx_cfg.cfg[cfg_entry].card_type, sizeof(name)/sizeof(name[0])-1)];
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static MAN_INFO mtpx_xdi_cfg_dir[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Id",          MI_HINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, card_id),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, card_id),
                  0,            0
  },
  {"Type",        MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, card_type),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, card_type),
                  0,            0
  },
  {"TypeName",    MI_ASCIIZ,    0, MI_CALL,  0,
                  mtpx_xdi_info_type_name,   0,
                  0,            0
  },
  {"CardType",    MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, cardtype),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, cardtype),
                  0,            0
  },
  {"Channels",    MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, channels),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, channels),
                  0,            0
  },
  {"Nr",          MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, logical_adapter_number),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, logical_adapter_number),
                  0,            0
  },
  {"Serial",      MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, serial_number),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, serial_number),
                  0,            0
  },
  {"Interface",   MI_UINT,      0, MI_CALL,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, ifc),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, ifc),
                  0,            0
  },
  {"Interfaces",  MI_UINT,      0, MI_CALL|MI_E,
                  DIVAS_SIZE_OF(diva_mtpx_adapter_config_entry_t, interfaces),
                  mtpx_xdi_info_v,
                  (void*)DIVAS_OFFSET_OF(diva_mtpx_adapter_config_entry_t, interfaces),
                  0,            0
  }
};

static void *mtpx_get_xdi_cfg_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara) ;
	dword cfg_entry = PtrToInt(context[0].fPara) ;
	const DIVA_CAPI_ADAPTER* a;
	void* ret = 0;

	diva_os_api_lock (&irql);

	if ((a = diva_os_get_adapter(instance)) != 0 &&
			a->mtpx_cfg.cfg != 0 &&
			a->mtpx_cfg.number_of_configuration_entries > cfg_entry) {
		if (cmd == CMD_NEXTNODE) {
			ret = mtpx_xdi_cfg_dir;
		} else if (cmd == CMD_READVAR) {
			ret = mtpx_xdi_cfg_dir;
		}
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

/*
	Information about XDI adapter
	*/
static MAN_INFO xdi_adapter[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"II",              MI_UINT,      0,                 MI_CALL,              sizeof(byte),
                      xdi_info_capi_controller,  0,
                      0,            0
  },
  {"Number",          MI_UINT,      0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,Id),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,Id),
                      0,            0
  },
  {"Disabled",        MI_BOOLEAN,   0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,adapter_disabled),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,adapter_disabled),
                      0,            0
  },
  {"Channels",        MI_UINT,      0, MI_CALL,DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.Channels),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.Channels),
                      0,            0
  },
  {"Law",             MI_ASCIIZ,    0,                 MI_CALL,              0,
                      xdi_info,     (void*)"A",
                      0,            0
  },
  {"Global",          MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.Global_Options),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.Global_Options),
                      0,            0
  },
  {"B1",              MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.B1_Protocols),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.B1_Protocols),
                      0,            0
  },
  {"B2",              MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.B2_Protocols),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.B2_Protocols),
                      0,            0
  },
  {"B3",              MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.B3_Protocols),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.B3_Protocols),
                      0,            0
  },
  {"Manufacturer",    MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,manufacturer_features),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,manufacturer_features),
                      0,            0
  },
  {"Manufacturer2",   MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,manufacturer_features2),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,manufacturer_features2),
                      0,            0
  },
  {"Priv Options",    MI_BITFLD,    0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,profile.Manuf.private_options),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,profile.Manuf.private_options),
                      0,            0
  },
  {"RTP Payloads",    MI_BITFLD,    0,MI_CALL,DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,rtp_primary_payloads),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,rtp_primary_payloads),
                      0,            0
  },
  {"Add. RTP Payloads",MI_BITFLD,0,MI_CALL,DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,rtp_additional_payloads),
                      xdi_info_v, (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,rtp_additional_payloads),
                      0,            0
  },
  {"ValidServicesCIPMask",MI_BITFLD,0,MI_CALL,DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,ValidServicesCIPMask),
                      xdi_info_v, (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,ValidServicesCIPMask),
                      0,            0
  },
  {"AdvCodecFLAG",    MI_BOOLEAN,   0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER, AdvCodecFLAG),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER, AdvCodecFLAG),
                      0,            0
  },
  {"GroupOpt",        MI_UINT,   MI_WRITE, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER,group_optimization_enabled),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,group_optimization_enabled),
                      0,            0
  },
  {"DynL1",           MI_BOOLEAN, MI_WRITE, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER, flag_dynamic_l1_down),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,flag_dynamic_l1_down),
                      0,            0
  },
  {"SDRAM BAR",       MI_HINT,      0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER, sdram_bar),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,sdram_bar),
                      0,            0
  },
  {"XdiLogicalNr",    MI_UINT,      0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER, xdi_logical_adapter_number),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,xdi_logical_adapter_number),
                      0,            0
  },
  {"VsCAPI",          MI_BOOLEAN,   0, MI_CALL, DIVAS_SIZE_OF(DIVA_CAPI_ADAPTER, vscapi_mode),
                      xdi_info_v,   (void*)DIVAS_OFFSET_OF(DIVA_CAPI_ADAPTER,vscapi_mode),
                      0,            0
  },
  {"XDI",             MI_DIR,       0,                 MI_ENUM|MI_CALL|MI_E, 0,
                      mtpx_get_xdi_cfg_dir, &mtpx_man_max_enums,
                      0,            0
  }
};

static void* xdi_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context->fPara) ;
	int found;

	diva_os_api_lock (&irql);
	found = (diva_os_get_adapter(instance) != 0);
	diva_os_api_unlock (&irql);

	if (found) {
		if (cmd == CMD_NEXTNODE) {
			return (xdi_adapter);
		} else if (cmd == CMD_READVAR) {
			return (xdi_adapter);
		}
	}

	return (0);
}

static void* capi_man_end_of_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	return (0);
}

static void *adapter_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int i = 0;

	switch (cmd) {
		case CMD_READVAR:
			diva_os_api_lock (&irql);
			while (diva_os_get_adapter(i)) {
				i++;
			}
			diva_os_api_unlock (&irql);
			pM->dw_out = i;
			return ((void*)&pM->dw_out);

		default:
			return (0);
	}
}

static void *appl_info_v (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara), i;
	const APPL* a = 0;
	void* ret = 0;

	diva_os_api_lock (&irql);

	for (i = 0; i < max_appl; i++) {
		if (application && (application[i].Id == (word)instance+1)) {
			a = &application[i];
		}
	}

	if (a) {
		if (cmd == CMD_READVAR) {
			ret = diva_man_read_value_by_offset (pM, (byte*)a, PtrToUlong(context->fPara), context->node);
		}
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

static void *appl_info (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context[1].fPara), i;
	const APPL* a = 0;
	void* ret = 0;

	diva_os_api_lock (&irql);

	for (i = 0; i < max_appl; i++) {
		if (application && (application[i].Id == (word)instance+1)) {
			a = &application[i];
		}
	}

	if (a) {
		if (cmd == CMD_READVAR) {
			switch (*(char*)context->fPara) {
				case 'A': {
					const DIVA_CAPI_ADAPTER* adapter;

					for (i = 0, pM->dw_out = 0; ((adapter = diva_os_get_adapter(i))); i++) {
						pM->dw_out |= adapter->CIP_Mask[a->Id-1];
					}
					ret = &pM->dw_out;
				} break;
				case 'B': {
					const DIVA_CAPI_ADAPTER* adapter;

					for (i = 0, pM->dw_out = 0; ((adapter = diva_os_get_adapter(i))); i++) {
						pM->dw_out |= adapter->Info_Mask[a->Id-1];
					}
					ret = &pM->dw_out;
				} break;
				case 'r': {
					const DIVA_CAPI_ADAPTER* adapter;

					for (i = 0, pM->dw_out = 0; ((adapter = diva_os_get_adapter(i))); i++) {
						pM->dw_out |= adapter->requested_options_table[a->Id-1];
					}
					ret = &pM->dw_out;
				} break;
			}
		}
	}

	diva_os_api_unlock (&irql);

	return (ret);
}

/*
	Information about XDI adapter
	*/
static MAN_INFO capi_appl[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Id",              MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, Id),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL,Id),
                      0,            0
  },
  {"CIP_Mask",        MI_HINT,      0,                 MI_CALL,              4,
                      appl_info,    (void*)"A",
                      0,            0
  },
  {"Info_Mask",       MI_HINT,      0,                 MI_CALL,              4,
                      appl_info,    (void*)"B",
                      0,            0
  },
  {"requested_opt",   MI_HINT,      0,                 MI_CALL,              4,
                      appl_info,    (void*)"r",
                      0,            0
  },

  {"NullCREnable",    MI_BOOLEAN,   0,                 MI_CALL, DIVAS_SIZE_OF(APPL, NullCREnable),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, NullCREnable),
                      0,            0
  },
  {"CDEnable",        MI_BOOLEAN,   0,                 MI_CALL, DIVAS_SIZE_OF(APPL, CDEnable),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, CDEnable),
                      0,            0
  },
  {"S_Handle",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, S_Handle),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, S_Handle),
                      0,            0
  },
  {"msg_lost",        MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, msg_lost),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, msg_lost),
                      0,            0
  },
  {"appl_flags",      MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, appl_flags),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, appl_flags),
                      0,            0
  },
  {"Number",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, Number),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, Number),
                      0,            0
  },
  {"MaxBuffer",       MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, MaxBuffer),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, MaxBuffer),
                      0,            0
  },
  {"MaxNCCI",         MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, MaxNCCI),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, MaxNCCI),
                      0,            0
  },
  {"MaxNCCIData",     MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, MaxNCCIData),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, MaxNCCIData),
                      0,            0
  },
  {"MaxDataLength",   MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(APPL, MaxDataLength),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, MaxDataLength),
                      0,            0
  },
  {"BadDataB3Resp",   MI_UINT,      0,                 MI_CALL|MI_E, DIVAS_SIZE_OF(APPL, BadDataB3Resp),
                      appl_info_v,  (void*)DIVAS_OFFSET_OF(APPL, BadDataB3Resp),
                      0,            0
  }
};

static void* appl_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int instance = PtrToInt(context->fPara) ;
	int found = 0, i;

	diva_os_api_lock (&irql);

	for (i = 0; i < max_appl; i++) {
		if (application && (application[i].Id == (word)instance+1)) {
			found = 1;
		}
	}

	diva_os_api_unlock (&irql);

	if (found) {
		if (cmd == CMD_NEXTNODE) {
			return (capi_appl);
		} else if (cmd == CMD_READVAR) {
			return (capi_appl);
		}
	}

	return (0);
}

static void *appl_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int i = 0;

	switch (cmd) {
		case CMD_READVAR:
			diva_os_api_lock (&irql);
			while (application && (application[i].Id != 0) && (i < max_appl)) {
				i++;
			}
			diva_os_api_unlock (&irql);
			pM->dw_out = i;
			return ((void*)&pM->dw_out);

		default:
			return (0);
	}
}

/*
	Directory with applications
	*/
static MAN_INFO applications[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Count",           MI_UINT,      0,                 MI_CALL,              4,
                      appl_count,   0,
                      0,            0
  },
  {"A",               MI_DIR,       0,                 MI_ENUM|MI_CALL|MI_E, 0,
                      appl_dir,     &capi_man_max_appls,
                      0,            0
  }
};

/*
	Directory with adapters
	*/
#define ADAPTER_ENTRY(__nr__) {"A"#__nr__, MI_DIR,       0,                 MI_CALL,              0, \
                                            xdi_dir,      (void*)(__nr__ - 1), \
                                            0,            0 }
static MAN_INFO adapters[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Count",           MI_UINT,      0,                 MI_CALL,              4,
                      adapter_count,0,
                      0,            0
  },
  ADAPTER_ENTRY(1),
  ADAPTER_ENTRY(2),
  ADAPTER_ENTRY(3),
  ADAPTER_ENTRY(4),
  ADAPTER_ENTRY(5),
  ADAPTER_ENTRY(6),
  ADAPTER_ENTRY(7),
  ADAPTER_ENTRY(8),
  ADAPTER_ENTRY(9),
  ADAPTER_ENTRY(10),
  ADAPTER_ENTRY(11),
  ADAPTER_ENTRY(12),
  ADAPTER_ENTRY(13),
  ADAPTER_ENTRY(14),
  ADAPTER_ENTRY(15),
  ADAPTER_ENTRY(16),
  ADAPTER_ENTRY(17),
  ADAPTER_ENTRY(18),
  ADAPTER_ENTRY(19),
  ADAPTER_ENTRY(20),
  ADAPTER_ENTRY(21),
  ADAPTER_ENTRY(22),
  ADAPTER_ENTRY(23),
  ADAPTER_ENTRY(24),
  ADAPTER_ENTRY(25),
  ADAPTER_ENTRY(26),
  ADAPTER_ENTRY(27),
  ADAPTER_ENTRY(28),
  ADAPTER_ENTRY(29),
  ADAPTER_ENTRY(30),
  ADAPTER_ENTRY(31),
  ADAPTER_ENTRY(32),
  ADAPTER_ENTRY(33),
  ADAPTER_ENTRY(34),
  ADAPTER_ENTRY(35),
  ADAPTER_ENTRY(36),
  ADAPTER_ENTRY(37),
  ADAPTER_ENTRY(38),
  ADAPTER_ENTRY(39),
  ADAPTER_ENTRY(40),
  ADAPTER_ENTRY(41),
  ADAPTER_ENTRY(42),
  ADAPTER_ENTRY(43),
  ADAPTER_ENTRY(44),
  ADAPTER_ENTRY(45),
  ADAPTER_ENTRY(46),
  ADAPTER_ENTRY(47),
  ADAPTER_ENTRY(48),
  ADAPTER_ENTRY(49),
  ADAPTER_ENTRY(50),
  ADAPTER_ENTRY(51),
  ADAPTER_ENTRY(52),
  ADAPTER_ENTRY(53),
  ADAPTER_ENTRY(54),
  ADAPTER_ENTRY(55),
  ADAPTER_ENTRY(56),
  ADAPTER_ENTRY(57),
  ADAPTER_ENTRY(58),
  ADAPTER_ENTRY(59),
  ADAPTER_ENTRY(60),
  ADAPTER_ENTRY(61),
  ADAPTER_ENTRY(62),
  ADAPTER_ENTRY(63),
  ADAPTER_ENTRY(64),
  {"end",             MI_UINT,      0,                 MI_CALL|MI_E,         0,
                      capi_man_end_of_dir, 0,
                      0,            0
  }
};


/*
	Debug directory
	*/
static MAN_INFO debug[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"NCCIRemaps",      MI_UINT,      0,                 0, sizeof(diva_capi_ncci_mapping_bug),
                      &diva_capi_ncci_mapping_bug, 0,
                      0,            0
  },
  {"DumpStatus",      MI_EXECUTE,             0,       MI_E,             0,
                      diva_capi_dump_status,  0,
                      0,            0
  }
};


static void *plci_info_v (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int plci = PtrToInt(context[1].fPara) ;
	int ctlr = PtrToInt(context[2].fPara) ;
	void* ret = 0;
	const DIVA_CAPI_ADAPTER* a;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		a = diva_os_get_adapter (ctlr);
		if (a && (plci < a->max_plci)) {
			ret = diva_man_read_value_by_offset (pM, (byte*)&a->plci[plci],
			                                     PtrToUlong(context->fPara), context->node);
		}
		diva_os_api_unlock (&irql);
	}
	return (ret);
}

static void *plci_info (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int plci = PtrToInt(context[1].fPara) ;
	int ctlr = PtrToInt(context[2].fPara) ;
	void* ret = 0;
	const DIVA_CAPI_ADAPTER* a;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		a = diva_os_get_adapter (ctlr);
		if (a && (plci < a->max_plci)) {
			switch (*(char*)context->fPara) {
				case 'i':
					pM->dw_out = (dword)(plci + 1);
					ret = &pM->dw_out;
					break;
				case 'A':
					pM->dw_out = a->Id;
					ret = &pM->dw_out;
					break;
				case 'L':
					pM->dw_out = a->plci[plci].appl ? a->plci[plci].appl->Id : 0;
					ret = &pM->dw_out;
					break;
				case 'R':
					pM->dw_out = (a->plci[plci].req_in != a->plci[plci].req_in) ? 1 : 0;
					ret = &pM->dw_out;
					break;
				case 'C':
					pM->dw_out = appl_mask_empty (a->plci[plci].c_ind_mask_table) ? 0 : 1;
					ret = &pM->dw_out;
					break;
			}
		}
		diva_os_api_unlock (&irql);
	}
	return (ret);
}

#if IMPLEMENT_PLCI_TRACE
static void *plci_info_trace (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	void* ret = 0;

	if (cmd == CMD_READVAR) {
		int trace_entry = PtrToInt(context[0].fPara);
		int plci_nr     = PtrToInt(context[1].fPara);
		int ctlr        = PtrToInt(context[2].fPara);
		const DIVA_CAPI_ADAPTER* a;

		diva_os_api_lock (&irql);
		if ((a = diva_os_get_adapter (ctlr)) != 0 && plci_nr < a->max_plci) {
			const PLCI* plci = &a->plci[plci_nr];
			pM->dw_out = (dword)plci->trace_queue[trace_entry];
			ret = &pM->dw_out;
		}
		diva_os_api_unlock (&irql);
	}
	return (ret);
}
#endif


static void *plci_info_ext (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int plci = PtrToInt(context[2].fPara) ;
	int ctlr = PtrToInt(context[3].fPara) ;
	void* ret = 0;
	const DIVA_CAPI_ADAPTER* a;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		a = diva_os_get_adapter (ctlr);
		if (a && (plci < a->max_plci)) {
			ret = diva_man_read_value_by_offset (pM, (byte*)&a->plci[plci],
			                                     PtrToUlong(context->fPara), context->node);
		}
		diva_os_api_unlock (&irql);
	}
	return (ret);
}


static MAN_INFO plci_info_dir_ext[] =  {
  {"hook_state",      MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, hook_state),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, hook_state),
                      0,            0
  },
  {"tel",             MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, tel),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, tel),
                      0,            0
  },
  {"fax_code",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, fax_code),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, fax_code),
                      0,            0
  },
  {"fax_feature_bits",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, fax_feature_bits),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, fax_feature_bits),
                      0,            0
  },
  {"fax_feature2_bits",MI_HINT,     0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, fax_feature2_bits),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, fax_feature2_bits),
                      0,            0
  },
  {"nsf_control_bits",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, nsf_control_bits),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, nsf_control_bits),
                      0,            0
  },
  {"req_opt_conn",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, requested_options_conn),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, requested_options_conn),
                      0,            0
  },
  {"req_opt",         MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, requested_options),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, requested_options),
                      0,            0
  },
  {"B1_facilities",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, B1_facilities),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, B1_facilities),
                      0,            0
  },
  {"adjust_b_fac",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_facilities),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_facilities),
                      0,            0
  },
  {"adjust_b_cmd",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_command),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_command),
                      0,            0
  },
  {"adjust_b_ncci",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_ncci),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_ncci),
                      0,            0
  },
  {"adjust_b_mode",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_mode),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_mode),
                      0,            0
  },
  {"adjust_b_state",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_state),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_state),
                      0,            0
  },
  {"adjust_b_restore",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adjust_b_restore),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, adjust_b_restore),
                      0,            0
  },
  {"dtmf_rec_active", MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_rec_active),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_rec_active),
                      0,            0
  },
  {"dtmf_rec_pulse_ms",MI_UINT,     0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_rec_pulse_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_rec_pulse_ms),
                      0,            0
  },
  {"dtmf_rec_pause_ms",MI_UINT,     0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_rec_pause_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_rec_pause_ms),
                      0,            0
  },
  {"dtmf_send_requests",MI_HINT,    0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_send_requests),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_send_requests),
                      0,            0
  },
  {"dtmf_send_pulse_ms",MI_UINT,    0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_send_pulse_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_send_pulse_ms),
                      0,            0
  },
  {"dtmf_send_pause_ms",MI_UINT,    0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_send_pause_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_send_pause_ms),
                      0,            0
  },
  {"dtmf_cmd",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, dtmf_cmd),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, dtmf_cmd),
                      0,            0
  },
  {"capidtmf_state",  MI_HINT,      0,                 MI_CALL, (byte)(DIVAS_SIZE_OF(PLCI, capidtmf_state)),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, capidtmf_state),
                      0,            0
  },
#if IMPLEMENT_LINE_INTERCONNECT2
  {"li_bchannel_id",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_bchannel_id),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_bchannel_id),
                      0,            0
  },
  {"li_channel_bits", MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_channel_bits),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_channel_bits),
                      0,            0
  },
  {"li_notify_update",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_notify_update),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_notify_update),
                      0,            0
  },
  {"li_cmd",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_cmd),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_cmd),
                      0,            0
  },
  {"li_write_command",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_write_command),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_write_command),
                      0,            0
  },
  {"li_write_channel",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_write_channel),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_write_channel),
                      0,            0
  },
  {"li_plci_b_write_pos",MI_HINT,   0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_plci_b_write_pos),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_plci_b_write_pos),
                      0,            0
  },
  {"li_plci_b_read_pos",MI_HINT,    0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_plci_b_read_pos),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_plci_b_read_pos),
                      0,            0
  },
  {"li_plci_b_req_pos",MI_HINT,     0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, li_plci_b_req_pos),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, li_plci_b_req_pos),
                      0,            0
  },
  #endif
  {"ec_cmd",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ec_cmd),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, ec_cmd),
                      0,            0
  },
  {"ec_idi_options",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ec_idi_options),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, ec_idi_options),
                      0,            0
  },
  {"ec_span_ms",      MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ec_span_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, ec_span_ms),
                      0,            0
  },
  {"ec_predelay_ms",  MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ec_predelay_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, ec_predelay_ms),
                      0,            0
  },
  {"ec_sparse_span_ms",MI_UINT,     0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ec_sparse_span_ms),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, ec_sparse_span_ms),
                      0,            0
  },
  {"tone_last_ind_code", MI_HINT,   0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, tone_last_indication_code),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, tone_last_indication_code),
                      0,            0
  },
  {"measure_active",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, measure_active),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, measure_active),
                      0,            0
  },
  {"measure_cmd",     MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, measure_cmd),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, measure_cmd),
                      0,            0
  },
  {"pitch_flags",     MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, pitch_flags),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, pitch_flags),
                      0,            0
  },
  {"rx_sample_rate",  MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, pitch_rx_sample_rate),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, pitch_rx_sample_rate),
                      0,            0
  },
  {"tx_sample_rate",  MI_UINT,      0,                 MI_CALL|MI_E, DIVAS_SIZE_OF(PLCI, pitch_tx_sample_rate),
                      plci_info_ext,(void*)DIVAS_OFFSET_OF(PLCI, pitch_tx_sample_rate),
                      0,            0
  }
};

static MAN_INFO plci_info_dir[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Nr",              MI_UINT,      0,                 MI_CALL,             4,
                      plci_info,    (void*)"i",
                      0,            0
  },
  {"Id",              MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, Id),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, Id),
                      0,            0
  },
  {"Adapter",         MI_UINT,      0,                 MI_CALL,             4,
                      plci_info,    (void*)"A",
                      0,            0
  },
  {"appl",            MI_UINT,      0,                 MI_CALL,             4,
                      plci_info,    (void*)"L",
                      0,            0
  },
  {"SigID",           MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, Sig.Id),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, Sig.Id),
                      0,            0
  },
  {"NetID",           MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, NL.Id),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, NL.Id),
                      0,            0
  },
  {"State",           MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, State),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, State),
                      0,            0
  },
  {"sig_req",         MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, sig_req),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, sig_req),
                      0,            0
  },
  {"nl_req",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, nl_req),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, nl_req),
                      0,            0
  },
  {"req_pending",     MI_UINT,      0,                 MI_CALL,             4,
                      plci_info,    (void*)"R",
                      0,            0
  },
  {"SuppState",       MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, SuppState),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, SuppState),
                      0,            0
  },
  {"c_ind_mask_set",  MI_UINT,      0,                 MI_CALL,             4,
                      plci_info,    (void*)"C",
                      0,            0
  },
  {"channels",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, channels),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, channels),
                      0,            0
  },
  {"B1_resource",     MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, B1_resource),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, B1_resource),
                      0,            0
  },
#if IMPLEMENT_NULL_PLCI
  {"B1_options",      MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, B1_options),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, B1_options),
                      0,            0
  },
#endif /* IMPLEMENT_NULL_PLCI */
  {"B2_prot",         MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, B2_prot),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, B2_prot),
                      0,            0
  },
  {"B3_prot",         MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, B3_prot),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, B3_prot),
                      0,            0
  },
  {"command",         MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, command),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, command),
                      0,            0
  },
  {"m_command",       MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, m_command),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, m_command),
                      0,            0
  },
  {"internal_command",MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, internal_command),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, internal_command),
                      0,            0
  },
  {"send_disc",       MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, send_disc),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, send_disc),
                      0,            0
  },
  {"sig_global_req",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, sig_global_req),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, sig_global_req),
                      0,            0
  },
  {"sig_remove_id",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, sig_remove_id),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, sig_remove_id),
                      0,            0
  },
  {"nl_global_req",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, nl_global_req),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, nl_global_req),
                      0,            0
  },
  {"nl_remove_id",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, nl_remove_id),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, nl_remove_id),
                      0,            0
  },
  {"b_channel",       MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, b_channel),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, b_channel),
                      0,            0
  },
  {"adv_nl",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, adv_nl),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, adv_nl),
                      0,            0
  },
  {"manufacturer",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, manufacturer),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, manufacturer),
                      0,            0
  },
  {"call_dir",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, call_dir),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, call_dir),
                      0,            0
  },
  {"spoofed_msg",     MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, spoofed_msg),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, spoofed_msg),
                      0,            0
  },
  {"ptyState",        MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ptyState),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, ptyState),
                      0,            0
  },
  {"cr_enquiry",      MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, cr_enquiry),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, cr_enquiry),
                      0,            0
  },
  {"hup_fc_timer",    MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, hangup_flow_ctrl_timer),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, hangup_flow_ctrl_timer),
                      0,            0
  },
  {"ncci_ring_list",  MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, ncci_ring_list),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, ncci_ring_list),
                      0,            0
  },
  {"vswitchstate",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, vswitchstate),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, vswitchstate),
                      0,            0
  },
  {"vsprot",          MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, vsprot),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, vsprot),
                      0,            0
  },
  {"vsprotdialect",   MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, vsprotdialect),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, vsprotdialect),
                      0,            0
  },
  {"notifiedcall",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, notifiedcall),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, notifiedcall),
                      0,            0
  },
  {"rx_dma_descr",    MI_INT,       0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, rx_dma_descriptor),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, rx_dma_descriptor),
                      0,            0
  },
  {"rx_dma_magic",    MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, rx_dma_magic),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, rx_dma_magic),
                      0,            0
  },
  {"calledfromccbscall",MI_HINT,    0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, calledfromccbscall),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, calledfromccbscall),
                      0,            0
  },
  {"gothandle",       MI_HINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, gothandle),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, gothandle),
                      0,            0
  },
#if IMPLEMENT_PLCI_TRACE
  {"T_POS",           MI_UINT,      0,                 MI_CALL, DIVAS_SIZE_OF(PLCI, trace_pos),
                      plci_info_v,  (void*)DIVAS_OFFSET_OF(PLCI, trace_pos),
                      0,            0
  },
  {"T",               MI_UINT,    0,                 MI_CALL|MI_ENUM,             sizeof(dword),
                      plci_info_trace,    &capi_man_max_plci_trace_entries,
                      0,            0
  },
#endif
  {"Extended",        MI_DIR,       0,                 MI_E,        0,
                      (void*)plci_info_dir_ext,0,
                      0,            0
  }
};

static void* plci_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	int ctlr = PtrToInt(context[1].fPara) ;
	const DIVA_CAPI_ADAPTER* a;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		a = diva_os_get_adapter (ctlr);
		if (a) {
			pM->dw_out = (dword)(a->max_plci);
		}
		diva_os_api_unlock (&irql);

		if (a) {
			return (&pM->dw_out);
		}
	}

	return (0);
}

/*
	View the PLCI directory for plcis: 0 <= plci < a->max_plci
	*/
static void* plci_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int plci = PtrToInt(context->fPara) ;
	const DIVA_CAPI_ADAPTER* a;
	int ctlr = PtrToInt(context[1].fPara) ;
	byte max_plci = 0;

	diva_os_api_lock (&irql);
	a = diva_os_get_adapter (ctlr);
	if (a) {
		max_plci = a->max_plci;
	}
	diva_os_api_unlock (&irql);

	if (a && (plci < max_plci)) {
		if (cmd == CMD_NEXTNODE) {
			return (plci_info_dir);
		} else if (cmd == CMD_READVAR) {
			return (plci_info_dir);
		}
	}
	return (0);
}

/*
	PLCI status directory
	*/
static MAN_INFO controller_plcis[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Count",           MI_UINT,      0,                 MI_CALL,              4,
                      plci_count,   0,
                      0,            0
  },
  {"P",               MI_DIR,       0,                 MI_ENUM|MI_CALL|MI_E, 0,
                      plci_dir,     &capi_man_max_plcis,
                      0,            0
  }
};

static void* controller_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_os_spin_lock_magic_t irql;
	int ctlr = PtrToInt(context->fPara) ;
	int found;

	diva_os_api_lock (&irql);
	found = (diva_os_get_adapter(ctlr) != 0);
	diva_os_api_unlock (&irql);

	if (found) {
		if (cmd == CMD_NEXTNODE) {
			return (controller_plcis);
		} else if (cmd == CMD_READVAR) {
			return (controller_plcis);
		}
	}

	return (0);
}

/*
	Directory with controllers
	*/
#define CONTROLLER_ENTRY(__nr__) {"C"#__nr__, MI_DIR,       0,                 MI_CALL,              0, \
                                            controller_dir,      (void*)(__nr__ - 1), \
                                            0,            0 }
static MAN_INFO controllers[] =  {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Count",           MI_UINT,      0,                 MI_CALL,              4,
                      adapter_count,0,
                      0,            0
  },
  CONTROLLER_ENTRY(1),
  CONTROLLER_ENTRY(2),
  CONTROLLER_ENTRY(3),
  CONTROLLER_ENTRY(4),
  CONTROLLER_ENTRY(5),
  CONTROLLER_ENTRY(6),
  CONTROLLER_ENTRY(7),
  CONTROLLER_ENTRY(8),
  CONTROLLER_ENTRY(9),
  CONTROLLER_ENTRY(10),
  CONTROLLER_ENTRY(11),
  CONTROLLER_ENTRY(12),
  CONTROLLER_ENTRY(13),
  CONTROLLER_ENTRY(14),
  CONTROLLER_ENTRY(15),
  CONTROLLER_ENTRY(16),
  CONTROLLER_ENTRY(17),
  CONTROLLER_ENTRY(18),
  CONTROLLER_ENTRY(19),
  CONTROLLER_ENTRY(20),
  CONTROLLER_ENTRY(21),
  CONTROLLER_ENTRY(22),
  CONTROLLER_ENTRY(23),
  CONTROLLER_ENTRY(24),
  CONTROLLER_ENTRY(25),
  CONTROLLER_ENTRY(26),
  CONTROLLER_ENTRY(27),
  CONTROLLER_ENTRY(28),
  CONTROLLER_ENTRY(29),
  CONTROLLER_ENTRY(30),
  CONTROLLER_ENTRY(31),
  CONTROLLER_ENTRY(32),
  CONTROLLER_ENTRY(33),
  CONTROLLER_ENTRY(34),
  CONTROLLER_ENTRY(35),
  CONTROLLER_ENTRY(36),
  CONTROLLER_ENTRY(37),
  CONTROLLER_ENTRY(38),
  CONTROLLER_ENTRY(39),
  CONTROLLER_ENTRY(40),
  CONTROLLER_ENTRY(41),
  CONTROLLER_ENTRY(42),
  CONTROLLER_ENTRY(43),
  CONTROLLER_ENTRY(44),
  CONTROLLER_ENTRY(45),
  CONTROLLER_ENTRY(46),
  CONTROLLER_ENTRY(47),
  CONTROLLER_ENTRY(48),
  CONTROLLER_ENTRY(49),
  CONTROLLER_ENTRY(50),
  CONTROLLER_ENTRY(51),
  CONTROLLER_ENTRY(52),
  CONTROLLER_ENTRY(53),
  CONTROLLER_ENTRY(54),
  CONTROLLER_ENTRY(55),
  CONTROLLER_ENTRY(56),
  CONTROLLER_ENTRY(57),
  CONTROLLER_ENTRY(58),
  CONTROLLER_ENTRY(59),
  CONTROLLER_ENTRY(60),
  CONTROLLER_ENTRY(61),
  CONTROLLER_ENTRY(62),
  CONTROLLER_ENTRY(63),
  CONTROLLER_ENTRY(64),
  {"end",             MI_UINT,      0,                 MI_CALL|MI_E,         0,
                      capi_man_end_of_dir, 0,
                      0,            0
  }
};

/*
	Return total count of active PLCI's
	*/
static void *plci_count_active (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	const DIVA_CAPI_ADAPTER* a;
	int plcis = 0, nr = 0, i;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		while ((a = diva_os_get_adapter (nr++))) {
			for (i = 0; i < a->max_plci; i++) {
				if (a->plci[i].Id) {
					plcis++;
				}
			}
		}
		diva_os_api_unlock (&irql);

		pM->dw_out = (dword)plcis;

		return (&pM->dw_out);
	}

	return (0);
}

/*
	Return total count of available PLCI's
	*/
static void *plci_count_total (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	diva_os_spin_lock_magic_t irql;
	const DIVA_CAPI_ADAPTER* a;
	int plcis = 0;
	int nr = 0;

	if (cmd == CMD_READVAR) {
		diva_os_api_lock (&irql);
		while ((a = diva_os_get_adapter (nr++))) {
			plcis += a->max_plci;
		}
		diva_os_api_unlock (&irql);

		pM->dw_out = (dword)plcis;

		return (&pM->dw_out);
	}

	return (0);
}

/*
	Root directory
	*/
static MAN_INFO root[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"MIF Version",     MI_HINT,      0,                 0,                    4,
                      &MIFVer,      0,
                      0,            0
  },
  {"Build",           MI_ASCIIZ,    0,                 0,                    0,
                      NVer,         0,
                      0,            0
  },
  {"XDI",             MI_DIR,       0,                 0,                    0,
                      adapters,     0,
                      0,            0
  },
  {"APPL",            MI_DIR,       0,                 0,                    0,
                      applications, 0,
                      0,            0
  },
  {"ActivePLCICount", MI_UINT,      0,                 MI_CALL,              4,
                      (void*)plci_count_active,0,
                      0,            0
  },
  {"TotalPLCICount",  MI_UINT,      0,                 MI_CALL,              4,
                      (void*)plci_count_total,0,
                      0,            0
  },
  {"PLCI",            MI_DIR,       0,                 0,                    0,
                      controllers,  0,
                      0,            0
  },
  {"Debug",           MI_DIR,       0,                 MI_E,                 0,
                      debug,        0,
                      0,            0
  }
};

MAN_INFO* drv_man_get_driver_root (void) {
	return (&root[0]);
}

const char* drv_man_get_driver_name (void) {
	return ("CAPI");
}

/*
 * vim:ts=2
 */
