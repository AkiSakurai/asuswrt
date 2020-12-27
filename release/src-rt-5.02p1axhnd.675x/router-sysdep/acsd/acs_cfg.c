/*
 *      acs_cfg.c
 *
 *      This module will retrieve acs configurations from nvram, if params are
 *      not set it will retrieve default values.
 *
 *      Copyright 2020 Broadcom
 *
 *      This program is the proprietary software of Broadcom and/or
 *      its licensors, and may only be used, duplicated, modified or distributed
 *      pursuant to the terms and conditions of a separate, written license
 *      agreement executed between you and Broadcom (an "Authorized License").
 *      Except as set forth in an Authorized License, Broadcom grants no license
 *      (express or implied), right to use, or waiver of any kind with respect to
 *      the Software, and Broadcom expressly reserves all rights in and to the
 *      Software and all intellectual property rights therein.  IF YOU HAVE NO
 *      AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *      WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *      THE SOFTWARE.
 *
 *      Except as expressly set forth in the Authorized License,
 *
 *      1. This program, including its structure, sequence and organization,
 *      constitutes the valuable trade secrets of Broadcom, and you shall use
 *      all reasonable efforts to protect the confidentiality thereof, and to
 *      use this information only in connection with your use of Broadcom
 *      integrated circuit products.
 *
 *      2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *      "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *      REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *      OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *      DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *      NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *      ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *      CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *      OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *      3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *      BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *      SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *      IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *      IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *      ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *      OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *      NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *	$Id: acs_cfg.c 761368 2018-05-08 02:06:19Z $
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>

#include "acsd_svr.h"
static const char *station_key_fmtstr = "toa-sta-%d";	/* format string, passed to snprintf() */

#define MAX_KEY_LEN 16				/* the expanded key format string must fit within */
#define ACS_VIDEO_STA_TYPE "video"		/* video sta type */

#define ACS_DFLT_FLAGS ACS_FLAGS_LASTUSED_CHK

extern int bcm_ether_atoe(const char *p, struct ether_addr *ea);

/*
 * Function to set a channel table by parsing a list consisting
 * of a comma-separated channel numbers.
 */
int
acs_set_chan_table(char *channel_list, chanspec_t *chspec_list,
                      unsigned int vector_size)
{
	int chan_index;
	int channel;
	int chan_count = 0;
	char *chan_str;
	char *delim = ",";
	char chan_buf[ACS_MAX_VECTOR_LEN + 2];
	int list_length;

	/*
	* NULL list means no channels are set. Return without
	* modifying the vector.
	*/
	if (channel_list == NULL)
		return 0;

	/*
	* A non-null list means that we must set the vector.
	*  Clear it first.
	* Then parse a list of <chan>,<chan>,...<chan>
	*/
	memset(chan_buf, 0, sizeof(chan_buf));
	list_length = strlen(channel_list);
	list_length = MIN(list_length, ACS_MAX_VECTOR_LEN);
	strncpy(chan_buf, channel_list, list_length);
	strncat(chan_buf, ",", list_length);

	chan_str = strtok(chan_buf, delim);

	for (chan_index = 0; chan_index < vector_size; chan_index++)
	{
		if (chan_str == NULL)
			break;
		channel = strtoul(chan_str, NULL, 16);
		if (channel == 0)
			break;
		chspec_list[chan_count++] = channel;
		chan_str = strtok(NULL, delim);
	}
	return chan_count;
}

