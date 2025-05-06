
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
#include "divasync.h"
#include "vidi_di.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "io.h"
#include "helpers.h"
#include "dsrv_pri.h"
#include "dsrv_4pri.h"
#include "dsp_defs.h"
#include "dsrv4bri.h" /* PLX related definitions */
#include "divatest.h" /* Adapter test framework */
#include "fpga_rom.h"
#include "fpga_rom_xdi.h"

#define DIVA_MEMORY_TEST_PATTERN "Hallo World!\0"
#define MAX_XLOG_SIZE  (64 * 1024)

extern int diva_xdi_disable_plx_reset;

static dword diva_4pri_detect_dsps (PISDN_ADAPTER IoAdapter);
static int load_4pri_hardware (PISDN_ADAPTER IoAdapter);

/*
  Start the CPU
  */
int start_4pri_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword* cpu_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_RESET_REG_OFFSET];
	dword cpu_reset = *cpu_reset_ctrl;

	/*
		Release cpu cold reset
		*/
	*cpu_reset_ctrl = cpu_reset & ~0x08000000;
	diva_os_wait(100);

	/*
		Release cpu warm reset
		*/
	*cpu_reset_ctrl = cpu_reset & ~0x18000000;
	diva_os_wait(100);

	return (0);
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

int pri_4pri_FPGA_download (PISDN_ADAPTER IoAdapter) {
	int            bit ;
	byte           *File ;
	dword          code = 0, FileLength;
	volatile word *addr = (volatile word*)IoAdapter->prom ;
	word           val, baseval = FPGA_CS | FPGA_PROG ;
	volatile dword* led_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_LED_CTRL_REG_OFFSET];
	volatile dword* cpu_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_RESET_REG_OFFSET];

	if (!(File = (byte *)xdiLoadFile("ds4pri_10.bit", &FileLength, 0))) {
		return (0);
	}

	/*
	 *	prepare download, pulse PROGRAM pin down.
	 */
	*addr = (baseval & ~FPGA_PROG); /* PROGRAM low pulse */
	diva_os_wait (5);
	*addr = baseval; /* release */
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
		if (code % 2048 == 0) {
			diva_os_sleep (0xffffffff);
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
				PCIwrite (Bus, Slot, 0x54 /* PEDEVCR */, &pedevcr, sizeof(pedevcr), hdev);
			}
		}

		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] &= ~(0xffffU | (1<<16) | (1<<17) | (1<<28));
		*(volatile dword*)&IoAdapter->reset[PLX9054_MABR0] |= (1U << 18) | (1U << 21);

		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] |= ((1U << 6) | (1 << 24));
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] &= ~(1U << 8); /* If 24 set clear 8 to enable bursts */
		*(volatile dword*)&IoAdapter->reset[PLX9054_LBRD0] &= ~((1U << 10) | (0xfU << 11));
	} else {
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
	}

	*(dword*)&IoAdapter->reset[0x80] |= (1U << 6) /* Enable j_ready_n for DMA */ | (1U << 23);
	*(dword*)&IoAdapter->reset[0x94] |= (1U << 6) /* Enable j_ready_n for DMA */ | (1U << 23);

	diva_xdi_show_fpga_rom_features (&IoAdapter->ctlReg[DIVA_4PRI_FPGA_INFO_REG_OFFSET]);

	*cpu_reset_ctrl = 0xf8000000;
	*led_ctrl = 0x00000000;

	bit = 8;
	while (bit--) {
		*led_ctrl = 0x00002121;
		diva_os_sleep(50);
		*led_ctrl = 0x00008484;
		diva_os_sleep(50);
	}

	*led_ctrl = 0x00000000;

 	if (IoAdapter->DivaAdapterTestProc) {
		if ((*(IoAdapter->DivaAdapterTestProc))(IoAdapter)) {
			return (0);
		}
	}

	return (1) ;
}

