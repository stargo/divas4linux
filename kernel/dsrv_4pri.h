
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

#ifndef __DIVA_XDI_DSRV_4PRI_INC__
#define __DIVA_XDI_DSRV_4PRI_INC__
/*
 Functions exported from os dependent part of
 PRI card configuration and used in
 OS independed part
 */
/*
 Prepare OS dependent part of 4PRI functions
 */
void diva_os_prepare_4pri_functions (PISDN_ADAPTER IoAdapter);

#define DIVA_4PRI_REAL_SDRAM_SIZE (64*1024*1024)

#define DIVA_4PRI_PCIE(__x__) (((__x__)->cardType == CARDTYPE_DIVASRV_V4P_V10H_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V2P_V10H_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V1P_V10H_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V4P_V10Z_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V8P_V10Z_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_M4P_V10Z_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_M8P_V10Z_PCIE))



#endif
