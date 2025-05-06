
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

#if !defined(UNIX_H)
#define UNIX_H

#ifdef DEC_UNIX
	#ifdef _KERNEL
		#define _INKERNEL
	#endif
#endif

/* include a standard set of header files for Unix drivers */

#if defined(_INKERNEL)
#include "kunix.h"
#endif

/* make sure NULL is defined */

#if !defined(NULL)
#define NULL ((void *) 0)
#endif

/* define a standard boolean type */

#if (defined(UNIXWARE) && defined(_KERNEL))
/*typedef boolean_t bool_t;*/
#else
typedef unsigned short int bool_t;
#endif

#if !defined(FALSE)
#define FALSE 0
#endif

#if !defined(TRUE)
#define TRUE 1
#endif

/* define a MACRO to get the number of elements in an array */

#define DIM(A)  ((sizeof(A)) / (sizeof(A[0])))

/* get rid of various type modifiers */

#define _cdecl
#define far

/* define an MACRO for assertions */

/*........................................
  . DEC: CT: Fri Jul 19 15:00:44         .
  . Given that I can only find an Assert .
  . function in maintlib.c that does an  .
  . fprintf and an exit (!!) I'm leaving .
  . the ASSERT macro to the O/S          .
  . definition unless it's in a userspace.
  . application                          .
  ........................................*/

#if (!defined(DEC_UNIX) || (defined(DEC_UNIX) && !defined(_KERNEL))) && ((!defined (UNIXWARE)) || (defined(UNIXWARE) && !defined(_KERNEL)))

#if defined(DEBUG)

    extern
    void    Assert(char *file_name, int line_number);

    #define ASSERT(condition)           \
                                        \
        if (condition)                  \
        {                               \
            ;                           \
        }                               \
        else                            \
        {                               \
            Assert(__FILE__, __LINE__); \
        }
#else /*  DEBUG not defined */

#if !defined(UNIXWARE) /* already defined in /usr/include/... */
    #define ASSERT(condition)           ;
#endif

#endif

#endif

/* define a standard function type */

typedef void    fn_t(void *);

#if !defined (UNIXWARE) && defined(DTRACE)
#define DPRINTF(args) EtdK_printf args 
#else
#define DPRINTF(args)
#endif /* defined UNIXWARE and DTRACE */

#endif /* of UNIX_H */
