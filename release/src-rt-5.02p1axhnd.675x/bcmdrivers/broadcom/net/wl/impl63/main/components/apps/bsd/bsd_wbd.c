/*
 * WBD Related functions
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
 * $Id: bsd_wbd.c 780861 2019-11-05 07:14:27Z $
 */

#include "bsd.h"
#include <fcntl.h>

/* General NVRAMs */
#define NVRAM_BR0_IFNAMES	"br0_ifnames"
#define NVRAM_BR1_IFNAMES	"br1_ifnames"

/* Wi-Fi Blanket Bridge Types of Interfaces */
#define	WBD_BRIDGE_INVALID	-1	/* -1 Bridge Type is Invalid */
#define	WBD_BRIDGE_LAN		0	/*  0 Bridge Type is LAN */
#define	WBD_BRIDGE_GUEST	1	/*  1 Bridge Type is GUEST */
#define WBD_BRIDGE_VALID(bridge) ((((bridge) < WBD_BRIDGE_LAN) || \
					((bridge) > WBD_BRIDGE_GUEST)) ? (0) : (1))

/* Wi-Fi Blanket Band Types */
#define	WBD_BAND_LAN_INVALID	0x00	/* 0 - auto-select */
#define	WBD_BAND_LAN_2G		0x01	/* 1 - 2.4 Ghz */
#define	WBD_BAND_LAN_5GL	0x02	/* 2 - 5 Ghz LOW */
#define	WBD_BAND_LAN_5GH	0x04	/* 4 - 5 Ghz HIGH */
#define	WBD_BAND_LAN_ALL	(WBD_BAND_LAN_2G | WBD_BAND_LAN_5GL | WBD_BAND_LAN_5GH)

#ifndef WBDSTRNCPY
#define WBDSTRNCPY(dst, src, len)	 \
	do { \
		strncpy(dst, src, len -1); \
		dst[len - 1] = '\0'; \
	} while (0)
#endif // endif

/* Read "wbd_ifnames" NVRAM and get actual ifnames */
extern int wbd_read_actual_ifnames(char *wbd_ifnames1, int len1, bool create);

static wbd_weak_sta_policy_t wbd_predefined_policy[] = {
/* idle_rate rssi phyrate tx_failures flags */
/* 0: low rssi or phyrate or high tx_failure */
{1000, -65, 3, 50, 20, 0x000001C},
/* End */
{0, 0, 0, 0, 0}
};

static int bsd_wbd_find_weak_sta_policy(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo,
	bsd_sta_info_t *sta, int *weak_flag);

typedef int (*bsd_wbd_algo_t)(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo,
	bsd_sta_info_t *sta, int *weak_flag);

/* Weak STA identification Algorithm for WBD, should be only one and controlled by config
 * Currently the Index 0 is for BSD and index 1 for WBD's internal weak STA alogirthm.
 * This list should be mutually exclusive. Lets say if we add one algorithm here,
 * then NULL needs to be added to the list in WBD(variable predefined_wbd_wc_algo in file
 * wbd/slave/wbd_slave_control.c) at the same index.
 */
static bsd_wbd_algo_t wbd_predefined_algo[] = {
	bsd_wbd_find_weak_sta_policy,
	NULL
};

#define BSD_WBD_MAX_POLICY (sizeof(wbd_predefined_policy)/sizeof(wbd_weak_sta_policy_t) - 1)
#define BSD_WBD_MAX_ALGO (sizeof(wbd_predefined_algo)/sizeof(bsd_wbd_algo_t))

#define BSD_WBD_MIN_PHYRATE	6

static int
bsd_wbd_get_max_algo(bsd_info_t *info)
{
	UNUSED_PARAMETER(info);
	return BSD_WBD_MAX_ALGO;
}

static int
bsd_wbd_get_max_policy(bsd_info_t *info)
{
	UNUSED_PARAMETER(info);
	return BSD_WBD_MAX_POLICY;
}

static wbd_weak_sta_policy_t*
bsd_wbd_get_weak_sta_cfg(bsd_wbd_bss_list_t *wbd_bssinfo)
{
	return &wbd_predefined_policy[wbd_bssinfo->policy];
}

static char*
bsd_wbd_nvram_safe_get(const char *name, int *status)
{
	char *p = NULL;

	p = nvram_get(name);
	*status = p ? BSD_OK : BSD_FAIL;

	return p ? p : "";
}

/* Allocate WBD info */
static bsd_wbd_info_t*
bsd_wbd_info_alloc(void)
{
	bsd_wbd_info_t *info;

	BSD_ENTER();

	info = (bsd_wbd_info_t *)malloc(sizeof(*info));
	if (info == NULL) {
		BSD_PRINT("malloc fails\n");
	} else {
		memset(info, 0, sizeof(*info));
		BSD_WBD("info=%p\n", info);
	}

	BSD_EXIT();
	return info;
}

static bsd_wbd_bss_list_t*
bsd_wbd_add_bssinfo(bsd_wbd_info_t *wbd_info, bsd_bssinfo_t *bssinfo)
{
	bsd_wbd_bss_list_t *newnode;
	BSD_ENTER();

	newnode = (bsd_wbd_bss_list_t*)malloc(sizeof(*newnode));
	if (newnode == NULL) {
		BSD_WBD("Failed to allocate memory for bsd_wbd_bss_list_t\n");
		return NULL;
	}
	memset(newnode, 0, sizeof(*newnode));

	newnode->weak_sta_cfg =
		(wbd_weak_sta_policy_t*)malloc(sizeof(*(newnode->weak_sta_cfg)));
	if (newnode->weak_sta_cfg == NULL) {
		BSD_WBD("Failed to allocate memory for weak_sta_cfg\n");
		free(newnode);
		return NULL;
	}
	memset(newnode->weak_sta_cfg, 0, sizeof(*(newnode->weak_sta_cfg)));

	newnode->bssinfo = bssinfo;
	newnode->next = wbd_info->bss_list;
	wbd_info->bss_list = newnode;

	BSD_EXIT();
	return newnode;
}

