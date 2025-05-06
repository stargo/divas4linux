
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
#include "io.h"
#include "mi_pc.h"
#include "pc_maint.h"
#include "diva_dma_fragment.h"

#define  DIVA_SYS_MSG_ID             0xff

#define  DIVA_SYS_MSG_ACK_REQ_ENTRY  0
#define  DIVA_SYS_MSG_ACK_IND_ENTRY  1
#define  DIVA_SYS_MSG_ACK_BUF_ENTRY  2
#define  DIVA_SYS_MSG_ACK_ID_ENTRY   3
#define  DIVA_SYS_MSG_RNR_IND        4
#define  DIVA_SYS_MSG_SYSTEM_START   5
#define  DIVA_SYS_MSG_TIMER_ON       6
#define  DIVA_SYS_MSG_TIMER_OFF      7
#define  DIVA_SYS_MSG_STOP_AND_INTERRUPT 8
#define  DIVA_SYS_MSG_READ_XLOG          9
#define  DIVA_SYS_MSG_CPU_STOP           10
#define  DIVA_SYS_MSG_SYSTEM_WARNING     11

static void cancel_rnr_on_remove(ADAPTER* a, ENTITY* e);
static int diva_vidi_oob_get (PISDN_ADAPTER IoAdapter, byte* Id, byte* Ch);

/*
  It initialize the structures in IoAdapter (there is one for buffers and one for Ids)
  Last Entry in the buffer is reserved
 */
void diva_vidi_init_adapter_handle_buffer (diva_vidi_adapter_handle_buffer_t* buf, void* data, int length) {
  buf->base          = (word*)data;
  buf->max_entries   = length/sizeof(word) - 1;
  buf->read_ptr      = buf->base;
  buf->write_ptr     = buf->base;

  buf->saved_entries = 0;
  buf->free_entries  = buf->max_entries;
}

/*
  It inserts data into the first free position and updates index and variables
  Returns 0 on success and -1 on error
  */
int diva_vidi_adapter_handle_buffer_put  (diva_vidi_adapter_handle_buffer_t* buf, word handle) {
  if (buf->free_entries > 0) {
    *buf->write_ptr++ = handle;
    if (buf->write_ptr >= &buf->base[buf->max_entries])
      buf->write_ptr = buf->base;

    buf->saved_entries++;
    buf->free_entries--;

		return (0);
  }

  return (-1);
}

/*
  Returns handle value on success
  Returns 0xffff on error
  */
word diva_vidi_adapter_handle_buffer_get (diva_vidi_adapter_handle_buffer_t* buf) {
  word handle;

  if (buf->saved_entries <= 0)
    return (0xffff);

  handle = *buf->read_ptr++;

  if (buf->read_ptr >= &buf->base[buf->max_entries])
    buf->read_ptr = buf->base;

  buf->saved_entries--; 
  buf->free_entries++; 

  return(handle);
}

/*
 It returns the length of the request
 */
static dword get_req_data (const ENTITY* e) {
  byte i;
  dword sum;

  for (i = 0, sum = 0; e->X && i < e->XNum; i++) {
    sum += e->X[i].PLength;
  }

  return (sum);
}

/*
  Copy the request into the buffer pointed by dst
  The buffer will depend on the size of the request
 */
static void copy_req_data (const ENTITY* e, volatile byte* dst) {
  byte i;

  for (i = 0; e->X && i < e->XNum; i++) {
    memcpy ((void*)dst, &e->X[i].P[0], e->X[i].PLength);
    dst += e->X[i].PLength;
  }
}

static inline void update_req_ptr (PISDN_ADAPTER IoAdapter) {
  IoAdapter->host_vidi.current_req_ptr += IoAdapter->host_vidi.dma_segment_length;
  if (IoAdapter->host_vidi.current_req_ptr >= IoAdapter->host_vidi.dma_req_buffer_length) {
    IoAdapter->host_vidi.current_req_ptr = 0;
  }
}

/*
  Used to send request to the adapter
  */
