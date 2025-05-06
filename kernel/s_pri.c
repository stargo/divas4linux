
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
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
#if defined(DIVA_INCLUDE_DISCONTINUED_HARDWARE) /* { */
#include "di_defs.h"
#include "pc.h"
#include "pr_pc.h"
#include "di.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "divasync.h"
#include "io.h"
#include "helpers.h"
#include "dsrv_pri.h"
#include "dsp_defs.h"
#ifdef DIVA_ADAPTER_TEST_SUPPORTED
#include "divatest.h" /* Adapter test framework */
#endif
/*****************************************************************************/
#define MAX_XLOG_SIZE  (64 * 1024)
/* -------------------------------------------------------------------------
  Does return offset between ADAPTER->ram and real begin of memory
  ------------------------------------------------------------------------- */
static dword pri_ram_offset (ADAPTER* a) {
 return ((dword)MP_SHARED_RAM_OFFSET);
}
/* -------------------------------------------------------------------------
  Recovery XLOG buffer from the card
  ------------------------------------------------------------------------- */
static void pri_cpu_trapped (PISDN_ADAPTER IoAdapter) {
 byte  *base ;
 dword   regs[4], TrapID, size ;
 Xdesc   xlogDesc ;
/*
 * check for trapped MIPS 46xx CPU, dump exception frame
 */
 base   = IoAdapter->ram - MP_SHARED_RAM_OFFSET ;
 TrapID = *((dword *)&base[0x80]) ;
 if ( (TrapID == 0x99999999) || (TrapID == 0x99999901) )
 {
  dump_trap_frame (IoAdapter, &base[0x90]) ;
  IoAdapter->trapped = 1 ;
 }
 memcpy (&regs[0], &base[MP_PROTOCOL_OFFSET + 0x70], sizeof(regs)) ;
 regs[0] &= IoAdapter->MemorySize - 1 ;
 if ( (regs[0] < IoAdapter->MemorySize - 1) )
 {
  size = IoAdapter->MemorySize - regs[0] ;
  if ( size > MAX_XLOG_SIZE )
   size = MAX_XLOG_SIZE ;
  xlogDesc.buf = (word*)&base[regs[0]];
  xlogDesc.cnt = *((word *)&base[regs[1] & (IoAdapter->MemorySize - 1)]) ;
  xlogDesc.out = *((word *)&base[regs[2] & (IoAdapter->MemorySize - 1)]) ;
  dump_xlog_buffer (IoAdapter, &xlogDesc, size) ;
  IoAdapter->trapped = 2 ;
 }
/*
 *  write a memory dump, if enabled
 */
 if ( IoAdapter->trapped  )
  diva_os_dump_file(IoAdapter) ;
}

/* -------------------------------------------------------------------------
  Hardware reset of PRI card
  ------------------------------------------------------------------------- */
static void reset_pri_hardware (PISDN_ADAPTER IoAdapter) {
 *IoAdapter->reset = _MP_RISC_RESET | _MP_LED1 | _MP_LED2 ;
 diva_os_wait (50) ;
 *IoAdapter->reset = 0x00 ;
 diva_os_wait (50) ;
}
/* -------------------------------------------------------------------------
  Stop Card Hardware
  ------------------------------------------------------------------------- */
static void stop_pri_hardware (PISDN_ADAPTER IoAdapter) {
 dword i;
 dword volatile *cfgReg = (dword volatile *)IoAdapter->cfg ;
 cfgReg[3] = 0x00000000 ;
 cfgReg[1] = 0x00000000 ;
 IoAdapter->a.ram_out (&IoAdapter->a, &RAM->SWReg, SWREG_HALT_CPU) ;
 i = 0 ;
 while ( (i < 100) && (IoAdapter->a.ram_in (&IoAdapter->a, &RAM->SWReg) != 0) )
 {
  diva_os_wait (1) ;
  i++ ;
 }
 DBG_TRC(("%s: PRI stopped (%d)", IoAdapter->Name, i))
 cfgReg[0] = (dword)(~0x03E00000) ;
 diva_os_wait (1) ;
 *IoAdapter->reset = _MP_RISC_RESET | _MP_LED1 | _MP_LED2 ;
}
#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* -------------------------------------------------------------------------
  Load protocol code to the PRI Card
  ------------------------------------------------------------------------- */
