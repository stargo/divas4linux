
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

#if !defined(LINUX) /* { */

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <process.h>
#include <tchar.h>
#include <signal.h>

#include "dievent.h"
#else /* } { */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mount.h>

#endif /* } */

#include "platform.h"
#include "os.h"
#include "idi_defs.h"
#include "st_ifc.h"
#include "dlist.h"

#ifdef BOARDCONF
#include "io_user_ifc.h"
#include "cfg_ifc.h"
#endif // BOARDCONF

#include "event.h"
#include "usage.h"
#include "diva_log.h"
#include "record.h"

#ifdef BOARDCONF
#include "debug.h"
#include "divatimer.h"
#include "cfg_types.h"
#include "cfg_notify.h"
#endif // BOARDCONF

/*
	LOCALS
	*/

#define DIVA_SUSPEND_TO   10
#define DIVA_DETECTION_TO 10
#define DIVA_STAT_UPDATE_TO 10
#define DIVA_ERROR_TO (22 /* driver to value + 2 Sec */ + (DIVA_STAT_UPDATE_TO)*2)

static int strace_main (HANDLE hStop, user_context_t* user_context);
static void remove_suspended (diva_entity_queue_t* q, time_t to);
static void insert_new (diva_entity_queue_t* q, user_context_t* user_context);
static int create_map (user_context_t* user_context, diva_adapter_t** adapters, HANDLE* handles);
static void suspend_adapters (diva_entity_queue_t* q);
static int move_log_files_to (int init);
static const char* GetLogFileName (int include_extension);
static dword get_last_log_file_index (void);
static void diva_local_error_proc (void* user_context,
                          			 diva_strace_library_interface_t* hLib,
																 int Adapter,
																 int error,
																 const char* file,
																 int line);
static void divalog_main (void);
static void divalog_consolestart(void);
static void divalog_hup(int signo);
#if !defined(LINUX)
static void WINAPI service_main(DWORD dwArgc, LPTSTR *lpszArgv);
static void WINAPI service_ctrl(DWORD dwCtrlCode);
static void CmdInstallService (void);
static void CmdStartService (void);
static void CmdStopService (void);
static void CmdRemoveService (void);
static LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize);
static int diva_get_config_from_registry (void);
#else
#define REG_SZ 0
#define ERROR_SUCCESS 0
#define HKEY_LOCAL_MACHINE 0

typedef struct _appl_info {
	struct _diva_cfg_lib_access_ifc* cfg_lib_ifc;
} appl_info_t;

typedef struct _registry_key {
	diva_entity_link_t link;
	HKEY               key;
	char               path[MAX_PATH+1];
} registry_key;

diva_entity_queue_t registry_keys;

long RegCreateBoardDir(HKEY hKey,char* lpSubKey,DWORD Reserved,LPTSTR lpClass,char* phkResult,DWORD* lpdwDisposition);
long RegCreateKeyEx(HKEY hKey,char* lpSubKey,DWORD Reserved,LPTSTR lpClass,HKEY* phkResult,DWORD* lpdwDisposition);
long RegSetValueEx(HKEY hKey,const char* lpValueName,DWORD Reserved,DWORD dwType,const char* lpData,DWORD cbData);
long RegDeleteValue(HKEY hKey,const char* lpValueName);
long RegCloseKey(HKEY hKey);
long RegDeleteKey(HKEY hKey,char* lpSubKey);
#endif
static int diva_create_registry_adapter_tree (diva_adapter_t* pA);
static int diva_destroy_registry_adapter_tree (diva_adapter_t* pA);
static int write_ascii_reg_value (HKEY key, const char* name, const char* data);
static void AddToMessageLog (LPTSTR lpszMsg);
static BOOL ReportStatusToSCMgr (DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
static int diva_create_adapter_directory (diva_adapter_t* pA);
#if defined(BOARDCONF)
static int write_ascii_value(key_ele_t *ConfKey, const char* ps, const char* ie_buffer);
static int del_var(char* dir_path, var_list_ele_t* list_ele);
static var_list_ele_t*  del_untouched_var(char* path, var_list_ele_t* list);
static int del_list_ele(var_list_ele_t* list_ele);
static int del_whole_list(char* path, var_list_ele_t* list_head);
static int del_port(int board_id, int port_id);
static diva_conf_t* del_board(int board_id);
static void  inc_update_count(void);
static void  del_untouched(void);
static void	ini_update_count(void);
#endif // BOARDCONF
static HANDLE hStopEvent = (int*)INVALID_HANDLE_VALUE;
#if !defined(LINUX)
static SERVICE_STATUS ssStatus;
static SERVICE_STATUS_HANDLE sshStatusHandle;
static DWORD dwErr = 0;
static TCHAR szErr[256];
static char path_separator[SEP_SIZE] = "\\";
static char log_file_path[MAX_PATH+1] = "%ProgramFiles%\\Diva Server";
static char log_file_extension [64] = ".csv";
#else
static char path_separator[SEP_SIZE] = "/";
static char log_file_path[MAX_PATH+1] = DEFAULT_LOG_FILE_LOG;
static int create_pid_file (int create);
static char log_file_extension[1] = "";
#endif
static int divalog_debug    = 0;
static int show_line_state  = 1;
static int max_log_segments = 10; /* amount of backlog *.N files */

#if defined(BOARDCONF)	
#define BOARDNAME_LEN 8
#define PORTPATH_LEN 14
#define PORTNAME_LEN 5
#define MAX_NR_BOARDS 32
#define MAX_NR_PORTS 8
static diva_conf_t* board_conf[MAX_NR_BOARDS];
static file_cont_t update_count;
#endif // BOARDCONF	

/*
	Local service definitions
	*/
#define SZAPPNAME "Divalog"
#define SZSERVICENAME        "DialogicDivaLogService"
#define SZSERVICEDISPLAYNAME "Dialogic Diva Log Service"
#define SZDEPENDENCIES       "DiMaint\0\0\0"
char * szServiceDescription = "Logging of calls and events for Dialogic Diva Boards";

#if !defined(LINUX)
#define DIVA_PUBLIC_DIALOGIC_REGISTRY_ROOT "SOFTWARE\\Eicon"
#define DIVA_PUBLIC_IFC_REGISTRY_ROOT DIVA_PUBLIC_DIALOGIC_REGISTRY_ROOT"\\DivaLogService"
#define DIVA_PUBLIC_IFC_BASE DIVA_PUBLIC_IFC_REGISTRY_ROOT"\\CurrentVersion\\ifc"
#define DIVA_PUBLIC_HW_BASE DIVA_PUBLIC_IFC_REGISTRY_ROOT"\\CurrentVersion\\hw"
#define DIVA_PUBLIC_ADAPTER_IFC_BASE DIVA_PUBLIC_IFC_BASE"\\adapter"
#else
static int enforce_adapter_status    = 0;
static char registry_path[MAX_PATH+1] = "/usr/lib/divas/registry";
#define DIVA_PUBLIC_IFC_REGISTRY_ROOT ""
#define DIVA_PUBLIC_IFC_BASE DIVA_PUBLIC_IFC_REGISTRY_ROOT"/ifc"
#define DIVA_PUBLIC_HW_BASE DIVA_PUBLIC_IFC_REGISTRY_ROOT"/hw"
#define DIVA_PUBLIC_BOARD_BASE DIVA_PUBLIC_IFC_REGISTRY_ROOT"/boardconf"
#define DIVA_PUBLIC_ADAPTER_IFC_BASE DIVA_PUBLIC_IFC_BASE"/adapter"
#define DIVA_UPDATE_COUNT_BASE DIVA_PUBLIC_BOARD_BASE"/update_count"
#endif

#define DIVA_CONFIG_REGISTRY_ROOT "SOFTWARE\\ISDN\\{EB50EDFF-DB3F-4c88-804F-0F2689A7D3E9}"
#define DIVA_CONFIG_REGISTRY_PATH DIVA_CONFIG_REGISTRY_ROOT"\\Divalog"



#define DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME "system"
#define DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME_SIZE (sizeof(DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME)-1)

#define DIVA_CFG_LIB_DIVALOG_SYSTEM_VAR_TIME_INFO_SYNC_NAME "timeinfosync"
#define DIVA_CFG_LIB_DIVALOG_SYSTEM_VAR_TIME_INFO_SYNC_NAME_SIZE (sizeof(DIVA_CFG_LIB_DIVALOG_SYSTEM_VAR_TIME_INFO_SYNC_NAME)-1)
extern int diva_os_time_info_cfg_sync;

#if !defined(LINUX) /* { */
void _cdecl
#else /* } { */
int
#endif /* } */
main(int argc, char **argv) {
#if !defined(LINUX) /* { */
	SERVICE_TABLE_ENTRY dispatchTable[] = {
		{ TEXT(SZSERVICENAME), (LPSERVICE_MAIN_FUNCTION)service_main },
		{ NULL, NULL }
	};

	if ( (argc > 1) && ((*argv[1] == '-') || (*argv[1] == '/')) ) {
		if ( _stricmp( "install", argv[1]+1 ) == 0 ) {
			CmdInstallService();
			exit(0);
		} else if ( _stricmp( "start", argv[1]+1 ) == 0 ) {
			CmdStartService();
			exit(0);
		} else if ( _stricmp( "stop", argv[1]+1 ) == 0 ) {
			CmdStopService();
			exit(0);
		} else if ( _stricmp( "remove", argv[1]+1 ) == 0 ) {
			CmdRemoveService();
			exit(0);
		} else if ( _stricmp( "debug", argv[1]+1 ) == 0 ) {
			divalog_consolestart();
			exit(0);
		}
	}

	if (!StartServiceCtrlDispatcher(dispatchTable)) {
		AddToMessageLog (TEXT("StartServiceCtrlDispatcher failed."));
	}
#else /* } { */
	int i, start_debug = 0;

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '-' || *argv[i] == '/') {
			if (strcasecmp ("nolinestate", argv[i]+1) == 0) {
				show_line_state = 0;
			} else if(strcasecmp ("debug", argv[i]+1) == 0) {
				start_debug = 1;
			} else if(strcasecmp ("enforce_adapter_status", argv[i]+1) == 0) {
				enforce_adapter_status = 1;
			} else if(strncasecmp ("registry_path=", argv[i]+1, strlen("registry_path=")) == 0) {
				strncpy(registry_path, argv[i]+1+strlen("registry_path="), MAX_PATH);
			} else if(strncasecmp ("logfile_path=", argv[i]+1, strlen("logfile_path=")) == 0) {
				strncpy(log_file_path, argv[i]+1+strlen("logfile_path="), MAX_PATH);
			} else if(strncasecmp ("max_log_segments=", argv[i]+1, strlen("max_log_segments=")) == 0) {
			  max_log_segments = atoi(&argv[i][strlen("max_log_segments=")+1]);
			}	
		}
	}

	if (start_debug) {
		if (create_pid_file (1)) {
			printf ("%s\n", "ERROR: failed to create pid file");
		}
		divalog_consolestart();
		create_pid_file (0);
	} else {
		openlog ("divalogd", LOG_CONS | LOG_NDELAY | LOG_PID, LOG_DAEMON);
		if (daemon (0,0)) {
			syslog (LOG_ERR, " %s", "failed to start");
			closelog();
			return (1);
		}
		if (create_pid_file (1)) {
			syslog (LOG_ERR, " %s", "failed to create pid file");
			closelog();
			return (1);
		}

		syslog (LOG_NOTICE, " %s", "started");

		divalog_main ();

		create_pid_file (0);
		syslog (LOG_NOTICE, " %s", "terminated");
		closelog();
	}

	return (0);
#endif /* } */
}

#if !defined(LINUX)
static void WINAPI service_main(DWORD argc, LPTSTR *argv) {
	if (!(sshStatusHandle = RegisterServiceCtrlHandler( TEXT(SZSERVICENAME), service_ctrl))) {
		/*
			Failed
			*/
		AddToMessageLog ("Failed to register service control routine");
		return;
	}
	ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ssStatus.dwServiceSpecificExitCode = 0;

	if (!ReportStatusToSCMgr (SERVICE_START_PENDING, NO_ERROR, 3000)) {
		AddToMessageLog ("Failed to mark service pending");
		(void)ReportStatusToSCMgr (SERVICE_STOPPED, dwErr, 0);
		return;
	}

	divalog_main();

	(void)ReportStatusToSCMgr (SERVICE_STOPPED, 0, 0);
}

