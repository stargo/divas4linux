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
#include "pr_pc.h"
#include "di.h"
#include "vidi_di.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "divasync.h"
#include "io.h"
#include "helpers.h"
#include "dsrv_pri.h"
#include "dsrv_pri3.h"
#include "dsp_defs.h"
#include "dsrv4bri.h" /* PLX related definitions */
#ifdef DIVA_ADAPTER_TEST_SUPPORTED
#include "divatest.h" /* Adapter test framework */
#endif
#include "fpga_rom.h"
#include "fpga_rom_xdi.h"

#define DIVA_MEMORY_TEST_PATTERN "Hallo World!\0"
#define MAX_XLOG_SIZE  (64 * 1024)

static int load_pri_v3_hardware (PISDN_ADAPTER IoAdapter);
static dword diva_pri_v3_detect_dsps (PISDN_ADAPTER IoAdapter);

/*
  Start the CPU
  */
int start_pri_v3_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
	reset += (0x20000/sizeof(*reset));


	/*
		Release MIPS cpu cold reset
		*/
	*reset |= 0x00000001;
	diva_os_wait(100);

	/*
		Release MIPS cpu warm reset
		*/
	*reset |= 0x00000002;
	diva_os_wait(100);

	return (0);
}

/* --------------------------------------------------------------------------
		Verify and align FPGA download image

		The FPGA file does not contain any header more. Instead of this
    feature memory area is implemented in the FPGA self.
	 -------------------------------------------------------------------------- */
#define FPGA_NAME_OFFSET         0x10
static byte* pri_v3_check_FPGAsrc (PISDN_ADAPTER IoAdapter, char *FileName, dword *Length, dword *code) {
	byte *File;
	dword  i;

	if (!(File = (byte *)xdiLoadFile (FileName, Length, 0))) {
		return (NULL) ;
	}
/*
 *	 scan file until FF and put id string into buffer
 */
	for ( i = 0 ; File[i] != 0xff ; )
	{
		if ( ++i >= *Length )
		{
			DBG_FTL(("FPGA download: start of data header not found"))
			xdiFreeFile (File) ;
			return (NULL) ;
		}
	}
	*code = i++ ;

	return (File);
}

/* -------------------------------------------------------------
		Download FPFA image
	 ------------------------------------------------------------- */
#define FPGA_PROG   0x0001		/* PROG enable low     */
#define FPGA_BUSY   0x0002		/* BUSY high, DONE low */
#define	FPGA_CS     0x000C		/* Enable I/O pins     */
#define FPGA_CCLK   0x0100
#define FPGA_DOUT   0x0400
#define FPGA_DIN    FPGA_DOUT   /* bidirectional I/O */

