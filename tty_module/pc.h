
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

#ifndef PC_H_INCLUDED
#define PC_H_INCLUDED

/*------------------------------------------------------------------*/
/* buffer definition                                                */
/*------------------------------------------------------------------*/

typedef struct {
  word length;          /* length of data/parameter field           */
  byte P[270];          /* data/parameter field                     */
} PBUFFER;

/*------------------------------------------------------------------*/
/* dual port ram structure                                          */
/*------------------------------------------------------------------*/

struct dual
{
  byte Req;             /* request register                         */
  byte ReqId;           /* request task/entity identification       */
  byte Rc;              /* return code register                     */
  byte RcId;            /* return code task/entity identification   */
  byte Ind;             /* Indication register                      */
  byte IndId;           /* Indication task/entity identification    */
  byte IMask;           /* Interrupt Mask Flag                      */
  byte RNR;             /* Receiver Not Ready (set by PC)           */
  byte XLock;           /* XBuffer locked Flag                      */
  byte Int;             /* ISDN-S interrupt                         */
  byte ReqCh;           /* Channel field for layer-3 Requests       */
  byte RcCh;            /* Channel field for layer-3 Returncodes    */
  byte IndCh;           /* Channel field for layer-3 Indications    */
  byte MInd;            /* more data indication field               */
  word MLength;         /* more data total packet length            */
  byte ReadyInt;        /* request field for ready interrupt        */
  byte SWReg;           /* Software register for special purposes   */
  byte Reserved[11];    /* reserved space                           */
  byte InterfaceType;   /* interface type 1=16K interface           */
  word Signature;       /* ISDN-S adapter Signature (GD)            */
  PBUFFER XBuffer;      /* Transmit Buffer                          */
  PBUFFER RBuffer;      /* Receive Buffer                           */
};

/*------------------------------------------------------------------*/
/* SWReg Values (0 means no command)                                */
/*------------------------------------------------------------------*/
#define SWREG_DIE_WITH_LEDON  0x01
#define SWREG_HALT_CPU        0x02 /* Push CPU into a while(1) loop */

/*------------------------------------------------------------------*/
/* Id Fields Coding                                                 */
/*------------------------------------------------------------------*/

#define IDI_ID_MASK 0xe0 /* Mask for the ID field                   */
#define GL_ERR_ID 0x1f  /* ID for error reporting on global requests*/

#define DSIG_ID  0x00   /* ID for D-channel signaling               */
#define NL_ID    0x20   /* ID for network-layer access (B or D)     */
#define BLLC_ID  0x60   /* ID for B-channel link level access       */
#define TASK_ID  0x80   /* ID for dynamic user tasks                */
#define TIMER_ID 0xa0   /* ID for timer task                        */
#define TEL_ID   0xc0   /* ID for telephone support                 */
#define MAN_ID   0xe0   /* ID for management                        */

/*------------------------------------------------------------------*/
/* ASSIGN and REMOVE requests are the same for all entities         */
/*------------------------------------------------------------------*/

#define ASSIGN  0x01
#define UREMOVE 0xfe	/* without return code */
#define REMOVE  0xff

/*------------------------------------------------------------------*/
/* Timer Interrupt Task Interface                                   */
/*------------------------------------------------------------------*/

#define ASSIGN_TIM 0x01
#define REMOVE_TIM 0xff

/*------------------------------------------------------------------*/
/* dynamic user task interface                                      */
/*------------------------------------------------------------------*/

#define ASSIGN_TSK 0x01
#define REMOVE_TSK 0xff

#define LOAD 0xf0
#define RELOCATE 0xf1
#define START 0xf2
#define LOAD2 0xf3
#define RELOCATE2 0xf4

/*------------------------------------------------------------------*/
/* dynamic user task messages                                       */
/*------------------------------------------------------------------*/

#define TSK_B2          0x0000
#define TSK_WAKEUP      0x2000
#define TSK_TIMER       0x4000
#define TSK_TSK         0x6000
#define TSK_PC          0xe000

/*------------------------------------------------------------------*/
/* LL management primitives                                         */
/*------------------------------------------------------------------*/

#define ASSIGN_LL 1     /* assign logical link                      */
#define REMOVE_LL 0xff  /* remove logical link                      */

/*------------------------------------------------------------------*/
/* LL service primitives                                            */
/*------------------------------------------------------------------*/

