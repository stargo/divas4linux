
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

#define CARDTYPE_H_WANT_DATA            1
#define CARDTYPE_H_WANT_IDI_DATA        0
#define CARDTYPE_H_WANT_RESOURCE_DATA   0
#define CARDTYPE_H_WANT_FILE_DATA       0

#include "platform.h"
#include "debuglib.h"
#include "cardtype.h"
#include "dlist.h"
#include "pc.h"
#include "di_defs.h"
#include "di.h"
#include "io.h"
#include "pc_maint.h"
#include "mi_pc.h"
#include "xdi_msg.h"
#include "xdi_adapter.h"
#include "diva_pci.h"
#include "diva.h"
#include "divasync.h"
#include "pc_init.h"
#include "diva_autoconf.h"

#ifdef CONFIG_ISDN_DIVAS_PRIPCI
#include "os_pri.h"
#include "os_pri3.h"
#include "os_4pri.h"
#include "os_4prie.h"
#include "os_4prile.h"
#endif
#ifdef CONFIG_ISDN_DIVAS_BRIPCI
#include "os_bri.h"
#include "os_4bri.h"
#endif
#ifdef CONFIG_ISDN_DIVAS_ANALOG
#include "os_analog.h"
#endif

PISDN_ADAPTER IoAdapters[MAX_ADAPTER];
extern IDI_CALL Requests[MAX_ADAPTER];
extern int create_adapter_proc(diva_os_xdi_adapter_t * a);
extern void remove_adapter_proc(diva_os_xdi_adapter_t * a);

#define DivaIdiReqFunc(N) \
static void DivaIdiRequest##N(ENTITY *e) \
{ if ( IoAdapters[N] ) (* IoAdapters[N]->DIRequest)(IoAdapters[N], e) ; }

/*
**  Create own 32 Adapters
*/
DivaIdiReqFunc(0)
DivaIdiReqFunc(1)
DivaIdiReqFunc(2)
DivaIdiReqFunc(3)
DivaIdiReqFunc(4)
DivaIdiReqFunc(5)
DivaIdiReqFunc(6)
DivaIdiReqFunc(7)
DivaIdiReqFunc(8)
DivaIdiReqFunc(9)
DivaIdiReqFunc(10)
DivaIdiReqFunc(11)
DivaIdiReqFunc(12)
DivaIdiReqFunc(13)
DivaIdiReqFunc(14)
DivaIdiReqFunc(15)
DivaIdiReqFunc(16)
DivaIdiReqFunc(17)
DivaIdiReqFunc(18)
DivaIdiReqFunc(19)
DivaIdiReqFunc(20)
DivaIdiReqFunc(21)
DivaIdiReqFunc(22)
DivaIdiReqFunc(23)
DivaIdiReqFunc(24)
DivaIdiReqFunc(25)
DivaIdiReqFunc(26)
DivaIdiReqFunc(27)
DivaIdiReqFunc(28)
DivaIdiReqFunc(29)
DivaIdiReqFunc(30)
DivaIdiReqFunc(31)
DivaIdiReqFunc(32)
DivaIdiReqFunc(33)
DivaIdiReqFunc(34)
DivaIdiReqFunc(35)
DivaIdiReqFunc(36)
DivaIdiReqFunc(37)
DivaIdiReqFunc(38)
DivaIdiReqFunc(39)
DivaIdiReqFunc(40)
DivaIdiReqFunc(41)
DivaIdiReqFunc(42)
DivaIdiReqFunc(43)
DivaIdiReqFunc(44)
DivaIdiReqFunc(45)
DivaIdiReqFunc(46)
DivaIdiReqFunc(47)
DivaIdiReqFunc(48)
DivaIdiReqFunc(49)
DivaIdiReqFunc(50)
DivaIdiReqFunc(51)
DivaIdiReqFunc(52)
DivaIdiReqFunc(53)
DivaIdiReqFunc(54)
DivaIdiReqFunc(55)
DivaIdiReqFunc(56)
DivaIdiReqFunc(57)
DivaIdiReqFunc(58)
DivaIdiReqFunc(59)
DivaIdiReqFunc(60)
DivaIdiReqFunc(61)
DivaIdiReqFunc(62)
DivaIdiReqFunc(63)


struct pt_regs;

/*
**  LOCALS
*/
diva_entity_queue_t adapter_queue;

typedef struct _diva_hotplug_wait_entry {
	diva_entity_link_t link;
	diva_os_xdi_adapter_t *pdiva;
	void* pdev;
	unsigned long CardOrdinal;
	int msi;
} diva_hotplug_wait_entry_t;
static diva_entity_queue_t hotplug_wait_q;

typedef struct _diva_get_xlog {
	word command;
	byte req;
	byte rc;
	byte data[sizeof(struct mi_pc_maint)];
} diva_get_xlog_t;

typedef struct _diva_supported_cards_info {
	int CardOrdinal;
	diva_init_card_proc_t init_card;
} diva_supported_cards_info_t;

