
/*
 *
  Copyright (c) Dialogic(R), 2009.
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

#include "platform.h"
#include "debug.h"
#include "di_defs.h"
#include "pc.h"
#include "divasync.h"
#include "manage.h"
#include "man_defs.h"
#include "drv_man.h"

extern void diva_os_add_adapter_descriptor (DESCRIPTOR* d, int operation);
extern MAN_INFO* drv_man_get_driver_root (void);
const char* drv_man_get_driver_name (void);

#define DIVA_MAN_DRV_ASSIGN_PENDING        1
#define DIVA_MAN_DRV_REQ_PENDING           2
#define DIVA_MAN_DRV_ASSIGN_FAILED_PENDING 3

static diva_driver_management_t diva_man_drv;

static void diva_management_request (ENTITY* e);
static void diva_man_drv_dpc (struct _diva_os_soft_isr* psoft_isr, void* Context);
static void diva_man_drv_process_request (diva_driver_management_t* pM, int request_index);

static void ComWRead (byte * ptr, byte far *in, word bloc, word maxlen);
static word ComParse(byte* req_data, int req_length, int argc,...);
static int diva_man_add_ie (byte *header, byte *value, void *context);
static void man_usr_process_req (byte Req, diva_driver_management_t* pE, ENTITY* e);
static int diva_copy_xdescr (byte* dst, const ENTITY* e, int max_length);
static void diva_copy_ind_to_user (ENTITY* e, byte* src, int length);
static void man_usr_delivery_ind (unsigned int message,
                                  diva_driver_management_t* pE,
                                  unsigned int buffer_idx,
																	ENTITY* e);
static void man_fill_ind_count_element(diva_driver_management_t* pE,
                                       unsigned int buffer_idx,
                                       unsigned int value);

#define PARSE_ESC           0x0800
#define PARSE_CLEAR         0x1000

/*
	INTERFACE: start driver management interface
	*/
int diva_start_management (int logical_adapter_nr) {
	diva_driver_management_t* pM = &diva_man_drv;
	DESCRIPTOR d;

	if (pM->initialized) {
		return (-1);
	}

	memset (pM, 0x00, sizeof(*pM));
	pM->initialized        = 1;
	pM->logical_adapter_nr = logical_adapter_nr;

  if (diva_os_initialize_spin_lock (&pM->req_spin_lock, "req")) {
		pM->initialized = 0;
		return (-1);
	}

	strcpy (pM->req_soft_isr.dpc_thread_name, "kdrvmd");
  if (diva_os_initialize_soft_isr (&pM->req_soft_isr, diva_man_drv_dpc, pM)) {
		diva_os_destroy_spin_lock (&pM->req_spin_lock, "rm");
		pM->initialized = 0;
		return (-1);
	}

	d.channels = 0;
	d.type     = IDI_DRIVER;
	d.features = DI_MANAGE;
	d.request  = diva_management_request;
	diva_os_add_adapter_descriptor (&d, 1);

	return (0);
}

/*
	INTERFACE: stop driver management interface
	*/
void diva_stop_management (void) {
	diva_driver_management_t* pM = &diva_man_drv;
	DESCRIPTOR d;

	if (pM->initialized) {
		diva_os_remove_soft_isr   (&pM->req_soft_isr);
		diva_os_destroy_spin_lock (&pM->req_spin_lock, "rm");

		d.channels = 0;
		d.type     = IDI_DRIVER;
		d.features = DI_MANAGE;
		d.request  = diva_management_request;
		diva_os_add_adapter_descriptor (&d, 0);

		pM->initialized = 0;
	}
}

