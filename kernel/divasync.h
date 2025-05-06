
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

#include <linux/version.h>

#ifndef __DIVA_SYNC__H  
#define __DIVA_SYNC__H
#define IDI_SYNC_REQ_REMOVE             0x00
#define IDI_SYNC_REQ_GET_NAME           0x01
#define IDI_SYNC_REQ_GET_SERIAL         0x02
#define IDI_SYNC_REQ_SET_POSTCALL       0x03
#define IDI_SYNC_REQ_GET_XLOG           0x04
#define IDI_SYNC_REQ_GET_FEATURES       0x05
#define IDI_SYNC_REQ_USB_REGISTER       0x06
#define IDI_SYNC_REQ_USB_RELEASE        0x07
#define IDI_SYNC_REQ_USB_ADD_DEVICE     0x08
#define IDI_SYNC_REQ_USB_START_DEVICE   0x09
#define IDI_SYNC_REQ_USB_STOP_DEVICE    0x0A
#define IDI_SYNC_REQ_USB_REMOVE_DEVICE  0x0B
#define IDI_SYNC_REQ_GET_CARDTYPE       0x0C
#define IDI_SYNC_REQ_GET_DBG_XLOG       0x0D
#define DIVA_USB
#define DIVA_USB_REQ                    0xAC
#define DIVA_USB_TEST                   0xAB
#define DIVA_USB_ADD_ADAPTER            0xAC
#define DIVA_USB_REMOVE_ADAPTER         0xAD
#define IDI_SYNC_REQ_SERIAL_HOOK        0x80
#define IDI_SYNC_REQ_XCHANGE_STATUS     0x81
#define IDI_SYNC_REQ_USB_HOOK           0x82
#define IDI_SYNC_REQ_PORTDRV_HOOK       0x83
#define IDI_SYNC_REQ_SLI                0x84   /*  SLI request from 3signal modem drivers */
#define IDI_SYNC_REQ_RECONFIGURE        0x85
#define IDI_SYNC_REQ_RESET              0x86
#define IDI_SYNC_REQ_GET_85X_DEVICE_DATA     0x87
#define IDI_SYNC_REQ_LOCK_85X                   0x88
#define IDI_SYNC_REQ_DIVA_85X_USB_DATA_EXCHANGE 0x99
#define IDI_SYNC_REQ_DIPORT_EXCHANGE_REQ   0x98
#define IDI_SYNC_REQ_GET_85X_EXT_PORT_TYPE      0xA0
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_GET_EXTENDED_FEATURES  0x92
/*
   To receive XDI features:
   1. set 'buffer_length_in_bytes' to length of you buffer
   2. set 'features' to pointer to your buffer
   3. issue synchronous request to XDI
   4. Check that feature 'DIVA_XDI_EXTENDED_FEATURES_VALID' is present
      after call. This feature does indicate that your request
      was processed and XDI does support this synchronous request
   5. if on return bit 31 (0x80000000) in 'buffer_length_in_bytes' is
      set then provided buffer was too small, and bits 30-0 does
      contain necessary length of buffer.
      in this case only features that do find place in the buffer
      are indicated to caller
*/
typedef struct _diva_xdi_get_extended_xdi_features {
  dword buffer_length_in_bytes;
  byte  *features;
} diva_xdi_get_extended_xdi_features_t;
/*
   features[0]
  */
#define DIVA_XDI_EXTENDED_FEATURES_VALID          0x01
#define DIVA_XDI_EXTENDED_FEATURE_CMA             0x02
#define DIVA_XDI_EXTENDED_FEATURE_SDRAM_BAR       0x04
#define DIVA_XDI_EXTENDED_FEATURE_CAPI_PRMS       0x08
#define DIVA_XDI_EXTENDED_FEATURE_NO_CANCEL_RC    0x10
#define DIVA_XDI_EXTENDED_FEATURE_RX_DMA          0x20
#define DIVA_XDI_EXTENDED_FEATURE_MANAGEMENT_DMA  0x40
#define DIVA_XDI_EXTENDED_FEATURE_WIDE_ID         0x80
/*
   features[1]
  */
