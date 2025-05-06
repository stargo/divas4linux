
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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
#include "di_defs.h"
#include "pc.h"
#include "divasync.h"
#include "manage.h"
#include "man_defs.h"
#include "sys_man.h"
#include "drv_man.h"
#include "port.h"
#include "hdlc.h"
#include "v110.h"
#include "piafs.h"
#include "isdn_if.h"
#include "isdn.h"
#include "atp_if.h"
#include "fax_stat.h"

#ifndef DIVA_BUILD
#define DIVA_BUILD BUILD
#endif

#ifndef DIVA_SMP
#define NVer DIVA_BUILD
#else
#define NVer DIVA_BUILD"-SMP"
#endif
#define MAN_INTERFACE_VERSION 0x0115
static dword MIFVer = MAN_INTERFACE_VERSION;
#define DIVA_MTPX_MAX_ENUM_DIRECTORIES     99
#define TTYS_PER_DIRECTORY 50
static int mtpx_man_max_enums = DIVA_MTPX_MAX_ENUM_DIRECTORIES;
static const char* isdnStateName (byte State);
static void *diva_get_nothing (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar);
static dword n_call_filters = MAX_CALL_FILTER_ENTRIES;

/*
	Global module parameters
	*/
/*
	Cause value to be used in case none of the TTY ports
	was willing to process the call (for example all ports
  was closed).
  */
byte ports_unavailable_cause = 0;

/*
	IMPORTS
	*/
extern int sprintf (char*, const char*, ...);
extern void print_adapter_status (void);
extern ISDN_PORT* diva_tty_get_port (int i);
extern ISDN_PORT* ttyFindPort (int nr);
extern byte atGetProfile (byte Prot);
extern int Channel_Count;
extern int global_options;
extern char diva_tty_init_prm[64];
extern diva_call_filter_t diva_tty_call_filter;
extern int divas_tty_wait_sig_disc;
extern int divas_tty_debug_sig;
extern int divas_tty_debug_net;
extern int divas_tty_debug_net_data;
extern int divas_tty_debug_tty_data;
extern dword diva_tty_dpc_scheduler_protection_channel_count;

extern ISDN_PORT_DRIVER PortDriver;

static void diva_tty_dump_status (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {

	print_adapter_status ();

}

/*
	Debug directory
	*/
static MAN_INFO debug[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"DumpStatus",      MI_EXECUTE,             0,          0,             0,
                      diva_tty_dump_status, 0,
                      0,            0
  },
  {"DebugSig",        MI_BOOLEAN,      MI_WRITE,          0,       sizeof(int),
                      (void*)&divas_tty_debug_sig, 0,
                      0,            0
  },
  {"DebugNet",        MI_BOOLEAN,      MI_WRITE,          0,       sizeof(int),
                      (void*)&divas_tty_debug_net, 0,
                      0,            0
  },
  {"DebugNetData",    MI_BOOLEAN,      MI_WRITE,          0,       sizeof(int),
                      (void*)&divas_tty_debug_net_data, 0,
                      0,            0
  },
  {"DebugTTYData",    MI_BOOLEAN,      MI_WRITE,          0,       sizeof(int),
                      (void*)&divas_tty_debug_tty_data, 0,
                      0,            0
  },
  {"Last",            MI_UINT,      0,         MI_CALL|MI_E,       4,
                      diva_get_nothing, 0,
                      0,            0
  }
};

static void *diva_get_tty_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;

	if (Channel_Count && (cmd == CMD_READVAR)) {
		pM->dw_out = (dword)(Channel_Count);
		return (&pM->dw_out);
	}

	return (0);
}

static void *diva_get_opened_tty_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	int i;

	for (i = 1, pM->dw_out = 0; i < (Channel_Count+1); i++) {
		pM->dw_out += (diva_tty_get_port(i) != 0);
	}

	return (&pM->dw_out);
}

static void *diva_get_connected_tty_count (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	int i;

	for (i = 1, pM->dw_out = 0; i < (Channel_Count+1); i++) {
		ISDN_PORT* P = diva_tty_get_port(i);
		if (P && P->Channel) {
			pM->dw_out++;
		}
	}

	return (&pM->dw_out);
}

static void *diva_get_nothing (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	return (0);
}


/*
	Retrieve information about specific TTY
	*/
