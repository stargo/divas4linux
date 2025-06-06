
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
#include "dsrv4bri.h"
#include "dsp_defs.h"
#include "sdp_hdr.h"
#include "divatest.h" /* Adapter test framework */
#include "fpga_rom.h"
#include "fpga_rom_xdi.h"

/*****************************************************************************/
#define	MAX_XLOG_SIZE	(64 * 1024)

/* There where a problem on some machines that applications crashs after
 *  loading ISDNXDI. The internal page table was full. The page table
 *  is a limited resource, mapping of 16MB occupies 4000 entries, that's
 *  a lot. Thatswhy the address space will be unmapped if adapter is
 *  loaded and only the 64k shared RAM will be mapped.
 * !!!The define UNMAP_UNUSED_MEM is used for portability reason!!!
 */

/* --------------------------------------------------------------------------
		Recovery XLOG from QBRI Card
	 -------------------------------------------------------------------------- */
static void qBri_cpu_trapped (PISDN_ADAPTER IoAdapter) {
	byte  *base ;
	dword   regs[4], TrapID, offset, size, min = 0, max = 0;
	Xdesc   xlogDesc ;
	int factor = (IoAdapter->tasks == 1) ? 1 : 2;

/*
 *	check for trapped MIPS 46xx CPU, dump exception frame
 */

  if (IoAdapter->reentrant != 0) {
    /*
      New protocol code version. XLOG buffer located inside of the shared memory
      segment.
      */
    offset = 0;
    min    = IoAdapter->ControllerNumber * (MP_SHARED_RAM_SIZE+MP_SHARED_RAM_OFFSET) +
                                                                    MP_SHARED_RAM_OFFSET;
    max    = min + MP_SHARED_RAM_SIZE; /* Shared memory end */
    base   = IoAdapter->ram - MP_SHARED_RAM_OFFSET;
  } else {
    /*
      Original protocol code version
      */
    offset = IoAdapter->ControllerNumber * (IoAdapter->MemorySize >> factor) ;
    base   = IoAdapter->ram - offset - ((IoAdapter->MemorySize >> factor) - MQ_SHARED_RAM_SIZE) ;
  }

	TrapID = *(dword *)(&base[0x80]) ;

	if ( (TrapID == 0x99999999) || (TrapID == 0x99999901) )
	{
		dump_trap_frame (IoAdapter, &base[0x90]) ;
		IoAdapter->trapped = 1 ;
	}

	memcpy (&regs[0], &(base + offset)[0x70], sizeof(regs)) ;
	regs[0] &= IoAdapter->MemorySize - 1 ;

  if (((IoAdapter->reentrant != 0) && (regs[0] > min) && (regs[0] < max)) ||
      ((regs[0] >= offset) && (regs[0] < offset + (IoAdapter->MemorySize >> factor) - 1)))
  {
    if (IoAdapter->reentrant != 0) {
      size = max - regs[0] - 12;
    } else {
      size = offset + (IoAdapter->MemorySize >> factor) - regs[0] ;
    }

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
	if ( IoAdapter->trapped )
		diva_os_dump_file(IoAdapter) ;
}

/* ---------------------------------------------------------------------
			Dump card memory to file
	 --------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
		Reset QBRI Hardware
	 -------------------------------------------------------------------------- */
static void reset_qBri_hardware (PISDN_ADAPTER IoAdapter) {
	dword  volatile *qBriCntrl ;

	qBriCntrl = (dword volatile *)(&IoAdapter->ctlReg[DIVA_4BRI_REVISION(IoAdapter) ? (MQ2_BREG_RISC) : (MQ_BREG_RISC)]);

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
		word volatile *qBriReset ;
		qBriReset = (word volatile *)IoAdapter->prom ;
		*qBriReset |= PLX9054_SOFT_RESET ;
		diva_os_wait (1) ;
		*qBriReset &= ~PLX9054_SOFT_RESET ;
		diva_os_wait (1);
		*qBriReset |= PLX9054_RELOAD_EEPROM ;
		diva_os_wait (1) ;
		*qBriReset &= ~PLX9054_RELOAD_EEPROM ;
		diva_os_wait (1);
		DBG_TRC(("resetted board @ reset addr 0x%08lx", qBriReset))
	}

	*qBriCntrl = 0 ;

	DBG_TRC(("resetted board @ cntrl addr 0x%08lx", qBriCntrl))
}

/* --------------------------------------------------------------------------
		Start Card CPU
	 -------------------------------------------------------------------------- */
void start_qBri_hardware (PISDN_ADAPTER IoAdapter) {
	dword volatile *qBriReset ;

	qBriReset = (dword volatile *)(&IoAdapter->ctlReg[DIVA_4BRI_REVISION(IoAdapter) ? (MQ2_BREG_RISC) : (MQ_BREG_RISC)]);
	*qBriReset = MQ_RISC_COLD_RESET_MASK ;
	diva_os_wait (2) ;
	*qBriReset = MQ_RISC_WARM_RESET_MASK | MQ_RISC_COLD_RESET_MASK ;
	diva_os_wait (10) ;

	DBG_TRC(("started processor @ addr 0x%08lx", qBriReset))

}

/* --------------------------------------------------------------------------
		Stop Card CPU
	 -------------------------------------------------------------------------- */
