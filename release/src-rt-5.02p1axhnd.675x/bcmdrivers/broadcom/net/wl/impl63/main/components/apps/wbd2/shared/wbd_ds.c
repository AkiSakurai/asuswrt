/*
 * WBD data structure manipulation file
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wbd_ds.c 782305 2019-12-17 04:58:40Z $
 */

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_com.h"
#ifdef BCM_APPEVENTD
#include "wbd_appeventd.h"
#endif /* BCM_APPEVENTD */

#ifdef WLHOSTFBT
/* Check whether FBT enabling is possible or not. First it checks for psk2 and then wbd_fbt */
extern int wbd_is_fbt_possible(char *prefix);
#endif /* WLHOSTFBT */

/* =========================== UTILITY FUNCTIONS ================================ */
/* Initialize generic list */
void
wbd_ds_glist_init(wbd_glist_t *list)
{
	list->count = 0;
	dll_init(&(list->head));
}

/* Append a node to generic list */
void
wbd_ds_glist_append(wbd_glist_t *list, dll_t *new_obj)
{
	dll_append((dll_t *)&(list->head), new_obj);
	++(list->count);
}

/* Delete a node from generic list */
void
wbd_ds_glist_delete(wbd_glist_t *list, dll_t *obj)
{
	dll_delete(obj);
	--(list->count);
}

/* Delete all the node from generic list */
int
wbd_ds_glist_cleanup(wbd_glist_t *list)
{
	int ret = WBDE_OK;
	dll_t *item_p, *next_p;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(list, WBDE_INV_ARG);

	if (list->count == 0) {
		goto end;
	}

	/* Travese List */
	foreach_safe_glist_item(item_p, (*list), next_p) {

		/* need to keep next item incase we remove node in between */
		next_p = dll_next_p(item_p);

		/* Remove item itself from list */
		wbd_ds_glist_delete(list, item_p);
		free(item_p);
	}

	/* Sanity Check */
	if (list->count) {
		WBD_WARNING("Error: List count [%d] after cleanup\n", list->count);
		ret = WBDE_DS_SLV_DTCURR;
	}

end:
	WBD_EXIT();
	return ret;

}
/* =========================== UTILITY FUNCTIONS ================================ */

/* ===================== ALLOC/INIT/CLEANUP FUNCTIONS =========================== */

