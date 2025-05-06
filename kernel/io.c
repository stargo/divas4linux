
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2004-2017
  Copyright 2000-2003 by Armin Schindler (mac@melware.de)
  Copyright 2000-2003 Cytronics & Melware (info@melware.de)

 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of Adapters.
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

#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "pr_pc.h"
#include "divasync.h"
#define MIPS_SCOM
#include "pkmaint.h" /* pc_main.h, packed in os-dependent fashion */
#include "di.h"
#include "mi_pc.h"
#include "io.h"
#include "vidi_di.h"
#include "dsrv_4prie.h"

extern ADAPTER * adapter[MAX_ADAPTER];
extern PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
void request (PISDN_ADAPTER, ENTITY *);
void pcm_req (PISDN_ADAPTER, ENTITY *);
/* --------------------------------------------------------------------------
  local functions
  -------------------------------------------------------------------------- */
#define ReqFunc(N) \
static void Request##N(ENTITY *e) \
{ if ( IoAdapters[N] ) (* IoAdapters[N]->DIRequest)(IoAdapters[N], e) ; }
ReqFunc(0)
ReqFunc(1)
ReqFunc(2)
ReqFunc(3)
ReqFunc(4)
ReqFunc(5)
ReqFunc(6)
ReqFunc(7)
ReqFunc(8)
ReqFunc(9)
ReqFunc(10)
ReqFunc(11)
ReqFunc(12)
ReqFunc(13)
ReqFunc(14)
ReqFunc(15)
ReqFunc(16)
ReqFunc(17)
ReqFunc(18)
ReqFunc(19)
ReqFunc(20)
ReqFunc(21)
ReqFunc(22)
ReqFunc(23)
ReqFunc(24)
ReqFunc(25)
ReqFunc(26)
ReqFunc(27)
ReqFunc(28)
ReqFunc(29)
ReqFunc(30)
ReqFunc(31)
IDI_CALL Requests[MAX_ADAPTER] =
{ &Request0, &Request1, &Request2, &Request3,
 &Request4, &Request5, &Request6, &Request7,
 &Request8, &Request9, &Request10, &Request11,
 &Request12, &Request13, &Request14, &Request15,
 &Request16, &Request17, &Request18, &Request19,
 &Request20, &Request21, &Request22, &Request23,
 &Request24, &Request25, &Request26, &Request27,
 &Request28, &Request29, &Request30, &Request31
};
static void diva_oob_put (PISDN_ADAPTER IoAdapter, ENTITY* e);
static void diva_process_streaming_mapping (PISDN_ADAPTER IoAdapter, IDI_SYNC_REQ *syncReq);
/*****************************************************************************/
/*
  This array should indicate all new services, that this version of XDI
  is able to provide to his clients
  */
static byte extended_xdi_features[DIVA_XDI_EXTENDED_FEATURES_MAX_SZ+1] = {
 (DIVA_XDI_EXTENDED_FEATURES_VALID       |
  DIVA_XDI_EXTENDED_FEATURE_CMA          |
  DIVA_XDI_EXTENDED_FEATURE_SDRAM_BAR    |
  DIVA_XDI_EXTENDED_FEATURE_CAPI_PRMS    |
#if defined(DIVA_IDI_RX_DMA)
  DIVA_XDI_EXTENDED_FEATURE_RX_DMA       |
  DIVA_XDI_EXTENDED_FEATURE_MANAGEMENT_DMA |
#endif
  DIVA_XDI_EXTENDED_FEATURE_NO_CANCEL_RC),
  (DIVA_XDI_EXTENDED_FEATURE_FC_OK_LABEL
   | DIVA_XDI_EXTENDED_FEATURE_XON_OOB_REQ),
 0
};
/*****************************************************************************/
void
dump_xlog_buffer (PISDN_ADAPTER IoAdapter, Xdesc *xlogDesc, dword max_size)
{
 dword   logLen ;
 word *Xlog   = xlogDesc->buf ;
 word  logCnt = xlogDesc->cnt ;
 word  logOut = xlogDesc->out / sizeof(*Xlog) ;
 word  logOutNext ;
 DBG_FTL(("%s: ************* XLOG recovery (%d) *************",
          &IoAdapter->Name[0], (int)logCnt))
 DBG_FTL(("Microcode: %s", &IoAdapter->ProtocolIdString[0]))
 if ( (logOut + 1) * sizeof(*Xlog) > max_size )
 {
  if ( logCnt > 2 )
  {
   DBG_FTL(("Possibly corrupted XLOG: %d entries left (%ld)",
            (int)logCnt, (long)max_size))
  }
 }
 else
 {
  for ( ; logCnt > 0 ; --logCnt )
  {
   if ( !Xlog[logOut] )
   {
    if ( --logCnt == 0 )
     break ;
    logOut = 0 ;
   }
   logOutNext = (Xlog[logOut] + 1) / sizeof(*Xlog) ;
   if ( (logOutNext + 1) * sizeof(*Xlog) > max_size )
   {
    if ( logCnt > 2 )
    {
     DBG_FTL(("Possibly corrupted XLOG: %d entries left (%ld)",
              (int)logCnt, (long)max_size))
    }
    break;
   }
   if ( Xlog[logOut] < (logOut + 1) * sizeof(*Xlog) )
   {
    if ( logCnt > 2 )
    {
     DBG_FTL(("Possibly corrupted XLOG: %d entries left",
              (int)logCnt))
    }
    break ;
   }
   logLen = (dword)(Xlog[logOut] - (logOut + 1) * sizeof(*Xlog)) ;
   DBG_FTL_MXLOG(( (char *)&Xlog[logOut + 1], (dword)logLen ))
   logOut = logOutNext ;
  }
 }
 DBG_FTL(("%s: ***************** end of XLOG *****************",
          &IoAdapter->Name[0]))
}
/*****************************************************************************/
char *(ExceptionCauseTable[]) =
{
 "Interrupt",
 "TLB mod /IBOUND",
 "TLB load /DBOUND",
 "TLB store",
 "Address error load",
 "Address error store",
 "Instruction load bus error",
 "Data load/store bus error",
 "Syscall",
 "Breakpoint",
 "Reverd instruction",
 "Coprocessor unusable",
 "Overflow",
 "TRAP",
 "VCEI",
 "Floating Point Exception",
 "CP2",
 "Reserved 17",
 "Reserved 18",
 "Reserved 19",
 "Reserved 20",
 "Reserved 21",
 "Reserved 22",
 "WATCH",
 "Reserved 24",
 "Reserved 25",
 "Reserved 26",
 "Reserved 27",
 "Reserved 28",
 "Reserved 29",
 "Reserved 30",
 "VCED"
} ;
void
dump_trap_frame (PISDN_ADAPTER IoAdapter, byte *exceptionFrame)
{
 MP_XCPTC *xcept = (MP_XCPTC *)exceptionFrame ;
 dword    *regs  = &xcept->regs[0] ;
 DBG_FTL(("%s: ***************** CPU TRAPPED *****************",
          &IoAdapter->Name[0]))
 DBG_FTL(("Microcode: %s", &IoAdapter->ProtocolIdString[0]))
 DBG_FTL(("Cause: %s",
          ExceptionCauseTable[(xcept->cr & 0x0000007c) >> 2]))
 DBG_FTL(("sr    0x%08x cr    0x%08x epc   0x%08x vaddr 0x%08x",
          xcept->sr, xcept->cr, xcept->epc, xcept->vaddr))
 DBG_FTL(("zero  0x%08x at    0x%08x v0    0x%08x v1    0x%08x",
          regs[ 0], regs[ 1], regs[ 2], regs[ 3]))
 DBG_FTL(("a0    0x%08x a1    0x%08x a2    0x%08x a3    0x%08x",
          regs[ 4], regs[ 5], regs[ 6], regs[ 7]))
 DBG_FTL(("t0    0x%08x t1    0x%08x t2    0x%08x t3    0x%08x",
          regs[ 8], regs[ 9], regs[10], regs[11]))
 DBG_FTL(("t4    0x%08x t5    0x%08x t6    0x%08x t7    0x%08x",
          regs[12], regs[13], regs[14], regs[15]))
 DBG_FTL(("s0    0x%08x s1    0x%08x s2    0x%08x s3    0x%08x",
          regs[16], regs[17], regs[18], regs[19]))
 DBG_FTL(("s4    0x%08x s5    0x%08x s6    0x%08x s7    0x%08x",
          regs[20], regs[21], regs[22], regs[23]))
 DBG_FTL(("t8    0x%08x t9    0x%08x k0    0x%08x k1    0x%08x",
          regs[24], regs[25], regs[26], regs[27]))
 DBG_FTL(("gp    0x%08x sp    0x%08x s8    0x%08x ra    0x%08x",
          regs[28], regs[29], regs[30], regs[31]))
 DBG_FTL(("md    0x%08x|%08x         resvd 0x%08x class 0x%08x",
          xcept->mdhi, xcept->mdlo, xcept->reseverd, xcept->xclass))
}
/* --------------------------------------------------------------------------
  Real XDI Request function
  -------------------------------------------------------------------------- */