#define DOWNLOAD_ADDR(IoAdapter) \
 (&IoAdapter->ram[IoAdapter->downloadAddr & (IoAdapter->MemorySize - 1)])
static int pri_protocol_load (PISDN_ADAPTER IoAdapter) {
 dword  FileLength ;
 dword *File ;
 dword *sharedRam ;
 dword  Addr ;
 if (!(File = (dword *)xdiLoadArchive (IoAdapter, &FileLength, 0))) {
  return (0) ;
 }
 IoAdapter->features = diva_get_protocol_file_features ((byte*)File,
                                        OFFS_PROTOCOL_ID_STRING,
                                        IoAdapter->ProtocolIdString,
                                        sizeof(IoAdapter->ProtocolIdString)) ;
 IoAdapter->a.protocol_capabilities = IoAdapter->features ;
 DBG_LOG(("Loading %s", IoAdapter->ProtocolIdString))
 IoAdapter->InitialDspInfo &= ~0xffff ;
 Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24) ;
        if ( Addr != 0 )
 {
  IoAdapter->DspCodeBaseAddr = (Addr + 3) & (~3) ;
  IoAdapter->MaxDspCodeSize = (MP_UNCACHED_ADDR (IoAdapter->MemorySize)
                            - IoAdapter->DspCodeBaseAddr) & (IoAdapter->MemorySize - 1) ;
  Addr = IoAdapter->DspCodeBaseAddr ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR] = (byte) Addr ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 1] = (byte)(Addr >> 8) ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 2] = (byte)(Addr >> 16) ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 3] = (byte)(Addr >> 24) ;
  IoAdapter->InitialDspInfo |= 0x80 ;
 }
 else
 {
  if ( IoAdapter->features & PROTCAP_VOIP )
   IoAdapter->MaxDspCodeSize = MP_VOIP_MAX_DSP_CODE_SIZE ;
  else if ( IoAdapter->features & PROTCAP_V90D )
   IoAdapter->MaxDspCodeSize = MP_V90D_MAX_DSP_CODE_SIZE ;
  else
   IoAdapter->MaxDspCodeSize = MP_ORG_MAX_DSP_CODE_SIZE ;
  IoAdapter->DspCodeBaseAddr = MP_CACHED_ADDR (IoAdapter->MemorySize -
                                               IoAdapter->MaxDspCodeSize) ;
  IoAdapter->InitialDspInfo |= (IoAdapter->MaxDspCodeSize
                            - MP_ORG_MAX_DSP_CODE_SIZE) >> 14 ;
 }
 DBG_LOG(("DSP code base 0x%08lx, max size 0x%08lx (%08lx,%02x)",
          IoAdapter->DspCodeBaseAddr, IoAdapter->MaxDspCodeSize,
          Addr, IoAdapter->InitialDspInfo))
 if ( FileLength > ((IoAdapter->DspCodeBaseAddr -
                     MP_CACHED_ADDR (MP_PROTOCOL_OFFSET)) & (IoAdapter->MemorySize - 1)) )
 {
  xdiFreeFile (File);
  DBG_FTL(("Protocol code '%s' too long (%ld)",
           &IoAdapter->Protocol[0], FileLength))
  return (0) ;
 }
 IoAdapter->downloadAddr = MP_UNCACHED_ADDR (MP_PROTOCOL_OFFSET) ;
 sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
 memcpy (sharedRam, File, FileLength) ;
 if ( memcmp (sharedRam, File, FileLength) )
 {
  DBG_FTL(("%s: Memory test failed!", IoAdapter->Properties.Name))
  xdiFreeFile (File);
  return (0) ;
 }
 xdiFreeFile (File);
 return (1) ;
}
#endif /* } */
/******************************************************************************/
static dword
diva_pri_detect_dsps (PISDN_ADAPTER IoAdapter)
{
 static dword   row_offset[] =
 {
  0x00000000,
  0x00000800, /* 1 - ROW 1 */
  0x00000840, /* 2 - ROW 2 */
  0x00001000, /* 3 - ROW 3 */
  0x00001040, /* 4 - ROW 4 */
  0x00000000  /* 5 - ROW 0 */
 };
 volatile byte *base = IoAdapter->reset - MP_RESET ;
 volatile byte *dsp_addr_port, *dsp_data_port ;
 dword          ret ;
 byte           row_state ;
 int            dsp_row, dsp_index, dsp_num ;
 char           d[30] ;
/*
 * Check if DSP's are present and operating
 * Information about detected DSP's is returned as bit mask
 * Bit 0  - DSP 0
 * ...
 * Bit 30 - DSP 30
 */
 if ( !base || !IoAdapter->reset )
 {
  return (0) ;
 }
 *(volatile byte *)(IoAdapter->reset) = _MP_RISC_RESET | _MP_DSP_RESET ;
 diva_os_wait (5) ;
 ret = 0 ;
 for ( dsp_num = 0; dsp_num < 30; dsp_num++ )
 {
  dsp_row   = dsp_num / 7 + 1 ;
  dsp_index = dsp_num % 7 ;
  dsp_data_port = base ;
  dsp_addr_port = base ;
  dsp_data_port += row_offset[dsp_row] ;
  dsp_addr_port += row_offset[dsp_row] ;
  dsp_data_port += (dsp_index * 8) ;
  dsp_addr_port += (dsp_index * 8) + 0x80 ;
  if ( !dsp_check_presence (dsp_addr_port, dsp_data_port, dsp_num+1) )
   ret |= (1 << dsp_num) ;
 }
 *(volatile byte *)(IoAdapter->reset) = _MP_RISC_RESET | _MP_LED1 | _MP_LED2 ;
 diva_os_wait (50) ;
/*
 * Verify modules
 */
 for ( dsp_row = 0; dsp_row < 4; dsp_row++ )
 {
  row_state = (byte)((ret >> (dsp_row*7)) & 0x7F) ;
  if ( row_state && (row_state != 0x7F) )
  {
   for ( dsp_index = 0; dsp_index < 7; dsp_index++ )
   {
    if ( !(row_state & (1 << dsp_index)) )
    {
     DBG_ERR(("MODULE[%d]-DSP[%d] failed", dsp_row+1, dsp_index+1))
    }
   }
  }
 }
 if ( !(ret & 0x10000000) )
 {
  DBG_ERR(("ON BOARD-DSP[1] failed"))
 }
 if ( !(ret & 0x20000000) )
 {
  DBG_ERR(("ON BOARD-DSP[2] failed"))
 }
/*
 * Display layout
*/
 for ( dsp_num = 0; dsp_num < 30; dsp_num++ )
  d[dsp_num] = (ret & (1L << dsp_num)) ? '+' : '-' ;
 if ( ret & ~0x30000000 )
 {
  DBG_LOG(("  +----------------------------------------------------+"))
  DBG_LOG(("  |                 %c3        %c10       %c17       %c24  |",
                                d[3],      d[10],     d[17],     d[24]))
  DBG_LOG(("  |                  %c2        %c9        %c16       %c23 |",
                                 d[2],      d[9],      d[16],     d[23]))
  DBG_LOG(("  |                 %c4        %c11       %c18       %c25  |",
                                d[4],      d[11],     d[18],     d[25]))
  DBG_LOG(("  |                  %c1  %c28   %c8        %c15 %c29   %c22 |",
                                 d[1],d[28], d[8],     d[15],d[29], d[22]))
  DBG_LOG(("  |                 %c5        %c12       %c19       %c26  |",
                                d[5],      d[12],     d[19],     d[26]))
  DBG_LOG(("  |                  %c0        %c7        %c14       %c21 |",
                                 d[0],      d[7],      d[14],     d[21]))
  DBG_LOG(("  |                 %c6        %c13       %c20       %c27  |",
                                d[6],      d[13],     d[20],     d[27]))
  DBG_LOG(("  |+--+            +-----------------------------------+"))
  DBG_LOG(("  ++  +------------+"))
 }
 else
 {
  DBG_LOG(("  +----------------------------------------------------+"))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                      %c28                 %c29       |",
                                     d[28],               d[29]))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |+--+            +-----------------------------------+"))
  DBG_LOG(("  ++  +------------+"))
 }
 DBG_LOG(("DSP's(present-absent):%08X-%08X", ret, ~ret & 0x3fffffff))
 *(volatile byte *)(IoAdapter->reset) = 0 ;
 diva_os_wait (50) ;
 return (ret) ;
}
static dword
diva_pri2_detect_dsps (PISDN_ADAPTER IoAdapter)
{
 static dword   row_offset[] =
{
    0x00000000,
    0x00000800, /* 1 - ROW 1 */
    0x00000840, /* 2 - ROW 2 */
    0x00001000, /* 3 - ROW 3 */
    0x00001040, /* 4 - ROW 4 */
    0x00000000  /* 5 - ROW 0 */
  };
 volatile byte *base = IoAdapter->reset - MP_RESET ;
 volatile byte *dsp_addr_port, *dsp_data_port ;
 dword          ret ;
 byte           row_state ;
 int            dsp_row, dsp_index, dsp_num ;
 char           d[30] ;
/*
 * Check if DSP's are present and operating
 * Information about detected DSP's is returned as bit mask
 * Bit 0  - DSP 0
 * ...
 * Bit 30 - DSP 30
 */
  if (!base || !IoAdapter->reset)
  {
    return (0);
  }
  *(volatile byte*)(IoAdapter->reset) = _MP_RISC_RESET | _MP_DSP_RESET;
  diva_os_wait (5) ;
 ret = 0 ;
 for ( dsp_num = 0; dsp_num < 30; dsp_num++ )
 {
    dsp_row   = dsp_num / 7 + 1;
    dsp_index = dsp_num % 7;
    dsp_data_port = base;
    dsp_addr_port = base;
    dsp_data_port += row_offset[dsp_row];
    dsp_addr_port += row_offset[dsp_row];
    dsp_data_port += (dsp_index * 8);
    dsp_addr_port += (dsp_index * 8) + 0x80;
  if ( !dsp_check_presence (dsp_addr_port, dsp_data_port, dsp_num+1) )
      ret |= (1 << dsp_num);
  }
  *(volatile byte*)(IoAdapter->reset) = _MP_RISC_RESET | _MP_LED1 | _MP_LED2;
  diva_os_wait (50) ;
  /*
 * Verify modules
  */
 for ( dsp_row = 0; dsp_row < 4; dsp_row++ )
 {
    row_state = (byte)((ret >> (dsp_row*7)) & 0x7F);
  if ( row_state && (row_state != 0x7F) )
  {
   for ( dsp_index = 0; dsp_index < 7; dsp_index++ )
   {
    if ( !(row_state & (1 << dsp_index)) )
    {
     DBG_ERR(("GROUP[%d]-DSP[%d] failed", dsp_row+1, dsp_index+1))
        }
      }
    }
  }
 if ( !(ret & 0x10000000) )
 {
  DBG_ERR(("SIGNLE-DSP[1] failed"))
  }
 if ( !(ret & 0x20000000) )
 {
  DBG_ERR(("SINGLE-DSP[2] failed"))
  }
  /*
 * Display layout
  */
 for ( dsp_num = 0; dsp_num < 30; dsp_num++ )
  d[dsp_num] = (ret & (1L << dsp_num)) ? '+' : '-' ;
 if ( ret & ~0x30000000 )
 {
  DBG_LOG(("  +----------------------------------------------------+"))
  DBG_LOG(("  |    %c28 %c29          %c0       %c7       %c14      %c21 |",
                  d[28],d[29],        d[0],     d[7],     d[14],    d[21]))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                 %c1  %c2   %c8  %c9   %c15 %c16  %c22 %c23 |",
                                d[1],d[2], d[8],d[9],d[15],d[16],d[22],d[23]))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                 %c3  %c4   %c10 %c11  %c17 %c18  %c24 %c25 |",
                               d[3],d[4],d[10],d[11],d[17],d[18],d[24],d[25]))
  DBG_LOG(("  |                                                    |"))
  DBG_LOG(("  |                 %c5  %c6   %c12 %c13  %c19 %c20  %c26 %c27 |",
                               d[5],d[6],d[12],d[13],d[19],d[20],d[26],d[27]))
  DBG_LOG(("  |+--+            +-----------------------------------+"))
  DBG_LOG(("  ++  +------------+"))
 }
 else
 {
  DBG_LOG(("  +-------------------------+"))
  DBG_LOG(("  |    %c28 %c29              |",
                  d[28],d[29]))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |                         |"))
  DBG_LOG(("  |+--+            +--------+"))
  DBG_LOG(("  ++  +------------+"))
 }
 DBG_LOG(("DSP's(present-absent):%08X-%08X", ret, ~ret & 0x3fffffff))
  *(volatile byte*)(IoAdapter->reset) = 0 ;
  diva_os_wait (50) ;
  return (ret);
}
#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* -------------------------------------------------------------------------
  helper used to download dsp code toi PRI Card
  ------------------------------------------------------------------------- */
