
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
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#if defined(CONFIG_DEVFS_FS)
#include <linux/devfs_fs_kernel.h>
#endif
#include <linux/pci.h>

#include "platform.h"
#include "di_defs.h"
#include "dadapter.h"
#include "divasync.h"
#include "did_vers.h"
#include "cfg_types.h"
#include "cfg_ifc.h"
#include "cfg_notify.h"
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)
#include "net/net_namespace.h"
#define divas_get_proc_net() init_net.proc_net
#else
#define divas_get_proc_net() proc_net
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
#include <linux/seq_file.h>
#endif

static char *main_revision = "$Revision: 1.13 $";

static int major;

static char *DRIVERNAME =
    "Dialogic DIVA - DIDD table (http://www.melware.net)";
static char *DRIVERLNAME = "divadidd";
static char *DEVNAME = "DivasDIDD";
char *DRIVERRELEASE_DIDD = "9.6.8-124.26-1";

static char *main_proc_dir = "eicon";

#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("DIDD table driver for diva drivers");
#endif
#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Cytronics & Melware, Sangoma");
#endif
#ifdef MODULE_SUPPORTED_DEVICE
MODULE_SUPPORTED_DEVICE("Sangoma Diva drivers");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)

extern int diddfunc_init(void);
extern void diddfunc_finit(void);

extern void DIVA_DIDD_Read(void *, int);

static struct proc_dir_entry *proc_didd;
struct proc_dir_entry *proc_net_eicon = NULL;

typedef struct _diva_cfglib_application {
  atomic_t              read_ready;
  void*                 notify_handle;
  diva_section_target_t target;
  wait_queue_head_t     read_wait;
  char*                 didd_info_buffer;
  int                   didd_info_offset;
  int                   didd_info_length;
} diva_cfglib_application_t;

#ifndef EXPORT_SYMBOL_NOVERS
#define EXPORT_SYMBOL_NOVERS EXPORT_SYMBOL
#endif

EXPORT_SYMBOL_NOVERS(DIVA_DIDD_Read);
EXPORT_SYMBOL_NOVERS(proc_net_eicon);

static ssize_t didd_read(struct file *file, char *buf, size_t count,
			   loff_t * offset);
static ssize_t didd_write(struct file *file, const char *buf,
			    size_t count, loff_t * offset);
static unsigned int didd_poll(struct file *file, poll_table * wait);
static int didd_open(struct inode *inode, struct file *file);
static int didd_release(struct inode *inode, struct file *file);
/*
 didd_ifc_lock is used to protect applications against adapter removal 
 and to serialize parallel DIDD requests
*/
static DECLARE_MUTEX(didd_ifc_lock);

/*
  Clock memory
  */
static void*      clock_data_addr;
static dma_addr_t clock_data_bus_addr;

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
	len += sprintf(page + len, "release  : %s\n", DRIVERRELEASE_DIDD); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "build    : %s(%s)\n",
		       diva_didd_common_code_build, DIVA_BUILD); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "revision : %s\n", getrev(tmprev)); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "major    : %d\n", major); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "cfglib   : %d\n", VieLlast); PRINT_BUF(m,page,len);
	len += sprintf(page + len, "cfgstate : %d\n", (diva_cfg_lib_get_storage () != 0) ? 1 : 0); PRINT_BUF(m,page,len);

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
static const struct file_operations main_proc_file_fops = {
 .owner = THIS_MODULE
};
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static const struct proc_ops proc_file_fops = {
 .proc_open   = proc_open,
 .proc_read   = seq_read,
 .proc_lseek = seq_lseek,
 .proc_release= single_release,
#else
static const struct file_operations proc_file_fops = {
 .owner  = THIS_MODULE,
 .open   = proc_open,
 .read   = seq_read,
 .llseek = seq_lseek,
 .release= single_release,
#endif
};
#endif

static int DIVA_INIT_FUNCTION create_proc(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
	proc_net_eicon = proc_mkdir(main_proc_dir, divas_get_proc_net());
#else
	proc_net_eicon = create_proc_entry(main_proc_dir, S_IFDIR, divas_get_proc_net());
#endif
	if (proc_net_eicon) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,9,0)
		proc_didd = proc_create(DRIVERLNAME, S_IFREG | S_IRUGO, proc_net_eicon, &proc_file_fops);
#else
		if ((proc_didd =
		     create_proc_entry(DRIVERLNAME, S_IFREG | S_IRUGO,
				       proc_net_eicon))) {
			proc_didd->read_proc = proc_read;
		}
#endif
		return (1);
	}
	return (0);
}

