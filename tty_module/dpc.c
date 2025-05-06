
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

#define __KERNEL_SYSCALLS__
#include <linux/config.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/smp_lock.h>

#include "platform.h"
#include "sys_if.h"
#include "debuglib.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)
#define init_MUTEX_LOCKED(x)	*(x)=MUTEX_LOCKED
#endif

#else /* } { */

#if !defined(LINUX_VERSION_CODE)
#include <linux/version.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/config.h>
#endif
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
#include <linux/smp_lock.h>
#endif
#include <linux/delay.h>
#include <asm/types.h>
#include <asm/io.h>

#include <linux/string.h>

#include "platform.h"
#include "sys_if.h"
#include "debuglib.h"

#if defined(DIVA_USES_MUTEX)
static spinlock_t diva_dpc_q_lock;
#else
#warning "Compilation for older kernel: dpc.c"
#endif

#endif /* } */


/*
	IMPORTS
	*/
extern void diva_tty_process_write_dpc (void);
extern int Channel_Count;
extern atomic_t dpc_init_complete;
dword diva_tty_dpc_scheduler_protection_channel_count = 120;

/*
	LOCALS
	*/
#if !defined(__KERNEL_VERSION_GT_2_4__)
static int diva_tty_dpc_proc (void* context);
#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
typedef void* diva_work_queue_fn_parameter_t;
#else
typedef struct work_struct* diva_work_queue_fn_parameter_t;
#endif

static void diva_tty_dpc_proc (diva_work_queue_fn_parameter_t context);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void diva_timer_function (unsigned long data);
#else
static void diva_timer_function (struct timer_list *t);
#endif

static void diva_process_timer_ticks (void);

/* --------------------------------------------------------------------------
	NEW DPC FRAMEWORK
	 -------------------------------------------------------------------------- */
static diva_entity_queue_t divas_dpc_q; /* Our real DPC queue */
static diva_entity_queue_t divas_t_q; /* Our real timer queue */

/*
	Our DPC simulation data
	*/
static atomic_t					divas_dpc_thread_running;

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
static struct semaphore divas_dpc_sem;
static struct semaphore divas_dpc_end;
static int							divas_dpc_pid = -1;
static atomic_t					divas_dpc_count;
static atomic_t					dpc_protector;

#else /* } { */
static struct work_struct tty_dpc;
static atomic_t           tty_dpc_count;
#endif /* } */

static spinlock_t diva_tty_timer_lock;

/*
	Timer support subsystem that allows us to use TIMERs that are running in the
	same context as main thread, and prevent any synchronization issues in this
	way. The resolution of this virtual timer is 20 mSec
	*/
#define DIVA_TIMER_RES_MS	20
#define DIVA_TIMER_TO ((DIVA_TIMER_RES_MS * HZ) / 1000)
static struct  timer_list	diva_timer_id;
atomic_t		divas_timer_running;
static unsigned long diva_to;
static volatile unsigned long last_dpc_time = 0;
static unsigned long diva_timer_ticks; /* in DIVA_TIMER_RES_MS */

/*
	Main DPC initialization/shutdown services
	*/
int	diva_start_main_dpc (void) {
	diva_q_init (&divas_dpc_q);
	diva_q_init (&divas_t_q);

	spin_lock_init(&diva_tty_timer_lock);
#if defined(DIVA_USES_MUTEX)
	spin_lock_init(&diva_dpc_q_lock);
#endif

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	init_MUTEX_LOCKED(&divas_dpc_sem);
	init_MUTEX_LOCKED(&divas_dpc_end);
	(void)test_and_clear_bit (1, &divas_dpc_count);
	(void)test_and_clear_bit (1, &dpc_protector);

  divas_dpc_pid = kernel_thread (diva_tty_dpc_proc,
																&divas_dpc_q,
																CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
#else /* } { */
  atomic_set(&tty_dpc_count, -1);
  atomic_set(&divas_dpc_thread_running, 1);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	INIT_WORK(&tty_dpc, diva_tty_dpc_proc, NULL);
#else
	INIT_WORK(&tty_dpc, diva_tty_dpc_proc);
#endif
#endif /* } */
	
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	if (divas_dpc_pid != (-1))
#endif /* } */
	{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)			  
		init_timer (&diva_timer_id);
		diva_timer_ticks = 0;

		diva_timer_id.function	= (void *)diva_timer_function;
		diva_timer_id.data		= (unsigned long)&diva_timer_ticks;
#else
		timer_setup(&diva_timer_id, diva_timer_function, 0);
		diva_timer_ticks = 0;
#endif
		if (!(diva_to = DIVA_TIMER_TO)) {
			diva_to = 1;
		}
		mod_timer (&diva_timer_id, jiffies + diva_to);
	}

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	return ((divas_dpc_pid == (-1)) ? (-1) : 0);
#else /* } { */
	return (0);
#endif /* } */
}

/*
	This function does dequeue and process all outstanding DPC's.
	It is called from OS, also have to provide necessary syncronization self.
	*/
