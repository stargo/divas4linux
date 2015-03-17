
/*
 *
  Copyright (c) Dialogic(R), 2009.
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

#define T30_ADDRESS_GSTN     0xff

#define T30_CONTROL          0x03
#define T30_FINAL_FRAME_BIT  0x10

#define T30_CFR              0x84  /* Confirmation to receive                */
#define T30_CIG              0x41  /* Calling subscriber identification      */
#define T30_CRP              0x1a  /* Command repeat                         */
#define T30_CSI              0x40  /* Called subscriber identification       */
#define T30_DCN              0xfa  /* Disconnect                             */
#define T30_DCS              0x82  /* Digital command signal                 */
#define T30_DIS              0x80  /* Digital identification signal          */
#define T30_DTC              0x81  /* Digital transmit command               */
#define T30_EOM              0x8e  /* End of message                         */
#define T30_EOP              0x2e  /* End of procedure                       */
#define T30_FCD              0x06  /* Facsimilecoded date                    */
#define T30_FDM              0xfc  /* File diagnostics message               */
#define T30_FTT              0x44  /* Failure to train                       */
#define T30_MCF              0x8c  /* Message confirmation                   */
#define T30_MPS              0x4e  /* Multi-page signal                      */
#define T30_NSC              0x21  /* Non-standard facilities command        */
#define T30_NSF              0x20  /* Non-standard facilities                */
#define T30_NSS              0x22  /* Non-standard set-up                    */
#define T30_PID              0x6c  /* Procedure interrupt disconnect         */
#define T30_PIN              0x2c  /* Procedural interrupt negative          */
#define T30_PIP              0xac  /* Procedural interrupt positive          */
#define T30_PRI_EOM          0x9e  /* Procedure interrupt-EOM                */
#define T30_PRI_EOP          0x3e  /* Procedure interrupt-EOP                */
#define T30_PRI_MPS          0x5e  /* Procedure interrupt-MPS                */
#define T30_RTN              0x4c  /* Retrain negative                       */
#define T30_RTP              0xcc  /* Retrain positive                       */
#define T30_TSI              0x42  /* Transmitting subscriver identification */
#define T30_PWP              0xc1  /* Password (for polling)                 */
#define T30_PWT              0xa2  /* Password (for transmission)            */
#define T30_SEP              0xa1  /* Selective polling                      */
#define T30_SUB              0xc2  /* Subaddress                             */

#define T30_DIS_RECEIVED_BIT 0x01
#define T30_FIF_EXTEND_BIT   0x80

#define T30_CTC              0x12  /* Continue to correct                    */
#define T30_CTR              0xc4  /* Response to continue to correct        */
#define T30_EOR              0xce  /* End of retransmission                  */
#define T30_ERR              0x1c  /* Response for end of retransmission     */
#define T30_PPS              0xbe  /* Partial page signal                    */
#define T30_PPR              0xbc  /* Partial page request                   */
#define T30_RNR              0xec  /* Receive not ready                      */
#define T30_RR               0x6e  /* Receive ready                          */

#define T30_PPS_EOR_NULL     0x00  /* PPS/EOR NULL                           */
#define T30_PPS_EOR_EOM      0x8f  /* PPS/EOR EOM                            */
#define T30_PPS_EOR_MPS      0x4f  /* PPS/EOR MPS                            */
#define T30_PPS_EOR_EOP      0x2f  /* PPS/EOR EOP                            */
#define T30_PPS_EOR_PRI_EOM  0x9f  /* PPS/EOR PRI-EOM                        */
#define T30_PPS_EOR_PRI_MPS  0x5f  /* PPS/EOR PRI-MPS                        */
#define T30_PPS_EOR_PRI_EOP  0x3f  /* PPS/EOR PRI-EOP                        */

#define T4_ECM_ADDRESS_GSTN  0xff

#define T4_ECM_CONTROL       0x03
#define T4_ECM_FINAL_FRAME_BIT 0x10


