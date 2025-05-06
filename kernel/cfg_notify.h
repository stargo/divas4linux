
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

#ifndef __DIVA_CFG_LIB_NOTIFY_INTERFACE__
#define __DIVA_CFG_LIB_NOTIFY_INTERFACE__

struct _diva_cfg_lib_access_ifc;

/*
	User provided callback function called every time
	configuration change was detected and from the context of
	"diva_cfg_lib_cfg_register_notify_callback" function
	INPUT:
		context  - user provided context
		instance - information about instance where configuration changfe apply
		owner    - information about owner where configuration change apply

		Only one of bot is set: or owner (change for all instances) or
		instance (change only for this instance).

		Normally this function is called from the context of
		"diva_cfg_lib_cfg_register_notify_callback" with "owner" to allow application
    to read they initial configuration.
		In case of later configuration change CfgLib will try to update configuration using
		the hot update procedure. In case this will fail then CfgLib will use
		this callback function with "instance" that points to data of instance changed.

	RETURN:
			zero - Application update completed
			-1   - Configuration update error
			 1   - Reserved for use by user mode applications
             (notification pending)
	*/
typedef int (*diva_cfg_lib_cfg_notify_callback_proc_t)(void* context,
																											 const byte* message,
																											 const byte* instance);

/*
	Register configuration change notification callback.
	Returns handle on success, zero on error
	*/
void* diva_cfg_lib_cfg_register_notify_callback (diva_cfg_lib_cfg_notify_callback_proc_t proc,
																								 void* context,
																								 dword owner);
/*
	Remove notification change notification callback using obtained by register of this callback
	handle.
	Returns zero on success and negative value on error
	*/
int   diva_cfg_lib_cfg_remove_notify_callback   (void* handle);

typedef enum _diva_cfg_lib_value_type {
	DivaCfgLibValueTypeBool,
	DivaCfgLibValueTypeSigned,
	DivaCfgLibValueTypeUnsigned,
	DivaCfgLibValueTypeBinaryString,
	DivaCfgLibValueTypeASCIIZ,

	DivaCfgLibValueTypeLast
} diva_cfg_lib_value_type_t;

typedef enum _diva_cfg_lib_return_type {
  DivaCfgLibParameter = -4,
  DivaCfgLibWrongType = -3,
  DivaCfgLibBufferTooSmall = -2,
  DivaCfgLibNotFound = -1,
	DivaCfgLibOK = 0,
} diva_cfg_lib_return_type_t;

typedef enum _diva_cfg_lib_notification_type {
	DivaCfgVariableChanged = 1,
	DivaCfgVariableRemoved = 2,

	DivaCfgNotifyLast
} diva_cfg_lib_notification_type_t;

typedef struct _diva_cfg_lib_variable_change_context {
	int instance_by_name;

	const byte* instance_ident;
	dword instance_ident_length;

	dword instance_nr;

	const byte* variable;

} diva_cfg_lib_variable_change_context_t;

typedef struct _diva_cfg_lib_notification_context {
	diva_cfg_lib_notification_type_t notification_type;
	union {
		diva_cfg_lib_variable_change_context_t variable_changed;
	} info;
} diva_cfg_lib_notification_context_t;