/* Allocate & Initialize Blanket Master structure object */
int
wbd_ds_blanket_master_init(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Allocate Master application structure */
	info->wbd_master = (wbd_blanket_master_t*)wbd_malloc(sizeof(*info->wbd_master), &ret);
	WBD_ASSERT();

	/* --- Initialize Blanket Master Application structure --- */

	/* keeps track of its parent */
	info->wbd_master->parent = info;

	/* Initialize br0 Master Info List of Blanket Master for multiple bands */
	wbd_ds_glist_init(&info->wbd_master->blanket_list);

	/* Initialize ignored MAC List of Blanket Master */
	wbd_ds_glist_init(&info->wbd_master->ignore_maclist);

	/* Initialize blanket log messages. */
	memset(&(info->wbd_master->master_logs), 0,
		sizeof(info->wbd_master->master_logs));
	info->wbd_master->master_logs.buflen = ARRAYSIZE(info->wbd_master->master_logs.logmsgs);

end:
	WBD_DS("Blanket Master Init %s\n", wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Free & Cleanup Blanket Master structure object from heap */
int
wbd_ds_blanket_master_cleanup(wbd_info_t *info)
{
	int ret = WBDE_OK;
	wbd_master_info_t* master_info;
	dll_t *master_item_p, *master_next_p;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(info->wbd_master, WBDE_INV_ARG);

	/* Remove all ignored MAC Addresses with this Master */
	wbd_ds_glist_cleanup(&info->wbd_master->ignore_maclist);

	/* Traverse br0 Master Info List for this Blanket Master */
	foreach_safe_glist_item(master_item_p, info->wbd_master->blanket_list, master_next_p) {

		/* need to keep next item incase we remove node in between */
		master_next_p = dll_next_p(master_item_p);

		master_info = (wbd_master_info_t*)master_item_p;

		/* Free & Cleanup Master Info structure for this Band */
		wbd_ds_master_info_cleanup(master_info, TRUE);

		/* In the end, Remove Slave item itself from Slave list */
		wbd_ds_glist_delete(&info->wbd_master->blanket_list, master_item_p);
		free(master_info);
		master_info = NULL;
	}

	/* cleanup bouncing table */
	wbd_ds_cleanup_sta_bounce_table(info->wbd_master);

	/* In the end, Remove Blanket Master itself */
	free(info->wbd_master);
	info->wbd_master = NULL;

end:
	WBD_DS("Blanket Master Cleanup %s\n", wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Allocate & Initialize Master Info structure object */
wbd_master_info_t*
wbd_ds_master_info_init(uint8 bkt_id, char *bkt_name, int *error)
{
	int ret = WBDE_OK;
	wbd_master_info_t *master_info = NULL;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(error, WBDE_INV_ARG);

	/* Allocate Master Info structure */
	master_info = (wbd_master_info_t*)wbd_malloc(sizeof(*master_info), &ret);
	WBD_ASSERT();

	/* --- Initialize Master Info structure --- */

	/* keeps track of its parent, Only Master Application updates it when created */
	master_info->parent = NULL;

	/* Updated Blanket ID and Name in Master */
	master_info->bkt_id = bkt_id;
	WBDSTRNCPY(master_info->bkt_name, bkt_name, sizeof(master_info->bkt_name));

	/* Updated Master's Blanket client count */
	master_info->blanket_client_count = 0;

	/* Updated Master's Weak client count, STA_REPOSTS cmd will update the same */
	master_info->weak_client_count = 0;

	/* Initialize Master's Slave list, JOIN/LEAVE cmd will update the same */
	wbd_ds_glist_init(&master_info->slave_list);

	/* Initialize AP Channel report */
	wbd_ds_glist_init(&master_info->ap_chan_report);

	/* Initialize Pointer to Slave running in Master mode, native Slave JOIN will update it */
	master_info->pm_slave = NULL;

	memset(&(master_info->fbt_info), 0, sizeof(master_info->fbt_info));

end:
	WBD_DS("BKTID[%d] BKTNAME[%s] Master Info Init %s\n", bkt_id, bkt_name, wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return master_info;
}

/* Create & Add a new Master Info to Blanket Master for specific blanket ID */
int
wbd_ds_create_master_for_blanket_id(wbd_blanket_master_t *wbd_master, uint8 bkt_id, char *bkt_name,
	wbd_master_info_t **out_master)
{
	int ret = WBDE_OK;
	wbd_master_info_t *new_master = NULL;
	WBD_ENTER();

	/* Allocate & Initialize Master Info structure for corrosponding blanket */
	new_master = wbd_ds_master_info_init(bkt_id, bkt_name, &ret);
	WBD_ASSERT();

	/* keeps track of its parent */
	new_master->parent = wbd_master;

	/* In the end, Add this new Slave item to Slave list */
	wbd_ds_glist_append(&wbd_master->blanket_list, (dll_t *)new_master);

	if (out_master) {
		*out_master = new_master;
	}

end:
	WBD_DS("BlanketID[%d] Master Info Created %s\n", bkt_id, wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Free & Cleanup Master Info structure object from heap */
int
wbd_ds_master_info_cleanup(wbd_master_info_t *master_info, bool free_master_mem)
{
	uint8 bkt_id = 0;
	int ret = WBDE_OK;
	wbd_slave_item_t* slave_item;
	dll_t *slave_item_p, *slave_next_p;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(master_info, WBDE_INV_ARG);

	/* Store Blanket ID for printing purpose */
	bkt_id = master_info->bkt_id;

	/* Updated Blanket client count maintained by Master */
	master_info->blanket_client_count = 0;

	/* Updated Weak client count maintained by Master */
	master_info->weak_client_count = 0;

	/* Initialize Pointer to Slave running in Master mode */
	master_info->pm_slave = NULL;

	/* Traverse Slave List for this Master */
	foreach_safe_glist_item(slave_item_p, master_info->slave_list, slave_next_p) {

		/* need to keep next item incase we remove node in between */
		slave_next_p = dll_next_p(slave_item_p);

		slave_item = (wbd_slave_item_t*)slave_item_p;

		/* Remove all STA items associated & monitored with this Slave */
		wbd_ds_slave_item_cleanup(slave_item, TRUE);

		/* In the end, Remove Slave item itself from Slave list */
		wbd_ds_glist_delete(&master_info->slave_list, slave_item_p);
		free(slave_item);
		slave_item = NULL;
	}

	/* Remove ap channel report with this Master */
	wbd_ds_ap_chan_report_cleanup(&master_info->ap_chan_report);

	/* Sanity Check */
	if (master_info->slave_list.count != 0 ||
		master_info->blanket_client_count != 0) {
		WBD_WARNING("BKTID[%d] Error: %s. Slave List Count[%d] and Blanket Client Count[%d]"
			" After Cleanup\n", bkt_id, wbderrorstr(WBDE_DS_MSTR_DTCURR),
			master_info->slave_list.count, master_info->blanket_client_count);
		ret = WBDE_DS_MSTR_DTCURR;
		goto end;
	}

	/* Free dfs_chan_list from master */
	if (master_info->dfs_chan_list) {
		free(master_info->dfs_chan_list);
	}

end:
	WBD_DS("BKTID[%d] Master Info Cleanup %s\n", bkt_id, wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Allocate & Initialize Blanket Slave structure object */
int
wbd_ds_blanket_slave_init(wbd_info_t *info)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);

	/* Allocate Slave application structure */
	info->wbd_slave = (wbd_blanket_slave_t*)wbd_malloc(sizeof(*info->wbd_slave), &ret);
	WBD_ASSERT();

	/* --- Initialize Blanket Slave Application structure --- */

	/* keeps track of its parent */
	info->wbd_slave->parent = info;

	/* Initialize br0 Slave Item List of Blanket Slave for multiple bands */
	wbd_ds_glist_init(&info->wbd_slave->br0_slave_list);

end:
	WBD_DS("Blanket Slave Init %s\n", wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Free & Cleanup Blanket Slave structure object from heap */
int
wbd_ds_blanket_slave_cleanup(wbd_info_t *info)
{
	int ret = WBDE_OK;
	wbd_slave_item_t* slave_item;
	dll_t *slave_item_p, *slave_next_p;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(info, WBDE_INV_ARG);
	WBD_ASSERT_ARG(info->wbd_slave, WBDE_INV_ARG);

	/* Traverse br0 Slave Item List for this Blanket Slave */
	foreach_safe_glist_item(slave_item_p, info->wbd_slave->br0_slave_list, slave_next_p) {

		/* need to keep next item incase we remove node in between */
		slave_next_p = dll_next_p(slave_item_p);

		slave_item = (wbd_slave_item_t*)slave_item_p;

		/* Remove all STA items associated & monitored with this Slave */
		wbd_ds_slave_item_cleanup(slave_item, TRUE);

		/* In the end, Remove Slave item itself from Slave list */
		wbd_ds_glist_delete(&info->wbd_slave->br0_slave_list, slave_item_p);
		free(slave_item);
		slave_item = NULL;
	}

	/* In the end, Remove Blanket Slave itself */
	free(info->wbd_slave);
	info->wbd_slave = NULL;

end:
	WBD_DS("Blanket Slave Cleanup %s\n", wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Allocate & Initialize Slave Item structure object */
wbd_slave_item_t *
wbd_ds_slave_item_init(int band, int *error)
{
	int ret = WBDE_OK;
	wbd_slave_item_t *slave_item = NULL;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(error, WBDE_INV_ARG);

	/* Allocate Slave Item structure */
	slave_item = (wbd_slave_item_t*)wbd_malloc(sizeof(*slave_item), &ret);
	WBD_ASSERT();

	/* --- Initialize Slave Item structure --- */

	/* keeps track of its parent, Only Slave Application updates it when created */
	slave_item->parent = NULL;

	/* Updated Band maintained by Slave */
	slave_item->band = band;

	/* Initialize Slave's stamon handle, Only Slave will INIT it fm App, else it will b NULL */
	slave_item->stamon_hdl = NULL;

	/* Initialize Slave's status, wbd_ifnames will update the same */
	slave_item->wbd_ifr.enabled = 0;

#ifdef PLC_WBD
	wbd_ds_glist_init(&slave_item->plc_sta_list);
#endif /* PLC_WBD */

	/* Initialize Slave's Type, JOIN cmd will update the same, Master/Slave both will use it */
	slave_item->slave_type = WBD_SLAVE_TYPE_UNDEFINED;

end:
	WBD_DS("Band[%d] Slave Item Init %s\n", band, wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return slave_item;
}

/* Create & Add a new Slave Item to Blanket Slave for specific band */
int
wbd_ds_create_slave_for_band(wbd_blanket_slave_t *wbd_slave, int band,
	wbd_slave_item_t **out_slave)
{
	int ret = WBDE_OK;
	wbd_slave_item_t *new_slave = NULL;
	WBD_ENTER();

	/* Allocate & Initialize Slave Info structure for corrosponding Band */
	new_slave = wbd_ds_slave_item_init(band, &ret);
	WBD_ASSERT();

	/* keeps track of its parent */
	new_slave->parent = wbd_slave;

	/* In the end, Add this new Slave item to Slave list */
	wbd_ds_glist_append(&wbd_slave->br0_slave_list, (dll_t *)new_slave);

	if (out_slave) {
		*out_slave = new_slave;
	}

end:
	WBD_DS("Band[%d] Slave Item Created %s\n", band, wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* Free & Cleanup Slave Item structure object from heap */
int
wbd_ds_slave_item_cleanup(wbd_slave_item_t *slave_item, bool free_slave_mem)
{
	int ret = WBDE_OK, band = 0;
	struct ether_addr slave_mac;
	WBD_ENTER();

	memset(&slave_mac, 0, sizeof(slave_mac));

	/* Validate arg */
	WBD_ASSERT_ARG(slave_item, WBDE_INV_ARG);

	if (free_slave_mem) {
		wbd_ds_slave_stamon_cleanup(slave_item);

		/* Clean up wlif module handle. */
		wl_wlif_deinit(slave_item->wlif_hdl);

		/* Free taf' list */
		wbd_ds_slave_free_taf_list(slave_item->taf_info);
	}

	/* Store Band and Slave MAC just to print */
	band = slave_item->band;
	eacopy(&slave_item->wbd_ifr.mac, &slave_mac);

#ifdef PLC_WBD
	/* Remove all PLC STA items on this Slave */
	ret = wbd_ds_slave_item_cleanup_plc_sta_list(slave_item);
#endif /* PLC_WBD */

end:
	WBD_DS("Band[%d] Slave["MACF"] Cleanup %s\n", band, ETHER_TO_MACF(slave_mac),
		wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

#ifdef PLC_WBD
/* Remove all PLC STA items on this Slave */
int
wbd_ds_slave_item_cleanup_plc_sta_list(wbd_slave_item_t *slave_item)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(slave_item, WBDE_INV_ARG);

	ret = wbd_ds_glist_cleanup(&(slave_item->plc_sta_list));
	WBD_DS("Band[%d] Slave["MACF"] PLClist Cleanup %s\n", slave_item->band,
		ETHER_TO_MACF(slave_item->wbd_ifr.mac), wbderrorstr(ret));
end:
	WBD_EXIT();
	return ret;
}
#endif /* PLC_WBD */

/* Initialize stamon module for Slave's interface */
int
wbd_ds_slave_stamon_init(wbd_slave_item_t *slave_item)
{
	int ret = WBDE_OK;
	BCM_STAMON_STATUS status;

	/* Validate arg */
	WBD_ASSERT_ARG(slave_item, WBDE_INV_ARG);

	/* Initialize STA monitoring module for this Slave */
	slave_item->stamon_hdl = bcm_stamon_init(slave_item->wbd_ifr.ifr.ifr_name);
	if (!slave_item->stamon_hdl) {
		WBD_WARNING("Band[%d] Slave["MACF"] Failed to get STAMON Handle\n",
			slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}
	/* Enable STA monitoring module for this Slave */
	status = bcm_stamon_command(slave_item->stamon_hdl, BCM_STAMON_CMD_ENB, NULL, NULL);
	if (status != BCM_STAMONE_OK) {
		WBD_WARNING("Band[%d] Slave["MACF"] Failed to Enable STAMON. Error : %s\n",
			slave_item->band, ETHER_TO_MACF(slave_item->wbd_ifr.mac),
			bcm_stamon_strerror(status));
		ret = WBDE_STAMON_ERROR;
		goto end;
	}
	WBD_DS("Band[%d] Slave["MACF"] Stamon Init Done\n", slave_item->band,
		ETHER_TO_MACF(slave_item->wbd_ifr.mac));
end:
	return ret;

}

/* Cleanup stamon module for Slave's interface */
void
wbd_ds_slave_stamon_cleanup(wbd_slave_item_t *slave_item)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(slave_item, WBDE_INV_ARG);

	/* Clean stamon handle */
	if (slave_item->stamon_hdl) {
		/* Delete all monitoring STA's */
		bcm_stamon_command(slave_item->stamon_hdl, BCM_STAMON_CMD_DEL, NULL, NULL);
		/* Disable stamon */
		bcm_stamon_command(slave_item->stamon_hdl, BCM_STAMON_CMD_DSB, NULL, NULL);
		bcm_stamon_deinit(slave_item->stamon_hdl);
		slave_item->stamon_hdl = NULL;
	}
	WBD_DS("Band[%d] Slave["MACF"] Stamon Cleanup Done\n", slave_item->band,
		ETHER_TO_MACF(slave_item->wbd_ifr.mac));
end:
	WBD_EXIT();
}

/* Free taf' list */
int
wbd_ds_slave_free_taf_list(wbd_taf_params_t* taf)
{
	int i;
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(taf, WBDE_INV_ARG);
	i = 0;
	while ((i < taf->sta_list.count) && (taf->sta_list.pStr[i])) {
		free(taf->sta_list.pStr[i]);
		taf->sta_list.pStr[i] = NULL;
		i++;
	}
	if (taf->sta_list.pStr) {
		free(taf->sta_list.pStr);
		taf->sta_list.pStr = NULL;
	}
	i = 0;
	while ((i < taf->bss_list.count) && (taf->bss_list.pStr[i])) {
		free(taf->bss_list.pStr[i]);
		taf->bss_list.pStr[i] = NULL;
		i++;
	}
	if (taf->bss_list.pStr) {
		free(taf->bss_list.pStr);
		taf->bss_list.pStr = NULL;
	}
	if (taf->pdef) {
		free(taf->pdef);
		taf->pdef = NULL;
	}
	free(taf);
end:
	WBD_EXIT();
	return ret;
}

/* Cleanup STA bounce table */
void
wbd_ds_cleanup_sta_bounce_table(wbd_blanket_master_t *wbd_master)
{
	int key;
	wbd_sta_bounce_detect_t *entry, *tmp;

	WBD_ENTER();

	for (key = 0; key < WBD_BOUNCE_HASH_SIZE; key++) {
		entry = wbd_master->sta_bounce_table[key];

		while (entry) {
			WBD_BOUNCE("free entry"MACF"\n", ETHER_TO_MACF(entry->sta_mac));
			tmp = entry;
			entry = entry->next;

			free(tmp);
		}
	}
	WBD_EXIT();
}

/* Cleanup all Beacon reports */
void
wbd_ds_cleanup_beacon_reports(wbd_info_t *info)
{
	wbd_beacon_reports_t* report;
	dll_t *node_p, *node_next_p;
	WBD_ENTER();

	if (info->beacon_reports.count == 0) {
		goto end;
	}

	/* Traverse beacon_reports list for this Master */
	foreach_safe_glist_item(node_p, info->beacon_reports,
		node_next_p) {

		/* need to keep next item incase we remove node in between */
		node_next_p = dll_next_p(node_p);

		report = (wbd_beacon_reports_t*)node_p;

		wbd_ds_glist_delete(&info->beacon_reports, node_p);
		if (report->report_element) {
			free(report->report_element);
			report->report_element = NULL;
		}
		free(report);
	}

end:
	WBD_EXIT();
}

/* Cleanup probe STA table */
void
wbd_ds_cleanup_prb_sta_table(wbd_info_t *info)
{
	int key;
	wbd_prb_sta_t *entry, *tmp;

	WBD_ENTER();

	for (key = 0; key < WBD_PROBE_HASH_SIZE; key++) {
		entry = info->prb_sta[key];

		while (entry) {
			WBD_PROBE("free entry"MACF"\n", ETHER_TO_MACF(entry->sta_mac));
			tmp = entry;
			entry = entry->next;

			free(tmp);
		}
	}
	WBD_EXIT();
}

/* ===================== ALLOC/INIT/CLEANUP FUNCTIONS =========================== */

/* =========================== TRAVERSE FUNCTIONS ================================ */

/* Traverse list of blankets to find a Master */
wbd_master_info_t*
wbd_ds_find_master_in_blanket_master(wbd_blanket_master_t *wbd_master, uint8 bkt_id, int* error)
{
	wbd_master_info_t *master_info = NULL, *ret_master_info = NULL;
	dll_t *master_item_p;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse br0 Master List for this Blanket Master for each band */
	foreach_glist_item(master_item_p, wbd_master->blanket_list) {

		master_info = (wbd_master_info_t*)master_item_p;

		/* Master Checking Condition */
		if (master_info->bkt_id == bkt_id) {
			ret_master_info = master_info;
			goto end;
		}
	}

	WBD_DS("BlanketID[%d] %s\n", bkt_id, wbderrorstr(WBDE_DS_UNKWN_MSTR));
	*error = WBDE_DS_UNKWN_MSTR;
end:
	WBD_EXIT();
	return ret_master_info;
}

/* Traverse Blanket Slave's br0 list to find a Slave */
wbd_slave_item_t*
wbd_ds_find_slave_addr_in_blanket_slave(wbd_blanket_slave_t *wbd_slave,
	struct ether_addr *find_slave_addr, int cmp_bssid, int* error)
{
	wbd_slave_item_t *slave_item, *ret_slave_item = NULL;
	dll_t *slave_item_p;
	struct ether_addr *cmp_addr = NULL;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse br0 Master List for this Blanket Master for each band */
	foreach_glist_item(slave_item_p, wbd_slave->br0_slave_list) {

		slave_item = (wbd_slave_item_t*)slave_item_p;

		/* Slave Checking Condition */
		if (cmp_bssid == WBD_CMP_BSSID)
			cmp_addr = &slave_item->wbd_ifr.bssid;
#ifdef PLC_WBD
		else if (cmp_bssid == WBD_CMP_PLC_MAC)
			cmp_addr = &slave_item->wbd_ifr.plc_mac;
#endif /* PLC_WBD */
		else
			cmp_addr = &slave_item->wbd_ifr.mac;
		if (eacmp(cmp_addr, find_slave_addr) == 0) {
			ret_slave_item = slave_item;
			goto end;
		}
	}

	WBD_DS("Slave["MACF"]. %s\n", ETHERP_TO_MACF(find_slave_addr),
		wbderrorstr(WBDE_DS_UNKWN_SLV));
	*error = WBDE_DS_UNKWN_SLV;
end:
	WBD_EXIT();
	return ret_slave_item;
}

/* Traverse Master Info's Slave List to find a Slave */
wbd_slave_item_t*
wbd_ds_find_slave_in_master(wbd_master_info_t *master_info, struct ether_addr *find_slave_addr,
	int cmp_bssid, int* error)
{
	wbd_slave_item_t *slave_item = NULL, *ret_slave_item = NULL;
	dll_t *slave_item_p;
	struct ether_addr *cmp_addr = NULL;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse Slave List for this Slave */
	foreach_glist_item(slave_item_p, master_info->slave_list) {

		slave_item = (wbd_slave_item_t*)slave_item_p;

		/* Slave Checking Condition */
		if (cmp_bssid == WBD_CMP_BSSID)
			cmp_addr = &slave_item->wbd_ifr.bssid;
#ifdef PLC_WBD
		else if (cmp_bssid == WBD_CMP_PLC_MAC)
			cmp_addr = &slave_item->wbd_ifr.plc_mac;
#endif /* PLC_WBD */
		else
			cmp_addr = &slave_item->wbd_ifr.mac;
		if (eacmp(cmp_addr, find_slave_addr) == 0) {
			ret_slave_item = slave_item;
			goto end;
		} else {
			continue;
		}
	}

	WBD_DS("BKTID[%d] Slave["MACF"]. %s\n", master_info->bkt_id,
		ETHERP_TO_MACF(find_slave_addr), wbderrorstr(WBDE_DS_UNKWN_SLV));
	*error = WBDE_DS_UNKWN_SLV;
end:
	WBD_EXIT();
	return ret_slave_item;
}

/* Traverse BSS's Monitor STA List to find an Monitored STA */
wbd_monitor_sta_item_t*
wbd_ds_find_sta_in_bss_monitorlist(i5_dm_bss_type *i5_bss, struct ether_addr *find_sta_mac,
	int* error)
{
	int ret = WBDE_OK;
	wbd_bss_item_t *bss;
	wbd_monitor_sta_item_t *sta_item = NULL, *ret_sta_item = NULL;
	dll_t *sta_item_p;
	*error = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(find_sta_mac, WBDE_INV_ARG);
	WBD_ASSERT_ARG(i5_bss->vndr_data, WBDE_INV_ARG);

	bss = (wbd_bss_item_t*)i5_bss->vndr_data;

	/* Travese STA items monitored by this Slave */
	foreach_glist_item(sta_item_p, bss->monitor_sta_list) {

		sta_item = (wbd_monitor_sta_item_t*)sta_item_p;

		/* STA Checking Condition */
		if (eacmp(&sta_item->sta_mac, find_sta_mac) == 0) {
			ret_sta_item = sta_item;
			goto end;
		} else {
			continue;
		}
	}

	WBD_DS("BSS["MACDBG"] STA["MACF"]. %s\n",
		MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(find_sta_mac),
		wbderrorstr(WBDE_DS_UN_MONSTA));
	*error = WBDE_DS_UN_MONSTA;
end:
	WBD_EXIT();
	return ret_sta_item;
}

/* Traverse Maclist to find MAC address exists or not */
wbd_mac_item_t*
wbd_ds_find_mac_in_maclist(wbd_glist_t *list, struct ether_addr *find_mac,
	int* error)
{
	wbd_mac_item_t *mac_item = NULL, *ret_mac_item = NULL;
	dll_t *mac_item_p;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Travese maclist items */
	foreach_glist_item(mac_item_p, (*list)) {

		mac_item = (wbd_mac_item_t*)mac_item_p;

		/* MAC address Checking Condition */
		if (eacmp(&mac_item->mac, find_mac) == 0) {
			ret_mac_item = mac_item;
			goto end;
		} else {
			continue;
		}
	}

	*error = WBDE_DS_UN_MAC;
end:
	WBD_EXIT();
	return ret_mac_item;
}

/* Find the STA MAC entry in Master's STA Bounce Table */
wbd_sta_bounce_detect_t*
wbd_ds_find_sta_in_bouncing_table(wbd_blanket_master_t *wbd_master, struct ether_addr *addr)
{
	int hash_key, ret = WBDE_OK;
	wbd_sta_bounce_detect_t *entry = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(addr, WBDE_INV_ARG);

	hash_key = WBD_BOUNCE_MAC_HASH(*addr);
	entry = wbd_master->sta_bounce_table[hash_key];

	while (entry) {
		if (eacmp(&(entry->sta_mac), addr) == 0) {
			break;
		}
		entry = entry->next;
	}

end:
	WBD_EXIT();
	return entry;
}

/* Count how many BSSes has the STA entry in Controller Topology.
 * Also get the assoc_time of STA which is associated to the BSS most recently.
 */
int
wbd_ds_find_duplicate_sta_in_controller(struct ether_addr *sta_mac,
	struct timeval *out_assoc_time)
{
	int sta_found_count = 0;
	i5_dm_network_topology_type *i5_topology = NULL;
	i5_dm_device_type *device = NULL;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *sta = NULL;
	wbd_assoc_sta_item_t *assoc_sta = NULL;
	struct timeval assoc_time = {0, 0};
	WBD_ENTER();

	/* Count how many slaves has the STA entry in assoclist */
	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {

		/* Loop only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(device->flags)) {
			continue;
		}

		/* For each interface in the device */
		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, device->interface_list) {

			/* For each BSS in the interface */
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {

				/* For each STA in the BSS */
				foreach_i5glist_item(sta, i5_dm_clients_type, i5_bss->client_list) {

					if (memcmp(sta->mac, sta_mac, MAC_ADDR_LEN) == 0) {

						/* Get Assoc item pointer */
						assoc_sta = sta->vndr_data;

						if (assoc_sta && out_assoc_time &&
							/* CSTYLED */
							(timercmp(&(assoc_sta->assoc_time),
							&assoc_time, >))) {
							assoc_time = assoc_sta->assoc_time;
						}
						sta_found_count++;
					}
				}
			}
		}
	}

	if (out_assoc_time) {
		*out_assoc_time = assoc_time;
	}

	WBD_EXIT();
	return sta_found_count;
}

/* Traverse Beacon reports list and find the beacon report based on token */
wbd_beacon_reports_t*
wbd_ds_find_item_fm_beacon_reports(wbd_info_t *info, struct ether_addr *sta_mac, int *error)
{
	wbd_beacon_reports_t *report = NULL, *ret_report = NULL;
	dll_t *node;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse each beacon_reports and get the current beacon_report */
	foreach_glist_item(node, info->beacon_reports) {
		report = (wbd_beacon_reports_t*)node;

		/* Check for current slave */
		if (!eacmp(sta_mac, &(report->sta_mac))) {
			ret_report = report;
			goto end;
		}
	}
	WBD_DS("sta["MACF"]. %s\n", ETHERP_TO_MACF(sta_mac), wbderrorstr(WBDE_DS_UN_BCN_REPORT));
	*error = WBDE_DS_UN_BCN_REPORT;

end:
	WBD_EXIT();
	return ret_report;
}

/* Find the STA MAC entry in Master's Probe STA Table */
wbd_prb_sta_t*
wbd_ds_find_sta_in_probe_sta_table(wbd_info_t *info, struct ether_addr *addr,
	bool enable)
{
	int ret = WBDE_OK, idx;
	wbd_prb_sta_t *sta = NULL;

	WBD_ENTER();

	idx = WBD_PROBE_MAC_HASH(*addr);
	sta = info->prb_sta[idx];

	/* search for the STA entry */
	while (sta) {
		WBD_PROBE("cmp: sta[%p]:"MACF" band:0x%x\n",
			sta, ETHER_TO_MACF(sta->sta_mac), sta->band);

		if (eacmp(&(sta->sta_mac), addr) == 0) {
			break;
		}
		sta = sta->next;
	}

	/* If STA not found and enable is TRUE, add the STA entry */
	if (!sta && enable) {

		sta = (wbd_prb_sta_t*)wbd_malloc(sizeof(wbd_prb_sta_t), &ret);
		WBD_ASSERT();

		memset(sta, 0, sizeof(wbd_prb_sta_t));
		memcpy(&sta->sta_mac, addr, sizeof(sta->sta_mac));

		sta->active = time(NULL);

		sta->next = info->prb_sta[idx];
		info->prb_sta[idx] = sta;

		WBD_PROBE("sta[%p]:"MACF"\n", sta, ETHERP_TO_MACF(addr));
	}

end:
	WBD_EXIT();
	return sta;
}
/* =========================== TRAVERSE FUNCTIONS ================================ */

/* ======================= ADD/REMOVE/UPDATE FUNCTIONS =========================== */

/* Removes a Slave item from Master Info */
int
wbd_ds_remove_slave(wbd_master_info_t *master_info, struct ether_addr *slave_bssid,
	wbd_trigger_t *out_trigger)
{
	int ret = WBDE_OK;
	wbd_slave_item_t* slave_item = NULL;
	WBD_ENTER();

	/* Traverse Slave List to find Slave, if Slave not found, get out */
	slave_item = wbd_ds_find_slave_in_master(master_info, slave_bssid, WBD_CMP_BSSID, &ret);
	if (!slave_item) {
		goto end;
	}

	/* Remove all STA items associated & monitored with this Slave first */
	ret = wbd_ds_slave_item_cleanup(slave_item, FALSE);

	/* If Slave running in Master is getting removed */
	if (slave_item->slave_type == WBD_SLAVE_TYPE_MASTER) {
		/* Make Slave running in Master ptr NULL */
		master_info->pm_slave = NULL;
	}

	/* In the end, Remove Slave item itself from Slave list */
	wbd_ds_glist_delete(&master_info->slave_list, &slave_item->node);
	free(slave_item);

	WBD_DS("BKTID[%d] Slave Removed ["MACF"] From blanket. Now Total Clients[%d] "
		"Weak Clients[%d]\n", master_info->bkt_id, ETHERP_TO_MACF(slave_bssid),
		master_info->blanket_client_count, master_info->weak_client_count);

end:
	WBD_EXIT();
	return ret;
}

/* Add a STA item in a BSS's Monitor STA List */
int
wbd_ds_add_sta_in_bss_monitorlist(i5_dm_bss_type *i5_bss, struct ether_addr *bssid,
	struct ether_addr *new_sta_mac, wbd_monitor_sta_item_t **out_sta_item)
{
	int ret = WBDE_OK;
	wbd_monitor_sta_item_t *new_sta_item = NULL;
	wbd_bss_item_t *bss;
	WBD_ENTER();

	/* Check if the STA is already there in monitorlist or not */
	if (wbd_ds_find_sta_in_bss_monitorlist(i5_bss, new_sta_mac, &ret)) {
		WBD_DS("STA["MACF"] %s in monitorlist of BSS["MACDBG"]\n",
			ETHERP_TO_MACF(new_sta_mac),
			wbderrorstr(WBDE_DS_STA_EXST), MAC2STRDBG(i5_bss->BSSID));
		ret = WBDE_DS_STA_EXST;
		goto end;
	}
	WBD_ASSERT_ARG(i5_bss->vndr_data, WBDE_INV_ARG);
	ret = WBDE_OK;

	/* Create a STA item */
	new_sta_item = (wbd_monitor_sta_item_t*)wbd_malloc(sizeof(*new_sta_item), &ret);
	WBD_ASSERT();

	/* Fill & Initialize STA item data */
	memcpy(&new_sta_item->sta_mac, new_sta_mac, sizeof(*new_sta_mac));
	memcpy(&new_sta_item->slave_bssid, bssid, sizeof(new_sta_item->slave_bssid));

	/* In the end, Add this new STA item to monitored STA list of this Slave */
	bss = (wbd_bss_item_t*)i5_bss->vndr_data;
	wbd_ds_glist_append(&bss->monitor_sta_list, (dll_t*)new_sta_item);

	if (out_sta_item) {
		*out_sta_item = new_sta_item;
	}
	WBD_DS("STA Added["MACF"] In BSS["MACDBG"]'s Monitorlist\n",
		ETHERP_TO_MACF(new_sta_mac), MAC2STRDBG(i5_bss->BSSID));
end:
	WBD_EXIT();
	return ret;
}

/* Add a STA item in all peer Devices' Monitor STA List. map_flags will tell whether to add it to
 * Fronthaul BSS or Bakhaul BSS
 */
int
wbd_ds_add_sta_in_peer_devices_monitorlist(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_assoc_sta, uint8 map_flags)
{
	int ret = WBDE_NO_SLAVE_TO_STEER;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_iter_device;
	i5_dm_interface_type *i5_iter_ifr;
	i5_dm_bss_type *i5_bss, *i5_iter_bss;

	WBD_ENTER();

	i5_topology = ieee1905_get_datamodel();
	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);

	/* Traverse Device List */
	foreach_i5glist_item(i5_iter_device, i5_dm_device_type, i5_topology->device_list) {

		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_iter_device->flags)) {
			continue;
		}

		/* For all the interfaces in this devices */
		foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type,
			i5_iter_device->interface_list) {

			/* For all the BSS in this Interface */
			foreach_i5glist_item(i5_iter_bss, i5_dm_bss_type, i5_iter_ifr->bss_list) {

				if (I5_IS_BSS_GUEST(i5_iter_bss->mapFlags)) {
					continue;
				}
				/* Only use BSS which has matching map Flags. This is to add STA to
				 * to Fronthaul BSS if the weak STA is connected to Fronthaul BSS
				 * or to Backhaul BSS if the weak STA is connected to Backhaul
				 */
				if (!(i5_iter_bss->mapFlags & map_flags)) {
					continue;
				}

				/* BSS Checking Condition */
				if (eacmp(&i5_bss->BSSID, i5_iter_bss->BSSID) == 0) {
					/* don't add in current BSS Monitor List */
					continue;
				}

				/* Don't add if the SSIDs are not matching */
				if ((i5_iter_bss->ssid.SSID_len != i5_bss->ssid.SSID_len) ||
					memcmp((char*)i5_iter_bss->ssid.SSID,
					(char*)i5_bss->ssid.SSID, i5_bss->ssid.SSID_len) != 0) {
					continue;
				}

				/* STA should be added to all the BSS on all the devices so that
				 * the RSSI from both un associated sta link metrics response and
				 * beacon reports can be stored.
				 */
				ret = wbd_ds_add_sta_in_bss_monitorlist(i5_iter_bss,
					(struct ether_addr*)i5_bss->BSSID,
					(struct ether_addr*)i5_assoc_sta->mac, NULL);
			}
		}
	}

	WBD_EXIT();
	return ret;
}

/* Remove STA from current BSS's monitor list, Check for Igonre maclist */
int
wbd_ds_add_sta_in_controller(wbd_blanket_master_t *wbd_master, i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss;
	wbd_assoc_sta_item_t *assoc_sta;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_assoc_sta->vndr_data, WBDE_INV_ARG);
	assoc_sta = (wbd_assoc_sta_item_t*)i5_assoc_sta->vndr_data;
	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);

	/* Remove STA from current BSS's monitor list if exists */
	wbd_ds_remove_sta_fm_bss_monitorlist(i5_bss, (struct ether_addr*)i5_assoc_sta->mac);

	/* Find new STA's MAC in ignore maclist */
	if (assoc_sta && wbd_ds_find_mac_in_maclist(&wbd_master->ignore_maclist,
		(struct ether_addr*)&i5_assoc_sta->mac, &ret)) {
		WBD_INFO("STA["MACDBG"] Is in ignore list\n", MAC2STRDBG(i5_assoc_sta->mac));
		/* If new STA is in ignore maclist, and Update STA Staus */
		assoc_sta->status = WBD_STA_STATUS_IGNORE;
	}

end:
	WBD_EXIT();
	return ret;
}

/* Adds to monitorlist from assoclist */
static int
wbd_ds_add_monitorlist_fm_peer_bss_assoclist(i5_dm_bss_type *i5_dst_bss, i5_dm_bss_type *i5_src_bss)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t* sta_item = NULL;
	uint8 map_flags;
	WBD_ENTER();

	/* Travese STA items in the BSS */
	foreach_i5glist_item(i5_sta, i5_dm_clients_type, i5_src_bss->client_list) {

		sta_item = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
		if (sta_item == NULL) {
			WBD_WARNING("STA["MACDBG"] NULL Vendor Data\n", MAC2STRDBG(i5_sta->mac));
			continue;
		}

		/* Only weak STAs are monitored. So, if not weak skip this STA */
		if (sta_item->status != WBD_STA_STATUS_WEAK)
			continue;

		if (I5_CLIENT_IS_BSTA(i5_sta)) {
			map_flags = IEEE1905_MAP_FLAG_BACKHAUL;
		} else {
			map_flags = IEEE1905_MAP_FLAG_FRONTHAUL;
		}

		/* Add it to matching Fronthaul or Backhaul BSS based on the STA */
		if (!(map_flags & i5_dst_bss->mapFlags)) {
			continue;
		}

		/* Don't add if the SSIDs are not matching */
		if ((i5_dst_bss->ssid.SSID_len != i5_src_bss->ssid.SSID_len) ||
			memcmp((char*)i5_dst_bss->ssid.SSID, (char*)i5_src_bss->ssid.SSID,
			i5_dst_bss->ssid.SSID_len) != 0) {
			continue;
		}

		ret = wbd_ds_add_sta_in_bss_monitorlist(i5_dst_bss,
			(struct ether_addr*)&i5_src_bss->BSSID, (struct ether_addr*)&i5_sta->mac,
			NULL);

		if (ret != WBDE_OK) {
			WBD_DS("BSS["MACDBG"] STA["MACDBG"] Failed to Add STA to "
				"monitorlist Error : %s\n",
				MAC2STRDBG(i5_dst_bss->BSSID),
				MAC2STRDBG(i5_sta->mac), wbderrorstr(ret));
			if (ret != WBDE_DS_STA_EXST) {
				goto end;
			}
		}
	}

end:
	WBD_EXIT();
	return ret;
}

/* Add To monitorlist of BSS from the assoclist of peer BSS */
int
wbd_ds_add_monitorlist_fm_peer_devices_assoclist(i5_dm_bss_type *i5_bss)
{
	int ret = WBDE_OK;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_peer_bss;
	WBD_ENTER();

	i5_topology = ieee1905_get_datamodel();
	/* Traverse BSS List to add the weak STA to the new BSS monitor list if any */
	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {

		/* Loop only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_peer_bss, i5_dm_bss_type, i5_ifr->bss_list) {

				/* BSS Checking Condition */
				if (memcmp(i5_peer_bss->BSSID, i5_bss->BSSID,
					sizeof(i5_peer_bss->BSSID)) == 0) {
					/* Peer BSS is Parent BSS, don't add from assoclist */
					continue;
				}

				if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
					continue;
				}
				ret = wbd_ds_add_monitorlist_fm_peer_bss_assoclist(i5_bss,
					i5_peer_bss);
			}
		}
	}

	WBD_EXIT();
	return ret;
}

/* Add a MAC address to maclist */
int
wbd_ds_add_mac_to_maclist(wbd_glist_t *list, struct ether_addr *mac, wbd_mac_item_t **out_mac_item)
{
	int ret = WBDE_OK;
	wbd_mac_item_t *new_node = NULL;
	WBD_ENTER();

	/* Check if the MAC is already there in maclist or not */
	if (wbd_ds_find_mac_in_maclist(list, mac, &ret)) {
		WBD_WARNING(""MACF" %s in maclist\n",
			ETHERP_TO_MACF(mac), wbderrorstr(WBDE_DS_MAC_EXST));
		ret = WBDE_DS_MAC_EXST;
		goto end;
	}
	ret = WBDE_OK;

	/* Create a MAC item */
	new_node = (wbd_mac_item_t*)wbd_malloc(sizeof(*new_node), &ret);
	WBD_ASSERT();

	/* Fill & Initialize MAC item data */
	memcpy(&new_node->mac, mac, sizeof(*mac));

	/* In the end, Add this new MAC item to maclist */
	wbd_ds_glist_append(list, (dll_t *) new_node);

	if (out_mac_item) {
		*out_mac_item = new_node;
	}
	WBD_DS("Added MAC["MACF"] to maclist\n", ETHERP_TO_MACF(mac));
end:
	WBD_EXIT();
	return ret;
}

/* Add STA to the bouncing table */
int
wbd_ds_add_sta_to_bounce_table(wbd_blanket_master_t *wbd_master, struct ether_addr *addr,
	struct ether_addr *src_addr, uint8 flags)
{
	int hash_key, ret = WBDE_OK;
	wbd_sta_bounce_detect_t *entry;
	time_t now = time(NULL);
	WBD_ENTER();

	hash_key = WBD_BOUNCE_MAC_HASH(*addr);
	entry = wbd_master->sta_bounce_table[hash_key];

	WBD_BOUNCE("hash_key:%d, addr:["MACF"]\n", hash_key, ETHERP_TO_MACF(addr));

	/* Find the STA MAC in Bounce Table */
	while (entry) {
		if (eacmp(&(entry->sta_mac), addr) == 0) {
			break;
		}
		entry = entry->next;
	}

	/* If the STA MAC is not found in Bounce Table, create a new entry, and add to Table */
	if (!entry) {
		entry = (wbd_sta_bounce_detect_t*)wbd_malloc(sizeof(*entry), &ret);
		WBD_ASSERT_MSG("bouncing table entry alloc failed for STA["MACF"]... "
			"Aborting...\n", ETHERP_TO_MACF(addr));

		memcpy(&entry->sta_mac, addr, sizeof(entry->sta_mac));
		entry->next = wbd_master->sta_bounce_table[hash_key];
		wbd_master->sta_bounce_table[hash_key] = entry;
	}
	if (!src_addr) {
		WBD_INFO("STA["MACF"] added to bounce table on receiving beacon report. "
			"Skiping Steering related initializations\n", ETHERP_TO_MACF(addr));
		goto end;
	}

	entry->flags = flags;
	/* If timestamp is reseted, set to current time and STATE to INIT */
	if (entry->timestamp == 0) {
		entry->state = WBD_BOUNCE_INIT;
		entry->timestamp = now;
	}

	/* Copy associated slave's mac addr */
	eacopy(src_addr, &entry->src_mac);

	/* If successfully sent STEER cmd, Update STA's Status = Steering */
	entry->sta_status = WBD_STA_STATUS_WEAK_STEERING;

	WBD_BOUNCE("entry["MACF"] window:%d, cnt:%d dwell_time:%d\n",
		ETHERP_TO_MACF(addr), entry->run.window, entry->run.cnt, entry->run.dwell_time);

end:
	WBD_EXIT();
	return ret;
}

/* Add an item to beacon reports list */
wbd_beacon_reports_t*
wbd_ds_add_item_to_beacon_reports(wbd_info_t *info, struct ether_addr *neighbor_al_mac,
	time_t timestamp, struct ether_addr *sta_mac)
{
	int ret = WBDE_OK;
	wbd_beacon_reports_t *report = NULL;
	WBD_ENTER();

	/* Allocate memory for a new beacon report item */
	report = (wbd_beacon_reports_t*)wbd_malloc(sizeof(*report), &ret);
	WBD_ASSERT();

	report->timestamp = timestamp;
	memcpy(&report->neighbor_al_mac, neighbor_al_mac, sizeof(report->neighbor_al_mac));
	memcpy(&report->sta_mac, sta_mac, sizeof(report->sta_mac));

	/* In the end, Add this new Capability item to Capability Object */
	wbd_ds_glist_append(&info->beacon_reports, (dll_t *)report);

	WBD_DS("Added STA["MACF"] with neighbor al MAC["MACF"]\n", ETHERP_TO_MACF(sta_mac),
		ETHERP_TO_MACF(neighbor_al_mac));
end:
	WBD_EXIT();
	return report;

}

/* add probe STA item */
void
wbd_ds_add_item_to_probe_sta_table(wbd_info_t *info, struct ether_addr *addr, uint8 band)
{
	wbd_prb_sta_t *sta;

	WBD_ENTER();

	sta = wbd_ds_find_sta_in_probe_sta_table(info, addr, TRUE);
	if (sta) {
		WBD_PROBE("Adding STA["MACF"] to Probe Table with band[%d] sta->band[%d]\n",
			ETHERP_TO_MACF(addr), band, sta->band);
		sta->band |= band;
		sta->active = time(NULL);
	}

	WBD_EXIT();
}

/* Remove a STA item from a BSS's Monitor STA List */
int
wbd_ds_remove_sta_fm_bss_monitorlist(i5_dm_bss_type *i5_bss, struct ether_addr *sta_mac)
{
	int ret = WBDE_OK;
	wbd_bss_item_t *bss;
	wbd_monitor_sta_item_t* sta_item = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_bss->vndr_data, WBDE_INV_ARG);

	bss = (wbd_bss_item_t*)i5_bss->vndr_data;
	/* Travese STA List monitored by this BSS to find STA */
	if ((sta_item = wbd_ds_find_sta_in_bss_monitorlist(i5_bss, sta_mac, &ret)) != NULL) {

		/* In the end, Remove STA item from monitored STA list of this BSS */
		wbd_ds_glist_delete(&bss->monitor_sta_list, &sta_item->node);
		free(sta_item);
		WBD_DS("STA Removed["MACF"] from BSS["MACDBG"]'s Monitorlist\n",
			ETHERP_TO_MACF(sta_mac), MAC2STRDBG(i5_bss->BSSID));
	}

end:
	WBD_EXIT();
	return ret;
}

/* Remove a STA item from all peer BSS' Monitor STA List */
int
wbd_ds_remove_sta_fm_peer_devices_monitorlist(struct ether_addr * parent_slave_bssid,
	struct ether_addr *sta_mac, uint8 mapFlags)
{
	int ret = WBDE_OK;
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	WBD_ENTER();

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}
		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				if (I5_IS_BSS_GUEST(i5_bss->mapFlags)) {
					continue;
				}
				/* Only if the mapFlags matches */
				if (!(i5_bss->mapFlags & mapFlags)) {
					continue;
				}
				/* BSS Checking Condition */
				if (eacmp(parent_slave_bssid, i5_bss->BSSID) == 0) {
					/* BSS is Parent BSS, don't remove fm its Monitor List */
					continue;
				}
				ret = wbd_ds_remove_sta_fm_bss_monitorlist(i5_bss, sta_mac);
			}
		}
	}

	WBD_EXIT();
	return ret;
}

