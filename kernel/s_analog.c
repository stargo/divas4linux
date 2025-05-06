
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
#include "cardtype.h"
#include "di_defs.h"
#include "pc.h"
#include "pr_pc.h"
#include "di.h"
#include "vidi_di.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "divasync.h"
#include "pc_init.h"
#include "io.h"
#include "helpers.h"
#include "dsrv_analog.h"
#include "dsrv4bri.h"
#include "dsp_defs.h"
#include "divatest.h" /* Adapter test framework */
#include "fpga_rom.h"
#include "fpga_rom_xdi.h"

/*****************************************************************************/
#define	MAX_XLOG_SIZE	(64 * 1024)

static void analog_cpu_trapped (PISDN_ADAPTER IoAdapter) {
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
}

/* --------------------------------------------------------------------------
		Reset Diva Server Analog Hardware
	 -------------------------------------------------------------------------- */
static void reset_analog_hardware (PISDN_ADAPTER IoAdapter) {
	dword  volatile *Cntrl;

	Cntrl = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_RISC]);

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
		word volatile *Reset ;

		Reset = (word volatile *)IoAdapter->prom ;
		*Reset |= PLX9054_SOFT_RESET ;
		diva_os_wait (1) ;
		*Reset &= ~PLX9054_SOFT_RESET ;
		diva_os_wait (1);
		*Reset |= PLX9054_RELOAD_EEPROM ;
		diva_os_wait (1) ;
		*Reset &= ~PLX9054_RELOAD_EEPROM ;
		diva_os_wait (1);
		DBG_TRC(("resetted board @ reset addr 0x%08lx", Reset))
	}

	*Cntrl = 0 ;

	DBG_TRC(("resetted board @ cntrl addr 0x%08lx", Cntrl))
}

/* --------------------------------------------------------------------------
		Start Card CPU
	 -------------------------------------------------------------------------- */
void start_analog_hardware (PISDN_ADAPTER IoAdapter) {
	dword volatile *Reset ;

	Reset = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_RISC]);
	*Reset = MQ_RISC_COLD_RESET_MASK ;
	diva_os_wait (2) ;
	*Reset = MQ_RISC_WARM_RESET_MASK | MQ_RISC_COLD_RESET_MASK ;
	diva_os_wait (10) ;

	DBG_TRC(("started processor @ addr 0x%08lx", Reset))
}

/* --------------------------------------------------------------------------
		Stop Card CPU
	 -------------------------------------------------------------------------- */
