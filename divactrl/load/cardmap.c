
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

#include "platform.h"
#include "cardtype.h"
#include <string.h>
#include "cardmap.h"

static int add_to_map (diva_card_map_entry_t** map,
											 dword cardType,
											 dword cardPort,
											 dword serialNumber) {
	diva_card_map_entry_t* m = malloc (sizeof(*m));

	if (m == 0) {
		diva_destroy_card_map (*map);
		return (-1);
	}

	memset (m, 0x00, sizeof(*m));

	m->cardType = cardType;
	m->cardPort = cardPort;
	m->serialNumber = serialNumber | (cardPort << 24);

	if (*map == 0) {
		*map = m;
	} else {
		diva_card_map_entry_t* i = *map;

		while (i->nextCard != 0) {
			i = (diva_card_map_entry_t*)i->nextCard;
		}
		i->nextCard = m;
	}

	return (0);
}


const diva_card_map_entry_t* diva_create_card_map (const char* info) {
	diva_card_map_entry_t* map = 0;

	while (info != 0 && *info != 0) {
		dword cardType, serialNumber;

		{
			const char* s = strchr (info, ',');
			const char* end = s != 0 ? strchr (s+1, ',') : 0;
			int length = (end != 0) ? (end - info) : strlen(info);
			char data[length+1];
			char* sn;

			if (s == 0)
				break;

			memcpy (data, info, length);
			data[length] = 0;
			sn = strchr(data, ',');
			*sn++ = 0;

			cardType = atoi(data);
			serialNumber = atoi(sn);

			info = (end != 0) ? (end+1) : 0;
		}

		if (cardType > 0 && cardType < CARDTYPE_MAX) {
			int i, controllers = CardProperties[cardType].Adapters;

			for (i = 0; i < controllers; i++) {
				if (add_to_map (&map, cardType, i, serialNumber) != 0)
					return (0);
			}
		}
	}

	return (map);
}

void diva_destroy_card_map (const diva_card_map_entry_t* cardMap) {
	while (cardMap != 0) {
		void *p = (void*)cardMap;

		cardMap = cardMap->nextCard;

		free (p);
	}
}

