
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

#define CARDTYPE_H_WANT_DATA            1
#define CARDTYPE_H_WANT_IDI_DATA        0
#define CARDTYPE_H_WANT_RESOURCE_DATA   0
#define CARDTYPE_H_WANT_FILE_DATA       0
/* #define __DIVA_UM_CFG_4BRI_CHECK_FOR_DIFFERENT_IMAGES__ 1 */

#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/wait.h>

#include "platform.h"
#include <dimaint.h>
#include <cardtype.h>
#include "diva.h"
#include "pri.h"
#include "pri3.h"
#include "analog.h"
#include "s4bri.h"
#include "s4pri.h"
#include "sdp_hdr.h"
#include "bri.h"
#include "dlist.h"
#include "cfg_types.h"
#include "pc.h"
#include "di_defs.h"
#include "di.h"

#include <xdi_msg.h>
#include <io.h>
#include <mi_pc.h>

#include "os.h"
#include "diva_cfg.h"

#include <di_defs.h>
#include <dsp_defs.h>
#include <divasync.h>
#include <pc_maint.h>
#include <xdi_vers.h>
#include <pc.h>
#include <fpga_rom_xdi.h>

#include "cardmap.h"

int card_number  =  1;
int cfg_lib_preffered_card_number = -1;
int card_ordinal = -1;
int logical_adapter=0;
int logical_adapter_separate_config=0;
FILE* VisualStream;
FILE* DebugStream;
diva_entity_link_t* diva_cfg_adapter_handle = 0;


/*
  IMPORTS
  */
extern word xlog(byte *stream, void * buffer);

/*
  LOCALS
  */
#if !defined(DIVA_BUILD)
#define DIVA_BUILD "local"
#endif
static void usage(char *name);
static int diva_load_get_protocol_by_name (const char* name);
static void diva_load_print_supported_protocols (const char* tab);
static int diva_configure_card (int ordinal);
static int diva_configure_pri_rev_1_30m (int rev_2);
static int diva_configure_pri_v3(void);
static int diva_configure_pri_v3_passive (void);
static int diva_configure_pri_passive (int tasks);
static int diva_configure_4pri(int tasks);
static int diva_configure_4prie(int tasks);
static int diva_configure_4bri_rev_1_8 (int revision, int tasks);
static void do_dprintf (char* fmt, ...);
static void diva_fix_configuration (int instance);
static dword protocol_features2idi_features (dword protocol_features);
static void diva_cfg_check_obsolete_card_type_option (void);
static int diva_read_xlog (void);
static int diva_configure_bri_rev_1_2 (int revision);
static int diva_reset_card (void);
static int diva_create_core_dump_image (void);
static int diva_recovery_maint_buffer (void);
static int diva_card_monitor (void);
static const char* entry_name (const char* entry);
static int create_pid_file (int c, int create);
static void diva_monitor_hup (int sig);
static int execute_user_script (const char* application_name);
static int diva_card_test (dword test_command);
static int diva_configure_analog_adapter (int instances);
static int diva_cfg_get_logical_adapter_nr (const byte* src);
static int diva_load_fpga_image (const char* name);
static int diva_write_flash_image (const char* name, dword image_offset);
static int diva_update_flash_image (const char* name);
static int diva_read_flash_image (int image_nr);
static int diva_read_flash_id (void);
static int diva_read_flash_sn (void);
static int diva_write_flash_sn (dword sn);
static int diva_hotplug_lookup_card_number (int requestedNr, dword* serialNumber);
static int diva_card_map_sn (int requestedNr);
static int diva_card_get_cardType2HwChip (dword cardType);
static int diva_card_get_cardType2HwFeatures (dword cardType);
static int diva_card_get_cardType2Name (dword cardType);
static int diva_card_get_cardType2BusName (dword cardType);
static int diva_card_cardMap2Shell (void);

static volatile int diva_monitor_run;

dword diva_nr_li_descriptors = 0; /* 0 - full (default), 1 - do not alloc Li descriptors, 2 - reduced */

/*
  RBS defines
  */
#define DIVA_RBS_GLARE_RESOLVE_PATY_BIT       0x01
#define DIVA_RBS_DIRECT_INWARD_DIALING_BIT    0x02
#define DIVA_RBS_SAX_SAO_BIT                  0x40
#define DIVA_RBS_NO_ANSWER_SUPERVISION        0x80

typedef struct _diva_cmd_line_parameter {
  const char* ident;  /* parameter identifier           */
  int found;    /* internally used: parameter detected  */
  int string;   /* has a string type value              */
  int value;    /* any value required                   */
  int length;   /* length of string type value          */
  dword min;    /* minimum numeric value                */
  dword max;    /* maximum numeric value                */
  dword  vi;    /* internally used: numeric value)      */
  char vs[256]; /* internally used: string value        */
} diva_cmd_line_parameter_t;