void request(PISDN_ADAPTER IoAdapter, ENTITY * e)
{
 byte i;
 diva_os_spin_lock_magic_t irql;
/*
 * if the Req field in the entity structure is 0,
 * we treat this request as a special function call
 */
 if ( !e->Req )
 {
  IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ *)e ;
  switch (e->Rc)
  {
#if defined(DIVA_IDI_RX_DMA)
    case IDI_SYNC_REQ_XON:
      diva_oob_put (IoAdapter, e);
      return;

    case IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION: {
      diva_xdi_dma_descriptor_operation_t* pI = \
                                   &syncReq->xdi_dma_descriptor_operation.info;
      if (IoAdapter->host_vidi.vidi_active != 0) {
        if (pI->operation == IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC) {
          pI->descriptor_number   = 1;
          pI->descriptor_address  = IntToPtr(1);
          pI->descriptor_magic    = 1;
          pI->operation           = 0;
        } else {
          pI->descriptor_number = -1;
          pI->operation         = 0;
        }
        return;
      }
      if (!IoAdapter->dma_map) {
        pI->operation         = -1;
        pI->descriptor_number = -1;
        return;
      }
      diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "dma_op");
      if (pI->operation == IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC) {
        pI->descriptor_number = diva_alloc_dma_map_entry (\
                               (struct _diva_dma_map_entry*)IoAdapter->dma_map);
        if (pI->descriptor_number >= 0) {
          unsigned long dma_magic;
          void* local_addr;
#if 0
          DBG_TRC(("A(%d) dma_alloc(%d)",
                   IoAdapter->ANum, pI->descriptor_number))
#endif
          diva_get_dma_map_entry (\
                               (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                               pI->descriptor_number,
                               &local_addr, &dma_magic);
          pI->descriptor_address  = local_addr;
          pI->descriptor_magic    = (dword)dma_magic;
          pI->operation           = 0;
        } else {
          pI->operation           = -1;
        }
      } else if ((pI->operation == IDI_SYNC_REQ_DMA_DESCRIPTOR_FREE) &&
                 (pI->descriptor_number >= 0)) {
#if 0
        DBG_TRC(("A(%d) dma_free(%d)", IoAdapter->ANum, pI->descriptor_number))
#endif
        diva_free_dma_map_entry((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                pI->descriptor_number);
        pI->descriptor_number = -1;
        pI->operation         = 0;
      } else {
        pI->descriptor_number = -1;
        pI->operation         = -1;
      }
      diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "dma_op");
    } return;
    case IDI_SYNC_REQ_XDI_MAP_ADDRESS:
      if (IoAdapter->dma_map != 0 && /* syncReq->xdi_map_address.info.bus_addr_hi == 0 && */
          IoAdapter->ANum == syncReq->xdi_map_address.info.logical_adapter_number   &&
          syncReq->xdi_map_address.info.info >= sizeof(syncReq->xdi_map_address.info)) {
        unsigned long phys, bus_addr_hi;
        dword bus_addr = syncReq->xdi_map_address.info.bus_addr_lo;
				dword requested_bus_addr_hi = syncReq->xdi_map_address.info.bus_addr_hi;
        void* virt;
        int ii = 0;
        do {
          virt = 0;
          diva_get_dma_map_entry (IoAdapter->dma_map,    ii, &virt, &phys);
          diva_get_dma_map_entry_hi (IoAdapter->dma_map, ii, &bus_addr_hi);
          if (virt != 0 && bus_addr_hi == requested_bus_addr_hi && phys <= bus_addr && bus_addr < phys+4*1024) {
              syncReq->xdi_map_address.info.info   = sizeof(syncReq->xdi_map_address.info);
              syncReq->xdi_map_address.info.addr   = virt;
              syncReq->xdi_map_address.info.handle = ii;
              syncReq->xdi_map_address.info.offset = (dword)(bus_addr - phys);
              syncReq->xdi_map_address.Rc = 0xff;
              return;
          }
          ii++;
        } while (virt != 0);
      }
      return;


    case IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING:
			if (IoAdapter->dma_map != 0) {
				diva_process_streaming_mapping (IoAdapter, syncReq);
			}
      return;

    case IDI_SYNC_REQ_HW_ACCESS:
      if (IoAdapter->get_hw_ifc != 0) {
        IoAdapter->get_hw_ifc (IoAdapter, &syncReq->diva_xdi_hw_access.info);
      }
      return;


#endif
    case IDI_SYNC_REQ_XDI_ADAPTER_REMOVED_NOT_AVAILABLE:
      DBG_ERR(("A(%d) received stop adapter command", IoAdapter->ANum))
      IoAdapter->host_vidi.cpu_stop_message_received = 1;
      if (IoAdapter->os_trap_nfy_Fnc != 0) {
        (*(IoAdapter->os_trap_nfy_Fnc))(IoAdapter, IoAdapter->ANum);
      }
      return;
    case IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER:
      if (IoAdapter->get_hw_ifc == 0) {
        diva_xdi_get_logical_adapter_number_s_t *pI = \
                                     &syncReq->xdi_logical_adapter_number.info;
        pI->logical_adapter_number = IoAdapter->ANum;
        pI->controller = IoAdapter->ControllerNumber;
        pI->total_controllers = IoAdapter->Properties.Adapters;
      }
      return;
    case IDI_SYNC_REQ_XDI_GET_CAPI_PARAMS: {
       diva_xdi_get_capi_parameters_t prms, *pI = &syncReq->xdi_capi_prms.info;
       memset (&prms, 0x00, sizeof(prms));
       prms.structure_length = MIN(sizeof(prms), pI->structure_length);
       memset (pI, 0x00, pI->structure_length);
       prms.flag_dynamic_l1_down = (IoAdapter->capi_cfg.cfg_1 & \
         DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON) ? 1 : 0;
       prms.group_optimization_enabled = (IoAdapter->capi_cfg.cfg_1 & \
         DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON) ? 1 : 0;
       memcpy (pI, &prms, prms.structure_length);
      } return;
    case IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR:
      syncReq->xdi_sdram_bar.info.bar = IoAdapter->sdram_bar;
      return;
    case IDI_SYNC_REQ_XDI_GET_EXTENDED_FEATURES: {
      dword dwi;
      diva_xdi_get_extended_xdi_features_t* pI =\
                                 &syncReq->xdi_extended_features.info;
      pI->buffer_length_in_bytes &= ~0x80000000;
      if (pI->buffer_length_in_bytes && pI->features) {
        memset (pI->features, 0x00, pI->buffer_length_in_bytes);
      }
      for (dwi = 0; ((pI->features) && (dwi < pI->buffer_length_in_bytes) &&
                   (dwi < DIVA_XDI_EXTENDED_FEATURES_MAX_SZ)); dwi++) {
        pI->features[dwi] = extended_xdi_features[dwi];
      }
      if ((pI->buffer_length_in_bytes < DIVA_XDI_EXTENDED_FEATURES_MAX_SZ) ||
          (!pI->features)) {
        pI->buffer_length_in_bytes =\
                           (0x80000000 | DIVA_XDI_EXTENDED_FEATURES_MAX_SZ);
      }
     } return;

  case IDI_SYNC_REQ_XDI_CLOCK_CONTROL:
    if (IoAdapter->clock_control_Fnc != 0) {
      diva_xdi_clock_control_command_t* cmd = &syncReq->diva_xdi_clock_control_command_req.info;

      if ((*(IoAdapter->clock_control_Fnc))(IoAdapter, cmd) == 0) {
        syncReq->diva_xdi_clock_control_command_req.Rc = 0xff;
      }
    }
    return;

  case IDI_SYNC_REQ_GET_NAME:
   if ( IoAdapter )
   {
    strcpy ((char*)&syncReq->GetName.name[0], (char*)IoAdapter->Name) ;
    DBG_TRC(("xdi: Adapter %d / Name '%s'",
             IoAdapter->ANum, IoAdapter->Name))
    return ;
   }
   syncReq->GetName.name[0] = '\0' ;
   break ;
  case IDI_SYNC_REQ_GET_SERIAL:
   if ( IoAdapter )
   {
    syncReq->GetSerial.serial = IoAdapter->serialNo ;
    DBG_TRC(("xdi: Adapter %d / SerialNo %ld",
             IoAdapter->ANum, IoAdapter->serialNo))
    return ;
   }
   syncReq->GetSerial.serial = 0 ;
   break ;
  case IDI_SYNC_REQ_GET_CARDTYPE:
   if ( IoAdapter )
   {
    syncReq->GetCardType.cardtype = IoAdapter->cardType ;
    DBG_TRC(("xdi: Adapter %d / CardType %ld",
             IoAdapter->ANum, IoAdapter->cardType))
    return ;
   }
   syncReq->GetCardType.cardtype = 0 ;
   break ;
  case IDI_SYNC_REQ_GET_XLOG:
   if ( IoAdapter )
   {
    pcm_req (IoAdapter, e) ;
    return ;
   }
   e->Ind = 0 ;
   break ;
  case IDI_SYNC_REQ_GET_DBG_XLOG:
   if ( IoAdapter )
   {
    pcm_req (IoAdapter, e) ;
    return ;
   }
   e->Ind = 0 ;
   break ;
#if 0
  case IDI_SYNC_REQ_GET_FEATURES:
   if ( IoAdapter )
   {
    syncReq->GetFeatures.features =
      (unsigned short)IoAdapter->features ;
    return ;
   }
   syncReq->GetFeatures.features = 0 ;
   break ;
#endif
  case IDI_SYNC_REQ_PORTDRV_HOOK:
   if ( IoAdapter )
   {
    DBG_TRC(("Xdi:IDI_SYNC_REQ_PORTDRV_HOOK - ignored"))
    return ;
   }
   break;
  case IDI_SYNC_REQ_XDI_SAVE_MAINT_BUFFER_OPERATION:
   if (IoAdapter != 0 && IoAdapter->save_maint_buffer_proc != 0) {
    diva_xdi_save_maint_buffer_operation_t* pI =
      &syncReq->xdi_save_maint_buffer_operation.info;
    pI->success =
      (*(IoAdapter->save_maint_buffer_proc))(IoAdapter, pI->start, pI->length);
   }
   break;
  case IDI_SYNC_REQ_GET_ADAPTER_DEBUG_STORAGE_ADDRESS:
   if ( IoAdapter ) {
    diva_get_adapter_debug_storage_address_t *pI =
     &syncReq->diva_get_adapter_debug_storage_address.info ;
    if ( IoAdapter->diva_get_adapter_debug_storage_address_proc )
     (*(IoAdapter->diva_get_adapter_debug_storage_address_proc))(IoAdapter, (void *)pI);
    else {
     pI->storage_start  = NULL ;
     pI->storage_length = 0 ;
    }
    return ;
   }
   break ;
  }
  if ( IoAdapter )
  {
#if 0
   DBG_FTL(("xdi: unknown Req 0 / Rc %d !", e->Rc))
#endif
   return ;
  }
 }
 DBG_TRC(("xdi: Id 0x%x / Req 0x%x / Rc 0x%x", e->Id, e->Req, e->Rc))
 if ( !IoAdapter )
 {
  DBG_FTL(("xdi: uninitialized Adapter used - ignore request"))
  return ;
 }
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req");
/*
 * assign an entity
 */
 if ( !(e->Id &0x1f) )
 {
  if ( IoAdapter->e_count >= IoAdapter->e_max )
  {
   DBG_FTL(("xdi: all Ids in use (max=%d) --> Req ignored",
            IoAdapter->e_max))
   diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req");
   return ;
  }
/*
 * find a new free id
 */
  for ( i = 1 ; IoAdapter->e_tbl[i].e ; ++i ) ;
  IoAdapter->e_tbl[i].e = e ;
  IoAdapter->e_count++ ;
  e->No = (byte)i ;
  e->More = 0 ;
  e->RCurrent = 0xff ;
 }
 else
 {
  i = e->No ;
 }
