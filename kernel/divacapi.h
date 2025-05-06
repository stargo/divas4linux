
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

#define IMPLEMENT_DTMF 1
#define IMPLEMENT_LINE_INTERCONNECT2 1
#define IMPLEMENT_ECHO_CANCELLER 1
#define IMPLEMENT_RTP 1
#define IMPLEMENT_T38 1
#define IMPLEMENT_FAX_SUB_SEP_PWD 1
#define IMPLEMENT_V18 1
#define IMPLEMENT_DTMF_TONE 1
#define IMPLEMENT_PIAFS 1
#define IMPLEMENT_FAX_PAPER_FORMATS 1
#define IMPLEMENT_VOWN 1
#define IMPLEMENT_CAPIDTMF 1
#define IMPLEMENT_FAX_NONSTANDARD 1
#define IMPLEMENT_SS7 1
#define IMPLEMENT_LISTENING 1
#define IMPLEMENT_MEASURE 1
#define IMPLEMENT_PITCH_CONTROL 1
#define IMPLEMENT_AUDIO_CONTROL 1
#define IMPLEMENT_NULL_PLCI 1
#define VSWITCH_SUPPORT 1


#define IMPLEMENT_LINE_INTERCONNECT 0
#define IMPLEMENT_MARKED_OK_AFTER_FC 1
#define IMPLEMENT_PLCI_TRACE 1
#define IMPLEMENT_X25_REVERSE_RESTART 1


#include "capidtmf.h"


/*------------------------------------------------------------------*/
/* Common API internal definitions                                  */
/*------------------------------------------------------------------*/

#define MAX_APPL 254
#define MAX_NCCI           127
#define MSG_IN_QUEUE_SIZE  ((4096*2 + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1))
#define C_IND_RESOURCE_INDEX_ENTRIES  64

#define MSG_IN_OVERHEAD    sizeof(APPL   *)

#define MAX_NL_CHANNEL     255
#define MAX_DATA_B3        8
#define MAX_DATA_ACK       MAX_DATA_B3
#define MAX_MULTI_IE       6
#define MAX_MSG_SIZE       2100
#define MAX_MSG_PARMS      20
#define MAX_CPN_MASK_SIZE  16
#define MAX_MSN_CONFIG     10
#define EXT_CONTROLLER     0x80
#define CODEC              0x01
#define CODEC_PERMANENT    0x02
#define ADV_VOICE          0x03
#define APPL_MASK_DWORDS   ((MAX_APPL+32) >> 5)


#define FAX_CONNECT_INFO_BUFFER_SIZE  256
#define NCPI_BUFFER_SIZE              256

#define MAX_CHANNELS_PER_PLCI         8
#define MAX_INTERNAL_COMMAND_LEVELS   4
#define INTERNAL_REQ_BUFFER_SIZE      272

#define INTERNAL_IND_BUFFER_SIZE      768

#define DTMF_PARAMETER_BUFFER_SIZE    64
#define ADV_VOICE_COEF_BUFFER_SIZE    50

#define LI_PLCI_B_QUEUE_ENTRIES       256

#define TRACE_QUEUE_ENTRIES           16

#define DIVA_CAPI_APPL_VSCAPI_ACCESS 0x01

struct _diva_mtpx_adapter_xdi_config_entry;
typedef struct _APPL APPL;
typedef struct _PLCI PLCI;
typedef struct _NCCI NCCI;
typedef struct _DIVA_CAPI_ADAPTER DIVA_CAPI_ADAPTER;
typedef struct _DATA_B3_DESC DATA_B3_DESC;
typedef struct _DATA_ACK_DESC DATA_ACK_DESC;
typedef struct fax_ncpi_s FAX_NCPI;
typedef struct api_parse_s API_PARSE;
typedef struct api_save_s API_SAVE;
typedef struct msn_config_s MSN_CONFIG;
typedef struct msn_config_max_s MSN_CONFIG_MAX;



#define MEASURE_MODULATOR_COUNT       16
#define MEASURE_FUNCTION_COUNT        4
#define MEASURE_SINEGEN_COUNT         4
#define MEASURE_FUNCGEN_COUNT         3
#define MEASURE_NOISEGEN_COUNT        1
#define MEASURE_FSKGEN_COUNT          1

#define MEASURE_MAX_RX_FILTER_POINTS  16
#define MEASURE_MAX_TX_FILTER_POINTS  16

struct measure_timer_setup_s {
  word periode;
};

struct measure_rx_selector_setup_s {
  word path;
};

struct measure_rx_filter_point_s {
  word frequency;
  word attenuation;
};

struct measure_rx_filter_setup_s {
  word curve_points;
  struct measure_rx_filter_point_s point_table[MEASURE_MAX_RX_FILTER_POINTS];
};

struct measure_spectrum_setup_s {
  word reserved;
  word flags;
  word peaks;
  word aperture;
};

struct measure_cepstrum_setup_s {
  word reserved;
  word flags;
  word peaks;
};

struct measure_echo_setup_s {
  word reserved;
  word flags;
  word peaks;
  word min_delay;
  word max_delay;
  word accumulate_time;
};

struct measure_single_tone_setup_s {
  word reserved;
  word flags;
  word min_duration;
  word min_snr;
  word min_level;
  word max_amplitude_modulation;
  word max_frequency_modulation;
  word max_dropout;
  word aperture;
};

struct measure_dual_tone_setup_s {
  word reserved;
  word flags;
  word min_duration;
  word min_snr;
  word min_level;
  word max_high_low_twist;
  word max_low_high_twist;
  word max_dropout;
  word aperture;
};

struct measure_fskdet_setup_s {
  word reserved_0;
  word reserved_1;
  word sensitivity;
  word space_frequency;
  word mark_frequency;
  word baud_rate;
  word framer_type;
  word async_format;
  word framer_options;
};

struct measure_tx_selector_setup_s {
  word path;
};

struct measure_tx_filter_point_s {
  word frequency;
  word attenuation;
};

struct measure_tx_filter_setup_s {
  word curve_points;
  struct measure_tx_filter_point_s point_table[MEASURE_MAX_TX_FILTER_POINTS];
};

struct measure_generator_setup_s {
  word sinegen_mask;
  word funcgen_mask;
  word noisegen_mask;
  word fskgen_mask;
};

struct measure_modulator_setup_s {
  word curve_points;
};

struct measure_function_setup_s {
  word curve_points;
};

struct measure_sinegen_setup_s {
  word amplitude_mod;
  word frequency_mod;
};

struct measure_funcgen_setup_s {
  word function;
  word amplitude_mod;
  word frequency_mod;
};

struct measure_noisegen_setup_s {
  word function;
  word amplitude_mod;
};

struct measure_fskgen_setup_s {
  word reserved_0;
  word reserved_1;
  word transmit_gain;
  word space_frequency;
  word mark_frequency;
  word baud_rate;
  word framer_type;
  word async_format;
  word framer_options;
};

struct measure_setup_s {
  struct measure_timer_setup_s       timer;
  struct measure_rx_selector_setup_s rx_selector;
  struct measure_rx_filter_setup_s   rx_filter;
  struct measure_spectrum_setup_s    spectrum;
  struct measure_cepstrum_setup_s    cepstrum;
  struct measure_echo_setup_s        echo;
  struct measure_single_tone_setup_s single_tone;
  struct measure_dual_tone_setup_s   dual_tone;
  struct measure_fskdet_setup_s      fskdet;
  struct measure_tx_selector_setup_s tx_selector;
  struct measure_tx_filter_setup_s   tx_filter;
  struct measure_generator_setup_s   generator;
  struct measure_modulator_setup_s   modulator[MEASURE_MODULATOR_COUNT];
  struct measure_function_setup_s    function[MEASURE_FUNCTION_COUNT];
  struct measure_sinegen_setup_s     sinegen[MEASURE_SINEGEN_COUNT];
  struct measure_funcgen_setup_s     funcgen[MEASURE_FUNCGEN_COUNT];
  struct measure_noisegen_setup_s    noisegen[MEASURE_NOISEGEN_COUNT];
  struct measure_fskgen_setup_s      fskgen[MEASURE_FSKGEN_COUNT];
};



struct fax_ncpi_s {
  word options;
  word format;
};

struct resource_profile_s {
  dword global_options;
  dword b1_protocols;
  dword b2_protocols;
  dword b3_protocols;
  dword manufacturer_features;
  dword rtp_primary_payloads;
  dword rtp_additional_payloads;
  dword manufacturer_features2;
};

#define RESOURCE_PROFILE_DWORDS  (sizeof(struct resource_profile_s) / sizeof(dword))

struct resource_index_entry {
  word references;
  word cmd;
  struct resource_profile_s profile;
};

struct msn_config_s {
  byte msn[MAX_CPN_MASK_SIZE];
};

struct msn_config_max_s {
  MSN_CONFIG    msn_conf[MAX_MSN_CONFIG];
};

struct api_parse_s {
  word          length;
  byte   *    info;
};

struct api_save_s {
  API_PARSE     parms[MAX_MSG_PARMS+1];
  byte          info[MAX_MSG_SIZE];
};

struct _DATA_B3_DESC {
  word          Handle;
  word          Number;
  word          Flags;
  word          Length;
  void   *    P;
};

struct _DATA_ACK_DESC {
  word          Handle;
  word          Number;
};

typedef void (* t_std_internal_command)(dword Id, PLCI   *plci, byte Rc);

/************************************************************************/
/* Don't forget to adapt dos.asm after changing the _APPL structure!!!! */
struct _APPL {
  word          Id;
  word          NullCREnable;
  word          CDEnable;
  dword         S_Handle;






  LIST_ENTRY    s_function;
  dword         s_context;
  word          s_count;
  APPL *        s_next;
  struct xbuffer_element {
    word          next_i;
    word          prev_i;
  } *xbuffer_table;
  void **       xbuffer_internal;
  void **       xbuffer_ptr;






  byte   *    queue;
  word          queue_size;
  word          queue_free;
  word          queue_read;
  word          queue_write;
  word          queue_wrap;
  word          queue_signal;
  word          msg_lost;
  word          appl_flags;
  word          Number;

  word          MaxBuffer;
  word          MaxNCCI;
  word          MaxNCCIData;
  word          MaxDataLength;
  word          NCCIDataFlowCtrlTimer;
  byte   *    ReceiveBuffer;
  word   *    DataNCCI;
  word   *    DataFlags;
  dword         BadDataB3Resp;
  byte          access;


  diva_entity_queue_t saved_action_q;

};


typedef struct _diva_saved_capi_appl_action_hdr {
  diva_entity_link_t link;
  word messageNr;
  void (*action)(struct _PLCI*, struct _diva_saved_capi_appl_action_hdr*);
} diva_saved_capi_appl_action_hdr_t;

typedef struct _diva_saved_capi_appl_stream_action {
  diva_saved_capi_appl_action_hdr_t hdr;
  int                               length;
  byte                              stream_data[256];
} diva_saved_capi_appl_stream_action_t;


struct _PLCI {
  ENTITY        Sig;
  ENTITY        NL;
  word          RNum;
  word          RFlags;
  BUFFERS       RData[2];
  BUFFERS       XData[1];
  BUFFERS       NData[2];

  DIVA_CAPI_ADAPTER   *adapter;
  APPL      *appl;
  PLCI      *relatedPTYPLCI;
  byte          Id;
  byte          NextId;
  byte          State;
  byte          sig_req;
  byte          nl_req;
  byte          SuppState;
  byte          channels;
  byte          tel;
  word          B1_resource;
  byte          B1_options;
  byte          B2_prot;
  byte          B3_prot;

  word          command;
  word          m_command;
  word          internal_command;
  word          number;
  word          req_in_start;
  word          req_in;
  word          req_out;
  word          msg_in_write_pos;
  word          msg_in_read_pos;
  word          msg_in_wrap_pos;

  void   *    data_sent_ptr;
  byte          data_sent;
  byte          send_disc;
  byte          sig_global_req;
  byte          sig_remove_id;
  byte          nl_global_req;
  byte          nl_remove_id;
  word          nl_remove_ncci;
  byte          nl_assign_rc;
  byte          b_channel;
  byte          adv_nl;
  byte          call_dir;
  byte          hook_state;
  byte          spoofed_msg;
  byte          ptyState;
  APPL      *ptyAppl;
  byte          cr_enquiry;
  word          hangup_flow_ctrl_timer;

  word          ch_flow_ring_list;
  word          ncci_ring_list;
  byte          inc_dis_ncci_table[MAX_CHANNELS_PER_PLCI];
  t_std_internal_command internal_command_queue[MAX_INTERNAL_COMMAND_LEVELS];
  dword         c_ind_mask_table[APPL_MASK_DWORDS];
  byte          c_ind_reserve_info[MAX_APPL + 1];
  word          c_ind_reserve_msg_number[MAX_APPL];
  struct resource_profile_s reserve_request_profile;
  struct resource_profile_s reserve_granted_profile;
  byte          reserve_send_request;
  byte          reserve_send_pending;
  byte          reserve_request_pending;
  word          reserve_pending_cmd;
  word          reserve_pending_msg_number;

  byte          RBuffer[2048];
  void   *    (msg_in_queue[MSG_IN_QUEUE_SIZE/sizeof(void   *)]);
  API_SAVE      saved_msg;
  API_SAVE      B_protocol;
  byte          fax_connect_info_length;
  byte          fax_connect_info_buffer[FAX_CONNECT_INFO_BUFFER_SIZE];
  byte          fax_code;
  word          fax_feature_bits;
  word          fax_feature2_bits;
  byte          fax_edata_ack_length;
  word          nsf_control_bits;
  byte          break_configuration;
  byte          ncpi_state;
  byte          ncpi_buffer[NCPI_BUFFER_SIZE];

