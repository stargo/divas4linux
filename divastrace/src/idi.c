
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

#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include "st_ifc.h"
#include "idi_ifs.h"
#include "os.h"
#include "um_xdi.h"
#include "pc.h"
#include "cardtype.h" 

/*
	LOCAL FUNCTIONS
	*/
static void single_p (byte * P, word * PLength, byte Id);

/*
	Return OS independent handle to IDI adapter
	descriptor
	*/
void* SuperTraceOpenAdapter (int AdapterNumber) {
	return (diva_os_open_adapter (AdapterNumber));
}

const dword* SuperTraceReadDescriptorList (void) {
	static byte data[2048+512];
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	diva_um_idi_ind_hdr_t* pInd = (diva_um_idi_ind_hdr_t*)&data[0];
	dword *p = (dword*)&pInd[1];

	pReq->type  = DIVA_UM_IDI_GET_DESCRIPTOR_LIST;
	pReq->Req   = 0;
	pReq->ReqCh = 0;
	pReq->data_length = 0;

	if (diva_os_read_descriptor_list (pReq, sizeof(*pReq), pInd, sizeof(data)-1) == 0 &&
			pInd->type == DIVA_UM_IDI_IND_DESCRIPTOR_LIST &&
			pInd->hdr.list.number_of_descriptors != 0) {
		p[-1] = (dword)pInd->hdr.list.number_of_descriptors;
#if !defined(LINUX)
		{
			dword i;

			for (i = 0; i < p[-1]; i++) {
				p[i] += (p[i] <= 100);
			}
		}
#endif
		return (&p[-1]);
	}

	*p = 0;

	return (p);
}

/*
		Close adapter descriptor
	*/
int SuperTraceCloseAdapter (void* AdapterHandle) {
	return (diva_os_close_adapter (AdapterHandle));
}

int SuperTraceWrite (void* AdapterHandle, const void* data, int length) {
	if (diva_os_put_req (AdapterHandle, data, length) < 0) {
		return (-1);
	}

	return (0);
}

int SuperTraceRead (void* AdapterHandle, void* data, int max_length) {
	int ret;
	byte* ptr = (byte*)data;

	if (max_length < 1)
		return (-1);

	if ((ret = diva_os_get_message (AdapterHandle, data, max_length-1)) > 0) {
		ptr[ret] = 0; /* add zero to terminate message chain */
		return (ret);
	}

	return (-1);
}

void* SuperTraceGetWaitableObject (void* AdapterHandle) {
	return (diva_os_convert_idi_handle_to_waitable_object (AdapterHandle));
}

int SuperTraceReadRequest (void* AdapterHandle, const char* name, byte* data) {
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	byte* xdata = (byte*)&pReq[1];
	char tmp = 0;
	word length;

	if (!strcmp(name, "\\")) { /* Read ROOT */
		name = &tmp;
	}
	length = SuperTraceCreateReadReq (xdata, name);
	single_p (xdata, &length, 0); /* End Of Message */

	pReq->type  = DIVA_UM_IDI_REQ_MAN;
	pReq->Req   = MAN_READ;
	pReq->ReqCh = 0;
	pReq->data_length = (dword)length;

	if (SuperTraceWrite (AdapterHandle, data, sizeof(*pReq) + length)) {
		return (-1);
	}

	return (0);
}

int SuperTraceWriteVar (void* AdapterHandle,
												byte* data,
										 		const char* name,
										 		void* var,
										 		byte type,
										 		byte var_length) {
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	byte* xdata = (byte*)&pReq[1];
	diva_man_var_header_t* pVar = (diva_man_var_header_t*)xdata;
	word length;

	length = SuperTraceCreateReadReq (xdata, name);
	memcpy (&xdata[length], var, var_length);
	length += var_length;
	pVar->length += var_length;
	pVar->value_length = var_length;
	pVar->type = type;
	single_p (xdata, &length, 0); /* End Of Message */

	pReq->type  = DIVA_UM_IDI_REQ_MAN;
	pReq->Req   = MAN_WRITE;
	pReq->ReqCh = 0;
	pReq->data_length = (dword)length;

	if (SuperTraceWrite (AdapterHandle, data, sizeof(*pReq) + length)) {
		return (-1);
	}

	return (0);
}

word SuperTraceCreateReadReq (byte* P, const char* path) {
	byte var_length;
	byte* plen;

	var_length = (byte)strlen (path);

	*P++ = ESC;
	plen = P++;
	*P++ = 0x80; /* MAN_IE */
	*P++ = 0x00; /* Type */
	*P++ = 0x00; /* Attribute */
	*P++ = 0x00; /* Status */
	*P++ = 0x00; /* Variable Length */
	*P++ = var_length;
	memcpy (P, path, var_length);
	P += var_length;
	*plen = var_length + 0x06;

	return ((word)(var_length + 0x08));
}

static void single_p(byte * P, word * PLength, byte Id) {
  P[(*PLength)++] = Id;
}