int pri_v3_FPGA_download (PISDN_ADAPTER IoAdapter) {
	int            bit ;
	byte           *File ;
	dword          code, FileLength ;
	volatile word *addr = (volatile word*)IoAdapter->prom ;
	word           val, baseval = FPGA_CS | FPGA_PROG ;
	volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
	char* name   = "dspri331.bit";
	reset += (0x20000/sizeof(*reset));

	if (!(File = pri_v3_check_FPGAsrc (IoAdapter, name, &FileLength, &code))) {
		return (0) ;
	}

/*
 *	prepare download, pulse PROGRAM pin down.
 */
	*addr = (baseval & ~FPGA_PROG); // PROGRAM low pulse
	diva_os_wait (5);
	*addr = baseval ;              // release
	diva_os_wait(200);

/*
 *	check done pin, must be low
 */
	if (*addr & FPGA_BUSY) {
		  DBG_FTL(("FPGA download: acknowledge for FPGA memory clear missing"))
		  xdiFreeFile (File) ;
		  return (0) ;
  }

/*
 *	put data onto the FPGA
 */
	while ( code < FileLength )
	{
		val = ((word)File[code++]) << 3 ;

		for ( bit = 8 ; bit-- > 0 ; val <<= 1 ) // put byte onto FPGA
		{
			baseval &= ~FPGA_DOUT ;             // clr  data bit
			baseval |= (val & FPGA_DOUT) ;      // copy data bit
			*addr    = baseval ;
			*addr    = baseval | FPGA_CCLK ;     // set CCLK hi
			*addr    = baseval | FPGA_CCLK ;     // set CCLK hi
			*addr    = baseval ;                 // set CCLK lo
		}
	}
	xdiFreeFile (File) ;
	diva_os_wait (100) ;
	val = *addr ;

	if ( !(val & FPGA_BUSY) )
	{
		DBG_FTL(("FPGA download: chip remains in busy state (0x%04x)", val))
		return (0) ;
	}

	diva_os_wait(200);

	if ( !(val & FPGA_BUSY) )
	{
		DBG_FTL(("FPGA download: during second check chip remains in busy state again (0x%04x)", val))
		return (0) ;
	}

	diva_xdi_show_fpga_rom_features (IoAdapter->ctlReg+0x30000);

	/*
		Set PLX Local BUS latency timer via MABR0
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |=
              ((0x000000ff | (1 << 16)) | (1 << 18) | (1 << 24) | (1 << 25));
	*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (0x00000000 | (1 << 17)  | (1 << 23));

	/*
		Disable prefeth for memory space zero via LBRD0
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |= ((1 << 7) | (1 << 24) | (1 << 27));

	/*
		Set the local register to his initial value (all resets active)
		*/
	*reset = 0;

	/*
		After FPGA download is completed show the short flash with LEDs
	*/
	bit = 4;
	while (bit--) {
		*reset = 0x10 | 0x20 | 0x40;
		diva_os_sleep(50);
		*reset = 0x08 | 0x20 | 0x40;
		diva_os_sleep(50);
		*reset = 0x08 | 0x10 | 0x40;
		diva_os_sleep(50);
		*reset = 0x08 | 0x10 | 0x20;
		diva_os_sleep(50);
	}
	*reset &= ~(0x08 | 0x10 | 0x20 | 0x40); /* Let the LEDs ON to show that adapter FPGA is loaded now */
 memcpy(IoAdapter->ram, DIVA_MEMORY_TEST_PATTERN, sizeof(DIVA_MEMORY_TEST_PATTERN));
 if (memcmp (IoAdapter->ram, DIVA_MEMORY_TEST_PATTERN, sizeof(DIVA_MEMORY_TEST_PATTERN))) {
		DBG_FTL(("Memory test failed"))
  DBG_BLK((IoAdapter->ram, sizeof(DIVA_MEMORY_TEST_PATTERN)))
		return (0);
  }

  if (IoAdapter->DivaAdapterTestProc) {
    if ((*(IoAdapter->DivaAdapterTestProc))(IoAdapter)) {
      return (0);
    }
  }

	return (1) ;
}

static void reset_pri_v3_hardware (PISDN_ADAPTER IoAdapter) {
	volatile word  *Reset ;
	volatile dword *Cntrl ;

	Reset = (word volatile *)IoAdapter->prom ;
	Cntrl = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_RISC]);
	*Reset |= PLX9054_SOFT_RESET ;
	diva_os_wait (1) ;
	*Reset &= ~PLX9054_SOFT_RESET ;
	diva_os_wait (1);
	*Reset |= PLX9054_RELOAD_EEPROM ;
	diva_os_wait (1) ;
	*Reset &= ~PLX9054_RELOAD_EEPROM ;
	diva_os_wait (1);

	*Cntrl = 0 ;

	DBG_TRC(("resetted board @ reset addr 0x%08lx", Reset))
	DBG_TRC(("resetted board @ cntrl addr 0x%08lx", Cntrl))
}

/*
  Offset between the start of physical memory and
  begin of the shared memory
  */
static dword pri_v3_ram_offset (ADAPTER* a) {
 return ((dword)MP_SHARED_RAM_OFFSET);
}