static void diva_management_request (ENTITY* e) {
	diva_driver_management_t* pM = &diva_man_drv;
	diva_os_spin_lock_magic_t irql;
	int i, free_entity = -1;

	if (!e->Req) { /* synchronous request */
		IDI_SYNC_REQ *syncReq = (IDI_SYNC_REQ *)e ;
  	switch (e->Rc) {
			case IDI_SYNC_REQ_GET_NAME:
				strcpy ((char*)&syncReq->GetName.name[0], drv_man_get_driver_name());
				return;
			case IDI_SYNC_REQ_GET_SERIAL:
				syncReq->GetSerial.serial = 202;
				return;
			case IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER: {
				diva_xdi_get_logical_adapter_number_s_t *pI = &syncReq->xdi_logical_adapter_number.info;
				pI->logical_adapter_number = pM->logical_adapter_nr;
				pI->controller             = 0;
				pI->total_controllers      = 0;
			} return;
			case IDI_SYNC_REQ_GET_FEATURES: {
				syncReq->GetFeatures.features = DI_MANAGE;
			} return;
		}
		return;
	}

	diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_req");

	for (i = 0; i < DIVA_DRIVER_MANAGEMENT_MAX_ENTITIES;  i++) {
		if (pM->entities[i].e == e) {
			if (pM->entities[i].req_pending == 0) {
				pM->entities[i].req_pending = DIVA_MAN_DRV_REQ_PENDING;
				diva_os_schedule_soft_isr (&pM->req_soft_isr);
			} else {
				DBG_FTL(("M-REQ: ignore request e:%p Id:%02x Req:%02x", e, e->Id, e->Req))
			}
			diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_req");
			return;
		} else if (pM->entities[i].e == 0) {
			free_entity = i;
		}
	}

	if (free_entity < 0) {
		DBG_ERR(("M-REQ all Id in use, assign failed, e:%p Id:%02x Req:%02x", e, e->Id, e->Req))
		if (pM->wrong_e != 0)
			pM->wrong_e = e;
	} else {
		pM->entities[free_entity].e = e;
		if (e->Id == MAN_ID && e->Req == ASSIGN) { /* Assign request */
			pM->entities[free_entity].req_pending = DIVA_MAN_DRV_ASSIGN_PENDING;
		} else if (e->Id == DSIG_ID && e->Req == ASSIGN) { /* Illegal Assign request */
			/* There is an old driver, which calls this ASSIGN request as long as
			 *  no failure is returned, but the request always fails...
			 * The resource hangs, but it's better than an endless loop.
			 */
			DBG_FTL(("M-REQ: ignore illegal sig request, e:%p", e))
			pM->entities[free_entity].e = 0;
			diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_req");
			return ;
		} else {
			pM->entities[free_entity].req_pending = DIVA_MAN_DRV_ASSIGN_FAILED_PENDING;
			DBG_ERR(("M-ASSIGN failed Id:%02x, Req:%02x", e->Id, e->Req))
		}
	}

	diva_os_schedule_soft_isr (&pM->req_soft_isr);
	diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_req");
}

/*
	Management interface DPC routine
	*/
static void diva_man_drv_dpc (struct _diva_os_soft_isr* psoft_isr, void* Context) {
	diva_driver_management_t* pM = &diva_man_drv;
	diva_os_spin_lock_magic_t irql;
	ENTITY* e;
	int i, one_processed;


	do {
		one_processed = 0;

		diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
		e = pM->wrong_e;
		pM->wrong_e = 0;
		diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");

		if (e != 0) {
			one_processed = 1;
			e->complete = 0xff;
			e->Rc = (e->Id & 0x1f) ? OUT_OF_RESOURCES : (ASSIGN_RC | OUT_OF_RESOURCES);
			(*(e->callback))(e);
		}

		for (i = 0; i < DIVA_DRIVER_MANAGEMENT_MAX_ENTITIES;  i++) {
			diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			e = pM->entities[i].req_pending != 0 ? pM->entities[i].e : 0;
			diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			if (e != 0) {
				one_processed = 0;
				diva_man_drv_process_request (pM, i);
			}
		}
	} while (one_processed != 0);
}