void vidi_host_pr_out (ADAPTER * a) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io; 
  byte e_no = look_req(a);
  ENTITY* e_req = 0;
  dword ReadyCount, data_length;
  volatile byte* req_ptr = 0;
  int req_sent = 0;

  if (IoAdapter->host_vidi.vidi_started == 0) {
    DBG_ERR(("A(%d) vidi not started", IoAdapter->ANum))
    return;
  }
  if (IoAdapter->host_vidi.cpu_stop_message_received != 0)
    return;

  if (IoAdapter->host_vidi.dma_nr_pending_requests >= IoAdapter->host_vidi.dma_max_nr_pending_requests) {
    DBG_TRC(("A(%d) not_ready, all entries occupied", IoAdapter->ANum))
    return;
  }
  ReadyCount = IoAdapter->host_vidi.dma_max_nr_pending_requests - IoAdapter->host_vidi.dma_nr_pending_requests;

  /*
    Stop adapter
    */
  if ((IoAdapter->host_vidi.request_flags & DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT) != 0 && ReadyCount > 0) {
    req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];

    *req_ptr++ = DIVA_SYS_MSG_ID;
    *req_ptr++ = DIVA_SYS_MSG_STOP_AND_INTERRUPT;
    *req_ptr++ = 0;                          /* Unused */
    *req_ptr++ = 0;                          /* data length */
    IoAdapter->host_vidi.rnr_timer = 0;
    ReadyCount--;
    IoAdapter->host_vidi.dma_nr_pending_requests++;
    IoAdapter->host_vidi.local_request_counter++;
    req_sent = 1;
    update_req_ptr (IoAdapter);
    IoAdapter->host_vidi.request_flags &= ~DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT;
  }

  /*
    Send first indication buffer acknowledges
    */
  if (IoAdapter->host_vidi.nr_ind_processed > 1 && ReadyCount != 0) {
    req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];

    *req_ptr++ = DIVA_SYS_MSG_ID;
    *req_ptr++ = DIVA_SYS_MSG_ACK_IND_ENTRY; /* Indication acknowledge (space in the ring buffer) */
    *req_ptr++ = 0;                          /* Unused */
    *req_ptr++ = 4;                          /* data length */
    WRITE_DWORD(req_ptr, IoAdapter->host_vidi.nr_ind_processed);

    IoAdapter->host_vidi.nr_ind_processed = 0;
    ReadyCount--;
    IoAdapter->host_vidi.dma_nr_pending_requests++;
    IoAdapter->host_vidi.local_request_counter++;
    req_sent = 1;
    update_req_ptr (IoAdapter); 
  }

  /*
    Start/Stop RNR timer message
    */
  if (IoAdapter->host_vidi.rnr_timer != 0 && ReadyCount != 0) {
    req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];

    *req_ptr++ = DIVA_SYS_MSG_ID;
    *req_ptr++ = IoAdapter->host_vidi.rnr_timer == 1 ? DIVA_SYS_MSG_TIMER_ON : DIVA_SYS_MSG_TIMER_OFF;
    *req_ptr++ = 0;                          /* Unused */
    *req_ptr++ = 0;                          /* data length */
    IoAdapter->host_vidi.rnr_timer = 0;
    ReadyCount--;
    IoAdapter->host_vidi.dma_nr_pending_requests++;
    IoAdapter->host_vidi.local_request_counter++;
    req_sent = 1;
    update_req_ptr (IoAdapter);
  }

  { /* Ack of the buffers */
    word entry_nr = -1;
    volatile byte* cur_length = 0;
    void* local_addr;
    unsigned long dma_magic, dma_magic_hi;

    while ( (ReadyCount != 0) && 
           ((entry_nr = diva_vidi_adapter_handle_buffer_get(&IoAdapter->host_vidi.pending_ind_buffer_ack)) != 0xffff) ) {
      if (cur_length == 0) {
        req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];
        *req_ptr++ = DIVA_SYS_MSG_ID;
        *req_ptr++ = DIVA_SYS_MSG_ACK_BUF_ENTRY; 
        *req_ptr++ = 0;                          /* Unused */
        cur_length = req_ptr++;                  /* cur_length is a pointer to the length byte */
        *cur_length = 0;
        ReadyCount--;
        IoAdapter->host_vidi.dma_nr_pending_requests++;
        IoAdapter->host_vidi.local_request_counter++;
        req_sent = 1;
        update_req_ptr (IoAdapter); 
      }
      diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                              entry_nr, &local_addr, &dma_magic);
      diva_get_dma_map_entry_hi ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                 entry_nr, &dma_magic_hi);
      WRITE_WORD(req_ptr, ((word)entry_nr));
      req_ptr += 2;
      WRITE_DWORD(req_ptr, ((dword)dma_magic));
      req_ptr += 4;
      WRITE_DWORD(req_ptr, ((dword)dma_magic_hi));
      req_ptr += 4;
      cur_length[0] += 10;
      if (cur_length[0]+10 >= (byte)IoAdapter->host_vidi.dma_segment_req_data_length)
        cur_length = 0;  /* We need another req buffer */
    }
  }

  { /* Ack of Ids */
    volatile byte* cur_length = 0;
    word Id;

    while ( (ReadyCount != 0) &&
           ((Id = diva_vidi_adapter_handle_buffer_get(&IoAdapter->host_vidi.pending_ind_ack)) != 0xffff) ) {
      if (cur_length == 0) {
        req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];
        *req_ptr++ = DIVA_SYS_MSG_ID;
        *req_ptr++ = DIVA_SYS_MSG_ACK_ID_ENTRY; /* Indication acknowledge (vidi) */
        *req_ptr++ = 0;    /* Unused */
        cur_length = req_ptr++;
        *cur_length = 0;
        ReadyCount--;
        IoAdapter->host_vidi.dma_nr_pending_requests++;
        IoAdapter->host_vidi.local_request_counter++;
        req_sent = 1;
        update_req_ptr (IoAdapter); 
      }
      *req_ptr++ = (byte)Id;
      cur_length[0]++;
      if (cur_length[0]+1 >= (byte)IoAdapter->host_vidi.dma_segment_req_data_length)
        cur_length = 0;
    }
  }


  if ((IoAdapter->host_vidi.request_flags & DIVA_VIDI_REQUEST_FLAG_READ_XLOG) != 0 && ReadyCount != 0) {
    req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];

    *req_ptr++ = DIVA_SYS_MSG_ID;
    *req_ptr++ = DIVA_SYS_MSG_READ_XLOG;
    *req_ptr++ = 0;                          /* Unused */
    *req_ptr++ = 0;                          /* data length */
    IoAdapter->host_vidi.rnr_timer = 0;
    ReadyCount--;
    IoAdapter->host_vidi.dma_nr_pending_requests++;
    IoAdapter->host_vidi.local_request_counter++;
    req_sent = 1;
    update_req_ptr (IoAdapter);
    IoAdapter->host_vidi.request_flags &= ~DIVA_VIDI_REQUEST_FLAG_READ_XLOG;
  }

  while (e_no != 0 && ReadyCount != 0) {
    if ((e_req = entity_ptr(a, e_no)) == 0) {
      DBG_FTL(("A(%d) req e_no:%d not found", IoAdapter->ANum, e_no))
      next_req(a);
      continue;
    }
    req_ptr = &IoAdapter->host_vidi.req_buffer_base_addr[IoAdapter->host_vidi.current_req_ptr];

    if ((data_length = get_req_data (e_req)) <= IoAdapter->host_vidi.dma_segment_req_data_length) {
     /*
       All data passed as part of the request
       */
      *req_ptr++ = e_req->Id;
      *req_ptr++ = e_req->Req;
      if ((e_req->Id & 0x1f) == 0) {
        *req_ptr++ = e_no;
      } else {
        *req_ptr++ = e_req->ReqCh;
      }
      *req_ptr++ = (byte)data_length;
      copy_req_data (e_req, req_ptr);
      e_req->XOffset = 0xffff;
    } else {
     /*
       Use external memory to pass the data to the adapter
       Allocation of the DMA descriptors does not need the syncronization:
       The allocation of the DMA descriptors by allocations is disabled for this adapter.
       */
      int entry_nr = diva_dma_fragment_map_alloc (IoAdapter->fragment_map,
                                                  (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                                  data_length, &IoAdapter->data_spin_lock);
      unsigned long dma_magic, dma_magic_hi;
      void* local_addr;
      if (entry_nr < 0) {

        DBG_TRC(("A(%d) req no external buffer", IoAdapter->ANum))

				if (req_sent != 0) {
					DBG_TRC(("A(%d) sent req, nr:%d", IoAdapter->ANum, IoAdapter->host_vidi.dma_nr_pending_requests))
					diva_vidi_sync_buffers_and_start_tx_dma(IoAdapter);
				}
        return;
      }

      diva_dma_fragment_map_get_dma_map_entry (IoAdapter->fragment_map,
                                               (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                               (word)entry_nr, &local_addr, &dma_magic);
      diva_dma_fragment_map_get_dma_map_entry_hi (IoAdapter->fragment_map,
                                                  (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                                  (word)entry_nr, &dma_magic_hi);
      *req_ptr++ = e_req->Id;
      *req_ptr++ = e_req->Req;
      if ((e_req->Id & 0x1f) == 0) {
        *req_ptr++ = e_no;
      } else {
        *req_ptr++ = e_req->ReqCh;
      }
      *req_ptr++ = 0x8a; /* 0x80 | (2 byte transfer length + 8 byte data) */
      WRITE_WORD(req_ptr, ((word)data_length));
      req_ptr += 2;
      WRITE_DWORD(req_ptr, ((dword)dma_magic)); /* Low */
      req_ptr += 4;
      WRITE_DWORD(req_ptr, ((dword)dma_magic_hi)); /* Hi */

      e_req->XOffset = (word)entry_nr;

      copy_req_data (e_req, local_addr);
    }
    next_req(a);
    ReadyCount--;
    IoAdapter->host_vidi.dma_nr_pending_requests++;
    IoAdapter->host_vidi.local_request_counter++;
    update_req_ptr (IoAdapter); 
    req_sent = 1;

    if ((e_req->Id & 0x1f) != 0) {
      xdi_xlog_request ((byte)IoAdapter->ANum, e_req->Id, e_req->ReqCh, e_req->Req, a->IdTypeTable[e_req->No]);
      if (e_req->Req == REMOVE) {
        a->misc_flags_table[e_no] |= DIVA_MISC_FLAGS_REMOVE_PENDING;
        cancel_rnr_on_remove(a, e_req);
      }
    } else {
      a->IdTypeTable[e_req->No] = e_req->Id;
      xdi_xlog_request ((byte)IoAdapter->ANum, e_req->Id, e_req->ReqCh, e_req->Req, e_req->Id);
      assign_queue(a, e_no, e_no);
    }
    e_no = look_req(a);
  }

  if (req_sent) {
    DBG_TRC(("A(%d) sent req, nr:%d", IoAdapter->ANum, IoAdapter->host_vidi.dma_nr_pending_requests))
    diva_vidi_sync_buffers_and_start_tx_dma(IoAdapter);
  }
}

static void vidi_save_ind_descr_ack (ADAPTER* a, word entry_nr) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;

  if (diva_vidi_adapter_handle_buffer_put  (&IoAdapter->host_vidi.pending_ind_buffer_ack, entry_nr) != 0) {
    DBG_FTL(("A(%d) ind buffer ack %d lost", IoAdapter->ANum, entry_nr))
  }
}

static void vidi_save_ind_ack (ADAPTER* a, byte Id) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;

  if (diva_vidi_adapter_handle_buffer_put  (&IoAdapter->host_vidi.pending_ind_ack, Id) != 0) {
    DBG_FTL(("A(%d) Id:%02x ind ack lost", IoAdapter->ANum, Id))
  }
}