static void *tty_info (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	int instance = (int)(long)context[1].fPara;
	int start    = (int)(long)context[2].fPara; /* start of this directory */
	void* ret = 0;
	int tty_nr;
	static char tmp_name[128];


	if (instance >= TTYS_PER_DIRECTORY) {
		/*
			Only TTYS_PER_DIRECTORY entries in every directory
			*/
		return (0);
	}

	instance += start;
	tty_nr = instance+1;


	if (instance < Channel_Count) {
		if (cmd == CMD_READVAR) {
			switch (*(char*)context->fPara) {
				case 1:
					pM->dw_out = (dword)tty_nr;
					ret = &pM->dw_out;
					break;
				case 2:
					str_cpy(&tmp_name[0], (diva_tty_get_port (tty_nr) != 0) ? "YES" : "NO");
					ret = &tmp_name[0];
					break;
				case 3: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					if (P) {
						sprintf (tmp_name, "ttyds%02d", tty_nr);
						ret = &tmp_name[0];
					}
				} break;
				case 4: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					if (P) {
						ret = &P->Name[0];
					}
				} break;
				case 5: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->b_out = 0;
					if (P && P->last_conn_info_valid) {
						pM->b_out = (byte)P->last_conn_info.Prot;
					}
					ret = &pM->b_out;
				} break;
				case 7: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					tmp_name[0] = 0;
					if (P) {
						str_cpy (&tmp_name[0], &P->At.Ring[0]);
					}
					ret = &tmp_name[0];
				} break;
				case 8: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					tmp_name[0] = 0;
					if (P) {
						str_cpy (&tmp_name[0], &P->CallbackParms.Dest[0]);
					}
					ret = &tmp_name[0];
				} break;
				case '-': {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					const char* name = "none";

					if (P && P->last_conn_info_valid) {
						name = "unknown";
						switch (P->last_conn_info.Prot) {
							case ISDN_PROT_X75:
								name = "X.75";
								break;
							case ISDN_PROT_V110_s:
								name = "V.110SYNC";
								break;
							case ISDN_PROT_V110_a:
								name = "V.110";
								break;
							case ISDN_PROT_V120:
								name = "V.120";
								break;
							case ISDN_PROT_FAX:
								name = "FAX";
								break;
							case ISDN_PROT_VOICE:
								name = "VOICE";
								break;
							case ISDN_PROT_RAW:
								name = "HDLC";
								break;
							case ISDN_PROT_BTX:
								name = "BTX";
								break;
							case ISDN_PROT_EXT_0:
								name = "EXT";
								break;
							case ISDN_PROT_X75V42:
								name = "X.75v42bis";
								break;
							case ISDN_PROT_PIAFS_32K:
								name = "PIAFS32K";
								break;
							case ISDN_PROT_PIAFS_64K_FIX:
								name = "PIAFS64KFIX";
								break;
							case ISDN_PROT_PIAFS_64K_VAR:
								name = "PIAFS64KVAR";
								break;
							case ISDN_PROT_DET:
								name = "DETECTION";
								break;
							case ISDN_PROT_MDM_s:
								name = "SYNCMODEM";
								break;
							case ISDN_PROT_MDM_a:
								name = "MODEM";
								break;
						}
					}
					str_cpy (&tmp_name[0], name);

					ret = &tmp_name[0];
				} break;
				case '+': {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					str_cpy (&tmp_name[0], (P && P->pMsrShadow && (*P->pMsrShadow & MS_RLSD_ON)) ? "ON" : "OFF");
					ret = &tmp_name[0];
				} break;
				case 'C': {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					ret = (P && P->Channel) ? "YES" : "NO";
				} break;
				case 9: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->dw_out = (P && P->Channel) ? (dword)(unsigned long)P->Channel : 0;
					ret = &pM->dw_out;
				} break;
				case 0x10: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					str_cpy(&tmp_name[0], (P && P->Channel) ? (char*)((ISDN_CHANNEL*)P->Channel)->Id : "");
					ret = &tmp_name[0];
				} break;
				case 0x11: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->b_out = 0xff;
					if (P && P->last_conn_info_valid) {
						pM->b_out = atGetProfile (P->last_conn_info.Prot);
					}
					ret = &pM->b_out;
				} break;
				case 0x12: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					tmp_name[0] = 0;
					if (P && P->last_conn_info_valid) {
						byte profile = atGetProfile (P->last_conn_info.Prot);
						if (profile < 0xff) {
							sprintf (tmp_name, "AT&F%d", (int)profile);
						}
					}
					ret = &tmp_name[0];
				} break;
				case 'D': {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					if (P) {
						ret = "";
					}
				} break;
				case 0x14: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->b_out = 0xff;
					if (P && P->last_conn_info_valid) {
						pM->b_out = P->last_conn_info.Mode;
						pM->b_out = P->CallbackParms.Mode;
					}
					ret = &pM->b_out;
				} break;
				case 0x15: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->dw_out = 0;
					if (P && P->last_conn_info_valid) {
						pM->dw_out = P->last_conn_info.TxSpeed;
					}
					ret = &pM->dw_out;
				} break;
				case 0x17: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->dw_out = 0;
					if (P && P->last_conn_info_valid) {
						pM->dw_out = P->last_conn_info.RxSpeed;
					}
					ret = &pM->dw_out;
				} break;
				case 0x18: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					pM->b_out = 0;
					if (P && P->last_conn_info_valid) {
						pM->b_out = P->At.f.Baud;
					}
					ret = &pM->b_out;
				} break;
				case 0x19: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					tmp_name[0] = 0;
					if (P) {
						sprintf (&tmp_name[0], "%s (%d)", (char*)atState(P->State), (int)P->State);
					}
					ret = &tmp_name[0];
				} break;
				case 0x20: {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					sprintf (&tmp_name[0], "%s (%d)", isdnStateName (0), 0);
					if (P && P->Channel) {
						sprintf (&tmp_name[0], "%s (%d)",
										 isdnStateName (((ISDN_CHANNEL*)P->Channel)->State),
										 ((ISDN_CHANNEL*)P->Channel)->State);
					}
					ret = &tmp_name[0];
				} break;
			}
		} else if (cmd == CMD_EXECUTE) {
			switch (*(char*)context->fPara) {
				case 'D': {
					ISDN_PORT* P = ttyFindPort (tty_nr);
					if (P) {
						DBG_LOG(("[%p:%s] Management DTR DROP", P->Channel, P->Name))
						atDtrOff (P);
					}
				} break;
			}
		}
	}

	return (ret);
}

/*
	TTY information directory
	*/
static MAN_INFO tty_info_dir[] =  {
  {"Nr",              MI_UINT,      0,                 MI_CALL,             4,
                      tty_info,     (void*)"\x01",
                      0,            0
  },
  {"Open",            MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x02",
                      0,            0
  },
  {"DCD",             MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"+",
                      0,            0
  },
  {"Connected",       MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"C",
                      0,            0
  },
  {"SystemName",      MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x03",
                      0,            0
  },
  {"InternalName",    MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x04",
                      0,            0
  },
  {"Protocol",        MI_UINT,      0,                 MI_CALL,             1,
                      tty_info,     (void*)"\x05",
                      0,            0
  },
  {"ProtocolName",    MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"-",
                      0,            0
  },
  {"Profile",         MI_UINT,      0,                 MI_CALL,             1,
                      tty_info,     (void*)"\x11",
                      0,            0
  },
  {"AtInit",          MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x12",
                      0,            0
  },
  {"CID",             MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x07",
                      0,            0
  },
  {"DID",             MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x08",
                      0,            0
  },
  {"Channel",         MI_HINT,      0,                 MI_CALL,             4,
                      tty_info,     (void*)"\x09",
                      0,            0
  },
  {"ChannelId",       MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x10",
                      0,            0
  },
  {"Mode",            MI_UINT,      0,                 MI_CALL,             1,
                      tty_info,     (void*)"\x14",
                      0,            0
  },
  {"TxSpeed",         MI_UINT,      0,                 MI_CALL,             4,
                      tty_info,     (void*)"\x15",
                      0,            0
  },
  {"RxSpeed",         MI_UINT,      0,                 MI_CALL,             4,
                      tty_info,     (void*)"\x17",
                      0,            0
  },
  {"Baud",            MI_UINT,      0,                 MI_CALL,             1,
                      tty_info,     (void*)"\x18",
                      0,            0
  },
  {"PortState",       MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x19",
                      0,            0
  },
  {"ChannelState",    MI_ASCIIZ,    0,                 MI_CALL,             0,
                      tty_info,     (void*)"\x20",
                      0,            0
  },
  {"DTR drop",        MI_EXECUTE,   0,                 0,                   0,
                      tty_info,     (void*)"D",
                      0,            0
  },
  {"Last",            MI_UINT,      0,                 MI_CALL|MI_E,        4,
                      diva_get_nothing,0,
                      0,            0
  }
};