diva_cmd_line_parameter_t parameters[] = {
/*                          found
                            |  string
                            |  |  value
                            |  |  |     length
                            |  |  |     |  min
                            |  |  |     |  |           max
                            |  |  |     |  |           |  vi
                            |  |  |     |  |           |  |  vs */
{ "c",                      0, 0, 1,    0, 1,       1024, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsAnswerDelay",         0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsAnswerDelay1",        0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsAnswerDelay2",        0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsAnswerDelay3",        0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsBearerCap",           0, 0, 1,    0, 0,        255, 0, {0, 0}},
{ "RbsBearerCap1",          0, 0, 1,    0, 0,        255, 0, {0, 0}},
{ "RbsBearerCap2",          0, 0, 1,    0, 0,        255, 0, {0, 0}},
{ "RbsBearerCap3",          0, 0, 1,    0, 0,        255, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsDigitTimeout",        0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsDigitTimeout1",       0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsDigitTimeout2",       0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "RbsDigitTimeout3",       0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsDID",                 0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsDID1",                0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsDID2",                0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsDID3",                0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsGlareResolve",        0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsGlareResolve1",       0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsGlareResolve2",       0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsGlareResolve3",       0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsTrunkMode",           0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsTrunkMode1",          0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsTrunkMode2",          0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsTrunkMode3",          0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsDialType",            0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsDialType1",           0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsDialType2",           0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "RbsDialType3",           0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsOfficeType",          0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsOfficeType1",         0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsOfficeType2",         0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsOfficeType3",         0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsAnswSw",              0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsAnswSw1",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsAnswSw2",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "RbsAnswSw3",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RbsDebug",               0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "RbsDebug1",              0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "RbsDebug2",              0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "RbsDebug3",              0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "RingerTone",             0, 0, 1,    0, 0,           1, 0, {0, 0}},
{ "RingerTone1",            0, 0, 1,    0, 0,           1, 0, {0, 0}},
{ "RingerTone2",            0, 0, 1,    0, 0,           1, 0, {0, 0}},
{ "RingerTone3",            0, 0, 1,    0, 0,           1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsEktsCachHandles",      0, 0, 1,    0, 0,          20, 0, {0, 0}},
{ "UsEktsCachHandles1",     0, 0, 1,    0, 0,          20, 0, {0, 0}},
{ "UsEktsCachHandles2",     0, 0, 1,    0, 0,          20, 0, {0, 0}},
{ "UsEktsCachHandles3",     0, 0, 1,    0, 0,          20, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsEktsBeginConf",        0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsBeginConf1",       0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsBeginConf2",       0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsBeginConf3",       0, 0, 1,    0, 0,         127, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsEktsDropConf",         0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsDropConf1",        0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsDropConf2",        0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsDropConf3",        0, 0, 1,    0, 0,         127, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsEktsCallTransfer",     0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsCallTransfer1",    0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsCallTransfer2",    0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsCallTransfer3",    0, 0, 1,    0, 0,         127, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsEktsMWI",              0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsMWI1",             0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsMWI2",             0, 0, 1,    0, 0,         127, 0, {0, 0}},
{ "UsEktsMWI3",             0, 0, 1,    0, 0,         127, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsForceVoiceAlert",      0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsForceVoiceAlert1",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsForceVoiceAlert2",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsForceVoiceAlert3",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "UsDisableAutoSPID",      0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsDisableAutoSPID1",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsDisableAutoSPID2",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
{ "UsDisableAutoSPID3",     0, 0, 0,    0, 0,           0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "Diva4BRIDisableFPGA",    0, 0, 0,    0, 0,           0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "BriLinkCount",           0, 0, 1,    0, 1,           8, 0, {0, 0}},
{ "BriLinkCount1",          0, 0, 1,    0, 1,           8, 0, {0, 0}},
{ "BriLinkCount2",          0, 0, 1,    0, 1,           8, 0, {0, 0}},
{ "BriLinkCount3",          0, 0, 1,    0, 1,           8, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "?",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "-version",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "h",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "-help",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "m",                      0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "m1",                     0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "m2",                     0, 0, 1,    0, 0,          2, 0, {0, 0}},
{ "m3",                     0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "e",                      0, 0, 1,    0, 1,          3, 0, {0, 0}},
{ "e1",                     0, 0, 1,    0, 1,          3, 0, {0, 0}},
{ "e2",                     0, 0, 1,    0, 1,          3, 0, {0, 0}},
{ "e3",                     0, 0, 1,    0, 1,          3, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "x",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "x1",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "x2",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "x3",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "z",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "z1",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "z2",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "z3",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "Monitor",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "Monitor1",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "Monitor2",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "Monitor3",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "l",                      0, 0, 1,    0, 1,         30, 0, {0, 0}},
{ "l1",                     0, 0, 1,    0, 1,         30, 0, {0, 0}},
{ "l2",                     0, 0, 1,    0, 1,         30, 0, {0, 0}},
{ "l3",                     0, 0, 1,    0, 1,         30, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"Fractional",              0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Fractional1",             0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Fractional2",             0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Fractional3",             0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "q",                      0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "q1",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "q2",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "q3",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "QsigDialect",            0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "QsigDialect1",           0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "QsigDialect2",           0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "QsigDialect3",           0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "QsigFeatures",           0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "QsigFeatures1",          0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "QsigFeatures2",          0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "QsigFeatures3",          0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "SuppSrvFeatures",        0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "SuppSrvFeatures1",       0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "SuppSrvFeatures2",       0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "SuppSrvFeatures3",       0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "R2Dialect",              0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "R2Dialect1",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "R2Dialect2",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "R2Dialect3",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "R2CtryLength",           0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "R2CtryLength1",          0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "R2CtryLength2",          0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "R2CtryLength3",          0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "R2CasOptions",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "R2CasOptions1",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "R2CasOptions2",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "R2CasOptions3",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "V34FaxOptions",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "V34FaxOptions1",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "V34FaxOptions2",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "V34FaxOptions3",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "DisabledDspsMask",       0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "DisabledDspsMask1",      0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "DisabledDspsMask2",      0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{ "DisabledDspsMask3",      0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "AlertTimeout",           0, 0, 1,    0, 0,  0xffff*20, 0, {0, 0}},
{ "AlertTimeout1",          0, 0, 1,    0, 0,  0xffff*20, 0, {0, 0}},
{ "AlertTimeout2",          0, 0, 1,    0, 0,  0xffff*20, 0, {0, 0}},
{ "AlertTimeout3",          0, 0, 1,    0, 0,  0xffff*20, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ModemEyeSetup",          0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "ModemEyeSetup1",         0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "ModemEyeSetup2",         0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
{ "ModemEyeSetup3",         0, 0, 1,    0, 0,     0xffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "CCBSReleaseTimer",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "CCBSReleaseTimer1",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "CCBSReleaseTimer2",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "CCBSReleaseTimer3",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "DisableDiscAfterProgress",     0, 0, 0,    0, 0,    0, 0, {0, 0}},
{ "DisableDiscAfterProgress1",    0, 0, 0,    0, 0,    0, 0, {0, 0}},
{ "DisableDiscAfterProgress2",    0, 0, 0,    0, 0,    0, 0, {0, 0}},
{ "DisableDiscAfterProgress3",    0, 0, 0,    0, 0,    0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "AniDniLimiterOne",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterOne1",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterOne2",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterOne3",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "AniDniLimiterTwo",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterTwo1",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterTwo2",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterTwo3",      0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "AniDniLimiterThree",     0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterThree1",    0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterThree2",    0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{ "AniDniLimiterThree3",    0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ChiFormat",              0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "ChiFormat1",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "ChiFormat2",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
{ "ChiFormat3",             0, 0, 1,    0, 0,          1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "n",                      0, 0, 1,    0, 0,         20, 0, {0, 0}},
{ "n1",                     0, 0, 1,    0, 0,         20, 0, {0, 0}},
{ "n2",                     0, 0, 1,    0, 0,         20, 0, {0, 0}},
{ "n3",                     0, 0, 1,    0, 0,         20, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "o",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "o1",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "o2",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "o3",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "p",                      0, 0, 1,    0, 1,          2, 0, {0, 0}},
{ "p1",                     0, 0, 1,    0, 1,          2, 0, {0, 0}},
{ "p2",                     0, 0, 1,    0, 1,          2, 0, {0, 0}},
{ "p3",                     0, 0, 1,    0, 1,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "BChMask",                0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "s",                      0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "s1",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "s2",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
{ "s3",                     0, 0, 1,    0, 0,          3, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "nosig",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "nosig1",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "nosig2",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "nosig3",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "t",                      0, 0, 1,    0, 0,         63, 0, {0, 0}},
{ "t1",                     0, 0, 1,    0, 0,         63, 0, {0, 0}},
{ "t2",                     0, 0, 1,    0, 0,         63, 0, {0, 0}},
{ "t3",                     0, 0, 1,    0, 0,         63, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "u",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "u1",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "u2",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{ "u3",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "f",                      0, 1, 1,   16, 0,          0, 0, {0, 0}},
{ "f1",                     0, 1, 1,   16, 0,          0, 0, {0, 0}},
{ "f2",                     0, 1, 1,   16, 0,          0, 0, {0, 0}},
{ "f3",                     0, 1, 1,   16, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "1spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "2spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "3spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "4spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "5spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "6spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "7spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "8spid",                  0, 1, 1,   59, 0,          0, 0, {0, 0}},

{ "1oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "2oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "3oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "4oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "5oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "6oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "7oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "8oad",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},

{ "1osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "2osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "3osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "4osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "5osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "6osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "7osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
{ "8osa",                   0, 1, 1,   59, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ProtVersion",            0, 0, 1,    0, 0,        254, 0, {0, 0}},
{ "ProtVersion1",           0, 0, 1,    0, 0,        254, 0, {0, 0}},
{ "ProtVersion2",           0, 0, 1,    0, 0,        254, 0, {0, 0}},
{ "ProtVersion3",           0, 0, 1,    0, 0,        254, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemGuardTone",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemGuardTone1",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemGuardTone2",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemGuardTone3",         0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemMinSpeed",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMinSpeed1",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMinSpeed2",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMinSpeed3",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemMaxSpeed",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMaxSpeed1",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMaxSpeed2",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemMaxSpeed3",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemOptions",            0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions1",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions2",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions3",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemOptions2",           0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions21",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions22",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemOptions23",          0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemNegotiationMode",    0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemNegotiationMode1",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemNegotiationMode2",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemNegotiationMode3",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemModulationsMask",    0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemModulationsMask1",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemModulationsMask2",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemModulationsMask3",   0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemTransmitLevel",      0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemTransmitLevel1",     0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemTransmitLevel2",     0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"ModemTransmitLevel3",     0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemCarrierWait",        0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierWait1",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierWait2",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierWait3",       0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"ModemCarrierLossWait",    0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierLossWait1",   0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierLossWait2",   0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"ModemCarrierLossWait3",   0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"FaxOptions",              0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxOptions1",             0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxOptions2",             0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxOptions3",             0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"FaxMaxSpeed",             0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxMaxSpeed1",            0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxMaxSpeed2",            0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
{"FaxMaxSpeed3",            0, 0, 1,    0, 0, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"Part68Lim",               0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Part68Lim1",              0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Part68Lim2",              0, 0, 1,    0, 0,          2, 0, {0, 0}},
{"Part68Lim3",              0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"PiafsRtfOff",             0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"PiafsRtfOff1",            0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"PiafsRtfOff2",            0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
{"PiafsRtfOff3",            0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"StopCard",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"InterfaceLoopback",       0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"InterfaceLoopback1",      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"InterfaceLoopback2",      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"InterfaceLoopback3",      0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardInfo",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"SerialNumber",            0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"Debug",                   0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardState",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"SoftwareFeatures",        0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardTest",                0, 0, 1,    0, 1, 0xffffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"Silent",                  0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"UseCardMap",              0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardMapSn",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardMap2Shell",           0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardType2CardName",       0, 0, 1,    0, 1,   CARDTYPE_MAX-1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardType2BusName",        0, 0, 1,    0, 1,   CARDTYPE_MAX-1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardType2HwChip",         0, 0, 1,    0, 1,   CARDTYPE_MAX-1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardType2HwFeatures",     0, 0, 1,    0, 1,   CARDTYPE_MAX-1, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardName",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardOrdinal",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{"CardPciRevision",         0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "TxAttenuation",          0, 0, 1,    0, 0,          2, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "CfgLib",                 0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "PlxRead",                0, 0, 1,    0, 0,       0xff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "IoRead",                 0, 0, 1,    0, 0,   0xffffff, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "PlxWrite",               0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "IoWrite",                0, 1, 1,  255, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ClockInterruptControl",  0, 0, 1,    0, 0,          4, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{ "ClockInterruptData",     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */

/*
  Obsolete card type option to be compatible with older
  install wizards
  */
{"y1",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y2",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y3",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y4",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y5",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y6",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y7",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y8",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y9",                      0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y10",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y11",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y12",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y13",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y14",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y15",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y16",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y17",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y18",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y19",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y20",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y21",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y22",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y23",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y24",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y25",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y26",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y27",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y28",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y29",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y30",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y31",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"y32",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"ReadXlog",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"CardMonitor",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"FlushXlog",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"ResetCard",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"CoreDump",                0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"DumpMaint",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"LoadFPGA",                0, 1, 1,  255, 0,          0, 0, {0, 0}},
{"WriteCUSTOM",              0, 1, 1,  255, 0,          0, 0, {0, 0}},
{"UpdateCUSTOM",             0, 1, 1,  255, 0,          0, 0, {0, 0}},
{"ReadCUSTOM",               0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"BackupCUSTOM",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"ReadCUSTOMID",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"ReadCUSTOMSN",             0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"WriteCUSTOMSN",            0, 0, 1,    0, 1, 0xfffffffe, 0, {0, 0}},
{"NoVerifyOnCUSTOMWrite",    0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"SeparateConfig",          0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"File",                    0, 1, 1,  255, 0,          0, 0, {0, 0}},
{"vb6",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
{"vd6",                     0, 0, 0,    0, 0,          0, 0, {0, 0}},
/* ------------------------------------------------------------------------ */
{  0 , 0, 0, 0, 0, 0,    0, 0, {0, 0}}
};

diva_cmd_line_parameter_t* find_entry (const char* c) {
  int i;

  for (i = 0; parameters[i].ident; i++) {
    if (!strcmp(parameters[i].ident, c)) {
      return (&parameters[i]);
    }
  }

  return (0);
}

static int diva_process_cmd_line (int argc, char** argv) {
  int i;
  char* p;
  diva_cmd_line_parameter_t* e;
  diva_cmd_line_parameter_t* silent = find_entry("Silent");


  for (i = 0; i < argc; i++) {
    p = argv[i];
    if (*p++ == '-') {
      if (!(e = find_entry (p))) {
        if (!silent->found) {
          fprintf (VisualStream, "A: invalid parameter: (%s)\n", p);
        }
        exit (1);
      }
      if (e->found) {
        if (!silent->found) {
          fprintf (VisualStream, "A: dublicate parameter: (%s)\n", p);
        }
        exit (1);
      }

      e->found = 1;
      if (!e->value) {
        continue;
      }
      if (((i+1) >= argc) || (argv[i+1][0] == '-')) {
        if (!silent->found) {
          fprintf (VisualStream, "A: value missing: (%s)\n", e->ident);
        }
        exit (1);
      }
      p = &argv[i+1][0];
      if (e->string) {
        int len = strlen (p);
        if (len >= e->length) {
          if (!silent->found) {
            fprintf (VisualStream, "A: value too long: (%s)\n", e->ident);
          }
          exit (1);
        }
        strcpy (&e->vs[0], p);
      } else {
        e->vi = strtoul(p, 0, 0);
        if ((e->vi > e->max) || (e->vi < e->min)) {
          if (!silent->found) {
            fprintf (VisualStream, "A: invalid value (%s)\n", e->ident);
          }
          exit (1);
        }
      }
      i++;
    }
  }

  return (0);
}

int
divaload_main (int argc, char** argv)
{
  diva_cmd_line_parameter_t* e;
  char property[256], serial[64];

  __diva_dprintf = do_dprintf;
  VisualStream = stdout;

  diva_process_cmd_line (argc, argv);
  diva_cfg_check_obsolete_card_type_option ();

  e = find_entry ("Silent");
  if (e->found) {
    if (!(VisualStream = fopen ("/dev/null", "w"))) {
      return (1);
    }
  } else {
    VisualStream = stdout;
  }

  e = find_entry ("SeparateConfig");
  if (e->found) {
    logical_adapter_separate_config=1;
  }

  diva_fix_configuration (0);
  if (logical_adapter_separate_config) {
    int i;
    for (i = 1; i < 4; i++) {
      diva_fix_configuration (i);
    }
  }


  diva_init_named_table ();

  DebugStream  = VisualStream;

  if (argc < 2) {
    usage (argv[0]);
    return (2);
  }
  e = find_entry ("?");
  if (e->found) {
    usage(argv[0]);
    return (2);
  }
  e = find_entry ("h");
  if (e->found) {
    usage(argv[0]);
    return (2);
  }
  e = find_entry ("-help");
  if (e->found) {
    usage(argv[0]);
    return (0);
  }
  e = find_entry ("-version");
  if (e->found) {
    fprintf (VisualStream,
             "divaload, BUILD (%s[%s]-%s-%s)\n",
             DIVA_BUILD, diva_xdi_common_code_build, __DATE__, __TIME__);
    return (0);
  }


  e = find_entry ("CardType2HwChip");
  if (e->found != 0)
    return diva_card_get_cardType2HwChip (e->vi);

  e = find_entry ("CardType2HwFeatures");
  if (e->found != 0)
    return diva_card_get_cardType2HwFeatures (e->vi);

  e = find_entry ("CardType2CardName");
  if (e->found != 0)
    return diva_card_get_cardType2Name (e->vi);

  e = find_entry ("CardType2BusName");
  if (e->found != 0)
    return diva_card_get_cardType2BusName (e->vi);

  e = find_entry ("CardMap2Shell");
  if (e->found != 0)
    return diva_card_cardMap2Shell ();


  e = find_entry("Debug");
  if (e->found) {
    DebugStream = VisualStream;
  } else {
    if (!(DebugStream = fopen ("/dev/null", "w"))) {
      fprintf (VisualStream, "A: can't open '/dev/null' for write\n");
      return (1);
    }
  }

  /*
      Set Card Number
    */
  e = find_entry ("c");
  if (e->found) {
    card_number = e->vi;
    cfg_lib_preffered_card_number = card_number;
  }

  /*
    One existing card will always provide card ordinal
    */
  if (!find_entry ("CfgLib")->found) {
    if ((card_ordinal = divas_get_card (card_number)) < 0) {
      if ((card_ordinal = diva_hotplug_lookup_card_number (card_number, 0)) < 0) {
        fprintf (VisualStream,
                 "A: can't get card type for DIVA adapter number %d\n",
                 card_number);
        return (1);
      }
    }
  }

  e = find_entry ("ResetCard");
  if (e->found) {
    return (diva_reset_card());
  }

  e = find_entry ("CoreDump");
  if (e->found) {
    return (diva_create_core_dump_image ());
  }

  e = find_entry ("DumpMaint");
  if (e->found) {
    return (diva_recovery_maint_buffer ());
  }

  e = find_entry ("LoadFPGA");
  if (e->found != 0) {
    return (diva_load_fpga_image(e->vs));
  }

  e = find_entry ("WriteCUSTOM");
  if (e->found != 0) {
    return (diva_write_flash_image(e->vs, 0));
  }

  e = find_entry ("UpdateCUSTOM");
  if (e->found != 0) {
    return (diva_update_flash_image(e->vs));
  }

  e = find_entry ("ReadCUSTOM");
  if (e->found != 0) {
    return (diva_read_flash_image (0));
  }

  e = find_entry ("BackupCUSTOM");
  if (e->found != 0) {
    return (diva_read_flash_image (1));
  }

  e = find_entry ("ReadCUSTOMID");
  if (e->found != 0) {
    return (diva_read_flash_id ());
  }

  e = find_entry ("ReadCUSTOMSN");
  if (e->found != 0) {
    return (diva_read_flash_sn ());
  }

  e = find_entry ("WriteCUSTOMSN");
  if (e->found != 0) {
    return (diva_write_flash_sn (e->vi));
  }

  e = find_entry ("CardMonitor");
  if (e->found) {
    return (diva_card_monitor());
  }
  e = find_entry ("FlushXlog");
  if (e->found) {
    e = find_entry ("ReadXlog");
    e->found = 1;
  }
  e = find_entry ("ReadXlog");
  if (e->found) {
    return (diva_read_xlog ());
  }

  e = find_entry("CardName");
  if (e->found) {
    fprintf (VisualStream, "%s\n", CardProperties[card_ordinal].Name);
    return (0);
  }
  e = find_entry("CardOrdinal");
  if (e->found) {
    fprintf (VisualStream, "%d\n", card_ordinal);
    return (0);
  }

  e = find_entry("CardPciRevision");
  if (e->found) {
    fprintf (VisualStream, "%d\n", divas_get_revision_id(card_number));
    return 0;
  }

  if (!find_entry ("CfgLib")->found) {
    if (divas_get_card_properties (card_number,
                                   "serial number",
                                   serial,
                                   sizeof(serial)) <= 0) {
      strcpy (serial, "0");
    }
  }

  /*
    Stop Card
    */
  e = find_entry ("StopCard");
  if (e->found) {
    fprintf (VisualStream,
             "Stop adapter Nr:%d - '%s', SN: %s ...",
             card_number,
             CardProperties[card_ordinal].Name,
             serial);
    fflush (VisualStream);
    if (divas_stop_adapter (card_number)) {
      if (divas_get_card_properties (card_number,
                                     "card state",
                                     property,
                                     sizeof(property)) <= 0) {
        strcpy (property, "unknown");
      }
      fprintf (VisualStream, " FAILED\nAdapter state is: %s\n", property);
      return (1);
    }
    fprintf (VisualStream, " OK\n");
    return (0);
  }

  e = find_entry ("SerialNumber");
  if (e->found) {
    dword sn;
    if (diva_card_map_sn (card_number) == 0 &&
        divas_get_card_properties (card_number,
                                   "serial number",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    } else if (diva_hotplug_lookup_card_number(card_number, &sn) > 0) {
      fprintf (VisualStream, "%d\n", sn);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }

  e = find_entry ("CardInfo");
  if (e->found) {
    if (divas_get_card_properties (card_number,
                                   "pci hw config",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }

  e = find_entry ("PlxRead");
  if (e->found != 0) {
    snprintf (property, sizeof(property), "%d", e->vi);
    if (divas_get_card_properties (card_number,
                                   "plx_read",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }

  e = find_entry ("IoRead");
  if (e->found != 0) {
    snprintf (property, sizeof(property), "%d", e->vi);
    if (divas_get_card_properties (card_number,
                                   "io_read",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }

  e = find_entry ("IoWrite");
  if (e->found != 0) {
    memcpy (property, e->vs, MIN(sizeof(property), sizeof(e->vs)));
    if (divas_get_card_properties (card_number,
                                   "io_write",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }


  e = find_entry ("PlxWrite");
  if (e->found != 0) {
    memcpy (property, e->vs, MIN(sizeof(property), sizeof(e->vs)));
    if (divas_get_card_properties (card_number,
                                   "plx_write",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "0\n");
    return (1);
  }

  e = find_entry ("ClockInterruptControl");
  if (e->found != 0) {
    snprintf (property, sizeof(property), "%d", e->vi);
    if (divas_get_card_properties (card_number,
                                   "clock_interrupt_control",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "FAILED\n");
    return (1);
  }
  e = find_entry ("ClockInterruptData");
  if (e->found != 0) {
    if (divas_get_card_properties (card_number,
                                   "clock_interrupt_data",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "FAILED\n");
    return (1);
  }

  e = find_entry ("CardTest");
  if (e->found) {
    if (diva_card_test (e->vi)) {
      return (1);
    }
    return (0);
  }

  e = find_entry ("CardState");
  if (e->found) {
    if (divas_get_card_properties (card_number,
                                   "card state",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "unknown\n");
    return (1);
  }

  e = find_entry ("SoftwareFeatures");
  if (e->found) {
    if (divas_get_card_properties (card_number,
                                   "software features",
                                   property,
                                   sizeof(property)) > 0) {
      fprintf (VisualStream, "%s\n", property);
      return (0);
    }
    fprintf (VisualStream, "%08x\n", 0);
    return (0);
  }

  if (find_entry ("CfgLib")->found) {
    /*
      Configuration using CfgLib
      */
    diva_entity_queue_t q;
    diva_entity_link_t* link;
    int ret = 0;

    diva_q_init (&q);
    if ((ret = diva_cfg_lib_read_configuration (&q)) < 0) {
      fprintf (VisualStream, "ERROR: CFGLib not available\n");
      return (1);
    }
    if (ret > 0) {

      link = diva_q_get_head (&q);
      while (link) {
        const byte* instance_data = (byte*)&link[1];
        dword adapters, adapter;
        int   logical_adapter_nr;
        dword adapter_disabled = 0;
        dword adapter_passive  = 0;
        dword nr_li_descriptors = 0;
        int hotplug_reserved = 0;

        diva_cfg_adapter_handle = link;

        logical_adapter=0;
        logical_adapter_separate_config=1;

        if ((logical_adapter_nr = diva_cfg_get_logical_adapter_nr (instance_data)) <= 0) {
          DBG_ERR(("Can not retrieve logical adapter number"))
          ret = 1;
          break;
        }
        if (diva_cfg_find_named_value (instance_data, (byte*)"adapters", 8, &adapters) || !adapters) {
          DBG_ERR(("Can not retrieve number of physical adapters"))
          ret = 1;
          break;
        }
        if (diva_cfg_find_named_value (instance_data, (byte*)"adapter", 7, &adapter)) {
          DBG_ERR(("Can not retrieve number of physical interface"))
          ret = 1;
          break;
        }

        (void)diva_cfg_find_named_value (instance_data, (byte*)"disabled", 8, &adapter_disabled);
        (void)diva_cfg_find_named_value (instance_data, (byte*)"passive", 7, &adapter_passive);
        (void)diva_cfg_find_named_value (instance_data, (byte*)"NoLiExports", sizeof("NoLiExports")-1, &nr_li_descriptors);

        card_number = logical_adapter_nr;

        DBG_TRC(("Logical Adapter Nr: %d, adapters:%lu, adapter:%lu LiDescriptors:%u",
                  logical_adapter_nr, adapters, adapter, nr_li_descriptors))

        /*
          Retrieve adapter properties
          */
        if ((card_ordinal = divas_get_card (card_number)) < 0) {
          dword hotplug_sn;
          if ((card_ordinal = diva_hotplug_lookup_card_number (card_number, &hotplug_sn)) < 0) {
            fprintf (VisualStream, "A: can't get card type for DIVA adapter number %d\n", card_number);
            ret = 1;
            break;
          } else {
            hotplug_reserved = 1;
            snprintf(serial, sizeof(serial), "%u", hotplug_sn);
            serial[sizeof(serial)-1] = 0;
          }
        }
        if (hotplug_reserved == 0 &&
            divas_get_card_properties (card_number, "serial number", serial, sizeof(serial)) <= 0) {
          strcpy (serial, "0");
        }

        fprintf (VisualStream,
                 "Start adapter Nr:%d - '%s', SN: %s ...",
                 card_number,
                 CardProperties[card_ordinal].Name,
                 serial);
        fflush (VisualStream);

        if (hotplug_reserved == 0 && divas_get_card_properties (card_number, "card state", property, sizeof(property)) <= 0) {
          strcpy (property, "unknown");
          fprintf (VisualStream, " FAILED\nAdapter state is: %s\n", property);
          ret = 1;
          break;
        }
        if (hotplug_reserved != 0) {
          fprintf (VisualStream, " Not Plugged (HotPlug reserved)\n");
        } else if (!strcmp (property, "active")) {
          fprintf (VisualStream, " OK (already active)\n");
        } else if (!strcmp (property, "trapped")) {
          fprintf (VisualStream, " TRAPPED (out of service)\n");
        } else if (strcmp (property, "ready")) {
          fprintf (VisualStream, " FAILED\nAdapter state is: %s\n", property);
          ret = 1;
          break;
        } else {
          if (adapter_disabled != 0) {
            fprintf (VisualStream,   " DISABLED\n");
          } else if (cfg_lib_preffered_card_number < 0 || cfg_lib_preffered_card_number == card_number) {
            diva_nr_li_descriptors = nr_li_descriptors;
            if (diva_configure_card (card_ordinal)) {
              fprintf (VisualStream, " FAILED\n");
              ret = 3;
            } else {
              fprintf (VisualStream, " OK\n");
            }
          } else {
            fprintf (VisualStream,   " SKIP\n");
          }
        }

        adapters--;

        /*
          Move to next adapter
          */
        if (adapters != 0) {
          dword i;
          for (i = 0; ((i < adapters) && link); i++) {
            link = diva_q_get_next (link);
          }
        }

        link = diva_q_get_next (link);
      }

      ret = 0;
    } else {
      DBG_ERR(("Can not retrieve system configuration using CfgLib interface"))
    }

    fflush (VisualStream);
    while ((link = diva_q_get_head (&q))) {
      diva_q_remove   (&q, link);
      free (link);
    }

    return (ret);

  } else {
    fprintf (VisualStream,
             "Start adapter Nr:%d - '%s', SN: %s ...",
             card_number,
             CardProperties[card_ordinal].Name,
             serial);
    fflush (VisualStream);

    if (divas_get_card_properties (card_number,
                                   "card state",
                                   property,
                                   sizeof(property)) <= 0) {
      strcpy (property, "unknown");
    }
    if (strcmp (property, "ready")) {
      fprintf (VisualStream, " FAILED\nAdapter state is: %s\n", property);
      return (-1);
    }


    if (diva_configure_card (card_ordinal)) {
      fprintf (VisualStream, " FAILED\n");
      return (3);
    }
    fprintf (VisualStream, " OK\n");
  }

  return (0);
}

void usage_controller (void) {
  fprintf (VisualStream, " controller, mandatory:\n");
  fprintf (VisualStream, "  -c n:    select controller number n\n");
  fprintf (VisualStream, "           default: 1\n");
  fprintf (VisualStream, "\n");
}

void usage_protocol (void) {
  fprintf (VisualStream, " protocol, mandatory:\n");
  diva_load_print_supported_protocols ("  ");
  fprintf (VisualStream, "\n");
}


typedef struct {
  const char*     key;
  const char*     text[64];
} diva_protocol_options_t;

diva_protocol_options_t protocol_options [] = {
{"-m [0,1,2]", {"Force Law",
                "0 - default Law",
                "1 - force a-Law",
                "2 - force u-Law",
                0}
},
{"-e [1,2,3]", {"Set layer 1 framing on PRI adapter.",
                "1 - doubleframing (no CRC4)",
                "2 - multiframing (CRC4)",
                "3 - autodetection",
                0}
},
{"-x",          {"Set PRI adapter in NT mode.",
                 "Default mode is TE.",
                 0},
},
{"-z",          {"Set adapter in high impedance state",
                 "until first user application does request",
                 "the interface activation. Switch off receiver.",
                 0},
},
{"-Monitor",    {"Set adapter into monitor mode.",
                 "Switch off transmitter; receive only.",
                 0},
},
{"-l [1...30]", {"Set starting channel number on PRI adapter.",
                 "By default the allocation of channels is made",
                 "on a high-to-low basis.",
                 "By specifying -l you select a low-to-high",
                 "allocation policy (in addition).",
                 0}
},
{"-Fractional [0,1,2]",
                 {"Set PRI adapter in fractional/hyperchannel mode.",
                  "0 - normal operation mode (default)",
                  "1 - fractional operation mode",
                  "2 - hyperchannel mode",
                  0},
},
{"-q [0..3]",    {"Select QSIG options.",
                  "0 - CR and CHI 2 bytes long (default)",
                  "1 - CR 1 byte, CHI 2 bytes",
                  "2 - CR is 2 bytes, CHI is 1 byte",
                  "3 - CR and CHI 1 byte",
                  "(CR - Call Reference, CHI - B-channel Ident.)",
                  0}
},
{"-QsigDialect [0..2]",
                 {"Select QSIG dialect.",
                  "0 - Autodetect (default)",
                  "1 - ECMA-QSIG  (older QSIG dialect)",
                  "    PABX: Alcatel 4200, Alcatel 4400,",
                  "          Hicom 150,",
                  "          Hicom 300 protocol ECMAV1",
                  "2 - reserved for future extensions",
                  "3 - ISO-QSIG (currently used QSIG-DIALECT)",
                  "    PABX: Lucent Definity, Alcatel 4400",
                  "4 - reserved for future extensions",
                  "5 - reserved for future extensions",
                  "6 - ECMA-Ericsson: Based on ECMA plus Ericsson",
                  "    extensions for MWI (Message Waiting)",
                  "    PABX: Ericsson MD110 firmware BC10 and BC11",
                  "7 - ISO-Ericsson: Based on ISO plus Ericsson",
                  "    extensions for MWI (Message Waiting)",
                  "    PABX: Ericsson MD110 firmware BC11",
                  "8 - ECMA-Alcatel: Based on ECMA plus Alcatel",
                  "    extensions",
                  "9 - ISO-Alcatel: Based on ISO plus Alcatel",
                  "    extensions",
                  0}
},
{"-QsigFeatures 0xXXXX",
                 {"Set QSIG protocol features - bit mask.",
                  "0x0000 - default settings",
                  "0x1000 - charge request in SETUP",
                  "0x2000 - no RATE charge request",
                  "0x4000 - no INTERIM charge request",
                  "0x8000 - no FINAL charge request",
                  "0xE000 - will disable charge request",
                  "0x0800 - take last redirecting number",
                  "         (last DivertingLegacyInfo2)",
                  "         instead of first",
                  0}
},
{"-SuppSrvFeatures 0xXXXX",
                 {"Set signaling protocol features - bit mask.",
                  "0x00000000 - default settings",
                  "0x00000001 - use Dummy Call Reference Facilities",
                  "             for ETSI MWI. (Default configuration",
                  "             uses REGISTER Message)",
                  "0x00000002 - ECT-Auto: explicit ETSI Call Transfer",
                  "             with fallback to implicit",
                  "0x00000004 - ECT-Explicit: explicit ETSI Call Transfer",
                  "             without fallback",
                  0}
},
{"-R2Dialect [0,1]",
                 {"Select R2 protocol dialect.",
                  "0 - Use R2 CN1 (default)",
                  "1 - Use R2 INDIA",
                  0}
},
{"-R2CtryLength [0,255]",
                 {"Used to separate R2 called party number in",
                  "Routing Information (India)/Country Code (CN1) part",
                  "and user part. Provides number of digits that belongs",
                  "to the Routing Information/Country Code part",
                  0}
},
{"-R2CasOptions 0xXXXX",
                 {"Set R2 protocol options.",
                  "0x00000000 - default settings",
                  "0x00001000 - do not request CiPN from network",
                  "             Normally switch provides CPN/CiPN, where",
                  "             every number is delivered after separate",
                  "             request. This option disables request for CiPN.",
                  "             Applies only to India R2 dialect,",
                  "             and affects processing of incoming",
                  "             calls only.",
                  0}
},
{"-V34FaxOptions 0xXXXX",
                 {"Set V.34 Fax options.",
                  "0x00000000 - default settings",
                  "0x00010000 - disable Tx reduction",
                  "0x00020000 - disable precoding",
                  "0x00040000 - disable pre-emphasis",
                  "0x00080000 - disable shaping",
                  "0x00100000 - disable non-linear processing",
                  "0x00200000 - disable manual reduction",
                  "0x00400000 - disable 16 points training",
                  "0x01000000 - disable 2400 symbols",
                  "0x02000000 - disable 2743 symbols",
                  "0x04000000 - disable 2800 symbols",
                  "0x08000000 - disable 3000 symbols",
                  "0x10000000 - disable 3200 symbols",
                  "0x20000000 - disable 3429 symbols",
                  0}
},
{"-DisabledDspsMask 0xXXXX",
                 {"Bit mask:",
                  "BIT0  - If set, disables DSP1",
                  "...",
                  "...",
                  "BIT30 - If set, disables DSP31",
                  0}
},
{"-AlertTimeout [0...1310700]",
                 {"Alert timeout in millisecounds.",
                  "0 - default value in accordance with selected",
                  "    D-channel protocol",
                  0}
},
{"-ModemEyeSetup 0xXXXX",
                 {"Analog modem debug info mask.",
                  "- V.32",
                  "  0x0000 - display DSPA",
                  "  0x0001 - display echo canceller",
                  "  0x0002 - display DSPB",
                  "  0x0003 - display DSPA and DSPB",
                  "- V.34",
                  "  0x0000 - display Rx constellation, equalizer coefficients",
                  "           and status line",
                  "  0x0001 - display echo canceller coefficients and status line",
                  "  0x0002 - display line companding and status",
                  "  0x0003 - display probing spectrum and status",
                  "  0x0004 - display equalizer coefficients and TX status",
                  "  0x0005 - display equalizer coefficients and RX status",
                  "  0x0008 - display debug info level 1",
                  "  0x0010 - display Rx precoding level",
                  "  0x0020 - display after Rx compensation",
                  "  0x0040 - display free process cycles",
                  "  0x0050 - display round trip delay",
                  "  0x0070 - display trellis decision length",
                  "  0x0080 - display Tx training process",
                  "  0x0090 - display mean of absolute error",
                  "  0x00a0 - display remote INFO0",
                  "  0x00b0 - display Rx training process",
                  "  0x00c0 - display frequency offset",
                  "  0x00d0 - display free",
                  "  0x00e0 - display transmit buffering",
                  0}
},
{"-CCBSReleaseTimer [0...255]",
                 {"QSIG CCBS release timer, sec.",
                  "0 - default",
                  0}
},
{"-DisableDiscAfterProgress",
                 {"Do not send DISC after certain PROGRESS messages.",
                  0}
},
{"-ChiFormat [0,1]",
                 {"Channel identifier format.",
                  "0 - use timeslots (default)",
                  "1 - use logical channels",
                  0}
},
{"-Part68Lim [0,1,2]",
                 {"Set encoded analog level limiter for Part 68 compliance.",
                  "0 - operation dependence on national requirement",
                  "1 - force ON",
                  "2 - force OFF",
                  0},
},
{"-PiafsRtfOff [0...255]",
                 {"Set PIAFS link turnaround time offset in frames.",
                  "0 - default",
                  0},
},
{"-n [0...20]",   {"Select NT2 mode and default length of DIDN.",
                   "(DIDN - Direct Inward Dialing Number)",
                  0}
},
{"-o",           {"Turn off order checking of information elements.",
                  0}
},
{"-p [1,2]",      {"Establish a permanent connection.",
                  "(e.g. leased line configuration)",
                  "1 - TE <-> TE mode, structured line",
                  "2 - NT <-> TE mode, raw line",
                  0}
},
{"-BChMask 0xYYYYYYYY",
                 {"Default B-channel bit mask for",
                  "unchanneled protocols (E1UNCH/T1UNCH).",
                  "Expl: full E1: 0xfffffffe",
                  "      full T1: 0x00ffffff",
                  0}
},
{"-nosig",       {"Turning internal signaling protocol stack off,",
                  "allows usage of the external layer 3 signaling",
                  "protocol stacks.",
                  0}
},
{"-s [0,1,2,3]",  {"D-channel layer 2 activation policy on BRI adapter.",
                  "0 - on demand",
                  "1 - de-activation only on NT side; preferred; default",
                  "2 - always active",
                  "3 - always active, mode 2",
                   0},
},
{"-t [0...63]",   {"Specifies a fixed TEI value.",
                   "(Default is an automatic TEI assignment.)",
                   0},
},
{"-u",          {"Select point-to-point mode on BRI adapter.",
                 "Uses default TEI '0'",
                 "NT2 mode is on.",
                 0}
},
{"-[1...8]oad|osa|spid",
                  {"BRI adapter B-channel options.",
                   "Specify the originating address (OAD),",
                   "originating Sub-address (OSA) and/or",
                   "service profile identifier (SPID)",
                   "for each B-channel (1 or 2 by BRI, 1...8 by 4BRI).",
                   "Example: -1oad 123456 -1spid 1234560001",
                   0}
},
{"-d",          {"Display protocol options summary.", 0}},
#if 0
{"-w",          {"Select alternative protocol code directory.",
                 "Default directory: \""DATADIR"\"",
                 0}
},
#endif
{"-vb6",        {"Protocol code version for BRI rev.1 adapter",
                 "Without this option V4 version of protocol code",
                 "(with V.90 modem and fax support) is used.",
                 "With this option new version of protocol code",
                 "(with QSIG and V.90 modem support) is used.",
                 0}
},
{"-vd6",        {"DSP code version for BRI rev.1 adapter",
                 "Without this option, but with '-vb6' option",
                 "new version of protocol code with QSIG and",
                 "V.90 modem support is used.",
                 "With this option new version of protocol code",
                 "with QSIG, fax and voice support is used",
                 0}
},
{"-h or -?",    {"help", 0}},
{"--help",      {"help", 0}},
{"--version",   {"build version", 0}},
{"-ProtVersion", {"Set protocol version for US protocols - obsolete.",
                  0}
},
{"-ModemGuardTone",
                 {"Controls modem guard tone.",
                  "0x00 - no guard tone (default)",
                  "0x01 - 550Hz guard tone",
                  "0x03 - 1800Hz guard tone",
                  0}
},
{"-ModemMinSpeed",
                 {"Lower limit for speed of modem.",
                  "connections, bit/sec.",
                  "0 - no lower limit (default)",
                  0}
},
{"-ModemMaxSpeed",
                 {"Upper limit for speed of modem",
                  "connections, bit/sec.",
                  "0 - no upper limit (default)",
                  0}
},
{"-ModemOptions",{"Bitfield, controls modem options.",
                  "0x0000 - default",
                  "0x0001 - disable V.42 and V.42bis",
                  "0x0002 - disable MNP5",
                  "0x0004 - always require error correction",
                  "         (disconnect if error correction was",
                  "          not negoatioted)",
                  "0x0008 - disable V.42 detection",
                  "0x0010 - disable compression",
                  "0x0020 - require error correction if norm above",
                  "         of V.34",
                  "0x0100 - no error correction at 1200 Bps",
                  "0x0200 - save error correction negotiation data",
                  "0x0400 - disable selective reject",
                  "0x0800 - disable MNP3",
                  "0x1000 - disable MNP4",
                  "0x2000 - disable MNP10",
                  0}
},
{"-ModemOptions2",
                 {"Bitfield, controls modem options.",
                  "0x00000000 - default",
                  "0x00000001 - leased line mode",
                  "0x00000002 - 4 wire operation",
                  "0x00000004 - disable BUSY detection",
                  "0x00000008 - disable calling tone",
                  "0x00000010 - disable answer tone",
                  "0x00000020 - enable dial tone detection",
                  "0x00000040 - use pots interface",
                  "0x00000080 - force R.T. fax",
                  "0x00000100 - disable retrain",
                  "0x00000200 - disable step down",
                  "0x00000400 - disable split speed",
                  "0x00000800 - disable trellis",
                  "0x00001000 - allow RDL test loop",
                  "0x00008000 - reverse direction",
                  "0x00010000 - disable tx direction",
                  "0x00020000 - disable precoding",
                  "0x00040000 - disable pre-emphasis",
                  "0x00080000 - disable shaping",
                  "0x00100000 - disable non-linear processing",
                  "0x00200000 - disable manual reduction",
                  "0x00400000 - disable 16 point training",
                  "0x01000000 - disable 2400 symbols",
                  "0x02000000 - disable 2743 symbols",
                  "0x04000000 - disable 2800 symbols",
                  "0x08000000 - disable 3000 symbols",
                  "0x10000000 - disable 3200 symbols",
                  "0x20000000 - disable 3429 symbols",
                  0}
},
{"-ModemNegotiationMode",
                 {"Modem negotiation mode.",
                  "0x00 - negotiate highest (default)",
                  "0x01 - negotiation disabled",
                  "0x02 - negotiate in class",
                  "0x03 - negotiate V.100",
                  "0x04 - negotiate V.8",
                  "0x05 - negotiate V.8bis",
                  0}
},
{"-ModemModulationsMask",
                 {"Bitfield, controls modem modulation mask.",
                  "0x00000000 - default",
                  "0x00000001 - disable V.21",
                  "0x00000002 - disable V.23",
                  "0x00000004 - disable V.22",
                  "0x00000008 - disable V.22bis",
                  "0x00000010 - disable V.32",
                  "0x00000020 - disable V.32bis",
                  "0x00000040 - disable V.34",
                  "0x00000080 - disable V.90",
                  "0x00000100 - disable BELL103",
                  "0x00000200 - disable BELL212A",
                  "0x00000400 - disable VFC",
                  "0x00000800 - disable K56FLEX",
                  "0x00001000 - disable X2",
                  "0x00010000 - enable V.29 full duplex",
                  "0x00020000 - enable V.33",
                  0}
},
{"-ModemTransmitLevel",
                 {"Modem transmit level, 0 ... 15.",
                  "0  -   0 dBm",
                  "15 - -15 dBm",
                  0}
},
{"-ModemCarrierWait",
                 {"Modem carrier wait time 0 ... 255 sec.",
                  "0  - use default value (default)",
                  0}
},
{"-ModemCarrierLossWait",
                 {"Modem carrier loss wait time 0 ... 255 sec/10.",
                  "0  - use default value (default)",
                  0}
},
{"-FaxOptions",  {"Bitfield, controls fax options.",
                  "0x0000 - default",
                  "0x0001 - disable fine resolution",
                  "0x0002 - disable ECM",
                  "0x0004 - ECM 64 Bytes",
                  "0x0008 - disable 2D coding",
                  "0x0010 - disable T.6 coding",
                  "0x0020 - disable uncompressed",
                  "0x0040 - refuse polling",
                  "0x0080 - hide total pages",
                  "0x0100 - hide all headlines",
                  "0x0180 - hide page info",
                  "0x0200 - disable feature fallback",
                  "0x0800 - V.34 fax control rate 1200 Bps",
                  "0x1000 - disable V.34 fax",
                  0}
},
{"-FaxMaxSpeed", {"Maximum allowed fax transmission speed bit/sec.",
                  0}
},
{"-RbsAnswerDelay",
                  {"Used by RBS protocol.",
                   "Timeout, if no answer after dialing.",
                  0}
},
{"-RbsBearerCap", {"Used by RBS protocol.",
                   "Bearer capabilities used to indicate",
                   "incomming call to application",
                   "4 - Voice/Analog",
                   "8 - Data/Digital",
                  0}
},
{"-RbsDigitTimeout",
                  {"Used by RBS protocol.",
                   "Timeout between incoming digits",
                   "if direct inward dialing is used",
                  0}
},
{"-RbsDID",       {"Used by RBS protocol.",
                   "Direct Inward Dialing",
                   "0 - no (default)",
                   "1 - yes",
                  0}
},
{"-RbsGlareResolve",
                  {"Used by RBS protocol.",
                   "Glare Resolve Party",
                   "0 - no",
                   "1 - yes",
                  0}
},
{"-RbsTrunkMode", {"Used by RBS protocol.",
                   "0 - Wink Start (default)",
                   "1 - Loop Start",
                   "2 - Ground Start",
                   0},
},
{"-RbsDialType",  {"Used by RBS protocol.",
                   "0 - Pulse",
                   "1 - DTMF (default)",
                   "2 - MF",
                   0},
},
{"-RbsOfficeType",{"Used by RBS protocol.",
                   "0 - FSX/FXO (default)",
                   "1 - SAS/SAO",
                   0},
},
{"-RbsAnswSw",    {"Used by RBS protocol.",
                   "0 - Answer Supervision (default)",
                   "1 - No Answer Supervision",
                   0},
},
{"-RbsDebug",    {"Used by RBS protocol.",
                  "reserved",
                  0}
},
{"-AniDniLimiterOne 0xXX",
                 {"Used by RBS protocol.",
                  "First *ANI*DNI* delimiter value",
                   "0 - not used (default)",
                   "XX - character value to be used as delimiter",
                  0}
},
{"-AniDniLimiterTwo 0xXX",
                 {"Used by RBS protocol.",
                  "second *ANI*DNI* delimiter value",
                   "0 - not used (default)",
                   "XX - character value to be used as delimiter",
                  0}
},
{"-AniDniLimiterThree 0xXX",
                 {"Used by RBS protocol.",
                  "third *ANI*DNI* delimiter value",
                   "0 - not used (default)",
                   "XX - character value to be used as delimiter",
                  0}
},
{"-UsEktsCachHandles",
                 {"Amount of Call Appearances (Call References)",
                  "if CACH (Call Appearance Call Handling) is",
                  "active.",
                  "0 ... 20, default 0 (off)",
                  0}
},
{"-UsEktsBeginConf",
                 {"Set value of Begin Conference feature.",
                  "0 ... 127, default 61",
                  0}
},
{"-UsEktsDropConf",
                 {"Set value of Remove Conference feature.",
                  "0 ... 127, default 63",
                  0}
},
{"-UsEktsCallTransfer",
                 {"Set value of Call Transfer feature.",
                  "0 ... 127, default 62",
                  0}
},
{"-UsEktsMWI",
                 {"Set value of Message Waiting feature.",
                  "0 ... 127, defauly 0 (off)",
                  0}
},
{"-UsForceVoiceAlert",
                 {"Always ring on incoming voice calls.",
                  0}
},
{"-UsDisableAutoSPID",
                 {"Disable fallback to auto-SPID",
                  "procedure.",
                  0}
},
{"-BriLinkCount [1...8]",
                 {"By default BRI adapter supports in P2MP mode",
                  "only one layer 2 link (i.e. only one TEI).",
                  "This parameter allows you to increase the number",
                  "of supported layer 2 links.",
                  0}
},
{"-RingerTone",  {"Generate local tones in B-channel",
                  "(ALERT/BUSY), if tones are not provided",
                  "by ISDN equipment.",
                  "Active only in point to point mode",
                  "0 - off (default)",
                  "1 - on",
                  0}
},
{"-SeparateConfig",
                 {"Use separate configurations for ports",
                  "of 4BRI adapter.",
                  "In this case the port number should be selected",
                  "after every parameter. Example:",
                  "'-f ETSI -f1 1TR6 -f2 ETSI -f3 QSIG' - configure",
                  "port 1 and 3 for ETSI, port 2 for 1TR6 and port 4",
                  "for QSIG D-channel protocol",
                  0}
},
{"-StopCard",    {"Stop and reset selected adapter.",
                  0}
},
{"-CardInfo",    {"View hardware configuration of selected adapter.",
                  0}
},
{"-SerialNumber",{"View serial number of selected adapter.",
                  0}
},
{"-Debug",       {"View debug messages.",
                  0}
},
{"-Diva4BRIDisableFPGA",
                 {"Disable FPGA access on 4BRI adapters.",
                  0}
},
{"-CardState",   {"View state of selected adapter.",
                  "possible states:",
                  "ready          - adapter is ready for configuration",
                  "active         - adapter is running",
                  "trapped        - adapter is out of service due to",
                  "                 internal problem",
                  "out of service - adapter is out of service due to",
                  "                 hardware configuration problem",
                  "unknown        - adapter status is unknown due to",
                  "                 software problem",
                  0}
},
{"-SoftwareFeatures",
                 {"Read PCINIT_SOFTWARE_FEATURES",
                  0}
},
{"-UseCardMap 'type,sn,type,sn,...'",
                 {"Use card map",
                  0}
},
{"-CardMapSn",   {"Always use CardMap for serial number",
                  0}
},
{"-CardMap2Shell", {"Convert UseCardMap parameters to shell friendly format",
                    0}
},
{"-CardType2HwChip type", {"Convert card type to card's chip property",
                           0}
},
{"-CardType2HwFeatures type", {"Convert card type to card's hardware features property",
                               0}
},
{"-CardType2CardName type", {"Convert card type to card name string",
                             0}
},
{"-CardType2BusName type", {"Convert card type to card bus string",
                            0}
},
{"-CardTest 0xXXXXXXXX",
                 {"Test adapter hardware.",
                  "possible tests:",
                  "0x00000001     - quick memory test (~ 2 sec.)",
                  "0x00000002     - fast memory test (~ 3 sec.)",
                  "0x00000004     - medium memory test (~ 45 sec.)",
                  "0x00000008     - large memory test (~ 15 sec.)",
                  "0x00000100     - memory test using on board CPU",
                  "0x00000200     - endless test using on board CPU",
                  "0x00000400     - turn on board CPU cache on",
                  "0x00001000     - DMA channel 0 from board to host",
                  "0x00002000     - DMA channel 1 from board to host",
                  "0x00004000     - DMA channel 0 from host to board",
                  "0x00008000     - DMA channel 1 from host to board",
                  0},
},
{"-Silent",      {"Silent operation mode, do not display messages.",
                  0}
},
{"-CardName",    {"View card name.",
                  0}
},
{"-ReadXlog",    {"Read XLOG information from selected Diva Server adapter",
                  "until user terminates execution.",
                  "'-File' command line option can be used",
                  "to provide output file name.",
                  0}
},
{"-FlushXlog",   {"Read XLOG information from selected Diva Server adapter",
                  "and exit if no information available.",
                  "Exit status is zero in case XLOG traces was read.",
                  "'-File' command line option can be used",
                  "to provide output file name.",
                  0}
},
{"-CardMonitor", {"Start in background as daemon and use XLOG channel",
                  "to monitor state of selected Diva Server adapter.",
                  "If adapter fails, report to syslog and exit.",
                  "'-File' command line option can be used to provide",
                  "name of optional application to be executed on",
                  "adapter failure with following parameters:",
                  "Argument 1: adapter number",
                  0}
},
{"-File",        {"Write information to file instead of console",
                  "filename should be provided",
                  0}
},
{"-ResetCard",   {"Reset adapter to 'ready' state if adapter was trapped.",
                  0}
},
{"-CoreDump",    {"Read adapter sdram context.",
                  "'-File' command line option can be used",
                  "to provide output file name.",
                  "Default output file name is 'diva_core'",
                  0}
},
{"-DumpMaint",   {"Read adapter sdram context.",
                  "Recover debug/trace buffer of MAINT driver",
                  "if any can be found in the adapter memory",
                  "'-File' command line option can be used",
                  "to provide output file name.",
                  "Default output file name is 'trace.bin'",
                  0}
},
{"-LoadFPGA name",
                 {"Load adapter FPGA.",
                  "'name' provides FPGA image file name.",
                  0}
},
{"-TxAttenuation X",
                 {"Attenuate Transmit Signal (E1 and PRI V3 only)",
                  "X values: 0 -> 0dBm, 1 -> -7.5dBm, 2 -> -15dBm",
                  0}
},
{"-PlxRead offset",
                 {"Read PLX register",
                  "offset: PLX register offset",
                  "        0x00 ... 0xff",
                  0}
},
{"-IoRead offset",
                 {"Read IO devices",
                  "offset: register offset",
                  "        0x000000 ... 0xffffff",
                  0}
},
{"-PlxWrite offset:value[:width]",
                 {"Write PLX register",
                  "value:  PLX register value",
                  "        0x00000000 ... 0xffffffff",
                  "offset: PLX register offset",
                  "        0x00 ... 0xff",
                  "width:  PLX register width",
                  "        1, 2, 4",
                  "        default - 4",
                  0}
},
{"-IoWrite offset:value",
                 {"Write 32 bit value to IO device",
                  "value:  register value",
                  "        0x00000000 ... 0xffffffff",
                  "offset: register offset",
                  "        0x000000 ... 0xffffff",
                  0}
},
{"-ClockInterruptControl port",
                 {"Activate or deactivate clock interrupt",
                  "port: Select interrupt clock master",
                  "      0               - deactivate",
                  "      Any other value - use port as source for clock interrupt",
                  0}
},
{"-ClockInterruptData",
                 {"Get clock interrupt statistics",
                  0}
},
{ 0, {0}}
};

static void usage_option (const char* tab, const diva_protocol_options_t* opt);
static void usage_options (const char* tab) {
  const diva_protocol_options_t* opt = &protocol_options [0];

  fprintf (VisualStream, " options, optional:\n");

  while (opt->key) {
    usage_option (tab, opt++);
  }
  fprintf (VisualStream, "\n");
}

static void usage_option (const char* tab, const diva_protocol_options_t* opt) {
  char buffer[2048];
  int s, j, i;

  strcpy (buffer, tab);
  strcat (buffer, opt->key);
  s = strlen (buffer);
  while (s++ < 22) {
    strcat (buffer, " ");
  }
  j = strlen (buffer) + 2;
  strcat (buffer, ": ");
  strcat (buffer, opt->text[0]);
  fprintf (VisualStream, "%s\n", buffer);

  for (i = 1; (opt->text[i] && (i < 64)); i++) {
    buffer[0] = 0;
    s = 0;
    while (s++ < j) {
      strcat (buffer, " ");
    }
    strcat (buffer, opt->text[i]);
    fprintf (VisualStream, "%s\n", buffer);
  }
}

/* -----------------------------------------------------------------------
      Usage function
   ----------------------------------------------------------------------- */
static void usage(char *name) {
  char* p = strstr(name, "/");
  while (p) {
    name = p+1;
    p = strstr (name, "/");
  }

  fprintf(VisualStream, "Usage: %s controller protocol [options]\n\n", name);
  usage_controller ();
  usage_protocol();
  usage_options ("  ");
}

#define DMLT_PRI  0x01
#define DMLT_BRI  0x02
#define DMLT_4BRI 0x04

typedef struct  {
  byte pri;
  byte bri;
  dword multi;
  byte id;
  const char* name;
  const char*  image;
  const char*  description;
  const char*  secondary_dmlt_image;
} diva_dmlt_protocols_t;

diva_dmlt_protocols_t dmlt_protocols [] = {
/* pri, bri, dmlt, id name    image ; description, secondary_dmlt_image */
  {1,   1,   0,    0, "1TR6", "te_1tr6",
"Germany, old protocol for PABX",          0},
  {1,   1,   7,    1, "ETSI", "te_etsi",
"DSS1, Europe (Germany, ...)",             0},
  {1,   1,   7,    2, "FRANC", "te_etsi",
"VN3, France, old protocol for PABX",      "te_etsi"},
  {1,   1,   7,    3, "BELG",  "te_etsi",
"NET3, Belgium, old protocol for PABX",    "te_etsi"},
  {1,   0,   1,    4, "SWED",  "te_etsi",
"DSS1 with CRC4 off, Sweden, Benelux",     "te_etsi"},
  {1,   1,   7,    5, "NI",    "te_us",
"NI1, NI2, North America, National ISDN",  "te_us"},
  {1,   1,   7,    6, "5ESS",  "te_us",
"5ESS, North America, AT&T",               "te_us"},
  {1,   1,   7,    7, "JAPAN", "te_japan",
"Japan, INS-NET64",                        0},
  {1,   1,   7,    8, "ATEL",  "te_etsi",
"ATEL, Australia, old TPH1962",            "te_etsi"},
  {0,   1,   6,    9, "US",    "te_us",
"North America, Auto Detect"/* V.6 < 5ESS Lucent, National ISDN*/, 0},
  {1,   1,   7,    10, "ITALY", "te_etsi",
"DSS1, Italy",                             "te_etsi"},
  {1,   1,   7,    11, "TWAN",  "te_etsi",
"DSS1, Taiwan",                            "te_etsi"},
  {1,   1,   7,    12, "AUSTRAL", "te_etsi",
"Australia, Microlink (TPH1962), OnRamp ETSI", "te_etsi"},
  {1,   0,   1,    13, "4ESS_SDN", "te_us",
"4ESS Software Defined Network",                "te_us"},
  {1,   0,   1,    14, "4ESS_SDS", "te_us",
"4ESS Switched Digital Service",                "te_us"},
  {1,   0,   1,    15, "4ESS_LDS", "te_us",
"4ESS Long Distance Service",                   "te_us"},
  {1,   0,   1,    16, "4ESS_MGC", "te_us",
"4ESS Megacom",                                 "te_us"},
  {1,   0,   1,    17, "4ESS_MGI", "te_us",
"4ESS Megacom International",                   "te_us"},
  {1,   1,   7,    18, "HONGKONG", "te_etsi",
"Hongkong",                                     "te_etsi"},
  {1,   0,   1,    19, "RBSCAS",  "te_dmlt",
"Robbed Bit Signaling, CAS",                    0},
  {1,   1,   7,    21, "QSIG",    "te_dmlt",
"QSIG, Intra PABX link protocol",               "te_etsi"},
  {0,   1,   6,    22, "EWSD",    "te_dmlt",
"Siemens, National ISDN EWSD",                  "te_us"},
  {0,   1,   6,    23, "5ESS_NI", "te_dmlt",
"5ESS switch National ISDN Lucent",             "te_us"},
  {1,   0,   1,    27, "T1QSIG",  "te_dmlt",
"QSIG for I1 trunk, Intra PABX link protocol",  "te_etsi"},
  {1,   0,   1,    28, "E1UNCH",  "te_dmlt",
"Unchannelized E1 (no sig, single channel group)", 0},
  {1,   0,   1,    29, "T1UNCH",  "te_dmlt",
"Unchannelized T1 (no sig, single channel group)", 0},
  {1,   0,   1,    30, "E1CHAN",  "te_dmlt",
"Channelized E1 (no sig, multiple channel groups)", 0},
  {1,   0,   1,    31, "T1CHAN",  "te_dmlt",
"Channelized T1 (no sig, multiple channel groups)", 0},
  {1,   0,   1,    32, "R2CAS",   "te_dmlt",
"R2 Signaling, CN1 channelized E1",                 0},
  {1,   1,   7,    33, "VN6", "te_etsi",
"VN6, ETSI France and Matra",      "te_etsi"},
  {0,   0,   0,    0xff, 0,       0,           0}
};

static void diva_load_print_supported_protocols (const char* tab) {
  int i, s;
  char buffer[256];

  if (!tab) tab = "";

  for (i = 0; dmlt_protocols [i].name; i++) {
    s = sprintf(buffer,"%s-f ", tab);
    strcat (buffer, dmlt_protocols [i].name);
    s = strlen (buffer) - s;
    while (s++ < 9) {
      strcat(buffer, " ");
    }
    strcat (buffer, dmlt_protocols [i].description);
    s = strlen (buffer) - s;
    while (s++ < 54) {
      strcat(buffer, ".");
    }
    if (dmlt_protocols[i].pri && dmlt_protocols[i].bri) {
      strcat (buffer, " PRI & BRI");
    } else if (dmlt_protocols[i].pri) {
      strcat (buffer, " PRI");
    } else if (dmlt_protocols[i].bri) {
      strcat (buffer, "       BRI");
    }
    fprintf (VisualStream, "%s\n", buffer);
  }
}

static int diva_load_get_protocol_by_name (const char* name) {
  int i;
  for (i = 0; dmlt_protocols [i].name; i++) {
    if (!strcmp (dmlt_protocols[i].name, name)) {
      return (i);
    }
  }

  return (-1);
}

/* --------------------------------------------------------------------------
    Allocate memory, write image to the memory and returm pointer to this
    memory
   -------------------------------------------------------------------------- */
static int diva_configure_card (int ordinal) {

  DBG_LOG(("\n"))

  switch (ordinal) {
    case CARDTYPE_DIVASRV_P_30M_PCI:
      return (diva_configure_pri_rev_1_30m (0));

    case CARDTYPE_DIVASRV_P_30M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI:
      return (diva_configure_pri_rev_1_30m (1));

    case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
      return (diva_configure_4bri_rev_1_8 (1, MQ_INSTANCE_COUNT));

    case CARDTYPE_DIVASRV_Q_8M_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_PCI:
      return (diva_configure_4bri_rev_1_8 (0, MQ_INSTANCE_COUNT));

    case CARDTYPE_DIVASRV_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      return (diva_configure_4bri_rev_1_8 (1, 1));

    case CARDTYPE_DIVASRV_B_2F_PCI:
    case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
      return (diva_configure_4bri_rev_1_8 (1, 1));

    case CARDTYPE_MAESTRA_PCI:
      return (diva_configure_bri_rev_1_2 (0));

    case CARDTYPE_DIVASRV_P_30M_V30_PCI:
    case CARDTYPE_DIVASRV_P_24M_V30_PCI:
    case CARDTYPE_DIVASRV_P_8M_V30_PCI:
    case CARDTYPE_DIVASRV_P_30V_V30_PCI:
    case CARDTYPE_DIVASRV_P_24V_V30_PCI:
    case CARDTYPE_DIVASRV_P_2V_V30_PCI:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_30M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_2V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCIE:
      return (diva_configure_pri_v3 ());

    case CARDTYPE_DIVA_L_P_V10_PCIE:
      return (diva_configure_pri_v3_passive ());

    case CARDTYPE_DIVA_L_1P_V10_PCIE:
      return (diva_configure_pri_passive (1));
    case CARDTYPE_DIVA_L_2P_V10_PCIE:
      return (diva_configure_pri_passive (2));
    case CARDTYPE_DIVA_L_4P_V10_PCIE:
      return (diva_configure_pri_passive (4));
    case CARDTYPE_DIVA_L_8P_V10_PCIE:
      return (diva_configure_pri_passive (8));

    case CARDTYPE_DIVASRV_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
      return (diva_configure_analog_adapter (2));
    case CARDTYPE_DIVASRV_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
      return (diva_configure_analog_adapter (4));
    case CARDTYPE_DIVASRV_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
      return (diva_configure_analog_adapter (8));

    case CARDTYPE_DIVASRV_V1P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM:
      return (diva_configure_4pri (1));
    case CARDTYPE_DIVASRV_2P_V_V10_PCI:
    case CARDTYPE_DIVASRV_2P_M_V10_PCI:
    case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
      return (diva_configure_4pri (2));
    case CARDTYPE_DIVASRV_4P_V_V10_PCI:
    case CARDTYPE_DIVASRV_4P_M_V10_PCI:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
    case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
      return (diva_configure_4pri (4));

    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
      return (diva_configure_4prie (4));
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
      return (diva_configure_4prie (8));
  }

  DBG_ERR(("A: unknown card type (%d)", ordinal))
  DBG_ERR(("   please update card configuration utility"))

  return (-1);
}

/* --------------------------------------------------------------------------
    Configure PRI 30M
    PM files
    4MBytes of memory
    dsp code
    configuration over shared memory
   -------------------------------------------------------------------------- */
static int diva_configure_pri_rev_1_30m (int rev_2) {
  dword sdram_length = MP_MEMORY_SIZE;
  byte* sdram;
  int fd, dsp_fd;
  int load_offset, protocol = 0;
  dword features, protocol_features;
  const char* protocol_suffix = ".pm";

  if (rev_2) {
    sdram_length = MP2_MEMORY_SIZE;
    protocol_suffix = ".pm2";
  }

  sdram = malloc (sdram_length);

  if (!sdram) {
    DBG_ERR(("A: can't alloc %ld bytes of memory", sdram_length))
    return (-1);
  }

  memset (sdram, 0x00, sdram_length);

  if (diva_cfg_adapter_handle) {
    const byte* file_name, *archive_name, *src = (byte*)&diva_cfg_adapter_handle[1];
    dword file_name_length, archive_name_length;

    /*
      Get protocol name from CfgLib
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeProtocol,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      DBG_ERR(("Can not retrieve protocol image name"))
      return (-1);
    } else {
      char protocol_image_name[file_name_length+1];
      memcpy (protocol_image_name, file_name, file_name_length);
      protocol_image_name[file_name_length] = 0;

      if ((fd = open (protocol_image_name, O_RDONLY, 0)) < 0) {
        DBG_ERR(("Can not open protocol image: (%s)", protocol_image_name))
        free (sdram);
        return (-1);
      }
    }

    /*
      Get DSP image name
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeDsp,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (fd);
      DBG_ERR(("Can not retrieve dsp image name"))
      return (-1);
    } else {
      char dsp_image_name[file_name_length+1];
      memcpy (dsp_image_name, file_name, file_name_length);
      dsp_image_name[file_name_length] = 0;

      if ((dsp_fd = open (dsp_image_name, O_RDONLY, 0)) < 0) {
        close (fd);
        free (sdram);
        DBG_ERR(("Can not open dsp image: (%s)", dsp_image_name))
        return (-1);
      }
    }
  } else {
    diva_cmd_line_parameter_t* e;
    char image_name[2048];
    int diva_uses_dmlt_protocol = 0;

  /*
    Get protocol file
    */
    strcpy (image_name, DATADIR);
    strcat (image_name, "/");
    e = find_entry ("f");
    if (e->found) {
      protocol = diva_load_get_protocol_by_name (&e->vs[0]);
    } else {
      protocol = diva_load_get_protocol_by_name ("ETSI");
    }
    if (protocol < 0) {
      DBG_ERR(("A: unknown protocol"))
      free (sdram);
      return (-1);
    }
    if (!dmlt_protocols[protocol].pri) {
      DBG_ERR(("A: protocol not supported on PRI interface"))
      free (sdram);
      return (-1);
    }
    if (dmlt_protocols[protocol].multi & DMLT_PRI) {
      strcat (image_name, "te_dmlt");
      diva_uses_dmlt_protocol = 1;
    } else {
      diva_uses_dmlt_protocol = 0;
      strcat (image_name, dmlt_protocols[protocol].image);
    }
    strcat (image_name, protocol_suffix);
    if ((fd = open (image_name, O_RDONLY, 0)) < 0) {
      if (dmlt_protocols[protocol].multi) {
        strcpy (image_name, DATADIR);
        strcat (image_name, "/");
        strcat (image_name, dmlt_protocols[protocol].image);
        strcat (image_name, protocol_suffix);
        fd = open (image_name, O_RDONLY, 0);
        diva_uses_dmlt_protocol = 0;
      }
    }
    if (fd < 0) {
      DBG_ERR(("A: can't open protocol image: (%s)", image_name))
      free (sdram);
      return (-1);
    }
    if (diva_uses_dmlt_protocol) {
      diva_cmd_line_parameter_t* e = find_entry ("ProtVersion");
      e->found = 1;
      e->vi = (byte)(((byte)(dmlt_protocols[protocol].id)) | 0x80);
    }

    strcpy (image_name, DATADIR);
    strcat (image_name, "/");
    strcat (image_name, "dspdload.bin");
    if ((dsp_fd = open (image_name, O_RDONLY, 0)) < 0) {
      DBG_ERR(("A: can't open dsp image: (%s)", image_name))
      free (sdram);
      close (fd);
      return (-1);
    }
  }

  if (divas_start_bootloader (card_number)) {
    close (fd);
    close (dsp_fd);
    free (sdram);
    DBG_ERR(("A: can't start bootloader"))
    return (-1);
  }

  if ((load_offset = divas_pri_create_image (rev_2,
                                             sdram,
                                             fd,
                                             dsp_fd,
                  diva_cfg_adapter_handle ? "" : dmlt_protocols[protocol].name,
                  diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol].id,
                                             &protocol_features)) < 0) {
    const char* error_string = "unknown error";
    switch (load_offset) {
      case -1:
        error_string = "not enough memory";
        break;
      case -2:
        error_string = "can't load protocol file";
        break;
      case -3:
        error_string = "can't load dsp file";
        break;
    }
    DBG_ERR(("A: %s", error_string))
    free (sdram);
    close (fd);
    close (dsp_fd);
    return (-1);
  }

  close (fd);
  close (dsp_fd);

  if (divas_ram_write (card_number,
                       0,
                       sdram+load_offset,
                       load_offset,
                       sdram_length - load_offset - 1)) {
    DBG_ERR(("A: can't write card memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }

  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number,
                           MP_UNCACHED_ADDR(MP_PROTOCOL_OFFSET),
                           features)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_load_fpga_image (const char* name) {
  int fpga_fd, i;
  byte* data;

  if ((fpga_fd = open (name, O_RDONLY, 0)) < 0) {
    DBG_ERR(("A: failed to open fpga image file"))
    return (-1);
  }

  if ((i = lseek (fpga_fd, 0, SEEK_END)) <= 0) {
    close (fpga_fd);
    DBG_ERR(("A: failed to get fpga image length"))
    return (-1);
  }
  lseek (fpga_fd, 0, SEEK_SET);

  if ((data = malloc (i + 1024)) == 0) {
    close (fpga_fd);
    DBG_ERR(("A: failed to alloc fpga image memory"))
    return (-1);
  }
  if (read (fpga_fd, data, i) != i) {
    free (data);
    close (fpga_fd);
    DBG_ERR(("A: failed to read fpga image"))
    return (-1);
  }

  close (fpga_fd);

  if (divas_write_fpga (card_number, 0, data, i)) {
    free (data);
    DBG_ERR(("A: failed to write fpga image"))
    return (-1);
  }

  free (data);

  return (0);
}


/*
 0x5000 : Flash Control Register
 0x5004 : Flash Low  Address Register (16:1)   => reg data (15:0)
 0x5008 : Flash High Address Register (22:17)  => reg data  (5:0)
 0x500C : Flash Write Data     Register (15:0)
 0x5010 : Flash Read Data     Register (15:0)

Bit
4 : CUSTOM_OE_N
3 : CUSTOM_WR_N
2 : CUSTOM_ADV_N
1 : CUSTOM_CS_N
0 : CUSTOM_RST_N
*/
#define DIVA_CUSTOM_VENDOR 0x89
#define DIVA_CUSTOM_DEVICE 0x881D /* 64 MBit T */
/*
  Programming region 1:

  Programming region 2:
  128 56 380000 - 38FFFF

  128 62 3E0000 - 3EFFFF


  32 66 3FC000 - 3FFFFF
*/



#define DIVA_CUSTOM_CONTROL_REGISTER_OFFSET 0x5000
#define DIVA_CUSTOM_LOW_ADDRESS_REGISTER    0x5004
#define DIVA_CUSTOM_HI_ADDRESS_REGISTER     0x5008
#define DIVA_CUSTOM_WRITE_DATA_REGISTER     0x500C
#define DIVA_CUSTOM_READ_DATA_REGISTER      0x5010

#define DIVA_CUSTOM_RST_N (1U << 0)
#define DIVA_CUSTOM_CS_N  (1U << 1)
#define DIVA_CUSTOM_ADV_N (1U << 2)
#define DIVA_CUSTOM_WR_N  (1U << 3)
#define DIVA_CUSTOM_OE_N  (1U << 4)

static dword diva_flash_control = DIVA_CUSTOM_RST_N | DIVA_CUSTOM_CS_N | DIVA_CUSTOM_ADV_N | DIVA_CUSTOM_WR_N | DIVA_CUSTOM_OE_N;

#define DIVA_CUSTOM_IMAGE_SIZE 1171392
#define DIVA_CUSTOM_IMAGE_OFFSET 0xa0000
#define DIVA_CUSTOM_IMAGE_POINTER_OFFSET 0x3FC000
#define DIVA_CUSTOM_MAX_IMAGE_ADDRESS (0x3e0000-DIVA_CUSTOM_IMAGE_SIZE)

static int diva_flash_reset (dword handle, int init) {
  int ret = 0;

  diva_flash_control = DIVA_CUSTOM_RST_N | DIVA_CUSTOM_CS_N | DIVA_CUSTOM_ADV_N | DIVA_CUSTOM_WR_N | DIVA_CUSTOM_OE_N;

  usleep (100);
  ret |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control);
  usleep (100);
  ret |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~DIVA_CUSTOM_RST_N);
  usleep (100);
  if (init != 0)
    diva_flash_control &= ~DIVA_CUSTOM_CS_N;
  ret |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control);
  usleep (100);

  return (ret);
}

static word diva_read_flash_word (dword handle, dword address) {
  int err = 0;
  dword ret;

  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_LOW_ADDRESS_REGISTER, (word)address);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_HI_ADDRESS_REGISTER,  (word)(address >> 16));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~DIVA_CUSTOM_ADV_N);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_ADV_N|DIVA_CUSTOM_CS_N|DIVA_CUSTOM_OE_N));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_CS_N|DIVA_CUSTOM_OE_N));
  ret = divas_read_bar_dword (handle, 0, DIVA_CUSTOM_READ_DATA_REGISTER, &err);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control);

  if (err != 0)
    DBG_ERR(("%s %08x", __FUNCTION__, address))

  return ((word)ret);
}

static void diva_write_flash_word (dword handle, dword address, word data) {
  int err = 0;

  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_LOW_ADDRESS_REGISTER, (word)address);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_HI_ADDRESS_REGISTER,  (word)(address >> 16));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~DIVA_CUSTOM_ADV_N);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_ADV_N|DIVA_CUSTOM_CS_N));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_CS_N));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_WRITE_DATA_REGISTER, data);
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_CS_N|DIVA_CUSTOM_WR_N));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control & ~(DIVA_CUSTOM_CS_N));
  err |= divas_write_bar_dword (handle, 0, DIVA_CUSTOM_CONTROL_REGISTER_OFFSET, diva_flash_control);

  if (err != 0)
    DBG_ERR(("%s %08x", __FUNCTION__, address))
}

static int diva_verify_flash_device (dword handle) {
  word vendor_id, device_id;

  diva_write_flash_word (handle, 0, 0x90); /* Read Device Identifier */
  vendor_id = diva_read_flash_word (handle, 0);
  device_id = diva_read_flash_word (handle, 1);
  diva_write_flash_word (handle, 0, 0xff);

  if (vendor_id != DIVA_CUSTOM_VENDOR || device_id != DIVA_CUSTOM_DEVICE) {
    DBG_ERR(("vendor:%04x device:%04x not supported", vendor_id, device_id))
    return (-1);
  }

  DBG_TRC(("vendor:%04x device:%04x", vendor_id, device_id))

  return (0);
}

static int diva_init_flash_status_register (dword handle) {
  byte status;

  diva_write_flash_word (handle, 0, 0x70);
  status = diva_read_flash_word (handle, 0);
  DBG_LOG(("status:%02x", status))
  if ((status & 0x80) == 0) {
    DBG_ERR(("device busy"))
    return (-1);
  }
  if (status != 0x80) {
    diva_write_flash_word (handle, 0, 0x50);
    diva_write_flash_word (handle, 0, 0x70);
    status = diva_read_flash_word (handle, 0);
    DBG_LOG(("status:%02x", status))
  }

  if (status != 0x80) {
    DBG_ERR(("status error"))
    return (-1);
  }
  diva_write_flash_word (handle, 0, 0xff);

  return (0);
}

static void diva_print_flash_status (byte status) {
  const char* p[] = {
    "0 - BEFP in-progress",
    "1 - Block locked",
    "2 - Program suspend",
    "3 - VPP < VPPLK",
    "4 - Program fail",
    "5 - Erase fail",
    "6 - Erase suspend",
    "7 - Device ready"
  };
  int i;

  for (i = 0; i < 8; i++) {
    if ((status & (1<<i)) != 0)
      DBG_LOG(("%s", p[i]))
  }

}

static int diva_erase_flash_block (dword handle, dword address) {
  byte status;
  int i = 0;

  diva_write_flash_word (handle, address, 0x60); // Unlock block
  diva_write_flash_word (handle, address, 0xd0); // Unlock block confirm
  diva_write_flash_word (handle, 0, 0x50); // Erase status
  diva_write_flash_word (handle, address, 0x20); // Erase block
  diva_write_flash_word (handle, address, 0xd0); // Erase block confirm
  diva_write_flash_word (handle, 0, 0x70); // Read Status

  do {
    i++;
    status = diva_read_flash_word (handle, 0);
  } while ((status & 0x80) == 0);

  diva_write_flash_word (handle, 0, 0xff);

  diva_print_flash_status (status);

  if (status != 0x80)
    DBG_LOG(("block erase:%08x status:%02x %d", address, status, i))

  return ((status == 0x80) ? 0 : -1);
}

static int diva_program_flash_word (dword handle, dword address, word data) {
  byte status;
  int i = 0;

  diva_write_flash_word (handle, 0, 0x50); // Erase status
  diva_write_flash_word (handle, address, 0x40); // Write start
  diva_write_flash_word (handle, address, data); // Write confirm
  diva_write_flash_word (handle, 0, 0x70); // Read Status

  do {
    i++;
    status = diva_read_flash_word (handle, 0);
  } while ((status & 0x80) == 0);

  diva_write_flash_word (handle, 0, 0xff);

  diva_print_flash_status (status);

  DBG_LOG(("write word:%08x status:%02x %d", address, status, i))

  return (0);
}

static int diva_program_flash_buffered (dword handle, dword address, word* data, byte length) {
  byte status;
  int i = 0;

  diva_write_flash_word (handle, 0, 0x50); // Erase status
  diva_write_flash_word (handle, address, 0xe8); // Buffered programing start
  do {
    i++;
    status = diva_read_flash_word (handle, address);
  } while ((status & 0x80) == 0);

  /*
  DBG_LOG(("buffer wait: %d", i))
  diva_print_flash_status (status);
  */

  diva_write_flash_word (handle, address, length-1); // Count

  diva_write_flash_word (handle, address, data[0]); // Data
  for (i = 1; i < length; i++) {
    diva_write_flash_word (handle, address+i, data[i]); // Data
  }

  diva_write_flash_word (handle, address, 0xd0); // Start
  diva_write_flash_word (handle, 0, 0x70); // Read Status

  i = 0;
  do {
    i++;
    status = diva_read_flash_word (handle, address);
  } while ((status & 0x80) == 0);

  if (status != 0x80) {
    DBG_LOG(("buffer write wait: %d", i))
    diva_print_flash_status (status);
  }

  diva_write_flash_word (handle, 0, 0xff);

  return ((status == 0x80) ? 0 : -1);
}

static int diva_read_flash_protection_register (dword handle, word reg, void* data, word length) {
  word* p = data;
  word i;

  length = length >> 1;

  diva_write_flash_word (handle, 0, 0x90); /* Read Device Identifier */

  for (i = 0; i < length; i++) {
    *p++ = diva_read_flash_word (handle, reg+i);
    DBG_TRC(("register[%04x]=%04x", reg+i, p[-1]))
  }
  diva_write_flash_word (handle, 0, 0xff);

  return (0);
}

static int diva_write_flash_protection_register (dword handle, word reg, word data) {
  dword address = 0x3F0000 + reg;
  dword status;
  int i;

  diva_write_flash_word (handle, 0, 0x50); // Clear Status
  diva_write_flash_word (handle, address, 0xc0);
  diva_write_flash_word (handle, address, data);
  diva_write_flash_word (handle, 0, 0x70); // Read Status

  i = 0;
  do {
    i++;
    status = diva_read_flash_word (handle, 0);
  } while ((status & 0x80) == 0);

  if (status != 0x80) {
    DBG_ERR(("write flash protection register: %d", i))
    diva_print_flash_status (status);
  }

  diva_write_flash_word (handle, 0, 0xff);

  return ((status == 0x80) ? 0 : -1);
}

static int diva_write_flash_image (const char* name, dword image_offset) {
  dword handle;
  dword i;
  int fd;
  word* data;
  dword percentage, step;
  int verify = find_entry("NoVerifyOnCUSTOMWrite")->found == 0;

  if ((fd = open (name, O_RDONLY, 0)) < 0) {
    DBG_ERR(("A: failed to open flash image file"))
    return (-1);
  }

  if ((i = lseek (fd, 0, SEEK_END)) != DIVA_CUSTOM_IMAGE_SIZE) {
    close (fd);
    DBG_ERR(("A: failed to get flash image length"))
    return (-1);
  }
  lseek (fd, 0, SEEK_SET);

  if ((data = malloc (i + 1024)) == 0) {
    close (fd);
    DBG_ERR(("A: failed to alloc flash image memory"))
    return (-1);
  }
  if (read (fd, data, i) != i) {
    free (data);
    close (fd);
    DBG_ERR(("A: failed to read flash image"))
    return (-1);
  }

  close (fd);

  handle = divas_open_driver (card_number);

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    free (data);
    DBG_ERR(("A: failed to open card %d", card_number))
    return (-1);
  }

  if (diva_flash_reset (handle, 1) != 0) {
    free (data);
    diva_flash_reset (handle, 0);
    divas_close_driver (handle);
    DBG_ERR(("A: failed to reset flash"))
    return (-1);
  }

  if (diva_verify_flash_device (handle) != 0) {
    free (data);
    diva_flash_reset (handle, 0);
    divas_close_driver (handle);
    DBG_ERR(("A: flash device not supported"))
    return (-1);
  }
  if (diva_init_flash_status_register(handle) != 0) {
    free (data);
    diva_flash_reset (handle, 0);
    divas_close_driver (handle);
    DBG_ERR(("A: failed to init flash status register"))
    return (-1);
  }

  percentage = DIVA_CUSTOM_IMAGE_SIZE/2/50;
  step = percentage;

  if (diva_erase_flash_block (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET) != 0) {
    free (data);
    diva_flash_reset (handle, 0);
    divas_close_driver (handle);
    DBG_ERR(("A: failed to erase flash sector at %p", DIVA_CUSTOM_IMAGE_POINTER_OFFSET))
    return (-1);
  }

  fprintf (VisualStream, "<");
  for (i = 0; i < DIVA_CUSTOM_IMAGE_SIZE/2; i += 32) {
    if (i >= step) {
      step += percentage;
      fprintf (VisualStream, "=");
      fflush(VisualStream);
    }
    if ((i % 0x10000) == 0) {
      fprintf (VisualStream, " E ");
    }
  }
  fprintf (VisualStream, " 100%s>\n", "%");
  fprintf (VisualStream, "[");
  fflush(VisualStream);
  step = percentage;
  for (i = 0; i < DIVA_CUSTOM_IMAGE_SIZE/2; i += 32) {
    dword j;

    if (i >= step) {
      step += percentage;
      fprintf (VisualStream, "+");
      fflush(VisualStream);
    }

    if ((i % 0x10000) == 0) {
      fprintf (VisualStream, " | ");
      fflush (VisualStream);
      if (diva_erase_flash_block (handle, i+image_offset) != 0) {
        free (data);
        diva_flash_reset (handle, 0);
        divas_close_driver (handle);
        DBG_ERR(("A: failed to erase flash sector at %p", i+image_offset))
        return (-1);
      }
    }
    if (diva_program_flash_buffered (handle, i + image_offset, &data[i], 32) != 0) {
      free (data);
      diva_flash_reset (handle, 0);
      divas_close_driver (handle);
      DBG_ERR(("A: failed to write flash sector at %p", i+image_offset))
      return (-1);
    }
    if (verify != 0) {
      for (j = 0; j < 32; j++) {
        dword tmp = diva_read_flash_word (handle, i + j + image_offset);
        if (tmp != data[i+j]) {
          free (data);
          diva_flash_reset (handle, 0);
          divas_close_driver (handle);
          DBG_ERR(("write error at %p %04x != %04x", i+j+image_offset, tmp, data[i+j]))
          return (-1);
        }
      }
    }

    fflush (VisualStream);
  }

  fprintf (VisualStream, " OK  ]\n");

  {
    word tmp, tmp1;

    diva_program_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET, (word)image_offset);
    diva_program_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET+1, (word)(image_offset >> 16));

    tmp = diva_read_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET);
    tmp1 = diva_read_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET+1);

    if (tmp != ((word)image_offset) || tmp1 != ((word)(image_offset >> 16))) {
      free (data);
      diva_flash_reset (handle, 0);
      divas_close_driver (handle);
      DBG_ERR(("write error at %p:%p %04x:%04x != %04x:%04x",
               DIVA_CUSTOM_IMAGE_POINTER_OFFSET,
               DIVA_CUSTOM_IMAGE_POINTER_OFFSET+1,
               tmp, tmp1, ((word)image_offset), ((word)(image_offset >> 16))))
      return (-1);
    }
  }

  diva_flash_reset (handle, 0);

  divas_close_driver (handle);

  free (data);

  return (0);
#if 0
  dword handle = divas_open_driver (card_number);
  int i = 0;
  dword tmp, tmp1;

  if (handle == DIVA_INVALID_FILE_HANDLE)
    return (-1);

  i |= diva_flash_reset (handle, 1);

  if (diva_verify_flash_device (handle) != 0) {
    divas_close_driver (handle);
    return (-1);
  }
  if (diva_init_flash_status_register(handle) != 0) {
    divas_close_driver (handle);
    return (-1);
  }

  tmp = diva_read_flash_word (handle, 0x3FC000);
  DBG_LOG(("read data: %04x", tmp))
  {
    word tmp_data[32];
    int i;

    for (i = 0; i < 32; i++) {
      tmp_data[i] = i;
      tmp = diva_read_flash_word (handle, 0x3FC000+32+i);
      DBG_LOG(("read buffered data:%04x", tmp))
    }

  }

  diva_erase_flash_block (handle, 0x3FC000);
  diva_program_flash_word (handle, 0x3FC000, 0xabcd);
  tmp = diva_read_flash_word (handle, 0x3FC000);
  DBG_LOG(("read data: %04x", tmp))
  {
    word tmp_data[32];
    int i;

    for (i = 0; i < 32; i++) {
      tmp_data[i] = i;
    }

    diva_program_flash_buffered (handle, 0x3FC000+32, tmp_data, 32);
  }

//  diva_read_flash_protection_register (handle, 0x80, void* data, 10);

  tmp = diva_read_flash_word (handle, 0x20>>1);
  tmp1 = diva_read_flash_word (handle, 0x24>>1);
  DBG_LOG(("divas_read_bar_dword: %08x", tmp))
  DBG_LOG(("divas_read_bar_dword: %08x", tmp1))


  i |= diva_flash_reset (handle, 0);

  divas_close_driver (handle);


  return (i);
#endif
}

static int diva_update_flash_image (const char* name) {
  int ret = diva_write_flash_image (name, DIVA_CUSTOM_IMAGE_OFFSET);

  return (ret);
}

static int diva_read_flash_image (int image_nr) {
  diva_cmd_line_parameter_t* e = find_entry ("File");
  FILE* out_file;
  const char* file_name;
  dword length = DIVA_CUSTOM_IMAGE_SIZE, i;
  dword handle;
  word* data;
  dword image_offset;

  if (!(data = (word*)malloc (length))) {
    DBG_ERR(("FAILED: Can't allocate %u bytes of memory\n", length))
    return (1);
  }

  if (e->found) {
    file_name = e->vs;
  } else {
    file_name = "flash.bin";
  }

  if (!(out_file = fopen (file_name, "wb"))) {
    fprintf (VisualStream,
             "FAILED: Can't create file '%s', errno=%d\n",
             file_name, errno);
    free (data);
    return (1);
  }

  handle = divas_open_driver (card_number);

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    fprintf (VisualStream, "FAILED: Can't access card\n");
    free (data);
    fclose (out_file);
    unlink (file_name);
    return (1);
  }

  diva_flash_reset (handle, 0);

  if (image_nr == 0) {
    image_offset = 0;
  } else {
    dword lo = diva_read_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET);
    dword hi = diva_read_flash_word (handle, DIVA_CUSTOM_IMAGE_POINTER_OFFSET+1);

    image_offset = (hi << 16) | lo;

    if (image_offset % 32 != 0 || image_offset >= DIVA_CUSTOM_MAX_IMAGE_ADDRESS) {
      free (data);
      fclose (out_file);
      unlink (file_name);
      diva_flash_reset (handle, 0);
      divas_close_driver (handle);
      DBG_ERR(("Failed to locate flash image at %p", image_offset))
      return (-1);
    }
  }


  length = length/2;

  for (i = 0; i < length; i++) {
    data[i] = diva_read_flash_word (handle,i+image_offset);
    data[i] = diva_read_flash_word (handle,i+image_offset);
    data[i] = diva_read_flash_word (handle,i+image_offset);
    data[i] = diva_read_flash_word (handle,i+image_offset);
  }

  diva_flash_reset (handle, 0);

  divas_close_driver (handle);

  if (fwrite (data, 1, length*2, out_file) != length*2) {
    free (data);
    fclose (out_file);
    unlink (file_name);
    fprintf (VisualStream,
             "FAILED: Can't write file '%s', errno=%d\n",
             file_name, errno);
    return (1);
  }

  fprintf (VisualStream, "Saved using offset %p as %s\n", (void*)(long)image_offset, file_name);

  free (data);
  fclose (out_file);

  return (0);
}

static int diva_read_flash_id (void) {
  dword handle;
  word data[128];

  handle = divas_open_driver (card_number);

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    fprintf (VisualStream, "FAILED: Can't access card\n");
    return (1);
  }

  diva_flash_reset (handle, 1);

  diva_read_flash_protection_register (handle, 0x81, data, 8);

  diva_flash_reset (handle, 0);

  divas_close_driver (handle);

  fprintf (VisualStream, "%04x:%04x:%04x:%04x\n", data[0], data[1], data[2], data[3]);

  return (0);
}

static int diva_read_flash_sn (void) {
  dword handle;
  dword sn_lo = 0, sn_hi = 0;

  handle = divas_open_driver (card_number);

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    fprintf (VisualStream, "FAILED: Can't access card\n");
    return (1);
  }

  diva_flash_reset (handle, 1);

  diva_read_flash_protection_register (handle, 0x85, &sn_lo, 2);
  diva_read_flash_protection_register (handle, 0x85+1, &sn_hi, 2);

  diva_flash_reset (handle, 0);

  divas_close_driver (handle);

  fprintf (VisualStream, "%u\n", sn_lo | (sn_hi << 16));

  return (0);
}

static int diva_write_flash_sn (dword sn) {
  int ret = 0;
  dword handle;
  dword sn_lo, sn_hi;

  handle = divas_open_driver (card_number);

  if (handle == DIVA_INVALID_FILE_HANDLE) {
    fprintf (VisualStream, "FAILED: Can't access card\n");
    return (1);
  }

  diva_flash_reset (handle, 1);

  ret |= diva_write_flash_protection_register (handle, 0x85, (word)sn);
  ret |= diva_write_flash_protection_register (handle, 0x85+1, ((word)(sn >> 16)) & 0x00ff);

  diva_read_flash_protection_register (handle, 0x85, &sn_lo, 2);
  diva_read_flash_protection_register (handle, 0x85+1, &sn_hi, 2);

  if (ret != 0 || (sn_lo | (sn_hi << 16)) != (sn & 0x00ffffff)) {
    diva_flash_reset (handle, 0);
    divas_close_driver (handle);
    DBG_ERR(("Failed to write serial number: %08x != %08x", (sn_lo | (sn_hi << 16)), sn))
    return (-1);
  }

  ret |= diva_write_flash_protection_register (handle, 0x80, 0xFFFD);

  diva_flash_reset (handle, 0);

  divas_close_driver (handle);

  if (ret == 0) {
    fprintf (VisualStream, "%u %08x OK\n", (sn_lo | (sn_hi << 16)), (sn_lo | (sn_hi << 16)));
  }

  return (ret);
}






/* --------------------------------------------------------------------------
    Configure PRI 30M
    PM3 files
    8MBytes of memory
    dsp code
    fpga code
    configuration over shared memory
   -------------------------------------------------------------------------- */
static int diva_configure_pri_v3 (void) {
  int   fpga_fd = -1, protocol_fd = -1, dsp_fd = -1, protocol = -1, i;
  dword sdram_length = MP2_MEMORY_SIZE;
  byte* sdram;
  dword features, protocol_features;
  int load_offset;

  if (!(sdram = malloc (sdram_length))) {
    DBG_ERR(("A: can't alloc %ld bytes of memory", sdram_length))
    return (-1);
  }

  memset (sdram, 0x00, sizeof(sdram_length));

  if (diva_cfg_adapter_handle) {
    const byte* file_name, *archive_name, *src = (byte*)&diva_cfg_adapter_handle[1];
    dword file_name_length, archive_name_length;

    /*
      Get protocol image
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeProtocol,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      DBG_ERR(("Can not retrieve protocol image name"))
      return (-1);
    } else {
      char protocol_image_name[file_name_length+1];
      memcpy (protocol_image_name, file_name, file_name_length);
      protocol_image_name[file_name_length] = 0;

      if ((protocol_fd = open (protocol_image_name, O_RDONLY, 0)) < 0) {
        DBG_ERR(("Can not open protocol image: (%s)", protocol_image_name))
        free (sdram);
        return (-1);
      }
    }

    /*
      Get DSP image
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeDsp,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (protocol_fd);
      DBG_ERR(("Can not retrieve dsp image name"))
      return (-1);
    } else {
      char dsp_image_name[file_name_length+1];
      memcpy (dsp_image_name, file_name, file_name_length);
      dsp_image_name[file_name_length] = 0;

      if ((dsp_fd = open (dsp_image_name, O_RDONLY, 0)) < 0) {
        close (protocol_fd);
        free (sdram);
        DBG_ERR(("Can not open dsp image: (%s)", dsp_image_name))
        return (-1);
      }
    }

    /*
      Get FPGA image
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeFPGA,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (protocol_fd);
      close (dsp_fd);
      DBG_ERR(("Can not retrieve FPGA image name"))
      return (-1);
    } else {
      char fpga_image_name[file_name_length+1];
      memcpy (fpga_image_name, file_name, file_name_length);
      fpga_image_name[file_name_length] = 0;

      if ((fpga_fd = open (fpga_image_name, O_RDONLY, 0)) < 0) {
        close (protocol_fd);
        close (dsp_fd);
        free (sdram);
        DBG_ERR(("Can not open fpga image: (%s)", fpga_image_name))
        return (-1);
      }
    }
  } else {
    diva_cmd_line_parameter_t* e;
    int diva_uses_dmlt_protocol = 0;
    const char* protocol_suffix = ".pm3";
    char  image_name[2048];

    /*
      Get protocol file
      */
    strcpy (image_name, DATADIR);
    strcat (image_name, "/");
    e = find_entry ("f");
    if (e->found) {
      protocol = diva_load_get_protocol_by_name (&e->vs[0]);
    } else {
      protocol = diva_load_get_protocol_by_name ("ETSI");
    }
    if (protocol < 0) {
      DBG_ERR(("A: unknown protocol"))
      free (sdram);
      return (-1);
    }
    if (!dmlt_protocols[protocol].pri) {
      DBG_ERR(("A: protocol not supported on PRI interface"))
      free (sdram);
      return (-1);
    }
    if (dmlt_protocols[protocol].multi & DMLT_PRI) {
      strcat (image_name, "te_dmlt");
      diva_uses_dmlt_protocol = 1;
    } else {
      diva_uses_dmlt_protocol = 0;
      strcat (image_name, dmlt_protocols[protocol].image);
    }
    strcat (image_name, protocol_suffix);
    if ((protocol_fd = open (image_name, O_RDONLY, 0)) < 0) {
      if (dmlt_protocols[protocol].multi) {
        strcpy (image_name, DATADIR);
        strcat (image_name, "/");
        strcat (image_name, dmlt_protocols[protocol].image);
        strcat (image_name, protocol_suffix);
        protocol_fd = open (image_name, O_RDONLY, 0);
        diva_uses_dmlt_protocol = 0;
      }
    }
    if (protocol_fd < 0) {
      DBG_ERR(("A: can't open protocol image: (%s)", image_name))
      free (sdram);
      return (-1);
    }
    if (diva_uses_dmlt_protocol) {
      diva_cmd_line_parameter_t* e = find_entry ("ProtVersion");
      e->found = 1;
      e->vi = (byte)(((byte)(dmlt_protocols[protocol].id)) | 0x80);
    }

    /*
      Get and download FPGA image
      */
    strcpy (&image_name[0], DATADIR);
    strcat (&image_name[0], "/");
    strcat (&image_name[0], "dspri331.bit");

    if ((fpga_fd = open (&image_name[0], O_RDONLY, 0)) < 0) {
      close (protocol_fd);
      free (sdram);
      DBG_ERR(("A: can't open fpga image: (%s)", &image_name[0]))
      return (-1);
    }
  }

  if ((i = lseek (fpga_fd, 0, SEEK_END)) <= 0) {
    free (sdram);
    close (fpga_fd);
    close (protocol_fd);
    DBG_ERR(("A: can't get fpga image length"))
    return (-1);
  }
  lseek (fpga_fd, 0, SEEK_SET);
  if (read (fpga_fd, sdram, i) != i) {
    close (fpga_fd);
    close (protocol_fd);
    free (sdram);
    DBG_ERR(("A: can't read fpga image"))
    return (-1);
  }

  if (divas_start_bootloader (card_number)) {
    DBG_ERR(("A: can't start bootloader"))
    close (fpga_fd);
    close (protocol_fd);
    free (sdram);
    return (-1);
  }

  if (divas_write_fpga (card_number, 0, sdram, i)) {
    close (fpga_fd);
    close (protocol_fd);
    free (sdram);
    DBG_ERR(("A: fpga write failed"))
    return (-1);
  }

  close (fpga_fd);
  memset (sdram, 0x00, sdram_length);

  i = read (protocol_fd, sdram, sdram_length);
  DBG_TRC(("protocol length=%d", i))

  if (!diva_cfg_adapter_handle) {
    char  image_name[2048];
    strcpy (&image_name[0], DATADIR);
    strcat (&image_name[0], "/");
    strcat (&image_name[0], "dspdload.bin");
    if ((dsp_fd = open (&image_name[0], O_RDONLY, 0)) < 0) {
      close (protocol_fd);
      free (sdram);
      DBG_ERR(("A: can't open fpga image: (%s)", &image_name[0]))
      return (-1);
    }
  }

  if ((load_offset = divas_pri_v3_create_image (0,
                                             sdram,
                                             protocol_fd,
                                             dsp_fd,
                              diva_cfg_adapter_handle ? "" : dmlt_protocols[protocol].name,
                              diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol].id,
                                             &protocol_features)) < 0) {
    const char* error_string = "unknown error";
    switch (load_offset) {
      case -1:
        error_string = "not enough memory";
        break;
      case -2:
        error_string = "can't load protocol file";
        break;
      case -3:
        error_string = "can't load dsp file";
        break;
    }
    DBG_ERR(("A: %s", error_string))
    free (sdram);
    close (protocol_fd);
    close (dsp_fd);
    return (-1);
  }

  close (protocol_fd);
  close (dsp_fd);

  if (divas_ram_write (card_number,
                       0,
                       sdram,
                       0, /* initial offset */
                       sdram_length - 1)) {
    DBG_ERR(("A: can't write card memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }

  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number, 0x00, features)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_configure_pri_v3_passive (void) {
  int   fpga_fd = -1, i;
  dword sdram_length = MP2_MEMORY_SIZE;
  byte* sdram;

  if (!(sdram = malloc (sdram_length))) {
    DBG_ERR(("A: can't alloc %ld bytes of memory", sdram_length))
    return (-1);
  }

  memset (sdram, 0x00, sizeof(sdram_length));

  if (diva_cfg_adapter_handle) {
    const byte* file_name, *archive_name, *src = (byte*)&diva_cfg_adapter_handle[1];
    dword file_name_length, archive_name_length;

    /*
      Get FPGA image
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeFPGA,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      DBG_ERR(("Can not retrieve FPGA image name"))
      return (-1);
    } else {
      char fpga_image_name[file_name_length+1];
      memcpy (fpga_image_name, file_name, file_name_length);
      fpga_image_name[file_name_length] = 0;

      if ((fpga_fd = open (fpga_image_name, O_RDONLY, 0)) < 0) {
        free (sdram);
        DBG_ERR(("Can not open fpga image: (%s)", fpga_image_name))
        return (-1);
      }
    }
  } else {
    return (-1);
  }

  if ((i = lseek (fpga_fd, 0, SEEK_END)) <= 0) {
    free (sdram);
    close (fpga_fd);
    DBG_ERR(("A: can't get fpga image length"))
    return (-1);
  }
  lseek (fpga_fd, 0, SEEK_SET);
  if (read (fpga_fd, sdram, i) != i) {
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: can't read fpga image"))
    return (-1);
  }
  if (divas_write_fpga (card_number, 0, sdram, i)) {
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: fpga write failed"))
    return (-1);
  }

  close (fpga_fd);

  free (sdram);

  if (divas_set_protocol_features (card_number, 0)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }

  if (divas_start_adapter (card_number, 0x00, 0)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_configure_pri_passive (int tasks) {
  if (!diva_cfg_adapter_handle) {
    return (-1);
  }
  if (divas_set_protocol_features (card_number, 0)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }
  if (divas_start_adapter (card_number, 0x00, 0)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

/*
  Configure 2/4 PRI 4/8 PRI PCIe
  */
static int __diva_configure_4pri (int tasks, int fs_pcie) {
  dword features = 0, protocol_features = 0, sdram_length = (fs_pcie == 0) ? (64*1024*1024 - 64) : (128*1024*1024 - 64);
  int fpga_fd = -1, protocol_fd = -1, dsp_fd = -1;
  diva_entity_link_t* link;
  byte* sdram;
  int i;

  if (diva_cfg_adapter_handle == 0) {
    DBG_ERR(("Missing CFGLib handle"))
    return (-1);
  }

  if (!(sdram = malloc (sdram_length))) {
    DBG_ERR(("Failed to alloc %ld bytes of memory", sdram_length))
    return (-1);
  }
  memset (sdram, 0x00, sizeof(sdram_length));

  DBG_TRC(("allocated %p length %u (%08x)", sdram, sdram_length, sdram_length))

  link = diva_cfg_adapter_handle;

  for (i = 0; i < tasks; i++) {
    if (link == 0) {
      free (sdram); close (fpga_fd); close (protocol_fd); close (dsp_fd);
      DBG_LOG(("Wrong number of physical interfaces"))
      return (-1);
    }
    if (i == 0) {
      const byte* file_name, *archive_name;
      dword file_name_length, archive_name_length;
      const byte* src = (const byte*)&link[1];

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeProtocol,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram);
        DBG_ERR(("Missing protocol image name"))
        return (-1);
      } else {
        char protocol_image_name[file_name_length+1];
        memcpy (protocol_image_name, file_name, file_name_length);
        protocol_image_name[file_name_length] = 0;

        if ((protocol_fd = open (protocol_image_name, O_RDONLY, 0)) < 0) {
          free (sdram); if (fpga_fd >= 0) close (fpga_fd);
          DBG_ERR(("Failed to open protocol image: (%s)", protocol_image_name))
          return (-1);
        }
      }

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeFPGA,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram); close (protocol_fd);
        DBG_ERR(("Missing FPGA image name"))
        return (-1);
      } else {
        char fpga_image_name[file_name_length+1];

        memcpy (fpga_image_name, file_name, file_name_length);
        fpga_image_name[file_name_length] = 0;
        if ((fpga_fd = open (fpga_image_name, O_RDONLY, 0)) < 0) {
          free (sdram); if (protocol_fd >= 0) close (protocol_fd);
          DBG_ERR(("Failed to open FPGA image: (%s)", fpga_image_name))
          return (-1);
        }
      }

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeDsp,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram); close (fpga_fd); close (protocol_fd);
        DBG_ERR(("Missing DSP image name"))
        return (-1);
      } else {
        char dsp_image_name[file_name_length+1];

        memcpy (dsp_image_name, file_name, file_name_length);
        dsp_image_name[file_name_length] = 0;
        if ((dsp_fd = open (dsp_image_name, O_RDONLY, 0)) < 0) {
          free (sdram); if (protocol_fd >= 0) close (protocol_fd);
          DBG_ERR(("Failed to open DSP image: (%s)", dsp_image_name))
          return (-1);
        }
      }
    }
    link = diva_q_get_next (link);
  }

  if ((i = (int)lseek (fpga_fd, 0, SEEK_END)) <= 0) {
    free (sdram); close (protocol_fd); close (fpga_fd); close(dsp_fd);
    DBG_ERR(("Failed to get FPGA image length"))
    return (-1);
  }
  lseek (fpga_fd, 0, SEEK_SET);
  if (read (fpga_fd, sdram, i) != i) {
    free (sdram); close (protocol_fd); close (fpga_fd); close(dsp_fd);
    DBG_ERR(("Failed read FPGA image"))
    return (-1);
  }
  close (fpga_fd);

  if (divas_start_bootloader (card_number)) {
    free (sdram); close (protocol_fd); close (dsp_fd);
    DBG_ERR(("Failed to start adapter bootloader"))
    return (-1);
  }

  if (divas_write_fpga (card_number, 0, sdram, i)) {
    free (sdram); close (protocol_fd); close (dsp_fd);
    DBG_ERR(("FPGA write failed"))
    return (-1);
  }

  if (divas_4pri_create_image (sdram,
                               protocol_fd,
                               dsp_fd,
                               &protocol_features,
                               tasks,
                               fs_pcie) != 0) {
    free (sdram); close (protocol_fd);
    DBG_ERR(("Failed to create protocol image"))
    return (-1);
  }

  close (protocol_fd);
  close (dsp_fd);

  if (divas_ram_write (card_number,
                       0,
                       sdram,
                       0, /* initial offset */
                       sdram_length - 1)) {
    DBG_ERR(("Failed to write protocol image to memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }
  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number, 0x00000000, features)) {
    DBG_ERR(("Adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_configure_4pri (int tasks) {
  return (__diva_configure_4pri (tasks, 0));
}

static int diva_configure_4prie (int tasks) {
  return (__diva_configure_4pri (tasks, 1));
}


static void do_dprintf (char* fmt, ...) {
  char buffer[256];
  va_list ap;

  buffer[0] = 0;

  va_start(ap, fmt);
  vsprintf (buffer, (char*)fmt, ap);
  va_end(ap);

  fprintf (DebugStream, "%s\n", buffer);
}

/* --------------------------------------------------------------------------
    Get/Fix configuration options
   -------------------------------------------------------------------------- */
byte cfg_get_L1TristateQSIG(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("z"));
  if (e->found) {
    return ((byte)(e->vi));
  }
  return (0);
}

byte cfg_get_tei(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("t"));
  if (e->found) {
    return ((((byte)e->vi) << 1) | 1);
  }
  return (0);
}

byte cfg_get_nt2(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("n"));
  if (e->found) {
    return (1);
  }
  return (0);
}

byte cfg_get_didd_len(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("n"));
  if (e->found) {
    return ((byte)e->vi);
  }
  return (0);
}

byte cfg_get_watchdog(void) {
  return (0);
}

byte cfg_get_permanent(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("p"));
  if (e->found) {
    return ((byte)e->vi);
  }
  return (0);
}

dword cfg_get_b_ch_mask(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("BChMask"));
  if (e->found) {
    return ((dword)e->vi);
  }
  return (0);
}

