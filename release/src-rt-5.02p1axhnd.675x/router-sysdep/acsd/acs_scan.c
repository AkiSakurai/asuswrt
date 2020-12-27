/*
 *	acs_scan.c
 *
 *      This module will initiate the escan and try to get the escan results
 *      from driver/firmware for all the channels.
 *
 *	Copyright 2020 Broadcom
 *
 *	This program is the proprietary software of Broadcom and/or
 *	its licensors, and may only be used, duplicated, modified or distributed
 *	pursuant to the terms and conditions of a separate, written license
 *	agreement executed between you and Broadcom (an "Authorized License").
 *	Except as set forth in an Authorized License, Broadcom grants no license
 *	(express or implied), right to use, or waiver of any kind with respect to
 *	the Software, and Broadcom expressly reserves all rights in and to the
 *	Software and all intellectual property rights therein.  IF YOU HAVE NO
 *	AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *	WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *	THE SOFTWARE.
 *
 *	Except as expressly set forth in the Authorized License,
 *
 *	1. This program, including its structure, sequence and organization,
 *	constitutes the valuable trade secrets of Broadcom, and you shall use
 *	all reasonable efforts to protect the confidentiality thereof, and to
 *	use this information only in connection with your use of Broadcom
 *	integrated circuit products.
 *
 *	2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *	"AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *	REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *	OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *	DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *	NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *	ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *	CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *	OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *	3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *	BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *	SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *	IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *	IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *	ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *	OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *	NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *	$Id: acs_scan.c 745861 2018-02-09 11:28:41Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>

#include "acsd_svr.h"

/*
 * This module retrieves the following information from the wl driver before
 * deciding on the best channel:
 * 1) scan result (wl_scan_result_t)
 * 2) channel interference stats (wl_chanim_stats_t)
 * 3) scan channel spec list
 * 4) channel spec candidate (all valid channel spec for the current band, bw, locale)
 * 5) band type, coex_enable, bw_cap.
 *
 * The facts which could be weighted in the channel scoring systems are:
 * 1) Number of BSS's detected during the scan process (from scan result)
 * 2) Channel Occupancy (percentage of time the channel is occupied by other BSS's)
 * 3) Channel Interference (from CCA stats)
 * 4) Channel FCS (from CCA stats)
 * 5) Channel MAX tx power
 * 6) Adjacent Channel Interference
 * The scoring algorithm for each factor is subject to update based on testing results.
 * The weight for each factor can be customized based on different channel eval policies.
 */

void
acs_ci_scan_update_idx(acs_scan_chspec_t *chspec_q, uint8 increment)
{
	uint8 idx = chspec_q->idx + increment;
	uint32 chan_flags;

	if (idx >= chspec_q->count)
		idx = 0;

	do {
		chan_flags = chspec_q->chspec_list[idx].flags;

		/* check if it is preffered channel and pref scanning requested */
		if ((chspec_q->ci_scan_running == ACS_CI_SCAN_RUNNING_PREF)) {
			if ((chan_flags & ACS_CI_SCAN_CHAN_PREF))
				break;
		} else if (!(chan_flags & ACS_CI_SCAN_CHAN_EXCL))
			break;

		/* increment index */
		if (++idx == chspec_q->count)
			idx = 0;

	} while (idx != chspec_q->idx);

	chspec_q->idx = idx;
}

/* maybe we do not care about 11b anymore */
static bool
acs_bss_is_11b(wl_bss_info_t* bi)
{
	uint i;
	bool b = TRUE;

	for (i = 0; i < bi->rateset.count; i++) {
		b = bi->rateset.rates[i] & 0x80;
		if (!b)
			break;
	}
	return b;
}

