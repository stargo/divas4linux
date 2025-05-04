/* $Id: capimain.c,v 1.24 2003/09/09 06:51:05 schindler Exp $
 *
 * ISDN interface module for Dialogic active cards DIVA.
 * CAPI Interface
 *
 * Copyright 2000-2010 by Armin Schindler (mac@melware.de)
 * Copyright 2000-2010 Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007 Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/skbuff.h>

#include "platform.h"
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,33)
#include <linux/seq_file.h>
#endif

#include "os_capi.h"

#include "di_defs.h"
#include "capi20.h"
#include "divacapi.h"
#include "cp_vers.h"
#include "capifunc.h"

#if defined(DIVA_EICON_CAPI)
#include <linux/device.h>
#include <linux/moduleparam.h>
#if defined(CONFIG_DEVFS_FS)
#include <linux/devfs_fs_kernel.h>
#endif
#include "local_capicmd.h"
#endif

#if defined(DIVA_EICON_CAPI)
int diva_capi_major = 68;
module_param(diva_capi_major, uint, 0);
#endif

static char *main_revision = "$Revision: 1.24 $";
static char *DRIVERNAME =
    "Dialogic DIVA - CAPI Interface driver (http://www.melware.net)";
static char *DRIVERLNAME = "divacapi";

MODULE_DESCRIPTION("CAPI driver for Dialogic DIVA cards");
MODULE_AUTHOR("Cytronics & Melware, Dialogic");
MODULE_LICENSE("GPL");

typedef struct _diva_os_thread_dpc {
	struct work_struct divas_task;
	diva_os_soft_isr_t *psoft_isr;
  atomic_t soft_isr_disabled;
  atomic_t soft_isr_count;
} diva_os_thread_dpc_t;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
typedef void* diva_work_queue_fn_parameter_t;
#else
typedef struct work_struct* diva_work_queue_fn_parameter_t;
#endif

#if defined(DIVA_EICON_CAPI)
static struct capi_ctr* diva_cards[64];

typedef struct _diva_capi_device {
  u16                         applid;
  u16                         errcode;
  atomic_t                    device_error;
  atomic_t                    appl_registered;
  wait_queue_head_t           recvwait;
  struct sk_buff_head         recvqueue;
	struct semaphore            msg_write_lock;
	byte                        write_msg[2150+MAX_MSG_SIZE];
  struct capi_register_params rp;
} diva_capi_device_t;

#define DIVA_CARD_DETECTED 1
#define DIVA_CARD_RUNNING  2

static diva_capi_device_t* diva_appl_map[MAX_APPL];
/*
	diva_appl_lock is used to protect applications against adapter removal and to
	serialize parallel CAPI_REGISTER requests
	*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
	static DECLARE_MUTEX(diva_appl_lock);
#else
	static DEFINE_SEMAPHORE(diva_appl_lock);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
static struct class_simple *diva_capi_class;
#else
static struct class *diva_capi_class;
#endif
static atomic_t diva_capi_controller_count;
extern u16 diva_capi20_get_manufacturer (u32 contr, u8 *buf);
extern u16 diva_capi20_get_profile (u32 contr, struct capi_profile *profile);
extern u16 diva_capi20_get_serial(u32 contr, u8 *serial);
extern u16 diva_capi20_get_version(u32 contr, struct capi_version *version);
extern u16 diva_capi20_send_message(struct capi_ctr *ctrl, CAPI_MSG* msg, dword length);
extern u16 diva_capi20_get_controller_count (void);
#endif

/*
 * get revision number from revision string
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
 * alloc a message buffer
 */
diva_os_message_buffer_s *diva_os_alloc_message_buffer(unsigned long size,
						       void **data_buf)
{
	diva_os_message_buffer_s *dmb = alloc_skb(size, GFP_ATOMIC);
	if (dmb) {
		*data_buf = skb_put(dmb, size);
	}
	return (dmb);
}

/*
 * free a message buffer
 */
void diva_os_free_message_buffer(diva_os_message_buffer_s * dmb)
{
	kfree_skb(dmb);
}

#if !defined(DIVA_EICON_CAPI)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
/*
 * proc function for controller info
 */