static void diva_process_queued_dpc (void) {
	unsigned long old_irql;
	diva_entity_queue_t q;
	sys_DPC* dpc;
	old_irql = eicon_splimp ();

#if defined(DIVA_USES_MUTEX)
	spin_lock_bh (&diva_dpc_q_lock);
#endif
	q = divas_dpc_q;
	diva_q_init (&divas_dpc_q);
#if defined(DIVA_USES_MUTEX)
	spin_unlock_bh(&diva_dpc_q_lock);
#endif

	if (!diva_q_get_head	(&q)) {
		eicon_splx (old_irql);
		return;
	}


	while ((dpc = (sys_DPC*)diva_q_get_head (&q))) {
		diva_q_remove (&q, &dpc->link);
		dpc->Pending = 0;
		(*(dpc->handler))(dpc->context);
	}
	eicon_splx (old_irql);
}

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

static int diva_tty_dpc_proc (void* context) {
	int i;

  atomic_inc(&divas_dpc_thread_running);
  if (atomic_read(&divas_dpc_thread_running) > 1) {
		DBG_ERR(("%s: thread already running", "Divatty"))
		return(0);
  }
  DBG_TRC(("%s: thread started with pid %d", "Divatty", divas_dpc_pid))

  exit_mm(current);
  for (i = 0; (current->files && (i < current->files->max_fds)); i++ ) {
    if (current->files->fd[i]) close(i);
  }
  current->policy = SCHED_RR;
  current->rt_priority = 32;
  strcpy (current->comm, "kttydsxxd");

  for(;;) {
    down_interruptible(&divas_dpc_sem);
    if(!(atomic_read(&divas_dpc_thread_running)))
      break;
    if(signal_pending(current)) {
			/* we may want to do something on signals here */
			spin_lock_irq(&current->sigmask_lock);
			flush_signals(current);
			spin_unlock_irq(&current->sigmask_lock);
    } else {
			(void)test_and_clear_bit (1, &divas_dpc_count);
			if (atomic_read(&dpc_init_complete) == 1) {
				diva_process_timer_ticks ();
				diva_process_queued_dpc();
				diva_tty_process_write_dpc ();
			} else {
				diva_timer_ticks = 0;
			}
			last_dpc_time = jiffies + diva_to;
    }
  }

  up(&divas_dpc_end);
  divas_dpc_pid = -1;
  return (0);
}

#else /* } { */

static void diva_tty_dpc_proc (diva_work_queue_fn_parameter_t context) {
	if (atomic_inc_and_test(&tty_dpc_count) == 1) {
		do {

			if (!(atomic_read(&divas_dpc_thread_running))) {
				break;
			}

			if (atomic_read(&dpc_init_complete) == 1) {
				diva_process_timer_ticks ();
				diva_process_queued_dpc();
				diva_tty_process_write_dpc ();
			} else {
				diva_timer_ticks = 0;
			}
			last_dpc_time = jiffies + diva_to;

		} while (atomic_add_negative(-1, &tty_dpc_count) == 0);
	}
}

#endif /* } */

void diva_tty_start_dpc(void) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	if (!test_and_set_bit (1, &dpc_protector)) {
		if ((Channel_Count <= diva_tty_dpc_scheduler_protection_channel_count) ||
			time_after_eq(jiffies, last_dpc_time)) {
			if (!test_and_set_bit (1, &divas_dpc_count)) {
				up(&divas_dpc_sem);
			}
		}
		(void)test_and_clear_bit (1, &dpc_protector);
	}
#else /* } { */
	if (!(atomic_read(&divas_dpc_thread_running))) {
		return;
	}
	if ((Channel_Count <= diva_tty_dpc_scheduler_protection_channel_count) ||
			time_after_eq(jiffies, last_dpc_time)) {
		schedule_work(&tty_dpc);
	}
#endif /* } */
}

void diva_shutdown_main_dpc (void) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	if (divas_dpc_pid != (-1)) {
		while (atomic_read(&divas_timer_running)) {
			atomic_set(&divas_timer_running, 0);
			del_timer (&diva_timer_id);
		}
		atomic_set(&divas_dpc_thread_running, 0);
  	up(&divas_dpc_sem);
  	down_interruptible(&divas_dpc_end);
	}
#else /* } { */
	while (atomic_read(&divas_timer_running)) {
		atomic_set(&divas_timer_running, 0);
		del_timer (&diva_timer_id);
	}

	atomic_set(&divas_dpc_thread_running, 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,0,0) || \
	(defined(RHEL_RELEASE_CODE) && LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0))
	/* This function removed in-tree in 6.x and eventually became a kernel warning, which RHEL 9.5 backported it
	*/
	cancel_work_sync(&tty_dpc);
#else
	flush_scheduled_work();
#endif
#endif /* } */
}


/*
	Initialize abstract DPC object
	*/
void sysInitializeDpc (sys_DPC *Dpc, sys_CALLBACK handler, void *context) {
	Dpc->handler = handler;
	Dpc->context = context;
	Dpc->Pending = 0;
}

