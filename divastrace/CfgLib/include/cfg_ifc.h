
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
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

#if !defined(__DIVA_UM_CFG_LIB_INTERFACE_H__)
#define __DIVA_UM_CFG_LIB_INTERFACE_H__

struct _diva_cfg_lib_access_ifc;
struct _diva_cfg_lib_access_ifc* diva_um_cfg_lib_get_cfg_interface (dword owner);

#if defined(__cplusplus)
extern "C" {
#endif
dword diva_um_cfg_lib_message_input (struct _diva_cfg_lib_access_ifc* ifc, const byte* data);
struct _diva_io_object_user_enum_ifc* diva_um_cfg_lib_init (dword owner,
                                                            struct _diva_cfg_lib_access_ifc** ifc);
struct _diva_io_object_user_enum_ifc* diva_um_cfg_lib_registry_init (dword owner,
                                                                     const char* base,
                                                                     struct _diva_cfg_lib_access_ifc** ifc);
#if defined(__cplusplus)
}
#endif

#endif