static void WINAPI service_ctrl (DWORD dwCtrlCode) {
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
			ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 0);
			if (hStopEvent != INVALID_HANDLE_VALUE) {
				SetEvent (hStopEvent);
			}
			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;

	}

	ReportStatusToSCMgr(ssStatus.dwCurrentState, NO_ERROR, 0);
}
#endif

/*
   command line main() for -debug,
   fr
  */

static void divalog_consolestart(void) {
	divalog_debug = 1;
#if !defined(LINUX) /* { */
	if (signal(SIGINT, divalog_hup) == SIG_ERR) {
		printf("divalogd ERROR: could not set SIGINT\n");
		exit(1);
	}
#endif /* } */
	printf("Starting divalog in DEBUG mode...\n");
	divalog_main ();
	printf("Exit debug mode ...\n");
}

static void divalog_hup(int signo) {
#if !defined(LINUX)
	if (signo == SIGINT) {
signal(SIGINT, SIG_DFL); // restore signal handler
		SetEvent (hStopEvent);   // stop main loop
	}
#else
	*hStopEvent = 1;
#endif
}

/*
	MAIN
	*/
static void divalog_main (void) {
	user_context_t user_context;
	int ret;

#if defined(LINUX)
	signal (SIGHUP,  divalog_hup);
	signal (SIGTERM, divalog_hup);
	signal (SIGABRT, divalog_hup);
	signal (SIGQUIT, divalog_hup);
	signal (SIGINT,  divalog_hup);

	diva_q_init(&registry_keys);

	if ((ret = umount2 (registry_path, 0x00000002 /* MNT_DETACH, "linus/fs.h" */))) {
		    if (divalog_debug) printf("umount2 failed: %d\n", ret);
	}
	if ((ret = umount (registry_path))) {
		if (divalog_debug) printf("umount failed: %d\n", ret);
	}
	rmdir (registry_path);
	mkdir (registry_path, 0700);
	if (mount ("/dev/shm", registry_path, "tmpfs", 0, 0)) {
		if (enforce_adapter_status) {
			syslog (LOG_NOTICE, " %s", "Can't mount(/dev/shm, tmpfs), using hard disc instead");
		} else {
			syslog (LOG_ERR, " %s", "Can't mount(/dev/shm, tmpfs)");
			*registry_path = 0;
		}
	}

#endif 

#if !defined(LINUX)
	ret = diva_get_config_from_registry();
	if (ret != 0 && divalog_debug) printf("Error: could not access configuration, using defaults\n");
#endif
	if (divalog_debug) printf("Using log_file_path %s and log_file_count %d\n\n",log_file_path,max_log_segments);

	/*
		Clean-up public interface
		*/
	for (ret = 1; ret < DIVA_MAX_ADAPTERS; ret++) {
		diva_adapter_t adapter;
		memset (&adapter, 0x00, sizeof(adapter));
		adapter.adapter_nr = ret;
		diva_create_adapter_directory (&adapter);
	}

	if (!divalog_debug && !ReportStatusToSCMgr (SERVICE_START_PENDING, NO_ERROR, 3000)) {
		return;
	}
	memset (&user_context, 0x00, sizeof(user_context));

	if (strlen(log_file_path)) {
		if (move_log_files_to (1)) {
			/*
			Can't rotate log files
			*/
			if (divalog_debug) printf("Error: cannot rotate log files. Another instance running?\n");
			return;
		}
		if (!(user_context.Log[0] = fopen (GetLogFileName(1), "w"))) {
		/*
			Can't open log file
			*/
			if (divalog_debug) printf("Error: cannot open log file. Another instance running?\n");
			return;
		}
		if (diva_write_log_header (user_context.Log)) {
		/*
			Can't write Log file header
			*/
			if (divalog_debug) printf("Error: cannot write log header. Another instance running?\n");
			fclose (user_context.Log[0]);
			return;
		}
	} else {
		user_context.Log[0] = 0;
	}

	if ((hStopEvent = CreateEvent (0, TRUE, FALSE, 0)) == (int*)INVALID_HANDLE_VALUE) {
		if (user_context.Log[0]) {
			fclose (user_context.Log[0]);
		}
		return;
	}
#if defined(LINUX)
	*hStopEvent = 0;
#endif

	if (!divalog_debug && !ReportStatusToSCMgr (SERVICE_RUNNING, NO_ERROR, 0)) {
		CloseHandle (hStopEvent);
		hStopEvent = (int*)INVALID_HANDLE_VALUE;
		if (user_context.Log[0]) {
			diva_write_log_suffix (user_context.Log);
			fclose (user_context.Log[0]);
		}
		return;
	}

	ret = strace_main (hStopEvent, &user_context);

#if defined(LINUX)
	signal (SIGHUP,  SIG_DFL);
	signal (SIGTERM, SIG_DFL);
	signal (SIGABRT, SIG_DFL);
	signal (SIGQUIT, SIG_DFL);
	signal (SIGINT,  SIG_DFL);

	if (*registry_path) {
		if (divalog_debug) printf("Cleanup\n");
		if ((ret = umount2 (registry_path, 0x00000002 /* MNT_DETACH, "linus/fs.h" */))) {
			if (divalog_debug) printf("umount2 failed: %d\n", ret);
		}
		if ((ret = umount (registry_path))) {
			if (divalog_debug) printf("umount failed: %d\n", ret);
		}
		rmdir (registry_path);
	}

#endif

	if (user_context.Log[0]) {
		diva_write_log_suffix (user_context.Log);
		fclose (user_context.Log[0]);
	}

	CloseHandle (hStopEvent);
}

static void remove_suspended (diva_entity_queue_t* q, time_t to) {
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_get_head (q);
	diva_adapter_t* cur;

	while (pA) {
		cur = pA;
		pA = (diva_adapter_t*)diva_q_get_next(&pA->link);

		if (!cur->hLib && ((to - cur->suspended) >= DIVA_SUSPEND_TO)) {

		if (divalog_debug) printf("remove suspended: delete adapter nr %d from list\n",cur->adapter_nr);

			diva_q_remove (q, &cur->link);

			HeapFree (GetProcessHeap(), 0, cur);
		}

	}
}


static int cmp_adapter_nr (const void* what, const diva_entity_link_t* p) {
	diva_adapter_t* pA = (diva_adapter_t*)p;
	int nr = *(const int*)what;

	return (nr != pA->adapter_nr);
}

/*
	Look for new adapters

	In case of failure no error is reported - detection
	procedure runs periodically.
	*/
static void insert_new (diva_entity_queue_t* q, user_context_t* user_context) {
	int count = diva_q_get_nr_of_entries (q), i;
	const dword* adapter_list = DivaSTraceLibraryGetDescriptorList ();
	dword adapter_index;
	diva_adapter_t* pA;

	if (divalog_debug) {
		if (adapter_list == 0) {
			printf("failed to get adapter list\n");
		} else if (*adapter_list == 0) {
			printf("no adapter found in adapter list\n");
			} else {
				printf ("Found %d adapters: ", adapter_list[0]);
				for (adapter_index = 0; adapter_index < adapter_list[0]; adapter_index++)
				printf (" %d", adapter_list[adapter_index+1]);
				printf ("\n");
			}
	}

	for (adapter_index = 0;
			 (adapter_list != 0 && adapter_index < adapter_list[0] && count < DIVA_SUPPORTED_ADAPTERS);
				adapter_index++) {
		i = (int)adapter_list[adapter_index+1];
		if (!(pA = (diva_adapter_t*) diva_q_find (q, &i, cmp_adapter_nr))) {
			pA = (diva_adapter_t*)HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pA));
			if (pA) {
				pA->user.user_context      = user_context;
				pA->user.notify_proc       = diva_log_notify_proc;
				pA->user.trace_proc        = diva_log_trace_proc;
				pA->user.error_notify_proc = diva_local_error_proc;
				pA->adapter_nr             = i;
				if ((pA->hLib = DivaSTraceLibraryCreateInstance (i, &pA->user))) {
					(*(pA->hLib->DivaSTraceInterfaceDisableTrace))(pA->hLib->hLib);
					if ((*(pA->hLib->DivaSTraceInterfaceGetNrChannels)) (pA->hLib->hLib) > 8) {
						(*(pA->hLib->DivaSTraceSetInterfaceFeatures))(pA->hLib->hLib,
					                                              DivaSTraceFeatureGlobalStatisticsPolling);
						pA->polling_mode = 1;
					} else {
						pA->polling_mode = 0;
					}
					pA->timestamp = time(0);
					pA->stat_update = time(0) - DIVA_STAT_UPDATE_TO; /* schedule statistics update immediately after insertion */
					diva_q_add_tail (q, &pA->link);
					count = diva_q_get_nr_of_entries (q);
					if (divalog_debug) printf("insert new: found new adapter nr %d (table size now %d)\n",i,count);
					else syslog(LOG_ERR, "Diva Media Board %d (%s) status reporting enabled", pA->adapter_nr, pA->hLib->adapter_name);
				} else {
					HeapFree (GetProcessHeap(), 0, pA);
				}
			}
		} else {
			/* adapter already in list */
			if (divalog_debug) printf("insert new: already in list %d, init state: %d\n",i,pA->diva_strace_initialised);
			if (pA->diva_strace_initialised==1) {
				diva_create_adapter_directory (pA);
				pA->diva_strace_initialised++;
			}
		}
	}
	/* insert softip service */
	i=1007;
	if (!(pA = (diva_adapter_t*) diva_q_find (q, &i, cmp_adapter_nr))) {
		pA = (diva_adapter_t*)HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pA));
		if (pA) {
			pA->user.user_context      = user_context;
			pA->user.notify_proc       = diva_log_notify_proc;
			pA->user.trace_proc        = diva_log_trace_proc;
			pA->user.error_notify_proc = diva_local_error_proc;
			pA->adapter_nr             = i;
			pA->adapter_type					 = DIVA_ADAPTER_TYPE_SOFTIP_SRV;
			if ((pA->hLib = (diva_strace_library_interface_t*)DivaSTraceLibrarySipCreateInstance (&pA->user))) {
				pA->timestamp = time(0);
				diva_q_add_tail (q, &pA->link);
				pA->diva_strace_initialised=2;
				count = diva_q_get_nr_of_entries (q);
				if (divalog_debug) printf("insert new: found new service nr %d (table size now %d)\n",i,count);
			} else {
				HeapFree (GetProcessHeap(), 0, pA);
			}
		} else {
			/* error heapalloc */
		}
	} else {
		  if (divalog_debug) printf("insert new: already in list %d\n",i);
	}
}

static int create_map (user_context_t* user_context, diva_adapter_t** adapters, HANDLE* handles) {
	diva_entity_queue_t* q = (diva_entity_queue_t*)user_context->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_get_head (q);
	diva_log_softip_map_t* sa = &user_context->softip_adapters;
	int nr = 0, softip_nr = 0;
	sa->first_softip_adapter = 0;

	while (pA) {
		if(pA->adapter_type !=DIVA_ADAPTER_TYPE_HM) {
			if (pA->hLib &&
			    ((handles[nr]=(HANDLE)(*(pA->hLib->DivaSTraceGetHandle))(pA->hLib->hLib)) != DIVA_OS_INVALID_HANDLE)) {
				adapters[nr] = pA;
				nr++;
				if ( pA->adapter_type==DIVA_ADAPTER_TYPE_SOFTIP ) {
					sa->softip_adapter_map[pA->adapter_nr]=++softip_nr;
					if ( (sa->first_softip_adapter == 0) || (sa->first_softip_adapter > pA->adapter_nr) ) {
						sa->first_softip_adapter = pA->adapter_nr;
					}
				}
			}
    		}
		pA = (diva_adapter_t*)diva_q_get_next(&pA->link);
	}
	return (nr);
}

/*
	After error every adapter is suspended for DIVA_SUSPEND_TO
	amount of time.
	*/
