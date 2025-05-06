
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

#if !defined(KUNIX_H)
#define KUNIX_H

#ifdef DEC_UNIX
#define Hz hz
#define seterror(x)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0)
#include <linux/stdarg.h>
#else
#include <stdarg.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,1,0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(5,14,0) && defined(__RHEL_EXTRAVERSION__) && \
	__RHEL_EXTRAVERSION__ >= 503)
#include <linux/termios_internal.h>
#endif

/* include a standard set of header files for Unix drivers */

/* standard headers for SCO UnixWare kernel */

#ifdef _KERNEL_HEADERS

#if !defined(UNIXWARE)
#include <util/types.h>
#endif
#include <util/param.h>
#if defined(UNIXWARE)
#include <sys/dir.h>
#else
#include <fs/dir.h>
#endif
#include <util/types.h>
#include <fs/stat.h>
#include <io/stream.h>
#include <io/strlog.h>
#include <util/sysmacros.h>
#include <svc/errno.h>
#include <svc/systm.h>
#include <proc/user.h>
#include <mem/immu.h>
#include <util/cmn_err.h>
#include <io/nd/sys/mdi.h>
#include <proc/cred.h>
#include <io/conf.h>
#include <util/mod/moddefs.h>
#include <util/debug.h>
#include <io/autoconf/confmgr/confmgr.h>
#include <io/autoconf/confmgr/cm_i386at.h>
#include <io/ddi.h>
#include <sys/kmem.h>
#include <proc/signal.h>
#include <svc/memory.h>
#include <io/stropts.h>
#include <io/poll.h>
#include <io/termio.h>
#include <io/termios.h>
#include <io/strtty.h>
#include <sys/eucioctl.h>
#include <io/ioctl.h>
#include <sys/ksynch.h>
#include <io/termiox.h>
#include <io/log/log.h>
#include <svc/clock.h>
 
#elif defined(_KERNEL) && !defined(LINUX)

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/mdi.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/moddefs.h>
#include <sys/debug.h>
#include <sys/confmgr.h>
#include <sys/cm_i386at.h>
#include <sys/ddi.h>

#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <memory.h>
#include <poll.h>

#include <sys/termio.h> 
#include <sys/termios.h>
#include <sys/strtty.h>
#include <sys/eucioctl.h>
#include <sys/ioctl.h>
#include <sys/ksynch.h>
#include <sys/termiox.h>
#include <sys/log.h>
#include <sys/clock.h>

#endif

#else


/*........................................
  . DEC: CT: Mon Jul 22 10:29:09         .
  . Take away definitions of MIN and MAX .
  ........................................*/

#if defined(DEC_UNIX) && !defined(_KERNEL) && !defined(_AIX)
	#ifdef MAX
		#undef MAX
	#endif
	#ifdef MIN
		#undef MIN
	#endif
#endif

#if !defined(UNIXWARE)
#include <sys/types.h>
#endif
/** ISW 17/07/1996 **/
#if !defined(_AIX) && !defined(LINUX)
#include <sys/cmn_err.h>
#endif
#include <sys/param.h>
#if !defined(LINUX)
#include <sys/dir.h>
#endif

/*........................................
  . DEC: CT: Fri Jul 19 10:29:42         .
  . Take away unused .h files and add    .
  . some more..                          .
  ........................................*/

#ifdef DEC_UNIX
#include <sys/kernel.h>
#include <io/common/devdriver.h>
#else
#include <sys/user.h>
/** ISW 17/07/1996 **/
#if !defined(_AIX) && !defined(LINUX)
#include <sys/immu.h>
#endif
#endif

#if !defined(LINUX)
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#endif

/*........................................
  . DEC: CT: Fri Jul 19 10:33:28         .
  . These are defined in DEC             .
  . files                                .
  ........................................*/

#ifndef DEC_UNIX
#ifndef _AIX
/* following should be defined in Unix include files */

#if !defined(UNIXWARE)
extern  void    printf(char *, ...);

extern  void    cmn_err(int, char *);
#endif

extern  void    seterror(int);

#if !defined(UNIXWARE)
extern  int     bzero(caddr_t, int);

extern  int     bcopy(caddr_t, caddr_t, int);
#endif
#define clear   bzero
#if !defined(UNIXWARE)
extern  int     copyin(caddr_t src, caddr_t dst, int cnt);

extern  int     copyout(caddr_t src, caddr_t dst, int cnt);
#endif

extern  caddr_t sptalloc(int pages, int mode, int base, int flag);

extern  void    sptfree(char *va, int npages, int freeflg);

extern  int     printcfg(char *name, unsigned base, unsigned offset, 
                            int vec, int dma, char *fmt, ...);

/* streams functions that should also be declared */
#if !defined(LINUX)
extern  void    flushq(queue_t *, int);

extern  int     canput(queue_t *);

extern  int     putbq(queue_t *, mblk_t *);

#if !defined(UNIXWARE) && !defined(SCO)
extern  int     qreply(queue_t *, mblk_t *);

extern  int     freemsg(mblk_t *);
#endif
#endif /* LINUX */

/* I/O functions that should also be declared */

#if !defined(UNIXWARE)
extern  int     inb(int);

extern  int     inw(int);

extern  int     outb(int, char);

extern  int     outw(int, int);

extern  int     repinsb(int, caddr_t, int);

extern  int     repoutsb(int, caddr_t, int);
#endif

/* interrupt related functions that should also be declared */

#if !defined(UNIXWARE)
extern  int     spl0(void);
extern  int     spl1(void);
extern  int     spl2(void);
extern  int     spl3(void);
extern  int     spl4(void);
extern  int     spl5(void);
extern  int     spl6(void);
extern  int     spl7(void);
extern  int     spltty(void);

extern  int     splx(int);
#endif
#endif /* #ifndef _AIX */

/* define MACROs to map internal names for I/O instructions to Unix ones */

#define inp(p)      inb(p)
#define inpw(p)     inw(p)
#define outp(p, v)  outb(p, v)
#else	/* #ifndef DEC_UNIX */
#define clear   bzero
#endif /* #ifndef DEC_UNIX */

/*
 * Replace calls to allocb with call to this function
 * Same functionality from caller's standpoint, but we keep a check on
 * what's heppening.....
 */

#if !defined(LINUX)
extern
mblk_t *ALLOCB(size_t size, int priority);

/*
 * Call this when "finished" with a message, i.e. IN ADDITION TO 
 * freemsg or putq or putnext etc.
 * This MUST be paired with an ALLOCB that allocated the message in the
 * first place
 * Returns zero for OK, -1 if error
 */

extern
int     RELEASEB(mblk_t     *msg);
#endif /* LINUX */

#endif /* of KUNIX_H */