static long pri_download_buffer (OsFileHandle *fp, long length, void **addr) {
 PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)fp->sysLoadDesc ;
 dword        *sharedRam ;
 *addr = ULongToPtr(IoAdapter->downloadAddr) ;
 if ( ((dword) length) > IoAdapter->DspCodeBaseAddr +
                         IoAdapter->MaxDspCodeSize - IoAdapter->downloadAddr )
 {
  DBG_FTL(("%s: out of card memory during DSP download (0x%X)",
           IoAdapter->Properties.Name,
           IoAdapter->downloadAddr + length))
  return (-1) ;
 }
 sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
 if ( fp->sysFileRead (fp, sharedRam, length) != length )
  return (-1) ;
 IoAdapter->downloadAddr += length ;
 IoAdapter->downloadAddr  = (IoAdapter->downloadAddr + 3) & (~3) ;
 return (0) ;
}
/* -------------------------------------------------------------------------
  Download DSP code to PRI Card
  ------------------------------------------------------------------------- */
static dword pri_telindus_load (PISDN_ADAPTER IoAdapter) {
 char                *error ;
 OsFileHandle        *fp ;
 t_dsp_portable_desc  download_table[DSP_MAX_DOWNLOAD_COUNT] ;
 word                 download_count ;
 dword               *sharedRam ;
 dword                FileLength ;
 if ( !(fp = OsOpenFile (DSP_TELINDUS_FILE)) )
  return (0) ;
 IoAdapter->downloadAddr = (IoAdapter->DspCodeBaseAddr
                         + sizeof(dword) + sizeof(download_table) + 3) & (~3) ;
 FileLength      = fp->sysFileSize ;
 fp->sysLoadDesc = (void *)IoAdapter ;
 fp->sysCardLoad = pri_download_buffer ;
 download_count = DSP_MAX_DOWNLOAD_COUNT ;
 memset (&download_table[0], '\0', sizeof(download_table)) ;
/*
 * set start address for download (use autoincrement mode !)
 */
 error = dsp_read_file (fp, (word)(IoAdapter->cardType),
                        &download_count, NULL, &download_table[0]) ;
 if ( error )
 {
  DBG_FTL(("download file error: %s", error))
  OsCloseFile (fp) ;
  return (0) ;
 }
 OsCloseFile (fp) ;
/*
 * calculate dsp image length
 */
 IoAdapter->DspImageLength = IoAdapter->downloadAddr - IoAdapter->DspCodeBaseAddr;
/*
 * store # of separate download files extracted from archive
 */
 IoAdapter->downloadAddr = IoAdapter->DspCodeBaseAddr ;
 sharedRam    = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
 sharedRam[0] = (dword)download_count ;
 memcpy (&sharedRam[1], &download_table[0], sizeof(download_table)) ;
 return (FileLength) ;
}
/* -------------------------------------------------------------------------
  Download PRI Card
  ------------------------------------------------------------------------- */