#define DIVA_XDI_EXTENDED_FEATURE_FC_OK_LABEL     0x01
#define DIVA_XDI_EXTENDED_FEATURE_XON_OOB_REQ     0x02
#define DIVA_XDI_EXTENDED_FEATURES_MAX_SZ    2
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_GET_ADAPTER_SDRAM_BAR   0x93
typedef struct _diva_xdi_get_adapter_sdram_bar {
 dword bar;
} diva_xdi_get_adapter_sdram_bar_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_GET_CAPI_PARAMS   0x94
/*
  CAPI Parameters will be written in the caller's buffer
  */
typedef struct _diva_xdi_get_capi_parameters {
  dword structure_length;
  byte flag_dynamic_l1_down;
  byte group_optimization_enabled;
} diva_xdi_get_capi_parameters_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER   0x95
/*
  Get logical adapter number, as assigned by XDI
  'controller' is starting with zero 'sub' controller number
  in case of one adapter that supports multiple interfaces
  'controller' is zero for Master adapter (and adapter that supports
  only one interface)
  */
typedef struct _diva_xdi_get_logical_adapter_number {
  dword logical_adapter_number;
  dword controller;
  dword total_controllers;
} diva_xdi_get_logical_adapter_number_s_t;
/******************************************************************************/
#define IDI_SYNC_REQ_UP1DM_OPERATION   0x96
/******************************************************************************/
#define IDI_SYNC_REQ_DMA_DESCRIPTOR_OPERATION 0x97
#define IDI_SYNC_REQ_DMA_DESCRIPTOR_ALLOC     0x01
#define IDI_SYNC_REQ_DMA_DESCRIPTOR_FREE      0x02
#define IDI_SYNC_REQ_DMA_DESCRIPTOR_HI        0x03
typedef struct _diva_xdi_dma_descriptor_operation {
  int   operation;
  int   descriptor_number;
  void* descriptor_address;
  dword descriptor_magic;
} diva_xdi_dma_descriptor_operation_t;
/******************************************************************************/
/*
  Request from MAINT driver to save trace buffer to the memory of one
  of Diva adapters.
  */
#define IDI_SYNC_REQ_XDI_SAVE_MAINT_BUFFER_OPERATION 0xA3
typedef struct _diva_xdi_save_maint_buffer_operation {
  const void* start;
  dword       length;
  int         success;
} diva_xdi_save_maint_buffer_operation_t;
/******************************************************************************/
#define IDI_SYNC_REQ_DIDD_REGISTER_ADAPTER_NOTIFY   0x01
#define IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER_NOTIFY     0x02
#define IDI_SYNC_REQ_DIDD_ADD_ADAPTER               0x03
#define IDI_SYNC_REQ_DIDD_REMOVE_ADAPTER            0x04
#define IDI_SYNC_REQ_DIDD_READ_ADAPTER_ARRAY        0x05
#define IDI_SYNC_REQ_DIDD_GET_CFG_LIB_IFC           0x10
typedef struct _diva_didd_adapter_notify {
 dword handle; /* Notification handle */
 void   * callback;
 void   * context;
} diva_didd_adapter_notify_t;
typedef struct _diva_didd_add_adapter {
 void   * descriptor;
} diva_didd_add_adapter_t;
typedef struct _diva_didd_remove_adapter {
 IDI_CALL p_request;
} diva_didd_remove_adapter_t;
typedef struct _diva_didd_read_adapter_array {
 void   * buffer;
 dword length;
} diva_didd_read_adapter_array_t;
typedef struct _diva_didd_get_cfg_lib_ifc {
 void* ifc;
} diva_didd_get_cfg_lib_ifc_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_GET_CLOCK_DATA    0x91
typedef struct _diva_xdi_get_clock_data {
  dword bus_addr_lo;
  dword bus_addr_hi;
  dword length;
  void* addr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
  void* pci_dev_handle;
#endif
} diva_xdi_get_clock_data_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_ADAPTER_REMOVED_NOT_AVAILABLE 0xA4
/*
  Used by client driver to indicate thet current adapter
  can not process any request or indication any more
  */
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_MAP_ADDRESS 0xA5
/*
  Used to map between DMA and virtual addresses
  */
