
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
#include "dsrv_bri.h"
#include "dsp_defs.h"
/******************************************************************************/
#define BRI_ADDR_COUNTER_MASK  0x0000ffffL
void bri_in_buffer (PISDN_ADAPTER IoAdapter, dword Pos, void *Buf, dword Len)
{
 byte   *addrHi, *addrLo, *ioaddr ;
 dword   i, n ;
 addrHi =   IoAdapter->port
        + ((IoAdapter->Properties.Bus == BUS_PCI) ? M_PCI_ADDRH : ADDRH) ;
 addrLo = IoAdapter->port + ADDR ;
 ioaddr = IoAdapter->port + DATA ;
 i = 0 ;
 do
 {
  n = (((Pos + i) & ~BRI_ADDR_COUNTER_MASK) != ((Pos + Len) & ~BRI_ADDR_COUNTER_MASK)) ?
       (BRI_ADDR_COUNTER_MASK + 1) - ((Pos + i) & BRI_ADDR_COUNTER_MASK) : (Len - i + 1) ;
  outpp  (addrHi, (byte)((Pos + i) >> 16)) ;
  outppw (addrLo, (word)(Pos + i)) ;
  inppw_buffer (ioaddr, &((byte *) Buf)[i], n) ;
  i += n ;
 } while ( i < Len );
}
int bri_out_buffer (PISDN_ADAPTER IoAdapter, dword Pos, void *Buf, dword Len, int Verify)
{
 byte   *addrHi, *addrLo, *ioaddr ;
 dword   i, n ;
 word    test ;
 addrHi =   IoAdapter->port
        + ((IoAdapter->Properties.Bus == BUS_PCI) ? M_PCI_ADDRH : ADDRH) ;
 addrLo = IoAdapter->port + ADDR ;
 ioaddr = IoAdapter->port + DATA ;
 i = 0 ;
 do
 {
  n = (((Pos + i) & ~BRI_ADDR_COUNTER_MASK) != ((Pos + Len) & ~BRI_ADDR_COUNTER_MASK)) ?
       (BRI_ADDR_COUNTER_MASK + 1) - ((Pos + i) & BRI_ADDR_COUNTER_MASK) : (Len - i + 1) ;
  outpp  (addrHi, (byte)((Pos + i) >> 16)) ;
  outppw (addrLo, (word)(Pos + i)) ;
  outppw_buffer (ioaddr, &((byte *) Buf)[i], n) ;
  i += n ;
 } while ( i < Len );
 if ( Verify )
 {
  i = 0 ;
  do
  {
   n = (((Pos + i) & ~BRI_ADDR_COUNTER_MASK) != ((Pos + Len) & ~BRI_ADDR_COUNTER_MASK)) ?
        (BRI_ADDR_COUNTER_MASK + 1) - ((Pos + i) & BRI_ADDR_COUNTER_MASK) : (Len - i) ;
   outpp  (addrHi, (byte)((Pos + i) >> 16)) ;
   outppw (addrLo, (word)(Pos + i)) ;
   n += i ;
   do
   {
    test = inppw (ioaddr) ;
    if ( test != *((word *)(&((byte *) Buf)[i])) )
    {
     DBG_FTL(("%s: Memory test failed! (0x%lX - 0x%04X/0x%04X)",
              IoAdapter->Properties.Name, Pos + i,
              test, *((word *)(&((byte *) Buf)[i]))))
     return (FALSE) ;
    }
    i += sizeof(word) ;
   } while ( i < n );
  } while ( i < Len );
 }
 return (TRUE) ;
}
/*****************************************************************************/
#define MAX_XLOG_SIZE (64 * 1024)
/* --------------------------------------------------------------------------
  Investigate card state, recovery trace buffer
  -------------------------------------------------------------------------- */
