/* $Id: divasmain.c,v 1.46 2003/10/10 12:28:14 armin Exp $
 *
 * Low level driver for Dialogic DIVA Server ISDN cards.
 *
 * Copyright 2000-2009 by Armin Schindler (mac@melware.de)
 * Copyright 2000-2009 Cytronics & Melware (info@melware.de)
 * Copyright 2007 Dialogic (www.eicon.com)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#if defined(CONFIG_DEVFS_FS)
#include <linux/devfs_fs_kernel.h>
#endif
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#include <linux/pci.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/kmod.h>

#include "platform.h"

#ifndef DMA_64BIT_MASK 
#define DMA_64BIT_MASK DMA_BIT_MASK(64)
#endif

#if (defined(CONFIG_PCIEAER) && defined(DIVA_XDI_AER_SUPPORT))
#include <linux/aer.h>
#endif

#undef N_DATA
#include "pc.h"
#include "dlist.h"
#include "di_defs.h"
#include "divasync.h"
#include "diva.h"
#include "di.h"
#include "xdi_msg.h"
#include "xdi_vers.h"
#include "diva_dma.h"
#include "diva_pci.h"

static char *main_revision = "$Revision: 1.46 $";

static int major;

static int dbgmask;

MODULE_DESCRIPTION("Kernel driver for Dialogic DIVA Server cards");
MODULE_AUTHOR("Cytronics & Melware, Dialogic");
MODULE_LICENSE("GPL");

module_param(dbgmask, uint, 0);
MODULE_PARM_DESC(dbgmask, "initial debug mask");

static unsigned int disabledac;
module_param(disabledac, uint, 0);
MODULE_PARM_DESC(disabledac, "disable DAC");

#ifdef CONFIG_PCI_MSI
static unsigned int no_msi;
module_param(no_msi, uint, 0);
MODULE_PARM_DESC(no_msi, "do not use MSI");
#endif

static char *DRIVERNAME =
    "Dialogic DIVA Server driver (http://www.melware.net)";
static char *DRIVERLNAME = "divas";
static char *DEVNAME = "Divas";
char *DRIVERRELEASE_DIVAS = "3.1.6-109.75-1";

extern irqreturn_t diva_os_irq_wrapper (void *context);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t __diva_os_irq_wrapper (int irq, void *context, struct pt_regs *regs) {
	return (diva_os_irq_wrapper (context));
}
#else
static irqreturn_t __diva_os_irq_wrapper (int irq, void *context) {
	return (diva_os_irq_wrapper (context));
}
#endif

extern int create_divas_proc(void);
extern void remove_divas_proc(void);
extern int divasfunc_init(int dbgmask);
extern void divasfunc_exit(void);
extern void DIVA_DIDD_Read (void *, int);
extern int diva_get_feature_bytes (dword* data, dword data_length, byte Bus, byte Slot, void* hdev, int pcie);

typedef struct _diva_os_thread_dpc {
	diva_os_soft_isr_t *psoft_isr;
#if defined(DIVA_XDI_USES_TASKLETS)
	struct tasklet_struct divas_tasklet;
#else
	struct work_struct divas_task;
  atomic_t soft_isr_disabled;
  atomic_t soft_isr_count;
#endif
} diva_os_thread_dpc_t;

struct _diva_os_usermode_proc;
typedef struct _diva_os_usermode_proc {
	struct work_struct  divas_task;
	diva_os_spin_lock_t request_lock;
	struct semaphore user_mode_helper_lock;
	dword request[4];
	dword request_locked[4];
} diva_os_usermode_proc_t;
static diva_os_usermode_proc_t diva_os_usermode_proc;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
typedef void* diva_work_queue_fn_parameter_t;
#else
typedef struct work_struct* diva_work_queue_fn_parameter_t;
#endif

/* --------------------------------------------------------------------------
    PCI driver interface section
   -------------------------------------------------------------------------- */
/*
  vendor, device	Vendor and device ID to match (or PCI_ANY_ID)
  subvendor,	Subsystem vendor and device ID to match (or PCI_ANY_ID)
  subdevice
  class,		Device class to match. The class_mask tells which bits
  class_mask	of the class are honored during the comparison.
  driver_data	Data private to the driver.
  */

#if !defined(PCI_DEVICE_ID_EICON_MAESTRAP_2)
#define PCI_DEVICE_ID_EICON_MAESTRAP_2       0xE015
#endif

#if !defined(PCI_DEVICE_ID_EICON_4BRI_VOIP)
#define PCI_DEVICE_ID_EICON_4BRI_VOIP        0xE016
#endif

#if !defined(PCI_DEVICE_ID_EICON_4BRI_2_VOIP)
#define PCI_DEVICE_ID_EICON_4BRI_2_VOIP      0xE017
#endif

#if !defined(PCI_DEVICE_ID_EICON_BRI2M_2)
#define PCI_DEVICE_ID_EICON_BRI2M_2          0xE018
#endif

#if !defined(PCI_DEVICE_ID_EICON_MAESTRAP_2_VOIP)
#define PCI_DEVICE_ID_EICON_MAESTRAP_2_VOIP  0xE019
#endif

#if !defined(PCI_DEVICE_ID_EICON_2F)
#define PCI_DEVICE_ID_EICON_2F               0xE01A
#endif

#if !defined(PCI_DEVICE_ID_EICON_BRI2M_2_VOIP)
#define PCI_DEVICE_ID_EICON_BRI2M_2_VOIP     0xE01B
#endif

#if !defined(PCI_DEVICE_ID_EICON_PRI_V3)
#define PCI_DEVICE_ID_EICON_PRI_V3           0xE01C
#endif

#if !defined(PCI_DEVICE_ID_EICON_2PRI)
#define PCI_DEVICE_ID_EICON_2PRI             0xE01E
#endif

#if !defined(PCI_DEVICE_ID_EICON_4PRI)
#define PCI_DEVICE_ID_EICON_4PRI             0xE020
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_2P)
#define PCI_DEVICE_ID_EICON_ANALOG_2P        0xE022
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_4P)
#define PCI_DEVICE_ID_EICON_ANALOG_4P        0xE024
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_8P)
#define PCI_DEVICE_ID_EICON_ANALOG_8P        0xE028
#endif

#if !defined(PCI_DEVICE_ID_EICON_IPMEDIA_300)
#define PCI_DEVICE_ID_EICON_IPMEDIA_300      0xE02A
#endif

#if !defined(PCI_DEVICE_ID_EICON_IPMEDIA_600)
#define PCI_DEVICE_ID_EICON_IPMEDIA_600      0xE02C
#endif

#if !defined(PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE)
#define PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE      0xE02E
#endif

#if !defined(PCI_DEVICE_ID_2PRI_PCIE)
#define PCI_DEVICE_ID_2PRI_PCIE              0xE030
#endif

#if !defined(PCI_DEVICE_DIVA_BRI_V1_PCIE)
#define PCI_DEVICE_DIVA_BRI_V1_PCIE          0xE032
#endif

#if !defined(PCI_DEVICE_ID_EICON_BRI_CTI)
#define PCI_DEVICE_ID_EICON_BRI_CTI          0xE034
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_2P_PCIE)
#define PCI_DEVICE_ID_EICON_ANALOG_2P_PCIE   0xE036
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_4P_PCIE)
#define PCI_DEVICE_ID_EICON_ANALOG_4P_PCIE   0xE038
#endif

