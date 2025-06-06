
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
#include "pri.h"
#include "os.h"
#include "diva_cfg.h"
#include <di_defs.h>
#include <cardtype.h>
#include <dlist.h>

#include <s_pri.c>
extern PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
extern int card_ordinal;
extern int card_number;

extern diva_entity_link_t* diva_cfg_adapter_handle;

typedef struct {
	byte *buffer ;
	dword  length ;
	dword  position ;
} FileInfo ;
static FileInfo     myInfo = { NULL } ;
static OsFileHandle myHandle = { NULL } ;
static long myFileSeek (OsFileHandle *handle, long position, int mode);
static long myFileRead (OsFileHandle *handle, void *buffer, long size);
void diva_configure_pri_protocol (PISDN_ADAPTER IoAdapter);

typedef struct _diva_named_value {
	char	ident[24];
	dword value;
} diva_named_value_t;

#define DIVA_MAX_NAMED_VALUES 32

static diva_named_value_t named_values[DIVA_MAX_NAMED_VALUES];

/* ------------------------------------------------------------------------
		Does create image in the allocated memory
		return:
						>	 0: operation failed
						<= 0: success, ofset to start download
	 ------------------------------------------------------------------------ */
int
divas_pri_create_image (int rev_2,
                        byte* sdram,
                        int prot_fd,
                        int dsp_fd,
                        const char* protocol_name,
                        int protocol_id,
                        dword* protocol_features)
{
	PISDN_ADAPTER IoAdapter = malloc (sizeof(*IoAdapter));
	dword card_bar = cfg_get_card_bar (0);

	if (!IoAdapter) {
		return (-1);
	}
	memset (IoAdapter, 0x00, sizeof(*IoAdapter));

	diva_set_named_value ("p0", (dword)prot_fd);
	strcpy (IoAdapter->protocol_name, "p0");
	diva_set_named_value (DSP_TELINDUS_FILE,	(dword)dsp_fd);

	//strncpy (IoAdapter->protocol_name, protocol_name, 7);
	IoAdapter->Properties = CardProperties[card_ordinal];
	IoAdapter->Channels   = IoAdapter->Properties.Channels;
	IoAdapter->protocol_id = protocol_id;
	IoAdapter->cardType = (word)card_ordinal;
	IoAdapter->ram = sdram;
	IoAdapter->sdram_bar = card_bar;
	IoAdapter->a.io = IoAdapter;
	IoAdapter->ANum = card_number;
	if (rev_2) {
		prepare_pri2_functions (IoAdapter);
		IoAdapter->dma_map = (void*)(unsigned long)card_number;
	} else {
		prepare_pri_functions (IoAdapter);
	}
	diva_configure_pri_protocol (IoAdapter);
	if (!pri_protocol_load (IoAdapter)) {
		return (-2);
	}

	if (!pri_telindus_load (IoAdapter)) {
		return (-3) ;
	}

	IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
	memset (IoAdapter->ram, 0x00, 256) ;
	diva_configure_protocol (IoAdapter,0xffffffff);

	*protocol_features = IoAdapter->features;

	free (IoAdapter);

	return (MP_SHARED_RAM_OFFSET);
}


/* -------------------------------------------------------------------------
		Wrapper for OS related file access functions.
		It should read path name and select appropriated fd,
		what is already opened
	 ------------------------------------------------------------------------- */
OsFileHandle *
OsOpenFile (char *path_name)
{
	dword FileLength ;
	void  *FileBuffer ;

	if (myInfo.buffer) {
		DBG_ERR(("XDI: handle only one open file at a time"))
		return (0) ;
	}
	FileBuffer = xdiLoadFile (path_name, &FileLength, 0);
	if (!FileBuffer) {
		return (0) ;
	}
	myInfo.buffer   = (byte*)FileBuffer;
	myInfo.length   = FileLength;
	myInfo.position = 0;

	myHandle.sysFileDesc = (void *)&myInfo ;
	myHandle.sysFileSize = FileLength ;
	myHandle.sysFileRead = myFileRead ;
	myHandle.sysFileSeek = myFileSeek ;
	myHandle.sysLoadDesc = 0;
	myHandle.sysCardLoad = 0;

	return (&myHandle) ;
}

/******************************************************************************/