static int
acs_toa_load_station(acs_fcs_t *fcs_info, const char *keyfmt, int stain)
{
	char keybuf[MAX_KEY_LEN];
	char *tokens, *sta_type;
	int index = fcs_info->video_sta_idx;
	char ea[ACS_STA_EA_LEN];

	if (acs_snprintf(keybuf, sizeof(keybuf), keyfmt, stain) >= sizeof(keybuf)) {
		ACSD_ERROR("key buffer too small\n");
		return BCME_ERROR;
	}

	tokens = nvram_get(keybuf);
	if (!tokens) {
		ACSD_INFO("No toa NVRAM params set\n");
		return BCME_ERROR;
	}

	strncpy(ea, tokens, ACS_STA_EA_LEN);
	ea[ACS_STA_EA_LEN -1] = '\0';
	sta_type = strstr(tokens, ACS_VIDEO_STA_TYPE);
	if (sta_type) {
		fcs_info->acs_toa_enable = TRUE;
		if (index >= ACS_MAX_VIDEO_STAS) {
			ACSD_ERROR("MAX VIDEO STAs exceeded\n");
			return BCME_ERROR;
		}

		strncpy(fcs_info->vid_sta[index].vid_sta_mac, ea,
				ACS_STA_EA_LEN);
		fcs_info->vid_sta[index].vid_sta_mac[ACS_STA_EA_LEN -1] = '\0';
		if (!bcm_ether_atoe(fcs_info->vid_sta[index].vid_sta_mac,
					&fcs_info->vid_sta[index].ea)) {
			ACSD_ERROR("toa video sta ether addr NOT proper\n");
			return BCME_ERROR;
		}
		fcs_info->video_sta_idx++;
		ACSD_INFO("VIDEOSTA %s\n", fcs_info->vid_sta[index].vid_sta_mac);
	}

	return BCME_OK;
}

static void
acs_bgdfs_acs_toa_retrieve_config(acs_chaninfo_t *c_info, char * prefix)
{
	/* retrieve toa related configuration from nvram */
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	int stain, ret = BCME_OK;

	fcs_info->acs_toa_enable = FALSE;
	fcs_info->video_sta_idx = 0;
	ACSD_INFO("retrieve TOA config from nvram ...\n");

	/* Load station specific settings */
	for (stain = 1; stain <= ACS_MAX_VIDEO_STAS; ++stain) {
		ret = acs_toa_load_station(fcs_info, station_key_fmtstr, stain);
		if (ret != BCME_OK)
			return;
	}
}

static void
acs_retrieve_config_bgdfs(acs_bgdfs_info_t *acs_bgdfs, char * prefix)
{
	char conf_word[128], tmp[100];

	if (acs_bgdfs == NULL) {
		ACSD_ERROR("acs_bgdfs is NULL");
		return;
	}

	/* acs_bgdfs_ahead */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_ahead", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_ahead is set. Retrieve default.\n");
		acs_bgdfs->ahead = ACS_BGDFS_AHEAD;
	} else {
		char *endptr = NULL;
		acs_bgdfs->ahead = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_ahead: 0x%x\n", acs_bgdfs->ahead);
	}

	/* acs_bgdfs_idle_interval */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_idle_interval", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_idle_interval is set. Retrieve default.\n");
		acs_bgdfs->idle_interval = ACS_BGDFS_IDLE_INTERVAL;
	} else {
		char *endptr = NULL;
		acs_bgdfs->idle_interval = strtoul(conf_word, &endptr, 0);
		if (acs_bgdfs->idle_interval < ACS_TRAFFIC_INFO_UPDATE_INTERVAL(acs_bgdfs)) {
			acs_bgdfs->idle_interval = ACS_TRAFFIC_INFO_UPDATE_INTERVAL(acs_bgdfs);
		}
		ACSD_INFO("acs_bgdfs_idle_interval: 0x%x\n", acs_bgdfs->idle_interval);
	}

	/* acs_bgdfs_idle_frames_thld */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_idle_frames_thld", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_idle_frames_thld is set. Retrieve default.\n");
		acs_bgdfs->idle_frames_thld = ACS_BGDFS_IDLE_FRAMES_THLD;
	} else {
		char *endptr = NULL;
		acs_bgdfs->idle_frames_thld = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_idle_frames_thld: 0x%x\n", acs_bgdfs->idle_frames_thld);
	}

	/* acs_bgdfs_avoid_on_far_sta */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_avoid_on_far_sta", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_avoid_on_far_sta is set. Retrieve default.\n");
		acs_bgdfs->bgdfs_avoid_on_far_sta = ACS_BGDFS_AVOID_ON_FAR_STA;
	} else {
		char *endptr = NULL;
		acs_bgdfs->bgdfs_avoid_on_far_sta = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_avoid_on_far_sta: 0x%x\n", acs_bgdfs->bgdfs_avoid_on_far_sta);
	}

	/* acs_bgdfs_fallback_blocking_cac */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_fallback_blocking_cac", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_fallback_blocking_cac set. Get default.\n");
		acs_bgdfs->fallback_blocking_cac = ACS_BGDFS_FALLBACK_BLOCKING_CAC;
	} else {
		char *endptr = NULL;
		acs_bgdfs->fallback_blocking_cac = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_fallback_blocking_cac: 0x%x\n",
				acs_bgdfs->fallback_blocking_cac);
	}

	/* acs_bgdfs_txblank_threshold */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_txblank_threshold", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_txblank_threshold set. Get default.\n");
		acs_bgdfs->txblank_th = ACS_BGDFS_TX_LOADING;
	} else {
		char *endptr = NULL;
		acs_bgdfs->txblank_th = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_txblank_threshold: 0x%x\n", acs_bgdfs->txblank_th);
	}
}