static void suspend_adapters (diva_entity_queue_t* q) {
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_get_head (q);
	user_context_t user_context;

	user_context.Log[0] = 0;
	user_context.Log[1] = 0;
	user_context.adapters = q;

	while (pA) {
		if (pA->hLib && pA->error) {
			if (divalog_debug) printf("ERROR: suspend adapter %d \n",pA->adapter_nr);
			if (IDI_IS_ADAPTER_ID(pA->adapter_nr)) {
				diva_log_cleanup_adapter (&user_context, pA->adapter_nr);
			}
			(*(pA->hLib->DivaSTraceLibraryFinit))(pA->hLib->hLib);
			diva_log_cleanup_adapter_strace(pA->adapter_nr);
			pA->hLib = 0;
			pA->suspended = time(0);
			pA->error = 0;
			pA->diva_strace_initialised = 0;
			if (IDI_IS_ADAPTER_ID(pA->adapter_nr)) {
				diva_create_adapter_directory (pA);
			}
		}
		pA = (diva_adapter_t*)diva_q_get_next(&pA->link);
	}
}

static void remove_all (diva_entity_queue_t* q) {
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_get_head (q);

	while (pA) {
		if (pA->hLib) {
			pA->error = 1;
			(*(pA->hLib->DivaSTraceLibraryFinit))(pA->hLib->hLib);
			diva_log_cleanup_adapter_strace(pA->adapter_nr);
			pA->hLib = 0;
			diva_create_adapter_directory (pA);
		}
		diva_q_remove (q, &pA->link);
		HeapFree (GetProcessHeap(), 0, pA);
		pA = (diva_adapter_t*)diva_q_get_head (q);
	}
}
	
#ifdef BOARDCONF	

static int diva_modules_user_ipc_proc (void* context, int mode) {
	return (0);
}

static void diva_modules_error_proc (const char* fmt, ...) {}


static void error_proc (const char* fmt, ...) {
	dbg_msg(DBG_ERROR, "error");
}
static int del_var(char *dir_path, var_list_ele_t * list_ele){
	char path[MAX_PATH+1];

	sprintf(path, "%s%s%s", dir_path, path_separator, list_ele->name);
	DeleteFile(path);
	return(ERROR_SUCCESS);

}
static var_list_ele_t *  del_untouched_var(char* path, var_list_ele_t * list){
	var_list_ele_t * tmp_list_ele;
	var_list_ele_t * tmp_list_ele_head;
	var_list_ele_t * vir_list_ele;
	
	tmp_list_ele_head=list;
	tmp_list_ele=list;

	for(tmp_list_ele=list; tmp_list_ele; tmp_list_ele=tmp_list_ele->next){
		if(tmp_list_ele->touch==0){
			while(tmp_list_ele->touch==0){
				del_var(path, tmp_list_ele);
				if(tmp_list_ele->prev)
					tmp_list_ele->prev->next=tmp_list_ele->next;
				else{
					tmp_list_ele_head=tmp_list_ele->next;
				}
				if(tmp_list_ele->next)
					tmp_list_ele->next->prev=tmp_list_ele->prev;
				vir_list_ele=tmp_list_ele->next;
				HeapFree (GetProcessHeap(), 0, tmp_list_ele);
				tmp_list_ele=vir_list_ele;
				if(tmp_list_ele==0)
				  break;
		}
		    
	}
		if(tmp_list_ele)
			tmp_list_ele->touch=0;
		else
			break;
	}
	return(tmp_list_ele_head);
}

static int del_list_ele(var_list_ele_t * list_ele){
	var_list_ele_t * list_ele_del;

	list_ele_del = list_ele;
	if(list_ele_del->prev)
		list_ele_del->prev->next=list_ele_del->next;
	if(list_ele_del->next)
		list_ele_del->next->prev=list_ele_del->prev;

	HeapFree (GetProcessHeap(), 0, list_ele_del);

	return(0);
}

static int del_whole_list(char* path, var_list_ele_t * list_head){
	var_list_ele_t * list_ele;


	while(list_head)
	{
		list_ele=list_head;
		list_head=list_ele->next;

		del_var(path, list_ele);
		del_list_ele(list_ele);
	}
	return(0);
}


static int del_port(int board_id, int port_id){
	board_conf[board_id]->PortKey[port_id].list_head=(var_list_ele_t *)del_whole_list(board_conf[board_id]->PortKey[port_id].path, board_conf[board_id]->PortKey[port_id].list_head);
	rmdir(board_conf[board_id]->PortKey[port_id].path);
	*board_conf[board_id]->PortKey[port_id].path=0;
	return(0);
}

static diva_conf_t* del_board(int board_id){
 	int i;

	for(i=0; i< MAX_NR_PORTS; i++){
		if(*(board_conf[board_id]->PortKey[i].path) != 0){
			del_port(board_id, i);
		}
	}
	board_conf[board_id]->ConfKey.list_head=(var_list_ele_t *)del_whole_list(board_conf[board_id]->ConfKey.path, board_conf[board_id]->ConfKey.list_head);
    rmdir(board_conf[board_id]->ConfKey.path);
	*(board_conf[board_id]->ConfKey.path)=0;
	HeapFree (GetProcessHeap(), 0, board_conf[board_id]);
	return(NULL);
}

static void  inc_update_count(void){
	char ie_buffer[16];
	FILE* fs;

	update_count.value += 1;

	snprintf(ie_buffer, sizeof(ie_buffer), "%x", update_count.value);
	ie_buffer[sizeof(ie_buffer)-1]=0;
				                              
	if ((fs = fopen (update_count.name, "w"))) {
		fprintf (fs, "%s\n", ie_buffer);
		fflush (fs);
		fclose (fs);
	}  
}

static void  del_untouched(void){
	int i, n;
	
	for(i=0; i < MAX_NR_BOARDS; i++){
		if(board_conf[i]!=0){
			if(board_conf[i]->ConfKey.touch==0 && *(board_conf[i]->ConfKey.path)!=0){
				board_conf[i]=del_board(i);
				continue;
			}
			else{
				board_conf[i]->ConfKey.list_head=del_untouched_var(board_conf[i]->ConfKey.path, board_conf[i]->ConfKey.list_head);
				for(n=0; n < MAX_NR_PORTS; n++){
					if(board_conf[i]->PortKey[n].touch==0&& *(board_conf[i]->PortKey[n].path)!=0){
						if(board_conf[i]->PortKey[n].path != NULL){
							del_port(i, n);
						}
					}
					else{
						board_conf[i]->PortKey[n].list_head=del_untouched_var(board_conf[i]->PortKey[n].path, board_conf[i]->PortKey[n].list_head);
					}
					board_conf[i]->PortKey[n].touch=0;
				}
			}
			board_conf[i]->ConfKey.touch=0;
		}
	}
	/* counter hochzählen */
}

static int diva_cfg_lib_cfg_notify_callback (void* context, const byte* message, const byte* instance) {
	appl_info_t* info = (appl_info_t*)context;
	diva_cfg_lib_return_type_t ret;
	dword nr;
	diva_vie_id_t instance_name_type;
	dword instance_name_length;
	const byte* instance_name;
	const byte* var = 0;
	pcbyte var_name;
	dword var_name_length;
	const byte* var_data;
	dword var_data_length;
	int   port_id=0;
	
	dbg_msg (DBG_INFO, "---------------------------------------------------");
	if (message == 0 && instance == 0) {
		dbg_msg (DBG_ERROR, "error at %d", __LINE__);
		return (0);
	}
		            
		            
	if (message != 0 && instance == 0) {
		const diva_cfg_lib_notification_context_t* nfy = (diva_cfg_lib_notification_context_t*)message;
	
		dbg_msg (DBG_INFO, "variable %s", nfy->notification_type == DivaCfgVariableChanged ? "changed" : "removed");
		if (nfy->info.variable_changed.instance_by_name != 0) {
			dbg_dump (DBG_INFO, (byte*)nfy->info.variable_changed.instance_ident, nfy->info.variable_changed.instance_ident_length, "instance name", 0, 0);
		} else {
			dbg_msg (DBG_INFO, "instance: %d", nfy->info.variable_changed.instance_nr);
		}
		(*(info->cfg_lib_ifc->diva_cfg_lib_get_name_ident_proc))(nfy->info.variable_changed.variable, &var_name, &var_name_length);
		dbg_dump (DBG_INFO, (byte*)var_name, var_name_length, "  name", 0, 0);
		var_data = (*(info->cfg_lib_ifc->diva_cfg_lib_get_variable_data_proc))(nfy->info.variable_changed.variable, &var_data_length);
		dbg_dump (DBG_INFO, (byte*)var_data, var_data_length, "    data", 0, 0);

		return (-1);
	}
	
	if ((ret = (*(info->cfg_lib_ifc->diva_cfg_lib_get_instance_ident_proc))(instance,
					&instance_name_type,
					&instance_name,
					&instance_name_length,
					&nr)) != DivaCfgLibOK) {
		dbg_msg (DBG_ERROR, "error %d at %d", ret, __LINE__);
		return (-1);
	}
	if (instance_name_type == VieInstance) {
		dbg_msg (DBG_INFO, "instance: %d", nr);
	} else {
		dbg_msg (DBG_INFO, "instance name (%d):", instance_name_length);
		dbg_dump (DBG_INFO, (byte*)instance_name, instance_name_length, "instance name", 0, 0);
	}

	/*
		Read system instance
		*/
	if (instance_name_type != VieInstance &&
			instance_name_length == DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME_SIZE &&
			memcmp (instance_name, DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME, DIVA_CFG_LIB_DIVALOG_SYSTEM_INSTANCE_NAME_SIZE) == 0) {
			const byte* var = (*(info->cfg_lib_ifc->diva_cfg_storage_find_variable_proc))(info->cfg_lib_ifc,
			                                                                              instance,
			                                                                              DIVA_CFG_LIB_DIVALOG_SYSTEM_VAR_TIME_INFO_SYNC_NAME,
			                                                                              DIVA_CFG_LIB_DIVALOG_SYSTEM_VAR_TIME_INFO_SYNC_NAME_SIZE);
			if (var != 0) {
				byte v;

				if ((*(info->cfg_lib_ifc->diva_cfg_lib_read_byte_value_proc))(var, &v) == DivaCfgLibOK) {
					diva_os_time_info_cfg_sync = (v != 0);
				}
			}

			return (0);
	}

	/* delete_all_boards(); */
	while ((var = (*(info->cfg_lib_ifc->diva_cfg_storage_enum_variable_proc))(info->cfg_lib_ifc,
					instance,
					var,
					0,
					0,
					0,
					0,
					&var_name,
					&var_name_length)) != 0) {

		dbg_dump (DBG_INFO, (byte*)var_name, var_name_length, "  name", 0, 0);
		var_data = (*(info->cfg_lib_ifc->diva_cfg_lib_get_variable_data_proc))(var, &var_data_length);
		dbg_dump (DBG_INFO, (byte*)var_data, var_data_length, "    data", 0, 0);
			
		if((var_name_length > 127) || (var_data_length > 255))
		{
			return(-1);
		}
		else
		{
			char                 name_buffer[128];
			char                 ie_buffer[128];
			int                  board_id;
			char path[MAX_PATH+1];
			char *ps, *qs;
			int len, i=0;
			char sep = '/';
			
			memset(ie_buffer, 0, sizeof(ie_buffer));
			memcpy(name_buffer, var_name, var_name_length);
			name_buffer[var_name_length] = 0;
			sep=name_buffer[BOARDNAME_LEN];
			name_buffer[var_name_length] = 0;
			board_id= (int)((name_buffer[BOARDNAME_LEN-2] - 0x30)*10 + (name_buffer[BOARDNAME_LEN-1] - 0x30));
			if(board_id>MAX_NR_BOARDS){
				dbg_msg (DBG_ERROR, "cfg_lib_ifc not allowed board_id %d\n", board_id);
				continue;
			 }
			if(board_conf[board_id-1]==NULL)
				board_conf[board_id-1]=(diva_conf_t *) HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(diva_conf_t));
			ps=name_buffer;
			while((qs=strchr(ps, sep))!= NULL){
				i++;
				if(i==2){
					port_id=(int)(name_buffer[PORTPATH_LEN-1] - 0x30);
					if(port_id>MAX_NR_PORTS){
						dbg_msg (DBG_ERROR, "cfg_lib_ifc not allowed port_id %d\n", port_id);
						continue;
					}
					if(*board_conf[board_id-1]->PortKey[port_id].path==0){
					snprintf(path, sizeof(path),"%s", ps);
						path[PORTNAME_LEN]=0;
						if (RegCreateBoardDir((int *)board_id,
							path,
							0,
							0,
									&(*board_conf[board_id-1]->PortKey[port_id].path),
							0) != ERROR_SUCCESS) {
						return -1;
					} 
					}
					board_conf[board_id-1]->PortKey[port_id].touch=1;
					memcpy(ie_buffer, var_data, var_data_length);
					ie_buffer[var_data_length]=0;
				                              
					if (write_ascii_value(&board_conf[board_id-1]->PortKey[port_id], qs+1, (char*)ie_buffer)) {
						return (-1);
					}
					
				}
				ps=qs+1;
			}
				
			if(board_conf[board_id-1]->ConfKey.touch==0){   
		
				if((ps=strchr(name_buffer, sep))== NULL){
					memset(ie_buffer, 0, sizeof(ie_buffer));
					return(0);
				}

				if(*board_conf[board_id-1]->ConfKey.path==0){
				len= var_name_length-strlen(ps);
				memcpy(ie_buffer, var_name, len);
				ie_buffer[len]=0;
									
				snprintf(path, sizeof(path), "%s%s%s", DIVA_PUBLIC_BOARD_BASE, path_separator, ie_buffer);
				path[sizeof(path)-1]=0;
					if (RegCreateBoardDir (HKEY_LOCAL_MACHINE,
						                           path,
						  0,
						  0,
										&(*board_conf[board_id-1]->ConfKey.path),
						  0) != ERROR_SUCCESS) {
					return -1;
				} 
				}   
				board_conf[board_id-1]->ConfKey.touch=1;
				memcpy(ie_buffer, var_data, var_data_length);
				ie_buffer[var_data_length]=0;
				          
				if(write_ascii_value(&board_conf[board_id-1]->ConfKey, ps+1, (char*)ie_buffer)){
						return -1;
			        }
			}
			else{ if(i==1){
					unsigned int value;
				
					memcpy(ie_buffer, var_data, var_data_length);
				value=(ie_buffer[0]&0xff) | (ie_buffer[1]&0xff)<<8 | (ie_buffer[2]&0xff)<<16 | (ie_buffer[3]&0xff)<<24;

					snprintf(ie_buffer, sizeof(ie_buffer), "%u", value);
					ie_buffer[sizeof(ie_buffer)-1]=0;	 
				if(write_ascii_value(&board_conf[board_id-1]->ConfKey, ps, (char*)ie_buffer)){
						return -1;
					}
					
					
				}
  
				/* Read in variable descriptor of management interface to check the type of variable and its size */
				memset(ie_buffer, 0, sizeof(ie_buffer));
			
			
			
			}
			
			
		}
		
	}
	del_untouched();
	inc_update_count();
	
	dbg_msg (DBG_INFO, "---------------------------------------------------");
	
	return (0);
	
}