#if !defined(PCI_DEVICE_ID_EICON_ANALOG_8P_PCIE)
#define PCI_DEVICE_ID_EICON_ANALOG_8P_PCIE   0xE03A
#endif

#if !defined(PCI_DEVICE_ID_EICON_4PRI_FS_PCIE)
#define PCI_DEVICE_ID_EICON_4PRI_FS_PCIE    0xE03C
#endif

/*
  This table should be sorted by PCI device ID
  */
static struct pci_device_id divas_pci_tbl[] = {
/* Diva Server BRI-2M PCI 0xE010 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRA,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_MAESTRA_PCI},
/* Diva Server 4BRI-8M PCI 0xE012 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRAQ,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_Q_8M_PCI},
/* Diva Server 4BRI-8M 2.0 PCI 0xE013 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRAQ_U,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_Q_8M_V2_PCI},
/* Diva Server PRI-30M PCI 0xE014 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRAP,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_P_30M_PCI},
/* Diva Server PRI 2.0 adapter 0xE015 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRAP_2,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_P_30M_V2_PCI},
/* Diva Server Voice 4BRI-8M PCI 0xE016 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4BRI_VOIP,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_VOICE_Q_8M_PCI},
/* Diva Server Voice 4BRI-8M 2.0 PCI 0xE017 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4BRI_2_VOIP,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI},
/* Diva Server BRI-2M 2.0 PCI 0xE018 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_BRI2M_2,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_B_2M_V2_PCI},
/* Diva Server Voice PRI 2.0 PCI 0xE019 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_MAESTRAP_2_VOIP,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0,
         CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI},
/* Diva Server 2FX 0xE01A */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_2F,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_B_2F_PCI},
/* Diva Server Voice BRI-2M 2.0 PCI 0xE01B */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_BRI2M_2_VOIP,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI},
/* Diva Server PRI 3.0 adapter, PCI 0xE01C */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_PRI_V3,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_P_30M_V30_PCI},
/* Diva Server 2PRI adapter, PCI 0xE01E */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_2PRI,
	 PCI_VENDOR_ID_EICON, 0x1E01, 0, 0, CARDTYPE_DIVASRV_2P_M_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_2PRI,
	 PCI_VENDOR_ID_EICON, 0x1E02, 0, 0, CARDTYPE_DIVASRV_2P_V_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_2PRI,
	 PCI_VENDOR_ID_EICON, 0x1E03, 0, 0, CARDTYPE_DIVASRV_2P_M_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_2PRI,
	 PCI_VENDOR_ID_EICON, 0xE01E, 0, 0, CARDTYPE_DIVASRV_2P_V_V10_PCI},
/* Diva Server 4PRI adapter, PCI 0xE020 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI,
	 PCI_VENDOR_ID_EICON, 0x2001, 0, 0, CARDTYPE_DIVASRV_4P_M_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI,
	 PCI_VENDOR_ID_EICON, 0x2002, 0, 0, CARDTYPE_DIVASRV_4P_V_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI,
	 PCI_VENDOR_ID_EICON, 0x2003, 0, 0, CARDTYPE_DIVASRV_4P_M_V10_PCI},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI,
	 PCI_VENDOR_ID_EICON, 0xE020, 0, 0, CARDTYPE_DIVASRV_4P_V_V10_PCI},
/* Diva Server Analog 2 Port adapter, PCI 0xE022 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_2P,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_ANALOG_2PORT},
/* Diva Server Analog 4 Port adapter, PCI 0xE024 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_4P,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_ANALOG_4PORT},
/* Diva Server Analog 8 Port adapter, PCI 0xE028 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_8P,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_ANALOG_8PORT},
/* Diva Server IPMedia 300, PCI 0xE02A */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_IPMEDIA_300,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_IPMEDIA_300_V10},
/* Diva Server IPMedia 600, PCI 0xE02C */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_IPMEDIA_600,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_IPMEDIA_600_V10},

/* Diva 4BRI-8 PCIe v1, PCI 0xE02E */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x2E01, 0, 0, CARDTYPE_DIVASRV_4BRI_V1_V_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x2E02, 0, 0, CARDTYPE_DIVASRV_4BRI_V1_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x2E03, 0, 0, CARDTYPE_DIVASRV_4BRI_V1_V_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_DIVA_4BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE02E, 0, 0, CARDTYPE_DIVASRV_4BRI_V1_PCIE},

/* Diva V-2PRI-60 PCIe, PCI 0xE030 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_2PRI_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3001, 0, 0, CARDTYPE_DIVASRV_V2P_V10H_PCIE},
/* Diva V-1PRI-30 PCIe */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_2PRI_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3002, 0, 0, CARDTYPE_DIVASRV_V1P_V10H_PCIE},
/* Dialogic Diva V-4PRI-120 PCIe v1 half size, PCI 0xE030 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_2PRI_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE030, 0, 0, CARDTYPE_DIVASRV_V4P_V10H_PCIE},

/* Diva BRI-2 PCIe v1, PCI 0xE032 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_DIVA_BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3201, 0, 0, CARDTYPE_DIVASRV_BRI_V1_V_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_DIVA_BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3202, 0, 0, CARDTYPE_DIVASRV_BRI_V1_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_DIVA_BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3203, 0, 0, CARDTYPE_DIVASRV_BRI_V1_V_PCIE},
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_DIVA_BRI_V1_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE032, 0, 0, CARDTYPE_DIVASRV_BRI_V1_PCIE},

/* Diva Server BRI-CTI 0xE034 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_BRI_CTI,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, CARDTYPE_DIVASRV_BRI_CTI_V2_PCI},

/* Diva Server V-Analog 2 Port PCIe adapter, PCI 0xE036 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_2P_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3601, 0, 0, CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE},
/* Diva Server Analog 2 Port PCIe adapter, PCI 0xE036 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_2P_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE036, 0, 0, CARDTYPE_DIVASRV_ANALOG_2P_PCIE},
/* Diva Server V-Analog 4 Port PCIe adapter, PCI 0xE038 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_4P_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3801, 0, 0, CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE},
/* Diva Server Analog 4 Port PCIe adapter, PCI 0xE038 */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_4P_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE038, 0, 0, CARDTYPE_DIVASRV_ANALOG_4P_PCIE},
/* Diva Server V-Analog 8 Port PCIe adapter, PCI 0xE03A */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_8P_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3A01, 0, 0, CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE},
/* Diva Server Analog 8 Port PCIe adapter, PCI 0xE03A */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_ANALOG_8P_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE03A, 0, 0, CARDTYPE_DIVASRV_ANALOG_8P_PCIE},
/* Diva V-8PRI PCIe FS v1, PCI 0xE03C */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI_FS_PCIE,
	 PCI_VENDOR_ID_EICON, 0x3C01, 0, 0, CARDTYPE_DIVASRV_V8P_V10Z_PCIE},
/* Diva V-4PRI PCIe FS v1, PCI 0xE03C */
	{PCI_VENDOR_ID_EICON, PCI_DEVICE_ID_EICON_4PRI_FS_PCIE,
	 PCI_VENDOR_ID_EICON, 0xE03C, 0, 0, CARDTYPE_DIVASRV_V4P_V10Z_PCIE},

	{0,}			/* 0 terminated list. */
};
MODULE_DEVICE_TABLE(pci, divas_pci_tbl);

