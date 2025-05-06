
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