static diva_supported_cards_info_t divas_supported_cards[] = {
#ifdef CONFIG_ISDN_DIVAS_PRIPCI
	/*
	   PRI Cards
	 */
//	{CARDTYPE_DIVASRV_P_30M_PCI, diva_pri_init_card},
	/*
	   PRI Rev.2 Cards
	 */
//	{CARDTYPE_DIVASRV_P_30M_V2_PCI, diva_pri_init_card},
	/*
	   PRI Rev.2 VoIP Cards
	 */
//	{CARDTYPE_DIVASRV_VOICE_P_30M_V2_PCI, diva_pri_init_card},
	/*
	   PRI Rev.3 Cards
	 */
	{CARDTYPE_DIVASRV_P_30M_V30_PCI, diva_pri_v3_init_card},
	/*
	   PRI L-Serie
	 */
	{ CARDTYPE_DIVA_L_P_V10_PCIE, diva_pri_v3_init_card},
	/*
	   4PRI cards
	 */
	{CARDTYPE_DIVASRV_2P_V_V10_PCI, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_2P_M_V10_PCI, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_4P_V_V10_PCI, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_4P_M_V10_PCI, diva_4pri_init_card},
	/*
	   IPMedia cards
	 */
	{CARDTYPE_DIVASRV_IPMEDIA_300_V10, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_IPMEDIA_600_V10, diva_4pri_init_card},
	/*
	   4PRI PCIe cards
	 */
	{CARDTYPE_DIVASRV_V4P_V10H_PCIE, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_V2P_V10H_PCIE, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_V1P_V10H_PCIE, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM, diva_4pri_init_card},
	{CARDTYPE_DIVASRV_V1P_V10H_PCIE_HYPERCOM, diva_4pri_init_card},

  {CARDTYPE_DIVASRV_V8P_V10Z_PCIE, diva_4prie_init_card},
  {CARDTYPE_DIVASRV_V4P_V10Z_PCIE, diva_4prie_init_card},
  {CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM, diva_4prie_init_card},
  {CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM, diva_4prie_init_card},
  {CARDTYPE_DIVASRV_M8P_V10Z_PCIE, diva_4prie_init_card},
  {CARDTYPE_DIVASRV_M4P_V10Z_PCIE, diva_4prie_init_card},

	/*
		L series PCIe cards
		*/
  {CARDTYPE_DIVA_L_1P_V10_PCIE, diva_4prile_init_card},
  {CARDTYPE_DIVA_L_2P_V10_PCIE, diva_4prile_init_card},
  {CARDTYPE_DIVA_L_4P_V10_PCIE, diva_4prile_init_card},
  {CARDTYPE_DIVA_L_8P_V10_PCIE, diva_4prile_init_card},

#endif
#ifdef CONFIG_ISDN_DIVAS_ANALOG
	{CARDTYPE_DIVASRV_ANALOG_2PORT,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_ANALOG_4PORT,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_ANALOG_8PORT,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_2PORT, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_4PORT, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_8PORT, diva_analog_init_card},

	{CARDTYPE_DIVASRV_ANALOG_2P_PCIE,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_ANALOG_4P_PCIE,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_ANALOG_8P_PCIE,   diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE, diva_analog_init_card},

	{CARDTYPE_DIVASRV_V_ANALOG_2P_PCIE_HYPERCOM, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_4P_PCIE_HYPERCOM, diva_analog_init_card},
	{CARDTYPE_DIVASRV_V_ANALOG_8P_PCIE_HYPERCOM, diva_analog_init_card},
#endif
#ifdef CONFIG_ISDN_DIVAS_BRIPCI
	/*
	   4BRI Rev 1 Cards
	 */
//	{CARDTYPE_DIVASRV_Q_8M_PCI,       diva_4bri_init_card},
//	{CARDTYPE_DIVASRV_VOICE_Q_8M_PCI, diva_4bri_init_card},
	/*
	   4BRI Rev 2 Cards
	 */
	{CARDTYPE_DIVASRV_Q_8M_V2_PCI,       diva_4bri_init_card},
	{CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI, diva_4bri_init_card},
	{CARDTYPE_DIVASRV_V_Q_8M_V2_PCI,     diva_4bri_init_card},
	/*
	   4BRI Based BRI Rev 2 Cards
	 */
	{CARDTYPE_DIVASRV_B_2M_V2_PCI,       diva_4bri_init_card},
	{CARDTYPE_DIVASRV_B_2F_PCI,          diva_4bri_init_card},
	{CARDTYPE_DIVASRV_BRI_CTI_V2_PCI,    diva_4bri_init_card},
	{CARDTYPE_DIVASRV_VOICE_B_2M_V2_PCI, diva_4bri_init_card},
	{CARDTYPE_DIVASRV_V_B_2M_V2_PCI,     diva_4bri_init_card},
	/*
	   Diva 4BRI-8 PCIe v1
	 */
	{CARDTYPE_DIVASRV_4BRI_V1_PCIE,       diva_4bri_init_card},
	{CARDTYPE_DIVASRV_4BRI_V1_V_PCIE,     diva_4bri_init_card},
	/*
	   Diva BRI-2 PCIe v1
	 */
	{CARDTYPE_DIVASRV_BRI_V1_PCIE,        diva_4bri_init_card},
	{CARDTYPE_DIVASRV_BRI_V1_V_PCIE,      diva_4bri_init_card},
	/*
	   BRI
	 */
//	{CARDTYPE_MAESTRA_PCI, diva_bri_init_card},
#endif

	/*
	   EOL
	 */
	{-1, 0}
};

static void diva_init_request_array(void);
static void *divas_create_pci_card(int handle, void *pci_dev_handle, int msi, int dac_disabled);

static diva_os_spin_lock_t adapter_lock;

static int diva_find_free_adapters(int base, int nr, dword cardType, dword serialNumber)
{
	int i, mapEntries, found = 0, ignoreSn;
	const unsigned int* map = diva_os_get_hotplug_map (&mapEntries, &ignoreSn);

	if (mapEntries >= 2) {
		int currentController = 0, pos;

		for (pos = 0; pos < (mapEntries - 1) && map[pos] != 0 && map[pos] < CARDTYPE_MAX; pos += 2) {
			if (currentController == base) {
				if (map[pos] == cardType && (ignoreSn != 0 || map[pos+1] == serialNumber)) {
					found = 1;
					break;
				} else {
					return (-1);
				}
			} else {
				currentController += CardProperties[map[pos]].Adapters;
			}
		}
	} else {
		found = 1;
	}

	if (found == 0)
		return (-1);


	for (i = 0; i < nr; i++) {
		if (IoAdapters[base + i]) {
			return (-1);
		}
	}

	return (0);
}

/* --------------------------------------------------------------------------
    Add card to the card list
   -------------------------------------------------------------------------- */