void diva_os_read_descriptor_array (void* t, int length) {
	DIVA_DIDD_Read (t, length);
}

static struct file_operations divas_didd_fops = {
	.owner   = THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,0)
	.llseek  = no_llseek,
#endif
	.read    = didd_read,
	.write   = didd_write,
	.poll    = didd_poll,
	.open    = didd_open,
	.release = didd_release
};

/*
  CfgLib services
  */
static int
didd_open(struct inode *inode, struct file *file)
{
  diva_cfglib_application_t* appl = (diva_cfglib_application_t*)vmalloc(sizeof(*appl));

  if (appl) {
	
    memset (appl, 0x00, sizeof(*appl));

    appl->target = TargetLast;
    atomic_set (&appl->read_ready, 0);
    init_waitqueue_head(&appl->read_wait);

    file->private_data = appl;
  } else {
    DBG_ERR(("didd_open failed to vmalloc (filp=%p)", file));      	  
    file->private_data = 0;
    return (-ENOMEM);
  }

  return (0);
}

static int
didd_release(struct inode *inode, struct file *file)
{
  diva_cfglib_application_t* appl;

  down(&didd_ifc_lock);
  if ((appl = (diva_cfglib_application_t*)file->private_data)) {
    file->private_data = 0;

    if (appl->notify_handle != 0) {
      diva_cfg_lib_cfg_remove_notify_callback (appl->notify_handle);
    }
    if (appl->didd_info_buffer) {
      vfree (appl->didd_info_buffer);
    }

    vfree (appl);
  }

  up(&didd_ifc_lock);
  
  return (0);
}

static int cfg_lib_notify (void* context, const byte* message, const byte* instance) {
  diva_cfglib_application_t* appl = (diva_cfglib_application_t*)context;

  if (message == 0) {
    atomic_set (&appl->read_ready, 1);
    wake_up_interruptible (&appl->read_wait);
    return ((appl->target == TargetCFGLibDrv) ? 0 : 1);
  } else {
    return (-1);
  }
}

static ssize_t
didd_write(struct file *file, const char *buf, size_t count,
       loff_t * offset)
{
  diva_cfglib_application_t* appl;
  int retval = 0;
  byte* data;

  if (count < 4)
    return (-EINVAL);

  if (!(appl = (diva_cfglib_application_t*)file->private_data)) {
    DBG_ERR(("didd_write null appl (filp=%p)", file));
    return (-ENOMEM);
  }

  if (!(data = (byte*)vmalloc ((unsigned int)count+1))) {
    DBG_ERR(("didd_write vmalloc fault (filp=%p)", file));
    return -ENOMEM;
  }


  if (copy_from_user(data, buf, count)) {
    vfree (data);
    DBG_ERR(("didd_write copy_fm_user error (filp=%p buf=%p cnt=%d)",
           file, buf, count));
    return (-EFAULT);
  }

  data[count] = 0;

  down(&didd_ifc_lock);

  if (appl->notify_handle == 0) {
    /*
      No application is bound to this file descriptor
      */

    if (data[0] == '<') {
      appl->target = TargetCFGLibDrv;
    } else if (count == 4) {
      appl->target = (diva_section_target_t)READ_DWORD(data);
    } else {
      retval = -EINVAL;
    }
    if (retval == 0) {
      if ((appl->notify_handle = diva_cfg_lib_cfg_register_notify_callback (cfg_lib_notify,
                                                                            appl, appl->target))) {
        if (appl->target == TargetCFGLibDrv) {
          if (diva_cfg_lib_write (data)) {
            retval = -EINVAL;
          }
        }
      } else {
        retval = -EINVAL;
      }
    }
  } else {
   /*
     Process application specific request from existing application
     */
    if (appl->target == TargetCFGLibDrv) {
      if (diva_cfg_lib_write (data)) {
        retval = -EINVAL;
      }
    } else if (count == 4) {
      /*
        Application configuration change response
        */
      byte* notify_data;

      diva_cfg_lib_ack_owner_data (appl->target, READ_DWORD(data));
      if (diva_cfg_lib_get_owner_data (&notify_data, appl->target) > 0) {
        atomic_set (&appl->read_ready, 1);
      }
    } else {
      retval = -EINVAL;
    }
  }

  up(&didd_ifc_lock);

  vfree (data);

  return ((retval != 0) ? (ssize_t)retval : (ssize_t)count);
}