/* Remove beacon report */
int
wbd_ds_remove_beacon_report(wbd_info_t *info, struct ether_addr *sta_mac)
{
	int ret = WBDE_OK;
	wbd_beacon_reports_t *report = NULL;
	WBD_ENTER();

	/* Traverse through each beaocn report */
	if ((report = wbd_ds_find_item_fm_beacon_reports(info, sta_mac, &ret)) != NULL) {
		WBD_DS("sta["MACF"] beacon report will be removed\n", ETHERP_TO_MACF(sta_mac));
		wbd_ds_glist_delete(&info->beacon_reports, &report->node);
		if (report->report_element) {
			free(report->report_element);
			report->report_element = NULL;
		}
		free(report);
		report = NULL;
	}

	WBD_EXIT();
	return ret;
}

/* Update slave in master's slavelist */
void
wbd_ds_update_slave_item(wbd_slave_item_t *dst, wbd_slave_item_t *src, int updateflag)
{
	WBD_ENTER();
	if (updateflag & WBD_UPDATE_SLAVE_RSSI)
		dst->wbd_ifr.RSSI = src->wbd_ifr.RSSI;
	if (updateflag & WBD_UPDATE_SLAVE_TX_RATE)
		dst->wbd_ifr.uplink_rate = src->wbd_ifr.uplink_rate;
	WBD_EXIT();
}

