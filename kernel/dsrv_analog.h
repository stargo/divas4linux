
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

#ifndef __DIVA_XDI_DSRV_ANALOG_INC__
#define __DIVA_XDI_DSRV_ANALOG_INC__
/*
 Functions exported from os dependent part of
 PRI card configuration and used in
 OS independed part
 */
/*
 Prepare OS dependent part of ANALOG functions
 */
void diva_os_prepare_analog_functions (PISDN_ADAPTER IoAdapter);
int analog_FPGA_download (PISDN_ADAPTER IoAdapter);

#define DIVA_ANALOG_PCIE(__x__) (((__x__)->cardType == CARDTYPE_DIVASRV_ANALOG_2P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_ANALOG_4P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_ANALOG_8P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM))

#define DIVA_ANALOG_PCIE_REAL_SDRAM_SIZE (64*1024*1024)

#endif