static void reset_4pri_hardware (PISDN_ADAPTER IoAdapter) {
	if (diva_xdi_disable_plx_reset == 0 &&
			(IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
		volatile word  *Reset = (word volatile *)IoAdapter->prom ;

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
}

/*
  Offset between the start of physical memory and
  begin of the shared memory
  */
static dword diva_4pri_ram_offset (ADAPTER* a) {
	PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
	return (IoAdapter->shared_ram_offset);
}

static void diva_4pri_cpu_trapped (PISDN_ADAPTER IoAdapter) {
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
 if ( regs[0] != 0 && (regs[0] < IoAdapter->MemorySize - 1) )
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

static void stop_4pri_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword* cpu_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_RESET_REG_OFFSET];
	volatile dword* dsp_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_DSP_RESET_REG_OFFSET];
	volatile dword* sys_status_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_SYS_STATUS_REG_OFFSET];
	volatile dword* fpga_bus_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_HW_DEBUG_FPGA_BUS_STATE_REG_OFFSET];
	dword i = 0;


	/*
		Disable clock interrupt in any case, interrupt is not valid any more in case adapter
		is not running
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_INPUT_ENABLE;

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

	i = *sys_status_ctrl;
	if ((i & (1 << 24)) != 0) {
		DBG_ERR(("Hardware debug register: address:%08x command:%08x",
							*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_HW_DEBUG_ADDRESS_REG_OFFSET],
							*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_HW_DEBUG_COMMAND_REG_OFFSET]))
	}

	if ((i = *fpga_bus_ctrl) != 0) {
		DBG_ERR(("FPGA bus state: %08x", i))
	}

	*cpu_reset_ctrl = 0xf8000000;
	*dsp_reset_ctrl = 0x000fffff;
}

static int diva_4pri_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList;
	dword mask;
	int i, serviced = 0;

#ifdef DIVA_CLOCK_ISR_TEST
	if (IoAdapter->clock_isr_active != 0 &&
			((mask = *(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER]) &
                                                  DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_ON) != 0) {
		IoAdapter->clock_time_stamp_count++;
		IoAdapter->clock_isr_count = (IoAdapter->clock_isr_count + 1) & 0x00ffffffU;
		mask &= DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_COUNTER;
		if (mask != 0) {
			*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER] = 0;
			mask = mask >> 24;
			IoAdapter->clock_isr_errors += mask;
			IoAdapter->clock_isr_count   = (IoAdapter->clock_isr_count + mask) & 0x00ffffffU;
		}
		(*(IoAdapter->clock_isr_callback))(IoAdapter->clock_isr_callback_context, IoAdapter->clock_isr_count);
		*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = (1U << 31);
		serviced = 1;
	}
#else
	if (IoAdapter->clock_isr_active != 0) {
		if ((IoAdapter->reset[PLX9054_INTCSR] & 0x80) != 0) {
			IoAdapter->clock_time_stamp_count++;
			IoAdapter->clock_isr_count = (IoAdapter->clock_isr_count + 1) & 0x00ffffffU;
			(*(IoAdapter->clock_isr_callback))(IoAdapter->clock_isr_callback_context, IoAdapter->clock_isr_count);
			*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = (1U << 31);
			serviced = 1;
		}
	}
#endif

	if (!(*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] & PLX9054_L2PDBELL_INT_PENDING)) {
		return (serviced);
	}

	mask = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = mask;

	QuadroList = IoAdapter->QuadroList;

	for (i = 0; i < IoAdapter->tasks; i++) {
		if (mask & (((dword)1) << i)) {
			IoAdapter = QuadroList->QuadroAdapter[i] ;
			if (IoAdapter && IoAdapter->Initialized && IoAdapter->tst_irq(&IoAdapter->a)) {
				IoAdapter->IrqCount++ ;
				diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
			}
		}
	}

	return (1);
}

/*
  MSI interrupt is not shared and must always return true
  The MSI acknowledge supposes that only one message was allocated for
  MSI. This allows to process interrupt without read of interrupt status
  register
  */
static int diva_4pri_msi_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList;
	dword mask;
	int i;

#ifdef DIVA_CLOCK_ISR_TEST
	if (IoAdapter->clock_isr_active != 0 &&
			((mask = *(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER]) &
                                                  DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_ON) != 0) {
		IoAdapter->clock_isr_count = (IoAdapter->clock_isr_count + 1) & 0x00ffffffU;
		mask &= DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_COUNTER;
		if (mask != 0) {
			*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER] = 0;
			mask = mask >> 24;
			IoAdapter->clock_isr_errors += mask;
			IoAdapter->clock_isr_count   = (IoAdapter->clock_isr_count + mask) & 0x00ffffffU;
		}
		(*(IoAdapter->clock_isr_callback))(IoAdapter->clock_isr_callback_context, IoAdapter->clock_isr_count);
		*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = 1U;
	}
