
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

# ifndef ATP_IF___H
# define ATP_IF___H

/*
 * AT command processor interface definitions
 */
#if !defined(UNIX)
# include "typedefs.h"
void atInit	(struct ISDN_PORT *P, int NewPort);
else
void atInit	(struct ISDN_PORT *P, int NewPort, byte *Address, int Profile);
#endif

void atSetup	(struct ISDN_PORT *P, int Profile);
void atFinit	(struct ISDN_PORT *P, int Detach);
void atWrite	(struct ISDN_PORT *P, byte *Data, word sizeData);
int  atEscape	(struct ISDN_PORT *P, byte *Data, word sizeData);
void atRsp	(struct ISDN_PORT *P, int Rc);
void atRspStr	(struct ISDN_PORT *P, byte *Str);
int  atRspPop	(struct ISDN_PORT *P);
void atRspFlush	(struct ISDN_PORT *P);
void atRspPurge	(struct ISDN_PORT *P);
void atRingOff	(struct ISDN_PORT *P);
void atDtrOff	(struct ISDN_PORT *P);
int  atDial	(struct ISDN_PORT *P, byte **Arg);
int  atAnswer	(struct ISDN_PORT *P);
void atDrop	(struct ISDN_PORT *P, byte Cause);
void atShut	(struct ISDN_PORT *P, byte Cause);
int  atScan	(struct ISDN_PORT *P, byte *Data, word sizeData);

byte *atState	(byte State);
int  atNumArg   (byte **Arg);
void atProfile	(struct AT_DATA *At, int Profile);
void atNewMode  (struct ISDN_PORT *P, struct AT_DATA *At);
int  atPlusI	(struct ISDN_PORT *P, struct AT_DATA *At, byte **Arg);

/* Command line scan status						*/

# define AT_INIT	0	/* initial state at begin of line			*/
# define AT_LOOK	1	/* saw 'a', looking for 't' or '/'			*/
# define AT_SCAN	2	/* saw "at", scan for '"' or '\r'			*/

/* Command line scan status codes returned by atScan()				*/

# define AT_SCAN_ERROR	0	/* bad syntax or line too long			*/
# define AT_SCAN_MORE	1	/* line not complete, need more data	*/
# define AT_SCAN_DOLAST	2	/* saw "A\", i.e. repeat last line		*/
# define AT_SCAN_DOTHIS	3	/* saw "AT...\r", i.e a complete line	*/

/* Numeric response codes expected by atRespond()					*/

# define R_VOID	       -1	/* private: accepted, no response now	*/
# define R_OK			0	/* command accepted					*/
# define R_CONNECT_300	1	/* connected at 300 baud (fax only)	*/
# define R_RING			2	/* incoming call					*/
# define R_NO_CARRIER	3	/* arbitrary disconnect messages	*/
# define R_ERROR		4	/* command line errors				*/
# define R_CONNECT_1200	5	/* connected at 1200 baud (unused)	*/
# define R_NO_DIALTONE	6	/* no silence when silence expected	*/
# define R_BUSY			7	/* remote station busy				*/
# define R_NO_ANSWER	8	/* remote station not responding	*/
# define R_CONNECT		9	/* connection established			*/
# define R_CONNECT_GEN 10	/* private, generic response code	*/

/* ISDN indications handled by AT processor, never call directly */

int  portUp	(void *C, void *hP, ISDN_CONN_INFO *Info);
int  portDown	(void *C, void *hP, byte Cause, byte q931_cause);
void portIdle	(void *C, void *hP);
void *portRing	(void *C, ISDN_CONN_PARMS *Parms);

# endif /* ATP_IF___H */