byte cfg_get_nosig(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("nosig"));
  if (e->found) {
    return (1);
  }
  return (0);
}

byte cfg_get_stable_l2(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("s"));
  if (e->found) {
    return ((byte)e->vi);
  }
  return (2);
}

byte cfg_get_no_order_check(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("o"));
  if (e->found) {
    return (1);
  }
  return (0);
}

byte cfg_get_fractional_flag (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("Fractional"));
  if (e->found) {
    if (e->vi == 0x02) {
      return (0x04);
    }
    return ((byte)e->vi);
  }
  return (0);
}

byte cfg_get_QsigDialect (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("QsigDialect"));
  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

word cfg_get_QsigFeatures (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("QsigFeatures"));
  if (e->found) {
    return ((word)e->vi);
  }

  return (0);
}

byte cfg_get_low_channel(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("l"));
  if (e->found) {
    return ((byte)e->vi);
  }
  return (0);
}

byte cfg_get_prot_version(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ProtVersion"));

  if (e->found) {
    return ((byte)(e->vi));
  }

  return (0);
}

byte cfg_get_crc4(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("e"));
  if (e->found) {
    return ((byte)e->vi);
  }
  return (0);
}

void cfg_get_spid (int nr, char* spid) {
  diva_cmd_line_parameter_t* e;
  char tmp[8];

  sprintf (tmp, "%dspid", nr);
  if ((e = find_entry (tmp))) {
    switch (e->found) {
      case 1:
        strcpy (spid, &e->vs[0]);
        break;

      case 2: /* already counted string */
        memcpy (spid, e->vs, e->vs[0]+1);
        break;
    }
  } else {
    fprintf (VisualStream, "W: SPID %d not found\n", nr);
  }
}