static void stop_analog_hardware (PISDN_ADAPTER IoAdapter) {
	dword volatile *Reset ;
	dword volatile *Irq ;
	dword volatile *DspReset ;
	int reset_offset = MQ2_BREG_RISC;
	int irq_offset   = MQ2_BREG_IRQ_TEST;
	int hw_offset    = MQ2_ISAC_DSP_RESET;
	int i = 0;

  Reset = (dword volatile *)(&IoAdapter->ctlReg[reset_offset]) ;
  Irq   = (dword volatile *)(&IoAdapter->ctlReg[irq_offset]) ;
  DspReset = (dword volatile *)(&IoAdapter->ctlReg[hw_offset]);

	if (IoAdapter->host_vidi.vidi_started != 0) {
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
   * clear interrupt line (reset Local Interrupt Test Register)
   */
	*Reset = 0 ;
 	*DspReset = 0 ;

	IoAdapter->reset[PLX9054_INTCSR] = 0x00 ;	/* disable PCI interrupts */
	*Irq   = MQ_IRQ_REQ_OFF ;
	if (IoAdapter->msi != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
	}

	DBG_TRC(("stopped processor @ addr 0x%08lx", Reset))
}

/******************************************************************************/

#define FPGA_PROG   0x0001		// PROG enable low
#define FPGA_BUSY   0x0002		// BUSY high, DONE low
#define	FPGA_CS     0x000C		// Enable I/O pins
#define FPGA_CCLK   0x0100
#define FPGA_DOUT   0x0400
#define FPGA_DIN    FPGA_DOUT   // bidirectional I/O

int analog_FPGA_download (PISDN_ADAPTER IoAdapter) {
	int            bit, fpga_status, fpga_errors;
	byte           *File ;
	dword          code, code_start, FileLength ;
	word volatile *addr = (word volatile *)IoAdapter->prom ;
	word           val, baseval;

  switch (IoAdapter->cardType) {
    case CARDTYPE_DIVASRV_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE:
    case CARDTYPE_DIVASRV_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE:
    case CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM:
    case CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM:
      if (!(File = (byte *)xdiLoadFile("dspi8pe1.bit", &FileLength, 0))) {
        return (0);
      }
      code_start = 0;
      break;

    case CARDTYPE_DIVASRV_ANALOG_2PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_2PORT:
      if (!(File = (byte *)xdiLoadFile("dsap2.bit", &FileLength, 0))) {
        return (0);
      }
      code_start = 0;
      break;

    case CARDTYPE_DIVASRV_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_4PORT:
    case CARDTYPE_DIVASRV_ANALOG_8PORT:
    case CARDTYPE_DIVASRV_V_ANALOG_8PORT:
      if (!(File = qBri_check_FPGAsrc (IoAdapter, "dsap8.bit", &FileLength, &code_start))) {
        return 0;
      }
      break;

    default:
      DBG_TRC(("Illegal card type %d", IoAdapter->cardType))
      return (0);
  }

  IoAdapter->fpga_features |= PCINIT_FPGA_PLX_ACCESS_SUPPORTED ;

  for (fpga_status = 0, fpga_errors = 0; fpga_status == 0;) {
		code = code_start;
		baseval = FPGA_CS | FPGA_PROG;

		/*
			prepare download, pulse PROGRAM pin down.
			*/
		*addr = baseval & ~FPGA_PROG ; // PROGRAM low pulse
		diva_os_wait (50) ;  // wait until FPGA initiated internal memory clear
		*addr = baseval ;              // release
		diva_os_wait (200) ;  // wait until FPGA finished internal memory clear

		/*
			check done pin, must be low
			*/
		if ((*addr & FPGA_BUSY) != 0) {
			DBG_FTL(("FPGA download: acknowledge for FPGA memory clear missing"))
			fpga_status = -1;
			break;
		}
		/*
			put data onto the FPGA
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
		diva_os_wait (120) ;
		val = *addr ;

		if ( !(val & FPGA_BUSY) )
		{
			DBG_FTL(("FPGA download: chip remains in busy state (0x%04x)", val))
			fpga_status = -2;
			break;
		}

		if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
			volatile dword* intcsr = (volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW];
			/*
				Check if FPGA is really started
				*/
			*intcsr  &= ~(1 << 8);
			*intcsr  |=  (1 << 11);

			if ((*intcsr & (1 << 15)) != 0) {
				volatile dword* Irq    = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_IRQ_TEST]);
				*Irq = MQ_IRQ_REQ_OFF;
				bit = 100;
				while (((*intcsr & (1 << 15)) != 0) && bit--);
				if ((*intcsr & (1 << 15)) == 0) {
					fpga_status = 1;
				} else if (++fpga_errors >= 3) {
					fpga_status = -3;
				}
			} else {
				DBG_LOG(("Skip FPGA test"))
				fpga_status = 1;
			}
			*intcsr  &= ~(1 << 11);
			*intcsr  |=  (1 << 8);
		} else {
			fpga_status = 1;
		}
	}

	xdiFreeFile (File) ;

	if (fpga_status < 0) {
		DBG_FTL(("Failed to start FPGA (%d)",	fpga_status))
		return (0);
	} else if (fpga_errors != 0) {
		DBG_LOG(("Started FPGA after %d trials", fpga_errors))
	}

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) != 0) {
		byte Bus   = (byte)IoAdapter->BusNumber;
		byte Slot  = (byte)IoAdapter->slotNumber;
		void* hdev = IoAdapter->hdev;
		dword pedevcr = 0;
		byte  MaxReadReq;
		byte revID = 0;

		PCIread (Bus, Slot, 0x54 /* PEDEVCR */, &pedevcr, sizeof(pedevcr), hdev);
		if ((MaxReadReq = (byte)((pedevcr >> 12) & 0x07)) != 0x02) {
			/*
				A.4.15. DMA Read access on PCIe side can fail when Max
				read request size is set to 128 or 256 bytes(J mode only)
				Applies to:
				  Silicon revision A0 (Intel part number D39505-001 or D44469-001)
				  Silicon revision A1 (Intel part number D66398-001 or D66396-001)
				  Device initializes max_read_request_size in PEDEVCR register (see A.3.1.28).
				  PCI Express Device Control register (PEDEVCR; PCI: 0x54; Local: 0x1054 )
				  to 512bytes after reset. On certain systems during enumeration, BIOS was writing zero
				  to this register setting max_read_request_size to 128bytes. Device currently supports
				  max_read_request size of 512bytes. Setting this register to 128/256 bytes can cause
				  DMA read access to fail on the PCIe side.
				  Driver should set max_read_request_size to 512bytes while initializing the boards.
			*/
			DBG_LOG(("PEDEVCR:%08x, MaxReadReq:%02x", pedevcr, MaxReadReq))
			pedevcr &= ~(0x07U << 12);
			pedevcr |=  (0x02U << 12);
			// Retrieve RevisionID from PCI Config Space (HERA/FPGA-Seaville Replacement is >=3 limited to 0x10)
			PCIread(Bus, Slot, 0x08, &revID, sizeof(revID), hdev);
			if (!((revID >=0x3) && (revID <= 0x10)))
			{
				PCIwrite (Bus, Slot, 0x54 /* PEDEVCR */, &pedevcr, sizeof(pedevcr), hdev);
			}
		}

		diva_xdi_show_fpga_rom_features (IoAdapter->ctlReg + MQ2_FPGA_INFO_ROM);

		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (1U << 21);
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (1U << 18);
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] &= ~(1U << 24);
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |=  (1U << 8); /* Disable prefetch */
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |=  (1U << 6);
	} else {
		/*
			Set PLX Local BUS latency timer via MABR0
			*/
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] &= ~0x000000ff;
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (((dword)1 << 16)|((dword)1 << 24)|((dword)1 << 25));
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (0x00000000 | ((dword)1 << 23));

		/*
			Disable prefeth for memory space zero via LBRD0
			*/
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] &= ~(((dword)1 << 7) | ((dword)1 << 24));
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |= ((dword)1 << 27);
	}

  if (IoAdapter->DivaAdapterTestProc) {
    if ((*(IoAdapter->DivaAdapterTestProc))(IoAdapter)) {
      return (0);
    }
  }

	DBG_TRC(("FPGA download successful"))

	return (1) ;
}