static void diva_man_drv_process_request (diva_driver_management_t* pM, int request_index) {
	diva_os_spin_lock_magic_t irql;
	ENTITY* e;
	int req_type;

	diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
	e        = pM->entities[request_index].e;
	req_type = pM->entities[request_index].req_pending;
	pM->entities[request_index].req_pending = 0;
	diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");

	if (e == 0 || req_type == 0) {
		return;
	}

	switch(req_type) {
		case DIVA_MAN_DRV_ASSIGN_PENDING:
			pM->entities[request_index].chain_ind = 0;
			pM->entities[request_index].nohide    = 0;
      if (e->XNum) {  // set capability of entity
        byte lli[12];
        ComParse (e->X[0].P, e->X[0].PLength,
                  1, LLI | PARSE_CLEAR, lli, sizeof(lli));
			  if (lli[1] &1) pM->entities[request_index].chain_ind = 1;
			  if (lli[1] &2) pM->entities[request_index].nohide    = 1;
      }
			e->complete = 0xff;
			e->Id = 2 + pM->assignId++;
			e->Rc = ASSIGN_OK;
			(*(e->callback))(e);
			return;
		case DIVA_MAN_DRV_ASSIGN_FAILED_PENDING:
			diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			pM->entities[request_index].e = 0;
			diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			e->complete = 0xff;
			e->Rc = (ASSIGN_RC | OUT_OF_RESOURCES);
			(*(e->callback))(e);
			return;
	}

	pM->chain_ind = pM->entities[request_index].chain_ind;
	pM->nohide    = pM->entities[request_index].nohide;

	switch (e->Req) {
		case MAN_READ:
		case MAN_WRITE:
		case MAN_EXECUTE:
			man_usr_process_req (e->Req, pM, pM->entities[request_index].e);
			return;
		case REMOVE: {
			diva_os_enter_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			pM->entities[request_index].e = 0;
			diva_os_leave_spin_lock (&pM->req_spin_lock, &irql, "man_dpc");
			e->complete = 0xff;
			e->Id = 0;
			e->Rc = OK;
			(*(e->callback))(e);
			pM->assignId--;
		} return;
	}

	e->complete = 0xff;
	e->Rc = WRONG_COMMAND;
	(*(e->callback))(e);
}


static void ComWRead (byte * ptr, byte far *in, word bloc, word maxlen) {
  word i;
  word n;

  bloc++;
  n = in[bloc]+1;
  n = (n>maxlen) ? maxlen : n;
  for(i=0;i<n;i++) ptr[i] = in[bloc+i];
  ptr[0] = (byte)(n-1);
}

/*------------------------------------------------------------------*/
/* Command Packet Parser                                            */
/*------------------------------------------------------------------*/
static word ComParse(byte* req_data, int req_length, int argc,...) {
  word ploc;            /* points to current location within packet */
  byte * found;
  byte w;
  word wlen;
  byte codeset,lock;
  byte far * in;
  word f_mask;
  word f_bit;
  int i;
  word maxlen;
  word plen;
  int code;
  va_list ap;

  va_start(ap,argc);
  for(i=0;i<argc;i++)
  {
    if(va_arg(ap,int) &PARSE_CLEAR) (va_arg(ap,byte *))[0] = 0;
    else (void)(va_arg(ap,byte *));
    (void)(va_arg(ap,int));
  }
  va_end(ap);

  in   = &req_data[0];
  plen = (word)req_length;

  ploc = 0;
  codeset = 0;
  lock = 0;
  f_mask = 0;
  while(1)
  {
    /* read information element id and length                   */
    if (ploc >= plen ||
        (((w = in[ploc])==0) && ((ploc+1)==plen)))
    {
      return(0);
    }
    w = in[ploc];
    if(w & 0x80)
    {
      /* single byte element */
      w &=0xf0;
      wlen = 1;
    }
    else
    {
      /* multi byte element, check if length present and valid */
      if ( (ploc + 1 >= plen)
      || (ploc + (wlen = (word)(in[ploc+1] + 2)) > plen) )
      {
        return(1);
      }
    }

    if(lock & 0x80) lock &=0x7f;
    else codeset = lock;

    if(w==SHIFT)
    {
      codeset = in[ploc];
      if(!(codeset & 0x08)) lock = (byte)(codeset & 7);
      codeset &=7;
      lock |=0x80;
    }
    else
    {
      if(w==ESC && wlen>=3) code = in[ploc+2] |PARSE_ESC;
      else code = w;
      code |= (codeset<<8);

      va_start(ap,argc);
      for(i=0,f_bit=1; i<argc; i++,f_bit<<=1)
      {
        if((va_arg(ap,int)&0xfff)==code)
        {
          if(!(f_mask &f_bit)) break;
        }
        (void)(va_arg(ap,byte *));
        (void)(va_arg(ap,int));
      }

      if (i<argc)
      {

        f_mask |=f_bit;
        found = va_arg(ap,byte *);
        maxlen = (word)va_arg(ap,int);

        if(w &0x80)
          *found = in[ploc];
        else
          ComWRead(found,in,ploc,maxlen);
      }
      va_end(ap);
    }
    ploc = ploc+wlen;
  }
}