/* Gets WBD Band Enumeration from ifname's Chanspec & Bridge Type */
int
bsd_wbd_identify_wbd_band_type(int bridge_dgt, chanspec_t ifr_chanspec, int *out_band)
{
	int ret = BSD_OK, channel = 0;
	BSD_ENTER();

	if (!out_band) {
		ret = BSD_FAIL;
		goto end;
	}
	*out_band = WBD_BAND_LAN_INVALID;

	/* Check if Bridge type is valid */
	if (!WBD_BRIDGE_VALID(bridge_dgt)) {
		goto end;
	}

	/* Fetch Current Channel form Chanspec */
	channel = CHSPEC_CHANNEL(ifr_chanspec);

	/* If Bridge type is LAN */
	if (bridge_dgt == WBD_BRIDGE_LAN) {

		/* If Current Channel is in Band 2G */
		if (CHSPEC_IS2G(ifr_chanspec)) {

			/* WBD band type is LAN_2G */
			*out_band = WBD_BAND_LAN_2G;
			goto end;

		/* If Current Channel is in Band 5G */
		} else if (CHSPEC_IS5G(ifr_chanspec)) {
			char *val = nvram_safe_get(BSD_WBD_NVRAM_MCHAN_SLAVE);
			int wbd_mchan = 0;

			if (val && (val[0] != '\0')) {
				wbd_mchan = strtoul(val, NULL, 0);
			}

			if (wbd_mchan) {
				*out_band = WBD_BAND_LAN_5GL;
				goto end;
			}

			/* If Current Channel is less than 100 */
			if (channel < 100) {

				/* WBD band type is LAN_5GL */
				*out_band = WBD_BAND_LAN_5GL;
				goto end;

			/* Else If Current Channel is greater than or equal to 100 */
			} else {

				/* WBD band type is LAN_5GH */
				*out_band = WBD_BAND_LAN_5GH;
				goto end;

			}

		/* If Band is not 2G, not 5G, Invalid Scenario */
		} else {
			goto end;
		}

	/* If Bridge type is GUEST */
	} else if (bridge_dgt == WBD_BRIDGE_GUEST) {

		/* Can be extended for Guest Netowrks */
	}

end:
	BSD_WBD("Bridge[br%d] Chanspec[0x%x] Channel[%d] WBD_BAND[%d]\n",
		bridge_dgt, ifr_chanspec, channel,
		((out_band) ? *out_band : WBD_BAND_LAN_INVALID));

	BSD_EXIT();
	return ret;
}

/* To get the digit of Bridge in which Interface belongs */
int
bsd_wbd_get_ifname_bridge(char* ifname, int *out_bridge)
{
	int ret = BSD_OK;
	char brX_ifnames[128] = {0};
	BSD_ENTER();

	/* Validate arg */
	if (!ifname || !out_bridge) {
		ret = BSD_FAIL;
		goto end;
	}
	*out_bridge = WBD_BRIDGE_INVALID;

	/* Find ifname in br0_ifnames (LAN Network) */
	WBDSTRNCPY(brX_ifnames, nvram_safe_get(NVRAM_BR0_IFNAMES), sizeof(brX_ifnames) - 1);
	if (strlen(brX_ifnames) > 0) {
		if (find_in_list(brX_ifnames, ifname)) {
			*out_bridge = WBD_BRIDGE_LAN;
			goto end;
		}
	}

	/* Find ifname in br1_ifnames (Guest Network) */
	WBDSTRNCPY(brX_ifnames, nvram_safe_get(NVRAM_BR1_IFNAMES), sizeof(brX_ifnames) - 1);
	if (strlen(brX_ifnames) > 0) {
		if (find_in_list(brX_ifnames, ifname)) {
			*out_bridge = WBD_BRIDGE_GUEST;
			goto end;
		}
	}

	/* Control reached here, means ifname is not in br0 or br1, which is invalid scenario */
	ret = BSD_FAIL;

end:
	BSD_EXIT();
	return ret;
}

/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
int
bsd_wbd_identify_band_type(bsd_bssinfo_t *bssinfo, int *out_band)
{
	int ret = BSD_OK, bridge_dgt = WBD_BRIDGE_LAN;
	BSD_ENTER();

	if (!out_band) {
		ret = BSD_FAIL;
		goto end;
	}
	*out_band = WBD_BAND_LAN_INVALID;

	/* Get Bridge from interface name */
	ret = bsd_wbd_get_ifname_bridge(bssinfo->ifnames, &bridge_dgt);
	if (ret != BSD_OK) {
		ret = BSD_FAIL;
		goto end;
	}

	/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
	ret = bsd_wbd_identify_wbd_band_type(bridge_dgt, bssinfo->chanspec, out_band);
	if (ret != BSD_OK) {
		ret = BSD_FAIL;
		goto end;
	}

end:
	BSD_WBD("ifname[%s] Bridge[%d] Chanspec[0x%x] WBD_BAND[%d]\n",
		bssinfo->ifnames, bridge_dgt, bssinfo->chanspec,
		((out_band) ? *out_band : WBD_BAND_LAN_INVALID));
	BSD_EXIT();
	return ret;
}