/*
 * if the entity is still busy, ignore the request call
 */
 if ( e->More & XBUSY )
 {
  DBG_FTL(("xdi: Id 0x%x busy --> Req 0x%x ignored", e->Id, e->Req))
  if ( !IoAdapter->trapped && IoAdapter->trapFnc )
  {
   IoAdapter->trapFnc (IoAdapter) ;
      /*
        Firs trap, also notify user if supported
       */
      if (IoAdapter->trapped && IoAdapter->os_trap_nfy_Fnc) {
        (*(IoAdapter->os_trap_nfy_Fnc))(IoAdapter, IoAdapter->ANum);
      }
  }
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req");
  return ;
 }
/*
 * initialize transmit status variables
 */
 e->More |= XBUSY ;
 e->More &= ~XMOREF ;
 e->XCurrent = 0 ;
 e->XOffset = 0 ;
/*
 * queue this entity in the adapter request queue
 */
 IoAdapter->e_tbl[i].next = 0 ;
 if ( IoAdapter->head )
 {
  IoAdapter->e_tbl[IoAdapter->tail].next = i ;
  IoAdapter->tail = i ;
 }
 else
 {
  IoAdapter->head = i ;
  IoAdapter->tail = i ;
 }
/*
 * queue the DPC to process the request
 */
 diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req");
}
/* ---------------------------------------------------------------------
  Main DPC routine
   --------------------------------------------------------------------- */