#define LL_UDATA 1      /* link unit data request/indication        */
#define LL_ESTABLISH 2  /* link establish request/indication        */
#define LL_RELEASE 3    /* link release request/indication          */
#define LL_DATA 4       /* data request/indication                  */
#define LL_LOCAL 5      /* switch to local operation (COM only)     */
#define LL_DATA_PEND 5  /* data pending indication (SDLC SHM only)  */
#define LL_REMOTE 6     /* switch to remote operation (COM only)    */
#define LL_TEST 8       /* link test request                        */
#define LL_MDATA 9      /* more data request/indication             */
#define LL_BUDATA 10    /* broadcast unit data request/indication   */
#define LL_XID 12       /* XID command request/indication           */
#define LL_XID_R 13     /* XID response request/indication          */

/*------------------------------------------------------------------*/
/* NL service primitives                                            */
/*------------------------------------------------------------------*/

#define N_MDATA         1       /* more data to come REQ/IND        */
#define N_CONNECT       2       /* OSI N-CONNECT REQ/IND            */
#define N_CONNECT_ACK   3       /* OSI N-CONNECT CON/RES            */
#define N_DISC          4       /* OSI N-DISC REQ/IND               */
#define N_DISC_ACK      5       /* OSI N-DISC CON/RES               */
#define N_RESET         6       /* OSI N-RESET REQ/IND              */
#define N_RESET_ACK     7       /* OSI N-RESET CON/RES              */
#define N_DATA          8       /* OSI N-DATA REQ/IND               */
#define N_EDATA         9       /* OSI N-EXPEDITED DATA REQ/IND     */
#define N_UDATA         10      /* OSI D-UNIT-DATA REQ/IND          */
#define N_BDATA         11      /* BROADCAST-DATA REQ/IND           */
#define N_DATA_ACK      12      /* data ack ind for D-bit procedure */
#define N_EDATA_ACK     13      /* data ack ind for INTERRUPT       */

#define N_Q_BIT         0x10    /* Q-bit for req/ind                */
#define N_M_BIT         0x20    /* M-bit for req/ind                */
#define N_D_BIT         0x40    /* D-bit for req/ind                */

/*------------------------------------------------------------------*/
/* Signaling management primitives                                  */
/*------------------------------------------------------------------*/

#define ASSIGN_SIG  1    /* assign signaling task                    */
#define UREMOVE_SIG 0xfe /* remove signaling task without return code*/
#define REMOVE_SIG  0xff /* remove signaling task                    */

/*------------------------------------------------------------------*/
/* Signaling service primitives                                     */
/*------------------------------------------------------------------*/

#define CALL_REQ 1      /* call request                             */
#define CALL_CON 1      /* call confirmation                        */
#define CALL_IND 2      /* incoming call connected                  */
#define LISTEN_REQ 2    /* listen request                           */
#define HANGUP 3        /* hangup request/indication                */
#define SUSPEND 4       /* call suspend request/confirm             */
#define RESUME 5        /* call resume request/confirm              */
#define SUSPEND_REJ 6   /* suspend rejected indication              */
#define USER_DATA 8     /* user data for user to user signaling     */
#define CONGESTION 9    /* network congestion indication            */
#define INDICATE_REQ 10 /* request to indicate an incoming call     */
#define INDICATE_IND 10 /* indicates that there is an incoming call */
#define CALL_RES 11     /* accept an incoming call                  */
#define CALL_ALERT 12   /* send ALERT for incoming call             */
#define INFO_REQ 13     /* INFO request                             */
#define INFO_IND 13     /* INFO indication                          */
#define REJECT 14       /* reject an incoming call                  */
#define RESOURCES 15    /* reserve B-Channel hardware resources     */
#define HW_CTRL 16      /* B-Channel hardware IOCTL req/ind         */
#define TEL_CTRL 16     /* Telephone control request/indication     */
#define STATUS_REQ 17   /* Request D-State (returned in INFO_IND)   */
#define FAC_REG_REQ 18  /* 1TR6 connection independent fac reg      */
#define FAC_REG_ACK 19  /* 1TR6 fac registration acknowledge        */
#define FAC_REG_REJ 20  /* 1TR6 fac registration reject             */
#define CALL_COMPLETE 21/* send a CALL_PROC for incoming call       */
#define SW_CTRL 22      /* extended software features               */
#define REGISTER_REQ 23 /* Q.931 connection independent reg req     */
#define REGISTER_IND 24 /* Q.931 connection independent reg ind     */
#define FACILITY_REQ 25 /* Q.931 connection independent fac req     */
#define FACILITY_IND 26 /* Q.931 connection independent fac ind     */
#define NCR_INFO_REQ 27 /* INFO_REQ with NULL CR                    */
#define GCR_MIM_REQ 28  /* MANAGEMENT_INFO_REQ with global CR       */
#define SIG_CTRL    29  /* Control for Signalling Hardware          */
#define DSP_CTRL    30  /* Control for DSPs                         */
#define LAW_REQ      31 /* Law config request for (returns info_i)  */
#define SPID_CTRL    32 /* Request/indication SPID related          */
#define NCR_FACILITY 33 /* Request/indication with NULL/DUMMY CR    */
#define CALL_HOLD    34 /* Request/indication to hold a CALL        */
#define CALL_RETRIEVE 35 /* Request/indication to retrieve a CALL   */
#define CALL_HOLD_ACK 36 /* OK of                hold a CALL        */
#define CALL_RETRIEVE_ACK 37 /* OK of             retrieve a CALL   */
#define CALL_HOLD_REJ 38 /* Reject of            hold a CALL        */
#define CALL_RETRIEVE_REJ 39 /* Reject of         retrieve a call   */
#define GCR_RESTART   40 /* Send/Receive Restart message            */
#define S_SERVICE     41 /* Send/Receive Supplementary Service      */
#define S_SERVICE_REJ 42 /* Reject Supplementary Service indication */
#define S_SUPPORTED   43 /* Req/Ind to get Supported Services       */
#define STATUS_ENQ    44 /* Req to send the D-ch request if !state0 */