static void pri_v3_cpu_trapped (PISDN_ADAPTER IoAdapter) {
 byte  *base ;
 dword   regs[4], TrapID, size ;
 Xdesc   xlogDesc ;
/*
 * check for trapped MIPS 46xx CPU, dump exception frame
 */
 base = IoAdapter->ram - MP_SHARED_RAM_OFFSET ;
 TrapID = *((dword *)&base[0x80]) ;
 if ( (TrapID == 0x99999999) || (TrapID == 0x99999901) )
 {
  dump_trap_frame (IoAdapter, &base[0x90]) ;
  IoAdapter->trapped = 1 ;
 }
 memcpy (&regs[0], &base[0x70], sizeof(regs)) ;
 regs[0] &= IoAdapter->MemorySize - 1 ;
 if (regs[0] != 0 && (regs[0] < IoAdapter->MemorySize - 1) )
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
}

static void stop_pri_v3_hardware (PISDN_ADAPTER IoAdapter) {
	dword i = 0;
	volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
	reset += (0x20000/sizeof(*reset));

	if (IoAdapter->host_vidi.vidi_active == 0) {
		IoAdapter->a.ram_out (&IoAdapter->a, &RAM->SWReg, SWREG_HALT_CPU) ;
		while ( (i < 100) && (IoAdapter->a.ram_in (&IoAdapter->a, &RAM->SWReg) != 0)) {
			diva_os_wait (1) ;
			i++ ;
		}
		if (i >= 100) {
			DBG_ERR(("A(%d) idi, failed to stop CPU", IoAdapter->ANum))
		}
	} else if (IoAdapter->host_vidi.vidi_started != 0) {
    diva_os_spin_lock_magic_t OldIrql;

    diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "stop");
    IoAdapter->host_vidi.request_flags |= DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT;
    diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
    diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "stop");

    do {
      diva_os_sleep(10);
    } while (i++ < 10 &&
             (IoAdapter->host_vidi.request_flags & DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT) != 0);
    if ((IoAdapter->host_vidi.request_flags & DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT) == 0) {
      i = 0;
      do {
        diva_os_sleep(10);
      } while (i++ < 10 && IoAdapter->host_vidi.cpu_stop_message_received != 2);
    }
    if (i >= 10) {
      DBG_ERR(("A(%d) vidi, failed to stop CPU", IoAdapter->ANum))
    }
  }

	/*
		Set the local register to his initial value (all resets active)
		*/
	*reset = 0;
	diva_os_wait (20) ;
}

static int pri_v3_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	if (!(*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] & PLX9054_L2PDBELL_INT_PENDING)) {
		return (0);
	}

	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = 1;

  IoAdapter->IrqCount++ ;
  if (IoAdapter->Initialized) {
    diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
  }

	return (1);
}

static void disable_pri_v3_interrupt (PISDN_ADAPTER IoAdapter) {
  *(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_L2PDBELL_INT_ENABLE;
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL]  = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
}

int diva_pri_v3_fpga_ready (PISDN_ADAPTER IoAdapter) {
	int ret = 0;

	if (IoAdapter->Initialized == 0) {
		int changes_detected = 0, loops = 800, state = 0;
		int pci_interrupt_state = (*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] & (1U << 8)) != 0;

		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~(1U << 8);  /* Disable PCI interrupts */
		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |=  (1U << 11); /* Enable local interrupt */

		while (changes_detected < 4 && loops-- > 0) {
			if (((*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] & (1U << 15)) != 0) != state) {
				state = !state;
				changes_detected++;
			}
		}

		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~(1U << 11);  /* Disable local interrupt */
		if (pci_interrupt_state != 0) {
			*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |=  (1U << 8); /* Enable PCI interrupts */
		}

		ret = (changes_detected >= 4) ? 0 : -1;
	}

	return (ret);
}

/*
  This function is called in case of critical system failure by MAINT
  driver and should save entire MAINT trace buffer to the memory of
  adapter.

  Returns zero on success.
	Returns -1 on error

	*/
static int pri_v3_save_maint_buffer (PISDN_ADAPTER IoAdapter, const void* data, dword length) {
	if (IoAdapter->ram != 0 && IoAdapter->MemorySize > MP_SHARED_RAM_OFFSET &&
			(IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET) > length) {
		if (IoAdapter->Initialized) {
			volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
			reset += (0x20000/sizeof(*reset));

			IoAdapter->Initialized = 0;
			disable_pri_v3_interrupt (IoAdapter);
			*reset = 0;
		}
		if (diva_pri_v3_fpga_ready (IoAdapter) == 0) {
			memcpy (IoAdapter->ram, data, length);
			return (0);
		}
	}

	return (-1);
}

