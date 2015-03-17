/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic Networks range of DIVA Server Adapters.
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
#include <stdarg.h>
#include "debuglib.h"
#include "cardtype.h"
#include "dlist.h"
#include "pc.h"
#include "pr_pc.h"
#include "di_defs.h"
#include "dsp_defs.h"
#include "di.h" 
#include "divasync.h"
#include "vidi_di.h" 
#include "io.h"

#include "xdi_msg.h"
#include "xdi_adapter.h"
#include "os_4pri.h"
#include "diva_pci.h"
#include "mi_pc.h"
#include "dsrv4bri.h" /* PLX constants */
#include "dsrv_4pri.h"

/*
	IMPORTS
	*/
extern void* diva_xdiLoadFileFile;
extern dword diva_xdiLoadFileLength;
extern void  prepare_4pri_functions   (PISDN_ADAPTER IoAdapter);
extern int   pri_4pri_FPGA_download   (PISDN_ADAPTER IoAdapter);
extern int   start_4pri_hardware      (PISDN_ADAPTER IoAdapter);
extern void diva_add_slave_adapter    (diva_os_xdi_adapter_t* a);
extern void diva_xdi_display_adapter_features (int card);
extern int diva_card_read_xlog (diva_os_xdi_adapter_t* a);
extern int diva_4pri_fpga_ready (PISDN_ADAPTER IoAdapter);
extern void diva_4pri_memcpy (PISDN_ADAPTER IoAdapter, byte* mem, dword address, const byte* src, dword length);

/*
**  LOCALS
*/
static unsigned long _4pri_bar_length[4] = {
  512,
  256, /* I/O */
  8*1024*1024,
  256*1024
};
static int diva_4pri_cleanup_adapter (diva_os_xdi_adapter_t* a);
static int diva_4pri_stop_no_io (diva_os_xdi_adapter_t* a);
static int diva_4pri_cleanup_slave_adapters (diva_os_xdi_adapter_t* a);
static int diva_4pri_reset_adapter (struct _diva_os_xdi_adapter* a);
static int diva_4pri_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length);
static int diva_4pri_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length);
static int diva_4pri_stop_adapter (struct _diva_os_xdi_adapter* a);
static int diva_4pri_start_adapter (PISDN_ADAPTER IoAdapter, dword features);
static int diva_4pri_write_sdram_block (PISDN_ADAPTER IoAdapter,
																		 		dword address,
																		 		const byte* data,
																		 		dword length,
																		 		dword limit);