/*------------------------------------------------------------------*/
/* management service primitives                                    */
/*------------------------------------------------------------------*/

#define MAN_READ        2
#define MAN_WRITE       3
#define MAN_EXECUTE     4
#define MAN_EVENT_ON    5
#define MAN_EVENT_OFF   6
#define MAN_LOCK        7
#define MAN_UNLOCK      8
   
#define MAN_INFO_IND    2
#define MAN_EVENT_IND   3
#define MAN_TRACE_IND   4

#define MAN_ESC         0x80

/*------------------------------------------------------------------*/
/* return code coding                                               */
/*------------------------------------------------------------------*/

#define UNKNOWN_COMMAND         0x01    /* unknown command          */
#define WRONG_COMMAND           0x02    /* wrong command            */
#define WRONG_ID                0x03    /* unknown task/entity id   */
#define WRONG_CH                0x04    /* wrong task/entity id     */
#define UNKNOWN_IE              0x05    /* unknown information el.  */
#define WRONG_IE                0x06    /* wrong information el.    */
#define OUT_OF_RESOURCES        0x07    /* ISDN-S card out of res.  */
#define N_FLOW_CONTROL          0x10    /* Flow-Control, retry      */
#define ASSIGN_RC               0xe0    /* ASSIGN acknowledgement   */
#define ASSIGN_OK               0xef    /* ASSIGN OK                */
#define OK_FC                   0xfc    /* Flow-Control RC          */
#define READY_INT               0xfd    /* Ready interrupt          */
#define TIMER_INT               0xfe    /* timer interrupt          */
#define OK                      0xff    /* command accepted         */

/*------------------------------------------------------------------*/
/* information elements                                             */
/*------------------------------------------------------------------*/

#define SHIFT 0x90              /* codeset shift                    */
#define MORE 0xa0               /* more data                        */
#define CL 0xb0                 /* congestion level                 */

        /* codeset 0                                                */

#define BC  0x04                /* Bearer Capability                */
#define CAU 0x08                /* cause                            */
#define CAD 0x0c                /* Connected address                */
#define CAI 0x10                /* call identity                    */
#define CHI 0x18                /* channel identification           */
#define LLI 0x19                /* logical link id                  */
#define CHA 0x1a                /* charge advice                    */
#define FTY 0x1c                /* Facility                         */
#define DT  0x29                /* ETSI date/time                   */
#define KEY 0x2c                /* keypad information element       */
#define UID 0x2d                /* user id information element      */
#define DSP 0x28                /* display                          */
#define SIG 0x34                /* signalling hardware control      */
#define OAD 0x6c                /* origination address              */
#define OSA 0x6d                /* origination sub-address          */
#define CPN 0x70                /* called party number              */
#define DSA 0x71                /* destination sub-address          */
#define RDX 0x73                /* redirected number extended       */
#define RDN 0x74                /* redirected number                */
#define RIN 0x76                /* redirectING number               */
#define RI  0x79                /* restart indicator                */
#define MIE 0x7a                /* management info element          */
#define LLC 0x7c                /* low layer compatibility          */
#define HLC 0x7d                /* high layer compatibility         */
#define UUI 0x7e                /* user user information            */
#define ESC 0x7f                /* escape extension                 */