  word          trace_queue[TRACE_QUEUE_ENTRIES];
  word          trace_pos;


  byte          internal_req_buffer[INTERNAL_REQ_BUFFER_SIZE];
  byte          internal_ind_buffer[INTERNAL_IND_BUFFER_SIZE + (sizeof(void   *)-1)];
  dword         requested_options_conn;
  dword         requested_options;
  word          B1_facilities;
  API_SAVE   *adjust_b_parms_msg;
  word          adjust_b_facilities;
  word          adjust_b_command;
  word          adjust_b_ncci;
  word          adjust_b_mode;
  word          adjust_b_state;
  word          adjust_b_info;
  word          adjust_b_prev_b1_facilities;
  byte          adjust_b_restore;
  word          adjust_b_prev_b1_resource;

  word          dtmf_rec_active;
  word          dtmf_rec_active_modem_fax;
  word          dtmf_rec_pulse_ms;
  word          dtmf_rec_pause_ms;
  byte          dtmf_pending_dtmf_digit_code;
  byte          dtmf_pending_mf_digit_code;
  byte          dtmf_pending_r2fwd_digit_code;
  byte          dtmf_pending_r2bwd_digit_code;
  byte          dtmf_send_requests;
  word          dtmf_send_pulse_ms;
  word          dtmf_send_pause_ms;
  word          dtmf_cmd;
  word          dtmf_msg_number_queue[8];
  byte          dtmf_parameter_length;
  byte          dtmf_parameter_buffer[DTMF_PARAMETER_BUFFER_SIZE];

  t_capidtmf_state capidtmf_state;



  word          dtmf_ondata_rec_active;
  word          dtmf_ondata_rec_pulse_ms;
  word          dtmf_ondata_rec_pause_ms;
  byte          dtmf_ondata_pending_dtmf_digit_code;
  byte          tone_last_indication_code;

  t_capidtmf_state capidtmf_ondata_state;



  word          li_bchannel_id;    /* BRI: 1..2, PRI: (1),2,..32 */
  byte          li_channel_bits;
  byte          li_notify_update;
  byte          li_update_waiting;
  byte          li_error;
  PLCI      *li_update_wait_next;
  word          li_msg_number;
  word          li_cmd;
  word          li_write_command;
  word          li_write_channel;
  byte          li_write_needs_sync;
  byte          li_write_is_sync;
  word          li_plci_b_write_pos;
  word          li_plci_b_read_pos;
  word          li_plci_b_req_pos;
  dword         li_plci_b_queue[LI_PLCI_B_QUEUE_ENTRIES];


  word          ec_cmd;
  word          ec_idi_options;
  word          ec_span_ms;
  word          ec_predelay_ms;
  word          ec_sparse_span_ms;


  word          measure_active;
  word          measure_cmd;
  struct measure_setup_s measure_setup;


  word          pitch_flags;
  word          pitch_rx_sample_rate;
  word          pitch_tx_sample_rate;


  word          audio_flags;
  word          tx_digital_gain;
  word          rx_digital_gain;

  byte          sig_tx_tone[5];
  byte          sig_tx_digits[4];
  byte          sig_tx_set_detector[5];
  byte          sig_tx_set_generator[5];
  byte          sig_tx_task_control[5];
  byte          sig_tx_set_modulation[7];

  word          rtp_cmd;
  byte          rtp_send_requests;
  word          rtp_msg_number_queue[8];


  word          t38_cmd;
  byte          t38_send_requests;
  word          t38_msg_number_queue[8];

  byte          vswitchstate;
  byte          vsprot;
  byte          vsprotdialect;
  byte          notifiedcall; /* Flag if it is a spoofed call */

  int           rx_dma_descriptor;
  dword         rx_dma_magic;
  byte          calledfromccbscall;
  byte          gothandle;
  dword         S_Handle;
  byte          waitforECTindOnRPlci;
  struct callinfo_s{
    byte        scs7ie[24];
    byte        scntinfo[45];
    byte        linkident[6];
    byte        cardident[10];
  }cinfo;
  PLCI       *relatedConsultPLCI;
  word          lastrejectcause;
  byte          ncrl_esccai[6];

  byte          stream_data[256];
};


struct _NCCI {
  byte          data_out;
  byte          data_pending;
  byte          data_ack_out;
  byte          data_ack_pending;
  DATA_B3_DESC  DBuffer[MAX_DATA_B3];
  DATA_ACK_DESC DataAck[MAX_DATA_ACK];
};


typedef struct _DIVA_CAPI_ADAPTER_MTPX_CFG {
  dword number_of_configuration_entries;
  const struct _diva_mtpx_adapter_xdi_config_entry* cfg;
} DIVA_CAPI_ADAPTER_MTPX_CFG_t;


struct _DIVA_CAPI_ADAPTER {
  IDI_CALL      request;
  byte          Id;
  byte          max_plci;
  byte          max_listen;
  byte          listen_active;
  PLCI      *plci;
  byte          first_free_plci_id;
  byte          last_free_plci_id;
  byte          ncrl_plci_id;
  byte          hangup_flow_control_plcis;
  byte          ch_ncci[MAX_NL_CHANNEL+1];
  byte          ncci_ch[MAX_NCCI+1];
  byte          ncci_plci[MAX_NCCI+1];
  byte          ncci_state[MAX_NCCI+1];
  byte          ncci_next[MAX_NCCI+1];
  NCCI          ncci[MAX_NCCI+1];

  byte          ch_flow_control[MAX_NL_CHANNEL+1];  /* Used by XON protocol */
  byte          ch_flow_control_pending;
  byte          ch_flow_plci[MAX_NL_CHANNEL+1];
  byte          ch_flow_next[MAX_NL_CHANNEL+1];

  dword         Info_Mask[MAX_APPL];
  dword         CIP_Mask[MAX_APPL];
  dword         ValidServicesCIPMask;       /* Configuration may restrict servcies */

  dword         Notification_Mask[MAX_APPL];
  PLCI      *codec_listen[MAX_APPL];
  dword         requested_options_table[MAX_APPL];
  API_PROFILE   profile;
  dword         rtp_primary_payloads;
  dword         rtp_additional_payloads;
  dword         manufacturer_features;
  dword         manufacturer_features2;

  byte          AdvCodecFLAG;
  PLCI      *AdvCodecPLCI;
  PLCI      *AdvSignalPLCI;
  APPL      *AdvSignalAppl;
  byte          TelOAD[23];
  byte          TelOSA[23];
  byte          scom_appl_disable;
  PLCI      *automatic_lawPLCI;
  byte          automatic_law;
  byte          u_law;

  byte          adv_voice_coef_length;
  byte          adv_voice_coef_buffer[ADV_VOICE_COEF_BUFFER_SIZE];

  byte          null_plci_base;
  byte          null_plci_channels;
  byte          null_plci_table[64];


  byte          li_pri;
  byte          li_query_channel_supported;
  byte          li_channels;
  word          li_base;

  byte adapter_disabled;
  byte group_optimization_enabled; /* use application groups if enabled */
  dword sdram_bar;
  byte flag_dynamic_l1_down; /* for hunt groups:down layer 1 if no appl present*/
  byte FlowControlIdTable[256];
  byte FlowControlSkipTable[256];
  void* os_card; /* pointer to associated OS dependent adapter structure */
  /*
    True if current adapter is MTPX adapter,
    MTPX adapter interface version
    */
  int   mtpx_adapter;

  byte xdi_logical_adapter_number; /* Logical adapter number of first XDI adapter associated to MTPX adapter */
  byte vscapi_mode; /* CAPI controller is used in vscapi mode */


  DIVA_CAPI_ADAPTER_MTPX_CFG_t mtpx_cfg;

};


/*------------------------------------------------------------------*/
/* Group optimisation options                                       */
/*------------------------------------------------------------------*/

#define GROUPOPT_DISABLE                0x00  /* No group optimisation */
#define GROUPOPT_ONLY_ON_SINGLE_CONN    0x01  /* Optimisation only for groups with MaxNCCI==1 */
#define GROUPOPT_SINGLE_CONN_TO_MULTI   0x02  /* Optimisation for all groups as if MaxNCCI==1 */
#define GROUPOPT_SINGLE_AND_MULTI_CONN  0x03  /* Optimisation for all groups according to MaxNCCI */
#define GROUPOPT_ENABLE_MASK            0x03
#define GROUPOPT_ALSO_ON_INDIVIDUALS    0x04  /* Optimisation also for individual applications */


/*------------------------------------------------------------------*/
/* Application flags                                                */
/*------------------------------------------------------------------*/

#define APPL_FLAG_OLD_LI_SPEC           0x01
#define APPL_FLAG_PRIV_EC_SPEC          0x02
#define APPL_FLAG_OLD_EC_SELECTOR       0x04
#define APPL_FLAG_USES_STREAMING        0x08


/*------------------------------------------------------------------*/
/* API parameter definitions                                        */
/*------------------------------------------------------------------*/

#define X75_TTX         1       /* x.75 for ttx                     */
#define TRF             2       /* transparent with hdlc framing    */
#define TRF_IN          3       /* transparent with hdlc fr. inc.   */
#define SDLC            4       /* sdlc, sna layer-2                */
#define X75_BTX         5       /* x.75 for btx                     */
#define LAPD            6       /* lapd (Q.921)                     */
#define X25_L2          7       /* x.25 layer-2                     */
#define V120_L2         8       /* V.120 layer-2 protocol           */
#define V42_IN          9       /* V.42 layer-2 protocol, incomming */
#define V42            10       /* V.42 layer-2 protocol            */
#define MDM_ATP        11       /* AT Parser built in the L2        */
#define X75_V42BIS     12       /* ISO7776 (X.75 SLP) modified to support V.42 bis compression */
#define RTPL2_IN       13       /* RTP layer-2 protocol, incomming  */
#define RTPL2          14       /* RTP layer-2 protocol             */
#define V120_V42BIS    15       /* V.120 layer-2 protocol supporting V.42 bis compression */

#define T70NL           1
#define X25PLP          2
#define T70NLX          3
#define TRANSPARENT_NL  4
#define ISO8208         5
#define T30             6


/*------------------------------------------------------------------*/
/* FAX interface to IDI                                             */
/*------------------------------------------------------------------*/

#define CAPI_MAX_HEAD_LINE_SPACE        89
#define CAPI_MAX_DATE_TIME_LENGTH       18

#define T30_MAX_STATION_ID_LENGTH       20
#define T30_MAX_SUBADDRESS_LENGTH       20
#define T30_MAX_PASSWORD_LENGTH         20

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
  byte  /*req*/ resolution_high;      /* ind resolution_high      */
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


#define T30_RESOLUTION_R8_0385          0x0000
#define T30_RESOLUTION_R8_0770_OR_200   0x0001
#define T30_RESOLUTION_R8_1540          0x0002
#define T30_RESOLUTION_R16_1540_OR_400  0x0004
#define T30_RESOLUTION_R4_0385_OR_100   0x0008
#define T30_RESOLUTION_300_300          0x0010
#define T30_RESOLUTION_300_400_JPEG     0x0020
#define T30_RESOLUTION_INCH_BASED       0x0040
#define T30_RESOLUTION_METRIC_BASED     0x0080
#define T30_RESOLUTION_600_600          0x0100
#define T30_RESOLUTION_1200_1200        0x0200
#define T30_RESOLUTION_300_600          0x0400
#define T30_RESOLUTION_400_800          0x0800
#define T30_RESOLUTION_600_1200         0x1000
#define T30_RESOLUTION_600_600_JPEG     0x2000
#define T30_RESOLUTION_1200_1200_JPEG   0x4000

#define T30_RECORDING_WIDTH_ISO_A4      0
#define T30_RECORDING_WIDTH_ISO_B4      1
#define T30_RECORDING_WIDTH_ISO_A3      2
#define T30_RECORDING_WIDTH_COUNT       3

#define T30_RECORDING_LENGTH_ISO_A4     0
#define T30_RECORDING_LENGTH_ISO_B4     1
#define T30_RECORDING_LENGTH_UNLIMITED  2
#define T30_RECORDING_LENGTH_COUNT      3

#define T30_MIN_SCANLINE_TIME_00_00_00  0
#define T30_MIN_SCANLINE_TIME_05_05_05  1
#define T30_MIN_SCANLINE_TIME_10_05_05  2
#define T30_MIN_SCANLINE_TIME_10_10_10  3
#define T30_MIN_SCANLINE_TIME_20_10_10  4
#define T30_MIN_SCANLINE_TIME_20_20_20  5
#define T30_MIN_SCANLINE_TIME_40_20_20  6
#define T30_MIN_SCANLINE_TIME_40_40_40  7
#define T30_MIN_SCANLINE_TIME_RES_8     8
#define T30_MIN_SCANLINE_TIME_RES_9     9
#define T30_MIN_SCANLINE_TIME_RES_10    10
#define T30_MIN_SCANLINE_TIME_10_10_05  11
#define T30_MIN_SCANLINE_TIME_20_10_05  12
#define T30_MIN_SCANLINE_TIME_20_20_10  13
#define T30_MIN_SCANLINE_TIME_40_20_10  14
#define T30_MIN_SCANLINE_TIME_40_40_20  15
#define T30_MIN_SCANLINE_TIME_COUNT     16

