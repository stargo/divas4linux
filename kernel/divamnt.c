
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/console.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/moduleparam.h>
#if defined(CONFIG_DEVFS_FS)
#include <linux/devfs_fs_kernel.h>
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
#include <linux/seq_file.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
#include <linux/panic_notifier.h>
#endif

#include "platform.h"
#include "di_defs.h"
#include "divasync.h"
#include "debug_if.h"

static char *main_revision = "$Revision: 1.32 $";

static int major;

#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("Maint driver for DIVA Server cards");
#endif
#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Cytronics & Melware, Sangoma");
#endif
#ifdef MODULE_SUPPORTED_DEVICE
MODULE_SUPPORTED_DEVICE("DIVA card driver");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

int buffer_length = 128;
module_param(buffer_length, uint, 0);
MODULE_PARM_DESC(buffer_length, "Trace buffer length in kBytes");
unsigned long diva_dbg_mem = 0;
module_param(diva_dbg_mem, ulong, 0);
MODULE_PARM_DESC(diva_dbg_mem, "External device memory address");
int diva_register_notifier = 0;
module_param(diva_register_notifier, uint, 0);
MODULE_PARM_DESC(diva_register_notifier, "Register system notifier (0,1)");

static char *DRIVERNAME =
    "Dialogic DIVA - MAINT module (http://www.melware.net)";
static char *DRIVERLNAME = "diva_mnt";
static char *DEVNAME     = "DivasMAINT";
#if defined(CONFIG_DEVFS_FS)
static char *DEVNAME_DBG = "DivasDBG";
static char *DEVNAME_DBG_IFC = "DivasDBGIFC";
#endif
char *DRIVERRELEASE_MNT = "9.6.8-124.26-1";

static wait_queue_head_t msgwaitq;
static DECLARE_MUTEX(opened_sem);
static int opened;
static int diva_notifier_registered;
static int diva_reboot_notifier_registered;

extern int mntfunc_init(int *, void **, unsigned long);
extern void mntfunc_finit(void);
extern int maint_read_write(void *buf, int count);
extern int maint_um_dbg_read_write (void** ident, const void* buf, int count);
extern void maint_write_system_info (unsigned long event, const char* info, int length);
extern void* mntfunc_add_debug_client (const char* name, const char* version);

static int diva_notifier_proc (struct notifier_block *self,
                               unsigned long event,
                               void *info);

/*
 *  helper functions
 */
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

/*
 * buffer alloc
 */
void *diva_os_malloc_tbuffer(unsigned long flags, unsigned long size)
{
	return (kmalloc(size, 0));
}
void diva_os_free_tbuffer(unsigned long flags, void *ptr)
{
	if (ptr) {
		kfree(ptr);
	}
}

/*
 * kernel/user space copy functions
 */
int diva_os_copy_to_user(void *os_handle, void *dst, const void *src,
			 int length)
{
	return (copy_to_user(dst, src, length));
}
int diva_os_copy_from_user(void *os_handle, void *dst, const void *src,
			   int length)
{
	return (copy_from_user(dst, src, length));
}

/*
 * get time
 */
void diva_os_get_time(dword * sec, dword * usec)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
	unsigned long j = jiffies;
	dword t_sec = (dword)(j/HZ);
	dword t_rem = (dword)(j%HZ);

	*sec = t_sec;
	*usec = jiffies_to_usecs (t_rem);
#else
	struct timeval tv;

	do_gettimeofday (&tv);

	*sec  = tv.tv_sec;
	*usec = tv.tv_usec;
#endif
}

/*
 * /proc entries
 */

extern struct proc_dir_entry *proc_net_eicon;
static struct proc_dir_entry *maint_proc_entry = NULL;
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
static int proc_read(struct seq_file *m, void *v)
#else
#define PROC_RETURN(x,y) return(y)
#define PRINT_BUF(m,b,l) /* no op for older kernel */
static int proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
#endif
{
	int len = 0;
	char tmprev[32];
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	char page[100];
#else
	void *m;
#endif
	strcpy(tmprev, main_revision);
	len += sprintf(page + len, "%s\n", DRIVERNAME); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "name     : %s\n", DRIVERLNAME); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "release  : %s\n", DRIVERRELEASE_MNT); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "build    : %s\n", DIVA_BUILD); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "revision : %s\n", getrev(tmprev)); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "major    : %d\n", major); PRINT_BUF(m,page,len);

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
static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_read, NULL);
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops proc_file_fops = {
 .proc_open   = proc_open,
 .proc_read   = seq_read,
 .proc_lseek = seq_lseek,
 .proc_release= single_release
};
#else
static const struct file_operations proc_file_fops = {
 .owner = THIS_MODULE,
 .open   = proc_open,
 .read   = seq_read,
 .llseek = seq_lseek,
 .release= single_release
};
#endif
#endif