static int vidi_save_rnr_ind (ADAPTER* a,
                              byte Id,
                              byte Ind,
                              byte IndCh,
                              byte* data,
                              word data_length,
                              int entry_nr) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  diva_rnr_q_entry_hdr_t* hdr;
  int initial_empty = diva_q_get_head (&IoAdapter->host_vidi.used_rnr_q) == 0;

  if (data_length > sizeof(hdr->ind_data.local_data)) {
    if ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(&IoAdapter->host_vidi.free_rnr_q)) != 0) {
      diva_q_remove (&IoAdapter->host_vidi.free_rnr_q, &hdr->link);
    } else if ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(&IoAdapter->host_vidi.free_rnr_hdr_q)) != 0) {
      diva_q_remove (&IoAdapter->host_vidi.free_rnr_hdr_q, &hdr->link);

      if ((hdr->ind_data.data = diva_os_malloc (DIVA_OS_MALLOC_ATOMIC,
                                               IoAdapter->host_vidi.dma_req_buffer_length)) == 0) {
        if (entry_nr >= 0) {
          hdr->data_length = data_length | DIVA_VIDI_RNR_ENTRY_VALID;
          hdr->ind_data.entry_nr = entry_nr;
          hdr->Id    = Id;
          hdr->Ind   = Ind;
          hdr->IndCh = IndCh;
          diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
          return (initial_empty);
        } else {
          DBG_ERR(("A(%d) ind lost, no entry Id:%02x Ind:%02x IndCh:%02x",
                    IoAdapter->ANum, Id, Ind, IndCh))
          vidi_save_ind_ack (a, Id);
          return (0);
        }
      }
    } else {
      DBG_ERR(("A(%d) ind lost Id:%02x Ind:%02x IndCh:%02x", IoAdapter->ANum, Id, Ind, IndCh))
      if (entry_nr >=0)
        vidi_save_ind_descr_ack (a, (word)entry_nr);
      vidi_save_ind_ack (a, Id);
      return (0);
    }
    if (entry_nr >=0)
      vidi_save_ind_descr_ack (a, (word)entry_nr);
    memcpy (hdr->ind_data.data, data, data_length);
    hdr->data_length = DIVA_VIDI_RNR_BUFFER_VALID | data_length;
    hdr->Id    = Id;
    hdr->Ind   = Ind;
    hdr->IndCh = IndCh;
    diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
    return (initial_empty);
  }

  if (entry_nr >=0)
    vidi_save_ind_descr_ack (a, (word)entry_nr);

  if ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(&IoAdapter->host_vidi.free_rnr_hdr_q)) != 0) {
    diva_q_remove (&IoAdapter->host_vidi.free_rnr_hdr_q, &hdr->link);
    memcpy (&hdr->ind_data.local_data[0], data, data_length);
    hdr->data_length = data_length;
    hdr->Id    = Id;
    hdr->Ind   = Ind;
    hdr->IndCh = IndCh;
    diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
    return (initial_empty);
  }
  if ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(&IoAdapter->host_vidi.free_rnr_q)) != 0) {
    diva_q_remove (&IoAdapter->host_vidi.free_rnr_q, &hdr->link);
    memcpy (hdr->ind_data.data, data, data_length);
    hdr->data_length = DIVA_VIDI_RNR_BUFFER_VALID | data_length;
    hdr->Id    = Id;
    hdr->Ind   = Ind;
    hdr->IndCh = IndCh;
    diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
    return (initial_empty);
  }

  DBG_ERR(("A(%d) ind lost no rnr entrues Id:%02x Ind:%02x IndCh:%02x",
            IoAdapter->ANum, Id, Ind, IndCh))
  vidi_save_ind_ack (a, Id);

  return (0);
}