/* Initializes the bss info for WBD */
static int
bsd_wbd_init_bss(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo)
{
	char *str, *endptr = NULL, *prefix;
	char tmp[100];
	int ret, num;
	wbd_weak_sta_policy_t policy;

	prefix = wbd_bssinfo->bssinfo->primary_prefix;
	/* index to weak STA finding algorithm. */
	str = bsd_wbd_nvram_safe_get(strcat_r(prefix, BSD_WBD_NVRAM_WEAK_STA_ALGO, tmp), &ret);
	if (ret == BSD_OK) {
		wbd_bssinfo->algo = (uint8)strtol(str, &endptr, 0);
		if (wbd_bssinfo->algo >= bsd_wbd_get_max_algo(info))
			wbd_bssinfo->algo = 0;
		if (wbd_predefined_algo[wbd_bssinfo->algo] == NULL) {
			BSD_WBD("WBD Algo[%d] is NULL\n", wbd_bssinfo->algo);
			return BSD_FAIL;
		}
	}

	/* Index to weak STA finding configuration */
	str = bsd_wbd_nvram_safe_get(strcat_r(prefix, BSD_WBD_NVRAM_WEAK_STA_POLICY, tmp), &ret);
	if (ret == BSD_OK) {
		wbd_bssinfo->policy = (uint8)strtol(str, &endptr, 0);
		if (wbd_bssinfo->policy >= bsd_wbd_get_max_policy(info))
			wbd_bssinfo->policy = 0;
	}

	memcpy(wbd_bssinfo->weak_sta_cfg, bsd_wbd_get_weak_sta_cfg(wbd_bssinfo),
		sizeof(*wbd_bssinfo->weak_sta_cfg));

	/* Configurations defined in NVRAM */
	str = bsd_wbd_nvram_safe_get(strcat_r(prefix, BSD_WBD_NVRAM_WEAK_STA_CFG, tmp), &ret);

	if (ret == BSD_OK) {
		num = sscanf(str, "%d %d %d %d %d %x",
			&policy.t_idle_rate, &policy.t_rssi, &policy.t_hysterisis,
			&policy.t_tx_rate, &policy.t_tx_failures, &policy.flags);
		if (num == 6) {
			memcpy(wbd_bssinfo->weak_sta_cfg, &policy,
				sizeof(*wbd_bssinfo->weak_sta_cfg));
		} else {
			BSD_ERROR("BSS[%s] %s[%s] format error\n", wbd_bssinfo->bssinfo->ifnames,
				BSD_WBD_NVRAM_WEAK_STA_CFG, str);
		}
	}

	/* Get WBD Band Enumeration from ifname's Chanspec & Bridge Type */
	bsd_wbd_identify_band_type(wbd_bssinfo->bssinfo, &wbd_bssinfo->wbd_band_type);

	BSD_WBD("Band[%d] Algo[%d] weak_sta_policy[%d]: "
		"idle_rate=%d rssi=%d Hysterisis=%d "
		"phyrate=%d tx_failures=%d flags=0x%x\n",
		wbd_bssinfo->wbd_band_type,
		wbd_bssinfo->algo, wbd_bssinfo->policy,
		wbd_bssinfo->weak_sta_cfg->t_idle_rate,
		wbd_bssinfo->weak_sta_cfg->t_rssi,
		wbd_bssinfo->weak_sta_cfg->t_hysterisis,
		wbd_bssinfo->weak_sta_cfg->t_tx_rate,
		wbd_bssinfo->weak_sta_cfg->t_tx_failures,
		wbd_bssinfo->weak_sta_cfg->flags);

	return BSD_OK;
}

/* As there is no deault ifnames for BSD, setting ifnames for bsd to detect weak client for
 * WBD if there is no ifnames for BSD.
 */
int
bsd_wbd_set_ifnames(bsd_info_t *info)
{
	char bsd_ifnames[80] = {0}, wbd_ifnames[80] = {0};
	char lan_ifnames[100] = {0};
	char tmp[10];

	char var_intf[BSD_IFNAME_SIZE], prefix[BSD_IFNAME_SIZE];
	char *next_intf, *nvval;
	int idx_intf, idx;

	BSDSTRNCPY(bsd_ifnames, nvram_safe_get(BSD_IFNAMES_NVRAM), sizeof(bsd_ifnames) - 1);
	BSDSTRNCPY(lan_ifnames, nvram_safe_get(LAN_IFNAMES_NVRAM), sizeof(lan_ifnames) - 1);
	BSD_INFO("lan_ifnames: [%s]\n", lan_ifnames);

	/* Read "wbd_ifnames" NVRAM and get actual ifnames */
	if (wbd_read_actual_ifnames(wbd_ifnames, sizeof(wbd_ifnames), FALSE) != 0) {
		BSD_EXIT();
		return BSD_FAIL;
	}
	BSD_WBD("nvram %s=%s and %s=%s\n", BSD_IFNAMES_NVRAM, bsd_ifnames,
		BSD_WBD_NVRAM_IFNAMES, wbd_ifnames);

	if (wbd_ifnames[0] == '\0') {
		BSD_WBD("%s not specified. So skipping\n", BSD_WBD_NVRAM_IFNAMES);
		return BSD_FAIL;
	}

	if (bsd_ifnames[0] != '\0') {
		goto end;
	}

	/* Add only fronthaul interfaces to bsd_ifnames */
	foreach(var_intf, wbd_ifnames, next_intf) {

		/* Avoid Guest network, because guest steering not supported yet */
		if (!find_in_list(lan_ifnames, var_intf)) {
			BSD_INFO("Interface [%s] is missing in lan_ifnames. "
				"Assuming Guest, not adding to bsd_ifnames\n", var_intf);
			continue;
		}

		/* Get prefix of the interface */
		if (wl_ioctl(var_intf, WLC_GET_INSTANCE, &idx_intf, sizeof(idx_intf))) {
			BSD_ERROR("Err: failed to get WL instance of %s!\n", var_intf);
			continue;
		}
		BSD_INFO("Get WL instance of %s: %d\n", var_intf, idx_intf);

		if (strstr(var_intf, ".")) {
			if ((get_ifname_unit(var_intf, NULL, &idx) != 0) ||
				(idx >= WL_MAXBSSCFG)) {
				BSD_ERROR("Failed to decode interface format of %s!\n", var_intf);
				continue;
			}
		} else {
			idx = 0;
		}
		BSD_INFO("Get subunit of %s: %d\n", var_intf, idx);

		if (idx == 0) {
			snprintf(prefix, sizeof(prefix), "wl%d_", idx_intf);
		} else {
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", idx_intf, idx);
		}
		/* Read map to check if interface is fronthaul */
		nvval = nvram_get(strcat_r(prefix, "map", tmp));
		if (nvval && ((strcmp(nvval, "1") == 0) || (strcmp(nvval, "3") == 0))) {
			add_to_list(var_intf, bsd_ifnames, sizeof(bsd_ifnames));
		}
	}

	nvram_set(BSD_IFNAMES_NVRAM, bsd_ifnames);
	BSD_WBD("Successfully set bsd_ifnames same as %s = %s\n", BSD_WBD_NVRAM_IFNAMES,
		bsd_ifnames);
end:
	return BSD_OK;
}

