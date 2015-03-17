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
#ifndef __DIVA_XDI_UM_CFG_MESSSGE_H__
#define __DIVA_XDI_UM_CFG_MESSAGE_H__

/*
  Definition of commands for user mode configuration
  utility to XDI device driver
*/

/*
  XDI WRITE command
*/
#define DIVA_XDI_IO_CMD_WRITE_MSG    0x3701

/*
  XDI READ command
*/
#define DIVA_XDI_IO_CMD_READ_MSG     0x3702


/*
  Definition of messages used to communicate between
  XDI device driver and user mode configuration utility
*/

/*
  As acknowledge one DWORD - card ordinal will be read from the card
*/
#define DIVA_XDI_UM_CMD_GET_CARD_ORDINAL	0

/*
  no acknowledge will be generated, memory block will be written in the
  memory at given offset
*/
#define DIVA_XDI_UM_CMD_WRITE_SDRAM_BLOCK	1

/*
  no acknowledge will be genatated, FPGA will be programmed
*/
#define DIVA_XDI_UM_CMD_WRITE_FPGA				2

/*
  As acknowledge block of SDRAM will be read in the user buffer
*/
#define DIVA_XDI_UM_CMD_READ_SDRAM				3

/*
  As acknowledge dword with serial number will be read in the user buffer
*/
#define DIVA_XDI_UM_CMD_GET_SERIAL_NR			4

/*
  As acknowledge struct consisting from 9 dwords with PCI info.
  dword[0...7] = 8 PCI BARS
  dword[9]		 = IRQ
*/
#define DIVA_XDI_UM_CMD_GET_PCI_HW_CONFIG	5

/*
  Reset of the board + activation of primary
  boot loader
*/
#define DIVA_XDI_UM_CMD_RESET_ADAPTER			6

/*
  Called after code download to start adapter
  at specified address
  Start does set new set of features due to fact that we not know
  if protocol features have changed
*/
#define DIVA_XDI_UM_CMD_START_ADAPTER			7

/*
  Stop adapter, called if user
  wishes to stop adapter without unload
  of the driver, to reload adapter with
  different protocol
*/
#define DIVA_XDI_UM_CMD_STOP_ADAPTER			8

/*
  Get state of current adapter
  Acknowledge is one dword with following values:
  0 - adapter ready for download
  1 - adapter running
  2 - adapter dead
  3 - out of service, driver should be restarted or hardware problem
*/
#define DIVA_XDI_UM_CMD_GET_CARD_STATE		9

/*
  Reads XLOG entry from the card
*/
#define DIVA_XDI_UM_CMD_READ_XLOG_ENTRY		10

/*
  Set untranslated protocol code features
  */
#define DIVA_XDI_UM_CMD_SET_PROTOCOL_FEATURES	11

/*
  Start adapter test procedure
  */
#define DIVA_XDI_UM_CMD_ADAPTER_TEST	12

/*
  Alloc DMA descriptor
  */
#define DIVA_XDI_UM_CMD_ALLOC_DMA_DESCRIPTOR 14


/*
  Get hardware information structure
  */
#define DIVA_XDI_UM_CMD_GET_HW_INFO_STRUCT 15

/*
	Change XDI driver protocol code version
	*/
#define DIVA_XDI_UM_CMD_SET_PROTOCOL_CODE_VERSION 17

/*
	Init VIDI, read VIDI info from driver
	*/
#define DIVA_XDI_UM_CMD_INIT_VIDI 18

/*
  Select VIDI mode
  */
#define DIVA_XDI_UM_CMD_SET_VIDI_MODE 19

typedef struct _diva_xdi_um_cfg_cmd_set_vidi_mode {
  dword vidi_mode;
} diva_xdi_um_cfg_cmd_set_vidi_mode_t;

typedef struct _diva_xdi_um_cfg_cmd_data_init_vidi {
	dword req_magic_lo;
	dword req_magic_hi;
	dword ind_magic_lo;
	dword ind_magic_hi;

	dword dma_segment_length;
	dword dma_req_buffer_length;
	dword dma_ind_buffer_length;
	dword dma_ind_remote_counter_offset;
} diva_xdi_um_cfg_cmd_data_init_vidi_t;

/*
	Get clock memory info
	*/
#define DIVA_XDI_UM_CMD_GET_CLOCK_MEMORY_INFO 20