/*
 * STA Steering Bouncing Prevention is globally controlled by
 * nvram var wbd_bounce_detect=<window time in sec> <cnts> <dwell time in sec>
 *
 * to prevent from moving STAs in a ping-pong, or cascading steering scenario
 * If a STA is steer'd <cnts> times within <window time> seconds,
 * the WBD engine will keep the STA in current slave, and not steer it for
 * a period of <dwell time> seconds
 *
 * The bouncing STA detection impl. is based on STA's bounce state transition
 * The periodic watchdog timer events, and steering STA event move the STA's
 * bounce state among following 4 states:
 *		WBD_BOUNCE_INIT
 *		WBD_BOUNCE_WINDOW_STATE
 *		WBD_BOUNCE_DWELL_STATE
 *		WBD_BOUNCE_CLEAN_STATE
 *
 *  The STA is declared as a bouncing STA if it is in WBD_BOUNCE_DWELL_STATE
 *
 */
void
wbd_ds_update_sta_bounce_table(wbd_blanket_master_t *wbd_master)
{
	int key;
	time_t now = time(NULL);
	time_t gap;
	wbd_sta_bounce_detect_t *entry;
	wbd_bounce_detect_t *bounce_cfg;

	WBD_ENTER();

	for (key = 0; key < WBD_BOUNCE_HASH_SIZE; key++) {
		entry = wbd_master->sta_bounce_table[key];

		while (entry) {
			gap = now - entry->timestamp;
			WBD_BOUNCE("entry["MACF"] state[%d] timestamp[%lu] now[%lu] gap[%lu]\n",
				ETHER_TO_MACF(entry->sta_mac), entry->state,
				(unsigned long)(entry->timestamp), (unsigned long)(now), gap);

			/* If no timestamp means, its not steered after INIT state */
			if (entry->timestamp == 0) {
				goto next;
			}

			/* Choose the bounce detect config based on Fronthaul or Backhaul STA */
			if (entry->flags & WBD_STA_BOUNCE_DETECT_FLAGS_BH) {
				bounce_cfg = &wbd_master->bounce_cfg_bh;
			} else {
				bounce_cfg = &wbd_master->bounce_cfg;
			}

			/* If its in dwell state, check if the dwell time is over or not */
			if (entry->state == WBD_BOUNCE_DWELL_STATE) {
				entry->run.dwell_time = gap;
				if (entry->run.dwell_time > bounce_cfg->dwell_time) {
					/* clean dwell state and Move to init state */
					memset(&entry->run, 0, sizeof(entry->run));
					entry->timestamp = 0;
					entry->state = WBD_BOUNCE_INIT;
				}
			} else {
				/* Move to window state and check the count */
				entry->run.window = gap;
				entry->state = WBD_BOUNCE_WINDOW_STATE;
				if (entry->run.window < bounce_cfg->window) {
					/* Within window state, check the steered count */
					if (entry->run.cnt >= bounce_cfg->cnt) {
						/* Move to dwell state */
						entry->timestamp = now;
						entry->state = WBD_BOUNCE_DWELL_STATE;
					}
				} else {
					/* clean the window state and move to init state */
					memset(&entry->run, 0, sizeof(entry->run));
					entry->timestamp = 0;
					entry->state = WBD_BOUNCE_INIT;
				}
			}

			WBD_BOUNCE("update entry["MACF"] state[%d] timestamp[%lu]\n",
				ETHER_TO_MACF(entry->sta_mac), entry->state,
				(unsigned long)(entry->timestamp));

next:
			entry = entry->next;
		}
	}

	WBD_EXIT();
}