typedef struct _diva_cfg_lib_access_ifc {
	int   ifc_length;  /* R0, set by CFGLib */
	int   ifc_version; /* R0, set by CFGLib */
	const byte* storage;     /* RO, set by CFGLib */
	void* cfg_lib_instance_context; /* RO, used by CFGLib */

/*
  Free received copy of configuration interface
  */
	void (*diva_cfg_lib_free_cfg_interface_proc)(struct _diva_cfg_lib_access_ifc* ifc);
  /*
    RE, Set by CFGLib. Used to retrieve configuration variable
    from CFGLib storage.
    */
	diva_cfg_lib_return_type_t
	(*diva_cfg_storage_read_cfg_var_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																				diva_section_target_t owner,
																				dword instance_by_nr,
																				byte* instance_by_name,
																				int   instance_by_name_length,
																				const char* name,
																				diva_cfg_lib_value_type_t value_type,
																				dword* data_length,
																				void* pdata);
  /*
    RE, Set by CFGLib. Used to register configuration change notification
    Used by applications und drivers which do not support management interface
    */
	void* (*diva_cfg_lib_cfg_register_notify_callback_proc)(diva_cfg_lib_cfg_notify_callback_proc_t proc,
																													void* context,
																													dword owner);
  /*
    RE, remove configuration change notification
    */
	int (*diva_cfg_lib_cfg_remove_notify_callback_proc)(void* handle);

  /*
    RE, used to retrieve information about configuration variable
        from configuration change notification
    */
	diva_cfg_lib_return_type_t
	(*diva_cfg_storage_read_instance_cfg_var_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																								 const byte* instance_data,
																								 const char* name,
																								 diva_cfg_lib_value_type_t value_type,
																								 dword* data_length,
																								 void* pdata);
	/*
		RE, retrieve information about image
		*/
	diva_cfg_lib_return_type_t
	(*diva_cfg_storage_get_image_info_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																					const byte* instance_data,
																					diva_cfg_image_type_t requested_image_type,
																					pcbyte* image_name,
																					dword*  image_name_length,
																					pcbyte* archive_name,
																					dword*  archive_name_length);
	/*
		RE, retrieve information about adapter configuration
		*/
	diva_cfg_lib_return_type_t
	(*diva_cfg_storage_get_adapter_cfg_info_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																								const   byte* instance_data,
																								dword   offset_after,
																								dword*  ram_offset,
																								pcbyte* data,
																								dword*  data_length);


	/*
		Enumerate all variables which math specified template
		*/
	const byte* (*diva_cfg_storage_enum_variable_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																										 const byte* instance_data,
																										 const byte* current_var,
																										 const byte* _template,
																										 dword template_length,
																										 pcbyte* key,
																										 dword* key_length,
																										 pcbyte* name,
																										 dword*  name_length);

	/*
		Find variable with specified name
		*/
	const byte* (*diva_cfg_storage_find_variable_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																										 const byte* instance_data,
																										 const byte* name,
																										 dword name_length);

	/*
		Read char value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_char_value_proc)(const byte* variable,
																																	char* value);

	/*
		Read byte value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_byte_value_proc)(const byte* variable,
																																	byte* value);

	/*
		Read short value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_short_value_proc)(const byte* variable,
																																	 short* value);

	/*
		Read word value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_word_value_proc)(const byte* variable,
																																	word* value);

	/*
		Read int value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_int_value_proc)(const byte* variable,
																																 int* value);

	/*
		Read dword value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_dword_value_proc)(const byte* variable,
																																	 dword* value);

	/*
		Read 64bit value
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_64bit_value_proc)(const byte* variable,
																																	 void* value);

	/*
		Read binary string
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_binary_string_proc)(const byte* variable,
																																		 void* value,
																																		 dword* length,
																																		 dword max_length);

	/*
		Read ascii zero terminated string
		Max length includes terminating zero
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_read_asciiz_string_proc)(const byte* variable,
																																		 char* value,
																																		 dword max_length);

	/*
		Allows access to variable data
		*/
	const byte*	(*diva_cfg_lib_get_variable_data_proc)(const byte* variable, dword* length);

	/*
		Provides access to instance data
		*/
	const byte* (*diva_cfg_lib_get_instance_proc)(struct _diva_cfg_lib_access_ifc* ifc,
																								diva_section_target_t owner,
																								int instance_by_name,
																								const byte* instance_ident,
																								dword instance_ident_length,
																								dword instance_nr);

	/*
		Release instance data
		*/
	void (*diva_cfg_lib_release_instance_proc)(struct _diva_cfg_lib_access_ifc* ifc, const byte* instance);

	/*
		Retrieve information about section ident
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_get_instance_ident_proc)(const byte* instance,
																																		 diva_vie_id_t* ident_type,
																																		 pcbyte* ident,
																																		 dword*  ident_length,
																																		 dword*  ident_nr);

	/*
		Retrieve information about variable ident
		*/
	diva_cfg_lib_return_type_t (*diva_cfg_lib_get_name_ident_proc)(const byte* variable,
																																 pcbyte* ident,
																																 dword*  ident_length);

	/*
		Copy instance handle
		*/
	const byte* (*diva_cfg_lib_copy_instance_proc)(const byte* instance);

} diva_cfg_lib_access_ifc_t;

/*
  Receive copy of configuration interface from CFGLib
  */
void* diva_cfg_lib_get_cfg_interface(void);

#endif