/* Allocate and initialize WBD related information */
int
bsd_wbd_init(bsd_info_t *info)
{
	bsd_wbd_info_t *wbd_info;
	char ifnames[64], name[IFNAMSIZ], *next = NULL;
	int idx_intf, idx, ret = BSD_OK;
	bsd_intf_info_t *intf_info;
	bsd_bssinfo_t *bssinfo;
	bsd_wbd_bss_list_t *cur;

	BSD_ENTER();

	if ((info->enable_flag & BSD_FLAG_WBD_ENABLED) == 0)
		goto done;

	/* Get the ifnames */
	/* Read "wbd_ifnames" NVRAM and get actual ifnames */
	if (wbd_read_actual_ifnames(ifnames, sizeof(ifnames), FALSE) != 0) {
		BSD_EXIT();
		return BSD_FAIL;
	}
	BSD_WBD("wbd_ifnames=%s\n", ifnames);

	wbd_info = bsd_wbd_info_alloc();
	if (wbd_info == NULL) {
		BSD_EXIT();
		return BSD_FAIL;
	}
	info->wbd_info = wbd_info;

	/* Now for each ifname find corresponding bss_info structure */
	foreach(name, ifnames, next) {
		/* For this ifname find the bssinfo */
		for (idx_intf = 0; idx_intf < info->max_ifnum; idx_intf++) {
			intf_info = &(info->intf_info[idx_intf]);
			for (idx = 0; idx < WL_MAXBSSCFG; idx++) {
				bssinfo = &(intf_info->bsd_bssinfo[idx]);
				if (bssinfo->valid &&
					(strncmp(name, bssinfo->ifnames, strlen(name)) == 0)) {
					BSD_WBD("Matching name=%s and bssinfo->ifnames=%s. BSSINFO"
						" ssid=%s BSSID="MACF"\n", name, bssinfo->ifnames,
						bssinfo->ssid, ETHER_TO_MACF(bssinfo->bssid));
					cur = bsd_wbd_add_bssinfo(wbd_info, bssinfo);
					if (cur) {
						ret = bsd_wbd_init_bss(info, cur);
						if (ret != BSD_OK)
							goto done;
					}
					break;
				}
			}
			if (idx < WL_MAXBSSCFG) {
				break;
			}
		}
	}

done:
	BSD_EXIT();
	return ret;
}

/* Update WBD related information */
void
bsd_wbd_reinit(bsd_info_t *info)
{
	bsd_wbd_info_t *wbd_info;
	bsd_bssinfo_t *bssinfo;
	bsd_wbd_bss_list_t *cur;

	BSD_ENTER();

	/* If WBD Flag is not enabled, Leave */
	if ((info->enable_flag & BSD_FLAG_WBD_ENABLED) == 0) {
		BSD_WBD("WBD enable_flag[%d]. So skipping\n", info->enable_flag);
		goto end;
	}

	/* Validate Pointer to WBD Info Structure */
	if (!info->wbd_info) {
		goto end;
	}

	/* Get Pointer to WBD Info Structure & Head of BSS Info List */
	wbd_info = info->wbd_info;
	cur = wbd_info->bss_list;

	/* Validate Head of BSS Info List */
	if (cur == NULL) {
		BSD_WBD("Ifnames not specified for WBD. So skipping\n");
		goto end;
	}

	/* Foreach BSS on which WiFi Blanket is enabled */
	while (cur) {

		/* Get Pointer to BSS Info Structure */
		bssinfo = cur->bssinfo;

		/* Validate Pointer to BSS Info Structure */
		if (!cur->bssinfo) {
			BSD_WBD("BSS INFO is NULL\n");
			goto nextbss;
		}

		/* Update WBD Band Enumeration from ifname's Chanspec & Bridge Type */
		bsd_wbd_identify_band_type(bssinfo, &cur->wbd_band_type);

nextbss:
		/* Go to next BSS Info Item */
		cur = cur->next;
	}

end:
	BSD_EXIT();
}

/* Cleanup all WiFi Blanket related info */
void
bsd_cleanup_wbd(bsd_wbd_info_t *info)
{
	BSD_ENTER();

	if (info != NULL) {
		bsd_wbd_bss_list_t *cur, *next;

		/* Free the memory allocated for bss on which WiFi Blanket is enabled */
		cur = info->bss_list;
		while (cur != NULL) {
			next = cur->next;
			if (cur->weak_sta_cfg)
				free(cur->weak_sta_cfg);
			free(cur);
			cur = next;
		}

		/* Free main info */
		free(info);
		BSD_WBD("WBD Cleanup Done\n");
	}

	BSD_EXIT();
}

/* Sends the data to socket */
static int
bsd_wbd_socket_send_data(int sockfd, char *data, unsigned int len)
{
	int	nret = 0;
	int	totalsize = len, totalsent = 0;
	BSD_ENTER();

	/* Loop till all the data sent */
	while (totalsent < totalsize) {
		fd_set WriteFDs;
		struct timeval tv;

		FD_ZERO(&WriteFDs);

		if (sockfd == BSD_DFLT_FD)
			return BSD_DFLT_FD;

		FD_SET(sockfd, &WriteFDs);

		tv.tv_sec = 5;
		tv.tv_usec = 0;
		if (select(sockfd+1, NULL, &WriteFDs, NULL, &tv) > 0) {
			if (FD_ISSET(sockfd, &WriteFDs))
				;
			else {
				BSD_EXIT();
				return BSD_DFLT_FD;
			}
		}

		nret = send(sockfd, &(data[totalsent]), len, 0);
		if (nret < 0) {
			BSD_WBD("send error is : %s\n", strerror(errno));
			BSD_EXIT();
			return BSD_DFLT_FD;
		}
		totalsent += nret;
		len -= nret;
		nret = 0;
	}

	BSD_EXIT();
	return totalsent;
}

/* To recieve data */
static int
bsd_wbd_socket_recv_data(int sockfd, char *read_buf, int read_buf_len)
{
	unsigned int nbytes, totalread = 0;
	struct timeval tv;
	fd_set ReadFDs, ExceptFDs;

	FD_ZERO(&ReadFDs);
	FD_ZERO(&ExceptFDs);
	FD_SET(sockfd, &ReadFDs);
	FD_SET(sockfd, &ExceptFDs);
	tv.tv_sec = 5; /* 5 seconds */
	tv.tv_usec = 0;

	/* Read till the null character or error */
	while (1) {
		/* Allocate memory for the buffer */
		if (totalread >= read_buf_len) {
			return totalread;
		}
		if (select(sockfd+1, &ReadFDs, NULL, &ExceptFDs, &tv) > 0) {
			if (FD_ISSET(sockfd, &ReadFDs)) {
				/* fprintf(stdout, "SOCKET : Data is ready to read\n"); */;
			} else {
				return BSD_DFLT_FD;
			}
		}

		nbytes = read(sockfd, read_buf+totalread, read_buf_len);
		totalread += nbytes;

		if (nbytes <= 0) {
			BSD_WBD("read error is : %s\n", strerror(errno));
			return BSD_DFLT_FD;
		}

		/* Check the last byte for NULL termination */
		if (read_buf[totalread-1] == '\0') {
			break;
		}
	}

	return totalread;
}