/* Increment the Bounce Count of a STA entry in Bounce Table */
int
wbd_ds_increment_bounce_count_of_entry(wbd_blanket_master_t *wbd_master, struct ether_addr *addr,
	struct ether_addr *dst_mac, wbd_wl_sta_stats_t *sta_stats)
{
	int ret = WBDE_OK;
	wbd_sta_bounce_detect_t *entry = NULL;
	WBD_ENTER();

	/* Find the STA MAC entry in Master's STA Bounce Table */
	entry = wbd_ds_find_sta_in_bouncing_table(wbd_master, addr);
	if (!entry) {
		goto end;
	}

	/* If STA entry is found in Bounce Table && STA Status is STEERING, so this STA has
	 * been steered from source to this TBSS. So let's increament the Bounce Count here
	 */
	if (entry->sta_status == WBD_STA_STATUS_WEAK_STEERING) {

		/* STA successfully steered to TBSS, Update STA's Status = Normal */
		entry->sta_status = WBD_STA_STATUS_NORMAL;

		/* Increament the Bounce Count */
		entry->run.cnt++;
#ifdef BCM_APPEVENTD
		/* Send the steer complete event to appeventd. */
		wbd_appeventd_steer_complete(APP_E_WBD_MASTER_STEER_END, " ", addr,
			&(entry->src_mac), dst_mac, sta_stats->rssi, sta_stats->tx_rate);
#endif /* BCM_APPEVENTD */
	}

end:
	WBD_EXIT();
	return ret;
}

/*
 * Adds logs info in the blanket master's logs buffer.
 * Once the buffer becomes full new logs entries will
 * overwrite the older ones.
 */
void
wbd_ds_add_logs_in_master(wbd_blanket_master_t *blanket_master_info, char *logmsg)
{
	wbd_logs_info_t *master_logs = &(blanket_master_info->master_logs);
	int buflen = master_logs->buflen - 1;
	int idx;

	WBD_ENTER();

	/* Wrap around index. */
	idx = master_logs->index & buflen;

	/* Copy logs. */
	WBDSTRNCPY(master_logs->logmsgs[idx], logmsg, sizeof(master_logs->logmsgs[idx]));

	/* Update write index. */
	master_logs->index++;
	master_logs->index &= buflen;

	WBD_EXIT();
}

