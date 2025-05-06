
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

#ifndef __DIVA_GET_CFG_OPTIONS_H__
#define __DIVA_GET_CFG_OPTIONS_H__

struct _diva_xdi_um_cfg_cmd_data_init_vidi;

byte cfg_get_tei(void);
byte cfg_get_nt2(void);
byte cfg_get_didd_len(void);
byte cfg_get_watchdog(void);
byte cfg_get_permanent(void);
dword cfg_get_b_ch_mask(void);
byte cfg_get_stable_l2(void);
byte cfg_get_no_order_check(void);
byte cfg_get_low_channel(void);
byte cfg_get_prot_version(void);
byte cfg_get_crc4(void);
void cfg_get_spid (int nr, char* spid);
void cfg_get_spid_oad (int nr, char* oad);
void cfg_get_spid_osa (int nr, char* osa);
byte cfg_get_forced_law(void);
dword cfg_get_ModemGuardTone(void);
dword cfg_get_ModemMinSpeed(void);
dword cfg_get_ModemMaxSpeed(void);
dword cfg_get_ModemOptions(void);
dword cfg_get_ModemOptions2(void);
dword cfg_get_ModemNegotiationMode(void);
dword cfg_get_ModemModulationsMask(void);
dword cfg_get_ModemTransmitLevel(void);
dword cfg_get_FaxOptions(void);
dword cfg_get_FaxMaxSpeed(void);
byte cfg_get_Part68LevelLimiter(void);
byte cfg_get_L1TristateQSIG(void);
byte cfg_get_forced_law(void);
void cfg_adjust_rbs_options(void);
byte cfg_get_RingerTone(void);
byte cfg_get_UsEktsCachHandles(void);
byte cfg_get_UsEktsBeginConf(void);
byte cfg_get_UsEktsDropConf(void);
byte cfg_get_UsEktsCallTransfer(void);
byte cfg_get_UsEktsMWI(void);
dword cfg_get_card_bar (int bar);
byte cfg_get_fractional_flag (void);
byte cfg_get_QsigDialect (void);
byte cfg_get_interface_loopback (void);
int cfg_get_Diva4BRIDisableFPGA (void);
byte cfg_get_TxAttenuation (void);
byte cfg_get_UsDisableAutoSPID (void);
byte cfg_get_UsForceVoiceAlert (void);
byte cfg_get_ModemCarrierWaitTime (void);
byte cfg_get_ModemCarrierLossWaitTime (void);
byte cfg_get_PiafsRtfOff (void);
word cfg_get_QsigFeatures (void);
byte cfg_get_nosig(void);
byte cfg_get_BriLinkCount (void);
dword diva_cfg_get_serial_number (void);
int diva_cfg_get_set_vidi_state (dword state);
dword diva_cfg_get_vidi_mode (void);
int diva_cfg_get_vidi_info_struct (int task_nr, struct _diva_xdi_um_cfg_cmd_data_init_vidi* p);
int diva_cfg_get_hw_info_struct (char *buffer, int size);
dword cfg_get_SuppSrvFeatures (void);
dword cfg_get_R2Dialect (void);
byte cfg_get_R2CtryLength (void);
dword cfg_get_R2CasOptions (void);
dword cfg_get_V34FaxOptions (void);
dword cfg_get_DisabledDspsMask (void);
word cfg_get_AlertTimeout (void);
word cfg_get_ModemEyeSetup (void);
byte cfg_get_CCBSReleaseTimer (void);
byte cfg_get_AniDniLimiter_1(void);
byte cfg_get_AniDniLimiter_2(void);
byte cfg_get_AniDniLimiter_3(void);
byte cfg_get_DiscAfterProgress(void);

void diva_set_vidi_values (PISDN_ADAPTER IoAdapter, const struct _diva_xdi_um_cfg_cmd_data_init_vidi* info);

struct _diva_entity_queue;
int diva_cfg_lib_read_configuration (struct _diva_entity_queue* q);
const byte* diva_cfg_get_ram_init (dword* length);

extern dword diva_nr_li_descriptors;

#endif