static ssize_t
didd_read(struct file *file, char *buf, size_t count, loff_t * offset)
{
  diva_cfglib_application_t* appl;
  int retval = 0;

  appl = (diva_cfglib_application_t*)file->private_data;
  if (!appl) {
    DBG_ERR(("didd_read null appl (filp=%p)", file));	  
    return (-ENOMEM);
  }

  down(&didd_ifc_lock);

  if (appl->notify_handle) {
    if (atomic_read (&appl->read_ready) != 0) {
      if (appl->target == TargetCFGLibDrv) {
        int length = diva_cfg_get_read_buffer_length ();

        if (length && (count >= (size_t)length)) {
          char* data = 0;

          atomic_set (&appl->read_ready, 0);

          length = diva_cfg_lib_read (&data, length);

          if (data) {
            if (copy_to_user (buf, data, length) == 0) {
              retval = length;
            } else {
              DBG_ERR(("didd_read copy_2_user (filp=%p buf=%p len=%d)", 
				      file, buf, length));	  
              retval = -EFAULT;
            }
            diva_os_free (0, data);
          } else {
            retval = -EIO;
          }
        } else {
          retval = -EMSGSIZE;
        }
      } else {
        /*
          Notification read by application
          */
        byte* data;
        int length = diva_cfg_lib_get_owner_data (&data, appl->target);
        if (length > 0) {
          if (count >= (size_t)length) {
            if (copy_to_user (buf, data, length) == 0) {
              retval = length;
              atomic_set (&appl->read_ready, 0);
            } else {
              DBG_ERR(("didd_read diva_cfg_lib_get_owner_data (filp=%p cnt=%d len=%d)", 
				      file, count, length));	  
              retval = -EFAULT;
            }
          } else {
            retval = -EMSGSIZE;
          }
        } else {
          retval = -EIO;
        }
      }
    } else {
      retval = -EIO;
    }
  } else {
    /*
      Request for information about DIDD version
      */
    if (appl->didd_info_buffer == 0) {
      int length = 0, max_length = 1024;
      char* dst = vmalloc (max_length);

      if ((appl->didd_info_buffer = dst)) {
        char tmprev[32];

        diva_snprintf (tmprev, sizeof(tmprev), "%s", main_revision);
        length += diva_snprintf (&dst[length], max_length - length, "%s\n", DRIVERNAME);
        length += diva_snprintf (&dst[length], max_length - length, "name     : %s\n", DRIVERLNAME);
        length += diva_snprintf (&dst[length], max_length - length, "build    : %s(%s)\n",
                                 diva_didd_common_code_build, DIVA_BUILD);
        length += diva_snprintf (&dst[length], max_length - length, "revision : %s\n", getrev(tmprev));
        length += diva_snprintf (&dst[length], max_length - length, "cfglib   : %d\n", VieLlast);
        length += diva_snprintf (&dst[length], max_length - length, "cfgstate : %d\n", 
																									(diva_cfg_lib_get_storage () != 0) ? 1 : 0);

        appl->didd_info_offset = 0;
        appl->didd_info_length = length;
      }
    }

    if (appl->didd_info_buffer) {
      int to_copy = MIN(count, appl->didd_info_length);

      if (to_copy) {
        if (copy_to_user (buf, &appl->didd_info_buffer[appl->didd_info_offset], to_copy) == 0) {
          appl->didd_info_offset += to_copy;
          appl->didd_info_length -= to_copy;
          retval = to_copy;
        } else {
          DBG_ERR(("didd_read copy_2_user (filp=%p buf=%p 2copy=%d)", 
				  file, buf, to_copy));	  
          retval = -EFAULT;
        }
      }
    } else {
      DBG_ERR(("didd_read Null info_buffer (filp=%p)", file));	  
      retval = -ENOMEM;
    }
  }

  up(&didd_ifc_lock);

  return (retval);
}