static int divas_init_one(struct pci_dev *pdev,
			  const struct pci_device_id *ent);
static void divas_remove_one(struct pci_dev *pdev);

typedef struct _diva_allocated_dma_descriptor {
	diva_entity_link_t link;
	dma_addr_t         dma_handle;
	void*              addr;
} diva_allocated_dma_descriptor_t;

typedef struct _diva_allocated_dma_descriptors {
	diva_entity_queue_t descriptor_q;
	dword               free_descriptors;
	dword               new_descriptors;
	struct semaphore    lock;
	int                 opened;
} diva_allocated_dma_descriptors_t;
static diva_allocated_dma_descriptors_t diva_allocated_dma_descriptors;
static struct device *divas_device = NULL;
static struct class *divas_class = NULL;

static dword diva_create_allocated_dma_descriptors  (diva_allocated_dma_descriptors_t* dma, dword nr);
static void  diva_release_allocated_dma_descriptors (diva_allocated_dma_descriptors_t* dma);

#if (defined(CONFIG_PCIEAER) && defined(DIVA_XDI_AER_SUPPORT))
static pci_ers_result_t diva_io_error_detected (struct pci_dev *pdev, pci_channel_state_t state);
static pci_ers_result_t diva_io_slot_reset (struct pci_dev *pdev);
static void diva_io_resume(struct pci_dev *pdev);

static struct pci_error_handlers diva_err_handler = {
  .error_detected = diva_io_error_detected,
  .slot_reset     = diva_io_slot_reset,
  .resume         = diva_io_resume,
};
#endif

static struct pci_driver diva_pci_driver = {
	.name     = "divas",
	.probe    = divas_init_one,
	.remove   = divas_remove_one,
	.id_table = divas_pci_tbl,
#if (defined(CONFIG_PCIEAER) && defined(DIVA_XDI_AER_SUPPORT))
	.err_handler = &diva_err_handler,
#endif
};

/*********************************************************
 ** little helper functions
 *********************************************************/
static char *getrev(const char *revision)
{
	char *rev;
	char *p;
	if ((p = strchr(revision, ':'))) {
		rev = p + 2;
		p = strchr(rev, '$');
		*--p = 0;
	} else
		rev = "1.0";
	return rev;
}

void diva_log_info(unsigned char *format, ...)
{
	va_list args;
	unsigned char line[160];

	va_start(args, format);
	vsprintf(line, format, args);
	va_end(args);

	printk(KERN_INFO "%s: %s\n", DRIVERLNAME, line);
}

void divas_get_version(char *p)
{
	char tmprev[32];

	strcpy(tmprev, main_revision);
	sprintf(p, "%s: %s(%s) %s(%s) major=%d\n", DRIVERLNAME, DRIVERRELEASE_DIVAS,
		getrev(tmprev), diva_xdi_common_code_build, DIVA_BUILD, major);
}

void diva_os_read_descriptor_array (void* t, int length) {
	DIVA_DIDD_Read (t, length);
}

/* --------------------------------------------------------------------------
    PCI Bus services
   -------------------------------------------------------------------------- */
byte diva_os_get_pci_bus(void *pci_dev_handle)
{
	struct pci_dev *pdev = (struct pci_dev *) pci_dev_handle;
	return ((byte) pdev->bus->number);
}

byte diva_os_get_pci_func(void *pci_dev_handle)
{
	struct pci_dev *pdev = (struct pci_dev *) pci_dev_handle;
	return ((byte) pdev->devfn);
}

unsigned int divasa_get_pci_irq(unsigned char bus, unsigned char func,
				 void *pci_dev_handle)
{
	unsigned int irq = 0;
	struct pci_dev *dev = (struct pci_dev *) pci_dev_handle;

	irq = dev->irq;

	return (irq);
}

unsigned long divasa_get_pci_bar(unsigned char bus, unsigned char func,
				 int bar, void *pci_dev_handle)
{
	unsigned long ret = 0;
	struct pci_dev *dev = (struct pci_dev *) pci_dev_handle;

	if (bar < 6) {
		ret = dev->resource[bar].start;
	}

	DBG_TRC(("GOT BAR[%d]=%08x", bar, ret));

	{
		unsigned long type = (ret & 0x00000001);
		if (type & PCI_BASE_ADDRESS_SPACE_IO) {
			DBG_TRC(("  I/O"));
			ret &= PCI_BASE_ADDRESS_IO_MASK;
		} else {
			DBG_TRC(("  memory"));
			ret &= PCI_BASE_ADDRESS_MEM_MASK;
		}
		DBG_TRC(("  final=%08x", ret));
	}

	return (ret);
}

void PCIwrite(byte bus, byte func, int offset, void *data, int length,
	      void *pci_dev_handle)
{
	struct pci_dev *dev = (struct pci_dev *) pci_dev_handle;

	switch (length) {
	case 1:		/* byte */
		pci_write_config_byte(dev, offset,
				      *(unsigned char *) data);
		break;
	case 2:		/* word */
		pci_write_config_word(dev, offset,
				      *(unsigned short *) data);
		break;
	case 4:		/* dword */
		pci_write_config_dword(dev, offset,
				       *(unsigned int *) data);
		break;

	default:		/* buffer */
		if (!(length % 4) && !(length & 0x03)) {	/* Copy as dword */
			dword *p = (dword *) data;
			length /= 4;

			while (length--) {
				pci_write_config_dword(dev, offset,
						       *(unsigned int *)
						       p++);
			}
		} else {	/* copy as byte stream */
			byte *p = (byte *) data;

			while (length--) {
				pci_write_config_byte(dev, offset,
						      *(unsigned char *)
						      p++);
			}
		}
	}
}

void PCIread(byte bus, byte func, int offset, void *data, int length,
	     void *pci_dev_handle)
{
	struct pci_dev *dev = (struct pci_dev *) pci_dev_handle;

	switch (length) {
	case 1:		/* byte */
		pci_read_config_byte(dev, offset, (unsigned char *) data);
		break;
	case 2:		/* word */
		pci_read_config_word(dev, offset, (unsigned short *) data);
		break;
	case 4:		/* dword */
		pci_read_config_dword(dev, offset, (unsigned int *) data);
		break;

	default:		/* buffer */
		if (!(length % 4) && !(length & 0x03)) {	/* Copy as dword */
			dword *p = (dword *) data;
			length /= 4;

			while (length--) {
				pci_read_config_dword(dev, offset,
						      (unsigned int *)
						      p++);
			}
		} else {	/* copy as byte stream */
			byte *p = (byte *) data;

			while (length--) {
				pci_read_config_byte(dev, offset,
						     (unsigned char *)
						     p++);
			}
		}
	}
}

/*
  Init map with DMA pages. It is not problem if some allocations fail -
  the channels that will not get one DMA page will use standard PIO
  interface
  */
static void *diva_pci_alloc_consistent(struct pci_dev *hwdev,
				       size_t size,
				       dma_addr_t * dma_handle,
				       void **addr_handle)
{
	void *addr;

	if (hwdev == NULL) {
		return NULL;
	}

	addr = dma_alloc_coherent(&hwdev->dev, size, dma_handle, GFP_ATOMIC);

	*addr_handle = addr;

	return (addr);
}

void diva_init_dma_map_pages(void *hdev,
		       struct _diva_dma_map_entry **ppmap, int nentries, int pages)
{
	struct pci_dev *pdev = (struct pci_dev *) hdev;
	struct _diva_dma_map_entry *pmap =
	    diva_alloc_dma_map(hdev, nentries, 0);