typedef struct _diva_xdi_map_address {
  dword info;
  dword logical_adapter_number;
  dword bus_addr_lo;
  dword bus_addr_hi;
  void* addr;
  dword handle;
  dword offset;
} diva_xdi_map_address_t;
/******************************************************************************/
#define IDI_SYNC_REQ_GET_INTERNAL_REQUEST_PROC 0xA6
/*
  Used by certain IDI clients to prevent synchronization problems
  in user mode IDI proxy code.
  */
typedef struct _diva_get_internal_request {
  void (*request_proc)(void* context, ENTITY* e);
  void* context;
} diva_get_internal_request_t;
/******************************************************************************/
#define IDI_SYNC_REQ_GET_ADAPTER_DEBUG_STORAGE_ADDRESS 0xA7
/*
  Used by MAINT driver to move internal memory to external storage
  */
typedef struct _diva_get_adapter_debug_storage_address {
  void* storage_start;
  dword storage_length;
} diva_get_adapter_debug_storage_address_t;
/******************************************************************************/
#define IDI_SYNC_REQ_GET_MTPX_ADAPTER_XDI_CONFIG 0xA8
/*
  Used to read MTPX adapter configuration
  */
typedef struct _diva_mtpx_adapter_xdi_config_entry {
  dword card_id;
  word card_type; /* 0 - PRI, 1 - BRI, 2 - Analog, 3 - SoftIP */
  word channels;
  word logical_adapter_number;
  word cardtype;
  dword serial_number;
  byte  interfaces;
  byte  ifc;
  word  unused;
} diva_mtpx_adapter_config_entry_t;
typedef struct _diva_get_mtx_adapter_xdi_config {
  dword number_of_configuration_entries;
  dword configuration_entry_size;
  void (*release_configuration_proc)(const diva_mtpx_adapter_config_entry_t* cfg);
  const diva_mtpx_adapter_config_entry_t* cfg;
} diva_get_mtpx_adapter_xdi_config_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XDI_CLOCK_CONTROL 0xA9
typedef void (*diva_xdi_clock_isr_proc_t)(const void* context, dword count);
typedef struct _diva_xdi_clock_control_command_on {
  diva_xdi_clock_isr_proc_t isr_callback;
  const void*               isr_callback_context;
  dword                     port;
	dword* volatile           sample_counter;
} diva_xdi_clock_control_command_on;
typedef struct _diva_xdi_clock_control_command_off {
  dword                     port;
} diva_xdi_clock_control_command_off;
typedef union _diva_xdi_clock_control_command_data {
  diva_xdi_clock_control_command_on  on;
  diva_xdi_clock_control_command_off off;
} diva_xdi_clock_control_command_data_t;
typedef struct _diva_xdi_clock_control_command {
  dword                                 command; /* 0 - Off, 1 - On */
  diva_xdi_clock_control_command_data_t data;
} diva_xdi_clock_control_command_t;
/******************************************************************************/
#define IDI_SYNC_REQ_GET_XDI_IFC 0xAA
typedef struct _diva_xdi_idi_ifc {
  void* (*ifc_alloc_proc_fn)(ENTITY*, PIDI_CALL, int, byte, byte, dword);
  void  (*request_proc_fn)(void* context);
  void  (*ctrl_proc_fn)(void* context, int, dword);
  void  (*ifc_release_proc_fn)(void*);
} diva_xdi_idi_ifc_t;
/******************************************************************************/
#define IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING 0xAB
#define IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK            0
#define IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_ALLOC_COMMAND 1
#define IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_FREE_COMMAND  2
#define IDI_SYNC_REQ_PROCESS_STREAMING_SYSTEM_MAP_COMMAND    3
#define IDI_SYNC_REQ_PROCESS_STREAMING_SYSTEM_INSTALL_CALLBACK_COMMAND 4
typedef struct _diva_xdi_process_streaming_mapping {
  dword request;  /* 0 - alloc, 1 - release, 2 - request system mapping */
  dword dma_lo;   /* DMA addr lo or system lo */
  dword dma_hi;   /* DMA addr hi or system hi */
  void* addr;     /* DMA or system mapped address */
  int dma_handle; /* DMA handle */
	void (*shedule_stream_proc)(const void* shedule_stream_proc_context);
	const void* shedule_stream_proc_context;
} diva_xdi_process_streaming_mapping_t;
/******************************************************************************/
#define IDI_SYNC_REQ_HW_ACCESS 0xAC
typedef struct _diva_xdi_hw_access {
  void* hdev;
  void* resource;
  dword bar[4];
  void* addr[4];
  void* (*alloc_dma_page_proc)(void* hdev, dword* lo, dword* hi);
  void  (*free_dma_page_proc)(void* hdev, void* addr, dword lo, dword hi);
	int (*connect_irq_proc)(void*, int (*)(void*), void*);
	dword logical_adapter_number;
  dword serial_number;
} diva_xdi_hw_access_t;
/******************************************************************************/
#define IDI_SYNC_REQ_XON 0xAD
typedef struct _diva_xdi_xon {
 byte Id;
 byte Ch;
} diva_xdi_xon_t;
/******************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
#define IDI_SYNC_REQ_XDI_UNMAP_CLOCK_DATA_ADDR 0xAE
/*
  Used to unmap clock data DMA
  */
