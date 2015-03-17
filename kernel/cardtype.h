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
#ifndef _CARDTYPE_H_
#define _CARDTYPE_H_
#ifndef CARDTYPE_H_WANT_DATA
#define CARDTYPE_H_WANT_DATA            0
#endif
#ifndef CARDTYPE_H_WANT_IDI_DATA
#define CARDTYPE_H_WANT_IDI_DATA        0
#endif
#ifndef CARDTYPE_H_WANT_RESOURCE_DATA
#define CARDTYPE_H_WANT_RESOURCE_DATA   1
#endif
#ifndef CARDTYPE_H_WANT_FILE_DATA
#define CARDTYPE_H_WANT_FILE_DATA       1
#endif
#ifndef CARDTYPE_EXCLUDE_CARD_NAME
#define CARDTYPE_EXCLUDE_CARD_NAME 0
#define CARDTYPE_CARD_NAME(__x__) __x__,
#else
#define CARDTYPE_CARD_NAME(__x__)
#endif
/*
 * D-channel protocol identifiers
 *
 * Attention: Unfortunately the identifiers defined here differ from
 *            the identifiers used in Protocol/1/Common/prot/q931.h .
 *            The only reason for this is that q931.h has not a global
 *            scope and we did not know about the definitions there.
 *            But the definitions here cannot be changed easily because
 *            they are used in setup scripts and programs.
 *            Thus the definitions here have to be mapped if they are
 *            used in the protocol code context !
 *
 * Now the identifiers are defined in the q931lib/constant.h file.
 * Unfortunately this file has also not a global scope.
 * But beginning with PROTTYPE_US any new identifier will get the same
 * value as the corresponding PROT_* definition in q931lib/constant.h !
 */
#define PROTTYPE_MINVAL     0
#define PROTTYPE_ETSI       0
#define PROTTYPE_1TR6       1
#define PROTTYPE_BELG       2
#define PROTTYPE_FRANC      3
#define PROTTYPE_ATEL       4
#define PROTTYPE_NI         5  /* DMS 100, Nortel, National ISDN */
#define PROTTYPE_5ESS       6  /* 5ESS   , AT&T,   5ESS Custom   */
#define PROTTYPE_JAPAN      7
#define PROTTYPE_SWED       8
#define PROTTYPE_US         9  /* US autodetect */
#define PROTTYPE_ITALY      10
#define PROTTYPE_TWAN       11
#define PROTTYPE_AUSTRAL    12
#define PROTTYPE_4ESDN      13
#define PROTTYPE_4ESDS      14
#define PROTTYPE_4ELDS      15
#define PROTTYPE_4EMGC      16
#define PROTTYPE_4EMGI      17
#define PROTTYPE_HONGKONG   18
#define PROTTYPE_RBSCAS     19
#define PROTTYPE_CORNETN    20
#define PROTTYPE_QSIG       21
#define PROTTYPE_NI_EWSD    22 /* EWSD, Siemens, National ISDN   */
#define PROTTYPE_5ESS_NI    23 /* 5ESS, Lucent, National ISDN    */
#define PROTTYPE_T1CORNETN  24
#define PROTTYPE_CORNETNQ   25
#define PROTTYPE_T1CORNETNQ 26
#define PROTTYPE_T1QSIG     27
#define PROTTYPE_E1UNCH     28
#define PROTTYPE_T1UNCH     29
#define PROTTYPE_E1CHAN     30
#define PROTTYPE_T1CHAN     31
#define PROTTYPE_R2CAS      32
#define PROTTYPE_VN6        33 /* ETSI France 2003 incl. national Suppl. Services */
#define PROTTYPE_POTS       34
#define PROTTYPE_CORNETT    35
#define PROTTYPE_NEWZEALAND 36
#define PROTTYPE_SERBIA     37
#define PROTTYPE_LINESIDEE1 38
#define PROTTYPE_MAXVAL     38
/*
 * Card type identifiers
 */
#define CARD_UNKNOWN                      0
#define CARD_NONE                         0
        /* DIVA cards */
#define CARDTYPE_NONE                     0  // value not set, no hardware present
        // New software should not use define CARDTYPE_DIVA_MCA.
        // Instead, define CARDTYPE_NONE is valid.
#define CARDTYPE_DIVA_MCA                 0
#define CARDTYPE_DIVA_ISA                 1
#define CARDTYPE_DIVA_PCM                 2
#define CARDTYPE_DIVAPRO_ISA              3
#define CARDTYPE_DIVAPRO_PCM              4
#define CARDTYPE_DIVAPICO_ISA             5
#define CARDTYPE_DIVAPICO_PCM             6
        /* DIVA 2.0 cards */
#define CARDTYPE_DIVAPRO20_PCI            7
#define CARDTYPE_DIVA20_PCI               8
        /* S cards */
#define CARDTYPE_QUADRO_ISA               9
#define CARDTYPE_S_ISA                    10
#define CARDTYPE_S_MCA                    11
#define CARDTYPE_SX_ISA                   12
#define CARDTYPE_SX_MCA                   13
#define CARDTYPE_SXN_ISA                  14
#define CARDTYPE_SXN_MCA                  15
#define CARDTYPE_SCOM_ISA                 16
#define CARDTYPE_SCOM_MCA                 17
#define CARDTYPE_PR_ISA                   18
#define CARDTYPE_PR_MCA                   19
        /* Diva Server cards (formerly called Maestra, later Amadeo) */
#define CARDTYPE_MAESTRA_ISA              20
#define CARDTYPE_MAESTRA_PCI              21
        /* Diva Server cards to be developed (Quadro, Primary rate) */
#define CARDTYPE_DIVASRV_Q_8M_PCI         22
#define CARDTYPE_DIVASRV_P_30M_PCI        23
#define CARDTYPE_DIVASRV_P_2M_PCI         24
#define CARDTYPE_DIVASRV_P_9M_PCI         25
        /* DIVA 2.0 cards */
#define CARDTYPE_DIVA20_ISA               26
#define CARDTYPE_DIVA20U_ISA              27
#define CARDTYPE_DIVA20U_PCI              28
#define CARDTYPE_DIVAPRO20_ISA            29
#define CARDTYPE_DIVAPRO20U_ISA           30
#define CARDTYPE_DIVAPRO20U_PCI           31
        /* DIVA combi cards (piccola ISDN + rockwell V.34 modem) */
#define CARDTYPE_DIVAMOBILE_PCM           32
#define CARDTYPE_TDKGLOBALPRO_PCM         33
        /* DIVA Pro PC OEM card for 'New Media Corporation' */
#define CARDTYPE_NMC_DIVAPRO_PCM          34
        /* DIVA Pro 2.0 OEM cards for 'British Telecom' */
#define CARDTYPE_BT_EXLANE_PCI            35
#define CARDTYPE_BT_EXLANE_ISA            36
        /* DIVA low cost cards, 1st name DIVA 3.0, 2nd DIVA 2.01, 3rd ??? */
#define CARDTYPE_DIVALOW_ISA              37
#define CARDTYPE_DIVALOWU_ISA             38
#define CARDTYPE_DIVALOW_PCI              39
#define CARDTYPE_DIVALOWU_PCI             40
        /* DIVA combi cards (piccola ISDN + rockwell V.90 modem) */
#define CARDTYPE_DIVAMOBILE_V90_PCM       41
#define CARDTYPE_TDKGLOBPRO_V90_PCM       42
#define CARDTYPE_DIVASRV_P_23M_PCI        43
#define CARDTYPE_DIVALOW_USB              44
        /* DIVA Audio (CT) family */
#define CARDTYPE_DIVA_CT_ST               45
#define CARDTYPE_DIVA_CT_U                46
#define CARDTYPE_DIVA_CTLITE_ST           47
#define CARDTYPE_DIVA_CTLITE_U            48
        /* DIVA ISDN plus V.90 series */
#define CARDTYPE_DIVAISDN_V90_PCM         49
#define CARDTYPE_DIVAISDN_V90_PCI         50
#define CARDTYPE_DIVAISDN_TA              51
        /* DIVA Server Voice cards */
#define CARDTYPE_DIVASRV_VOICE_Q_8M_PCI   52
        /* DIVA Server V2 cards */
#define CARDTYPE_DIVASRV_Q_8M_V2_PCI      53
#define CARDTYPE_DIVASRV_P_30M_V2_PCI     54
        /* DIVA Server Voice V2 cards */
#define CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI 55
#define CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI 56
        /* Diva LAN */
#define CARDTYPE_DIVAISDN_LAN             57
#define CARDTYPE_DIVA_202_PCI_ST          58
#define CARDTYPE_DIVA_202_PCI_U           59
#define CARDTYPE_DIVASRV_B_2M_V2_PCI      60
#define CARDTYPE_DIVASRV_B_2F_PCI         61
#define CARDTYPE_DIVALOW_USBV2            62
#define CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI 63
#define CARDTYPE_DIVA_PRO_30_PCI_ST       64
#define CARDTYPE_DIVA_CT_ST_V20           65
/* Diva Mobile V.90 PC Card and Diva ISDN PC Card */
#define CARDTYPE_DIVAMOBILE_V2_PCM        66
#define CARDTYPE_DIVA_V2_PCM              67
/* Re-badged Diva Pro PC Card */
#define CARDTYPE_DIVA_PC_CARD             68
/* Renamed to Diva Pro 3.0 PC Card */
#define CARDTYPE_DIVA_PRO_PC_CARD_20      69
        /* DIVA Server V3.0 cards */
