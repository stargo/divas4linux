/* $Id: capifunc.c,v 1.48 2004/01/11 19:20:54 armin Exp $
 *
 * ISDN interface module for Dialogic active cards DIVA.
 * CAPI Interface common functions
 *
 * Copyright 2000-2009 by Armin Schindler (mac@melware.de)
 * Copyright 2000-2009 Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007 Dialogic
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */
#define CARDTYPE_H_WANT_DATA       1
#define CARDTYPE_EXCLUDE_CARD_NAME 1
#include "platform.h"
#include "os_capi.h"
#include "di_defs.h"
#include "capi20.h"
#include "divacapi.h"
#include "divasync.h"
#include "capifunc.h"
#include "dlist.h"
#include "drv_man.h"
#include "cfg_types.h"
#include "cfg_notify.h"

#define DBG_MINIMUM  (DL_LOG + DL_FTL + DL_ERR)
#define DBG_DEFAULT  (DBG_MINIMUM + DL_XLOG + DL_REG)

DIVA_CAPI_ADAPTER *adapter = (DIVA_CAPI_ADAPTER *) NULL;
APPL *application = (APPL *) NULL;
byte max_appl = MAX_APPL;
byte max_adapter = 0;
static byte diva_capi_adapter_count;
#if !defined(DIVA_EICON_CAPI)
static CAPI_MSG *mapped_msg = (CAPI_MSG *) NULL;
#endif
struct resource_index_entry *c_ind_resource_index = (struct resource_index_entry *) NULL;

byte UnMapController(byte);
char DRIVERRELEASE_CAPI[32];

extern void AutomaticLaw(DIVA_CAPI_ADAPTER *);
extern void callback(ENTITY *);
extern word api_remove_start(void);
extern word CapiRelease(word);
extern word CapiRegister(word);
extern word api_put(APPL *, CAPI_MSG *);

static diva_os_spin_lock_t api_lock;
static diva_os_spin_lock_t mem_q_lock;

static diva_card *cards;
static dword notify_handle;
static void DIRequest(ENTITY * e);
static DESCRIPTOR MAdapter;
static DESCRIPTOR DAdapter;
static byte ControllerMap[MAX_ADAPTER + 1];
static int diva_capi_uses_madapter;


static void diva_register_appl(struct capi_ctr *, __u16,
			       capi_register_params *);
static void diva_release_appl(struct capi_ctr *, __u16);
#if !defined(DIVA_EICON_CAPI)
static char *diva_procinfo(struct capi_ctr *);
static u16 diva_send_message(struct capi_ctr *, diva_os_message_buffer_s *);
#endif

static int diva_capi_read_cfg_value (IDI_SYNC_REQ* sync_req,
                                     int instance,
                                     const char* name,
                                     diva_cfg_lib_value_type_t value_type,
                                     void* data,
                                     dword data_length);
static void diva_process_mem_release_q(int force);

typedef struct _diva_mem_release {
  diva_entity_link_t link;
  unsigned long t;
  void* DataNCCI;
  void* DataFlags;
  void* ReceiveBuffer;
  void* xbuffer_internal;
  void* xbuffer_table;
  int   num_data;
  void** xbuffer_ptr;
} diva_mem_release_t;
static diva_entity_queue_t mem_release_q;


#if !defined(DIVA_EICON_CAPI)
extern void diva_os_set_controller_struct(struct capi_ctr *);
#endif

extern void DIVA_DIDD_Read(DESCRIPTOR *, int);

void *ReceiveBufferGet(APPL *appl, int Num);

/*
 * debug
 */
static void no_printf(unsigned char *fmt, ...) { }
DIVA_DI_PRINTF dprintf = no_printf;

#include "debuglib.c"

void xlog(char *x, ...)
{
	va_list ap;
	if (myDriverDebugHandle.dbgMask & DL_XLOG) {
		va_start(ap, x);
		if (myDriverDebugHandle.dbg_irq) {
			myDriverDebugHandle.dbg_irq(myDriverDebugHandle.id,
						    DLI_XLOG, x, ap);
		} else if (myDriverDebugHandle.dbg_old) {
			myDriverDebugHandle.dbg_old(myDriverDebugHandle.id,
						    x, ap);
		}
		va_end(ap);
	}
}

#if !defined(DIVA_EICON_CAPI)
/*
 * info for proc
 */
static char *diva_procinfo(struct capi_ctr *ctrl)
{
	return (ctrl->serial);
}
#endif

/*
 * stop debugging
 */
static void stop_dbg(void)
{
	DbgDeregister();
	memset(&MAdapter, 0, sizeof(MAdapter));
	dprintf = no_printf;
}

/*
 * Controller mapping
 */
byte MapController(byte Controller)
{
  byte i;
  byte MappedController = 0;
  byte ctrl = Controller & 0x7f;  // mask external controller bit off

  for (i = 1; i < max_adapter + 1; i++)
  {
    if (ctrl == ControllerMap[i])
    {
      MappedController = (byte) i;
      break;
    }
  }
  if (i > max_adapter)
  {
    ControllerMap[0] = ctrl;
    MappedController = 0;
  }
  return (MappedController | (Controller & 0x80)); // put back external controller bit
}

/*
 * Controller unmapping
 */
byte UnMapController(byte MappedController)
{
  byte Controller;
  byte ctrl = MappedController & 0x7f;  // mask external controller bit off

  if (ctrl <= max_adapter)
  {
    Controller = ControllerMap[ctrl];
  }
  else
  {
    Controller = 0;
  }

  return (Controller | (MappedController & 0x80)); // put back external controller bit
}

/*
 * find a new free id
 */
static int find_free_id(void)
{
	int num = 0;
	diva_card *p;

	while (num < 100) {
		num++;
		p = cards;
		while (p) {
			if (p->Id == num)
				break;
			p = p->next;
		}
		if(!p) {
		  return (num);
		}
	}
	return (999);
}

/*
 * find a card structure by controller number
 */
static diva_card *find_card_by_ctrl(word controller)
{
	diva_card *p;

	p = cards;

	while (p) {
		if (ControllerMap[p->Id] == controller) {
      if (p->remove_in_progress) {
			  return 0;
      } else {
			  return p;
      }
		}
		p = p->next;
	}

	return (diva_card *) 0;
}

/*
 * api_remove_start/complete for cleanup
 */
void api_remove_complete(void)
{
	DBG_PRV1(("api_remove_complete"))
}

/*
 * main function called by message.c
 */