/*
**  BAR0 - MEM - 512       - CONFIG MEM
**  BAR1 - I/O - 256       - UNUSED
**  BAR2 - MEM - 8MBytes   - SDRAM
**  BAR3 - MEM - 256KBytes - CNTRL
**
**  Called by master adapter, that will initialize and add slave adapters
*/
int diva_4pri_init_card (diva_os_xdi_adapter_t* a) {
	unsigned long bar_length[sizeof(_4pri_bar_length)/sizeof(_4pri_bar_length[0])];
	int i, bar, diva_tasks;
  PADAPTER_LIST_ENTRY quadro_list;
  diva_os_xdi_adapter_t* diva_current;
  diva_os_xdi_adapter_t* adapter_list[4];
  PISDN_ADAPTER Slave;
	dword offset;

	/*
		Fix up card ordinal to get right adapter name and features
		*/
	{
		byte Bus, Slot;
		void* hdev;
		word id = 0;

		Bus = a->resources.pci.bus;
		Slot = a->resources.pci.func;
		hdev = a->resources.pci.hdev;

		PCIread (Bus, Slot, 0x2e, &id, sizeof(id), hdev) ;

		DBG_LOG(("A(%d) read Subsystem ID %04x", a->xdi_adapter.ANum, id))

	  a->xdi_adapter.BusNumber  = a->resources.pci.bus;
	  a->xdi_adapter.slotNumber = a->resources.pci.func;
	  a->xdi_adapter.hdev       = a->resources.pci.hdev;
    if (!diva_get_serial_number (&a->xdi_adapter)) {
    /*
      Get Serial Number and card subsystem id (csid)
      The serial number is accessible in accordance with PCI spec
      via command register located in configuration space, also we do not
      have to map any BAR before we can access it
      */
      DBG_ERR(("Failed to read serial number for %s adapter", a->xdi_adapter.Properties.Name))
      return (-1);
    }

		switch (id) {
			case 0xE01E:
			case 0x1E02:
		    a->CardOrdinal = CARDTYPE_DIVASRV_2P_V_V10_PCI;
				a->dsp_mask = 0x0000001f;
				diva_tasks = 2;
        break;

			case 0x1E01:
			case 0x1E03:
				a->CardOrdinal = CARDTYPE_DIVASRV_2P_M_V10_PCI;
				a->dsp_mask = 0x0000001f;
				diva_tasks = 2;
        break;

			case 0xE020:
			case 0x2002:
				a->CardOrdinal = CARDTYPE_DIVASRV_4P_V_V10_PCI;
				a->dsp_mask = 0x000003ff;
				diva_tasks = 4;
				break;

			case 0x2001:
			case 0x2003:
				a->CardOrdinal = CARDTYPE_DIVASRV_4P_M_V10_PCI;
				a->dsp_mask = 0x000003ff;
				diva_tasks = 4;
				break;

			case 0xE02A:
				a->CardOrdinal = CARDTYPE_DIVASRV_IPMEDIA_300_V10;
				a->dsp_mask = 0x000003ff;
				diva_tasks = 2;
				break;

			case 0xE02C:
				a->CardOrdinal = CARDTYPE_DIVASRV_IPMEDIA_600_V10;
				a->dsp_mask = 0x000003ff;
				diva_tasks = 4;
				break;

			case 0x3001:
				a->CardOrdinal = CARDTYPE_DIVASRV_V2P_V10H_PCIE;
				a->dsp_mask    = 0x000003ff;
				diva_tasks     = 2;
				break;

			case 0x3002:
				a->CardOrdinal = CARDTYPE_DIVASRV_V1P_V10H_PCIE;
				a->dsp_mask    = 0x000003ff;
				diva_tasks     = 1;
				break;

			case 0xE030:
				a->CardOrdinal = CARDTYPE_DIVASRV_V4P_V10H_PCIE;
				a->dsp_mask    = 0x00000007;
				diva_tasks     = 4;
				break;

			default:
				DBG_ERR(("Unknown adapter"))
				return -1;
		}
	}

	/*
		Set properties
		*/
	a->xdi_adapter.Properties = CardProperties[a->CardOrdinal];

	memcpy (bar_length, _4pri_bar_length, sizeof(bar_length));

	DBG_LOG(("Load %s, SN:%ld, bus:%02x, func:%02x",
					a->xdi_adapter.Properties.Name,
					a->xdi_adapter.serialNo,
					a->resources.pci.bus,
					a->resources.pci.func))
    
	/*
		First initialization step: get and check hardware resoures.
		Do not map resources and do not access card at this step.
		*/
	for (bar = 0; bar < 4; bar++) {
		a->resources.pci.bar[bar] = divasa_get_pci_bar (a->resources.pci.bus,
																										a->resources.pci.func,
																										bar,
																										a->resources.pci.hdev);
		if (!a->resources.pci.bar[bar] || (a->resources.pci.bar[bar] == 0xFFFFFFF0)) {
			if (bar != 1) {
				DBG_ERR(("%s : invalid bar[%d]=%08x",
							a->xdi_adapter.Properties.Name, bar, a->resources.pci.bar[bar]))
				return (-1);
			} else {
				a->resources.pci.bar[bar] = 0;
			}
		}
	}
	a->resources.pci.irq = divasa_get_pci_irq (a->resources.pci.bus,
																						 a->resources.pci.func,
																						 a->resources.pci.hdev);
	if (a->resources.pci.irq == 0) {
		DBG_FTL(("%s : failed to read irq", a->xdi_adapter.Properties.Name));
		return (-1);
	}

	a->xdi_adapter.sdram_bar = a->resources.pci.bar [2];

	/*
		Map all MEMORY BAR's
		*/
	for (bar = 0; bar < 4; bar++) {
		if (bar != 1) { /* ignore I/O */
			a->resources.pci.addr[bar] = divasa_remap_pci_bar (a, a->resources.pci.bar[bar], bar_length[bar]);
			if (a->resources.pci.addr[bar] == 0) {
				DBG_FTL(("%s : failed to map bar[%d]", a->xdi_adapter.Properties.Name, bar))
				diva_4pri_cleanup_adapter (a);
				return (-1);
			}
		}
	}

	/*
		Register I/O port
		*/
	sprintf (&a->port_name[0],
					 "DIVA %sPRI %ld",
					 ((diva_tasks == 4) ? "4" : "2"),
					 (long)a->xdi_adapter.serialNo);
	if (a->resources.pci.bar[1] != 0 &&
			diva_os_register_io_port (a, 1, a->resources.pci.bar[1],
																bar_length[1],
																&a->port_name[0])) {
		DBG_ERR(("%s : failed to register bar[1]", a->xdi_adapter.Properties.Name))
		diva_4pri_cleanup_adapter (a);
		return (-1);
	}
	a->resources.pci.addr[1] = (void*)(unsigned long)a->resources.pci.bar[1];

  /*
    Set cleanup pointer for base adapter only, so slave adapter
    will be unable to get cleanup
  */
  a->interface.cleanup_adapter_proc = diva_4pri_cleanup_adapter;
  a->interface.stop_no_io_proc      = diva_4pri_stop_no_io;

	/*
		Create slave adapters
		*/
	for (i = 0; i < diva_tasks - 1; i++) {
		if (!(a->slave_adapters[i] = (diva_os_xdi_adapter_t*)diva_os_malloc (0, sizeof(*a)))) {
			for (i = 0; i < diva_tasks - 1; i++) {
				if (a->slave_adapters[i] != 0) {
					diva_os_free (0, a->slave_adapters[i]);
					a->slave_adapters[i] = 0;
				}
			}
			diva_4pri_cleanup_adapter (a);
			return (-1);
		}
		memset (a->slave_adapters[i], 0x00, sizeof(*a));
	}

  adapter_list[0] = a;
  adapter_list[1] = a->slave_adapters[0];
  adapter_list[2] = a->slave_adapters[1];
  adapter_list[3] = a->slave_adapters[2];

	/*
		Alloc adapter list
		*/
	quadro_list = (PADAPTER_LIST_ENTRY)diva_os_malloc (0, sizeof(*quadro_list));
	if ((a->slave_list = quadro_list) == 0) {
		for (i = 0; i < diva_tasks - 1; i++) {
			if (a->slave_adapters[i] != 0) {
				diva_os_free (0, a->slave_adapters[i]);
				a->slave_adapters[i] = 0;
			}
		}
		diva_4pri_cleanup_adapter (a);
		return (-1);
	}
	memset (quadro_list, 0x00, sizeof(*quadro_list));

	/*
		Set interfaces
		*/
	a->xdi_adapter.QuadroList = quadro_list;
	for (i = 0; i < diva_tasks; i++) {
		adapter_list[i]->xdi_adapter.ControllerNumber = i;
		adapter_list[i]->xdi_adapter.tasks = diva_tasks;
		quadro_list->QuadroAdapter[i] = &adapter_list[i]->xdi_adapter;
	}

	for (i = 0; i < diva_tasks; i++) {
		diva_current = adapter_list[i];

		diva_current->dsp_mask = 0x7fffffff;
    diva_current->xdi_adapter.a.io = &diva_current->xdi_adapter;
    diva_current->xdi_adapter.DIRequest = request;
    diva_current->interface.cmd_proc = diva_4pri_cmd_card_proc;
    diva_current->xdi_adapter.Properties = CardProperties[a->CardOrdinal];
    diva_current->CardOrdinal = a->CardOrdinal;

    diva_current->xdi_adapter.Channels = CardProperties[a->CardOrdinal].Channels;
    diva_current->xdi_adapter.e_max    = CardProperties[a->CardOrdinal].E_info;
    diva_current->xdi_adapter.e_tbl    = diva_os_malloc (0, diva_current->xdi_adapter.e_max * sizeof(E_INFO)); 

		if (diva_current->xdi_adapter.e_tbl == 0) {
			diva_4pri_cleanup_slave_adapters (a);
			diva_4pri_cleanup_adapter (a);
			for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
			return (-1);
		}
		memset (diva_current->xdi_adapter.e_tbl, 0x00, diva_current->xdi_adapter.e_max * sizeof(E_INFO));

		if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.isr_spin_lock, "isr")) {
			diva_4pri_cleanup_slave_adapters (a);
			diva_4pri_cleanup_adapter (a);
			for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
			return (-1);
		}
		if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.data_spin_lock, "data")) {
			diva_4pri_cleanup_slave_adapters (a);
			diva_4pri_cleanup_adapter (a);
			for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
			return (-1);
		}

		if (diva_tasks == 2)
			strcpy (diva_current->xdi_adapter.req_soft_isr.dpc_thread_name, "kdivas2prid");
		else
			strcpy (diva_current->xdi_adapter.req_soft_isr.dpc_thread_name, "kdivas4prid");

		if (diva_os_initialize_soft_isr (&diva_current->xdi_adapter.req_soft_isr,
																		 DIDpcRoutine,
																		 &diva_current->xdi_adapter)) {
			diva_4pri_cleanup_slave_adapters (a);
			diva_4pri_cleanup_adapter (a);
			for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
			return (-1);
		}

		/*
			Do not initialize second DPC - only one thread will be created
			*/
		diva_current->xdi_adapter.isr_soft_isr.object = diva_current->xdi_adapter.req_soft_isr.object;


	}

	prepare_4pri_functions (&a->xdi_adapter);

	/*
		Set up hardware related pointers
		*/
	a->xdi_adapter.cfg     = (void*)(unsigned long)a->resources.pci.bar[0]; /* BAR0 CONFIG */
	a->xdi_adapter.port    = (void*)(unsigned long)a->resources.pci.bar[1]; /* BAR1        */
	a->xdi_adapter.Address = a->resources.pci.addr[2];    /* BAR2 SDRAM  */
	a->xdi_adapter.reset   = a->resources.pci.addr[0];      /* BAR0 CONFIG */
	a->xdi_adapter.ram     = a->resources.pci.addr[2];      /* BAR2 SDRAM  */

	a->xdi_adapter.AdapterTestMemoryStart  = a->resources.pci.addr[2]; /* BAR2 SDRAM  */
	a->xdi_adapter.AdapterTestMemoryLength = bar_length[2];

	/*
		ctlReg contains the register address for the MIPS CPU reset control
		*/
	a->xdi_adapter.ctlReg = a->resources.pci.addr[3];  /* BAR3 CNTRL  */
	/*
		prom contains the register address for FPGA and EEPROM programming
		*/
	a->xdi_adapter.prom = &a->xdi_adapter.reset[0x6E];
	/*
		reset contains the base address for the PLX 9054 register set
		*/
	a->xdi_adapter.reset[PLX9054_INTCSR] = 0x00; /* disable PCI interrupts */

  for (i = 0; i < diva_tasks; i++) {
		Slave            = a->xdi_adapter.QuadroList->QuadroAdapter[i];
		diva_init_dma_map_pages (a->resources.pci.hdev, (struct _diva_dma_map_entry**)&Slave->dma_map2, 1, 2);
	}

	/*
		Replicate addresses to all instances, set shared memory
		address for all instances
		*/
  for (i = 0; i < diva_tasks; i++) {
		Slave            = a->xdi_adapter.QuadroList->QuadroAdapter[i];
		offset           = Slave->ControllerNumber * (MP_SHARED_RAM_OFFSET + MP_SHARED_RAM_SIZE);
		Slave->Address   = a->xdi_adapter.Address + offset;
		Slave->ram       = a->xdi_adapter.ram     + offset;
		Slave->reset     = a->xdi_adapter.reset;
		Slave->ctlReg    = a->xdi_adapter.ctlReg;
		Slave->prom      = a->xdi_adapter.prom;
		Slave->reset     = a->xdi_adapter.reset;
		Slave->sdram_bar = a->xdi_adapter.sdram_bar;

		/*
			Nr DMA pages:
			Tx 31+31+31
			Rx 72
			Li 32
			*/

		diva_init_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry**)&Slave->dma_map, 200);
	}
	for (i = 0; i < diva_tasks; i++) {
		Slave       = a->xdi_adapter.QuadroList->QuadroAdapter[i];
		Slave->ram += MP_SHARED_RAM_OFFSET;
		DBG_LOG(("Adapter %d ram at %p (%p)", i, Slave->ram, Slave->ram - MP_SHARED_RAM_OFFSET))
	}
  for (i = 1; i < diva_tasks; i++) {
		Slave           = a->xdi_adapter.QuadroList->QuadroAdapter[i];
		Slave->serialNo = ((dword)(Slave->ControllerNumber << 24)) | a->xdi_adapter.serialNo;
		Slave->cardType = a->xdi_adapter.cardType;
		memcpy (Slave->hw_info, a->xdi_adapter.hw_info, sizeof(a->xdi_adapter.hw_info));
	}

	/*
		Set IRQ handler
		*/
	a->xdi_adapter.irq_info.irq_nr = a->resources.pci.irq;
	sprintf (a->xdi_adapter.irq_info.irq_name, "DIVA %dPRI %ld", diva_tasks, (long)a->xdi_adapter.serialNo);

  if (diva_os_register_irq (a, a->xdi_adapter.irq_info.irq_nr,
                            a->xdi_adapter.irq_info.irq_name, a->xdi_adapter.msi)) {
    diva_4pri_cleanup_slave_adapters (a);
    diva_4pri_cleanup_adapter (a);
    for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
    return (-1);
  }

  a->xdi_adapter.irq_info.registered = 1;

	/*
		Add slave adapter
		*/
	for (i = 1; i < diva_tasks; i++) {
		diva_add_slave_adapter (adapter_list[i]);
	}

	diva_log_info("%s IRQ:%u SerNo:%d",
								a->xdi_adapter.Properties.Name,
								a->resources.pci.irq,
								a->xdi_adapter.serialNo);

	return (0);
}