/*
	View the TTY directory for ttys: start <= tty < start+TTYS_PER_DIRECTORY
	The range of the instance parameter is independent, and also will vary
	in the range 0 ... MAX (where MAX >> PLCIS_PER_DIRECTORY)
	*/
static void *tty_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	int instance = (int)(long)context->fPara;
	int tty_nr   = (int)(long)context[1].fPara; /* start of this directory */

	if (instance >= TTYS_PER_DIRECTORY) {
		/*
			Only TTYS_PER_DIRECTORY entries in every directory
			*/
		return (0);
	}

	tty_nr += instance;
	if (tty_nr < Channel_Count) {
		if (cmd == CMD_NEXTNODE) {
			return (tty_info_dir);
		} else if (cmd == CMD_READVAR) {
			return (tty_info_dir);
		}
	}

	return (0);
}

static MAN_INFO tty_ports[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"T",               MI_DIR,       0,                 MI_ENUM|MI_CALL|MI_E, 0,
                      tty_dir,      &mtpx_man_max_enums,
                      0,            0
  }
};

/*
	Display directory only in case tty_nr < Channel_Count
	*/
static void *get_ttys (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	int tty_nr = (int)(long)context->fPara; /* start of this directory */

	if (tty_nr < Channel_Count) {
		if (cmd == CMD_NEXTNODE) {
			return (tty_ports);
		} else if (cmd == CMD_READVAR) {
			return (tty_ports);
		}
	}

	return (0);
}

#define TTY_ENTRY(__name__,__start__) \
  {"TTY"__name__,     MI_DIR,       0,                 MI_CALL,               0, \
                      get_ttys,     (void*)(__start__), \
                      0,            0 \
	}

static MAN_INFO ttys[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"Count",           MI_UINT,                0,       MI_CALL,                   4,
                      diva_get_tty_count,     0,
                      0,            0
  },
  {"Open",            MI_UINT,                0,       MI_CALL,                   4,
                      diva_get_opened_tty_count,0,
                      0,            0
  },
  {"Connected",       MI_UINT,                0,       MI_CALL,                   4,
                      diva_get_connected_tty_count,0,
                      0,            0
  },

	TTY_ENTRY("1-50",     0),
	TTY_ENTRY("51-100",   50),
	TTY_ENTRY("101-150",  100),
	TTY_ENTRY("151-200",  150),
	TTY_ENTRY("201-250",  200),
	TTY_ENTRY("251-300",  250),
	TTY_ENTRY("301-350",  300),
	TTY_ENTRY("351-400",  350),
	TTY_ENTRY("401-450",  400),
	TTY_ENTRY("451-500",  450),
	TTY_ENTRY("501-550",  500),

  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,              1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

static void *global_options_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;

	if (cmd == CMD_READVAR) {
		int mask = (int)(long)context->fPara; /* bit mask for current option */
		pM->b_out = ((global_options & mask) != 0);
		return (&pM->b_out);
	} else if (cmd == CMD_WRITEVAR) {
		int mask = (int)(long)context->fPara; /* bit mask for current option */
		if (*(byte*)info) {
			global_options |=  mask;
		} else {
			global_options &= ~mask;
		}
	}

	return (0);
}

static void *global_port_options_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
  diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	dword mask                   = (dword)(long)context->fPara; /* bit mask for current option */

  if (cmd == CMD_READVAR) {
    pM->b_out = ((PortDriver.Fixes & mask) != 0);
    return (&pM->b_out);
  } else if (cmd == CMD_WRITEVAR) {
    if (*(byte*)info) {
      PortDriver.Fixes |=  mask;
    } else {
      PortDriver.Fixes &= ~mask;
    }
  }

  return (0);
}

static void *global_options1_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;

	if (cmd == CMD_READVAR) {
		switch (*(const char*)context->fPara) {
			case 'J':
				pM->b_out = ports_unavailable_cause;
				return (&pM->b_out);
		}
	} else if (cmd == CMD_WRITEVAR) {
		switch (*(const char*)context->fPara) {
			case 'J':
				ports_unavailable_cause = ((*(byte*)info) & 0x7f);
				break;
		}
	}

	return (0);
}

static void *tty_init_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	static byte tmp[69];

	if (cmd == CMD_READVAR) {
		str_cpy (tmp, &diva_tty_init_prm[0]);
		return (&tmp[0]);
	} else if (cmd == CMD_WRITEVAR) {
		str_cpy (&diva_tty_init_prm[0], (char*)info);
	}

	return (0);
}

/*
	Root directory
	*/