/*
  Copy data from FSM buffer to the buffer chain provided by application
  */
static void vidi_copy_ind_to_user (ENTITY* e, const byte* src, int length) {
  int i = 0;
  int to_copy;

  while ((i < e->RNum) && length) {
    if ((to_copy = MIN(e->R[i].PLength, length))) {
      if (e->R[i].P) {
        memcpy (e->R[i].P, src, to_copy);
      }
      e->R[i].PLength = (word)to_copy;
      length -= to_copy;
      src    += to_copy;
    }
    i++;
  }

  if ((e->RNum = (byte)i) == 0)
    e->R->PLength = 0;
}

static void stop_rnr_timer (PISDN_ADAPTER IoAdapter, int force) {
  if (diva_q_get_head(&IoAdapter->host_vidi.used_rnr_q) == 0) {
    if (IoAdapter->host_vidi.rnr_timer == 1 && force == 0) {
      IoAdapter->host_vidi.rnr_timer = 0;
    } else {
      IoAdapter->host_vidi.rnr_timer = 2;
    }
  }
}

static void free_rnr_entry (ADAPTER* a, diva_rnr_q_entry_hdr_t* hdr) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;

  hdr->rnr_counter = 0;

  if ((hdr->data_length & DIVA_VIDI_RNR_BUFFER_VALID) != 0) {
    diva_q_add_tail (&IoAdapter->host_vidi.free_rnr_q, &hdr->link);
  } else {
    if ((hdr->data_length & DIVA_VIDI_RNR_ENTRY_VALID) != 0) {
      vidi_save_ind_descr_ack (a, (word)hdr->ind_data.entry_nr);
      hdr->data_length = 0;
    }
    diva_q_add_tail (&IoAdapter->host_vidi.free_rnr_hdr_q, &hdr->link);
  }
}

static void cancel_rnr_on_remove(ADAPTER* a, ENTITY* e) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  diva_entity_queue_t* q = &IoAdapter->host_vidi.used_rnr_q;
  diva_rnr_q_entry_hdr_t* hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(q);

  while (hdr != 0) {
    if (hdr->Id == e->Id) {
      diva_q_remove (q, &hdr->link);
      free_rnr_entry (a,hdr);
      stop_rnr_timer (IoAdapter, 0);
      break;
    }
    hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_next(&hdr->link);
  }
}

static void diva_process_rnr_ind (ADAPTER* a, byte requestedId, byte requestedCh) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  diva_entity_queue_t q   = IoAdapter->host_vidi.used_rnr_q;
  diva_rnr_q_entry_hdr_t* hdr;

  diva_q_init (&IoAdapter->host_vidi.used_rnr_q);

  while ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(&q)) != 0) {
    ENTITY* e_ind;
    byte e_no;
    diva_q_remove (&q, &hdr->link);

    if (requestedId != 0 && (hdr->Id != requestedId || hdr->IndCh != requestedCh)) {
      diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
      continue;
    }

    if ((e_no = a->IdTable[hdr->Id]) == 0 || (e_ind = entity_ptr(a,e_no)) == 0) {
      DBG_ERR(("A(%d) failed to map rnrn ind Id:%02x Ind:%02x IndCh:%02x",
               IoAdapter->ANum, hdr->Id, hdr->Ind, hdr->IndCh))
      free_rnr_entry (a, hdr);
    } else {
      byte* local_addr = &hdr->ind_data.local_data[0];
      word data_length = hdr->data_length & ~(DIVA_VIDI_RNR_BUFFER_VALID | DIVA_VIDI_RNR_ENTRY_VALID);

      if (a->IdTypeTable[e_no] == DSIG_ID) {
        if (++hdr->rnr_counter < 5) {
          diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
          continue;
        }
        hdr->rnr_counter = 0;
      }

      if (hdr->data_length & DIVA_VIDI_RNR_BUFFER_VALID) {
        local_addr = hdr->ind_data.data;
      } else if (hdr->data_length & DIVA_VIDI_RNR_ENTRY_VALID) {
        unsigned long dma_magic;
        diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                hdr->ind_data.entry_nr, (void**)&local_addr, &dma_magic);
      }
      e_ind->Ind      = hdr->Ind;
      e_ind->IndCh    = hdr->IndCh;
      e_ind->complete = (data_length <= sizeof(IoAdapter->RBuffer.P));
      e_ind->RLength  = data_length;
      e_ind->RNum     = 0;
      e_ind->RNR      = 0;

      if ((IoAdapter->RBuffer.length = MIN(data_length, sizeof(IoAdapter->RBuffer.P)))) {
        memcpy (IoAdapter->RBuffer.P, local_addr, IoAdapter->RBuffer.length);
      }
      e_ind->RBuffer = (DBUFFER*)&IoAdapter->RBuffer;
      CALLBACK(a, e_ind);
      xdi_xlog_ind ((byte)IoAdapter->ANum, hdr->Id, hdr->IndCh, hdr->Ind,
                    1/* rnr_valid */, e_ind->RNR/* rnr */, a->IdTypeTable[e_no]);
      if (e_ind->RNR == 1) {
        e_ind->RNR = 0;
        diva_q_add_tail (&IoAdapter->host_vidi.used_rnr_q, &hdr->link);
        continue;
      }
      if (e_ind->RNum == 0 || e_ind->RNR == 2) {
        if (e_ind->RNum == 0) {
          xdi_xlog_ind ((byte)IoAdapter->ANum, hdr->Id, hdr->IndCh,
                        hdr->Ind, 2/* rnr_valid */, 0/* rnr */, a->IdTypeTable[e_no]);
        }
        vidi_save_ind_ack (a, hdr->Id);
        e_ind->RNR = 0;
        free_rnr_entry (a, hdr);
        continue;
      }
      vidi_save_ind_ack (a, hdr->Id);  /* Indication accepted */
      e_ind->Ind = hdr->Ind;
      vidi_copy_ind_to_user (e_ind, local_addr, data_length);
      e_ind->complete = 2;
      xdi_xlog_ind ((byte)IoAdapter->ANum, hdr->Id, hdr->IndCh,
                    hdr->Ind, 3/* rnr_valid */, 0/* rnr */, a->IdTypeTable[e_no]);
      CALLBACK(a, e_ind);
      free_rnr_entry (a, hdr);
    }
  }
}

