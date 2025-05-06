
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

#ifndef __DIVA_XDI_COMMON_IO_H_INC__ /* { */
#define __DIVA_XDI_COMMON_IO_H_INC__
#include "dlist.h"
#include "diva_pci.h"
struct _diva_dma_fragment_map;
struct _diva_xdi_clock_control_command;
struct _diva_xdi_hw_access;

typedef struct _diva_oob_entity_queue {
  int count;
  int read;
  int write;
  byte Id[0xff+1];
  byte Ch[0xff+1];
} diva_oob_entity_queue_t;

/*
  Structure use to ack the page entries or Id
  The last of position of the buffer will contain the number of entries to be read
  If none is to be read, it will be equal to 0xffff
  Each entry is 16-bit size
 */
typedef struct _diva_vidi_adapter_handle_buffer {
  word* base;               /* Pointer to the init of the buffer */
  int   max_entries;        /* Number of entries in the buffer   */
  int   saved_entries;      /* Occupied entries                  */
  int   free_entries;       /* Free entries                      */
  word* read_ptr;           /* Index to read from                */
  word* write_ptr;          /* Index to write to                 */
} diva_vidi_adapter_handle_buffer_t;
#define DIVA_VIDI_RNR_BUFFER_VALID 0x8000 /* set if ind_data->data if valid     */
#define DIVA_VIDI_RNR_ENTRY_VALID  0x4000 /* set if ind_data->entry_nr if valid */
typedef struct _diva_rnr_q_entry_hdr {
  diva_entity_link_t link;
  byte  Id;
  byte  Ind;
  byte  IndCh;
  byte  rnr_counter;
  word  data_length;
  union {
    int   entry_nr;
    byte* data;
    byte  local_data[32];
  } ind_data;
} diva_rnr_q_entry_hdr_t;
/*
 maximum = 16 adapters
 */
#define DI_MAX_LINKS    MAX_ADAPTER
#define ISDN_MAX_NUM_LEN 60
/* --------------------------------------------------------------------------
  structure for quadro card management (obsolete for
  systems that do provide per card load event)
  -------------------------------------------------------------------------- */
typedef struct {
 dword         Num ;
 DEVICE_NAME   DeviceName[8] ;
 PISDN_ADAPTER QuadroAdapter[8] ;
} ADAPTER_LIST_ENTRY, *PADAPTER_LIST_ENTRY ;
/* --------------------------------------------------------------------------
  Special OS memory support structures
  -------------------------------------------------------------------------- */
#define MAX_MAPPED_ENTRIES 8
typedef struct {
 void  * Address;
 dword    Length;
} ADAPTER_MEMORY ;
/* --------------------------------------------------------------------------
  Configuration of XDI clients carried by XDI
  -------------------------------------------------------------------------- */
#define DIVA_XDI_CAPI_CFG_1_DYNAMIC_L1_ON      0x01
#define DIVA_XDI_CAPI_CFG_1_GROUP_POPTIMIZATION_ON 0x02
typedef struct _diva_xdi_capi_cfg {
  byte cfg_1;
} diva_xdi_capi_cfg_t;
/* --------------------------------------------------------------------------
    VIDi request flags
   -------------------------------------------------------------------------- */
#define DIVA_VIDI_REQUEST_FLAG_STOP_AND_INTERRUPT 0x00000001
#define DIVA_VIDI_REQUEST_FLAG_READ_XLOG          0x00000002
/* --------------------------------------------------------------------------
  Main data structure kept per adapter
  -------------------------------------------------------------------------- */