#else
	if (IoAdapter->clock_isr_active != 0) {
		if ((IoAdapter->reset[PLX9054_INTCSR] & 0x80) != 0) {
			IoAdapter->clock_time_stamp_count++;
			IoAdapter->clock_isr_count = (IoAdapter->clock_isr_count + 1) & 0x00ffffffU;
			(*(IoAdapter->clock_isr_callback))(IoAdapter->clock_isr_callback_context, IoAdapter->clock_isr_count);
			*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = 1U;
		}
	}
#endif

	if (!(*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] & PLX9054_L2PDBELL_INT_PENDING)) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
		return (1);
	}

	mask = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = mask;
	*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;

	QuadroList = IoAdapter->QuadroList;

	for (i = 0; i < IoAdapter->tasks; i++) {
		if (mask & (((dword)1) << i)) {
			IoAdapter = QuadroList->QuadroAdapter[i] ;
			if (IoAdapter && IoAdapter->Initialized && IoAdapter->tst_irq(&IoAdapter->a)) {
				IoAdapter->IrqCount++ ;
				diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
			}
		}
	}

	return (1);
}

static void disable_4pri_interrupt (PISDN_ADAPTER IoAdapter) {
	/*
		Disable clock interrupt in any case, interrupt is not valid any more in case adapter
		is not running
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_INPUT_ENABLE;
	if (IoAdapter->clock_isr_active != 0) {
		*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_SELECTION_REGISTER_OFFSET] = 0;
		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_INPUT_ENABLE;
		*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_SELECTION_REGISTER_OFFSET] = 0;
		do {
			*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = 1U;
		} while ((*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER] &
                                                  DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_ON) != 0);
		IoAdapter->clock_isr_active = 0;
	}

  /*
    This is bug in the PCIe device. In case bit 8 of INTCSR is cleared
    in the time interrupt is active then device will generate unexpected
    interrupt or hang if in MSI mode. For this reason clear bit 11 and bit 9 of INTCSR
    first, acknowledge and clear the cause of interrupt and finally clear
    bit 8 of INTCSR.
    This behavior is compatible with existing devices and PCIe device does not
    supports I/O
    */
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~(PLX9054_L2PDBELL_INT_ENABLE & ~(1U << 8));
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];

	if (IoAdapter->msi != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_PCIE_PLX_MSIACK] = 0x00000001U;
	}

	*((volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW]) &= ~PLX9054_L2PDBELL_INT_ENABLE;
}

int diva_4pri_fpga_ready (PISDN_ADAPTER IoAdapter) {
	int ret = 0;

	if (IoAdapter->Initialized == 0) {
		if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
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
		} else {
			ret = -1;
		}
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
static int diva_4pri_save_maint_buffer (PISDN_ADAPTER IoAdapter, const void* data, dword length) {
	if (IoAdapter->ram != 0 && IoAdapter->MemorySize > MP_SHARED_RAM_OFFSET &&
			(IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET) > length) {
		if (IoAdapter->Initialized) {
			volatile dword* cpu_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_RESET_REG_OFFSET];
			volatile dword* dsp_reset_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_DSP_RESET_REG_OFFSET];
			volatile dword* sys_status_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_SYS_STATUS_REG_OFFSET];
			volatile dword* fpga_bus_ctrl = (dword*)&IoAdapter->ctlReg[DIVA_4PRI_HW_DEBUG_FPGA_BUS_STATE_REG_OFFSET];

			IoAdapter->Initialized = 0;
			disable_4pri_interrupt (IoAdapter);


			if ((*sys_status_ctrl & (1 << 24)) != 0 || *fpga_bus_ctrl != 0)
				return (-1);

			*cpu_reset_ctrl = 0xf8000000;
			*dsp_reset_ctrl = 0x000fffff;
		}
		if (diva_4pri_fpga_ready (IoAdapter) == 0) {
			memcpy (IoAdapter->ram, data, length);
			return (0);
		}
	}

	return (-1);
}

/*
  Control source of rate interrupt
  */
