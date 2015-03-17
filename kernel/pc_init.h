
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
#ifndef PC_INIT_H_
#define PC_INIT_H_
/*------------------------------------------------------------------*/
/*
  Initialisation parameters for the card
  0x0008 <byte> TEI
  0x0009 <byte> NT2 flag
  0x000a <byte> Default DID length
  0x000b <byte> Disable watchdog flag
  0x000c <byte> Permanent connection flag
  0x000d <byte> Bit 0: QSig small CR length if set to 1
                Bit 1: QSig small CHI length if set to 1
                Bit 2: L1 Hunt Group mode (TX tristate, RX off)
                Bit 3: Monitor mode (TX tristate, RX on)
  0x000e <byte> Bit 2-0: Stable L2, 0=OnDemand,1=NoDisc,2=permanent
                Bit 3: NT mode
                Bit 4: QSig Channel ID format
                Bit 5: QSig Call Forwarding Allowed Flag
                Bit 6: Disable AutoSPID Flag
  0x000f <byte> No order check flag
  0x0010 <byte> Force companding type:0=default,1=a-law,2=u-law
  0x0012 <byte> Low channel flag
  0x0013 <byte> Protocol version
  0x0014 <byte> Bit 1-0: CRC4 option:0=default,1=double_frm,2=multi_frm,3=auto
                Bit 4: L1 disable jitter attenuation
                Bit 5: L1 free running TE
  0x0015 <byte> Bit 0: NoHscx30
                Bit 1: Loopback flag
                Bit 2: ForceHscx30
                Bit 3: Mask robbed bit on B-channel data for PROT_T1CHAN and PROT_T1UNCH
  0x0016 <byte> DSP info
  0x0017-0x0019 Serial number
  0x001a <byte> Card type
  0x0020 <string> OAD 0
  0x0040 <string> OSA 0
  0x0060 <string> SPID 0 (if not T.1)
  0x0060 <struct> if T.1: Robbed Bit Configuration
  0x0060          length (8)
  0x0061          RBS Answer Delay
  0x0062          RBS Config Bit 3, 4:
                             0  0 -> Wink Start
                             1  0 -> Loop Start
                             0  1 -> Ground Start
                             1  1 -> reserved
                             Bit 5, 6:
                             0  0 -> Pulse Dial -> Rotary
                             1  0 -> DTMF
                             0  1 -> MF
                             1  1 -> reserved
  0x0063          RBS RX Digit Timeout
  0x0064          RBS Bearer Capability
  0x0065-0x0068   RBS Debug Mask
  0x0080 <string> OAD 1
  0x00a0 <string> OSA 1
  0x00c0 <string> SPID 1
  0x00e0 <w-element list> Additional configuration
*/
#define PCINIT_END_OF_LIST                0x00
#define PCINIT_MODEM_GUARD_TONE           0x01
#define PCINIT_MODEM_MIN_SPEED            0x02
#define PCINIT_MODEM_MAX_SPEED            0x03
#define PCINIT_MODEM_PROTOCOL_OPTIONS     0x04
#define PCINIT_FAX_OPTIONS                0x05
#define PCINIT_FAX_MAX_SPEED              0x06
#define PCINIT_MODEM_OPTIONS              0x07
#define PCINIT_MODEM_NEGOTIATION_MODE     0x08
#define PCINIT_MODEM_MODULATIONS_MASK     0x09
#define PCINIT_MODEM_TRANSMIT_LEVEL       0x0a
#define PCINIT_FAX_DISABLED_RESOLUTIONS   0x0b
#define PCINIT_FAX_MAX_RECORDING_WIDTH    0x0c
#define PCINIT_FAX_MAX_RECORDING_LENGTH   0x0d
#define PCINIT_FAX_MIN_SCANLINE_TIME      0x0e
#define PCINIT_US_EKTS_CACH_HANDLES       0x0f
#define PCINIT_US_EKTS_BEGIN_CONF         0x10
#define PCINIT_US_EKTS_DROP_CONF          0x11
#define PCINIT_US_EKTS_CALL_TRANSFER      0x12
#define PCINIT_RINGERTONE_OPTION          0x13
#define PCINIT_CARD_ADDRESS               0x14
#define PCINIT_FPGA_FEATURES              0x15
#define PCINIT_US_EKTS_MWI                0x16
#define PCINIT_SOFTWARE_OPTIONS           0x17
#define PCINIT_RESERVED_18                0x18
#define PCINIT_MODEM_CARRIER_WAIT_TIME    0x19
#define PCINIT_MODEM_CARRIER_LOSS_TIME    0x1a
#define PCINIT_UNCHAN_B_MASK              0x1b
#define PCINIT_PART68_LIMITER             0x1c
#define PCINIT_XDI_FEATURES               0x1d
#define PCINIT_QSIG_DIALECT               0x1e
#define PCINIT_DISABLE_AUTOSPID_FLAG      0x1f
#define PCINIT_FORCE_VOICE_MAIL_ALERT     0x20
#define PCINIT_PIAFS_TURNAROUND_FRAMES    0x21
#define PCINIT_L2_COUNT                   0x22
#define PCINIT_QSIG_FEATURES              0x23
#define PCINIT_NO_SIGNALLING              0x24
#define PCINIT_CARD_SN                    0x25
#define PCINIT_CARD_PORT                  0x26
#define PCINIT_ALERTTO                    0x27
#define PCINIT_MODEM_EYE_SETUP            0x28
#define PCINIT_FAX_V34_OPTIONS            0x29
#define PCINIT_CCBS_REL_TIMER             0x2a
#define PCINIT_DISABLED_DSPS_MASK         0x2b
#define PCINIT_SUPPL_SERVICE_FEATURES     0x2c
#define PCINIT_R2_DIALECT                 0x2d
#define PCINIT_R2_CASOPTIONS              0x2e
#define PCINIT_R2_CTRYLENGTH              0x2f
#define PCINIT_DISCAFTERPROGRESS          0x30
#define PCINIT_V110_OPTIONS               0x31
#define PCINIT_V110_PURE_SYNC_TIME        0x32
#define PCINIT_V110_DATA_START_DELAY      0x33
#define PCINIT_DSP_IMAGE_LENGTH           0x34
#define PCINIT_DIAL_PULSE_DETECTOR        0x35
#define PCINIT_ANIDNILIMITER              0x36
#define PCINIT_CORNETDEFAULTS             0x37
#define PCINIT_XCONNECT_EXPORT_MEMORY     0x38
#define PCINIT_XCONNECT_EXPORT            0x39
#define PCINIT_TXATTENUATION              0x3a
#define PCINIT_VIRTUAL_SIGNALLING_ACCESS  0x3b
#define PCINIT_CFG_LINE_NUMBER            0x3c
#define PCINIT_REJECTCAUSENOLISTEN        0x3d
#define PCINIT_SET_LINE_TYPE              0x3e
#define PCINIT_CAS_BC                     0x3f
#define PCINIT_CAS_ANSWERDELAY            0x40
#define PCINIT_CAS_DEBUG                  0x41
#define PCINIT_CAS_RXDIGITTIMEOUT         0x42
#define PCINIT_CAS_CONFIG                 0x43
#define PCINIT_CAS_GLARERESOLUTION        0x44
#define PCINIT_CAS_DIALTYPE               0x45
#define PCINIT_CAS_TRUNCTYPE              0x46
#define PCINIT_CAS_ANSWERSUPERVISION      0x47
#define PCINIT_DID_LENGTH                 0x48
#define PCINIT_POTS_DIAL_CHAR             0x49
#define PCINIT_DTMF_SUPPRESSOR            0x4a
#define PCINIT_FAX_MIN_SPEED              0x4b
#define PCINIT_FAX_MAX_OVERHEAD           0x4c
#define PCINIT_FAX_TRANSMIT_LEVEL         0x4d
#define PCINIT_FAX_FONT_CHARACTER         0x4e
#define PCINIT_RECORDING_AGC              0x4f
#define PCINIT_DTMFCOLLECTLEN             0x50
#define PCINIT_CARD_HIS                   0x51
#define PCINIT_RBS_SILENCE_TIMEOUT        0x52
#define PCINIT_CALLERIDDETECT             0x53
#define PCINIT_RINGSTILANSWER             0x54
#define PCINIT_SPI_DMA_DESCRIPTOR         0x55
#define PCINIT_SPI_DMA_DESCRIPTOR_LENGTH  0x56
#define PCINIT_REJECTCAUSEIGNORE          0x57
#define PCINIT_POTS_SWAPHOLD              0x58
#define PCINIT_POTS_DISCACTIVCALL         0x59
#define PCINIT_POTS_DISCPASSIVCALL        0x5a
#define PCINIT_POTS_CONSULT               0x5b
#define PCINIT_POTS_3PTYCONNECT           0x5c
#define PCINIT_POTS_3PTYDISC              0x5d
#define PCINIT_POTS_TRANSFER              0x5e
#define PCINIT_POTS_CALLBACKAFTERRETRIEVE 0x5f
#define PCINIT_POTS_CALLBACKTIMER         0x60
#define PCINIT_POTS_SWAPRETRIEVEACKTIMER  0x61
#define PCINIT_POTS_RETRIEVEHOOKOFFTIMER  0x62
#define PCINIT_FLASHLENGTH                0x63
#define PCINIT_MONITOR_SPI_DMA_DESCRIPTOR 0x64
#define PCINIT_SPI_DMA_XCONN_DESCRIPTOR   0x65
#define PCINIT_POTS_CONFIGCPN             0x66
#define PCINIT_POTS_SUPPRESSCPN           0x67
#define PCINIT_POTS_DIRECTION             0x68
#define PCINIT_POTS_CALIBRATIONMODE       0x69
#define PCINIT_XCONNECT_TRANSFER_MEMORY   0x6a
#define PCINIT_CHI_INTERFACE_IDENT        0x6b
#define PCINIT_VIDI_INFO                  0x6c
#define PCINIT_CLOCK_REF_MEMORY           0x6d
#define PCINIT_CLOCK_REF_PARAMETERS       0x6e
#define PCINIT_CARD_MODE                  0x6f
#define PCINIT_L1_CLOCK_BIAS              0x70
#define PCINIT_L1_SYNC_OP_MODE            0x71
#define PCINIT_L1_SYNC_PORT_NO            0x72
#define PCINIT_CAS_TX_IDLE                0x73
#define PCINIT_OPTIMIZATION               0x74
/*------------------------------------------------------------------*/
#define PCINIT_MODEM_GUARD_TONE_NONE            0x00
#define PCINIT_MODEM_GUARD_TONE_550HZ           0x01
#define PCINIT_MODEM_GUARD_TONE_1800HZ          0x02
#define PCINIT_MODEM_GUARD_TONE_CHOICES         0x03
#define PCINIT_MODEMPROT_DISABLE_V42_V42BIS     0x0001
#define PCINIT_MODEMPROT_DISABLE_MNP_MNP5       0x0002
#define PCINIT_MODEMPROT_REQUIRE_PROTOCOL       0x0004
#define PCINIT_MODEMPROT_DISABLE_V42_DETECT     0x0008
#define PCINIT_MODEMPROT_DISABLE_COMPRESSION    0x0010
#define PCINIT_MODEMPROT_REQUIRE_PROTOCOL_V34UP 0x0020
#define PCINIT_MODEMPROT_DISABLE_SDLC           0x0040
#define PCINIT_MODEMPROT_NO_PROTOCOL_IF_1200    0x0100
#define PCINIT_MODEMPROT_BUFFER_IN_V42_DETECT   0x0200
#define PCINIT_MODEMPROT_DISABLE_V42_SREJ       0x0400
#define PCINIT_MODEMPROT_DISABLE_MNP3           0x0800
#define PCINIT_MODEMPROT_DISABLE_MNP4           0x1000
#define PCINIT_MODEMPROT_DISABLE_MNP10          0x2000
#define PCINIT_MODEMPROT_NO_PROTOCOL_IF_V22BIS  0x4000
#define PCINIT_MODEMPROT_NO_PROTOCOL_IF_V32BIS  0x8000
#define PCINIT_MODEMCONFIG_LEASED_LINE_MODE     0x00000001L
#define PCINIT_MODEMCONFIG_4_WIRE_OPERATION     0x00000002L
#define PCINIT_MODEMCONFIG_DISABLE_BUSY_DETECT  0x00000004L
#define PCINIT_MODEMCONFIG_DISABLE_CALLING_TONE 0x00000008L
#define PCINIT_MODEMCONFIG_DISABLE_ANSWER_TONE  0x00000010L
#define PCINIT_MODEMCONFIG_ENABLE_DIAL_TONE_DET 0x00000020L
#define PCINIT_MODEMCONFIG_USE_POTS_INTERFACE   0x00000040L
#define PCINIT_MODEMCONFIG_FORCE_RAY_TAYLOR_FAX 0x00000080L
#define PCINIT_MODEMCONFIG_DISABLE_RETRAIN      0x00000100L
#define PCINIT_MODEMCONFIG_DISABLE_STEPDOWN     0x00000200L
#define PCINIT_MODEMCONFIG_DISABLE_SPLIT_SPEED  0x00000400L
#define PCINIT_MODEMCONFIG_SHORT_ANSWER_TONE    0x00000800L
#define PCINIT_MODEMCONFIG_ALLOW_RDL_TEST_LOOP  0x00001000L
#define PCINIT_MODEMCONFIG_DISABLE_STEPUP       0x00002000L
#define PCINIT_MODEMCONFIG_DISABLE_FLUSH_TIMER  0x00004000L
#define PCINIT_MODEMCONFIG_REVERSE_DIRECTION    0x00008000L
#define PCINIT_MODEMCONFIG_DISABLE_TX_REDUCTION 0x00010000L
#define PCINIT_MODEMCONFIG_DISABLE_PRECODING    0x00020000L
#define PCINIT_MODEMCONFIG_DISABLE_PREEMPHASIS  0x00040000L
#define PCINIT_MODEMCONFIG_DISABLE_SHAPING      0x00080000L
#define PCINIT_MODEMCONFIG_DISABLE_NONLINEAR_EN 0x00100000L
#define PCINIT_MODEMCONFIG_DISABLE_MANUALREDUCT 0x00200000L
#define PCINIT_MODEMCONFIG_DISABLE_16_POINT_TRN 0x00400000L
#define PCINIT_MODEMCONFIG_EXTENDED_TRAINING    0x00800000L
#define PCINIT_MODEMCONFIG_DISABLE_2400_SYMBOLS 0x01000000L
#define PCINIT_MODEMCONFIG_DISABLE_2743_SYMBOLS 0x02000000L
#define PCINIT_MODEMCONFIG_DISABLE_2800_SYMBOLS 0x04000000L
#define PCINIT_MODEMCONFIG_DISABLE_3000_SYMBOLS 0x08000000L
#define PCINIT_MODEMCONFIG_DISABLE_3200_SYMBOLS 0x10000000L
#define PCINIT_MODEMCONFIG_DISABLE_3429_SYMBOLS 0x20000000L
#define PCINIT_MODEMCONFIG_EXTENDED_LEC         0x80000000L
#define PCINIT_MODEM_NEGOTIATE_HIGHEST          0x00
#define PCINIT_MODEM_NEGOTIATE_DISABLED         0x01
#define PCINIT_MODEM_NEGOTIATE_IN_CLASS         0x02
#define PCINIT_MODEM_NEGOTIATE_V100             0x03
#define PCINIT_MODEM_NEGOTIATE_V8               0x04
#define PCINIT_MODEM_NEGOTIATE_V8BIS            0x05
#define PCINIT_MODEM_NEGOTIATE_CHOICES          0x06
#define PCINIT_MODEMMODULATION_DISABLE_V21      0x00000001L
#define PCINIT_MODEMMODULATION_DISABLE_V23      0x00000002L
#define PCINIT_MODEMMODULATION_DISABLE_V22      0x00000004L
#define PCINIT_MODEMMODULATION_DISABLE_V22BIS   0x00000008L
#define PCINIT_MODEMMODULATION_DISABLE_V32      0x00000010L
#define PCINIT_MODEMMODULATION_DISABLE_V32BIS   0x00000020L
#define PCINIT_MODEMMODULATION_DISABLE_V34      0x00000040L
#define PCINIT_MODEMMODULATION_DISABLE_V90      0x00000080L
#define PCINIT_MODEMMODULATION_DISABLE_BELL103  0x00000100L
#define PCINIT_MODEMMODULATION_DISABLE_BELL212A 0x00000200L
#define PCINIT_MODEMMODULATION_DISABLE_VFC      0x00000400L
#define PCINIT_MODEMMODULATION_DISABLE_K56FLEX  0x00000800L
#define PCINIT_MODEMMODULATION_DISABLE_X2       0x00001000L
#define PCINIT_MODEMMODULATION_ENABLE_V29FDX    0x00010000L
#define PCINIT_MODEMMODULATION_ENABLE_V33       0x00020000L
#define PCINIT_MODEMMODULATION_ENABLE_V90A      0x00040000L
#define PCINIT_MODEMMODULATION_ENABLE_V22FC     0x00080000L
#define PCINIT_MODEMMODULATION_ENABLE_V22BISFC  0x00100000L
#define PCINIT_MODEMMODULATION_ENABLE_V29FC     0x00200000L
#define PCINIT_MODEMMODULATION_ENABLE_V27FC     0x00400000L
#define PCINIT_MODEM_TRANSMIT_LEVEL_CHOICES     0x10
/*------------------------------------------------------------------*/
#define PCINIT_FAXCONFIG_DISABLE_FINE               0x00000001
#define PCINIT_FAXCONFIG_DISABLE_ECM                0x00000002
#define PCINIT_FAXCONFIG_ECM_64_BYTES               0x00000004
#define PCINIT_FAXCONFIG_DISABLE_2D_CODING          0x00000008
#define PCINIT_FAXCONFIG_DISABLE_T6_CODING          0x00000010
#define PCINIT_FAXCONFIG_ENABLE_UNCOMPR             0x00000020
#define PCINIT_FAXCONFIG_REFUSE_POLLING             0x00000040
#define PCINIT_FAXCONFIG_HIDE_TOTAL_PAGES           0x00000080
#define PCINIT_FAXCONFIG_HIDE_ALL_HEADLINE          0x00000100
#define PCINIT_FAXCONFIG_HIDE_PAGE_INFO             0x00000180
#define PCINIT_FAXCONFIG_HEADLINE_OPTIONS_MASK      0x00000180
#define PCINIT_FAXCONFIG_DISABLE_FEATURE_FALLBACK   0x00000200
#define PCINIT_FAXCONFIG_V34FAX_CONTROL_RATE_1200   0x00000800
#define PCINIT_FAXCONFIG_DISABLE_V34FAX             0x00001000
#define PCINIT_FAXCONFIG_DISABLE_T85_CODING         0x00008000
#define PCINIT_FAXCONFIG_DISABLE_JPEG               0x00010000
#define PCINIT_FAXCONFIG_DISABLE_FULL_COLOR_MODE    0x00020000
#define PCINIT_FAXCONFIG_DISABLE_12BIT_MODE         0x00040000
#define PCINIT_FAXCONFIG_DISABLE_CUSTOM_ILLUMINANT  0x00080000
#define PCINIT_FAXCONFIG_DISABLE_CUSTOM_GAMMUT      0x00100000
#define PCINIT_FAXCONFIG_DISABLE_NO_SUBSAMPLING     0x00200000
#define PCINIT_FAXCONFIG_DISABLE_PLANE_INTERLEAVE   0x00400000
#define PCINIT_FAXCONFIG_DISABLE_T43_CODING         0x00800000
#define PCINIT_FAXCONFIG_DISABLE_R8_0770_OR_200     0x01
#define PCINIT_FAXCONFIG_DISABLE_R8_1540            0x02
#define PCINIT_FAXCONFIG_DISABLE_R16_1540_OR_400    0x04
#define PCINIT_FAXCONFIG_DISABLE_R4_0385_OR_100     0x08
#define PCINIT_FAXCONFIG_DISABLE_300_300            0x10
#define PCINIT_FAXCONFIG_DISABLE_300_400_JPEG       0x20
#define PCINIT_FAXCONFIG_DISABLE_INCH_BASED         0x40
#define PCINIT_FAXCONFIG_DISABLE_METRIC_BASED       0x80
#define PCINIT_FAXCONFIG_REC_WIDTH_ISO_A3       0
#define PCINIT_FAXCONFIG_REC_WIDTH_ISO_B4       1
#define PCINIT_FAXCONFIG_REC_WIDTH_ISO_A4       2
#define PCINIT_FAXCONFIG_REC_WIDTH_COUNT        3
#define PCINIT_FAXCONFIG_REC_LENGTH_UNLIMITED   0
#define PCINIT_FAXCONFIG_REC_LENGTH_ISO_B4      1
#define PCINIT_FAXCONFIG_REC_LENGTH_ISO_A4      2
#define PCINIT_FAXCONFIG_REC_LENGTH_COUNT       3
#define PCINIT_FAXCONFIG_SCANLINE_TIME_00_00_00 0
#define PCINIT_FAXCONFIG_SCANLINE_TIME_05_05_05 1
#define PCINIT_FAXCONFIG_SCANLINE_TIME_10_05_05 2
#define PCINIT_FAXCONFIG_SCANLINE_TIME_10_10_10 3
#define PCINIT_FAXCONFIG_SCANLINE_TIME_20_10_10 4
#define PCINIT_FAXCONFIG_SCANLINE_TIME_20_20_20 5
#define PCINIT_FAXCONFIG_SCANLINE_TIME_40_20_20 6
#define PCINIT_FAXCONFIG_SCANLINE_TIME_40_40_40 7
#define PCINIT_FAXCONFIG_SCANLINE_TIME_RES_8    8
#define PCINIT_FAXCONFIG_SCANLINE_TIME_RES_9    9
#define PCINIT_FAXCONFIG_SCANLINE_TIME_RES_10   10
#define PCINIT_FAXCONFIG_SCANLINE_TIME_10_10_05 11
#define PCINIT_FAXCONFIG_SCANLINE_TIME_20_10_05 12
#define PCINIT_FAXCONFIG_SCANLINE_TIME_20_20_10 13
#define PCINIT_FAXCONFIG_SCANLINE_TIME_40_20_10 14
#define PCINIT_FAXCONFIG_SCANLINE_TIME_40_40_20 15
#define PCINIT_FAXCONFIG_SCANLINE_TIME_COUNT    16
#define PCINIT_FAXCONFIG_DISABLE_TX_REDUCTION   0x00010000L
#define PCINIT_FAXCONFIG_DISABLE_PRECODING      0x00020000L
#define PCINIT_FAXCONFIG_DISABLE_PREEMPHASIS    0x00040000L
#define PCINIT_FAXCONFIG_DISABLE_SHAPING        0x00080000L
#define PCINIT_FAXCONFIG_DISABLE_NONLINEAR_EN   0x00100000L
#define PCINIT_FAXCONFIG_DISABLE_MANUALREDUCT   0x00200000L
#define PCINIT_FAXCONFIG_DISABLE_16_POINT_TRN   0x00400000L
#define PCINIT_FAXCONFIG_EXTENDED_TRAINING      0x00800000L
#define PCINIT_FAXCONFIG_DISABLE_2400_SYMBOLS   0x01000000L
#define PCINIT_FAXCONFIG_DISABLE_2743_SYMBOLS   0x02000000L
#define PCINIT_FAXCONFIG_DISABLE_2800_SYMBOLS   0x04000000L
#define PCINIT_FAXCONFIG_DISABLE_3000_SYMBOLS   0x08000000L
#define PCINIT_FAXCONFIG_DISABLE_3200_SYMBOLS   0x10000000L
#define PCINIT_FAXCONFIG_DISABLE_3429_SYMBOLS   0x20000000L
/*--------------------------------------------------------------------------*/
#define PCINIT_XDI_CMA_FOR_ALL_NL_PRIMITIVES    0x01
/*--------------------------------------------------------------------------*/
#define PCINIT_FPGA_PLX_ACCESS_SUPPORTED        0x01
#define PCINIT_FPGA_PROGRAMMABLE_OPERATION_MODES_SUPPORTED 0x02
/*--------------------------------------------------------------------------*/
#define PCINIT_TXATTENUATION_00DBM              0x00
#define PCINIT_TXATTENUATION_07DBM              0x01
#define PCINIT_TXATTENUATION_15DBM              0x02
/*--------------------------------------------------------------------------*/
/* PCINIT_OPTIMIZATION                                                      */
/* Bit mask                                                                 */
/*--------------------------------------------------------------------------*/
#define PCINIT_OPTIMIZATION_NET_CONNECTION      0x01
#define PCINIT_OPTIMIZATION_NO_LI               0x02
/*--------------------------------------------------------------------------*/
#endif
/*--------------------------------------------------------------------------*/
