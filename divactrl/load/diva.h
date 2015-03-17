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
#ifndef __DIVA_UM_OS_API_H__
#define __DIVA_UM_OS_API_H__

#ifdef __cplusplus
extern "C" {
#endif

int divas_close_driver (dword handle);
int divas_get_card (int card_number);
int divas_get_card_properties (int card_number,
															 const char* name,
															 char* buffer,
															 int   size);
int divas_start_bootloader (int card_number);
int divas_ram_write (int card_number,
										 int ram_number,
										 const void* buffer,
										 dword offset,
										 dword length);
int divas_start_adapter (int card_number, dword start_address, dword features);
int divas_stop_adapter (int card_number);
int divas_write_fpga (int card_number,
										  int fpga_number,
										 	const void* buffer,
										 	dword length);
int divas_ram_read (int card_number,
										int ram_number,
										void* buffer,
										dword offset,
										dword length);
int divas_set_protocol_features (int card_number, dword features);
int divas_memory_test (int card_number, dword test_cmd);
int divas_set_protocol_code_version (int card_number, dword version);

#ifdef __cplusplus
}
#endif


#endif