	if (pmap) {
		int i;
		dma_addr_t dma_handle;
		void *cpu_addr;
		void *addr_handle;

		for (i = 0; i < nentries; i++) {
			if (!(cpu_addr = diva_pci_alloc_consistent(pdev,
								   pages*PAGE_SIZE,
								   &dma_handle,
								   &addr_handle)))
			{
				DBG_ERR(("Failed to allocate descriptor: %d", i))
				break;
			}
			diva_init_dma_map_entry(pmap, i, cpu_addr,
						(dword) dma_handle,
						addr_handle);
			if ((((qword)dma_handle) >> 32) == 0) {
				DBG_TRC(("dma map alloc [%d]=(%08lx:%08x:%08lx) %u",
								i, (unsigned long) cpu_addr, (dword) dma_handle, (unsigned long) addr_handle, pages*PAGE_SIZE))
			} else {
				diva_init_dma_map_entry_hi (pmap, i, (dword)(((qword)dma_handle) >> 32));
				DBG_TRC(("dma map alloc [%d]=(%08lx:lo %08x:hi %08x:%08lx) %u",
								i, (unsigned long)cpu_addr, (dword)dma_handle,
								(dword)(((qword)dma_handle) >> 32), (unsigned long) addr_handle, pages*PAGE_SIZE))
			}
		}

		if (i == 0) {
			diva_free_dma_mapping(pmap, 0);
			pmap = 0;
		}
	}

	*ppmap = pmap;
}

void diva_init_dma_map(void *hdev,
		       struct _diva_dma_map_entry **ppmap, int nentries)
{
  diva_init_dma_map_pages(hdev, ppmap, nentries, 1);
}

/*
  Free all contained in the map entries and memory used by the map
  Should be always called after adapter removal from DIDD array
  */
void diva_free_dma_map_pages(void *hdev, struct _diva_dma_map_entry *pmap, int pages)
{
	struct pci_dev *pdev = (struct pci_dev *) hdev;
	int i;
	unsigned long phys_addr;
	void *cpu_addr;
	dma_addr_t dma_handle;
	unsigned long addr_hi;
	void *addr_handle;

	for (i = 0; (pmap != 0); i++) {
		diva_get_dma_map_entry(pmap, i, &cpu_addr, &phys_addr);
		if (!cpu_addr) {
			break;
		}
		addr_handle = diva_get_entry_handle(pmap, i);

		diva_get_dma_map_entry_hi (pmap, i, &addr_hi);

		dma_handle = (dma_addr_t)(((qword)phys_addr) | (((qword)addr_hi) << 32));

		dma_free_coherent(pdev == NULL ? NULL : &pdev->dev, pages*PAGE_SIZE, addr_handle,
				    dma_handle);
		DBG_TRC(("dma map free [%d]=(%08lx:lo %08x:hi %08x:%08lx)", i,
			 (unsigned long) cpu_addr, (dword) dma_handle, (dword)(((qword)dma_handle) >> 32),
			 (unsigned long) addr_handle))
	}

	diva_free_dma_mapping(pmap, 0);
}

void diva_free_dma_map(void *hdev, struct _diva_dma_map_entry *pmap)
{
	diva_free_dma_map_pages(hdev, pmap, 1);
}

static dword diva_create_allocated_dma_descriptors  (diva_allocated_dma_descriptors_t* dma, dword nr) {
	if (dma->free_descriptors >= nr) {
		dma->free_descriptors -= nr;
		dma->new_descriptors  += nr;
	} else {
		diva_allocated_dma_descriptor_t* descriptor;

		nr                    -= dma->free_descriptors;
		dma->new_descriptors  += dma->free_descriptors;
		dma->free_descriptors = 0;

		do {
			if ((descriptor = (diva_allocated_dma_descriptor_t*)diva_os_malloc (0, sizeof(*descriptor))) != 0) {
				if ((descriptor->addr = dma_alloc_coherent(divas_device, PAGE_SIZE, &descriptor->dma_handle, GFP_ATOMIC)) != 0) {
					sprintf ((char*)descriptor->addr,
									 "%s %08x:%08x", "DESCRIPTOR:",
									 (dword)(((qword)descriptor->dma_handle) >> 32),
									 (dword)(descriptor->dma_handle & 0xffffffffU));
					diva_q_add_tail (&dma->descriptor_q, &descriptor->link);
					dma->new_descriptors++;
				} else {
					diva_os_free (0, descriptor);
					descriptor = 0;
				}
			}
		} while (descriptor != 0 && --nr > 0);
	}

	if (nr != 0) {
		DBG_ERR(("failed to alloc %d dma descriptors", nr))
	}

	return (dma->new_descriptors);
}

static void diva_release_allocated_dma_descriptors (diva_allocated_dma_descriptors_t* dma) {
	diva_entity_link_t* link;
	int nr = 0;

	while ((link = diva_q_get_head (&dma->descriptor_q)) != 0) {
		diva_allocated_dma_descriptor_t* descriptor =
					DIVAS_CONTAINING_RECORD(link, diva_allocated_dma_descriptor_t, link);

		dma_free_coherent (divas_device, PAGE_SIZE, descriptor->addr, descriptor->dma_handle);
		diva_q_remove (&dma->descriptor_q, &descriptor->link);
		diva_os_free (0, descriptor);
		nr++;
	}

	dma->free_descriptors = \
	dma->new_descriptors  = 0;
}

/*********************************************************
 ** I/O port utilities
 *********************************************************/

int
diva_os_register_io_port(void *adapter, int on, unsigned long port,
			 unsigned long length, const char *name)
{
	if (on) {
		if (!request_region(port, length, name)) {
			DBG_ERR(("A: I/O: can't register port=%08x", port))
			return (-1);
		}
	} else {
		release_region(port, length);
	}
	return (0);
}

void *divasa_remap_pci_bar(struct _diva_os_xdi_adapter *a, unsigned long bar, unsigned long area_length)
{
	void *ret;

	ret = (void *) ioremap(bar, area_length);
	DBG_TRC(("remap(%08x)->%p", bar, ret));
	return (ret);
}

void divasa_unmap_pci_bar(void *bar)
{
	if (bar) {
		iounmap(bar);
	}
}

/*********************************************************
 ** I/O port access
 *********************************************************/
byte __inline__ inpp(void *addr)
{
	return (inb((unsigned long) addr));
}

word __inline__ inppw(void *addr)
{
	return (inw((unsigned long) addr));
}

void __inline__ inppw_buffer(void *addr, void *P, int length)
{
	insw((unsigned long) addr, (word *) P, length >> 1);
}

void __inline__ outppw_buffer(void *addr, void *P, int length)
{
	outsw((unsigned long) addr, (word *) P, length >> 1);
}

void __inline__ outppw(void *addr, word w)
{
	outw(w, (unsigned long) addr);
}

void __inline__ outpp(void *addr, word p)
{
	outb(p, (unsigned long) addr);
}

/* --------------------------------------------------------------------------
    IRQ request / remove
   -------------------------------------------------------------------------- */
int diva_os_register_irq(void *context, unsigned int irq, const char *name, int msi)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	int result = request_irq (irq, __diva_os_irq_wrapper, SA_SHIRQ, name, context);
#else
	int result = request_irq (irq, __diva_os_irq_wrapper, (msi == 0) ? IRQF_SHARED : 0, name, context);
