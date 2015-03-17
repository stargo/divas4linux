/*
 *
  Copyright (c) Dialogic, 2008.
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
#include "dsrv_4prie.h"
#include "os_4prie.h"
#include "diva_pci.h"
#include "mi_pc.h"

#include "fpga_rom.h" /* HERE */
#include "fpga_rom_xdi.h" /* HERE */
/*
  IMPORTS
  */
extern void* diva_xdiLoadFileFile;
extern dword diva_xdiLoadFileLength;
extern void diva_add_slave_adapter    (diva_os_xdi_adapter_t* a);
extern void diva_xdi_display_adapter_features (int card);
extern int diva_card_read_xlog (diva_os_xdi_adapter_t* a);
extern void  prepare_4prie_functions   (PISDN_ADAPTER IoAdapter);
extern int pri_4prie_FPGA_download (PISDN_ADAPTER IoAdapter);

/*
**  LOCALS
*/
static int diva_4prie_stop_adapter (struct _diva_os_xdi_adapter* a);
static int diva_4prie_cleanup_adapter (diva_os_xdi_adapter_t* a);
static int diva_4prie_stop_no_io (diva_os_xdi_adapter_t* a);
static int diva_4prie_cleanup_slave_adapters (diva_os_xdi_adapter_t* a);
static int diva_4prie_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length);
static int diva_4prie_write_sdram_block (PISDN_ADAPTER IoAdapter,
																				 dword address,
																		 		 const byte* data,
																		 		 dword length,
																		 		 dword limit);
static int diva_4prie_reset_adapter (struct _diva_os_xdi_adapter* a);

static unsigned long _4prie_bar_length[4] = {
  64*1024*1024, /* MEM */
  0,
  256*1024,     /* REGISTER MEM */
  0
};