static int diva_ctl_read_proc(char *page, char **start, off_t off,
			      int count, int *eof, struct capi_ctr *ctrl)
{
	diva_card *card = (diva_card *) ctrl->driverdata;
	int len = 0;

	len += sprintf(page + len, "%s\n", ctrl->name);
	len += sprintf(page + len, "Serial No. : %s\n", ctrl->serial);
	len += sprintf(page + len, "Id         : %d\n", card->Id);
	len += sprintf(page + len, "Channels   : %d\n", card->d.channels);

	if (off + count >= len)
		*eof = 1;
	if (len < off)
		return 0;
	*start = page + off;
	return ((count < len - off) ? count : len - off);
}
#else
static int diva_proc_show(struct seq_file *m, void *v)
{
	struct capi_ctr *ctrl = m->private;
	diva_card *card = (diva_card *) ctrl->driverdata;

	seq_printf(m, "%s\n", ctrl->name);
	seq_printf(m, "Serial No. : %s\n", ctrl->serial);
	seq_printf(m, "Id         : %d\n", card->Id);
	seq_printf(m, "Channels   : %d\n", card->d.channels);

	return 0;
}

static int diva_proc_open(struct inode *inode, struct file *file)
{
	    return single_open(file, diva_proc_show, PDE(inode)->data);
}

static const struct file_operations diva_proc_fops = {
	.owner      = THIS_MODULE,
	.open       = diva_proc_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};
#endif

/*
 * set additional os settings in capi_ctr struct
 */
void diva_os_set_controller_struct(struct capi_ctr *ctrl)
{
	ctrl->driver_name = DRIVERLNAME;
	ctrl->load_firmware = 0;
	ctrl->reset_ctr = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
	ctrl->ctr_read_proc = diva_ctl_read_proc;
#else
	ctrl->proc_fops = &diva_proc_fops;
#endif
	ctrl->owner = THIS_MODULE;
}
#endif

static void diva_os_dpc_proc (diva_work_queue_fn_parameter_t context)
{
  diva_os_thread_dpc_t *psoft_isr = DIVAS_CONTAINING_RECORD(context, diva_os_thread_dpc_t, divas_task);
	diva_os_soft_isr_t *pisr = psoft_isr->psoft_isr;

  if (atomic_read (&(psoft_isr->soft_isr_disabled)) == 0) {
    if (atomic_inc_and_test(&(psoft_isr->soft_isr_count)) == 1) {
      do {
        (*(pisr->callback)) (pisr, pisr->callback_context);
      } while (atomic_add_negative(-1, &(psoft_isr->soft_isr_count)) == 0);
    }
  }
}

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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&pdpc->divas_task, diva_os_dpc_proc, &pdpc->divas_task);
#else
	INIT_WORK(&pdpc->divas_task, diva_os_dpc_proc);
#endif
  atomic_set (&(pdpc->soft_isr_disabled), 0);
  atomic_set (&(pdpc->soft_isr_count), -1);

	return (0);
}

void diva_os_remove_soft_isr(diva_os_soft_isr_t * psoft_isr)
{
	if (psoft_isr && psoft_isr->object) {
		diva_os_thread_dpc_t *pdpc =
		    (diva_os_thread_dpc_t *) psoft_isr->object;

    atomic_set (&(pdpc->soft_isr_disabled), 1);
		flush_scheduled_work();
		diva_os_free(0, pdpc);
	}
}

int diva_os_schedule_soft_isr(diva_os_soft_isr_t * psoft_isr)
{
	if (psoft_isr && psoft_isr->object) {
		diva_os_thread_dpc_t *pdpc =
		    (diva_os_thread_dpc_t *) psoft_isr->object;

		schedule_work(&pdpc->divas_task);
	}

	return (1);
}

#if defined(DIVA_EICON_CAPI)
static diva_capi_device_t* diva_capi_init_device (void) {
	diva_capi_device_t* capi = kmalloc(sizeof(*capi), GFP_KERNEL);

	if (capi == 0)
		return (0);

	memset (capi, 0x00, sizeof(*capi));
	atomic_set(&capi->device_error, 0);
	atomic_set(&capi->appl_registered, 0);
	init_waitqueue_head(&capi->recvwait);
	skb_queue_head_init(&capi->recvqueue);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
	init_MUTEX(&capi->msg_write_lock);
#else
	sema_init(&capi->msg_write_lock, 1);
#endif

	capi->errcode = CAPI_NOERROR;

	return (capi);
}