void sendf(APPL *appl, word command, dword Id, word Number, byte *format, ...)
{
  word i;
  word length, e_length = 0, dlength = 0;
  CAPI_MSG* msg;
  static CAPI_MSG tmp_msg;
  byte *write, *p_length, level;
  byte *string = 0;
  va_list ap;
  diva_os_message_buffer_s *dmb;
  diva_card *card = NULL;
  unsigned long tmp;

  if(!appl) return;

  DBG_PRV1(("sendf(a=%d,cmd=%x,format=%s)",
             appl->Id,command,(byte *)format))

	/*
		Get message length
		*/
  write = (byte*)&tmp_msg;
  write += 12;
  va_start(ap,format);
  for(i = 0, length = 12, level = 0; format[i] != 0 && level != 0xff; i++) {
    switch(format[i]) {
      case 'b':
        length++;
        if (length <= sizeof(tmp_msg)) {
          tmp = va_arg(ap,unsigned long);
          *(byte *)write = (byte)(tmp & 0xff);
          write += 1;
        }
        break;
      case 'w':
        length += 2;
        if (length <= sizeof(tmp_msg)) {
          tmp = va_arg(ap,unsigned long);
          WRITE_WORD (write, ((word)(tmp & 0xffff)));
          write += 2;
        }
        break;
      case 'd':
        length += 4;
        if (length <= sizeof(tmp_msg)) {
          tmp = va_arg(ap,unsigned long);
          WRITE_DWORD(write, ((dword)tmp));
          write += 4;
        }
        break;
      case 's':
      case 'S':
        string  = va_arg(ap, byte*);
        length += (string[0]+1);
        write  += (string[0]+1);
        break;
      case '(':
        length++;
        write++;
        level++;
        break;
      case ')':
        if (level != 0) {
          level--;
        } else {
          level = 0xff;
        }
        break;
    }
  }
  va_end(ap);

  if (command == _DATA_B3_I) {
    dlength = READ_WORD(((byte*)&tmp_msg.info.data_b3_ind.Data_Length));
    if (sizeof(void*) > 4) {
      e_length = sizeof(void*);
    }
  }
  if (level != 0 ||
      (dmb = diva_os_alloc_message_buffer(length + dlength + e_length,(void**)&write)) == 0) {
    appl->msg_lost = 2;
    DBG_ERR (("AppId 0x%04x Id: %08x command:%04x fmt:%s %s, message lost",
              appl->Id, Id, command, format, (level == 0) ? "no memory" : "format error"))
    return;
  }
  msg = (CAPI_MSG*)write;
  write += 12;
  WRITE_WORD(&msg->header.appl_id, appl->Id);
  WRITE_WORD(&msg->header.command, command);
  if((byte)(command>>8) == 0x82)
    Number = appl->Number++;
  WRITE_WORD(&msg->header.number, Number);
  WRITE_DWORD(((byte *)&msg->header.controller), Id);

  va_start(ap,format);
  for (i = 0, p_length = 0, level = 0; format[i] != 0 && level == 0; i++) {
    switch(format[i]) {
      case 'b':
        tmp = va_arg(ap,unsigned long);
        *(byte *)write = (byte)(tmp & 0xff);
        write += 1;
        break;
      case 'w':
        tmp = va_arg(ap,unsigned long);
        WRITE_WORD (write, ((word)(tmp & 0xffff)));
        write += 2;
        break;
      case 'd':
        tmp = va_arg(ap,unsigned long);
        WRITE_DWORD(write, ((dword)tmp));
        write += 4;
        break;
      case 's':
      case 'S':
        string = va_arg(ap,byte *);
        memcpy (write, &string[0], string[0]+1);
        write += (string[0]+1);
        break;
      case '(':
        *write = (p_length != 0) ? write - p_length : 0;
        p_length = write++;
        break;
      case ')':
        if (p_length != 0) {
          level = *p_length;
          *p_length = (byte)((write - p_length) - 1);
          p_length = (level != 0) ? p_length - level : 0;
          level = 0;
        } else {
          level = 0xff;
        }
        break;
    }
  }
  va_end(ap);

  if (level != 0) {
    appl->msg_lost = 2;
    DBG_ERR (("AppId 0x%04x Id: %08x command:%04x fmt:%s format error, message lost",
             appl->Id, Id, command, format))
    diva_os_free_message_buffer(dmb);
    return;
  }

  if (e_length != 0) {
    WRITE_DWORD(((byte*)&tmp_msg.info.data_b3_ind.Data), 0);
  }

  msg->header.controller = UnMapController(msg->header.controller);
  WRITE_WORD(&msg->header.length, length+e_length);
  write = (byte*)msg;

  /* if DATA_B3_IND, copy data too */
  if (command == _DATA_B3_I) {
    word Num   = READ_WORD(((byte*)&msg->info.data_b3_ind.Number));
    void* data = ReceiveBufferGet(appl, Num);

#if ((!defined(BITS_PER_LONG)) || (BITS_PER_LONG > 32))
    if (sizeof(void*) > 4) {
      write[length  ] = (unsigned long)data & 0xff;
      write[length+1] = ((unsigned long)data >> 8)  & 0xff;
      write[length+2] = ((unsigned long)data >> 16) & 0xff;
      write[length+3] = ((unsigned long)data >> 24) & 0xff;
      write[length+4] = ((unsigned long)data >> 32) & 0xff;
      write[length+5] = ((unsigned long)data >> 40) & 0xff;
      write[length+6] = ((unsigned long)data >> 48) & 0xff;
      write[length+7] = ((unsigned long)data >> 56) & 0xff;
    }
#endif

    memcpy (write + length + e_length, (void *)data, dlength);
  }

  if ( myDriverDebugHandle.dbgMask & DL_XLOG )
  {
    switch ( command )
    {
      default:
        xlog ("\x00\x02", write, 0x81, length) ;
        break ;

      case _DATA_B3_R|CONFIRM:
        if ( myDriverDebugHandle.dbgMask & DL_BLK)
          xlog ("\x00\x02", write, 0x81, length) ;
        break ;

      case _DATA_B3_I:
        if ( myDriverDebugHandle.dbgMask & DL_BLK)
        {
          word Num   = READ_WORD(((byte*)&msg->info.data_b3_ind.Number));
          byte* data = (byte*)ReceiveBufferGet(appl, Num);

          xlog ("\x00\x02", write, 0x81, length+e_length) ;
          for (i = 0; i < dlength; i += 256)
          {
            DBG_BLK((data + i,
              ((dlength - i) < 256) ?
               (dlength - i) : 256  ))
            if ( !(myDriverDebugHandle.dbgMask & DL_PRV0) )
              break ; // not more if not explicitely requested
          }
        }
        break ;
    }
  }

  /* find the card structure for this controller */
  if(!(card = find_card_by_ctrl(write[8] & 0x7f))) {
    DBG_ERR(("sendf - controller %d not found, incoming msg dropped",
            write[8] & 0x7f))
    diva_os_free_message_buffer(dmb);
    return;
  }

  /* send capi msg to capi */
  capi_ctr_handle_message(&card->capi_ctrl, appl->Id, dmb);
}

/*
 * cleanup adapter
 */
static void clean_adapter(int id, diva_entity_queue_t* free_mem_q)
{
	DIVA_CAPI_ADAPTER *a;
	int i, k;

	a = &adapter[id];
	if (diva_capi_adapter_count != 0) {
		diva_capi_adapter_count -= (a->vscapi_mode == 0);
	}
	k = li_total_channels - a->li_channels;
	if (k == 0) {
    if (li_config_table) {
		  diva_q_add_tail (free_mem_q, (diva_entity_link_t*)li_config_table);
		  li_config_table = NULL;
    }
	} else {
		if (a->li_base < k) {
			memmove(&li_config_table[a->li_base],
				&li_config_table[a->li_base + a->li_channels],
				(k - a->li_base) * sizeof(LI_CONFIG));
			for (i = 0; i < k; i++) {
				memmove(&li_config_table[i].flag_table[a->li_base],
					&li_config_table[i].flag_table[a->li_base + a->li_channels],
					k - a->li_base);
				memmove(&li_config_table[i].
					coef_table[a->li_base],
					&li_config_table[i].coef_table[a->li_base + a->li_channels],
					k - a->li_base);
			}
		}
	}
	li_total_channels = k;
	for (i = id; i < max_adapter; i++) {
		if (adapter[i].request)
			adapter[i].li_base -= a->li_channels;
	}
	if (a->plci)
		diva_q_add_tail (free_mem_q, (diva_entity_link_t*)a->plci);

  if (a->mtpx_cfg.cfg != 0)
    diva_q_add_tail (free_mem_q, (diva_entity_link_t*)a->mtpx_cfg.cfg);

	memset(a, 0x00, sizeof(DIVA_CAPI_ADAPTER));
	while ((max_adapter != 0) && !adapter[max_adapter - 1].request)
		max_adapter--;
}

/*
 * remove a card
 * Ensures consisten state of LI tables in the time adapter
 * is removed
 */
static void divacapi_remove_card(DESCRIPTOR * d)
{
	diva_card *last;
	diva_card *card;
	diva_os_spin_lock_magic_t old_irql;
  diva_entity_queue_t free_mem_q;
  diva_entity_link_t* link;

  diva_q_init (&free_mem_q);

  /*
    Set "remove in progress flag".
    Ensures that this is no calls from sendf to CAPI in the
    time CAPI controller is about to be removed.
    */
	diva_os_enter_spin_lock(&api_lock, &old_irql, "remove card");
  card = cards;
	while (card) {
    if (card->d.request == d->request) {
      card->remove_in_progress = 1;
      break;
    }
    card = card->next;
  }
	diva_os_leave_spin_lock(&api_lock, &old_irql, "remove card");

  if (card) {
    /*
      Detach CAPI. Sendf can not call to CAPI any more.
      After detach no calls to "send_message" is done too.
      */
    detach_capi_ctr(&card->capi_ctrl);

    /*
      Now get API lock (to ensure stable state of LI tables)
      and update the adapter map/LI table.
      */
    diva_os_enter_spin_lock(&api_lock, &old_irql, "delete card");

    last = card = cards;
    while (card) {
      if (card->d.request == d->request) {
        clean_adapter(card->Id - 1, &free_mem_q);
			  DBG_LOG(("DelAdapterMap (%d) -> (%d)", ControllerMap[card->Id], card->Id))
	      ControllerMap[card->Id] = 0;
			  DBG_LOG(("adapter remove, max_adapter=%d", max_adapter));

		    if (card == last)
			    cards = card->next;
		    else
			    last->next = card->next;

        break;
      }
      last = card;
      card = card->next;
    }

    diva_os_leave_spin_lock(&api_lock, &old_irql, "delete card");

    while ((link = diva_q_get_head (&free_mem_q))) {
      diva_q_remove (&free_mem_q, link);
      diva_os_free (0, link);
    }

    if (card) {
      diva_os_free (0, card);
    }
  }
}

/*
 * remove cards
 */
static void DIVA_EXIT_FUNCTION divacapi_remove_cards(void)
{
  DESCRIPTOR d;

  while (cards) {
    d.request = cards->d.request;
    divacapi_remove_card (&d);
  }
}

/*
 * sync_callback
 */