int diva_4prie_init_card (diva_os_xdi_adapter_t* a) {
  unsigned long bar_length[sizeof(_4prie_bar_length)/sizeof(_4prie_bar_length[0])];
  int i, bar, diva_tasks;
  PADAPTER_LIST_ENTRY quadro_list;
  diva_os_xdi_adapter_t* diva_current;
  diva_os_xdi_adapter_t* adapter_list[8];
  PISDN_ADAPTER Slave;
  dword offset;

  switch (a->CardOrdinal) {
    case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
      a->dsp_mask = 0x000003ff;
      diva_tasks = 4;
      break;

    case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
      a->dsp_mask = 0x000003ff;
      diva_tasks = 8;
      break;

    default:
     DBG_ERR(("4PRIE unknown adapter: %d", a->CardOrdinal))
     return (-1);
  }

  a->xdi_adapter.BusNumber  = a->resources.pci.bus;
  a->xdi_adapter.slotNumber = a->resources.pci.func;
  a->xdi_adapter.hdev       = a->resources.pci.hdev;

  /*
    Set properties
    */  
  a->xdi_adapter.Properties = CardProperties[a->CardOrdinal];

  memcpy (bar_length, _4prie_bar_length, sizeof(bar_length));

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
    a->resources.pci.bar[bar] = 0;
    if (bar_length[bar] != 0) {
      a->resources.pci.bar[bar] = divasa_get_pci_bar (a->resources.pci.bus,
                                                      a->resources.pci.func,
                                                      bar,
                                                      a->resources.pci.hdev);
      if (a->resources.pci.bar[bar] == 0 || a->resources.pci.bar[bar] == 0xFFFFFFF0) {
        DBG_ERR(("%s : bar[%d]=%08x", a->xdi_adapter.Properties.Name, bar, a->resources.pci.bar[bar]))
        return (-1);
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

  a->xdi_adapter.sdram_bar = a->resources.pci.bar [0];

  /*
    Map all MEMORY BAR's
    */
  for (bar = 0; bar < 4; bar++) {
    a->resources.pci.addr[bar] = 0;
    if (bar_length[bar] != 0) {
      a->resources.pci.addr[bar] = divasa_remap_pci_bar (a, a->resources.pci.bar[bar], bar_length[bar]);
      if (a->resources.pci.addr[bar] == 0) { 
        DBG_FTL(("%s : failed to map bar[%d]", a->xdi_adapter.Properties.Name, bar))
        diva_4prie_cleanup_adapter (a);
        return (-1);
      }
    }
  }

  sprintf (&a->port_name[0], "DIVA %sPRI %ld", ((diva_tasks == 4) ? "4" : "8"), (long)a->xdi_adapter.serialNo);

  /* 
    Set cleanup pointer for base adapter only, so slave adapter
    will be unable to get cleanup
  */
  a->interface.cleanup_adapter_proc = diva_4prie_cleanup_adapter;
  a->interface.stop_no_io_proc      = diva_4prie_stop_no_io;

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
      diva_4prie_cleanup_adapter (a);
      return (-1);
    }
    memset (a->slave_adapters[i], 0x00, sizeof(*a));
  }

  adapter_list[0] = a;
  adapter_list[1] = a->slave_adapters[0];
  adapter_list[2] = a->slave_adapters[1];
  adapter_list[3] = a->slave_adapters[2];
  adapter_list[4] = a->slave_adapters[3];
  adapter_list[5] = a->slave_adapters[4];
  adapter_list[6] = a->slave_adapters[5];
  adapter_list[7] = a->slave_adapters[6];

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
    diva_4prie_cleanup_adapter (a);
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
    diva_current->interface.cmd_proc = diva_4prie_cmd_card_proc;
    diva_current->xdi_adapter.Properties = CardProperties[a->CardOrdinal];
    diva_current->CardOrdinal = a->CardOrdinal;

    diva_current->xdi_adapter.Channels = CardProperties[a->CardOrdinal].Channels;
    diva_current->xdi_adapter.e_max    = CardProperties[a->CardOrdinal].E_info;
    diva_current->xdi_adapter.e_tbl    = diva_os_malloc (0, diva_current->xdi_adapter.e_max * sizeof(E_INFO)); 

    if (diva_current->xdi_adapter.e_tbl == 0) {
      diva_4prie_cleanup_slave_adapters (a);
      diva_4prie_cleanup_adapter (a);
      for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }
    memset (diva_current->xdi_adapter.e_tbl, 0x00, diva_current->xdi_adapter.e_max * sizeof(E_INFO));

    if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.isr_spin_lock, "isr")) {
      diva_4prie_cleanup_slave_adapters (a);
      diva_4prie_cleanup_adapter (a);
      for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }
    if (diva_os_initialize_spin_lock (&diva_current->xdi_adapter.data_spin_lock, "data")) {
      diva_4prie_cleanup_slave_adapters (a);
      diva_4prie_cleanup_adapter (a);
      for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }

    if (diva_tasks == 4)
      strcpy (diva_current->xdi_adapter.req_soft_isr.dpc_thread_name, "kdivas4prid");
    else
      strcpy (diva_current->xdi_adapter.req_soft_isr.dpc_thread_name, "kdivas8prid");

    if (diva_os_initialize_soft_isr (&diva_current->xdi_adapter.req_soft_isr,
                                     DIDpcRoutine,
                                     &diva_current->xdi_adapter)) {
      diva_4prie_cleanup_slave_adapters (a);
      diva_4prie_cleanup_adapter (a);
      for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
      return (-1);
    }

    /*
      Do not initialize second DPC - only one thread will be created
      */
    diva_current->xdi_adapter.isr_soft_isr.object = diva_current->xdi_adapter.req_soft_isr.object;
  }

  prepare_4prie_functions (&a->xdi_adapter);

  /*
    Set up hardware related pointers
    */
  a->xdi_adapter.Address = a->resources.pci.addr[0];    /* BAR0 MEM  */
  a->xdi_adapter.ram     = a->resources.pci.addr[0];    /* BAR0 MEM  */
  a->xdi_adapter.reset   = a->resources.pci.addr[2];    /* BAR2 REGISTERS */
    
  a->xdi_adapter.AdapterTestMemoryStart  = a->resources.pci.addr[0]; /* BAR0 MEM  */
  a->xdi_adapter.AdapterTestMemoryLength = bar_length[0];

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

    diva_init_dma_map (a->resources.pci.hdev, (struct _diva_dma_map_entry**)&Slave->dma_map, 2 /* HERE */);
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
#if 0 /* HERE */
  if (diva_os_register_irq (a, a->xdi_adapter.irq_info.irq_nr,
                            a->xdi_adapter.irq_info.irq_name, a->xdi_adapter.msi)) {
    diva_4prie_cleanup_slave_adapters (a);
    diva_4prie_cleanup_adapter (a);
    for (i = 1; i < diva_tasks; i++) { diva_os_free (0, adapter_list[i]); }
    return (-1);
  }

  a->xdi_adapter.irq_info.registered = 1;
#else
  a->xdi_adapter.irq_info.registered = 0;
#endif

  /*
    Add slave adapter
    */
  for (i = 1; i < diva_tasks; i++) {
    diva_add_slave_adapter (adapter_list[i]);
  }

	{
byte* p = a->resources.pci.addr[2];
	diva_xdi_show_fpga_rom_features (p + 0x9000);
	}

  diva_log_info("%s IRQ:%u SerNo:%d", a->xdi_adapter.Properties.Name, a->resources.pci.irq, a->xdi_adapter.serialNo);

  return (0);
}

static int diva_4prie_stop_adapter (struct _diva_os_xdi_adapter* a) {

  return (0);
}

