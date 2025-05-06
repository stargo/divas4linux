
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
#include "divasync.h"
#include "io.h"

#include "xdi_msg.h"
#include "xdi_adapter.h"
#include "os_pri3.h"
#include "diva_pci.h"
#include "mi_pc.h"
#include "dsrv_pri.h"
#include "dsrv_pri3.h"
#include "dsrv4bri.h" /* PLX constants */
#include "passivp3.h"
#include "divatest.h" /* Adapter test framework */

/*
	IMPORTS
	*/
extern void* diva_xdiLoadFileFile;
extern dword diva_xdiLoadFileLength;
extern void prepare_pri_v3_functions (PISDN_ADAPTER IoAdapter);
extern int  pri_v3_FPGA_download     (PISDN_ADAPTER IoAdapter);
extern int diva_card_read_xlog (diva_os_xdi_adapter_t* a);
extern void diva_xdi_display_adapter_features (int card);
extern int diva_pri_v3_fpga_ready (PISDN_ADAPTER IoAdapter);

/*
**  LOCALS
*/
static unsigned long _pri_v3_bar_length[4] = {
  512,
  256, /* I/O */
  8*1024*1024,
  256*1024
};

static unsigned long _pri_v3_passive_bar_length[4] = {
  512,
  256, /* I/O */
  128*1024,
  256*1024
};

static int diva_pri_v3_cleanup_adapter (diva_os_xdi_adapter_t* a);
static int diva_pri_v3_stop_no_io (diva_os_xdi_adapter_t* a);
static int diva_pri_v3_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length);
static int diva_pri_v3_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length);
static int diva_pri_v3_write_sdram_block (PISDN_ADAPTER IoAdapter,
																			 		dword address,
																			 		const byte* data,
																			 		dword length,
																			 		dword limit);
static int diva_pri_v3_reset_adapter (struct _diva_os_xdi_adapter* a);
static int diva_pri_v3_stop_adapter (diva_os_xdi_adapter_t* a);
static int diva_pri_v3_start_adapter (PISDN_ADAPTER IoAdapter, dword features);

