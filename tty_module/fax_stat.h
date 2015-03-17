
/*
 *
  Copyright (c) Dialogic(R), 2009.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic(R) File Revision :    2.1
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

#ifndef __DIVA_FAX_STAT__
#define __DIVA_FAX_STAT__

void diva_update_fax_speed_statistics      (word  speed, word return_code);
void diva_clear_fax_speed_statistics       (void);

byte diva_get_fax_speed_statistics_percent (word speed);
dword diva_get_fax_speed_statistics_total  (int speed);

byte  diva_fax_get_fax_cls2_err_statistics_percent (word return_code);
dword diva_fax_get_fax_cls2_err_statistics_total   (word return_code);

#define DIVA_FAX_CLASS_2_RET_CODES 120 /* 0 ... 119 */

#endif