static void bri_cpu_trapped (PISDN_ADAPTER IoAdapter) {
 byte *addrHi, *addrLo ;
 word *Xlog ;
 dword regs[4], TrapID, size ;
 Xdesc xlogDesc ;
/*
 * first read pointers and trap frame
 */
 if ( !(Xlog = (word *)diva_os_malloc (DIVA_OS_MALLOC_ATOMIC, MAX_XLOG_SIZE)) )
  return ;
 bri_in_buffer (IoAdapter, 0, Xlog, 0x100) ;
/*
 * check for trapped MIPS 3xxx CPU, dump only exception frame
 */
 TrapID = *(dword *)(&Xlog[0x80 / sizeof(Xlog[0])]) ;
 if ( TrapID == 0x99999999 )
 {
  dump_trap_frame (IoAdapter, &((byte *)Xlog)[0x90]) ;
  IoAdapter->trapped = 1 ;
 }
 memcpy (&regs[0], &((byte *)Xlog)[0x70], sizeof(regs)) ;
 bri_in_buffer (IoAdapter, regs[1], &(xlogDesc.cnt), sizeof(word)) ;
 bri_in_buffer (IoAdapter, regs[2], &(xlogDesc.out), sizeof(word)) ;
 xlogDesc.buf = Xlog ;
 regs[0] &= IoAdapter->MemorySize - 1 ;
 if ( (regs[0] < IoAdapter->MemorySize - 1) )
 {
  size = IoAdapter->MemorySize - regs[0] ;
  if ( size > MAX_XLOG_SIZE )
   size = MAX_XLOG_SIZE ;
  bri_in_buffer (IoAdapter, regs[0], Xlog, size) ;
  dump_xlog_buffer (IoAdapter, &xlogDesc, size) ;
  IoAdapter->trapped = 2 ;
 }
 addrHi = IoAdapter->port
   + ((IoAdapter->Properties.Bus == BUS_PCI) ? M_PCI_ADDRH : ADDRH) ;
 addrLo = IoAdapter->port + ADDR ;
 outpp  (addrHi, (byte)((BRI_UNCACHED_ADDR (IoAdapter->MemoryBase + IoAdapter->MemorySize -
                                            BRI_SHARED_RAM_SIZE)) >> 16)) ;
 outppw (addrLo, 0x00) ;
 diva_os_free (DIVA_OS_MALLOC_ATOMIC, Xlog) ;
/*
 *  write a memory dump, if enabled
 */
 if ( IoAdapter->trapped  )
  diva_os_dump_file(IoAdapter) ;
}
/* ---------------------------------------------------------------------
   Dump card memory to file
  --------------------------------------------------------------------- */
#if 0
static void
bri_mem_dump (PISDN_ADAPTER IoAdapter)
{
 char   Name[48] ;
 char  *pBuffer ;
 void  *PtrResult ;
 /* allocated a buffer for adapter memory data */
 pBuffer = diva_os_malloc(IoAdapter->MemorySize) ;
 if (pBuffer == NULL)
 {
  DBG_ERR(("[%s] Writing adapter memory into file failed: no memory",
      IoAdapter->Name)) ;
  return;
 }
 /* copy data from adapter memory into our buffer */
 bri_in_buffer (IoAdapter, 0, pBuffer, IoAdapter->MemorySize) ;
 /* write data into the file */
 strcpy(Name, IoAdapter->Name) ;
 strcat(Name, "_ram.bin") ;
 PtrResult = CopyMemoryToFile(Name, IoAdapter->MemorySize, pBuffer) ;
 if (PtrResult == NULL) {
  DBG_ERR(("[%s] Writing adapter memory into file failed: file error",
      IoAdapter->Name)) ;
 }
 else {
  DBG_LOG(("[%s] Adapter memory was written into file %s",
      IoAdapter->Name, Name)) ;
 }
 /* free allocated buffer */
 diva_os_free(pBuffer);
}
#endif
/* ---------------------------------------------------------------------
   Reset hardware
  --------------------------------------------------------------------- */
static void reset_bri_hardware (PISDN_ADAPTER IoAdapter) {
 outpp (IoAdapter->ctlReg, 0x00) ;
}
/* ---------------------------------------------------------------------
   Halt system
  --------------------------------------------------------------------- */
static void stop_bri_hardware (PISDN_ADAPTER IoAdapter) {
 if (IoAdapter->reset) {
  outpp (IoAdapter->reset, 0x00) ; /* disable interrupts ! */
 }
 outpp (IoAdapter->ctlReg, 0x00) ;    /* clear int, halt cpu */
}
#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* ---------------------------------------------------------------------
  Load protocol on the card
  --------------------------------------------------------------------- */