static int load_pri_hardware (PISDN_ADAPTER IoAdapter) {
 dword           i, d ;
 struct mp_load *boot = (struct mp_load *)IoAdapter->ram ;
 if ( IoAdapter->Properties.Card != CARD_MAEP )
  return (0) ;
 boot->err = 0 ;
#if 0
 IoAdapter->rstFnc (IoAdapter) ;
#else
 d = IoAdapter->DetectDsps(IoAdapter) ; /* makes reset */
 i = 0;
 while ( d != 0 )
 {
  if ( d & 1 )
   i++ ;
  d >>= 1 ;
 }
 IoAdapter->InitialDspInfo &= 0xffff ;
 IoAdapter->InitialDspInfo |= (i << 16) ;
 if ( i < 2 ) {
  DBG_FTL(("%s: DSP error!", IoAdapter->Properties.Name))
  return (0) ;
 }
#endif
/*
 * check if CPU is alive
 */
 diva_os_wait (10) ;
 i = boot->live ;
 diva_os_wait (10) ;
 if ( i == boot->live )
 {
  DBG_FTL(("%s: CPU is not alive!", IoAdapter->Properties.Name))
  return (0) ;
 }
 if ( boot->err )
 {
  DBG_FTL(("%s: Board Selftest failed!", IoAdapter->Properties.Name))
  return (0) ;
 }
/*
 * download protocol and dsp files
 */
#if !SETPROTOCOLLOAD
 if ( !xdiSetProtocol (IoAdapter, IoAdapter->ProtocolSuffix, 0) )
  return (0) ;
#endif
 if ( !pri_protocol_load (IoAdapter) )
  return (0) ;
 if ( !pri_telindus_load (IoAdapter) )
  return (0) ;
/*
 * copy configuration parameters
 */
 IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
 memset (IoAdapter->ram, '\0', 256) ;
 diva_configure_protocol (IoAdapter, MP_SHARED_RAM_SIZE);
/*
 * start adapter
 */
 boot->addr = MP_UNCACHED_ADDR (MP_PROTOCOL_OFFSET) ;
 boot->cmd  = 3 ;
/*
 * wait for signature in shared memory (max. 3 seconds)
 */
 for ( i = 0 ; i < 300 ; ++i )
 {
  diva_os_wait (10) ;
  if ( (boot->signature >> 16) == 0x4447 )
  {
   DBG_TRC(("Protocol startup time %d.%02d seconds",
            (i / 100), (i % 100) ))
   return (1) ;
  }
 }
 DBG_FTL(("%s: Adapter selftest failed (0x%04X)!",
          IoAdapter->Properties.Name, boot->signature >> 16))
 pri_cpu_trapped (IoAdapter) ;
 return (0) ;
}
#else /* } { */
static int load_pri_hardware (PISDN_ADAPTER IoAdapter) {
 return (0);
}
#endif /* } */
/* --------------------------------------------------------------------------
  PRI Adapter interrupt Service Routine
   -------------------------------------------------------------------------- */