static int
acs_build_scanlist(acs_chaninfo_t *c_info)
{
	wl_uint32_list_t *list;
	chanspec_t input = 0, c = 0;
	int ret = 0, i, j;
	int count = 0;
	scan_chspec_elemt_t *ch_list;
	acs_rsi_t *rsi = &c_info->rs_info;
	fcs_conf_chspec_t *pref_chans = &(c_info->acs_fcs.pref_chans);
	fcs_conf_chspec_t *excl_chans = &(c_info->acs_fcs.excl_chans);

	char *data_buf, *data_buf1 = NULL;
	data_buf = acsd_malloc(ACS_SM_BUF_LEN);

	input |= WL_CHANSPEC_BW_20;

	if (BAND_5G(rsi->band_type))
		input |= WL_CHANSPEC_BAND_5G;
	else
		input |= WL_CHANSPEC_BAND_2G;

	ret = acs_get_perband_chanspecs(c_info, input, data_buf, ACS_SM_BUF_LEN);

	if (ret < 0)
		ACS_FREE(data_buf);
	ACS_ERR(ret, "failed to get valid chanspec lists");

	list = (wl_uint32_list_t *)data_buf;
	count = dtoh32(list->count);

	c_info->scan_chspec_list.count = count;
	c_info->scan_chspec_list.idx = 0;
	c_info->scan_chspec_list.pref_count = 0;
	c_info->scan_chspec_list.excl_count = 0;

	if (!count) {
		ACSD_ERROR("number of valid chanspec is 0\n");
		ret = -1;
		goto cleanup_sl;
	}

	ACS_FREE(c_info->scan_chspec_list.chspec_list);

	ch_list = c_info->scan_chspec_list.chspec_list =
		(scan_chspec_elemt_t *)acsd_malloc(count * sizeof(scan_chspec_elemt_t));

	data_buf1 = acsd_malloc(ACS_SM_BUF_LEN);

	for (i = 0; i < count; i++) {
		c = (chanspec_t)dtoh32(list->element[i]);

		ch_list[i].chspec = c;

		if (BAND_5G(rsi->band_type)) {
			input = c;
			ret = acs_get_per_chan_info(c_info, input, data_buf1, ACS_SM_BUF_LEN);
			if (ret < 0) {
				ACS_FREE(data_buf);
				ACS_FREE(data_buf1);
			}
			ACS_ERR(ret, "failed to get per_chan_info");

			ch_list[i].chspec_info = dtoh32(*(uint32 *)data_buf1);

			/* Exclude DFS channels if 802.11h spectrum management is off */
			if (!rsi->reg_11h && (ch_list[i].chspec_info & WL_CHAN_RADAR)) {
				ch_list[i].flags |= ACS_CI_SCAN_CHAN_EXCL;
				c_info->scan_chspec_list.excl_count++;
			}
		}

		/* Update preffered channel attribute */
		if (pref_chans && pref_chans->count) {
			for (j = 0; j < pref_chans->count; j++) {
				if (c == pref_chans->clist[j]) {
					ch_list[i].flags |= ACS_CI_SCAN_CHAN_PREF;
					c_info->scan_chspec_list.pref_count++;
					break;
				}
			}
		}

		/* Update exclude channel attribute */
		if (AUTOCHANNEL(c_info) && excl_chans && excl_chans->count) {
			for (j = 0; j < excl_chans->count; j++) {
				if (c == excl_chans->clist[j]) {
					ch_list[i].flags |= ACS_CI_SCAN_CHAN_EXCL;
					c_info->scan_chspec_list.excl_count++;
					break;
				}
			}
		}
		ACSD_INFO("chanspec: (0x%04x), chspec_info: 0x%x  pref_chan: 0x%x\n", c,
			ch_list[i].chspec_info, ch_list[i].flags);
	}
	acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);

cleanup_sl:
	ACS_FREE(data_buf);
	ACS_FREE(data_buf1);

	return ret;
}

static int
acs_scan_prep(acs_chaninfo_t *c_info, wl_scan_params_t *params, int *params_size)
{
	int ret = 0;
	int i, scount = 0;
	acs_scan_chspec_t* scan_chspec_p = &c_info->scan_chspec_list;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = WL_SCANFLAGS_PASSIVE;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = ACS_CS_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 0;

	ret = acs_build_scanlist(c_info);
	ACS_ERR(ret, "failed to build scan chanspec list");

	for (i = 0; i < scan_chspec_p->count; i++) {
		if (scan_chspec_p->chspec_list[i].flags & ACS_CI_SCAN_CHAN_EXCL)
			continue;
		params->channel_list[scount++] = htodchanspec(scan_chspec_p->chspec_list[i].chspec);
	}
	params->channel_num = htod32(scount & WL_SCAN_PARAMS_COUNT_MASK);
	ACSD_INFO("scan channel number: %d\n", params->channel_num);

	*params_size = WL_SCAN_PARAMS_FIXED_SIZE + scount * sizeof(uint16);
	ACSD_INFO("params size: %d\n", *params_size);

	return ret;
}

/* channel information (quick) scan at run time */
int
acs_run_ci_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	if (c_info->acs_escan->acs_use_escan) {
		ret = acs_run_escan(c_info, ACS_SCAN_TYPE_CI);

		if (ret == BCME_UNSUPPORTED) {
			/* Escan unsupported. Use normal scan */
			c_info->acs_escan->acs_use_escan = 0;
		}
	}

	if (!c_info->acs_escan->acs_use_escan) {
		ret = acs_run_normal_ci_scan(c_info);
	}

	return ret;
}

void
acs_expire_scan_entry(acs_chaninfo_t *c_info, time_t limit)
{
	time_t now;
	acs_bss_info_entry_t *curptr, *previous = NULL, *past;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	now = time(NULL);

	while (curptr) {
		time_t diff = now - curptr->timestamp;
		if (diff > limit) {
			ACSD_FCS("Scan expire: diff %dsec chanspec 0x%x, SSID %s\n",
				(int)diff, curptr->binfo_local.chanspec, curptr->binfo_local.SSID);
			if (previous == NULL)
				*rootp = curptr->next;
			else
				previous->next = curptr->next;

			past = curptr;
			curptr = curptr->next;
			ACS_FREE(past);
			continue;
		}
		previous = curptr;
		curptr = curptr->next;
	}
}