struct _ISDN_ADAPTER {
 void             (* DIRequest)(PISDN_ADAPTER, ENTITY *) ;
 int                 State ; /* from NT4 1.srv, a good idea, but  a poor achievment */
 volatile int        Initialized ;
 int     RegisteredWithDidd ;
 volatile int        Unavailable ;  /* callback function possible? */
 int     ResourcesClaimed ;
 int     PnpBiosConfigUsed ;
 void        *Resources; /* Ptr to OS specific resource structure */
 void        *ptask;     /* Contains diva_ptask value of this instance */
 dword    Logging ;
#define CARDMODE_STANDARD   1
#define CARDMODE_RESOURCEBOARD  2
#define CARDMODE_DEBUGBUFFER  3
#define CARDMODE_PLUS_DEBUGBUFFER 0x10
#define CARDMODE_SAVEDEBUG   0x20
#define CARDMODE_MASK    0x0F
#define DBG_MAGIC (0x47114711L)
 dword               CardMode ; /* 1:standard, (2:resourceboard), 3:debugbuffer, 11:normal+debugbuffer */
 dword    features ;
 char    ProtocolIdString[80] ;
 /*
  remember mapped memory areas
 */
 ADAPTER_MEMORY     MappedMemory[MAX_MAPPED_ENTRIES] ;
 CARD_PROPERTIES     Properties ;
 dword               cardType ;
 dword               protocol_id ;       /* configured protocol identifier */
 char                protocol_name[8] ;  /* readable name of protocol */
 dword               BusType ;
 dword               BusNumber ;
 dword               slotNumber ;
 dword               slotId ;
 dword               ControllerNumber ;  /* for QUADRO cards only */
 PISDN_ADAPTER       MultiMaster ;       /* for 4-BRI card only - use MultiMaster or QuadroList */
 PADAPTER_LIST_ENTRY QuadroList ;        /* for QUADRO card  only */
 PDEVICE_OBJECT      DeviceObject ;
 dword               DeviceId ;
 diva_os_adapter_irq_info_t irq_info;
 dword volatile      IrqCount ;
 int                 trapped ;
 dword               DspCodeBaseAddr ;
 dword               MaxDspCodeSize ;
 dword               downloadAddr ;
 dword               DspCodeBaseAddrTable[4] ; /* add. for MultiMaster */
 dword               MaxDspCodeSizeTable[4] ; /* add. for MultiMaster */
 dword               downloadAddrTable[4] ; /* add. for MultiMaster */
 dword    downloadCodeVariant ; /* Protocol+DSP Code - default:0; DSPDVMDM.BIN:1; DSPDVFAX.BIN:2 */
 dword               MemoryBase ;
 dword               MemorySize ;
 byte    *Address ;
 byte    *reset ;
 byte    *port ;
 byte    *image_start;
 byte    *ram ;
 byte    *cfg ;
 byte    *prom ;
 byte    *ctlReg ;
 dword plx_offset;
 struct pc_maint  *pcm ;
 diva_os_dependent_devica_name_t os_name;
 byte                Name[32] ;
 byte                csid ;
 dword               serialNo ;
 dword               ANum ;
 dword               InitialDspInfo ;
 dword               ArchiveType ; /* ARCHIVE_TYPE_NONE ..._SINGLE ..._USGEN ..._MULTI */
 dword               Channels ;
 dword               ProtVersion ;
 char                AddDownload[32] ; /* Dsp- or other additional download files */
 char               *ProtocolSuffix ; /* internal protocolfile table */
 char                Archive[32] ;
 char                Protocol[32] ;
/* number of files hold in AddDownload variable (e.g. DSP + FPGA) */
#define ADD_FILES 2
#if !defined(DIVA_XDI_CFG_LIB_ONLY) /* { */
 char                Oad1[ISDN_MAX_NUM_LEN] ;
 char                Osa1[ISDN_MAX_NUM_LEN] ;
 char                Oad2[ISDN_MAX_NUM_LEN] ;
 char                Osa2[ISDN_MAX_NUM_LEN] ;
 char                Spid1[ISDN_MAX_NUM_LEN] ;
 char                Spid2[ISDN_MAX_NUM_LEN] ;
 byte                nosig ;
 byte                BriLayer2LinkCount ; /* amount of TEI's that adapter will support in P2MP mode */
 dword               tei ;
 dword               nt2 ;
 dword               TerminalCount ;
 dword               WatchDog ;
 dword               Permanent ;
 dword               BChMask ; /* B channel mask for unchannelized modes */
 dword               StableL2 ;
 dword               DidLen ;
 dword               NoOrderCheck ;
 dword               ForceLaw; /* VoiceCoding - default:0, a-law: 1, my-law: 2 */
 dword               SigFlags ;
 dword               LowChannel ;
 dword               NoHscx30 ;
 dword               crc4 ;
 dword               L1TristateOrQsig ; /* enable Layer 1 Tristate (bit 2)Or Qsig params (bit 0,1)*/
 dword               ModemGuardTone ;
 dword               ModemMinSpeed ;
 dword               ModemMaxSpeed ;
 dword               ModemOptions ;
 dword               ModemOptions2 ;
 dword               ModemNegotiationMode ;
 dword               ModemModulationsMask ;
 dword               ModemTransmitLevel ;
 dword               FaxOptions ;
 dword               FaxMaxSpeed ;
 dword               Part68LevelLimiter ;
 dword               LiViaHostMemory ;
 dword               UsEktsNumCallApp ;
 byte                UsEktsFeatAddConf ;
 byte                UsEktsFeatRemoveConf ;
 byte                UsEktsFeatCallTransfer ;
 byte                UsEktsFeatMsgWaiting ;
 byte                QsigDialect;
 byte                ForceVoiceMailAlert;
 byte                DisableAutoSpid;
 byte                ModemCarrierWaitTimeSec;
 byte                ModemCarrierLossWaitTimeTenthSec;
 byte                PiafsLinkTurnaroundInFrames;
 byte                DiscAfterProgress;
 byte                AniDniLimiter[3];
 byte                TxAttenuation;  /* PRI/E1 only: attenuate TX signal */
 word                QsigFeatures;
 dword               GenerateRingtone ;
 dword               SupplementaryServicesFeatures;
 dword               R2Dialect;
 dword               R2CasOptions;
 dword               FaxV34Options;
 dword               DisabledDspMask;
 word                AlertToIn20mSecTicks;
 word                ModemEyeSetup;
 byte                R2CtryLength;
 byte                CCBSRelTimer;
#endif /* } */
 dword               DspImageLength;
 dword               AdapterTestMask;
 byte               *PcCfgBufferFile;/* flexible parameter via file */
 byte               *PcCfgBuffer ; /* flexible parameter via multistring */
 diva_os_dump_file_t dump_file; /* dump memory to file at lowest irq level */
 diva_os_board_trace_t board_trace ; /* traces from the board */
 diva_os_spin_lock_t isr_spin_lock;
 diva_os_spin_lock_t data_spin_lock;
 diva_os_soft_isr_t req_soft_isr;
 diva_os_soft_isr_t isr_soft_isr;
 diva_os_atomic_t in_dpc;
 PBUFFER             RBuffer;        /* Copy of receive lookahead buffer */
 word                e_max;
 word                e_count;
 E_INFO             *e_tbl;
 word                assign;         /* list of pending ASSIGNs  */
 word                head;           /* head of request queue    */
 word                tail;           /* tail of request queue    */
 ADAPTER             a ;             /* not a seperate structure */
 void        (* out)(ADAPTER * a) ;
 byte        (* dpc)(ADAPTER * a) ;
 byte        (* tst_irq)(ADAPTER * a) ;
 void        (* clr_irq)(ADAPTER * a) ;
 int         (* load)(PISDN_ADAPTER) ;
 int         (* mapmem)(PISDN_ADAPTER) ;
 int         (* chkIrq)(PISDN_ADAPTER) ;
 void        (* disIrq)(PISDN_ADAPTER) ;
 void        (* start)(PISDN_ADAPTER) ;
 void        (* stop)(PISDN_ADAPTER) ;
 void        (* rstFnc)(PISDN_ADAPTER) ;
 void        (* trapFnc)(PISDN_ADAPTER) ;
 dword            (* DetectDsps)(PISDN_ADAPTER) ;
 void        (* os_trap_nfy_Fnc)(PISDN_ADAPTER, dword) ;
 int         (*clock_control_Fnc)(PISDN_ADAPTER, const struct _diva_xdi_clock_control_command* cmd);
 diva_os_isr_callback_t diva_isr_handler;
 dword               sdram_bar;  /* must be 32 bit */
 dword               fpga_features;
 volatile int        pcm_pending;
 volatile void *     pcm_data;
 diva_xdi_capi_cfg_t capi_cfg;
 dword               tasks;
 void               *dma_map;
 void               *dma_map2;
 struct _diva_dma_fragment_map *fragment_map;
 int             (*DivaAdapterTestProc)(PISDN_ADAPTER);
 void               *AdapterTestMemoryStart;
 dword               AdapterTestMemoryLength;
 const byte* cfg_lib_memory_init;
 dword       cfg_lib_memory_init_length;
 void*       hdev;
 byte        hw_info[60];
 byte        reentrant;
 byte        msi;
 dword       shared_ram_offset; /* Offset between start of physical ram and the adapter memory */
 byte        uncached_mode;
 dword       ControllerNumberShuffled ;  /* for QUADRO cards only */
 dword       software_options; /* OR to PCINIT_SOFTWARE_OPTIONS */