static void	ini_update_count(void){
	char path[MAX_PATH+1];
	char ie_buffer[16];
	FILE* fs;

	snprintf(path, sizeof(path), "%s%s%s", registry_path, path_separator, DIVA_PUBLIC_BOARD_BASE);
	path[sizeof(path)-1]=0;
	if(mkdir(path, 0777))
		perror("Error to create Dir");
	snprintf(path, sizeof(path), "%s%s%s", registry_path, path_separator, DIVA_UPDATE_COUNT_BASE);
	path[sizeof(path)-1]=0;
	if(mkdir(path, 0777))
		perror("Error to create Dir");
	snprintf(ie_buffer, sizeof(ie_buffer), "%x", 0);
	ie_buffer[sizeof(ie_buffer)-1]=0;
	snprintf(update_count.name, sizeof(update_count.name), "%s%s%s", path, path_separator, "update_count");
	update_count.name[sizeof(update_count.name)-1]=0;
				                              
	if ((fs = fopen (update_count.name, "w"))) {
		fprintf (fs, "%s\n", ie_buffer);
		fflush (fs);
		fclose (fs);
	}
	update_count.value=0;
	
}
#endif // BOARDCONF	


/*
	Main STrace message loop.
	Loop is terminated by set of the hStop event (Should be "ManualReset" event)
	*/
static int strace_main (HANDLE hStop, user_context_t* user_context) {
	HANDLE hWait[DIVA_SUPPORTED_ADAPTERS+1]; /* Completion handles */
	diva_adapter_t* Map[DIVA_SUPPORTED_ADAPTERS]; /* Map's completion nr to adapter nr */
#if defined(LINUX)
	struct pollfd fds[DIVA_SUPPORTED_ADAPTERS+1];
	int ret;
#else
	DWORD ret;
#endif
	diva_entity_queue_t adapters;
	time_t detection_to = 0, to;
	int first_detection = 1;
	int nr=0, return_status = 0, i, one_suspended;

#ifdef BOARDCONF	
	appl_info_t info;
	struct _diva_io_object_user_enum_ifc*  modules[1];
	int nr_io_modules = 0;
	diva_modules_user_ipc_proc_t user_ipc_proc = 0;
	void*                        user_ipc_proc_context = 0;
	int run = 1;
	diva_modules_error_proc_t    user_error_proc	= error_proc;
	
	ini_update_count();	
	
	if ((modules[nr_io_modules++] = diva_um_cfg_lib_init (TargetDivalog,	&info.cfg_lib_ifc)) == 0)
	{
		int i;
		for (i = 0; i < nr_io_modules-1; i++) {
			(*(modules[i]->release_proc))(modules[i]);
		}
		dbg_msg (DBG_ERROR, "failed to initialize CfgLib");
		dbg_delete();
		diva_timer_deinit ();
		return (-1);
	}
	if ((*(info.cfg_lib_ifc->diva_cfg_lib_cfg_register_notify_callback_proc))
								(diva_cfg_lib_cfg_notify_callback, &info, TargetDivalog) == 0) {
		int i;

		for (i = 0; i < nr_io_modules; i++) {
			(*(modules[i]->release_proc))(modules[i]);
		}
		dbg_msg (DBG_ERROR, "failed to register CfgLib notification callback");
		dbg_delete();
		diva_timer_deinit ();
		return (-1);
	}

	user_ipc_proc   = (user_ipc_proc == 0)   ? diva_modules_user_ipc_proc : user_ipc_proc;
	user_error_proc = (user_error_proc == 0) ? diva_modules_error_proc    : user_error_proc;

#endif // BOARDCONF

	diva_q_init (&adapters);

	user_context->adapters = &adapters;

	hWait[0] = hStop;
	
	for (;;) {
#ifdef BOARDCONF
		int one_sent, nr_modules = nr_io_modules;
#endif // BOARDCONF	
		one_suspended = 0;
		suspend_adapters (&adapters);
		to = time(0);
		remove_suspended (&adapters, to);

		if ((to - detection_to) >= DIVA_DETECTION_TO) {
#ifdef BOARDCONF	
		/*
		Process module specific IPC messages and used IPC
		*/
			do {
				int user_ipc_state;

				one_sent = 0;
				for (i = 0; i < nr_modules; i++) {
					one_sent |= modules[i]->ipc_proc(modules[i]);
				}

				user_ipc_state = (*user_ipc_proc)(user_ipc_proc_context, run);
				if (user_ipc_state > 0) {
					one_sent |= 1;
				} else if (user_ipc_state < 0) {
					(*user_error_proc)("error in %s at %d", __FILE__, __LINE__);
					run = 0;
				}
			} while (one_sent != 0);
		

			for (i = 0, nr = 0; i < nr_modules; i++) {
				nr += modules[i]->nr(modules[i]);
			}
		
			{
				struct pollfd fdss[nr]; /* Linux: read, write and status events are always the same */
				diva_io_object_user_ifc_t* ifcs[nr];
				DIVA_OS_HANDLE h;
				int nr_fds = 0;
				int nr_signalled = 0;
			
			

				for (i = 0; i < nr_modules; i++) {
					diva_io_object_user_enum_ifc_t* module = modules[i];
					diva_io_object_user_ifc_t* ifc = 0;

					while ((ifc = module->get(module,ifc)) != 0) {
						fdss[nr_fds].events = 0;
						if ((h = ifc->read_handle_proc(ifc)) != DIVA_OS_INVALID_HANDLE_CFG) {
							fdss[nr_fds].fd = h;
							fdss[nr_fds].events |= POLLIN;
						}
						if ((h = ifc->write_handle_proc(ifc)) != DIVA_OS_INVALID_HANDLE_CFG) {
							fdss[nr_fds].fd = h;
							fdss[nr_fds].events |= POLLOUT;
						}
 						if ((h = ifc->state_change_proc(ifc)) != DIVA_OS_INVALID_HANDLE_CFG) {
							fdss[nr_fds].fd = h;
							fdss[nr_fds].events |= POLLHUP;
						}
						if (fdss[nr_fds].events != 0) {
							fdss[nr_fds].events |= (POLLERR | POLLHUP | POLLNVAL);
							fdss[nr_fds].revents = 0;
							ifcs[nr_fds] = ifc;
							nr_fds++;
						}
					}
				}
				switch ((nr_signalled = poll (fdss, nr_fds, 10))) 
				{
					case 0:
						break;
					case -1:
						if (errno != EINTR) {
							run = 0;
						} else if (run == 1) {
							run = 2; /* Start shutdown */
						}
						break;

					default:
						for (i = 0; i < nr_fds && nr_signalled != 0; i++) {
							if (fdss[i].revents != 0) {
								nr_signalled--;
								if (ifcs[i]->message_input_proc (ifcs[i],
									(fdss[i].revents & POLLIN) != 0, 
									(fdss[i].revents & POLLOUT) != 0,
									(fdss[i].revents & POLLHUP) != 0,
									(fdss[i].revents & (POLLERR | POLLNVAL)) != 0) != 0) {
									if (run == 1) {
										run = 0;
									}
								}
							}
						}
						break;
				}
			}
#endif // BOARDCONF

//			if ((to - detection_to) >= DIVA_DETECTION_TO) {}
			insert_new (&adapters, user_context);
			if (first_detection) {
				detection_to = to - DIVA_DETECTION_TO + 3;
				first_detection = 0;
			} else {
				detection_to = to;
			}
		}
		nr = create_map (user_context, Map, &hWait[1]);
#if defined(LINUX)
		for (i = 0; i < nr; i++) {
			fds[i].fd      = (int)(long)hWait[i+1];
			fds[i].events  = POLLIN | POLLERR | POLLHUP | POLLNVAL;
			fds[i].revents = 0;
		}
#endif

#if 0
		if (divalog_debug) printf("after createmap: no of adapters %d\n",nr);
 		if (divalog_debug) for (i = 0; i < nr; i++) {
			printf("  adapter[%d]->hlib = %d, ->hlib->hlib = %d\n",i,Map[i]->hLib,Map[i]->hLib->hLib);
		}
#endif

		/*
			Request update of statistics if necessary
			*/
		for (i = 0; i < nr; i++) {
			to = time(0);
			if(Map[i]->adapter_type != DIVA_ADAPTER_TYPE_HM) {
				if ((to - Map[i]->stat_update) >= DIVA_STAT_UPDATE_TO) {
					int res = 0;
					if (Map[i]->adapter_nr < DIVA_MAX_ADAPTERS) {
						if (divalog_debug) printf("diva_stat_update_to: adapter %d\n",Map[i]->adapter_nr);
						Map[i]->stat_update = to;
						res |= (*(Map[i]->hLib->DivaSTraceGetModemStatistics)) (Map[i]->hLib->hLib);
						res |= (*(Map[i]->hLib->DivaSTraceGetFaxStatistics)) (Map[i]->hLib->hLib);
						res |= (*(Map[i]->hLib->DivaSTraceGetBLayer1Statistics)) (Map[i]->hLib->hLib);
						res |= (*(Map[i]->hLib->DivaSTraceGetBLayer2Statistics)) (Map[i]->hLib->hLib);
						res |= (*(Map[i]->hLib->DivaSTraceGetDLayer1Statistics)) (Map[i]->hLib->hLib);
						res |= (*(Map[i]->hLib->DivaSTraceGetDLayer2Statistics)) (Map[i]->hLib->hLib);
						if (Map[i]->polling_mode != 0) {
							res |= (*(Map[i]->hLib->DivaSTraceGetOutgoingCallStatistics)) (Map[i]->hLib->hLib);
							res |= (*(Map[i]->hLib->DivaSTraceGetIncomingCallStatistics)) (Map[i]->hLib->hLib);
							res |= (*(Map[i]->hLib->DivaSTraceGetLayer1Statistics)) (Map[i]->hLib->hLib);
						}
						res |= (*(Map[i]->hLib->DivaSTraceGetResourceInfo)) (Map[i]->hLib->hLib);
						(*(Map[i]->hLib->DivaSTraceUpdateTime)) (Map[i]->hLib->hLib);

#if 0
						if (divalog_debug) printf("  end of statistic updates, result: %d\n",res);
#endif
						{
							/*
								Update channel statistics for all active channels
							*/
							int ch, state;

							for (ch = 0; ((!res)&&((state = diva_log_is_channel_active (Map[i]->adapter_nr, ch))>=0));ch++) {
								if (!state) {
									res |= (*(Map[i]->hLib->DivaSTraceGetBLayer1ChannelStatistics)) (Map[i]->hLib->hLib, ch);
									res |= (*(Map[i]->hLib->DivaSTraceGetBLayer2ChannelStatistics)) (Map[i]->hLib->hLib, ch);
								}
							}
						}
					} else {
						if (divalog_debug) printf("diva_stat_update_to: service %d request registrar state\n",Map[i]->adapter_nr);
							Map[i]->stat_update = to;
							/* res |= (*(((diva_strace_library_sip_interface_t*)Map[i]->hLib)->DivaSTraceSipGetHeartBeat)) (Map[i]->hLib->hLib); */
							res |= (*(((diva_strace_library_sip_interface_t*)Map[i]->hLib)->DivaSTraceSipGetRegistrar)) (Map[i]->hLib->hLib);
						}
						if (res) {
							if (divalog_debug) { printf("Adapter %d (%s) WDog error\n", Map[i]->adapter_nr, Map[i]->hLib->adapter_name); }
							else syslog (LOG_ERR, "Diva Media Board %d (%s) status reporting temporary disabled\n", Map[i]->adapter_nr, Map[i]->hLib->adapter_name);
							Map[i]->error = 1;
							one_suspended = 1;
						}
					}
				}
			}
		/*
			Check if some adapter had not responded for too long time
			*/
		to = time(0);
		for (i = 0; i < nr; i++) {
			if(Map[i]->adapter_type !=DIVA_ADAPTER_TYPE_HM) {
				if ((to - Map[i]->timestamp) > DIVA_ERROR_TO) {
					if (divalog_debug) {
						printf("error_to encountered on adapter: %d\n", i);
						printf("Adapter %d (%s) WDog error\n", Map[i]->adapter_nr, Map[i]->hLib->adapter_name);
					} else {
					  syslog (LOG_ERR, "Diva Media Board %d (%s) status reporting temporary disabled\n", Map[i]->adapter_nr, Map[i]->hLib->adapter_name);
					}
					Map[i]->error = 1;
					one_suspended = 1;
				}
			}
		}

		if (one_suspended) {
			/*
				In case one of adapters was suspended due to internale error
				then it is necessary to start from begin on
				*/
			continue;
		}

#if !defined(LINUX) /* { */
		ret = WaitForMultipleObjects (nr+1, hWait, FALSE, 1000);
		if (ret == WAIT_FAILED) {
			/*
				Critical error, terminate application
				*/
			return_status = 1;
			break;
		} else if (ret == WAIT_TIMEOUT) {
			/*
				Time out
				*/
		} else if (ret == WAIT_OBJECT_0) {
			/*
				Terminate application
				*/
			break;
		} else {
			ret -= (WAIT_OBJECT_0+1);
			if (ret >= (dword)nr) {
				/*
					Serious error - signalled handle not available
					*/
				return_status = 1;
				break;
			}
			if(Map[ret]->adapter_type !=DIVA_ADAPTER_TYPE_HM) {
				if ((*(Map[ret]->hLib->DivaSTraceMessageInput))(Map[ret]->hLib->hLib)) {
					/*
						Error mark adapter for suspend
						*/
					Map[ret]->error = 1;
				} else {
					/*
						Store the time stamp of the last succesfull processed message
						*/
					Map[ret]->timestamp = time(0);
				}
			}
		}
#else /* } { */
		if ((ret = poll (fds, nr, 1000)) < 0) {
			if (errno != EINTR)
				return_status = 1;
			break;
		}
		for(i = 0; i < nr && ret > 0; i++) {
			if(Map[i]->adapter_type !=DIVA_ADAPTER_TYPE_HM) {
    				if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
					Map[i]->error = 1;
				} else if (fds[i].revents & POLLIN) {
					ret--;
					if ((*(Map[i]->hLib->DivaSTraceMessageInput))(Map[i]->hLib->hLib)) {
						Map[i]->error = 1;
					} else {
						Map[i]->timestamp = time(0);
					}
				}
			}
		}
		if (*hStopEvent) {
		  break;
		}
#endif /* } */
	}

  	if (divalog_debug) printf("remove_all: adapters \n");

	remove_all (&adapters);

	return (return_status);
}