#define T30_DATA_FORMAT_SFF             0
#define T30_DATA_FORMAT_PLAIN_MH        1
#define T30_DATA_FORMAT_PCX             2
#define T30_DATA_FORMAT_DCX             3
#define T30_DATA_FORMAT_TIFF            4
#define T30_DATA_FORMAT_ASCII           5
#define T30_DATA_FORMAT_EXTENDED_ANSI   6
#define T30_DATA_FORMAT_BFT             7
#define T30_DATA_FORMAT_CFF             8
#define T30_DATA_FORMAT_COUNT           9


#define T30_OPERATING_MODE_BIT_INFO_EX  0x80
#define T30_OPERATING_MODE_NUMBER_MASK  0x0f
#define T30_OPERATING_MODE_STANDARD     0
#define T30_OPERATING_MODE_CLASS2       1
#define T30_OPERATING_MODE_CLASS1       2
#define T30_OPERATING_MODE_CAPI         3
#define T30_OPERATING_MODE_CAPI_NEG     4
#define T30_OPERATING_MODE_MONITOR      5
#define T30_OPERATING_MODE_COUNT        6

        /* EDATA transmit messages */
#define EDATA_T30_DIS         0x01
#define EDATA_T30_FTT         0x02
#define EDATA_T30_MCF         0x03
#define EDATA_T30_PROGRESS    0x04

        /* EDATA receive messages */
#define EDATA_T30_DCS         0x81
#define EDATA_T30_TRAIN_OK    0x82
#define EDATA_T30_EOP         0x83
#define EDATA_T30_MPS         0x84
#define EDATA_T30_EOM         0x85
#define EDATA_T30_DTC         0x86
#define EDATA_T30_PAGE_END    0x87   /* Indicates end of page data. Reserved, but not implemented ! */
#define EDATA_T30_EOP_CAPI    0x88


#define T30_SUCCESS                         0
#define T30_ERR_NO_ENERGY_DETECTED          1
#define T30_ERR_TIMEOUT_NO_RESPONSE         2
/* #define T30_ERR_RETRY_NO_RESPONSE           3 */
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


#define T30_CONTROL_BIT_DISABLE_FINE        0x0001
#define T30_CONTROL_BIT_ENABLE_ECM          0x0002
#define T30_CONTROL_BIT_ECM_64_BYTES        0x0004
#define T30_CONTROL_BIT_ENABLE_2D_CODING    0x0008
#define T30_CONTROL_BIT_ENABLE_T6_CODING    0x0010
#define T30_CONTROL_BIT_ENABLE_UNCOMPR      0x0020
#define T30_CONTROL_BIT_ACCEPT_POLLING      0x0040
#define T30_CONTROL_BIT_REQUEST_POLLING     0x0080
#define T30_CONTROL_BIT_MORE_DOCUMENTS      0x0100
#define T30_CONTROL_BIT_ACCEPT_SUBADDRESS   0x0200
#define T30_CONTROL_BIT_ACCEPT_SEL_POLLING  0x0400
#define T30_CONTROL_BIT_ACCEPT_PASSWORD     0x0800
#define T30_CONTROL_BIT_ENABLE_V34FAX       0x1000
#define T30_CONTROL_BIT_EARLY_CONNECT       0x2000
#define T30_CONTROL_BIT_ENABLE_T85_CODING   0x8000

#define T30_CONTROL_BIT_ALL_FEATURES  (T30_CONTROL_BIT_ENABLE_ECM | T30_CONTROL_BIT_ENABLE_2D_CODING |   T30_CONTROL_BIT_ENABLE_T6_CODING | T30_CONTROL_BIT_ENABLE_UNCOMPR |   T30_CONTROL_BIT_ENABLE_V34FAX | T30_CONTROL_BIT_ENABLE_T85_CODING)

#define T30_FEATURE_BIT_FINE                0x0001
#define T30_FEATURE_BIT_ECM                 0x0002
#define T30_FEATURE_BIT_ECM_64_BYTES        0x0004
#define T30_FEATURE_BIT_2D_CODING           0x0008
#define T30_FEATURE_BIT_T6_CODING           0x0010
#define T30_FEATURE_BIT_UNCOMPR_ENABLED     0x0020
#define T30_FEATURE_BIT_POLLING             0x0040
#define T30_FEATURE_BIT_EOP                 0x0080
#define T30_FEATURE_BIT_MORE_DOCUMENTS      0x0100
#define T30_FEATURE_COPY_QUALITY_NOERR      0x0000
#define T30_FEATURE_COPY_QUALITY_OK         0x0200
#define T30_FEATURE_COPY_QUALITY_RTP        0x0400
#define T30_FEATURE_COPY_QUALITY_RTN        0x0600
#define T30_FEATURE_COPY_QUALITY_MASK       0x0600
#define T30_FEATURE_BIT_V34FAX              0x1000
#define T30_FEATURE_BIT_T85_CODING          0x8000

#define T30_CONTROL2_BIT_ENABLE_JPEG        0x0001
#define T30_CONTROL2_BIT_FULL_COLOR_MODE    0x0002
#define T30_CONTROL2_BIT_ENABLE_12BIT_MODE  0x0004
#define T30_CONTROL2_BIT_CUSTOM_ILLUMINANT  0x0008
#define T30_CONTROL2_BIT_CUSTOM_GAMMUT      0x0010
#define T30_CONTROL2_BIT_NO_SUBSAMPLING     0x0020
#define T30_CONTROL2_BIT_PREF_HUFFMAN_TAB   0x0040
#define T30_CONTROL2_BIT_PLANE_INTERLEAVE   0x0040
#define T30_CONTROL2_BIT_ENABLE_T43_CODING  0x0080
#define T30_CONTROL2_BIT_ACCEPT_PRI         0x0100
#define T30_CONTROL2_BIT_REQUEST_PRI        0x0200

#define T30_FEATURE2_BIT_JPEG               0x0001
#define T30_FEATURE2_BIT_FULL_COLOR_MODE    0x0002
#define T30_FEATURE2_BIT_12BIT_MODE         0x0004
#define T30_FEATURE2_BIT_CUSTOM_ILLUMINANT  0x0008
#define T30_FEATURE2_BIT_CUSTOM_GAMMUT      0x0010
#define T30_FEATURE2_BIT_NO_SUBSAMPLING     0x0020
#define T30_FEATURE2_BIT_PREF_HUFFMAN_TAB   0x0040
#define T30_FEATURE2_BIT_PLANE_INTERLEAVE   0x0040
#define T30_FEATURE2_BIT_T43_CODING         0x0080
#define T30_FEATURE2_BIT_PRI                0x0100

#define T30_NSF_CONTROL_BIT_ENABLE_NSF      0x0001
#define T30_NSF_CONTROL_BIT_RAW_NSF_INFO    0x0002
#define T30_NSF_CONTROL_BIT_NEGOTIATE_IND   0x0004
#define T30_NSF_CONTROL_BIT_NEGOTIATE_RESP  0x0008
#define T30_NSF_CONTROL_BIT_INFO_IND        0x0010
#define T30_NSF_CONTROL_BIT_DIS_DTC_INFO    0x0020
#define T30_NSF_CONTROL_BIT_PROGRESS_REPORT 0x0040

#define T30_NSF_ELEMENT_NSF_FIF             0x00
#define T30_NSF_ELEMENT_NSC_FIF             0x01
#define T30_NSF_ELEMENT_NSS_FIF             0x02
#define T30_NSF_ELEMENT_COMPANY_NAME        0x03
#define T30_NSF_ELEMENT_FAX_DEVICE          0x04
#define T30_NSF_ELEMENT_DIS_FIF             0x05
#define T30_NSF_ELEMENT_DTC_FIF             0x06
#define T30_NSF_ELEMENT_DCS_FIF_REPORT      0x07
#define T30_NSF_ELEMENT_TRAIN_CHECK_REPORT  0x08
#define T30_NSF_ELEMENT_CFR_REPORT          0x09
#define T30_NSF_ELEMENT_PAGE_QUALITY_REPORT 0x0a
#define T30_NSF_ELEMENT_MCF_REPORT          0x0b
#define T30_NSF_ELEMENT_PARTIAL_PAGE_REPORT 0x0c
#define T30_NSF_ELEMENT_PPR_REPORT          0x0d
#define T30_NSF_ELEMENT_TIMEOUT_REPORT      0x0e
#define T30_NSF_ELEMENT_RESULT_REPORT       0x0f


/*------------------------------------------------------------------*/
/* Analog modem definitions                                         */
/*------------------------------------------------------------------*/

typedef struct async_s ASYNC_FORMAT;
struct async_s {
  unsigned pe:    1;
  unsigned parity:2;
  unsigned spare: 2;
  unsigned stp:   1;
  unsigned ch_len:2;   // 3th octett in CAI
};


/*------------------------------------------------------------------*/
/* PLCI/NCCI states                                                 */
/*------------------------------------------------------------------*/

#define IDLE                    0
#define OUTG_CON_PENDING        1
#define INC_CON_PENDING         2
#define INC_CON_ALERT           3
#define INC_CON_ACCEPT          4
#define INC_ACT_PENDING         5
#define LISTENING               6
#define CONNECTED               7
#define OUTG_DIS_PENDING        8
#define INC_DIS_PENDING         9
#define LOCAL_CONNECT           10
#define INC_RES_PENDING         11
#define OUTG_RES_PENDING        12
#define SUSPENDING              13
#define ADVANCED_VOICE_SIG      14
#define ADVANCED_VOICE_NOSIG    15
#define RESUMING                16
#define INC_CON_CONNECTED_PEND  17
#define INC_CON_CONNECTED_ALERT 18
#define OUTG_REJ_PENDING        19
#define INC_CON_BLIND_RETRIEVE  20
#define LISTENING_NCRL          21


/*------------------------------------------------------------------*/
/* auxilliary states for supplementary services                     */
/*------------------------------------------------------------------*/

#define IDLE                0
#define HOLD_REQUEST        1
#define HOLD_INDICATE       2
#define CALL_HELD           3
#define RETRIEVE_REQUEST    4
#define RETRIEVE_INDICATION 5
#define BLIND_RETRIEVE_REQUEST    6

/*------------------------------------------------------------------*/
/* Capi IE + Msg types                                              */
/*------------------------------------------------------------------*/
#define ESC_CAUSE        0x800|CAU          /* Escape cause element */
#define ESC_MSGTYPE      0x800|MSGTYPEIE    /* Escape message type  */
#define ESC_CHI          0x800|CHI          /* Escape channel id    */
#define ESC_LAW          0x800|BC           /* Escape law info      */
#define ESC_CR           0x800|CRIE         /* Escape CallReference */
#define ESC_PROFILE      0x800|PROFILEIE    /* Escape profile       */
#define ESC_SSEXT        0x800|SSEXTIE      /* Escape Supplem. Serv.*/
#define ESC_VSWITCH      0x800|VSWITCHIE    /* Escape VSwitch       */
#define ESC_CAI          0x800|CAI          /* Escape CAI           */
#define ESC_SMSG         0x800|SMSG         /* Escape SegmentedMsg  */
#define ESC_OLINE        0x800|OLINE        /* Escape Originating Line ANI II */
#define CST              0x14               /* Call State i.e.      */
#define PI               0x1E               /* Progress Indicator   */
#define NI               0x27               /* Notification Ind     */
#define CONN_NR          0x4C               /* Connected Number     */
#define CONG_RNR         0xBF               /* Congestion RNR       */
#define CONG_RR          0xB0               /* Congestion RR        */
#define RESERVED         0xFF               /* Res. for future use  */
#define HOOK_SUPPORT     0x01               /* activate Hook signal */
#define SCR              0x7a               /* unscreened number    */

#define HOOK_OFF_REQ     0x9001             /* internal conn req    */
#define HOOK_ON_REQ      0x9002             /* internal disc req    */
#define SUSPEND_REQ      0x9003             /* internal susp req    */
#define RESUME_REQ       0x9004             /* internal resume req  */
#define USELAW_REQ       0x9005             /* internal law    req  */
#define LISTEN_SIG_ASSIGN_PEND  0x9006
#define PERM_LIST_REQ    0x900a             /* permanent conn DCE   */
#define C_HOLD_REQ       0x9011
#define C_RETRIEVE_REQ   0x9012
#define C_NCR_FAC_REQ    0x9013
#define PERM_COD_ASSIGN  0x9014
#define PERM_COD_CALL    0x9015
#define PERM_COD_HOOK    0x9016
#define PERM_COD_CONN_PEND 0x9017           /* wait for connect_con */
#define PTY_REQ_PEND     0x9018
#define CD_REQ_PEND      0x9019
#define CF_START_PEND    0x901a
#define CF_STOP_PEND     0x901b
#define ECT_REQ_PEND     0x901c
#define GETSERV_REQ_PEND 0x901d
#define BLOCK_PLCI       0x901e
#define INTERR_NUMBERS_REQ_PEND         0x901f
#define INTERR_DIVERSION_REQ_PEND       0x9020
#define MWI_ACTIVATE_REQ_PEND           0x9021
#define MWI_DEACTIVATE_REQ_PEND         0x9022
#define SSEXT_REQ_COMMAND               0x9023
#define SSEXT_NC_REQ_COMMAND            0x9024
#define START_L1_SIG_ASSIGN_PEND        0x9025
#define REM_L1_SIG_ASSIGN_PEND          0x9026
#define CONF_BEGIN_REQ_PEND             0x9027
#define CONF_ADD_REQ_PEND               0x9028
#define CONF_SPLIT_REQ_PEND             0x9029
#define CONF_DROP_REQ_PEND              0x902a
#define CONF_ISOLATE_REQ_PEND           0x902b
#define CONF_REATTACH_REQ_PEND          0x902c
#define VSWITCH_REQ_PEND                0x902d
#define GET_MWI_STATE                   0x902e
#define CCBS_REQUEST_REQ_PEND           0x902f
#define CCBS_DEACTIVATE_REQ_PEND        0x9030
#define CCBS_INTERROGATE_REQ_PEND       0x9031
#define CCBS_STATUS_REQ_PEND            0x9032
#define CCNR_REQUEST_REQ_PEND           0x9033
#define CCNR_INTERROGATE_REQ_PEND       0x9034
#define LISTEN_SIG_NCRL_ASSIGN_PEND     0x9035