 /*
  VIDI Variables
  */
 struct _host_vidi {
    int vidi_active;
    byte* req_buffer_base_addr;        /* Request buffer              */
    dword req_buffer_base_dma_magic;
    dword req_buffer_base_dma_magic_hi;
    dword current_req_ptr;             /* Index to the request buffer */
    int req_entry_nr;
    byte* ind_buffer_base_addr;        /* Indication Buffer           */
    dword ind_buffer_base_dma_magic;
    dword ind_buffer_base_dma_magic_hi;
    dword current_ind_ptr;             /* Index to Indication buffer  */
    int dma_nr_pending_requests;       /* Req to be transmitted       */
    int dma_max_nr_pending_requests;   /* Maximum number of request in buffer */
    dword adapter_req_counter_offset;  /* Address in mips for the requests */
    int local_request_counter;         /* Number of request sent so         */
    int local_indication_counter;      /* Number of indications received so */
    dword remote_indication_counter_offset;   /* Pointer to last entry in indication buffer */
    byte* remote_indication_counter;   /* Pointer to last entry in indication buffer */
    dword dma_segment_length;          /* Length of req or ind in corresponding buffers */
    dword dma_segment_req_data_length; /* Length of req in buffer */
    dword dma_ind_buffer_length;             /* Size of indication buffer */
    dword dma_req_buffer_length;             /* Size of request buffer    */
    diva_vidi_adapter_handle_buffer_t   pending_ind_buffer_ack;  
    diva_vidi_adapter_handle_buffer_t   pending_ind_ack;
    dword nr_ind_processed;                  /* Used to Ack Indications */
    volatile int vidi_started;
    volatile dword request_flags;
    volatile int cpu_stop_message_received;
    byte* pcm_data;
    volatile dword pcm_state;
    /*
      Memory, used to manage RNR indications
      */
		diva_entity_queue_t    used_rnr_q;     /* indications in the rnr state */
		diva_entity_queue_t    free_rnr_q;     /* free headers with external memeory */
		diva_entity_queue_t    free_rnr_hdr_q; /* free headers */
    diva_rnr_q_entry_hdr_t rnr_hdrs[0xff];
    dword rnr_timer;
 } host_vidi;
 int (*save_maint_buffer_proc)(PISDN_ADAPTER, const void*, dword);
 int (*notify_adapter_error_proc)(PISDN_ADAPTER, byte type, byte msg_source, byte data, byte info);
 int (*diva_get_adapter_debug_storage_address_proc)(PISDN_ADAPTER, void *);