void
acs_retrieve_config(acs_chaninfo_t *c_info, char *prefix)
{
	/* retrieve policy related configuration from nvram */
	char conf_word[128], conf_var[16], tmp[100];
	char *next;
	int i = 0, val;
	acs_policy_index index;
	acs_policy_t *a_pol = &c_info->acs_policy;
	uint32 flags;
	int acs_bgdfs_enab = 0;
	bool bgdfs_cap = FALSE;

	/* the current layout of config */
	ACSD_INFO("retrieve config from nvram ...\n");

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_scan_entry_expire", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_scan_entry_expire set. Retrieve default.\n");
		c_info->acs_scan_entry_expire = ACS_CI_SCAN_EXPIRE;
	}
	else {
		char *endptr = NULL;
		c_info->acs_scan_entry_expire = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_scan_entry_expire: 0x%x\n", c_info->acs_scan_entry_expire);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_fcs_mode", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_fcs_mode is set. Retrieve default.\n");
		c_info->acs_fcs_mode = ACS_FCS_MODE_DEFAULT;
	}
	else {
		char *endptr = NULL;
		c_info->acs_fcs_mode = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_fcs_mode: 0x%x\n", c_info->acs_fcs_mode);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acsd_scs_dfs_scan", tmp));

	if ((!strcmp(conf_word, "")) || c_info->acs_fcs_mode) {
		ACSD_INFO("No acsd_scs_dfs_scan retrieve default.\n");
		c_info->acsd_scs_dfs_scan = ACSD_SCS_DFS_SCAN_DEFAULT;
	}
	else {
		char *endptr = NULL;
		c_info->acsd_scs_dfs_scan = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acsd_scs_dfs_scan: 0x%x\n", c_info->acsd_scs_dfs_scan);
	}

	/* acs_bgdfs_enab */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_bgdfs_enab", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_bgdfs_enab is set. Retrieve default.\n");
		acs_bgdfs_enab = ACS_BGDFS_ENAB;
	}
	else {
		char *endptr = NULL;
		acs_bgdfs_enab = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_bgdfs_enab: 0x%x\n", acs_bgdfs_enab);
	}

	bgdfs_cap = acs_bgdfs_capable(c_info);

	if (acs_bgdfs_enab && bgdfs_cap) {
		/* allocate core data structure for bgdfs */
		c_info->acs_bgdfs =
			(acs_bgdfs_info_t *)acsd_malloc(sizeof(*(c_info->acs_bgdfs)));

		acs_retrieve_config_bgdfs(c_info->acs_bgdfs, prefix);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_boot_only", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_boot_only is set. Retrieve default. \n");
		c_info->acs_boot_only = ACS_BOOT_ONLY_DEFAULT;
	} else {
		char *endptr = NULL;
		c_info->acs_boot_only = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_boot_only: 0x%x\n", c_info->acs_boot_only);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_flags", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs flag set. Retrieve default.\n");
		flags = ACS_DFLT_FLAGS;
	}
	else {
		char *endptr = NULL;
		flags = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs flags: 0x%x\n", flags);
	}

	acs_safe_get_conf(conf_word, sizeof(conf_word),
		strcat_r(prefix, "acs_pol", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs policy set. Retrieve default.\n");

		acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_pol_idx", tmp));

		if (!strcmp(conf_word, "")) {
			if (ACS_FCS_MODE(c_info)) {
				index = ACS_POLICY_FCS;
			} else {
				index = ACS_POLICY_DEFAULT;
			}
		} else {
			index = atoi(conf_word);
		}
		acs_default_policy(a_pol, index);

	} else {

		index = ACS_POLICY_USER;
		memset(a_pol, 0, sizeof(*a_pol));	/* Initialise policy values to all zeroes */
		foreach(conf_var, conf_word, next) {
			val = atoi(conf_var);
			ACSD_DEBUG("i: %d conf_var: %s val: %d\n", i, conf_var, val);

			if (i == 0)
				a_pol->bgnoise_thres = val;
			else if (i == 1)
				a_pol->intf_threshold = val;
			else {
				if ((i - 2) >= CH_SCORE_MAX) {
					ACSD_ERROR("Ignoring excess values in %sacs_pol=\"%s\"\n",
						prefix, conf_word);
					break; /* Prevent overwriting innocent memory */
				}
				a_pol->acs_weight[i - 2] = val;
				ACSD_DEBUG("weight No. %d, value: %d\n", i-2, val);
			}
			i++;
		}
		a_pol->chan_selector = acs_pick_chanspec;
	}

	acs_fcs_retrieve_config(c_info, prefix);

	acs_bgdfs_acs_toa_retrieve_config(c_info, prefix);

	/* Customer req
	 * bootup on nondfs in scs mode
	 */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_start_on_nondfs", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_start_on_nondfs set,Retrieve default.ifname:%s\n", c_info->name);
		c_info->acs_start_on_nondfs = ACS_START_ON_NONDFS;
	} else {
		char *endptr = NULL;
		c_info->acs_start_on_nondfs = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_start_on_nondfs: 0x%x ifname: %s\n", c_info->acs_start_on_nondfs,
			c_info->name);
	}

	/* Customer Knob #1
	 * Preference for DFS and Non-DFS channels
	 */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_cs_dfs_pref", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_cs_dfs_pref set. Use val of acs_dfs instead\n");
		c_info->acs_cs_dfs_pref = c_info->acs_fcs.acs_dfs;
	}
	else {
		char *endptr = NULL;
		c_info->acs_cs_dfs_pref = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_cs_dfs_pref: 0x%x\n", c_info->acs_cs_dfs_pref);
	}

	/* Customer Knob #2
	 * Preference for channel power
	 */
	acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_cs_high_pwr_pref", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No acs_cs_high_pwr_pref set. Disabled by default.\n");
		c_info->acs_cs_high_pwr_pref = 0;
	}
	else {
		char *endptr = NULL;
		c_info->acs_cs_high_pwr_pref = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs_cs_high_pwr_pref: 0x%x\n", c_info->acs_cs_high_pwr_pref);
	}

	/* allocate core data structure for escan */
	c_info->acs_escan =
		(acs_escaninfo_t *)acsd_malloc(sizeof(*(c_info->acs_escan)));

	acs_safe_get_conf(conf_word, sizeof(conf_word),
			strcat_r(prefix, "acs_use_escan", tmp));

	if (!strcmp(conf_word, "")) {
		ACSD_INFO("No escan config set. use defaults\n");
		c_info->acs_escan->acs_use_escan = ACS_ESCAN_DEFAULT;
	}
	else {
		char *endptr = NULL;
		c_info->acs_escan->acs_use_escan = strtoul(conf_word, &endptr, 0);
		ACSD_DEBUG("acs escan enable: %d\n", c_info->acs_escan->acs_use_escan);
	}

	c_info->flags = flags;
	c_info->policy_index = index;
#ifdef DEBUG
	acs_dump_policy(a_pol);
#endif // endif
}