void DIDpcRoutine (struct _diva_os_soft_isr* psoft_isr, void* Context) {
 PISDN_ADAPTER IoAdapter  = (PISDN_ADAPTER)Context ;
 ADAPTER* a        = &IoAdapter->a ;
 diva_os_atomic_t* pin_dpc = &IoAdapter->in_dpc;
 if (diva_os_atomic_increment (pin_dpc) == 1) {
  do {
   if ( IoAdapter->tst_irq (a) )
   {
    /* IDI_SYNC_REQ_XDI_ADAPTER_REMOVED_NOT_AVAILABLE received from Mtpx Adapter */
    if (!IoAdapter->Unavailable && !IoAdapter->host_vidi.cpu_stop_message_received) {
     IoAdapter->dpc (a);
    }
    IoAdapter->clr_irq (a);
   }
   if (!IoAdapter->Unavailable && !IoAdapter->host_vidi.cpu_stop_message_received) {
     IoAdapter->out (a) ;
   }
  } while (diva_os_atomic_decrement (pin_dpc) > 0);

	{
		diva_os_spin_lock_magic_t irql;
		void (*shedule_stream_proc)(const void*);
		const void*               shedule_stream_proc_context;

		diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
		shedule_stream_proc = IoAdapter->shedule_stream_proc;
		shedule_stream_proc_context = IoAdapter->shedule_stream_proc_context;
		diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");

		if (likely(shedule_stream_proc != 0 && shedule_stream_proc_context != 0)) {
			(*shedule_stream_proc)(shedule_stream_proc_context);
		}
	}

  /* ----------------------------------------------------------------
    Look for XLOG request (cards with indirect addressing)
    ---------------------------------------------------------------- */
  if (IoAdapter->pcm_pending) {
   struct pc_maint *pcm;
   diva_os_spin_lock_magic_t OldIrql ;
   diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
                &OldIrql,
                "data_dpc");
   pcm = (struct pc_maint *)IoAdapter->pcm_data;
   switch (IoAdapter->pcm_pending) {
    case 1: /* ask card for XLOG */
     a->ram_out (a, &IoAdapter->pcm->rc, 0) ;
     a->ram_out (a, &IoAdapter->pcm->req, pcm->req) ;
     IoAdapter->pcm_pending = 2;
     break;
    case 2: /* Try to get XLOG from the card */
     if ((int)(a->ram_in (a, &IoAdapter->pcm->rc))) {
      a->ram_in_buffer (a, IoAdapter->pcm, pcm, sizeof(*pcm)) ;
      IoAdapter->pcm_pending = 3;
     }
     break;
    case 3: /* let XDI recovery XLOG */
     break;
   }
   diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
                &OldIrql,
                "data_dpc");
  }
  /* ---------------------------------------------------------------- */
 }
}
/* --------------------------------------------------------------------------
  XLOG interface
  -------------------------------------------------------------------------- */
void
pcm_req (PISDN_ADAPTER IoAdapter, ENTITY *e)
{
 diva_os_spin_lock_magic_t OldIrql ;
 int              i, rc ;
 ADAPTER         *a = &IoAdapter->a ;
 struct pc_maint *pcm = (struct pc_maint *)&e->Ind ;
#if !defined(DIVA_SOFTIP)
 if (IoAdapter->host_vidi.vidi_active != 0) {
  if (vidi_pcm_req (IoAdapter, e) != 0)
   goto Trapped;
  return;
 }
#endif
/*
 * special handling of I/O based card interface
 * the memory access isn't an atomic operation !
 */
 if ( IoAdapter->Properties.Card == CARD_MAE )
 {
  diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
               &OldIrql,
               "data_pcm_1");
  IoAdapter->pcm_data = (void *)pcm;
  IoAdapter->pcm_pending = 1;
  diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
               &OldIrql,
               "data_pcm_1");
  for ( rc = 0, i = (IoAdapter->trapped ? 3000 : 250) ; !rc && (i > 0) ; --i )
  {
   diva_os_sleep (1) ;
   if (IoAdapter->pcm_pending == 3) {
    diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
                 &OldIrql,
                 "data_pcm_3");
    IoAdapter->pcm_pending = 0;
    IoAdapter->pcm_data    = NULL ;
    diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
                 &OldIrql,
                 "data_pcm_3");
    return ;
   }
   diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
                &OldIrql,
                "data_pcm_2");
   diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
   diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
                &OldIrql,
                "data_pcm_2");
  }
  diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
               &OldIrql,
               "data_pcm_4");
  IoAdapter->pcm_pending = 0;
  IoAdapter->pcm_data    = NULL ;
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
               &OldIrql,
               "data_pcm_4");
  goto Trapped ;
 }
/*
 * memory based shared ram is accessable from different
 * processors without disturbing concurrent processes.
 */
 a->ram_out (a, &IoAdapter->pcm->rc, 0) ;
 a->ram_out (a, &IoAdapter->pcm->req, pcm->req) ;
 for ( i = (IoAdapter->trapped ? 3000 : 250) ; --i > 0 ; )
 {
  diva_os_sleep (1) ;
  rc = (int)(a->ram_in (a, &IoAdapter->pcm->rc)) ;
  if ( rc )
  {
   a->ram_in_buffer (a, IoAdapter->pcm, pcm, sizeof(*pcm)) ;
   return ;
  }
 }
