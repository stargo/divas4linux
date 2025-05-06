
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
#include "dsrv_4prile.h"

/*
  LOCALS
*/
static void reset_4prile_hardware (PISDN_ADAPTER IoAdapter);

int start_4prile_hardware (PISDN_ADAPTER IoAdapter) {
  dword i, tasks = IoAdapter->tasks, max_tasks = (tasks <= 4) ? 4 : 8;

	reset_4prile_hardware (IoAdapter);

	for (i = tasks; i < max_tasks; i++) {
		*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_IMASK_REGISTER_OFFSET(i)] = DIVA_4PRILE_FALC_ISR_MASK;
	}

	return (0);
}

static void stop_4prile_hardware (PISDN_ADAPTER IoAdapter) {
	volatile dword *taskTdmCtrl;
	dword ControllerNumberShuffled;
	dword i;

	for (i = 0; i < 8; i++) {
		ControllerNumberShuffled = DIVA_L_MP_SHUFFLE(i);
		taskTdmCtrl = (volatile dword*)&IoAdapter->reset[DIVA_4PRIE_TDM_CONTROL_REGISTER_OFFSET(ControllerNumberShuffled)];
		*taskTdmCtrl = 0; /* Stop and reset all perepherials and turn all LEDs on */
	}
}

/*
	Disable interrupta for all tasks
	*/
static void disable_4prile_interrupt (PISDN_ADAPTER IoAdapter) {
	volatile dword* doorbellRegister = (volatile dword*)&IoAdapter->reset[DIVA_4PRILE_FPGA_DOORBELL_REGISTER_OFFSET];
  dword doorbellRegisterValue;
  dword i;

	*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_DMA_MASK_REGISTER_OFFSET] = 0;
	do {
		for (i = 0; i < 8; i++) {
			*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(i)]  = 0;
			*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_IMASK_REGISTER_OFFSET(i)] = 0;
		}

		doorbellRegisterValue = *doorbellRegister;
		*doorbellRegister = doorbellRegisterValue;

	} while (doorbellRegisterValue != 0);
}

static void reset_4prile_hardware (PISDN_ADAPTER IoAdapter) {
	stop_4prile_hardware (IoAdapter);
	disable_4prile_interrupt (IoAdapter);
}

static int diva_4prile_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	PADAPTER_LIST_ENTRY QuadroList = IoAdapter->QuadroList;
	PISDN_ADAPTER currentIoAdapter;
	volatile dword* doorbellRegister = (volatile dword*)&IoAdapter->reset[DIVA_4PRILE_FPGA_DOORBELL_REGISTER_OFFSET];
	dword i, v, tasks = IoAdapter->tasks;
	dword doorbellRegisterValue, maskRegisterValue;
	dword falcStatus;
	dword dmaStatus;

	doorbellRegisterValue = *doorbellRegister;

	if (doorbellRegisterValue == 0)
		return (0);

	*doorbellRegister = doorbellRegisterValue;

	maskRegisterValue = *(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(IoAdapter->ControllerNumber)];

	for (i = 0; i < tasks; i++) {
		falcStatus = (1U << i); // Not shifted
		dmaStatus  = (1U << (i+16)); // Shifted
		if ((doorbellRegisterValue & (falcStatus | dmaStatus)) != 0) {
			currentIoAdapter = QuadroList->QuadroAdapter[i] ;
			v = (DIVA_4PRILE_FALC_ISR_MASK << (currentIoAdapter->ControllerNumber * 2)) |
						(1U << (currentIoAdapter->ControllerNumber + 16));
			if ((maskRegisterValue & v) != 0) {
				dword isrInfo = 0;
				*(volatile dword*)&currentIoAdapter->reset[DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(currentIoAdapter->ControllerNumber)] = 0;
				if ((doorbellRegisterValue & falcStatus) != 0)
					isrInfo |= 1;
				if ((doorbellRegisterValue & dmaStatus) != 0)
					isrInfo |= 4;
				isrInfo <<= 24;
				isrInfo |= ((dword)(unsigned long)currentIoAdapter->chained_isr_proc_context);

				currentIoAdapter->chained_isr_proc ((void*)(long)isrInfo);
			}
		}
	}

	return (1);
}

static int diva_4prile_msi_ISR (struct _ISDN_ADAPTER* IoAdapter) {
	volatile dword* doorbellRegister = (volatile dword*)&IoAdapter->reset[DIVA_4PRILE_FPGA_DOORBELL_REGISTER_OFFSET];

	do {
		diva_4prile_ISR (IoAdapter);
	} while (*doorbellRegister != 0);

	return (1);
}