static int diva_4pri_clock_control (PISDN_ADAPTER IoAdapter, const diva_xdi_clock_control_command_t* cmd) {
	int ret = -1;

	if (IoAdapter->Initialized == 0 ||
			(diva_xdi_decode_fpga_rom_features (&IoAdapter->ctlReg[DIVA_4PRI_FPGA_INFO_REG_OFFSET]) &
			 DIVA_FPGA_HARDWARE_FEATURE_HW_TIMER) == 0) {
		DBG_ERR(("HW timer not supported"))
		return (-1);
	}

	switch (cmd->command) {
		case 1:
			if (IoAdapter->clock_isr_active == 0 &&
					cmd->data.on.isr_callback != 0 && cmd->data.on.port < IoAdapter->tasks) {
				IoAdapter->clock_isr_callback         = cmd->data.on.isr_callback;
				IoAdapter->clock_isr_callback_context = cmd->data.on.isr_callback_context;
				IoAdapter->clock_isr_count            = 0;
				IoAdapter->clock_isr_errors           = 0;
				IoAdapter->clock_time_stamp_count     = 0;
				IoAdapter->clock_isr_active           = 1;
        *(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_SELECTION_REGISTER_OFFSET] =
																																			(1U << cmd->data.on.port);
				*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |= PLX9054_LOCAL_INT_INPUT_ENABLE;
				DBG_LOG(("A(%d) port %d clock on", IoAdapter->ANum, cmd->data.on.port + 1))
				ret = 0;
			}
			break;

		case 0:
			if (IoAdapter->clock_isr_active != 0) {
				*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_INPUT_ENABLE;
        *(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_SELECTION_REGISTER_OFFSET] = 0;
        do {
          *(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_ACKNOWLEDGE_REGISTER] = 1U;
        } while ((*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER] &
                                                  DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_ON) != 0);
				IoAdapter->clock_isr_active = 0;
				DBG_LOG(("clock interrupts:%d errors:%d", IoAdapter->clock_isr_count, IoAdapter->clock_isr_errors))
				IoAdapter->clock_isr_count  = 0;
				IoAdapter->clock_isr_errors = 0;
				DBG_LOG(("A(%d) clock off", IoAdapter->ANum))
				ret = 0;
			}
			break;

		case 2: {/* Time stamp register */
			dword volatile** reg_addr = (dword volatile **)cmd->data.on.isr_callback_context;
			DBG_LOG(("A(%d) registered timestamp"))
			*reg_addr = &IoAdapter->clock_time_stamp_count;
		} break;
  }

	return (ret);
}

/* -------------------------------------------------------------------------
  Install entry points for 4PRI Adapter
  ------------------------------------------------------------------------- */
void prepare_4pri_functions (PISDN_ADAPTER BaseIoAdapter) {
	int i;

  for (i = 0; i < BaseIoAdapter->tasks; i++) {
		PISDN_ADAPTER IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i];
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
		a->ram_offset       = diva_4pri_ram_offset ;
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
		IoAdapter->stop     = stop_4pri_hardware ;

		IoAdapter->load     = load_4pri_hardware ;
		IoAdapter->disIrq   = disable_4pri_interrupt ;
		IoAdapter->rstFnc   = reset_4pri_hardware;
		IoAdapter->trapFnc  = diva_4pri_cpu_trapped ;

		IoAdapter->diva_isr_handler       = IoAdapter->msi == 0 ? diva_4pri_ISR : diva_4pri_msi_ISR;
		if (i == 0) {
			IoAdapter->save_maint_buffer_proc = diva_4pri_save_maint_buffer;
			IoAdapter->clock_control_Fnc      = diva_4pri_clock_control;
    }

		IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
		IoAdapter->DetectDsps = diva_4pri_detect_dsps;

		IoAdapter->shared_ram_offset = i * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE) + MP_SHARED_RAM_OFFSET;

		diva_os_prepare_4pri_functions (IoAdapter);
	}
}

static dword diva_4pri_detect_dsps (PISDN_ADAPTER IoAdapter) {
	return (0);
}

