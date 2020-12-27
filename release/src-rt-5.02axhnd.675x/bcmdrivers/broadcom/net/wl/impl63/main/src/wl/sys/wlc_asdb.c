/*
 * Adaptive Simultaneous Dual Band (ASDB) functionality of 802.11 Networking Driver
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlc_asdb.c 549348 2015-04-15 14:54:30Z $
 */

/**
 * @file
 * @brief
 * Twiki: http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/AdaptiveSimultaneousDualBand
 */

#ifdef WL_ASDB
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <wlioctl.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_modesw.h>
#include <wlc_pub.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#include <wlc_objregistry.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_asdb.h>

#define RSSI_VSDB_THRESH (-70)
#define RSSI_RSDB_THRESH (-60)
#define MCS_RSDB_THRESH (3)
#define MCS_VSDB_THRESH (2)
#define NSS_RSDB_THRESH (2)
#define NSS_VSDB_THRESH (1)
#define MCS_SPEC_INVALID (0xFFFF)

#define ASDB_MIN_SWITCH_GAP (3) /* Minimum seconds to wait b/w switches from this module */

#define ASDB_CFG_TXBYTES(cfg) ((cfg)->wlcif->_cnt.txbyte)

/* Check to go to RSDB with VHT rates. This condition checks that both RSSI and rates are good
 * to go to RSDB.
 */
#define MCS_NSS_MEET_RSDB_THRESHOLD_VHT(asdb, mcs, rssi, cfg) \
	((rssi >= (asdb)->range_info->rssi_rsdb_thresh) && ((wlc_asdb_tp_running(asdb, cfg)) ? \
		(mcs > (asdb)->range_info->mcs_rsdb_thresh) : TRUE))

/* Check to go to RSDB with HT rates. With current implementation this check is similar to VHT  */
#define MCS_NSS_MEET_RSDB_THRESHOLD_HT(asdb, mcs, rssi, cfg) \
	MCS_NSS_MEET_RSDB_THRESHOLD_VHT(asdb, mcs, rssi, cfg)

/* Check to go to VSDB with VHT rates. If any of mcs, nss and rssi meets the threshold then
 * move to VSDB.
 */
#define MCS_NSS_MEET_VSDB_THRESHOLD_VHT(asdb, mcs, nss, rssi, cfg) \
	((wlc_asdb_tp_running(asdb, cfg) && (((nss > (asdb)->range_info->nss_vsdb_thresh) && \
	(!mcs)) || ((nss == (asdb)->range_info->nss_vsdb_thresh) && mcs <= \
	(asdb)->range_info->mcs_vsdb_thresh))) || \
	(rssi <= (asdb)->range_info->rssi_vsdb_thresh))

/* Check to go to VSDB with HT rates */
#define MCS_NSS_MEET_VSDB_THRESHOLD_HT(asdb, mcs, rssi, cfg) \
	((wlc_asdb_tp_running(asdb, cfg) && \
	(mcs <= (asdb)->range_info->mcs_vsdb_thresh)) || \
	(rssi <= (asdb)->range_info->rssi_vsdb_thresh))

/* IOVAR table */
enum {
	IOV_ASDB_ENAB = 1,
	IOV_ASDB_LAST
};

/* BAND indexes */
enum {
	ASDB_BAND_INDEX_2G = 0, /* Index for cfg related information in 2g */
	ASDB_BAND_INDEX_5G = 1, /* Index for cfg related information in 5g */
	ASDB_BAND_INDEX_MAX
};