static dword bri_protocol_load (PISDN_ADAPTER IoAdapter) {
 dword   FileLength ;
 word    *File = NULL ;
 char   *FileName = &IoAdapter->Protocol[0] ;
 dword   Addr ;
 /* -------------------------------------------------------------------
   Try to load protocol code. 'File' points to memory location
   that does contain entire protocol code
   ------------------------------------------------------------------- */
 if ( !(File = (word *)xdiLoadArchive (IoAdapter, &FileLength, 0)) )
  return (0) ;
 /* -------------------------------------------------------------------
   Get protocol features and calculate load addresses
   ------------------------------------------------------------------- */
 IoAdapter->features = diva_get_protocol_file_features ((byte*)File,
                                         OFFS_PROTOCOL_ID_STRING,
                                         IoAdapter->ProtocolIdString,
                                                sizeof(IoAdapter->ProtocolIdString));
 IoAdapter->a.protocol_capabilities = IoAdapter->features ;
 DBG_LOG(("Loading %s", IoAdapter->ProtocolIdString))
 Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24) ;
        if ( Addr != 0 )
 {
  IoAdapter->DspCodeBaseAddr = (Addr + 3) & (~3) ;
  IoAdapter->MaxDspCodeSize = (BRI_UNCACHED_ADDR (IoAdapter->MemoryBase + IoAdapter->MemorySize -
                                                  BRI_SHARED_RAM_SIZE)
                            - IoAdapter->DspCodeBaseAddr) & (IoAdapter->MemorySize - 1) ;
  Addr = IoAdapter->DspCodeBaseAddr ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR] = (byte) Addr ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 1] = (byte)(Addr >> 8) ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 2] = (byte)(Addr >> 16) ;
  ((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 3] = (byte)(Addr >> 24) ;
  IoAdapter->InitialDspInfo = 0x80 ;
 }
 else
 {
  if ( IoAdapter->features & PROTCAP_V90D )
   IoAdapter->MaxDspCodeSize = BRI_V90D_MAX_DSP_CODE_SIZE ;
  else
   IoAdapter->MaxDspCodeSize = BRI_ORG_MAX_DSP_CODE_SIZE ;
  IoAdapter->DspCodeBaseAddr = BRI_CACHED_ADDR (IoAdapter->MemoryBase + IoAdapter->MemorySize -
                                                BRI_SHARED_RAM_SIZE - IoAdapter->MaxDspCodeSize);
  IoAdapter->InitialDspInfo = (IoAdapter->MaxDspCodeSize - BRI_ORG_MAX_DSP_CODE_SIZE) >> 14 ;
 }
 DBG_LOG(("DSP code base 0x%08lx, max size 0x%08lx (%08lx,%02x)",
      IoAdapter->DspCodeBaseAddr, IoAdapter->MaxDspCodeSize,
      Addr, IoAdapter->InitialDspInfo))
 if ( FileLength > ((IoAdapter->DspCodeBaseAddr -
                     BRI_CACHED_ADDR (IoAdapter->MemoryBase)) & (IoAdapter->MemorySize - 1)) )
 {
  xdiFreeFile (File) ;
  DBG_FTL(("Protocol code '%s' too big (%ld)", FileName, FileLength))
  return (0) ;
 }
