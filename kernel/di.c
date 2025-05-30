
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
#include "pc.h"
#include "pr_pc.h"
#include "di_defs.h"
#include "di.h"
#if !defined USE_EXTENDED_DEBUGS
  #include "dimaint.h"
#else
  #undef dprintf
  #define dprintf
#endif
#include "io.h"
#define PR_RAM  ((struct pr_ram *)0)
#define RAM ((struct dual *)0)
/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/
void pr_out(ADAPTER * a);
byte pr_dpc(ADAPTER * a);
void scom_out(ADAPTER * a);
byte scom_dpc(ADAPTER * a);
static byte pr_ready(ADAPTER * a);
static byte scom_ready(ADAPTER * a);
static byte isdn_rc(ADAPTER *, byte, byte, byte, word, dword, dword);
static byte isdn_ind(ADAPTER *, byte, byte, byte, PBUFFER *, byte, word);
/* -----------------------------------------------------------------
    Functions used for the extended XDI Debug
    macros
    global convergence counter (used by all adapters)
    Look by the implementation part of the functions
    about the parameters.
    If you change the dubugging parameters, then you should update
    the aididbg.doc in the IDI doc's.
   ----------------------------------------------------------------- */
#if defined(XDI_USE_XLOG)
#define XDI_A_NR(_x_) ((byte)(((ISDN_ADAPTER *)(_x_->io))->ANum))
#else
#define XDI_A_NR(_x_) ((byte)0)
#endif
byte xdi_xlog_sec = 0;
void xdi_xlog (byte *msg, word code, int length);
/*------------------------------------------------------------------*/
/* output function                                                  */
/*------------------------------------------------------------------*/
void pr_out(ADAPTER * a)
{
  byte e_no;
  ENTITY  * this = 0;
  BUFFERS  *X;
  word length;
  word i;
  word clength;
  REQ * ReqOut;
  byte more;
  byte ReadyCount;
  byte ReqCount;
  byte Id;
  dtrc(dprintf("pr_out"));
        /* while a request is pending ...                           */
  e_no = look_req(a);
  if(!e_no)
  {
    dtrc(dprintf("no_req"));
    return;
  }
  ReadyCount = pr_ready(a);
  if(!ReadyCount)
  {
    dtrc(dprintf("not_ready"));
    return;
  }
  ReqCount = 0;
  while(e_no && ReadyCount) {
    next_req(a);
    this = entity_ptr(a, e_no);
#ifdef USE_EXTENDED_DEBUGS
    if ( !this )
    {
      ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
      DBG_FTL(("XDI: [%02x] !A%d ==> NULL entity ptr - try to ignore",
               xdi_xlog_sec++, (int)io->ANum))
      e_no = look_req(a) ;
      ReadyCount-- ;
      continue ;
    }
    {
      ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
      DBG_TRC((">A%d Id=0x%x Req=0x%x", io->ANum, this->Id, this->Req))
    }
#else
    dbug(dprintf("out:Req=%x,Id=%x,Ch=%x",this->Req,this->Id,this->ReqCh));
#endif
        /* get address of next available request buffer             */
    ReqOut = (REQ *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextReq)];
    if (!(a->tx_stream[this->Id]   &&
        this->Req == N_DATA)) {
        /* now copy the data from the current data buffer into the  */
        /* adapters request buffer                                  */
      length = 0;
      i = this->XCurrent;
      X = PTR_X(a,this);
      while(i<this->XNum && length<270) {
        clength = MIN((word)(270-length),X[i].PLength-this->XOffset);
        a->ram_out_buffer(a,
                          &ReqOut->XBuffer.P[length],
                          PTR_P(a,this,&X[i].P[this->XOffset]),
                          clength);
        length +=clength;
        this->XOffset +=clength;
        if(this->XOffset==X[i].PLength) {
          this->XCurrent = (byte)++i;
          this->XOffset = 0;
        }
      }
    } else { /* Use CMA extension in order to transfer data to the card */
      dword data_length = 0;
      i = this->XCurrent;
      X = PTR_X(a,this);
      while (i < this->XNum) {
        a->ram_out_buffer (a, ULongToPtr(a->tx_stream[this->Id] + sizeof(dword) + data_length),
                           PTR_P(a,this,&X[i].P[0]), X[i].PLength);
        data_length += X[i].PLength;
        this->XCurrent = (byte)++i;
      }
      a->ram_out_dw (a, ULongToPtr(a->tx_stream[this->Id]), &data_length, 1);
      length = 0;
    }
    a->ram_outw(a, &ReqOut->XBuffer.length, length);
    a->ram_out(a, &ReqOut->ReqId, this->Id);
    a->ram_out(a, &ReqOut->ReqCh, this->ReqCh);
        /* if its a specific request (no ASSIGN) ...                */
    if(this->Id &0x1f) {
        /* if buffers are left in the list of data buffers do       */
        /* do chaining (LL_MDATA, N_MDATA)                          */
      this->More++;
      if(i<this->XNum && this->MInd) {
        xdi_xlog_request (XDI_A_NR(a), this->Id, this->ReqCh, this->MInd,
                          a->IdTypeTable[this->No]);
        a->ram_out(a, &ReqOut->Req, this->MInd);
        more = TRUE;
      }
      else {
        xdi_xlog_request (XDI_A_NR(a), this->Id, this->ReqCh, this->Req,
                          a->IdTypeTable[this->No]);
        this->More |=XMOREF;
        a->ram_out(a, &ReqOut->Req, this->Req);
        more = FALSE;
        if (a->FlowControlIdTable[this->ReqCh] == this->Id)
          a->FlowControlSkipTable[this->ReqCh] = TRUE;
        /*
           Note that remove request was sent to the card
           */
        if (this->Req == REMOVE) {
          a->misc_flags_table[e_no] |= DIVA_MISC_FLAGS_REMOVE_PENDING;
        }
      }
        /* if we did chaining, this entity is put back into the     */
        /* request queue                                            */
      if(more) {
        this->More |= XMOREP;
        req_queue(a,this->No);
      } else {
        this->More &= ~XMOREP;
      }
    }
        /* else it's a ASSIGN                                       */
    else {
        /* save the request code used for buffer chaining           */
      this->MInd = 0;
      if (this->Id==BLLC_ID) this->MInd = LL_MDATA;
      if (this->Id==NL_ID   ||
          this->Id==TASK_ID ||
          this->Id==MAN_ID
        ) this->MInd = N_MDATA;
        /* send the ASSIGN                                          */
      a->IdTypeTable[this->No] = this->Id;
      xdi_xlog_request (XDI_A_NR(a),this->Id,this->ReqCh,this->Req, this->Id);
      this->More |=XMOREF;
      this->More &= ~XMOREP;
      a->ram_out(a, &ReqOut->Req, this->Req);
        /* save the reference of the ASSIGN                         */
      assign_queue(a, this->No, a->ram_inw(a, &ReqOut->Reference));
    }
    a->ram_outw(a, &PR_RAM->NextReq, a->ram_inw(a, &ReqOut->next));
    ReadyCount--;
    ReqCount++;
    e_no = look_req(a);
  }
        /* send the filled request buffers to the ISDN adapter      */
  a->ram_out(a, &PR_RAM->ReqInput,
             (byte)(a->ram_in(a, &PR_RAM->ReqInput) + ReqCount));
        /* if it is a 'unreturncoded' UREMOVE request, remove the  */
        /* Id from our table after sending the request             */
  if(this && (this->Req==UREMOVE) && this->Id) {
    Id = this->Id;
    e_no = a->IdTable[Id];
    free_entity(a, e_no);
    for (i = 0; i < 256; i++)
    {
      if (a->FlowControlIdTable[i] == Id)
        a->FlowControlIdTable[i] = 0;
    }
    a->IdTable[Id] = 0;
    this->Id = 0;
  }
}
static byte pr_ready(ADAPTER * a)
{
  byte ReadyCount;
  ReadyCount = (byte)(a->ram_in(a, &PR_RAM->ReqOutput) -
                      a->ram_in(a, &PR_RAM->ReqInput));
  if(!ReadyCount) {
    if(!a->ReadyInt) {
      a->ram_inc(a, &PR_RAM->ReadyInt);
      a->ReadyInt++;
    }
  }
  return ReadyCount;
}
/*------------------------------------------------------------------*/
/* isdn interrupt handler                                           */
/*------------------------------------------------------------------*/
byte pr_dpc(ADAPTER * a)
{
  byte Count;
  RC * RcIn;
  IND * IndIn;
  byte c;
  byte RNRId;
  byte Rc;
  byte Ind;
        /* if return codes are available ...                        */
  if((Count = a->ram_in(a, &PR_RAM->RcOutput)) != 0) {
    dtrc(dprintf("#Rc=%x",Count));
        /* get the buffer address of the first return code          */
    RcIn = (RC *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextRc)];
        /* for all return codes do ...                              */
    while(Count--) {
      if((Rc=a->ram_in(a, &RcIn->Rc)) != 0) {
        dword tmp[2];
        /*
          Get extended information, associated with return code
          */
        a->ram_in_buffer(a,
                         &RcIn->Reserved2[0],
                         (byte*)&tmp[0],
                         8);
        /* call return code handler, if it is not our return code   */
        /* the handler returns 2                                    */
        /* for all return codes we process, we clear the Rc field   */
        isdn_rc(a,
                Rc,
                a->ram_in(a, &RcIn->RcId),
                a->ram_in(a, &RcIn->RcCh),
                a->ram_inw(a, &RcIn->Reference),
                tmp[0],  /* type of extended informtion */
                tmp[1]); /* extended information        */
        a->ram_out(a, &RcIn->Rc, 0);
      }
        /* get buffer address of next return code                   */
      RcIn = (RC *)&PR_RAM->B[a->ram_inw(a, &RcIn->next)];
    }
        /* clear all return codes (no chaining!)                    */
    a->ram_out(a, &PR_RAM->RcOutput ,0);
        /* call output function                                     */
    pr_out(a);
  }
        /* clear RNR flag                                           */
  RNRId = 0;
        /* if indications are available ...                         */
  if((Count = a->ram_in(a, &PR_RAM->IndOutput)) != 0) {
    dtrc(dprintf("#Ind=%x",Count));
        /* get the buffer address of the first indication           */
    IndIn = (IND *)&PR_RAM->B[a->ram_inw(a, &PR_RAM->NextInd)];
        /* for all indications do ...                               */
    while(Count--) {
        /* if the application marks an indication as RNR, all       */
        /* indications from the same Id delivered in this interrupt */
        /* are marked RNR                                           */
      if(RNRId && RNRId==a->ram_in(a, &IndIn->IndId)) {
        a->ram_out(a, &IndIn->Ind, 0);
        a->ram_out(a, &IndIn->RNR, TRUE);
      }
      else {
        Ind = a->ram_in(a, &IndIn->Ind);
        if(Ind) {
          RNRId = 0;
        /* call indication handler, a return value of 2 means chain */
        /* a return value of 1 means RNR                            */
        /* for all indications we process, we clear the Ind field   */
          c = isdn_ind(a,
                       Ind,
                       a->ram_in(a, &IndIn->IndId),
                       a->ram_in(a, &IndIn->IndCh),
                       &IndIn->RBuffer,
                       a->ram_in(a, &IndIn->MInd),
                       a->ram_inw(a, &IndIn->MLength));
          if(c==1) {
            dtrc(dprintf("RNR"));
            a->ram_out(a, &IndIn->Ind, 0);
            RNRId = a->ram_in(a, &IndIn->IndId);
            a->ram_out(a, &IndIn->RNR, TRUE);
          }
        }
      }
        /* get buffer address of next indication                    */
      IndIn = (IND *)&PR_RAM->B[a->ram_inw(a, &IndIn->next)];
    }
    a->ram_out(a, &PR_RAM->IndOutput, 0);
  }
  return FALSE;
}
byte pr_test_int(ADAPTER * a)
{
  return a->ram_in(a,(void *)0x3ffc);
}
void pr_clear_int(ADAPTER * a)
{
  a->ram_out(a,(void *)0x3ffc,0);
}
/*------------------------------------------------------------------*/
/* output function                                                  */
/*------------------------------------------------------------------*/
void scom_out(ADAPTER * a)
{
  byte e_no;
  ENTITY  * this;
  BUFFERS  * X;
  word length;
  word i;
  word clength;
  byte more;
  byte Id;
  dtrc(dprintf("scom_out"));
        /* check if the adapter is ready to accept an request:      */
  e_no = look_req(a);
  if(!e_no)
  {
    dtrc(dprintf("no_req"));
    return;
  }
  if(!scom_ready(a))
  {
    dtrc(dprintf("not_ready"));
    return;
  }
  this = entity_ptr(a,e_no);
  dtrc(dprintf("out:Req=%x,Id=%x,Ch=%x",this->Req,this->Id,this->ReqCh));
  next_req(a);
        /* now copy the data from the current data buffer into the  */
        /* adapters request buffer                                  */
  length = 0;
  i = this->XCurrent;
  X = PTR_X(a, this);
  while(i<this->XNum && length<270) {
    clength = MIN((word)(270-length),X[i].PLength-this->XOffset);
    a->ram_out_buffer(a,
                      &RAM->XBuffer.P[length],
                      PTR_P(a,this,&X[i].P[this->XOffset]),
                      clength);
    length +=clength;
    this->XOffset +=clength;
    if(this->XOffset==X[i].PLength) {
      this->XCurrent = (byte)++i;
      this->XOffset = 0;
    }
  }
  a->ram_outw(a, &RAM->XBuffer.length, length);
  a->ram_out(a, &RAM->ReqId, this->Id);
  a->ram_out(a, &RAM->ReqCh, this->ReqCh);
        /* if its a specific request (no ASSIGN) ...                */
  if(this->Id &0x1f) {
        /* if buffers are left in the list of data buffers do       */
        /* chaining (LL_MDATA, N_MDATA)                             */
    this->More++;
    if(i<this->XNum && this->MInd) {
      a->ram_out(a, &RAM->Req, this->MInd);
      more = TRUE;
    }
    else {
      this->More |=XMOREF;
      a->ram_out(a, &RAM->Req, this->Req);
      more = FALSE;
      if (a->FlowControlIdTable[this->ReqCh] == this->Id)
        a->FlowControlSkipTable[this->ReqCh] = TRUE;
      /*
         Note that remove request was sent to the card
         */
      if (this->Req == REMOVE) {
        a->misc_flags_table[e_no] |= DIVA_MISC_FLAGS_REMOVE_PENDING;
      }
    }
    if(more) {
      this->More |= XMOREP;
      req_queue(a,this->No);
    } else {
      this->More &= ~XMOREP;
    }
  }
        /* else it's a ASSIGN                                       */
  else {
        /* save the request code used for buffer chaining           */
    this->MInd = 0;
    if (this->Id==BLLC_ID) this->MInd = LL_MDATA;
    if (this->Id==NL_ID   ||
        this->Id==TASK_ID ||
        this->Id==MAN_ID
      ) this->MInd = N_MDATA;
        /* send the ASSIGN                                          */
    this->More |=XMOREF;
    this->More &= ~XMOREP;
    a->ram_out(a, &RAM->Req, this->Req);
        /* save the reference of the ASSIGN                         */
    assign_queue(a, this->No, 0);
  }
        /* if it is a 'unreturncoded' UREMOVE request, remove the  */
        /* Id from our table after sending the request             */
  if(this->Req==UREMOVE && this->Id) {
    Id = this->Id;
    e_no = a->IdTable[Id];
    free_entity(a, e_no);
    for (i = 0; i < 256; i++)
    {
      if (a->FlowControlIdTable[i] == Id)
        a->FlowControlIdTable[i] = 0;
    }
    a->IdTable[Id] = 0;
    this->Id = 0;
  }
}
static byte scom_ready(ADAPTER * a)
{
  if(a->ram_in(a, &RAM->Req)) {
    if(!a->ReadyInt) {
      a->ram_inc(a, &RAM->ReadyInt);
      a->ReadyInt++;
    }
    return 0;
  }
  return 1;
}
/*------------------------------------------------------------------*/
/* isdn interrupt handler                                           */
/*------------------------------------------------------------------*/
byte scom_dpc(ADAPTER * a)
{
  byte c;
        /* if a return code is available ...                        */
  if(a->ram_in(a, &RAM->Rc)) {
        /* call return code handler, if it is not our return code   */
        /* the handler returns 2, if it's the return code to an     */
        /* ASSIGN the handler returns 1                             */
    c = isdn_rc(a,
                a->ram_in(a, &RAM->Rc),
                a->ram_in(a, &RAM->RcId),
                a->ram_in(a, &RAM->RcCh),
                0,
                /*
                  Scom Card does not provide extended information
                  */
                0, 0);
    switch(c) {
    case 0:
      a->ram_out(a, &RAM->Rc, 0);
      break;
    case 1:
      a->ram_out(a, &RAM->Req, 0);
      a->ram_out(a, &RAM->Rc, 0);
      break;
    case 2:
      return TRUE;
    }
        /* call output function                                     */
    scom_out(a);
  }
  else {
        /* if an indications is available ...                       */
    if(a->ram_in(a, &RAM->Ind)) {
        /* call indication handler, a return value of 2 means chain */
        /* a return value of 1 means RNR                            */
      c = isdn_ind(a,
                   a->ram_in(a, &RAM->Ind),
                   a->ram_in(a, &RAM->IndId),
                   a->ram_in(a, &RAM->IndCh),
                   &RAM->RBuffer,
                   a->ram_in(a, &RAM->MInd),
                   a->ram_inw(a, &RAM->MLength));
      switch(c) {
      case 0:
        a->ram_out(a, &RAM->Ind, 0);
        break;
      case 1:
        dtrc(dprintf("RNR"));
        a->ram_out(a, &RAM->RNR, TRUE);
        break;
      case 2:
        return TRUE;
      }
    }
  }
  return FALSE;
}
byte scom_test_int(ADAPTER * a)
{
  return a->ram_in(a,(void *)0x3fe);
}
void scom_clear_int(ADAPTER * a)
{
  a->ram_out(a,(void *)0x3fe,0);
}
void quadro_clear_int(ADAPTER * a)
{
  a->ram_out(a,(void *)0x3fe,0);
  a->ram_out(a,(void *)0x401,0);
}
/*------------------------------------------------------------------*/
/* return code handler                                              */
/*------------------------------------------------------------------*/
byte isdn_rc(ADAPTER * a,
             byte Rc,
             byte Id,
             byte Ch,
             word Ref,
             dword extended_info_type,
             dword extended_info)
{
  ENTITY  * this;
  byte e_no;
  word i;
  int cancel_rc;
#ifdef USE_EXTENDED_DEBUGS
  {
    ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
    DBG_TRC(("<A%d Id=0x%x Rc=0x%x", io->ANum, Id, Rc))
  }
#else
  dbug(dprintf("isdn_rc(Rc=%x,Id=%x,Ch=%x)",Rc,Id,Ch));
#endif
        /* check for ready interrupt                                */
  if(Rc==READY_INT) {
    xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 0, 0);
    if(a->ReadyInt) {
      a->ReadyInt--;
      return 0;
    }
    return 2;
  }
        /* if we know this Id ...                                   */
  e_no = a->IdTable[Id];
  if(e_no) {
    int remove_complete = 0;
    byte RNR;

    this = entity_ptr(a,e_no);
    RNR = this->RNR;
    this->RNR = 0;
    xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 0, a->IdTypeTable[this->No]);
    this->RcCh = Ch;
        /* if it is a return code to a REMOVE request, remove the   */
        /* Id from our table                                        */
    if ((a->misc_flags_table[e_no] & DIVA_MISC_FLAGS_REMOVE_PENDING) &&
        (Rc==OK)) {
      if (a->IdTypeTable[e_no] == NL_ID) {
        if (a->RcExtensionSupported &&
            (extended_info_type != DIVA_RC_TYPE_REMOVE_COMPLETE)) {
        dtrc(dprintf("XDI: N-REMOVE, A(%02x) Id:%02x, ignore RC=OK",
                        XDI_A_NR(a),Id));
          this->RNR = RNR;
          return (0);
        }
        if (extended_info_type == DIVA_RC_TYPE_REMOVE_COMPLETE)
          a->RcExtensionSupported = TRUE;
      }
      a->misc_flags_table[e_no] &= ~DIVA_MISC_FLAGS_REMOVE_PENDING;
      a->misc_flags_table[e_no] &= ~DIVA_MISC_FLAGS_NO_RC_CANCELLING;
      free_entity(a, e_no);
      for (i = 0; i < 256; i++)
      {
        if (a->FlowControlIdTable[i] == Id)
          a->FlowControlIdTable[i] = 0;
      }
      a->IdTable[Id] = 0;
      this->Id = 0;
      remove_complete = 1;
      /* ---------------------------------------------------------------
        If we send N_DISC or N_DISK_ACK after we have received OK_FC
        then the card will respond with OK_FC and later with RC==OK.
        If we send N_REMOVE in this state we will receive only RC==OK
        This will create the state in that the XDI is waiting for the
        additional RC and does not delivery the RC to the client. This
        code corrects the counter of outstanding RC's in this case.
      --------------------------------------------------------------- */
      if ((this->More & XMOREC) > 1) {
        this->More &= ~XMOREC;
        this->More |= 1;
        dtrc(dprintf("XDI: correct MORE on REMOVE A(%02x) Id:%02x",
                     XDI_A_NR(a),Id));
      }
    }
    if (Rc==OK_FC) {
      a->FlowControlIdTable[Ch] = Id;
      a->FlowControlSkipTable[Ch] = FALSE;
      this->Rc = Rc;
      this->More &= ~(XBUSY | XMOREC);
      this->complete=0xff;
      xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 1, a->IdTypeTable[this->No]);
      CALLBACK(a, this);
      this->RNR = RNR;
      return 0;
    }
    /*
      New protocol code sends return codes that comes from release
      of flow control condition marked with DIVA_RC_TYPE_OK_FC extended
      information element type.
      If like return code arrives then application is able to process
      all return codes self and XDI should not cances return codes.
      This return code does not decrement XMOREC partial return code
      counter due to fact that it was no request for this return code,
      also XMOREC was not incremented.
      */
    if (extended_info_type == DIVA_RC_TYPE_OK_FC) {
      a->misc_flags_table[e_no] |= DIVA_MISC_FLAGS_NO_RC_CANCELLING;
      this->RNR = DIVA_RC_TYPE_OK_FC;
      this->Rc = Rc;
      this->complete=0xff;
      xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 1, a->IdTypeTable[this->No]);
      DBG_TRC(("XDI OK_FC A(%02x) Id:%02x Ch:%02x Rc:%02x",
      XDI_A_NR(a), Id, Ch, Rc))
      CALLBACK(a, this);
      if (remove_complete == 0)
        this->RNR = RNR;
      return 0;
    }
    cancel_rc = !(a->misc_flags_table[e_no] & DIVA_MISC_FLAGS_NO_RC_CANCELLING);
    if (cancel_rc && (a->FlowControlIdTable[Ch] == Id))
    {
      a->FlowControlIdTable[Ch] = 0;
      if ((Rc != OK) || !a->FlowControlSkipTable[Ch])
      {
        this->Rc = Rc;
        if (Ch == this->ReqCh)
        {
          this->More &=~(XBUSY | XMOREC);
          this->complete=0xff;
        }
        xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 1, a->IdTypeTable[this->No]);
        CALLBACK(a, this);
      }
      if (remove_complete == 0)
        this->RNR = RNR;
      return 0;
    }
    if (this->More &XMOREC)
      this->More--;
        /* call the application callback function                   */
    if (((!cancel_rc) || (this->More & XMOREF)) && !(this->More & (XMOREC | XMOREP))) {
      this->Rc = Rc;
      this->More &=~XBUSY;
      this->complete=0xff;
      xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 1, a->IdTypeTable[this->No]);
      CALLBACK(a, this);
    }
    if (remove_complete == 0)
      this->RNR = RNR;
    return 0;
  }
        /* if it's an ASSIGN return code check if it's a return     */
        /* code to an ASSIGN request from us                        */
  if((Rc &0xf0)==ASSIGN_RC) {
    e_no = get_assign(a, Ref);
    if(e_no) {
      this = entity_ptr(a,e_no);
      this->Id = Id;
      xdi_xlog_rc_event (XDI_A_NR(a), Id, Ch, Rc, 2, a->IdTypeTable[this->No]);
        /* call the application callback function                   */
      this->Rc = Rc;
      this->More &=~XBUSY;
      this->complete=0xff;
      if ((Rc == ASSIGN_OK) && a->ram_offset &&
          (a->IdTypeTable[this->No] == NL_ID) &&
          ((extended_info_type == DIVA_RC_TYPE_RX_DMA) ||
          (extended_info_type == DIVA_RC_TYPE_CMA_PTR)) &&
          extended_info) {
        dword offset = (*(a->ram_offset)) (a);
        dword tmp[2];
        extended_info -= offset;
        a->ram_in_dw(a, (void*)NumToPtr(extended_info), (dword*)&tmp[0], 2);
        a->tx_stream[Id]  = tmp[0];
        a->rx_stream[Id]  = tmp[1];
        if (extended_info_type == DIVA_RC_TYPE_RX_DMA) {
          DBG_TRC(("Id=0x%x RxDMA=%08x:%08x",
                    Id, a->tx_stream[Id], a->rx_stream[Id]))
          a->misc_flags_table[this->No] |= DIVA_MISC_FLAGS_RX_DMA;
        } else {
          DBG_TRC(("Id=0x%x CMA=%08x:%08x",
                    Id, a->tx_stream[Id], a->rx_stream[Id]))
          a->misc_flags_table[this->No] &= ~DIVA_MISC_FLAGS_RX_DMA;
          a->rx_stream[Id] -= offset;
        }
        a->tx_stream[Id] -= offset;
      } else if ((Rc == ASSIGN_OK) && a->ram_offset &&
                 (a->IdTypeTable[this->No] == MAN_ID) &&
                 (extended_info_type == DIVA_RC_TYPE_RX_DMA)) {
        a->tx_stream[Id]  = 0;
        a->rx_stream[Id]  = extended_info;
        a->misc_flags_table[this->No] |= DIVA_MISC_FLAGS_RX_DMA;
        DBG_TRC(("Id=0x%x management RxDMA=%08x:%08x",
                  Id, a->tx_stream[Id], a->rx_stream[Id]))
      } else {
        a->tx_stream[Id] = 0;
        a->rx_stream[Id] = 0;
        a->misc_flags_table[this->No] &= ~DIVA_MISC_FLAGS_RX_DMA;
      }
      CALLBACK(a, this);
      if(Rc==ASSIGN_OK) {
        a->IdTable[Id] = e_no;
      }
      else
      {
        free_entity(a, e_no);
        for (i = 0; i < 256; i++)
        {
          if (a->FlowControlIdTable[i] == Id)
            a->FlowControlIdTable[i] = 0;
        }
        a->IdTable[Id] = 0;
      }
      return 1;
    }
  }
  return 2;
}
/*------------------------------------------------------------------*/
/* indication handler                                               */
/*------------------------------------------------------------------*/
byte isdn_ind(ADAPTER * a,
              byte Ind,
              byte Id,
              byte Ch,
              PBUFFER * RBuffer,
              byte MInd,
              word MLength)
{
  ENTITY  * this;
  word clength;
  word offset;
  BUFFERS  *R;
  byte* cma = 0;
#ifdef USE_EXTENDED_DEBUGS
  {
    ISDN_ADAPTER *io = (ISDN_ADAPTER *)a->io ;
    DBG_TRC(("<A%d Id=0x%x Ind=0x%x", io->ANum, Id, Ind))
  }
#else
  dbug(dprintf("isdn_ind(Ind=%x,Id=%x,Ch=%x)",Ind,Id,Ch));
#endif
  if(a->IdTable[Id]) {
    this = entity_ptr(a,a->IdTable[Id]);
    this->IndCh = Ch;
    xdi_xlog_ind (XDI_A_NR(a), Id, Ch, Ind,
                  0/* rnr_valid */, 0 /* rnr */, a->IdTypeTable[this->No]);
        /* if the Receive More flag is not yet set, this is the     */
        /* first buffer of the packet                               */
    if(this->RCurrent==0xff) {
        /* check for receive buffer chaining                        */
      if(Ind==this->MInd) {
        this->complete = 0;
        this->Ind = MInd;
      }
      else {
        this->complete = 1;
        this->Ind = Ind;
      }
        /* call the application callback function for the receive   */
        /* look ahead                                               */
      this->RLength = MLength;
      if (a->rx_stream[this->Id] != 0 || (a->misc_flags_table[this->No] & DIVA_MISC_FLAGS_RX_DMA) != 0) {
        PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io ;
        if (a->misc_flags_table[this->No] & DIVA_MISC_FLAGS_RX_DMA) {
          unsigned long d;
          diva_get_dma_map_entry (\
                   (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                   (int)a->rx_stream[this->Id], (void**)&cma, &d);
          this->RLength = MLength = (word)*(dword*)cma;
          cma += 4;
        } else {
          dword data_length;
          a->ram_in_dw (a, ULongToPtr(a->rx_stream[this->Id]), &data_length, 1);
          this->RLength = MLength = (word)data_length;
          cma = a->cma_va (a, ULongToPtr(a->rx_stream[this->Id]+sizeof(dword)));
        }
        IoAdapter->RBuffer.length = MIN(MLength, 270);
        if (IoAdapter->RBuffer.length != MLength) {
          this->complete = 0;
        } else {
          this->complete = 1;
        }
        memcpy (IoAdapter->RBuffer.P, cma, IoAdapter->RBuffer.length) ;
        this->RBuffer = (DBUFFER *)&IoAdapter->RBuffer ;
      }
      if (!cma) {
        a->ram_look_ahead(a, RBuffer, this);
      }
      this->RNum = 0;
      CALLBACK(a, this);
        /* map entity ptr, selector could be re-mapped by call to   */
        /* IDI from within callback                                 */
      this = entity_ptr(a,a->IdTable[Id]);
      xdi_xlog_ind (XDI_A_NR(a), Id, Ch, Ind,
          1/* rnr_valid */, this->RNR/* rnr */, a->IdTypeTable[this->No]);
        /* check for RNR                                            */
      if(this->RNR==1) {
        this->RNR = 0;
        return 1;
      }
        /* if no buffers are provided by the application, the       */
        /* application want to copy the data itself including       */
        /* N_MDATA/LL_MDATA chaining                                */
      if(!this->RNR && !this->RNum) {
        xdi_xlog_ind (XDI_A_NR(a), Id, Ch, Ind,
            2/* rnr_valid */, 0/* rnr */, a->IdTypeTable[this->No]);
        return 0;
      }
        /* if there is no RNR, set the More flag                    */
      this->RCurrent = 0;
      this->ROffset = 0;
    }
    if(this->RNR==2) {
      if(Ind!=this->MInd) {
        this->RCurrent = 0xff;
        this->RNR = 0;
      }
      return 0;
    }
        /* if we have received buffers from the application, copy   */
        /* the data into these buffers                              */
    offset = 0;
    R = PTR_R(a,this);
    do {
      if(this->ROffset==R[this->RCurrent].PLength) {
        this->ROffset = 0;
        this->RCurrent++;
      }
      if (this->RCurrent >= this->RNum) break;  // avoid segmentation faults
      
      if (cma) {
        clength = MIN(MLength-offset, R[this->RCurrent].PLength-this->ROffset);
      } else {
        clength = MIN(a->ram_inw(a, &RBuffer->length)-offset,
                      R[this->RCurrent].PLength-this->ROffset);
      }
      if(R[this->RCurrent].P) {
        if (cma) {
          memcpy (PTR_P(a,this,&R[this->RCurrent].P[this->ROffset]),
                  &cma[offset],
                  clength);
        } else {
          a->ram_in_buffer(a,
                           &RBuffer->P[offset],
                           PTR_P(a,this,&R[this->RCurrent].P[this->ROffset]),
                           clength);
        }
      }
      offset +=clength;
      this->ROffset +=clength;
    } while (offset < ((word)(cma ? MLength : a->ram_inw(a, &RBuffer->length))));
        /* if it's the last buffer of the packet, call the          */
        /* application callback function for the receive complete   */
        /* call                                                     */
    if(Ind!=this->MInd) {
      R[this->RCurrent].PLength = this->ROffset;
      if(this->ROffset) this->RCurrent++;
      this->RNum = this->RCurrent;
      this->RCurrent = 0xff;
      this->Ind = Ind;
      this->complete = 2;
      xdi_xlog_ind (XDI_A_NR(a), Id, Ch, Ind,
          3/* rnr_valid */, 0/* rnr */, a->IdTypeTable[this->No]);
      CALLBACK(a, this);
    }
    return 0;
  }
  return 2;
}
/* -----------------------------------------------------------
   This function works in the same way as xlog on the
   active board
   ----------------------------------------------------------- */
void xdi_xlog (byte *msg, word code, int length) {
#if defined(XDI_USE_XLOG)
#if MODULDRIVERDEBUG
  xdi_dbg_xlogH (&MYDRIVERDEBUGHANDLE, "\x00\x02", msg, code, length);
#else
  xdi_dbg_xlog ("\x00\x02", msg, code, length);
#endif
#endif
}
/* -----------------------------------------------------------
    This function writes the information about the Return Code
    processing in the trace buffer. Trace ID is 221.
    INPUT:
        Adapter - system unicue adapter number (0 ... 255)
        Id      - Id of the entity that had sent this return code
        Ch      - Channel of the entity that had sent this return code
        Rc      - return code value
        cb:       (0...2)
                  switch (cb) {
                   case 0: printf ("DELIVERY"); break;
                   case 1: printf ("CALLBACK"); break;
                   case 2: printf ("ASSIGN"); break;
                  }
                  DELIVERY - have entered isdn_rc with this RC
                  CALLBACK - about to make callback to the application
                             for this RC
                  ASSIGN   - about to make callback for RC that is result
                             of ASSIGN request. It is no DELIVERY message
                             before of this message
        type   - the Id that was sent by the ASSIGN of this entity.
                 This should be global Id like NL_ID, DSIG_ID, MAN_ID.
                 An unknown Id will cause "?-" in the front of the request.
                 In this case the log.c is to be extended.
   ----------------------------------------------------------- */
void xdi_xlog_rc_event (byte Adapter,
                               byte Id, byte Ch, byte Rc, byte cb, byte type) {
#if defined(XDI_USE_XLOG)
  word LogInfo[4];
  LogInfo[0] = (word)Adapter | (word)(xdi_xlog_sec++ << 8);
  LogInfo[1] = (word)Id | (word)(Ch << 8);
  LogInfo[2] = (word)Rc | (word)(type << 8);
  LogInfo[3] = cb;
  xdi_xlog ((byte*)&LogInfo[0], 221, sizeof(LogInfo));
#endif
}
/* ------------------------------------------------------------------------
    This function writes the information about the request processing
    in the trace buffer. Trace ID is 220.
    INPUT:
        Adapter - system unicue adapter number (0 ... 255)
        Id      - Id of the entity that had sent this request
        Ch      - Channel of the entity that had sent this request
        Req     - Code of the request
        type    - the Id that was sent by the ASSIGN of this entity.
                  This should be global Id like NL_ID, DSIG_ID, MAN_ID.
                  An unknown Id will cause "?-" in the front of the request.
                  In this case the log.c is to be extended.
   ------------------------------------------------------------------------ */
void xdi_xlog_request (byte Adapter, byte Id,
                              byte Ch, byte Req, byte type) {
#if defined(XDI_USE_XLOG)
  word LogInfo[3];
  LogInfo[0] = (word)Adapter | (word)(xdi_xlog_sec++ << 8);
  LogInfo[1] = (word)Id | (word)(Ch << 8);
  LogInfo[2] = (word)Req | (word)(type << 8);
  xdi_xlog ((byte*)&LogInfo[0], 220, sizeof(LogInfo));
#endif
}
/* ------------------------------------------------------------------------
    This function writes the information about the indication processing
    in the trace buffer. Trace ID is 222.
    INPUT:
        Adapter - system unicue adapter number (0 ... 255)
        Id      - Id of the entity that had sent this indication
        Ch      - Channel of the entity that had sent this indication
        Ind     - Code of the indication
        rnr_valid: (0 .. 3) supported
          switch (rnr_valid) {
            case 0: printf ("DELIVERY"); break;
            case 1: printf ("RNR=%d", rnr);
            case 2: printf ("RNum=0");
            case 3: printf ("COMPLETE");
          }
          DELIVERY - indication entered isdn_rc function
          RNR=...  - application had returned RNR=... after the
                     look ahead callback
          RNum=0   - aplication had not returned any buffer to copy
                     this indication and will copy it self
          COMPLETE - XDI had copied the data to the buffers provided
                     bu the application and is about to issue the
                     final callback
        rnr:  Look case 1 of the rnr_valid
        type: the Id that was sent by the ASSIGN of this entity. This should
              be global Id like NL_ID, DSIG_ID, MAN_ID. An unknown Id will
              cause "?-" in the front of the request. In this case the
              log.c is to be extended.
   ------------------------------------------------------------------------ */
void xdi_xlog_ind (byte Adapter,
                          byte Id,
                          byte Ch,
                          byte Ind,
                          byte rnr_valid,
                          byte rnr,
                          byte type) {
#if defined(XDI_USE_XLOG)
  word LogInfo[4];
  LogInfo[0] = (word)Adapter | (word)(xdi_xlog_sec++ << 8);
  LogInfo[1] = (word)Id | (word)(Ch << 8);
  LogInfo[2] = (word)Ind | (word)(type << 8);
  LogInfo[3] = (word)rnr | (word)(rnr_valid << 8);
  xdi_xlog ((byte*)&LogInfo[0], 222, sizeof(LogInfo));
#endif
}
