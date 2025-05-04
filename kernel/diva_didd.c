/* $Id: diva_didd.c,v 1.13 2003/08/27 10:11:21 schindler Exp $
 *
 * DIDD Interface module for Dialogic active cards.
 *
 * Functions are in dadapter.c
 *
 * Copyright 2002-2009 by Armin Schindler (mac@melware.de)
 * Copyright 2002-2009 Cytronics & Melware (info@melware.de)
 * Copyright 2002-2007 Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
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

static char *main_revision = "$Revision: 1.13 $";

static int major;

static char *DRIVERNAME =
    "Dialogic DIVA - DIDD table (http://www.melware.net)";
static char *DRIVERLNAME = "divadidd";
static char *DEVNAME = "DivasDIDD";
char *DRIVERRELEASE_DIDD = "3.1.6-109.75-1";

static char *main_proc_dir = "eicon";

MODULE_DESCRIPTION("DIDD table driver for diva drivers");
MODULE_AUTHOR("Cytronics & Melware, Dialogic");
MODULE_LICENSE("GPL");

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
	int 									didd_info_offset;
	int										didd_info_length;
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
static DECLARE_MUTEX(didd_ifc_lock);
#else
static DEFINE_SEMAPHORE(didd_ifc_lock);
#endif

/*
  Clock memory
  */
static void*      clock_data_addr;
static dma_addr_t clock_data_bus_addr;
static struct device *clock_data_dev = NULL;
static struct class *divadidd_class = NULL;

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

static int
proc_show(struct seq_file *m, void *v)
{
	char tmprev[32];

	strcpy(tmprev, main_revision);
	seq_printf(m, "%s\n", DRIVERNAME);
	seq_printf(m, "name     : %s\n", DRIVERLNAME);
	seq_printf(m, "release  : %s\n", DRIVERRELEASE_DIDD);
	seq_printf(m, "build    : %s(%s)\n",
		       diva_didd_common_code_build, DIVA_BUILD);
	seq_printf(m, "revision : %s\n", getrev(tmprev));
	seq_printf(m, "major    : %d\n", major);
	seq_printf(m, "cfglib   : %d\n", VieLlast);
	seq_printf(m, "cfgstate : %d\n",
																 (diva_cfg_lib_get_storage () != 0) ? 1 : 0);

	return 0;
}

static int divadidd_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

static const struct proc_ops divadidd_proc_fops = {
	.proc_open      = divadidd_proc_open,
	.proc_read      = seq_read,
	.proc_lseek    = seq_lseek,
	.proc_release   = single_release,
};

static int DIVA_INIT_FUNCTION create_proc(void)
{
	proc_net_eicon = proc_mkdir("eicon", init_net.proc_net);

	if (proc_net_eicon) {
		proc_didd = proc_create(DRIVERLNAME, S_IRUGO, proc_net_eicon,
				&divadidd_proc_fops);
		return (1);
	}
	return (0);
}

void diva_os_read_descriptor_array (void* t, int length) {
	DIVA_DIDD_Read (t, length);
}

static struct file_operations divas_didd_fops = {
	.owner   = THIS_MODULE,
	.llseek  = no_llseek,
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

  if (!(data = (byte*)vmalloc ((unsigned int)count+1)))
    return -ENOMEM;

  if (copy_from_user(data, buf, count)) {
    vfree (data);
    return (-EFAULT);
  }

  data[count] = 0;

  if (!(appl = (diva_cfglib_application_t*)file->private_data)) {
    vfree (data);
    return (-ENOMEM);
  }

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

  down(&didd_ifc_lock);

  appl = (diva_cfglib_application_t*)file->private_data;

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
          retval = -EFAULT;
        }
      }
    } else {
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
#if defined(CONFIG_DEVFS_FS)
	  devfs_remove(DEVNAME);
#endif
	  unregister_chrdev(major, DEVNAME);
  }
}

int diva_get_clock_data (dword* bus_addr_lo,
                         dword* bus_addr_hi,
                         dword* length,
                         void** addr) {
	if (clock_data_addr != 0 &&
			(sizeof(clock_data_bus_addr) == sizeof(dword) || ((dword)(clock_data_bus_addr >> 32)) == 0)) {
		*bus_addr_lo = (dword)clock_data_bus_addr;
		*addr        = clock_data_addr;
		*length      = PAGE_SIZE;

		return (0);
	}

  return (-1);
}

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

static void DIVA_EXIT_FUNCTION remove_proc(void)
{
	remove_proc_entry(DRIVERLNAME, proc_net_eicon);
	remove_proc_entry(main_proc_dir, divas_get_proc_net());
}

static int DIVA_INIT_FUNCTION divadidd_init(void)
{
	char tmprev[32];
	int ret = 0;

	printk(KERN_INFO "%s\n", DRIVERNAME);
	printk(KERN_INFO "%s: Rel:%s  Rev:", DRIVERLNAME, DRIVERRELEASE_DIDD);
	strcpy(tmprev, main_revision);
	printk("%s  Build:%s(%s)\n", getrev(tmprev),
	       diva_didd_common_code_build, DIVA_BUILD);

	divadidd_class = class_create(THIS_MODULE, "divadidd");
	if (IS_ERR(divadidd_class)) {
		printk(KERN_ERR "%s: failed to create character device class.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	clock_data_dev = device_create(divadidd_class, NULL, MKDEV(0, 0), NULL, "divadidd");
	if (IS_ERR(clock_data_dev)) {
		printk(KERN_ERR "%s: failed to create character device.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (dma_coerce_mask_and_coherent(clock_data_dev, DMA_BIT_MASK(24))) {
		printk(KERN_ERR "%s: failed to set DMA mask.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

	if (IS_ERR(clock_data_dev)) {
		printk(KERN_ERR "%s: failed to set DMA mask.\n", DRIVERLNAME);
		ret = -EIO;
		goto out;
	}

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

	if ((clock_data_addr = dma_alloc_coherent(clock_data_dev, PAGE_SIZE, &clock_data_bus_addr, GFP_ATOMIC)) != 0) {
		memset (clock_data_addr, 0x00, PAGE_SIZE);
	} else {
		printk(KERN_ERR "%s: failed to allocate DMA for clock data.\n", DRIVERLNAME);
	}

out:

	return (ret);
}

void DIVA_EXIT_FUNCTION divadidd_exit(void)
{
  divas_didd_unregister_chrdev();
  diva_cfg_lib_finit ();
	diddfunc_finit();
	remove_proc();

	if (clock_data_dev) {
		if (clock_data_addr != 0) {
			dma_free_coherent(clock_data_dev, PAGE_SIZE, clock_data_addr, clock_data_bus_addr);
		}
		device_destroy(divadidd_class, MKDEV(0, 0));
	}
	class_destroy(divadidd_class);

	printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

module_init(divadidd_init);
module_exit(divadidd_exit);