static void diva_vidi_process_oob (ADAPTER *a) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  byte Id, Ch;

  while (diva_vidi_oob_get (IoAdapter, &Id, &Ch) == 0) {
		diva_process_rnr_ind (a, Id, Ch);
  }
}

/*
  Process one indication vidi
  */
static void vidi_dispatch_ind (ADAPTER* a) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  volatile byte* msg = &IoAdapter->host_vidi.ind_buffer_base_addr[IoAdapter->host_vidi.current_ind_ptr];
  byte Id         = *msg++;
  byte Ind        = *msg++;
  byte IndCh      = *msg++;
  byte msg_type   = *msg++;
  byte msg_length = *msg++;


  if (Id == DIVA_SYS_MSG_ID) {
    /*
      Process system message
      */
    switch (Ind) {
      case DIVA_SYS_MSG_ACK_REQ_ENTRY: {
        dword nr = READ_DWORD(msg);

        DBG_TRC(("A(%d) received ack: %d pending req = %d", IoAdapter->ANum, nr, IoAdapter->host_vidi.dma_nr_pending_requests))

        if (nr <= (dword)IoAdapter->host_vidi.dma_nr_pending_requests) {
          IoAdapter->host_vidi.dma_nr_pending_requests -= nr;
        } else {
          DBG_FTL(("A(%d) too many requests acknowledged %d > %d",
                   IoAdapter->ANum, nr , IoAdapter->host_vidi.dma_nr_pending_requests))
        }
      } break;

      case DIVA_SYS_MSG_RNR_IND: /* Reply saved RNR indications */
        diva_process_rnr_ind (a, 0, 0);
        stop_rnr_timer (IoAdapter, 1);
        break;

      case DIVA_SYS_MSG_SYSTEM_START:
        DBG_LOG(("A(%d) adapter started, vidi", IoAdapter->ANum))
        IoAdapter->host_vidi.vidi_started = 1;
        break;

      case DIVA_SYS_MSG_READ_XLOG: {
        diva_os_spin_lock_magic_t OldIrql;
        diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm_read");
        if (IoAdapter->host_vidi.pcm_data != 0) {
          int xlog_msg_start = IndCh & 0x01;
          int xlog_msg_end   = IndCh & 0x02;

          if (xlog_msg_start != 0) {
            memcpy (IoAdapter->host_vidi.pcm_data, (byte*)msg, msg_length);
            if (xlog_msg_end != 0) {
              IoAdapter->host_vidi.pcm_data  = 0;
              IoAdapter->host_vidi.pcm_state = 0;
            } else {
              IoAdapter->host_vidi.pcm_state = 0x80000000 + msg_length;
            }
          } else if ((IoAdapter->host_vidi.pcm_state & 0x80000000) != 0) {
            memcpy (&IoAdapter->host_vidi.pcm_data[IoAdapter->host_vidi.pcm_state & ~0x80000000],
                    (byte*)msg, msg_length);
            if (xlog_msg_end != 0) {
              IoAdapter->host_vidi.pcm_data  = 0;
              IoAdapter->host_vidi.pcm_state = 0;
            } else {
              IoAdapter->host_vidi.pcm_state += msg_length;
            }
          }
        } else {
          IoAdapter->host_vidi.pcm_state = 0;
        }
        diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm_read");
      } break;

      case DIVA_SYS_MSG_CPU_STOP:
        if (msg_length >= 4 && msg[1] == 'T') {
          const char* msgFmt = "Stop Board %d due to thermal problem, T=%dC, %dC above max";
          DBG_ERR(((char*)msgFmt, IoAdapter->ANum, msg[2], msg[3]))
          diva_os_write_system_log_message (-1, "Thermal problem");
        } else {
          DBG_ERR(("A(%d) CPU stop message received", IoAdapter->ANum))
        }
        IoAdapter->host_vidi.cpu_stop_message_received = 2;
        break;

      case DIVA_SYS_MSG_SYSTEM_WARNING:
        if (msg_length >= 4) {
          const char* warning_type = "Unknown";

          switch (msg[0]) {
            case 'E':
            case 'e':
              warning_type = "Error, stop";
              break;
            case 'W':
              warning_type = "Warning for";
              break;
          }
          switch (msg[1]) {
            case 'T': {
              const char* msgFmt = "%s Board %d due to thermal problem, T=%dC, %dC above max";
              DBG_ERR(((char*)msgFmt, warning_type, IoAdapter->ANum, msg[2], msg[3]))
              diva_os_write_system_log_message (msg[0] != 'W' ? -1 : 0, "Thermal problem");
            } break;
            default:
              DBG_ERR(("A(%d) %s event, info=%02x %02x",
                       IoAdapter->ANum, warning_type, msg[2], msg[3]))
              break;
          }
          if (msg[0] == 'e' || msg[0] == 'E') {
            /*
              Stop adapter
              */
            int stop_adapter = 1;

            if (IoAdapter->notify_adapter_error_proc != 0)
              stop_adapter = IoAdapter->notify_adapter_error_proc(IoAdapter, msg[0], msg[1], msg[2], msg[3]);
            if (stop_adapter != 0 || msg[0] == 'E')
              IoAdapter->host_vidi.request_flags |= DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT;
          }
        }
        break;

      default:
        DBG_ERR(("A(%d) unknown system message: %02x", IoAdapter->ANum, Ind))
        if (msg_length != 0) {
          DBG_BLK(((void*)msg, msg_length))
        }
        break;
    }
  } else {
    if (msg_type == 0) {
      /*
        Process return codes
        */
      byte e_no = a->IdTable[Id];
      byte Rc   = Ind;
      byte RcCh = IndCh;

      DBG_TRC(("A(%d) e_no = %d, received rc: %02x %x %x %x %x", IoAdapter->ANum, e_no, Id, Ind, IndCh, msg_type, msg_length))

      if (e_no != 0) { /* Entity already exists */
        ENTITY* e_rc = entity_ptr(a,e_no);
        byte e_type = a->IdTypeTable[e_no];
        dword extended_info_type = 0;
        int entity_removed = 0;

        if (msg_length > 3)
          extended_info_type = READ_DWORD(msg);

        xdi_xlog_rc_event ((byte)IoAdapter->ANum, Id, RcCh, Rc, 0, e_type);

        if ((a->misc_flags_table[e_no] & DIVA_MISC_FLAGS_REMOVE_PENDING) != 0 && Rc == OK) {
          if ((extended_info_type != DIVA_RC_TYPE_REMOVE_COMPLETE) && e_type == NL_ID) {
            DBG_TRC(("XDI: A(%02x) Id:%02x, N-REMOVE ignore RC=OK", IoAdapter->ANum, Id))
            return;
          }
          a->misc_flags_table[e_no] &= ~DIVA_MISC_FLAGS_REMOVE_PENDING;

          cancel_rnr_on_remove(a, e_rc);

          free_entity(a, e_no);
          a->IdTable[Id] = 0;
          e_rc->Id = 0;
          entity_removed = 1;
        }

        if (extended_info_type != DIVA_RC_TYPE_OK_FC && e_rc->XOffset != 0xffff) {
          diva_dma_fragment_map_free  (IoAdapter->fragment_map,
                                       (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                       e_rc->XOffset, &IoAdapter->data_spin_lock);
          e_rc->XOffset = 0xffff;
        }

        e_rc->RNR  = (byte)extended_info_type;
        e_rc->Rc   = Rc;
        e_rc->RcCh = RcCh;
        e_rc->complete=0xff;
        xdi_xlog_rc_event ((byte)IoAdapter->ANum, Id, RcCh, Rc, 1, e_type);
        e_rc->More &= ~XBUSY;
        CALLBACK(a, e_rc);
        if (entity_removed == 0) {
          e_rc->RNR  = 0;
        }
      } else if ((Rc & 0xf0) == ASSIGN_RC) { /* Process assign Rc */
        if ((e_no = get_assign(a, RcCh)) != 0) { /* RcCh contains assign reference */
          ENTITY* e_rc = entity_ptr(a,e_no);

          xdi_xlog_rc_event ((byte)IoAdapter->ANum, Id, RcCh, Rc, 2, a->IdTypeTable[e_no]);

          e_rc->More &= ~XBUSY;
          e_rc->Id = Id;
          e_rc->Rc = Rc;
          e_rc->complete=0xff;

          if (e_rc->XOffset != 0xffff) {
            diva_dma_fragment_map_free  (IoAdapter->fragment_map,
                                         (struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                         e_rc->XOffset, &IoAdapter->data_spin_lock);
            e_rc->XOffset = 0xffff;
          }

          CALLBACK(a, e_rc);

          if (Rc == ASSIGN_OK) {
            a->IdTable[Id] = e_no;
          } else {
            free_entity(a, e_no);
            a->IdTable[Id] = 0;
          }
        } else {
          DBG_FTL(("A(%d) unknown assign rc: Id:%02x Rc:%02x RcCh:%02x", IoAdapter->ANum, Id, Rc, RcCh))
        }
      } else { /* Unknown entity, unknown Rc */
        DBG_FTL(("A(%d) unknown rc: Id:%02x Rc:%02x RcCh:%02x", IoAdapter->ANum, Id, Rc, RcCh))
      }
    } else {
      /*
        Process indications
        */
      int entry_nr_to_free = -1;
      byte* local_addr = (byte*)msg;
      word data_length = msg_length;
      byte e_no = a->IdTable[Id];

      if ((msg_length & 0x80) != 0) {
        unsigned long dma_magic;
        /*
          Data located in the external buffer
          */
        data_length      = READ_WORD(msg);
        entry_nr_to_free = READ_WORD(&msg[2]);
        diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                                entry_nr_to_free, (void**)&local_addr, &dma_magic);
      }

      if (e_no != 0) {
        ENTITY* e_ind = entity_ptr(a,e_no);

        xdi_xlog_ind ((byte)IoAdapter->ANum, Id, IndCh, Ind, 0/* rnr_valid */, 0 /* rnr */, a->IdTypeTable[e_no]);

        if ((a->misc_flags_table[e_no] & DIVA_MISC_FLAGS_REMOVE_PENDING) != 0) {
          if (entry_nr_to_free >= 0)
            vidi_save_ind_descr_ack (a, (word)entry_nr_to_free);
          return;
        }

        e_ind->Ind      = Ind;
        e_ind->IndCh    = IndCh;
        e_ind->complete = (data_length <= sizeof(IoAdapter->RBuffer.P));
        e_ind->RLength  = data_length;
        e_ind->RNum     = 0;
        e_ind->RNR      = 0;

        if ((IoAdapter->RBuffer.length = MIN(data_length, sizeof(IoAdapter->RBuffer.P)))) {
          memcpy (IoAdapter->RBuffer.P, local_addr, IoAdapter->RBuffer.length);
        }
        e_ind->RBuffer = (DBUFFER*)&IoAdapter->RBuffer;
        CALLBACK(a, e_ind);
        xdi_xlog_ind ((byte)IoAdapter->ANum, Id, IndCh, Ind,
                      1/* rnr_valid */, e_ind->RNR/* rnr */, a->IdTypeTable[e_no]);

        DBG_TRC(("e->RNR = %d - e->RNum = %d", e_ind->RNR, e_ind->RNum))
 
        if (e_ind->RNR == 1) {
          e_ind->RNR = 0;
          if (vidi_save_rnr_ind (a, Id, Ind, IndCh, local_addr, data_length, entry_nr_to_free) != 0) {
            IoAdapter->host_vidi.rnr_timer = 1;
          }
          return;
        }
        if (e_ind->RNum == 0 || e_ind->RNR == 2) {
          if (e_ind->RNum == 0) {
            xdi_xlog_ind ((byte)IoAdapter->ANum, Id, IndCh, Ind, 2/* rnr_valid */, 0/* rnr */, a->IdTypeTable[e_no]);
          }
          vidi_save_ind_ack (a, Id);
          if (entry_nr_to_free >= 0)
            vidi_save_ind_descr_ack (a, (word)entry_nr_to_free);
          e_ind->RNR = 0;
          return;
        }
        if (e_ind->RNR == 3) {
          /*
             Delayed indication acknowledge
             */
          vidi_save_ind_ack (a, Id);
          if (entry_nr_to_free >= 0)
            vidi_save_ind_descr_ack (a, (word)entry_nr_to_free);
          e_ind->RNR = 0;
          return;
        }
        vidi_save_ind_ack (a, Id);  /* Indication accepted */
        e_ind->Ind = Ind;
        vidi_copy_ind_to_user (e_ind, local_addr, data_length);
        e_ind->complete = 2;
        xdi_xlog_ind ((byte)IoAdapter->ANum, Id, IndCh, Ind, 3/* rnr_valid */, 0/* rnr */, a->IdTypeTable[e_no]);
        CALLBACK(a, e_ind);
        if (entry_nr_to_free >= 0)
          vidi_save_ind_descr_ack (a, (word)entry_nr_to_free);
      } else {
        if (entry_nr_to_free >= 0)
          vidi_save_ind_descr_ack (a, (word)entry_nr_to_free);

        DBG_FTL(("A(%d) unexpected Ind:%02x for Id:%02x", IoAdapter->ANum, Ind, Id))
      }
    }
  }
}