 void (*clock_isr_callback)(const void*, dword);
 const void*               clock_isr_callback_context;
 volatile dword            clock_isr_active;
 volatile dword            clock_isr_count;
 volatile dword            clock_isr_errors;
 volatile dword            clock_time_stamp_count;

 void (*shedule_stream_proc)(const void* shedule_stream_context);
 const void*               shedule_stream_proc_context;

 void (*get_hw_ifc)(struct _ISDN_ADAPTER*, struct _diva_xdi_hw_access*);
 int (*chained_isr_proc)(void*);
 void* chained_isr_proc_context;

 diva_oob_entity_queue_t oob_q;

 divaXconnectExportMode_t xconnectExportMode;
};
/* ---------------------------------------------------------------------
  Entity table
   --------------------------------------------------------------------- */
struct e_info_s {
  ENTITY *      e;
  byte          next;                   /* chaining index           */
  word          assign_ref;             /* assign reference         */
};
/* ---------------------------------------------------------------------
  S-cards shared ram structure for loading
   --------------------------------------------------------------------- */
struct s_load {
 byte ctrl;
 byte card;
 byte msize;
 byte fill0;
 word ebit;
 word elocl;
 word eloch;
 byte reserved[20];
 word signature;
 byte fill[224];
 byte b[256];
};
#define PR_RAM  ((struct pr_ram *)0)
#define RAM ((struct dual *)0)
/* ---------------------------------------------------------------------
  Functions for port io
   --------------------------------------------------------------------- */
void outp_words_from_buffer (word* adr, byte* P, dword len);
void inp_words_to_buffer    (word* adr, byte* P, dword len);
/* ---------------------------------------------------------------------
  platform specific conversions
   --------------------------------------------------------------------- */
extern void * PTR_P(ADAPTER * a, ENTITY * e, void * P);
extern void * PTR_X(ADAPTER * a, ENTITY * e);
extern void * PTR_R(ADAPTER * a, ENTITY * e);
extern void CALLBACK(ADAPTER * a, ENTITY * e);
extern void set_ram(void * * adr_ptr);
/* ---------------------------------------------------------------------
  ram access functions for io mapped cards
   --------------------------------------------------------------------- */