/* -------------------------------------------------------------------------
  Install entry points for PRI Rev.3 Adapter
  ------------------------------------------------------------------------- */
void prepare_pri_v3_functions (PISDN_ADAPTER IoAdapter) {
	ADAPTER *a = &IoAdapter->a ;

	IoAdapter->MemorySize = MP2_MEMORY_SIZE;

	a->ram_in           = mem_in ;
	a->ram_inw          = mem_inw ;
	a->ram_in_buffer    = mem_in_buffer ;
	a->ram_look_ahead   = mem_look_ahead ;
	a->ram_out          = mem_out ;
	a->ram_outw         = mem_outw ;
	a->ram_out_buffer   = mem_out_buffer ;
	a->ram_inc          = mem_inc ;
	a->ram_offset       = pri_v3_ram_offset ;
	a->ram_out_dw       = mem_out_dw;
	a->ram_in_dw        = mem_in_dw;


	if (IoAdapter->host_vidi.vidi_active) {
		IoAdapter->out      = vidi_host_pr_out;
		IoAdapter->dpc      = vidi_host_pr_dpc;
		IoAdapter->tst_irq  = vidi_host_test_int;
		IoAdapter->clr_irq  = vidi_host_clear_int;
	} else {
		IoAdapter->out      = pr_out ;
		IoAdapter->dpc      = pr_dpc ;
		IoAdapter->tst_irq  = scom_test_int ;
		IoAdapter->clr_irq  = scom_clear_int ;
	}

	IoAdapter->pcm      = (struct pc_maint *)(MIPS_MAINT_OFFS - MP_SHARED_RAM_OFFSET) ;
	IoAdapter->stop     = stop_pri_v3_hardware ;

	IoAdapter->load     = load_pri_v3_hardware ;
	IoAdapter->disIrq   = disable_pri_v3_interrupt ;
	IoAdapter->rstFnc   = reset_pri_v3_hardware;
	IoAdapter->trapFnc  = pri_v3_cpu_trapped ;

	IoAdapter->diva_isr_handler = pri_v3_ISR;
	IoAdapter->save_maint_buffer_proc = pri_v3_save_maint_buffer;

#ifdef DIVA_ADAPTER_TEST_SUPPORTED
  IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
#endif
	IoAdapter->DetectDsps = diva_pri_v3_detect_dsps;

	diva_os_prepare_pri_v3_functions (IoAdapter);
}

#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* -------------------------------------------------------------------------
  Load protocol code to the PRI Card
  ------------------------------------------------------------------------- */
#define DOWNLOAD_ADDR(IoAdapter) \
 (&IoAdapter->ram[IoAdapter->downloadAddr & (IoAdapter->MemorySize - 1)])
static int pri_v3_protocol_load (PISDN_ADAPTER IoAdapter) {
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
 if ((Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16)
   | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24)))
 {
  DBG_TRC(("Protocol end address (%08x)", Addr))

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
  DBG_FTL(("Invalid protocol image - protocol end address missing"))
  xdiFreeFile (File);
  return (0) ;
 }

 DBG_LOG(("DSP code base 0x%08lx, max size 0x%08lx (%08lx,%02x)",
          IoAdapter->DspCodeBaseAddr, IoAdapter->MaxDspCodeSize,
          Addr, IoAdapter->InitialDspInfo))
 if ( FileLength > ((IoAdapter->DspCodeBaseAddr -
                     MP_CACHED_ADDR (0)) & (IoAdapter->MemorySize - 1)) )
 {
  xdiFreeFile (File);
  DBG_FTL(("Protocol code '%s' too long (%ld)",
           &IoAdapter->Protocol[0], FileLength))
  return (0) ;
 }
 IoAdapter->downloadAddr = 0;
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
#endif /* } */

/*
  The DSP check routine. Should be called only after FPGA
  was downloaded.
  */