/*
**  BAR0 - MEM - 512      - CONFIG MEM
**  BAR1 - I/O - 256      - UNUSED
**  BAR2 - MEM - PRI_V3_MEMORY_SIZE - SDRAM (8Mbyte)
**  BAR3 - MEM - 256KBytes - CNTRL
**
**  Called by master adapter, that will initialize and add slave adapters
*/
int diva_pri_v3_init_card (diva_os_xdi_adapter_t* a) {
  int bar;
  unsigned long bar_length[sizeof(_pri_v3_bar_length)/sizeof(_pri_v3_bar_length[0])];

  memcpy (bar_length, (IDI_PROP(a->CardOrdinal,Card) != CARD_PASSIVEP) ? _pri_v3_bar_length : _pri_v3_passive_bar_length, sizeof(bar_length));

  a->xdi_adapter.BusNumber  = a->resources.pci.bus;
  a->xdi_adapter.slotNumber = a->resources.pci.func;
  a->xdi_adapter.hdev       = a->resources.pci.hdev;

  /*
    Get Serial Number and additional info
    The serial number of PRI3 is accessible in accordance with PCI spec
    via command register located in configuration space, also we do not
    have to map any BAR before we can access it
  */
  if (!diva_get_serial_number (&a->xdi_adapter)) {
    DBG_ERR(("A: can't get serial number for PRI 3.0 adapter"))
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }


  if (a->CardOrdinal != CARDTYPE_DIVA_L_P_V10_PCIE) {
  /*
    Fix up card ordinal to get righ adapter name and features
    */
    byte Bus, Slot;
    void* hdev;
    word id = 0;

    Bus = a->resources.pci.bus;
    Slot = a->resources.pci.func;
    hdev = a->resources.pci.hdev;

    PCIread (Bus, Slot, 0x2e, &id, sizeof(id), hdev) ;

    DBG_LOG(("A(%d) read Subsystem ID %04x", a->xdi_adapter.ANum, id))

    switch (id) {
      case 0x1C01:
      case 0x1C07:
        a->CardOrdinal = CARDTYPE_DIVASRV_P_8M_V30_PCI;
        break;
      case 0x1C02:
      case 0x1C08:
        if (a->xdi_adapter.csid == 0x03 || a->xdi_adapter.csid == 0x07) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24M_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24M_V30_PCI;
        }
        break;
      case 0x1C03:
      case 0x1C09:
        if (a->xdi_adapter.csid == 0x03 || a->xdi_adapter.csid == 0x07) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30M_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30M_V30_PCI;
        }
        break;
      case 0x1C04:
      case 0x1C0A:
        if (a->xdi_adapter.csid == 0x02 || a->xdi_adapter.csid == 0x06) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_2V_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_2V_V30_PCI;
        }
        break;
      case 0x1C05:
      case 0x1C0B:
        if (a->xdi_adapter.csid == 0x02 || a->xdi_adapter.csid == 0x06) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24V_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24V_V30_PCI;
        }
        break;
      case 0x1C06:
      case 0x1C0C:
        if (a->xdi_adapter.csid == 0x02 || a->xdi_adapter.csid == 0x06) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30V_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30V_V30_PCI;
        }
        break;
      case 0x1C0E:
        if (a->xdi_adapter.csid == 0x02 || a->xdi_adapter.csid == 0x06) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24UM_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_24UM_V30_PCI;
        }
        break;
      case 0x1C0D:
        if (a->xdi_adapter.csid == 0x02 || a->xdi_adapter.csid == 0x06) {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30UM_V30_PCIE;
        } else {
          a->CardOrdinal = CARDTYPE_DIVASRV_P_30UM_V30_PCI;
        }
        break;
    }
  }

  /*
    Set properties
  */
  a->xdi_adapter.cardType   = a->CardOrdinal;
  a->xdi_adapter.Properties = CardProperties[a->CardOrdinal];
  a->xdi_adapter.xconnectExportMode = MIN(DivaXconnectExportModeMax, diva_os_get_nr_li_exports (&a->xdi_adapter));
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
      DBG_ERR(("A: invalid bar[%d]=%08x", bar, a->resources.pci.bar[bar]))
      return (-1);
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
      if (!a->resources.pci.addr[bar]) {
				DBG_FTL(("A: %s : can't map bar[%d]", a->xdi_adapter.Properties.Name, bar))
        diva_pri_v3_cleanup_adapter (a);
        return (-1);
      }
    }
  }

  /*
    Register I/O port
  */
  sprintf (&a->port_name[0], "DIVA PRI 3.0 %ld", (long)a->xdi_adapter.serialNo);

  if (diva_os_register_io_port (a, 1, a->resources.pci.bar[1],
                                bar_length[1],
                                &a->port_name[0])) {
    DBG_ERR(("A: %s : can't register bar[1]", a->xdi_adapter.Properties.Name))
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }

  a->resources.pci.addr[1] = (void*)(unsigned long)a->resources.pci.bar[1];

  /*
    Set up hardware related pointers
  */
  a->xdi_adapter.cfg = (void*)(unsigned long)a->resources.pci.bar[0]; /* BAR0 CONFIG */
  a->xdi_adapter.port = (void*)(unsigned long)a->resources.pci.bar[1]; /* BAR1        */
  a->xdi_adapter.Address = a->resources.pci.addr[2];        /* BAR2 SDRAM  */
  a->xdi_adapter.ctlReg = (void*)(unsigned long)a->resources.pci.bar[3]; /* BAR3 CNTRL  */

  a->xdi_adapter.reset = a->resources.pci.addr[0]; /* BAR0 CONFIG */
  a->xdi_adapter.ram   = a->resources.pci.addr[2]; /* BAR2 SDRAM  */
	if (IDI_PROP(a->CardOrdinal,Card) != CARD_PASSIVEP) {
		a->xdi_adapter.ram  += MP_SHARED_RAM_OFFSET;
	}

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
		Initialize OS objects
		*/
  if (diva_os_initialize_spin_lock (&a->xdi_adapter.isr_spin_lock, "isr")) {
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }
  if (diva_os_initialize_spin_lock (&a->xdi_adapter.data_spin_lock, "data")) {
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }

  strcpy (a->xdi_adapter.req_soft_isr.dpc_thread_name, "kdspri3d");

  if (diva_os_initialize_soft_isr (&a->xdi_adapter.req_soft_isr,
                                    DIDpcRoutine,
                                    &a->xdi_adapter)) {
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }

  /*
    Do not initialize second DPC - only one thread will be created
  */
  a->xdi_adapter.isr_soft_isr.object = a->xdi_adapter.req_soft_isr.object;

  /*
    Next step of card initialization:
    set up all interface pointers
  */
  a->xdi_adapter.Channels = CardProperties[a->CardOrdinal].Channels;
  a->xdi_adapter.e_max    = CardProperties[a->CardOrdinal].E_info;

  a->xdi_adapter.e_tbl = diva_os_malloc (0, a->xdi_adapter.e_max * sizeof(E_INFO));
  if (!a->xdi_adapter.e_tbl) {
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }
  memset (a->xdi_adapter.e_tbl, 0x00, a->xdi_adapter.e_max * sizeof(E_INFO));

  a->xdi_adapter.a.io               = &a->xdi_adapter;
  a->xdi_adapter.DIRequest          = request;
  a->interface.cleanup_adapter_proc = diva_pri_v3_cleanup_adapter;
  a->interface.stop_no_io_proc      = diva_pri_v3_stop_no_io;
  a->interface.cmd_proc             = diva_pri_v3_cmd_card_proc;

  prepare_pri_v3_functions (&a->xdi_adapter);

  {
    PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
    int LiChannels = IoAdapter->Properties.nrExportXconnectDescriptors[0][IoAdapter->xconnectExportMode];

    DBG_REG(("A(%d) use idi:%d Li:%d descriptors", IoAdapter->ANum, 32*2, LiChannels))
    diva_init_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry**)&a->xdi_adapter.dma_map, 32*2+LiChannels);
  }

  /*
    Set IRQ handler
  */
  a->xdi_adapter.irq_info.irq_nr = a->resources.pci.irq;
  sprintf (a->xdi_adapter.irq_info.irq_name,
            "DIVA PRI V.3 %ld",
            (long)a->xdi_adapter.serialNo);

  if (diva_os_register_irq (a, a->xdi_adapter.irq_info.irq_nr,
                            a->xdi_adapter.irq_info.irq_name, a->xdi_adapter.msi)) {
    diva_pri_v3_cleanup_adapter (a);
    return (-1);
  }
  a->xdi_adapter.irq_info.registered = 1;

  diva_log_info("%s IRQ:%u SerNo:%d", a->xdi_adapter.Properties.Name,
                                      a->resources.pci.irq,
                                      a->xdi_adapter.serialNo);

  return (0);
}