static int diva_4pri_cleanup_slave_adapters (diva_os_xdi_adapter_t* a) {
	diva_os_xdi_adapter_t* adapter_list[4];
	diva_os_xdi_adapter_t* diva_current;
	int i;

	adapter_list[0] = a;
	adapter_list[1] = a->slave_adapters[0];
	adapter_list[2] = a->slave_adapters[1];
	adapter_list[3] = a->slave_adapters[2];

	for (i = 0; i < a->xdi_adapter.tasks; i++) {
		diva_current = adapter_list[i];
		if (diva_current != 0) {
			diva_os_cancel_soft_isr (&diva_current->xdi_adapter.req_soft_isr);
			diva_os_cancel_soft_isr (&diva_current->xdi_adapter.isr_soft_isr);

			diva_os_remove_soft_isr (&diva_current->xdi_adapter.req_soft_isr);
			diva_current->xdi_adapter.isr_soft_isr.object = 0;

			diva_os_destroy_spin_lock (&diva_current->xdi_adapter.isr_spin_lock, "unload");
			diva_os_destroy_spin_lock (&diva_current->xdi_adapter.data_spin_lock,"unload");

			diva_cleanup_vidi (&diva_current->xdi_adapter);

			diva_free_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry*)diva_current->xdi_adapter.dma_map);
			diva_current->xdi_adapter.dma_map = 0;
			diva_free_dma_map_pages (a->resources.pci.hdev,
															 (struct _diva_dma_map_entry*)diva_current->xdi_adapter.dma_map2, 2);
			diva_current->xdi_adapter.dma_map2 = 0;

			if (diva_current->xdi_adapter.e_tbl) {
				diva_os_free (0, diva_current->xdi_adapter.e_tbl);
			}
			diva_current->xdi_adapter.e_tbl   = 0;
			diva_current->xdi_adapter.e_max   = 0;
			diva_current->xdi_adapter.e_count = 0;
		}
	}

	return (0);
}

