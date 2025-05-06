
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

# ifndef LOG_IF___H
# define LOG_IF___H

#if !defined(UNIX)
# include "typedefs.h"
# include "debuglib.h"
#endif
/*
 * A dirty hack to use the new logging facilities whithout code changes
 */

#if defined(UNIX)
# define logMask 
#else
# define logMask (*((byte *)&myDriverDebugHandle.dbgMask))
#endif

void	logInit		(void);
void	logAdjust	(int Level);
void	logFinit	(void);
void	logHexDump	(byte *data, word size);
byte	*logHexString	(byte *data, word size);
byte	*logHexLine	(byte *data, word size);

# if 0 /* LOG__C */ /* called from log.c, no macros, no externals please! */
#	undef	USE_LOG
#	define	USE_LOG 0
# else	/* !LOG__C */
#if !defined(UNIX)
#	ifndef USE_LOG
#		define USE_LOG 1
#	endif /* USE_LOG */
#endif
	extern	void	(* _cdecl dprintf)(char *fmt, ...);
	extern	word	logDumpMax;
	extern	word	logStringMax;
# endif /* LOG__C */

# define LOG_PAR_ 0x80
# define LOG_DAT_ 0x40
# define LOG_NFY_ 0x20
# define LOG_FNC_ 0x10
# define LOG_NET_ 0x08
# define LOG_PPP_ 0x04
# define LOG_SIG_ 0x02
# define LOG_ERR_ 0x01

# if 0 /* USE_LOG */

#	define	LOG_PAR(_args) { if (logMask & LOG_PAR_) dprintf _args; }
#	define	LOG_DAT(_args) { if (logMask & LOG_DAT_) dprintf _args; }
#	define	LOG_NFY(_args) { if (logMask & LOG_NFY_) dprintf _args; }
#	define	LOG_FNC(_args) { if (logMask & LOG_FNC_) dprintf _args; }
#	define	LOG_NET(_args) { if (logMask & LOG_NET_) dprintf _args; }
#	define	LOG_PPP(_args) { if (logMask & LOG_PPP_) dprintf _args; }
#	define	LOG_SIG(_args) { if (logMask & LOG_SIG_) dprintf _args; }
#	define	LOG_ERR(_args) { if (logMask & LOG_ERR_) dprintf _args; }
#	define	LOG_PRT(_args) dprintf _args;
#	define	LOG_FAT(_args) dprintf _args;

# else	/* !USE_LOG */


#if defined(UNIX) && defined(UNIXWARE) && defined(DEBUG)
#	define	LOG_PAR(_args) cmn_err(CE_NOTE, _args) 
#	define	LOG_NFY(_args) cmn_err(CE_NOTE, _args)
#	define	LOG_FNC(_args) cmn_err(CE_NOTE, "LOG_FNC")
#	define	LOG_NET(_args) cmn_err(CE_NOTE, "LOG_NET")  
#	define	LOG_PPP(_args) cmn_err(CE_NOTE, "LOG_PPP")
#	define	LOG_SIG(_args)  
#	define	LOG_DAT(_args)  
#	define	LOG_ERR(_args)   
#	define	LOG_PRT(_args) cmn_err(CE_NOTE, _args)
#	define	LOG_FAT(_args) cmn_err(CE_NOTE, _args)  
 
#else


#if defined(UNIX) && defined(LINUX) && defined(DEBUG)

void DivasPrintf(char *fmt, ...);

#undef DPRINTF
#define DPRINTF(_args) DivasPrintf _args ;

#   define  LOG_PAR(_args) DivasPrintf _args ;
#   define  LOG_NFY(_args) DivasPrintf _args ;
#   define  LOG_FNC(_args) DivasPrintf _args ;
#   define  LOG_NET(_args) DivasPrintf _args ;
#   define  LOG_PPP(_args) DivasPrintf _args ;
#   define  LOG_SIG(_args) DivasPrintf _args ;
#   define  LOG_DAT(_args) DivasPrintf _args ;
#   define  LOG_ERR(_args) DivasPrintf _args ;
#   define  LOG_PRT(_args) DivasPrintf _args ;
#   define  LOG_FAT(_args) DivasPrintf _args ;

#endif /* LINUX */                        

#if !defined(DEBUG)
#	define	LOG_PAR(_args) 
#	define	LOG_NFY(_args) 
#	define	LOG_FNC(_args) 
#	define	LOG_NET(_args) 
#	define	LOG_PPP(_args) 
#	define	LOG_SIG(_args) 
#	define	LOG_DAT(_args)  
#	define	LOG_ERR(_args)  
#	define	LOG_PRT(_args)  
#	define	LOG_FAT(_args)  
#endif /* !defined(DEBUG) */


# endif /*UNIX*/
# endif	/* USE_LOG */

# endif /* LOG_IF___H */