static int pri_ISR (struct _ISDN_ADAPTER* IoAdapter) {
 if ( !(*((dword *)IoAdapter->cfg) & 0x80000000) )
  return (0) ;
 /*
  clear interrupt line
  */
 *((dword *)IoAdapter->cfg) = (dword)~0x03E00000 ;
 IoAdapter->IrqCount++ ;
 if ( IoAdapter->Initialized )
 {
  diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
 }
 return (1) ;
}
/* -------------------------------------------------------------------------
  Disable interrupt in the card hardware
  ------------------------------------------------------------------------- */
static void disable_pri_interrupt (PISDN_ADAPTER IoAdapter) {
 dword volatile *cfgReg = (dword volatile *)IoAdapter->cfg ;
 cfgReg[3] = 0x00000000 ;
 cfgReg[1] = 0x00000000 ;
 cfgReg[0] = (dword)(~0x03E00000) ;
}

/*
  This function is called in case of critical system failure by MAINT
  driver and should save entire MAINT trace buffer to the memory of
  adapter.

  Returns zero on success.
	Returns -1 on error

	*/
static int pri_save_maint_buffer (PISDN_ADAPTER IoAdapter, const void* data, dword length) {
	if (IoAdapter->ram != 0 && IoAdapter->MemorySize > MP_SHARED_RAM_OFFSET &&
			(IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET) > length) {
		dword volatile *cfgReg = (dword volatile *)IoAdapter->cfg ;
		dword i = 100000;

		IoAdapter->Initialized = 0;
		disable_pri_interrupt (IoAdapter);
		cfgReg[3] = 0x00000000 ;
		cfgReg[1] = 0x00000000 ;
		IoAdapter->a.ram_out (&IoAdapter->a, &RAM->SWReg, SWREG_HALT_CPU) ;
		while ((i-- > 0) && (IoAdapter->a.ram_in (&IoAdapter->a, &RAM->SWReg) != 0));

		cfgReg[0] = (dword)(~0x03E00000) ;
		i = 1000;
		while (i-- > 0) {
			(void)IoAdapter->a.ram_in (&IoAdapter->a, &RAM->SWReg);
		}
		*IoAdapter->reset = _MP_RISC_RESET | _MP_LED1 | _MP_LED2 ;

		memcpy (IoAdapter->ram, data, length);

		return (0);
	}

	return (-1);
}