int
acs_do_ci_update(uint ticks, acs_chaninfo_t * c_info)
{
	int ret = 0;

	if (ticks % c_info->acs_ci_scan_timer)
		return ret;

	acs_expire_scan_entry(c_info, (time_t)c_info->acs_scan_entry_expire);

	if (ACS_FCS_MODE(c_info) && (!(c_info->scan_chspec_list.ci_scan_running)))
		return ret;

	if (c_info->cur_is_dfs) {
		ACSD_INFO("No CI scan when running in DFS chan:%x\n", c_info->cur_chspec);
	}
	else {
		ret = acs_run_ci_scan(c_info);
		ACS_ERR(ret, "ci scan failed\n");

		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");
	}

	acs_fcs_ci_scan_finish_check(c_info);

	return ret;
}

/* Default CI scan - Non EScan
 */
int acs_run_normal_ci_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + sizeof(uint16);
	acs_scan_chspec_t* scan_chspec_q = &c_info->scan_chspec_list;
	scan_chspec_elemt_t *scan_elemt = NULL;
	bool is_dfs = FALSE;
	channel_info_t ci;

	if ((scan_chspec_q->count - scan_chspec_q->excl_count) == 0) {
		ACSD_INFO("scan chanspec queue is empty.\n");
		return ret;
	}

	scan_elemt = &scan_chspec_q->chspec_list[scan_chspec_q->idx];
	if (scan_elemt->chspec_info & WL_CHAN_RADAR)
		is_dfs = TRUE;

	params = (wl_scan_params_t*)acsd_malloc(params_size);

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = is_dfs ? WL_SCANFLAGS_PASSIVE : 0;
	params->nprobes = -1;
	params->active_time = ACS_CI_SCAN_DWELL;
	params->passive_time = ACS_CI_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 1; /* 1 channel for each ci scan */

	params->channel_list[0] = htodchanspec(scan_elemt->chspec);

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	if (ret < 0)
		ACS_FREE(params);
	ACS_ERR(ret, "WLC_SCAN failed");

	if (!ret) {
		acs_ci_scan_update_idx(scan_chspec_q, 1);
		if (ACS_FCS_MODE(c_info))
			c_info->acs_fcs.timestamp_acs_scan = time(NULL);
		sleep_ms(ACS_CI_SCAN_DWELL * 5);
		for (i = 0; i < 10; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));

			if (ret < 0)
				ACS_FREE(params);
			ACS_ERR(ret, "WLC_GET_CHANNEL failed");

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(2);
		}
	}
	ACSD_INFO("ci scan on chspec: 0x%x\n", scan_elemt->chspec);
	ACS_FREE(params);
	return ret;
}

/* channel selection (full) scan at init/reset time */
int
acs_run_cs_scan(acs_chaninfo_t *c_info)
{
	int ret;

	if (c_info->acs_escan->acs_use_escan) {
		ret = acs_run_escan(c_info, ACS_SCAN_TYPE_CS);

		if (ret == BCME_UNSUPPORTED) {
			/* Escan unsupported. Use normal scan */
			c_info->acs_escan->acs_use_escan = 0;
		}
	}

	if (!c_info->acs_escan->acs_use_escan) {
		ret = acs_run_normal_cs_scan(c_info);
	}

	return ret;
}

/* Run full scan without using ESCAN
 */
int
acs_run_normal_cs_scan(acs_chaninfo_t *c_info)
{
	int ret = 0;
	int i;
	wl_scan_params_t *params = NULL;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + ACS_NUMCHANNELS * sizeof(uint16);
	channel_info_t ci;

	params = (wl_scan_params_t*)acsd_malloc(params_size);
	ret = acs_scan_prep(c_info, params, &params_size);
	if (ret < 0) {
		ACS_FREE(params);
		ACS_ERR(ret, "failed to do scan prep");
	}

	ret = wl_ioctl(c_info->name, WLC_SCAN, params, params_size);
	if (ret < 0) {
		ACS_FREE(params);
		ACS_ERR(ret, "WLC_SCAN failed");
	}

	memset(&ci, 0, sizeof(channel_info_t));
	/* loop to check if cs scan is done, check for scan in progress */
	if (!ret) {
		if (ACS_FCS_MODE(c_info)) {
			c_info->acs_fcs.timestamp_acs_scan = time(NULL);
			c_info->acs_fcs.timestamp_tx_idle = c_info->acs_fcs.timestamp_acs_scan;
		}
		/* this time needs to be < 1000 to prevent mpc kicking in for 2nd radio */
		sleep_ms(ACS_CS_SCAN_DWELL);
		for (i = 0; i < 100; i++) {
			ret = wl_ioctl(c_info->name, WLC_GET_CHANNEL, &ci, sizeof(channel_info_t));
			if (ret < 0) {
				ACS_FREE(params);
				ACS_ERR(ret, "WLC_GET_CHANNEL failed");
			}

			ci.scan_channel = dtoh32(ci.scan_channel);
			if (!ci.scan_channel)
				break;

			ACSD_PRINT("scan in progress ...\n");
			sleep_ms(ACS_CS_SCAN_DWELL);
		}
	}
	ACS_FREE(params);
	return ret;
}