#define DLC 0x20                /* data link layer configuration    */
#define NLC 0x21                /* network layer configuration      */

        /* codeset 6                                                */

#define SIN 0x01                /* service indicator                */
#define CIF 0x02                /* charging information             */
#define DATE 0x03               /* date                             */
#define CPS 0x07                /* called party status              */

/*------------------------------------------------------------------*/
/* TEL_CTRL contents                                                */
/*------------------------------------------------------------------*/

#define RING_ON         0x01
#define RING_OFF        0x02
#define HANDS_FREE_ON   0x03
#define HANDS_FREE_OFF  0x04
#define ON_HOOK         0x80
#define OFF_HOOK        0x90

/* operation values used by ETSI supplementary services */
#define THREE_PTY_BEGIN           0x04
#define THREE_PTY_END             0x05
#define ECT_EXECUTE               0x06
#define ACTIVATION_DIVERSION      0x07
#define DEACTIVATION_DIVERSION    0x08
#define CALL_DEFLECTION           0x0D
#define INTERROGATION_DIVERSION   0x0B
#define INTERROGATION_SERV_USR_NR 0x11
#define GET_SUPPORTED_SERVICES    0xff

#define DIVERSION_PROCEDURE_CFU     0x70
#define DIVERSION_PROCEDURE_CFB     0x71
#define DIVERSION_PROCEDURE_CFNR    0x72
#define DIVERSION_DEACTIVATION_CFU  0x80
#define DIVERSION_DEACTIVATION_CFB  0x81
#define DIVERSION_DEACTIVATION_CFNR 0x82
/* Service Masks */
#define SMASK_HOLD_RETRIEVE        0x00000001
#define SMASK_TERMINAL_PORTABILITY 0x00000002
#define SMASK_ECT                  0x00000004
#define SMASK_3PTY                 0x00000008
#define SMASK_CALL_FORWARDING      0x00000010
#define SMASK_CALL_DEFLECTION      0x00000020

#define PROFILEIE        0xef   /* Profile info element             */

#define SSEXT_REQ 48
#define SSEXT_IND 49
#define SSEXTIE   0xee

/* -----------------------------------------------------------**
** The PROTOCOL_FEATURE_STRING in feature.h (included         **
** in prstart.sx and astart.sx) defines capabilities and      **
** features of the actual protocol code. It's used as a bit   **
** mask.                                                      **
** The following Bits are defined:                            **
** -----------------------------------------------------------*/
#define PROTCAP_TELINDUS  0x0001  /* Telindus Variant of protocol code   */
#define PROTCAP_MAN_IF    0x0002  /* Management interface implemented    */
#define PROTCAP_V_42      0x0004  /* V42 implemented                     */
#define PROTCAP_V90D      0x0008  /* V.90D (implies up to 384k DSP code) */
#define PROTCAP_EXTD_FAX  0x0010  /* Extended FAX (ECM, 2D, T6, Polling) */
#define PROTCAP_EXTD_RXFC 0x0020  /* RxFC (Extd Flow Control), OOB Chnl  */
#define PROTCAP_VOIP      0x0040  /* VoIP (implies up to 512k DSP code)  */
#define PROTCAP_CMA_ALLPR 0x0080  /* CMA support for all NL primitives   */
#define PROTCAP_FREE8     0x0100  /* not used                            */
#define PROTCAP_FREE9     0x0200  /* not used                            */
#define PROTCAP_FREE10    0x0400  /* not used                            */
#define PROTCAP_FREE11    0x0800  /* not used                            */
#define PROTCAP_FREE12    0x1000  /* not used                            */
#define PROTCAP_FREE13    0x2000  /* not used                            */
#define PROTCAP_FREE14    0x4000  /* not used                            */
#define PROTCAP_EXTENSION 0x8000  /* used for future extentions          */

