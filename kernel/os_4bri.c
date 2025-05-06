
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
#include <linux/stdarg.h>
#include "debuglib.h"
#include "cardtype.h"
#include "dlist.h"
#include "pc.h"
#include "pr_pc.h"
#include "di_defs.h"
#include "dsp_defs.h"
#include "di.h"
#include "vidi_di.h"
#include "io.h"

#include "xdi_msg.h"
#include "xdi_adapter.h"
#include "os_4bri.h"
#include "diva_pci.h"
#include "mi_pc.h"
#include "dsrv4bri.h"
#include "divatest.h" /* Adapter test framework */

void* diva_xdiLoadFileFile   = 0;
dword diva_xdiLoadFileLength = 0;

/*
**  IMPORTS
*/
extern void prepare_qBri_functions (PISDN_ADAPTER IoAdapter);
extern void prepare_qBri2_functions (PISDN_ADAPTER IoAdapter);
extern void diva_xdi_display_adapter_features (int card);
extern void diva_add_slave_adapter (diva_os_xdi_adapter_t* a);

extern int qBri_FPGA_download (PISDN_ADAPTER IoAdapter);

extern int diva_card_read_xlog (diva_os_xdi_adapter_t* a);

/*
**  LOCALS
*/
static unsigned long _4bri_bar_length[4] = {
  0x100,
  0x100, /* I/O */
  MQ_MEMORY_SIZE,
  0x2000
};
static unsigned long _4bri_v2_bar_length[4] = {
  0x100,
  0x100, /* I/O */
  MQ2_MEMORY_SIZE,
  0x10000
};
static unsigned long _4bri_v2_bri_bar_length[4] = {
  0x100,
  0x100, /* I/O */
  BRI2_MEMORY_SIZE,
  0x10000
};


static int diva_4bri_cleanup_adapter (diva_os_xdi_adapter_t* a);
static int diva_4bri_stop_no_io (diva_os_xdi_adapter_t* a);
static int diva_4bri_cmd_card_proc (struct _diva_os_xdi_adapter* a,
                                    diva_xdi_um_cfg_cmd_t* cmd,
                                    int length);
static int diva_4bri_cleanup_slave_adapters (diva_os_xdi_adapter_t* a);
static int diva_4bri_write_fpga_image (diva_os_xdi_adapter_t* a,
                                       byte* data,
                                       dword length);
static int diva_4bri_reset_adapter (PISDN_ADAPTER IoAdapter);
static int diva_4bri_write_sdram_block (PISDN_ADAPTER IoAdapter,
                                        dword address,
                                        const byte* data,
                                        dword length,
                                        dword limit);
static int diva_4bri_start_adapter (PISDN_ADAPTER IoAdapter,
                                    dword start_address,
                                    dword features);
static void check_qBri_interrupt (PISDN_ADAPTER IoAdapter);
static int diva_4bri_stop_adapter (diva_os_xdi_adapter_t* a);

static int
_4bri_is_rev_2_card (int card_ordinal)
{
  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_B_2F_PCI:
    case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
    case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      return (1);
  }
  return (0);
}

static int _4bri_is_pcie_card (int card_ordinal) {
  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      return (1);
  }
  return (0);
}

static int
_4bri_is_rev_2_bri_card (int card_ordinal) {
  switch (card_ordinal) {
    case CARDTYPE_DIVASRV_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_B_2F_PCI:
    case CARDTYPE_DIVASRV_BRI_CTI_V2_PCI:
    case CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_V_B_2M_V2_PCI:
    case CARDTYPE_DIVASRV_BRI_V1_PCIE:
    case CARDTYPE_DIVASRV_BRI_V1_V_PCIE:
      return (1);
  }
  return (0);
}

