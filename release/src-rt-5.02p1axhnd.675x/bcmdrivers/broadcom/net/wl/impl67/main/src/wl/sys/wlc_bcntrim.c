/*
 * Beacon Trim Functionality module source, 802.1x related.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_bcntrim.c 786648 2020-05-04 14:21:00Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bsscfg.h>
#include <wlc_bcntrim.h>
#include <wlc_bmac.h>
#include <wlc_assoc.h>
#include <wlc_ie_mgmt.h>
#include <wlc_lq.h>
#include <wlc_pm.h>
#include <wlc_hw_priv.h>
#include <wlc_stf.h>
#include <wlc_stf_mimops.h>
#include <wlc_ops.h>

/*
 * iovars
 */
enum {
	IOV_BCNTRIM = 0,
	IOV_BCNTRIM_STATS = 1,
	IOV_BCNTRIM_CFG = 2,
	IOV_BCNTRIM_STATUS = 3,
	IOV_LAST
};

/* iovar table */
static const bcm_iovar_t bcntrim_iovars[] = {
	{"bcntrim", IOV_BCNTRIM, 0, 0, IOVT_INT32, 0},
	{"bcntrim_cfg", IOV_BCNTRIM_CFG, 0, 0, IOVT_BUFFER, OFFSETOF(wl_bcntrim_cfg_t, data)},
	{"bcntrim_stats", IOV_BCNTRIM_STATS, IOVF_GET_CLK, 0,
	IOVT_BUFFER, (BCNTRIM_STATS_MAX*sizeof(uint16))},
	{"bcntrim_status", IOV_BCNTRIM_STATUS, 0, 0,
	IOVT_BUFFER, (BCNTRIM_STATS_MAX*sizeof(uint32) + OFFSETOF(wl_bcntrim_status_t, data))},
	{NULL, 0, 0, 0, 0, 0}
};

/*
 * module private states
 */
struct wlc_bcntrim_info {
	wlc_info_t *wlc;
	int cfgh;
	bool bcntrim_allowed;
	uint32 bcntrim_shmem_addr;
	uint32 chan_mismatch_cnt;
	uint32 invalid_rssi_cnt;
	uint32 *stat_offset_addr;
	uint32 *curr_stats;
	uint16 *prev_stats;
};

/* bsscfg private states */
typedef struct {
	/* Beacon trim feature related vars */
	bool				bcntrim_enabled;
	uint8				bcntrim_threshold;
	struct ether_addr		bcntrim_BSSID;
	uint16				bcntrim_channel;
	uint8				bcntrim_threshold_applied;
	uint16				tsf_drift;	/* tsf drfit config */
	uint32				phyrate_threshold; /* beacon phy rate threshold */
	uint32				bcn_rate_kbps;	/* current beacon rate (kbps) */
	uint32				override_disable_mask; /* disable mask override from host */
	uint32				disable_reqs;	/* current disable requests */
	uint32				disable_duration;	/* disable duration */
	uint32				disable_start_time;
} bcntrim_bsscfg_cubby_t;

typedef struct {
	bool				bcntrim_enabled;
	uint8				bcntrim_threshold;
	uint32				disable_reqs;
	uint32				disable_duration;
	uint32				override_disable_mask;
	uint32				disable_start_time;
	uint32				phyrate_threshold;
	uint16				tsf_drift;
} bcntrim_bsscfg_cubby_copy_t;

/* access macros */
#define BCNTRIM_BSSCFG_CUBBY_LOC(bcntrim, cfg) \
		((bcntrim_bsscfg_cubby_t **)BSSCFG_CUBBY(cfg, (bcntrim)->cfgh))
#define BCNTRIM_BSSCFG_CUBBY(bcntrim, cfg) \
		(*BCNTRIM_BSSCFG_CUBBY_LOC(bcntrim, cfg))

/* Beacon trim feature related definitions */
#define BCNTRIM_TIMEND				(100)	/* bytes */
#define BCNTRIM_TSF_TOLERENCE			(0x900)	/* 2300 usecs */
#define BCNTRIM_MAX_N_VAL			(10)	/* # of expected beacon */
#define BCNTRIM_MACHDR_SZ			(24)	/* bytes */
#define BCNTRIM_TIMIDDIST_TOLERANCE		(20)	/* bytes */
#define BCNTRIM_TIMIE_MINLEN			(5)	/* bytes */
#define C_BCNTRIM_RSSI_VALID			(1 << 15)
#define C_BCNTRIM_RSSI_MASK			(0xFF)
#define BCNTRIM_STATS_UPD_TIME			30	/* periodic stats update time (ms) */

/* Beacon trim feature defaults */
#define	BCNTRIM_DEFAULT_N           3
#define	BCNTRIM_DEFAULT_PHY_RATE    WLC_RATE_6M  /* in 500kbps, i.e., 6mbps */

#define WL_BCNTRIM_INFO(args)	WL_INFORM(args)
#define WL_BCNTRIM_TRACE(args)	WL_TRACE(args)

static int wlc_bcntrim_handle_up_state(void *ctx);
static int wlc_bcntrim_handle_down(void *ctx);
static void wlc_bcntrim_watchdog(void *ctx);
static void wlc_bcntrim_handle_assoc_state(void *ctx, bss_assoc_state_data_t *evt_data);
static void wlc_bcntrim_disassoc_state_cb(void *ctx, bss_disassoc_notif_data_t *notif_data);
static int wlc_bcntrim_parse_tim_ie(void *ctx, wlc_iem_parse_data_t *data);
static void wlc_bcntrim_update_AID(wlc_bsscfg_t *cfg);
static int bcntrim_cfg_get(wlc_bcntrim_info_t *bcntrim, void *params, uint p_len,
                           void *arg, uint len, wlc_bsscfg_t *bsscfg);
static int bcntrim_cfg_set(wlc_bcntrim_info_t *bcntrim,
                           void *arg, uint len, wlc_bsscfg_t *bsscfg);
static int wlc_bcntrim_status_get(wlc_bcntrim_info_t *bcntrim, void *params, uint p_len,
                                  void *arg, uint len, wlc_bsscfg_t *bsscfg);
static void wlc_bcntrim_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *data);
static void wlc_bcntrim_update_phyrate_disable(wlc_bcntrim_info_t *bcntrim, wlc_bsscfg_t *cfg);
static void wlc_bcntrim_update_disable_req(wlc_bcntrim_info_t *bcntrim_info, wlc_bsscfg_t *bsscfg,
                               uint32 req, bool disable);
static void wlc_bcntrim_cleanup(wlc_bcntrim_info_t *bcntrim, wlc_bsscfg_t *cfg,
                                bcntrim_bsscfg_cubby_t *bcntrim_bsscfg);