static int
acs_get_scan(char* name, char *scan_buf, uint buf_len)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_buf;
	int ret = 0;

	list->buflen = htod32(buf_len);
	ret = wl_ioctl(name, WLC_SCAN_RESULTS, scan_buf, buf_len);
	if (ret)
		ACSD_ERROR("err from WLC_SCAN_RESULTS: %d\n", ret);

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
	           list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	           list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}
	ACSD_INFO("list->count: %d, list->buflen: %d\n", list->count, list->buflen);

	if (list->count)
		ret = 0;

	return ret;
}

/* channel selection (full) scan at init/reset time */
int
acs_run_escan(acs_chaninfo_t *c_info, uint8 scan_type)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params)) +
		(WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	int err;
	struct timeval tv, tv_tmp;
	time_t escan_timeout;

	params = (wl_escan_params_t*)acsd_malloc(params_size);
	if (params == NULL) {
		ACSD_ERROR("Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	if (scan_type == ACS_SCAN_TYPE_CS) {
		tv.tv_usec = 0;
		tv.tv_sec = 1;
		err = acs_escan_prep_cs(c_info, &params->params, &params_size);
		escan_timeout = time(NULL) + WL_CS_SCAN_TIMEOUT;
	} else if (scan_type == ACS_SCAN_TYPE_CI) {
		tv.tv_sec = 0;
		tv.tv_usec = WL_CI_SCAN_TIMEOUT;
		err = acs_escan_prep_ci(c_info, &params->params, &params_size);
		escan_timeout = time(NULL) + 1;
	} else {
		ACSD_ERROR("%s Unknown scan type %d\n", c_info->name, scan_type);
		return BCME_ERROR;
	}

	params->version = htod32(ESCAN_REQ_VERSION);
	params->action = htod16(WL_SCAN_ACTION_START);

	srand((unsigned)time(NULL));
	params->sync_id = htod16(random() & 0xffff);

	params_size += OFFSETOF(wl_escan_params_t, params);
	err = acs_set_escan_params(c_info, params, params_size);
	if (err != 0)
		goto exit2;

	c_info->acs_escan->scan_type = scan_type;
	c_info->acs_escan->acs_escan_inprogress = TRUE;

	acs_escan_free(c_info->acs_escan->escan_bss_head);
	c_info->acs_escan->escan_bss_head = NULL;
	c_info->acs_escan->escan_bss_tail = NULL;

	ACSD_INFO("Escan start \n");
	while (time(NULL) < escan_timeout && c_info->acs_escan->acs_escan_inprogress) {
		memcpy(&tv_tmp, &tv, sizeof(tv));
		acsd_main_loop(&tv_tmp);
	}
	c_info->acs_escan->acs_escan_inprogress = FALSE;
exit2:
	free(params);

	return err;
}

/* channel information (quick) scan at run time */
int
acs_escan_prep_ci(acs_chaninfo_t *c_info, wl_scan_params_t *params, int *params_size)
{
	int ret = 0;
	acs_scan_chspec_t* scan_chspec_q = &c_info->scan_chspec_list;
	scan_chspec_elemt_t *scan_elemt = NULL;
	bool is_dfs = FALSE;

	if ((scan_chspec_q->count - scan_chspec_q->excl_count) == 0) {
		ACSD_INFO("scan chanspec queue is empty.\n");
		return ret;
	}

	scan_elemt = &scan_chspec_q->chspec_list[scan_chspec_q->idx];
	if (scan_elemt->chspec_info & WL_CHAN_RADAR)
		is_dfs = TRUE;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = is_dfs ? WL_SCANFLAGS_PASSIVE : 0;
	params->nprobes = -1;
	params->active_time = ACS_CI_SCAN_DWELL;
	params->passive_time = ACS_CI_SCAN_DWELL;
	params->home_time = -1;
	params->channel_num = 1; /* 1 channel for each ci scan */

	params->channel_list[0] = htodchanspec(scan_elemt->chspec);

	acs_ci_scan_update_idx(scan_chspec_q, 1);
	if (ACS_FCS_MODE(c_info))
		c_info->acs_fcs.timestamp_acs_scan = time(NULL);

	*params_size = WL_SCAN_PARAMS_FIXED_SIZE + params->channel_num * sizeof(uint16);
	ACSD_INFO("ci scan on chspec: 0x%x\n", scan_elemt->chspec);

	return ret;
}

int
acs_escan_prep_cs(acs_chaninfo_t *c_info, wl_scan_params_t *params, int *params_size)
{
	int ret = 0;
	int i, scount = 0;
	acs_scan_chspec_t* scan_chspec_p = &c_info->scan_chspec_list;

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0; /* ACTIVE SCAN; */
	params->nprobes = -1;
	params->active_time = ACS_CS_SCAN_DWELL_ACTIVE;
	params->passive_time = ACS_CS_SCAN_DWELL;

	params->home_time = -1;
	params->channel_num = 0;

	ret = acs_build_scanlist(c_info);
	ACS_ERR(ret, "failed to build scan chanspec list");

	for (i = 0; i < scan_chspec_p->count; i++) {
		if (scan_chspec_p->chspec_list[i].flags & ACS_CI_SCAN_CHAN_EXCL)
			continue;
		params->channel_list[scount++] = htodchanspec(scan_chspec_p->chspec_list[i].chspec);
	}
	params->channel_num = htod32(scount & WL_SCAN_PARAMS_COUNT_MASK);
	ACSD_INFO("scan channel number: %d\n", params->channel_num);

	*params_size = WL_SCAN_PARAMS_FIXED_SIZE + scount * sizeof(uint16);
	ACSD_INFO("params size: %d\n", *params_size);

	return ret;
}

void
acs_escan_free(struct escan_bss *node)
{
	struct escan_bss *tmp;

	while (node) {
		tmp = node->next;
		free(node);
		node = tmp;
	}
}

void
acs_cleanup_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *headptr = c_info->acs_bss_info_q;
	acs_bss_info_entry_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		ACS_FREE(curptr);
	}
	c_info->acs_bss_info_q = NULL;
}