void cfg_get_spid_oad (int nr, char* oad) {
  diva_cmd_line_parameter_t* e;
  char tmp[8];

  sprintf (tmp, "%doad", nr);
  if ((e = find_entry (tmp))) {
    switch (e->found) {
      case 1:
        strcpy (oad, &e->vs[0]);
        break;

      case 2: /* already counted string */
        memcpy (oad, e->vs, e->vs[0]+1);
        break;
    }
  } else {
    fprintf (VisualStream, "W: OAD %d not found\n", nr);
  }
}

void cfg_get_spid_osa (int nr, char* osa) {
  diva_cmd_line_parameter_t* e;
  char tmp[8];

  sprintf (tmp, "%dosa", nr);
  if ((e = find_entry (tmp))) {
    switch (e->found) {
      case 1:
        strcpy (osa, &e->vs[0]);
        break;

      case 2: /* already counted string */
        memcpy (osa, e->vs, e->vs[0]+1);
        break;
    }
  } else {
    fprintf (VisualStream, "W: OSA %d not found\n", nr);
  }
}

byte cfg_get_forced_law(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("m"));
  if (e->found) {
    return ((byte)(e->vi));
  }
  return (0);
}

dword cfg_get_ModemGuardTone(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemGuardTone"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemMinSpeed(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemMinSpeed"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemMaxSpeed(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemMaxSpeed"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemOptions(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemOptions"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemOptions2(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemOptions2"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemNegotiationMode(void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("ModemNegotiationMode"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemModulationsMask(void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("ModemModulationsMask"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_ModemTransmitLevel(void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("ModemTransmitLevel"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

byte cfg_get_ModemCarrierWaitTime (void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("ModemCarrierWait"));
  if (e->found) {
    return ((byte)(e->vi));
  }
  return (0);
}

byte cfg_get_ModemCarrierLossWaitTime (void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("ModemCarrierLossWait"));
  if (e->found) {
    return ((byte)(e->vi));
  }
  return (0);
}

byte cfg_get_PiafsRtfOff (void) {
  diva_cmd_line_parameter_t* e=find_entry(entry_name("PiafsRtfOff"));
  if (e->found) {
    return ((byte)(e->vi));
  }
  return (0);
}

dword cfg_get_FaxOptions(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("FaxOptions"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

dword cfg_get_FaxMaxSpeed(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("FaxMaxSpeed"));
  if (e->found) {
    return (e->vi);
  }
  return (0);
}

byte cfg_get_Part68LevelLimiter (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("Part68Lim"));
  if (e->found) {
    return ((e->vi == 2) ? 0 : 1);
  }
  return (1);
}

byte cfg_get_RingerTone(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("RingerTone"));

  if (e->found) {
    return ((e->vi) ? 1 : 0);
  }

  return (0);
}

