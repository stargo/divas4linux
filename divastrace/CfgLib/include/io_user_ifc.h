
/*
 *
  Copyright (c) Dialogic(R), 2009-2014.
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

struct _diva_io_object_user_ifc;

typedef int (*diva_io_object_user_release_proc_t)(struct _diva_io_object_user_ifc* hIfc);
typedef DIVA_OS_HANDLE (*diva_io_object_user_get_handle_proc)(struct _diva_io_object_user_ifc* hIfc);
typedef int (*diva_io_object_user_completion_proc)(struct _diva_io_object_user_ifc* hIfc, int read, int write, int status, int error);
typedef int (*diva_io_object_user_write_proc)(struct _diva_io_object_user_ifc* hIfc, const void* data, int data_length);
typedef void (*diva_io_object_user_shutdown_socket_proc)(struct _diva_io_object_user_ifc* hIfc);
typedef void* (*diva_io_object_user_get_extended_interface_proc)(struct _diva_io_object_user_ifc* hIfc,
                                                                 int interface_nr, const void* data);


/*
	Abstraction for file descriptors and sockets to be used
	as abstraction for I/O multiplexing
	*/
typedef struct _diva_io_object_user_ifc {
	diva_entity_link_t link; /* used internally by enumerator */
	void* user_context; /* used internaly by module */

	/*
		Public interface functions, common for all types of
		objects
		*/
	diva_io_object_user_release_proc_t       release_proc;
	diva_io_object_user_get_handle_proc      read_handle_proc;
	diva_io_object_user_get_handle_proc      write_handle_proc;
	diva_io_object_user_get_handle_proc      state_change_proc;

	diva_io_object_user_completion_proc      message_input_proc;
	diva_io_object_user_write_proc           write_input_proc;
	diva_io_object_user_shutdown_socket_proc shutdown_socket_device;
	diva_io_object_user_get_extended_interface_proc get_extended_interface;
} diva_io_object_user_ifc_t;


struct _diva_io_object_user_enum_ifc;

typedef int (*diva_io_object_enum_release_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc);
typedef diva_io_object_user_ifc_t* (*diva_io_object_enum_get_object_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc, diva_io_object_user_ifc_t* ident);
typedef int (*diva_io_object_enum_nr_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc);
typedef int (*diva_io_object_enum_free_ifc_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc);
typedef int (*diva_io_object_enum_ipc_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc);
typedef int (*diva_io_object_enum_shutdown_proc_t)(struct _diva_io_object_user_enum_ifc* enum_ifc);


typedef struct _diva_io_object_user_enum_ifc {
	diva_entity_link_t link; /* used by interafce user */
	void* user_context;      /* used internally by module */
	diva_io_object_enum_release_proc_t release_proc;
	diva_io_object_enum_get_object_proc_t get;
	diva_io_object_enum_nr_proc_t nr;
	diva_io_object_enum_free_ifc_proc_t free_ifc_proc;
	diva_io_object_enum_ipc_proc_t ipc_proc;
	diva_io_object_enum_shutdown_proc_t start_shutdown;
	diva_io_object_enum_shutdown_proc_t shutdown_complete;
} diva_io_object_user_enum_ifc_t;

