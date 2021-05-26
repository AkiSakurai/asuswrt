/*
 * WBD PLC utility for slave
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
 * $Id: wbd_plc_utility.c 732754 2017-11-21 04:09:45Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <bcmnvram.h>
#include <typedefs.h>
#include <shutils.h>
#include <wlutils.h>
#include <wlioctl.h>
#include <bcmendian.h>
#include <common_utils.h>

#ifdef PLC_WBD
/* This file is only compiled when PLC_WBD is enabled */
#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_wl_utility.h"
#include "wbd_plc_utility.h"
#include "homeplugctl_drv/l2_driver.h"

#define WBD_PLC_PHY_RATE_DIV (2)

extern struct ether_addr *ether_aton(const char *asc);

static char dpw[] = "This is our Device Password given to the peer STA";

/* Get local PLC MAC address */
int
wbd_plc_get_local_plc_mac(wbd_plc_info_t *plc_info, struct ether_addr *mac)
{
	eacopy(&plc_info->plc_mac, mac);
	return WBDE_OK;
}

/* Returns 1 if it is the PLC is enabled */
int
wbd_plc_is_enabled(wbd_plc_info_t *plc_info)
{
	return (plc_info->enabled);
}

/* Initialize PLC interface */
int
wbd_plc_init(wbd_plc_info_t *plc_info)
{
	int ret = WBDE_OK;
	char *tmp;
	struct ether_addr *addr;

	WBD_ENTER();

	/* Initializing enabled flag to 0 */
	plc_info->enabled = 0;

	/* Get the PLC intreface name */
	tmp = blanket_nvram_safe_get(WBD_NVRAM_PLCIF);
	if (tmp == NULL || strlen(tmp) == 0) {
		WBD_WARNING("%s : %s\n", wbderrorstr(WBDE_UNDEF_NVRAM), WBD_NVRAM_PLCIF);
		ret = WBDE_PLC_BAD_CONF;
		goto end;
	}
	strncpy(plc_info->ifname, tmp, sizeof(plc_info->ifname));

	/* Get the PLC mac address */
	tmp = blanket_nvram_safe_get(WBD_NVRAM_PLCMAC);
	if (tmp == NULL || strlen(tmp) == 0) {
		WBD_WARNING("%s : %s\n", wbderrorstr(WBDE_UNDEF_NVRAM), WBD_NVRAM_PLCMAC);
		ret = WBDE_PLC_BAD_CONF;
		goto end;
	}

	/* Check if the MAC address defined in NVRAM is in correct format or not */
	if ((addr = ether_aton(tmp)) == NULL) {
		WBD_WARNING("%s : %s = %s\n", wbderrorstr(WBDE_INV_NVVAL),
			WBD_NVRAM_PLCMAC, tmp);
		ret = WBDE_PLC_BAD_CONF;
		goto end;
	}
	eacopy(addr, &plc_info->plc_mac);

	/* Initialize PLC L2 library with the appropriate interface */
	l2driver_ini(plc_info->ifname);

	/* Establish PLC target endpoint using the local PLC MAC ADDRESS */
	if (l2driver_open(tmp, dpw) == 0) {
		WBD_WARNING("%s %s\n", wbderrorstr(WBDE_PLC_OPEN_MAC), tmp);
		/* De-initialize PLC L2 library */
		l2driver_close(plc_info->ifname);
		ret = WBDE_PLC_OPEN_MAC;
		goto end;
	}

	WBD_INFO("Local PLC device "MACF" at %s\n", ETHER_TO_MACF(plc_info->plc_mac),
		plc_info->ifname);
	plc_info->enabled = 1;

end:
	WBD_EXIT();
	return ret;
}

/* Get PLC assoc info */
int
wbd_plc_get_assoc_info(wbd_plc_info_t *plc_info, wbd_plc_assoc_info_t **out_assoc_info)
{
	tS_APCM_NW_STATS_Result apcm_nw_stats_result;
	tS_APCM_NW_STATS_REQ apcm_nw_stats_req;
	apcm_nw_stats_req.BandIdentifier = LOW_BAND;
	tS_PHYRates *p_sta;
	wbd_plc_assoc_info_t *assoc_info = NULL;
	int ret = WBDE_OK;
	int count = 0;
	int n_stas;

	WBD_ENTER();

	if (!plc_info->enabled) {
		WBD_ERROR("%s\n", wbderrorstr(WBDE_PLC_DISABLED));
		return WBDE_PLC_DISABLED;
	}

	/* Launch Network stats primitive to get the PLC-associated nodes */
	Exec_APCM_NW_STATS(apcm_nw_stats_req,  &apcm_nw_stats_result);
	if (apcm_nw_stats_result.result != CNF_ARRIVED) {
		WBD_ERROR("Getting PLC NW stats (result = %d)\n", apcm_nw_stats_result.result);
		return WBDE_PLC_L2_ERROR;
	}
	/* Get the number of PLC-associated nodes from the primitive confirmation */
	n_stas = apcm_nw_stats_result.cnf.NumSTAs;

	if (n_stas > 0) {
		/* Allocate buffer to holde assoclist */
		assoc_info = (wbd_plc_assoc_info_t*) wbd_malloc(sizeof(wbd_plc_assoc_info_t) +
				((n_stas-1) * sizeof(wbd_plc_node_info_t)), &ret);
		WBD_ASSERT();

		p_sta = apcm_nw_stats_result.cnf.first;
		while (p_sta) {
			eacopy(p_sta->DA, &assoc_info->node[count].mac);
			assoc_info->node[count].tx_rate = (float)p_sta->AvgPHYDR_TX /
				(float)WBD_PLC_PHY_RATE_DIV;
			assoc_info->node[count].rx_rate = (float)p_sta->AvgPHYDR_RX /
				(float)WBD_PLC_PHY_RATE_DIV;
			count++;
			p_sta = p_sta->next;
		}
	}
	assoc_info->count = count;

	if (out_assoc_info) {
		*out_assoc_info = assoc_info;
	}

end:
	/* Free confirmation structure */
	Free_APCM_NW_STATS_CNF(&apcm_nw_stats_result.cnf);
	WBD_EXIT();
	return ret;
}
#endif /* PLC_WBD */