static void diva_release_device (void* context) {
	if (context != 0) {
		diva_capi_device_t* capi = (diva_capi_device_t*)context;
		u16 applid               = capi->applid;

		atomic_set(&capi->device_error, 1);
		wake_up_interruptible(&capi->recvwait);
		if (atomic_read(&capi->appl_registered) != 0) {
			int i;

			atomic_set(&capi->appl_registered, 0);

			down(&diva_appl_lock);
			for (i = 0; i < sizeof(diva_cards)/sizeof(diva_cards[0]); i++) {
				if (diva_cards[i] != 0 && diva_cards[i]->cardstate == DIVA_CARD_RUNNING) {
					diva_cards[i]->release_appl(diva_cards[i], applid);
				}
			}
			up(&diva_appl_lock);

			applid--;
			if (applid < MAX_APPL) {
				diva_appl_map[applid] = 0;
			}
		}
		skb_queue_purge(&capi->recvqueue);
		kfree (context);
	}
}

static ssize_t diva_capi_read  (struct file *file, char *buf, size_t count, loff_t *ppos) {
	diva_capi_device_t* capi = (diva_capi_device_t*)file->private_data;
	struct sk_buff *skb;
	size_t copied;

	if (atomic_read(&capi->device_error) != 0 || atomic_read(&capi->appl_registered) == 0)
		return (-EIO);

	if ((skb = skb_dequeue(&capi->recvqueue)) == 0) {
		if ((file->f_flags & O_NONBLOCK) != 0)
			return (-EAGAIN);

		do {
			if (wait_event_interruptible ((capi->recvwait), (skb_queue_empty(&capi->recvqueue) == 0)) != 0)
				return (-ERESTARTNOHAND);
		} while ((skb = skb_dequeue(&capi->recvqueue)) == 0);
	}

	if (skb->len > count) {
		skb_queue_head(&capi->recvqueue, skb);
		return (-EMSGSIZE);
	}

	if (copy_to_user(buf, skb->data, skb->len)) {
		skb_queue_head(&capi->recvqueue, skb);
		return (-EFAULT);
	}

	copied = skb->len;

	kfree_skb(skb);

	return (copied);
}

static ssize_t diva_capi_write (struct file *file, const char *buf, size_t count, loff_t *ppos) {
	diva_capi_device_t* capi = (diva_capi_device_t*)file->private_data;
	byte* dst = &capi->write_msg[0];
	u16 errcode;

	if (atomic_read(&capi->device_error) != 0) {
		capi->errcode = CAPI_REGNOTINSTALLED;
		return (-EIO);
	}
	if (atomic_read(&capi->appl_registered) == 0) {
		capi->errcode = CAPI_ILLAPPNR;
		return (-ENODEV);
	}
	if (count > sizeof(capi->write_msg)) {
		capi->errcode = CAPI_MSGOSRESOURCEERR;
		DBG_ERR(("failed to write %d bytes, message too long", (dword)count))
		return (-EMSGSIZE);
	}

	down(&capi->msg_write_lock);

	if (copy_from_user(dst, buf, count)) {
		up(&capi->msg_write_lock);
		capi->errcode = CAPI_MSGOSRESOURCEERR;
		DBG_ERR(("failed to write %d bytes, message not accessible", (dword)count))
		return (-EFAULT);
	}

	CAPIMSG_SETAPPID(dst, capi->applid);

	{
		u16 cnr = (CAPIMSG_CONTROLLER(dst)) - 1;

		down(&diva_appl_lock);
		if (cnr < sizeof(diva_cards)/sizeof(diva_cards[0]) && diva_cards[cnr] != 0 &&
				diva_cards[cnr]->cardstate == DIVA_CARD_RUNNING)
			errcode = diva_capi20_send_message(diva_cards[cnr], (CAPI_MSG*)dst, (dword)count);
		else
			errcode = CAPI_REGNOTINSTALLED;

		up(&diva_appl_lock);
	}

	capi->errcode = errcode;

	up(&capi->msg_write_lock);

	if (errcode == CAPI_NOERROR)
		return (count);

	switch (errcode) {
		case CAPI_SENDQUEUEFULL:
			return (-EBUSY);
		case CAPI_REGNOTINSTALLED:
			return (-ENODEV);
	}

	return (-EIO);
}