static void stop_qBri_hardware (PISDN_ADAPTER IoAdapter) {
	dword volatile *qBriReset ;
	dword volatile *qBriIrq ;
	dword volatile *qBriIsacDspReset ;
	dword volatile *qBriIpacxConfig ;
	int rev2 = DIVA_4BRI_REVISION(IoAdapter);
	int reset_offset = rev2 ? (MQ2_BREG_RISC)      : (MQ_BREG_RISC);
	int irq_offset   = rev2 ? (MQ2_BREG_IRQ_TEST)  : (MQ_BREG_IRQ_TEST);
	int hw_offset    = rev2 ? (MQ2_ISAC_DSP_RESET) : (MQ_ISAC_DSP_RESET);
	dword i = 0;

	if ( IoAdapter->ControllerNumber > 0 )
		return ;
 qBriReset = (dword volatile *)(&IoAdapter->ctlReg[reset_offset]) ;
 qBriIrq   = (dword volatile *)(&IoAdapter->ctlReg[irq_offset]) ;
 qBriIsacDspReset = (dword volatile *)(&IoAdapter->ctlReg[hw_offset]);
 qBriIpacxConfig = rev2 ? (dword volatile *)(&IoAdapter->ctlReg[MQ2_IPACX_CONFIG]) : NULL ;

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
 *	clear interrupt line (reset Local Interrupt Test Register)
 */
	*qBriReset = 0 ;
 	*qBriIsacDspReset = 0 ;
	if ( qBriIpacxConfig ) *qBriIpacxConfig = 0	;	/* Reset hardware to TE mode */

	IoAdapter->reset[PLX9054_INTCSR] = 0x00 ;	/* disable PCI interrupts */
	*qBriIrq   = MQ_IRQ_REQ_OFF ;
	if (IoAdapter->msi != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
	}

	DBG_TRC(("stopped processor @ addr 0x%08lx", qBriReset))

}

/* --------------------------------------------------------------------------
		FPGA download
	 -------------------------------------------------------------------------- */
#define FPGA_NAME_OFFSET         0x10

byte * qBri_check_FPGAsrc (PISDN_ADAPTER IoAdapter, char *FileName,
                           dword *Length, dword *code) {
	byte *File ;
	char  *fpgaFile, *fpgaType, *fpgaDate, *fpgaTime ;
	dword  fpgaFlen,  fpgaTlen,  fpgaDlen, cnt, year, i ;

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

	if ( (File[i] & 0xF0) != 0x20 )
	{
    /*
      Check if file is created by new tool
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
    *code = i;

    if (DIVA_4BRI_REVISION(IoAdapter) != 0) {
      IoAdapter->fpga_features |= PCINIT_FPGA_PLX_ACCESS_SUPPORTED ;
    }

		DBG_LOG(("FPGA download: use new file format"))

    return (File);
	}

	fpgaFlen = (dword)  File[FPGA_NAME_OFFSET - 1] ;
	if ( fpgaFlen == 0 )
		fpgaFlen = 12 ;
	fpgaFile = (char *)&File[FPGA_NAME_OFFSET] ;
	fpgaTlen = (dword)  fpgaFile[fpgaFlen + 2] ;
	if ( fpgaTlen == 0 )
		fpgaTlen = 10 ;
	fpgaType = (char *)&fpgaFile[fpgaFlen + 3] ;
	fpgaDlen = (dword)  fpgaType[fpgaTlen + 2] ;
	if ( fpgaDlen == 0 )
		fpgaDlen = 11 ;
	fpgaDate = (char *)&fpgaType[fpgaTlen + 3] ;
	fpgaTime = (char *)&fpgaDate[fpgaDlen + 3] ;
	cnt = (dword)(((File[  i  ] & 0x0F) << 20) + (File[i + 1] << 12)
	             + (File[i + 2]         <<  4) + (File[i + 3] >>  4)) ;

  if ( (dword)(i + (cnt / 8)) > *Length )
  {
    DBG_FTL(("FPGA download: 'file too small (%ld < %ld)",
             *Length, code + ((cnt + 7) / 8) ))
    xdiFreeFile (File) ;
    return (NULL) ;
  }
	i = 0 ;
	do
	{
		while ( (fpgaDate[i] != '\0')
		     && ((fpgaDate[i] < '0') || (fpgaDate[i] > '9')) )
		{
			i++;
		}
		year = 0 ;
		while ( (fpgaDate[i] >= '0') && (fpgaDate[i] <= '9') )
			year = year * 10 + (fpgaDate[i++] - '0') ;
	} while ( (year < 2000) && (fpgaDate[i] != '\0') );

	switch (IoAdapter->cardType) {
		case CARDTYPE_DIVASRV_B_2F_PCI:
		case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
			break;

		default:
	    if ( year >= 2001 ) {
				IoAdapter->fpga_features |= PCINIT_FPGA_PLX_ACCESS_SUPPORTED ;
			}
	}

	switch (IoAdapter->cardType) {
		case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
		case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
		case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
	    if (year >= 2007) {
				IoAdapter->fpga_features |= PCINIT_FPGA_PROGRAMMABLE_OPERATION_MODES_SUPPORTED;
			}
			break;
	}

	DBG_LOG(("FPGA[%s] file %s (%s %s) len %d",
	         fpgaType, fpgaFile, fpgaDate, fpgaTime, cnt))
	return (File) ;
}

/******************************************************************************/

#define FPGA_PROG   0x0001		// PROG enable low
#define FPGA_BUSY   0x0002		// BUSY high, DONE low
#define	FPGA_CS     0x000C		// Enable I/O pins
#define FPGA_CCLK   0x0100
#define FPGA_DOUT   0x0400
#define FPGA_DIN    FPGA_DOUT   // bidirectional I/O

int qBri_FPGA_download (PISDN_ADAPTER IoAdapter) {
	int            bit, fpga_status, fpga_errors;
	byte           *File ;
	dword          code, code_start, FileLength ;
	word volatile *addr = (word volatile *)IoAdapter->prom ;
	word           val, baseval;
	volatile dword* Irq    = (dword volatile*)(&IoAdapter->ctlReg[DIVA_4BRI_REVISION(IoAdapter) ? (MQ2_BREG_IRQ_TEST) : (MQ_BREG_IRQ_TEST)]);

  if (DIVA_4BRI_REVISION(IoAdapter)) {
    char* name;

    switch (IoAdapter->cardType) {
      case CARDTYPE_DIVASRV_B_2F_PCI:
      case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
        name = "dsbri2f.bit";
        break;

      case CARDTYPE_DIVASRV_B_2M_V2_PCI:
      case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
      case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
        name = "dsbri2m.bit";
        break;

      default:
        name = "ds4bri2.bit";
    }

    File = qBri_check_FPGAsrc (IoAdapter, name, &FileLength, &code_start);
  }
  else
  {
    File = qBri_check_FPGAsrc (IoAdapter, "ds4bri.bit",
                               &FileLength, &code_start);
  }
	if ( !File )
		return (0) ;

	for (fpga_status = 0, fpga_errors = 0; fpga_status == 0;) {
		code = code_start;
		baseval = FPGA_CS | FPGA_PROG;

		/*
		 *	prepare download, pulse PROGRAM pin down.
		 */
		*addr = baseval & ~FPGA_PROG ; // PROGRAM low pulse
		diva_os_wait (50) ;  // wait until FPGA initiated internal memory clear
		*addr = baseval ;              // release
		diva_os_wait (200) ;  // wait until FPGA finished internal memory clear
		/*
		 *	check done pin, must be low
		 */
		if ( *addr & FPGA_BUSY )
		{
			DBG_FTL(("FPGA download: acknowledge for FPGA memory clear missing"))
			fpga_status = -1;
			break;
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
		diva_os_wait (120) ;
		val = *addr ;

		if ( !(val & FPGA_BUSY) )
		{
			DBG_FTL(("FPGA download: chip remains in busy state (0x%04x)", val))
			fpga_status = -2;
			break;
		}

		if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) &
        DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO) != 0) {
			diva_xdi_show_fpga_rom_features (IoAdapter->ctlReg + MQ2_FPGA_INFO_ROM);
		}

		if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
			volatile dword* intcsr = (volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW];

			/*
				Check if FPGA is really started
				*/
			*intcsr  &= ~(1 << 8);
			*intcsr  |=  (1 << 11);

			if ((*intcsr & (1 << 15)) != 0) {
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

  if (IoAdapter->DivaAdapterTestProc) {
    if ((*(IoAdapter->DivaAdapterTestProc))(IoAdapter)) {
      return (0);
    }
  }

	DBG_TRC(("FPGA download successful"))

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) != 0) {
		byte Bus   = (byte)IoAdapter->BusNumber;
		byte Slot  = (byte)IoAdapter->slotNumber;
		void* hdev = IoAdapter->hdev;
		dword pedevcr = 0;
		byte  MaxReadReq;
		byte revID = 0;

		PCIread (Bus, Slot, 0x54 /* PEDEVCR */, &pedevcr, sizeof(pedevcr), hdev);
		MaxReadReq = (byte)((pedevcr >> 12) & 0x07);

		if (MaxReadReq != 0x02) {
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
				// Write to 0x54 only if it is *NOT* HERA/FPGA-Seaville Replacement
				PCIwrite (Bus, Slot, 0x54 /* PEDEVCR */, &pedevcr, sizeof(pedevcr), hdev);
			}
		}

		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (1U << 18);
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (1U << 21);
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] &= ~(1U << 24);
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |=  (1U << 8); /* Disable prefetch */
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |=  (1U << 6);
	}

	return (1) ;
}