static int diva_4pri_cleanup_adapter (diva_os_xdi_adapter_t* a) {
	int bar;

	/*
		Stop Adapter if adapter is running
		*/
	if (a->xdi_adapter.Initialized) {
		diva_4pri_stop_adapter (a);
	}

	/*
		Remove IRQ handler
		*/
	if (a->xdi_adapter.irq_info.registered) {
		diva_os_remove_irq (a, a->xdi_adapter.irq_info.irq_nr);
	}
	a->xdi_adapter.irq_info.registered = 0;

  /*
    Free DPC's and spin locks on all adapters
  */
  diva_4pri_cleanup_slave_adapters (a);

	/*
		Unmap all BARS
		*/
	for (bar = 0; bar < 4; bar++) {
		if (bar != 1) {
			if (a->resources.pci.bar[bar] && a->resources.pci.addr[bar]) {
				divasa_unmap_pci_bar (a->resources.pci.addr[bar]);
				a->resources.pci.bar[bar]  = 0;
				a->resources.pci.addr[bar] = 0;
			}
		}
	}

	/*
		Unregister I/O
		*/
	if (a->resources.pci.bar[1] != 0 && a->resources.pci.addr[1]) {
		diva_os_register_io_port (a, 0, a->resources.pci.bar[1], 
															_4pri_bar_length[1],
															&a->port_name[0]);
		a->resources.pci.bar[1]  = 0;
		a->resources.pci.addr[1] = 0;
	}

  if (a->slave_list) {
    diva_os_free (0, a->slave_list);
    a->slave_list = 0;
  }

	DBG_LOG(("A(%d) %s adapter cleanup complete", a->xdi_adapter.ANum, a->xdi_adapter.Properties.Name))

	return (0);
}

static void diva_xdi_clock_isr_proc (const void* context, dword count) {
	struct _diva_os_xdi_adapter* a = (struct _diva_os_xdi_adapter*)context;
	diva_os_schedule_soft_isr (&a->clock_interrupt_soft_isr);
}

static void diva_xdi_clock_soft_isr (struct _diva_os_soft_isr* psoft_isr, void* context) {
	PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)context;

	DBG_LOG(("A(%d) clock interrupt interrupts:%u errors:%u status:%08x",
					IoAdapter->ANum, IoAdapter->clock_isr_count, IoAdapter->clock_isr_errors,
					*(volatile dword*)&IoAdapter->ctlReg[DIVA_4PRI_FPGA_CLOCK_INTERRUPT_STATUS_REGISTER]))
}