Trapped:
 if ( IoAdapter->trapFnc )
 {
    int trapped = IoAdapter->trapped;
  IoAdapter->trapFnc (IoAdapter) ;
    /*
      Firs trap, also notify user if supported
     */
    if (!trapped && IoAdapter->trapped && IoAdapter->os_trap_nfy_Fnc) {
      (*(IoAdapter->os_trap_nfy_Fnc))(IoAdapter, IoAdapter->ANum);
    }
 }
}
/*------------------------------------------------------------------*/
/* ram access functions for memory mapped cards                     */
/*------------------------------------------------------------------*/
byte mem_in (ADAPTER *a, void *addr)
{
 volatile byte* Base = (byte*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 return (*Base) ;
}
word mem_inw (ADAPTER *a, void *addr)
{
 volatile word* Base = (word*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 return (*Base) ;
}
void mem_in_dw (ADAPTER *a, void *addr, dword* data, int dwords)
{
 volatile dword* Base = (dword*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 while (dwords--) {
  *data++ = *Base++;
 }
}
void mem_in_buffer (ADAPTER *a, void *addr, void *buffer, word length)
{
 byte* Base = (byte*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 memcpy (buffer, Base, length) ;
}
void mem_look_ahead (ADAPTER *a, PBUFFER *RBuffer, ENTITY *e)
{
 PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io ;
 IoAdapter->RBuffer.length = mem_inw (a, &RBuffer->length) ;
 mem_in_buffer (a, RBuffer->P, IoAdapter->RBuffer.P,
                IoAdapter->RBuffer.length) ;
 e->RBuffer = (DBUFFER *)&IoAdapter->RBuffer ;
}
void mem_out (ADAPTER *a, void *addr, byte data)
{
 volatile byte* Base = (byte*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 *Base = data ;
}
void mem_outw (ADAPTER *a, void *addr, word data)
{
 volatile word* Base = (word*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 *Base = data ;
}
void mem_out_dw (ADAPTER *a, void *addr, const dword* data, int dwords)
{
 volatile dword* Base = (dword*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 while (dwords--) {
  *Base++ = *data++;
 }
}
void mem_out_buffer (ADAPTER *a, void *addr, void *buffer, word length)
{
 byte* Base = (byte*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 memcpy (Base, buffer, length) ;
}
void mem_inc (ADAPTER *a, void *addr)
{
 volatile byte* Base = (byte*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)] ;
 byte  x = *Base ;
 *Base = x + 1 ;
}
void* mem_cma_va (ADAPTER* a, void* addr) {
  void* Base = (void*)&((PISDN_ADAPTER)a->io)->ram[PtrToUlong(addr)];
  return (Base);
}
/*------------------------------------------------------------------*/
/* ram access functions for io-mapped cards                         */
/*------------------------------------------------------------------*/
byte io_in(ADAPTER * a, void * adr)
{
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  /* 4 seems to be ADDR */
  return inpp(((PISDN_ADAPTER)a->io)->port);
}
word io_inw(ADAPTER * a, void * adr)
{
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  return inppw(((PISDN_ADAPTER)a->io)->port);
}
void io_in_buffer(ADAPTER * a, void * adr, void * buffer, word len)
{
 byte* P = (byte*)buffer;
  if (PtrToLong(adr) & 1) {
    outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
    *P = inpp(((PISDN_ADAPTER)a->io)->port);
    P++;
    adr = ((byte *) adr) + 1;
    len--;
    if (!len) return;
  }
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  inppw_buffer (((PISDN_ADAPTER)a->io)->port, P, len+1);
}
void io_look_ahead(ADAPTER * a, PBUFFER * RBuffer, ENTITY * e)
{
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(RBuffer));
  ((PISDN_ADAPTER)a->io)->RBuffer.length = inppw(((PISDN_ADAPTER)a->io)->port);
  inppw_buffer (((PISDN_ADAPTER)a->io)->port, ((PISDN_ADAPTER)a->io)->RBuffer.P, ((PISDN_ADAPTER)a->io)->RBuffer.length + 1);
  e->RBuffer = (DBUFFER *) &(((PISDN_ADAPTER)a->io)->RBuffer);
}
void io_out(ADAPTER * a, void * adr, byte data)
{
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  outpp(((PISDN_ADAPTER)a->io)->port, data);
}
void io_outw(ADAPTER * a, void * adr, word data)
{
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  outppw(((PISDN_ADAPTER)a->io)->port, data);
}
void io_out_buffer(ADAPTER * a, void * adr, void * buffer, word len)
{
 byte* P = (byte*)buffer;
  if (PtrToLong(adr) & 1) {
    outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
    outpp(((PISDN_ADAPTER)a->io)->port, *P);
    P++;
    adr = ((byte *) adr) + 1;
    len--;
    if (!len) return;
  }
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  outppw_buffer (((PISDN_ADAPTER)a->io)->port, P, len+1);
}
void io_inc(ADAPTER * a, void * adr)
{
  byte x;
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  x = inpp(((PISDN_ADAPTER)a->io)->port);
  outppw(((PISDN_ADAPTER)a->io)->port+4, PtrToUshort(adr));
  outpp(((PISDN_ADAPTER)a->io)->port, x+1);
}
/*------------------------------------------------------------------*/
/* OS specific functions related to queuing of entities             */
/*------------------------------------------------------------------*/
void free_entity(ADAPTER * a, byte e_no)
{
  PISDN_ADAPTER IoAdapter;
 diva_os_spin_lock_magic_t irql;
  IoAdapter = (PISDN_ADAPTER) a->io;
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_free");
  IoAdapter->e_tbl[e_no].e = NULL;
  IoAdapter->e_count--;
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_free");
}
void assign_queue(ADAPTER * a, byte e_no, word ref)
{
  PISDN_ADAPTER IoAdapter;
 diva_os_spin_lock_magic_t irql;
  IoAdapter = (PISDN_ADAPTER) a->io;
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_assign");
  IoAdapter->e_tbl[e_no].assign_ref = ref;
  IoAdapter->e_tbl[e_no].next = (byte)IoAdapter->assign;
  IoAdapter->assign = e_no;
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_assign");
}
byte get_assign(ADAPTER * a, word ref)
{
  PISDN_ADAPTER IoAdapter;
 diva_os_spin_lock_magic_t irql;
  byte e_no;
  IoAdapter = (PISDN_ADAPTER) a->io;
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock,
              &irql,
              "data_assign_get");
  for(e_no = (byte)IoAdapter->assign;
      e_no && IoAdapter->e_tbl[e_no].assign_ref!=ref;
      e_no = IoAdapter->e_tbl[e_no].next);
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock,
              &irql,
              "data_assign_get");
  return e_no;
}
void req_queue(ADAPTER * a, byte e_no)
{
  PISDN_ADAPTER IoAdapter;
 diva_os_spin_lock_magic_t irql;
  IoAdapter = (PISDN_ADAPTER) a->io;
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req_q");
  IoAdapter->e_tbl[e_no].next = 0;
  if(IoAdapter->head) {
    IoAdapter->e_tbl[IoAdapter->tail].next = e_no;
    IoAdapter->tail = e_no;
  }
  else {
    IoAdapter->head = e_no;
    IoAdapter->tail = e_no;
  }
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req_q");
}
byte look_req(ADAPTER * a)
{
  PISDN_ADAPTER IoAdapter;
  IoAdapter = (PISDN_ADAPTER) a->io;
  return ((byte)IoAdapter->head) ;
}
void next_req(ADAPTER * a)
{
  PISDN_ADAPTER IoAdapter;
 diva_os_spin_lock_magic_t irql;
  IoAdapter = (PISDN_ADAPTER) a->io;
 diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req_next");
  IoAdapter->head = IoAdapter->e_tbl[IoAdapter->head].next;
  if(!IoAdapter->head) IoAdapter->tail = 0;
 diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "data_req_next");
}
/*------------------------------------------------------------------*/
/* memory map functions                                             */
/*------------------------------------------------------------------*/
ENTITY * entity_ptr(ADAPTER * a, byte e_no)
{
  PISDN_ADAPTER IoAdapter;
  IoAdapter = (PISDN_ADAPTER) a->io;
  return (IoAdapter->e_tbl[e_no].e);
}
void * PTR_X(ADAPTER * a, ENTITY * e)
{
  return ((void *) e->X);
}
void * PTR_R(ADAPTER * a, ENTITY * e)
{
  return ((void *) e->R);
}
void * PTR_P(ADAPTER * a, ENTITY * e, void * P)
{
  return P;
}
void CALLBACK(ADAPTER * a, ENTITY * e)
{
 if ( e && e->callback )
  e->callback (e) ;
}
/* --------------------------------------------------------------------------
  routines for aligned reading and writing on RISC
  -------------------------------------------------------------------------- */
void outp_words_from_buffer (word* adr, byte* P, dword len)
{
  dword i = 0;
  word w;
  while (i < (len & 0xfffffffe)) {
    w = P[i++];
    w += (P[i++])<<8;
    outppw (adr, w);
  }
}
void inp_words_to_buffer (word* adr, byte* P, dword len)
{
  dword i = 0;
  word w;
  while (i < (len & 0xfffffffe)) {
    w = inppw (adr);
    P[i++] = (byte)(w);
    P[i++] = (byte)(w>>8);
  }
}

static void diva_reset_custom_device (volatile byte* device_address, int init, dword* device_control) {
	*device_control = DIVA_CUSTOM_DEVICE_RST_N |
										DIVA_CUSTOM_DEVICE_CS_N  |
										DIVA_CUSTOM_DEVICE_ADV_N |
										DIVA_CUSTOM_DEVICE_WR_N  |
										DIVA_CUSTOM_DEVICE_OE_N;

	diva_os_sleep(1);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control[0];
	diva_os_sleep (1);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control[0] & ~DIVA_CUSTOM_DEVICE_RST_N;
	diva_os_sleep (1);
	if (init != 0)
		device_control[0] &= ~DIVA_CUSTOM_DEVICE_CS_N;
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control[0];
	diva_os_sleep (1);
}

static word diva_read_custom_device_word (volatile byte* device_address, dword address, dword device_control) {
  dword ret;

	*(volatile dword*)&device_address[DIVA_CUSTOM_LOW_ADDRESS_REGISTER] = (word)address;
	*(volatile dword*)&device_address[DIVA_CUSTOM_HI_ADDRESS_REGISTER] = (word)(address >> 16);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~DIVA_CUSTOM_DEVICE_ADV_N;
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] =
													device_control & ~(DIVA_CUSTOM_DEVICE_ADV_N|DIVA_CUSTOM_DEVICE_CS_N|DIVA_CUSTOM_DEVICE_OE_N);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~(DIVA_CUSTOM_DEVICE_CS_N|DIVA_CUSTOM_DEVICE_OE_N);
	ret = *(volatile dword*)&device_address[DIVA_CUSTOM_READ_DATA_REGISTER];
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control;

  return ((word)ret);
}

static void diva_write_custom_device_word (volatile byte* device_address, dword address, dword device_control, word data) {
	*(volatile dword*)&device_address[DIVA_CUSTOM_LOW_ADDRESS_REGISTER] = (word)address;
	*(volatile dword*)&device_address[DIVA_CUSTOM_HI_ADDRESS_REGISTER] = (word)(address >> 16);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~DIVA_CUSTOM_DEVICE_ADV_N;
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~(DIVA_CUSTOM_DEVICE_ADV_N|DIVA_CUSTOM_DEVICE_CS_N);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~(DIVA_CUSTOM_DEVICE_CS_N);
	*(volatile dword*)&device_address[DIVA_CUSTOM_WRITE_DATA_REGISTER] = (dword)data;
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~(DIVA_CUSTOM_DEVICE_CS_N|DIVA_CUSTOM_DEVICE_WR_N);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control & ~(DIVA_CUSTOM_DEVICE_CS_N);
	*(volatile dword*)&device_address[DIVA_CUSTOM_CONTROL_REGISTER_OFFSET] = device_control;
}

static dword diva_get_custom_serial_number (PISDN_ADAPTER IoAdapter) {
	dword device_control;
	dword sn_lo, sn_hi;

	diva_reset_custom_device (IoAdapter->reset, 1, &device_control);

	diva_write_custom_device_word (IoAdapter->reset, 0, device_control, 0x90);
	sn_lo = diva_read_custom_device_word (IoAdapter->reset, 0x85, device_control);
	sn_hi = diva_read_custom_device_word (IoAdapter->reset, 0x85+1, device_control);
	diva_write_custom_device_word (IoAdapter->reset, 0, device_control, 0xff);

	diva_reset_custom_device (IoAdapter->reset, 0, &device_control);

	if ((sn_lo == 0xffff && sn_hi == 0xffff) || (sn_lo == 0 && sn_hi == 0)) {
		DBG_ERR(("A(%d) set serial number to 1", IoAdapter->ANum))
		sn_lo = 1;
		sn_hi = 0;
	}

	IoAdapter->csid       = 0;
	IoAdapter->hw_info[0] = 0;

	return (sn_lo | (sn_hi << 16));
}

/*
 Get serial number and extended info
 Only valid for PLX chip (PCI9054,...) based cards!
 */
dword diva_get_serial_number (PISDN_ADAPTER IoAdapter) {
  dword data[64] ;
  dword serNo ;
  byte csid ;
  word i, j ;
  byte Bus   = (byte)IoAdapter->BusNumber;
  byte Slot  = (byte)IoAdapter->slotNumber;
  int pcie =
    ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE) != 0);

	if ((IDI_PROP_SAFE(IoAdapter->cardType,HardwareFeatures) & DIVA_CARDTYPE_HARDWARE_PROPERTY_CUSTOM_PCIE) != 0) {
		serNo = diva_get_custom_serial_number (IoAdapter);

		IoAdapter->serialNo = serNo;
		DBG_REG(("Serial No.          : %ld", IoAdapter->serialNo))

		return (0);
	}

	if (diva_get_feature_bytes (data,
															sizeof(data),
															(byte)IoAdapter->BusNumber,
															(byte)IoAdapter->slotNumber,
															IoAdapter->hdev, pcie)) {
      return(0) ;
    }

#if 0
 /* The serial number of the first 4Bri-8M Rev.1 prototypes
  (030-534-02) is located in data[32] */
  serNo  = data[32] ;
  if (serNo == 0 || serNo == 0xffffffff)
#endif
  serNo  = data[63] ;
  if (serNo == 0 || serNo == 0xffffffff) {
    DBG_LOG(("Wrong Serial Number, create serial number"));
    serNo  = 0;
    serNo |= Bus  << 16;
    serNo |= Slot << 8;
    serNo |= IoAdapter->ANum;
  }
  IoAdapter->serialNo = serNo;
  DBG_REG(("Serial No.          : %ld", IoAdapter->serialNo))
  /* card subtype id */
  csid   = (byte)data[62] ;
  if (0xff != csid)
  {
    IoAdapter->csid = csid;
    DBG_REG(("CSID                : %x", IoAdapter->csid))
  }
  /*
    Now read additional information
    */
  IoAdapter->hw_info[0] = 0;
  j = (word)((data[0x30]>>16) & 0xff); /* length */
  if ((j == 0xff) || (j == 0) || (j > 59)) {
    DBG_REG(("Additional information length=%d, ignored", j))
  } else {
    for (i=0; i <= j; i+=4) {
      IoAdapter->hw_info[i + 1] = (byte)((data[0x30 + i/4] >> 24) &0xff);
      IoAdapter->hw_info[i + 0] = (byte)((data[0x30 + i/4] >> 16) &0xff);
      IoAdapter->hw_info[i + 3] = (byte)((data[0x30 + i/4] >>  8) &0xff);
      IoAdapter->hw_info[i + 2] = (byte)((data[0x30 + i/4] >>  0) &0xff);
    }
    DBG_REG(("Read %d bytes additional information", IoAdapter->hw_info[0]))
    DBG_BLK(((char*)&IoAdapter->hw_info[1], IoAdapter->hw_info[0]))
  }
  return (serNo) ;
}

