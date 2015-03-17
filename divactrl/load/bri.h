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
#ifndef __DIVA_UM_BRI_REV_1_CONFIG_H__
#define __DIVA_UM_BRI_REV_1_CONFIG_H__

#define SHAREDM_SEG ((byte)((BRI_MEMORY_BASE+BRI_MEMORY_SIZE-BRI_SHARED_RAM_SIZE)>>16))
int divas_bri_create_image (byte* sdram,
                            int prot_fd,
                            int dsp_fd,
													  const char* ProtocolName,
														int protocol_id,
														dword* protocol_features,
														dword* max_download_address);


#endif
