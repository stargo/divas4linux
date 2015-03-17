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
#ifndef __DIVA_UM_DEBUG_ACCESS_H__
#define __DIVA_UM_DEBUG_ACCESS_H__

/*
  Message format:

  Byte 0       - operation register
  Byte 1,2,3,4 - default debug mask
  Byte 5       - description of driver / application
                  driver name (zero terminated) - mandatory
                  driver tag (zero terminated) - mandatory
                  management path of debug mask variable (zero terminated ) - optional
                  management ID (logical adapter number of misc drivers) - optional

  Byte 0       - operation set debug mask
  Byte 1,2,3,4 - debug mask

  Byte 0   - operation write
  Byte 1,2 - DLI_... value
  Byte 4   - First byte of the parameter
  */

#define DIVA_UM_IDI_TRACE_CMD_REGISTER 0x00
#define DIVA_UM_IDI_TRACE_CMD_SET_MASK 0x01
#define DIVA_UM_IDI_TRACE_CMD_WRITE    0x02


#endif