static int diva_4prie_cleanup_adapter (diva_os_xdi_adapter_t* a) {
   int bar;

  /*
    Stop Adapter if adapter is running
    */
  if (a->xdi_adapter.Initialized) {
    diva_4prie_stop_adapter (a);
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
  diva_4prie_cleanup_slave_adapters (a);

  /*
    Unmap all BARS
    */
  for (bar = 0; bar < 4; bar++) {
    if (a->resources.pci.bar[bar] && a->resources.pci.addr[bar]) {
      divasa_unmap_pci_bar (a->resources.pci.addr[bar]);
      a->resources.pci.bar[bar]  = 0;
      a->resources.pci.addr[bar] = 0;
    }
  }

  if (a->slave_list) {
    diva_os_free (0, a->slave_list);
    a->slave_list = 0;
  }

  DBG_LOG(("A(%d) %s adapter cleanup complete", a->xdi_adapter.ANum, a->xdi_adapter.Properties.Name))

  return (0);
}

static int diva_4prie_stop_no_io (diva_os_xdi_adapter_t* a) {
  if (a->xdi_adapter.ControllerNumber != 0)
    return (-1);

  return (0);
}

static int diva_4prie_cleanup_slave_adapters (diva_os_xdi_adapter_t* a) {
  diva_os_xdi_adapter_t* adapter_list[8];
  diva_os_xdi_adapter_t* diva_current;
  int i;

  adapter_list[0] = a;
  adapter_list[1] = a->slave_adapters[0];
  adapter_list[2] = a->slave_adapters[1];
  adapter_list[3] = a->slave_adapters[2];
  adapter_list[4] = a->slave_adapters[3];
  adapter_list[5] = a->slave_adapters[4];
  adapter_list[6] = a->slave_adapters[5];
  adapter_list[7] = a->slave_adapters[6];

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

static int diva_4prie_write_fpga_image (diva_os_xdi_adapter_t* a, byte* data, dword length) {
  PISDN_ADAPTER IoAdapter = &a->xdi_adapter;
  int ret;

  diva_xdiLoadFileFile = data;
  diva_xdiLoadFileLength = length;

  ret = pri_4prie_FPGA_download (IoAdapter);

  diva_xdiLoadFileFile = 0;
  diva_xdiLoadFileLength = 0;

  return (ret ? 0 : -1);
}

void diva_os_prepare_4prie_functions (PISDN_ADAPTER IoAdapter) {
}

static int diva_4prie_cmd_card_proc (struct _diva_os_xdi_adapter* a, diva_xdi_um_cfg_cmd_t* cmd, int length) {
  int ret = -1;

  if (cmd->adapter != a->controller) {
    DBG_ERR(("4prie_cmd, wrong controller=%d != %d", cmd->adapter, a->controller))
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

    case DIVA_XDI_UM_CMD_RESET_ADAPTER:
      if (a->xdi_adapter.ControllerNumber == 0) {
				ret = diva_4prie_reset_adapter (a);
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
        ret = diva_4prie_write_fpga_image (a, (byte*)&cmd[1], cmd->command_data.write_fpga.image_length);
      }
      break;

    case DIVA_XDI_UM_CMD_ADAPTER_TEST:
      if (a->xdi_adapter.ControllerNumber == 0) {
        a->xdi_adapter.AdapterTestMask = cmd->command_data.test.test_command;

        if ((*(a->xdi_adapter.DivaAdapterTestProc))(&a->xdi_adapter) == 0) {
          DBG_LOG(("Memory test : OK"))
          ret = 0;
        } else {
          DBG_ERR(("Memory test : FAILED"))
          ret = -1;
          break;
        }
        a->xdi_adapter.AdapterTestMask = 0;
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

    case DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK:
			if (a->xdi_adapter.ControllerNumber == 0) {
      	ret = diva_4prie_write_sdram_block (&a->xdi_adapter,
																					 	cmd->command_data.write_sdram.offset,
																					 	(byte*)&cmd[1],
																					 	cmd->command_data.write_sdram.length,
																					 	64*1024*1024-64/* a->xdi_adapter.MemorySize */);
			}
			break;

    default:
      DBG_ERR(("A(%d) unknown cmd=%d", a->controller, cmd->command))
      break;
  }

  return (ret);
}

static int diva_4prie_reset_adapter (struct _diva_os_xdi_adapter* a) {
	PISDN_ADAPTER IoAdapter = &a->xdi_adapter;

	if (!IoAdapter->Address || !IoAdapter->reset) {
		return (-1);
	}
	if (IoAdapter->Initialized != 0) {
		DBG_ERR(("A: A(%d) can't reset %s adapter - please stop first",
						IoAdapter->ANum, a->xdi_adapter.Properties.Name))
		return (-1);
	}

	(*(IoAdapter->rstFnc))(IoAdapter);

	return (0);
}

static int diva_4prie_write_sdram_block (PISDN_ADAPTER IoAdapter,
																				 dword address,
																		 		 const byte* data,
																		 		 dword length,
																		 		 dword limit)  {
	byte* mem = IoAdapter->Address;

  if (((address + length) >= limit) || !IoAdapter->Address) {
    DBG_ERR(("A(%d) %s write address=0x%08x",
            IoAdapter->ANum,
            IoAdapter->Properties.Name,
            address+length))
    return (-1);
  }

	memcpy (&mem[address], data, length);

	return (0);
}

