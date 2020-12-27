/*
 *	acs_fcs.c
 *
 *	This module explains the behavior of FCS mode and it will initiate
 *	the ci_scan update the results to timer.
 *
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
 *	$Id: acs_fcs.c 764752 2018-05-31 05:37:14Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <assert.h>
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmendian.h>

#include "acsd_svr.h"

void acs_fcs_retrieve_config(acs_chaninfo_t *c_info, char * prefix)
{
	/* retrieve policy related configuration from nvram */
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	char tmp[100], *str;
	uint8 chan_count;

	ACSD_INFO("retrieve FCS config from nvram ...\n");

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_period", tmp))) == NULL)
		fcs_info->acs_txdelay_period = ACS_TXDELAY_PERIOD;
	else
		fcs_info->acs_txdelay_period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_cnt", tmp))) == NULL)
		fcs_info->acs_txdelay_cnt = ACS_TXDELAY_CNT;
	else
		fcs_info->acs_txdelay_cnt = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_txdelay_ratio", tmp))) == NULL)
		fcs_info->acs_txdelay_ratio = ACS_TXDELAY_RATIO;
	else
		fcs_info->acs_txdelay_ratio = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_far_sta_rssi", tmp))) == NULL)
		fcs_info->acs_far_sta_rssi = ACS_FAR_STA_RSSI;
	else
		fcs_info->acs_far_sta_rssi = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_nofcs_least_rssi", tmp))) == NULL)
		fcs_info->acs_nofcs_least_rssi = ACS_NOFCS_LEAST_RSSI;
	else
		fcs_info->acs_nofcs_least_rssi = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_scan_chanim_stats", tmp))) == NULL)
		fcs_info->acs_scan_chanim_stats = ACS_SCAN_CHANIM_STATS;
	else
		fcs_info->acs_scan_chanim_stats = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_fcs_chanim_stats", tmp))) == NULL)
		fcs_info->acs_fcs_chanim_stats = ACS_FCS_CHANIM_STATS;
	else
		fcs_info->acs_fcs_chanim_stats = atoi(str);

	memset(&fcs_info->pref_chans, 0, sizeof(fcs_conf_chspec_t));
	if ((str = nvram_get(strcat_r(prefix, "acs_pref_chans", tmp))) == NULL)	{
		fcs_info->pref_chans.count = 0;
	} else {
		chan_count = acs_set_chan_table(str, fcs_info->pref_chans.clist, ACS_MAX_LIST_LEN);
		fcs_info->pref_chans.count = chan_count;
	}

	memset(&fcs_info->excl_chans, 0, sizeof(fcs_conf_chspec_t));
	if ((str = nvram_get(strcat_r(prefix, "acs_excl_chans", tmp))) == NULL)	{
		fcs_info->excl_chans.count = 0;
	} else {
		chan_count = acs_set_chan_table(str, fcs_info->excl_chans.clist, ACS_MAX_LIST_LEN);
		fcs_info->excl_chans.count = chan_count;
	}

	if ((str = nvram_get(strcat_r(prefix, "acs_dfs", tmp))) == NULL)
		fcs_info->acs_dfs = ACS_DFS_ENABLED;
	else
		fcs_info->acs_dfs = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_chan_dwell_time", tmp))) == NULL)
		fcs_info->acs_chan_dwell_time = ACS_CHAN_DWELL_TIME;
	else
		fcs_info->acs_chan_dwell_time = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_chan_flop_period", tmp))) == NULL)
		fcs_info->acs_chan_flop_period = ACS_CHAN_FLOP_PERIOD;
	else
		fcs_info->acs_chan_flop_period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_tx_idle_cnt", tmp))) == NULL)
		fcs_info->acs_tx_idle_cnt = ACS_TX_IDLE_CNT;
	else
		fcs_info->acs_tx_idle_cnt = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_ci_scan_timeout", tmp))) == NULL)
		fcs_info->acs_ci_scan_timeout = ACS_CI_SCAN_TIMEOUT;
	else
		fcs_info->acs_ci_scan_timeout = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_cs_scan_timer", tmp))) == NULL)
		c_info->acs_cs_scan_timer = ACS_DFLT_CS_SCAN_TIMER;
	else
		c_info->acs_cs_scan_timer = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "acs_ci_scan_timer", tmp))) == NULL)
		c_info->acs_ci_scan_timer = ACS_DFLT_CI_SCAN_TIMER;
	else
		c_info->acs_ci_scan_timer = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "intfer_period", tmp))) == NULL)
		fcs_info->intfparams.period = ACS_INTFER_SAMPLE_PERIOD;
	else
		fcs_info->intfparams.period = atoi(str);

	if ((str = nvram_get(strcat_r(prefix, "intfer_cnt", tmp))) == NULL)
		fcs_info->intfparams.cnt = ACS_INTFER_SAMPLE_COUNT;
	else
		fcs_info->intfparams.cnt = atoi(str);

	fcs_info->intfparams.thld_setting = ACSD_INTFER_THLD_SETTING;

	if ((str = nvram_get(strcat_r(prefix, "intfer_txfail", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD].txfail_thresh =
			ACS_INTFER_TXFAIL_THRESH;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD].txfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_tcptxfail", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD].tcptxfail_thresh =
			ACS_INTFER_TCPTXFAIL_THRESH;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD].tcptxfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_txfail_hi", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD_HI].txfail_thresh =
			ACS_INTFER_TXFAIL_THRESH_HI;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD_HI].txfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_tcptxfail_hi", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD_HI].tcptxfail_thresh =
			ACS_INTFER_TCPTXFAIL_THRESH_HI;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_80_THLD_HI].tcptxfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_txfail_160", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD].txfail_thresh =
			ACS_INTFER_TXFAIL_THRESH_160;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD].txfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_tcptxfail_160", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD].tcptxfail_thresh =
			ACS_INTFER_TCPTXFAIL_THRESH_160;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD].tcptxfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_txfail_160_hi", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD_HI].txfail_thresh =
			ACS_INTFER_TXFAIL_THRESH_160_HI;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD_HI].txfail_thresh =
			strtoul(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "intfer_tcptxfail_160_hi", tmp))) == NULL) {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD_HI].tcptxfail_thresh =
			ACS_INTFER_TCPTXFAIL_THRESH_160_HI;
	} else {
		fcs_info->intfparams.acs_txfail_thresholds[ACSD_INTFER_PARAMS_160_THLD_HI].tcptxfail_thresh =
			strtoul(str, NULL, 0);
	}

	if (nvram_match(strcat_r(prefix, "dcs_csa_unicast", tmp), "1"))
		fcs_info->acs_dcs_csa = CSA_UNICAST_ACTION_FRAME;
	else
		fcs_info->acs_dcs_csa = CSA_BROADCAST_ACTION_FRAME;

	if ((str = nvram_get(strcat_r(prefix, "fcs_txop_weight", tmp))) == NULL)
		fcs_info->txop_weight = ACS_DEFAULT_TXOP_WEIGHT;
	else
		fcs_info->txop_weight = strtol(str, NULL, 0);

	if ((str = nvram_get(strcat_r(prefix, "acs_dfs_reentry", tmp))) == NULL) {
		c_info->dfs_reentry = ACS_DFS_REENTRY_EN;
	} else {
		c_info->dfs_reentry = strtol(str, NULL, 0);
	}

	if ((str = nvram_get(strcat_r(prefix, "acs_inttrf_thresh", tmp))) == NULL) {
		fcs_info->trf_params.thresh = ACS_INTFER_TRF_THRESH;
	} else {
		fcs_info->trf_params.thresh = atoi(str);
	}

	if ((str = nvram_get(strcat_r(prefix, "acs_inttrf_numsecs", tmp))) == NULL) {
		fcs_info->trf_params.num_secs = ACS_INTFER_TRF_NUMSECS;
	} else {
		fcs_info->trf_params.num_secs = atoi(str);
	}