static void sync_callback(ENTITY * e)
{
	diva_os_spin_lock_magic_t old_irql;

	DBG_TRC(("cb:Id=%x,Rc=%x,Ind=%x", e->Id, e->Rc, e->Ind))

	diva_os_enter_spin_lock(&api_lock, &old_irql, "sync_callback");
	callback(e);
	diva_os_leave_spin_lock(&api_lock, &old_irql, "sync_callback");
}

/*
** TX Buffer allocation
*/

#define XBUFFER_USED_ANCHOR   0
#define XBUFFER_FREE_ANCHOR   1
#define XBUFFER_ANCHOR_COUNT  2

#define XBufferNext(t) ((t)[XBUFFER_FREE_ANCHOR].next_i - XBUFFER_ANCHOR_COUNT)

static void XBufferInit (struct xbuffer_element *t, word n)
{
  word i;

  t[XBUFFER_USED_ANCHOR].prev_i = XBUFFER_USED_ANCHOR;
  t[XBUFFER_USED_ANCHOR].next_i = XBUFFER_USED_ANCHOR;
  t[XBUFFER_FREE_ANCHOR].prev_i = XBUFFER_FREE_ANCHOR;
  t[XBUFFER_FREE_ANCHOR].next_i = XBUFFER_FREE_ANCHOR;
  for (i = 0; i < n; i++)
  {
    t[XBUFFER_ANCHOR_COUNT + i].prev_i = t[XBUFFER_FREE_ANCHOR].prev_i;
    t[XBUFFER_ANCHOR_COUNT + i].next_i = XBUFFER_FREE_ANCHOR;
    t[t[XBUFFER_FREE_ANCHOR].prev_i].next_i = XBUFFER_ANCHOR_COUNT + i;
    t[XBUFFER_FREE_ANCHOR].prev_i = XBUFFER_ANCHOR_COUNT + i;
  }
}

static void XBufferAlloc (struct xbuffer_element *t, word e)
{
  word i;

  i = e + XBUFFER_ANCHOR_COUNT;
  t[t[i].prev_i].next_i = t[i].next_i;
  t[t[i].next_i].prev_i = t[i].prev_i;
  t[i].next_i = t[XBUFFER_USED_ANCHOR].next_i;
  t[i].prev_i = XBUFFER_USED_ANCHOR;
  t[t[XBUFFER_USED_ANCHOR].next_i].prev_i = i;
  t[XBUFFER_USED_ANCHOR].next_i = i;
}

static void XBufferFree (struct xbuffer_element *t, word e)
{
  word i;

  i = e + XBUFFER_ANCHOR_COUNT;
  t[t[i].prev_i].next_i = t[i].next_i;
  t[t[i].next_i].prev_i = t[i].prev_i;
  t[i].next_i = t[XBUFFER_FREE_ANCHOR].next_i;
  t[i].prev_i = XBUFFER_FREE_ANCHOR;
  t[t[XBUFFER_FREE_ANCHOR].next_i].prev_i = i;
  t[XBUFFER_FREE_ANCHOR].next_i = i;
}

/*
** Buffer RX/TX
*/
void *TransmitBufferSet(APPL *appl, dword ref)
{
  XBufferAlloc (appl->xbuffer_table, (word) ref);
  DBG_PRV1(("%d:xbuf_used(%d)", appl->Id, ref + 1))
  return ((void*)(unsigned long)ref);
}

void *TransmitBufferGet(APPL *appl, void *p)
{
  if(appl->xbuffer_internal[(unsigned long)p])
    return appl->xbuffer_internal[(unsigned long)p];

  return appl->xbuffer_ptr[(unsigned long)p];
}

void TransmitBufferFree(APPL *appl, void *p)
{
  XBufferFree (appl->xbuffer_table, (word)((unsigned long) p));
  DBG_PRV1(("%d:xbuf_free(%d)", appl->Id, ((word)(unsigned long) p) + 1))
}

void *ReceiveBufferGet(APPL *appl, int Num)
{
  return &appl->ReceiveBuffer[Num * appl->MaxDataLength];
}

/*
 * add a new card
 */