#define NO_INTERNAL_COMMAND             0
#define DTMF_COMMAND_1                  1
#define DTMF_COMMAND_2                  2
#define DTMF_COMMAND_3                  3
#define MIXER_COMMAND_1                 4
#define MIXER_COMMAND_2                 5
#define MIXER_COMMAND_3                 6
#define MIXER_COMMAND_4                 7
#define MIXER_COMMAND_5                 8
#define MIXER_COMMAND_6                 9
#define MIXER_COMMAND_7                 10
#define MIXER_COMMAND_8                 11
#define MIXER_COMMAND_9                 12
#define MIXER_COMMAND_10                13
#define ADV_VOICE_COMMAND_CONNECT_1     14
#define ADV_VOICE_COMMAND_CONNECT_2     15
#define ADV_VOICE_COMMAND_CONNECT_3     16
#define ADV_VOICE_COMMAND_DISCONNECT_1  17
#define ADV_VOICE_COMMAND_DISCONNECT_2  18
#define ADV_VOICE_COMMAND_DISCONNECT_3  19
#define ADJUST_B_RESTORE_1              20
#define ADJUST_B_RESTORE_2              21
#define RESET_B3_COMMAND_1              22
#define SELECT_B_COMMAND_1              23
#define FAX_CONNECT_INFO_COMMAND_1      24
#define FAX_CONNECT_INFO_COMMAND_2      25
#define FAX_ADJUST_B23_COMMAND_1        26
#define FAX_ADJUST_B23_COMMAND_2        27
#define EC_COMMAND_1                    28
#define EC_COMMAND_2                    29
#define EC_COMMAND_3                    30
#define RTP_CONNECT_B3_REQ_COMMAND_1    31
#define RTP_CONNECT_B3_REQ_COMMAND_2    32
#define RTP_CONNECT_B3_REQ_COMMAND_3    33
#define RTP_CONNECT_B3_RES_COMMAND_1    34
#define RTP_CONNECT_B3_RES_COMMAND_2    35
#define RTP_CONNECT_B3_RES_COMMAND_3    36
#define HOLD_SAVE_COMMAND_1             37
#define RETRIEVE_RESTORE_COMMAND_1      38
#define FAX_DISCONNECT_COMMAND_1        39
#define FAX_DISCONNECT_COMMAND_2        40
#define FAX_DISCONNECT_COMMAND_3        41
#define FAX_EDATA_ACK_COMMAND_1         42
#define FAX_EDATA_ACK_COMMAND_2         43
#define FAX_CONNECT_ACK_COMMAND_1       44
#define FAX_CONNECT_ACK_COMMAND_2       45
#define MEASURE_COMMAND_1               46
#define MEASURE_COMMAND_2               47
#define MEASURE_COMMAND_3               48
#define CONTROL_MESSAGE_COMMAND_1       49
#define CONTROL_MESSAGE_COMMAND_2       50
#define PITCH_COMMAND_1                 51
#define PITCH_COMMAND_2                 52
#define PITCH_COMMAND_3                 53
#define FAX_DISCONNECT_INFO_COMMAND_1   54
#define FAX_DISCONNECT_INFO_COMMAND_2   55
#define AUDIO_COMMAND_1                 56
#define AUDIO_COMMAND_2                 57
#define SIG_TX_COMMAND_1                58
#define SIG_TX_COMMAND_2                59
#define SIG_TX_COMMAND_3                60
#define SIG_TX_COMMAND_4                61
#define SIG_TX_COMMAND_5                62
#define SIG_TX_COMMAND_6                63
#define SIG_TX_COMMAND_7                64
#define RESET_B3_N_RESET_COMMAND_1      65
#define RTP_COMMAND_1                   66
#define RTP_COMMAND_2                   67
#define T38_COMMAND_1                   68
#define T38_COMMAND_2                   69
#define STD_INTERNAL_COMMAND_COUNT      70

#define UID              0x2d               /* User Id for Mgmt      */

#define CALL_DIR_OUT             0x01       /* call direction of initial call */
#define CALL_DIR_IN              0x02
#define CALL_DIR_ORIGINATE       0x04       /* DTE/DCE direction according to */
#define CALL_DIR_ANSWER          0x08       /*   state of B-Channel Operation */
#define CALL_DIR_FORCE_OUTG_NL   0x10       /* for RESET_B3 reconnect, after DISC_B3... */
#define CALL_DIR_OUTG_NL         0x20       /* direction of NL establishment  */
#define CALL_DIR_MONITOR         0x80       /* B-Channel operation monitoring */

#define AWAITING_MANUF_CON 0x80             /* command spoofing flags */
#define SPOOFING_REQUIRED  0xff
#define AWAITING_SELECT_B  0xef

/*------------------------------------------------------------------*/
/* B_CTRL / DSP_CTRL                                                */
/*------------------------------------------------------------------*/

#define DSP_CTRL_OLD_SET_MIXER_COEFFICIENTS     0x01
#define DSP_CTRL_SET_BCHANNEL_PASSIVATION_BRI   0x02
#define DSP_CTRL_SET_DTMF_PARAMETERS            0x03

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

#define MANUFACTURER_FEATURE2_LISTENING           0x00000001L
#define MANUFACTURER_FEATURE2_SS_DIFFCONTPOSSIBLE 0x00000002L
#define MANUFACTURER_FEATURE2_GENERIC_TONE        0x00000004L
#define MANUFACTURER_FEATURE2_COLOR_FAX           0x00000008L
#define MANUFACTURER_FEATURE2_SS_ECT_DIFFCONTPOSSIBLE 0x00000010L
#define MANUFACTURER_FEATURE2_TRANSPARENT_N_RESET 0x00000020L
/*
  Following four bits are reserved for vendor specific extensions
  Vendor specific extension allows values from 0x01 to 0x0e
  */
#define MANUFACTURER_FEATURE2_VENDOR_MASK_FIRST_BIT 6 /* 0x00000040L */
#define MANUFACTURER_FEATURE2_VENDOR_BITS           0x0000000fL
#define MANUFACTURER_FEATURE2_VENDOR_MASK    (MANUFACTURER_FEATURE2_VENDOR_BITS << MANUFACTURER_FEATURE2_VENDOR_MASK_FIRST_BIT)
#define MANUFACTURER_FEATURE2_CAPTARIS_ADAPTER   (0x00000001L << MANUFACTURER_FEATURE2_VENDOR_MASK_FIRST_BIT)

#define MANUFACTURER_FEATURE2_DIALING_CHARS_ACCEPTED  0x00000400L
#define MANUFACTURER_FEATURE2_NULL_PLCI           0x00000800L
#define MANUFACTURER_FEATURE2_X25_REVERSE_RESTART 0x00001000L
#define MANUFACTURER_FEATURE2_SOFTIP              0x00002000L
#define MANUFACTURER_FEATURE2_CLEAR_CHANNEL       0x00004000L
#define MANUFACTURER_FEATURE2_LINE_SIDE_T38       0x00008000L
#define MANUFACTURER_FEATURE2_RTP_LINE            0x00010000L
#define MANUFACTURER_FEATURE2_NO_CLOCK_LINE       0x00020000L
#define MANUFACTURER_FEATURE2_TRANSCODING         0x00040000L
#define MANUFACTURER_FEATURE2_MODEM_V34_V90       0x00080000L
#define MANUFACTURER_FEATURE2_PLUGIN_MODULATION   0x00100000L
#define MANUFACTURER_FEATURE2_FAX_V34             0x00200000L

/*------------------------------------------------------------------*/
/* UDATA message transfer via MANUFACTURER_REQ                      */
/*------------------------------------------------------------------*/

#define UDATA_REQUEST_SIMPLIF_0        36
#define UDATA_INDICATION_SIMPLIF_0     36
#define UDATA_REQUEST_SIMPLIF_1        37
#define UDATA_INDICATION_SIMPLIF_1     37

/*------------------------------------------------------------------*/
/* DTMF interface to IDI                                            */
/*------------------------------------------------------------------*/


#define DTMF_DIGIT_TONE_LOW_GROUP_697_HZ        0x00
#define DTMF_DIGIT_TONE_LOW_GROUP_770_HZ        0x01
#define DTMF_DIGIT_TONE_LOW_GROUP_852_HZ        0x02
#define DTMF_DIGIT_TONE_LOW_GROUP_941_HZ        0x03
#define DTMF_DIGIT_TONE_LOW_GROUP_MASK          0x03
#define DTMF_DIGIT_TONE_HIGH_GROUP_1209_HZ      0x00
#define DTMF_DIGIT_TONE_HIGH_GROUP_1336_HZ      0x04
#define DTMF_DIGIT_TONE_HIGH_GROUP_1477_HZ      0x08
#define DTMF_DIGIT_TONE_HIGH_GROUP_1633_HZ      0x0c
#define DTMF_DIGIT_TONE_HIGH_GROUP_MASK         0x0c
#define DTMF_DIGIT_TONE_CODE_0                  0x07
#define DTMF_DIGIT_TONE_CODE_1                  0x00
#define DTMF_DIGIT_TONE_CODE_2                  0x04
#define DTMF_DIGIT_TONE_CODE_3                  0x08
#define DTMF_DIGIT_TONE_CODE_4                  0x01
#define DTMF_DIGIT_TONE_CODE_5                  0x05
#define DTMF_DIGIT_TONE_CODE_6                  0x09
#define DTMF_DIGIT_TONE_CODE_7                  0x02
#define DTMF_DIGIT_TONE_CODE_8                  0x06
#define DTMF_DIGIT_TONE_CODE_9                  0x0a
#define DTMF_DIGIT_TONE_CODE_STAR               0x03
#define DTMF_DIGIT_TONE_CODE_HASHMARK           0x0b
#define DTMF_DIGIT_TONE_CODE_A                  0x0c
#define DTMF_DIGIT_TONE_CODE_B                  0x0d
#define DTMF_DIGIT_TONE_CODE_C                  0x0e
#define DTMF_DIGIT_TONE_CODE_D                  0x0f

#define DTMF_DIAL_PULSE_DIGIT_CODE_1            0x40
#define DTMF_DIAL_PULSE_DIGIT_CODE_2            0x41
#define DTMF_DIAL_PULSE_DIGIT_CODE_3            0x42
#define DTMF_DIAL_PULSE_DIGIT_CODE_4            0x43
#define DTMF_DIAL_PULSE_DIGIT_CODE_5            0x44
#define DTMF_DIAL_PULSE_DIGIT_CODE_6            0x45
#define DTMF_DIAL_PULSE_DIGIT_CODE_7            0x46
#define DTMF_DIAL_PULSE_DIGIT_CODE_8            0x47
#define DTMF_DIAL_PULSE_DIGIT_CODE_9            0x48
#define DTMF_DIAL_PULSE_DIGIT_CODE_10           0x49
#define DTMF_DIAL_PULSE_DIGIT_CODE_11           0x4a
#define DTMF_DIAL_PULSE_DIGIT_CODE_12           0x4b
#define DTMF_DIAL_PULSE_DIGIT_CODE_13           0x4c
#define DTMF_DIAL_PULSE_DIGIT_CODE_14           0x4d
#define DTMF_DIAL_PULSE_DIGIT_CODE_15           0x4e
#define DTMF_DIAL_PULSE_DIGIT_CODE_16           0x4f

#define DTMF_DIGIT_TONE_CODE_NONE               0x7f

#define DTMF_UDATA_REQUEST_SEND_DIGITS            16
#define DTMF_UDATA_REQUEST_ENABLE_RECEIVER        17
#define DTMF_UDATA_REQUEST_DISABLE_RECEIVER       18
#define DTMF_UDATA_INDICATION_DIGITS_SENT         16
#define DTMF_UDATA_INDICATION_DIGITS_RECEIVED     17
#define DTMF_UDATA_INDICATION_MODEM_CALLING_TONE  18
#define DTMF_UDATA_INDICATION_FAX_CALLING_TONE    19
#define DTMF_UDATA_INDICATION_ANSWER_TONE         20

#define DSP_ENABLE_DTMF_DETECTION               0x0001
#define DSP_ENABLE_DTMF_EVENT_DETECTION         0x0002
#define DSP_ENABLE_MF_DETECTION                 0x0004
#define DSP_ENABLE_DIAL_PULSE_DETECTION         0x0008
#define DSP_ENABLE_EXTENDED_TONE_DETECTION      0x0010
#define DSP_ENABLE_DTMF_FRAME_FLUSHING          0x0020
#define DSP_ENABLE_HOOK_DETECTION               0x0040
#define DSP_ENABLE_DTMF_SUPPRESSION             0x0080
#define DSP_ENABLE_R2FWD_DETECTION              0x0100
#define DSP_ENABLE_R2BWD_DETECTION              0x0200