/*
	Returns true if current log segment exists and
	is not empty
	*/
static int log_segment_not_empty (const char* name) {
	struct stat buf;

	if (stat (name, &buf) == 0) {
		if (buf.st_size > (12*1024)) {
			return (1);
		} else if (buf.st_size > 24) {
			char buffer[12*1024+1];
			FILE* f = fopen (name, "r");
			if (f != 0) {
				long length = (long)(fread (buffer, 1, 12*1024, f));
				fclose(f);
				if (length > 0) {
					buffer[length] = 0;

					if (strstr(buffer, "\nIN")  || strstr(buffer, "\rIN") ||
							strstr(buffer, "\nOUT") || strstr(buffer, "\rOUT")) {
						return (1);
					}
				}
			}
		}
	}

	return (0);
}

/*
	Rename all log files:

	xxx.N   -> xxx.index
	xxx.N-1 -> xxx.N
	...
	...
	...
	xxx     -> xxx.1

	After this operation xxx can be written again.
	*/
static int move_log_files_to (int init) {
	dword index = get_last_log_file_index ();
	const char* fname = GetLogFileName (0); /* get log file name without extension */
	char fname_ext[MAX_PATH+1+512];
	struct stat buf;
	char src[MAX_PATH+1+512];
	char dst[MAX_PATH+1+512];

	sprintf (fname_ext,"%s%s",fname,log_file_extension);

	/*
		Initial file rotation only if first segment is
		not empty.
		Otherwise just delete first segment.
		Archived filenames are name.nr.ext, where ext on linux is empty
		*/
	if (init == 0 || log_segment_not_empty (fname_ext)) {
		while (index) {
			sprintf (dst, "%s.%d%s", fname, index, log_file_extension);
			if (index-1) {
				sprintf (src, "%s.%d%s", fname, index-1, log_file_extension);
			} else {
				sprintf (src, "%s%s", fname, log_file_extension);
			}
			if (divalog_debug) printf("Rotate logfile %s to %s\n",src,dst);
			rename (src, dst);
			index--;
		}
	} else {
		if (divalog_debug) printf("Delete and recreate %s (emtpy file).\n",fname_ext);
		DeleteFile (fname_ext);
	}

	return (stat (fname_ext, &buf) ? 0 : (-1));
}

/*
	Used to rotate the LOG files.
	*/
int diva_rotate_log_file (FILE** pLog) {
	time_t t = time(0);
	if (fprintf (*pLog, "# Diva Log file closed at %s\n", asctime (localtime (&t))) <= 0) {
		return (-1);
	}
	fclose (*pLog);

	if (move_log_files_to (0)) {
		*pLog = fopen ("/dev/null", "w");
		return (-1);
	}
	if (!(*pLog = fopen (GetLogFileName(1), "w"))) {
		*pLog = stderr;
		return (-1);
	}
	{
		FILE* Log[2];
		Log[0] = *pLog;
		Log[1] = 0;
		if (diva_write_log_header (Log)) {
			return (-1);
		}
	}

	return (0);
}

static const char* GetLogFileName (int include_extension) {
	static char path [MAX_PATH + 512];

	strcpy(path, log_file_path);

#if !defined(LINUX)
	if (strlen (path) && path[strlen(path)-1] != '\\') {
		strcat (path, "\\");
	}

	strcat (path, "divalog");
	if (include_extension) {
		strcat (path, log_file_extension);
	}
#endif

	return (&path[0]);
}

/*
	Returns in index of the last not existing file, to that we can rotate

	This is the function where it is possible to implement free space
	policy (i.e. remove last segment(s) instead of rename.
	*/
static dword get_last_log_file_index (void) {
	const char* fname = GetLogFileName (0);
	char name[MAX_PATH+1+512];
	int nr;
	struct stat buf;

	for (nr = 1; ; nr++) {
		sprintf (name, "%s.%d%s", fname, nr, log_file_extension);
		if (stat (name, &buf)) {
			break;
		}
		if (nr >= max_log_segments) {
			DeleteFile (name);
		}
	}

	return ((nr < max_log_segments) ? nr : max_log_segments);
}

/*
	Should store error information in the system event log
	*/
void diva_syslog_record (const char* message) {
	AddToMessageLog ((LPTSTR)message);
}

#if !defined(LINUX)
int syslog (int err, const char* fmt, ...) {
	va_list ap;
	char tmp[2048];

	va_start (ap, fmt);
	_vsnprintf (tmp, sizeof(tmp)-1, fmt, ap);
	tmp[sizeof(tmp)-1] = 0;
	va_end (ap);

	diva_syslog_record (&tmp[0]);

	return (0);
}
#endif

static void diva_local_error_proc (void* user_context,
                          				 diva_strace_library_interface_t* hLib,
																	 int Adapter,
																	 int error,
																	 const char* file,
																	 int line) {
	diva_entity_queue_t* q = ((user_context_t*)user_context)->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_get_head (q);

	if (divalog_debug) { printf("ERROR: %d %s:%d on Adapter %d\n", error, file, line, Adapter); }
	else syslog (LOG_ERR, "ERROR: %d %s:%d on Adapter %d\n", error, file, line, Adapter);

	while (pA) {
		if (pA->hLib == hLib) {
			pA->error = 1;
			return;
		}
		pA = (diva_adapter_t*)diva_q_get_next (&pA->link);
	}
}