void *diva_driver_add_card(void *pdev, unsigned long CardOrdinal, int msi, int dac_disabled)
{
	diva_os_spin_lock_magic_t old_irql;
	diva_os_xdi_adapter_t *pdiva, *pa;
	int i, j, max, nr;

	for (i = 0; divas_supported_cards[i].CardOrdinal != -1; i++) {
		if (divas_supported_cards[i].CardOrdinal == CardOrdinal) {
			if (!(pdiva = divas_create_pci_card(i, pdev, msi, dac_disabled))) {
				return (0);
			}
			switch (CardOrdinal) {
			case CARDTYPE_DIVASRV_2P_V_V10_PCI:
			case CARDTYPE_DIVASRV_2P_M_V10_PCI:
			case CARDTYPE_DIVASRV_IPMEDIA_300_V10:
			case CARDTYPE_DIVASRV_V2P_V10H_PCIE:
			case CARDTYPE_DIVASRV_V2P_V10H_PCIE_HYPERCOM:
				max = MAX_ADAPTER - 2;
				nr = 2;
				break;

			case CARDTYPE_DIVASRV_4P_V_V10_PCI:
			case CARDTYPE_DIVASRV_4P_M_V10_PCI:
			case CARDTYPE_DIVASRV_IPMEDIA_600_V10:
			case CARDTYPE_DIVASRV_V4P_V10Z_PCIE:
			case CARDTYPE_DIVASRV_V4P_V10Z_PCIE_HYPERCOM:
			case CARDTYPE_DIVASRV_M4P_V10Z_PCIE:
			case CARDTYPE_DIVASRV_V4P_V10H_PCIE:
			case CARDTYPE_DIVASRV_V4P_V10H_PCIE_HYPERCOM:
				max = MAX_ADAPTER - 4;
				nr = 4;
				break;

			case CARDTYPE_DIVASRV_V8P_V10Z_PCIE:
			case CARDTYPE_DIVASRV_V8P_V10Z_PCIE_HYPERCOM:
			case CARDTYPE_DIVASRV_M8P_V10Z_PCIE:
				max = MAX_ADAPTER - 8;
				nr = 8;
				break;

			case CARDTYPE_DIVA_L_2P_V10_PCIE:
				max = MAX_ADAPTER - 2;
				nr = 2;
				break;

			case CARDTYPE_DIVA_L_4P_V10_PCIE:
				max = MAX_ADAPTER - 4;
				nr = 4;
				break;

			case CARDTYPE_DIVA_L_8P_V10_PCIE:
				max = MAX_ADAPTER - 8;
				nr = 8;
				break;

			case CARDTYPE_DIVASRV_Q_8M_PCI:
			case CARDTYPE_DIVASRV_VOICE_Q_8M_PCI:
			case CARDTYPE_DIVASRV_Q_8M_V2_PCI:
			case CARDTYPE_DIVASRV_VOICE_Q_8M_V2_PCI:
			case CARDTYPE_DIVASRV_V_Q_8M_V2_PCI:
			case CARDTYPE_DIVASRV_4BRI_V1_PCIE:
			case CARDTYPE_DIVASRV_4BRI_V1_V_PCIE:
				max = MAX_ADAPTER - 4;
				nr = 4;
				break;

			default:
				max = MAX_ADAPTER;
				nr = 1;
			}

			diva_os_enter_spin_lock(&adapter_lock, &old_irql, "add card");

			for (i = 0; i < max; i++) {
				if (!diva_find_free_adapters(i, nr, pdiva->xdi_adapter.cardType, pdiva->xdi_adapter.serialNo)) {
					pdiva->controller = i + 1;
					pdiva->xdi_adapter.ANum = pdiva->controller;
					IoAdapters[i] = &pdiva->xdi_adapter;
					diva_os_leave_spin_lock(&adapter_lock, &old_irql, "add card");
					create_adapter_proc(pdiva);	/* add adapter to proc file system */

					DBG_LOG(("add %s:%d",
						 CardProperties
						 [CardOrdinal].Name,
						 pdiva->controller))

					diva_os_enter_spin_lock(&adapter_lock, &old_irql, "add card");
					pa = pdiva;
					for (j = 1; j < nr; j++) {	/* slave adapters, if any */
						pa = (diva_os_xdi_adapter_t *) diva_q_get_next(&pa->link);
						if (pa && !pa->interface.cleanup_adapter_proc) {
							pa->controller = i + 1 + j;
							pa->xdi_adapter.ANum = pa->controller;
							IoAdapters[i + j] = &pa->xdi_adapter;
							diva_os_leave_spin_lock(&adapter_lock, &old_irql, "add card");
							DBG_LOG(("add slave adapter (%d)",
								 pa->controller))
							create_adapter_proc(pa);	/* add adapter to proc file system */
							diva_os_enter_spin_lock(&adapter_lock, &old_irql, "add card");
						} else {
							DBG_ERR(("slave adapter problem"))
							break;
						}
					}

					diva_os_leave_spin_lock(&adapter_lock, &old_irql, "add card");

					diva_os_notify_user_mode_helper (DivaUserModeHelperEventAdapterInserted,
																						pdiva->xdi_adapter.cardType,
																						pdiva->xdi_adapter.serialNo);

					return (pdiva);
				}
			}

			diva_os_leave_spin_lock(&adapter_lock, &old_irql, "add card");

			/*
				Wait until hot plug interface updated
				*/
			{
				int nr, ignoreSn;
				const unsigned int* hotplug_map = diva_os_get_hotplug_map (&nr, &ignoreSn);

				if (hotplug_map != 0 && nr >=2 ) {
					diva_hotplug_wait_entry_t* pWait = diva_os_malloc (0, sizeof(*pWait));

					if (pWait != 0) {
						pWait->pdiva = pdiva;
						pWait->pdev = pdev;
						pWait->CardOrdinal = CardOrdinal;
						pWait->msi = msi;
						diva_os_enter_spin_lock(&adapter_lock, &old_irql, "wait card");
						diva_q_add_tail (&hotplug_wait_q, &pWait->link);
						diva_os_leave_spin_lock(&adapter_lock, &old_irql, "wait card");

						DBG_LOG(("hotplug wait for %u %u", pdiva->xdi_adapter.cardType, pdiva->xdi_adapter.serialNo))

						diva_os_notify_user_mode_helper (DivaUserModeHelperEventAdapterInsertedNoCfg,
																							pdiva->xdi_adapter.cardType,
																							pdiva->xdi_adapter.serialNo);

						return (pdiva);
					}
				}
			}

			/*
			   Not able to add adapter - remove it and return error
			 */
			DBG_ERR(("can not alloc request array"))
			diva_driver_remove_card(pdiva, 0);

			return (0);
		}
	}

	return (0);
}

/* --------------------------------------------------------------------------
    Called on driver load, MAIN, main, DriverEntry
   -------------------------------------------------------------------------- */
int divasa_xdi_driver_entry(void)
{
	int nr, pos = 0, ignoreSn;
	const unsigned int* hotplug_map = diva_os_get_hotplug_map (&nr, &ignoreSn);
	dword currentController = 0;

	diva_os_initialize_spin_lock(&adapter_lock, "adapter");
	diva_q_init(&adapter_queue);
	memset(&IoAdapters[0], 0x00, sizeof(IoAdapters));
	diva_init_request_array();

	for (pos = 0; pos < nr-1; pos += 2) {
		dword cardType     = hotplug_map[pos];
		dword serialNumber = hotplug_map[pos+1];

		if (cardType != 0 && cardType < CARDTYPE_MAX) {
			int controllers = CardProperties[cardType].Adapters;
			if (currentController + controllers <= MAX_ADAPTER) {
				dword i;

				for (i = 0; i < controllers; currentController++, i++) {
					DBG_LOG(("HotPlug map[%u:%u%s-%u]=%u", cardType, serialNumber, ignoreSn == 0 ? "" : "i", i+1, currentController))
				}
			}
		}
	}

	return (0);
}

/* --------------------------------------------------------------------------
    Remove adapter from list
   -------------------------------------------------------------------------- */
static diva_os_xdi_adapter_t *get_and_remove_from_queue(void)
{
	diva_os_spin_lock_magic_t old_irql;
	diva_os_xdi_adapter_t *a;

	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "driver_unload");

	if ((a = (diva_os_xdi_adapter_t *) diva_q_get_head(&adapter_queue)))
		diva_q_remove(&adapter_queue, &a->link);

	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "driver_unload");
	return (a);
}

/* --------------------------------------------------------------------------
    Remove card from the card list
   -------------------------------------------------------------------------- */