/*------------------------------------------------------------------*/
/* T.30 errors                                                      */
/*------------------------------------------------------------------*/

#define T30_SUCCESS                         0
#define T30_ERR_NO_ENERGY_DETECTED          1
#define T30_ERR_TIMEOUT_NO_RESPONSE         2
#define T30_ERR_RETRY_NO_RESPONSE           3
#define T30_ERR_TOO_MANY_REPEATS            4
#define T30_ERR_UNEXPECTED_MESSAGE          5
#define T30_ERR_UNEXPECTED_DCN              6
#define T30_ERR_DTC_UNSUPPORTED             7
#define T30_ERR_ALL_RATES_FAILED            8
#define T30_ERR_TOO_MANY_TRAINS             9
#define T30_ERR_RECEIVE_CORRUPTED           10
#define T30_ERR_UNEXPECTED_DISC             11
#define T30_ERR_APPLICATION_DISC            12
#define T30_ERR_INCOMPATIBLE_DIS            13
#define T30_ERR_INCOMPATIBLE_DCS            14
#define T30_ERR_TIMEOUT_NO_COMMAND          15
#define T30_ERR_RETRY_NO_COMMAND            16
#define T30_ERR_TIMEOUT_COMMAND_TOO_LONG    17
#define T30_ERR_TIMEOUT_RESPONSE_TOO_LONG   18
#define T30_ERR_NO_FAX_DETECTED             19
#define T30_ERR_SUPERVISORY_TIMEOUT         20
#define T30_ERR_TOO_LONG_SCAN_LINE          21
/* #define T30_ERR_RETRY_NO_PAGE_AFTER_MPS     22 */
#define T30_ERR_RETRY_NO_PAGE_RECEIVED      23
#define T30_ERR_RETRY_NO_DCS_AFTER_FTT      24
#define T30_ERR_RETRY_NO_DCS_AFTER_EOM      25
#define T30_ERR_RETRY_NO_DCS_AFTER_MPS      26
#define T30_ERR_RETRY_NO_DCN_AFTER_MCF      27
#define T30_ERR_RETRY_NO_DCN_AFTER_RTN      28
#define T30_ERR_RETRY_NO_CFR                29
#define T30_ERR_RETRY_NO_MCF_AFTER_EOP      30
#define T30_ERR_RETRY_NO_MCF_AFTER_EOM      31
#define T30_ERR_RETRY_NO_MCF_AFTER_MPS      32
#define T30_ERR_SUB_SEP_UNSUPPORTED         33
#define T30_ERR_PWD_UNSUPPORTED             34
#define T30_ERR_SUB_SEP_PWD_UNSUPPORTED     35
#define T30_ERR_INVALID_COMMAND_FRAME       36
#define T30_ERR_UNSUPPORTED_PAGE_CODING     37
#define T30_ERR_INVALID_PAGE_CODING         38
#define T30_ERR_INCOMPATIBLE_PAGE_CONFIG    39
#define T30_ERR_TIMEOUT_FROM_APPLICATION    40
#define T30_ERR_V34FAX_NO_REACTION_ON_MARK  41
#define T30_ERR_V34FAX_TRAINING_TIMEOUT     42
#define T30_ERR_V34FAX_UNEXPECTED_V21       43
#define T30_ERR_V34FAX_PRIMARY_CTS_ON       44
#define T30_ERR_V34FAX_TURNAROUND_POLLING   45
#define T30_ERR_V34FAX_V8_INCOMPATIBILITY   46
#define T30_ERR_V34FAX_KNOWN_ECM_BUG        47
#define T30_ERR_NOT_IDENTIFIED              48
#define T30_ERR_DROP_BELOW_MIN_SPEED        49
#define T30_ERR_MAX_OVERHEAD_EXCEEDED       50