#if !defined(LINUX)
static void CmdInstallService (void) {
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;
	TCHAR szPath[MAX_PATH+1];
	int installed = 0;

	if (GetModuleFileName( NULL, szPath, sizeof(szPath)-1) == 0) {
		_tprintf(TEXT("Unable to install %s - %s\n"),
				TEXT(SZSERVICEDISPLAYNAME), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
		return;
	}

	if ((schSCManager = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
		if ((schService = CreateService(schSCManager,
															 TEXT(SZSERVICENAME),        // name of service
															 TEXT(SZSERVICEDISPLAYNAME), // name to display
															 SERVICE_ALL_ACCESS,         // desired access
															 SERVICE_WIN32_OWN_PROCESS,  // service type
															 SERVICE_AUTO_START,         // start type
															 SERVICE_ERROR_NORMAL,       // error control type
															 szPath,                     // service's binary
															 NULL,                       // no load ordering group
															 NULL,                       // no tag identifier
															 TEXT(SZDEPENDENCIES),       // dependencies
															 NULL,                       // LocalSystem account
															 NULL))) {                   // no password

		  SERVICE_DESCRIPTION serviceDesc;
  		serviceDesc.lpDescription = szServiceDescription;
  		ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &serviceDesc);

			_tprintf(TEXT("%s installed.\n"), TEXT(SZSERVICEDISPLAYNAME) );
			CloseServiceHandle(schService);
			installed = 1;
		} else {
			_tprintf(TEXT("CreateService failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
		}
		CloseServiceHandle(schSCManager);
	} else {
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
	}

	if (!installed) {
		exit (1);
	}
}

static void CmdStartService (void) {
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;
	int started = 0;

	if ((schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
		if ((schService = OpenService(schSCManager, TEXT(SZSERVICENAME), SERVICE_ALL_ACCESS))) {
			/*
				Start the service
				*/
			if (StartService (schService, 0, 0)) {
				_tprintf(TEXT("Starting %s."), TEXT(SZSERVICEDISPLAYNAME));
				Sleep (1000);

				while (QueryServiceStatus( schService, &ssStatus)) {
					if (ssStatus.dwCurrentState == SERVICE_START_PENDING) {
						_tprintf(TEXT("."));
						Sleep( 1000 );
					} else {
						break;
					}
				}

				if (ssStatus.dwCurrentState == SERVICE_RUNNING) {
					_tprintf(TEXT("\n%s started.\n"), TEXT(SZSERVICEDISPLAYNAME));
					started = 1;
				} else {
					_tprintf(TEXT("\n%s failed to start.\n"), TEXT(SZSERVICEDISPLAYNAME) );
				}

			}
			CloseServiceHandle(schService);
		} else {
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
		}
		CloseServiceHandle(schSCManager);
	} else {
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
	}

	if (!started) {
		exit (1);
	}
}

static void CmdStopService (void) {
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;
	int stopped = 0;

	if ((schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
		if ((schService = OpenService(schSCManager, TEXT(SZSERVICENAME), SERVICE_ALL_ACCESS))) {
			/*
				Stop the service
				*/
			if (ControlService (schService, SERVICE_CONTROL_STOP, &ssStatus )) {
				_tprintf(TEXT("Stopping %s."), TEXT(SZSERVICEDISPLAYNAME));
				Sleep (1000);

				while (QueryServiceStatus( schService, &ssStatus)) {
					if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
						_tprintf(TEXT("."));
						Sleep( 1000 );
					} else {
						break;
					}
				}

				if ( ssStatus.dwCurrentState == SERVICE_STOPPED ) {
					_tprintf(TEXT("\n%s stopped.\n"), TEXT(SZSERVICEDISPLAYNAME));
					stopped = 1;
				} else {
					_tprintf(TEXT("\n%s failed to stop.\n"), TEXT(SZSERVICEDISPLAYNAME) );
				}

			}
			CloseServiceHandle(schService);
		} else {
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
		}
		CloseServiceHandle(schSCManager);
	} else {
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
	}

	if (!stopped) {
		exit (1);
	}
}

static void CmdRemoveService (void) {
	SC_HANDLE   schService;
	SC_HANDLE   schSCManager;
	int deleted = 0;

	if ((schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
		if ((schService = OpenService(schSCManager, TEXT(SZSERVICENAME), SERVICE_ALL_ACCESS))) {
			/*
				Stop the service
				*/
			if (ControlService (schService, SERVICE_CONTROL_STOP, &ssStatus )) {
				_tprintf(TEXT("Stopping %s."), TEXT(SZSERVICEDISPLAYNAME));
				Sleep (1000);

				while (QueryServiceStatus( schService, &ssStatus)) {
					if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
						_tprintf(TEXT("."));
						Sleep( 1000 );
					} else {
						break;
					}
				}

				if ( ssStatus.dwCurrentState == SERVICE_STOPPED ) {
					_tprintf(TEXT("\n%s stopped.\n"), TEXT(SZSERVICEDISPLAYNAME));
				} else {
					_tprintf(TEXT("\n%s failed to stop.\n"), TEXT(SZSERVICEDISPLAYNAME) );
				}

			}

			if (DeleteService(schService)) {
				_tprintf(TEXT("%s removed.\n"), TEXT(SZSERVICEDISPLAYNAME) );
				deleted = 1;
			} else {
				_tprintf(TEXT("DeleteService failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
			}
			CloseServiceHandle(schService);
		} else {
			_tprintf(TEXT("OpenService failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
		}
		CloseServiceHandle(schSCManager);
	} else {
		_tprintf(TEXT("OpenSCManager failed - %s\n"), GetLastErrorText(szErr, sizeof(szErr)/sizeof(szErr[0])));
	}

	if (!deleted) {
		exit (1);
	}
}
#endif

static void AddToMessageLog (LPTSTR lpszMsg) {
#if !defined(LINUX)
	TCHAR   szMsg[512];
	HANDLE  hEventSource;
	LPTSTR  lpszStrings[2];

	dwErr = GetLastError();

	/*
		Use event logging to log the error.
		*/
	hEventSource = RegisterEventSource(NULL, TEXT("Dimaint"));

	_stprintf(szMsg, TEXT("%s error: %d"), TEXT(SZSERVICENAME), dwErr);
	lpszStrings[0] = szMsg;
	lpszStrings[1] = lpszMsg;

	if (hEventSource != NULL) {
		ReportEvent (hEventSource, // handle of event source
								 EVENTLOG_INFORMATION_TYPE,  // event type
								 0,                    // event category
								 DIEVENT_INFORMATIONAL,      // event ID
								 NULL,                 // current user's SID
								 2,                    // strings in lpszStrings
								 0,                    // no bytes of raw data
								 lpszStrings,          // array of error strings
								 NULL);                // no raw data
		(void)DeregisterEventSource (hEventSource);
	}
#else
	syslog (LOG_ERR, "%s\n", lpszMsg);
#endif
}

static BOOL ReportStatusToSCMgr (DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
	BOOL fResult = TRUE;
#if !defined(LINUX)
	static DWORD dwCheckPoint = 1;


	if (dwCurrentState == SERVICE_START_PENDING) {
		ssStatus.dwControlsAccepted = 0;
	} else {
		ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

	ssStatus.dwCurrentState = dwCurrentState;
	ssStatus.dwWin32ExitCode = dwWin32ExitCode;
	ssStatus.dwWaitHint = dwWaitHint;

	if ((dwCurrentState == SERVICE_RUNNING) ||
			(dwCurrentState == SERVICE_STOPPED )) {
            ssStatus.dwCheckPoint = 0;
	} else {
		ssStatus.dwCheckPoint = dwCheckPoint++;
	}

	if (!(fResult = SetServiceStatus (sshStatusHandle, &ssStatus))) {
		AddToMessageLog(TEXT("SetServiceStatus"));
	}

#endif
	return (fResult);
}

#if !defined(LINUX)
static LPTSTR GetLastErrorText(LPTSTR lpszBuf, DWORD dwSize) {
	LPTSTR lpszTemp = 0;
	DWORD dwRet = FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
															 FORMAT_MESSAGE_FROM_SYSTEM |
															 FORMAT_MESSAGE_ARGUMENT_ARRAY,
															 NULL,
															 GetLastError(),
															 LANG_NEUTRAL,
															 (LPTSTR)&lpszTemp,
															 0,
															 NULL);
	if (!dwRet || (dwSize < dwRet+14)) {
		lpszBuf[0] = TEXT('\0');
	} else {
		lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  //remove cr and newline character
		_stprintf( lpszBuf, TEXT("%s (0x%x)"), lpszTemp, GetLastError() );
	}
	if (lpszTemp) {
		LocalFree((HLOCAL) lpszTemp );
	}

	return (lpszBuf);
}
#endif

/* ----------------------------------------------------------------------------
		Functions used to update the state of the public interface.

		There are two types of the run-time interface:
			- File system based interface
			- Registry based interface

		Both interfaces have following structure:
    \
    +-ADAPTERX\
              +-NAME (Contains adapter name)
              +-SERIAL (Contains adapter serial number)
              +-STATE\
                     +-CHANNELX (Contains information about state of the channel X)
	---------------------------------------------------------------------------- */
int diva_write_channel_state (FILE** pLog, int adapter_number, int line_number, const char* data) {
	user_context_t* user_context = (user_context_t*)pLog;
	diva_entity_queue_t* q = ((user_context_t*)user_context)->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_find(q, &adapter_number, cmp_adapter_nr);
	int ret = -1;

	if (pA && pA->hLib) {
		char path[MAX_PATH+1+512];
		int key_id;

		if (pA->adapter_type==DIVA_ADAPTER_TYPE_ANALOG ) {
			key_id = line_number-1;
		} else {
			key_id = 0;
		}
		if (pA->ifc_available) {
			if (pA->adapter_type==DIVA_ADAPTER_TYPE_SOFTIP ) {
				diva_log_softip_map_t * sipmap = &user_context->softip_adapters;
				int i;
				i = (sipmap->softip_adapter_map[adapter_number] - 1) * pA->channels; /* should be same for all softip adapters (30) */
		sprintf (path, "channel%d", line_number + i);
			} else {
	  	sprintf (path, "channel%d", line_number);
			}
			if (data && data[0]) {
	    ret = write_ascii_reg_value (pA->ChannelsKey[key_id], path, data);
			} else {
				ret = write_ascii_reg_value (pA->ChannelsKey[key_id], path, 0);
			}
		}
	}

	return (ret);
}

int diva_write_config (diva_adapter_t* pA, int line_number) {
	diva_ifc_config_t * p = ((diva_ifc_config_t *) diva_log_get_adapter_config_ptr (pA->adapter_nr));
	static int s_chan = 0;
	char data[4096];
	char data2[4096];

	sprintf(data2, ",%d,%d,%d,%d,'%s','%s',%d,'%s','%s',%d", p->InterfaceNr, p->BoardRevision, p->SubFunction, p->SubDevice, p->ProtocolBuild, p->DSPCodeBuild, p->analogChannels, p->PRI ? "YES" : "NO", p->PCIDMA ? "YES" : "NO", p->cardtype);
	switch (p->type) {
		case DIVA_ADAPTER_TYPE_ANALOG:
			sprintf (data, "ANALOG,%d,-,-,-%s", p->channels, data2);
			break;
		case DIVA_ADAPTER_TYPE_PRI:
			sprintf (data, "PRI,%d,%d,", p->channels, p->protocol);
			if (p->NTmode) strcat(data, "NT,TRUE"); else strcat(data, "TE,TRUE");
			strcat(data, data2);
			break;
		case DIVA_ADAPTER_TYPE_BRI:
 			sprintf (data, "BRI,%d,%d,", p->channels, p->protocol);
			if (p->NTmode) strcat(data, "NT,"); else strcat(data, "TE,");
			if (p->TEI) strcat(data, "TRUE"); else strcat(data, "FALSE");
			strcat(data, data2);
			break;
		case DIVA_ADAPTER_TYPE_SOFTIP:
			s_chan += p->channels;
			sprintf (data, "SoftIP,%d,-,-,-,-,-,%d,%d,'%s',-,-,'YES','YES',%d", s_chan, p->SubFunction, p->SubDevice, p->ProtocolBuild, p->cardtype);
			break;
		default:
	 		sprintf (data, "ERROR");
	}  

	sprintf (&data[strlen(data)], ",%c", p->law != 0 ? p->law : '?');

	if (write_ascii_reg_value (pA->InfoKey[line_number-1], "Config", data)) {
		return (-1);
	}
	if (pA->BoardKey) {
		char buffer[64];
		sprintf (buffer, "LineType,Signaling,CodingType,Channels,CardOrdinal");
		if (write_ascii_reg_value (pA->BoardKey, "config description","LineType,Signaling,CodingType,Channels,CardOrdinal") ) {
			return (-1);
		}
		sprintf (buffer, "%s,Clear,%s,%d,%d", (p->channels>24?"MF":"ESF"), (p->channels>24?"HDB3":"B8ZS"), p->channels, p->cardtype);
		if (write_ascii_reg_value (pA->BoardKey, "config", buffer)) {
			return (-1);
		}
	}
	return 0;
}

int diva_write_sip_registrar_state (FILE** pLog, const char* data) {
	user_context_t* user_context = (user_context_t*)pLog;
	diva_log_softip_map_t * sipmap = &user_context->softip_adapters;
	diva_entity_queue_t* q = ((user_context_t*)user_context)->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_find(q, &sipmap->first_softip_adapter, cmp_adapter_nr);
	
	return (pA ? write_ascii_reg_value (pA->RegistrarKey, "Server 1", data) : 0);
}

/*
	Create directory that represents the state of the current adapter
	*/
static int diva_create_adapter_directory (diva_adapter_t* pA) {
	if (pA->hLib) {
		diva_create_registry_adapter_tree (pA);
	} else {
		diva_destroy_registry_adapter_tree (pA);
	}
	return (0);
}

#if !defined(LINUX)

static int diva_get_config_from_registry (void) {

	char		name [sizeof(DIVA_CONFIG_REGISTRY_PATH)+1];
	char		tmp_str[MAX_PATH+1];
	DWORD		dwDisposition, size, tmp_dword;
	HKEY		reg_key;
	int 		ret = 0;

	sprintf (name, "%s", DIVA_CONFIG_REGISTRY_PATH);
	if (RegCreateKeyEx (HKEY_LOCAL_MACHINE,
			name,
			0,
			0,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			0,
			&reg_key,
			&dwDisposition) != ERROR_SUCCESS) {
		return (-1);
	}
	if (dwDisposition == REG_CREATED_NEW_KEY) {
		sprintf (name, "LogFilePath");
		if (RegSetValueEx (reg_key, name, 0, REG_SZ, (const byte *)log_file_path, (DWORD)(strlen(log_file_path)+1)) != ERROR_SUCCESS) {
		/* could not set value */
			ret=-1;
		}
		sprintf (name, "LogFileCount");
		if (RegSetValueEx (reg_key, name, 0, REG_DWORD, (const byte *)&max_log_segments, sizeof(max_log_segments)) != ERROR_SUCCESS) {
		/* could not set value */
			ret=-1;
		}
	} else {
	/* read in values found in registry */
		sprintf (name, "LogFilePath");
		size=sizeof(tmp_str);
		if (RegQueryValueEx(reg_key, name, 0, 0, (byte *)tmp_str, &size) != ERROR_SUCCESS) {
			ret=-1;
		} else {
			sprintf(log_file_path, "%s", tmp_str);
		}
		sprintf (name, "LogFileCount");
		size=sizeof(tmp_dword);
		if (RegQueryValueEx(reg_key, name, 0, 0, (byte *)&tmp_dword, &size) != ERROR_SUCCESS) {
			ret=-1;
		} else {
			max_log_segments = (int)tmp_dword;
		}
	}
	RegCloseKey (reg_key);
	if (!ExpandEnvironmentStrings(log_file_path,tmp_str,sizeof(tmp_str))) {
		/* could not expand env vars */
	} else {
		sprintf(log_file_path, "%s", tmp_str);
	}
	return(ret);
}
#endif

static int diva_create_registry_adapter_tree (diva_adapter_t* pA) {
	if (!pA->ifc_available && pA->hLib) {
		char name [sizeof(DIVA_PUBLIC_ADAPTER_IFC_BASE)+24];
		char boardname [sizeof(DIVA_PUBLIC_HW_BASE)+24];
		int key_count = 1;
		int i;
#if !defined(LINUX)
		HKEY tmpkey;
#endif

		if (pA->adapter_type == DIVA_ADAPTER_TYPE_HM) {
			return(0);    
		}
		
#if !defined(LINUX)

		/*  
			create the top of the registry tree as non volatile to avoid 
			clashes with the configuration of eiconcards.
			These do not expect the tree to be volatile.
		*/

		if (RegCreateKeyEx (HKEY_LOCAL_MACHINE,
					DIVA_PUBLIC_DIALOGIC_REGISTRY_ROOT,
					0,
					0,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					0,
					&tmpkey,
					0) != ERROR_SUCCESS) {
			return -1;
		}
		RegCloseKey (tmpkey);
#endif		
		
		if (pA->adapter_type==DIVA_ADAPTER_TYPE_ANALOG ) key_count = pA->channels;
    
  		for (i=0; i < key_count; i++) {

			pA->board_nr = 0;
			if (pA->adapter_type < DIVA_ADAPTER_TYPE_SOFTIP) { // = PRI, BRI or ANALOG
				pA->board_adapter_nr = 0;

				if (!pA->board_nr) {
					pA->board_nr = pA->adapter_nr;
					sprintf (boardname, "%s%sadapter%d", DIVA_PUBLIC_HW_BASE, path_separator, pA->board_nr);
					if (RegCreateKeyEx (HKEY_LOCAL_MACHINE,
                            boardname,
                            0,
#if !defined(LINUX)
                            0,
                            REG_OPTION_VOLATILE,
                            KEY_ALL_ACCESS,
#endif
                            0,
                              &pA->BoardKey,
                            0) != ERROR_SUCCESS) {
						return (-1);
					}
				}
			}
      
			if (pA->adapter_type==DIVA_ADAPTER_TYPE_ANALOG ) {
				sprintf (name, "%s%d.%d", DIVA_PUBLIC_ADAPTER_IFC_BASE, pA->adapter_nr, i+1);
			} else if (pA->adapter_type==DIVA_ADAPTER_TYPE_SOFTIP ) {
				sprintf (name, "%s_softip", DIVA_PUBLIC_ADAPTER_IFC_BASE);
			} else {
				sprintf (name, "%s%d", DIVA_PUBLIC_ADAPTER_IFC_BASE, pA->adapter_nr);
			}

			if (RegCreateKeyEx (HKEY_LOCAL_MACHINE,
								name,
								0,
#if !defined(LINUX)
								0,
								REG_OPTION_VOLATILE,
								KEY_ALL_ACCESS,
#endif
								0,
								&pA->AdapterKey[i],
								0) != ERROR_SUCCESS) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				return (-1);
			}
			if (write_ascii_reg_value (pA->AdapterKey[i], "name", pA->hLib->adapter_name)) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}

			if (write_ascii_reg_value (pA->AdapterKey[i], "displayname", pA->hLib->adapter_display_name)) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
			if (write_ascii_reg_value (pA->AdapterKey[i],
									 "ifcstate description",
									 diva_get_interface_state_description ())) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
			if (write_ascii_reg_value (pA->AdapterKey[i],
									 "resource description",
									 diva_get_resource_description ())) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
			{
				char buffer[24];
				sprintf (buffer, "%u", pA->hLib->adapter_serial_number);

				if (write_ascii_reg_value (pA->AdapterKey[i], "serial", buffer)) {
					RegCloseKey (pA->BoardKey);
					RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
					RegCloseKey (pA->AdapterKey[i]);
					RegDeleteKey (HKEY_LOCAL_MACHINE, name);
					return (-1);
				}
			}
			if (RegCreateKeyEx (pA->AdapterKey[i],
								"channels",
								0,
					  		    0,
#if !defined(LINUX)
								REG_OPTION_VOLATILE,
								KEY_ALL_ACCESS,
								0,
#endif
								&pA->ChannelsKey[i],
								0) != ERROR_SUCCESS) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
		    if (write_ascii_reg_value (pA->ChannelsKey[i], "description", diva_get_call_record_description ())) {
	            RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->ChannelsKey[i]);
				RegDeleteKey (pA->AdapterKey[i], "channels");
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
			if (RegCreateKeyEx (pA->AdapterKey[i],
								"info",
								0,
								0,
#if !defined(LINUX)
								REG_OPTION_VOLATILE,
								KEY_ALL_ACCESS,
								0,
#endif
								&pA->InfoKey[i],
								0) != ERROR_SUCCESS) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->ChannelsKey[i]);
				RegDeleteKey (pA->AdapterKey[i], "channels");
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}
			if (write_ascii_reg_value (pA->InfoKey[i], "Config description", diva_get_interface_info_description ()) || diva_write_config (pA, i+1) ) {
				RegCloseKey (pA->BoardKey);
				RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
				RegCloseKey (pA->InfoKey[i]);
				RegDeleteKey (pA->AdapterKey[i], "info");
				RegCloseKey (pA->ChannelsKey[i]);
				RegDeleteKey (pA->AdapterKey[i], "channels");
				RegCloseKey (pA->AdapterKey[i]);
				RegDeleteKey (HKEY_LOCAL_MACHINE, name);
				return (-1);
			}

			if (pA->adapter_type==DIVA_ADAPTER_TYPE_SOFTIP ) {
				if (RegCreateKeyEx (pA->AdapterKey[i],
									"registrar",
									0,
									0,
#if !defined(LINUX)
									REG_OPTION_VOLATILE,
									KEY_ALL_ACCESS,
									0,
#endif
									&pA->RegistrarKey,
									0) != ERROR_SUCCESS) {
					RegCloseKey (pA->BoardKey);
					RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
					RegCloseKey (pA->InfoKey[i]);
					RegDeleteKey (pA->AdapterKey[i], "info");
					RegCloseKey (pA->ChannelsKey[i]);
					RegDeleteKey (pA->AdapterKey[i], "channels");
					RegCloseKey (pA->AdapterKey[i]);
					RegDeleteKey (HKEY_LOCAL_MACHINE, name);
					return (-1);
				}
				if (write_ascii_reg_value (pA->RegistrarKey, "Registrar description", diva_get_registrar_description ()) ) {
					RegCloseKey (pA->BoardKey);
					RegDeleteKey (HKEY_LOCAL_MACHINE, boardname);
					RegCloseKey (pA->RegistrarKey);
					RegDeleteKey (pA->AdapterKey[i], "registrar");
					RegCloseKey (pA->InfoKey[i]);
					RegDeleteKey (pA->AdapterKey[i], "info");
					RegCloseKey (pA->ChannelsKey[i]);
					RegDeleteKey (pA->AdapterKey[i], "channels");
					RegCloseKey (pA->AdapterKey[i]);
					RegDeleteKey (HKEY_LOCAL_MACHINE, name);
					return (-1);
				}
			}
		}
		pA->ifc_available = 1;
		return (0);
	}
	return (-1);
}

static int diva_destroy_registry_adapter_tree (diva_adapter_t* pA) {
#if !defined(LINUX)
	int ret = 1;

	if (pA->ifc_available) {
		char name [sizeof(DIVA_PUBLIC_ADAPTER_IFC_BASE)+24];
		int key_count = 1;
		int i;

		/*
			Delete values to clean-up the interface if we can not
			remove keys later.
			*/

		if (pA->adapter_type==DIVA_ADAPTER_TYPE_ANALOG ) key_count = pA->channels;

		for (i=0; i < key_count; i++) {
			RegDeleteValue (pA->AdapterKey[i], "name");
			RegDeleteValue (pA->AdapterKey[i], "displayname");
			RegDeleteValue (pA->AdapterKey[i], "serial");
			RegDeleteValue (pA->ChannelsKey[i], "description");
			RegDeleteValue (pA->AdapterKey[i], "ifcstate");
			RegDeleteValue (pA->AdapterKey[i], "ifcstate description");

			{
				int j;
				char buffer[24];

				for (j = 0; j < 32; j++) {
					sprintf (buffer, "channel%d", j);
					RegDeleteValue (pA->ChannelsKey[i], buffer);
				}
			}

			RegCloseKey (pA->InfoKey[i]);
			RegDeleteKey (pA->AdapterKey[i], "info");
			RegCloseKey (pA->ChannelsKey[i]);
			RegDeleteKey (pA->AdapterKey[i], "channels");
			RegCloseKey (pA->AdapterKey[i]);

			if (pA->adapter_type==DIVA_ADAPTER_TYPE_ANALOG ) {
				sprintf (name, "adapter%d.%d", pA->adapter_nr, i+1);
				RegDeleteValue (pA->BoardKey, name);
				sprintf (name, "%s%d.%d", DIVA_PUBLIC_ADAPTER_IFC_BASE, pA->adapter_nr, i+1);
			} else if (pA->adapter_type==DIVA_ADAPTER_TYPE_SOFTIP ) {
				sprintf (name, "%s_softip", DIVA_PUBLIC_ADAPTER_IFC_BASE);
			} else {
				sprintf (name, "adapter%d", pA->adapter_nr);
				RegDeleteValue (pA->BoardKey, name);
				sprintf (name, "%s%d", DIVA_PUBLIC_ADAPTER_IFC_BASE, pA->adapter_nr);
			}
				
			if (pA->adapter_type!=DIVA_ADAPTER_TYPE_SOFTIP ) {
				int in_use = 0;

				diva_adapter_t* qA = pA;
				while (!in_use && (qA = (diva_adapter_t*)diva_q_get_prev(&qA->link))) {
					if (qA->board_nr == pA->board_nr) {
						in_use = 1;
					}
				}
				qA = pA;
				while (!in_use && (qA = (diva_adapter_t*)diva_q_get_next(&qA->link))) {
					if (qA->board_nr == pA->board_nr) {
						in_use = 1;
					}
				}
				if (!in_use) {
				char boardname [sizeof(DIVA_PUBLIC_HW_BASE)+24];
					sprintf (boardname, "%s%cadapter%d", DIVA_PUBLIC_HW_BASE, path_separator, pA->board_nr);
					RegCloseKey (pA->BoardKey);
				ret = (RegDeleteKey (HKEY_LOCAL_MACHINE, boardname) != ERROR_SUCCESS);
				}
				pA->board_nr = 0;
			}

			ret = (RegDeleteKey (HKEY_LOCAL_MACHINE, name) != ERROR_SUCCESS);
		}
		pA->ifc_available = 0;
	}

	return (ret ? (-1) : 0);

#else
	return (0);
#endif
}
static var_list_ele_t * find_alloc_list_ele(key_ele_t *ConfKey, const char* name){
	var_list_ele_t * tmp_ele_p;
	var_list_ele_t * ret_elep = 0;
	var_list_ele_t * ele_new;

	if(ConfKey->list_head!=0){
		for(tmp_ele_p = ConfKey->list_head; tmp_ele_p; tmp_ele_p=tmp_ele_p->next){
			if(!strcmp(tmp_ele_p->name, name)){
				ret_elep=tmp_ele_p;
				ret_elep->touch=TRUE;
				return(ret_elep);
			}
		}
	}

	if((ele_new = (var_list_ele_t *) HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(var_list_ele_t)))==NULL){
		perror("find_alloc_value HeapAlloc: ");
		return(0);
	}
	ele_new->touch=TRUE;
	ele_new->prev=0;
	snprintf(ele_new->name, sizeof(ele_new->name), "%s", name);
	if(ConfKey->list_head==0){
		ConfKey->list_head=ele_new;
		ele_new->next=0;
	}
	else{
		ele_new->next=ConfKey->list_head;
		ConfKey->list_head->prev=ele_new;
		ConfKey->list_head=ele_new;
	}
	return(ele_new);
}

static int write_ascii_value(key_ele_t *ConfKey, const char* name, const char* data){
	char path[MAX_PATH+1];
	var_list_ele_t* ele; 
	FILE* fs;

	if((ele=find_alloc_list_ele(ConfKey, name))==0)
		perror("write_ascii_value: ");
	sprintf(path, "%s%s%s", ConfKey->path, path_separator, name);
	if ((fs = fopen (path, "w"))) {
		fprintf (fs, "%s\n", data);
		fflush (fs);
		fclose (fs);
		return(ERROR_SUCCESS);
	}
	return(ERROR_SUCCESS);
}

static int write_ascii_reg_value (HKEY key, const char* name, const char* data) {

 	if (data) {
		return ((RegSetValueEx (key, name, 0, REG_SZ, (const byte *)data, (DWORD)(strlen(data)+1)) != ERROR_SUCCESS) ? (-1) : 0);
	} else {
		return ((RegDeleteValue (key, name) != ERROR_SUCCESS) ? (-1) : 0);
	}
}

int diva_write_interface_state (FILE** Log, int adapter_number, int channel_number, const char* data) {
	user_context_t* user_context = (user_context_t*)Log;
	diva_entity_queue_t* q = ((user_context_t*)user_context)->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_find(q, &adapter_number, cmp_adapter_nr);
	int ret = -1;

	if (pA && !pA->diva_strace_initialised) {
	   /* first invocation of callback. initialisation is complete */
	   pA->diva_strace_initialised++;
	   pA->adapter_type = diva_log_get_adapter_type (adapter_number);
	   pA->channels = diva_log_get_channel_count (adapter_number);
	   return (0);
	}

	if (pA && pA->ifc_available) {
		ret = write_ascii_reg_value (pA->AdapterKey[channel_number-1], "ifcstate", data);
		if (pA->adapter_type==DIVA_ADAPTER_TYPE_PRI ) {
			diva_ifc_config_t * p = ((diva_ifc_config_t *) diva_log_get_adapter_config_ptr (pA->adapter_nr));
			ret += write_ascii_reg_value (pA->InfoKey[channel_number-1], "Red Alarm", (p->alarm_red)?"TRUE":0);
			ret += write_ascii_reg_value (pA->InfoKey[channel_number-1], "Yellow Alarm", (p->alarm_yellow)?"TRUE":0);
			ret += write_ascii_reg_value (pA->InfoKey[channel_number-1], "Blue Alarm", (p->alarm_blue)?"TRUE":0);
		}
	}

	return (ret);
}

int diva_write_adapter_resource (FILE** Log, int adapter_number, const char* data) {
	user_context_t* user_context = (user_context_t*)Log;
	diva_entity_queue_t* q = ((user_context_t*)user_context)->adapters;
	diva_adapter_t* pA = (diva_adapter_t*)diva_q_find(q, &adapter_number, cmp_adapter_nr);
	int ret = -1;

	if (pA != 0 && pA->diva_strace_initialised != 0 && pA->ifc_available != 0) {
		ret = write_ascii_reg_value (pA->AdapterKey[0], "resource", data);
	}

	return (ret);
}

int diva_eventlog_layer1 (FILE** Log, int adapter_number, int channel_number, diva_log_ifc_info_t* pInfo) {
diva_trace_interface_state_t * p = (diva_trace_interface_state_t *) &pInfo->state[channel_number-1];
#if !defined(LINUX)
	TCHAR   szMsg2[16], szMsg3[16];
	HANDLE  hEventSource;
	LPTSTR  lpszStrings[3];

	dwErr = GetLastError();
	hEventSource = RegisterEventSource(NULL, TEXT("Dimaint"));

	if (hEventSource != NULL) {

		_stprintf(szMsg2, TEXT("%d"), channel_number);
		_stprintf(szMsg3, TEXT("Adapter %d"), adapter_number);
		lpszStrings[0] = NULL;
		lpszStrings[1] = szMsg2;
		lpszStrings[2] = szMsg3;

		if (p && (strstr(p->Layer1,"Down") != NULL)) { 

		ReportEvent (hEventSource, // handle of event source
								 EVENTLOG_ERROR_TYPE,  // event type
								 0,                    // event category
								 (DWORD) DIEVENT_ERROR_L1, // event ID
								 NULL,                 // current user's SID
								 3,                    // strings in lpszStrings
								 0,                    // no bytes of raw data
								 lpszStrings,          // array of error strings
								 NULL);                // no raw data
		} else {
			ReportEvent (hEventSource, // handle of event source
								 EVENTLOG_INFORMATION_TYPE,  // event type
								 0,                    // event category
								 (DWORD) DIEVENT_INFO_L1, // event ID
								 NULL,                 // current user's SID
								 3,                    // strings in lpszStrings
								 0,                    // no bytes of raw data
								 lpszStrings,          // array of error strings
								 NULL);                // no raw data
		}
		(void)DeregisterEventSource (hEventSource);
	}
#else
	syslog (LOG_INFO, "Diva Info: Layer 1 %s on adapter %d line %d\n", p ? p->Layer1 : "changed", adapter_number, channel_number);
#endif

  return (0);
}


#if defined(LINUX)
static int create_pid_file (int create) {
	char name[64];

	sprintf (name, "/var/run/divalogd.pid");

	if (create) {
		FILE* fs;
		if ((fs = fopen (name, "w"))) {
			fprintf (fs, "%ld\n", (long int)getpid());
			fflush (fs);
			fclose (fs);
		} else {
			return (-1);
		}
	} else {
		unlink (name);
	}

	return (0);
}

static int cmp_registry_key (const void* what, const diva_entity_link_t* p) {
	registry_key* pK = (registry_key*)p;
	HKEY key = *(const HKEY*)what;

	return (key != pK->key);
}

static HKEY get_next_key() {
	HKEY ret = (HKEY) 1;
	registry_key* pK = (registry_key*)diva_q_get_head(&registry_keys);
	while (pK != 0) {
		if (pK->key>=ret) {
			ret = pK->key+1;
		}   
		pK = (registry_key*)diva_q_get_next(&pK->link);
	}
	return(ret);
}

long RegCreateBoardDir(HKEY hKey,char* lpSubKey,DWORD Reserved,LPTSTR lpClass,char* path,DWORD* lpdwDisposition) {
	char tmp_path[MAX_PATH+1];

	strcpy(tmp_path, lpSubKey);
	if (hKey == HKEY_LOCAL_MACHINE) {
		char *ps, *qs;
		if (!*registry_path) {
			return(!ERROR_SUCCESS);
		}
		qs = tmp_path+1;
		while ((ps=strchr(qs, '/'))) {
			sprintf(path, "%s%s", registry_path, tmp_path);
			path[strlen(registry_path)+ps-qs+1] = 0;
			mkdir(path, 0777);
			qs = ps+1;
		}
		sprintf(path, "%s%s", registry_path, tmp_path);
	} 
	else
		sprintf(path, "%s%s%s", board_conf[(int)hKey-1]->ConfKey.path, path_separator, tmp_path);
	mkdir(path, 0777);
	return(ERROR_SUCCESS);
	
}

long RegCreateKeyEx(HKEY hKey,char* lpSubKey,DWORD Reserved,LPTSTR lpClass,HKEY* phkResult,DWORD* lpdwDisposition) {
	registry_key* pK = (registry_key*)HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*pK));
	char path[MAX_PATH+1];
	if (hKey == HKEY_LOCAL_MACHINE) {
		char *ps, *qs;
		if (!*registry_path) {
			return(!ERROR_SUCCESS);
		}
		strcpy(pK->path, lpSubKey);
		qs = pK->path+1;
		while ((ps=strchr(qs, '/'))) {
			sprintf(path, "%s%s", registry_path, pK->path);
			path[strlen(registry_path)+ps-qs+1] = 0;
			mkdir(path, 0777);
			qs = ps+1;
		}
	} else {
		registry_key* pParentK = (registry_key*)diva_q_find(&registry_keys, &hKey, cmp_registry_key);
		if (!pParentK) {
			printf("hKey not found!\n");
			return(!ERROR_SUCCESS);
		}
		sprintf(pK->path, "%s/%s", pParentK->path, lpSubKey);
	}
	sprintf(path, "%s%s", registry_path, pK->path);
	mkdir(path, 0777);
	pK->key = get_next_key();
	diva_q_add_tail(&registry_keys, &pK->link);
	*phkResult = pK->key;
	 return(ERROR_SUCCESS);
}

long RegSetValueEx(HKEY hKey,const char* lpValueName,DWORD Reserved,DWORD dwType,const char* lpData,DWORD cbData) {
	char path[MAX_PATH+1];
	registry_key* pParentK = (registry_key*)diva_q_find(&registry_keys, &hKey, cmp_registry_key);
	FILE* fs;
	if (!pParentK) {
		printf("hKey not found!\n");
		return(!ERROR_SUCCESS);
	}
	sprintf(path, "%s%s/%s", registry_path, pParentK->path, lpValueName);
	if ((fs = fopen (path, "w"))) {
		fprintf (fs, "%s\n", lpData);
		fflush (fs);
		fclose (fs);
	}      
	return(ERROR_SUCCESS);
}

long RegDeleteValue(HKEY hKey,const char* lpValueName) {
	char path[MAX_PATH+1];
	registry_key* pParentK = (registry_key*)diva_q_find(&registry_keys, &hKey, cmp_registry_key);
	if (!pParentK) {
		printf("hKey not found!\n");
		return(!ERROR_SUCCESS);
	}
	sprintf(path, "%s%s/%s", registry_path, pParentK->path, lpValueName);
	DeleteFile(path);
	return(ERROR_SUCCESS);
}

long RegCloseKey(HKEY hKey) {
	registry_key* pK = (registry_key*)diva_q_find(&registry_keys, &hKey, cmp_registry_key);
	if (!pK) {
		printf("hKey not found!\n");
		return(!ERROR_SUCCESS);
	}
	diva_q_remove(&registry_keys, &pK->link);
	HeapFree (GetProcessHeap(), 0, pK);
	return(ERROR_SUCCESS);
}

long RegDeleteKey(HKEY hKey,char* lpSubKey) {
	char path[MAX_PATH+1];
	registry_key* pK = (registry_key*)diva_q_find(&registry_keys, &hKey, cmp_registry_key);
	if (!pK) {
		printf("hKey not found!\n");
		return(!ERROR_SUCCESS);
	}
	sprintf(path, "%s%s/%s", registry_path, pK->path, lpSubKey);
	rmdir(path);
	return(ERROR_SUCCESS);
}
#endif