#ifdef ACSD_SEGMENT_CHANIM
	if ((str = nvram_get(strcat_r(prefix, "acs_segment_chanim", tmp))) == NULL) {
		c_info->segment_chanim = ACSD_SEGMENT_CHANIM_DEFAULT;
	} else {
		c_info->segment_chanim = (bool) strtol(str, NULL, 0);
	}
	if ((str = nvram_get(strcat_r(prefix, "acs_chanim_num_segments", tmp))) == NULL) {
		c_info->num_seg = ACSD_NUM_SEG_DEFAULT;
	} else {
		int tmp = strtol(str, NULL, 0);
		if (tmp < ACSD_NUM_SEG_MIN || tmp > ACSD_NUM_SEG_MAX) {
			c_info->num_seg = ACSD_NUM_SEG_DEFAULT;
		} else {
			c_info->num_seg = tmp;
		}
	}
#endif /* ACSD_SEGMENT_CHANIM */

#ifdef DEBUG
	acs_dump_config_extra(c_info);
#endif /* DEBUG */
}

/*
 * acs_pick_chanspec_fcs_policy() - FCS policy specific function to pick a chanspec to switch to.
 *
 * c_info:	pointer to the acs_chaninfo_t for this interface.
 * bw:		bandwidth to chose from
 *
 * Returned value:
 *	The returned value is the most preferred valid chanspec from the candidate array.
 *
 * This function picks the most preferred chanspec according to the FCS policy. At the time of
 * this writing, this is a selection based on the CH_SCORE_ADJ score.
 */