/*------------------------------------------------------------------*/
/* CAPI-like profile to indicate features on LAW_REQ                */
/*------------------------------------------------------------------*/
#define GL_INTERNAL_CONTROLLER_SUPPORTED     0x00000001L
#define GL_EXTERNAL_EQUIPMENT_SUPPORTED      0x00000002L
#define GL_HANDSET_SUPPORTED                 0x00000004L
#define GL_DTMF_SUPPORTED                    0x00000008L
#define GL_SUPPLEMENTARY_SERVICES_SUPPORTED  0x00000010L
#define GL_CHANNEL_ALLOCATION_SUPPORTED      0x00000020L
#define GL_BCHANNEL_OPERATION_SUPPORTED      0x00000040L
#define GL_LINE_INTERCONNECT_SUPPORTED       0x00000080L
#define B1_HDLC_SUPPORTED                    0x00000001L
#define B1_TRANSPARENT_SUPPORTED             0x00000002L
#define B1_V110_ASYNC_SUPPORTED              0x00000004L
#define B1_V110_SYNC_SUPPORTED               0x00000008L
#define B1_T30_SUPPORTED                     0x00000010L
#define B1_HDLC_INVERTED_SUPPORTED           0x00000020L
#define B1_TRANSPARENT_R_SUPPORTED           0x00000040L
#define B1_MODEM_ALL_NEGOTIATE_SUPPORTED     0x00000080L
#define B1_MODEM_ASYNC_SUPPORTED             0x00000100L
#define B1_MODEM_SYNC_HDLC_SUPPORTED         0x00000200L
#define B2_X75_SUPPORTED                     0x00000001L
#define B2_TRANSPARENT_SUPPORTED             0x00000002L
#define B2_SDLC_SUPPORTED                    0x00000004L
#define B2_LAPD_SUPPORTED                    0x00000008L
#define B2_T30_SUPPORTED                     0x00000010L
#define B2_PPP_SUPPORTED                     0x00000020L
#define B2_TRANSPARENT_NO_CRC_SUPPORTED      0x00000040L
#define B2_MODEM_EC_COMPRESSION_SUPPORTED    0x00000080L
#define B2_X75_V42BIS_SUPPORTED              0x00000100L
#define B2_V120_ASYNC_SUPPORTED              0x00000200L
#define B2_V120_ASYNC_V42BIS_SUPPORTED       0x00000400L
#define B2_V120_BIT_TRANSPARENT_SUPPORTED    0x00000800L
#define B2_LAPD_FREE_SAPI_SEL_SUPPORTED      0x00001000L
#define B3_TRANSPARENT_SUPPORTED             0x00000001L
#define B3_T90NL_SUPPORTED                   0x00000002L
#define B3_ISO8208_SUPPORTED                 0x00000004L
#define B3_X25_DCE_SUPPORTED                 0x00000008L
#define B3_T30_SUPPORTED                     0x00000010L
#define B3_T30_WITH_EXTENSIONS_SUPPORTED     0x00000020L
#define B3_RESERVED_SUPPORTED                0x00000040L
#define B3_MODEM_SUPPORTED                   0x00000080L
#define MANUFACTURER_FEATURE_SLAVE_CODEC          0x00000001L
#define MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS   0x00000002L
#define MANUFACTURER_FEATURE_HARDDTMF             0x00000004L
#define MANUFACTURER_FEATURE_SOFTDTMF_SEND        0x00000008L
#define MANUFACTURER_FEATURE_DTMF_PARAMETERS      0x00000010L
#define MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE     0x00000020L
#define MANUFACTURER_FEATURE_FAX_SUB_SEP_PWD      0x00000040L
#define MANUFACTURER_FEATURE_V18                  0x00000080L
#define MANUFACTURER_FEATURE_MIXER_CH_CH          0x00000100L
#define MANUFACTURER_FEATURE_MIXER_CH_PC          0x00000200L
#define MANUFACTURER_FEATURE_MIXER_PC_CH          0x00000400L
#define MANUFACTURER_FEATURE_MIXER_PC_PC          0x00000800L
#define MANUFACTURER_FEATURE_ECHO_CANCELLER       0x00001000L
#define MANUFACTURER_FEATURE_RTP                  0x00002000L
#define MANUFACTURER_FEATURE_T38                  0x00004000L
#define MANUFACTURER_FEATURE_TRANSP_DELIVERY_CONF 0x00008000L
#define MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL  0x00010000L
#define MANUFACTURER_FEATURE_OOB_CHANNEL          0x00020000L
#define MANUFACTURER_FEATURE_IN_BAND_CHANNEL      0x00040000L
#define MANUFACTURER_FEATURE_IN_BAND_FEATURE      0x00080000L
#define MANUFACTURER_FEATURE_PIAFS                0x00100000L
#define MANUFACTURER_FEATURE_DTMF_TONE            0x00200000L
#define MANUFACTURER_FEATURE_FAX_PAPER_FORMATS    0x00400000L
#define MANUFACTURER_FEATURE_OK_FC_LABEL          0x00800000L
#define MANUFACTURER_FEATURE_VOWN                 0x01000000L
#define MANUFACTURER_FEATURE_XCONNECT             0x02000000L
#define MANUFACTURER_FEATURE_DMACONNECT           0x04000000L
#define MANUFACTURER_FEATURE_AUDIO_TAP            0x08000000L
#define MANUFACTURER_FEATURE_FAX_NONSTANDARD      0x10000000L
#define MANUFACTURER_FEATURE_SS7                  0x20000000L
#define MANUFACTURER_FEATURE_MADAPTER             0x40000000L
#define MANUFACTURER_FEATURE_MEASURE              0x80000000L
#define RTP_PRIM_PAYLOAD_PCMU_8000     0
#define RTP_PRIM_PAYLOAD_1016_8000     1
#define RTP_PRIM_PAYLOAD_G726_32_8000  2
#define RTP_PRIM_PAYLOAD_GSM_8000      3
#define RTP_PRIM_PAYLOAD_G723_8000     4
#define RTP_PRIM_PAYLOAD_DVI4_8000     5
#define RTP_PRIM_PAYLOAD_DVI4_16000    6
#define RTP_PRIM_PAYLOAD_LPC_8000      7
#define RTP_PRIM_PAYLOAD_PCMA_8000     8
#define RTP_PRIM_PAYLOAD_G722_16000    9
#define RTP_PRIM_PAYLOAD_QCELP_8000    12
#define RTP_PRIM_PAYLOAD_G728_8000     14
#define RTP_PRIM_PAYLOAD_G729_8000     18
#define RTP_PRIM_PAYLOAD_GSM_HR_8000   30
#define RTP_PRIM_PAYLOAD_GSM_EFR_8000  31
#define RTP_ADD_PAYLOAD_BASE           32
#define RTP_ADD_PAYLOAD_RED            32
#define RTP_ADD_PAYLOAD_CN_8000        33
#define RTP_ADD_PAYLOAD_DTMF           34
#define RTP_PRIM_PAYLOAD_PCMU_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_PCMU_8000)
#define RTP_PRIM_PAYLOAD_1016_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_1016_8000)
#define RTP_PRIM_PAYLOAD_G726_32_8000_SUPPORTED  (1L << RTP_PRIM_PAYLOAD_G726_32_8000)
#define RTP_PRIM_PAYLOAD_GSM_8000_SUPPORTED      (1L << RTP_PRIM_PAYLOAD_GSM_8000)
#define RTP_PRIM_PAYLOAD_G723_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G723_8000)
#define RTP_PRIM_PAYLOAD_DVI4_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_DVI4_8000)
#define RTP_PRIM_PAYLOAD_DVI4_16000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_DVI4_16000)
#define RTP_PRIM_PAYLOAD_LPC_8000_SUPPORTED      (1L << RTP_PRIM_PAYLOAD_LPC_8000)
#define RTP_PRIM_PAYLOAD_PCMA_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_PCMA_8000)
#define RTP_PRIM_PAYLOAD_G722_16000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_G722_16000)
#define RTP_PRIM_PAYLOAD_QCELP_8000_SUPPORTED    (1L << RTP_PRIM_PAYLOAD_QCELP_8000)
#define RTP_PRIM_PAYLOAD_G728_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G728_8000)
#define RTP_PRIM_PAYLOAD_G729_8000_SUPPORTED     (1L << RTP_PRIM_PAYLOAD_G729_8000)
#define RTP_PRIM_PAYLOAD_GSM_HR_8000_SUPPORTED   (1L << RTP_PRIM_PAYLOAD_GSM_HR_8000)
#define RTP_PRIM_PAYLOAD_GSM_EFR_8000_SUPPORTED  (1L << RTP_PRIM_PAYLOAD_GSM_EFR_8000)
#define RTP_ADD_PAYLOAD_RED_SUPPORTED            (1L << (RTP_ADD_PAYLOAD_RED - RTP_ADD_PAYLOAD_BASE))
#define RTP_ADD_PAYLOAD_CN_8000_SUPPORTED        (1L << (RTP_ADD_PAYLOAD_CN_8000 - RTP_ADD_PAYLOAD_BASE))
#define RTP_ADD_PAYLOAD_DTMF_SUPPORTED           (1L << (RTP_ADD_PAYLOAD_DTMF - RTP_ADD_PAYLOAD_BASE))

#else
#endif