static int diva_add_card(DESCRIPTOR * d)
{
	int k = 0, i = 0;
	diva_os_spin_lock_magic_t old_irql;
	diva_card *card = NULL;
	struct capi_ctr *ctrl = NULL;
	DIVA_CAPI_ADAPTER *a = NULL;
	IDI_SYNC_REQ* info_sync_req;
	char serial[16];
	LI_CONFIG *new_li_config_table;
	int j;
  void* mem_to_free = 0;
  dword serial_nr = 0;
  dword cardtype = 0;
	byte xdi_logical_adapter_number = 0;
	byte vscapi_mode = 0;
  const struct _diva_mtpx_adapter_xdi_config_entry* mtpx_cfg = 0;
  dword mtpx_cfg_number_of_configuration_entries = 0;

	if ((info_sync_req = (IDI_SYNC_REQ*)diva_os_malloc(0, sizeof(*info_sync_req))) == 0) {
		DBG_ERR(("diva_add_card: failed to allocate info_sync_req struct."))
		return (0);
	}

	if (!(card = (diva_card *) diva_os_malloc(0, sizeof(diva_card)))) {
		DBG_ERR(("diva_add_card: failed to allocate card struct."))
		diva_os_free (0, info_sync_req);
		    return (0);
	}
	memset((char *) card, 0x00, sizeof(diva_card));
	memcpy(&card->d, d, sizeof(DESCRIPTOR));
  if (diva_capi_uses_madapter) {
    card->d.type = IDI_ADAPTER_MAESTRA;
  }

  /*
    Retrieve channel count from management interface of XDI adapter
    */
  {
    diva_mgnt_adapter_info_t diva_adapter_info;
    int channels = diva_capi_read_channel_count (d, &diva_adapter_info);
    if (channels > 0) {
      card->d.channels = (byte)channels;
      cardtype = diva_adapter_info.cardtype;
      serial_nr = diva_adapter_info.serial_number;
			vscapi_mode = (diva_adapter_info.vscapi_mode != 0);
      xdi_logical_adapter_number = (byte)diva_adapter_info.logical_adapter_number;
      mtpx_cfg_number_of_configuration_entries = diva_adapter_info.mtpx_cfg_number_of_configuration_entries;
      mtpx_cfg = diva_adapter_info.mtpx_cfg;
    }
  }

	info_sync_req->GetName.Req = 0;
	info_sync_req->GetName.Rc = IDI_SYNC_REQ_GET_NAME;
	card->d.request((ENTITY *)info_sync_req);
	strlcpy(card->name, info_sync_req->GetName.name, sizeof(card->name));
	ctrl = &card->capi_ctrl;
	strcpy(ctrl->name, card->name);
	ctrl->register_appl = diva_register_appl;
	ctrl->release_appl = diva_release_appl;
#if !defined(DIVA_EICON_CAPI)
	ctrl->send_message = diva_send_message;
	ctrl->procinfo = diva_procinfo;
#endif
	ctrl->driverdata = card;
#if !defined(DIVA_EICON_CAPI)
	diva_os_set_controller_struct(ctrl);
#endif

#if defined(DIVA_EICON_CAPI)
	/*
		Assign Id, this is necesssary to ensure one to one
		mapping
		*/
	diva_os_enter_spin_lock(&api_lock, &old_irql, "find id");
	card->Id  = find_free_id();
	diva_os_leave_spin_lock(&api_lock, &old_irql, "find id");
	ctrl->cnr = (int)card->Id;
#endif

	if (attach_capi_ctr(ctrl)) {
		DBG_ERR(("diva_add_card: failed to attach controller."))
		diva_os_free (0, info_sync_req);
		diva_os_free(0, card);
		return (0);
	}

  card->PRI = (card->d.channels > 2);
  if (cardtype == 0)
  {
    info_sync_req->GetCardType.Req = 0;
    info_sync_req->GetCardType.Rc  = IDI_SYNC_REQ_GET_CARDTYPE;
    info_sync_req->GetCardType.cardtype = 0;
    card->d.request((ENTITY *)info_sync_req);
    cardtype = info_sync_req->GetCardType.cardtype;
  }

  switch (IDI_PROP_SAFE(cardtype,Card)) {
    case CARD_POTS:
    case CARD_POTSV:
      card->PRI = TRUE;
      break;
  }

#if !defined(DIVA_EICON_CAPI)
	diva_os_enter_spin_lock(&api_lock, &old_irql, "find id");
	card->Id = find_free_id();
	diva_os_leave_spin_lock(&api_lock, &old_irql, "find id");
#endif

	strlcpy(ctrl->manu, M_COMPANY, sizeof(ctrl->manu));
	ctrl->version.majorversion = 2;
	ctrl->version.minorversion = 0;
	ctrl->version.majormanuversion = DRRELMAJOR;
	ctrl->version.minormanuversion = DRRELMINOR;
  if (serial_nr != 0) {
	  info_sync_req->GetSerial.serial = serial_nr;
  } else {
	  info_sync_req->GetSerial.Req = 0;
	  info_sync_req->GetSerial.Rc = IDI_SYNC_REQ_GET_SERIAL;
	  info_sync_req->GetSerial.serial = 0;
	  card->d.request((ENTITY*)info_sync_req);
  }
	if ((i = ((info_sync_req->GetSerial.serial & 0xff000000) >> 24))) {
		sprintf(serial, "%ld-%d",
			info_sync_req->GetSerial.serial & 0x00ffffff, i + 1);
	} else {
		sprintf(serial, "%ld", info_sync_req->GetSerial.serial);
	}
	serial[CAPI_SERIAL_LEN - 1] = 0;
	strlcpy(ctrl->serial, serial, sizeof(ctrl->serial));
#if defined(DIVA_EICON_CAPI)
	memcpy (card->serial, ctrl->serial, MIN(sizeof(card->serial),sizeof(ctrl->serial)));
#endif

	a = &adapter[card->Id - 1];
  a->mtpx_cfg.cfg = mtpx_cfg;
  a->mtpx_cfg.number_of_configuration_entries = mtpx_cfg_number_of_configuration_entries;
	card->adapter = a;
	a->os_card = card;
	ControllerMap[card->Id] = (byte) (ctrl->cnr);

	DBG_TRC(("AddAdapterMap (%d) -> (%d)", ctrl->cnr, card->Id))

  info_sync_req->xdi_capi_prms.Req = 0;
	info_sync_req->xdi_capi_prms.Rc = IDI_SYNC_REQ_XDI_GET_CAPI_PARAMS;
	info_sync_req->xdi_capi_prms.info.structure_length =
	    sizeof(diva_xdi_get_capi_parameters_t);
	card->d.request((ENTITY*)info_sync_req);
	a->flag_dynamic_l1_down =
	    info_sync_req->xdi_capi_prms.info.flag_dynamic_l1_down;
	a->group_optimization_enabled =
	    info_sync_req->xdi_capi_prms.info.group_optimization_enabled;

  a->xdi_logical_adapter_number = xdi_logical_adapter_number;
  a->vscapi_mode                = vscapi_mode;


  diva_capi_read_cfg_value (info_sync_req,
                            card->Id,
                            "XDI\\AN\\GroupOpt",
                            DivaCfgLibValueTypeUnsigned,
                            &a->group_optimization_enabled,
                            sizeof(a->group_optimization_enabled));
  diva_capi_read_cfg_value (info_sync_req,
                            card->Id,
                            "XDI\\AN\\DynL1",
                            DivaCfgLibValueTypeBool,
                            &a->flag_dynamic_l1_down,
                            sizeof(a->flag_dynamic_l1_down));


	a->request = DIRequest;	/* card->d.request; */
	a->max_listen = (a->vscapi_mode == 0) ? (card->PRI ? 8 : 2) : 2;
	k = card->d.channels + ((a->vscapi_mode == 0) ? 32 : 4) + a->max_listen;
#if IMPLEMENT_NULL_PLCI
	if (a->vscapi_mode == 0) {
		k += 2 + card->d.channels;
	}
#endif /* IMPLEMENT_NULL_PLCI */
	if (a->vscapi_mode != 0) {
		k *= 2;
	}
	a->max_plci = (byte)((k > 255) ? 255 : k);
	if (!
	    (a->plci =
	     (PLCI *) diva_os_malloc(0, sizeof(PLCI) * a->max_plci))) {
		DBG_ERR(("diva_add_card: failed alloc plci struct."))
		if (a->mtpx_cfg.cfg != 0) {
			diva_os_free (0, (void*)a->mtpx_cfg.cfg);
		}
		memset(a, 0, sizeof(DIVA_CAPI_ADAPTER));
		diva_os_free (0, info_sync_req);
		return (0);
	}
	memset(a->plci, 0, sizeof(PLCI) * a->max_plci);

	for (k = 0; k < a->max_plci; k++) {
		a->Id = (byte) card->Id;
		a->plci[k].Sig.callback = sync_callback;
		a->plci[k].Sig.XNum = 1;
		a->plci[k].Sig.X = a->plci[k].XData;
		a->plci[k].Sig.user[0] = (word) (card->Id - 1);
		a->plci[k].Sig.user[1] = (word) k;
		a->plci[k].NL.callback = sync_callback;
		a->plci[k].NL.XNum = 1;
		a->plci[k].NL.X = a->plci[k].XData;
		a->plci[k].NL.user[0] = (word) ((card->Id - 1) | 0x8000);
		a->plci[k].NL.user[1] = (word) k;
		a->plci[k].adapter = a;
	}

	a->profile.Number = max_adapter;
	a->profile.Channels = card->d.channels;
	if (card->d.features & DI_FAX3) {
		a->profile.Global_Options = 0x71;
		if (card->d.features & DI_CODEC)
			a->profile.Global_Options |= 0x6;
#if IMPLEMENT_DTMF
		a->profile.Global_Options |= 0x8;
#endif				/* IMPLEMENT_DTMF */
		if (a->vscapi_mode == 0) {
			a->profile.Global_Options |= 0x80; /* Line Interconnect */
		}
#if IMPLEMENT_ECHO_CANCELLER
		a->profile.Global_Options |= 0x300;
#endif				/* IMPLEMENT_ECHO_CANCELLER */
		a->profile.B1_Protocols = 0x3df;
		a->profile.B2_Protocols = 0x1fdf;
		a->profile.B3_Protocols = 0xbf;
		a->manufacturer_features = MANUFACTURER_FEATURE_HARDDTMF;
	} else {
		a->profile.Global_Options = 0x71;
		if (card->d.features & DI_CODEC)
			a->profile.Global_Options |= 0x2;
		a->profile.B1_Protocols = 0x43;
		a->profile.B2_Protocols = 0x1f0f;
		a->profile.B3_Protocols = 0x0f;
		a->manufacturer_features = 0;
	}

	if (a->vscapi_mode == 0) {

	a->li_pri = card->PRI;
	a->li_channels = a->li_pri ?
#if IMPLEMENT_NULL_PLCI
		MIXER_TOTAL_CHANNELS_PRI : MIXER_TOTAL_CHANNELS_BRI;
#else /* IMPLEMENT_NULL_PLCI */
		MIXER_CHANNELS_PRI : MIXER_CHANNELS_BRI;
#endif /* IMPLEMENT_NULL_PLCI */
	a->li_base = 0;
	for (i = 0; &adapter[i] != a; i++) {
		if (adapter[i].request)
			a->li_base = adapter[i].li_base + adapter[i].li_channels;
	}
	k = li_total_channels + a->li_channels;
	new_li_config_table =
		(LI_CONFIG *) diva_os_malloc(0, ((k * sizeof(LI_CONFIG) + 3) & ~3) + (2 * k) * ((k + 3) & ~3));
	if (new_li_config_table == NULL) {
		DBG_ERR(("diva_add_card: failed alloc li_config table."))
		if (a->mtpx_cfg.cfg != 0) {
			diva_os_free (0, (void*)a->mtpx_cfg.cfg);
		}
		memset(a, 0, sizeof(DIVA_CAPI_ADAPTER));
		diva_os_free (0, info_sync_req);
		return (0);
	}

  /*
    Prevent accesses to line interconnect table in the
    process of update
    */
  diva_os_enter_spin_lock(&api_lock, &old_irql, "add card");

	j = 0;
	for (i = 0; i < k; i++) {
		if ((i >= a->li_base) && (i < a->li_base + a->li_channels))
			memset(&new_li_config_table[i], 0, sizeof(LI_CONFIG));
		else
			memcpy(&new_li_config_table[i], &li_config_table[j], sizeof(LI_CONFIG));
		new_li_config_table[i].flag_table =
			((byte *) new_li_config_table) + (((k * sizeof(LI_CONFIG) + 3) & ~3) + (2 * i) * ((k + 3) & ~3));
		new_li_config_table[i].coef_table =
			((byte *) new_li_config_table) + (((k * sizeof(LI_CONFIG) + 3) & ~3) + (2 * i + 1) * ((k + 3) & ~3));
		if ((i >= a->li_base) && (i < a->li_base + a->li_channels)) {
			new_li_config_table[i].adapter = a;
			memset(&new_li_config_table[i].flag_table[0], 0, k);
			memset(&new_li_config_table[i].coef_table[0], 0, k);
		} else {
			if (a->li_base != 0) {
				memcpy(&new_li_config_table[i].flag_table[0],
				       &li_config_table[j].flag_table[0],
				       a->li_base);
				memcpy(&new_li_config_table[i].coef_table[0],
				       &li_config_table[j].coef_table[0],
				       a->li_base);
			}
			memset(&new_li_config_table[i].flag_table[a->li_base], 0, a->li_channels);
			memset(&new_li_config_table[i].coef_table[a->li_base], 0, a->li_channels);
			if (a->li_base + a->li_channels < k) {
				memcpy(&new_li_config_table[i].flag_table[a->li_base +
				       a->li_channels],
				       &li_config_table[j].flag_table[a->li_base],
				       k - (a->li_base + a->li_channels));
				memcpy(&new_li_config_table[i].coef_table[a->li_base +
				       a->li_channels],
				       &li_config_table[j].coef_table[a->li_base],
				       k - (a->li_base + a->li_channels));
			}
			j++;
		}
	}

  li_total_channels = k;
  mem_to_free = li_config_table;

  li_config_table = new_li_config_table;
  for (i = card->Id; i < max_adapter; i++) {
    if (adapter[i].request)
      adapter[i].li_base += a->li_channels;
  }
#if IMPLEMENT_NULL_PLCI
  if (card->PRI)
  {
    a->null_plci_base = 32;
    a->null_plci_channels = 32;
  }
  else
  {
    a->null_plci_base = 5;
    a->null_plci_channels = 4;
  }
#endif /* IMPLEMENT_NULL_PLCI */

	} else {
		diva_os_enter_spin_lock(&api_lock, &old_irql, "add card");

		a->li_channels = 0;
		a->li_base = 0;
		a->null_plci_base = 0;
		a->null_plci_channels = 0;
	}

	if (a == &adapter[max_adapter])
		max_adapter++;

  diva_capi_adapter_count += (a->vscapi_mode == 0);

	card->next = cards;
	cards = card;

  AutomaticLaw(a);

	diva_os_leave_spin_lock(&api_lock, &old_irql, "add card");

  if (mem_to_free != 0) {
    diva_os_free (0, mem_to_free);
  }

	i = 0;
	while (i++ < 30) {
		if (a->automatic_law > 3)
			break;
		diva_os_sleep(10);
	}

	/* profile information */
	WRITE_WORD(&ctrl->profile.nbchannel, card->d.channels);
	ctrl->profile.goptions = a->profile.Global_Options;
	ctrl->profile.support1 = a->profile.B1_Protocols;
	ctrl->profile.support2 = a->profile.B2_Protocols;
	ctrl->profile.support3 = a->profile.B3_Protocols;
	/* manufacturer profile information */
	ctrl->profile.manu[0] = a->profile.Manuf.private_options;
	ctrl->profile.manu[1] = a->profile.Manuf.reserved_1;
	ctrl->profile.manu[2] = a->profile.Manuf.reserved_2;
	ctrl->profile.manu[3] = a->profile.Manuf.reserved_3;
	ctrl->profile.manu[4] = a->profile.Manuf.reserved_4;

	capi_ctr_ready(ctrl);

	DBG_TRC(("adapter added, max_adapter=%d", max_adapter));

	diva_os_free (0, info_sync_req);

	return (1);
}

