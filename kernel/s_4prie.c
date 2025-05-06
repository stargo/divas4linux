
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
#include "dsp_defs.h"
#include "divatest.h" /* Adapter test framework */
#include "fpga_rom.h"
#include "fpga_rom_xdi.h"
#include "dsrv_4prie.h"

#define MAX_XLOG_SIZE  (64*1024)

int start_4prie_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword *cpu_reset_ctrl = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET];
	dword cpu_reset = *cpu_reset_ctrl;

	DBG_LOG(("CPU RESET REGISTER: %d %d",
					(cpu_reset & DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT) != 0,
					(cpu_reset & DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT) != 0))


	*cpu_reset_ctrl = cpu_reset & ~DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT;
	diva_os_wait(100);

	*cpu_reset_ctrl = cpu_reset & ~(DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT | DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT);
	diva_os_wait(100);

	return (0);
}

static void stop_4prie_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword *cpu_reset_ctrl = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET];
  volatile dword *dsp_reset = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DSP_RESET_REGISTER_OFFSET];
	dword cpu_reset = *cpu_reset_ctrl;
	dword i = 0;

	if (IoAdapter->host_vidi.vidi_active == 0) {
		IoAdapter->a.ram_out (&IoAdapter->a, &RAM->SWReg, SWREG_HALT_CPU) ;
		while ( (i < 100) && (IoAdapter->a.ram_in (&IoAdapter->a, &RAM->SWReg) != 0)) {
			diva_os_wait (1) ;
			i++ ;
		}
		if (i >= 100) {
			DBG_ERR(("A(%d) idi, failed to stop CPU", IoAdapter->ANum))
		}
	} else {
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

	*cpu_reset_ctrl = cpu_reset | (DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT | DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT);
	*dsp_reset = DIVA_4PRIE_FS_DSP_RESET_BITS;
}

static int load_4prie_hardware (PISDN_ADAPTER IoAdapter) {
  return (0);
}

static void disable_4prie_interrupt (PISDN_ADAPTER IoAdapter) {
	dword mask;
	int i = 1000;

	*(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_INTERRUPT_CONTROL_REGISTER] &= ~DIVA_4PRIE_FS_INTERRUPT_CONTROL_REGISTER_INTERRUPT_REGISTER_BIT;
	while (i-- > 0 && (mask = *(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET]) != 0) {
		*(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET] = mask;
	}
}

static void reset_4prie_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword *cpu_reset_ctrl = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET];
  volatile dword *dsp_reset = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DSP_RESET_REGISTER_OFFSET];
	dword cpu_reset = *cpu_reset_ctrl;

	*cpu_reset_ctrl = cpu_reset |
        (DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT | DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT) /* Stop CPU */ |
				DIVA_4PRIE_FS_CPU_RESET_REGISTER_FPGA_RESET_BITS; /* Reset FPGA */
	*dsp_reset = DIVA_4PRIE_FS_DSP_RESET_BITS;
  diva_os_sleep (1);

	cpu_reset = *cpu_reset_ctrl;
	DBG_LOG(("CPU RESET REGISTER: %d %d",
					(cpu_reset & DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT) != 0,
					(cpu_reset & DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT) != 0))
}

/*
  Offset between the start of physical memory and
  begin of the shared memory
  */
static dword diva_4prie_ram_offset (ADAPTER* a) {
	PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
	return (IoAdapter->shared_ram_offset);
}

static void diva_4prie_cpu_trapped (PISDN_ADAPTER IoAdapter) {
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

static int diva_4prie_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList = IoAdapter->QuadroList;
	dword mask;
	int i;

	mask = *(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET];

	if (mask == 0)
		return (0);

	*(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET] = mask;

	for (i = 0; i < IoAdapter->tasks; i++) {
		if (mask & (((dword)1) << i)) {
		IoAdapter = QuadroList->QuadroAdapter[i] ;
		IoAdapter->IrqCount++ ;
			if (IoAdapter && IoAdapter->Initialized && IoAdapter->tst_irq(&IoAdapter->a)) {
				diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
			}
		}
	}

  return (1);
}