static MAN_INFO global_options_dir[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"GlobalOptionsRaw",MI_HINT,      MI_WRITE,          0,                    4,
											&global_options,0,
                      0,            0
  },
  {"FAX_FORCE_ECM",   MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_FAX_FORCE_ECM,
                      0,            0
  },
  {"FAX_FORCE_V34",   MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_FAX_FORCE_V34,
                      0,            0
  },
  {"FAX_FORCE_SEP_SUB_PWD",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_FAX_FORCE_SEP_SUB_PWD,
                      0,            0
  },
  {"DIVA_FAX_ALLOW_V34_CODES",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_FAX_ALLOW_V34_CODES,
                      0,            0
  },
  {"DIVA_FAX_ALLOW_HIRES",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_FAX_ALLOW_HIRES,
                      0,            0
  },
  {"DIVA_ISDN_IGNORE_NUMBER_TYPE",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_ISDN_IGNORE_NUMBER_TYPE,
                      0,            0
  },
  {"DIVA_ISDN_AT_RSP_IF_RINGING",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_ISDN_AT_RSP_IF_RINGING,
                      0,            0
  },
  {"DIVA_ISDN_DIAL_RR",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_options_proc, (void*)DIVA_ISDN_DIAL_RR,
                      0,            0
  },

  {"TTY_INIT",        MI_ASCIIZ,    MI_WRITE,          MI_CALL,              64,
                      (void*)tty_init_proc,0,
                      0,            0
  },
  {"Cause",           MI_HINT,      MI_WRITE,          MI_CALL,              1,
                      (void*)global_options1_proc, (void*)"J",
                      0,            0
  },
  {"WaitSigDisc",     MI_BOOLEAN,      MI_WRITE,          0,       sizeof(int),
                      (void*)&divas_tty_wait_sig_disc,    0,
                      0,            0
  },
  {"ROUND_ROBIN_RINGS",
											MI_BOOLEAN,   MI_WRITE,          MI_CALL,              1,
											(void*)global_port_options_proc, (void*)P_FIX_ROUND_ROBIN_RINGS,
                      0,            0
  },
  {"DpcLimiterChannels",MI_UINT,      MI_WRITE,          0,
											sizeof(diva_tty_dpc_scheduler_protection_channel_count),
											&diva_tty_dpc_scheduler_protection_channel_count,0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

/*
	Retrieve information about specific call filter
	*/
static void *filter_info (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	int instance = (int)(long)context[1].fPara;
	int start    = (int)(long)context[2].fPara; /* start of this directory */
	void* ret = 0;
	int filter_nr;
	static char tmp[ISDN_MAX_NUMBER];

	if (instance >= TTYS_PER_DIRECTORY) {
		/*
			Only TTYS_PER_DIRECTORY entries in every directory
			*/
		return (0);
	}

	instance += start;
	filter_nr = instance+1;

	if (instance < MAX_CALL_FILTER_ENTRIES) {
		if (cmd == CMD_READVAR) {
			switch (*(char*)context->fPara) {
				case 1:
					pM->dw_out = (dword)filter_nr;
					ret = &pM->dw_out;
					break;
				case 2: /* Number */
					strcpy (tmp, diva_tty_call_filter.entry[instance].number);
					ret = &tmp[0];
					break;
				case 3: { /* Protocol */
					const char* name = diva_tty_isdn_get_protocol_name (diva_tty_call_filter.entry[instance].prot);
					if (!name) {
						diva_tty_call_filter.entry[instance].prot = 0;
						name = diva_tty_isdn_get_protocol_name (diva_tty_call_filter.entry[instance].prot);
					}
					str_cpy (tmp, name);
					ret = &tmp[0];
				} break;
			}
		} else if (cmd == CMD_WRITEVAR) {
			switch (*(char*)context->fPara) {
				case 2: { /* Number */
					int initial = (diva_tty_call_filter.entry[instance].number[0] != 0);

					str_cpy (&diva_tty_call_filter.entry[instance].number[0], (char*)info);
					if (!initial && diva_tty_call_filter.entry[instance].number[0]) {
						/* Add new filter */
						diva_tty_call_filter.n_entries++;
					} else if (initial && !diva_tty_call_filter.entry[instance].number[0] &&
										 (diva_tty_call_filter.n_entries > 0)) {
						/* Filter removed */
						diva_tty_call_filter.n_entries--;
					}
				} break;
				case 3: { /* Protocol */
					const char* name;
					int i;

					for (i = 0; (name = diva_tty_isdn_get_protocol_name (i)); i++) {
						if ((str_len((char*)name) == str_len((char*)info)) &&
								!mem_i_cmp ((char*)name, (char*)info, str_len ((char*)name))) {
							diva_tty_call_filter.entry[instance].prot = (byte)i;
							break;
						}
					}
				} break;
			}
		} else if (cmd == CMD_EXECUTE) {
			switch (*(char*)context->fPara) {
				case 4:
					if (diva_tty_call_filter.entry[instance].number[0]) {
						diva_tty_call_filter.entry[instance].number[0] = 0;
						if (diva_tty_call_filter.n_entries) {
							diva_tty_call_filter.n_entries--;
						}
					}
					diva_tty_call_filter.entry[instance].prot = 0;
					break;
			}
		}
	}

	return (ret);
}

/*
	TTY information directory
	*/
static MAN_INFO filter_info_dir[] =  {
  {"Nr",              MI_UINT,      0,                 MI_CALL,             4,
                      filter_info,  (void*)"\x01",
                      0,            0
  },
  {"Number",          MI_ASCIIZ,    MI_WRITE,          MI_CALL,             (ISDN_MAX_NUMBER-1),
                      filter_info,  (void*)"\x02",
                      0,            0
  },
  {"Protocol",        MI_ASCIIZ,    MI_WRITE,          MI_CALL,             24,
                      filter_info,  (void*)"\x03",
                      0,            0
  },
  {"Reset",           MI_EXECUTE,   0,                 0,                   0,
                      filter_info,  (void*)"\x04",
                      0,            0
  },
  {"Last",            MI_UINT,      0,                 MI_CALL|MI_E,        4,
                      diva_get_nothing,0,
                      0,            0
  }
};

/*
	View the FILTER directory for ttys: start <= filter < start+TTYS_PER_DIRECTORY
	The range of the instance parameter is independent, and also will vary
	in the range 0 ... MAX (where MAX >> TTYS_PER_DIRECTORY)
	*/
static void *filter_dir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	int instance = (int)(long)context->fPara;
	int filter_nr   = (int)(long)context[1].fPara; /* start of this directory */

	if (instance >= TTYS_PER_DIRECTORY) {
		/*
			Only TTYS_PER_DIRECTORY entries in every directory
			*/
		return (0);
	}

	filter_nr += instance;
	if (filter_nr < MAX_CALL_FILTER_ENTRIES) {
		if (cmd == CMD_NEXTNODE) {
			return (filter_info_dir);
		} else if (cmd == CMD_READVAR) {
			return (filter_info_dir);
		}
	}

	return (0);
}

static MAN_INFO filters[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"F",               MI_DIR,       0,                 MI_ENUM|MI_CALL|MI_E, 0,
                      filter_dir,   &mtpx_man_max_enums,
                      0,            0
  }
};

/*
	Display directory only in case filter_nr < total_filter_nr
	*/
static void *get_filters (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	int nr = (int)(long)context->fPara; /* start of this directory */

	if (nr < MAX_CALL_FILTER_ENTRIES) {
		if (cmd == CMD_NEXTNODE) {
			return (filters);
		} else if (cmd == CMD_READVAR) {
			return (filters);
		}
	}

	return (0);
}

#define FILTER_ENTRY(__name__,__start__) \
  {"F"__name__,       MI_DIR,       0,                 MI_CALL,               0, \
                      get_filters,     (void*)(__start__), \
                      0,            0 \
	}

/*
	Get the list of protocols available for call filter operation
	*/
static void *cf_get_available_protocols (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	static char tmp[512];

	if (cmd == CMD_READVAR) {
		const char* name;
		int i;

		for (i = 0, tmp[0] = 0; (name = diva_tty_isdn_get_protocol_name (i)); i++) {
			if (*name) {
				str_cpy (&tmp[str_len(tmp)], name);
				str_cpy (&tmp[str_len(tmp)], ", ");
			}
		}

		if ((i = str_len(&tmp[0])) >= 2) {
			tmp[i-1] = 0;
			tmp[i-2] = 0;
		}


		return (&tmp[0]);
	}

	return (0);
}