/*------------------------------------------------------------------*/
/* Constants used to create header line                             */
/* +FPH command                                                     */
/*------------------------------------------------------------------*/
#define T30_HEADER_LINE_LEFT_INDENT                   4
#define T30_HEADER_LINE_RIGHT_INDENT                  4
#define T30_HEADER_LINE_PAGE_INFO_SPACE              10
#define T30_HEADER_FONT_WIDTH_PIXELS                 16
#define T30_R16_1540_OR_400_HEADER_FONT_WIDTH_PIXELS 32

#define T30_MAX_HEADER_LINE_LENGTH (SFF_WD_A3/T30_HEADER_FONT_WIDTH_PIXELS - \
																									T30_HEADER_LINE_LEFT_INDENT - \
																										T30_HEADER_LINE_RIGHT_INDENT)


#if 0
/*------------------------------------------------------------------*/
/* T.30 info structure                                              */
/*------------------------------------------------------------------*/

typedef struct t30_info_s T30_INFO;
struct t30_info_s {
  byte  /*ind*/ code;                 /* req                      */
  byte  /*req*/ rate_div_2400;        /* ind rate_div_2400        */
  byte  /*req*/ resolution;           /* ind resolution           */
  byte  /*req*/ data_format;          /* ind data_format          */
  byte  /*ind*/ pages_low;            /* req                      */
  byte  /*ind*/ pages_high;           /* req                      */
  byte  /*req*/ operating_mode;       /* ind                      */
  byte  /*req*/ control_bits_low;     /* ind feature2_bits_low    */
  byte  /*req*/ control_bits_high;    /* ind feature2_bits_high   */
  byte  /*ind*/ feature_bits_low;     /* req control2_bits_low    */
  byte  /*ind*/ feature_bits_high;    /* req control2_bits_high   */
  byte  /*req*/ recording_properties; /* ind recording_properties */
  byte  /*req*/ universal_6;          /* ind universal_6          */
  byte  /*req*/ universal_7;          /* ind universal_7          */
  byte  /*req*/ station_id_len;       /* ind station_id_len       */
  byte  /*req*/ head_line_len;        /* ind                      */
  byte          station_id[T30_MAX_STATION_ID_LENGTH];
/* byte          head_line[];       */
/* byte          sub_sep_length;    */
/* byte          sub_sep_field[];   */
/* byte          pwd_length;        */
/* byte          pwd_field[];       */
/* byte          nsf_info_length;   */
/* byte          nsf_info_field[];  */
};
#endif



        /* EDATA transmit messages */
#define EDATA_T30_DIS       0x01
#define EDATA_T30_FTT       0x02
#define EDATA_T30_MCF       0x03

        /* EDATA receive messages */
#define EDATA_T30_DCS       0x81
#define EDATA_T30_TRAIN_OK  0x82
#define EDATA_T30_EOP       0x83
#define EDATA_T30_MPS       0x84
#define EDATA_T30_EOM       0x85


/*---------------------------------------------------------------------------*/
/*
  FAX Class1 support interface
*/
/*---------------------------------------------------------------------------*/

#define CLASS1_SUPPORTED_PROTOCOLS_TX_DIVA     0x0000007e
#define CLASS1_SUPPORTED_PROTOCOLS_RX_DIVA     0x0000007f
#define CLASS1_SUPPORTED_PROTOCOLS_TX_DIVAPRO  0x0000fe7e
#define CLASS1_SUPPORTED_PROTOCOLS_RX_DIVAPRO  0x0000fe7f

#define CLASS1_RECONFIGURE_REQUEST             0
/*
  N_UDATA
  <byte> CLASS1_RECONFIGURE_REQUEST
  <word> reconfigure delay (in 8kHz samples)
  <word> reconfigure code
  <word> reconfigure hdlc preamble flags
*/