#define UDATA_REQUEST_MIXER_TAP_DATA        27
#define UDATA_INDICATION_MIXER_TAP_DATA     27

#define DTMF_LISTEN_ACTIVE_FLAG        0x0001
#define DTMF_SEND_FLAG                 0x0001
#define DTMF_EDGE_ACTIVE_FLAG          0x0002


/*------------------------------------------------------------------*/
/* Mixer interface to IDI                                           */
/*------------------------------------------------------------------*/


#define MIXER_BCHANNELS_BRI    2
#define MIXER_IC_CHANNELS_BRI  MIXER_BCHANNELS_BRI
#define MIXER_IC_CHANNEL_BASE  MIXER_BCHANNELS_BRI
#define MIXER_CHANNELS_BRI     (MIXER_BCHANNELS_BRI + MIXER_IC_CHANNELS_BRI)
#define MIXER_CHANNELS_PRI     32

#define MIXER_TOTAL_CHANNELS_BRI (2 * MIXER_CHANNELS_BRI)
#define MIXER_TOTAL_CHANNELS_PRI (2 * MIXER_CHANNELS_PRI)


typedef struct li_config_s LI_CONFIG;

struct xconnect_bus_address_s {
  dword low;
  dword high;
};

struct xconnect_transfer_address_s {
  dword card_id;
  struct xconnect_bus_address_s bus_address;
};

struct li_config_s {
  DIVA_CAPI_ADAPTER   *adapter;
  struct xconnect_transfer_address_s send_b;
  struct xconnect_transfer_address_s send_pc;
  byte   *flag_table;  /* dword aligned and sized */
  byte   *coef_table;  /* dword aligned and sized */
  byte source_info_b;
  byte source_info_pc;
  byte channel;
  byte curchnl;
  byte chflags;
};

extern LI_CONFIG   *li_config_table;
extern word li_total_channels;

#define LI_NOTIFY_UPDATE_NONE      0
#define LI_NOTIFY_UPDATE_QUEUED    1
#define LI_NOTIFY_UPDATE_ENQUEUE   2

#define LI_CHANNEL_INVOLVED        0x01
#define LI_CHANNEL_USED_SOURCE_B   0x02
#define LI_CHANNEL_TX_DATA         0x04
#define LI_CHANNEL_RX_DATA         0x08
#define LI_CHANNEL_CONFERENCE      0x10
#define LI_CHANNEL_USED_SOURCE_PC  0x20
#define LI_CHANNEL_NO_ADDRESSES    0x40
#define LI_CHANNEL_ADDRESSES_SET   0x80

#define LI_CHFLAG_MONITOR          0x01
#define LI_CHFLAG_MIX              0x02
#define LI_CHFLAG_LOOP             0x04
#define LI_CHFLAG_NULL_PLCI        0x08
#define LI_CHFLAG_NOT_SEND_X       0x10
#define LI_CHFLAG_NOT_RECEIVE_X    0x20
#define LI_CHFLAG_LINE_SIDE        0x80

#define LI_FLAG_INTERCONNECT       0x01
#define LI_FLAG_MONITOR            0x02
#define LI_FLAG_MIX                0x04
#define LI_FLAG_PCCONNECT          0x08
#define LI_FLAG_CONFERENCE         0x10
#define LI_FLAG_ANNOUNCEMENT       0x20
#define LI_FLAG_BCONNECT           0x40

#define LI_COEF_CH_CH              0x01
#define LI_COEF_CH_PC              0x02
#define LI_COEF_PC_CH              0x04
#define LI_COEF_PC_PC              0x08
#define LI_COEF_CH_CH_SET          0x10
#define LI_COEF_CH_PC_SET          0x20
#define LI_COEF_PC_CH_SET          0x40
#define LI_COEF_PC_PC_SET          0x80

#define LI_REQ_SILENT_UPDATE       0xffff

#define LI_PLCI_B_LAST_FLAG        ((dword) 0x80000000L)
#define LI_PLCI_B_DISC_FLAG        ((dword) 0x40000000L)
#define LI_PLCI_B_SKIP_FLAG        ((dword) 0x20000000L)
#define LI_PLCI_B_ERROR_FLAG       ((dword) 0x10000000L)
#define LI_PLCI_B_FLAG_MASK        ((dword) 0xf0000000L)

#define UDATA_REQUEST_SET_MIXER_COEFS_BRI       24
#define UDATA_REQUEST_SET_MIXER_COEFS_PRI_SYNC  25
#define UDATA_REQUEST_SET_MIXER_COEFS_PRI_ASYN  26
#define UDATA_INDICATION_MIXER_COEFS_SET        24

#define MIXER_FEATURE_ENABLE_TX_DATA        0x0001
#define MIXER_FEATURE_ENABLE_RX_DATA        0x0002
#define MIXER_FEATURE_UNUSED_SOURCE_B       0x0004
#define MIXER_FEATURE_UNUSED_SOURCE_PC      0x0008

#define MIXER_COEF_LINE_CHANNEL_MASK        0x1f
#define MIXER_COEF_LINE_FROM_PC_FLAG        0x20
#define MIXER_COEF_LINE_TO_PC_FLAG          0x40
#define MIXER_COEF_LINE_ROW_FLAG            0x80

#define UDATA_REQUEST_XCONNECT_FROM         28
#define UDATA_INDICATION_XCONNECT_FROM      28
#define UDATA_REQUEST_XCONNECT_TO           29
#define UDATA_INDICATION_XCONNECT_TO        29

#define XCONNECT_CHANNEL_PORT_B             0x0000
#define XCONNECT_CHANNEL_PORT_PC            0x8000
#define XCONNECT_CHANNEL_PORT_MASK          0x8000
#define XCONNECT_CHANNEL_NUMBER_MASK        0x7fff
#define XCONNECT_CHANNEL_PORT_COUNT         2

#define XCONNECT_SUCCESS           0x0000
#define XCONNECT_ERROR             0x0001


/*------------------------------------------------------------------*/
/* Echo canceller interface to IDI                                  */
/*------------------------------------------------------------------*/


#define PRIVATE_ECHO_CANCELLER         0

#define PRIV_SELECTOR_ECHO_CANCELLER   255

#define EC_ENABLE_OPERATION            1
#define EC_DISABLE_OPERATION           2
#define EC_FREEZE_COEFFICIENTS         3
#define EC_RESUME_COEFFICIENT_UPDATE   4
#define EC_RESET_COEFFICIENTS          5

#define EC_DISABLE_NON_LINEAR_PROCESSING     0x0001
#define EC_DO_NOT_REQUIRE_REVERSALS          0x0002
#define EC_DETECT_DISABLE_TONE               0x0004

#define EC_SUCCESS                           0
#define EC_UNSUPPORTED_OPERATION             1

#define EC_BYPASS_DUE_TO_CONTINUOUS_2100HZ   1
#define EC_BYPASS_DUE_TO_REVERSED_2100HZ     2
#define EC_BYPASS_RELEASED                   3

#define DSP_CTRL_SET_LEC_PARAMETERS          0x05

#define LEC_ENABLE_ECHO_CANCELLER            0x0001
#define LEC_ENABLE_2100HZ_DETECTOR           0x0002
#define LEC_REQUIRE_2100HZ_REVERSALS         0x0004
#define LEC_MANUAL_DISABLE                   0x0008
#define LEC_ENABLE_NONLINEAR_PROCESSING      0x0010
#define LEC_FREEZE_COEFFICIENTS              0x0020
#define LEC_RESET_COEFFICIENTS               0x8000

#define LEC_MAX_SUPPORTED_TAIL_LENGTH        32

#define LEC_UDATA_INDICATION_DISABLE_DETECT  9

#define LEC_DISABLE_TYPE_CONTIGNUOUS_2100HZ  0x00
#define LEC_DISABLE_TYPE_REVERSED_2100HZ     0x01
#define LEC_DISABLE_RELEASED                 0x02
#define LEC_DISABLE_TYPE_INTERNAL            0x03


/*------------------------------------------------------------------*/
/* No resource                                                      */
/*------------------------------------------------------------------*/

#define B1_NO_RESOURCE          0xffff
#define B2_NO_RESOURCE          0xffff
#define B3_NO_RESOURCE          0xffff

#define PRIV_BCHANNEL_INFO_NO_RESERVATION  0x0103


/*------------------------------------------------------------------*/
/* B-channel operation monitoring                                   */
/*------------------------------------------------------------------*/

#define PRIV_BCHANNEL_OPERATION_MONITOR  255


/*------------------------------------------------------------------*/
/* RTP interface to IDI                                             */
/*------------------------------------------------------------------*/


#define B1_RTP                  31
#define B2_RTP                  31
#define B3_RTP                  31

#define PRIVATE_RTP                    1

#define PRIV_SELECTOR_RTP              254

#define RTP_GET_SUPPORTED_PARAMETERS   1
#define RTP_GET_SUPPORTED_PAYLOAD_PROT 2
#define RTP_GET_SUPPORTED_PAYLOAD_OPT  3
#define RTP_QUERY_RTCP_REPORT          4
#define RTP_REQUEST_RTP_CONTROL        5

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
#define RTP_PRIM_PAYLOAD_G728_8000     15
#define RTP_PRIM_PAYLOAD_G729_8000     18
#define RTP_PRIM_PAYLOAD_RTAUDIO_8000  26
#define RTP_PRIM_PAYLOAD_ILBC_8000     27
#define RTP_PRIM_PAYLOAD_AMR_NB_8000   28
#define RTP_PRIM_PAYLOAD_T38           29
#define RTP_PRIM_PAYLOAD_GSM_HR_8000   30
#define RTP_PRIM_PAYLOAD_GSM_EFR_8000  31

#define RTP_ADD_PAYLOAD_BASE           32
#define RTP_ADD_PAYLOAD_RED            32
#define RTP_ADD_PAYLOAD_CN_8000        33
#define RTP_ADD_PAYLOAD_DTMF           34
#define RTP_ADD_PAYLOAD_FEC            35

#define RTP_SUCCESS                         0
#define RTP_ERR_SSRC_OR_PAYLOAD_CHANGE      1

#define UDATA_REQUEST_RTP_CONTROL           33
#define UDATA_INDICATION_RTP_STATE          33
#define UDATA_REQUEST_RTP_RECONFIGURE       64
#define UDATA_INDICATION_RTP_CHANGE         65
#define UDATA_REQUEST_QUERY_RTCP_REPORT     66
#define UDATA_INDICATION_RTCP_REPORT        66

#define RTP_CONNECT_OPTION_DISC_ON_SSRC_CHANGE    0x00000001L
#define RTP_CONNECT_OPTION_DISC_ON_PT_CHANGE      0x00000002L
#define RTP_CONNECT_OPTION_DISC_ON_UNKNOWN_PT     0x00000004L
#define RTP_CONNECT_OPTION_NO_SILENCE_TRANSMIT    0x00010000L

#define RTP_PAYLOAD_OPTION_VOICE_ACTIVITY_DETECT  0x0001
#define RTP_PAYLOAD_OPTION_DISABLE_POST_FILTER    0x0002
#define RTP_PAYLOAD_OPTION_G723_LOW_CODING_RATE   0x0100

#define RTP_PACKET_FILTER_IGNORE_UNKNOWN_SSRC     0x00000001L

#define RTP_CHANGE_FLAG_SSRC_CHANGE               0x00000001L
#define RTP_CHANGE_FLAG_PAYLOAD_TYPE_CHANGE       0x00000002L
#define RTP_CHANGE_FLAG_UNKNOWN_PAYLOAD_TYPE      0x00000004L

#define RTP_REASON_SSRC_OR_PAYLOAD_CHANGE  0x3600


/*------------------------------------------------------------------*/
/* T.38 interface to IDI                                            */
/*------------------------------------------------------------------*/


#define B2_T38                  30
#define B3_TRANSPARENT_IP       30

#define PRIVATE_T38                    2

#define PRIV_SELECTOR_T38              253

#define T38_GET_SUPPORTED_PARAMETERS   1
#define T38_GET_SUPPORTED_PAYLOAD_PROT 2
#define T38_GET_SUPPORTED_PAYLOAD_OPT  3
#define T38_QUERY_RTCP_REPORT          4

#define T38_T30_OPTION_SUPPRESS_FINE    0x0001
#define T38_T30_OPTION_SUPPRESS_POLLING 0x0002
#define T38_T30_OPTION_SUPPRESS_JPEG    0x0400
#define T38_T30_OPTION_SUPPRESS_T43     0x0800
#define T38_T30_OPTION_SUPPRESS_JBIG    0x1000
#define T38_T30_OPTION_SUPPRESS_MR      0x2000
#define T38_T30_OPTION_SUPPRESS_MMR     0x4000
#define T38_T30_OPTION_SUPPRESS_ECM     0x8000

#define T38_T30_OPTION2_SUPPRESS_IAF    0x0001  /* IAF negotiation */
#define T38_T30_OPTION2_SUPPRESS_PRI    0x0100  /* procedure interrupt */

#define T38_TRANSPORT_TCP_FLAG          0x0001
#define T38_TRANSPORT_TCP_TPKT_FLAG     0x0002
#define T38_TRANSPORT_UDP_UDPTL_FLAG    0x0004
#define T38_TRANSPORT_UDP_RTP_FLAG      0x0008

#define T38_VERSION_0_FLAG              0x0001
#define T38_VERSION_1_FLAG              0x0002
#define T38_VERSION_2_FLAG              0x0004
#define T38_VERSION_3_FLAG              0x0008