/*
**  BAR0 - MEM - 0x100    - CONFIG MEM
**  BAR1 - I/O - 0x100    - UNUSED
**  BAR2 - MEM - MQ_MEMORY_SIZE (MQ2_MEMORY_SIZE on Rev.2) - SDRAM
**  BAR3 - MEM - 0x2000 (0x10000 on Rev.2)   - CNTRL
**
**  Called by master adapter, that will initialize and add slave adapters
*/
int
diva_4bri_init_card (diva_os_xdi_adapter_t* a)
{
  int bar, i;
  PADAPTER_LIST_ENTRY quadro_list;
  diva_os_xdi_adapter_t* diva_current;
  diva_os_xdi_adapter_t* adapter_list[4];
  PISDN_ADAPTER Slave;
  dword offset;
  unsigned long bar_length[sizeof(_4bri_bar_length)/sizeof(_4bri_bar_length[0])];
  int v2 = _4bri_is_rev_2_card (a->CardOrdinal), pcie = _4bri_is_pcie_card (a->CardOrdinal);
  int tasks = _4bri_is_rev_2_bri_card (a->CardOrdinal) ? 1 : MQ_INSTANCE_COUNT;
  int factor = (tasks == 1) ? 1 : 2;

#if !defined(DIVA_INCLUDE_DISCONTINUED_HARDWARE)
	if (v2 == 0) {
		return (-1);
	}
#endif


  /*
    Fix up card ordinal to get righ adapter name and features
    */
  if (v2 != 0 && pcie == 0) {
    byte Bus, Slot;
    void* hdev;
    word id = 0;

    Bus = a->resources.pci.bus;
    Slot = a->resources.pci.func;
    hdev = a->resources.pci.hdev;

    PCIread (Bus, Slot, 0x2e, &id, sizeof(id), hdev) ;
    DBG_LOG(("A(%d) read Subsystem ID %04x", a->xdi_adapter.ANum, id))

		if (a->CardOrdinal == CARDTYPE_DIVASRV_Q_8M_V2_PCI) {
			switch (id) {
				case 0x1300:
					a->CardOrdinal = CARDTYPE_DIVASRV_V_Q_8M_V2_PCI;
					break;
			}
		} else if (a->CardOrdinal == CARDTYPE_DIVASRV_B_2M_V2_PCI) {
			switch (id) {
				case 0x1800:
					a->CardOrdinal = CARDTYPE_DIVASRV_V_B_2M_V2_PCI;
					break;
			}
		}
  }

  if (v2) {
    if (_4bri_is_rev_2_bri_card (a->CardOrdinal)) {
      memcpy (bar_length, _4bri_v2_bri_bar_length, sizeof(bar_length));
    } else {
      memcpy (bar_length, _4bri_v2_bar_length, sizeof(bar_length));
    }
  } else {
    memcpy (bar_length, _4bri_bar_length, sizeof(bar_length));
  }
  DBG_TRC(("SDRAM_LENGTH=%08x, tasks=%d, factor=%d",
           bar_length[2], tasks, factor))

	a->xdi_adapter.BusNumber  = a->resources.pci.bus;
	a->xdi_adapter.slotNumber = a->resources.pci.func;
	a->xdi_adapter.hdev       = a->resources.pci.hdev;

  /*
    Get Serial Number and additional info
    The serial number of 4BRI is accessible in accordance with PCI spec
    via command register located in configuration space, also we do not
    have to map any BAR before we can access it
  */
	if (!diva_get_serial_number (&a->xdi_adapter)) {
		DBG_ERR(("Failed to read serial number"))
		if (pcie == 0) {
			diva_4bri_cleanup_adapter (a);
			return (-1);
		} else {
			a->xdi_adapter.serialNo = (dword)a->controller |
																((dword)a->resources.pci.func << 8) |
																((dword)a->resources.pci.bus << 16);
		}
	}

  /*
    Set properties
  */
  a->xdi_adapter.cardType   = a->CardOrdinal;
  a->xdi_adapter.Properties = CardProperties[a->CardOrdinal];
  DBG_LOG(("Load %s, SN:%ld, bus:%02x, func:%02x",
            a->xdi_adapter.Properties.Name,
            a->xdi_adapter.serialNo,
            a->resources.pci.bus,
            a->resources.pci.func))

  /*
    First initialization step: get and check hardware resoures.
    Do not map resources and do not access card at this step
  */
  for (bar = 0; bar < 4; bar++) {
    a->resources.pci.bar[bar] = divasa_get_pci_bar (a->resources.pci.bus,
                                                    a->resources.pci.func,
                                                    bar,
                                                    a->resources.pci.hdev);
    if (!a->resources.pci.bar[bar] ||
        (a->resources.pci.bar[bar] == 0xFFFFFFF0)) {
			if (bar == 1) {
				a->resources.pci.bar[bar] = 0;
			} else {
				DBG_ERR(("A: invalid bar[%d]=%08x", bar, a->resources.pci.bar[bar]))
				return (-1);
			}
    }
  }
  a->resources.pci.irq = divasa_get_pci_irq (a->resources.pci.bus,
                                             a->resources.pci.func,
                                             a->resources.pci.hdev);
  if (a->resources.pci.irq == 0) {
    DBG_ERR(("failed to read irq"));
    return (-1);
  }

  a->xdi_adapter.sdram_bar = a->resources.pci.bar [2];

  /*
    Map all MEMORY BAR's
  */
  for (bar = 0; bar < 4; bar++) {
    if (bar != 1) { /* ignore I/O */
      a->resources.pci.addr[bar] = divasa_remap_pci_bar (a, a->resources.pci.bar[bar],
                                                         bar_length[bar]);
      if (!a->resources.pci.addr[bar]) {
        DBG_ERR(("A: can't map bar[%d]", bar))
        diva_4bri_cleanup_adapter (a);
        return (-1);
      }
    }
  }

  /*
    Register I/O port
  */
  sprintf (&a->port_name[0], "DIVA BRI %ld", (long)a->xdi_adapter.serialNo);

  if (a->resources.pci.bar[1] != 0 &&
			diva_os_register_io_port (a, 1, a->resources.pci.bar[1],
                                bar_length[1],
                                &a->port_name[0])) {
    DBG_ERR(("A: can't register bar[1]"))
    diva_4bri_cleanup_adapter (a);
    return (-1);
  }

  a->resources.pci.addr[1] = (void*)(unsigned long)a->resources.pci.bar[1];

  /*
    Set cleanup pointer for base adapter only, so slave adapter
    will be unable to get cleanup
  */
  a->interface.cleanup_adapter_proc = diva_4bri_cleanup_adapter;
  a->interface.stop_no_io_proc      = diva_4bri_stop_no_io;

  /*
    Create slave adapters
  */
  if (tasks > 1) {
    if (!(a->slave_adapters[0] = (diva_os_xdi_adapter_t*)diva_os_malloc (0, sizeof(*a)))) {
      diva_4bri_cleanup_adapter (a);
      return (-1);
    }
    if (!(a->slave_adapters[1] = (diva_os_xdi_adapter_t*)diva_os_malloc (0, sizeof(*a)))) {
      diva_os_free (0, a->slave_adapters[0]);
      a->slave_adapters[0] = 0;
      diva_4bri_cleanup_adapter (a);
      return (-1);
    }
    if (!(a->slave_adapters[2] = (diva_os_xdi_adapter_t*)diva_os_malloc (0, sizeof(*a)))) {
      diva_os_free (0, a->slave_adapters[0]);
      diva_os_free (0, a->slave_adapters[1]);
      a->slave_adapters[0] = 0;
      a->slave_adapters[1] = 0;
      diva_4bri_cleanup_adapter (a);
      return (-1);
    }
    memset (a->slave_adapters[0], 0x00, sizeof(*a));
    memset (a->slave_adapters[1], 0x00, sizeof(*a));
    memset (a->slave_adapters[2], 0x00, sizeof(*a));
  }

  adapter_list[0] = a;
  adapter_list[1] = a->slave_adapters[0];
  adapter_list[2] = a->slave_adapters[1];
  adapter_list[3] = a->slave_adapters[2];

  /*
    Allocate slave list
  */
  quadro_list = (PADAPTER_LIST_ENTRY)diva_os_malloc (0, sizeof(*quadro_list));
  if (!(a->slave_list = quadro_list)) {
    for (i = 0; i < (tasks-1); i++) {
      diva_os_free (0, a->slave_adapters[i]);
      a->slave_adapters[i] = 0;
    }
    diva_4bri_cleanup_adapter (a);
    return (-1);
  }
  memset (quadro_list, 0x00, sizeof(*quadro_list));

  /*
    Set interfaces
  */
  a->xdi_adapter.QuadroList = quadro_list;
  for (i = 0; i < tasks; i++) {
    adapter_list[i]->xdi_adapter.reentrant        = 0;
    adapter_list[i]->xdi_adapter.ControllerNumber = i;
    adapter_list[i]->xdi_adapter.tasks = tasks;
    quadro_list->QuadroAdapter[i] = &adapter_list[i]->xdi_adapter;
  }

  for (i = 0; i < tasks; i++) {
    diva_current = adapter_list[i];

    diva_current->dsp_mask = 0x00000003;

    diva_current->xdi_adapter.a.io = &diva_current->xdi_adapter;
    diva_current->xdi_adapter.DIRequest = request;
    diva_current->interface.cmd_proc = diva_4bri_cmd_card_proc;
    diva_current->xdi_adapter.Properties = CardProperties[a->CardOrdinal];
    diva_current->CardOrdinal = a->CardOrdinal;

    diva_current->xdi_adapter.Channels = CardProperties[a->CardOrdinal].Channels;
    diva_current->xdi_adapter.e_max = CardProperties[a->CardOrdinal].E_info;
    diva_current->xdi_adapter.e_tbl = diva_os_malloc (0, diva_current->xdi_adapter.e_max * sizeof(E_INFO));

    if (!diva_current->xdi_adapter.e_tbl) {
      diva_4bri_cleanup_slave_adapters (a);
      diva_4bri_cleanup_adapter (a);
      for (i = 1; i < tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }
    memset (diva_current->xdi_adapter.e_tbl, 0x00, diva_current->xdi_adapter.e_max * sizeof(E_INFO));

    if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.isr_spin_lock, "isr")) {
      diva_4bri_cleanup_slave_adapters (a);
      diva_4bri_cleanup_adapter (a);
      for (i = 1; i < tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }
    if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.data_spin_lock, "data")) {
      diva_4bri_cleanup_slave_adapters (a);
      diva_4bri_cleanup_adapter (a);
      for (i = 1; i < tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }

    strcpy (diva_current->xdi_adapter.req_soft_isr.dpc_thread_name, "kdivas4brid");

    if (diva_os_initialize_soft_isr (&diva_current->xdi_adapter.req_soft_isr,
                                     DIDpcRoutine,
                                     &diva_current->xdi_adapter)) {
      diva_4bri_cleanup_slave_adapters (a);
      diva_4bri_cleanup_adapter (a);
      for (i = 1; i < tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }

    /*
     Do not initialize second DPC - only one thread will be created
    */
    diva_current->xdi_adapter.isr_soft_isr.object = diva_current->xdi_adapter.req_soft_isr.object;
  }

  if (v2) {
       prepare_qBri2_functions (&a->xdi_adapter);
  } else {
       prepare_qBri_functions (&a->xdi_adapter);
  }

  /*
    Set up hardware related pointers
  */
  a->xdi_adapter.cfg = (void*)(unsigned long)a->resources.pci.bar[0]; /* BAR0 CONFIG */
  a->xdi_adapter.port = (void*)(unsigned long)a->resources.pci.bar[1]; /* BAR1        */
  a->xdi_adapter.Address = a->resources.pci.addr[2];        /* BAR2 SDRAM  */
  a->xdi_adapter.ctlReg = (void*)(unsigned long)a->resources.pci.bar[3]; /* BAR3 CNTRL  */

  a->xdi_adapter.reset = a->resources.pci.addr[0]; /* BAR0 CONFIG */
  a->xdi_adapter.ram = a->resources.pci.addr[2]; /* BAR2 SDRAM  */

  a->xdi_adapter.AdapterTestMemoryStart  = a->resources.pci.addr[2]; /* BAR2 SDRAM  */
  a->xdi_adapter.AdapterTestMemoryLength = bar_length[2],

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
  a->xdi_adapter.reset[PLX9054_INTCSR] = 0x00 ;	/* disable PCI interrupts */

  /*
    Replicate addresses to all instances, set shared memory
    address for all instances
  */
  for (i = 0; i < tasks; i++) {
    Slave = a->xdi_adapter.QuadroList->QuadroAdapter[i];
    offset = Slave->ControllerNumber * (a->xdi_adapter.MemorySize>>factor);
    Slave->Address = &a->xdi_adapter.Address[offset];
    Slave->ram	= &a->xdi_adapter.ram[offset];
    Slave->reset = a->xdi_adapter.reset;
    Slave->ctlReg = a->xdi_adapter.ctlReg;
    Slave->prom	= a->xdi_adapter.prom;
    Slave->reset = a->xdi_adapter.reset;
    Slave->sdram_bar = a->xdi_adapter.sdram_bar;
    diva_init_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry**)&Slave->dma_map, (2/*Ch*/+2/*Sig*/+2/*NULLPlci*/)*2/*Rx+Tx*/ + 2/*RxTxBuffers*/ + 1/*MAN*/ + 6/*LI*/);
  }
  for (i = 0; i < tasks; i++) {
    Slave = a->xdi_adapter.QuadroList->QuadroAdapter[i];
    Slave->ram += ((a->xdi_adapter.MemorySize >> factor) - MQ_SHARED_RAM_SIZE);
  }
  for (i = 1; i < tasks; i++) {
    Slave = a->xdi_adapter.QuadroList->QuadroAdapter[i];
    Slave->serialNo = ((dword)(Slave->ControllerNumber << 24)) | a->xdi_adapter.serialNo;
    Slave->cardType = a->xdi_adapter.cardType;
		Slave->software_options = a->xdi_adapter.software_options;
    memcpy (Slave->hw_info, a->xdi_adapter.hw_info, sizeof(a->xdi_adapter.hw_info));
  }

  /*
    Set IRQ handler
  */
  a->xdi_adapter.irq_info.irq_nr = a->resources.pci.irq;
  sprintf (a->xdi_adapter.irq_info.irq_name, "DIVA %s %ld",
           (tasks > 1) ? "4BRI" : "BRI",
           (long)a->xdi_adapter.serialNo);

  if (diva_os_register_irq (a, a->xdi_adapter.irq_info.irq_nr,
                            a->xdi_adapter.irq_info.irq_name, a->xdi_adapter.msi)) {
    diva_4bri_cleanup_slave_adapters (a);
    diva_4bri_cleanup_adapter (a);
    for (i = 1; i < tasks; i++) { diva_os_free (0, adapter_list[i]); }
    return (-1);
  }

  a->xdi_adapter.irq_info.registered = 1;

  /*
    Add three slave adapters
  */
  if (tasks > 1) {
    diva_add_slave_adapter (adapter_list[1]);
    diva_add_slave_adapter (adapter_list[2]);
    diva_add_slave_adapter (adapter_list[3]);
  }

  diva_log_info("%s IRQ:%u SerNo:%d", a->xdi_adapter.Properties.Name,
                                      a->resources.pci.irq,
                                      a->xdi_adapter.serialNo);

  return (0);
}