/*
**  Cleanup function
*/
static int diva_pri_v3_cleanup_adapter (diva_os_xdi_adapter_t* a) {
  int bar;

  /*
    Stop Adapter if adapter is running
  */
  if (a->xdi_adapter.Initialized) {
    diva_pri_v3_stop_adapter (a);
  }

  /*
    Remove ISR Handler
  */
  if (a->xdi_adapter.irq_info.registered) {
    diva_os_remove_irq (a, a->xdi_adapter.irq_info.irq_nr);
  }
  a->xdi_adapter.irq_info.registered = 0;

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
  if (a->resources.pci.bar[1] && a->resources.pci.addr[1]) {
    diva_os_register_io_port (a, 0, a->resources.pci.bar[1],
      _pri_v3_bar_length[1],
      &a->port_name[0]);
    a->resources.pci.bar[1]  = 0;
    a->resources.pci.addr[1] = 0;
  }

	/*
		Free OS objects
		*/
  diva_os_cancel_soft_isr (&a->xdi_adapter.isr_soft_isr);
  diva_os_cancel_soft_isr (&a->xdi_adapter.req_soft_isr);

  diva_os_remove_soft_isr (&a->xdi_adapter.req_soft_isr);
  a->xdi_adapter.isr_soft_isr.object = 0;

  diva_os_destroy_spin_lock (&a->xdi_adapter.isr_spin_lock, "rm");
  diva_os_destroy_spin_lock (&a->xdi_adapter.data_spin_lock, "rm");

  diva_cleanup_vidi (&a->xdi_adapter);

  /*
    Free memory occupied by XDI adapter
  */
  if (a->xdi_adapter.e_tbl) {
    diva_os_free (0, a->xdi_adapter.e_tbl);
    a->xdi_adapter.e_tbl = 0;
  }
  a->xdi_adapter.Channels = 0;
  a->xdi_adapter.e_max = 0;

  /*
    Free adapter DMA map
    */
  diva_free_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry*)a->xdi_adapter.dma_map);
  a->xdi_adapter.dma_map = 0;

  DBG_LOG(("A(%d) %s adapter cleanup complete", a->xdi_adapter.ANum, a->xdi_adapter.Properties.Name))

  return (0);
}