static const bcm_iovar_t asdb_iovars[] = {
	{"asdb_enab", IOV_ASDB_ENAB, 0, 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/*	Feature Bit map allocated based on priority. Bit 0 with highest and bit 7 being lowest.
 *	+===============================================================+
 *	|Resrved|Resrved|Resrved|Resrved|Resrved|Resrved|Resrved| Range	|
 *	|	|	|	|	|	|	|	|	|
 *	| bit 7	| bit 6	| bit 5	| bit 4	| bit 3	| bit 2	| bit 1	| bit 0	|
 *	+===============================================================+
 */
typedef enum {
	ASDB_FEATURE_NONE = 0x00,
	ASDB_FEATURE_RANGE = 0x01, /* Range Based Trigger */
} asdb_feature_enab;

/* RSSI, MCS and NSS threshold values for VSDB and RSDB modes.
 * TODO: These can be modified dynamically with IOVAR.
 */
typedef struct wlc_asdb_range_info {
	int rssi_vsdb_thresh;
	int rssi_rsdb_thresh;
	uint8 mcs_rsdb_thresh;
	uint8 mcs_vsdb_thresh;
	uint8 nss_rsdb_thresh;
	uint8 nss_vsdb_thresh;
} wlc_asdb_range_info_t;

typedef struct wlc_asdb_bsscfgs_info {
	wlc_bsscfg_t *cfg;
	uint32 tx_delta;
	uint64 old_txbyte;
} wlc_asdb_bsscfgs_info_t;

/* ASDB Information Structure */
typedef struct wlc_asdb_info {
	wlc_info_t *wlc; /* To hold primary wlc inorder to have acess all WLCs */
	wlc_asdb_range_info_t *range_info;
	uint32 asdb_err_counter; /* Error counter for ASDB */
	uint8 min_switch_gap; /* Minimum number of seconds between switches from ASDB */
	uint8 switch_gap_counter; /* Counter to count down the switch gap seconds */
	/* Bit positions in this flag indicates the enabling of features */
	uint8 feature_enab_flag;
	wlc_asdb_bsscfgs_info_t *cfgs_info; /* Information related to bsscfgs active in ASDB */
} wlc_asdb_info_t;

struct wlc_asdb {
	wlc_info_t *wlc;
	wlc_asdb_info_t *asdb_info;
};

static int wlc_asdb_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_asdb_handle_modesw_trigger(wlc_asdb_t *asdb);
static void wlc_asdb_watchdog(void *context);
static void wlc_asdb_enab_disab_feature(wlc_asdb_info_t *asdb, asdb_feature_enab feature,
	bool enab);
static bool wlc_asdb_is_feature_enabled(wlc_asdb_info_t *asdb, asdb_feature_enab feature);
static void wlc_asdb_switch_gap_start(wlc_asdb_info_t *asdb);
static int wlc_asdb_dyn_switch(wlc_asdb_info_t *asdb);
static bool wlc_asdb_range_modesw_required(wlc_asdb_info_t *asdb);
static bool wlc_asdb_tp_running(wlc_asdb_info_t *asdb, wlc_bsscfg_t *cfg);

#include <wlc_patch.h>

INLINE
static void
wlc_asdb_enab_disab_feature(wlc_asdb_info_t *asdb, asdb_feature_enab feature, bool enab)
{
	if (enab) {
		asdb->feature_enab_flag |= feature;
	} else {
		asdb->feature_enab_flag &= (~feature);
	}
}

INLINE
static bool
wlc_asdb_is_feature_enabled(wlc_asdb_info_t *asdb, asdb_feature_enab feature)
{
	return (asdb->feature_enab_flag & feature);
}

/* Attach function for ASDB module. Asdb module information is allocated only once and the
 * same is shared with both WLCs.
 */
wlc_asdb_t *
BCMATTACHFN(wlc_asdb_attach)(wlc_info_t *wlc)
{
	wlc_asdb_t *asdb_hdl;
	wlc_asdb_info_t *asdb;

	if ((asdb_hdl = MALLOCZ_PERSIST(wlc->osh, sizeof(wlc_asdb_t))) == NULL) {
		WL_ATTACH_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto error;
	}

	asdb_hdl->wlc = wlc;

	asdb = (wlc_asdb_info_t *) obj_registry_get(wlc->objr, OBJR_ASDB_INFO);

	if (asdb == NULL) {
		/* No object found !, so alloc new object here as set the object */

		if (!(asdb = (wlc_asdb_info_t *)MALLOCZ_PERSIST(wlc->osh,
			sizeof(wlc_asdb_info_t)))) {
			WL_ATTACH_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto error;
		}
		/* The asdb->wlc is always primary wlc. Attach in asdb module happens only for
		 * primary wlc. The same asdb object is shared across two wlcs using object
		 * registry.
		 */
		asdb->wlc = wlc;

		obj_registry_set(wlc->objr, OBJR_ASDB_INFO, asdb);

		if ((asdb->range_info = (wlc_asdb_range_info_t *) MALLOC_PERSIST(wlc->osh,
			sizeof(wlc_asdb_range_info_t))) == NULL) {
			WL_ATTACH_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto error;
		}

		if ((asdb->cfgs_info = MALLOC_PERSIST(wlc->osh,
			sizeof(wlc_asdb_bsscfgs_info_t) * ASDB_BAND_INDEX_MAX)) == NULL) {
			WL_ATTACH_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto error;
		}

		asdb->range_info->rssi_rsdb_thresh = RSSI_RSDB_THRESH;
		asdb->range_info->rssi_vsdb_thresh = RSSI_VSDB_THRESH;
		asdb->range_info->mcs_rsdb_thresh = MCS_RSDB_THRESH;
		asdb->range_info->mcs_vsdb_thresh = MCS_VSDB_THRESH;
		asdb->range_info->nss_rsdb_thresh = NSS_RSDB_THRESH;
		asdb->range_info->nss_vsdb_thresh = NSS_VSDB_THRESH;
		asdb->min_switch_gap = ASDB_MIN_SWITCH_GAP;
		asdb->cfgs_info[ASDB_BAND_INDEX_2G].cfg = NULL;
		asdb->cfgs_info[ASDB_BAND_INDEX_5G].cfg = NULL;

		/* Include all the enabling and disabling of features here. */
		/* RANGE Based asdb enabled */
		wlc_asdb_enab_disab_feature(asdb, ASDB_FEATURE_RANGE, FALSE);
	}

	asdb_hdl->asdb_info = asdb;
	/* Registration of ASDB Module */
	if (wlc_module_register(wlc->pub, asdb_iovars, "asdb", asdb_hdl, wlc_asdb_doiovar,
		wlc_asdb_watchdog, NULL, NULL)) {
		goto error;
	}

	/* Hit a reference for this object */
	(void) obj_registry_ref(wlc->objr, OBJR_ASDB_INFO);

	wlc->pub->cmn->_asdb = TRUE;
	return asdb_hdl;
error:
	MODULE_DETACH(asdb_hdl, wlc_asdb_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_asdb_detach)(wlc_asdb_t *asdb_hdl)
{
	wlc_asdb_info_t *asdb;

	if (!asdb_hdl)
		return;

	asdb = asdb_hdl->asdb_info;

	if (obj_registry_unref(asdb->wlc->objr, OBJR_ASDB_INFO) == 0) {
		if (asdb->range_info) {
			MFREE_PERSIST(asdb->wlc->osh, asdb->range_info,
				sizeof(wlc_asdb_range_info_t));
		}

		if (asdb->cfgs_info) {
			MFREE_PERSIST(asdb->wlc->osh, asdb->cfgs_info,
				sizeof(wlc_asdb_bsscfgs_info_t) * ASDB_BAND_INDEX_MAX);
		}
		MFREE_PERSIST(asdb->wlc->osh, asdb, sizeof(wlc_asdb_info_t));
	}
	MFREE_PERSIST(asdb_hdl->wlc->osh, asdb_hdl, sizeof(wlc_asdb_t));
}

static int
wlc_asdb_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
#ifdef ASDB_DBG
	wlc_asdb_t *asdb_hdl = (wlc_asdb_t *) handle;
	wlc_asdb_info_t *asdb = asdb_hdl->asdb_info;
#endif /* ASDB_DBG */
	int err = BCME_OK;
	switch (actionid) {
#ifdef ASDB_DBG
		case IOV_GVAL(IOV_ASDB_ENAB):
			*((uint32*)a) = (uint32) asdb->feature_enab_flag;
			break;
		case IOV_SVAL(IOV_ASDB_ENAB):
			asdb->feature_enab_flag = *((int*) p);
			break;
#endif /* ASDB_DBG */
		default:
			break;
	}
	return err;
}

/* This function returns TRUE if throughput was running for the cfg for last watchdog
 * else returns FALSE;
 */
static bool
wlc_asdb_tp_running(wlc_asdb_info_t *asdb, wlc_bsscfg_t *cfg)
{
	if (CHSPEC_IS2G(cfg->current_bss->chanspec)) {
		if (asdb->cfgs_info[ASDB_BAND_INDEX_2G].tx_delta) {
			return TRUE;
		}
	} else if (CHSPEC_IS5G(cfg->current_bss->chanspec)) {
		if (asdb->cfgs_info[ASDB_BAND_INDEX_5G].tx_delta) {
			return TRUE;
		}
	}
	return FALSE;
}

/* This function returns whether our connections are valid for ASDB to operate. i.e. We need
 * to have two connection in 2 different bands to switch b/w RSDB and VSDB.
 */
static bool
wlc_asdb_is_dual_band(wlc_asdb_info_t *asdb)
{
	wlc_info_t *wlc = asdb->wlc; /* primary wlc */
	wlc_bsscfg_t *icfg;
	int idx;
	bool is_2g = FALSE, is_5g = FALSE;

	FOREACH_ALL_WLC_BSS(wlc, idx, icfg) {
		if (WLC_BSS_CONNECTED(icfg)) {
			if (CHSPEC_IS5G(icfg->current_bss->chanspec)) {
				is_5g = TRUE;
			} else if (CHSPEC_IS2G(icfg->current_bss->chanspec)) {
				is_2g = TRUE;
			}
		}
	}

	if (is_2g && is_5g) {
		return TRUE;
	}
	return FALSE;
}

/* This function update the CFG information which are active in ASDB.
 * Currently is updates the tx bytes and delta of tx bytes b/w seconds.
 */
static void
wlc_asdb_update_cfgs_info(wlc_asdb_info_t *asdb)
{
	wlc_info_t *wlc = asdb->wlc; /* primary wlc */
	wlc_asdb_bsscfgs_info_t *cfgs_info2g = &(asdb->cfgs_info[ASDB_BAND_INDEX_2G]);
	wlc_asdb_bsscfgs_info_t *cfgs_info5g = &(asdb->cfgs_info[ASDB_BAND_INDEX_5G]);
	wlc_bsscfg_t *icfg;
	int idx;

	ASSERT(wlc == wlc->cmn->wlc[0]);

	FOREACH_ALL_WLC_BSS(wlc, idx, icfg) {
		if (WLC_BSS_CONNECTED(icfg)) {
			if (CHSPEC_IS5G(icfg->current_bss->chanspec)) {
				cfgs_info5g->cfg = icfg;
			} else if (CHSPEC_IS2G(icfg->current_bss->chanspec)) {
				cfgs_info2g->cfg = icfg;
			}
		}
	}

	if (cfgs_info2g->cfg && cfgs_info5g->cfg) {
		/* If tx bytes of cfgs is reset or wraps around then, reset cached
		 * values.
		 */
		if (ASDB_CFG_TXBYTES(cfgs_info2g->cfg) < cfgs_info2g->old_txbyte)
			cfgs_info2g->old_txbyte = 0;
		if (ASDB_CFG_TXBYTES(cfgs_info5g->cfg) < cfgs_info5g->old_txbyte)
			cfgs_info5g->old_txbyte = 0;
		/* Calculate the throughput delta b/w seconds */
		cfgs_info2g->tx_delta = ASDB_CFG_TXBYTES(cfgs_info2g->cfg) -
			cfgs_info2g->old_txbyte;
		cfgs_info5g->tx_delta = ASDB_CFG_TXBYTES(cfgs_info5g->cfg) -
			cfgs_info5g->old_txbyte;

		/* Cache the current t/p values */
		cfgs_info2g->old_txbyte = ASDB_CFG_TXBYTES(cfgs_info2g->cfg);
		cfgs_info5g->old_txbyte = ASDB_CFG_TXBYTES(cfgs_info5g->cfg);
	}
}
/* This is function is an API to other modules which wants to know if ASDB is active */
bool
wlc_asdb_active(wlc_asdb_t *asdb_hdl)
{
	wlc_asdb_info_t *asdb = asdb_hdl->asdb_info;
	/* If ASDB is not enabled OR No feature in asdb is enabled OR
	 * If connections are not in dual band then, we cannot switch b/w RSDB and
	 * VSDB, and this imples that ASDB is not active.
	 */
	if (ASDB_ENAB(asdb->wlc->pub) && asdb->feature_enab_flag && wlc_asdb_is_dual_band(asdb)) {
		return TRUE;
	}
	return FALSE;
}

static void
wlc_asdb_watchdog(void *context)
{
	wlc_asdb_t *asdb_hdl = (wlc_asdb_t *) context;
	wlc_asdb_info_t *asdb = asdb_hdl->asdb_info;
	wlc_info_t *wlc = asdb_hdl->wlc;
	int ret;

	/* Process watchdog for only one wlc. */
	if (wlc != WLC_RSDB_GET_PRIMARY_WLC(wlc)) {
		return;
	}

	/* If ASDB is not active then do not process anything */
	if (!wlc_asdb_active(asdb_hdl)) {
		return;
	}

	/* update the CFGs information */
	wlc_asdb_update_cfgs_info(asdb);

	/* Decrement the gap counter every second and do not allow any switches till the
	 * counter is zero.
	 */
	if (asdb->switch_gap_counter) {
		asdb->switch_gap_counter--;
		return;
	}

	/* If either MCHAN is active or RSDB then invoke wlc_asdb_handle_modesw_trigger */
	if (MCHAN_ACTIVE(wlc->pub) || RSDB_ACTIVE(wlc->pub)) {
		if ((ret = wlc_asdb_handle_modesw_trigger(asdb_hdl)) != BCME_OK) {
			asdb->asdb_err_counter++;
			WL_INFORM(("wl%d: ret:%d err_count:%d:%s:"
				"wlc_asdb_handle_modesw_trigger failed !\n", wlc->pub->unit, ret,
				asdb->asdb_err_counter, __FUNCTION__));
		}
	}
}

/* Reset the counter to avoid switches for ASDB_MIN_SWITCH_GAP seconds */
INLINE
static void
wlc_asdb_switch_gap_start(wlc_asdb_info_t *asdb)
{
	asdb->switch_gap_counter = asdb->min_switch_gap;
}

static int
wlc_asdb_handle_modesw_trigger(wlc_asdb_t *asdb_hdl)
{
	wlc_info_t *wlc = asdb_hdl->wlc;
	wlc_asdb_info_t *asdb = asdb_hdl->asdb_info;
	int ret = BCME_OK;
	BCM_REFERENCE(wlc);

	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		return BCME_EPERM;
	}

	if (wlc_modesw_get_switch_type(wlc->cmn->wlc[0]->modesw) != MODESW_NO_SW_IN_PROGRESS) {
		WL_ERROR(("%s: Returning, as mode switch is in progress = %d !\n", __func__,
			wlc_modesw_get_switch_type(wlc->cmn->wlc[0]->modesw)));
		return BCME_BUSY;
	}

	if (wlc_asdb_modesw_required(asdb_hdl)) {
		ret = wlc_asdb_dyn_switch(asdb);
	}

	return ret;
}

/* Generic API for handling all triggers for mode switching */
bool
wlc_asdb_modesw_required(wlc_asdb_t *asdb_hdl)
{
	if (wlc_asdb_is_feature_enabled(asdb_hdl->asdb_info, ASDB_FEATURE_RANGE) &&
		wlc_asdb_range_modesw_required(asdb_hdl->asdb_info)) {
		return TRUE;
	}
	return FALSE;
}

/* This function toggles the current mode of operation b/w RSDB and VSDB */
static int
wlc_asdb_dyn_switch(wlc_asdb_info_t *asdb)
{
	int ret = BCME_OK;
	if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(asdb->wlc))) {
		/* Switch the working mode to RSDB */
		ret = wlc_rsdb_dyn_switch(asdb->wlc, MODE_RSDB);
	} else {
		/* Switch the working mode to VSDB */
		ret = wlc_rsdb_dyn_switch(asdb->wlc, MODE_VSDB);
	}
	/* if switch is initiated successfully, start to gap counter */
	if (ret == BCME_OK)
		wlc_asdb_switch_gap_start(asdb);

	return ret;
}