#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* -------------------------------------------------------------------------
  Load protocol code to the analog adapter
  ------------------------------------------------------------------------- */
#define DOWNLOAD_ADDR(IoAdapter) \
 (&IoAdapter->ram[IoAdapter->downloadAddr & (IoAdapter->MemorySize - 1)])
static int analog_protocol_load (PISDN_ADAPTER IoAdapter) {
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

	if (DIVA_ANALOG_PCIE(IoAdapter) == 0) {
	  IoAdapter->MaxDspCodeSize = (MQ_UNCACHED_ADDR (IoAdapter->MemorySize)
 	                           - IoAdapter->DspCodeBaseAddr) & (IoAdapter->MemorySize - 1) ;
	} else {
	  IoAdapter->MaxDspCodeSize = (MQ_UNCACHED_ADDR (DIVA_ANALOG_PCIE_REAL_SDRAM_SIZE)
 	                           - IoAdapter->DspCodeBaseAddr) & (DIVA_ANALOG_PCIE_REAL_SDRAM_SIZE - 1) ;
	}

  Addr = IoAdapter->DspCodeBaseAddr ;

	((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT]   =(byte)1;
	((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT+1] = (byte)IoAdapter->cardType;

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
                     MQ_CACHED_ADDR (0)) & (IoAdapter->MemorySize - 1)) )
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
  Helper used to download dsp code to analog adapter
  ------------------------------------------------------------------------- */
#if !defined(DIVA_XDI_VIRTUAL_FLAT_MEMORY) /* { */
static long analog_download_buffer (OsFileHandle *fp, long length, void **addr) {
	PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)fp->sysLoadDesc ;
	dword        *sharedRam ;
	*addr = ULongToPtr(IoAdapter->downloadAddr) ;
	dword segment_length = IoAdapter->MemorySize;
	dword segment        = IoAdapter->downloadAddr/segment_length;
	volatile dword* las0ba_ctrl = (volatile dword*)&IoAdapter->reset[0x04];
	dword las0ba_original = *las0ba_ctrl;
	long written;

	if ( ((dword) length) > IoAdapter->DspCodeBaseAddr +
                         IoAdapter->MaxDspCodeSize - IoAdapter->downloadAddr )
	{
		DBG_FTL(("%s: out of card memory during DSP download (0x%X)",
           IoAdapter->Properties.Name,
           IoAdapter->downloadAddr + length))
		return (-1) ;
	}

  while (length != 0) {
    long to_copy = MIN(length, segment_length - IoAdapter->downloadAddr % segment_length);

    if (segment != 0) {
      *las0ba_ctrl = (dword)(las0ba_original + segment * segment_length);
    }
		sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
		written = fp->sysFileRead (fp, sharedRam, to_copy);
		if (written != length) {
			if (segment != 0) {
				*las0ba_ctrl = las0ba_original;
			}
			return (-1);
		}
    length -= to_copy;
		IoAdapter->downloadAddr += to_copy;
    if (length != 0) {
      segment++;
    }
  }

	IoAdapter->downloadAddr  = (IoAdapter->downloadAddr + 3) & (~3) ;

	if (segment != 0) {
		*las0ba_ctrl = las0ba_original;
	}

	return (0) ;
}
#else /* } { */
static long analog_download_buffer (OsFileHandle *fp, long length, void **addr) {
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

 if (DIVA_ANALOG_PCIE(IoAdapter) == 0) {
  sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
 } else {
  sharedRam = (dword *)(&IoAdapter->ram[IoAdapter->downloadAddr & (DIVA_ANALOG_PCIE_REAL_SDRAM_SIZE - 1)]);
 }
 if ( fp->sysFileRead (fp, sharedRam, length) != length )
  return (-1) ;
 IoAdapter->downloadAddr += length ;
 IoAdapter->downloadAddr  = (IoAdapter->downloadAddr + 3) & (~3) ;
 return (0) ;
}
#endif