/*
 * Fetch logs from the blanket master and returns the total len of logs.
 * The logs will be copied in oldest to newest order.
 */
int
wbd_ds_get_logs_from_master(wbd_blanket_master_t *blanket_master_info, char *logs, int len)
{
	wbd_logs_info_t *master_logs = &(blanket_master_info->master_logs);
	int buflen = master_logs->buflen - 1;
	int idx, start, end, logs_len = 0;

	WBD_ENTER();

	/* set the start and end of the buffer.  */
	idx = master_logs->index;
	if (!strlen(master_logs->logmsgs[idx])) {
		start = 0;
		end = idx;
	} else {
		start = idx;
		idx--;
		end = idx & buflen;
	}

	/* copy the logs from start to end - 1. */
	for (idx = start; idx ^ end;) {
		logs_len += snprintf(logs + strlen(logs), len - strlen(logs),
			"%s", master_logs->logmsgs[idx]);
		idx++;
		idx &= buflen;
	}

	/* Copy last log. */
	logs_len += snprintf(logs + strlen(logs), len - strlen(logs),
		"%s", master_logs->logmsgs[idx]);

	WBD_EXIT();

	return logs_len;
}

/* ======================= ADD/REMOVE/UPDATE FUNCTIONS =========================== */

/* Helper function to check if device investigated is self_device or not  */
bool
wbd_ds_is_i5_device_self(unsigned char *device_id)
{
	int ret = WBDE_OK;
	i5_dm_device_type *self_device = NULL;
	WBD_ENTER();

	/* Get the self device */
	self_device = wbd_ds_get_self_i5_device(&ret);
	WBD_ASSERT_MSG("%s", wbderrorstr(ret));

	/* Cmp Self Device ID & Investigated Device ID, if matching return TRUE, else FALSE */
	if (memcmp(self_device->DeviceId, device_id, MAC_ADDR_LEN) == 0) {
		return TRUE;
	}

end:
	WBD_EXIT();
	return FALSE;
}

/* Helper function to get the controller device */
i5_dm_device_type*
wbd_ds_get_controller_i5_device(int *error)
{
	int ret = WBDE_OK;
	i5_dm_device_type *controller_device = NULL;
	WBD_ENTER();

	controller_device = i5DmFindController();
	if (controller_device == NULL) {
		ret = WBDE_AGENT_NT_JOINED;
		goto end;
	}

end:
	WBD_DS("Controller Device Find : %s\n", wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return controller_device;
}

/* Helper function to get the device from device id */
i5_dm_device_type*
wbd_ds_get_i5_device(unsigned char *device_id, int *error)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device = NULL;
	WBD_ENTER();

	if (!device_id) {
		ret = WBDE_DS_UN_DEV;
		device_id = (unsigned char *)&ether_null;
	} else if (!(device = i5DmDeviceFind(device_id))) {
		ret = WBDE_DS_UN_DEV;
	}

	WBD_DS("Device ["MACDBG"] Find : %s\n",
		MAC2STRDBG(device_id), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return device;
}

/* Helper function to get the self device */
i5_dm_device_type*
wbd_ds_get_self_i5_device(int *error)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device = NULL;
	WBD_ENTER();

	if ((device = i5DmGetSelfDevice()) == NULL) {
		ret = WBDE_DS_UN_DEV;
	}

	WBD_DS("Device ["MACDBG"] Find : %s\n",
		MAC2STRDBG(ieee1905_get_al_mac()), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return device;
}

/* Helper Function to find interface using AL mac and interface mac */
i5_dm_interface_type*
wbd_ds_get_i5_interface(unsigned char *device_id, unsigned char *if_mac, int *error)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device;
	i5_dm_interface_type *i5_ifr = NULL;
	WBD_ENTER();

	device = wbd_ds_get_i5_device(device_id, &ret);
	WBD_ASSERT();

	/* Now Find the interface */
	if (!if_mac) {
		ret = WBDE_DS_UN_IFR;
		if_mac = (unsigned char *)&ether_null;
	} else if (!(i5_ifr = i5DmInterfaceFind(device, if_mac))) {
		ret = WBDE_DS_UN_IFR;
	}
end:
	WBD_DS("Interface["MACDBG"] Find : %s\n",
		MAC2STRDBG(if_mac), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_ifr;
}

/* Helper Function to find interface in the self device */
i5_dm_interface_type*
wbd_ds_get_self_i5_interface(unsigned char *mac, int *error)
{
	return wbd_ds_get_i5_ifr_in_device(i5DmGetSelfDevice(), mac, error);
}

/* Helper Function to find interface using i5_device and interface mac */
i5_dm_interface_type*
wbd_ds_get_i5_ifr_in_device(i5_dm_device_type *i5_device, unsigned char *if_mac,
	int *error)
{
	int ret = WBDE_OK;
	i5_dm_interface_type *i5_ifr = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);

	/* Now Find the interface */
	if (!if_mac) {
		ret = WBDE_DS_UN_IFR;
		if_mac = (unsigned char *)&ether_null;
	} else if (!(i5_ifr = i5DmInterfaceFind(i5_device, if_mac))) {
		ret = WBDE_DS_UN_IFR;
	}

end:
	WBD_DS("Interface["MACDBG"] Find : %s\n",
		MAC2STRDBG(if_mac), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_ifr;
}

/* Helper Function to find self interface from ifname */
i5_dm_interface_type*
wbd_ds_get_self_i5_ifr_from_ifname(char *ifname, int *error)
{
	int ret = WBDE_DS_UN_IFR;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_iter_ifr, *i5_ifr = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);

	WBD_SAFE_GET_I5_SELF_DEVICE(i5_device, &ret);

	/* For all the interfaces in this devices */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		if (strcmp(i5_iter_ifr->ifname, ifname) == 0) {
			i5_ifr = i5_iter_ifr;
			ret = WBDE_OK;
			goto end;
		}
	}

end:
	WBD_DS("ifname[%s] Find : %s\n", ifname, wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_ifr;
}