#endif

	return (result);
}

void diva_os_remove_irq (void *context, unsigned int irq)
{
	free_irq (irq, context);
}

/* --------------------------------------------------------------------------
    DPC framework implementation
   -------------------------------------------------------------------------- */
#if defined(DIVA_XDI_USES_TASKLETS)
static void diva_os_dpc_proc (unsigned long context) {
	diva_os_thread_dpc_t *psoft_isr = DIVAS_CONTAINING_RECORD(((char*)context),
																														diva_os_thread_dpc_t,
																														divas_tasklet);
	diva_os_soft_isr_t *pisr = psoft_isr->psoft_isr;

	(*(pisr->callback))(pisr, pisr->callback_context);
}
#else
static void diva_os_dpc_proc (diva_work_queue_fn_parameter_t context) {
	diva_os_thread_dpc_t *psoft_isr = DIVAS_CONTAINING_RECORD(((char*)context),
																														diva_os_thread_dpc_t,
																														divas_task);
	diva_os_soft_isr_t *pisr = psoft_isr->psoft_isr;

  if (atomic_read (&(psoft_isr->soft_isr_disabled)) == 0) {
    if (atomic_inc_and_test(&(psoft_isr->soft_isr_count)) == 1) {
      do {
        (*(pisr->callback)) (pisr, pisr->callback_context);
      } while (atomic_add_negative(-1, &(psoft_isr->soft_isr_count)) == 0);
    }
  }
}
#endif

int diva_os_initialize_soft_isr(diva_os_soft_isr_t * psoft_isr,
				diva_os_soft_isr_callback_t callback,
				void *callback_context)
{
	diva_os_thread_dpc_t *pdpc;

	pdpc = (diva_os_thread_dpc_t *) diva_os_malloc(0, sizeof(*pdpc));
	if (!(psoft_isr->object = pdpc)) {
		return (-1);
	}
	memset(pdpc, 0x00, sizeof(*pdpc));
	psoft_isr->callback = callback;
	psoft_isr->callback_context = callback_context;
	pdpc->psoft_isr = psoft_isr;

#if defined(DIVA_XDI_USES_TASKLETS)
	tasklet_init (&pdpc->divas_tasklet,
                diva_os_dpc_proc,
                (unsigned long)&pdpc->divas_tasklet);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&pdpc->divas_task, diva_os_dpc_proc, &pdpc->divas_task);
#else
	INIT_WORK(&pdpc->divas_task, diva_os_dpc_proc);
#endif
  atomic_set (&(pdpc->soft_isr_count), -1);
  atomic_set (&(pdpc->soft_isr_disabled), 0);
#endif

	return (0);
}

int diva_os_schedule_soft_isr(diva_os_soft_isr_t * psoft_isr)
{
	if (psoft_isr && psoft_isr->object) {
		diva_os_thread_dpc_t *pdpc =
		    (diva_os_thread_dpc_t *) psoft_isr->object;

#if defined(DIVA_XDI_USES_TASKLETS)
		tasklet_schedule(&pdpc->divas_tasklet);
#else
		schedule_work(&pdpc->divas_task);
#endif
	}

	return (1);
}

int diva_os_cancel_soft_isr(diva_os_soft_isr_t * psoft_isr)
{

#if defined(DIVA_XDI_USES_TASKLETS)
	if (psoft_isr && psoft_isr->object) {
		diva_os_thread_dpc_t *pdpc =
		    (diva_os_thread_dpc_t *) psoft_isr->object;
		tasklet_disable(&pdpc->divas_tasklet);
		flush_scheduled_work();
		tasklet_enable(&pdpc->divas_tasklet);
	}
#else
	flush_scheduled_work();
#endif
	return (0);
}

void diva_os_remove_soft_isr(diva_os_soft_isr_t * psoft_isr)
{
	if (psoft_isr && psoft_isr->object) {
		diva_os_thread_dpc_t *pdpc =
		    (diva_os_thread_dpc_t *) psoft_isr->object;
#if defined(DIVA_XDI_USES_TASKLETS)
		tasklet_kill(&pdpc->divas_tasklet);
#else
    atomic_set (&(pdpc->soft_isr_disabled), 1);
#endif
		flush_scheduled_work();
		diva_os_free(0, pdpc);
	}
}

static dword get_usermode_request (void) {
	diva_os_spin_lock_magic_t irql;
	dword index;
	dword bit;

	diva_os_enter_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");
	for (index = 0;
			 index < sizeof(diva_os_usermode_proc.request)/sizeof(diva_os_usermode_proc.request[0]);
			 index++) {
		for (bit = 0; bit < 32; bit++) {
			if ((diva_os_usermode_proc.request_locked[index] & (1U << bit)) == 0 &&
					(diva_os_usermode_proc.request[index] & (1U << bit)) != 0) {
				diva_os_usermode_proc.request_locked[index] |= (1U << bit);
				diva_os_usermode_proc.request[index]        &= ~(1U << bit);
				diva_os_leave_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");

				return (index*32U+bit);
			}
		}
	}
	diva_os_leave_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");

	return (0xffffffff);
}

static void free_usermode_request (dword adapter_nr) {
	diva_os_spin_lock_magic_t irql;
	dword index = adapter_nr/32;
	dword bit   = adapter_nr%32;

	diva_os_enter_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");
	diva_os_usermode_proc.request_locked[index] &= ~(1U << bit);
	diva_os_leave_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");
}

static void diva_os_process_usermode_request (diva_work_queue_fn_parameter_t context) {
	dword adapter_nr;

	if (down_trylock(&diva_os_usermode_proc.user_mode_helper_lock) == 0) {
		while ((adapter_nr = get_usermode_request ()) != 0xffffffff) {
			char *argv[2] = { NULL, NULL };
			char *envp[5] = { NULL, NULL, NULL, NULL, NULL};
			char data[]   = "DIVA_ADAPTER=    ";
			int ret;

			sprintf (strchr(data, ' '), "%d", adapter_nr);

			argv[0] = "/usr/lib/eicon/divas/divas_usermode_helper";

			envp[0] = "HOME=/";
			envp[1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
			envp[2] = data;
			envp[3] = "DIVA_ADAPTER_COMMAND=debug";

			if ((ret = call_usermodehelper(argv[0], argv, envp, 1)) != 0) {
				DBG_ERR(("Call to %s %s %s failed (%d)", argv[0], envp[2], envp[3], ret))
			}

			free_usermode_request (adapter_nr);
		}
		up(&diva_os_usermode_proc.user_mode_helper_lock);
	}
}

void diva_os_shedule_user_mode_event (dword adapter_nr, diva_user_mode_event_type_t event_type) {
	dword index = adapter_nr/32;
	dword bit   = adapter_nr%32;

	if (index < sizeof(diva_os_usermode_proc.request)/sizeof(diva_os_usermode_proc.request[0])) {
		diva_os_spin_lock_magic_t irql;

		diva_os_enter_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");
		if ((diva_os_usermode_proc.request_locked[index] & (1U << bit)) == 0) {
			diva_os_usermode_proc.request[index] |= (1U << bit);
		}
		diva_os_leave_spin_lock (&diva_os_usermode_proc.request_lock, &irql, "user mode event");
		schedule_work (&diva_os_usermode_proc.divas_task);
	}
}

/*
 * kernel/user space copy functions
 */
static int
xdi_copy_to_user(void *os_handle, void *dst, const void *src, int length)
{
	if (copy_to_user(dst, src, length)) {
		return (-EFAULT);
	}
	return (length);
}

static int
xdi_copy_from_user(void *os_handle, void *dst, const void *src, int length)
{
	if (copy_from_user(dst, src, length)) {
		return (-EFAULT);
	}
	return (length);
}

/*
 * device node operations
 */
static int divas_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	int ret = 0;

	if (minor == 1) {
		down(&diva_allocated_dma_descriptors.lock);
		if (diva_allocated_dma_descriptors.opened == 0) {
			diva_allocated_dma_descriptors.opened = 1;
			file->private_data = &diva_allocated_dma_descriptors;
			/*
				Necessary to free allocated DMA descriptors or this is not possible
				to map it to user space using /dev/mem. Necessary to investigate why
				Sleep 100 mSec to ensure Diva adapter does not access resources any more
				*/
			if (diva_q_get_head (&diva_allocated_dma_descriptors.descriptor_q) != 0) {
				diva_os_sleep (100);
			}
			diva_release_allocated_dma_descriptors (&diva_allocated_dma_descriptors);
		} else {
			ret = -EBUSY;
		}
		up (&diva_allocated_dma_descriptors.lock);
	}

	return (ret);
}

