/* $Id: os_capi.h,v 1.7 2003/04/12 21:40:49 schindler Exp $
 *
 * ISDN interface module for Dialogic active cards DIVA.
 * CAPI Interface OS include files 
 * 
 * Copyright 2000-2010 by Armin Schindler (mac@melware.de) 
 * Copyright 2000-2010 Cytronics & Melware (info@melware.de)
 * Copyright 2000-2007 Dialogic
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#ifndef __OS_CAPI_H__ 
#define __OS_CAPI_H__

#if defined(DIVA_EICON_CAPI)
#include "local_capi.h"
#include "local_kernelcapi.h"
#include "local_capiutil.h"
#include "local_capilli.h"
#else
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capilli.h>
#endif

#endif /* __OS_CAPI_H__ */