static void
display_scan_entry_local(acs_bss_info_sm_t * bsm)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_format_ssid(ssidbuf, bsm->SSID, bsm->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);
	printf("BSSID: %s\t", wl_ether_etoa(&bsm->BSSID));
	printf("chanspec: 0x%x\n", bsm->chanspec);
	printf("RSSI: %d dBm\t", (int16)bsm->RSSI);
	printf("Type: %s", ((bsm->type == ACS_BSS_TYPE_11A) ? "802.11A" :
		((bsm->type == ACS_BSS_TYPE_11G) ? "802.11G" : "802.11B")));
	printf("\n");
}

void
acs_dump_scan_entry(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *curptr = c_info->acs_bss_info_q;

	while (curptr) {
		display_scan_entry_local(&curptr->binfo_local);
		printf("timestamp: %u\n", (uint32)curptr->timestamp);
		curptr = curptr->next;
	}
}

static int
acs_insert_scan_entry(acs_chaninfo_t *c_info, acs_bss_info_entry_t * new)
{
	acs_bss_info_entry_t *curptr, *previous = NULL;
	acs_bss_info_entry_t **rootp = &c_info->acs_bss_info_q;

	curptr = *rootp;
	previous = curptr;

	while (curptr &&
	   memcmp(&curptr->binfo_local.BSSID, &new->binfo_local.BSSID, sizeof(struct ether_addr))) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;
	if (previous == NULL)
		*rootp = new;
	else {
		if (curptr == NULL)
			previous->next = new;
		else /* find an existing entry */ {
			curptr->timestamp = new->timestamp;
			memcpy(&curptr->binfo_local, &new->binfo_local, sizeof(acs_bss_info_sm_t));
			ACS_FREE(new);
		}
	}
	return 0;
}

static int
acs_update_escanresult_queue(acs_chaninfo_t *c_info)
{
	struct escan_bss *escan_bss_head;
	wl_bss_info_t *bi;
	acs_bss_info_entry_t * new_entry = NULL;
	acs_channel_t chan;
	chanspec_t cur_chspec;

	for (escan_bss_head = c_info->acs_escan->escan_bss_head;
		escan_bss_head != NULL;
		escan_bss_head = escan_bss_head->next) {

		new_entry = (acs_bss_info_entry_t*)acsd_malloc(sizeof(acs_bss_info_entry_t));
		bi = escan_bss_head->bss;
		new_entry->binfo_local.chanspec = cur_chspec = dtoh16(bi->chanspec);
		new_entry->binfo_local.RSSI = dtoh16(bi->RSSI);
		new_entry->binfo_local.SSID_len = bi->SSID_len;
		memcpy(new_entry->binfo_local.SSID, bi->SSID, bi->SSID_len);
		memcpy(&new_entry->binfo_local.BSSID, &bi->BSSID, sizeof(struct ether_addr));
		new_entry->timestamp = time(NULL);
		acs_parse_chanspec(cur_chspec, &chan);
		ACSD_FCS("Scan: chanspec 0x%x, control %x SSID %s\n", cur_chspec,
			chan.control, new_entry->binfo_local.SSID);
		/* BSS type in 2.4G band */
		if (chan.control <= ACS_CS_MAX_2G_CHAN) {
			if (acs_bss_is_11b(bi))
				new_entry->binfo_local.type = ACS_BSS_TYPE_11B;
			else
				new_entry->binfo_local.type = ACS_BSS_TYPE_11G;
		}
		else
			new_entry->binfo_local.type = ACS_BSS_TYPE_11A;
		acs_insert_scan_entry(c_info, new_entry);
	}
	return 0;
}