/******************************************************************************/
#endif
/*
 * IDI_SYNC_REQ_SERIAL_HOOK - special interface for the DIVA Mobile card
 */
typedef struct
{ unsigned char LineState;         /* Modem line state (STATUS_R) */
#define SERIAL_GSM_CELL 0x01   /* GSM or CELL cable attached  */
 unsigned char CardState;          /* PCMCIA card state (0 = down) */
 unsigned char IsdnState;          /* ISDN layer 1 state (0 = down)*/
 unsigned char HookState;          /* current logical hook state */
#define SERIAL_ON_HOOK 0x02   /* set in DIVA CTRL_R register */
} SERIAL_STATE;
typedef int (  * SERIAL_INT_CB) (void *Context) ;
typedef int (  * SERIAL_DPC_CB) (void *Context) ;
typedef unsigned char (  * SERIAL_I_SYNC) (void *Context) ;
typedef struct
{ /* 'Req' and 'Rc' must be at the same place as in the ENTITY struct */
 unsigned char Req;             /* request (must be always 0) */
 unsigned char Rc;              /* return code (is the request) */
 unsigned char Function;           /* private function code  */
#define SERIAL_HOOK_ATTACH 0x81
#define SERIAL_HOOK_STATUS 0x82
#define SERIAL_HOOK_I_SYNC 0x83
#define SERIAL_HOOK_NOECHO 0x84
#define SERIAL_HOOK_RING 0x85
#define SERIAL_HOOK_DETACH 0x8f
 unsigned char Flags;           /* function refinements   */
 /* parameters passed by the the ATTACH request      */
 SERIAL_INT_CB InterruptHandler; /* called on each interrupt  */
 SERIAL_DPC_CB DeferredHandler; /* called on hook state changes */
 void   *HandlerContext; /* context for both handlers */
 /* return values for both the ATTACH and the STATUS request   */
 unsigned long IoBase;    /* IO port assigned to UART  */
 SERIAL_STATE State;
 /* parameters and return values for the I_SYNC function    */
 SERIAL_I_SYNC SyncFunction;  /* to be called synchronized */
 void   *SyncContext;  /* context for this function */
 unsigned char SyncResult;   /* return value of function  */
} SERIAL_HOOK;
/*
 * IDI_SYNC_REQ_XCHANGE_STATUS - exchange the status between IDI and WMP
 * IDI_SYNC_REQ_RECONFIGURE - reconfiguration of IDI from WMP
 */
typedef struct
{ /* 'Req' and 'Rc' must be at the same place as in the ENTITY struct */
 unsigned char Req;             /* request (must be always 0) */
 unsigned char Rc;              /* return code (is the request) */
#define DRIVER_STATUS_BOOT  0xA1
#define DRIVER_STATUS_INIT_DEV 0xA2
#define DRIVER_STATUS_RUNNING 0xA3
#define DRIVER_STATUS_SHUTDOWN 0xAF
#define DRIVER_STATUS_TRAPPED 0xAE
 unsigned char wmpStatus;          /* exported by WMP              */
 unsigned char idiStatus;   /* exported by IDI              */
 unsigned long wizProto ;   /* from WMP registry to IDI     */
 /* the cardtype value is defined by cardtype.h */
 unsigned long cardType ;   /* from IDI registry to WMP     */
 unsigned long nt2 ;    /* from IDI registry to WMP     */
 unsigned long permanent ;   /* from IDI registry to WMP     */
 unsigned long stableL2 ;   /* from IDI registry to WMP     */
 unsigned long tei ;    /* from IDI registry to WMP     */
#define CRC4_MASK   0x00000003
#define L1_TRISTATE_MASK 0x00000004
#define WATCHDOG_MASK  0x00000008
#define NO_ORDER_CHECK_MASK 0x00000010
#define LOW_CHANNEL_MASK 0x00000020
#define NO_HSCX30_MASK  0x00000040
#define MODE_MASK   0x00000080
#define SET_BOARD   0x00001000
#define SET_CRC4   0x00030000
#define SET_L1_TRISTATE  0x00040000
#define SET_WATCHDOG  0x00080000
#define SET_NO_ORDER_CHECK 0x00100000
#define SET_LOW_CHANNEL  0x00200000
#define SET_NO_HSCX30  0x00400000
#define SET_MODE   0x00800000
#define SET_PROTO   0x02000000
#define SET_CARDTYPE  0x04000000
#define SET_NT2    0x08000000
#define SET_PERMANENT  0x10000000
#define SET_STABLEL2  0x20000000
#define SET_TEI    0x40000000
#define SET_NUMBERLEN  0x80000000
 unsigned long Flag ;  /* |31-Type-16|15-Mask-0| */
 unsigned long NumberLen ; /* reconfiguration: union is empty */
 union {
  struct {    /* possible reconfiguration, but ... ; SET_BOARD */
   unsigned long SerialNumber ;
   char     *pCardname ; /* di_defs.h: BOARD_NAME_LENGTH */
  } board ;
  struct {      /* reset: need resources */
   void * pRawResources ;
   void * pXlatResources ;
  } res ;
  struct { /* reconfiguration: wizProto == PROTTYPE_RBSCAS */
#define GLARE_RESOLVE_MASK 0x00000001
#define DID_MASK   0x00000002
#define BEARER_CAP_MASK  0x0000000c
#define SET_GLARE_RESOLVE 0x00010000
#define SET_DID    0x00020000
#define SET_BEARER_CAP  0x000c0000
   unsigned long Flag ;  /* |31-Type-16|15-VALUE-0| */
   unsigned short DigitTimeout ;
   unsigned short AnswerDelay ;
  } rbs ;
  struct { /* reconfiguration: wizProto == PROTTYPE_QSIG */
#define CALL_REF_LENGTH1_MASK 0x00000001
#define BRI_CHANNEL_ID_MASK  0x00000002
#define SET_CALL_REF_LENGTH  0x00010000
#define SET_BRI_CHANNEL_ID  0x00020000
   unsigned long Flag ;  /* |31-Type-16|15-VALUE-0| */
  } qsig ;
  struct { /* reconfiguration: NumberLen != 0 */
#define SET_SPID1   0x00010000
#define SET_NUMBER1   0x00020000
#define SET_SUBADDRESS1  0x00040000
#define SET_SPID2   0x00100000
#define SET_NUMBER2   0x00200000
#define SET_SUBADDRESS2  0x00400000
#define MASK_SET   0xffff0000
   unsigned long Flag ;   /* |31-Type-16|15-Channel-0| */
   unsigned char *pBuffer ; /* number value */
  } isdnNo ;
 }
parms
;
} isdnProps ;
/*
 * IDI_SYNC_REQ_PORTDRV_HOOK - signal plug/unplug (Award Cardware only)
 */