void diva_driver_remove_card(void *pdiva, int* msi)
{
	diva_os_spin_lock_magic_t old_irql;
	diva_os_xdi_adapter_t *a[8];
	diva_os_xdi_adapter_t *pa;
	diva_entity_link_t* link;
	int i;

	pa = a[0] = (diva_os_xdi_adapter_t *) pdiva;
	a[1] = a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 0;

	if (msi != 0) {
		*msi = pa->xdi_adapter.msi != 0;
	}

	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "remode adapter");

	for (i = 1; i < 8; i++) {
		if ((pa = (diva_os_xdi_adapter_t *) diva_q_get_next(&pa->link))
		    && !pa->interface.cleanup_adapter_proc) {
			a[i] = pa;
		} else {
			break;
		}
	}

	for (i = 0; ((i < 8) && a[i]); i++) {
		diva_q_remove(&adapter_queue, &a[i]->link);
	}

	link = diva_q_get_head(&hotplug_wait_q);
	while (link != 0) {
		diva_hotplug_wait_entry_t* pWait = DIVAS_CONTAINING_RECORD(link, diva_hotplug_wait_entry_t, link);
		if (pWait->pdiva == pdiva) {
			diva_q_remove(&hotplug_wait_q, link);
			break;
		}
		link = diva_q_get_next(link);
	}

	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "driver_unload");

	if (link != 0) {
		diva_hotplug_wait_entry_t* pWait = DIVAS_CONTAINING_RECORD(link, diva_hotplug_wait_entry_t, link);
		DBG_LOG(("remove hotplug wait %u %u", pWait->pdiva->xdi_adapter.cardType, pWait->pdiva->xdi_adapter.serialNo))
		diva_os_free (0, pWait);
	}

	(*(a[0]->interface.cleanup_adapter_proc)) (a[0]);

	for (i = 0; i < 8; i++) {
		if (a[i]) {
			if (a[i]->controller) {
				DBG_LOG(("remove adapter (%d)",
					 a[i]->controller)) IoAdapters[a[i]->controller - 1] = 0;
				remove_adapter_proc(a[i]);
			}
			diva_os_free(0, a[i]);
		}
	}
}

void diva_restart_hotplug_wait_q (void) {
	int mapEntries, ignoreSn;
	diva_entity_link_t* link;
	diva_os_spin_lock_magic_t old_irql;

	diva_os_get_hotplug_map (&mapEntries, &ignoreSn);
	if (mapEntries < 2)
		return;

	/* Clean up hotplug wait q */
	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");

	link = diva_q_get_head(&hotplug_wait_q);
	while (link != 0) {
		diva_hotplug_wait_entry_t* pWait = DIVAS_CONTAINING_RECORD(link, diva_hotplug_wait_entry_t, link);
		int i, j, max = MAX_ADAPTER - pWait->pdiva->xdi_adapter.tasks, nr = pWait->pdiva->xdi_adapter.tasks;

		link = diva_q_get_next (link);

		for (i = 0; i < max; i++) {
			if (!diva_find_free_adapters(i, nr, pWait->pdiva->xdi_adapter.cardType, pWait->pdiva->xdi_adapter.serialNo)) {
				diva_os_xdi_adapter_t *pdiva = pWait->pdiva, *pa;

				pdiva->controller = i + 1;
				pdiva->xdi_adapter.ANum = pdiva->controller;
				IoAdapters[i] = &pdiva->xdi_adapter;
				diva_q_remove (&hotplug_wait_q, &pWait->link);

				diva_os_leave_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");

				diva_os_free (0, pWait);
				create_adapter_proc(pdiva);	/* add adapter to proc file system */
				DBG_LOG(("delayed add %s:%d", CardProperties [pdiva->xdi_adapter.cardType].Name, pdiva->controller))

				diva_os_enter_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");

				pa = pdiva;
				for (j = 1; j < nr; j++) {	/* slave adapters, if any */
					pa = (diva_os_xdi_adapter_t *) diva_q_get_next(&pa->link);
					if (pa && !pa->interface.cleanup_adapter_proc) {
						pa->controller = i + 1 + j;
						pa->xdi_adapter.ANum = pa->controller;
						IoAdapters[i + j] = &pa->xdi_adapter;

						diva_os_leave_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");

						DBG_LOG(("delayed add slave adapter (%d)", pa->controller))
						create_adapter_proc(pa);	/* add adapter to proc file system */

						diva_os_enter_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");

					} else {
						DBG_ERR(("slave adapter problem"))
						break;
					}
				}
				break;
			}
		}
	}

	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "restart hotplug q");
}

int diva_driver_stop_card_no_io (void* pdiva) {
	diva_os_xdi_adapter_t *pa = (diva_os_xdi_adapter_t *)pdiva;
	int ret = -1;

	if (pa->interface.stop_no_io_proc != 0) {
		ret = (*(pa->interface.stop_no_io_proc)) (pa);
	}

	return (ret);
}

/* --------------------------------------------------------------------------
    Create diva PCI adapter and init internal adapter structures
   -------------------------------------------------------------------------- */
static void *divas_create_pci_card(int handle, void *pci_dev_handle, int msi, int dac_disabled)
{
	diva_supported_cards_info_t *pI = &divas_supported_cards[handle];
	diva_os_spin_lock_magic_t old_irql;
	diva_os_xdi_adapter_t *a;

	DBG_LOG(("found %d-%s", pI->CardOrdinal, CardProperties[pI->CardOrdinal].Name))

	if (!(a = (diva_os_xdi_adapter_t *) diva_os_malloc(0, sizeof(*a)))) {
		DBG_ERR(("A: can't alloc adapter"));
		return (0);
	}

	memset(a, 0x00, sizeof(*a));

	a->CardIndex = handle;
	a->CardOrdinal = pI->CardOrdinal;
	a->Bus = DIVAS_XDI_ADAPTER_BUS_PCI;
	a->xdi_adapter.cardType = a->CardOrdinal;
	a->xdi_adapter.msi      = (msi != 0);
	a->xdi_adapter.software_options |= ((dac_disabled == 0) ? 0 : (PCINIT_SOFTWARE_OPTION_DAC_DISABLED));
	a->resources.pci.bus = diva_os_get_pci_bus(pci_dev_handle);
	a->resources.pci.func = diva_os_get_pci_func(pci_dev_handle);
  if (diva_os_get_pci_revision_id(pci_dev_handle, &a->resources.pci.revId) != 0)
  {
    DBG_ERR(("A: error reading PCI Revision ID"));
    return (0);
  }
	a->resources.pci.hdev = pci_dev_handle;

	/*
	   Add master adapter first, so slave adapters will receive higher
	   numbers as master adapter
	 */
	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "found_pci_card");
	diva_q_add_tail(&adapter_queue, &a->link);
	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "found_pci_card");

	if ((*(pI->init_card)) (a)) {
		diva_os_enter_spin_lock(&adapter_lock, &old_irql, "found_pci_card");
		diva_q_remove(&adapter_queue, &a->link);
		diva_os_leave_spin_lock(&adapter_lock, &old_irql, "found_pci_card");
		diva_os_free(0, a);
		DBG_ERR(("A: can't get adapter resources"));
		return (0);
	}

	return (a);
}

