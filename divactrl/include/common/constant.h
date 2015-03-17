
/*
 *
  Copyright (c) Eicon Networks, 2005.
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
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
#ifndef CONSTANT_H_INCLUDED  /* { */
#define CONSTANT_H_INCLUDED
/*------------------------------------------------------------------*/
/* Q.931 information elements maximum length                        */
/* excluding the identifier, including the length field             */
/*------------------------------------------------------------------*/
#define MAX_LEN_BC      13
#define MAX_LEN_LLC     19 /* ctr3 */
#define MAX_LEN_HLC     6  /* ctr3 */
#define MAX_LEN_UUI     255 /* Hicom USBS req */
#define MAX_LEN_NUM     24
#define MAX_LEN_DSP     83 /* ctr3 */
#define MAX_LEN_NI      128
#define MAX_LEN_PI      5
#define MAX_LEN_SIN     3
#define MAX_LEN_CST     4
#define MAX_LEN_SIG     2
#define MAX_LEN_SPID    32
#define MAX_LEN_EID     3
#define MAX_LEN_CHI     35  /* ctr3 */
#define MAX_LEN_CAU     33
#define MAX_LEN_FTY     255
#define MAX_LEN_KEY     83  /* ctr3 */
#define MAX_LEN_RSI     4
#define MAX_LEN_CAI     11
#define MAX_NUM_SPID    8
#define MAX_NUM_CA      20 /* number of US Call Appearances per L2 link (SPID) */
#define MAX_LEN_USERID  9
#define MAX_LEN_APPLID  5
#define MAX_LEN_NTTCIF  15
#define MAX_LEN_ASSIGN_CAI 256
#define MAX_LEN_NL_ASSIGN_LLC 3
#define MAX_LEN_FI  5
#define MAX_LEN_CALL_APP 3
#define MAX_LEN_DSP_US  129
#define MAX_LEN_CPNAME  34
#define MAX_LEN_QSIG_STRING 256
#define MAX_LEN_CHI_INTERFACE_IDENT 20
#define MAX_LEN_SIPADDR 255
/*------------------------------------------------------------------*/
/* decision return values                                           */
/*------------------------------------------------------------------*/
#define YES             1
#define NO              0
/*-------------------------------------------------------------------*/
/* w element coding                                                  */
/*-------------------------------------------------------------------*/
#define SMSG            0x00
#define NTTCIF          0x01
#define CP_CATEGORY     0x01 /* codeset 7, used in DTAG network */
#define BC              0x04
#define CAU             0x08
#define CAD             0x0c
#define CAI             0x10
#define CST             0x14
#define CHI             0x18
#define LLI             0x19
#define CHA             0x1a
#define FTY             0x1c
#define PI              0x1e
#define NFAC            0x20
#define TC              0x24
#define ATT_EID         0x26
#define NI              0x27
#define DSP             0x28
#define DT              0x29
#define DSP_US          0x2a
#define KEY             0x2c
#define KP              0x2c
#define UID             0x2d
#define SIG             0x34
#define OTHER_CR_5ESS   0x37
#define FR              0x38
#define FI              0x39
#define SPID            0x3a
#define EID             0x3b
#define DSPF            0x3c
#define ECAD            0x4c
#define OAD             0x6c
#define OSA             0x6d
#define DAD             0x70
#define CPN             0x70
#define DSA             0x71
#define RDX             0x73
#define RAD             0x74
#define RDN             0x74   /* redirecting number               */
#define RIN             0x76   /* redirection number               */
#define IUP             0x76   /* VN6 rerouter->PCS (codeset 6)    */
#define IPU             0x77   /* VN6 PCS->rerouter (codeset 6)    */
#define RSI             0x79
#define SCR             0x7A   /* internal unscreened CPN          */
#define MIE             0x7a   /* internal management info element */
#define CALL_APP        0x7b   /* US Call Appearance  */
#define LLC             0x7c
#define HLC             0x7d
#define UUI             0x7e
#define ESC             0x7f
#if defined(CORNETN_SUPPORT) || defined(QSIG_SUPPORT)
/* Codeset 7 for Cornet-N */
#define PNSF   0x01
#endif
#ifdef CORNETN_SUPPORT
#define CHARGEINFO  0x02
#define CLASSMARKS  0x03
#define PIN    0x04
#define CUSERNUMBER     0x05 /* mobile user number */
#define CUSERNAME       0x0e /* mobile user name */
#define CRDIONPNAME  0x61 /* redirection partyname */
#define CRDINGPNAME  0x63 /* redirecting partyname */
#define CEDPNAME  0x67 /* Called party name */
#define CINPNAME  0x73 /* Calling party name */
#define CPNAME   0x7c /* Connected party name */
#endif
#define SHIFT           0x90
#define MORE            0xa0
#define SDNCMPL         0xa1
#define CL              0xb0
/* information elements used on the spid interface */
#define SPID_CMD        0xc0
#define SPID_LINK       0x10
#define SPID_DN         0x70
#define SPID_BC         0x04
#define SPID_SWITCH     0x11
/* US text tags with the display text i.e. */
#define CALLED_ADDRESS     0x83
#define CAUSE              0x84
#define PROGRESS_IND       0x85
#define NOTIFICATION_IND   0x86
#define FEATURE_STATUS     0x89
#define CALLING_ADDRESS    0x8b
#define RDN_REASON         0x8c
#define CALLING_PARTY_NAME 0x8d
#define CALLED_PARTY_NAME  0x8e
#define ORG_CALL_NAME      0x8f
#define RDN_NAME           0x90
#define CONNECTED_NAME     0x91
#define FEATURE_ADDRESS    0x95
#define RDIN_NAME          0x96
#define RDION_NUMBER       0x97
#define RDN_NUMBER         0x98
#define ORG_CLD_NUMBER     0x99
#define CONNECTED_NUMBER   0x9a
#define DISP_TEXT          0x9e
/* US 5ESSCustom text tags */
#define CALL_APPEARANCE_ID5 0x01
#define CALLED_PARTY_IDENT5 0x02
#define CLLING_PARTY_IDENT5 0x03
#define CALLED_PARTY_NAME5  0x04
#define CLLING_PARTY_NAME5  0x05
#define ORIGINATING_PERM5   0x06
#define CALL_IDEN5          0x07
#define CALL_INFO5          0x08
#define ENTIRE_DISPLAY5     0x09
#define DATE_TIME5          0x0A
/*------------------------------------------------------------------*/
/* global configuration parameters, defined in exec.c               */
/* these parameters are configured with program loading             */
/*------------------------------------------------------------------*/
#define PROT_1TR6       0
#define PROT_ETSI       1
#define PROT_FRANC      2
#define PROT_BELG       3
#define PROT_SWED       4
#define PROT_NI         5 /* default DMS100 Northern Telecom */
#define PROT_5ESS       6 /* default 5ESS_CUSTOM AT&T        */
#define PROT_JAPAN      7
#define PROT_ATEL       8
#define PROT_US         9
#define PROT_ITALY      10
#define PROT_TWAN       11
#define PROT_AUSTRAL    12
#define PROT_4ESS_SDN   13 /* AT&T Software Defined Network */
#define PROT_4ESS_SDS   14 /* AT&T Switched Digital Service */
#define PROT_4ESS_LDS   15 /* AT&T Long Distance Service    */
#define PROT_4ESS_MGC   16 /* AT&T Megacom                  */
#define PROT_4ESS_MGI   17 /* AT&T Megacom International    */
#define PROT_HONGKONG   18
#define PROT_RBSCAS     19
#define PROT_CORNETN    20
#define PROT_QSIG       21
#define PROT_NI_EWSD    22
#define PROT_5ESS_NI    23 /* 5ESS switch National ISDN Lucent */
#define PROT_T1CORNETN  24
#define PROT_CORNETNQ   25
#define PROT_T1CORNETNQ 26
#define PROT_T1QSIG     27
#define PROT_E1UNCH     28 /* Unchannelized E1 (PRI E1 no D-Ch, single partial or full Hyperchannel) */
#define PROT_T1UNCH     29 /* Unchannelized T1 (PRI T1 no D-Ch, single partial or full Hyperchannel) */
#define PROT_E1CHAN     30 /* Channelized E1 (PRI E1 no D-Ch, multible groups with one or more B-ch) */
#define PROT_T1CHAN     31 /* Channelized T1 (PRI T1 no D-Ch, multible groups with one or more B-ch) */
#define PROT_R2CAS      32
#define PROT_VN6        33 /* ETSI Basic Call, French National&Matra Supplementary Services */
#define PROT_POTS       34 /* TE-Analog frontend card */
#define PROT_CORNETT    35
#define PROT_NEWZEALAND 36
#define INIT_PROT_1TR6    0x80|PROT_1TR6
#define INIT_PROT_ETSI    0x80|PROT_ETSI
#define INIT_PROT_FRANC   0x80|PROT_FRANC
#define INIT_PROT_BELG    0x80|PROT_BELG
#define INIT_PROT_SWED    0x80|PROT_SWED
#define INIT_PROT_NI      0x80|PROT_NI
#define INIT_PROT_5ESS    0x80|PROT_5ESS
#define INIT_PROT_JAPAN   0x80|PROT_JAPAN
#define INIT_PROT_ATEL    0x80|PROT_ATEL
#define INIT_PROT_ITALY   0x80|PROT_ITALY
#define INIT_PROT_TWAN    0x80|PROT_TWAN
#define INIT_PROT_AUSTRAL 0x80|PROT_AUSTRAL
#define INIT_PROT_4ESS_SDN 0x80|PROT_4ESS_SDN
#define INIT_PROT_4ESS_SDS 0x80|PROT_4ESS_SDS
#define INIT_PROT_4ESS_LDS 0x80|PROT_4ESS_LDS
#define INIT_PROT_4ESS_MGC 0x80|PROT_4ESS_MGC
#define INIT_PROT_HONGKONG 0x80|PROT_HONGKONG
#define INIT_PROT_RBSCAS   0x80|PROT_RBSCAS
#define INIT_PROT_CORNETN  0x80|PROT_CORNETN
#define INIT_PROT_QSIG     0x80|PROT_QSIG
#define INIT_PROT_NI_EWSD  0x80|PROT_NI_EWSD
#define INIT_PROT_NI_NORTHT 0x80|PROT_NI_NORTHT
#define INIT_PROT_5ESS_CUST 0x80|PROT_5ESS_CUST
#define INIT_PROT_T1CORNETN 0x80|PROT_T1CORNETN
#define INIT_PROT_CORNETNQ 0x80|PROT_CORNETNQ
#define INIT_PROT_T1CORNETNQ 0x80|PROT_T1CORNETNQ
#define INIT_PROT_T1QSIG     0x80|PROT_T1QSIG
#define INIT_PROT_E1UNCH     0x80|PROT_E1UNCH
#define INIT_PROT_T1UNCH     0x80|PROT_T1UNCH
#define INIT_PROT_E1CHAN     0x80|PROT_E1CHAN
#define INIT_PROT_T1CHAN     0x80|PROT_T1CHAN
#define INIT_PROT_R2CAS      0x80|PROT_R2CAS
#define INIT_PROT_VN6        0x80|PROT_VN6
#define INIT_PROT_POTS       0x80|PROT_POTS
#define INIT_PROT_CORNETT    0x80|PROT_CORNETT
#define INIT_PROT_NEWZEALAND    0x80|PROT_NEWZEALAND
#define PROT_STR_VN6        "TE_VN6"
#define PROT_STR_1TR6       "TE_1TR6"
#define PROT_STR_ETSI       "TE_ETSI"
#define PROT_STR_FRANC      "TE_FRANC"
#define PROT_STR_BELG       "TE_BELG"
#define PROT_STR_SWED       "TE_SWED"
#define PROT_STR_NI         "TE_NI"
#define PROT_STR_5ESS       "TE_5ESS"
#define PROT_STR_JAPAN      "TE_JAPAN"
#define PROT_STR_ATEL       "TE_ATEL"
#define PROT_STR_ITALY      "TE_ITALY"
#define PROT_STR_TWAN       "TE_TWAN"
#define PROT_STR_AUSTRAL    "TE_AUSTRAL"
#define PROT_STR_4ESS_SDN   "TE_4ESS_SDN"
#define PROT_STR_4ESS_SDS   "TE_4ESS_SDS"
#define PROT_STR_4ESS_LDS   "TE_4ESS_LDS"
#define PROT_STR_4ESS_MGC   "TE_4ESS_MGC"
#define PROT_STR_HONGKONG   "TE_HONGKONG"
#define PROT_STR_RBSCAS     "TE_RBSCAS"
#define PROT_STR_CORNETN    "TE_CORNETN"
#define PROT_STR_QSIG       "TE_QSIG"
#define PROT_STR_NI_EWSD    "TE_US_EWSD"
#define PROT_STR_NI_NORTHT  "TE_NORTHERN_T"
#define PROT_STR_5ESS_CUST  "TE_5ESS_CUSTOM"
#define PROT_STR_T1CORNETN  "TE_T1CORNETN"
#define PROT_STR_CORNETNQ "TE_CORNETNQ"
#define PROT_STR_T1CORNETNQ "TE_T1CORNETNQ"
#define PROT_STR_T1QSIG     "TE_T1QSIG"
#define PROT_STR_E1UNCH     "TE_E1_UNCHAN"
#define PROT_STR_T1UNCH     "TE_T1_UNCHAN"
#define PROT_STR_E1CHAN     "TE_E1_CHAN"
#define PROT_STR_T1CHAN     "TE_T1_CHAN"
#define PROT_STR_R2CAS      "TE_R2CAS"
#define PROT_STR_POTS       "TE_POTS"
#define PROT_STR_CORNETT    "TE_CORNETT"
#define PROT_STR_NEWZEALAND "TE_NEWZEALAND"
#define PROT_R2_CN1     0
#define PROT_R2_INDIA   1
#define PROT_R2_INDONESIA   2
#define PROT_R2_THAILAND    3
#define PROT_R2_PHILIPPINES 4
#define PROT_R2_BRAZIL      5
#define PROT_R2_MEXICO      6
#define PROT_R2_ITU_T       100
#endif /* CONSTANT_H_INCLUDED  } */