static void man_usr_process_req (byte Req, diva_driver_management_t* pE, ENTITY* e) {
  byte      Rc;
  byte      path[PATHR_SIZE];
  byte      var[VARR_SIZE];
  PATH_CTXT DCtxt[DIR_LEVELS+3]; /* actual directory context         */
  byte      DLevel;              /* actual directory nesting level   */
  unsigned int i;

	/*
		Copy request data
		*/
	pE->sig_assign.length = (word)diva_copy_xdescr (pE->sig_assign.P,
																									e,
																									sizeof(pE->sig_assign.P));

  /* Retrieve ESC-MAN info element in the format Length/Element content   */
  /* and make sure that the info element contains something meaningful!   */
  ComParse (&pE->sig_assign.P[0],
						pE->sig_assign.length,
						1, 0x80 | PARSE_ESC | PARSE_CLEAR, path, sizeof(path) - 1);

  if ((path[0] < 6)                       || /* info element too short    */
      (path[0] &0x80)                     || /* length range ...127       */
      (path[0] + 1 > PATHR_SIZE - 1)      || /* info elem too long        */
      (path[5] + path[6] != path[0] - 6)  || /* length fields inconsistet */
      (path[5] > VARR_SIZE -1)               /* variable too long         */) {
    DBG_ERR(("MIF: got illegal info element(p0=0x%x,p5=0x%x,p6=0x%x,PS=0x%x,VS=0x%x)!",
             path[0], path[5], path[6], PATHR_SIZE, VARR_SIZE))
		e->complete = 0xff;
		e->Rc = WRONG_COMMAND;
		(*(e->callback))(e);
		return;
  }

  /* Copy variable (if any) into separate var buffer */
  memset(var, 0x00, VARR_SIZE);

  memcpy(var, &path[7 + path[6]], path[5]);

  path[7 + path[6]] = 0;  /* null terminate path for use with find_path function */

  /* initialize path trace variables */
  memset((byte *)DCtxt, 0x00, sizeof(DCtxt)) ;
  DLevel = DIR_LEVELS-1 ;
  Rc = 0xff ;

  switch (Req) {
    case MAN_READ:
			pE->ind_count = 0;
			pE->man_ind_data_to_user[0].length = 0;
      man_read(drv_man_get_driver_root(), (char*)&path[7], diva_man_add_ie, pE, pE, DCtxt, &DLevel);
			if (!pE->man_ind_data_to_user[0].length) {
        Rc = WRONG_COMMAND; /* inhibit an RC == OK in this case, to avoid application */
				break;              /* waiting for an info indication                         */
			}
			if (pE->chain_ind && pE->ind_count) { // create chained indication
			  pE->sig_ind_to_user = MAN_INFO_IND;
			  man_fill_ind_count_element(pE, NUM_NET_DBUFFER, pE->ind_count + 1);
			  man_usr_delivery_ind (Req, pE, NUM_NET_DBUFFER, e);
			}
			for (i=0; (i<=pE->ind_count) && pE->man_ind_data_to_user[i].length; i++)
			{
			  pE->sig_ind_to_user = MAN_INFO_IND;
			  pE->man_ind_data_to_user[i].P[pE->man_ind_data_to_user[i].length++] = 0;
			  man_usr_delivery_ind (Req, pE, i, e);
			}
      break;

    case MAN_WRITE:
			if (!man_write(drv_man_get_driver_root(), (char*)&path[7], var, pE, pE, DCtxt, &DLevel)) {
				Rc = WRONG_COMMAND;
			}
      break;

    case MAN_EXECUTE:
      man_execute(drv_man_get_driver_root(), (char*)&path[7], pE, pE, DCtxt, &DLevel);
      break;

		default:
			Rc = 0x00;
	}

	e->complete = 0xff;
	if (Rc == 0xff) {
		e->Rc = 0xff;
	} else {
		e->Rc = WRONG_COMMAND;
	}
	(*(e->callback))(e);
}