/* --------------------------------------------------------------------------
    Called on driver unload FINIT, finit, Unload
   -------------------------------------------------------------------------- */
void divasa_xdi_driver_unload(void)
{
	diva_os_xdi_adapter_t *a;
	diva_entity_link_t* link;
	diva_os_spin_lock_magic_t old_irql;

	/* Clean up hotplug wait q */
	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "remode adapter");
	for (link = diva_q_get_head(&hotplug_wait_q); link != 0; link = diva_q_get_head(&hotplug_wait_q)) {
		diva_hotplug_wait_entry_t* pWait = DIVAS_CONTAINING_RECORD(link, diva_hotplug_wait_entry_t, link);
		diva_q_remove(&hotplug_wait_q, link);
		diva_os_leave_spin_lock(&adapter_lock, &old_irql, "driver_unload");
		diva_os_free (0, pWait);
		diva_os_enter_spin_lock(&adapter_lock, &old_irql, "remode adapter");
	}
	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "driver_unload");

	while ((a = get_and_remove_from_queue())) {
		if (a->interface.cleanup_adapter_proc) {
			(*(a->interface.cleanup_adapter_proc)) (a);
		}
		if (a->controller) {
			IoAdapters[a->controller - 1] = 0;
			remove_adapter_proc(a);
		}
		diva_os_free(0, a);
	}
	diva_os_destroy_spin_lock(&adapter_lock, "adapter");
}

/*
**  Receive and process command from user mode utility
*/
static int cmp_adapter_nr(const void *what, const diva_entity_link_t * p)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) p;
	dword nr = (dword) (unsigned long) what;

	return (nr != a->controller);
}

void *diva_xdi_open_adapter(void *os_handle, const void *src,
			    int length,
			    divas_xdi_copy_from_user_fn_t cp_fn)
{
	diva_xdi_um_cfg_cmd_t msg;
	diva_os_xdi_adapter_t *a;
	diva_os_spin_lock_magic_t old_irql;

	if (length < sizeof(diva_xdi_um_cfg_cmd_t)) {
		DBG_ERR(("A: A(?) open, msg too small (%d < %d)",
			 length, sizeof(diva_xdi_um_cfg_cmd_t)))
		return (0);
	}
	if ((*cp_fn) (os_handle, &msg, src, sizeof(msg)) <= 0) {
		DBG_ERR(("A: A(?) open, write error"))
		return (0);
	}
	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "open_adapter");
	a = (diva_os_xdi_adapter_t *) diva_q_find(&adapter_queue,
						  (void *) (unsigned long)
						  msg.adapter,
						  cmp_adapter_nr);
	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "open_adapter");

	if (!a) {
		DBG_ERR(("A: A(%d) open, adapter not found", msg.adapter))
	}

	return (a);
}

/*
**  Easy cleanup mailbox status
*/
void diva_xdi_close_adapter(void *adapter, void *os_handle)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) adapter;

	a->xdi_mbox.status &= ~DIVA_XDI_MBOX_BUSY;
	if (a->xdi_mbox.data) {
		diva_os_free(0, a->xdi_mbox.data);
		a->xdi_mbox.data = 0;
	}
}