/* -------------------------------------------------------------------------
  Download DSP code to analog adapter
  ------------------------------------------------------------------------- */
static dword analog_telindus_load (PISDN_ADAPTER IoAdapter) {
 char                *error ;
 OsFileHandle        *fp ;
 t_dsp_portable_desc *download_table ;
 word                 download_count ;
 dword               *sharedRam ;
 dword                FileLength ;

 download_table = (t_dsp_portable_desc *)diva_os_malloc (0,
                  DSP_MAX_DOWNLOAD_COUNT * sizeof(t_dsp_portable_desc));
 if ( !download_table )
 {
  DBG_FTL(("Out of memory allocating download table"))
  return (0) ;
 }
 if ( !(fp = OsOpenFile (DSP_TELINDUS_FILE)) )
 {
  diva_os_free (0, download_table) ;
  return (0) ;
 }
 IoAdapter->downloadAddr = (IoAdapter->DspCodeBaseAddr
                         + sizeof(dword)
                         + DSP_MAX_DOWNLOAD_COUNT*sizeof(t_dsp_portable_desc)
                         + 3) & (~3) ;
 FileLength      = fp->sysFileSize ;
 fp->sysLoadDesc = (void *)IoAdapter ;
 fp->sysCardLoad = analog_download_buffer ;
 download_count = DSP_MAX_DOWNLOAD_COUNT ;
 memset (download_table, '\0',
         DSP_MAX_DOWNLOAD_COUNT * sizeof(t_dsp_portable_desc)) ;
/*
 * set start address for download (use autoincrement mode !)
 */
 error = dsp_read_file (fp, (word)(IoAdapter->cardType),
                        &download_count, NULL, download_table) ;
 if ( error )
 {
  DBG_FTL(("Download file error: %s", error))
  OsCloseFile (fp) ;
  diva_os_free (0, download_table) ;
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
 memcpy (&sharedRam[1], download_table,
         DSP_MAX_DOWNLOAD_COUNT * sizeof(t_dsp_portable_desc)) ;
 diva_os_free (0, download_table) ;
 return (FileLength) ;
}

static int load_analog_hardware (PISDN_ADAPTER IoAdapter) {
  int i, started = 0;
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
	if (!analog_FPGA_download (IoAdapter)) {
		return (0);
	}
#if !SETPROTOCOLLOAD
	if (!xdiSetProtocol (IoAdapter, IoAdapter->ProtocolSuffix, 0)) {
		return (0);
	}
#endif
	if (!analog_protocol_load (IoAdapter)) {
		return (0) ;
	}
	if (!analog_telindus_load (IoAdapter)) {
	return (0);
}

	/*
		Configure hardware
		*/
	IoAdapter->ram += MP_SHARED_RAM_OFFSET ;
	memset (IoAdapter->ram, '\0', 256) ;
	diva_configure_protocol (IoAdapter, MQ_SHARED_RAM_SIZE);

  /*
		Start hardware
	*/
  start_analog_hardware (IoAdapter);
	signature = (word *)(&IoAdapter->ram[0x1E]) ;
/*
 *	wait for signature in shared memory (max. 3 seconds)
 */
	for ( started = i = 0 ; i < 300 ; ++i )
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
		analog_cpu_trapped (IoAdapter) ;
		return (0) ;
	}

	return (TRUE);
}