/*
	Read PLX register
	*/
#define DIVA_XDI_UM_CMD_READ_WRITE_PLX_REGISTER 21

/*
	Create DMA descriptor
	*/
#define DIVA_XDI_UM_CMD_CREATE_XDI_DESCRIPTORS   22

/*
	Clock interrupt
	*/
#define DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_CONTROL 23

/*
	Clock interrupt data
	*/
#define DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_DATA 24

typedef struct _diva_xdi_um_cfg_cmd_get_clock_memory_info {
	dword bus_addr_lo;
	dword bus_addr_hi;
	dword length;
} diva_xdi_um_cfg_cmd_get_clock_memory_info_t;

typedef struct _diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor {
  dword nr;
  dword low;
  dword high;
} diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t;

typedef struct _diva_xdi_um_cfg_cmd_data_adapter_test {
  dword test_command;
} diva_xdi_um_cfg_cmd_data_adapter_test_t;

typedef struct _diva_xdi_um_cfg_cmd_data_set_features {
	dword features;
} diva_xdi_um_cfg_cmd_data_set_features_t;

typedef struct _diva_xdi_um_cfg_cmd_data_start {
	dword offset;
	dword features;
} diva_xdi_um_cfg_cmd_data_start_t;

typedef struct _diva_xdi_um_cfg_cmd_data_write_sdram {
	dword	ram_number;
	dword	offset;
	dword	length;
} diva_xdi_um_cfg_cmd_data_write_sdram_t;

typedef struct _diva_xdi_um_cfg_cmd_data_write_fpga {
	dword	fpga_number;
	dword	image_length;
} diva_xdi_um_cfg_cmd_data_write_fpga_t;

typedef struct _diva_xdi_um_cfg_cmd_data_read_sdram {
	dword	ram_number;
	dword	offset;
	dword	length;
} diva_xdi_um_cfg_cmd_data_read_sdram_t;

typedef struct _diva_xdi_um_cfg_cmd_data_set_protocol_code_version {
	dword version;
} diva_xdi_um_cfg_cmd_data_set_protocol_code_version_t;

typedef struct _diva_xdi_um_cfg_cmd_read_write_plx_register {
	byte write;
	byte offset;
	byte length;
	dword value;
} diva_xdi_um_cfg_cmd_read_write_plx_register_t;

typedef struct _diva_xdi_um_cfg_cmd_clock_interrupt_control {
	dword command;
} diva_xdi_um_cfg_cmd_clock_interrupt_control_t;

typedef struct _diva_xdi_um_cfg_cmd_get_clock_interrupt_data {
	dword state;
	dword clock;
	dword errors;
} diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t;

typedef union _diva_xdi_um_cfg_cmd_data {
  diva_xdi_um_cfg_cmd_data_write_sdram_t	write_sdram;
  diva_xdi_um_cfg_cmd_data_write_fpga_t	write_fpga;
  diva_xdi_um_cfg_cmd_data_read_sdram_t	read_sdram;
  diva_xdi_um_cfg_cmd_data_start_t	start;
  diva_xdi_um_cfg_cmd_data_set_features_t features;
  diva_xdi_um_cfg_cmd_data_adapter_test_t test;
  diva_xdi_um_cfg_cmd_data_alloc_dma_descriptor_t dma;
  diva_xdi_um_cfg_cmd_data_set_protocol_code_version_t protocol_code_version;
  diva_xdi_um_cfg_cmd_data_init_vidi_t vidi;
  diva_xdi_um_cfg_cmd_set_vidi_mode_t  vidi_mode;
  diva_xdi_um_cfg_cmd_get_clock_memory_info_t clock_memory;
  diva_xdi_um_cfg_cmd_read_write_plx_register_t     plx_register;
  diva_xdi_um_cfg_cmd_clock_interrupt_control_t     clock_interrupt_control;
  diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t    clock_interrupt_data;
} diva_xdi_um_cfg_cmd_data_t;

typedef struct _diva_xdi_um_cfg_cmd {
  dword  adapter; /* Adapter number 1...N */
  dword  command;
  diva_xdi_um_cfg_cmd_data_t  command_data;
  dword  data_length; /* Plain binary data will follow */
} diva_xdi_um_cfg_cmd_t;

typedef struct _diva_xdi_io_cmd {
  unsigned int length;
  void * cmd;
} diva_xdi_io_cmd;

#endif