static int diva_pri_v3_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length) {
  int ret = -1;

  if (cmd->adapter != a->controller) {
    DBG_ERR(("A: pri_v3_cmd, invalid controller=%d != %d", cmd->adapter, a->controller))
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
      break;

    case DIVA_XDI_UM_CMD_GET_CARD_STATE:
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
      break;

    case DIVA_XDI_UM_CMD_WRITE_FPGA:
			ret = diva_pri_v3_write_fpga_image (a, (byte*)&cmd[1], cmd->command_data.write_fpga.image_length);
      break;

    case DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK:
      ret = diva_pri_v3_write_sdram_block (&a->xdi_adapter,
                                           cmd->command_data.write_sdram.offset,
                                           (byte*)&cmd[1],
                                           cmd->command_data.write_sdram.length,
                                           a->xdi_adapter.MemorySize);
			break;

    case DIVA_XDI_UM_CMD_RESET_ADAPTER:
			ret = diva_pri_v3_reset_adapter (a);
      break;

    case DIVA_XDI_UM_CMD_SET_PROTOCOL_FEATURES:
      a->xdi_adapter.features = cmd->command_data.features.features;
      a->xdi_adapter.a.protocol_capabilities = a->xdi_adapter.features;
      DBG_TRC(("Set raw protocol features (%08x)", a->xdi_adapter.features))
      ret = 0;
      break;

    case DIVA_XDI_UM_CMD_ADAPTER_TEST:
      if (a->xdi_adapter.DivaAdapterTestProc) {
        a->xdi_adapter.AdapterTestMask = cmd->command_data.test.test_command;
        if ((*(a->xdi_adapter.DivaAdapterTestProc))(&a->xdi_adapter)) {
          ret = -1;
        } else {
          ret = 0;
        }
      } else {
        ret = -1;
      }
      a->xdi_adapter.AdapterTestMask = 0;
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
      ret = diva_pri_v3_start_adapter (&a->xdi_adapter, cmd->command_data.start.features);
      break;

    case DIVA_XDI_UM_CMD_READ_XLOG_ENTRY:
			ret = diva_card_read_xlog (a);
      break;

    case DIVA_XDI_UM_CMD_READ_SDRAM:
      if (a->xdi_adapter.Address != 0 && diva_pri_v3_fpga_ready (&a->xdi_adapter) == 0) {
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

    case DIVA_XDI_UM_CMD_STOP_ADAPTER:
      ret = diva_pri_v3_stop_adapter (a);
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
      if (a->xdi_adapter.Initialized == 0 &&
          a->xdi_adapter.host_vidi.remote_indication_counter == 0 &&
          (cmd->command_data.vidi_mode.vidi_mode == 0 || a->xdi_adapter.dma_map != 0)) {
        PISDN_ADAPTER IoAdapter = &a->xdi_adapter;

        DBG_LOG(("A(%d) vidi %s", IoAdapter->ANum,
                cmd->command_data.vidi_mode.vidi_mode != 0 ? "on" : "off"))

        IoAdapter->host_vidi.vidi_active = cmd->command_data.vidi_mode.vidi_mode != 0;
        prepare_pri_v3_functions (IoAdapter);

        ret = 0;
      }
      break;

    default:
      DBG_ERR(("A: A(%d) invalid cmd=%d", a->controller, cmd->command))
	}

	return (ret);
}

static int diva_pri_v3_reset_adapter (struct _diva_os_xdi_adapter* a) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
  byte Bus, Slot;
  void* hdev;
  byte irq;

  if (!IoAdapter->Address || !IoAdapter->reset) {
    return (-1);
  }
  if (IoAdapter->Initialized) {
    DBG_ERR(("A: A(%d) can't reset %s adapter - please stop first",
						IoAdapter->ANum, a->xdi_adapter.Properties.Name))
    return (-1);
  }
  /*
    Reset erases interrupt setting from the
    PCI configuration space
  */

  Bus = a->resources.pci.bus;
  Slot = a->resources.pci.func;
  hdev = a->resources.pci.hdev;

  PCIread (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
  DBG_TRC(("SAVE IRQ LINE: %02x", irq))

  (*(IoAdapter->rstFnc))(IoAdapter);

  PCIwrite (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
  PCIread  (Bus, Slot, 0x3c, &irq, sizeof(irq), hdev) ;
  DBG_TRC(("RESTORED IRQ LINE: %02x", irq))

/*
    Forget all outstanding entities
  */
  IoAdapter->e_count =  0;
  if (IoAdapter->e_tbl) {
    memset (IoAdapter->e_tbl, 0x00, IoAdapter->e_max * sizeof(E_INFO));
  }
  IoAdapter->head = 0;
  IoAdapter->tail = 0;
  IoAdapter->assign = 0;
  IoAdapter->trapped = 0;

  memset (&IoAdapter->a.IdTable[0],              0x00, sizeof(IoAdapter->a.IdTable));
  memset (&IoAdapter->a.IdTypeTable[0],          0x00, sizeof(IoAdapter->a.IdTypeTable));
  memset (&IoAdapter->a.FlowControlIdTable[0],   0x00, sizeof(IoAdapter->a.FlowControlIdTable));
  memset (&IoAdapter->a.FlowControlSkipTable[0], 0x00, sizeof(IoAdapter->a.FlowControlSkipTable));
  memset (&IoAdapter->a.misc_flags_table[0],     0x00, sizeof(IoAdapter->a.misc_flags_table));
  memset (&IoAdapter->a.rx_stream[0],            0x00, sizeof(IoAdapter->a.rx_stream));
  memset (&IoAdapter->a.tx_stream[0],            0x00, sizeof(IoAdapter->a.tx_stream));

  diva_cleanup_vidi (IoAdapter);

  if (IoAdapter->dma_map) {
    diva_reset_dma_mapping (IoAdapter->dma_map);
  }

  return (0);
}

/*
	Download FPGA image
	*/
static int diva_pri_v3_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
	volatile dword* reset = (volatile dword*)IoAdapter->ctlReg;
  int ret;

	if (IoAdapter->Initialized != 0) {
		return (-1);
	}

	reset += (0x20000/sizeof(*reset));

  diva_xdiLoadFileFile = data;
  diva_xdiLoadFileLength = length;

	{
		dword csum = 0;
		int p;

		for (p = 0; p < length; p++) {
			csum += data[p];
		}
		DBG_TRC(("FPGA IMAGE CSUM=%08x, LENGTH=%d", csum, length))
	}

  ret = pri_v3_FPGA_download (IoAdapter);

  diva_xdiLoadFileFile = 0;
  diva_xdiLoadFileLength = 0;

	a->dsp_mask = 0;

	if (ret != 0 && IDI_PROP(a->CardOrdinal,Card) != CARD_PASSIVEP) {
		/*
			Can't access the DSP before FPGA was downloaded.
			Also report DSP failure as FPGA download problem.
			*/
  	a->dsp_mask = (*(IoAdapter->DetectDsps))(IoAdapter);

		{
			int dsp_nr, dsp_count;

			for (dsp_nr = 0, dsp_count = 0; dsp_nr < 32; dsp_nr++) {
				dsp_count += ((a->dsp_mask & (1 << dsp_nr)) != 0);
			}
			if (dsp_count < 2) {
				DBG_FTL(("%s DSP error", IoAdapter->Properties.Name))
	    	return (-1);
			}
		}
	}

  return (ret ? 0 : -1);
}

static int diva_pri_v3_write_sdram_block (PISDN_ADAPTER IoAdapter,
																			 		dword address,
																			 		const byte* data,
																			 		dword length,
																			 		dword limit) {
	byte* mem = IoAdapter->Address;

	if (((address + length) >= limit) || !mem) {
		DBG_ERR(("A: A(%d) write PRI 3.0 address=0x%08lx", IoAdapter->ANum, address+length))
		return (-1);
	}

	if (diva_dma_write_sdram_block (IoAdapter, 1, 1, address, data, length) != 0) {
		mem += address;
		memcpy (mem, data, length);
	}

	return (0);
}

static void
diva_pri_v3_clear_interrupts (diva_os_xdi_adapter_t* a)
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

static int diva_pri_v3_stop_adapter_w_io (diva_os_xdi_adapter_t* a, int do_io) {
  PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  divas_pci_card_resources_t *p_pci = &(a->resources.pci);
#endif

  if (!IoAdapter->ram) {
    return (-1);
  }
  if (!IoAdapter->Initialized) {
    DBG_ERR(("A: A(%d) can't stop PRI V.3 adapter - not running",
              IoAdapter->ANum))
    return (-1); /* nothing to stop */
  }
  DBG_LOG(("%s Adapter: %d", __FUNCTION__, IoAdapter->ANum));

  if (do_io != 0) {
    /*
      Stop Adapter
      */
    if (IoAdapter->host_vidi.vidi_active == 0) {
      a->clear_interrupts_proc = diva_pri_v3_clear_interrupts;
      IoAdapter->Initialized = 0;
    }
    IoAdapter->stop (IoAdapter) ;
    if (a->clear_interrupts_proc) {
      diva_pri_v3_clear_interrupts (a);
      a->clear_interrupts_proc = 0;
    }
  }
  IoAdapter->Initialized = 0;

  diva_os_cancel_soft_isr (&a->xdi_adapter.isr_soft_isr);
  diva_os_cancel_soft_isr (&a->xdi_adapter.req_soft_isr);

  /*
    Disconnect Adapters from DIDD
    */
  diva_xdi_didd_remove_adapter (IoAdapter->ANum);

  diva_os_cancel_soft_isr (&a->xdi_adapter.isr_soft_isr);
  diva_os_cancel_soft_isr (&a->xdi_adapter.req_soft_isr);

  IoAdapter->a.ReadyInt = 0;

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

static int diva_pri_v3_stop_adapter (diva_os_xdi_adapter_t* a) {
	return (diva_pri_v3_stop_adapter_w_io (a, 1));
}

static int diva_pri_v3_stop_no_io (diva_os_xdi_adapter_t* a) {
  if (a->xdi_adapter.Initialized != 0) {
		return (diva_pri_v3_stop_adapter_w_io (a, 0));
	}

	return (0);
}

static int vidi_diva_pri3_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int i;

	if (IoAdapter->host_vidi.vidi_active == 0 || IoAdapter->host_vidi.remote_indication_counter == 0) {
		DBG_ERR(("A(%d) vidi not initialized", IoAdapter->ANum))
		return (-1);
	}
	DBG_LOG(("A(%d) vidi start", IoAdapter->ANum))

	/*
		Allow interrupts. VIDI reports adapter start up using message.
		*/
	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_ENABLE;
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |= PLX9054_L2PDBELL_INT_ENABLE;

	/*
		Activate DPC
		*/
	IoAdapter->Initialized = 1;
	IoAdapter->host_vidi.vidi_started = 0;

	start_pri_v3_hardware (IoAdapter);

	for (i = 0; i < 300; i++) {
		diva_os_sleep (10);
		if (IoAdapter->host_vidi.vidi_started != 0)
			return (i);
	}

	/*
		Adapter start failed, de-activate DPC
		*/
	IoAdapter->Initialized = 0;

	/*
		De-activate interrupts
		*/
	IoAdapter->disIrq (IoAdapter) ;

	return (-1);
}

static int idi_diva_pri3_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int i, started = -1;

	DBG_LOG(("A(%d) idi start", IoAdapter->ANum))

	/*
		Start adapter
		*/
	start_pri_v3_hardware (IoAdapter);

	for (i = 0; started < 0 && i < 300; ++i) {
		diva_os_wait (10);
		if (*(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_SIGNATURE) == DIVA_PRI_V3_BOOT_SIGNATURE_OK) {
			started = i;
		}
	}

	if (started < 0)
		return (-1);

	/*
		Test interrupt
		*/
	diva_os_wait(100);

	IoAdapter->Initialized = 1;
	IoAdapter->IrqCount = 0;

	*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_ENABLE;
	*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] |= PLX9054_L2PDBELL_INT_ENABLE;

	IoAdapter->a.ReadyInt = 1 ;
	IoAdapter->a.ram_out (&IoAdapter->a, &PR_RAM->ReadyInt, 1) ;

	for (i = 100; !IoAdapter->IrqCount && (i-- > 0); diva_os_wait (10));

	if (!IoAdapter->IrqCount) {
		DBG_ERR(("A(%d) interrupt test failed", IoAdapter->ANum))
		IoAdapter->disIrq(IoAdapter);
		IoAdapter->Initialized = 0;
		return (-1);
	}

	return (started);
}

