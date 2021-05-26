/**
 * Opportunistic Power Save (OPS) module source
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
 * http://confluence.broadcom.com/display/WLAN/Opportunistic+Power+Save
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *
 * $Id: wlc_ops.c $
 *
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WLMCNX
#error "WLMCNX must be defined when WL_OPS is defined!"
#endif // endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bsscfg.h>
#include <wlc_ops.h>
#include <wlc_lq.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wlc_mcnx.h>
#include <wlc_alloc.h>
#include <wlc_assoc.h>

/*
 * iovars
 */
enum {
	IOV_OPS_CFG = 0,
	IOV_OPS_STATUS = 1,
	IOV_LAST
};

/* iovar table */
static const bcm_iovar_t ops_iovars[] = {
	{"ops_status", IOV_OPS_STATUS, (0), 0,
	IOVT_BUFFER, sizeof(wl_ops_status_t)},
	{"ops_cfg", IOV_OPS_CFG, 0, (0), IOVT_BUFFER, OFFSETOF(wl_ops_cfg_t, data)},
	{NULL, 0, 0, 0, 0, 0}
};

/*
 * module private stats which is returned part of iovar ops_status,
 * the implemenation expects field-by-field match with
 * stats portion of the ops_status_t
 */
typedef struct wlc_ops_stats_priv {
	uint32  partial_ops_dur;
	uint32  full_ops_dur;
	uint32  count_dur_hist[OPS_DUR_HIST_BINS];
	uint32  nav_cnt;
	uint32  plcp_cnt;
	uint32  mybss_cnt;
	uint32  obss_cnt;
	uint32  miss_dur_cnt;
	uint32  miss_premt_cnt;
	uint32  max_dur_cnt;
	uint32	wake_cnt;
	uint32	bcn_wait_cnt;
} wlc_ops_stats_priv_t;

typedef struct wlc_ops_info {
	wlc_info_t *wlc;
	uint32	ops_cfg;
	uint32	ops_max_sleep_dur;
	uint32	disable_reqs;
	uint32	disable_duration;
	uint32  disable_start_time;
	uint32  applied_ops_config;
	uint32  last_bcn_time;
	uint32	bcn_pend_time;
	uint32  bcn_miss_cnt;
	uint32  consecutive_bcn_cnt;
	uint32	rssi_diff_chk_fail_cnt;
	bool	bcn_pend;
	bool	disable_obss;
	int8	prev_bcn_rssi;
	uint32	slice_mask;
	wlc_ops_stats_priv_t *prev_stats;	/* prev stats from ucode for reference */
	wlc_ops_stats_priv_t *curr_stats;	/* current sum of stats on the slice */
} wlc_ops_info_t;

#define OPS_PHY_PREEMPTION_THRESH	10	/* dB prempt thresh */
#define	OPS_STATS_UPD_TIME		30	/* periodic stats update time (ms) */
#define OPS_BCN_RSSI_DIFF		5	/* in dB */
#define OPS_CONSECUTIVE_BCN_THRESH	2

#define	OPS_CFG_OBSS_MASK		(WL_OPS_OBSS_PLCP_DUR | WL_OPS_OBSS_NAV_DUR)

#define WL_OPS_INFO(args)	WL_INFORM(args)

/* read 16bit counters from shm and update stats */
#define WLOPS_CNT16_UPD(tot, ref, shm, wlc) \
{ \
	uint16 _val, _diff; \
	_val = wlc_bmac_read_shm((wlc)->hw, (shm)); \
	_diff = _val - (uint16)ref; \
	tot += _diff; \
	ref = _val; \
}
static int wlc_ops_handle_up_state(void *ctx);
static int wlc_ops_handle_down_state(void *ctx);
static void wlc_ops_watchdog(void *ctx);
static void wlc_ops_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *notif_data);
static void wlc_ops_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data);
static void wlc_ops_disassoc_state_cb(void *ctx, bss_disassoc_notif_data_t *notif_data);
static void wlc_ops_configure_shmem(wlc_ops_info_t *ops_info);
static void wlc_ops_update_disable_reqs(wlc_ops_info_t *ops_info, uint32 req, bool disable);
static int ops_cfg_get(wlc_ops_info_t *ops, void *params, uint p_len,
                       void *arg, uint len);