#define T38_OPTION_FILL_BIT_REMOVAL     0x0001
#define T38_OPTION_MMR_TRANSCODING      0x0002
#define T38_OPTION_JBIG_TRANSCODING     0x0004
#define T38_OPTION_LOCAL_TCF            0x4000
#define T38_OPTION_TRANSFERRED_TCF      0x8000

#define T38_ERROR_CORRECTION_REDUNDANCY 0x0001
#define T38_ERROR_CORRECTION_FEC        0x0002

#define T38_ENCAPSULATION_LEVEL_NONE_UDP  0x0000
#define T38_ENCAPSULATION_LEVEL_UDP       0x0001
#define T38_ENCAPSULATION_LEVEL_UDP_IP    0x0002
#define T38_ENCAPSULATION_LEVEL_NONE_TCP  0x0008
#define T38_ENCAPSULATION_LEVEL_TCP       0x0009
#define T38_ENCAPSULATION_LEVEL_TCP_IP    0x000a
#define T38_ENCAPSULATION_LEVEL_MASK      0x000f


/*------------------------------------------------------------------*/
/* Connect support, i.e. profile and support for each connect type  */
/*------------------------------------------------------------------*/

#define PRIV_SELECTOR_CONNECT_SUPPORT  252

#define CONNECT_SUPPORT_GET_TYPES            1
#define CONNECT_SUPPORT_GET_PROFILE          2
#define CONNECT_SUPPORT_GET_RTP_PAYLOAD_PROT 3
#define CONNECT_SUPPORT_GET_T38_PAYLOAD_PROT 4

#define CONNECT_SUPPORT_INTERNAL_CONTROLLER  0
#define CONNECT_SUPPORT_EXTERNAL_EQUIPMENT   1
#define CONNECT_SUPPORT_NULL_PLCI            2
#define CONNECT_SUPPORT_LINE_SIDE            3
#define CONNECT_SUPPORT_DATA_STUB            4
#define CONNECT_SUPPORT_LINE_STUB            5
#define CONNECT_SUPPORT_DATA_NO_CLOCK        6
#define CONNECT_SUPPORT_LINE_NO_CLOCK        7

/*------------------------------------------------------------------*/
/* Resource reservation                                             */
/*------------------------------------------------------------------*/

#define PRIV_SELECTOR_RESOURCE_RESERVATION   251

#define RESERVE_PROFILE                      2
#define RESERVE_RTP_PAYLOAD_PROT             3
#define RESERVE_T38_PAYLOAD_PROT             4

#define RESERVE_INFO_PROFILE_INDEX_MASK      0x7f
#define RESERVE_INFO_REQUEST_PENDING         0x80

#define INFO_RELATED_RESOURCE_ERROR     0x20fd
#define INFO_OUT_OF_B_RESOURCES         0x20fe
#define INFO_OUT_OF_B_LICENSES          0x20ff

#define REASON_RELATED_RESOURCE_ERROR   0x330d
#define REASON_OUT_OF_B_RESOURCES       0x330e
#define REASON_OUT_OF_B_LICENSES        0x330f

/*------------------------------------------------------------------*/
/* PIAFS interface to IDI                                            */
/*------------------------------------------------------------------*/


#define B1_PIAFS                29
#define B2_PIAFS                29

#define PRIVATE_PIAFS           29

/*
  B2 configuration for PIAFS:
+---------------------+------+-----------------------------------------+
| PIAFS Protocol      | byte | Bit 1 - Protocol Speed                  |
| Speed configuration |      |         0 - 32K                         |
|                     |      |         1 - 64K (default)               |
|                     |      | Bit 2 - Variable Protocol Speed         |
|                     |      |         0 - Speed is fix                |
|                     |      |         1 - Speed is variable (default) |
+---------------------+------+-----------------------------------------+
| Direction           | word | Enable compression/decompression for    |
|                     |      | 0: All direction                        |
|                     |      | 1: disable outgoing data                |
|                     |      | 2: disable incomming data               |
|                     |      | 3: disable both direction (default)     |
+---------------------+------+-----------------------------------------+
| Number of code      | word | Parameter P1 of V.42bis in accordance   |
| words               |      | with V.42bis                            |
+---------------------+------+-----------------------------------------+
| Maximum String      | word | Parameter P2 of V.42bis in accordance   |
| Length              |      | with V.42bis                            |
+---------------------+------+-----------------------------------------+
| control (UDATA)     | byte | enable PIAFS control communication      |
| abilities           |      |                                         |
+---------------------+------+-----------------------------------------+
*/
#define PIAFS_UDATA_ABILITIES  0x80

/*------------------------------------------------------------------*/
/* FAX SUB/SEP/PWD extension                                        */
/*------------------------------------------------------------------*/


#define PRIVATE_FAX_SUB_SEP_PWD        3



/*------------------------------------------------------------------*/
/* V.18 extension                                                   */
/*------------------------------------------------------------------*/


#define PRIVATE_V18                    4



/*------------------------------------------------------------------*/
/* DTMF TONE extension                                              */
/*------------------------------------------------------------------*/


#define PRIV_SELECTOR_DTMF_ONDATA  250

#define DTMF_LISTEN_R2BWD_START          0xeb
#define DTMF_LISTEN_R2BWD_STOP           0xec
#define DTMF_SEND_R2BWD                  0xed
#define DTMF_LISTEN_R2FWD_START          0xee
#define DTMF_LISTEN_R2FWD_STOP           0xef
#define DTMF_SEND_R2FWD                  0xf0
#define DTMF_LISTEN_HOOK_START           0xf1
#define DTMF_LISTEN_HOOK_STOP            0xf2
#define DTMF_SEND_HOOK                   0xf3
#define DTMF_SUPPRESSION_START           0xf4
#define DTMF_SUPPRESSION_STOP            0xf5
#define DTMF_LISTEN_PULSE_START          0xf6
#define DTMF_LISTEN_PULSE_STOP           0xf7
#define DTMF_GET_SUPPORTED_DETECT_CODES  0xf8
#define DTMF_GET_SUPPORTED_SEND_CODES    0xf9
#define DTMF_LISTEN_TONE_START           0xfa
#define DTMF_LISTEN_TONE_STOP            0xfb
#define DTMF_SEND_TONE                   0xfc
#define DTMF_LISTEN_MF_START             0xfd
#define DTMF_LISTEN_MF_STOP              0xfe
#define DTMF_SEND_MF                     0xff

#define DTMF_MF_DIGIT_TONE_CODE_1               0x10
#define DTMF_MF_DIGIT_TONE_CODE_2               0x11
#define DTMF_MF_DIGIT_TONE_CODE_3               0x12
#define DTMF_MF_DIGIT_TONE_CODE_4               0x13
#define DTMF_MF_DIGIT_TONE_CODE_5               0x14
#define DTMF_MF_DIGIT_TONE_CODE_6               0x15
#define DTMF_MF_DIGIT_TONE_CODE_7               0x16
#define DTMF_MF_DIGIT_TONE_CODE_8               0x17
#define DTMF_MF_DIGIT_TONE_CODE_9               0x18
#define DTMF_MF_DIGIT_TONE_CODE_0               0x19
#define DTMF_MF_DIGIT_TONE_CODE_K1              0x1a
#define DTMF_MF_DIGIT_TONE_CODE_K2              0x1b
#define DTMF_MF_DIGIT_TONE_CODE_KP              0x1c
#define DTMF_MF_DIGIT_TONE_CODE_S1              0x1d
#define DTMF_MF_DIGIT_TONE_CODE_ST              0x1e
#define DTMF_MF_DIGIT_TONE_CODE_NONE            0x1f

#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_1       0x20
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_2       0x21
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_3       0x22
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_4       0x23
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_5       0x24
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_6       0x25
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_7       0x26
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_8       0x27
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_9       0x28
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_10      0x29
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_11      0x2a
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_12      0x2b
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_13      0x2c
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_14      0x2d
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_15      0x2e
#define DTMF_R2_FORWARD_DIGIT_TONE_CODE_NONE    0x2f

#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_1      0x30
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_2      0x31
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_3      0x32
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_4      0x33
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_5      0x34
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_6      0x35
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_7      0x36
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_8      0x37
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_9      0x38
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_10     0x39
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_11     0x3a
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_12     0x3b
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_13     0x3c
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_14     0x3d
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_15     0x3e
#define DTMF_R2_BACKWARD_DIGIT_TONE_CODE_NONE   0x3f

#define DTMF_DIGIT_CODE_COUNT                   16
#define DTMF_MF_DIGIT_CODE_BASE                 DTMF_DIGIT_CODE_COUNT
#define DTMF_MF_DIGIT_CODE_COUNT                16
#define DTMF_R2_FORWARD_DIGIT_CODE_BASE         (DTMF_MF_DIGIT_CODE_BASE + DTMF_MF_DIGIT_CODE_COUNT)
#define DTMF_R2_FORWARD_DIGIT_CODE_COUNT        16
#define DTMF_R2_BACKWARD_DIGIT_CODE_BASE        (DTMF_R2_FORWARD_DIGIT_CODE_BASE + DTMF_R2_FORWARD_DIGIT_CODE_COUNT)
#define DTMF_R2_BACKWARD_DIGIT_CODE_COUNT       16
#define DTMF_DIAL_PULSE_DIGIT_CODE_BASE         (DTMF_R2_BACKWARD_DIGIT_CODE_BASE + DTMF_R2_BACKWARD_DIGIT_CODE_COUNT)
#define DTMF_DIAL_PULSE_DIGIT_CODE_COUNT        16
#define DTMF_TOTAL_DIGIT_CODE_COUNT             (DTMF_DIAL_PULSE_DIGIT_CODE_BASE + DTMF_DIAL_PULSE_DIGIT_CODE_COUNT)

#define DTMF_TONE_DIGIT_BASE                    0x80

#define DTMF_SIGNAL_NO_TONE                     (DTMF_TONE_DIGIT_BASE + 0)
#define DTMF_SIGNAL_UNIDENTIFIED_TONE           (DTMF_TONE_DIGIT_BASE + 1)

#define DTMF_SIGNAL_DIAL_TONE                   (DTMF_TONE_DIGIT_BASE + 2)
#define DTMF_SIGNAL_PABX_INTERNAL_DIAL_TONE     (DTMF_TONE_DIGIT_BASE + 3)
#define DTMF_SIGNAL_SPECIAL_DIAL_TONE           (DTMF_TONE_DIGIT_BASE + 4)   /* stutter dial tone */
#define DTMF_SIGNAL_SECOND_DIAL_TONE            (DTMF_TONE_DIGIT_BASE + 5)
#define DTMF_SIGNAL_RINGING_TONE                (DTMF_TONE_DIGIT_BASE + 6)
#define DTMF_SIGNAL_SPECIAL_RINGING_TONE        (DTMF_TONE_DIGIT_BASE + 7)
#define DTMF_SIGNAL_BUSY_TONE                   (DTMF_TONE_DIGIT_BASE + 8)
#define DTMF_SIGNAL_CONGESTION_TONE             (DTMF_TONE_DIGIT_BASE + 9)   /* reorder tone */
#define DTMF_SIGNAL_SPECIAL_INFORMATION_TONE    (DTMF_TONE_DIGIT_BASE + 10)
#define DTMF_SIGNAL_COMFORT_TONE                (DTMF_TONE_DIGIT_BASE + 11)
#define DTMF_SIGNAL_HOLD_TONE                   (DTMF_TONE_DIGIT_BASE + 12)
#define DTMF_SIGNAL_RECORD_TONE                 (DTMF_TONE_DIGIT_BASE + 13)
#define DTMF_SIGNAL_CALLER_WAITING_TONE         (DTMF_TONE_DIGIT_BASE + 14)
#define DTMF_SIGNAL_CALL_WAITING_TONE           (DTMF_TONE_DIGIT_BASE + 15)
#define DTMF_SIGNAL_PAY_TONE                    (DTMF_TONE_DIGIT_BASE + 16)
#define DTMF_SIGNAL_POSITIVE_INDICATION_TONE    (DTMF_TONE_DIGIT_BASE + 17)
#define DTMF_SIGNAL_NEGATIVE_INDICATION_TONE    (DTMF_TONE_DIGIT_BASE + 18)
#define DTMF_SIGNAL_WARNING_TONE                (DTMF_TONE_DIGIT_BASE + 19)
#define DTMF_SIGNAL_INTRUSION_TONE              (DTMF_TONE_DIGIT_BASE + 20)
#define DTMF_SIGNAL_CALLING_CARD_SERVICE_TONE   (DTMF_TONE_DIGIT_BASE + 21)
#define DTMF_SIGNAL_PAYPHONE_RECOGNITION_TONE   (DTMF_TONE_DIGIT_BASE + 22)
#define DTMF_SIGNAL_CPE_ALERTING_SIGNAL         (DTMF_TONE_DIGIT_BASE + 23)
#define DTMF_SIGNAL_OFF_HOOK_WARNING_TONE       (DTMF_TONE_DIGIT_BASE + 24)
#define DTMF_SIGNAL_SIT_RESERVED_0              (DTMF_TONE_DIGIT_BASE + 32)
#define DTMF_SIGNAL_SIT_RESERVED_1              (DTMF_TONE_DIGIT_BASE + 33)
#define DTMF_SIGNAL_SIT_RESERVED_2              (DTMF_TONE_DIGIT_BASE + 34)
#define DTMF_SIGNAL_SIT_RESERVED_3              (DTMF_TONE_DIGIT_BASE + 35)
#define DTMF_SIGNAL_SIT_OPERATOR_INTERCEPT      (DTMF_TONE_DIGIT_BASE + 36)
#define DTMF_SIGNAL_SIT_VACANT_CIRCUIT          (DTMF_TONE_DIGIT_BASE + 37)
#define DTMF_SIGNAL_SIT_REORDER                 (DTMF_TONE_DIGIT_BASE + 38)
#define DTMF_SIGNAL_SIT_NO_CIRCUIT_FOUND        (DTMF_TONE_DIGIT_BASE + 39)
#define DTMF_SIGNAL_SS5_F1                      (DTMF_TONE_DIGIT_BASE + 59)
#define DTMF_SIGNAL_SS5_F2                      (DTMF_TONE_DIGIT_BASE + 60)
#define DTMF_SIGNAL_SS5_F1_F2                   (DTMF_TONE_DIGIT_BASE + 61)
#define DTMF_SIGNAL_HOLD_MUSIC                  (DTMF_TONE_DIGIT_BASE + 62)
#define DTMF_SIGNAL_INTERCEPT_TONE              (DTMF_TONE_DIGIT_BASE + 63)