static int diva_pri_v3_start_adapter (PISDN_ADAPTER IoAdapter, dword features) {
	int adapter_status;

  if (IoAdapter->Initialized != 0) {
    DBG_ERR(("A(%d) adapter already running", IoAdapter->ANum))
    return (-1);
  }
  if (IoAdapter->Address == 0) {
    DBG_ERR(("A(%d) adapter not mapped", IoAdapter->ANum))
    return (-1);
  }

	if (IDI_PROP(IoAdapter->cardType,Card) != CARD_PASSIVEP) {
		if (IoAdapter->host_vidi.vidi_active != 0) {
			adapter_status = vidi_diva_pri3_start_adapter (IoAdapter, features);
		} else {
			adapter_status = idi_diva_pri3_start_adapter (IoAdapter, features);
		}
	} else {
		IoAdapter->Initialized = 1;
		IoAdapter->IrqCount = 0;
		*(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL] = *(volatile dword*)&IoAdapter->reset[PLX9054_L2PDBELL];
		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_LOCAL_INT_ENABLE;
		*(volatile dword*)&IoAdapter->reset[PLX9054_INTCSR_DW] &= ~PLX9054_L2PDBELL_INT_ENABLE;
		adapter_status = 1;
	}

	if (adapter_status >= 0) {
		DBG_LOG(("A(%d) Protocol startup time %d.%02d seconds",
							IoAdapter->ANum, (adapter_status / 100), (adapter_status % 100)))
		IoAdapter->Properties.Features = (word)features;

		diva_xdi_display_adapter_features (IoAdapter->ANum);

		DBG_LOG(("A(%d) %s adapter successfull started", IoAdapter->ANum, IDI_PROP(IoAdapter->cardType,Name)))

		/*
			Register with DIDD
			*/
		if (IDI_PROP(IoAdapter->cardType,Card) == CARD_PASSIVEP) {
			IoAdapter->Properties.DescType = IDI_UP1DM;
			IoAdapter->Properties.Channels = 0;
			IoAdapter->Properties.Features = 0;
		}
		diva_xdi_didd_register_adapter (IoAdapter->ANum);

	} else {
    DBG_ERR(("A(%d) Adapter start failed Signature=0x%08lx, TrapId=%08lx, boot count=%08lx",
              IoAdapter->ANum,
              *(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_SIGNATURE),
              *(volatile dword*)(IoAdapter->Address + 0x80),
              *(volatile dword*)(IoAdapter->Address + DIVA_PRI_V3_BOOT_COUNT)))
    IoAdapter->stop(IoAdapter);
    DBG_ERR(("-----------------------------------------------------------------"))
    DBG_ERR(("XLOG recovery for adapter %d %s (%p)",
              IoAdapter->ANum,
              IoAdapter->Properties.Name,
              IoAdapter->ram))
    (*(IoAdapter->trapFnc))(IoAdapter);
    DBG_ERR(("-----------------------------------------------------------------"))
	}

	return ((adapter_status >= 0) ? 0 : -1);
}