static int diva_4pri_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length) {
	int ret = -1;

	if (cmd->adapter != a->controller) {
		DBG_ERR(("4pri_cmd, wrong controller=%d != %d", cmd->adapter, a->controller))
		return (-1);
	}

	switch (cmd->command) {
		case DIVA_XDI_UM_CMD_GET_CARD_ORDINAL:
			a->xdi_mbox.data_length = sizeof(dword);
			a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
			if (a->xdi_mbox.data) {
				*(dword*)a->xdi_mbox.data = (dword)a->CardOrdinal;
				a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
				ret = 0;
			}
			break;

    case DIVA_XDI_UM_CMD_START_ADAPTER:
			if (a->xdi_adapter.ControllerNumber == 0) {
				ret = diva_4pri_start_adapter (&a->xdi_adapter, cmd->command_data.start.features);
			}
      break;

		case DIVA_XDI_UM_CMD_STOP_ADAPTER:
			if (a->xdi_adapter.ControllerNumber == 0) {
				ret = diva_4pri_stop_adapter (a);
			}
			break;

		case DIVA_XDI_UM_CMD_RESET_ADAPTER:
			if (a->xdi_adapter.ControllerNumber == 0) {
				ret = diva_4pri_reset_adapter (a);
			}
			break;

		case DIVA_XDI_UM_CMD_GET_SERIAL_NR:
			a->xdi_mbox.data_length = sizeof(dword);
			a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
			if (a->xdi_mbox.data) {
				*(dword*)a->xdi_mbox.data = (dword)a->xdi_adapter.serialNo;
				a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
				ret = 0;
			}
			break;

		case DIVA_XDI_UM_CMD_GET_PCI_HW_CONFIG:
			if (a->xdi_adapter.ControllerNumber == 0) {
				a->xdi_mbox.data_length = sizeof(dword)*9;
				a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
				if (a->xdi_mbox.data) {
					int i;
					dword* data = (dword*)a->xdi_mbox.data;

					for (i = 0; i < 8; i++) {
						*data++ = a->resources.pci.bar[i];
					}
					*data++ = a->resources.pci.irq;
					a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
					ret = 0;
				}
			}
			break;

		case DIVA_XDI_UM_CMD_GET_CARD_STATE:
			if (a->xdi_adapter.ControllerNumber == 0) {
				a->xdi_mbox.data_length = sizeof(dword);
				a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
				if (a->xdi_mbox.data) {
					dword* data = (dword*)a->xdi_mbox.data;
					if (!a->xdi_adapter.ram || !a->xdi_adapter.reset ||
							!a->xdi_adapter.cfg) {
						*data = 3;
					} else if (a->xdi_adapter.trapped) {
						*data = 2;
					} else if (a->xdi_adapter.Initialized) {
						*data = 1;
					} else {
						*data = 0;
					}
					a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
					ret = 0;
				}
			}
			break;

		case DIVA_XDI_UM_CMD_WRITE_FPGA:
			if (a->xdi_adapter.ControllerNumber == 0) {
				ret = diva_4pri_write_fpga_image (a, (byte*)&cmd[1], cmd->command_data.write_fpga.image_length);
			}
			break;

		case DIVA_XDI_UM_CMD_ADAPTER_TEST:
			if (a->xdi_adapter.ControllerNumber == 0) {
				volatile dword* las0ba_ctrl = (volatile dword*)&a->xdi_adapter.reset[0x04];
				dword las0ba_original = *las0ba_ctrl;
				dword i;

				a->xdi_adapter.AdapterTestMask = cmd->command_data.test.test_command;

				for (i = 0; i < 8 && a->xdi_adapter.DivaAdapterTestProc != 0; i++) {
					if (i != 0) {
						*las0ba_ctrl = (dword)(las0ba_original + i * _4pri_bar_length[2]);
					}
					if ((*(a->xdi_adapter.DivaAdapterTestProc))(&a->xdi_adapter) == 0) {
						DBG_LOG(("Memory test[%d] : OK", i))
						ret = 0;
					} else {
						DBG_ERR(("Memory test[%d] : FAILED", i))
						ret = -1;
						break;
					}
				}
				*las0ba_ctrl = las0ba_original;

				a->xdi_adapter.AdapterTestMask = 0;
			}
			break;

    case DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK:
			if (a->xdi_adapter.ControllerNumber == 0) {
      	ret = diva_4pri_write_sdram_block (&a->xdi_adapter,
																					 cmd->command_data.write_sdram.offset,
																					 (byte*)&cmd[1],
																					 cmd->command_data.write_sdram.length,
																					 64*1024*1024-64/* a->xdi_adapter.MemorySize */);
			}
			break;

		case DIVA_XDI_UM_CMD_READ_XLOG_ENTRY:
			ret = diva_card_read_xlog (a);
			break;

		case DIVA_XDI_UM_CMD_READ_SDRAM:
			if (a->xdi_adapter.ControllerNumber == 0 && a->xdi_adapter.Address != 0 &&
					diva_4pri_fpga_ready (&a->xdi_adapter) == 0) {
				if ((a->xdi_mbox.data_length = cmd->command_data.read_sdram.length)) {
					if ((a->xdi_mbox.data_length+cmd->command_data.read_sdram.offset) < a->xdi_adapter.MemorySize) { 
						a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
						if (a->xdi_mbox.data) {
							byte* src = a->xdi_adapter.Address;
							byte* dst = a->xdi_mbox.data;
							dword len = a->xdi_mbox.data_length;

							src += cmd->command_data.read_sdram.offset;

							while (len--) {
								*dst++ = *src++;
							}
							a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
							ret = 0;
						}
					}
				}
			}
			break;

		case DIVA_XDI_UM_CMD_GET_HW_INFO_STRUCT:
			a->xdi_mbox.data_length = sizeof(a->xdi_adapter.hw_info);
			if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length)) != 0) {
				memcpy (a->xdi_mbox.data, a->xdi_adapter.hw_info, sizeof(a->xdi_adapter.hw_info));
				a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
				ret = 0;
			}
			break;

    case DIVA_XDI_UM_CMD_SET_PROTOCOL_FEATURES:
      if (!a->xdi_adapter.ControllerNumber) {
        PISDN_ADAPTER IoAdapter = &a->xdi_adapter, Slave;
        int i;

        IoAdapter->features                = cmd->command_data.features.features;
        IoAdapter->a.protocol_capabilities = IoAdapter->features;

        DBG_TRC(("Set raw protocol features: %08x for Adapter %d",
                IoAdapter->ANum, IoAdapter->features))

        for (i = 0; ((i < IoAdapter->tasks) && IoAdapter->QuadroList); i++) {
          Slave = IoAdapter->QuadroList->QuadroAdapter[i];
          if (Slave != 0 && Slave != IoAdapter) {
            Slave->features                = IoAdapter->features;
            Slave->a.protocol_capabilities = IoAdapter->features;
            DBG_TRC(("Set raw protocol features: %08x for Adapter %d",
                Slave->ANum, Slave->features))
          }
        }
        ret = 0;
      }
      break;


		case DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_CONTROL:
      if (a->xdi_adapter.clock_control_Fnc != 0) {
				diva_xdi_clock_control_command_t clock_control_cmd;
				int start = (clock_control_cmd.command = (cmd->command_data.clock_interrupt_control.command != 0));

				if (start != 0) {
					if (diva_os_initialize_soft_isr (&a->clock_interrupt_soft_isr,
																					 diva_xdi_clock_soft_isr,
																					 &a->xdi_adapter) == 0) {
					clock_control_cmd.data.on.isr_callback         = diva_xdi_clock_isr_proc;
						clock_control_cmd.data.on.isr_callback_context = a;
					clock_control_cmd.data.on.port = (cmd->command_data.clock_interrupt_control.command - 1);

						DBG_LOG(("A(%d) start clock %s",
											a->xdi_adapter.ANum, clock_control_cmd.command != 0 ? "on" : "off"))
						ret = (*(a->xdi_adapter.clock_control_Fnc))(&a->xdi_adapter, &clock_control_cmd);
						DBG_LOG(("A(%d) clock %s complete",
											a->xdi_adapter.ANum, clock_control_cmd.command != 0 ? "on" : "off"))
						if (ret != 0) {
							diva_os_cancel_soft_isr (&a->clock_interrupt_soft_isr);
							diva_os_remove_soft_isr (&a->clock_interrupt_soft_isr);
						}
				}
				} else {
					DBG_LOG(("A(%d) start clock %s",
										a->xdi_adapter.ANum, clock_control_cmd.command != 0 ? "on" : "off"))
				ret = (*(a->xdi_adapter.clock_control_Fnc))(&a->xdi_adapter, &clock_control_cmd);
					DBG_LOG(("A(%d) clock %s complete",
										a->xdi_adapter.ANum, clock_control_cmd.command != 0 ? "on" : "off"))
					if (ret == 0) {
						diva_os_cancel_soft_isr (&a->clock_interrupt_soft_isr);
						diva_os_remove_soft_isr (&a->clock_interrupt_soft_isr);
					}
				}
			}
			break;

    case DIVA_XDI_UM_CMD_ALLOC_DMA_DESCRIPTOR:
      if (a->xdi_adapter.dma_map) {
        a->xdi_mbox.data_length = sizeof(diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t);
        if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length))) {
          diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t* p = 
            (diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t*)a->xdi_mbox.data;
          int nr = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)a->xdi_adapter.dma_map);
          unsigned long dma_magic, dma_magic_hi;
          void* local_addr;

          if (nr >= 0) {
            diva_get_dma_map_entry ((struct _diva_dma_map_entry*)a->xdi_adapter.dma_map,
                                    nr,
                                    &local_addr, &dma_magic);
						diva_get_dma_map_entry_hi ((struct _diva_dma_map_entry*)a->xdi_adapter.dma_map,
																			 nr,
																			 &dma_magic_hi);
            p->nr   = (dword)nr;
            p->low  = (dword)dma_magic;
            p->high = (dword)dma_magic_hi;
          } else {
            p->nr   = 0xffffffff;
            p->high = 0;
            p->low  = 0;
          }
          a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
          ret = 0;
        }
      }
      break;

    case DIVA_XDI_UM_CMD_INIT_VIDI:
      if (diva_init_vidi (&a->xdi_adapter) == 0) {
        PISDN_ADAPTER IoAdapter = &a->xdi_adapter;

        a->xdi_mbox.data_length = sizeof(diva_xdi_um_cfg_cmd_data_init_vidi_t);
        if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length)) != 0) {
          diva_xdi_um_cfg_cmd_data_init_vidi_t* vidi_data =
                              (diva_xdi_um_cfg_cmd_data_init_vidi_t*)a->xdi_mbox.data;

          vidi_data->req_magic_lo = IoAdapter->host_vidi.req_buffer_base_dma_magic;
          vidi_data->req_magic_hi = IoAdapter->host_vidi.req_buffer_base_dma_magic_hi;
          vidi_data->ind_magic_lo = IoAdapter->host_vidi.ind_buffer_base_dma_magic;
          vidi_data->ind_magic_hi = IoAdapter->host_vidi.ind_buffer_base_dma_magic_hi;
          vidi_data->dma_segment_length    = IoAdapter->host_vidi.dma_segment_length;
          vidi_data->dma_req_buffer_length = IoAdapter->host_vidi.dma_req_buffer_length;
          vidi_data->dma_ind_buffer_length = IoAdapter->host_vidi.dma_ind_buffer_length;
          vidi_data->dma_ind_remote_counter_offset = IoAdapter->host_vidi.dma_ind_buffer_length;

          a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
          ret = 0;
        }
      }
      break;

		case DIVA_XDI_UM_CMD_SET_VIDI_MODE:
			if (a->xdi_adapter.ControllerNumber == 0 && a->xdi_adapter.Initialized == 0 &&
					a->xdi_adapter.host_vidi.remote_indication_counter == 0 &&
					(cmd->command_data.vidi_mode.vidi_mode == 0 || a->xdi_adapter.dma_map != 0)) {
				PISDN_ADAPTER IoAdapter = &a->xdi_adapter, Slave;
				int i;

				DBG_LOG(("A(%d) vidi %s", IoAdapter->ANum,
								cmd->command_data.vidi_mode.vidi_mode != 0 ? "on" : "off"))

				for (i = 0; ((i < IoAdapter->tasks) && IoAdapter->QuadroList); i++) {
					Slave = IoAdapter->QuadroList->QuadroAdapter[i];
					Slave->host_vidi.vidi_active = cmd->command_data.vidi_mode.vidi_mode != 0;
				}
				prepare_4pri_functions (IoAdapter);

				ret = 0;
			}
			break;

		default:
			DBG_ERR(("A: A(%d) invalid cmd=%d", a->controller, cmd->command))
			break;
	}

	return (ret);
}