/* Connects to the server given the IP address and port number */
static int
bsd_wbd_connect_to_server(char* straddrs, unsigned int nport)
{
	struct sockaddr_in server_addr;
	int res, valopt;
	long arg;
	fd_set readfds;
	struct timeval tv;
	socklen_t lon;
	int sockfd;
	BSD_ENTER();

	sockfd = BSD_DFLT_FD;
	memset(&server_addr, 0, sizeof(server_addr));

	BSD_WBD("Connecting to server = %s\t port = %d\n", straddrs, nport);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		BSD_WBD("Error in socket is : %s\n", strerror(errno));
		goto error;
	}

	/* Set nonblock on the socket so we can timeout */
	if ((arg = fcntl(sockfd, F_GETFL, NULL)) < 0 ||
		fcntl(sockfd, F_SETFL, arg | O_NONBLOCK) < 0) {
			BSD_WBD("Error in fcntl is : %s\n", strerror(errno));
			goto error;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(nport);
	server_addr.sin_addr.s_addr = inet_addr(straddrs);

	res = connect(sockfd, (struct sockaddr*)&server_addr,
		sizeof(struct sockaddr));
	if (res < 0) {
		if (errno == EINPROGRESS) {
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			FD_ZERO(&readfds);
			FD_SET(sockfd, &readfds);
			if (select(sockfd+1, NULL, &readfds, NULL, &tv) > 0) {
				lon = sizeof(int);
				getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
					(void*)(&valopt), &lon);
				if (valopt) {
					BSD_WBD("Error in connection() %d - %s\n",
						valopt, strerror(valopt));
					goto error;
				}
			} else {
				BSD_WBD("Timeout or error() %d - %s\n",
					valopt, strerror(valopt));
				goto error;
			}
		} else {
			BSD_WBD("Error connecting %d - %s\n",
				errno, strerror(errno));
			goto error;
		}
	}
	BSD_WBD("Connection Successfull with server : %s on port : %d\n", straddrs, nport);

	BSD_EXIT();
	return sockfd;

	/* Error handling */
error:
	if (sockfd != BSD_DFLT_FD)
		close(sockfd);
	BSD_EXIT();
	return BSD_DFLT_FD;
}

/* Sends the request to WBD server */
static int
bsd_wbd_send_req(char *data, char *read_buf, int read_buf_len)
{
	int sockfd = BSD_DFLT_FD, ret = BSD_OK;
	BSD_ENTER();

	/* Connect to the server */
	sockfd = bsd_wbd_connect_to_server(BSD_WBD_LOOPBACK_IP, BSD_WBD_SERVERT_PORT);
	if (sockfd == BSD_DFLT_FD) {
		ret = BSD_FAIL;
		goto end;
	}

	/* Send the data */
	if (bsd_wbd_socket_send_data(sockfd, data, strlen(data)+1) <= 0) {
		ret = BSD_FAIL;
		goto end;
	}

	/* Recieve the data */
	if (bsd_wbd_socket_recv_data(sockfd, read_buf, read_buf_len) <= 0) {
		ret = BSD_FAIL;
		goto end;
	}

end:
	if (sockfd != BSD_DFLT_FD)
		close(sockfd);

	BSD_EXIT();
	return ret;
}

/* Get the value for the token
 * Buffer is in the format "resp&mac=XX:XX:XX:XX:XX:XX&errorcode=1"
 */
static int
bsd_wbd_extract_token_val(char *data, const char *token, char *output, int len)
{
	char *p, *c, *val;
	char copydata[BSD_WBD_REQ_BUFSIZE];

	if (data == NULL)
		goto err;

	BSDSTRNCPY(copydata, data, sizeof(copydata));

	p = strstr(copydata, token);
	if (!p)
		goto err;

	if ((c = strchr(p, '&')))
		*c++ = '\0';

	val = strchr(p, '=');
	if (!val)
		goto err;

	val += 1;
	BSD_WBD("Token[%s] value[%s]\n", token, val);

	BSDSTRNCPY(output, val, len);

	return strlen(output);

err:
	return BSD_FAIL;
}

/* Get the value from the buffer for token.
 * Buffer is in the format "resp&mac=XX:XX:XX:XX:XX:XX&errorcode=1&dwell=100"
 */
static int
bsd_wbd_get_token_val(char *data, char *token, int *value)
{
	char output[10];

	memset(output, 0, sizeof(output));
	if (bsd_wbd_extract_token_val(data, token, output, sizeof(output)) <= 0)
		return BSD_FAIL;
	*value = atoi(output);

	return BSD_OK;
}

/* Creates the request and sends the weak client request to WBD */
static int
bsd_wbd_send_weak_client_req(bsd_info_t *info, bsd_bssinfo_t *bssinfo, bsd_sta_info_t *sta,
	int weak_flag, int wbd_band_type, int *wbd_ret)
{
	char data[BSD_WBD_REQ_BUFSIZE], read_buf[BSD_WBD_REQ_BUFSIZE];
	int ret = BSD_OK;
	BSD_ENTER();

	snprintf(data, sizeof(data), "{ \"Cmd\": \"WEAK_CLIENT_BSD\", \"SrcMAC\": \""MACF"\", "
		"\"DstMAC\": \""MACF"\", \"Band\": %d, \"Data\": { \"MAC\": \""MACF"\", "
		"\"WeakFlag\": %d, \"RSSI\": %d, \"TxRate\": %d, \"TxFailures\": %d, "
		"\"ReportedRSSI\": %d, \"DataRate\": %d, \"RxTotPkts\": %d, \"RxTotBytes\": %d, "
		"\"TxTotFailures\": %d } }",
		ETHER_TO_MACF(bssinfo->bssid), ETHER_TO_MACF(bssinfo->bssid),  wbd_band_type,
		ETHER_TO_MACF(sta->addr), weak_flag, sta->rssi, sta->tx_rate, sta->tx_failures,
		sta->reported_rssi, sta->datarate, sta->rx_tot_pkts, (uint32)sta->rx_tot_bytes,
		sta->tx_tot_failures);
	BSD_WBD("Request Data : %s\n", data);

	ret = bsd_wbd_send_req(data, read_buf, sizeof(read_buf));
	if (ret == BSD_OK) {
		BSD_WBD("Read Data : %s\n", read_buf);
		ret = bsd_wbd_get_token_val(read_buf, "errorcode", wbd_ret);
	}

	BSD_EXIT();
	return ret;
}