static int diva_pri_v3_connect_interrupt (void* resource, int (*isr_proc)(void*), void* isr_proc_context) {
	diva_os_xdi_adapter_t* a = (diva_os_xdi_adapter_t*)resource;

	return (diva_pri_passive_connect_interrupt (&a->xdi_adapter, isr_proc, isr_proc_context));
}

static void diva_pri_v3_get_hw_ifc (PISDN_ADAPTER IoAdapter, diva_xdi_hw_access_t* info) {
	diva_os_xdi_adapter_t* a = DIVAS_CONTAINING_RECORD(IoAdapter, diva_os_xdi_adapter_t, xdi_adapter);
	int i;

	memset (info, 0x00, sizeof(*info));
	info->hdev     = a->resources.pci.hdev;
	info->resource = a;

  for (i = 0; i < 4; i++) {
		info->bar[i]  = a->resources.pci.bar[i];
		info->addr[i] = a->resources.pci.addr[i];
	}

	info->alloc_dma_page_proc = diva_hw_ifc_alloc_dma_page;
	info->free_dma_page_proc  = diva_hw_ifc_free_dma_page;

	info->connect_irq_proc = diva_pri_v3_connect_interrupt;

	info->logical_adapter_number = IoAdapter->ANum;
	info->serial_number          = IoAdapter->serialNo;
}

void diva_os_prepare_pri_v3_functions (PISDN_ADAPTER IoAdapter) {
	diva_os_xdi_adapter_t* a = DIVAS_CONTAINING_RECORD(IoAdapter, diva_os_xdi_adapter_t, xdi_adapter);

	if (IDI_PROP(a->CardOrdinal,Card) == CARD_PASSIVEP) {
		IoAdapter->get_hw_ifc = diva_pri_v3_get_hw_ifc;
	}
}