/*
  Scheduled by interrupt handler to process
  indications and return codes
  */
byte vidi_host_pr_dpc(ADAPTER *a) {
  PISDN_ADAPTER IoAdapter = (PISDN_ADAPTER)a->io;
  int original_ind_count;
  int nr_ind;

  if (IoAdapter->host_vidi.cpu_stop_message_received != 0)
    return (1);

  if (IoAdapter->oob_q.count != 0)
    diva_vidi_process_oob (a);

  do { /* Check for new indications and process them */
    original_ind_count = IoAdapter->host_vidi.local_indication_counter;

    IoAdapter->host_vidi.local_indication_counter = *(volatile int*)IoAdapter->host_vidi.remote_indication_counter;
    nr_ind = (IoAdapter->host_vidi.local_indication_counter - original_ind_count);
    IoAdapter->host_vidi.nr_ind_processed += nr_ind;

    DBG_TRC(("A(%d) local ind: %d", IoAdapter->ANum, IoAdapter->host_vidi.local_indication_counter))

    while (nr_ind) {
      vidi_dispatch_ind (a);
      IoAdapter->host_vidi.current_ind_ptr += IoAdapter->host_vidi.dma_segment_length;
      if (IoAdapter->host_vidi.current_ind_ptr >= IoAdapter->host_vidi.dma_ind_buffer_length) {
        IoAdapter->host_vidi.current_ind_ptr = 0;
      }
      nr_ind--;
    }
  } while (nr_ind > 0);

  /* Check now for the request */
  vidi_host_pr_out (a);

	return (1);
}