int diva_get_feature_bytes (dword* data, dword data_length, byte Bus, byte Slot, void* hdev, int pcie) {
        word addr, status = 0, i, j ;
        word VPDCAP  = 0x82;
        word VPDDATA = 0x84;
        void* vaddr = 0;
        word addr_reg_offset;
        word data_reg_offset;
        byte revID = 0;

        // Retrieve RevisionID from PCI Config Space (HERA/FPGA-Seaville Replacement is >=3 limited to 0x10)
        PCIread(Bus, Slot, 0x08, &revID, sizeof(revID), hdev); 
        if ((revID >=0x3) && (revID <= 0x10)) {
            VPDCAP  = 0x172;
            VPDDATA = 0x174;
            vaddr = divasa_remap_pci_bar(NULL, (divasa_get_pci_bar(Bus, Slot, 0, hdev)), 0x200);
            DBG_TRC(("HERA/FPGA-Seaville Replacement\n"))
            if (!vaddr) {
                DBG_ERR(("ERROR: Unable to Map BAR[0]\n"))
                return (-1);
            }
        }

        addr_reg_offset = (pcie == 0) ? 0x4E : VPDCAP;
        data_reg_offset = (pcie == 0) ? 0x50 : VPDDATA;
        DBG_TRC(("VPDCAP=%04x VPDDATA=%08x", addr_reg_offset, data_reg_offset))

        if (data_length/sizeof(data[0]) < 64) {
                return (-1);
        }

        data[63] = 0x0 ; /* serial number */
        for ( i = 0 ; i < 64 ; ++i )
        {
                addr = i * 4 ;
                for ( j = 0 ; j < 5; ++j )
                {
                        if (vaddr) {
                            iowrite16(addr, vaddr+addr_reg_offset);
                        } else {
                            PCIwrite (Bus, Slot, addr_reg_offset, &addr, sizeof(addr), hdev) ;
                        }
                        diva_os_wait (1) ;
                        if (vaddr) {
                            status = ioread16(vaddr+addr_reg_offset);
                        } else {
                            PCIread (Bus, Slot, addr_reg_offset, &status, sizeof(status), hdev) ;
                        }
                        if (status & 0x8000)
                                break ;
                }
               if (j >= 5){
                        DBG_ERR(("EEPROM[%d] read failed (0x%x)", i * 4, addr))
                        return(-1) ;
                }
                if (vaddr) {
                    data[i] = ioread32(vaddr+data_reg_offset);
                } else {
                    PCIread (Bus, Slot, data_reg_offset, &data[i], sizeof (data[0]), hdev) ;
                }
                if (pcie != 0) {
                        data[i] = (data[i] >> 16) | (data[i] << 16);
                }
        }

        if (vaddr) {
            divasa_unmap_pci_bar(vaddr);
        }

        DBG_BLK(((char *)&data[0], data_length))

        return (0);
}