/*
	This call happens under the protection of spin,
	also no synchronization is necessary
	*/
int sysScheduleDpc (sys_DPC *Dpc) {

#if defined(DIVA_USES_MUTEX)
	spin_lock_bh (&diva_dpc_q_lock);
#endif

	if (Dpc->Pending == 0) {
		Dpc->Pending = 1;
		diva_q_add_tail (&divas_dpc_q, &Dpc->link);
#if !defined(__KERNEL_VERSION_GT_2_4__)
		if (!test_and_set_bit (1, &divas_dpc_count)) {
			up(&divas_dpc_sem);
		}
#else
#if defined(DIVA_USES_MUTEX)
		spin_unlock_bh(&diva_dpc_q_lock);
#endif
		schedule_work(&tty_dpc);
#endif

		return (1);
	}

#if defined(DIVA_USES_MUTEX)
	spin_unlock_bh(&diva_dpc_q_lock);
#endif

	return (1);
}

/*
	This call is done under protection of spin,
	also no synchronization is necessary
	*/
int sysCancelDpc (sys_DPC *Dpc) {
#if defined(DIVA_USES_MUTEX)
	spin_lock_bh (&diva_dpc_q_lock);
#endif

	if (Dpc->Pending) {
		Dpc->Pending = 0;
		diva_q_remove   (&divas_dpc_q, &Dpc->link);
	}

#if defined(DIVA_USES_MUTEX)
	spin_unlock_bh(&diva_dpc_q_lock);
#endif

	return (1);
}

/*
	Timer callback. UN-SAFE, and can run in interrupt context
	*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void diva_timer_function (unsigned long data) {
#else
static void diva_timer_function (struct timer_list *t) {
#endif
	unsigned long flags;

	if (!atomic_read(&divas_timer_running)) {
		return;
	}

	spin_lock_irqsave(&diva_tty_timer_lock,flags);
	diva_timer_ticks++;
	spin_unlock_irqrestore(&diva_tty_timer_lock,flags);

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
	if (!test_and_set_bit (1, &divas_dpc_count)) {
		up(&divas_dpc_sem);
	}
#else /* } { */
	schedule_work(&tty_dpc);
#endif /* } */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
	mod_timer (&diva_timer_id, jiffies + diva_to);
#else
	mod_timer (t, jiffies + diva_to);
#endif
	atomic_set(&divas_timer_running, 1);
}

static void diva_process_timer_ticks (void) {
	unsigned long local_ticks = 0;
	unsigned long flags;
	sys_TIMER* t, *t_exp;
	diva_entity_queue_t q;

	spin_lock_irqsave(&diva_tty_timer_lock,flags);
	local_ticks = diva_timer_ticks;
	diva_timer_ticks = 0;
	spin_unlock_irqrestore(&diva_tty_timer_lock,flags);

	flags = eicon_splimp ();
	if (!diva_q_get_head	(&divas_t_q)) {
		eicon_splx (flags);
		return;
	}
	diva_q_init (&q);
	t = (sys_TIMER*)diva_q_get_head (&divas_t_q);

	while (t) {
		if (t->timeout > local_ticks) {
			t->timeout -= local_ticks;
			t = (sys_TIMER*)diva_q_get_next	(&t->link);
		} else {
			t_exp = t;
			t = (sys_TIMER*)diva_q_get_next	(&t->link);
			diva_q_remove (&divas_t_q, &t_exp->link);
			diva_q_add_tail (&q, &t_exp->link);
		}
	}

	while ((t = (sys_TIMER*)diva_q_get_head (&q))) {
		diva_q_remove (&q, &t->link);
		t->timeout = 0;
		t->Pending = 0;
		(*(t->handler))(t->context);
	}

	eicon_splx (flags);
}

/*
	TIMER
	*/
void sysInitializeTimer (sys_TIMER *Timer,
												 sys_CALLBACK handler,
												 void *context) {
	Timer->Pending = 0;
	Timer->timeout = 0;
	Timer->handler = handler;
	Timer->context = context;
}

/*
	Called unter protection, also no synchronization is necessary
	*/
int sysStartTimer (sys_TIMER *Timer, unsigned long msec) {
	return (sysScheduleTimer(Timer, msec));
}

/*
	Called under protection, also no synchronization is necessary
	*/
int sysScheduleTimer (sys_TIMER *Timer, unsigned long msec) {
	if (Timer->Pending) {
		return 1;
	}

	Timer->Pending = 1;
	Timer->timeout = (msec / DIVA_TIMER_RES_MS) + 1;
	diva_q_add_tail (&divas_t_q, &Timer->link);

	return (1);
}

/*
	Called under protection, also no synchronization is necessary
	*/
int sysCancelTimer (sys_TIMER *Timer) {
	if (Timer->Pending) {
		diva_q_remove (&divas_t_q, &Timer->link);
		Timer->Pending = 0;
		Timer->timeout = 0;
	}
	return (1);
}

/*
	Called under protection, also no synchronization is necessary
	*/
int sysTimerPending(sys_TIMER *Timer) {
	return (Timer->Pending);
}