int SuperTraceGetNumberOfChannels (void* AdapterHandle) {
	diva_um_idi_req_hdr_t Req;
	diva_um_idi_ind_hdr_t Ind;
	int ret;

	Req.type = DIVA_UM_IDI_GET_FEATURES;
	if (diva_os_put_req (AdapterHandle, &Req, sizeof(Req)) < 0) {
		return (0);
	}

	if ((ret = diva_os_get_message (AdapterHandle, &Ind, sizeof(Ind))) <= 0) {
		return (0);
	}

	if (Ind.type == DIVA_UM_IDI_IND_FEATURES) {
		if (Ind.hdr.features.type >= 0x40 /*IDI_VADAPTER*/ ) {
			return (0);
		}
		return (Ind.hdr.features.channels);
	}

	return (0);
}

unsigned int SuperTraceGetAdapterSerialNumber (void* AdapterHandle) {
	diva_um_idi_req_hdr_t Req;
	diva_um_idi_ind_hdr_t Ind;
	int ret;

	Req.type = DIVA_UM_IDI_GET_FEATURES;
	if (diva_os_put_req (AdapterHandle, &Req, sizeof(Req)) < 0) {
		return (0xffffffff);
	}

	if ((ret = diva_os_get_message (AdapterHandle, &Ind, sizeof(Ind))) <= 0) {
		return (0xffffffff);
	}

	if (Ind.type == DIVA_UM_IDI_IND_FEATURES) {
		return ((unsigned int)(Ind.hdr.features.serial_number));
	}

	return (0xffffffff);
}

int SuperTraceGetAdapterName (void* AdapterHandle, char* data, int max_length) {
	diva_um_idi_req_hdr_t Req;
	diva_um_idi_ind_hdr_t Ind;
	int ret;

	if (!data || (max_length < 2)) {
		return (-1);
	}

	Req.type = DIVA_UM_IDI_GET_FEATURES;
	if (diva_os_put_req (AdapterHandle, &Req, sizeof(Req)) < 0) {
		return (-1);
	}

	memset (&Ind, 0x00, sizeof(Ind));
	if ((ret = diva_os_get_message (AdapterHandle, &Ind, sizeof(Ind))) <= 0) {
		return (-1);
	}

	if (Ind.type == DIVA_UM_IDI_IND_FEATURES) {
		int nameLength = strlen(Ind.hdr.features.name)+1;
		int cardType = 0;

		if (nameLength+24 < sizeof(Ind.hdr.features.name)) {
			const char* p = &Ind.hdr.features.name[nameLength];
	
			if (p[0] == 'T' && p[1] == ':') {
				cardType = atoi(&p[2]);
		        	if (cardType >= CARDTYPE_MAX)
					cardType = 0;
			}
		}

		if (cardType != 0) {
			nameLength--;
			if (strlen(CardProperties[cardType].Name) > nameLength) {
				const char* p = strstr (CardProperties[cardType].Name, Ind.hdr.features.name);

				if (p != 0 && p == CardProperties[cardType].Name) {

					memcpy (data, CardProperties[cardType].Name, MIN((strlen(CardProperties[cardType].Name)+1), max_length));
				} else {

					memcpy (data, &Ind.hdr.features.name[0], MIN((sizeof(Ind.hdr.features.name)), max_length));
				}
			}
		} else {

			memcpy (data, &Ind.hdr.features.name[0], MIN((sizeof(Ind.hdr.features.name)), max_length));
		}

		data[max_length-1] = 0;
		return ((int)(strlen(data)));
	}

	return (-1);
}

/*
	Let IDI associate one entiry with file descriptor
	*/
int SuperTraceASSIGN (void* AdapterHandle, byte* data) {
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	byte lli[] = { 0x19, 0x02, 0x41, 0x01 };

	pReq->type = DIVA_UM_IDI_REQ_MAN;
	pReq->Req = ASSIGN;
	pReq->ReqCh = 0;
	pReq->data_length = sizeof(lli);
	memcpy (&pReq[1], lli, sizeof(lli));

	if (SuperTraceWrite (AdapterHandle, data, sizeof(*pReq)+pReq->data_length)) {
		return (-1);
	}

	return (0);
}

/*
	Issue EVENT ON request to IDI
	*/
int SuperTraceTraceOnRequest(void* hAdapter, const char* name, byte* data) {
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	byte* xdata = (byte*)&pReq[1];
	char tmp = 0;
	word length;

	if (!strcmp(name, "\\")) { /* Read ROOT */
		name = &tmp;
	}
	length = SuperTraceCreateReadReq (xdata, name);
	single_p (xdata, &length, 0); /* End Of Message */

	pReq->type  = DIVA_UM_IDI_REQ_MAN;
	pReq->Req   = MAN_EVENT_ON;
	pReq->ReqCh = 0;
	pReq->data_length = (dword)length;

	if (SuperTraceWrite (hAdapter, data, sizeof(*pReq) + length)) {
		return (-1);
	}

	return (0);
}

int SuperTraceExecuteRequest (void* AdapterHandle,
															const char* name,
															byte* data) {
	diva_um_idi_req_hdr_t* pReq = (diva_um_idi_req_hdr_t*)&data[0];
	byte* xdata = (byte*)&pReq[1];
	word length;

	length = SuperTraceCreateReadReq (xdata, name);
	single_p (xdata, &length, 0); /* End Of Message */

	pReq->type  = DIVA_UM_IDI_REQ_MAN;
	pReq->Req   = MAN_EXECUTE;
	pReq->ReqCh = 0;
	pReq->data_length = (dword)length;

	if (SuperTraceWrite (AdapterHandle, data, sizeof(*pReq) + length)) {
		return (-1);
	}

	return (0);
}