#define CARDTYPE_DIVASRV_P_30M_V30_PCI    70
#define CARDTYPE_DIVASRV_P_24M_V30_PCI    71
#define CARDTYPE_DIVASRV_P_8M_V30_PCI     72
#define CARDTYPE_DIVASRV_P_30V_V30_PCI    73
#define CARDTYPE_DIVASRV_P_24V_V30_PCI    74
#define CARDTYPE_DIVASRV_P_2V_V30_PCI     75
        /* virtual M-Adapter */
#define CARDTYPE_DIVASRV_MADAPTER_VIRTUAL 76
        /* Diva Server Analog cards */
#define CARDTYPE_DIVASRV_ANALOG_4PORT     77
#define CARDTYPE_DIVASRV_ANALOG_8PORT     78
        /* Diva Server 2/4PRI Cards */
#define CARDTYPE_DIVASRV_2P_V_V10_PCI     79  /* Diva Server V-2PRI/E1/T1 */
#define CARDTYPE_DIVASRV_IPMEDIA_300_V10  80  /* Diva Server IPM-300 */
#define CARDTYPE_DIVASRV_4P_V_V10_PCI     81  /* Diva Server V-4PRI/E1/T1 */
#define CARDTYPE_DIVASRV_IPMEDIA_600_V10  82  /* Diva Server IPM-600 */
#define CARDTYPE_DIVASRV_2P_M_V10_PCI     83  /* Diva Server 2PRI/E1/T1 */
#define CARDTYPE_DIVASRV_SOFTIP_V20       84  /* Diva Server SoftIP */
#define CARDTYPE_DIVASRV_4P_M_V10_PCI     85  /* Diva Server 4PRI/E1/T1 */
#define CARDTYPE_DIVASRV_dummy_86         86  /* unused */
        /* V version of Diva Server cards */
#define CARDTYPE_DIVASRV_V_B_2M_V2_PCI    87
#define CARDTYPE_DIVASRV_V_Q_8M_V2_PCI    88
#define CARDTYPE_DIVASRV_V_ANALOG_4PORT   89
#define CARDTYPE_DIVASRV_V_ANALOG_8PORT   90
#define CARDTYPE_DIVA_USB_V4              91
#define CARDTYPE_DIVASRV_ANALOG_2PORT     92
#define CARDTYPE_DIVASRV_V_ANALOG_2PORT   93
        /* PCI Express version of Diva Server PRI v.3 cards */
#define CARDTYPE_DIVASRV_P_30M_V30_PCIE   94 /* Diva Server PRI/E1-30 PCIe     */
#define CARDTYPE_DIVASRV_P_24M_V30_PCIE   95 /* Diva Server PRI/T1-24 PCIe     */
#define CARDTYPE_DIVASRV_P_30V_V30_PCIE   96 /* Diva Server V-PRI/E1-30 PCIe   */
#define CARDTYPE_DIVASRV_P_24V_V30_PCIE   97 /* Diva Server V-PRI/T1-24 PCIe   */
#define CARDTYPE_DIVASRV_P_2V_V30_PCIE    98 /* Diva Server PRI/E1/T1-CTI PCIe */
        /* 4BRI PCIe */
#define CARDTYPE_DIVASRV_4BRI_V1_PCIE     99  /* Diva 4BRI-8 PCIe v2 */
#define CARDTYPE_DIVASRV_4BRI_V1_V_PCIE   100 /* Diva UM-4BRI-8 PCIe v2 */
        
        /* BRI PCIe */
#define CARDTYPE_DIVASRV_BRI_V1_PCIE      101 /* Diva BRI-2 PCIe v2 */
#define CARDTYPE_DIVASRV_BRI_V1_V_PCIE    102 /* Diva UM-BRI-2 PCIe v2 */
        
#define CARDTYPE_DIVASRV_BRI_CTI_V2_PCI   103 /* BRI-CTI: 2FX without fax and modem */ 
        /* Analog PCIe  */
#define CARDTYPE_DIVASRV_ANALOG_2P_PCIE   104
#define CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE 105
#define CARDTYPE_DIVASRV_ANALOG_4P_PCIE   106
#define CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE 107
#define CARDTYPE_DIVASRV_ANALOG_8P_PCIE   108
#define CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE 109
        /* Hermes  */
#define CARDTYPE_DIVASRV_V4P_V10H_PCIE    110  /* Hermes Diva V-4PRI PCIe */
#define CARDTYPE_DIVASRV_V2P_V10H_PCIE    111  /* Hermes Diva V-2PRI PCIe */
#define CARDTYPE_DIVASRV_V1P_V10H_PCIE    112  /* Hermes Diva V-1PRI PCIe */
        /* Zeus  */
#define CARDTYPE_DIVASRV_V4P_V10Z_PCIE    113  /* Zeus Diva V-4PRI PCIe */
#define CARDTYPE_DIVASRV_V8P_V10Z_PCIE    114  /* Zeus Diva V-8PRI PCIe */
        /* Diva Unified Messaging Media Boards (UM boards) */
#define CARDTYPE_DIVASRV_P_30UM_V30_PCI   115  /* Diva UM-PRI/E1-30 */
#define CARDTYPE_DIVASRV_P_24UM_V30_PCI   116  /* Diva UM-PRI/T1-24 */
#define CARDTYPE_DIVASRV_P_30UM_V30_PCIE  117  /* Diva UM-PRI/E1-30 PCIe */
#define CARDTYPE_DIVASRV_P_24UM_V30_PCIE  118  /* Diva UM-PRI/T1-24 PCIe */
        /* Lancaster */
#define CARDTYPE_LANCASTER_BRI            119  /* Lancaster v1 (LEP) */
        /* next free card type identifier */
#define CARDTYPE_MAX                      120
/*
 * The card families
 */
#define FAMILY_DIVA         1       /* all Diva Client cards */
#define FAMILY_S            2       /* all S cards           */
#define FAMILY_MAESTRA      3       /* all Diva Server cards */
#define FAMILY_VIRTUAL      4       /* all virtual adapters  */
#define FAMILY_MAX          5
/*
 * The basic card types
 */
#define CARD_DIVA           1       /* DSP based, old DSP   */
#define CARD_PRO            2       /* DSP based, new DSP   */
#define CARD_PICO           3       /* HSCX based           */
#define CARD_S              4       /* IDI on board based   */
#define CARD_SX             5       /* IDI on board based   */
#define CARD_SXN            6       /* IDI on board based   */
#define CARD_SCOM           7       /* IDI on board based   */
#define CARD_QUAD           8       /* IDI on board based   */
#define CARD_PR             9       /* IDI on board based   */
#define CARD_MAE            10      /* IDI on board based   */
#define CARD_MAEQ           11      /* IDI on board based   */
#define CARD_MAEP           12      /* IDI on board based   */
#define CARD_DIVALOW        13      /* IPAC based           */
#define CARD_CT             14      /* SCOUT based          */
#define CARD_DIVATA         15      /* DIVA TA                        */
#define CARD_DIVALAN        16      /* DIVA LAN                       */
#define CARD_MAE2           17      /* IDI on board based             */
#define CARD_POTS           18      /* analog board with POTS ports   */
#define CARD_MTPX           19      /* M-Adapter                      */
#define CARD_MAEP3          20      /* All PRI/E1/T1 adapters         */
#define CARD_MAEP3V         21      /* All V-PRI/E1/T1 adapters       */
#define CARD_MAE2P          22      /* 2PRI adapters (Non-V)          */
#define CARD_MAE4P          23      /* 4PRI adapters (Non-V)          */
#define CARD_MAE2V          24      /* V version of the BRI 2.0 card  */
#define CARD_MAEQV          25      /* V version of the 4BRI 2.0 card */
#define CARD_POTSV          26      /* V version of the analog card   */
#define CARD_MAE2PV         27      /* V-2PRI adapters                */
#define CARD_MAE4PV         28      /* V-4PRI adapters                */
#define CARD_SOFTIP         29      /* Soft IP adapters               */
#define CARD_MAEP3UM        30      /* All UM-PRI/E1/T1 adapters      */
#define CARD_MAE1PHV        31      /* V-1PRI HS adapters             */
#define CARD_MAE2PHV        32      /* V-2PRI HS adapters             */
#define CARD_MAE4PHV        33      /* V-4PRI HS adapters             */
#define CARD_MAE4PFV        34      /* V-4PRI FS adapters             */
#define CARD_MAE8PFV        35      /* V-8PRI FS adapters             */
#define CARD_MAX            36
/*
 * The internal card types of the S family
 */
#define CARD_I_NONE         0
#define CARD_I_S            0
#define CARD_I_SX           1
#define CARD_I_SCOM         2
#define CARD_I_QUAD         3
#define CARD_I_PR           4
/*
 * The bus types we support
 */