static int diva_4prie_msi_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList = IoAdapter->QuadroList;
	dword mask;
	int i;

	mask = *(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET];

	if (mask == 0)
		return (1);

	*(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DOORBELL_REGISTER_OFFSET] = mask;

	for (i = 0; i < IoAdapter->tasks; i++) {
		if (mask & (((dword)1) << i)) {
			IoAdapter = QuadroList->QuadroAdapter[i];
			IoAdapter->IrqCount++ ;
			if (IoAdapter && IoAdapter->Initialized && IoAdapter->tst_irq(&IoAdapter->a)) {
				diva_os_schedule_soft_isr (&IoAdapter->isr_soft_isr);
			}
		}
	}

	return (1);
}

static int diva_4prie_save_maint_buffer (PISDN_ADAPTER IoAdapter, const void* data, dword length) {
  if (IoAdapter->ram != 0 && IoAdapter->MemorySize > MP_SHARED_RAM_OFFSET &&
      (IoAdapter->MemorySize - MP_SHARED_RAM_OFFSET) > length) {
    if (IoAdapter->Initialized) {
			volatile dword *cpu_reset_ctrl = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_CPU_RESET_REGISTER_OFFSET];
			volatile dword *dsp_reset = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DSP_RESET_REGISTER_OFFSET];
			dword cpu_reset = *cpu_reset_ctrl;

			*cpu_reset_ctrl = cpu_reset | (DIVA_4PRIE_FS_CPU_RESET_REGISTER_COLD_RESET_BIT | DIVA_4PRIE_FS_CPU_RESET_REGISTER_WARM_RESET_BIT);
			*dsp_reset = DIVA_4PRIE_FS_DSP_RESET_BITS;
			reset_4prie_hardware (IoAdapter);
      IoAdapter->Initialized = 0;
			disable_4prie_interrupt (IoAdapter);

      memcpy (IoAdapter->ram, data, length);
      return (0);
		}
	}

  return (-1);
}

static int diva_4prie_clock_control (PISDN_ADAPTER IoAdapter, const diva_xdi_clock_control_command_t* cmd) {
  return (-1);
}

static dword diva_4prie_detect_dsps (PISDN_ADAPTER IoAdapter) {
  return (0);
}

/* -------------------------------------------------------------
                Download FPFA image
         ------------------------------------------------------------- */
#if 0
#define FPGA_PROG   0x0001 /* PROG enable low     */
#define FPGA_BUSY   0x0002 changed to bit 0     /* BUSY high, DONE low */
#define FPGA_CS     0x000C not used     /* Enable I/O pins     */
#define FPGA_CCLK   0x0100
#define FPGA_DOUT   0x0400
#define FPGA_DIN    FPGA_DOUT   /* bidirectional I/O */
#endif

#define DIVA_4PRIE_CUSTOM_HARDWARE_FPFA_DOWNLOAD_REGISTER_ADDRESS 0x3010
#define FPGA_PROG   (1U << 16) /* PROG enable low     */
#define FPGA_BUSY   (1U << 17)  /* BUSY high, DONE low */
#define FPGA_CS     0x000C not used     /* Enable I/O pins     */
#define FPGA_CCLK   (1U << 24)
#define FPGA_DOUT   (1U << 26)
#define FPGA_DIN    FPGA_DOUT   /* bidirectional I/O */
/*
Bit 0:    DONE              (read only)


Bit 16: Program Pin      (read and write)

Bit 24: CCLK               (read and write)

Bit 26:  DATA               (read and write)
*/