static void wlc_bcntrim_update_tsf(wlc_info_t * wlc, wlc_bsscfg_t *bsscfg);
#ifdef STA
static int wlc_bcntrim_parse_quiet_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif /* STA */
static void wlc_bcntrim_stats_snapshot(wlc_info_t *wlc);
static int wlc_bcntrim_parse_qbssload_ie(void *ctx, wlc_iem_parse_data_t *data);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static void
wlc_bcntrim_reset_beacon_state(wlc_info_t *wlc)
{
	wlc_bcntrim_info_t *bcntrim = wlc->bcntrim;
	ASSERT(wlc->clk);
	/* Reset Rxed beacon params */
	wlc_bmac_write_shm(wlc->hw,
		bcntrim->bcntrim_shmem_addr + (M_BCNTRIM_RSSI_OFFSET(wlc)), 0);
	wlc_bmac_write_shm(wlc->hw,
		bcntrim->bcntrim_shmem_addr + (M_BCNTRIM_CHAN_OFFSET(wlc)), 0);
	wlc_bmac_write_shm(wlc->hw,
		bcntrim->bcntrim_shmem_addr + (M_BCNTRIM_SNR_OFFSET(wlc)), 0);
}

static void
wlc_bcntrim_configure_shmem(wlc_info_t *wlc)
{
	wlc_bcntrim_info_t *bcntrim = wlc->bcntrim;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	wlc_bsscfg_t *bsscfg = wlc->primary_bsscfg;
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);

	ASSERT(bcntrim_bsscfg);
	bcntrim->bcntrim_shmem_addr = M_BCNTRIM_BLK(wlc);
	wlc_bcntrim_reset_beacon_state(wlc);

	WL_BCNTRIM_INFO(("wl%d, bcntrim_configure_shmem, %p\n",
		wlc->pub->unit, bcntrim->bcntrim_shmem_addr));

	ASSERT(wlc->clk);
	wlc_bmac_write_shm(wlc->hw, bcntrim->bcntrim_shmem_addr
		+(M_BCNTRIM_TIMEND_OFFSET(wlc)), BCNTRIM_TIMEND);
	wlc_bcntrim_update_disable_req(bcntrim, bsscfg, WL_BCNTRIM_DISABLE_HOST,
		!(bcntrim_bsscfg->bcntrim_threshold > 1));
	wlc_bcntrim_update_tsf(wlc, bsscfg);

}

/* module entries */
static int
wlc_bcntrim_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_bcntrim_info_t *bcntrim = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	wlc_bsscfg_t *bsscfg;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_BCNTRIM_CFG): {
		err = bcntrim_cfg_get(bcntrim, params, p_len, arg, len, bsscfg);
		break;
	}
	case IOV_SVAL(IOV_BCNTRIM_CFG): {
		err = bcntrim_cfg_set(bcntrim, arg, len, bsscfg);
		break;
	}
	case IOV_GVAL(IOV_BCNTRIM_STATS): {
			uint16 *ret_params = (uint16 *) arg;
			uint8 i;

			for (i = 0; i < BCNTRIM_STATS_MAX; i++, ret_params++) {
				*ret_params = (uint16) wlc_bmac_read_shm(wlc->hw,
					bcntrim->bcntrim_shmem_addr + bcntrim->stat_offset_addr[i]);
			}
		break;
	}
	case IOV_SVAL(IOV_BCNTRIM_STATS): {
		/* Setting of params not allowed */
		err = BCME_UNSUPPORTED;
		break;
	}
	case IOV_GVAL(IOV_BCNTRIM): {

		/* Read from the wlc_info var, rather than the shmem */
		*ret_int_ptr = bcntrim_bsscfg->bcntrim_threshold;
		WL_BCNTRIM_INFO(("wl%d: Get bcntrim_threshold = %d\n",
		                wlc->pub->unit, bcntrim_bsscfg->bcntrim_threshold));
		break;
	}
	case IOV_SVAL(IOV_BCNTRIM): {
		bool bool_val = (int_val > 1);

		/* Value in range */
		if (int_val < 0 || int_val > BCNTRIM_MAX_N_VAL) {
			err = BCME_RANGE;
			break;
		}

		/* Set the flag if required */
		bcntrim_bsscfg->bcntrim_threshold = int_val;
		bcntrim_bsscfg->bcntrim_enabled = bool_val;
		wlc_bcntrim_update_disable_req(bcntrim, bsscfg,
		                               WL_BCNTRIM_DISABLE_HOST, !bool_val);

		WL_BCNTRIM_DBG(("BCNTRIM: bcntrim_enabled = %d..bcntrim_threshold = %d "
			"...bcntrim_shmem_addr = %x \n", bcntrim_bsscfg->bcntrim_enabled,
			bcntrim_bsscfg->bcntrim_threshold,
			bcntrim->bcntrim_shmem_addr));

		WL_BCNTRIM_INFO(("wl%d: Set bcntrim_threshold = %d\n",
		                wlc->pub->unit, bcntrim_bsscfg->bcntrim_threshold));
		break;
	}
	case IOV_GVAL(IOV_BCNTRIM_STATUS): {
		err = wlc_bcntrim_status_get(bcntrim, params, p_len, arg, len, bsscfg);
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static void
wlc_bcntrim_reset_stats(wlc_info_t *wlc, bool shm_reset)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_bcntrim_info_t *bcntrim = wlc->bcntrim;
	int i;

	/* reset wl stats */
	memset(bcntrim->curr_stats, 0, sizeof(uint32)*BCNTRIM_STATS_MAX);
	memset(bcntrim->prev_stats, 0, sizeof(uint16)*BCNTRIM_STATS_MAX);

	if (shm_reset) {
		/* reset bcntrim shmem */
		for (i = 0; i < BCNTRIM_STATS_MAX; i++) {
			wlc_bmac_write_shm(wlc_hw, (bcntrim->bcntrim_shmem_addr +
			                   bcntrim->stat_offset_addr[i]), 0);
		}
	}
}

void
wlc_bcntrim_stats_snapshot(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_bcntrim_info_t *bcntrim = wlc->bcntrim;
	int i;
	uint16  diff, curr_stat;

	/* read current stats from ucode, save as ref for next time and update curr */
	for (i = 0; i < BCNTRIM_STATS_MAX; i++) {
		curr_stat = (uint16)wlc_bmac_read_shm(wlc_hw,
		                bcntrim->bcntrim_shmem_addr + bcntrim->stat_offset_addr[i]);
		diff = curr_stat - bcntrim->prev_stats[i];
		bcntrim->curr_stats[i] += diff;
		bcntrim->prev_stats[i] = curr_stat;
	}
}

/* returns ucode stats */
static void
wlc_bcntrim_report_stats(wlc_bcntrim_info_t *bcntrim, uint32 *stats, uint8 reset)
{

	wlc_info_t *wlc = bcntrim->wlc;
	int i;

	if (!wlc->clk) {
	/* no need to set mpc and wake since would have snapshot when went down */
	} else {
		wlc_bcntrim_stats_snapshot(wlc);
	}
	for (i = 0; i < BCNTRIM_STATS_MAX; i++) {
		stats[i] += wlc->bcntrim->curr_stats[i];
	}
	/* reset stats */
	if (reset) {
		wlc_bcntrim_reset_stats(wlc, wlc->clk);
	}
}

