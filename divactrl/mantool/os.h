/*------------------------------------------------------------------------------
 *
 * (c) COPYRIGHT 1999-2007       Dialogic Corporation
 *
 * ALL RIGHTS RESERVED
 *
 * This software is the property of Dialogic Corporation and/or its
 * subsidiaries ("Dialogic"). This copyright notice may not be removed,
 * modified or obliterated without the prior written permission of
 * Dialogic.
 *
 * This software is a Trade Secret of Dialogic and may not be
 * copied, transmitted, provided to or otherwise made available to any company,
 * corporation or other person or entity without written permission of
 * Dialogic.
 *
 * No right, title, ownership or other interest in the software is hereby
 * granted or transferred. The information contained herein is subject
 * to change without notice and should not be construed as a commitment of
 * Dialogic.
 *
 *------------------------------------------------------------------------------*/
#ifndef __OS__H__
#define __OS__H__

int diva_open_adapter (int adapter_nr);
int diva_close_adapter (int handle);
int diva_put_req (int handle, const void* data, int length);
int diva_get_message (int handle, void* data, int max_length);
int diva_wait_idi_message (int entity);
unsigned long GetTickCount (void);
void diva_wait_idi_message_and_stdin (int entity);
int diva_wait_idi_message_interruptible (int entity, int interruptible);

#define LOBYTE(__x__) ((byte)(__x__))
#define HIBYTE(__x__) ((byte)(((word)(__x__)) >> 8))

#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif
