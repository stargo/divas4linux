
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

#ifndef __DIVA_CFG_LIB_TYPES_H__
#define __DIVA_CFG_LIB_TYPES_H__

#define DIVA_CFG_LIB_VERSION 0x00000001

typedef enum _diva_vie_id {
	IeNotDefined = 0,
	Tlie     = 1,
	VieOwner,
	VieInstance,
  VieInstance2,
  ViePcInitname,
  ViePcInitValue,
  VieRamInitOffset,
	VieRamInitOperation,
	VieRamInitValue,
  VieImageType,
	VieImageName,
	VieImageArchive,
	VieImageData,
	VieNamedVarName,
	VieNamedVarValue,
	VieManagementInfo,

	VieBindingDirection,
	VieBindingEnable,
	VieBindingService,
	VieBindingServiceMask,
	VieBindingBC,
	VieBindingHLC,
	VieBindingLLC,

	VieBindingRangeStart,
	VieBindingRangeEnd,
	VieBindingRangeStartCiPN,
	VieBindingRangeEndCiPN,

	VieBindingRangeStartSub,
	VieBindingRangeEndSub,
	VieBindingRangeStartCiPNSub,
	VieBindingRangeEndCiPNSub,

  VieBindingBindTo,
  VieBindingBindTo2,

	VieParameterLine,

	VieLlast
} diva_vie_id_t;

typedef enum _diva_section_target {
	TargetCFGLibDrv = 0,
	TargetXdiDrv,
	TargetXdiAdapter,
	TargetCapiDrv,
	TargetTapiDrv,
	TargetWmpDrv,
	TargetComDrv,
	TargetMAINTDrv,
	TargetIOCTLDrv,
	TargetMtpxMngt,
	TargetMtpxAdapter,
	TargetSoftIPAdapter,
	TargetSoftIPControl,
	TargetSipControl,
	TargetMTP3,
	TargetISUP,
	TargetHSI,
	TargetDAL,
	TargetHMPDongle,
	TargetDiva_IP_HMP,
	TargetDivalog,

	TargetLast
} diva_section_target_t;

typedef enum _diva_ram_init_operation {
	RamInitOperationNotDefined = 0,

	RamInitOperationWrite,
	RamInitOperationSetBits,
	RamInitOperationClearBits,

	RamInitOperationLast
} diva_ram_init_operation_t;

typedef enum _diva_hot_update_operation_result {
	HotUpdateOK = 0,
	HotUpdateNoChangesDetected,
	HotUpdateFailedNotPossible,
	HotUpdateFailedOwnerNotFound,

	HotUpdateLastApplicationResponse,
	HotUpdatePending,
	HotUpdateInvalidApplicationResponse,

	HotUpdateLast
} diva_hot_update_operation_result_t;

typedef enum _diva_cfg_binding_direction {
	BindingDirectionIn = 0,
	BindingDirectionOut,
	BindingDirectionInAndOut,

	BindingDirectionLast
} diva_cfg_binding_direction_t;

typedef enum _diva_cfg_binding_enable {
	BindingEnable = 0,
	BindingDisable,

	BindingLast
} diva_cfg_binding_enable_t;

typedef enum _diva_cfg_binding_service_mask_bits {
	BindingServiceVoice    = 0,
	BindingServiceFax      = 1,
	BindingServiceModem    = 2,
	BindingServiceHDLC     = 3,
	BindingServiceX75      = 4,
	BindingServiceV120     = 5,
	BindingServiceV110     = 6,
	BindingServicePiafs    = 7,

	BindingServiceLast
} diva_cfg_binding_service_mask_bits_t;

typedef enum _diva_cfg_image_type {
	DivaImageTypeNone = 0,

	DivaImageTypeProtocol,
	DivaImageTypeDsp,
	DivaImageTypeFPGA,

	DivaImageTypeSDP1,
	DivaImageTypeSDP2,

	DivaImageTypeLast
} diva_cfg_image_type_t;

/*
	Functions used for processing of configuration
	update notifications
	*/
dword vle_to_dword   (const byte* vle, dword* pos);
dword get_max_length (const byte* src);
dword diva_get_bs  (const byte* src, pcbyte* dst);
const byte* diva_cfg_get_section_element (const byte* src, dword required_id);
struct _diva_vie_tags;
struct _diva_vie_tags* find_tag_from_id (dword id);
diva_vie_id_t diva_cfg_get_section_type (const byte* src);
const byte* diva_cfg_get_next_cfg_section (const byte* src, const byte* current_section);
int diva_cfg_get_image_info (const byte* src,
														 diva_cfg_image_type_t image_type,
														 pcbyte* image_name,
														 dword*  image_name_length,
														 pcbyte* archive_name,
														 dword*  archive_name_length);
const byte* diva_cfg_get_next_ram_init_section (const byte* src, const byte* section);
diva_ram_init_operation_t diva_cfg_get_ram_init_operation (const byte* src);
dword diva_cfg_get_ram_init_offset (const byte* src);
dword diva_cfg_get_ram_init_value (const byte* src, pcbyte* value);
const byte* diva_cfg_find_named_variable (const byte* src,
																					const byte* variable_name,
																					dword variable_name_length,
																					dword* data_length);
int diva_cfg_find_named_value (const byte* src,
															 const byte* variable_name,
															 dword variable_name_length,
															 dword* value);

#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
struct _diva_cfg_lib_instance;
typedef const byte* (*find_equal_cfg_section_proc_t)(const byte* ref_section, const byte* instance);
typedef int   (*cmp_cfg_sections_proc_t)(const byte* ref_section, const byte* org_section);
typedef dword (diva_cfg_input_proc_t)(struct _diva_cfg_lib_instance* pLib, struct _diva_vie_tags* tag);
struct _diva_vie_tags* diva_cfg_get_vie_attributes(void);
int diva_cfg_get_vie_attributes_length(void);
struct _diva_cfg_section* identify_section (diva_vie_id_t id);
byte* diva_cfg_translate_xdi_data (dword header_length, const byte* src, dword* data_length);
#endif

/*
	Description of configuration sections
	*/
typedef struct _diva_cfg_section {
	diva_vie_id_t  type;
	const char*    name;
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
	find_equal_cfg_section_proc_t find;
	cmp_cfg_sections_proc_t				cmp;
#endif
	diva_vie_id_t* required;
	diva_vie_id_t* optional;
} diva_cfg_section_t;

typedef struct _diva_vie_tags {
	const char* tag;
	diva_vie_id_t id;
	const char* attribute;
#ifdef __DIVA_CFGLIB_IMPLEMENTATION__
	diva_cfg_input_proc_t* read_proc;
	dword (*dump)(struct _diva_vie_tags* tag, byte* src);
#endif
	dword (*skip)(const byte* src, dword max_length);
	int is_read_vle_vie;
	int is_read_vle_bs;
} diva_vie_tags_t;


#endif