static int divas_release(struct inode *inode, struct file *file)
{
	if (file->private_data == &diva_allocated_dma_descriptors) {
		/*
			Do not free allocated by application DMA descriptors. This is still
			possible that Diva adapter still descriptors.
			Only mark all descriptors as new.
			*/
		down(&diva_allocated_dma_descriptors.lock);
		diva_allocated_dma_descriptors.opened = 0;
		diva_allocated_dma_descriptors.free_descriptors = \
		diva_allocated_dma_descriptors.new_descriptors  = \
						diva_q_get_nr_of_entries (&diva_allocated_dma_descriptors.descriptor_q);
		DBG_LOG(("free %d allocated dma descriptors", diva_allocated_dma_descriptors.free_descriptors))
		up (&diva_allocated_dma_descriptors.lock);
	} else if (file->private_data != 0) {
		diva_xdi_close_adapter(file->private_data, file);
	}
	return (0);
}

static ssize_t divas_write(struct file *file, const char *buf,
			   size_t count, loff_t * ppos)
{
	int ret = -EINVAL;

	if (file->private_data == &diva_allocated_dma_descriptors) {
		diva_allocated_dma_descriptors_t* dma = &diva_allocated_dma_descriptors;

		down (&dma->lock);
		if (dma->new_descriptors == 0) {

			if (count == 2*sizeof(dword)) {
				dword nr_dma_descriptors[2];

				if (copy_from_user (nr_dma_descriptors, buf, 2*sizeof(dword)) == 0) {
					if (nr_dma_descriptors[0] == DIVA_XDI_UM_CMD_CREATE_XDI_DESCRIPTORS) {
						if (diva_create_allocated_dma_descriptors  (dma, nr_dma_descriptors[1]) != 0) {
							ret = 2*sizeof(dword);
						} else {
							ret = -ENOMEM;
						}
					} else {
						ret = -EINVAL;
					}
				} else {
					ret = -EFAULT;
				}
			} else {
				ret = -EINVAL;
			}
		} else {
			ret = -EBUSY;
		}
		up (&dma->lock);

		return (ret);
	} else if (file->private_data == 0) {
		file->private_data = diva_xdi_open_adapter(file, buf,
							   count,
							   xdi_copy_from_user);
	}
	if (!file->private_data) {
		return (-ENODEV);
	}

	ret = diva_xdi_write(file->private_data, file,
			     buf, count, xdi_copy_from_user);
	switch (ret) {
	case -1:		/* Message should be removed from rx mailbox first */
		ret = -EBUSY;
		break;
	case -2:		/* invalid adapter was specified in this call */
		ret = -ENOMEM;
		break;
	case -3:
		ret = -ENXIO;
		break;
	}
	DBG_TRC(("write: ret %d", ret));
	return (ret);
}

static ssize_t divas_read(struct file *file, char *buf,
			  size_t count, loff_t * ppos)
{
	int ret = -EINVAL;

	if (file->private_data == &diva_allocated_dma_descriptors) {
		diva_allocated_dma_descriptors_t* dma = &diva_allocated_dma_descriptors;

		down (&dma->lock);
		if (dma->new_descriptors != 0) {
			if (count >= (3*sizeof(dword)+sizeof(dma_addr_t))) {
				diva_entity_link_t* link = diva_q_get_tail (&dma->descriptor_q);
				dword hdr[3], nr = dma->new_descriptors;

				while (nr-- > 1) {
					link = diva_q_get_prev (link);
				}

				ret    = sizeof(hdr);
				count -= sizeof(hdr);
				nr = 0;

				do {
					diva_allocated_dma_descriptor_t* descriptor =
							DIVAS_CONTAINING_RECORD(link, diva_allocated_dma_descriptor_t, link);
					link = diva_q_get_next (&descriptor->link);

					if (copy_to_user (&buf[ret], &descriptor->dma_handle, sizeof(dma_addr_t)) == 0) {
						ret    += sizeof(dma_addr_t);
						count  -= sizeof(dma_addr_t);
					} else {
						ret = -EFAULT;
					}
				} while (ret > 0 && (dma->new_descriptors > ++nr) && count >= sizeof(dma_addr_t));

				if (ret > 0) {
					hdr[0] = DIVA_XDI_UM_CMD_CREATE_XDI_DESCRIPTORS;
					hdr[1] = nr;
					hdr[2] = (dword)sizeof(dma_addr_t);
					if (copy_to_user (buf, hdr, sizeof(hdr)) == 0) {
						dma->new_descriptors -= nr;
					} else {
						ret = -EFAULT;
					}
				}
			} else {
				ret = -EMSGSIZE;
			}
		} else {
			ret = -EAGAIN;
		}
		up (&dma->lock);

		return (ret);
	} else if (file->private_data == 0) {
		file->private_data = diva_xdi_open_adapter(file, buf,
							   count,
							   xdi_copy_from_user);
	}
	if (!file->private_data) {
		return (-ENODEV);
	}

	ret = diva_xdi_read(file->private_data, file,
			    buf, count, xdi_copy_to_user);
	switch (ret) {
	case -1:		/* RX mailbox is empty */
		ret = -EAGAIN;
		break;
	case -2:		/* no memory, mailbox was cleared, last command is failed */
		ret = -ENOMEM;
		break;
	case -3:		/* can't copy to user, retry */
		ret = -EFAULT;
		break;
	}
	DBG_TRC(("read: ret %d", ret));
	return (ret);
}

static unsigned int divas_poll(struct file *file, poll_table * wait)
{
	if (!file->private_data) {
		return (POLLERR);
	}
	return (POLLIN | POLLRDNORM);
}

static struct file_operations divas_fops = {
	.owner   = THIS_MODULE,
	.llseek  = no_llseek,
	.read    = divas_read,
	.write   = divas_write,
	.poll    = divas_poll,
	.open    = divas_open,
	.release = divas_release
};

static void divas_unregister_chrdev(void)
{
#if defined(CONFIG_DEVFS_FS)
	devfs_remove(DEVNAME);
#endif
	unregister_chrdev(major, DEVNAME);
}