static int
acs_update_scanresult_queue(acs_chaninfo_t *c_info)
{
	wl_scan_results_t* s_result = c_info->scan_results;
	wl_bss_info_t *bi = s_result->bss_info;
	int b, len = 0;
	acs_bss_info_entry_t * new_entry = NULL;
	acs_channel_t chan;
	chanspec_t cur_chspec;

	for (b = 0; b < s_result->count; b ++, bi = (wl_bss_info_t*)((int8*)bi + len)) {

		len = dtoh32(bi->length);
		new_entry = (acs_bss_info_entry_t*)acsd_malloc(sizeof(acs_bss_info_entry_t));

		new_entry->binfo_local.chanspec = cur_chspec = dtoh16(bi->chanspec);
		new_entry->binfo_local.RSSI = dtoh16(bi->RSSI);
		new_entry->binfo_local.SSID_len = bi->SSID_len;
		memcpy(new_entry->binfo_local.SSID, bi->SSID, bi->SSID_len);
		memcpy(&new_entry->binfo_local.BSSID, &bi->BSSID, sizeof(struct ether_addr));
		new_entry->timestamp = time(NULL);
		acs_parse_chanspec(cur_chspec, &chan);
		ACSD_FCS("Scan: chanspec 0x%x, control %x SSID %s\n", cur_chspec,
			chan.control, new_entry->binfo_local.SSID);
		/* BSS type in 2.4G band */
		if (chan.control <= ACS_CS_MAX_2G_CHAN) {
			if (acs_bss_is_11b(bi))
				new_entry->binfo_local.type = ACS_BSS_TYPE_11B;
			else
				new_entry->binfo_local.type = ACS_BSS_TYPE_11G;
		}
		else
			new_entry->binfo_local.type = ACS_BSS_TYPE_11A;
		acs_insert_scan_entry(c_info, new_entry);
	}
	return 0;
}

#ifdef ACS_DEBUG
static void
acs_dump_chan_bss(acs_chan_bssinfo_t* bssinfo, int ncis)
{
	int c;
	acs_chan_bssinfo_t *cur;

	printf("channel nCtrl nExt20 nExt40 nExt80\n");
	for (c = 0; c < ncis; c++) {
		cur = &bssinfo[c];
		printf("%3d  %5d%6d%7d%7d\n", cur->channel, cur->nCtrl,
			cur->nExt20, cur->nExt40, cur->nExt80);
	}
}
#endif /* ACS_DEBUG */

static void
acs_incr_bss_count(acs_chan_bssinfo_t *bss_info,
	acs_channel_t *chan_p, uint8 channel)
{
	int i = 0;
	uint8 *chan_parse = (uint8 *) chan_p;
	uint8 *bss_info_chan = (uint8 *) bss_info;
	uint8 channel_info_length = sizeof(acs_channel_t)/sizeof(uint8);

	for (i = 0; i < channel_info_length; i++) {
		if (*(chan_parse + i * sizeof(*chan_parse)) == channel) {
			int j = 1;
			int k = i;
			/* Find the bss count index to increment
			 * index - 0		- increase nCtrl
			 * index - 1		- increase nExt20
			 * index - 2,3		- increase nExt40
			 * index - 4,5,6,7	- increase nExt80
			 */
			while (k) {
				k >>= 1;
				j++;
			}
			*(bss_info_chan + j * sizeof(*chan_parse)) += 1;
		}
	}
}

