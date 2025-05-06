
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2022
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
