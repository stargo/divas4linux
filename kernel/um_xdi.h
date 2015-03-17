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
/* $Id: um_xdi.h,v 1.1.2.1 2001/02/08 12:25:44 armin Exp $ */

#ifndef __DIVA_USER_MODE_XDI_H__
#define __DIVA_USER_MODE_XDI_H__

/*
  Contains declaratiom of structures shared between application
  and user mode idi driver
*/

#define DIVA_UM_IDI_IO_CMD_WRITE      0x3703
#define DIVA_UM_IDI_IO_CMD_READ       0x3704

typedef struct _diva_um_io_cmd {
  unsigned int length;
  void * data;
} diva_um_io_cmd;

typedef struct _diva_um_idi_adapter_features {
  dword type;
  dword features;
  dword channels;
  dword serial_number;
  char  name [128];
} diva_um_idi_adapter_features_t;

/*
  Used in indication to provide number of
  descriptors in list.
  The descriptor list follows the message header.
  Every descriptor is 4 byte long number
  */
typedef struct _diva_um_idi_descriptor_list {
 dword number_of_descriptors;
} diva_um_idi_descriptor_list_t;

#define DIVA_UM_IDI_REQ_MASK			0x0000FFFF
#define DIVA_UM_IDI_REQ_TYPE_MASK		(~(DIVA_UM_IDI_REQ_MASK))
#define DIVA_UM_IDI_GET_FEATURES		1	/* trigger features indication */
#define DIVA_UM_IDI_REQ				2
#define DIVA_UM_IDI_GET_DESCRIPTOR_LIST 3
#define DIVA_UM_IDI_REQ_TYPE_MAN		0x10000000
#define DIVA_UM_IDI_REQ_TYPE_SIG		0x20000000
#define DIVA_UM_IDI_REQ_TYPE_NET		0x30000000
#define DIVA_UM_IDI_REQ_TYPE_REGISTER_ADAPTER	0x0000ffff
#define DIVA_UM_IDI_REQ_TYPE_REMOVE_ADAPTER   0x50000000

#define DIVA_UM_IDI_REQ_MAN			(DIVA_UM_IDI_REQ | DIVA_UM_IDI_REQ_TYPE_MAN)
#define DIVA_UM_IDI_REQ_SIG			(DIVA_UM_IDI_REQ | DIVA_UM_IDI_REQ_TYPE_SIG)
#define DIVA_UM_IDI_REQ_NET			(DIVA_UM_IDI_REQ | DIVA_UM_IDI_REQ_TYPE_NET)
/*
  data_length  bytes will follow this structure
*/
typedef struct _diva_um_idi_req_hdr {
  dword type;
  dword Req;
  dword ReqCh;
  dword data_length;
} diva_um_idi_req_hdr_t;

typedef struct _diva_um_idi_ind_parameters {
  dword Ind;
  dword IndCh;
} diva_um_idi_ind_parameters_t;

typedef struct _diva_um_idi_rc_parameters {
  dword Rc;
  dword RcCh;
} diva_um_idi_rc_parameters_t;

typedef union _diva_um_idi_ind {
  diva_um_idi_adapter_features_t  features;
  diva_um_idi_ind_parameters_t    ind;
  diva_um_idi_rc_parameters_t     rc;
  diva_um_idi_descriptor_list_t   list;
} diva_um_idi_ind_t;

#define DIVA_UM_IDI_IND_FEATURES  1  /* features indication */
#define DIVA_UM_IDI_IND           2
#define DIVA_UM_IDI_IND_RC        3
#define DIVA_UM_IDI_IND_DESCRIPTOR_LIST 4
/*
  data_length bytes of data follow
  this structure
*/
typedef struct _diva_um_idi_ind_hdr {
  dword type;
  diva_um_idi_ind_t  hdr;
  dword  data_length;
} diva_um_idi_ind_hdr_t;


/*
	User mode IDI proxy structures
	*/
typedef struct _diva_um_idi_proxy_req {
	dword Id;
	dword descriptor_type;
	dword channels;
	dword features;
	dword logical_adapter_number;
	dword controller;
	dword total_controllers;
	dword max_entities;
	dword serial_number;
	dword card_type;
	dword name_length;
	byte  name[24];
} diva_um_idi_proxy_req_t;

typedef struct _diva_um_idi_proxy_idi_req {
	word Id;
	word Req;
	word Ch;
	word data_length;
} diva_um_idi_proxy_idi_req_t;

/*
	data_length:
		(data_length & 0x8000)  != 0 for return code,
		(data_length & ~0x8000) return code type
	*/
#define DIVA_UM_IDI_PROXY_RC_TYPE_REMOVE_COMPLETE 0x0001
typedef struct _diva_um_idi_proxy_idi_rc_or_ind {
	word Id;
	word RcInd;
	word Ch;
	word data_length;
} diva_um_idi_proxy_idi_rc_or_ind_t;




#endif