/*
 *  register appl
 */
static void diva_register_appl(struct capi_ctr *ctrl, __u16 appl,
			       capi_register_params * rp)
{
  APPL *this;
  word bnum, xnum;
  int i = 0, j = 0;
  void * DataNCCI, * DataFlags, * ReceiveBuffer;
  struct xbuffer_element * xbuffer_table;
  void ** xbuffer_ptr, ** xbuffer_internal;
	diva_os_spin_lock_magic_t old_irql;

  if (diva_os_in_irq()) {
    DBG_ERR(("CAPI_REGISTER - in interrupt context !"))
    return;
  }

  diva_process_mem_release_q(0);

  DBG_TRC(("application register"))

  if(appl > MAX_APPL) {
    DBG_ERR(("CAPI_REGISTER - appl.Id exceeds MAX_APPL"))
    return;
  }

  if ((rp->level3cnt < 1) ||
      (rp->datablkcnt < 1) ||
      (rp->datablklen < 80) ||
      (rp->datablklen > 2150) ||
      (((dword)(((long) rp->level3cnt) * ((long) rp->datablkcnt))) >= 0xffffL) ||
      (((dword)(((long) rp->level3cnt) * ((long) MAX_DATA_B3))) >= 0xffffL))
  {
    DBG_ERR(("CAPI_REGISTER - invalid parameters"))
    return;
  }

  if (application[appl - 1].Id == appl) {
		DBG_LOG(("CAPI_REGISTER - appl already registered"))
    return; /* appl already registered */
  }

  /* alloc memory */

  bnum = rp->level3cnt * rp->datablkcnt;
  xnum = rp->level3cnt * MAX_DATA_B3;

  if (!(DataNCCI = diva_os_malloc(0, bnum * sizeof(word)))) {
    DBG_ERR(("CAPI_REGISTER - DataNCCI memory allocation failed"))
    return;
  }
  memset(DataNCCI, 0, bnum * sizeof(word));

  if (!(DataFlags = diva_os_malloc(0, bnum * sizeof(word)))) {
    DBG_ERR(("CAPI_REGISTER - DataFlags memory allocation failed"))
    diva_os_free (0, DataNCCI);
    return;
  }
  memset(DataFlags, 0, bnum * sizeof(word));

  if(!(ReceiveBuffer = diva_os_malloc(0, bnum * rp->datablklen))) {
    DBG_ERR(("CAPI_REGISTER - ReceiveBuffer memory allocation failed"))
    diva_os_free(0, DataNCCI);
    diva_os_free(0, DataFlags);
    return;
  }
  memset(ReceiveBuffer, 0, bnum * rp->datablklen);

  if (!(xbuffer_table = (struct xbuffer_element *) diva_os_malloc(0, (XBUFFER_ANCHOR_COUNT +
       xnum) * sizeof(struct xbuffer_element)))) {
    DBG_ERR(("CAPI_REGISTER - XbufferTable memory allocation failed"))
    diva_os_free(0, DataNCCI);
    diva_os_free(0, DataFlags);
    diva_os_free(0, ReceiveBuffer);
    return;
  }
  XBufferInit (xbuffer_table, xnum);

  if(!(xbuffer_ptr = (void **) diva_os_malloc(0, xnum * sizeof(void *)))) {
    DBG_ERR(("CAPI_REGISTER - XBuffer memory allocation failed"))
    diva_os_free(0, DataNCCI);
    diva_os_free(0, DataFlags);
    diva_os_free(0, ReceiveBuffer);
    diva_os_free(0, xbuffer_table);
    return;
  }
  memset(xbuffer_ptr, 0, xnum * sizeof(void *));

  if(!(xbuffer_internal = (void **) diva_os_malloc(0, xnum * sizeof(void *)))) {
    DBG_ERR(("CAPI_REGISTER - internal XBuffer memory allocation failed"))
    diva_os_free(0, DataNCCI);
    diva_os_free(0, DataFlags);
    diva_os_free(0, ReceiveBuffer);
    diva_os_free(0, xbuffer_table);
    diva_os_free(0, xbuffer_ptr);
    return;
  }
  memset(xbuffer_internal, 0, xnum * sizeof(void *));

  for ( i = 0 ; i < xnum ; i++ )
  {
    xbuffer_ptr[i] = diva_os_malloc(0, rp->datablklen);
    if ( !xbuffer_ptr[i] )
    {
      DBG_ERR(("CAPI_REGISTER - TransmitBuffer memory allocation failed"))
      if (i) {
        for(j = 0; j < i; j++)
          if(xbuffer_ptr[j])
            diva_os_free(0, xbuffer_ptr[j]);
      }
      diva_os_free(0, DataNCCI);
      diva_os_free(0, DataFlags);
      diva_os_free(0, ReceiveBuffer);
      diva_os_free(0, xbuffer_table);
      diva_os_free(0, xbuffer_ptr);
      diva_os_free(0, xbuffer_internal);
      return;
    }
  }

  DBG_LOG(("CAPI_REGISTER - Id = %d", appl))
  DBG_LOG(("  MaxLogicalConnections = %d", rp->level3cnt))
  DBG_LOG(("  MaxBDataBuffers       = %d", rp->datablkcnt))
  DBG_LOG(("  MaxBDataLength        = %d", rp->datablklen))

  /* initialize application data */

	diva_os_enter_spin_lock(&api_lock, &old_irql, "register_appl");

  this = &application[appl - 1] ;
  memset(this, 0, sizeof (APPL));

  this->Id = appl;

  for (i = 0; i < max_adapter; i++)
  {
    adapter[i].CIP_Mask[appl - 1] = 0;
  }

  this->queue_size = 1000;

  this->MaxNCCI = rp->level3cnt;
  this->MaxNCCIData = rp->datablkcnt;
  this->MaxBuffer = bnum;
  this->MaxDataLength = rp->datablklen;

  this->DataNCCI         = DataNCCI;
  this->DataFlags        = DataFlags;
  this->ReceiveBuffer    = ReceiveBuffer;
  this->xbuffer_table    = xbuffer_table;
  this->xbuffer_ptr      = xbuffer_ptr;
  this->xbuffer_internal = xbuffer_internal;
  for ( i = 0 ; i < xnum ; i++ ) {
    this->xbuffer_ptr[i] = xbuffer_ptr[i];
  }

  CapiRegister(this->Id);

	diva_os_leave_spin_lock(&api_lock, &old_irql, "register_appl");
}