chanspec_t acs_pick_chanspec_fcs_policy(acs_chaninfo_t *c_info, int bw)
{
	if (c_info->acs_fcs.txop_weight) /* use ADJ + TXOP */
		return acs_pick_chanspec_common(c_info, bw, CH_SCORE_TOTAL);
	else
		return acs_pick_chanspec_common(c_info, bw, CH_SCORE_ADJ);
}

int acs_fcs_tx_idle_check(acs_chaninfo_t *c_info)
{
	uint timer = c_info->acs_cs_scan_timer;
	time_t now = time(NULL);
	char cntbuf[ACSD_WL_CNTBUF_SIZE];
	wl_cnt_info_t *cntinfo;
	const wl_cnt_wlc_t *wlc_cnt;
	int full_scan = 0;
	int ret = 0;
	uint32 acs_txframe;
	acs_fcs_t *fcs_info = &c_info->acs_fcs;

	if (!ACS_FCS_MODE(c_info))
		return full_scan;

	/* Check for idle period "acs_cs_scan_timer" */
	if ((now - fcs_info->timestamp_tx_idle) < timer)
		return full_scan;

	ACSD_FCS("acs_fcs_tx_idle: now %u(%u)\n", (uint)now, fcs_info->timestamp_tx_idle);

	/* Check wl transmit activity and trigger full scan if it is idle */
	ret = acs_get_dfsr_counters(c_info->name, cntbuf);
	if (ret < 0) {
		ACSD_ERROR("wl counters failed (%d)\n", ret);
		return full_scan;
	}

	cntinfo = (wl_cnt_info_t *)cntbuf;
	cntinfo->version = dtoh16(cntinfo->version);
	cntinfo->datalen = dtoh16(cntinfo->datalen);
	/* Translate traditional (ver <= 10) counters struct to new xtlv type struct */
	/* As we need only wlc layer ctrs here, no need to input corerev.  */
	ret = wl_cntbuf_to_xtlv_format(NULL, cntbuf, ACSD_WL_CNTBUF_SIZE, 0);
	if (ret < 0) {
		ACSD_ERROR("wl_cntbuf_to_xtlv_format failed (%d)\n", ret);
		return full_scan;
	}

	if (!(wlc_cnt = GET_WLCCNT_FROM_CNTBUF(cntbuf))) {
		ACSD_ERROR("wlc_cnt NULL\n");
		return full_scan;
	}

	ACSD_FCS("acs_fcs_tx_idle: txframe %d(%d)\n", wlc_cnt->txframe, fcs_info->acs_txframe);

	if (wlc_cnt->txframe > fcs_info->acs_txframe)
		acs_txframe = wlc_cnt->txframe - fcs_info->acs_txframe;
	else
		acs_txframe = wlc_cnt->txframe + ((uint32)0xFFFFFFFF - fcs_info->acs_txframe);

	if (acs_txframe < (fcs_info->acs_tx_idle_cnt * (now - fcs_info->timestamp_tx_idle))) {
		ACSD_FCS("acs_fcs_tx_idle fullscan: %d\n",	fcs_info->acs_txframe);
		full_scan = 1;
	}

	fcs_info->acs_txframe = wlc_cnt->txframe;
	fcs_info->timestamp_tx_idle = now;
	return full_scan;
}

