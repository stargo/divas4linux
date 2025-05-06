
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

#ifndef __CAPIFUNC_H__
#define __CAPIFUNC_H__

#define DRRELMAJOR  3
#define DRRELMINOR  0
#define DRRELEXTRA  "3.2.0-124.26-1"

#define M_COMPANY "Eicon Networks"

extern char DRIVERRELEASE_CAPI[];

typedef struct _diva_card {
	int Id;
	struct _diva_card *next;
	struct capi_ctr capi_ctrl;
	DIVA_CAPI_ADAPTER *adapter;
	DESCRIPTOR d;
  int remove_in_progress;
	char name[32];
	int PRI;
#if defined(DIVA_EICON_CAPI)
	char serial[CAPI_SERIAL_LEN];
#endif
} diva_card;

/*
 * prototypes
 */
int init_capifunc(void);
void finit_capifunc(void);

#endif /* __CAPIFUNC_H__ */