static int diva_4pri_reset_adapter (struct _diva_os_xdi_adapter* a) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
  PISDN_ADAPTER Slave;
	byte Bus, Slot;
	void* hdev;
	byte irq;
	int i;

	if (!IoAdapter->Address || !IoAdapter->reset) {
		return (-1);
	}
	if (IoAdapter->Initialized != 0) {
		DBG_ERR(("A: A(%d) can't reset %s adapter - please stop first",
						IoAdapter->ANum, a->xdi_adapter.Properties.Name))
		return (-1);
	}
	IoAdapter->Initialized = 0;

	/*
		Reset erases interrupt setting from the
		PCI configuration space
		*/
	Bus = a->resources.pci.bus;
	Slot = a->resources.pci.func;
	hdev = a->resources.pci.hdev;

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
		PCIread (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
		DBG_TRC(("SAVE IRQ LINE: %02x", irq))
	}

	(*(IoAdapter->rstFnc))(IoAdapter);

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) == 0) {
		PCIwrite (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
		PCIread  (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
		DBG_TRC(("RESTORED IRQ LINE: %02x", irq))
	}

  for (i = 0; ((i < IoAdapter->tasks) && IoAdapter->QuadroList); i++) {
		Slave = IoAdapter->QuadroList->QuadroAdapter[i];
		Slave->e_count =  0;
		if (Slave->e_tbl) {
			memset (Slave->e_tbl, 0x00, Slave->e_max * sizeof(E_INFO));
		}
		Slave->head	= 0;
		Slave->tail = 0;
		Slave->assign = 0;
		Slave->trapped = 0;

		memset (&Slave->a.IdTable[0],              0x00, sizeof(Slave->a.IdTable));
		memset (&Slave->a.IdTypeTable[0],          0x00, sizeof(Slave->a.IdTypeTable));
		memset (&Slave->a.FlowControlIdTable[0],   0x00, sizeof(Slave->a.FlowControlIdTable));
		memset (&Slave->a.FlowControlSkipTable[0], 0x00, sizeof(Slave->a.FlowControlSkipTable));
		memset (&Slave->a.misc_flags_table[0],     0x00, sizeof(Slave->a.misc_flags_table));
		memset (&Slave->a.rx_stream[0],            0x00, sizeof(Slave->a.rx_stream));
		memset (&Slave->a.tx_stream[0],            0x00, sizeof(Slave->a.tx_stream));

    diva_cleanup_vidi (Slave);

		if (Slave->dma_map) {
      diva_reset_dma_mapping (Slave->dma_map);
		}
		if (Slave->dma_map2) {
			diva_reset_dma_mapping (Slave->dma_map2);
		}
	}

	return (0);
}

static int diva_4pri_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
	int ret;

	diva_xdiLoadFileFile = data;
	diva_xdiLoadFileLength = length;

	ret = pri_4pri_FPGA_download (IoAdapter);

	diva_xdiLoadFileFile = 0;
	diva_xdiLoadFileLength = 0;

	return (ret ? 0 : -1);
}