static dword diva_4prile_detect_dsps (PISDN_ADAPTER IoAdapter) {
  return (0);
}

/* -------------------------------------------------------------------------
  Install entry points for 4PRI Adapter
  ------------------------------------------------------------------------- */
void prepare_4prile_functions (PISDN_ADAPTER BaseIoAdapter) {
  int i;

  for (i = 0; i < BaseIoAdapter->tasks; i++) {
    PISDN_ADAPTER IoAdapter = BaseIoAdapter->QuadroList->QuadroAdapter[i];
    ADAPTER *a = &IoAdapter->a;

    IoAdapter->MemorySize = 2048;
    IoAdapter->ControllerNumberShuffled = DIVA_L_MP_SHUFFLE(IoAdapter->ControllerNumber);

    a->ram_in           = no_mem_in;
    a->ram_inw          = no_mem_inw;
    a->ram_in_buffer    = no_mem_in_buffer;
    a->ram_look_ahead   = no_mem_look_ahead;
    a->ram_out          = no_mem_out;
    a->ram_outw         = no_mem_outw;
    a->ram_out_buffer   = no_mem_out_buffer;
    a->ram_inc          = no_mem_inc;
    a->ram_offset       = no_ram_offset;
    a->ram_out_dw       = no_mem_out_dw;
    a->ram_in_dw        = no_mem_in_dw;

    IoAdapter->out      = no_pr_out;
    IoAdapter->dpc      = no_dpc;
    IoAdapter->tst_irq  = no_test_int;
    IoAdapter->clr_irq  = no_clear_int;

    IoAdapter->pcm      = (struct pc_maint *)(MIPS_MAINT_OFFS - MP_SHARED_RAM_OFFSET);
    IoAdapter->stop     = stop_4prile_hardware;
    
    IoAdapter->load     = no_load;
    IoAdapter->disIrq   = disable_4prile_interrupt;
    IoAdapter->rstFnc   = reset_4prile_hardware;
    IoAdapter->trapFnc  = no_cpu_trapped;

    IoAdapter->diva_isr_handler = IoAdapter->msi == 0 ? diva_4prile_ISR : diva_4prile_msi_ISR;
		IoAdapter->save_maint_buffer_proc = 0;

    IoAdapter->DivaAdapterTestProc = DivaAdapterTest;
    IoAdapter->DetectDsps = diva_4prile_detect_dsps;

    IoAdapter->shared_ram_offset = i * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE) + MP_SHARED_RAM_OFFSET;

    diva_os_prepare_4prile_functions (IoAdapter);
  }
}

static void diva_4prile_disable_task_interrupts (PISDN_ADAPTER IoAdapter) {
	volatile dword* doorbellRegister = (volatile dword*)&IoAdapter->reset[DIVA_4PRILE_FPGA_DOORBELL_REGISTER_OFFSET];
	dword v = (1U << IoAdapter->ControllerNumber) | (1U << (IoAdapter->ControllerNumberShuffled+16));
	dword dmaMaskRegisterValue = \
			*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(IoAdapter->ControllerNumberShuffled)];
	volatile dword *taskTdmCtrl = \
			(volatile dword*)&IoAdapter->reset[DIVA_4PRIE_TDM_CONTROL_REGISTER_OFFSET(IoAdapter->ControllerNumberShuffled)];

	*taskTdmCtrl = 0; /* Stop and reset all perepherials and turn all LEDs on */

	dmaMaskRegisterValue = (dmaMaskRegisterValue >> 16) & 0xffU;
	dmaMaskRegisterValue &= ~(1U << IoAdapter->ControllerNumberShuffled);
	*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_DMA_MASK_REGISTER_OFFSET] = dmaMaskRegisterValue; /* Disable DMA interrupts */

	do {
		/* Disable FALC and EC interrupts */
		*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_MASK_REGISTER_OFFSET(IoAdapter->ControllerNumber)]  = 0;
		/* Clear imask */
		*(volatile dword*)&IoAdapter->reset[DIVA_4PRILE_TASK_IMASK_REGISTER_OFFSET(IoAdapter->ControllerNumber)] = 0;
		*doorbellRegister = v;
	} while ((*doorbellRegister & v) != 0);
}