static int DIVA_INIT_FUNCTION create_maint_proc(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	maint_proc_entry = proc_create("maint", S_IFREG | S_IRUGO | S_IWUSR, proc_net_eicon, &proc_file_fops);
	if (!maint_proc_entry)
		return (0);
#else
	maint_proc_entry =
	    create_proc_entry("maint", S_IFREG | S_IRUGO | S_IWUSR,
			      proc_net_eicon);
	if (!maint_proc_entry)
		return (0);

	maint_proc_entry->read_proc = proc_read;
#endif
	return (1);
}

static void remove_maint_proc(void)
{
	if (maint_proc_entry) {
		remove_proc_entry("maint", proc_net_eicon);
		maint_proc_entry = NULL;
	}
}

/*
 * device node operations
 */
static unsigned int maint_poll(struct file *file, poll_table * wait)
{
  unsigned int mask = 0;

  if (file->private_data != 0)
    return (POLLERR);

  poll_wait(file, &msgwaitq, wait);
  mask = POLLOUT | POLLWRNORM;
  if (diva_dbg_q_length()) {
    mask |= POLLIN | POLLRDNORM;
  }
  return (mask);
}

static int maint_open(struct inode *ino, struct file *filep)
{
  int minor = iminor(ino);

  if (minor == 0) {
    down(&opened_sem);
    if (opened) {
      up(&opened_sem);
      return (-EBUSY);
    }
    opened++;
    up(&opened_sem);
    filep->private_data = 0;
  } else if (minor == 1) {
    filep->private_data = (void*)1L;
  } else if (minor == 2) {
		char tmp[32];
		sprintf (tmp, "pid:%u", current->pid);
		if ((filep->private_data = mntfunc_add_debug_client (tmp, "user")) == 0) {
      return (-ENODEV);
    }
  }

  return (0);
}

static int maint_close(struct inode *ino, struct file *filep)
{
  if (filep->private_data == 0) {
    down(&opened_sem);
    opened--;
    up(&opened_sem);
  } else {
    maint_um_dbg_read_write(&filep->private_data, 0, -1);
		filep->private_data = 0;
  }

  return (0);
}

static ssize_t divas_maint_write(struct file *file, const char *buf,
				 size_t count, loff_t * ppos)
{
	if (file->private_data == 0) {
    return (maint_read_write((char *)buf, (int)count));
	} else {
    return (maint_um_dbg_read_write(&file->private_data, (char *)buf, (int)count));
	}
}

static ssize_t divas_maint_read(struct file *file, char *buf,
				size_t count, loff_t * ppos)
{
  if (file->private_data == 0)
    return (maint_read_write(buf, (int) count));

  return (-EINVAL);
}

static struct file_operations divas_maint_fops = {
	.owner   = THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.llseek  = no_llseek,
#endif
	.read    = divas_maint_read,
	.write   = divas_maint_write,
	.poll    = maint_poll,
	.open    = maint_open,
	.release = maint_close
};

static void divas_maint_unregister_chrdev(void)
{
#if defined(CONFIG_DEVFS_FS)
	devfs_remove(DEVNAME_DBG_IFC);
	devfs_remove(DEVNAME_DBG);
	devfs_remove(DEVNAME);
#endif
	unregister_chrdev(major, DEVNAME);
}

static int DIVA_INIT_FUNCTION divas_maint_register_chrdev(void)
{
	if ((major = register_chrdev(0, DEVNAME, &divas_maint_fops)) < 0)
	{
		printk(KERN_ERR "%s: failed to create /dev entry.\n",
		       DRIVERLNAME);
		return (0);
	}
#if defined(CONFIG_DEVFS_FS)
	devfs_mk_cdev(MKDEV(major, 0), S_IFCHR|S_IRUSR|S_IWUSR, DEVNAME);
	devfs_mk_cdev(MKDEV(major, 1), S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, DEVNAME_DBG);
	devfs_mk_cdev(MKDEV(major, 2), S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH, DEVNAME_DBG_IFC);
#endif

	return (1);
}