/******************************************************************************/
#define DSP_SIGNATURE_PROBE_WORD 0x5a5a
int
dsp_check_presence (volatile byte* addr, volatile byte* data, int dsp)
{
  word pattern;
/*
 * Checks presence of DSP on board
 */
  *(volatile word*)addr = 0x4000;
  *(volatile word*)data = DSP_SIGNATURE_PROBE_WORD;
  *(volatile word*)addr = 0x4000;
  pattern = *(volatile word*)data;
 if ( pattern != DSP_SIGNATURE_PROBE_WORD )
 {
  DBG_TRC(("dsp_check_presence: DSP[%d] %04x(is) != %04x(should)",
              dsp, pattern, DSP_SIGNATURE_PROBE_WORD))
    return (-1);
  }
  *(volatile word*)addr = 0x4000;
 *(volatile word *)data = (word)(~DSP_SIGNATURE_PROBE_WORD) ;
  *(volatile word*)addr = 0x4000;
  pattern = *(volatile word*)data;
 if ( pattern != (word)~DSP_SIGNATURE_PROBE_WORD )
 {
  DBG_TRC(("dsp_check_presence: DSP[%d] %04x(is) != %04x(should)",
              dsp, pattern, (word)~DSP_SIGNATURE_PROBE_WORD))
    return (-2);
  }
  DBG_TRC (("DSP[%d] present", dsp))
  return (0);
}

/*
	address and length must be dword aligned
	*/
int diva_dma_write_sdram_block (PISDN_ADAPTER IoAdapter,
																int use_burst,
																int alloc_descriptors,
																dword address,
																const byte* data,
																dword length) {
	int ret = 0;

#ifdef DIVA_VERIFY_DMA_DOWNLOAD
	const byte* ref_data = data;
	dword       ref_address = address;
	dword       ref_length = length;
#endif
	dword       adapter_index;
	PISDN_ADAPTER currentAdapter = IoAdapter;

	if (IoAdapter->dma_map == 0 || (address & 3U) != 0) {
		DBG_ERR(("dma download dma_map: %p address: %08x", IoAdapter->dma_map, address))
		return (-1);
	}

	if (address < 1024) {
		dword to_copy = MIN(length, 1024);
		byte* mem = (byte*)IoAdapter->Address;

		if (use_burst != 0) {
			memcpy (&mem[address], data, to_copy);
		} else {
			volatile byte* dst = &mem[address];
			const byte*    src = data;
			dword len          = to_copy;

			while (len--) {
				*dst++ = *src++;
			}
		}
		address += to_copy;
		data    += to_copy;
		length  -= to_copy;
	}

	if (length != 0) {
		byte *plx = &(((byte*)IoAdapter->reset)[IoAdapter->plx_offset]);
		volatile byte  *dmacsr0  = (byte*)&plx[0xa8];
		volatile dword *dmamode0 = (dword*)&plx[0x80];
		volatile dword *dmadpr0  = (dword*)&plx[0x90];
		byte  dmacsr0_ref  = *dmacsr0;
		dword dmamode0_ref = *dmamode0;
		dword dmadpr0_ref  = *dmadpr0;
		int dma_handles[16], i;
		PISDN_ADAPTER dma_adapters[sizeof(dma_handles)/sizeof(dma_handles[0])];
		byte save_mem[32*(sizeof(dma_handles)/sizeof(dma_handles[0])+4)];
		int dma_descriptors;
		dword max_transfer_length, descriptor_length = 4*1024;

		if (use_burst != 0) {
			memcpy (save_mem, IoAdapter->Address, sizeof(save_mem));
		} else {
			byte* dst          = save_mem;
			volatile byte* src = IoAdapter->Address;
			dword len          = sizeof(save_mem);

			while (len--) {
				*dst++ = *src++;
			}
		}

		for (i = 0; i < sizeof(dma_handles)/sizeof(dma_handles[0]); i++) {
			dma_handles[i] = -1;
		}
    i = 0;
    dma_descriptors = 0;
		adapter_index = 0;

    do {
			int dma_handle_index;

			for (dma_handle_index = 0; i < sizeof(dma_handles)/sizeof(dma_handles[0]); i++) {
				if (alloc_descriptors != 0) {
					if ((dma_handles[i] = diva_alloc_dma_map_entry (currentAdapter->dma_map)) >= 0) {
						dma_adapters[i] = currentAdapter;
						dma_descriptors++;
					} else {
						break;
					}
				} else {
					unsigned long dma_magic;
					void* local_addr = 0;

					diva_get_dma_map_entry (currentAdapter->dma_map, dma_handle_index, &local_addr, &dma_magic);
					if (local_addr != 0) {
						dma_handles[i] = dma_handle_index;
						dma_adapters[i] = currentAdapter;
						dma_descriptors++;
						dma_handle_index++;
					} else {
						break;
					}
				}
			}

			adapter_index++;

		} while (alloc_descriptors == 0 && dma_descriptors < 8 &&
						 IoAdapter->QuadroList != 0 &&
						 adapter_index < IoAdapter->tasks &&
						 (currentAdapter = IoAdapter->QuadroList->QuadroAdapter[adapter_index]) != 0 &&
						 IoAdapter->QuadroList->QuadroAdapter[adapter_index]->dma_map != 0);

		if (dma_descriptors == 0)
			return (-1);

		max_transfer_length = dma_descriptors * descriptor_length;

		*dmacsr0  &= ~1U; /* Stop channel */
		if (use_burst != 0) {
			*dmamode0 |= (1U <<  8); /* Local burst enable */
		} else {
			*dmamode0 &= ~(1U <<  8); /* Local burst enable */
		}
		/* *dmamode0 |= (1U <<  6); */
		*dmamode0 |= (1U <<  9); /* Scatter gatter mode */
		if (use_burst != 0) {
			*dmamode0 |= (1U <<  7); /* Contineous burst mode */
		} else {
			*dmamode0 &= ~(1U <<  7); /* Contineous burst mode */
		}
		*dmamode0 |= (1U << 18); /* Long descriptor format */

		do {
			dword to_copy = MIN(length, max_transfer_length);
			dword addr    = 32;
			byte* mem = (byte*)IoAdapter->Address;
			int entry = 0;
			int used_descriptors = to_copy/descriptor_length + ((to_copy%descriptor_length) != 0);
			void* local_addr;
			unsigned long dma_magic;
			unsigned long dma_magic_hi;

			for (i = 0; i < (used_descriptors-1); i++) {
				diva_get_dma_map_entry    (dma_adapters[entry]->dma_map, dma_handles[entry], &local_addr, &dma_magic);
				diva_get_dma_map_entry_hi (dma_adapters[entry]->dma_map, dma_handles[entry], &dma_magic_hi);
				memcpy (local_addr, data, descriptor_length);

				*(volatile dword*)&mem[addr] = (dword)dma_magic;  addr += 4; /* PCIe address */
				*(volatile dword*)&mem[addr] = address;           addr += 4; /* sdram address */
				*(volatile dword*)&mem[addr] = descriptor_length; addr += 4; /* Transfer length */
				*(volatile dword*)&mem[addr] = (addr + 4+16); addr += 4;
				*(volatile dword*)&mem[addr] = (dword)dma_magic_hi; addr += 4;
				addr += 12; /* Align to 32 byte boundary */

				data    += descriptor_length;
				address += descriptor_length;
				to_copy -= descriptor_length;
				length  -= descriptor_length;
				entry++;
			}

			diva_get_dma_map_entry    (dma_adapters[entry]->dma_map, dma_handles[entry], &local_addr, &dma_magic);
			diva_get_dma_map_entry_hi (dma_adapters[entry]->dma_map, dma_handles[entry], &dma_magic_hi);
			memcpy (local_addr, data, to_copy);

			*(volatile dword*)&mem[addr] = (dword)dma_magic;  addr += 4; /* PCIe address */
			*(volatile dword*)&mem[addr] = address;           addr += 4; /* sdram address */
			*(volatile dword*)&mem[addr] = (to_copy + (sizeof(dword)-1)) & ~(sizeof(dword)-1); addr += 4;
			*(volatile dword*)&mem[addr] = (1L << 1);        addr += 4;
			*(volatile dword*)&mem[addr] = (dword)dma_magic_hi; addr += 4;

			data    += to_copy;
			address += to_copy;
			length  -= to_copy;

			*dmacsr0 |= 0x01; /* Start channel */
			*dmadpr0 = 32;
			*dmacsr0 |= 0x03; /* Start transfer */
			i = 10000;
			do {
				diva_os_sleep (0xffffffff);
			} while ((*dmacsr0 & (1U << 4)) == 0 && i-- > 0);
			*dmacsr0 &= ~0x01; /* Stop channel */
			if (i <= 0) {
				DBG_ERR(("dma download error, use memory mapped download"))
				ret = -1;
			}
		} while (length != 0);

		if (alloc_descriptors != 0) {
			for (i = 0; i < sizeof(dma_handles)/sizeof(dma_handles[0]); i++) {
				if (dma_handles[i] >= 0) {
					diva_free_dma_map_entry (dma_adapters[i]->dma_map, dma_handles[i]);
				}
			}
		}
		if (use_burst != 0) {
			memcpy (IoAdapter->Address, save_mem, sizeof(save_mem));
		} else {
			volatile byte* dst = IoAdapter->Address;
			const byte* src    = save_mem;
			dword len          = sizeof(save_mem);

			while (len--) {
				*dst++ = *src++;
			}
		}
		*dmadpr0  = dmadpr0_ref;
		*dmacsr0  = dmacsr0_ref;
		*dmamode0 = dmamode0_ref;
	}

#ifdef DIVA_VERIFY_DMA_DOWNLOAD
	if (ret == 0) {
		if (memcmp (ref_data, IoAdapter->Address + ref_address, ref_length) != 0) {
			dword i;
			dword last_ok = 0, last_error = 0;

			for (i = 0; i < ref_length; i++) {
				if (ref_data[i] != IoAdapter->Address[ref_address+i]) {
					if (last_ok != 0) {
						DBG_ERR(("Last ok: %08x", last_ok))
					}
					last_ok = 0;
					last_error = i;
						DBG_ERR(("Error at: %08x %02x != %02x",
											ref_address+i, ref_data[i], IoAdapter->Address[ref_address+i]))
				} else {
					if (last_error != 0) {
						DBG_ERR(("Last error: %08x", last_error))
					}
					last_error = 0;
					last_ok = i;
				}
			}

			ret = -1;
		}
	}
#endif

	return (ret);
}