#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/******************************************************************************/
/*
 * GetAdrOfDspTable
 *
 * The current protocol code find the memory addresses of the dsp files
 * through a table. The table is written in the memory directly after the
 * protocol code of the highest controller. The protocol code self include
 * the address where the protocol ends in the memory.
 */
#if 0
static int
GetAdrOfDspTable (PISDN_ADAPTER BaseIoAdapter, dword *TableAddr)
{
	unsigned long              FileLength;
	unsigned long *            File;
	dword              Addr;
	dword              i;
	PISDN_ADAPTER      IoAdapter;

	/*--- search the highest controller ------------------------------------*/
	for (i = 0 ; i < BaseIoAdapter->tasks ; i++)
	{
		IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i] ;

		if (IoAdapter->ControllerNumber == (IoAdapter->tasks-1))
			break ;
	}
	if ( i >=  BaseIoAdapter->tasks){
		DBG_FTL(("The highest controller(%d) not exist in the controller list",
		    IoAdapter->tasks-1))
		return (0) ;
	}

	/*--- read the protocol file of the highest controller -----------------*/
	File = (dword *)xdiLoadArchive (IoAdapter, &FileLength, 0) ;
	if ( !File )
		return (0) ;

	/*--- read the end address of protocol code from the protocol code -----*/
	Addr =(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR    ]))      )
	    | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) <<  8)
	    | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16)
	    | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24) ;

	/*--- compute the address of the dsp table -----------------------------*/
	*TableAddr = (Addr + 3) & (~3) ;

	/* Address can be zero for old protocol files. Then we return 0, */
	/* which indicate that no end address is available.              */

	DBG_TRC(("Address of DSP file table: 0x%08x", *TableAddr))

	/*--- free memory of the readed protocol file --------------------------*/
	xdiFreeFile (File) ;

	return (1) ;
}
#endif

/* --------------------------------------------------------------------------
		Download protocol code to the adapter
	 -------------------------------------------------------------------------- */

#define	DOWNLOAD_ADDR(IoAdapter) (&IoAdapter->ram[IoAdapter->downloadAddr & (IoAdapter->MemorySize - 1)])