static int get_nr_dma_map_entries (PISDN_ADAPTER IoAdapter) {
	if (IoAdapter->dma_map != 0) {
		int i;

		for (i = 0;; i++) {
			void* v;
			unsigned long h;

			diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map, i, &v, &h);
			if (v == 0)
				return (i);
		}
	}

	return (0);
}

int diva_init_vidi (PISDN_ADAPTER IoAdapter) {

  DBG_LOG(("A(%d) start vidi, active:%d dma_map:%p counter:%p",
          IoAdapter->ANum, IoAdapter->host_vidi.vidi_active, IoAdapter->dma_map,
          IoAdapter->host_vidi.remote_indication_counter))

	if (IoAdapter->host_vidi.vidi_active != 0 && IoAdapter->dma_map != 0 &&
			IoAdapter->host_vidi.remote_indication_counter == 0) {
    int entry_nr;
    unsigned long dma_magic, dma_magic_hi;
    void* local_addr;
    struct _diva_dma_map_entry* ind_dma_map = IoAdapter->dma_map2 != 0 ?
								(struct _diva_dma_map_entry*)IoAdapter->dma_map2 :
								(struct _diva_dma_map_entry*)IoAdapter->dma_map;

    IoAdapter->host_vidi.dma_segment_length = 32;

    IoAdapter->host_vidi.remote_indication_counter_offset = \
    IoAdapter->host_vidi.dma_ind_buffer_length =
			4096 * (1 + (IoAdapter->dma_map2 != 0)) - IoAdapter->host_vidi.dma_segment_length;

    IoAdapter->host_vidi.dma_req_buffer_length = 4096;
    IoAdapter->host_vidi.dma_max_nr_pending_requests = 4096/32;
    IoAdapter->host_vidi.dma_segment_req_data_length = IoAdapter->host_vidi.dma_segment_length - 4;
    /* Offset in the control memory block of every Adapter */
    IoAdapter->host_vidi.adapter_req_counter_offset = 0x68;

    entry_nr = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map);
    if (entry_nr < 0) {
      DBG_ERR(("Adapter %d VIDI init failed", IoAdapter->ANum))
      return (-1);
    }

    IoAdapter->host_vidi.req_entry_nr = entry_nr;
    diva_get_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                            entry_nr, &local_addr, &dma_magic);
    diva_get_dma_map_entry_hi ((struct _diva_dma_map_entry*)IoAdapter->dma_map,
                               entry_nr, &dma_magic_hi);
    IoAdapter->host_vidi.req_buffer_base_addr = (byte*)local_addr;
    IoAdapter->host_vidi.req_buffer_base_dma_magic = (dword)dma_magic;
    IoAdapter->host_vidi.req_buffer_base_dma_magic_hi = (dword)dma_magic_hi;

		DBG_LOG(("A(%d) vidi req: %d:(%08x:%08x)%p",
						IoAdapter->ANum, entry_nr, (dword)dma_magic, (dword)dma_magic_hi, local_addr))

    entry_nr = diva_alloc_dma_map_entry (ind_dma_map);
    if (entry_nr < 0) {
      DBG_ERR(("Adapter %d VIDI init failed", IoAdapter->ANum))
      return (-1);
    }
    diva_get_dma_map_entry (ind_dma_map, entry_nr, &local_addr, &dma_magic);
    diva_get_dma_map_entry_hi (ind_dma_map, entry_nr, &dma_magic_hi);
    IoAdapter->host_vidi.ind_buffer_base_addr = (byte*)local_addr;
    IoAdapter->host_vidi.ind_buffer_base_dma_magic = (dword)dma_magic;
    IoAdapter->host_vidi.ind_buffer_base_dma_magic_hi = (dword)dma_magic_hi;

		DBG_LOG(("A(%d) vidi ind: %d:(%08x:%08x):%p",
						IoAdapter->ANum, entry_nr, (dword)dma_magic, (dword)dma_magic_hi, local_addr))

    IoAdapter->host_vidi.remote_indication_counter =
            IoAdapter->host_vidi.ind_buffer_base_addr + IoAdapter->host_vidi.dma_ind_buffer_length;

    *(volatile dword*)IoAdapter->host_vidi.remote_indication_counter = 0;

		diva_vidi_init_adapter_handle_buffer (&IoAdapter->host_vidi.pending_ind_buffer_ack,
																					(byte*)&IoAdapter->a.rx_stream[0],
																					sizeof(IoAdapter->a.rx_stream));

		diva_vidi_init_adapter_handle_buffer (&IoAdapter->host_vidi.pending_ind_ack,
																					(byte*)&IoAdapter->a.tx_stream[0],
																					sizeof(IoAdapter->a.tx_stream));
		/*
			Allocate and send RX buffers to adapter
			*/
    {
      int nr_dma_map_entries = get_nr_dma_map_entries (IoAdapter);
      int i, nr_free_dma_entries = diva_nr_free_dma_entries((struct _diva_dma_map_entry*)IoAdapter->dma_map);
      int nr_rx_dma_entries;
      int nr_tx_dma_entries;
      int bri_hardware = IoAdapter->Properties.Channels == 2 && !(IoAdapter->Properties.Card == CARD_POTS || IoAdapter->Properties.Card == CARD_POTSV);

      if (nr_dma_map_entries == nr_free_dma_entries+2-(IoAdapter->dma_map2 != 0))
        nr_free_dma_entries -= IoAdapter->Properties.nrExportXconnectDescriptors[0][IoAdapter->xconnectExportMode];

      if (IoAdapter->fragment_map == 0) {
      	nr_rx_dma_entries = nr_free_dma_entries/3;
      } else {
        if (bri_hardware == 0) {
          if (nr_free_dma_entries > 32*2) {
            nr_rx_dma_entries = MIN(60, (nr_free_dma_entries-32));
          } else {
            nr_rx_dma_entries = nr_free_dma_entries/2;
          }
        } else {
          nr_rx_dma_entries = 7;
        }
      }
      nr_tx_dma_entries = nr_free_dma_entries - nr_rx_dma_entries;

      DBG_LOG(("A(%d) %d:%d free %d rx %d tx dma entries",
              IoAdapter->ANum, nr_dma_map_entries, nr_free_dma_entries,
							nr_rx_dma_entries, nr_tx_dma_entries))

			for (i = 0, entry_nr = 0; entry_nr >= 0 && i < nr_rx_dma_entries; i++) {
				if ((entry_nr = diva_alloc_dma_map_entry ((struct _diva_dma_map_entry*)IoAdapter->dma_map)) >= 0) {
					if (diva_vidi_adapter_handle_buffer_put (&IoAdapter->host_vidi.pending_ind_buffer_ack,
																													(word)entry_nr) != 0) {
						diva_free_dma_map_entry((struct _diva_dma_map_entry*)IoAdapter->dma_map, entry_nr);
						entry_nr = -1;
					}
				}
			}
		}

		/*
			Initialize RNR headers
			*/
		{
			int i;

			for (i = 0; i < 0xff; i++) {
				IoAdapter->host_vidi.rnr_hdrs[i].data_length = 0;
				diva_q_add_tail (&IoAdapter->host_vidi.free_rnr_hdr_q,
												 &IoAdapter->host_vidi.rnr_hdrs[i].link);
			}
		}

    IoAdapter->host_vidi.vidi_started = 0;

		return (0);
  }

	return (-1);
}

