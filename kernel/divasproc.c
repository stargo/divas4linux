
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
#include <linux/seq_file.h>
#endif

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
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static ssize_t info_write(struct file *file, const char *buffer, size_t count, loff_t *offp);
#else
static int info_write(struct file *file, const char *buffer, unsigned long count, void *data);
#endif
int proc_cnt=0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
int eof[1];
#define PROC_RETURN(x,y) { \
   if (!x) { \
       x=1; \
       return(y); \
   } else { \
       x=0; \
       return(0); \
   } \
}
#define PRINT_BUF(m,b,l) { \
           seq_printf(m, "%s", b); \
           l = 0; \
}
static int divas_read(struct seq_file *m, void *v)
#else
#define PROC_RETURN(x,y) return(y)
#define PRINT_BUF(m,b,l) /* no op for older kernel */
static int divas_read(char *page, char **start, off_t off, int count, int *eof, void *data)
#endif
{
	int len = 0;
	int cadapter;
	char tmpbuf[80];
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	char page[120];
#else
	void *m;
#endif
	divas_get_version(tmpbuf);
	len = strlen (tmpbuf);
	memcpy (page, tmpbuf, len+1);
	PRINT_BUF(m,page,len);

	for (cadapter = 0; cadapter < MAX_ADAPTER; cadapter++) {
		if (IoAdapters[cadapter]) {
			diva_get_vserial_number(IoAdapters[cadapter], tmpbuf);
			len += sprintf (page+len,
						"%2d: %-30s Serial:%-10s IRQ:%2d\n",
						cadapter + 1,
						IoAdapters[cadapter]->Properties.Name,
						tmpbuf,
						IoAdapters[cadapter]->irq_info.irq_nr);
			PRINT_BUF(m,page,len);
		}
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	if (off + count >= len)
		*eof = 1;

	if (len < off)
		return 0;
	*start = page + off;
	PROC_RETURN(proc_cnt,((count < len-off) ? count : len-off));
#else
	return 0;
#endif
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static int divas_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, divas_read, NULL);
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops divas_proc_fops = {
 .proc_open   = divas_proc_open,
 .proc_read   = seq_read,
 .proc_write  = info_write,
 .proc_lseek = seq_lseek,
 .proc_release= single_release
};
#else
static const struct file_operations divas_proc_fops = {
 .owner = THIS_MODULE,
 .open   = divas_proc_open,
 .read   = seq_read,
 .write  = info_write,
 .llseek = seq_lseek,
 .release= single_release
};
#endif
#endif

int create_divas_proc(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	divas_proc_entry = proc_create(divas_proc_name, S_IFREG | S_IRUGO, proc_net_eicon, &divas_proc_fops);
	if (!divas_proc_entry)
		return (0);
#else
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
#endif
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
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define PDE_DATA(inode)	PROC_I(inode)->pde->data
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
#define PDE_DATA(inode) pde_data(inode)
#endif /* < 3.10 */
static ssize_t write_grp_opt(struct file *file, const char *user_data, size_t count, loff_t *offp)
{
	void *data=PDE_DATA(file_inode(file));
#else
static int write_grp_opt(struct file *file, const char *user_data, unsigned long count, void *data)
{
	void *m;
#endif
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
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static ssize_t write_d_l1_down(struct file *file, const char *user_data, size_t count, loff_t *offp)
{
	void *data=PDE_DATA(file_inode(file));
#else
static int write_d_l1_down(struct file *file, const char *user_data, unsigned long count, void *data)
{
#endif
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
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static int read_d_l1_down(struct seq_file *m, void *v)
{
	void *data=m->private;
	char page[100];
#else
static int read_d_l1_down(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	void *m;
#endif
	int len = 0;
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len += sprintf(page + len, "%s\n",
		       (IoAdapter->capi_cfg.
			cfg_1 & DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON) ? "1" :
		       "0");
	PRINT_BUF(m,page,len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	PROC_RETURN(proc_cnt,((count < len-off) ? count : len-off));
#else
	return 0;
#endif
}


/*
** read group_optimization
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static int read_grp_opt(struct seq_file *m, void *v)
{
	void *data=m->private;
	char page[100];
#else
static int read_grp_opt(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	void *m;
#endif
	int len = 0;
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len += sprintf(page + len, "%s\n",
		       (IoAdapter->capi_cfg.
			cfg_1 & DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON)
		       ? "1" : "0");
	PRINT_BUF(m,page,len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	PROC_RETURN(proc_cnt,((count < len-off) ? count : len-off));
#else
	return 0;
#endif
}


/*
** info write
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static ssize_t info_write(struct file *file, const char *buffer, size_t count, loff_t *offp)
#else
static int info_write(struct file *file, const char *buffer, unsigned long count, void *data)
#endif
{
	return (-EIO);
}

/*
** info read
*/
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static int info_read(struct seq_file *m, void *v)
{
	void *data=m->private;
	char page[120];
#else
static int info_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	void *m;
#endif
	int i = 0;
	int len = 0;
	char *p;
	char tmpser[16];
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) data;
	PISDN_ADAPTER IoAdapter = IoAdapters[a->controller - 1];

	len +=
	    sprintf(page + len, "Name        : %s\n",
		    IoAdapter->Properties.Name); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "DSP state   : %08x\n", a->dsp_mask); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "Channels    : %02d\n",
		       IoAdapter->Properties.Channels); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "E. max/used : %03d/%03d\n",
		       IoAdapter->e_max, IoAdapter->e_count); PRINT_BUF(m,page,len);
	diva_get_vserial_number(IoAdapter, tmpser);
	len += sprintf(page + len, "Serial      : %s\n", tmpser); PRINT_BUF(m,page,len);
	len +=
	    sprintf(page + len, "IRQ         : %d\n",
		    IoAdapter->irq_info.irq_nr); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "CardIndex   : %d\n", a->CardIndex); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "CardOrdinal : %d\n", a->CardOrdinal); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "Controller  : %d\n", a->controller); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "Bus-Type    : %s\n",
		       (a->Bus ==
			DIVAS_XDI_ADAPTER_BUS_ISA) ? "ISA" : "PCI"); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "Port-Name   : %s\n", a->port_name); PRINT_BUF(m,page,len);
	if (a->Bus == DIVAS_XDI_ADAPTER_BUS_PCI) {
		len +=
		    sprintf(page + len, "PCI-bus     : %d\n",
			    a->resources.pci.bus); PRINT_BUF(m,page,len); PRINT_BUF(m,page,len);
		len +=
		    sprintf(page + len, "PCI-func    : %d\n",
			    a->resources.pci.func); PRINT_BUF(m,page,len); PRINT_BUF(m,page,len);
		for (i = 0; i < 8; i++) {
			if (a->resources.pci.bar[i]) {
				len +=
				    sprintf(page + len,
					    "Mem / I/O %d : 0x%x / mapped : 0x%lx",
					    i, a->resources.pci.bar[i],
					    (unsigned long) a->resources.pci.addr[i]); PRINT_BUF(m,page,len);
				if (a->resources.pci.length[i]) {
					len +=
					    sprintf(page + len,
						    " / length : %d",
						    a->resources.pci.length[i]); PRINT_BUF(m,page,len);
				}
				len += sprintf(page + len, "\n"); PRINT_BUF(m,page,len);
			}
		}
	}
	if ((!a->xdi_adapter.port) &&
	    ((!a->xdi_adapter.ram) ||
	    (!a->xdi_adapter.reset)
	     /* || (!a->xdi_adapter.cfg) */)) {
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
	len += sprintf(page + len, "State       : %s\n", p); PRINT_BUF(m,page,len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	PROC_RETURN(proc_cnt,((count < len-off) ? count : len-off));
#else
	return 0;
#endif
}

/*
** adapter proc init/de-init
*/

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
static int info_proc_open(struct inode *inode, struct file *filp)
{
	void *data=PDE_DATA(file_inode(filp));
	return single_open(filp, info_read, data);
}
static int grp_opt_proc_open(struct inode *inode, struct file *filp)
{
	void *data=PDE_DATA(file_inode(filp));
	return single_open(filp, read_grp_opt, data);
}
static int d_l1_down_proc_open(struct inode *inode, struct file *filp)
{
	void *data=PDE_DATA(file_inode(filp));
	return single_open(filp, read_d_l1_down, data);
}

static const struct file_operations de_proc_fops = {
 .owner = THIS_MODULE,
};
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops pe_proc_fops = {
 .proc_open   = info_proc_open,
 .proc_read   = seq_read,
 .proc_write  = info_write,
 .proc_lseek = seq_lseek,
 .proc_release= single_release
};
static const struct proc_ops grp_proc_fops = {
 .proc_open   = grp_opt_proc_open,
 .proc_read   = seq_read,
 .proc_write  = write_grp_opt,
 .proc_lseek = seq_lseek,
 .proc_release= single_release
};
static const struct proc_ops d_l1_proc_fops = {
 .proc_open   = d_l1_down_proc_open,
 .proc_read   = seq_read,
 .proc_write  = write_d_l1_down,
 .proc_lseek = seq_lseek,
 .proc_release= single_release
};
#else
static const struct file_operations pe_proc_fops = {
 .owner = THIS_MODULE,
 .open   = info_proc_open,
 .read   = seq_read,
 .write  = info_write,
 .llseek = seq_lseek,
 .release= single_release
};
static const struct file_operations grp_proc_fops = {
 .owner = THIS_MODULE,
 .open   = grp_opt_proc_open,
 .read   = seq_read,
 .write  = write_grp_opt,
 .llseek = seq_lseek,
 .release= single_release
};
static const struct file_operations d_l1_proc_fops = {
 .owner = THIS_MODULE,
 .open   = d_l1_down_proc_open,
 .read   = seq_read,
 .write  = write_d_l1_down,
 .llseek = seq_lseek,
 .release= single_release
};
#endif
#endif

/* --------------------------------------------------------------------------
    Create adapter directory and files in proc file system
   -------------------------------------------------------------------------- */
int create_adapter_proc(diva_os_xdi_adapter_t * a)
{
	struct proc_dir_entry *de, *pe;
	char tmp[16];

	sprintf(tmp, "%s%d", adapter_dir_name, a->controller);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	if (!(de = proc_mkdir_mode(tmp, S_IFDIR, proc_net_eicon)))
		return (0);
	a->proc_adapter_dir = (void *) de;
	if (!(pe = proc_create_data(info_proc_name, S_IFREG | S_IRUGO | S_IWUSR, de, &pe_proc_fops, a)))
		return (0);
	a->proc_info = (void *) pe;
	if (!(pe = proc_create_data(grp_opt_proc_name,S_IFREG | S_IRUGO | S_IWUSR, de, &grp_proc_fops, a)))
		return (0);
	a->proc_grp_opt = (void *) pe;
	if (!(pe = proc_create_data(d_l1_down_proc_name,S_IFREG | S_IRUGO | S_IWUSR, de, &d_l1_proc_fops, a)))
		return (0);
	a->proc_d_l1_down = (void *) pe;
#else
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
#endif
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