/* Helper function to find BSS using al_mac and bssid */
i5_dm_bss_type*
wbd_ds_get_i5_bss(unsigned char *device_id, unsigned char *bssid, int *error)
{
	int ret = WBDE_OK;
	i5_dm_device_type *device;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	device = wbd_ds_get_i5_device(device_id, &ret);
	WBD_ASSERT();

	/* Now Find the BSS */
	if (!bssid) {
		ret = WBDE_DS_UN_BSS;
		bssid = (unsigned char *)&ether_null;
	} else if (!(i5_bss = i5DmFindBSSFromDevice(device, bssid))) {
		ret = WBDE_DS_UN_BSS;
	}

end:
	WBD_DS("BSS["MACDBG"] Find : %s\n",
		MAC2STRDBG(bssid), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Helper function to find BSS in the self device */
i5_dm_bss_type *
wbd_ds_get_self_i5_bss(unsigned char *bssid, int *error)
{
	return wbd_ds_get_i5_bss_in_device(i5DmGetSelfDevice(), bssid, error);
}

/* Find BSS using Device and BSSID */
i5_dm_bss_type*
wbd_ds_get_i5_bss_in_device(i5_dm_device_type *i5_device, unsigned char *bssid, int *error)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);

	/* Now Find the BSS */
	if (!bssid) {
		bssid = (unsigned char *)&ether_null;
	} else if (!(i5_bss = i5DmFindBSSFromDevice(i5_device, bssid))) {
		ret = WBDE_DS_UN_BSS;
	}

end:
	WBD_DS("BSS["MACDBG"] Find : %s\n",
		MAC2STRDBG(bssid), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Find Fronthaul BSS using Device and band */
i5_dm_bss_type*
wbd_ds_get_i5_bss_in_device_for_band_and_mapflag(i5_dm_device_type *i5_device, int in_band,
	uint8 map, int *error)
{
	int ret = WBDE_DS_UN_BSS;
	i5_dm_interface_type *i5_iter_ifr;
	i5_dm_bss_type *i5_iter_bss, *i5_bss = NULL;

	/* For all the interfaces in this devices */
	foreach_i5glist_item(i5_iter_ifr, i5_dm_interface_type, i5_device->interface_list) {

		if ((i5_iter_ifr->chanspec == 0) ||
			!i5DmIsInterfaceWireless(i5_iter_ifr->MediaType) ||
				!i5_iter_ifr->BSSNumberOfEntries) {
			continue;
		}

		if (i5_iter_ifr->band & in_band) {
			/* For all the BSS in this devices */
			foreach_i5glist_item(i5_iter_bss, i5_dm_bss_type, i5_iter_ifr->bss_list) {
				if (I5_IS_BSS_GUEST(i5_iter_bss->mapFlags)) {
					continue;
				}
				if (map & i5_iter_bss->mapFlags) {
					i5_bss = i5_iter_bss;
					ret = WBDE_OK;
					goto end;
				}
			}
		}
	}

end:
	WBD_DS("Band[%d] Find : %s\n", in_band, wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Find BSS from complete topology */
i5_dm_bss_type*
wbd_ds_get_i5_bss_in_topology(unsigned char *bssid, int *error)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	/* Now Find the BSS */
	if (!bssid) {
		bssid = (unsigned char *)&ether_null;
	} else if (!(i5_bss = i5DmFindBSSFromNetwork(bssid))) {
		ret = WBDE_DS_UN_BSS;
	}

	WBD_DS("BSS["MACDBG"] Find : %s\n",
		MAC2STRDBG(bssid), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Find BSS using i5_dm_interface_type and BSSID */
i5_dm_bss_type*
wbd_ds_get_i5_bss_in_ifr(i5_dm_interface_type *i5_ifr, unsigned char *bssid, int *error)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	/* Now Find the BSS */
	if (!bssid) {
		bssid = (unsigned char *)&ether_null;
	} else if (!(i5_bss = i5DmFindBSSFromInterface(i5_ifr, bssid))) {
		ret = WBDE_DS_UN_BSS;
	}

	WBD_DS("BSS["MACDBG"] Find : %s\n",
		MAC2STRDBG(bssid), wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Find BSS using i5_dm_interface_type and ifname */
i5_dm_bss_type*
wbd_ds_get_i5_bss_match_ifname_in_ifr(i5_dm_interface_type *i5_ifr, unsigned char *ifname,
	int *error)
{
	int ret = WBDE_OK;
	i5_dm_bss_type *i5_bss = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_ifr, WBDE_INV_ARG);
	WBD_ASSERT_ARG(ifname, WBDE_INV_ARG);

	if (!(i5_bss = i5DmGetBSSMatchingIfnameFromInterface(i5_ifr, ((const char*)ifname)))) {
		ret = WBDE_DS_UN_BSS;
	}
end:
	WBD_DS("BSS with ifname[%s] Find : %s\n", ifname, wbderrorstr(ret));
	if (error) {
		*error = ret;
	}
	WBD_EXIT();
	return i5_bss;
}

/* Traverse IEEE1905 BSS's Assoc STA List to find an Associated STA */
i5_dm_clients_type*
wbd_ds_find_sta_in_bss_assoclist(i5_dm_bss_type *i5_bss,
	struct ether_addr *find_sta_mac, int *error, wbd_assoc_sta_item_t **assoc_sta)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *i5_assoc_sta = NULL;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(find_sta_mac, WBDE_INV_ARG);

	if ((i5_assoc_sta = i5DmFindClientInBSS(i5_bss, (unsigned char*)find_sta_mac)) == NULL) {
		WBD_DS("Slave["MACDBG"] STA["MACF"]. %s\n",
			MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(find_sta_mac),
			wbderrorstr(WBDE_DS_UN_ASCSTA));
		*error = WBDE_DS_UN_ASCSTA;
		goto end;
	}

	if (assoc_sta) {
		if (!i5_assoc_sta->vndr_data) {
			wbd_assoc_sta_item_t *assoc_sta_new;

			WBD_WARNING("Slave["MACDBG"] STA["MACF"]. Vendor Data NULL. Allocate\n",
				MAC2STRDBG(i5_bss->BSSID), ETHERP_TO_MACF(find_sta_mac));
			assoc_sta_new = (wbd_assoc_sta_item_t*)
				wbd_malloc(sizeof(*assoc_sta_new), &ret);
			WBD_ASSERT();
			i5_assoc_sta->vndr_data = assoc_sta_new;
		}

		*assoc_sta = (wbd_assoc_sta_item_t*)i5_assoc_sta->vndr_data;
	}

end:
	WBD_EXIT();
	return i5_assoc_sta;

}

/* Find the sta in the i5 typology */
i5_dm_clients_type*
wbd_ds_find_sta_in_topology(unsigned char *sta_mac, int *err)
{
	int ret = WBDE_OK;
	i5_dm_network_topology_type *i5_topology = NULL;
	i5_dm_device_type *device = NULL;
	i5_dm_clients_type *sta = NULL;

	/* Validate args */
	WBD_ASSERT_ARG(sta_mac, WBDE_INV_ARG);
	WBD_ASSERT_ARG(err, WBDE_INV_ARG);

	ret = WBDE_DS_UN_ASCSTA;

	i5_topology = ieee1905_get_datamodel();
	foreach_i5glist_item(device, i5_dm_device_type, i5_topology->device_list) {

		/* Only in Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(device->flags)) {
			continue;
		}

		if ((sta = i5DmFindClientInDevice(device, sta_mac)) != NULL) {
			ret = WBDE_OK;
			break;
		}
	}

	*err = ret;
end:
	return sta;
}

/* Find the sta in the i5 Device */
i5_dm_clients_type*
wbd_ds_find_sta_in_device(i5_dm_device_type *i5_device,	unsigned char *sta_mac, int *err)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *sta = NULL;

	/* Validate args */
	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);
	WBD_ASSERT_ARG(sta_mac, WBDE_INV_ARG);
	WBD_ASSERT_ARG(err, WBDE_INV_ARG);

	ret = WBDE_DS_UN_ASCSTA;

	if ((sta = i5DmFindClientInDevice(i5_device, sta_mac)) != NULL) {
		ret = WBDE_OK;
	}

	*err = ret;
end:
	return sta;
}

/* This Init the device on the topology */
int
wbd_ds_device_init(i5_dm_device_type *i5_device)
{
	int ret = WBDE_OK;
	wbd_device_item_t *device_vndr_data; /* 1905 device item vendor data */
	WBD_ENTER();

	device_vndr_data = (wbd_device_item_t *) wbd_malloc(sizeof(*device_vndr_data), &ret);
	WBD_ASSERT_MSG("Failed to allocate memory for device_vndr_data\n");

	i5_device->vndr_data = (void*)device_vndr_data;
end:
	WBD_EXIT();
	return ret;

}

/* This Deinit the device on the topology */
int
wbd_ds_device_deinit(i5_dm_device_type *i5_device)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_device, WBDE_INV_ARG);

	if (i5_device->vndr_data) {
		free(i5_device->vndr_data);
		i5_device->vndr_data = NULL;
	}

end:
	WBD_EXIT();
	return ret;
}

/* This Init the interface on the device */
int
wbd_ds_interface_init(i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	wbd_ifr_item_t *ifr_vndr_data; /* 1905 interface item vendor data */
	WBD_ENTER();

	ifr_vndr_data = (wbd_ifr_item_t *) wbd_malloc(sizeof(*ifr_vndr_data), &ret);
	WBD_ASSERT_MSG("Failed to allocate memory for ifr_vndr_data\n");

	ifr_vndr_data->chan_info = (wbd_interface_chan_info_t *)wbd_malloc(
		sizeof(wbd_interface_chan_info_t), &ret);

	i5_ifr->vndr_data = (void*)ifr_vndr_data;
end:
	WBD_EXIT();
	return ret;

}

/* This Deinit the interface on the device */
int
wbd_ds_interface_deinit(i5_dm_interface_type *i5_ifr)
{
	int ret = WBDE_OK;
	wbd_ifr_item_t *ifr_vndr_data = NULL;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_ifr, WBDE_INV_ARG);

	if (i5_ifr->vndr_data) {

		ifr_vndr_data = (wbd_ifr_item_t*)i5_ifr->vndr_data;

#if defined(MULTIAPR2)
		/* Erase Last Fresh Channel Scan Request data saved on this Radio */
		if (ifr_vndr_data->last_fresh_chscan_req.num_of_opclass > 0) {
			i5DmGlistCleanup(&ifr_vndr_data->last_fresh_chscan_req.opclass_list);
			ifr_vndr_data->last_fresh_chscan_req.num_of_opclass = 0;
		}
#endif /* MULTIAPR2 */

		if (ifr_vndr_data->chan_info) {
			free(ifr_vndr_data->chan_info);
		}
		ifr_vndr_data->chan_info = NULL;
		free(i5_ifr->vndr_data);
		i5_ifr->vndr_data = NULL;
	}

end:
	WBD_EXIT();
	return ret;
}

/* This Init the BSS on the interface */
int
wbd_ds_bss_init(i5_dm_bss_type *i5_bss)
{
	int ret = WBDE_OK;
	wbd_bss_item_t *bss_vndr_data; /* 1905 BSS item vendor data */
	WBD_ENTER();

	bss_vndr_data = (wbd_bss_item_t *) wbd_malloc(sizeof(*bss_vndr_data), &ret);
	WBD_ASSERT_MSG("Failed to allocate memory for bss_vndr_data\n");

	/* Initialize list which holds the monitored STA list */
	wbd_ds_glist_init(&bss_vndr_data->monitor_sta_list);

	i5_bss->vndr_data = (void*)bss_vndr_data;
end:
	WBD_EXIT();
	return ret;

}

/* This Deinit the BSS on the interface */
int
wbd_ds_bss_deinit(i5_dm_bss_type *i5_bss)
{
	int ret = WBDE_OK;
	i5_dm_clients_type *i5_sta;
	wbd_bss_item_t *bss_vndr_data; /* 1905 BSS item vendor data */
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_bss, WBDE_INV_ARG);

	bss_vndr_data = i5_bss->vndr_data;

	/* Travese STA items associated with this BSS */
	foreach_i5glist_item(i5_sta, i5_dm_clients_type, i5_bss->client_list) {

		/* Remove a STA item from all peer BSS' Monitor STA List */
		ret = wbd_ds_remove_sta_fm_peer_devices_monitorlist(
			(struct ether_addr*)&i5_bss->BSSID, (struct ether_addr*)&i5_sta->mac,
			i5_bss->mapFlags);
	}

	if (bss_vndr_data) {
		/* Remove all STA items monitored by this BSS */
		wbd_ds_glist_cleanup(&(bss_vndr_data->monitor_sta_list));
		free(bss_vndr_data);
		i5_bss->vndr_data = NULL;
	}

end:
	WBD_EXIT();
	return ret;
}

/* This Init the STA on the BSS */
void
wbd_ds_sta_init(wbd_info_t *info, i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	uint8 band = WBD_BAND_LAN_INVALID;
	uint8 prb_band = WBD_BAND_LAN_2G;	/* By default supports 2G */
	wbd_assoc_sta_item_t *assoc_sta;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	WBD_ENTER();

	assoc_sta = (wbd_assoc_sta_item_t*)wbd_malloc(sizeof(*assoc_sta), &ret);
	WBD_ASSERT();

	i5_bss = (i5_dm_bss_type*)WBD_I5LL_PARENT(i5_assoc_sta);
	i5_ifr = (i5_dm_interface_type*)WBD_I5LL_PARENT(i5_bss);

	/* Initialize with the current time */
	gettimeofday(&assoc_sta->assoc_time, NULL);

	i5_assoc_sta->vndr_data = assoc_sta;

	if (i5_ifr->chanspec != 0) {
		band = WBD_BAND_FROM_CHANNEL(wf_chspec_ctlchan(i5_ifr->chanspec));
		prb_band |= band;
	}
	wbd_ds_add_item_to_probe_sta_table(info, (struct ether_addr*)i5_assoc_sta->mac,
		prb_band);

end:
	WBD_EXIT();
}

/* This Deinit the STA on the BSS */
void
wbd_ds_sta_deinit(i5_dm_clients_type *i5_assoc_sta)
{
	int ret = WBDE_OK;
	WBD_ENTER();

	WBD_ASSERT_ARG(i5_assoc_sta, WBDE_INV_ARG);

	if (i5_assoc_sta->vndr_data) {
		free(i5_assoc_sta->vndr_data);
		i5_assoc_sta->vndr_data = NULL;
	}

end:
	WBD_EXIT();
}

/* Check if FBT is possible on this 1905 Device or not */
int
wbd_ds_is_fbt_possible_on_agent()
{
#ifdef WLHOSTFBT
	unsigned char map = 0;
	struct ether_addr bssid;
	char wbd_ifnames[NVRAM_MAX_VALUE_LEN] = {0};
	char var_intf[IFNAMSIZ] = {0}, ifname[IFNAMSIZ] = {0}, *next_intf;
	WBD_ENTER();

	WBDSTRNCPY(wbd_ifnames, blanket_nvram_safe_get(WBD_NVRAM_IFNAMES),
		sizeof(wbd_ifnames));

	/* Traverse wbd_ifnames for each ifname */
	foreach(var_intf, wbd_ifnames, next_intf) {

		char prefix[IFNAMSIZ];

		/* Copy interface name temporarily */
		WBDSTRNCPY(ifname, var_intf, sizeof(ifname));

		/* Get prefix of the interface from Driver */
		blanket_get_interface_prefix(ifname, prefix, sizeof(prefix));

		/* Get the BSSID/MAC of the BSS */
		wbd_ether_atoe(blanket_nvram_prefix_safe_get(prefix, NVRAM_HWADDR), &bssid);

		/* Get IEEE1905_MAP_FLAG_XXX flag value for this BSS */
		map = wbd_get_map_flags(prefix);

		/* Loop for below activity, only for Fronthaul BSS */
		if (!I5_IS_BSS_FRONTHAUL(map)) {
			continue;
		}

		/* If FBT not possible on this BSS, go for next */
		if (!wbd_is_fbt_possible(prefix)) {
			continue;
		}

		/* FBT is possible on this BSS, return WBDE_OK */
		WBD_EXIT();
		return WBDE_OK;
	}

	WBD_EXIT();
#endif /* WLHOSTFBT */
	return WBDE_DS_FBT_NT_POS;
}

/* Traverse rclass list to find the rclass */
wbd_bcn_req_rclass_list_t*
wbd_ds_find_rclass_in_ap_chan_report(wbd_glist_t *ap_chan_report,
	unsigned char rclass, int* error)
{
	wbd_bcn_req_rclass_list_t *rclass_list = NULL, *ret_rclass_list = NULL;
	dll_t *rclass_list_p;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse each rclass list and get the rclass */
	foreach_glist_item(rclass_list_p, *ap_chan_report) {
		rclass_list = (wbd_bcn_req_rclass_list_t*)rclass_list_p;

		/* Check for rclass */
		if (rclass == rclass_list->rclass) {
			ret_rclass_list = rclass_list;
			goto end;
		}
	}
	WBD_DS("rclass[%d]. %s\n", rclass, wbderrorstr(WBDE_DS_UN_BCN_REPORT));
	*error = WBDE_DS_UN_BCN_REPORT;

end:
	WBD_EXIT();
	return ret_rclass_list;
}