int diva_xdi_write_direct (void *os_handle,
													 const void *src,
													 int length,
													 divas_xdi_copy_from_user_fn_t cp_fn) {
	diva_xdi_direct_access_cmd_t cmd;
	int ret = -1;

	if (unlikely(length > sizeof(cmd) || length < sizeof(cmd.hdr)+sizeof(dword)))
		return (-1);

	if ((*cp_fn) (os_handle, &cmd, src, length) != length)
		return (-1);

	if (unlikely(cmd.hdr.ident != DIVA_XDI_DIRECT_ACCESS_CMD_IDENT))
		return (-1);

	if (cmd.hdr.cmd == DIVA_XDI_DIRECT_ACCESS_CMD_WRITE_BY_ADDRESS &&
			sizeof(cmd.cmd.write_by_address) == length - sizeof(cmd.hdr) &&
			(cmd.cmd.write_by_address.address & 0x3U) == 0) {
		diva_os_xdi_adapter_t *a;
		diva_os_spin_lock_magic_t old_irql;

		diva_os_enter_spin_lock(&adapter_lock, &old_irql, "write_direct");

		for (a = (diva_os_xdi_adapter_t *) diva_q_get_head(&adapter_queue);
				 a != 0;
				 a = (diva_os_xdi_adapter_t *)diva_q_get_next(&a->link)) {
			if (a->xdi_adapter.ControllerNumber == 0 && a->xdi_adapter.Initialized != 0 && a->xdi_adapter.sdram_bar != 0) {
				dword start = a->xdi_adapter.sdram_bar;
				dword end   = a->xdi_adapter.sdram_bar + a->xdi_adapter.AdapterTestMemoryLength;
				dword dst   = cmd.cmd.write_by_address.address;

				if (dst > start && dst < end) {
					dword offset = dst - start;
					volatile dword* vdst = (dword*)(((byte*)(a->xdi_adapter.AdapterTestMemoryStart)) + offset);

					*vdst = cmd.cmd.write_by_address.value;
					ret = length;
					break;
				}
			}
		}

		diva_os_leave_spin_lock(&adapter_lock, &old_irql, "write_direct");
	}

	return (ret);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
static int diva_get_clock_memory_info (dword* bus_addr_lo,
																			 dword* bus_addr_hi,
																			 dword* length) {
#else
static int diva_get_clock_memory_info (dword* bus_addr_lo,
																			 dword* bus_addr_hi,
																			 dword* length,
																			 void*  pci_dev_handle) {
#endif
  int mem_length = MAX(sizeof(IDI_SYNC_REQ),(sizeof(DESCRIPTOR)*MAX_DESCRIPTORS));
  byte* mem = (byte*)diva_os_malloc(0, mem_length);
	int ret = -1;

  if (mem != 0) {
		IDI_CALL dadapter_request = 0;
		DESCRIPTOR* t = (DESCRIPTOR*)mem;
		int i;

		memset (t, 0x00, sizeof(*t)*MAX_DESCRIPTORS);
		diva_os_read_descriptor_array (t, sizeof(*t)*MAX_DESCRIPTORS);

		for (i = 0; i < MAX_DESCRIPTORS; i++) {
			if (t[i].type == IDI_DADAPTER) {
				dadapter_request = t[i].request;
				break;
			}
		}

		if (dadapter_request != 0) {
			IDI_SYNC_REQ* sync_req = (IDI_SYNC_REQ*)mem;

			memset (sync_req, 0x00, sizeof(*sync_req));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,16,0)
			sync_req->xdi_clock_data.info.pci_dev_handle = pci_dev_handle;
#endif

      sync_req->xdi_clock_data.Rc = IDI_SYNC_REQ_XDI_GET_CLOCK_DATA;
			(*dadapter_request)((ENTITY*)sync_req);
      if (sync_req->xdi_clock_data.Rc == 0xff) {
				*bus_addr_lo = sync_req->xdi_clock_data.info.bus_addr_lo;
				*bus_addr_hi = sync_req->xdi_clock_data.info.bus_addr_hi;
				*length      = sync_req->xdi_clock_data.info.length;
				DBG_LOG(("%s: (%08x:%08x), length:%d",
					__FUNCTION__,
					sync_req->xdi_clock_data.info.bus_addr_lo,
					sync_req->xdi_clock_data.info.bus_addr_hi,
					sync_req->xdi_clock_data.info.length))
				ret = 0;
			}	
		}

		diva_os_free (0, mem);
	}

	return (ret);
}


static int diva_common_cmd_card_proc (struct _diva_os_xdi_adapter* a,
																			diva_xdi_um_cfg_cmd_t* cmd,
																			int length,
																			int* adapter_specific_command) {
	int ret = -1;

	if (cmd->adapter != a->controller) {
		DBG_ERR(("common_cmd, wrong controller=%d != %d", cmd->adapter, a->controller))
		return (-1);
	}

	switch (cmd->command) {
		case DIVA_XDI_UM_CMD_GET_CLOCK_MEMORY_INFO:
			DBG_LOG(("clock data cmd: adapter= %d", a->controller))
			a->xdi_mbox.data_length = sizeof(diva_xdi_um_cfg_cmd_get_clock_memory_info_t);
			if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length)) != 0) {
				diva_xdi_um_cfg_cmd_get_clock_memory_info_t* info =
									(diva_xdi_um_cfg_cmd_get_clock_memory_info_t*)a->xdi_mbox.data;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,16,0)
				if (diva_get_clock_memory_info (&info->bus_addr_lo, &info->bus_addr_hi, &info->length) == 0) {
#else
				if (diva_get_clock_memory_info (
							&info->bus_addr_lo, &info->bus_addr_hi, &info->length, a->resources.pci.hdev) == 0) {
					a->resources.pci.clock_data_bus_addr = info->bus_addr_lo; // save bus address for stop/unmap
#endif
					a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
					ret = 0;
					DBG_LOG(("%s: cmd=0x%x A(%d) (pci_dev=%p)",
						 	__FUNCTION__, cmd->command, a->controller, a->resources.pci.hdev));
					DBG_LOG(("%s: (%08x:%08x), length:%d",
						 	__FUNCTION__, info->bus_addr_lo, info->bus_addr_hi, info->length));
				} else {
					DBG_ERR(("%s xdi_clock_data, A(%d) (pci_dev=%p)", __FUNCTION__, a->controller, a->resources.pci.hdev))
					diva_os_free (0, a->xdi_mbox.data);
					a->xdi_mbox.data        = 0;
					a->xdi_mbox.data_length = 0;
				}
			}
			break;

		case DIVA_XDI_UM_CMD_READ_WRITE_PLX_REGISTER:
			if (a->xdi_adapter.ControllerNumber == 0 && a->xdi_adapter.reset != 0) {
				a->xdi_mbox.data_length = sizeof(diva_xdi_um_cfg_cmd_read_write_plx_register_t);
				if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length)) != 0) {
					diva_xdi_um_cfg_cmd_read_write_plx_register_t* info =
															(diva_xdi_um_cfg_cmd_read_write_plx_register_t*)a->xdi_mbox.data;
					diva_xdi_um_cfg_cmd_read_write_plx_register_t* data = &cmd->command_data.plx_register;
					dword length = data->length;

					info->write  = data->write;
					info->offset = data->offset;

					a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
					ret = 0;

					switch ((info->offset & 3)) {
						case 1:
						case 3:
							length = 1;
							break;

						case 2:
							length = (length > 2) ? 2 : length;
							break;

						default:
							if (length > 4 || length == 3)
								length = 4;
							break;
					}

					info->length = length;

					if (data->write != 0) {
						switch (length) {
							case 1:
								*(volatile byte*)&a->xdi_adapter.reset[data->offset]  = (byte)data->value;
								break;
							case 2:
								*(volatile word*)&a->xdi_adapter.reset[data->offset]  = (word)data->value;
								break;
							default:
								*(volatile dword*)&a->xdi_adapter.reset[data->offset] = data->value;
								break;
						}
					}

					switch (length) {
						case 1:
							info->value = *(volatile byte*)&a->xdi_adapter.reset[data->offset];
							break;
						case 2:
							info->value = *(volatile word*)&a->xdi_adapter.reset[data->offset];
							break;
						default:
							info->value = *(volatile dword*)&a->xdi_adapter.reset[data->offset];
							break;
					}
				}
			} else {
				a->xdi_mbox.data        = 0;
				a->xdi_mbox.data_length = 0;
			}
			break;

		case DIVA_XDI_UM_CMD_BAR_ACCESS:
			a->xdi_mbox.data        = 0;
			a->xdi_mbox.data_length = 0;
			if (a->xdi_adapter.ControllerNumber == 0) {
				diva_xdi_um_bar_access_data_t* data = &cmd->command_data.bar_access_data;
				void* base = 0;

				switch (data->bar)
				{
				case 0:
					base = a->xdi_adapter.reset;
					break;

				case 3:
					base = a->xdi_adapter.ctlReg;
					break;
				}
				if (!base) {
					break;
				}

				byte* const response = diva_os_malloc(0, data->length + sizeof(dword));

				if (!response) {
					break;
				}
				a->xdi_mbox.data = response;
				a->xdi_mbox.data_length = data->length + sizeof(dword);
				a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
				ret = 0;
				*(dword*) response = 0x80000000;
				if (!(data->access_type & DIVA_BAR_ACCESS_TYPE_SINGLE)) {
					break;
				}

				if ((data->access_type & DIVA_BAR_ACCESS_TYPE_WRITE) ||
						(data->access_type & DIVA_BAR_ACCESS_TYPE_WRITE_READ)) {
					switch (data->length)
					{
					case 1:
						*(volatile byte*) (base + data->offset) = (byte) data->data;
						*(dword*) response = 0;
						break;

					case 2:
						*(volatile word*) (base + data->offset) = (word) data->data;
						*(dword*) response = 0;
						break;

					case 4:
						*(volatile dword*) (base + data->offset) = data->data;
						*(dword*) response = 0;
						break;
					}
					if ((*(dword*) response & 0x80000000) != 0) {
						break;
					}
				}

				if (!(data->access_type & DIVA_BAR_ACCESS_TYPE_WRITE) ||
						(data->access_type & DIVA_BAR_ACCESS_TYPE_WRITE_READ)) {
					switch (data->length)
					{
					case 1:
						response[sizeof(dword)] = *(volatile byte*) (base + data->offset);
						*(dword*) response = 1;
						break;

					case 2:
						*(word*) &response[sizeof(dword)] = *(volatile word*) (base +data->offset);
						*(dword*) response = 2;
						break;

					case 4:
						*(dword*) &response[sizeof(dword)] = *(volatile dword*) (base + data->offset);
						*(dword*) response = 4;
						break;
					}
				}
			}
			break;

		case DIVA_XDI_UM_CMD_CLOCK_INTERRUPT_DATA:
			a->xdi_mbox.data_length = sizeof(diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t);
			if ((a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length)) != 0) {
				diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t* info =
					(diva_xdi_um_cfg_cmd_get_clock_interrupt_data_t*)a->xdi_mbox.data;

				info->state  = a->xdi_adapter.clock_isr_active;
				info->clock  = a->xdi_adapter.clock_isr_count;
				info->errors = a->xdi_adapter.clock_isr_errors;
				a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
				ret = 0;
			}
			break;

    case DIVA_XDI_UM_CMD_READ_SOFTWARE_OPTIONS:
      a->xdi_mbox.data_length = sizeof(dword);
      a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
      if (a->xdi_mbox.data) {
        *(dword*)a->xdi_mbox.data = a->xdi_adapter.software_options;
        a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
        ret = 0;
      } else {
        a->xdi_mbox.data_length = 0;
      }
      break;

    case DIVA_XDI_UM_CMD_GET_PCI_REVISION_ID:
      a->xdi_mbox.data_length = 1;
      a->xdi_mbox.data = diva_os_malloc (0, a->xdi_mbox.data_length);
      if (a->xdi_mbox.data) {
        *(byte*)a->xdi_mbox.data = a->resources.pci.revId;
        a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;
        ret = 0;
      } else {
        a->xdi_mbox.data_length = 0;
      }
      break;

    default:
      *adapter_specific_command = 1;
      break;
	}

	return (ret);
}