static void diva_oob_put (PISDN_ADAPTER IoAdapter, ENTITY* e) {
  diva_os_spin_lock_magic_t irql;
  diva_oob_entity_queue_t* pQ = &IoAdapter->oob_q;

  diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "xon");

  if (pQ->count < sizeof(pQ->Id)/sizeof(pQ->Id[0])) {
    IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ*)e ;
    pQ->Id[pQ->write] = syncReq->diva_xdi_xon.info.Id;
    pQ->Ch[pQ->write] = syncReq->diva_xdi_xon.info.Ch;
    pQ->write++;
    if (pQ->write >= sizeof(pQ->Id)/sizeof(pQ->Id[0])) {
      pQ->write = 0;
    }
    pQ->count++;
  }

  diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "xon");
} 

static void diva_process_streaming_mapping (PISDN_ADAPTER IoAdapter, IDI_SYNC_REQ *syncReq) {
  diva_xdi_process_streaming_mapping_t* pI = &syncReq->diva_xdi_streaming_mapping_req.info;
	diva_os_spin_lock_magic_t irql;

  switch (pI->request) {
    case IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_ALLOC_COMMAND: {
			int min_free_entries;

      diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
			min_free_entries = (IoAdapter->fragment_map != 0) ? 10 : 30;
			if (diva_nr_free_dma_entries ((struct _diva_dma_map_entry*)IoAdapter->dma_map) > min_free_entries) {
				pI->dma_handle = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map);
				if (pI->dma_handle >= 0) {
					unsigned long dma_magic, dma_magic_hi;
					void* local_addr;
					diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map, pI->dma_handle, &local_addr, &dma_magic);
					diva_get_dma_map_entry_hi (IoAdapter->dma_map, pI->dma_handle, &dma_magic_hi);
					pI->addr    = local_addr;
					pI->dma_lo  = (dword)dma_magic;
					pI->dma_hi  = (dword)dma_magic_hi;
					pI->request = IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK;
				}
			}
      diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
		} break;

    case IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_FREE_COMMAND:
			diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
			if (pI->dma_handle >= 0) {
				diva_free_dma_map_entry((struct _diva_dma_map_entry*)IoAdapter->dma_map, pI->dma_handle);
				pI->addr    = 0;
				pI->dma_lo  = 0;
				pI->dma_hi  = 0;
				pI->request = IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK;
			}
			diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
      break;

    case IDI_SYNC_REQ_PROCESS_STREAMING_SYSTEM_MAP_COMMAND:
			pI->dma_lo = IoAdapter->sdram_bar;
			pI->dma_hi = 0;
			pI->addr   = IoAdapter->ram - IoAdapter->shared_ram_offset;
			pI->request = IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK;
      break;

		case IDI_SYNC_REQ_PROCESS_STREAMING_SYSTEM_INSTALL_CALLBACK_COMMAND:
			diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
			IoAdapter->shedule_stream_proc = pI->shedule_stream_proc;
			IoAdapter->shedule_stream_proc_context = pI->shedule_stream_proc_context;
			diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "stream_dma_op");
			pI->shedule_stream_proc         = 0;
			pI->shedule_stream_proc_context = 0;
			break;
  }
}

/*
  Dummy functions for L series
	*/
byte no_mem_in (ADAPTER *a, void *addr) { return (0); }
word no_mem_inw (ADAPTER *a, void *addr) { return (0); }
void no_mem_in_dw (ADAPTER *a, void *addr, dword* data, int dwords) { WRITE_DWORD(data, 0); }
void no_mem_in_buffer (ADAPTER *a, void *addr, void *buffer, word length) { memset(buffer, 0x00, length); }
void no_mem_look_ahead (ADAPTER *a, PBUFFER *RBuffer, ENTITY *e) { PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io; IoAdapter->RBuffer.length = 0; e->RBuffer = (DBUFFER *)&IoAdapter->RBuffer; }
void no_mem_out (ADAPTER *a, void *addr, byte data) { }
void no_mem_outw (ADAPTER *a, void *addr, word data) { }
void no_mem_out_dw (ADAPTER *a, void *addr, const dword* data, int dwords) { }
void no_mem_out_buffer (ADAPTER *a, void *addr, void *buffer, word length) { }
void no_mem_inc (ADAPTER *a, void *addr) { }
void no_pr_out(ADAPTER * a) { }
dword no_ram_offset (ADAPTER* a) { return (0); }
byte no_dpc(ADAPTER * a) { return FALSE; }
byte no_test_int(ADAPTER * a) { return (0); }
void no_clear_int(ADAPTER * a) { }
void no_cpu_trapped (PISDN_ADAPTER IoAdapter) { }
int no_load (PISDN_ADAPTER IoAdapter) { return (TRUE); }