/*
 * download protocol file and verify
 */
        if ( !bri_out_buffer (IoAdapter, 0, File, FileLength, TRUE) )
 {
  xdiFreeFile (File) ;
  return (0) ;
 }
 xdiFreeFile (File) ;
 return (FileLength) ;
}
/******************************************************************************/
static long bri_download_buffer (OsFileHandle *fp, long length, void **addr) {
 PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)fp->sysLoadDesc ;
 word          buffer[256] ;
 long          len ;
 *addr = ULongToPtr(IoAdapter->downloadAddr) ;
 if ( ((dword) length) > IoAdapter->DspCodeBaseAddr +
                         IoAdapter->MaxDspCodeSize - IoAdapter->downloadAddr )
 {
  DBG_FTL(("%s: out of card memory during DSP download (0x%X)",
           IoAdapter->Properties.Name,
           IoAdapter->downloadAddr + length))
  return (-1) ;
 }
 for ( len = 0 ; length > 0 ; length -= len )
 {
  len = (length > sizeof(buffer) ? sizeof(buffer) : length) ;
  if ( fp->sysFileRead (fp, &buffer[0], len) != len )
   return (-1) ;
  if ( !bri_out_buffer (IoAdapter, IoAdapter->downloadAddr, buffer, len, TRUE) )
   return (-2) ;
  IoAdapter->downloadAddr += len ;
 }
 IoAdapter->downloadAddr = (IoAdapter->downloadAddr + 3) & (~3) ;
 return (0) ;
}
/******************************************************************************/
static dword bri_telindus_load (PISDN_ADAPTER IoAdapter, char *DspTelindusFile) 
{
 char                *error ;
 OsFileHandle        *fp ;
 t_dsp_portable_desc  download_table[DSP_MAX_DOWNLOAD_COUNT] ;
 word                 download_count ;
 dword                FileLength ;
 dword                download_count_data ;
 if (!(fp = OsOpenFile (DspTelindusFile))) {
  return (0) ;
 }
 IoAdapter->downloadAddr = (IoAdapter->DspCodeBaseAddr +\
                           sizeof(dword) + sizeof(download_table) + 3) & (~3) ;
 FileLength      = fp->sysFileSize ;
 fp->sysLoadDesc = (void *)IoAdapter ;
 fp->sysCardLoad = bri_download_buffer ;
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
 * store # of separate download files extracted from archive
 */
 download_count_data = (dword)download_count ;
 if ( !bri_out_buffer (IoAdapter, IoAdapter->DspCodeBaseAddr,
                       &download_count_data, sizeof(dword), TRUE) )
 {
  return (0) ;
 }
/*
 * copy download table to board
 */
 if ( !bri_out_buffer (IoAdapter, IoAdapter->DspCodeBaseAddr + sizeof(dword),
                       download_table, sizeof(download_table), TRUE) )
 {
  return (0) ;
 }
 return (FileLength) ;
}
/******************************************************************************/
#define PROTOCOLVARIANTCHAR '2'
static int load_bri_hardware (PISDN_ADAPTER IoAdapter) {
 dword   i ;
 byte*  addrHi, *addrLo, *ioaddr ;
 dword   test ;
 if ( IoAdapter->Properties.Card != CARD_MAE )
 {
  return (FALSE) ;
 }
 addrHi =   IoAdapter->port \
    + ((IoAdapter->Properties.Bus==BUS_PCI) ? M_PCI_ADDRH : ADDRH);
 addrLo = IoAdapter->port + ADDR ;
 ioaddr = IoAdapter->port + DATA ;
 reset_bri_hardware (IoAdapter) ;
 diva_os_wait (100);
/*
 * recover
 */
 outpp  (addrHi, (byte) 0) ;
 outppw (addrLo, (word) 0) ;
 outppw (ioaddr, (word) 0) ;
/*
 * clear shared memory
 */
 outpp  (addrHi, (byte)((BRI_UNCACHED_ADDR (IoAdapter->MemoryBase + \
          IoAdapter->MemorySize - BRI_SHARED_RAM_SIZE)) >> 16)) ;
 outppw (addrLo, 0) ;
 for ( i = 0 ; i < 0x8000 ; outppw (ioaddr, 0), ++i ) ;
 diva_os_wait (100) ;
/*
 * download protocol and dsp files
 */
 switch ( IoAdapter->protocol_id ) {
 default:
#if !SETPROTOCOLLOAD
  if ( !xdiSetProtocol (IoAdapter, IoAdapter->ProtocolSuffix, 0) )
   return (FALSE) ;
#endif
  if ( !bri_protocol_load (IoAdapter) )
   return (FALSE) ;
  if ( !bri_telindus_load (IoAdapter, DSP_TELINDUS_FILE) )
   return (FALSE) ;
  break ;
 case PROTTYPE_QSIG:
 case PROTTYPE_CORNETN:
#if !SETPROTOCOLLOAD
  if ( !xdiSetProtocol (IoAdapter, IoAdapter->ProtocolSuffix, PROTOCOLVARIANTCHAR) )
   return (FALSE) ;
#endif
  if ( !bri_protocol_load (IoAdapter) )
   return (FALSE) ;
  if ( !bri_telindus_load (IoAdapter, DSP_QSIG_TELINDUS_FILE) )
   return (FALSE) ;
  break ;
 }
/*
 * clear signature
 */
 outpp  (addrHi, (byte)((BRI_UNCACHED_ADDR (IoAdapter->MemoryBase + \
     IoAdapter->MemorySize - BRI_SHARED_RAM_SIZE)) >> 16)) ;
 outppw (addrLo, 0x1e) ;
 outpp (ioaddr, 0) ;
 outpp (ioaddr, 0) ;
/*
 * copy parameters
 */
 diva_configure_protocol (IoAdapter, BRI_SHARED_RAM_SIZE);
/*
 * start the protocol code
 */
 outpp (IoAdapter->ctlReg, 0x08) ;
/*
 * wait for signature (max. 3 seconds)
 */
 for ( i = 0 ; i < 300 ; ++i )
 {
  diva_os_wait (10) ;
  outpp (addrHi, (byte)((BRI_UNCACHED_ADDR (IoAdapter->MemoryBase + \
      IoAdapter->MemorySize - BRI_SHARED_RAM_SIZE)) >> 16)) ;
  outppw (addrLo, 0x1e) ;
  test = (dword)inppw (ioaddr) ;
  if ( test == 0x4447 )
  {
   DBG_TRC(("Protocol startup time %d.%02d seconds",
            (i / 100), (i % 100) ))
   return (TRUE) ;
  }
 }
 DBG_FTL(("%s: Adapter selftest failed (0x%04X)!",
          IoAdapter->Properties.Name, test))
 bri_cpu_trapped (IoAdapter) ;
 return (FALSE) ;
}
#else /* } { */
static int load_bri_hardware (PISDN_ADAPTER IoAdapter) {
 return (0);
}
#endif /* } */
/******************************************************************************/
static int bri_ISR (struct _ISDN_ADAPTER* IoAdapter) {
 if ( !(inpp (IoAdapter->ctlReg) & 0x01) )
  return (0) ;
 /*
  clear interrupt line
  */
 outpp (IoAdapter->ctlReg, 0x08) ;
 IoAdapter->IrqCount++ ;
 if ( IoAdapter->Initialized ) {
  diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
 }
 return (1) ;
}
/* --------------------------------------------------------------------------
  Disable IRQ in the card hardware
  -------------------------------------------------------------------------- */
