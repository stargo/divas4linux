
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

# ifndef RNA_IF___H
# define RNA_IF___H

/*
 * RNA helper interface definitions
 */

#if !defined(UNIX)
# include "typedefs.h"
#endif
# include "ppp_if.h"

# define MAX_RNA_STREAM_BUFFER	3200	/* enough to escape all 1500 bytes    */

typedef struct RNA_DATA {
	byte		Framing;	/* current RNA framing type	*/
	byte		Patches;	/* flags to patch RNA bugs	*/
	byte		align[2];	/* align to dword 		*/
	PPP_CONFIG	Ppp;	 	/* current PPP configuration	*/
	PPP_STREAM	Explode;	/* incoming stream conversion	*/
	PPP_STREAM	Implode;	/* outgoing stream conversion	*/
} RNA_DATA;

#if !defined(UNIX)
word rnaWrite	(struct ISDN_PORT *P, byte **Data, word sizeData,
				      byte *Frame, word sizeFrame);

word rnaRecv	(struct ISDN_PORT *P, byte **Data, word sizeData,
				      byte *Stream, word sizeStream);

int  rnaUp	(struct ISDN_PORT *P);
#endif

# endif /* RNA_IF___H */