static unsigned int diva_capi_poll (struct file *file, poll_table * wait) {
	diva_capi_device_t* capi = (diva_capi_device_t*)file->private_data;

	if (atomic_read(&capi->device_error) == 0 && atomic_read(&capi->appl_registered) != 0) {
		poll_wait(file, &(capi->recvwait), wait);
		return ((skb_queue_empty(&capi->recvqueue)) ?
							(POLLOUT | POLLWRNORM) : (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM));
	}

	return (POLLERR);
}

static u16 find_free_appl_id (void) {
	u16 applid;

	for (applid = 0; applid < MAX_APPL; applid++) {
		if (diva_appl_map[applid] == 0)
			return (applid+1);
	}

	return (0);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int diva_capi_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
#else
static long diva_capi_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {
#endif
	diva_capi_device_t* capi = (diva_capi_device_t*)file->private_data;
	int ret = -EINVAL;

	if (cmd == CAPI_REGISTER) {
		down(&diva_appl_lock);

		if (atomic_read(&capi->appl_registered) == 0) {
			if (atomic_read(&capi->device_error) == 0) {
				if (copy_from_user(&capi->rp, (void*)arg, sizeof(struct capi_register_params))) {
					ret = -EFAULT;
				} else {
					if ((capi->applid = find_free_appl_id ()) == 0) {
						capi->errcode = CAPI_TOOMANYAPPLS;
						ret = -EIO;
					} else {
						int i;

						ret = (int)capi->applid;
						capi->errcode = CAPI_NOERROR;
						diva_appl_map[capi->applid-1] = capi;
						atomic_set(&capi->appl_registered, 1);

						for (i = 0; i < sizeof(diva_cards)/sizeof(diva_cards[0]); i++) {
							if (diva_cards[i] != 0 && diva_cards[i]->cardstate == DIVA_CARD_RUNNING) {
								diva_cards[i]->register_appl(diva_cards[i], capi->applid, &capi->rp);
							}
						}
					}
				}
			} else {
				capi->errcode = CAPI_REGNOTINSTALLED;
				ret = -EIO;
			}
		} else {
			ret = -EEXIST;
		}

		up(&diva_appl_lock);
	} else if (cmd == CAPI_GET_VERSION) {
		capi_ioctl_struct data;

		if (copy_from_user((void *) &data.contr, (void *) arg, sizeof(data.contr))) {
			ret = -EFAULT;
		} else {
			if ((capi->errcode = diva_capi20_get_version(data.contr, &data.version)) == CAPI_NOERROR) {
				if (copy_to_user((void *)arg, (void *)&data.version, sizeof(data.version)))
					ret = -EFAULT;
				else
					ret = 0;
			} else {
				ret = -EIO;
			}
		}
	} else if (cmd == CAPI_GET_SERIAL) {
		capi_ioctl_struct data;

		if (copy_from_user((void*)&data.contr, (void*)arg, sizeof(data.contr))) {
			ret = -EFAULT;
		} else {
			if ((capi->errcode = diva_capi20_get_serial (data.contr, data.serial)) == CAPI_NOERROR) {
				if (copy_to_user((void*)arg, (void*)data.serial, sizeof(data.serial)))
					ret = -EFAULT;
				else
					ret = 0;
			} else {
				ret = -EIO;
			}
		}
	} else if (cmd == CAPI_GET_PROFILE) {
		capi_ioctl_struct data;

		if (copy_from_user((void *)&data.contr, (void *)arg, sizeof(data.contr))) {
			ret = -EFAULT;
		} else {
			if (data.contr == 0) {
				data.profile.ncontroller = diva_capi20_get_controller_count ();
				capi->errcode = CAPI_NOERROR;
				if (copy_to_user ((void*)arg, (void*)&data.profile.ncontroller, sizeof(data.profile.ncontroller)))
					ret = -EFAULT;
				else
					ret = 0;
			} else {
				if ((capi->errcode = diva_capi20_get_profile(data.contr, &data.profile)) == CAPI_NOERROR) {
					if (copy_to_user((void*)arg, (void*)&data.profile, sizeof(data.profile)))
						ret = -EFAULT;
					else
						ret = 0;
				} else {
					ret = -EIO;
				}
			}
		}
	} else if (cmd == CAPI_GET_MANUFACTURER) {
		capi_ioctl_struct data;

		if (copy_from_user((void *)&data.contr, (void *)arg, sizeof(data.contr))) {
			ret = -EFAULT;
		} else {
			if ((capi->errcode = diva_capi20_get_manufacturer (data.contr, data.manufacturer)) == CAPI_NOERROR) {
				if (copy_to_user((void*)arg, (void*)data.manufacturer, sizeof(data.manufacturer)))
					ret = -EFAULT;
				else
					ret = 0;
			} else {
				ret = -EIO;
			}
		}
	} else if (cmd == CAPI_GET_ERRCODE) {
		u16 errcode = capi->errcode;
		if (atomic_read(&capi->device_error) == 0) {
			capi_ioctl_struct data;

			data.errcode = errcode;
			capi->errcode = CAPI_NOERROR;

			if (arg != 0) {
				if (copy_to_user((void *)arg, (void *)&data.errcode, sizeof(data.errcode)))
					ret = -EFAULT;
				else
					ret = (int)data.errcode;
			} else {
				ret = (int)data.errcode;
			}
		} else {
			ret = (-ENODEV);
		}
	} else if (cmd == CAPI_INSTALLED) {
		if (atomic_read(&diva_capi_controller_count) != 0)
			ret = 0;
		else
			ret = -ENXIO;
	}

	return (ret);
}

static int diva_capi_open (struct inode *inode, struct file *file) {
	diva_capi_device_t* capi;

	if (file->private_data != 0)
		return (-EEXIST);

	if ((capi = diva_capi_init_device ()) == 0) {
		return (-ENOMEM);
	}
	file->private_data = (void*)capi;

	return (0);
}

static int diva_capi_release(struct inode *inode, struct file *file) {
	diva_release_device (file->private_data);
	file->private_data = 0;
	return (0);
}

static struct file_operations diva_capi_fops = {
	.owner   = THIS_MODULE,
	.llseek  = no_llseek,
	.read    = diva_capi_read,
	.write   = diva_capi_write,
	.poll    = diva_capi_poll,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	.ioctl   = diva_capi_ioctl,
#else
	.unlocked_ioctl	= diva_capi_ioctl,
#endif
	.open    = diva_capi_open,
	.release = diva_capi_release
};

/*
	Local compatible to CAPI manager interface
	*/

/*
	DIDD driver ensures that adapter insertions and removals
	are serialized, also sunchronization between these events
	is not necessary
	*/
int attach_capi_ctr(struct capi_ctr *card) {
	if (card->cnr <= 0 || card->cnr > sizeof(diva_cards)/sizeof(diva_cards[0]) ||
			diva_cards[card->cnr - 1] != 0) {
		DBG_ERR(("add controller mapping error, cnr=%d", card->cnr))
		return (-EBUSY);
	}
	down(&diva_appl_lock);

	card->cardstate = DIVA_CARD_DETECTED;
	diva_cards[card->cnr-1] = card;

	up(&diva_appl_lock);

	atomic_inc(&diva_capi_controller_count);

	DBG_REG(("Controller %d: %s attached", card->cnr, card->name))

	return (0);
}

void capi_ctr_ready(struct capi_ctr * card) {
	int i;

	card->cardstate = DIVA_CARD_RUNNING;

	down(&diva_appl_lock);

	for (i = 0; i < MAX_APPL; i++) {
		if (diva_appl_map[i] != 0 &&
				atomic_read(&diva_appl_map[i]->device_error) == 0 &&
				atomic_read(&diva_appl_map[i]->appl_registered) != 0) {
			card->register_appl(card, diva_appl_map[i]->applid, &diva_appl_map[i]->rp);
		}
	}

	up(&diva_appl_lock);
}

int detach_capi_ctr (struct capi_ctr *card) {
	if (card <= 0 || card->cnr > sizeof(diva_cards)/sizeof(diva_cards[0]) ||
			diva_cards[card->cnr - 1] != card) {
		DBG_ERR(("remove controller mapping error, cnr=%d", card ? card->cnr : -1))
		return (-EIO);
	}

	down(&diva_appl_lock);

	diva_cards[card->cnr-1] = 0;
	atomic_dec(&diva_capi_controller_count);

	up(&diva_appl_lock);

	DBG_REG(("Controller %d: %s removed", card->cnr, card->name))

	return (0);
}

void capi_ctr_handle_message (struct capi_ctr *card, u16 applid, struct sk_buff *skb) {

	applid--;

	if (atomic_read(&diva_appl_map[applid]->device_error) == 0 &&
			atomic_read(&diva_appl_map[applid]->appl_registered) != 0) {
		skb_queue_tail(&diva_appl_map[applid]->recvqueue, skb);
		wake_up_interruptible(&diva_appl_map[applid]->recvwait);
	}
}

#endif

/*
 * module init
 */
static int DIVA_INIT_FUNCTION divacapi_init (void) {
	char tmprev[32];
	int ret = 0;


	sprintf(DRIVERRELEASE_CAPI, "%s", DRRELEXTRA);
	printk(KERN_INFO "%s\n", DRIVERNAME);
	printk(KERN_INFO "%s: Rel:%s  Rev:", DRIVERLNAME, DRIVERRELEASE_CAPI);
	strcpy(tmprev, main_revision);
	printk("%s  Build: %s(%s)\n", getrev(tmprev), diva_capi_common_code_build, DIVA_BUILD);

#if defined(DIVA_EICON_CAPI)
	atomic_set (&diva_capi_controller_count, 0);

	if (register_chrdev(diva_capi_major, "capi20", &diva_capi_fops)) {
		printk(KERN_ERR "diva capi20: unable to get major %d\n", diva_capi_major);
		return (-EIO);
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	diva_capi_class = class_simple_create(THIS_MODULE, "divacapi");
#else
	diva_capi_class = class_create(THIS_MODULE, "divacapi");
#endif
	if (IS_ERR(diva_capi_class)) {
		ret = PTR_ERR(diva_capi_class);
		unregister_chrdev(diva_capi_major, "capi20");
		printk(KERN_ERR "diva capi20: unable to create divacapi class\n");
		return (ret);
	}
#if defined(CONFIG_DEVFS_FS)
	devfs_mk_dir("isdn");
	devfs_mk_cdev(MKDEV(diva_capi_major,0), S_IFCHR | S_IRUSR | S_IWUSR, "isdn/capi20");
#endif
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
		struct class_device* tmp_class =
						class_simple_device_add(diva_capi_class, MKDEV(diva_capi_major, 0), NULL, "capi20");
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
		struct class_device* tmp_class =
						class_device_create(diva_capi_class, MKDEV(diva_capi_major, 0), NULL, "capi20");
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
		struct class_device* tmp_class =
						class_device_create(diva_capi_class, NULL, MKDEV(diva_capi_major, 0), NULL, "capi20");
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
		struct device* tmp_class =
						device_create(diva_capi_class, NULL, MKDEV(diva_capi_major, 0), "capi20");
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
		struct device* tmp_class =
						device_create_drvdata(diva_capi_class, NULL, MKDEV(diva_capi_major, 0), NULL, "capi20");
#else
		struct device* tmp_class =
						device_create(diva_capi_class, NULL, MKDEV(diva_capi_major, 0), NULL, "capi20");
#endif
#endif
#endif
#endif
#endif
		if (IS_ERR(tmp_class)) {
			ret = PTR_ERR(tmp_class);
#if defined(CONFIG_DEVFS_FS)
			devfs_remove("isdn/capi20");
			devfs_remove("isdn");
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
			class_simple_destroy(diva_capi_class);
#else
			class_destroy(diva_capi_class);
#endif
			unregister_chrdev(diva_capi_major, "capi20");
			printk(KERN_ERR "diva capi20: unable to add capi device\n");
			return (ret);
		}
	}
#endif

	if (!(init_capifunc())) {
		printk(KERN_ERR "%s: failed init capi_driver.\n", DRIVERLNAME);
		ret = -EIO;
	}

	return ret;
}

/*
 * module exit
 */
static void DIVA_EXIT_FUNCTION divacapi_exit(void)
{
#if defined(DIVA_EICON_CAPI)
	unregister_chrdev(diva_capi_major, "capi20");
#if defined(CONFIG_DEVFS_FS)
	devfs_remove("isdn/capi20");
	devfs_remove("isdn");
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
	class_simple_device_remove(MKDEV(diva_capi_major, 0));
	class_simple_destroy(diva_capi_class);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
	class_device_destroy(diva_capi_class, MKDEV(diva_capi_major, 0));
#else
	device_destroy(diva_capi_class, MKDEV(diva_capi_major, 0));
#endif
	class_destroy(diva_capi_class);
#endif
#endif

	finit_capifunc();
	printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

module_init(divacapi_init);
module_exit(divacapi_exit);