byte cfg_get_UsEktsCachHandles(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsEktsCachHandles"));

  if (e->found && e->vi) {
    return ((byte)e->vi);
  }

  return (0);
}

byte cfg_get_UsEktsBeginConf(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsEktsBeginConf"));

  if (e->found && e->vi) {
    return ((byte)e->vi | 0x80);
  }

  return (0);
}

byte cfg_get_UsEktsDropConf(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsEktsDropConf"));

  if (e->found && e->vi) {
    return ((byte)e->vi | 0x80);
  }

  return (0);
}

byte cfg_get_UsEktsCallTransfer(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsEktsCallTransfer"));

  if (e->found && e->vi) {
    return ((byte)e->vi | 0x80);
  }

  return (0);
}

byte cfg_get_UsEktsMWI(void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsEktsMWI"));

  if (e->found && e->vi) {
    return ((byte)e->vi | 0x80);
  }

  return (0);
}

byte cfg_get_UsForceVoiceAlert (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsForceVoiceAlert"));

  if (e->found) {
    return (1);
  }

  return (0);
}

byte cfg_get_UsDisableAutoSPID (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("UsDisableAutoSPID"));

  if (e->found) {
    return (1);
  }

  return (0);
}

byte cfg_get_BriLinkCount (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("BriLinkCount"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

int cfg_get_Diva4BRIDisableFPGA (void) {
  diva_cmd_line_parameter_t* e = find_entry ("Diva4BRIDisableFPGA");

  if (e->found) {
    return (1);
  }

  return (0);
}

byte cfg_get_TxAttenuation (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("TxAttenuation"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

static void diva_fix_configuration (int instance) {
  diva_cmd_line_parameter_t* stable_l2;
  diva_cmd_line_parameter_t* tristate;
  diva_cmd_line_parameter_t* e;
  char s_name[8] = "s";
  char p_name[8] = "p";
  char u_name[8] = "u";
  char t_name[8] = "t";
  char z_name[8] = "z";
  char n_name[8] = "n";
  char x_name[8] = "x";
  char q_name[8] = "q";
  char q_d_name  [16] = "QsigDialect";
  char chi_f_name[16] = "ChiFormat";
  char monitor_name[16] = "Monitor";

  if (instance) {
    sprintf (s_name, "s%d", instance);
    sprintf (p_name, "p%d", instance);
    sprintf (u_name, "u%d", instance);
    sprintf (t_name, "t%d", instance);
    sprintf (z_name, "z%d", instance);
    sprintf (n_name, "n%d", instance);
    sprintf (x_name, "x%d", instance);
    sprintf (q_name, "q%d", instance);
    sprintf (q_d_name,   "QsigDialect%d", instance);
    sprintf (chi_f_name, "ChiFormat%d", instance);
    sprintf (monitor_name, "Monitor%d", instance);
  }
  stable_l2 = find_entry (s_name);
  tristate  = find_entry (z_name);
  e = find_entry (x_name);


  if (e->found) { /* NT mode operation was selected */
    stable_l2->found = 1;
    stable_l2->vi = 0x02 /* L2 Permanent */ | 0x08 /* NT Mode */;
  }

  e = find_entry (p_name);
  if (e->found) { /* Permanent connection */
    stable_l2->found = 1;
    stable_l2->vi = (byte)(2 | (stable_l2->vi & 0x08)); /* L2 Permanent */
    e = find_entry (n_name);
    e->found = 1;
    e->vi    = 0;
  }

  e = find_entry(u_name);
  if (e->found) {
    stable_l2->found = 1;
    stable_l2->vi = (byte)(2 | (stable_l2->vi & 0x08));
    e = find_entry (n_name);
    e->found = 1;
    e = find_entry (t_name);
    e->found = 1;
    e->vi    = 0;
  }

  e = find_entry (chi_f_name);
  if (e->found && e->vi) { /* CHI IE format */
    stable_l2->found = 1;
    stable_l2->vi |= 0x10; /* Use logical channels */
  }

  if (tristate->found) {
    tristate->vi |= (byte)(0x01 << 2);  /* Huntgroup support */
  }

  e = find_entry (monitor_name);
  if (e->found) {
    tristate->found = 1;
    tristate->vi |= (byte)(0x02 << 2);  /* Monitor support */
  }

  e = find_entry(q_name);
  if (e->found && e->vi) {
    tristate->found = 1;
    tristate->vi |= (byte)e->vi;
  }
}

static dword protocol_features2idi_features (dword protocol_features) {
  dword features = 0;

  if (protocol_features & PROTCAP_MAN_IF ) {
    features |= DI_MANAGE ;
  }
  if (protocol_features & PROTCAP_V_42) {
    features |= DI_V_42 ;
  }
  if (protocol_features & PROTCAP_EXTD_FAX ) {
    features |= DI_EXTD_FAX ;
  }

  return (features);
}

void cfg_adjust_rbs_options (void) {
  diva_cmd_line_parameter_t* e;
  byte  RbsOptions      = DIVA_RBS_GLARE_RESOLVE_PATY_BIT ;
  dword RbsAnswerDelay  = 30;
  dword RbsDigitTimeout = 10;
  byte  RbsBearerCap    = 4;
  dword RbsDebugFlag    = 0x0003;
  byte* Spid1;
  byte  RbsDialType     = 0x10; /* DTMF */
  byte  RbsTrunkMode    = 0x00; /* Wink Start */

  int  bRbsGlareResolve;
  int  bRbsDid;
  dword uRbsAnswerDelay;
  dword uRbsDigitTimeout;
  byte uRbsBearerCap;
  dword uRbsDebug;

  int i ;

  e = find_entry("x");
  if (e->found) {
    RbsOptions = 0 ;
    RbsDigitTimeout = 15 ;
  }

  bRbsGlareResolve = RbsOptions & DIVA_RBS_GLARE_RESOLVE_PATY_BIT    ? 1 : 0;
  bRbsDid          = RbsOptions & DIVA_RBS_DIRECT_INWARD_DIALING_BIT ? 1 : 0;
  uRbsAnswerDelay  = RbsAnswerDelay;
  uRbsDigitTimeout = RbsDigitTimeout;
  uRbsBearerCap    = RbsBearerCap;
  uRbsDebug        = RbsDebugFlag;

  /*
    Get Settings
    */
  e = find_entry("RbsAnswerDelay");
  if (e->found) {
    uRbsAnswerDelay = e->vi;
  }

  e = find_entry("RbsTrunkMode");
  if (e->found) {
    switch (e->vi) {
      case 0:
        RbsTrunkMode = 0x00; /* Wink Start */
        break;
      case 1:
        RbsTrunkMode = 0x04; /* Loop Start */
        break;
      case 2:
        RbsTrunkMode = 0x08; /* Ground Start */
        break;
    }
  }

  e = find_entry("RbsDialType");
  if (e->found) {
    switch (e->vi) {
      case 0:
        RbsDialType = 0x00; /* Pulse */
        break;
      case 1:
        RbsDialType = 0x10; /* DTMF */
        break;
      case 2:
        RbsDialType = 0x20; /* MF */
        break;
    }
  }

  e = find_entry("RbsBearerCap");
  if (e->found) {
    switch (e->vi) {
      case 4: /* voice */
        uRbsBearerCap = e->vi;
        break;
      case 8: /* data */
        uRbsBearerCap = e->vi;
        break;
      default:
        DBG_ERR(("A: ignore invalid RbsBearerCap = %d", e->vi))
    }
  }

  e = find_entry("RbsDigitTimeout");
  if (e->found) {
    uRbsDigitTimeout = e->vi;
  }

  e = find_entry("RbsDID");
  if (e->found) {
    bRbsDid = (int)e->vi;
  }

  e = find_entry("RbsGlareResolve");
  if (e->found) {
    bRbsGlareResolve = (int)e->vi;
  }

  e = find_entry("RbsDebug");
  if (e->found) {
    uRbsDebug = e->vi;
  }

  /*
    Adjust Settings
    */
  RbsOptions      = bRbsGlareResolve ? DIVA_RBS_GLARE_RESOLVE_PATY_BIT : 0 ;
  RbsOptions     |= bRbsDid ? DIVA_RBS_DIRECT_INWARD_DIALING_BIT : 0 ;
  RbsAnswerDelay  = uRbsAnswerDelay ;
  RbsDigitTimeout = uRbsDigitTimeout ;
  RbsBearerCap    = uRbsBearerCap ? (byte)uRbsBearerCap : RbsBearerCap ;
  RbsDebugFlag    = uRbsDebug ? uRbsDebug : RbsDebugFlag ;
  RbsOptions     |= RbsDialType;
  RbsOptions     |= RbsTrunkMode;

  e = find_entry("RbsOfficeType");
  if (e->found && e->vi) {
    RbsOptions     |= DIVA_RBS_SAX_SAO_BIT;
  }
  e = find_entry("RbsAnswSw");
  if (e->found && e->vi) {
    RbsOptions     |= DIVA_RBS_NO_ANSWER_SUPERVISION;
  }

  /*
    Store settings
    */
  e = find_entry("1spid");
  e->found = 2;
  Spid1 = (byte*)&e->vs[0];

  i = 0 ;
  Spid1[++i] = (unsigned char)RbsAnswerDelay ;
  Spid1[++i] = RbsOptions ;
  Spid1[++i] = (unsigned char)RbsDigitTimeout ;
  Spid1[++i] = RbsBearerCap ;
  Spid1[++i] = (unsigned char)RbsDebugFlag, RbsDebugFlag>>=8 ;
  Spid1[++i] = (unsigned char)RbsDebugFlag, RbsDebugFlag>>=8 ;
  Spid1[++i] = (unsigned char)RbsDebugFlag, RbsDebugFlag>>=8 ;
  Spid1[++i] = (unsigned char)RbsDebugFlag, RbsDebugFlag>>=8 ;
  Spid1[0]   = (unsigned char)i ;  /* size */
  DBG_PRV0(("GetRbsConfig - Configure %d Bytes", (int)Spid1[0]))
}

static void diva_cfg_check_obsolete_card_type_option (void) {
  diva_cmd_line_parameter_t* e;
  char tmp[8];
  int i;

  for (i = 1; i < 33; i++) {
    sprintf (tmp, "y%d", i);
    e = find_entry (tmp);
    if (e && e->found) {
      int ret = 1;

      card_number = i;
      card_ordinal = divas_get_card (card_number);

      switch (card_ordinal) {
        case CARDTYPE_DIVASRV_P_30M_PCI: /* PRI CARDS */
        case CARDTYPE_DIVASRV_P_30M_V2_PCI:
        case CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI:
        case CARDTYPE_DIVASRV_P_2M_PCI:
        case CARDTYPE_DIVASRV_P_9M_PCI:
          fprintf (VisualStream, "0\n");
          ret = 0;
          break;

        case CARDTYPE_MAESTRA_PCI: /* BRI CARD */
          fprintf (VisualStream, "1\n");
          ret = 0;
          break;

        case CARDTYPE_DIVASRV_Q_8M_PCI: /* QBRI CARD */
        case CARDTYPE_DIVASRV_VOICE_Q_8M_PCI:
          fprintf (VisualStream, "2\n");
          ret = 0;
          break;

        default:
          fprintf (VisualStream, "255\n");
      }
      exit (ret);
    }
  }
}

static int diva_configure_4bri_rev_1_8 (int revision, int tasks) {
  dword sdram_length = MQ_MEMORY_SIZE, features, protocol_features;
  byte* sdram;
  int p_fd[MQ_INSTANCE_COUNT], dsp_fd[2], fpga_fd = -1, i, protocol[MQ_INSTANCE_COUNT];
  int load_offset;
  int reentrant_protocol = 0;
  dsp_fd[0] = dsp_fd[1] = -1;

  if (revision) {
    sdram_length = (tasks == 1) ? BRI2_MEMORY_SIZE : MQ2_MEMORY_SIZE;
  }
  for (i = 0; i < MQ_INSTANCE_COUNT; i++) {
    p_fd[i] = -1;
    protocol[i] = -1;
  }

  if (!(sdram = malloc (sdram_length))) {
    DBG_ERR(("Van not allocate %ld bytes of memory", sdram_length))
    return (-1);
  }
  memset (sdram, 0x00, sdram_length);

  if (diva_cfg_adapter_handle) {
    diva_entity_link_t* link = diva_cfg_adapter_handle;

    for (i = 0; i < tasks; i++) {
      if (!link) {
        free (sdram);
        for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
        DBG_LOG(("Wrong number of physical interfaces"))
        return (-1);
      } else {
        const byte* file_name, *archive_name;
        dword file_name_length, archive_name_length;
        const byte* src = (const byte*)&link[1];

        if (i == 0) {
          const char* var_name = "ProtocolImageVersion";
          dword var_value = 0;

          if (diva_cfg_find_named_value (src, (const byte*)var_name, strlen(var_name), &var_value) == 0) {
            reentrant_protocol = var_value != 0;
          }
        }

        if (diva_cfg_get_image_info (src,
                                     DivaImageTypeProtocol,
                                     &file_name, &file_name_length,
                                     &archive_name, &archive_name_length)) {
          if (i == 0 || (i != 0 && reentrant_protocol == 0)) {
            free (sdram);
            for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) {
              close (p_fd[i]);
            }
            DBG_ERR(("Can not retrieve protocol image name"))
            return (-1);
          } else {
            p_fd[i] = -1;
          }
        } else {
          char protocol_image_name[file_name_length+1];
          memcpy (protocol_image_name, file_name, file_name_length);
          protocol_image_name[file_name_length] = 0;

          if ((p_fd[i] = open (protocol_image_name, O_RDONLY, 0)) < 0) {
            free (sdram);
            for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
            DBG_ERR(("Can not open protocol image: (%s)", protocol_image_name))
            return (-1);
          }
        }
        link = diva_q_get_next (link);
      }
    }

    if (card_ordinal == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI) {

    } else if (card_ordinal == CARDTYPE_DIVASRV_B_2F_PCI) {
      const byte* file_name, *archive_name;
      dword file_name_length, archive_name_length;
      const byte* src = (const byte*)&diva_cfg_adapter_handle[1];

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeSDP1,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram);
        for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
        DBG_ERR(("Can not retrieve sdp 1 image name"))
        return (-1);
      } else {
        char sdp_image_name[file_name_length+1];
        memcpy (sdp_image_name, file_name, file_name_length);
        sdp_image_name[file_name_length] = 0;

        if ((dsp_fd[0] = open (sdp_image_name, O_RDONLY, 0)) < 0) {
          for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
          free (sdram);
          DBG_ERR(("Can not open sdp image: (%s)", sdp_image_name))
          return (-1);
        }
      }

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeSDP2,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram);
        for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
        DBG_ERR(("Can not retrieve sdp 2 image name"))
        return (-1);
      } else {
        char sdp_image_name[file_name_length+1];
        memcpy (sdp_image_name, file_name, file_name_length);
        sdp_image_name[file_name_length] = 0;

        if ((dsp_fd[1] = open (sdp_image_name, O_RDONLY, 0)) < 0) {
          free (sdram);
          for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
          close (dsp_fd[0]);
          DBG_ERR(("Can not open sdp image: (%s)", sdp_image_name))
          return (-1);
        }
      }

    } else {
      const byte* src = (const byte*)&diva_cfg_adapter_handle[1];
      const byte* file_name, *archive_name;
      dword file_name_length, archive_name_length;

      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeDsp,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram);
        for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
        DBG_ERR(("Can not retrieve dsp image name"))
        return (-1);
      } else {
        char dsp_image_name[file_name_length+1];
        memcpy (dsp_image_name, file_name, file_name_length);
        dsp_image_name[file_name_length] = 0;

        if ((dsp_fd[0] = open (dsp_image_name, O_RDONLY, 0)) < 0) {
          free (sdram);
          for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
          DBG_ERR(("Can not open dsp image: (%s)", dsp_image_name))
          return (-1);
        }
      }
    }

    {
      const byte* src = (const byte*)&diva_cfg_adapter_handle[1];
      const byte* file_name, *archive_name;
      dword file_name_length, archive_name_length;

      /*
        Get FPGA image
        */
      if (diva_cfg_get_image_info (src,
                                   DivaImageTypeFPGA,
                                   &file_name, &file_name_length,
                                   &archive_name, &archive_name_length)) {
        free (sdram);
        for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
        for (i = 0; ((i < sizeof(dsp_fd)/sizeof(dsp_fd[0])) && (dsp_fd[i] >= 0)); i++) { close (dsp_fd[i]); }
        DBG_ERR(("Can not retrieve FPGA image name"))
        return (-1);
      } else {
        char fpga_image_name[file_name_length+1];
        memcpy (fpga_image_name, file_name, file_name_length);
        fpga_image_name[file_name_length] = 0;

        if ((fpga_fd = open (fpga_image_name, O_RDONLY, 0)) < 0) {
          free (sdram);
          for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
          for (i = 0; ((i < sizeof(dsp_fd)/sizeof(dsp_fd[0])) && (dsp_fd[i] >= 0)); i++) { close (dsp_fd[i]); }
          DBG_ERR(("Can not open fpga image: (%s)", fpga_image_name))
          return (-1);
        }
      }
    }
  } else {
    char image_name[MQ_INSTANCE_COUNT][2048], protocol_image_name[MQ_INSTANCE_COUNT][2048];
    diva_cmd_line_parameter_t* e[MQ_INSTANCE_COUNT];
    int diva_uses_dmlt_protocol[MQ_INSTANCE_COUNT];
    const char* protocol_suffix = (revision) ? ".2q" : ".qm";



  memset (diva_uses_dmlt_protocol, 0x00, sizeof(diva_uses_dmlt_protocol));
  memset (e, 0x00, sizeof(e));
  for (i = 0; i < tasks; i++) {
    protocol[i] = -1;
    p_fd[i]     = -1;
    strcpy (image_name[i], DATADIR);
    strcat (image_name[i], "/");
    protocol_image_name[i][0] = 0;
  }

  /*
    Get protocol file
    */

  e[0] = find_entry ("f");
  if (!e[0]->found) {
    e[0]->found = 1;
    strcpy (&e[0]->vs[0], "ETSI");
  }
  for (i = 1; i < tasks; i++) {
    char f_name[8];

    sprintf (f_name, "f%d", i);
    e[i] = find_entry (f_name);
    if (logical_adapter_separate_config) {
      if (!e[i]->found) {
        e[i]->found = 1;
        strcpy (&e[i]->vs[0], "ETSI");
      }
    } else {
      e[i]->found = 1;
      strcpy (&e[i]->vs[0], &e[0]->vs[0]);
    }
  }

  for (i = 0; i < tasks; i++) {
    protocol[i] = diva_load_get_protocol_by_name (&e[i]->vs[0]);
    if (protocol[i] < 0) {
      DBG_ERR(("A: unknown protocol (%s)", e[i]->vs))
      free (sdram);
      for (i=0;i<tasks;i++) {if(p_fd[i]>=0)close(p_fd[i]);}
      return (-1);
    }
    if (!dmlt_protocols[protocol[i]].bri) {
      free (sdram);
      DBG_ERR(("A: protocol (%s) not supported on BRI interface", e[i]->vs))
      for (i=0;i<tasks;i++) {if(p_fd[i]>=0)close(p_fd[i]);}
      return (-1);
    }
    if (dmlt_protocols[protocol[i]].multi & DMLT_4BRI) {
      strcat (image_name[i], "te_dmlt");
      diva_uses_dmlt_protocol[i] = 1;
    } else {
      diva_uses_dmlt_protocol[i] = 0;
      strcat (image_name[i], dmlt_protocols[protocol[i]].image);
    }
    strcat (image_name[i], protocol_suffix);
    sprintf (protocol_image_name[i], "%s%d", image_name[i], i);

    if ((p_fd[i] = open (protocol_image_name[i], O_RDONLY, 0)) < 0) {
      if (dmlt_protocols[protocol[i]].multi) {
        if (dmlt_protocols[protocol[i]].secondary_dmlt_image) {
          DBG_TRC(("Use secondary dmlt protocol image: (%s)",
                    dmlt_protocols[protocol[i]].secondary_dmlt_image))
          strcpy (image_name[i], DATADIR);
          strcat (image_name[i], "/");
          strcat (image_name[i],
                  dmlt_protocols[protocol[i]].secondary_dmlt_image);
          strcat (image_name[i], protocol_suffix);
          sprintf(protocol_image_name[i], "%s%d", image_name[i], i);
          p_fd[i] = open (protocol_image_name[i], O_RDONLY, 0);
        }

        if (p_fd[i] < 0) {
          strcpy (image_name[i], DATADIR);
          strcat (image_name[i], "/");
          strcat (image_name[i], dmlt_protocols[protocol[i]].image);
          strcat (image_name[i], protocol_suffix);
          sprintf(protocol_image_name[i], "%s%d", image_name[i], i);
          p_fd[i] = open (protocol_image_name[i], O_RDONLY, 0);
          diva_uses_dmlt_protocol[i] = 0;
        }
      }
    }

    if (p_fd[i] < 0) {
      free (sdram);
      for (i= 0;i<tasks;i++) {if(p_fd[i]>=0)close(p_fd[i]);}
      DBG_ERR(("A: can't open protocol image: (%s)", protocol_image_name))
      return (-1);
    }

    if (diva_uses_dmlt_protocol[i]) {
      diva_cmd_line_parameter_t* etmp;
      char pv_name[24];
      if (i) {
        sprintf (pv_name, "ProtVersion%d", i);
      } else {
        strcpy (pv_name, "ProtVersion");
      }
      etmp = find_entry (pv_name);
      etmp->found = 1;
      etmp->vi = (byte)(((byte)(dmlt_protocols[protocol[i]].id)) | 0x80);
    }
  }

#ifdef __DIVA_UM_CFG_4BRI_CHECK_FOR_DIFFERENT_IMAGES__ /* { */
  /*
    Due to bug in protocol code different images can't be loaded to
    diva card. After this bug is fixed this check will be removed
    */
  for (i = 1; i < tasks; i++) {
    if ((diva_uses_dmlt_protocol[0] != diva_uses_dmlt_protocol[i]) ||
        strcmp (image_name[0], image_name[i])) {
      free (sdram);
      DBG_ERR(("A: can't load different protocol images: (%s)-(%s)", \
               protocol_image_name[0], protocol_image_name[i]))
      for (i = 0; i < tasks; i++) { close (p_fd[i]); }
      return (-1);
    }
  }
#endif /* } */

  strcpy (image_name[0], DATADIR);
  strcat (image_name[0], "/");
  if (revision) {
    if (tasks == 1) {
      if (card_ordinal == CARDTYPE_DIVASRV_B_2F_PCI       ||
          card_ordinal == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI    ) {
        strcat (image_name[0], "dsbri2f.bit");
      } else {
        strcat (image_name[0], "dsbri2m.bit");
      }
    } else {
      strcat (image_name[0], "ds4bri2.bit");
    }
  } else {
    strcat (image_name[0], "ds4bri.bit");
  }
  if ((fpga_fd = open (image_name[0], O_RDONLY, 0)) < 0) {
    free (sdram);
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    DBG_ERR(("A: can't open fpga image: (%s)", image_name[0]))
    return (-1);
  }


  if (card_ordinal == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI) {

  } else if (card_ordinal == CARDTYPE_DIVASRV_B_2F_PCI) {
    strcpy (image_name[0], DATADIR);
    strcat (image_name[0], "/");
    strcat (image_name[0], DIVA_BRI2F_SDP_1_NAME);
    if ((dsp_fd[0] = open (image_name[0], O_RDONLY, 0)) < 0) {
      DBG_TRC(("I: can't open dsp image: (%s)", image_name[0]))
    }
    strcpy (image_name[0], DATADIR);
    strcat (image_name[0], "/");
    strcat (image_name[0], DIVA_BRI2F_SDP_2_NAME);
    if ((dsp_fd[1] = open (image_name[0], O_RDONLY, 0)) < 0) {
      DBG_TRC(("I: can't open dsp image: (%s)", image_name[0]))
    }
    if (dsp_fd[0] < 0) {
      dsp_fd[0] = dsp_fd[1];
      dsp_fd[1] = -1;
    }

  } else {
    strcpy (image_name[0], DATADIR);
    strcat (image_name[0], "/");
    strcat (image_name[0], "dspdload.bin");
    if ((dsp_fd[0] = open (image_name[0], O_RDONLY, 0)) < 0) {
      free (sdram);
      for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
      close (fpga_fd);
      DBG_ERR(("A: can't open dsp image: (%s)", image_name[0]))
      return (-1);
    }
  }

  }

  if (divas_set_protocol_code_version (card_number, reentrant_protocol) != 0 &&
      reentrant_protocol != 0) {
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    if (dsp_fd[0] >= 0) close (dsp_fd[0]);
    if (dsp_fd[1] >= 0) close (dsp_fd[1]);
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("Failed to set protocol code version. Incompatible XDI driver"))
    return (-1);
  }

  if ((i = lseek (fpga_fd, 0, SEEK_END)) <= 0) {
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    if (dsp_fd[0] >= 0)
      close (dsp_fd[0]);
    if (dsp_fd[1] >= 0)
      close (dsp_fd[1]);
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: can't get fpga image length"))
    return (-1);
  }
  lseek (fpga_fd, 0, SEEK_SET);
  if (read (fpga_fd, sdram, i) != i) {
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    if (dsp_fd[0] >= 0)
      close (dsp_fd[0]);
    if (dsp_fd[1] >= 0)
      close (dsp_fd[1]);
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: can't read fpga image"))
    return (-1);
  }

  if (divas_write_fpga (card_number,
                        0,
                        sdram,
                        i)) {
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    if (dsp_fd[0] >= 0)
      close (dsp_fd[0]);
    if (dsp_fd[1] >= 0)
      close (dsp_fd[1]);
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: fpga write failed"))
    return (-1);
  }
  memset (sdram, 0x00, sdram_length);

  if (divas_start_bootloader (card_number)) {
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    if (dsp_fd[0] >= 0)
      close (dsp_fd[0]);
    if (dsp_fd[1] >= 0)
      close (dsp_fd[1]);
    close (fpga_fd);
    free (sdram);
    DBG_ERR(("A: can't start bootloader"))
    return (-1);
  }

  if ((load_offset = divas_4bri_create_image (revision,
                                             sdram,
                                             p_fd,
                                             &dsp_fd[0],
                                             fpga_fd,
            diva_cfg_adapter_handle ? "" : dmlt_protocols[protocol[0]].name,
            diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol[0]].id,
            diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol[1]].id,
            diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol[2]].id,
            diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol[3]].id,
                                             &protocol_features,
                                             tasks,
                                             reentrant_protocol)) < 0) {
    const char* error_string = "unknown error";
    switch (load_offset) {
      case -1:
        error_string = "not enough memory";
        break;
      case -2:
        error_string = "can't load protocol file";
        break;
      case -3:
        error_string = "can't load dsp file";
        break;
    }
    DBG_ERR(("A: %s", error_string))
    free (sdram);
    close (fpga_fd);
    if (dsp_fd[0] >= 0)
      close (dsp_fd[0]);
    if (dsp_fd[1] >= 0)
      close (dsp_fd[1]);
    for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
    return (-1);
  }
  for (i = 0; ((i < MQ_INSTANCE_COUNT) && (p_fd[i] >= 0)); i++) { close (p_fd[i]); }
  if (dsp_fd[0] >= 0)
    close (dsp_fd[0]);
  if (dsp_fd[1] >= 0)
    close (dsp_fd[1]);
  close (fpga_fd);

  if (divas_ram_write (card_number,
                       0,
                       sdram+load_offset,
                       load_offset,
                       sdram_length - load_offset - 1)) {
    DBG_ERR(("A: can't write card memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }

  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number, 0x00000000, features)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_configure_analog_adapter (int instances) {
  dword sdram_length = MQ2_MEMORY_SIZE, features, protocol_features;
  int dsp_fd = -1, protocol_fd = -1, fpga_fd = -1, load_offset;
  byte* sdram = 0;

  if (!(sdram = malloc (sdram_length))) {
    DBG_ERR(("Failed to alloc adapter memory: %ld bytes", sdram_length))
    return (-1);
  }

  memset (sdram, 0x00, sdram_length);

  if (diva_cfg_adapter_handle) {
    const byte* file_name, *archive_name, *src = (byte*)&diva_cfg_adapter_handle[1];
    dword file_name_length, archive_name_length;

    /*
      Get protocol name from CfgLib
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeProtocol,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      DBG_ERR(("Can not retrieve protocol image name"))
      return (-1);
    } else {
      char protocol_image_name[file_name_length+1];
      memcpy (protocol_image_name, file_name, file_name_length);
      protocol_image_name[file_name_length] = 0;

      if ((protocol_fd = open (protocol_image_name, O_RDONLY, 0)) < 0) {
        DBG_ERR(("Can not open protocol image: (%s)", protocol_image_name))
        free (sdram);
        return (-1);
      }
    }

    /*
      Get DSP image name
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeDsp,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (protocol_fd);
      DBG_ERR(("Can not retrieve dsp image name"))
      return (-1);
    } else {
      char dsp_image_name[file_name_length+1];
      memcpy (dsp_image_name, file_name, file_name_length);
      dsp_image_name[file_name_length] = 0;

      if ((dsp_fd = open (dsp_image_name, O_RDONLY, 0)) < 0) {
        close (protocol_fd);
        free (sdram);
        DBG_ERR(("Can not open dsp image: (%s)", dsp_image_name))
        return (-1);
      }
    }

    /*
      Get FPGA image
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeFPGA,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (protocol_fd);
      close (dsp_fd);
      DBG_ERR(("Can not retrieve FPGA image name"))
      return (-1);
    } else {
      char fpga_image_name[file_name_length+1];
      memcpy (fpga_image_name, file_name, file_name_length);
      fpga_image_name[file_name_length] = 0;

      if ((fpga_fd = open (fpga_image_name, O_RDONLY, 0)) < 0) {
        close (protocol_fd);
        close (dsp_fd);
        free (sdram);
        DBG_ERR(("Can not open fpga image: (%s)", fpga_image_name))
        return (-1);
      }
    }
  } else {
    char image_name[2048];

    strcpy (image_name, DATADIR);
    strcat (image_name, "/te_dmlt.am");
    if ((protocol_fd = open (image_name, O_RDONLY, 0)) < 0) {
      free (sdram);
      DBG_ERR(("Failed to open protocol image: '%s'", image_name))
      return (-1);
    }
    strcpy (image_name, DATADIR);
    strcat (image_name, "/dspdload.bin");
    if ((dsp_fd = open (image_name, O_RDONLY, 0)) < 0) {
      close (protocol_fd);
      free (sdram);
      DBG_ERR(("Failed to open dsp image: '%s'", image_name))
      return (-1);
    }
    strcpy (image_name, DATADIR);
    strcat (image_name, "/dsap8.bit");
    if ((fpga_fd = open (image_name, O_RDONLY, 0)) < 0) {
      close (protocol_fd);
      close (dsp_fd);
      free (sdram);
      DBG_ERR(("Failed to open fpga image: '%s'", image_name))
      return (-1);
    }
  }

  if (divas_start_bootloader (card_number)) {
    DBG_ERR(("Failed to start adapter bootloader"))
    free (sdram);
    return (-1);
  }
  {
    int fpga_image_length = lseek (fpga_fd, 0, SEEK_END);

    if ((fpga_image_length <= 0) || (fpga_image_length >= sdram_length)) {
      close (protocol_fd);
      close (fpga_fd);
      close (dsp_fd);
      free (sdram);
      DBG_ERR(("Failed to determile length of fpga image"))
      return (-1);
    }
    if (lseek (fpga_fd, 0, SEEK_SET) < 0) {
      close (protocol_fd);
      close (fpga_fd);
      close (dsp_fd);
      free (sdram);
      DBG_ERR(("Failed to init fpga image read"))
      return (-1);
    }
    if (read (fpga_fd, sdram, fpga_image_length) != fpga_image_length) {
      close (protocol_fd);
      close (fpga_fd);
      close (dsp_fd);
      free (sdram);
      DBG_ERR(("Failed to read fpga image"))
      return (-1);
    }
    if (divas_write_fpga (card_number, 0, sdram, fpga_image_length)) {
      close (dsp_fd);
      close (protocol_fd);
      close (fpga_fd);
      free (sdram);
      DBG_ERR(("Failed to write FPGA image"))
      return (-1);
    }

    memset (sdram, 0x00, fpga_image_length);
  }

  if ((load_offset = divas_analog_create_image (instances,
                                                sdram,
                                                protocol_fd,
                                                dsp_fd,
                                                fpga_fd,
                                                "te_dmlt.am",
                                                &protocol_features)) < 0) {
    const char* error_string = "unknown error";
    switch (load_offset) {
      case -1:
        error_string = "not enough memory";
        break;
      case -2:
        error_string = "can't load protocol file";
        break;
      case -3:
        error_string = "can't load dsp file";
        break;
    }
    DBG_ERR(("Failed to create sdram image: %s", error_string))

    free (sdram);
    close (fpga_fd);
    close (protocol_fd);
    close (dsp_fd);

    return (-1);
  }

  close (protocol_fd);
  close (dsp_fd);
  close (fpga_fd);

  if (divas_ram_write (card_number,
                       0,
                       sdram+load_offset,
                       load_offset,
                       sdram_length - load_offset - 1)) {
    DBG_ERR(("Failed to swite sdram image to adapter memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("Failed to set set raw protocol features"))
  }

  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number, 0x00000000, features)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_read_xlog (void) {
  static byte tmp_xlog_buffer[4096*2];
  word tmp_xlog_pos;
  int ret, fret = 1, dontpoll = 0, print_count = 0;
  struct mi_pc_maint pcm;
  diva_cmd_line_parameter_t* e = find_entry ("FlushXlog");
  FILE* old_visual = VisualStream;
  dword events = 0;
  char vtbl[] = " +*";
  int max_entries = 400;
  struct mlog {
    short code;
    word timeh;
    word timel;
    char buffer[MIPS_BUFFER_SZ-6];
  } mwork;

  if (e->found) {
    dontpoll = 1;
  }
  e = find_entry ("File");
  if (e->found) {
    FILE* out_file = fopen (e->vs, "w");
    if (!out_file) {
      fprintf (VisualStream, "A: cant open output file (%s)\n", e->vs);
      return (-1);
    }
    VisualStream = out_file;
    if (!dontpoll)
      print_count = 1;
  }

  for (;;) {
    do {
      memset (&pcm, 0x00, sizeof(pcm));

      ret = divas_get_card_properties (card_number,
                                       "xlog",
                                       (char*)&pcm,
                                       sizeof(pcm));
      if (ret > 0 && pcm.rc == OK) {
        dword msec, sec;
        byte proc;

        *(MIPS_BUFFER*)&mwork = *(MIPS_BUFFER*)&pcm.data;

        fret = 0;
        msec = (((dword)mwork.timeh) << 16) + mwork.timel;
        sec  = msec/1000;
        proc = mwork.code >> 8;

        if (proc) {
          fprintf (VisualStream, "%5u:%04d:%03d - P(%d)",
                   sec / 3600, sec % 3600, msec % 1000, proc) ;

        } else {
          fprintf (VisualStream, "%5u:%04d:%03d - ",
                   sec / 3600, sec % 3600, msec % 1000) ;
        }

        switch (mwork.code & 0xff) {
          case 1:
            fprintf (VisualStream, "%s\n", mwork.buffer);
            break;

          case 2:
            tmp_xlog_buffer[0] = 0;
            tmp_xlog_pos = xlog(&tmp_xlog_buffer[0], mwork.buffer);
            tmp_xlog_buffer[tmp_xlog_pos] = 0;
            fprintf(VisualStream, "%s", &tmp_xlog_buffer[0]);
            break;

          default:
            fprintf (VisualStream,
                     "ERROR: unknown log_code 0x%02X\n",
                     mwork.code & 0xFF) ;
        }
        fflush (VisualStream);
        if (dontpoll) {
          max_entries--;
        }
        if (print_count) {
          events++;
          fprintf (old_visual, "%c %d\r", vtbl[events%3], events);
          fflush (old_visual);
        }
      }
    } while ((ret > 0) && (pcm.rc == OK) && max_entries);

    if (ret < 0 || dontpoll || (!max_entries)) {
      break;
    }
    usleep (10000);
  }

  return (fret);
}

/*
  Configure DIVA BRI Rev. 1

  Protocol code selection for BRI Rev.1 card
  Universal:     - use V4 code + dspdload.bin (default)
  Ras optimized: - use new code + dspdvmdm.bin (DSP_MDM_TELINDUS_FILE)
  Fax optimized: - use new code + dspdvfax.bin (DSP_FAX_TELINDUS_FILE)
  V4 code does have suffix "sm.4"
  */
static int diva_configure_bri_rev_1_2 (int revision) {
  dword sdram_length = BRI_MEMORY_SIZE + BRI_SHARED_RAM_SIZE;
  byte* sdram;
  int protocol = 0, fd = -1, dsp_fd = -1;
  int load_offset;
  dword features, protocol_features, max_download_address = 0;

  sdram = malloc (sdram_length);

  if (!sdram) {
    DBG_ERR(("A: can't alloc %ld bytes of memory"))
    return (-1);
  }
  memset (sdram, 0x00, sdram_length);

  if (diva_cfg_adapter_handle) {
    const byte* file_name, *archive_name, *src = (byte*)&diva_cfg_adapter_handle[1];
    dword file_name_length, archive_name_length;

    /*
      Get protocol name from CfgLib
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeProtocol,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      DBG_ERR(("Can not retrieve protocol image name"))
      return (-1);
    } else {
      char protocol_image_name[file_name_length+1];
      memcpy (protocol_image_name, file_name, file_name_length);
      protocol_image_name[file_name_length] = 0;

      if ((fd = open (protocol_image_name, O_RDONLY, 0)) < 0) {
        DBG_ERR(("Can not open protocol image: (%s)", protocol_image_name))
        free (sdram);
        return (-1);
      }
    }

    /*
      Get DSP image name
      */
    if (diva_cfg_get_image_info (src,
                                 DivaImageTypeDsp,
                                 &file_name, &file_name_length,
                                 &archive_name, &archive_name_length)) {
      free (sdram);
      close (fd);
      DBG_ERR(("Can not retrieve dsp image name"))
      return (-1);
    } else {
      char dsp_image_name[file_name_length+1];
      memcpy (dsp_image_name, file_name, file_name_length);
      dsp_image_name[file_name_length] = 0;

      if ((dsp_fd = open (dsp_image_name, O_RDONLY, 0)) < 0) {
        close (fd);
        free (sdram);
        DBG_ERR(("Can not open dsp image: (%s)", dsp_image_name))
        return (-1);
      }
    }
  } else {
    diva_cmd_line_parameter_t* e;
    char image_name[2048];
    const char* protocol_suffix = ".sm.4";
    int use_v6_protocol, diva_uses_dmlt_protocol;

  /*
    Get protocol file
    */
  e = find_entry ("f");
  if (e->found) {
    protocol = diva_load_get_protocol_by_name (&e->vs[0]);
  } else {
    protocol = diva_load_get_protocol_by_name ("ETSI");
  }
  if (protocol < 0) {
    DBG_ERR(("A: unknown protocol"))
    free (sdram);
    return (-1);
  }

  if (!dmlt_protocols[protocol].bri) {
    DBG_ERR(("A: protocol not supported on BRI interface"))
    free (sdram);
    return (-1);
  }

  strcpy (image_name, DATADIR);
  strcat (image_name, "/");

  e = find_entry ("vb6");
  if (find_entry ("vd6")->found) {
    e->found = 1;
  }
  use_v6_protocol =\
         (e->found || (!strcmp (dmlt_protocols[protocol].image, "te_dmlt")));
  if (use_v6_protocol) {
    protocol_suffix = ".sm";
    if (dmlt_protocols[protocol].multi & DMLT_BRI) {
      strcat (image_name, "te_dmlt");
      diva_uses_dmlt_protocol = 1;
    } else {
      diva_uses_dmlt_protocol = 0;
      strcat (image_name, dmlt_protocols[protocol].image);
    }
  } else {
    diva_uses_dmlt_protocol = 0;
    strcat (image_name, dmlt_protocols[protocol].image);
  }
  strcat (image_name, protocol_suffix);

  fd = open (image_name, O_RDONLY, 0);
  if ((fd < 0) && diva_uses_dmlt_protocol) {
    if (dmlt_protocols[protocol].secondary_dmlt_image) {
      DBG_TRC(("Use secondary dmlt protocol image: (%s)",
                dmlt_protocols[protocol].secondary_dmlt_image))
      strcpy (image_name, DATADIR);
      strcat (image_name, "/");
      strcat (image_name, dmlt_protocols[protocol].secondary_dmlt_image);
      strcat (image_name, protocol_suffix);
      fd = open (image_name, O_RDONLY, 0);
    }
    if (fd < 0) {
      diva_uses_dmlt_protocol = 0;
      strcpy (image_name, DATADIR);
      strcat (image_name, "/");
      strcat (image_name, dmlt_protocols[protocol].image);
      strcat (image_name, protocol_suffix);
      fd = open (image_name, O_RDONLY, 0);
    }
  }
  if (fd < 0) {
    DBG_ERR(("A: can't open protocol image: (%s)", image_name))
    free (sdram);
    return (-1);
  }
  DBG_LOG(("used protocol image: (%s), dmlt=%d, v6=%d", \
          image_name, diva_uses_dmlt_protocol, use_v6_protocol))

  strcpy (image_name, DATADIR);
  strcat (image_name, "/");

  if (use_v6_protocol) {
    if (find_entry ("vd6")->found) {
      strcat (image_name, DSP_FAX_TELINDUS_FILE);
    } else {
      strcat (image_name, DSP_MDM_TELINDUS_FILE);
    }
  } else {
    strcat (image_name, "dspdload.bin");
  }

  if ((dsp_fd = open (image_name, O_RDONLY, 0)) < 0) {
    DBG_ERR(("A: can't open dsp image: (%s)", image_name))
    free (sdram);
    close (fd);
    return (-1);
  }

  DBG_LOG(("used dsp image: (%s)", image_name))

  if (diva_uses_dmlt_protocol) {
    diva_cmd_line_parameter_t* e = find_entry ("ProtVersion");
    e->found = 1;
    e->vi = (byte)(((byte)(dmlt_protocols[protocol].id)) | 0x80);
  }

  }

  if (divas_start_bootloader (card_number)) {
    close (fd);
    close (dsp_fd);
    free (sdram);
    DBG_ERR(("A: can't start bootloader"))
    return (-1);
  }

  if ((load_offset = divas_bri_create_image (sdram,
                                             fd,
                                             dsp_fd,
                  diva_cfg_adapter_handle ? "" : dmlt_protocols[protocol].name,
                  diva_cfg_adapter_handle ? -1 : dmlt_protocols[protocol].id,
                                             &protocol_features,
                                             &max_download_address)) < 0) {
    const char* error_string = "unknown error";
    switch (load_offset) {
      case -1:
        error_string = "not enough memory";
        break;
      case -2:
        error_string = "can't load protocol file";
        break;
      case -3:
        error_string = "can't load dsp file";
        break;
    }
    DBG_ERR(("A: %s", error_string))
    free (sdram);
    close (fd);
    close (dsp_fd);
    return (-1);
  }

  close (fd);
  close (dsp_fd);

  /*
    Write protocol code image + dsp image
    */
  if (divas_ram_write (card_number,
                       0,
                       sdram+load_offset,
                       load_offset,
                       max_download_address)) {
    DBG_ERR(("A: can't write card memory"))
    free (sdram);
    return (-1);
  }

  /*
    Now write second segment with rest of data
    */
  if (divas_ram_write (card_number,
                       0,
                       sdram+BRI_MEMORY_SIZE,
                       (dword)(SHAREDM_SEG << 16),
                       BRI_SHARED_RAM_SIZE-1)) {
    DBG_ERR(("A: can't write memory"))
    free (sdram);
    return (-1);
  }

  free (sdram);

  DBG_TRC(("Raw protocol features: %08x", protocol_features))
  if (divas_set_protocol_features (card_number, protocol_features)) {
    DBG_TRC(("W: Can not set raw protocol features"))
  }

  features  = CardProperties[card_ordinal].Features;
  features |= protocol_features2idi_features (protocol_features);

  if (divas_start_adapter (card_number, 0x00000000, features)) {
    DBG_ERR(("A: adapter start failed"))
    return (-1);
  }

  return (0);
}

static int diva_reset_card (void) {
  char property[256];

  if (divas_get_card_properties (card_number,
                                 "serial number",
                                 property,
                                 sizeof(property)) <= 0) {
    strcpy (property, "0");
  }
  fprintf (VisualStream,
           "Reset adapter Nr:%d - '%s', SN: %s ...",
           card_number,
           CardProperties[card_ordinal].Name,
           property);
  fflush (VisualStream);

  if (divas_start_bootloader (card_number)) {
    if (divas_get_card_properties (card_number,
                                   "card state",
                                   property,
                                   sizeof(property)) <= 0) {
      strcpy (property, "unknown");
    }
    fprintf (VisualStream, " FAILED\nAdapter state is: %s\n", property);
    return (1);
  }
  fprintf (VisualStream, " OK\n");

  return (0);
}

static int diva_create_core_dump_image (void) {
  diva_cmd_line_parameter_t* e = find_entry ("File");
  FILE* out_file;
  const char* file_name;
  dword sdram_length = 0;
  byte* sdram;

  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_P_30M_PCI:
      sdram_length = MP_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_P_30M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI:
      sdram_length = MP2_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_V_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      sdram_length = MQ2_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_Q_8M_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_PCI:
      sdram_length = MQ_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_B_2F_PCI:
    case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
      sdram_length = BRI2_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_P_30M_V30_PCI:
    case CARDTYPE_DIVASRV_P_24M_V30_PCI:
    case CARDTYPE_DIVASRV_P_8M_V30_PCI:
    case CARDTYPE_DIVASRV_P_30V_V30_PCI:
    case CARDTYPE_DIVASRV_P_24V_V30_PCI:
    case CARDTYPE_DIVASRV_P_2V_V30_PCI:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_30M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_2V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCIE:
      sdram_length = MP2_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_2P_V_V10_PCI:
    case CARDTYPE_DIVASRV_2P_M_V10_PCI:
    case CARDTYPE_DIVASRV_4P_V_V10_PCI:
    case CARDTYPE_DIVASRV_4P_M_V10_PCI:
    case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
    case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
      sdram_length = MP2_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
      sdram_length = MP2_MEMORY_SIZE;
      break;
  }

  if (!sdram_length) {
    fprintf (VisualStream,
             "Core dump not supported for '%s'\n",
             CardProperties[card_ordinal].Name);
    return (1);
  }

  if (!(sdram = (byte*)malloc (sdram_length))) {
    fprintf (VisualStream,
             "FAILED: Can't allocate %u bytes of memory\n",
             sdram_length);
    return (1);
  }

  if (e->found) {
    file_name = e->vs;
  } else {
    file_name = "diva_core";
  }

  if (!(out_file = fopen (file_name, "wb"))) {
    fprintf (VisualStream,
             "FAILED: Can't create file '%s', errno=%d\n",
             file_name, errno);
    free (sdram);
    return (1);
  }

  if (divas_ram_read (card_number, 0, sdram, 0, sdram_length - 8)) {
    free (sdram);
    fclose (out_file);
    unlink (file_name);
    fprintf (VisualStream, "FAILED: Can't read card\n");
    return (1);
  }

  if (fwrite (sdram, 1, sdram_length, out_file) != sdram_length) {
    free (sdram);
    fclose (out_file);
    unlink (file_name);
    fprintf (VisualStream,
             "FAILED: Can't write file '%s', errno=%d\n",
             file_name, errno);
    return (1);
  }

  free (sdram);
  fclose (out_file);

  return (0);
}


static int diva_recovery_maint_buffer (void) {
  diva_cmd_line_parameter_t* e = find_entry ("File");
  FILE* out_file;
  const char* file_name;
  dword sdram_length = 0;
  byte* sdram;
  static byte maint_pattern[] = {
0x11, 0x47, 0x11, 0x47, 0x00, 0x08, 0x00, 0x00, 0x4b, 0x45, 0x52, 0x4e,
0x45, 0x4c, 0x20, 0x4d, 0x4f, 0x44, 0x45, 0x20, 0x42, 0x55, 0x46, 0x46,
0x45, 0x52, 0x0a, 0x00
};

  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_P_2M_PCI:
    case CARDTYPE_DIVASRV_P_9M_PCI:
    case CARDTYPE_DIVASRV_P_30M_PCI:
      sdram_length = MP_MEMORY_SIZE;
      break;

    case CARDTYPE_DIVASRV_2P_V_V10_PCI:
    case CARDTYPE_DIVASRV_2P_M_V10_PCI:
    case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
    case CARDTYPE_DIVASRV_4P_V_V10_PCI:
    case CARDTYPE_DIVASRV_4P_M_V10_PCI:
    case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
    case CARDTYPE_DIVASRV_P_30M_V30_PCI:
    case CARDTYPE_DIVASRV_P_24M_V30_PCI:
    case CARDTYPE_DIVASRV_P_8M_V30_PCI:
    case CARDTYPE_DIVASRV_P_30V_V30_PCI:
    case CARDTYPE_DIVASRV_P_24V_V30_PCI:
    case CARDTYPE_DIVASRV_P_2V_V30_PCI:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_23M_PCI:
    case CARDTYPE_DIVASRV_P_30M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI:
    case CARDTYPE_DIVASRV_P_30M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_2V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
      sdram_length = MP2_MEMORY_SIZE;
      break;

    default:
      fprintf(VisualStream, "%s", "FAILED: core dump not supported by this adapter\n");
      return (1);
  }

  if (!(sdram = (byte*)malloc (sdram_length))) {
    fprintf (VisualStream,
             "FAILED: Can't allocate %u bytes of memory\n",
             sdram_length);
    return (1);
  }

  memset (sdram, 0x00, sizeof(maint_pattern));
  if (divas_ram_read (card_number, 0, sdram, 4*1024, sizeof(maint_pattern)) != 0) {
    fprintf (VisualStream,
             "FAILED: failed to read %d bytes\n", (int)sizeof(maint_pattern));
    free (sdram);
    return (1);
  }
  if (memcmp(sdram, maint_pattern, sizeof(maint_pattern)) != 0) {
    fprintf (VisualStream, "%s",
             "FAILED: missing MAINT signature\n");
    free (sdram);
    return (1);
  }
  if (divas_ram_read (card_number, 0, sdram, 4*1024, sdram_length - 4*1024 - 8) != 0) {
    fprintf (VisualStream,
             "FAILED: failed to read %d bytes\n", (int)sizeof(maint_pattern));
    free (sdram);
    return (1);
  }


  if (e->found) {
    file_name = e->vs;
  } else {
    file_name = "trace.bin";
  }

  if (!(out_file = fopen (file_name, "wb"))) {
    fprintf (VisualStream,
             "FAILED: Can't create file '%s', errno=%d\n",
             file_name, errno);
    free (sdram);
    return (1);
  }

  if (fwrite (sdram, 1, sdram_length-4*1024, out_file) != sdram_length-4*1024) {
    free (sdram);
    fclose (out_file);
    unlink (file_name);
    fprintf (VisualStream,
             "FAILED: Can't write file '%s', errno=%d\n",
             file_name, errno);
    return (1);
  }

  free (sdram);
  fclose (out_file);

  return (0);
}

static const char* entry_name (const char* entry) {
  static char name[256];

  if (logical_adapter_separate_config && logical_adapter) {
    sprintf (name, "%s%d", entry, logical_adapter);
  } else {
    strcpy (name, entry);
  }

  return (&name[0]);
}

dword cfg_get_card_bar (int bar) {
  char property[256];
  unsigned long ret;
  char* p = 0, *ptr = &property[0];

  if (divas_get_card_properties (card_number,
                                 "pci hw config",
                                 property,
                                 sizeof(property)) <= 0) {
    DBG_ERR(("Can't get BAR[%d] for card [%d]", bar, card_number));
    return (0);
  }

  while (bar--) {
    if (!(ptr = strstr (ptr, " "))) {
      DBG_ERR(("Can't find BAR[%d] for card [%d]", bar, card_number));
      DBG_ERR(("<%s>", property))
      return (0);
    }
    ptr++;
  }

  if ((ret = strtoul (ptr, &p, 16)) == 0) {
    DBG_ERR(("Can't read BAR[%d] for card [%d]", bar, card_number));
  }

  return (ret);
}

byte cfg_get_interface_loopback (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("InterfaceLoopback"));

  if (e->found) {
    return (0x02);
  }

  return (0);
}

dword cfg_get_SuppSrvFeatures (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("SuppSrvFeatures"));

  if (e->found) {
    return (e->vi);
  }

  return (0);
}