void diva_os_get_time(dword * sec, dword * usec) {
	ktime_t tv;

	tv = ktime_get_real();

	*sec  = tv / 1000000000;
	*usec = tv / 1000;
	*usec = (*sec * 1000000) - *usec;
}

static int DIVA_INIT_FUNCTION divas_register_chrdev(void)
{
	if ((major = register_chrdev(0, DEVNAME, &divas_fops)) < 0)
	{
		printk(KERN_ERR "%s: failed to create /dev entry.\n",
		       DRIVERLNAME);
		return (0);
	}
#if defined(CONFIG_DEVFS_FS)
	devfs_mk_cdev(MKDEV(major, 0), S_IFCHR|S_IRUSR|S_IWUSR, DEVNAME);
#endif

	return (1);
}

/* --------------------------------------------------------------------------
    PCI driver section
   -------------------------------------------------------------------------- */
#ifndef __devinit
#define __devinit
#endif
static int __devinit divas_init_one(struct pci_dev *pdev,
				    const struct pci_device_id *ent)
{
	void *pdiva = 0;
	u8 pci_latency;
	u8 new_latency = 32;
	u16 pci_status = 0;
	int msi = 0;

	DBG_TRC(("%s bus: %08x fn: %08x insertion",
		 CardProperties[ent->driver_data].Name,
		 pdev->bus->number, pdev->devfn))
	printk(KERN_INFO "%s: %s bus: %08x fn: %08x insertion.\n",
		DRIVERLNAME, CardProperties[ent->driver_data].Name,
		pdev->bus->number, pdev->devfn);

	if (pci_enable_device(pdev)) {
		DBG_ERR(("%s bus: %08x fn: %08x device init failed",
			 CardProperties[ent->driver_data].Name,
			 pdev->bus->number,
			 pdev->devfn))
		printk(KERN_ERR
			"%s: %s bus: %08x fn: %08x device init failed.\n",
			DRIVERLNAME,
			CardProperties[ent->driver_data].
			Name, pdev->bus->number,
			pdev->devfn);
		return (-EIO);
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
	if (sizeof(dma_addr_t) == sizeof(qword) && disabledac == 0) {
		int use_dac = 0;

		if (ent->driver_data == CARDTYPE_DIVASRV_P_30M_V30_PCI) {
			dword data[64];

			if (diva_get_feature_bytes (data, sizeof(data), pdev->bus->number, pdev->devfn, pdev, 0) == 0) {
				if (((byte)data[62]) != 0xff) {
					use_dac = (((data[62] >> 8) & 0x01) == 0);
					if (use_dac == 0) {
						DBG_LOG(("Please update adapter NVRAM to activate support for 64Bit PCIe addressing"))
					}
				} else {
					use_dac =
						(CardProperties[ent->driver_data].HardwareFeatures & DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC) != 0;
				}
			}
		} else {
			use_dac = (CardProperties[ent->driver_data].HardwareFeatures & DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC) != 0;
		}

		if (use_dac != 0) {
			DBG_LOG(("bus: %08x fn: %08x set dma mask to %s", pdev->bus->number, pdev->devfn, "DMA_64BIT_MASK"))
			if (dma_set_mask_and_coherent(&pdev->dev, DMA_64BIT_MASK) != 0) {
				DBG_ERR(("bus: %08x fn: %08x failed to set dma mask to %s",
					pdev->bus->number, pdev->devfn, "DMA_64BIT_MASK"))
			}
		}
	}
#endif

	pci_read_config_word(pdev, PCI_STATUS, &pci_status);
	if ((pci_status & PCI_STATUS_REC_MASTER_ABORT) != 0) {
		pci_status = PCI_STATUS_REC_MASTER_ABORT;
		DBG_ERR(("bus: %08x fn: %08x clear master error", pdev->bus->number, pdev->devfn))
		pci_write_config_word(pdev, PCI_STATUS, pci_status);
	}

	pci_set_master(pdev);

#if (defined(CONFIG_PCIEAER) && defined(DIVA_XDI_AER_SUPPORT))
	if (pci_enable_pcie_error_reporting(pdev) == 0) {
		DBG_LOG(("bus: %08x fn: %08x enable AER", pdev->bus->number, pdev->devfn))
		printk(KERN_INFO "%s: bus: %08x fn: %08x enable AER.\n",
					 DRIVERLNAME, pdev->bus->number, pdev->devfn);
	}
#endif

  if ((CardProperties[ent->driver_data].HardwareFeatures &
      (DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE | DIVA_CARDTYPE_HARDWARE_PROPERTY_CUSTOM_PCIE)) == 0) {
		pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &pci_latency);
		if (!pci_latency) {
			DBG_TRC(("%s: bus: %08x fn: %08x fix latency.\n",
			 	DRIVERLNAME, pdev->bus->number, pdev->devfn))
			printk(KERN_INFO
				"%s: bus: %08x fn: %08x fix latency.\n",
			 	DRIVERLNAME, pdev->bus->number, pdev->devfn);
			pci_write_config_byte(pdev, PCI_LATENCY_TIMER, new_latency);
		}
  }

#ifdef CONFIG_PCI_MSI
	msi = ((no_msi == 0) && (pci_enable_msi (pdev) == 0));
#endif

	if (!(pdiva = diva_driver_add_card(pdev, ent->driver_data, msi))) {
		DBG_TRC(("%s: %s bus: %08x fn: %08x card init failed.\n",
			 DRIVERLNAME,
			 CardProperties[ent->driver_data].Name,
			 pdev->bus->number,
			 pdev->devfn))
		printk(KERN_ERR
			"%s: %s bus: %08x fn: %08x card init failed.\n",
			DRIVERLNAME,
			CardProperties[ent->driver_data].
			Name, pdev->bus->number,
			pdev->devfn);
#ifdef CONFIG_PCI_MSI
		if (msi != 0) {
			pci_disable_msi (pdev);
		}
#endif
		return (-EIO);
	}

	pci_set_drvdata(pdev, pdiva);

	return (0);
}

#if (defined(CONFIG_PCIEAER) && defined(DIVA_XDI_AER_SUPPORT))
/**
 * diva_io_error_detected - called when PCI error is detected
 *
 * This function is called after a PCI bus error affecting
 * this device has been detected.
 *
 * \note Avoid PCI I/O
 *
 */
static pci_ers_result_t diva_io_error_detected (struct pci_dev *pdev /**< Pointer to PCI device */,
																			pci_channel_state_t state /**< The current pci connection state */) {
	void *pdiva = pci_get_drvdata(pdev);
	pci_ers_result_t ret = PCI_ERS_RESULT_DISCONNECT;

	DBG_ERR(("bus: %08x fn: %08x PCI error detected", pdev->bus->number, pdev->devfn))
	printk(KERN_ERR "%s: bus: %08x fn: %08x PCI error detected.\n", DRIVERLNAME, pdev->bus->number, pdev->devfn);

	if (pdiva != 0) {
		if (diva_driver_stop_card_no_io (pdiva) == 0) {
			ret = PCI_ERS_RESULT_NEED_RESET;
		}
	}

	pci_disable_device(pdev);

	return (ret);
}

/**
 * diva_io_slot_reset - called after the pci bus has been reset.
 *
 * Restart the card from scratch, as if from a cold-boot.
 * At this point, the card has exprienced a hard reset,
 * followed by fixups by BIOS, and has its config space
 * set up identically to what it was at cold boot.
 */
