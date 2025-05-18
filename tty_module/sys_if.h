
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

# ifndef SYS_IF___H
# define SYS_IF___H
# include "platform.h"

#if defined(LINUX)
#undef byte
#undef word
#include <linux/timer.h>
#define byte unsigned char
#define word unsigned short
#else    
# include "kunix.h"
#endif
 
/*
 *  wrapped system function interfaces
 */

void* diva_mem_malloc (dword length);
void   *sysPageAlloc	(dword *pSize, void **pHandle);
void   *sysPageRealloc	(dword *pSize, void **pHandle);
void	sysPageFree	(void *Handle);
void   *sysHeapAlloc	(dword Size);
void	sysHeapFree	(void *Address);

/*
	DPC
	*/
typedef void (_cdecl * sys_CALLBACK) (void *context);
typedef struct sys_DPC {
	diva_entity_link_t link;
	volatile int	Pending;
	sys_CALLBACK  handler;
	void					*context;
} sys_DPC;

int	diva_start_main_dpc (void);
void diva_shutdown_main_dpc (void);
void	sysInitializeDpc (sys_DPC *Dpc, sys_CALLBACK Handler, void *Context);
int	sysScheduleDpc   (sys_DPC *Dpc);
int	sysCancelDpc	 (sys_DPC *Dpc);

/*
	TIMER
	*/
typedef struct sys_TIMER {
	diva_entity_link_t link;
	int						Pending;
	sys_CALLBACK  handler;
	void					*context;
	unsigned long	timeout;
} sys_TIMER;

void	sysInitializeTimer (sys_TIMER *, sys_CALLBACK, void *);
int		sysScheduleTimer	 (sys_TIMER *Timer, unsigned long msec);
int		sysCancelTimer		 (sys_TIMER *Timer);
int		sysStartTimer			 (sys_TIMER *Timer, unsigned long msec);
int		sysTimerPending		 (sys_TIMER *Timer);

/*
	Synchronization
	*/

#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
extern struct semaphore diva_tty_lock;
#else
extern spinlock_t diva_tty_lock;
#endif
#endif

static inline unsigned long eicon_splimp (void) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

	local_bh_disable();
#if defined(DIVA_SMP)
	lock_kernel();
	return (0);
#else
	{
		ulong i;
		save_flags(i);
		cli();
		return (i);
	}
#endif

#else /* } { */

#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    down (&diva_tty_lock);
#else
    spin_lock_bh(&diva_tty_lock);
#endif
#else
	local_bh_disable();
	lock_kernel();
#endif

	return (0);
#endif /* } */
}

static inline void eicon_splx (unsigned long i) {
#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */

#if defined(DIVA_SMP)
	unlock_kernel();
#else
	ulong f = (ulong)i;
	restore_flags(f);
#endif
	local_bh_enable();

#else /* } { */
#if defined(DIVA_USES_MUTEX)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
	up (&diva_tty_lock);
#else
    spin_unlock_bh(&diva_tty_lock);
#endif
#else
	unlock_kernel();
	local_bh_enable();
#endif
#endif /* } */
}

/*
	Debug output
	*/
int DivasPrintf (const char*, ...);

#if defined(UNIX)
typedef struct sys_EVENT 
{
	int KernelEvent;
} sys_EVENT;
#endif

void	sysInitializeEvent (sys_EVENT *Event);
void	sysResetEvent	   (sys_EVENT *Event);
void	sysWaitEvent	   (sys_EVENT *Event);
void	sysPostEvent	   (sys_EVENT *Event);

unsigned long	sysTimerTicks (void) ;
unsigned long	sysLastUpdatedTimerTick (void) ;
void		sysDelayExecution(unsigned long milliseconds);


unsigned long	sysRegOpenService	(char *KeyName) ;
unsigned long	sysRegOpen			(char *KeyName) ;
unsigned long	sysRegCreate		(char *KeyName) ;
unsigned long	sysRegOpenNode		(unsigned long KeyHandle, char *KeyName) ;
int				sysRegEnum			(unsigned long KeyHandle,
									 unsigned long Index,
									 char *Name, int Size) ;
int				sysRegGetVal		(unsigned long KeyHandle,
									 char *Name, unsigned long Type,
									 void *Value, int Size, int Required) ;
int				sysRegSetVal		(unsigned long KeyHandle,
									 char *Name, unsigned long Type,
									 void *Value, int Size) ;
int   			sysRegDelete		(unsigned long KeyHandle) ;
void   			sysRegClose			(unsigned long KeyHandle) ;
int				sysUniToAnsi		(void *Ansi, int AnsiMax,
									 void *Uni, int UniLength, char *Function) ;
int				sysAnsiToUni		(void *Uni, int UniMax,
									 void *Ansi, char *Function) ;
int				sysAnsiToCuni		(void *Cuni, void *Uni, int UniMax,
									 void *Ansi, char *Function) ;
int				sysRegString		(unsigned long KeyHandle, char *Name,
									 char *String, int MaxString) ;
int				sysRegDword			(unsigned long KeyHandle, char *Name,
									unsigned long *Value) ;
	
unsigned long	Convert_Decimal_String (char *String) ;
unsigned long	Convert_Hex_String (char *String) ;

void   *mem_cpy (void *s1, void *s2, unsigned long len);
void   *mem_set (void *s1, int c, unsigned long len);
void   *mem_zero (void *s1, unsigned long len);
int	mem_cmp (void *s1, void *s2, unsigned long len);
int	mem_i_cmp (void *s1, void *s2, unsigned long len);
int	str_len (char *s1);
char   *str_chr (char *s1, int c);
char   *str_rchr (char *s1, int c);
char   *str_cpy (char *s1, const char *s2);
int	str_cmp (char *s1, char *s2);
int	str_i_cmp (char *s1, char *s2);
int	str_match (char *String, char *Substring);
int	str_2_int (char *s);
int str_n_cmp (const char* s1, const char* s2, unsigned int n);

#if defined(LINUX)
unsigned char *int_2_str (unsigned long value, unsigned char *string,
    unsigned long radix, int width, char prefix);
#else  
static unsigned char *int_2_str (unsigned long value, unsigned char *string,
	unsigned long radix, int width, char prefix);

int	_cdecl sprintf (unsigned char *buf, unsigned char *fmt, ...);
#endif

int diva_snprintf (char* dst, int length, const char* format, ...);

# endif /* SYS_IF___H */