/* This function gives the MCS and NSS values of the bsscfg passed */
static void
wlc_asdb_mcs_nss_get(wlc_bsscfg_t *bsscfg, uint32 *mcs, uint32 *nss, uint8 *isvht)
{
	ratespec_t rspec = wlc_get_rspec_history(bsscfg);

	*mcs = *nss = MCS_SPEC_INVALID;
	*isvht = FALSE;

	if (RSPEC_ISLEGACY(rspec)) {
		/* legacy case (if required) to be handled here */
	} else if (RSPEC_ISHT(rspec)) {
		*mcs = (rspec & WL_RSPEC_HT_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		*mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		*nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
		*isvht = TRUE;
	}
}

/* This function decides whether (as per current mode of operation) mode switch is required or not.
 *
 * 1. If we are in RSDB mode and the range is not good, then this function returns TRUE,
 * meaning that mode switch from RSDB to VSDB is required to get MIMO advantage.
 *
 * 2. Same applies during join time where, if the range of already existing connection
 * is bad in MIMO and we are tyring to bring up a new interface in other band then,
 * connection should not happen in RSDB, but should happen in VSDB MIMO. So in this case
 * it will return FALSE.
 *
 * 3. If we are already in VSDB and the range is good enough to sustain SISO operation,
 * then we should go to RSDB mode.
 */

static bool
wlc_asdb_range_modesw_required(wlc_asdb_info_t *asdb)
{
	wlc_info_t *wlc = asdb->wlc; /* primary wlc */
	wlc_info_t *wlc_iter;
	wlc_bsscfg_t *bsscfg;
	uint8 cfg_idx, wlc_idx;
	uint32 mcs, nss;
	uint8 is_vht;
	uint8 ret = FALSE;

	/* If device is in MIMO[VSDB] check whether conditions favour to go to RSDB.
	 * So this if part handles two cases
	 *	1. We are in VSDB and would like to go to RSDB.
	 *	2. There exists a connection in MIMO and want switch to RSDB for the new
	 *	   connection coming up.
	 */
	if (WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc))) {
		FOREACH_AS_STA(wlc, cfg_idx, bsscfg) {
			/* Get MCS and NSS values for associated CFG */
			wlc_asdb_mcs_nss_get(bsscfg, &mcs, &nss, &is_vht);
			/* The below check condition makes sure that, to go to RSDB,
			 * all the associated CFGs meet the requirement of
			 * good-range. Even if any one of the CFGs do not then
			 * stay in MIMO VSDB.
			 */
			if (is_vht) {
				/* RSDB threshold check for VHT */
				if (MCS_NSS_MEET_RSDB_THRESHOLD_VHT(asdb, mcs,
					bsscfg->link->rssi, bsscfg)) {
					continue;
				} else {
					return FALSE;
				}
			} else {
				/* RSDB threshold check for HT */
				if (MCS_NSS_MEET_RSDB_THRESHOLD_HT(asdb, mcs,
					bsscfg->link->rssi, bsscfg)) {
					continue;
				} else {
					return FALSE;
				}
			}
		}
		/* If all the CFGs are meeting the requirement to switch to RSDB
		 * then return TRUE.
		 */
		ret = TRUE;
	} else {
		/* This part handles the case to see if we want to move to VSDB.
		 * i.e. Device is in RSDB and due to Range, need to switch to VSDB.
		 */
		FOREACH_WLC(wlc->cmn, wlc_idx, wlc_iter) {
			FOREACH_AS_STA(wlc_iter, cfg_idx, bsscfg)
			{
				/* Get MCS and NSS values for associated CFG */
				wlc_asdb_mcs_nss_get(bsscfg, &mcs, &nss, &is_vht);
				/* If any of the CFGs has bad range then mode switch
				 * to VSDB is required.
				 */
				if (is_vht) {
					if (MCS_NSS_MEET_VSDB_THRESHOLD_VHT(asdb, mcs, nss,
						bsscfg->link->rssi, bsscfg)) {
						return TRUE;
					}
				} else {
					if (MCS_NSS_MEET_VSDB_THRESHOLD_HT(asdb, mcs,
						bsscfg->link->rssi, bsscfg)) {
						return TRUE;
					}
				}
			}
		}
	}
	return ret;
}
#endif /* WL_ASDB */