/*
 *  release appl
 */
static void diva_release_appl(struct capi_ctr *ctrl, __u16 appl)
{
	diva_os_spin_lock_magic_t old_irql;
  APPL *this = &application[appl - 1] ;
  void* DataNCCI = 0, *DataFlags = 0, *ReceiveBuffer = 0, **xbuffer_internal = 0;
  void* xbuffer_table = 0;
  word MaxNCCI = 0;
  int i = 0;
  void**  xbuffer_ptr = 0;
  diva_mem_release_t* mem_release;

	if (diva_os_in_irq()) {
    DBG_ERR(("CAPI_RELEASE - in interrupt context !"))
    return;
  }

  diva_process_mem_release_q(0);

	diva_os_enter_spin_lock(&api_lock, &old_irql, "release_appl");
  if (this->Id)
  {
    DBG_TRC(("application %d cleanup", this->Id))

    CapiRelease(this->Id);
    DataNCCI  = this->DataNCCI;
    this->DataNCCI = 0;
    DataFlags = this->DataFlags;
    this->DataFlags = 0;
    ReceiveBuffer = this->ReceiveBuffer;
    this->ReceiveBuffer = 0;
    if (this->xbuffer_ptr)
    {
      MaxNCCI = this->MaxNCCI;
      xbuffer_ptr = this->xbuffer_ptr;
      this->xbuffer_ptr = 0;
    }
    xbuffer_internal = this->xbuffer_internal;
    this->xbuffer_internal = 0;
    xbuffer_table = this->xbuffer_table;
    this->xbuffer_table = 0;
    this->Id = 0;
  }
	diva_os_leave_spin_lock(&api_lock, &old_irql, "release_appl");

  if ((mem_release = diva_os_malloc (0, sizeof(*mem_release))) != 0)
  {
    memset (mem_release, 0x00, sizeof(mem_release));
    mem_release->t         = jiffies;
    mem_release->DataNCCI  = DataNCCI;
    mem_release->DataFlags = DataFlags;
    mem_release->ReceiveBuffer = ReceiveBuffer;
    mem_release->xbuffer_internal = xbuffer_internal;
    mem_release->xbuffer_table = xbuffer_table;
    mem_release->num_data      = MAX_DATA_B3 * MaxNCCI;
    mem_release->xbuffer_ptr = xbuffer_ptr;

    diva_os_enter_spin_lock(&mem_q_lock, &old_irql, "release_appl_mem");
    diva_q_add_tail (&mem_release_q, &mem_release->link);
    diva_os_leave_spin_lock(&mem_q_lock, &old_irql, "release_appl_mem");

  }
  else
  {
    if (DataNCCI)
      diva_os_free (0, DataNCCI);
  	if (DataFlags)
      diva_os_free (0, DataFlags);
    if (ReceiveBuffer)
      diva_os_free (0, ReceiveBuffer);
    if (xbuffer_internal)
      diva_os_free (0, xbuffer_internal);
    if (xbuffer_table) {
      diva_os_free (0, xbuffer_table);
    }

    if (xbuffer_ptr)
    {
      for (i = 0; i < (MAX_DATA_B3 * MaxNCCI); i++)
        if (xbuffer_ptr[i]) diva_os_free(0, xbuffer_ptr[i]);

      diva_os_free(0, xbuffer_ptr);
    }
  }
}

/*
 *  send message
 */
#if defined(DIVA_EICON_CAPI)
u16 diva_capi20_send_message(struct capi_ctr *ctrl, CAPI_MSG* msg, dword length)
#else
static u16 diva_send_message(struct capi_ctr *ctrl, diva_os_message_buffer_s * dmb)
#endif
{
  int i = 0, j = 0;
	diva_os_spin_lock_magic_t old_irql;
#if defined(DIVA_EICON_CAPI)
	CAPI_MSG* mapped_msg = msg;
#else
	CAPI_MSG *msg = (CAPI_MSG*)DIVA_MESSAGE_BUFFER_DATA(dmb);
	__u32 length = DIVA_MESSAGE_BUFFER_LEN(dmb);
#endif
  APPL *this = &application[msg->header.appl_id - 1];
  diva_card *card = ctrl->driverdata;
	word clength = READ_WORD(&msg->header.length);
	word command = READ_WORD(&msg->header.command);
  word DataLength = 0;

	if (diva_os_in_irq()) {
    DBG_ERR(("CAPI_SEND_MSG - in interrupt context !"))
    return CAPI_REGOSRESOURCEERR;
  }

	diva_os_enter_spin_lock(&api_lock, &old_irql, "send message");

  if (card->remove_in_progress) {
	  diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
    DBG_ERR(("CAPI_SEND_MSG - remove in progress!"))
    return CAPI_REGOSRESOURCEERR;
  }

	if (!this->Id) {
    diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
    DBG_ERR(("CAPI_SEND_MSG - wrong application Id"))
		return CAPI_ILLAPPNR;
	}

  DBG_PRV1(("Write - appl = %d, cmd = 0x%x", this->Id, command))

  /* patch controller number */
  msg->header.controller = ControllerMap[card->Id]
                           | (msg->header.controller &0x80);  /* preserve external controller bit */

  switch (command)
  {
    default:
      xlog ("\x00\x02", msg, 0x80, clength);
      break;

    case _DATA_B3_I|RESPONSE:
      if ( myDriverDebugHandle.dbgMask & DL_BLK )
        xlog ("\x00\x02", msg, 0x80, clength);
      break;

    case _DATA_B3_R:
      if ( myDriverDebugHandle.dbgMask & DL_BLK )
        xlog ("\x00\x02", msg, 0x80, clength);

      if (clength == 24) {
        /*
           Workaround for PPcom bug, header is always 22 bytes long
           */
        clength = 22;
      }
      DataLength = READ_WORD(&msg->info.data_b3_req.Data_Length);
      if (DataLength > this->MaxDataLength) 
      {
        diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
        DBG_ERR(("Write - invalid message size %d, maximum %d", DataLength, this->MaxDataLength))
        return CAPI_ILLCMDORSUBCMDORMSGTOSMALL;
      }

      if (DataLength > (length - clength))
      {
        diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
        DBG_ERR(("Write - invalid message size %d, expected %d", length, DataLength + clength))
        return CAPI_ILLCMDORSUBCMDORMSGTOSMALL;
      }

      if ((i = XBufferNext (this->xbuffer_table)) < 0)
      {
        diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
        DBG_ERR(("Write - too many data pending"))
        return CAPI_SENDQUEUEFULL;
      }
      msg->info.data_b3_req.Data = i;
      this->xbuffer_internal[i] = NULL;
      memcpy (this->xbuffer_ptr[i], &((__u8 *)msg)[clength], DataLength);

      if ( (myDriverDebugHandle.dbgMask & DL_BLK)
        && (myDriverDebugHandle.dbgMask & DL_XLOG))
      {
        for ( j = 0 ; j < DataLength; j += 256 )
        {
          DBG_BLK((((char *)this->xbuffer_ptr[i]) + j,
            ((DataLength - j) < 256) ? (DataLength - j) : 256))
          if ( !(myDriverDebugHandle.dbgMask & DL_PRV0) )
            break ; // not more if not explicitely requested
        }
      }
      break ;
  }

#if !defined(DIVA_EICON_CAPI)
  memcpy (mapped_msg, msg, clength);
#endif
  mapped_msg->header.controller = MapController (mapped_msg->header.controller);
	mapped_msg->header.length = clength;

  switch (api_put (this, mapped_msg))
  {
    case 0:
      break;

    case _BAD_MSG:
      diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
      DBG_ERR(("Write - bad message"))
      return CAPI_ILLCMDORSUBCMDORMSGTOSMALL;

    case _QUEUE_FULL:
      diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
      DBG_ERR(("Write - queue full"))
      return CAPI_SENDQUEUEFULL;

    default:
      diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");
      DBG_ERR(("Write - api_put returned unknown error"))
      return CAPI_UNKNOWNNOTPAR;
  }

	diva_os_leave_spin_lock(&api_lock, &old_irql, "send message");

#if !defined(DIVA_EICON_CAPI)
	diva_os_free_message_buffer(dmb);
#endif

  return (CAPI_NOERROR);
}


/*
 * cards request function
 */
static void DIRequest(ENTITY * e)
{
	DIVA_CAPI_ADAPTER *a = &(adapter[(byte) e->user[0]]);
	diva_card *os_card = (diva_card *) a->os_card;

	if (e->Req && (a->FlowControlIdTable[e->ReqCh] == e->Id)) {
		a->FlowControlSkipTable[e->ReqCh] = 1;
	}

	(*(os_card->d.request)) (e);
}