#define DTMF_SIGNAL_MODEM_CALLING_TONE          (DTMF_TONE_DIGIT_BASE + 64)
#define DTMF_SIGNAL_FAX_CALLING_TONE            (DTMF_TONE_DIGIT_BASE + 65)
#define DTMF_SIGNAL_ANSWER_TONE                 (DTMF_TONE_DIGIT_BASE + 66)
#define DTMF_SIGNAL_REVERSED_ANSWER_TONE        (DTMF_TONE_DIGIT_BASE + 67)
#define DTMF_SIGNAL_ANSAM_TONE                  (DTMF_TONE_DIGIT_BASE + 68)
#define DTMF_SIGNAL_REVERSED_ANSAM_TONE         (DTMF_TONE_DIGIT_BASE + 69)
#define DTMF_SIGNAL_BELL103_ANSWER_TONE         (DTMF_TONE_DIGIT_BASE + 70)
#define DTMF_SIGNAL_FAX_FLAGS                   (DTMF_TONE_DIGIT_BASE + 71)
#define DTMF_SIGNAL_G2_FAX_GROUP_ID             (DTMF_TONE_DIGIT_BASE + 72)
#define DTMF_SIGNAL_HUMAN_SPEECH                (DTMF_TONE_DIGIT_BASE + 73)
#define DTMF_SIGNAL_ANSWERING_MACHINE_390       (DTMF_TONE_DIGIT_BASE + 74)
#define DTMF_SIGNAL_TONE_ALERTING_SIGNAL        (DTMF_TONE_DIGIT_BASE + 75)

#define DTMF_SIGNAL_HOOK_DIGIT_1                (DTMF_TONE_DIGIT_BASE + 112)
#define DTMF_SIGNAL_HOOK_DIGIT_2                (DTMF_TONE_DIGIT_BASE + 113)
#define DTMF_SIGNAL_HOOK_DIGIT_3                (DTMF_TONE_DIGIT_BASE + 114)
#define DTMF_SIGNAL_HOOK_DIGIT_4                (DTMF_TONE_DIGIT_BASE + 115)
#define DTMF_SIGNAL_HOOK_DIGIT_5                (DTMF_TONE_DIGIT_BASE + 116)
#define DTMF_SIGNAL_HOOK_DIGIT_6                (DTMF_TONE_DIGIT_BASE + 117)
#define DTMF_SIGNAL_HOOK_DIGIT_7                (DTMF_TONE_DIGIT_BASE + 118)
#define DTMF_SIGNAL_HOOK_DIGIT_8                (DTMF_TONE_DIGIT_BASE + 119)
#define DTMF_SIGNAL_HOOK_DIGIT_9                (DTMF_TONE_DIGIT_BASE + 120)
#define DTMF_SIGNAL_HOOK_DIGIT_10               (DTMF_TONE_DIGIT_BASE + 121)
#define DTMF_SIGNAL_HOOK_RESERVED_11            (DTMF_TONE_DIGIT_BASE + 122)
#define DTMF_SIGNAL_HOOK_RESERVED_12            (DTMF_TONE_DIGIT_BASE + 123)
#define DTMF_SIGNAL_HOOK_FLASH                  (DTMF_TONE_DIGIT_BASE + 124)
#define DTMF_SIGNAL_HOOK_RING                   (DTMF_TONE_DIGIT_BASE + 125)
#define DTMF_SIGNAL_HOOK_STATE_OFF_HOOK         (DTMF_TONE_DIGIT_BASE + 126)
#define DTMF_SIGNAL_HOOK_STATE_ON_HOOK          (DTMF_TONE_DIGIT_BASE + 127)

#define DTMF_SUPPRESSION_ACTIVE_FLAG   0x0400
#define DTMF_PRIV_EDGE_ACTIVE_FLAG     0x0008
#define DTMF_PRIV_LISTEN_ACTIVE_FLAG   0x0010
#define DTMF_PRIV_SEND_FLAG            0x0010
#define DTMF_TONE_LISTEN_ACTIVE_FLAG   0x0020
#define DTMF_TONE_SEND_FLAG            0x0020
#define DTMF_PULSE_LISTEN_ACTIVE_FLAG  0x0040
#define DTMF_PULSE_SEND_FLAG           0x0040
#define DTMF_MF_LISTEN_ACTIVE_FLAG     0x0080
#define DTMF_MF_SEND_FLAG              0x0080
#define DTMF_HOOK_LISTEN_ACTIVE_FLAG   0x0004
#define DTMF_HOOK_SEND_FLAG            0x0004
#define DTMF_R2FWD_LISTEN_ACTIVE_FLAG  0x0100
#define DTMF_R2FWD_SEND_FLAG           0x0100
#define DTMF_R2BWD_LISTEN_ACTIVE_FLAG  0x0200
#define DTMF_R2BWD_SEND_FLAG           0x0200

#define PRIVATE_DTMF_TONE              5


/*------------------------------------------------------------------*/
/* FAX paper format extension                                       */
/*------------------------------------------------------------------*/


#define PRIVATE_FAX_PAPER_FORMATS      6



/*------------------------------------------------------------------*/
/* V.OWN extension                                                  */
/*------------------------------------------------------------------*/


#define PRIVATE_VOWN                   7



/*------------------------------------------------------------------*/
/* FAX non-standard facilities extension                            */
/*------------------------------------------------------------------*/


#define PRIVATE_FAX_NONSTANDARD        8



/*------------------------------------------------------------------*/
/* MADAPTER extension                                               */
/*------------------------------------------------------------------*/

#define PRIVATE_MADAPTER               9

/*------------------------------------------------------------------*/
/* Vendor specific extension, 4 bits, enumeration                   */
/*------------------------------------------------------------------*/

#define PRIVATE_VENDOR_BIT_0           10
#define PRIVATE_VENDOR_BIT_1           (PRIVATE_VENDOR_BIT_0 + 1)
#define PRIVATE_VENDOR_BIT_2           (PRIVATE_VENDOR_BIT_0 + 2)
#define PRIVATE_VENDOR_BIT_3           (PRIVATE_VENDOR_BIT_0 + 3)

/*------------------------------------------------------------------*/
/* X.25 reverse restart extension                                   */
/*------------------------------------------------------------------*/

#define PRIVATE_X25_REVERSE_RESTART    14


/*------------------------------------------------------------------*/
/* LISTENING extension                                              */
/*------------------------------------------------------------------*/


#define B2_LISTENING            27

#define PRIVATE_LISTENING       27



/*------------------------------------------------------------------*/
/* SS7 extension                                                    */
/*------------------------------------------------------------------*/


#define B1_SS7                  28
#define B2_SS7                  28

#define PRIVATE_SS7             28



/*------------------------------------------------------------------*/
/* Measure extension                                                */
/*------------------------------------------------------------------*/


#define MEASURE_GET_SUPPORTED_SERVICES 0
#define MEASURE_ENABLE_OPERATION       1
#define MEASURE_DISABLE_OPERATION      2

#define MEASURE_MAX_CONFIRM_SIZE_WORDS          0x10

#define MEASURE_INFO_INSTANCE_MASK              0x00ff
#define MEASURE_INFO_REASON_MASK                0xff00
#define MEASURE_INFO_SUCCESS                    0x0000
#define MEASURE_INFO_IGNORED_PARAMETERS_FLAG    0x0100
#define MEASURE_INFO_SHRINKED_PARAMETERS_FLAG   0x0200
#define MEASURE_INFO_FATAL_FLAG                 0x8000
#define MEASURE_INFO_FATAL_PARAMETER_ERROR      0xfc00
#define MEASURE_INFO_FATAL_RESOURCE_ERROR       0xfd00
#define MEASURE_INFO_FATAL_MISSING_PARAMETERS   0xfe00
#define MEASURE_INFO_FATAL_UNKNOWN_REQUEST      0xff00

#define MEASURE_REQUEST_TIMER_SUPPORT           0x00
#define MEASURE_INDICATION_TIMER_SUPPORT        0x00
#define MEASURE_REQUEST_TIMER_SETUP             0x01
#define MEASURE_INDICATION_TIMER_SETUP          0x01
#define MEASURE_INDICATION_TIMER_TIMEOUT        0x02
#define MEASURE_REQUEST_RXSELECT_SUPPORT        0x40
#define MEASURE_INDICATION_RXSELECT_SUPPORT     0x40
#define MEASURE_REQUEST_RXSELECT_SETUP          0x41
#define MEASURE_INDICATION_RXSELECT_SETUP       0x41
#define MEASURE_REQUEST_RXFILTER_SUPPORT        0x44
#define MEASURE_INDICATION_RXFILTER_SUPPORT     0x44
#define MEASURE_REQUEST_RXFILTER_SETUP          0x45
#define MEASURE_INDICATION_RXFILTER_SETUP       0x45
#define MEASURE_REQUEST_SPECTRUM_SUPPORT        0x50
#define MEASURE_INDICATION_SPECTRUM_SUPPORT     0x50
#define MEASURE_REQUEST_SPECTRUM_SETUP          0x51
#define MEASURE_INDICATION_SPECTRUM_SETUP       0x51
#define MEASURE_INDICATION_SPECTRUM_RESULT      0x52
#define MEASURE_REQUEST_CEPSTRUM_SUPPORT        0x54
#define MEASURE_INDICATION_CEPSTRUM_SUPPORT     0x54
#define MEASURE_REQUEST_CEPSTRUM_SETUP          0x55
#define MEASURE_INDICATION_CEPSTRUM_SETUP       0x55
#define MEASURE_INDICATION_CEPSTRUM_RESULT      0x56
#define MEASURE_REQUEST_ECHO_SUPPORT            0x58
#define MEASURE_INDICATION_ECHO_SUPPORT         0x58
#define MEASURE_REQUEST_ECHO_SETUP              0x59
#define MEASURE_INDICATION_ECHO_SETUP           0x59
#define MEASURE_INDICATION_ECHO_RESULT          0x5a
#define MEASURE_REQUEST_SNGLTONE_SUPPORT        0x60
#define MEASURE_INDICATION_SNGLTONE_SUPPORT     0x60
#define MEASURE_REQUEST_SNGLTONE_SETUP          0x61
#define MEASURE_INDICATION_SNGLTONE_SETUP       0x61
#define MEASURE_INDICATION_SNGLTONE_START       0x62
#define MEASURE_INDICATION_SNGLTONE_END         0x63
#define MEASURE_REQUEST_DUALTONE_SUPPORT        0x64
#define MEASURE_INDICATION_DUALTONE_SUPPORT     0x64
#define MEASURE_REQUEST_DUALTONE_SETUP          0x65
#define MEASURE_INDICATION_DUALTONE_SETUP       0x65
#define MEASURE_INDICATION_DUALTONE_START       0x66
#define MEASURE_INDICATION_DUALTONE_END         0x67
#define MEASURE_REQUEST_FSKDET_SUPPORT  0x70
#define MEASURE_INDICATION_FSKDET_SUPPORT 0x70
#define MEASURE_REQUEST_FSKDET_SETUP  0x71
#define MEASURE_INDICATION_FSKDET_SETUP  0x71
#define MEASURE_INDICATION_FSKDET_DATA  0x72
#define MEASURE_REQUEST_TXSELECT_SUPPORT        0x80
#define MEASURE_INDICATION_TXSELECT_SUPPORT     0x80
#define MEASURE_REQUEST_TXSELECT_SETUP          0x81
#define MEASURE_INDICATION_TXSELECT_SETUP       0x81
#define MEASURE_REQUEST_TXFILTER_SUPPORT        0x84
#define MEASURE_INDICATION_TXFILTER_SUPPORT     0x84
#define MEASURE_REQUEST_TXFILTER_SETUP          0x85
#define MEASURE_INDICATION_TXFILTER_SETUP       0x85
#define MEASURE_REQUEST_GENERATOR_SUPPORT       0x90
#define MEASURE_INDICATION_GENERATOR_SUPPORT    0x90
#define MEASURE_REQUEST_GENERATOR_SETUP         0x91
#define MEASURE_INDICATION_GENERATOR_SETUP      0x91
#define MEASURE_REQUEST_MODULATOR_SUPPORT       0x94
#define MEASURE_INDICATION_MODULATOR_SUPPORT    0x94
#define MEASURE_REQUEST_MODULATOR_SETUP         0x95
#define MEASURE_INDICATION_MODULATOR_SETUP      0x95
#define MEASURE_REQUEST_FUNCTION_SUPPORT        0x98
#define MEASURE_INDICATION_FUNCTION_SUPPORT     0x98
#define MEASURE_REQUEST_FUNCTION_SETUP          0x99
#define MEASURE_INDICATION_FUNCTION_SETUP       0x99
#define MEASURE_REQUEST_SINEGEN_SUPPORT         0xa0
#define MEASURE_INDICATION_SINEGEN_SUPPORT      0xa0
#define MEASURE_REQUEST_SINEGEN_SETUP           0xa1
#define MEASURE_INDICATION_SINEGEN_SETUP        0xa1
#define MEASURE_REQUEST_FUNCGEN_SUPPORT         0xa4
#define MEASURE_INDICATION_FUNCGEN_SUPPORT      0xa4
#define MEASURE_REQUEST_FUNCGEN_SETUP           0xa5
#define MEASURE_INDICATION_FUNCGEN_SETUP        0xa5
#define MEASURE_REQUEST_NOISEGEN_SUPPORT        0xa8
#define MEASURE_INDICATION_NOISEGEN_SUPPORT     0xa8
#define MEASURE_REQUEST_NOISEGEN_SETUP          0xa9
#define MEASURE_INDICATION_NOISEGEN_SETUP       0xa9
#define MEASURE_REQUEST_FSKGEN_SUPPORT  0xac
#define MEASURE_INDICATION_FSKGEN_SUPPORT 0xac
#define MEASURE_REQUEST_FSKGEN_SETUP  0xad
#define MEASURE_INDICATION_FSKGEN_SETUP  0xad
#define MEASURE_REQUEST_FSKGEN_DATA  0xae
#define MEASURE_INDICATION_FSKGEN_DATA  0xae