static int
wlc_bcntrim_status_get(wlc_bcntrim_info_t *bcntrim, void *params, uint p_len,
                           void *arg, uint len, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	wl_bcntrim_status_query_t *query;
	wl_bcntrim_status_t *response;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	uint retlen;
	uint32	stats[BCNTRIM_STATS_MAX];
	uint32 now = OSL_SYSUPTIME();

	ASSERT(bcntrim != NULL);
	ASSERT(params != NULL);
	ASSERT(arg != NULL);

	query = (wl_bcntrim_status_query_t *)params;
	response = (wl_bcntrim_status_t *)arg;

	if (p_len < sizeof(*query)) {
		return BCME_BUFTOOSHORT;
	}
	if (query->version != WL_BCNTRIM_STATUS_VERSION) {
		return BCME_VERSION;
	}
	if (query->len < sizeof(*query)) {
		return BCME_BADLEN;
	}
	retlen = OFFSETOF(wl_bcntrim_status_t, data) + sizeof(uint32)*BCNTRIM_STATS_MAX;
	/* validate length */
	if (len < ROUNDUP(retlen, sizeof(uint32))) {
		return BCME_BUFTOOSHORT;
	}
	memset(stats, 0, sizeof(stats));
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);
	response->version = query->version;
	response->len = retlen;
	response->curr_slice_id = bsscfg->wlc->pub->unit;
	response->fw_status =  bcntrim_bsscfg->disable_reqs;
	response->applied_cfg = bcntrim_bsscfg->bcntrim_threshold_applied;
	memset(response->pad, 0, sizeof(response->pad));

	/* update disable time */
	if (bcntrim_bsscfg->disable_reqs & ~bcntrim_bsscfg->override_disable_mask) {
		bcntrim_bsscfg->disable_duration += (now - bcntrim_bsscfg->disable_start_time);
		bcntrim_bsscfg->disable_start_time = now;
	}
	response->total_disable_dur = bcntrim_bsscfg->disable_duration;

	/* collect stats from each slice, sum up and report */
	wlc_bcntrim_report_stats(bcntrim, stats, query->reset);

	memcpy(response->data, stats, sizeof(uint32)*BCNTRIM_STATS_MAX);

	if (bcntrim->wlc->clk) {
		uint16 tim_end;
		tim_end = wlc_bmac_read_shm(bcntrim->wlc->hw,
		            bcntrim->bcntrim_shmem_addr +
		            (M_BCNTRIM_TIMEND_OFFSET(bcntrim->wlc)));
		WL_BCNTRIM_INFO(("wl%d, tim_end: %u\n", bcntrim->wlc->pub->unit, tim_end));
	}

	return err;
}