#define BUS_NONE            0
#define BUS_ISA             1
#define BUS_PCM             2
#define BUS_PCI             3
#define BUS_MCA             4
#define BUS_USB             5
#define BUS_COM             6
#define BUS_LAN             7
#define BUS_SOFT            8
#define BUS_PCIE            9
/*
 * The chips we use for B channel traffic
 */
#define CHIP_NONE           0
#define CHIP_DSP            1
#define CHIP_HSCX           2
#define CHIP_IPAC           3
#define CHIP_SCOUT          4
#define CHIP_EXTERN         5
#define CHIP_IPACX          6
#define CHIP_HFC            7
#define CHIP_DSP_BF         8
/*
 * Maximum ram entity info structs per adapter
 */
#define E_INFO_PRI  256
#define E_INFO_BRI  64
#define E_INFO_BRI1 32
#define E_INFO_BRIS 16
/*
 * Card Subtype Id
 */
#define CSID_DIVASRV_2P_V_V10_PCI         0
#define CSID_DIVASRV_2P_M_V10_PCI         1
#define CSID_DIVASRV_4P_V_V10_PCI         0
#define CSID_DIVASRV_4P_M_V10_PCI         1
#define CSID_DIVASRV_4BRI_V1_PCIE         3 /* Diva 4BRI-8 PCIe v1 */
#define CSID_DIVASRV_4BRI_V1_V_PCIE       2 /* Diva V-4BRI-8 PCIe v1 */
#define CSID_DIVASRV_BRI_V1_PCIE          3 /* Diva BRI-2 PCIe v1 */
#define CSID_DIVASRV_BRI_V1_V_PCIE        2 /* Diva V-BRI-2 PCIe v1 */
#define CSID_DIVASRV_P_30M_V30_PCIE       3 /* Diva Server PRI/E1-30 PCIe     */
#define CSID_DIVASRV_P_24M_V30_PCIE       3 /* Diva Server PRI/T1-24 PCIe     */
#define CSID_DIVASRV_P_30V_V30_PCIE       2 /* Diva Server V-PRI/E1-30 PCIe   */
#define CSID_DIVASRV_P_24V_V30_PCIE       2 /* Diva Server V-PRI/T1-24 PCIe   */
#define CSID_DIVASRV_P_2V_V30_PCIE        2 /* Diva Server PRI/E1/T1-CTI PCIe */
#define CSID_DIVASRV_P_24UM_V30_PCIE      2 /* Diva Server UM-PRI/T1-24 PCIe  */
#define CSID_DIVASRV_P_30UM_V30_PCIE      2 /* Diva Server UM-PRI/E1-30 PCIe  */
#define CSID_FULL                         1 /* Full featured card */
#define CSID_PCIE                         2 /* PCIe card */
#define CSID_NONE                         0xff
#if CARDTYPE_H_WANT_IDI_DATA
/* include "di_defs.h" for IDI adapter type and feature flag definitions    */
#include "di_defs.h"
#else /*!CARDTYPE_H_WANT_IDI_DATA*/
/* define IDI adapter types and feature flags here to prevent inclusion     */
#ifndef IDI_ADAPTER_S
#define IDI_ADAPTER_S           1
#define IDI_ADAPTER_PR          2
#define IDI_ADAPTER_DIVA        3
#define IDI_ADAPTER_MAESTRA     4
#define IDI_MADAPTER            0x41
#define IDI_DRIVER              0x80
#endif
#ifndef DI_VOICE
#define DI_VOICE          0x0 /* obsolete define */
#define DI_FAX3           0x1
#define DI_MODEM          0x2
#define DI_POST           0x4
#define DI_V110           0x8
#define DI_V120           0x10
#define DI_POTS           0x20
#define DI_CODEC          0x40
#define DI_MANAGE         0x80
#define DI_V_42           0x0100
#define DI_EXTD_FAX       0x0200 /* Extended FAX (ECM, 2D, T.6, Polling) */
#define DI_AT_PARSER      0x0400 /* Build-in AT Parser in the L2 */
#define DI_VOICE_OVER_IP  0x0800 /* Voice over IP support */
#endif
#endif /*CARDTYPE_H_WANT_IDI_DATA*/
/*
 * The structures where the card properties are aggregated by id
 */