typedef void (  * PORTDRV_HOOK_CB) (void *Context, int Plug) ;
typedef struct
{ /* 'Req' and 'Rc' must be at the same place as in the ENTITY struct */
 unsigned char Req;             /* request (must be always 0) */
 unsigned char Rc;              /* return code (is the request) */
 unsigned char Function;           /* private function code  */
 unsigned char Flags;           /* function refinements   */
 PORTDRV_HOOK_CB Callback;   /* to be called on plug/unplug */
 void   *Context;   /* context for callback   */
 unsigned long Info;    /* more info if needed   */
} PORTDRV_HOOK ;
/*  Codes for the 'Rc' element in structure below. */
#define SLI_INSTALL     (0xA1)
#define SLI_UNINSTALL   (0xA2)
typedef int ( * SLIENTRYPOINT)(void* p3SignalAPI, void* pContext);
typedef struct
{   /* 'Req' and 'Rc' must be at the same place as in the ENTITY struct */
    unsigned char   Req;                /* request (must be always 0)   */
    unsigned char   Rc;                 /* return code (is the request) */
    unsigned char   Function;           /* private function code        */
    unsigned char   Flags;              /* function refinements         */
    SLIENTRYPOINT   Callback;           /* to be called on plug/unplug  */
    void            *Context;           /* context for callback         */
    unsigned long   Info;               /* more info if needed          */
} SLIENTRYPOINT_REQ ;
/******************************************************************************/
/*
 *  Definitions for DIVA USB
 */
typedef int  (  * USB_SEND_REQ) (unsigned char PipeIndex, unsigned char Type,void *Data, int sizeData);
typedef int  (  * USB_START_DEV) (void *Adapter, void *Ipac) ;
/* called from WDM */
typedef void (  * USB_RECV_NOTIFY) (void *Ipac, void *msg) ;
typedef void (  * USB_XMIT_NOTIFY) (void *Ipac, unsigned char PipeIndex) ;
/******************************************************************************/
/*
 * Parameter description for synchronous requests.
 *
 * Sorry, must repeat some parts of di_defs.h here because
 * they are not defined for all operating environments
 */