static pci_ers_result_t diva_io_slot_reset (struct pci_dev *pdev /**< Pointer to PCI device */) {
	DBG_ERR(("bus: %08x fn: %08x PCI slot reset", pdev->bus->number, pdev->devfn))
	printk(KERN_ERR "%s: bus: %08x fn: %08x PCI slot reset.\n", DRIVERLNAME, pdev->bus->number, pdev->devfn);

	if (pci_enable_device(pdev)) {
		DBG_ERR(("bus: %08x fn: %08x Cannot re-enable PCI device after reset", pdev->bus->number, pdev->devfn))
		printk(KERN_ERR "%s: bus: %08x fn: %08x Cannot re-enable PCI device after reset.\n",
					 DRIVERLNAME, pdev->bus->number, pdev->devfn);

		return (PCI_ERS_RESULT_DISCONNECT);
  }

  pci_set_master(pdev);

#ifdef CONFIG_PCI_MSI
	if (no_msi == 0) {
		pci_enable_msi (pdev);
	}
#endif

	return (PCI_ERS_RESULT_RECOVERED);
}

/**
 * diva_io_resume - called when traffic can start flowing again.
 *
 * This callback is called when the error recovery driver tells
 * us that its OK to resume normal operation.
 */
static void diva_io_resume(struct pci_dev *pdev /**< Pointer to PCI device */) {
	DBG_LOG(("bus: %08x fn: %08x PCI device resume after reset", pdev->bus->number, pdev->devfn))
	printk(KERN_INFO "%s: bus: %08x fn: %08x PCI device resume after reset.\n",
				 DRIVERLNAME, pdev->bus->number, pdev->devfn);

	/**
		\todo This is necessary to call user mode helper
					/usr/lib/eicon/divas/divas_cfg.rc restart N, where N is the adapter number
	*/
}
#endif

#ifndef __devexit
#define __devexit
#endif
static void __devexit divas_remove_one(struct pci_dev *pdev)
{
	void *pdiva = pci_get_drvdata(pdev);


	if (pdiva) {
		int msi = 0;

		diva_driver_remove_card(pdiva, &msi);

#ifdef CONFIG_PCI_MSI
		if (msi != 0) {
			pci_disable_msi (pdev);
		}
#endif

		DBG_TRC(("bus: %08x fn: %08x msi:%d removal.\n",
						pdev->bus->number, pdev->devfn, msi))
		printk(KERN_INFO "%s: bus: %08x fn: %08x msi:%d removal.\n",
					 DRIVERLNAME, pdev->bus->number, pdev->devfn, msi);
	} else {
		DBG_ERR(("bus: %08x fn: %08x msi:%d removal.\n",
						pdev->bus->number, pdev->devfn, -1))
		printk(KERN_ERR "%s: bus: %08x fn: %08x msi:%d removal.\n",
					 DRIVERLNAME, pdev->bus->number, pdev->devfn, -1);
	}

}

/* --------------------------------------------------------------------------
    Driver Load / Startup
   -------------------------------------------------------------------------- */
static int DIVA_INIT_FUNCTION divas_init(void)
{
	char tmprev[50];
	int ret = 0;
	int destroy_spin_lock = 0;

	printk(KERN_INFO "%s\n", DRIVERNAME);
	printk(KERN_INFO "%s: Rel:%s  Rev:", DRIVERLNAME, DRIVERRELEASE_DIVAS);
	strcpy(tmprev, main_revision);
	printk("%s  Build: %s(%s)\n", getrev(tmprev),
	       diva_xdi_common_code_build, DIVA_BUILD);
	printk(KERN_INFO "%s: support for: ", DRIVERLNAME);
#ifdef CONFIG_ISDN_DIVAS_BRIPCI
	printk("BRI/PCI ");
#endif
#ifdef CONFIG_ISDN_DIVAS_PRIPCI
	printk("PRI/PCI ");
#endif
	printk("adapters\n");

	divas_class = class_create(THIS_MODULE, "divas");
	if (IS_ERR(divas_class)) {
		printk(KERN_ERR "%s: failed to create character device class.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	divas_device = device_create(divas_class, NULL, MKDEV(0, 0), NULL, "divas");
	if (IS_ERR(divas_device)) {
		printk(KERN_ERR "%s: failed to create character device.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (dma_coerce_mask_and_coherent(divas_device, DMA_BIT_MASK(24))) {
		printk(KERN_ERR "%s: failed to set DMA mask.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (diva_os_initialize_spin_lock (&diva_os_usermode_proc.request_lock, "init")) {
		destroy_spin_lock = 1;
		printk(KERN_ERR "%s: failed to connect initialize spin lock.\n",
		       DRIVERLNAME);
		ret = -EIO;
		goto out;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
	init_MUTEX(&diva_os_usermode_proc.user_mode_helper_lock);
#else
	sema_init(&diva_os_usermode_proc.user_mode_helper_lock, 1);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&diva_os_usermode_proc.divas_task,
						diva_os_process_usermode_request,
						&diva_os_usermode_proc.divas_task);
#else
	INIT_WORK(&diva_os_usermode_proc.divas_task, diva_os_process_usermode_request);
#endif

	if (!divasfunc_init(dbgmask)) {
		printk(KERN_ERR "%s: failed to connect to DIDD.\n",
		       DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (!divas_register_chrdev()) {
#ifdef MODULE
		divasfunc_exit();
#endif
		ret = -EIO;
		goto out;
	}

	if (!create_divas_proc()) {
#ifdef MODULE
		remove_divas_proc();
		divas_unregister_chrdev();
		divasfunc_exit();
#endif
		printk(KERN_ERR "%s: failed to create proc entry.\n",
		       DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
	if ((ret = pci_module_init(&diva_pci_driver))) {
#else
	if ((ret = pci_register_driver(&diva_pci_driver))) {
#endif
#ifdef MODULE
		remove_divas_proc();
		divas_unregister_chrdev();
		divasfunc_exit();
#endif
		printk(KERN_ERR "%s: failed to init pci driver.\n",
		       DRIVERLNAME);
		goto out;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
	init_MUTEX(&diva_allocated_dma_descriptors.lock);
#else
	sema_init(&diva_allocated_dma_descriptors.lock, 1);
#endif

	printk(KERN_INFO "%s: started with major %d\n", DRIVERLNAME, major);

out:

	if (destroy_spin_lock != 0) {
		diva_os_destroy_spin_lock (&diva_os_usermode_proc.request_lock, "init");
	}

	return (ret);
}

/* --------------------------------------------------------------------------
    Driver Unload
   -------------------------------------------------------------------------- */
static void DIVA_EXIT_FUNCTION divas_exit(void)
{
	pci_unregister_driver(&diva_pci_driver);
	remove_divas_proc();
	divas_unregister_chrdev();
	divasfunc_exit();

	device_destroy(divas_class, MKDEV(0, 0));
	class_destroy(divas_class);

	flush_scheduled_work();
	diva_os_destroy_spin_lock (&diva_os_usermode_proc.request_lock, "init");

	/*
		Diva server adapters are stopped now and user mode SoftIP driver
		is not running. Release allocated DMA descriptors
		*/
	diva_release_allocated_dma_descriptors (&diva_allocated_dma_descriptors);

	printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

module_init(divas_init);
module_exit(divas_exit);