static int
acs_update_chan_bssinfo(acs_chaninfo_t *c_info)
{
	acs_bss_info_entry_t *biq;
	scan_chspec_elemt_t* chspec_list;
	char * new_buf = NULL;
	acs_channel_t chan;
	acs_channel_t *chan_p = &chan;
	chanspec_t cur_chspec;
	int count = 0, buf_size, c;
	acs_chan_bssinfo_t *bss_info;

	count = c_info->scan_chspec_list.count;
	chspec_list = c_info->scan_chspec_list.chspec_list;

	if (count == 0)
		return 0;

	buf_size = sizeof(acs_chan_bssinfo_t) * count;
	new_buf = acsd_malloc(buf_size);

	bss_info = (acs_chan_bssinfo_t *) new_buf;

	for (c = 0; c < count; c ++) {
		bzero(&bss_info[c], sizeof(acs_chan_bssinfo_t));

		biq = c_info->acs_bss_info_q;
		/* set channel range centered by the scan channel */
		bss_info[c].channel = CHSPEC_CHANNEL(chspec_list[c].chspec);
		ACSD_DEBUG("count: %d, channel: %d\n", c, bss_info[c].channel);

		while (biq) {
			assert(biq);
			cur_chspec = biq->binfo_local.chanspec;
			acs_parse_chanspec(cur_chspec, chan_p);

			/* Find and increase bss counts per channel */
			acs_incr_bss_count(&bss_info[c], chan_p, bss_info[c].channel);
			biq = biq->next;
		}
		ACSD_DEBUG(" channel %u: %u nCtrl %u nExt20 %u nExt40 %u nExt80\n",
			bss_info[c].channel, bss_info[c].nCtrl, bss_info[c].nExt20,
			bss_info[c].nExt40, bss_info[c].nExt80);
	}

	ACS_FREE(c_info->ch_bssinfo);
	c_info->ch_bssinfo = (acs_chan_bssinfo_t *) new_buf;

#ifdef ACS_DEBUG
	acs_dump_chan_bss(c_info->ch_bssinfo, c_info->scan_chspec_list.count);
	acs_dump_scan_entry(c_info);
#endif /* ACS_DEBUG */

	return 0;
}

int
acs_request_data(acs_chaninfo_t *c_info)
{
	if (c_info->acs_escan->acs_use_escan)
		return acs_request_escan_data(c_info);
	else
		return acs_request_normal_scan_data(c_info);
}

int
acs_request_escan_data(acs_chaninfo_t *c_info)
{
	int ret;

	ret = acs_update_escanresult_queue(c_info);
	acs_update_chan_bssinfo(c_info);

	acsd_chanim_query(c_info, WL_CHANIM_COUNT_ALL, 0);

	return ret;
}

int
acs_request_normal_scan_data(acs_chaninfo_t *c_info)
{
	int ret = 0;

	char *dump_buf = acsd_malloc(ACS_SRSLT_BUF_LEN);

	ret = acs_get_scan(c_info->name, dump_buf, ACS_SRSLT_BUF_LEN);

	ACS_FREE(c_info->scan_results);
	c_info->scan_results = (wl_scan_results_t *)dump_buf;

	acs_update_scanresult_queue(c_info);
	acs_update_chan_bssinfo(c_info);

	acsd_chanim_query(c_info, WL_CHANIM_COUNT_ALL, 0);

	return ret;
}

/* Derive bandwidth from a given chanspec(i.e cur_chspec) */
int
acs_derive_bw_from_given_chspec(acs_chaninfo_t * c_info)
{
	int bw, chbw;

	chbw = CHSPEC_BW(c_info->cur_chspec);
	switch (chbw) {
		case WL_CHANSPEC_BW_160:
			bw = ACS_BW_160;
			break;
		case WL_CHANSPEC_BW_8080:
			bw = ACS_BW_8080;
			break;
		case WL_CHANSPEC_BW_80:
			bw = ACS_BW_80;
			break;
		case WL_CHANSPEC_BW_40:
			bw = ACS_BW_40;
			break;
		case WL_CHANSPEC_BW_20:
			bw = ACS_BW_20;
			break;
		default:
			ACSD_ERROR("bandwidth unsupported ");
			return BCME_UNSUPPORTED;
	}
	return bw;
}

/* When ACSD is in SCS mode on 5G, nvram control and 802.11h is enabled, this function checks
 * whether the channel is dfs or not if so, it will change from dfs to non dfs channel.
 * These changes are made according to customer requirement.
 */
int
acs_scs_cs_scan_change_from_dfs_to_nondfs(acs_chaninfo_t * c_info)
{

	int cur_chspec = 0;
	acs_rsi_t *rsi = &c_info->rs_info;
	int count = 0, ret = 0;
	int i, bw;
	ch_candidate_t *candi;

	if (!ACS_FCS_MODE(c_info) && BAND_5G(c_info->rs_info.band_type) &&
		(rsi->reg_11h == TRUE) && c_info->cur_is_dfs && c_info->acsd_scs_dfs_scan) {
		bw = acs_derive_bw_from_given_chspec(c_info);
		ACS_ERR(bw, "bandwidth unsupported for given chanspec\n");

find_chan:
		count = c_info->c_count[bw];
		if (!count) {
			if (bw > ACS_BW_20)
			{
				bw = bw - 1;
				goto find_chan;
			}
			ACSD_ERROR("acs number of valid chanspec is %d\n", count);
			return -1;
		}

		candi = c_info->candidate[bw];
		for (i = 0; i < count; i++) {
			if (!(acs_channel_info(c_info, candi[i].chspec) & WL_CHAN_RADAR)) {
				cur_chspec = candi[i].chspec;
				ACSD_DEBUG("picking non dfs chanspec is %x \n", cur_chspec);
				break;
			}
		}

		if (cur_chspec == 0) {
			if (bw > ACS_BW_20)
			{
				bw = bw - 1;
				goto find_chan;
			}
			ACSD_ERROR("acs found no valid non-DFS channel\n");
			return -1;
		}

		ACSD_DEBUG("Set to non dfs channel\n");
		ret = acs_set_chanspec(c_info, cur_chspec);
		ACS_ERR(ret, "set chanspec failed\n");

		c_info->cur_chspec = cur_chspec;
		ret = acs_update_driver(c_info);
		ACS_ERR(ret, "update driver failed\n");
	}
	return ret;
}
/*
 * acs_scan_timer_or_dfsr_check() - check for scan timer or dfs reentry, change channel if needed.
 *
 * This function checks whether we need to change channels because of CS scan timer expiration or
 * DFS Reentry, and does the channel switch if so.
 */