/* bcntrim_cfg GET iovar */
static int
bcntrim_cfg_get(wlc_bcntrim_info_t *bcntrim, void *params, uint p_len,
                           void *arg, uint len, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	wl_bcntrim_cfg_t *bcntrim_cfg;
	wl_bcntrim_cfg_t *bcntrim_cfg_out;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	uint cfg_hdr_len;

	ASSERT(bcntrim != NULL);
	ASSERT(params != NULL);
	ASSERT(arg != NULL);
	bcntrim_cfg = (wl_bcntrim_cfg_t *)params;
	bcntrim_cfg_out = (wl_bcntrim_cfg_t *)arg;

	cfg_hdr_len = OFFSETOF(wl_bcntrim_cfg_t, data);

	/* verify length */
	if (p_len < cfg_hdr_len) {
		return BCME_BUFTOOSHORT;
	}
	/* verify version */
	if (bcntrim_cfg->version != WL_BCNTRIM_CFG_VERSION) {
		return BCME_VERSION;
	}

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);
	/* copy version */
	bcntrim_cfg_out->version = bcntrim_cfg->version;
	/* copy subcommand to output */
	bcntrim_cfg_out->subcmd_id = bcntrim_cfg->subcmd_id;
	bcntrim_cfg_out->pad = 0;

	/* process subcommand */
	switch (bcntrim_cfg->subcmd_id) {
	case WL_BCNTRIM_CFG_SUBCMD_PHY_RATE_THRESH:
	{
		wl_bcntrim_cfg_phy_rate_thresh_t *phy_thresh =
			(wl_bcntrim_cfg_phy_rate_thresh_t *)bcntrim_cfg_out->data;
		bcntrim_cfg_out->len = cfg_hdr_len + sizeof(*phy_thresh);
		if (len <  bcntrim_cfg_out->len)
			return BCME_BUFTOOSHORT;
		phy_thresh->rate = bcntrim_bsscfg->phyrate_threshold;
		break;
	}
	case WL_BCNTRIM_CFG_SUBCMD_OVERRIDE_DISABLE_MASK:
	{
		wl_bcntrim_cfg_override_disable_mask_t *override_disable =
			(wl_bcntrim_cfg_override_disable_mask_t *)bcntrim_cfg_out->data;
		bcntrim_cfg_out->len = cfg_hdr_len + sizeof(*override_disable);
		if (len <  bcntrim_cfg_out->len)
			return BCME_BUFTOOSHORT;
		override_disable->mask = bcntrim_bsscfg->override_disable_mask;
		break;
	}
	case WL_BCNTRIM_CFG_SUBCMD_TSF_DRIFT_LIMIT:
	{
		wl_bcntrim_cfg_tsf_drift_limit_t *bcntrim_tsf =
			(wl_bcntrim_cfg_tsf_drift_limit_t *)bcntrim_cfg_out->data;
		bcntrim_cfg_out->len = cfg_hdr_len + sizeof(*bcntrim_tsf);
		if (len <  bcntrim_cfg_out->len)
			return BCME_BUFTOOSHORT;
		bcntrim_tsf->drift =  bcntrim_bsscfg->tsf_drift;
		memset(bcntrim_tsf->pad, 0, sizeof(bcntrim_tsf->pad));
		break;
	}
	default:
		ASSERT(0);
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static int
bcntrim_cfg_set(wlc_bcntrim_info_t *bcntrim, void *arg, uint len, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	wl_bcntrim_cfg_t *bcntrim_cfg;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	uint cfg_hdr_len;

	ASSERT(bcntrim != NULL);
	ASSERT(arg != NULL);
	ASSERT(bsscfg->wlc);

	bcntrim_cfg = (wl_bcntrim_cfg_t *)arg;

	cfg_hdr_len = OFFSETOF(wl_bcntrim_cfg_t, data);
	/* verify length */
	if (len < bcntrim_cfg->len) {
		return BCME_BUFTOOSHORT;
	}
	/* verify versio */
	if (bcntrim_cfg->version != WL_BCNTRIM_CFG_VERSION) {
		return BCME_VERSION;
	}
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);

	/* process subcommand */
	switch (bcntrim_cfg->subcmd_id) {
	case WL_BCNTRIM_CFG_SUBCMD_PHY_RATE_THRESH:
	{
		wl_bcntrim_cfg_phy_rate_thresh_t *phy_thresh =
			(wl_bcntrim_cfg_phy_rate_thresh_t *)bcntrim_cfg->data;
		/* verify length */
		if (bcntrim_cfg->len >= cfg_hdr_len + sizeof(*phy_thresh)) {
			/* Value in range */
			if (phy_thresh->rate > BCNTRIM_MAX_PHY_RATE) {
				err = BCME_RANGE;
				break;
			}
			bcntrim_bsscfg->phyrate_threshold = phy_thresh->rate;
			wlc_bcntrim_update_phyrate_disable(bcntrim, bsscfg);
		} else {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_BCNTRIM_CFG_SUBCMD_OVERRIDE_DISABLE_MASK:
	{
		wl_bcntrim_cfg_override_disable_mask_t *override_disable =
			(wl_bcntrim_cfg_override_disable_mask_t *)bcntrim_cfg->data;
		uint32 disable_reqs;
		bool prev_disable, curr_disable;

		/* verify length */
		if (bcntrim_cfg->len < cfg_hdr_len + sizeof(*override_disable)) {
			err = BCME_BADLEN;
			break;
		}
		/* verify mask */
		if (override_disable->mask & ~WL_BCNTRIM_OVERRIDE_DISABLE_MASK) {
			err = BCME_EPERM;
			break;
		}
		prev_disable = !!(bcntrim_bsscfg->disable_reqs &
		                  ~bcntrim_bsscfg->override_disable_mask);
		curr_disable = !!(bcntrim_bsscfg->disable_reqs &
		                  ~override_disable->mask);

		WL_BCNTRIM_INFO(("wl%d: prev_curr: disable %d/%d, mask 0x%x/0x%x\n",
		                bsscfg->wlc->pub->unit, prev_disable, curr_disable,
		                bcntrim_bsscfg->override_disable_mask,
		                override_disable->mask));

		/* override mask changes effective dsable */
		if ((prev_disable != curr_disable)) {
			/* update disable duration if now enable */
			if (prev_disable && !curr_disable) {
				bcntrim_bsscfg->disable_duration += (OSL_SYSUPTIME()
					- bcntrim_bsscfg->disable_start_time);
			}
			/* reapply active disable reqs in the override mask */
			bcntrim_bsscfg->override_disable_mask = override_disable->mask;
			disable_reqs = bcntrim_bsscfg->disable_reqs
				& WL_BCNTRIM_OVERRIDE_DISABLE_MASK;
			bcntrim_bsscfg->disable_reqs &= ~WL_BCNTRIM_OVERRIDE_DISABLE_MASK;
			wlc_bcntrim_update_disable_req(bcntrim, bsscfg, disable_reqs, TRUE);
		} else {
			bcntrim_bsscfg->override_disable_mask = override_disable->mask;
		}
		break;
	}
	case WL_BCNTRIM_CFG_SUBCMD_TSF_DRIFT_LIMIT:
	{
		wl_bcntrim_cfg_tsf_drift_limit_t *bcntrim_tsf =
			(wl_bcntrim_cfg_tsf_drift_limit_t *)bcntrim_cfg->data;
		/* verify length */
		if (bcntrim_cfg->len >= cfg_hdr_len + sizeof(*bcntrim_tsf)) {
			bcntrim_bsscfg->tsf_drift = bcntrim_tsf->drift;
			/* update tsf */
			wlc_bcntrim_update_tsf(bsscfg->wlc, bsscfg);
		} else {
			err = BCME_BADLEN;
		}
		break;
	}
	default:
		ASSERT(0);
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

/* bsscfg cubby */
static int
wlc_bcntrim_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;
	bcntrim_bsscfg_cubby_t **pbcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY_LOC(bcntrim_info, cfg);
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = NULL;

	/* allocate memory and point bsscfg cubby to it */
	if ((bcntrim_bsscfg = MALLOCZ(wlc->osh, sizeof(bcntrim_bsscfg_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	*pbcntrim_bsscfg = bcntrim_bsscfg;
	/* default init */
	bcntrim_bsscfg->bcntrim_threshold = BCNTRIM_DEFAULT_N;
	bcntrim_bsscfg->tsf_drift = BCNTRIM_TSF_TOLERENCE;
	bcntrim_bsscfg->phyrate_threshold = BCNTRIM_DEFAULT_PHY_RATE;

	return BCME_OK;

}

static void
wlc_bcntrim_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;
	bcntrim_bsscfg_cubby_t **pbcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY_LOC(bcntrim_info, cfg);
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = NULL;

	/* free the Cubby reserve allocated memory  */
	bcntrim_bsscfg = *pbcntrim_bsscfg;
	if (bcntrim_bsscfg) {
		MFREE(wlc->osh, bcntrim_bsscfg, sizeof(bcntrim_bsscfg_cubby_t));
		*pbcntrim_bsscfg = NULL;
	}
}

/* build static stats offset table for reading stats */
static void
wlc_bcntrim_init_stat_offset(wlc_bcntrim_info_t *bcntrim_info)
{
	wlc_info_t *wlc = bcntrim_info->wlc;
	const uint32 stat_offset_addr[] =
		{M_BCNTRIM_CANTRIM_OFFSET(wlc), M_BCNTRIM_CNT_OFFSET(wlc),
		M_BCNTRIM_RXMBSS_OFFSET(wlc), M_BCNTRIM_TIMNOTFOUND_OFFSET(wlc),
		M_BCNTRIM_LENCHG_OFFSET(wlc), M_BCNTRIM_TSFDRF_OFFSET(wlc),
		M_BCNTRIM_TIMBITSET_OFFSET(wlc), M_BCNTRIM_WAKE_OFFSET(wlc),
		M_BCNTRIM_SSID_OFFSET(wlc), M_BCNTRIM_DTIM_OFFSET(wlc)};
	uint num_of_stats = ARRAYSIZE(stat_offset_addr);

	ASSERT(BCNTRIM_STATS_MAX == num_of_stats);
	memcpy(bcntrim_info->stat_offset_addr, stat_offset_addr, (sizeof(uint32)*num_of_stats));

}

static const char BCMATTACHDATA(rstr_bcntrim)[] = "bcntrim";
wlc_bcntrim_info_t *
BCMATTACHFN(wlc_bcntrim_attach)(wlc_info_t *wlc)
{
	wlc_bcntrim_info_t *bcntrim_info;
	bsscfg_cubby_params_t cubby_params;

	if ((bcntrim_info = MALLOCZ(wlc->osh, sizeof(wlc_bcntrim_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bcntrim_info->wlc = wlc;

	/* Memory allocation for stats offset address array */
	if ((bcntrim_info->stat_offset_addr = (uint32 *)MALLOCZ(wlc->osh,
	                       sizeof(uint32)*BCNTRIM_STATS_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	/* Memory allocation for maintaining stats */
	if ((bcntrim_info->curr_stats = (uint32 *)MALLOCZ(wlc->osh,
	                       sizeof(uint32)*BCNTRIM_STATS_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	if ((bcntrim_info->prev_stats = (uint16 *)MALLOCZ(wlc->osh,
	                       sizeof(uint16)*BCNTRIM_STATS_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	bzero(&cubby_params, sizeof(cubby_params));
	cubby_params.context = bcntrim_info;
	cubby_params.fn_init = wlc_bcntrim_bsscfg_init;
	cubby_params.fn_deinit = wlc_bcntrim_bsscfg_deinit;
	cubby_params.fn_dump = NULL;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	bcntrim_info->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(bcntrim_bsscfg_cubby_t *),
		&cubby_params);
	if (bcntrim_info->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* assoc join-start/done callback */
	if (wlc_bss_assoc_state_register(wlc,
		(bss_assoc_state_fn_t)wlc_bcntrim_handle_assoc_state,
		bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* register disassoc notification callback */
	if (wlc_bss_disassoc_notif_register(wlc,
		(bss_disassoc_notif_fn_t)wlc_bcntrim_disassoc_state_cb,
		bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_disassoc_notif_register() failed\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* register wlc module */
	if (wlc_module_register(wlc->pub, bcntrim_iovars, rstr_bcntrim, bcntrim_info,
	                        wlc_bcntrim_doiovar, wlc_bcntrim_watchdog,
	                        wlc_bcntrim_handle_up_state,
	                        wlc_bcntrim_handle_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};
	/* register bcn notify */
	if (wlc_bss_rx_bcn_register(wlc, (bss_rx_bcn_notif_fn_t)wlc_bcntrim_rx_bcn_notif,
	                            bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_rx_bcn_notif_register() failed\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef STA
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_TIM_ID,
		wlc_bcntrim_parse_tim_ie, bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, tim ie in bcn\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_QUIET_ID,
	                         wlc_bcntrim_parse_quiet_ie, bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, quiet in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* STA */

	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_QBSS_LOAD_ID,
	                         wlc_bcntrim_parse_qbssload_ie, bcntrim_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, qbss in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Enable in pub and turn on interrupt */
	wlc->pub->_bcntrim = TRUE;
	wlc->hw->defmacintmask |= MI_BCNTRIM_RX;

	wlc_bcntrim_init_stat_offset(bcntrim_info);

	return bcntrim_info;

fail:
	MODULE_DETACH(bcntrim_info, wlc_bcntrim_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_bcntrim_detach)(wlc_bcntrim_info_t *bcntrim_info)
{
	wlc_info_t *wlc;

	if (bcntrim_info == NULL)
		return;

	wlc = bcntrim_info->wlc;
	/* unregister bcn notify */
	wlc_bss_rx_bcn_unregister(wlc, (bss_rx_bcn_notif_fn_t)wlc_bcntrim_rx_bcn_notif,
	                          bcntrim_info);
	/* unregister assoc state update callbacks */
	wlc_bss_assoc_state_unregister(wlc, wlc_bcntrim_handle_assoc_state, bcntrim_info);
	(void)wlc_bss_disassoc_notif_unregister(wlc,
		(bss_disassoc_notif_fn_t)wlc_bcntrim_disassoc_state_cb, bcntrim_info);

	wlc_module_unregister(wlc->pub, rstr_bcntrim, bcntrim_info);

	if (bcntrim_info->stat_offset_addr) {
		MFREE(wlc->osh, bcntrim_info->stat_offset_addr, sizeof(uint32)*BCNTRIM_STATS_MAX);
	}
	if (bcntrim_info->curr_stats) {
		MFREE(wlc->osh, bcntrim_info->curr_stats, sizeof(uint32)*BCNTRIM_STATS_MAX);
	}
	if (bcntrim_info->prev_stats) {
		MFREE(wlc->osh, bcntrim_info->prev_stats, sizeof(uint16)*BCNTRIM_STATS_MAX);
	}

	MFREE(wlc->osh, bcntrim_info, sizeof(wlc_bcntrim_info_t));
}

static void
wlc_bcntrim_reset_refstats(wlc_info_t *wlc)
{
	wlc_bcntrim_info_t *bcntrim_info = wlc->bcntrim;
	/* reset refernce counters as SHM is getting reset on up */
	memset(bcntrim_info->prev_stats, 0, sizeof(uint16)*BCNTRIM_STATS_MAX);

}

/*  This will be called during wl up functionality. */
static int
wlc_bcntrim_handle_up_state(void *ctx)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;

	wlc_bcntrim_configure_shmem(wlc);
	wlc_bcntrim_reset_refstats(wlc);

	return BCME_OK;
}

static int
BCMUNINITFN(wlc_bcntrim_handle_down)(void *ctx)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;

	/* take the current bcntrim stats before going down */
	wlc_bcntrim_stats_snapshot(wlc);
	return BCME_OK;
}

static void
wlc_bcntrim_watchdog(void *ctx)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;
	wlc_bsscfg_t *cfg = wlc->primary_bsscfg;

	if (!((BSSCFG_INFRA_STA(cfg)) && cfg->associated)) {
		return;
	}
	/* periodically snapshot every BCNTRIM_STATS_UPD_TIME to avoid counter wrap */
	if ((!(wlc->pub->now % BCNTRIM_STATS_UPD_TIME))) {
		wlc_bcntrim_stats_snapshot(wlc);
	}

}

static void wlc_bcntrim_update_AID(wlc_bsscfg_t *cfg)
{
	wlc_info_t * wlc = cfg->wlc;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg =
		BCNTRIM_BSSCFG_CUBBY(wlc->bcntrim, cfg);
	wlc_bss_info_t *target_bss = cfg->target_bss;

	/* update bcntrim state in shmem */
	wlc_bcntrim_update_disable_req(wlc->bcntrim, cfg, WL_BCNTRIM_DISABLE_HOST,
	                               !(bcntrim_bsscfg->bcntrim_threshold > 1));
	/* update bcntrim tsf drift config */
	wlc_bcntrim_update_tsf(wlc, cfg);

	/* Write AID to the Shmem and note the BSSID and channel */
	wlc_bmac_write_shm(wlc->hw, M_AID_NBIT(wlc), cfg->AID);
	memcpy(&bcntrim_bsscfg->bcntrim_BSSID, &target_bss->BSSID,
		sizeof(struct ether_addr));
	bcntrim_bsscfg->bcntrim_channel = CHSPEC_CHANNEL(target_bss->chanspec);
}

/* This function is used to update  M_AID_NBIT , bcntrim_BSSID &
*  bcntrim_channel value
*  This will be called after successful association
*/
static void
wlc_bcntrim_handle_assoc_state(void *ctx, bss_assoc_state_data_t *evt_data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg = evt_data->cfg;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	uint16 ssidu16;
	uint8 ssidlen;
	uint8 ssidbuf[DOT11_MAX_SSID_LEN];
	uint ssid_blk_addr;
	uint offset, i;

	ASSERT(ctx != NULL);
	ASSERT(evt_data != NULL);
	ASSERT(evt_data->cfg != NULL);
	wlc = bcntrim_info->wlc;
	cfg = evt_data->cfg;

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, cfg);
	ASSERT(bcntrim_bsscfg);

	if (!((BSSCFG_INFRA_STA(cfg)))) {
		WL_BCNTRIM_INFO(("wl%d, handle_assoc_state: %d infra sta %d\n",
		                wlc->pub->unit, evt_data->state, (BSSCFG_INFRA_STA(cfg))));
		return;
	}

	if (evt_data->state == AS_JOIN_ADOPT) {
		memset(ssidbuf, 0, sizeof(ssidbuf));
		ssidlen = evt_data->cfg->target_bss->SSID_len;
		memcpy(ssidbuf, evt_data->cfg->target_bss->SSID, ssidlen);

		ssid_blk_addr = M_BCNTRIM_SSIDBLK_OFFSET(wlc);

		/* updated SSID TLV in shmem */
		wlc_bmac_write_shm(wlc->hw, (bcntrim_info->bcntrim_shmem_addr + ssid_blk_addr),
			(ssidlen << 8 | DOT11_MNG_SSID_ID));
		offset = 2;
		for (i = 0; i < DOT11_MAX_SSID_LEN; i += 2) {
			ssidu16 = (ssidbuf[i+1] << 8 | ssidbuf[i]);
			wlc_bmac_write_shm(wlc->hw,
				(bcntrim_info->bcntrim_shmem_addr +
			     ssid_blk_addr + offset + i), ssidu16);
		}

		WL_BCNTRIM_DBG(("%s: bssid = %x:%x chanspec = 0x%x\n",
			__FUNCTION__,
			cfg->target_bss->BSSID.octet[4],
			cfg->target_bss->BSSID.octet[5],
			cfg->target_bss->chanspec));

		WL_BCNTRIM_INFO(("wl%d, bcntrim handle_assoc_state (join_adopt): disable_req 0x%x "
			"bssid = %x:%x chanspec = 0x%x\n", wlc->pub->unit,
			bcntrim_bsscfg->disable_reqs,
			cfg->target_bss->BSSID.octet[4],
			cfg->target_bss->BSSID.octet[5],
			cfg->target_bss->chanspec));
		wlc_bcntrim_update_AID(cfg);
	} else if (evt_data->state == AS_IDLE && ETHER_ISNULLADDR(&cfg->current_bss->BSSID)) {
		wlc_bcntrim_cleanup(bcntrim_info, cfg, bcntrim_bsscfg);
	}
}

static void
wlc_bcntrim_disassoc_state_cb(void *ctx, bss_disassoc_notif_data_t *notif_data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_bsscfg_t *cfg;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;

	ASSERT(ctx != NULL);
	ASSERT(notif_data != NULL);
	ASSERT(notif_data->cfg != NULL);

	cfg = notif_data->cfg;
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, cfg);
	ASSERT(bcntrim_bsscfg);

	wlc_bcntrim_cleanup(bcntrim_info, cfg, bcntrim_bsscfg);

	/* snapshot bcntrim stats as sampling occurs watchdog modulo */
	wlc_bcntrim_stats_snapshot(cfg->wlc);

}

/* This function will enable bcntrim functionality in SHM
*  This will be called after successful association
*/
void
wlc_bcntrim_update_bcntrim_enab(wlc_bcntrim_info_t *bcntrim_info, wlc_bsscfg_t * bsscfg,
uint32 new_mc)
{
	wlc_info_t *wlc = bcntrim_info->wlc;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(wlc->bcntrim, bsscfg);

	WL_BCNTRIM_DBG(("%s: bcntrim_enabled = %d new_mc = %d \n", __FUNCTION__,
		bcntrim_bsscfg->bcntrim_enabled, new_mc));

	if (bcntrim_bsscfg->bcntrim_enabled) {
		if (new_mc) {
			/* Enable the beacon trimming in shmem */
			wlc_bmac_write_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr +
				(M_BCNTRIM_PER_OFFSET(wlc)), bcntrim_bsscfg->bcntrim_threshold);
		} else {
			/* Disable the beacon trimming in shmem */
			wlc_bmac_write_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr +
				(M_BCNTRIM_PER_OFFSET(wlc)),	0);
		}
	}
}

/* This function is used to update TIM distance to M_BCNTRIM_TIMEND
*  SHM address
*  This will be called from wlc_bcn_parse_tim_ie
*/
static int
wlc_bcntrim_parse_tim_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	wlc_info_t *wlc = bcntrim_info->wlc;
	wlc_bsscfg_t *cfg;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	dot11_tim_ie_t *tim_ie;
	uint8* body;
	uint16 tim_end, tim_len;

	if (data == NULL)
		return BCME_ERROR;
	cfg = data->cfg;
	if (cfg == NULL)
		return BCME_ERROR;

	tim_ie = (dot11_tim_ie_t*)data->ie;
	body = data->pparm->body;
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, cfg);

	if ((tim_ie == NULL)||(body == NULL)||(bcntrim_bsscfg == NULL))
		return BCME_ERROR;

	WL_BCNTRIM_DBG(("%s: bcntrim_enabled = %d PM = %d \n", __FUNCTION__,
		bcntrim_bsscfg->bcntrim_enabled,
		cfg->pm->PM_override));

	WL_BCNTRIM_TRACE(("wl%d:bcntrim_parse_tim, dis_reqs 0x%x, applied %d PM=%d\n",
	                 wlc->pub->unit,
	                 bcntrim_bsscfg->disable_reqs,
	                 bcntrim_bsscfg->bcntrim_threshold_applied,
	                 cfg->pm->PM_override));

	if ((bcntrim_bsscfg->bcntrim_threshold_applied > 1) &&
		(cfg->pm->PM_override == FALSE)) {

		/* Compute the max distance to end of TIM */
		tim_len = CEIL(cfg->AID, 8);
		if ((tim_len % 8) == 0)
			tim_len++;
		tim_end = ((uint8 *) tim_ie - (uint8 *) body) + tim_len +
			BCNTRIM_TIMIE_MINLEN + BCNTRIM_MACHDR_SZ +
			BCNTRIM_TIMIDDIST_TOLERANCE;	/* bytes */

		/* Write to TIM distance */
		wlc_bmac_write_shm(wlc->hw,
			bcntrim_info->bcntrim_shmem_addr + (M_BCNTRIM_TIMEND_OFFSET(wlc)), tim_end);

		WL_BCNTRIM_DBG(("%s: tim_len= %d tim_end = %d \n", __FUNCTION__,
			tim_len, tim_end));
		WL_BCNTRIM_TRACE(("\ttim_len= %d tim_end = %d \n", tim_len, tim_end));
	}
	return BCME_OK;
}

/* This function is used to process partial beacon and update
*  SM variables properly
*  This will be called from wlc_high_dpc function
*/
void
wlc_bcntrim_recv_process_partial_beacon(wlc_bcntrim_info_t *bcntrim_info)
{
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_roam_t *roam = NULL;
	uint16 rssi_word, chan_num;
	int8 rssi;
#ifdef BCMDBG
	uint16 counter;
#endif // endif
	wlc_info_t *wlc = bcntrim_info->wlc;
	/* this works for primary bss being infra, case for multi infras ? */
	bsscfg = wlc->primary_bsscfg;

	/* Make sure device is STA and associated */
	ASSERT(BSSCFG_STA(bsscfg));
	if (!bsscfg->associated)
		return;
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, bsscfg);
	ASSERT(bcntrim_bsscfg);

	WL_BCNTRIM_DBG(("%s: bcntrim_enabled = %d \n", __FUNCTION__,
		bcntrim_bsscfg->bcntrim_enabled));

	WL_BCNTRIM_TRACE(("wl%d:bcntrim_recv_partial_bcn, applied %d dis_req 0x%x",
	                 wlc->pub->unit,
	                 bcntrim_bsscfg->bcntrim_threshold_applied,
	                 bcntrim_bsscfg->disable_reqs));

	if (!(bcntrim_bsscfg->bcntrim_threshold_applied > 1))
		return;

	/* This feature should not be enabled along with the following */
	ASSERT(!MCHAN_ENAB(wlc->pub));
	ASSERT(!AP_ACTIVE(wlc));
	ASSERT(!CCX_ENAB(wlc->pub));

	/* Read & reset the RSSI in shmem */
	rssi_word = wlc_bmac_read_shm(wlc->hw,
		bcntrim_info->bcntrim_shmem_addr + (M_BCNTRIM_RSSI_OFFSET(wlc)));
	wlc_bmac_write_shm(wlc->hw,
		bcntrim_info->bcntrim_shmem_addr + (M_BCNTRIM_RSSI_OFFSET(wlc)), 0);

	/* Read the Channel from Shmem */
	chan_num = wlc_bmac_read_shm(wlc->hw,
		bcntrim_info->bcntrim_shmem_addr + (M_BCNTRIM_CHAN_OFFSET(wlc)));

	rssi = rssi_word & C_BCNTRIM_RSSI_MASK;

	WL_BCNTRIM_TRACE((" rssi_valid %d rssi %d chan_num 0x%x bcntrim_chan %d\n",
	                 !!(rssi_word & C_BCNTRIM_RSSI_VALID),
	                 rssi, chan_num, bcntrim_bsscfg->bcntrim_channel));

	/* Quit if RSSI has not been updated */
	if (!(rssi_word & C_BCNTRIM_RSSI_VALID)) {
		bcntrim_info->invalid_rssi_cnt++;
		return;
	}

	if (CHSPEC_CHANNEL(chan_num) == bcntrim_bsscfg->bcntrim_channel) {
		roam = bsscfg->roam;
	} else {
		bcntrim_info->chan_mismatch_cnt++;
		return;
	}

#ifdef BCMDBG
	counter = wlc_bmac_read_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr +
		(M_BCNTRIM_CNT_OFFSET(wlc)));
	WL_BCNTRIM_DBG(("P = %d 0x%x %d\n", counter, rssi, chan_num));
#endif // endif

	if (BSSCFG_STA(bsscfg) && bsscfg->BSS) {
		wlc_lq_rssi_bss_sta_ma_upd_bcntrim(wlc, bsscfg,
			CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec), rssi, 0);
		wlc_lq_rssi_bss_sta_event_upd(wlc, bsscfg);
		bsscfg->current_bss->RSSI = bsscfg->link->rssi;

		/* update rssi */
#ifdef OCL
		if (OCL_ENAB(wlc->pub)) {
			wlc_stf_ocl_rssi_thresh_handling(bsscfg);
		}
#endif // endif
#ifdef WL_MIMOPS_CFG
		if (WLC_MIMOPS_ENAB(wlc->pub)) {
			wlc_stf_mrc_thresh_handling(bsscfg);
		}
#endif // endif
		if (WL_OPS_ENAB(wlc->pub)) {
			wlc_ops_recv_bcn_process(wlc->ops_info, bsscfg, rssi);
		}

		if (TRUE &&
#ifdef WLP2P
		    !BSS_P2P_ENAB(wlc, bsscfg) &&
#endif // endif
		    (!WLC_ROAM_DISABLED(roam))) {
			wlc_roamscan_start(bsscfg, WLC_E_REASON_LOW_RSSI);
		}
	}

	/* Clear out the interval count */
	roam->bcn_interval_cnt = 0;

	roam->time_since_bcn = 0;
#ifdef BCMDBG
	roam->tbtt_since_bcn = 0;
#endif // endif
	wlc_assoc_homech_req_update(bsscfg);

}

static void
wlc_bcntrim_cleanup(wlc_bcntrim_info_t *bcntrim, wlc_bsscfg_t *cfg,
                    bcntrim_bsscfg_cubby_t *bcntrim_bsscfg)
{

	uint32 reqs = (WL_BCNTRIM_DISABLE_PHY_RATE |
	               WL_BCNTRIM_DISABLE_QUIET_IE | WL_BCNTRIM_DISABLE_QBSSLOAD_IE);
	wlc_bcntrim_update_disable_req(bcntrim, cfg, reqs, FALSE);
}

static void
wlc_bcntrim_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	bss_rx_bcn_notif_data_t *bcn = data;
	ratespec_t rspec;
	uint32 bcn_rate_kbps;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;

	ASSERT(bcntrim_info);
	ASSERT(bcntrim_info->wlc);
	ASSERT(bcn->cfg);
	ASSERT(bcn->wrxh);
	ASSERT(bcn->plcp);

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, bcn->cfg);
	ASSERT(bcntrim_bsscfg);
	rspec = wlc_recv_compute_rspec(bcntrim_info->wlc->pub->corerev, &bcn->wrxh->rxhdr,
		bcn->plcp);
	bcn_rate_kbps = RSPEC2KBPS(rspec);

	if (BSSCFG_STA(bcn->cfg) &&
	    bcn->cfg->BSS &&
	    bcn->cfg->assoc->state == AS_WAIT_RCV_BCN) {
		WL_BCNTRIM_INFO(("wl%d, bcntrim: assoc bcn recv: update_phyrate: %d\n",
		                bcntrim_info->wlc->pub->unit, bcn_rate_kbps));
		bcntrim_bsscfg->bcn_rate_kbps = bcn_rate_kbps;
	}

	if (bcntrim_bsscfg->bcn_rate_kbps != bcn_rate_kbps) {
		WL_BCNTRIM_INFO(("wl%d, bcntrim: bcn rate change after assoc %d -> %d\n",
		                bcntrim_info->wlc->pub->unit, bcntrim_bsscfg->bcn_rate_kbps,
		                bcn_rate_kbps));
		bcntrim_bsscfg->bcn_rate_kbps = bcn_rate_kbps;
	}
	wlc_bcntrim_update_phyrate_disable(bcntrim_info, bcn->cfg);

}

static void
wlc_bcntrim_update_phyrate_disable(wlc_bcntrim_info_t *bcntrim, wlc_bsscfg_t *cfg)
{
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, cfg);
	wlc_info_t *wlc = bcntrim->wlc;
	bool disable, bit_present;

	ASSERT(bcntrim_bsscfg);
	if (!((BSSCFG_INFRA_STA(cfg)) && cfg->associated)) {
		WL_BCNTRIM_INFO(("wl%d, update_phyrate: disable_req 0x%x infra sta %d\n",
		                wlc->pub->unit, bcntrim_bsscfg->disable_reqs,
		                (BSSCFG_INFRA_STA(cfg))));
		return;
	}

	if (!bcntrim_bsscfg->phyrate_threshold ||
	    bcntrim_bsscfg->bcn_rate_kbps <
				WLC_RATE_500K_TO_KBPS(bcntrim_bsscfg->phyrate_threshold)) {
		disable = FALSE;
	} else {
		disable = TRUE;
	}
	bit_present = !!(WL_BCNTRIM_DISABLE_PHY_RATE & bcntrim_bsscfg->disable_reqs);

	if ((disable && !bit_present) || (!disable && bit_present)) {
		wlc_bcntrim_update_disable_req(bcntrim, cfg, WL_BCNTRIM_DISABLE_PHY_RATE, disable);
	}
}

static void
wlc_bcntrim_update_tsf(wlc_info_t * wlc, wlc_bsscfg_t *bsscfg)
{
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	wlc_bcntrim_info_t *bcntrim_info = wlc->bcntrim;

	ASSERT(bcntrim_info);
	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(wlc->bcntrim, bsscfg);

	if (wlc->clk) {
		WL_BCNTRIM_INFO(("wl%d: bcntrim_update_tsf %d\n",
		                wlc->pub->unit, bcntrim_bsscfg->tsf_drift));
		wlc_bmac_write_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr
			+ (M_BCNTRIM_TSFLMT_OFFSET(wlc)), bcntrim_bsscfg->tsf_drift);
	}

}
/* process BSS Load IE in beacon */
static int
wlc_bcntrim_parse_qbssload_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	bool disable, bit_present;

	ASSERT(bcntrim_info);
	ASSERT(bcntrim_info->wlc);
	ASSERT(data->cfg);

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, data->cfg);

	if (!((BSSCFG_INFRA_STA(data->cfg)) && data->cfg->associated)) {
		WL_BCNTRIM_INFO(("wl%d, qbss_ie: disable_req 0x%x disable_mask"
		                "0x%x infra sta %d\n",
		                bcntrim_info->wlc->pub->unit, bcntrim_bsscfg->disable_reqs,
		                bcntrim_bsscfg->override_disable_mask,
		                (BSSCFG_INFRA_STA(data->cfg))));
		return BCME_OK;
	}

	/* too short ies are possible: guarded against here */
	if (data->ie != NULL && data->ie_len >= BSS_LOAD_IE_SIZE) {
		disable = TRUE;
	} else {
		disable = FALSE;
	}
	bit_present = !!(WL_BCNTRIM_DISABLE_QBSSLOAD_IE & bcntrim_bsscfg->disable_reqs);
	if ((disable && !bit_present) || (!disable && bit_present)) {
		wlc_bcntrim_update_disable_req(bcntrim_info, data->cfg,
		                               WL_BCNTRIM_DISABLE_QBSSLOAD_IE, disable);
	}
	return BCME_OK;

}

#ifdef STA
static int
wlc_bcntrim_parse_quiet_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_bcntrim_info_t *bcntrim_info = (wlc_bcntrim_info_t *)ctx;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	bool disable, bit_present;

	ASSERT(bcntrim_info);
	ASSERT(bcntrim_info->wlc);
	ASSERT(data->cfg);

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, data->cfg);

	if (!((BSSCFG_INFRA_STA(data->cfg)) && data->cfg->associated)) {
		WL_BCNTRIM_INFO(("wl%d, quiet_ie: disable_req 0x%x disable_mask"
		                "0x%x infra sta %d\n",
		                bcntrim_info->wlc->pub->unit, bcntrim_bsscfg->disable_reqs,
		                bcntrim_bsscfg->override_disable_mask,
		                (BSSCFG_INFRA_STA(data->cfg))));
		return BCME_OK;
	}
	/* too short ies are possible: guarded against here */
	if (data->ie != NULL && data->ie_len >= sizeof(dot11_quiet_t)) {
		disable = TRUE;
	} else {
		disable = FALSE;
	}
	bit_present = !!(WL_BCNTRIM_DISABLE_QUIET_IE & bcntrim_bsscfg->disable_reqs);
	if ((disable && !bit_present) || (!disable && bit_present)) {
		wlc_bcntrim_update_disable_req(bcntrim_info, data->cfg,
		                               WL_BCNTRIM_DISABLE_QUIET_IE, disable);
	}
	return BCME_OK;
}
#endif /* STA */

static void
wlc_bcntrim_update_disable_req(wlc_bcntrim_info_t *bcntrim_info, wlc_bsscfg_t *bsscfg,
                               uint32 req, bool disable)
{

	wlc_info_t *wlc = bcntrim_info->wlc;
	uint32 prev_disable_reqs, cur_disable_reqs;
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim_info, bsscfg);
	uint32 now = OSL_SYSUPTIME();

	prev_disable_reqs = (bcntrim_bsscfg->disable_reqs &
	                     ~bcntrim_bsscfg->override_disable_mask);

	if (disable) {
		bcntrim_bsscfg->disable_reqs |= req;
	} else {
		bcntrim_bsscfg->disable_reqs &= ~req;
	}
	cur_disable_reqs = (bcntrim_bsscfg->disable_reqs &
	                    ~bcntrim_bsscfg->override_disable_mask);

	WL_BCNTRIM_INFO(("wl%d, update_disable_req: req|disable 0x%x prev/cur 0x%x/0x%x clk %d\n",
	                wlc->pub->unit, (req << 8 | disable), prev_disable_reqs,
	                cur_disable_reqs, wlc->clk));

	/* transitioning state */
	if (cur_disable_reqs && !prev_disable_reqs) {
		bcntrim_bsscfg->disable_start_time = now;
	} else if (!cur_disable_reqs && prev_disable_reqs) {
		bcntrim_bsscfg->disable_duration += (now - bcntrim_bsscfg->disable_start_time);
	}
	if (!bcntrim_info->wlc->clk) {
		return;
	}

	if (cur_disable_reqs) {
		bcntrim_bsscfg->bcntrim_threshold_applied = 0;
		wlc_bmac_write_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr
			+ (M_BCNTRIM_PER_OFFSET(wlc)), 0);
	} else {
		bcntrim_bsscfg->bcntrim_threshold_applied = bcntrim_bsscfg->bcntrim_threshold;
		wlc_bmac_write_shm(wlc->hw, bcntrim_info->bcntrim_shmem_addr
			+ (M_BCNTRIM_PER_OFFSET(wlc)), bcntrim_bsscfg->bcntrim_threshold);
	}
	WL_BCNTRIM_INFO(("wl%d, update_disable_req: threshold/applied %d %d\n",
	                wlc->pub->unit,
	                bcntrim_bsscfg->bcntrim_threshold,
	                bcntrim_bsscfg->bcntrim_threshold_applied));
}

/* need to update bcntrim channel for csa case */
void
wlc_stf_bcntrim_handle_csa_chanspec(wlc_bcntrim_info_t *bcntrim, wlc_bsscfg_t *bsscfg)
{
	bcntrim_bsscfg_cubby_t *bcntrim_bsscfg;
	wlc_info_t *wlc = bcntrim->wlc;

	bcntrim_bsscfg = BCNTRIM_BSSCFG_CUBBY(bcntrim, bsscfg);

	ASSERT(bcntrim_bsscfg);

	if (bsscfg->associated && (!ETHER_ISNULLADDR(&bsscfg->BSSID))) {
		bcntrim_bsscfg->bcntrim_channel = CHSPEC_CHANNEL(bsscfg->current_bss->chanspec);
		WL_BCNTRIM_TRACE(("wl%d: bcntrim_handle_csa, bcntrim_chan %d\n",
		                 wlc->pub->unit, bcntrim_bsscfg->bcntrim_channel));
	}
}