static dword diva_pri_v3_detect_dsps (PISDN_ADAPTER IoAdapter) {
	volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
  byte* base = IoAdapter->ctlReg;
  dword ret = 0;
  dword row_offset[7] = {
    0x00000000,
    0x00000800, /* 1 - ROW 1 */
    0x00000840, /* 2 - ROW 2 */
    0x00001000, /* 3 - ROW 3 */
    0x00001040, /* 4 - ROW 4 */
    0x00000000  /* 5 - ROW 0 */
  };

  byte* dsp_addr_port, *dsp_data_port;
  int dsp_row, dsp_index, dsp_num;
  char d[31] ;
	reset += (0x20000/sizeof(*reset));

  if (!base || !IoAdapter->reset) {
    return (0);
  }

	if (*reset & 0x0000ff80) {
		*reset &= ~0x0000ff80; /* insert DSP reset */
		diva_os_wait(20);
	}
	*reset |= 0x0000ff80; /* remove DSP reset */
	diva_os_wait(50);

  for (dsp_num = 0; dsp_num < 31; dsp_num++) {
    dsp_row   = dsp_num / 7 + 1;
    dsp_index = dsp_num % 7;

    dsp_data_port = base;
    dsp_addr_port = base;

    dsp_data_port += row_offset[dsp_row];
    dsp_addr_port += row_offset[dsp_row];

    dsp_data_port += (dsp_index * 8);
    dsp_addr_port += (dsp_index * 8) + 0x80;

    if (!dsp_check_presence (dsp_addr_port, dsp_data_port, dsp_num+1)) {
      ret |= (1 << dsp_num);
    }
  }

	*reset &= ~0x0000ff80; /* Insert DSP reset again */
	diva_os_wait(10);


  if (!(ret & 0x10000000)) {
    DBG_ERR (("A: SIGNLE-DSP[1] failed"))
  }
  if (!(ret & 0x20000000)) {
    DBG_ERR (("A: SIGNLE-DSP[2] failed"))
  }
  if (!(ret & 0x40000000)) {
    DBG_ERR (("W: SINGLE-DSP[3] not found"))
  }
  for ( dsp_num = 0; dsp_num < 31; dsp_num++ )
    d[dsp_num] = (ret & (1L << dsp_num)) ? '+' : '-' ;
  if (ret & ~0x7000007f)
	{
    DBG_LOG(("  +----------------------------------------------------+"))
    DBG_LOG(("  | %c28 %c29         %c0  %c1       %c7       %c14      %c21 |",
                 d[28],d[29],       d[0],d[1],     d[7],     d[14],    d[21]))
    DBG_LOG(("  |                                                    |"))
    DBG_LOG(("  |     %c30         %c2  %c3   %c8  %c9   %c15 %c16  %c22 %c23 |",
                      d[30],       d[2],d[3], d[8],d[9],d[15],d[16],d[22],d[23]))
    DBG_LOG(("  |                                                    |"))
    DBG_LOG(("  |                 %c5  %c4   %c10 %c11  %c17 %c18  %c24 %c25 |",
                                 d[5],d[4],d[10],d[11],d[17],d[18],d[24],d[25]))
    DBG_LOG(("  |                                                    |"))
    DBG_LOG(("  |                     %c6   %c12 %c13  %c19 %c20  %c26 %c27 |",
                                     d[6],d[12],d[13],d[19],d[20],d[26],d[27]))
    DBG_LOG(("  |+--+            +-----------------------------------+"))
    DBG_LOG(("  ++  +------------+"))
		}
  else
  {
    DBG_LOG(("  +-------------------------+"))
    DBG_LOG(("  | %c28 %c29         %c0  %c1  |",
                 d[28],d[29],       d[0],d[1]))
	DBG_LOG(("  |                                        |"))
    DBG_LOG(("  |     %c30         %c2  %c3  |",
                      d[30],       d[2],d[3]))
	DBG_LOG(("  |                                        |"))
    DBG_LOG(("  |                 %c5  %c4  |",
                                  d[5],d[4]))
	DBG_LOG(("    |         |"))
    DBG_LOG(("  |                     %c6  |",
                                      d[6]))
    DBG_LOG(("  |+--+            +--------+"))
    DBG_LOG(("  ++  +------------+"))
	}

  DBG_LOG(("DSP's(present-absent):%08x-%08x", ret, ~ret & 0x7fffffff))

  return (ret);
}

