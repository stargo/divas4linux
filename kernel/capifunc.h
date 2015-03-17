/* $Id: capifunc.h,v 1.10 2003/08/25 10:06:37 schindler Exp $
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
 */

#ifndef __CAPIFUNC_H__
#define __CAPIFUNC_H__

#define DRRELMAJOR  3
#define DRRELMINOR  0
#define DRRELEXTRA  "3.1.6-109.75-1"

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