int
diva_xdi_write(void *adapter, void *os_handle, const void *src,
	       int length, divas_xdi_copy_from_user_fn_t cp_fn)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) adapter;
	void *data;

	if (a->xdi_mbox.status & DIVA_XDI_MBOX_BUSY) {
		DBG_ERR(("A: A(%d) write, mbox busy", a->controller))
		return (-1);
	}

	if (length < sizeof(diva_xdi_um_cfg_cmd_t)) {
		DBG_ERR(("A: A(%d) write, message too small (%d < %d)",
			 a->controller, length,
			 sizeof(diva_xdi_um_cfg_cmd_t)))
		return (-3);
	}

	if (!(data = diva_os_malloc(0, length))) {
		DBG_ERR(("A: A(%d) write, ENOMEM", a->controller))
		return (-2);
	}

	length = (*cp_fn) (os_handle, data, src, length);
	if (length > 0) {
		int adapter_specific_command = 0;
		int common_command_info = diva_common_cmd_card_proc (a,
																												 (diva_xdi_um_cfg_cmd_t*)data,
																												 length,
																												 &adapter_specific_command);
		if (adapter_specific_command == 0) {
			if (common_command_info != 0)
				length = -3;
		} else {
			if ((*(a->interface.cmd_proc))(a, (diva_xdi_um_cfg_cmd_t *) data, length)) {
				length = -3;
			}
		}
	} else {
		DBG_ERR(("A: A(%d) write error (%d)", a->controller,
			 length))
	}

	diva_os_free(0, data);

	return (length);
}

/*
**  Write answers to user mode utility, if any
*/
int
diva_xdi_read(void *adapter, void *os_handle, void *dst,
	      int max_length, divas_xdi_copy_to_user_fn_t cp_fn)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) adapter;
	int ret;

	if (!(a->xdi_mbox.status & DIVA_XDI_MBOX_BUSY)) {
		DBG_ERR(("A: A(%d) rx mbox empty", a->controller))
		return (-1);
	}
	if (!a->xdi_mbox.data) {
		a->xdi_mbox.status &= ~DIVA_XDI_MBOX_BUSY;
		DBG_ERR(("A: A(%d) rx ENOMEM", a->controller))
		return (-2);
	}

	if (max_length < a->xdi_mbox.data_length) {
		DBG_ERR(("A: A(%d) rx buffer too short(%d < %d)",
			 a->controller, max_length,
			 a->xdi_mbox.data_length))
		return (-3);
	}

	ret = (*cp_fn) (os_handle, dst, a->xdi_mbox.data,
		      a->xdi_mbox.data_length);
	if (ret > 0) {
		diva_os_free(0, a->xdi_mbox.data);
		a->xdi_mbox.data = 0;
		a->xdi_mbox.status &= ~DIVA_XDI_MBOX_BUSY;
	}

	return (ret);
}


irqreturn_t diva_os_irq_wrapper(void *context)
{
	diva_os_xdi_adapter_t *a = (diva_os_xdi_adapter_t *) context;
	diva_xdi_clear_interrupts_proc_t clear_int_proc;

	if (!a || !a->xdi_adapter.diva_isr_handler) {
		return IRQ_NONE;
	}

	if ((clear_int_proc = a->clear_interrupts_proc)) {
		(*clear_int_proc) (a);
		a->clear_interrupts_proc = 0;
		return IRQ_HANDLED;
	}

  if ((*(a->xdi_adapter.diva_isr_handler)) (&a->xdi_adapter) != 0) {
    return IRQ_HANDLED;
  } else {
    return IRQ_NONE;
  }
}

static void diva_init_request_array(void)
{
	Requests[0] = DivaIdiRequest0;
	Requests[1] = DivaIdiRequest1;
	Requests[2] = DivaIdiRequest2;
	Requests[3] = DivaIdiRequest3;
	Requests[4] = DivaIdiRequest4;
	Requests[5] = DivaIdiRequest5;
	Requests[6] = DivaIdiRequest6;
	Requests[7] = DivaIdiRequest7;
	Requests[8] = DivaIdiRequest8;
	Requests[9] = DivaIdiRequest9;
	Requests[10] = DivaIdiRequest10;
	Requests[11] = DivaIdiRequest11;
	Requests[12] = DivaIdiRequest12;
	Requests[13] = DivaIdiRequest13;
	Requests[14] = DivaIdiRequest14;
	Requests[15] = DivaIdiRequest15;
	Requests[16] = DivaIdiRequest16;
	Requests[17] = DivaIdiRequest17;
	Requests[18] = DivaIdiRequest18;
	Requests[19] = DivaIdiRequest19;
	Requests[20] = DivaIdiRequest20;
	Requests[21] = DivaIdiRequest21;
	Requests[22] = DivaIdiRequest22;
	Requests[23] = DivaIdiRequest23;
	Requests[24] = DivaIdiRequest24;
	Requests[25] = DivaIdiRequest25;
	Requests[26] = DivaIdiRequest26;
	Requests[27] = DivaIdiRequest27;
	Requests[28] = DivaIdiRequest28;
	Requests[29] = DivaIdiRequest29;
	Requests[30] = DivaIdiRequest30;
	Requests[31] = DivaIdiRequest31;
  Requests[32] = DivaIdiRequest32;
  Requests[33] = DivaIdiRequest33;
  Requests[34] = DivaIdiRequest34;
  Requests[35] = DivaIdiRequest35;
  Requests[36] = DivaIdiRequest36;
  Requests[37] = DivaIdiRequest37;
  Requests[38] = DivaIdiRequest38;
  Requests[39] = DivaIdiRequest39;
  Requests[40] = DivaIdiRequest40;
  Requests[41] = DivaIdiRequest41;
  Requests[42] = DivaIdiRequest42;
  Requests[43] = DivaIdiRequest43;
  Requests[44] = DivaIdiRequest44;
  Requests[45] = DivaIdiRequest45;
  Requests[46] = DivaIdiRequest46;
  Requests[47] = DivaIdiRequest47;
  Requests[48] = DivaIdiRequest48;
  Requests[49] = DivaIdiRequest49;
  Requests[50] = DivaIdiRequest50;
  Requests[51] = DivaIdiRequest51;
  Requests[52] = DivaIdiRequest52;
  Requests[53] = DivaIdiRequest53;
  Requests[54] = DivaIdiRequest54;
  Requests[55] = DivaIdiRequest55;
  Requests[56] = DivaIdiRequest56;
  Requests[57] = DivaIdiRequest57;
  Requests[58] = DivaIdiRequest58;
  Requests[59] = DivaIdiRequest59;
  Requests[60] = DivaIdiRequest60;
  Requests[61] = DivaIdiRequest61;
  Requests[62] = DivaIdiRequest62;
  Requests[63] = DivaIdiRequest63;
}