static int ops_cfg_set(wlc_ops_info_t *ops, void *arg, uint len);
static void wlc_ops_stats_snapshot_cntr(wlc_info_t *wlc);
static void wlc_ops_stats_snapshot(wlc_info_t *wlc);
static void wlc_ops_update_tbtt(void *ctx, wlc_mcnx_intr_data_t *notif_data);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* module entries */
static int
wlc_ops_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_info_t *wlc = ops_info->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	/* convenience int val for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	switch (actionid) {
	case IOV_GVAL(IOV_OPS_CFG): {
		err = ops_cfg_get(ops_info, params, p_len, arg, len);
		break;
	}
	case IOV_SVAL(IOV_OPS_CFG): {
		err = ops_cfg_set(ops_info, arg, len);
		break;
	}
	case IOV_GVAL(IOV_OPS_STATUS): {
		wl_ops_status_t *status_ptr_in = (wl_ops_status_t *)params;
		wl_ops_status_t *status_ptr = (wl_ops_status_t *)arg;
		uint32 now = OSL_SYSUPTIME();

		/* validate len, version */
		if (p_len < (STRUCT_SIZE_THROUGH(status_ptr_in, version))) {
			return BCME_BUFTOOSHORT;
		}
		if (status_ptr_in->version != WL_OPS_STATUS_VERSION) {
			return BCME_VERSION;
		}

		memset(status_ptr, 0, sizeof(*status_ptr));
		status_ptr->version = WL_OPS_STATUS_VERSION;
		status_ptr->len = (uint16) sizeof(*status_ptr);
		status_ptr->slice_index = (uint8) ops_info->wlc->pub->unit;
		memset(status_ptr->pad, 0, sizeof(status_ptr->pad));

		status_ptr->disable_reasons = ops_info->disable_reqs;
		/* update disable time */
		if (ops_info->disable_reqs) {
			ops_info->disable_duration += (now - ops_info->disable_start_time);
			ops_info->disable_start_time = now;
		}
		status_ptr->disable_duration = ops_info->disable_duration;
		status_ptr->applied_ops_config = ops_info->applied_ops_config;
		status_ptr->disable_obss = ops_info->disable_obss;

		wlc_ops_stats_snapshot(wlc);
		memcpy((uint8 *)&status_ptr->partial_ops_dur,
		       (uint8 *)ops_info->curr_stats, (uint8)sizeof(*ops_info->curr_stats));
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static const char BCMATTACHDATA(rstr_ops)[] = "ops";

