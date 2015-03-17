/* $Id: divasproc.c,v 1.18 2003/09/09 06:46:29 schindler Exp $
 *
 * Low level driver for Dialogic DIVA Server ISDN cards.
 * /proc functions
 *
 * Copyright 2000-2009 by Armin Schindler (mac@melware.de)
 * Copyright 2000-2009 Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007 Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>

#include "platform.h"
#include "debuglib.h"
#include "dlist.h"
#undef N_DATA
#include "pc.h"
#include "di_defs.h"
#include "divasync.h"
#include "di.h"
#include "io.h"
#include "xdi_msg.h"
#include "xdi_adapter.h"
#include "diva.h"
#include "diva_pci.h"


extern PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
extern diva_entity_queue_t adapter_queue;
extern void divas_get_version(char *);
extern void diva_get_vserial_number(PISDN_ADAPTER IoAdapter, char *buf);

/*********************************************************
 ** Functions for /proc interface / File operations
 *********************************************************/

static char *divas_proc_name = "divas";
static char *adapter_dir_name = "adapter";
static char *info_proc_name = "info";
static char *grp_opt_proc_name = "group_optimization";
static char *d_l1_down_proc_name = "dynamic_l1_down";

/*
** "divas" entry
*/

extern struct proc_dir_entry *proc_net_eicon;
static struct proc_dir_entry *divas_proc_entry = NULL;
static int
info_write(struct file *file, const char *buffer, unsigned long count, void *data);

static int
divas_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int cadapter;
	char tmpbuf[80];

	divas_get_version(tmpbuf);
	len = strlen (tmpbuf);
	memcpy (page, tmpbuf, len);

	for (cadapter = 0; cadapter < MAX_ADAPTER; cadapter++) {
		if (IoAdapters[cadapter]) {
			diva_get_vserial_number(IoAdapters[cadapter], tmpbuf);
			len += sprintf (page+len,
											"%2d: %-30s Serial:%-10s IRQ:%2d\n",
											cadapter + 1,
											IoAdapters[cadapter]->Properties.Name,
											tmpbuf,
											IoAdapters[cadapter]->irq_info.irq_nr);
		}
	}

	if (off + count >= len)
		*eof = 1;

	if (len < off)
		return 0;

	*start = page + off;

	return ((count < len - off) ? count : len - off);
}

int create_divas_proc(void)
{
	divas_proc_entry = create_proc_entry(divas_proc_name,
					     S_IFREG | S_IRUGO,
					     proc_net_eicon);
	if (!divas_proc_entry)
		return (0);

	divas_proc_entry->write_proc = info_write;
	divas_proc_entry->read_proc  = divas_read;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	divas_proc_entry->owner = THIS_MODULE;
#endif
	divas_proc_entry->data       = 0;

	return (1);
}

void remove_divas_proc(void)
{
	if (divas_proc_entry) {
		remove_proc_entry(divas_proc_name, proc_net_eicon);
		divas_proc_entry = NULL;
	}
}

/*
** write group_optimization 
*/
static int
write_grp_opt(struct file *file, const char *user_data, unsigned long count,
	      void *data)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];
  char info;

	if (count == 0 || count > 2) {
		return (-EINVAL);
	}
	if (get_user(info, user_data) != 0) {
		return (-EFAULT);
	}

	switch (info) {
		case '0':
			IoAdapter->capi_cfg.cfg_1 &=
			    ~DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON;
			break;
		case '1':
			IoAdapter->capi_cfg.cfg_1 |=
			    DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON;
			break;
		default:
			return (-EINVAL);
	}

	return (count);
}

/*
** write dynamic_l1_down
*/
static int
write_d_l1_down(struct file *file, const char *user_data, unsigned long count,
		void *data)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];
	char info;

	if (count == 0 || count > 2) {
		return (-EINVAL);
	}
	if (get_user(info, user_data) != 0) {
		return (-EFAULT);
	}

	switch (info) {
		case '0':
			IoAdapter->capi_cfg.cfg_1 &=
			    ~DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON;
			break;
		case '1':
			IoAdapter->capi_cfg.cfg_1 |=
			    DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON;
			break;
		default:
			return (-EINVAL);
	}

	return (count);
}