#define CLASS1_RECONFIGURE_TX_FLAG             0x8000
#define CLASS1_RECONFIGURE_SHORT_TRAIN_FLAG    0x4000
#define CLASS1_RECONFIGURE_ECHO_PROTECT_FLAG   0x2000
#define CLASS1_RECONFIGURE_HDLC_FLAG           0x1000
#define CLASS1_RECONFIGURE_SYNC_FLAG           0x0800
#define CLASS1_RECONFIGURE_CI_FLAG             0x0400
#define CLASS1_RECONFIGURE_IGNORE_DCD_FLAG     0x0200
#define CLASS1_RECONFIGURE_PROTOCOL_MASK       0x00ff
#define CLASS1_RECONFIGURE_IDLE                0
#define CLASS1_RECONFIGURE_V25                 1
#define CLASS1_RECONFIGURE_V21_CH2             2
#define CLASS1_RECONFIGURE_V27_2400            3
#define CLASS1_RECONFIGURE_V27_4800            4
#define CLASS1_RECONFIGURE_V29_7200            5
#define CLASS1_RECONFIGURE_V29_9600            6
#define CLASS1_RECONFIGURE_V33_12000           7
#define CLASS1_RECONFIGURE_V33_14400           8
#define CLASS1_RECONFIGURE_V17_7200            9
#define CLASS1_RECONFIGURE_V17_9600            10
#define CLASS1_RECONFIGURE_V17_12000           11
#define CLASS1_RECONFIGURE_V17_14400           12
#define CLASS1_RECONFIGURE_V8                  13
#define CLASS1_RECONFIGURE_V34_CONTROL_CHANNEL 14
#define CLASS1_RECONFIGURE_V34_PRIMARY_CHANNEL 15

#define CLASS1_REQUEST_CLEARDOWN               2
/*
  N_UDATA
  - none -
*/

#define CLASS1_REQUEST_RETRAIN                 8
/*
  N_UDATA
  - none -
*/

#define CLASS1_REQUEST_V34FAX_SETUP            9
/*
  N_UDATA
  <word> flags
  <word> control channel preferred speed (bit/s)
  <word> primary channel min tx speed (bit/s)
  <word> primary channel max tx speed (bit/s)
  <word> primary channel min rx speed (bit/s)
  <word> primary channel max rx speed (bit/s)
  <word> enabled modulations mask
*/

#define CLASS1_V34FAX_SETUP_NORMAL_CAPABILITY  0x0001
#define CLASS1_V34FAX_SETUP_POLLING_CAPABILITY 0x0002

#define CLASS1_V34FAX_ENABLED_MODULATION_V27   0x0100
#define CLASS1_V34FAX_ENABLED_MODULATION_V29   0x0200
#define CLASS1_V34FAX_ENABLED_MODULATION_V33   0x0400
#define CLASS1_V34FAX_ENABLED_MODULATION_V17   0x0800

#define CLASS1_REQUEST_SEND_MARK               14
/*
  N_UDATA
  <word> mark bits to send
*/

#define CLASS1_REQUEST_DETECT_MARK             15
/*
  N_UDATA
  <word> mark bits to detect
*/


#define CLASS1_INDICATION_SYNC                 0
/*
  N_EDATA
  <byte> CLASS1_INDICATION_SYNC
  <word> time of sync (sampled from counter at 8kHz)
*/

#define CLASS1_INDICATION_DCD_OFF              1
/*
  N_EDATA
  <byte> CLASS1_INDICATION_DCD_OFF
  <word> time of DCD off (sampled from counter at 8kHz)
*/

#define CLASS1_INDICATION_DCD_ON               2
/*
  N_EDATA
  <byte> CLASS1_INDICATION_DCD_ON
  <word> time of DCD on (sampled from counter at 8kHz)
  <byte> connected norm
  <word> connected options
  <dword> connected speed (bit/s)
  <word> roundtrip delay (ms)
  <dword> connected speed tx (bit/s)
  <dword> connected speed rx (bit/s)
  <word> secondary channel speed tx (bit/s)
  <word> secondary channel speed rx (bit/s)
*/