dword cfg_get_R2Dialect (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("R2Dialect"));

  if (e->found) {
    return (e->vi);
  }

  return (0);
}

byte cfg_get_R2CtryLength (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("R2CtryLength"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

dword cfg_get_R2CasOptions (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("R2CasOptions"));

  if (e->found) {
    return (e->vi);
  }

  return (0);
}

dword cfg_get_V34FaxOptions (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("V34FaxOptions"));

  if (e->found) {
    return (e->vi);
  }

  return (0);
}

dword cfg_get_DisabledDspsMask (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("DisabledDspsMask"));

  if (e->found) {
    return (e->vi);
  }

  return (0);
}

word cfg_get_AlertTimeout (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("AlertTimeout"));

  if (e->found) {
    return ((word)e->vi/20 + ((e->vi != 0) ? 1 : 0));
  }

  return (0);
}

word cfg_get_ModemEyeSetup (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("ModemEyeSetup"));

  if (e->found) {
    return ((word)e->vi);
  }

  return (0);
}

byte cfg_get_CCBSReleaseTimer (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("CCBSReleaseTimer"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

static int diva_card_monitor (void) {
  struct mi_pc_maint pcm;

  {
    char property[256];
    char tmp_ident[256];

    if (divas_get_card_properties (card_number,
                                   "serial number",
                                   property,
                                   sizeof(property)) <= 0) {
      strcpy (property, "0");
    }
    sprintf (tmp_ident, "Card %d '%s' SN:%s ",
             card_number,
             CardProperties[card_ordinal].Name,
             property);
    openlog (tmp_ident, LOG_CONS | LOG_NDELAY, LOG_DAEMON);
  }


  if (daemon (0,0)) {
    syslog (LOG_ERR, " [%ld] %s", (long)getpid(), "Card monitor failed");
    closelog();
    exit (1);
  } else {
    if (create_pid_file (card_number, 1)) {
      syslog (LOG_NOTICE, " [%ld] %s", (long)getpid(), "Card monitor failed, can not create pid file");
      closelog();
      exit (1);
    }
    syslog (LOG_NOTICE, " [%ld] %s", (long)getpid(), "Card monitor started");
  }
  diva_monitor_run = 1;

  signal (SIGHUP,  diva_monitor_hup);
  signal (SIGTERM, diva_monitor_hup);
  signal (SIGABRT, diva_monitor_hup);
  signal (SIGQUIT, diva_monitor_hup);
  signal (SIGINT,  diva_monitor_hup);


  while (diva_monitor_run) {
    memset (&pcm, 0x00, sizeof(pcm));
    if (divas_get_card_properties (card_number,
                                   "xlog",
                                   (char*)&pcm,
                                   sizeof(pcm)) < 0) {
      diva_cmd_line_parameter_t* e = find_entry ("File");
      syslog (LOG_ERR, "%s", "Card hardware failed");
      diva_monitor_run = 0;
      if (e->found) {
        execute_user_script (e->vs);
      }
    }
    sleep (60); /* Wait 1 Minute between card checks */
  }

  syslog (LOG_NOTICE," [%ld] %s", (long)getpid(),"Card monitor terminated");
  closelog();
  create_pid_file (card_number, 0);

  signal (SIGHUP,  SIG_DFL);
  signal (SIGTERM, SIG_DFL);
  signal (SIGABRT, SIG_DFL);
  signal (SIGQUIT, SIG_DFL);
  signal (SIGINT,  SIG_DFL);

  return (0);
}

static int create_pid_file (int c, int create) {
  char name[64];

  sprintf (name, "/var/run/divascardd%d.pid", c);

  if (create) {
    FILE* fs;
    if ((fs = fopen (name, "w"))) {
      fprintf (fs, "%ld\n", (long int)getpid());
      fflush (fs);
      fclose (fs);
    } else {
      return (-1);
    }
  } else {
    unlink (name);
  }

  return (0);
}

static void diva_monitor_hup (int sig) {
  diva_monitor_run = 0;
}

static int execute_user_script (const char* application_name) {
  int status;
  char cmd_string[512];

  sprintf (cmd_string, "%s %d", application_name, card_number);
  status = system (cmd_string);
  if (status != -1) {
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == 0) {
        return (0);
      } else {
        syslog (LOG_ERR, "%s%d", "User notification failed, error=", WEXITSTATUS(status));
        return (-1);
      }
    }
  } else {
    syslog (LOG_ERR, "%s%d", "User notification failed, errno=", errno);
    return (-1);
  }

  syslog (LOG_ERR, "%s", "User notification failed");

  return (-1);
}