typedef union
{ ENTITY Entity;
 struct
 { /* 'Req' and 'Rc' are at the same place as in the ENTITY struct */
  unsigned char   Req; /* request (must be always 0) */
  unsigned char   Rc;  /* return code (is the request) */
 }   Request;
 struct
 { unsigned char   Req; /* request (must be always 0) */
  unsigned char   Rc;  /* return code (0x01)   */
  unsigned char   name[BOARD_NAME_LENGTH];
 }   GetName;
 struct
 { unsigned char   Req; /* request (must be always 0) */
  unsigned char   Rc;  /* return code (0x02)   */
  unsigned long   serial; /* serial number    */
 }   GetSerial;
 struct
 { unsigned char   Req; /* request (must be always 0) */
  unsigned char   Rc;  /* return code (0x02)   */
  unsigned long   lineIdx;/* line, 0 if card has only one */
 }   GetLineIdx;
 struct
 { unsigned char  Req;     /* request (must be always 0) */
  unsigned char  Rc;      /* return code (0x02)   */
  unsigned long  cardtype;/* card type        */
 }   GetCardType;
 struct
 { unsigned short command;/* command = 0x0300 */
  unsigned short dummy; /* not used */
  IDI_CALL       callback;/* routine to call back */
  ENTITY      *contxt; /* ptr to entity to use */
 }   PostCall;
 struct
 { unsigned char  Req;  /* request (must be always 0) */
  unsigned char  Rc;   /* return code (0x04)   */
  unsigned char  pcm[1]; /* buffer (a pc_maint struct) */
 }   GetXlog;
 struct
 { unsigned char  Req;  /* request (must be always 0) */
  unsigned char  Rc;   /* return code (0x05)   */
  unsigned short features;/* feature defines see below */
 }   GetFeatures;
 SERIAL_HOOK  SerialHook;
/* Added for DIVA USB */
 struct
 { unsigned char   Req;
  unsigned char   Rc;
  USB_SEND_REQ    UsbSendRequest; /* function in Diva Usb WDM driver in usb_os.c, */
                                        /* called from usb_drv.c to send a message to our device */
                                        /* eg UsbSendRequest (USB_PIPE_SIGNAL, USB_IPAC_START, 0, 0) ; */
  USB_RECV_NOTIFY usb_recv;       /* called from usb_os.c to pass a received message and ptr to IPAC */
                                        /* on to usb_drv.c by a call to usb_recv(). */
  USB_XMIT_NOTIFY usb_xmit;       /* called from usb_os.c in DivaUSB.sys WDM to indicate a completed transmit */
                                        /* to usb_drv.c by a call to usb_xmit(). */
  USB_START_DEV   UsbStartDevice; /* Start the USB Device, in usb_os.c */
  IDI_CALL        callback;       /* routine to call back */
  ENTITY          *contxt;     /* ptr to entity to use */
  void            ** ipac_ptr;    /* pointer to struct IPAC in VxD */
 } Usb_Msg_old;
/* message used by WDM and VXD to pass pointers of function and IPAC* */
 struct
 { unsigned char Req;
  unsigned char Rc;
        USB_SEND_REQ    pUsbSendRequest;/* function in Diva Usb WDM driver in usb_os.c, */
                                        /* called from usb_drv.c to send a message to our device */
                                        /* eg UsbSendRequest (USB_PIPE_SIGNAL, USB_IPAC_START, 0, 0) ; */
        USB_RECV_NOTIFY p_usb_recv;     /* called from usb_os.c to pass a received message and ptr to IPAC */
                                        /* on to usb_drv.c by a call to usb_recv(). */
        USB_XMIT_NOTIFY p_usb_xmit;     /* called from usb_os.c in DivaUSB.sys WDM to indicate a completed transmit */
                                        /* to usb_drv.c by a call to usb_xmit().*/
  void            *ipac_ptr;      /* &Diva.ipac pointer to struct IPAC in VxD */
 } Usb_Msg;
 PORTDRV_HOOK PortdrvHook;
    SLIENTRYPOINT_REQ   sliEntryPointReq;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_get_clock_data_t info;
  } xdi_clock_data;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_get_extended_xdi_features_t info;
  } xdi_extended_features;
 struct {
    unsigned char Req;
    unsigned char Rc;
  diva_xdi_get_adapter_sdram_bar_t info;
 } xdi_sdram_bar;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_get_capi_parameters_t info;
  } xdi_capi_prms;
 struct {
  ENTITY           e;
  diva_didd_adapter_notify_t info;
 } didd_notify;
 struct {
  ENTITY           e;
  diva_didd_add_adapter_t   info;
 } didd_add_adapter;
 struct {
  ENTITY           e;
  diva_didd_remove_adapter_t info;
 } didd_remove_adapter;
 struct {
  ENTITY             e;
  diva_didd_read_adapter_array_t info;
 } didd_read_adapter_array;
 struct {
  ENTITY             e;
  diva_didd_get_cfg_lib_ifc_t     info;
 } didd_get_cfg_lib_ifc;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_get_logical_adapter_number_s_t info;
  } xdi_logical_adapter_number;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_dma_descriptor_operation_t info;
  } xdi_dma_descriptor_operation;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_save_maint_buffer_operation_t info;
  } xdi_save_maint_buffer_operation;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_map_address_t info;
  } xdi_map_address;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_get_internal_request_t info;
  } diva_get_internal_request;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_get_adapter_debug_storage_address_t info;
  } diva_get_adapter_debug_storage_address;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_get_mtpx_adapter_xdi_config_t info;
  } diva_get_mtpx_adapter_xdi_config;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_clock_control_command_t info;
  } diva_xdi_clock_control_command_req;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_idi_ifc_t info;
  } diva_xdi_idi_ifc_req;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_process_streaming_mapping_t info;
  } diva_xdi_streaming_mapping_req;
  struct {
    unsigned char Req;
    unsigned char Rc;
    diva_xdi_hw_access_t info;
  } diva_xdi_hw_access;
 struct {
    unsigned char Req;
    unsigned char Rc;
  diva_xdi_xon_t info;
 } diva_xdi_xon;
} IDI_SYNC_REQ;
/******************************************************************************/
#endif /* __DIVA_SYNC__H */  