#define CLASS1_INDICATION_CTS_OFF              3
/*
  N_EDATA
  <byte> CLASS1_INDICATION_CTS_OFF
  <word> time of CTS off (sampled from counter at 8kHz)
*/

#define CLASS1_INDICATION_CTS_ON               4
/*
  N_EDATA
  <byte> CLASS1_INDICATION_CTS_ON
  <word> time of CTS on (sampled from counter at 8kHz)
  <byte> connected norm
  <word> connected options
  <dword> connected speed (bit/s)
  <word> roundtrip delay (ms)
  <dword> connected speed tx (bit/s)
  <dword> connected speed rx (bit/s)
  <word> secondary channel speed tx (bit/s)
  <word> secondary channel speed rx (bit/s)
*/

#define CLASS1_CONNECTED_NORM_UNSPECIFIED      0
#define CLASS1_CONNECTED_NORM_V21              1
#define CLASS1_CONNECTED_NORM_V23              2
#define CLASS1_CONNECTED_NORM_V22              3
#define CLASS1_CONNECTED_NORM_V22_BIS          4
#define CLASS1_CONNECTED_NORM_V32_BIS          5
#define CLASS1_CONNECTED_NORM_V34              6
#define CLASS1_CONNECTED_NORM_V8               7
#define CLASS1_CONNECTED_NORM_BELL_212A        8
#define CLASS1_CONNECTED_NORM_BELL_103         9
#define CLASS1_CONNECTED_NORM_V29_LEASED_LINE  10
#define CLASS1_CONNECTED_NORM_V33_LEASED_LINE  11
#define CLASS1_CONNECTED_NORM_TFAST            12
#define CLASS1_CONNECTED_NORM_V21_CH2          13
#define CLASS1_CONNECTED_NORM_V27_TER          14
#define CLASS1_CONNECTED_NORM_V29              15
#define CLASS1_CONNECTED_NORM_V33              16
#define CLASS1_CONNECTED_NORM_V17              17
#define CLASS1_CONNECTED_NORM_V34FAX_CONTROL   70
#define CLASS1_CONNECTED_NORM_V34FAX_PRIMARY   71

#define CLASS1_CONNECTED_OPTION_TRELLIS        0x0001

#define CLASS1_INDICATION_CI_DETECTED          14
/*
  N_EDATA
  <word> time of CI detect (sampled from counter at 8kHz)
*/

#define CLASS1_INDICATION_SIGNAL_TONE          21
/*
  N_EDATA
  <byte> CLASS1_INDICATION_SIGNAL_TONE
  <byte> signal tone code
*/

#define CLASS1_SIGNAL_FAX_FLAGS                71


#define CLASS1_INDICATION_DATA                 0
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> CLASS1_INDICATION_DATA
*/

#define CLASS1_INDICATION_FLUSHED_DATA         1
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> CLASS1_INDICATION_FLUSHED_DATA
*/

#define CLASS1_INDICATION_HDLC_DATA            2
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> CRC 0
  <byte> CRC 1
  <byte> number of preamble flags
  <byte> CLASS1_INDICATION_HDLC_DATA
*/

#define CLASS1_INDICATION_ABORTED_HDLC_DATA    3
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> number of preamble flags
  <byte> CLASS1_INDICATION_ABORTED_HDLC_DATA
*/

#define CLASS1_INDICATION_WRONG_CRC_HDLC_DATA  4
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> wrong CRC 0
  <byte> wrong CRC 1
  <byte> number of preamble flags
  <byte> CLASS1_INDICATION_WRONG_CRC_HDLC_DATA
*/

#define CLASS1_INDICATION_FLUSHED_HDLC_DATA    5
/*
  N_DATA
  <byte> data byte 0
  <byte> data byte 1
  ...
  <byte> number of preamble flags
  <byte> CLASS1_INDICATION_FLUSHED_HDLC_DATA
*/


/*------------------------------------------------------------------*/