typedef struct CARD_PROPERTIES
{
#if !(CARDTYPE_EXCLUDE_CARD_NAME)
    char           *Name;       /* official marketing name                  */
#endif
    unsigned short  PnPId;      /* plug and play ID (for non PCMIA cards)   */
    unsigned short  Version;    /* major and minor version no of the card   */
    unsigned char   DescType;   /* card type to set in the IDI descriptor   */
    unsigned char   Family;     /* basic family of the card                 */
    unsigned short  Features;   /* features bits to set in the IDI desc.    */
    unsigned char   Card;       /* basic card type                          */
    unsigned char   IType;      /* internal type of S cards (read from ram) */
    unsigned char   Bus;        /* bus type this card is designed for       */
    unsigned char   Chip;       /* chipset used on card                     */
    unsigned char   Adapters;   /* number of adapters on card               */
    unsigned char   Channels;   /* # of channels per adapter                */
    unsigned short  E_info;     /* # of ram entity info structs per adapter */
    unsigned short  SizeIo;     /* size of IO window per adapter            */
    unsigned short  SizeMem;    /* size of memory window per adapter        */
    unsigned char   CsId;       /* Card Subtype Id                          */
    unsigned short  PnPId2;     /* plug and play ID (subtype)               */
    unsigned int    HardwareFeatures;
} CARD_PROPERTIES;
typedef struct CARD_RESOURCE
{   unsigned char   Int [10];
    unsigned short  IoFirst;
    unsigned short  IoStep;
    unsigned short  IoCnt;
    unsigned long   MemFirst;
    unsigned long   MemStep;
    unsigned short  MemCnt;
} CARD_RESOURCE;
/* test if the card of type 't' is a plug & play card */
#define IS_PNP(t) \
( \
    ( \
        CardProperties[t].Bus != BUS_ISA \
        && \
        CardProperties[t].Bus != BUS_MCA \
    ) \
    || \
    ( \
        CardProperties[t].Family != FAMILY_S \
        && \
        CardProperties[t].Card != CARD_DIVA \
    ) \
)
/* test if the card has any DSP chips on board */
#define IS_CHIP_DSP(t) \
( \
       CardProperties[t].Chip == CHIP_DSP \
    || CardProperties[t].Chip == CHIP_DSP_BF \
)
/* extract IDI Descriptor info for card type 't' (p == DescType/Features) */
#define IDI_PROP(t,p)   (CardProperties[t].p)
#define IDI_PROP_SAFE(t,p) (((t) < (CARDTYPE_MAX)) ? CardProperties[t].p : 0)
/*
  CARD_PROPERTIES HardwareFeatures
  */
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE 0x00000001
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO   0x00000002
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC         0x00000004
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO    0x00000008
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE    0x00000010
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6     0x00000020
#define DIVA_CARDTYPE_HARDWARE_PROPERTY_CUSTOM_PCIE 0x00000040
#if CARDTYPE_H_WANT_DATA
#define DI_V1x0         (DI_V110 | DI_V120)
#define DI_NULL         0x0000
#if defined(SOFT_DSP_SUPPORT)
#define SOFT_DSP_ADD_FEATURES  (DI_MODEM | DI_FAX3 | DI_AT_PARSER)
#else
#define SOFT_DSP_ADD_FEATURES  0
#endif
#if defined(SOFT_V110_SUPPORT)
#define DI_SOFT_V110  DI_V110
#else
#define DI_SOFT_V110  0
#endif
/*--- CardProperties [Index=CARDTYPE_....] ---------------------------------*/
#ifdef __cplusplus
extern
#endif
OSCONST CARD_PROPERTIES CardProperties [ ] =
{
{   //   0
    CARDTYPE_CARD_NAME("Diva MCA")                           0x6336, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3,
    CARD_DIVA,          CARD_I_NONE,    BUS_MCA,    CHIP_DSP,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   1
    CARDTYPE_CARD_NAME("Diva ISA")                           0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3,
    CARD_DIVA,          CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   2
    CARDTYPE_CARD_NAME("Diva/PCM")                           0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3,
    CARD_DIVA,          CARD_I_NONE,    BUS_PCM,    CHIP_DSP,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   3
    CARDTYPE_CARD_NAME("Diva PRO ISA")                       0x0031, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_PRO,           CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   4
    CARDTYPE_CARD_NAME("Diva PRO PC-Card")                   0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_PRO,           CARD_I_NONE,    BUS_PCM,    CHIP_DSP,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   5
    CARDTYPE_CARD_NAME("Diva PICCOLA ISA")                   0x0051, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_ISA,    CHIP_HSCX,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   6
    CARDTYPE_CARD_NAME("Diva PICCOLA PCM")                   0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_HSCX,
    1,  2,   0,            8,    0,     CSID_NONE,           0x0000,
    0x00000000
},
{   //   7
    CARDTYPE_CARD_NAME("Diva PRO 2.0 S/T PCI")               0xE001, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE001,
    0x00000000
},
{   //   8
    CARDTYPE_CARD_NAME("Diva 2.0 S/T PCI")                   0xE002, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | DI_POTS | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCI,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE002,
    0x00000000
},
{   //   9
    CARDTYPE_CARD_NAME("QUADRO ISA")                         0x0000, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_NULL,
    CARD_QUAD,          CARD_I_QUAD,    BUS_ISA,    CHIP_NONE,
    4,  2,   E_INFO_BRIS,  0,   0x800,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  10
    CARDTYPE_CARD_NAME("S ISA")                              0x0000, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_CODEC,
    CARD_S,             CARD_I_S,       BUS_ISA,    CHIP_NONE,
    1,  1,   E_INFO_BRIS,  0,   0x800,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  11
    CARDTYPE_CARD_NAME("S MCA")                              0x6A93, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_CODEC,
    CARD_S,             CARD_I_S,       BUS_MCA,    CHIP_NONE,
    1,  1,   E_INFO_BRIS,  16,  0x400,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  12
    CARDTYPE_CARD_NAME("SX ISA")                             0x0000, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_NULL,
    CARD_SX,            CARD_I_SX,      BUS_ISA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  0,   0x800,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  13
    CARDTYPE_CARD_NAME("SX MCA")                             0x6A93, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_NULL,
    CARD_SX,            CARD_I_SX,      BUS_MCA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  16,  0x400,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  14
    CARDTYPE_CARD_NAME("SXN ISA")                            0x0000, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_NULL,
    CARD_SXN,           CARD_I_SCOM,    BUS_ISA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  0,   0x800,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  15
    CARDTYPE_CARD_NAME("SXN MCA")                            0x6A93, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_NULL,
    CARD_SXN,           CARD_I_SCOM,    BUS_MCA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  16,  0x400,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  16
    CARDTYPE_CARD_NAME("SCOM ISA")                           0x0000, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_CODEC,
    CARD_SCOM,          CARD_I_SCOM,    BUS_ISA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  0,   0x800,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  17
    CARDTYPE_CARD_NAME("SCOM MCA")                           0x6A93, 0x0100,
    IDI_ADAPTER_S,      FAMILY_S,       DI_CODEC,
    CARD_SCOM,          CARD_I_SCOM,    BUS_MCA,    CHIP_NONE,
    1,  2,   E_INFO_BRIS,  16,  0x400,  CSID_NONE,           0x0000,
    0x00000000
},
{   //  18
    CARDTYPE_CARD_NAME("S2M ISA")                            0x0000, 0x0100,
    IDI_ADAPTER_PR,     FAMILY_S,       DI_NULL,
    CARD_PR,            CARD_I_PR,      BUS_ISA,    CHIP_NONE,
    1,  30,  E_INFO_PRI,   0,   0x4000, CSID_NONE,           0x0000,
    0x00000000
},
{   //  19
    CARDTYPE_CARD_NAME("S2M MCA")                            0x6ABB, 0x0100,
    IDI_ADAPTER_PR,     FAMILY_S,       DI_NULL,
    CARD_PR,            CARD_I_PR,      BUS_MCA,    CHIP_NONE,
    1,  30,  E_INFO_PRI,   16,  0x4000, CSID_NONE,           0x0000,
    0x00000000
},
{   //  20
    CARDTYPE_CARD_NAME("Diva Server BRI-2M ISA")             0x0041, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE,           CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   E_INFO_BRI1,  8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  21
    CARDTYPE_CARD_NAME("Diva Server BRI-2M PCI")             0xE010, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE,           CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI1,  8,   0,      CSID_NONE,           0xE010,
    0x00000000
},
{   //  22
    CARDTYPE_CARD_NAME("Diva Server 4BRI-8M PCI")            0xE012, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEQ,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE012,
    0x00000000
},
{   //  23
    CARDTYPE_CARD_NAME("Diva Server PRI-30M PCI")            0xE014, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  30,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE014,
    0x00000000
},
{   //  24
    CARDTYPE_CARD_NAME("Diva Server PRI-2M PCI")             0xE014, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  30,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE014,
    0x00000000
},
{   //  25
    CARDTYPE_CARD_NAME("Diva Server PRI-9M PCI")             0x0000, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  30,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  26
    CARDTYPE_CARD_NAME("Diva 2.0 S/T ISA")                   0x0071, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | DI_POTS | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_ISA,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  27
    CARDTYPE_CARD_NAME("Diva 2.0 U ISA")                     0x0091, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | DI_POTS | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_ISA,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  28
    CARDTYPE_CARD_NAME("Diva 2.0 U PCI")                     0xE004, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | DI_POTS | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCI,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE004,
    0x00000000
},
{   //  29
    CARDTYPE_CARD_NAME("Diva PRO 2.0 S/T ISA")               0x0061, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  30
    CARDTYPE_CARD_NAME("Diva PRO 2.0 U ISA")                 0x0081, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  31
    CARDTYPE_CARD_NAME("Diva PRO 2.0 U PCI")                 0xE003, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE003,
    0x00000000
},
{   //  32
    CARDTYPE_CARD_NAME("Diva MOBILE")                        0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  33
    CARDTYPE_CARD_NAME("TDK DFI3600")                        0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  34 (OEM version of 4 - "Diva PRO PC-Card")
    CARDTYPE_CARD_NAME("New Media ISDN")                     0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_PRO,           CARD_I_NONE,    BUS_PCM,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  35 (OEM version of 7 - "Diva PRO 2.0 S/T PCI")
    CARDTYPE_CARD_NAME("BT ExLane PCI")                      0xE101, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE101,
    0x00000000
},
{   //  36 (OEM version of 29 - "Diva PRO 2.0 S/T ISA")
    CARDTYPE_CARD_NAME("BT ExLane ISA")                      0x1061, 0x0200,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_POTS,
    CARD_PRO,           CARD_I_NONE,    BUS_ISA,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  37
    CARDTYPE_CARD_NAME("Diva 2.01 S/T ISA")                  0x00A1, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_ISA,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  38
    CARDTYPE_CARD_NAME("Diva 2.01 U ISA")                    0x00B1, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_ISA,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  39
    CARDTYPE_CARD_NAME("Diva 2.01 S/T PCI")                  0xE005, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_PCI,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE005,
    0x00000000
},
{   //  40
    CARDTYPE_CARD_NAME("Diva 2.01 U PCI")                    0x0000, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_PCI,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  41
    CARDTYPE_CARD_NAME("Diva MOBILE V.90")                   0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  42
    CARDTYPE_CARD_NAME("TDK DFI3600 V.90")                   0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_HSCX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  43
    CARDTYPE_CARD_NAME("Diva Server PRI-23M PCI")            0xE014, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  30,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE014,
    0x00000000
},
{   //  44
    CARDTYPE_CARD_NAME("Diva 2.01 S/T USB")                  0x1000, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_USB,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  45
    CARDTYPE_CARD_NAME("Diva CT S/T PCI")                    0xE006, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_CT,            CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   0,   0,      CSID_NONE,           0xE006,
    0x00000000
},
{   //  46
    CARDTYPE_CARD_NAME("Diva CT U PCI")                      0xE007, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_CT,            CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   0,   0,      CSID_NONE,           0xE007,
    0x00000000
},
{   //  47
    CARDTYPE_CARD_NAME("Diva CT Lite S/T PCI")               0xE008, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_CT,            CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   0,   0,      CSID_NONE,           0xE008,
    0x00000000
},
{   //  48
    CARDTYPE_CARD_NAME("Diva CT Lite U PCI")                 0xE009, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_CT,            CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   0,   0,      CSID_NONE,           0xE009,
    0x00000000
},
{   //  49
    CARDTYPE_CARD_NAME("Diva ISDN+V.90 PC Card")             0x8D8C, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_DIVALOW,   CARD_I_NONE,    BUS_PCM,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  50
    CARDTYPE_CARD_NAME("Diva ISDN+V.90 PCI")                 0xE00A, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120  | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_PCI,    CHIP_IPAC,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE00A,
    0x00000000
},
{   //  51 (DivaTA)
    CARDTYPE_CARD_NAME("Diva TA")                            0x0000, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V110 | DI_FAX3 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVATA,        CARD_I_NONE,    BUS_COM,    CHIP_EXTERN,
    1,  1,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  52 (Diva Server 4BRI-8M PCI adapter enabled for Voice)
    CARDTYPE_CARD_NAME("Diva Server Voice 4BRI-8M PCI")      0xE016, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM | DI_VOICE_OVER_IP,
    CARD_MAEQ,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE016,
    0x00000000
},
{   //  53 (Diva Server 4BRI 2.0 adapter)
    //CARDTYPE_CARD_NAME("Diva Server 4BRI-8M 2.0 PCI")        0xE013, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva 4BRI-8 PCI v2")        0xE013, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEQ,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE013,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  54 (Diva Server PRI 2.0 adapter)
    //CARDTYPE_CARD_NAME("Diva Server PRI 2.0 PCI")            0xE015, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI-30 PCI v2")        0xE015, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE015,
    0x00000000
},
{   //  55 (Diva Server 4BRI-8M 2.0 PCI adapter enabled for Voice)
    CARDTYPE_CARD_NAME("Diva Server Voice 4BRI-8M 2.0 PCI")  0xE017, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM | DI_VOICE_OVER_IP,
    CARD_MAEQ,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE017,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  56 (Diva Server PRI 2.0 PCI adapter enabled for Voice)
    CARDTYPE_CARD_NAME("Diva Server Voice PRI 2.0 PCI")      0xE019, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM | DI_VOICE_OVER_IP,
    CARD_MAEP,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE019,
    0x00000000
},
{   //  57 (DivaLan )                       no ID
    CARDTYPE_CARD_NAME("Diva LAN")                           0x0000, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V110 | DI_FAX3 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALAN,       CARD_I_NONE,    BUS_LAN,    CHIP_EXTERN,
    1,  1,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  58
    //CARDTYPE_CARD_NAME("Diva 2.02 PCI S/T")                  0xE00B, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva ISDN PCI 2.02")        0xE00B, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA, DI_V120 | SOFT_DSP_ADD_FEATURES | DI_SOFT_V110,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_PCI,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE00B,
    0x00000000
},
{   //  59
    CARDTYPE_CARD_NAME("Diva 2.02 PCI U")                    0xE00C, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_PCI,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,           0xE00C,
    0x00000000
},
{   //  60
    //CARDTYPE_CARD_NAME("Diva Server BRI-2M 2.0 PCI")         0xE018, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva BRI-2 PCI v2")         0xE018, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE2,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE018,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  61  (the previous name was Diva Server BRI-2F 2.0 PCI)
    //CARDTYPE_CARD_NAME("Diva Server 2FX")                    0xE01A, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva BRI-2FX PCI v2")       0xE01A, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM | DI_SOFT_V110,
    CARD_MAE2,          CARD_I_NONE,    BUS_PCI,    CHIP_IPACX,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE01A,
    0x00000000
},
{   //  62
    CARDTYPE_CARD_NAME(" Diva ISDN USB 2.0")                 0x1003, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_USB,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  63 (Diva Server BRI-2M 2.0 PCI adapter enabled for Voice)
    CARDTYPE_CARD_NAME("Diva Server Voice BRI-2M 2.0 PCI")   0xE01B, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM | DI_VOICE_OVER_IP,
    CARD_MAE2,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE01B,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  64
    //CARDTYPE_CARD_NAME("Diva Pro 3.0 PCI")                   0xE00D, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva Pro 3.0 PCI")          0xE00D, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_PRO,           CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   0,            0,   0,      CSID_NONE,           0xE00D,
    0x00000000
},
{   //  65
    CARDTYPE_CARD_NAME("Diva ISDN + CT 2.0")                 0xE00E, 0x0300,
    IDI_ADAPTER_DIVA   ,FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM | DI_CODEC,
    CARD_CT,            CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,   2,  E_INFO_BRI,   0,   0,      CSID_NONE,           0xE00E,
    0x00000000
},
{   //  66
    CARDTYPE_CARD_NAME("Diva Mobile V.90 PC Card")           0x8331, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  67
    CARDTYPE_CARD_NAME("Diva ISDN PC Card")                  0x8311, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120 | SOFT_DSP_ADD_FEATURES,
    CARD_PICO,          CARD_I_NONE,    BUS_PCM,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  68
    CARDTYPE_CARD_NAME("Diva ISDN PC Card")                  0x0000, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_PRO,           CARD_I_NONE,    BUS_PCM,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  69 rebranded, previously Diva Pro PC Card 2.0
    CARDTYPE_CARD_NAME("Diva Pro 3.0 PC Card")               0x2038, 0x0300,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_PRO,           CARD_I_NONE,    BUS_PCM,    CHIP_DSP,
    1,  2,   0,            8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  70 (Diva Server PRI 3.0 adapter, 30M)
    //CARDTYPE_CARD_NAME("Diva Server PRI/E1-30")              0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/E1-30 PCI v3")     0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C03,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  71 (Diva Server PRI 3.0 adapter, 24M)
    //CARDTYPE_CARD_NAME("Diva Server PRI/T1-24")              0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/T1-24 PCI v3")     0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C02,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  72 (Diva Server PRI 3.0 adapter, 8M)
    //CARDTYPE_CARD_NAME("Diva Server PRI/E1/T1-8")            0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/E1/T1-8 PCI v3")   0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C01,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  73 (Diva Server PRI 3.0 adapter, 30V)
    //CARDTYPE_CARD_NAME("Diva Server V-PRI/E1-30")            0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva V-PRI/E1-30 PCI v3")   0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAEP3V,        CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C06,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  74 (Diva Server PRI 3.0 adapter, 24V)
    //CARDTYPE_CARD_NAME("Diva Server V-PRI/T1-24")            0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva V-PRI/T1-24 PCI v3")   0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAEP3V,        CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C05,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  75 (Diva Server PRI 3.0 adapter, 0M)
    //CARDTYPE_CARD_NAME("Diva Server PRI/E1/T1")              0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/E1/T1-CTI PCI v3") 0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V120,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C04,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  76 (M-Adapter, Virtual multiplex adapter)
    CARDTYPE_CARD_NAME("Diva M-Adapter")                     0x0000, 0x0100,
    IDI_MADAPTER,       FAMILY_VIRTUAL, 0,
    CARD_MTPX,          CARD_I_NONE,    BUS_NONE,   CHIP_NONE,
    1,  120, 0xffff,       0,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  77 (Diva Server 4POTS)
    //CARDTYPE_CARD_NAME("Diva Server Analog-4P")              0xE024, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-4 PCI v1")      0xE024, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  4,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE024,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  78 (Diva Server 8POTS)
    //CARDTYPE_CARD_NAME("Diva Server Analog-8P")              0xE028, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-8 PCI v1")              0xE028, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  8,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE028,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  79
    //CARDTYPE_CARD_NAME("Diva Server V-2PRI/E1/T1")           0xE01E, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva V-2PRI/E1/T1-60 PCI v1") 0xE01E, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAE2PV,        CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    2,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_2P_V_V10_PCI, 0xE01E,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  80
    //CARDTYPE_CARD_NAME("Diva Server IPM-300")                0xE02A, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva IPM-300 PCI v1")       0xE02A, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAE2P,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    2,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE02A,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  81
    //CARDTYPE_CARD_NAME("Diva Server V-4PRI/E1/T1")           0xE020, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva V-4PRI/E1/T1-120 PCI v1") 0xE020, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAE4PV,        CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    4,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_4P_V_V10_PCI, 0xE020,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  82
    //CARDTYPE_CARD_NAME("Diva Server IPM-600")                0xE02C, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva IPM-600 PCI v1")       0xE02C, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAE4P,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    4,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0xE02C,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  83
    //CARDTYPE_CARD_NAME("Diva Server 2PRI/E1/T1")             0xE01E,     0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva 2PRI/E1/T1-60 PCI v1") 0xE01E,     0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE2P,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    2,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_2P_M_V10_PCI, 0x1E01,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  84
    CARDTYPE_CARD_NAME("Dialogic Diva softIP")                 0x0000, 0x0000,
    IDI_ADAPTER_MAESTRA,FAMILY_VIRTUAL, 0,
    CARD_SOFTIP,        CARD_I_NONE,    BUS_SOFT,   CHIP_NONE,
    1,  30,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  85
    //CARDTYPE_CARD_NAME("Diva Server 4PRI/E1/T1")             0xE020, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva 4PRI/E1/T1-120 PCI v1")  0xE020, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE4P,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP_BF,
    4,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_4P_M_V10_PCI, 0x2001,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  86
    CARDTYPE_CARD_NAME("unused")                             0x0000, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    0,                  CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  24,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  87
    //CARDTYPE_CARD_NAME("Diva Server V-BRI-2")                0xE018, 0x0200,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-BRI-2 PCI v2")       0xE018, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-BRI-2 PCI v2")      0xE018, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAE2V,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0x1800,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  88
    //CARDTYPE_CARD_NAME("Diva Server V-4BRI-8")               0xE013, 0x0200,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-4BRI-8 PCI v2")      0xE013, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-4BRI-8 PCI v2")     0xE013, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEQV,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0x1300,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  89
    //CARDTYPE_CARD_NAME("Diva Server V-Analog-4P")            0xE024, 0x0100,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-4 PCI v1")    0xE024, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-4 PCI v1")   0xE024, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  4,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x2400,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  90
    //CARDTYPE_CARD_NAME("Diva Server V-Analog-8P")            0xE028, 0x0100,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-8 PCI v1")    0xE028, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-8 PCI v1")   0xE028, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  8,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x2800,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  91
    //CARDTYPE_CARD_NAME("Diva ISDN USB 4.0")                  0x1005, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva ISDN USB v4")          0x1005, 0x0100,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA,    DI_V120,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_USB,    CHIP_HFC,
    1,  2,  0,             8,   0,      CSID_NONE,           0x0000,
    0x00000000
},
{   //  92
    //CARDTYPE_CARD_NAME("Diva Server Analog-2P")              0xE022, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-2 PCI v1")      0xE022, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE022,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  93
    //CARDTYPE_CARD_NAME("Diva Server V-Analog-2P")            0xE022, 0x0100,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-2 PCI v1")    0xE022, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-2 PCI v1")   0xE022, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  2,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x2200,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  94 Diva Server PRI/E1-30 PCIe
    //CARDTYPE_CARD_NAME("Diva Server PRI/E1-30 PCIe")         0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/E1-30 PCIe v3")    0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_30M_V30_PCIE, 0x1C03,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  95 Diva Server PRI/T1-24 PCIe
    //CARDTYPE_CARD_NAME("Diva Server PRI/T1-24 PCIe")         0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/T1-24 PCIe v3")    0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_24M_V30_PCIE, 0x1C02,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  96 Diva Server V-PRI/E1-30 PCIe
    //CARDTYPE_CARD_NAME("Diva Server V-PRI/E1-30 PCIe")       0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva V-PRI/E1-30 PCIe v3")  0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAEP3V,        CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_30V_V30_PCIE, 0x1C06,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  97 Diva Server V-PRI/T1-24 PCIe
    //CARDTYPE_CARD_NAME("Diva Server V-PRI/T1-24 PCIe")       0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva V-PRI/T1-24 PCIe v3")  0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAEP3V,        CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_24V_V30_PCIE, 0x1C05,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  98 Diva Server PRI/E1/T1-CTI PCIe
    //CARDTYPE_CARD_NAME("Diva Server PRI/E1/T1-CTI PCIe")     0xE01C, 0x0300,
    CARDTYPE_CARD_NAME("Dialogic Diva PRI/E1/T1-CTI PCIe v3") 0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V120,
    CARD_MAEP3,         CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_2V_V30_PCIE, 0x1C04,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
  DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  99
    //CARDTYPE_CARD_NAME("Diva 4BRI-8 PCIe v1")                0xE02E, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva 4BRI-8 PCIe v2")       0xE02E, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAEQ,          CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_DIVASRV_4BRI_V1_PCIE, 0xE02E,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE
},
{   //  100
    //CARDTYPE_CARD_NAME("Diva V-4BRI-8 PCIe v1")              0xE02E, 0x0200,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-4BRI-8 PCIe v2")     0xE02E, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-4BRI-8 PCIe v2")    0xE02E, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEQV,         CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    4,  2,   E_INFO_BRI,   8,   0,      CSID_DIVASRV_4BRI_V1_V_PCIE, 0x2E01,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE
},
{   //  101
    //CARDTYPE_CARD_NAME("Diva BRI-2 PCIe v1")                   0xE032, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva BRI-2 PCIe v2")          0xE032, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_FAX3 | DI_MODEM,
    CARD_MAE2,          CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_DIVASRV_BRI_V1_PCIE,      0xE032,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE
},
{   //  102
    //CARDTYPE_CARD_NAME("Diva V-BRI-2 PCIe v1")                 0xE032, 0x0200,
    //CARDTYPE_CARD_NAME("Dialogic Diva V-BRI-2 PCIe v2")        0xE032, 0x0200,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-BRI-2 PCIe v2")       0xE032, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAE2V,         CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_DIVASRV_BRI_V1_V_PCIE,    0x3201,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE
},
{   //  103 is a 2FX without fax and modem
    CARDTYPE_CARD_NAME("Dialogic Diva BRI-CTI PCI v2")         0xE034, 0x0200,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, 0,
    CARD_MAE2,          CARD_I_NONE,    BUS_PCI,    CHIP_IPACX,
    1,  2,   E_INFO_BRI,   8,   0,      CSID_NONE,           0xE034,
    0x00000000
},
{   //  104
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-2 PCIe v1")      0xE036, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  2,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE036,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  105
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-2 PCIe v1")    0xE036, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-2 PCIe v1")   0xE036, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  2,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x3601,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  106
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-4 PCIe v1")      0xE038, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  4,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE038,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  107
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-4 PCIe v1")    0xE038, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-4 PCIe v1")   0xE038, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  4,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x3801,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  108
    CARDTYPE_CARD_NAME("Dialogic Diva Analog-8 PCIe v1")      0xE03A, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_FAX3 | DI_MODEM,
    CARD_POTS,          CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  8,   E_INFO_PRI,   8,   0,      CSID_NONE,           0xE03A,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  109
    //CARDTYPE_CARD_NAME("Dialogic Diva V-Analog-8 PCIe v1")    0xE03A, 0x0100,
    CARDTYPE_CARD_NAME("Dialogic Diva UM-Analog-8 PCIe v1")   0xE03A, 0x0100,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_MODEM,
    CARD_POTSV,         CARD_I_NONE,    BUS_PCIE,    CHIP_DSP,
    1,  8,   E_INFO_PRI,   8,   0,      CSID_NONE,           0x3A01,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC | DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  110 Hermes
    CARDTYPE_CARD_NAME("Dialogic Diva V-4PRI PCIe HS v1") 0xE030, 0x0100,
    IDI_ADAPTER_MAESTRA, FAMILY_MAESTRA, 0,
    CARD_MAE4PHV,       CARD_I_NONE,    BUS_PCIE,    CHIP_DSP_BF,
    4,  31,  E_INFO_PRI,   8,   0,      CSID_PCIE, 0xE030,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  111 Hermes
    CARDTYPE_CARD_NAME("Dialogic Diva V-2PRI PCIe HS v1") 0xE030, 0x0100,
    IDI_ADAPTER_MAESTRA, FAMILY_MAESTRA, 0,
    CARD_MAE2PHV,       CARD_I_NONE,    BUS_PCIE,    CHIP_DSP_BF,
    2,  31,  E_INFO_PRI,   8,   0,      CSID_PCIE, 0x3001,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  112 Hermes
    CARDTYPE_CARD_NAME("Dialogic Diva V-1PRI PCIe HS v1") 0xE030, 0x0100,
    IDI_ADAPTER_MAESTRA, FAMILY_MAESTRA, 0,
    CARD_MAE1PHV,       CARD_I_NONE,    BUS_PCIE,    CHIP_DSP_BF,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_PCIE, 0x3002,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_SEAVILLE
},
{   //  113 Zeus
    CARDTYPE_CARD_NAME("Dialogic Diva V-4PRI PCIe FS v1") 0xE03C, 0x0100,
    IDI_ADAPTER_MAESTRA, FAMILY_MAESTRA, 0,
    CARD_MAE4PFV,       CARD_I_NONE,    BUS_PCIE,    CHIP_DSP_BF,
    4,  31,  E_INFO_PRI,   8,   0,      CSID_PCIE, 0xE03C,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_CUSTOM_PCIE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  114 Zeus
    CARDTYPE_CARD_NAME("Dialogic Diva V-8PRI PCIe FS v1") 0xE03C, 0x0100,
    IDI_ADAPTER_MAESTRA, FAMILY_MAESTRA, 0,
    CARD_MAE8PFV,       CARD_I_NONE,    BUS_PCIE,    CHIP_DSP_BF,
    8,  31,  E_INFO_PRI,   8,   0,      CSID_PCIE, 0x3C01,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_CUSTOM_PCIE |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_NO_1TR6
},
{   //  115
    CARDTYPE_CARD_NAME("Dialogic Diva UM-PRI/E1-30 PCI v3")   0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEP3UM,       CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C0D,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  116
    CARDTYPE_CARD_NAME("Dialogic Diva UM-PRI/T1-24 PCI v3")   0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEP3UM,       CARD_I_NONE,    BUS_PCI,    CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_NONE,           0x1C0E,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO | DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC
},
{   //  117
    CARDTYPE_CARD_NAME("Dialogic Diva UM-PRI/E1-30 PCIe v3")  0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEP3UM,       CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  31,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_30UM_V30_PCIE, 0x1C0D,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{   //  118
    CARDTYPE_CARD_NAME("Dialogic Diva UM-PRI/T1-24 PCIe v3")  0xE01C, 0x0300,
    IDI_ADAPTER_MAESTRA,FAMILY_MAESTRA, DI_V1x0 | DI_MODEM,
    CARD_MAEP3UM,       CARD_I_NONE,    BUS_PCIE,   CHIP_DSP,
    1,  24,  E_INFO_PRI,   8,   0,      CSID_DIVASRV_P_24UM_V30_PCIE, 0x1C0E,
    DIVA_CARDTYPE_HARDWARE_PROPERTY_TEMPERATURE | DIVA_CARDTYPE_HARDWARE_PROPERTY_FPGA_INFO |
    DIVA_CARDTYPE_HARDWARE_PROPERTY_DAC_INFO
},
{    //119
    CARDTYPE_CARD_NAME("Lancaster v1")           0x0000, 0x0000,
    IDI_ADAPTER_DIVA,   FAMILY_DIVA, 0,
    CARD_DIVALOW,       CARD_I_NONE,    BUS_NONE,    CHIP_IPACX,
    1,  2,   0,            8,   0,      CSID_NONE,      0x0000,
    0x00000000
}
} ;
#if CARDTYPE_H_WANT_RESOURCE_DATA
/*--- CardResource [Index=CARDTYPE_....]   ---------------------------(GEI)-*/
#ifdef __cplusplus
extern
#endif
OSCONST CARD_RESOURCE CardResource [ ] =  {
//          Interrupts                  IO-Address          Mem-Address
/* 0*/ { { 3,4,9,0,0,0,0,0,0,0      },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA MCA
/* 1*/ { { 3,4,9,10,11,12,0,0,0,0   },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA ISA
/* 2*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA PCMCIA
/* 3*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA PRO ISA
/* 4*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA PRO PCMCIA
/* 5*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA PICCOLA ISA
/* 6*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA PICCOLA PCMCIA
/* 7*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA PRO 2.0 PCI
/* 8*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.0 PCI
/* 9*/ { { 3,4,5,7,9,10,11,12,0,0   },   0x0,0x0,0,      0x80000,0x2000,64   }, // QUADRO ISA
/*10*/ { { 3,4,9,10,11,12,0,0,0,0   },   0x0,0x0,0,      0xc0000,0x2000,16   }, // S ISA
/*11*/ { { 3,4,9,0,0,0,0,0,0,0      },   0xc00,0x10,16,  0xc0000,0x2000,16   }, // S MCA
/*12*/ { { 3,4,9,10,11,12,0,0,0,0   },   0x0,0x0,0,      0xc0000,0x2000,16   }, // SX ISA
/*13*/ { { 3,4,9,0,0,0,0,0,0,0      },   0xc00,0x10,16,  0xc0000,0x2000,16   }, // SX MCA
/*14*/ { { 3,4,5,7,9,10,11,12,0,0   },   0x0,0x0,0,      0x80000,0x0800,256  }, // SXN ISA
/*15*/ { { 3,4,9,0,0,0,0,0,0,0      },   0xc00,0x10,16,  0xc0000,0x2000,16   }, // SXN MCA
/*16*/ { { 3,4,5,7,9,10,11,12,0,0   },   0x0,0x0,0,      0x80000,0x0800,256  }, // SCOM ISA
/*17*/ { { 3,4,9,0,0,0,0,0,0,0      },   0xc00,0x10,16,  0xc0000,0x2000,16   }, // SCOM MCA
/*18*/ { { 3,4,5,7,9,10,11,12,0,0   },   0x0,0x0,0,      0xc0000,0x4000,16   }, // S2M ISA
/*19*/ { { 3,4,9,0,0,0,0,0,0,0      },   0xc00,0x10,16,  0xc0000,0x4000,16   }, // S2M MCA
/*20*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // MAESTRA ISA
/*21*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // MAESTRA PCI
/*22*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // MAESTRA QUADRO ISA
/*23*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x20,2048,  0x0,0x0,0           }, // MAESTRA QUADRO PCI
/*24*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // MAESTRA PRIMARY ISA
/*25*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // MAESTRA PRIMARY PCI
/*26*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA 2.0 ISA
/*27*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA 2.0 /U ISA
/*28*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.0 /U PCI
/*29*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA PRO 2.0 ISA
/*30*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA PRO 2.0 /U ISA
/*31*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA PRO 2.0 /U PCI
/*32*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA MOBILE
/*33*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // TDK DFI3600 (same as DIVA MOBILE [32])
/*34*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // New Media ISDN (same as DIVA PRO PCMCIA [4])
/*35*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // BT ExLane PCI (same as DIVA PRO 2.0 PCI [7])
/*36*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // BT ExLane ISA (same as DIVA PRO 2.0 ISA [29])
/*37*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA 2.01 S/T ISA
/*38*/ { { 3,5,7,9,10,11,12,14,15,0 },   0x200,0x20,16,  0x0,0x0,0           }, // DIVA 2.01 U ISA
/*39*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.01 S/T PCI
/*40*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.01 U PCI
/*41*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA MOBILE V.90
/*42*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // TDK DFI3600 V.90 (same as DIVA MOBILE V.90 [39])
/*43*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x20,2048,  0x0,0x0,0           }, // DIVA Server PRI-23M PCI
/*44*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA 2.01 S/T USB
/*45*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA CT S/T PCI
/*46*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA CT U PCI
/*47*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA CT Lite S/T PCI
/*48*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA CT Lite U PCI
/*49*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA ISDN+V.90 PC Card
/*50*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA ISDN+V.90 PCI
/*51*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA TA
/*52*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x20,2048,  0x0,0x0,0           }, // MAESTRA VOICE QUADRO PCI
/*53*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x20,2048,  0x0,0x0,0           }, // MAESTRA VOICE QUADRO PCI
/*54*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // MAESTRA VOICE PRIMARY PCI
/*55*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x20,2048,  0x0,0x0,0           }, // MAESTRA VOICE QUADRO PCI
/*56*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // MAESTRA VOICE PRIMARY PCI
/*57*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA LAN
/*58*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.02 S/T PCI
/*59*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 2.02 U PCI
/*60*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server BRI-2M 2.0 PCI
/*61*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server BRI-2F PCI
/*62*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA 2.01 S/T USB
/*63*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server Voice BRI-2M 2.0 PCI
/*64*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA 3.0 PCI
/*65*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA CT S/T PCI V2.0
/*66*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA Mobile V.90 PC Card
/*67*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // DIVA ISDN PC Card
/*68*/ { { 3,4,5,7,9,10,11,12,14,15 },   0x0,0x8,8192,   0x0,0x0,0           }, // DIVA ISDN PC Card
/*69*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Pro PC Card 2.0
/*70*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server PRI/E1-30
/*71*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server PRI/T1-24
/*72*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server PRI/E1/T1-8
/*73*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-PRI/E1-30
/*74*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-PRI/T1-24
/*75*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server PRI/E1/T1
/*76*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // M-Adapter
/*77*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server Analog 4-Ports
/*78*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server Analog 8-Ports
/*79*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-2PRI/E1-60
/*80*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-2PRI/T1-48
/*81*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-4PRI/E1-120
/*82*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-4PRI/T1-96
/*83*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server 2PRI/E1-60
/*84*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server 2PRI/T1-48
/*85*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server 4PRI/E1-120
/*86*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server 4PRI/T1-96
/*87*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-BRI-2M 2.0 PCI
/*88*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-4BRI-8M 2.0 PCI
/*89*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 4-Ports
/*90*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 8-Ports
/*91*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva ISDN USB V 4.0
/*92*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server Analog 2-Ports
/*93*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*94*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*95*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*96*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*97*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*98*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Diva Server V-Analog 2-Ports
/*99*/ { { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*100*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*101*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*102*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*103*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // BRI-CTI PCI v2
/*104*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Analog PCIe
/*105*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*106*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*107*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*108*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*109*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*110*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Hermes
/*111*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*112*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*113*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // Zeus
/*114*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*115*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           }, // UM boards
/*116*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*117*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*118*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           },
/*119*/{ { 0,0,0,0,0,0,0,0,0,0      },   0x0,0x0,0,      0x0,0x0,0           } // DIVA LEP BRI-PCI
};
#endif /*CARDTYPE_H_WANT_RESOURCE_DATA*/
#else /*!CARDTYPE_H_WANT_DATA*/
extern OSCONST CARD_PROPERTIES      CardProperties [] ;
extern OSCONST CARD_RESOURCE        CardResource [] ;
#endif /*CARDTYPE_H_WANT_DATA*/
/*
 * all existing download files
 */
#define CARD_DSP_CNT        5
#define CARD_PROT_CNT       9
#define CARD_FT_UNKNOWN     0
#define CARD_FT_B           1
#define CARD_FT_D           2
#define CARD_FT_S           3
#define CARD_FT_M           4
#define CARD_FT_NEW_DSP_COMBIFILE 5  /* File format of new DSP code (the DSP code powered by Telindus) */
#define CARD_FILE_NONE      0
#define CARD_B_S            1
#define CARD_B_P            2
#define CARD_D_K1           3
#define CARD_D_K2           4
#define CARD_D_H            5
#define CARD_D_V            6
#define CARD_D_M            7
#define CARD_D_F            8
#define CARD_P_S_E          9
#define CARD_P_S_1          10
#define CARD_P_S_B          11
#define CARD_P_S_F          12
#define CARD_P_S_A          13
#define CARD_P_S_N          14
#define CARD_P_S_5          15
#define CARD_P_S_J          16
#define CARD_P_SX_E         17
#define CARD_P_SX_1         18
#define CARD_P_SX_B         19
#define CARD_P_SX_F         20
#define CARD_P_SX_A         21
#define CARD_P_SX_N         22
#define CARD_P_SX_5         23
#define CARD_P_SX_J         24
#define CARD_P_SY_E         25
#define CARD_P_SY_1         26
#define CARD_P_SY_B         27
#define CARD_P_SY_F         28
#define CARD_P_SY_A         29
#define CARD_P_SY_N         30
#define CARD_P_SY_5         31
#define CARD_P_SY_J         32
#define CARD_P_SQ_E         33
#define CARD_P_SQ_1         34
#define CARD_P_SQ_B         35
#define CARD_P_SQ_F         36
#define CARD_P_SQ_A         37
#define CARD_P_SQ_N         38
#define CARD_P_SQ_5         39
#define CARD_P_SQ_J         40
#define CARD_P_P_E          41
#define CARD_P_P_1          42
#define CARD_P_P_B          43
#define CARD_P_P_F          44
#define CARD_P_P_A          45
#define CARD_P_P_N          46
#define CARD_P_P_5          47
#define CARD_P_P_J          48
#define CARD_P_M_E          49
#define CARD_P_M_1          50
#define CARD_P_M_B          51
#define CARD_P_M_F          52
#define CARD_P_M_A          53
#define CARD_P_M_N          54
#define CARD_P_M_5          55
#define CARD_P_M_J          56
#define CARD_P_S_S          57
#define CARD_P_SX_S         58
#define CARD_P_SY_S         59
#define CARD_P_SQ_S         60
#define CARD_P_P_S          61
#define CARD_P_M_S          62
#define CARD_D_NEW_DSP_COMBIFILE 63
typedef struct CARD_FILES_DATA
{
    char *              Name;
    unsigned char       Type;
}
CARD_FILES_DATA;
typedef struct CARD_FILES
{
    unsigned char       Boot;
    unsigned char       Dsp  [CARD_DSP_CNT];
    unsigned char       DspTelindus;
    unsigned char       Prot [CARD_PROT_CNT];
}
CARD_FILES;
#if CARDTYPE_H_WANT_DATA
#if CARDTYPE_H_WANT_FILE_DATA
CARD_FILES_DATA CardFData [] =  {
//  Filename            Filetype
  { 0,              CARD_FT_UNKNOWN           },
  { "didnload.bin", CARD_FT_B                 },
  { "diprload.bin", CARD_FT_B                 },
  { "didiva.bin",   CARD_FT_D                 },
  { "didivapp.bin", CARD_FT_D                 },
  { "dihscx.bin",   CARD_FT_D                 },
  { "div110.bin",   CARD_FT_D                 },
  { "dimodem.bin",  CARD_FT_D                 },
  { "difax.bin",    CARD_FT_D                 },
  { "di_etsi.bin",  CARD_FT_S                 },
  { "di_1tr6.bin",  CARD_FT_S                 },
  { "di_belg.bin",  CARD_FT_S                 },
  { "di_franc.bin", CARD_FT_S                 },
  { "di_atel.bin",  CARD_FT_S                 },
  { "di_ni.bin",    CARD_FT_S                 },
  { "di_5ess.bin",  CARD_FT_S                 },
  { "di_japan.bin", CARD_FT_S                 },
  { "di_etsi.sx",   CARD_FT_S                 },
  { "di_1tr6.sx",   CARD_FT_S                 },
  { "di_belg.sx",   CARD_FT_S                 },
  { "di_franc.sx",  CARD_FT_S                 },
  { "di_atel.sx",   CARD_FT_S                 },
  { "di_ni.sx",     CARD_FT_S                 },
  { "di_5ess.sx",   CARD_FT_S                 },
  { "di_japan.sx",  CARD_FT_S                 },
  { "di_etsi.sy",   CARD_FT_S                 },
  { "di_1tr6.sy",   CARD_FT_S                 },
  { "di_belg.sy",   CARD_FT_S                 },
  { "di_franc.sy",  CARD_FT_S                 },
  { "di_atel.sy",   CARD_FT_S                 },
  { "di_ni.sy",     CARD_FT_S                 },
  { "di_5ess.sy",   CARD_FT_S                 },
  { "di_japan.sy",  CARD_FT_S                 },
  { "di_etsi.sq",   CARD_FT_S                 },
  { "di_1tr6.sq",   CARD_FT_S                 },
  { "di_belg.sq",   CARD_FT_S                 },
  { "di_franc.sq",  CARD_FT_S                 },
  { "di_atel.sq",   CARD_FT_S                 },
  { "di_ni.sq",     CARD_FT_S                 },
  { "di_5ess.sq",   CARD_FT_S                 },
  { "di_japan.sq",  CARD_FT_S                 },
  { "di_etsi.p",    CARD_FT_S                 },
  { "di_1tr6.p",    CARD_FT_S                 },
  { "di_belg.p",    CARD_FT_S                 },
  { "di_franc.p",   CARD_FT_S                 },
  { "di_atel.p",    CARD_FT_S                 },
  { "di_ni.p",      CARD_FT_S                 },
  { "di_5ess.p",    CARD_FT_S                 },
  { "di_japan.p",   CARD_FT_S                 },
  { "di_etsi.sm",   CARD_FT_M                 },
  { "di_1tr6.sm",   CARD_FT_M                 },
  { "di_belg.sm",   CARD_FT_M                 },
  { "di_franc.sm",  CARD_FT_M                 },
  { "di_atel.sm",   CARD_FT_M                 },
  { "di_ni.sm",     CARD_FT_M                 },
  { "di_5ess.sm",   CARD_FT_M                 },
  { "di_japan.sm",  CARD_FT_M                 },
  { "di_swed.bin",  CARD_FT_S                 },
  { "di_swed.sx",   CARD_FT_S                 },
  { "di_swed.sy",   CARD_FT_S                 },
  { "di_swed.sq",   CARD_FT_S                 },
  { "di_swed.p",    CARD_FT_S                 },
  { "di_swed.sm",   CARD_FT_M                 },
  { "didspdld.bin", CARD_FT_NEW_DSP_COMBIFILE }
};
CARD_FILES CardFiles [] =
{
  { /* CARD_UNKNOWN */
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  },
  { /* CARD_DIVA */
    CARD_FILE_NONE,
    {
      CARD_D_K1, CARD_D_H, CARD_D_V, CARD_FILE_NONE, CARD_D_F
    },
    CARD_D_NEW_DSP_COMBIFILE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  },
  { /* CARD_PRO  */
    CARD_FILE_NONE,
    {
      CARD_D_K2, CARD_D_H, CARD_D_V, CARD_D_M, CARD_D_F
    },
    CARD_D_NEW_DSP_COMBIFILE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  },
  { /* CARD_PICO */
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  },
  { /* CARD_S    */
    CARD_B_S,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_S_E, CARD_P_S_1, CARD_P_S_B, CARD_P_S_F,
      CARD_P_S_A, CARD_P_S_N, CARD_P_S_5, CARD_P_S_J,
      CARD_P_S_S
    }
  },
  { /* CARD_SX   */
    CARD_B_S,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_SX_E, CARD_P_SX_1, CARD_P_SX_B, CARD_P_SX_F,
      CARD_P_SX_A, CARD_P_SX_N, CARD_P_SX_5, CARD_P_SX_J,
      CARD_P_SX_S
    }
  },
  { /* CARD_SXN    */
    CARD_B_S,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_SY_E, CARD_P_SY_1, CARD_P_SY_B, CARD_P_SY_F,
      CARD_P_SY_A, CARD_P_SY_N, CARD_P_SY_5, CARD_P_SY_J,
      CARD_P_SY_S
    }
  },
  { /* CARD_SCOM */
    CARD_B_S,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_SY_E, CARD_P_SY_1, CARD_P_SY_B, CARD_P_SY_F,
      CARD_P_SY_A, CARD_P_SY_N, CARD_P_SY_5, CARD_P_SY_J,
      CARD_P_SY_S
    }
  },
  { /* CARD_QUAD */
    CARD_B_S,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_SQ_E, CARD_P_SQ_1, CARD_P_SQ_B, CARD_P_SQ_F,
      CARD_P_SQ_A, CARD_P_SQ_N, CARD_P_SQ_5, CARD_P_SQ_J,
      CARD_P_SQ_S
    }
  },
  { /* CARD_PR   */
    CARD_B_P,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_P_P_E, CARD_P_P_1, CARD_P_P_B, CARD_P_P_F,
      CARD_P_P_A, CARD_P_P_N, CARD_P_P_5, CARD_P_P_J,
      CARD_P_P_S
    }
  },
  { /* CARD_MAE  */
    CARD_FILE_NONE,
    {
      CARD_D_K2, CARD_D_H, CARD_D_V, CARD_D_M, CARD_D_F
    },
    CARD_D_NEW_DSP_COMBIFILE,
    {
      CARD_P_M_E, CARD_P_M_1, CARD_P_M_B, CARD_P_M_F,
      CARD_P_M_A, CARD_P_M_N, CARD_P_M_5, CARD_P_M_J,
      CARD_P_M_S
    }
  },
  { /* CARD_MAEQ */       /* currently not supported */
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  },
  { /* CARD_MAEP */       /* currently not supported */
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE
    },
    CARD_FILE_NONE,
    {
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE, CARD_FILE_NONE,
      CARD_FILE_NONE
    }
  }
};
#endif /*CARDTYPE_H_WANT_FILE_DATA*/
#else /*!CARDTYPE_H_WANT_DATA*/
extern CARD_FILES_DATA      CardFData [] ;
extern CARD_FILES           CardFiles [] ;
#endif /*CARDTYPE_H_WANT_DATA*/
#endif /* _CARDTYPE_H_ */