/*
** read dynamic_l1_down 
*/
static int
read_d_l1_down(char *page, char **start, off_t off, int count, int *eof,
	       void *data)
{
	int len = 0;
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len += sprintf(page + len, "%s\n",
		       (IoAdapter->capi_cfg.
			cfg_1 & DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON) ? "1" :
		       "0");

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

/*
** read group_optimization
*/
static int
read_grp_opt(char *page, char **start, off_t off, int count, int *eof,
	     void *data)
{
	int len = 0;
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len += sprintf(page + len, "%s\n",
		       (IoAdapter->capi_cfg.
			cfg_1 & DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON)
		       ? "1" : "0");

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

/*
** info write
*/
static int
info_write(struct file *file, const char *buffer, unsigned long count,
	   void *data)
{
	return (-EIO);
}

/*
** info read
*/
static int
info_read(char *page, char **start, off_t off, int count, int *eof,
	  void *data)
{
	int i = 0;
	int len = 0;
	char *p;
	char tmpser[16];
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len +=
	    sprintf(page + len, "Name        : %s\n",
		    IoAdapter->Properties.Name);
	len += sprintf(page + len, "DSP state   : %08x\n", a->dsp_mask);
	len += sprintf(page + len, "Channels    : %02d\n",
		       IoAdapter->Properties.Channels);
	len += sprintf(page + len, "E. max/used : %03d/%03d\n",
		       IoAdapter->e_max, IoAdapter->e_count);
	diva_get_vserial_number(IoAdapter, tmpser);
	len += sprintf(page + len, "Serial      : %s\n", tmpser);
	len +=
	    sprintf(page + len, "IRQ         : %d\n",
		    IoAdapter->irq_info.irq_nr);
	len += sprintf(page + len, "CardIndex   : %d\n", a->CardIndex);
	len += sprintf(page + len, "CardOrdinal : %d\n", a->CardOrdinal);
	len += sprintf(page + len, "Controller  : %d\n", a->controller);
	len += sprintf(page + len, "Bus-Type    : %s\n",
		       (a->Bus ==
			DIVAS_XDI_ADAPTER_BUS_ISA) ? "ISA" : "PCI");
	len += sprintf(page + len, "Port-Name   : %s\n", a->port_name);
	if (a->Bus == DIVAS_XDI_ADAPTER_BUS_PCI) {
		len +=
		    sprintf(page + len, "PCI-bus     : %d\n",
			    a->resources.pci.bus);
		len +=
		    sprintf(page + len, "PCI-func    : %d\n",
			    a->resources.pci.func);
		for (i = 0; i < 8; i++) {
			if (a->resources.pci.bar[i]) {
				len +=
				    sprintf(page + len,
					    "Mem / I/O %d : 0x%x / mapped : 0x%lx",
					    i, a->resources.pci.bar[i],
					    (unsigned long) a->resources.
					    pci.addr[i]);
				if (a->resources.pci.length[i]) {
					len +=
					    sprintf(page + len,
						    " / length : %d",
						    a->resources.pci.
						    length[i]);
				}
				len += sprintf(page + len, "\n");
			}
		}
	}
	if ((!a->xdi_adapter.port) &&
	    ((!a->xdi_adapter.ram) ||
	    (!a->xdi_adapter.reset)
	     || (!a->xdi_adapter.cfg))) {
		if (!IoAdapter->irq_info.irq_nr) {
			p = "slave";
		} else {
			p = "out of service";
		}
	} else if (a->xdi_adapter.trapped) {
		p = "trapped";
	} else if (a->xdi_adapter.Initialized) {
		p = "active";
	} else {
		p = "ready";
	}
	len += sprintf(page + len, "State       : %s\n", p);

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}

/*
** adapter proc init/de-init
*/

/* --------------------------------------------------------------------------
    Create adapter directory and files in proc file system
   -------------------------------------------------------------------------- */
int create_adapter_proc(diva_os_xdi_adapter_t * a)
{
	struct proc_dir_entry *de, *pe;
	char tmp[16];

	sprintf(tmp, "%s%d", adapter_dir_name, a->controller);
	if (!(de = create_proc_entry(tmp, S_IFDIR, proc_net_eicon)))
		return (0);
	a->proc_adapter_dir = (void *) de;

	if (!(pe =
	     create_proc_entry(info_proc_name, S_IFREG | S_IRUGO | S_IWUSR, de)))
		return (0);
	a->proc_info = (void *) pe;
	pe->write_proc = info_write;
	pe->read_proc = info_read;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	pe->owner = THIS_MODULE;
#endif
	pe->data = a;

	if ((pe = create_proc_entry(grp_opt_proc_name,
			       S_IFREG | S_IRUGO | S_IWUSR, de))) {
		a->proc_grp_opt = (void *) pe;
		pe->write_proc = write_grp_opt;
		pe->read_proc = read_grp_opt;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
		pe->owner = THIS_MODULE;
#endif
		pe->data = a;
	}
	if ((pe = create_proc_entry(d_l1_down_proc_name,
			       S_IFREG | S_IRUGO | S_IWUSR, de))) {
		a->proc_d_l1_down = (void *) pe;
		pe->write_proc = write_d_l1_down;
		pe->read_proc = read_d_l1_down;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
		pe->owner = THIS_MODULE;
#endif
		pe->data = a;
	}

	DBG_TRC(("proc entry %s created", tmp));

	return (1);
}

/* --------------------------------------------------------------------------
    Remove adapter directory and files in proc file system
   -------------------------------------------------------------------------- */
void remove_adapter_proc(diva_os_xdi_adapter_t * a)
{
	char tmp[16];

	if (a->proc_adapter_dir) {
		if (a->proc_d_l1_down) {
			remove_proc_entry(d_l1_down_proc_name,
					  (struct proc_dir_entry *) a->proc_adapter_dir);
		}
		if (a->proc_grp_opt) {
			remove_proc_entry(grp_opt_proc_name,
					  (struct proc_dir_entry *) a->proc_adapter_dir);
		}
		if (a->proc_info) {
			remove_proc_entry(info_proc_name,
					  (struct proc_dir_entry *) a->proc_adapter_dir);
		}
		sprintf(tmp, "%s%d", adapter_dir_name, a->controller);
		remove_proc_entry(tmp, proc_net_eicon);
		DBG_TRC(("proc entry %s%d removed", adapter_dir_name,
			 a->controller));
	}
}