static unsigned int didd_poll(struct file *file, poll_table * wait)
{
  diva_cfglib_application_t* appl;
  unsigned int retval = 0;

  appl = (diva_cfglib_application_t*)file->private_data;

  if (appl && appl->notify_handle) {
  	poll_wait (file, &appl->read_wait, wait);
    if (atomic_read (&appl->read_ready) != 0) {
      retval = (POLLIN | POLLRDNORM);
    }
  } else {
    retval = POLLERR;
  }

  return (retval);
}

static void divas_didd_unregister_chrdev(void)
{
  if (major >= 0) {
#if defined (CONFIG_DEVFS_FS)
	  devfs_remove(DEVNAME);
#endif
	  unregister_chrdev(major, DEVNAME);
  }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
int diva_get_clock_data (dword* bus_addr_lo,
                         dword* bus_addr_hi,
                         dword* length,
                         void** addr) {
#else
int diva_get_clock_data (dword* bus_addr_lo,
                         dword* bus_addr_hi,
                         dword* length,
                         void** addr,
                         void*  pci_dev_handle) {

	struct device *dev_didd = NULL;
	if (clock_data_addr && pci_dev_handle) {
		struct pci_dev *hwdev = (struct pci_dev *)pci_dev_handle;
		dev_didd = &hwdev->dev;
		/*
		 Set DMA mask to 32-bit
		 */
		if (dma_set_mask(dev_didd, DMA_BIT_MASK(32))) {
			printk(KERN_ERR "%s: No suitable DMA available pci_dev=%p\n", __FUNCTION__, pci_dev_handle);
			goto map_error_handling;
		}

		/*
		 Map Streaming DMA address
		 */
		clock_data_bus_addr = dma_map_single(dev_didd, clock_data_addr, PAGE_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(dev_didd, clock_data_bus_addr )) {
			printk(KERN_ERR "%s: dma_mapping_error lo=%08llx addr=%p length=%ld\n",
			       __FUNCTION__, clock_data_bus_addr,clock_data_addr,PAGE_SIZE);
			goto map_error_handling;
		}
	}
#endif
	if (clock_data_addr != 0 &&
			(sizeof(clock_data_bus_addr) == sizeof(dword) || ((dword)(clock_data_bus_addr >> 32)) == 0)) {
		*bus_addr_lo = (dword)clock_data_bus_addr;
		*addr        = clock_data_addr;
		*length      = PAGE_SIZE;
		DBG_LOG(("%s: lo=%08x addr=%p length=%d",
			       __FUNCTION__, clock_data_bus_addr,clock_data_addr,PAGE_SIZE))
		return (0);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
	else if (clock_data_addr != 0 && clock_data_bus_addr != 0) {
		printk(KERN_ERR "%s: lo=%08llx sz=%ld addr=%p pdev=%p\n",
			__FUNCTION__, clock_data_bus_addr,sizeof(clock_data_bus_addr),clock_data_addr, pci_dev_handle);
		dma_unmap_single(dev_didd, clock_data_bus_addr, PAGE_SIZE, DMA_FROM_DEVICE);
	}
map_error_handling:
#endif
  DBG_ERR(("%s addr=%p", __FUNCTION__, clock_data_addr))
  return (-1);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
int diva_unmap_clock_data (dword bus_addr_lo,
                           dword bus_addr_hi,
                           void* pci_dev_handle) {
	if (bus_addr_lo && pci_dev_handle) {

		struct pci_dev *hwdev = (struct pci_dev *)pci_dev_handle;
		struct device  *dev_didd = &hwdev->dev;
		dma_addr_t     clock_data_bus_addr = (dma_addr_t)bus_addr_lo;
		DBG_LOG(("%s: lo=%08x pci_dev=%p",
			       __FUNCTION__, clock_data_bus_addr,pci_dev_handle))

		dma_unmap_single(dev_didd, clock_data_bus_addr, PAGE_SIZE, DMA_FROM_DEVICE);
		return 0;
	}

	DBG_ERR(("%s lo=%08x pci_dev=%p", __FUNCTION__, bus_addr_lo, pci_dev_handle))
	return (-1);
}
#endif

static int DIVA_INIT_FUNCTION divas_didd_register_chrdev(void)
{
  if ((major = register_chrdev(0, DEVNAME, &divas_didd_fops)) < 0)
  {
    printk(KERN_ERR "%s: failed to create /dev entry.\n",
           DRIVERLNAME);
    return (-1);
  }
#if defined(CONFIG_DEVFS_FS)
  devfs_mk_cdev(MKDEV(major, 0), S_IFCHR|S_IRUSR|S_IWUSR, DEVNAME);
#endif
  return (0);
}

static void remove_proc(void)
{
	remove_proc_entry(DRIVERLNAME, proc_net_eicon);
	remove_proc_entry(main_proc_dir, divas_get_proc_net());
}

static int DIVA_INIT_FUNCTION divadidd_init(void)
{
	char tmprev[32];
	int ret = 0;

	printk(KERN_INFO "%s\n", DRIVERNAME);
	strcpy(tmprev, main_revision);
	printk(KERN_INFO "%s: Rel:%s  Rev: %s  Build:%s(%s)\n", DRIVERLNAME, DRIVERRELEASE_DIDD, getrev(tmprev), diva_didd_common_code_build, DIVA_BUILD);

	if (!create_proc()) {
		printk(KERN_ERR "%s: could not create proc entry\n",
		       DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (!diddfunc_init()) {
		printk(KERN_ERR "%s: failed to connect to DIDD.\n",
		       DRIVERLNAME);
#ifdef MODULE
		remove_proc();
#endif
		ret = -EIO;
		goto out;
	}

  /*
    In case character device was not created
    then no CfgLib services are available, but
    DIDD sill provides remainding functionality
    */
  if (diva_cfg_lib_init () == 0) {
    if (divas_didd_register_chrdev()) {
      diva_cfg_lib_finit ();
      major = -1;
    }
  } else {
    major = -1;
  }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
  if ((clock_data_addr = pci_alloc_consistent (0, PAGE_SIZE, &clock_data_bus_addr)) != 0)
	memset (clock_data_addr, 0x00, PAGE_SIZE);
#else
  clock_data_addr = (void *)get_zeroed_page(GFP_ATOMIC);
#endif

out:

  return (ret);
}

void DIVA_EXIT_FUNCTION divadidd_exit(void)
{
	divas_didd_unregister_chrdev();
	diva_cfg_lib_finit ();
	diddfunc_finit();
	remove_proc();

	if (clock_data_addr != 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
		pci_free_consistent (0, PAGE_SIZE, clock_data_addr, clock_data_bus_addr);
#else
		free_page((unsigned long)clock_data_addr);
#endif
	}

	printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

module_init(divadidd_init);
module_exit(divadidd_exit);