static int qBri_reentrant_protocol_load (PISDN_ADAPTER IoAdapter)
{
	dword  FileLength ;
	dword *File ;
	dword *sharedRam ;
	dword  Addr ;

	if (IoAdapter->reentrant == 0) {
		return (0);
	}
	if (IoAdapter->ControllerNumber != 0) {
		return (1);
	}

	if (!(File = (dword *)xdiLoadArchive (IoAdapter, &FileLength, 0))) {
		return (0) ;
	}

	IoAdapter->features = diva_get_protocol_file_features ((byte*)File,
																												 OFFS_PROTOCOL_ID_STRING,
																												 IoAdapter->ProtocolIdString,
																												 sizeof(IoAdapter->ProtocolIdString));
	IoAdapter->a.protocol_capabilities = IoAdapter->features;
	DBG_LOG(("Loading %s", IoAdapter->ProtocolIdString))
	IoAdapter->InitialDspInfo = 0;

	((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT]   = (byte)IoAdapter->tasks;
	((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT+1] = (byte)IoAdapter->cardType;

	if ((Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))     |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)  |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16) |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24))) {
		DBG_TRC(("Protocol end address (%08x)", Addr))

		IoAdapter->DspCodeBaseAddr = (Addr + 15) & (~15); /* Align at paragraph boundary */
		IoAdapter->MaxDspCodeSize =
			(MP_UNCACHED_ADDR (IoAdapter->MemorySize) - IoAdapter->DspCodeBaseAddr) & (IoAdapter->MemorySize - 1);
		Addr = IoAdapter->DspCodeBaseAddr;
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR]     = (byte) Addr;
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 1] = (byte)(Addr >> 8);
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 2] = (byte)(Addr >> 16);
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 3] = (byte)(Addr >> 24);
		IoAdapter->InitialDspInfo = 0x80 ;
	} else {
		DBG_FTL(("Invalid protocol image - protocol end address missing"))
		xdiFreeFile (File);
		return (0) ;
	}

	DBG_LOG(("DSP code base 0x%08lx, max size 0x%08lx (%08lx,%02x)",
					IoAdapter->DspCodeBaseAddr, IoAdapter->MaxDspCodeSize,
					Addr, IoAdapter->InitialDspInfo))

	if (FileLength > ((IoAdapter->DspCodeBaseAddr - MP_CACHED_ADDR (0)) & (IoAdapter->MemorySize - 1))) {
		xdiFreeFile (File);
		DBG_FTL(("Protocol code '%s' too long (%ld)", &IoAdapter->Protocol[0], FileLength))
		return (0) ;
	}

	IoAdapter->downloadAddr = 0;
	sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
	memcpy (sharedRam, File, FileLength);
	if (memcmp (sharedRam, File, FileLength)) {
		DBG_FTL(("%s: Memory test failed!", IoAdapter->Properties.Name))
		xdiFreeFile (File);
		return (0) ;
	}
	xdiFreeFile (File);

	return (1) ;
}

