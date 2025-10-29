
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

#include <stdio.h>
#include <stdarg.h>

#ifdef LINUX
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <winioctl.h>
#endif

#include "platform.h"
#include "debug.h"
#include "debuglib.h"
#include "um_dbg.h"
#include "dlist.h"
#include "divatimer.h"
#include "BuildVer.h"

#ifndef _NO_DEBUG_MGNT
#include "man_defs.h"
#include "manage.h"
#endif

typedef struct start_dbg_timer_s {
  char           serviceName[128];
  char           buildVersion[128];
  timerentry_t   timer;
} start_dbg_timer_t;

//--- actual debug level
static dword dbg_level = DBG_ALL;
static dword console_debug = 0;
static char local_name[128] = { '\0' };
static char local_version[128];
static start_dbg_timer_t start_dbg_timer;
static diva_debug_mask_callback_proc_t* change_mask_proc = NULL;

static DIVA_OS_HANDLE dbg_fd = (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE;
#ifdef LINUX
#define DEBUG_MAX_LINE_SIZE 256
#endif
#ifdef WIN32
#define DEBUG_MAX_LINE_SIZE 250
#define IOCTL_DIMAINT_WRITE_LINE    CTL_CODE (0x8001U, 8, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifdef WIN32
void OutputDebug( char *pFormat);
#endif


char *formatBitmask(char *retString, void *value, int size)
{
    int i;
    char *tmp = NULL;
    byte *actBytePtr = (byte *)value;
    byte actByte     = *actBytePtr;
    int digitCount = 0;
    int numBits    = 0;

    switch (size) {
      case (1):
        i = 9;
        break;
      case (2):
        i = 19;
        break;
      case (4):
        i = 39;
        break;
      case (8):
        i = 79;
        break;
      default:
        sprintf(retString, "Error - unknown size");
        return(&(retString[0]));
    }

    tmp    = &(retString[i]);
    *tmp-- = '\0';
    i--;

    while(i >= 0) {

      if(digitCount == 4) {
        *tmp-- = ':';
        i--;
        digitCount = 0;
      }
      if(numBits == 8) {
        actBytePtr++;
        actByte = *actBytePtr;
        numBits = 0;
      }

      *tmp-- = (actByte &0x01) ? '1' : '0';
      actByte = actByte >> 1;
      digitCount++;
      numBits++;
      i--;
    }
    return(&(retString[0]));
}

//---------------------------------------------------------------------------
// Constructor
void dbg_new(dword level)
{
  dbg_level = level;
}

//---------------------------------------------------------------------------
dword dbg_get_level(void)
{
  return dbg_level;
}

int dbg_isStarted(void)
{
  if(dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return 1;
  }
  else {
    return 0;
  }
}

void dbg_init(const char *name, const char *version, int debug)
{
  strcpy(local_name, name);
  strcpy(local_version, version);
  console_debug = debug;

  if (dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  if (console_debug) {
    fprintf(stdout, "Initialise debugging %s - Build %s\n", local_name, version);
    fflush(stdout);
  }

#ifdef LINUX
  dbg_fd = open("/dev/DivasDBG", O_RDWR);

  if (dbg_fd < 0) {
    if ((dbg_fd = open("/proc/net/isdn/eicon/DivasDBG", O_RDWR)) < 0) {
      dbg_fd = (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE;
      return;
    }
  }
  {
    char data[128];

    data[0] = 0;
    data[1] = (byte) (0xFFFFFFFF &0x0000000FF); // Debug Mask, Byte 0
    data[2] = (byte) (0xFFFFFFFF &0x00000FF00) >> 8; // Debug Mask, Byte 1
    data[3] = (byte) (0xFFFFFFFF &0x000FF0000) >> 16; // Debug Mask, Byte 2
    data[4] = (byte) (0xFFFFFFFF &0x0FF000000) >> 24; // Debug Mask, Byte 3

    memcpy (&data[5], name, strlen(name) + 1);
    memcpy (&data[5 + strlen(name) + 1], version, strlen(version) + 1);

    {
      size_t str_length =  5 + strlen(name) + 1 + strlen(version) + 1;
      ssize_t i = write (dbg_fd, data, str_length);

      if (i != str_length) {
        if(i == -1) {
          if (console_debug) {
            fprintf(stderr, "%s - Was not able to init trace(%d:\"%s\")\n", local_name, errno, strerror(errno));
            fflush(stderr);
          }
        }
        else {
          if (console_debug) {
            fprintf(stderr, "%s - Was not able to init trace(ret:%d)\n", local_name, (int)i);
            fflush(stderr);
          }
        }
        close(dbg_fd);
        dbg_fd = (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE;
      }
    }
  }
#endif
#ifdef WIN32
#if !_VC_TRACE

  if ( dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE )
  {
    dbg_fd = (DIVA_OS_HANDLE)CreateFileA ( "\\\\.\\Diehl_DIMAINT",
      GENERIC_READ | GENERIC_WRITE,
      0,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL );
  }
#endif

  if ( dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE ) {
    if (console_debug) printf("%s - trace is now available\n", local_name);
  }
  else {
    if (console_debug) printf("%s - Was not able to init trace\n", local_name);
  }
#endif
  return;

}

void dbg_start(void)
{
  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE)  dbg_init(local_name, local_version, console_debug);
  return;
}



//---------------------------------------------------------------------------
// Destructor
void dbg_delete(void)
{
#ifdef LINUX
  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
  }
  else {
    close(dbg_fd);
    dbg_fd = (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE;
  }
#endif
#ifdef WIN32
#if !_VC_TRACE
    if ( dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE )
    {
        CloseHandle ( dbg_fd );
        dbg_fd = (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE;
    }
#endif
#endif
}
#if 0
static void dbg_timerCallb(timerentry_t *timerentry)
{
  start_dbg_timer_t* start_dbg_timer = (start_dbg_timer_t*)timerentry->context;
  if(dbg_level) {
    dbg_start();
  }
  diva_timer_start(&(start_dbg_timer->timer), 10000);
  return;
}

void dbg_start_timer(dword trace_mask, char *name, char *version, int debug)
{

  strcpy(start_dbg_timer.serviceName, name);
  strcpy(start_dbg_timer.buildVersion, version);

  start_dbg_timer.timer.callb   = dbg_timerCallb;
  start_dbg_timer.timer.context = (void *)&(start_dbg_timer);

  dbg_new(trace_mask);
  if(dbg_level)/* dbg_level '0' / debug driver not started */
  {
    dbg_init(start_dbg_timer.serviceName, start_dbg_timer.buildVersion, debug);
  }

  diva_timer_start(&(start_dbg_timer.timer), 10000);
}

void dbg_delete_timer(void)
{
  diva_timer_stop(&(start_dbg_timer.timer));
  dbg_delete();
}
#endif
void diva_dbg_nop(const char *format, ...)
{
}

void diva_dbg_ftl(const char *format, ...)
{
  va_list args;

  if(!(DBG_ERROR & dbg_level)) {
    return;
  }

  if ((DBG_ERROR & dbg_level) & DBG_PERROR)
  {
    perror(format);
    return;
  }

  {
    char data[1024 + 3 + 2];
#ifdef LINUX
    size_t str_length;
    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
    data[3] = 'E';
    data[4] = '-';
    va_start(args, format);
    vsnprintf(&data[3+2], (1024 + 3 + 2) -  (3 + 2), format, args);
    va_end(args);
    if(console_debug) {
      fprintf(stderr,"%s\n", &(data[3]));
      fflush(stderr);
    }

    if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
      return;
    }

    {
      size_t restLength;
      word   dataIndex  = 0;
      str_length =  strlen(&data[3]) + 3;

      restLength = str_length;
      while(restLength) {
        word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

        restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;
        if(tmpLength == 4) {
          tmpLength++;         // add one byte to avoid error of usermode trace */
          data[dataIndex + tmpLength] = ' ';
          data[dataIndex + tmpLength + 1] = 0;
        }
        if(tmpLength > 1) {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if ((i != (tmpLength)) && console_debug) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + 2; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - 2;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          data[dataIndex + 3] = 'E';
          data[dataIndex + 4] = '-';
        }
      }
    }
#endif
#ifdef WIN32
    {
      data[0] = 'E';
      data[1] = '-';
      va_start(args, format);
      _vsnprintf(&data[2], (1024 + 3 + 15) - 2, format, args);
      va_end(args);
      if(console_debug) {
        fprintf(stderr,"%s\n", data);
        fflush(stderr);
      }
      OutputDebug(data);
    }
#endif
  }
}

void diva_dbg_err(const char *format, ...) {
  va_list args;

  if(!(DBG_WARN & dbg_level)) {
    return;
  }

  if ((DBG_WARN & dbg_level) & DBG_PERROR)
  {
    perror(format);
    return;
  }

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  {
    char data[1024 + 3 + 2];
#ifdef LINUX
    size_t str_length;
    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
    data[3] = 'W';
    data[4] = '-';
    va_start(args, format);
    vsnprintf(&data[3+2], (1024 + 3 + 2) -  (3 + 2), format, args);
    va_end(args);

    {
      size_t restLength;
      word   dataIndex  = 0;
      str_length =  strlen(&data[3]) + 3;
      restLength = str_length;
      while(restLength) {
        word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

        restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;
        if(tmpLength == 4) {
          tmpLength++;         // add one byte to avoid error of usermode trace */
          data[dataIndex + tmpLength] = ' ';
          data[dataIndex + tmpLength + 1] = 0;
        }

        if(tmpLength > 1) {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if ((i != (tmpLength)) && console_debug) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + 2; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - 2;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          data[dataIndex + 3] = 'W';
          data[dataIndex + 4] = '-';
        }
      }
    }
#endif
#ifdef WIN32
    {
      data[0] = 'W';
      data[1] = '-';
      va_start(args, format);
      _vsnprintf(&data[2], (1024 + 3 + 15) - 2, format, args);
      va_end(args);
      OutputDebug(data);
    }
#endif
  }
}

void diva_dbg_inf(const char *format, ...) {
  va_list args;

  if(!(DBG_INFO & dbg_level)) {
    va_start(args, format);
    va_end(args);
    return;
  }

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  {
    char data[1024 + 3 + 2];
#ifdef LINUX
    size_t str_length;
    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
    data[3] = 'I';
    data[4] = '-';
    va_start(args, format);
    vsnprintf(&data[3+2], (1024 + 3 + 2) -  (3 + 2), format, args);
    va_end(args);

    {
      size_t restLength;
      word   dataIndex  = 0;
      str_length =  strlen(&data[3]) + 3;
      restLength = str_length;
      while(restLength) {
        word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

        restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;
        if(tmpLength == 4) {
          tmpLength++;         // add one byte to avoid error of usermode trace */
          data[dataIndex + tmpLength] = ' ';
          data[dataIndex + tmpLength + 1] = 0;
        }
        if(tmpLength > 1) {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if ((i != (tmpLength)) && console_debug) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + 2; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - 2;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          data[dataIndex + 3] = 'I';
          data[dataIndex + 4] = '-';
        }
      }
    }
#endif
#ifdef WIN32
    {
      data[0] = 'I';
      data[1] = '-';
      va_start(args, format);
      _vsnprintf(&data[2], (1024 + 3 + 15) - 2, format, args);
      va_end(args);
      OutputDebug(data);
    }
#endif
  }
}

void diva_dbg_dtl(const char *format, ...) {
  va_list args;

  if(!(DBG_DETAIL & dbg_level)) {
    return;
  }

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  {
    char data[1024 + 3 + 2];
#ifdef LINUX
    size_t str_length;
    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
    data[3] = 'V';
    data[4] = '-';
    va_start(args, format);
    vsnprintf(&data[3+2], (1024 + 3 + 2) -  (3 + 2), format, args);
    va_end(args);

    {
      size_t restLength;
      word   dataIndex  = 0;
      str_length =  strlen(&data[3]) + 3;
      restLength = str_length;
      while(restLength) {
        word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

        restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;
        if(tmpLength == 4) {
          tmpLength++;         // add one byte to avoid error of usermode trace */
          data[dataIndex + tmpLength] = ' ';
          data[dataIndex + tmpLength + 1] = 0;
        }
        if(tmpLength > 1) {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if ((i != (tmpLength)) && console_debug) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + 2; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - 2;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          data[dataIndex + 3] = 'V';
          data[dataIndex + 4] = '-';
        }
      }
    }
#endif
#ifdef WIN32
    {
      data[0] = 'V';
      data[1] = '-';
      va_start(args, format);
      _vsnprintf(&data[2], (1024 + 3 + 15) - 2, format, args);
      va_end(args);
      OutputDebug(data);
    }
#endif
  }
}




//---------------------------------------------------------------------------
// Destructor

void dbg_msg_discard(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  va_end(args);
}


//---------------------------------------------------------------------------
void dbg_msg(dword level, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  dbg_msg_VArg(level, format, args);
  va_end(args);
}

void dbg_msg_VArg(dword level, const char *format, va_list args)
{
  char dbgTypeSTR[15];
  byte diva_dbg_level_byte_1 = 0;
#ifdef _DBG_TO_STDOUT_
  byte to_stdtout = TRUE;
#else
  byte to_stdtout = FALSE;
#endif  

  if((!(level & dbg_level)) && (level != DBG_ALL)) {
    return;
  }
  if(console_debug) {
    if(level &DBG_STATE) {
      to_stdtout = TRUE;
      dbgTypeSTR[0] = 'S';
      dbgTypeSTR[1] = '-';
      dbgTypeSTR[2] = 0;
    }
  }

  if(level) {
    if (((level & dbg_level) & DBG_PERROR) && (level != DBG_ALL))
    {
      perror(format);
      return;
    }

    switch (level) {
      case DBG_NOPRINT:
        return;
        break;
      case DBG_ALL:
        sprintf(dbgTypeSTR, "B-");
        diva_dbg_level_byte_1  = (byte)(DLI_LOG >> 8);
        break;
      case DBG_STATE:
        sprintf(dbgTypeSTR, "S-");
        diva_dbg_level_byte_1  = (byte)(DLI_LOG >> 8);
        break;
      case DBG_ERROR:
        sprintf(dbgTypeSTR, "E-");
        diva_dbg_level_byte_1  = (byte)(DLI_FTL >> 8);
        break;
      case DBG_WARN:
        sprintf(dbgTypeSTR, "W-");
        diva_dbg_level_byte_1  = (byte)(DLI_ERR >> 8);
        break;
      case DBG_INFO:
        sprintf(dbgTypeSTR, "I-");
        diva_dbg_level_byte_1  = (byte)(DLI_LOG >> 8);
        break;
      case DBG_DETAIL:
        sprintf(dbgTypeSTR, "V-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_DATA:
        sprintf(dbgTypeSTR, "D-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_PERROR:
        sprintf(dbgTypeSTR, "R-");
        diva_dbg_level_byte_1  = (byte)(DLI_FTL >> 8);
        break;
      case DBG_NCCI_STM:
        sprintf(dbgTypeSTR, "N-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_PLCI_STM:
        sprintf(dbgTypeSTR, "P-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_CONT_STM:
        sprintf(dbgTypeSTR, "C-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_MALLOC:
        sprintf(dbgTypeSTR, "M-");
        diva_dbg_level_byte_1  = (byte)(DLI_MEM >> 8);
        break;
      case DBG_TIMER:
        sprintf(dbgTypeSTR, "T-");
        diva_dbg_level_byte_1  = (byte)(DLI_TIM >> 8);
        break;
      case DBG_WRAPPER:
        sprintf(dbgTypeSTR, "A-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_INTERNAL:
        sprintf(dbgTypeSTR, "L-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_TST:
        sprintf(dbgTypeSTR, "H-");
        break;
      case DBG_FUNC:
        sprintf(dbgTypeSTR, "F-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_LICENSE:
        sprintf(dbgTypeSTR, "LC-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      case DBG_SOCKET:
        sprintf(dbgTypeSTR, "SO-");
        diva_dbg_level_byte_1  = (byte)(DLI_TRC >> 8);
        break;
      default:
        sprintf(dbgTypeSTR, "%08x-", level);
        diva_dbg_level_byte_1  = (byte)(DLI_ERR >> 8);
        break;
    }
  }

  if(diva_dbg_level_byte_1 || to_stdtout) {
    char data[1024 + 3 + 15];
    size_t type_strlen = strlen(dbgTypeSTR);
#ifdef LINUX
    size_t str_length;
    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
    memcpy (&data[3], dbgTypeSTR, type_strlen);
    vsnprintf(&data[3+type_strlen], (1024 + 3 + 15) -1 - (3 + type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
		data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed was longer than buffer */

		if(console_debug) {
      if(diva_dbg_level_byte_1 >= (byte)(DLI_FTL >> 8)) {
        fprintf(stderr,"%s\n", &(data[3]));
        fflush(stderr);
      }
      else {
        if(to_stdtout) {
          fprintf(stdout, "%s\n", &(data[3]));
          fflush(stdout);
        }
      }
    }

    if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
      return;
    }

    {
      size_t restLength;
      word   dataIndex  = 0;
      str_length =  strlen(&data[3]) + 3;
      restLength = str_length;
      while(restLength > 3) {
        word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

        restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;

        if(tmpLength == 4) {
          tmpLength++;         // add one byte to avoid error of usermode trace
          data[dataIndex + tmpLength] = ' ';
          data[dataIndex + tmpLength + 1] = 0;
        }


        if(tmpLength > 1) {
          {
            ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
            if (i != (tmpLength)) {
              if(i == -1) {
                if(console_debug) {
                  fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                  fflush(stderr);
                }
              }
              else {
                if(console_debug) {
                  fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                  fflush(stderr);
                }
              }
              dbg_delete();
              restLength = 0;
            }
          }
          if(restLength) {
            restLength = restLength + 3 + type_strlen; /* for the header */
            dataIndex += DEBUG_MAX_LINE_SIZE - 3 - type_strlen;
            data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
            *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
            memcpy (&(data[dataIndex+ 3]), dbgTypeSTR, type_strlen);
          }
        }
      }
    }
#endif
#ifdef WIN32
    memcpy (data, dbgTypeSTR, type_strlen);
    _vsnprintf(&data[type_strlen], (1024 + 3 + 15) -1 -  (type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
		data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed is longer than the buffer */

    if(console_debug) {
      if(diva_dbg_level_byte_1 >= (byte)(DLI_LOG >> 8)) {
        fprintf(stderr,"%s\n", data);
        fflush(stderr);
      }
      else  {
        if(to_stdtout) {
          fprintf(stdout,"%s\n", data);
          fflush(stdout);
        }
      }
    }

    OutputDebug(data);

#endif
  }
}

void dbg_out_VArg(dword level, const char *format, va_list args)
{
  char data[1024 + 3 + 15];
	char dbgTypeSTR[15] = {0};
  size_t type_strlen = strlen(dbgTypeSTR);
#ifdef LINUX
#ifdef _DBG_TO_STDOUT_
  byte to_stdtout = TRUE;
#else
  byte to_stdtout = FALSE;
#endif  

  size_t str_length;
  data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
  *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
  memcpy (&data[3], dbgTypeSTR, type_strlen);
  vsnprintf(&data[3+type_strlen], (1024 + 3 + 15) -1 - (3 + type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
	data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed was longer than buffer */

	if(console_debug) {
    if(to_stdtout) {
      fprintf(stdout, "%s\n", &(data[3]));
      fflush(stdout);
    }
  }

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  {
    size_t restLength;
    word   dataIndex  = 0;
    str_length =  strlen(&data[3]) + 3;
    restLength = str_length;
    while(restLength > 3) {
      word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

      restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;

      if(tmpLength == 4) {
        tmpLength++;         // add one byte to avoid error of usermode trace
        data[dataIndex + tmpLength] = ' ';
        data[dataIndex + tmpLength + 1] = 0;
      }


      if(tmpLength > 1) {
        {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if (i != (tmpLength)) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + type_strlen; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - type_strlen;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          memcpy (&(data[dataIndex+ 3]), dbgTypeSTR, type_strlen);
        }
      }
    }
  }
#endif
#ifdef WIN32
  memcpy (data, dbgTypeSTR, type_strlen);
  _vsnprintf(&data[type_strlen], (1024 + 3 + 15) -1 -  (type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
	data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed is longer than the buffer */

  if(console_debug) {
    fprintf(stdout,"%s\n", data);
    fflush(stdout);
  }

  OutputDebug(data);
#endif
}

void dbg_err_VArg(dword level, const char *format, va_list args)
{
  char data[1024 + 3 + 15];
	char dbgTypeSTR[15] = {0};
  size_t type_strlen = strlen(dbgTypeSTR);

#ifdef LINUX
  size_t str_length;
  data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
  *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;
  memcpy (&data[3], dbgTypeSTR, type_strlen);
  vsnprintf(&data[3+type_strlen], (1024 + 3 + 15) -1 - (3 + type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
	data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed was longer than buffer */

	if(console_debug) {
    fprintf(stderr,"%s\n", &(data[3]));
    fflush(stderr);
  }

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
    return;
  }

  {
    size_t restLength;
    word   dataIndex  = 0;
    str_length =  strlen(&data[3]) + 3;
    restLength = str_length;
    while(restLength > 3) {
      word tmpLength = (restLength > (DEBUG_MAX_LINE_SIZE)) ? (DEBUG_MAX_LINE_SIZE) : restLength;

      restLength = (restLength > tmpLength) ?  (restLength - tmpLength) : 0;

      if(tmpLength == 4) {
        tmpLength++;         // add one byte to avoid error of usermode trace
        data[dataIndex + tmpLength] = ' ';
        data[dataIndex + tmpLength + 1] = 0;
      }


      if(tmpLength > 1) {
        {
          ssize_t i = write (dbg_fd, &(data[dataIndex]), tmpLength);
          if (i != (tmpLength)) {
            if(i == -1) {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            else {
              if(console_debug) {
                fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)i, &(data[dataIndex + 3]), tmpLength);
                fflush(stderr);
              }
            }
            dbg_delete();
            restLength = 0;
          }
        }
        if(restLength) {
          restLength = restLength + 3 + type_strlen; /* for the header */
          dataIndex += DEBUG_MAX_LINE_SIZE - 3 - type_strlen;
          data[dataIndex] = DIVA_UM_IDI_TRACE_CMD_WRITE;
          *(unsigned short*)&data[dataIndex + 1] = (unsigned short)DLI_LOG;
          memcpy (&(data[dataIndex+ 3]), dbgTypeSTR, type_strlen);
        }
      }
    }
  }
#endif
#ifdef WIN32
  memcpy (data, dbgTypeSTR, type_strlen);
  _vsnprintf(&data[type_strlen], (1024 + 3 + 15) -1 -  (type_strlen), format, args);  /* Reserve 1 byte for terminating 0 */
	data[sizeof(data)-1] = 0; /* Ensure terminating 0 if text to be printed is longer than the buffer */

  if(console_debug) {
    fprintf(stderr,"%s\n", data);
    fflush(stderr);
  }

  OutputDebug(data);
#endif
}


#ifdef WIN32
void OutputDebug( char *debugBuffer)
{
  int nLength;

  nLength = strlen( debugBuffer);

#if _VC_TRACE
  strcpy( debugBuffer + nLength, "\n" );
  OutputDebugString( debugBuffer );
#else
  /*
   * Write the buffer to the ditrace via device IO. Note
   * that only buffers of 60 bytes are passed down at a time.
   */
  if ( dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE )
      return;
  {
    char    *pStart = debugBuffer;
    char    *pWrap;
    char    cSave;
    DWORD   dwBytesWritten;

    while (nLength > DEBUG_MAX_LINE_SIZE)
    {
        pWrap = pStart + DEBUG_MAX_LINE_SIZE;

        while (pWrap > pStart && *pWrap != ' ')
            pWrap -= 1;

        if (pWrap < pStart + DEBUG_MAX_LINE_SIZE/2 )
            pWrap = pStart + DEBUG_MAX_LINE_SIZE;

        cSave = *pWrap;
        *pWrap = '\0';

        DeviceIoControl( dbg_fd, IOCTL_DIMAINT_WRITE_LINE,
                         pStart, (pWrap - pStart) + 1, NULL, 0, &dwBytesWritten, NULL );
        if ( cSave == ' ')
            pWrap += 1;
        else
            *pWrap = cSave;
        nLength -= (int) (pWrap - pStart);
        pStart = pWrap;
    }

    DeviceIoControl( dbg_fd, IOCTL_DIMAINT_WRITE_LINE,
                     pStart, nLength + 1, NULL, 0, &dwBytesWritten, NULL );
  }
#endif
}
#endif

void dbg_dump(dword level, const byte *buffer, dword length, const char *caption, const char *offset, dword addressoffset)
{
  char dbgTypeSTR[15];
	char captionPrefix[512];

  if(!(level & dbg_level)) return;
  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) return;
  if(!length) return;
  if(!buffer) return;
	if(caption && (strlen(caption)+sizeof(dbgTypeSTR) > sizeof(captionPrefix))) return; /* Make sure that the sum of caption and debug type prefix does not exceed buffer size */

  switch (level) {
    case DBG_NOPRINT:
      return;
      break;
    case DBG_ALL:
      sprintf(dbgTypeSTR, "B-");
      break;
    case DBG_STATE:
      sprintf(dbgTypeSTR, "S-");
      break;
    case DBG_ERROR:
      sprintf(dbgTypeSTR, "E-");
      break;
    case DBG_WARN:
      sprintf(dbgTypeSTR, "W-");
      break;
    case DBG_INFO:
      sprintf(dbgTypeSTR, "I-");
      break;
    case DBG_DETAIL:
      sprintf(dbgTypeSTR, "V-");
      break;
    case DBG_DATA:
      sprintf(dbgTypeSTR, "D-");
      break;
    case DBG_PERROR:
      sprintf(dbgTypeSTR, "R-");
      break;
    case DBG_NCCI_STM:
      sprintf(dbgTypeSTR, "N-");
      break;
    case DBG_PLCI_STM:
      sprintf(dbgTypeSTR, "P-");
      break;
    case DBG_CONT_STM:
      sprintf(dbgTypeSTR, "C-");
      break;
    case DBG_MALLOC:
      sprintf(dbgTypeSTR, "M-");
      break;
    case DBG_TIMER:
      sprintf(dbgTypeSTR, "T-");
      break;
    case DBG_WRAPPER:
      sprintf(dbgTypeSTR, "A-");
      break;
    case DBG_INTERNAL:
      sprintf(dbgTypeSTR, "L-");
      break;
    case DBG_TST:
      sprintf(dbgTypeSTR, "H-");
      break;
    case DBG_FUNC:
      sprintf(dbgTypeSTR, "F-");
      break;
    case DBG_LICENSE:
      sprintf(dbgTypeSTR, "LC-");
      break;
    case DBG_SOCKET:
      sprintf(dbgTypeSTR, "SO-");
      break;
    default:
      sprintf(dbgTypeSTR, "%08x-", level);
      break;
  }

  if (caption) sprintf(captionPrefix, "%s%s", dbgTypeSTR, caption);
  else         sprintf(captionPrefix, "%s",   dbgTypeSTR);

	dbg_dump_raw(buffer, length, captionPrefix, offset, addressoffset);
}

#ifdef LINUX
void dbg_dump_raw(const byte *buffer,dword length, const char *caption, const char *offset,dword addressoffset)
{
  static const char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

  dword i = 0, j = 0;
  char *p = NULL,*q = NULL;
  char hex_line[80];
  char char_line[80];

  if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) return;
  if(!length) return;
  if(!buffer) return;

  {
    byte data[512];
    ssize_t y;

    data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
    *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;

    if (caption) sprintf((char*)&data[3], "%s len:%u", caption, length);
    else         sprintf((char*)&data[3], " len:%u", length);

    y = write (dbg_fd, &(data[0]), strlen((char*)&data[3]) + 3);
    // old if (y != strlen(data)) {
    if (y != (strlen((char*)&data[3]) + 3)) {
      if(y == -1) {
        if(console_debug) {
          fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[0]), (int)strlen((char*)&data[3]));
          fflush(stderr);
        }
        dbg_msg(DBG_ERROR, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[0]), (int)strlen((char*)&data[3]));
      }
      else {
        if(console_debug) {
          fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)y, &(data[0]), (int)strlen((char*)&data[3]));
          fflush(stderr);
        }
        dbg_msg(DBG_ERROR, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)y, &(data[0]), (int)strlen((char*)&data[3]));
      }
      dbg_delete();
    }
  }

  for (i = 0; i < length; i += 16)
  {
    p = &(hex_line[0]);
    q = &(char_line[0]);
    for (j = 0;j < 16; j++)
    {
      if(i+j < length)
      {
          *(p++) = ' ';
          *(p++) = hex_digit_table[buffer[i+j] >> 4];
          *(p++) = hex_digit_table[buffer[i+j] & 0xf];
          *q=buffer[i+j];
          if(*q<'0') *q='.';
          q++;
      }
      else
      {
          *(p++) = ' ';
          *(p++) = ' ';
          *(p++) = ' ';
      }
    }
    *p = '\0';
    *q = '\0';

    {
       byte data[512];
      data[0] = DIVA_UM_IDI_TRACE_CMD_WRITE;
      *(unsigned short*)&data[1] = (unsigned short)DLI_LOG;

      /*if(caption&&!offset) sprintf(outstring,"%s[%04x]%s %s\n\r", caption,(unsigned int) i,hex_line,char_line);*/
      if(caption) sprintf((char*)&data[3], "%s [%04d]%s %s", caption, (unsigned int) i, hex_line, char_line);
      else        sprintf((char*)&data[3], " [%04d]%s %s", (unsigned int) i, hex_line, char_line);
      if (console_debug) {
        fprintf(stdout, "%s\n", &(data[3]));
        fflush(stdout);
      }

      {
        ssize_t y = write (dbg_fd, &(data[0]), strlen((char*)&data[3]) + 3);
        // old if (y != strlen(data)) {
        if (y != (strlen((char*)&data[3])+ 3)) {
          if(y == -1) {
            if(console_debug) {
              fprintf(stderr, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[0]), (int)strlen((char*)&data[3]));
              fflush(stderr);
            }
            dbg_msg(DBG_ERROR, "Was not able to write to trace(%d:\"%s\"):\n - %s:%d bytes -\n", errno, strerror(errno), &(data[0]), (int)strlen((char*)&data[3]));
          }
          else {
            if(console_debug) {
              fprintf(stderr, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)y, &(data[0]), (int)strlen((char*)&data[3]));
              fflush(stderr);
            }
            dbg_msg(DBG_ERROR, "Was not able to write to trace(ret:%d):\n - %s:%d bytes -\n", (int)y, &(data[0]), (int)strlen((char*)&data[3]));
          }
          dbg_delete();
        }
      }
    }
  }
}
#endif
#ifdef WIN32
void dbg_dump_raw(const byte *buffer, dword length, const char *caption, const char *offset, dword addressoffset)
{
	static const char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	dword i = 0, j = 0;
	char *p = NULL,*q = NULL;
	char hex_line[80];
	char char_line[80];
	char data[512];

	if(dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) return;

	if(!buffer) return;
	if(!length) return;
	if(caption && (strlen(caption)+20 > sizeof(data))) return; /* Make sure the sprintf() later does not write over the buffer boundary */

	if(caption) sprintf(data, "%s len:%u", caption, length);
	else        sprintf(data, " len:%u", length);
	OutputDebug(data);
	if (console_debug) {
		fprintf(stderr, "%s\n", data);
	}

	for (i = 0; i < length; i += 16)
	{
		p = &(hex_line[0]);
		q = &(char_line[0]);
		for (j = 0;j < 16; j++)
		{
			if(i+j < length)
			{
					*(p++) = ' ';
					*(p++) = hex_digit_table[buffer[i+j] >> 4];
					*(p++) = hex_digit_table[buffer[i+j] & 0xf];
					*q=buffer[i+j];
					if(*q<'0') *q='.';
					q++;
			}
			else
			{
					*(p++) = ' ';
					*(p++) = ' ';
					*(p++) = ' ';
			}
		}
		*p = '\0';
		*q = '\0';
	{
       char data[512];


      /*if(caption&&!offset) sprintf(outstring,"%s[%04x]%s %s\n\r",(char *) caption,(unsigned int) i,hex_line,char_line);*/
      if(caption) sprintf(data, "%s [%04d]%s %s", caption, (unsigned int) i, hex_line, char_line);
      else        sprintf(data, " [%04d]%s %s",            (unsigned int) i, hex_line, char_line);

    OutputDebug(data);
    if (console_debug) {
      fprintf(stderr, "%s\n", data);
    }
  }
  }
}
#endif

void DBG_ERR_2_dbg_msg (const char* format, ...) {
  va_list args;
  va_start(args, format);
  dbg_msg_VArg(DBG_ERROR, format, args);
  va_end(args);
}

void DBG_LOG_2_dbg_msg (const char* format, ...) {
  va_list args;
  va_start(args, format);
  dbg_msg_VArg(DBG_INFO, format, args);
  va_end(args);
}

void DBG_BLK_2_dbg_dump (const void* data, dword data_length) {
  dbg_dump (DBG_INFO, (byte*)data, data_length, "DBG_BLK", 0, 0);
}


#ifndef _NO_DEBUG_MGNT
static void* diva_mgnt_debug_mask_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar)
{
  switch (cmd)
  {
  case CMD_READVAR:
    return &dbg_level;

  case CMD_WRITEVAR:
    dbg_new(*(dword *) info);
    if (change_mask_proc && (dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE)) {
      (*(change_mask_proc))(*(dword *) info);
    }
    break;
  }
  return((void *)0);
}

void* diva_mgnt_debug_proc (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
  if (((dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) && context->fPara != 0) ||
      ((dbg_fd != (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) && context->fPara == 0))
    return (0);

  if (cmd == CMD_EXECUTE) {
    if (dbg_fd == (DIVA_OS_HANDLE)DIVA_OS_INVALID_HANDLE) {
      /*
        Start debug
        */
      if(strlen(local_name) == 0) {
        if(strlen(PRIMARY)) {
          sprintf(local_name, "%s", PRIMARY);
          sprintf(local_version,"%s",REVISION);
        } else {
      	  sprintf(local_name, "%s", start_dbg_timer.serviceName);
          sprintf(local_version,"%s", start_dbg_timer.buildVersion);
        }
      }
      dbg_init(local_name, local_version, console_debug);
      dbg_msg(DBG_ALL, "****************************************************************");
      dbg_msg(DBG_ALL, " " PRIMARY " " BUILDVERSIONSTR " ");
      dbg_msg(DBG_ALL, " Build: " REVISION " (" __DATE__ " " __TIME__ ")");
      dbg_msg(DBG_ALL, " " COPYRIGHT );
      dbg_msg(DBG_ALL, "****************************************************************");

      if (change_mask_proc) {
        (*(change_mask_proc))(dbg_level);
      }
    } else {
      /*
        Stop debug
        */
      dbg_delete();

      if (change_mask_proc) {
        (*(change_mask_proc))(0);
      }
    }
  }

  return ("");
}

OSCONST MAN_INFO debugdir[] =
{
/*  *name,            type,         attrib,            flags,                max_len, */
/*                    *info,        *context,                                         */
/*                    wlock,        *EvQ,                                             */

  {"debug_mask",      MI_HINT,      MI_WRITE,    MI_CALL, sizeof(dword),
                      (void*)&diva_mgnt_debug_mask_proc, 0,
                      0,            0
  },
  {"DebugStart",      MI_EXECUTE,   0,           MI_CALL,                    0,
                      (void*)&diva_mgnt_debug_proc, 0,
                      0,            0
  },
  {"DebugStop",       MI_EXECUTE,   0,           MI_CALL|MI_E,               0,
                      (void*)&diva_mgnt_debug_proc, (void*)"",
                      0,            0
  }
};

void* diva_get_debugdir (void *info, PATH_CTXT *context, int cmd, INST_PARA *Ipar) {
  return ((void*) &debugdir);
}

void dbg_set_mask_callback(diva_debug_mask_callback_proc_t new_callback) {
  change_mask_proc = new_callback;
}
#endif

//--------- end of file --------------