#define UDATA_REQUEST_MEASURE          UDATA_REQUEST_SIMPLIF_0
#define UDATA_INDICATION_MEASURE       UDATA_INDICATION_SIMPLIF_0
#define UDATA_REQUEST_GENERIC_TONE     UDATA_REQUEST_SIMPLIF_1
#define UDATA_INDICATION_GENERIC_TONE  UDATA_INDICATION_SIMPLIF_1



/*------------------------------------------------------------------*/
/* Pitch control                                                    */
/*------------------------------------------------------------------*/


#define DSP_CTRL_SET_PITCH_PARAMETERS        0x0a

#define PITCH_FLAG_ENABLE               0x0001

#define PITCH_MIN_CALL_RATE_8000        1250
#define PITCH_MAX_CALL_RATE_8000        51200



/*------------------------------------------------------------------*/
/* Audio control                                                    */
/*------------------------------------------------------------------*/


#define DSP_CTRL_SET_AUDIO_PARAMETERS        0x0b

#define AUDIO_FLAG_FORCE_PART68_LIMITER_ON   0x0001
#define AUDIO_FLAG_FORCE_PART68_LIMITER_OFF  0x0002
#define AUDIO_FLAG_ENABLE_REC_COMPRESSOR     0x0008




/*------------------------------------------------------------------*/
/* SIG transmit                                                     */
/*------------------------------------------------------------------*/
#define DSP_CTRL_SIG_TRANSMIT                   0x08
/*
FTY parameters:
  <byte> message length
  <byte> DSP_CTRL_SIG_TRANSMIT
  <byte> transmit signal type
  DSP_CTRL_SIG_TX_TONE:
    <byte> signal tone code (defines DSP_SIGNAL_...)
    <byte> signal protocol (defines DSP_SIGNAL_PROTOCOL_...)
  DSP_CTRL_SIG_TX_DIGITS:
    <byte> steady digit (defines DSP_DTMF_.../DSP_MF_..., 0x80 for none/off)
    <byte>...<byte> digits (defines DSP_DTMF_.../DSP_MF_...)
  DSP_CTRL_SIG_TX_SET_DETECTOR:
    <word> detector flags
  DSP_CTRL_SIG_TX_SET_GENERATOR:
    <word> generator flags
  DSP_CTRL_SIG_TX_TASK_CONTROL:
    <word> control flags
  DSP_CTRL_SIG_TX_SET_MODULATION:
    <word> options
    <word> rx norm low
    <word> rx norm high
  DSP_CTRL_SIG_TX_DATA:
    <byte> norm
    <byte>...<byte> data
  DSP_CTRL_SIG_TX_PEER_HANGUP:
*/

#define DSP_CTRL_SIG_TX_TONE                 0
#define DSP_CTRL_SIG_TX_DIGITS               1
#define DSP_CTRL_SIG_TX_SET_DETECTOR         2
#define DSP_CTRL_SIG_TX_SET_GENERATOR        3
#define DSP_CTRL_SIG_TX_TASK_CONTROL         4
#define DSP_CTRL_SIG_TX_SET_MODULATION       5
#define DSP_CTRL_SIG_TX_DATA                 6
#define DSP_CTRL_SIG_TX_PEER_HANGUP          7

#define DSP_CTRL_SIG_ENABLE_DTMF_DETECTION   0x0001
#define DSP_CTRL_SIG_ENABLE_MF_DETECTION     0x0004
#define DSP_CTRL_SIG_ENABLE_R2FWD_DETECTION  0x0008
#define DSP_CTRL_SIG_ENABLE_R2BWD_DETECTION  0x0010
#define DSP_CTRL_SIG_ENABLE_TONE_DETECTION   0x0020
#define DSP_CTRL_SIG_ENABLE_HOOK_DETECTION   0x0040
#define DSP_CTRL_SIG_ENABLE_EXTD_DETECTION   0x0080

#define DSP_CTRL_SIG_ENABLE_GENERATION       0x0001
#define DSP_CTRL_SIG_STOP_GENERATION_ON_DATA 0x0002
#define DSP_CTRL_SIG_FEED_BACK_GENERATOR     0x0004
#define DSP_CTRL_SIG_MUTE_TRANSMIT_DATA      0x0008

#define DSP_CTRL_SIG_INHIBIT_L1_ACTIVE       0x0001

#define DSP_CTRL_SIG_MODULATION_ENABLE       0x0001
#define DSP_CTRL_SIG_CARRIER_ENABLE          0x0002

#define DSP_CTRL_SIG_NORM_LOW_NONE           0x0000
#define DSP_CTRL_SIG_NORM_LOW_V23_OFF_HOOK   0x0002
#define DSP_CTRL_SIG_NORM_LOW_V23_ON_HOOK    0x0004
#define DSP_CTRL_SIG_NORM_LOW_BELL202_CID    0x0008

#define DSP_CTRL_SIG_DATA_V23_OFF_HOOK       1
#define DSP_CTRL_SIG_DATA_V23_ON_HOOK        2
#define DSP_CTRL_SIG_DATA_BELL202_CID        3

/*------------------------------------------------------------------*/
/* Stream control                                                   */
/*------------------------------------------------------------------*/
#define DSP_CTRL_STREAM_COMMAND                 0x0e

/*------------------------------------------------------------------*/
/* Advanced voice                                                   */
/*------------------------------------------------------------------*/

#define ADV_VOICE_WRITE_ACTIVATION    0
#define ADV_VOICE_WRITE_DEACTIVATION  1
#define ADV_VOICE_WRITE_UPDATE        2

#define ADV_VOICE_OLD_COEF_COUNT    6
#define ADV_VOICE_NEW_COEF_BASE     (ADV_VOICE_OLD_COEF_COUNT * sizeof(word))

/*------------------------------------------------------------------*/
/* B1 resource switching                                            */
/*------------------------------------------------------------------*/

#define B1_FACILITY_LOCAL           0x0001
#define B1_FACILITY_MIXER           0x0002
#define B1_FACILITY_DTMFX           0x0004
#define B1_FACILITY_DTMFR           0x0008
#define B1_FACILITY_VOICE           0x0010
#define B1_FACILITY_EC              0x0020
#define B1_FACILITY_DTMFTONE        0x0040
#define B1_FACILITY_GENTONE         0x0080
#define B1_FACILITY_MEASURE         0x0100
#define B1_FACILITY_PITCH           0x0200
#define B1_FACILITY_LINEAR16        0x0400

#define ADJUST_B_MODE_SAVE          0x0001
#define ADJUST_B_MODE_REMOVE_L23    0x0002
#define ADJUST_B_MODE_SWITCH_L1     0x0004
#define ADJUST_B_MODE_NO_RESOURCE   0x0008
#define ADJUST_B_MODE_ASSIGN_L23    0x0010
#define ADJUST_B_MODE_USER_CONNECT  0x0020
#define ADJUST_B_MODE_CONNECT       0x0040
#define ADJUST_B_MODE_RESTORE       0x0080
#define ADJUST_B_MODE_LI_INVOLVED   0x0100

#define ADJUST_B_START                     0
#define ADJUST_B_SAVE_MEASURE_1            1
#define ADJUST_B_SAVE_MIXER_1              2
#define ADJUST_B_SAVE_DTMF_1               3
#define ADJUST_B_REMOVE_L23_1              4
#define ADJUST_B_REMOVE_L23_2              5
#define ADJUST_B_SAVE_EC_1                 6
#define ADJUST_B_SAVE_DTMF_PARAMETER_1     7
#define ADJUST_B_SAVE_SIG_TX_1             8
#define ADJUST_B_SAVE_AUDIO_PARAMETER_1    9
#define ADJUST_B_SAVE_PITCH_PARAMETER_1    10
#define ADJUST_B_SAVE_VOICE_1              11
#define ADJUST_B_SWITCH_L1_1               12
#define ADJUST_B_SWITCH_L1_2               13
#define ADJUST_B_UNSWITCH_L1_1             14
#define ADJUST_B_UNSWITCH_L1_2             15
#define ADJUST_B_RESTORE_VOICE_1           16
#define ADJUST_B_RESTORE_VOICE_2           17
#define ADJUST_B_RESTORE_PITCH_PARAMETER_1 18
#define ADJUST_B_RESTORE_PITCH_PARAMETER_2 19
#define ADJUST_B_RESTORE_AUDIO_PARAMETER_1 20
#define ADJUST_B_RESTORE_AUDIO_PARAMETER_2 21
#define ADJUST_B_RESTORE_SIG_TX_1          22
#define ADJUST_B_RESTORE_SIG_TX_2          23
#define ADJUST_B_RESTORE_SIG_TX_3          24
#define ADJUST_B_RESTORE_SIG_TX_4          25
#define ADJUST_B_RESTORE_SIG_TX_5          26
#define ADJUST_B_RESTORE_SIG_TX_6          27
#define ADJUST_B_RESTORE_SIG_TX_7          28
#define ADJUST_B_RESTORE_DTMF_PARAMETER_1  29
#define ADJUST_B_RESTORE_DTMF_PARAMETER_2  30
#define ADJUST_B_RESTORE_EC_1              31
#define ADJUST_B_RESTORE_EC_2              32
#define ADJUST_B_ASSIGN_L23_1              33
#define ADJUST_B_ASSIGN_L23_2              34
#define ADJUST_B_CONNECT_1                 35
#define ADJUST_B_CONNECT_2                 36
#define ADJUST_B_CONNECT_3                 37
#define ADJUST_B_CONNECT_4                 38
#define ADJUST_B_CONNECT_5                 39
#define ADJUST_B_RESTORE_DTMF_1            40
#define ADJUST_B_RESTORE_DTMF_2            41
#define ADJUST_B_RESTORE_MIXER_1           42
#define ADJUST_B_RESTORE_MIXER_2           43
#define ADJUST_B_RESTORE_MIXER_3           44
#define ADJUST_B_RESTORE_MIXER_4           45
#define ADJUST_B_RESTORE_MIXER_5           46
#define ADJUST_B_RESTORE_MIXER_6           47
#define ADJUST_B_RESTORE_MIXER_7           48
#define ADJUST_B_RESTORE_MEASURE_1         49
#define ADJUST_B_RESTORE_MEASURE_2         50
#define ADJUST_B_END                       51

/*------------------------------------------------------------------*/
/* XON Protocol def's                                               */
/*------------------------------------------------------------------*/
#define N_CH_XOFF               0x01
#define N_XON_SENT              0x02
#define N_XON_REQ               0x04
#define N_XON_CONNECT_IND       0x08
#define N_RX_FLOW_CONTROL_MASK  0x3f
#define N_OK_FC_PENDING         0x80
#define N_TX_FLOW_CONTROL_MASK  0xc0

/*------------------------------------------------------------------*/
/* NCPI state                                                       */
/*------------------------------------------------------------------*/
#define NCPI_VALID_CONNECT_B3_IND  0x01
#define NCPI_VALID_CONNECT_B3_ACT  0x02
#define NCPI_VALID_DISC_B3_IND     0x04
#define NCPI_CONNECT_B3_ACT_SENT   0x08
#define NCPI_NEGOTIATE_B3_SENT     0x10
#define NCPI_NEGOTIATE_B3_EXPECTED 0x20
#define NCPI_MDM_CTS_ON_RECEIVED   0x40
#define NCPI_MDM_DCD_ON_RECEIVED   0x80


typedef struct _diva_mgnt_adapter_info {
 dword cardtype;
 dword controller;
 dword total_controllers;
 dword serial_number;
 dword count ;
 dword logical_adapter_number; /* Logical adapter number of XDI adapter */
 dword vscapi_mode;
 dword mtpx_cfg_number_of_configuration_entries;
 const struct _diva_mtpx_adapter_xdi_config_entry* mtpx_cfg;
} diva_mgnt_adapter_info_t;
int diva_capi_read_channel_count (DESCRIPTOR* d, diva_mgnt_adapter_info_t* info);

/*------------------------------------------------------------------*/