/* Creates the request and sends the weak cancel request to WBD */
static int
bsd_wbd_send_weak_cancel_req(bsd_info_t *info, bsd_bssinfo_t *bssinfo, bsd_sta_info_t *sta,
	int wbd_band_type, int *wbd_ret)
{
	char data[BSD_WBD_REQ_BUFSIZE], read_buf[BSD_WBD_REQ_BUFSIZE];
	int ret = BSD_OK;
	BSD_ENTER();

	snprintf(data, sizeof(data), "{ \"Cmd\": \"WEAK_CANCEL_BSD\", \"SrcMAC\": \""MACF"\", "
		"\"DstMAC\": \""MACF"\", \"Band\": %d, \"Data\": { \"MAC\": \""MACF"\", "
		"\"RSSI\": %d, \"TxRate\": %d, \"TxFailures\": %d, "
		"\"ReportedRSSI\": %d, \"DataRate\": %d, \"RxTotPkts\": %d, \"RxTotBytes\": %d, "
		"\"TxTotFailures\": %d } }",
		ETHER_TO_MACF(bssinfo->bssid), ETHER_TO_MACF(bssinfo->bssid), wbd_band_type,
		ETHER_TO_MACF(sta->addr), sta->rssi, sta->tx_rate, sta->tx_failures,
		sta->reported_rssi, sta->datarate, sta->rx_tot_pkts, (uint32)sta->rx_tot_bytes,
		sta->tx_tot_failures);
	BSD_WBD("Request Data : %s\n", data);

	ret = bsd_wbd_send_req(data, read_buf, sizeof(read_buf));
	if (ret == BSD_OK) {
		BSD_WBD("Read Data : %s\n", read_buf);
		ret = bsd_wbd_get_token_val(read_buf, "errorcode", wbd_ret);
	}

	BSD_EXIT();
	return ret;
}

/* Creates the request and sends the sta status request to WBD */
static int
bsd_wbd_send_sta_status_req(bsd_info_t *info, bsd_bssinfo_t *bssinfo, bsd_sta_info_t *sta,
	int wbd_band_type, int *wbd_ret)
{
	char data[BSD_WBD_REQ_BUFSIZE], read_buf[BSD_WBD_REQ_BUFSIZE];
	int ret = BSD_OK;
	BSD_ENTER();

	snprintf(data, sizeof(data), "{ \"Cmd\": \"STA_STATUS_BSD\", \"SrcMAC\": \""MACF"\", "
		"\"DstMAC\": \""MACF"\", \"Band\": %d, \"Data\": { \"MAC\": \""MACF"\"} }",
		ETHER_TO_MACF(bssinfo->bssid), ETHER_TO_MACF(bssinfo->bssid), wbd_band_type,
		ETHER_TO_MACF(sta->addr));
	BSD_WBD("Request Data : %s\n", data);

	ret = bsd_wbd_send_req(data, read_buf, sizeof(read_buf));
	if (ret == BSD_OK) {
		BSD_WBD("Read Data : %s\n", read_buf);
		ret = bsd_wbd_get_token_val(read_buf, "errorcode", wbd_ret);
		if ((*wbd_ret) == BSDE_WBD_BOUNCING_STA) {
			ret = bsd_wbd_get_token_val(read_buf, "dwell", (int*)&sta->wbd_dwell_time);
			if (ret == BSD_OK)
				sta->wbd_dwell_start = time(NULL);
		}
	}

	BSD_EXIT();
	return ret;
}

/* Creates the request and sends the block client request to WBD */
int
bsd_wbd_send_block_client_req(bsd_info_t *info, bsd_bssinfo_t *tbssinfo, bsd_sta_info_t *sta,
	int *wbd_ret)
{
	char data[BSD_WBD_REQ_BUFSIZE] = {0}, read_buf[BSD_WBD_REQ_BUFSIZE] = {0};
	int ret = BSD_OK, wbd_band_type = WBD_BAND_LAN_INVALID;
	BSD_ENTER();

	/* Identify the wbd band. */
	bsd_wbd_identify_band_type(sta->bssinfo, &wbd_band_type);

	snprintf(data, sizeof(data), "{ \"Cmd\": \"BLOCK_CLIENT_BSD\", \"SrcMAC\": \""MACF"\", "
		"\"DstMAC\": \""MACF"\", \"Band\": %d, \"Data\": { \"MAC\": \""MACF"\","
		" \"BSSID\": \""MACF"\"} }",
		ETHER_TO_MACF(sta->bssinfo->bssid), ETHER_TO_MACF(sta->bssinfo->bssid),
		wbd_band_type, ETHER_TO_MACF(sta->addr), ETHER_TO_MACF(tbssinfo->bssid));
	BSD_WBD("Request Data : %s\n", data);

	ret = bsd_wbd_send_req(data, read_buf, sizeof(read_buf));
	if (ret == BSD_OK) {
		BSD_WBD("Read Data : %s\n", read_buf);
		ret = bsd_wbd_get_token_val(read_buf, "errorcode", wbd_ret);
	}

	BSD_EXIT();

	return ret;
}