/*
 * wake up reader
 */
void diva_maint_wakeup_read(void)
{
	wake_up_interruptible(&msgwaitq);
}



static struct notifier_block diva_notifier = {
	.notifier_call	= diva_notifier_proc,
	.priority = (INT_MAX-1)
};

static struct notifier_block diva_reboot_notifier = {
	.notifier_call	= diva_notifier_proc,
};

static int diva_notifier_proc (struct notifier_block *self,
                               unsigned long event,
                               void *info) {
	int reboot = (self == &diva_reboot_notifier);

	if (info == 0 || *(char*)info ==0)
		info = (reboot == 0) ? "System message" : "Reboot";

	maint_write_system_info (event, (char*)info, strlen(info));

	if (reboot == 0 ||
			event == SYS_DOWN || event == SYS_RESTART ||
			event == SYS_HALT || event == SYS_POWER_OFF) {
		maint_write_system_info (event, (char*)0, -1);
	}

	return (NOTIFY_DONE);
}

static void diva_console_write_msg (struct console *con, const char *msg, unsigned int length) {
	if (msg != 0 && *msg != 0 && length > 0) {
		maint_write_system_info (0, msg, (int)length);
	}
}

static struct console diva_maint_console = {
	.flags = CON_ENABLED,
	.write = diva_console_write_msg
};


/*
 *  Driver Load
 */
static int DIVA_INIT_FUNCTION maint_init(void)
{
	char tmprev[50];
	int ret = 0;
	void *buffer = 0;

	init_waitqueue_head(&msgwaitq);

	printk(KERN_INFO "%s\n", DRIVERNAME);
	strcpy(tmprev, main_revision);
	printk(KERN_INFO "%s: Rel:%s  Rev: %s  Build: %s \n", DRIVERLNAME, DRIVERRELEASE_MNT, getrev(tmprev), DIVA_BUILD);

	if (!divas_maint_register_chrdev()) {
		ret = -EIO;
		goto out;
	}
	if (!create_maint_proc()) {
		printk(KERN_ERR "%s: failed to create proc entry.\n",
		       DRIVERLNAME);
		divas_maint_unregister_chrdev();
		ret = -EIO;
		goto out;
	}

	if (!(mntfunc_init(&buffer_length, &buffer, diva_dbg_mem))) {
		printk(KERN_ERR "%s: failed to connect to DIDD.\n",
		       DRIVERLNAME);
		remove_maint_proc();
		divas_maint_unregister_chrdev();
		ret = -EIO;
		goto out;
	}


	if (diva_register_notifier != 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
		diva_notifier_registered = (atomic_notifier_chain_register(&panic_notifier_list, &diva_notifier) == 0);
#else
		diva_notifier_registered = (notifier_chain_register(&panic_notifier_list, &diva_notifier) == 0);
#endif
		register_console(&diva_maint_console);
		diva_reboot_notifier_registered =
			(register_reboot_notifier(&diva_reboot_notifier) == 0);
	}

	printk(KERN_INFO "%s: trace buffer = %p - %d kBytes, %s (Major: %d)%s%s\n",
	       DRIVERLNAME, buffer, (buffer_length / 1024),
	       (diva_dbg_mem == 0) ? "internal" : "external", major,
         (diva_notifier_registered == 0) ? "" : ", notifier registered",
         (diva_reboot_notifier_registered == 0) ? "" : ", reboot notifier registered");

      out:
	return (ret);
}

/*
**  Driver Unload
*/
static void DIVA_EXIT_FUNCTION maint_exit(void)
{
	if (diva_register_notifier != 0) {
		unregister_console(&diva_maint_console);
	}
	if (diva_notifier_registered != 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
		atomic_notifier_chain_unregister(&panic_notifier_list, &diva_notifier);
#else
		notifier_chain_unregister(&panic_notifier_list, &diva_notifier);
#endif
	}
	if (diva_reboot_notifier_registered != 0) {
		unregister_reboot_notifier(&diva_reboot_notifier);
	}
	remove_maint_proc();
	divas_maint_unregister_chrdev();
	mntfunc_finit();

	printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

module_init(maint_init);
module_exit(maint_exit);