wlc_ops_info_t *
BCMATTACHFN(wlc_ops_attach)(wlc_info_t *wlc)
{
	wlc_ops_info_t *ops_info = NULL;
	wl_ops_status_t *status_ptr = NULL;
	int err;

	if ((ops_info = MALLOCZ(wlc->osh, sizeof(wlc_ops_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	ops_info->wlc = wlc;

	ops_info->ops_cfg = WL_OPS_DEFAULT_CFG;
	ops_info->ops_max_sleep_dur = WL_OPS_MAX_SLEEP_DUR;

	/* Memory allocation for maintaining stats */
	if ((ops_info->curr_stats = (wlc_ops_stats_priv_t *)MALLOCZ(wlc->osh,
	                             sizeof(wlc_ops_stats_priv_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	if ((ops_info->prev_stats = (wlc_ops_stats_priv_t *)MALLOCZ(wlc->osh,
	                             sizeof(wlc_ops_stats_priv_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* register wlc module */
	if (wlc_module_register(wlc->pub, ops_iovars, rstr_ops, ops_info,
		wlc_ops_doiovar,  wlc_ops_watchdog,
	    wlc_ops_handle_up_state, wlc_ops_handle_down_state) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	/* register assoc state change notification callback */
	if ((err = wlc_bss_assoc_state_register(wlc,
	                                        (bss_assoc_state_fn_t)wlc_ops_assoc_state_cb,
	                                        ops_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register failed err=%d\n",
		         WLCWLUNIT(wlc), __FUNCTION__, err));
		goto fail;
	}
	/* register disassoc notification callback */
	if ((err = wlc_bss_disassoc_notif_register(wlc,
	                                    (bss_disassoc_notif_fn_t)wlc_ops_disassoc_state_cb,
	                                    ops_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_disassoc_notif_register() failed\n",
		          WLCWLUNIT(wlc), __FUNCTION__));
		goto fail;
	}
	/* register bcn notify */
	if ((err = wlc_bss_rx_bcn_register(wlc, (bss_rx_bcn_notif_fn_t)wlc_ops_rx_bcn_notif,
	                                   ops_info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_rx_bcn_notif_register() failed err=%d\n",
		         WLCWLUNIT(wlc), __FUNCTION__, err));
		goto fail;
	}
	/* register preTBTT callback */
	if ((wlc_mcnx_intr_register(wlc->mcnx, wlc_ops_update_tbtt, ops_info)) != BCME_OK) {
		WL_ERROR(("wl%d: wlc_mcnx_intr_register failed (lq_tbtt)\n",
			wlc->pub->unit));
		goto fail;
	}
	/* sanity check the internal and external stats */
	if ((sizeof(*status_ptr) != (STRUCT_SIZE_THROUGH(status_ptr,
		applied_ops_config) + sizeof(*ops_info->curr_stats)))) {
		WL_ERROR(("wl%d: internal external stats not match %d != %d\n",
			wlc->pub->unit, sizeof(*status_ptr), (STRUCT_SIZE_THROUGH(status_ptr,
			applied_ops_config) + sizeof(*ops_info->curr_stats))));
		goto fail;
	}

	/* Enable OPS */
	wlc->pub->_ops = TRUE;

	/* init with disable due to unassoc */
	ops_info->disable_reqs |= OPS_DISABLED_UNASSOC;
	ops_info->disable_start_time = OSL_SYSUPTIME();

	ops_info->slice_mask = ((1 << MAX_MIMO_MAC_NUM) - 1);

	return ops_info;

fail:
	MODULE_DETACH(ops_info, wlc_ops_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_ops_detach)(wlc_ops_info_t *ops_info)
{
	wlc_info_t *wlc;

	if (ops_info == NULL)
		return;

	wlc = ops_info->wlc;

	/* unregister bcn notify */
	(void)wlc_bss_rx_bcn_unregister(wlc,
	                                (bss_rx_bcn_notif_fn_t)wlc_ops_rx_bcn_notif, ops_info);
	/* unregister assoc state change notification callback */
	(void)wlc_bss_assoc_state_unregister(wlc,
	                                     (bss_assoc_state_fn_t)wlc_ops_assoc_state_cb,
	                                     ops_info);
	/* unregister disassoc notify */
	(void)wlc_bss_disassoc_notif_unregister(wlc,
	                                       (bss_disassoc_notif_fn_t)wlc_ops_disassoc_state_cb,
	                                       ops_info);
	wlc_mcnx_intr_unregister(wlc->mcnx, wlc_ops_update_tbtt, ops_info);

	if (ops_info->curr_stats) {
		MFREE(wlc->osh, ops_info->curr_stats, sizeof(*ops_info->curr_stats));
	}
	if (ops_info->prev_stats) {
		MFREE(wlc->osh, ops_info->prev_stats, sizeof(*ops_info->prev_stats));
	}
	wlc_module_unregister(wlc->pub, rstr_ops, ops_info);
	MFREE(wlc->osh, ops_info, sizeof(wlc_ops_info_t));
}

/* ops_cfg GET iovar */
static int
ops_cfg_get(wlc_ops_info_t *ops_info, void *params, uint p_len,
            void *arg, uint len)
{
	int err = BCME_OK;
	wl_ops_cfg_t ops_cfg;
	wl_ops_cfg_t *ops_cfg_out;
	uint cfg_hdr_len;

	ASSERT(ops_info != NULL);
	ASSERT(ops_info->wlc != NULL);
	ASSERT(params != NULL);
	ASSERT(arg != NULL);

	ops_cfg_out = (wl_ops_cfg_t *)arg;

	cfg_hdr_len = OFFSETOF(wl_ops_cfg_t, data);
	/* verify length */
	if (p_len < cfg_hdr_len) {
		return BCME_BUFTOOSHORT;
	}
	memcpy((uint8 *)&ops_cfg, (uint8 *)params, sizeof(ops_cfg));

	/* verify version */
	if (ops_cfg.version != WL_OPS_CFG_VERSION) {
		return BCME_VERSION;
	}
	/* copy version */
	ops_cfg_out->version = ops_cfg.version;
	/* copy subcommand to output */
	ops_cfg_out->subcmd_id = ops_cfg.subcmd_id;
	ops_cfg_out->padding = 0;

	/* process subcommand */
	switch (ops_cfg.subcmd_id) {
	case WL_OPS_CFG_SUBCMD_ENABLE:
	{
		wl_ops_cfg_enable_t *enable_cfg =
			(wl_ops_cfg_enable_t *)ops_cfg_out->data;
		ops_cfg_out->len = cfg_hdr_len + sizeof(*enable_cfg);
		if (len < ops_cfg_out->len)
			return BCME_BUFTOOSHORT;
		enable_cfg->bits = (ops_info->ops_cfg |
		                    (WL_OPS_SUPPORTED_CFG << WL_OPS_CFG_CAP_SHIFT));
		break;
	}
	case WL_OPS_CFG_SUBCMD_MAX_SLEEP_DUR:
	{
		wl_ops_cfg_max_sleep_dur_t *max_sleep =
			(wl_ops_cfg_max_sleep_dur_t *)ops_cfg_out->data;
		ops_cfg_out->len = cfg_hdr_len + sizeof(*max_sleep);
		if (len < ops_cfg_out->len)
			return BCME_BUFTOOSHORT;
		max_sleep->val = ops_info->ops_max_sleep_dur;
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static int
ops_cfg_set(wlc_ops_info_t *ops_info, void *arg, uint len)
{
	int err = BCME_OK;
	wl_ops_cfg_t *ops_cfg;
	wlc_info_t *wlc;
	uint cfg_hdr_len;

	ASSERT(ops_info != NULL);
	ASSERT(ops_info->wlc != NULL);
	ASSERT(arg != NULL);
	ops_cfg = (wl_ops_cfg_t *)arg;
	wlc = ops_info->wlc;

	cfg_hdr_len = OFFSETOF(wl_ops_cfg_t, data);
	/* verify length */
	if (len < ops_cfg->len) {
		return BCME_BUFTOOSHORT;
	}
	/* verify version */
	if (ops_cfg->version != WL_OPS_CFG_VERSION) {
		return BCME_VERSION;
	}

	/* process subcommand */
	switch (ops_cfg->subcmd_id) {
	case WL_OPS_CFG_SUBCMD_ENABLE:
	{
		wl_ops_cfg_enable_t *enable_cfg =
			(wl_ops_cfg_enable_t *)ops_cfg->data;
		bool bool_val;
		/* verify length */
		if (ops_cfg->len >= cfg_hdr_len + sizeof(*enable_cfg)) {
			if (enable_cfg->bits & ~WL_OPS_SUPPORTED_CFG) {
				err = BCME_BADOPTION;
				break;
			}
			ops_info->ops_cfg = enable_cfg->bits;
			bool_val = (enable_cfg->bits != 0) ? TRUE : FALSE;
			wlc_ops_update_disable_reqs(wlc->ops_info, OPS_DISABLED_HOST, !bool_val);
		} else {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_OPS_CFG_SUBCMD_MAX_SLEEP_DUR:
	{
		wl_ops_cfg_max_sleep_dur_t *max_sleep =
			(wl_ops_cfg_max_sleep_dur_t *)ops_cfg->data;
		/* verify length */
		if (ops_cfg->len >= cfg_hdr_len + sizeof(*max_sleep)) {
			if (max_sleep->val < WL_OPS_MINOF_MAX_SLEEP_DUR ||
				max_sleep->val > WL_OPS_MAX_SLEEP_DUR) {
				err = BCME_RANGE;
				break;
			}
			wlc->ops_info->ops_max_sleep_dur = max_sleep->val;
			if ((wlc->clk && wlc->pub->hw_up)) {
				wlc_bmac_write_shm(wlc->hw, (M_OPS_MAX_LMT(wlc)), max_sleep->val);
			}
		} else {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_OPS_CFG_SUBCMD_RESET_STATS:
	{
		wl_ops_cfg_reset_stats_t *reset_stats =
			(wl_ops_cfg_reset_stats_t *)ops_cfg->data;
		/* verify length */
		if (ops_cfg->len >= cfg_hdr_len + sizeof(*reset_stats)) {
			/* validate bitmap of slices to reset */
			if (reset_stats->val & ~ops_info->slice_mask) {
				err = BCME_BADOPTION;
				break;
			}
			/* all slices */
			if (!reset_stats->val)
				reset_stats->val = ops_info->slice_mask;
			/* reset slice */
			if (reset_stats->val & (1 << wlc_get_wlcindex(wlc))) {
				wlc_ops_stats_snapshot(wlc);
				memset(wlc->ops_info->curr_stats, 0, sizeof(*ops_info->curr_stats));
			}
		} else {
			err = BCME_BADLEN;
		}
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static void
wlc_ops_stats_snapshot_cntr(wlc_info_t *wlc)
{
	wlc_ops_info_t *ops_info = wlc->ops_info;
	wlc_ops_stats_priv_t *prev = ops_info->prev_stats;
	wlc_ops_stats_priv_t *curr = ops_info->curr_stats;
	int i;

	/* read latest counters from shm and update curr and prev cnt */
	WLOPS_CNT16_UPD(curr->nav_cnt, prev->nav_cnt,
		(M_OPS_NAV_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->plcp_cnt, prev->plcp_cnt,
		(M_OPS_PLCP_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->mybss_cnt, prev->mybss_cnt,
		(M_OPS_MYBSS_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->obss_cnt, prev->obss_cnt,
		(M_OPS_OBSS_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->miss_dur_cnt, prev->miss_dur_cnt,
		(M_OPS_MISS_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->miss_premt_cnt, prev->miss_premt_cnt,
		(M_OPS_RSSI_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->max_dur_cnt, prev->max_dur_cnt,
		(M_OPS_MAXLMT_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->wake_cnt, prev->wake_cnt,
		(M_OPS_WAKE_CNT(wlc)), wlc);
	WLOPS_CNT16_UPD(curr->bcn_wait_cnt, prev->bcn_wait_cnt,
		(M_OPS_BCN_CNT(wlc)), wlc);
	for (i = 0; i < OPS_DUR_HIST_BINS; i++) {
		WLOPS_CNT16_UPD(curr->count_dur_hist[i], prev->count_dur_hist[i],
			((M_OPS_HIST(wlc)) + (i * 2)), wlc);
	}

}

static void
wlc_ops_stats_snapshot(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_ops_info_t *ops_info = wlc->ops_info;
	uint32 partial_ops_dur, full_ops_dur;
	wlc_ops_stats_priv_t *prev = ops_info->prev_stats;
	wlc_ops_stats_priv_t *curr = ops_info->curr_stats;

	if (!(wlc->clk && wlc->pub->hw_up))
		return;

	wlc_ops_stats_snapshot_cntr(wlc);

	partial_ops_dur = wlc_bmac_read_counter(wlc_hw, 0, (M_OPS_LIGHT_L(wlc)),
	                                        (M_OPS_LIGHT_H(wlc)));
	full_ops_dur = wlc_bmac_read_counter(wlc_hw, 0, (M_OPS_FULL_L(wlc)),
	                                     (M_OPS_FULL_H(wlc)));

	/* update cummulative count */
	curr->partial_ops_dur += (partial_ops_dur - prev->partial_ops_dur);
	curr->full_ops_dur += (full_ops_dur - prev->full_ops_dur);
	/* update reference for the next time */
	prev->partial_ops_dur = partial_ops_dur;
	prev->full_ops_dur = full_ops_dur;

}

static int
BCMUNINITFN(wlc_ops_handle_down_state)(void *ctx)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_info_t *wlc = ops_info->wlc;

	/* take the current ops stats before going down */
	wlc_ops_stats_snapshot(wlc);
	return BCME_OK;
}

static void
wlc_ops_watchdog(void *ctx)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_info_t *wlc = ops_info->wlc;
	wlc_bsscfg_t *cfg = wlc->primary_bsscfg;

	if (!((BSSCFG_INFRA_STA(cfg)) && cfg->associated)) {
		return;
	}
	/* periodically sample counters every OPS_STATS_UPD_TIME to avoid counter wrap */
	if ((!(wlc->pub->now % OPS_STATS_UPD_TIME))) {
		wlc_ops_stats_snapshot_cntr(wlc);
	}
}

static void
wlc_ops_reset_stats_priv(wlc_info_t *wlc)
{
	wlc_ops_info_t *ops_info = wlc->ops_info;
	memset(ops_info->prev_stats, 0, sizeof(*ops_info->prev_stats));
}

/* callback from wl up to apply ops config
 */
static int
wlc_ops_handle_up_state(void *ctx)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_info_t *wlc = ops_info->wlc;

	WL_OPS_INFO(("wl%d: ops : up state apply ops_cfg 0x%x",
		WLCWLUNIT(wlc), ops_info->ops_cfg));

	wlc_bmac_write_shm(wlc->hw, (M_OPS_MAX_LMT(wlc)), ops_info->ops_max_sleep_dur);

	/* reset wl stats since shm is getting init */
	wlc_ops_reset_stats_priv(wlc);

	/* write ops config to shmem */
	wlc_ops_configure_shmem(ops_info);

	return BCME_OK;
}

static void
wlc_ops_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *notif_data)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_bsscfg_t *cfg;

	ASSERT(ctx != NULL);
	ASSERT(notif_data != NULL);
	ASSERT(notif_data->cfg != NULL);
	ASSERT(notif_data->wrxh != NULL);

	cfg = notif_data->cfg;
	wlc_ops_recv_bcn_process(ops_info, cfg, notif_data->wrxh->rssi);

}

void
wlc_ops_recv_bcn_process(wlc_ops_info_t *ops_info, wlc_bsscfg_t *cfg, int8 rssi)
{
	wlc_info_t *wlc;
	struct scb *scb;
	struct ether_addr *bssid;
	uint32 now = OSL_SYSUPTIME();
	int8 max_rssi;
	bool rssi_diff_chk_pass = FALSE;

	wlc = ops_info->wlc;
	ASSERT(wlc == cfg->wlc);

	if (!(BSSCFG_INFRA_STA(cfg) && cfg->associated))
		return;

	if (rssi == WLC_RSSI_INVALID)
		return;

	/* update last bcn recv time */
	ops_info->last_bcn_time = now;
	ops_info->consecutive_bcn_cnt++;
	ops_info->bcn_pend = FALSE;

	/* check for consecutive target beacons and rssi difference thereof */
	if ((ops_info->consecutive_bcn_cnt >= OPS_CONSECUTIVE_BCN_THRESH)) {
		rssi_diff_chk_pass = (ABS(ops_info->prev_bcn_rssi - rssi) <= OPS_BCN_RSSI_DIFF);
		if (rssi_diff_chk_pass) {
			/* program preemption thresh */
			bssid = &cfg->BSSID;
			if ((scb = wlc_scbfind(wlc, cfg, bssid))) {
				max_rssi = wlc_lq_rssi_max_get(wlc, cfg, scb);
				if (max_rssi != WLC_RSSI_INVALID) {
					wlc_bmac_write_shm(wlc->hw, (M_OPS_RSSI_THRSH(wlc)),
					        (rssi - OPS_PHY_PREEMPTION_THRESH));
					/* enable obss if cfg allows */
					ops_info->disable_obss = FALSE;
					wlc_ops_configure_shmem(ops_info);
				}
			}
		} else {
			ops_info->rssi_diff_chk_fail_cnt++;
		}
	}
	ops_info->prev_bcn_rssi = rssi;

}

static void
wlc_ops_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	wlc_bsscfg_t *cfg;
	bool unassoc_bit_present;

	ASSERT(ctx != NULL);
	ASSERT(notif_data != NULL);
	ASSERT(notif_data->cfg != NULL);
	ASSERT(notif_data->cfg->wlc != NULL);
	cfg = notif_data->cfg;

	if (!((BSSCFG_INFRA_STA(cfg)))) {
		return;
	}

	unassoc_bit_present = !!(OPS_DISABLED_UNASSOC & ops_info->disable_reqs);

	switch (notif_data->state) {
	case AS_IDLE:
	{
		if (ETHER_ISNULLADDR(&cfg->current_bss->BSSID)) {
			ops_info->bcn_pend = FALSE;
			ops_info->disable_obss = FALSE;
			ops_info->consecutive_bcn_cnt = 0;
			/* set unassoc and clear bcn_miss */
			if (!unassoc_bit_present) {
				wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_UNASSOC, TRUE);
			}
		} else if (cfg->associated) {
			if (unassoc_bit_present) {
				wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_UNASSOC, FALSE);
			}
		}
		break;
	}
	case AS_SCAN:
	{
		/* assoc and roam join scan has unassoc bit */
		if (!unassoc_bit_present &&
			(notif_data->type == AS_ASSOCIATION || notif_data->type == AS_ROAM)) {
			wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_UNASSOC, TRUE);
		}
		break;
	}
	case AS_JOIN_START:
	{
		/* since most of the joins are preceded by scan so this maybe just noop */
		if (!unassoc_bit_present) {
			wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_UNASSOC, TRUE);
		}
		break;
	}
	case AS_JOIN_ADOPT:
	{
		/* start with MyBSS OPS after join complete */
		ops_info->disable_obss = TRUE;
		ops_info->consecutive_bcn_cnt = 0;
		break;
	}
	default:
		break;
	}

}

static void
wlc_ops_disassoc_state_cb(void *ctx, bss_disassoc_notif_data_t *notif_data)
{
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;
	bool bit_present;

	ASSERT(ctx != NULL);
	ASSERT(notif_data != NULL);
	ASSERT(notif_data->cfg != NULL);
	ASSERT(notif_data->cfg->wlc != NULL);

	/* set unassoc disable reasons */
	bit_present = !!(OPS_DISABLED_UNASSOC & ops_info->disable_reqs);
	if (!bit_present) {
		wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_UNASSOC, TRUE);
	}
	ops_info->bcn_pend = FALSE;
	ops_info->disable_obss = FALSE;
	ops_info->consecutive_bcn_cnt = 0;
	/* take the current ops stats before disassoc */
	wlc_ops_stats_snapshot(ops_info->wlc);
}

static void wlc_ops_update_tbtt(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_info_t *wlc = notif_data->cfg->wlc;
	wlc_bsscfg_t *cfg = notif_data->cfg;
	wlc_ops_info_t *ops_info = (wlc_ops_info_t *)ctx;

	if (!(BSSCFG_INFRA_STA(cfg) && cfg->associated &&
	     (!ETHER_ISNULLADDR(&cfg->current_bss->BSSID))))
		return;

	if (notif_data->intr != M_P2P_I_PRE_TBTT)
		return;

	/* at tbtt disable obss and enable upon bcn recv after additional checks */
	if (!ops_info->disable_obss) {
		ops_info->disable_obss = TRUE;
		/* reconfig obss cfg */
		wlc_ops_configure_shmem(ops_info);
	}

	/* missed receiving bcn */
	if (ops_info->bcn_pend) {
		ops_info->consecutive_bcn_cnt = 0;
		++ops_info->bcn_miss_cnt;
		WL_OPS_INFO(("wl%d ops tbtt bcn_miss, bcn_pend_time %u last_bcn "
		            "%u bcn_miss_cnt %u applied_cfg 0x%x\n",
		            wlc->pub->unit, ops_info->bcn_pend_time,
		            ops_info->last_bcn_time, ops_info->bcn_miss_cnt,
		            ops_info->applied_ops_config));
	} else {
		ops_info->bcn_pend = TRUE;
		ops_info->bcn_pend_time = OSL_SYSUPTIME();
	}
}

/* disable for scan reason */
void
wlc_ops_update_scan_disable_reqs(wlc_ops_info_t *ops_info, bool disable)
{
	bool bit_present;

	bit_present = !!(OPS_DISABLED_SCAN & ops_info->disable_reqs);
	if ((disable && !bit_present) || (!disable && bit_present)) {
		wlc_ops_update_disable_reqs(ops_info, OPS_DISABLED_SCAN, disable);
	}
}

/* update the disable req and write to the shmem */
static void
wlc_ops_update_disable_reqs(wlc_ops_info_t *ops_info, uint32 req, bool disable)
{
	uint32 prev_disable_reqs = ops_info->disable_reqs;
	uint32 now = OSL_SYSUPTIME();

	if (disable) {
		ops_info->disable_reqs |= req;
	} else {
		ops_info->disable_reqs &= ~req;
	}
	/* transitioning state */
	if (ops_info->disable_reqs && !prev_disable_reqs) {
		ops_info->disable_start_time = now;
	} else if (!ops_info->disable_reqs && prev_disable_reqs) {
		ops_info->disable_duration += (now - ops_info->disable_start_time);
	}
	WL_OPS_INFO(("wl%d, req|disable 0x%x prev/cur_disable 0x%x 0x%x\n",
	            WLCWLUNIT(ops_info->wlc), (req << 8 | disable),
	            prev_disable_reqs, ops_info->disable_reqs));
	wlc_ops_configure_shmem(ops_info);

}

static void
wlc_ops_configure_shmem(wlc_ops_info_t *ops_info)
{
	wlc_info_t *wlc = ops_info->wlc;

	if (!(wlc->clk && wlc->pub->hw_up)) {
		WL_OPS_INFO(("ops_cfg_shmem: wl%d: noclk, skip applying ops_cfg 0x%x: "
		            "disable_res 0x%x \n", WLCWLUNIT(wlc),
		            ops_info->ops_cfg, ops_info->disable_reqs));
		return;
	}
	if (!ops_info->disable_reqs) {
		ops_info->applied_ops_config = ops_info->ops_cfg;
	} else {
		ops_info->applied_ops_config = 0;
	}

	/* maskout obss config from applied cfg */
	if (ops_info->disable_obss) {
		ops_info->applied_ops_config &= ~OPS_CFG_OBSS_MASK;
	}

	wlc_bmac_write_shm(wlc->hw, (M_OPS_MODE(wlc)), ops_info->applied_ops_config);

	WL_OPS_INFO(("wl%d, applied_cfg/cmn_ops_cfg 0x%x/0x%x disable_req 0x%x disable_obss %d\n",
	            WLCWLUNIT(wlc),  ops_info->applied_ops_config,
	            ops_info->ops_cfg, ops_info->disable_reqs, ops_info->disable_obss));
}