static void disable_bri_interrupt (PISDN_ADAPTER IoAdapter) {
 if ( IoAdapter->reset )
 {
  outpp (IoAdapter->reset, 0x00) ; // disable interrupts !
 }
 outpp (IoAdapter->ctlReg, 0x00) ;    // clear int, halt cpu
}
/* -------------------------------------------------------------------------
  Fill card entry points
  ------------------------------------------------------------------------- */
void prepare_maestra_functions (PISDN_ADAPTER IoAdapter) {
 ADAPTER *a = &IoAdapter->a ;
 a->ram_in             = io_in ;
 a->ram_inw            = io_inw ;
 a->ram_in_buffer      = io_in_buffer ;
 a->ram_look_ahead     = io_look_ahead ;
 a->ram_out            = io_out ;
 a->ram_outw           = io_outw ;
 a->ram_out_buffer     = io_out_buffer ;
 a->ram_inc            = io_inc ;
 IoAdapter->MemoryBase = BRI_MEMORY_BASE ;
 IoAdapter->MemorySize = BRI_MEMORY_SIZE ;
 IoAdapter->out        = pr_out ;
 IoAdapter->dpc        = pr_dpc ;
 IoAdapter->tst_irq    = scom_test_int ;
 IoAdapter->clr_irq    = scom_clear_int ;
 IoAdapter->pcm        = (struct pc_maint *)MIPS_MAINT_OFFS ;
 IoAdapter->load       = load_bri_hardware ;
 IoAdapter->disIrq     = disable_bri_interrupt ;
 IoAdapter->rstFnc     = reset_bri_hardware ;
 IoAdapter->stop       = stop_bri_hardware ;
 IoAdapter->trapFnc    = bri_cpu_trapped ;
 IoAdapter->diva_isr_handler = bri_ISR;
 /*
  Prepare OS dependent functions
  */
 diva_os_prepare_maestra_functions (IoAdapter);
}
/* -------------------------------------------------------------------------- */
#endif /* } */