/*
  Create virtual ind_count information element
  */
static void man_fill_ind_count_element(diva_driver_management_t* pE,
                                       unsigned int buffer_idx,
                                       unsigned int value)
{
	word *plength = &pE->man_ind_data_to_user[buffer_idx].length;
	byte *data    = &pE->man_ind_data_to_user[buffer_idx].P[0];

  *plength = 0;
  data[(*plength)++] = 0x7f;
  data[(*plength)++] = 0x00; // overall length, filled in below
  data[(*plength)++] = 0x80;
  data[(*plength)++] = 0x82; // MI_UINT
  data[(*plength)++] = 0x04; // is an ESC variable
  data[(*plength)++] = 0x00; // status
  data[(*plength)++] = 0x01; // var length
  data[(*plength)++] = 0;    // path length, filled in below
  data[(*plength)++] = 'i';  // path
  data[(*plength)++] = 'n';
  data[(*plength)++] = 'd';
  data[(*plength)++] = '_';
  data[(*plength)++] = 'c';
  data[(*plength)++] = 'o';
  data[(*plength)++] = 'u';
  data[(*plength)++] = 'n';
  data[(*plength)++] = 't';
  data[7]            = (byte)(*plength - 8); // path length
  data[(*plength)++] = (byte)value; // variable value
  data[(*plength)++] = 0x00;  // required by applications (vlad)
  data[1] = (byte)(*plength - 2); // overall length
}

/*
  Delivery current indication to user
  Save RNR condition
  */
static void man_usr_delivery_ind (unsigned int message,
                                  diva_driver_management_t* pE,
                                  unsigned int buffer_idx,
																	ENTITY* e) {
	byte Ind = pE->sig_ind_to_user, IndCh = 0;
	word length = pE->man_ind_data_to_user[buffer_idx].length;
	byte*	data  = &pE->man_ind_data_to_user[buffer_idx].P[0];
	PBUFFER RBuffer;

	/*
		DBG_TRC(("ManUsr: Ind:%02x, IndCh:%02x, length=%d", Ind, IndCh, length))
		*/

  e->complete = (byte)(length <= sizeof(RBuffer.P));
  e->Ind      = Ind;
  e->IndCh    = IndCh;
  e->RLength  = length;
	e->RNum     = 0;
	e->RNR      = 0;

	if ((RBuffer.length = MIN(length, sizeof(RBuffer.P)))>0) {
		memcpy (RBuffer.P, data, RBuffer.length);
	}
	e->RBuffer = (DBUFFER*)&RBuffer;

  (*(e->callback))(e);

  if (e->RNR == 1) {
		e->RNR = 0;
    return;
	}

  if (e->RNR) {
		e->RNR = 0;
		return;
	}

	if (e->RNum == 0) {
    return;
	}

  e->Ind = Ind;

  diva_copy_ind_to_user (e, data, length);

  e->complete = 2;

  (*(e->callback))(e);

  pE->sig_ind_to_user = 0;
}

/*
  Add information element to flat buffer
  Switch buffer for chained indications if necessary and possible
  */
static int diva_man_add_ie (byte *header, byte *value, void *context) {
	diva_driver_management_t* pE = (diva_driver_management_t*)context;
	byte* dst;

	if (header) {
  	word      size = (word)(header[1]+2);
		if ((size + pE->man_ind_data_to_user[pE->ind_count].length) <
												                      DIVA_MAN_DRV_MAX_NET_DATA_IND) {
	    dst = &pE->man_ind_data_to_user[pE->ind_count].P[pE->man_ind_data_to_user[pE->ind_count].length];
			memcpy (dst, header, size);
			pE->man_ind_data_to_user[pE->ind_count].length = pE->man_ind_data_to_user[pE->ind_count].length+size;
			return (size);
		} else {
      if (!pE->chain_ind || ((pE->ind_count+1) >= NUM_NET_DBUFFER)) {
			DBG_FTL(("I.E. ignored due to buffer overflow"))
        return (0);
      }
      pE->ind_count++;
			pE->man_ind_data_to_user[pE->ind_count].length = size;
	    dst = pE->man_ind_data_to_user[pE->ind_count].P;
			memcpy (dst, header, size);
			return (size);
		}
	} else {
		DBG_FTL(("Unhandled I.E."))
	}

	return (0);
}