/*
	Clear/Reset entire call filter
	*/
static void* reset_filter_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	if (cmd == CMD_EXECUTE) {
		int i;

		diva_tty_call_filter.n_entries = 0;
		for (i = 0; i < MAX_CALL_FILTER_ENTRIES; i++) {
			diva_tty_call_filter.entry[i].number[0] = 0;
			diva_tty_call_filter.entry[i].prot      = 0;
		}
	}

	return (0);
}


/*
	Call filter directory
	*/
static MAN_INFO call_filter_dir[] = {
/* ---------------------------------------------------------------------------------- */
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */
/* ---------------------------------------------------------------------------------- */
  {"TotalFilters",    MI_UINT,      0,                 0,                    4,
											&n_call_filters,0,
                      0,            0
  },
  {"ActiveFilters",   MI_INT,       0,                 0,                     4,
											&diva_tty_call_filter.n_entries,0,
                      0,            0
  },
  {"AvailableProtocols",MI_ASCIIZ,  0,                 MI_CALL,               0,
											(void*)cf_get_available_protocols,0,
                      0,            0
  },

	FILTER_ENTRY("1-50",    0),
	FILTER_ENTRY("51-100",  50),
	FILTER_ENTRY("101-150", 100),
	FILTER_ENTRY("151-200", 150),


  {"Reset",           MI_EXECUTE,   0,                 0,                   0,
                      reset_filter_proc,0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

/* ----------------------------------------------------------------------------
		Fax speed statistics
   ---------------------------------------------------------------------------- */
static void* get_fax_speed (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	void* ret = 0;

	if (cmd == CMD_READVAR) {
		int speed  = (int)(long)context->fPara;
		pM->dw_out = diva_get_fax_speed_statistics_total (speed);
		ret = (speed < 0) ? &pM->dw_out : ((pM->dw_out != 0) ? &pM->dw_out : 0);
	}

	return (ret);
}

static void* get_fax_speed_relative (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	void* ret = 0;

	if (cmd == CMD_READVAR) {
		int speed  = (int)(long)context->fPara;
		if (speed < 0) {
			pM->dw_out = diva_get_fax_speed_statistics_total (speed);
			ret = &pM->dw_out;
		} else {
			pM->b_out = diva_get_fax_speed_statistics_percent ((word)speed);
			ret = (pM->b_out != 0) ? &pM->b_out : 0;
		}
	}

	return (ret);
}


static void* clear_fax_speed_stat (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	if (cmd == CMD_EXECUTE) {
		diva_clear_fax_speed_statistics ();
	}

	return (0);
}


#define FAX_SPEED_ENTRY(__x__, __v__) \
  {__x__,             MI_UINT,      0,                 MI_CALL,             4, \
                      get_fax_speed,  (void*)__v__, \
                      0,            0 \
  }

/*
	Fax speed statistics directory
	*/
static MAN_INFO fax_stat_speed_dir_total [] = {
	FAX_SPEED_ENTRY("Total", (-1)),
	FAX_SPEED_ENTRY("33600", 33600),
	FAX_SPEED_ENTRY("31200", 31200),
	FAX_SPEED_ENTRY("28800", 28800),
	FAX_SPEED_ENTRY("26400", 26400),
	FAX_SPEED_ENTRY("24000", 24000),
	FAX_SPEED_ENTRY("21600", 21600),
	FAX_SPEED_ENTRY("19200", 19200),
	FAX_SPEED_ENTRY("16800", 16800),
	FAX_SPEED_ENTRY("14400", 14400),
	FAX_SPEED_ENTRY("12000", 12000),
	FAX_SPEED_ENTRY("9600",  9600),
	FAX_SPEED_ENTRY("7200",  7200),
	FAX_SPEED_ENTRY("4800",  4800),
	FAX_SPEED_ENTRY("2400",  2400),
	FAX_SPEED_ENTRY("0", 0),

  {"Clear",           MI_EXECUTE,   0,                 0,                   0,
                      clear_fax_speed_stat, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

#define FAX_SPEED_RELATIVE_ENTRY(__x__, __v__) \
  {__x__" (%)",       MI_UINT,      0,                 MI_CALL,             1, \
                      get_fax_speed_relative,  (void*)__v__, \
                      0,            0 \
  }

static MAN_INFO fax_stat_speed_dir_relative [] = {
	FAX_SPEED_ENTRY("Total", (-1)),
	FAX_SPEED_RELATIVE_ENTRY("33600", 33600),
	FAX_SPEED_RELATIVE_ENTRY("31200", 31200),
	FAX_SPEED_RELATIVE_ENTRY("28800", 28800),
	FAX_SPEED_RELATIVE_ENTRY("26400", 26400),
	FAX_SPEED_RELATIVE_ENTRY("24000", 24000),
	FAX_SPEED_RELATIVE_ENTRY("21600", 21600),
	FAX_SPEED_RELATIVE_ENTRY("19200", 19200),
	FAX_SPEED_RELATIVE_ENTRY("16800", 16800),
	FAX_SPEED_RELATIVE_ENTRY("14400", 14400),
	FAX_SPEED_RELATIVE_ENTRY("12000", 12000),
	FAX_SPEED_RELATIVE_ENTRY("9600",  9600),
	FAX_SPEED_RELATIVE_ENTRY("7200",  7200),
	FAX_SPEED_RELATIVE_ENTRY("4800",  4800),
	FAX_SPEED_RELATIVE_ENTRY("2400",  2400),
	FAX_SPEED_RELATIVE_ENTRY("0", 0),

  {"Clear",           MI_EXECUTE,   0,                 0,                   0,
                      clear_fax_speed_stat, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

static MAN_INFO fax_stat_speed_dir [] = {
  {"Total",           MI_DIR,       0,                 0,                    0,
                      fax_stat_speed_dir_total, 0,
                      0,            0
  },
  {"Percent",         MI_DIR,       0,                 0,                    0,
                      fax_stat_speed_dir_relative, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

/* ----------------------------------------------------------------------------
		Fax error code statistics
   ---------------------------------------------------------------------------- */

static void* get_fax_fhng (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	void* ret = 0;

	if (cmd == CMD_READVAR) {
		word return_code  = (word)(dword)(unsigned long)context->fPara;
		pM->dw_out = diva_fax_get_fax_cls2_err_statistics_total ((word)return_code);
		ret = (pM->dw_out != 0) ? &pM->dw_out : 0;
	}

	return (ret);
}

#define FAX_FHNG_ENTRY(__x__, __v__) \
  {__x__,       MI_UINT,      0,                 MI_CALL,             4, \
                      get_fax_fhng,  (void*)__v__, \
                      0,            0 \
  }

static MAN_INFO fax_stat_fhng_dir_total [] = {
	FAX_SPEED_ENTRY("Total", (-1)),

	FAX_FHNG_ENTRY("0", 0),
	FAX_FHNG_ENTRY("1", 1),
	FAX_FHNG_ENTRY("2", 2),
	FAX_FHNG_ENTRY("3", 3),
	FAX_FHNG_ENTRY("4", 4),
	FAX_FHNG_ENTRY("5", 5),
	FAX_FHNG_ENTRY("6", 6),
	FAX_FHNG_ENTRY("7", 7),
	FAX_FHNG_ENTRY("8", 8),
	FAX_FHNG_ENTRY("9", 9),
	FAX_FHNG_ENTRY("10", 10),
	FAX_FHNG_ENTRY("11", 11),
	FAX_FHNG_ENTRY("12", 12),
	FAX_FHNG_ENTRY("13", 13),
	FAX_FHNG_ENTRY("14", 14),
	FAX_FHNG_ENTRY("15", 15),
	FAX_FHNG_ENTRY("16", 16),
	FAX_FHNG_ENTRY("17", 17),
	FAX_FHNG_ENTRY("18", 18),
	FAX_FHNG_ENTRY("19", 19),
	FAX_FHNG_ENTRY("20", 20),
	FAX_FHNG_ENTRY("21", 21),
	FAX_FHNG_ENTRY("22", 22),
	FAX_FHNG_ENTRY("23", 23),
	FAX_FHNG_ENTRY("24", 24),
	FAX_FHNG_ENTRY("25", 25),
	FAX_FHNG_ENTRY("26", 26),
	FAX_FHNG_ENTRY("27", 27),
	FAX_FHNG_ENTRY("28", 28),
	FAX_FHNG_ENTRY("29", 29),
	FAX_FHNG_ENTRY("30", 30),
	FAX_FHNG_ENTRY("31", 31),
	FAX_FHNG_ENTRY("32", 32),
	FAX_FHNG_ENTRY("33", 33),
	FAX_FHNG_ENTRY("34", 34),
	FAX_FHNG_ENTRY("35", 35),
	FAX_FHNG_ENTRY("36", 36),
	FAX_FHNG_ENTRY("37", 37),
	FAX_FHNG_ENTRY("38", 38),
	FAX_FHNG_ENTRY("39", 39),
	FAX_FHNG_ENTRY("40", 40),
	FAX_FHNG_ENTRY("41", 41),
	FAX_FHNG_ENTRY("42", 42),
	FAX_FHNG_ENTRY("43", 43),
	FAX_FHNG_ENTRY("44", 44),
	FAX_FHNG_ENTRY("45", 45),
	FAX_FHNG_ENTRY("46", 46),
	FAX_FHNG_ENTRY("47", 47),
	FAX_FHNG_ENTRY("48", 48),
	FAX_FHNG_ENTRY("49", 49),
	FAX_FHNG_ENTRY("50", 50),
	FAX_FHNG_ENTRY("51", 51),
	FAX_FHNG_ENTRY("52", 52),
	FAX_FHNG_ENTRY("53", 53),
	FAX_FHNG_ENTRY("54", 54),
	FAX_FHNG_ENTRY("55", 55),
	FAX_FHNG_ENTRY("56", 56),
	FAX_FHNG_ENTRY("57", 57),
	FAX_FHNG_ENTRY("58", 58),
	FAX_FHNG_ENTRY("59", 59),
	FAX_FHNG_ENTRY("60", 60),
	FAX_FHNG_ENTRY("61", 61),
	FAX_FHNG_ENTRY("62", 62),
	FAX_FHNG_ENTRY("63", 63),
	FAX_FHNG_ENTRY("64", 64),
	FAX_FHNG_ENTRY("65", 65),
	FAX_FHNG_ENTRY("66", 66),
	FAX_FHNG_ENTRY("67", 67),
	FAX_FHNG_ENTRY("68", 68),
	FAX_FHNG_ENTRY("69", 69),
	FAX_FHNG_ENTRY("70", 70),
	FAX_FHNG_ENTRY("71", 71),
	FAX_FHNG_ENTRY("72", 72),
	FAX_FHNG_ENTRY("73", 73),
	FAX_FHNG_ENTRY("74", 74),
	FAX_FHNG_ENTRY("75", 75),
	FAX_FHNG_ENTRY("76", 76),
	FAX_FHNG_ENTRY("77", 77),
	FAX_FHNG_ENTRY("78", 78),
	FAX_FHNG_ENTRY("79", 79),
	FAX_FHNG_ENTRY("80", 80),
	FAX_FHNG_ENTRY("81", 81),
	FAX_FHNG_ENTRY("82", 82),
	FAX_FHNG_ENTRY("83", 83),
	FAX_FHNG_ENTRY("84", 84),
	FAX_FHNG_ENTRY("85", 85),
	FAX_FHNG_ENTRY("86", 86),
	FAX_FHNG_ENTRY("87", 87),
	FAX_FHNG_ENTRY("88", 88),
	FAX_FHNG_ENTRY("89", 89),
	FAX_FHNG_ENTRY("90", 90),
	FAX_FHNG_ENTRY("91", 91),
	FAX_FHNG_ENTRY("92", 92),
	FAX_FHNG_ENTRY("93", 93),
	FAX_FHNG_ENTRY("94", 94),
	FAX_FHNG_ENTRY("95", 95),
	FAX_FHNG_ENTRY("96", 96),
	FAX_FHNG_ENTRY("97", 97),
	FAX_FHNG_ENTRY("98", 98),
	FAX_FHNG_ENTRY("99", 99),
	FAX_FHNG_ENTRY("100", 100),
	FAX_FHNG_ENTRY("101", 101),
	FAX_FHNG_ENTRY("102", 102),
	FAX_FHNG_ENTRY("103", 103),
	FAX_FHNG_ENTRY("104", 104),
	FAX_FHNG_ENTRY("105", 105),
	FAX_FHNG_ENTRY("106", 106),
	FAX_FHNG_ENTRY("107", 107),
	FAX_FHNG_ENTRY("108", 108),
	FAX_FHNG_ENTRY("109", 109),
	FAX_FHNG_ENTRY("110", 110),
	FAX_FHNG_ENTRY("111", 111),
	FAX_FHNG_ENTRY("112", 112),
	FAX_FHNG_ENTRY("113", 113),
	FAX_FHNG_ENTRY("114", 114),
	FAX_FHNG_ENTRY("115", 115),
	FAX_FHNG_ENTRY("116", 116),
	FAX_FHNG_ENTRY("117", 117),
	FAX_FHNG_ENTRY("118", 118),
	FAX_FHNG_ENTRY("119", 119),
	FAX_FHNG_ENTRY("unknown", 0xffff),

  {"Clear",           MI_EXECUTE,   0,                 0,                   0,
                      clear_fax_speed_stat, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};


static void* get_fax_fhng_relative (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
	diva_driver_management_t* pM = (diva_driver_management_t*)Ipar;
	void* ret = 0;

	if (cmd == CMD_READVAR) {
		word return_code  = (word)(dword)(unsigned long)context->fPara;
		pM->b_out = diva_fax_get_fax_cls2_err_statistics_percent ((word)return_code);
		ret = (pM->b_out != 0) ? &pM->b_out : 0;
	}

	return (ret);
}

#define FAX_FHNG_RELATIVE_ENTRY(__x__, __v__) \
  {__x__" (%)",       MI_UINT,      0,                 MI_CALL,             1, \
                      get_fax_fhng_relative,  (void*)__v__, \
                      0,            0 \
  }

static MAN_INFO fax_stat_fhng_dir_relative [] = {
	FAX_SPEED_ENTRY("Total", (-1)),

	FAX_FHNG_RELATIVE_ENTRY("0", 0),
	FAX_FHNG_RELATIVE_ENTRY("1", 1),
	FAX_FHNG_RELATIVE_ENTRY("2", 2),
	FAX_FHNG_RELATIVE_ENTRY("3", 3),
	FAX_FHNG_RELATIVE_ENTRY("4", 4),
	FAX_FHNG_RELATIVE_ENTRY("5", 5),
	FAX_FHNG_RELATIVE_ENTRY("6", 6),
	FAX_FHNG_RELATIVE_ENTRY("7", 7),
	FAX_FHNG_RELATIVE_ENTRY("8", 8),
	FAX_FHNG_RELATIVE_ENTRY("9", 9),
	FAX_FHNG_RELATIVE_ENTRY("10", 10),
	FAX_FHNG_RELATIVE_ENTRY("11", 11),
	FAX_FHNG_RELATIVE_ENTRY("12", 12),
	FAX_FHNG_RELATIVE_ENTRY("13", 13),
	FAX_FHNG_RELATIVE_ENTRY("14", 14),
	FAX_FHNG_RELATIVE_ENTRY("15", 15),
	FAX_FHNG_RELATIVE_ENTRY("16", 16),
	FAX_FHNG_RELATIVE_ENTRY("17", 17),
	FAX_FHNG_RELATIVE_ENTRY("18", 18),
	FAX_FHNG_RELATIVE_ENTRY("19", 19),
	FAX_FHNG_RELATIVE_ENTRY("20", 20),
	FAX_FHNG_RELATIVE_ENTRY("21", 21),
	FAX_FHNG_RELATIVE_ENTRY("22", 22),
	FAX_FHNG_RELATIVE_ENTRY("23", 23),
	FAX_FHNG_RELATIVE_ENTRY("24", 24),
	FAX_FHNG_RELATIVE_ENTRY("25", 25),
	FAX_FHNG_RELATIVE_ENTRY("26", 26),
	FAX_FHNG_RELATIVE_ENTRY("27", 27),
	FAX_FHNG_RELATIVE_ENTRY("28", 28),
	FAX_FHNG_RELATIVE_ENTRY("29", 29),
	FAX_FHNG_RELATIVE_ENTRY("30", 30),
	FAX_FHNG_RELATIVE_ENTRY("31", 31),
	FAX_FHNG_RELATIVE_ENTRY("32", 32),
	FAX_FHNG_RELATIVE_ENTRY("33", 33),
	FAX_FHNG_RELATIVE_ENTRY("34", 34),
	FAX_FHNG_RELATIVE_ENTRY("35", 35),
	FAX_FHNG_RELATIVE_ENTRY("36", 36),
	FAX_FHNG_RELATIVE_ENTRY("37", 37),
	FAX_FHNG_RELATIVE_ENTRY("38", 38),
	FAX_FHNG_RELATIVE_ENTRY("39", 39),
	FAX_FHNG_RELATIVE_ENTRY("40", 40),
	FAX_FHNG_RELATIVE_ENTRY("41", 41),
	FAX_FHNG_RELATIVE_ENTRY("42", 42),
	FAX_FHNG_RELATIVE_ENTRY("43", 43),
	FAX_FHNG_RELATIVE_ENTRY("44", 44),
	FAX_FHNG_RELATIVE_ENTRY("45", 45),
	FAX_FHNG_RELATIVE_ENTRY("46", 46),
	FAX_FHNG_RELATIVE_ENTRY("47", 47),
	FAX_FHNG_RELATIVE_ENTRY("48", 48),
	FAX_FHNG_RELATIVE_ENTRY("49", 49),
	FAX_FHNG_RELATIVE_ENTRY("50", 50),
	FAX_FHNG_RELATIVE_ENTRY("51", 51),
	FAX_FHNG_RELATIVE_ENTRY("52", 52),
	FAX_FHNG_RELATIVE_ENTRY("53", 53),
	FAX_FHNG_RELATIVE_ENTRY("54", 54),
	FAX_FHNG_RELATIVE_ENTRY("55", 55),
	FAX_FHNG_RELATIVE_ENTRY("56", 56),
	FAX_FHNG_RELATIVE_ENTRY("57", 57),
	FAX_FHNG_RELATIVE_ENTRY("58", 58),
	FAX_FHNG_RELATIVE_ENTRY("59", 59),
	FAX_FHNG_RELATIVE_ENTRY("60", 60),
	FAX_FHNG_RELATIVE_ENTRY("61", 61),
	FAX_FHNG_RELATIVE_ENTRY("62", 62),
	FAX_FHNG_RELATIVE_ENTRY("63", 63),
	FAX_FHNG_RELATIVE_ENTRY("64", 64),
	FAX_FHNG_RELATIVE_ENTRY("65", 65),
	FAX_FHNG_RELATIVE_ENTRY("66", 66),
	FAX_FHNG_RELATIVE_ENTRY("67", 67),
	FAX_FHNG_RELATIVE_ENTRY("68", 68),
	FAX_FHNG_RELATIVE_ENTRY("69", 69),
	FAX_FHNG_RELATIVE_ENTRY("70", 70),
	FAX_FHNG_RELATIVE_ENTRY("71", 71),
	FAX_FHNG_RELATIVE_ENTRY("72", 72),
	FAX_FHNG_RELATIVE_ENTRY("73", 73),
	FAX_FHNG_RELATIVE_ENTRY("74", 74),
	FAX_FHNG_RELATIVE_ENTRY("75", 75),
	FAX_FHNG_RELATIVE_ENTRY("76", 76),
	FAX_FHNG_RELATIVE_ENTRY("77", 77),
	FAX_FHNG_RELATIVE_ENTRY("78", 78),
	FAX_FHNG_RELATIVE_ENTRY("79", 79),
	FAX_FHNG_RELATIVE_ENTRY("80", 80),
	FAX_FHNG_RELATIVE_ENTRY("81", 81),
	FAX_FHNG_RELATIVE_ENTRY("82", 82),
	FAX_FHNG_RELATIVE_ENTRY("83", 83),
	FAX_FHNG_RELATIVE_ENTRY("84", 84),
	FAX_FHNG_RELATIVE_ENTRY("85", 85),
	FAX_FHNG_RELATIVE_ENTRY("86", 86),
	FAX_FHNG_RELATIVE_ENTRY("87", 87),
	FAX_FHNG_RELATIVE_ENTRY("88", 88),
	FAX_FHNG_RELATIVE_ENTRY("89", 89),
	FAX_FHNG_RELATIVE_ENTRY("90", 90),
	FAX_FHNG_RELATIVE_ENTRY("91", 91),
	FAX_FHNG_RELATIVE_ENTRY("92", 92),
	FAX_FHNG_RELATIVE_ENTRY("93", 93),
	FAX_FHNG_RELATIVE_ENTRY("94", 94),
	FAX_FHNG_RELATIVE_ENTRY("95", 95),
	FAX_FHNG_RELATIVE_ENTRY("96", 96),
	FAX_FHNG_RELATIVE_ENTRY("97", 97),
	FAX_FHNG_RELATIVE_ENTRY("98", 98),
	FAX_FHNG_RELATIVE_ENTRY("99", 99),
	FAX_FHNG_RELATIVE_ENTRY("100", 100),
	FAX_FHNG_RELATIVE_ENTRY("101", 101),
	FAX_FHNG_RELATIVE_ENTRY("102", 102),
	FAX_FHNG_RELATIVE_ENTRY("103", 103),
	FAX_FHNG_RELATIVE_ENTRY("104", 104),
	FAX_FHNG_RELATIVE_ENTRY("105", 105),
	FAX_FHNG_RELATIVE_ENTRY("106", 106),
	FAX_FHNG_RELATIVE_ENTRY("107", 107),
	FAX_FHNG_RELATIVE_ENTRY("108", 108),
	FAX_FHNG_RELATIVE_ENTRY("109", 109),
	FAX_FHNG_RELATIVE_ENTRY("110", 110),
	FAX_FHNG_RELATIVE_ENTRY("111", 111),
	FAX_FHNG_RELATIVE_ENTRY("112", 112),
	FAX_FHNG_RELATIVE_ENTRY("113", 113),
	FAX_FHNG_RELATIVE_ENTRY("114", 114),
	FAX_FHNG_RELATIVE_ENTRY("115", 115),
	FAX_FHNG_RELATIVE_ENTRY("116", 116),
	FAX_FHNG_RELATIVE_ENTRY("117", 117),
	FAX_FHNG_RELATIVE_ENTRY("118", 118),
	FAX_FHNG_RELATIVE_ENTRY("119", 119),
	FAX_FHNG_RELATIVE_ENTRY("unknown", 0xffff),

  {"Clear",           MI_EXECUTE,   0,                 0,                   0,
                      clear_fax_speed_stat, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};


static MAN_INFO fax_stat_fhng_dir [] = {
  {"Total",           MI_DIR,       0,                 0,                    0,
                      fax_stat_fhng_dir_total, 0,
                      0,            0
  },
  {"Percent",         MI_DIR,       0,                 0,                    0,
                      fax_stat_fhng_dir_relative, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

/*
	Fax statistics dir
	*/
static MAN_INFO fax_stat_dir [] = {
  {"Speed",           MI_DIR,       0,                 0,                    0,
                      fax_stat_speed_dir, 0,
                      0,            0
  },
  {"FHNG",            MI_DIR,       0,                 0,                    0,
                      fax_stat_fhng_dir, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

/*
	Statistics directory
	*/
static MAN_INFO statistics_dir [] = {
  {"FAX",             MI_DIR,       0,                 0,                    0,
                      fax_stat_dir, 0,
                      0,            0
  },
  {"Last",            MI_UINT,                0,       MI_CALL|MI_E,         1,
                      diva_get_nothing,       0,
                      0,            0
  }
};

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
  {"GlobalOptions",   MI_DIR,       0,                 0,                    0,
                      global_options_dir,0,
                      0,            0
  },
  {"CallFilter",      MI_DIR,       0,                 0,                    0,
                      call_filter_dir,0,
                      0,            0
  },
  {"Statistics",      MI_DIR,       0,                 0,                    0,
                      statistics_dir,0,
                      0,            0
  },
  {"TTY",             MI_DIR,       0,                 0,                    0,
                      ttys,         0,
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
	return ("TTY");

	diva_os_initialize_soft_isr (0, 0, 0);
	diva_os_remove_soft_isr (0);
	diva_os_initialize_spin_lock(0,0);
}

static const char* isdnStateName (byte State) {
	switch (State) {
		case C_DOWN:
			return ("C_DOWN");
		case C_INIT:
			return ("C_INIT");
		case C_IDLE:
			return ("C_IDLE");
		case C_CALL_DIAL:
			return ("C_CALL_DIAL");
		case C_CALL_OFFER:
			return ("C_CALL_OFFER");
		case C_CALL_ACCEPT:
			return ("C_CALL_ACCEPT");
		case C_CALL_ANSWER:
			return ("C_CALL_ANSWER");
		case C_CALL_UP:
			return ("C_CALL_UP");
		case C_CALL_DISC_X:
			return ("C_CALL_DISC_X");
		case C_DATA_CONN_X:
			return ("C_DATA_CONN_X");
		case C_DATA_WAIT:
			return ("C_DATA_WAIT");
		case C_DATA_XFER:
			return ("C_DATA_XFER");
	}

	return ("OTHER");
}
