
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

#ifndef __DIVA_XDI_CARD_CONFIG_HELPERS_INC__
#define __DIVA_XDI_CARD_CONFIG_HELPERS_INC__
dword diva_get_protocol_file_features  (byte* File,
                      int offset,
                      char *IdStringBuffer,
                      dword IdBufferSize);
void diva_configure_protocol (PISDN_ADAPTER IoAdapter, dword ramSize);
/*
 Low level file access system abstraction
 */
/* -------------------------------------------------------------------------
  Access to single file
  Return pointer to the image of the requested file,
  write image length to 'FileLength'
  ------------------------------------------------------------------------- */
void *xdiLoadFile (char *FileName, dword *FileLength, dword MaxLoadSize) ;
/* -------------------------------------------------------------------------
  Dependent on the protocol settings does read return pointer
  to the image of appropriate protocol file
  ------------------------------------------------------------------------- */
void *xdiLoadArchive (PISDN_ADAPTER IoAdapter, dword *FileLength, dword MaxLoadSize) ;
/* --------------------------------------------------------------------------
  Free all system resources accessed by xdiLoadFile and xdiLoadArchive
  -------------------------------------------------------------------------- */
void xdiFreeFile (void* handle);
#endif