void diva_xdi_display_adapter_features(int card)
{
	dword features;
	if (!card || ((card - 1) >= MAX_ADAPTER) || !IoAdapters[card - 1]) {
		return;
	}
	card--;
	features = IoAdapters[card]->Properties.Features;

	DBG_LOG(("FEATURES FOR ADAPTER: %d", card + 1))
	DBG_LOG((" DI_FAX3          :  %s",
		     (features & DI_FAX3) ? "Y" : "N"))
	DBG_LOG((" DI_MODEM         :  %s",
		     (features & DI_MODEM) ? "Y" : "N"))
	DBG_LOG((" DI_POST          :  %s",
		     (features & DI_POST) ? "Y" : "N"))
	DBG_LOG((" DI_V110          :  %s",
		     (features & DI_V110) ? "Y" : "N"))
	DBG_LOG((" DI_V120          :  %s",
		     (features & DI_V120) ? "Y" : "N"))
	DBG_LOG((" DI_POTS          :  %s",
		     (features & DI_POTS) ? "Y" : "N"))
	DBG_LOG((" DI_CODEC         :  %s",
		     (features & DI_CODEC) ? "Y" : "N"))
	DBG_LOG((" DI_MANAGE        :  %s",
		     (features & DI_MANAGE) ? "Y" : "N"))
	DBG_LOG((" DI_V_42          :  %s",
		     (features & DI_V_42) ? "Y" : "N"))
	DBG_LOG((" DI_EXTD_FAX      :  %s",
		     (features & DI_EXTD_FAX) ? "Y" : "N"))
	DBG_LOG((" DI_AT_PARSER     :  %s",
		     (features & DI_AT_PARSER) ? "Y" : "N"))
	DBG_LOG((" DI_VOICE_OVER_IP :  %s",
		     (features & DI_VOICE_OVER_IP) ? "Y" : "N"))
}

void diva_add_slave_adapter(diva_os_xdi_adapter_t * a)
{
	diva_os_spin_lock_magic_t old_irql;

	diva_os_enter_spin_lock(&adapter_lock, &old_irql, "add_slave");
	diva_q_add_tail(&adapter_queue, &a->link);
	diva_os_leave_spin_lock(&adapter_lock, &old_irql, "add_slave");
}

int diva_card_read_xlog(diva_os_xdi_adapter_t * a)
{
	diva_get_xlog_t *req;
	byte *data;

	if (!a->xdi_adapter.Initialized || !a->xdi_adapter.DIRequest) {
		return (-1);
	}
	if (!(data = diva_os_malloc(0, sizeof(struct mi_pc_maint)))) {
		return (-1);
	}
	memset(data, 0x00, sizeof(struct mi_pc_maint));

	if (!(req = diva_os_malloc(0, sizeof(*req)))) {
		diva_os_free(0, data);
		return (-1);
	}
	req->command = 0x0400;
	req->req = LOG;
	req->rc = 0x00;

	if (IDI_PROP(a->CardOrdinal,Card) != CARD_PASSIVEP) {
		(*(a->xdi_adapter.DIRequest)) (&a->xdi_adapter, (ENTITY *) req);
	} else {
		DESCRIPTOR DIDD_Table[MAX_DESCRIPTORS];
		IDI_SYNC_REQ syncReq;
		IDI_CALL request;
		int x;

		diva_os_read_descriptor_array(DIDD_Table, sizeof(DIDD_Table));

		for (x = 0, request = 0; x < MAX_DESCRIPTORS && request == 0; x++) {
			if (DIDD_Table[x].channels != 0 && DIDD_Table[x].type < IDI_VADAPTER) {
				syncReq.xdi_logical_adapter_number.Req = 0;
				syncReq.xdi_logical_adapter_number.Rc  = IDI_SYNC_REQ_XDI_GET_LOGICAL_ADAPTER_NUMBER;
				syncReq.xdi_logical_adapter_number.info.logical_adapter_number = 0;
				DIDD_Table[x].request((ENTITY*)&syncReq);
				if (syncReq.xdi_logical_adapter_number.info.logical_adapter_number == a->xdi_adapter.ANum) {
					request = DIDD_Table[x].request;
				}
			}
		}

		if (request != 0) {
			request((ENTITY*)req);
		} else {
			diva_os_free(0, data);
			diva_os_free(0, req);
			return (-1);
		}
	}

	if (!req->rc || req->req) {
		diva_os_free(0, data);
		diva_os_free(0, req);
		return (-1);
	}

	memcpy(data, &req->req, sizeof(struct mi_pc_maint));

	diva_os_free(0, req);

	a->xdi_mbox.data_length = sizeof(struct mi_pc_maint);
	a->xdi_mbox.data = data;
	a->xdi_mbox.status = DIVA_XDI_MBOX_BUSY;

	return (0);
}

void xdiFreeFile(void *handle)
{
}

void diva_vidi_sync_buffers_and_start_tx_dma(PISDN_ADAPTER IoAdapter) {
  byte* req_ptr = (byte*)IoAdapter->ram;
  dword* dma_req_counter = (dword*)(req_ptr - MP_SHARED_RAM_OFFSET + IoAdapter->host_vidi.adapter_req_counter_offset);

  *(volatile dword*)dma_req_counter = (dword)IoAdapter->host_vidi.local_request_counter;
}

// vim: set tabstop=2 softtabstop=2 shiftwidth=2 :