/* Special STA check for WBD */
static int
bsd_wbd_special_sta_check(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo,
	bsd_sta_info_t *sta)
{
	int ret = BSD_OK, wbd_ret = BSDE_WBD_FAIL;
	BSD_ENTER();

	/* Don't consider ignored STAs */
	if ((sta->wbd_sta_status & BSD_WBD_STA_IGNORE)) {
		BSD_WBD("ifname[%s] STA["MACF"] Ignored[0x%X]\n",
			wbd_bssinfo->bssinfo->ifnames, ETHER_TO_MACF(sta->addr),
			sta->wbd_sta_status);
		ret = BSD_FAIL;
		goto end;
	}

	/* Get STA status from WBD for the pending weak STAs */
	if ((sta->wbd_sta_status & BSD_WBD_STA_WEAK_PENDING)) {
		ret = bsd_wbd_send_sta_status_req(info, wbd_bssinfo->bssinfo, sta,
			wbd_bssinfo->wbd_band_type, &wbd_ret);

		/* If we get successfull response. remove weak pending */
		if (ret == BSD_OK) {
			sta->wbd_sta_status &= ~(BSD_WBD_STA_WEAK_PENDING);
		}

		if (wbd_ret == BSDE_WBD_OK) {
			sta->wbd_sta_status |= BSD_WBD_STA_WEAK;
		} else if (wbd_ret == BSDE_WBD_IGNORE_STA) {
			sta->wbd_sta_status |= BSD_WBD_STA_IGNORE;
		} else if (wbd_ret == BSDE_WBD_BOUNCING_STA) {
			sta->wbd_sta_status |= BSD_WBD_STA_DWELL;
			sta->wbd_sta_status &= ~(BSD_WBD_STA_WEAK);
		} else {
			sta->wbd_sta_status &= ~(BSD_WBD_STA_WEAK);
		}
		BSD_WBD("STA["MACF"] in ifname[%s] status[0x%x] dwell[%lu] dwell_start[%lu]\n",
			ETHER_TO_MACF(sta->addr), wbd_bssinfo->bssinfo->ifnames,
			sta->wbd_sta_status, (unsigned long)sta->wbd_dwell_time,
			(unsigned long)sta->wbd_dwell_start);
	}

	/* Skip the WBD's bouncing STA */
	if ((sta->wbd_sta_status & BSD_WBD_STA_DWELL)) {
		time_t now = time(NULL);
		time_t gap;

		gap = now - sta->wbd_dwell_start;
		/* Check if the STA still in dwell state */
		if (sta->wbd_dwell_time > gap) {
			BSD_WBD("Still in dwell state. STA["MACF"] in ifname[%s] "
				"status[0x%x] dwell[%lu] dwell_start[%lu] gap[%lu]\n",
				ETHER_TO_MACF(sta->addr), wbd_bssinfo->bssinfo->ifnames,
				sta->wbd_sta_status, (unsigned long)sta->wbd_dwell_time,
				(unsigned long)sta->wbd_dwell_start, (unsigned long)gap);
			ret = BSD_FAIL;
			goto end;
		}
		/* Dwell state is expired. so, remove the dwell state */
		sta->wbd_sta_status &= ~(BSD_WBD_STA_DWELL);
		sta->wbd_dwell_start = 0;
		sta->wbd_dwell_time = 0;
	}

end:
	BSD_EXIT();
	return ret;
}

/* Check whether the STA is weak or not */
static int
bsd_wbd_find_weak_sta_policy(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo,
	bsd_sta_info_t *sta, int *out_weak_flag)
{
	bsd_bssinfo_t *bssinfo;
	int isweak = FALSE, fail_cnt = 0;
	uint32 adj_mcs_phyrate = 0;
	wbd_weak_sta_metrics_t sta_stats;
	BSD_ENTER();

	bssinfo = wbd_bssinfo->bssinfo;
	BSD_WBD("STA="MACF" in ifname=%s ssid=%s BSSID="MACF" cfg_flags[0x%x]\n",
		ETHER_TO_MACF(sta->addr), bssinfo->ifnames,
		bssinfo->ssid, ETHER_TO_MACF(bssinfo->bssid), wbd_bssinfo->weak_sta_cfg->flags);
	BSD_WBD("RSSI[%d] ReportedRSSI[%d] steer_flag[%d] band[%d] tx_rate[%d] flags[0x%X] "
		"phyrate[%d] datarate[%d] mcs_phyrate[%d] tx_bps[%d] rx_bps[%d] "
		"tx_failures[%d]\n",
		sta->rssi, sta->reported_rssi, sta->steerflag, sta->band, sta->tx_rate, sta->flags,
		sta->phyrate, sta->datarate, sta->mcs_phyrate, sta->tx_bps, sta->rx_bps,
		sta->tx_failures);

	/* skipped non-steerable STA */
	if (sta->steerflag & BSD_BSSCFG_NOTSTEER) {
		BSD_WBD("sta[%p]:"MACF" is not steerable. Skipped.\n",
			sta, ETHERP_TO_MACF(&sta->addr));
		goto end;
	}

	/* Skipped macmode mismatch STA */
	if (bsd_aclist_steerable(bssinfo, &sta->addr) == BSD_FAIL) {
		BSD_WBD("sta[%p]:"MACF" not steerable match w/ static maclist. Skipped.\n",
			sta, ETHERP_TO_MACF(&sta->addr));
		goto end;
	}

	/* Using tx_rate directly because, if the tx_rate drops drastically,
	 * the mcs_phyrate is not getting updated.
	 * Adjust MCS phyrate by filtering bogus tx_rate
	 */
	adj_mcs_phyrate = ((sta->tx_rate < BSD_MAX_DATA_RATE) &&
		(sta->tx_rate > BSD_WBD_MIN_PHYRATE)) ? sta->tx_rate : 0;
	BSD_WBD("sta->tx_rate[%d] adj_mcs_phyrate[%d] threshold[%d]\n",
		sta->tx_rate, adj_mcs_phyrate, wbd_bssinfo->weak_sta_cfg->t_tx_rate);

	/* Fill STA Stats, which needs to be compared for Weak Client Indentification */
	memset(&sta_stats, 0, sizeof(sta_stats));
	sta_stats.idle_rate = sta->datarate;
	sta_stats.rssi = sta->rssi;
	sta_stats.last_weak_rssi = sta->reported_rssi;
	sta_stats.tx_rate = adj_mcs_phyrate;
	sta_stats.tx_failures = sta->tx_failures;

	/* Common algo to compare STA Stats and Thresholds, & identify if STA is Weak or not */
	isweak = wbd_weak_sta_identification(&sta->addr, &sta_stats,
		wbd_bssinfo->weak_sta_cfg, &fail_cnt, out_weak_flag);

	if (isweak) {
		BSD_WBD("Found Weak STA="MACF" fail_cnt=%d sta_rule_met=0x%X in ifname=%s ssid=%s "
			"BSSID="MACF"\n", ETHER_TO_MACF(sta->addr), fail_cnt,
			out_weak_flag ? *out_weak_flag : 0, bssinfo->ifnames,
			bssinfo->ssid, ETHER_TO_MACF(bssinfo->bssid));
	}
end:
	BSD_EXIT();
	return isweak;
}