/*
 * callback function from didd
 */
static void didd_callback(void *context, DESCRIPTOR * adapter, int removal)
{
	if (adapter->type == IDI_DADAPTER) {
		DBG_ERR(("Notification about IDI_DADAPTER change ! Oops."));
		return;
	} else if (adapter->type == IDI_DIMAINT) {
		if (removal) {
			stop_dbg();
		} else {
			memcpy(&MAdapter, adapter, sizeof(MAdapter));
			dprintf = (DIVA_DI_PRINTF) MAdapter.request;
			DbgRegister("CAPI20", DRIVERRELEASE_CAPI, DBG_DEFAULT);
		}
    return;
	}

  if (diva_capi_uses_madapter != 0) {
    if (adapter->type == IDI_MADAPTER) {
      if (removal == 0) {
        DBG_LOG(("Ignore MADAPTER after notification"));
        /* diva_add_card(adapter); */
      } else {
        DBG_ERR(("Notification about IDI_MADAPTER change ! Oops."));
        return;
      }
    }
  } else {
    if ((adapter->type > 0) && (adapter->type < 16)) {	/* IDI Adapter */
      if (removal == 0) {
        diva_add_card(adapter);
      } else {
        divacapi_remove_card(adapter);
      }
    }
  }
}

/*
 * connect to didd
 */
static int divacapi_connect_didd(void)
{
	int x = 0;
	IDI_SYNC_REQ req;
	static DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];

	memset (DIDD_Table, 0x00, sizeof(DIDD_Table));
	DIVA_DIDD_Read(DIDD_Table, sizeof(DIDD_Table));

  /*
    Locate DADAPTER
    */
	for (x = 0; x < MAX_DESCRIPTORS; x++) {
		if (DIDD_Table[x].type == IDI_DADAPTER) {	/* DADAPTER found */
			memcpy(&DAdapter, &DIDD_Table[x], sizeof(DAdapter));
			break;
		}
	}

	if (DAdapter.request == 0) {
		return (0);
	}

  /*
    Register with MAINT driver
    */
	for (x = 0; x < MAX_DESCRIPTORS; x++) {
		if (DIDD_Table[x].type == IDI_DIMAINT) {	/* MAINT found */
			memcpy(&MAdapter, &DIDD_Table[x], sizeof(MAdapter));
			dprintf = (DIVA_DI_PRINTF) MAdapter.request;
			DbgRegister("CAPI20", DRIVERRELEASE_CAPI, DBG_DEFAULT);
			break;
		}
	}

  diva_capi_uses_madapter = 0;

  /*
    First check for MTPX adapters
    */
  for (x = 0; x < MAX_DESCRIPTORS; x++) {
    if (DIDD_Table[x].type == IDI_MADAPTER) {
      diva_capi_uses_madapter = 1;
      DBG_LOG(("Add MADAPTER"))
      diva_add_card(&DIDD_Table[x]);
    }
  }

  /*
    Check for XDI adapters only if no MTPX adapter
    was found
    */
  if (diva_capi_uses_madapter == 0) {
    for (x = 0; x < MAX_DESCRIPTORS; x++) {
      if ((DIDD_Table[x].type > 0) && (DIDD_Table[x].type < 16)) {
        /*
           IDI Adapter found
           */
        DBG_LOG(("Add XDI adapter"))
        diva_add_card(&DIDD_Table[x]);
      }
    }
  }

  /*
    Finally register adapter insertion/removal notification
    */
	req.didd_notify.e.Req = 0;
	req.didd_notify.e.Rc =
	    IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY;
	req.didd_notify.info.callback = (void *)didd_callback;
	req.didd_notify.info.context = 0;
	DAdapter.request((ENTITY *) & req);
	if (req.didd_notify.e.Rc != 0xff) {
		stop_dbg();
		return (0);
	}
	notify_handle = req.didd_notify.info.handle;

	return (1);
}

/*
 * diconnect from didd
 */
static void divacapi_disconnect_didd(void)
{
	IDI_SYNC_REQ req;

	stop_dbg();

	req.didd_notify.e.Req = 0;
	req.didd_notify.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY;
	req.didd_notify.info.handle = notify_handle;
	DAdapter.request((ENTITY *) & req);
}

/*
 * we do not provide date/time here,
 * the application should do this.
 */
int fax_head_line_time(char *buffer)
{
	return (0);
}

/*
 * init (alloc) main structures
 */
static int DIVA_INIT_FUNCTION init_main_structs(void)
{
	memset(ControllerMap, 0, MAX_ADAPTER + 1);
	max_adapter = 0;

#if !defined(DIVA_EICON_CAPI)
	if (!(mapped_msg = (CAPI_MSG *) diva_os_malloc(0, MAX_MSG_SIZE))) {
		DBG_ERR(("init: failed alloc mapped_msg."))
    return 0;
	}
#endif

	if (!(adapter = diva_os_malloc(0, sizeof(DIVA_CAPI_ADAPTER) * MAX_ADAPTER))) {
		DBG_ERR(("init: failed alloc adapter struct."))
#if !defined(DIVA_EICON_CAPI)
		diva_os_free(0, mapped_msg);
#endif
		return 0;
	}
	memset(adapter, 0, sizeof(DIVA_CAPI_ADAPTER) * MAX_ADAPTER);

	if (!(application = diva_os_malloc(0, sizeof(APPL) * MAX_APPL))) {
		DBG_ERR(("init: failed alloc application struct."))
#if !defined(DIVA_EICON_CAPI)
		diva_os_free(0, mapped_msg);
#endif
		diva_os_free(0, adapter);
		return 0;
	}
	memset(application, 0, sizeof(APPL) * MAX_APPL);

#ifdef C_IND_RESOURCE_INDEX_ENTRIES
	if (!(c_ind_resource_index = diva_os_malloc(0, sizeof(struct resource_index_entry) * C_IND_RESOURCE_INDEX_ENTRIES))) {
		DBG_ERR(("init: failed alloc resource index struct."))
#if !defined(DIVA_EICON_CAPI)
		diva_os_free(0, mapped_msg);
#endif
		diva_os_free(0, adapter);
		diva_os_free(0, application);
		return 0;
	}
	memset(c_ind_resource_index, 0, sizeof(struct resource_index_entry) * C_IND_RESOURCE_INDEX_ENTRIES);
#endif

	return (1);
}

/*
 * remove (free) main structures
 */
static void remove_main_structs(void)
{
	if (c_ind_resource_index)
		diva_os_free(0, c_ind_resource_index);
	if (application)
		diva_os_free(0, application);
	if (adapter)
		diva_os_free(0, adapter);
#if !defined(DIVA_EICON_CAPI)
	if (mapped_msg)
		diva_os_free(0, mapped_msg);
#endif
}

/*
 * init
 */
int DIVA_INIT_FUNCTION init_capifunc(void)
{
	diva_os_initialize_spin_lock(&api_lock, "capifunc");
	diva_os_initialize_spin_lock(&mem_q_lock, "capifunc");

	if (!init_main_structs()) {
		DBG_ERR(("init: failed to init main structs."))
		return (0);
	}

	if (!divacapi_connect_didd()) {
	  int ret, count = 100;
		DBG_ERR(("init: failed to connect to DIDD."))

    do {
      diva_os_spin_lock_magic_t old_irql;

      diva_os_enter_spin_lock(&api_lock, &old_irql, "api remove start");
		  ret = api_remove_start();
      diva_os_leave_spin_lock(&api_lock, &old_irql, "api remove start");

		  diva_os_sleep(10);
    } while (ret && count--);

    divacapi_remove_cards();
		remove_main_structs();

		return (0);
	}

  if (diva_start_management (1000)) {
    DBG_ERR(("Failed to start CAPI Management Interface"))
  }

	return (1);
}

/*
 * finit
 */
void DIVA_EXIT_FUNCTION finit_capifunc(void)
{
	int ret, count = 100;

  do {
    diva_os_spin_lock_magic_t old_irql;

    diva_os_enter_spin_lock(&api_lock, &old_irql, "api remove start");
    ret = api_remove_start();
    diva_os_leave_spin_lock(&api_lock, &old_irql, "api remove start");

    diva_os_sleep(10);
  } while (ret && count--);

	if (ret)
		DBG_ERR(("could not remove signaling ID's"))

  diva_stop_management ();
  divacapi_disconnect_didd();
  divacapi_remove_cards();

  diva_process_mem_release_q(1);
	remove_main_structs();

	diva_os_destroy_spin_lock(&mem_q_lock, "capifunc");
	diva_os_destroy_spin_lock(&api_lock, "capifunc");
}

void diva_os_api_lock   (diva_os_spin_lock_magic_t* old_irql) {
	diva_os_enter_spin_lock(&api_lock, old_irql, "capi_man");
}

