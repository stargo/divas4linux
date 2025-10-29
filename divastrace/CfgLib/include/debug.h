
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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

#if !defined(__DIVA_DEBUG_H__)
#define __DIVA_DEBUG_H__

#include <stdarg.h>
#include "platform.h"

#if defined(__cplusplus)
extern "C" {
#endif

//--- debug masks
#define DBG_NOPRINT     0
#define DBG_STATE     0x00000001
#define DBG_ERROR     0x00000002
#define DBG_WARN      0x00000004
#define DBG_INFO      0x00000008
#define DBG_WRAPPER     0x00000010
#define DBG_DETAIL      0x00000020
#define DBG_DATA        0x00000040
#define DBG_PERROR      0x00000080
#define DBG_NCCI_STM    0x00000100
#define DBG_PLCI_STM    0x00000200
#define DBG_CONT_STM    0x00000400
#define DBG_MALLOC      0x00000800
#define DBG_TIMER       0x00001000
#define DBG_INTERNAL    0x00002000
#define DBG_TST         0x00004000
#define DBG_FUNC        0x00008000
#define DBG_LICENSE     0x00200000            /* Detailed information about licensing issues */
#define DBG_SOCKET      0x00400000            /* Detailed information about socket module */
#define DBG_ALL         0xFFFFFFFF

//---------------------------------------------------------------------------
// Constructor
void dbg_new(dword level);
int dbg_isStarted(void);
dword dbg_get_level(void);
void dbg_init(const char *name, const char *version, int debug);
//---------------------------------------------------------------------------
// Destructor 
void dbg_delete();

//---------------------------------------------------------------------------
void dbg_msg_discard(const char *format, ...);
void dbg_start(void);

void dbg_msg(dword level, const char *format, ...);
void dbg_msg_VArg(dword level, const char *format, va_list args);
void dbg_out_VArg(dword level, const char *format, va_list args); /* Print out all messages unconditionally */
void dbg_err_VArg(dword level, const char *format, va_list args); /* Print out all messages unconditionally (for error debugs, will be printed to stderr instead of stdout) */
#define dbg_dmp(level,buffer,length,caption) dbg_dump(level,buffer,length,caption,0,0);
void dbg_dump(dword level,const byte *buffer,dword length, const char *caption, const char *offset,dword addressoffset);
void dbg_dump_raw(const byte *buffer, dword length, const char *caption, const char *offset, dword addressoffset); /* Dump buffer unconditionally */

#define DBG_SRC  dbg_msg(DBG_ERROR,"[%s:%s:%d]", __FILE__, __FUNCTION__, __LINE__);

#if defined(__DIVA_DEBUG_H_INCLUDE_COMMON_DBG_HELPERS__)
void DBG_ERR_2_dbg_msg (const char* fmt, ...);
void DBG_LOG_2_dbg_msg (const char* fmt, ...);
void DBG_BLK_2_dbg_dump (const void* data, dword data_length);
#define DBG_ERR(__x__) DBG_ERR_2_dbg_msg __x__  ;
#define DBG_LOG(__x__) DBG_LOG_2_dbg_msg __x__  ;
#define DBG_FTL(__x__) DBG_ERR_2_dbg_msg __x__  ;
#define DBG_TRC(__x__) DBG_LOG_2_dbg_msg __x__  ;
#define DBG_BLK(__x__) DBG_BLK_2_dbg_dump __x__ ;
#define DBG_REG(__x__) DBG_LOG_2_dbg_msg __x__  ;
#endif

char *formatBitmask(char *retString, void *value, int size);

void diva_dbg_nop(const char *format, ...);
void diva_dbg_ftl(const char *format, ...);
void diva_dbg_err(const char *format, ...);
void diva_dbg_inf(const char *format, ...);
void diva_dbg_dtl(const char *format, ...);

typedef int (diva_debug_mask_callback_proc_t)(dword level);
void dbg_set_mask_callback(diva_debug_mask_callback_proc_t new_callback);

#if defined(__cplusplus)
}
#endif

#endif // #define _DEBUG_H_

//---------------- end of file --------------------