/* Traverse channel list of rclass to find the channel */
wbd_bcn_req_chan_list_t*
wbd_ds_find_channel_in_rclass_list(wbd_bcn_req_rclass_list_t *rclass_list,
	unsigned char channel, int* error)
{
	wbd_bcn_req_chan_list_t *chan_list = NULL, *ret_chan_list = NULL;
	dll_t *chan_list_p;
	*error = WBDE_OK;
	WBD_ENTER();

	/* Traverse each channel list and get the channel */
	foreach_glist_item(chan_list_p, rclass_list->chan_list) {
		chan_list = (wbd_bcn_req_chan_list_t*)chan_list_p;

		/* Check for channel */
		if (channel == chan_list->channel) {
			ret_chan_list = chan_list;
			goto end;
		}
	}
	WBD_DS("channel[%d]. %s\n", channel, wbderrorstr(WBDE_DS_UN_BCN_REPORT));
	*error = WBDE_DS_UN_BCN_REPORT;

end:
	WBD_EXIT();
	return ret_chan_list;
}

/* Add an rclass in ap channel report */
int
wbd_ds_add_rclass_in_ap_chan_report(wbd_glist_t *ap_chan_report, unsigned char rclass,
	wbd_bcn_req_rclass_list_t** out_rclass_list)
{
	int ret = WBDE_OK;
	wbd_bcn_req_rclass_list_t* new_rclass_list = NULL;
	WBD_ENTER();

	/* Create a rclass list item */
	new_rclass_list = (wbd_bcn_req_rclass_list_t*)wbd_malloc(sizeof(*new_rclass_list), &ret);
	WBD_ASSERT();

	/* Fill & Initialize rclass */
	new_rclass_list->rclass = rclass;
	wbd_ds_glist_init(&new_rclass_list->chan_list);

	/* In the end, Add this new rclass list item to ap channel report */
	wbd_ds_glist_append(ap_chan_report, (dll_t *)new_rclass_list);

	if (out_rclass_list) {
		*out_rclass_list = new_rclass_list;
	}
	WBD_DS("rclass Added[%d] in ap channel report\n", rclass);
end:
	WBD_EXIT();
	return ret;
}

/* Add an channel in rclass list */
int
wbd_ds_add_channel_in_rclass_list(wbd_bcn_req_rclass_list_t *rclass_list, unsigned char channel,
	wbd_bcn_req_chan_list_t** out_channel_list)
{
	int ret = WBDE_OK;
	wbd_bcn_req_chan_list_t* new_channel_list = NULL;
	WBD_ENTER();

	/* Create a channel list item */
	new_channel_list = (wbd_bcn_req_chan_list_t*)wbd_malloc(sizeof(*new_channel_list), &ret);
	WBD_ASSERT();

	/* Fill & Initialize channel */
	new_channel_list->channel = channel;

	/* In the end, Add this new channel list item to ap channel report */
	wbd_ds_glist_append(&rclass_list->chan_list, (dll_t *)new_channel_list);

	if (out_channel_list) {
		*out_channel_list = new_channel_list;
	}
	WBD_DS("channel Added[%d] in rclass list[%d]\n", channel, rclass_list->rclass);
end:
	WBD_EXIT();
	return ret;
}

/* Free & Cleanup ap channel report from heap */
int
wbd_ds_ap_chan_report_cleanup(wbd_glist_t *ap_chan_report)
{
	int ret = WBDE_OK;
	wbd_bcn_req_rclass_list_t* rclass_list;
	dll_t *rclass_list_item_p, *rclass_list_next_p;
	WBD_ENTER();

	/* Validate arg */
	WBD_ASSERT_ARG(ap_chan_report, WBDE_INV_ARG);

	/* Traverse br0 rclass_list */
	foreach_safe_glist_item(rclass_list_item_p, *ap_chan_report,
		rclass_list_next_p) {

		/* need to keep next item incase we remove node in between */
		rclass_list_next_p = dll_next_p(rclass_list_item_p);

		rclass_list = (wbd_bcn_req_rclass_list_t*)rclass_list_item_p;

		/* Free & Cleanup channel list */
		wbd_ds_glist_cleanup(&rclass_list->chan_list);

		/* In the end, Remove rclass list item itself */
		wbd_ds_glist_delete(ap_chan_report, rclass_list_item_p);
		free(rclass_list);
	}

end:
	WBD_DS("Blanket rclass_list Cleanup %s\n", wbderrorstr(ret));
	WBD_EXIT();
	return ret;
}

/* timeout probe STA list */
void
wbd_ds_timeout_prbsta(bcm_usched_handle *hdl, void *arg)
{
	int ret = WBDE_OK, idx;
	wbd_info_t *info = (wbd_info_t*)arg;
	wbd_prb_sta_t *sta, *next, *head, *prev;
	i5_dm_clients_type *i5_sta;
	time_t now = time(NULL);

	WBD_ENTER();
	WBD_PROBE("now[%lu]\n", (unsigned long)now);

	for (idx = 0; idx < WBD_PROBE_HASH_SIZE; idx++) {
		sta = info->prb_sta[idx];
		head = NULL;
		prev = NULL;

		while (sta) {
			WBD_PROBE("sta[%p]:"MACF" active[%lu]\n",
				sta, ETHER_TO_MACF(sta->sta_mac),
				(unsigned long)(sta->active));
			if (now - sta->active > info->max.tm_probe) {
				/* remove from the probe list only if the STA is not found */
				i5_sta = wbd_ds_find_sta_in_topology((unsigned char*)&sta->sta_mac,
					&ret);
				if (!i5_sta) {
					next = sta->next;
					WBD_PROBE("sta[%p]:"MACF" now[%lu] timestamp[%lu]\n",
						sta, ETHER_TO_MACF(sta->sta_mac),
						(unsigned long)now,
						(unsigned long)(sta->active));

					free(sta);
					sta = next;

					if (prev)
						prev->next = sta;

					continue;
				} else {
					WBD_PROBE("sta[%p]:"MACF" In Assoclist Dont remove\n",
						sta, ETHER_TO_MACF(sta->sta_mac));
				}
			}

			if (head == NULL)
				head = sta;

			prev = sta;
			sta = sta->next;
		}
		info->prb_sta[idx] = head;
	}

	WBD_EXIT();
}

/* Check if the given interface is dedicated backhaul */
int wbd_ds_is_interface_dedicated_backhaul(i5_dm_interface_type *interface)
{
	i5_dm_bss_type *bss;
	int BackHaulBSS = 0;

	if (!interface) {
		WBD_ERROR("NULL Interface passed in\n");
		return -1;
	}
	foreach_i5glist_item(bss, i5_dm_bss_type, interface->bss_list) {
		if (I5_IS_BSS_FRONTHAUL(bss->mapFlags)) {
			return 0;
		}
		BackHaulBSS |= I5_IS_BSS_BACKHAUL(bss->mapFlags);
	}
	return BackHaulBSS;
}

/* Count the number of Fronthaul BSSs in the network */
int
wbd_ds_count_fhbss()
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	int count = 0;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If fronthaul BSS increment the count */
				if (I5_IS_BSS_FRONTHAUL(i5_bss->mapFlags)) {
					count++;
				}
			}
		}
	}

	return count;
}

/* Count the number of backhaul STAs in the network */
int
wbd_ds_count_bstas()
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	int count = 0;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If not backhaul BSS continue */
				if (!I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
					continue;
				}

				foreach_i5glist_item(i5_sta, i5_dm_clients_type,
					i5_bss->client_list) {

					if (I5_CLIENT_IS_BSTA(i5_sta)) {
						count++;
					}
				}
			}
		}
	}

	return count;
}

/* Does the backhaul STA device has the backhaul BSS in it */
int
wbd_ds_is_bsta_device_has_bbss(unsigned char *sta_mac)
{
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;

	/* Find the interface in the network which is matching the STA MAC */
	if ((i5_ifr = i5DmFindInterfaceFromNetwork(sta_mac)) == NULL) {
		WBD_DS("STA["MACDBG"] Not found\n", MAC2STRDBG(sta_mac));
		goto end;
	}

	i5_device = (i5_dm_device_type*)WBD_I5LL_PARENT(i5_ifr);
	return I5_HAS_BH_BSS(i5_device->flags) ? 1 : 0;
end:
	return 0;
}

/* Get STA for which the backhaul optimization is pending */
i5_dm_clients_type*
wbd_ds_get_bh_opt_pending_sta()
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t *assoc_sta;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If not backhaul BSS continue */
				if (!I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
					continue;
				}

				foreach_i5glist_item(i5_sta, i5_dm_clients_type,
					i5_bss->client_list) {

					if (!I5_CLIENT_IS_BSTA(i5_sta)) {
						continue;
					}
					assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
					if (assoc_sta &&
						!WBD_ASSOC_ITEM_IS_BH_OPT_DONE(assoc_sta->flags)) {
						return i5_sta;
					}
				}
			}
		}
	}

	return NULL;
}

/* Get backhaul optimization STA based on the assoc item flags */
i5_dm_clients_type*
wbd_ds_get_bh_opt_sta(uint32 flags)
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t *assoc_sta;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If not backhaul BSS continue */
				if (!I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
					continue;
				}

				foreach_i5glist_item(i5_sta, i5_dm_clients_type,
					i5_bss->client_list) {

					if (!I5_CLIENT_IS_BSTA(i5_sta)) {
						continue;
					}
					assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
					if (assoc_sta && (flags & assoc_sta->flags)) {
						return i5_sta;
					}
				}
			}
		}
	}

	return NULL;
}

/* Unset the backhaul optimization flags of all the backhaul STAs */
void
wbd_ds_unset_bh_opt_flags(uint32 flags)
{
	i5_dm_network_topology_type *i5_topology;
	i5_dm_device_type *i5_device;
	i5_dm_interface_type *i5_ifr;
	i5_dm_bss_type *i5_bss;
	i5_dm_clients_type *i5_sta;
	wbd_assoc_sta_item_t *assoc_sta;

	i5_topology = ieee1905_get_datamodel();

	foreach_i5glist_item(i5_device, i5_dm_device_type, i5_topology->device_list) {
		/* Loop for only for Agent Devices */
		if (!I5_IS_MULTIAP_AGENT(i5_device->flags)) {
			continue;
		}

		foreach_i5glist_item(i5_ifr, i5_dm_interface_type, i5_device->interface_list) {
			foreach_i5glist_item(i5_bss, i5_dm_bss_type, i5_ifr->bss_list) {
				/* If not backhaul BSS continue */
				if (!I5_IS_BSS_BACKHAUL(i5_bss->mapFlags)) {
					continue;
				}

				foreach_i5glist_item(i5_sta, i5_dm_clients_type,
					i5_bss->client_list) {

					if (!I5_CLIENT_IS_BSTA(i5_sta)) {
						continue;
					}
					assoc_sta = (wbd_assoc_sta_item_t*)i5_sta->vndr_data;
					if (assoc_sta) {
						assoc_sta->flags &= ~flags;
					}
				}
			}
		}
	}
}