/* -------------------------------------------------------------------------
  Install entry points for PRI Adapter
  ------------------------------------------------------------------------- */
static void prepare_common_pri_functions (PISDN_ADAPTER IoAdapter) {
 ADAPTER *a = &IoAdapter->a ;
 a->ram_in           = mem_in ;
 a->ram_inw          = mem_inw ;
 a->ram_in_buffer    = mem_in_buffer ;
 a->ram_look_ahead   = mem_look_ahead ;
 a->ram_out          = mem_out ;
 a->ram_outw         = mem_outw ;
 a->ram_out_buffer   = mem_out_buffer ;
 a->ram_inc          = mem_inc ;
 a->ram_offset       = pri_ram_offset ;
 a->ram_out_dw       = mem_out_dw;
 a->ram_in_dw        = mem_in_dw;
 a->cma_va           = mem_cma_va;
 IoAdapter->out      = pr_out ;
 IoAdapter->dpc      = pr_dpc ;
 IoAdapter->tst_irq  = scom_test_int ;
 IoAdapter->clr_irq  = scom_clear_int ;
 IoAdapter->pcm      = (struct pc_maint *)(MIPS_MAINT_OFFS
                                        - MP_SHARED_RAM_OFFSET) ;
 IoAdapter->load     = load_pri_hardware ;
 IoAdapter->disIrq   = disable_pri_interrupt ;
 IoAdapter->rstFnc   = reset_pri_hardware ;
 IoAdapter->stop     = stop_pri_hardware ;
 IoAdapter->trapFnc  = pri_cpu_trapped ;
 IoAdapter->diva_isr_handler = pri_ISR;
 IoAdapter->save_maint_buffer_proc = pri_save_maint_buffer;

#ifdef DIVA_ADAPTER_TEST_SUPPORTED
  IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
#endif
}
/* -------------------------------------------------------------------------
  Install entry points for PRI Adapter
  ------------------------------------------------------------------------- */
void prepare_pri_functions (PISDN_ADAPTER IoAdapter) {
 IoAdapter->MemorySize = MP_MEMORY_SIZE ;
 IoAdapter->DetectDsps = diva_pri_detect_dsps ;
 prepare_common_pri_functions (IoAdapter) ;
 diva_os_prepare_pri_functions (IoAdapter);
}
/* -------------------------------------------------------------------------
  Install entry points for PRI Rev.2 Adapter
  ------------------------------------------------------------------------- */
void prepare_pri2_functions (PISDN_ADAPTER IoAdapter) {
 IoAdapter->MemorySize = MP2_MEMORY_SIZE ;
 IoAdapter->DetectDsps = diva_pri2_detect_dsps ;
 prepare_common_pri_functions (IoAdapter) ;
 diva_os_prepare_pri2_functions (IoAdapter);
}
/* ------------------------------------------------------------------------- */
#endif /* } */