int diva_cfg_get_set_vidi_state (dword state) {
  if (divas_get_card_properties (card_number,
                                 "vidi_mode",
                                 (void*)&state,
                                 sizeof(dword)) <= 0) {
    return -1;
  }

  return 0;
}

int diva_cfg_get_vidi_info_struct (int task_nr, diva_xdi_um_cfg_cmd_data_init_vidi_t* p) {
  if (divas_get_card_properties (card_number + task_nr,
                                 "vidi_info",
                                 (void*)p,
                                 sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t)) <= 0) {
    return -1;
  }

  return 0;
}

int diva_cfg_get_hw_info_struct (char *hw_info, int size) {

  if (divas_get_card_properties (card_number,
                                 "hw_info_struct",
                                 hw_info, size) <= 0) {
    return -1;
  }
  return 0;
}

dword diva_cfg_get_serial_number (void) {
  char serial[64];

  if (divas_get_card_properties (card_number,
                                 "serial number",
                                 serial,
                                 sizeof(serial)) <= 0) {
    strcpy (serial, "0");
  }

  return ((dword)atol(serial));
}

static int diva_card_test (dword test_command) {
  const char* fpga_name = 0;
  int reset_adapter     = 0;
  int start_bootloader  = 0;
  byte* protocol_sdram = 0;
  int sdram_length = 0, ret;

  if (test_command & 0x100) {
    char image_name[sizeof(DATADIR)+sizeof("/te_testf.mi")+1];
    int fd;

    strcpy (image_name, DATADIR);
    switch (card_ordinal)
    {
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
      // Choose test firmware for either FPGA or IDT MIPS
      strcat (image_name, (divas_get_revision_id(card_number) >= 0x10) ?
                          "/te_testf.mi" : "/te_test.mi");
      break;

    default:
      strcat (image_name, "/te_test.mi");
      break;
    }

    if ((fd = open (image_name, O_RDONLY, 0)) < 0) {
      DBG_ERR(("Failed to open protocol image file: %s", image_name))
      return (-1);
    }
    if ((sdram_length = lseek(fd, 0, SEEK_END)) <= 0) {
      close (fd);
      DBG_ERR(("Failed to retrieve protocol image file '%s' length", image_name))
      return (-1);
    }
    if (sdram_length < 64) {
      close (fd);
      DBG_ERR(("Invalid protocol image"))
      return (-1);
    }
    if ((protocol_sdram = malloc (sdram_length)) == 0) {
      close (fd);
      DBG_ERR(("Failed to alloc %d bytes", sdram_length))
      return (-1);
    }
    lseek (fd, 0, SEEK_SET);
    if (read (fd, protocol_sdram, sdram_length) != sdram_length) {
      free (protocol_sdram);
      close (fd);
      DBG_ERR(("Failed to read protocol image data"))
      return (-1);
    }
  }


  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_P_2M_PCI:
    case CARDTYPE_DIVASRV_P_9M_PCI:
    case CARDTYPE_DIVASRV_P_30M_PCI:
    case CARDTYPE_DIVASRV_P_23M_PCI:
    case CARDTYPE_DIVASRV_P_30M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI:
      reset_adapter     = 1;
      break;

    case CARDTYPE_DIVASRV_P_30M_V30_PCI:
    case CARDTYPE_DIVASRV_P_24M_V30_PCI:
    case CARDTYPE_DIVASRV_P_8M_V30_PCI:
    case CARDTYPE_DIVASRV_P_30V_V30_PCI:
    case CARDTYPE_DIVASRV_P_24V_V30_PCI:
    case CARDTYPE_DIVASRV_P_2V_V30_PCI:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCI:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCI:
      reset_adapter     = 1;
      start_bootloader  = 1;
      fpga_name = "dspri331.bit";
      break;

    case CARDTYPE_DIVASRV_P_30M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24M_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_2V_V30_PCIE:
    case CARDTYPE_DIVASRV_P_30UM_V30_PCIE:
    case CARDTYPE_DIVASRV_P_24UM_V30_PCIE:
      reset_adapter     = 1;
      start_bootloader  = 1;
      fpga_name = "dsprie31.bit";
      break;

    case CARDTYPE_DIVASRV_Q_8M_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_PCI:
      fpga_name = "ds4bri.bit";
      break;

    case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
      fpga_name = "ds4bri2.bit";
      break;

    case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
      fpga_name = "d4bripe1.bit";
      break;

    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      // Choose core logic FPGA code for either FPGA or IDT MIPS
      fpga_name = (divas_get_revision_id(card_number) >= 0x10) ?
                  "dbripe2.bit" : "dbripe1.bit";
      break;

    case CARDTYPE_DIVASRV_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
      fpga_name = "dsbri2m.bit";
      break;

    case CARDTYPE_DIVASRV_B_2F_PCI:
    case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
      fpga_name = "dsbri2f.bit";
      break;

    case CARDTYPE_DIVASRV_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_2PORT:
      reset_adapter     = 1;
      start_bootloader  = 1;
      fpga_name = "dsap2.bit";
      break;

    case CARDTYPE_DIVASRV_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_8PORT:
      reset_adapter     = 1;
      start_bootloader  = 1;
      fpga_name = "dsap8.bit";
      break;

    case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
      reset_adapter     = 1;
      start_bootloader  = 1;
      // Choose core logic FPGA code for either FPGA or IDT MIPS
      fpga_name = (divas_get_revision_id(card_number) >= 0x10) ?
                   "dap2pe2.bit": "dap8pe1.bit"; // IDT uses 8-channel bitfile
      break;

    case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
      reset_adapter     = 1;
      start_bootloader  = 1;
      // Choose core logic FPGA code for either FPGA or IDT MIPS
      fpga_name = (divas_get_revision_id(card_number) >= 0x10) ?
                   "dap8pe2.bit": "dap8pe1.bit";
      break;

    case CARDTYPE_DIVASRV_2P_V_V10_PCI:
    case CARDTYPE_DIVASRV_2P_M_V10_PCI:
      reset_adapter     = 1;
      start_bootloader  = 0;
      fpga_name = "ds2pri100.bit";
      break;

    case CARDTYPE_DIVASRV_4P_V_V10_PCI:
    case CARDTYPE_DIVASRV_4P_M_V10_PCI:
      reset_adapter     = 1;
      start_bootloader  = 0;
      fpga_name = "ds4pri100.bit";
      break;

    case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
    /* todo */
      reset_adapter     = 1;
      start_bootloader  = 0;
      fpga_name = "dsipm300.bit";
      break;

    case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
    /* todo */
      reset_adapter     = 1;
      start_bootloader  = 0;
      fpga_name = "dsipm600.bit";
      break;

    case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM:
      reset_adapter     = 1;
      start_bootloader  = 0;
      fpga_name = "ds4pe100.bit";
      break;

    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
    case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
      reset_adapter     = 0;
      start_bootloader  = 0;
      fpga_name = 0;
      break;

    default:
      DBG_TRC(("Adapter type %d not supported by test procedure"))
      if (protocol_sdram)
        free(protocol_sdram);
      return (-1);
  }

  if (reset_adapter && diva_reset_card ()) {
    if (protocol_sdram)
      free(protocol_sdram);
    return (-1);
  }
  if (start_bootloader && divas_start_bootloader (card_number)) {
    if (protocol_sdram)
      free (protocol_sdram);
    return (-1);
  }

  if (fpga_name) {
    int   fpga_fd = -1, i;
    char  image_name[sizeof(DATADIR)+64];
    byte* sdram = 0;

    strcpy (&image_name[0], DATADIR);
    strcat (&image_name[0], "/");
    strcat (&image_name[0], fpga_name);

    if ((fpga_fd = open (&image_name[0], O_RDONLY, 0)) < 0) {
      DBG_ERR(("A: can't open fpga image: (%s)", &image_name[0]))
      return (-1);
    }
    if ((i = lseek (fpga_fd, 0, SEEK_END)) <= 0) {
      close (fpga_fd);
      if (protocol_sdram)
        free (protocol_sdram);
      DBG_ERR(("A: can't get fpga image length"))
      return (-1);
    }
    lseek (fpga_fd, 0, SEEK_SET);
    if (!(sdram = (byte*)malloc(i))) {
      DBG_ERR(("A: can't alloc fpga memory (%d)", i));
      close (fpga_fd);
      if (protocol_sdram)
        free (protocol_sdram);
      return (-1);
    }
    if (read (fpga_fd, sdram, i) != i) {
      close (fpga_fd);
      free (sdram);
      if (protocol_sdram)
        free (protocol_sdram);
      DBG_ERR(("A: can't read fpga image"))
      return (-1);
    }
    close (fpga_fd);
    if (divas_write_fpga (card_number, 0, sdram, i)) {
      free (sdram);
      if (protocol_sdram)
        free (protocol_sdram);
      DBG_ERR(("A: fpga write failed"))
      return (-1);
    }
    free (sdram);
  }

  if (protocol_sdram) {
    if (divas_ram_write (card_number,
                         0,
                         protocol_sdram,
                         0, /* initial offset */
                         sdram_length)) {
      DBG_ERR(("Failed to write protocol image"))
      free (protocol_sdram);
      return (-1);
    }
    free (protocol_sdram);
  }

  ret = divas_memory_test (card_number, test_command);

  if ((protocol_sdram != 0) && ((test_command & 0x200) == 0)) {
    diva_reset_card ();
  }

  return (ret);
}