/*
  Copy data described by request buffer descriptor to flat buffer
  return lenght of data in buffer
  */
static int diva_copy_xdescr (byte* dst, const ENTITY* e, int max_length) {
  int i = 0;
  int length = 0, to_copy, descriptor_too_long = 0;

  while (i < e->XNum && max_length != 0) {
    if (e->X[i].PLength != 0 && e->X[i].P != 0) {
      descriptor_too_long |= ((to_copy = MIN(max_length, e->X[i].PLength)) != e->X[i].PLength);
      memcpy (&dst[length], &e->X[i].P[0], to_copy);
      length     += to_copy;
      max_length -= to_copy;
    }
    i++;
  }
  if (descriptor_too_long != 0 || i < e->XNum) {
		DBG_ERR(("descriptor too long %s at %d", __FILE__, __LINE__))
  }

  return (length);
}

/*
  Copy data from internal buffer to the buffer chain provided by application
  */
static void diva_copy_ind_to_user (ENTITY* e, byte* src, int length) {
  int i = 0;
  int to_copy;

  while ((i < e->RNum) && length) {
    if ((to_copy = MIN(e->R[i].PLength, length))!=0) {
      if (e->R[i].P) {
        memcpy (e->R[i].P, src, to_copy);
      }
      e->R[i].PLength = (word)to_copy;
      length -= to_copy;
    }
    i++;
  }

  e->RNum = (byte)i;
}

/*
	Helper functions used by driver management interface instances
  */
void* diva_man_read_value_by_offset (diva_driver_management_t* pM,
																		 byte* base,
																		 dword offset,
																		 MAN_INFO* info) {
	void* ret = 0;

	if (info) {
		switch (info->type) {
  		case MI_UINT:
			case MI_BITFLD:
			case MI_HINT:
			case MI_INT:
				if (info->max_len <= sizeof(pM->dw_out)) {
					ret = &pM->dw_out;
					memcpy (ret, &base[offset], info->max_len);
				}
				break;

			case MI_BOOLEAN:
				if (info->max_len <= sizeof(pM->dw_out)) {
					ret = &pM->dw_out;
					switch (info->max_len) {
						case 1: {
							byte i = *(byte*)(base+offset) != 0;
							memcpy (ret, &i, sizeof(i));
						} break;
						case 2: {
							word i = *(word*)(base+offset) != 0;
							memcpy (ret, &i, sizeof(i));
						} break;
						default: {
							dword i = *(dword*)(base+offset) != 0;
							memcpy (ret, &i, info->max_len);
						} break;
					}
				}
				break;

			case MI_ASCIIZ:
				break;

			default:
				DBG_FTL(("Read of type %02x not supported", info->type))
				break;
		}
	}

	return (ret);
}

/*
	Write management interface variable
	*/
void diva_man_write_value_by_offset (diva_driver_management_t* pM,
																		 const byte* src,
																		 byte* base,
																		 dword offset,
																		 MAN_INFO* info) {

	if (info) {
		switch (info->type) {
  		case MI_UINT:
			case MI_BITFLD:
			case MI_HINT:
			case MI_INT:
				if (info->max_len <= sizeof(pM->dw_out)) {
					memcpy ((base + offset), (void*)src, info->max_len);
				}
				break;

			case MI_BOOLEAN:
				if (info->max_len <= sizeof(pM->dw_out)) {
					switch (info->max_len) {
						case 1: {
							*(byte*)(base+offset) = *(const byte*)src;
						} break;
						case 2: {
							*(word*)(base+offset) = *(const word*)src;
						} break;
						default: {
							*(dword*)(base+offset) = *(const dword*)src;
						} break;
					}
				}
				break;

			case MI_ASCIIZ:
				break;

			default:
				DBG_FTL(("Read of type %02x not supported", info->type))
				break;
		}
	}
}

int drv_man_nohide (const void* Ipar) {
	const diva_driver_management_t* pE = (const diva_driver_management_t*)Ipar;

	return ((pE == 0) || (pE->nohide != 0));
}