void diva_cleanup_vidi (PISDN_ADAPTER IoAdapter) {
  int q_nr;
  diva_rnr_q_entry_hdr_t* hdr;
  diva_entity_queue_t* q[2];

  q[0] = &IoAdapter->host_vidi.free_rnr_q;
  q[1] = &IoAdapter->host_vidi.used_rnr_q;

  for (q_nr = 0; q_nr < sizeof(q)/sizeof(q[0]); q_nr++) {
    while ((hdr = (diva_rnr_q_entry_hdr_t*)diva_q_get_head(q[q_nr])) != 0) {
      if (q_nr == 0 || (hdr->data_length & DIVA_VIDI_RNR_BUFFER_VALID) != 0) {
        diva_os_free (DIVA_OS_MALLOC_ATOMIC, hdr->ind_data.data);
      }
      diva_q_remove (q[q_nr], &hdr->link);
    }
  }

  memset (&IoAdapter->host_vidi, 0x00, sizeof(IoAdapter->host_vidi));
}

static int diva_vidi_oob_get (PISDN_ADAPTER IoAdapter, byte* Id, byte* Ch) {
	diva_os_spin_lock_magic_t irql;
	diva_oob_entity_queue_t* pQ;
	int ret = -1;

	diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &irql, "xon");

	pQ = &IoAdapter->oob_q;

	if (pQ->count != 0) {
		*Id = pQ->Id[pQ->read];
		*Ch = pQ->Ch[pQ->read];
		pQ->read++;
		if (pQ->read >= sizeof(pQ->Id)/sizeof(pQ->Id[0])) {
			pQ->read = 0;
		}
		pQ->count--;

		ret = 0;
	}

	diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &irql, "xon");

	return (ret);
}

int vidi_pcm_req (PISDN_ADAPTER IoAdapter, ENTITY *e) {
  struct pc_maint *pcm = (struct pc_maint *)&e->Ind;
  diva_os_spin_lock_magic_t OldIrql;
  int i = 0, to_wait = IoAdapter->trapped != 0 ? 300 : 30, read_complete = 0;

  diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");
  IoAdapter->host_vidi.pcm_data = (byte*)pcm;
  IoAdapter->host_vidi.request_flags |= DIVA_VIDI_REQUEST_FLAG_READ_XLOG;
  IoAdapter->host_vidi.pcm_state      = 0;
  diva_os_schedule_soft_isr (&IoAdapter->req_soft_isr);
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");

  do {
    diva_os_sleep (i == to_wait ? 2 : 10);

    diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");
    read_complete = IoAdapter->host_vidi.pcm_data == 0;
    diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");
  } while (read_complete == 0 && ++i <= to_wait);

  diva_os_enter_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");
  IoAdapter->host_vidi.request_flags &= ~DIVA_VIDI_REQUEST_FLAG_READ_XLOG;
  IoAdapter->host_vidi.pcm_data  = 0;
  IoAdapter->host_vidi.pcm_state = 0;
  diva_os_leave_spin_lock (&IoAdapter->data_spin_lock, &OldIrql, "vidi_pcm");

  return (read_complete != 0 ? 0 : -1);
}

/*
  Return one if indication/return codes received
  */
byte vidi_host_test_int (ADAPTER * a) {
  return (1);
}

/*
  Leave empty
  */
void vidi_host_clear_int(ADAPTER * a) {
}