byte cfg_get_AniDniLimiter_1 (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("AniDniLimiterOne"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);

}

byte cfg_get_AniDniLimiter_2 (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("AniDniLimiterTwo"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

byte cfg_get_AniDniLimiter_3 (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("AniDniLimiterThree"));

  if (e->found) {
    return ((byte)e->vi);
  }

  return (0);
}

byte cfg_get_DiscAfterProgress (void) {
  diva_cmd_line_parameter_t* e = find_entry (entry_name("DisableDiscAfterProgress"));

  if (e->found) {
    return (1);
  }

  return (0);
}

/*
  Read information about configuration instance and retrieve
  logical adapter number.
  In case adapter was addressed by name then it is necessary
  to find adapter with corresponding name and return his
  logical adapter number
  */
static int diva_cfg_get_logical_adapter_nr (const byte* src) {
  dword used, value, position = 0, name_length = 0;
  const byte* name = 0;
  int logical_adapter_number = 0;

  vle_to_dword (src, &used); /* tlie tag */
  position += used;
  vle_to_dword (src+position, &used); /* total length */
  position += used;

  value = vle_to_dword (src+position, &used); /* instance handle type */
  position += used;

  switch (value) {
    case VieInstance2:
      name_length = diva_get_bs  (src+position, &name);
      break;

    default:
      logical_adapter_number = vle_to_dword (src+position, &used);
      break;
  }

  if (name) {
    char tmp[name_length+1];

    memcpy (tmp, name, name_length);
    tmp[name_length] = 0;

    DBG_TRC(("Adapter name: '%s'", tmp))
  }

  return (logical_adapter_number);
}

const byte* diva_cfg_get_ram_init (dword* length) {
  diva_entity_link_t* link = diva_cfg_adapter_handle;
  const byte* ret = 0, *ram_section, *src;
  int nr = logical_adapter;

  while (nr && link) {
    link = diva_q_get_next (link);
    nr--;
  }

  if (!link || nr) {
    DBG_ERR(("Wrong number of physical interfaces"))
    return (0);
  }

  src = (byte*)&link[1];

  if ((ram_section = diva_cfg_get_next_ram_init_section (src, 0))) {
    *length = diva_cfg_get_ram_init_value (ram_section, &ret);
    DBG_TRC(("Read RAM init section, length=%lu bytes", *length))
  } else {
    DBG_ERR(("Can not find RAM section"))
  }

  return (ret);
}

dword diva_cfg_get_vidi_mode (void) {
  diva_entity_link_t* link = diva_cfg_adapter_handle;
  const byte* src = (byte*)&link[1];
  dword value = 0;

  if (diva_cfg_find_named_value (src, (const byte*)"vidi_mode", 9, &value))
    return (0);

  return (value != 0);
}

void diva_set_vidi_values (PISDN_ADAPTER IoAdapter,
                           const diva_xdi_um_cfg_cmd_data_init_vidi_t* vidi_data) {
  IoAdapter->host_vidi.vidi_active                      = 1;
  IoAdapter->host_vidi.req_buffer_base_dma_magic        = vidi_data->req_magic_lo;
  IoAdapter->host_vidi.req_buffer_base_dma_magic_hi     = vidi_data->req_magic_hi;
  IoAdapter->host_vidi.ind_buffer_base_dma_magic        = vidi_data->ind_magic_lo;
  IoAdapter->host_vidi.ind_buffer_base_dma_magic_hi     = vidi_data->ind_magic_hi;
  IoAdapter->host_vidi.dma_segment_length               = vidi_data->dma_segment_length;
  IoAdapter->host_vidi.dma_req_buffer_length            = vidi_data->dma_req_buffer_length;
  IoAdapter->host_vidi.dma_ind_buffer_length            = vidi_data->dma_ind_buffer_length;
  IoAdapter->host_vidi.remote_indication_counter_offset = vidi_data->dma_ind_remote_counter_offset;
}

int DivaAdapterTest(PISDN_ADAPTER IoAdapter) {
  return (0);
}

void vidi_host_pr_out (ADAPTER * a) {
}

byte vidi_host_pr_dpc(ADAPTER *a) {
  return (0);
}

byte vidi_host_test_int (ADAPTER * a) {
  return (1);
}

void vidi_host_clear_int(ADAPTER * a) {
}

int vidi_pcm_req (PISDN_ADAPTER IoAdapter, ENTITY *e) {
  return (0);
}

void  diva_xdi_show_fpga_rom_features   (const byte* fpga_mem_address) {
}

dword diva_xdi_decode_fpga_rom_features (const byte* fpga_mem_address) {
  return (0);
}

int diva_nr_free_dma_entries (struct _diva_dma_map_entry* pmap) {
  return (0);
}

static int diva_hotplug_lookup_card_number (int requestedNr, dword* serialNumber) {
  diva_cmd_line_parameter_t* e = find_entry("UseCardMap");
  int ret = -1;

  if (e->found != 0) {
    const diva_card_map_entry_t* map = diva_create_card_map (e->vs), *nmap = map;
    int nr = 0;

    while (map != 0) {
      nr++;
      if (nr == requestedNr) {
        ret = map->cardType;
        if (serialNumber != 0)
          *serialNumber = map->serialNumber;
        break;
      }
      map = map->nextCard;
    }

    diva_destroy_card_map (nmap);
  }

  return (ret);
}

static int diva_card_map_sn (int requestedNr) {
  if (find_entry("CardMapSn")->found != 0) {
    return (diva_hotplug_lookup_card_number (requestedNr, 0) > 0);
  }

  return 0;
}

static int diva_card_get_cardType2HwChip (dword cardType) {
  if (cardType != 0 || cardType < CARDTYPE_MAX) {
    fprintf (VisualStream, "%u\n", CardProperties[cardType].Chip);
    return 0;
  }
  return -1;
}

static int diva_card_get_cardType2HwFeatures (dword cardType) {
  if (cardType != 0 || cardType < CARDTYPE_MAX) {
    fprintf (VisualStream, "%u\n", CardProperties[cardType].HardwareFeatures);
    return 0;
  }
  return -1;
}

static int diva_card_get_cardType2Name (dword cardType) {
  if (cardType != 0 || cardType < CARDTYPE_MAX) {
    fprintf (VisualStream, "%s\n", CardProperties[cardType].Name);
    return 0;
  }

  return -1;
}

static int diva_card_get_cardType2BusName (dword cardType) {
  if (cardType != 0 || cardType < CARDTYPE_MAX) {
    const char* name;

    switch (CardProperties[cardType].Bus) {
      case BUS_PCI:
        name = "PCI";
        break;

      case BUS_PCIE:
        name = "PCIE";
        break;

      default:
        name = "OTHER";
        break;
    }

    fprintf (VisualStream, "%s\n", name);

    return 0;
  }

  return -1;
}

static int diva_card_cardMap2Shell (void) {
  diva_cmd_line_parameter_t* e = find_entry("UseCardMap");

  if (e->found != 0) {
    const diva_card_map_entry_t* map = diva_create_card_map (e->vs), *nmap = map;
    int nr = 1;

    while (map != 0) {
      if (map->cardType != 0 && map->cardType < CARDTYPE_MAX && map->cardPort == 0) {
        fprintf (VisualStream, "VCCardMapNAME[%d]=\"%u\"\n", nr, map->cardType);
        fprintf (VisualStream, "VCCardMapSN[%d]=\"%u\"\n", nr, map->serialNumber);
        nr++;
      }
      map = map->nextCard;
    }

    diva_destroy_card_map (nmap);

    fprintf (VisualStream, "VCCardMapSN[%d]=\"-1\"\n", nr);

    return (0);
  }


  return (-1);
}

void diva_log_info(unsigned char *fmt, ...) {
}

#include "cardmap.c"