int acs_fcs_ci_scan_check(acs_chaninfo_t *c_info)
{
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	acs_scan_chspec_t* chspec_q = &c_info->scan_chspec_list;
	time_t now = time(NULL);

	/* return for non fcs mode or no chan to scan */
	if (!ACS_FCS_MODE(c_info) || (chspec_q->count <= chspec_q->excl_count)) {
		return 0;
	}

	if ((CHSPEC_BW(c_info->cur_chspec) > WL_CHANSPEC_BW_40) &&
		!acsd_is_lp_chan(c_info, c_info->cur_chspec)) {
		ACSD_FCS("%s@%d: No CI scan if running in %dM high chan\n", __FUNCTION__, __LINE__,
			((CHSPEC_BW(c_info->cur_chspec) == WL_CHANSPEC_BW_80) ? 80 :
			(CHSPEC_BW(c_info->cur_chspec) == WL_CHANSPEC_BW_8080) ? 8080 : 160));
		return 0;
	}

	/* start ci scan:
	1. when txop is less than thld, start ci scan for pref chan
	2. if no scan for a long period, start ci scan
	*/

	/* scan pref chan: when txop < thld, start ci scan for pref chan */
	if (c_info->scan_chspec_list.ci_pref_scan_request && (chspec_q->pref_count > 0)) {
		c_info->scan_chspec_list.ci_pref_scan_request = FALSE;

		if (chspec_q->ci_scan_running != ACS_CI_SCAN_RUNNING_PREF) {
			ACSD_FCS("acs_ci_scan_timeout start CI pref scan: scan_count %d\n",
				chspec_q->pref_count);
			chspec_q->ci_scan_running = ACS_CI_SCAN_RUNNING_PREF;
			fcs_info->acs_ci_scan_count = chspec_q->pref_count;
			acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);
		}
	}

	/* check for current scanning status */
	if (chspec_q->ci_scan_running)
		return 1;

	/* check scan timeout, and trigger CI scan if timeout happened */
	if ((now - fcs_info->timestamp_acs_scan) >= fcs_info->acs_ci_scan_timeout) {
		fcs_info->acs_ci_scan_count = chspec_q->count - chspec_q->excl_count;
		chspec_q->ci_scan_running = ACS_CI_SCAN_RUNNING_NORM;
		acs_ci_scan_update_idx(&c_info->scan_chspec_list, 0);
		ACSD_FCS("acs_ci_scan_timeout start CI scan: now %u(%u), scan_count %d\n",
			(uint)now, fcs_info->timestamp_acs_scan,
			chspec_q->count - chspec_q->excl_count);
		return 1;
	}

	return 0;
}

int acs_fcs_ci_scan_finish_check(acs_chaninfo_t * c_info)
{
	acs_fcs_t *fcs_info = &c_info->acs_fcs;
	acs_scan_chspec_t* chspec_q = &c_info->scan_chspec_list;

	/* do nothing for fcs mode or scanning not active  */
	if ((!ACS_FCS_MODE(c_info)) || (!chspec_q->ci_scan_running))
		return 0;

	/* Check for end of scan: scanned all channels once */
	if ((fcs_info->acs_ci_scan_count) && (!(--fcs_info->acs_ci_scan_count))) {
		ACSD_FCS("acs_ci_scan_timeout stop CI scan: now %u \n", (uint)time(NULL));
		chspec_q->ci_scan_running = 0;
	}

	return 0;
}

/* check for availability of high power channel present in the list of
 * valid channels to select
 */
bool acs_fcs_check_for_hp_chan(acs_chaninfo_t *c_info, int bw)
{
	int i;
	ch_candidate_t* candi;
	bool ret = FALSE;
	candi = c_info->candidate[bw];
	for (i = 0; i < c_info->c_count[bw]; i++) {
		if ((!candi[i].valid) || (candi[i].is_dfs)) {
			continue;
		}

		if (!acsd_is_lp_chan(c_info, candi[i].chspec)) {
			ret = TRUE;
			break;
		}
	}
	return ret;

}

/* check for overlap between the passed channel arguments */
bool acs_fcs_check_for_overlap(chanspec_t cur_chspec, chanspec_t candi_chspec)
{
	uint8 channel1, channel2;

	FOREACH_20_SB(candi_chspec, channel1) {
		FOREACH_20_SB(cur_chspec, channel2) {
			if (channel1 == channel2) {
				return TRUE;
			}
		}
	}

	return FALSE;
}
