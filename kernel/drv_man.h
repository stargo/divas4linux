
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
#ifndef __DIVA_DRV_MANAGEMENT_
#define __DIVA_DRV_MANAGEMENT_

/*
  Defines for logical adapter numbers of misc drivers
  */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_CAPI      1000 /* CAPI */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_TTY       1001 /* TTY Interface */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_WMP       1002 /* WMP or any other networking */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_CFG       1003 /* CFGLibrary/Dadapter */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_MTPX_MGNT 1004 /* MTPX adapter manager */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_SIPCTRL   1005 /* SIP control */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_SOFTIP    1007 /* SoftIP */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_DIVA_IP_HMP 1008 /* Diva IP HMP */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_MTP3      1010 /* MTP3 */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_ISUP      1011 /* ISUP */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_DAL       1020 /* DAL */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_SSDD      1021 /* SSDD */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_DVSC      1022 /* DVSC */
#define DIVA_DRV_MAN_LOGICAL_ADAPTER_NR_NEXT_FREE 1023

int diva_start_management (int logical_adapter_nr);
void diva_stop_management (void);

#define NUM_NET_DBUFFER 8

#ifndef DIVA_MTPX_MAX_NET_DATA_IND
#define DIVA_MAN_DRV_MAX_NET_DATA_IND 2152
typedef struct _diva_mtpx_net_dbuffer {
	word length;
	byte P[DIVA_MAN_DRV_MAX_NET_DATA_IND];
} NET_DBUFFER;
#else
#define DIVA_MAN_DRV_MAX_NET_DATA_IND DIVA_MTPX_MAX_NET_DATA_IND
#endif

/*
	Small Management interface implementation for device drivers.
	- Only one management interface entity is upported
  - Only READ/WRITE/EXECUTE operations are supported
  - All data types except EVENT are supported
	- No flow control is supported
  */
#define DIVA_DRIVER_MANAGEMENT_MAX_ENTITIES 2

typedef struct _diva_driver_management_req_entry {
	ENTITY* e; /* User entity */
	int req_pending; /* User request pending */
	unsigned int chain_ind; // set by appl if multible INDs for one REQ supported
} diva_driver_management_req_entry_t;

typedef struct _diva_driver_management {
	int initialized;
	int logical_adapter_nr;

	diva_driver_management_req_entry_t entities[DIVA_DRIVER_MANAGEMENT_MAX_ENTITIES];
	byte assignId;
	ENTITY* wrong_e; /* User entity */

	diva_os_soft_isr_t  req_soft_isr;
	diva_os_spin_lock_t req_spin_lock;
	DBUFFER	sig_assign;
	NET_DBUFFER man_ind_data_to_user[NUM_NET_DBUFFER+1];
	unsigned int chain_ind; // set by appl if multible INDs for one REQ supported
	unsigned int ind_count; // number of (by MIF info elements) occupied NET_DBUFFER's
	byte sig_ind_to_user;

	int   int_out;
	dword dw_out;
	byte  b_out;
	char  string_out[128];
} diva_driver_management_t;

struct man_info_s;
void* diva_man_read_value_by_offset (diva_driver_management_t* pM,
																		 byte* base,
																		 dword offset,
																		 struct man_info_s* info);
void diva_man_write_value_by_offset (diva_driver_management_t* pM,
																		 const byte* src,
																		 byte* base,
																		 dword offset,
																		 struct man_info_s* info);
#endif