void diva_os_prepare_4pri_functions (PISDN_ADAPTER IoAdapter) {
}

static void diva_4pri_clear_interrupts (diva_os_xdi_adapter_t* a) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;

	/*
		clear any pending interrupt
		*/
	IoAdapter->disIrq (IoAdapter) ;

	IoAdapter->tst_irq (&IoAdapter->a) ;
	IoAdapter->clr_irq (&IoAdapter->a) ;
	IoAdapter->tst_irq (&IoAdapter->a) ;
}


static int diva_4pri_stop_adapter_w_io (struct _diva_os_xdi_adapter* a, int do_io) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
	int i;

	if (!IoAdapter->ram) {
		return (-1);
	}
	if (!IoAdapter->Initialized) {
		DBG_ERR(("A: A(%d) can not stop 4PRI adapter - not running", IoAdapter->ANum))
		return (-1); /* nothing to stop */
	}

	if (do_io != 0) {
		/*
			Stop Adapter
			*/
		if (IoAdapter->host_vidi.vidi_active == 0) {
			for (i = 0; i < IoAdapter->tasks; i++) {
				IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 0;
			}
			a->clear_interrupts_proc = diva_4pri_clear_interrupts;
		}
		IoAdapter->stop (IoAdapter) ;
		if (a->clear_interrupts_proc) {
			diva_4pri_clear_interrupts (a);
			a->clear_interrupts_proc = 0;
		}
	}
	for (i = 0; i < IoAdapter->tasks; i++) {
		IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 0;
	}

	diva_os_cancel_soft_isr (&a->xdi_adapter.isr_soft_isr);
	diva_os_cancel_soft_isr (&a->xdi_adapter.req_soft_isr);

	/*
		Disconnect Adapters from DIDD
		*/
	for (i = 0; i < IoAdapter->tasks; i++) {
		diva_xdi_didd_remove_adapter(IoAdapter->QuadroList->QuadroAdapter[i]->ANum);
	}

	diva_os_cancel_soft_isr (&a->xdi_adapter.isr_soft_isr);
	diva_os_cancel_soft_isr (&a->xdi_adapter.req_soft_isr);

	IoAdapter->a.ReadyInt = 0;

	return (0);
}

static int diva_4pri_stop_adapter (struct _diva_os_xdi_adapter* a) {
	return (diva_4pri_stop_adapter_w_io (a, 1));
}

static int diva_4pri_stop_no_io (diva_os_xdi_adapter_t* a) {
	if (a->xdi_adapter.ControllerNumber != 0)
		return (-1);

	if (a->xdi_adapter.Initialized != 0)
		return (diva_4pri_stop_adapter_w_io (a, 0));

	return (0);
}