int
acs_scan_timer_or_dfsr_check(acs_chaninfo_t * c_info)
{
	uint cs_scan_timer;
	chanim_info_t * ch_info;
	int ret = 0;
	uint8 cur_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	int switch_reason = APCS_INIT; /* Abuse APCS_INIT as undefined switch reason (no switch) */
	ch_info = c_info->chanim_info;
	cur_idx = chanim_mark(ch_info).record_idx;
	start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
	start_record = &ch_info->record[start_idx];

	if (AUTOCHANNEL(c_info) && ((c_info->acsd_scs_dfs_scan && !ACS_FCS_MODE(c_info)) ||
		 !(c_info->cur_is_dfs))) {
		/* Check whether we should switch now because of the CS scan timer */
		cs_scan_timer = c_info->acs_cs_scan_timer;

		if (SCAN_TIMER_ON(c_info) && cs_scan_timer) {
			time_t passed;

			ACSD_DEBUG(" timer: %d\n", cs_scan_timer);

			passed = time(NULL) - start_record->timestamp;

			if (acs_fcs_tx_idle_check(c_info) ||
				((passed > cs_scan_timer) && (!acs_check_assoc_scb(c_info)))) {
				switch_reason = APCS_CSTIMER;
			}
		}
	}

	/* If not switching because of CS scan timer, see if DFS Reentry switch is needed */
	if ((switch_reason == APCS_INIT) &&
		ACS_FCS_MODE(c_info) &&
		acs_dfsr_reentry_type(ACS_DFSR_CTX(c_info)) == DFS_REENTRY_IMMEDIATE) {
			ACSD_DFSR("Switching Channels for DFS Reentry.\n");
			switch_reason = APCS_DFS_REENTRY;
	}
	c_info->switch_reason = switch_reason;

	switch (switch_reason) {
	case APCS_CSTIMER:
		/* Handling the ACS_SCS mode criteria according to customer requirement */
		ret = acs_scs_cs_scan_change_from_dfs_to_nondfs(c_info);
		ACS_ERR(ret, "acs_scs_cs_scan_change_from_dfs_to_non_dfs failed\n");
		/* start scan */
		ret = acs_run_cs_scan(c_info);
		ACS_ERR(ret, "cs scan failed\n");
		acs_cleanup_scan_entry(c_info);

		ret = acs_request_data(c_info);
		ACS_ERR(ret, "request data failed\n");

		/* Fall through to common case */

	case APCS_DFS_REENTRY:

		if (acs_select_chspec(c_info)) {
			/* Other APP can request to change the channel via acsd, in that
			 * case proper reason will be provided by requesting APP, For ACSD
			 * USE_ACSD_DEF_METHOD: ACSD's own default method to set channel
			 */
			acs_set_chspec(c_info, TRUE, ACSD_USE_DEF_METHOD);

			/*
			 * In FCC, on txfail, if bgdfs is not successful due to txblanking
			 * we enable flag to do CAC on Full MIMO
			 */
			if (ACS_11H_AND_BGDFS(c_info) &&
				c_info->acs_bgdfs->fallback_blocking_cac &&
				c_info->acs_bgdfs->state != BGDFS_STATE_IDLE &&
				!c_info->country_is_edcrs_eu) {
					c_info->acs_bgdfs->acs_bgdfs_on_txfail = TRUE;
			}

			chanim_upd_acs_record(ch_info, c_info->selected_chspec, switch_reason);

			if (c_info->acs_bgdfs == NULL ||
				c_info->acs_bgdfs->state == BGDFS_STATE_IDLE) {
					ret = acs_update_driver(c_info);
					ACS_ERR(ret, "update driver failed\n");
			}
		}
		else {
		    start_record->timestamp = time(NULL);
		}
		break;

	default:
		break;
	}
	return ret;
}
