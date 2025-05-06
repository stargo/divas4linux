
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

# ifndef BTX_IF___H
# define BTX_IF___H

/*
 * BTX helper interface definitions
 */

#if !defined(UNIX)
# include "typedefs.h"
#endif

# define BTX_MAX_PAGE	2048
# define BTX_MAX_HOLD	256

typedef struct BTX_DATA {
	byte	State;
	word	nextAck;
	byte	Page[BTX_MAX_PAGE];
	word	sizePage;
	word	sizeData;
	word	nextData;
	byte	Hold[BTX_MAX_HOLD];
	word	sizeHold;
} BTX_DATA;

#if !defined(UNIX)
word btxWrite	(struct ISDN_PORT *P, byte **Data, word sizeData,
				      byte *Frame, word sizeFrame);

word btxRecv	(struct ISDN_PORT *P, byte **Data, word sizeData,
				      byte *Stream, word sizeStream);

int  btxUp	(struct ISDN_PORT *P);
#endif

# endif /* BTX_IF___H */