byte io_in(ADAPTER * a, void * adr);
word io_inw(ADAPTER * a, void * adr);
void io_in_buffer(ADAPTER * a, void * adr, void * P, word length);
void io_look_ahead(ADAPTER * a, PBUFFER * RBuffer, ENTITY * e);
void io_out(ADAPTER * a, void * adr, byte data);
void io_outw(ADAPTER * a, void * adr, word data);
void io_out_buffer(ADAPTER * a, void * adr, void * P, word length);
void io_inc(ADAPTER * a, void * adr);
void bri_in_buffer (PISDN_ADAPTER IoAdapter, dword Pos,
                    void *Buf, dword Len);
int bri_out_buffer (PISDN_ADAPTER IoAdapter, dword Pos,
                    void *Buf, dword Len, int Verify);
/* ---------------------------------------------------------------------
  ram access functions for memory mapped cards
   --------------------------------------------------------------------- */
byte mem_in(ADAPTER * a, void * adr);
word mem_inw(ADAPTER * a, void * adr);
void mem_in_buffer(ADAPTER * a, void * adr, void * P, word length);
void mem_look_ahead(ADAPTER * a, PBUFFER * RBuffer, ENTITY * e);
void mem_out(ADAPTER * a, void * adr, byte data);
void mem_outw(ADAPTER * a, void * adr, word data);
void mem_out_buffer(ADAPTER * a, void * adr, void * P, word length);
void mem_inc(ADAPTER * a, void * adr);
void mem_in_dw (ADAPTER *a, void *addr, dword* data, int dwords);
void mem_out_dw (ADAPTER *a, void *addr, const dword* data, int dwords);
void* mem_cma_va (ADAPTER* a, void* cma);
/*
  Dummy functions for L series
	*/
byte no_mem_in (ADAPTER *a, void *addr);
word no_mem_inw (ADAPTER *a, void *addr);
void no_mem_in_dw (ADAPTER *a, void *addr, dword* data, int dwords);
void no_mem_in_buffer (ADAPTER *a, void *addr, void *buffer, word length);
void no_mem_look_ahead (ADAPTER *a, PBUFFER *RBuffer, ENTITY *e);
void no_mem_out (ADAPTER *a, void *addr, byte data);
void no_mem_outw (ADAPTER *a, void *addr, word data);
void no_mem_out_dw (ADAPTER *a, void *addr, const dword* data, int dwords);
void no_mem_out_buffer (ADAPTER *a, void *addr, void *buffer, word length);
void no_mem_inc (ADAPTER *a, void *addr);
void no_pr_out(ADAPTER * a);
dword no_ram_offset (ADAPTER* a);
byte no_dpc(ADAPTER * a);
byte no_test_int(ADAPTER * a);
void no_clear_int(ADAPTER * a);
void no_cpu_trapped (PISDN_ADAPTER IoAdapter);
int no_load (PISDN_ADAPTER IoAdapter);
/* ---------------------------------------------------------------------
  functions exported by io.c
   --------------------------------------------------------------------- */
extern IDI_CALL Requests[MAX_ADAPTER] ;
extern void     DIDpcRoutine (struct _diva_os_soft_isr* psoft_isr, void* context);
extern void     request (PISDN_ADAPTER, ENTITY *) ;
/* ---------------------------------------------------------------------
  trapFn helpers, used to recover debug trace from dead card
   --------------------------------------------------------------------- */
typedef struct {
 word *buf ;
 word  cnt ;
 word  out ;
} Xdesc ;
extern void     dump_trap_frame  (PISDN_ADAPTER IoAdapter, byte *exception) ;
extern void     dump_xlog_buffer (PISDN_ADAPTER IoAdapter, Xdesc *xlogDesc, dword max_size) ;
extern dword diva_get_serial_number (PISDN_ADAPTER IoAdapter);
extern int diva_get_feature_bytes (dword* data, dword data_length, byte Bus, byte Slot, void* hdev, int pcie);
/* --------------------------------------------------------------------- */
/* ---------------------------------------------------------------------
		used by vidi functions
   --------------------------------------------------------------------- */
void diva_vidi_sync_buffers_and_start_tx_dma(PISDN_ADAPTER IoAdapter);
/* ---------------------------------------------------------------------
		used by adapter download
   --------------------------------------------------------------------- */
int diva_dma_write_sdram_block (PISDN_ADAPTER IoAdapter,
                                int use_burst,
																int alloc_descriptors,
                                dword address,
                                const byte* data,
                                dword length);
/* --------------------------------------------------------------------- */

#endif  /* } __DIVA_XDI_COMMON_IO_H_INC__ */