void diva_os_api_unlock (diva_os_spin_lock_magic_t* old_irql) {
	diva_os_leave_spin_lock(&api_lock, old_irql, "capi_man");
}

const DIVA_CAPI_ADAPTER* diva_os_get_adapter (int nr) {
  DIVA_CAPI_ADAPTER* ret = 0;
  diva_card *p;

  p = cards;
  while (p && nr--) {
    p = p->next;
  }

  ret = p ? p->adapter : 0;

  return (ret);
}

void diva_os_add_adapter_descriptor (DESCRIPTOR* d, int operation) {
  IDI_SYNC_REQ req;

  if (operation) { /* Add    */
    req.didd_remove_adapter.e.Req = 0;
    req.didd_add_adapter.e.Rc = IDI_SYNC_REQ_DIDD_ADD_ADAPTER;
    req.didd_add_adapter.info.descriptor = (void *)d;
    DAdapter.request((ENTITY *)&req);
  } else {         /* Remove */
    req.didd_remove_adapter.e.Req = 0;
    req.didd_remove_adapter.e.Rc = IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER;
    req.didd_remove_adapter.info.p_request = d->request;
    DAdapter.request((ENTITY *)&req);
  }
}

static int diva_capi_read_cfg_value (IDI_SYNC_REQ* sync_req,
                                     int instance,
                                     const char* name,
                                     diva_cfg_lib_value_type_t value_type,
                                     void* data,
                                     dword data_length)
{
  diva_cfg_lib_return_type_t ret = DivaCfgLibNotFound;

  memset (sync_req, 0x00, sizeof(*sync_req));
  sync_req->didd_get_cfg_lib_ifc.e.Rc = IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC;
  DAdapter.request((ENTITY *)sync_req);

  if (sync_req->didd_get_cfg_lib_ifc.e.Rc == 0xff)
  {
    diva_cfg_lib_access_ifc_t* cfg_lib_ifc = sync_req->didd_get_cfg_lib_ifc.info.ifc;

    if (cfg_lib_ifc != 0)
    {
      ret = cfg_lib_ifc->diva_cfg_storage_read_cfg_var_proc(cfg_lib_ifc,
                           TargetCapiDrv, instance, 0, 0, name,
                           value_type, &data_length, data);

      (*(cfg_lib_ifc->diva_cfg_lib_free_cfg_interface_proc))(cfg_lib_ifc);

      if (ret == DivaCfgLibOK) {
        DBG_LOG(("CAPI(%d) read %s configuration value", instance, name))
      } else {
        DBG_ERR(("CAPI(%d) failed to read %s configuration value", instance, name))
      }
    }
  }

  return ((ret == DivaCfgLibOK) ? 0 : -1);
}

static void diva_process_mem_release_q(int force)
{
	diva_os_spin_lock_magic_t old_irql;
  diva_mem_release_t* mem_release;
  diva_entity_queue_t q;

  diva_q_init (&q);

  diva_os_enter_spin_lock(&mem_q_lock, &old_irql, "release_appl_mem");

  mem_release = (diva_mem_release_t*)diva_q_get_head(&mem_release_q);

  while (mem_release != 0)
  {
    if (force != 0 || time_after(jiffies, mem_release->t + 2*HZ))
    {
      diva_mem_release_t* next = (diva_mem_release_t*)diva_q_get_next(&mem_release->link);
      diva_q_remove(&mem_release_q, &mem_release->link);
      diva_q_add_tail(&q, &mem_release->link);
      mem_release = next;
    }
    else
    {
      mem_release = (diva_mem_release_t*)diva_q_get_next(&mem_release->link);
    }
  }

  diva_os_leave_spin_lock(&mem_q_lock, &old_irql, "release_appl_mem");

  while ((mem_release = (diva_mem_release_t*)diva_q_get_head(&q)) != 0)
  {
    diva_q_remove (&q, &mem_release->link);

    if (mem_release->DataNCCI != 0)
      diva_os_free (0, mem_release->DataNCCI);
    if (mem_release->DataFlags != 0)
      diva_os_free (0, mem_release->DataFlags);
    if (mem_release->ReceiveBuffer != 0)
      diva_os_free (0, mem_release->ReceiveBuffer);
    if (mem_release->xbuffer_internal != 0)
      diva_os_free (0, mem_release->xbuffer_internal);
    if (mem_release->xbuffer_table != 0)
      diva_os_free (0, mem_release->xbuffer_table);

    if (mem_release->xbuffer_ptr != 0)
    {
      int i;

      for (i = 0; i < mem_release->num_data; i++)
        if (mem_release->xbuffer_ptr[i])
          diva_os_free(0, mem_release->xbuffer_ptr[i]);

      diva_os_free (0, mem_release->xbuffer_ptr);
    }

    diva_os_free (0, mem_release);
  }
}


#if defined(DIVA_EICON_CAPI) /* { */
u16 diva_capi20_get_manufacturer (u32 contr, u8 *buf) {
	diva_os_spin_lock_magic_t old_irql;
	int found;

	if (contr == 0) {
		strlcpy(buf, M_COMPANY, CAPI_MANUFACTURER_LEN);
		return (CAPI_NOERROR);
	}

	diva_os_enter_spin_lock(&api_lock, &old_irql, "manufacturer");
	found = find_card_by_ctrl(contr & 0x7f) != 0;
	diva_os_leave_spin_lock(&api_lock, &old_irql, "manufacturer");

	if (found != 0) {
		strlcpy(buf, M_COMPANY, CAPI_MANUFACTURER_LEN);
		return (CAPI_NOERROR);
	}

	return (CAPI_REGNOTINSTALLED);
}

u16 diva_capi20_get_profile (u32 contr, struct capi_profile *profile) {
	diva_os_spin_lock_magic_t old_irql;
	diva_card *card;

	diva_os_enter_spin_lock(&api_lock, &old_irql, "profile");
	if ((card = find_card_by_ctrl(contr & 0x7f)) != 0 && card->adapter != 0) {
		DIVA_CAPI_ADAPTER* a = card->adapter;


		/*
			Write profile information
			*/
		WRITE_WORD(&profile->nbchannel, card->d.channels);
		profile->goptions = a->profile.Global_Options;
		profile->support1 = a->profile.B1_Protocols;
		profile->support2 = a->profile.B2_Protocols;
		profile->support3 = a->profile.B3_Protocols;

		/*
			Manufacturer profile information
			*/
		profile->manu[0] = a->profile.Manuf.private_options;
		profile->manu[1] = a->profile.Manuf.reserved_1;
		profile->manu[2] = a->profile.Manuf.reserved_2;
		profile->manu[3] = a->profile.Manuf.reserved_3;
		profile->manu[4] = a->profile.Manuf.reserved_4;
	}
	diva_os_leave_spin_lock(&api_lock, &old_irql, "profile");

	return ((card != 0) ? CAPI_NOERROR : CAPI_REGNOTINSTALLED);
}

u16 diva_capi20_get_serial(u32 contr, u8 *serial) {
	diva_os_spin_lock_magic_t old_irql;
	diva_card *card;

	if (contr == 0) {
		strlcpy(serial, "0004711", CAPI_SERIAL_LEN);
		return CAPI_NOERROR;
	}

	diva_os_enter_spin_lock(&api_lock, &old_irql, "profile");
	if ((card = find_card_by_ctrl(contr & 0x7f)) != 0) {
		memcpy (serial, card->serial, sizeof(card->serial));
	}
	diva_os_leave_spin_lock(&api_lock, &old_irql, "profile");

	return ((card != 0) ? CAPI_NOERROR : CAPI_REGNOTINSTALLED);
}

u16 diva_capi20_get_version(u32 contr, struct capi_version* version) {
	diva_os_spin_lock_magic_t old_irql;
	diva_card *card;

	if (contr == 0) {
		version->majorversion = 2;
		version->minorversion = 0;
		version->majormanuversion = DRRELMAJOR;
		version->minormanuversion = DRRELMINOR;
		return (CAPI_NOERROR);
	}

	diva_os_enter_spin_lock(&api_lock, &old_irql, "profile");
	if ((card = find_card_by_ctrl(contr & 0x7f)) != 0) {
		version->majorversion = 2;
		version->minorversion = 0;
		version->majormanuversion = DRRELMAJOR;
		version->minormanuversion = DRRELMINOR;
	}
	diva_os_leave_spin_lock(&api_lock, &old_irql, "profile");

	return ((card != 0) ? CAPI_NOERROR : CAPI_REGNOTINSTALLED);
}


u16 diva_capi20_get_controller_count (void) {
	diva_os_spin_lock_magic_t old_irql;
	u16 ret;

	diva_os_enter_spin_lock(&api_lock, &old_irql, "controller_count");
	ret = diva_capi_adapter_count;
	diva_os_leave_spin_lock(&api_lock, &old_irql, "controller_count");

	return (ret);
}











#endif /* } */