static int vidi_diva_4pri_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int i, adapter, adapter_started;

	if (IoAdapter->host_vidi.vidi_active == 0 || IoAdapter->host_vidi.remote_indication_counter == 0) {
		DBG_ERR(("A(%d) vidi not initialized", IoAdapter->ANum))
		return (-1);
	}
	DBG_LOG(("A(%d) vidi start", IoAdapter->ANum))

	/*
		Allow interrupts. VIDI reports adapter start up using message.
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL]  =
																*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_ENABLE;
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |= PLX9054_L2PDBELL_INT_ENABLE;

	/*
		Activate DPC
		*/
	for (i = 0; i < IoAdapter->tasks; i++) {
		IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 1;
		IoAdapter->QuadroList->QuadroAdapter[i]->host_vidi.vidi_started = 0;
	}

	start_4pri_hardware (IoAdapter);

	for (i = 0; i < 2000; i++) {
		diva_os_sleep (10);

		for (adapter = 0, adapter_started = 0; adapter < IoAdapter->tasks; adapter++)
			adapter_started += (IoAdapter->QuadroList->QuadroAdapter[adapter]->host_vidi.vidi_started != 0);

		if (adapter_started == IoAdapter->tasks)
			return (i);
	}

	/*
		Adapter start failed, de-activate DPC
		*/
	for (i = 0; i < IoAdapter->tasks; i++) {
		IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 0;
	}

	/*
		De-activate interrupts
		*/
	IoAdapter->disIrq (IoAdapter) ;

	return (-1);
}

static int idi_diva_4pri_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int i;

	DBG_LOG(("A(%d) idi start", IoAdapter->ANum))

	/*
		Start adapter
		*/
	start_4pri_hardware (IoAdapter);

  for (i = 0; i < 2000; ++i) {
    diva_os_wait (10);
    if (*(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_SIGNATURE) == DIVA_PRI_V3_BOOT_SIGNATURE_OK) {

			/*
				Allow interruptes
				*/
			*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL]  =
																	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
			*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_ENABLE;
			*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |= PLX9054_L2PDBELL_INT_ENABLE;
			return (i);
    }
  }

	return (-1);
}

static int diva_4pri_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int adapter_status, i;

  if (IoAdapter->Initialized != 0) {
    DBG_ERR(("A(%d) adapter already running", IoAdapter->ANum))
    return (-1);
  }
  if (IoAdapter->Address == 0) {
    DBG_ERR(("A(%d) adapter not mapped", IoAdapter->ANum))
    return (-1);
  }

	if (IoAdapter->host_vidi.vidi_active != 0) {
		adapter_status = vidi_diva_4pri_start_adapter (IoAdapter, features);
	} else {
		adapter_status = idi_diva_4pri_start_adapter (IoAdapter, features);
	}

	if (adapter_status >= 0) {
		DBG_LOG(("A(%d) Protocol startup time %d.%02d seconds",
							IoAdapter->ANum, (adapter_status / 100), (adapter_status % 100)))

		for (i = 1; i < IoAdapter->tasks; i++) {
			IoAdapter->QuadroList->QuadroAdapter[i]->features                = IoAdapter->features;
			IoAdapter->QuadroList->QuadroAdapter[i]->a.protocol_capabilities = IoAdapter->features;
			memcpy (IoAdapter->QuadroList->QuadroAdapter[i]->ProtocolIdString,
			        IoAdapter->ProtocolIdString, sizeof(IoAdapter->ProtocolIdString));
		}

		for (i = 0; i < IoAdapter->tasks; i++) {
			IoAdapter->QuadroList->QuadroAdapter[i]->Initialized         = 1;
			IoAdapter->QuadroList->QuadroAdapter[i]->IrqCount            = 0;
			IoAdapter->QuadroList->QuadroAdapter[i]->Properties.Features = (word)features;
			sprintf (IoAdapter->QuadroList->QuadroAdapter[i]->Name, "A(%d)", (int)IoAdapter->ANum);
		}

		/*
			Wait until protocol code started
			*/
		diva_os_sleep(100);

		/*
			Show adapter features
			*/
		diva_xdi_display_adapter_features (IoAdapter->ANum);

		for (i = 0; i < IoAdapter->tasks; i++) {
			DBG_LOG(("A(%d) %s adapter successfull started",
								IoAdapter->QuadroList->QuadroAdapter[i]->ANum,
								IoAdapter->QuadroList->QuadroAdapter[i]->Properties.Name))
			diva_xdi_didd_register_adapter (IoAdapter->QuadroList->QuadroAdapter[i]->ANum);
		}
	} else {
    DBG_ERR(("A(%d) Adapter start failed Signature=0x%08lx, TrapId=%08lx, boot count=%08lx",
              IoAdapter->ANum,
              *(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_SIGNATURE),
              *(volatile dword*)(IoAdapter->Address + 0x80),
              *(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_COUNT)))
    IoAdapter->stop(IoAdapter);
    for (i = 0; i < IoAdapter->tasks; i++) {
      DBG_ERR(("-----------------------------------------------------------------"))
      DBG_ERR(("XLOG recovery for adapter %d %s (%p)",
                IoAdapter->QuadroList->QuadroAdapter[i]->ANum,
                IoAdapter->QuadroList->QuadroAdapter[i]->Properties.Name,
                IoAdapter->QuadroList->QuadroAdapter[i]->ram))
      (*(IoAdapter->QuadroList->QuadroAdapter[i]->trapFnc))(IoAdapter->QuadroList->QuadroAdapter[i]);
      DBG_ERR(("-----------------------------------------------------------------"))
    }
	}

	return ((adapter_status >= 0) ? 0 : -1);
}

static int diva_4pri_write_sdram_block (PISDN_ADAPTER IoAdapter,
																		 		dword address,
																		 		const byte* data,
																		 		dword length,
																		 		dword limit) {

  if (((address + length) >= limit) || !IoAdapter->Address) {
    DBG_ERR(("A(%d) %s write address=0x%08x",
            IoAdapter->ANum,
            IoAdapter->Properties.Name,
            address+length))
    return (-1);
  }

	if (diva_dma_write_sdram_block (IoAdapter, 1, 1, address, data, length) != 0) {
		diva_4pri_memcpy (IoAdapter, IoAdapter->Address, address, data, length);
	}

	return (0);
}