void
OsCloseFile (OsFileHandle *fp)
{
	if ( (fp == &myHandle)
	  && (fp->sysFileDesc == (void *)&myInfo) ) {
		if ( myInfo.buffer )
		{
			free (myInfo.buffer) ;
			memset (&myInfo, '\0', sizeof(myInfo)) ;
		}
		memset (&myHandle, '\0', sizeof(myHandle)) ;
	}
}

/******************************************************************************/

void
xdiFreeFile (void* handle)
{
	diva_os_free (0, handle);
}

/******************************************************************************/

static long
myFileSeek (OsFileHandle *handle, long position, int mode)
{
	if ( myInfo.buffer && (myInfo.length > 0) )
	{
		switch (mode)
		{
		case OS_SEEK_CUR:
			position += (long)myInfo.position ;

		case OS_SEEK_SET:
			break ;

		case OS_SEEK_END:
			position = (long)myInfo.position - position ;
			if ( position >= 0 )
				break ;

		default:
			return (-1) ;
		}
		if ( (dword)position <= myInfo.length )
		{
			myInfo.position = (dword)position ;
			return (0) ;
		}
	}
	return (-1) ;
}

/******************************************************************************/

static long
myFileRead (OsFileHandle *handle, void *buffer, long size)
{
	long n, avail ;

	avail = myInfo.length - myInfo.position ;
	n = (avail >= size ? size : avail) ;

	if ( n > 0 )
	{
		memcpy (buffer, &myInfo.buffer[myInfo.position], n) ;
		myInfo.position += n ;
	}
	return ((long)n) ;
}

/******************************************************************************/
/*
		Load file into memory and return pointer to this memory block
		length of file will be writtent to FileLength
	*/
void *
xdiLoadFile (char *FileName, dword *FileLength, dword lim)
{
	int fd;
	void* handle;
	off_t length;

	if ( !FileName || !FileName[0] || !FileLength ) {
		if (FileLength) {
			*FileLength = 0;
		}
		DBG_FTL(("xdiLoadFile: NULL arg"))
		return (NULL) ;
	}
	fd = (int)diva_get_value_by_name (FileName);
	if (fd == -1) {
		return (0);
	}
	if ((length = lseek (fd, 0, SEEK_END)) <= 0) {
		return (0);
	}
	lseek (fd, 0, SEEK_SET);
	if (!length) {
		return (0);
	}
	if (!(handle = malloc (length))) {
		return (0);
	}
	if (read(fd, handle, length) != length) {
		free(handle);
		return (0);
	}
	*FileLength = length;

	return (handle);
}

/******************************************************************************/

void *
xdiLoadArchive (PISDN_ADAPTER IoAdapter, dword *FileLength, dword lim)
{
	return (xdiLoadFile (IoAdapter->protocol_name, FileLength, 0));
}

int xdiSetProtocol (void* p1, void* p2) {
	return (1);
}

/* --------------------------------------------------------------------------
		Framework used to save and restore named dword values
		Do not check if value was not inserted: it will cause faulure later,
		as xdiLoadFile will ask for file descriptor
	 -------------------------------------------------------------------------- */
void
diva_set_named_value (const char* ident, dword value)
{
	int i;
	diva_named_value_t* e = 0;

	if (strlen(ident) > 23) {
		DBG_ERR(("A: can't set named value (%s), ident is too long", ident))
		return;
	}

	for (i = 0; i < DIVA_MAX_NAMED_VALUES; i++) {
		if (!strcmp(named_values[i].ident, ident)) {
			e = &named_values[i];
			break;
		}
	}
	if (!e) {
		for (i = 0; i < DIVA_MAX_NAMED_VALUES; i++) {
			if (!named_values[i].ident[0]) {
				e = &named_values[i];
				strcpy (e->ident, ident);
				break;
			}
		}
	}
	if (e) {
		e->value = value;
	} else {
		DBG_ERR(("A: can't set named value (%s), table is full", ident))
	}
}

dword
diva_get_value_by_name (const char* ident)
{
	int i;

	for (i = 0; i < DIVA_MAX_NAMED_VALUES; i++) {
		if (!strcmp(named_values[i].ident, ident)) {
			return (named_values[i].value);
		}
	}

	return ((dword)(-1));
}

void
diva_init_named_table (void)
{
	memset (&named_values, 0x00, sizeof(named_values));
}

void
diva_configure_pri_protocol (PISDN_ADAPTER IoAdapter)
{

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
}

