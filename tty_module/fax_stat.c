
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
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

#include "platform.h"
#include "fax_stat.h"


#define DIVA_FAX_STAT_BINS 15
#define DIVA_FAX_CLASS_2_RET_CODES 120 /* 0 ... 119 */

typedef struct _diva_fax_statistics {
	dword bins[DIVA_FAX_STAT_BINS+1];

	dword cls2_ret_code_bins[DIVA_FAX_CLASS_2_RET_CODES+1];
	dword cls2_ret_code_unknown;

	dword	total_calls;

} diva_fax_speed_statistics_t;

static diva_fax_speed_statistics_t diva_fax_statistics;

void diva_update_fax_speed_statistics (word  speed, word return_code) {
	int i = speed/2400;

	if (i > (DIVA_FAX_STAT_BINS-1))
		i = DIVA_FAX_STAT_BINS - 1;

	diva_fax_statistics.total_calls++;
	diva_fax_statistics.bins[i]++;

	if (return_code >= DIVA_FAX_CLASS_2_RET_CODES) {
		diva_fax_statistics.cls2_ret_code_unknown++;
	} else {
		diva_fax_statistics.cls2_ret_code_bins[return_code]++;
	}
}

void diva_clear_fax_speed_statistics (void) {
	int i;

	diva_fax_statistics.total_calls = 0;
	for (i = 0; i < DIVA_FAX_STAT_BINS; i++) {
		diva_fax_statistics.bins[i] = 0;
	}

	diva_fax_statistics.cls2_ret_code_unknown = 0;

	for (i = 0; i < DIVA_FAX_CLASS_2_RET_CODES; i++) {
		diva_fax_statistics.cls2_ret_code_bins[i] = 0;
	}
}

byte diva_get_fax_speed_statistics_percent (word speed) {
	int i = speed/2400;

	if (i > (DIVA_FAX_STAT_BINS-1))
		i = DIVA_FAX_STAT_BINS - 1;

	if (!diva_fax_statistics.total_calls) {
		return (0);
	}

	return ((byte)((diva_fax_statistics.bins[i]*100)/diva_fax_statistics.total_calls));
}

dword diva_get_fax_speed_statistics_total  (int speed) {
	int i = speed/2400;

	if (speed < 0) {
		return (diva_fax_statistics.total_calls);
	}

	if (i > (DIVA_FAX_STAT_BINS-1))
		i = DIVA_FAX_STAT_BINS - 1;


	return (diva_fax_statistics.bins[i]);
}

byte diva_fax_get_fax_cls2_err_statistics_percent (word return_code) {
	dword value;

	if (!diva_fax_statistics.total_calls) {
		return (0);
	}

	if (return_code >= DIVA_FAX_CLASS_2_RET_CODES) {
		value = diva_fax_statistics.cls2_ret_code_unknown;
	} else {
		value = diva_fax_statistics.cls2_ret_code_bins[return_code];
	}

	return ((byte)((value*100)/diva_fax_statistics.total_calls));
}

dword diva_fax_get_fax_cls2_err_statistics_total (word return_code) {
	if (return_code >= DIVA_FAX_CLASS_2_RET_CODES) {
		return (diva_fax_statistics.cls2_ret_code_unknown);
	} else {
		return (diva_fax_statistics.cls2_ret_code_bins[return_code]);
	}
}