/*
**  Cleanup function will be called for master adapter only
**  this is garanteed by design: cleanup callback is set
**  by master adapter only
*/
static int
diva_4bri_cleanup_adapter (diva_os_xdi_adapter_t* a)
{
  int bar;

  /*
    Stop adapter if running
  */
  if (a->xdi_adapter.Initialized) {
    diva_4bri_stop_adapter (a);
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
  diva_4bri_cleanup_slave_adapters (a);

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
      _4bri_is_rev_2_card (a->CardOrdinal) ? _4bri_v2_bar_length[1] : _4bri_bar_length[1],
      &a->port_name[0]);
    a->resources.pci.bar[1]  = 0;
    a->resources.pci.addr[1] = 0;
  }

  if (a->slave_list) {
    diva_os_free (0, a->slave_list);
    a->slave_list = 0;
  }

  return (0);
}

/*
**  Release resources of slave adapters
*/
static int
diva_4bri_cleanup_slave_adapters (diva_os_xdi_adapter_t* a)
{
  diva_os_xdi_adapter_t* adapter_list[4];
  diva_os_xdi_adapter_t* diva_current;
  int i;

  adapter_list[0] = a;
  adapter_list[1] = a->slave_adapters[0];
  adapter_list[2] = a->slave_adapters[1];
  adapter_list[3] = a->slave_adapters[2];

  for (i = 0; i < a->xdi_adapter.tasks; i++) {
    diva_current = adapter_list[i];
    if (diva_current) {
      diva_os_cancel_soft_isr (&diva_current->xdi_adapter.req_soft_isr);
      diva_os_cancel_soft_isr (&diva_current->xdi_adapter.isr_soft_isr);

      diva_os_remove_soft_isr (&diva_current->xdi_adapter.req_soft_isr);
      diva_current->xdi_adapter.isr_soft_isr.object = 0;

      diva_os_destroy_spin_lock (&diva_current->xdi_adapter.isr_spin_lock, "unload");
      diva_os_destroy_spin_lock (&diva_current->xdi_adapter.data_spin_lock,"unload");

			diva_cleanup_vidi (&diva_current->xdi_adapter);

      diva_free_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry*)diva_current->xdi_adapter.dma_map);
      diva_current->xdi_adapter.dma_map = 0;

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

static int
diva_4bri_cmd_card_proc (struct _diva_os_xdi_adapter* a,
                         diva_xdi_um_cfg_cmd_t* cmd,
                         int length)
{
  int ret = -1;

  if (cmd->adapter != a->controller) {
    DBG_ERR(("A: 4bri_cmd, invalid controller=%d != %d",
                  cmd->adapter, a->controller))
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
      if (!a->xdi_adapter.ControllerNumber) {
        /*
          Only master adapter can access hardware config
        */
        a->xdi_mbox.data_length = 36;
        a->xdi_mbox.data = diva_os_malloc(0, a->xdi_mbox.data_length);
        if (a->xdi_mbox.data) {
          int i;
          void* const data = a->xdi_mbox.data;

          for (i = 0; i < 8; i++) {
            ((dword*) data)[i] = a->resources.pci.bar[i];
          }
          ((dword*) data)[8] = a->resources.pci.irq;

          a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
          ret = 0;
        }
      }
      break;

    case DIVA_XDI_UM_CMD_GET_CARD_STATE:
      if (!a->xdi_adapter.ControllerNumber) {
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
      if (!a->xdi_adapter.ControllerNumber) {
        ret = diva_4bri_write_fpga_image (a, (byte*)&cmd[1],
                                          cmd->command_data.write_fpga.image_length);
      }
      break;

    case DIVA_XDI_UM_CMD_RESET_ADAPTER:
      if (!a->xdi_adapter.ControllerNumber) {
        ret = diva_4bri_reset_adapter (&a->xdi_adapter);
      }
      break;

    case DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK:
      if (!a->xdi_adapter.ControllerNumber) {
        ret = diva_4bri_write_sdram_block (&a->xdi_adapter,
                                           cmd->command_data.write_sdram.offset,
                                           (byte*)&cmd[1],
                                           cmd->command_data.write_sdram.length,
                                           a->xdi_adapter.MemorySize);
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

    case DIVA_XDI_UM_CMD_START_ADAPTER:
      if (!a->xdi_adapter.ControllerNumber) {
        ret = diva_4bri_start_adapter (&a->xdi_adapter,
                                       cmd->command_data.start.offset,
                                       cmd->command_data.start.features);
      }
      break;

    case DIVA_XDI_UM_CMD_RAW_START_ADAPTER:
      if (!a->xdi_adapter.ControllerNumber)
      {
        start_qBri_hardware(&a->xdi_adapter);
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


		case DIVA_XDI_UM_CMD_SET_PROTOCOL_CODE_VERSION:
      if (a->xdi_adapter.ControllerNumber == 0 &&
					a->xdi_adapter.Initialized      == 0 &&
					a->xdi_adapter.trapped          == 0 &&
					a->xdi_adapter.ram              != 0) {
				PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
        int version = cmd->command_data.protocol_code_version.version != 0;
				int factor = (IoAdapter->tasks == 1) ? 1 : 2;
				dword offset, i;

				if (version != IoAdapter->reentrant) {
					IoAdapter->Address = a->resources.pci.addr[2]; /* BAR2 SDRAM  */
					IoAdapter->ram     = a->resources.pci.addr[2]; /* BAR2 SDRAM  */

					for (i = 0; i < (IoAdapter->tasks ? IoAdapter->tasks : 1); i++) {
						PISDN_ADAPTER Slave = IoAdapter->QuadroList->QuadroAdapter[i];
						DBG_TRC(("Set protocol code version to %s for Adapter %d",
											version ? "new" : "original", Slave->ANum))
						Slave->reentrant = (byte)version;

						if (version != 0) {
							offset = Slave->ControllerNumber * (MP_SHARED_RAM_OFFSET+MP_SHARED_RAM_SIZE);
						} else {
							offset = Slave->ControllerNumber * (a->xdi_adapter.MemorySize>>factor);
						}
						Slave->Address = &IoAdapter->Address[offset];
						Slave->ram     = &IoAdapter->ram[offset];
					}
					for (i = 0; i < (IoAdapter->tasks ? IoAdapter->tasks : 1); i++) {
						PISDN_ADAPTER Slave = IoAdapter->QuadroList->QuadroAdapter[i];
						if (version != 0) {
							Slave->ram += MP_SHARED_RAM_OFFSET;
						} else {
							Slave->ram += ((a->xdi_adapter.MemorySize >> factor) - MQ_SHARED_RAM_SIZE);
						}
					}
				}

        ret = 0;
			}
			break;

    case DIVA_XDI_UM_CMD_ADAPTER_TEST:
			if (a->xdi_adapter.ControllerNumber == 0 && a->xdi_adapter.DivaAdapterTestProc) {
				a->xdi_adapter.AdapterTestMask = cmd->command_data.test.test_command;

				if (DIVA_4BRI_PCIE(&a->xdi_adapter) != 0) {
					volatile dword* las0ba_ctrl = (volatile dword*)&a->xdi_adapter.reset[0x04];
					dword las0ba_original = *las0ba_ctrl;
					dword i;

					for (i = 0; i < ((64*1024*1024)/_4bri_v2_bar_length[2]); i++) {
						if (i != 0) {
							*las0ba_ctrl = (dword)(las0ba_original + i * _4bri_v2_bar_length[2]);
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
				} else {
					if ((*(a->xdi_adapter.DivaAdapterTestProc))(&a->xdi_adapter)) {
						ret = -1;
					} else {
						ret = 0;
					}
				}
				a->xdi_adapter.AdapterTestMask = 0;
			}
      break;

    case DIVA_XDI_UM_CMD_STOP_ADAPTER:
      if (!a->xdi_adapter.ControllerNumber) {
        ret = diva_4bri_stop_adapter (a);
      }
      break;

    case DIVA_XDI_UM_CMD_READ_XLOG_ENTRY:
      ret = diva_card_read_xlog (a);
      break;

    case DIVA_XDI_UM_CMD_READ_SDRAM:
      if (!a->xdi_adapter.ControllerNumber && a->xdi_adapter.Address) {
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
				prepare_qBri2_functions (IoAdapter);

				ret = 0;
			}
			break;

    default:
      DBG_ERR(("A: A(%d) invalid cmd=%d", a->controller, cmd->command))
  }

  return (ret);
}

void *
xdiLoadFile (char *FileName, dword *FileLength, dword lim)
{
  void* ret = diva_xdiLoadFileFile;

  if (FileLength) {
    *FileLength = diva_xdiLoadFileLength;
  }
  diva_xdiLoadFileFile	 = 0;
  diva_xdiLoadFileLength = 0;

  return (ret);
}

void
diva_os_set_qBri_functions (PISDN_ADAPTER IoAdapter)
{
}

void
diva_os_set_qBri2_functions (PISDN_ADAPTER IoAdapter)
{
}

static int
diva_4bri_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length)
{
  int ret;

  diva_xdiLoadFileFile = data;
  diva_xdiLoadFileLength = length;

  ret = qBri_FPGA_download (&a->xdi_adapter);

  diva_xdiLoadFileFile = 0;
  diva_xdiLoadFileLength = 0;

  return (ret ? 0 : -1);
}

static int
diva_4bri_reset_adapter (PISDN_ADAPTER IoAdapter)
{
  PISDN_ADAPTER Slave;
  int i;

  if (!IoAdapter->Address || !IoAdapter->reset) {
    return (-1);
  }
  if (IoAdapter->Initialized) {
    DBG_ERR(("A: can't reset adapter %d - please stop first", IoAdapter->ANum))
    return (-1);
  }
#if 0
  /*
    Reset erases interrupt setting from the
    PCI configuration space
  */
  (*(IoAdapter->rstFnc))(IoAdapter);
#endif

  /*
    Forget all entities on all adapters
  */
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
  }

  return (0);
}


static int
diva_4bri_write_sdram_block (PISDN_ADAPTER IoAdapter,
                             dword address,
                             const byte* data,
                             dword length,
                             dword limit)
{
  byte* mem = IoAdapter->Address;

  if (((address + length) >= limit) || !mem) {
    DBG_ERR(("A: A(%d) write BRI address=0x%08lx (%lu+%lu %lu)", IoAdapter->ANum, address+length, address, length, limit))
    return (-1);
  }

	/*
		Diva 4BRI uses two dma descriptors and both are already allocated at this moment.
		But descriptors are not used until adapter started. This is possible to use dma
    descriptors for download if only accessing the descriptor data without allocating
		the descriptor
		*/
	if (DIVA_4BRI_REVISION(IoAdapter) == 0 ||
			diva_dma_write_sdram_block (IoAdapter, 0, 1, address, data, length) != 0) {
		mem += address;

		while (length--) {
			*mem++ = *data++;
		}
	}

  return (0);
}

static int vidi_diva_4bri_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int i, adapter, adapter_started;

	if (IoAdapter->host_vidi.vidi_active == 0 || IoAdapter->host_vidi.remote_indication_counter == 0) {
		DBG_ERR(("A(%d) vidi not initialized", IoAdapter->ANum))
		return (-1);
	}
	DBG_LOG(("A(%d) vidi start", IoAdapter->ANum))

	/*
		Allow interrupts. VIDI reports adapter start up using message.
		*/
	IoAdapter->reset[PLX9054_INTCSR] = PLX9054_INT_ENABLE;

	/*
		Activate DPC
		*/
	for (i = 0; i < IoAdapter->tasks; i++) {
		IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 1;
		IoAdapter->QuadroList->QuadroAdapter[i]->host_vidi.vidi_started = 0;
	}

  /*
    start adapter
  */
  start_qBri_hardware (IoAdapter);

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

static int idi_diva_4bri_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
  volatile word *signature;
  int started = 0;
  int i;

  /*
    start adapter
  */
  start_qBri_hardware (IoAdapter);

  /*
    wait for signature in shared memory (max. 3 seconds)
  */
  signature = (volatile word *)(&IoAdapter->ram[0x1E]) ;

  for ( i = 0 ; i < 300 ; ++i ) {
    diva_os_wait (10) ;
    if (signature[0] == 0x4447) {
      started = i+1;
      break;
    }
  }

  if (started == 0) {
		IoAdapter->disIrq (IoAdapter) ;
    return (-1);
  }

  diva_os_sleep (200);

	check_qBri_interrupt (IoAdapter);

  return (started);
}

static int
diva_4bri_start_adapter (PISDN_ADAPTER IoAdapter,
                         dword start_address,
                         dword features) {
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
		adapter_status = vidi_diva_4bri_start_adapter (IoAdapter, features);
	} else {
		adapter_status = idi_diva_4bri_start_adapter (IoAdapter, features);
	}

	if (adapter_status >= 0) {
		DBG_LOG(("A(%d) Protocol startup time %d.%02d seconds",
							IoAdapter->ANum, (adapter_status / 100), (adapter_status % 100)))

  for (i = 1; i < IoAdapter->tasks; i++) {
    IoAdapter->QuadroList->QuadroAdapter[i]->features = IoAdapter->features;
			IoAdapter->QuadroList->QuadroAdapter[i]->a.protocol_capabilities = IoAdapter->features;
    memcpy (IoAdapter->QuadroList->QuadroAdapter[i]->ProtocolIdString,
            IoAdapter->ProtocolIdString, sizeof(IoAdapter->ProtocolIdString));
  }

  for (i = 0; i < IoAdapter->tasks; i++) {
    IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 1;
    IoAdapter->QuadroList->QuadroAdapter[i]->IrqCount = 0;
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

static void
check_qBri_interrupt (PISDN_ADAPTER IoAdapter)
{
  dword volatile *qBriIrq ;
  /*
    Reset on-board interrupt register
  */
  IoAdapter->IrqCount = 0 ;
  qBriIrq = (dword volatile *)(&IoAdapter->ctlReg[_4bri_is_rev_2_card (IoAdapter->cardType) ? (MQ2_BREG_IRQ_TEST) : (MQ_BREG_IRQ_TEST)]);

  *qBriIrq = MQ_IRQ_REQ_OFF ;

  IoAdapter->reset[PLX9054_INTCSR] = PLX9054_INT_ENABLE ;

  diva_os_wait(100);
}

static void
diva_4bri_clear_interrupts (diva_os_xdi_adapter_t* a)
{
  PISDN_ADAPTER IoAdapter = &a->xdi_adapter;

  /*
    clear any pending interrupt
  */
  IoAdapter->disIrq (IoAdapter) ;

  IoAdapter->tst_irq (&IoAdapter->a) ;
  IoAdapter->clr_irq (&IoAdapter->a) ;
  IoAdapter->tst_irq (&IoAdapter->a) ;
}

static int diva_4bri_stop_adapter_w_io (diva_os_xdi_adapter_t* a, int do_io) {
  PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
  int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  divas_pci_card_resources_t *p_pci = &(a->resources.pci);
#endif

  if (!IoAdapter->ram) {
    return (-1);
  }
  DBG_LOG(("%s Adapter: %d", __FUNCTION__, IoAdapter->ANum));

  if (!IoAdapter->Initialized) {
    DBG_ERR(("A: A(%d) can't stop adapter - not running", IoAdapter->ANum))
    return (-1); /* nothing to stop */
  }

	if (IoAdapter->host_vidi.vidi_active == 0) {
    for (i = 0; i < IoAdapter->tasks; i++) {
      IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 0;
    }

    /*
      Disconnect Adapters from DIDD
    */
    for (i = 0; i < IoAdapter->tasks; i++) {
      diva_xdi_didd_remove_adapter(IoAdapter->QuadroList->QuadroAdapter[i]->ANum);
    }
	}

	if (do_io != 0) {
		i = 100;

		/*
		 Stop interrupts
		*/
		if (IoAdapter->host_vidi.vidi_active == 0) {
			a->clear_interrupts_proc = diva_4bri_clear_interrupts;
			IoAdapter->a.ReadyInt = 1;
			IoAdapter->a.ram_inc (&IoAdapter->a, &PR_RAM->ReadyInt) ;
		} else {
			IoAdapter->stop (IoAdapter);
			a->clear_interrupts_proc = diva_4bri_clear_interrupts;
			i = 0;
		}
		do {
			diva_os_sleep (10);
		} while (i-- && a->clear_interrupts_proc);
		if (a->clear_interrupts_proc) {
			diva_4bri_clear_interrupts (a);
			a->clear_interrupts_proc = 0;
		}
		IoAdapter->a.ReadyInt = 0;
	}

  for (i = 0; i < IoAdapter->tasks; i++) {
		IoAdapter->QuadroList->QuadroAdapter[i]->Initialized = 0;
	}

  /*
    kill pending dpcs
  */
  diva_os_cancel_soft_isr (&IoAdapter->req_soft_isr);
  diva_os_cancel_soft_isr (&IoAdapter->isr_soft_isr);

	if (IoAdapter->host_vidi.vidi_active != 0) {
		/*
			Disconnect Adapters from DIDD
			*/
		for (i = 0; i < IoAdapter->tasks; i++) {
			diva_xdi_didd_remove_adapter(IoAdapter->QuadroList->QuadroAdapter[i]->ANum);
		}
	}

	if (do_io != 0 && IoAdapter->host_vidi.vidi_active == 0) {
		/*
			Stop and reset adapter
			*/
		IoAdapter->stop (IoAdapter) ;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  /*
    Unmap Clock Data DMA from DIDD
  */
  diva_xdi_didd_unmap_clock_data_addr(IoAdapter->ANum,
                                      p_pci->clock_data_bus_addr,
                                      p_pci->hdev);
#endif

  return (0);
}

static int diva_4bri_stop_adapter (diva_os_xdi_adapter_t* a) {
	return (diva_4bri_stop_adapter_w_io (a, 1));
}

static int diva_4bri_stop_no_io (diva_os_xdi_adapter_t* a) {
	if (a->xdi_adapter.ControllerNumber != 0)
		return (-1);

	if (a->xdi_adapter.Initialized != 0)
		return (diva_4bri_stop_adapter_w_io (a, 0));

	return (0);
}

// vim: set tabstop=2 softtabstop=2 shiftwidth=2 expandtab :
