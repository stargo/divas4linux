
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

#ifndef __DIVAS_USER_MODE_IDI_ENTITY__
#define __DIVAS_USER_MODE_IDI_ENTITY__

#define DIVA_UM_IDI_RC_PENDING      0x00000001
#define DIVA_UM_IDI_REMOVE_PENDING  0x00000002
#define DIVA_UM_IDI_TX_FLOW_CONTROL 0x00000004
#define DIVA_UM_IDI_REMOVED         0x00000008
#define DIVA_UM_IDI_ASSIGN_PENDING  0x00000010
#define DIVA_UM_IDI_TIMEOUT         0x00000100
#define DIVA_UM_IDI_ME_TIMEOUT      0x00000200
#define DIVA_UM_IDI_ME_CHAINED_IND_PENDING 0x00000400

#define DIVA_UM_IDI_MAX_TIMEOUTS    10

#define DIVA_UM_IDI_ENTITY_TYPE_MANAGEMENT 0
#define DIVA_UM_IDI_ENTITY_TYPE_MASTER     1
#define DIVA_UM_IDI_ENTITY_TYPE_LOGICAL    2

typedef enum _diva_um_idi_entity_type {
	DivaUmIdiEntityTypeManagement = 0,
	DivaUmIdiEntityTypeMaster,
	DivaUmidiEntityTypeLogical
} diva_um_idi_entity_type_t;

typedef enum _diva_um_idi_multiplex_type {
	DivaUmIdiMasterTypeIdi = 0,
	DivaUmIdiMasterTypeLicense
} diva_um_idi_multiplex_type_t;

typedef enum _diva_um_idi_license_state {
	DivaUmIdiLicenseIdle = 0,
	DivaUmIdiLicenseInit,
	DivaUmIdiLicenseWrite,
	DivaUmIdiLicenseWaitResponse
} diva_um_idi_license_state_t;

typedef enum _diva_um_idi_master_entity_state {
  DivaUmIdiMasterIdle = 0,
  DivaUmIdiMasterAssignPending,
  DivaUmIdiMasterAssigned,
  DivaUmIdiMasterResponsePending,
  DivaUmIdiMasterResponsePendingCancelled,
  DivaUmIdiMasterRemovePending,
  DivaUmIdiMasterOutOfService,
} diva_um_idi_master_entity_state_t;

typedef enum _diva_um_idi_entity_function_type {
	DivaUmIdiEntity = 0,
	DivaUmIdiProxy
} diva_um_idi_entity_function_type_t;

struct _divas_um_idi_entity;

#define DIVA_UM_IDI_APPLICATION_FEATURE_COMBI_IND 0x00000001

typedef struct _divas_um_idi_entity {
  diva_entity_link_t        link; /* should be first */
	diva_um_idi_entity_function_type_t entity_function; /* DivaUmIdiEntity */
  diva_um_idi_adapter_t*    adapter; /* Back to adapter */
  ENTITY                    e;
  void*                     os_ref;
  dword                     status;
  void*                     os_context;
  int                       rc_count;
  diva_um_idi_data_queue_t  data; /* definad by user 1 ... MAX */
  diva_um_idi_data_queue_t  rc;   /* two entries */
  BUFFERS                   XData;
  BUFFERS                   RData;
  byte                      buffer[2048+512];
	int												cbuffer_length;
	int												cbuffer_pos;
	byte											cbuffer[4096];

	diva_um_idi_entity_type_t entity_type;

	union {
		struct {
			diva_um_idi_master_entity_state_t state;
			diva_um_idi_multiplex_type_t			type;
			diva_um_idi_license_state_t       license_state;
			diva_entity_queue_t       active_multiplex_q;
			diva_entity_queue_t       idle_multiplex_q;
		} master;
		struct {
			diva_entity_link_t        link; /* used to link on multiplex queue */
			struct _divas_um_idi_entity* xdi_entity; /* associated XDI entity */
			int on_active_q;
		} logical;
  } multiplex;

	dword application_features;

} divas_um_idi_entity_t;

typedef enum _diva_um_idi_proxy_state {
	DivaUmIdiProxyIdle = 0,
	DivaUmIdiProxyRegistered,
} diva_um_idi_proxy_state_t;


struct _diva_um_idi_proxy_entity;

typedef struct _divas_um_idi_proxy {
	diva_entity_link_t        link; /* should be first */
	diva_um_idi_entity_function_type_t entity_function; /* DivaUmIdiProxy */
	void*                     os_ref;
	void*                     os_context;
	diva_um_idi_proxy_state_t state;
	int                       req_nr;
	dword logical_adapter_number;
	dword controller;
	dword total_controllers;
	dword serial_number;
	dword adapter_features;
	dword card_type;
	dword max_entities;
	byte name[BOARD_NAME_LENGTH];

	dword nr_entities;
	struct _diva_um_idi_proxy_entity* entity_map[0xff+1];

	/*
		Used to save pending assigns until read function
		was called
		*/
	int nr_pending_requests;
	ENTITY* pending_requests_q[0xff+1];

	diva_os_spin_lock_t req_lock;
	diva_os_spin_lock_t callback_lock;
	diva_entity_queue_t entity_q;
	diva_entity_queue_t assign_q;
	diva_entity_queue_t req_q;

	IDI_CALL dadapter_request;

} divas_um_idi_proxy_t;


#endif
