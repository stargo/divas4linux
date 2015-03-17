
/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
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
#include "debuglib.h"
_DbgHandle_ myDriverDebugHandle = { 0 /*!Registered*/, DBG_HANDLE_VERSION };
_DbgHandle_ *pMyDriverDebugHandle = &myDriverDebugHandle ;
/*****************************************************************************/
#define DBG_FUNC(name) \
void  \
myDbgPrint_##name (char *format, ...) \
{ va_list ap ; \
 if ( pMyDriverDebugHandle->dbg_prt ) \
 { va_start (ap, format) ; \
  (pMyDriverDebugHandle->dbg_prt) \
   (pMyDriverDebugHandle->id, DLI_##name, format, ap) ; \
  va_end (ap) ; \
} }
DBG_FUNC(LOG)
DBG_FUNC(FTL)
DBG_FUNC(ERR)
DBG_FUNC(TRC)
DBG_FUNC(MXLOG)
DBG_FUNC(FTL_MXLOG)
void 
myDbgPrint_EVL (long msgID, ...)
{ va_list ap ;
 if ( pMyDriverDebugHandle->dbg_ev )
 { va_start (ap, msgID) ;
  (pMyDriverDebugHandle->dbg_ev)
   (pMyDriverDebugHandle->id, (unsigned long)msgID, ap) ;
  va_end (ap) ;
} }
DBG_FUNC(REG)
DBG_FUNC(MEM)
DBG_FUNC(SPL)
DBG_FUNC(IRP)
DBG_FUNC(TIM)
DBG_FUNC(BLK)
DBG_FUNC(TAPI)
DBG_FUNC(NDIS)
DBG_FUNC(CONN)
DBG_FUNC(STAT)
DBG_FUNC(SEND)
DBG_FUNC(RECV)
DBG_FUNC(PRV0)
DBG_FUNC(PRV1)
DBG_FUNC(PRV2)
DBG_FUNC(PRV3)
/*****************************************************************************/
/*****************************************************************************/
int
DbgRegister (char *drvName, char *drvTag, unsigned long dbgMask)
{
 int len;
/*
 * deregister (if already registered) and zero out myDriverDebugHandle
 */
 DbgDeregister () ;
/*
 * initialize the debug handle
 */
 myDriverDebugHandle.Version = DBG_HANDLE_VERSION ;
 myDriverDebugHandle.id  = -1 ;
 myDriverDebugHandle.dbgMask = dbgMask | (DL_EVL | DL_FTL | DL_LOG) ;
 len = strlen (drvName) ;
 memcpy (myDriverDebugHandle.drvName, drvName,
         (len < sizeof(myDriverDebugHandle.drvName)) ?
    len : sizeof(myDriverDebugHandle.drvName) - 1) ;
 len = strlen (drvTag) ;
 memcpy (myDriverDebugHandle.drvTag, drvTag,
         (len < sizeof(myDriverDebugHandle.drvTag)) ?
    len : sizeof(myDriverDebugHandle.drvTag) - 1) ;
/*
 * Try to register debugging via old (and only) interface
 */
 dprintf("\000\377", &myDriverDebugHandle) ;
 if ( myDriverDebugHandle.dbg_prt )
 {
  return (1) ;
 }
/*
 * Check if we registered whith an old maint driver (see debuglib.h)
 */
 if ( myDriverDebugHandle.dbg_end != NULL
   /* location of 'dbg_prt' in _OldDbgHandle_ struct */
   && (myDriverDebugHandle.regTime.LowPart ||
       myDriverDebugHandle.regTime.HighPart  ) )
   /* same location as in _OldDbgHandle_ struct */
 {
  dprintf("%s: Cannot log to old maint driver !", drvName) ;
  myDriverDebugHandle.dbg_end =
  ((_OldDbgHandle_ *)&myDriverDebugHandle)->dbg_end ;
  DbgDeregister () ;
 }
 return (0) ;
}
/*****************************************************************************/
void
DbgSetLevel (unsigned long dbgMask)
{
 myDriverDebugHandle.dbgMask = dbgMask | (DL_EVL | DL_FTL | DL_LOG) ;
}
/*****************************************************************************/
void
DbgDeregister (void)
{
 if ( myDriverDebugHandle.dbg_end )
 {
  (myDriverDebugHandle.dbg_end)(&myDriverDebugHandle) ;
 }
 memset (&myDriverDebugHandle, 0, sizeof(myDriverDebugHandle)) ;
}
void  xdi_dbg_xlog (char* x, ...) {
 va_list ap;
 va_start (ap, x);
 if (myDriverDebugHandle.dbg_end &&
   (myDriverDebugHandle.dbg_irq || myDriverDebugHandle.dbg_old) &&
   (myDriverDebugHandle.dbgMask & DL_STAT)) {
  if (myDriverDebugHandle.dbg_irq) {
   (*(myDriverDebugHandle.dbg_irq))(myDriverDebugHandle.id,
       (x[0] != 0) ? DLI_TRC : DLI_XLOG, x, ap);
  } else {
   (*(myDriverDebugHandle.dbg_old))(myDriverDebugHandle.id, x, ap);
  }
 }
 va_end(ap);
}
/*****************************************************************************/
int
DbgRegisterH (_DbgHandle_ *pDebugHandle, char *drvName, char *drvTag, unsigned long dbgMask)
{
 int len;
/*
 * deregister (if already registered) and zero out myDriverDebugHandle
 */
 DbgDeregisterH (pDebugHandle) ;
/*
 * initialize the debug handle
 */
 pDebugHandle->Version = DBG_HANDLE_VERSION ;
 pDebugHandle->id  = -1 ;
 pDebugHandle->dbgMask = dbgMask | (DL_EVL | DL_FTL | DL_LOG) ;
 len = strlen (drvName) ;
 memcpy (pDebugHandle->drvName, drvName,
         (len < sizeof(pDebugHandle->drvName)) ?
    len : sizeof(pDebugHandle->drvName) - 1) ;
 len = strlen (drvTag) ;
 memcpy (pDebugHandle->drvTag, drvTag,
         (len < sizeof(pDebugHandle->drvTag)) ?
    len : sizeof(pDebugHandle->drvTag) - 1) ;
/*
 * Try to register debugging via old (and only) interface
 */
 dprintf("\000\377", pDebugHandle) ;
 if ( pDebugHandle->dbg_prt )
 {
  return (1) ;
 }
/*
 * Check if we registered whith an old maint driver (see debuglib.h)
 */
 if ( pDebugHandle->dbg_end != NULL
   /* location of 'dbg_prt' in _OldDbgHandle_ struct */
   && (pDebugHandle->regTime.LowPart ||
       pDebugHandle->regTime.HighPart  ) )
   /* same location as in _OldDbgHandle_ struct */
 {
  dprintf("%s: Cannot log to old maint driver !", drvName) ;
  pDebugHandle->dbg_end =
  ((_OldDbgHandle_ *)pDebugHandle)->dbg_end ;
  DbgDeregisterH (pDebugHandle) ;
 }
 return (0) ;
}
/*****************************************************************************/
void
DbgSetLevelH (_DbgHandle_ *pDebugHandle, unsigned long dbgMask)
{
 pDebugHandle->dbgMask = dbgMask | (DL_EVL | DL_FTL | DL_LOG) ;
}
/*****************************************************************************/
void
DbgDeregisterH (_DbgHandle_ *pDebugHandle)
{
 if ( pDebugHandle->dbg_end )
 {
  (pDebugHandle->dbg_end)(pDebugHandle) ;
 }
 memset (pDebugHandle, 0, sizeof(*pDebugHandle)) ;
}
/*****************************************************************************/
void  xdi_dbg_xlogH (_DbgHandle_ *pDebugHandle, char* x, ...) {
 va_list ap;
 va_start (ap, x);
 if (pDebugHandle->dbg_end &&
   (pDebugHandle->dbg_irq || pDebugHandle->dbg_old) &&
   (pDebugHandle->dbgMask & DL_STAT)) {
  if (pDebugHandle->dbg_irq) {
   (*(pDebugHandle->dbg_irq))(pDebugHandle->id,
       (x[0] != 0) ? DLI_TRC : DLI_XLOG, x, ap);
  } else {
   (*(pDebugHandle->dbg_old))(pDebugHandle->id, x, ap);
  }
 }
 va_end(ap);
}
/*****************************************************************************/
