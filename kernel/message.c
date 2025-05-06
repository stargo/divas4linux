
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

#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"

#include "dlist.h"

#include "divacapi.h"
#include "mdm_msg.h"
#include "divasync.h"

#include "cardtype.h"



#define FILE_ "MESSAGE.C"
#define dprintf










#define trcq(plci)       trcq_ (plci, __LINE__)
#define trcq_(plci,line) trcq__ (plci, line)

void trcq__ (PLCI   *plci, word line)
{
  if (plci)
  {
    plci->trace_queue[plci->trace_pos] = line;
    if (++(plci->trace_pos) == TRACE_QUEUE_ENTRIES)
      plci->trace_pos = 0;
  }
}

#define get_plci(a)        get_plci__ (a, __LINE__)
#define get_plci__(a,line) get_plci___ (a, line)

static word get_plci_ (DIVA_CAPI_ADAPTER   *a);

static word get_plci___ (DIVA_CAPI_ADAPTER   *a, word line)
{
  word i;

  i = get_plci_ (a);
  if (i != 0)
    trcq__ (&a->plci[i-1], line);
  return (i);
}

static void free_plci (PLCI   *plci);
void plci_remove_ (PLCI   *plci);

void plci_remove (PLCI   *plci)
{
  trcq (plci);
  plci_remove_ (plci);
}

#define plci_remove(plci)        plci_remove__ (plci, __LINE__)
#define plci_remove__(plci,line) plci_remove___ (plci, line)

void plci_remove___ (PLCI   *plci, word line)
{
  trcq__ (plci, line);
  plci_remove_ (plci);
}

#define sig_req(plci,req,Id)        sig_req__ (plci, req, Id, __LINE__)
#define sig_req__(plci,req,Id,line) sig_req___ (plci, req, Id, line)

static void sig_req_ (PLCI   * plci, byte req, byte Id);

static void sig_req___ (PLCI   *plci, byte req, byte Id, word line)
{
  trcq__ (plci, line);
  sig_req_ (plci, req, Id);
}

#define nl_req_ncci(plci,req,ncci)        nl_req_ncci__ (plci, req, ncci, __LINE__)
#define nl_req_ncci__(plci,req,ncci,line) nl_req_ncci___ (plci, req, ncci, line)

static void nl_req_ncci_ (PLCI   * plci, byte req, word ncci);

static void nl_req_ncci___ (PLCI   *plci, byte req, word ncci, word line)
{
  trcq__ (plci, line);
  nl_req_ncci_ (plci, req, ncci);
}




typedef struct _diva_man_var_header {
  byte   escape;
  byte   length;
  byte   management_id;
  byte   type;
  byte   attribute;
  byte   status;
  byte   value_length;
  byte   path_length;
} diva_man_var_header_t;

typedef struct _diva_capi_mngt_work_item {
  char*  name;
  void*  dst;
  int    size;
} diva_capi_mngt_work_item_t;

typedef struct _diva_capi_mngt_work_entry {
  char* directory_name;
  diva_capi_mngt_work_item_t* work_items;
} diva_capi_mngt_work_entry_t;

typedef struct _diva_capi_mngt_ctxt {
  ENTITY  e;
  volatile int     state;
  BUFFERS XData;
  BUFFERS RData;
  byte    xbuffer[2048+1];
  DESCRIPTOR* d;

  int current_entry;
  diva_capi_mngt_work_entry_t** work_entries;
  int pending_msg;
} diva_capi_mngt_ctxt_t;




/*------------------------------------------------------------------*/
/* This is options supported for all adapters that are server by    */
/* XDI driver. Allo it is not necessary to ask it from every adapter*/
/* and it is not necessary to save it separate for every adapter    */
/* Macrose defined here have only local meaning                     */
/*------------------------------------------------------------------*/
dword diva_xdi_extended_features = 0;

#define DIVA_CAPI_USE_CMA                 0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVA_CAPI_XDI_PROVIDES_RX_DMA     0x00000008

/*
  CAPI can request to process all return codes self only if:
  protocol code supports this && xdi supports this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCEL(__a__)   (((__a__)->manufacturer_features&MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)&&    ((__a__)->manufacturer_features & MANUFACTURER_FEATURE_OK_FC_LABEL) &&     (diva_xdi_extended_features   & DIVA_CAPI_XDI_PROVIDES_NO_CANCEL))

/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/

void group_optimization(PLCI   *plci);
void AutomaticLaw(DIVA_CAPI_ADAPTER   *);
word CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(byte   *, word, byte *, API_PARSE *);
static void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE   *in, API_PARSE   *out);
static void api_swap_msg(API_SAVE   *in1, API_SAVE   *in2);

word api_remove_start(void);
void api_remove_complete(void);

void plci_remove_(PLCI   *);
static void diva_get_extended_adapter_features (DIVA_CAPI_ADAPTER  * a);
static void diva_get_mtpx_adapter_interface_version (DIVA_CAPI_ADAPTER* a);
static void diva_ask_for_xdi_sdram_bar (DIVA_CAPI_ADAPTER  *, IDI_SYNC_REQ  *);

void   callback(ENTITY   *);

static void control_rc(PLCI   *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte, void   *);
static void data_ack(PLCI   *, byte);
static void sig_ind(PLCI   *);
static void SendInfo(PLCI   *, dword, byte   * *, byte, byte);
static void SendSetupInfo(APPL   *, PLCI   *, dword, byte   * *, byte);
byte SendSSExtInd(APPL   *, PLCI   * plci, dword Id, byte   * * parms);

void VSwitchReqInd(PLCI   *plci, dword Id, byte   **parms);

static void nl_ind(PLCI   *);

static byte connect_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_a_res(dword,word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte listen_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte info_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte info_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte alert_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte facility_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte facility_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_a_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte data_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte data_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte reset_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte reset_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_t90_a_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte select_b_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte manufacturer_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte manufacturer_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);

static word get_plci_(DIVA_CAPI_ADAPTER   *);
static void add_p(PLCI   *, byte, byte   *);
static void add_s(PLCI   * plci, byte code, API_PARSE * p);
static void add_ss(PLCI   * plci, byte code, API_PARSE * p);
static void add_ie(PLCI   * plci, byte code, byte   * p, word p_length);
static void add_d(PLCI   *, word, byte   *);
static void add_ai(PLCI   *, API_PARSE *);
static word add_b1(PLCI   *, API_PARSE *, word, word);
static word add_b23(PLCI   *, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_parms);
static void sig_req_(PLCI   *, byte, byte);
static void nl_req_ncci_(PLCI   *, byte, word);
static void send_req(PLCI   *);
static void send_data(PLCI   *);
static word plci_remove_check(PLCI   *);
static void listen_check(DIVA_CAPI_ADAPTER   *);
static byte AddInfo(byte   **, byte   **, byte   *, byte *);
static byte getChannel(API_PARSE *);
static void IndParse(PLCI   *, word *, byte   **, byte);
static byte ie_compare(byte   *, byte *);
static word find_cip(DIVA_CAPI_ADAPTER   *, byte   *, byte   *);
static word CPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);
static void reject_call(dword, PLCI   *, APPL   *, word , API_PARSE *);
/*
  XON protocol helpers
  */
static void channel_flow_control_plci (PLCI   * plci, byte ch);
static void channel_flow_control_remove (PLCI   * plci);
static void channel_x_off (PLCI   * plci, byte ch, byte flag);
static void channel_x_on (PLCI   * plci, byte ch);
static void channel_request_xon (PLCI   * plci, byte ch);
static void channel_xmit_xon (PLCI   * plci);
static int channel_can_xon (PLCI   * plci, byte ch);
static void channel_xmit_extended_xon (PLCI   * plci);

static byte SendMultiIE(PLCI   * plci, dword Id, byte   * * parms, word ie_type, dword info_mask, APPL   * setupAppl, byte redirectedind, byte shiftcodeset);
static word AdvCodecSupport(DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, byte);
static void CodecIdCheck(DIVA_CAPI_ADAPTER   *, PLCI   *);
static void SetVoiceChannel(PLCI   *, byte   *, DIVA_CAPI_ADAPTER   * );
static void VoiceChannelOff(PLCI   *plci);
static void adv_voice_write_coefs (PLCI   *plci, word write_command);
static void adv_voice_clear_config (PLCI   *plci);

static word get_b1_facilities (PLCI   * plci, word b1_resource);
static word add_b1_facilities (PLCI   * plci, word b1_resource, word b1_facilities);
static void adjust_b1_facilities (PLCI   *plci, word new_b1_resource, word new_b1_facilities);
static word adjust_b_process (dword Id, PLCI   *plci, byte Rc);
static void adjust_b1_resource (dword Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_facilities, word internal_command);
static void adjust_b_restore (dword Id, PLCI   *plci, byte Rc);
static void reset_b3_command (dword Id, PLCI   *plci, byte Rc);
static void reset_b3_n_reset_command (dword Id, PLCI   *plci, byte Rc);
static void select_b_protocol_command (dword Id, PLCI   *plci, byte Rc);
static void connect_res_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_edata_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_info_command (dword Id, PLCI   *plci, byte Rc);
static void fax_disconnect_info_command (dword Id, PLCI   *plci, byte Rc);
static void fax_adjust_b23_command (dword Id, PLCI   *plci, byte Rc);
static void fax_disconnect_command (dword Id, PLCI   *plci, byte Rc);
static void hold_save_command (dword Id, PLCI   *plci, byte Rc);
static void retrieve_restore_command (dword Id, PLCI   *plci, byte Rc);
static void init_b1_config (PLCI   *plci);
static void clear_b1_config (PLCI   *plci);
static byte control_message_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void control_message_indication (dword Id, PLCI   *plci, byte   *msg, word length);

static void dtmf_command (dword Id, PLCI   *plci, byte Rc);
static byte dtmf_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void dtmf_confirmation (dword Id, PLCI   *plci);
static void dtmf_indication (dword Id, PLCI   *plci, byte   *msg, word length);
static void dtmf_parameter_write (PLCI   *plci);


static void dtmf_ondata_indication (dword Id, PLCI   *plci, byte   *msg, word length);


static void mixer_set_bchannel_id_unchecked (PLCI   *plci, byte bchannel_id);
static void mixer_set_bchannel_id_esc (PLCI   *plci, byte bchannel_id);
static void mixer_set_bchannel_id (PLCI   *plci, byte   *chi);
static void mixer_clear_config (PLCI   *plci);
static void mixer_notify_update (PLCI   *plci, byte others);
static void mixer_update_done (void);
static void mixer_command (dword Id, PLCI   *plci, byte Rc);
static byte mixer_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void mixer_indication_coefs_set (dword Id, PLCI   *plci);
static void mixer_indication_xconnect_from (dword Id, PLCI   *plci, byte   *msg, word length);
static void mixer_indication_xconnect_to (dword Id, PLCI   *plci, byte   *msg, word length);
static void mixer_remove (PLCI   *plci);


static void ec_command (dword Id, PLCI   *plci, byte Rc);
static byte ec_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication (dword Id, PLCI   *plci, byte   *msg, word length);


static void pitch_command (dword Id, PLCI   *plci, byte Rc);
static byte pitch_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);


static void audio_command (dword Id, PLCI   *plci, byte Rc);
static byte audio_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);

static void sig_tx_command (dword Id, PLCI   *plci, byte Rc);
static byte sig_tx_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);

static void rtp_command (dword Id, PLCI   *plci, byte Rc);
static byte rtp_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void rtp_confirmation (dword Id, PLCI   *plci, byte   *msg, word length);
static void rtp_indication (dword Id, PLCI   *plci, byte   *msg, word length);
static void rtp_connect_b3_req_command (dword Id, PLCI   *plci, byte Rc);
static void rtp_connect_b3_res_command (dword Id, PLCI   *plci, byte Rc);


static void t38_command (dword Id, PLCI   *plci, byte Rc);
static byte t38_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void t38_confirmation (dword Id, PLCI   *plci, byte   *msg, word length);


static void measure_command (dword Id, PLCI   *plci, byte Rc);
static byte measure_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void measure_indication (dword Id, PLCI   *plci, byte   *msg, word length);

static byte connect_support_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static byte resource_reservation_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void resource_reservation_indication (PLCI   * plci, byte   *esc_profile);
static void reserve_clear_incoming_call (PLCI   *plci);
static void reserve_assign_incoming_call (PLCI   *plci, APPL   *appl);
static void reserve_ignore_incoming_call (PLCI   *plci, APPL   *appl);

static int  diva_get_dma_descriptor  (PLCI   *plci, dword   *dma_magic);
static void diva_free_dma_descriptor (PLCI   *plci, int nr);

static int check_adapter_appl_cip_mask_empty (const DIVA_CAPI_ADAPTER* a);


static void diva_capi_init_stream_data (PLCI* plci, diva_saved_capi_appl_action_hdr_t* acion_hdr);
static void diva_capi_apply_saved_action (APPL* appl, PLCI* plci, word messageNr);
dword diva_capi_add_streaming_application_id (byte* dst, dword max_length, dword Id);


static void diva_capi_check_l1_activation (word id, DIVA_CAPI_ADAPTER   *a);
static void diva_capi_check_l1_shutdown (word id, DIVA_CAPI_ADAPTER   *a);

/*------------------------------------------------------------------*/
/* external function prototypes                                     */
/*------------------------------------------------------------------*/

extern byte MapController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) | MapController ((byte)(Id)))
#define UnMapId(Id) (((Id) & 0xffffff00L) | UnMapController ((byte)(Id)))

void   sendf(APPL   *, word, dword, word, byte *, ...);
void   * TransmitBufferSet(APPL   * appl, dword ref);
void   * TransmitBufferGet(APPL   * appl, void   * p);
void TransmitBufferFree(APPL   * appl, void   * p);
void   * ReceiveBufferGet(APPL   * appl, int Num);

int fax_head_line_time (char *buffer, byte us_style);


/*------------------------------------------------------------------*/
/* Global data definitions                                          */
/*------------------------------------------------------------------*/
extern byte max_adapter;
extern byte max_appl;
extern DIVA_CAPI_ADAPTER   * adapter;
extern APPL   * application;
extern struct resource_index_entry   *c_ind_resource_index;








#define NUMBER_OF_CONTROLLER max_adapter


PLCI   * li_update_wait_list = NULL;
byte remove_started = FALSE;
PLCI dummy_plci;


struct _ftable {
  word command;
  byte * format;
  byte (* function)(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
} ftable[] = {
  {_DATA_B3_R,                          "dwww",         data_b3_req},
  {_DATA_B3_I|RESPONSE,                 "w",            data_b3_res},
  {_INFO_R,                             "ss",           info_req},
  {_INFO_I|RESPONSE,                    "",             info_res},
  {_CONNECT_R,                          "wsssssssssss", connect_req},
  {_CONNECT_R,                          "wsssssssss",   connect_req},
  {_CONNECT_I|RESPONSE,                 "wsssss",       connect_res},
  {_CONNECT_ACTIVE_I|RESPONSE,          "",             connect_a_res},
  {_DISCONNECT_R,                       "s",            disconnect_req},
  {_DISCONNECT_R,                       "",             disconnect_req},
  {_DISCONNECT_I|RESPONSE,              "",             disconnect_res},
  {_LISTEN_R,                           "dddss",        listen_req},
  {_ALERT_R,                            "s",            alert_req},
  {_FACILITY_R,                         "ws",           facility_req},
  {_FACILITY_I|RESPONSE,                "ws",           facility_res},
  {_CONNECT_B3_R,                       "s",            connect_b3_req},
  {_CONNECT_B3_I|RESPONSE,              "ws",           connect_b3_res},
  {_CONNECT_B3_ACTIVE_I|RESPONSE,       "",             connect_b3_a_res},
  {_DISCONNECT_B3_R,                    "s",            disconnect_b3_req},
  {_DISCONNECT_B3_I|RESPONSE,           "",             disconnect_b3_res},
  {_RESET_B3_R,                         "s",            reset_b3_req},
  {_RESET_B3_I|RESPONSE,                "",             reset_b3_res},
  {_CONNECT_B3_T90_ACTIVE_I|RESPONSE,   "ws",           connect_b3_t90_a_res},
  {_CONNECT_B3_T90_ACTIVE_I|RESPONSE,   "",             connect_b3_t90_a_res},
  {_SELECT_B_REQ,                       "s",            select_b_req},
  {_MANUFACTURER_R,                     "dws",          manufacturer_req},
  {_MANUFACTURER_I|RESPONSE,            "dws",          manufacturer_res},
  {_MANUFACTURER_I|RESPONSE,            "",             manufacturer_res}
};

byte * cip_bc[29][2] = {
  { "",                     ""                     }, /* 0 */
  { "\x03\x80\x90\xa3",     "\x03\x80\x90\xa2"     }, /* 1 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 2 */
  { "\x02\x89\x90",         "\x02\x89\x90"         }, /* 3 */
  { "\x03\x90\x90\xa3",     "\x03\x90\x90\xa2"     }, /* 4 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 5 */
  { "\x02\x98\x90",         "\x02\x98\x90"         }, /* 6 */
  { "\x04\x88\xc0\xc6\xe6", "\x04\x88\xc0\xc6\xe6" }, /* 7 */
  { "\x04\x88\x90\x21\x8f", "\x04\x88\x90\x21\x8f" }, /* 8 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 9 */
  { "",                     ""                     }, /* 10 */
  { "",                     ""                     }, /* 11 */
  { "",                     ""                     }, /* 12 */
  { "",                     ""                     }, /* 13 */
  { "",                     ""                     }, /* 14 */
  { "",                     ""                     }, /* 15 */

  { "\x03\x80\x90\xa3",     "\x03\x80\x90\xa2"     }, /* 16 */
  { "\x03\x90\x90\xa3",     "\x03\x90\x90\xa2"     }, /* 17 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 18 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 20 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 21 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 22 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 23 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 24 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 25 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 26 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 27 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }  /* 28 */
};

byte * cip_hlc[29] = {
  "",                           /* 0 */
  "",                           /* 1 */
  "",                           /* 2 */
  "",                           /* 3 */
  "",                           /* 4 */
  "",                           /* 5 */
  "",                           /* 6 */
  "",                           /* 7 */
  "",                           /* 8 */
  "",                           /* 9 */
  "",                           /* 10 */
  "",                           /* 11 */
  "",                           /* 12 */
  "",                           /* 13 */
  "",                           /* 14 */
  "",                           /* 15 */

  "\x02\x91\x81",               /* 16 */
  "\x02\x91\x84",               /* 17 */
  "\x02\x91\xa1",               /* 18 */
  "\x02\x91\xa4",               /* 19 */
  "\x02\x91\xa8",               /* 20 */
  "\x02\x91\xb1",               /* 21 */
  "\x02\x91\xb2",               /* 22 */
  "\x02\x91\xb5",               /* 23 */
  "\x02\x91\xb8",               /* 24 */
  "\x02\x91\xc1",               /* 25 */
  "\x02\x91\x81",               /* 26 */
  "\x03\x91\xe0\x01",           /* 27 */
  "\x03\x91\xe0\x02"            /* 28 */
};

/*------------------------------------------------------------------*/

#define V120_HEADER_LENGTH 1
#define V120_HEADER_EXTEND_BIT  0x80
#define V120_HEADER_BREAK_BIT   0x40
#define V120_HEADER_C1_BIT      0x04
#define V120_HEADER_C2_BIT      0x08
#define V120_HEADER_FLUSH_COND  (V120_HEADER_BREAK_BIT | V120_HEADER_C1_BIT | V120_HEADER_C2_BIT)

static byte v120_default_header[] =
{

  0x83                          /*  Ext, BR , res, res, C2 , C1 , B  , F   */

};

static byte v120_break_header[] =
{

  0xc3 | V120_HEADER_BREAK_BIT  /*  Ext, BR , res, res, C2 , C1 , B  , F   */

};


/*------------------------------------------------------------------*/
/* API_PUT function                                                 */
/*------------------------------------------------------------------*/

static word api_put_reenter(APPL   * appl, CAPI_MSG   * msg)
{
  word i, j, k, l, n;
  word ret;
  byte c;
  byte controller;
  DIVA_CAPI_ADAPTER   * a;
  PLCI   * plci;
  NCCI   * ncci_ptr;
  word ncci;
  CAPI_MSG   *m;
  static API_PARSE msg_parms[MAX_MSG_PARMS+1];

  if (msg->header.length < sizeof (msg->header) ||
      msg->header.length > MAX_MSG_SIZE) {
    dbug(1,dprintf("bad len"));
    return _BAD_MSG;
  }

  controller = (byte)((msg->header.controller &0x7f)-1);

  /* controller starts with 0 up to (max_adapter - 1) */
  if ( controller >= max_adapter )
  {
    if (((msg->header.controller &0x7f) != 0)
     || ((msg->header.command != _MANUFACTURER_R)
      && (msg->header.command != (_MANUFACTURER_I|RESPONSE))))
    {
      dbug(1,dprintf("invalid ctrl"));
      return _BAD_MSG;
    }
    controller = 0;
  }

  a = &adapter[controller];
  plci = 0;
  if ((msg->header.plci != 0) && (msg->header.plci <= a->max_plci) && !a->adapter_disabled)
  {
    dbug(1,dprintf("plci=%x",msg->header.plci));
    plci = &a->plci[msg->header.plci-1];
    ncci = msg->header.ncci;
    if (plci->Id
     && (plci->appl
      || (plci->State == INC_CON_PENDING)
      || (plci->State == INC_CON_ALERT)
      || (msg->header.command == (_DISCONNECT_I|RESPONSE)))
     && ((ncci == 0)
      || (msg->header.command == (_DISCONNECT_B3_I|RESPONSE))
      || ((ncci < MAX_NCCI+1) && (a->ncci_plci[ncci] == plci->Id))))
    {
      i = plci->msg_in_read_pos;
      j = plci->msg_in_write_pos;
      if (j >= i)
      {
        if (j + msg->header.length + MSG_IN_OVERHEAD <= MSG_IN_QUEUE_SIZE)
          i = MSG_IN_QUEUE_SIZE - j + 1;
        else
          j = 0;
      }
      else
      {
        n = (((CAPI_MSG   *)(plci->msg_in_queue))->header.length + MSG_IN_OVERHEAD + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1);
        if (i > MSG_IN_QUEUE_SIZE - n)
          i = MSG_IN_QUEUE_SIZE - n + 1;
        i = (i > j) ? i - j : 0;
      }
      if (i <= ((msg->header.length + MSG_IN_OVERHEAD + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1)))
      {
        dbug(0,dprintf("Q-FULL1(msg) - len=%d write=%d read=%d wrap=%d free=%d",
          msg->header.length, plci->msg_in_write_pos,
          plci->msg_in_read_pos, plci->msg_in_wrap_pos, i));

        return _QUEUE_FULL;
      }
      c = FALSE;
      if ((((byte   *) msg) < ((byte   *)(plci->msg_in_queue)))
       || (((byte   *) msg) >= ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {
        if (plci->msg_in_write_pos != plci->msg_in_read_pos)
          c = TRUE;
      }
      if (msg->header.command == _DATA_B3_R)
      {
        if (msg->header.length < 20)
        {
          dbug(1,dprintf("DATA_B3 REQ wrong length %d", msg->header.length));
          return _BAD_MSG;
        }
        ncci_ptr = &(a->ncci[ncci]);
        n = ncci_ptr->data_pending;
        l = ncci_ptr->data_ack_pending;
        k = plci->msg_in_read_pos;
        while (k != plci->msg_in_write_pos)
        {
          if (k == plci->msg_in_wrap_pos)
            k = 0;
          if ((((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.command == _DATA_B3_R)
           && (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.ncci == ncci))
          {
            n++;
            if (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->info.data_b3_req.Flags & 0x0004)
              l++;
          }
          k += (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.length +
            MSG_IN_OVERHEAD + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1);
        }
        if ((n >= MAX_DATA_B3) || (l >= MAX_DATA_ACK))
        {
          dbug(0,dprintf("Q-FULL2(data) - pending=%d/%d ack_pending=%d/%d",
                          ncci_ptr->data_pending, n, ncci_ptr->data_ack_pending, l));

          return _QUEUE_FULL;
        }
        if (plci->req_in || plci->internal_command)
        {
          if ((((byte   *) msg) >= ((byte   *)(plci->msg_in_queue)))
           && (((byte   *) msg) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
          {
            dbug(0,dprintf("Q-FULL3(requeue)"));

            return _QUEUE_FULL;
          }
          c = TRUE;
        }
      }
      else
      {
        if (plci->req_in || plci->internal_command)
          c = TRUE;
      }
      if (c)
      {
        if (msg->header.command == (_DATA_B3_I|RESPONSE))
        {
          if (msg->header.length < 14)
          {
            dbug(1,dprintf("DATA_B3 RESP wrong length %d", msg->header.length));
            return _BAD_MSG;
          }
          n = READ_WORD(&(msg->info.data_b3_res.Number));
          if (!ncci
           || (n >= appl->MaxBuffer)
           || (appl->DataNCCI[n] != (ncci | (((word) a->Id) << 8)))
           || ((byte)(appl->DataFlags[n] >> 8) != plci->Id)
           || !(appl->DataFlags[n] & 0x4)
           || !(appl->DataFlags[n] & 0x10)
           || !plci->req_in)
          {
            c = FALSE;
          }
        }
        if (c)
        {
          dbug(1,dprintf("enqueue msg(0x%04x,0x%x,0x%x) - len=%d write=%d read=%d wrap=%d free=%d",
            msg->header.command, plci->req_in, plci->internal_command,
            msg->header.length, plci->msg_in_write_pos,
            plci->msg_in_read_pos, plci->msg_in_wrap_pos, i));
          if (j == 0)
            plci->msg_in_wrap_pos = plci->msg_in_write_pos;
          m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]);
          for (i = 0; i < msg->header.length; i++)
            ((byte   *)(plci->msg_in_queue))[j++] = ((byte   *) msg)[i];
          if (m->header.command == _DATA_B3_R)
          {
            /*
             * As the data pointer is only an index to an internal buffer, the
             * only thing is to convert the pointers in order to keep the compiler quiet.
             */
            m->info.data_b3_req.Data = (dword)PtrToUlong(TransmitBufferSet (appl, m->info.data_b3_req.Data));
          }
          j = (j + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1);
          *((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) = appl;
          plci->msg_in_write_pos = j + MSG_IN_OVERHEAD;
          return 0;
        }
      }
      else
      {
        plci->command = msg->header.command;
        plci->number = msg->header.number;
      }
    }
    else
    {
      plci = 0;
    }
  }
  dbug(1,dprintf("com=%x",msg->header.command));

  for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
  for(i=0, ret = _BAD_MSG;
      i<(sizeof(ftable)/sizeof(struct _ftable));
      i++) {

    if(ftable[i].command==msg->header.command) {
      /* break loop if the message is correct, otherwise continue scan  */
      /* (for example: CONNECT_B3_T90_ACT_RES has two specifications)   */
      if(!api_parse(&((byte   *)msg)[sizeof(msg->header)],(word)(msg->header.length-12),ftable[i].format,msg_parms)) {
        ret = 0;
        break;
      }
      for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
    }
  }
  if(ret) {
    dbug(1,dprintf("BAD_MSG"));
    if(plci) plci->command = 0;
    return ret;
  }


  c = ftable[i].function(READ_DWORD(&msg->header.controller),
                         msg->header.number,
                         a,
                         plci,
                         appl,
                         msg_parms);

  channel_xmit_extended_xon (plci);

  if(c==1) send_req(plci);
  if(c==2 && plci) plci->req_in = plci->req_in_start = plci->req_out = 0;
  if(plci && !plci->req_in) plci->command = 0;
  return 0;
}


word api_put(APPL   * appl, CAPI_MSG   * msg)
{
  word ret;

  ret = api_put_reenter (appl, msg);

  if (li_update_wait_list != NULL)
    mixer_update_done ();

  return (ret);
}


/*------------------------------------------------------------------*/
/* api_parse function, check the format of api messages             */

/*------------------------------------------------------------------*/

word api_parse(byte   * msg, word length, byte * format, API_PARSE * parms)
{
  word i;
  word p;

  for(i=0,p=0; format[i]; i++) {
    if(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    case 'b':
      p +=1;
      break;
    case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0xff) {
        if ((p+2) > length) return TRUE;
        parms[i].info +=2;
        parms[i].length = msg[p+1] + (msg[p+2]<<8);
        p +=(parms[i].length +3);
      }
      else {
        parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
      break;
    case 'r':
        parms[i].length = length - p;
        p += parms[i].length;
        break;
    }

    if(p>length) return TRUE;
  }
  if(parms) parms[i].info = 0;
  return FALSE;
}

void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out)
{
  word i, j, n;
  byte   *p;

  p = out->info;
  for (i = 0; format[i] != '\0'; i++)
  {
    out->parms[i].info = p;
    out->parms[i].length = in[i].length;
    switch (format[i])
    {
    case 'b':
      n = 1;
      break;
    case 'w':
      n = 2;
      break;
    case 'd':
      n = 4;
      break;
    case 's':
      n = in[i].length + 1;
      break;
    default:
      n = 0;
      break;
    }
    for (j = 0; j < n; j++)
      *(p++) = in[i].info[j];
  }
  out->parms[i].info = 0;
  out->parms[i].length = 0;
}

void api_load_msg(API_SAVE   *in, API_PARSE   *out)
{
  word i;

  i = 0;
  do
  {
    out[i].info = in->parms[i].info;
    out[i].length = in->parms[i].length;
  } while (in->parms[i++].info);
}

void api_swap_msg(API_SAVE   *in1, API_SAVE   *in2)
{
  word i, w;
  byte b;
  byte   *p;

  for (i = 0; i < MAX_MSG_PARMS+1; i++)
  {
    w = in1->parms[i].length;
    in1->parms[i].length = in2->parms[i].length;
    in2->parms[i].length = w;
    p = in1->parms[i].info;
    if (p != NULL)
      p += in2->info - in1->info;
    in1->parms[i].info = in2->parms[i].info;
    if (in1->parms[i].info != NULL)
      in1->parms[i].info += in1->info - in2->info;
    in2->parms[i].info = p;
  }
  for (i = 0; i < MAX_MSG_SIZE / sizeof(byte   *); i++)
  {
    p = ((byte   *   *)(in1->info))[i];
    ((byte   *   *)(in1->info))[i] = ((byte   *   *)(in2->info))[i];
    ((byte   *   *)(in2->info))[i] = p;
  }
  i = (MAX_MSG_SIZE / sizeof(byte   *)) * sizeof(byte   *);
  while (i < MAX_MSG_SIZE)
  {
    b = in1->info[i];
    in1->info[i] = in2->info[i];
    in2->info[i] = b;
    i++;
  }
}

byte api_compare_msg(API_SAVE   *in, byte *format, API_PARSE   *cmp)
{
  word i, n;
  byte   *p,   *q;

  i = 0;
  while (format[i] != '\0')
  {
    if (in->parms[i].length != cmp[i].length)
      return (1);
    p = in->parms[i].info;
    q = cmp[i].info;
    switch (format[i])
    {
    case 'b':
      n = 1;
      break;
    case 'w':
      n = 2;
      break;
    case 'd':
      n = 4;
      break;
    case 's':
      n = in->parms[i].length;
      p++;
      q++;
      break;
    default:
      n = 0;
      break;
    }
    while (n != 0)
    {
      if (*(p++) != *(q++))
        return (1);
      n--;
    }
    i++;
  }
  return (0);
}


/*------------------------------------------------------------------*/
/* CAPI remove function                                             */
/*------------------------------------------------------------------*/

word api_remove_start(void)
{
  word i;
  word j;

  if(!remove_started) {
    remove_started = TRUE;
    for(i=0;i<max_adapter;i++) {
      if(adapter[i].request) {
        for(j=0;j<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) plci_remove(&adapter[i].plci[j]);
        }
      }
    }
    return 1;
  }
  else {
    for(i=0;i<max_adapter;i++) {
      if(adapter[i].request) {
        for(j=0;j<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) return 1;
        }
      }
    }
  }
  api_remove_complete();
  return 0;
}


/*------------------------------------------------------------------*/
/* internal command queue                                           */
/*------------------------------------------------------------------*/

void init_internal_command_queue (PLCI   *plci)
{
  word i;

  dbug (1, dprintf ("%s,%d: init_internal_command_queue",
    (char   *)(FILE_), __LINE__));

  plci->internal_command = 0;
  for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS; i++)
    plci->internal_command_queue[i] = 0;
}


void start_internal_command (dword Id, PLCI   *plci, t_std_internal_command command_function)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: start_internal_command",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = command_function;
    (* command_function)(Id, plci, OK);
  }
  else
  {
    i = 1;
    while (plci->internal_command_queue[i] != 0)
      i++;
    plci->internal_command_queue[i] = command_function;
  }
}


void next_internal_command (dword Id, PLCI   *plci)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: next_internal_command",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  plci->internal_command = 0;
  plci->internal_command_queue[0] = 0;
  while (plci->internal_command_queue[1] != 0)
  {
    for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] = plci->internal_command_queue[i+1];
    plci->internal_command_queue[MAX_INTERNAL_COMMAND_LEVELS - 1] = 0;
    (*(plci->internal_command_queue[0]))(Id, plci, OK);
    if (plci->internal_command != 0)
      return;
    plci->internal_command_queue[0] = 0;
  }
}


/*------------------------------------------------------------------*/
/* NCCI allocate/remove function                                    */
/*------------------------------------------------------------------*/

dword diva_capi_ncci_mapping_bug = 0;

static word get_ncci (PLCI   *plci, byte ch, word force_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  word ncci, i, j, k;

  a = plci->adapter;
  if (!ch || a->ch_ncci[ch])
  {
    diva_capi_ncci_mapping_bug++;
    dbug(1,dprintf("NCCI mapping exists %ld %02x %02x %02x-%02x",
      (long) diva_capi_ncci_mapping_bug, ch, force_ncci, a->ncci_ch[a->ch_ncci[ch]], a->ch_ncci[ch]));
    ncci = ch;
  }
  else
  {
    if (force_ncci)
      ncci = force_ncci;
    else
    {
      if ((ch < MAX_NCCI+1) && !a->ncci_ch[ch])
        ncci = ch;
      else
      {
        ncci = 1;
        while ((ncci < MAX_NCCI+1) && a->ncci_ch[ncci])
          ncci++;
        if (ncci == MAX_NCCI+1)
        {
          diva_capi_ncci_mapping_bug++;
          i = 1;
          do
          {
            j = 1;
            while ((j < MAX_NCCI+1) && (a->ncci_ch[j] != i))
              j++;
            k = j;
            if (j < MAX_NCCI+1)
            {
              do
              {
                j++;
              } while ((j < MAX_NCCI+1) && (a->ncci_ch[j] != i));
            }
          } while ((i < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANNEL+1)
          {
            dbug(1,dprintf("NCCI mapping overflow %ld %02x %02x %02x-%02x-%02x",
              (long) diva_capi_ncci_mapping_bug, ch, force_ncci, i, k, j));
          }
          else
          {
            dbug(1,dprintf("NCCI mapping overflow %ld %02x %02x",
              (long) diva_capi_ncci_mapping_bug, ch, force_ncci));
          }
          ncci = ch;
        }
      }
      a->ncci_plci[ncci] = plci->Id;
      a->ncci_state[ncci] = IDLE;
      if (!plci->ncci_ring_list)
        plci->ncci_ring_list = ncci;
      else
        a->ncci_next[ncci] = a->ncci_next[plci->ncci_ring_list];
      a->ncci_next[plci->ncci_ring_list] = (byte) ncci;
    }
    a->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = (byte) ncci;
    dbug(1,dprintf("NCCI mapping established %ld %02x %02x %02x-%02x",
      (long) diva_capi_ncci_mapping_bug, ch, force_ncci, ch, ncci));
  }
  return (ncci);
}


static void ncci_free_receive_buffers (APPL   *appl, PLCI   *plci, word ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  word i, ncci_code;
  dword Id;

  a = plci->adapter;
  Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) | a->Id;
  if (!appl)
  {
    diva_capi_ncci_mapping_bug++;
    dbug(1,dprintf("NCCI mapping appl expected %ld %08lx",
      (long) diva_capi_ncci_mapping_bug, (unsigned long) Id));
  }
  else
  {
    if (ncci)
    {
      ncci_code = ncci | (((word) a->Id) << 8);
      for (i = 0; i < appl->MaxBuffer; i++)
      {
        if ((appl->DataNCCI[i] == ncci_code)
         && (((byte)(appl->DataFlags[i] >> 8)) == plci->Id))
        {
          appl->DataNCCI[i] = 0;
        }
      }
    }
    else
    {
      for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
      {
        ncci_code = ncci | (((word) a->Id) << 8);
        for (i = 0; i < appl->MaxBuffer; i++)
        {
          if ((appl->DataNCCI[i] == ncci_code)
           && (((byte)(appl->DataFlags[i] >> 8)) == plci->Id))
          {
            appl->DataNCCI[i] = 0;
          }
        }
      }
    }
  }
}


static void cleanup_ncci_data (PLCI   *plci, word ncci)
{
  NCCI   *ncci_ptr;

  if (ncci && (plci->adapter->ncci_plci[ncci] == plci->Id))
  {
    ncci_ptr = &(plci->adapter->ncci[ncci]);
    if (plci->appl)
    {
      while (ncci_ptr->data_pending != 0)
      {
        if (!plci->data_sent || (ncci_ptr->DBuffer[ncci_ptr->data_out].P != plci->data_sent_ptr))
          TransmitBufferFree (plci->appl, ncci_ptr->DBuffer[ncci_ptr->data_out].P);
        (ncci_ptr->data_out)++;
        if (ncci_ptr->data_out == MAX_DATA_B3)
          ncci_ptr->data_out = 0;
        (ncci_ptr->data_pending)--;
      }
    }
    ncci_ptr->data_out = 0;
    ncci_ptr->data_pending = 0;
    ncci_ptr->data_ack_out = 0;
    ncci_ptr->data_ack_pending = 0;
  }
}


static void ncci_remove (PLCI   *plci, word ncci, byte preserve_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  dword Id;
  word i;

  a = plci->adapter;
  Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) | a->Id;
  if (ncci)
  {
    if (a->ncci_plci[ncci] != plci->Id)
    {
      diva_capi_ncci_mapping_bug++;
      dbug(1,dprintf("NCCI mapping doesn't exist %ld %08lx %02x",
        (long) diva_capi_ncci_mapping_bug, (unsigned long) Id, preserve_ncci));
    }
    else
    {
      cleanup_ncci_data (plci, ncci);
      dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%02x",
        (long) diva_capi_ncci_mapping_bug, (unsigned long) Id, preserve_ncci, a->ncci_ch[ncci], ncci));
      a->ch_ncci[a->ncci_ch[ncci]] = 0;
      if (!preserve_ncci)
      {
        a->ncci_ch[ncci] = 0;
        a->ncci_plci[ncci] = 0;
        a->ncci_state[ncci] = IDLE;
        i = plci->ncci_ring_list;
        while ((i != 0) && (a->ncci_next[i] != plci->ncci_ring_list) && (a->ncci_next[i] != ncci))
          i = a->ncci_next[i];
        if ((i != 0) && (a->ncci_next[i] == ncci))
        {
          if (i == ncci)
            plci->ncci_ring_list = 0;
          else if (plci->ncci_ring_list == ncci)
            plci->ncci_ring_list = i;
          a->ncci_next[i] = a->ncci_next[ncci];
        }
        a->ncci_next[ncci] = 0;
      }
    }
  }
  else
  {
    for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      {
        cleanup_ncci_data (plci, ncci);
        dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%02x",
          (long) diva_capi_ncci_mapping_bug, (unsigned long) Id, preserve_ncci, a->ncci_ch[ncci], ncci));
        a->ch_ncci[a->ncci_ch[ncci]] = 0;
        if (!preserve_ncci)
        {
          a->ncci_ch[ncci] = 0;
          a->ncci_plci[ncci] = 0;
          a->ncci_state[ncci] = IDLE;
          a->ncci_next[ncci] = 0;
        }
      }
    }
    if (!preserve_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*------------------------------------------------------------------*/
/* application mask and c_ind_mask operations for arbitrary MAX_APPL*/
/*------------------------------------------------------------------*/

void clear_appl_mask (dword   *appl_mask_table)
{
  word i;

  for (i = 0; i < APPL_MASK_DWORDS; i++)
    appl_mask_table[i] = 0;
}

byte appl_mask_empty (dword   *appl_mask_table)
{
  word i;

  i = 0;
  while ((i < APPL_MASK_DWORDS) && (appl_mask_table[i] == 0))
    i++;
  return (i == APPL_MASK_DWORDS);
}

#define set_appl_mask_bit(appl_mask_table,b)  ((appl_mask_table)[(b) >> 5] |= (1L << ((b) & 0x1f)))

#define clear_appl_mask_bit(appl_mask_table,b)  ((appl_mask_table)[(b) >> 5] &= ~(1L << ((b) & 0x1f)))

#define test_appl_mask_bit(appl_mask_table,b)  (((appl_mask_table)[(b) >> 5] & (1L << ((b) & 0x1f))) != 0)

void dump_appl_mask (dword   *appl_mask_table, char   *caption)
{
static char hex_digit_table[0x10] =
  {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  word i, j, k;
  dword d;
  char *p;
    char buf[40];

  for (i = 0; i < APPL_MASK_DWORDS; i += 4)
  {
    p = buf + 36;
    *p = '\0';
    for (j = 0; j < 4; j++)
    {
      if (i+j < APPL_MASK_DWORDS)
      {
        d = appl_mask_table[i+j];
        for (k = 0; k < 8; k++)
        {
          *(--p) = hex_digit_table[d & 0xf];
          d >>= 4;
        }
      }
      else if (i != 0)
      {
        for (k = 0; k < 8; k++)
          *(--p) = ' ';
      }
      *(--p) = ' ';
    }
    dbug(1,dprintf ("%s[%02x]%s", caption, i << 5, (char   *) p));
  }
}

#define clear_c_ind_mask(plci) clear_appl_mask ((plci)->c_ind_mask_table)

#define c_ind_mask_empty(plci) appl_mask_empty ((plci)->c_ind_mask_table)

void set_c_ind_mask_bit (PLCI   *plci, word b)
{
  set_appl_mask_bit (plci->c_ind_mask_table, b);
}

void clear_c_ind_mask_bit (PLCI   *plci, word b)
{
  clear_appl_mask_bit (plci->c_ind_mask_table, b);
}

byte test_c_ind_mask_bit (PLCI   *plci, word b)
{
  return (test_appl_mask_bit (plci->c_ind_mask_table, b));
}

void dump_c_ind_mask (PLCI   *plci)
{
  dump_appl_mask (plci->c_ind_mask_table, "c_ind_mask");
}


static void clear_incoming_call (PLCI   *plci, byte disc_others, word reason)
{
  dword Id;
  word i;

  if (disc_others)
  {
    Id = ((word)plci->Id<<8)|plci->adapter->Id;
    for (i = 0; i < max_appl; i++)
    {
      if (test_c_ind_mask_bit (plci, i))
        sendf (&application[i], _DISCONNECT_I, Id, 0, "w", reason);
    }
  }
  reserve_clear_incoming_call (plci);
}


static void assign_incoming_call (PLCI   *plci, APPL   *appl, byte disc_others)
{
  dword Id;
  word i;

  plci->appl = appl;
  clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
  dump_c_ind_mask (plci);
  if (disc_others)
  {
    Id = ((word)plci->Id<<8)|plci->adapter->Id;
    for (i = 0; i < max_appl; i++)
    {
      if (test_c_ind_mask_bit (plci, i))
        sendf (&application[i], _DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
    }
  }
  reserve_assign_incoming_call (plci, appl);
}


static void ignore_incoming_call (PLCI   *plci, APPL   *appl)
{
  clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
  dump_c_ind_mask (plci);
  if (c_ind_mask_empty (plci))
    reserve_clear_incoming_call (plci);
  else
    reserve_ignore_incoming_call (plci, appl);
}


void dump_plcis (DIVA_CAPI_ADAPTER   *a)
{
static char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

static word power_10_table[] = { 1, 10, 100, 1000, 10000 };
  word k, w, l;

  PLCI   *plci;
  static char text_line[256];
  char *p;
  word i, j;
  byte b;

/* Format of a PLCI Debug Line:
PLCI[01,00] 00c800  4934  5060  4918 10815  4912  3056 10815  3018  3011 11551  5650  4918 10815  7502  7492  4934
                    16 sourcecodeline positions of the last PLCI actions starting with the last action
            ||0x0f-State
              0x10-sig_req
              0x20-nl_req
              0x40-req_in!=req_out
              0x80-sig_remove_id!00 || nl_remove_id!00
              ||0x01-channels!00
                0x02-!c_ind_mask_empty
                0x04-NL.Id!=0
                0x08-Sig.Id!=0
                0x10-command!=0
                0x20-internal_command!=0
                0x40-appl!=0
                0x80-Id!=0
                ||(SuppState | (ptyState << 3))
*/

  for (i = 0; i < a->max_plci; i += 16)
  {
    p = text_line;
    for (j = 0; (j < 16) && (i+j < a->max_plci); j++)
    {
      plci = &(a->plci[i+j]);
      *(p++) = ' ';
      b = plci->State & 0xf;
      if (plci->sig_req)
        b |= 0x10;
      if (plci->nl_req)
        b |= 0x20;
      if (plci->req_in!=plci->req_out)
        b |= 0x40;
      if ((plci->sig_remove_id != 0) || (plci->nl_remove_id != 0))
        b |= 0x80;
      *(p++) = hex_digit_table[b >> 4];
      *(p++) = hex_digit_table[b & 0xf];
      b = 0;
      if (plci->channels != 0)
        b |= 0x01;
      if (!c_ind_mask_empty (plci))
        b |= 0x02;
      if (plci->NL.Id != 0)
        b |= 0x04;
      if (plci->Sig.Id != 0)
        b |= 0x08;
      if (plci->command)
        b |= 0x10;
      if (plci->internal_command)
        b |= 0x20;
      if (plci->appl)
        b |= 0x40;
      if (plci->Id)
        b |= 0x80;
      *(p++) = hex_digit_table[b >> 4];
      *(p++) = hex_digit_table[b & 0xf];
      b = (byte)(plci->SuppState | (plci->ptyState << 3));
      *(p++) = hex_digit_table[b >> 4];
      *(p++) = hex_digit_table[b & 0xf];

      k = plci->trace_pos;
      do
      {
        k = (k == 0) ? TRACE_QUEUE_ENTRIES - 1 : k - 1;
        *(p++) = ' ';
        w = plci->trace_queue[k];
        l = sizeof(power_10_table) / sizeof(power_10_table[0]) - 1;
        while ((l != 0) && (power_10_table[l] > w))
        {
          l--;
          *(p++) = ' ';
        }
        l++;
        do
        {
          l--;
          b = '0';
          while (w >= power_10_table[l])
          {
            w -= power_10_table[l];
            b++;
          }
          *(p++) = b;
        } while (l != 0);
      } while (k != plci->trace_pos);
      *p = '\0';

      DBG_FTL (("PLCI[%02x,%02x]%s", UnMapController (a->Id), i+j, text_line))

      p = text_line;

    }

  }
}


/*------------------------------------------------------------------*/
/* PLCI remove function                                             */
/*------------------------------------------------------------------*/

static void plci_free_msg_in_queue (PLCI   *plci)
{
  word i, j;

  if (plci->appl)
  {
    i = plci->msg_in_read_pos;
    while (i != plci->msg_in_write_pos)
    {
      if (i == plci->msg_in_wrap_pos)
        i = 0;
      j = i + ((((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.length +
        (sizeof(void   *)-1)) & ~(sizeof(void   *)-1));
      if (*((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) != NULL)
      {
        if (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.command == _DATA_B3_R)
        {
          TransmitBufferFree (plci->appl,
            ULongToPtr(((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->info.data_b3_req.Data));
        }
      }
      i = j + MSG_IN_OVERHEAD;
    }
  }
  plci->msg_in_write_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_read_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
}


static void plci_free_queued_appl_msg (PLCI   *plci, APPL   * appl)
{
  word i, j;

  i = plci->msg_in_read_pos;
  while (i != plci->msg_in_write_pos)
  {
    if (i == plci->msg_in_wrap_pos)
      i = 0;
    j = i + ((((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.length +
      (sizeof(void   *)-1)) & ~(sizeof(void   *)-1));
    if (*((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) == appl)
    {
      *((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) = NULL;
      if (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.command == _DATA_B3_R)
      {
        TransmitBufferFree (appl,
          ULongToPtr(((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->info.data_b3_req.Data));
      }
    }
    i = j + MSG_IN_OVERHEAD;
  }
}


void plci_remove_(PLCI   * plci)
{

  if(!plci) {
    dbug(1,dprintf("plci_remove(no plci)"));
    return;
  }
  init_internal_command_queue (plci);
  dbug(1,dprintf("plci_remove(%x,tel=%x)",plci->Id,plci->tel));
  if(plci_remove_check(plci))
  {
    trcq (plci);
    return;
  }
  if (plci->Sig.Id == 0xff)
  {
    dbug(1,dprintf("D-channel X.25 plci->NL.Id:%0x", plci->NL.Id));
    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nl_req_ncci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->sig_remove_id
     && (plci->Sig.Id
      || (plci->req_in!=plci->req_out)
      || (plci->nl_req || plci->sig_req)))
    {
      sig_req(plci,HANGUP,0);
      send_req(plci);
    }
  }
  if (plci->appl)
    ncci_free_receive_buffers (plci->appl, plci, 0);
  if (plci->Id)
    ncci_remove (plci, 0, FALSE);
  plci_free_msg_in_queue (plci);

  plci->channels = 0;
  plci->appl = 0;
  if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
  {
    plci->State = OUTG_DIS_PENDING;
    trcq (plci);
    clear_incoming_call (plci, FALSE, 0);
  }
}

/*
NOTE: the listen has not really effect if there is a MTPX between capi and adaper because the MTPX sets/controlls the
listener entities by itself and dont cares what the capi is setting here.
*/
void add_p_listen(PLCI   * plci)
{
  add_p(plci,OAD,"\x01\xfd");

  add_p(plci,KEY,"\x04\x43\x41\x32\x30");

  add_p(plci,CAI,"\x01\xc0");
  add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
  /* NCRL(non call related link): continue sending the bits NCR_FACILITY and MWI because we need them
  for compatibility for old protocolcodes, in a new protocolcode we switch off these bits if we get a NCRL ASSIGN */
  if(plci->State == LISTENING_NCRL) {
    add_p(plci,LLI,"\x02\x00\x08"); /* NCRL (non call related link) */
  }
  else {
    add_p(plci,LLI,"\x02\xc4\x02"); /* support Dummy CR FAC + MWI + SpoofNotify, Profile change */
  }
  add_p(plci,ESC,"\x02\x19\x68"); /* Capi can handle resource causes -> 0x20 and long facility -> 0x40 and SS-Notifications -> 0x08 */
  add_p(plci,SHIFT|0x08|6,0); /* non-locking SHIFT, codeset 6 */
  add_p(plci,SIN,"\x02\x00\x00");
}


void add_p_indicate(PLCI   * plci)
{
  add_p(plci,ESC,"\x02\x18\x00");             /* support call waiting */
}


/*------------------------------------------------------------------*/
/* translation function for each message                            */
/*------------------------------------------------------------------*/

byte connect_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ch;
  word i,j;
  word Info;
  word CIP;
  word LinkLayer;
  API_PARSE * ai;
  API_PARSE * bp;
    API_PARSE ai_parms[6];
  word channel = 0;
  dword ch_mask;
  static byte esc_chi[35] = {0x02,0x18,0x01};
  static byte lli[2] = {0x01,0x00};
  static byte esc_lli[255] = {0x02,0x19,0x00};
  byte noCh = 0;
  word dir = 0;
  byte   *p_chi = "";
  byte calledfromccbscall=0;
    byte SSparms[] = "\x05\x00\x00\x02\x00\x00";
  word sendingcomplete=0xffff;
  API_PARSE * pcplci;
  dword consultplci=0;
  PLCI   *rplci=0;
  static byte ssextie_t[255];
  DIVA_CAPI_ADAPTER   * relatedadapter;

  for(i=0;i<6;i++) ai_parms[i].length = 0;
  /* we call connect_req from a facility request S_CCBS_CALL,
  the calling party number is empty at a CCBS_CALL so we use it to send further infos
  for the CCBS_CALL to the protocol */
  if(parms[1].length==7 &&
      parms[1].info[0]>=7 &&
      parms[1].info[1]=='C' &&
      parms[1].info[2]=='C' &&
      parms[1].info[3]=='B' &&
      parms[1].info[4]=='S' &&
      parms[1].info[5]==2)
  {
    calledfromccbscall=1;
  }
  dbug(1,dprintf("connect_req(%d)",parms->length));
  Info = _WRONG_IDENTIFIER;

  if(a != 0 && (a->vscapi_mode == 0 || (appl->access & DIVA_CAPI_APPL_VSCAPI_ACCESS) != 0))
  {
    if(a->adapter_disabled)
    {
      dbug(1,dprintf("adapter disabled"));
      Id = ((word)1<<8)|a->Id;
      if(calledfromccbscall)
      {
        WRITE_WORD(&SSparms[1],S_CCBS_CALL);
        WRITE_WORD(&SSparms[4],0x3010); /* Request not allowed in this state */
        sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",0x3010,(word)3,SSparms);
      }
      else
      {
        sendf(appl,_CONNECT_R|CONFIRM,Id,Number,"w",0);
        sendf(appl, _DISCONNECT_I, Id, 0, "w", _L1_ERROR);
      }
      return FALSE;
    }
    Info = _OUT_OF_PLCI;
    if((i=get_plci(a)))
    {
      Info = 0;
      plci = &a->plci[i-1];
      plci->appl = appl;
      plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
      if(calledfromccbscall) plci->calledfromccbscall=calledfromccbscall;
      diva_capi_apply_saved_action (appl, plci, Number);
      /* check 'external controller' bit for codec support */
      if(Id & EXT_CONTROLLER)
      {
        if(AdvCodecSupport(a, plci, appl, 0) )
        {
          free_plci (plci);
          if(calledfromccbscall)
          {
            WRITE_WORD(&SSparms[1],S_CCBS_CALL);
            WRITE_WORD(&SSparms[4],0x3010); /* Request not allowed in this state */
            sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",0x3010,(word)3,SSparms);
          }
          else sendf(appl, _CONNECT_R|CONFIRM, Id, Number, "w", _WRONG_IDENTIFIER);
          return 2;
        }
      }
      ai = &parms[9];
      bp = &parms[5];
      ch = 0;
      if(bp->length)LinkLayer = READ_WORD(&(bp->info[3]));
      else LinkLayer = 0;
      if(ai->length)
      {
        ch=0xffff;
        if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
        {
          ch = 0;
          if(ai_parms[0].length)
          {
            ch = READ_WORD(ai_parms[0].info+1);
            if(ch>4 && (ch != PRIV_BCHANNEL_INFO_NO_RESERVATION))
              Info = _WRONG_MESSAGE_FORMAT; /* safety -> ignore ChannelID */
            if(ch==4) /* explizit CHI in message */
            {
              /* check length of B-CH struct */
              if((ai_parms[0].info)[3]>=1)
              {
                if((ai_parms[0].info)[4]==CHI)
                {
                  p_chi = &((ai_parms[0].info)[5]);
                }
                else
                {
                  p_chi = &((ai_parms[0].info)[3]);
                }
                if(p_chi[0]>35) /* check length of channel ID */
                {
                  Info = _WRONG_MESSAGE_FORMAT;
                }
              }
              else Info = _WRONG_MESSAGE_FORMAT;
            }

            if((ch==3 || (ch == PRIV_BCHANNEL_INFO_NO_RESERVATION))
             && ai_parms[0].length>=7 && ai_parms[0].length<=36)
            {
              dir = READ_WORD(ai_parms[0].info+3);
              ch_mask = 0;
              for(i=0; i+5<=ai_parms[0].length; i++)
              {
                if(ai_parms[0].info[i+5]!=0)
                {
                  if ((ai_parms[0].info[i+5] != 0xff)
                   && (ai_parms[0].info[i+5] != 0xfe)
                   && (ai_parms[0].info[i+5] != 0x7e)
                   && (ai_parms[0].info[i+5] != 0xfd)
                   && ((i != 0)
                    || (ai_parms[0].info[i+5] != 0xc0)))
                  {
                    Info = _WRONG_MESSAGE_FORMAT;
                  }
                  else
                  {
                    if (ch_mask == 0)
                      channel = i;
                    ch_mask |= 1L << i;
                  }
                }
              }
              if (ch_mask == 0)
              {

                if ((a->manufacturer_features2 & MANUFACTURER_FEATURE2_NULL_PLCI)
                 && (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT))
                {
                  channel = 0;
                  if ((a->null_plci_base != 32) || (a->null_plci_table[channel] != 0))
                  {
                    channel = a->null_plci_base;
                    while ((channel < a->null_plci_base + a->null_plci_channels) && (a->null_plci_table[channel] != 0))
                      channel++;
                  }
                  if (channel < a->null_plci_base + a->null_plci_channels)
                  {
                    a->null_plci_table[channel] = plci->Id;
                    esc_chi[0] = 34;
                    esc_chi[2] = (byte) channel;
                    for (i = 0; i < 32; i++)
                      esc_chi[3+i] = 0;
                    plci->B1_options |= DSP_CAI_ARRANGEMENT_NULL_PLCI;
                    plci->b_channel = (byte) channel;
                    mixer_set_bchannel_id_unchecked (plci, plci->b_channel);
                    trcq (plci);
                    plci->State = LOCAL_CONNECT;
                  }
                  else
                  {
                    dbug(1,dprintf("NULL PLCI exhaustet"));
                    Info = _OUT_OF_PLCI;
                  }
                }
                else

                {
                  dbug(1,dprintf("NULL PLCI not supported"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
              }
              else if (ch_mask==1 && !Info){ /* if somebody masks the D-Channel then we simply say: "Use D-Channel" */
                ch=1;
              }
              else if (!Info)
              {
                if ((ai_parms[0].length == 36) || (ch_mask != ((dword)(1L << channel))))
                {
                  esc_chi[0] = (byte)(ai_parms[0].length - 2);
                  for(i=0; i+5<=ai_parms[0].length; i++)
                    esc_chi[i+3] = ai_parms[0].info[i+5];
                }
                else
                  esc_chi[0] = 2;
                esc_chi[2] = (byte)channel;
                plci->b_channel = (byte)channel; /* not correct for ETSI ch 17..31 if mask length is below 32 */
                trcq (plci);
                plci->State = LOCAL_CONNECT;
              }
            }
          }
          /* search for sending complete */
          if(!api_parse(&ai->info[1],(word)ai->length,"sssss",ai_parms))
          {
            if(ai_parms[4].length>=2) sendingcomplete=READ_WORD(&ai_parms[4].info[1]);
          }
        }
        else  Info = _WRONG_MESSAGE_FORMAT;
      }

      /* look if we have a consultationcall PLCI and do the necessary actions
      like adding information from the first callleg... */
      pcplci = &parms[11];
      if(pcplci->length == 4)
      {
        consultplci = READ_DWORD(&pcplci->info[1]);
        consultplci &= 0x0000FFFF;
        dbug(1,dprintf("cPLCI=%lx", (unsigned long) consultplci));
        /* controller starts with 0 up to (max_adapter - 1) */
        if (((consultplci & 0x7f) == 0)
         || (MapController ((byte)(consultplci & 0x7f)) == 0)
         || (MapController ((byte)(consultplci & 0x7f)) > max_adapter))
        {
          dbug(0, dprintf("wrong Controller in cPLCI"));
          /* accept it anyway and do nothing ?? */
        }
        else
        {
          relatedadapter = &adapter[MapController ((byte)(consultplci & 0x7f))-1];
          consultplci >>=8;
          /* find PLCI PTR*/
          rplci = &relatedadapter->plci[consultplci-1];
          if (consultplci!=0 && consultplci<=relatedadapter->max_plci
           && rplci->Id && rplci->State!=IDLE)
          { /* remember the consultation PLCI */
            rplci->relatedConsultPLCI=plci;
            plci->relatedConsultPLCI=rplci;
          }
        }
      }

      dbug(1,dprintf("ch=%x,dir=%x,p_ch=%d",ch,dir,channel));
      plci->command = _CONNECT_R;
      plci->number = Number;
      if(dir==1)
      {
        plci->call_dir = (plci->call_dir & ~(CALL_DIR_OUT | CALL_DIR_ORIGINATE)) |
          CALL_DIR_IN | CALL_DIR_ANSWER;
      }
      /* x.31 or D-ch free SAPI in LinkLayer or transparent or listening LinkLayer? */
      if(ch==1 && LinkLayer!=3 && LinkLayer!=12 && LinkLayer!=1 && LinkLayer!=27) noCh = TRUE;
      if((ch==0 || ch==2 || noCh || ch==3 || ch==4 || (ch == PRIV_BCHANNEL_INFO_NO_RESERVATION)) && !Info)
      {
        if (plci->State == LOCAL_CONNECT)
        {
          lli[1] = 0x00;
          if (ch == PRIV_BCHANNEL_INFO_NO_RESERVATION)
            lli[1] |= 0x12;
          add_p(plci,LLI,lli);
          add_p(plci,ESC,esc_chi);
        }
        else if (a->Info_Mask[appl->Id-1] & 0x200)
        {
          /* early B3 connect (CIP mask bit 9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
          /*esc_lli[2]|=0x4; lli todo */
        }
        /* B-channel used for B3 connections (ch==0), or no B channel    */
        /* is used (ch==2) or perm. connection (3) is used  do a CALL    */
        if(noCh || ch==2) Info = add_b1(plci,&parms[5],2,0);    /* no resource    */
        else     Info = add_b1(plci,&parms[5],ch,0);
        add_s(plci,OAD,&parms[2]);
        add_s(plci,OSA,&parms[4]);
        add_s(plci,BC,&parms[6]);
        add_s(plci,LLC,&parms[7]);
        add_s(plci,HLC,&parms[8]);
        CIP = READ_WORD(parms[0].info);
        esc_lli[2]=0;
        if(a->Notification_Mask[appl->Id-1]&SMASK_CCBS) esc_lli[2]|=0x1;
        if(a->Notification_Mask[appl->Id-1]&SMASK_CCNR) esc_lli[2]|=0x2;
        esc_lli[2]|=0x28; /* Capi can handle SS-Notifications and resource causes */
        if(sendingcomplete!=0xffff) esc_lli[2]|=0x10; /* sending complete exists in AI */
        esc_lli[2]|=0x40; /* long facility support */
        /* ESC_LLI is now a true bitmask. Its used to send special information,
          features about the call */
        if(esc_lli[2]) add_p(plci,ESC,esc_lli);
        if(READ_WORD(parms[0].info)<29) {
          add_p(plci,BC,cip_bc[READ_WORD(parms[0].info)][a->u_law]);
          add_p(plci,HLC,cip_hlc[READ_WORD(parms[0].info)]);
        }
        add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
        sig_req(plci,ASSIGN,DSIG_ID);
      }
      else if(ch==1) {

        /* D-Channel used for B3 connections */
        plci->Sig.Id = 0xff;
        Info = 0;
      }

      if(!Info && ch!=2 && !noCh && ((ch==1) || (plci->B1_resource != 0))) {
        Info = add_b23(plci,&parms[5]);
        if(!Info) {
          if(!(plci->tel && !plci->adv_nl)){
            if (ch==1)
              add_ai(plci,&parms[9]);
            nl_req_ncci(plci,ASSIGN,0);
          }
        }
      }

      if(!Info)
      {
        if(ch==0 || ch==2 || ch==3 || noCh || ch==4 || (ch == PRIV_BCHANNEL_INFO_NO_RESERVATION))
        {
          if(plci->spoofed_msg==SPOOFING_REQUIRED)
          {
            api_save_msg(parms, "wsssssssss", &plci->saved_msg);
            plci->spoofed_msg = CALL_REQ;
            plci->internal_command = BLOCK_PLCI;
            plci->command = 0;
            dbug(1,dprintf("Spoof"));
            send_req(plci);
            return FALSE;
          }
          if(sendingcomplete==1) add_p(plci,SDNCMPL,0);
          if(ch==4)add_p(plci,CHI,p_chi);
          add_s(plci,CPN,&parms[1]);
          add_s(plci,DSA,&parms[3]);
          if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /* D-channel, no B-L3 */
          if(ch==2) add_p(plci,ESC,"\x02\x18\x00");  /* no B- no D-Channel */
          add_ai(plci,&parms[9]);

          if(plci->relatedConsultPLCI && rplci && plci->relatedConsultPLCI==rplci)
          {
            if(rplci->cinfo.scs7ie[0])
            {
              for(i=0;i<(rplci->cinfo.scs7ie[0]+1);i++) ssextie_t[7+i]=rplci->cinfo.scs7ie[i];
              ssextie_t[0]=6+ssextie_t[7]+1; /* length */
              ssextie_t[1]=SSEXTIE;
              ssextie_t[2]=SSEXT_REQ;
              ssextie_t[3]=3+ssextie_t[7]+1; /* length */
              WRITE_WORD(&ssextie_t[4],SSTRANSPORT); /* CornetCommand */
              ssextie_t[6]=SCS7IE_IND; /* secret siemens ie */
              add_p(plci,ESC,ssextie_t);
            }
            if(rplci->cinfo.scntinfo[0])
            {
              for(i=0;i<(rplci->cinfo.scntinfo[0]+1);i++) ssextie_t[7+i]=rplci->cinfo.scntinfo[i];
              ssextie_t[0]=6+ssextie_t[7]+1; /* length */
              ssextie_t[1]=SSEXTIE;
              ssextie_t[2]=SSEXT_REQ;
              ssextie_t[3]=3+ssextie_t[7]+1; /* length */
              WRITE_WORD(&ssextie_t[4],SSTRANSPORT); /* CornetCommand */
              ssextie_t[6]=SCNT_IND; /* siemens cornet-n */
              add_p(plci,ESC,ssextie_t);
            }
            /* now send it packed in one element because we have only max. 5 */
            j=6;
            if(rplci->cinfo.cardident[0])
            {
              ssextie_t[j++]=CARDIDENT_IND;
              for(i=0;i<(rplci->cinfo.cardident[0]+1);i++) ssextie_t[j+i]=rplci->cinfo.cardident[i];
              j+=(rplci->cinfo.cardident[0]+1);
            }
            if(rplci->cinfo.linkident[0])
            {
              ssextie_t[j++]=LINKIDENT_IND;
              for(i=0;i<(rplci->cinfo.linkident[0]+1);i++) ssextie_t[j+i]=rplci->cinfo.linkident[i];
              j+=(rplci->cinfo.linkident[0]+1);
            }
            if(j>6)
            {
              ssextie_t[0]=j-1; /* length */
              ssextie_t[1]=SSEXTIE;
              ssextie_t[2]=SSEXT_REQ;
              ssextie_t[3]=ssextie_t[0]-3; /* length */
              WRITE_WORD(&ssextie_t[4],SSTRANSPORT); /* CornetCommand */
              add_p(plci,ESC,ssextie_t);
            }
          }

          if(!dir)sig_req(plci,CALL_REQ,0);
          else
          {
            plci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(plci,LISTEN_REQ,0);
            send_req(plci);
            return FALSE;
          }
        }
        send_req(plci);
        return FALSE;
      }
      free_plci (plci);
    } else {
      diva_capi_apply_saved_action (appl, 0, Number);
    }
  }
  if(calledfromccbscall)
  {
    WRITE_WORD(&SSparms[1],S_CCBS_CALL);
    WRITE_WORD(&SSparms[4],Info);
    sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",Info,(word)3,SSparms);
  }
  else
  {
    sendf(appl,
          _CONNECT_R|CONFIRM,
          Id,
          Number,
          "w",Info);
  }
  return 2;
}

byte connect_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word i, Info;
  word Reject;
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->State));
  for(i=0;i<5;i++) ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dprintf("ai->length=%d",ai->length));

  if(ai->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("ai_parms[0].length=%d/0x%x",ai_parms[0].length,READ_WORD(ai_parms[0].info+1)));
      ch = 0;
      if(ai_parms[0].length)
      {
        ch = READ_WORD(ai_parms[0].info+1);
        dbug(1,dprintf("BCH-I=0x%x",ch));
      }
    }
  }

  Reject = READ_WORD(parms[0].info);
  dbug(1,dprintf("Reject=0x%x",Reject));

  if((plci->State==INC_CON_CONNECTED_PEND) || (plci->State==INC_CON_CONNECTED_ALERT))
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
    if(Reject) {
      reject_call(Id, plci, appl, Reject, ai);
      return 1;
    }
    if (a->Info_Mask[appl->Id-1] & 0x200)
    {
    /* early B3 connect (CIP mask bit 9) no release after a disc */
      add_p(plci,LLI,"\x01\x01");
    }
    add_s(plci, CONN_NR, &parms[2]);
    add_s(plci, LLC, &parms[4]);
    add_ai(plci, &parms[5]);
    plci->State = INC_CON_ACCEPT;
    sig_req(plci, CALL_RES,0);
    if ((parms[1].length != 0) && (api_compare_msg(&plci->B_protocol, "s", &parms[1]) != 0))
    {
      api_save_msg(&parms[1], "s", &plci->saved_msg);
      if (plci->call_dir & CALL_DIR_OUT)
        plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
      else if (plci->call_dir & CALL_DIR_IN)
        plci->call_dir = CALL_DIR_IN | CALL_DIR_ANSWER;
      start_internal_command (Id, plci, connect_res_command);
    }
    return 1;
  }
  else if(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT || (plci->State==INC_CON_BLIND_RETRIEVE)) {
    if(Reject) {
      ignore_incoming_call (plci, appl);
      reject_call(Id, plci, appl, Reject, ai);
    }
    else {
      trcq (plci);
      assign_incoming_call (plci, appl, (byte)(plci->State != INC_CON_BLIND_RETRIEVE));
      plci->State = INC_CON_ACCEPT;
      if(Id & EXT_CONTROLLER){
        if(AdvCodecSupport(a, plci, appl, 0)){
          dbug(1,dprintf("connect_res(error from AdvCodecSupport)"));
          sig_req(plci,HANGUP,0);
          return 1;
        }
        if(plci->tel == ADV_VOICE && a->AdvCodecPLCI)
        {
          Info = add_b23(plci, &parms[1]);
          if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23)"));
            sig_req(plci,HANGUP,0);
            return 1;
          }
          if(plci->adv_nl)
          {
            nl_req_ncci(plci, ASSIGN, 0);
          }
        }
      }
      else
      {
        plci->tel = 0;
        if((ch!=2) && ((parms[1].length < 2) || (READ_WORD(&(parms[1].info[1])) != B1_NO_RESOURCE)))
        {
          Info = add_b23(plci, &parms[1]);
          if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23 2)"));
            sig_req(plci,HANGUP,0);
            return 1;
          }
          nl_req_ncci(plci, ASSIGN, 0);
        }
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plci->saved_msg);
        plci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1] & 0x200)
        {
          /* early B3 connect (CIP mask bit 9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
        }
        add_s(plci, CONN_NR, &parms[2]);
        add_s(plci, LLC, &parms[4]);
        add_ai(plci, &parms[5]);
        sig_req(plci, CALL_RES,0);
        mixer_set_bchannel_id_esc (plci, plci->b_channel);
      }
    }
  }
  return 1;
}

byte connect_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  dbug(1,dprintf("connect_a_res"));
  return FALSE;
}

byte disconnect_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word Info;

  word i;


  dbug(1,dprintf("disconnect_req"));

  Info = _WRONG_IDENTIFIER;

  if(plci)
  {
    if(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT)
    {
      assign_incoming_call (plci, appl, TRUE);
      plci->State = OUTG_DIS_PENDING;
      trcq (plci);
    }
    if(plci->Sig.Id && plci->appl)
    {
      Info = 0;
      if(plci->Sig.Id!=0xff)
      {
        if((plci->State!=IDLE) && (plci->State!=INC_DIS_PENDING))
        {

          for (i = 0; i < a->null_plci_base + a->null_plci_channels; i++)
          {
            if (a->null_plci_table[i] == Id)
              a->null_plci_table[i] = 0;
          }

          add_ai(plci, &msg[0]);
          sig_req(plci,HANGUP,0);
          plci->State = OUTG_DIS_PENDING;
          mixer_remove (plci);
          return 1;
        }
        else
          trcq (plci);
      }
      else
      {
        if (plci->NL.Id && !plci->nl_remove_id)
        {
          plci->State = INC_DIS_PENDING;
          mixer_remove (plci);
          nl_req_ncci(plci,REMOVE,0);
          sendf(appl,_DISCONNECT_R|CONFIRM,Id,Number,"w",0);
          sendf(appl, _DISCONNECT_I, Id, 0, "w", 0);
        }
        return 1;
      }
    }
    else
      trcq (plci);
  }

  if(!appl)  return FALSE;
  sendf(appl, _DISCONNECT_R|CONFIRM, Id, Number, "w",Info);
  return FALSE;
}

byte disconnect_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word i;

  dbug(1,dprintf("disconnect_res"));
  if(plci)
  {
    /* clear ind mask bit, just in case of collsion of          */
    /* DISCONNECT_IND and CONNECT_RES                           */
    clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
    ncci_free_receive_buffers (appl, plci, 0);
    if(plci_remove_check(plci))
    {
      trcq (plci);
      return 0;
    }
    if(plci->State==INC_DIS_PENDING
    || plci->State==SUSPENDING) {
      if(c_ind_mask_empty (plci)) {
        if(plci->State!=SUSPENDING)
        {
          plci->State = IDLE;
          trcq (plci);
        }
        else
          trcq (plci);
        dbug(1,dprintf("chs=%d/%d",plci->channels,
          (plci->channels && ((plci->channels > MAX_CHANNELS_PER_PLCI) || !plci->inc_dis_ncci_table[plci->channels - 1]))));
        if (!plci->channels || ((plci->channels <= MAX_CHANNELS_PER_PLCI) && plci->inc_dis_ncci_table[plci->channels - 1]))
        {
          for (i = 0; i < plci->channels; i++)
          {
            dbug(1,dprintf("Missing DISCONNECT_B3_RESP for %08lx",
              (unsigned long)(UnMapId (Id | (((dword)(plci->inc_dis_ncci_table[i])) << 16)))));
            ncci_free_receive_buffers (appl, plci, plci->inc_dis_ncci_table[i]);
          }
          plci_remove(plci);
          return 0;
        }
      }
      else
        trcq (plci);
    }
    else
      trcq (plci);
  }
  return 0;
}

byte listen_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word Info;
  word i;

  dbug(1,dprintf("listen_req(Appl=0x%x)",appl->Id));

  Info = _WRONG_IDENTIFIER;
  if(a != 0 && (a->vscapi_mode == 0 || (appl->access & DIVA_CAPI_APPL_VSCAPI_ACCESS) != 0)) {
    Info = 0;
    a->Info_Mask[appl->Id-1] = READ_DWORD(parms[0].info);
    a->CIP_Mask[appl->Id-1] = READ_DWORD(parms[1].info);
    dbug(1,dprintf("CIP_MASK=0x%lx", (unsigned long)(a->CIP_Mask[appl->Id-1])));
    if (a->Info_Mask[appl->Id-1] & 0x200){ /* early B3 connect provides */
      a->Info_Mask[appl->Id-1] |=  0x10;   /* call progression infos    */
    }

    /* check if external controller listen and switch listen on or off*/
    if(Id&EXT_CONTROLLER && READ_DWORD(parms[1].info)){
      if(a->profile.Global_Options & GL_EXTERNAL_EQUIPMENT_SUPPORTED) {
        dummy_plci.State = IDLE;
        a->codec_listen[appl->Id-1] = &dummy_plci;
        a->TelOAD[0] = (byte)(parms[3].length);
        for(i=1;parms[3].length>=i && i<22;i++) {
          a->TelOAD[i] = parms[3].info[i];
        }
        a->TelOAD[i] = 0;
        a->TelOSA[0] = (byte)(parms[4].length);
        for(i=1;parms[4].length>=i && i<22;i++) {
          a->TelOSA[i] = parms[4].info[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info = 0x2002; /* wrong controller, codec not supported */
    }
    else{               /* clear listen */
      a->codec_listen[appl->Id-1] = (PLCI   *)0;
    }
  }
  sendf(appl,
        _LISTEN_R|CONFIRM,
        Id,
        Number,
        "w",Info);

  if (a) listen_check(a);
  return FALSE;
}

byte info_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word i;
  API_PARSE * ai;
  PLCI   * rc_plci = 0;
    API_PARSE ai_parms[6];
  word Info = 0;
  word sendingcomplete=0;

  dbug(1,dprintf("info_req"));
  for(i=0;i<6;i++) ai_parms[i].length = 0;

  ai = &msg[1];

  if(ai->length)
  {
    if(api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("AddInfo wrong"));
      Info = _WRONG_MESSAGE_FORMAT;
    }
    else
    {
      /* additionally search for sending complete */
      if(!api_parse(&ai->info[1],(word)ai->length,"sssss",ai_parms))
      {
        if(ai_parms[4].length>=2) sendingcomplete=READ_WORD(&ai_parms[4].info[1]);
      }
    }
  }
  if(!a) Info = _WRONG_STATE;

  if(!Info && plci)
  {                /* no fac, with CPN, or KEY */
    rc_plci = plci;
    if(!ai_parms[3].length && plci->State && (msg[0].length || ai_parms[1].length) )
    {
      /* overlap sending option */
      dbug(1,dprintf("OvlSnd"));
      if(sendingcomplete==1) add_p(plci,SDNCMPL,0);
      add_s(plci,CPN,&msg[0]);
      add_s(plci,KEY,&ai_parms[1]);
      sig_req(plci,INFO_REQ,0);
      send_req(plci);
      return FALSE;
    }

    if(plci->State && ai_parms[2].length)
    {
      /* if the Application sends this request in case of incoming call then it gets the call */
      if((plci->State==INC_CON_PENDING) || (plci->State==INC_CON_ALERT))
      {
        plci->State = (plci->State==INC_CON_ALERT) ? INC_CON_CONNECTED_ALERT : INC_CON_CONNECTED_PEND;
        trcq (plci);
        assign_incoming_call (plci, appl, TRUE); /* disconnect the other appls its quasi a connect */
      }
      /* User_Info option */
      dbug(1,dprintf("UUI"));
      add_s(plci,UUI,&ai_parms[2]);
      sig_req(plci,USER_DATA,0);
    }
    else if(plci->State && ai_parms[3].length)
    {
      /* if the Application sends this request in case of incoming call then it gets the call */
      if((plci->State==INC_CON_PENDING) || (plci->State==INC_CON_ALERT))
      {
        plci->State = (plci->State==INC_CON_ALERT) ? INC_CON_CONNECTED_ALERT : INC_CON_CONNECTED_PEND;
        trcq (plci);
        assign_incoming_call (plci, appl, TRUE); /* disconnect the other appls its quasi a connect */
      }
      /* Facility option */
      dbug(1,dprintf("FAC"));
      add_s(plci,CPN,&msg[0]);
      add_ai(plci, &msg[1]);
      sig_req(plci,FACILITY_REQ,0);
    }
    else
    {
      Info = _WRONG_STATE;
    }
  }
  else if((ai_parms[1].length || ai_parms[2].length || ai_parms[3].length) && !Info)
  {
    /* NCR_Facility option -> send UUI and Keypad too */
    dbug(1,dprintf("NCR_FAC"));
    if((i=get_plci(a)))
    {
      rc_plci = &a->plci[i-1];
      appl->NullCREnable  = TRUE;
      rc_plci->internal_command = C_NCR_FAC_REQ;
      rc_plci->appl = appl;
      add_p(rc_plci,CAI,"\x01\x80");
      add_p(rc_plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
      sig_req(rc_plci,ASSIGN,DSIG_ID);
      send_req(rc_plci);
    }
    else
    {
      Info = _OUT_OF_PLCI;
    }

    if(!Info)
    {
      add_s(rc_plci,CPN,&msg[0]);
      add_ai(rc_plci, &msg[1]);
      sig_req(rc_plci,NCR_FACILITY,0);
      send_req(rc_plci);
      return FALSE;
     /* for application controlled supplementary services    */
    }
  }

  if (!rc_plci)
  {
    Info = _WRONG_MESSAGE_FORMAT;
  }

  if(!Info)
  {
    send_req(rc_plci);
  }
  else
  {  /* appl is not assigned to a PLCI or error condition */
    dbug(1,dprintf("localInfoCon"));
    sendf(appl,
          _INFO_R|CONFIRM,
          Id,
          Number,
          "w",Info);
  }
  return FALSE;
}

byte info_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  dbug(1,dprintf("info_res"));
  return FALSE;
}

byte alert_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word Info,i;
  byte ret;
  API_PARSE * ai;
    API_PARSE ai_parms[6];
  word sendingcomplete=0;

  dbug(1,dprintf("alert_req"));

  for(i=0;i<6;i++) ai_parms[i].length = 0;
  Info = _WRONG_IDENTIFIER;
  ret = FALSE;
  if(plci) {
    Info = _WRONG_STATE;
    if((plci->State==INC_CON_PENDING) || (plci->State==INC_CON_CONNECTED_PEND)) {
      Info = 0;
      ai = &msg[0];
      if(ai->length)
      {
        if(!api_parse(&ai->info[1],(word)ai->length,"sssss",ai_parms))
        {
          if(ai_parms[4].length>=2) sendingcomplete=READ_WORD(&ai_parms[4].info[1]);
        }
        else if(api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
        {
          dbug(1,dprintf("AddInfo wrong"));
          Info = _WRONG_MESSAGE_FORMAT;
        }
      }
      if(!Info)
      {
        if(sendingcomplete==1)
        {
          add_ai(plci, &msg[0]);
          sig_req(plci,CALL_COMPLETE,0);
        }
        else
        {
          plci->State = (plci->State==INC_CON_PENDING) ? INC_CON_ALERT : INC_CON_CONNECTED_ALERT;
          add_ai(plci, &msg[0]);
          sig_req(plci,CALL_ALERT,0);
        }
        ret = 1;
      }
    }
    else if ((plci->State==INC_CON_ALERT) || (plci->State==INC_CON_CONNECTED_ALERT)) {
      Info = _ALERT_IGNORED;
    }
    else if (plci->appl && (plci->appl != appl)) {
      return ret;
    }
  }
  sendf(appl,
        _ALERT_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return ret;
}


static word enqueue_resend_busy_rplci (PLCI   * plci, PLCI   * rplci, APPL   * appl, API_PARSE * msg)
{
  word i;
  static CAPI_MSG ftyreq_msg;

  /* create the new capi message */
  ftyreq_msg.header.length = sizeof(CAPI_MSG_HEADER)+msg[1].length+10;
  ftyreq_msg.header.appl_id = appl->Id;
  ftyreq_msg.header.command = _MANUFACTURER_R;
  ftyreq_msg.header.number = plci->number;
  ftyreq_msg.header.controller = rplci->adapter->Id;
  ftyreq_msg.header.plci = rplci->Id;
  ftyreq_msg.header.ncci = 0;
  WRITE_DWORD(&ftyreq_msg.info.b[0],_DI_MANU_ID);
  WRITE_WORD(&ftyreq_msg.info.b[4],_DI_RESEND_BUSY_RPLCI);
  ftyreq_msg.info.b[6] = (byte)(msg[1].length+3);
  WRITE_WORD(&ftyreq_msg.info.b[7],READ_WORD(msg[0].info)); //fty selector
  ftyreq_msg.info.b[9] = (byte)(msg[1].length);
  for(i=0;i<msg[1].length;i++)
    ftyreq_msg.info.b[10+i]=msg[1].info[1+i];
  WRITE_DWORD(&(ftyreq_msg.info.b[10+3]),(dword)((plci->Id << 8) | UnMapController (plci->adapter->Id)));
  return (api_put_reenter (appl, &ftyreq_msg));
}


byte facility_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word Info = 0;
  word i    = 0;

  word selector;
  word SSreq;
  dword relatedPLCIvalue;
  DIVA_CAPI_ADAPTER   * relatedadapter;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x00\x00\x02\x00\x00";
    byte SSstruct[] = "\x09\x00\x00\x06\x00\x00\x00\x00\x00\x00";
  API_PARSE * parms;
  static API_PARSE ss_parms[16];
  PLCI   *rplci;
    byte cai[15];
    byte nothing[1];
  dword d;
    API_PARSE dummy;
  word w;

  dbug(1,dprintf("facility_req"));
  for(i=0;i<16;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    dbug(1,dprintf("wrong Ctrl"));
    Info = _WRONG_IDENTIFIER;
  }

  selector = READ_WORD(msg[0].info);

  if(!Info)
  {
    switch(selector)
    {
      case SELECTOR_HANDSET:
        Info = AdvCodecSupport(a, plci, appl, HOOK_SUPPORT);
        break;

      case SELECTOR_SU_SERV:
        if(!msg[1].length)
        {
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        SSreq = READ_WORD(&(msg[1].info[1]));
        WRITE_WORD(&RCparms[1],SSreq);
        SSparms = RCparms;
        switch(SSreq)
        {
          case S_GET_SUPPORTED_SERVICES:
            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              rplci->appl = appl;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              WRITE_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
              SSparms = (byte *)SSstruct;
              break;
            }
            rplci->internal_command = GETSERV_REQ_PEND;
            rplci->number = Number;
            rplci->appl = appl;
            sig_req(rplci,S_SUPPORTED,0);
            send_req(rplci);
            return FALSE;
            break;

          case S_LISTEN:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            a->Notification_Mask[appl->Id-1] = READ_DWORD(ss_parms[2].info);
            if(a->Notification_Mask[appl->Id-1] & SMASK_MWI) /* MWI active? */
            {
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                break;
              }
              rplci->internal_command = GET_MWI_STATE;
              rplci->number = Number;
              sig_req(rplci,MWI_POLL,0);
              send_req(rplci);
            }
            break;

          case S_HOLD:
            api_parse(&parms->info[1],(word)parms->length,"ws",ss_parms);
            if(plci && plci->State && plci->SuppState==IDLE)
            {
              plci->SuppState = HOLD_REQUEST;
              plci->command = C_HOLD_REQ;
              add_s(plci,CAI,&ss_parms[1]);
              sig_req(plci,CALL_HOLD,0);
              send_req(plci);
              return FALSE;
            }
            else Info = 0x3010;                    /* wrong state           */
            break;
          case S_RETRIEVE:
            if(plci && plci->State)               /* call could be in auto-hold state on the card */
            {                                     /* so don't check if call is on hold */
              if(Id & EXT_CONTROLLER)
              {
                if(AdvCodecSupport(a, plci, appl, 0))
                {
                  Info = 0x3010;                    /* wrong state           */
                  break;
                }
              }
              else plci->tel = 0;

              if(!plci->appl || plci->State == INC_CON_ALERT) /* once a retrieve_rej happens need to re-try, but appl is already present */
              {
                dbug(1,dprintf("Retrieve_Req no Appl"));
                if(plci->State != INC_CON_ALERT)
                {
                  dbug(1,dprintf("expect Alerting state"));
                  Info = 0x3010;                    /* wrong state           */
                  break;
                }
                plci->SuppState = BLIND_RETRIEVE_REQUEST;
                assign_incoming_call (plci, appl, TRUE); /* application does retrieve in ringing state, select_b can follow or connect_Res */
                plci->State = INC_CON_BLIND_RETRIEVE;
                trcq (plci);
              }
       else plci->SuppState = RETRIEVE_REQUEST;
              plci->command = C_RETRIEVE_REQ;
              if(plci->spoofed_msg==SPOOFING_REQUIRED)
              {
                plci->spoofed_msg = CALL_RETRIEVE;
                plci->internal_command = BLOCK_PLCI;
                plci->command = 0;
                dbug(1,dprintf("Spoof"));
                return FALSE;
              }
              else
              {
                sig_req(plci,CALL_RETRIEVE,0);
                send_req(plci);
                return FALSE;
              }
            }
            else Info = 0x3010;                    /* wrong state           */
            break;
          case S_SUSPEND:
            if(parms->length)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            if(plci && plci->State)
            {
              add_s(plci,CAI,&ss_parms[2]);
              plci->command = SUSPEND_REQ;
              sig_req(plci,SUSPEND,0);
              plci->State = SUSPENDING;
              send_req(plci);
            }
            else Info = 0x3010;                    /* wrong state           */
            break;

          case S_RESUME:
            if(parms->length)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            if(!(i=get_plci(a)) )
            {
              Info = _OUT_OF_PLCI;
              break;
            }
            rplci = &a->plci[i-1];
            rplci->appl = appl;
            rplci->number = Number;
            rplci->tel = 0;
            rplci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
            /* check 'external controller' bit for codec support */
            if(Id & EXT_CONTROLLER)
            {
              if(AdvCodecSupport(a, rplci, appl, 0) )
              {
                free_plci (rplci);
                Info = 0x300A;
                break;
              }
            }
            dummy.length = 0;
            dummy.info = "\x00";
            add_b1(rplci, &dummy, 0, 0);
            if (a->Info_Mask[appl->Id-1] & 0x200)
            {
              /* early B3 connect (CIP mask bit 9) no release after a disc */
              add_p(rplci,LLI,"\x01\x01");
            }
            add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
            sig_req(rplci,ASSIGN,DSIG_ID);
            send_req(rplci);
            add_s(rplci,CAI,&ss_parms[2]);
            rplci->command = RESUME_REQ;
            sig_req(rplci,RESUME,0);
            rplci->State = RESUMING;
            send_req(rplci);
            break;

          case S_CONF_BEGIN: /* Request */
          case S_CONF_DROP:
          case S_CONF_ISOLATE:
          case S_CONF_REATTACH:
            if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(plci && plci->State && ((plci->SuppState==IDLE)||(plci->SuppState==CALL_HELD)))
            {
              d = READ_DWORD(ss_parms[2].info);
              if(d>=0x80)
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
              plci->ptyState = (byte)SSreq;
              plci->ptyAppl = appl;
              plci->command = 0;
              cai[0] = 2;
              switch(SSreq)
              {
              case S_CONF_BEGIN:
                  cai[1] = CONF_BEGIN;
                  plci->internal_command = CONF_BEGIN_REQ_PEND;
                  break;
              case S_CONF_DROP:
                  cai[1] = CONF_DROP;
                  plci->internal_command = CONF_DROP_REQ_PEND;
                  break;
              case S_CONF_ISOLATE:
                  cai[1] = CONF_ISOLATE;
                  plci->internal_command = CONF_ISOLATE_REQ_PEND;
                  break;
              case S_CONF_REATTACH:
                  cai[1] = CONF_REATTACH;
                  plci->internal_command = CONF_REATTACH_REQ_PEND;
                  break;
              }
              cai[2] = (byte)d; /* Conference Size resp. PartyId */
              add_p(plci,CAI,cai);
              sig_req(plci,S_SERVICE,0);
              send_req(plci);
              return FALSE;
            }
            else Info = 0x3010;                    /* wrong state           */
            break;

          case S_ECT:
          case S_3PTY_BEGIN:
          case S_3PTY_END:
          case S_CONF_ADD:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else if(parms->length==8) /* workaround for the T-View-S */
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbdb",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!msg[1].length)
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if (!plci)
            {
              Info = _WRONG_IDENTIFIER;
              break;
            }
            relatedPLCIvalue = READ_DWORD(ss_parms[2].info);
            relatedPLCIvalue &= 0x0000FFFF;
            dbug(1,dprintf("PTY/ECT/addCONF,relPLCI=%lx", (unsigned long) relatedPLCIvalue));
            /* controller starts with 0 up to (max_adapter - 1) */
            if (((relatedPLCIvalue & 0x7f) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) > max_adapter))
            {
              if(SSreq==S_3PTY_END)
              {
                dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCI"));
                rplci = plci;
              }
              else
              {
                Info = 0x3010;                    /* wrong state           */
                break;
              }
            }
            else
            {
              relatedadapter = &adapter[MapController ((byte)(relatedPLCIvalue & 0x7f))-1];
              relatedPLCIvalue >>=8;
              /* find PLCI PTR*/
              rplci = &relatedadapter->plci[relatedPLCIvalue-1];
              if (relatedPLCIvalue==0 || relatedPLCIvalue>relatedadapter->max_plci
               || !rplci->Id)
              {
                if(SSreq==S_3PTY_END)
                {
                  dbug(1, dprintf("use 2nd PLCI=PLCI"));
                  rplci = plci;
                }
                else
                {
                  Info = 0x3010;                    /* wrong state           */
                  break;
                }
              }
            }
/*
            dbug(1,dprintf("rplci:%x",rplci));
            dbug(1,dprintf("plci:%x",plci));
            dbug(1,dprintf("rplci->ptyState:%x",rplci->ptyState));
            dbug(1,dprintf("plci->ptyState:%x",plci->ptyState));
            dbug(1,dprintf("SSreq:%x",SSreq));
            dbug(1,dprintf("rplci->internal_command:%x",rplci->internal_command));
            dbug(1,dprintf("rplci->appl:%x",rplci->appl));
            dbug(1,dprintf("rplci->Id:%x",rplci->Id));
*/
            if( (rplci->req_in || rplci->internal_command) && rplci->appl )
            { /* if there is an internal_command then we have to requeue it until this busy condition
              is solved */
              if (!enqueue_resend_busy_rplci (plci, rplci, appl, msg)) return FALSE;
              /* if it returns with an error then we have no chance -> end up the request with an error */
            }
            /* send PTY/ECT req, cannot check all states because of US stuff */
            if( !(rplci->req_in || rplci->internal_command) && rplci->appl )
            {
              plci->command = 0;
              rplci->relatedPTYPLCI = plci;
              plci->relatedPTYPLCI = rplci;
              rplci->ptyState = (byte)SSreq;
              rplci->ptyAppl = appl;
              if(SSreq==S_ECT)
              {
                rplci->internal_command = ECT_REQ_PEND;
                cai[1] = ECT_EXECUTE;

                rplci->vswitchstate=0;
                rplci->vsprot=0;
                rplci->vsprotdialect=0;
                plci->vswitchstate=0;
                plci->vsprot=0;
                plci->vsprotdialect=0;

              }
              else if(SSreq==S_CONF_ADD)
              {
                rplci->internal_command = CONF_ADD_REQ_PEND;
                cai[1] = CONF_ADD;
              }
              else
              {
                rplci->internal_command = PTY_REQ_PEND;
                cai[1] = (byte)(SSreq-3);
              }
              rplci->number = Number;
              if(plci!=rplci) /* explicit invocation */
              {
                cai[0] = 2;
                cai[2] = plci->Sig.Id;
                dbug(1,dprintf("explicit invocation"));
                rplci->waitforECTindOnRPlci=1;
                /* this plci (the current/consultationcall) has to RNR/delay an incomming Hangup until the FaciliteInd
                S_SERVICE_REJ or S_SERVICE with ECT_EXECUTE comes on the related PLCI or a timeout fires */
                plci->waitforECTindOnRPlci=2;
              }
              else
              {
                dbug(1,dprintf("implicit invocation"));
                cai[0] = 1;
              }
              add_p(rplci,CAI,cai);
              {
                byte tmp[sizeof(void*)+1];
                void *p = &plci->Sig;

                tmp[0] = sizeof(void*);
                memcpy (&tmp[1], &p, sizeof(void*));
                add_p(rplci,CAI,tmp);
              }
              sig_req(rplci,S_SERVICE,0);
              send_req(rplci);
              return FALSE;
            }
            else
            {
              dbug(0,dprintf("Wrong line"));
              Info = 0x3010;                    /* wrong state           */
              break;
            }
            break;

          case S_CALL_DEFLECTION:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if (!plci)
            {
              Info = _WRONG_IDENTIFIER;
              break;
            }
            /* reuse unused screening indicator */
            ss_parms[3].info[3] = (byte)READ_WORD(&(ss_parms[2].info[0]));
            plci->command = 0;
            plci->internal_command = CD_REQ_PEND;
            if(!plci->appl) appl->CDEnable = TRUE;
            cai[0] = 1;
            cai[1] = CALL_DEFLECTION;
            add_p(plci,CAI,cai);
            add_p(plci,CPN,ss_parms[3].info);
            sig_req(plci,S_SERVICE,0);
            send_req(plci);
            return FALSE;
            break;

          case S_CALL_FORWARDING_START:
            /* new parameter for QSIG */
            if(api_parse(&parms->info[1],(word)parms->length,"wbdwwssss",ss_parms))
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbdwwsss",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              rplci->appl = appl;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              Info = _OUT_OF_PLCI;
              break;
            }

            rplci->internal_command = CF_START_PEND;
            rplci->appl = appl;
            rplci->number = Number;
            appl->S_Handle = READ_DWORD(&(ss_parms[2].info[0]));
            cai[0] = 2;
            cai[1] = 0x70|(byte)READ_WORD(&(ss_parms[3].info[0])); /* Function */
            cai[2] = (byte)READ_WORD(&(ss_parms[4].info[0])); /* Basic Service */
            add_p(rplci,CAI,cai);
            add_p(rplci,OAD,ss_parms[5].info);
            add_p(rplci,CPN,ss_parms[6].info);
            if(ss_parms[8].info) add_p(rplci,OSA,ss_parms[8].info);
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return FALSE;
            break;

          case S_INTERROGATE_NUMBERS:
          case S_CCBS_REQUEST:
          case S_CCBS_DEACTIVATE:
          case S_CCBS_INTERROGATE:
          case S_CALL_FORWARDING_STOP:
          case S_INTERROGATE_DIVERSION:
          case S_CCNR_REQUEST:
          case S_CCNR_INTERROGATE:
            switch(SSreq)
            {
            case S_INTERROGATE_NUMBERS:
                if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
            case S_CCNR_REQUEST:
                if(api_parse(&parms->info[1],(word)parms->length,"wbdw",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CCBS_INTERROGATE:
            case S_CCNR_INTERROGATE:
                if(api_parse(&parms->info[1],(word)parms->length,"wbdws",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CALL_FORWARDING_STOP:
            case S_INTERROGATE_DIVERSION:
                /* new parameter for QSIG */
                if(api_parse(&parms->info[1],(word)parms->length,"wbdwwss",ss_parms))
                {
                    if(api_parse(&parms->info[1],(word)parms->length,"wbdwws",ss_parms))
                    {
                      dbug(0,dprintf("format wrong"));
                      Info = _WRONG_MESSAGE_FORMAT;
                      break;
                    }
                }
                break;
            }

            if(Info) break;
            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              switch(SSreq)
              {
                case S_INTERROGATE_DIVERSION: /* use cai with S_SERVICE below */
                  cai[1] = 0x60|(byte)READ_WORD(&(ss_parms[3].info[0])); /* Function */
                  rplci->internal_command = INTERR_DIVERSION_REQ_PEND; /* move to rplci if assigned */
                  break;
                case S_INTERROGATE_NUMBERS: /* use cai with S_SERVICE below */
                  cai[1] = DIVERSION_INTERROGATE_NUM; /* Function */
                  rplci->internal_command = INTERR_NUMBERS_REQ_PEND; /* move to rplci if assigned */
                  break;
                case S_CALL_FORWARDING_STOP:
                  rplci->internal_command = CF_STOP_PEND;
                  cai[1] = 0x80|(byte)READ_WORD(&(ss_parms[3].info[0])); /* Function */
                  break;
                case S_CCBS_REQUEST:
                  cai[1] = CCBS_REQUEST;
                  rplci->internal_command = CCBS_REQUEST_REQ_PEND;
                  break;
                case S_CCNR_REQUEST:
                  cai[1] = CCNR_REQUEST;
                  rplci->internal_command = CCNR_REQUEST_REQ_PEND;
                  break;
                case S_CCBS_DEACTIVATE:
                  cai[1] = CCBS_DEACTIVATE;
                  rplci->internal_command = CCBS_DEACTIVATE_REQ_PEND;
                  break;
                case S_CCBS_INTERROGATE:
                  cai[1] = CCBS_INTERROGATE;
                  rplci->internal_command = CCBS_INTERROGATE_REQ_PEND;
                  break;
                case S_CCNR_INTERROGATE:
                  cai[1] = CCNR_INTERROGATE;
                  rplci->internal_command = CCNR_INTERROGATE_REQ_PEND;
                  break;
                default:
                  cai[1] = 0;
                break;
              }
              rplci->appl = appl;
              rplci->number = Number;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              Info = _OUT_OF_PLCI;
              break;
            }

            appl->S_Handle = READ_DWORD(&(ss_parms[2].info[0]));
            switch(SSreq)
            {
            case S_INTERROGATE_NUMBERS:
                cai[0] = 1;
                add_p(rplci,CAI,cai);
                break;
            case S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
            case S_CCNR_REQUEST:
                cai[0] = 3;
                WRITE_WORD(&cai[2],READ_WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                break;
            case S_CCBS_INTERROGATE:
            case S_CCNR_INTERROGATE:
                cai[0] = 3;
                WRITE_WORD(&cai[2],READ_WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                add_p(rplci,OAD,ss_parms[4].info);
                break;
            default:
                cai[0] = 2;
                cai[2] = (byte)READ_WORD(&(ss_parms[4].info[0])); /* Basic Service */
                add_p(rplci,CAI,cai);
                add_p(rplci,OAD,ss_parms[5].info);
                if(ss_parms[6].info) add_p(rplci,OSA,ss_parms[6].info);
                break;
            }

            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return FALSE;
            break;

          case S_CCBS_CALL:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwwwsssss",ss_parms))
            {
              dbug(0,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            ss_parms[0].length=ss_parms[3].length; /* CIP value */
            ss_parms[0].info=ss_parms[3].info;
            cai[0]=7;
            cai[1]='C';
            cai[2]='C';
            cai[3]='B';
            cai[4]='S';
            cai[5]=2;
            w=READ_WORD(&ss_parms[2].info[0]); /* CCBS Reference */
            if(w>127) /* 0..127 */
            {
              dbug(0,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            cai[6]=w&0xff;
            cai[7]=w>>8;
            ss_parms[1].length=7;                  /* CPN */
            ss_parms[1].info=cai;
            nothing[0]=0;
            ss_parms[2].length=0;                  /* OAD */
            ss_parms[2].info=nothing;
            ss_parms[3].length=0;                  /* DSA */
            ss_parms[3].info=nothing;
            ss_parms[4].length=0;                  /* OSA */
            ss_parms[4].info=nothing;
            ss_parms[5].length=ss_parms[5].length; /* B-Protocol */
            ss_parms[5].info=ss_parms[5].info;
            ss_parms[6].length=ss_parms[6].length; /* BC */
            ss_parms[6].info=ss_parms[6].info;
            ss_parms[7].length=ss_parms[7].length; /* LLC */
            ss_parms[7].info=ss_parms[7].info;
            ss_parms[8].length=ss_parms[8].length; /* HLC */
            ss_parms[8].info=ss_parms[8].info;
            ss_parms[9].length=ss_parms[9].length; /* AI */
            ss_parms[9].info=ss_parms[9].info;
            connect_req(Id,Number,a,plci,appl,ss_parms);
            return FALSE;

          case S_MWI_ACTIVATE:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwdwwwssssd",ss_parms)){
                if(api_parse(&parms->info[1],(word)parms->length,"wbwdwwwssss",ss_parms))
                {
                  dbug(1,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                  break;
                }
            }
            if(!plci)
            {
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                rplci->cr_enquiry=TRUE;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                Info = _OUT_OF_PLCI;
                break;
              }
            }
            else
            {
              trcq (plci);
              rplci = plci;
              rplci->cr_enquiry=FALSE;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_ACTIVATE_REQ_PEND;
            rplci->appl = appl;
            rplci->number = Number;

            if(ss_parms[11].info){
                rplci->S_Handle = READ_DWORD(&(ss_parms[11].info[0]));
                rplci->gothandle=1;
            }
            else rplci->gothandle=0;
            cai[0] = 13;
            cai[1] = ACTIVATION_MWI; /* Function */
            WRITE_WORD(&cai[2],READ_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            WRITE_DWORD(&cai[4],READ_DWORD(&(ss_parms[3].info[0]))); /* Number of Messages */
            WRITE_WORD(&cai[8],READ_WORD(&(ss_parms[4].info[0]))); /* Message Status */
            WRITE_WORD(&cai[10],READ_WORD(&(ss_parms[5].info[0]))); /* Message Reference */
            WRITE_WORD(&cai[12],READ_WORD(&(ss_parms[6].info[0]))); /* Invocation Mode */
            add_p(rplci,CAI,cai);
            add_p(rplci,CPN,ss_parms[7].info); /* Receiving User Number */
            add_p(rplci,OAD,ss_parms[8].info); /* Controlling User Number */
            add_p(rplci,OSA,ss_parms[9].info); /* Controlling User Provided Number */
            add_p(rplci,UID,ss_parms[10].info); /* Time */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return FALSE;

          case S_MWI_DEACTIVATE:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwwssd",ss_parms)){
                if(api_parse(&parms->info[1],(word)parms->length,"wbwwss",ss_parms))
                {
                  dbug(1,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                  break;
                }
            }
            if(!plci)
            {
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                rplci->cr_enquiry=TRUE;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                Info = _OUT_OF_PLCI;
                break;
              }
            }
            else
            {
              trcq (plci);
              rplci = plci;
              rplci->cr_enquiry=FALSE;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_DEACTIVATE_REQ_PEND;
            rplci->appl = appl;
            rplci->number = Number;

            if(ss_parms[6].info){
                rplci->S_Handle = READ_DWORD(&(ss_parms[6].info[0]));
                rplci->gothandle=1;
            }
            else rplci->gothandle=0;
            cai[0] = 5;
            cai[1] = DEACTIVATION_MWI; /* Function */
            WRITE_WORD(&cai[2],READ_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            WRITE_WORD(&cai[4],READ_WORD(&(ss_parms[3].info[0]))); /* Invocation Mode */
            add_p(rplci,CAI,cai);
            add_p(rplci,CPN,ss_parms[4].info); /* Receiving User Number */
            add_p(rplci,OAD,ss_parms[5].info); /* Controlling User Number */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return FALSE;

          default:
            Info = 0x300E;  /* not supported */
            break;
        }
        break; /* case SELECTOR_SU_SERV: end */



      case PRIV_SELECTOR_DTMF_ONDATA:

      case SELECTOR_DTMF:
        return (dtmf_request (Id, Number, a, plci, appl, msg));



      case SELECTOR_LINE_INTERCONNECT:
        return (mixer_request (Id, Number, a, plci, appl, msg));



      case PRIV_SELECTOR_ECHO_CANCELLER:
        appl->appl_flags = (appl->appl_flags & ~APPL_FLAG_OLD_EC_SELECTOR) | APPL_FLAG_PRIV_EC_SPEC;
        return (ec_request (Id, Number, a, plci, appl, msg));

      case SELECTOR_ECHO_CANCELLER_OLD:
        appl->appl_flags = (appl->appl_flags & ~APPL_FLAG_PRIV_EC_SPEC) | APPL_FLAG_OLD_EC_SELECTOR;
        return (ec_request (Id, Number, a, plci, appl, msg));

      case SELECTOR_ECHO_CANCELLER_NEW:
        appl->appl_flags &= ~(APPL_FLAG_PRIV_EC_SPEC | APPL_FLAG_OLD_EC_SELECTOR);
        return (ec_request (Id, Number, a, plci, appl, msg));



      case PRIV_SELECTOR_RTP:
        return (rtp_request (Id, Number, a, plci, appl, msg));



      case PRIV_SELECTOR_T38:
        return (t38_request (Id, Number, a, plci, appl, msg));


      case PRIV_SELECTOR_CONNECT_SUPPORT:
        return (connect_support_request (Id, Number, a, plci, appl, msg));

      case PRIV_SELECTOR_RESOURCE_RESERVATION:
        return (resource_reservation_request (Id, Number, a, plci, appl, msg));

      case SELECTOR_V42BIS:
      default:
        Info = _FACILITY_NOT_SUPPORTED;
        break;
    } /* end of switch(selector) */
  }

  dbug(1,dprintf("SendFacRc"));
  sendf(appl,
        _FACILITY_R|CONFIRM,
        Id,
        Number,
        "wws",Info,selector,SSparms);
  return FALSE;
}

byte facility_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word Info = 0;
  word i    = 0;

  word selector;
  word SSreq;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x00\x00\x02\x00\x00";
  API_PARSE * parms;
    API_PARSE ss_parms[11];
  PLCI   *rplci;
    byte cai[15];

  dbug(1,dprintf("facility_res"));
  for(i=0;i<9;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    dbug(1,dprintf("wrong Ctrl"));
    Info = _WRONG_IDENTIFIER;
  }

  selector = READ_WORD(msg[0].info);

  if(!Info)
  {
    switch(selector)
    {
    case SELECTOR_SU_SERV:
        if(!msg[1].length)
        {
            Info = _WRONG_MESSAGE_FORMAT;
            break;
        }
        SSreq = READ_WORD(&(msg[1].info[1]));
        WRITE_WORD(&RCparms[1],SSreq);
        SSparms = RCparms;
        switch(SSreq)
        {
        case S_CCBS_STATUS:
            if(api_parse(&parms->info[1],(word)parms->length,"wbw",ss_parms))
            {
              dbug(0,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            /* find related PLCI */
            for(i=0;i<a->max_plci;i++)
            {
                if(!a->plci[i].Id) continue;
                if(a->plci[i].ptyState==CCBS_STATUS) break;
            }
            if(i==a->max_plci) {
              dbug(1,dprintf("CCBS_STATUS resp: PLCI not found"));
              break;
            }
            rplci = &a->plci[i];
            rplci->ptyState=0;
            cai[0] = 3;
            cai[1] = CCBS_STATUS; /* Function */
            WRITE_WORD(&cai[2],READ_WORD(&(ss_parms[2].info[0])));
            add_p(rplci,CAI,cai);
            rplci->command = 0;
            rplci->internal_command = 0; /* no interest on response */
            add_p(rplci,ESC,rplci->ncrl_esccai);
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            break;
        }
        break;
    }
  }
  return FALSE;
}

byte connect_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word Info = 0;
  byte req;
  byte len;
  word i, w;
  word fax_control_bits, fax_control2_bits, fax_info_change;
    API_PARSE bp[2];
    API_PARSE bp_parms[8];
    API_PARSE b3_config_parms[6];
  API_PARSE * ncpi;
    byte pvc[2];

    API_PARSE fax_parms[9];


  dbug(1,dprintf("connect_b3_req"));
  if(plci)
  {
    if ((plci->State == IDLE) || (plci->State == OUTG_DIS_PENDING)
     || (plci->State == INC_DIS_PENDING) || (plci->SuppState != IDLE))
    {
      Info = _WRONG_STATE;
    }
    else
    {
      /* local reply if assign unsuccessfull
         or B3 protocol allows only one layer 3 connection
           and already connected
             or B2 protocol not any LAPD
               and connect_b3_req contradicts originate/answer direction */
      if (!plci->NL.Id
       || (((plci->B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE))
        && ((plci->channels != 0)
         || (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_LAPD_FREE_SAPI_SEL)
           && (plci->B2_prot != B2_X75) && (plci->B2_prot != B2_X75_V42BIS))
          && ((plci->call_dir & CALL_DIR_ANSWER) && !(plci->call_dir & CALL_DIR_FORCE_OUTG_NL))))))
      {
        dbug(1,dprintf("B3 already connected=%d or no NL.Id=0x%x, dir=%d sstate=0x%x",
                       plci->channels,plci->NL.Id,plci->call_dir,plci->SuppState));
        Info = _WRONG_STATE;

        if (!plci->NL.Id
         && plci->appl
         && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
           & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
        {
          if (plci->nl_assign_rc == (ASSIGN_RC | WRONG_CH))
            Info = INFO_RELATED_RESOURCE_ERROR;
        }

        sendf(appl,
              _CONNECT_B3_R|CONFIRM,
              Id,
              Number,
              "w",Info);
        return FALSE;
      }
      plci->requested_options_conn = 0;
      plci->call_dir |= CALL_DIR_OUTG_NL;

      req = N_CONNECT;
      ncpi = &parms[0];
      if(plci->B3_prot==B3_ISO8208 || plci->B3_prot==B3_X25_DCE)
      {
        if(ncpi->length>2)
        {
          /* check for PVC */
          if(ncpi->info[2] || ncpi->info[3])
          {
            pvc[0] = ncpi->info[3];
            pvc[1] = ncpi->info[2];
            add_d(plci,2,pvc);
            req = N_RESET;
          }
          else
          {
            if(ncpi->info[1] &1) req = N_CONNECT | N_D_BIT;
            add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
          }
        }
      }
      else if(plci->B3_prot==B3_T30_WITH_EXTENSIONS)
      {
        if (plci->NL.Id && !plci->nl_remove_id)
        {
          fax_control_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low);
          fax_control2_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low);
          if (!(fax_control_bits & T30_CONTROL_BIT_MORE_DOCUMENTS)
           || (plci->fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS))
          {
            len = (byte)(((byte *)(&(((T30_INFO *) 0)->station_id_len))) - ((byte *) 0));
            fax_info_change = FALSE;
            if (ncpi->length >= 4)
            {
              w = READ_WORD(&ncpi->info[3]);
              api_load_msg (&plci->B_protocol, bp);
              if(!api_parse(&bp->info[1], (word)bp->length, "wwwsss", bp_parms))
              {
                if(!api_parse(&bp_parms[5].info[1], (word)bp_parms[5].length, "wwss", b3_config_parms))
                {
                  w |= READ_WORD(b3_config_parms[0].info) & 0x0001;
                }
              }
              i = ((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution |
                (((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution_high << 8);
              if (((w & 0x0001) != 0) != ((i & T30_RESOLUTION_R8_0770_OR_200) != 0))
              {
                i = (i & ~T30_RESOLUTION_R8_0770_OR_200) | ((w & 0x0001) ? T30_RESOLUTION_R8_0770_OR_200 : 0);
                ((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution = (byte) i;
                ((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution_high = (byte)(i >> 8);
                fax_info_change = TRUE;
              }
              fax_control_bits &= ~(T30_CONTROL_BIT_REQUEST_POLLING | T30_CONTROL_BIT_MORE_DOCUMENTS);
              if (w & 0x0002)  /* Fax-polling request */
                fax_control_bits |= T30_CONTROL_BIT_REQUEST_POLLING;
              if ((w & 0x0004) /* Request to send / poll another document */
               && (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS))
              {
                fax_control_bits |= T30_CONTROL_BIT_MORE_DOCUMENTS;
              }
              if (ncpi->length >= 6)
              {
                w = READ_WORD(&ncpi->info[5]);
                if (((byte) w) != ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format)
                {
                  ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format = (byte) w;
                  fax_info_change = TRUE;
                }

                if ((a->profile.Manuf.private_options & (1L << PRIVATE_FAX_SUB_SEP_PWD))
                 && (READ_WORD(&ncpi->info[5]) & 0x8000)) /* Private SEP/SUB/PWD enable */
                {
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_SUB_SEP_PWD);
                }
                if ((a->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
                 && (READ_WORD(&ncpi->info[5]) & 0x4000)) /* Private non-standard facilities enable */
                {
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_NONSTANDARD);
                }
                fax_control_bits &= ~(T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_SEL_POLLING |
                  T30_CONTROL_BIT_ACCEPT_PASSWORD);
                fax_control2_bits &= ~T30_CONTROL2_BIT_REQUEST_PRI;
                if ((a->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
                 && (READ_WORD(&ncpi->info[5]) & 0x0100)) /* Request procedure interrupt */
                {
                  fax_control2_bits |= T30_CONTROL2_BIT_REQUEST_PRI;
                }
                if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                  & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (api_parse (&ncpi->info[1], ncpi->length, "wwwwsb", fax_parms))
                  {
                    fax_parms[5].length = 0;
                    fax_parms[6].length = 0;
                    if (api_parse (&ncpi->info[1], ncpi->length, "wwwws", fax_parms))
                      Info = _WRONG_MESSAGE_FORMAT;
                  }
                  else if (api_parse (&ncpi->info[1], ncpi->length, "wwwwsss", fax_parms))
                    Info = _WRONG_MESSAGE_FORMAT;

                  if (Info == GOOD)
                  {
                    if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVATE_FAX_SUB_SEP_PWD))
                    {
                      fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
                      if (fax_control_bits & T30_CONTROL_BIT_ACCEPT_POLLING)
                        fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SEL_POLLING;
                    }
                    w = fax_parms[4].length;
                    if (w > 20)
                      w = 20;
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->station_id_len = (byte) w;
                    for (i = 0; i < w; i++)
                      ((T30_INFO   *)(plci->fax_connect_info_buffer))->station_id[i] = fax_parms[4].info[1+i];
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->head_line_len = 0;
                    len = (byte)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0));
                    w = fax_parms[5].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[5].info[1+i];
                    w = fax_parms[6].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[6].info[1+i];
                    if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVATE_FAX_NONSTANDARD))
                    {
                      if (api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
                      {
                        dbug(1,dprintf("non-standard facilities info missing or wrong format"));
                        plci->fax_connect_info_buffer[len++] = 0;
                      }
                      else
                      {
                        if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
                          plci->nsf_control_bits = READ_WORD(&fax_parms[7].info[2]);
                        plci->fax_connect_info_buffer[len++] = (byte)(fax_parms[7].length);
                        for (i = 0; i < fax_parms[7].length; i++)
                          plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
                      }
                    }
                  }
                }
                else
                {
                  len = (byte)(((byte *)(&(((T30_INFO *) 0)->station_id_len))) - ((byte *) 0));
                }
                fax_info_change = TRUE;

              }
              if ((fax_control_bits != READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low))
               || (fax_control2_bits != READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low)))
              {
                WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
                WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low, fax_control2_bits);
                fax_info_change = TRUE;
              }
            }
            if (Info == GOOD)
            {
              plci->fax_connect_info_length = len;
              if (fax_info_change)
              {
                if (plci->fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
                {
                  start_internal_command (Id, plci, fax_connect_info_command);
                  return FALSE;
                }
                else
                {
                  start_internal_command (Id, plci, fax_adjust_b23_command);
                  return FALSE;
                }
              }
            }
          }
          else  Info = _WRONG_STATE;
        }
        else  Info = _WRONG_STATE;
      }

      else if ((plci->B3_prot == B3_RTP) && (ncpi->length != 0))
      {
        plci->internal_req_buffer[0] = ncpi->length + 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_buffer[2+w] = ncpi->info[1+w];
        start_internal_command (Id, plci, rtp_connect_b3_req_command);
        return FALSE;
      }

      if(!Info)
      {
        nl_req_ncci(plci,req,0);
        return 1;
      }
    }
  }
  else Info = _WRONG_IDENTIFIER;

  sendf(appl,
        _CONNECT_B3_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return FALSE;
}

byte connect_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ncci;
  API_PARSE * ncpi;
  byte req;

  word w;


    API_PARSE fax_parms[9];
  word i, fax_control_bits, fax_control2_bits;
  byte len;


  dbug(1,dprintf("connect_b3_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    if(a->ncci_state[ncci]==INC_CON_PENDING) {
      if (READ_WORD (&parms[0].info[0]) != 0)
      {
        a->ncci_state[ncci] = OUTG_REJ_PENDING;
        channel_request_xon (plci, a->ncci_ch[ncci]);
        channel_xmit_xon (plci);
        cleanup_ncci_data (plci, ncci);
        nl_req_ncci(plci,N_DISC,ncci);
        return 1;
      }
      a->ncci_state[ncci] = INC_ACT_PENDING;

      req = N_CONNECT_ACK;
      ncpi = &parms[1];
      if ((plci->B3_prot == 4) || (plci->B3_prot == 5) || (plci->B3_prot == 7))
      {

        if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
          & (1L << PRIVATE_FAX_NONSTANDARD))
        {
          if (((plci->B3_prot == 4) || (plci->B3_prot == 5))
           && ((plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP)
            || (ncpi->length >= 9)))
          {
            len = (byte)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0));
            if (plci->fax_connect_info_length < len)
            {
              ((T30_INFO *)(plci->fax_connect_info_buffer))->station_id_len = 0;
              ((T30_INFO *)(plci->fax_connect_info_buffer))->head_line_len = 0;
            }
            if (api_parse (&ncpi->info[1], ncpi->length, "wwwws", fax_parms))
            {
              dbug(1,dprintf("non-standard facilities info missing or wrong format"));
            }
            else
            {
              fax_control_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low);
              fax_control2_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low);
              if (ncpi->length >= 6)
              {
                w = READ_WORD(&ncpi->info[5]);
                if (((byte) w) == 0xfe) /* plain data format */
                {
                  ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format = (byte) w;
                }
                if ((w & 0x0100) /* Request procedure interrupt */
                 && (a->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  fax_control2_bits |= T30_CONTROL2_BIT_REQUEST_PRI;
                }
              }
              WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
              WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low, fax_control2_bits);
              if (!api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
              {
                if (plci->fax_connect_info_length <= len)
                  plci->fax_connect_info_buffer[len] = 0;
                len += 1 + plci->fax_connect_info_buffer[len];
                if (plci->fax_connect_info_length <= len)
                  plci->fax_connect_info_buffer[len] = 0;
                len += 1 + plci->fax_connect_info_buffer[len];
                if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
                  plci->nsf_control_bits = READ_WORD(&fax_parms[7].info[2]);
                plci->fax_connect_info_buffer[len++] = (byte)(fax_parms[7].length);
                for (i = 0; i < fax_parms[7].length; i++)
                  plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
              }
            }
            plci->fax_connect_info_length = len;
            ((T30_INFO *)(plci->fax_connect_info_buffer))->code = 0;
            start_internal_command (Id, plci, fax_connect_ack_command);
            return FALSE;
          }
        }

        nl_req_ncci(plci,req,ncci);
        if ((plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
         && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
        {
          if (plci->B3_prot == 4)
            sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
          else
            sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
          plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
        }
      }

      else if ((plci->B3_prot == B3_RTP) && (ncpi->length != 0))
      {
        plci->internal_req_buffer[0] = ncpi->length + 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_buffer[2+w] = ncpi->info[1+w];
        start_internal_command (Id, plci, rtp_connect_b3_res_command);
        return FALSE;
      }

      else
      {
        if(ncpi->length>2) {
          if(ncpi->info[1] &1) req = N_CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
        }
        nl_req_ncci(plci,req,ncci);
        sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
        if (plci->adjust_b_restore)
        {
          plci->adjust_b_restore = FALSE;
          start_internal_command (Id, plci, adjust_b_restore);
        }
      }
      return 1;
    }
  }
  return FALSE;
}

byte connect_b3_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ncci;

  ncci = (word)(Id>>16);
  dbug(1,dprintf("connect_b3_a_res(ncci=0x%x)",ncci));

  if (plci && ncci && (plci->State != IDLE) && (plci->State != INC_DIS_PENDING)
   && (plci->State != OUTG_DIS_PENDING))
  {
    if(a->ncci_state[ncci]==INC_ACT_PENDING) {
      a->ncci_state[ncci] = CONNECTED;
      if((plci->State!=INC_CON_CONNECTED_PEND) && (plci->State!=INC_CON_CONNECTED_ALERT))
      {
        plci->State = CONNECTED;
        trcq (plci);
      }
      channel_request_xon (plci, a->ncci_ch[ncci]);
      channel_xmit_xon (plci);
    }
  }
  return FALSE;
}

byte disconnect_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word Info;
  word ncci;
  word w, fax_control_bits, fax_control2_bits;
  API_PARSE * ncpi;

  dbug(1,dprintf("disconnect_b3_req"));

  Info = _WRONG_IDENTIFIER;
  ncci = (word)(Id>>16);
  if (plci && ncci)
  {
    Info = _WRONG_STATE;
    if ((a->ncci_state[ncci] == CONNECTED)
     || (a->ncci_state[ncci] == OUTG_CON_PENDING)
     || (a->ncci_state[ncci] == INC_CON_PENDING)
     || (a->ncci_state[ncci] == INC_ACT_PENDING))
    {
      a->ncci_state[ncci] = OUTG_DIS_PENDING;
      channel_request_xon (plci, a->ncci_ch[ncci]);
      channel_xmit_xon (plci);

      ncpi = &parms[0];
      if (plci->B3_prot==B3_T30_WITH_EXTENSIONS)
      {
        fax_control_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low);
        fax_control2_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low);
        if (ncpi->length >= 4)
        {
          w = READ_WORD(&ncpi->info[3]);
          if ((w & 0x0004) /* Request to send / poll another document */
           && (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS))
          {
            fax_control_bits |= T30_CONTROL_BIT_MORE_DOCUMENTS;
          }
          if (ncpi->length >= 6)
          {
            w = READ_WORD(&ncpi->info[5]);
            if ((w & 0x0100) /* Request procedure interrupt */
             && (a->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD)))
            {
              fax_control2_bits |= T30_CONTROL2_BIT_REQUEST_PRI;
            }
          }
        }
        WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
        WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low, fax_control2_bits);
      }

      if (a->ncci[ncci].data_pending
       && ((plci->B3_prot == B3_TRANSPARENT)
        || (plci->B3_prot == B3_T30)
        || (plci->B3_prot == B3_T30_WITH_EXTENSIONS)))
      {
        trcq (plci);
        plci->send_disc = (byte)ncci;
        plci->command = 0;
        return FALSE;
      }
      else
      {
        trcq (plci);
        cleanup_ncci_data (plci, ncci);

        if(plci->B3_prot==B3_ISO8208 || plci->B3_prot==B3_X25_DCE)
        {
          if(ncpi->length>3)
          {
            add_d(plci, (word)(ncpi->length - 3) ,(byte   *)&(ncpi->info[4]));
          }
        }
        else if(plci->B3_prot==B3_T30_WITH_EXTENSIONS)
        {
          start_internal_command (Id, plci, fax_disconnect_info_command);
          return FALSE;
        }
        nl_req_ncci(plci,N_DISC,ncci);
      }
      return 1;
    }
    else
      trcq (plci);
  }
  sendf(appl,
        _DISCONNECT_B3_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return FALSE;
}

byte disconnect_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ncci;
  word i;

  ncci = (word)(Id>>16);
  dbug(1,dprintf("disconnect_b3_res(ncci=0x%x",ncci));
  if(plci && ncci) {
    plci->requested_options_conn = 0;
    plci->fax_connect_info_length = 0;
    plci->ncpi_state = 0x00;
    if (((plci->B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE))
      && ((plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_LAPD_FREE_SAPI_SEL)))
    {
      plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;
    }
    for(i=0; i<MAX_CHANNELS_PER_PLCI && plci->inc_dis_ncci_table[i]!=(byte)ncci; i++);
    if(i<MAX_CHANNELS_PER_PLCI) {
      if(plci->channels)plci->channels--;
      for(; i<MAX_CHANNELS_PER_PLCI-1; i++) plci->inc_dis_ncci_table[i] = plci->inc_dis_ncci_table[i+1];
      plci->inc_dis_ncci_table[MAX_CHANNELS_PER_PLCI-1] = 0;

      ncci_free_receive_buffers (appl, plci, ncci);

      if((plci->State==IDLE || plci->State==SUSPENDING) && !plci->channels){
        if(plci->State == SUSPENDING){
          sendf(plci->appl,
                _FACILITY_I,
                Id & 0xffffL,
                0,
                "ws", (word)3, "\x03\x04\x00\x00");
          sendf(plci->appl, _DISCONNECT_I, Id & 0xffffL, 0, "w", 0);
        }
        plci_remove(plci);
        return FALSE;
      }
      else
        trcq (plci);
    }
    else
    {
      if (((((plci->B3_prot == 4) || (plci->B3_prot == 5))
         && (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS))
        || ((plci->B2_prot == B2_SDLC) && (((plci->B1_resource & 0xff) == 17) || ((plci->B1_resource & 0xff) == 18)))
        /*|| ((plci->B2_prot == B2_X75) && ((plci->B1_resource & 0xff) == 18))*//* We cannot activate since X.75 transfers UDATA end to end */
        || (plci->B2_prot == 7)
        || (plci->B3_prot == 7))
       && (plci->State != OUTG_DIS_PENDING)
       && (a->ncci_state[ncci] == INC_DIS_PENDING))
      {
        ncci_free_receive_buffers (appl, plci, ncci);
        if ((plci->B3_prot == 4) || (plci->B3_prot == 5))
        {
          trcq (plci);
          nl_req_ncci(plci,N_EDATA,ncci);
        }
        else
        {
          trcq (plci);
          nl_req_ncci(plci,N_DISC,ncci);
        }
        plci->adapter->ncci_state[ncci] = IDLE;
        start_internal_command (Id, plci, fax_disconnect_command);
        return 1;
      }
      else
        trcq (plci);
    }
  }
  return FALSE;
}

byte data_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  NCCI   *ncci_ptr;
  DATA_B3_DESC   *data;
  word Info;
  word ncci;
  word i;

  dbug(1,dprintf("data_b3_req"));

  Info = _WRONG_IDENTIFIER;
  ncci = (word)(Id>>16);
  dbug(1,dprintf("ncci=0x%x, plci=0x%x",ncci,plci));

  if (plci && ncci)
  {
    Info = _WRONG_STATE;
    if ((a->ncci_state[ncci] == CONNECTED)
     || (a->ncci_state[ncci] == INC_ACT_PENDING))
    {
        /* queue data */
      ncci_ptr = &(a->ncci[ncci]);
      i = ncci_ptr->data_out + ncci_ptr->data_pending;
      if (i >= MAX_DATA_B3)
        i -= MAX_DATA_B3;
      data = &(ncci_ptr->DBuffer[i]);
      data->Number = Number;
      if ((((byte   *)(parms[0].info)) >= ((byte   *)(plci->msg_in_queue)))
       && (((byte   *)(parms[0].info)) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {
        data->P = ULongToPtr(*((dword   *)(parms[0].info)));
      }
      else
        data->P = TransmitBufferSet(appl,READ_DWORD(parms[0].info));
      data->Length = READ_WORD(parms[1].info);
      data->Handle = READ_WORD(parms[2].info);
      data->Flags = READ_WORD(parms[3].info);
      (ncci_ptr->data_pending)++;

        /* check for delivery confirmation */
      if (data->Flags & 0x0004)
      {
        i = ncci_ptr->data_ack_out + ncci_ptr->data_ack_pending;
        if (i >= MAX_DATA_ACK)
          i -= MAX_DATA_ACK;
        ncci_ptr->DataAck[i].Number = data->Number;
        ncci_ptr->DataAck[i].Handle = data->Handle;
        (ncci_ptr->data_ack_pending)++;
      }

      send_data(plci);
      return FALSE;
    }
  }
  if (appl)
  {
    if ((Id >> 8) & 0xff)
    {
      plci = &a->plci[((Id >> 8) & 0xff)-1];
      if ((((byte   *)(parms[0].info)) >= ((byte   *)(plci->msg_in_queue)))
       && (((byte   *)(parms[0].info)) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {
        TransmitBufferFree (appl, ULongToPtr(*((dword   *)(parms[0].info))));
      }
    }
    sendf(appl,
          _DATA_B3_R|CONFIRM,
          Id,
          Number,
          "ww",READ_WORD(parms[2].info),Info);
  }
  return FALSE;
}

byte data_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word n;
  word ncci;
  word NCCIcode;

  dbug(1,dprintf("data_b3_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    n = READ_WORD(parms[0].info);
    dbug(1,dprintf("free(%d)",n));
    NCCIcode = ncci | (((word) a->Id) << 8);
    if(n<appl->MaxBuffer &&
       appl->DataNCCI[n]==NCCIcode &&
       (byte)(appl->DataFlags[n]>>8)==plci->Id &&
       (appl->DataFlags[n] &0x10)) {
      dbug(1,dprintf("found"));
      appl->DataNCCI[n] = 0;

      if (channel_can_xon (plci, a->ncci_ch[ncci])) {
        channel_request_xon (plci, a->ncci_ch[ncci]);
      }
      channel_xmit_xon (plci);

      if(appl->DataFlags[n] &0x4) {
        nl_req_ncci(plci,N_DATA_ACK,ncci);
        return 1;
      }
    }
    else
    {
      if ((n >= appl->MaxBuffer) || (appl->DataNCCI[n] != 0))
        (appl->BadDataB3Resp)++;
      dbug(1,dprintf("DATA_B3 IND not found %ld %d %d %08lx %08lx %02x",
        (long)(appl->BadDataB3Resp), n, appl->MaxBuffer, (unsigned long)(UnMapId (Id)),
        (unsigned long)((n < appl->MaxBuffer) ? UnMapController ((byte)(appl->DataNCCI[n] >> 8)) |
          (appl->DataFlags[n] & 0xff00) | (((dword)(appl->DataNCCI[n] & 0xff)) << 16) : 0),
        (n < appl->MaxBuffer) ? appl->DataFlags[n] & 0xff : 0));
    }
  }
  return FALSE;
}

byte reset_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  NCCI   * ncci_ptr;
  word Info;
  word ncci;

  dbug(1,dprintf("reset_b3_req"));

  Info = _WRONG_IDENTIFIER;
  ncci = (word)(Id>>16);
  if(plci && ncci)
  {
    Info = _WRONG_STATE;
    switch (plci->B3_prot)
    {
    case B3_ISO8208:
    case B3_X25_DCE:
      if(a->ncci_state[ncci]==CONNECTED)
      {
        nl_req_ncci(plci,N_RESET,(byte)ncci);
        send_req(plci);
        Info = GOOD;
      }
      break;
    case B3_TRANSPARENT:
      if(a->ncci_state[ncci]==CONNECTED)
      {
        if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSPARENT_N_RESET)
        {
          ncci_ptr = &(plci->adapter->ncci[ncci]);
          while (ncci_ptr->data_pending)
            data_rc (plci, plci->adapter->ncci_ch[ncci], ncci_ptr->DBuffer[ncci_ptr->data_out].P);
          while (ncci_ptr->data_ack_pending)
            data_ack (plci, plci->adapter->ncci_ch[ncci]);
          nl_req_ncci(plci,N_RESET,(byte)ncci);
          send_req(plci);
          start_internal_command (Id, plci, reset_b3_n_reset_command);
        }
        else
          start_internal_command (Id, plci, reset_b3_command);
        Info = GOOD;
      }
      break;
    default:
      Info = _RESET_NOT_SUPPORTED;
      break;
    }
  }
  /* reset_b3 must result in a reset_b3_con & reset_b3_Ind */
  sendf(appl,
        _RESET_B3_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return FALSE;
}

byte reset_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ncci;

  dbug(1,dprintf("reset_b3_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    switch (plci->B3_prot)
    {
    case B3_ISO8208:
    case B3_X25_DCE:
    case B3_TRANSPARENT:
      if(a->ncci_state[ncci]==INC_RES_PENDING)
      {
        a->ncci_state[ncci] = CONNECTED;
        nl_req_ncci(plci,N_RESET_ACK,ncci);
        return TRUE;
      }
      break;
    }
  }
  return FALSE;
}

byte connect_b3_t90_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word ncci;
  API_PARSE * ncpi;
  byte req;

  dbug(1,dprintf("connect_b3_t90_a_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    if(a->ncci_state[ncci]==INC_ACT_PENDING) {
      a->ncci_state[ncci] = CONNECTED;
    }
    else if(a->ncci_state[ncci]==INC_CON_PENDING) {
      a->ncci_state[ncci] = CONNECTED;

      req = N_CONNECT_ACK;

        /* parms[0]==0 for CAPI original message definition! */
      if(parms[0].info) {
        ncpi = &parms[1];
        if(ncpi->length>2) {
          if(ncpi->info[1] &1) req = N_CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
        }
      }
      nl_req_ncci(plci,req,ncci);
      return 1;
    }
  }
  return FALSE;
}


byte select_b_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word Info=0;
  word i;
  byte tel;
    API_PARSE bp_parms[7];

  if(!plci || !msg)
  {
    Info = _WRONG_IDENTIFIER;
  }
  else
  {
    dbug(1,dprintf("select_b_req[%d],PLCI=0x%x,Tel=0x%x,NL=0x%x,appl=0x%x,sstate=0x%x",
                   msg->length,plci->Id,plci->tel,plci->NL.Id,plci->appl,plci->SuppState));
    dbug(1,dprintf("PlciState=0x%x",plci->State));
    for(i=0;i<7;i++) bp_parms[i].length = 0;

    /* check if no channel is open, no B3 connected only */
    if((plci->State == IDLE) || (plci->State == OUTG_DIS_PENDING) || (plci->State == INC_DIS_PENDING)
     || (plci->SuppState != IDLE) || plci->channels || plci->nl_remove_id)
    {
      Info = _WRONG_STATE;
    }
    /* check message format and fill bp_parms pointer */
    else if(msg->length && api_parse(&msg->info[1], (word)msg->length, "wwwsss", bp_parms))
    {
      Info = _WRONG_MESSAGE_FORMAT;
    }
    else
    {
      if((plci->State==INC_CON_PENDING) || (plci->State==INC_CON_ALERT)) /* send alert tone inband to the network, */
      {                                                                  /* e.g. Qsig or RBS or Cornet-N or xess PRI */
        if(Id & EXT_CONTROLLER)
        {
          sendf(appl, _SELECT_B_REQ|CONFIRM, Id, Number, "w", 0x2002); /* wrong controller */
          return 0;
        }
        plci->State = (plci->State==INC_CON_ALERT) ? INC_CON_CONNECTED_ALERT : INC_CON_CONNECTED_PEND;
        trcq (plci);
        assign_incoming_call (plci, appl, TRUE); /* disconnect the other appls its quasi a connect */
      }
      else if(plci->State== INC_CON_BLIND_RETRIEVE)
      {
        plci->State = INC_CON_CONNECTED_ALERT;
        trcq (plci);
      }
      api_save_msg(msg, "s", &plci->saved_msg);
      tel = plci->tel;
      if(Id & EXT_CONTROLLER)
      {
        if(tel) /* external controller in use by this PLCI */
        {
          if(a->AdvSignalAppl && a->AdvSignalAppl!=appl)
          {
            dbug(1,dprintf("Ext_Ctrl in use 1"));
            Info = _WRONG_STATE;
          }
        }
        else  /* external controller NOT in use by this PLCI ? */
        {
          if(a->AdvSignalPLCI)
          {
            dbug(1,dprintf("Ext_Ctrl in use 2"));
            Info = _WRONG_STATE;
          }
          else /* activate the codec */
          {
            dbug(1,dprintf("Ext_Ctrl start"));
            if(AdvCodecSupport(a, plci, appl, 0) )
            {
              dbug(1,dprintf("Error in codec procedures"));
              Info = _WRONG_STATE;
            }
            else if(plci->spoofed_msg==SPOOFING_REQUIRED) /* wait until codec is active */
            {
              plci->spoofed_msg = AWAITING_SELECT_B;
              plci->internal_command = BLOCK_PLCI; /* lock other commands */
              plci->command = 0;
              dbug(1,dprintf("continue if codec loaded"));
              return FALSE;
            }
          }
        }
      }
      else /* external controller bit is OFF */
      {
        if(tel) /* external controller in use, need to switch off */
        {
          if(a->AdvSignalAppl==appl)
          {
            CodecIdCheck(a, plci);
            plci->tel = 0;
            plci->adv_nl = 0;
            dbug(1,dprintf("Ext_Ctrl disable"));
          }
          else
          {
            dbug(1,dprintf("Ext_Ctrl not requested"));
          }
        }
      }
      if (!Info)
      {
        if (plci->call_dir & CALL_DIR_OUT)
          plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
        else if (plci->call_dir & CALL_DIR_IN)
          plci->call_dir = CALL_DIR_IN | CALL_DIR_ANSWER;
        start_internal_command (Id, plci, select_b_protocol_command);
        return FALSE;
      }
    }
  }
  sendf(appl, _SELECT_B_REQ|CONFIRM, Id, Number, "w", Info);
  return FALSE;
}

byte manufacturer_req(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * parms)
{
  word command;
  word i;
  word ncci;
  API_PARSE * m;
    API_PARSE m_parms[5];
  word codec;
  byte req;
  byte ch, channel;
  byte dir;
  static byte chi[4];
  static byte esc_chi[35];
  static byte lli[2] = {0x01,0x00};
  static byte codec_cai[2] = {0x01,0x01};
  static byte empty_bp[] = { 0 };
    API_PARSE bp[2];
  PLCI   * v_plci;
  word Info=0;
  dword relatedPLCIvalue;
  DIVA_CAPI_ADAPTER   * relatedadapter;
  int ret=0;

  dbug(1,dprintf("manufacturer_req"));
  for(i=0;i<5;i++) m_parms[i].length = 0;

  if(READ_DWORD(parms[0].info)!=_DI_MANU_ID) {
    Info = _WRONG_MESSAGE_FORMAT;
  }
  command = READ_WORD(parms[1].info);
  m = &parms[2];
  if(a != 0 && a->vscapi_mode != 0 && (appl->access & DIVA_CAPI_APPL_VSCAPI_ACCESS) == 0 &&
     Info == GOOD && command != _DI_MANAGEMENT_INFO) {
    Info = _WRONG_IDENTIFIER;
  }
  if (!Info)
  {
    switch(command) {
    case _DI_ASSIGN_PLCI:
      if(api_parse(&m->info[1],(word)m->length,"wbbs",m_parms)) {
        diva_capi_apply_saved_action (appl, 0, Number);
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      codec = READ_WORD(m_parms[0].info);
      ch = m_parms[1].info[0];
      dir = m_parms[2].info[0];
      if((i=get_plci(a))) {
        plci = &a->plci[i-1];
        plci->appl = appl;
        plci->command = _MANUFACTURER_R;
        plci->m_command = command;
        plci->number = Number;
        plci->State = LOCAL_CONNECT;
        Id = (((word)plci->Id<<8)|plci->adapter->Id|(Id & EXT_CONTROLLER));
        diva_capi_apply_saved_action (appl, plci, Number);
        dbug(1,dprintf("ManCMD,plci=0x%x",Id));

        if(dir<=2) {
          chi[0] = 0x01;
          chi[1] = (byte)(0x80|ch);
          esc_chi[0] = 0;
          lli[1] = 0;
          plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
          if(!dir)
          {
            plci->call_dir = (plci->call_dir & ~(CALL_DIR_OUT | CALL_DIR_ORIGINATE)) |
              CALL_DIR_IN | CALL_DIR_ANSWER;
          }
          switch(codec)
          {
          case 0:
            Id = (((word)plci->Id<<8)|plci->adapter->Id|(/*Id&*/EXT_CONTROLLER));
            Info = add_b1(plci, &m_parms[3], (word)(((m_parms[3].length == 0) && (dir == 2)) ? 2 : 0), 0);
            break;
          case 1:
            Id = (((word)plci->Id<<8)|plci->adapter->Id|(/*Id&*/EXT_CONTROLLER));
            add_p(plci,CAI,codec_cai);
            break;
          /* manual 'swich on' to the codec support without signalling */
          /* first 'assign plci' with this function, then use */
          case 2:
            Id = (((word)plci->Id<<8)|plci->adapter->Id|(/*Id&*/EXT_CONTROLLER));
            if(AdvCodecSupport(a, plci, appl, 0) ) {
              Info = _RESOURCE_ERROR;
              break;
            }
            api_parse (empty_bp, sizeof(empty_bp), "s", bp);
            Info = add_b1(plci,bp,0,B1_FACILITY_LOCAL);
            lli[1] = 0x10; /* local call codec stream */
            break;
          case 3: /* line side */
            if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_SOFTIP))
            {
              Info = _RESOURCE_ERROR;
              break;
            }
            chi[0] = 0x03;
            chi[1] = 0xa9;
            chi[2] = 0x83;
            chi[3] = (byte)(0x80|ch);
            plci->B1_options |= DSP_CAI_ARRANGEMENT_LINE_SIDE;
            mixer_set_bchannel_id_unchecked (plci, (byte)(a->null_plci_base + ch));
            Info = add_b1(plci,&m_parms[3],0,0);
            break;
          case 4: /* data stub */
          case 6: /* data no clock */
            channel = 0;
            if ((a->null_plci_base != 32) || (a->null_plci_table[channel] != 0))
            {
              channel = a->null_plci_base;
              while ((channel < a->null_plci_base + a->null_plci_channels) && (a->null_plci_table[channel] != 0))
                channel++;
            }
            if (channel == a->null_plci_base + a->null_plci_channels)
            {
              dbug(1,dprintf("Data entities exhaustet"));
              Info = _OUT_OF_PLCI;
              break;
            }
            if (codec == 6)
            {
              if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_NO_CLOCK_LINE)
               || !(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT))
              {
                Info = _RESOURCE_ERROR;
                break;
              }
              plci->B1_options |= DSP_CAI_ARRANGEMENT_DATA_NO_CLOCK;
            }
            else
            {
              if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_RTP_LINE)
               || !(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT))
              {
                Info = _RESOURCE_ERROR;
                break;
              }
              plci->B1_options |= DSP_CAI_ARRANGEMENT_DATA_STUB;
            }
            a->null_plci_table[channel] = plci->Id;
            esc_chi[0] = 34;
            esc_chi[1] = 0x18;
            esc_chi[2] = channel;
            for (i = 0; i < 32; i++)
              esc_chi[3+i] = 0;
            plci->b_channel = channel;
            mixer_set_bchannel_id_unchecked (plci, plci->b_channel);
            Info = add_b1(plci,&m_parms[3],0,0);
            break;
          case 5: /* line stub */
          case 7: /* line no clock */
            channel = 0;
            if ((a->null_plci_base != 32) || (a->null_plci_table[channel] != ch))
            {
              channel = a->null_plci_base;
              while ((channel < a->null_plci_base + a->null_plci_channels) && (a->null_plci_table[channel] != ch))
                channel++;
            }
            if ((ch == 0) || (channel == a->null_plci_base + a->null_plci_channels))
            {
              dbug(1,dprintf("Line entity not associated"));
              Info = _OUT_OF_PLCI;
              break;
            }
            if (codec == 7)
            {
              if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_NO_CLOCK_LINE)
               || !(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT))
              {
                Info = _RESOURCE_ERROR;
                break;
              }
              plci->B1_options |= DSP_CAI_ARRANGEMENT_LINE_NO_CLOCK;
            }
            else
            {
              if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_RTP_LINE)
               || !(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT))
              {
                Info = _RESOURCE_ERROR;
                break;
              }
              plci->B1_options |= DSP_CAI_ARRANGEMENT_LINE_STUB;
            }
            esc_chi[0] = 34;
            esc_chi[1] = 0x18;
            esc_chi[2] = channel;
            for (i = 0; i < 32; i++)
              esc_chi[3+i] = 0;
            Info = add_b1(plci,&m_parms[3],0,0);
            break;
          default:
            Info = _RESOURCE_ERROR;
            break;
          }

          plci->State = LOCAL_CONNECT;
          plci->command = _MANUFACTURER_R;
          plci->m_command = command;
          plci->number = Number;

          if(!Info)
          {
            add_p(plci,LLI,lli);
            if (chi[0] != 0)
              add_p(plci,CHI,chi);
            add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
            if (esc_chi[0] != 0)
              add_p(plci,ESC,esc_chi);
            sig_req(plci,ASSIGN,DSIG_ID);

            if((codec != 1) && (codec != 2) && (plci->B1_resource != 0))
            {
              Info = add_b23(plci,&m_parms[3]);
              if(!Info)
              {
                nl_req_ncci(plci,ASSIGN,0);
                send_req(plci);
              }
            }
            if(!Info)
            {
              dbug(1,dprintf("dir=0x%x,spoof=0x%x",dir,plci->spoofed_msg));
              if (plci->spoofed_msg==SPOOFING_REQUIRED)
              {
                api_save_msg (m_parms, "wbbs", &plci->saved_msg);
                plci->spoofed_msg = AWAITING_MANUF_CON;
                plci->internal_command = BLOCK_PLCI; /* reject other req meanwhile */
                plci->command = 0;
                send_req(plci);
                return FALSE;
              }
              if(dir==1) {
                sig_req(plci,CALL_REQ,0);
              }
              else if(!dir){
                sig_req(plci,LISTEN_REQ,0);
              }
              send_req(plci);
              return FALSE;
            }
            else
            {
              sendf(appl,
                    _MANUFACTURER_R|CONFIRM,
                    Id,
                    Number,
                    "dww",_DI_MANU_ID,command,Info);
              return 2;
            }
          }
        }
      }
      else
      {
        diva_capi_apply_saved_action (appl, 0, Number);
        Info = _OUT_OF_PLCI;
      }
      break;

    case _DI_IDI_CTRL:
      if(!plci)
      {
        Info = _WRONG_IDENTIFIER;
        break;
      }
      if(api_parse(&m->info[1],(word)m->length,"bs",m_parms)) {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      req = m_parms[0].info[0];
      plci->command = _MANUFACTURER_R;
      plci->m_command = command;
      plci->number = Number;
      if(req==CALL_REQ)
      {
        plci->b_channel = getChannel(&m_parms[1]);
        mixer_set_bchannel_id_esc (plci, plci->b_channel);
        if(plci->spoofed_msg==SPOOFING_REQUIRED)
        {
          plci->spoofed_msg = CALL_REQ | AWAITING_MANUF_CON;
          plci->internal_command = BLOCK_PLCI; /* reject other req meanwhile */
          plci->command = 0;
          break;
        }
      }
      add_ss(plci,FTY,&m_parms[1]);
      sig_req(plci,req,0);
      send_req(plci);
      if(req==HANGUP)
      {
        if (plci->NL.Id && !plci->nl_remove_id)
        {
          if (plci->channels)
          {
            for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
            {
              if ((a->ncci_plci[ncci] == plci->Id) && (a->ncci_state[ncci] == CONNECTED))
              {
                a->ncci_state[ncci] = OUTG_DIS_PENDING;
                cleanup_ncci_data (plci, ncci);
                nl_req_ncci(plci,N_DISC,ncci);
              }
            }
          }
          mixer_remove (plci);
          nl_req_ncci(plci,REMOVE,0);
          send_req(plci);
        }
      }
      else if (req == LAW_REQ)
      {
        plci->cr_enquiry = TRUE;
      }
      else if (req == STATUS_REQ)
      {
        return (FALSE);
      }
      break;

    case _DI_STREAM_CTRL:

      if ((a->manufacturer_features2 & MANUFACTURER_FEATURE2_STREAMING_IDI) == 0)
      {
        Info = _FACILITY_NOT_SUPPORTED;
        break;
      }
      if(api_parse(&m->info[1],(word)m->length,"bs",m_parms))
      {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      if (m_parms[1].length >= sizeof(plci->stream_data))
      {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      if (m_parms[0].info[0] == 2)
      {
        break;
      }
      if (m_parms[0].info[0] == 1)
      { /* Save tagged by message number stream info */
        diva_saved_capi_appl_stream_action_t* action;
        if (plci != 0 || a == 0)
        {
          Info = _WRONG_STATE;
          break;
        }
        action = diva_os_malloc (DIVA_OS_MALLOC_ATOMIC, sizeof(*action));
        if (action == 0)
        {
          Info = _RESOURCE_ERROR;
          break;
        }
        action->hdr.messageNr = Number;
        action->hdr.action    = diva_capi_init_stream_data;
        action->length = m_parms[1].length+1;
        memcpy (action->stream_data, m_parms[1].info, m_parms[1].length+1);
        diva_q_add_tail (&appl->saved_action_q, &action->hdr.link);
        appl->appl_flags |= APPL_FLAG_USES_STREAMING;
        action->length = diva_capi_add_streaming_application_id (action->stream_data, sizeof(action->stream_data), appl->Id);
        break;
      }
      if(m_parms[0].info[0] != 0 || plci == 0 || a == 0)
      {
        Info = _WRONG_IDENTIFIER;
        break;
      }
      memcpy (plci->stream_data, m_parms[1].info, m_parms[1].length+1);
      diva_capi_add_streaming_application_id (plci->stream_data, sizeof(plci->stream_data), appl->Id);
      appl->appl_flags |= APPL_FLAG_USES_STREAMING;


      break;

    case _DI_SIG_CTRL:
    /* signalling control for loop activation B-channel */
      if(!plci)
      {
        Info = _WRONG_IDENTIFIER;
        break;
      }
      if(m->length){
        plci->command = _MANUFACTURER_R;
        plci->m_command = command;
        plci->number = Number;
        add_ss(plci,FTY,m);
        sig_req(plci,SIG_CTRL,0);
        send_req(plci);
      }
      else Info = _WRONG_MESSAGE_FORMAT;
      break;

    case _DI_RXT_CTRL:
    /* activation control for receiver/transmitter B-channel */
      if(!plci)
      {
        Info = _WRONG_IDENTIFIER;
        break;
      }
      if (plci
       && (m->length >= 3)
       && (m->info[1] == 0x1c)
       && (m->info[2] >= 1))
      {
        if (m->info[3] == DSP_CTRL_SIG_TRANSMIT)
        {
          return (sig_tx_request (Id, Number, a, plci, appl, parms));
        }
      }
      if(m->length){
        plci->command = _MANUFACTURER_R;
        plci->m_command = command;
        plci->number = Number;
        add_ss(plci,FTY,m);
        sig_req(plci,DSP_CTRL,0);
        send_req(plci);
      }
      else Info = _WRONG_MESSAGE_FORMAT;
      break;

    case _DI_ADV_CODEC:
    case _DI_DSP_CTRL:
      /* TEL_CTRL commands to support non standard adjustments: */
      /* Ring on/off, Handset micro volume, external micro vol. */
      /* handset+external speaker volume, receiver+transm. gain,*/
      /* handsfree on (hookinfo off), set mixer command         */

      if(command == _DI_ADV_CODEC)
      {
        if(!a->AdvCodecPLCI) {
          Info = _WRONG_STATE;
          break;
        }
        v_plci = a->AdvCodecPLCI;
      }
      else
      {
        if (plci
         && (m->length >= 3)
         && (m->info[1] == 0x1c)
         && (m->info[2] >= 1))
        {
          if (m->info[3] == DSP_CTRL_OLD_SET_MIXER_COEFFICIENTS)
          {
            if ((plci->tel != ADV_VOICE) || (plci != a->AdvSignalPLCI))
            {
              Info = _WRONG_STATE;
              break;
            }
            a->adv_voice_coef_length = m->info[2] - 1;
            if (a->adv_voice_coef_length > m->length - 3)
              a->adv_voice_coef_length = (byte)(m->length - 3);
            if (a->adv_voice_coef_length > ADV_VOICE_COEF_BUFFER_SIZE)
              a->adv_voice_coef_length = ADV_VOICE_COEF_BUFFER_SIZE;
            for (i = 0; i < a->adv_voice_coef_length; i++)
              a->adv_voice_coef_buffer[i] = m->info[4 + i];
            if (get_b1_facilities (plci, plci->B1_resource) & B1_FACILITY_VOICE)
              adv_voice_write_coefs (plci, ADV_VOICE_WRITE_UPDATE);
            break;
          }
          else if (m->info[3] == DSP_CTRL_SET_DTMF_PARAMETERS)
          {
            if (!(a->manufacturer_features & MANUFACTURER_FEATURE_DTMF_PARAMETERS))
            {
              Info = _FACILITY_NOT_SUPPORTED;
              break;
            }

            plci->dtmf_parameter_length = m->info[2] - 1;
            if (plci->dtmf_parameter_length > m->length - 3)
              plci->dtmf_parameter_length = (byte)(m->length - 3);
            if (plci->dtmf_parameter_length > DTMF_PARAMETER_BUFFER_SIZE)
              plci->dtmf_parameter_length = DTMF_PARAMETER_BUFFER_SIZE;
            for (i = 0; i < plci->dtmf_parameter_length; i++)
              plci->dtmf_parameter_buffer[i] = m->info[4+i];
            if (get_b1_facilities (plci, plci->B1_resource) & B1_FACILITY_DTMFR)
              dtmf_parameter_write (plci);
            break;

          }

          else if (m->info[3] == DSP_CTRL_SET_PITCH_PARAMETERS)
          {
            return (pitch_request (Id, Number, a, plci, appl, parms));
          }


          else if (m->info[3] == DSP_CTRL_SET_AUDIO_PARAMETERS)
          {
            return (audio_request (Id, Number, a, plci, appl, parms));
          }

        }
        v_plci = plci;
      }

      if(!v_plci)
      {
        Info = _WRONG_IDENTIFIER;
        break;
      }
      if(m->length){
        add_ss(v_plci,FTY,m);
        sig_req(v_plci,TEL_CTRL,0);
        send_req(v_plci);
      }
      else Info = _WRONG_MESSAGE_FORMAT;

      break;

    case _DI_OPTIONS_REQUEST:
      if(api_parse(&m->info[1],(word)m->length,"d",m_parms)) {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      if (READ_DWORD (m_parms[0].info) & ~(a->profile.Manuf.private_options |
          (MANUFACTURER_FEATURE2_VENDOR_BITS << PRIVATE_VENDOR_BIT_0)))
      {
        Info = _FACILITY_NOT_SUPPORTED;
        break;
      }
      /* only one GCD-Application is allowed to register */
      if(READ_DWORD(m_parms[0].info)&(1L << PRIVATE_MADAPTER))
      {
        for(i=0;i<max_appl;i++)
        {
          if(a->requested_options_table[i]&(1L << PRIVATE_MADAPTER))
          {
            Info = _REQUEST_NOT_ALLOWED_IN_THIS_STATE;
            break;
          }
        }
        if(i<max_appl) break;
      }
      if (plci)
        plci->requested_options = READ_DWORD (m_parms[0].info);
      else
      {
        if ((a->requested_options_table[appl->Id-1] ^ READ_DWORD (m_parms[0].info)) & (1L << PRIVATE_MADAPTER))
        {
          if (a->requested_options_table[appl->Id-1] & (1L << PRIVATE_MADAPTER))
          {
            a->requested_options_table[appl->Id-1] &= ~(1L << PRIVATE_MADAPTER);
            diva_capi_check_l1_activation (appl->Id, a);
          }
          else
          {
            diva_capi_check_l1_shutdown (appl->Id, a);
          }
        }
        a->requested_options_table[appl->Id-1] = READ_DWORD (m_parms[0].info);
      }
      break;


    case _DI_SSEXT_CTRL: {
      /* Test if message has the right length */
      if((m->length < 2) || (m->length > 0xfe))
      {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }

      switch(READ_WORD(&m->info[1])) /* first parameter - Ssext Command word */
      {
      case SSTUNNEL: {
        if (api_parse (&parms[2].info[1], parms[2].length, "ws", m_parms))
        {
          dbug(1,dprintf("wrong format in basic SSEXT_CTRL parameters"));
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        if(m_parms[1].length && m_parms[1].info[0]) 
        {
          switch(m_parms[1].info[1])
          {
          case CCBS_IN_AVAILABLE:
          case CCBS_IN_CLEAR_CB:
            if (api_parse (&parms[2].info[1], parms[2].length, "wsws", m_parms))
            {
              dbug(1,dprintf("wrong format in SSTUNNEL SSEXT_CTRL parameters"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if((READ_WORD(m_parms[2].info)==SSIDENTIFIER) &&
              m_parms[3].length && m_parms[3].info[0])
            { /* search for the NCRL-PLCI for this Controller */
              if(a->ncrl_plci_id == 0) {
                dbug(1,dprintf("SSTUNNEL: no NCRL PLCI"));
                dump_plcis (a);
                Info = _WRONG_STATE;
                break;
              }
              v_plci = &a->plci[a->ncrl_plci_id-1];
              /* now we have everything - the S_SERVICE parameter, the ESC_CAI for the MTPX to route to the right adapter
                and the NCRL PLCI */
              chi[0]=1;
              chi[1]=m_parms[1].info[1];
              add_p(v_plci,CAI,chi); /* the CAI IE must be not soo long so only use it to transfer the command */
              add_p(v_plci,FTY,m_parms[1].info);
              v_plci->command = 0;
              v_plci->internal_command = 0; /* no interest on response */
              add_p(v_plci,ESC,m_parms[3].info); /* ESC_CAI for the MPTX to route to the right adapter */
              sig_req(v_plci,S_SERVICE,0);
              send_req(v_plci);
              ret=1;
              break;
            }
            break;
          default:
            break;
          }
        }
      }  break;
      default:
        break;
      }

      if(Info || ret) break;

      if(!plci) /* old version of _DI_SSEXT_CTRL */
      {
        if((i=get_plci(a)))
        {
          v_plci = &a->plci[i-1];
          v_plci->internal_command = SSEXT_REQ_COMMAND;
          v_plci->appl = appl;
          v_plci->number = Number;
          v_plci->cr_enquiry=TRUE;
            /* make space for the InfoElement SSEXTIE */
          v_plci->saved_msg.info[0] = (byte)(m->length + 1);
          v_plci->saved_msg.info[1] = SSEXTIE;
          WRITE_WORD(&v_plci->saved_msg.info[2],READ_WORD(&m->info[1])); /* SSEXT Command word */
          for (i = 2; i < m->length; i++)
            v_plci->saved_msg.info[2+i] = m->info[1+i];
          v_plci->saved_msg.parms[0].info = v_plci->saved_msg.info;
          v_plci->saved_msg.parms[0].length = v_plci->saved_msg.info[0];
          v_plci->saved_msg.parms[1].info = NULL;
          v_plci->saved_msg.parms[1].length = 0;

          add_p(v_plci,CAI,"\x01\x80");
          add_p(v_plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
          sig_req(v_plci,ASSIGN,DSIG_ID);
          send_req(v_plci);
          return FALSE;
        }
        Info = _OUT_OF_PLCI;
      }
      else
      {
        plci->internal_command = SSEXT_REQ_COMMAND;
        plci->command = 0;
        plci->number = Number;
        plci->cr_enquiry=FALSE;
          /* make space for the InfoElement SSEXTIE */
        plci->saved_msg.info[0] = (byte)(m->length + 1);
        plci->saved_msg.info[1] = SSEXTIE;
        WRITE_WORD(&plci->saved_msg.info[2],READ_WORD(&m->info[1])); /* SSEXT Command word */
        for (i = 2; i < m->length; i++)
          plci->saved_msg.info[2+i] = m->info[1+i];
        add_p(plci,ESC,plci->saved_msg.info);
        sig_req(plci,SSEXT_REQ,0);
        send_req(plci);
        return FALSE;
      }
    } break;

    case _DI_GCD_REDIRECT: {
        /*
        Test if message has the right length
        The message formati is:
          p[0]   - total length of the redirect data
          p[1]   - first i.e.
          p[2]   - first i.e. length
          ...
          p[x]   - next i.e.
          p[x+1] - next i.e. length

        */
        int total_length;

        if ((m->length < 3) || (m->length > 0xfe))
        {
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        if(!plci)
        {
          Info = _REQUEST_NOT_ALLOWED_IN_THIS_STATE;
          break;
        }

        for (i = 1, total_length = m->info[0]; i < total_length;) {
          if ((i + m->info[i+1] + 1) <= total_length) {
            add_ie (plci, m->info[i], &m->info[i+1], m->info[i+1]);
            i += (m->info[i+1] + 2);
          } else {
            Info = _WRONG_MESSAGE_FORMAT;
            break;
          }
        }
        if (!Info) {
          sig_req(plci,HANGUP,0);
          send_req(plci);
        }
      }
      break;


    case _DI_MEASURE_CTRL:
    case _DI_GENERIC_TONE:
      return (measure_request (Id, Number, a, plci, appl, parms));


    case _DI_CONTROL_MESSAGE:
      return (control_message_request (Id, Number, a, plci, appl, parms));

    case _DI_RESEND_BUSY_RPLCI:
      if(!plci) return FALSE;
      if(api_parse(&parms[2].info[1],(word)parms[2].length,"ws",m_parms)) return FALSE;
      /* exchange the PLCIs back so this is an original request again */
      relatedPLCIvalue = READ_DWORD(&m_parms[1].info[1+3]);
      relatedPLCIvalue = (relatedPLCIvalue & ~((dword) 0x7f)) | MapController ((byte)(relatedPLCIvalue & 0x7f));
      if (((relatedPLCIvalue & 0x7f) == 0) || ((relatedPLCIvalue & 0xff00) == 0))
        return FALSE;
      /* write the original PLCI value in the message */
      WRITE_DWORD(&m_parms[1].info[1+3],(dword)((plci->Id << 8) | UnMapController (plci->adapter->Id)));
      relatedadapter = &adapter[(relatedPLCIvalue & 0x7f) - 1];
      v_plci = &relatedadapter->plci[((relatedPLCIvalue >> 8) & 0xff) - 1];
      plci->command=0; /* kill the original manufacturer command */
      if (v_plci->Id && appl)
      {
        v_plci->number = Number;
        facility_req(relatedPLCIvalue,Number,relatedadapter,v_plci,appl,m_parms);
      }
      return FALSE;


    case _DI_MANAGEMENT_INFO: {
      if(api_parse(&m->info[1],(word)m->length,"bs",m_parms)) {
        Info = _WRONG_MESSAGE_FORMAT;
        break;
      }
      switch (m_parms[0].info[0]) {
        case _DI_MANAGEMENT_INFO_IDENTIFY:
          if (a != 0 && appl != 0 &&
              m_parms[1].length == sizeof(_DI_MANAGEMENT_INFO_IDENTIFY_DATA)-1 &&
              memcmp (&m_parms[1].info[1],
              _DI_MANAGEMENT_INFO_IDENTIFY_DATA,
              sizeof(_DI_MANAGEMENT_INFO_IDENTIFY_DATA)-1) == 0) {
            appl->access |= DIVA_CAPI_APPL_VSCAPI_ACCESS;
            sendf (appl,
                   _MANUFACTURER_R|CONFIRM,
                   Id,
                   Number,
                   "dwwb(b)", _DI_MANU_ID, command, Info, _DI_MANAGEMENT_INFO_IDENTIFY, NUMBER_OF_CONTROLLER);
            return (FALSE);
          } else {
            Info = (a != 0) ? _WRONG_MESSAGE_FORMAT : _OUT_OF_PLCI;
          }
          break;

        case _DI_MANAGEMENT_INFO_READ: {
            byte length = 2+1 + /* Logical adapter number */
                          2+4 + /* Serial number */
                          2+2 + /* Card type */
                          2+4 + /* Card Id */
                          2+2;  /* Card interfaces */
            int info_size = (length+1)*a->mtpx_cfg.number_of_configuration_entries + 1;
            static byte info[512];

            if (sizeof(info) > info_size && a != 0 && a->mtpx_cfg.cfg != 0) {
              dword i, j;

              for (i = 0, j = 1; i < a->mtpx_cfg.number_of_configuration_entries; i++) {
                info[j++] = length;

                info[j++] = _DI_MANAGEMENT_INFO_READ_LOGICAL_ADAPTER_NUMBER_IE;
                info[j++] = 1;
                info[j++] = (byte)a->mtpx_cfg.cfg[i].logical_adapter_number;

                info[j++] = _DI_MANAGEMENT_INFO_READ_ADAPTER_SERIAL_NUMBER_IE;
                info[j++] = 4;
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].serial_number);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].serial_number >> 8);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].serial_number >> 16);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].serial_number >> 24);

                info[j++] = _DI_MANAGEMENT_INFO_READ_CARD_TYPE_IE;
                info[j++] = 2;
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].cardtype);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].cardtype >> 8);

                info[j++] = _DI_MANAGEMENT_INFO_READ_CARD_ID_IE;
                info[j++] = 4;
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].card_id);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].card_id >> 8);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].card_id >> 16);
                info[j++] = (byte)(a->mtpx_cfg.cfg[i].card_id >> 24);

                info[j++] = _DI_MANAGEMENT_INFO_READ_CARD_INTERFACES_IE;
                info[j++] = 2;
                info[j++] = a->mtpx_cfg.cfg[i].ifc;
                info[j++] = a->mtpx_cfg.cfg[i].interfaces;
              }
              info[0] = (byte)(info_size-1);

              sendf (appl,
                     _MANUFACTURER_R|CONFIRM,
                     Id,
                     Number,
                     "dwwb((bbb)s)", _DI_MANU_ID, command, Info, _DI_MANAGEMENT_INFO_READ,
                     _DI_MANAGEMENT_INFO_READ_VSCAPI_IE, 1, a->vscapi_mode != 0,
                     info);
              return (FALSE);
            } else {
              Info = _OUT_OF_PLCI;
            }
          } break;

        default:
          Info = _WRONG_MESSAGE_FORMAT;
          break;
      }
    } break;



    default:
      Info = _WRONG_MESSAGE_FORMAT;
      break;
    }
  }

  sendf(appl,
        _MANUFACTURER_R|CONFIRM,
        Id,
        Number,
        "dww",_DI_MANU_ID,command,Info);
  return FALSE;
}


byte manufacturer_res(dword Id, word Number, DIVA_CAPI_ADAPTER   * a, PLCI   * plci, APPL   * appl, API_PARSE * msg)
{
  word indication;
  word i;
    API_PARSE m_parms[5];
  PLCI   *rplci;
  byte chi[2];

  API_PARSE *ncpi;
    API_PARSE fax_parms[9];
  word fax_control_bits, fax_control2_bits;
  byte len;


  dbug(1,dprintf("manufacturer_res"));

  if ((msg[0].info == NULL)
   || (READ_DWORD(msg[0].info)!=_DI_MANU_ID))
  {
    return FALSE;
  }
  indication = READ_WORD(msg[1].info);
  switch (indication)
  {

  case _DI_NEGOTIATE_B3:
    if(!plci)
      break;
    if (((plci->B3_prot != 4) && (plci->B3_prot != 5))
     || !(plci->ncpi_state & NCPI_NEGOTIATE_B3_EXPECTED))
    {
      dbug(1,dprintf("wrong state for NEGOTIATE_B3 parameters"));
      break;
    }
    plci->ncpi_state &= ~NCPI_NEGOTIATE_B3_EXPECTED;
    if (((((T30_INFO   *)(plci->fax_connect_info_buffer))->operating_mode &
         T30_OPERATING_MODE_NUMBER_MASK) == T30_OPERATING_MODE_CAPI_NEG)
     && (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
    {
      if (api_parse (&msg[2].info[1], msg[2].length, "ws", m_parms))
      {
        dbug(1,dprintf("wrong format in NEGOTIATE_B3 parameters"));
        break;
      }
      ncpi = &m_parms[1];
      len = (byte)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0));
      if (plci->fax_connect_info_length < len)
      {
        ((T30_INFO *)(plci->fax_connect_info_buffer))->station_id_len = 0;
        ((T30_INFO *)(plci->fax_connect_info_buffer))->head_line_len = 0;
      }
      if (api_parse (&ncpi->info[1], ncpi->length, "wwwws", fax_parms))
      {
        dbug(1,dprintf("non-standard facilities info missing or wrong format"));
      }
      else
      {
        fax_control_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low);
        fax_control2_bits = READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low);
        if (ncpi->length >= 6)
        {
          if ((READ_WORD(&ncpi->info[5]) & 0x0100) /* Request procedure interrupt */
           && (a->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD)))
          {
            fax_control2_bits |= T30_CONTROL2_BIT_REQUEST_PRI;
          }
        }
        WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
        WRITE_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low, fax_control2_bits);
        if (!api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
        {
          if (plci->fax_connect_info_length <= len)
            plci->fax_connect_info_buffer[len] = 0;
          len += 1 + plci->fax_connect_info_buffer[len];
          if (plci->fax_connect_info_length <= len)
            plci->fax_connect_info_buffer[len] = 0;
          len += 1 + plci->fax_connect_info_buffer[len];
          if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
            plci->nsf_control_bits = READ_WORD(&fax_parms[7].info[2]);
          plci->fax_connect_info_buffer[len++] = (byte)(fax_parms[7].length);
          for (i = 0; i < fax_parms[7].length; i++)
            plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
        }
      }
      plci->fax_connect_info_length = len;
      plci->fax_edata_ack_length = plci->fax_connect_info_length;
      start_internal_command (Id, plci, fax_edata_ack_command);
    }
    break;

  case _DI_INFO_B3:
    break;

  case _DI_SSEXT_CTRL:
    /* byte overall length of parameterstring
       word Ssext Command
       byte lengt of Ssext Command
       word Ssext Command -> OPTIONAL
       byte lengt of Ssext Command
       ... */
    if(msg[2].length == 0) break;
    if (api_parse (&msg[2].info[1], msg[2].length, "ws", m_parms))
    {
      dbug(1,dprintf("wrong format in basic SSEXT_CTRL parameters"));
      break;
    }
    switch(READ_WORD(m_parms[0].info)) /* first parameter - Ssext Command word */
    {
    case SSTUNNEL:
      if(m_parms[1].length && m_parms[1].info[0]) 
      {
        switch(m_parms[1].info[1])
        {
        case CCBS_IN_SET_CB:
        case CCBS_IN_CLEAR_CB:
          if (api_parse (&msg[2].info[1], msg[2].length, "wsws", m_parms))
          {
            dbug(1,dprintf("wrong format in SSTUNNEL SSEXT_CTRL parameters"));
            break;
          }
          if((READ_WORD(m_parms[2].info)==SSIDENTIFIER) &&
            m_parms[3].length && m_parms[3].info[0])
          { /* search for the NCRL-PLCI for this Controller */
            if(a->ncrl_plci_id == 0) {
              dbug(1,dprintf("SSTUNNEL: no NCRL PLCI"));
              dump_plcis (a);
              return FALSE;
            }
            rplci = &a->plci[a->ncrl_plci_id-1];
            /* now we have everything - the S_SERVICE parameter, the ESC_CAI for the MTPX to route to the right adapter
               and the NCRL PLCI */
            chi[0]=1;
            chi[1]=m_parms[1].info[1];
            add_p(rplci,CAI,chi); /* the CAI IE must be not soo long so only use it to transfer the command */
            add_p(rplci,FTY,m_parms[1].info);
            rplci->command = 0;
            rplci->internal_command = 0; /* no interest on response */
            add_p(rplci,ESC,m_parms[3].info); /* ESC_CAI for the MPTX to route to the right adapter */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            break;
          }
          break;
        default:
          break;
        }
      }
      break;
    default:
      break;
    }
    break;
  }
  return FALSE;
}

/*------------------------------------------------------------------*/
/* IDI callback function                                            */
/*------------------------------------------------------------------*/

void   callback(ENTITY   * e)
{
  DIVA_CAPI_ADAPTER   * a;
  APPL   * appl;
  PLCI   * plci;
  CAPI_MSG   *m;
  word i, j;
  byte rc;
  byte ch;
  byte req;
  byte global_req;
  int no_cancel_rc;

  dbug(1,dprintf("%x:CB(%x:Req=%x,Rc=%x,Ind=%x)",
                 (e->user[0]+1)&0x7fff,e->Id,e->Req,e->Rc,e->Ind));

  a = &(adapter[(byte)e->user[0]]);
  plci = &(a->plci[e->user[1]]);
  no_cancel_rc = DIVA_CAPI_SUPPORTS_NO_CANCEL(a);

  /*
     If new protocol code and new XDI is used then CAPI should work
     fully in accordance with IDI cpec an look on callback field instead
     of Rc field for return codes.
   */
  if (((e->complete == 0xff) && no_cancel_rc) ||
      (e->Rc && !no_cancel_rc)) {
    rc = e->Rc;
    ch = e->RcCh;
    req = e->Req;
    e->Rc = 0;

    if (e->user[0] & 0x8000)
    {
      /*
         If REMOVE request was sent then we have to wait until
         return code with Id set to zero arrives.
         All other return codes should be ignored.
         */
      if (req == REMOVE)
      {
        if (e->Id)
        {
          dbug(1,dprintf("cancel RC in REMOVE state"));
          return;
        }
        plci->data_sent = FALSE;
        if ((plci->nl_remove_ncci == 0) && plci->appl)
          ncci_free_receive_buffers (plci->appl, plci, 0);
        ncci_remove (plci, 0, (byte)(plci->nl_remove_ncci != 0));
        channel_flow_control_remove (plci);
        for (i = 0; i < 256; i++)
        {
          if (a->FlowControlIdTable[i] == plci->nl_remove_id)
            a->FlowControlIdTable[i] = 0;
        }
        plci->nl_remove_id = 0;

        e->Ind   = 0;
        e->IndCh = 0;
        e->ReqCh = 0;
        e->RcCh  = 0;
        e->RNR   = 0;

        if (plci->rx_dma_descriptor > 0) {
          diva_free_dma_descriptor (plci, plci->rx_dma_descriptor - 1);
          plci->rx_dma_descriptor = 0;
        }
      }
      if (rc == OK_FC)
      {
        trcq (plci);
        a->FlowControlIdTable[ch] = e->Id;
        a->FlowControlSkipTable[ch] = 0;

        a->ch_flow_control[ch] |= N_OK_FC_PENDING;
        channel_flow_control_plci (plci, ch);
        plci->nl_req = 0;
      }
      else
      {
        /*
          Cancel return codes self, if feature was requested
          */
        if (no_cancel_rc && (a->FlowControlIdTable[ch] == e->Id) && e->Id) {
          a->FlowControlIdTable[ch] = 0;
          if ((rc == OK) && a->FlowControlSkipTable[ch]) {
            trcq (plci);
            dbug(3,dprintf ("XDI CAPI: RC cancelled Id:0x02, Ch:%02x",                              e->Id, ch));
            return;
          }
        }

        if (a->ch_flow_control[ch] & N_OK_FC_PENDING)
        {
          a->ch_flow_control[ch] &= ~N_OK_FC_PENDING;
          if (ch == e->ReqCh)
          {
            trcq (plci);
            plci->nl_req = 0;
          }
          else
            trcq (plci);
        }
        else
        {
          trcq (plci);
          plci->nl_req = 0;
        }
      }
      if (plci->nl_req)
        control_rc (plci, 0, rc, ch, 0, TRUE);
      else
      {
        if (req == N_XON)
        {
          channel_x_on (plci, ch);
          if (plci->internal_command)
            control_rc (plci, req, rc, ch, 0, TRUE);
        }
        else
        {
          if (plci->nl_global_req)
          {
            global_req = plci->nl_global_req;
            plci->nl_global_req = 0;
            if (rc != ASSIGN_OK) {
              e->Id = 0;
              if (plci->rx_dma_descriptor > 0) {
                diva_free_dma_descriptor (plci, plci->rx_dma_descriptor - 1);
                plci->rx_dma_descriptor = 0;
              }
            }
            channel_xmit_xon (plci);
            control_rc (plci, 0, rc, ch, global_req, TRUE);
          }
          else if (plci->data_sent)
          {
            channel_xmit_xon (plci);
            plci->data_sent = FALSE;
            plci->NL.XNum = 1;
            data_rc (plci, ch, plci->data_sent_ptr);
            if (plci->internal_command)
              control_rc (plci, req, rc, ch, 0, TRUE);
          }
          else
          {
            channel_xmit_xon (plci);
            control_rc (plci, req, rc, ch, 0, TRUE);
          }
        }
      }
    }
    else
    {
      /*
         If REMOVE request was sent then we have to wait until
         return code with Id set to zero arrives.
         All other return codes should be ignored.
         */
      if (req == REMOVE)
      {
        if (e->Id)
        {
          trcq (plci);
          dbug(1,dprintf("cancel RC in REMOVE state"));
          return;
        }
        plci->sig_remove_id = 0;

        e->Ind   = 0;
        e->IndCh = 0;
        e->ReqCh = 0;
        e->RcCh  = 0;
        e->RNR   = 0;
      }
      plci->sig_req = 0;
      if (plci->sig_global_req)
      {
        global_req = plci->sig_global_req;
        plci->sig_global_req = 0;
        if (rc != ASSIGN_OK)
        {
          trcq (plci);
          e->Id = 0;
        }
        else
          trcq (plci);
        channel_xmit_xon (plci);
        control_rc (plci, 0, rc, ch, global_req, FALSE);
      }
      else
      {
        trcq (plci);
        channel_xmit_xon (plci);
        control_rc (plci, req, rc, ch, 0, FALSE);
      }
    }
    /*
      Again: in accordance with IDI spec Rc and Ind can't be delivered in the
      same callback. Also if new XDI and protocol code used then jump
      direct to finish.
      */
    if (no_cancel_rc) {
      channel_xmit_xon(plci);
      goto capi_callback_suffix;
    }
  }
  else
    trcq (plci);

  channel_xmit_xon(plci);

  if (e->Ind) {
    if (e->user[0] &0x8000) {
      byte Ind = e->Ind & 0x0f;
      byte Ch = e->IndCh;
      if (((Ind==N_DISC) || (Ind==N_DISC_ACK)) &&
          (a->ch_flow_plci[Ch] == plci->Id)) {
        if (a->ch_flow_control[Ch] & N_RX_FLOW_CONTROL_MASK) {
          dbug(3,dprintf ("XDI CAPI: I: pending N-XON Ch:%02x", Ch));
        }
        a->ch_flow_control[Ch] &= ~N_RX_FLOW_CONTROL_MASK;
      }
      nl_ind(plci);
      if ((e->RNR != 1) &&
          (a->ch_flow_plci[Ch] == plci->Id) &&
          (a->ch_flow_control[Ch] & N_RX_FLOW_CONTROL_MASK)) {
        a->ch_flow_control[Ch] &= ~N_RX_FLOW_CONTROL_MASK;
        dbug(3,dprintf ("XDI CAPI: I: remove faked N-XON Ch:%02x", Ch));
      }
    } else {
      sig_ind(plci);
    }
    e->Ind = 0;
  }

capi_callback_suffix:

  while (!plci->req_in
   && !plci->internal_command
   && (plci->msg_in_write_pos != plci->msg_in_read_pos))
  {
    j = (plci->msg_in_read_pos == plci->msg_in_wrap_pos) ? 0 : plci->msg_in_read_pos;
    i = (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]))->header.length + (sizeof(void   *)-1)) & ~(sizeof(void   *)-1);
    m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]);
    appl = *((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j+i]));
    dbug(1,dprintf("dequeue msg(0x%04x) - write=%d read=%d wrap=%d",
      m->header.command, plci->msg_in_write_pos, plci->msg_in_read_pos, plci->msg_in_wrap_pos));
    if (plci->msg_in_read_pos == plci->msg_in_wrap_pos)
    {
      plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
      plci->msg_in_read_pos = i + MSG_IN_OVERHEAD;
    }
    else
    {
      plci->msg_in_read_pos = j + i + MSG_IN_OVERHEAD;
    }
    if (plci->msg_in_read_pos == plci->msg_in_write_pos)
    {
      plci->msg_in_write_pos = MSG_IN_QUEUE_SIZE;
      plci->msg_in_read_pos = MSG_IN_QUEUE_SIZE;
    }
    else if (plci->msg_in_read_pos == plci->msg_in_wrap_pos)
    {
      plci->msg_in_read_pos = MSG_IN_QUEUE_SIZE;
      plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
    }
    if (appl != NULL)
    {
      i = api_put_reenter (appl, m);
      if (i != 0)
      {
        if (m->header.command == _DATA_B3_R)
          TransmitBufferFree (appl, ULongToPtr(m->info.data_b3_req.Data));
        dbug(1,dprintf("Error 0x%04x from msg(0x%04x)", i, m->header.command));
        break;
      }
    }

    if (plci->li_notify_update == LI_NOTIFY_UPDATE_ENQUEUE)
    {
      plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
      mixer_notify_update (plci, FALSE);
    }

  }

  if (li_update_wait_list != NULL)
    mixer_update_done ();

  if (!plci->req_in && !plci->internal_command)
    send_data(plci);
  send_req(plci);
}


void control_rc(PLCI   * plci, byte req, byte rc, byte ch, byte global_req, byte nl_rc)
{
  dword Id;
  dword rId;
  word Number;
  word Info=0;
  word i;
  word ncci;
  DIVA_CAPI_ADAPTER   * a;
  APPL   * appl;
  PLCI   * rplci;
    byte SSparms[]  = "\x05\x00\x00\x02\x00\x00";
    byte SSstruct[] = "\x09\x00\x00\x06\x00\x00\x00\x00\x00\x00";

  if (!plci) {
    dbug(0,dprintf("A: control_rc, no plci %02x:%02x:%02x:%02x:%02x", req, rc, ch, global_req, nl_rc));
    return;
  }
  dbug(1,dprintf("req0_in/out=%d/%d",plci->req_in,plci->req_out));
  if(plci->req_in!=plci->req_out)
  {
    if (nl_rc || (global_req != ASSIGN) || (rc == ASSIGN_OK))
    {
      if ((req == RESERVE_REQ) && (rc != OK))
        resource_reservation_indication (plci, NULL);
      if ((global_req == ASSIGN) && nl_rc)
        plci->nl_assign_rc = rc;
      dbug(1,dprintf("req_1return"));
      return;
    }
    /* cancel outstanding request on the PLCI after SIG ASSIGN failure */
  }
  plci->req_in = plci->req_in_start = plci->req_out = 0;
  dbug(1,dprintf("control_rc"));

  appl = plci->appl;
  a = plci->adapter;
  ncci = a->ch_ncci[ch];
  if(appl)
  {
    Id = (((dword)(ncci ? ncci : ch)) << 16) | ((word)plci->Id << 8) | a->Id;
    if(plci->tel && plci->SuppState!=CALL_HELD) Id|=EXT_CONTROLLER;
    Number = plci->number;
    dbug(1,dprintf("Contr_RC-Id=%08lx,plci=%x,tel=%x, entity=0x%x, command=0x%x, int_command=0x%x",
      (unsigned long) Id, plci->Id, plci->tel, plci->Sig.Id, plci->command, plci->internal_command));
    dbug(1,dprintf("channels=0x%x",plci->channels));
    if ((global_req != ASSIGN) && plci_remove_check(plci))
    {
      trcq (plci);
      return;
    }
    trcq (plci);
    if(req==REMOVE && rc==ASSIGN_OK)
    {
      sig_req(plci,HANGUP,0);
      sig_req(plci,REMOVE,0);
      send_req(plci);
    }
    if(plci->command)
    {
      switch(plci->command)
      {
      case C_HOLD_REQ:
        dbug(1,dprintf("HoldRC=0x%x",rc));
        SSparms[1] = (byte)S_HOLD;
        if(rc!=OK)
        {
          plci->SuppState = IDLE;
          Info = 0x2001;
        }
        sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",Info,3,SSparms);
        break;

      case C_RETRIEVE_REQ:
        dbug(1,dprintf("RetrieveRC=0x%x",rc));
        SSparms[1] = (byte)S_RETRIEVE;
        if(rc!=OK)
        {
          plci->SuppState = CALL_HELD;
          Info = 0x2001;
        }
        sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",Info,3,SSparms);
        break;

      case _INFO_R:
        dbug(1,dprintf("InfoRC=0x%x",rc));
        if(rc!=OK) Info=_WRONG_STATE;
        sendf(appl,_INFO_R|CONFIRM,Id,Number,"w",Info);
        break;

      case _CONNECT_R:
        dbug(1,dprintf("Connect_R=0x%x/0x%x/0x%x/0x%x",req,rc,global_req,nl_rc));
        if (plci->State == INC_DIS_PENDING)
          break;
        if(plci->Sig.Id!=0xff)
        {
          if ((global_req == ASSIGN) && nl_rc)
            plci->nl_assign_rc = rc;
          if (((global_req == ASSIGN) && (rc != ASSIGN_OK))
           || (!nl_rc && (req == CALL_REQ) && (rc != OK))
           || (plci->nl_assign_rc != ASSIGN_OK))
          {
            dbug(1,dprintf("No more IDs/Call_Req failed"));
            if(plci->calledfromccbscall)
            {
              WRITE_WORD(&SSparms[1],S_CCBS_CALL);
              WRITE_WORD(&SSparms[4],_OUT_OF_PLCI);
              sendf(appl,_FACILITY_R|CONFIRM,Id&0xffL,Number,"wws",_OUT_OF_PLCI,(word)3,SSparms);
            }

            else if ((plci->nl_assign_rc == (ASSIGN_RC | WRONG_CH))
              && plci->appl
              && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
                & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
            {
              sendf(appl,_CONNECT_R|CONFIRM,Id&0xffL,Number,"w",INFO_RELATED_RESOURCE_ERROR);
            }

            else
            {
              sendf(appl,_CONNECT_R|CONFIRM,Id&0xffL,Number,"w",_OUT_OF_PLCI);
            }
            plci_remove(plci);
            break;
          }
          if(plci->State!=LOCAL_CONNECT)
          {
            plci->State = OUTG_CON_PENDING;
            trcq (plci);
          }
          if(plci->calledfromccbscall)
          {
            WRITE_WORD(&SSparms[1],S_CCBS_CALL);
            WRITE_WORD(&SSparms[4],0);
            sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",0,(word)3,SSparms);
          }
          else sendf(appl,_CONNECT_R|CONFIRM,Id,Number,"w",0);
        }
        else /* D-ch activation */
        {
          if (rc != ASSIGN_OK)
          {
            dbug(1,dprintf("No more IDs/X.25 Call_Req failed"));
            if(plci->calledfromccbscall)
            {
              WRITE_WORD(&SSparms[1],S_CCBS_CALL);
              WRITE_WORD(&SSparms[4],_OUT_OF_PLCI);
              sendf(appl,_FACILITY_R|CONFIRM,Id&0xffL,Number,"wws",_OUT_OF_PLCI,(word)3,SSparms);
            }
            else sendf(appl,_CONNECT_R|CONFIRM,Id&0xffL,Number,"w",_OUT_OF_PLCI);
            plci_remove(plci);
            break;
          }
          if(plci->calledfromccbscall)
          {
            WRITE_WORD(&SSparms[1],S_CCBS_CALL);
            WRITE_WORD(&SSparms[4],0);
            sendf(appl,_FACILITY_R|CONFIRM,Id,Number,"wws",0,(word)3,SSparms);
          }
          else sendf(appl,_CONNECT_R|CONFIRM,Id,Number,"w",0);
          sendf(plci->appl,_CONNECT_ACTIVE_I,Id,0,"sss","","","");
          plci->State = INC_ACT_PENDING;
          trcq (plci);
        }
        break;

      case _CONNECT_I|RESPONSE:
        if ((plci->State != IDLE) && (plci->State != INC_DIS_PENDING))
        {
          plci->State = INC_CON_ACCEPT;
          trcq (plci);
          if ((global_req == ASSIGN) && nl_rc)
            plci->nl_assign_rc = rc;
          if (plci->nl_assign_rc != ASSIGN_OK)
          {
            dbug(1,dprintf("connect_res(assign failed)"));
            sig_req(plci,HANGUP,0);
            send_req(plci);
          }
        }
        break;

      case _DISCONNECT_R:
        if ((plci->State == IDLE) || (plci->State == INC_DIS_PENDING))
          break;
        if(plci->Sig.Id!=0xff)
        {
          plci->State = OUTG_DIS_PENDING;
          sendf(appl,_DISCONNECT_R|CONFIRM,Id,Number,"w",0);
          trcq (plci);
        }
        break;

      case SUSPEND_REQ:
        break;

      case RESUME_REQ:
        break;

      case _CONNECT_B3_R:
        if(rc!=OK)
        {
          sendf(appl,_CONNECT_B3_R|CONFIRM,Id,Number,"w",_WRONG_IDENTIFIER);
          break;
        }
        ncci = get_ncci (plci, ch, 0);
        Id = (Id & 0xffff) | (((dword) ncci) << 16);
        plci->channels++;
        if(req==N_RESET)
        {
          a->ncci_state[ncci] = INC_ACT_PENDING;
          sendf(appl,_CONNECT_B3_R|CONFIRM,Id,Number,"w",0);
          sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
        }
        else
        {
          a->ncci_state[ncci] = OUTG_CON_PENDING;
          sendf(appl,_CONNECT_B3_R|CONFIRM,Id,Number,"w",0);
        }
        break;

      case _CONNECT_B3_I|RESPONSE:
        break;

      case _RESET_B3_R:
/*        sendf(appl,_RESET_B3_R|CONFIRM,Id,Number,"w",0);*/
        break;

      case _DISCONNECT_B3_R:
        sendf(appl,_DISCONNECT_B3_R|CONFIRM,Id,Number,"w",0);
        break;

      case _MANUFACTURER_R:
        if (plci->State == INC_DIS_PENDING)
          break;
        if (plci->m_command == _DI_ASSIGN_PLCI)
        {
          if (((global_req == ASSIGN) && (rc != ASSIGN_OK))
           || (!nl_rc && ((req == CALL_REQ) || (req == LISTEN_REQ)) && (rc != OK)))
          {

            if ((global_req == ASSIGN) && nl_rc && (rc == (ASSIGN_RC | WRONG_CH))
              && plci->appl
              && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
                & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
            {
              sendf(appl,_MANUFACTURER_R|CONFIRM,Id,Number,"dww",_DI_MANU_ID,plci->m_command,INFO_RELATED_RESOURCE_ERROR);
            }
            else

            {
              sendf(appl,_MANUFACTURER_R|CONFIRM,Id,Number,"dww",_DI_MANU_ID,plci->m_command,_OUT_OF_PLCI);
            }
            plci_remove(plci);
          }
          else
          {
            sendf(appl,_MANUFACTURER_R|CONFIRM,Id,Number,"dww",_DI_MANU_ID,plci->m_command,0);
          }
        }
        else if (plci->m_command == _DI_IDI_CTRL)
        {
          if (req == STATUS_REQ)
          {
            sendf(appl,_MANUFACTURER_R|CONFIRM,Id,Number,"dww",_DI_MANU_ID,plci->m_command,
                  (rc == OK) ? 0 : _WRONG_STATE);
          }
        }
        break;

      case PERM_LIST_REQ:
        if(rc!=OK)
        {
          Info = _WRONG_IDENTIFIER;
          if(plci->calledfromccbscall)
          {
            WRITE_WORD(&SSparms[1],S_CCBS_CALL);
            WRITE_WORD(&SSparms[4],Info);
            sendf(plci->appl,_FACILITY_R|CONFIRM,Id,Number,"wws",Info,(word)3,SSparms);
          }
          else sendf(plci->appl,_CONNECT_R|CONFIRM,Id,Number,"w",Info);
          plci_remove(plci);
        }
        else if(plci->calledfromccbscall)
        {
          WRITE_WORD(&SSparms[1],S_CCBS_CALL);
          WRITE_WORD(&SSparms[4],Info);
          sendf(plci->appl,_FACILITY_R|CONFIRM,Id,Number,"wws",Info,(word)3,SSparms);
        }
        else sendf(plci->appl,_CONNECT_R|CONFIRM,Id,Number,"w",Info);
        break;

      default:
        break;
      }
      plci->command = 0;
      if ((global_req == ASSIGN) && plci_remove_check(plci))
        return;
    }
    else if (plci->internal_command)
    {
      switch(plci->internal_command)
      {
      case BLOCK_PLCI:
        if (global_req == ASSIGN)
          plci_remove_check(plci);
        return;

      case GET_MWI_STATE:
        if(rc==OK) /* command supported, wait for indication */
        {
          if (global_req == ASSIGN)
            plci_remove_check(plci);
          return;
        }
        plci_remove(plci);
        break;

        /* Get Supported Services */
      case GETSERV_REQ_PEND:
        if(rc==OK) /* command supported, wait for indication */
        {
          break;
        }
        WRITE_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
        sendf(appl, _FACILITY_R|CONFIRM, Id, Number, "wws",0,3,SSstruct);
        plci_remove(plci);
        break;

      case INTERR_DIVERSION_REQ_PEND:      /* Interrogate Parameters        */
      case INTERR_NUMBERS_REQ_PEND:
      case CF_START_PEND:                  /* Call Forwarding Start pending */
      case CF_STOP_PEND:                   /* Call Forwarding Stop pending  */
      case CCBS_REQUEST_REQ_PEND:
      case CCBS_DEACTIVATE_REQ_PEND:
      case CCBS_INTERROGATE_REQ_PEND:
      case CCNR_REQUEST_REQ_PEND:
      case CCNR_INTERROGATE_REQ_PEND:
        switch(plci->internal_command)
        {
          case INTERR_DIVERSION_REQ_PEND:
            SSparms[1] = S_INTERROGATE_DIVERSION;
            break;
          case INTERR_NUMBERS_REQ_PEND:
            SSparms[1] = S_INTERROGATE_NUMBERS;
            break;
          case CF_START_PEND:
            SSparms[1] = S_CALL_FORWARDING_START;
            break;
          case CF_STOP_PEND:
            SSparms[1] = S_CALL_FORWARDING_STOP;
            break;
          case CCBS_REQUEST_REQ_PEND:
            SSparms[1] = S_CCBS_REQUEST;
            break;
          case CCBS_DEACTIVATE_REQ_PEND:
            SSparms[1] = S_CCBS_DEACTIVATE;
            break;
          case CCBS_INTERROGATE_REQ_PEND:
            SSparms[1] = S_CCBS_INTERROGATE;
            break;
          case CCNR_REQUEST_REQ_PEND:
            SSparms[1] = S_CCNR_REQUEST;
            break;
          case CCNR_INTERROGATE_REQ_PEND:
            SSparms[1] = S_CCNR_INTERROGATE;
            break;
        }
        if(global_req==ASSIGN)
        {
          dbug(1,dprintf("AssignDiversion_RC=0x%x/0x%x",req,rc));
          plci_remove_check(plci);
          return;
        }
        if(!plci->appl) break;
        if(rc==ISDN_GUARD_REJ)
        {
          Info = _CAPI_GUARD_ERROR;
        }
        else if(rc!=OK)
        {
          Info = _SUPPLEMENTARY_SERVICE_NOT_SUPPORTED;
        }
        sendf(plci->appl,_FACILITY_R|CONFIRM,Id&0x7fL,
              plci->number,"wws",Info,(word)3,SSparms);
        if(Info) plci_remove(plci);
        break;

        /* 3pty conference pending */
      case PTY_REQ_PEND:
        if(!plci->relatedPTYPLCI) break;
        rplci = plci->relatedPTYPLCI;
        SSparms[1] = plci->ptyState;
        rId = ((word)rplci->Id<<8)|rplci->adapter->Id;
        if(rplci->tel) rId|=EXT_CONTROLLER;
        if(rc!=OK)
        {
          Info = 0x300E; /* not supported */
          plci->relatedPTYPLCI = 0;
          plci->ptyState = 0;
        }
        if (plci->ptyAppl == NULL)
          plci->ptyAppl = rplci->appl;
        sendf(plci->ptyAppl,
              _FACILITY_R|CONFIRM,
              rId,
              plci->number,
              "wws",Info,(word)3,SSparms);
        break;

        /* Explicit Call Transfer pending */
      case ECT_REQ_PEND:
        dbug(1,dprintf("ECT_RC=0x%x/0x%x",req,rc));
        if(!plci->relatedPTYPLCI) break;
        rplci = plci->relatedPTYPLCI;
        SSparms[1] = S_ECT;
        rId = ((word)rplci->Id<<8)|rplci->adapter->Id;
        if(rplci->tel) rId|=EXT_CONTROLLER;
        if(rc!=OK)
        {
          Info = 0x300E; /* not supported */
          plci->relatedPTYPLCI = 0;
          plci->ptyState = 0;
        }
        if (plci->ptyAppl == NULL)
          plci->ptyAppl = rplci->appl;
        sendf(plci->ptyAppl,
              _FACILITY_R|CONFIRM,
              rId,
              plci->number,
              "wws",Info,(word)3,SSparms);
        break;

      case _MANUFACTURER_R:
        dbug(1,dprintf("_Manufacturer_R=0x%x/0x%x",req,rc));
        if ((global_req == ASSIGN) && (rc != ASSIGN_OK))
        {
          dbug(1,dprintf("No more IDs"));
          sendf(appl,_MANUFACTURER_R|CONFIRM,Id,Number,"dww",_DI_MANU_ID,plci->m_command,_OUT_OF_PLCI);
          plci_remove(plci);  /* after codec init, internal codec commands pending */
        }
        break;

      case _CONNECT_R:
        dbug(1,dprintf("_Connect_R=0x%x/0x%x",req,rc));
        if ((global_req == ASSIGN) && (rc != ASSIGN_OK))
        {
          dbug(1,dprintf("No more IDs"));
          if(plci->calledfromccbscall)
          {
            WRITE_WORD(&SSparms[1],S_CCBS_CALL);
            WRITE_WORD(&SSparms[4],_OUT_OF_PLCI);
            sendf(appl,_FACILITY_R|CONFIRM,Id&0xffL,Number,"wws",_OUT_OF_PLCI,(word)3,SSparms);
          }
          else sendf(appl,_CONNECT_R|CONFIRM,Id&0xffL,Number,"w",_OUT_OF_PLCI);
          plci_remove(plci);  /* after codec init, internal codec commands pending */
        }
        break;

      case PERM_COD_HOOK:                     /* finished with Hook_Ind */
        if (global_req == ASSIGN)
          plci_remove_check(plci);
        return;

      case PERM_COD_CALL:
        dbug(1,dprintf("***Codec Connect_Pending A, Rc = 0x%x",rc));
        if ((global_req == ASSIGN) && plci_remove_check(plci))
          return;
        plci->internal_command = PERM_COD_CONN_PEND;
        return;

      case PERM_COD_ASSIGN:
        dbug(1,dprintf("***Codec Assign A, Rc = 0x%x",rc));
        if ((global_req == ASSIGN) && (rc == ASSIGN_OK))
        {
          sig_req(plci,CALL_REQ,0);
          send_req(plci);
          plci->internal_command = PERM_COD_CALL;
          return;
        }
        break;

        /* Null Call Reference Request pending */
      case C_NCR_FAC_REQ:
        dbug(1,dprintf("NCR_FAC=0x%x/0x%x",req,rc));
        if(global_req==ASSIGN)
        {
          if(rc==ASSIGN_OK)
          {
            return;
          }
          else
          {
            sendf(appl,_INFO_R|CONFIRM,Id&0x7fL,Number,"w",_WRONG_STATE);
            appl->NullCREnable = FALSE;
            plci_remove(plci);
          }
        }
        else if(req==NCR_FACILITY)
        {
          if(rc==OK)
          {
            sendf(appl,_INFO_R|CONFIRM,Id&0x7fL,Number,"w",0);
          }
          else
          {
            sendf(appl,_INFO_R|CONFIRM,Id&0x7fL,Number,"w",_WRONG_STATE);
            appl->NullCREnable = FALSE;
          }
          plci_remove(plci);
        }
        break;

      case HOOK_ON_REQ:
        if(plci->channels)
        {
          if(a->ncci_state[ncci]==CONNECTED)
          {
            a->ncci_state[ncci] = OUTG_DIS_PENDING;
            cleanup_ncci_data (plci, ncci);
            nl_req_ncci(plci,N_DISC,ncci);
          }
          break;
        }
        break;

      case HOOK_OFF_REQ:
        if (plci->State == INC_DIS_PENDING)
          break;
        sig_req(plci,CALL_REQ,0);
        send_req(plci);
        plci->State=OUTG_CON_PENDING;
        break;


      case MWI_ACTIVATE_REQ_PEND:
      case MWI_DEACTIVATE_REQ_PEND:
        if(global_req == ASSIGN && rc==ASSIGN_OK)
        {
          dbug(1,dprintf("MWI_REQ assigned"));
          return;
        }
        else if(rc!=OK)
        {
          if(rc==WRONG_IE)
          {
            Info = 0x2007; /* Illegal message parameter coding */
            dbug(1,dprintf("MWI_REQ illegal parameter"));
          }
          else
          {
            Info = 0x300B; /* not supported */
            dbug(1,dprintf("MWI_REQ not supported"));
          }
          /* 0x3010: Request not allowed in this state */
          WRITE_WORD(&SSparms[4],0x300E); /* SS not supported */

        }
        if(plci->internal_command==MWI_ACTIVATE_REQ_PEND)
        {
          WRITE_WORD(&SSparms[1],S_MWI_ACTIVATE);
        }
        else WRITE_WORD(&SSparms[1],S_MWI_DEACTIVATE);

        if(plci->cr_enquiry)
        {
          sendf(plci->appl,
                _FACILITY_R|CONFIRM,
                Id&0x7fL,
                plci->number,
                "wws",Info,(word)3,SSparms);
          if(rc!=OK) plci_remove(plci);
        }
        else
        {
          sendf(plci->appl,
                _FACILITY_R|CONFIRM,
                Id,
                plci->number,
                "wws",Info,(word)3,SSparms);
        }
        break;

      case CONF_BEGIN_REQ_PEND:
      case CONF_ADD_REQ_PEND:
      case CONF_SPLIT_REQ_PEND:
      case CONF_DROP_REQ_PEND:
      case CONF_ISOLATE_REQ_PEND:
      case CONF_REATTACH_REQ_PEND:
        dbug(1,dprintf("CONF_RC=0x%x/0x%x",req,rc));
        if((plci->internal_command==CONF_ADD_REQ_PEND)&&(!plci->relatedPTYPLCI)) break;
        rplci = plci;
        rId = Id;
        switch(plci->internal_command)
        {
          case CONF_BEGIN_REQ_PEND:
            SSparms[1] = S_CONF_BEGIN;
            break;
          case CONF_ADD_REQ_PEND:
            SSparms[1] = S_CONF_ADD;
            rplci = plci->relatedPTYPLCI;
            rId = ((word)rplci->Id<<8)|rplci->adapter->Id;
            break;
          case CONF_SPLIT_REQ_PEND:
            SSparms[1] = S_CONF_SPLIT;
            break;
          case CONF_DROP_REQ_PEND:
            SSparms[1] = S_CONF_DROP;
            break;
          case CONF_ISOLATE_REQ_PEND:
            SSparms[1] = S_CONF_ISOLATE;
            break;
          case CONF_REATTACH_REQ_PEND:
            SSparms[1] = S_CONF_REATTACH;
            break;
        }

        if(rc!=OK)
        {
          Info = 0x300E; /* not supported */
          plci->relatedPTYPLCI = 0;
          plci->ptyState = 0;
        }
        if (plci->ptyAppl == NULL)
          plci->ptyAppl = rplci->appl;
        sendf(plci->ptyAppl,
              _FACILITY_R|CONFIRM,
              rId,
              plci->number,
              "wws",Info,(word)3,SSparms);
        break;

      case VSWITCH_REQ_PEND:
        if(rc!=OK)
        {
          if(plci->relatedPTYPLCI)
          {
            plci->relatedPTYPLCI->vswitchstate=0;
            plci->relatedPTYPLCI->vsprot=0;
            plci->relatedPTYPLCI->vsprotdialect=0;
          }
          plci->vswitchstate=0;
          plci->vsprot=0;
          plci->vsprotdialect=0;
        }
        else
        {
          if(plci->relatedPTYPLCI &&
             plci->vswitchstate==1 &&
             plci->relatedPTYPLCI->vswitchstate==3) /* join complete */
            plci->vswitchstate=3;
        }
        break;

        /* Call Deflection Request pending (SSCT) */
      case CD_REQ_PEND:
        SSparms[1] = S_CALL_DEFLECTION;
        if(rc!=OK)
        {
          Info = 0x300E; /* not supported */
          plci->appl->CDEnable = 0;
        }
        sendf(plci->appl,_FACILITY_R|CONFIRM,Id,
          plci->number,"wws",Info,(word)3,SSparms);
        break;
      case RESERVE_REQ:
        if (rc != OK)
          resource_reservation_indication (plci, NULL);
        break;

      case RTP_CONNECT_B3_REQ_COMMAND_2:
        if (rc == OK)
        {
          ncci = get_ncci (plci, ch, 0);
          Id = (Id & 0xffff) | (((dword) ncci) << 16);
          plci->channels++;
          a->ncci_state[ncci] = OUTG_CON_PENDING;
        }

      default:
        if (plci->internal_command_queue[0])
        {
          (*(plci->internal_command_queue[0]))(Id, plci, rc);
          if (plci->internal_command)
          {
            if (global_req == ASSIGN)
              plci_remove_check(plci);
            return;
          }
        }
        break;
      }
      if ((global_req == ASSIGN) && plci_remove_check(plci))
        return;
      next_internal_command (Id, plci);
    }
  }
  else /* appl==0 */
  {
    Id = ((word)plci->Id<<8)|plci->adapter->Id;
    if(plci->tel) Id|=EXT_CONTROLLER;
    trcq (plci);
    if ((req == RESERVE_REQ) && (rc != OK))
      resource_reservation_indication (plci, NULL);

    switch(plci->internal_command)
    {
    case BLOCK_PLCI:
      return;

    case START_L1_SIG_ASSIGN_PEND:
    case REM_L1_SIG_ASSIGN_PEND:
      if(global_req == ASSIGN)
      {
        break;
      }
      else
      {
        dbug(1,dprintf("***L1 Req rem PLCI"));
        plci->internal_command = 0;
        sig_req(plci,REMOVE,0);
        send_req(plci);
      }
      break;

      /* Call Deflection Request pending, just no appl ptr assigned */
    case CD_REQ_PEND:
      SSparms[1] = S_CALL_DEFLECTION;
      if(rc!=OK)
      {
        Info = 0x300E; /* not supported */
      }
      for(i=0; i<max_appl; i++)
      {
        if(application[i].CDEnable)
        {
          if(!application[i].Id) application[i].CDEnable = 0;
          else
          {
            sendf(&application[i],_FACILITY_R|CONFIRM,Id,
                  plci->number,"wws",Info,(word)3,SSparms);
            if(Info) application[i].CDEnable = 0;
          }
        }
      }
      plci->internal_command = 0;
      break;

    case PERM_COD_HOOK:                   /* finished with Hook_Ind */
      return;

    case PERM_COD_CALL:
      plci->internal_command = PERM_COD_CONN_PEND;
      dbug(1,dprintf("***Codec Connect_Pending, Rc = 0x%x",rc));
      return;

    case PERM_COD_ASSIGN:
      dbug(1,dprintf("***Codec Assign, Rc = 0x%x",rc));
      plci->internal_command = 0;
      if(rc!=ASSIGN_OK) break;
      plci->internal_command = PERM_COD_CALL;
      sig_req(plci,CALL_REQ,0);
      send_req(plci);
      return;

    case LISTEN_SIG_ASSIGN_PEND:
      if(rc == ASSIGN_OK)
      {
        plci->internal_command = 0;
        dbug(1,dprintf("ListenCheck, new SIG_ID = 0x%x",plci->Sig.Id));
        add_p_indicate(plci);
        sig_req(plci,INDICATE_REQ,0);
        send_req(plci);
      }
      else
      {
        dbug(1,dprintf("ListenCheck failed (assignRc=0x%x)",rc));
        a->listen_active--;
        plci_remove(plci);
        return;
      }
      break;

    case LISTEN_SIG_NCRL_ASSIGN_PEND:
      if(rc==ASSIGN_OK)
      {
        plci->internal_command = 0;
        dbug(1,dprintf("ListenCheck, new NCRL SIG_ID = 0x%x",plci->Sig.Id));
      }
      else
      {
        dbug(1,dprintf("ListenCheck NCRL failed (assignRc=0x%x)",rc));
        plci_remove(plci);
        return;
      }
      break;

    case USELAW_REQ:
      if(global_req == ASSIGN)
      {
        if (rc==ASSIGN_OK)
        {
          sig_req(plci,LAW_REQ,0);
          send_req(plci);
          dbug(1,dprintf("Auto-Law assigned"));
        }
        else
        {
          dbug(1,dprintf("Auto-Law assign failed"));
          a->automatic_law = 3;
          plci->internal_command = 0;
          a->automatic_lawPLCI = 0;
        }
        break;
      }
      else if(req == LAW_REQ && rc==OK)
      {
        dbug(1,dprintf("Auto-Law initiated"));
        a->automatic_law = 2;
        plci->internal_command = 0;
      }
      else
      {
        dbug(1,dprintf("Auto-Law not supported"));
        a->automatic_law = 3;
        plci->internal_command = 0;
        sig_req(plci,REMOVE,0);
        send_req(plci);
        a->automatic_lawPLCI = 0;
      }
      break;
    }
    plci_remove_check(plci);
  }
}

void data_rc(PLCI   * plci, byte ch, void   *data_ptr)
{
  dword Id;
  DIVA_CAPI_ADAPTER   * a;
  NCCI   *ncci_ptr;
  DATA_B3_DESC   *data;
  word ncci;

  if (plci->appl)
  {
    if (!plci->data_sent || (data_ptr != plci->data_sent_ptr))
      TransmitBufferFree (plci->appl, data_ptr);
    a = plci->adapter;
    ncci = a->ch_ncci[ch];
    if (ncci && (a->ncci_plci[ncci] == plci->Id))
    {
      ncci_ptr = &(a->ncci[ncci]);
      dbug(1,dprintf("data_out=%d, data_pending=%d",ncci_ptr->data_out,ncci_ptr->data_pending));
      if (ncci_ptr->data_pending)
      {
        data = &(ncci_ptr->DBuffer[ncci_ptr->data_out]);
        if (data->P == data_ptr)
        {
          if (!(data->Flags &4) && a->ncci_state[ncci])
          {
            Id = (((dword)ncci)<<16)|((word)plci->Id<<8)|a->Id;
            if(plci->tel) Id|=EXT_CONTROLLER;
            sendf(plci->appl,_DATA_B3_R|CONFIRM,Id,data->Number,
                  "ww",data->Handle,0);
          }
          (ncci_ptr->data_out)++;
          if (ncci_ptr->data_out == MAX_DATA_B3)
            ncci_ptr->data_out = 0;
          (ncci_ptr->data_pending)--;
        }
      }
    }
  }
}

void data_ack(PLCI   * plci, byte ch)
{
  dword Id;
  DIVA_CAPI_ADAPTER   * a;
  NCCI   *ncci_ptr;
  word ncci;

  a = plci->adapter;
  ncci = a->ch_ncci[ch];
  ncci_ptr = &(a->ncci[ncci]);
  if (ncci_ptr->data_ack_pending)
  {
    if (a->ncci_state[ncci] && (a->ncci_plci[ncci] == plci->Id))
    {
      Id = (((dword)ncci)<<16)|((word)plci->Id<<8)|a->Id;
      if(plci->tel) Id|=EXT_CONTROLLER;
      sendf(plci->appl,_DATA_B3_R|CONFIRM,Id,ncci_ptr->DataAck[ncci_ptr->data_ack_out].Number,
            "ww",ncci_ptr->DataAck[ncci_ptr->data_ack_out].Handle,0);
    }
    (ncci_ptr->data_ack_out)++;
    if (ncci_ptr->data_ack_out == MAX_DATA_ACK)
      ncci_ptr->data_ack_out = 0;
    (ncci_ptr->data_ack_pending)--;
  }
}

void sig_ind(PLCI   * plci)
{
  dword x_Id;
  dword Id;
  dword rId;
  word Number = 0;
  word i,j,k;
  word cip, reason;
  dword cip_mask;
  byte   *ie;
  DIVA_CAPI_ADAPTER   * a;
  static API_PARSE saved_parms[MAX_MSG_PARMS+1];
#define MAXPARMSIDS 32
  static byte   * parms[MAXPARMSIDS];
  static byte   * add_i[4];
  static byte   * multi_fac_parms[MAX_MULTI_IE];
  static byte   * multi_pi_parms [MAX_MULTI_IE];
  static byte   * multi_rdn_parms [MAX_MULTI_IE];
  static byte   * multi_iup_parms [MAX_MULTI_IE];
  static byte   * multi_ipu_parms [MAX_MULTI_IE];
  static byte   * multi_ssext_parms [MAX_MULTI_IE];
  static byte   * multi_CiPN_parms [MAX_MULTI_IE];

  static byte   * multi_vswitch_parms [MAX_MULTI_IE];

  static byte   * multi_esccai_parms [MAX_MULTI_IE];
  static byte   * multi_escoline_parms [MAX_MULTI_IE];
  byte ai_len;
    byte   *esc_chi = "";
    byte   *esc_law = "";
    byte   *pty_cai = "";
    byte   *esc_cr  = "";
    byte   *esc_smsg = "";
    byte   *esc_profile = "";

  static byte facility[256];
  PLCI   * tplci = 0;
  byte chi[] = "\x02\x18\x01";
  byte voice_cai[]  = "\x06\x14\x00\x00\x00\x00\x08";
    byte resume_cau[] = "\x05\x05\x00\x02\x00\x00";
  /* ESC_MSGTYPE must be the last but one message, a new IE has to be */
  /* included before the ESC_MSGTYPE and MAXPARMSIDS has to be incremented */
  /* SMSG is situated at the end because its 0 (for compatibility reasons */
  /* (see Info_Mask Bit 4, first IE. then the message type)           */
    word parms_id[1+MAXPARMSIDS] =
         {MAXPARMSIDS, CPN, SDNCMPL, DSA, OSA, BC, LLC, HLC, ESC_CAUSE, DSP, DT, CHA,
          UUI, CONG_RR, CONG_RNR, ESC_CHI, KEY, CHI, CAU, ESC_LAW,
          0xff, RDX, CONN_NR, RIN, NI, CAI, ESC_CR,
          CST, ESC_PROFILE, ESC_SMSG, 0xff, ESC_MSGTYPE, SMSG};
          /* 14 FTY repl by ESC_CHI */
          /* 18 PI  repl by ESC_LAW */
          /* 19 RDN moved to multi IE */
         /* removed OAD changed to 0xff for future use, OAD is multiIE now */
     word multi_iup_id[] = {1, IUP|0x600};
     word multi_ipu_id[] = {1, IPU|0x600};
     word multi_fac_id[] = {1, FTY};
     word multi_pi_id[]  = {1, PI};
     word multi_rdn_id[]  = {1, RDN};
     word multi_CiPN_id[]  = {1, OAD};
     word multi_ssext_id[]  = {1, ESC_SSEXT};

     word multi_vswitch_id[]  = {1, ESC_VSWITCH};

     word multi_esccai_id[]  = {1, ESC_CAI};
     word multi_escoline_id[]  = {1, ESC_OLINE|0x600};
  byte   * cau;
  word ncci;
    byte SS_Ind[] = "\x05\x02\x00\x02\x00\x00"; /* Hold_Ind struct*/
    byte CF_Ind[] = "\x09\x02\x00\x06\x00\x00\x00\x00\x00\x00";
    byte Interr_Err_Ind[] = "\x0a\x02\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
    byte CONF_Ind[] = "\x09\x16\x00\x06\x00\x00\0x00\0x00\0x00\0x00";
  byte VariableSS_Ind[] = "\x09\x00\x00\x06\x00\x00\x00\x00\x00\x00";
  byte force_mt_info = FALSE;
  byte dir, profile_change;
  dword d;
  word w;
  byte *p,*p1; /* general purpose pointer */
  byte redirectedind=0;

  a = plci->adapter;
  Id = ((word)plci->Id<<8)|a->Id;
  WRITE_WORD(&SS_Ind[4],0x0000);

  if (plci->sig_remove_id)
  {
    plci->Sig.RNR = 2; /* discard */
    dbug(1,dprintf("SIG discard while remove pending"));
    return;
  }
  if(plci->tel && plci->SuppState!=CALL_HELD) Id|=EXT_CONTROLLER;
  dbug(1,dprintf("SigInd-Id=%08lx,plci=%x,tel=%x,state=0x%x,channels=%d/%d,Discflowcl=%d",
    (unsigned long) Id, plci->Id, plci->tel, plci->State, plci->channels,
    (plci->channels && ((plci->channels > MAX_CHANNELS_PER_PLCI) || !plci->inc_dis_ncci_table[plci->channels - 1])),
    plci->hangup_flow_ctrl_timer));
  if (plci->Sig.Ind==CALL_HOLD_ACK
   && (plci->channels && ((plci->channels > MAX_CHANNELS_PER_PLCI) || !plci->inc_dis_ncci_table[plci->channels - 1])))
  {
    plci->Sig.RNR = 1;
    return;
  }
  if (plci->Sig.Ind==HANGUP
   && (plci->channels && ((plci->channels > MAX_CHANNELS_PER_PLCI) || !plci->inc_dis_ncci_table[plci->channels - 1])))
  {
    plci->Sig.RNR = 1;
    if (plci->hangup_flow_ctrl_timer == 0)
      (a->hangup_flow_control_plcis)++;
    else if (plci->hangup_flow_ctrl_timer == 3)
      (a->hangup_flow_control_plcis)--;
    plci->hangup_flow_ctrl_timer++;
    /* recover the network layer after timeout */
    if(plci->hangup_flow_ctrl_timer==100)
    {
      dbug(1,dprintf("Exceptional disc"));
      plci->Sig.RNR = 0;
      if (plci->hangup_flow_ctrl_timer <= 3)
        (a->hangup_flow_control_plcis)--;
      plci->hangup_flow_ctrl_timer = 0;
      for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
      {
        if (a->ncci_plci[ncci] == plci->Id)
        {
          cleanup_ncci_data (plci, ncci);
          if(plci->channels)plci->channels--;
          if (plci->appl)
            sendf(plci->appl,_DISCONNECT_B3_I, (((dword) ncci) << 16) | Id,0,"ws",0,"");
        }
      }
      if (plci->appl)
        sendf(plci->appl, _DISCONNECT_I, Id, 0, "w", 0);
      plci_remove(plci);
    }
    return;
  }
  if (plci->Sig.Ind==INDICATE_IND && a->hangup_flow_control_plcis)
  {
    if (!plci->notifiedcall)
    {
      plci->notifiedcall = 1;
      listen_check(a);
    }
    plci->Sig.RNR = 1;
    return;
  }
  if(plci->Sig.Ind==HANGUP &&
    plci->waitforECTindOnRPlci>=2 &&
    plci->relatedPTYPLCI &&
    plci->relatedPTYPLCI->waitforECTindOnRPlci==1 &&
    plci->relatedPTYPLCI->relatedPTYPLCI==plci)
  {   /* the primary/held-call has the value 1 and the secondary/consultation-call has >=2 */
      plci->Sig.RNR = 1;
      plci->waitforECTindOnRPlci++;
      /* 3 basic + wait 5 times (5*100ms) and then timeout */
      if(plci->waitforECTindOnRPlci>=(3+5))
      {
        plci->Sig.RNR = 0;
        plci->waitforECTindOnRPlci=0;
        plci->relatedPTYPLCI->waitforECTindOnRPlci=0;
      }
      else return;
  }

  /* search for REDIRECT_IE and extract header */
  if(plci->Sig.Ind==INDICATE_IND &&
      plci->Sig.RBuffer->P[0]==REDIRECT_IE &&
      plci->Sig.RBuffer->length>3)
  { /* test if a registered GCD-Application exist -> if not then reject the call */
      dbug(1,dprintf("RedirectIE"));
      for(i=0;i<max_appl;i++) if(a->requested_options_table[i]&(1L << PRIVATE_MADAPTER)) break;
      if(i<max_appl){
          dbug(1,dprintf("GCD App found[%d]",i));
          plci->Sig.RBuffer->length-=2;
          for(i=0;i<plci->Sig.RBuffer->length;i++) plci->Sig.RBuffer->P[i]=plci->Sig.RBuffer->P[i+2];
          redirectedind=1;
      }
      else{ /* reject */
          dbug(1,dprintf("no GCD App found"));
          sig_req(plci,HANGUP,0);
          send_req(plci);
          return;
      }
  }

  /* do first parse the info with no OAD in, because OAD will be converted */
  /* first the multiple facility IE, then mult. progress ind.              */
  /* then the parameters for the info_ind + conn_ind                       */
  IndParse(plci,multi_fac_id,multi_fac_parms,MAX_MULTI_IE);
  IndParse(plci,multi_pi_id,multi_pi_parms,MAX_MULTI_IE);
  IndParse(plci,multi_ssext_id,multi_ssext_parms,MAX_MULTI_IE);
  IndParse(plci,multi_iup_id,multi_iup_parms,MAX_MULTI_IE);
  IndParse(plci,multi_ipu_id,multi_ipu_parms,MAX_MULTI_IE);
  IndParse(plci,multi_rdn_id,multi_rdn_parms,MAX_MULTI_IE);

  IndParse(plci,multi_vswitch_id,multi_vswitch_parms,MAX_MULTI_IE);

  IndParse(plci,parms_id,parms,0);
  IndParse(plci,multi_CiPN_id,multi_CiPN_parms,MAX_MULTI_IE);
  IndParse(plci,multi_esccai_id,multi_esccai_parms,MAX_MULTI_IE);
  IndParse(plci,multi_escoline_id,multi_escoline_parms,MAX_MULTI_IE);
  esc_chi  = parms[14];
  esc_law  = parms[18];
  pty_cai  = parms[24];
  esc_cr   = parms[25];
  esc_profile = parms[27];
  esc_smsg = parms[28];
  if(esc_cr[0] && plci)
  {
    if(plci->cr_enquiry && plci->appl)
    {
      plci->cr_enquiry = FALSE;
      /* d = MANU_ID            */
      /* w = m_command          */
      /* b = total length       */
      /* b = indication type    */
      /* b = length of all IEs  */
      /* b = IE1                */
      /* S = IE1 length + cont. */
      /* b = IE2                */
      /* S = IE2 lenght + cont. */
      sendf(plci->appl,
        _MANUFACTURER_I,
        Id,
        0,
        "dwbbbbSbS",_DI_MANU_ID,plci->m_command,
        2+1+1+esc_cr[0]+1+1+esc_law[0],plci->Sig.Ind,1+1+esc_cr[0]+1+1+esc_law[0],ESC,esc_cr,ESC,esc_law);
    }
  }
  if(esc_smsg[0] && plci && (plci->Sig.Ind == INFO_IND))
  {
    if((plci->State != IDLE) && (plci->State != INC_DIS_PENDING) && plci->appl)
    {
      /* d = MANU_ID            */
      /* w = m_command          */
      /* b = total length       */
      /* b = indication type    */
      /* b = length of all IEs  */
      /* b = IE1                */
      /* S = IE1 length + cont. */
      sendf(plci->appl,
        _MANUFACTURER_I,
        Id,
        0,
        "dwbbbbS",_DI_MANU_ID,_DI_IDI_CTRL,
        2+1+1+esc_smsg[0],plci->Sig.Ind,1+1+esc_smsg[0],ESC,esc_smsg);
    }
  }
  /* create the additional info structure                                  */
  add_i[1] = parms[15]; /* KEY of additional info */
  add_i[2] = parms[11]; /* UUI of additional info */
  ai_len = AddInfo(add_i,multi_fac_parms, esc_chi, facility);

  profile_change = (byte)((a->automatic_law >= 4)        &&
                          (plci != a->automatic_lawPLCI) &&
                          (esc_law[0] != 0)              &&
                          (esc_profile[0] != 0)          &&
                          (plci != a->AdvCodecPLCI)      &&
                          (plci->internal_command == 0)  &&
                          (plci->Sig.Ind == INFO_IND)    &&
                          (plci->appl    == 0)           &&
                          (plci->State == LISTENING));
  if (profile_change != 0) {
    a->mtpx_adapter |= 0x02U; /* Request retrival of new hardware info from MTPX */
  }
  /* the ESC_LAW indicates if u-Law or a-Law is actually used by the card  */
  /* indication returns by the card if requested by the function           */
  /* AutomaticLaw() after driver init                                      */
  if ((a->automatic_law<4) || profile_change)
  {
    if(esc_law[0]){
      if(esc_law[2]){
        dbug(0,dprintf("u-Law selected"));
        a->u_law = 1;
      }
      else {
        dbug(0,dprintf("a-Law selected"));
        a->u_law = 0;
      }
      if (a->automatic_law<4)
      {
        a->automatic_law = 4;
        if(plci==a->automatic_lawPLCI) {
          plci->internal_command = 0;
          sig_req(plci,REMOVE,0);
          send_req(plci);
          a->automatic_lawPLCI = 0;
        }
      }
    }
    if (esc_profile[0])
    {
      dbug (1, dprintf ("[%06lx] CardProfile: %lx %lx %lx %lx %lx %lx",
        (unsigned long)(UnMapController (a->Id)), (unsigned long)(READ_DWORD (&esc_profile[6])),
        (unsigned long)(READ_DWORD (&esc_profile[10])), (unsigned long)(READ_DWORD (&esc_profile[14])),
        (unsigned long)(READ_DWORD (&esc_profile[18])), (unsigned long)(READ_DWORD (&esc_profile[46])),
        (unsigned long)(READ_DWORD (&esc_profile[58]))));

      a->profile.Global_Options = READ_DWORD (&esc_profile[6]) |
        GL_BCHANNEL_OPERATION_SUPPORTED;
      a->profile.B1_Protocols = READ_DWORD (&esc_profile[10]);
      a->profile.B2_Protocols = READ_DWORD (&esc_profile[14]);
      a->profile.B3_Protocols = READ_DWORD (&esc_profile[18]);
      d = READ_DWORD (&esc_profile[46]);
      if (profile_change)
      {
#define PROFILE_CHANGE_STATIC_MANUFACTURER_FEATURES  (MANUFACTURER_FEATURE_MIXER_CH_CH |  MANUFACTURER_FEATURE_MIXER_CH_PC |  MANUFACTURER_FEATURE_MIXER_PC_CH |  MANUFACTURER_FEATURE_MIXER_PC_PC |  MANUFACTURER_FEATURE_XCONNECT |  MANUFACTURER_FEATURE_DMACONNECT)
        a->manufacturer_features =
          (a->manufacturer_features & PROFILE_CHANGE_STATIC_MANUFACTURER_FEATURES) |
          (d & ~PROFILE_CHANGE_STATIC_MANUFACTURER_FEATURES);
      }
      else
      {
        a->manufacturer_features = d;
      }
      a->rtp_primary_payloads = READ_DWORD (&esc_profile[50]);
      a->rtp_additional_payloads = READ_DWORD (&esc_profile[54]);
      a->manufacturer_features2 = READ_DWORD (&esc_profile[58]);

      a->profile.Global_Options &= 0x000000ffL;
      a->profile.B1_Protocols &= 0x000003ffL;
      a->profile.B2_Protocols &= 0x00001fdfL;
      a->profile.B3_Protocols &= 0x000000bfL;

      a->profile.Manuf.private_options = 0;

      if (a->manufacturer_features & MANUFACTURER_FEATURE_ECHO_CANCELLER)
      {
        a->profile.Manuf.private_options |= 1L << PRIVATE_ECHO_CANCELLER;
        a->profile.Global_Options |= GL_ECHO_CANCELLER_SUPPORTED_OLD | GL_ECHO_CANCELLER_SUPPORTED_NEW;
      }


      if (a->manufacturer_features & MANUFACTURER_FEATURE_RTP)
        a->profile.Manuf.private_options |= 1L << PRIVATE_RTP;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_T38)
        a->profile.Manuf.private_options |= 1L << PRIVATE_T38;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_SUB_SEP_PWD)
        a->profile.Manuf.private_options |= 1L << PRIVATE_FAX_SUB_SEP_PWD;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_V18)
        a->profile.Manuf.private_options |= 1L << PRIVATE_V18;


      if ((a->manufacturer_features & MANUFACTURER_FEATURE_DTMF_TONE)
       || (a->manufacturer_features2 & MANUFACTURER_FEATURE2_SOFTIP))
      {
        a->profile.Manuf.private_options |= 1L << PRIVATE_DTMF_TONE;
      }


      if (a->manufacturer_features & MANUFACTURER_FEATURE_PIAFS)
        a->profile.Manuf.private_options |= 1L << PRIVATE_PIAFS;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_SS7) {
        a->profile.Manuf.private_options |= 1L << PRIVATE_SS7;
      }


      if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_LISTENING)
        a->profile.Manuf.private_options |= 1L << PRIVATE_LISTENING;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
        a->profile.Manuf.private_options |= 1L << PRIVATE_FAX_PAPER_FORMATS;


      if (a->manufacturer_features & MANUFACTURER_FEATURE_VOWN)
        a->profile.Manuf.private_options |= 1L << PRIVATE_VOWN;


      a->profile.Manuf.private_options |= 1L << PRIVATE_FAX_NONSTANDARD;


      if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_X25_REVERSE_RESTART)
        a->profile.Manuf.private_options |= 1L << PRIVATE_X25_REVERSE_RESTART;


      /* MADAPTER FEATURES */
      if (a->manufacturer_features & MANUFACTURER_FEATURE_MADAPTER)
        a->profile.Manuf.private_options |= 1L << PRIVATE_MADAPTER;
      /* Vendor specific extensions */
      if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_VENDOR_MASK) {
        dword vendor_id =
          (a->manufacturer_features2 >> MANUFACTURER_FEATURE2_VENDOR_MASK_FIRST_BIT) &
          MANUFACTURER_FEATURE2_VENDOR_BITS;
        a->profile.Manuf.private_options |= (vendor_id << PRIVATE_VENDOR_BIT_0);
      }
    }
    else
    {
      a->profile.Global_Options &= 0x0000007fL;
      a->profile.B1_Protocols &= 0x000003dfL;
      a->profile.B2_Protocols &= 0x00001fdfL;
      a->profile.B3_Protocols &= 0x000000bfL;
      a->manufacturer_features &= MANUFACTURER_FEATURE_HARDDTMF;
    }
    if (a->manufacturer_features & (MANUFACTURER_FEATURE_HARDDTMF |
      MANUFACTURER_FEATURE_SOFTDTMF_SEND | MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE))
    {
      a->profile.Global_Options |= GL_DTMF_SUPPORTED;
    }
    a->manufacturer_features &= ~MANUFACTURER_FEATURE_OOB_CHANNEL;
    dbug (1, dprintf ("[%06lx] Profile: %lx %lx %lx %lx %lx %lx",
      (unsigned long)(UnMapController (a->Id)), (unsigned long)(a->profile.Global_Options),
      (unsigned long)(a->profile.B1_Protocols), (unsigned long)(a->profile.B2_Protocols),
      (unsigned long)(a->profile.B3_Protocols), (unsigned long)(a->manufacturer_features),
      (unsigned long)(a->manufacturer_features2)));
  }

  /* codec plci for the handset/hook state support is just an internal id  */
  if(plci!=a->AdvCodecPLCI)
  {
    if ((plci->State != IDLE) && (plci->State != INC_DIS_PENDING))
    {
      force_mt_info =  SendMultiIE(plci,Id,multi_fac_parms, FTY, 0x20, NULL, redirectedind, 0);
      force_mt_info |= SendMultiIE(plci,Id,multi_pi_parms, PI, 0x210, NULL, redirectedind, 0);
      force_mt_info |= SendMultiIE(plci,Id,multi_rdn_parms, RDN, 0x400, NULL, redirectedind, 0);
      force_mt_info |= SendMultiIE(plci,Id,multi_iup_parms, IUP, 0x400, NULL, redirectedind, 6);
      force_mt_info |= SendMultiIE(plci,Id,multi_ipu_parms, IPU, 0x400, NULL, redirectedind, 6);
      force_mt_info |= SendSSExtInd(0,plci,Id,multi_ssext_parms);
      SendInfo(plci,Id, parms, force_mt_info, redirectedind);
    }

    VSwitchReqInd(plci,Id,multi_vswitch_parms);

  }

  /* switch the codec to the b-channel                                     */
  if(esc_chi[0] && plci && !plci->SuppState){
    plci->b_channel = esc_chi[esc_chi[0]]&0x1f;
    mixer_set_bchannel_id_esc (plci, plci->b_channel);
    dbug(1,dprintf("storeChannel=0x%x",plci->b_channel));
    if(plci->tel==ADV_VOICE && plci->appl) {
      SetVoiceChannel(a->AdvCodecPLCI, esc_chi, a);
    }
  }

  if(plci->appl) Number = plci->appl->Number++;

  switch(plci->Sig.Ind) {
  /* Response to Get_Supported_Services request */
  case S_SUPPORTED:
    dbug(1,dprintf("S_Supported"));
    if(!plci->appl) break;
    if(pty_cai[0]==4)
    {
      WRITE_DWORD(&CF_Ind[6],READ_DWORD(&pty_cai[1]) );
    }
    else
    {
      WRITE_DWORD(&CF_Ind[6],MASK_TERMINAL_PORTABILITY | MASK_HOLD_RETRIEVE);
    }
    WRITE_WORD (&CF_Ind[1], 0);
    WRITE_WORD (&CF_Ind[4], 0);
    sendf(plci->appl,_FACILITY_R|CONFIRM,Id&0x7fL,plci->number, "wws",0,3,CF_Ind);
    plci_remove(plci);
    break;

  /* Supplementary Service rejected */
  case S_SERVICE_REJ:
    dbug(1,dprintf("S_Reject=0x%x",pty_cai[5]));
    if(!pty_cai[0]) break;
    switch (pty_cai[5])
    {
    case ECT_EXECUTE:
    case THREE_PTY_END:
    case THREE_PTY_BEGIN:
      if(!plci->relatedPTYPLCI) break;
      tplci = plci->relatedPTYPLCI;
      rId = ( (word)tplci->Id<<8)|tplci->adapter->Id;
      if(tplci->tel) rId|=EXT_CONTROLLER;
      if(pty_cai[2]!=0xff)
      {
        WRITE_WORD(&SS_Ind[4],0x3600|(word)pty_cai[2]);
      }
      else
      {
        WRITE_WORD(&SS_Ind[4],0x300E);
      }
      if(pty_cai[5]==ECT_EXECUTE)
      {
        plci->waitforECTindOnRPlci=0;
        tplci->waitforECTindOnRPlci=0;
        WRITE_WORD(&SS_Ind[1],S_ECT);

        if(plci->vswitchstate==3 && tplci->vswitchstate==3)
        {
          /* dont change the states because the D-Channel Bridging must remain working */
          sendf(tplci->appl,_FACILITY_I,rId,0,"ws",3, SS_Ind);
          break;
        }else{
          plci->vswitchstate=0;
          tplci->vswitchstate=0;
        }

      }
      else
      {
        WRITE_WORD(&SS_Ind[1],pty_cai[5]+3);
      }
      plci->relatedPTYPLCI = 0;
      plci->ptyState = 0;
      if (plci->ptyAppl == NULL)
        plci->ptyAppl = tplci->appl;
      sendf(plci->ptyAppl,_FACILITY_I,rId,0,"ws",3, SS_Ind);
      break;

    case CALL_DEFLECTION:
      if(pty_cai[2]!=0xff)
      {
        if(pty_cai[1]==0xaf){
          /* give the Q.931 cause to the application */
          WRITE_WORD(&SS_Ind[4],0x3400|(word)pty_cai[2]);
        }else{
          WRITE_WORD(&SS_Ind[4],0x3600|(word)pty_cai[2]);
        }
      }
      else
      {
        WRITE_WORD(&SS_Ind[4],0x300E);
      }
      WRITE_WORD(&SS_Ind[1],pty_cai[5]);
      if(plci->appl){
        sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
      }else{
        for(i=0; i<max_appl; i++)
        {
          if(application[i].CDEnable)
          {
            if(application[i].Id) sendf(&application[i],_FACILITY_I,Id,0,"ws",3, SS_Ind);
            application[i].CDEnable = FALSE;
          }
        }
      }
      break;

    case DEACTIVATION_DIVERSION:
    case ACTIVATION_DIVERSION:
    case DIVERSION_INTERROGATE_CFU:
    case DIVERSION_INTERROGATE_CFB:
    case DIVERSION_INTERROGATE_CFNR:
    case DIVERSION_INTERROGATE_NUM:
    case CCBS_REQUEST:
    case CCBS_DEACTIVATE:
    case CCBS_INTERROGATE:
    case CCNR_REQUEST:
    case CCNR_INTERROGATE:
      if(!plci->appl) break;
      if(pty_cai[2]!=0xff)
      {
        WRITE_WORD(&Interr_Err_Ind[4],0x3600|(word)pty_cai[2]);
      }
      else
      {
        WRITE_WORD(&Interr_Err_Ind[4],0x300E);
      }
      switch (pty_cai[5])
      {
        case DEACTIVATION_DIVERSION:
          dbug(1,dprintf("Deact_Div"));
          Interr_Err_Ind[0]=0x9;
          Interr_Err_Ind[3]=0x6;
          WRITE_WORD(&Interr_Err_Ind[1],S_CALL_FORWARDING_STOP);
          break;
        case ACTIVATION_DIVERSION:
          dbug(1,dprintf("Act_Div"));
          Interr_Err_Ind[0]=0x9;
          Interr_Err_Ind[3]=0x6;
          WRITE_WORD(&Interr_Err_Ind[1],S_CALL_FORWARDING_START);
          break;
        case DIVERSION_INTERROGATE_CFU:
        case DIVERSION_INTERROGATE_CFB:
        case DIVERSION_INTERROGATE_CFNR:
          dbug(1,dprintf("Interr_Div"));
          Interr_Err_Ind[0]=0xa;
          Interr_Err_Ind[3]=0x7;
          WRITE_WORD(&Interr_Err_Ind[1],S_INTERROGATE_DIVERSION);
          break;
        case DIVERSION_INTERROGATE_NUM:
          dbug(1,dprintf("Interr_Num"));
          Interr_Err_Ind[0]=0xa;
          Interr_Err_Ind[3]=0x7;
          WRITE_WORD(&Interr_Err_Ind[1],S_INTERROGATE_NUMBERS);
          break;
        case CCBS_REQUEST:
          dbug(1,dprintf("CCBS Request"));
          Interr_Err_Ind[0]=0xd;
          Interr_Err_Ind[3]=0xa;
          WRITE_WORD(&Interr_Err_Ind[1],S_CCBS_REQUEST);
          break;
        case CCBS_DEACTIVATE:
          dbug(1,dprintf("CCBS Deactivate"));
          Interr_Err_Ind[0]=0x9;
          Interr_Err_Ind[3]=0x6;
          WRITE_WORD(&Interr_Err_Ind[1],S_CCBS_DEACTIVATE);
          break;
        case CCBS_INTERROGATE:
          dbug(1,dprintf("CCBS Interrogate"));
          Interr_Err_Ind[0]=0xb;
          Interr_Err_Ind[3]=0x8;
          WRITE_WORD(&Interr_Err_Ind[1],S_CCBS_INTERROGATE);
          break;
        case CCNR_REQUEST:
          dbug(1,dprintf("CCNR Request"));
          Interr_Err_Ind[0]=0xd;
          Interr_Err_Ind[3]=0xa;
          WRITE_WORD(&Interr_Err_Ind[1],S_CCNR_REQUEST);
          break;
        case CCNR_INTERROGATE:
          dbug(1,dprintf("CCNR Interrogate"));
          Interr_Err_Ind[0]=0xb;
          Interr_Err_Ind[3]=0x8;
          WRITE_WORD(&Interr_Err_Ind[1],S_CCNR_INTERROGATE);
          break;
      }
      WRITE_DWORD(&Interr_Err_Ind[6],plci->appl->S_Handle);
      sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"ws",3, Interr_Err_Ind);
      plci_remove(plci);
      break;
    case ACTIVATION_MWI:
    case DEACTIVATION_MWI:
      if(plci->gothandle){
          VariableSS_Ind[0]=9;
          VariableSS_Ind[3]=6;
          WRITE_DWORD(&VariableSS_Ind[6],plci->S_Handle);
      }
      else {
          VariableSS_Ind[0]=5;
          VariableSS_Ind[3]=2;
      }
      if(pty_cai[5]==ACTIVATION_MWI)
      {
        WRITE_WORD(&VariableSS_Ind[1],S_MWI_ACTIVATE);
      }
      else WRITE_WORD(&VariableSS_Ind[1],S_MWI_DEACTIVATE);
      if(pty_cai[2]!=0xff)
      {
        WRITE_WORD(&VariableSS_Ind[4],0x3600|(word)pty_cai[2]);
      }
      else
      {
        WRITE_WORD(&VariableSS_Ind[4],0x300E);
      }
      if(plci->cr_enquiry)
      {
        sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"ws",3, VariableSS_Ind);
        plci_remove(plci);
      }
      else
      {
        sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, VariableSS_Ind);
      }
      break;
    case CONF_ADD: /* ERROR */
    case CONF_BEGIN:
    case CONF_DROP:
    case CONF_ISOLATE:
    case CONF_REATTACH:
      CONF_Ind[0]=9;
      CONF_Ind[3]=6;
      switch(pty_cai[5])
      {
      case CONF_BEGIN:
          WRITE_WORD(&CONF_Ind[1],S_CONF_BEGIN);
          plci->ptyState = 0;
          break;
      case CONF_DROP:
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          WRITE_WORD(&CONF_Ind[1],S_CONF_DROP);
          plci->ptyState = CONNECTED;
          break;
      case CONF_ISOLATE:
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          WRITE_WORD(&CONF_Ind[1],S_CONF_ISOLATE);
          plci->ptyState = CONNECTED;
          break;
      case CONF_REATTACH:
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          WRITE_WORD(&CONF_Ind[1],S_CONF_REATTACH);
          plci->ptyState = CONNECTED;
          break;
      case CONF_ADD:
          WRITE_WORD(&CONF_Ind[1],S_CONF_ADD);
          plci->relatedPTYPLCI = 0;
          tplci=plci->relatedPTYPLCI;
          if(tplci) tplci->ptyState = CONNECTED;
          plci->ptyState = CONNECTED;
          break;
      }

      if(pty_cai[2]!=0xff)
      {
        WRITE_WORD(&CONF_Ind[4],0x3600|(word)pty_cai[2]);
      }
      else
      {
        WRITE_WORD(&CONF_Ind[4],0x3303); /* Time-out: network did not respond
                                            within the required time */
      }

      WRITE_DWORD(&CONF_Ind[6],0x0);
      if (plci->ptyAppl == NULL)
        plci->ptyAppl = plci->appl;
      sendf(plci->ptyAppl,_FACILITY_I,Id,0,"ws",3, CONF_Ind);
      break;
    }
    break;

  /* Supplementary Service indicates success */
  case S_SERVICE:
    dbug(1,dprintf("Service_Ind"));
    WRITE_WORD (&CF_Ind[4], 0);
    switch (pty_cai[5])
    {
    case THREE_PTY_END:
    case THREE_PTY_BEGIN:
    case ECT_EXECUTE:
      if(!plci->relatedPTYPLCI) break;
      tplci = plci->relatedPTYPLCI;
      rId = ( (word)tplci->Id<<8)|tplci->adapter->Id;
      if(tplci->tel) rId|=EXT_CONTROLLER;
      if (plci->ptyAppl == NULL)
        plci->ptyAppl = tplci->appl;
      if(pty_cai[5]==ECT_EXECUTE)
      {
        plci->waitforECTindOnRPlci=0;
        tplci->waitforECTindOnRPlci=0;
        WRITE_WORD(&SS_Ind[1],S_ECT);

        if(plci->vswitchstate!=3)
        {

            plci->ptyState = IDLE;
            plci->relatedPTYPLCI = 0;
            plci->ptyState = 0;

        }

        dbug(1,dprintf("ECT OK"));
        sendf(plci->ptyAppl,_FACILITY_I,rId,0,"ws",3, SS_Ind);



      }
      else
      {
        switch (plci->ptyState)
        {
        case S_3PTY_BEGIN:
          plci->ptyState = CONNECTED;
          dbug(1,dprintf("3PTY ON"));
          break;

        case S_3PTY_END:
          plci->ptyState = IDLE;
          plci->relatedPTYPLCI = 0;
          plci->ptyState = 0;
          dbug(1,dprintf("3PTY OFF"));
          break;
        }
        WRITE_WORD(&SS_Ind[1],pty_cai[5]+3);
        sendf(plci->ptyAppl,_FACILITY_I,rId,0,"ws",3, SS_Ind);
      }
      break;

    case CALL_DEFLECTION:
      WRITE_WORD(&SS_Ind[1],pty_cai[5]);
      if(plci->appl){
        sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
      }else{
        for(i=0; i<max_appl; i++)
        {
          if(application[i].CDEnable)
          {
            if(application[i].Id) sendf(&application[i],_FACILITY_I,Id,0,"ws",3, SS_Ind);
            application[i].CDEnable = FALSE;
          }
        }
      }
      break;

    case DEACTIVATION_DIVERSION:
    case ACTIVATION_DIVERSION:
      if(!plci->appl) break;
      WRITE_WORD(&CF_Ind[1],pty_cai[5]+2);
      WRITE_DWORD(&CF_Ind[6],plci->appl->S_Handle);
      sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"ws",3, CF_Ind);
      plci_remove(plci);
      break;

    case DIVERSION_INTERROGATE_CFU:
    case DIVERSION_INTERROGATE_CFB:
    case DIVERSION_INTERROGATE_CFNR:
    case DIVERSION_INTERROGATE_NUM:
    case CCBS_REQUEST:
    case CCBS_DEACTIVATE:
    case CCBS_INTERROGATE:
    case CCNR_REQUEST:
    case CCNR_INTERROGATE:
      if(!plci->appl) break;
      switch (pty_cai[5])
      {
        case DIVERSION_INTERROGATE_CFU:
        case DIVERSION_INTERROGATE_CFB:
        case DIVERSION_INTERROGATE_CFNR:
          dbug(1,dprintf("Interr_Div"));
          WRITE_WORD(&pty_cai[1],S_INTERROGATE_DIVERSION);
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          break;
        case DIVERSION_INTERROGATE_NUM:
          dbug(1,dprintf("Interr_Num"));
          WRITE_WORD(&pty_cai[1],S_INTERROGATE_NUMBERS);
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          break;
        case CCBS_REQUEST:
          dbug(1,dprintf("CCBS Request"));
          WRITE_WORD(&pty_cai[1],S_CCBS_REQUEST);
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          break;
        case CCNR_REQUEST:
          dbug(1,dprintf("CCNR Request"));
          WRITE_WORD(&pty_cai[1],S_CCNR_REQUEST);
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          break;
        case CCBS_DEACTIVATE:
          dbug(1,dprintf("CCBS Deactivate"));
          WRITE_WORD(&pty_cai[1],S_CCBS_DEACTIVATE);
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          break;
        case CCBS_INTERROGATE:
          dbug(1,dprintf("CCBS Interrogate"));
          WRITE_WORD(&pty_cai[1],S_CCBS_INTERROGATE);
          goto CCNR_INTERROGATE_1;
        case CCNR_INTERROGATE:
          dbug(1,dprintf("CCNR Interrogate"));
          WRITE_WORD(&pty_cai[1],S_CCNR_INTERROGATE);
CCNR_INTERROGATE_1:
          pty_cai[3]=pty_cai[0]-3; /* Supplementary Service-specific parameter len */
          /* add CIP values */
          k=13; /* Index for length CCBS Interrogate Response */
          while(k<(pty_cai[12]+13)) /* pty_cai[12] length CCBS Instances */
          {
              i=k+3;
              j=i; /* CIP value */
              i+=2;
              p=&pty_cai[i]; /* struct BC */
              i+=pty_cai[i]+1; /* struct LLC */
              i+=pty_cai[i]+1; /* struct HLC */
              w=find_cip(a,p,&pty_cai[i]);
              WRITE_WORD(&pty_cai[j],w);
              k=k+pty_cai[k]+1;
          }
          break;
      }
      WRITE_WORD(&pty_cai[4],0); /* Supplementary Service Reason */
      WRITE_DWORD(&pty_cai[6],plci->appl->S_Handle);
      sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"wS",3, pty_cai);
      plci_remove(plci);
      break;

    case ACTIVATION_MWI:
    case DEACTIVATION_MWI:
      if(plci->gothandle){
          VariableSS_Ind[0]=9;
          VariableSS_Ind[3]=6;
          WRITE_DWORD(&VariableSS_Ind[6],plci->S_Handle);
      }
      else {
          VariableSS_Ind[0]=5;
          VariableSS_Ind[3]=2;
      }
      if(pty_cai[5]==ACTIVATION_MWI)
      {
        WRITE_WORD(&VariableSS_Ind[1],S_MWI_ACTIVATE);
      }
      else WRITE_WORD(&VariableSS_Ind[1],S_MWI_DEACTIVATE);
      WRITE_WORD(&VariableSS_Ind[4],0);
      if(plci->cr_enquiry)
      {
        sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"ws",3, VariableSS_Ind);
        plci_remove(plci);
      }
      else
      {
        sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, VariableSS_Ind);
      }
      break;
    case MWI_INDICATION:
      if(pty_cai[0]>=0x12)
      {
        WRITE_WORD(&pty_cai[3],S_MWI_INDICATE);
        pty_cai[2]=pty_cai[0]-2; /* len Parameter */
        pty_cai[5]=pty_cai[0]-5; /* Supplementary Service-specific parameter len */
        if(plci->appl && (a->Notification_Mask[plci->appl->Id-1]&SMASK_MWI))
        {
          if(plci->internal_command==GET_MWI_STATE) /* result on Message Waiting Listen */
          {
            sendf(plci->appl,_FACILITY_I,Id&0x7fL,0,"wS",3, &pty_cai[2]);
            plci_remove(plci);
            return;
          }
          else  sendf(plci->appl,_FACILITY_I,Id,0,"wS",3, &pty_cai[2]);
          pty_cai[0]=0;
        }
        else
        {
          for(i=0; i<max_appl; i++)
          {
            if(a->Notification_Mask[i]&SMASK_MWI)
            {
              sendf(&application[i],_FACILITY_I,Id&0x7fL,0,"wS",3, &pty_cai[2]);
              pty_cai[0]=0;
            }
          }
        }

        if(!pty_cai[0])
        { /* acknowledge */
          facility[2]= 0; /* returncode */
        }
        else facility[2]= 0xff;
      }
      else
      {
        /* reject */
        facility[2]= 0xff; /* returncode */
      }
      facility[0]= 2;
      facility[1]= MWI_RESPONSE; /* Function */
      add_p(plci,CAI,facility);
      add_p(plci,ESC,multi_ssext_parms[0]); /* remembered parameter -> only one possible */
      /* in the ESC_CAI we get an ID of the adapter where the original request comes from -
      we give it back and the MTPX can now find the right one - very important in case of combined adapter */
      add_p(plci,ESC,multi_esccai_parms[0]);
      if(multi_esccai_parms[0] && multi_esccai_parms[0][0]==5)
      {
        dbug(1,dprintf("MWI_RESPONSE ESC_CAI:%02x.%02x.%02x.%02x",
          multi_esccai_parms[0][2],multi_esccai_parms[0][3],
          multi_esccai_parms[0][4],multi_esccai_parms[0][5]));
        /* for(i=0;i<6;i++) dbug(1,dprintf("MWI_RESPONSE ESC_CAI:%x",multi_esccai_parms[0][i])); */
      }
      else dbug(1,dprintf("MWI_RESPONSE no ESC_CAI"));
      sig_req(plci,S_SERVICE,0);
      send_req(plci);
      plci->command = 0;
      next_internal_command (Id, plci);
      break;
    case CONF_ADD: /* OK */
    case CONF_BEGIN:
    case CONF_DROP:
    case CONF_ISOLATE:
    case CONF_REATTACH:
    case CONF_PARTYDISC:
      CONF_Ind[0]=9;
      CONF_Ind[3]=6;
      switch(pty_cai[5])
      {
      case CONF_BEGIN:
          WRITE_WORD(&CONF_Ind[1],S_CONF_BEGIN);
          if(pty_cai[0]==6)
          {
              d=pty_cai[6];
              WRITE_DWORD(&CONF_Ind[6],d); /* PartyID */
          }
          else
          {
              WRITE_DWORD(&CONF_Ind[6],0x0);
          }
          break;
      case CONF_ISOLATE:
          WRITE_WORD(&CONF_Ind[1],S_CONF_ISOLATE);
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          break;
      case CONF_REATTACH:
          WRITE_WORD(&CONF_Ind[1],S_CONF_REATTACH);
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          break;
      case CONF_DROP:
          WRITE_WORD(&CONF_Ind[1],S_CONF_DROP);
          CONF_Ind[0]=5;
          CONF_Ind[3]=2;
          break;
      case CONF_ADD:
          WRITE_WORD(&CONF_Ind[1],S_CONF_ADD);
          d=pty_cai[6];
          WRITE_DWORD(&CONF_Ind[6],d); /* PartyID */
          tplci=plci->relatedPTYPLCI;
          if(tplci) tplci->ptyState = CONNECTED;
          break;
      case CONF_PARTYDISC:
          CONF_Ind[0]=7;
          CONF_Ind[3]=4;
          WRITE_WORD(&CONF_Ind[1],S_CONF_PARTYDISC);
          d=pty_cai[6];
          WRITE_DWORD(&CONF_Ind[4],d); /* PartyID */
          break;
      }
      plci->ptyState = CONNECTED;
      if (plci->ptyAppl == NULL)
        plci->ptyAppl = plci->appl;
      sendf(plci->ptyAppl,_FACILITY_I,Id,0,"ws",3, CONF_Ind);
      break;
    case CCBS_INFO_RETAIN:
    case CCNR_INFO_RETAIN:
    case CCBS_ERASECALLLINKAGEID:
    case CCBS_STOP_ALERTING:
    case CCBS_STATUS:
    case CCBS_ERASE:
    case CCBS_B_FREE:
    case CCBS_REMOTE_USER_FREE:
      facility[0]=5;
      facility[3]=2;
      switch(pty_cai[5])
      {
      case CCBS_INFO_RETAIN:
        WRITE_WORD(&facility[1],S_CCBS_INFO_RETAIN);
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* PartyID */
        break;
      case CCNR_INFO_RETAIN:
        WRITE_WORD(&facility[1],S_CCNR_INFO_RETAIN);
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* PartyID */
        break;
      case CCBS_STOP_ALERTING:
        WRITE_WORD(&facility[1],S_CCBS_STOP_ALERTING);
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* PartyID */
        break;
      case CCBS_ERASECALLLINKAGEID:
        WRITE_WORD(&facility[1],S_CCBS_ERASECALLLINKAGEID);
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* PartyID */
        i=6;k=8;
        /* Called Party Number */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* Called Party Subaddress */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k];
        facility[0]=(byte)i;
        facility[3]=facility[0]-3;
        break;
      case CCBS_STATUS:
        WRITE_WORD(&facility[1],S_CCBS_STATUS);
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* CCSB Recall Mode */
        w=pty_cai[8];
        w+=pty_cai[9]<<8;
        WRITE_WORD(&facility[6],w); /* CCSB Reference */
        i=10;k=12;
        /* BC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        p=&facility[i];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* LLC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* HLC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        p1=&facility[i];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* CIP Value */
        w=find_cip(a,p,p1);
        WRITE_WORD(&facility[8],w);
        /* Called Party Number */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* Called Party Subaddress */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k];
        facility[0]=(byte)i;
        facility[3]=(byte)i-3;
        break;
      case CCBS_ERASE:
        WRITE_WORD(&facility[1],S_CCBS_ERASE);
        goto CCBS_1;
      case CCBS_B_FREE:
        WRITE_WORD(&facility[1],S_CCBS_B_FREE);
        goto CCBS_1;
      case CCBS_REMOTE_USER_FREE:
        WRITE_WORD(&facility[1],S_CCBS_REMOTE_USER_FREE);
CCBS_1:
        w=pty_cai[6];
        w+=pty_cai[7]<<8;
        WRITE_WORD(&facility[4],w); /* CCSB Recall Mode */
        w=pty_cai[8];
        w+=pty_cai[9]<<8;
        WRITE_WORD(&facility[6],w); /* CCSB Reference */
        i=8;k=10;
        if(pty_cai[5]==CCBS_ERASE)
        {
            w=pty_cai[k++];
            w+=pty_cai[k++]<<8;
            WRITE_WORD(&facility[i],w); /* CCBS Erase Reason */
            i+=2;
        }
        i+=2;k+=2; /* CIP Value */
        /* BC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        p=&facility[i];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* LLC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* HLC */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        p1=&facility[i];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* CIP Value */
        w=find_cip(a,p,p1);
        if(pty_cai[5]==CCBS_ERASE){ WRITE_WORD(&facility[10],w); }
        else{  WRITE_WORD(&facility[8],w); }
        /* Called Party Number */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* Called Party Subaddress */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* Address of B */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k]+1;
        k+=pty_cai[k]+1;
        /* Subaddress of B */
        for(j=0;j<pty_cai[k]+1;j++) facility[i+j]=pty_cai[k+j];
        i+=pty_cai[k];
        facility[0]=(byte)i;
        facility[3]=(byte)i-3;
        break;
      }
      if(pty_cai[5]==CCNR_INFO_RETAIN)
      {
          if(plci->appl && (a->Notification_Mask[plci->appl->Id-1]&SMASK_CCNR))
          {
            sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, facility);
          }
          else
          {
            for(i=0; i<max_appl; i++)
                if(a->Notification_Mask[i]&SMASK_CCNR)
                    sendf(&application[i],_FACILITY_I,Id&0x7fL,0,"ws",3, facility);
          }

      }
      else
      {
          if(plci->appl && (a->Notification_Mask[plci->appl->Id-1]&SMASK_CCBS))
          {
            sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, facility);
          }
          else
          {
            for(i=0; i<max_appl; i++)
                if(a->Notification_Mask[i]&SMASK_CCBS)
                {
                    sendf(&application[i],_FACILITY_I,Id&0x7fL,0,"ws",3, facility);
                    /* remember the indication that we can find the PLCI for the response */
                    if(pty_cai[5]==CCBS_STATUS)
                    {
                      plci->ptyState=CCBS_STATUS;
                      /* in the ESC_CAI we get an ID of the adapter where the original request comes from -
                      we give it back and the MTPX can now find the right one - very important in case of combined adapter -
                      we store it in the plci and if we get the facility_res for the CCBS_STATUS we use it */
                      if(multi_esccai_parms[0] && multi_esccai_parms[0][0]==5)
                      {
                        dbug(1,dprintf("CCBS_STATUS ESC_CAI:%02x.%02x.%02x.%02x",
                          multi_esccai_parms[0][2],multi_esccai_parms[0][3],
                          multi_esccai_parms[0][4],multi_esccai_parms[0][5]));
                        for(j=0;j<(multi_esccai_parms[0][0]+1);j++)
                          plci->ncrl_esccai[j]=multi_esccai_parms[0][j];
                      }
                      else dbug(1,dprintf("CCBS_STATUS no ESC_CAI"));
                    }
                }
          }
      }
      break;
    case CCBS_IN_SET_CB:
    case CCBS_IN_CLEAR_CB:
      /* in the ESC_CAI we get an ID of the adapter where the original request comes from -
      the application must give it back so that the MTPX can now find the right one
      - very important in case of combined adapter - */
      if(!multi_esccai_parms[0])
      {
        dbug(1,dprintf("CCBS_IN no ESC_CAI")); break;
      }
      if(multi_esccai_parms[0][0]<5)
      {
        dbug(1,dprintf("CCBS_IN ESC_CAI too small:0x%x",multi_esccai_parms[0][0])); break;
      }
      /* build parameter string */
      for(j=0;j<(pty_cai[6]+1);j++) facility[j]=pty_cai[6+j];
      /* build additional id string */
      WRITE_WORD(&facility[j],SSIDENTIFIER);
      for(i=0;i<(multi_esccai_parms[0][0]+1);i++)
        facility[i+j+2]=multi_esccai_parms[0][i];
      
      facility[0]+=(multi_esccai_parms[0][0]+3); /* correct the overall length */
      
      for(i=0; i<max_appl; i++)
      {
        if(a->Notification_Mask[i]&SMASK_CCBS)
        {
          sendf(&application[i],_MANUFACTURER_I,Id&0x7fL,0,"dwS",_DI_MANU_ID,_DI_SSEXT_CTRL,facility);
        }
      }
      break;
    }
    break;


  case CALL_HOLD: /* Call Hold indication, probably in NT mode */
    if(plci->SuppState == IDLE)
    {
      plci->SuppState = HOLD_INDICATE;
      dbug(1,dprintf("Hold_Indicate"));
      sig_req(plci,CALL_HOLD_ACK,0); /* results in B3 disconnect */
      send_req(plci);
    }
    else
    {
      sig_req(plci,CALL_HOLD_REJ,0);
      send_req(plci);
    }
    break;

  case CALL_HOLD_COMPLETE: /* Call Hold indication, probably in NT mode */
    dbug(1,dprintf("Hold_Complete"));
    if(plci->SuppState == HOLD_INDICATE)
    {
      plci->SuppState = CALL_HELD;
      CodecIdCheck(a, plci);
      start_internal_command (Id, plci, hold_save_command);
    }
    break;

  case CALL_RETRIEVE: /* Call Retrieve indication, probably in NT mode */
    if(plci->SuppState == CALL_HELD)
    {
      plci->SuppState = RETRIEVE_INDICATION;
      sig_req(plci,CALL_RETRIEVE_ACK,0);
      send_req(plci);
    }
    break;

  case CALL_RETRIEVE_COMPLETE: /* Call Retrieve indication, probably in NT mode */
      plci->SuppState = IDLE;
      plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;
      plci->b_channel = esc_chi[esc_chi[0]]&0x1f;
      dbug(1,dprintf("Retrieve Complete"));
      if(plci->tel)
      {
        mixer_set_bchannel_id_esc (plci, plci->b_channel);
        dbug(1,dprintf("RetrChannel=0x%x",plci->b_channel));
        SetVoiceChannel(a->AdvCodecPLCI, esc_chi, a);
        if(plci->B2_prot==B2_TRANSPARENT && plci->B3_prot==B3_TRANSPARENT)
        {
          dbug(1,dprintf("Get B-ch"));
          start_internal_command (Id, plci, retrieve_restore_command);
        }
        else
          sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
      }
      else
        start_internal_command (Id, plci, retrieve_restore_command);
    break;

  case CALL_HOLD_REJ:
    cau = parms[7];
    if(cau[0])
    {
      i = _L3_CAUSE | cau[2];
      if(cau[2]==0) i = 0x3603;
    }
    else
    {
      i = 0x3603;
    }
    WRITE_WORD(&SS_Ind[1],S_HOLD);
    WRITE_WORD(&SS_Ind[4],i);
    if(plci->SuppState == HOLD_REQUEST)
    {
      plci->SuppState = IDLE;
      sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
    }
    break;

  case CALL_HOLD_ACK:
    if(plci->SuppState == HOLD_REQUEST)
    {
      plci->SuppState = CALL_HELD;
      CodecIdCheck(a, plci);
      start_internal_command (Id, plci, hold_save_command);
    }
    break;

  case CALL_RETRIEVE_REJ:
    cau = parms[7];
    if(cau[0])
    {
      i = _L3_CAUSE | cau[2];
      if(cau[2]==0) i = 0x3603;
    }
    else
    {
      i = 0x3603;
    }
    WRITE_WORD(&SS_Ind[1],S_RETRIEVE);
    WRITE_WORD(&SS_Ind[4],i);
    if(plci->SuppState == BLIND_RETRIEVE_REQUEST) /* received retrieve_rej in alerting state */
    {
      plci->State = INC_CON_ALERT;
      plci->SuppState = IDLE;
      CodecIdCheck(a, plci);
      sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
      trcq (plci);
    }
    else if(plci->SuppState == RETRIEVE_REQUEST)
    {
      plci->SuppState = CALL_HELD;
      CodecIdCheck(a, plci);
      sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
    }
    break;

  case CALL_RETRIEVE_ACK:
    WRITE_WORD(&SS_Ind[1],S_RETRIEVE);

    if(plci->SuppState == BLIND_RETRIEVE_REQUEST)
    {
      plci->b_channel = esc_chi[esc_chi[0]]&0x1f;
      plci->SuppState = IDLE;
      sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
    }
    else if(plci->SuppState == RETRIEVE_REQUEST)
    {
      plci->SuppState = IDLE;
      plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;
      plci->b_channel = esc_chi[esc_chi[0]]&0x1f;
      if(plci->tel)
      {
        mixer_set_bchannel_id_esc (plci, plci->b_channel);
        dbug(1,dprintf("RetrChannel=0x%x",plci->b_channel));
        SetVoiceChannel(a->AdvCodecPLCI, esc_chi, a);
        if(plci->B2_prot==B2_TRANSPARENT && plci->B3_prot==B3_TRANSPARENT)
        {
          dbug(1,dprintf("Get B-ch"));
          start_internal_command (Id, plci, retrieve_restore_command);
        }
        else
          sendf(plci->appl,_FACILITY_I,Id,0,"ws",3, SS_Ind);
      }
      else
        start_internal_command (Id, plci, retrieve_restore_command);
    }
    break;

  case INDICATE_IND:
    if(plci->State != LISTENING) {
      sig_req(plci,HANGUP,0);
      send_req(plci);
      break;
    }
    cip = find_cip(a,parms[4],parms[6]);
    cip_mask = (1L<<cip) | 1;
    dbug(1,dprintf("cip=%d,cip_mask=%lx,valid_cip=%lx",cip,
      (unsigned long) cip_mask, (unsigned long)(a->ValidServicesCIPMask)));
    clear_c_ind_mask (plci);
    if (!remove_started && !a->adapter_disabled)
    {
      set_c_ind_mask_bit (plci, MAX_APPL);
      for(i=0; i<max_appl; i++)
      {
        if(application[i].Id
        && (a->CIP_Mask[i] & cip_mask)
        && ((((a->ValidServicesCIPMask == 0) || (cip_mask & a->ValidServicesCIPMask)) /* If restricted by configuration */
          && CPN_filter_ok(parms[0],a,i))
         || (application[i].access & DIVA_CAPI_APPL_VSCAPI_ACCESS))
        && ((redirectedind && (a->requested_options_table[i]&(1L << PRIVATE_MADAPTER)))
         || (!redirectedind && !(a->requested_options_table[i]&(1L << PRIVATE_MADAPTER)))))
        {
          set_c_ind_mask_bit (plci, i);
        }
      }
      dump_c_ind_mask (plci);
      if (a->group_optimization_enabled)
      {
        group_optimization(plci);
        dump_appl_mask (plci->c_ind_mask_table, "group_mask");
      }
      for(i=0; i<max_appl; i++) {
        if(test_c_ind_mask_bit (plci, i)) {
          dbug(1,dprintf("storedcip_mask[%d]=0x%lx", i, (unsigned long)(a->CIP_Mask[i])));
          plci->State = INC_CON_PENDING;
          plci->call_dir = (plci->call_dir & ~(CALL_DIR_OUT | CALL_DIR_ORIGINATE)) |
            CALL_DIR_IN | CALL_DIR_ANSWER;
          if(esc_chi[0]) {
            plci->b_channel = esc_chi[esc_chi[0]]&0x1f;
            mixer_set_bchannel_id_esc (plci, plci->b_channel);
          }
          /* if a listen on the ext controller is done, check if hook states */
          /* are supported or if just a on board codec must be activated     */
          if(a->codec_listen[i] && !a->AdvSignalPLCI) {
            if(a->profile.Global_Options & GL_HANDSET_SUPPORTED)
              plci->tel = ADV_VOICE;
            else if(a->profile.Global_Options & GL_EXTERNAL_EQUIPMENT_SUPPORTED)
              plci->tel = CODEC;
            if(plci->tel) Id|=EXT_CONTROLLER;
            a->codec_listen[i] = plci;
          }

          sendf(&application[i],_CONNECT_I,Id,0,
                "wSSSSSSSbSSSSS", cip,    /* CIP                 */
                             parms[0],    /* CalledPartyNumber   */
                             multi_CiPN_parms[0],    /* CallingPartyNumber  */
                             parms[2],    /* CalledPartySubad    */
                             parms[3],    /* CallingPartySubad   */
                             parms[4],    /* BearerCapability    */
                             parms[5],    /* LowLC               */
                             parms[6],    /* HighLC              */
                             ai_len,      /* nested struct add_i */
                             add_i[0],    /* B channel info    */
                             add_i[1],    /* keypad facility   */
                             add_i[2],    /* user user data    */
                             add_i[3],    /* nested facility   */
                             multi_CiPN_parms[1]    /* second CiPN(SCR)   */
                             );
          SendSetupInfo(&application[i],
                        plci,
                        Id,
                        parms,
                        (byte)(SendSSExtInd(&application[i],plci,Id,multi_ssext_parms)|
                        SendMultiIE(plci,Id,multi_pi_parms, PI, 0x210, &application[i], redirectedind,0)|
                        SendMultiIE(plci,Id,multi_rdn_parms, RDN, 0x400, &application[i], redirectedind,0)|
                        SendMultiIE(plci,Id,multi_escoline_parms, ESC_OLINE, 0x400, &application[i], redirectedind,6)|
                        SendMultiIE(plci,Id,multi_iup_parms, IUP, 0x400, &application[i], redirectedind,6)|
                        SendMultiIE(plci,Id,multi_ipu_parms, IPU, 0x400, &application[i], redirectedind,6) ));
        }
      }
      clear_c_ind_mask_bit (plci, MAX_APPL);
    }
    if(c_ind_mask_empty (plci)) {
      sig_req(plci,HANGUP,0);
      send_req(plci);
      plci->State = IDLE;
    }
    else
      trcq (plci);
    plci->notifiedcall = 0;
    a->listen_active--;
    listen_check(a);
    break;

  case CALL_PEND_NOTIFY:
    plci->notifiedcall = 1;
    listen_check(a);
    break;

  case CALL_IND:
  case CALL_CON:
    if(plci->State==ADVANCED_VOICE_SIG || plci->State==ADVANCED_VOICE_NOSIG)
    {
      if(plci->internal_command==PERM_COD_CONN_PEND)
      {
        if(plci->State==ADVANCED_VOICE_NOSIG)
        {
          dbug(1,dprintf("***Codec OK"));
          if(a->AdvSignalPLCI)
          {
            tplci = a->AdvSignalPLCI;
            if(tplci->spoofed_msg)
            {
              dbug(1,dprintf("***Spoofed Msg(0x%x)",tplci->spoofed_msg));
              tplci->command = 0;
              tplci->internal_command = 0;
              x_Id = ((word)tplci->Id<<8)|tplci->adapter->Id | EXT_CONTROLLER;
              switch (tplci->spoofed_msg)
              {
              case CALL_RES:
                tplci->command = _CONNECT_I|RESPONSE;
                api_load_msg (&tplci->saved_msg, saved_parms);
                add_b1(tplci,&saved_parms[1],0,tplci->B1_facilities);
                if (tplci->adapter->Info_Mask[tplci->appl->Id-1] & 0x200)
                {
                  /* early B3 connect (CIP mask bit 9) no release after a disc */
                  add_p(tplci,LLI,"\x01\x01");
                }
                add_s(tplci, CONN_NR, &saved_parms[2]);
                add_s(tplci, LLC, &saved_parms[4]);
                add_ai(tplci, &saved_parms[5]);
                sig_req(tplci, CALL_RES,0);
                send_req(tplci);
                break;

              case AWAITING_SELECT_B:
                dbug(1,dprintf("Select_B continue"));
                start_internal_command (x_Id, tplci, select_b_protocol_command);
                break;

              case AWAITING_MANUF_CON: /* Get_Plci per Manufacturer_Req to ext controller */
                if(!tplci->Sig.Id)
                {
                  dbug(1,dprintf("No SigID!"));
                  sendf(tplci->appl, _MANUFACTURER_R|CONFIRM,x_Id,tplci->number, "dww",_DI_MANU_ID,plci->m_command,_OUT_OF_PLCI);
                  plci_remove(tplci);
                  break;
                }
                tplci->command = _MANUFACTURER_R;
                api_load_msg (&tplci->saved_msg, saved_parms);
                dir = saved_parms[2].info[0];
                if(dir==1) {
                  sig_req(tplci,CALL_REQ,0);
                }
                else if(!dir){
                  sig_req(tplci,LISTEN_REQ,0);
                }
                send_req(tplci);
                sendf(tplci->appl, _MANUFACTURER_R|CONFIRM,x_Id,tplci->number, "dww",_DI_MANU_ID,plci->m_command,0);
                break;

              case (CALL_REQ|AWAITING_MANUF_CON):
                sig_req(tplci,CALL_REQ,0);
                send_req(tplci);
                break;

              case CALL_REQ:
                if(!tplci->Sig.Id)
                {
                  dbug(1,dprintf("No SigID!"));
                  sendf(tplci->appl,_CONNECT_R|CONFIRM,tplci->adapter->Id,0,"w",_OUT_OF_PLCI);
                  plci_remove(tplci);
                  break;
                }
                tplci->command = _CONNECT_R;
                api_load_msg (&tplci->saved_msg, saved_parms);
                add_s(tplci,CPN,&saved_parms[1]);
                add_s(tplci,DSA,&saved_parms[3]);
                add_ai(tplci,&saved_parms[9]);
                sig_req(tplci,CALL_REQ,0);
                send_req(tplci);
                break;

              case CALL_RETRIEVE:
                tplci->command = C_RETRIEVE_REQ;
                sig_req(tplci,CALL_RETRIEVE,0);
                send_req(tplci);
                break;
              }
              tplci->spoofed_msg = 0;
              if (tplci->internal_command == 0)
                next_internal_command (x_Id, tplci);
            }
          }
          next_internal_command (Id, plci);
          break;
        }
        dbug(1,dprintf("***Codec Hook Init Req"));
        plci->internal_command = PERM_COD_HOOK;
        add_p(plci,FTY,"\x01\x09");             /* Get Hook State*/
        sig_req(plci,TEL_CTRL,0);
        send_req(plci);
      }
    }
    else if(((plci->command != _MANUFACTURER_R)
     || ((plci->m_command != _DI_ASSIGN_PLCI)
      && (plci->m_command != _DI_IDI_CTRL)))  /* old style permanent connect */
    && (plci->State!=INC_ACT_PENDING))
    {
      mixer_set_bchannel_id_esc (plci, plci->b_channel);
      if(plci->tel == ADV_VOICE && plci->SuppState == IDLE) /* with permanent codec switch on immediately */
      {
        chi[2] = plci->b_channel;
        SetVoiceChannel(a->AdvCodecPLCI, chi, a);
      }
      sendf(plci->appl,_CONNECT_ACTIVE_I,Id,0,"SsS",parms[21],"",parms[5]);
      plci->State = INC_ACT_PENDING;
      trcq (plci);
    }
    break;

  case TEL_CTRL:
    Number = 0;
    ie = multi_fac_parms[0]; /* inspect the facility hook indications */
    if(plci->State==ADVANCED_VOICE_SIG && ie[0]){
      switch (ie[1]&0x91) {
        case 0x80:   /* hook off */
        case 0x81:
          if(plci->internal_command==PERM_COD_HOOK)
          {
            dbug(1,dprintf("init:hook_off"));
            plci->hook_state = ie[1];
            next_internal_command (Id, plci);
            break;
          }
          else /* ignore doubled hook indications */
          {
            if( ((plci->hook_state)&0xf0)==0x80)
            {
              dbug(1,dprintf("ignore hook"));
              break;
            }
            plci->hook_state = ie[1]&0x91;
          }
          /* check for incoming call pending */
          /* and signal '+'.Appl must decide */
          /* with connect_res if call must   */
          /* accepted or not                 */
          for(i=0, tplci=0;i<max_appl;i++){
            if(a->codec_listen[i]
            && (a->codec_listen[i]->State==INC_CON_PENDING
              ||a->codec_listen[i]->State==INC_CON_ALERT) ){
              tplci = a->codec_listen[i];
              tplci->appl = &application[i];
              trcq (plci);
            }
          }
          /* no incoming call, do outgoing call */
          /* and signal '+' if outg. setup   */
          if(!a->AdvSignalPLCI && !tplci){
            if((i=get_plci(a))) {
              a->AdvSignalPLCI = &a->plci[i-1];
              tplci = a->AdvSignalPLCI;
              tplci->tel  = ADV_VOICE;
              WRITE_WORD(&voice_cai[5],a->AdvSignalAppl->MaxDataLength);
              if (a->Info_Mask[a->AdvSignalAppl->Id-1] & 0x200){
                /* early B3 connect (CIP mask bit 9) no release after a disc */
                add_p(tplci,LLI,"\x01\x01");
              }
              add_p(tplci, CAI, voice_cai);
              add_p(tplci, OAD, a->TelOAD);
              add_p(tplci, OSA, a->TelOSA);
              add_p(tplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              add_p(tplci,SHIFT|0x08|6,0); /* non-locking SHIFT, codeset 6 */
              add_p(tplci,SIN,"\x02\x01\x00");
              sig_req(tplci,ASSIGN,DSIG_ID);
              a->AdvSignalPLCI->internal_command = HOOK_OFF_REQ;
              a->AdvSignalPLCI->command = 0;
              tplci->appl = a->AdvSignalAppl;
              tplci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
              send_req(tplci);
            }

          }

          if(!tplci) break;
          Id = ((word)tplci->Id<<8)|a->Id;
          Id|=EXT_CONTROLLER;
          sendf(tplci->appl,
                _FACILITY_I,
                Id,
                0,
                "ws", (word)0, "\x01+");
          break;

        case 0x90:   /* hook on  */
        case 0x91:
          if(plci->internal_command==PERM_COD_HOOK)
          {
            dbug(1,dprintf("init:hook_on"));
            plci->hook_state = ie[1]&0x91;
            next_internal_command (Id, plci);
            break;
          }
          else /* ignore doubled hook indications */
          {
            if( ((plci->hook_state)&0xf0)==0x90) break;
            plci->hook_state = ie[1]&0x91;
          }
          /* hangup the adv. voice call and signal '-' to the appl */
          if(a->AdvSignalPLCI) {
            Id = ((word)a->AdvSignalPLCI->Id<<8)|a->Id;
            if(plci->tel) Id|=EXT_CONTROLLER;
            sendf(a->AdvSignalAppl,
                  _FACILITY_I,
                  Id,
                  0,
                  "ws", (word)0, "\x01-");
            a->AdvSignalPLCI->internal_command = HOOK_ON_REQ;
            a->AdvSignalPLCI->command = 0;
            sig_req(a->AdvSignalPLCI,HANGUP,0);
            send_req(a->AdvSignalPLCI);
          }
          break;
      }
    }
    break;

  case RESUME:
    clear_c_ind_mask_bit (plci, (word)(plci->appl->Id-1));
    WRITE_WORD(&resume_cau[4],GOOD);
    sendf(plci->appl,_FACILITY_I,Id,0,"ws", (word)3, resume_cau);
    break;

  case SUSPEND:
    clear_c_ind_mask (plci);

    if (plci->NL.Id && !plci->nl_remove_id) {
      mixer_remove (plci);
      nl_req_ncci(plci,REMOVE,0);
    }
    if (!plci->sig_remove_id) {
      plci->internal_command = 0;
      sig_req(plci,REMOVE,0);
    }
    send_req(plci);
    if(!plci->channels) {
      sendf(plci->appl,_FACILITY_I,Id,0,"ws", (word)3, "\x05\x04\x00\x02\x00\x00");
      sendf(plci->appl, _DISCONNECT_I, Id, 0, "w", 0);
    }
    break;

  case SUSPEND_REJ:
    break;

  case HANGUP:
    if ((plci->hangup_flow_ctrl_timer != 0) && (plci->hangup_flow_ctrl_timer <= 3))
      (a->hangup_flow_control_plcis)--;
    plci->hangup_flow_ctrl_timer=0;
    cau = parms[7];
    if (plci->nl_assign_rc != ASSIGN_OK) {
      reason = _L3_ERROR;

      if (plci->appl
       && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
         & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
      {
        if (plci->nl_assign_rc == (ASSIGN_RC | WRONG_CH))
          reason = REASON_RELATED_RESOURCE_ERROR;
      }

    }
    else if(cau[0]) {
      reason = _L3_CAUSE | cau[2];
      if(cau[2]==0) reason = 0;
      else if(cau[2]==8) reason = _L1_ERROR;
      else if(cau[2]==9 || cau[2]==10) reason = _L2_ERROR;
      else if(cau[2]==5) reason = _CAPI_GUARD_ERROR;
      else if(cau[2]==11) reason = _L3_ERROR;
      else if(cau[2]==12)
      {
        DBG_FTL(("[%06lx] %s,%d: FATAL out of resources",
          (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
          (char   *)(FILE_), __LINE__));

        if (plci->appl
         && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
           & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
        {
          reason = REASON_OUT_OF_B_RESOURCES;
        }
        else

        {
          reason = _L3_CAUSE | 0xa2; /* no channel available */
        }
      }
      else if(cau[2]==13)
      {
        DBG_FTL(("[%06lx] %s,%d: FATAL out of licenses",
          (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
          (char   *)(FILE_), __LINE__));

        if (plci->appl
         && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
           & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
        {
          reason = REASON_OUT_OF_B_LICENSES;
        }
        else

        {
          reason = _L3_CAUSE | 0xa2; /* no channel available */
        }
      }
    }
    else {
      reason = _L3_ERROR;
    }

    if(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT)
    {
      clear_incoming_call (plci, TRUE, reason);
    }
    else
    {
      clear_c_ind_mask (plci);
    }
    if(!plci->appl)
    {
      if (plci->State == LISTENING)
      {
        plci->notifiedcall=0;
        a->listen_active--;
      }
      if(c_ind_mask_empty (plci))
      {
        trcq (plci);
        if(plci->State!=IDLE && plci->State!=INC_DIS_PENDING && plci->State!=OUTG_DIS_PENDING)
        {
          plci->State = IDLE;
          mixer_remove (plci);
        }
        plci->State = IDLE;
        if (plci->NL.Id && !plci->nl_remove_id)
        {
          nl_req_ncci(plci,REMOVE,0);
        }
        if (!plci->sig_remove_id)
        {
          plci->internal_command = 0;
          sig_req(plci,REMOVE,0);
        }
        send_req(plci);
      }
      else
      {
        trcq (plci);
        if(plci->State!=IDLE && plci->State!=INC_DIS_PENDING && plci->State!=OUTG_DIS_PENDING)
        {
          plci->State = INC_DIS_PENDING;
          mixer_remove (plci);
        }
        plci->State = INC_DIS_PENDING;
      }
    }
    else
    {
        /* collision of DISCONNECT or CONNECT_RES with HANGUP can   */
        /* result in a second HANGUP! Don't generate another        */
        /* DISCONNECT                                               */
      if(plci->State!=IDLE && plci->State!=INC_DIS_PENDING)
      {
        if(plci->State==RESUMING)
        {
          WRITE_WORD(&resume_cau[4],reason);
          sendf(plci->appl,_FACILITY_I,Id,0,"ws", (word)3, resume_cau);
        }
        if (plci->State!=OUTG_DIS_PENDING)
        {
          plci->State = INC_DIS_PENDING;
          mixer_remove (plci);
        }
        plci->State = INC_DIS_PENDING;
        sendf(plci->appl,_DISCONNECT_I,Id,0,"w",reason);
        trcq (plci);
      }
      else
        trcq (plci);
    }
    init_internal_command_queue (plci);
    break;

  case SSEXT_IND:
    SendSSExtInd(0,plci,Id,multi_ssext_parms);
    break;

  case VSWITCH_REQ:
    VSwitchReqInd(plci,Id,multi_vswitch_parms);
    break;
  case VSWITCH_IND:
    if(plci->appl &&
       plci->State &&
       plci->relatedPTYPLCI &&
       plci->relatedPTYPLCI->State &&
       plci->relatedPTYPLCI->appl &&
       plci->vswitchstate==3 &&
       plci->relatedPTYPLCI->vswitchstate==3 &&
       parms[MAXPARMSIDS-1][0] &&
       plci->relatedPTYPLCI->relatedPTYPLCI==plci) /* Segmented Message */
    {
      add_p(plci->relatedPTYPLCI,SMSG,parms[MAXPARMSIDS-1]);
      sig_req(plci->relatedPTYPLCI,VSWITCH_REQ,0);
      send_req(plci->relatedPTYPLCI);
    }
    else VSwitchReqInd(plci,Id,multi_vswitch_parms);
    break;


  case RESERVE_IND:
    resource_reservation_indication (plci, esc_profile);
    break;
  }
}


static void SendSetupInfo(APPL   * appl, PLCI   * plci, dword Id, byte   * * parms, byte Info_Sent_Flag)
{
  word i;
  byte   * ie;
  word Info_Number;
  byte   * Info_Element;
  word Info_Mask = 0;
  int  delaymtinfoind = 0;

  dbug(1,dprintf("SetupInfo"));

  for(i=0; i<MAXPARMSIDS; i++) {
    ie = parms[i];
    Info_Number = 0;
    Info_Element = ie;
    if(ie[0]) {
      switch(i) {
      case 0:
        dbug(1,dprintf("CPN "));
        Info_Number = 0x0070;
        Info_Mask   = 0x80;
        break;
      case 1:
        dbug(1,dprintf("SDNCMPL "));
        Info_Number = 0x00A1;
        Info_Mask   = 0x1000;
        Info_Element = "";
        break;
      case 8:  /* display      */
        dbug(1,dprintf("display(%d)",i));
        Info_Number = 0x0028;
        Info_Mask = 0x04;
        break;
      case 16: /* Channel Id */
        dbug(1,dprintf("CHI"));
        Info_Number = 0x0018;
        Info_Mask = 0x100;
        mixer_set_bchannel_id (plci, Info_Element);
        break;
      case 17: /* special cau as sign that we should later send the MT INFO_IND */
        dbug(1,dprintf("CAU"));
        delaymtinfoind=1;
        break;
      case 20: /* Redirected Number extended */
        dbug(1,dprintf("RDX"));
        Info_Number = 0x0073;
        Info_Mask = 0x400;
        break;
      case 22: /* Redirecing Number  */
        dbug(1,dprintf("RIN"));
        Info_Number = 0x0076;
        Info_Mask = 0x400;
        break;
      default:
        Info_Number = 0;
        break;
      }
    }

    if((i==MAXPARMSIDS-2) && !delaymtinfoind)
    { /* to indicate the final message type "Setup" to avoid a timer requirement in the application */
      Info_Number = 0x8000 |5;
      if(plci->adapter->Info_Mask[appl->Id-1] & 0x05FF) Info_Mask = 0xffff; /* if the only bit 9 (earlyB3),    */
      else  Info_Mask = 0;                                                  /* or bit 11 (reserved)            */
      Info_Element = "";                                                    /* or bit 12 (reserved) is set,    */
    }                                                                       /* don't send final Setup Info_Ind */

    if(Info_Number){
      if(plci->adapter->Info_Mask[appl->Id-1] & Info_Mask) {
        sendf(appl,_INFO_I,Id,0,"wS",Info_Number,Info_Element);
        Info_Sent_Flag=TRUE;
      }
    }
  }
}


void SendInfo(PLCI   * plci, dword Id, byte   * * parms, byte iesent, byte redirectedind)
{ /* NOTE: if you add a single octet IE then add the IE in IndParse too!!! */
  word i;
  word j;
  word k;
  byte   * ie;
  word Info_Number;
  byte   * Info_Element;
  word Info_Mask = 0;
  static byte charges[5] = {4,0,0,0,0};
  static byte cause[] = {0x02,0x80,0x00};
  APPL   *appl;

  dbug(1,dprintf("InfoParse "));

  if(
      !plci->appl
      && !plci->State
      && plci->Sig.Ind!=NCR_FACILITY
    )
  {
    dbug(1,dprintf("NoParse "));
    return;
  }
  cause[2] = 0;
  for(i=0; i<MAXPARMSIDS; i++) {
    ie = parms[i];
    Info_Number = 0;
    Info_Element = ie;
    if(ie[0]) {
      switch(i) {
      case 0:
        dbug(1,dprintf("CPN "));
        Info_Number = 0x0070;
        Info_Mask   = 0x80;
        break;
      case 1:
        dbug(1,dprintf("SDNCMPL "));
        Info_Number = 0x00A1;
        Info_Mask   = 0x1000;
        Info_Element = "";
        break;
      case 7: /* ESC_CAU */
        dbug(1,dprintf("cau(0x%x)",ie[2]));
        Info_Number = 0x0008;
        Info_Mask = 0x00;
        cause[2] = ie[2];
        Info_Element = 0;
        break;
      case 8:  /* display      */
        dbug(1,dprintf("display(%d)",i));
        Info_Number = 0x0028;
        Info_Mask = 0x04;
        break;
      case 9:  /* Date display */
        dbug(1,dprintf("date(%d)",i));
        Info_Number = 0x0029;
        Info_Mask = 0x02;
        break;
      case 10: /* charges */
        for(j=0;j<4;j++) charges[1+j] = 0;
        for(j=0; j<ie[0] && !(ie[1+j]&0x80); j++);
        for(k=1,j++; j<ie[0] && k<=4; j++,k++) charges[k] = ie[1+j];
        Info_Number = 0x4000;
        Info_Mask = 0x40;
        Info_Element = charges;
        break;
      case 11: /* user user info */
        dbug(1,dprintf("uui"));
        Info_Number = 0x007E;
        Info_Mask = 0x08;
        break;
      case 12: /* congestion receiver ready */
        dbug(1,dprintf("clRDY"));
        Info_Number = 0x00B0;
        Info_Mask = 0x08;
        Info_Element = "";
        break;
      case 13: /* congestion receiver not ready */
        dbug(1,dprintf("clNRDY"));
        Info_Number = 0x00BF;
        Info_Mask = 0x08;
        Info_Element = "";
        break;
      case 15: /* Keypad Facility */
        dbug(1,dprintf("KEY"));
        Info_Number = 0x002C;
        Info_Mask = 0x20;
        break;
      case 16: /* Channel Id */
        dbug(1,dprintf("CHI"));
        Info_Number = 0x0018;
        Info_Mask = 0x100;
        mixer_set_bchannel_id (plci, Info_Element);
        break;
      case 17: /* if no 1tr6 cause, send full cause, else esc_cause */
        dbug(1,dprintf("q9cau(0x%x)",ie[2]));
        if(!cause[2] || cause[2]<0x80) break;  /* eg. layer 1 error */
        Info_Number = 0x0008;
        Info_Mask = 0x01;
        if(cause[2] != ie[2]) Info_Element = cause;
        break;
      case 20: /* Redirected Number extended */
        dbug(1,dprintf("RDX"));
        Info_Number = 0x0073;
        Info_Mask = 0x400;
        break;
      case 22: /* Redirecing Number  */
        dbug(1,dprintf("RIN"));
        Info_Number = 0x0076;
        Info_Mask = 0x400;
        break;
      case 23: /* Notification Indicator  */
        dbug(1,dprintf("NI"));
        Info_Number = (word)NI;
        Info_Mask = 0x210;
        break;
      case 26: /* Call State  */
        dbug(1,dprintf("CST"));
        Info_Number = (word)CST;
        Info_Mask = 0x01; /* do with cause i.e. for now */
        break;
      case MAXPARMSIDS-2:  /* Escape Message Type, must be the last indication */
        dbug(1,dprintf("ESC/MT[0x%x]",ie[3]));
        Info_Number = 0x8000 |ie[3];
        if(iesent) Info_Mask = 0xffff;
        else  Info_Mask = 0x10;
        Info_Element = "";
        break;
      default:
        Info_Number  = 0;
        Info_Mask    = 0;
        Info_Element = "";
        break;
      }
    }

    if(plci->Sig.Ind==NCR_FACILITY ||
      (plci->Sig.Ind==S_SERVICE && plci->State==LISTENING)) /* check controller broadcast */
    {
      for(j=0; j<max_appl; j++)
      {
        appl = &application[j];
        if(redirectedind && !(plci->adapter->requested_options_table[appl->Id-1]&(1L << PRIVATE_MADAPTER))) continue;
        if(Info_Number
        && appl->Id
        && plci->adapter->Info_Mask[appl->Id-1] &Info_Mask)
        {
          dbug(1,dprintf("NCR_Ind"));
          iesent=TRUE;
          sendf(&application[j],_INFO_I,Id&0x7fL,0,"wS",Info_Number,Info_Element);
        }
      }
    }
    else if(!plci->appl)
    { /* overlap receiving broadcast */
      if(Info_Number==CPN
      || Info_Number==KEY
      || Info_Number==NI
      || Info_Number==DSP
      || Info_Number==UUI
      || Info_Number==SDNCMPL
      || Info_Number&0x8000)
      {
        for(j=0; j<max_appl; j++)
        {
          appl = &application[j];
          if(redirectedind && !(plci->adapter->requested_options_table[appl->Id-1]&(1L << PRIVATE_MADAPTER))) continue;
          if(test_c_ind_mask_bit (plci, j))
          /* Todo possibly check Info_Mask?
          if(test_c_ind_mask_bit (plci, j) &&
              (plci->adapter->Info_Mask[j]&Info_Mask))*/
          {
            dbug(1,dprintf("Ovl_Ind"));
            iesent=TRUE;
            sendf(&application[j],_INFO_I,Id,0,"wS",Info_Number,Info_Element);
          }
        }
      }
    }               /* all other signalling states */
    else if(Info_Number
    && plci->adapter->Info_Mask[plci->appl->Id-1] &Info_Mask)
    {
      dbug(1,dprintf("Std_Ind"));
      iesent=TRUE;
      sendf(plci->appl,_INFO_I,Id,0,"wS",Info_Number,Info_Element);
    }
  }
}


byte SendMultiIE(PLCI   * plci, dword Id, byte   * * parms, word ie_type, dword info_mask, APPL   * setupAppl, byte redirectedind, byte shiftcodeset)
{
  word i;
  word j;
  byte   * ie;
  word Info_Number;
  byte   * Info_Element;
  APPL   *appl;
  word Info_Mask = 0;
  byte iesent=0;
  byte esccopy[128];
#define NON_LOCK_SHIFT 0x98

  if(
      !plci->appl
      && !plci->State
      && plci->Sig.Ind!=NCR_FACILITY
      && !setupAppl
    )
  {
    dbug(1,dprintf("NoM-IEParse "));
    return 0;
  }
  dbug(1,dprintf("M-IEParse "));

  for(i=0; i<MAX_MULTI_IE; i++)
  {
    ie = parms[i];
    Info_Number = 0;
    Info_Element = ie;
    
    if(ie_type&0x800) /* move esc infoelements because they come in with format "[0]len [1]ie [2..]data" and we need only "[0]len [1..]data" */
    {
      esccopy[0]=0;
      Info_Element=esccopy;
      if(ie[0]>1)
      {
        if(ie[0]>sizeof(esccopy)){ esccopy[0]=sizeof(esccopy)-1; }
        else{ esccopy[0]=ie[0]-1; }
        for(j=0;j<esccopy[0];j++) esccopy[j+1]=ie[j+2];
      }
    }

    if(Info_Element[0])
    {
      dbug(1,dprintf("[Ind0x%x]:IE=0x%x",plci->Sig.Ind,ie_type));
      Info_Number = ie_type&~0x800; /* kill esc flag */
      Info_Mask = (word)info_mask;
    }

    if(plci->Sig.Ind==NCR_FACILITY ||
      (plci->Sig.Ind==S_SERVICE && plci->State==LISTENING))           /* check controller broadcast */
    {
      for(j=0; j<max_appl; j++)
      {
        appl = &application[j];
        if(redirectedind && !(plci->adapter->requested_options_table[appl->Id-1]&(1L << PRIVATE_MADAPTER))) continue;
        if(Info_Number
        && appl->Id
        && plci->adapter->Info_Mask[appl->Id-1] &Info_Mask)
        {
          iesent = TRUE;
          dbug(1,dprintf("Mlt_NCR_Ind"));
          sendf(&application[j],_INFO_I,Id&0x7fL,0,"wS",Info_Number,Info_Element);
        }
      }
    }
    else if(!plci->appl)
    {                                        /* overlap receiving broadcast */
      if (Info_Number)
      {
        if (setupAppl)
        {
          if(!redirectedind || (plci->adapter->requested_options_table[setupAppl->Id-1]&(1L << PRIVATE_MADAPTER)))
          {
            if(plci->adapter->Info_Mask[setupAppl->Id-1] & Info_Mask)
            {
              iesent = TRUE;
              dbug(1,dprintf("Mlt_Setup_Ind"));
              if(shiftcodeset) sendf(setupAppl,_INFO_I,Id,0,"wS",shiftcodeset|NON_LOCK_SHIFT,""); /* send a codeset shift before the Info_Element */
              sendf(setupAppl,_INFO_I,Id,0,"wS",Info_Number,Info_Element);
            }
          }
        }
        else
        {
          for(j=0; j<max_appl; j++)
          {
            appl = &application[j];
            if(redirectedind && !(plci->adapter->requested_options_table[appl->Id-1]&(1L << PRIVATE_MADAPTER))) continue;
            if(test_c_ind_mask_bit (plci, j))
            {
              iesent = TRUE;
              dbug(1,dprintf("Mlt_Ovl_Ind"));
              if(shiftcodeset) sendf(&application[j],_INFO_I,Id,0,"wS",shiftcodeset|NON_LOCK_SHIFT,""); /* send a codeset shift before the Info_Element */
              sendf(&application[j],_INFO_I,Id,0,"wS",Info_Number,Info_Element);
            }
          }
        }
      }
    }                                        /* all other signalling states */
    else if(Info_Number
    && plci->adapter->Info_Mask[plci->appl->Id-1] &Info_Mask)
    {
      iesent = TRUE;
      dbug(1,dprintf("Mlt_Std_Ind"));
      sendf(plci->appl,_INFO_I,Id,0,"wS",Info_Number,Info_Element);
    }
  }
  return iesent;
}

byte SendSSExtInd(APPL   * appl, PLCI   * plci, dword Id, byte   * * parms)
{
  word i,j,k;
  byte fac[20];
  byte sent_info=0;
  APPL *app,*ap;
  /* Format of multi_ssext_parms[i][]:
  0 byte length
  1 byte SSEXTIE
  2 byte SSEXT_REQ/SSEXT_IND
  3 byte length
  4 word SSExtCommand
  6... Params
  */
  if(
    plci
    && plci->State
    && plci->Sig.Ind!=NCR_FACILITY
    )
  {
    for(i=0;i<MAX_MULTI_IE;i++)
    {
      if(parms[i][0]<6) continue;
      if(parms[i][2]==SSEXT_REQ) continue;

      if(appl) app=appl;
      else if(plci->appl) app=plci->appl;
      else{
        if(READ_WORD(&parms[i][4])==SSTRANSPORT) continue;
        for(j=0; j<max_appl; j++)
        {
          ap = &application[j];
          //if(redirectedind && !(plci->adapter->requested_options_table[ap->Id-1]&(1L << PRIVATE_MADAPTER))) continue;
          if(test_c_ind_mask_bit (plci, j))
          {
            sendf(&application[j],_MANUFACTURER_I,Id,0,"dwS",_DI_MANU_ID,_DI_SSEXT_CTRL,&parms[i][3]);
            sent_info=1;
          }
        }
        continue;
      }

      parms[i][0]=0; /* kill it */
      if(READ_WORD(&parms[i][4])==SSTRANSPORT)
      { /* Format of sstransport:
        parms[i][3] - byte length
        parms[i][4] - word SSExtCommand SSTRANSPORT
        parms[i][6] - Infoelement like values
        parms[i][7] - byte length of IE
        ...         - values
        parms[i][7+parms[i][7]+1] - next infoelement if any */
        j=6;
        while((j-3)<(parms[i][3]))
        {
          switch(parms[i][j])
          {
          case HOLD_NOTIFY: /* only one byte without length */
          case RETRIEVE_NOTIFY:
            if(plci->adapter->Notification_Mask[app->Id-1]&SMASK_HOLD_RETRIEVE)
            {
              fac[0]=3;
              WRITE_WORD(&fac[1],S_HOLD_NOTIFICATION+parms[i][j]);
              fac[3]=0;
              sendf(app,_FACILITY_I,Id,0,"ws",3, fac);
              sent_info=1;
            }
            break;
          case SCS7IE_IND: /* store it for a consultation call */
            if(parms[i][j+1]<sizeof(plci->cinfo.scs7ie))
            {
              for(k=0;k<(parms[i][j+1]+1);k++)
              {
                plci->cinfo.scs7ie[k]=parms[i][j+1+k];
              }
            }
            break;
          case SCNT_IND: /* store it for a consultation call */
            if(parms[i][j+1]<sizeof(plci->cinfo.scntinfo))
            {
              for(k=0;k<(parms[i][j+1]+1);k++)
              {
                plci->cinfo.scntinfo[k]=parms[i][j+1+k];
              }
            }
            break;
          case CARDIDENT_IND:
            if(parms[i][j+1]<sizeof(plci->cinfo.cardident))
            {
              for(k=0;k<(parms[i][j+1]+1);k++)
              {
                plci->cinfo.cardident[k]=parms[i][j+1+k];
              }
            }
            break;
          case LINKIDENT_IND:
            if(parms[i][j+1]<sizeof(plci->cinfo.linkident))
            {
              for(k=0;k<(parms[i][j+1]+1);k++)
              {
                plci->cinfo.linkident[k]=parms[i][j+1+k];
              }
            }
            break;
          }
          if((j-3)>=(parms[i][3])) break; /* for one byte IEs - no length byte */
          j+=(parms[i][j+1]+2);
        }
      }
      else{
        sendf(app,_MANUFACTURER_I,Id,0,"dwS",_DI_MANU_ID,_DI_SSEXT_CTRL,&parms[i][3]);
        sent_info=1;
      }
    }
  }

  return sent_info;
}

void nl_ind(PLCI   * plci)
{
  byte ch;
  word ncci;
  dword Id;
  DIVA_CAPI_ADAPTER   * a;
  word NCCIcode;
  APPL   * APPLptr;
  word count;
  word Num;
  word i, ncpi_state;
  byte len, ncci_state;
  word msg;
  word info = 0;
  byte fax_send_edata_ack;
  static byte v120_header_buffer[2 + 3];
  static word fax_info[] = {
    0,                     /* T30_SUCCESS                        */
    _FAX_NO_CONNECTION,    /* T30_ERR_NO_ENERGY_DETECTED         */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_TIMEOUT_NO_RESPONSE        */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_RESPONSE          */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_TOO_MANY_REPEATS           */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_UNEXPECTED_MESSAGE         */
    _FAX_REMOTE_ABORT,     /* T30_ERR_UNEXPECTED_DCN             */
    _FAX_LOCAL_ABORT,      /* T30_ERR_DTC_UNSUPPORTED            */
    _FAX_TRAINING_ERROR,   /* T30_ERR_ALL_RATES_FAILED           */
    _FAX_TRAINING_ERROR,   /* T30_ERR_TOO_MANY_TRAINS            */
    _FAX_PARAMETER_ERROR,  /* T30_ERR_RECEIVE_CORRUPTED          */
    _FAX_REMOTE_ABORT,     /* T30_ERR_UNEXPECTED_DISC            */
    _FAX_LOCAL_ABORT,      /* T30_ERR_APPLICATION_DISC           */
    _FAX_REMOTE_REJECT,    /* T30_ERR_INCOMPATIBLE_DIS           */
    _FAX_LOCAL_ABORT,      /* T30_ERR_INCOMPATIBLE_DCS           */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_TIMEOUT_NO_COMMAND         */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_COMMAND           */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_TIMEOUT_COMMAND_TOO_LONG   */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_TIMEOUT_RESPONSE_TOO_LONG  */
    _FAX_NO_CONNECTION,    /* T30_ERR_NO_FAX_DETECTED            */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_SUPERVISORY_TIMEOUT        */
    _FAX_PARAMETER_ERROR,  /* T30_ERR_TOO_LONG_SCAN_LINE         */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_PAGE_AFTER_MPS    */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_PAGE_AFTER_CFR    */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_DCS_AFTER_FTT     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_DCS_AFTER_EOM     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_DCS_AFTER_MPS     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_DCN_AFTER_MCF     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_DCN_AFTER_RTN     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_CFR               */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_MCF_AFTER_EOP     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_MCF_AFTER_EOM     */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_RETRY_NO_MCF_AFTER_MPS     */
    0x331d,                /* T30_ERR_SUB_SEP_UNSUPPORTED        */
    0x331e,                /* T30_ERR_PWD_UNSUPPORTED            */
    0x331f,                /* T30_ERR_SUB_SEP_PWD_UNSUPPORTED    */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_INVALID_COMMAND_FRAME      */
    _FAX_PARAMETER_ERROR,  /* T30_ERR_UNSUPPORTED_PAGE_CODING    */
    _FAX_PARAMETER_ERROR,  /* T30_ERR_INVALID_PAGE_CODING        */
    _FAX_REMOTE_REJECT,    /* T30_ERR_INCOMPATIBLE_PAGE_CONFIG   */
    _FAX_LOCAL_ABORT,      /* T30_ERR_TIMEOUT_FROM_APPLICATION   */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_V34FAX_NO_REACTION_ON_MARK */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_V34FAX_TRAINING_TIMEOUT    */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_V34FAX_UNEXPECTED_V21      */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_V34FAX_PRIMARY_CTS_ON      */
    _FAX_LOCAL_ABORT,      /* T30_ERR_V34FAX_TURNAROUND_POLLING  */
    _FAX_LOCAL_ABORT,      /* T30_ERR_V34FAX_V8_INCOMPATIBILITY  */
    _FAX_LOCAL_ABORT,      /* T30_ERR_V34FAX_KNOWN_ECM_BUG       */
    _FAX_PROTOCOL_ERROR,   /* T30_ERR_NOT_IDENTIFIED             */
    _FAX_TRAINING_ERROR,   /* T30_ERR_DROP_BELOW_MIN_SPEED       */
    _FAX_LOCAL_ABORT       /* T30_ERR_MAX_OVERHEAD_EXCEEDED      */
  };

    byte dtmf_code_buffer[CAPIDTMF_RECV_DIGIT_BUFFER_SIZE + 1];


  static word rtp_info[] = {
    GOOD,                  /* RTP_SUCCESS                       */
    0x3600                 /* RTP_ERR_SSRC_OR_PAYLOAD_CHANGE    */
  };

  static dword udata_forwarding_table[0x100 / sizeof(dword)] =
  {
    0x8020301e, 0x00000010, 0x00000f79, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
  };

  ch = plci->NL.IndCh;
  a = plci->adapter;
  ncci = a->ch_ncci[ch];
  Id = (((dword)(ncci ? ncci : ch)) << 16) | (((word) plci->Id) << 8) | a->Id;
  if(plci->tel) Id|=EXT_CONTROLLER;
  APPLptr = plci->appl;
  dbug(1,dprintf("NL_IND-Id(NL:0x%x)=0x%08lx,plci=%x,tel=%x,state=0x%x,ch=0x%x,chs=%d,Ind=%x",
    plci->NL.Id, (unsigned long) Id, plci->Id, plci->tel, plci->State, ch, plci->channels, plci->NL.Ind &0x0f));

  /* in the case if no connect_active_Ind was sent to the appl we wait for */
  if (plci->nl_remove_id
   && ((plci->NL.Ind &0x0f) != N_DATA)
   && ((plci->NL.Ind &0x0f) != N_UDATA)
   && ((plci->NL.Ind &0x0f) != N_BDATA)
   && ((plci->NL.Ind &0x0f) != N_EDATA))
  {
    plci->NL.RNR = 2; /* discard */
    dbug(1,dprintf("NL discard while remove pending"));
    return;
  }
  if((plci->NL.Ind &0x0f)==N_CONNECT)
  {
    if(plci->State==INC_DIS_PENDING
    || plci->State==OUTG_DIS_PENDING
    || ((plci->State==IDLE)
     && (plci->command != _CONNECT_R)
     && (plci->command != PERM_LIST_REQ)
     && ((plci->command != _MANUFACTURER_R) || (plci->m_command != _DI_ASSIGN_PLCI))))
    {
      plci->NL.RNR = 2; /* discard */
      dbug(1,dprintf("discard n_connect"));
      return;
    }
    if((plci->State < INC_ACT_PENDING)
    || ((plci->State==LOCAL_CONNECT)
     && ((plci->command == _CONNECT_R)
      || (plci->command == PERM_LIST_REQ)
      || ((plci->command == _MANUFACTURER_R) && (plci->m_command == _DI_ASSIGN_PLCI)))))
    {
      plci->NL.RNR = 1; /* flow control */
      channel_x_off (plci, ch, N_XON_CONNECT_IND);
      return;
    }
  }

  if(!APPLptr)                         /* no application or invalid data */
  {                                    /* while reloading the DSP        */
    dbug(1,dprintf("discard1"));
    plci->NL.RNR = 2;
    return;
  }

  if (((plci->NL.Ind &0x0f) == N_UDATA)
   && ((plci->NL.RBuffer->P[0] == DSP_UDATA_INDICATION_DCD_ON)
    || (plci->NL.RBuffer->P[0] == DSP_UDATA_INDICATION_CTS_ON))
   && (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_X75) && (((plci->B1_resource & 0xff) == 17) || ((plci->B1_resource & 0xff) == 18)))
    || (plci->B2_prot == 7)
    || (plci->B3_prot == 7)))
  {
    plci->ncpi_buffer[0] = 0;

    ncpi_state = plci->ncpi_state;
    if (plci->NL.complete == 1)
    {
      byte   *data = &plci->NL.RBuffer->P[0];

      if (plci->NL.RBuffer->length >= 12)
      {
        word conn_opt, ncpi_opt = 0x00;
//      HexDump ("MDM N_UDATA:", plci->NL.RBuffer->length, data);

        if (data[0] == DSP_UDATA_INDICATION_DCD_ON)
          plci->ncpi_state |= NCPI_MDM_DCD_ON_RECEIVED;
        if (data[0] == DSP_UDATA_INDICATION_CTS_ON)
          plci->ncpi_state |= NCPI_MDM_CTS_ON_RECEIVED;

        if ((data[3] == DSP_CONNECTED_NORM_V18) || (data[3] == DSP_CONNECTED_NORM_VOWN))
          ncpi_state &= ~(NCPI_MDM_DCD_ON_RECEIVED | NCPI_MDM_CTS_ON_RECEIVED);
        conn_opt = READ_WORD(&(data[4]));

        WRITE_WORD (&(plci->ncpi_buffer[1]), (word)(READ_DWORD(&(data[6])) & 0x0000FFFF));

        if (conn_opt & DSP_CONNECTED_OPTION_MASK_V42)
        {
          ncpi_opt |= MDM_NCPI_ECM_V42;
        }
        else if (conn_opt & DSP_CONNECTED_OPTION_MASK_MNP)
        {
          ncpi_opt |= MDM_NCPI_ECM_MNP;
        }
        else if (!(conn_opt & DSP_CONNECTED_OPTION_MASK_SDLC))
        {
          ncpi_opt |= MDM_NCPI_TRANSPARENT;
        }
        if (conn_opt & DSP_CONNECTED_OPTION_MASK_COMPRESSION)
        {
          ncpi_opt |= MDM_NCPI_COMPRESSED;
        }
        plci->ncpi_buffer[0] = 4;

        if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
          & ((1L << PRIVATE_V18) | (1L << PRIVATE_VOWN)))
        {
          plci->ncpi_buffer[5] = (byte)(plci->NL.RBuffer->length);
          plci->ncpi_buffer[6+0] = 0; /* reserved */
          plci->ncpi_buffer[6+1] = 0; /* reserved */
          plci->ncpi_buffer[6+2] = data[5]; /* connected options */
          plci->ncpi_buffer[6+3] = data[3]; /* connected norm */
          WRITE_WORD (&(plci->ncpi_buffer[6+4]), 0); /* reserved */
          WRITE_WORD (&(plci->ncpi_buffer[6+6]), 0); /* reserved */
          WRITE_WORD (&(plci->ncpi_buffer[6+8]), 0); /* reserved */
          for (i = 10; i < plci->NL.RBuffer->length; i++)
            plci->ncpi_buffer[6+i] = data[i];
          plci->ncpi_buffer[0] = plci->ncpi_buffer[5] + 5;
        }

        WRITE_WORD (&(plci->ncpi_buffer[3]), ncpi_opt);
        plci->ncpi_state |= NCPI_VALID_CONNECT_B3_IND | NCPI_VALID_CONNECT_B3_ACT | NCPI_VALID_DISC_B3_IND;
      }
    }
    if (plci->B3_prot == 7)
    {
      if (((a->ncci_state[ncci] == INC_ACT_PENDING) || (a->ncci_state[ncci] == OUTG_CON_PENDING))
       && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
       && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
      {
        a->ncci_state[ncci] = INC_ACT_PENDING;
        sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
        plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
      }
    }
    if (!(ncpi_state & NCPI_MDM_DCD_ON_RECEIVED)
     || !(ncpi_state & NCPI_MDM_CTS_ON_RECEIVED))
    {
      plci->NL.RNR = 2;
      return;
    }
  }

  if(plci->NL.complete == 2)
  {
    if (((plci->NL.Ind &0x0f) == N_UDATA)
     && (plci->B2_prot != B2_X75)
     && (plci->B2_prot != B2_X75_V42BIS)
     && !(udata_forwarding_table[plci->RData[0].P[0] >> 5] & (1L << (plci->RData[0].P[0] & 0x1f)))
     && ((plci->break_configuration == 0)
      || ((plci->RData[0].P[0] != DSP_UDATA_INDICATION_BREAK_SET)
       && (plci->RData[0].P[0] != DSP_UDATA_INDICATION_BREAK_CLEARED))))
    {
      switch(plci->RData[0].P[0])
      {
      case DSP_UDATA_INDICATION_DISCONNECT:
        if (a->ncci_state[ncci] == CONNECTED)
        {
          cleanup_ncci_data (plci, ncci);
          sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"wS",GOOD,plci->ncpi_buffer);
          a->ncci_state[ncci] = INC_DIS_PENDING;
          plci->ncpi_state = 0;
        }
        break;

      case DTMF_UDATA_INDICATION_FAX_CALLING_TONE:

        if (plci->dtmf_rec_active & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                                     DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG))

        {
          sendf(plci->appl, _FACILITY_I, Id & 0xffffL, 0,"ws", SELECTOR_DTMF, "\x01\x58");
        }
        break;
      case DTMF_UDATA_INDICATION_ANSWER_TONE:

        if (plci->dtmf_rec_active & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                                     DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG))

        {
          sendf(plci->appl, _FACILITY_I, Id & 0xffffL, 0,"ws", SELECTOR_DTMF, "\x01\x59");
        }
        break;
      case DTMF_UDATA_INDICATION_DIGITS_RECEIVED:
        dtmf_indication (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;
      case DTMF_UDATA_INDICATION_DIGITS_SENT:
        dtmf_confirmation (Id, plci);
        break;


      case UDATA_INDICATION_MIXER_TAP_DATA:
        capidtmf_recv_process_block (&(plci->capidtmf_state), plci->RData[0].P + 1, (word)(plci->RData[0].PLength - 1));
        i = capidtmf_indication (&(plci->capidtmf_state), dtmf_code_buffer + 1);
        if (i != 0)
        {
          dtmf_code_buffer[0] = DTMF_UDATA_INDICATION_DIGITS_RECEIVED;
          dtmf_indication (Id, plci, dtmf_code_buffer, (word)(i + 1));
        }
        break;


      case UDATA_INDICATION_MIXER_COEFS_SET:
        mixer_indication_coefs_set (Id, plci);
        break;
      case UDATA_INDICATION_XCONNECT_FROM:
        mixer_indication_xconnect_from (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;
      case UDATA_INDICATION_XCONNECT_TO:
        mixer_indication_xconnect_to (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;


      case LEC_UDATA_INDICATION_DISABLE_DETECT:
        ec_indication (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;

      case UDATA_INDICATION_SIMPLIF_1:
        if (((plci->B1_resource & 0xff) == 17) || ((plci->B1_resource & 0xff) == 18))
        {
          control_message_indication (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
          break;
        }

        measure_indication (Id, plci, plci->RData[0].P, plci->RData[0].PLength);

        break;

      case UDATA_INDICATION_RTCP_REPORT:

        if (plci->B2_prot == B2_T38)
        {
          t38_confirmation (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
          break;
        }

        rtp_confirmation (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;
      case UDATA_INDICATION_RTP_STATE:
        rtp_indication (Id, plci, plci->RData[0].P, plci->RData[0].PLength);
        break;

      default:
        break;
      }
    }
    else
    {
      plci->appl->DataFlags[plci->RNum] |= 0x10;
      if ((plci->RData[0].PLength != 0)
       && ((plci->B2_prot == B2_V120_ASYNC)
        || (plci->B2_prot == B2_V120_ASYNC_V42BIS)
        || (plci->B2_prot == B2_V120_BIT_TRANSPARENT)))
      {

  /* Be note for 64 bit architecture: The implementation of sendf function
   *  must ensure, that the truncated pointer on Dword place will be replaced
   *  by 0 and the real pointer must be added after the "dwww" area ("dwwwq)".
   */
        sendf(plci->appl,_DATA_B3_I,Id,0,
              "dwww",
              (dword)PtrToUlong(plci->RData[1].P),
              (plci->NL.RNum < 2) ? 0 : plci->RData[1].PLength,
              plci->RNum,
              plci->RFlags);

      }
      else
      {

        sendf(plci->appl,_DATA_B3_I,Id,0,
              "dwww",
              (dword)PtrToUlong(plci->RData[0].P),
              plci->RData[0].PLength,
              plci->RNum,
              plci->RFlags);

      }
    }
    return;
  }

  if((plci->NL.Ind &0x0f)==N_CONNECT ||
     (plci->NL.Ind &0x0f)==N_CONNECT_ACK ||
     (plci->NL.Ind &0x0f)==N_DISC ||
     (plci->NL.Ind &0x0f)==N_EDATA ||
     (plci->NL.Ind &0x0f)==N_DISC_ACK)
  {
    info = 0;
    switch (plci->B3_prot) {
    default:
    case  0: /*XPARENT*/
    case  1: /*T.90 NL*/
      plci->ncpi_buffer[0] = 0;
      break;    /* no network control protocol info - jfr */
    case  2: /*ISO8202*/
    case  3: /*X25 DCE*/
      for(i=0; i<plci->NL.RLength; i++) plci->ncpi_buffer[4+i] = plci->NL.RBuffer->P[i];
      plci->ncpi_buffer[0] = (byte)(i+3);
      plci->ncpi_buffer[1] = (byte)(plci->NL.Ind &N_D_BIT? 1:0);
      plci->ncpi_buffer[2] = 0;
      plci->ncpi_buffer[3] = 0;
      break;
    case  4: /*T.30 - FAX*/
    case  5: /*T.30 - FAX*/
      plci->ncpi_buffer[0] = 0;
      if(plci->NL.RLength<sizeof(T30_INFO))
      {
        if (((plci->NL.Ind & 0x0f) == N_DISC) || ((plci->NL.Ind & 0x0f) == N_DISC_ACK))
          info = _FAX_LOCAL_ABORT;
      }
      else
      {
        dbug(1,dprintf("FaxStatus %04x", ((T30_INFO   *)plci->NL.RBuffer->P)->code));
        len = 9;
        WRITE_WORD(&(plci->ncpi_buffer[1]),((T30_INFO   *)plci->NL.RBuffer->P)->rate_div_2400 * 2400);
        plci->fax_feature_bits = READ_WORD(&((T30_INFO   *)plci->NL.RBuffer->P)->feature_bits_low);
        if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_COLOR_FAX)
        {
          plci->fax_feature2_bits = READ_WORD(&((T30_INFO   *)plci->NL.RBuffer->P)->control_bits_low);
        }
        else
        {
          plci->fax_feature2_bits = 0;
        }
        i = ((T30_INFO   *)plci->NL.RBuffer->P)->resolution |
          (((T30_INFO   *)plci->NL.RBuffer->P)->resolution_high << 8);
        i = (i & T30_RESOLUTION_R8_0770_OR_200) ? 0x0001 : 0x0000;
        if (plci->B3_prot == 5)
        {
          if (!(plci->fax_feature_bits & T30_FEATURE_BIT_ECM))
            i |= 0x8000; /* This is not an ECM connection */
          if (plci->fax_feature2_bits & (T30_FEATURE2_BIT_JPEG | T30_FEATURE2_BIT_T43_CODING))
          {
            if (plci->fax_feature2_bits & T30_FEATURE2_BIT_T43_CODING)
              i |= 0x0800; /* This is a connection with T.43 compression */
            if (plci->fax_feature2_bits & T30_FEATURE2_BIT_JPEG)
              i |= 0x0400; /* This is a connection with JPEG compression */
          }
          else
          {
            if (plci->fax_feature_bits & T30_FEATURE_BIT_T6_CODING)
              i |= 0x4000; /* This is a connection with MMR compression */
            if (plci->fax_feature_bits & T30_FEATURE_BIT_2D_CODING)
              i |= 0x2000; /* This is a connection with MR compression */
            if (plci->fax_feature_bits & T30_FEATURE_BIT_T85_CODING)
              i |= 0x1000; /* This is a connection with T.85 compression */
          }
          if (plci->fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
            i |= 0x0004; /* More documents */
          if (((plci->fax_feature_bits & T30_FEATURE_BIT_POLLING) != 0) ^
           ((plci->call_dir & CALL_DIR_MONITOR) ? 0 :
            ((plci->call_dir & CALL_DIR_ORIGINATE) != 0) ^ ((plci->call_dir & CALL_DIR_OUTG_NL) != 0)))
          {
            i |= 0x0002; /* Fax-polling indication */
          }
        }
        dbug(1,dprintf("FAX Options %04x %04x %04x %02x",plci->fax_feature_bits,plci->fax_feature2_bits,i,plci->call_dir));
        WRITE_WORD(&(plci->ncpi_buffer[3]),i);
        i = ((T30_INFO   *)plci->NL.RBuffer->P)->data_format;
        if (plci->fax_feature2_bits & T30_FEATURE2_BIT_PRI)
          i |= 0x0100; /* Procedure interrupt indication */
        WRITE_WORD(&(plci->ncpi_buffer[5]),i);
        plci->ncpi_buffer[7] = ((T30_INFO   *)plci->NL.RBuffer->P)->pages_low;
        plci->ncpi_buffer[8] = ((T30_INFO   *)plci->NL.RBuffer->P)->pages_high;
        plci->ncpi_buffer[len] = 0;
        if(((T30_INFO   *)plci->NL.RBuffer->P)->station_id_len)
        {
          plci->ncpi_buffer[len] = 20;
          for (i = 0; i < 20; i++)
            plci->ncpi_buffer[++len] = ((T30_INFO   *)plci->NL.RBuffer->P)->station_id[i];
        }
        if (((plci->NL.Ind & 0x0f) == N_DISC) || ((plci->NL.Ind & 0x0f) == N_DISC_ACK))
        {
          if (((T30_INFO   *)plci->NL.RBuffer->P)->code < sizeof(fax_info) / sizeof(fax_info[0]))
            info = fax_info[((T30_INFO   *)plci->NL.RBuffer->P)->code];
          else
            info = _FAX_PROTOCOL_ERROR;
        }

        if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
          & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
        {
          i = ((word)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0))) + ((T30_INFO   *)plci->NL.RBuffer->P)->head_line_len;
          while (i < plci->NL.RBuffer->length)
            plci->ncpi_buffer[++len] = plci->NL.RBuffer->P[i++];
        }

        plci->ncpi_buffer[0] = len;

        plci->ncpi_state |= NCPI_VALID_CONNECT_B3_IND;
        if (((plci->NL.Ind &0x0f) == N_CONNECT_ACK)
         || (((plci->NL.Ind &0x0f) == N_CONNECT)
          && (((plci->fax_feature_bits & T30_FEATURE_BIT_POLLING) != 0) ^
           ((plci->call_dir & CALL_DIR_MONITOR) ? 0 :
            ((plci->call_dir & CALL_DIR_ORIGINATE) != 0) ^ ((plci->call_dir & CALL_DIR_OUTG_NL) != 0))))
         || (((plci->NL.Ind &0x0f) == N_EDATA)
          && ((((T30_INFO   *)plci->NL.RBuffer->P)->code == EDATA_T30_TRAIN_OK)
           || (((T30_INFO   *)plci->NL.RBuffer->P)->code == EDATA_T30_DIS)
           || (((T30_INFO   *)plci->NL.RBuffer->P)->code == EDATA_T30_DTC))))
        {
          plci->ncpi_state |= NCPI_VALID_CONNECT_B3_ACT;
        }
        if (((plci->NL.Ind &0x0f) == N_DISC)
         || ((plci->NL.Ind &0x0f) == N_DISC_ACK)
         || (((plci->NL.Ind &0x0f) == N_EDATA)
          && (((T30_INFO   *)plci->NL.RBuffer->P)->code == EDATA_T30_EOP_CAPI)))
        {
          plci->ncpi_state |= NCPI_VALID_CONNECT_B3_ACT | NCPI_VALID_DISC_B3_IND;
        }
      }
      break;

    case B3_RTP:
      plci->ncpi_buffer[0] = 0;
      if (((plci->NL.Ind & 0x0f) == N_DISC) || ((plci->NL.Ind & 0x0f) == N_DISC_ACK))
      {
        if (plci->NL.RLength != 0)
        {
          if (plci->NL.RBuffer->P[0] < sizeof(rtp_info) / sizeof(rtp_info[0]))
            info = rtp_info[plci->NL.RBuffer->P[0]];
          plci->ncpi_buffer[0] = plci->NL.RLength - 1;
          for (i = 1; i < plci->NL.RLength; i++)
            plci->ncpi_buffer[i] = plci->NL.RBuffer->P[i];
        }
      }
      break;

    case 7: /* Modem */
      if((plci->NL.Ind &0x0f)==N_CONNECT)
        plci->ncpi_buffer[0] = 0;
      break;
    }
    plci->NL.RNR = 2;
  }
  switch(plci->NL.Ind &0x0f) {
  case N_EDATA:
    if ((plci->B3_prot == 4) || (plci->B3_prot == 5))
    {
      dbug(1,dprintf("EDATA ncci=0x%x state=%d code=%02x", ncci, a->ncci_state[ncci],
        ((T30_INFO   *)plci->NL.RBuffer->P)->code));
      fax_send_edata_ack = ((((T30_INFO   *)(plci->fax_connect_info_buffer))->operating_mode &
        T30_OPERATING_MODE_NUMBER_MASK) == T30_OPERATING_MODE_CAPI_NEG);

      if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
        & (1L << PRIVATE_FAX_NONSTANDARD))
      {
        switch (((T30_INFO   *)plci->NL.RBuffer->P)->code)
        {
        case EDATA_T30_DIS:
          if ((a->ncci_state[ncci] == OUTG_CON_PENDING)
           && (plci->nsf_control_bits & (T30_NSF_CONTROL_BIT_NEGOTIATE_IND | T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
           && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
           && !(plci->ncpi_state & NCPI_NEGOTIATE_B3_SENT))
          {
            plci->fax_code = ((T30_INFO   *)plci->NL.RBuffer->P)->code;
            sendf(plci->appl,_MANUFACTURER_I,Id,0,"dwbS",_DI_MANU_ID,_DI_NEGOTIATE_B3,
              (byte)(plci->ncpi_buffer[0] + 1), plci->ncpi_buffer);
            plci->ncpi_state |= NCPI_NEGOTIATE_B3_SENT | NCPI_NEGOTIATE_B3_EXPECTED;
            if (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP)
              fax_send_edata_ack = FALSE;
          }
          break;

        case EDATA_T30_MCF:
        case EDATA_T30_EOP:
        case EDATA_T30_MPS:
        case EDATA_T30_EOM:
          if (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_INFO_IND)
          {
            switch (((T30_INFO   *)plci->NL.RBuffer->P)->code)
            {
            case EDATA_T30_EOP:
              plci->ncpi_buffer[6] |= 0x30;
              break;
            case EDATA_T30_EOM:
              plci->ncpi_buffer[6] |= 0x20;
              break;
            case EDATA_T30_MPS:
              plci->ncpi_buffer[6] |= 0x10;
              break;
            default:
              plci->ncpi_buffer[6] |= (plci->fax_feature_bits & T30_FEATURE_BIT_EOP) ? 0x30 : 0x10;
              break;
            }
            switch (plci->fax_feature_bits & T30_FEATURE_COPY_QUALITY_MASK)
            {
            case T30_FEATURE_COPY_QUALITY_RTN:
              plci->ncpi_buffer[6] |= 0xc0;
              break;
            case T30_FEATURE_COPY_QUALITY_RTP:
              plci->ncpi_buffer[6] |= 0x80;
              break;
            case T30_FEATURE_COPY_QUALITY_OK:
              plci->ncpi_buffer[6] |= 0x40;
              break;
            default:
              break;
            }
            sendf(plci->appl,_MANUFACTURER_I,Id,0,"dwbS",_DI_MANU_ID,_DI_INFO_B3,
              (byte)(plci->ncpi_buffer[0] + 1), plci->ncpi_buffer);
            plci->ncpi_buffer[6] = 0;
          }
          break;

        case EDATA_T30_PROGRESS:
          if (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_INFO_IND)
          {
            sendf(plci->appl,_MANUFACTURER_I,Id,0,"dwbS",_DI_MANU_ID,_DI_INFO_B3,
              (byte)(plci->ncpi_buffer[0] + 1), plci->ncpi_buffer);
            plci->ncpi_buffer[6] = 0;
          }
          fax_send_edata_ack = FALSE;
          break;
        }
      }

      if (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
      {
        switch (((T30_INFO   *)plci->NL.RBuffer->P)->code)
        {
        case EDATA_T30_DIS:
          if ((a->ncci_state[ncci] == OUTG_CON_PENDING)
           && !(READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low) & T30_CONTROL_BIT_REQUEST_POLLING)
           && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
           && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
          {
            a->ncci_state[ncci] = INC_ACT_PENDING;
            if (plci->B3_prot == 4)
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
            else
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
            plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
          }
          break;

        case EDATA_T30_TRAIN_OK:
          if ((a->ncci_state[ncci] == INC_ACT_PENDING)
           && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
           && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
          {
            if (plci->B3_prot == 4)
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
            else
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
            plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
          }
          break;

        case EDATA_T30_EOP_CAPI:
          if (a->ncci_state[ncci] == CONNECTED)
          {
            cleanup_ncci_data (plci, ncci);
            sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"wS",GOOD,plci->ncpi_buffer);
            a->ncci_state[ncci] = INC_DIS_PENDING;
            plci->ncpi_state = 0;
            fax_send_edata_ack = FALSE;
          }
          break;
        }
      }
      else
      {
        switch (((T30_INFO   *)plci->NL.RBuffer->P)->code)
        {
        case EDATA_T30_TRAIN_OK:
          if ((a->ncci_state[ncci] == INC_ACT_PENDING)
           && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
           && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
          {
            if (plci->B3_prot == 4)
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
            else
              sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
            plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
          }
          break;
        }
      }
      if (fax_send_edata_ack)
      {
        plci->fax_code = ((T30_INFO   *)plci->NL.RBuffer->P)->code;
        plci->fax_edata_ack_length = 1;
        start_internal_command (Id, plci, fax_edata_ack_command);
      }
    }
    else
    {
      dbug(1,dprintf("EDATA ncci=0x%x state=%d", ncci, a->ncci_state[ncci]));
    }
    break;
  case N_CONNECT:
    if (!a->ch_ncci[ch])
    {
      ncci = get_ncci (plci, ch, 0);
      Id = (Id & 0xffff) | (((dword) ncci) << 16);
    }
    dbug(1,dprintf("N_CONNECT: ch=%d state=%d plci=%lx plci_Id=%lx plci_State=%d",
      ch, a->ncci_state[ncci], (unsigned long)(a->ncci_plci[ncci]), (unsigned long)(plci->Id), plci->State));

    msg = _CONNECT_B3_I;
    if (a->ncci_state[ncci] == IDLE)
      plci->channels++;
    else if (plci->B3_prot == 1)
      msg = _CONNECT_B3_T90_ACTIVE_I;

    a->ncci_state[ncci] = INC_CON_PENDING;
    if(plci->B3_prot == 4)
      sendf(plci->appl,msg,Id,0,"s","");
    else
      sendf(plci->appl,msg,Id,0,"S",plci->ncpi_buffer);
    break;
  case N_CONNECT_ACK:
    dbug(1,dprintf("N_connect_Ack"));
    if (plci->internal_command_queue[0]
     && ((plci->adjust_b_state == ADJUST_B_CONNECT_2)
      || (plci->adjust_b_state == ADJUST_B_CONNECT_3)
      || (plci->adjust_b_state == ADJUST_B_CONNECT_4)))
    {
      (*(plci->internal_command_queue[0]))(Id, plci, 0);
      if (!plci->internal_command)
        next_internal_command (Id, plci);
      break;
    }
    msg = _CONNECT_B3_ACTIVE_I;
    if (plci->B3_prot == 1)
    {
      if (a->ncci_state[ncci] != OUTG_CON_PENDING)
        msg = _CONNECT_B3_T90_ACTIVE_I;
      a->ncci_state[ncci] = INC_ACT_PENDING;
      sendf(plci->appl,msg,Id,0,"S",plci->ncpi_buffer);
    }
    else if ((plci->B3_prot == 4) || (plci->B3_prot == 5) || (plci->B3_prot == 7))
    {
      if ((a->ncci_state[ncci] == OUTG_CON_PENDING)
       && (plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
       && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
      {
        a->ncci_state[ncci] = INC_ACT_PENDING;
        if (plci->B3_prot == 4)
          sendf(plci->appl,msg,Id,0,"s","");
        else
          sendf(plci->appl,msg,Id,0,"S",plci->ncpi_buffer);
        plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
      }
    }
    else
    {
      a->ncci_state[ncci] = INC_ACT_PENDING;
      sendf(plci->appl,msg,Id,0,"S",plci->ncpi_buffer);
    }
    if (plci->adjust_b_restore)
    {
      plci->adjust_b_restore = FALSE;
      start_internal_command (Id, plci, adjust_b_restore);
    }
    break;
  case N_DISC:
  case N_DISC_ACK:
    if (plci->internal_command_queue[0]
     && ((plci->adjust_b_state == ADJUST_B_CONNECT_2)
      || (plci->adjust_b_state == ADJUST_B_CONNECT_3)
      || (plci->adjust_b_state == ADJUST_B_CONNECT_4)
      || (plci->internal_command == RESET_B3_N_RESET_COMMAND_1)
      || (plci->internal_command == FAX_DISCONNECT_COMMAND_1)
      || (plci->internal_command == FAX_DISCONNECT_COMMAND_2)
      || (plci->internal_command == FAX_DISCONNECT_COMMAND_3)))
    {
      (*(plci->internal_command_queue[0]))(Id, plci, 0);
      if (!plci->internal_command)
        next_internal_command (Id, plci);
    }
    ncci_state = a->ncci_state[ncci];
    ncci_remove (plci, ncci, FALSE);
    plci->call_dir &= ~CALL_DIR_OUTG_NL;

      /* need a connect_b3_ind before a disconnect_b3_ind with FAX */
    if (!plci->channels
     && ((plci->B3_prot == 4) || (plci->B3_prot == 5))
     && (plci->State <= CONNECTED))
    {
      trcq (plci);
      if ((plci->State == CONNECTED) || (plci->State == INC_ACT_PENDING))
      {
        len = 9;
        i = (word)((((T30_INFO   *)plci->fax_connect_info_buffer)->rate_div_2400 != 0xff) ?
          ((T30_INFO   *)plci->fax_connect_info_buffer)->rate_div_2400 * 2400 : 14400);
        WRITE_WORD (&plci->ncpi_buffer[1], i);
        WRITE_WORD (&plci->ncpi_buffer[3], 0);
        i = ((T30_INFO   *)plci->fax_connect_info_buffer)->data_format;
        WRITE_WORD (&plci->ncpi_buffer[5], i);
        WRITE_WORD (&plci->ncpi_buffer[7], 0);
        plci->ncpi_buffer[len] = 0;
        plci->ncpi_buffer[0] = len;
        if(plci->B3_prot == 4)
          sendf(plci->appl,_CONNECT_B3_I,Id,0,"s","");
        else
        {

          if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
            & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
          {
            plci->ncpi_buffer[++len] = 0;
            plci->ncpi_buffer[++len] = 0;
            plci->ncpi_buffer[++len] = 0;
            plci->ncpi_buffer[0] = len;
          }

          sendf(plci->appl,_CONNECT_B3_I,Id,0,"S",plci->ncpi_buffer);
        }
        sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"wS",info,plci->ncpi_buffer);
      }
      plci->ncpi_state = 0;
      sig_req(plci,HANGUP,0);
      send_req(plci);
      plci->State = OUTG_DIS_PENDING;
      mixer_remove (plci);
      /* disc here */
    }
    else if ((((a->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
       && ((plci->B3_prot == 4) || (plci->B3_prot == 5)))
      || ((plci->B2_prot == B2_SDLC) && (((plci->B1_resource & 0xff) == 17) || ((plci->B1_resource & 0xff) == 18)))
      /*|| ((plci->B2_prot == B2_X75) && ((plci->B1_resource & 0xff) == 18))*//* We cannot activate since X.75 transfers UDATA end to end */
      || (plci->B2_prot == 7)
      || (plci->B3_prot == 7))
     && ((ncci_state == IDLE) || (ncci_state == INC_DIS_PENDING)))
    {
      if (ncci_state == IDLE)
      {
        trcq (plci);
        if (plci->channels)
          plci->channels--;
        if((plci->State==IDLE || plci->State==SUSPENDING) && !plci->channels){
          if(plci->State == SUSPENDING){
            sendf(plci->appl,
                  _FACILITY_I,
                  Id & 0xffffL,
                  0,
                  "ws", (word)3, "\x03\x04\x00\x00");
            sendf(plci->appl, _DISCONNECT_I, Id & 0xffffL, 0, "w", 0);
          }
          plci_remove(plci);
          return;
        }
      }
      else
      {
        for(i=0; (i<MAX_CHANNELS_PER_PLCI) && plci->inc_dis_ncci_table[i]; i++);
        if (i<MAX_CHANNELS_PER_PLCI)
        {
          plci->inc_dis_ncci_table[i] = (byte) ncci;
          trcq (plci);
        }
        else
        {
          if(plci->channels)plci->channels--;
          trcq (plci);
        }
      }
    }
    else if (plci->channels)
    {
      if (plci->send_disc == ncci)
      {
        sendf(plci->appl,_DISCONNECT_B3_R|CONFIRM,Id,plci->number,"w",0);
        plci->send_disc = 0;
      }
          /* with N_DISC or N_DISC_ACK the IDI frees the respective   */
          /* channel, so we cannot store the state in ncci_state! The */
          /* information which channel we received a N_DISC is thus   */
          /* stored in the inc_dis_ncci_table buffer.                 */
      for(i=0; (i<MAX_CHANNELS_PER_PLCI) && plci->inc_dis_ncci_table[i]; i++);
      if (i<MAX_CHANNELS_PER_PLCI)
      {
        plci->inc_dis_ncci_table[i] = (byte) ncci;
        trcq (plci);
      }
      else
      {
        if(plci->channels)plci->channels--;
        trcq (plci);
      }
      sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"wS",info,plci->ncpi_buffer);
      plci->ncpi_state = 0;
      if ((ncci_state == OUTG_REJ_PENDING)
       && ((plci->B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE)))
      {
        sig_req(plci,HANGUP,0);
        send_req(plci);
        plci->State = OUTG_DIS_PENDING;
        mixer_remove (plci);
      }
    }
    break;
  case N_RESET:
    a->ncci_state[ncci] = INC_RES_PENDING;
    sendf(plci->appl,_RESET_B3_I,Id,0,"S",plci->ncpi_buffer);
    break;
  case N_RESET_ACK:
    if (plci->internal_command_queue[0]
     && (plci->internal_command == RESET_B3_N_RESET_COMMAND_1))
    {
      (*(plci->internal_command_queue[0]))(Id, plci, 0);
      if (!plci->internal_command)
        next_internal_command (Id, plci);
    }
    if (plci->send_disc == 0)
      a->ncci_state[ncci] = CONNECTED;
    sendf(plci->appl,_RESET_B3_I,Id,0,"S",plci->ncpi_buffer);
    break;

  case N_UDATA:
    if ((plci->B2_prot != B2_X75)
     && (plci->B2_prot != B2_X75_V42BIS)
     && !(udata_forwarding_table[plci->NL.RBuffer->P[0] >> 5] & (1L << (plci->NL.RBuffer->P[0] & 0x1f)))
     && ((plci->break_configuration == 0)
      || ((plci->NL.RBuffer->P[0] != DSP_UDATA_INDICATION_BREAK_SET)
       && (plci->NL.RBuffer->P[0] != DSP_UDATA_INDICATION_BREAK_CLEARED))))
    {
      plci->RData[0].P = plci->internal_ind_buffer + (-((long)(PtrToUlong(plci->internal_ind_buffer))) & 3);
      plci->RData[0].PLength = INTERNAL_IND_BUFFER_SIZE;
      plci->NL.R = plci->RData;
      plci->NL.RNum = 1;
      return;
    }
  case N_BDATA:
  case N_DATA:
    if (((a->ncci_state[ncci] != CONNECTED)
      && (a->ncci_state[ncci] != INC_ACT_PENDING)
      && (plci->B2_prot == B2_TRANSPARENT))
     || (a->ncci_state[ncci] == IDLE)
     || (a->ncci_state[ncci] == INC_DIS_PENDING))
    {
      plci->NL.RNR = 2;
      break;
    }
    if ((a->ncci_state[ncci] != CONNECTED)
     && (a->ncci_state[ncci] != INC_ACT_PENDING)
     && (a->ncci_state[ncci] != OUTG_DIS_PENDING)
     && (a->ncci_state[ncci] != OUTG_REJ_PENDING))
    {
      dbug(1,dprintf("flow control"));
      plci->NL.RNR = 1; /* flow control  */
      channel_x_off (plci, ch, 0);
      break;
    }

    NCCIcode = ncci | (((word)a->Id) << 8);

                /* count all buffers within the Application pool    */
                /* belonging to the same NCCI. If this is below the */
                /* number of buffers available per NCCI we accept   */
                /* this packet, otherwise we reject it              */
    count = 0;
    Num = 0xffff;
    for(i=0; i<APPLptr->MaxBuffer; i++) {
      if(NCCIcode==APPLptr->DataNCCI[i]) count++;
      if(!APPLptr->DataNCCI[i] && Num==0xffff) Num = i;
    }

    if(count>=APPLptr->MaxNCCIData || Num==0xffff)
    {
      dbug(3,dprintf("Flow-Control"));
      plci->NL.RNR = 1;
      if( ++(APPLptr->NCCIDataFlowCtrlTimer)>=
       (word)((a->manufacturer_features & MANUFACTURER_FEATURE_OOB_CHANNEL) ? 40 : 2000))
      {
        plci->NL.RNR = 2;
        dbug(3,dprintf("DiscardData"));
      } else {
        channel_x_off (plci, ch, 0);
      }
      break;
    }
    else
    {
      APPLptr->NCCIDataFlowCtrlTimer = 0;
    }

    plci->RData[0].P = ReceiveBufferGet(APPLptr,Num);
    if(!plci->RData[0].P) {
      plci->NL.RNR = 1;
      channel_x_off (plci, ch, 0);
      break;
    }

    APPLptr->DataNCCI[Num] = NCCIcode;
    APPLptr->DataFlags[Num] = (plci->Id<<8) | (plci->NL.Ind>>4);
    dbug(3,dprintf("Buffer(%d), Max = %d",Num,APPLptr->MaxBuffer));

    plci->RNum = Num;
    plci->RFlags = plci->NL.Ind>>4;
    plci->RData[0].PLength = APPLptr->MaxDataLength;
    plci->NL.R = plci->RData;
    if ((plci->NL.RLength != 0)
     && ((plci->B2_prot == B2_V120_ASYNC)
      || (plci->B2_prot == B2_V120_ASYNC_V42BIS)
      || (plci->B2_prot == B2_V120_BIT_TRANSPARENT)))
    {
      plci->RData[1].P = plci->RData[0].P;
      plci->RData[1].PLength = plci->RData[0].PLength;
      plci->RData[0].P = v120_header_buffer + (-((long)(PtrToUlong(v120_header_buffer))) & 3);
      if ((plci->NL.RBuffer->P[0] & V120_HEADER_EXTEND_BIT) || (plci->NL.RLength == 1))
        plci->RData[0].PLength = 1;
      else
        plci->RData[0].PLength = 2;
      if (plci->NL.RBuffer->P[0] & V120_HEADER_BREAK_BIT)
        plci->RFlags |= 0x0010;
      if (plci->NL.RBuffer->P[0] & (V120_HEADER_C1_BIT | V120_HEADER_C2_BIT))
        plci->RFlags |= 0x8000;
      plci->NL.RNum = 2;
    }
    else
    {
      if((plci->NL.Ind &0x0f)==N_UDATA)
        plci->RFlags |= 0x0010;
      plci->NL.RNum = 1;
    }
    break;
  case N_DATA_ACK:
    data_ack (plci, ch);
    break;
  default:
    plci->NL.RNR = 2;
    break;
  }
}

/*------------------------------------------------------------------*/
/* find a free PLCI                                                 */
/*------------------------------------------------------------------*/

word get_plci_(DIVA_CAPI_ADAPTER   * a)
{
  word i,j;
  PLCI   * plci;
  static byte empty_bp[] = { 0 };
    API_PARSE bp[2];

  if (a->first_free_plci_id == 0)
  {
    if ((a->max_plci == 0) || (a->plci[0].Id != 0))
    {
      dbug(1,dprintf("get_plci: out of PLCIs"));
      dump_plcis (a);
      return 0;
    }
    a->first_free_plci_id = 1;
    for (i = 0; i < a->max_plci - 1; i++)
      a->plci[i].NextId = i + 2;
    a->last_free_plci_id = a->max_plci;
  }
  i = a->first_free_plci_id - 1;
  plci = &a->plci[i];
  if (a->first_free_plci_id != a->last_free_plci_id)
    a->first_free_plci_id = plci->NextId;
  else
    a->first_free_plci_id = 0;

  plci->Id = (byte)(i+1);
  plci->Sig.Id = 0;
  plci->NL.Id = 0;
  plci->sig_req = 0;
  plci->nl_req = 0;

  plci->appl = 0;
  plci->relatedPTYPLCI = 0;
  plci->State = IDLE;
  plci->SuppState = IDLE;
  plci->channels = 0;
  plci->tel = 0;
  plci->B1_resource = 0;
  plci->B1_options = 0;
  plci->B2_prot = 0;
  plci->B3_prot = 0;
  plci->stream_data[0] = 0;
  api_parse (empty_bp, sizeof(empty_bp), "s", bp);
  api_save_msg (bp, "s", &plci->B_protocol);

  plci->command = 0;
  plci->m_command = 0;
  init_internal_command_queue (plci);
  plci->number = 0;
  plci->req_in_start = 0;
  plci->req_in = 0;
  plci->req_out = 0;
  plci->msg_in_write_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_read_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;

  plci->data_sent = FALSE;
  plci->send_disc = 0;
  plci->sig_global_req = 0;
  plci->sig_remove_id = 0;
  plci->nl_global_req = 0;
  plci->nl_remove_id = 0;
  plci->nl_assign_rc = ASSIGN_OK;
  plci->adv_nl = 0;
  plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
  plci->spoofed_msg = 0;
  plci->ptyState = 0;
  plci->ptyAppl = NULL;
  plci->cr_enquiry = FALSE;
  plci->hangup_flow_ctrl_timer = 0;

  plci->ncci_ring_list = 0;
  for(j=0;j<MAX_CHANNELS_PER_PLCI;j++) plci->inc_dis_ncci_table[j] = 0;
  clear_c_ind_mask (plci);
  plci->reserve_send_request = FALSE;
  plci->reserve_send_pending = FALSE;
  plci->reserve_request_pending = FALSE;
  for (j = 0; j < RESOURCE_PROFILE_DWORDS; j++)
    ((dword *)(&(plci->reserve_request_profile)))[j] = 0;
  for (j = 0; j < RESOURCE_PROFILE_DWORDS; j++)
    ((dword *)(&(plci->reserve_granted_profile)))[j] = 0;

  plci->fax_connect_info_length = 0;
  plci->fax_code = 0;
  plci->fax_feature_bits = 0;
  plci->fax_feature2_bits = 0;
  plci->nsf_control_bits = 0;
  plci->break_configuration = 0;
  plci->ncpi_state = 0x00;
  plci->ncpi_buffer[0] = 0;

  plci->requested_options_conn = 0;
  plci->requested_options = 0;
  plci->notifiedcall = 0;
  plci->vswitchstate = 0;
  plci->vsprot = 0;
  plci->vsprotdialect = 0;
  plci->calledfromccbscall = 0;
  plci->gothandle = 0;
  plci->waitforECTindOnRPlci = 0;

  plci->rtp_send_requests = 0;


  plci->t38_send_requests = 0;


  plci->cinfo.scs7ie[0]=0;
  plci->cinfo.scntinfo[0]=0;
  plci->cinfo.linkident[0]=0;
  plci->cinfo.cardident[0]=0;
  plci->relatedConsultPLCI = 0;
  plci->lastrejectcause=0;
  plci->ncrl_esccai[0]=0;

  init_b1_config (plci);
  dbug(1,dprintf("get_plci(%x)",plci->Id));
  return i+1;
}

/*------------------------------------------------------------------*/
/* put a parameter in the parameter buffer                          */
/*------------------------------------------------------------------*/

static void add_p(PLCI   * plci, byte code, byte   * p)
{
  word p_length;

  p_length = 0;
  if(p) p_length = p[0];
  add_ie(plci, code, p, p_length);
}

/*------------------------------------------------------------------*/
/* put a structure in the parameter buffer                          */
/*------------------------------------------------------------------*/
static void add_s(PLCI   * plci, byte code, API_PARSE * p)
{
  if(p) add_ie(plci, code, p->info, (word)p->length);
}

/*------------------------------------------------------------------*/
/* put multiple structures in the parameter buffer                  */
/*------------------------------------------------------------------*/
static void add_ss(PLCI   * plci, byte code, API_PARSE * p)
{
  word i;

  if(p){
    dbug(1,dprintf("add_ss(%x,len=%d)",code,p->length));
    for(i=2;(i<=p->length) && (i+p->info[i]<=p->length);i+=p->info[i]+2){
      dbug(1,dprintf("add_ss_ie(%x,len=%d)",p->info[i-1],p->info[i]));
      add_ie(plci, p->info[i-1], (byte   *)&(p->info[i]), (word)p->info[i]);
    }
  }
}

/*------------------------------------------------------------------*/
/* return the channel number sent by the application in a esc_chi   */
/*------------------------------------------------------------------*/
static byte getChannel(API_PARSE * p)
{
  word i;

  if(p){
    for(i=2;i<p->length;i+=p->info[i]+2){
      if(p->info[i]==2){
        if(p->info[i-1]==ESC && p->info[i+1]==CHI) return (p->info[i+2]);
      }
    }
  }
  return 0;
}


/*------------------------------------------------------------------*/
/* put an information element in the parameter buffer               */
/*------------------------------------------------------------------*/

static void add_ie(PLCI   * plci, byte code, byte   * p, word p_length)
{
  word i;

  if(!(code &0x80) && !p_length) return;

  if(plci->req_in==plci->req_in_start) {
    plci->req_in +=2;
  }
  else {
    plci->req_in--;
  }
  plci->RBuffer[plci->req_in++] = code;

  if(p) {
    plci->RBuffer[plci->req_in++] = (byte)p_length;
    for(i=0;i<p_length;i++) plci->RBuffer[plci->req_in++] = p[1+i];
  }

  plci->RBuffer[plci->req_in++] = 0;

  if (plci->req_in > sizeof(plci->RBuffer))
  {
    DBG_FTL(("[%06lx] %s,%d: FATAL request queue overrun",
      (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
      (char   *)(FILE_), __LINE__));
  }
}

/*------------------------------------------------------------------*/
/* put a unstructured data into the buffer                          */
/*------------------------------------------------------------------*/

void add_d(PLCI   * plci, word length, byte   * p)
{
  word i;

  if(plci->req_in==plci->req_in_start) {
    plci->req_in +=2;
  }
  else {
    plci->req_in--;
  }
  for(i=0;i<length;i++) plci->RBuffer[plci->req_in++] = p[i];

  if (plci->req_in > sizeof(plci->RBuffer))
  {
    DBG_FTL(("[%06lx] %s,%d: FATAL request queue overrun",
      (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
      (char   *)(FILE_), __LINE__));
  }
}

/*------------------------------------------------------------------*/
/* put parameters from the Additional Info parameter in the         */
/* parameter buffer                                                 */
/*------------------------------------------------------------------*/

void add_ai(PLCI   * plci, API_PARSE * ai)
{
  word i;
    API_PARSE ai_parms[5];

  for(i=0;i<5;i++) ai_parms[i].length = 0;

  if(!ai->length)
    return;
  if(api_parse(&ai->info[1], (word)ai->length, "ssss", ai_parms))
    return;

  add_s (plci,KEY,&ai_parms[1]);
  add_s (plci,UUI,&ai_parms[2]);
  add_ss(plci,FTY,&ai_parms[3]);
}

/*------------------------------------------------------------------*/
/* put parameter for b1 protocol in the parameter buffer            */
/*------------------------------------------------------------------*/

word add_b1(PLCI   * plci, API_PARSE * bp, word b_channel_info, word b1_facilities)
{
    API_PARSE bp_parms[8];
    API_PARSE mdm_cfg[9];
    API_PARSE global_config[2];
  static byte cai[256];
  byte resource[] = {5,9,13,12,16,39,9,17,17,18};
  word i, w, ans_tone_time;

    API_PARSE mdm_cfg_v18[4];
  word j, n;
  dword d;


  for(i=0;i<8;i++) bp_parms[i].length = 0;
  for(i=0;i<2;i++) global_config[i].length = 0;
  for(i=2;i<sizeof(cai);i++) cai[i] = 0;
  cai[0] = (plci->B1_options != 0) ? 4 : 1;
  cai[4] = plci->B1_options;

  dbug(1,dprintf("add_b1"));
  api_save_msg(bp, "s", &plci->B_protocol);

  if(b_channel_info==2){
    plci->B1_resource = 0;
    adjust_b1_facilities (plci, plci->B1_resource, b1_facilities);
    cai[1] = 0x00;
    add_p(plci, CAI, cai);
    dbug(1,dprintf("Cai=1,0 (no resource)"));
    return 0;
  }

  if(plci->tel == CODEC_PERMANENT) return 0;
  else if(plci->tel == CODEC){
    plci->B1_resource = 1;
    adjust_b1_facilities (plci, plci->B1_resource, b1_facilities);
    cai[1] = 0x01;
    add_p(plci, CAI, cai);
    dbug(1,dprintf("Cai=1,1 (Codec)"));
    return 0;
  }
  else if(plci->tel == ADV_VOICE){
    plci->B1_resource = add_b1_facilities (plci, 9, (word)(b1_facilities | B1_FACILITY_VOICE));
    adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities | B1_FACILITY_VOICE));
    cai[1] = (byte)(plci->B1_resource);
    cai[2] = (byte)(plci->B1_resource >> 8);
    WRITE_WORD (&cai[5], plci->appl->MaxDataLength);
    cai[0] = 6;
    add_p(plci, CAI, cai);
    dbug(1,dprintf("Cai=1,0x%x (AdvVoice)",cai[1]));
    return 0;
  }
  plci->call_dir &= ~(CALL_DIR_ORIGINATE | CALL_DIR_ANSWER | CALL_DIR_MONITOR);
  if (plci->call_dir & CALL_DIR_OUT)
    plci->call_dir |= CALL_DIR_ORIGINATE;
  else if (plci->call_dir & CALL_DIR_IN)
    plci->call_dir |= CALL_DIR_ANSWER;

  if(!bp->length){
    plci->B1_resource = 0x5;
    adjust_b1_facilities (plci, plci->B1_resource, b1_facilities);
    cai[1] = 0x05;
    add_p(plci, CAI, cai);
    return 0;
  }

  dbug(1,dprintf("b_prot_len=%d",(word)bp->length));
  if(bp->length>256) return _WRONG_MESSAGE_FORMAT;
  if(api_parse(&bp->info[1], (word)bp->length, "wwwsssb", bp_parms))
  {
    bp_parms[6].length = 0;
    if(api_parse(&bp->info[1], (word)bp->length, "wwwsss", bp_parms))
    {
      dbug(1,dprintf("b-form.!"));
      return _WRONG_MESSAGE_FORMAT;
    }
  }
  else if (api_parse(&bp->info[1], (word)bp->length, "wwwssss", bp_parms))
  {
    dbug(1,dprintf("b-form.!"));
    return _WRONG_MESSAGE_FORMAT;
  }

  if(bp_parms[6].length)
  {
    if(api_parse(&bp_parms[6].info[1], (word)bp_parms[6].length, "w", global_config))
    {
      return _WRONG_MESSAGE_FORMAT;
    }
    switch(READ_WORD(global_config[0].info))
    {
    case B_OPERATION_DTE:
      plci->call_dir = (plci->call_dir & ~CALL_DIR_ANSWER) | CALL_DIR_ORIGINATE;
      break;
    case B_OPERATION_DCE:
      plci->call_dir = (plci->call_dir & ~CALL_DIR_ORIGINATE) | CALL_DIR_ANSWER;
      break;
    case PRIV_BCHANNEL_OPERATION_MONITOR:
      plci->call_dir |= CALL_DIR_MONITOR;
      break;
    }
  }
  dbug(1,dprintf("call_dir=%04x", plci->call_dir));

  if (READ_WORD(bp_parms[0].info) == B1_NO_RESOURCE)
  {
    plci->B1_resource = 0;
    adjust_b1_facilities (plci, plci->B1_resource, b1_facilities);
    cai[1] = 0x00;
    add_p(plci, CAI, cai);
    dbug(1,dprintf("Cai=1,0 (B1 no resource)"));
    return 0;
  }

  if ((READ_WORD(bp_parms[0].info) == B1_RTP)
   && ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_RTP))
    || (plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    if ((READ_WORD(bp_parms[1].info) == B2_TRANSPARENT)
     && (READ_WORD(bp_parms[2].info) == B3_TRANSPARENT)
     && !(plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)
     && (bp_parms[3].length >= 6)
     && (bp_parms[3].info[2] >= 4)
     && ((((bp_parms[3].info[1] == RTP_PRIM_PAYLOAD_PCMU_8000) && plci->adapter->u_law)
       || ((bp_parms[3].info[1] == RTP_PRIM_PAYLOAD_PCMA_8000) && !plci->adapter->u_law)
       || (bp_parms[3].info[1] == RTP_PRIM_PAYLOAD_LINEAR16_8000))
      && (READ_WORD(&bp_parms[3].info[3]) == 0x010f))) /* storage format, no DTMF/VAD/CNG, no other options */
    {
      i = b1_facilities;
      if (bp_parms[3].info[1] == RTP_PRIM_PAYLOAD_LINEAR16_8000)
        i |= B1_FACILITY_LINEAR16;
      plci->B1_resource = add_b1_facilities (plci, 9/* Transparent */, (word)(i & ~B1_FACILITY_VOICE));
      adjust_b1_facilities (plci, plci->B1_resource, (word)(i & ~B1_FACILITY_VOICE));
      i = READ_WORD(&bp_parms[3].info[5]);
      if (i == 0)
        i = plci->appl->MaxDataLength;
      else if (bp_parms[3].info[1] == RTP_PRIM_PAYLOAD_LINEAR16_8000)
        i *= 2;
      cai[1] = (byte)(plci->B1_resource);
      cai[2] = (byte)(plci->B1_resource >> 8);
      WRITE_WORD(&cai[5],i);
      cai[0] = 6;
      add_p(plci, CAI, cai);
      return 0;
    }
    plci->B1_resource = add_b1_facilities (plci, 31/* Hardware resource RTP */, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    cai[1] = (byte)(plci->B1_resource);
    cai[2] = (byte)(plci->B1_resource >> 8);
    WRITE_WORD(&cai[5],plci->appl->MaxDataLength);
    if ((READ_WORD(bp_parms[1].info) == B2_PACKET_VOICE)
     && (bp_parms[3].length >= 1)
     && (bp_parms[3].info[1] != 0xff))
    {
      cai[7] = 0xff;
      cai[8] = bp_parms[3].info[1+1] + 2;
      for (i = 0; i < bp_parms[3].length; i++)
        cai[9+i] = bp_parms[3].info[1+i];
      cai[0] = 8 + bp_parms[3].length;
    }
    else
    {
      for (i = 0; i < bp_parms[3].length; i++)
        cai[7+i] = bp_parms[3].info[1+i];
      cai[0] = 6 + bp_parms[3].length;
    }
    add_p(plci, CAI, cai);
    return 0;
  }


  if ((READ_WORD(bp_parms[1].info) == B2_T38)
   && ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_T38))
    || (plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    plci->B1_resource = add_b1_facilities (plci, 16/* Hardware resource FAX */, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    cai[1] = (byte)(plci->B1_resource);
    cai[2] = (byte)(plci->B1_resource >> 8);
    WRITE_WORD(&cai[5],256);
    cai[0] = 6;
    if ((plci->call_dir & CALL_DIR_MONITOR) || (bp_parms[3].length >= 2))
    {
      cai[0] = 8;
      w = 0;
      if (plci->call_dir & CALL_DIR_MONITOR)
        w |= 0x0400; /* FAX monitoring mode */
      if (bp_parms[3].length >= 2)
      {
        i = READ_WORD(&(bp_parms[3].info[1])); /* maximum bit rate */
        if ((i != 0) && (i <= 14400))
          w |= 0x1000; /* disable V.34 FAX */
        if (i == 64000)
          w |= 0x2000; /* use digital modem */
      }
      WRITE_WORD(&(cai[7]), w);
    }
    add_p(plci, CAI, cai);
    return 0;
  }


  if ((READ_WORD(bp_parms[0].info) == B1_PIAFS)
   && (plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_PIAFS)))
  {
    plci->B1_resource = add_b1_facilities (plci, 35/* PIAFS HARDWARE FACILITY */, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    cai[1] = (byte)(plci->B1_resource);
    cai[2] = (byte)(plci->B1_resource >> 8);
    WRITE_WORD(&cai[5],plci->appl->MaxDataLength);
    cai[0] = 6;
    add_p(plci, CAI, cai);
    return 0;
  }


  if ((READ_WORD(bp_parms[0].info) == B1_SS7)
   && (plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_SS7)))
  {
    plci->B1_resource = add_b1_facilities (plci, 40/* SS7 HARDWARE FACILITY (MTP1) */, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities & ~B1_FACILITY_VOICE));
    cai[1] = (byte)(plci->B1_resource);
    cai[2] = (byte)(plci->B1_resource >> 8);
    WRITE_WORD(&cai[5],plci->appl->MaxDataLength);
    cai[0] = 6;
    add_p(plci, CAI, cai);
    return 0;
  }


  if ((READ_WORD(bp_parms[0].info) >= 32)
   || (!((1L << READ_WORD(bp_parms[0].info)) & plci->adapter->profile.B1_Protocols)
    && ((READ_WORD(bp_parms[0].info) != 3)
     || !((1L << B1_HDLC) & plci->adapter->profile.B1_Protocols)
     || ((bp_parms[3].length != 0) && (READ_WORD(&bp_parms[3].info[1]) != 0) && (READ_WORD(&bp_parms[3].info[1]) != 56000)))))
  {
    return _B1_NOT_SUPPORTED;
  }
  plci->B1_resource = add_b1_facilities (plci, resource[READ_WORD(bp_parms[0].info)],
    (word)(b1_facilities & ~B1_FACILITY_VOICE));
  adjust_b1_facilities (plci, plci->B1_resource, (word)(b1_facilities & ~B1_FACILITY_VOICE));
  cai[0] = 6;
  cai[1] = (byte)(plci->B1_resource);
  cai[2] = (byte)(plci->B1_resource >> 8);

  if ((READ_WORD(bp_parms[0].info) == B1_MODEM_ALL_NEGOTIATE)
   || (READ_WORD(bp_parms[0].info) == B1_MODEM_ASYNC)
   || (READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC))
  { /* B1 - modem */
    for (i=0;i<7;i++) mdm_cfg[i].length = 0;

    if (bp_parms[3].length)
    {
      if(api_parse(&bp_parms[3].info[1],(word)bp_parms[3].length,"wwwwww", mdm_cfg))
      {
        return (_WRONG_MESSAGE_FORMAT);
      }

      dbug(1,dprintf("MDM Max Bit Rate:<%d>", READ_WORD(mdm_cfg[0].info)));

      WRITE_WORD (&cai[13], 0);                          /* Min Tx speed */
      WRITE_WORD (&cai[15], READ_WORD(mdm_cfg[0].info)); /* Max Tx speed */
      WRITE_WORD (&cai[17], 0);                          /* Min Rx speed */
      WRITE_WORD (&cai[19], READ_WORD(mdm_cfg[0].info)); /* Max Rx speed */

      cai[3] = 0; /* Async framing parameters */
      switch (READ_WORD (mdm_cfg[2].info))
      {       /* Parity     */
      case 1: /* odd parity */
        cai[3] |= (DSP_CAI_ASYNC_PARITY_ENABLE | DSP_CAI_ASYNC_PARITY_ODD);
        dbug(1,dprintf("MDM: odd parity"));
        break;

      case 2: /* even parity */
        cai[3] |= (DSP_CAI_ASYNC_PARITY_ENABLE | DSP_CAI_ASYNC_PARITY_EVEN);
        dbug(1,dprintf("MDM: even parity"));
        break;

      default:
        dbug(1,dprintf("MDM: no parity"));
        break;
      }

      switch (READ_WORD (mdm_cfg[3].info))
      {       /* stop bits   */
      case 1: /* 2 stop bits */
        cai[3] |= DSP_CAI_ASYNC_TWO_STOP_BITS;
        dbug(1,dprintf("MDM: 2 stop bits"));
        break;

      default:
        dbug(1,dprintf("MDM: 1 stop bit"));
        break;
      }

      switch (READ_WORD (mdm_cfg[1].info))
      {     /* char length */
      case 5:
        cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_5;
        dbug(1,dprintf("MDM: 5 bits"));
        break;

      case 6:
        cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_6;
        dbug(1,dprintf("MDM: 6 bits"));
        break;

      case 7:
        cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_7;
        dbug(1,dprintf("MDM: 7 bits"));
        break;

      default:
        dbug(1,dprintf("MDM: 8 bits"));
        break;
      }

      cai[7] = 0; /* Line taking options */
      cai[8] = 0; /* Modulation negotiation options */
      cai[9] = 0; /* Modulation options */

      if (((plci->call_dir & CALL_DIR_ORIGINATE) != 0) ^ ((plci->call_dir & CALL_DIR_OUT) != 0))
      {
        cai[9] |= DSP_CAI_MODEM_REVERSE_DIRECTION;
        dbug(1, dprintf("MDM: Reverse direction"));
      }

      if (READ_WORD (mdm_cfg[4].info) & MDM_CAPI_DISABLE_RETRAIN)
      {
        cai[9] |= DSP_CAI_MODEM_DISABLE_RETRAIN;
        dbug(1, dprintf("MDM: Disable retrain"));
      }

      if (READ_WORD (mdm_cfg[4].info) & MDM_CAPI_DISABLE_RING_TONE)
      {
        cai[7] |= DSP_CAI_MODEM_DISABLE_CALLING_TONE | DSP_CAI_MODEM_DISABLE_ANSWER_TONE;
        dbug(1, dprintf("MDM: Disable ring tone"));
      }

      switch (READ_WORD (mdm_cfg[4].info) & MDM_CAPI_GUARD_MASK)
      {
      case MDM_CAPI_GUARD_1800:
        cai[8] |= DSP_CAI_MODEM_GUARD_TONE_1800HZ;
        dbug(1, dprintf("MDM: 1800 guard tone"));
        break;

      case MDM_CAPI_GUARD_550:
        cai[8] |= DSP_CAI_MODEM_GUARD_TONE_550HZ;
        dbug(1, dprintf("MDM: 550 guard tone"));
        break;
      }

      switch (READ_WORD (mdm_cfg[4].info) & MDM_CAPI_SPEAKER_MASK)
      {
      case MDM_CAPI_SPEAKER_TIL_CONNECT:
        cai[24] |= DSP_CAI_MODEM_SPEAKER_TIL_CONNECT;
        dbug(1, dprintf("MDM: Speaker on til connect"));
        break;

      case MDM_CAPI_SPEAKER_ALWAYS_ON:
        cai[24] |= DSP_CAI_MODEM_SPEAKER_ALWAYS_ON;
        dbug(1, dprintf("MDM: Speaker always on"));
        break;
      }

      switch (READ_WORD (mdm_cfg[4].info) & MDM_CAPI_VOLUME_MASK)
      {
      case MDM_CAPI_VOLUME_NORMAL_LOW:
        cai[24] |= DSP_CAI_MODEM_SPEAKER_VOLUME_LOW;
        dbug(1, dprintf("MDM: Speaker volume normal low"));
        break;

      case MDM_CAPI_VOLUME_NORMAL_HIGH:
        cai[24] |= DSP_CAI_MODEM_SPEAKER_VOLUME_HIGH;
        dbug(1, dprintf("MDM: Speaker volume normal high"));
        break;

      case MDM_CAPI_VOLUME_MAXIMUM:
        cai[24] |= DSP_CAI_MODEM_SPEAKER_VOLUME_MAX;
        dbug(1, dprintf("MDM: Speaker volume maximum"));
        break;
      }

      switch (READ_WORD (mdm_cfg[5].info) & 0x00ff)
      {
      case MDM_CAPI_NEG_V100:
        cai[8] |= DSP_CAI_MODEM_NEGOTIATE_V100;
        dbug(1, dprintf("MDM: V100"));
        break;

      case MDM_CAPI_NEG_MOD_CLASS:
        cai[8] |= DSP_CAI_MODEM_NEGOTIATE_IN_CLASS;
        dbug(1, dprintf("MDM: IN CLASS"));
        break;

      case MDM_CAPI_NEG_DISABLED:
        cai[8] |= DSP_CAI_MODEM_NEGOTIATE_DISABLED;
        dbug(1, dprintf("MDM: DISABLED"));
        break;
      }
      cai[0] = 26;

      if ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_V18))
       && (READ_WORD(mdm_cfg[5].info) & 0x8000)) /* Private V.18 enable */
      {
        plci->requested_options |= 1L << PRIVATE_V18;
      }
      if (READ_WORD(mdm_cfg[5].info) & 0x4000) /* Private VOWN enable */
        plci->requested_options |= 1L << PRIVATE_VOWN;

      if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
        & ((1L << PRIVATE_V18) | (1L << PRIVATE_VOWN)))
      {
        if (!api_parse(&bp_parms[3].info[1],(word)bp_parms[3].length,"wwwwwws", mdm_cfg))
        {
          i = 27;
          if (mdm_cfg[6].length >= 4)
          {
            d = READ_DWORD(&mdm_cfg[6].info[1]);
            cai[7] |= (byte) d;          /* line taking options */
            cai[9] |= (byte)(d >> 8);    /* modulation options */
            cai[++i] = (byte)(d >> 16);  /* vown modulation options */
            cai[++i] = (byte)(d >> 24);
            if (mdm_cfg[6].length >= 8)
            {
              d = READ_DWORD(&mdm_cfg[6].info[5]);
              cai[10] |= (byte) d;        /* disabled modulations mask */
              cai[11] |= (byte)(d >> 8);
              if (mdm_cfg[6].length >= 12)
              {
                d = READ_DWORD(&mdm_cfg[6].info[9]);
                cai[12] = (byte) d;          /* enabled modulations mask */
                cai[++i] = (byte)(d >> 8);   /* vown enabled modulations */
                cai[++i] = (byte)(d >> 16);
                cai[++i] = (byte)(d >> 24);
                cai[++i] = 0;
                if (mdm_cfg[6].length >= 14)
                {
                  w = READ_WORD(&mdm_cfg[6].info[13]);
                  if (w != 0)
                    WRITE_WORD(&cai[13], w);  /* min tx speed */
                  if (mdm_cfg[6].length >= 16)
                  {
                    w = READ_WORD(&mdm_cfg[6].info[15]);
                    if (w != 0)
                      WRITE_WORD(&cai[15], w);  /* max tx speed */
                    if (mdm_cfg[6].length >= 18)
                    {
                      w = READ_WORD(&mdm_cfg[6].info[17]);
                      if (w != 0)
                        WRITE_WORD(&cai[17], w);  /* min rx speed */
                      if (mdm_cfg[6].length >= 20)
                      {
                        w = READ_WORD(&mdm_cfg[6].info[19]);
                        if (w != 0)
                          WRITE_WORD(&cai[19], w);  /* max rx speed */
                        if (mdm_cfg[6].length >= 22)
                        {
                          w = READ_WORD(&mdm_cfg[6].info[21]);
                          cai[23] = (byte)(-((((short) w) >= 0) ? 0 : ((((short) w) <= -15) ? -15 : ((short) w))));  /* transmit level */
                          if (mdm_cfg[6].length >= 24)
                          {
                            w = READ_WORD(&mdm_cfg[6].info[23]);
                            cai[22] |= (byte) w;        /* info options mask */
                            cai[21] |= (byte)(w >> 8);  /* disabled symbol rates */
                            if (mdm_cfg[6].length >= 28)
                            {
                              ans_tone_time = READ_WORD(&mdm_cfg[6].info[27]);
                              if (ans_tone_time != 0)
                              {
                                cai[24] |= (byte)((ans_tone_time >= 0x1500) ? 0xf0 :
                                  ((ans_tone_time >= 0x0900) ? ((ans_tone_time + 0x900) >> 5) & 0xf0 :
                                   ((ans_tone_time >= 0x0880) ? 0x90 :
                                    ((ans_tone_time >= 0x0080) ? ((ans_tone_time + 0x80) >> 4) & 0xf0 : 0x10))));  /* answer tone time */
                                cai[++i] = (byte)(ans_tone_time);       /* Pack the full, non-scaled value also */
                                cai[++i] = (byte)(ans_tone_time >> 8);
                              }
                              if (mdm_cfg[6].length >= 30)
                              {
                                w = READ_WORD(&mdm_cfg[6].info[29]);
                                cai[25] = (byte)((w == 0xffff) ? 255 : (w + 999) / 1000);  /* carrier wait time seconds */
                                if (mdm_cfg[6].length >= 32)
                                {
                                  w = READ_WORD(&mdm_cfg[6].info[31]);
                                  cai[26] = (byte)((w >= 25500) ? 255 : (w + 99) / 100);  /* carrier loss time 1/10 sec */
                                  if (mdm_cfg[6].length >= 34)
                                  {
                                    w = READ_WORD(&mdm_cfg[6].info[33]);
                                    cai[8] |= (byte)(((((w <= 256) ? 256 : ((w >= 352) ? 352 : w)) + 0x10) >> 2) & 0x18);  /* V.29FC answer tone time */
                                    if (mdm_cfg[6].length >= 36)
                                    {
                                      if (ans_tone_time == 0)
                                      {

                                        cai[++i] = 0;
                                        cai[++i] = 0;
                                      }
                                      w = READ_WORD(&mdm_cfg[6].info[35]);
                                      cai[++i] = (byte)(w);       /* answer tone delay */
                                      cai[++i] = (byte)(w >> 8);


                                      for (j = 37; j <= mdm_cfg[6].length; j++)
                                        cai[++i] = mdm_cfg[6].info[j];
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          cai[27] = i - 27;
          i++;
          if (!api_parse(&bp_parms[3].info[1],(word)bp_parms[3].length,"wwwwwwss", mdm_cfg))
          {
            if (!api_parse(&mdm_cfg[7].info[1],(word)mdm_cfg[7].length,"sss", mdm_cfg_v18))
            {
              for (n = 0; n < 3; n++)
              {
                cai[i] = (byte)(mdm_cfg_v18[n].length);
                for (j = 1; j < ((word)(cai[i] + 1)); j++)
                  cai[i+j] = mdm_cfg_v18[n].info[j];
                i += cai[i] + 1;
              }
            }
          }
          cai[0] = (byte)(i - 1);
        }
      }

    }
  }
  if(READ_WORD(bp_parms[0].info)==2 ||                         // V.110 async
     READ_WORD(bp_parms[0].info)==3 )                          // V.110 sync
  {
    if(bp_parms[3].length){
      dbug(1,dprintf("V.110,%d",READ_WORD(&bp_parms[3].info[1])));
      switch(READ_WORD(&bp_parms[3].info[1])){                 // Rate
        case 0:
        case 56000:
          if(READ_WORD(bp_parms[0].info)==3){                  //V.110 sync 56k
            dbug(1,dprintf("56k sync HSCX"));
            cai[1] = 8;
            cai[2] = 0;
            cai[3] = 0;
          }
          else if(READ_WORD(bp_parms[0].info)==2){
            dbug(1,dprintf("56k async DSP"));
            cai[2] = 9;
          }
          break;
        case 50:     cai[2] = 1;  break;
        case 75:     cai[2] = 1;  break;
        case 110:    cai[2] = 1;  break;
        case 150:    cai[2] = 1;  break;
        case 200:    cai[2] = 1;  break;
        case 300:    cai[2] = 1;  break;
        case 600:    cai[2] = 1;  break;
        case 1200:   cai[2] = 2;  break;
        case 2400:   cai[2] = 3;  break;
        case 4800:   cai[2] = 4;  break;
        case 7200:   cai[2] = 10; break;
        case 9600:   cai[2] = 5;  break;
        case 12000:  cai[2] = 13; break;
        case 24000:  cai[2] = 0;  break;
        case 14400:  cai[2] = 11; break;
        case 19200:  cai[2] = 6;  break;
        case 28800:  cai[2] = 12; break;
        case 38400:  cai[2] = 7;  break;
        case 48000:  cai[2] = 8;  break;
        case 76:     cai[2] = 15; break;  /* 75/1200     */
        case 1201:   cai[2] = 14; break;  /* 1200/75     */
        case 56001:  cai[2] = 9;  break;  /* V.110 56000 */

        default:
          return _B1_PARM_NOT_SUPPORTED;
      }
      cai[3] = 0;
      if (cai[1] == 13)                                        // v.110 async
      {
        if (bp_parms[3].length >= 8)
        {
          switch (READ_WORD (&bp_parms[3].info[3]))
          {       /* char length */
          case 5:
            cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_5;
            break;
          case 6:
            cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_6;
            break;
          case 7:
            cai[3] |= DSP_CAI_ASYNC_CHAR_LENGTH_7;
            break;
          }
          switch (READ_WORD (&bp_parms[3].info[5]))
          {       /* Parity     */
          case 1: /* odd parity */
            cai[3] |= (DSP_CAI_ASYNC_PARITY_ENABLE | DSP_CAI_ASYNC_PARITY_ODD);
            break;
          case 2: /* even parity */
            cai[3] |= (DSP_CAI_ASYNC_PARITY_ENABLE | DSP_CAI_ASYNC_PARITY_EVEN);
            break;
          }
          switch (READ_WORD (&bp_parms[3].info[7]))
          {       /* stop bits   */
          case 1: /* 2 stop bits */
            cai[3] |= DSP_CAI_ASYNC_TWO_STOP_BITS;
            break;
          }
        }
      }
    }
    else if(cai[1]==8 || READ_WORD(bp_parms[0].info)==3 ){
      dbug(1,dprintf("V.110 default 56k sync"));
      cai[1] = 8;
      cai[2] = 0;
      cai[3] = 0;
    }
    else {
      dbug(1,dprintf("V.110 default 9600 async"));
      cai[2] = 5;
    }
  }
  else if ((plci->B1_resource & 0xff) == 16)
  { /* T30 */
    if ((plci->call_dir & CALL_DIR_MONITOR) || (bp_parms[3].length >= 2))
    {
      cai[0] = 8;
      w = 0;
      if (plci->call_dir & CALL_DIR_MONITOR)
        w |= 0x0400; /* FAX monitoring mode */
      if (bp_parms[3].length >= 2)
      {
        i = READ_WORD(&(bp_parms[3].info[1])); /* maximum bit rate */
        if ((i != 0) && (i <= 14400))
          w |= 0x1000; /* disable V.34 FAX */
        if (i == 64000)
          w |= 0x2000; /* use digital modem */
      }
      WRITE_WORD(&(cai[7]), w);
    }
  }
  WRITE_WORD(&cai[5],plci->appl->MaxDataLength);
  dbug(1,dprintf("CAI[%d]=%x,%x,%x,%x,%x,%x", cai[0], cai[1], cai[2], cai[3], cai[4], cai[5], cai[6]));
//HexDump ("CAI", sizeof(cai), &cai[0]);

  add_p(plci, CAI, cai);
  return 0;
}

/*------------------------------------------------------------------*/
/* put parameter for b2 and B3  protocol in the parameter buffer    */
/*------------------------------------------------------------------*/

word add_b23(PLCI   * plci, API_PARSE * bp)
{
  word i, resolution, data_format, fax_control_bits, fax_control2_bits;
  byte pos, len;
  byte SAPI = 0x40;  /* default SAPI 16 for x.31 */
    API_PARSE bp_parms[8];
  API_PARSE * b1_config;
  API_PARSE * b2_config;
    API_PARSE b2_config_parms[8];
  API_PARSE * b3_config;
    API_PARSE b3_config_parms[6];
    API_PARSE global_config[2];

  static byte llc[3] = {2,0,0};
  static byte dlc[64];
  static byte nlc[256];
  static byte lli[12] = {1,1};

  byte llc2_out[] = {1,2,4,6,2,0,0,0, X75_V42BIS,V120_L2,V120_V42BIS,V120_L2,6};
  byte llc2_in[]  = {1,3,4,6,3,0,0,0, X75_V42BIS,V120_L2,V120_V42BIS,V120_L2,6};

  byte llc3[] = {TRANSPARENT_NL,T70NLX,ISO8208,X25PLP,T30,T30,0,TRANSPARENT_NL};
  byte header[] = {0,2,3,3,0,0,0,0};

  for(i=0;i<sizeof(bp_parms)/sizeof(bp_parms[0]);i++) bp_parms[i].length = 0;
  for(i=0;i<sizeof(b2_config_parms)/sizeof(b2_config_parms[0]);i++) b2_config_parms[i].length = 0;
  for(i=0;i<sizeof(b3_config_parms)/sizeof(b3_config_parms[0]);i++) b3_config_parms[i].length = 0;

  lli[0] = 1;
  lli[1] = 1;
  if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)
    lli[1] |= 2;
  if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_OOB_CHANNEL)
    lli[1] |= 4;

  if ((lli[1] & 0x02) && (diva_xdi_extended_features & DIVA_CAPI_USE_CMA)) {
    lli[1] |= 0x10;
    if (plci->rx_dma_descriptor <= 0) {
      plci->rx_dma_descriptor=diva_get_dma_descriptor(plci,&plci->rx_dma_magic);
      if (plci->rx_dma_descriptor >= 0)
        plci->rx_dma_descriptor++;
    }
    if (plci->rx_dma_descriptor > 0) {
      lli[0] = 6;
      lli[1] |= 0x40;
      lli[2] = (byte)(plci->rx_dma_descriptor - 1);
      lli[3] = (byte)plci->rx_dma_magic;
      lli[4] = (byte)(plci->rx_dma_magic >>  8);
      lli[5] = (byte)(plci->rx_dma_magic >> 16);
      lli[6] = (byte)(plci->rx_dma_magic >> 24);
    }
  }

  if (DIVA_CAPI_SUPPORTS_NO_CANCEL(plci->adapter)) {
    lli[1] |= 0x20;
  }

  dbug(1,dprintf("add_b23"));
  api_save_msg(bp, "s", &plci->B_protocol);

  if(!bp->length && plci->tel)
  {
    plci->adv_nl = TRUE;
    dbug(1,dprintf("Default adv.Nl"));
    add_p(plci,LLI,lli);
    plci->B2_prot = 1 /*XPARENT*/;
    plci->B3_prot = 0 /*XPARENT*/;
    llc[1] = 2;
    llc[2] = 4;
    add_p(plci, LLC, llc);
    dlc[0] = 2;
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    add_p(plci, DLC, dlc);
    if (plci->stream_data[0] != 0) {
      add_p(plci, SIG, plci->stream_data);
    }
    return 0;
  }

  if(!bp->length) /*default*/
  {
    dbug(1,dprintf("ret default"));
    add_p(plci,LLI,lli);
    plci->B2_prot = 0 /*X.75   */;
    plci->B3_prot = 0 /*XPARENT*/;
    llc[1] = 1;
    llc[2] = 4;
    add_p(plci, LLC, llc);
    dlc[0] = 2;
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    add_p(plci, DLC, dlc);
    if (plci->stream_data[0] != 0) {
      add_p(plci, SIG, plci->stream_data);
    }
    return 0;
  }
  dbug(1,dprintf("b_prot_len=%d",(word)bp->length));
  if((word)bp->length > 256)    return _WRONG_MESSAGE_FORMAT;

  if(api_parse(&bp->info[1], (word)bp->length, "wwwsssb", bp_parms))
  {
    bp_parms[6].length = 0;
    if(api_parse(&bp->info[1], (word)bp->length, "wwwsss", bp_parms))
    {
      dbug(1,dprintf("b-form.!"));
      return _WRONG_MESSAGE_FORMAT;
    }
  }
  else if (api_parse(&bp->info[1], (word)bp->length, "wwwssss", bp_parms))
  {
    dbug(1,dprintf("b-form.!"));
    return _WRONG_MESSAGE_FORMAT;
  }

  if(plci->tel==ADV_VOICE) /* transparent B on advanced voice */
  {
    if(READ_WORD(bp_parms[1].info)!=1
    || READ_WORD(bp_parms[2].info)!=0) return _B2_NOT_SUPPORTED;
    plci->adv_nl = TRUE;
  }
  else if(plci->tel) return _B2_NOT_SUPPORTED;

  if(bp_parms[6].length)
  {
    if(api_parse(&bp_parms[6].info[1], (word)bp_parms[6].length, "w", global_config))
    {
      return _WRONG_MESSAGE_FORMAT;
    }
    switch(READ_WORD(global_config[0].info))
    {
    case B_OPERATION_DTE:
      plci->call_dir = (plci->call_dir & ~CALL_DIR_ANSWER) | CALL_DIR_ORIGINATE;
      break;
    case B_OPERATION_DCE:
      plci->call_dir = (plci->call_dir & ~CALL_DIR_ORIGINATE) | CALL_DIR_ANSWER;
      break;
    case PRIV_BCHANNEL_OPERATION_MONITOR:
      plci->call_dir |= CALL_DIR_MONITOR;
      break;
    default: break ;
    }
  }
  dbug(1,dprintf("call_dir=%04x", plci->call_dir));


  if ((READ_WORD(bp_parms[1].info) == B2_RTP)
   && (READ_WORD(bp_parms[2].info) == B3_RTP)
   && ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_RTP))
    || (plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    add_p(plci,LLI,lli);
    plci->B2_prot = (byte) READ_WORD(bp_parms[1].info);
    plci->B3_prot = (byte) READ_WORD(bp_parms[2].info);
    llc[1] = (plci->call_dir & (CALL_DIR_ORIGINATE | CALL_DIR_FORCE_OUTG_NL)) ? 14 : 13;
    llc[2] = 4;
    add_p(plci, LLC, llc);
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    dlc[3] = 3; /* Addr A */
    dlc[4] = 1; /* Addr B */
    dlc[5] = 7; /* modulo mode */
    dlc[6] = 7; /* window size */
    dlc[7] = 0; /* XID len Lo  */
    dlc[8] = 0; /* XID len Hi  */
    for (i = 0; i < bp_parms[4].length; i++)
      dlc[9+i] = bp_parms[4].info[1+i];
    dlc[0] = (byte)(8 + bp_parms[4].length);
    add_p(plci, DLC, dlc);
    for (i = 0; i < bp_parms[5].length; i++)
      nlc[1+i] = bp_parms[5].info[1+i];
    nlc[0] = (byte)(bp_parms[5].length);
    add_p(plci, NLC, nlc);
    if (plci->stream_data[0] != 0) {
      add_p(plci, SIG, plci->stream_data);
    }
    return 0;
  }


  if ((READ_WORD(bp_parms[1].info) == B2_T38)
   && ((READ_WORD(bp_parms[2].info) == B3_TRANSPARENT_IP) || (READ_WORD(bp_parms[2].info) == B3_RTP))
   && ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_T38))
    || (plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    add_p(plci,LLI,lli);
    plci->B2_prot = (byte) READ_WORD(bp_parms[1].info);
    plci->B3_prot = (byte) READ_WORD(bp_parms[2].info);
    llc[1] = (plci->call_dir & (CALL_DIR_ORIGINATE | CALL_DIR_FORCE_OUTG_NL)) ? 16 : 17;
    llc[2] = 4;
    add_p(plci, LLC, llc);
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    dlc[3] = 3; /* Addr A */
    dlc[4] = 1; /* Addr B */
    dlc[5] = 7; /* modulo mode */
    dlc[6] = 7; /* window size */
    dlc[7] = 0; /* XID len Lo  */
    dlc[8] = 0; /* XID len Hi  */
    for (i = 0; i < bp_parms[4].length; i++)
      dlc[9+i] = bp_parms[4].info[1+i];
    dlc[0] = (byte)(8 + bp_parms[4].length);
    add_p(plci, DLC, dlc);
    for (i = 0; i < bp_parms[5].length; i++)
      nlc[1+i] = bp_parms[5].info[1+i];
    nlc[0] = (byte)(bp_parms[5].length);
    add_p(plci, NLC, nlc);
    if (plci->stream_data[0] != 0) {
      add_p(plci, SIG, plci->stream_data);
    }
    return 0;
  }




 i = READ_WORD(bp_parms[1].info) ;
 if ((i >= 32)
      || ((!((1L << i) & plci->adapter->profile.B2_Protocols))
          && ((i != B2_PIAFS)
              || !(plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_PIAFS)))
          && ((i != B2_SS7)
              || !(plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_SS7)))
          && ((i != B2_LISTENING)
              || !(plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_LISTENING)))))


  {
    return _B2_NOT_SUPPORTED;
  }
  if ((READ_WORD(bp_parms[2].info) >= 32)
   || !((1L << READ_WORD(bp_parms[2].info)) & plci->adapter->profile.B3_Protocols))
  {
    return _B3_NOT_SUPPORTED;
  }
  if ((READ_WORD(bp_parms[1].info) != B2_SDLC)
/*
  Allows X.75 L2 on top of the sync modem
  */
   && (!((READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)
     && ((READ_WORD(bp_parms[1].info) == B2_X75) || (READ_WORD(bp_parms[1].info) == B2_X75_V42BIS))))

   && ((READ_WORD(bp_parms[0].info) == B1_MODEM_ALL_NEGOTIATE)
    || (READ_WORD(bp_parms[0].info) == B1_MODEM_ASYNC)
    || (READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)))
  {
    return (add_modem_b23 (plci, bp_parms));
  }

  plci->B2_prot = (byte) READ_WORD(bp_parms[1].info);
  plci->B3_prot = (byte) READ_WORD(bp_parms[2].info);
  if(plci->B2_prot==12) SAPI = 0; /* default SAPI D-channel */


  if (plci->B2_prot == B2_PIAFS) {
    llc[1] = PIAFS_CRC;
  }
  else


  if (plci->B2_prot == B2_SS7) {
      dbug(1,dprintf("B2_SS7"));
    llc[1] = MTP2;
  }
  else


  if (plci->B2_prot == B2_LISTENING) {
      dbug(1,dprintf("B2_LISTENING"));
    llc[1] = LISTENER;
  }
  else

  {
    llc[1] = (plci->call_dir & (CALL_DIR_ORIGINATE | CALL_DIR_FORCE_OUTG_NL)) ?
             llc2_out[READ_WORD(bp_parms[1].info)] : llc2_in[READ_WORD(bp_parms[1].info)];
  }
  llc[2] = llc3[READ_WORD(bp_parms[2].info)];

  dlc[0] = 2;
  WRITE_WORD(&dlc[1], plci->appl->MaxDataLength +
                      header[READ_WORD(bp_parms[2].info)]);

  b1_config = &bp_parms[3];
  nlc[0] = 0;
  if(plci->B3_prot == 4
  || plci->B3_prot == 5)
  {
    for (i=0;i<sizeof(T30_INFO);i++) nlc[i] = 0;
    nlc[0] = sizeof(T30_INFO);
    if (plci->adapter->manufacturer_features2 & MANUFACTURER_FEATURE2_COLOR_FAX)
      ((T30_INFO *)&nlc[1])->operating_mode |= T30_OPERATING_MODE_BIT_INFO_EX;
    if (plci->call_dir & CALL_DIR_MONITOR)
    {
      ((T30_INFO *)&nlc[1])->operating_mode &= ~T30_OPERATING_MODE_NUMBER_MASK;
      ((T30_INFO *)&nlc[1])->operating_mode |= T30_OPERATING_MODE_MONITOR;
    }
    else if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
    {
      ((T30_INFO *)&nlc[1])->operating_mode &= ~T30_OPERATING_MODE_NUMBER_MASK;
      ((T30_INFO *)&nlc[1])->operating_mode |= T30_OPERATING_MODE_CAPI;
    }
    ((T30_INFO *)&nlc[1])->rate_div_2400 = 0xff;
    if(b1_config->length>=2)
    {
      ((T30_INFO *)&nlc[1])->rate_div_2400 = (byte)(READ_WORD(&b1_config->info[1])/2400);
    }
  }
  b2_config = &bp_parms[4];


  if (llc[1] == PIAFS_CRC)
  {
    if (plci->B3_prot != B3_TRANSPARENT)
    {
      return _B_STACK_NOT_SUPPORTED;
    }
    if(b2_config->length && api_parse(&b2_config->info[1], (word)b2_config->length, "bwww", b2_config_parms)) {
      return _WRONG_MESSAGE_FORMAT;
    }
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    dlc[3] = 0; /* Addr A */
    dlc[4] = 0; /* Addr B */
    dlc[5] = 0; /* modulo mode */
    dlc[6] = 0; /* window size */
    if (b2_config->length >= 7){
      dlc[ 7] = 7;
      dlc[ 8] = 0;
      dlc[ 9] = b2_config_parms[0].info[0]; /* PIAFS protocol Speed configuration */
      dlc[10] = b2_config_parms[1].info[0]; /* V.42bis P0 */
      dlc[11] = b2_config_parms[1].info[1]; /* V.42bis P0 */
      dlc[12] = b2_config_parms[2].info[0]; /* V.42bis P1 */
      dlc[13] = b2_config_parms[2].info[1]; /* V.42bis P1 */
      dlc[14] = b2_config_parms[3].info[0]; /* V.42bis P2 */
      dlc[15] = b2_config_parms[3].info[1]; /* V.42bis P2 */
      dlc[ 0] = 15;
      if(b2_config->length >= 8) { /* PIAFS control abilities */
        dlc[ 7] = 10;
        dlc[16] = 2; /* Length of PIAFS extention */
        dlc[17] = PIAFS_UDATA_ABILITIES; /* control (UDATA) ability */
        dlc[18] = b2_config_parms[4].info[0]; /* value */
        dlc[ 0] = 18;
      }
    }
    else /* default values, 64K, variable, no compression */
    {
      dlc[ 7] = 7;
      dlc[ 8] = 0;
      dlc[ 9] = 0x03; /* PIAFS protocol Speed configuration */
      dlc[10] = 0x03; /* V.42bis P0 */
      dlc[11] = 0;    /* V.42bis P0 */
      dlc[12] = 0;    /* V.42bis P1 */
      dlc[13] = 0;    /* V.42bis P1 */
      dlc[14] = 0;    /* V.42bis P2 */
      dlc[15] = 0;    /* V.42bis P2 */
      dlc[ 0] = 15;
    }
  }
  else


  if (llc[1] == MTP2)
  {
    if (plci->B3_prot != B3_TRANSPARENT) {
      return _B_STACK_NOT_SUPPORTED;
    }

    if(b2_config->length && api_parse(&b2_config->info[1], (word)b2_config->length, "b", b2_config_parms)) {
      return _WRONG_MESSAGE_FORMAT;
    }
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    dlc[3] = 0; /* Addr A */
    dlc[4] = 0; /* Addr B */
    dlc[5] = 0; /* modulo mode */
    dlc[6] = 0; /* window size */

    if (b2_config->length != 0)
    {
      if(b2_config->length != 1) {
        return _WRONG_MESSAGE_FORMAT;
      }
      dlc[7] = (byte) 1;
      dlc[8] = (byte) 0;
      dlc[9] = (byte)b2_config_parms[0].info[0]; /* normal/preventive mode */
      dlc[0] = (byte)9;
    }
    else {
      dlc[0] = (byte)6;
    }
  }
  else


  if (llc[1] == LISTENER) {
    WRITE_WORD(&dlc[1],plci->appl->MaxDataLength);
    if(b2_config->length && api_parse(&b2_config->info[1], (word)b2_config->length, "wws", b2_config_parms)) {
      return _WRONG_MESSAGE_FORMAT;
    }
    dlc[3] = 0; /* Addr A */
    dlc[4] = 0; /* Addr B */
    dlc[5] = 0; /* modulo mode */
    dlc[6] = 0; /* window size */
    dlc[0] = 6;

    if (b2_config->length >= 5){
      dlc[0] += 2;
      dlc[++(dlc[0])] = b2_config_parms[0].info[0]; /* B2 protocol to listen */
      dlc[++(dlc[0])] = b2_config_parms[0].info[1]; /* B2 protocol to listen */
      dlc[++(dlc[0])] = b2_config_parms[1].info[0]; /* Listening feature mask */
      dlc[++(dlc[0])] = b2_config_parms[1].info[1]; /* Listening feature mask */
      for (i = 0; i < b2_config_parms[2].length; i++)
        dlc[++(dlc[0])] = b2_config_parms[2].info[1+i];
      dlc[7] = dlc[0] - 8;
      dlc[8] = 0;
    }
  }
  else


  if ((llc[1] == V120_L2) || (llc[1] == V120_V42BIS))
  {
    if (plci->B3_prot != B3_TRANSPARENT)
      return _B_STACK_NOT_SUPPORTED;

    dlc[0] = 6;
    WRITE_WORD (&dlc[1], READ_WORD (&dlc[1]) + 2);
    dlc[3] = 0x08;
    dlc[4] = 0x01;
    dlc[5] = 127;
    dlc[6] = 7;
    if (b2_config->length != 0)
    {
      if((llc[1]==V120_V42BIS) && api_parse(&b2_config->info[1], (word)b2_config->length, "bbbbwww", b2_config_parms)) {
        return _WRONG_MESSAGE_FORMAT;
      }
      dlc[3] = (byte)((b2_config->info[2] << 3) | ((b2_config->info[1] >> 5) & 0x04));
      dlc[4] = (byte)((b2_config->info[1] << 1) | 0x01);
      if ((b2_config->info[3] & (128 | 8)) != 128)
      {
        dbug(1,dprintf("1D-dlc= %x %x %x %x %x", dlc[0], dlc[1], dlc[2], dlc[3], dlc[4]));
        return _B2_PARM_NOT_SUPPORTED;
      }
      dlc[5] = (byte)((b2_config->info[3] & (128 | 8)) - 1);
      dlc[6] = b2_config->info[4];
      if(llc[1]==V120_V42BIS){
        if (b2_config->length >= 10){
          dlc[ 7] = 6;
          dlc[ 8] = 0;
          dlc[ 9] = b2_config_parms[4].info[0];
          dlc[10] = b2_config_parms[4].info[1];
          dlc[11] = b2_config_parms[5].info[0];
          dlc[12] = b2_config_parms[5].info[1];
          dlc[13] = b2_config_parms[6].info[0];
          dlc[14] = b2_config_parms[6].info[1];
          dlc[ 0] = 14;
          dbug(1,dprintf("b2_config_parms[4].info[0] [1]:  %x %x", b2_config_parms[4].info[0], b2_config_parms[4].info[1]));
          dbug(1,dprintf("b2_config_parms[5].info[0] [1]:  %x %x", b2_config_parms[5].info[0], b2_config_parms[5].info[1]));
          dbug(1,dprintf("b2_config_parms[6].info[0] [1]:  %x %x", b2_config_parms[6].info[0], b2_config_parms[6].info[1]));
        }
      }
      if (b2_config->info[3] & ~(128 | 8)) /* other bits used as options */
      {
        if (dlc[0] < 8)
        {
          dlc[0] = 8;
          dlc[7] = 0;
          dlc[8] = 0;
        }
        dlc[++(dlc[0])] = b2_config->info[3] & ~(128 | 8);
      }
    }
  }
  else
  {
    if(b2_config->length)
    {
      dbug(1,dprintf("B2-Config"));
      if(llc[1]==X75_V42BIS){
        if(api_parse(&b2_config->info[1], (word)b2_config->length, "bbbbwww", b2_config_parms))
        {
          return _WRONG_MESSAGE_FORMAT;
        }
      }
      else {
        if(api_parse(&b2_config->info[1], (word)b2_config->length, "bbbbs", b2_config_parms))
        {
          return _WRONG_MESSAGE_FORMAT;
        }
      }
          /* if B2 Protocol is LAPD, b2_config structure is different */
      if(llc[1]==6)
      {
        dlc[0] = 4;
        if(b2_config->length>=1) dlc[2] = b2_config->info[1];      /* TEI */
        else dlc[2] = 0x01;
        if( (b2_config->length>=2) && (plci->B2_prot==12) )
        {
          SAPI = b2_config->info[2];    /* SAPI */
        }
        dlc[1] = SAPI;
        if( (b2_config->length>=3) && (b2_config->info[3]==128) )
        {
          dlc[3] = 127;      /* Mode */
        }
        else
        {
          dlc[3] = 7;        /* Mode */
        }

        if(b2_config->length>=4) dlc[4] = b2_config->info[4];      /* Window */
        else dlc[4] = 1;
        dbug(1,dprintf("D-dlc[%d]=%x,%x,%x,%x", dlc[0], dlc[1], dlc[2], dlc[3], dlc[4]));
        if(b2_config->length>5) return _B2_PARM_NOT_SUPPORTED;
      }
      else
      {
        dlc[0] = 6;
        dlc[3] = b2_config->info[1];
        dlc[4] = b2_config->info[2];
        if(b2_config->info[3]!=8 && b2_config->info[3]!=128){
          dbug(1,dprintf("1D-dlc= %x %x %x %x %x", dlc[0], dlc[1], dlc[2], dlc[3], dlc[4]));
          return _B2_PARM_NOT_SUPPORTED;
        }

        dlc[5] = (byte)(b2_config->info[3]-1);
        dlc[6] = b2_config->info[4];
        if(dlc[6]>dlc[5]){
          dbug(1,dprintf("2D-dlc= %x %x %x %x %x %x %x", dlc[0], dlc[1], dlc[2], dlc[3], dlc[4], dlc[5], dlc[6]));
          return _B2_PARM_NOT_SUPPORTED;
        }

        if(llc[1]==X75_V42BIS) {
          if (b2_config->length >= 10){
            dlc[ 7] = 6;
            dlc[ 8] = 0;
            dlc[ 9] = b2_config_parms[4].info[0];
            dlc[10] = b2_config_parms[4].info[1];
            dlc[11] = b2_config_parms[5].info[0];
            dlc[12] = b2_config_parms[5].info[1];
            dlc[13] = b2_config_parms[6].info[0];
            dlc[14] = b2_config_parms[6].info[1];
            dlc[ 0] = 14;
            dbug(1,dprintf("b2_config_parms[4].info[0] [1]:  %x %x", b2_config_parms[4].info[0], b2_config_parms[4].info[1]));
            dbug(1,dprintf("b2_config_parms[5].info[0] [1]:  %x %x", b2_config_parms[5].info[0], b2_config_parms[5].info[1]));
            dbug(1,dprintf("b2_config_parms[6].info[0] [1]:  %x %x", b2_config_parms[6].info[0], b2_config_parms[6].info[1]));
          }
        }
        else {
          if (b2_config_parms[4].length)
          {
            dlc[0] = (byte)(b2_config_parms[4].length+10);
            WRITE_WORD(&dlc[7], (word)b2_config_parms[4].length + 2);
            for(i=0; i<b2_config_parms[4].length; i++)
              dlc[11+i] = b2_config_parms[4].info[1+i];
          }
          if ((plci->B2_prot == B2_SDLC)
           && ((READ_WORD(bp_parms[0].info) == B1_MODEM_ALL_NEGOTIATE)
            || (READ_WORD(bp_parms[0].info) == B1_MODEM_ASYNC)
            || (READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)))
          {
            if (dlc[0] < 8)
            {
              dlc[0] = 8;
              dlc[7] = 0;
              dlc[8] = 0;
            }
            i = SDLC_OPTION_WAIT_CTS_DCD_ON |
                SDLC_OPTION_SINGLE_DATA_PACKETS |
                SDLC_OPTION_FAST_POLL_RECOVERY;
            if (plci->B3_prot == B3_MODEM)
              i |= SDLC_OPTION_INDICATE_CTS_DCD_ON;
            dlc[++(dlc[0])] = (byte) i;
          }
          if ((READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)
           && ((llc[1] == X75T) || (llc[1] == X75_V42BIS)))
          {
            if (dlc[0] < 8)
            {
              dlc[0] = 8;
              dlc[7] = 0;
              dlc[8] = 0;
            }
            i = X75_OPTION_WAIT_CTS_DCD_ON;
            dlc[++(dlc[0])] = (byte) i;
          }
        }
      }
    }
    else
    {
      if ((plci->B2_prot == B2_SDLC)
       && ((READ_WORD(bp_parms[0].info) == B1_MODEM_ALL_NEGOTIATE)
        || (READ_WORD(bp_parms[0].info) == B1_MODEM_ASYNC)
        || (READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)))
      {
        dlc[0] = 8;
        dlc[3] = 0xc1;
        dlc[4] = 1;
        dlc[5] = 7;
        dlc[6] = 7;
        dlc[7] = 0;
        dlc[8] = 0;
        i = SDLC_OPTION_WAIT_CTS_DCD_ON |
            SDLC_OPTION_SINGLE_DATA_PACKETS |
            SDLC_OPTION_FAST_POLL_RECOVERY;
        if (plci->B3_prot == B3_MODEM)
          i |= SDLC_OPTION_INDICATE_CTS_DCD_ON;
        dlc[++(dlc[0])] = (byte) i;
      }
      if ((READ_WORD(bp_parms[0].info) == B1_MODEM_SYNC_HDLC)
       && ((llc[1] == X75T) || (llc[1] == X75_V42BIS)))
      {
        dlc[0] = 8;
        dlc[3] = 3;
        dlc[4] = 1;
        dlc[5] = 7;
        dlc[6] = 7;
        dlc[7] = 0;
        dlc[8] = 0;
        i = X75_OPTION_WAIT_CTS_DCD_ON;
        dlc[++(dlc[0])] = (byte) i;
      }
    }
  }

  b3_config = &bp_parms[5];
  if(plci->B3_prot == 4
  || plci->B3_prot == 5)
  {
    if(b3_config->length)
    {
      if(api_parse(&b3_config->info[1], (word)b3_config->length, "wwss", b3_config_parms))
      {
        return _WRONG_MESSAGE_FORMAT;
      }
    }
    else
    {
      for (i = 0; i < 4; i++)
        b3_config_parms[i].length = 0;
    }
    i = (b3_config->length >= 2) ? READ_WORD((byte   *)(b3_config_parms[0].info)) : 0;
    data_format = (b3_config->length >= 4) ? READ_WORD((byte   *)b3_config_parms[1].info) : 0;
    resolution = ((i & 0x0001) || ((plci->B3_prot == 4) && (((byte) data_format) != 5))) ? T30_RESOLUTION_R8_0770_OR_200 : 0;
    ((T30_INFO *)&nlc[1])->data_format = (byte) data_format;
    fax_control_bits = T30_CONTROL_BIT_ALL_FEATURES;
    fax_control2_bits = 0;
    if ((((T30_INFO *)&nlc[1])->rate_div_2400 != 0) && (((T30_INFO *)&nlc[1])->rate_div_2400 <= 6))
      fax_control_bits &= ~T30_CONTROL_BIT_ENABLE_V34FAX;
    if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
    {

      if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
        & (1L << PRIVATE_FAX_PAPER_FORMATS))
      {
        resolution |= T30_RESOLUTION_R8_1540 | T30_RESOLUTION_R16_1540_OR_400 |
          T30_RESOLUTION_300_300 | T30_RESOLUTION_300_600 | T30_RESOLUTION_400_800 |
          T30_RESOLUTION_600_1200 | T30_RESOLUTION_600_600 | T30_RESOLUTION_1200_1200 |
          T30_RESOLUTION_INCH_BASED | T30_RESOLUTION_METRIC_BASED;
      }

      ((T30_INFO *)&nlc[1])->recording_properties =
        T30_RECORDING_WIDTH_ISO_A3 |
        (T30_RECORDING_LENGTH_UNLIMITED << 2) |
        (T30_MIN_SCANLINE_TIME_00_00_00 << 4);
    }
    if(plci->B3_prot == 5)
    {
      if (i & 0x0002) /* Accept incoming fax-polling requests */
        fax_control_bits |= T30_CONTROL_BIT_ACCEPT_POLLING;
      if (i & 0x0400) /* Enable JPEG negotiation */
      {
        fax_control2_bits |= T30_CONTROL2_BIT_ENABLE_JPEG | T30_CONTROL2_BIT_FULL_COLOR_MODE |
          T30_CONTROL2_BIT_ENABLE_12BIT_MODE | T30_CONTROL2_BIT_CUSTOM_ILLUMINANT |
          T30_CONTROL2_BIT_CUSTOM_GAMMUT | T30_CONTROL2_BIT_NO_SUBSAMPLING;
        resolution |= T30_RESOLUTION_R4_0385_OR_100 | T30_RESOLUTION_300_400_JPEG |
          T30_RESOLUTION_600_600_JPEG | T30_RESOLUTION_1200_1200_JPEG;
      }
      if (i & 0x0800) /* Enable T.43 negotiation */
      {
        fax_control2_bits |= T30_CONTROL2_BIT_ENABLE_T43_CODING | T30_CONTROL2_BIT_FULL_COLOR_MODE |
          T30_CONTROL2_BIT_ENABLE_12BIT_MODE | T30_CONTROL2_BIT_CUSTOM_ILLUMINANT |
          T30_CONTROL2_BIT_CUSTOM_GAMMUT | T30_CONTROL2_BIT_PLANE_INTERLEAVE;
        resolution |= T30_RESOLUTION_R4_0385_OR_100 | T30_RESOLUTION_300_400_JPEG |
          T30_RESOLUTION_600_600_JPEG | T30_RESOLUTION_1200_1200_JPEG;
      }
      if (i & 0x1000) /* Do not use T.85 compression */
        fax_control_bits &= ~T30_CONTROL_BIT_ENABLE_T85_CODING;
      if (i & 0x2000) /* Do not use MR compression */
        fax_control_bits &= ~T30_CONTROL_BIT_ENABLE_2D_CODING;
      if (i & 0x4000) /* Do not use MMR compression */
        fax_control_bits &= ~T30_CONTROL_BIT_ENABLE_T6_CODING;
      if (i & 0x8000) /* Do not use ECM */
        fax_control_bits &= ~T30_CONTROL_BIT_ENABLE_ECM;
      if (plci->fax_connect_info_length != 0)
      {
        resolution = ((T30_INFO   *)plci->fax_connect_info_buffer)->resolution |
          (((T30_INFO   *)plci->fax_connect_info_buffer)->resolution_high << 8);
        ((T30_INFO *)&nlc[1])->data_format = ((T30_INFO   *)plci->fax_connect_info_buffer)->data_format;
        ((T30_INFO *)&nlc[1])->recording_properties = ((T30_INFO   *)plci->fax_connect_info_buffer)->recording_properties;
        fax_control_bits |= READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low) &
          (T30_CONTROL_BIT_REQUEST_POLLING | T30_CONTROL_BIT_MORE_DOCUMENTS);
        fax_control2_bits |= READ_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low) &
          T30_CONTROL2_BIT_REQUEST_PRI;
      }
    }
    ((T30_INFO *)&nlc[1])->resolution = (byte) resolution;
    ((T30_INFO *)&nlc[1])->resolution_high = (byte)(resolution >> 8);
    /* copy station id to NLC */
    for(i=0; i<20; i++)
    {
      if(i<b3_config_parms[2].length)
      {
        ((T30_INFO *)&nlc[1])->station_id[i] = ((byte   *)b3_config_parms[2].info)[1+i];
      }
      else
      {
        ((T30_INFO *)&nlc[1])->station_id[i] = ' ';
      }
    }
    ((T30_INFO *)&nlc[1])->station_id_len = 20;
    /* copy head line to NLC */
    if(b3_config_parms[3].length)
    {

      pos = (byte)(fax_head_line_time (&(((byte *)(((T30_INFO *)&nlc[1])->station_id))[20]), plci->adapter->u_law));
      if (pos != 0)
      {
        if (CAPI_MAX_DATE_TIME_LENGTH + 2 + b3_config_parms[3].length > CAPI_MAX_HEAD_LINE_SPACE)
          pos = 0;
        else
        {
          ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ' ';
          ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ' ';
          len = (byte)b3_config_parms[2].length;
          if (len > 20)
            len = 20;
          if (CAPI_MAX_DATE_TIME_LENGTH + 2 + len + 2 + b3_config_parms[3].length <= CAPI_MAX_HEAD_LINE_SPACE)
          {
            for (i = 0; i < len; i++)
              ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ((byte   *)b3_config_parms[2].info)[1+i];
            ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ' ';
            ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ' ';
          }
        }
      }

      len = (byte)b3_config_parms[3].length;
      if (len > CAPI_MAX_HEAD_LINE_SPACE - pos)
        len = (byte)(CAPI_MAX_HEAD_LINE_SPACE - pos);
      ((T30_INFO *)&nlc[1])->head_line_len = (byte)(pos + len);
      nlc[0] += (byte)(pos + len);
      for (i = 0; i < len; i++)
        ((byte *)(((T30_INFO *)&nlc[1])->station_id))[20 + pos++] = ((byte   *)b3_config_parms[3].info)[1+i];
    }
    else
      ((T30_INFO *)&nlc[1])->head_line_len = 0;

    plci->nsf_control_bits = 0;
    if(plci->B3_prot == 5)
    {
      if ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_FAX_SUB_SEP_PWD))
       && (data_format & 0x8000)) /* Private SUB/SEP/PWD enable */
      {
        plci->requested_options |= 1L << PRIVATE_FAX_SUB_SEP_PWD;
      }
      if ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
       && (data_format & 0x4000)) /* Private non-standard facilities enable */
      {
        plci->requested_options |= 1L << PRIVATE_FAX_NONSTANDARD;
      }
      if ((plci->adapter->profile.Manuf.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
       && (data_format & 0x0100)) /* Accept procedure interrupts */
      {
        fax_control2_bits |= T30_CONTROL2_BIT_ACCEPT_PRI;
      }
      if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
        & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
      {
        if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
          & (1L << PRIVATE_FAX_SUB_SEP_PWD))
        {
          fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
          if (fax_control_bits & T30_CONTROL_BIT_ACCEPT_POLLING)
            fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SEL_POLLING;
        }
        len = nlc[0];
        pos = (byte)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0));
        if (pos < plci->fax_connect_info_length)
        {
          for (i = 1 + plci->fax_connect_info_buffer[pos]; i != 0; i--)
            nlc[++len] = plci->fax_connect_info_buffer[pos++];
        }
        else
          nlc[++len] = 0;
        if (pos < plci->fax_connect_info_length)
        {
          for (i = 1 + plci->fax_connect_info_buffer[pos]; i != 0; i--)
            nlc[++len] = plci->fax_connect_info_buffer[pos++];
        }
        else
          nlc[++len] = 0;
        if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
          & (1L << PRIVATE_FAX_NONSTANDARD))
        {
          if ((pos < plci->fax_connect_info_length) && (plci->fax_connect_info_buffer[pos] != 0))
          {
            if ((plci->fax_connect_info_buffer[pos] >= 3) && (plci->fax_connect_info_buffer[pos+1] >= 2))
              plci->nsf_control_bits = READ_WORD(&plci->fax_connect_info_buffer[pos+2]);
            for (i = 1 + plci->fax_connect_info_buffer[pos]; i != 0; i--)
              nlc[++len] = plci->fax_connect_info_buffer[pos++];
          }
          else
          {
            if(api_parse(&b3_config->info[1], (word)b3_config->length, "wwsss", b3_config_parms))
            {
              dbug(1,dprintf("non-standard facilities info missing or wrong format"));
              nlc[++len] = 0;
            }
            else
            {
              if ((b3_config_parms[4].length >= 3) && (b3_config_parms[4].info[1] >= 2))
                plci->nsf_control_bits = READ_WORD(&b3_config_parms[4].info[2]);
              nlc[++len] = (byte)(b3_config_parms[4].length);
              for (i = 0; i < b3_config_parms[4].length; i++)
                nlc[++len] = b3_config_parms[4].info[1+i];
            }
          }
        }
        nlc[0] = len;
        if ((plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_FAX_NONSTANDARD)
         && (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
        {
          ((T30_INFO *)&nlc[1])->operating_mode &= ~T30_OPERATING_MODE_NUMBER_MASK;
          ((T30_INFO *)&nlc[1])->operating_mode |= T30_OPERATING_MODE_CAPI_NEG;
        }
      }
    }

    WRITE_WORD(&(((T30_INFO *)&nlc[1])->control_bits_low), fax_control_bits);
    WRITE_WORD(&(((T30_INFO *)&nlc[1])->feature_bits_low), fax_control2_bits);
    len = (byte)(((byte *)(((T30_INFO *) 0)->station_id + 20)) - ((byte *) 0));
    for (i = 0; i < len; i++)
      plci->fax_connect_info_buffer[i] = nlc[1+i];
    ((T30_INFO   *) plci->fax_connect_info_buffer)->head_line_len = 0;
    i += ((T30_INFO *)&nlc[1])->head_line_len;
    while (i < nlc[0])
      plci->fax_connect_info_buffer[len++] = nlc[++i];
    plci->fax_connect_info_length = len;
  }
  else
  {
    if (b3_config->length)
    {
      nlc[0] = 14;

      if((b3_config->length ==16) || (b3_config->length ==18) ) {
      }
      else

      {
        if(b3_config->length!=16)
          return _B3_PARM_NOT_SUPPORTED;
      }
      for(i=0; i<12; i++) nlc[1+i] = b3_config->info[1+i];
      if(READ_WORD(&b3_config->info[13])!=8 && READ_WORD(&b3_config->info[13])!=128)
        return _B3_PARM_NOT_SUPPORTED;
      nlc[13] = b3_config->info[13];
      if(READ_WORD(&b3_config->info[15])>=nlc[13])
        return _B3_PARM_NOT_SUPPORTED;
      nlc[14] = b3_config->info[15];

      if(b3_config->length ==18) {
        nlc[0] = 16;  /* add Private X.25 Extensions */
        if(b3_config->info[17] != 1) {
          return _B3_PARM_NOT_SUPPORTED;
        }
        nlc[15] = b3_config->info[17];
        nlc[16] = b3_config->info[18];
      }

    }
  }
  add_p(plci, LLI, lli);
  add_p(plci, LLC, llc);
  add_p(plci, DLC, dlc);
  add_p(plci, NLC, nlc);
  if (plci->stream_data[0] != 0) {
    add_p(plci, SIG, plci->stream_data);
  }
  return 0;
}

/*----------------------------------------------------------------*/
/*      make the same as add_b23, but only for the modem related  */
/*      L2 and L3 B-Chan protocol.                                */
/*                                                                */
/*      Enabled L2 and L3 Configurations:                         */
/*        If L1 == Modem all negotiation                          */
/*          only L2 == Modem with full negotiation is allowed     */
/*        If L1 == Modem async or sync                            */
/*          only L2 == Transparent is allowed                     */
/*        L3 == Modem or L3 == Transparent are allowed            */
/*      B2 Configuration for modem:                               */
/*          word : enable/disable compression, bitoptions         */
/*      B3 Configuration for modem:                               */
/*          empty                                                 */
/*----------------------------------------------------------------*/
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_parms)
{
  static byte lli[12] = {1,1};
  static byte llc[3] = {2,0,0};
  static byte dlc[64];
    API_PARSE mdm_cfg[4];
    API_PARSE sdlc_cfg[6];
  word i, j, n;
  word w;
  word options;

  for(i=0;i<4;i++) mdm_cfg[i].length = 0;

  if (((READ_WORD(bp_parms[0].info) == B1_MODEM_ALL_NEGOTIATE)
    && (READ_WORD(bp_parms[1].info) != B2_MODEM_EC_COMPRESSION))
   || ((READ_WORD(bp_parms[0].info) != B1_MODEM_ALL_NEGOTIATE)
    && (READ_WORD(bp_parms[1].info) != B2_TRANSPARENT)))
  {
    return (_B_STACK_NOT_SUPPORTED);
  }
  if ((READ_WORD(bp_parms[2].info) != B3_MODEM)
   && (READ_WORD(bp_parms[2].info) != B3_TRANSPARENT))
  {
    return (_B_STACK_NOT_SUPPORTED);
  }

  plci->B2_prot = (byte) READ_WORD(bp_parms[1].info);
  plci->B3_prot = (byte) READ_WORD(bp_parms[2].info);

  /* OK, L2 is modem */

  lli[0] = 1;
  lli[1] = 1;
  if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)
    lli[1] |= 2;
  if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_OOB_CHANNEL)
    lli[1] |= 4;

  if ((lli[1] & 0x02) && (diva_xdi_extended_features & DIVA_CAPI_USE_CMA)) {
    lli[1] |= 0x10;
    if (plci->rx_dma_descriptor <= 0) {
      plci->rx_dma_descriptor=diva_get_dma_descriptor(plci,&plci->rx_dma_magic);
      if (plci->rx_dma_descriptor >= 0)
        plci->rx_dma_descriptor++;
    }
    if (plci->rx_dma_descriptor > 0) {
      lli[1] |= 0x40;
      lli[0] = 6;
      lli[2] = (byte)(plci->rx_dma_descriptor - 1);
      lli[3] = (byte)plci->rx_dma_magic;
      lli[4] = (byte)(plci->rx_dma_magic >>  8);
      lli[5] = (byte)(plci->rx_dma_magic >> 16);
      lli[6] = (byte)(plci->rx_dma_magic >> 24);
    }
  }

  if (DIVA_CAPI_SUPPORTS_NO_CANCEL(plci->adapter)) {
    lli[1] |= 0x20;
  }

  llc[1] = (plci->call_dir & (CALL_DIR_ORIGINATE | CALL_DIR_FORCE_OUTG_NL)) ?
    /*V42*/ 10 : /*V42_IN*/ 9;
  llc[2] = 4;                      /* pass L3 always transparent */
  i = 1;
  WRITE_WORD (&dlc[i], plci->appl->MaxDataLength);
  i += 2;
  if (READ_WORD(bp_parms[1].info) == B2_MODEM_EC_COMPRESSION)
  {
    if (bp_parms[4].length)
    {
      if (api_parse (&bp_parms[4].info[1], (word)bp_parms[4].length, "w", mdm_cfg))
      {
        return (_WRONG_MESSAGE_FORMAT);
      }
      options = READ_WORD(mdm_cfg[0].info);

      dbug(1, dprintf("MDM options=%02x", options));
      dlc[i++] = 3; /* Addr A */
      dlc[i++] = 1; /* Addr B */
      dlc[i++] = 7; /* modulo mode */
      dlc[i++] = 7; /* window size */
      dlc[i++] = 0; /* XID len Lo  */
      dlc[i++] = 0; /* XID len Hi  */
      dlc[i] = 0;   /* negotiation options */
      if (options & MDM_B2_DISABLE_V42bis)
      {
        dlc[i] |= DLC_MODEMPROT_DISABLE_V42_V42BIS;
      }
      if (options & MDM_B2_DISABLE_MNP)
      {
        dlc[i] |= DLC_MODEMPROT_DISABLE_MNP_MNP5;
      }
      if (options & MDM_B2_DISABLE_TRANS)
      {
        dlc[i] |= DLC_MODEMPROT_REQUIRE_PROTOCOL;
      }
      if (options & MDM_B2_DISABLE_V42)
      {
        dlc[i] |= DLC_MODEMPROT_DISABLE_V42_DETECT;
      }
      if (options & MDM_B2_DISABLE_COMP)
      {
        dlc[i] |= DLC_MODEMPROT_DISABLE_COMPRESSION;
      }
      i++;


      if ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
        & ((1L << PRIVATE_V18) | (1L << PRIVATE_VOWN)))
      {
        if (!api_parse(&bp_parms[4].info[1],(word)bp_parms[4].length,"ws", mdm_cfg))
        {

          dlc[i-1] |= (mdm_cfg[1].length >= 1) ? mdm_cfg[1].info[1] : 0; 
          dlc[i++] = (mdm_cfg[1].length >= 2) ? mdm_cfg[1].info[2] : 0; 
          dlc[i++] = (mdm_cfg[1].length >= 3) ? mdm_cfg[1].info[3] : 0;  
          plci->break_configuration = dlc[i-1] & DLC_MODEMPROT_BREAK_CONFIG_MASK;
          dlc[i++] = (mdm_cfg[1].length >= 4) ? mdm_cfg[1].info[4] : 0;  
          dlc[i++] = 0;
          if (mdm_cfg[1].length >= 5)
          {

            n = mdm_cfg[1].length - 4;
            dlc[i-1] = (byte)n;

            dlc[i++] = mdm_cfg[1].info[5]; 

            if (mdm_cfg[1].length >= 7)
            {

              w = READ_WORD(&mdm_cfg[1].info[6]);  
              dlc[i++] = (byte) w;
              dlc[i++] = (byte)(w >> 8);
              
              if (mdm_cfg[1].length >= 8)
              {

                for (j = 8; j <= mdm_cfg[1].length; j++)
                  dlc[i++] = mdm_cfg[1].info[j];
              }
            }
          }
          if (!api_parse(&bp_parms[4].info[1],(word)bp_parms[4].length,"wss", mdm_cfg))
          {
            dlc[i++] = 0;
            if (mdm_cfg[2].length)
            {
              if(api_parse(&mdm_cfg[2].info[1], (word)mdm_cfg[2].length, "bbbbs", sdlc_cfg))
              {
                return _WRONG_MESSAGE_FORMAT;
              }
              WRITE_WORD (&dlc[i], plci->appl->MaxDataLength);
              dlc[i+2] = mdm_cfg[2].info[1];
              dlc[i+3] = mdm_cfg[2].info[2];
              dlc[i+4] = (byte)(mdm_cfg[2].info[3]-1);
              dlc[i+5] = mdm_cfg[2].info[4];
              if (sdlc_cfg[4].length)
              {

                WRITE_WORD(&dlc[i+6], (word)sdlc_cfg[4].length + 2);
                for(j=0; j<sdlc_cfg[4].length; j++)
                  dlc[i+j+10] = sdlc_cfg[4].info[1+j];
                j += 10;
              }
              else
              {
                dlc[i+6] = 0;
                dlc[i+7] = 0;
                j = 8;
              }
              dlc[i+(j++)] = SDLC_OPTION_WAIT_CTS_DCD_ON;
              if (plci->B3_prot == B3_MODEM)
                dlc[i+(j-1)] |= SDLC_OPTION_INDICATE_CTS_DCD_ON;
              dlc[i-1] = (byte) j;
              n = (word)((mdm_cfg[2].info + mdm_cfg[2].length) - (sdlc_cfg[4].info + sdlc_cfg[4].length));
              if (n >= 1)
              {

                dlc[i+(j-1)] |= sdlc_cfg[4].info[1 + sdlc_cfg[4].length + j - dlc[i-1]];
                while (j < dlc[i-1] + n - 1)
                {
                  j++;
                  dlc[i+(j-1)] = sdlc_cfg[4].info[1 + sdlc_cfg[4].length + j - dlc[i-1]];
                }
                dlc[i-1] = (byte) j;
              }
              i += j;
            }
          }
        }
      }

    }
  }
  else
  {
    dlc[i++] = 3; /* Addr A */
    dlc[i++] = 1; /* Addr B */
    dlc[i++] = 7; /* modulo mode */
    dlc[i++] = 7; /* window size */
    dlc[i++] = 0; /* XID len Lo  */
    dlc[i++] = 0; /* XID len Hi  */
    dlc[i++] =  /* negotiation options */
      DLC_MODEMPROT_DISABLE_V42_V42BIS |
      DLC_MODEMPROT_DISABLE_MNP_MNP5 |
      DLC_MODEMPROT_DISABLE_SDLC;
  }
  dlc[0] = (byte)(i - 1);
//HexDump ("DLC", sizeof(dlc), &dlc[0]);
  add_p(plci, LLI, lli);
  add_p(plci, LLC, llc);
  add_p(plci, DLC, dlc);
  return (0);
}


/*------------------------------------------------------------------*/
/* send a request for the signaling entity                          */
/*------------------------------------------------------------------*/

void sig_req_(PLCI   * plci, byte req, byte Id)
{
static char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  static byte esc_uid[8];

  if(!plci) return;
  if(plci->adapter->adapter_disabled) return;
  esc_uid[0] = 4;
  esc_uid[1] = UID;
  esc_uid[2] = 'C';
  esc_uid[3] = hex_digit_table[(plci->Id >> 4) & 0xf];
  esc_uid[4] = hex_digit_table[plci->Id & 0xf];
  add_p(plci,ESC,esc_uid);
  dbug(1,dprintf("sig_req(%x)",req));
  if (req == REMOVE)
    plci->sig_remove_id = plci->Sig.Id;
  if(plci->req_in==plci->req_in_start) {
    plci->req_in +=2;
    plci->RBuffer[plci->req_in++] = 0;
  }
  WRITE_WORD(&plci->RBuffer[plci->req_in_start], plci->req_in-plci->req_in_start-2);
  plci->RBuffer[plci->req_in++] = Id;   /* sig/nl flag */
  plci->RBuffer[plci->req_in++] = req;  /* request */
  plci->RBuffer[plci->req_in++] = 0;    /* channel */
  plci->req_in_start = plci->req_in;

  if (plci->req_in > sizeof(plci->RBuffer))
  {
    DBG_FTL(("[%06lx] %s,%d: FATAL request queue overrun",
      (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
      (char   *)(FILE_), __LINE__));
  }
}

/*------------------------------------------------------------------*/
/* send a request for the network layer entity                      */
/*------------------------------------------------------------------*/

void nl_req_ncci_(PLCI   * plci, byte req, word ncci)
{
  if(!plci) return;
  if(plci->adapter->adapter_disabled) return;
  dbug(1,dprintf("nl_req %02x %02x %02x", plci->Id, req, ncci));
  if (req == REMOVE)
  {
    plci->nl_remove_id = plci->NL.Id;
    plci->nl_remove_ncci = ncci;
    ncci = 0;
  }
  if(plci->req_in==plci->req_in_start) {
    plci->req_in +=2;
    plci->RBuffer[plci->req_in++] = 0;
  }
  WRITE_WORD(&plci->RBuffer[plci->req_in_start], plci->req_in-plci->req_in_start-2);
  plci->RBuffer[plci->req_in++] = 1;    /* sig/nl flag */
  plci->RBuffer[plci->req_in++] = req;  /* request */
  plci->RBuffer[plci->req_in++] = plci->adapter->ncci_ch[(byte)ncci];   /* channel */
  plci->req_in_start = plci->req_in;

  if (plci->req_in > sizeof(plci->RBuffer))
  {
    DBG_FTL(("[%06lx] %s,%d: FATAL request queue overrun",
      (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
      (char   *)(FILE_), __LINE__));
  }
}

void send_req(PLCI   * plci)
{
  ENTITY   * e;
  word l;
//  word i;

  if(!plci) return;
  if(plci->adapter->adapter_disabled) return;
  channel_xmit_xon (plci);

        /* if nothing to do, return */
  if(plci->req_in==plci->req_out) return;
  dbug(1,dprintf("send_req(in=%d,out=%d)",plci->req_in,plci->req_out));

  if(plci->nl_req || plci->sig_req) return;

  l = READ_WORD(&plci->RBuffer[plci->req_out]);
  plci->req_out += 2;
  plci->XData[0].P = &plci->RBuffer[plci->req_out];
  plci->req_out += l;
  if(plci->RBuffer[plci->req_out]==1)
  {
    e = &plci->NL;
    plci->req_out++;
    e->Req = plci->nl_req = plci->RBuffer[plci->req_out++];
    e->ReqCh = plci->RBuffer[plci->req_out++];
    if(!(e->Id & 0x1f))
    {
      e->Id = NL_ID;
      plci->RBuffer[plci->req_out-4] = CAI;
      plci->RBuffer[plci->req_out-3] = 1;
      plci->RBuffer[plci->req_out-2] = (plci->Sig.Id==0xff) ? 0 : plci->Sig.Id;
      plci->RBuffer[plci->req_out-1] = 0;
      l+=3;
      plci->nl_global_req = plci->nl_req;
    }
    dbug(1,dprintf("%x:NLREQ(%x:%x:%x)",plci->adapter->Id,e->Id,e->Req,e->ReqCh));
    trcq (plci);
  }
  else
  {
    e = &plci->Sig;
    if(plci->RBuffer[plci->req_out])
      e->Id = plci->RBuffer[plci->req_out];
    plci->req_out++;
    e->Req = plci->sig_req = plci->RBuffer[plci->req_out++];
    e->ReqCh = plci->RBuffer[plci->req_out++];
    if(!(e->Id & 0x1f))
      plci->sig_global_req = plci->sig_req;
    dbug(1,dprintf("%x:SIGREQ(%x:%x:%x)",plci->adapter->Id,e->Id,e->Req,e->ReqCh));
    trcq (plci);
  }
  plci->XData[0].PLength = l;
  e->X = plci->XData;
  plci->adapter->request(e);
  dbug(1,dprintf("send_ok"));
}

void send_data(PLCI   * plci)
{
  DIVA_CAPI_ADAPTER   * a;
  DATA_B3_DESC   * data;
  NCCI   *ncci_ptr;
  word ncci;


  word i;
    byte dtmf_code_buffer[CAPIDTMF_RECV_DIGIT_BUFFER_SIZE + 1];



  if (!plci->nl_req && plci->ncci_ring_list)
  {
    a = plci->adapter;
    ncci = plci->ncci_ring_list;
    do
    {
      ncci = a->ncci_next[ncci];
      ncci_ptr = &(a->ncci[ncci]);
      if (!(a->ncci_ch[ncci]
         && (a->ch_flow_control[a->ncci_ch[ncci]] & N_OK_FC_PENDING)))
      {
        if (ncci_ptr->data_pending)
        {
          if ((a->ncci_state[ncci] == CONNECTED)
           || (a->ncci_state[ncci] == INC_ACT_PENDING)
           || (plci->send_disc == ncci))
          {
            data = &(ncci_ptr->DBuffer[ncci_ptr->data_out]);
            if ((plci->B2_prot == B2_V120_ASYNC)
             || (plci->B2_prot == B2_V120_ASYNC_V42BIS)
             || (plci->B2_prot == B2_V120_BIT_TRANSPARENT))
            {
              plci->NData[1].P = TransmitBufferGet (plci->appl, data->P);
              plci->NData[1].PLength = data->Length;
              if (data->Flags & 0x10)
                plci->NData[0].P = v120_break_header;
              else
                plci->NData[0].P = v120_default_header;
              plci->NData[0].PLength = 1 ;
              plci->NL.XNum = 2;
              plci->NL.Req = plci->nl_req = (byte)((data->Flags&0x07)<<4 |N_DATA);
            }
            else
            {
              plci->NData[0].P = TransmitBufferGet (plci->appl, data->P);
              plci->NData[0].PLength = data->Length;


              if (plci->capidtmf_ondata_state.recv.state != CAPIDTMF_RECV_STATE_IDLE)
              {
                capidtmf_recv_block (&(plci->capidtmf_ondata_state), plci->NData[0].P, plci->NData[0].PLength);
                i = capidtmf_indication (&(plci->capidtmf_ondata_state), dtmf_code_buffer + 1);
                if (i != 0)
                {
                  dtmf_code_buffer[0] = DTMF_UDATA_INDICATION_DIGITS_RECEIVED;
                  dtmf_ondata_indication (((word)plci->Id<<8)|a->Id, plci, dtmf_code_buffer, (word)(i + 1));
                }
              }


              if (data->Flags & 0x10)
                plci->NL.Req = plci->nl_req = (byte)N_UDATA;
              else
                plci->NL.Req = plci->nl_req = (byte)((data->Flags&0x07)<<4 |N_DATA);
            }
            plci->NL.X = plci->NData;
            plci->NL.ReqCh = a->ncci_ch[ncci];
            dbug(1,dprintf("%x:DREQ(%x:%x)",a->Id,plci->NL.Id,plci->NL.Req));
            plci->data_sent = TRUE;
            plci->data_sent_ptr = data->P;
            trcq (plci);
            a->request(&plci->NL);
          }
          else {
            cleanup_ncci_data (plci, ncci);
          }
        }
        else if ((plci->send_disc == ncci) && !plci->command)
        {
          //dprintf("N_DISC");
          if(plci->B3_prot==B3_T30_WITH_EXTENSIONS)
          {
            plci->NData[0].P = plci->fax_connect_info_buffer;
            plci->NData[0].PLength = plci->fax_connect_info_length;
            plci->NL.X = plci->NData;
          }
          else
          {
            plci->NData[0].PLength = 0;
          }
          plci->NL.ReqCh = a->ncci_ch[ncci];
          plci->NL.Req = plci->nl_req = N_DISC;
          trcq (plci);
          a->request(&plci->NL);
          plci->command = _DISCONNECT_B3_R;
          plci->send_disc = 0;
        }
      }
    } while (!plci->nl_req && (ncci != plci->ncci_ring_list));
    plci->ncci_ring_list = ncci;
  }
}

void listen_check(DIVA_CAPI_ADAPTER   * a)
{
  word i,j;
  PLCI   * plci;
  byte activnotifiedcalls = 0;

  dbug(1,dprintf("listen_check(%d,%d)",a->listen_active,a->max_listen));
  if (!remove_started && !a->adapter_disabled &&
      (a->vscapi_mode == 0 || check_adapter_appl_cip_mask_empty (a) == 0))
  {
    for(i=0;i<a->max_plci;i++)
    {
      plci = &(a->plci[i]);
      if(plci->notifiedcall) activnotifiedcalls++;
    }
    dbug(1,dprintf("listen_check(%d)",activnotifiedcalls));

    for(i=a->listen_active; i < ((word)(a->max_listen+activnotifiedcalls)); i++) {
      if((j=get_plci(a))) {
        a->listen_active++;
        plci = &a->plci[j-1];
        plci->State = LISTENING;
        add_p_listen (plci);
        plci->internal_command = LISTEN_SIG_ASSIGN_PEND;     /* do indicate_req if OK  */
        sig_req(plci,ASSIGN,DSIG_ID);
        send_req(plci);
      }
    }
  }
  /* NCRL establish a statical non call related link for such indications */
  if (!remove_started && !a->adapter_disabled && (a->vscapi_mode == 0)) /* only for non internal adapters */
  {
    if(a->ncrl_plci_id == 0)
    {
      dbug(1,dprintf("listen_check NCRL"));

      if((j=get_plci(a))) {
        a->ncrl_plci_id = (byte) j;
        plci = &a->plci[j-1];
        plci->State = LISTENING_NCRL;
        add_p_listen (plci);
        plci->internal_command = LISTEN_SIG_NCRL_ASSIGN_PEND;
        sig_req(plci,ASSIGN,DSIG_ID);
        send_req(plci);
      }
    }
  }
}

/*------------------------------------------------------------------*/
/* functions for all parameters sent in INDs                        */
/*------------------------------------------------------------------*/

void IndParse(PLCI   * plci, word * parms_id, byte   ** parms, byte multiIEsize)
{
  word ploc;            /* points to current location within packet */
  byte w;
  byte wlen;
  byte codeset,lock;
  byte   * in;
  word i;
  word code;
  word mIEindex = 0;
  ploc = 0;
  codeset = 0;
  lock = 0;

  in = plci->Sig.RBuffer->P;
  for(i=0; i<parms_id[0]; i++)   /* multiIE parms_id contains just the 1st */
  {                            /* element but parms array is larger      */
    parms[i] = (byte   *)"";
  }
  for(i=0; i<multiIEsize; i++)
  {
    parms[i] = (byte   *)"";
  }

  while(ploc<plci->Sig.RBuffer->length-1) {

        /* read information element id and length                   */
    w = in[ploc];

    if(w & 0x80) {
/*    w &=0xf0; removed, cannot detect congestion levels */
/*    upper 4 bit masked with w==SHIFT now               */
      wlen = 0;
    }
    else {
      wlen = (byte)(in[ploc+1]+1);
    }
        /* check if length valid (not exceeding end of packet)      */
    if((ploc+wlen) > 270) return ;
    if(lock & 0x80) lock &=0x7f;
    else codeset = lock;

    if((w&0xf0)==SHIFT) {
      codeset = in[ploc];
      if(!(codeset & 0x08)) lock = (byte)(codeset & 7);
      codeset &=7;
      lock |=0x80;
    }
    else {
      if(w==ESC && wlen>=3) code = in[ploc+2] |0x800;
      else code = w;
      code |= (codeset<<8);

      for(i=1; i<parms_id[0]+1 && parms_id[i]!=code; i++);

      if(i<parms_id[0]+1) {
        if(!multiIEsize) { /* with multiIEs use next field index,          */
          mIEindex = i-1;    /* with normal IEs use same index like parms_id */
        }

        /* Single Octet IEs */
        if(code==SDNCMPL ||
            code==CONG_RR ||
            code==CONG_RNR)
        {
            parms[mIEindex] = &in[ploc];
            dbug(1,dprintf("mIE[%d]=0x%x",1,in[ploc]));
        }
        else
        {
            parms[mIEindex] = &in[ploc+1];
            dbug(1,dprintf("mIE[%d]=0x%x",*parms[mIEindex],in[ploc]));
        }

        if(parms_id[i]==OAD
        || parms_id[i]==CONN_NR
        || parms_id[i]==CAD) {
          if(in[ploc+2] &0x80) {
            in[ploc+0] = (byte)(in[ploc+1]+1);
            in[ploc+1] = (byte)(in[ploc+2] &0x7f);
            in[ploc+2] = 0x80;
            parms[mIEindex] = &in[ploc];
          }
        }
        mIEindex++;       /* effects multiIEs only */
      }
    }

    ploc +=(wlen+1);
  }
  return ;
}

/*------------------------------------------------------------------*/
/* try to match a cip from received BC and HLC                      */
/*------------------------------------------------------------------*/

byte ie_compare(byte   * ie1, byte * ie2)
{
  word i;
  if(!ie1 || ! ie2) return FALSE;
  if(!ie1[0]) return FALSE;
  for(i=0;i<(word)(ie1[0]+1);i++) if(ie1[i]!=ie2[i]) return FALSE;
  return TRUE;
}

word find_cip(DIVA_CAPI_ADAPTER   * a, byte   * bc, byte   * hlc)
{
  word i;
  word j;

  for(i=9;i && !ie_compare(bc,cip_bc[i][a->u_law]);i--);

  for(j=16;j<29 &&
           (!ie_compare(bc,cip_bc[j][a->u_law]) || !ie_compare(hlc,cip_hlc[j])); j++);
  if(j==29) return i;
  return j;
}


static byte AddInfo(byte   **add_i,
                    byte   **fty_i,
                    byte   *esc_chi,
                    byte *facility)
{
  byte i;
  byte j;
  byte k;
  byte flen;
  byte len=0;
   /* facility is a nested structure */
   /* FTY can be more than once      */

  if(esc_chi[0] && !(esc_chi[esc_chi[0]]&0x7f))
  {
    add_i[0] = (byte   *)"\x02\x02\x00"; /* use neither b nor d channel */
  }

  else
  {
    add_i[0] = (byte   *)"";
  }
  if(!fty_i[0][0])
  {
    add_i[3] = (byte   *)"";
  }
  else
  {    /* facility array found  */
    for(i=0,j=1;i<MAX_MULTI_IE && fty_i[i][0];i++)
    {
      dbug(1,dprintf("AddIFac[%d]",fty_i[i][0]));
      len += fty_i[i][0];
      len += 2;
      flen=fty_i[i][0];
      facility[j++]=0x1c; /* copy fac IE */
      for(k=0;k<=flen;k++,j++)
      {
        facility[j]=fty_i[i][k];
//      dbug(1,dprintf("%x ",facility[j]));
      }
    }
    facility[0] = len;
    add_i[3] = facility;
  }
//  dbug(1,dprintf("FacArrLen=%d ",len));
  len = add_i[0][0]+add_i[1][0]+add_i[2][0]+add_i[3][0];
  len += 4;                          // calculate length of all
  return(len);
}

/*------------------------------------------------------------------*/
/* voice and codec features                                         */
/*------------------------------------------------------------------*/

void SetVoiceChannel(PLCI   *plci, byte   *chi, DIVA_CAPI_ADAPTER   * a)
{
  byte voice_chi[] = "\x02\x18\x01";
  byte channel;

  channel = chi[chi[0]]&0x3;
  dbug(1,dprintf("ExtDevON(Ch=0x%x)",channel));
  voice_chi[2] = (channel) ? channel : 1;
  add_p(plci,FTY,"\x02\x01\x07");             // B On, default on 1
  add_p(plci,ESC,voice_chi);                  // Channel
  sig_req(plci,TEL_CTRL,0);
  send_req(plci);
  if(a->AdvSignalPLCI)
  {
    adv_voice_write_coefs (a->AdvSignalPLCI, ADV_VOICE_WRITE_ACTIVATION);
  }
}

void VoiceChannelOff(PLCI   *plci)
{
  dbug(1,dprintf("ExtDevOFF"));
  add_p(plci,FTY,"\x02\x01\x08");             // B Off
  sig_req(plci,TEL_CTRL,0);
  send_req(plci);
  if(plci->adapter->AdvSignalPLCI)
  {
    adv_voice_clear_config (plci->adapter->AdvSignalPLCI);
  }
}


word AdvCodecSupport(DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, byte hook_listen)
{
  word j;
  PLCI   *splci;

  /* check if hardware supports handset with hook states (adv.codec) */
  /* or if just a on board codec is supported                        */
  /* the advanced codec plci is just for internal use                */

  /* diva Pro with on-board codec:                                   */
  if(a->profile.Global_Options & GL_HANDSET_SUPPORTED)
  {
    /* new call, but hook states are already signalled */
    if(a->AdvCodecFLAG)
    {
      if(a->AdvSignalAppl!=appl || a->AdvSignalPLCI)
      {
        dbug(1,dprintf("AdvSigPlci=0x%x",a->AdvSignalPLCI));
        return 0x2001; /* codec in use by another application */
      }
      if(plci!=0)
      {
        a->AdvSignalPLCI = plci;
        plci->tel=ADV_VOICE;
      }
      return 0;                      /* adv codec still used */
    }
    if((j=get_plci(a)))
    {
      splci = &a->plci[j-1];
      splci->tel = CODEC_PERMANENT;
      /* hook_listen indicates if a facility_req with handset/hook support */
      /* was sent. Otherwise if just a call on an external device was made */
      /* the codec will be used but the hook info will be discarded (just  */
      /* the external controller is in use                                 */
      if(hook_listen) splci->State = ADVANCED_VOICE_SIG;
      else
      {
        splci->State = ADVANCED_VOICE_NOSIG;
        if(plci)
        {
          plci->spoofed_msg = SPOOFING_REQUIRED;
        }
                                               /* indicate D-ch connect if  */
      }                                        /* codec is connected OK     */
      if(plci!=0)
      {
        a->AdvSignalPLCI = plci;
        plci->tel=ADV_VOICE;
      }
      a->AdvSignalAppl = appl;
      a->AdvCodecFLAG = TRUE;
      a->AdvCodecPLCI = splci;
      add_p(splci,CAI,"\x01\x15");
      add_p(splci,LLI,"\x01\x00");
      add_p(splci,ESC,"\x02\x18\x00");
      add_p(splci,UID,"\x06\x43\x61\x70\x69\x32\x30");
      splci->internal_command = PERM_COD_ASSIGN;
      dbug(1,dprintf("Codec Assign"));
      sig_req(splci,ASSIGN,DSIG_ID);
      send_req(splci);
    }
    else
    {
      return 0x2001; /* wrong state, no more plcis */
    }
  }
  else if(a->profile.Global_Options & GL_EXTERNAL_EQUIPMENT_SUPPORTED)
  {
    if(hook_listen) return 0x300B;               /* Facility not supported */
                                                 /* no hook with SCOM      */
    if(plci!=0) plci->tel = CODEC;
    dbug(1,dprintf("S/SCOM codec"));
    /* first time we use the scom-s codec we must shut down the internal   */
    /* handset application of the card. This can be done by an assign with */
    /* a cai with the 0x80 bit set. Assign return code is 'out of resource'*/
    if(!a->scom_appl_disable){
      if((j=get_plci(a))) {
        splci = &a->plci[j-1];
        add_p(splci,CAI,"\x01\x80");
        add_p(splci,UID,"\x06\x43\x61\x70\x69\x32\x30");
        sig_req(splci,ASSIGN,0xC0);  /* 0xc0 is the TEL_ID */
        send_req(splci);
        a->scom_appl_disable = TRUE;
      }
      else{
        return 0x2001; /* wrong state, no more plcis */
      }
    }
  }
  else return 0x300B;               /* Facility not supported */

  return 0;
}


void CodecIdCheck(DIVA_CAPI_ADAPTER   *a, PLCI   *plci)
{

  dbug(1,dprintf("CodecIdCheck"));

  if(a->AdvSignalPLCI == plci)
  {
    dbug(1,dprintf("PLCI owns codec"));
    VoiceChannelOff(a->AdvCodecPLCI);
    if(a->AdvCodecPLCI->State == ADVANCED_VOICE_NOSIG)
    {
      dbug(1,dprintf("remove temp codec PLCI"));
      plci_remove(a->AdvCodecPLCI);
      a->AdvCodecFLAG  = 0;
      a->AdvCodecPLCI  = 0;
      a->AdvSignalAppl = 0;
    }
    a->AdvSignalPLCI = 0;
  }
}

/* -------------------------------------------------------------------
    Ask for physical address of card on PCI bus
   ------------------------------------------------------------------- */
static void diva_ask_for_xdi_sdram_bar (DIVA_CAPI_ADAPTER  * a,
                                        IDI_SYNC_REQ  * preq) {
  a->sdram_bar = 0;
  if (diva_xdi_extended_features & DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR) {
    ENTITY   * e = (ENTITY   *)preq;

    e->user[0] = a->Id - 1;
    preq->xdi_sdram_bar.info.bar    = 0;
    preq->xdi_sdram_bar.Req         = 0;
    preq->xdi_sdram_bar.Rc           = IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR;

    (*(a->request))(e);

    a->sdram_bar = preq->xdi_sdram_bar.info.bar;
    dbug(3,dprintf("A(%d) SDRAM BAR = %08lx", a->Id, (unsigned long)(a->sdram_bar)));
  }
}

/* -------------------------------------------------------------------
     Ask XDI about extended features
   ------------------------------------------------------------------- */
static void diva_get_extended_adapter_features (DIVA_CAPI_ADAPTER  * a) {
  IDI_SYNC_REQ   * preq;
    char buffer[              ((sizeof(preq->xdi_extended_features)+4) > sizeof(ENTITY)) ?                     (sizeof(preq->xdi_extended_features)+4) : sizeof(ENTITY)];

    char features[4];
  preq = (IDI_SYNC_REQ   *)&buffer[0];

  if (!diva_xdi_extended_features) {
    ENTITY   * e = (ENTITY   *)preq;
    diva_xdi_extended_features |= 0x80000000;

    e->user[0] = a->Id - 1;
    preq->xdi_extended_features.Req = 0;
    preq->xdi_extended_features.Rc  = IDI_SYNC_REQ_XDI_GET_EXTENDED_FEATURES;
    preq->xdi_extended_features.info.buffer_length_in_bytes = sizeof(features);
    preq->xdi_extended_features.info.features = &features[0];

    (*(a->request))(e);

    if (features[0] & DIVA_XDI_EXTENDED_FEATURES_VALID) {
      /*
         Check features located in the byte '0'
         */
      if (features[0] & DIVA_XDI_EXTENDED_FEATURE_CMA) {
        diva_xdi_extended_features |= DIVA_CAPI_USE_CMA;
      }
      if (features[0] & DIVA_XDI_EXTENDED_FEATURE_RX_DMA) {
        diva_xdi_extended_features |= DIVA_CAPI_XDI_PROVIDES_RX_DMA;
        dbug(1,dprintf("XDI provides RxDMA"));
      }
      if (features[0] & DIVA_XDI_EXTENDED_FEATURE_SDRAM_BAR) {
        diva_xdi_extended_features |= DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR;
      }
      if (features[0] & DIVA_XDI_EXTENDED_FEATURE_NO_CANCEL_RC) {
        diva_xdi_extended_features |= DIVA_CAPI_XDI_PROVIDES_NO_CANCEL;
        dbug(3,dprintf("XDI provides NO_CANCEL_RC feature"));
      }

    }
  }

  diva_ask_for_xdi_sdram_bar (a, preq);
}

static void diva_get_mtpx_adapter_interface_version (DIVA_CAPI_ADAPTER* a) {
  IDI_SYNC_REQ* preq;
  char buffer[((sizeof(preq->GetCardType)+4) > sizeof(ENTITY)) ? (sizeof(preq->GetCardType)+4) : sizeof(ENTITY)];
  ENTITY* e;

  preq       = (IDI_SYNC_REQ*)&buffer[0];
  e          = (ENTITY*)preq;
  e->user[0] = a->Id - 1;

  preq->GetCardType.Req = 0;
  preq->GetCardType.Rc  = IDI_SYNC_REQ_GET_CARDTYPE;
  preq->GetCardType.cardtype = 0;

  (*(a->request))(e);

  a->mtpx_adapter = (preq->GetCardType.cardtype == CARDTYPE_DIVASRV_MADAPTER_VIRTUAL);
}

/*------------------------------------------------------------------*/
/* automatic law                                                    */
/*------------------------------------------------------------------*/
/* called from OS specific part after init time to get the Law              */
/* a-law (Euro) and u-law (us,japan) use different BCs in the Setup message */
void AutomaticLaw(DIVA_CAPI_ADAPTER   *a)
{
  word j;
  PLCI   *splci;

  if(a->automatic_law) {
    return;
  }
  if((j=get_plci(a))) {
    diva_get_extended_adapter_features (a);
    diva_get_mtpx_adapter_interface_version (a);
    splci = &a->plci[j-1];
    a->automatic_lawPLCI = splci;
    a->automatic_law = 1;
    add_p(splci,CAI,"\x01\x80");
    add_p(splci,UID,"\x06\x43\x61\x70\x69\x32\x30");
    splci->internal_command = USELAW_REQ;
    splci->command = 0;
    splci->number = 0;
    sig_req(splci,ASSIGN,DSIG_ID);
    send_req(splci);
  }
}

/* called from OS specific part if an application sends an Capi20Release */
word CapiRelease(word Id)
{
  word i, j;
  PLCI   *plci;
  APPL   *this;
  DIVA_CAPI_ADAPTER   *a;
  int cleanup_all_plci;
  if (!Id)
  {
    dbug(0,dprintf("A: CapiRelease(Id==0)"));
    return (_WRONG_APPL_ID);
  }

  this = &application[Id-1];               /* get application pointer */

  for(i=0; i<max_adapter; i++)             /* scan all adapters...    */
  {
    a = &adapter[i];
    if (a->request)
    {

      if(a->adapter_disabled == 0 &&
          (a->manufacturer_features2 & MANUFACTURER_FEATURE2_STREAMING_IDI) != 0 &&
          (this->appl_flags & APPL_FLAG_USES_STREAMING) != 0 &&
          a->ncrl_plci_id != 0)
      { /* Release all used by this application streaming resources */
        PLCI   * v_plci = &a->plci[a->ncrl_plci_id-1];
        byte fty_t[] = { 0, DSP_CTRL_STREAM_COMMAND /* List of info elements */,
                          KEY, 8, (byte)Id, (byte)(Id >> 8), 0, 0, 0, 0, 0, 0,
                         0 /* end of list */ };

        fty_t[0] = (byte)(sizeof(fty_t)-1);
        add_p(v_plci,FTY,fty_t);
        sig_req(v_plci,HW_CTRL,0);
        send_req(v_plci);
      }


      a->Info_Mask[Id-1] = 0;
      a->CIP_Mask[Id-1] = 0;
      a->Notification_Mask[Id-1] = 0;
      a->codec_listen[Id-1] = 0;
      cleanup_all_plci = (a->vscapi_mode != 0 && check_adapter_appl_cip_mask_empty (a) != 0);

      for(j=0; j<a->max_plci; j++)           /* and all PLCIs connected */
      {                                      /* with this application   */
        plci = &a->plci[j];
        if(plci->Id)                         /* if plci owns no application */
        {                                    /* it may be not jet connected */
          if (plci->ptyAppl == this)
            plci->ptyAppl = NULL;
          if(plci->State==INC_CON_PENDING
          || plci->State==INC_CON_ALERT)
          {
            if(test_c_ind_mask_bit (plci, (word)(Id-1)))
            {
              ignore_incoming_call (plci, this);
              if(c_ind_mask_empty (plci))
              {
                sig_req(plci,HANGUP,0);
                send_req(plci);
                plci->State = OUTG_DIS_PENDING;
              }
            }
          }
          if(plci->appl!=this)
            plci_free_queued_appl_msg (plci, this);
          if(test_c_ind_mask_bit (plci, (word)(Id-1)))
          {
            clear_c_ind_mask_bit (plci, (word)(Id-1));
            if(c_ind_mask_empty (plci))
            {
              if(!plci->appl)
              {
                plci_remove(plci);
                plci = 0;
              }
            }
          }
          if (plci != 0 && cleanup_all_plci && plci->appl == 0 && plci->State == LISTENING) {
            plci_remove(plci);
            plci = 0;
          }
          if(plci && (plci->appl==this))
          {
            CodecIdCheck (a, plci);
            plci->appl = 0;
            plci_remove(plci);
          }
        }
      }
      listen_check(a);

      diva_capi_check_l1_shutdown (Id, a);
      a->requested_options_table[Id-1] = 0;

      if(a->AdvSignalAppl==this)
      {
        this->NullCREnable = FALSE;
        if (a->AdvCodecPLCI)
        {
          plci_remove(a->AdvCodecPLCI);
          a->AdvCodecPLCI->tel = 0;
          a->AdvCodecPLCI->adv_nl = 0;
        }
        a->AdvSignalAppl = 0;
        a->AdvSignalPLCI = 0;
        a->AdvCodecFLAG = 0;
        a->AdvCodecPLCI = 0;
      }
    }
  }


  {
    diva_entity_link_t* link;

    while ((link = diva_q_get_head (&this->saved_action_q)) != 0) {
      diva_q_remove (&this->saved_action_q, link);
      diva_os_free (DIVA_OS_MALLOC_ATOMIC, link);
    }
  }


  this->Id = 0;


  if (li_update_wait_list != NULL)
    mixer_update_done ();

  return GOOD;
}

static int check_adapter_appl_cip_mask_empty (const DIVA_CAPI_ADAPTER* a) {
  byte i;

  for (i = 0; i < max_appl; i++) {
    if (application[i].Id != 0 && a->CIP_Mask[i] != 0) {
      return (0);
    }
  }

  return (1);
}

static void free_plci (PLCI   *plci)
{
  DIVA_CAPI_ADAPTER   *a;

  word i;


  a = plci->adapter;
  clear_b1_config (plci);
  if (plci->appl)
    ncci_free_receive_buffers (plci->appl, plci, 0);
  if (plci->Id)
    ncci_remove (plci, 0, FALSE);
  plci_free_msg_in_queue (plci);
  channel_flow_control_remove (plci);

  for (i = 0; i < a->null_plci_base + a->null_plci_channels; i++)
  {
    if (a->null_plci_table[i] == plci->Id)
      a->null_plci_table[i] = 0;
  }

  if (plci->Id == a->ncrl_plci_id)
    a->ncrl_plci_id = 0;
  if (a->first_free_plci_id != 0)
    a->plci[a->last_free_plci_id-1].NextId = plci->Id;
  else
    a->first_free_plci_id = plci->Id;
  a->last_free_plci_id = plci->Id;
  if ((plci->hangup_flow_ctrl_timer != 0) && (plci->hangup_flow_ctrl_timer <= 3))
    (a->hangup_flow_control_plcis)--;
  plci->hangup_flow_ctrl_timer = 0;
  plci->Id = 0;
  plci->State = IDLE;
  plci->channels = 0;
  plci->appl = 0;
  plci->notifiedcall = 0;
  plci->stream_data[0] = 0;
}

static word plci_remove_check(PLCI   *plci)
{

  if(!plci) return TRUE;
  if(!plci->NL.Id && c_ind_mask_empty (plci))
  {
    if(plci->Sig.Id == 0xff)
      plci->Sig.Id = 0;
    if(!plci->Sig.Id)
    {
      int listen_check_state = plci->State == LISTENING && plci->Sig.Id == 0 && plci->Sig.Req == ASSIGN;

      dbug(1,dprintf("plci_remove_complete(%x)",plci->Id));
      dbug(1,dprintf("tel=0x%x,Sig=0x%x",plci->tel,plci->Sig.Id));
      if (plci->Id)
      {
        clear_incoming_call (plci, FALSE, 0);
        CodecIdCheck(plci->adapter, plci);
        free_plci (plci);
        trcq (plci);
      }
      if (listen_check_state == 0) {
        listen_check(plci->adapter);
      } else {
        DBG_ERR(("skip listen(%d,%d), assign failed",
          plci->adapter->listen_active, plci->adapter->max_listen))
      }
      return TRUE;
    }
  }
  return FALSE;
}


/*------------------------------------------------------------------*/

static byte plci_nl_busy (PLCI   *plci)
{
  /* only applicable for non-multiplexed protocols */
  return (plci->nl_req
    || (plci->ncci_ring_list
     && plci->adapter->ncci_ch[plci->ncci_ring_list]
     && (plci->adapter->ch_flow_control[plci->adapter->ncci_ch[plci->ncci_ring_list]] & N_OK_FC_PENDING)));
}


/*------------------------------------------------------------------*/
/* UDATA message transfer via MANUFACTURER_REQ                      */
/*------------------------------------------------------------------*/

static void control_message_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: control_message_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
  case CONTROL_MESSAGE_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = CONTROL_MESSAGE_COMMAND_1;
      return;
    }
    for (i = 0; i < plci->saved_msg.parms[2].length; i++)
      plci->internal_req_buffer[i] = plci->saved_msg.parms[2].info[1+i];
    plci->NData[0].PLength = plci->saved_msg.parms[2].length;
    plci->NData[0].P = plci->internal_req_buffer;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_UDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    plci->internal_command = CONTROL_MESSAGE_COMMAND_2;
    return;
  case CONTROL_MESSAGE_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Control command failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    break;
  }
  sendf (plci->appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, plci->number,
    "dww", _DI_MANU_ID, READ_WORD (plci->saved_msg.parms[1].info), Info);
}


static byte control_message_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: control_message_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING)
   || !plci->NL.Id || plci->nl_remove_id)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_STATE;
  }
  else if (msg[2].length > INTERNAL_REQ_BUFFER_SIZE)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Control request too long",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _DATA_LEN_NOT_SUPPORTED;
  }
  else
  {
    plci->command = 0;
    api_save_msg (msg, "dws", &plci->saved_msg);
    start_internal_command (Id, plci, control_message_command);
    return (FALSE);
  }
  sendf (appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, Number,
    "dww", _DI_MANU_ID, READ_WORD (msg[1].info), Info);
  return (FALSE);
}


static void control_message_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{
  static byte result[MAX_MSG_SIZE];
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: control_message_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if ((length >= MAX_MSG_SIZE) || (length > 0xff))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Control indication too long %d",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, length));
  }
  else
  {
    result[0] = (byte) length;
    for (i = 0; i < length; i++)
      result[i+1] = msg[i];
    sendf (plci->appl, _MANUFACTURER_I, Id & 0xffff, 0,
      "dws", _DI_MANU_ID, _DI_CONTROL_MESSAGE, result);
  }
}


/*------------------------------------------------------------------*/
/* DTMF facilities                                                  */
/*------------------------------------------------------------------*/


static struct
{
  word send_mask;
  word listen_mask;
  byte character;
  byte code;
} dtmf_digit_map[] =
{
  { 0x11, 0x1b, 0x23, DTMF_DIGIT_TONE_CODE_HASHMARK },
  { 0x11, 0x1b, 0x2a, DTMF_DIGIT_TONE_CODE_STAR },
  { 0x11, 0x1b, 0x30, DTMF_DIGIT_TONE_CODE_0 },
  { 0x11, 0x1b, 0x31, DTMF_DIGIT_TONE_CODE_1 },
  { 0x11, 0x1b, 0x32, DTMF_DIGIT_TONE_CODE_2 },
  { 0x11, 0x1b, 0x33, DTMF_DIGIT_TONE_CODE_3 },
  { 0x11, 0x1b, 0x34, DTMF_DIGIT_TONE_CODE_4 },
  { 0x11, 0x1b, 0x35, DTMF_DIGIT_TONE_CODE_5 },
  { 0x11, 0x1b, 0x36, DTMF_DIGIT_TONE_CODE_6 },
  { 0x11, 0x1b, 0x37, DTMF_DIGIT_TONE_CODE_7 },
  { 0x11, 0x1b, 0x38, DTMF_DIGIT_TONE_CODE_8 },
  { 0x11, 0x1b, 0x39, DTMF_DIGIT_TONE_CODE_9 },
  { 0x11, 0x1b, 0x41, DTMF_DIGIT_TONE_CODE_A },
  { 0x11, 0x1b, 0x42, DTMF_DIGIT_TONE_CODE_B },
  { 0x11, 0x1b, 0x43, DTMF_DIGIT_TONE_CODE_C },
  { 0x11, 0x1b, 0x44, DTMF_DIGIT_TONE_CODE_D },
  { 0x11, 0x00, 0x61, DTMF_DIGIT_TONE_CODE_A },
  { 0x11, 0x00, 0x62, DTMF_DIGIT_TONE_CODE_B },
  { 0x11, 0x00, 0x63, DTMF_DIGIT_TONE_CODE_C },
  { 0x11, 0x00, 0x64, DTMF_DIGIT_TONE_CODE_D },
  { 0x00, 0x0a, 0x7e, DTMF_DIGIT_TONE_CODE_NONE },

  { 0x20, 0x20, 0x80, DTMF_SIGNAL_NO_TONE },
  { 0x00, 0x20, 0x81, DTMF_SIGNAL_UNIDENTIFIED_TONE },
  { 0x20, 0x20, 0x82, DTMF_SIGNAL_DIAL_TONE },
  { 0x20, 0x20, 0x83, DTMF_SIGNAL_PABX_INTERNAL_DIAL_TONE },
  { 0x20, 0x20, 0x84, DTMF_SIGNAL_SPECIAL_DIAL_TONE },
  { 0x20, 0x20, 0x85, DTMF_SIGNAL_SECOND_DIAL_TONE },
  { 0x20, 0x20, 0x86, DTMF_SIGNAL_RINGING_TONE },
  { 0x20, 0x20, 0x87, DTMF_SIGNAL_SPECIAL_RINGING_TONE },
  { 0x20, 0x20, 0x88, DTMF_SIGNAL_BUSY_TONE },
  { 0x20, 0x20, 0x89, DTMF_SIGNAL_CONGESTION_TONE },
  { 0x20, 0x20, 0x8a, DTMF_SIGNAL_SPECIAL_INFORMATION_TONE },
  { 0x20, 0x20, 0x8b, DTMF_SIGNAL_COMFORT_TONE },
  { 0x20, 0x20, 0x8c, DTMF_SIGNAL_HOLD_TONE },
  { 0x20, 0x20, 0x8d, DTMF_SIGNAL_RECORD_TONE },
  { 0x20, 0x20, 0x8e, DTMF_SIGNAL_CALLER_WAITING_TONE },
  { 0x20, 0x20, 0x8f, DTMF_SIGNAL_CALL_WAITING_TONE },
  { 0x20, 0x20, 0x90, DTMF_SIGNAL_PAY_TONE },
  { 0x20, 0x20, 0x91, DTMF_SIGNAL_POSITIVE_INDICATION_TONE },
  { 0x20, 0x20, 0x92, DTMF_SIGNAL_NEGATIVE_INDICATION_TONE },
  { 0x20, 0x20, 0x93, DTMF_SIGNAL_WARNING_TONE },
  { 0x20, 0x20, 0x94, DTMF_SIGNAL_INTRUSION_TONE },
  { 0x20, 0x20, 0x95, DTMF_SIGNAL_CALLING_CARD_SERVICE_TONE },
  { 0x20, 0x20, 0x96, DTMF_SIGNAL_PAYPHONE_RECOGNITION_TONE },
  { 0x20, 0x20, 0x97, DTMF_SIGNAL_CPE_ALERTING_SIGNAL },
  { 0x20, 0x20, 0x98, DTMF_SIGNAL_OFF_HOOK_WARNING_TONE },
  { 0x20, 0x20, 0xa0, DTMF_SIGNAL_SIT_RESERVED_0 },
  { 0x20, 0x20, 0xa1, DTMF_SIGNAL_SIT_RESERVED_1 },
  { 0x20, 0x20, 0xa2, DTMF_SIGNAL_SIT_RESERVED_2 },
  { 0x20, 0x20, 0xa3, DTMF_SIGNAL_SIT_RESERVED_3 },
  { 0x20, 0x20, 0xa4, DTMF_SIGNAL_SIT_OPERATOR_INTERCEPT },
  { 0x20, 0x20, 0xa5, DTMF_SIGNAL_SIT_VACANT_CIRCUIT },
  { 0x20, 0x20, 0xa6, DTMF_SIGNAL_SIT_REORDER },
  { 0x20, 0x20, 0xa7, DTMF_SIGNAL_SIT_NO_CIRCUIT_FOUND },
  { 0x20, 0x20, 0xab, DTMF_SIGNAL_SS5_F1 },
  { 0x20, 0x20, 0xac, DTMF_SIGNAL_SS5_F2 },
  { 0x20, 0x20, 0xad, DTMF_SIGNAL_SS5_F1_F2 },
  { 0x20, 0x00, 0xae, DTMF_SIGNAL_HOLD_MUSIC },
  { 0x20, 0x20, 0xaf, DTMF_SIGNAL_INTERCEPT_TONE },
  {0x200,0x200, 0xb0, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_NONE },
  {0x200,0x200, 0xb1, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_1 },
  {0x200,0x200, 0xb2, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_2 },
  {0x200,0x200, 0xb3, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_3 },
  {0x200,0x200, 0xb4, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_4 },
  {0x200,0x200, 0xb5, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_5 },
  {0x200,0x200, 0xb6, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_6 },
  {0x200,0x200, 0xb7, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_7 },
  {0x200,0x200, 0xb8, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_8 },
  {0x200,0x200, 0xb9, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_9 },
  {0x200,0x200, 0xba, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_10 },
  {0x200,0x200, 0xbb, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_11 },
  {0x200,0x200, 0xbc, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_12 },
  {0x200,0x200, 0xbd, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_13 },
  {0x200,0x200, 0xbe, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_14 },
  {0x200,0x200, 0xbf, DTMF_R2_BACKWARD_DIGIT_TONE_CODE_15 },
  { 0x20, 0x20, 0xc0, DTMF_SIGNAL_MODEM_CALLING_TONE },
  { 0x20, 0x20, 0xc1, DTMF_SIGNAL_FAX_CALLING_TONE },
  { 0x20, 0x20, 0xc2, DTMF_SIGNAL_ANSWER_TONE },
  { 0x20, 0x20, 0xc3, DTMF_SIGNAL_REVERSED_ANSWER_TONE },
  { 0x20, 0x20, 0xc4, DTMF_SIGNAL_ANSAM_TONE },
  { 0x20, 0x20, 0xc5, DTMF_SIGNAL_REVERSED_ANSAM_TONE },
  { 0x20, 0x20, 0xc6, DTMF_SIGNAL_BELL103_ANSWER_TONE },
  { 0x20, 0x20, 0xc7, DTMF_SIGNAL_FAX_FLAGS },
  { 0x20, 0x20, 0xc8, DTMF_SIGNAL_G2_FAX_GROUP_ID },
  { 0x00, 0x20, 0xc9, DTMF_SIGNAL_HUMAN_SPEECH },
  { 0x20, 0x20, 0xca, DTMF_SIGNAL_ANSWERING_MACHINE_390 },
  { 0x20, 0x20, 0xcb, DTMF_SIGNAL_TONE_ALERTING_SIGNAL },
  {0x100,0x100, 0xd0, DTMF_R2_FORWARD_DIGIT_TONE_CODE_NONE },
  {0x100,0x100, 0xd1, DTMF_R2_FORWARD_DIGIT_TONE_CODE_1 },
  {0x100,0x100, 0xd2, DTMF_R2_FORWARD_DIGIT_TONE_CODE_2 },
  {0x100,0x100, 0xd3, DTMF_R2_FORWARD_DIGIT_TONE_CODE_3 },
  {0x100,0x100, 0xd4, DTMF_R2_FORWARD_DIGIT_TONE_CODE_4 },
  {0x100,0x100, 0xd5, DTMF_R2_FORWARD_DIGIT_TONE_CODE_5 },
  {0x100,0x100, 0xd6, DTMF_R2_FORWARD_DIGIT_TONE_CODE_6 },
  {0x100,0x100, 0xd7, DTMF_R2_FORWARD_DIGIT_TONE_CODE_7 },
  {0x100,0x100, 0xd8, DTMF_R2_FORWARD_DIGIT_TONE_CODE_8 },
  {0x100,0x100, 0xd9, DTMF_R2_FORWARD_DIGIT_TONE_CODE_9 },
  {0x100,0x100, 0xda, DTMF_R2_FORWARD_DIGIT_TONE_CODE_10 },
  {0x100,0x100, 0xdb, DTMF_R2_FORWARD_DIGIT_TONE_CODE_11 },
  {0x100,0x100, 0xdc, DTMF_R2_FORWARD_DIGIT_TONE_CODE_12 },
  {0x100,0x100, 0xdd, DTMF_R2_FORWARD_DIGIT_TONE_CODE_13 },
  {0x100,0x100, 0xde, DTMF_R2_FORWARD_DIGIT_TONE_CODE_14 },
  {0x100,0x100, 0xdf, DTMF_R2_FORWARD_DIGIT_TONE_CODE_15 },
  { 0x00, 0x40, 0xe0, DTMF_DIAL_PULSE_DIGIT_CODE_1 },
  { 0x00, 0x40, 0xe1, DTMF_DIAL_PULSE_DIGIT_CODE_2 },
  { 0x00, 0x40, 0xe2, DTMF_DIAL_PULSE_DIGIT_CODE_3 },
  { 0x00, 0x40, 0xe3, DTMF_DIAL_PULSE_DIGIT_CODE_4 },
  { 0x00, 0x40, 0xe4, DTMF_DIAL_PULSE_DIGIT_CODE_5 },
  { 0x00, 0x40, 0xe5, DTMF_DIAL_PULSE_DIGIT_CODE_6 },
  { 0x00, 0x40, 0xe6, DTMF_DIAL_PULSE_DIGIT_CODE_7 },
  { 0x00, 0x40, 0xe7, DTMF_DIAL_PULSE_DIGIT_CODE_8 },
  { 0x00, 0x40, 0xe8, DTMF_DIAL_PULSE_DIGIT_CODE_9 },
  { 0x00, 0x40, 0xe9, DTMF_DIAL_PULSE_DIGIT_CODE_10 },
  { 0x00, 0x40, 0xea, DTMF_DIAL_PULSE_DIGIT_CODE_11 },
  { 0x00, 0x40, 0xeb, DTMF_DIAL_PULSE_DIGIT_CODE_12 },
  { 0x00, 0x40, 0xec, DTMF_DIAL_PULSE_DIGIT_CODE_13 },
  { 0x00, 0x40, 0xed, DTMF_DIAL_PULSE_DIGIT_CODE_14 },
  { 0x00, 0x40, 0xee, DTMF_DIAL_PULSE_DIGIT_CODE_15 },
  { 0x00, 0x40, 0xef, DTMF_DIAL_PULSE_DIGIT_CODE_16 },
  { 0x80, 0x80, 0xf0, DTMF_MF_DIGIT_TONE_CODE_NONE },
  { 0x80, 0x80, 0xf1, DTMF_MF_DIGIT_TONE_CODE_1 },
  { 0x80, 0x80, 0xf2, DTMF_MF_DIGIT_TONE_CODE_2 },
  { 0x80, 0x80, 0xf3, DTMF_MF_DIGIT_TONE_CODE_3 },
  { 0x80, 0x80, 0xf4, DTMF_MF_DIGIT_TONE_CODE_4 },
  { 0x80, 0x80, 0xf5, DTMF_MF_DIGIT_TONE_CODE_5 },
  { 0x80, 0x80, 0xf6, DTMF_MF_DIGIT_TONE_CODE_6 },
  { 0x80, 0x80, 0xf7, DTMF_MF_DIGIT_TONE_CODE_7 },
  { 0x80, 0x80, 0xf8, DTMF_MF_DIGIT_TONE_CODE_8 },
  { 0x80, 0x80, 0xf9, DTMF_MF_DIGIT_TONE_CODE_9 },
  { 0x80, 0x80, 0xfa, DTMF_MF_DIGIT_TONE_CODE_0 },
  { 0x80, 0x80, 0xfb, DTMF_MF_DIGIT_TONE_CODE_K1 },
  { 0x80, 0x80, 0xfc, DTMF_MF_DIGIT_TONE_CODE_K2 },
  { 0x80, 0x80, 0xfd, DTMF_MF_DIGIT_TONE_CODE_KP },
  { 0x80, 0x80, 0xfe, DTMF_MF_DIGIT_TONE_CODE_S1 },
  { 0x80, 0x80, 0xff, DTMF_MF_DIGIT_TONE_CODE_ST },
  { 0x04, 0x04, 0x10, DTMF_SIGNAL_HOOK_DIGIT_1 },
  { 0x04, 0x04, 0x11, DTMF_SIGNAL_HOOK_DIGIT_2 },
  { 0x04, 0x04, 0x12, DTMF_SIGNAL_HOOK_DIGIT_3 },
  { 0x04, 0x04, 0x13, DTMF_SIGNAL_HOOK_DIGIT_4 },
  { 0x04, 0x04, 0x14, DTMF_SIGNAL_HOOK_DIGIT_5 },
  { 0x04, 0x04, 0x15, DTMF_SIGNAL_HOOK_DIGIT_6 },
  { 0x04, 0x04, 0x16, DTMF_SIGNAL_HOOK_DIGIT_7 },
  { 0x04, 0x04, 0x17, DTMF_SIGNAL_HOOK_DIGIT_8 },
  { 0x04, 0x04, 0x18, DTMF_SIGNAL_HOOK_DIGIT_9 },
  { 0x04, 0x04, 0x19, DTMF_SIGNAL_HOOK_DIGIT_10 },
  { 0x04, 0x04, 0x1a, DTMF_SIGNAL_HOOK_RESERVED_11 },
  { 0x04, 0x04, 0x1b, DTMF_SIGNAL_HOOK_RESERVED_12 },
  { 0x04, 0x04, 0x1c, DTMF_SIGNAL_HOOK_FLASH },
  { 0x04, 0x04, 0x1d, DTMF_SIGNAL_HOOK_RING },
  { 0x04, 0x04, 0x1e, DTMF_SIGNAL_HOOK_STATE_OFF_HOOK },
  { 0x04, 0x04, 0x1f, DTMF_SIGNAL_HOOK_STATE_ON_HOOK },

  { 0x00, 0x1b, 0x23, DTMF_DIAL_PULSE_DIGIT_CODE_12 },
  { 0x00, 0x1b, 0x2a, DTMF_DIAL_PULSE_DIGIT_CODE_11 },
  { 0x00, 0x1b, 0x30, DTMF_DIAL_PULSE_DIGIT_CODE_10 },
  { 0x00, 0x1b, 0x31, DTMF_DIAL_PULSE_DIGIT_CODE_1 },
  { 0x00, 0x1b, 0x32, DTMF_DIAL_PULSE_DIGIT_CODE_2 },
  { 0x00, 0x1b, 0x33, DTMF_DIAL_PULSE_DIGIT_CODE_3 },
  { 0x00, 0x1b, 0x34, DTMF_DIAL_PULSE_DIGIT_CODE_4 },
  { 0x00, 0x1b, 0x35, DTMF_DIAL_PULSE_DIGIT_CODE_5 },
  { 0x00, 0x1b, 0x36, DTMF_DIAL_PULSE_DIGIT_CODE_6 },
  { 0x00, 0x1b, 0x37, DTMF_DIAL_PULSE_DIGIT_CODE_7 },
  { 0x00, 0x1b, 0x38, DTMF_DIAL_PULSE_DIGIT_CODE_8 },
  { 0x00, 0x1b, 0x39, DTMF_DIAL_PULSE_DIGIT_CODE_9 },
  { 0x00, 0x1b, 0x41, DTMF_DIAL_PULSE_DIGIT_CODE_13 },
  { 0x00, 0x1b, 0x42, DTMF_DIAL_PULSE_DIGIT_CODE_14 },
  { 0x00, 0x1b, 0x43, DTMF_DIAL_PULSE_DIGIT_CODE_15 },
  { 0x00, 0x1b, 0x44, DTMF_DIAL_PULSE_DIGIT_CODE_16 },
};

#define DTMF_DIGIT_MAP_ENTRIES (sizeof(dtmf_digit_map) / sizeof(dtmf_digit_map[0]))


static void dtmf_enable_receiver (PLCI   *plci, word enable_mask)
{
  word min_digit_duration, min_gap_duration, w;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_enable_receiver %02x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, enable_mask));

  if (enable_mask != 0)
  {
    min_digit_duration = (plci->dtmf_rec_pulse_ms == 0) ? 40 : plci->dtmf_rec_pulse_ms;
    min_gap_duration = (plci->dtmf_rec_pause_ms == 0) ? 40 : plci->dtmf_rec_pause_ms;
    plci->internal_req_buffer[0] = DTMF_UDATA_REQUEST_ENABLE_RECEIVER;
    WRITE_WORD (&plci->internal_req_buffer[1], min_digit_duration);
    WRITE_WORD (&plci->internal_req_buffer[3], min_gap_duration);
    plci->NData[0].PLength = 5;
    WRITE_WORD (&plci->internal_req_buffer[plci->NData[0].PLength], INTERNAL_IND_BUFFER_SIZE);
    plci->NData[0].PLength += 2;

    capidtmf_recv_enable (&(plci->capidtmf_state), min_digit_duration, min_gap_duration,
      plci->dtmf_pending_dtmf_digit_code);

    w = 0;
    if (enable_mask & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                       DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG | DTMF_TONE_LISTEN_ACTIVE_FLAG))
    {
      w |= DSP_ENABLE_DTMF_DETECTION;
    }
    if (enable_mask & (DTMF_EDGE_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG))
      w |= DSP_ENABLE_DTMF_FRAME_FLUSHING;
    if (enable_mask & DTMF_MF_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_MF_DETECTION;
    if (enable_mask & DTMF_PULSE_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_DIAL_PULSE_DETECTION;
    if (enable_mask & DTMF_TONE_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_EXTENDED_TONE_DETECTION;
    if (enable_mask & DTMF_SUPPRESSION_ACTIVE_FLAG)
      w |= DSP_ENABLE_DTMF_SUPPRESSION;
    if (enable_mask & DTMF_HOOK_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_HOOK_DETECTION;
    if (enable_mask & DTMF_R2FWD_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_R2FWD_DETECTION;
    if (enable_mask & DTMF_R2BWD_LISTEN_ACTIVE_FLAG)
      w |= DSP_ENABLE_R2BWD_DETECTION;
    WRITE_WORD (&plci->internal_req_buffer[plci->NData[0].PLength], w);
    plci->NData[0].PLength += 2;
    plci->internal_req_buffer[(plci->NData[0].PLength)++] = plci->dtmf_pending_dtmf_digit_code;
    plci->internal_req_buffer[(plci->NData[0].PLength)++] = plci->dtmf_pending_mf_digit_code;
    plci->internal_req_buffer[(plci->NData[0].PLength)++] = plci->dtmf_pending_r2fwd_digit_code;
    plci->internal_req_buffer[(plci->NData[0].PLength)++] = plci->dtmf_pending_r2bwd_digit_code;
  }
  else
  {
    plci->internal_req_buffer[0] = DTMF_UDATA_REQUEST_DISABLE_RECEIVER;
    plci->NData[0].PLength = 1;

    capidtmf_recv_disable (&(plci->capidtmf_state));

  }
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void dtmf_send_digits (PLCI   *plci, byte   *digit_buffer, word digit_count)
{
  word w, i;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_send_digits %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, digit_count));

  plci->internal_req_buffer[0] = DTMF_UDATA_REQUEST_SEND_DIGITS;
  w = (plci->dtmf_send_pulse_ms == 0) ? 40 : plci->dtmf_send_pulse_ms;
  WRITE_WORD (&plci->internal_req_buffer[1], w);
  w = (plci->dtmf_send_pause_ms == 0) ? 40 : plci->dtmf_send_pause_ms;
  WRITE_WORD (&plci->internal_req_buffer[3], w);
  for (i = 0; i < digit_count; i++)
  {
    w = 0;
    while ((w < DTMF_DIGIT_MAP_ENTRIES)
      && (digit_buffer[i] != dtmf_digit_map[w].character))
    {
      w++;
    }
    plci->internal_req_buffer[5+i] = (w < DTMF_DIGIT_MAP_ENTRIES) ?
      dtmf_digit_map[w].code : DTMF_DIGIT_TONE_CODE_STAR;
  }
  plci->NData[0].PLength = 5 + digit_count;
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void dtmf_rec_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_rec_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->dtmf_rec_active = 0;
  plci->dtmf_rec_active_modem_fax = 0;
  plci->dtmf_rec_pulse_ms = 0;
  plci->dtmf_rec_pause_ms = 0;
  plci->dtmf_pending_dtmf_digit_code = DTMF_DIGIT_TONE_CODE_NONE;
  plci->dtmf_pending_mf_digit_code = DTMF_MF_DIGIT_TONE_CODE_NONE;
  plci->dtmf_pending_r2fwd_digit_code = DTMF_R2_FORWARD_DIGIT_TONE_CODE_NONE;
  plci->dtmf_pending_r2bwd_digit_code = DTMF_R2_BACKWARD_DIGIT_TONE_CODE_NONE;

  capidtmf_init (&(plci->capidtmf_state), plci->adapter->u_law);


  plci->dtmf_ondata_rec_active = 0;
  plci->dtmf_ondata_rec_pulse_ms = 0;
  plci->dtmf_ondata_rec_pause_ms = 0;
  plci->dtmf_ondata_pending_dtmf_digit_code = DTMF_DIGIT_TONE_CODE_NONE;

  capidtmf_init (&(plci->capidtmf_ondata_state), plci->adapter->u_law);


}


static void dtmf_send_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_send_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->dtmf_send_requests = 0;
  plci->dtmf_send_pulse_ms = 0;
  plci->dtmf_send_pause_ms = 0;

  plci->dtmf_ondata_rec_active = 0;
  plci->dtmf_ondata_rec_pulse_ms = 0;
  plci->dtmf_ondata_rec_pause_ms = 0;

}


static void dtmf_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  while (plci->dtmf_send_requests != 0)
    dtmf_confirmation (Id, plci);
}


static word dtmf_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word dtmf_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if ((plci->B1_facilities & B1_FACILITY_DTMFR)
   || ((plci->dtmf_rec_active_modem_fax != 0)
    && (((plci->B1_resource & 0xff) == 16)
     || ((plci->B1_resource & 0xff) == 17)
     || ((plci->B1_resource & 0xff) == 18))))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_DTMF_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci_nl_busy (plci))
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_1;
        break;
      }
      dtmf_enable_receiver (plci, plci->dtmf_rec_active);
      plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_2;
      break;
    case ADJUST_B_RESTORE_DTMF_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Reenable DTMF receiver failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static word dtmf_function_mask (PLCI   *plci, APPL   *appl, word dtmf_cmd)
{
  word mask;

  switch (dtmf_cmd)
  {

  case DTMF_LISTEN_R2BWD_START:
  case DTMF_LISTEN_R2BWD_STOP:
    mask = DTMF_R2BWD_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_R2FWD_START:
  case DTMF_LISTEN_R2FWD_STOP:
    mask = DTMF_R2FWD_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_HOOK_START:
  case DTMF_LISTEN_HOOK_STOP:
    mask = DTMF_HOOK_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_SEND_HOOK:
    mask = DTMF_HOOK_SEND_FLAG;
    break;
  case DTMF_SUPPRESSION_START:
  case DTMF_SUPPRESSION_STOP:
    mask = DTMF_SUPPRESSION_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_MF_START:
  case DTMF_LISTEN_MF_STOP:
    mask = DTMF_MF_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_PULSE_START:
  case DTMF_LISTEN_PULSE_STOP:
    mask = DTMF_PULSE_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_TONE_START:
  case DTMF_LISTEN_TONE_STOP:
    mask = DTMF_TONE_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_START:
    mask = ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[appl->Id-1])
      & (1L << PRIVATE_DTMF_TONE)) ? DTMF_PRIV_LISTEN_ACTIVE_FLAG : DTMF_LISTEN_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_STOP:
    mask = DTMF_LISTEN_ACTIVE_FLAG | DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG;
    break;
  case DTMF_LISTEN_EDGE:
    mask = ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[appl->Id-1])
      & (1L << PRIVATE_DTMF_TONE)) ? DTMF_PRIV_EDGE_ACTIVE_FLAG : DTMF_EDGE_ACTIVE_FLAG;
    break;
  case DTMF_SEND_R2BWD:
    mask = DTMF_R2BWD_SEND_FLAG;
    break;
  case DTMF_SEND_R2FWD:
    mask = DTMF_R2FWD_SEND_FLAG;
    break;
  case DTMF_SEND_MF:
    mask = DTMF_MF_SEND_FLAG;
    break;
  case DTMF_SEND_TONE:
    mask = DTMF_TONE_SEND_FLAG;
    break;
  case DTMF_DIGITS_SEND:
    mask = ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[appl->Id-1])
      & (1L << PRIVATE_DTMF_TONE)) ? DTMF_PRIV_SEND_FLAG : DTMF_SEND_FLAG;
    break;

  default:
    mask = 0;
  }
  return (mask);
}


static void dtmf_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
  word mask, b1_facilities;
    byte result[4];

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_command %02x %04x %04x %d %d %d %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->dtmf_cmd, plci->dtmf_rec_pulse_ms, plci->dtmf_rec_pause_ms,
    plci->dtmf_send_pulse_ms, plci->dtmf_send_pause_ms));

  Info = GOOD;
  result[0] = 2;
  WRITE_WORD (&result[1], DTMF_SUCCESS);
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  mask = dtmf_function_mask (plci, plci->appl, plci->dtmf_cmd);
  switch (plci->dtmf_cmd)
  {

  case DTMF_LISTEN_R2BWD_START:
  case DTMF_LISTEN_R2FWD_START:
  case DTMF_LISTEN_HOOK_START:
  case DTMF_SUPPRESSION_START:
  case DTMF_LISTEN_PULSE_START:
  case DTMF_LISTEN_TONE_START:
  case DTMF_LISTEN_MF_START:

  case DTMF_LISTEN_START:
  case DTMF_LISTEN_EDGE:
    switch (internal_command)
    {
    default:

      if (mask & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                  DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG))
      {
        plci->dtmf_rec_active &= ~(DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                                   DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG);
      }

      b1_facilities = plci->B1_facilities | B1_FACILITY_DTMFR;

      if ((plci->dtmf_rec_active | mask) & (DTMF_TONE_LISTEN_ACTIVE_FLAG |
          DTMF_MF_LISTEN_ACTIVE_FLAG | DTMF_SUPPRESSION_ACTIVE_FLAG | DTMF_HOOK_LISTEN_ACTIVE_FLAG |
          DTMF_R2FWD_LISTEN_ACTIVE_FLAG | DTMF_R2BWD_LISTEN_ACTIVE_FLAG))
      {
        b1_facilities |= B1_FACILITY_DTMFTONE;
      }

      adjust_b1_resource (Id, plci, NULL, b1_facilities, DTMF_COMMAND_1);
    case DTMF_COMMAND_1:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Load DTMF failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
    case DTMF_COMMAND_2:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = DTMF_COMMAND_2;
        return;
      }
      plci->internal_command = DTMF_COMMAND_3;
      dtmf_enable_receiver (plci, (word)(plci->dtmf_rec_active | mask));
      return;
    case DTMF_COMMAND_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Enable DTMF receiver failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }

      plci->tone_last_indication_code = DTMF_SIGNAL_NO_TONE;

      plci->dtmf_rec_active |= mask;
      if (((plci->B1_resource & 0xff) == 16)
       || ((plci->B1_resource & 0xff) == 17)
       || ((plci->B1_resource & 0xff) == 18))
      {
        plci->dtmf_rec_active_modem_fax |= mask;
      }
      break;
    }
    break;


  case DTMF_LISTEN_R2BWD_STOP:
  case DTMF_LISTEN_R2FWD_STOP:
  case DTMF_LISTEN_HOOK_STOP:
  case DTMF_SUPPRESSION_STOP:
  case DTMF_LISTEN_PULSE_STOP:
  case DTMF_LISTEN_TONE_STOP:
  case DTMF_LISTEN_MF_STOP:

  case DTMF_LISTEN_STOP:
    switch (internal_command)
    {
    default:
      plci->dtmf_rec_active &= ~mask;
      plci->dtmf_rec_active_modem_fax &= ~mask;

      if (!(plci->dtmf_rec_active & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG |
                                     DTMF_PRIV_LISTEN_ACTIVE_FLAG | DTMF_PRIV_EDGE_ACTIVE_FLAG)))

      {
        plci->dtmf_pending_dtmf_digit_code = DTMF_DIGIT_TONE_CODE_NONE;
      }
      if (!(plci->dtmf_rec_active & DTMF_MF_LISTEN_ACTIVE_FLAG))
      {
        plci->dtmf_pending_mf_digit_code = DTMF_MF_DIGIT_TONE_CODE_NONE;
      }
      if (!(plci->dtmf_rec_active & DTMF_R2FWD_LISTEN_ACTIVE_FLAG))
      {
        plci->dtmf_pending_r2fwd_digit_code = DTMF_R2_FORWARD_DIGIT_TONE_CODE_NONE;
      }
      if (!(plci->dtmf_rec_active & DTMF_R2BWD_LISTEN_ACTIVE_FLAG))
      {
        plci->dtmf_pending_r2bwd_digit_code = DTMF_R2_BACKWARD_DIGIT_TONE_CODE_NONE;
      }
    case DTMF_COMMAND_1:
      b1_facilities = plci->B1_facilities;

      if (!(plci->dtmf_rec_active & (DTMF_TONE_LISTEN_ACTIVE_FLAG |
           DTMF_MF_LISTEN_ACTIVE_FLAG | DTMF_SUPPRESSION_ACTIVE_FLAG | DTMF_HOOK_LISTEN_ACTIVE_FLAG |
           DTMF_R2FWD_LISTEN_ACTIVE_FLAG | DTMF_R2BWD_LISTEN_ACTIVE_FLAG)))
      {
        b1_facilities &= ~B1_FACILITY_DTMFTONE;
      }

      if (plci->dtmf_rec_active == 0)
        b1_facilities &= ~(B1_FACILITY_DTMFX | B1_FACILITY_DTMFR);
      if (add_b1_facilities (plci, plci->B1_resource, b1_facilities) ==
          add_b1_facilities (plci, plci->B1_resource, plci->B1_facilities))
      {
        if (plci_nl_busy (plci))
        {
          plci->internal_command = DTMF_COMMAND_1;
          return;
        }
        plci->internal_command = DTMF_COMMAND_2;
        dtmf_enable_receiver (plci, plci->dtmf_rec_active);
        return;
      }
      Rc = OK;
    case DTMF_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Disable DTMF receiver failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }

      if (plci->dtmf_rec_active & (DTMF_TONE_LISTEN_ACTIVE_FLAG |
          DTMF_MF_LISTEN_ACTIVE_FLAG | DTMF_SUPPRESSION_ACTIVE_FLAG | DTMF_HOOK_LISTEN_ACTIVE_FLAG |
          DTMF_R2FWD_LISTEN_ACTIVE_FLAG | DTMF_R2BWD_LISTEN_ACTIVE_FLAG))
      {
        break;
      }
      if (!(plci->B1_facilities & B1_FACILITY_DTMFTONE))

      {
        if (plci->dtmf_rec_active != 0)
          break;
      }
      b1_facilities = plci->B1_facilities;

      b1_facilities &= ~B1_FACILITY_DTMFTONE;

      if (plci->dtmf_rec_active == 0)
        b1_facilities &= ~(B1_FACILITY_DTMFX | B1_FACILITY_DTMFR);
      adjust_b1_resource (Id, plci, NULL, b1_facilities, DTMF_COMMAND_3);
    case DTMF_COMMAND_3:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Unload DTMF failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
      break;
    }
    break;


  case DTMF_SEND_R2BWD:
  case DTMF_SEND_R2FWD:
  case DTMF_SEND_HOOK:
  case DTMF_SEND_TONE:
  case DTMF_SEND_MF:

  case DTMF_DIGITS_SEND:
    switch (internal_command)
    {
    default:
      b1_facilities = plci->B1_facilities | B1_FACILITY_DTMFX;
      if (plci->dtmf_parameter_length != 0)
        b1_facilities |= B1_FACILITY_DTMFR;

      if (mask & (DTMF_MF_SEND_FLAG | DTMF_TONE_SEND_FLAG |
          DTMF_R2FWD_SEND_FLAG | DTMF_R2BWD_SEND_FLAG))
      {
        b1_facilities |= B1_FACILITY_DTMFTONE;
      }

      adjust_b1_resource (Id, plci, NULL, b1_facilities, DTMF_COMMAND_1);
    case DTMF_COMMAND_1:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Load DTMF failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
    case DTMF_COMMAND_2:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = DTMF_COMMAND_2;
        return;
      }
      plci->dtmf_msg_number_queue[(plci->dtmf_send_requests)++] = plci->number;
      plci->internal_command = DTMF_COMMAND_3;
      dtmf_send_digits (plci, &plci->saved_msg.parms[3].info[1], plci->saved_msg.parms[3].length);
      return;
    case DTMF_COMMAND_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Send DTMF digits failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        if (plci->dtmf_send_requests != 0)
          (plci->dtmf_send_requests)--;
        Info = _WRONG_STATE;
        break;
      }
      return;
    }
    break;
  }
  sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->number,
    "wws", Info, SELECTOR_DTMF, result);
}


static byte dtmf_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i, j;
  word mask;
    API_PARSE dtmf_parms[5];
    byte result[40];

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 2;
  WRITE_WORD (&result[1], DTMF_SUCCESS);
  if (!(a->profile.Global_Options & GL_DTMF_SUPPORTED))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else if (api_parse (&msg[1].info[1], msg[1].length, "w", dtmf_parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }

  else if ((READ_WORD (dtmf_parms[0].info) == DTMF_GET_SUPPORTED_DETECT_CODES)
    || (READ_WORD (dtmf_parms[0].info) == DTMF_GET_SUPPORTED_SEND_CODES))
  {
    if (READ_WORD(msg[0].info) == PRIV_SELECTOR_DTMF_ONDATA)
    {
      for (i = 0; i < 32; i++)
        result[4 + i] = 0;
      if (READ_WORD (dtmf_parms[0].info) == DTMF_GET_SUPPORTED_DETECT_CODES)
      {

        for (i = 0; i < DTMF_DIGIT_MAP_ENTRIES; i++)
        {
          if (dtmf_digit_map[i].listen_mask & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG))
            result[4 + (dtmf_digit_map[i].character >> 3)] |= (1 << (dtmf_digit_map[i].character & 0x7));
        }

      }
      else
      {
      }
      result[0] = 3 + 32;
      result[3] = 32;
    }
    else
    {
      if (!((a->requested_options_table[appl->Id-1]) & (1L << PRIVATE_DTMF_TONE)))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: DTMF unknown request %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, READ_WORD (dtmf_parms[0].info)));
        WRITE_WORD (&result[1], DTMF_UNKNOWN_REQUEST);
      }
      else
      {
        for (i = 0; i < 32; i++)
          result[4 + i] = 0;
        if (READ_WORD (dtmf_parms[0].info) == DTMF_GET_SUPPORTED_DETECT_CODES)
        {
          for (i = 0; i < DTMF_DIGIT_MAP_ENTRIES; i++)
          {
            if ((dtmf_digit_map[i].listen_mask != 0)
             && (((a->manufacturer_features & MANUFACTURER_FEATURE_DTMF_TONE)
               && (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_SOFTIP)
                || (a->manufacturer_features2 & MANUFACTURER_FEATURE2_CLEAR_CHANNEL)))
              || (dtmf_digit_map[i].listen_mask & (DTMF_LISTEN_ACTIVE_FLAG | DTMF_EDGE_ACTIVE_FLAG))))
            {
              result[4 + (dtmf_digit_map[i].character >> 3)] |= (1 << (dtmf_digit_map[i].character & 0x7));
            }
          }
        }
        else
        {
          for (i = 0; i < DTMF_DIGIT_MAP_ENTRIES; i++)
          {
            if (dtmf_digit_map[i].send_mask != 0)
              result[4 + (dtmf_digit_map[i].character >> 3)] |= (1 << (dtmf_digit_map[i].character & 0x7));
          }
        }
        result[0] = 3 + 32;
        result[3] = 32;
      }
    }
  }

  else if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else
  {
    if ((plci->State == IDLE)
     || (plci->State == OUTG_DIS_PENDING)
     || (plci->State == INC_DIS_PENDING)
     || !plci->NL.Id || plci->nl_remove_id)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      Info = _WRONG_STATE;
    }
    else
    {
      plci->command = 0;
      plci->dtmf_cmd = READ_WORD (dtmf_parms[0].info);
      mask = dtmf_function_mask (plci, appl, plci->dtmf_cmd);

      if (READ_WORD(msg[0].info) == PRIV_SELECTOR_DTMF_ONDATA)
      {
        switch (plci->dtmf_cmd)
        {
        case DTMF_LISTEN_START:
        case DTMF_LISTEN_STOP:
        case DTMF_LISTEN_EDGE:
          if (api_parse (&msg[1].info[1], msg[1].length, "wwws", dtmf_parms))
          {
            plci->dtmf_ondata_rec_pulse_ms = 0;
            plci->dtmf_ondata_rec_pause_ms = 0;
          }
          else
          {
            plci->dtmf_ondata_rec_pulse_ms = READ_WORD (dtmf_parms[1].info);
            plci->dtmf_ondata_rec_pause_ms = READ_WORD (dtmf_parms[2].info);
          }
          if ((plci->dtmf_cmd == DTMF_LISTEN_START)
           || (plci->dtmf_cmd == DTMF_LISTEN_EDGE))
          {
            plci->dtmf_ondata_rec_active |= mask;

            capidtmf_recv_enable (&(plci->capidtmf_ondata_state),
              plci->dtmf_ondata_rec_pulse_ms, plci->dtmf_ondata_rec_pause_ms,
              plci->dtmf_ondata_pending_dtmf_digit_code);

          }
          else
          {
            plci->dtmf_ondata_rec_active &= ~mask;
            plci->dtmf_ondata_pending_dtmf_digit_code = DTMF_DIGIT_TONE_CODE_NONE;

            capidtmf_recv_disable (&(plci->capidtmf_ondata_state));

          }
          break;

        default:
          dbug (1, dprintf ("[%06lx] %s,%d: DTMF unknown request %04x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->dtmf_cmd));
          WRITE_WORD (&result[1], DTMF_UNKNOWN_REQUEST);
          break;
        }
      }
      else

      {
        switch (plci->dtmf_cmd)
        {

        case DTMF_LISTEN_R2BWD_START:
        case DTMF_LISTEN_R2BWD_STOP:
        case DTMF_LISTEN_R2FWD_START:
        case DTMF_LISTEN_R2FWD_STOP:
        case DTMF_LISTEN_HOOK_START:
        case DTMF_LISTEN_HOOK_STOP:
        case DTMF_SUPPRESSION_START:
        case DTMF_SUPPRESSION_STOP:
        case DTMF_LISTEN_PULSE_START:
        case DTMF_LISTEN_PULSE_STOP:
        case DTMF_LISTEN_TONE_START:
        case DTMF_LISTEN_TONE_STOP:
        case DTMF_LISTEN_MF_START:
        case DTMF_LISTEN_MF_STOP:
          if (!((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[appl->Id-1])
            & (1L << PRIVATE_DTMF_TONE)))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: DTMF unknown request %04x",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, READ_WORD (dtmf_parms[0].info)));
            WRITE_WORD (&result[1], DTMF_UNKNOWN_REQUEST);
            break;
          }

        case DTMF_LISTEN_START:
        case DTMF_LISTEN_STOP:
        case DTMF_LISTEN_EDGE:
          if (!(a->manufacturer_features & MANUFACTURER_FEATURE_HARDDTMF)
           && !(a->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            Info = _FACILITY_NOT_SUPPORTED;
            break;
          }
          if ((plci->dtmf_cmd == DTMF_LISTEN_START) || (plci->dtmf_cmd == DTMF_LISTEN_STOP) || (plci->dtmf_cmd == DTMF_LISTEN_EDGE))
          {
            if (api_parse (&msg[1].info[1], msg[1].length, "wwws", dtmf_parms))
            {
              plci->dtmf_rec_pulse_ms = 0;
              plci->dtmf_rec_pause_ms = 0;
            }
            else
            {
              plci->dtmf_rec_pulse_ms = READ_WORD (dtmf_parms[1].info);
              plci->dtmf_rec_pause_ms = READ_WORD (dtmf_parms[2].info);
            }
          }
          start_internal_command (Id, plci, dtmf_command);
          return (FALSE);


        case DTMF_SEND_R2BWD:
        case DTMF_SEND_R2FWD:
        case DTMF_SEND_HOOK:
        case DTMF_SEND_TONE:
        case DTMF_SEND_MF:
          if (!((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[appl->Id-1])
            & (1L << PRIVATE_DTMF_TONE)))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: DTMF unknown request %04x",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, READ_WORD (dtmf_parms[0].info)));
            WRITE_WORD (&result[1], DTMF_UNKNOWN_REQUEST);
            break;
          }

        case DTMF_DIGITS_SEND:
          if (api_parse (&msg[1].info[1], msg[1].length, "wwws", dtmf_parms))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            Info = _WRONG_MESSAGE_FORMAT;
            break;
          }
          if (plci->dtmf_cmd == DTMF_DIGITS_SEND)
          {
            plci->dtmf_send_pulse_ms = READ_WORD (dtmf_parms[1].info);
            plci->dtmf_send_pause_ms = READ_WORD (dtmf_parms[2].info);
          }
          i = 0;
          j = 0;
          while ((i < dtmf_parms[3].length) && (j < DTMF_DIGIT_MAP_ENTRIES))
          {
            j = 0;
            while ((j < DTMF_DIGIT_MAP_ENTRIES)
              && ((dtmf_parms[3].info[i+1] != dtmf_digit_map[j].character)
               || ((dtmf_digit_map[j].send_mask & mask) == 0)))
            {
              j++;
            }
            i++;
          }
          if (j == DTMF_DIGIT_MAP_ENTRIES)
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Incorrect DTMF digit %02x",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, dtmf_parms[3].info[i]));
            WRITE_WORD (&result[1], DTMF_INCORRECT_DIGIT);
            break;
          }
          if (plci->dtmf_send_requests >=
            sizeof(plci->dtmf_msg_number_queue) / sizeof(plci->dtmf_msg_number_queue[0]))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: DTMF request overrun",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            Info = _WRONG_STATE;
            break;
          }
          api_save_msg (dtmf_parms, "wwws", &plci->saved_msg);
          start_internal_command (Id, plci, dtmf_command);
          return (FALSE);

        default:
          dbug (1, dprintf ("[%06lx] %s,%d: DTMF unknown request %04x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->dtmf_cmd));
          WRITE_WORD (&result[1], DTMF_UNKNOWN_REQUEST);
        }
      }
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, READ_WORD(msg[0].info), result);
  return (FALSE);
}


static void dtmf_confirmation (dword Id, PLCI   *plci)
{
  word Info;
  word i;
    byte result[4];

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_confirmation",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 2;
  WRITE_WORD (&result[1], DTMF_SUCCESS);
  if (plci->dtmf_send_requests != 0)
  {
    sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->dtmf_msg_number_queue[0],
      "wws", GOOD, SELECTOR_DTMF, result);
    (plci->dtmf_send_requests)--;
    for (i = 0; i < plci->dtmf_send_requests; i++)
      plci->dtmf_msg_number_queue[i] = plci->dtmf_msg_number_queue[i+1];
  }
}


static void dtmf_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{
  word i, j, n;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  n = 0;
  for (i = 1; i < length; i++)
  {
    if ((msg[i] < 0x10) || (msg[i] == DTMF_DIGIT_TONE_CODE_NONE))
      plci->dtmf_pending_dtmf_digit_code = msg[i];
    if ((msg[i] >= DTMF_MF_DIGIT_TONE_CODE_1) && (msg[i] <= DTMF_MF_DIGIT_TONE_CODE_NONE))
      plci->dtmf_pending_mf_digit_code = msg[i];
    if ((msg[i] >= DTMF_R2_FORWARD_DIGIT_TONE_CODE_1) && (msg[i] <= DTMF_R2_FORWARD_DIGIT_TONE_CODE_NONE))
      plci->dtmf_pending_r2fwd_digit_code = msg[i];
    if ((msg[i] >= DTMF_R2_BACKWARD_DIGIT_TONE_CODE_1) && (msg[i] <= DTMF_R2_BACKWARD_DIGIT_TONE_CODE_NONE))
      plci->dtmf_pending_r2bwd_digit_code = msg[i];
    j = 0;
    while ((j < DTMF_DIGIT_MAP_ENTRIES)
      && ((msg[i] != dtmf_digit_map[j].code)
       || ((dtmf_digit_map[j].listen_mask & plci->dtmf_rec_active) == 0)))
    {
      j++;
    }
    if (j < DTMF_DIGIT_MAP_ENTRIES)
    {

      if ((dtmf_digit_map[j].listen_mask & DTMF_TONE_LISTEN_ACTIVE_FLAG)
       && (plci->tone_last_indication_code == DTMF_SIGNAL_NO_TONE)
       && (dtmf_digit_map[j].character != DTMF_SIGNAL_UNIDENTIFIED_TONE))
      {
        if (n + 1 == i)
        {
          for (i = length; i > n + 1; i--)
            msg[i] = msg[i - 1];
          length++;
          i++;
        }
        msg[++n] = DTMF_SIGNAL_UNIDENTIFIED_TONE;
      }
      plci->tone_last_indication_code = dtmf_digit_map[j].character;

      msg[++n] = dtmf_digit_map[j].character;
    }
  }
  if (n != 0)
  {
    msg[0] = (byte) n;
    sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "wS", SELECTOR_DTMF, msg);
  }
}



static void dtmf_ondata_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{
  word i, j, n;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_ondata_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  n = 0;
  for (i = 1; i < length; i++)
  {
    if ((msg[i] < 0x10) || (msg[i] == DTMF_DIGIT_TONE_CODE_NONE))
      plci->dtmf_ondata_pending_dtmf_digit_code = msg[i];
    j = 0;
    while ((j < DTMF_DIGIT_MAP_ENTRIES)
      && ((msg[i] != dtmf_digit_map[j].code)
       || ((dtmf_digit_map[j].listen_mask & plci->dtmf_ondata_rec_active) == 0)))
    {
      j++;
    }
    if (j < DTMF_DIGIT_MAP_ENTRIES)
    {
      msg[++n] = dtmf_digit_map[j].character;
    }
  }
  if (n != 0)
  {
    msg[0] = (byte) n;
    sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "wS", PRIV_SELECTOR_DTMF_ONDATA, msg);
  }
}



/*------------------------------------------------------------------*/
/* DTMF parameters                                                  */
/*------------------------------------------------------------------*/

static void dtmf_parameter_write (PLCI   *plci)
{
  word i;
    byte parameter_buffer[DTMF_PARAMETER_BUFFER_SIZE + 2];

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_parameter_write",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  parameter_buffer[0] = plci->dtmf_parameter_length + 1;
  parameter_buffer[1] = DSP_CTRL_SET_DTMF_PARAMETERS;
  for (i = 0; i < plci->dtmf_parameter_length; i++)
    parameter_buffer[2+i] = plci->dtmf_parameter_buffer[i];
  add_p (plci, FTY, parameter_buffer);
  sig_req (plci, TEL_CTRL, 0);
  send_req (plci);
}


static void dtmf_parameter_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_parameter_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->dtmf_parameter_length = 0;
}


static void dtmf_parameter_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_parameter_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word dtmf_parameter_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_parameter_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word dtmf_parameter_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_parameter_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if ((plci->B1_facilities & B1_FACILITY_DTMFR)
   && (plci->dtmf_parameter_length != 0))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_DTMF_PARAMETER_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci->sig_req)
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_PARAMETER_1;
        break;
      }
      dtmf_parameter_write (plci);
      plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_PARAMETER_2;
      break;
    case ADJUST_B_RESTORE_DTMF_PARAMETER_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore DTMF parameters failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


/*------------------------------------------------------------------*/
/* Pitch parameters                                                 */
/*------------------------------------------------------------------*/


static void pitch_parameter_write (PLCI   *plci)
{
    byte parameter_buffer[8];

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_parameter_write",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  parameter_buffer[0] = 7;
  parameter_buffer[1] = DSP_CTRL_SET_PITCH_PARAMETERS;
  parameter_buffer[2] = (byte)(plci->pitch_flags);
  parameter_buffer[3] = (byte)(plci->pitch_flags >> 8);
  parameter_buffer[4] = (byte)(plci->pitch_rx_sample_rate);
  parameter_buffer[5] = (byte)(plci->pitch_rx_sample_rate >> 8);
  parameter_buffer[6] = (byte)(plci->pitch_tx_sample_rate);
  parameter_buffer[7] = (byte)(plci->pitch_tx_sample_rate >> 8);
  add_p (plci, FTY, parameter_buffer);
  sig_req (plci, TEL_CTRL, 0);
  send_req (plci);
}


static void pitch_parameter_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_parameter_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->pitch_flags = 0;
  plci->pitch_rx_sample_rate = 0;
  plci->pitch_tx_sample_rate = 0;
}


static void pitch_parameter_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_parameter_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word pitch_parameter_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_parameter_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word pitch_parameter_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_parameter_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if ((plci->B1_facilities & B1_FACILITY_PITCH)
   && (plci->pitch_flags & PITCH_FLAG_ENABLE))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_PITCH_PARAMETER_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci->sig_req)
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_PITCH_PARAMETER_1;
        break;
      }
      pitch_parameter_write (plci);
      plci->adjust_b_state = ADJUST_B_RESTORE_PITCH_PARAMETER_2;
      break;
    case ADJUST_B_RESTORE_PITCH_PARAMETER_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore pitch parameters failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static void pitch_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_command %02x %04x %04x %d %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->pitch_flags, plci->pitch_rx_sample_rate, plci->pitch_tx_sample_rate));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  if (plci->pitch_flags & PITCH_FLAG_ENABLE)
  {
    switch (internal_command)
    {
    default:
      adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities |
        B1_FACILITY_PITCH), PITCH_COMMAND_1);
    case PITCH_COMMAND_1:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Load pitch failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
    case PITCH_COMMAND_2:
      if (plci->sig_req)
      {
        plci->internal_command = PITCH_COMMAND_2;
        return;
      }
      plci->internal_command = PITCH_COMMAND_3;
      pitch_parameter_write (plci);
      return;
    case PITCH_COMMAND_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Enable pitch failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  else
  {
    switch (internal_command)
    {
    default:
    case PITCH_COMMAND_1:
      if (plci->B1_facilities & B1_FACILITY_PITCH)
      {
        if (plci->sig_req)
        {
          plci->internal_command = PITCH_COMMAND_1;
          return;
        }
        plci->internal_command = PITCH_COMMAND_2;
        pitch_parameter_write (plci);
        return;
      }
      Rc = OK;
    case PITCH_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Disable pitch failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities &
        ~B1_FACILITY_PITCH), PITCH_COMMAND_3);
    case PITCH_COMMAND_3:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Unload pitch failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
      break;
    }
  }
  sendf(plci->appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, plci->number,
    "dww", _DI_MANU_ID, _DI_DSP_CTRL, Info);
}


static byte pitch_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  byte parm_len;

  dbug (1, dprintf ("[%06lx] %s,%d: pitch_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;

  if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING)
   || !plci->NL.Id || plci->nl_remove_id)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_STATE;
  }
  else
  {
    plci->command = 0;
    plci->pitch_flags = 0;
    plci->pitch_rx_sample_rate = 0;
    plci->pitch_tx_sample_rate = 0;
    parm_len = msg[2].info[2] - 1;
    if (parm_len > msg[2].length - 3)
      parm_len = (byte)(msg[2].length - 3);
    if (parm_len >= 2)
    {
      plci->pitch_flags = READ_WORD(&msg[2].info[4]);
      if (parm_len >= 4)
      {
        plci->pitch_rx_sample_rate = READ_WORD(&msg[2].info[6]);
        if (parm_len >= 6)
        {
          plci->pitch_tx_sample_rate = READ_WORD(&msg[2].info[8]);
        }
      }
    }
    start_internal_command (Id, plci, pitch_command);
    return (FALSE);
  }
  sendf(appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, Number,
    "dww", _DI_MANU_ID, _DI_DSP_CTRL, Info);
  return (FALSE);
}


/*------------------------------------------------------------------*/
/* Audio parameters                                                 */
/*------------------------------------------------------------------*/


static void audio_parameter_write (PLCI   *plci)
{
    byte parameter_buffer[8];
  word w;

  dbug (1, dprintf ("[%06lx] %s,%d: audio_parameter_write",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  parameter_buffer[0] = 7;
  parameter_buffer[1] = DSP_CTRL_SET_AUDIO_PARAMETERS;
  parameter_buffer[2] = (byte)(plci->audio_flags);
  parameter_buffer[3] = (byte)(plci->audio_flags >> 8);
  w = (plci->tx_digital_gain != 0) ? plci->tx_digital_gain : 0x8000;
  parameter_buffer[4] = (byte) w;
  parameter_buffer[5] = (byte)(w >> 8);
  w = (plci->rx_digital_gain != 0) ? plci->rx_digital_gain : 0x8000;
  parameter_buffer[6] = (byte) w;
  parameter_buffer[7] = (byte)(w >> 8);
  add_p (plci, FTY, parameter_buffer);
  sig_req (plci, TEL_CTRL, 0);
  send_req (plci);
}


static void audio_parameter_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: audio_parameter_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->audio_flags = 0;
  plci->tx_digital_gain = 0;
  plci->rx_digital_gain = 0;
}


static void audio_parameter_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: audio_parameter_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word audio_parameter_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: audio_parameter_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word audio_parameter_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: audio_parameter_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if ((plci->audio_flags != 0)
   || ((plci->tx_digital_gain != 0) && (plci->tx_digital_gain != 0x8000))
   || ((plci->rx_digital_gain != 0) && (plci->rx_digital_gain != 0x8000)))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_AUDIO_PARAMETER_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci->sig_req)
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_AUDIO_PARAMETER_1;
        break;
      }
      audio_parameter_write (plci);
      plci->adjust_b_state = ADJUST_B_RESTORE_AUDIO_PARAMETER_2;
      break;
    case ADJUST_B_RESTORE_AUDIO_PARAMETER_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore audio parameters failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static void audio_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;

  dbug (1, dprintf ("[%06lx] %s,%d: audio_command %02x %04x %04x %04x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->audio_flags, plci->tx_digital_gain, plci->rx_digital_gain));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
  case AUDIO_COMMAND_1:
    if (plci->sig_req)
    {
      plci->internal_command = AUDIO_COMMAND_1;
      return;
    }
    plci->internal_command = AUDIO_COMMAND_2;
    audio_parameter_write (plci);
    return;
  case AUDIO_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Enable audio failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    break;
  }
  sendf(plci->appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, plci->number,
    "dww", _DI_MANU_ID, _DI_DSP_CTRL, Info);
}


static byte audio_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  byte parm_len;

  dbug (1, dprintf ("[%06lx] %s,%d: audio_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;

  if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING)
   || !plci->NL.Id || plci->nl_remove_id)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_STATE;
  }
  else
  {
    plci->command = 0;
    plci->audio_flags = 0;
    plci->tx_digital_gain = 0;
    plci->rx_digital_gain = 0;
    parm_len = msg[2].info[2] - 1;
    if (parm_len > msg[2].length - 3)
      parm_len = (byte)(msg[2].length - 3);
    if (parm_len >= 2)
    {
      plci->audio_flags = READ_WORD(&msg[2].info[4]);
      if (parm_len >= 4)
      {
        plci->tx_digital_gain = READ_WORD(&msg[2].info[6]);
        if (parm_len >= 6)
        {
          plci->rx_digital_gain = READ_WORD(&msg[2].info[8]);
        }
      }
    }
    start_internal_command (Id, plci, audio_command);
    return (FALSE);
  }
  sendf(appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, Number,
    "dww", _DI_MANU_ID, _DI_DSP_CTRL, Info);
  return (FALSE);
}


/*------------------------------------------------------------------*/
/* SIG transmit                                                     */
/*------------------------------------------------------------------*/

static void sig_tx_write (PLCI   *plci, byte *fty_parameters)
{
  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_write %02x %02x %02x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__,
    fty_parameters[0], fty_parameters[1], fty_parameters[2]));

  add_p (plci, FTY, fty_parameters);
  sig_req (plci, DSP_CTRL, 0);
  send_req (plci);
}


static void sig_tx_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->sig_tx_tone[0] = 0;
  plci->sig_tx_digits[0] = 0;
  plci->sig_tx_set_detector[0] = 0;
  plci->sig_tx_set_generator[0] = 0;
  plci->sig_tx_task_control[0] = 0;
  plci->sig_tx_set_modulation[0] = 0;
}


static void sig_tx_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word sig_tx_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word sig_tx_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if ((plci->sig_tx_tone[0] != 0)
   || (plci->sig_tx_digits[0] != 0)
   || (plci->sig_tx_set_detector[0] != 0)
   || (plci->sig_tx_set_generator[0] != 0)
   || (plci->sig_tx_task_control[0] != 0)
   || (plci->sig_tx_set_modulation[0] != 0))
  {
    if (plci->sig_req)
    {
      plci->internal_command = plci->adjust_b_command;
      return (Info);
    }
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_SIG_TX_1:
      if (plci->sig_tx_task_control[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_task_control);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_2;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->sig_tx_set_generator[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_set_generator);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_3;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->sig_tx_tone[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_tone);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_4;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_4:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->sig_tx_digits[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_digits);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_5;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_5:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->sig_tx_set_detector[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_set_detector);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_6;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_6:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->sig_tx_set_modulation[0] != 0)
      {
        plci->internal_command = plci->adjust_b_command;
        sig_tx_write (plci, plci->sig_tx_set_modulation);
        plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_7;
        break;
      }
      Rc = OK;
    case ADJUST_B_RESTORE_SIG_TX_7:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore SIG TX failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static void sig_tx_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  if (plci->sig_req)
    return;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
  case SIG_TX_COMMAND_1:
    if (plci->sig_tx_task_control[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_2;
      sig_tx_write (plci, plci->sig_tx_task_control);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->sig_tx_set_generator[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_3;
      sig_tx_write (plci, plci->sig_tx_set_generator);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_3:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->sig_tx_tone[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_4;
      sig_tx_write (plci, plci->sig_tx_tone);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_4:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->sig_tx_digits[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_5;
      sig_tx_write (plci, plci->sig_tx_digits);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_5:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->sig_tx_set_detector[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_6;
      sig_tx_write (plci, plci->sig_tx_set_detector);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_6:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->sig_tx_set_modulation[0] != 0)
    {
      plci->internal_command = SIG_TX_COMMAND_7;
      sig_tx_write (plci, plci->sig_tx_set_modulation);
      return;
    }
    Rc = OK;
  case SIG_TX_COMMAND_7:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: SIG TX failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    break;
  }
  sendf(plci->appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, plci->number,
    "dww", _DI_MANU_ID, _DI_RXT_CTRL, Info);
}


static byte sig_tx_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info, i;
  byte parm_len;

  dbug (1, dprintf ("[%06lx] %s,%d: sig_tx_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;

  if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING)
   || !plci->NL.Id || plci->nl_remove_id)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_STATE;
  }
  else
  {
    plci->command = 0;
    parm_len = msg[2].info[2];
    switch (msg[2].info[4])
    {
    case DSP_CTRL_SIG_TX_TONE:
      if (parm_len >= sizeof(plci->sig_tx_tone))
        parm_len = sizeof(plci->sig_tx_tone) - 1;
      plci->sig_tx_tone[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_tone[i] = msg[2].info[2+i];
      break;
    case DSP_CTRL_SIG_TX_DIGITS:
      if (parm_len >= sizeof(plci->sig_tx_digits))
        parm_len = sizeof(plci->sig_tx_digits) - 1;
      plci->sig_tx_digits[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_digits[i] = msg[2].info[2+i];
      break;
    case DSP_CTRL_SIG_TX_SET_DETECTOR:
      if (parm_len >= sizeof(plci->sig_tx_set_detector))
        parm_len = sizeof(plci->sig_tx_set_detector) - 1;
      plci->sig_tx_set_detector[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_set_detector[i] = msg[2].info[2+i];
      break;
    case DSP_CTRL_SIG_TX_SET_GENERATOR:
      if (parm_len >= sizeof(plci->sig_tx_set_generator))
        parm_len = sizeof(plci->sig_tx_set_generator) - 1;
      plci->sig_tx_set_generator[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_set_generator[i] = msg[2].info[2+i];
      if (parm_len >= 4)
      {
        i = READ_WORD(&(plci->sig_tx_set_generator[3]));
        if (!(i & DSP_CTRL_SIG_ENABLE_GENERATION))
        {
          plci->sig_tx_tone[0] = 0;
          plci->sig_tx_digits[0] = 0;
        }
      }
      break;
    case DSP_CTRL_SIG_TX_TASK_CONTROL:
      if (parm_len >= sizeof(plci->sig_tx_task_control))
        parm_len = sizeof(plci->sig_tx_task_control) - 1;
      plci->sig_tx_task_control[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_task_control[i] = msg[2].info[2+i];
      break;
    case DSP_CTRL_SIG_TX_SET_MODULATION:
      if (parm_len >= sizeof(plci->sig_tx_set_modulation))
        parm_len = sizeof(plci->sig_tx_set_modulation) - 1;
      plci->sig_tx_set_modulation[0] = parm_len;
      for (i = 1; i <= parm_len; i++)
        plci->sig_tx_set_modulation[i] = msg[2].info[2+i];
      break;
    }
    start_internal_command (Id, plci, sig_tx_command);
    return (FALSE);
  }
  sendf(appl, _MANUFACTURER_R|CONFIRM, Id & 0xffff, Number,
    "dww", _DI_MANU_ID, _DI_RXT_CTRL, Info);
  return (FALSE);
}

/*------------------------------------------------------------------*/
/* Line interconnect facilities                                     */
/*------------------------------------------------------------------*/


LI_CONFIG   *li_config_table = 0;
word li_total_channels = 0;


/*------------------------------------------------------------------*/
/* translate a CHI information element to a channel number          */
/* returns 0xff - any channel                                       */
/*         0xfe - chi wrong coding                                  */
/*         0xfd - D-channel                                         */
/*         0x00 - no channel                                        */
/*         else channel number / PRI: timeslot                      */
/* if channels is provided we accept more than one channel.         */
/*------------------------------------------------------------------*/

static byte chi_to_channel (byte   *chi, dword *pchannelmap)
{
  int p;
  int i;
  dword map;
  byte excl;
  byte ofs;
  byte ch;

  if (pchannelmap) *pchannelmap = 0;
  if(!chi[0]) return 0xff;
  excl = 0;

  if(chi[1] & 0x20) {
    if(chi[0]==1 && chi[1]==0xac) return 0xfd; /* exclusive d-channel */
    for(i=1; i<chi[0] && !(chi[i] &0x80); i++);
    if(i==chi[0] || !(chi[i] &0x80)) return 0xfe;
    if((chi[1] |0xc8)!=0xe9) return 0xfe;
    if(chi[1] &0x08) excl = 0x40;

        /* int. id present */
    if(chi[1] &0x40) {
      p=i+1;
      for(i=p; i<chi[0] && !(chi[i] &0x80); i++);
      if(i==chi[0] || !(chi[i] &0x80)) return 0xfe;
    }

        /* coding standard, Number/Map, Channel Type */
    p=i+1;
    for(i=p; i<chi[0] && !(chi[i] &0x80); i++);
    if(i==chi[0] || !(chi[i] &0x80)) return 0xfe;
    if((chi[p]|0xd0)!=0xd3) return 0xfe;

        /* Number/Map */
    if(chi[p] &0x10) {

        /* map */
      if((chi[0]-p)==4) ofs = 0;
      else if((chi[0]-p)==3) ofs = 1;
      else return 0xfe;
      ch = 0;
      map = 0;
      for(i=0; i<4 && p<chi[0]; i++) {
        p++;
        ch += 8;
        map <<= 8;
        if(chi[p]) {
          for (ch=0; !(chi[p] & (1 << ch)); ch++);
          map |= chi[p];
        }
      }
      ch += ofs;
      map <<= ofs;
    }
    else {

        /* number */
      p=i+1;
      ch = chi[p] &0x3f;
      if(pchannelmap) {
        if((byte)(chi[0]-p)>30) return 0xfe;
        map = 0;
        for(i=p; i<=chi[0]; i++) {
          if ((chi[i] &0x7f) > 31) return 0xfe;
          map |= (1L << (chi[i] &0x7f));
        }
      }
      else {
        if(p!=chi[0]) return 0xfe;
        if (ch > 31) return 0xfe;
        map = (1L << ch);
      }
      if(chi[p] &0x40) return 0xfe;
    }
    if (pchannelmap) *pchannelmap = map;
    else if (map != ((dword)(1L << ch))) return 0xfe;
    return (byte)(excl | ch);
  }
  else {  /* not PRI */
    for(i=1; i<chi[0] && !(chi[i] &0x80); i++);
    if(i!=chi[0] || !(chi[i] &0x80)) return 0xfe;
    if(chi[1] &0x08) excl = 0x40;

    switch(chi[1] |0x98) {
    case 0x98: return 0;
    case 0x99:
      if (pchannelmap) *pchannelmap = 2;
      return excl |1;
    case 0x9a:
      if (pchannelmap) *pchannelmap = 4;
      return excl |2;
    case 0x9b: return 0xff;
    case 0x9c: return 0xfd; /* d-ch */
    default: return 0xfe;
    }
  }
}


static void mixer_set_bchannel_id_unchecked (PLCI   *plci, byte bchannel_id)
{
  word old_id;

  old_id = plci->li_bchannel_id;
  plci->li_bchannel_id = (plci->adapter->li_pri) ? bchannel_id + 1 : bchannel_id;
  if (plci->li_bchannel_id != old_id)
    plci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
  dbug (1, dprintf ("[%06lx] %s,%d: mixer_set_bchannel_id_unchecked %d %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, bchannel_id, plci->li_bchannel_id));
}


static void mixer_set_bchannel_id_esc (PLCI   *plci, byte bchannel_id)
{
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *splci;
  word old_id;

  if (plci->B1_resource == 0)
    return;
  if ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION)
  {
    a = plci->adapter;
    old_id = plci->li_bchannel_id;
    if (a->li_pri)
    {
      plci->li_bchannel_id = (bchannel_id & 0x1f) + 1;
      if (plci->li_bchannel_id != old_id)
        plci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
    }
    else
    {
      if (((bchannel_id & 0x03) == 1) || ((bchannel_id & 0x03) == 2))
      {
        plci->li_bchannel_id = bchannel_id & 0x03;
        if (plci->li_bchannel_id != old_id)
          plci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
        if ((a->AdvSignalPLCI != NULL) && (a->AdvSignalPLCI != plci) && (a->AdvSignalPLCI->tel == ADV_VOICE))
        {
          splci = a->AdvSignalPLCI;
          if (splci->li_bchannel_id != 3 - plci->li_bchannel_id)
            splci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
          splci->li_bchannel_id = 3 - plci->li_bchannel_id;
          dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_set_bchannel_id_esc %d",
            (unsigned long)((splci->Id << 8) | UnMapController (splci->adapter->Id)),
            (char   *)(FILE_), __LINE__, splci->li_bchannel_id));
        }
      }
    }
  }
  dbug (1, dprintf ("[%06lx] %s,%d: mixer_set_bchannel_id_esc %d %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, bchannel_id, plci->li_bchannel_id));
}


static void mixer_set_bchannel_id (PLCI   *plci, byte   *chi)
{
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *splci;
  word old_id;
  byte ch;

  if (plci->B1_resource == 0)
    return;
  ch = 0;
  if ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION)
  {
    a = plci->adapter;
    old_id = plci->li_bchannel_id;
    ch = chi_to_channel (chi, NULL);
    if (!(ch & 0x80))
    {
      if (a->li_pri)
      {
        plci->li_bchannel_id = (ch & 0x1f) + 1;
        if (plci->li_bchannel_id != old_id)
          plci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
      }
      else
      {
        if (((ch & 0x1f) == 1) || ((ch & 0x1f) == 2))
        {
          plci->li_bchannel_id = ch & 0x1f;
          if (plci->li_bchannel_id != old_id)
            plci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);

          if ((a->AdvSignalPLCI != NULL) && (a->AdvSignalPLCI != plci) && (a->AdvSignalPLCI->tel == ADV_VOICE))
          {
            splci = a->AdvSignalPLCI;
            if (splci->li_bchannel_id != 3 - plci->li_bchannel_id)
              splci->li_channel_bits &= ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES);
            splci->li_bchannel_id = 3 - plci->li_bchannel_id;
            dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_set_bchannel_id %d",
              (unsigned long)((splci->Id << 8) | UnMapController (splci->adapter->Id)),
              (char   *)(FILE_), __LINE__, splci->li_bchannel_id));
          }
        }
      }
    }
  }
  dbug (1, dprintf ("[%06lx] %s,%d: mixer_set_bchannel_id %02x %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, ch, plci->li_bchannel_id));
}


static void mixer_update_uninvolve (word ch)
{
  word i;
  byte uninvolve;

  uninvolve = TRUE;
  if ((li_config_table[ch].chflags & ~(LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE |
      LI_CHFLAG_MONITOR | LI_CHFLAG_MIX | LI_CHFLAG_LOOP)) != 0)
  {
    uninvolve = FALSE;
  }
  else
  {
    for (i = 0; i < li_total_channels; i++)
    {
      if (i == ch)
      {
        if (li_config_table[ch].flag_table[ch] & ~(LI_FLAG_MONITOR | LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT))
          uninvolve = FALSE;
      }
      else
      {
        if ((li_config_table[ch].flag_table[i] != 0)
         || (li_config_table[i].flag_table[ch] != 0))
        {
          uninvolve = FALSE;
        }
      }
    }
  }
  if (uninvolve)
  {
    li_config_table[ch].flag_table[ch] = LI_FLAG_MONITOR | LI_FLAG_MIX;
    li_config_table[ch].channel &= ~LI_CHANNEL_INVOLVED;
  }
}


#define MIXER_MAX_DUMP_CHANNELS 34

static void mixer_dump_channels (DIVA_CAPI_ADAPTER   *a)
{
static char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  word n, i, j;
  byte b;
  char *p;
  static word dump_channel_table[MIXER_MAX_DUMP_CHANNELS];
  static char hex_line[2 * MIXER_MAX_DUMP_CHANNELS + MIXER_MAX_DUMP_CHANNELS / 8 + 4];

  n = 0;
  i = 0;
  while ((i < li_total_channels) && (n < MIXER_MAX_DUMP_CHANNELS))
  {
    if (li_config_table[i].channel & LI_CHANNEL_INVOLVED)
      dump_channel_table[n++] = i;
    i++;
  }
  p = hex_line;
  for (j = 0; j < n; j++)
  {
    if ((j & 0x7) == 0)
      *(p++) = ' ';
    b = UnMapController (li_config_table[dump_channel_table[j]].adapter->Id);
    *(p++) = hex_digit_table[b >> 4];
    *(p++) = hex_digit_table[b & 0xf];
  }
  *p = '\0';
  dbug (1, dprintf ("[%06lx] CTLR    %s",
    (unsigned long)(UnMapController (a->Id)), (char   *) hex_line));
  p = hex_line;
  for (j = 0; j < n; j++)
  {
    if ((j & 0x7) == 0)
      *(p++) = ' ';
    b = (byte)(dump_channel_table[j] - li_config_table[dump_channel_table[j]].adapter->li_base);
    *(p++) = hex_digit_table[b >> 4];
    *(p++) = hex_digit_table[b & 0xf];
  }
  *p = '\0';
  dbug (1, dprintf ("[%06lx] BCHNL   %s",
    (unsigned long)(UnMapController (a->Id)), (char   *) hex_line));
  p = hex_line;
  for (j = 0; j < n; j++)
  {
    if ((j & 0x7) == 0)
      *(p++) = ' ';
    b = li_config_table[dump_channel_table[j]].curchnl;
    *(p++) = hex_digit_table[b >> 4];
    *(p++) = hex_digit_table[b & 0xf];
  }
  *p = '\0';
  dbug (1, dprintf ("[%06lx] CURRENT %s",
    (unsigned long)(UnMapController (a->Id)), (char   *) hex_line));
  p = hex_line;
  for (j = 0; j < n; j++)
  {
    if ((j & 0x7) == 0)
      *(p++) = ' ';
    b = li_config_table[dump_channel_table[j]].channel;
    *(p++) = hex_digit_table[b >> 4];
    *(p++) = hex_digit_table[b & 0xf];
  }
  *p = '\0';
  dbug (1, dprintf ("[%06lx] CHANNEL %s",
    (unsigned long)(UnMapController (a->Id)), (char   *) hex_line));
  p = hex_line;
  for (j = 0; j < n; j++)
  {
    if ((j & 0x7) == 0)
      *(p++) = ' ';
    b = li_config_table[dump_channel_table[j]].chflags;
    *(p++) = hex_digit_table[b >> 4];
    *(p++) = hex_digit_table[b & 0xf];
  }
  *p = '\0';
  dbug (1, dprintf ("[%06lx] CHFLAG  %s",
    (unsigned long)(UnMapController (a->Id)), (char   *) hex_line));
  for (i = 0; i < n; i++)
  {
    p = hex_line;
    for (j = 0; j < n; j++)
    {
      if ((j & 0x7) == 0)
        *(p++) = ' ';
      b = li_config_table[dump_channel_table[i]].flag_table[dump_channel_table[j]];
      *(p++) = hex_digit_table[b >> 4];
      *(p++) = hex_digit_table[b & 0xf];
    }
    *p = '\0';
    dbug (1, dprintf ("[%06lx] FL[%04x]%s",
      (unsigned long)(UnMapController (a->Id)), (UnMapController (li_config_table[dump_channel_table[i]].adapter->Id) << 8) |
      (dump_channel_table[i] - li_config_table[dump_channel_table[i]].adapter->li_base), (char   *) hex_line));
  }
  for (i = 0; i < n; i++)
  {
    p = hex_line;
    for (j = 0; j < n; j++)
    {
      if ((j & 0x7) == 0)
        *(p++) = ' ';
      b = li_config_table[dump_channel_table[i]].coef_table[dump_channel_table[j]];
      *(p++) = hex_digit_table[b >> 4];
      *(p++) = hex_digit_table[b & 0xf];
    }
    *p = '\0';
    dbug (1, dprintf ("[%06lx] CO[%04x]%s",
      (unsigned long)(UnMapController (a->Id)), (UnMapController (li_config_table[dump_channel_table[i]].adapter->Id) << 8) |
      (dump_channel_table[i] - li_config_table[dump_channel_table[i]].adapter->li_base), (char   *) hex_line));
  }
}


#define MIXER_ALIGNED_FLAG_TABLE

static void mixer_calculate_coefs (DIVA_CAPI_ADAPTER   *a)
{
  word n, i, j;

  byte b;


  dbug (1, dprintf ("[%06lx] %s,%d: mixer_calculate_coefs",
    (unsigned long)(UnMapController (a->Id)), (char   *)(FILE_), __LINE__));

  for (i = 0; i < li_total_channels; i++)
  {
    li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
    if (((li_config_table[i].chflags & ~(LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE)) != 0)
     || ((li_config_table[i].flag_table[i] & (LI_FLAG_MONITOR | LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT))
      && (!(li_config_table[i].flag_table[i] & LI_FLAG_MONITOR)
       || !(li_config_table[i].flag_table[i] & (LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT))))
     || (li_config_table[i].flag_table[i] & ~(LI_FLAG_MONITOR | LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT)))
    {
      li_config_table[i].channel |= LI_CHANNEL_INVOLVED;
    }
    if (li_config_table[i].flag_table[i] & LI_FLAG_CONFERENCE)
      li_config_table[i].channel |= LI_CHANNEL_CONFERENCE;
  }

  for (i = 0; i < li_total_channels; i++)
  {
    b = 0;
    for (j = 0; j < i; j++)
      b |= li_config_table[i].flag_table[j] | li_config_table[j].flag_table[i];
    for (j = i + 1; j < li_total_channels; j++)
      b |= li_config_table[i].flag_table[j] | li_config_table[j].flag_table[i];
    if (b != 0)
      li_config_table[i].channel |= LI_CHANNEL_INVOLVED;
    if (b & LI_FLAG_CONFERENCE)
      li_config_table[i].channel |= LI_CHANNEL_CONFERENCE;
  }
  for (i = 0; i < li_total_channels; i++)
  {
    for (j = 0; j < li_total_channels; j++)
      li_config_table[i].coef_table[j] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
  }

  for (i = 0; i < li_total_channels; i++)
  {
    if (li_config_table[i].channel & LI_CHANNEL_CONFERENCE)
    {
      for (j = 0; j < li_total_channels; j++)
      {
        if (li_config_table[i].flag_table[j] & LI_FLAG_CONFERENCE)
          li_config_table[i].coef_table[j] |= LI_COEF_CH_CH;
      }
    }
  }
  for (n = 0; n < li_total_channels; n++)
  {
    if (li_config_table[n].channel & LI_CHANNEL_CONFERENCE)
    {
      for (i = 0; i < li_total_channels; i++)
      {
        if (li_config_table[i].channel & LI_CHANNEL_CONFERENCE)
        {
          for (j = 0; j < li_total_channels; j++)
          {
            li_config_table[i].coef_table[j] |=
              li_config_table[i].coef_table[n] & li_config_table[n].coef_table[j];
          }
        }
      }
    }
  }
  for (i = 0; i < li_total_channels; i++)
  {
    if (li_config_table[i].channel & LI_CHANNEL_INVOLVED)
    {
      li_config_table[i].coef_table[i] &= ~LI_COEF_CH_CH;
      for (j = 0; j < li_total_channels; j++)
      {
        if (li_config_table[i].coef_table[j] & LI_COEF_CH_CH)
          li_config_table[i].flag_table[j] |= LI_FLAG_CONFERENCE;
      }
      if (li_config_table[i].flag_table[i] & LI_FLAG_CONFERENCE)
        li_config_table[i].coef_table[i] |= LI_COEF_CH_CH;
    }
  }
  for (i = 0; i < li_total_channels; i++)
  {
    if (li_config_table[i].channel & LI_CHANNEL_INVOLVED)
    {
      for (j = 0; j < li_total_channels; j++)
      {
        if (li_config_table[i].flag_table[j] & LI_FLAG_INTERCONNECT)
        {
          if (!(li_config_table[i].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_SEND_X))
           && !(li_config_table[j].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_RECEIVE_X)))
          {
            li_config_table[i].coef_table[j] |= LI_COEF_CH_CH;
          }
        }
        if (li_config_table[i].flag_table[j] & LI_FLAG_BCONNECT)
          li_config_table[i].coef_table[j] |= LI_COEF_CH_CH;
        if (li_config_table[i].flag_table[j] & LI_FLAG_MONITOR)
          li_config_table[i].coef_table[j] |= LI_COEF_CH_PC;
        if (li_config_table[i].flag_table[j] & LI_FLAG_MIX)
          li_config_table[i].coef_table[j] |= LI_COEF_PC_CH;
        if (li_config_table[i].flag_table[j] & LI_FLAG_PCCONNECT)
          li_config_table[i].coef_table[j] |= LI_COEF_PC_PC;
      }
      if (li_config_table[i].chflags & LI_CHFLAG_MONITOR)
      {
        for (j = 0; j < li_total_channels; j++)
        {
          if (li_config_table[i].flag_table[j] & LI_FLAG_INTERCONNECT)
          {
            if (!(li_config_table[j].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_RECEIVE_X)))
              li_config_table[i].coef_table[j] |= LI_COEF_CH_PC;
            if (li_config_table[j].chflags & LI_CHFLAG_MIX)
              li_config_table[i].coef_table[j] |= LI_COEF_PC_PC;
          }
        }
      }
      if (li_config_table[i].chflags & LI_CHFLAG_MIX)
      {
        for (j = 0; j < li_total_channels; j++)
        {
          if (li_config_table[j].flag_table[i] & LI_FLAG_INTERCONNECT)
          {
            if (!(li_config_table[j].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_SEND_X)))
              li_config_table[j].coef_table[i] |= LI_COEF_PC_CH;
          }
        }
      }
      if (li_config_table[i].chflags & LI_CHFLAG_LOOP)
      {
        for (j = 0; j < li_total_channels; j++)
        {
          if (li_config_table[i].flag_table[j] & LI_FLAG_INTERCONNECT)
          {
            for (n = 0; n < li_total_channels; n++)
            {
              if (li_config_table[n].flag_table[i] & LI_FLAG_INTERCONNECT)
              {
                if (!(li_config_table[n].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_SEND_X))
                 && !(li_config_table[j].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_RECEIVE_X)))
                {
                  li_config_table[n].coef_table[j] |= LI_COEF_CH_CH;
                }
                if (li_config_table[j].chflags & LI_CHFLAG_MIX)
                {
                  if (!(li_config_table[n].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_SEND_X)))
                    li_config_table[n].coef_table[j] |= LI_COEF_PC_CH;
                  if (li_config_table[n].chflags & LI_CHFLAG_MONITOR)
                    li_config_table[n].coef_table[j] |= LI_COEF_PC_PC;
                }
                if (li_config_table[n].chflags & LI_CHFLAG_MONITOR)
                {
                  if (!(li_config_table[j].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE | LI_CHFLAG_NOT_RECEIVE_X)))
                    li_config_table[n].coef_table[j] |= LI_COEF_CH_PC;
                }
              }
            }
          }
        }
      }
    }
  }
  for (i = 0; i < li_total_channels; i++)
  {
    if (li_config_table[i].channel & LI_CHANNEL_INVOLVED)
    {
      if (li_config_table[i].chflags & LI_CHFLAG_MONITOR)
        li_config_table[i].channel |= LI_CHANNEL_RX_DATA | LI_CHANNEL_USED_SOURCE_B;
      if (li_config_table[i].chflags & LI_CHFLAG_MIX)
        li_config_table[i].channel |= LI_CHANNEL_TX_DATA | LI_CHANNEL_USED_SOURCE_PC;
      for (j = 0; j < li_total_channels; j++)
      {
        if (li_config_table[i].flag_table[j] & (LI_FLAG_PCCONNECT | LI_FLAG_MONITOR))
          li_config_table[i].channel |= LI_CHANNEL_RX_DATA;
      }
      for (j = 0; j < i; j++)
      {
        if (li_config_table[j].flag_table[i] & (LI_FLAG_PCCONNECT | LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX))
          li_config_table[i].channel |= LI_CHANNEL_TX_DATA | LI_CHANNEL_USED_SOURCE_PC;
        if (li_config_table[j].flag_table[i] & (LI_FLAG_INTERCONNECT | LI_FLAG_MONITOR | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT))
          li_config_table[i].channel |= LI_CHANNEL_USED_SOURCE_B;
      }
      for (j = i + 1; j < li_total_channels; j++)
      {
        if (li_config_table[j].flag_table[i] & (LI_FLAG_PCCONNECT | LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX))
          li_config_table[i].channel |= LI_CHANNEL_TX_DATA | LI_CHANNEL_USED_SOURCE_PC;
        if (li_config_table[j].flag_table[i] & (LI_FLAG_INTERCONNECT | LI_FLAG_MONITOR | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT))
          li_config_table[i].channel |= LI_CHANNEL_USED_SOURCE_B;
      }
      if (li_config_table[i].flag_table[i] & (LI_FLAG_PCCONNECT | LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX))
        li_config_table[i].channel |= LI_CHANNEL_TX_DATA;
    }
  }
  for (i = 0; i < li_total_channels; i++)
  {
    if (li_config_table[i].channel & LI_CHANNEL_INVOLVED)
    {
      j = 0;
      while ((j < li_total_channels) && !(li_config_table[i].flag_table[j] & LI_FLAG_ANNOUNCEMENT))
        j++;
      if (j < li_total_channels)
      {
        for (j = 0; j < li_total_channels; j++)
        {
          li_config_table[i].coef_table[j] &= ~(LI_COEF_CH_CH | LI_COEF_PC_CH);
          if (li_config_table[i].flag_table[j] & LI_FLAG_ANNOUNCEMENT)
            li_config_table[i].coef_table[j] |= LI_COEF_PC_CH;
        }
      }
    }
  }
  mixer_dump_channels (a);
  for (i = 0; i < li_total_channels; i++)
  {
    if (!(li_config_table[i].flag_table[i] & LI_FLAG_MONITOR)
     || !(li_config_table[i].flag_table[i] & (LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT)))
    {
      li_config_table[i].channel |= LI_CHANNEL_INVOLVED;
    }
  }
}


static struct
{
  byte mask;
  byte line_flags;
} mixer_write_prog_pri[] =
{
  { LI_COEF_CH_CH, 0 },
  { LI_COEF_CH_PC, MIXER_COEF_LINE_TO_PC_FLAG },
  { LI_COEF_PC_CH, MIXER_COEF_LINE_FROM_PC_FLAG },
  { LI_COEF_PC_PC, MIXER_COEF_LINE_TO_PC_FLAG | MIXER_COEF_LINE_FROM_PC_FLAG }
};

static struct
{
  byte from_ch;
  byte to_ch;
  byte mask;
  byte xconnect_override;
} mixer_write_prog_bri[] =
{
  { 0, 0, LI_COEF_CH_CH, 0x01 },  /* B      to B      */
  { 1, 0, LI_COEF_CH_CH, 0x01 },  /* Alt B  to B      */
  { 0, 0, LI_COEF_PC_CH, 0x80 },  /* PC     to B      */
  { 1, 0, LI_COEF_PC_CH, 0x01 },  /* Alt PC to B      */
  { 2, 0, LI_COEF_CH_CH, 0x00 },  /* IC     to B      */
  { 3, 0, LI_COEF_CH_CH, 0x00 },  /* Alt IC to B      */
  { 0, 0, LI_COEF_CH_PC, 0x80 },  /* B      to PC     */
  { 1, 0, LI_COEF_CH_PC, 0x01 },  /* Alt B  to PC     */
  { 0, 0, LI_COEF_PC_PC, 0x01 },  /* PC     to PC     */
  { 1, 0, LI_COEF_PC_PC, 0x01 },  /* Alt PC to PC     */
  { 2, 0, LI_COEF_CH_PC, 0x00 },  /* IC     to PC     */
  { 3, 0, LI_COEF_CH_PC, 0x00 },  /* Alt IC to PC     */
  { 0, 2, LI_COEF_CH_CH, 0x00 },  /* B      to IC     */
  { 1, 2, LI_COEF_CH_CH, 0x00 },  /* Alt B  to IC     */
  { 0, 2, LI_COEF_PC_CH, 0x00 },  /* PC     to IC     */
  { 1, 2, LI_COEF_PC_CH, 0x00 },  /* Alt PC to IC     */
  { 2, 2, LI_COEF_CH_CH, 0x00 },  /* IC     to IC     */
  { 3, 2, LI_COEF_CH_CH, 0x00 },  /* Alt IC to IC     */
  { 1, 1, LI_COEF_CH_CH, 0x01 },  /* Alt B  to Alt B  */
  { 0, 1, LI_COEF_CH_CH, 0x01 },  /* B      to Alt B  */
  { 1, 1, LI_COEF_PC_CH, 0x80 },  /* Alt PC to Alt B  */
  { 0, 1, LI_COEF_PC_CH, 0x01 },  /* PC     to Alt B  */
  { 3, 1, LI_COEF_CH_CH, 0x00 },  /* Alt IC to Alt B  */
  { 2, 1, LI_COEF_CH_CH, 0x00 },  /* IC     to Alt B  */
  { 1, 1, LI_COEF_CH_PC, 0x80 },  /* Alt B  to Alt PC */
  { 0, 1, LI_COEF_CH_PC, 0x01 },  /* B      to Alt PC */
  { 1, 1, LI_COEF_PC_PC, 0x01 },  /* Alt PC to Alt PC */
  { 0, 1, LI_COEF_PC_PC, 0x01 },  /* PC     to Alt PC */
  { 3, 1, LI_COEF_CH_PC, 0x00 },  /* Alt IC to Alt PC */
  { 2, 1, LI_COEF_CH_PC, 0x00 },  /* IC     to Alt PC */
  { 1, 3, LI_COEF_CH_CH, 0x00 },  /* Alt B  to Alt IC */
  { 0, 3, LI_COEF_CH_CH, 0x00 },  /* B      to Alt IC */
  { 1, 3, LI_COEF_PC_CH, 0x00 },  /* Alt PC to Alt IC */
  { 0, 3, LI_COEF_PC_CH, 0x00 },  /* PC     to Alt IC */
  { 3, 3, LI_COEF_CH_CH, 0x00 },  /* Alt IC to Alt IC */
  { 2, 3, LI_COEF_CH_CH, 0x00 }   /* IC     to Alt IC */
};

static byte mixer_swapped_index_bri[] =
{
  18,  /* B      to B      */
  19,  /* Alt B  to B      */
  20,  /* PC     to B      */
  21,  /* Alt PC to B      */
  22,  /* IC     to B      */
  23,  /* Alt IC to B      */
  24,  /* B      to PC     */
  25,  /* Alt B  to PC     */
  26,  /* PC     to PC     */
  27,  /* Alt PC to PC     */
  28,  /* IC     to PC     */
  29,  /* Alt IC to PC     */
  30,  /* B      to IC     */
  31,  /* Alt B  to IC     */
  32,  /* PC     to IC     */
  33,  /* Alt PC to IC     */
  34,  /* IC     to IC     */
  35,  /* Alt IC to IC     */
  0,   /* Alt B  to Alt B  */
  1,   /* B      to Alt B  */
  2,   /* Alt PC to Alt B  */
  3,   /* PC     to Alt B  */
  4,   /* Alt IC to Alt B  */
  5,   /* IC     to Alt B  */
  6,   /* Alt B  to Alt PC */
  7,   /* B      to Alt PC */
  8,   /* Alt PC to Alt PC */
  9,   /* PC     to Alt PC */
  10,  /* Alt IC to Alt PC */
  11,  /* IC     to Alt PC */
  12,  /* Alt B  to Alt IC */
  13,  /* B      to Alt IC */
  14,  /* Alt PC to Alt IC */
  15,  /* PC     to Alt IC */
  16,  /* Alt IC to Alt IC */
  17   /* IC     to Alt IC */
};

static struct
{
  byte mask;
  byte from_pc;
  byte to_pc;
} xconnect_write_prog[] =
{
  { LI_COEF_CH_CH, FALSE, FALSE },
  { LI_COEF_CH_PC, FALSE, TRUE },
  { LI_COEF_PC_CH, TRUE, FALSE },
  { LI_COEF_PC_PC, TRUE, TRUE }
};


static void xconnect_query_addresses (PLCI   *plci)
{
  DIVA_CAPI_ADAPTER   *a;
  word w;
  byte   *p;

  dbug (1, dprintf ("[%06lx] %s,%d: xconnect_query_addresses",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  a = plci->adapter;
  p = plci->internal_req_buffer;
  *(p++) = UDATA_REQUEST_XCONNECT_FROM;
  if (!a->li_pri)
  {
    *(p++) = (byte) 2;
    *(p++) = (byte)(2 >> 8);
    *(p++) = (byte)(XCONNECT_CHANNEL_PORT_PC | 2);
    *(p++) = (byte)((XCONNECT_CHANNEL_PORT_PC | 2) >> 8);
  }
  *(p++) = (byte) 0;
  *(p++) = (byte)(0 >> 8);
  *(p++) = (byte) XCONNECT_CHANNEL_PORT_PC;
  *(p++) = (byte)(XCONNECT_CHANNEL_PORT_PC >> 8);
  if (a->li_pri && (plci->li_bchannel_id != 0))
  {
    w = plci->li_bchannel_id - 1;
    *(p++) = (byte) w;
    *(p++) = (byte)(w >> 8);
    w = (plci->li_bchannel_id - 1) | XCONNECT_CHANNEL_PORT_PC;
    *(p++) = (byte) w;
    *(p++) = (byte)(w >> 8);
  }
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NData[0].PLength = (word)(p - plci->internal_req_buffer);
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void xconnect_write_coefs (PLCI   *plci, word internal_command, byte needs_sync)
{

  dbug (1, dprintf ("[%06lx] %s,%d: xconnect_write_coefs %04x %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, internal_command, needs_sync));

  plci->li_write_command = internal_command;
  plci->li_write_channel = 0;
  plci->li_write_needs_sync = needs_sync;
  plci->li_write_is_sync = FALSE;
}


static byte xconnect_write_coefs_process (dword Id, PLCI   *plci, byte Rc)
{
  DIVA_CAPI_ADAPTER   *a;
  word w, n, i, j, r, s, to_ch;
  dword d;
  byte   *p;
  struct xconnect_transfer_address_s   *transfer_address;
  byte ch_map[MIXER_CHANNELS_BRI];
  byte source_info;

  dbug (1, dprintf ("[%06lx] %s,%d: xconnect_write_coefs_process %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->li_write_channel));

  a = plci->adapter;
  if ((plci->li_bchannel_id == 0)
   && (!(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    return (TRUE);
  }
  i = a->li_base + (plci->li_bchannel_id - 1);
  j = plci->li_write_channel;
  p = plci->internal_req_buffer;
  if (j != 0)
  {
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: LI write coefs failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      return (FALSE);
    }
  }
  if (li_config_table[i].adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
  {
    r = 0;
    s = 0;
    if (j < li_total_channels)
    {
      if (li_config_table[i].channel & LI_CHANNEL_ADDRESSES_SET)
      {
        s = ((li_config_table[i].send_b.bus_address.low | li_config_table[i].send_b.bus_address.high | li_config_table[i].send_b.card_id) ?
            (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_CH_PC | LI_COEF_PC_PC)) &
          ((li_config_table[i].send_pc.bus_address.low | li_config_table[i].send_pc.bus_address.high | li_config_table[i].send_pc.card_id) ?
            (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_CH_CH | LI_COEF_PC_CH));
      }
      r = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
      while ((j < li_total_channels)
        && ((r == 0)
         || (!(li_config_table[j].channel & LI_CHANNEL_ADDRESSES_SET))
         || (!li_config_table[j].adapter->li_pri
          && (j >= li_config_table[j].adapter->li_base + MIXER_BCHANNELS_BRI)
          && (j < li_config_table[j].adapter->li_base + MIXER_CHANNELS_BRI))
         || ((li_config_table[j].send_b.card_id != li_config_table[i].send_b.card_id)
          && (!(a->manufacturer_features & MANUFACTURER_FEATURE_DMACONNECT)
           || !(li_config_table[j].adapter->manufacturer_features & MANUFACTURER_FEATURE_DMACONNECT)))
         || ((li_config_table[j].adapter->li_base != a->li_base)
          && !(r & s &
            ((li_config_table[j].send_b.bus_address.low | li_config_table[j].send_b.bus_address.high | li_config_table[j].send_b.card_id) ?
              (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_PC_CH | LI_COEF_PC_PC)) &
            ((li_config_table[j].send_pc.bus_address.low | li_config_table[j].send_pc.bus_address.high | li_config_table[j].send_pc.card_id) ?
              (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_CH_CH | LI_COEF_CH_PC))))))
      {
        j++;
        if (j < li_total_channels)
          r = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
      }
    }
    if (j < li_total_channels)
    {
      plci->internal_command = plci->li_write_command;
      if (plci_nl_busy (plci))
        return (TRUE);
      if (a->li_pri)
        to_ch = plci->li_bchannel_id - 1;
      else
        to_ch = a->li_query_channel_supported ? plci->li_bchannel_id + 2 : 0;
      *(p++) = UDATA_REQUEST_XCONNECT_TO;
      do
      {
        if (li_config_table[j].adapter->li_base != a->li_base)
        {
          r &= s &
            ((li_config_table[j].send_b.bus_address.low | li_config_table[j].send_b.bus_address.high | li_config_table[j].send_b.card_id) ?
              (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_PC_CH | LI_COEF_PC_PC)) &
            ((li_config_table[j].send_pc.bus_address.low | li_config_table[j].send_pc.bus_address.high | li_config_table[j].send_pc.card_id) ?
              (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_CH_CH | LI_COEF_CH_PC));
        }
        n = 0;
        do
        {
          if (r & xconnect_write_prog[n].mask)
          {
            if (xconnect_write_prog[n].from_pc)
            {
              transfer_address = &(li_config_table[j].send_pc);
              source_info = li_config_table[j].source_info_pc;
            }
            else
            {
              transfer_address = &(li_config_table[j].send_b);
              source_info = li_config_table[j].source_info_b;
            }
            d = transfer_address->card_id;
            *(p++) = (byte) d;
            *(p++) = (byte)(d >> 8);
            *(p++) = (byte)(d >> 16);
            *(p++) = (byte)(d >> 24);
            d = transfer_address->bus_address.high;
            *(p++) = (byte) d;
            *(p++) = (byte)(d >> 8);
            *(p++) = (byte)(d >> 16);
            *(p++) = (byte)(d >> 24);
            d = transfer_address->bus_address.low;
            *(p++) = (byte) d;
            *(p++) = (byte)(d >> 8);
            *(p++) = (byte)(d >> 16);
            *(p++) = (byte)(d >> 24);
            w = xconnect_write_prog[n].to_pc ? to_ch | XCONNECT_CHANNEL_PORT_PC : to_ch;
            *(p++) = (byte) w;
            *(p++) = (byte)(w >> 8);
            w = ((li_config_table[i].coef_table[j] & xconnect_write_prog[n].mask) == 0) ? 0x01 :
              (li_config_table[i].adapter->u_law ?
                 (li_config_table[j].adapter->u_law ? 0x80 : 0x86) :
                 (li_config_table[j].adapter->u_law ? 0x7a : 0x80));
            *(p++) = (byte) w;
            *(p++) = (byte) source_info;
            li_config_table[i].coef_table[j] ^= xconnect_write_prog[n].mask << 4;
          }
          n++;
        } while ((n < sizeof(xconnect_write_prog) / sizeof(xconnect_write_prog[0]))
          && ((p - plci->internal_req_buffer) + 16 < INTERNAL_REQ_BUFFER_SIZE));
        if (n == sizeof(xconnect_write_prog) / sizeof(xconnect_write_prog[0]))
        {
          do
          {
            j++;
            if (j < li_total_channels)
              r = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
          } while ((j < li_total_channels)
            && ((r == 0)
             || (!(li_config_table[j].channel & LI_CHANNEL_ADDRESSES_SET))
             || (!li_config_table[j].adapter->li_pri
              && (j >= li_config_table[j].adapter->li_base + MIXER_BCHANNELS_BRI)
              && (j < li_config_table[j].adapter->li_base + MIXER_CHANNELS_BRI))
             || ((li_config_table[j].send_b.card_id != li_config_table[i].send_b.card_id)
              && (!(a->manufacturer_features & MANUFACTURER_FEATURE_DMACONNECT)
               || !(li_config_table[j].adapter->manufacturer_features & MANUFACTURER_FEATURE_DMACONNECT)))
             || ((li_config_table[j].adapter->li_base != a->li_base)
              && !(r & s &
                ((li_config_table[j].send_b.bus_address.low | li_config_table[j].send_b.bus_address.high | li_config_table[j].send_b.card_id) ?
                  (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_PC_CH | LI_COEF_PC_PC)) &
                ((li_config_table[j].send_pc.bus_address.low | li_config_table[j].send_pc.bus_address.high | li_config_table[j].send_pc.card_id) ?
                  (LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC) : (LI_COEF_CH_CH | LI_COEF_CH_PC))))));
        }
      } while ((j < li_total_channels)
        && ((p - plci->internal_req_buffer) + 16 < INTERNAL_REQ_BUFFER_SIZE));
    }
    else if (j == li_total_channels)
    {
      w = ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION) ?
        0 : MIXER_FEATURE_ENABLE_TX_DATA | MIXER_FEATURE_ENABLE_RX_DATA;
      if (a->li_pri)
      {
        if (plci->li_write_needs_sync
         || ((li_config_table[i].channel ^ li_config_table[i].curchnl) &
             (LI_CHANNEL_TX_DATA | LI_CHANNEL_RX_DATA |
              LI_CHANNEL_USED_SOURCE_PC | LI_CHANNEL_USED_SOURCE_B)))
        {
          plci->internal_command = plci->li_write_command;
          if (plci_nl_busy (plci))
            return (TRUE);
          plci->li_write_is_sync = TRUE;
          *(p++) = UDATA_REQUEST_SET_MIXER_COEFS_PRI_SYNC;
          if (li_config_table[i].channel & LI_CHANNEL_TX_DATA)
            w |= MIXER_FEATURE_ENABLE_TX_DATA;
          if (li_config_table[i].channel & LI_CHANNEL_RX_DATA)
            w |= MIXER_FEATURE_ENABLE_RX_DATA;
          if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_PC))
            w |= MIXER_FEATURE_UNUSED_SOURCE_PC;
          if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_B))
            w |= MIXER_FEATURE_UNUSED_SOURCE_B;
          *(p++) = (byte) w;
          *(p++) = (byte)(w >> 8);
        }
      }
      else
      {
        plci->internal_command = plci->li_write_command;
        if (plci_nl_busy (plci))
          return (TRUE);
        plci->li_write_is_sync = TRUE;
        *(p++) = UDATA_REQUEST_SET_MIXER_COEFS_BRI;
        if ((plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI)
         && (ADV_VOICE_NEW_COEF_BASE + sizeof(word) <= a->adv_voice_coef_length))
        {
          w |= READ_WORD (a->adv_voice_coef_buffer + ADV_VOICE_NEW_COEF_BASE);
        }
        if (li_config_table[i].channel & LI_CHANNEL_TX_DATA)
          w |= MIXER_FEATURE_ENABLE_TX_DATA;
        if (li_config_table[i].channel & LI_CHANNEL_RX_DATA)
          w |= MIXER_FEATURE_ENABLE_RX_DATA;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_PC))
          w |= MIXER_FEATURE_UNUSED_SOURCE_PC;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_B))
          w |= MIXER_FEATURE_UNUSED_SOURCE_B;
        *(p++) = (byte) w;
        *(p++) = (byte)(w >> 8);
        if (plci->li_bchannel_id <= MIXER_CHANNELS_BRI)
        {
          for (j = 0; j < sizeof(ch_map); j += 2)
          {
            if (plci->li_bchannel_id == 2)
            {
              ch_map[j] = (byte)(j+1);
              ch_map[j+1] = (byte) j;
            }
            else
            {
              ch_map[j] = (byte) j;
              ch_map[j+1] = (byte)(j+1);
            }
          }
          for (n = 0; n < sizeof(mixer_write_prog_bri) / sizeof(mixer_write_prog_bri[0]); n++)
          {
            i = a->li_base + ch_map[mixer_write_prog_bri[n].to_ch];
            j = a->li_base + ch_map[mixer_write_prog_bri[n].from_ch];
            if (li_config_table[i].channel & li_config_table[j].channel & LI_CHANNEL_INVOLVED)
            {
              *p = (mixer_write_prog_bri[n].xconnect_override != 0) ?
                mixer_write_prog_bri[n].xconnect_override :
                ((li_config_table[i].coef_table[j] & mixer_write_prog_bri[n].mask) ? 0x80 : 0x01);
              if ((i >= a->li_base + MIXER_BCHANNELS_BRI) || (j >= a->li_base + MIXER_BCHANNELS_BRI))
              {
                w = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
                li_config_table[i].coef_table[j] ^= (w & mixer_write_prog_bri[n].mask) << 4;
              }
            }
            else
            {
              *p = 0x00;
              if ((a->AdvSignalPLCI != NULL) && (a->AdvSignalPLCI->tel == ADV_VOICE))
              {
                w = (plci == a->AdvSignalPLCI) ? n : mixer_swapped_index_bri[n];
                if (ADV_VOICE_NEW_COEF_BASE + sizeof(word) + w < a->adv_voice_coef_length)
                  *p = a->adv_voice_coef_buffer[ADV_VOICE_NEW_COEF_BASE + sizeof(word) + w];
              }
            }
            p++;
          }
        }
      }
      j = li_total_channels + 1;
    }
  }
  else
  {
    if (j <= li_total_channels)
    {
      plci->internal_command = plci->li_write_command;
      if (plci_nl_busy (plci))
        return (TRUE);
      if (j < a->li_base)
        j = a->li_base;
      w = ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION) ?
        0 : MIXER_FEATURE_ENABLE_TX_DATA | MIXER_FEATURE_ENABLE_RX_DATA;
      if (a->li_pri)
      {
        if (plci->li_write_needs_sync
         || ((li_config_table[i].channel ^ li_config_table[i].curchnl) &
             (LI_CHANNEL_TX_DATA | LI_CHANNEL_RX_DATA |
              LI_CHANNEL_USED_SOURCE_PC | LI_CHANNEL_USED_SOURCE_B)))
        {
          plci->li_write_is_sync = TRUE;
          *(p++) = UDATA_REQUEST_SET_MIXER_COEFS_PRI_SYNC;
        }
        else
        {
          *(p++) = UDATA_REQUEST_SET_MIXER_COEFS_PRI_ASYN;
        }
        if (li_config_table[i].channel & LI_CHANNEL_TX_DATA)
          w |= MIXER_FEATURE_ENABLE_TX_DATA;
        if (li_config_table[i].channel & LI_CHANNEL_RX_DATA)
          w |= MIXER_FEATURE_ENABLE_RX_DATA;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_PC))
          w |= MIXER_FEATURE_UNUSED_SOURCE_PC;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_B))
          w |= MIXER_FEATURE_UNUSED_SOURCE_B;
        *(p++) = (byte) w;
        *(p++) = (byte)(w >> 8);
        for (n = 0; n < sizeof(mixer_write_prog_pri) / sizeof(mixer_write_prog_pri[0]); n++)
        {
          *(p++) = (byte)((plci->li_bchannel_id - 1) | mixer_write_prog_pri[n].line_flags);
          for (j = a->li_base; j < a->li_base + MIXER_CHANNELS_PRI; j++)
          {
            w = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
            if (w & mixer_write_prog_pri[n].mask)
            {
              *(p++) = (li_config_table[i].coef_table[j] & mixer_write_prog_pri[n].mask) ? 0x80 : 0x01;
              li_config_table[i].coef_table[j] ^= mixer_write_prog_pri[n].mask << 4;
            }
            else
              *(p++) = 0x00;
          }
          *(p++) = (byte)((plci->li_bchannel_id - 1) | MIXER_COEF_LINE_ROW_FLAG | mixer_write_prog_pri[n].line_flags);
          for (j = a->li_base; j < a->li_base + MIXER_CHANNELS_PRI; j++)
          {
            w = ((li_config_table[j].coef_table[i] & 0xf) ^ (li_config_table[j].coef_table[i] >> 4));
            if (w & mixer_write_prog_pri[n].mask)
            {
              *(p++) = (li_config_table[j].coef_table[i] & mixer_write_prog_pri[n].mask) ? 0x80 : 0x01;
              li_config_table[j].coef_table[i] ^= mixer_write_prog_pri[n].mask << 4;
            }
            else
              *(p++) = 0x00;
          }
        }
      }
      else
      {
        plci->li_write_is_sync = TRUE;
        *(p++) = UDATA_REQUEST_SET_MIXER_COEFS_BRI;
        if ((plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI)
         && (ADV_VOICE_NEW_COEF_BASE + sizeof(word) <= a->adv_voice_coef_length))
        {
          w |= READ_WORD (a->adv_voice_coef_buffer + ADV_VOICE_NEW_COEF_BASE);
        }
        if (li_config_table[i].channel & LI_CHANNEL_TX_DATA)
          w |= MIXER_FEATURE_ENABLE_TX_DATA;
        if (li_config_table[i].channel & LI_CHANNEL_RX_DATA)
          w |= MIXER_FEATURE_ENABLE_RX_DATA;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_PC))
          w |= MIXER_FEATURE_UNUSED_SOURCE_PC;
        if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_B))
          w |= MIXER_FEATURE_UNUSED_SOURCE_B;
        *(p++) = (byte) w;
        *(p++) = (byte)(w >> 8);
        if (plci->li_bchannel_id <= MIXER_CHANNELS_BRI)
        {
          for (j = 0; j < sizeof(ch_map); j += 2)
          {
            if (plci->li_bchannel_id == 2)
            {
              ch_map[j] = (byte)(j+1);
              ch_map[j+1] = (byte) j;
            }
            else
            {
              ch_map[j] = (byte) j;
              ch_map[j+1] = (byte)(j+1);
            }
          }
          for (n = 0; n < sizeof(mixer_write_prog_bri) / sizeof(mixer_write_prog_bri[0]); n++)
          {
            i = a->li_base + ch_map[mixer_write_prog_bri[n].to_ch];
            j = a->li_base + ch_map[mixer_write_prog_bri[n].from_ch];
            if (li_config_table[i].channel & li_config_table[j].channel & LI_CHANNEL_INVOLVED)
            {
              *p = ((li_config_table[i].coef_table[j] & mixer_write_prog_bri[n].mask) ? 0x80 : 0x01);
              w = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
              li_config_table[i].coef_table[j] ^= (w & mixer_write_prog_bri[n].mask) << 4;
            }
            else
            {
              *p = 0x00;
              if ((a->AdvSignalPLCI != NULL) && (a->AdvSignalPLCI->tel == ADV_VOICE))
              {
                w = (plci == a->AdvSignalPLCI) ? n : mixer_swapped_index_bri[n];
                if (ADV_VOICE_NEW_COEF_BASE + sizeof(word) + w < a->adv_voice_coef_length)
                  *p = a->adv_voice_coef_buffer[ADV_VOICE_NEW_COEF_BASE + sizeof(word) + w];
              }
            }
            p++;
          }
        }
      }
      j = li_total_channels + 1;
    }
  }
  plci->li_write_channel = j;
  if (p != plci->internal_req_buffer)
  {
    plci->NData[0].P = plci->internal_req_buffer;
    plci->NData[0].PLength = (word)(p - plci->internal_req_buffer);
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_UDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
  }
  return (TRUE);
}


static void mixer_notify_update (PLCI   *plci, byte others)
{
  DIVA_CAPI_ADAPTER   *a;
  word n, i, w;
  PLCI   *notify_plci;
  static byte msg[sizeof(CAPI_MSG_HEADER) + 6];

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_notify_update %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, others));

  a = NULL;
  n = 0;
  if (others)
  {
    while ((n < max_adapter)
      && (!adapter[n].request || !(adapter[n].profile.Global_Options & GL_LINE_INTERCONNECT_SUPPORTED)))
    {
      n++;
    }
    a = &(adapter[n]);
  }
  i = 0;
  while (n < max_adapter)
  {
    notify_plci = NULL;
    if (others)
    {
      while ((i < a->max_plci)
        && (!(a->plci[i].li_channel_bits & LI_CHANNEL_INVOLVED) || (&(a->plci[i]) == plci)))
      {
        i++;
      }
      if (i < a->max_plci)
        notify_plci = &(a->plci[i++]);
      else
      {
        do
        {
          n++;
        } while ((n < max_adapter)
          && (!adapter[n].request || !(adapter[n].profile.Global_Options & GL_LINE_INTERCONNECT_SUPPORTED)));
        a = &(adapter[n]);
        i = 0;
      }
    }
    else
    {
      notify_plci = plci;
      n = max_adapter;
    }
    if ((notify_plci != NULL)
     && (notify_plci->li_notify_update == LI_NOTIFY_UPDATE_NONE)
     && (notify_plci->li_bchannel_id != 0)
     && (notify_plci->appl != NULL)
     && (notify_plci->State != IDLE))
    {
      notify_plci->li_notify_update = LI_NOTIFY_UPDATE_QUEUED;
      ((CAPI_MSG *) msg)->header.length = 18;
      ((CAPI_MSG *) msg)->header.appl_id = notify_plci->appl->Id;
      ((CAPI_MSG *) msg)->header.command = _FACILITY_R;
      ((CAPI_MSG *) msg)->header.number = 0;
      ((CAPI_MSG *) msg)->header.controller = notify_plci->adapter->Id;
      ((CAPI_MSG *) msg)->header.plci = notify_plci->Id;
      ((CAPI_MSG *) msg)->header.ncci = 0;
      ((CAPI_MSG *) msg)->info.facility_req.Selector = SELECTOR_LINE_INTERCONNECT;
      ((CAPI_MSG *) msg)->info.facility_req.structs[0] = 3;
      WRITE_WORD (&(((CAPI_MSG *) msg)->info.facility_req.structs[1]), LI_REQ_SILENT_UPDATE);
      ((CAPI_MSG *) msg)->info.facility_req.structs[3] = 0;
      w = api_put_reenter (notify_plci->appl, (CAPI_MSG *) msg);
      if (w == _QUEUE_FULL)
        notify_plci->li_notify_update = LI_NOTIFY_UPDATE_ENQUEUE;
      else if (w != GOOD)
      {
        notify_plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
        dbug (1, dprintf ("[%06lx] %s,%d: Interconnect notify failed %06lx %d",
          (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
          (char   *)(FILE_), __LINE__,
          (unsigned long)((notify_plci->Id << 8) | UnMapController (notify_plci->adapter->Id)), w));
      }
    }
  }
}


static void mixer_wait_update (PLCI   *plci)
{
  dbug (1, dprintf ("[%06lx] %s,%d: mixer_wait_update",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  if (!plci->li_update_waiting)
  {
    plci->li_update_waiting = TRUE;
    plci->li_update_wait_next = li_update_wait_list;
    li_update_wait_list = plci;
  }
}


static void mixer_cancel_wait (PLCI   *plci)
{
  PLCI   *wait_plci;
  PLCI   *   *wait_append;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_cancel_wait",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  wait_append = &li_update_wait_list;
  wait_plci = *wait_append;
  while ((wait_plci != plci) && (wait_plci != NULL))
  {
    wait_append = &(wait_plci->li_update_wait_next);
    wait_plci = *wait_append;
  }
  if (wait_plci != NULL)
  {
    *wait_append = wait_plci->li_update_wait_next;
    wait_plci->li_update_waiting = FALSE;
  }
}


static void mixer_update_done (void)
{
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *wait_plci;
  PLCI   *wait_next;
  dword Id;

  dbug (1, dprintf ("%s,%d: mixer_update_done",
    (char   *)(FILE_), __LINE__));

  wait_plci = li_update_wait_list;
  li_update_wait_list = NULL;
  while (wait_plci != NULL)
  {
    wait_next = wait_plci->li_update_wait_next;
    wait_plci->li_update_waiting = FALSE;
/*
    dbug (1, dprintf ("%s,%d: mixer_update_done for %06lx %d %d %d %d %d",
      (char far *)(FILE_), __LINE__,
      (unsigned long)((wait_plci->Id << 8) | UnMapController (wait_plci->adapter->Id)),
      wait_plci->State, wait_plci->NL.Id, wait_plci->nl_remove_id, wait_plci->li_bchannel_id, wait_plci->internal_command));
*/
    a = wait_plci->adapter;
    if (a->request && (a->profile.Global_Options & GL_LINE_INTERCONNECT_SUPPORTED))
    {
      if (wait_plci->State
       && wait_plci->NL.Id && !wait_plci->nl_remove_id
       && (wait_plci->li_bchannel_id != 0))
      {
        if (wait_plci->internal_command_queue[0]
         && (wait_plci->internal_command == MIXER_COMMAND_6))
        {
          Id = (((word)(wait_plci->Id)) << 8) | a->Id;
          (*(wait_plci->internal_command_queue[0]))(Id, wait_plci, 0);
          if (!wait_plci->internal_command)
            next_internal_command (Id, wait_plci);
          send_req(wait_plci);
        }
      }
    }
    wait_plci = wait_next;
  }
}


static void mixer_clear_config (PLCI   *plci)
{
  DIVA_CAPI_ADAPTER   *a;
  word i, j;
  byte flag_a_b_mask, flag_b_a_mask, coef_a_b_mask, coef_b_a_mask;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  a = plci->adapter;
  if ((plci->li_bchannel_id != 0)
   && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
   && (!(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    i = a->li_base + (plci->li_bchannel_id - 1);
    if ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
      li_config_table[i].chflags &= ~LI_CHFLAG_LINE_SIDE;
    if ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
      li_config_table[i].chflags &= LI_CHFLAG_LINE_SIDE;
    flag_a_b_mask = 0;
    flag_b_a_mask = 0;
    coef_a_b_mask = LI_COEF_CH_CH_SET | LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET | LI_COEF_PC_PC_SET;
    coef_b_a_mask = 0;
    if (li_config_table[i].chflags & LI_CHFLAG_NULL_PLCI)
    {
      flag_a_b_mask |= LI_FLAG_PCCONNECT | LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT;
      flag_b_a_mask |= LI_FLAG_PCCONNECT | LI_FLAG_MONITOR;
      coef_a_b_mask |= LI_COEF_PC_PC | LI_COEF_CH_PC;
      coef_b_a_mask |= LI_COEF_PC_PC | LI_COEF_PC_CH |
        LI_COEF_CH_CH_SET | LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET | LI_COEF_PC_PC_SET;
      li_config_table[i].channel &= ~LI_CHANNEL_CONFERENCE;
    }
    else if (li_config_table[i].chflags & LI_CHFLAG_LINE_SIDE)
    {
      flag_a_b_mask |= LI_FLAG_BCONNECT | LI_FLAG_MONITOR;
      flag_b_a_mask |= LI_FLAG_BCONNECT | LI_FLAG_MIX | LI_FLAG_ANNOUNCEMENT;
      coef_a_b_mask |= LI_COEF_CH_CH | LI_COEF_PC_CH;
      coef_b_a_mask |= LI_COEF_CH_CH | LI_COEF_CH_PC |
        LI_COEF_CH_CH_SET | LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET | LI_COEF_PC_PC_SET;
      li_config_table[i].channel &= ~LI_CHANNEL_USED_SOURCE_PC;
    }
    else
    {
      li_config_table[i].curchnl = 0;
      li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
    }
    for (j = 0; j < li_total_channels; j++)
    {
      if ((li_config_table[i].flag_table[j] != 0)
       || (li_config_table[j].flag_table[i] != 0))
      {
        li_config_table[i].flag_table[j] &= flag_b_a_mask;
        li_config_table[j].flag_table[i] &= flag_a_b_mask;
        if ((li_config_table[i].flag_table[j] == 0)
         && (li_config_table[j].flag_table[i] == 0)
         && (j != i))
        {
          mixer_update_uninvolve (j);
        }
      }
      li_config_table[i].coef_table[j] &= coef_b_a_mask;
      li_config_table[j].coef_table[i] &= coef_a_b_mask;
    }
    if (!(li_config_table[i].chflags & (LI_CHFLAG_NULL_PLCI | LI_CHFLAG_LINE_SIDE)))
      li_config_table[i].flag_table[i] = LI_FLAG_MONITOR | LI_FLAG_MIX;
    if (!a->li_pri)
    {
      li_config_table[i].coef_table[i] |= LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET;
      if ((plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI))
      {
        i = a->li_base + MIXER_IC_CHANNEL_BASE + (plci->li_bchannel_id - 1);
        li_config_table[i].curchnl = 0;
        li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
        li_config_table[i].chflags = 0;
        for (j = 0; j < li_total_channels; j++)
        {
          li_config_table[i].flag_table[j] = 0;
          li_config_table[j].flag_table[i] = 0;
          li_config_table[i].coef_table[j] = 0;
          li_config_table[j].coef_table[i] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
        }
        if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
        {
          i = a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id);
          li_config_table[i].curchnl = 0;
          li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
          li_config_table[i].chflags = 0;
          for (j = 0; j < li_total_channels; j++)
          {
            li_config_table[i].flag_table[j] = 0;
            li_config_table[j].flag_table[i] = 0;
            li_config_table[i].coef_table[j] = 0;
            li_config_table[j].coef_table[i] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
          }
        }
      }
    }
  }
  plci->li_channel_bits &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
}


static void mixer_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  do
  {
    mixer_indication_coefs_set (Id, plci);
  } while (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos);
}


static word mixer_save_config (dword Id, PLCI   *plci, byte Rc)
{
  DIVA_CAPI_ADAPTER   *a;
  word i, j;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  a = plci->adapter;
  if ((plci->li_bchannel_id != 0)
   && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
   && (!(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    i = a->li_base + (plci->li_bchannel_id - 1);
    for (j = 0; j < li_total_channels; j++)
    {
      li_config_table[i].coef_table[j] &= LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC;
      li_config_table[j].coef_table[i] &= LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC;
    }
    if (!a->li_pri)
      li_config_table[i].coef_table[i] |= LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET;
  }
  return (GOOD);
}


static word mixer_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  DIVA_CAPI_ADAPTER   *a;
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  a = plci->adapter;
  if ((plci->B1_facilities & B1_FACILITY_MIXER)
   && (plci->li_bchannel_id != 0)
   && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
   && (!(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_MIXER_1:
      if (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
      {
        plci->internal_command = plci->adjust_b_command;
        if (plci_nl_busy (plci))
        {
          plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_1;
          break;
        }
        xconnect_query_addresses (plci);
        plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_2;
        break;
      }
      plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_5;
      Rc = OK;
    case ADJUST_B_RESTORE_MIXER_2:
    case ADJUST_B_RESTORE_MIXER_3:
    case ADJUST_B_RESTORE_MIXER_4:
      if ((Rc != OK) && (Rc != OK_FC) && (Rc != 0))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B query addresses failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (Rc == OK)
      {
        if (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_2)
          plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_3;
        else if (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_4)
          plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_5;
      }
      else if (Rc == 0)
      {
        if (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_2)
          plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_4;
        else if (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_3)
          plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_5;
      }
      if (plci->adjust_b_state != ADJUST_B_RESTORE_MIXER_5)
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
    case ADJUST_B_RESTORE_MIXER_5:
      xconnect_write_coefs (plci, plci->adjust_b_command, TRUE);
      plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_6;
      Rc = OK;
    case ADJUST_B_RESTORE_MIXER_6:
      if (!xconnect_write_coefs_process (Id, plci, Rc))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Write mixer coefs failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        break;
      plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_7;
    case ADJUST_B_RESTORE_MIXER_7:
      break;
    }
    if (Info == GOOD)
    {
      if (plci->li_cmd != LI_REQ_SILENT_UPDATE)
        mixer_notify_update (plci, TRUE);
    }
  }
  return (Info);
}


static void li_update_connect (dword Id, DIVA_CAPI_ADAPTER   *a, PLCI   *plci,
  dword plci_b_id, byte connect, dword li_flags)
{
  word i, ch_a, ch_a_v, ch_a_s, ch_b, ch_b_v, ch_b_s;
  PLCI   *plci_b;
  DIVA_CAPI_ADAPTER   *a_b;

/*
  dbug (1, dprintf ("[%06lx] %s,%d: li_update_connect %06lx %d %08lx",
    (unsigned long)(UnMapId (Id)), (char far *)(FILE_), __LINE__,
    (unsigned long)(UnMapId (plci_b_id)), connect, (unsigned long) li_flags));
*/

  a_b = &(adapter[MapController ((byte)(plci_b_id & 0x7f)) - 1]);
  plci_b = &(a_b->plci[((plci_b_id >> 8) & 0xff) - 1]);
  ch_a = a->li_base + (plci->li_bchannel_id - 1);
  if (!a->li_pri && (plci->tel == ADV_VOICE)
   && (plci == a->AdvSignalPLCI) && (Id & EXT_CONTROLLER))
  {
    ch_a_v = ch_a + MIXER_IC_CHANNEL_BASE;
    ch_a_s = (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC) ?
      a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id) : ch_a_v;
  }
  else
  {
    ch_a_v = ch_a;
    ch_a_s = ch_a;
  }
  ch_b = a_b->li_base + (plci_b->li_bchannel_id - 1);
  if (!a_b->li_pri && (plci_b->tel == ADV_VOICE)
   && (plci_b == a_b->AdvSignalPLCI) && (plci_b_id & EXT_CONTROLLER))
  {
    ch_b_v = ch_b + MIXER_IC_CHANNEL_BASE;
    ch_b_s = (a_b->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC) ?
      a_b->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci_b->li_bchannel_id) : ch_b_v;
  }
  else
  {
    ch_b_v = ch_b;
    ch_b_s = ch_b;
  }
  if (connect)
  {
    if ((ch_b != ch_a) && !(li_config_table[ch_b].channel & LI_CHANNEL_INVOLVED))
    {
      li_config_table[ch_b].flag_table[ch_b] = 0;
      li_config_table[ch_b].channel |= LI_CHANNEL_INVOLVED;
    }
    li_config_table[ch_a].flag_table[ch_a_v] &= ~LI_FLAG_MONITOR;
    li_config_table[ch_a].flag_table[ch_a_s] &= ~LI_FLAG_MONITOR;
    li_config_table[ch_a_v].flag_table[ch_a] &= ~(LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX);
    li_config_table[ch_a_s].flag_table[ch_a] &= ~(LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX);
  }
  li_config_table[ch_a].flag_table[ch_b_v] &= ~LI_FLAG_MONITOR;
  li_config_table[ch_a].flag_table[ch_b_s] &= ~LI_FLAG_MONITOR;
  li_config_table[ch_b_v].flag_table[ch_a] &= ~(LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX);
  li_config_table[ch_b_s].flag_table[ch_a] &= ~(LI_FLAG_ANNOUNCEMENT | LI_FLAG_MIX);
  if (ch_a_v == ch_b_v)
  {
    li_config_table[ch_a_v].flag_table[ch_b_v] &= ~LI_FLAG_CONFERENCE;
    li_config_table[ch_a_s].flag_table[ch_b_s] &= ~LI_FLAG_CONFERENCE;
  }
  else
  {
    if (li_config_table[ch_a_v].flag_table[ch_b_v] & LI_FLAG_CONFERENCE)
    {
      for (i = 0; i < li_total_channels; i++)
      {
        if (i != ch_a_v)
          li_config_table[ch_a_v].flag_table[i] &= ~LI_FLAG_CONFERENCE;
      }
    }
    if (li_config_table[ch_a_s].flag_table[ch_b_v] & LI_FLAG_CONFERENCE)
    {
      for (i = 0; i < li_total_channels; i++)
      {
        if (i != ch_a_s)
          li_config_table[ch_a_s].flag_table[i] &= ~LI_FLAG_CONFERENCE;
      }
    }
    if (li_config_table[ch_b_v].flag_table[ch_a_v] & LI_FLAG_CONFERENCE)
    {
      for (i = 0; i < li_total_channels; i++)
      {
        if (i != ch_a_v)
          li_config_table[i].flag_table[ch_a_v] &= ~LI_FLAG_CONFERENCE;
      }
    }
    if (li_config_table[ch_b_v].flag_table[ch_a_s] & LI_FLAG_CONFERENCE)
    {
      for (i = 0; i < li_total_channels; i++)
      {
        if (i != ch_a_s)
          li_config_table[i].flag_table[ch_a_s] &= ~LI_FLAG_CONFERENCE;
      }
    }
  }
  if (li_flags & LI_FLAG_CONFERENCE_A_B)
  {
    li_config_table[ch_b_v].flag_table[ch_a_v] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_b_s].flag_table[ch_a_v] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_b_v].flag_table[ch_a_s] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_b_s].flag_table[ch_a_s] |= LI_FLAG_CONFERENCE;
  }
  if (li_flags & LI_FLAG_CONFERENCE_B_A)
  {
    li_config_table[ch_a_v].flag_table[ch_b_v] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_a_v].flag_table[ch_b_s] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_a_s].flag_table[ch_b_v] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_a_s].flag_table[ch_b_s] |= LI_FLAG_CONFERENCE;
  }
  if (li_flags & LI_FLAG_MONITOR_A)
  {
    li_config_table[ch_a].flag_table[ch_a_v] |= LI_FLAG_MONITOR;
    li_config_table[ch_a].flag_table[ch_a_s] |= LI_FLAG_MONITOR;
  }
  if (li_flags & LI_FLAG_MONITOR_B)
  {
    li_config_table[ch_a].flag_table[ch_b_v] |= LI_FLAG_MONITOR;
    li_config_table[ch_a].flag_table[ch_b_s] |= LI_FLAG_MONITOR;
  }
  if (li_flags & LI_FLAG_ANNOUNCEMENT_A)
  {
    li_config_table[ch_a_v].flag_table[ch_a] |= LI_FLAG_ANNOUNCEMENT;
    li_config_table[ch_a_s].flag_table[ch_a] |= LI_FLAG_ANNOUNCEMENT;
  }
  if (li_flags & LI_FLAG_ANNOUNCEMENT_B)
  {
    li_config_table[ch_b_v].flag_table[ch_a] |= LI_FLAG_ANNOUNCEMENT;
    li_config_table[ch_b_s].flag_table[ch_a] |= LI_FLAG_ANNOUNCEMENT;
  }
  if (li_flags & LI_FLAG_MIX_A)
  {
    li_config_table[ch_a_v].flag_table[ch_a] |= LI_FLAG_MIX;
    li_config_table[ch_a_s].flag_table[ch_a] |= LI_FLAG_MIX;
  }
  if (li_flags & LI_FLAG_MIX_B)
  {
    li_config_table[ch_b_v].flag_table[ch_a] |= LI_FLAG_MIX;
    li_config_table[ch_b_s].flag_table[ch_a] |= LI_FLAG_MIX;
  }
  if (ch_a_v != ch_a_s)
  {
    li_config_table[ch_a_v].flag_table[ch_a_s] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_a_s].flag_table[ch_a_v] |= LI_FLAG_CONFERENCE;
  }
  if (ch_b_v != ch_b_s)
  {
    li_config_table[ch_b_v].flag_table[ch_b_s] |= LI_FLAG_CONFERENCE;
    li_config_table[ch_b_s].flag_table[ch_b_v] |= LI_FLAG_CONFERENCE;
  }
  if (!connect)
  {
    mixer_update_uninvolve (ch_a);
    mixer_update_uninvolve (ch_b);
  }
}


static void li2_update_connect (dword Id, DIVA_CAPI_ADAPTER   *a, PLCI   *plci,
  dword plci_b_id, byte connect, dword li_flags)
{
  word ch_a, ch_a_v, ch_a_s, ch_b, ch_b_v, ch_b_s;
  PLCI   *plci_b;
  DIVA_CAPI_ADAPTER   *a_b;

/*
  dbug (1, dprintf ("[%06lx] %s,%d: li2_update_connect %06lx %d %08lx",
    (unsigned long)(UnMapId (Id)), (char far *)(FILE_), __LINE__,
    (unsigned long)(UnMapId (plci_b_id)), connect, (unsigned long) li_flags));
*/

  a_b = &(adapter[MapController ((byte)(plci_b_id & 0x7f)) - 1]);
  plci_b = &(a_b->plci[((plci_b_id >> 8) & 0xff) - 1]);
  ch_a = a->li_base + (plci->li_bchannel_id - 1);
  if (!a->li_pri && (plci->tel == ADV_VOICE)
   && (plci == a->AdvSignalPLCI) && (Id & EXT_CONTROLLER))
  {
    ch_a_v = ch_a + MIXER_IC_CHANNEL_BASE;
    ch_a_s = (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC) ?
      a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id) : ch_a_v;
  }
  else
  {
    ch_a_v = ch_a;
    ch_a_s = ch_a;
  }
  ch_b = a_b->li_base + (plci_b->li_bchannel_id - 1);
  if (!a_b->li_pri && (plci_b->tel == ADV_VOICE)
   && (plci_b == a_b->AdvSignalPLCI) && (plci_b_id & EXT_CONTROLLER))
  {
    ch_b_v = ch_b + MIXER_IC_CHANNEL_BASE;
    ch_b_s = (a_b->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC) ?
      a_b->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci_b->li_bchannel_id) : ch_b_v;
  }
  else
  {
    ch_b_v = ch_b;
    ch_b_s = ch_b;
  }
  if (connect)
  {
    li_config_table[ch_b].flag_table[ch_b_v] &= ~LI_FLAG_MONITOR;
    li_config_table[ch_b].flag_table[ch_b_s] &= ~LI_FLAG_MONITOR;
    li_config_table[ch_b_v].flag_table[ch_b] &= ~LI_FLAG_MIX;
    li_config_table[ch_b_s].flag_table[ch_b] &= ~LI_FLAG_MIX;
    if ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
    {
      li_config_table[ch_b_v].flag_table[ch_b_v] &= ~LI_FLAG_BCONNECT;
      li_config_table[ch_b_s].flag_table[ch_b_v] &= ~LI_FLAG_BCONNECT;
      li_config_table[ch_b_v].flag_table[ch_b_s] &= ~LI_FLAG_BCONNECT;
      li_config_table[ch_b_s].flag_table[ch_b_s] &= ~LI_FLAG_BCONNECT;
    }
    if ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
    {
      li_config_table[ch_b].flag_table[ch_b] &= ~LI_FLAG_PCCONNECT;
      li_config_table[ch_b].chflags &= ~(LI_CHFLAG_MONITOR | LI_CHFLAG_MIX | LI_CHFLAG_LOOP |
        LI_CHFLAG_NOT_SEND_X | LI_CHFLAG_NOT_RECEIVE_X);
    }
    if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
     && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE))
    {
      li_config_table[ch_b].flag_table[ch_a_v] &= ~LI_FLAG_MONITOR;
      li_config_table[ch_b].flag_table[ch_a_s] &= ~LI_FLAG_MONITOR;
      li_config_table[ch_a_v].flag_table[ch_b] &= ~LI_FLAG_MIX;
      li_config_table[ch_a_s].flag_table[ch_b] &= ~LI_FLAG_MIX;
    }
    if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
     && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI))
    {
      li_config_table[ch_a].flag_table[ch_b_v] &= ~LI_FLAG_MONITOR;
      li_config_table[ch_a].flag_table[ch_b_s] &= ~LI_FLAG_MONITOR;
      li_config_table[ch_b_v].flag_table[ch_a] &= ~LI_FLAG_MIX;
      li_config_table[ch_b_s].flag_table[ch_a] &= ~LI_FLAG_MIX;
    }
    if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
     && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE))
    {
      li_config_table[ch_b].flag_table[ch_a] &= ~LI_FLAG_PCCONNECT;
      li_config_table[ch_a].flag_table[ch_b] &= ~LI_FLAG_PCCONNECT;
    }
  }
  li_config_table[ch_b_v].flag_table[ch_a_v] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_b_s].flag_table[ch_a_v] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_b_v].flag_table[ch_a_s] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_b_s].flag_table[ch_a_s] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_a_v].flag_table[ch_b_v] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_a_v].flag_table[ch_b_s] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_a_s].flag_table[ch_b_v] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  li_config_table[ch_a_s].flag_table[ch_b_s] &= ~(LI_FLAG_INTERCONNECT | LI_FLAG_CONFERENCE | LI_FLAG_BCONNECT);
  if (li_flags & LI2_FLAG_INTERCONNECT_A_B)
  {
    li_config_table[ch_b_v].flag_table[ch_a_v] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_b_s].flag_table[ch_a_v] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_b_v].flag_table[ch_a_s] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_b_s].flag_table[ch_a_s] |= LI_FLAG_INTERCONNECT;
  }
  if (li_flags & LI2_FLAG_INTERCONNECT_B_A)
  {
    li_config_table[ch_a_v].flag_table[ch_b_v] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_a_v].flag_table[ch_b_s] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_a_s].flag_table[ch_b_v] |= LI_FLAG_INTERCONNECT;
    li_config_table[ch_a_s].flag_table[ch_b_s] |= LI_FLAG_INTERCONNECT;
  }
  if ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
  {
    if (li_flags & LI2_FLAG_MONITOR_B)
    {
      li_config_table[ch_b].flag_table[ch_b_v] |= LI_FLAG_MONITOR;
      li_config_table[ch_b].flag_table[ch_b_s] |= LI_FLAG_MONITOR;
    }
    if (li_flags & LI2_FLAG_MIX_B)
    {
      li_config_table[ch_b_v].flag_table[ch_b] |= LI_FLAG_MIX;
      li_config_table[ch_b_s].flag_table[ch_b] |= LI_FLAG_MIX;
    }
  }
  if (li_flags & LI2_FLAG_MONITOR_X)
    li_config_table[ch_b].chflags |= LI_CHFLAG_MONITOR;
  if (li_flags & LI2_FLAG_MIX_X)
    li_config_table[ch_b].chflags |= LI_CHFLAG_MIX;
  if (((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
   && (li_flags & LI2_FLAG_LOOP_B))
  {
    li_config_table[ch_b_v].flag_table[ch_b_v] |= LI_FLAG_BCONNECT;
    li_config_table[ch_b_s].flag_table[ch_b_v] |= LI_FLAG_BCONNECT;
    li_config_table[ch_b_v].flag_table[ch_b_s] |= LI_FLAG_BCONNECT;
    li_config_table[ch_b_s].flag_table[ch_b_s] |= LI_FLAG_BCONNECT;
  }
  if (li_flags & LI2_FLAG_LOOP_PC)
    li_config_table[ch_b].flag_table[ch_b] |= LI_FLAG_PCCONNECT;
  if (li_flags & LI2_FLAG_LOOP_X)
    li_config_table[ch_b].chflags |= LI_CHFLAG_LOOP;
  if (li_flags & LI2_FLAG_NOT_SEND_X)
    li_config_table[ch_b].chflags |= LI_CHFLAG_NOT_SEND_X;
  if (li_flags & LI2_FLAG_NOT_RECEIVE_X)
    li_config_table[ch_b].chflags |= LI_CHFLAG_NOT_RECEIVE_X;
  if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
   && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI))
  {
    if (li_flags & LI2_FLAG_BCONNECT_A_B)
    {
      li_config_table[ch_b_v].flag_table[ch_a_v] |= LI_FLAG_BCONNECT;
      li_config_table[ch_b_s].flag_table[ch_a_v] |= LI_FLAG_BCONNECT;
      li_config_table[ch_b_v].flag_table[ch_a_s] |= LI_FLAG_BCONNECT;
      li_config_table[ch_b_s].flag_table[ch_a_s] |= LI_FLAG_BCONNECT;
    }
    if (li_flags & LI2_FLAG_BCONNECT_B_A)
    {
      li_config_table[ch_a_v].flag_table[ch_b_v] |= LI_FLAG_BCONNECT;
      li_config_table[ch_a_v].flag_table[ch_b_s] |= LI_FLAG_BCONNECT;
      li_config_table[ch_a_s].flag_table[ch_b_v] |= LI_FLAG_BCONNECT;
      li_config_table[ch_a_s].flag_table[ch_b_s] |= LI_FLAG_BCONNECT;
    }
  }
  if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
   && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE))
  {
    if (li_flags & LI2_FLAG_MONITOR_A_B)
    {
      li_config_table[ch_b].flag_table[ch_a_v] |= LI_FLAG_MONITOR;
      li_config_table[ch_b].flag_table[ch_a_s] |= LI_FLAG_MONITOR;
    }
    if (li_flags & LI2_FLAG_MIX_A_B)
    {
      li_config_table[ch_b_v].flag_table[ch_a] |= LI_FLAG_MIX;
      li_config_table[ch_b_s].flag_table[ch_a] |= LI_FLAG_MIX;
    }
  }
  if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
   && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI))
  {
    if (li_flags & LI2_FLAG_MONITOR_B_A)
    {
      li_config_table[ch_a].flag_table[ch_b_v] |= LI_FLAG_MONITOR;
      li_config_table[ch_a].flag_table[ch_b_s] |= LI_FLAG_MONITOR;
    }
    if (li_flags & LI2_FLAG_MIX_B_A)
    {
      li_config_table[ch_a_v].flag_table[ch_b] |= LI_FLAG_MIX;
      li_config_table[ch_a_s].flag_table[ch_b] |= LI_FLAG_MIX;
    }
  }
  if (((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE)
   && ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_LINE_SIDE))
  {
    if (li_flags & LI2_FLAG_PCCONNECT_A_B)
      li_config_table[ch_b].flag_table[ch_a] |= LI_FLAG_PCCONNECT;
    if (li_flags & LI2_FLAG_PCCONNECT_B_A)
      li_config_table[ch_a].flag_table[ch_b] |= LI_FLAG_PCCONNECT;
  }
  if ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
  {
    if (ch_a_v != ch_a_s)
    {
      li_config_table[ch_a_v].flag_table[ch_a_s] |= LI_FLAG_CONFERENCE;
      li_config_table[ch_a_s].flag_table[ch_a_v] |= LI_FLAG_CONFERENCE;
    }
  }
  if ((plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK) != DSP_CAI_ARRANGEMENT_NULL_PLCI)
  {
    if (ch_b_v != ch_b_s)
    {
      li_config_table[ch_b_v].flag_table[ch_b_s] |= LI_FLAG_CONFERENCE;
      li_config_table[ch_b_s].flag_table[ch_b_v] |= LI_FLAG_CONFERENCE;
    }
  }
  switch (plci_b->B1_options & DSP_CAI_ARRANGEMENT_MASK)
  {
  case DSP_CAI_ARRANGEMENT_NULL_PLCI:
    li_config_table[ch_b].chflags |= LI_CHFLAG_NULL_PLCI;
    break;
  case DSP_CAI_ARRANGEMENT_LINE_SIDE:
    li_config_table[ch_b].chflags |= LI_CHFLAG_LINE_SIDE;
    break;
  }
  if (!connect)
  {
    mixer_update_uninvolve (ch_a);
    mixer_update_uninvolve (ch_b);
  }
}


static word li_check_main_plci (dword Id, PLCI   *plci)
{
  if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    return (_WRONG_IDENTIFIER);
  }
  if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING)
   || !plci->NL.Id || plci->nl_remove_id
   || (plci->li_bchannel_id == 0))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state %d %02x %02x %d",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
      plci->State, plci->NL.Id, plci->nl_remove_id, plci->li_bchannel_id));
    return (_WRONG_STATE);
  }
  return (GOOD);
}


static PLCI   *li_check_plci_b (dword Id, PLCI   *plci,
  dword plci_b_id, word plci_b_write_pos, byte   *p_result)
{
  byte ctlr_b;
  PLCI   *plci_b;

  if (((plci->li_plci_b_read_pos > plci_b_write_pos) ? plci->li_plci_b_read_pos :
    LI_PLCI_B_QUEUE_ENTRIES + plci->li_plci_b_read_pos) - plci_b_write_pos - 1 < 2)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    WRITE_WORD (p_result, _REQUEST_NOT_ALLOWED_IN_THIS_STATE);
    return (NULL);
  }
  ctlr_b = 0;
  if ((plci_b_id & 0x7f) != 0)
  {
    ctlr_b = MapController ((byte)(plci_b_id & 0x7f));
    if ((ctlr_b > max_adapter) || ((ctlr_b != 0) && (adapter[ctlr_b - 1].request == NULL)))
      ctlr_b = 0;
  }
  if ((ctlr_b == 0)
   || (((plci_b_id >> 8) & 0xff) == 0)
   || (((plci_b_id >> 8) & 0xff) > adapter[ctlr_b - 1].max_plci))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI invalid second PLCI %08lx",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (unsigned long) plci_b_id));
    WRITE_WORD (p_result, _WRONG_IDENTIFIER);
    return (NULL);
  }
  plci_b = &(adapter[ctlr_b - 1].plci[((plci_b_id >> 8) & 0xff) - 1]);
  if ((plci_b->State == IDLE)
   || (plci_b->State == OUTG_DIS_PENDING)
   || (plci_b->State == INC_DIS_PENDING)
   || (plci_b->li_bchannel_id == 0))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI peer in wrong state %08lx %d %d",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
      (unsigned long) plci_b_id, plci_b->State, plci_b->li_bchannel_id));
    WRITE_WORD (p_result, _REQUEST_NOT_ALLOWED_IN_THIS_STATE);
    return (NULL);
  }
  if (((byte)(plci_b_id & ~EXT_CONTROLLER)) !=
    ((byte)(UnMapController (plci->adapter->Id) & ~EXT_CONTROLLER))
   && (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || !(plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI not on same ctrl %08lx",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (unsigned long) plci_b_id));
    WRITE_WORD (p_result, _WRONG_IDENTIFIER);
    return (NULL);
  }
  if (!(get_b1_facilities (plci_b, add_b1_facilities (plci_b, plci_b->B1_resource,
    (word)(plci_b->B1_facilities | B1_FACILITY_MIXER))) & B1_FACILITY_MIXER))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Interconnect peer cannot mix %04x",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci_b->B1_resource));
    WRITE_WORD (p_result, _REQUEST_NOT_ALLOWED_IN_THIS_STATE);
    return (NULL);
  }
  return (plci_b);
}


static PLCI   *li2_check_plci_b (dword Id, PLCI   *plci,
  dword plci_b_id, word plci_b_write_pos, byte   *p_result)
{
  byte ctlr_b;
  PLCI   *plci_b;

  if (((plci->li_plci_b_read_pos > plci_b_write_pos) ? plci->li_plci_b_read_pos :
    LI_PLCI_B_QUEUE_ENTRIES + plci->li_plci_b_read_pos) - plci_b_write_pos - 1 < 2)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    WRITE_WORD (p_result, _WRONG_STATE);
    return (NULL);
  }
  ctlr_b = 0;
  if ((plci_b_id & 0x7f) != 0)
  {
    ctlr_b = MapController ((byte)(plci_b_id & 0x7f));
    if ((ctlr_b > max_adapter) || ((ctlr_b != 0) && (adapter[ctlr_b - 1].request == NULL)))
      ctlr_b = 0;
  }
  if ((ctlr_b == 0)
   || (((plci_b_id >> 8) & 0xff) == 0)
   || (((plci_b_id >> 8) & 0xff) > adapter[ctlr_b - 1].max_plci))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI invalid second PLCI %08lx",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (unsigned long) plci_b_id));
    WRITE_WORD (p_result, _WRONG_IDENTIFIER);
    return (NULL);
  }
  plci_b = &(adapter[ctlr_b - 1].plci[((plci_b_id >> 8) & 0xff) - 1]);
  if ((plci_b->State == IDLE)
   || (plci_b->State == OUTG_DIS_PENDING)
   || (plci_b->State == INC_DIS_PENDING)
   || (plci_b->li_bchannel_id == 0))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI peer in wrong state %08lx %d %d",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
      (unsigned long) plci_b_id, plci_b->State, plci_b->li_bchannel_id));
    WRITE_WORD (p_result, _WRONG_STATE);
    return (NULL);
  }
  if (((byte)(plci_b_id & ~EXT_CONTROLLER)) !=
    ((byte)(UnMapController (plci->adapter->Id) & ~EXT_CONTROLLER))
   && (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || !(plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI not on same ctrl %08lx",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (unsigned long) plci_b_id));
    WRITE_WORD (p_result, _WRONG_IDENTIFIER);
    return (NULL);
  }
  if (!(get_b1_facilities (plci_b, add_b1_facilities (plci_b, plci_b->B1_resource,
    (word)(plci_b->B1_facilities | B1_FACILITY_MIXER))) & B1_FACILITY_MIXER))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Interconnect peer cannot mix %04x",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci_b->B1_resource));
    WRITE_WORD (p_result, _WRONG_STATE);
    return (NULL);
  }
  return (plci_b);
}


#define MIXER_PROCESS_OPERATION_NONE      0
#define MIXER_PROCESS_OPERATION_PRECHECK  1
#define MIXER_PROCESS_OPERATION_CHECK     2
#define MIXER_PROCESS_OPERATION_UPDATE    3

static byte mixer_process_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl,
  API_PARSE *li_parms, word operation, byte   *p_update_possible)
{
  word Info;
  word i;
  dword li_flags, plci_b_id;
  PLCI   *plci_b;
    API_PARSE li_req_parms[3];
    API_PARSE li_participant_struct[2];
    API_PARSE li_participant_parms[3];
  word participant_parms_pos;
  static byte result_buffer[((word)(MAX_MSG_SIZE / (1+4))) * (1+4+2)];
  word result_pos;
  word plci_b_write_pos;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_process_request %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (int) operation));

  *p_update_possible = TRUE;
  Info = GOOD;
  {
    result_buffer[0] = 3;
    WRITE_WORD (&result_buffer[1], READ_WORD (li_parms[0].info));
    result_buffer[3] = 0;
    switch (READ_WORD (li_parms[0].info))
    {
    case LI_REQ_CONNECT:
      if (li_parms[1].length == 8)
      {
        appl->appl_flags |= APPL_FLAG_OLD_LI_SPEC;
        if (api_parse (&li_parms[1].info[1], li_parms[1].length, "dd", li_req_parms))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        plci_b_id = READ_DWORD (li_req_parms[0].info) & 0xffff;
        li_flags = READ_DWORD (li_req_parms[1].info);
        Info = li_check_main_plci (Id, plci);
        result_buffer[0] = 9;
        result_buffer[3] = 6;
        WRITE_DWORD (&result_buffer[4], plci_b_id);
        WRITE_WORD (&result_buffer[8], GOOD);
        if (Info != GOOD)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            plci->li_channel_bits));
          Info = _WRONG_STATE;
          break;
        }
        plci->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        plci_b_write_pos = plci->li_plci_b_write_pos;
        plci_b = li_check_plci_b (Id, plci, plci_b_id, plci_b_write_pos, &result_buffer[8]);
        if (plci_b == NULL)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci_b->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address on %08lx %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            (unsigned long) plci_b_id, plci_b->li_channel_bits));
          WRITE_WORD (&result_buffer[8], _WRONG_STATE);
          break;
        }
        plci_b->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        if ((plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
         && !(plci_b->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
        {
          *p_update_possible = FALSE;
          if ((operation == MIXER_PROCESS_OPERATION_PRECHECK) || (operation == MIXER_PROCESS_OPERATION_CHECK))
            mixer_notify_update (plci_b, FALSE);
        }
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
        {
          plci_b->li_channel_bits |= LI_CHANNEL_INVOLVED;
          li_update_connect (Id, a, plci, plci_b_id, TRUE, li_flags);
        }
        plci->li_plci_b_queue[plci_b_write_pos] = plci_b_id | LI_PLCI_B_LAST_FLAG;
        plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          plci->li_plci_b_write_pos = plci_b_write_pos;
      }
      else
      {
        appl->appl_flags &= ~APPL_FLAG_OLD_LI_SPEC;
        if (api_parse (&li_parms[1].info[1], li_parms[1].length, "ds", li_req_parms))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        li_flags = READ_DWORD (li_req_parms[0].info) & ~(LI2_FLAG_INTERCONNECT_A_B | LI2_FLAG_INTERCONNECT_B_A);
        Info = li_check_main_plci (Id, plci);
        result_buffer[0] = 6;
        result_buffer[3] = 3;
        WRITE_WORD (&result_buffer[4], Info);
        result_buffer[6] = 0;
        if (Info != GOOD)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            plci->li_channel_bits));
          Info = _WRONG_STATE;
          break;
        }
        plci->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        plci_b_write_pos = plci->li_plci_b_write_pos;
        participant_parms_pos = 0;
        result_pos = 7;
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          li2_update_connect (Id, a, plci, UnMapId (Id), TRUE, li_flags);
        while (participant_parms_pos < li_req_parms[1].length)
        {
          result_buffer[result_pos] = 6;
          result_pos += 7;
          WRITE_DWORD (&result_buffer[result_pos - 6], 0);
          WRITE_WORD (&result_buffer[result_pos - 2], GOOD);
          if (api_parse (&li_req_parms[1].info[1 + participant_parms_pos],
            (word)(li_parms[1].length - participant_parms_pos), "s", li_participant_struct))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_MESSAGE_FORMAT);
            break;
          }
          if (api_parse (&li_participant_struct[0].info[1],
            li_participant_struct[0].length, "dd", li_participant_parms))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_MESSAGE_FORMAT);
            break;
          }
          plci_b_id = READ_DWORD (li_participant_parms[0].info) & 0xffff;
          li_flags = READ_DWORD (li_participant_parms[1].info);
          WRITE_DWORD (&result_buffer[result_pos - 6], plci_b_id);
          if (sizeof(result_buffer) - result_pos < 7)
          {
            dbug (1, dprintf ("[%06lx] %s,%d: LI result overrun",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_STATE);
            break;
          }
          plci_b = li2_check_plci_b (Id, plci, plci_b_id, plci_b_write_pos, &result_buffer[result_pos-2]);
          if (plci_b != NULL)
          {
            if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
             && (plci_b->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
            {
              dbug (1, dprintf ("[%06lx] %s,%d: No LI address on %08lx %02x",
                (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
                (unsigned long) plci_b_id, plci_b->li_channel_bits));
              WRITE_WORD (&result_buffer[result_pos-2], _WRONG_STATE);
            }
            else
            {
              plci_b->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
              if ((plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
               && !(plci_b->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
              {
                *p_update_possible = FALSE;
                if ((operation == MIXER_PROCESS_OPERATION_PRECHECK) || (operation == MIXER_PROCESS_OPERATION_CHECK))
                  mixer_notify_update (plci_b, FALSE);
              }
              if (operation == MIXER_PROCESS_OPERATION_UPDATE)
              {
                plci_b->li_channel_bits |= LI_CHANNEL_INVOLVED;
                li2_update_connect (Id, a, plci, plci_b_id, TRUE, li_flags);
              }
              plci->li_plci_b_queue[plci_b_write_pos] = plci_b_id |
                ((li_flags & (LI2_FLAG_INTERCONNECT_A_B | LI2_FLAG_INTERCONNECT_B_A |
                LI2_FLAG_PCCONNECT_A_B | LI2_FLAG_PCCONNECT_B_A)) ? 0 : LI_PLCI_B_DISC_FLAG);
              plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
            }
          }
          participant_parms_pos = (word)((&li_participant_struct[0].info[1 + li_participant_struct[0].length]) -
            (&li_req_parms[1].info[1]));
        }
        result_buffer[0] = (byte)(result_pos - 1);
        result_buffer[3] = (byte)(result_pos - 4);
        result_buffer[6] = (byte)(result_pos - 7);
        i = (plci_b_write_pos == 0) ? LI_PLCI_B_QUEUE_ENTRIES-1 : plci_b_write_pos - 1;
        if ((plci_b_write_pos == plci->li_plci_b_read_pos)
         || (plci->li_plci_b_queue[i] & LI_PLCI_B_LAST_FLAG))
        {
          plci->li_plci_b_queue[plci_b_write_pos] = LI_PLCI_B_SKIP_FLAG | LI_PLCI_B_LAST_FLAG;
          plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
        }
        else
          plci->li_plci_b_queue[i] |= LI_PLCI_B_LAST_FLAG;
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          plci->li_plci_b_write_pos = plci_b_write_pos;
      }
      if (operation == MIXER_PROCESS_OPERATION_UPDATE)
      {
        mixer_calculate_coefs (a);
        sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
          "wwS", Info, SELECTOR_LINE_INTERCONNECT, (byte   *) result_buffer);
      }
      return (TRUE);

    case LI_REQ_DISCONNECT:
      if (li_parms[1].length == 4)
      {
        appl->appl_flags |= APPL_FLAG_OLD_LI_SPEC;
        if (api_parse (&li_parms[1].info[1], li_parms[1].length, "d", li_req_parms))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        plci_b_id = READ_DWORD (li_req_parms[0].info) & 0xffff;
        Info = li_check_main_plci (Id, plci);
        result_buffer[0] = 9;
        result_buffer[3] = 6;
        WRITE_DWORD (&result_buffer[4], READ_DWORD (li_req_parms[0].info));
        WRITE_WORD (&result_buffer[8], GOOD);
        if (Info != GOOD)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            plci->li_channel_bits));
          Info = _WRONG_STATE;
          break;
        }
        plci->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        plci_b_write_pos = plci->li_plci_b_write_pos;
        plci_b = li_check_plci_b (Id, plci, plci_b_id, plci_b_write_pos, &result_buffer[8]);
        if (plci_b == NULL)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci_b->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address on %08lx %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            (unsigned long) plci_b_id, plci_b->li_channel_bits));
          WRITE_WORD (&result_buffer[8], _WRONG_STATE);
          break;
        }
        plci_b->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        if ((plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
         && !(plci_b->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
        {
          *p_update_possible = FALSE;
          if ((operation == MIXER_PROCESS_OPERATION_PRECHECK) || (operation == MIXER_PROCESS_OPERATION_CHECK))
            mixer_notify_update (plci_b, FALSE);
        }
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          li_update_connect (Id, a, plci, plci_b_id, FALSE, 0);
        plci->li_plci_b_queue[plci_b_write_pos] = plci_b_id | LI_PLCI_B_DISC_FLAG | LI_PLCI_B_LAST_FLAG;
        plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          plci->li_plci_b_write_pos = plci_b_write_pos;
      }
      else
      {
        appl->appl_flags &= ~APPL_FLAG_OLD_LI_SPEC;
        if (api_parse (&li_parms[1].info[1], li_parms[1].length, "s", li_req_parms))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        Info = li_check_main_plci (Id, plci);
        result_buffer[0] = 6;
        result_buffer[3] = 3;
        WRITE_WORD (&result_buffer[4], Info);
        result_buffer[6] = 0;
        if (Info != GOOD)
          break;
        if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
         && (plci->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: No LI address %02x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
            plci->li_channel_bits));
          Info = _WRONG_STATE;
          break;
        }
        plci->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
        plci_b_write_pos = plci->li_plci_b_write_pos;
        participant_parms_pos = 0;
        result_pos = 7;
        while (participant_parms_pos < li_req_parms[0].length)
        {
          result_buffer[result_pos] = 6;
          result_pos += 7;
          WRITE_DWORD (&result_buffer[result_pos - 6], 0);
          WRITE_WORD (&result_buffer[result_pos - 2], GOOD);
          if (api_parse (&li_req_parms[0].info[1 + participant_parms_pos],
            (word)(li_parms[1].length - participant_parms_pos), "s", li_participant_struct))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_MESSAGE_FORMAT);
            break;
          }
          if (api_parse (&li_participant_struct[0].info[1],
            li_participant_struct[0].length, "d", li_participant_parms))
          {
            dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_MESSAGE_FORMAT);
            break;
          }
          plci_b_id = READ_DWORD (li_participant_parms[0].info) & 0xffff;
          WRITE_DWORD (&result_buffer[result_pos - 6], plci_b_id);
          if (sizeof(result_buffer) - result_pos < 7)
          {
            dbug (1, dprintf ("[%06lx] %s,%d: LI result overrun",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
            WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_STATE);
            break;
          }
          plci_b = li2_check_plci_b (Id, plci, plci_b_id, plci_b_write_pos, &result_buffer[result_pos - 2]);
          if (plci_b != NULL)
          {
            if ((operation != MIXER_PROCESS_OPERATION_PRECHECK)
             && (plci_b->li_channel_bits & LI_CHANNEL_NO_ADDRESSES))
            {
              dbug (1, dprintf ("[%06lx] %s,%d: No LI address on %08lx %02x",
                (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
                (unsigned long) plci_b_id, plci_b->li_channel_bits));
              WRITE_WORD (&result_buffer[result_pos - 2], _WRONG_STATE);
            }
            else
            {
              plci_b->li_channel_bits &= ~LI_CHANNEL_NO_ADDRESSES;
              if ((plci_b->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
               && !(plci_b->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
              {
                *p_update_possible = FALSE;
                if ((operation == MIXER_PROCESS_OPERATION_PRECHECK) || (operation == MIXER_PROCESS_OPERATION_CHECK))
                  mixer_notify_update (plci_b, FALSE);
              }
              if (operation == MIXER_PROCESS_OPERATION_UPDATE)
                li2_update_connect (Id, a, plci, plci_b_id, FALSE, 0);
              plci->li_plci_b_queue[plci_b_write_pos] = plci_b_id | LI_PLCI_B_DISC_FLAG;
              plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
            }
          }
          participant_parms_pos = (word)((&li_participant_struct[0].info[1 + li_participant_struct[0].length]) -
            (&li_req_parms[0].info[1]));
        }
        result_buffer[0] = (byte)(result_pos - 1);
        result_buffer[3] = (byte)(result_pos - 4);
        result_buffer[6] = (byte)(result_pos - 7);
        i = (plci_b_write_pos == 0) ? LI_PLCI_B_QUEUE_ENTRIES-1 : plci_b_write_pos - 1;
        if ((plci_b_write_pos == plci->li_plci_b_read_pos)
         || (plci->li_plci_b_queue[i] & LI_PLCI_B_LAST_FLAG))
        {
          plci->li_plci_b_queue[plci_b_write_pos] = LI_PLCI_B_SKIP_FLAG | LI_PLCI_B_LAST_FLAG;
          plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
        }
        else
          plci->li_plci_b_queue[i] |= LI_PLCI_B_LAST_FLAG;
        if (operation == MIXER_PROCESS_OPERATION_UPDATE)
          plci->li_plci_b_write_pos = plci_b_write_pos;
      }
      if (operation == MIXER_PROCESS_OPERATION_UPDATE)
      {
        mixer_calculate_coefs (a);
        sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
          "wwS", Info, SELECTOR_LINE_INTERCONNECT, (byte   *) result_buffer);
      }
      return (TRUE);
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wwS", Info, SELECTOR_LINE_INTERCONNECT, (byte   *) result_buffer);
  return (FALSE);
}


static void mixer_command (dword Id, PLCI   *plci, byte Rc)
{
  DIVA_CAPI_ADAPTER   *a;
    API_PARSE li_parms[3];
  byte result_buffer[8];
  word i, internal_command, Info;
  byte update_possible;
  dword d;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_command %02x %04x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->li_cmd));

  Info = GOOD;
  a = plci->adapter;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (plci->li_cmd)
  {
  case LI_REQ_CONNECT:
  case LI_REQ_DISCONNECT:
  case LI_REQ_SILENT_UPDATE:
    switch (internal_command)
    {

    default:
      if ((a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
       && !(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
      {
        adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities |
          B1_FACILITY_MIXER), MIXER_COMMAND_1);
      }
    case MIXER_COMMAND_1:
      if ((a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
       && !(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
      {
        if (adjust_b_process (Id, plci, Rc) != GOOD)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Load mixer failed",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
          break;
        }
        if (plci->internal_command)
          return;
      }
    case MIXER_COMMAND_2:
      if ((a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
       && !(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
      {
        if (plci_nl_busy (plci))
        {
          plci->internal_command = MIXER_COMMAND_2;
          return;
        }
        xconnect_query_addresses (plci);
        plci->internal_command = MIXER_COMMAND_3;
        return;
      }
      internal_command = MIXER_COMMAND_6;
      Rc = OK;
    case MIXER_COMMAND_3:
    case MIXER_COMMAND_4:
    case MIXER_COMMAND_5:
      if ((Rc != OK) && (Rc != OK_FC) && (Rc != 0))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B query addresses failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      if (Rc == OK)
      {
        if ((internal_command == MIXER_COMMAND_3)
         || (internal_command == MIXER_COMMAND_4))
        {
          plci->internal_command = MIXER_COMMAND_4;
          return;
        }
      }
      else if (Rc == 0)
      {
        if ((internal_command == MIXER_COMMAND_3)
         || (internal_command == MIXER_COMMAND_5))
        {
          plci->internal_command = MIXER_COMMAND_5;
          return;
        }
      }
    case MIXER_COMMAND_6:
      if (plci->li_cmd == LI_REQ_SILENT_UPDATE)
        plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
      else
      {
        api_load_msg (&plci->saved_msg, li_parms);
        if (!mixer_process_request (Id, plci->li_msg_number, a, plci, plci->appl,
                                    li_parms, MIXER_PROCESS_OPERATION_CHECK, &update_possible))
        {
          break;
        }
        if (!update_possible)
        {
          mixer_wait_update (plci);
          plci->internal_command = MIXER_COMMAND_6;
          return;
        }
        if (!mixer_process_request (Id, plci->li_msg_number, a, plci, plci->appl,
                                    li_parms, MIXER_PROCESS_OPERATION_UPDATE, &update_possible))
        {
          break;
        }
      }
      if (li_config_table[a->li_base + (plci->li_bchannel_id - 1)].channel & LI_CHANNEL_INVOLVED)
      {
        plci->li_channel_bits = (plci->li_channel_bits & (LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES)) |
          (li_config_table[a->li_base + (plci->li_bchannel_id - 1)].channel & ~(LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES));
        adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities |
          B1_FACILITY_MIXER), MIXER_COMMAND_7);
      }
      else
      {
        mixer_clear_config (plci);
      }
      internal_command = MIXER_COMMAND_7;
    case MIXER_COMMAND_7:
      if (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
      {
        if (adjust_b_process (Id, plci, Rc) != GOOD)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Load mixer failed",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
          break;
        }
        if (plci->internal_command)
          return;
      }
    case MIXER_COMMAND_8:
      plci->li_plci_b_req_pos = plci->li_plci_b_write_pos;
      if ((plci->li_channel_bits & LI_CHANNEL_INVOLVED)
       || ((get_b1_facilities (plci, plci->B1_resource) & B1_FACILITY_MIXER)
        && (add_b1_facilities (plci, plci->B1_resource, (word)(plci->B1_facilities &
         ~B1_FACILITY_MIXER)) == plci->B1_resource)))
      {
        xconnect_write_coefs (plci, MIXER_COMMAND_9, (byte)(plci->li_cmd != LI_REQ_SILENT_UPDATE));
      }
      else
      {
        do
        {
          mixer_indication_coefs_set (Id, plci);
        } while (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos);
      }
    case MIXER_COMMAND_9:
      if ((plci->li_channel_bits & LI_CHANNEL_INVOLVED)
       || ((get_b1_facilities (plci, plci->B1_resource) & B1_FACILITY_MIXER)
        && (add_b1_facilities (plci, plci->B1_resource, (word)(plci->B1_facilities &
         ~B1_FACILITY_MIXER)) == plci->B1_resource)))
      {

        if (!xconnect_write_coefs_process (Id, plci, Rc))
        {

          dbug (1, dprintf ("[%06lx] %s,%d: Write mixer coefs failed",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        plci->li_error = TRUE;
        i = plci->li_plci_b_read_pos;
        if (i != plci->li_plci_b_req_pos)
        {
          do
          {
            d = plci->li_plci_b_queue[i];
            plci->li_plci_b_queue[i] = d | LI_PLCI_B_ERROR_FLAG;
            i = (i == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : i + 1;
          } while (!(d & LI_PLCI_B_LAST_FLAG) && (i != plci->li_plci_b_req_pos));
        }
           do
           {
             mixer_indication_coefs_set (Id, plci);
           } while (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos);
           Info = _WRONG_STATE;
           break;
        }
        if (plci->internal_command)
          return;


        if (!plci->li_write_is_sync)
        {
          do
          {
            mixer_indication_coefs_set (Id, plci);
          } while (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos);
        }
      }
      if (!(plci->li_channel_bits & LI_CHANNEL_INVOLVED))
      {
        adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities &
          ~B1_FACILITY_MIXER), MIXER_COMMAND_10);
      }
    case MIXER_COMMAND_10:
      if (!(plci->li_channel_bits & LI_CHANNEL_INVOLVED))
      {
        if (adjust_b_process (Id, plci, Rc) != GOOD)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Unload mixer failed",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
          break;
        }
        if (plci->internal_command)
          return;
      }
      break;
    }
    break;
  }
  if (internal_command < MIXER_COMMAND_6)
  {
    if (plci->li_cmd == LI_REQ_SILENT_UPDATE)
    {
      plci->li_channel_bits |= LI_CHANNEL_NO_ADDRESSES;
      plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
    }
    else
    {
      api_load_msg (&plci->saved_msg, li_parms);
      if (plci->appl->appl_flags & APPL_FLAG_OLD_LI_SPEC)
      {
        result_buffer[0] = 3;
        WRITE_WORD (&result_buffer[1], READ_WORD (li_parms[0].info));
        result_buffer[3] = 0;
      }
      else
      {
        result_buffer[0] = 6;
        WRITE_WORD (&result_buffer[1], READ_WORD (li_parms[0].info));
        result_buffer[3] = 3;
        WRITE_WORD (&result_buffer[4], Info);
        result_buffer[6] = 0;
      }
      sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->li_msg_number,
        "wwS", Info, SELECTOR_LINE_INTERCONNECT, (byte   *) result_buffer);
    }
  }
  else if (internal_command > MIXER_COMMAND_6)
  {
    if ((plci->li_bchannel_id != 0)
     && (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
      || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
    {
      i = a->li_base + (plci->li_bchannel_id - 1);
      li_config_table[i].curchnl = plci->li_channel_bits;
      if (!a->li_pri && (plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI))
      {
        i = a->li_base + MIXER_IC_CHANNEL_BASE + (plci->li_bchannel_id - 1);
        li_config_table[i].curchnl = plci->li_channel_bits;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
        {
          i = a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id);
          li_config_table[i].curchnl = plci->li_channel_bits;
        }
      }
    }
    if (plci->li_cmd != LI_REQ_SILENT_UPDATE)
      mixer_notify_update (plci, TRUE);
  }
  plci->li_cmd = 0;
}


static byte mixer_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i;
  dword d;
    API_PARSE li_parms[3];
  byte result_buffer[32];
  word plci_b_write_pos;
  byte update_possible;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result_buffer[0] = 0;
  if (!(a->profile.Global_Options & GL_LINE_INTERCONNECT_SUPPORTED))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else if (api_parse (&msg[1].info[1], msg[1].length, "ws", li_parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }
  else
  {
    result_buffer[0] = 3;
    WRITE_WORD (&result_buffer[1], READ_WORD (li_parms[0].info));
    result_buffer[3] = 0;
    switch (READ_WORD (li_parms[0].info))
    {
    case LI_GET_SUPPORTED_SERVICES:
      if (appl->appl_flags & APPL_FLAG_OLD_LI_SPEC)
      {
        result_buffer[0] = 17;
        result_buffer[3] = 14;
        WRITE_WORD (&result_buffer[4], GOOD);
        d = 0;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_CH_CH)
          d |= LI_CONFERENCING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_CH_PC)
          d |= LI_MONITORING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_PC_CH)
          d |= LI_ANNOUNCEMENTS_SUPPORTED | LI_MIXING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
          d |= LI_CROSS_CONTROLLER_SUPPORTED;
        WRITE_DWORD (&result_buffer[6], d);
        if (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
        {
          d = 0;
          for (i = 0; i < li_total_channels; i++)
          {
            if ((li_config_table[i].adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
             && (li_config_table[i].adapter->li_pri
              || (i < li_config_table[i].adapter->li_base + MIXER_BCHANNELS_BRI)))
            {
              d++;
            }
          }
        }
        else
        {
          d = a->li_pri ? MIXER_CHANNELS_PRI : MIXER_BCHANNELS_BRI;

          if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_NULL_PLCI)
            d *= 2;

        }
        WRITE_DWORD (&result_buffer[10], d / 2);
        WRITE_DWORD (&result_buffer[14], d);
      }
      else
      {
        result_buffer[0] = 25;
        result_buffer[3] = 22;
        WRITE_WORD (&result_buffer[4], GOOD);
        d = LI2_ASYMMETRIC_SUPPORTED | LI2_B_LOOPING_SUPPORTED | LI2_X_LOOPING_SUPPORTED |
          LI2_NOT_X_SENDING_SUPPORTED | LI2_NOT_X_RECEIVING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_CH_PC)
          d |= LI2_MONITORING_SUPPORTED | LI2_REMOTE_MONITORING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_PC_CH)
          d |= LI2_MIXING_SUPPORTED | LI2_REMOTE_MIXING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_MIXER_PC_PC)
          d |= LI2_PC_LOOPING_SUPPORTED;
        if (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
          d |= LI2_CROSS_CONTROLLER_SUPPORTED;
        WRITE_DWORD (&result_buffer[6], d);
        d = a->li_pri ? MIXER_CHANNELS_PRI : MIXER_BCHANNELS_BRI;

        if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_NULL_PLCI)
          d *= 2;

        WRITE_DWORD (&result_buffer[10], d / 2);
        WRITE_DWORD (&result_buffer[14], d - 1);
        if (a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
        {
          d = 0;
          for (i = 0; i < li_total_channels; i++)
          {
            if ((li_config_table[i].adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
             && (li_config_table[i].adapter->li_pri
              || (i < li_config_table[i].adapter->li_base + MIXER_BCHANNELS_BRI)))
            {
              d++;
            }
          }
        }
        WRITE_DWORD (&result_buffer[18], d / 2);
        WRITE_DWORD (&result_buffer[22], d - 1);
      }
      break;

    case LI_REQ_CONNECT:
    case LI_REQ_DISCONNECT:
      if (!mixer_process_request (Id, Number, a, plci, appl,
                                  li_parms, MIXER_PROCESS_OPERATION_PRECHECK, &update_possible))
      {
        return (FALSE);
      }
      api_save_msg (li_parms, "ws", &plci->saved_msg);
      plci->command = 0;
      plci->li_cmd = READ_WORD (li_parms[0].info);
      plci->li_msg_number = Number;
      start_internal_command (Id, plci, mixer_command);
      return (FALSE);

    case LI_REQ_SILENT_UPDATE:
      if (!plci
       || (plci->State == IDLE)
       || (plci->State == OUTG_DIS_PENDING)
       || (plci->State == INC_DIS_PENDING)
       || !plci->NL.Id || plci->nl_remove_id
       || (plci->li_bchannel_id == 0))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        if (plci)
          plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
        return (FALSE);
      }
      plci_b_write_pos = plci->li_plci_b_write_pos;
      i = (plci_b_write_pos == 0) ? LI_PLCI_B_QUEUE_ENTRIES-1 : plci_b_write_pos - 1;
      if ((plci_b_write_pos == plci->li_plci_b_read_pos)
       || (plci->li_plci_b_queue[i] & LI_PLCI_B_LAST_FLAG))
      {
        if (((plci->li_plci_b_read_pos > plci_b_write_pos) ? plci->li_plci_b_read_pos :
          LI_PLCI_B_QUEUE_ENTRIES + plci->li_plci_b_read_pos) - plci_b_write_pos - 1 < 2)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        }
        else
        {
          plci->li_plci_b_queue[plci_b_write_pos] = LI_PLCI_B_SKIP_FLAG | LI_PLCI_B_LAST_FLAG;
          plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
        }
      }
      else
        plci->li_plci_b_queue[i] |= LI_PLCI_B_LAST_FLAG;
      plci->li_plci_b_write_pos = plci_b_write_pos;
      plci->command = 0;
      plci->li_cmd = READ_WORD (li_parms[0].info);
      start_internal_command (Id, plci, mixer_command);
      return (FALSE);

    default:
      dbug (1, dprintf ("[%06lx] %s,%d: LI unknown request %04x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, READ_WORD (li_parms[0].info)));
      Info = _FACILITY_NOT_SUPPORTED;
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wwS", Info, SELECTOR_LINE_INTERCONNECT, (byte   *) result_buffer);
  return (FALSE);
}


static void mixer_indication_coefs_set (dword Id, PLCI   *plci)
{
  dword d;
  byte ctlr_b;
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *plci_b;
    byte result[12];

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_indication_coefs_set",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  a = plci->adapter;
  if (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos)
  {
    do
    {
      d = plci->li_plci_b_queue[plci->li_plci_b_read_pos];
      if (!(d & LI_PLCI_B_SKIP_FLAG))
      {
        ctlr_b = 0;
        if ((d & 0x7f) != 0)
        {
          ctlr_b = MapController ((byte)(d & 0x7f));
          if ((ctlr_b <= max_adapter)
           && (ctlr_b != 0)
           && (adapter[ctlr_b - 1].request != NULL)
           && (((d >> 8) & 0xff) != 0)
           && (((d >> 8) & 0xff) <= adapter[ctlr_b - 1].max_plci))
          {
            plci_b = &(adapter[ctlr_b - 1].plci[((d >> 8) & 0xff) - 1]);
            if (plci_b->li_error)
            {
              plci_b->li_error = FALSE;
              d |= LI_PLCI_B_ERROR_FLAG;
            }
          }
        }
        if (plci->appl->appl_flags & APPL_FLAG_OLD_LI_SPEC)
        {
          if (d & (LI_PLCI_B_DISC_FLAG | LI_PLCI_B_ERROR_FLAG))
          {
            result[0] = 5;
            WRITE_WORD (&result[1], LI_IND_DISCONNECT);
            result[3] = 2;
            WRITE_WORD (&result[4], (d & LI_PLCI_B_ERROR_FLAG) ? _LI_LINES_NOT_COMPATIBLE : _LI_USER_INITIATED);
          }
          else
          {
            result[0] = 7;
            WRITE_WORD (&result[1], LI_IND_CONNECT_ACTIVE);
            result[3] = 4;
            WRITE_DWORD (&result[4], d & ~LI_PLCI_B_FLAG_MASK);
          }
        }
        else
        {
          if (d & (LI_PLCI_B_DISC_FLAG | LI_PLCI_B_ERROR_FLAG))
          {
            result[0] = 9;
            WRITE_WORD (&result[1], LI_IND_DISCONNECT);
            result[3] = 6;
            WRITE_DWORD (&result[4], d & ~LI_PLCI_B_FLAG_MASK);
            WRITE_WORD (&result[8], (d & LI_PLCI_B_ERROR_FLAG) ? _LI2_LINES_NOT_COMPATIBLE : _LI2_USER_INITIATED);
          }
          else
          {
            result[0] = 7;
            WRITE_WORD (&result[1], LI_IND_CONNECT_ACTIVE);
            result[3] = 4;
            WRITE_DWORD (&result[4], d & ~LI_PLCI_B_FLAG_MASK);
          }
        }
        if ((plci->State != IDLE) && (plci->State != INC_DIS_PENDING))
        {
          sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0,
            "ws", SELECTOR_LINE_INTERCONNECT, result);
        }
      }
      plci->li_plci_b_read_pos = (plci->li_plci_b_read_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ?
        0 : plci->li_plci_b_read_pos + 1;
    } while (!(d & LI_PLCI_B_LAST_FLAG) && (plci->li_plci_b_read_pos != plci->li_plci_b_req_pos));
  }
}


static void mixer_indication_xconnect_from (dword Id, PLCI   *plci, byte   *msg, word length)
{
  word i, j, ch;
  byte source_info;
  struct xconnect_transfer_address_s s;
  DIVA_CAPI_ADAPTER   *a;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_indication_xconnect_from %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (int) length));

  a = plci->adapter;
  if ((plci->State != IDLE)
   && plci->NL.Id && !plci->nl_remove_id)
  {
    for (i = 1; i < length; i += 16)
    {
      s.card_id = msg[i] | (msg[i+1] << 8) | (((dword)(msg[i+2])) << 16) | (((dword)(msg[i+3])) << 24);
      s.bus_address.high = msg[i+4] | (msg[i+5] << 8) | (((dword)(msg[i+6])) << 16) | (((dword)(msg[i+7])) << 24);
      s.bus_address.low = msg[i+8] | (msg[i+9] << 8) | (((dword)(msg[i+10])) << 16) | (((dword)(msg[i+11])) << 24);
      ch = msg[i+12] | (msg[i+13] << 8);
      source_info = msg[i+15];
      j = ch & XCONNECT_CHANNEL_NUMBER_MASK;
      if (a->li_pri)
      {
        if ((j != 0) && !(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
        {
          plci->li_bchannel_id = j + 1;
          a->li_query_channel_supported = TRUE;
        }
      }
      else
      {
        if (j > 2)
        {
          if (!(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
          {
            plci->li_bchannel_id = j - 2;
            a->li_query_channel_supported = TRUE;
          }
          j -= 3;
        }
        else
        {
          j &= 1;
          if (plci->li_bchannel_id == 2)
            j = 1 - j;
        }
      }
      j += a->li_base;
      if (!(plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET))
      {
        plci->li_channel_bits |= LI_CHANNEL_ADDRESSES_SET;
        if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
          li_config_table[j].channel |= LI_CHANNEL_INVOLVED;
      }
      if (ch & XCONNECT_CHANNEL_PORT_PC)
      {
        li_config_table[j].send_pc.card_id = s.card_id;
        li_config_table[j].send_pc.bus_address.low = s.bus_address.low;
        li_config_table[j].send_pc.bus_address.high = s.bus_address.high;
        li_config_table[j].source_info_pc = source_info;
      }
      else
      {
        li_config_table[j].send_b.card_id = s.card_id;
        li_config_table[j].send_b.bus_address.low = s.bus_address.low;
        li_config_table[j].send_b.bus_address.high = s.bus_address.high;
        li_config_table[j].source_info_b = source_info;
      }
      li_config_table[j].channel |= LI_CHANNEL_ADDRESSES_SET;
    }
    if (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)
    {
      for (i = 0; i < a->max_plci; i++)
      {
        if ((a->plci[i].li_bchannel_id == plci->li_bchannel_id)
         && (a->plci[i].li_channel_bits & LI_CHANNEL_ADDRESSES_SET)
         && (&(a->plci[i]) != plci))
        {
          a->plci[i].li_channel_bits &= ~LI_CHANNEL_ADDRESSES_SET;
        }
      }
    }
  }
  if (plci->internal_command_queue[0]
   && ((plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_2)
    || (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_3)
    || (plci->adjust_b_state == ADJUST_B_RESTORE_MIXER_4)
    || (plci->internal_command == MIXER_COMMAND_3)
    || (plci->internal_command == MIXER_COMMAND_4)
    || (plci->internal_command == MIXER_COMMAND_5)))
  {
    (*(plci->internal_command_queue[0]))(Id, plci, 0);
    if (!plci->internal_command)
      next_internal_command (Id, plci);
  }
}


static void mixer_indication_xconnect_to (dword Id, PLCI   *plci, byte   *msg, word length)
{
  dword d;
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_indication_xconnect_to %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, (int) length));

  if ((plci->State != IDLE)
   && plci->NL.Id && !plci->nl_remove_id)
  {
    i = 1;
    while ((i < length) && ((msg[i] | (msg[i+1] << 8)) != XCONNECT_ERROR))
      i += 2;
    if (i < length)
    {
      plci->li_error = TRUE;
      i = plci->li_plci_b_read_pos;
      if (i != plci->li_plci_b_req_pos)
      {
        do
        {
          d = plci->li_plci_b_queue[i];
          plci->li_plci_b_queue[i] = d | LI_PLCI_B_ERROR_FLAG;
          i = (i == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : i + 1;
        } while (!(d & LI_PLCI_B_LAST_FLAG) && (i != plci->li_plci_b_req_pos));
      }
    }
  }
}


static byte mixer_notify_source_removed (PLCI   *plci, dword plci_b_id)
{
  word plci_b_write_pos;

  plci_b_write_pos = plci->li_plci_b_write_pos;
  if (((plci->li_plci_b_read_pos > plci_b_write_pos) ? plci->li_plci_b_read_pos :
    LI_PLCI_B_QUEUE_ENTRIES + plci->li_plci_b_read_pos) - plci_b_write_pos - 1 < 1)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun",
      (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
      (char   *)(FILE_), __LINE__));
    return (FALSE);
  }
  plci->li_plci_b_queue[plci_b_write_pos] = plci_b_id | LI_PLCI_B_DISC_FLAG;
  plci_b_write_pos = (plci_b_write_pos == LI_PLCI_B_QUEUE_ENTRIES-1) ? 0 : plci_b_write_pos + 1;
  plci->li_plci_b_write_pos = plci_b_write_pos;
  return (TRUE);
}


static void mixer_remove (PLCI   *plci)
{
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *notify_plci;
  dword plci_b_id;
  word i, j, k, n;

  dbug (1, dprintf ("[%06lx] %s,%d: mixer_remove",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  a = plci->adapter;
  plci_b_id = (plci->Id << 8) | UnMapController (plci->adapter->Id);
  if ((plci->li_bchannel_id != 0)
   && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
   && (!(a->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    i = a->li_base + (plci->li_bchannel_id - 1);
    if ((li_config_table[i].curchnl | li_config_table[i].channel) & LI_CHANNEL_INVOLVED)
    {
      for (n = 0; n < max_adapter; n++)
      {
        if (adapter[n].request && (adapter[n].profile.Global_Options & GL_LINE_INTERCONNECT_SUPPORTED))
        {
          for (k = 0; k < adapter[n].max_plci; k++)
          {
            notify_plci = &(adapter[n].plci[k]);
            if ((notify_plci->li_channel_bits & LI_CHANNEL_INVOLVED)
             && (notify_plci->li_bchannel_id != 0)
             && (!(notify_plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
              || (notify_plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
            {
              j = adapter[n].li_base + (notify_plci->li_bchannel_id - 1);
              if ((li_config_table[i].flag_table[j] & LI_FLAG_INTERCONNECT)
               || (li_config_table[j].flag_table[i] & LI_FLAG_INTERCONNECT))
              {
                if ((notify_plci != plci)
                 && (notify_plci->appl != NULL)
                 && !(notify_plci->appl->appl_flags & APPL_FLAG_OLD_LI_SPEC)
                 && (notify_plci->State != IDLE))
                {
                  mixer_notify_source_removed (notify_plci, plci_b_id);
                }
              }
            }
          }
        }
      }
      mixer_clear_config (plci);
      mixer_calculate_coefs (a);
      if (plci->li_cmd != LI_REQ_SILENT_UPDATE)
        mixer_notify_update (plci, TRUE);
    }
    plci->li_channel_bits = 0;
    plci->li_bchannel_id = 0;
    mixer_cancel_wait (plci);
    plci->li_cmd = 0;
    plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
  }
}


/*------------------------------------------------------------------*/
/* Echo canceller facilities                                        */
/*------------------------------------------------------------------*/


static void ec_write_parameters (PLCI   *plci)
{
    byte parameter_buffer[10];

  dbug (1, dprintf ("[%06lx] %s,%d: ec_write_parameters",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  parameter_buffer[0] = 9;
  parameter_buffer[1] = DSP_CTRL_SET_LEC_PARAMETERS;
  WRITE_WORD (&parameter_buffer[2], plci->ec_idi_options);
  plci->ec_idi_options &= ~LEC_RESET_COEFFICIENTS;
  WRITE_WORD (&parameter_buffer[4], plci->ec_span_ms);
  WRITE_WORD (&parameter_buffer[6], plci->ec_predelay_ms);
  WRITE_WORD (&parameter_buffer[8], plci->ec_sparse_span_ms);
  add_p (plci, FTY, parameter_buffer);
  sig_req (plci, TEL_CTRL, 0);
  send_req (plci);
}


static void ec_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: ec_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->ec_idi_options = LEC_ENABLE_ECHO_CANCELLER |
    LEC_MANUAL_DISABLE | LEC_ENABLE_NONLINEAR_PROCESSING;
  plci->ec_span_ms = 0;
  plci->ec_predelay_ms = 0;
  plci->ec_sparse_span_ms = 0;
}


static void ec_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: ec_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word ec_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: ec_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word ec_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: ec_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if (plci->B1_facilities & B1_FACILITY_EC)
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_EC_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci->sig_req)
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_EC_1;
        break;
      }
      ec_write_parameters (plci);
      plci->adjust_b_state = ADJUST_B_RESTORE_EC_2;
      break;
    case ADJUST_B_RESTORE_EC_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore EC failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static void ec_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
    byte result[8];

  dbug (1, dprintf ("[%06lx] %s,%d: ec_command %02x %04x %04x %04x %d %d %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->ec_cmd, plci->ec_idi_options, plci->ec_span_ms, plci->ec_predelay_ms, plci->ec_sparse_span_ms));

  Info = GOOD;
  if (plci->appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC)
  {
    result[0] = 2;
    WRITE_WORD (&result[1], EC_SUCCESS);
  }
  else
  {
    result[0] = 5;
    WRITE_WORD (&result[1], plci->ec_cmd);
    result[3] = 2;
    WRITE_WORD (&result[4], GOOD);
  }
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (plci->ec_cmd)
  {
  case EC_ENABLE_OPERATION:
  case EC_FREEZE_COEFFICIENTS:
  case EC_RESUME_COEFFICIENT_UPDATE:
  case EC_RESET_COEFFICIENTS:
    switch (internal_command)
    {
    default:
      adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities |
        B1_FACILITY_EC), EC_COMMAND_1);
    case EC_COMMAND_1:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Load EC failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
    case EC_COMMAND_2:
      if (plci->sig_req)
      {
        plci->internal_command = EC_COMMAND_2;
        return;
      }
      plci->internal_command = EC_COMMAND_3;
      ec_write_parameters (plci);
      return;
    case EC_COMMAND_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Enable EC failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
    break;

  case EC_DISABLE_OPERATION:
    switch (internal_command)
    {
    default:
    case EC_COMMAND_1:
      if (plci->B1_facilities & B1_FACILITY_EC)
      {
        if (plci->sig_req)
        {
          plci->internal_command = EC_COMMAND_1;
          return;
        }
        plci->internal_command = EC_COMMAND_2;
        ec_write_parameters (plci);
        return;
      }
      Rc = OK;
    case EC_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Disable EC failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      adjust_b1_resource (Id, plci, NULL, (word)(plci->B1_facilities &
        ~B1_FACILITY_EC), EC_COMMAND_3);
    case EC_COMMAND_3:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Unload EC failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
      break;
    }
    break;
  }
  sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->number,
    "wws", Info, (plci->appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC) ? PRIV_SELECTOR_ECHO_CANCELLER :
    ((plci->appl->appl_flags & APPL_FLAG_OLD_EC_SELECTOR) ? SELECTOR_ECHO_CANCELLER_OLD : SELECTOR_ECHO_CANCELLER_NEW), result);
}


static byte ec_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word opt;
    API_PARSE ec_parms[3];
    byte result[16];

  dbug (1, dprintf ("[%06lx] %s,%d: ec_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 0;
  if (!(a->profile.Manuf.private_options & (1L << PRIVATE_ECHO_CANCELLER)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else
  {
    if (appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC)
    {
      if (api_parse (&msg[1].info[1], msg[1].length, "w", ec_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_MESSAGE_FORMAT;
      }
      else
      {
        if (plci == NULL)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_IDENTIFIER;
        }
        else if ((plci->State == IDLE)
         || (plci->State == OUTG_DIS_PENDING)
         || (plci->State == INC_DIS_PENDING)
         || !plci->NL.Id || plci->nl_remove_id)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
        }
        else
        {
          plci->command = 0;
          plci->ec_cmd = READ_WORD (ec_parms[0].info);
          plci->ec_idi_options &= ~(LEC_MANUAL_DISABLE | LEC_RESET_COEFFICIENTS);
          result[0] = 2;
          WRITE_WORD (&result[1], EC_SUCCESS);
          if (msg[1].length >= 4)
          {
            opt = READ_WORD (&ec_parms[0].info[2]);
            plci->ec_idi_options &= ~(LEC_ENABLE_NONLINEAR_PROCESSING |
              LEC_ENABLE_2100HZ_DETECTOR | LEC_REQUIRE_2100HZ_REVERSALS);
            if (!(opt & EC_DISABLE_NON_LINEAR_PROCESSING))
              plci->ec_idi_options |= LEC_ENABLE_NONLINEAR_PROCESSING;
            if (opt & EC_DETECT_DISABLE_TONE)
              plci->ec_idi_options |= LEC_ENABLE_2100HZ_DETECTOR;
            if (!(opt & EC_DO_NOT_REQUIRE_REVERSALS))
              plci->ec_idi_options |= LEC_REQUIRE_2100HZ_REVERSALS;
            if (msg[1].length >= 6)
            {
              plci->ec_span_ms = READ_WORD (&ec_parms[0].info[4]);
            }
            if (msg[1].length >= 8)
            {
              plci->ec_predelay_ms = READ_WORD (&ec_parms[0].info[6]);
            }
            if (msg[1].length >= 10)
            {
              plci->ec_sparse_span_ms = READ_WORD (&ec_parms[0].info[8]);
            }
          }
          switch (plci->ec_cmd)
          {
          case EC_ENABLE_OPERATION:
            plci->ec_idi_options &= ~LEC_FREEZE_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          case EC_DISABLE_OPERATION:
            plci->ec_idi_options = LEC_ENABLE_ECHO_CANCELLER |
              LEC_MANUAL_DISABLE | LEC_ENABLE_NONLINEAR_PROCESSING |
              LEC_RESET_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          case EC_FREEZE_COEFFICIENTS:
            plci->ec_idi_options |= LEC_FREEZE_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          case EC_RESUME_COEFFICIENT_UPDATE:
            plci->ec_idi_options &= ~LEC_FREEZE_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          case EC_RESET_COEFFICIENTS:
            plci->ec_idi_options |= LEC_RESET_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          default:
            dbug (1, dprintf ("[%06lx] %s,%d: EC unknown request %04x",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->ec_cmd));
            WRITE_WORD (&result[1], EC_UNSUPPORTED_OPERATION);
          }
        }
      }
    }
    else
    {
      if (api_parse (&msg[1].info[1], msg[1].length, "ws", ec_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_MESSAGE_FORMAT;
      }
      else
      {
        if (READ_WORD (ec_parms[0].info) == EC_GET_SUPPORTED_SERVICES)
        {
          result[0] = 11;
          WRITE_WORD (&result[1], EC_GET_SUPPORTED_SERVICES);
          result[3] = 8;
          WRITE_WORD (&result[4], GOOD);
          WRITE_WORD (&result[6], 0x0007);
          WRITE_WORD (&result[8], LEC_MAX_SUPPORTED_TAIL_LENGTH);
          WRITE_WORD (&result[10], 0);
        }
        else if (plci == NULL)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_IDENTIFIER;
        }
        else if ((plci->State == IDLE)
         || (plci->State == OUTG_DIS_PENDING)
         || (plci->State == INC_DIS_PENDING)
         || !plci->NL.Id || plci->nl_remove_id)
        {
          dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
        }
        else
        {
          plci->command = 0;
          plci->ec_cmd = READ_WORD (ec_parms[0].info);
          plci->ec_idi_options &= ~(LEC_MANUAL_DISABLE | LEC_RESET_COEFFICIENTS);
          result[0] = 5;
          WRITE_WORD (&result[1], plci->ec_cmd);
          result[3] = 2;
          WRITE_WORD (&result[4], GOOD);
          plci->ec_idi_options &= ~(LEC_ENABLE_NONLINEAR_PROCESSING |
            LEC_ENABLE_2100HZ_DETECTOR | LEC_REQUIRE_2100HZ_REVERSALS);
          plci->ec_span_ms = 0;
          plci->ec_predelay_ms = 0;
          plci->ec_sparse_span_ms = 0;
          if (ec_parms[1].length >= 2)
          {
            opt = READ_WORD (&ec_parms[1].info[1]);
            if (opt & EC_ENABLE_NON_LINEAR_PROCESSING)
              plci->ec_idi_options |= LEC_ENABLE_NONLINEAR_PROCESSING;
            if (opt & EC_DETECT_DISABLE_TONE)
              plci->ec_idi_options |= LEC_ENABLE_2100HZ_DETECTOR;
            if (!(opt & EC_DO_NOT_REQUIRE_REVERSALS))
              plci->ec_idi_options |= LEC_REQUIRE_2100HZ_REVERSALS;
            if (ec_parms[1].length >= 4)
            {
              plci->ec_span_ms = READ_WORD (&ec_parms[1].info[3]);
            }
            if (ec_parms[1].length >= 6)
            {
              plci->ec_predelay_ms = READ_WORD (&ec_parms[1].info[5]);
            }
            if (ec_parms[1].length >= 8)
            {
              plci->ec_sparse_span_ms = READ_WORD (&ec_parms[1].info[7]);
            }
          }
          switch (plci->ec_cmd)
          {
          case EC_ENABLE_OPERATION:
            plci->ec_idi_options &= ~LEC_FREEZE_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          case EC_DISABLE_OPERATION:
            plci->ec_idi_options = LEC_ENABLE_ECHO_CANCELLER |
              LEC_MANUAL_DISABLE | LEC_ENABLE_NONLINEAR_PROCESSING |
              LEC_RESET_COEFFICIENTS;
            start_internal_command (Id, plci, ec_command);
            return (FALSE);

          default:
            dbug (1, dprintf ("[%06lx] %s,%d: EC unknown request %04x",
              (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->ec_cmd));
            WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
          }
        }
      }
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, (appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC) ? PRIV_SELECTOR_ECHO_CANCELLER :
    ((appl->appl_flags & APPL_FLAG_OLD_EC_SELECTOR) ? SELECTOR_ECHO_CANCELLER_OLD : SELECTOR_ECHO_CANCELLER_NEW), result);
  return (FALSE);
}


static void ec_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{
    byte result[8];
  word w;

  dbug (1, dprintf ("[%06lx] %s,%d: ec_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if (!(plci->ec_idi_options & LEC_MANUAL_DISABLE))
  {
    switch (msg[1])
    {
    case LEC_DISABLE_TYPE_CONTIGNUOUS_2100HZ:
      w = EC_BYPASS_DUE_TO_CONTINUOUS_2100HZ;
      break;
    case LEC_DISABLE_TYPE_REVERSED_2100HZ:
      w = EC_BYPASS_DUE_TO_REVERSED_2100HZ;
      break;
    case LEC_DISABLE_RELEASED:
      w = EC_BYPASS_RELEASED;
      break;
    case LEC_DISABLE_TYPE_INTERNAL:
      return;
    default:
      w = msg[1];
      break;
    }
    if (plci->appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC)
    {
      result[0] = 2;
      WRITE_WORD (&result[1], w);
    }
    else
    {
      result[0] = 5;
      WRITE_WORD (&result[1], EC_BYPASS_INDICATION);
      result[3] = 2;
      WRITE_WORD (&result[4], w);
    }
    sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "ws", (plci->appl->appl_flags & APPL_FLAG_PRIV_EC_SPEC) ? PRIV_SELECTOR_ECHO_CANCELLER :
      ((plci->appl->appl_flags & APPL_FLAG_OLD_EC_SELECTOR) ? SELECTOR_ECHO_CANCELLER_OLD : SELECTOR_ECHO_CANCELLER_NEW), result);
  }
}



/*------------------------------------------------------------------*/
/* Measure facilities / universal tone detector                     */
/*------------------------------------------------------------------*/


#define MEASURE_SUPPORTED_TIMER_PERIODE        0xfa00
#define MEASURE_SUPPORTED_SPECTRUM_FLAGS       0x0007
#define MEASURE_SUPPORTED_SPECTRUM_PEAKS       0x0020
#define MEASURE_SUPPORTED_SPECTRUM_APERTURE    512
#define MEASURE_SUPPORTED_CEPSTRUM_FLAGS       0x0007
#define MEASURE_SUPPORTED_CEPSTRUM_PEAKS       0x0020
#define MEASURE_SUPPORTED_ECHO_FLAGS           0x0007
#define MEASURE_SUPPORTED_ECHO_PEAKS           0x0020
#define MEASURE_SUPPORTED_ECHO_DELAY           0x6fff
#define MEASURE_ECHO_MIN_DELAY_DEFAULT         0
#define MEASURE_ECHO_MAX_DELAY_DEFAULT         MEASURE_SUPPORTED_ECHO_DELAY
#define MEASURE_ECHO_ACCUMULATE_TIME_DEFAULT   (MEASURE_SUPPORTED_ECHO_DELAY >> 5)
#define MEASURE_SUPPORTED_SNGLTONE_FLAGS       0x003e
#define MEASURE_SUPPORTED_SNGLTONE_DURATION    0xfa00
#define MEASURE_SUPPORTED_SNGLTONE_FREQ_MOD    0x7d00
#define MEASURE_SUPPORTED_SNGLTONE_APERTURE    512
#define MEASURE_SNGLTONE_MIN_DURATION_DEFAULT  800      /* 100 ms (unit is 8 kHz samples) */
#define MEASURE_SNGLTONE_MIN_SNR_DEFAULT       4608     /* 18 dB */
#define MEASURE_SNGLTONE_MIN_LEVEL_DEFAULT     -9216    /* -36 dBm */
#define MEASURE_SNGLTONE_MAX_AMPL_MOD_DEFAULT  512      /* 2 dB */
#define MEASURE_SNGLTONE_MAX_FREQ_MOD_DEFAULT  64       /* 8 Hz */
#define MEASURE_SUPPORTED_DUALTONE_FLAGS       0x003e
#define MEASURE_SUPPORTED_DUALTONE_DURATION    0xfa00
#define MEASURE_SUPPORTED_DUALTONE_APERTURE    512
#define MEASURE_DUALTONE_MIN_DURATION_DEFAULT  320      /* 40 ms (unit is 8 kHz samples) */
#define MEASURE_DUALTONE_MIN_SNR_DEFAULT       2560     /* 10 dB */
#define MEASURE_DUALTONE_MIN_LEVEL_DEFAULT     -9216    /* -36 dBm */
#define MEASURE_DUALTONE_MAX_HL_TWIST_DEFAULT  2560     /* 10 dB */
#define MEASURE_DUALTONE_MAX_LH_TWIST_DEFAULT  2560     /* 10 dB */
#define MEASURE_SUPPORTED_CURVE_POINTS         512
#define MEASURE_FSKDET_SUPPORTED_FREQUENCY     0x7d00
#define MEASURE_FSKDET_SUPPORTED_FRAMERS       0x0004
#define MEASURE_FSKDET_SUPPORTED_ASYNC_FORMAT  0xefff
#define MEASURE_FSKDET_SUPPORTED_FRAMER_OPTIONS 0x0000
#define MEASURE_FSKGEN_SUPPORTED_FREQUENCY     0x7d00
#define MEASURE_FSKGEN_SUPPORTED_FRAMERS       0x0004
#define MEASURE_FSKGEN_SUPPORTED_ASYNC_FORMAT  0xcfff
#define MEASURE_FSKGEN_SUPPORTED_FRAMER_OPTIONS 0x0000

static byte measure_support_table[] =
{
/*Ind   Len   Inst  Info  WordParm1   WordParm2   WordParm3   WordParm4   WordParm5   WordParm6   WordParm7   WordParm8   WordParm9   */
  0x00, 0x04, 0x01, 0x00, 0x00, 0xfa,
  0x40, 0x02, 0x01, 0x00,
  0x44, 0x04, 0x01, 0x00, 0xff, 0xff,
  0x50, 0x0a, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x20, 0x00, 0x00, 0x02,
  0x54, 0x08, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x20, 0x00,
  0x58, 0x0e, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x20, 0x00, 0xff, 0x6f, 0xff, 0x6f, 0xff, 0xff,
  0x60, 0x14, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0xfa, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0xff, 0x00, 0x7d, 0x00, 0xfa, 0x00, 0x02,
  0x64, 0x14, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0xfa, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0x00, 0xfa, 0x00, 0x02,
  0x70, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x7d, 0x00, 0x7d, 0x00, 0x7d, 0x04, 0x00, 0xff, 0xef, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00,
  0x84, 0x04, 0x01, 0x00, 0xff, 0xff,
  0x90, 0x0a, 0x01, 0x00, 0xff, 0x0f, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x94, 0x04, 0x18, 0x00, 0x00, 0x02,
  0x98, 0x04, 0x02, 0x00, 0x00, 0x02,
  0xa0, 0x02, 0x0c, 0x00,
  0xa4, 0x02, 0x01, 0x00,
  0xa8, 0x02, 0x01, 0x00,
  0xac, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x7d, 0x00, 0x7d, 0x00, 0x7d, 0x04, 0x00, 0xff, 0xcf, 0x00, 0x00
};

static byte measure_support_tone_table[] =
{
/*Ind   Len   Inst  Info  WordParm1   WordParm2   WordParm3   WordParm4   WordParm5   WordParm6   WordParm7   WordParm8   WordParm9   */
  0x00, 0x04, 0x01, 0x00, 0x00, 0xfa,
  0x60, 0x14, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0xfa, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0xff, 0x00, 0x7d, 0x00, 0xfa, 0x00, 0x02,
  0x64, 0x14, 0x01, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0xfa, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0xff, 0x7f, 0x00, 0xfa, 0x00, 0x02,
  0x70, 0x14, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x7d, 0x00, 0x7d, 0x00, 0x7d, 0x04, 0x00, 0xff, 0xef, 0x00, 0x00,
  0x90, 0x08, 0x01, 0x00, 0xff, 0x0f, 0x01, 0x00, 0x01, 0x00,
  0x94, 0x04, 0x18, 0x00, 0x00, 0x02,
  0x98, 0x04, 0x02, 0x00, 0x00, 0x02,
  0xa0, 0x02, 0x0c, 0x00,
  0xa4, 0x02, 0x01, 0x00,
  0xa8, 0x02, 0x01, 0x00
};


static void measure_clear_setup (PLCI   *plci)
{
  struct measure_setup_s   *m;
  word i;

  m = &(plci->measure_setup);
  m->timer.periode = 0;
  m->rx_selector.path = 0;
  m->rx_filter.curve_points = 0;
  m->spectrum.reserved = 0;
  m->spectrum.flags = 0;
  m->spectrum.peaks = 0;
  m->spectrum.aperture = 0;
  m->cepstrum.reserved = 0;
  m->cepstrum.flags = 0;
  m->cepstrum.peaks = 0;
  m->echo.reserved = 0;
  m->echo.flags = 0;
  m->echo.peaks = 0;
  m->echo.min_delay = MEASURE_ECHO_MIN_DELAY_DEFAULT;
  m->echo.max_delay = MEASURE_ECHO_MAX_DELAY_DEFAULT;
  m->echo.accumulate_time = MEASURE_ECHO_ACCUMULATE_TIME_DEFAULT;
  m->single_tone.reserved = 0;
  m->single_tone.flags = 0;
  m->single_tone.min_duration = MEASURE_SNGLTONE_MIN_DURATION_DEFAULT;
  m->single_tone.min_snr = MEASURE_SNGLTONE_MIN_SNR_DEFAULT;
  m->single_tone.min_level = (word)(MEASURE_SNGLTONE_MIN_LEVEL_DEFAULT);
  m->single_tone.max_amplitude_modulation = MEASURE_SNGLTONE_MAX_AMPL_MOD_DEFAULT;
  m->single_tone.max_frequency_modulation = MEASURE_SNGLTONE_MAX_FREQ_MOD_DEFAULT;
  m->single_tone.max_dropout = 0;
  m->single_tone.aperture = 0;
  m->dual_tone.reserved = 0;
  m->dual_tone.flags = 0;
  m->dual_tone.min_duration = MEASURE_DUALTONE_MIN_DURATION_DEFAULT;
  m->dual_tone.min_snr = MEASURE_DUALTONE_MIN_SNR_DEFAULT;
  m->dual_tone.min_level = (word)(MEASURE_DUALTONE_MIN_LEVEL_DEFAULT);
  m->dual_tone.max_high_low_twist = MEASURE_DUALTONE_MAX_HL_TWIST_DEFAULT;
  m->dual_tone.max_low_high_twist = MEASURE_DUALTONE_MAX_LH_TWIST_DEFAULT;
  m->dual_tone.max_dropout = 0;
  m->dual_tone.aperture = 0;
  m->fskdet.framer_type = 0;
  m->tx_selector.path = 0;
  m->tx_filter.curve_points = 0;
  m->generator.sinegen_mask = 0;
  m->generator.funcgen_mask = 0;
  m->generator.noisegen_mask = 0;
  m->generator.fskgen_mask = 0;
  for (i = 0; i < MEASURE_MODULATOR_COUNT; i++)
    m->modulator[i].curve_points = 0;
  for (i = 0; i < MEASURE_FUNCTION_COUNT; i++)
    m->function[i].curve_points = 0;
  for (i = 0; i < MEASURE_SINEGEN_COUNT; i++)
  {
    m->sinegen[i].amplitude_mod = 0;
    m->sinegen[i].frequency_mod = 0;
  }
  for (i = 0; i < MEASURE_FUNCGEN_COUNT; i++)
  {
    m->funcgen[i].function = 0;
    m->funcgen[i].amplitude_mod = 0;
    m->funcgen[i].frequency_mod = 0;
  }
  for (i = 0; i < MEASURE_NOISEGEN_COUNT; i++)
  {
    m->noisegen[i].function = 0;
    m->noisegen[i].amplitude_mod = 0;
  }
  for (i = 0; i < MEASURE_FSKGEN_COUNT; i++)
  {
    m->fskgen[i].framer_type = 0;
  }
}


static byte measure_write_setup (PLCI   *plci, byte force_write)
{
  struct measure_setup_s   *m;
  byte   *p;
  word i, n;
/*word k;*/

  dbug (1, dprintf ("[%06lx] %s,%d: measure_write_setup",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  m = &(plci->measure_setup);
  p = plci->internal_req_buffer;
  n = 0;
  p[n++] = UDATA_REQUEST_GENERIC_TONE;
  p[n++] = 0;
  if ((m->timer.periode != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_TIMER_SETUP;
    p[n++] = 4;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->timer.periode);
    p[n++] = (byte)(m->timer.periode >> 8);
  }
  if ((m->rx_selector.path != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_RXSELECT_SETUP;
    p[n++] = 4;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->rx_selector.path);
    p[n++] = (byte)(m->rx_selector.path >> 8);
  }
  if ((m->rx_filter.curve_points != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_RXFILTER_SETUP;
    p[n++] = 6 + 4 * m->rx_filter.curve_points;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->rx_filter.curve_points);
    p[n++] = (byte)(m->rx_filter.curve_points >> 8);
    for (i = 0; i < m->rx_filter.curve_points; i++)
    {
      p[n++] = (byte)(m->rx_filter.point_table[i].frequency);
      p[n++] = (byte)(m->rx_filter.point_table[i].frequency >> 8);
      p[n++] = (byte)(m->rx_filter.point_table[i].attenuation);
      p[n++] = (byte)(m->rx_filter.point_table[i].attenuation >> 8);
    }
  }
  if ((m->spectrum.reserved != 0) || (m->spectrum.flags != 0) || (m->spectrum.peaks != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_SPECTRUM_SETUP;
    p[n++] = 10;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->spectrum.reserved);
    p[n++] = (byte)(m->spectrum.reserved >> 8);
    p[n++] = (byte)(m->spectrum.flags);
    p[n++] = (byte)(m->spectrum.flags >> 8);
    p[n++] = (byte)(m->spectrum.peaks);
    p[n++] = (byte)(m->spectrum.peaks >> 8);
    p[n++] = (byte)(m->spectrum.aperture);
    p[n++] = (byte)(m->spectrum.aperture >> 8);
  }
  if ((m->cepstrum.reserved != 0) || (m->cepstrum.flags != 0) || (m->cepstrum.peaks != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_CEPSTRUM_SETUP;
    p[n++] = 8;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->cepstrum.reserved);
    p[n++] = (byte)(m->cepstrum.reserved >> 8);
    p[n++] = (byte)(m->cepstrum.flags);
    p[n++] = (byte)(m->cepstrum.flags >> 8);
    p[n++] = (byte)(m->cepstrum.peaks);
    p[n++] = (byte)(m->cepstrum.peaks >> 8);
  }
  if ((m->echo.reserved != 0) || (m->echo.flags != 0) || (m->echo.peaks != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_ECHO_SETUP;
    p[n++] = 14;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->echo.reserved);
    p[n++] = (byte)(m->echo.reserved >> 8);
    p[n++] = (byte)(m->echo.flags);
    p[n++] = (byte)(m->echo.flags >> 8);
    p[n++] = (byte)(m->echo.peaks);
    p[n++] = (byte)(m->echo.peaks >> 8);
    p[n++] = (byte)(m->echo.min_delay);
    p[n++] = (byte)(m->echo.min_delay >> 8);
    p[n++] = (byte)(m->echo.max_delay);
    p[n++] = (byte)(m->echo.max_delay >> 8);
    p[n++] = (byte)(m->echo.accumulate_time);
    p[n++] = (byte)(m->echo.accumulate_time >> 8);
  }
  if ((m->single_tone.reserved != 0) || (m->single_tone.flags != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_SNGLTONE_SETUP;
    p[n++] = 20;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->single_tone.reserved);
    p[n++] = (byte)(m->single_tone.reserved >> 8);
    p[n++] = (byte)(m->single_tone.flags);
    p[n++] = (byte)(m->single_tone.flags >> 8);
    p[n++] = (byte)(m->single_tone.min_duration);
    p[n++] = (byte)(m->single_tone.min_duration >> 8);
    p[n++] = (byte)(m->single_tone.min_snr);
    p[n++] = (byte)(m->single_tone.min_snr >> 8);
    p[n++] = (byte)(m->single_tone.min_level);
    p[n++] = (byte)(m->single_tone.min_level >> 8);
    p[n++] = (byte)(m->single_tone.max_amplitude_modulation);
    p[n++] = (byte)(m->single_tone.max_amplitude_modulation >> 8);
    p[n++] = (byte)(m->single_tone.max_frequency_modulation);
    p[n++] = (byte)(m->single_tone.max_frequency_modulation >> 8);
    p[n++] = (byte)(m->single_tone.max_dropout);
    p[n++] = (byte)(m->single_tone.max_dropout >> 8);
    p[n++] = (byte)(m->single_tone.aperture);
    p[n++] = (byte)(m->single_tone.aperture >> 8);
  }
  if ((m->dual_tone.reserved != 0) || (m->dual_tone.flags != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_DUALTONE_SETUP;
    p[n++] = 20;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->dual_tone.reserved);
    p[n++] = (byte)(m->dual_tone.reserved >> 8);
    p[n++] = (byte)(m->dual_tone.flags);
    p[n++] = (byte)(m->dual_tone.flags >> 8);
    p[n++] = (byte)(m->dual_tone.min_duration);
    p[n++] = (byte)(m->dual_tone.min_duration >> 8);
    p[n++] = (byte)(m->dual_tone.min_snr);
    p[n++] = (byte)(m->dual_tone.min_snr >> 8);
    p[n++] = (byte)(m->dual_tone.min_level);
    p[n++] = (byte)(m->dual_tone.min_level >> 8);
    p[n++] = (byte)(m->dual_tone.max_high_low_twist);
    p[n++] = (byte)(m->dual_tone.max_high_low_twist >> 8);
    p[n++] = (byte)(m->dual_tone.max_low_high_twist);
    p[n++] = (byte)(m->dual_tone.max_low_high_twist >> 8);
    p[n++] = (byte)(m->dual_tone.max_dropout);
    p[n++] = (byte)(m->dual_tone.max_dropout >> 8);
    p[n++] = (byte)(m->dual_tone.aperture);
    p[n++] = (byte)(m->dual_tone.aperture >> 8);
  }
  if ((m->fskdet.framer_type != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_FSKDET_SETUP;
    p[n++] = 20;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->fskdet.reserved_0);
    p[n++] = (byte)(m->fskdet.reserved_0 >> 8);
    p[n++] = (byte)(m->fskdet.reserved_1);
    p[n++] = (byte)(m->fskdet.reserved_1 >> 8);
    p[n++] = (byte)(m->fskdet.sensitivity);
    p[n++] = (byte)(m->fskdet.sensitivity >> 8);
    p[n++] = (byte)(m->fskdet.space_frequency);
    p[n++] = (byte)(m->fskdet.space_frequency >> 8);
    p[n++] = (byte)(m->fskdet.mark_frequency);
    p[n++] = (byte)(m->fskdet.mark_frequency >> 8);
    p[n++] = (byte)(m->fskdet.baud_rate);
    p[n++] = (byte)(m->fskdet.baud_rate >> 8);
    p[n++] = (byte)(m->fskdet.framer_type);
    p[n++] = (byte)(m->fskdet.framer_type >> 8);
    p[n++] = (byte)(m->fskdet.async_format);
    p[n++] = (byte)(m->fskdet.async_format >> 8);
    p[n++] = (byte)(m->fskdet.framer_options);
    p[n++] = (byte)(m->fskdet.framer_options >> 8);
  }
  if ((m->tx_selector.path != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_TXSELECT_SETUP;
    p[n++] = 4;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->tx_selector.path);
    p[n++] = (byte)(m->tx_selector.path >> 8);
  }
  if ((m->tx_filter.curve_points != 0) || force_write)
  {
    p[n++] = MEASURE_REQUEST_TXFILTER_SETUP;
    p[n++] = 6 + 4 * m->tx_filter.curve_points;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->tx_filter.curve_points);
    p[n++] = (byte)(m->tx_filter.curve_points >> 8);
    for (i = 0; i < m->tx_filter.curve_points; i++)
    {
      p[n++] = (byte)(m->tx_filter.point_table[i].frequency);
      p[n++] = (byte)(m->tx_filter.point_table[i].frequency >> 8);
      p[n++] = (byte)(m->tx_filter.point_table[i].attenuation);
      p[n++] = (byte)(m->tx_filter.point_table[i].attenuation >> 8);
    }
  }
  if (force_write)
  {
    p[n++] = MEASURE_REQUEST_GENERATOR_SETUP;
    p[n++] = 10;
    p[n++] = 0;
    p[n++] = 0;
    p[n++] = (byte)(m->generator.sinegen_mask);
    p[n++] = (byte)(m->generator.sinegen_mask >> 8);
    p[n++] = (byte)(m->generator.funcgen_mask);
    p[n++] = (byte)(m->generator.funcgen_mask >> 8);
    p[n++] = (byte)(m->generator.noisegen_mask);
    p[n++] = (byte)(m->generator.noisegen_mask >> 8);
    p[n++] = (byte)(m->generator.fskgen_mask);
    p[n++] = (byte)(m->generator.fskgen_mask >> 8);
/*
    for (k = 0; k < MEASURE_MODULATOR_COUNT; k++)
    {
      m->modulator[k].curve_points = 0;
      p[n++] = MEASURE_REQUEST_MODULATOR_SETUP;
      p[n++] = 6 + 4 * m->modulator[k].curve_points;
      p[n++] = (byte) k;
      p[n++] = (byte)(k >> 8);
      p[n++] = 0;
      p[n++] = 0;
      p[n++] = (byte)(m->modulator[k].curve_points);
      p[n++] = (byte)(m->modulator[k].curve_points >> 8);
      for (i = 0; i < m->modulator[k].curve_points; i++)
      {
        p[n++] = (byte)(m->modulator[k].point_table[i].d);
        p[n++] = (byte)(m->modulator[k].point_table[i].d >> 8);
        p[n++] = (byte)(m->modulator[k].point_table[i].y);
        p[n++] = (byte)(m->modulator[k].point_table[i].y >> 8);
      }
    }
    for (k = 0; k < MEASURE_FUNCTION_COUNT; k++)
    {
      m->function[k].curve_points = 0;
      p[n++] = MEASURE_REQUEST_FUNCTION_SETUP;
      p[n++] = 6 + 4 * m->function[k].curve_points;
      p[n++] = (byte) k;
      p[n++] = (byte)(k >> 8);
      p[n++] = 0;
      p[n++] = 0;
      p[n++] = (byte)(m->function[k].curve_points);
      p[n++] = (byte)(m->function[k].curve_points >> 8);
      for (i = 0; i < m->function[k].curve_points; i++)
      {
        p[n++] = (byte)(m->function[k].point_table[i].d);
        p[n++] = (byte)(m->function[k].point_table[i].d >> 8);
        p[n++] = (byte)(m->function[k].point_table[i].y);
        p[n++] = (byte)(m->function[k].point_table[i].y >> 8);
      }
    }
    for (k = 0; k < MEASURE_SINEGEN_COUNT; k++)
    {
      p[n++] = MEASURE_REQUEST_SINEGEN_SETUP;
      p[n++] = 6;
      p[n++] = (byte) k;
      p[n++] = (byte)(k >> 8);
      p[n++] = (byte)(m->sinegen[k].amplitude_mod);
      p[n++] = (byte)(m->sinegen[k].amplitude_mod >> 8);
      p[n++] = (byte)(m->sinegen[k].frequency_mod);
      p[n++] = (byte)(m->sinegen[k].frequency_mod >> 8);
    }
    for (k = 0; k < MEASURE_FUNCGEN_COUNT; k++)
    {
      p[n++] = MEASURE_REQUEST_FUNCGEN_SETUP;
      p[n++] = 6;
      p[n++] = (byte) k;
      p[n++] = (byte)(k >> 8);
      p[n++] = (byte)(m->funcgen[k].function);
      p[n++] = (byte)(m->funcgen[k].function >> 8);
      p[n++] = (byte)(m->funcgen[k].amplitude_mod);
      p[n++] = (byte)(m->funcgen[k].amplitude_mod >> 8);
      p[n++] = (byte)(m->funcgen[k].frequency_mod);
      p[n++] = (byte)(m->funcgen[k].frequency_mod >> 8);
    }
    for (k = 0; k < MEASURE_NOISEGEN_COUNT; k++)
    {
      p[n++] = MEASURE_REQUEST_NOISEGEN_SETUP;
      p[n++] = 6;
      p[n++] = (byte) k;
      p[n++] = (byte)(k >> 8);
      p[n++] = (byte)(m->noisegen[k].function);
      p[n++] = (byte)(m->noisegen[k].function >> 8);
      p[n++] = (byte)(m->noisegen[k].amplitude_mod);
      p[n++] = (byte)(m->noisegen[k].amplitude_mod >> 8);
    }
    for (k = 0; k < MEASURE_FSKGEN_COUNT; k++)
    {
      p[n++] = MEASURE_REQUEST_FSKGEN_SETUP;
      p[n++] = 20;
      p[n++] = 0;
      p[n++] = 0;
      p[n++] = (byte)(m->fskgen[k].reserved_0);
      p[n++] = (byte)(m->fskgen[k].reserved_0 >> 8);
      p[n++] = (byte)(m->fskgen[k].reserved_1);
      p[n++] = (byte)(m->fskgen[k].reserved_1 >> 8);
      p[n++] = (byte)(m->fskgen[k].transmit_gain);
      p[n++] = (byte)(m->fskgen[k].transmit_gain >> 8);
      p[n++] = (byte)(m->fskgen[k].space_frequency);
      p[n++] = (byte)(m->fskgen[k].space_frequency >> 8);
      p[n++] = (byte)(m->fskgen[k].mark_frequency);
      p[n++] = (byte)(m->fskgen[k].mark_frequency >> 8);
      p[n++] = (byte)(m->fskgen[k].baud_rate);
      p[n++] = (byte)(m->fskgen[k].baud_rate >> 8);
      p[n++] = (byte)(m->fskgen[k].framer_type);
      p[n++] = (byte)(m->fskgen[k].framer_type >> 8);
      p[n++] = (byte)(m->fskgen[k].async_format);
      p[n++] = (byte)(m->fskgen[k].async_format >> 8);
      p[n++] = (byte)(m->fskgen[k].framer_options);
      p[n++] = (byte)(m->fskgen[k].framer_options >> 8);
    }
*/
  }
  if (n <= 2)
    return (FALSE);
  plci->NData[0].PLength = n;
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
  return (TRUE);
}


static void measure_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: measure_clear_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->measure_active = 0;
  measure_clear_setup (plci);
}


static void measure_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: measure_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word measure_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: measure_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word measure_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: measure_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  if (plci->B1_facilities & (B1_FACILITY_MEASURE | B1_FACILITY_GENTONE))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_MEASURE_1:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = plci->adjust_b_command;
        plci->adjust_b_state = ADJUST_B_RESTORE_MEASURE_1;
        break;
      }
      if (!measure_write_setup (plci, FALSE))
        break;
      plci->internal_command = plci->adjust_b_command;
      plci->adjust_b_state = ADJUST_B_RESTORE_MEASURE_2;
      break;
    case ADJUST_B_RESTORE_MEASURE_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore measure setup failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}


static word measure_curve_points (struct measure_setup_s   *m)
{
  word i, n;

  n = 0;
  for (i = 0; i < MEASURE_MODULATOR_COUNT; i++)
    n += m->modulator[i].curve_points;
  for (i = 0; i < MEASURE_FUNCTION_COUNT; i++)
    n += m->function[i].curve_points;
  return (n);
}


static void measure_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
  static byte result[MAX_MSG_SIZE];
    word rbuf[MEASURE_MAX_CONFIRM_SIZE_WORDS];
  struct measure_setup_s   *m;
  word i, j, l, n, o, r, inst, b1_facilities;
  byte *p;

  dbug (1, dprintf ("[%06lx] %s,%d: measure_command %02x %04x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->measure_cmd));

  Info = GOOD;
  r = 1;
  result[r++] = plci->saved_msg.parms[2].info[1];
  result[r++] = plci->saved_msg.parms[2].info[2];
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (plci->measure_cmd)
  {
  case MEASURE_ENABLE_OPERATION:
    switch (internal_command)
    {
    default:
      b1_facilities = (plci->B1_facilities &
        ~(B1_FACILITY_MEASURE | B1_FACILITY_GENTONE)) | plci->measure_active;
      adjust_b1_resource (Id, plci, NULL, b1_facilities, MEASURE_COMMAND_1);
    case MEASURE_COMMAND_1:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Load measure failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
    case MEASURE_COMMAND_2:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = MEASURE_COMMAND_2;
        return;
      }
      if (plci->saved_msg.parms[2].length > 2)
      {
        plci->internal_req_buffer[0] = UDATA_REQUEST_GENERIC_TONE;
        plci->internal_req_buffer[1] = 0;
        for (i = 2; i < plci->saved_msg.parms[2].length; i++)
          plci->internal_req_buffer[i] = plci->saved_msg.parms[2].info[1+i];
        plci->NData[0].PLength = plci->saved_msg.parms[2].length;
        plci->NData[0].P = plci->internal_req_buffer;
        plci->NL.X = plci->NData;
        plci->NL.ReqCh = 0;
        plci->NL.Req = plci->nl_req = (byte) N_UDATA;
        trcq (plci);
        plci->adapter->request (&plci->NL);
        plci->internal_command = MEASURE_COMMAND_3;
        return;
      }
      break;
    case MEASURE_COMMAND_3:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Enable measure failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
    if (Info == GOOD)
    {
      m = &(plci->measure_setup);
      n = 3;
      while (n <= plci->saved_msg.parms[2].length)
      {
        result[r] = plci->saved_msg.parms[2].info[n];
        p = &(plci->saved_msg.parms[2].info[n]);
        l = p[1];
        inst = ((l >= 2) ? p[2] | (p[3] << 8) : 0) & MEASURE_INFO_INSTANCE_MASK;
        rbuf[0] = inst;
        switch (plci->saved_msg.parms[2].info[n])
        {
        case MEASURE_REQUEST_TIMER_SETUP:
          m->timer.periode = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          if ((inst >= 1)
           || (m->timer.periode >= MEASURE_SUPPORTED_TIMER_PERIODE))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 4)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->timer.periode;
          l = 4;
          break;

        case MEASURE_REQUEST_RXSELECT_SETUP:
          m->rx_selector.path = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          if (inst >= 1)
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          else if (l > 4)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->rx_selector.path;
          l = 4;
          break;

        case MEASURE_REQUEST_RXFILTER_SETUP:
          o = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          i = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->rx_filter.curve_points = 0;
          if (inst >= 1)
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          else if (l < 6 + 4 * i)
            rbuf[0] |= MEASURE_INFO_FATAL_MISSING_PARAMETERS;
          else
          {
            if (l > 6 + 4 * i)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            if (o + i <= MEASURE_MAX_RX_FILTER_POINTS)
            {
              m->rx_filter.curve_points = o + i;
              for (j = 0; j < i; j++)
              {
                m->rx_filter.point_table[o+j].frequency = p[8 + 4 * j] | (p[9 + 4 * j] << 8);
                m->rx_filter.point_table[o+j].attenuation = p[10 + 4 * j] | (p[11 + 4 * j] << 8);
              }
            }
          }
          rbuf[1] = i;
          l = 4;
          break;

        case MEASURE_REQUEST_SPECTRUM_SETUP:
          m->spectrum.reserved = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->spectrum.flags = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->spectrum.peaks = (l >= 8) ? p[8] | (p[9] << 8) : 0;
          m->spectrum.aperture = (l >= 10) ? p[10] | (p[11] << 8) : 0;
          if ((inst >= 1)
           || (m->spectrum.flags & ~MEASURE_SUPPORTED_SPECTRUM_FLAGS)
           || (m->spectrum.peaks > MEASURE_SUPPORTED_SPECTRUM_PEAKS)
           || (m->spectrum.aperture > MEASURE_SUPPORTED_SPECTRUM_APERTURE))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 10)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->spectrum.reserved;
          rbuf[2] = m->spectrum.flags;
          rbuf[3] = m->spectrum.peaks;
          rbuf[4] = m->spectrum.aperture;
          l = 10;
          break;

        case MEASURE_REQUEST_CEPSTRUM_SETUP:
          m->cepstrum.reserved = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->cepstrum.flags = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->cepstrum.peaks = (l >= 8) ? p[8] | (p[9] << 8) : 0;
          if ((inst >= 1)
           || (m->cepstrum.flags & ~MEASURE_SUPPORTED_CEPSTRUM_FLAGS)
           || (m->cepstrum.peaks > MEASURE_SUPPORTED_CEPSTRUM_PEAKS))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 8)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->cepstrum.reserved;
          rbuf[2] = m->cepstrum.flags;
          rbuf[3] = m->cepstrum.peaks;
          l = 8;
          break;

        case MEASURE_REQUEST_ECHO_SETUP:
          m->echo.reserved = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->echo.flags = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->echo.peaks = (l >= 8) ? p[8] | (p[9] << 8) : 0;
          m->echo.min_delay = (l >= 10) ? p[10] | (p[11] << 8) : MEASURE_ECHO_MIN_DELAY_DEFAULT;
          m->echo.max_delay = (l >= 12) ? p[12] | (p[13] << 8) : MEASURE_ECHO_MAX_DELAY_DEFAULT;
          m->echo.accumulate_time = (word)((l >= 14) ? p[14] | (p[15] << 8) : MEASURE_ECHO_ACCUMULATE_TIME_DEFAULT);
          if ((inst >= 1)
           || (m->echo.flags & ~MEASURE_SUPPORTED_ECHO_FLAGS)
           || (m->echo.peaks > MEASURE_SUPPORTED_ECHO_PEAKS)
           || (m->echo.min_delay > MEASURE_SUPPORTED_ECHO_DELAY)
           || (m->echo.max_delay > MEASURE_SUPPORTED_ECHO_DELAY))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 14)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->echo.reserved;
          rbuf[2] = m->echo.flags;
          rbuf[3] = m->echo.peaks;
          rbuf[4] = m->echo.min_delay;
          rbuf[5] = m->echo.max_delay;
          rbuf[6] = m->echo.accumulate_time;
          l = 14;
          break;

        case MEASURE_REQUEST_SNGLTONE_SETUP:
          m->single_tone.reserved = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->single_tone.flags = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->single_tone.min_duration = (l >= 8) ? p[8] | (p[9] << 8) : MEASURE_SNGLTONE_MIN_DURATION_DEFAULT;
          m->single_tone.min_snr = (l >= 10) ? p[10] | (p[11] << 8) : MEASURE_SNGLTONE_MIN_SNR_DEFAULT;
          m->single_tone.min_level = (l >= 12) ? p[12] | (p[13] << 8) : MEASURE_SNGLTONE_MIN_LEVEL_DEFAULT;
          m->single_tone.max_amplitude_modulation = (l >= 14) ? p[14] | (p[15] << 8) : MEASURE_SNGLTONE_MAX_AMPL_MOD_DEFAULT;
          m->single_tone.max_frequency_modulation = (l >= 16) ? p[16] | (p[17] << 8) : MEASURE_SNGLTONE_MAX_FREQ_MOD_DEFAULT;
          m->single_tone.max_dropout = (l >= 18) ? p[18] | (p[19] << 8) : 0;
          m->single_tone.aperture = (l >= 20) ? p[20] | (p[21] << 8) : 0;
          if ((inst >= 1)
           || (m->single_tone.flags & ~MEASURE_SUPPORTED_SNGLTONE_FLAGS)
           || (m->single_tone.min_duration > MEASURE_SUPPORTED_SNGLTONE_DURATION)
           || (m->single_tone.max_frequency_modulation > MEASURE_SUPPORTED_SNGLTONE_FREQ_MOD)
           || (m->single_tone.aperture > MEASURE_SUPPORTED_SNGLTONE_APERTURE))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 20)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->single_tone.reserved;
          rbuf[2] = m->single_tone.flags;
          rbuf[3] = m->single_tone.min_duration;
          rbuf[4] = m->single_tone.min_snr;
          rbuf[5] = m->single_tone.min_level;
          rbuf[6] = m->single_tone.max_amplitude_modulation;
          rbuf[7] = m->single_tone.max_frequency_modulation;
          rbuf[8] = m->single_tone.max_dropout;
          rbuf[9] = m->single_tone.aperture;
          l = 20;
          break;

        case MEASURE_REQUEST_DUALTONE_SETUP:
          m->dual_tone.reserved = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->dual_tone.flags = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->dual_tone.min_duration = (l >= 8) ? p[8] | (p[9] << 8) : MEASURE_DUALTONE_MIN_DURATION_DEFAULT;
          m->dual_tone.min_snr = (l >= 10) ? p[10] | (p[11] << 8) : MEASURE_DUALTONE_MIN_SNR_DEFAULT;
          m->dual_tone.min_level = (l >= 12) ? p[12] | (p[13] << 8) : MEASURE_DUALTONE_MIN_LEVEL_DEFAULT;
          m->dual_tone.max_high_low_twist = (l >= 14) ? p[14] | (p[15] << 8) : MEASURE_DUALTONE_MAX_HL_TWIST_DEFAULT;
          m->dual_tone.max_low_high_twist = (l >= 16) ? p[16] | (p[17] << 8) : MEASURE_DUALTONE_MAX_LH_TWIST_DEFAULT;
          m->dual_tone.max_dropout = (l >= 18) ? p[18] | (p[19] << 8) : 0;
          m->dual_tone.aperture = (l >= 20) ? p[20] | (p[21] << 8) : 0;
          if ((inst >= 1)
           || (m->dual_tone.flags & ~MEASURE_SUPPORTED_DUALTONE_FLAGS)
           || (m->dual_tone.min_duration > MEASURE_SUPPORTED_DUALTONE_DURATION)
           || (m->dual_tone.aperture > MEASURE_SUPPORTED_DUALTONE_APERTURE))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 20)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->dual_tone.reserved;
          rbuf[2] = m->dual_tone.flags;
          rbuf[3] = m->dual_tone.min_duration;
          rbuf[4] = m->dual_tone.min_snr;
          rbuf[5] = m->dual_tone.min_level;
          rbuf[6] = m->dual_tone.max_high_low_twist;
          rbuf[7] = m->dual_tone.max_low_high_twist;
          rbuf[8] = m->dual_tone.max_dropout;
          rbuf[9] = m->dual_tone.aperture;
          l = 20;
          break;

        case MEASURE_REQUEST_FSKDET_SETUP:
          m->fskdet.reserved_0 = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->fskdet.reserved_1 = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->fskdet.sensitivity = (l >= 8) ? p[8] | (p[9] << 8) : 0;
          m->fskdet.space_frequency = (l >= 10) ? p[10] | (p[11] << 8) : 0;
          m->fskdet.mark_frequency = (l >= 12) ? p[12] | (p[13] << 8) : 0;
          m->fskdet.baud_rate = (l >= 14) ? p[14] | (p[15] << 8) : 0;
          m->fskdet.framer_type = (l >= 16) ? p[16] | (p[17] << 8) : 0;
          m->fskdet.async_format = (l >= 18) ? p[18] | (p[19] << 8) : 0;
          m->fskdet.framer_options = (l >= 20) ? p[20] | (p[21] << 8) : 0;
          if ((inst >= 1)
           || (m->fskdet.space_frequency > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
           || (m->fskdet.mark_frequency > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
           || (m->fskdet.baud_rate > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
           || (m->fskdet.framer_type & ~MEASURE_FSKDET_SUPPORTED_FRAMERS)
           || (m->fskdet.async_format & ~MEASURE_FSKDET_SUPPORTED_ASYNC_FORMAT)
           || (m->fskdet.framer_options & ~MEASURE_FSKDET_SUPPORTED_FRAMER_OPTIONS))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 20)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->fskdet.reserved_0;
          rbuf[2] = m->fskdet.reserved_1;
          rbuf[3] = m->fskdet.sensitivity;
          rbuf[4] = m->fskdet.space_frequency;
          rbuf[5] = m->fskdet.mark_frequency;
          rbuf[6] = m->fskdet.baud_rate;
          rbuf[7] = m->fskdet.framer_type;
          rbuf[8] = m->fskdet.async_format;
          rbuf[9] = m->fskdet.framer_options;
          l = 20;
          break;

        case MEASURE_REQUEST_TXSELECT_SETUP:
          m->tx_selector.path = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          if (inst >= 1)
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          else if (l > 4)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->tx_selector.path;
          l = 4;
          break;

        case MEASURE_REQUEST_TXFILTER_SETUP:
          o = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          i = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->tx_filter.curve_points = 0;
          if (inst >= 1)
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          else if (l < 6 + 4 * i)
            rbuf[0] |= MEASURE_INFO_FATAL_MISSING_PARAMETERS;
          else
          {
            if (l > 6 + 4 * i)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            if (o + i <= MEASURE_MAX_TX_FILTER_POINTS)
            {
              m->tx_filter.curve_points = o + i;
              for (j = 0; j < i; j++)
              {
                m->tx_filter.point_table[o+j].frequency = p[8 + 4 * j] | (p[9 + 4 * j] << 8);
                m->tx_filter.point_table[o+j].attenuation = p[10 + 4 * j] | (p[11 + 4 * j] << 8);
              }
            }
          }
          rbuf[1] = i;
          l = 4;
          break;

        case MEASURE_REQUEST_GENERATOR_SETUP:
          m->generator.sinegen_mask = (l >= 4) ? p[4] | (p[5] << 8) : 0;
          m->generator.funcgen_mask = (l >= 6) ? p[6] | (p[7] << 8) : 0;
          m->generator.noisegen_mask = (l >= 8) ? p[8] | (p[9] << 8) : 0;
          m->generator.fskgen_mask = (l >= 10) ? p[10] | (p[11] << 8) : 0;
          if ((inst >= 1)
           || (m->generator.sinegen_mask & (0xffff << MEASURE_SINEGEN_COUNT))
           || (m->generator.funcgen_mask & (0xffff << MEASURE_FUNCGEN_COUNT))
           || (m->generator.noisegen_mask & (0xffff << MEASURE_NOISEGEN_COUNT))
           || (m->generator.fskgen_mask & (0xffff << MEASURE_FSKGEN_COUNT)))
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
          }
          else if (l > 10)
            rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
          rbuf[1] = m->generator.sinegen_mask;
          rbuf[2] = m->generator.funcgen_mask;
          rbuf[3] = m->generator.noisegen_mask;
          rbuf[4] = m->generator.fskgen_mask;
          l = 10;
          break;

        case MEASURE_REQUEST_MODULATOR_SETUP:
          if (inst >= MEASURE_MODULATOR_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            o = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            i = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            m->modulator[inst].curve_points = 0;
            if (l < 6 + 4 * i)
              rbuf[0] |= MEASURE_INFO_FATAL_MISSING_PARAMETERS;
            else if (measure_curve_points (m) + o + i > MEASURE_SUPPORTED_CURVE_POINTS)
              rbuf[0] |= MEASURE_INFO_FATAL_RESOURCE_ERROR;
            else
            {
              if (l > 6 + 4 * i)
                rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
              m->modulator[inst].curve_points = o + i;
            }
            rbuf[1] = i;
            l = 4;
          }
          break;

        case MEASURE_REQUEST_FUNCTION_SETUP:
          if (inst >= MEASURE_FUNCTION_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            o = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            i = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            m->function[inst].curve_points = 0;
            if (l < 6 + 4 * i)
              rbuf[0] |= MEASURE_INFO_FATAL_MISSING_PARAMETERS;
            else if (measure_curve_points (m) + o + i > MEASURE_SUPPORTED_CURVE_POINTS)
              rbuf[0] |= MEASURE_INFO_FATAL_RESOURCE_ERROR;
            else
            {
              if (l > 6 + 4 * i)
                rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
              m->function[inst].curve_points = o + i;
            }
            rbuf[1] = i;
            l = 4;
          }
          break;

        case MEASURE_REQUEST_SINEGEN_SETUP:
          if (inst >= MEASURE_SINEGEN_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            m->sinegen[inst].amplitude_mod = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            m->sinegen[inst].frequency_mod = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            if ((m->sinegen[inst].amplitude_mod > MEASURE_MODULATOR_COUNT)
             || (m->sinegen[inst].frequency_mod > MEASURE_MODULATOR_COUNT))
            {
              rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            }
            else if (l > 6)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            rbuf[1] = m->sinegen[inst].amplitude_mod;
            rbuf[2] = m->sinegen[inst].frequency_mod;
            l = 6;
          }
          break;

        case MEASURE_REQUEST_FUNCGEN_SETUP:
          if (inst >= MEASURE_FUNCGEN_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            m->funcgen[inst].function = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            m->funcgen[inst].amplitude_mod = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            m->funcgen[inst].frequency_mod = (l >= 8) ? p[8] | (p[9] << 8) : 0;
            if ((m->funcgen[inst].function > MEASURE_FUNCTION_COUNT)
             || (m->funcgen[inst].amplitude_mod > MEASURE_MODULATOR_COUNT)
             || (m->funcgen[inst].frequency_mod > MEASURE_MODULATOR_COUNT))
            {
              rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            }
            else if (l > 8)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            rbuf[1] = m->funcgen[inst].function;
            rbuf[2] = m->funcgen[inst].amplitude_mod;
            rbuf[3] = m->funcgen[inst].frequency_mod;
            l = 8;
          }
          break;

        case MEASURE_REQUEST_NOISEGEN_SETUP:
          if (inst >= MEASURE_NOISEGEN_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            m->noisegen[inst].function = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            m->noisegen[inst].amplitude_mod = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            if ((m->noisegen[inst].function > MEASURE_FUNCTION_COUNT)
             || (m->noisegen[inst].amplitude_mod > MEASURE_MODULATOR_COUNT))
            {
              rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            }
            else if (l > 6)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            rbuf[1] = m->noisegen[inst].function;
            rbuf[2] = m->noisegen[inst].amplitude_mod;
            l = 6;
          }
          break;

        case MEASURE_REQUEST_FSKGEN_SETUP:
          if (inst >= MEASURE_FSKGEN_COUNT)
          {
            rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            l = 2;
          }
          else
          {
            m->fskgen[inst].reserved_0 = (l >= 4) ? p[4] | (p[5] << 8) : 0;
            m->fskgen[inst].reserved_1 = (l >= 6) ? p[6] | (p[7] << 8) : 0;
            m->fskgen[inst].transmit_gain = (l >= 8) ? p[8] | (p[9] << 8) : 0;
            m->fskgen[inst].space_frequency = (l >= 10) ? p[10] | (p[11] << 8) : 0;
            m->fskgen[inst].mark_frequency = (l >= 12) ? p[12] | (p[13] << 8) : 0;
            m->fskgen[inst].baud_rate = (l >= 14) ? p[14] | (p[15] << 8) : 0;
            m->fskgen[inst].framer_type = (l >= 16) ? p[16] | (p[17] << 8) : 0;
            m->fskgen[inst].async_format = (l >= 18) ? p[18] | (p[19] << 8) : 0;
            m->fskgen[inst].framer_options = (l >= 20) ? p[20] | (p[21] << 8) : 0;
            if ((m->fskgen[inst].space_frequency > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
             || (m->fskgen[inst].mark_frequency > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
             || (m->fskgen[inst].baud_rate > MEASURE_FSKDET_SUPPORTED_FREQUENCY)
             || (m->fskgen[inst].framer_type & ~MEASURE_FSKDET_SUPPORTED_FRAMERS)
             || (m->fskgen[inst].async_format & ~MEASURE_FSKDET_SUPPORTED_ASYNC_FORMAT)
             || (m->fskgen[inst].framer_options & ~MEASURE_FSKDET_SUPPORTED_FRAMER_OPTIONS))
            {
              rbuf[0] |= MEASURE_INFO_FATAL_PARAMETER_ERROR;
            }
            else if (l > 20)
              rbuf[0] |= MEASURE_INFO_IGNORED_PARAMETERS_FLAG;
            rbuf[1] = m->fskgen[inst].reserved_0;
            rbuf[2] = m->fskgen[inst].reserved_1;
            rbuf[3] = m->fskgen[inst].transmit_gain;
            rbuf[4] = m->fskgen[inst].space_frequency;
            rbuf[5] = m->fskgen[inst].mark_frequency;
            rbuf[6] = m->fskgen[inst].baud_rate;
            rbuf[7] = m->fskgen[inst].framer_type;
            rbuf[8] = m->fskgen[inst].async_format;
            rbuf[9] = m->fskgen[inst].framer_options;
            l = 20;
          }
          break;

        default:
          rbuf[0] |= MEASURE_INFO_FATAL_UNKNOWN_REQUEST;
          l = 2;
        }
        if (r + 2 + l > sizeof(result))
        {
          result[0] = r - 1;
          sendf (plci->appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, plci->number,
            "dwws", _DI_MANU_ID, READ_WORD (plci->saved_msg.parms[1].info), Info, result);
          r = 3;
        }
        result[r+1] = (byte) l;
        for (i = 0; 2*i < l; i++)
        {
          result[r + 2 + 2*i] = (byte)(rbuf[i]);
          result[r + 3 + 2*i] = (byte)(rbuf[i] >> 8);
        }
        r += 2 + l;
        n += 2 + plci->saved_msg.parms[2].info[n+1];
      }
    }
    break;

  case MEASURE_DISABLE_OPERATION:
    switch (internal_command)
    {
    default:
      if (plci->measure_active == 0)
        measure_clear_setup (plci);
    case MEASURE_COMMAND_1:
      b1_facilities = (plci->B1_facilities &
        ~(B1_FACILITY_MEASURE | B1_FACILITY_GENTONE)) | plci->measure_active;
      if (add_b1_facilities (plci, plci->B1_resource, b1_facilities) ==
          add_b1_facilities (plci, plci->B1_resource, plci->B1_facilities))
      {
        if (plci_nl_busy (plci))
        {
          plci->internal_command = MEASURE_COMMAND_1;
          return;
        }
        plci->internal_command = MEASURE_COMMAND_2;
        measure_write_setup (plci, TRUE);
        return;
      }
      Rc = OK;
    case MEASURE_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Disable measure failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      b1_facilities = (plci->B1_facilities &
        ~(B1_FACILITY_MEASURE | B1_FACILITY_GENTONE)) | plci->measure_active;
      adjust_b1_resource (Id, plci, NULL, b1_facilities, MEASURE_COMMAND_3);
    case MEASURE_COMMAND_3:
      if (adjust_b_process (Id, plci, Rc) != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Unload measure failed",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
        break;
      }
      if (plci->internal_command)
        return;
      break;
    }
    break;
  }
  result[0] = r - 1;
  sendf (plci->appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, plci->number,
    "dwws", _DI_MANU_ID, READ_WORD (plci->saved_msg.parms[1].info), Info, result);
}


static void measure_command_support (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, APPL   *appl, API_PARSE *msg)
{
  word Info;
  static byte result[MAX_MSG_SIZE];
  word i, j, l, n, r;
  byte *p;

  dbug (1, dprintf ("[%06lx] %s,%d: measure_command_support",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  r = 1;
  result[r++] = msg[2].info[1];
  result[r++] = msg[2].info[2];
  if (READ_WORD (msg[1].info) == _DI_MEASURE_CTRL)
  {
    p = measure_support_table;
    l = sizeof(measure_support_table);
  }
  else
  {
    p = measure_support_tone_table;
    l = sizeof(measure_support_tone_table);
  }
  n = 3;
  while (n <= msg[2].length)
  {
    i = 0;
    while ((i < l) && (p[i] != msg[2].info[n]))
      i += 2 + p[i+1];
    if (i < l)
    {
      if (r + 2 + p[i+1] > sizeof(result))
      {
        result[0] = r - 1;
        sendf (appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, Number,
          "dwws", _DI_MANU_ID, READ_WORD (msg[1].info), Info, result);
        r = 3;
      }
      for (j = 0; j < 2 + p[i+1]; j++)
        result[r++] = p[i+j];
    }
    else
    {
      if (r + 4 > sizeof(result))
      {
        result[0] = r - 1;
        sendf (appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, Number,
          "dwws", _DI_MANU_ID, READ_WORD (msg[1].info), Info, result);
        r = 3;
      }
      result[r++] = msg[2].info[n];
      result[r++] = 2;
      result[r++] = MEASURE_INFO_FATAL_UNKNOWN_REQUEST & 0xff;
      result[r++] = MEASURE_INFO_FATAL_UNKNOWN_REQUEST >> 8;
    }
    n += 2 + msg[2].info[n+1];
  }
  result[0] = r - 1;
  sendf (appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, Number,
    "dwws", _DI_MANU_ID, READ_WORD (msg[1].info), Info, result);
}


static byte measure_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
    API_PARSE measure_parms[3];
    byte result[4];
  word r;

  dbug (1, dprintf ("[%06lx] %s,%d: measure_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  r = 1;
  if (((READ_WORD (msg[1].info) == _DI_MEASURE_CTRL) && !(a->manufacturer_features & MANUFACTURER_FEATURE_MEASURE))
   || ((READ_WORD (msg[1].info) == _DI_GENERIC_TONE) && !(a->manufacturer_features2 & MANUFACTURER_FEATURE2_GENERIC_TONE)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else
  {
    if (api_parse (&msg[2].info[1], msg[2].length, "w", measure_parms))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      Info = _WRONG_MESSAGE_FORMAT;
    }
    else
    {
      result[r++] = measure_parms[0].info[0];
      result[r++] = measure_parms[0].info[1];
      if (READ_WORD (measure_parms[0].info) == MEASURE_GET_SUPPORTED_SERVICES)
      {
        measure_command_support (Id, Number, a, appl, msg);
        return (FALSE);
      }
      else if (plci == NULL)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_IDENTIFIER;
      }
      else if ((plci->State == IDLE)
       || (plci->State == OUTG_DIS_PENDING)
       || (plci->State == INC_DIS_PENDING)
       || !plci->NL.Id || plci->nl_remove_id)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        Info = _WRONG_STATE;
      }
      else
      {
        plci->command = 0;
        plci->measure_cmd = READ_WORD (measure_parms[0].info);
        api_save_msg (msg, "dws", &plci->saved_msg);
        switch (plci->measure_cmd)
        {
        case MEASURE_ENABLE_OPERATION:
          plci->measure_active |= (READ_WORD (msg[1].info) == _DI_MEASURE_CTRL) ? B1_FACILITY_MEASURE : B1_FACILITY_GENTONE;
          start_internal_command (Id, plci, measure_command);
          return (FALSE);

        case MEASURE_DISABLE_OPERATION:
          plci->measure_active &= (READ_WORD (msg[1].info) == _DI_MEASURE_CTRL) ? ~B1_FACILITY_MEASURE : ~B1_FACILITY_GENTONE;
          start_internal_command (Id, plci, measure_command);
          return (FALSE);

        default:
          dbug (1, dprintf ("[%06lx] %s,%d: Measure unknown request %04x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->measure_cmd));
          Info = _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP;
        }
      }
    }
  }
  result[0] = r - 1;
  sendf (appl, _MANUFACTURER_R | CONFIRM, Id & 0xffff, Number,
    "dwws", _DI_MANU_ID, READ_WORD (msg[1].info), Info, result);
  return (FALSE);
}


static void measure_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{
  static byte result[MAX_MSG_SIZE];
  word i, n, r;

  dbug (1, dprintf ("[%06lx] %s,%d: measure_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if (plci->measure_active & B1_FACILITY_GENTONE)
  {
    r = 1;
    result[r++] = MEASURE_ENABLE_OPERATION & 0xff;
    result[r++] = MEASURE_ENABLE_OPERATION >> 8;
    n = 2;
    while (n < length)
    {
      switch (msg[n])
      {
      case MEASURE_INDICATION_TIMER_SETUP:
      case MEASURE_INDICATION_RXSELECT_SETUP:
      case MEASURE_INDICATION_RXFILTER_SETUP:
      case MEASURE_INDICATION_SPECTRUM_SETUP:
      case MEASURE_INDICATION_CEPSTRUM_SETUP:
      case MEASURE_INDICATION_ECHO_SETUP:
      case MEASURE_INDICATION_SNGLTONE_SETUP:
      case MEASURE_INDICATION_DUALTONE_SETUP:
      case MEASURE_INDICATION_FSKDET_SETUP:
      case MEASURE_INDICATION_TXSELECT_SETUP:
      case MEASURE_INDICATION_TXFILTER_SETUP:
      case MEASURE_INDICATION_GENERATOR_SETUP:
      case MEASURE_INDICATION_MODULATOR_SETUP:
      case MEASURE_INDICATION_FUNCTION_SETUP:
      case MEASURE_INDICATION_SINEGEN_SETUP:
      case MEASURE_INDICATION_FUNCGEN_SETUP:
      case MEASURE_INDICATION_NOISEGEN_SETUP:
      case MEASURE_INDICATION_FSKGEN_SETUP:
        break;

      default:
        if (r + 2 + msg[n+1] > sizeof(result))
        {
          result[0] = r - 1;
          sendf (plci->appl, _MANUFACTURER_I, Id & 0xffff, 0,
            "dws", _DI_MANU_ID, _DI_GENERIC_TONE, result);
          r = 3;
        }
        for (i = 0; i < 2 + msg[n+1]; i++)
          result[r++] = msg[n+i];
      }
      n += 2 + msg[n+1];
    }
    if (r > 3)
    {
      result[0] = r - 1;
      sendf (plci->appl, _MANUFACTURER_I, Id & 0xffff, 0,
        "dws", _DI_MANU_ID, _DI_GENERIC_TONE, result);
    }
  }
}



/*------------------------------------------------------------------*/
/* Advanced voice                                                   */
/*------------------------------------------------------------------*/

static void adv_voice_write_coefs (PLCI   *plci, word write_command)
{
  DIVA_CAPI_ADAPTER   *a;
  word i;
  byte *p;

  word w, n, j, k;
  byte ch_map[MIXER_CHANNELS_BRI];

    byte coef_buffer[ADV_VOICE_COEF_BUFFER_SIZE + 2];

  dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_write_coefs %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, write_command));

  a = plci->adapter;
  p = coef_buffer + 1;
  *(p++) = DSP_CTRL_OLD_SET_MIXER_COEFFICIENTS;
  i = 0;
  while (i + sizeof(word) <= a->adv_voice_coef_length)
  {
    WRITE_WORD (p, READ_WORD (a->adv_voice_coef_buffer + i));
    p += 2;
    i += 2;
  }
  while (i < ADV_VOICE_OLD_COEF_COUNT * sizeof(word))
  {
    WRITE_WORD (p, 0x8000);
    p += 2;
    i += 2;
  }

  if (!a->li_pri && (plci->li_bchannel_id == 0))
  {
    j = FALSE;
    k = FALSE;
    for (i = 0; i < a->max_plci; i++)
    {
      if (a->plci[i].State
       && a->plci[i].NL.Id && !a->plci[i].nl_remove_id)
      {
        switch (a->plci[i].li_bchannel_id)
        {
        case 1:
          j = TRUE;
          break;
        case 2:
          k = TRUE;
          break;
        }
      }
    }
    if (!j && k)
    {
      plci->li_bchannel_id = 1;
      dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_set_bchannel_id %d",
        (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
        (char   *)(FILE_), __LINE__, plci->li_bchannel_id));
    }
    else if (j && !k)
    {
      plci->li_bchannel_id = 2;
      dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_set_bchannel_id %d",
        (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
        (char   *)(FILE_), __LINE__, plci->li_bchannel_id));
    }
  }
  if (!a->li_pri && (plci->li_bchannel_id != 0))
  {
    i = a->li_base + (plci->li_bchannel_id - 1);
    switch (write_command)
    {
    case ADV_VOICE_WRITE_ACTIVATION:
    case ADV_VOICE_WRITE_UPDATE:
      j = a->li_base + MIXER_IC_CHANNEL_BASE + (plci->li_bchannel_id - 1);
      k = a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id);
      if (!(plci->B1_facilities & B1_FACILITY_MIXER))
      {
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 2))
        {
          if ((a->adv_voice_coef_length < 2) || (READ_WORD (a->adv_voice_coef_buffer + 0) != 0))
            li_config_table[j].flag_table[i] |= LI_FLAG_CONFERENCE;
          else
            li_config_table[j].flag_table[i] &= ~LI_FLAG_CONFERENCE;
        }
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 4))
        {
          if ((a->adv_voice_coef_length < 4) || (READ_WORD (a->adv_voice_coef_buffer + 2) != 0))
            li_config_table[j].flag_table[i] |= LI_FLAG_MIX;
          else
            li_config_table[j].flag_table[i] &= ~LI_FLAG_MIX;
        }
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 6))
        {
          if ((a->adv_voice_coef_length < 6) || (READ_WORD (a->adv_voice_coef_buffer + 4) != 0))
            li_config_table[i].flag_table[j] |= LI_FLAG_CONFERENCE;
          else
            li_config_table[i].flag_table[j] &= ~LI_FLAG_CONFERENCE;
        }
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 8))
        {
          if ((a->adv_voice_coef_length < 8) || (READ_WORD (a->adv_voice_coef_buffer + 6) != 0))
            li_config_table[i].flag_table[i] |= LI_FLAG_MIX;
          else
            li_config_table[i].flag_table[i] &= ~LI_FLAG_MIX;
        }
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 10))
        {
          if ((a->adv_voice_coef_length < 10) || (READ_WORD (a->adv_voice_coef_buffer + 8) != 0))
            li_config_table[i].flag_table[i] |= LI_FLAG_MONITOR;
          else
            li_config_table[i].flag_table[i] &= ~LI_FLAG_MONITOR;
        }
        if ((write_command == ADV_VOICE_WRITE_ACTIVATION) || (a->adv_voice_coef_length >= 12))
        {
          if ((a->adv_voice_coef_length < 12) || (READ_WORD (a->adv_voice_coef_buffer + 10) != 0))
            li_config_table[i].flag_table[j] |= LI_FLAG_MONITOR;
          else
            li_config_table[i].flag_table[j] &= ~LI_FLAG_MONITOR;
        }
      }
      if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
      {
        li_config_table[k].flag_table[i] = (li_config_table[k].flag_table[i] & ~(LI_FLAG_CONFERENCE | LI_FLAG_MIX)) |
          (li_config_table[j].flag_table[i] & (LI_FLAG_CONFERENCE | LI_FLAG_MIX));
        li_config_table[i].flag_table[k] = (li_config_table[i].flag_table[k] & ~(LI_FLAG_CONFERENCE | LI_FLAG_MONITOR)) |
          (li_config_table[i].flag_table[j] & (LI_FLAG_CONFERENCE | LI_FLAG_MONITOR));
        li_config_table[k].flag_table[j] |= LI_FLAG_CONFERENCE;
        li_config_table[j].flag_table[k] |= LI_FLAG_CONFERENCE;
      }
      mixer_calculate_coefs (a);
      li_config_table[i].curchnl = li_config_table[i].channel;
      li_config_table[j].curchnl = li_config_table[j].channel;
      if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
        li_config_table[k].curchnl = li_config_table[k].channel;
      break;

    case ADV_VOICE_WRITE_DEACTIVATION:
      for (j = 0; j < li_total_channels; j++)
      {
        li_config_table[i].flag_table[j] = 0;
        li_config_table[j].flag_table[i] = 0;
      }
      k = a->li_base + MIXER_IC_CHANNEL_BASE + (plci->li_bchannel_id - 1);
      for (j = 0; j < li_total_channels; j++)
      {
        li_config_table[k].flag_table[j] = 0;
        li_config_table[j].flag_table[k] = 0;
      }
      if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
      {
        k = a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id);
        for (j = 0; j < li_total_channels; j++)
        {
          li_config_table[k].flag_table[j] = 0;
          li_config_table[j].flag_table[k] = 0;
        }
      }
      mixer_calculate_coefs (a);
      break;
    }
    if (plci->B1_facilities & (B1_FACILITY_VOICE | B1_FACILITY_MIXER))
    {
      w = ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION) ?
        0 : MIXER_FEATURE_ENABLE_TX_DATA | MIXER_FEATURE_ENABLE_RX_DATA;
      if (ADV_VOICE_NEW_COEF_BASE + sizeof(word) <= a->adv_voice_coef_length)
        w |= READ_WORD (a->adv_voice_coef_buffer + ADV_VOICE_NEW_COEF_BASE);
      if (li_config_table[i].channel & LI_CHANNEL_TX_DATA)
        w |= MIXER_FEATURE_ENABLE_TX_DATA;
      if (li_config_table[i].channel & LI_CHANNEL_RX_DATA)
        w |= MIXER_FEATURE_ENABLE_RX_DATA;
      if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_PC))
        w |= MIXER_FEATURE_UNUSED_SOURCE_PC;
      if (!(li_config_table[i].channel & LI_CHANNEL_USED_SOURCE_B))
        w |= MIXER_FEATURE_UNUSED_SOURCE_B;
      *(p++) = (byte) w;
      *(p++) = (byte)(w >> 8);
      for (j = 0; j < sizeof(ch_map); j += 2)
      {
        ch_map[j] = (byte)(j + (plci->li_bchannel_id - 1));
        ch_map[j+1] = (byte)(j + (2 - plci->li_bchannel_id));
      }
      for (n = 0; n < sizeof(mixer_write_prog_bri) / sizeof(mixer_write_prog_bri[0]); n++)
      {
        i = a->li_base + ch_map[mixer_write_prog_bri[n].to_ch];
        j = a->li_base + ch_map[mixer_write_prog_bri[n].from_ch];
        if (li_config_table[i].channel & li_config_table[j].channel & LI_CHANNEL_INVOLVED)
        {
          *(p++) = ((li_config_table[i].coef_table[j] & mixer_write_prog_bri[n].mask) ? 0x80 : 0x01);
          w = ((li_config_table[i].coef_table[j] & 0xf) ^ (li_config_table[i].coef_table[j] >> 4));
          li_config_table[i].coef_table[j] ^= (w & mixer_write_prog_bri[n].mask) << 4;
        }
        else
        {
          *(p++) = (ADV_VOICE_NEW_COEF_BASE + sizeof(word) + n < a->adv_voice_coef_length) ?
            a->adv_voice_coef_buffer[ADV_VOICE_NEW_COEF_BASE + sizeof(word) + n] : 0x00;
        }
      }
    }
    else
    {
      for (i = ADV_VOICE_NEW_COEF_BASE; i < a->adv_voice_coef_length; i++)
        *(p++) = a->adv_voice_coef_buffer[i];
    }
  }
  else

  {
    for (i = ADV_VOICE_NEW_COEF_BASE; i < a->adv_voice_coef_length; i++)
      *(p++) = a->adv_voice_coef_buffer[i];
  }
  coef_buffer[0] = (byte)((p - coef_buffer) - 1);
  add_p (plci, FTY, coef_buffer);
  sig_req (plci, TEL_CTRL, 0);
  send_req (plci);
}


static void adv_voice_clear_config (PLCI   *plci)
{
  DIVA_CAPI_ADAPTER   *a;

  word i, j;


  dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_clear_config %d %d %d %02x %08lx",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__,
    (int)(plci->tel), (plci == plci->adapter->AdvSignalPLCI), (int)(plci->li_bchannel_id),
    (unsigned int)(plci->li_channel_bits), (unsigned long)(plci->adapter->manufacturer_features)));

  a = plci->adapter;
  if ((plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI))
  {
    a->adv_voice_coef_length = 0;

    if (!a->li_pri && (plci->li_bchannel_id != 0))
    {
      i = a->li_base + (plci->li_bchannel_id - 1);
      li_config_table[i].curchnl = 0;
      li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
      li_config_table[i].chflags = 0;
      for (j = 0; j < li_total_channels; j++)
      {
        if ((li_config_table[i].flag_table[j] != 0)
         || (li_config_table[j].flag_table[i] != 0))
        {
          li_config_table[i].flag_table[j] = 0;
          li_config_table[j].flag_table[i] = 0;
          if (j != i)
            mixer_update_uninvolve (j);
        }
        li_config_table[i].coef_table[j] = 0;
        li_config_table[j].coef_table[i] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
      }
      li_config_table[i].flag_table[i] = LI_FLAG_MONITOR | LI_FLAG_MIX;
      li_config_table[i].coef_table[i] |= LI_COEF_CH_PC_SET | LI_COEF_PC_CH_SET;
      i = a->li_base + MIXER_IC_CHANNEL_BASE + (plci->li_bchannel_id - 1);
      li_config_table[i].curchnl = 0;
      li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
      li_config_table[i].chflags = 0;
      for (j = 0; j < li_total_channels; j++)
      {
        li_config_table[i].flag_table[j] = 0;
        li_config_table[j].flag_table[i] = 0;
        li_config_table[i].coef_table[j] = 0;
        li_config_table[j].coef_table[i] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
      }
      if (a->manufacturer_features & MANUFACTURER_FEATURE_SLAVE_CODEC)
      {
        i = a->li_base + MIXER_IC_CHANNEL_BASE + (2 - plci->li_bchannel_id);
        li_config_table[i].curchnl = 0;
        li_config_table[i].channel &= LI_CHANNEL_ADDRESSES_SET | LI_CHANNEL_NO_ADDRESSES;
        li_config_table[i].chflags = 0;
        for (j = 0; j < li_total_channels; j++)
        {
          li_config_table[i].flag_table[j] = 0;
          li_config_table[j].flag_table[i] = 0;
          li_config_table[i].coef_table[j] = 0;
          li_config_table[j].coef_table[i] &= ~(LI_COEF_CH_CH | LI_COEF_CH_PC | LI_COEF_PC_CH | LI_COEF_PC_PC);
        }
      }
    }

  }
}


static void adv_voice_prepare_switch (dword Id, PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_prepare_switch",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

}


static word adv_voice_save_config (dword Id, PLCI   *plci, byte Rc)
{

  dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_save_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  return (GOOD);
}


static word adv_voice_restore_config (dword Id, PLCI   *plci, byte Rc)
{
  DIVA_CAPI_ADAPTER   *a;
  word Info;

  dbug (1, dprintf ("[%06lx] %s,%d: adv_voice_restore_config %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  a = plci->adapter;
  if ((plci->B1_facilities & B1_FACILITY_VOICE)
   && (plci->tel == ADV_VOICE) && (plci == a->AdvSignalPLCI))
  {
    switch (plci->adjust_b_state)
    {
    case ADJUST_B_RESTORE_VOICE_1:
      plci->internal_command = plci->adjust_b_command;
      if (plci->sig_req)
      {
        plci->adjust_b_state = ADJUST_B_RESTORE_VOICE_1;
        break;
      }
      adv_voice_write_coefs (plci, ADV_VOICE_WRITE_UPDATE);
      plci->adjust_b_state = ADJUST_B_RESTORE_VOICE_2;
      break;
    case ADJUST_B_RESTORE_VOICE_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Restore voice config failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      break;
    }
  }
  return (Info);
}




/*------------------------------------------------------------------*/
/* RTP facilities                                                   */
/*------------------------------------------------------------------*/


static byte rtp_supported_parameters[] =
{
  0x06,
  0xe8, 0x03,  /* max supported late packet ratio */
  0xe8, 0x03,  /* max supported dejitter delay in milliseconds */
  0x00, 0x00   /* min supported dejitter delay in milliseconds */
};

static byte *(rtp_supported_payload_options[]) =
{
  "\x04\x0f\xc1\x40\x06",            /* PCMU_8000     */
  "\x04\x0f\xc1\x40\x06",            /* LINEAR16_8000 */
  "\x04\x1f\xc3\x40\x06",            /* G726_32_8000  */
  "\x04\x0f\xc0\x40\x06",            /* GSM_8000      */
  "\x04\x1f\xc1\x90\x06",            /* G723_8000     */
  "\x04\x0f\xc0\x40\x06",            /* DVI4_8000     */
  "\x04\x0f\xc0\x80\x0c",            /* DVI4_16000    */
  "\x04\x0f\xc0\x40\x06",            /* LPC_8000      */
  "\x04\x0f\xc1\x40\x06",            /* PCMA_8000     */
  "\x04\x0f\xc0\x80\x0c",            /* G722_16000    */
  "\x00",                            /* 10            */
  "\x00",                            /* 11            */
  "\x04\x0f\xc0\x40\x06",            /* QCELP_8000    */
  "\x00",                            /* 13            */
  "\x04\x0f\xc0\x40\x06",            /* G728_8000     */
  "\x00",                            /* 15            */
  "\x00",                            /* 16            */
  "\x00",                            /* 17            */
  "\x04\x0f\xc0\x40\x06",            /* G729_8000     */
  "\x00",                            /* 19            */
  "\x00",                            /* 20            */
  "\x00",                            /* 21            */
  "\x00",                            /* 22            */
  "\x00",                            /* 23            */
  "\x00",                            /* 24            */
  "\x00",                            /* 25            */
  "\x10\x0e\xc0\x40\x06"             /* RTAUDIO_8000  */
    "\x01\x00\x01\x00"
    "\x01\x01\x00\x00"
    "\x00\x00\x00\x00",
  "\x04\x1c\xc1\x40\x06",            /* ILBC_8000     */
  "\x10\x0f\xc0\x40\x06"             /* AMR_NB_8000   */
    "\xa0\xff\x00\x00"
    "\xff\x80\xff\xff"
    "\xff\x80\xff\xff",
  "\x00",                            /* T38           */
  "\x00",                            /* GSM_HR_8000   */
  "\x00",                            /* GSM_EFR_8000  */

  "\x00",                            /* RED           */
  "\x02\x03\x00",                    /* CN_8000       */
  "\x25\x0c\xc1\xe8\x03\x20"         /* DTMF          */
    "\xff\xff\x01\x00\x10\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00",
  "\x00",                            /* FEC           */
  "\x00",                            /* 36            */
  "\x00",                            /* 37            */
  "\x00",                            /* 38            */
  "\x00",                            /* 39            */
  "\x00",                            /* 40            */
  "\x00",                            /* 41            */
  "\x00",                            /* 42            */
  "\x00",                            /* 43            */
  "\x00",                            /* 44            */
  "\x00",                            /* 45            */
  "\x00",                            /* 46            */
  "\x00",                            /* 47            */
  "\x00",                            /* 48            */
  "\x00",                            /* 49            */
  "\x00",                            /* 50            */
  "\x00",                            /* 51            */
  "\x00",                            /* 52            */
  "\x00",                            /* 53            */
  "\x00",                            /* 54            */
  "\x00",                            /* 55            */
  "\x00",                            /* 56            */
  "\x00",                            /* 57            */
  "\x00",                            /* 58            */
  "\x00",                            /* 59            */
  "\x00",                            /* 60            */
  "\x00",                            /* 61            */
  "\x00",                            /* 62            */
  "\x00",                            /* 63            */
};


static void rtp_query_rtcp_report (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_query_rtcp_report",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->internal_req_buffer[0] = UDATA_REQUEST_QUERY_RTCP_REPORT;
  plci->NData[0].PLength = 1;
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void rtp_request_rtp_control (PLCI   *plci, byte   *param_buffer, word param_length)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_request_rtp_control %d",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, param_length));

  plci->internal_req_buffer[0] = UDATA_REQUEST_RTP_CONTROL;
  for (i = 0; i < param_length; i++)
    plci->internal_req_buffer[1+i] = param_buffer[i];
  plci->NData[0].PLength = 1 + param_length;
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void rtp_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
    byte result[8];

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_command %02x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->rtp_cmd));

  Info = GOOD;
  WRITE_WORD (&result[1], plci->rtp_cmd);
  WRITE_WORD (&result[4], GOOD);
  result[3] = 2;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (plci->rtp_cmd)
  {
  case RTP_QUERY_RTCP_REPORT:
    switch (internal_command)
    {
    default:
    case RTP_COMMAND_1:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = RTP_COMMAND_1;
        return;
      }
      plci->internal_command = RTP_COMMAND_2;
      plci->rtp_msg_number_queue[(plci->rtp_send_requests)++] = plci->number;
      rtp_query_rtcp_report (plci);
      return;
    case RTP_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: RTP command failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      return;
    }
    break;

  case RTP_REQUEST_RTP_CONTROL:
    switch (internal_command)
    {
    default:
    case RTP_COMMAND_1:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = RTP_COMMAND_1;
        return;
      }
      plci->internal_command = RTP_COMMAND_2;
      rtp_request_rtp_control (plci, &plci->saved_msg.parms[1].info[1], plci->saved_msg.parms[1].length);
      return;
    case RTP_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: RTP command failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      result[3] = 3;
      result[6] = plci->saved_msg.parms[1].info[1];
      break;
    }
    break;
  }
  result[0] = result[3] + 3;
  sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->number,
    "wws", Info, PRIV_SELECTOR_RTP, result);
}


static byte rtp_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i;
  byte payload;
    API_PARSE rtp_parms[3];
    API_PARSE rtp_request_parms[2];
    byte result[64];

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 0;
  if (!(a->profile.Manuf.private_options & (1L << PRIVATE_RTP))
   && ((plci == NULL) || !(plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else if (api_parse (&msg[1].info[1], msg[1].length, "ws", rtp_parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }
  else if ((READ_WORD (rtp_parms[0].info) == RTP_GET_SUPPORTED_PARAMETERS)
    || (READ_WORD (rtp_parms[0].info) == RTP_GET_SUPPORTED_PAYLOAD_PROT)
    || (READ_WORD (rtp_parms[0].info) == RTP_GET_SUPPORTED_PAYLOAD_OPT))
  {
    WRITE_WORD (&result[1], READ_WORD (rtp_parms[0].info));
    WRITE_WORD (&result[4], GOOD);
    result[3] = 2;
    switch (READ_WORD (rtp_parms[0].info))
    {
    case RTP_GET_SUPPORTED_PARAMETERS:
      for (i = 1; i <= rtp_supported_parameters[0]; i++)
        result[5+i] = rtp_supported_parameters[i];
      result[3] = 2 + rtp_supported_parameters[0];
      break;

    case RTP_GET_SUPPORTED_PAYLOAD_PROT:
      WRITE_DWORD (&result[6], a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f)));
      WRITE_DWORD (&result[10], a->rtp_additional_payloads);
      result[3] = 10;
      break;

    case RTP_GET_SUPPORTED_PAYLOAD_OPT:
      if (api_parse (&rtp_parms[1].info[1], rtp_parms[1].length, "b", rtp_request_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _WRONG_MESSAGE_FORMAT);
      }
      else if ((((rtp_request_parms[0].info[0] & 0xe0) == 0x00)
         && !(a->rtp_primary_payloads & (((dword) 1) << (rtp_request_parms[0].info[0] & 0x1f)) &
              ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f))))
        || (((rtp_request_parms[0].info[0] & 0xe0) == 0x20)
         && !(a->rtp_additional_payloads & (((dword) 1) << (rtp_request_parms[0].info[0] & 0x1f))))
        || (rtp_request_parms[0].info[0] >= 0x40))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Payload protocol not supported",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
        result[6] = rtp_request_parms[0].info[0];
        result[7] = 0;
        result[3] = result[7] + 4;
      }
      else
      {
        payload = rtp_request_parms[0].info[0];
        result[6] = payload;
        for (i = 0; i <= rtp_supported_payload_options[payload][0]; i++)
          result[7+i] = rtp_supported_payload_options[payload][i];
        result[3] = result[7] + 4;
      }
      break;

    default:
      dbug (1, dprintf ("[%06lx] %s,%d: RTP unknown request %04x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->rtp_cmd));
      WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
    }
    result[0] = result[3] + 3;
  }
  else if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else
  {
    if ((plci->State == IDLE)
     || (plci->State == OUTG_DIS_PENDING)
     || (plci->State == INC_DIS_PENDING)
     || !plci->NL.Id || plci->nl_remove_id)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      Info = _WRONG_STATE;
    }
    else
    {
      plci->command = 0;
      plci->rtp_cmd = READ_WORD (rtp_parms[0].info);
      WRITE_WORD (&result[1], plci->rtp_cmd);
      WRITE_WORD (&result[4], GOOD);
      result[3] = 2;
      switch (plci->rtp_cmd)
      {
      case RTP_QUERY_RTCP_REPORT:
        if (plci->rtp_send_requests >=
          sizeof(plci->rtp_msg_number_queue) / sizeof(plci->rtp_msg_number_queue[0]))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: RTP request overrun",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
          break;
        }
        start_internal_command (Id, plci, rtp_command);
        return (FALSE);

      case RTP_REQUEST_RTP_CONTROL:
        api_save_msg (rtp_parms, "ws", &plci->saved_msg);
        start_internal_command (Id, plci, rtp_command);
        return (FALSE);

      default:
        dbug (1, dprintf ("[%06lx] %s,%d: RTP unknown request %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->rtp_cmd));
        WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
      }
      result[0] = result[3] + 3;
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, PRIV_SELECTOR_RTP, result);
  return (FALSE);
}


static void rtp_confirmation (dword Id, PLCI   *plci, byte   *msg, word length)
{
  word i;
    byte result[40];

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_confirmation",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if (plci->rtp_send_requests != 0)
  {
    WRITE_WORD (&result[1], plci->rtp_cmd);
    WRITE_WORD (&result[4], GOOD);
    for (i = 1; i < length; i++)
      result[5+i] = msg[i];
    result[3] = 1 + length;
    result[0] = result[3] + 3;
    sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->rtp_msg_number_queue[0],
      "wws", GOOD, PRIV_SELECTOR_RTP, result);
    (plci->rtp_send_requests)--;
    for (i = 0; i < plci->rtp_send_requests; i++)
      plci->rtp_msg_number_queue[i] = plci->rtp_msg_number_queue[i+1];
  }
}


static void rtp_indication (dword Id, PLCI   *plci, byte   *msg, word length)
{

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_indication",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  msg[0] = (byte)(length - 1);
  sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "wbwS",
    PRIV_SELECTOR_RTP, msg[0] + 3, RTP_REQUEST_RTP_CONTROL, msg);
}


/*------------------------------------------------------------------*/
/* T38 facilities                                                   */
/*------------------------------------------------------------------*/


static byte t38_supported_parameters[] =
{
  0x16,
  0x0f, 0x00,  /* supported transport protocols */
  0x0f, 0x00,  /* supported ASN.1 versions */
  0x40, 0x83,  /* max supported bit rate */
  0x00, 0xc0,  /* supported T.38 options */
  0x00, 0x08,  /* maximum supported datagram size */
  0x00, 0x10,  /* provided buffer size */
  0x03, 0x00,  /* supported error correction */
  0x02, 0x00,  /* supported number of secondaries */
  0x02, 0x00,  /* supported number of IFP's in FEC packet */
  0x02, 0xfc,  /* supported T.30 options */
  0x01, 0x01,  /* supported T.30 options 2 */
};



static byte *(t38_supported_payload_options[]) =
{
  "\x00",                            /* PCMU_8000     */
  "\x00",                            /* LINEAR16_8000 */
  "\x00",                            /* G726_32_8000  */
  "\x00",                            /* GSM_8000      */
  "\x00",                            /* G723_8000     */
  "\x00",                            /* DVI4_8000     */
  "\x00",                            /* DVI4_16000    */
  "\x00",                            /* LPC_8000      */
  "\x00",                            /* PCMA_8000     */
  "\x00",                            /* G722_16000    */
  "\x00",                            /* 10            */
  "\x00",                            /* 11            */
  "\x00",                            /* QCELP_8000    */
  "\x00",                            /* 13            */
  "\x00",                            /* G728_8000     */
  "\x00",                            /* 15            */
  "\x00",                            /* 16            */
  "\x00",                            /* 17            */
  "\x00",                            /* G729_8000     */
  "\x00",                            /* 19            */
  "\x00",                            /* 20            */
  "\x00",                            /* 21            */
  "\x00",                            /* 22            */
  "\x00",                            /* 23            */
  "\x00",                            /* 24            */
  "\x00",                            /* 25            */
  "\x00",                            /* RTAUDIO_8000  */
  "\x00",                            /* ILBC_8000     */
  "\x00",                            /* AMR_NB_8000   */
  t38_supported_parameters,          /* T38           */
  "\x00",                            /* GSM_HR_8000   */
  "\x00",                            /* GSM_EFR_8000  */

  "\x00",                            /* RED           */
  "\x00",                            /* CN_8000       */
  "\x00",                            /* DTMF          */
  "\x00",                            /* FEC           */
  "\x00",                            /* 36            */
  "\x00",                            /* 37            */
  "\x00",                            /* 38            */
  "\x00",                            /* 39            */
  "\x00",                            /* 40            */
  "\x00",                            /* 41            */
  "\x00",                            /* 42            */
  "\x00",                            /* 43            */
  "\x00",                            /* 44            */
  "\x00",                            /* 45            */
  "\x00",                            /* 46            */
  "\x00",                            /* 47            */
  "\x00",                            /* 48            */
  "\x00",                            /* 49            */
  "\x00",                            /* 50            */
  "\x00",                            /* 51            */
  "\x00",                            /* 52            */
  "\x00",                            /* 53            */
  "\x00",                            /* 54            */
  "\x00",                            /* 55            */
  "\x00",                            /* 56            */
  "\x00",                            /* 57            */
  "\x00",                            /* 58            */
  "\x00",                            /* 59            */
  "\x00",                            /* 60            */
  "\x00",                            /* 61            */
  "\x00",                            /* 62            */
  "\x00",                            /* 63            */
};

#define man_profile_t38_additional_payloads(a)  (((a)->rtp_primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f))) ?    (((dword) 1) << (RTP_ADD_PAYLOAD_RED & 0x1f)) | (((dword) 1) << (RTP_ADD_PAYLOAD_FEC & 0x1f)) : 0)


static void t38_query_rtcp_report (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: t38_query_rtcp_report %02x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->internal_req_buffer[0] = UDATA_REQUEST_QUERY_RTCP_REPORT;
  plci->NData[0].PLength = 1;
  plci->NData[0].P = plci->internal_req_buffer;
  plci->NL.X = plci->NData;
  plci->NL.ReqCh = 0;
  plci->NL.Req = plci->nl_req = (byte) N_UDATA;
  trcq (plci);
  plci->adapter->request (&plci->NL);
}


static void t38_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command, Info;
    byte result[6];

  dbug (1, dprintf ("[%06lx] %s,%d: t38_command %02x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command,
    plci->t38_cmd));

  Info = GOOD;
  WRITE_WORD (&result[1], plci->t38_cmd);
  WRITE_WORD (&result[4], GOOD);
  result[3] = 2;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (plci->t38_cmd)
  {
  case T38_QUERY_RTCP_REPORT:
    switch (internal_command)
    {
    default:
    case T38_COMMAND_1:
      if (plci_nl_busy (plci))
      {
        plci->internal_command = T38_COMMAND_1;
        return;
      }
      plci->internal_command = T38_COMMAND_2;
      plci->t38_msg_number_queue[(plci->t38_send_requests)++] = plci->number;
      t38_query_rtcp_report (plci);
      return;
    case T38_COMMAND_2:
      if ((Rc != OK) && (Rc != OK_FC))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: T38 command failed %02x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
        Info = _WRONG_STATE;
        break;
      }
      return;
    }
    break;
  }
  result[0] = result[3] + 3;
  sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->number,
    "wws", Info, PRIV_SELECTOR_T38, result);
}



static byte t38_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i;
  byte payload;
    API_PARSE t38_parms[3];
    API_PARSE t38_request_parms[2];
    byte result[64];

  dbug (1, dprintf ("[%06lx] %s,%d: t38_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 0;
  if (!(a->profile.Manuf.private_options & (1L << PRIVATE_T38))
   && ((plci == NULL) || !(plci->B1_options & DSP_CAI_ARRANGEMENT_LINE_FLAG)))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Facility not supported",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _FACILITY_NOT_SUPPORTED;
  }
  else if (api_parse (&msg[1].info[1], msg[1].length, "ws", t38_parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }
  else if ((READ_WORD (t38_parms[0].info) == T38_GET_SUPPORTED_PARAMETERS)
    || (READ_WORD (t38_parms[0].info) == T38_GET_SUPPORTED_PAYLOAD_PROT)
    || (READ_WORD (t38_parms[0].info) == T38_GET_SUPPORTED_PAYLOAD_OPT))
  {
    WRITE_WORD (&result[1], READ_WORD (t38_parms[0].info));
    WRITE_WORD (&result[4], GOOD);
    result[3] = 2;
    switch (READ_WORD (t38_parms[0].info))
    {
    case T38_GET_SUPPORTED_PARAMETERS:
      for (i = 1; i <= t38_supported_parameters[0]; i++)
        result[5+i] = t38_supported_parameters[i];
      result[3] = 2 + t38_supported_parameters[0];
      break;


    case T38_GET_SUPPORTED_PAYLOAD_PROT:
      WRITE_DWORD (&result[6], a->rtp_primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f)));
      WRITE_DWORD (&result[10], man_profile_t38_additional_payloads (a));
      result[3] = 10;
      break;

    case T38_GET_SUPPORTED_PAYLOAD_OPT:
      if (api_parse (&t38_parms[1].info[1], t38_parms[1].length, "b", t38_request_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _WRONG_MESSAGE_FORMAT);
      }
      else if ((((t38_request_parms[0].info[0] & 0xe0) == 0x00)
         && !(a->rtp_primary_payloads & (((dword) 1) << (t38_request_parms[0].info[0] & 0x1f)) &
              (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f))))
        || (((t38_request_parms[0].info[0] & 0xe0) == 0x20)
         && !(man_profile_t38_additional_payloads (a) & (((dword) 1) << (t38_request_parms[0].info[0] & 0x1f))))
        || (t38_request_parms[0].info[0] >= 0x40))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Payload protocol not supported",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
        result[6] = t38_request_parms[0].info[0];
        result[7] = 0;
        result[3] = result[7] + 4;
      }
      else
      {
        payload = t38_request_parms[0].info[0];
        result[6] = payload;
        for (i = 0; i <= t38_supported_payload_options[payload][0]; i++)
          result[7+i] = t38_supported_payload_options[payload][i];
        result[3] = result[7] + 4;
      }
      break;


    default:
      dbug (1, dprintf ("[%06lx] %s,%d: T38 unknown request %04x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->rtp_cmd));
      WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
    }
    result[0] = result[3] + 3;
  }
  else if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else
  {
    if ((plci->State == IDLE)
     || (plci->State == OUTG_DIS_PENDING)
     || (plci->State == INC_DIS_PENDING)
     || !plci->NL.Id || plci->nl_remove_id)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      Info = _WRONG_STATE;
    }
    else
    {
      plci->command = 0;
      plci->t38_cmd = READ_WORD (t38_parms[0].info);
      WRITE_WORD (&result[1], plci->t38_cmd);
      WRITE_WORD (&result[4], GOOD);
      result[3] = 2;
      switch (plci->t38_cmd)
      {

      case T38_QUERY_RTCP_REPORT:
        if (plci->t38_send_requests >=
          sizeof(plci->t38_msg_number_queue) / sizeof(plci->t38_msg_number_queue[0]))
        {
          dbug (1, dprintf ("[%06lx] %s,%d: T38 request overrun",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
          Info = _WRONG_STATE;
          break;
        }
        start_internal_command (Id, plci, t38_command);
        return (FALSE);


      default:
        dbug (1, dprintf ("[%06lx] %s,%d: T38 unknown request %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->t38_cmd));
        WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
      }
      result[0] = result[3] + 3;
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, PRIV_SELECTOR_T38, result);
  return (FALSE);
}



static void t38_confirmation (dword Id, PLCI   *plci, byte   *msg, word length)
{
  word i;
    byte result[40];

  dbug (1, dprintf ("[%06lx] %s,%d: t38_confirmation",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  if (plci->t38_send_requests != 0)
  {
    WRITE_WORD (&result[1], plci->t38_cmd);
    WRITE_WORD (&result[4], GOOD);
    for (i = 1; i < length; i++)
      result[5+i] = msg[i];
    result[3] = 1 + length;
    result[0] = result[3] + 3;
    sendf (plci->appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->t38_msg_number_queue[0],
      "wws", GOOD, PRIV_SELECTOR_T38, result);
    (plci->t38_send_requests)--;
    for (i = 0; i < plci->t38_send_requests; i++)
      plci->t38_msg_number_queue[i] = plci->t38_msg_number_queue[i+1];
  }
}



/*------------------------------------------------------------------*/
/* Connect support, i.e. profile and support for each connect type  */
/*------------------------------------------------------------------*/

static byte connect_support_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i;
  byte connect_type;
  dword supported_connect_types;
  dword primary_payloads, additional_payloads;
    API_PARSE parms[3];
    API_PARSE request_parms[2];
    byte result[80];
    API_PROFILE profile;

  dbug (1, dprintf ("[%06lx] %s,%d: connect_support_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 0;
  if (api_parse (&msg[1].info[1], msg[1].length, "ws", parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }
  else
  {
    WRITE_WORD (&result[1], READ_WORD (parms[0].info));
    WRITE_WORD (&result[4], GOOD);
    result[3] = 2;
    supported_connect_types = 0;
    if (a->profile.Global_Options & GL_INTERNAL_CONTROLLER_SUPPORTED)
      supported_connect_types |= 1L << CONNECT_SUPPORT_INTERNAL_CONTROLLER;
    if (a->profile.Global_Options & GL_EXTERNAL_EQUIPMENT_SUPPORTED)
      supported_connect_types |= 1L << CONNECT_SUPPORT_EXTERNAL_EQUIPMENT;

    if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_NULL_PLCI)
      supported_connect_types |= 1L << CONNECT_SUPPORT_NULL_PLCI;

    if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_SOFTIP)
      supported_connect_types |= 1L << CONNECT_SUPPORT_LINE_SIDE;
    if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_RTP_LINE)
      supported_connect_types |= (1L << CONNECT_SUPPORT_DATA_STUB) | (1L << CONNECT_SUPPORT_LINE_STUB);
    if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_NO_CLOCK_LINE)
      supported_connect_types |= (1L << CONNECT_SUPPORT_DATA_NO_CLOCK) | (1L << CONNECT_SUPPORT_LINE_NO_CLOCK);
    if (READ_WORD (parms[0].info) == CONNECT_SUPPORT_GET_TYPES)
    {
      WRITE_DWORD (&result[6], supported_connect_types);
      result[3] = 6;
    }
    else
    {
      if (api_parse (&parms[1].info[1], parms[1].length, "b", request_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _WRONG_MESSAGE_FORMAT);
      }
      else if ((request_parms[0].info[0] >= 0x20)
        || !(supported_connect_types & (((dword) 1) << request_parms[0].info[0])))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Connect type not supported",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
        result[6] = request_parms[0].info[0];
        result[7] = 0;
        result[3] = result[7] + 4;
      }
      else
      {
        connect_type = request_parms[0].info[0];
        result[6] = connect_type;
        switch (READ_WORD (parms[0].info))
        {
        case CONNECT_SUPPORT_GET_PROFILE:
          for (i = 0; i < sizeof(API_PROFILE); i++)
            ((byte *)(&profile))[i] = 0;
          switch (connect_type)
          {
          case CONNECT_SUPPORT_INTERNAL_CONTROLLER:
            for (i = 0; i < sizeof(API_PROFILE); i++)
              ((byte *)(&profile))[i] = ((byte *)(&a->profile))[i];
            break;
          case CONNECT_SUPPORT_EXTERNAL_EQUIPMENT:
            profile.Number = a->profile.Number;
            break;
          case CONNECT_SUPPORT_NULL_PLCI:

            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels + 2;
            profile.Global_Options = 0x000000c1;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            break;
          case CONNECT_SUPPORT_LINE_SIDE:
            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels;
            profile.Global_Options = 0x00000041;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            profile.Manuf.private_options |= 1L << PRIVATE_RTP;


            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_LINE_SIDE_T38)
            {
              profile.Manuf.private_options |= 1L << PRIVATE_T38;
            }

            break;
          case CONNECT_SUPPORT_DATA_STUB:
            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels;
            profile.Global_Options = 0x000000c1;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING)
            {
              profile.Manuf.private_options |= 1L << PRIVATE_RTP;
            }

            break;
          case CONNECT_SUPPORT_LINE_STUB:
            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels;
            profile.Global_Options = 0x00000041;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            profile.Manuf.private_options |= 1L << PRIVATE_RTP;


            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_LINE_SIDE_T38)
            {
              profile.Manuf.private_options |= 1L << PRIVATE_T38;
            }

            break;
          case CONNECT_SUPPORT_DATA_NO_CLOCK:
            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels;
            profile.Global_Options = 0x00000041;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING)
            {
              profile.Manuf.private_options |= 1L << PRIVATE_RTP;
            }

            break;
          case CONNECT_SUPPORT_LINE_NO_CLOCK:
            profile.Number = a->profile.Number;
            profile.Channels = a->profile.Channels;
            profile.Global_Options = 0x00000041;
            profile.B1_Protocols = 0x00000002;
            profile.B2_Protocols = 0x00000002;
            profile.B3_Protocols = 0x00000001;

            profile.Manuf.private_options |= 1L << PRIVATE_RTP;

            break;
          }
          result[7] = sizeof(API_PROFILE);
          for (i = 0; i < sizeof(API_PROFILE); i++)
            result[8+i] = 0;
          WRITE_WORD (&(((API_PROFILE *)(&result[8]))->Number), profile.Number);
          WRITE_WORD (&(((API_PROFILE *)(&result[8]))->Channels), profile.Channels);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Global_Options), profile.Global_Options);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->B1_Protocols), profile.B1_Protocols);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->B2_Protocols), profile.B2_Protocols);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->B3_Protocols), profile.B3_Protocols);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Manuf.private_options), profile.Manuf.private_options);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Manuf.reserved_1), profile.Manuf.reserved_1);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Manuf.reserved_2), profile.Manuf.reserved_2);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Manuf.reserved_3), profile.Manuf.reserved_3);
          WRITE_DWORD (&(((API_PROFILE *)(&result[8]))->Manuf.reserved_4), profile.Manuf.reserved_4);
          result[3] = result[7] + 4;
          break;


        case CONNECT_SUPPORT_GET_RTP_PAYLOAD_PROT:
          primary_payloads = 0;
          additional_payloads = 0;
          switch (connect_type)
          {
          case CONNECT_SUPPORT_INTERNAL_CONTROLLER:
            if (!(a->manufacturer_features2 & MANUFACTURER_FEATURE2_SOFTIP)
             || (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING))
            {
              primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
              additional_payloads = a->rtp_additional_payloads;
            }
            else if (a->profile.Manuf.private_options & (1L << PRIVATE_RTP))
            {
              primary_payloads = 0x00000101;
              additional_payloads = 0x00000000;
            }
            break;
          case CONNECT_SUPPORT_EXTERNAL_EQUIPMENT:
            break;
          case CONNECT_SUPPORT_NULL_PLCI:
            break;
          case CONNECT_SUPPORT_LINE_SIDE:
            primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
            additional_payloads = a->rtp_additional_payloads;
            break;
          case CONNECT_SUPPORT_DATA_STUB:
            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING)
            {
              primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
              additional_payloads = a->rtp_additional_payloads;
            }
            break;
          case CONNECT_SUPPORT_LINE_STUB:
            primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
            additional_payloads = a->rtp_additional_payloads;
            break;
          case CONNECT_SUPPORT_DATA_NO_CLOCK:
            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING)
            {
              primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
              additional_payloads = a->rtp_additional_payloads;
            }
            break;
          case CONNECT_SUPPORT_LINE_NO_CLOCK:
            primary_payloads = a->rtp_primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
            additional_payloads = a->rtp_additional_payloads;
            break;
          }
          result[7] = 8;
          WRITE_DWORD (&result[8], primary_payloads);
          WRITE_DWORD (&result[12], additional_payloads);
          result[3] = result[7] + 4;
          break;



        case CONNECT_SUPPORT_GET_T38_PAYLOAD_PROT:
          primary_payloads = 0;
          additional_payloads = 0;
          switch (connect_type)
          {
          case CONNECT_SUPPORT_INTERNAL_CONTROLLER:
            primary_payloads = a->rtp_primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
            additional_payloads = man_profile_t38_additional_payloads (a);
            break;
          case CONNECT_SUPPORT_EXTERNAL_EQUIPMENT:
            break;
          case CONNECT_SUPPORT_NULL_PLCI:
            break;
          case CONNECT_SUPPORT_LINE_SIDE:
            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_LINE_SIDE_T38)
            {
              primary_payloads = ((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f);
              additional_payloads = man_profile_t38_additional_payloads (a);
            }
            break;
          case CONNECT_SUPPORT_DATA_STUB:
            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_TRANSCODING)
            {
              primary_payloads = a->rtp_primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
              additional_payloads = man_profile_t38_additional_payloads (a);
            }
            break;
          case CONNECT_SUPPORT_LINE_STUB:
            if (a->manufacturer_features2 & MANUFACTURER_FEATURE2_LINE_SIDE_T38)
            {
              primary_payloads = ((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f);
              additional_payloads = man_profile_t38_additional_payloads (a);
            }
            break;
          case CONNECT_SUPPORT_DATA_NO_CLOCK:
            break;
          case CONNECT_SUPPORT_LINE_NO_CLOCK:
            break;
          }
          result[7] = 8;
          WRITE_DWORD (&result[8], primary_payloads);
          WRITE_DWORD (&result[12], additional_payloads);
          result[3] = result[7] + 4;
          break;


        default:
          dbug (1, dprintf ("[%06lx] %s,%d: Connect support unknown request %04x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->rtp_cmd));
          WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
        }
      }
    }
    result[0] = result[3] + 3;
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, PRIV_SELECTOR_CONNECT_SUPPORT, result);
  return (FALSE);
}


/*------------------------------------------------------------------*/
/* Resource reservation                                             */
/*------------------------------------------------------------------*/

static void add_reserve_request (PLCI   *plci)
{
    byte parameter_buffer[80];
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: add_reserve_request",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  parameter_buffer[0] = 1 + sizeof(API_PROFILE);
  parameter_buffer[1] = PROFILEIE;
  for (i = 0; i < sizeof(API_PROFILE); i++)
    parameter_buffer[2+i] = 0;
  WRITE_DWORD (&parameter_buffer[6], plci->reserve_request_profile.global_options);
  WRITE_DWORD (&parameter_buffer[10], plci->reserve_request_profile.b1_protocols);
  WRITE_DWORD (&parameter_buffer[14], plci->reserve_request_profile.b2_protocols);
  WRITE_DWORD (&parameter_buffer[18], plci->reserve_request_profile.b3_protocols);
  WRITE_DWORD (&parameter_buffer[46], plci->reserve_request_profile.manufacturer_features);
  WRITE_DWORD (&parameter_buffer[50], plci->reserve_request_profile.rtp_primary_payloads);
  WRITE_DWORD (&parameter_buffer[54], plci->reserve_request_profile.rtp_additional_payloads);
  WRITE_DWORD (&parameter_buffer[58], plci->reserve_request_profile.manufacturer_features2);
  add_p (plci, ESC, parameter_buffer);
  sig_req (plci, RESERVE_REQ, 0);
}


static void indicate_reserve_grant (PLCI   *plci, APPL   *appl)
{
    byte result[80+16+16];
    struct resource_profile_s resource_profile;
  struct resource_profile_s *p_resource_profile;
  struct resource_index_entry   *p_index;
  word i, reserve_cmd, offs;
  dword private_options;
  dword primary_payloads, additional_payloads;
  dword Id, d;

  dbug (1, dprintf ("[%06lx] %s,%d: indicate_reserve_grant",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
  {
    p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
    reserve_cmd = p_index->cmd;
    for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
    {
      ((dword *)(&resource_profile))[i] =
        ((dword *)(&(plci->reserve_granted_profile)))[i] & ((dword *)(&(p_index->profile)))[i];
    }
    p_resource_profile = &resource_profile;
  }
  else
  {
    reserve_cmd = plci->reserve_pending_cmd;
    p_resource_profile = &(plci->reserve_granted_profile);
  }

  offs = 0;
  do
  {
    WRITE_WORD (&result[offs+1], reserve_cmd & 0x7);
    WRITE_WORD (&result[offs+4], GOOD);
    result[offs+6] = 0;
    switch (reserve_cmd & 0x7)
    {
    case RESERVE_RTP_PAYLOAD_PROT:
      result[offs+7] = 8;
      primary_payloads = 0;
      additional_payloads = 0;
      if (p_resource_profile->manufacturer_features & MANUFACTURER_FEATURE_RTP)
      {
        primary_payloads |= p_resource_profile->rtp_primary_payloads &
          ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
        additional_payloads |= p_resource_profile->rtp_additional_payloads;
      }
      WRITE_DWORD (&result[offs+8], primary_payloads);
      WRITE_DWORD (&result[offs+12], additional_payloads);
      break;

    case RESERVE_T38_PAYLOAD_PROT:
      result[offs+7] = 8;
      primary_payloads = 0;
      additional_payloads = 0;
      if (p_resource_profile->manufacturer_features & MANUFACTURER_FEATURE_T38)
      {
        primary_payloads |= p_resource_profile->rtp_primary_payloads &
          (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
        additional_payloads |= (primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f))) ?
          (((dword) 1) << (RTP_ADD_PAYLOAD_RED & 0x1f)) | (((dword) 1) << (RTP_ADD_PAYLOAD_FEC & 0x1f)) : 0;
      }
      WRITE_DWORD (&result[offs+8], primary_payloads);
      WRITE_DWORD (&result[offs+12], additional_payloads);
      break;

    default:
      result[offs+7] = sizeof(API_PROFILE);
      for (i = 0; i < sizeof(API_PROFILE); i++)
        result[offs+8+i] = 0;
      d = p_resource_profile->global_options;
      if (p_resource_profile->manufacturer_features & MANUFACTURER_FEATURE_ECHO_CANCELLER)
        d |= GL_ECHO_CANCELLER_SUPPORTED_NEW;
      WRITE_DWORD (&result[offs+12], d);
      WRITE_DWORD (&result[offs+16], p_resource_profile->b1_protocols);
      WRITE_DWORD (&result[offs+20], p_resource_profile->b2_protocols);
      WRITE_DWORD (&result[offs+24], p_resource_profile->b3_protocols);
      private_options = 0;
      WRITE_DWORD (&result[offs+52], private_options);
    }
    result[offs+3] = result[offs+7] + 4;
    offs += result[offs+3] + 3;
    reserve_cmd >>= 3;
  } while (reserve_cmd != 0);
  result[0] = (byte) offs;

  Id = ((word)plci->Id<<8)|plci->adapter->Id;
  if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
  {
    plci->c_ind_reserve_info[appl->Id-1] &= ~RESERVE_INFO_REQUEST_PENDING;
    sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->c_ind_reserve_msg_number[appl->Id-1],
      "wws", GOOD, PRIV_SELECTOR_RESOURCE_RESERVATION, result);
  }
  else
  {
    plci->reserve_request_pending = FALSE;
    sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, plci->reserve_pending_msg_number,
      "wws", GOOD, PRIV_SELECTOR_RESOURCE_RESERVATION, result);
  }
}


static byte resource_reservation_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg)
{
  word Info;
  word i, j, n, reserve_cmd, reserve_cmd_number, offs;
  dword private_options;
  dword primary_payloads, additional_payloads;
  struct resource_index_entry   *p_index;
    API_PARSE parms[3];
    API_PARSE request_parms[3];
    byte result[8];
    struct resource_profile_s resource_profile;

  dbug (1, dprintf ("[%06lx] %s,%d: resource_reservation_request",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));

  Info = GOOD;
  result[0] = 0;
  if (api_parse (&msg[1].info[1], msg[1].length, "ws", parms))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_MESSAGE_FORMAT;
  }
  else if (plci == NULL)
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong PLCI",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_IDENTIFIER;
  }
  else if ((plci->State == IDLE)
   || (plci->State == OUTG_DIS_PENDING)
   || (plci->State == INC_DIS_PENDING))
  {
    dbug (1, dprintf ("[%06lx] %s,%d: Wrong state",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    Info = _WRONG_STATE;
  }
  else if ((appl != plci->appl)
    && (plci->State != INC_CON_PENDING) && (plci->State != INC_CON_ALERT))
  {
    return (0);
  }
  else
  {
    if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
    {
      if ((plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) != 0)
      {
        p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
        for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
          ((dword *)(&resource_profile))[i] = ((dword *)(&(p_index->profile)))[i];
      }
      else
      {
        for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
          ((dword *)(&resource_profile))[i] = 0;
      }
    }
    else
    {
      for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
        ((dword *)(&resource_profile))[i] = ((dword *)(&(plci->reserve_request_profile)))[i];
    }
    WRITE_WORD (&result[1], READ_WORD (parms[0].info));
    WRITE_WORD (&result[4], GOOD);
    result[3] = 2;
    result[0] = result[3] + 3;
    reserve_cmd = 0;
    reserve_cmd_number = 0;
    offs = 0;
    do
    {
      if (api_parse (&parms[1].info[1], parms[1].length, "bs", request_parms))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Wrong message format",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
        WRITE_WORD (&result[4], _WRONG_MESSAGE_FORMAT);
      }
      else
      {
        i = READ_WORD (parms[0].info);
        reserve_cmd |= i << (3 * reserve_cmd_number);
        switch (i)
        {
        case RESERVE_PROFILE:
          resource_profile.global_options = 0;
          resource_profile.b1_protocols = 0;
          resource_profile.b2_protocols = 0;
          resource_profile.b3_protocols = 0;
          resource_profile.manufacturer_features &= (MANUFACTURER_FEATURE_RTP | MANUFACTURER_FEATURE_T38);
          resource_profile.manufacturer_features2 = 0;
          if (request_parms[1].length >= 8)
            resource_profile.global_options = READ_DWORD (&(request_parms[1].info[5]));
          if (request_parms[1].length >= 12)
            resource_profile.b1_protocols = READ_DWORD (&(request_parms[1].info[9]));
          if (request_parms[1].length >= 16)
            resource_profile.b2_protocols = READ_DWORD (&(request_parms[1].info[13]));
          if (request_parms[1].length >= 20)
            resource_profile.b3_protocols = READ_DWORD (&(request_parms[1].info[17]));
          if (request_parms[1].length >= 48)
            private_options = READ_DWORD (&(request_parms[1].info[45]));

          if (resource_profile.global_options & GL_ECHO_CANCELLER_SUPPORTED_NEW)
            resource_profile.manufacturer_features |= MANUFACTURER_FEATURE_ECHO_CANCELLER;
          break;

        case RESERVE_RTP_PAYLOAD_PROT:
          resource_profile.manufacturer_features &= ~MANUFACTURER_FEATURE_RTP;
          resource_profile.rtp_primary_payloads &= (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
          resource_profile.rtp_additional_payloads = 0;
          primary_payloads = 0;
          additional_payloads = 0;
          if (request_parms[1].length >= 4)
            primary_payloads = READ_DWORD (&(request_parms[1].info[1]));
          if (request_parms[1].length >= 8)
            additional_payloads = READ_DWORD (&(request_parms[1].info[5]));
          resource_profile.rtp_primary_payloads |= primary_payloads & ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
          resource_profile.rtp_additional_payloads |= additional_payloads;
          if ((primary_payloads != 0) || (additional_payloads != 0))
            resource_profile.manufacturer_features |= MANUFACTURER_FEATURE_RTP;
          break;

        case RESERVE_T38_PAYLOAD_PROT:
          resource_profile.manufacturer_features &= ~MANUFACTURER_FEATURE_T38;
          resource_profile.rtp_primary_payloads &= ~(((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
          primary_payloads = 0;
          additional_payloads = 0;
          if (request_parms[1].length >= 4)
            primary_payloads = READ_DWORD (&(request_parms[1].info[1]));
          if (request_parms[1].length >= 8)
            additional_payloads = READ_DWORD (&(request_parms[1].info[5]));
          resource_profile.rtp_primary_payloads |= primary_payloads & (((dword) 1) << (RTP_PRIM_PAYLOAD_T38 & 0x1f));
          if ((primary_payloads != 0) || (additional_payloads != 0))
            resource_profile.manufacturer_features |= MANUFACTURER_FEATURE_T38;
          break;

        default:
          dbug (1, dprintf ("[%06lx] %s,%d: Resource reservation unknown request %04x",
            (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->rtp_cmd));
          WRITE_WORD (&result[4], _FACILITY_SPECIFIC_FUNCTION_NOT_SUPP);
        }
        reserve_cmd_number++;
        offs += (word)((&(parms[1].info[1+parms[1].length])) - parms[0].info);
      }
    } while ((reserve_cmd_number < 5)
      && (READ_WORD (&result[4]) == GOOD)
      && (api_parse (&msg[1].info[1+offs], (word)(msg[1].length - offs), "ws", parms) == 0));

    if (READ_WORD (&result[4]) == GOOD)
    {
      if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
      {
        if (plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_REQUEST_PENDING)
          indicate_reserve_grant (plci, appl);
        if ((plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) != 0)
        {
          p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
          if (p_index->references != 0)
            (p_index->references)--;
          plci->c_ind_reserve_info[appl->Id-1] &= ~RESERVE_INFO_PROFILE_INDEX_MASK;
        }
        n = 0;
        p_index = c_ind_resource_index;
        do
        {
          i = 0;
          if ((p_index->references != 0) && (p_index->cmd == reserve_cmd))
          {
            while ((i < RESOURCE_PROFILE_DWORDS)
              && (((dword *)(&(p_index->profile)))[i] == ((dword *)(&resource_profile))[i]))
            {
              i++;
            }
          }
          n++;
          p_index++;
        } while ((n < C_IND_RESOURCE_INDEX_ENTRIES) && (i < RESOURCE_PROFILE_DWORDS));
        if (i == RESOURCE_PROFILE_DWORDS)
        {
          n--;
          p_index--;
        }
        else
        {
          n = 0;
          p_index = c_ind_resource_index;
          while ((n < C_IND_RESOURCE_INDEX_ENTRIES) && (c_ind_resource_index[i].references != 0))
          {
            n++;
            p_index++;
          }
          if (n == C_IND_RESOURCE_INDEX_ENTRIES)
          {
            sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
              "wws", _RESOURCE_ERROR, PRIV_SELECTOR_RESOURCE_RESERVATION, result);
            return (0);
          }
          p_index->cmd = reserve_cmd;
          for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
            ((dword *)(&(p_index->profile)))[i] = ((dword *)(&resource_profile))[i];
        }
        (p_index->references)++;
        for (j = 0; j < max_appl; j++)
        {
          if ((plci->c_ind_reserve_info[j] & RESERVE_INFO_PROFILE_INDEX_MASK) != 0)
          {
            p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[j] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
            for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
              ((dword *)(&resource_profile))[i] |= ((dword *)(&(p_index->profile)))[i];
          }
        }
        plci->c_ind_reserve_info[appl->Id-1] |= n + 1;
        plci->c_ind_reserve_info[appl->Id-1] |= RESERVE_INFO_REQUEST_PENDING;
        plci->c_ind_reserve_msg_number[appl->Id-1] = Number;
      }
      else
      {
        if (plci->reserve_request_pending)
          indicate_reserve_grant (plci, appl);
        plci->reserve_request_pending = TRUE;
        plci->reserve_pending_cmd = reserve_cmd;
        plci->reserve_pending_msg_number = Number;
      }
      for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
        ((dword *)(&(plci->reserve_request_profile)))[i] = ((dword *)(&resource_profile))[i];
      if (plci->reserve_send_pending)
        plci->reserve_send_request = TRUE;
      else
      {
        plci->reserve_send_pending = TRUE;
        if (plci->appl != NULL)
        {
          plci->internal_command = RESERVE_REQ;
          plci->command = 0;
        }
        add_reserve_request (plci);
        send_req (plci);
      }
      if (!plci->reserve_send_pending)
        indicate_reserve_grant (plci, appl);
      return (0);
    }
  }
  sendf (appl, _FACILITY_R | CONFIRM, Id & 0xffffL, Number,
    "wws", Info, PRIV_SELECTOR_RESOURCE_RESERVATION, result);
  return (0);
}


static void resource_reservation_indication (PLCI   * plci, byte   *esc_profile)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: resource_reservation_indication",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->reserve_send_pending = FALSE;
  if ((esc_profile == NULL) || (esc_profile[0] < 1 + sizeof(API_PROFILE)))
  {
    plci->reserve_send_request = FALSE;
  }
  else
  {
    plci->reserve_granted_profile.global_options = READ_DWORD (&esc_profile[6]);
    plci->reserve_granted_profile.b1_protocols = READ_DWORD (&esc_profile[10]);
    plci->reserve_granted_profile.b2_protocols = READ_DWORD (&esc_profile[14]);
    plci->reserve_granted_profile.b3_protocols = READ_DWORD (&esc_profile[18]);
    plci->reserve_granted_profile.manufacturer_features = READ_DWORD (&esc_profile[46]);
    plci->reserve_granted_profile.rtp_primary_payloads = READ_DWORD (&esc_profile[50]);
    plci->reserve_granted_profile.rtp_additional_payloads = READ_DWORD (&esc_profile[54]);
    plci->reserve_granted_profile.manufacturer_features2 = READ_DWORD (&esc_profile[58]);
  }
  if (plci->reserve_send_request)
  {
    plci->reserve_send_request = FALSE;
    if ((plci->State != IDLE)
     && (plci->State != INC_DIS_PENDING))
    {
      plci->reserve_send_pending = TRUE;
      add_reserve_request (plci);
      send_req (plci);
    }
  }
  else
  {
    if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
    {
      for (i = 0; i < max_appl; i++)
      {
        if (plci->c_ind_reserve_info[i] & RESERVE_INFO_REQUEST_PENDING)
          indicate_reserve_grant (plci, &(application[i]));
      }
    }
    else
    {
      if (plci->reserve_request_pending
       && (plci->State != IDLE)
       && (plci->State != INC_DIS_PENDING)
       && plci->appl)
      {
        indicate_reserve_grant (plci, plci->appl);
      }
    }
  }
}


static void reserve_clear_incoming_call (PLCI   *plci)
{
  struct resource_index_entry   *p_index;
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: reserve_clear_incoming_call",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  for (i = 0; i < max_appl; i++)
  {
    if (plci->c_ind_reserve_info[i] & RESERVE_INFO_PROFILE_INDEX_MASK)
    {
      p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[i] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
      if (p_index->references != 0)
        (p_index->references)--;
      plci->c_ind_reserve_info[i] = 0;
    }
  }
}


static void reserve_assign_incoming_call (PLCI   *plci, APPL   *appl)
{
  struct resource_index_entry   *p_index;
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: reserve_assign_incoming_call",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  i = 0;
  while ((i < RESOURCE_PROFILE_DWORDS) && (((dword *)(&(plci->reserve_request_profile)))[i] == 0))
    i++;
  if (i < RESOURCE_PROFILE_DWORDS)
  {
    if (plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK)
    {
      p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
      for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
        ((dword *)(&(plci->reserve_request_profile)))[i] = ((dword *)(&(p_index->profile)))[i];
    }
    else
    {
      for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
        ((dword *)(&(plci->reserve_request_profile)))[i] = 0;
    }
    if (plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_REQUEST_PENDING)
      plci->reserve_request_pending = TRUE;
    if (plci->reserve_send_pending)
      plci->reserve_send_request = TRUE;
    else
    {
      plci->reserve_send_pending = TRUE;
      add_reserve_request (plci);
    }
  }
  for (i = 0; i < max_appl; i++)
  {
    if (plci->c_ind_reserve_info[i] & RESERVE_INFO_PROFILE_INDEX_MASK)
    {
      p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[i] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
      if (p_index->references != 0)
        (p_index->references)--;
      plci->c_ind_reserve_info[i] = 0;
    }
  }
}


static void reserve_ignore_incoming_call (PLCI   *plci, APPL   *appl)
{
  struct resource_index_entry   *p_index;
    struct resource_profile_s resource_profile;
  word i, j;

  dbug (1, dprintf ("[%06lx] %s,%d: reserve_ignore_incoming_call",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  if (plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK)
  {
    p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[appl->Id-1] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
    if (p_index->references != 0)
      (p_index->references)--;
    plci->c_ind_reserve_info[appl->Id-1] = 0;
  }
  for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
    ((dword *)(&resource_profile))[i] = 0;
  for (j = 0; j < max_appl; j++)
  {
    if ((plci->c_ind_reserve_info[j] & RESERVE_INFO_PROFILE_INDEX_MASK) != 0)
    {
      p_index = &(c_ind_resource_index[(plci->c_ind_reserve_info[j] & RESERVE_INFO_PROFILE_INDEX_MASK) - 1]);
      for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
        ((dword *)(&resource_profile))[i] |= ((dword *)(&(p_index->profile)))[i];
    }
  }
  i = 0;
  while ((i < RESOURCE_PROFILE_DWORDS)
    && (((dword *)(&resource_profile))[i] == ((dword *)(&(plci->reserve_request_profile)))[i]))
  {
    i++;
  }
  if (i < RESOURCE_PROFILE_DWORDS)
  {
    for (i = 0; i < RESOURCE_PROFILE_DWORDS; i++)
      ((dword *)(&(plci->reserve_request_profile)))[i] = ((dword *)(&resource_profile))[i];
    if (plci->reserve_send_pending)
      plci->reserve_send_request = TRUE;
    else
    {
      plci->reserve_send_pending = TRUE;
      if (plci->appl != NULL)
      {
        plci->internal_command = RESERVE_REQ;
        plci->command = 0;
      }
      add_reserve_request (plci);
      send_req (plci);
    }
  }
}


/*------------------------------------------------------------------*/
/* B1 resource switching                                            */
/*------------------------------------------------------------------*/

static word b1_facilities_table[] =
{
  0x0000,  /* 0  No bchannel resources      */
  0x0000,  /* 1  Codec (automatic law)      */
  0x0000,  /* 2  Codec (A-law)              */
  0x0000,  /* 3  Codec (y-law)              */
  0x0000,  /* 4  HDLC for X.21              */
  0x0000,  /* 5  HDLC                       */
  0x0000,  /* 6  External Device 0          */
  0x0000,  /* 7  External Device 1          */
  0x0000,  /* 8  HDLC 56k                   */
  0x0000,  /* 9  Transparent                */
  0x0000,  /* 10 Loopback to network        */
  0x0000,  /* 11 Test pattern to net        */
  0x0000,  /* 12 Rate adaptation sync       */
  0x0000,  /* 13 Rate adaptation async      */
  0x0000,  /* 14 R-Interface                */
  0x0000,  /* 15 HDLC 128k leased line      */
  0x0000,  /* 16 FAX                        */
  0x0000,  /* 17 Modem async                */
  0x0000,  /* 18 Modem sync HDLC            */
  0x0000,  /* 19 V.110 async HDLC           */
  0x0012,  /* 20 Adv voice (Trans,mixer)    */
  0x0000,  /* 21 Codec connected to IC      */
  0x000c,  /* 22 Trans,DTMF                 */
  0x001e,  /* 23 Trans,DTMF+mixer           */
  0x001f,  /* 24 Trans,DTMF+mixer+local     */
  0x0013,  /* 25 Trans,mixer+local          */
  0x0012,  /* 26 HDLC,mixer                 */
  0x0012,  /* 27 HDLC 56k,mixer             */
  0x026c,  /* 28 Trans,LEC+TONE             */
  0x027e,  /* 29 Trans,LEC+TONE+mixer       */
  0x027f,  /* 30 Trans,LEC+TONE+mixer+local */
  0x002c,  /* 31 RTP,LEC+DTMF               */
  0x003e,  /* 32 RTP,LEC+DTMF+mixer         */
  0x003f,  /* 33 RTP,LEC+DTMF+mixer+local   */
  0x0000,  /* 34 Signaling task             */
  0x0000,  /* 35 PIAFS                      */
  0x02cc,  /* 36 Trans,DTMF+TONE            */
  0x02de,  /* 37 Trans,DTMF+TONE+mixer      */
  0x02df,  /* 38 Trans,DTMF+TONE+mixer+local*/
  0x0000,  /* 39 HDLC inverted              */
  0x0000,  /* 40 MTP1                       */
  0x0000,  /* 41 MTP1 extended format       */
  0x0100,  /* 42 Trans,measure              */
  0x0112,  /* 43 Trans,measure+mixer        */
  0x0113   /* 44 Trans,measure+mixer+local  */
};


static word get_b1_facilities (PLCI   * plci, word b1_resource)
{
  word b1_facilities;

  b1_facilities = b1_facilities_table[b1_resource & 0xff];
  if ((b1_resource == 9) || (b1_resource == 20) || (b1_resource == 25))
  {
    if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_SEND)
      b1_facilities |= B1_FACILITY_DTMFX;
    if (plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE)
      b1_facilities |= B1_FACILITY_DTMFR;
  }
  if (((b1_resource & 0xff) == 17) || ((b1_resource & 0xff) == 18))
  {
    if (plci->adapter->manufacturer_features & (MANUFACTURER_FEATURE_V18 | MANUFACTURER_FEATURE_VOWN))
      b1_facilities |= B1_FACILITY_DTMFX | B1_FACILITY_DTMFR;
  }
  if ((b1_resource & 0x0f00) == 0x0100)
  {
    b1_facilities |= B1_FACILITY_MIXER;
  }
/*
  dbug (1, dprintf ("[%06lx] %s,%d: get_b1_facilities %04x %04x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char far *)(FILE_), __LINE__, b1_resource, b1_facilites));
*/
  return (b1_facilities);
}


static word add_b1_facilities (PLCI   * plci, word b1_resource, word b1_facilities)
{
  word b;

  switch (b1_resource)
  {
  case 5:
  case 26:
    if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 26;
    else
      b = 5;
    break;

  case 8:
  case 27:
    if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 27;
    else
      b = 8;
    break;

  case 9:
  case 20:
  case 22:
  case 23:
  case 24:
  case 25:
  case 28:
  case 29:
  case 30:
  case 36:
  case 37:
  case 38:
  case 42:
  case 43:
  case 44:
    if (b1_facilities & B1_FACILITY_EC)
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 30;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 29;
      else
        b = 28;
    }

    else if (b1_facilities & B1_FACILITY_PITCH)
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 38;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 37;
      else
        b = 36;
    }


    else if (b1_facilities & B1_FACILITY_DTMFTONE)
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 38;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 37;
      else
        b = 36;
    }


    else if (b1_facilities & B1_FACILITY_GENTONE)
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 38;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 37;
      else
        b = 36;
    }
    else if (b1_facilities & B1_FACILITY_MEASURE)
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 44;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 43;
      else
        b = 42;
    }

    else if (((plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_HARDDTMF)
      && !(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE)
      && ((plci->B1_options & DSP_CAI_ARRANGEMENT_MASK) == DSP_CAI_ARRANGEMENT_CONNECTION))
     || ((b1_facilities & B1_FACILITY_DTMFR)
      && ((b1_facilities & B1_FACILITY_MIXER)
       || !(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_RECEIVE)))
     || ((b1_facilities & B1_FACILITY_DTMFX)
      && ((b1_facilities & B1_FACILITY_MIXER)
       || !(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_SOFTDTMF_SEND))))
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 24;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 23;
      else
        b = 22;
    }
    else
    {
      if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
        b = 25;
      else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
        b = 20;
      else
        b = 9;
    }
    break;

  case 31:
  case 32:
  case 33:
    if (b1_facilities & (B1_FACILITY_LOCAL | B1_FACILITY_LINEAR16))
      b = 33;
    else if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 32;
    else
      b = 31;
    break;

  case 16:
  case 0x100 | 16:
    if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 0x100 | 16;
    else
      b = 16;
    break;

  case 17:
  case 0x100 | 17:
    if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 0x100 | 17;
    else
      b = 17;
    break;

  case 18:
  case 0x100 | 18:
    if (b1_facilities & (B1_FACILITY_MIXER | B1_FACILITY_VOICE))
      b = 0x100 | 18;
    else
      b = 18;
    break;

  default:
    b = b1_resource;
  }
  dbug (1, dprintf ("[%06lx] %s,%d: add_b1_facilities %04x %04x %04x %04x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__,
    b1_resource, b1_facilities, b, get_b1_facilities (plci, b)));
  return (b);
}


static void adjust_b1_facilities (PLCI   *plci, word new_b1_resource, word new_b1_facilities)
{
  word removed_facilities;

  dbug (1, dprintf ("[%06lx] %s,%d: adjust_b1_facilities %04x %04x %04x",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__, new_b1_resource, new_b1_facilities,
    new_b1_facilities & get_b1_facilities (plci, new_b1_resource)));

  new_b1_facilities &= get_b1_facilities (plci, new_b1_resource);
  removed_facilities = plci->B1_facilities & ~new_b1_facilities;

  if (removed_facilities & B1_FACILITY_EC)
    ec_clear_config (plci);


  if (removed_facilities & B1_FACILITY_PITCH)
    pitch_parameter_clear_config (plci);


  if (removed_facilities & B1_FACILITY_DTMFR)
  {
    dtmf_rec_clear_config (plci);
    dtmf_parameter_clear_config (plci);
  }
  if (removed_facilities & B1_FACILITY_DTMFX)
    dtmf_send_clear_config (plci);

  if (removed_facilities & B1_FACILITY_VOICE)
    adv_voice_clear_config (plci);

  if (removed_facilities & B1_FACILITY_MIXER)
    mixer_clear_config (plci);


  if ((plci->B1_facilities & (B1_FACILITY_MEASURE | B1_FACILITY_GENTONE))
   && !(new_b1_facilities & (B1_FACILITY_MEASURE | B1_FACILITY_GENTONE)))
  {
    measure_clear_config (plci);
  }

  plci->B1_facilities = new_b1_facilities;
}


static void adjust_b_clear (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: adjust_b_clear",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->adjust_b_restore = FALSE;
}


static word adjust_b_process (dword Id, PLCI   *plci, byte Rc)
{
  word Info, i, b1_resource;
  byte call_dir;
  NCCI   * ncci_ptr;
    API_PARSE bp[2];

  dbug (1, dprintf ("[%06lx] %s,%d: adjust_b_process %02x %d",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->adjust_b_state));

  Info = GOOD;
  switch (plci->adjust_b_state)
  {
  case ADJUST_B_START:
    plci->adjust_b_info = GOOD;
    plci->adjust_b_prev_b1_resource = plci->B1_resource;
    plci->adjust_b_prev_b1_facilities = plci->B1_facilities;
    if ((plci->adjust_b_parms_msg == NULL)
     && (plci->adjust_b_mode & ADJUST_B_MODE_SWITCH_L1)
     && ((plci->adjust_b_mode & (ADJUST_B_MODE_REMOVE_L23 | ADJUST_B_MODE_ASSIGN_L23 |
         ADJUST_B_MODE_USER_CONNECT | ADJUST_B_MODE_CONNECT)) == 0))
    {
      b1_resource = (plci->adjust_b_mode == ADJUST_B_MODE_NO_RESOURCE) ?
        0 : add_b1_facilities (plci, plci->B1_resource, plci->adjust_b_facilities);
      if (b1_resource == plci->B1_resource)
      {
        adjust_b1_facilities (plci, b1_resource, plci->adjust_b_facilities);
        break;
      }
      if (plci->adjust_b_facilities & ~get_b1_facilities (plci, b1_resource))
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B nonsupported facilities %04x %04x %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
          plci->B1_resource, b1_resource, plci->adjust_b_facilities));
        Info = _WRONG_STATE;
        break;
      }
    }
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      measure_prepare_switch (Id, plci);


      mixer_prepare_switch (Id, plci);


      pitch_parameter_prepare_switch (Id, plci);


      audio_parameter_prepare_switch (Id, plci);

      sig_tx_prepare_switch (Id, plci);

      dtmf_prepare_switch (Id, plci);
      dtmf_parameter_prepare_switch (Id, plci);


      ec_prepare_switch (Id, plci);

      adv_voice_prepare_switch (Id, plci);
    }
    plci->adjust_b_state = ADJUST_B_SAVE_MEASURE_1;
    Rc = OK;
  case ADJUST_B_SAVE_MEASURE_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = measure_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_MIXER_1;
    Rc = OK;
  case ADJUST_B_SAVE_MIXER_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = mixer_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_DTMF_1;
    Rc = OK;
  case ADJUST_B_SAVE_DTMF_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = dtmf_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_REMOVE_L23_1;
  case ADJUST_B_REMOVE_L23_1:
    if ((plci->adjust_b_mode & ADJUST_B_MODE_REMOVE_L23)
     && plci->NL.Id && !plci->nl_remove_id)
    {
      plci->internal_command = plci->adjust_b_command;
      if (plci->adjust_b_ncci != 0)
      {
        ncci_ptr = &(plci->adapter->ncci[plci->adjust_b_ncci]);
        while (ncci_ptr->data_pending)
        {
          data_rc (plci, plci->adapter->ncci_ch[plci->adjust_b_ncci],
            ncci_ptr->DBuffer[ncci_ptr->data_out].P);
        }
        while (ncci_ptr->data_ack_pending)
          data_ack (plci, plci->adapter->ncci_ch[plci->adjust_b_ncci]);
      }
      nl_req_ncci (plci, REMOVE,
        (word)((plci->adjust_b_mode & ADJUST_B_MODE_CONNECT) ? plci->adjust_b_ncci : 0));
      send_req (plci);
      plci->adjust_b_state = ADJUST_B_REMOVE_L23_2;
      break;
    }
    plci->adjust_b_state = ADJUST_B_REMOVE_L23_2;
    Rc = OK;
  case ADJUST_B_REMOVE_L23_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Adjust B remove failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci->adjust_b_mode & ADJUST_B_MODE_REMOVE_L23)
    {
      if (plci_nl_busy (plci))
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
    }
    plci->adjust_b_state = ADJUST_B_SAVE_EC_1;
    Rc = OK;
  case ADJUST_B_SAVE_EC_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = ec_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_DTMF_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_SAVE_DTMF_PARAMETER_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = dtmf_parameter_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_SIG_TX_1;
    Rc = OK;
  case ADJUST_B_SAVE_SIG_TX_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {
      Info = sig_tx_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;
    }
    plci->adjust_b_state = ADJUST_B_SAVE_AUDIO_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_SAVE_AUDIO_PARAMETER_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = audio_parameter_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_PITCH_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_SAVE_PITCH_PARAMETER_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {

      Info = pitch_parameter_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_SAVE_VOICE_1;
    Rc = OK;
  case ADJUST_B_SAVE_VOICE_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SAVE)
    {
      Info = adv_voice_save_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;
    }
    plci->adjust_b_state = ADJUST_B_SWITCH_L1_1;
  case ADJUST_B_SWITCH_L1_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_SWITCH_L1)
    {
      if (plci->sig_req)
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
      if (plci->adjust_b_parms_msg != NULL)
        api_swap_msg (&plci->B_protocol, plci->adjust_b_parms_msg);
      api_load_msg (&plci->B_protocol, bp);
      plci->adjust_b_info = add_b1 (plci, bp,
        (word)((plci->adjust_b_mode & ADJUST_B_MODE_NO_RESOURCE) ? 2 : 0),
        plci->adjust_b_facilities);
      if (plci->adjust_b_info != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B invalid L1 parameters %04x %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
          plci->B1_resource, plci->adjust_b_facilities));
      }
      else
      {
        plci->internal_command = plci->adjust_b_command;
        sig_req (plci, RESOURCES, 0);
        send_req (plci);
        plci->adjust_b_state = ADJUST_B_SWITCH_L1_2;
        break;
      }
    }
    plci->adjust_b_state = ADJUST_B_SWITCH_L1_2;
    Rc = OK;
  case ADJUST_B_SWITCH_L1_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      DBG_FTL(("[%06lx] %s,%d: Adjust B switch failed %02x %04x %04x %04x %04x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
        Rc, plci->B1_resource, plci->adjust_b_facilities,
        plci->adjust_b_prev_b1_resource, plci->adjust_b_prev_b1_facilities));
      plci->adjust_b_info = _WRONG_STATE;
      if ((Rc == OUT_OF_RESOURCES) || (Rc == OUT_OF_LICENSES))
      {

        if (plci->appl
         && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
           & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
        {
          if (Rc == OUT_OF_LICENSES)
            plci->adjust_b_info = INFO_OUT_OF_B_LICENSES;
          else
            plci->adjust_b_info = INFO_OUT_OF_B_RESOURCES;
        }
        else

        {
          if (((plci->B1_resource & 0xff) == 16) && ((plci->adjust_b_prev_b1_resource & 0xff) != 16))
            plci->adjust_b_info = _OUT_OF_FAX;
          else if ((plci->adjust_b_facilities & ~plci->adjust_b_prev_b1_facilities) & B1_FACILITY_MIXER)
            plci->adjust_b_info = _OUT_OF_INTERCONNECT_RESOURCES;
        }
      }
    }
    if (plci->adjust_b_info == GOOD)
    {
      if (plci->adjust_b_mode & ADJUST_B_MODE_SWITCH_L1)
      {
        if (plci->adjust_b_parms_msg != NULL)
        {
          api_load_msg (&plci->B_protocol, bp);
          api_save_msg (bp, "s", plci->adjust_b_parms_msg);
        }
      }
    }
    else
    {
      if (plci->adjust_b_parms_msg != NULL)
        api_swap_msg (&plci->B_protocol, plci->adjust_b_parms_msg);
      plci->B1_resource = plci->adjust_b_prev_b1_resource;
      plci->B1_facilities = plci->adjust_b_prev_b1_facilities;
    }
    plci->adjust_b_state = ADJUST_B_UNSWITCH_L1_1;
    Rc = OK;
  case ADJUST_B_UNSWITCH_L1_1:
    if (plci->adjust_b_info != GOOD)
    {
      if (plci->sig_req)
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
      api_load_msg (&plci->B_protocol, bp);
      Info = add_b1 (plci, bp, (word)((plci->B1_resource == 0) ? 2 : 0), plci->B1_facilities);
      if (Info != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B unswitch invalid L1 parameters %04x %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
          plci->B1_resource, plci->adjust_b_facilities));
        break;
      }
      plci->internal_command = plci->adjust_b_command;
      sig_req (plci, RESOURCES, 0);
      send_req (plci);
      plci->adjust_b_state = ADJUST_B_UNSWITCH_L1_2;
      break;
    }
    plci->adjust_b_state = ADJUST_B_UNSWITCH_L1_2;
    Rc = OK;
  case ADJUST_B_UNSWITCH_L1_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      DBG_FTL(("[%06lx] %s,%d: Adjust B unswitch failed %02x %04x %04x %04x %04x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
        Rc, plci->B1_resource, plci->adjust_b_facilities,
        plci->adjust_b_prev_b1_resource, plci->adjust_b_prev_b1_facilities));
      Info = _WRONG_STATE;
      break;
    }
    plci->adjust_b_state = ADJUST_B_RESTORE_VOICE_1;
    Rc = OK;
  case ADJUST_B_RESTORE_VOICE_1:
  case ADJUST_B_RESTORE_VOICE_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {
      Info = adv_voice_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;
    }
    plci->adjust_b_state = ADJUST_B_RESTORE_PITCH_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_RESTORE_PITCH_PARAMETER_1:
  case ADJUST_B_RESTORE_PITCH_PARAMETER_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = pitch_parameter_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_RESTORE_AUDIO_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_RESTORE_AUDIO_PARAMETER_1:
  case ADJUST_B_RESTORE_AUDIO_PARAMETER_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = audio_parameter_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_RESTORE_SIG_TX_1;
    Rc = OK;
  case ADJUST_B_RESTORE_SIG_TX_1:
  case ADJUST_B_RESTORE_SIG_TX_2:
  case ADJUST_B_RESTORE_SIG_TX_3:
  case ADJUST_B_RESTORE_SIG_TX_4:
  case ADJUST_B_RESTORE_SIG_TX_5:
  case ADJUST_B_RESTORE_SIG_TX_6:
  case ADJUST_B_RESTORE_SIG_TX_7:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {
      Info = sig_tx_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;
    }
    plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_PARAMETER_1;
    Rc = OK;
  case ADJUST_B_RESTORE_DTMF_PARAMETER_1:
  case ADJUST_B_RESTORE_DTMF_PARAMETER_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = dtmf_parameter_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_RESTORE_EC_1;
    Rc = OK;
  case ADJUST_B_RESTORE_EC_1:
  case ADJUST_B_RESTORE_EC_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = ec_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_ASSIGN_L23_1;
  case ADJUST_B_ASSIGN_L23_1:
    if ((plci->adjust_b_mode & ADJUST_B_MODE_ASSIGN_L23)
     && (plci->B1_resource != 0))
    {
      if (plci_nl_busy (plci))
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
      api_load_msg (&plci->B_protocol, bp);
      call_dir = plci->call_dir;
      if (plci->adjust_b_mode & ADJUST_B_MODE_CONNECT)
        plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;
      Info = add_b23 (plci, bp);
      plci->call_dir = call_dir;
      if (Info != GOOD)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B invalid L23 parameters %04x",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Info));
        break;
      }
      plci->internal_command = plci->adjust_b_command;
      nl_req_ncci (plci, ASSIGN, 0);
      send_req (plci);
      plci->adjust_b_state = ADJUST_B_ASSIGN_L23_2;
      break;
    }
    plci->adjust_b_state = ADJUST_B_ASSIGN_L23_2;
    Rc = ASSIGN_OK;
  case ADJUST_B_ASSIGN_L23_2:
    if ((Rc != OK) && (Rc != OK_FC) && (Rc != ASSIGN_OK))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Adjust B assign failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;

      if (plci->appl
       && ((plci->requested_options_conn | plci->requested_options | plci->adapter->requested_options_table[plci->appl->Id-1])
         & ((1L << PRIVATE_RTP) | (1L << PRIVATE_T38))))
      {
        if (Rc == (ASSIGN_RC | WRONG_CH))
          Info = INFO_RELATED_RESOURCE_ERROR;
      }

      break;
    }
    if (plci->adjust_b_mode & ADJUST_B_MODE_ASSIGN_L23)
    {
      if (Rc != ASSIGN_OK)
      {
        plci->internal_command = plci->adjust_b_command;
        break;
      }
      if (plci->hangup_flow_ctrl_timer != 0)
      {
        dbug (1, dprintf ("[%06lx] %s,%d: Adjust B hangup during assign %d",
          (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, plci->hangup_flow_ctrl_timer));

        if (plci->adjust_b_ncci != 0)
        {
          ncci_free_receive_buffers (plci->appl, plci, plci->adjust_b_ncci);
          ncci_remove (plci, plci->adjust_b_ncci, FALSE);

          if (plci->appl)
          {
            for(i=0; (i<MAX_CHANNELS_PER_PLCI) && plci->inc_dis_ncci_table[i]; i++);
            if (i<MAX_CHANNELS_PER_PLCI)
            {
              plci->inc_dis_ncci_table[i] = (byte)(plci->adjust_b_ncci);
              trcq (plci);
            }
            else
            {
              if(plci->channels)plci->channels--;
              trcq (plci);
            }
            sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"wS",0,plci->ncpi_buffer);
          }
          else
          {
            if(plci->channels)plci->channels--;
            trcq (plci);
          }
          plci->ncpi_state = 0;
        }
        Info = _WRONG_STATE;
        break;
      }
    }
    if (plci->adjust_b_mode & ADJUST_B_MODE_USER_CONNECT)
    {
      plci->adjust_b_restore = TRUE;
      Info = plci->adjust_b_info;
      plci->adjust_b_info = GOOD;
      break;
    }
    plci->adjust_b_state = ADJUST_B_CONNECT_1;
  case ADJUST_B_CONNECT_1:
    if (plci->adjust_b_mode & ADJUST_B_MODE_CONNECT)
    {
      plci->internal_command = plci->adjust_b_command;
      if (plci_nl_busy (plci))
        break;
      nl_req_ncci (plci, N_CONNECT, 0);
      send_req (plci);
      plci->adjust_b_state = ADJUST_B_CONNECT_2;
      break;
    }
    plci->adjust_b_state = ADJUST_B_CONNECT_5;
    Rc = OK;
  case ADJUST_B_CONNECT_2:
  case ADJUST_B_CONNECT_3:
  case ADJUST_B_CONNECT_4:
    if ((Rc != OK) && (Rc != OK_FC) && (Rc != 0))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Adjust B connect failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (Rc == OK)
    {
      if (plci->adjust_b_mode & ADJUST_B_MODE_CONNECT)
      {
        get_ncci (plci, (byte)(Id >> 16), plci->adjust_b_ncci);
        Id = (Id & 0xffff) | (((dword)(plci->adjust_b_ncci)) << 16);
      }
      if (plci->adjust_b_state == ADJUST_B_CONNECT_2)
        plci->adjust_b_state = ADJUST_B_CONNECT_3;
      else if (plci->adjust_b_state == ADJUST_B_CONNECT_4)
        plci->adjust_b_state = ADJUST_B_CONNECT_5;
    }
    else if (Rc == 0)
    {
      if (plci->adjust_b_state == ADJUST_B_CONNECT_2)
        plci->adjust_b_state = ADJUST_B_CONNECT_4;
      else if (plci->adjust_b_state == ADJUST_B_CONNECT_3)
        plci->adjust_b_state = ADJUST_B_CONNECT_5;
    }
    if (plci->adjust_b_state != ADJUST_B_CONNECT_5)
    {
      plci->internal_command = plci->adjust_b_command;
      break;
    }
  case ADJUST_B_CONNECT_5:
    plci->adjust_b_state = ADJUST_B_RESTORE_DTMF_1;
    Rc = OK;
  case ADJUST_B_RESTORE_DTMF_1:
  case ADJUST_B_RESTORE_DTMF_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = dtmf_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_RESTORE_MIXER_1;
    Rc = OK;
  case ADJUST_B_RESTORE_MIXER_1:
  case ADJUST_B_RESTORE_MIXER_2:
  case ADJUST_B_RESTORE_MIXER_3:
  case ADJUST_B_RESTORE_MIXER_4:
  case ADJUST_B_RESTORE_MIXER_5:
  case ADJUST_B_RESTORE_MIXER_6:
  case ADJUST_B_RESTORE_MIXER_7:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = mixer_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_RESTORE_MEASURE_1;
    Rc = OK;
  case ADJUST_B_RESTORE_MEASURE_1:
  case ADJUST_B_RESTORE_MEASURE_2:
    if (plci->adjust_b_mode & ADJUST_B_MODE_RESTORE)
    {

      Info = measure_restore_config (Id, plci, Rc);
      if ((Info != GOOD) || plci->internal_command)
        break;

    }
    plci->adjust_b_state = ADJUST_B_END;
  case ADJUST_B_END:
    Info = plci->adjust_b_info;
    break;
  }
  return (Info);
}


static void adjust_b1_resource (dword Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_facilities, word internal_command)
{

  dbug (1, dprintf ("[%06lx] %s,%d: adjust_b1_resource %04x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
    plci->B1_resource, b1_facilities));

  plci->adjust_b_parms_msg = bp_msg;
  plci->adjust_b_facilities = b1_facilities;
  plci->adjust_b_command = internal_command;
  plci->adjust_b_ncci = (word)(Id >> 16);
  if (((bp_msg == NULL) && (plci->B1_resource == 0))
   || ((bp_msg != NULL) && ((bp_msg->parms[0].length < 2) || (READ_WORD(&(bp_msg->parms[0].info[1])) == B1_NO_RESOURCE))))
  {
    plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_NO_RESOURCE | ADJUST_B_MODE_SWITCH_L1;
  }
  else
  {
    plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_SWITCH_L1 | ADJUST_B_MODE_RESTORE;
  }
  plci->adjust_b_state = ADJUST_B_START;
  dbug (1, dprintf ("[%06lx] %s,%d: Adjust B1 resource %04x %04x...",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__,
    plci->B1_resource, b1_facilities));
}


static void adjust_b_restore (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: adjust_b_restore %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    if (plci->req_in != 0)
    {
      plci->internal_command = ADJUST_B_RESTORE_1;
      break;
    }
    Rc = OK;
  case ADJUST_B_RESTORE_1:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Adjust B enqueued failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
    }
    plci->adjust_b_parms_msg = NULL;
    plci->adjust_b_facilities = plci->B1_facilities;
    plci->adjust_b_command = ADJUST_B_RESTORE_2;
    plci->adjust_b_ncci = (word)(Id >> 16);
    plci->adjust_b_mode = ADJUST_B_MODE_RESTORE;
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: Adjust B restore...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case ADJUST_B_RESTORE_2:
    if (adjust_b_process (Id, plci, Rc) != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Adjust B restore failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
    }
    if (plci->internal_command)
      break;
    break;
  }
}


static void reset_b3_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: reset_b3_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->adjust_b_parms_msg = NULL;
    plci->adjust_b_facilities = plci->B1_facilities;
    plci->adjust_b_command = RESET_B3_COMMAND_1;
    plci->adjust_b_ncci = (word)(Id >> 16);
    plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_REMOVE_L23 |
      ADJUST_B_MODE_ASSIGN_L23 | ADJUST_B_MODE_CONNECT | ADJUST_B_MODE_RESTORE;
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: Reset B3...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case RESET_B3_COMMAND_1:
    Info = adjust_b_process (Id, plci, Rc);
    if (Info != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Reset failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      break;
    }
    if (plci->internal_command)
      return;
    break;
  }
  if ((plci->adapter->ncci_plci[plci->adjust_b_ncci] == plci->Id)
   && (plci->adapter->ncci_state[plci->adjust_b_ncci] == CONNECTED))
  {
/*    sendf (plci->appl, _RESET_B3_R | CONFIRM, Id, plci->number, "w", Info);*/
    sendf(plci->appl,_RESET_B3_I,Id,0,"s","");
  }
}


static void reset_b3_n_reset_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: reset_b3_n_reset_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->internal_command = RESET_B3_N_RESET_COMMAND_1;
    return;
  case RESET_B3_N_RESET_COMMAND_1:
    if ((Rc != OK) && (Rc != OK_FC) && (Rc != 0))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: RESET_B3 N-RESET failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      break;
    }
    if (Rc == OK)
      plci->internal_command = RESET_B3_N_RESET_COMMAND_1;
    return;
  }
}


static word select_b_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;
  byte esc_chi[3];

  dbug (1, dprintf ("[%06lx] %s,%d: select_b_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->adjust_b_parms_msg = &plci->saved_msg;
    if ((plci->tel == ADV_VOICE) && (plci == plci->adapter->AdvSignalPLCI))
      plci->adjust_b_facilities = plci->B1_facilities | B1_FACILITY_VOICE;
    else
      plci->adjust_b_facilities = plci->B1_facilities & ~B1_FACILITY_VOICE;
    plci->adjust_b_command = SELECT_B_COMMAND_1;
    plci->adjust_b_ncci = (word)(Id >> 16);
    if ((plci->saved_msg.parms[0].length < 2)
     || (READ_WORD(&(plci->saved_msg.parms[0].info[1])) == B1_NO_RESOURCE))
    {
      plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_REMOVE_L23 | ADJUST_B_MODE_SWITCH_L1 |
        ADJUST_B_MODE_NO_RESOURCE;
    }
    else
    {
      plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_REMOVE_L23 | ADJUST_B_MODE_SWITCH_L1 |
        ADJUST_B_MODE_ASSIGN_L23 | ADJUST_B_MODE_USER_CONNECT | ADJUST_B_MODE_RESTORE;
    }
    if ((plci->li_bchannel_id != 0)
     && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
     && (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
      || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
    {
      plci->adjust_b_mode |= ADJUST_B_MODE_LI_INVOLVED;
    }
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: Select B...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case SELECT_B_COMMAND_1:
    Info = adjust_b_process (Id, plci, Rc);
    if (Info != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: Select B failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      break;
    }
    if (plci->internal_command)
      return (Info);
    mixer_set_bchannel_id_esc (plci, plci->b_channel);
    if (plci->tel == ADV_VOICE)
    {
      esc_chi[0] = 0x02;
      esc_chi[1] = 0x18;
      esc_chi[2] = plci->b_channel;
      SetVoiceChannel (plci->adapter->AdvCodecPLCI, esc_chi, plci->adapter);
    }
    if ((plci->adjust_b_mode & ADJUST_B_MODE_LI_INVOLVED)
     && !(plci->li_channel_bits & LI_CHANNEL_INVOLVED))
    {
      mixer_calculate_coefs (plci->adapter);
      if (plci->li_cmd != LI_REQ_SILENT_UPDATE)
        mixer_notify_update (plci, TRUE);
    }
    break;
  }
  return (Info);
}


static void select_b_protocol_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  
  Info = select_b_command (Id, plci, Rc);
  if (plci->internal_command)
    return;
  sendf (plci->appl, _SELECT_B_REQ | CONFIRM, Id, plci->number, "w", Info);
}


static void connect_res_command (dword Id, PLCI   *plci, byte Rc)
{
  select_b_command (Id, plci, Rc);
}


static void fax_connect_ack_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_connect_ack_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case FAX_CONNECT_ACK_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_CONNECT_ACK_COMMAND_1;
      return;
    }
    plci->internal_command = FAX_CONNECT_ACK_COMMAND_2;
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_connect_info_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_CONNECT_ACK;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    return;
  case FAX_CONNECT_ACK_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX issue CONNECT ACK failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      break;
    }
  }
  if ((plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
   && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
  {
    if (plci->B3_prot == 4)
      sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
    else
      sendf(plci->appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
    plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
  }
}


static void fax_edata_ack_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_edata_ack_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case FAX_EDATA_ACK_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_EDATA_ACK_COMMAND_1;
      return;
    }
    plci->internal_command = FAX_EDATA_ACK_COMMAND_2;
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_edata_ack_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_EDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    return;
  case FAX_EDATA_ACK_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX issue EDATA ACK failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      break;
    }
  }
}


static void fax_connect_info_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_connect_info_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case FAX_CONNECT_INFO_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_CONNECT_INFO_COMMAND_1;
      return;
    }
/*
    plci->internal_command = FAX_CONNECT_INFO_COMMAND_2;
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_connect_info_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_EDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    return;
  case FAX_CONNECT_INFO_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX setting connect info failed %02x",
        (unsigned long)(UnMapId (Id)), (char far *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_CONNECT_INFO_COMMAND_2;
      return;
    }
*/
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_connect_info_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_CONNECT;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    plci->command = _CONNECT_B3_R;
     /* make sure that plci->command is not cleared on return */
    if (plci->req_in == 0)
      plci->req_in = plci->req_out = 1;
    return;
  }
  sendf (plci->appl, _CONNECT_B3_R | CONFIRM, Id, plci->number, "w", Info);
}


static void fax_disconnect_info_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_disconnect_info_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case FAX_DISCONNECT_INFO_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_DISCONNECT_INFO_COMMAND_1;
      return;
    }
/*
    plci->internal_command = FAX_DISCONNECT_INFO_COMMAND_2;
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_connect_info_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_EDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    return;
  case FAX_DISCONNECT_INFO_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX setting connect info failed %02x",
        (unsigned long)(UnMapId (Id)), (char far *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_DISCONNECT_INFO_COMMAND_2;
      return;
    }
*/
    plci->NData[0].P = plci->fax_connect_info_buffer;
    plci->NData[0].PLength = plci->fax_connect_info_length;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = plci->adapter->ncci_ch[(word)(Id >> 16)];
    plci->NL.Req = plci->nl_req = N_DISC;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    plci->command = _DISCONNECT_B3_R;
     /* make sure that plci->command is not cleared on return */
    if (plci->req_in == 0)
      plci->req_in = plci->req_out = 1;
    return;
  }
  sendf (plci->appl, _DISCONNECT_B3_R | CONFIRM, Id, plci->number, "w", Info);
}


static void fax_adjust_b23_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_adjust_b23_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->adjust_b_parms_msg = NULL;
    plci->adjust_b_facilities = plci->B1_facilities;
    plci->adjust_b_command = FAX_ADJUST_B23_COMMAND_1;
    plci->adjust_b_ncci = (word)(Id >> 16);
    plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_REMOVE_L23 |
      ADJUST_B_MODE_ASSIGN_L23 | ADJUST_B_MODE_RESTORE;
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: FAX adjust B23...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case FAX_ADJUST_B23_COMMAND_1:
    Info = adjust_b_process (Id, plci, Rc);
    if (Info != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX adjust failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      break;
    }
    if (plci->internal_command)
      return;
  case FAX_ADJUST_B23_COMMAND_2:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = FAX_ADJUST_B23_COMMAND_2;
      return;
    }
    plci->command = _CONNECT_B3_R;
    nl_req_ncci (plci, N_CONNECT, 0);
    send_req (plci);
    return;
  }
  sendf (plci->appl, _CONNECT_B3_R | CONFIRM, Id, plci->number, "w", Info);
}


static void fax_disconnect_command (dword Id, PLCI   *plci, byte Rc)
{
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: fax_disconnect_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->internal_command = FAX_DISCONNECT_COMMAND_1;
    return;
  case FAX_DISCONNECT_COMMAND_1:
  case FAX_DISCONNECT_COMMAND_2:
  case FAX_DISCONNECT_COMMAND_3:
    if ((Rc != OK) && (Rc != OK_FC) && (Rc != 0))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: FAX disconnect EDATA failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      break;
    }
    if (Rc == OK)
    {
      if ((internal_command == FAX_DISCONNECT_COMMAND_1)
       || (internal_command == FAX_DISCONNECT_COMMAND_2))
      {
        plci->internal_command = FAX_DISCONNECT_COMMAND_2;
      }
    }
    else if (Rc == 0)
    {
      if ((internal_command == FAX_DISCONNECT_COMMAND_1)
       || (internal_command == FAX_DISCONNECT_COMMAND_3))
      {
        plci->internal_command = FAX_DISCONNECT_COMMAND_3;
      }
    }
    return;
  }
}



static void rtp_connect_b3_req_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_connect_b3_req_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case RTP_CONNECT_B3_REQ_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = RTP_CONNECT_B3_REQ_COMMAND_1;
      return;
    }
    plci->internal_command = RTP_CONNECT_B3_REQ_COMMAND_2;
    nl_req_ncci (plci, N_CONNECT, 0);
    send_req (plci);
    return;
  case RTP_CONNECT_B3_REQ_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: RTP setting connect info failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci_nl_busy (plci))
    {
      plci->internal_command = RTP_CONNECT_B3_REQ_COMMAND_2;
      return;
    }
    plci->internal_command = RTP_CONNECT_B3_REQ_COMMAND_3;
    plci->NData[0].PLength = plci->internal_req_buffer[0];
    plci->NData[0].P = plci->internal_req_buffer + 1;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_UDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    break;
  case RTP_CONNECT_B3_REQ_COMMAND_3:
    return;
  }
  sendf (plci->appl, _CONNECT_B3_R | CONFIRM, Id, plci->number, "w", Info);
}


static void rtp_connect_b3_res_command (dword Id, PLCI   *plci, byte Rc)
{
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: rtp_connect_b3_res_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
  case RTP_CONNECT_B3_RES_COMMAND_1:
    if (plci_nl_busy (plci))
    {
      plci->internal_command = RTP_CONNECT_B3_RES_COMMAND_1;
      return;
    }
    plci->internal_command = RTP_CONNECT_B3_RES_COMMAND_2;
    nl_req_ncci (plci, N_CONNECT_ACK, (word)(Id >> 16));
    send_req (plci);
    return;
  case RTP_CONNECT_B3_RES_COMMAND_2:
    if ((Rc != OK) && (Rc != OK_FC))
    {
      dbug (1, dprintf ("[%06lx] %s,%d: RTP setting connect resp info failed %02x",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc));
      Info = _WRONG_STATE;
      break;
    }
    if (plci_nl_busy (plci))
    {
      plci->internal_command = RTP_CONNECT_B3_RES_COMMAND_2;
      return;
    }
    sendf (plci->appl, _CONNECT_B3_ACTIVE_I, Id, 0, "s", "");
    plci->internal_command = RTP_CONNECT_B3_RES_COMMAND_3;
    plci->NData[0].PLength = plci->internal_req_buffer[0];
    plci->NData[0].P = plci->internal_req_buffer + 1;
    plci->NL.X = plci->NData;
    plci->NL.ReqCh = 0;
    plci->NL.Req = plci->nl_req = (byte) N_UDATA;
    trcq (plci);
    plci->adapter->request (&plci->NL);
    return;
  case RTP_CONNECT_B3_RES_COMMAND_3:
    return;
  }
}



static void hold_save_command (dword Id, PLCI   *plci, byte Rc)
{
    byte SS_Ind[] = "\x05\x02\x00\x02\x00\x00"; /* Hold_Ind struct*/
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: hold_save_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    if (!plci->NL.Id)
      break;
    plci->command = 0;
    plci->adjust_b_parms_msg = NULL;
    plci->adjust_b_facilities = plci->B1_facilities;
    plci->adjust_b_command = HOLD_SAVE_COMMAND_1;
    plci->adjust_b_ncci = (word)(Id >> 16);
    plci->adjust_b_mode = ADJUST_B_MODE_SAVE | ADJUST_B_MODE_REMOVE_L23;
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: HOLD save...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case HOLD_SAVE_COMMAND_1:
    Info = adjust_b_process (Id, plci, Rc);
    if (Info != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: HOLD save failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      break;
    }
    if (plci->internal_command)
      return;
  }
  sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "ws", 3, SS_Ind);
}


static void retrieve_restore_command (dword Id, PLCI   *plci, byte Rc)
{
    byte SS_Ind[] = "\x05\x03\x00\x02\x00\x00"; /* Retrieve_Ind struct*/
  word Info;
  word internal_command;

  dbug (1, dprintf ("[%06lx] %s,%d: retrieve_restore_command %02x %04x",
    (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__, Rc, plci->internal_command));

  Info = GOOD;
  internal_command = plci->internal_command;
  plci->internal_command = 0;
  switch (internal_command)
  {
  default:
    plci->command = 0;
    plci->adjust_b_parms_msg = NULL;
    plci->adjust_b_facilities = plci->B1_facilities;
    plci->adjust_b_command = RETRIEVE_RESTORE_COMMAND_1;
    plci->adjust_b_ncci = (word)(Id >> 16);
    plci->adjust_b_mode = ADJUST_B_MODE_ASSIGN_L23 | ADJUST_B_MODE_USER_CONNECT | ADJUST_B_MODE_RESTORE;
    plci->adjust_b_state = ADJUST_B_START;
    dbug (1, dprintf ("[%06lx] %s,%d: RETRIEVE restore...",
      (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
  case RETRIEVE_RESTORE_COMMAND_1:
    Info = adjust_b_process (Id, plci, Rc);
    if (Info != GOOD)
    {
      dbug (1, dprintf ("[%06lx] %s,%d: RETRIEVE restore failed",
        (unsigned long)(UnMapId (Id)), (char   *)(FILE_), __LINE__));
      break;
    }
    if (plci->internal_command)
      return;
  }
  sendf (plci->appl, _FACILITY_I, Id & 0xffffL, 0, "ws", 3, SS_Ind);
}


static void init_b1_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: init_b1_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  plci->B1_resource = 0;
  plci->B1_facilities = 0;

  measure_clear_config (plci);

  adv_voice_clear_config (plci);

  plci->li_channel_bits = 0;
  plci->li_bchannel_id = 0;
  plci->li_error = FALSE;
  plci->li_cmd = 0;
  plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
  plci->li_plci_b_write_pos = 0;
  plci->li_plci_b_read_pos = 0;
  plci->li_plci_b_req_pos = 0;
  mixer_clear_config (plci);


  ec_clear_config (plci);


  dtmf_rec_clear_config (plci);
  dtmf_send_clear_config (plci);
  dtmf_parameter_clear_config (plci);


  audio_parameter_clear_config (plci);

  sig_tx_clear_config (plci);

  pitch_parameter_clear_config (plci);

  adjust_b_clear (plci);
}


static void clear_b1_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: clear_b1_config",
    (unsigned long)((plci->Id << 8) | UnMapController (plci->adapter->Id)),
    (char   *)(FILE_), __LINE__));

  adv_voice_clear_config (plci);
  adjust_b_clear (plci);

  ec_clear_config (plci);


  dtmf_rec_clear_config (plci);
  dtmf_send_clear_config (plci);
  dtmf_parameter_clear_config (plci);


  audio_parameter_clear_config (plci);

  sig_tx_clear_config (plci);

  pitch_parameter_clear_config (plci);


  if ((plci->li_bchannel_id != 0)
   && (plci->li_channel_bits & LI_CHANNEL_INVOLVED)
   && (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XCONNECT)
    || (plci->li_channel_bits & LI_CHANNEL_ADDRESSES_SET)))
  {
    mixer_clear_config (plci);
    plci->li_channel_bits = 0;
    plci->li_bchannel_id = 0;
    mixer_cancel_wait (plci);
    plci->li_cmd = 0;
    plci->li_notify_update = LI_NOTIFY_UPDATE_NONE;
  }


  measure_clear_config (plci);

  plci->B1_resource = 0;
  plci->B1_facilities = 0;
}


/* -----------------------------------------------------------------
                XON protocol local helpers
   ----------------------------------------------------------------- */
static void channel_flow_control_plci (PLCI   * plci, byte ch) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;
  PLCI   * other_plci;
  word i;

  if (a->ch_flow_plci[ch] != plci->Id)
  {
    if (a->ch_flow_plci[ch] != 0)
    {
      other_plci = &(a->plci[a->ch_flow_plci[ch] - 1]);
      if (other_plci->ch_flow_ring_list != 0)
      {
        i = other_plci->ch_flow_ring_list;
        while (a->ch_flow_next[i] != ch)
          i = a->ch_flow_next[i];
        if (i == ch)
          other_plci->ch_flow_ring_list = 0;
        else
        {
          if (other_plci->ch_flow_ring_list == a->ch_flow_next[i])
            other_plci->ch_flow_ring_list = i;
          a->ch_flow_next[i] = a->ch_flow_next[a->ch_flow_next[i]];
        }
      }
    }
    if (plci->ch_flow_ring_list == 0)
    {
      a->ch_flow_next[ch] = ch;
      plci->ch_flow_ring_list = ch;
    }
    else
    {
      a->ch_flow_next[ch] = a->ch_flow_next[plci->ch_flow_ring_list];
      a->ch_flow_next[plci->ch_flow_ring_list] = ch;
    }
    a->ch_flow_plci[ch] = plci->Id;
  }
}

static void channel_flow_control_remove (PLCI   * plci) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;
  word i;

  if (plci->ch_flow_ring_list != 0)
  {
    i = plci->ch_flow_ring_list;
    do
    {
      i = a->ch_flow_next[i];
      a->ch_flow_plci[i] = 0;
      a->ch_flow_control[i] = 0;
    } while (i != plci->ch_flow_ring_list);
    plci->ch_flow_ring_list = 0;
  }
}

static void channel_x_on (PLCI   * plci, byte ch) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;
  if (a->ch_flow_control[ch] & N_XON_SENT) {
    a->ch_flow_control[ch] &= ~N_XON_SENT;
  }
}

static void channel_x_off (PLCI   * plci, byte ch, byte flag) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;

  if ((a->ch_flow_control[ch] & N_RX_FLOW_CONTROL_MASK) == 0) {
    a->ch_flow_control[ch] |= (N_CH_XOFF | flag);
    channel_flow_control_plci (plci, ch);
    a->ch_flow_control_pending++;
  }
}

static void channel_request_xon (PLCI   * plci, byte ch) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;

  if (a->ch_flow_control[ch] & N_CH_XOFF) {
    a->ch_flow_control[ch] |= N_XON_REQ;
    a->ch_flow_control[ch] &= ~N_CH_XOFF;
    a->ch_flow_control[ch] &= ~N_XON_CONNECT_IND;
  }
}

static void channel_xmit_extended_xon (PLCI   * plci) {
  DIVA_CAPI_ADAPTER   * a;
  word i, one_requested = 0;

  if ((!plci) || (!plci->Id) || ((a = plci->adapter) == 0)) {
    return;
  }

  if (plci->ch_flow_ring_list != 0)
  {
    i = plci->ch_flow_ring_list;
    do
    {
      i = a->ch_flow_next[i];
      if ((a->ch_flow_control[i] & N_CH_XOFF) &&
          (a->ch_flow_control[i] & N_XON_CONNECT_IND)) {
        channel_request_xon (plci, (byte)i);
        one_requested = 1;
      }
    } while (i != plci->ch_flow_ring_list);

    if (one_requested) {
      channel_xmit_xon (plci);
    }
  }
}

/*
  Try to xmit next X_ON
  */
static int find_channel_with_pending_x_on (DIVA_CAPI_ADAPTER   * a, PLCI   * plci) {
  word i;

  if (!(plci->adapter->manufacturer_features & MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)) {
    return (0);
  }
  if (plci->ch_flow_ring_list != 0)
  {
    i = plci->ch_flow_ring_list;
    do
    {
      i = a->ch_flow_next[i];
      if (a->ch_flow_control[i] & N_XON_REQ)
      {
        plci->ch_flow_ring_list = i;
        return (i);
      }
    } while (i != plci->ch_flow_ring_list);
  }
  return (0);
}

static void channel_xmit_xon (PLCI   * plci) {
  DIVA_CAPI_ADAPTER   * a = plci->adapter;
  byte ch;

  if (plci->nl_req || !plci->NL.Id || plci->nl_remove_id) {
    return;
  }
  if ((ch = (byte)find_channel_with_pending_x_on (a, plci)) == 0) {
    return;
  }
  a->ch_flow_control[ch] &= ~N_XON_REQ;
  a->ch_flow_control[ch] |= N_XON_SENT;

  plci->NL.Req = plci->nl_req = (byte)N_XON;
  plci->NL.ReqCh         = ch;
  plci->NL.X             = plci->NData;
  plci->NL.XNum          = 1;
  plci->NData[0].P       = &plci->RBuffer[0];
  plci->NData[0].PLength = 0;

  trcq (plci);
  plci->adapter->request(&plci->NL);
}

static int channel_can_xon (PLCI   * plci, byte ch) {
  APPL   * APPLptr;
  DIVA_CAPI_ADAPTER   * a;
  word NCCIcode;
  dword count;
  word Num;
  word i;

  APPLptr = plci->appl;
  a = plci->adapter;

  if (!APPLptr)
    return (0);

  NCCIcode = a->ch_ncci[ch] | (((word) a->Id) << 8);

                /* count all buffers within the Application pool    */
                /* belonging to the same NCCI. XON if a first is    */
                /* used.                                            */
  count = 0;
  Num = 0xffff;
  for(i=0; i<APPLptr->MaxBuffer; i++) {
    if(NCCIcode==APPLptr->DataNCCI[i]) count++;
    if(!APPLptr->DataNCCI[i] && Num==0xffff) Num = i;
  }
  if ((count > 2) || (Num == 0xffff)) {
    return (0);
  }
  return (1);
}


/*------------------------------------------------------------------*/

/* to be completed */
void disable_adapter(byte adapter_number)
{
  word j, ncci;
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *plci;
  dword Id;

  if ((adapter_number == 0) || (adapter_number > max_adapter) || !adapter[adapter_number-1].request)
  {
    dbug(1,dprintf("disable adapter: number %d/%d invalid",adapter_number,max_adapter));
    return;
  }
  dbug(1,dprintf("disable adapter number %d",adapter_number));
    /* Capi20 starts with Nr. 1, internal field starts with 0 */
  a = &adapter[adapter_number-1];
  a->adapter_disabled = TRUE;
  for(j=0;j<a->max_plci;j++)
  {
    if(a->plci[j].Id) /* disconnect logical links */
    {
      plci = &a->plci[j];
      if(plci->channels)
      {
        for(ncci=1;ncci<MAX_NCCI+1 && plci->channels;ncci++)
        {
          if(a->ncci_plci[ncci]==plci->Id)
          {
            Id = (((dword)ncci)<<16)|((word)plci->Id<<8)|a->Id;
            sendf(plci->appl,_DISCONNECT_B3_I,Id,0,"ws",0,"");
            if(plci->channels)
              plci->channels--;
          }
        }
      }

      if(plci->State!=LISTENING) /* disconnect physical links */
      {
        Id = ((word)plci->Id<<8)|a->Id;
        sendf(plci->appl, _DISCONNECT_I, Id, 0, "w", _L1_ERROR);
        plci_remove(plci);
        plci->Sig.Id = 0;
        plci->NL.Id = 0;
        plci_remove(plci);
      }
    }
  }

  if (li_update_wait_list != NULL)
    mixer_update_done ();

}

void enable_adapter(byte adapter_number)
{
  DIVA_CAPI_ADAPTER   *a;

  if ((adapter_number == 0) || (adapter_number > max_adapter) || !adapter[adapter_number-1].request)
  {
    dbug(1,dprintf("enable adapter: number %d invalid",adapter_number));
    return;
  }
  dbug(1,dprintf("enable adapter number %d",adapter_number));
    /* Capi20 starts with Nr. 1, internal field starts with 0 */
  a = &adapter[adapter_number-1];
  a->adapter_disabled = FALSE;
  listen_check(a);
}


static word CPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   * a,word appl_index)
{
  return 1;
}



/**********************************************************************************/
/* function groups the listening applications according to the CIP mask and the   */
/* Info_Mask. Each group gets just one Connect_Ind. Some application manufacturer */
/* are not multi-instance capable, so they start e.g. 30 applications what causes */
/* big problems on application level (one call, 30 Connect_Ind, ect). The         */
/* function must be enabled by setting "a->group_optimization_enabled" from the   */
/* OS specific part (per adapter).                                                */
/**********************************************************************************/

void group_optimization(PLCI   * plci)
{
  DIVA_CAPI_ADAPTER   *a;
  PLCI   *tplci;
  APPL   *appl;
    word appl_busy_plcis[MAX_APPL];
    dword next_appl_mask_table[APPL_MASK_DWORDS];
  word app, i, j, least_busy_app, least_busy_plcis;

  a = plci->adapter;
  for (i = 0; i < max_appl; i++)
    appl_busy_plcis[i] = 0;
  for (i = 0; i < a->max_plci; i++)
  {
    tplci = &(a->plci[i]);
    if (tplci != plci)
    {
      if ((tplci->State==INC_CON_PENDING) || (tplci->State==INC_CON_ALERT))
      {
        j = 0;
        while (j < max_appl)
        {
          if (tplci->c_ind_mask_table[j >> 5] == 0)
            j += 32;
          else
          {
            do
            {
              if (tplci->c_ind_mask_table[j >> 5] & (1L << (j & 0x1f)))
                (appl_busy_plcis[j])++;
              j++;
            } while ((j < max_appl) && ((j & 0x1f) != 0));
          }
        }
      }
      else if ((tplci->State != IDLE) && (tplci->appl != NULL))
      {
        j = (word)(tplci->appl - application);
        (appl_busy_plcis[j])++;
      }
    }
  }
  for (j = 0; j < APPL_MASK_DWORDS; j++)
    next_appl_mask_table[j] = plci->c_ind_mask_table[j];
  for (app = 0; app < max_appl; app++)
  {
    if (test_appl_mask_bit (next_appl_mask_table, app))
    {
      appl = &(application[app]);
      if ((appl->MaxNCCI == 1)
       || ((a->group_optimization_enabled & GROUPOPT_ENABLE_MASK) != GROUPOPT_ONLY_ON_SINGLE_CONN))
      {
        i = 0;
        least_busy_app = app;
        least_busy_plcis = appl_busy_plcis[app];
        for (j = app; j < max_appl; j++)
        {
          if (test_appl_mask_bit (next_appl_mask_table, j)
           && (a->CIP_Mask[j] == a->CIP_Mask[app])
           && (a->Info_Mask[j] == a->Info_Mask[app])
           && (application[j].MaxNCCI == appl->MaxNCCI)
           && (application[j].MaxNCCIData == appl->MaxNCCIData)
           && (application[j].MaxDataLength == appl->MaxDataLength))
          {
            i++;
            if (appl_busy_plcis[j] < least_busy_plcis)
            {
              least_busy_app = j;
              least_busy_plcis = appl_busy_plcis[j];
            }
          }
        }
        for (j = app; j < max_appl; j++)
        {
          if (test_appl_mask_bit (next_appl_mask_table, j)
           && (a->CIP_Mask[j] == a->CIP_Mask[app])
           && (a->Info_Mask[j] == a->Info_Mask[app])
           && (application[j].MaxNCCI == appl->MaxNCCI)
           && (application[j].MaxNCCIData == appl->MaxNCCIData)
           && (application[j].MaxDataLength == appl->MaxDataLength))
          {
            clear_appl_mask_bit (next_appl_mask_table, j);
            if (((i > 1)
              || (a->group_optimization_enabled & GROUPOPT_ALSO_ON_INDIVIDUALS))
             && ((j != least_busy_app)
              || (least_busy_plcis >= appl->MaxNCCI)
              || ((least_busy_plcis != 0)
               && ((a->group_optimization_enabled & GROUPOPT_ENABLE_MASK) == GROUPOPT_SINGLE_CONN_TO_MULTI))))
            {
              clear_c_ind_mask_bit (plci, j);
            }
          }
        }
      }
    }
  }
}


/* OS notifies the driver about a application Capi_Register */
word CapiRegister(word id)
{
  word i;

  for (i=0; i<max_adapter; i++)                   /* scan all adapters...    */
    diva_capi_check_l1_activation (id, &adapter[i]);
  return FALSE;
}

/*------------------------------------------------------------------*/
static void diva_capi_check_l1_activation (word id, DIVA_CAPI_ADAPTER   *a)
{
  word j, non_gdc_appls_found;
  PLCI *plci;

  if (a->request)
  {
    if (a->flag_dynamic_l1_down  /* remove adapter from L1 tristate (Huntgroup) */
     && !(a->requested_options_table[id-1] & (1L << PRIVATE_MADAPTER)))
    {
      non_gdc_appls_found = 0;
      for (j=0; j<max_appl; j++)
      {
        if (application[j].Id
         && !(a->requested_options_table[application[j].Id-1] & (1L << PRIVATE_MADAPTER)))
        {
          non_gdc_appls_found++;
        }
      }
      if (non_gdc_appls_found == 1)           /* first application does a capi register   */
      {
        if ((j=get_plci(a)))                    /* activate L1 of all adapters */
        {
          dbug(1,dprintf("[%06lx] DynamicL1Up %d",
            (unsigned long)(UnMapController (a->Id)), id));
          plci = &a->plci[j-1];
          plci->command = 0;
          add_p(plci,OAD,"\x01\xfd");
          add_p(plci,CAI,"\x01\x80");
          add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
          add_p(plci,SHIFT|0x08|6,0); /* non-locking SHIFT, codeset 6 */
          add_p(plci,SIN,"\x02\x00\x00");
          plci->internal_command = START_L1_SIG_ASSIGN_PEND;
          sig_req(plci,ASSIGN,DSIG_ID);
          add_p(plci,FTY,"\x02\xff\x07"); /* l1 start */
          sig_req(plci,SIG_CTRL,0);
          send_req(plci);
        }
      }
    }
  }
}

static void diva_capi_check_l1_shutdown (word id, DIVA_CAPI_ADAPTER   *a)
{
  word j, non_gdc_appls_found;
  PLCI *plci;

  if (a->request)
  {
    if (a->flag_dynamic_l1_down  /* remove adapter from L1 tristate (Huntgroup) */
     && !(a->requested_options_table[id-1] & (1L << PRIVATE_MADAPTER)))
    {
      non_gdc_appls_found = 0;
      for (j=0; j<max_appl; j++)
      {
        if (application[j].Id
         && !(a->requested_options_table[application[j].Id-1] & (1L << PRIVATE_MADAPTER)))
        {
          non_gdc_appls_found++;
        }
      }
      if (non_gdc_appls_found == 1)           /* last application does a capi release   */
      {
        if((j=get_plci(a)))                    /* deactivate L1 of all adapters */
        {
          dbug(1,dprintf("[%06lx] DynamicL1Down %d",
            (unsigned long)(UnMapController (a->Id)), id));
          plci = &a->plci[j-1];
          plci->command = 0;
          add_p(plci,OAD,"\x01\xfd");
          add_p(plci,CAI,"\x01\x80");
          add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
          add_p(plci,SHIFT|0x08|6,0); /* non-locking SHIFT, codeset 6 */
          add_p(plci,SIN,"\x02\x00\x00");
          plci->internal_command = REM_L1_SIG_ASSIGN_PEND;
          sig_req(plci,ASSIGN,DSIG_ID);
          add_p(plci,FTY,"\x02\xff\x06"); /* l1 down */
          sig_req(plci,SIG_CTRL,0);
          send_req(plci);
        }
      }
    }
  }
}

/*------------------------------------------------------------------*/

/* Functions for virtual Switching e.g. Transfer by join, Conference */

void VSwitchReqInd(PLCI   *plci, dword Id, byte   **parms)
{
  word i;
  /* Format of vswitch_t:
  0 byte length
  1 byte VSWITCHIE
  2 byte VSWITCH_REQ/VSWITCH_IND
  3 byte reserved
  4 word VSwitchcommand
  6 word returnerror
  8... Params
  */
  if(!plci ||
    !plci->appl ||
    !plci->State ||
        !plci->relatedPTYPLCI ||
        !plci->relatedPTYPLCI->appl ||
        !plci->relatedPTYPLCI->State ||
    plci->Sig.Ind==NCR_FACILITY ||
    plci->relatedPTYPLCI->relatedPTYPLCI!=plci
    )
    return;

  for(i=0;i<MAX_MULTI_IE;i++)
  {
        if(!parms[i][0]) continue;
    if(parms[i][0]<7)
    {
      parms[i][0]=0; /* kill it */
      continue;
    }
    dbug(1,dprintf("VSwitchReqInd(%d)",parms[i][4]));
    switch(parms[i][4])
    {
    case VSJOIN:
      if(plci->ptyState!=S_ECT && plci->relatedPTYPLCI->ptyState!=S_ECT)
      { /* Error */
        break;
      }
      /* remember all necessary informations */
      if(parms[i][0]<11 || parms[i][8]<3) /* Length Test */
      {
        break;
      }
      if(parms[i][2]==VSWITCH_IND && parms[i][9]==1)
      {   /* first indication after ECT-Request on Consultation Call */
        plci->vswitchstate=parms[i][9];
        parms[i][9]=2; /* State */
        /* now ask first Call to join */
      }
      else if(parms[i][2]==VSWITCH_REQ && parms[i][9]==3)
      { /* Answer of VSWITCH_REQ from first Call */
        plci->vswitchstate=parms[i][9];
        /* tell consultation call to join
        and the protocol capabilities of the first call */
      }
      else
      { /* Error */
        break;
      }
      plci->vsprot=parms[i][10]; /* protocol */
      plci->vsprotdialect=parms[i][11]; /* protocoldialect */
      /* send join request to related PLCI */
      parms[i][1]=VSWITCHIE;
      parms[i][2]=VSWITCH_REQ;

      plci->relatedPTYPLCI->command = 0;
      plci->relatedPTYPLCI->internal_command = VSWITCH_REQ_PEND;
      add_p(plci->relatedPTYPLCI,ESC,&parms[i][0]);
      sig_req(plci->relatedPTYPLCI,VSWITCH_REQ,0);
      send_req(plci->relatedPTYPLCI);
      break;
    case VSTRANSPORT:
    default:
      if(plci->vswitchstate==3 &&
        plci->relatedPTYPLCI->vswitchstate==3)
      {
        add_p(plci->relatedPTYPLCI,ESC,&parms[i][0]);
        sig_req(plci->relatedPTYPLCI,VSWITCH_REQ,0);
        send_req(plci->relatedPTYPLCI);
      }
      break;
    }
    parms[i][0]=0; /* kill it */
  }
}


/*------------------------------------------------------------------*/

static int diva_get_dma_descriptor (PLCI   *plci, dword   *dma_magic) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

  if (!(diva_xdi_extended_features & DIVA_CAPI_XDI_PROVIDES_RX_DMA)) {
    return (-1);
  }

  pReq->xdi_dma_descriptor_operation.Req = 0;
  pReq->xdi_dma_descriptor_operation.Rc = IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

  pReq->xdi_dma_descriptor_operation.info.operation =     IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC;
  pReq->xdi_dma_descriptor_operation.info.descriptor_number  = -1;
  pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
  pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  e.user[0] = plci->adapter->Id - 1;
  plci->adapter->request((ENTITY*)pReq);

  if (!pReq->xdi_dma_descriptor_operation.info.operation &&
      (pReq->xdi_dma_descriptor_operation.info.descriptor_number >= 0) &&
      pReq->xdi_dma_descriptor_operation.info.descriptor_magic) {
    *dma_magic = pReq->xdi_dma_descriptor_operation.info.descriptor_magic;
    dbug(3,dprintf("dma_alloc, a:%d (%d-%08lx)",
         plci->adapter->Id,
         pReq->xdi_dma_descriptor_operation.info.descriptor_number,
         (unsigned long)(*dma_magic)));
    return (pReq->xdi_dma_descriptor_operation.info.descriptor_number);
  } else {
    dbug(1,dprintf("dma_alloc not possible"));
    return (-1);
  }
}

static void diva_free_dma_descriptor (PLCI   *plci, int nr) {
  ENTITY e;
  IDI_SYNC_REQ* pReq = (IDI_SYNC_REQ*)&e;

  if (nr < 0) {
    return;
  }

  pReq->xdi_dma_descriptor_operation.Req = 0;
  pReq->xdi_dma_descriptor_operation.Rc = IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION;

  pReq->xdi_dma_descriptor_operation.info.operation =                                                IDI_SYNC_REQ_DMA_DESCRIPTOR_FREE;
  pReq->xdi_dma_descriptor_operation.info.descriptor_number  = nr;
  pReq->xdi_dma_descriptor_operation.info.descriptor_address = 0;
  pReq->xdi_dma_descriptor_operation.info.descriptor_magic   = 0;

  e.user[0] = plci->adapter->Id - 1;
  plci->adapter->request((ENTITY*)pReq);

  if (!pReq->xdi_dma_descriptor_operation.info.operation) {
    dbug(1,dprintf("dma_free(%d)", nr));
  } else {
    dbug(1,dprintf("dma_free failed (%d)", nr));
  }
}



static void reject_call(dword Id, PLCI   * plci, APPL   * appl, word Reject, API_PARSE * ai_parms)
{
  static byte esc_t[] = {0x03,0x08,0x00,0x00};
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0xd8,0x9b};

  if(Reject)
  {
    if(c_ind_mask_empty (plci))
    {
      if((Reject&0xff00)==0x3400)
      {
        esc_t[2] = ((byte)(Reject&0x00ff)) | 0x80;
        add_p(plci,ESC,esc_t);
        add_ai(plci, ai_parms);
        sig_req(plci,REJECT,0);
      }
      else if(Reject==1 || Reject>8)
      {
        if(plci->lastrejectcause)
        {
          if((plci->lastrejectcause&0xff00)==0x3400)
          {
            esc_t[2] = ((byte)(Reject&0x00ff)) | 0x80;
            add_p(plci,ESC,esc_t);
          }
          else
          {
            esc_t[2] = cau_t[(plci->lastrejectcause&0x000f)];
            add_p(plci,ESC,esc_t);
          }
          add_ai(plci, ai_parms);
          sig_req(plci,REJECT,0);
        }
        else
        {
          add_ai(plci, ai_parms);
          sig_req(plci,HANGUP,0);
        }
      }
      else
      {
        esc_t[2] = cau_t[(Reject&0x000f)];
        add_p(plci,ESC,esc_t);
        add_ai(plci, ai_parms);
        sig_req(plci,REJECT,0);
      }
      plci->appl = appl;
    }
    else
    {
      if(((Reject&0xff00)==0x3400) || (Reject!=1 && Reject<9))
      {
        plci->lastrejectcause=Reject;
      }
      sendf(appl, _DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
    }
  }
}



static word diva_capi_create_mngt_read_req (byte* P, const char* path) {
  byte var_length;
  byte* plen;

  if ((var_length = (byte)strlen (path)) >= 128) {
    return (0);
  }

  *P++ = ESC;
  plen = P++;
  *P++ = 0x80; /* MAN_IE */
  *P++ = 0x00; /* Type */
  *P++ = 0x00; /* Attribute */
  *P++ = 0x00; /* Status */
  *P++ = 0x00; /* Variable Length */
  *P++ = var_length;
  memcpy (P, path, var_length);
  P += var_length;
  *plen = var_length + 0x06;
  *P++ = 0; /* terminate */

  return ((word)(var_length + 0x09));
}

static void diva_capi_send_mngt_req (diva_capi_mngt_ctxt_t* pE, byte Req, word length, int new_state) {
  ENTITY* e = &pE->e;

  e->XNum        = 1;
  e->X           = &pE->XData;
  e->Req         = Req;
  e->X->PLength  = length;
  e->X->P        = pE->xbuffer;
  pE->state      = new_state;

  (*(pE->d->request))(e);
}

static diva_man_var_header_t* get_next_var (diva_man_var_header_t* pVar) {
  byte* msg   = (byte*)pVar;
  byte* start;
  int msg_length;

  if (*msg != ESC) return (0);

  start = msg + 2;
  msg_length = *(msg+1);
  msg = (start+msg_length);

  if (*msg != ESC) return (0);

  return ((diva_man_var_header_t*)msg);
}

static diva_man_var_header_t* find_var (diva_man_var_header_t* pVar,
                                        const char* name) {
  const char* path;
  int i;

  do {
    path = (char*)&pVar->path_length+1;

    for (i = 0; (name[i] && (i < pVar->path_length) && (name[i] == path[i])); i++);
    if ((i >= pVar->path_length) && (name[i] == 0)) {
      return (pVar);
    }

  } while ((pVar = get_next_var (pVar)));

  return (pVar);
}

static void diva_capi_read_var (const diva_man_var_header_t* pVar, void* dst, int size) {
  if (pVar && dst && size) {
    byte* ptr     = (byte*)&pVar->path_length;
    int   use_int = 0;
    int   v_i     = 0;
    dword v_u     = 0;

    ptr += (pVar->path_length + 1);

    switch (pVar->type) {
      case 0x81: /* MI_INT    - signed integer */
        use_int = 1;
        switch (pVar->value_length) {
          case 1:
            v_i = *(char*)ptr;
            break;
          case 2:
            v_i = (int)READ_WORD(ptr);
            break;
          case 4:
            v_i = (int)READ_DWORD(ptr);
            break;
        }
        break;

      case 0x82: /* MI_UINT   - unsigned integer */
      case 0x83: /* MI_HINT   - unsigned integer, hex representetion */
      case 0x87: /* MI_BITFLD - unsigned integer, bit representation */
        switch (pVar->value_length) {
          case 1:
            v_u = *(byte*)ptr;
            break;
          case 2:
            v_u = READ_WORD(ptr);
            break;
          case 4:
            v_u = READ_DWORD(ptr);
            break;
        }
        break;

      case 0x85: /* MI_BOOLEAN */
        switch (pVar->value_length) {
          case 1:
            v_u = (*(byte*)ptr  != 0);
            break;
          case 2:
            v_u = READ_WORD(ptr) != 0;
            break;
          case 4:
            v_u = READ_DWORD(ptr) != 0;
            break;
        }
        break;

      default:
        DBG_FTL(("Variable type %02x not supported", pVar->type))
        break;
    }

    if (use_int) {
      switch(size) {
        case 1:
          *(char*)dst  = (char)v_i;
          break;
        case 2:
          *(short*)dst = (short)v_i;
          break;
        case 4:
          *(int*)dst   = (int)v_i;
          break;
      }
    } else {
      switch(size) {
        case 1:
          *(byte*)dst   = (byte)v_u;
          break;
        case 2:
          *(word*)dst   = (word)v_u;
          break;
        case 4:
          *(dword*)dst  = (dword)v_u;
          break;
      }
    }
  }
}

static void diva_capi_process_mngt_indication (diva_capi_mngt_ctxt_t* pE) {
  if (!--pE->pending_msg) {

    if (pE->xbuffer[0]) {
      int current_work_entry;
      for (current_work_entry = 0;
           pE->work_entries[pE->current_entry]->work_items[current_work_entry].name;
           current_work_entry++) {
        char* name = pE->work_entries[pE->current_entry]->work_items[current_work_entry].name;
        void* dst  = pE->work_entries[pE->current_entry]->work_items[current_work_entry].dst;
        int   size = pE->work_entries[pE->current_entry]->work_items[current_work_entry].size;

        diva_capi_read_var (find_var ((diva_man_var_header_t*)&pE->xbuffer[0], name), dst, size);
      }
    }

    if (pE->work_entries[++pE->current_entry]) {
      word length;

      pE->pending_msg = 2;
      DBG_LOG(("Read: '%s' directory", pE->work_entries[pE->current_entry]->directory_name))
      length = diva_capi_create_mngt_read_req (pE->xbuffer,
                                               pE->work_entries[pE->current_entry]->directory_name);
      diva_capi_send_mngt_req (pE, MAN_READ, length, pE->state + 1);
    } else {
      pE->xbuffer[0] = 0;
      diva_capi_send_mngt_req (pE, REMOVE, 1, -1);
    }
  }
}

static void diva_capi_mngt_callback (ENTITY* e) {
  diva_capi_mngt_ctxt_t* pE = (diva_capi_mngt_ctxt_t*)e;

  if (e->complete == 255) { /* Return code */
    switch (pE->state) {
      case 1: /* ASSIGN pending */
        if (e->Rc == ASSIGN_OK) {
          word length;
          DBG_LOG(("Read: '%s' directory", pE->work_entries[pE->current_entry]->directory_name))
          pE->pending_msg        = 2;
          length = diva_capi_create_mngt_read_req (pE->xbuffer,
                                                   pE->work_entries[pE->current_entry]->directory_name);
          diva_capi_send_mngt_req (pE, MAN_READ, length, 2);
        } else {
          DBG_FTL(("Management interface ASSIGN failed"))
          pE->state = 0;
        }
        break;

      case -1: /* REMOVE pending */
        if (e->Rc != 0xff) {
          DBG_FTL(("Management interface REMOVE failed (%02x)", e->Rc))
        }
        pE->state = 0;
        break;

      default: /* Assigned, information retrival state */
        if (e->Rc != 0xff) {
          DBG_ERR(("failed to read: '%s' directory (%02x)",
                  pE->work_entries[pE->current_entry]->directory_name, e->Rc))
          pE->pending_msg = 1;
          pE->xbuffer[0]  = 0;
        }
        diva_capi_process_mngt_indication (pE);
        break;
    }
  } else { /* Indication */
    if (pE->state > 1) { /* Assigned, information retrival state */
      if (e->complete != 2) { /* start copy indication */
        e->RNum       = 1;
        e->R          = &pE->RData;
        e->R->P       = &pE->xbuffer[0];
        e->R->PLength = sizeof(pE->xbuffer) - 1;
      } else {
        pE->xbuffer[e->R->PLength] = 0;
        diva_capi_process_mngt_indication (pE);
      }
    } else {
      e->RNum = 0;
      e->RNR  = 2;
    }

    e->Ind = 0;
  }
}

/*
  Read real channel cound from the adapter management interface
  Retrieve information about XDI adapter type from management interface
  of MTPX adapter if possible.

  INPUT:
         d    - IDI adapter descriptor (XDI adapter or MTPX adapter)
  OUTPUT:
         info - information about XDI adapter
  RETURN:
         -1 on error or channel count as read from adapter management interface
  */
int diva_capi_read_channel_count (DESCRIPTOR* d, diva_mgnt_adapter_info_t* info) {
  int channels = -1;

  if (d && d->request && info && (d->channels || (d->type == IDI_MADAPTER))) {
    diva_capi_mngt_ctxt_t* pE;

    if ((pE = diva_os_malloc (0, sizeof(*pE)))) {
      ENTITY* e = &pE->e;
      int count = 500;
      int prev_state;
      diva_capi_mngt_work_item_t ch_items[] = { { "Info\\Channels", &channels, sizeof(channels) },
                                                { "Info\\VsCAPI",   &info->vscapi_mode, sizeof(info->vscapi_mode) },
                                                { 0, 0, 0 } };
      diva_capi_mngt_work_item_t info_items[] = {
           { "Status\\XDI\\A1\\Card Type",   &info->cardtype,          sizeof(info->cardtype)          },
           { "Status\\XDI\\A1\\Controller",  &info->controller,        sizeof(info->controller)        },
           { "Status\\XDI\\A1\\Controllers", &info->total_controllers, sizeof(info->total_controllers) },
           { "Status\\XDI\\A1\\Serial",      &info->serial_number,     sizeof(info->serial_number)     },
           { "Status\\XDI\\A1\\Logical",     &info->logical_adapter_number, sizeof(info->logical_adapter_number) },
           { 0, 0, 0 } };
      diva_capi_mngt_work_item_t status_items[] = { { "Status\\XDI\\Count", &info->count, sizeof(info->count)  },
                                                { 0, 0, 0 } };

      diva_capi_mngt_work_entry_t info_entry = { "Info",            ch_items   };
      diva_capi_mngt_work_entry_t xdi_entry  = { "Status\\XDI\\A1", info_items };
      diva_capi_mngt_work_entry_t status_entry = { "Status\\XDI",   status_items };
      diva_capi_mngt_work_entry_t* entries[] = { &info_entry, &xdi_entry, &status_entry, 0 };

      info->cardtype          = 0;
      info->serial_number     = 0;
      info->controller        = 0;
      info->total_controllers = 0;
      info->count             = 0;
      info->logical_adapter_number = 0;
      info->vscapi_mode       = 0;
      info->mtpx_cfg_number_of_configuration_entries = 0;
      info->mtpx_cfg = 0;

      memset (pE, 0x00, sizeof(*pE));
      pE->d = d;
      pE->work_entries = entries;

      e->Id          = MAN_ID;
      e->callback    = diva_capi_mngt_callback;
      diva_capi_send_mngt_req (pE, ASSIGN, 1, 1);
      prev_state = pE->state;
      diva_os_sleep(10);
      while (pE->state && --count) {
        diva_os_sleep(10);
        if (pE->state != prev_state) {
          prev_state = pE->state;
          count = 500;
        }
      }

      if (pE->state) {
        DBG_FTL(("Management entity removal failed"))
      } else {

        if (d->type == IDI_MADAPTER) {
          IDI_SYNC_REQ* sync_req = (IDI_SYNC_REQ*)diva_os_malloc (0, sizeof(*sync_req));

          if (sync_req != 0) {
            memset (&sync_req->diva_get_mtpx_adapter_xdi_config,
                    0x00,
                    sizeof(sync_req->diva_get_mtpx_adapter_xdi_config));
            sync_req->diva_get_mtpx_adapter_xdi_config.Rc = IDI_SYNC_REQ_GET_MTPX_ADAPTER_XDI_CONFIG;
            (*(d->request))((ENTITY*)sync_req);

            if (sync_req->diva_get_mtpx_adapter_xdi_config.Rc == 0xff) {
              if (sync_req->diva_get_mtpx_adapter_xdi_config.info.number_of_configuration_entries != 0 &&
                  sync_req->diva_get_mtpx_adapter_xdi_config.info.cfg != 0) {
                dword nr_entries =
                  sync_req->diva_get_mtpx_adapter_xdi_config.info.number_of_configuration_entries;
         diva_mtpx_adapter_config_entry_t* cfg;
                dword entry_size = MIN((dword)sizeof(*cfg),
                                     sync_req->diva_get_mtpx_adapter_xdi_config.info.configuration_entry_size);

                if ((cfg = (diva_mtpx_adapter_config_entry_t*)diva_os_malloc (0,
                                               MAX((sizeof(*cfg)*nr_entries), (2*sizeof(void*))))) != 0) {
                  dword i;

                  memset (cfg, 0x00, sizeof(*cfg)*nr_entries);

                  for (i = 0; i < nr_entries; i++) {
                    memcpy (&cfg[i], &sync_req->diva_get_mtpx_adapter_xdi_config.info.cfg[i], entry_size);
                    cfg[i].serial_number &= ~0xff000000;
                  }

                  info->mtpx_cfg_number_of_configuration_entries = nr_entries;
                  info->mtpx_cfg = cfg;
                }
              }

              if (sync_req->diva_get_mtpx_adapter_xdi_config.info.cfg != 0) {
                (*(sync_req->diva_get_mtpx_adapter_xdi_config.info.release_configuration_proc))(
                                                   sync_req->diva_get_mtpx_adapter_xdi_config.info.cfg);
              }
            }
            diva_os_free (0, sync_req);
          }
        }

        DBG_LOG(("channels:    %d",  channels))
        DBG_LOG(("cardtype:    %ld", (long)(info->cardtype)))
        DBG_LOG(("serial:      %ld", (long)(info->serial_number)))
        DBG_LOG(("controller:  %ld", (long)(info->controller)))
        DBG_LOG(("controllers: %ld", (long)(info->total_controllers)))
        DBG_LOG(("count:       %ld", (long)(info->count)))
        DBG_LOG(("adapter:     %ld", (long)(info->logical_adapter_number)))

        if (info->mtpx_cfg != 0) {
          const char* name[] ={ "PRI", "BRI", "Analog", "SoftIP", "Unknown" };
          dword i;

          DBG_LOG(("xdi adapter%s:", (info->mtpx_cfg_number_of_configuration_entries > 1) ? "s" : ""))

          for (i = 0; i < info->mtpx_cfg_number_of_configuration_entries; i++) {
            DBG_LOG((" Nr:%d Id:%08lx Channels:%d SN:%d %s %d of %d",
                    info->mtpx_cfg[i].logical_adapter_number,
                    (unsigned long)(info->mtpx_cfg[i].card_id),
                    info->mtpx_cfg[i].channels,
                    info->mtpx_cfg[i].serial_number,
                    name[MIN(info->mtpx_cfg[i].card_type, sizeof(name)/sizeof(name[0])-1)],
                    info->mtpx_cfg[i].ifc, info->mtpx_cfg[i].interfaces))
          }
        }
      }

      diva_os_free (0, pE);
    }
  }

  return (channels);
}




static void diva_capi_init_stream_data (PLCI* plci, diva_saved_capi_appl_action_hdr_t* acion_hdr)
{
  if (plci != 0) {
    diva_saved_capi_appl_stream_action_t* action = (diva_saved_capi_appl_stream_action_t*)acion_hdr;

    memcpy (plci->stream_data, action->stream_data, action->length);
  }
}

static void diva_capi_apply_saved_action (APPL* appl, PLCI* plci, word messageNr)
{
  diva_entity_link_t* link = diva_q_get_head (&appl->saved_action_q);

  while (link != 0) {
    diva_saved_capi_appl_action_hdr_t* actionHdr = (diva_saved_capi_appl_action_hdr_t*)link;

    if (actionHdr->messageNr == messageNr) {
      diva_q_remove (&appl->saved_action_q, link);
      actionHdr->action (plci, actionHdr);
      diva_os_free (DIVA_OS_MALLOC_ATOMIC, actionHdr);
      break;
    }
    link = diva_q_get_next (link);
  }
}

/*
 Add application id to stream description
  KEY 8 .. .. .. .. .. .. .. ..
 */
dword diva_capi_add_streaming_application_id (byte* dst, dword max_length, dword Id) {
 byte key_t[] = { KEY, 8, (byte)Id, (byte)(Id >> 8), (byte)(Id >> 16), (byte)(Id >> 24), 0, 0, 0, 0, /* End of list */ 0 };
 byte *stream_t = &dst[1], *start = &stream_t[2+stream_t[1]*sizeof(dword)*5];

 if (dst[0]+1 < 2+sizeof(dword)*5+1 || dst[0]+1 + sizeof (key_t) >= max_length)
  return (dst[0]+1);

 if ((stream_t[0] & 2U) != 0) {
  while (start < &dst[max_length] && *start != 0) {
   start += start[1] + 2;
  }
 }
 if (start + sizeof(key_t) >= &dst[max_length])
  return (dst[0]+1);

 memcpy (start, key_t, sizeof(key_t));
 dst[0] += sizeof(key_t) - ((stream_t[0] & 2U) != 0);
 stream_t[0] |= 2U;

 return (dst[0]+1);
}



/*------------------------------------------------------------------*/
