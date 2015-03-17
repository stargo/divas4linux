
/*
 *
  Copyright (c) Dialogic, 2007.
 *
  This source file is supplied for the use with
  Dialogic range of DIVA Server Adapters.
 *
  Dialogic File Revision :    2.1
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
#ifndef __DIVA_XDI_DSRV_4_BRI_INC__
#define __DIVA_XDI_DSRV_4_BRI_INC__
/*
 * Some special registers in the PLX 9054
 */
#define PLX9054_P2LDBELL    0x60
#define PLX9054_L2PDBELL    0x64
#define PLX9054_INTCSR      0x69
#define PLX9054_INTCSR_DW   0x68
#define DIVA_PCIE_PLX_MSIACK 0x124
#define PLX9054_MABR0       0xac
#define PLX9054_LBRD0       0x18
#define PLX9054_INT_ENABLE  0x09
#define PLX9054_L2PDBELL_INT_ENABLE 0x00000300
#define PLX9054_LOCAL_INT_ENABLE    0x00010000
#define PLX9054_LOCAL_INT_INPUT_ENABLE 0x00000800
#define PLX9054_L2PDBELL_INT_PENDING 0x00002000
#define PLX9054_SOFT_RESET 0x4000
#define PLX9054_RELOAD_EEPROM 0x2000

#define DIVA_4BRI_PCIE(__x__) (((__x__)->cardType == CARDTYPE_DIVASRV_4BRI_V1_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_4BRI_V1_V_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_BRI_V1_PCIE) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_BRI_V1_V_PCIE))

#define DIVA_4BRI_REVISION(__x__) (((__x__)->cardType == CARDTYPE_DIVASRV_Q_8M_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_B_2M_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_B_2F_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_BRI_CTI_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_B_2M_V2_PCI) \
 || ((__x__)->cardType == CARDTYPE_DIVASRV_V_Q_8M_V2_PCI) \
 || DIVA_4BRI_PCIE(__x__))

void diva_os_set_qBri_functions (PISDN_ADAPTER IoAdapter);
void diva_os_set_qBri2_functions (PISDN_ADAPTER IoAdapter);
byte* qBri_check_FPGAsrc (PISDN_ADAPTER IoAdapter, char *FileName, dword *Length, dword *code);
#endif
