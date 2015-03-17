/*****************************************************************************
 *
 * (c) COPYRIGHT 2005-2008       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be copied,
 * transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *****************************************************************************/
/* $Id: adapter.h,v 1.1.2.2 2002/10/02 14:38:37 armin Exp $ */

#ifndef __DIVA_USER_MODE_IDI_ADAPTER_H__
#define __DIVA_USER_MODE_IDI_ADAPTER_H__

#define DIVA_UM_IDI_ADAPTER_REMOVED 0x00000001

typedef struct _diva_um_idi_adapter {
	diva_entity_link_t link;
	DESCRIPTOR d;
	int adapter_nr;
	diva_entity_queue_t entity_q;	/* entities linked to this adapter */
	diva_entity_queue_t delayed_cleanup_q;	/* entities linked to this adapter */
	dword status;
	dword nr_delayed_cleanup_entities;
} diva_um_idi_adapter_t;

#endif