int diva_4prile_connect_interrupt (PISDN_ADAPTER IoAdapter, int (*isr_proc)(void*), void* isr_proc_context) {
	if (isr_proc != 0) {
		IoAdapter->chained_isr_proc = isr_proc;
		IoAdapter->chained_isr_proc_context = isr_proc_context;
	} else {
		diva_4prile_disable_task_interrupts (IoAdapter);
	}

	return (0);
}

static void diva_4prile_write2flash (volatile byte* devices, dword address, dword v) {
	volatile dword* addr  = (dword*)&devices[DIVA_L_MP_FLASH_ADDRESS_REGISTER_OFFSET];
	volatile dword* data  = (dword*)&devices[DIVA_L_MP_FLASH_DATA_REGISTER_OFFSET];
	volatile dword* cntrl = (dword*)&devices[DIVA_L_MP_FLASH_CONTROL_REGISTER_OFFSET];

	*addr = address;
	*data = v;

   /* WP_n, Wr_n, OE_n, CS_n ={1110} active low signals */
   *cntrl = 0x8000000E; /* Wp_n=1 means write allowed */
   *cntrl = 0x8000000A; /* Wp_n=1 WE_n=0 cs_n=0 */

   // WP_n, Wr_n, OE_n, CS_n ={1110}    
   *cntrl = 0x8000000E; // We_n goes high, wirte complete
   *cntrl = 0x8000000F; // Ce_n goes High, cycle complaete 
}

static word diva_4prile_read4flash (volatile byte* devices, dword address) {
	volatile dword* addr  = (dword*)&devices[DIVA_L_MP_FLASH_ADDRESS_REGISTER_OFFSET];
	volatile dword* data  = (dword*)&devices[DIVA_L_MP_FLASH_DATA_REGISTER_OFFSET];
	volatile dword* cntrl = (dword*)&devices[DIVA_L_MP_FLASH_CONTROL_REGISTER_OFFSET];
	word ret;

	*addr = address;

	*cntrl = 0x80000006; /* wp_n,we_n,oe_n,cs_n ={0110} prepare to read */
  *cntrl = 0x80000004; /* wp_n,we_n,oe_n,cs_n ={0100} oe_n active */

  ret  = (word)*data;
 
  *cntrl = 0x80000006; /* wp_n,we_n,oe_n,cs_n ={0110} oe_n goes high */
  *cntrl = 0x80000007; /* wp_n,we_n,oe_n,cs_n ={0110} Rd Cmplt, ce_n goes high */

	return (ret);
}

static word diva_4prile_clear_flash_status (volatile byte* devices) {
	word status, vendor;

	diva_4prile_write2flash(devices, 0, 0x0070); /* Read status */
	status = diva_4prile_read4flash (devices, 0);

	diva_4prile_write2flash(devices, 0, 0x0050); /* Clear status */

	diva_4prile_write2flash(devices, 0, 0x0090);
	vendor = diva_4prile_read4flash (devices, 0);

	DBG_TRC(("flash status:%04x vendor:%04x", status, vendor))

	return (status);
}

/*
  Read serial number from flash.
  Call only if all perepherial devices are in reset.
  */
void diva_4prile_read_serial_number (PISDN_ADAPTER IoAdapter) {
	volatile byte* devices = IoAdapter->reset;
	volatile dword* cntrl = (dword*)&devices[DIVA_L_MP_FLASH_CONTROL_REGISTER_OFFSET];
	byte data[9+1];
	dword i;

  *cntrl = 0x80000007;

	diva_4prile_clear_flash_status (devices);
	diva_4prile_write2flash (devices, 0, 0x00ff);

	for (i = 0; i < (sizeof(data)-1)/2; i += 1) {
		word v = diva_4prile_read4flash (devices, 0xFFC031+i);
		*(word*)&data[i*2] = v;
	}

  *cntrl = 0x00000007;

	if (data[0] == 'R' && data[1] == 'C') {
		char* e;

		data[sizeof(data)-2] = data[2]; /* Move to end of string */
		data[2] = '0'; /* Replace by zero */
		data[sizeof(data)-1] = 0;
		IoAdapter->serialNo = (dword)(strtol(&data[2], &e, 10) & 0x00ffffffU);

		if (&data[sizeof(data)]-1 != (byte*)e)
			IoAdapter->serialNo = 0;
	}

	if (IoAdapter->serialNo == 0) {
		DBG_ERR(("A(%d) failed to read serial number", IoAdapter->ANum))
		IoAdapter->serialNo = 1;
	}
}