int pri_4prie_FPGA_download (PISDN_ADAPTER IoAdapter) {
  volatile dword *addr = (volatile dword*)&IoAdapter->reset[0x3010];
  volatile dword *dsp_reset = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_FS_DSP_RESET_REGISTER_OFFSET];
  dword code   = 0, FileLength;
  dword val, baseval = FPGA_PROG;
  int bit;
  byte *File;

  if (!(File = (byte *)xdiLoadFile("ds4prie_10.bit", &FileLength, 0))) {
    return (0);
  }

  /*
    Reset DSP
    */
	*dsp_reset = DIVA_4PRIE_FS_DSP_RESET_BITS;

  /*
   *  prepare download, pulse PROGRAM pin down.
   */
  *addr = (baseval & ~FPGA_PROG); /* PROGRAM low pulse */
  diva_os_wait (5);
  *addr = baseval; /* release */
  diva_os_wait(200);

  /*
   *  check done pin, must be low
   */
  if (*addr & FPGA_BUSY) {
    DBG_FTL(("FPGA download: acknowledge for FPGA memory clear missing"))
    xdiFreeFile (File);
    return (0);
  }

  /*
   *  put data onto the FPGA
   */
  while ( code < FileLength )
  {
    val = ((dword)File[code++]) << (3+16);

    for ( bit = 8; bit-- > 0; val <<= 1 ) // put byte onto FPGA
    {
      baseval &= ~FPGA_DOUT;             // clr  data bit
      baseval |= (val & FPGA_DOUT);      // copy data bit
      *addr    = baseval;
      *addr    = baseval | FPGA_CCLK;     // set CCLK hi
      *addr    = baseval | FPGA_CCLK;     // set CCLK hi
      *addr    = baseval;                 // set CCLK lo
    }
    if (code % 2048 == 0) {
      diva_os_sleep (0xffffffff);
    }
  }

  xdiFreeFile (File);
  diva_os_wait (100);
  val = *addr;

  if ( !(val & FPGA_BUSY) )
  {
    DBG_FTL(("FPGA download: chip remains in busy state (0x%04x)", val))
    return (0);
  }

  diva_os_wait(200);

  if ( !(val & FPGA_BUSY) )
  {
    DBG_FTL(("FPGA download: during second check chip remains in busy state again (0x%04x)", val))
    return (0);
  }

  return (1);
}

/* -------------------------------------------------------------------------
  Install entry points for 4PRI Adapter
  ------------------------------------------------------------------------- */
void prepare_4prie_functions (PISDN_ADAPTER BaseIoAdapter) {
  int i;

  for (i = 0; i < BaseIoAdapter->tasks; i++) {
    PISDN_ADAPTER IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i];
    ADAPTER *a = &IoAdapter->a;

    IoAdapter->MemorySize = MP2_MEMORY_SIZE;
    IoAdapter->plx_offset = 0x20080;

    a->ram_in           = mem_in;
    a->ram_inw          = mem_inw;
    a->ram_in_buffer    = mem_in_buffer;
    a->ram_look_ahead   = mem_look_ahead;
    a->ram_out          = mem_out;
    a->ram_outw         = mem_outw;
    a->ram_out_buffer   = mem_out_buffer;
    a->ram_inc          = mem_inc;
    a->ram_offset       = diva_4prie_ram_offset;
    a->ram_out_dw       = mem_out_dw;
    a->ram_in_dw        = mem_in_dw;

    if (IoAdapter->host_vidi.vidi_active) {
      IoAdapter->out      = vidi_host_pr_out;
      IoAdapter->dpc      = vidi_host_pr_dpc;
      IoAdapter->tst_irq  = vidi_host_test_int;
      IoAdapter->clr_irq  = vidi_host_clear_int;
    } else {
      IoAdapter->out      = pr_out;
      IoAdapter->dpc      = pr_dpc;
      IoAdapter->tst_irq  = scom_test_int;
      IoAdapter->clr_irq  = scom_clear_int;
    }

    IoAdapter->pcm      = (struct pc_maint *)(MIPS_MAINT_OFFS - MP_SHARED_RAM_OFFSET);
    IoAdapter->stop     = stop_4prie_hardware;

    IoAdapter->load     = load_4prie_hardware;
    IoAdapter->disIrq   = disable_4prie_interrupt;
    IoAdapter->rstFnc   = reset_4prie_hardware;
    IoAdapter->trapFnc  = diva_4prie_cpu_trapped;

    IoAdapter->diva_isr_handler       = IoAdapter->msi == 0 ? diva_4prie_ISR : diva_4prie_msi_ISR;
    if (i == 0) {
    IoAdapter->save_maint_buffer_proc = diva_4prie_save_maint_buffer;
    IoAdapter->clock_control_Fnc      = diva_4prie_clock_control;
    }

    IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
    IoAdapter->DetectDsps = diva_4prie_detect_dsps;

    IoAdapter->shared_ram_offset = i * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE) + MP_SHARED_RAM_OFFSET;

    diva_os_prepare_4prie_functions (IoAdapter);
  }
}