#else /* } { */
static int load_analog_hardware (PISDN_ADAPTER IoAdapter) {
	return (0);
}
#endif /* } */

/* --------------------------------------------------------------------------
		Card ISR
	 -------------------------------------------------------------------------- */
static int analog_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	dword volatile     *Irq ;

	if (!(IoAdapter->reset[PLX9054_INTCSR] & 0x80)) {
		return (0) ;
  }

  /*
   * clear interrupt line (reset Local Interrupt Test Register)
   */
	Irq = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_IRQ_TEST]);
	*Irq = MQ_IRQ_REQ_OFF;

  if (IoAdapter && IoAdapter->Initialized && IoAdapter->tst_irq (&IoAdapter->a)) {
			IoAdapter->IrqCount++ ;
			diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
  }

	/*
		Always serviced if cause indicated by PLX.
		tst_irq may return false even it interrupt is indicated
		by PLX. This is in case DPC running on parallel CPU already
		called clr_irq
		*/

	return (1) ;
}

/*
  MSI interrupt is not shared and must always return true
  The MSI acknowledge supposes that only one message was allocated for
  MSI. This allows to process interrupt without read of interrupt status
  register
  */
static int analog_msi_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	/*
		clear interrupt line (reset Local Interrupt Test Register)
		*/
	*(dword volatile *)&IoAdapter->ctlReg[MQ2_BREG_IRQ_TEST] = MQ_IRQ_REQ_OFF;

	/*
		Acknowledge MSI interrupt
		*/
	*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;

	if (IoAdapter->Initialized != 0 && IoAdapter->tst_irq (&IoAdapter->a) != 0) {
		IoAdapter->IrqCount++ ;
		diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
	}

  return (1);
}

/* --------------------------------------------------------------------------
		Disables the interrupt on the card
	 -------------------------------------------------------------------------- */
static void disable_analog_interrupt (PISDN_ADAPTER IoAdapter) {
	dword volatile *Irq = (dword volatile *)(&IoAdapter->ctlReg[MQ2_BREG_IRQ_TEST]);

	if (IoAdapter->ControllerNumber != 0)
		return;

	/*
		This is bug in the PCIe device. In case bit 8 of INTCSR is cleared
		in the time interrupt is active then device will generate unexpected
		interrupt or hang if in MSI mode. For this reason clear bit 11 of INTCSR
		first, acknowledge and clear the cause of interrupt and finally clear
		bit 8 of INTCSR.
		This behavior is compatible with existing devices and PCIe device does not
		supports I/O
		*/
	*((volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW]) &= ~(1U << 11);
	*Irq   = MQ_IRQ_REQ_OFF ;

	if (IoAdapter->msi != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
	}

	*((volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW]) &= ~((1U << 8) | (1U << 11));
}

/* --------------------------------------------------------------------------
		Install Adapter Entry Points
	 -------------------------------------------------------------------------- */
void prepare_analog_functions (PISDN_ADAPTER IoAdapter) {
	ADAPTER *a;

	a = &IoAdapter->a ;

	if (DIVA_ANALOG_PCIE(IoAdapter) == 0) {
		IoAdapter->MemorySize = MQ2_MEMORY_SIZE;
	} else {
		IoAdapter->MemorySize = MP2_MEMORY_SIZE;
	}

	a->ram_in           = mem_in ;
	a->ram_inw          = mem_inw ;
	a->ram_in_buffer    = mem_in_buffer ;
	a->ram_look_ahead   = mem_look_ahead ;
	a->ram_out          = mem_out ;
	a->ram_outw         = mem_outw ;
	a->ram_out_buffer   = mem_out_buffer ;
	a->ram_inc          = mem_inc ;

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

	IoAdapter->pcm      = (struct pc_maint *)MIPS_MAINT_OFFS ;

	IoAdapter->load     = load_analog_hardware ;

	IoAdapter->disIrq   = disable_analog_interrupt ;
	IoAdapter->rstFnc   = reset_analog_hardware ;
	IoAdapter->stop     = stop_analog_hardware ;
	IoAdapter->trapFnc  = analog_cpu_trapped ;

	IoAdapter->diva_isr_handler = (IoAdapter->msi == 0) ? analog_ISR : analog_msi_ISR;

	IoAdapter->a.io       = (void*)IoAdapter ;

#if defined(DIVA_ADAPTER_TEST_SUPPORTED)
  IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
#endif

  diva_os_prepare_analog_functions (IoAdapter);
}

/* -------------------------------------------------------------------------- */





