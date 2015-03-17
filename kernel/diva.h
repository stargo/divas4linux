/* $Id: diva.h,v 1.1.2.2 2001/02/08 12:25:43 armin Exp $ */

#ifndef __DIVA_XDI_OS_PART_H__
#define __DIVA_XDI_OS_PART_H__


int divasa_xdi_driver_entry(void);
void divasa_xdi_driver_unload(void);
void *diva_driver_add_card(void *pdev, unsigned long CardOrdinal, int msi);
void diva_driver_remove_card(void *pdiva, int* msi);
int diva_driver_stop_card_no_io (void* pdiva);

typedef int (*divas_xdi_copy_to_user_fn_t) (void *os_handle, void *dst,
					    const void *src, int length);

typedef int (*divas_xdi_copy_from_user_fn_t) (void *os_handle, void *dst,
					      const void *src, int length);

int diva_xdi_read(void *adapter, void *os_handle, void *dst,
		  int max_length, divas_xdi_copy_to_user_fn_t cp_fn);

int diva_xdi_write(void *adapter, void *os_handle, const void *src,
		   int length, divas_xdi_copy_from_user_fn_t cp_fn);

void *diva_xdi_open_adapter(void *os_handle, const void *src,
			    int length,
			    divas_xdi_copy_from_user_fn_t cp_fn);

void diva_xdi_close_adapter(void *adapter, void *os_handle);

typedef enum _diva_user_mode_event_type {
	DivaUserModeEventTypeNone,
	DivaUserModeEventTypeDebug,
	DivaUserModeEventTypeNewAdapter,
	DivaUserModeEventTypeAdapterRemoved
} diva_user_mode_event_type_t;

void diva_os_shedule_user_mode_event (dword adapter_nr, diva_user_mode_event_type_t event_type);

#endif