/* Checks the STA is weak or not using algorithms defined */
static int
bsd_wbd_is_weak_sta(bsd_info_t *info, bsd_wbd_bss_list_t *wbd_bssinfo, bsd_sta_info_t *sta,
	int *weak_flag)
{
	return (wbd_predefined_algo[wbd_bssinfo->algo])(info, wbd_bssinfo, sta, weak_flag);
}

/* Checks for the weak STAs in all the BSS on which WBD is enabled */
void
bsd_wbd_check_weak_sta(bsd_info_t *info)
{
	int ret, wbd_band_type;
	bsd_wbd_info_t *wbd_info;
	bsd_wbd_bss_list_t *cur;
	bsd_bssinfo_t *bssinfo;
	bsd_sta_info_t *sta = NULL;
	BSD_ENTER();

	BCM_REFERENCE(ret);

	if ((info->enable_flag & BSD_FLAG_WBD_ENABLED) == 0) {
		BSD_WBD("WBD enable_flag[%d]. So skipping\n", info->enable_flag);
		goto end;
	}

	wbd_info = info->wbd_info;
	cur = wbd_info->bss_list;

	if (cur == NULL) {
		BSD_WBD("Ifnames not specified for WBD. So skipping\n");
		goto end;
	}

	/* For each BSS */
	while (cur) {
		bssinfo = cur->bssinfo;
		wbd_band_type = cur->wbd_band_type;
		if (!bssinfo) {
			BSD_WBD("BSS INFO is NULL\n");
			goto nextbss;
		}
		/* check for dfs_ap_stop event if set for bss, if set skip bss for sta's stats
		 */
		if (bssinfo->rd_stats_again) {
			BSD_WBD(" DFS_AP_RESUME event set for BSS, compute sta stats again \n");
			/* Reset rd_stats_again */
			bssinfo->rd_stats_again = FALSE;
			goto nextbss;
		}

		BSD_WBD("Checking STAs in ifname=%s ssid=%s BSSID="MACF"\n", bssinfo->ifnames,
			bssinfo->ssid, ETHER_TO_MACF(bssinfo->bssid));

		/* assoclist */
		sta = bssinfo->assoclist;
		while (sta) {
			int wbd_ret = BSDE_WBD_FAIL;
			int weak_flag = 0;

			/* Skip DWDS STA */
			if ((sta->wbd_sta_status & BSD_WBD_STA_DWDS))
				goto nextsta;

			/* Do a wbd specific STA check */
			if (bsd_wbd_special_sta_check(info, cur, sta) != BSD_OK)
				goto nextsta;

			if (bsd_wbd_is_weak_sta(info, cur, sta, &weak_flag) == TRUE) {
				/* If its already weak, don't inform to WBD */
				if (!(sta->wbd_sta_status & BSD_WBD_STA_WEAK)) {
					ret = bsd_wbd_send_weak_client_req(info, bssinfo, sta,
						weak_flag, wbd_band_type, &wbd_ret);
					if (wbd_ret == BSDE_WBD_OK) {
						sta->reported_rssi = sta->rssi;
						sta->wbd_sta_status |= BSD_WBD_STA_WEAK;
						sta->wbd_sta_status |= BSD_WBD_STA_WEAK_PENDING;
						BSD_WBD("ifname[%s] BSSID["MACF"] STA["MACF"] "
							"sta->reported_rssi[%d]\n\n",
							bssinfo->ifnames,
							ETHER_TO_MACF(bssinfo->bssid),
							ETHER_TO_MACF(sta->addr),
							sta->reported_rssi);
					} else if (wbd_ret == BSDE_WBD_IGNORE_STA) {
						sta->wbd_sta_status |= BSD_WBD_STA_IGNORE;
						BSD_WBD("ifname[%s] ssid[%s] BSSID["MACF"] "
							"STA["MACF"]. Ignore STA\n",
							bssinfo->ifnames, bssinfo->ssid,
							ETHER_TO_MACF(bssinfo->bssid),
							ETHER_TO_MACF(sta->addr));
					}
				}
			} else {
				/* Not weak. So check if its already weak */
				if ((sta->wbd_sta_status & BSD_WBD_STA_WEAK)) {
					/* Inform WBD */
					ret = bsd_wbd_send_weak_cancel_req(info, bssinfo, sta,
						wbd_band_type, &wbd_ret);
					if (wbd_ret == BSDE_WBD_OK) {
						sta->reported_rssi = sta->rssi;
						BSD_WBD("sta->reported_rssi[%d]\n\n",
							sta->reported_rssi);
						sta->wbd_sta_status &= ~(BSD_WBD_STA_WEAK);
						sta->wbd_sta_status &= ~(BSD_WBD_STA_WEAK_PENDING);
					}
				}
			}
nextsta:
			sta = sta->next;
		}
nextbss:
		cur = cur->next;
	}

end:
	BSD_EXIT();
}

/* update bss info if exist in wbd_ifnames */
void bsd_wbd_update_bss_info(bsd_info_t *info, char *ifname)
{
	bsd_wbd_info_t *wbd_info;
	bsd_wbd_bss_list_t *cur;
	bsd_bssinfo_t *bssinfo;

	BSD_ENTER();

	wbd_info = info->wbd_info;
	cur = wbd_info->bss_list;

	if (cur == NULL) {
		BSD_WBD("Ifnames not specified for WBD. So skipping\n");
		goto end;
	}

	while (cur) {
		bssinfo = cur->bssinfo;
		if (!bssinfo) {
			BSD_WBD("Not found ifname:%s\n", ifname);
			return;
		}

		if (!strcmp(ifname, bssinfo->ifnames)) {
			/* Update bss info stats with the event */
			BSD_WBD("BSS INFO update with event \n");
			/* At time of DFS_AP_RESUME, It was observed that sta's stats
			 * tx_failure in particular shoots over Max allowed range
			 * and still there is a possibility of raising of WEAK CLIENT
			 * message.
			 * Use another bss bool to take care of this situation and
			 * instruct check_weak_sta to do one more try
			 */
			bssinfo->rd_stats_again = TRUE;
			break;
		} else {
			cur = cur->next;
		}
	}
end:
	BSD_EXIT();
}