static int qBri_protocol_load (PISDN_ADAPTER BaseIoAdapter, PISDN_ADAPTER IoAdapter)
{

	PISDN_ADAPTER HighIoAdapter;

	dword  FileLength ;
	dword *sharedRam, *File;
	dword  Addr, ProtOffset, SharedRamOffset, i;
	dword tasks = BaseIoAdapter->tasks ;
	int factor = (tasks == 1) ? 1 : 2;

	if (!(File = (dword *)xdiLoadArchive (IoAdapter, &FileLength, 0))) {
		return (0) ;
	}

	IoAdapter->features = diva_get_protocol_file_features ((byte*)File,
	                                       OFFS_PROTOCOL_ID_STRING,
	                                       IoAdapter->ProtocolIdString,
	                                       sizeof(IoAdapter->ProtocolIdString)) ;
	IoAdapter->a.protocol_capabilities = IoAdapter->features ;

	DBG_LOG(("Loading %s", IoAdapter->ProtocolIdString))

	ProtOffset = IoAdapter->ControllerNumber * (IoAdapter->MemorySize >> factor);
	SharedRamOffset = (IoAdapter->MemorySize >> factor) - MQ_SHARED_RAM_SIZE;
	Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))
	  | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)
	  | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16)
	  | (((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24) ;
        if ( Addr != 0 )
	{
		IoAdapter->DspCodeBaseAddr = (Addr + 3) & (~3) ;
		IoAdapter->MaxDspCodeSize = (MQ_UNCACHED_ADDR (ProtOffset + SharedRamOffset) -
										IoAdapter->DspCodeBaseAddr) & ((IoAdapter->MemorySize >> factor) - 1);

		i = 0 ;
		while ( BaseIoAdapter->QuadroList->QuadroAdapter[i]->ControllerNumber != tasks - 1 )
			i++ ;
		HighIoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i] ;
		Addr = HighIoAdapter->DspCodeBaseAddr ;

		((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT]   = (byte)tasks;
		((byte *) File)[OFFS_DIVA_INIT_TASK_COUNT+1] = (byte)BaseIoAdapter->cardType;

		((byte *) File)[OFFS_DSP_CODE_BASE_ADDR] = (byte) Addr ;
		((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 1] = (byte)(Addr >> 8) ;
		((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 2] = (byte)(Addr >> 16) ;
		((byte *) File)[OFFS_DSP_CODE_BASE_ADDR + 3] = (byte)(Addr >> 24) ;
		IoAdapter->InitialDspInfo = 0x80 ;
	}
	else
	{
		if ( IoAdapter->features & PROTCAP_VOIP )
		{
			IoAdapter->DspCodeBaseAddr = MQ_CACHED_ADDR (ProtOffset + SharedRamOffset - MQ_VOIP_MAX_DSP_CODE_SIZE) ;

			IoAdapter->MaxDspCodeSize = MQ_VOIP_MAX_DSP_CODE_SIZE ;

		}
		else if ( IoAdapter->features & PROTCAP_V90D )
		{
			IoAdapter->DspCodeBaseAddr = MQ_CACHED_ADDR (ProtOffset + SharedRamOffset - MQ_V90D_MAX_DSP_CODE_SIZE) ;

			IoAdapter->MaxDspCodeSize = (IoAdapter->ControllerNumber == tasks - 1) ? MQ_V90D_MAX_DSP_CODE_SIZE : 0 ;

		}
		else
		{
			IoAdapter->DspCodeBaseAddr = MQ_CACHED_ADDR (ProtOffset + SharedRamOffset - MQ_ORG_MAX_DSP_CODE_SIZE) ;

			IoAdapter->MaxDspCodeSize = (IoAdapter->ControllerNumber == tasks - 1) ? MQ_ORG_MAX_DSP_CODE_SIZE : 0 ;

		}
		IoAdapter->InitialDspInfo = (MQ_CACHED_ADDR (ProtOffset + SharedRamOffset -
															MQ_ORG_MAX_DSP_CODE_SIZE) - IoAdapter->DspCodeBaseAddr) >> 14 ;

	}
	DBG_LOG(("%d: DSP code base 0x%08lx, max size 0x%08lx (%08lx,%02x)",
	         IoAdapter->ControllerNumber,
	         IoAdapter->DspCodeBaseAddr, IoAdapter->MaxDspCodeSize,
	         Addr, IoAdapter->InitialDspInfo))

	if (FileLength > ((IoAdapter->DspCodeBaseAddr - MQ_CACHED_ADDR (ProtOffset)) & (IoAdapter->MemorySize - 1)) )
	{
		xdiFreeFile (File) ;
		DBG_FTL(("Protocol code '%s' too long (%ld)",
		         &IoAdapter->Protocol[0], FileLength))
		return (0) ;
	}
	IoAdapter->downloadAddr = 0 ;
	sharedRam = (dword *)DOWNLOAD_ADDR(IoAdapter) ;
	memcpy (sharedRam, File, FileLength) ;

	DBG_TRC(("Download addr 0x%08x len %ld - virtual 0x%08x",
	         IoAdapter->downloadAddr, FileLength, sharedRam))

	if ( memcmp (sharedRam, File, FileLength) )
	{
		DBG_FTL(("%s: Memory test failed!", IoAdapter->Properties.Name))

		DBG_FTL(("File=0x%x, sharedRam=0x%x", File, sharedRam))
		DBG_BLK(( (char *)File, 256))
		DBG_BLK(( (char *)sharedRam, 256))

		xdiFreeFile (File) ;
		return (0) ;
	}
	xdiFreeFile (File) ;

	return (1) ;
}

/* --------------------------------------------------------------------------
		DSP Code download
	 -------------------------------------------------------------------------- */
static long qBri_download_buffer (OsFileHandle *fp, long length, void **addr) {
	PISDN_ADAPTER BaseIoAdapter = (PISDN_ADAPTER)fp->sysLoadDesc ;
	PISDN_ADAPTER IoAdapter;
	word        i ;
	dword       *sharedRam ;

	i = 0 ;

	do
	{
		IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i++] ;
	} while ( (i < BaseIoAdapter->tasks)
	       && (((dword) length) > IoAdapter->DspCodeBaseAddr +
	                IoAdapter->MaxDspCodeSize - IoAdapter->downloadAddr) );

	*addr = (void *)(unsigned long)IoAdapter->downloadAddr ;
	if ( ((dword) length) > IoAdapter->DspCodeBaseAddr +
	                        IoAdapter->MaxDspCodeSize - IoAdapter->downloadAddr )
	{
		DBG_FTL(("%s: out of card memory during DSP download (0x%X)",
		         IoAdapter->Properties.Name,
		         IoAdapter->downloadAddr + length))
		return (-1) ;
	}
	sharedRam = (dword*)(&BaseIoAdapter->ram[IoAdapter->downloadAddr &
	                                         (IoAdapter->MemorySize - 1)]) ;


	if ( fp->sysFileRead (fp, sharedRam, length) != length )
		return (-1) ;

	IoAdapter->downloadAddr += length ;
	IoAdapter->downloadAddr  = (IoAdapter->downloadAddr + 3) & (~3) ;

	return (0) ;
}

/******************************************************************************/
static long qBri_reentrant_download_buffer (OsFileHandle *fp, long length, void **addr) {
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

static dword qBri_reentrant_telindus_load (PISDN_ADAPTER IoAdapter) {
 char                *error ;
 OsFileHandle        *fp ;
 t_dsp_portable_desc  download_table[DSP_MAX_DOWNLOAD_COUNT] ;
 word                 download_count ;
 dword               *sharedRam ;
 dword                FileLength ;

 if (IoAdapter->reentrant == 0)
	return (0);

 if ( !(fp = OsOpenFile (DSP_TELINDUS_FILE)) )
  return (0) ;

 IoAdapter->downloadAddr = (IoAdapter->DspCodeBaseAddr
                         + sizeof(dword) + sizeof(download_table) + 3) & (~3) ;
 FileLength      = fp->sysFileSize ;
 fp->sysLoadDesc = (void *)IoAdapter ;
 fp->sysCardLoad = qBri_reentrant_download_buffer ;
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

static dword qBri_telindus_load (PISDN_ADAPTER BaseIoAdapter) {
	PISDN_ADAPTER        IoAdapter = 0;
	PISDN_ADAPTER        HighIoAdapter = NULL ;
	char                *error ;
	OsFileHandle        *fp ;
	t_dsp_portable_desc  download_table[DSP_MAX_DOWNLOAD_COUNT] ;
	word                 download_count, i ;
	dword               *sharedRam ;
	dword                FileLength ;

	if ( !(fp = OsOpenFile (DSP_TELINDUS_FILE)) ) {
		DBG_FTL(("qBri_telindus_load: %s not found!", DSP_TELINDUS_FILE))
		return (0) ;
	}


	for ( i = 0 ; i < BaseIoAdapter->tasks ; ++i )
	{
		IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i] ;
		IoAdapter->downloadAddr = IoAdapter->DspCodeBaseAddr ;
		if ( IoAdapter->ControllerNumber == BaseIoAdapter->tasks - 1 )
		{
			HighIoAdapter = IoAdapter ;
			HighIoAdapter->downloadAddr = (HighIoAdapter->downloadAddr
			           + sizeof(dword) + sizeof(download_table) + 3) & (~3) ;
		}
	}


	FileLength      = fp->sysFileSize ;
	fp->sysLoadDesc = (void *)BaseIoAdapter ;
	fp->sysCardLoad = qBri_download_buffer ;

	download_count = DSP_MAX_DOWNLOAD_COUNT ;
	memset (&download_table[0], '\0', sizeof(download_table)) ;
/*
 *	set start address for download
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
	 *	store # of download files extracted from the archive and download table
	 */
		HighIoAdapter->downloadAddr = HighIoAdapter->DspCodeBaseAddr ;
		sharedRam = (dword *)(&BaseIoAdapter->ram[HighIoAdapter->downloadAddr &
		                                          (IoAdapter->MemorySize - 1)]) ;
		sharedRam[0] = (dword)download_count ;
		memcpy (&sharedRam[1], &download_table[0], sizeof(download_table)) ;


	/* memory check */
	if ( memcmp (&sharedRam[1], &download_table, download_count) ) {
		DBG_FTL(("%s: Dsp Memory test failed!", IoAdapter->Properties.Name))
	}

	return (FileLength) ;
}

/*
	Load SDP tasks to the card
	Return start address of image on succesful load
	Return zero in case of problem

	INPUT:
		task			->	name of the image containing this task
		link_addr	->	pointer to start of previous task
	*/
static byte* qBri_sdp_load (PISDN_ADAPTER BaseIoAdapter,
													char* task,
													byte*	link_addr) {
	OsFileHandle *fp;
	dword FileLength;
	byte tmp[sizeof(dword)];
	dword gp_addr;
	dword entry_addr;
	dword start_addr = 0;
	dword phys_start_addr;
	dword end_addr;
	byte* sharedRam = 0;

  if (task) {
		if (!(fp = OsOpenFile (task))) {
			DBG_ERR(("Can't open [%s] image", task))
			return (0);
		}
		if ((FileLength = fp->sysFileSize) < DIVA_MIPS_TASK_IMAGE_ID_STRING_OFFS) {
			OsCloseFile (fp) ;
			DBG_ERR(("Image [%s] too short", task))
			return (0);
		}

		fp->sysFileSeek (fp, DIVA_MIPS_TASK_IMAGE_GP_OFFS, OS_SEEK_SET);
		if (fp->sysFileRead (fp, tmp, sizeof(dword)) != sizeof(dword)) {
			OsCloseFile (fp) ;
			DBG_ERR(("Can't read image [%s]", task))
			return (0);
		}
		gp_addr = ((dword)tmp[0])					|
							(((dword)tmp[1]) << 8)	|
							(((dword)tmp[2]) << 16) |
							(((dword)tmp[3]) << 24);
		DBG_TRC(("Image [%s] GP = %08lx", task, gp_addr))

		fp->sysFileSeek (fp, DIVA_MIPS_TASK_IMAGE_ENTRY_OFFS, OS_SEEK_SET);
		if (fp->sysFileRead (fp, tmp, sizeof(dword)) != sizeof(dword)) {
			OsCloseFile (fp) ;
			DBG_ERR(("Can't read image [%s]", task))
			return (0);
		}
		entry_addr = ((dword)tmp[0])					|
									(((dword)tmp[1]) << 8)	|
									(((dword)tmp[2]) << 16) |
									(((dword)tmp[3]) << 24);
		DBG_TRC(("Image [%s] entry = %08lx", task, entry_addr))

		fp->sysFileSeek (fp, DIVA_MIPS_TASK_IMAGE_LOAD_ADDR_OFFS, OS_SEEK_SET);
		if (fp->sysFileRead (fp, tmp, sizeof(dword)) != sizeof(dword)) {
			OsCloseFile (fp) ;
			DBG_ERR(("Can't read image [%s]", task))
			return (0);
		}
		start_addr = ((dword)tmp[0])					|
									(((dword)tmp[1]) << 8)	|
									(((dword)tmp[2]) << 16) |
									(((dword)tmp[3]) << 24);
		DBG_TRC(("Image [%s] start = %08lx", task, start_addr))

		fp->sysFileSeek (fp, DIVA_MIPS_TASK_IMAGE_END_ADDR_OFFS, OS_SEEK_SET);
		if (fp->sysFileRead (fp, tmp, sizeof(dword)) != sizeof(dword)) {
			OsCloseFile (fp) ;
			DBG_ERR(("Can't read image [%s]", task))
			return (0);
		}
		end_addr = ((dword)tmp[0])					|
								(((dword)tmp[1]) << 8)	|
								(((dword)tmp[2]) << 16) |
								(((dword)tmp[3]) << 24);
		DBG_TRC(("Image [%s] end = %08lx", task, end_addr))

		phys_start_addr = start_addr & 0x1fffffff;

		if ((phys_start_addr + FileLength) >= BaseIoAdapter->MemorySize) {
			OsCloseFile (fp) ;
			DBG_ERR(("Image [%s] too long", task))
			return (0);
		}

		fp->sysFileSeek (fp, 0, OS_SEEK_SET);
		sharedRam = &BaseIoAdapter->ram[phys_start_addr];
		if ((dword)fp->sysFileRead (fp, sharedRam, FileLength) != FileLength) {
			OsCloseFile (fp) ;
			DBG_ERR(("Can't read image [%s]", task))
			return (0);
		}

		OsCloseFile (fp) ;
  }

	if (!link_addr) {
		link_addr = &BaseIoAdapter->ram[OFFS_DSP_CODE_BASE_ADDR];
	}

	DBG_TRC(("Write task [%s] link %08lx at %08lx",
						task ? task : "none",
						start_addr,
						link_addr - (byte*)&BaseIoAdapter->ram[0]))

	link_addr[0] = (byte)(start_addr         & 0xff);
	link_addr[1] = (byte)((start_addr >>  8) & 0xff);
	link_addr[2] = (byte)((start_addr >> 16) & 0xff);
	link_addr[3] = (byte)((start_addr >> 24) & 0xff);

	return (task ? &sharedRam[DIVA_MIPS_TASK_IMAGE_LINK_OFFS] : 0);
}

/* --------------------------------------------------------------------------
		Load Card
	 -------------------------------------------------------------------------- */
static int load_qBri_hardware (PISDN_ADAPTER IoAdapter) {
	dword         i, offset, controller ;
	word         *signature ;
	int           factor = (IoAdapter->tasks == 1) ? 1 : 2;

	PISDN_ADAPTER Slave ;


	if (

		!IoAdapter->QuadroList

	  || ( (IoAdapter->cardType != CARDTYPE_DIVASRV_Q_8M_PCI)
	    && (IoAdapter->cardType != CARDTYPE_DIVASRV_VOICE_Q_8M_PCI)
	    && (IoAdapter->cardType != CARDTYPE_DIVASRV_Q_8M_V2_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_B_2M_V2_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_B_2F_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_BRI_CTI_V2_PCI)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_4BRI_V1_PCIE)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_4BRI_V1_V_PCIE)

     && (IoAdapter->cardType != CARDTYPE_DIVASRV_BRI_V1_PCIE)
     && (IoAdapter->cardType != CARDTYPE_DIVASRV_BRI_V1_V_PCIE)

) )
	{
		return (0) ;
	}

/*
 *	Check for first instance
 */
	if ( IoAdapter->ControllerNumber > 0 )
		return (1) ;

/*
 *	first initialize the onboard FPGA
 */
	if ( !qBri_FPGA_download (IoAdapter) )
		return (0) ;


	for ( i = 0; i < IoAdapter->tasks; i++ )
	{
		Slave = IoAdapter->QuadroList->QuadroAdapter[i] ;
		Slave->fpga_features = IoAdapter->fpga_features ;
	}


/*
 *	download protocol code for all instances
 */

	controller = IoAdapter->tasks;
	do
	{
		controller-- ;
		i = 0 ;
		while ( IoAdapter->QuadroList->QuadroAdapter[i]->ControllerNumber != controller )
			i++ ;
/*
 *	calculate base address for instance
 */
		Slave          = IoAdapter->QuadroList->QuadroAdapter[i] ;
		offset         = Slave->ControllerNumber * (IoAdapter->MemorySize >> factor) ;
		Slave->Address = &IoAdapter->Address[offset] ;
		Slave->ram     = &IoAdapter->ram[offset] ;

		// set addresses (copy it from controller 0)
		Slave->reset   = IoAdapter->reset ;
		Slave->ctlReg  = IoAdapter->ctlReg ;
		Slave->prom    = IoAdapter->prom ;

		if ( !qBri_protocol_load (IoAdapter, Slave) )
			return (0) ;

	} while (controller != 0) ;


/*
 *	download only one copy of the DSP code
 */
 if (!(IoAdapter->cardType == CARDTYPE_DIVASRV_B_2F_PCI        ||
       IoAdapter->cardType == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI)   ) {
	if ( !qBri_telindus_load (IoAdapter) )
		return (0) ;
 } else {
   byte* link_addr = 0;
   link_addr = qBri_sdp_load (IoAdapter, DIVA_BRI2F_SDP_1_NAME, link_addr);
   link_addr = qBri_sdp_load (IoAdapter, DIVA_BRI2F_SDP_2_NAME, link_addr);
   if (!link_addr) {
     qBri_sdp_load (IoAdapter, 0, link_addr);
   }
 }

/*
 *	copy configuration parameters
 */

	for ( i = 0 ; i < IoAdapter->tasks ; ++i )
	{
		Slave = IoAdapter->QuadroList->QuadroAdapter[i] ;
		if (Slave != IoAdapter)
			memcpy (Slave->ProtocolIdString, IoAdapter->ProtocolIdString, sizeof(IoAdapter->ProtocolIdString));
		Slave->ram += (IoAdapter->MemorySize >> factor) - MQ_SHARED_RAM_SIZE ;
		DBG_TRC(("Configure instance %d shared memory @ 0x%08lx",
		         Slave->ControllerNumber, Slave->ram))
		memset (Slave->ram, '\0', 256) ;
		diva_configure_protocol (Slave, MQ_SHARED_RAM_SIZE);
	}

/*
 *	start adapter
 */
	start_qBri_hardware (IoAdapter) ;
	signature = (word *)(&IoAdapter->ram[0x1E]) ;
/*
 *	wait for signature in shared memory (max. 3 seconds)
 */
	for ( i = 0 ; i < 300 ; ++i )
	{
		diva_os_wait (10) ;

		if ( signature[0] == 0x4447 )
		{
			DBG_TRC(("Protocol startup time %d.%02d seconds",
			         (i / 100), (i % 100) ))

			return (1) ;
		}
	}
	DBG_FTL(("%s: Adapter selftest failed (0x%04X)!",
	         IoAdapter->Properties.Name, signature[0] >> 16))
	qBri_cpu_trapped (IoAdapter) ;
	return (FALSE) ;
}
#else /* } { */
static int load_qBri_hardware (PISDN_ADAPTER IoAdapter) {
	return (0);
}
#endif /* } */

/* --------------------------------------------------------------------------
		Card ISR
	 -------------------------------------------------------------------------- */
static int qBri_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	dword volatile     *qBriIrq ;

	PADAPTER_LIST_ENTRY QuadroList = IoAdapter->QuadroList ;

	word              	i ;


	if ( !(IoAdapter->reset[PLX9054_INTCSR] & 0x80) )
		return (0) ;

/*
 *	clear interrupt line (reset Local Interrupt Test Register)
 */
	qBriIrq = (dword volatile *)(&IoAdapter->ctlReg[DIVA_4BRI_REVISION(IoAdapter) ? (MQ2_BREG_IRQ_TEST)  : (MQ_BREG_IRQ_TEST)]);
	*qBriIrq = MQ_IRQ_REQ_OFF ;

	for ( i = 0 ; i < IoAdapter->tasks; ++i )
	{
		IoAdapter = QuadroList->QuadroAdapter[i] ;

		if ( IoAdapter && IoAdapter->Initialized
		  && IoAdapter->tst_irq (&IoAdapter->a) )
		{
			IoAdapter->IrqCount++ ;
			diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
		}
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
static int qBri_msi_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList = IoAdapter->QuadroList ;
	word              	i ;

	/*
		clear interrupt line (reset Local Interrupt Test Register)
		*/
	*(dword volatile *)&IoAdapter->ctlReg[MQ2_BREG_IRQ_TEST] = MQ_IRQ_REQ_OFF;

	/*
		Acknowledge MSI interrupt
		*/
	*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;

  for ( i = 0 ; i < IoAdapter->tasks; ++i )
  {
    IoAdapter = QuadroList->QuadroAdapter[i] ;

    if (IoAdapter                          != 0 &&
				IoAdapter->Initialized             != 0 &&
				IoAdapter->tst_irq (&IoAdapter->a) != 0) {
      IoAdapter->IrqCount++ ;
      diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
    }
  }

	return (1);
}

/* --------------------------------------------------------------------------
		Does disable the interrupt on the card
	 -------------------------------------------------------------------------- */
static void disable_qBri_interrupt (PISDN_ADAPTER IoAdapter) {
	dword volatile *qBriIrq ;

	if ( IoAdapter->ControllerNumber > 0 )
		return ;

	/*
		This is bug in the PCIe device. In case bit 8 of INTCSR is cleared
		in the time interrupt is active then device will generate unexpected
		interrupt or hang if in MSI mode. For this reason clear bit 11 of INTCSR
		first, acknowledge and clear the cause of interrupt and finally clear
		bit 8 of INTCSR.
		This behavior is compatible with existing devices and PCIe device does not
		supports I/O
		*/
	qBriIrq = (dword volatile *)(&IoAdapter->ctlReg[DIVA_4BRI_REVISION(IoAdapter) ? (MQ2_BREG_IRQ_TEST)  : (MQ_BREG_IRQ_TEST)]);

	*((volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW]) &= ~(1U << 11);
	*qBriIrq   = MQ_IRQ_REQ_OFF ;

	if (IoAdapter->msi != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
	}

	*((volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW]) &= ~((1U << 8) | (1U << 11));
}

/* --------------------------------------------------------------------------
		Install Adapter Entry Points
	 -------------------------------------------------------------------------- */
static void set_common_qBri_functions (PISDN_ADAPTER IoAdapter) {
	ADAPTER *a;

	a = &IoAdapter->a ;

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

	IoAdapter->load     = load_qBri_hardware ;

	IoAdapter->disIrq   = disable_qBri_interrupt ;
	IoAdapter->rstFnc   = reset_qBri_hardware ;
	IoAdapter->stop     = stop_qBri_hardware ;
	IoAdapter->trapFnc  = qBri_cpu_trapped ;

	IoAdapter->diva_isr_handler = (IoAdapter->msi == 0) ? qBri_ISR : qBri_msi_ISR;

	IoAdapter->a.io       = (void*)IoAdapter ;

#if defined(DIVA_ADAPTER_TEST_SUPPORTED)
  IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
#endif
}

static void set_qBri_functions (PISDN_ADAPTER IoAdapter) {
	if (!IoAdapter->tasks) {
		IoAdapter->tasks = MQ_INSTANCE_COUNT;
	}
	IoAdapter->MemorySize = MQ_MEMORY_SIZE ;
	set_common_qBri_functions (IoAdapter) ;
	diva_os_set_qBri_functions (IoAdapter) ;
}

static void set_qBri2_functions (PISDN_ADAPTER IoAdapter) {
	if (!IoAdapter->tasks) {
		IoAdapter->tasks = MQ_INSTANCE_COUNT;
	}
	IoAdapter->MemorySize = (IoAdapter->tasks == 1) ? BRI2_MEMORY_SIZE : MQ2_MEMORY_SIZE;
	set_common_qBri_functions (IoAdapter) ;
	diva_os_set_qBri2_functions (IoAdapter) ;
}

/******************************************************************************/

void prepare_qBri_functions (PISDN_ADAPTER IoAdapter) {

	set_qBri_functions (IoAdapter->QuadroList->QuadroAdapter[0]) ;
	set_qBri_functions (IoAdapter->QuadroList->QuadroAdapter[1]) ;
	set_qBri_functions (IoAdapter->QuadroList->QuadroAdapter[2]) ;
	set_qBri_functions (IoAdapter->QuadroList->QuadroAdapter[3]) ;

}

void prepare_qBri2_functions (PISDN_ADAPTER IoAdapter) {
	if (!IoAdapter->tasks) {
		IoAdapter->tasks = MQ_INSTANCE_COUNT;
	}

	set_qBri2_functions (IoAdapter->QuadroList->QuadroAdapter[0]) ;
	if (IoAdapter->tasks > 1) {
		set_qBri2_functions (IoAdapter->QuadroList->QuadroAdapter[1]) ;
		set_qBri2_functions (IoAdapter->QuadroList->QuadroAdapter[2]) ;
		set_qBri2_functions (IoAdapter->QuadroList->QuadroAdapter[3]) ;
	}

}

/* -------------------------------------------------------------------------- */

// vim: set tabstop=2 softtabstop=2 shiftwidth=2 expandtab :