#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/*
	Download FPGA, protocol and DSP code, configure and start hardware.
  */
static int load_pri_v3_hardware (PISDN_ADAPTER IoAdapter) {
  int i, d, started = 0;
 word  *signature ;
  byte *ram = IoAdapter->ram ;
	/*
		Verify and download hardware
		*/
  if (IoAdapter->Initialized) {
    DBG_ERR(("A(%d) adapter already running", IoAdapter->ANum))
		return (0);
	}
 if (!ram)
 {
		DBG_FTL(("A(%d) can't start, adapter not mapped", IoAdapter->ANum))
  return (0) ;
	}
	if (!(pri_v3_FPGA_download (IoAdapter))) {
		return (0);
	}
 d = diva_pri_v3_detect_dsps (IoAdapter) ;
 i = 0;
 while ( d != 0 )
 {
  if ( d & 1 )
   i++ ;
  d >>= 1 ;
 }
 /* Use high word of InitialDspInfo to store DspCount */
 IoAdapter->InitialDspInfo &= 0xffff ;
 IoAdapter->InitialDspInfo |= (i << 16) ;
 if ( i < 2 ) {
  DBG_FTL(("A(%d) DSP error!", IoAdapter->ANum))
		return (0);
	}
#if !SETPROTOCOLLOAD
 if (!xdiSetProtocol (IoAdapter, IoAdapter->ProtocolSuffix, 0)) {
		return (0);
	}
#endif
	if (!(pri_v3_protocol_load (IoAdapter))) {
		return (0) ;
	}
	if (!pri_telindus_load (IoAdapter)) {
		return (0) ;
	}

	/*
		Configure hardware
		*/
	IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
	memset (IoAdapter->ram, '\0', 256) ;
 diva_configure_protocol (IoAdapter, MP_SHARED_RAM_SIZE);
  /*
		Start hardware
	*/
  start_pri_v3_hardware (IoAdapter);

	DBG_TRC(("MABR0=%08lx", *(volatile dword*)&IoAdapter->reset[0xac]))
	DBG_TRC(("LBRD0=%08lx", *(volatile dword*)&IoAdapter->reset[0x18]))

  for (i = 0; i < 300; ++i) {
    diva_os_wait (10);
 if (*(volatile dword*)(ram + DIVA_PRI_V3_BOOT_SIGNATURE) == DIVA_PRI_V3_BOOT_SIGNATURE_OK) {
      DBG_LOG(("A(%d) Protocol startup time %d.%02d seconds",
                IoAdapter->ANum, (i / 100), (i % 100)))
      started = 1;
      break;
    }
  }

  if (!started) {
  DBG_FTL(("A(%d) Adapter start failed Signature=0x%08lx, TrapId=%08lx, boot count=%08lx",
              IoAdapter->ANum,
              *(volatile dword*)(ram + DIVA_PRI_V3_BOOT_SIGNATURE),
              *(volatile dword*)(ram + 0x80),
              *(volatile dword*)(ram + DIVA_PRI_V3_BOOT_COUNT)))
  return ( 0 ) ;
  }
 signature = (word *)(&IoAdapter->ram[0x1E]) ;
  /*
 * wait for signature in shared memory (max. 100 ms)
  */
 for ( started = i = 0 ; i < 10 ; ++i )
 {
  diva_os_wait (10) ;
  if ( signature[0] == 0x4447 )
  {
   DBG_TRC(("Protocol init time %d.%02d seconds",
            (i / 100), (i % 100) ))
   started = 1 ;
   break ;
  }
 }
 if ( !started ) {
  DBG_FTL(("%s: Adapter selftest failed (0x%04X)!",
           IoAdapter->Properties.Name, signature[0] >> 16))
  pri_v3_cpu_trapped (IoAdapter) ;
    return (0);
  }
	return (TRUE);
}

#else /* } { */

static int load_pri_v3_hardware (PISDN_ADAPTER IoAdapter) {
	return (0);
}

#endif /* } */