#if defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
void diva_4pri_memcpy (PISDN_ADAPTER IoAdapter, byte* mem, dword address, const byte* src, dword length) {
  volatile dword* las0ba_ctrl = (volatile dword*)&IoAdapter->reset[0x04];
  dword las0ba_original = *las0ba_ctrl;
  dword segment_length = IoAdapter->MemorySize;
  dword segment        = address/segment_length;

  while (length != 0) {
    dword to_copy = MIN(length, segment_length - address % segment_length);

    if (segment != 0) {
      *las0ba_ctrl = (dword)(las0ba_original + segment * segment_length);
    }

    memcpy (&mem[address % segment_length], src, to_copy);

    length -= to_copy;
    if (length != 0) {
      src     += to_copy;
      address += to_copy;
      segment++;
    }
  }

  if (segment != 0) {
    *las0ba_ctrl = las0ba_original;
  }
}
#else
void diva_4pri_memcpy (PISDN_ADAPTER IoAdapter, byte* mem, dword address, const byte* src, dword length) {
	DBG_TRC(("copy from:%p to:(%p-%p) at:%08x length:%d",
						src, mem, mem + DIVA_4PRI_REAL_SDRAM_SIZE, address, length))
	memcpy (&mem[address], src, length);
}
#endif

#if !defined(DIVA_USER_MODE_CARD_CONFIG) /* { */
/* -------------------------------------------------------------------------
  Load protocol code to the PRI Card
  ------------------------------------------------------------------------- */
#define DOWNLOAD_ADDR(IoAdapter) \
 (&IoAdapter->ram[IoAdapter->downloadAddr & (IoAdapter->MemorySize - 1)])
static int qpri_protocol_load (PISDN_ADAPTER IoAdapter) {
	dword  FileLength ;
	dword *File ;
	dword *sharedRam ;
	dword  Addr ;

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
	IoAdapter->InitialDspInfo &= ~0xffff ;

	if ((Addr = ((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR]))     |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 1])) << 8)  |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 2])) << 16) |
			(((dword)(((byte *) File)[OFFS_PROTOCOL_END_ADDR + 3])) << 24))) {
		DBG_TRC(("Protocol end address (%08x)", Addr))

		IoAdapter->DspCodeBaseAddr = (Addr + 15) & (~15); /* Align at paragraph boundary */

    IoAdapter->MaxDspCodeSize =
      (MP_UNCACHED_ADDR (DIVA_4PRI_REAL_SDRAM_SIZE) - IoAdapter->DspCodeBaseAddr) &
                                                         (DIVA_4PRI_REAL_SDRAM_SIZE - 1);
		Addr = IoAdapter->DspCodeBaseAddr;
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR]     = (byte) Addr;
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 1] = (byte)(Addr >> 8);
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 2] = (byte)(Addr >> 16);
		((byte*)File)[OFFS_DSP_CODE_BASE_ADDR + 3] = (byte)(Addr >> 24);
		IoAdapter->InitialDspInfo |= 0x80 ;
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

static int qpri_dsp_load (PISDN_ADAPTER IoAdapter) {
	byte           *File;
	dword          FileLength = 0;

	if (IoAdapter->ControllerNumber != 0) {
		return (1);
	}

	if (!(File = (byte*)xdiLoadFile(&IoAdapter->AddDownload[0], &FileLength, 0))) {
		DBG_ERR(("Failed to open '%s' DSP image file", IoAdapter->AddDownload[0]))
		return (0);
	}
	if (FileLength >= IoAdapter->MaxDspCodeSize) {
		DBG_ERR(("DSP Image length %08x, max: %08x", FileLength, IoAdapter->MaxDspCodeSize))
		xdiFreeFile (File);
		return (0) ;
	}

	IoAdapter->downloadAddr = IoAdapter->DspCodeBaseAddr;
	DBG_REG(("Start DSP download at %08x, length=%08x", IoAdapter->downloadAddr, FileLength))
#if 0
	dword *sharedRam ;
	sharedRam = (dword*)DOWNLOAD_ADDR(IoAdapter) ;
	memcpy (sharedRam, File, FileLength);
	if (memcmp (sharedRam, File, FileLength)) {
		DBG_FTL(("%s: Memory test failed!", IoAdapter->Properties.Name))
		xdiFreeFile (File);
		return (0) ;
	}
#else
	diva_4pri_memcpy (IoAdapter,
										IoAdapter->ram,
										IoAdapter->downloadAddr & (DIVA_4PRI_REAL_SDRAM_SIZE-1),
										File,
										FileLength);
#endif
	IoAdapter->DspImageLength = FileLength;

	xdiFreeFile (File);

	return (1);
}

static int load_4pri_hardware (PISDN_ADAPTER IoAdapter) {
	return (0);
}

#else /* } { */

static int load_4pri_hardware (PISDN_ADAPTER IoAdapter) {
	return (0);
}

#endif /* } */

