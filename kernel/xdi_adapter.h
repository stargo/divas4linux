
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

#ifndef __DIVA_OS_XDI_ADAPTER_H__
#define __DIVA_OS_XDI_ADAPTER_H__

#define DIVAS_XDI_ADAPTER_BUS_PCI  0
#define DIVAS_XDI_ADAPTER_BUS_ISA  1

typedef struct _divas_pci_card_resources {
  byte bus;
  byte func;
  byte revId;
  void *hdev;

  dword bar[8];     /* contains context of appropriate BAR Register */
  void *addr[8];    /* same bar, but mapped into memory */
  dword length[8];  /* bar length */
  unsigned int qoffset;
  unsigned int irq;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  dword clock_data_bus_addr;
#endif
} divas_pci_card_resources_t;

typedef union _divas_card_resources {
  divas_pci_card_resources_t pci;
} divas_card_resources_t;

struct _diva_os_xdi_adapter;
typedef int (*diva_init_card_proc_t) (struct _diva_os_xdi_adapter * a);
typedef int (*diva_cmd_card_proc_t) (struct _diva_os_xdi_adapter * a,
             diva_xdi_um_cfg_cmd_t * data,
             int length);
typedef void (*diva_xdi_clear_interrupts_proc_t) (struct
              _diva_os_xdi_adapter *);

#define DIVA_XDI_MBOX_BUSY      1
#define DIVA_XDI_MBOX_WAIT_XLOG 2

typedef struct _xdi_mbox_t {
  dword status;
  diva_xdi_um_cfg_cmd_data_t cmd_data;
  dword data_length;
  void *data;
} xdi_mbox_t;

typedef struct _diva_os_idi_adapter_interface {
  diva_init_card_proc_t cleanup_adapter_proc;
  diva_cmd_card_proc_t cmd_proc;
  diva_init_card_proc_t stop_no_io_proc;
} diva_os_idi_adapter_interface_t;

typedef struct _diva_os_xdi_adapter {
  diva_entity_link_t link;
  int CardIndex;
  int CardOrdinal;
  int controller;          /* number of this controller */
  int Bus;                 /* PCI, ISA, ... */
  divas_card_resources_t resources;
  char port_name[24];
  ISDN_ADAPTER xdi_adapter;
  xdi_mbox_t xdi_mbox;
  diva_os_idi_adapter_interface_t interface;
  struct _diva_os_xdi_adapter *slave_adapters[7];
  void *slave_list;
  void *proc_adapter_dir;  /* adapterX proc entry */
  void *proc_info;         /* info proc entry     */
  void *proc_grp_opt;      /* group_optimization  */
  void *proc_d_l1_down;    /* dynamic_l1_down     */
  volatile diva_xdi_clear_interrupts_proc_t clear_interrupts_proc;
  dword dsp_mask;
  diva_os_soft_isr_t clock_interrupt_soft_isr;
} diva_os_xdi_adapter_t;

#endif
