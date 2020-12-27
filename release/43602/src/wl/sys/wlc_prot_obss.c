/*
 * OBSS Protection support
 * Broadcom 802.11 Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

/**
 * @file
 * @brief
 * Out of Band BSS
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan.h>
#ifdef APCS
#include <wlc_apcs.h>
#endif
#include <wlc_rm.h>
#include <wlc_ap.h>
#include <wlc_scb.h>
#include <wlc_txc.h>
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#ifdef WLOFFLD
#include <wlc_offloads.h>
#endif
#include "wlc_vht.h"

#include <wlc_txc.h>
#include <wlc_prot_obss.h>
#include <wlc_modesw.h>
#include <wlc_lq.h>
#include <wlc_prot_obss.h>

#define PROT_OBSS_DELTASTATS(result, curr, prev) \
			result = (curr - prev);

typedef struct {
	uint32 obss_inactivity_period;	/* quiet time prior to deactivating OBSS protection */
	uint8 obss_dur_thres;		/* OBSS protection trigger/RX CRS Sec */
	int8 obss_sec_rssi_lim0;	/* OBSS secondary RSSI limit 0 */
	int8 obss_sec_rssi_lim1;	/* OBSS secondary RSSI limit 1 */
} wlc_prot_obss_config_t;

/* module private states */
typedef struct {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_prot_obss_config_t *config;
	wlc_prot_dynbwsw_config_t *cfg_dbs;

	/* ucode previous and current stat counters */
	wlc_bmac_obss_counts_t *prev_stats;
	wlc_bmac_obss_counts_t *curr_stats;

	/* Cummulative stat counters */
	wlc_bmac_obss_counts_t *total_stats;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* For diagnostic measurements */
	wlc_bmac_obss_counts_t *msrmnt_stored;
	cca_stats_n_flags *results;
	struct wl_timer *stats_timer;
#endif

	uint16 obss_inactivity;	/* # of secs of OBSS inactivity */
	int8 mode;

	int cfgh;   /* bsscfg cubby handle */
	uint32 obss_active_cnt; /* global: non-zero value indicates that at least
		* one bss has obss active
		*/
	uint8 dyn_bwsw_enabled;	/* whether to disable dynamic bwswitch
		* specifically. Note: this could be temporary to disable pseudo bwsw
		* statemachine. By default pseudo bwsw is enabled.
		*/
} wlc_prot_obss_info_priv_t;

/*
 * module states layout
 */
typedef struct {
	wlc_prot_obss_info_t pub;
	wlc_prot_obss_info_priv_t priv;

	wlc_prot_obss_config_t config;	/* OBSS protection configuration */
	wlc_prot_dynbwsw_config_t cfg_dbs; /* OBSS DYN BWSW Config */

	/* ucode previous and current stat counters */
	wlc_bmac_obss_counts_t prev_stats;
	wlc_bmac_obss_counts_t curr_stats;

	/* Cummulative stat counters */
	wlc_bmac_obss_counts_t total_stats;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* For diagnostic measurements */
	wlc_bmac_obss_counts_t msrmnt_stored;
	cca_stats_n_flags results;
#endif
} wlc_prot_obss_t;

static uint16 wlc_prot_obss_info_priv_offset = OFFSETOF(wlc_prot_obss_t, priv);

#define WLC_PROT_OBSS_SIZE (sizeof(wlc_prot_obss_t))
#define WLC_PROT_OBSS_INFO_PRIV(prot) ((wlc_prot_obss_info_priv_t *) \
				    ((uintptr)(prot) + wlc_prot_obss_info_priv_offset))

/* Private cubby struct for protOBSS */
typedef struct {
	uint8 obss_activity;	/* obss activity: inactive, ht or non-ht */
	chanspec_t orig_chanspec;  /* original assoc[STA]/start[AP] chanspec */
	uint16 bwswpsd_state;	/* possible states: BWSW_PSEUDOUPGD_XXX above */
	chanspec_t bwswpsd_cspec_for_tx;	/* If we are in pseudo phy
		* cspec upgraded state, tx pkts should use this overridden cspec.
		* Also, if we find that pseudo upgrade doesn't solve obss issue, we
		* need to return to this cspec.
		*/
	uint8 obss_bwsw_no_activity_cfm_count;	/* # of secs of OBSS inactivity
		* for bwswitch. [checked against obss_bwsw_no_activity_cfm_count_max]
		*/
	uint8 obss_bwsw_no_activity_cfm_count_max; /* initial value:
		* obss_bwsw_no_activity_cfm_count_cfg and later incremented in steps
		* of obss_bwsw_no_activity_cfm_count_incr_cfg, if required.
		*/
	uint8 obss_bwsw_activity_cfm_count; /* after we detect OBSS using stats, we
		* will confirm it by waiting for # of secs before we BWSW. This field
		* is to count that. [checked against obss_bwsw_activity_cfm_count_cfg]
		*/
	uint16 obss_bwsw_pseudo_sense_count; /* number of seconds/cnt to be in
		* pseudo state. This is used to sense/measure the stats from lq.
		*/
} protobss_bsscfg_cubby_t;

typedef struct wlc_po_cb_ctx {
	uint16		connID;
} wlc_po_cb_ctx_t;

/* bsscfg specific info access accessor */
#define PROTOBSS_BSSCFG_CUBBY_LOC(protobss, cfg) \
	((protobss_bsscfg_cubby_t **)BSSCFG_CUBBY((cfg), (protobss)->cfgh))
#define PROTOBSS_BSSCFG_CUBBY(protobss, cfg) (*(PROTOBSS_BSSCFG_CUBBY_LOC(protobss, cfg)))

#define PROTOBSS_BSS_MATCH(wlc, cfg)			\
	( 	/* for assoc STA's... */					\
		((BSSCFG_STA(cfg) && cfg->associated) ||	\
		/* ... or UP AP's ... */						\
		(BSSCFG_AP(cfg) && cfg->up)) &&			\
		/* ... AND chanspec matches currently measured chanspec */	\
		(cfg->current_bss->chanspec == wlc->chanspec))

#define chspec_to_rspec(chspec_bw)	\
		(((uint32)(((((uint16)(chspec_bw)) & WL_CHANSPEC_BW_MASK) >> \
			WL_CHANSPEC_BW_SHIFT) - 1)) << RSPEC_BW_SHIFT)

enum {
	OBSS_INACTIVE = 0,
	OBSS_ACTIVE_HT,
	OBSS_ACTIVE_NONHT
};

enum bwsw_pseudoupgd_states {
	BWSW_PSEUDOUPGD_NOTACTIVE = 0,
	BWSW_PSEUDOUPGD_PENDING,
	BWSW_PSEUDOUPGD_ACTIVE
};

/* iovar table */
enum {
	IOV_OBSS_PROT = 1,
	IOV_OBSS_DYN_BWSW_ENAB = 2,
	IOV_OBSS_DYN_BWSW_PARAMS = 3,
	IOV_OBSS_INACTIVITY_PERIOD = 4,
	IOV_OBSS_DUR = 5,
	IOV_OBSS_DUMP = 6,
	IOV_OBSS_DYN_BWSW_DUMP = 7,
	IOV_LAST
};

/* OBSS BWSW enable enums */
enum {
	OBSS_DYN_BWSW_DISABLE = 0,
	OBSS_DYN_BWSW_ENAB_RXCRS = 1,
	OBSS_DYN_BWSW_ENAB_TXOP = 2
};

static const bcm_iovar_t wlc_prot_obss_iovars[] = {
	{"obss_prot", IOV_OBSS_PROT,
	(0), IOVT_BUFFER, sizeof(wl_config_t),
	},
	{"dyn_bwsw_enab", IOV_OBSS_DYN_BWSW_ENAB,
	(IOVF_SET_DOWN), IOVT_UINT8, 0
	},
	{"dyn_bwsw_params", IOV_OBSS_DYN_BWSW_PARAMS,
	(0), IOVT_BUFFER, sizeof(obss_config_params_t),
	},

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	{"dump_obss", IOV_OBSS_DUMP,
	(IOVF_GET_UP), IOVT_BUFFER, sizeof(cca_stats_n_flags),
	},
	{"dump_obss_dyn_bwsw", IOV_OBSS_DYN_BWSW_DUMP,
	(IOVF_GET_UP), IOVT_BUFFER, WLC_IOCTL_MAXLEN,
	},
#endif
	{NULL, 0, 0, 0, 0}
};

static int
wlc_prot_obss_manage_bw_switch(wlc_prot_obss_info_t *prot);

static int
wlc_prot_obss_bssinfo_init(void *ctx, wlc_bsscfg_t *cfg);

static void
wlc_prot_obss_bssinfo_deinit(void *ctx, wlc_bsscfg_t *cfg);

#ifdef WL_MODESW
static void
wlc_prot_obss_modesw_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data);

#endif /* WL_MODESW */


#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_prot_obss_stats_sample(wlc_prot_obss_info_t *prot, int duration)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	if (duration) {
		/* Store results of first read */
		wlc_bmac_obss_stats_read(wlc->hw, priv->msrmnt_stored);
		wl_add_timer(wlc->wl, priv->stats_timer, duration, 0);
		priv->results->msrmnt_time = duration;
	} else {
		wlc_bmac_obss_counts_t delta, now;
		wlc_bmac_obss_counts_t *curr = &now;
		wlc_bmac_obss_counts_t *prev = priv->msrmnt_stored;

		/* Read current values and compute the delta. */
		wlc_bmac_obss_stats_read(wlc->hw, curr);

		memset(&delta, '\0', sizeof(delta));

		delta.usecs = curr->usecs - prev->usecs;
		delta.txdur = curr->txdur - prev->txdur;
		delta.ibss = curr->ibss - prev->ibss;
		delta.obss = curr->obss - prev->obss;
		delta.noctg = curr->noctg - prev->noctg;
		delta.nopkt = curr->nopkt - prev->nopkt;
		delta.PM = curr->PM - prev->PM;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
		delta.txopp = curr->txopp - prev->txopp;
		delta.gdtxdur = curr->gdtxdur - prev->gdtxdur;
		delta.bdtxdur = curr->bdtxdur - prev->bdtxdur;
		delta.slot_time_txop = curr->slot_time_txop;
#endif

#ifdef ISID_STATS
		delta.crsglitch = curr->crsglitch - prev->crsglitch;
		delta.badplcp = curr->badplcp - prev->badplcp;
		delta.bphy_crsglitch = curr->bphy_crsglitch - prev->bphy_crsglitch;
		delta.bphy_badplcp = curr->bphy_badplcp - prev->bphy_badplcp;
#endif

		delta.rxdrop20s = curr->rxdrop20s - prev->rxdrop20s;
		delta.rx20s = curr->rx20s - prev->rx20s;

		delta.rxcrs_pri = curr->rxcrs_pri - prev->rxcrs_pri;
		delta.rxcrs_sec20 = curr->rxcrs_sec20 - prev->rxcrs_sec20;
		delta.rxcrs_sec40 = curr->rxcrs_sec40 - prev->rxcrs_sec40;

		delta.sec_rssi_hist_hi = curr->sec_rssi_hist_hi - prev->sec_rssi_hist_hi;
		delta.sec_rssi_hist_med = curr->sec_rssi_hist_med - prev->sec_rssi_hist_med;
		delta.sec_rssi_hist_low = curr->sec_rssi_hist_low - prev->sec_rssi_hist_low;

		memcpy(priv->msrmnt_stored, &delta, sizeof(wlc_bmac_obss_counts_t));
		priv->results->msrmnt_done = 1;
	}
}

static void
wlc_prot_obss_stats_timeout(void *arg)
{
	wlc_prot_obss_info_t *prot = (wlc_prot_obss_info_t *) arg;

	wlc_prot_obss_stats_sample(prot, 0);
}
#endif 

static void
wlc_prot_obss_enable(wlc_prot_obss_info_t *prot, bool enable)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	WL_NONE(("wl%d: %s CTS2SELF\n", wlc->pub->unit, enable ? "Enabling" : "Disabling"));
	wlc_bmac_mhf(wlc->hw, MHF1,
	             MHF1_CTS2SELF,
	             enable ? MHF1_CTS2SELF : 0,
	             WLC_BAND_5G);
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
	prot->protection = enable;
}

/* static inline function for  TXOP stats update */
static INLINE bool
wlc_prot_obss_bwsw_txop_detection_logic(uint32 sample_dur,
uint32 slot_time_txop, uint32 txopp_stats, uint32 txdur,
uint8 txop_threshold)
{
	uint32 limit = (txop_threshold * sample_dur) / 100;

	/* Calculate slot time ifs */
	uint32 slot_time_ifs = ((slot_time_txop >> 8) & 0xFF) +
		(slot_time_txop & 0xFF);

	/* Calculate txop using slot time ifs  */
	uint32 txop = (txopp_stats)*(slot_time_ifs);

	return ((txop + txdur) <= limit) ? TRUE : FALSE;
}
static INLINE bool
wlc_prot_obss_sec_interf_detection_logic(uint32 sample_dur,
uint32 crs_total, uint8 thresh_seconds)
{
	uint32 limit = ((thresh_seconds * sample_dur) / 100);
	return (crs_total >= limit) ? TRUE : FALSE;
}

static INLINE bool
wlc_prot_obss_prim_interf_detection_logic(uint32 sample_dur,
uint32 crs_prim, uint32 ibss, uint8 thresh_seconds)
{
	uint32 limit = ((thresh_seconds * sample_dur) / 100);
	bool result = ((crs_prim > limit) &&
		(ibss < (crs_prim *
		OBSS_RATIO_RXCRS_PRI_VS_IBSS_DEFAULT) / 100)) ? TRUE : FALSE;
	if (result)
		WL_MODE_SWITCH(("PRI.OBSS:sample_dur:%d,rxcrs_pri:%d,"
		"rxcrs_pri_limit:%d,ibss:%d \n",
		sample_dur, crs_prim, limit, ibss));
	return result;
}

bool
wlc_prot_obss_interference_detected_for_bwsw(wlc_prot_obss_info_t *prot,
	wlc_bmac_obss_counts_t *delta_stats)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	uint32 sample_dur, txop, slot_time_ifs, txdur;
	bool prim_detected = FALSE, sec_detected = FALSE;
	uint32 rxcrs_sec40, rxcrs_sec20, rxcrs_sec, rxcrs_pri, ibss;
	bool result = FALSE;

	/* Use current stats during normal case */
	if (delta_stats == NULL)
	{
		/* Sample duration is the TSF difference (in usec) between two reads. */
		sample_dur = priv->curr_stats->usecs - priv->prev_stats->usecs;
		PROT_OBSS_DELTASTATS(rxcrs_sec40, priv->curr_stats->rxcrs_sec40,
			priv->prev_stats->rxcrs_sec40);
		PROT_OBSS_DELTASTATS(rxcrs_sec20, priv->curr_stats->rxcrs_sec20,
			priv->prev_stats->rxcrs_sec20);
		rxcrs_sec = rxcrs_sec40 + rxcrs_sec20;
		/* Check Primary OBSS */
		PROT_OBSS_DELTASTATS(rxcrs_pri, priv->curr_stats->rxcrs_pri,
			priv->prev_stats->rxcrs_pri);
		PROT_OBSS_DELTASTATS(ibss, priv->curr_stats->ibss,
			priv->prev_stats->ibss);

		slot_time_ifs = priv->curr_stats->slot_time_txop;
		PROT_OBSS_DELTASTATS(txop, priv->curr_stats->txopp,
			priv->prev_stats->txopp);
		PROT_OBSS_DELTASTATS(txdur, priv->curr_stats->txdur,
			priv->prev_stats->txdur);
	}
	else
	{
		/* Use Delta stats during pseudo Case */
		sample_dur = delta_stats->usecs;
		rxcrs_sec = delta_stats->rxcrs_sec40 + delta_stats->rxcrs_sec20;
		rxcrs_pri = delta_stats->rxcrs_pri;
		ibss = delta_stats->ibss;
		slot_time_ifs = delta_stats->slot_time_txop;
		txop = delta_stats->txopp;
		txdur = delta_stats->txdur;
	}

	/* RXCRS stats for OBSS detection */
	if (priv->dyn_bwsw_enabled == OBSS_DYN_BWSW_ENAB_RXCRS)
	{
		/* OBSS detected if RX CRS Secondary exceeds configured limit */
		sec_detected = wlc_prot_obss_sec_interf_detection_logic(sample_dur,
			rxcrs_sec,
			priv->cfg_dbs->obss_bwsw_dur_thres);

		prim_detected = wlc_prot_obss_prim_interf_detection_logic(sample_dur,
			rxcrs_pri, ibss,
			priv->cfg_dbs->obss_bwsw_rx_crs_threshold_cfg);

		WL_MODE_SWITCH(("OBSS Detected RXCRS: sample_dur:%d, rxcrs_sec:%d "
			"pri:%d, sec:%d\n", sample_dur, rxcrs_sec,
			prim_detected, sec_detected));

		/* If both Secondary and PRIMARY OBSS is still active , return TRUE */
		result = (sec_detected && prim_detected) ? TRUE: FALSE;
	}
	else if (priv->dyn_bwsw_enabled == OBSS_DYN_BWSW_ENAB_TXOP)
	/* TXOP  stats for OBSS detection */
	{
		result = wlc_prot_obss_bwsw_txop_detection_logic(sample_dur,
			slot_time_ifs, txop, txdur,
			priv->cfg_dbs->obss_bwsw_txop_threshold_cfg);

		if (result) {
			WL_MODE_SWITCH(("OBSS Detected TXOP: sample_dur:%d ,"
							"txop:%d,txdur:%d\n",
							sample_dur, txop, txdur));
		}
	}

	return result;
}


/*
 * Returns TRUE if OBSS interference detected. This is
 * determined by RX CRS secondary duration exceeding
 * threshold limit.
 */
bool
wlc_prot_obss_secondary_interference_detected(wlc_prot_obss_info_t *prot,
	uint8 obss_dur_threshold)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);

	uint32 rxcrs_sec40, rxcrs_sec20, limit, sample_dur;

	/* Calculate RX CRS Secondary duration */
	rxcrs_sec40 = priv->curr_stats->rxcrs_sec40 - priv->prev_stats->rxcrs_sec40;
	rxcrs_sec20 = priv->curr_stats->rxcrs_sec20 - priv->prev_stats->rxcrs_sec20;

	/* Sample duration is the TSF difference (in usec) between two reads. */
	sample_dur = priv->curr_stats->usecs - priv->prev_stats->usecs;

	/* OBSS detected if RX CRS Secondary exceeds configured limit */
	limit = (obss_dur_threshold * sample_dur) / 100;

	if ((rxcrs_sec40 + rxcrs_sec20) >= limit)
		WL_INFORM(("SEC.OBSS:sdur=%d,rxcrs_sec=%d\n",
			sample_dur, rxcrs_sec40 + rxcrs_sec20));
	return ((rxcrs_sec40 + rxcrs_sec20) >= limit) ? TRUE : FALSE;
}

/* Note: this function should be called inside the check of
*	wlc_prot_obss_interference_detected(). This is written like this because
*	2 separate actions are required for wlc_prot_obss_interference_detected and
*	wlc_prot_obss_all_subbands_interference_detected
*/
bool
wlc_prot_obss_primary_interference_detected(wlc_prot_obss_info_t *prot)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	uint32 rxcrs_pri, ibss, rxcrs_pri_limit, sample_dur;
	bool result = FALSE;

	rxcrs_pri = priv->curr_stats->rxcrs_pri -  priv->prev_stats->rxcrs_pri;
	ibss = priv->curr_stats->ibss - priv->prev_stats->ibss;

	/* Sample duration is the TSF difference (in usec) between two reads. */
	sample_dur = priv->curr_stats->usecs - priv->prev_stats->usecs;

	rxcrs_pri_limit = (priv->cfg_dbs->obss_bwsw_rx_crs_threshold_cfg * sample_dur) / 100;

	result = ((rxcrs_pri > rxcrs_pri_limit) &&
		(ibss < (rxcrs_pri * OBSS_RATIO_RXCRS_PRI_VS_IBSS_DEFAULT) / 100)) ? TRUE : FALSE;

	if (result)
		WL_INFORM(("PRI.OBSS:sample_dur:%d,rxcrs_pri:%d,"
			"rxcrs_pri_limit:%d,ibss:%d,obss:%d\n",
			sample_dur, rxcrs_pri, rxcrs_pri_limit, ibss,
			priv->curr_stats->obss - priv->prev_stats->obss));

	return result;
}

static int
wlc_prot_obss_init(void *cntxt)
{
	wlc_prot_obss_info_t *prot = (wlc_prot_obss_info_t *) cntxt;
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	if (WLC_CCASTATS_CAP(wlc)) {
		/* Seed the stats */
		wlc_bmac_obss_stats_read(wlc->hw, priv->curr_stats);
	}

	if (WLC_PROT_OBSS_ENAB(wlc->pub)) {
		/* Set secondary RSSI histogram limits */
		wlc_bmac_write_shm(wlc->hw, M_SECRSSI0_MIN,
		                   priv->config->obss_sec_rssi_lim0);
		wlc_bmac_write_shm(wlc->hw, M_SECRSSI1_MIN,
		                   priv->config->obss_sec_rssi_lim1);
	}

	return BCME_OK;
}

/*
 * OBSS protection logic. Enable OBSS protection if it detects OBSS interference.
 * Disable OBSS protection if it no longer is required.
 */
static void
wlc_prot_obss_detection(wlc_prot_obss_info_t *prot, chanspec_t chanspec)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);

	if (priv->mode != AUTO) {
		return;
	}

	if (CHSPEC_IS20(chanspec)) {
		if (WLC_PROT_OBSS_PROTECTION(prot)) {
			/* In 20 MHz, disable OBSS protection */
			wlc_prot_obss_enable(prot, FALSE);
		}

		return;
	}

	if (wlc_prot_obss_secondary_interference_detected(prot,
		priv->config->obss_dur_thres)) {
		if (!WLC_PROT_OBSS_PROTECTION(prot)) {
			/* Enable OBSS Full Protection */
			wlc_prot_obss_enable(prot, TRUE);
		}

		/* Clear inactivity timer. */
		priv->obss_inactivity = 0;
	} else {
		/* No OBSS detected. */
		if (WLC_PROT_OBSS_PROTECTION(prot)) {
			/* OBSS protection is in progress. Disable after inactivity period */
			priv->obss_inactivity++;

			if (priv->obss_inactivity >= priv->config->obss_inactivity_period) {
				/* OBSS inactivity criteria met. Disable OBSS protection */
				wlc_prot_obss_enable(prot, FALSE);
			}
		}

		return;
	}
}

static void
wlc_prot_obss_update(wlc_prot_obss_info_t *prot, chanspec_t chanspec)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	uint16 delta;
	wlc_bmac_obss_counts_t *curr = priv->curr_stats;
	wlc_bmac_obss_counts_t *prev = priv->prev_stats;
	wlc_bmac_obss_counts_t *o_total = priv->total_stats;

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		return;
	}

	BCM_REFERENCE(delta);

	/* Save a copy of previous counters */
	memcpy(prev, curr, sizeof(*prev));

	/* Read current ucode counters */
	wlc_bmac_obss_stats_read(wlc->hw, curr);

	/*
	 * Calculate the total counts.
	 */

	/* CCA stats */
	o_total->usecs += curr->usecs - prev->usecs;
	o_total->txdur += curr->txdur - prev->txdur;
	o_total->ibss += curr->ibss - prev->ibss;
	o_total->obss += curr->obss - prev->obss;
	o_total->noctg += curr->noctg - prev->noctg;
	o_total->nopkt += curr->nopkt - prev->nopkt;
	o_total->PM += curr->PM - prev->PM;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	o_total->txopp += curr->txopp - prev->txopp;
	o_total->gdtxdur += curr->gdtxdur - prev->gdtxdur;
	o_total->bdtxdur += curr->bdtxdur - prev->bdtxdur;
	o_total->slot_time_txop = curr->slot_time_txop;
#endif

#ifdef ISID_STATS
	delta = curr->crsglitch - prev->crsglitch;
	o_total->crsglitch += delta;

	delta = curr->badplcp - prev->badplcp;
	o_total->badplcp += delta;

	delta = curr->bphy_crsglitch - prev->bphy_crsglitch;
	o_total->bphy_crsglitch += delta;

	delta = curr->bphy_badplcp - prev->bphy_badplcp;
	o_total->bphy_badplcp += delta;
#endif

	if (!WLC_PROT_OBSS_ENAB(wlc->pub)) {
		return;	/* OBSS stats unsupported */
	}

	/* OBSS stats */
	delta = curr->rxdrop20s - prev->rxdrop20s;
	o_total->rxdrop20s += delta;

	delta = curr->rx20s - prev->rx20s;
	o_total->rx20s += delta;

	o_total->rxcrs_pri += curr->rxcrs_pri - prev->rxcrs_pri;
	o_total->rxcrs_sec20 += curr->rxcrs_sec20 - prev->rxcrs_sec20;
	o_total->rxcrs_sec40 += curr->rxcrs_sec40 - prev->rxcrs_sec40;

	delta = curr->sec_rssi_hist_hi - prev->sec_rssi_hist_hi;
	o_total->sec_rssi_hist_hi += delta;
	delta = curr->sec_rssi_hist_med - prev->sec_rssi_hist_med;
	o_total->sec_rssi_hist_med += delta;
	delta = curr->sec_rssi_hist_low - prev->sec_rssi_hist_low;
	o_total->sec_rssi_hist_low += delta;
}

static int
wlc_prot_obss_watchdog(void *cntxt)
{

	wlc_prot_obss_info_t *prot = (wlc_prot_obss_info_t *) cntxt;
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	if (WLC_CCASTATS_CAP(wlc)) {
		wlc_prot_obss_update(prot, wlc->chanspec);

		if (WLC_PROT_OBSS_ENAB(wlc->pub)) {
			wlc_prot_obss_detection(prot, wlc->chanspec);
		}
	}

	if (priv->dyn_bwsw_enabled)
		wlc_prot_obss_manage_bw_switch(prot);

	return BCME_OK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#define DIV_QUO(num, div) ((num)/div)  /* Return the quotient of division to avoid floats */
#define DIV_REM(num, div) (((num%div) * 10)/div) /* Return the remainder of division */

/* Format OBSS stats in string buffer. */
static void
wlc_prot_obss_stats(wlc_prot_obss_info_t *prot, struct bcmstrbuf *b)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	int slot_time_ifs;
	wlc_bmac_obss_counts_t *stats = priv->msrmnt_stored;
	uint32 time_from_driver;
	bool rate_per_sec = FALSE;

	if (stats->usecs == 0) {
		/* This means the TSF (usecs) is not updating. Just return to prevent divide by 0 */
		bcm_bprintf(b, "\nInvalid usecs 0x%x prev 0x%x curr 0x%x\n",
		            stats->usecs,
		            priv->prev_stats->usecs,
		            priv->curr_stats->usecs);
		return;
	}

	time_from_driver = stats->usecs / 1000;

	if ((time_from_driver / 1000) > 0) {
		/* Sample time > 1 sec. Show the rate/sec stats. */
		rate_per_sec = TRUE;
	}

	bcm_bprintf(b, "\nMeasured Time %d ms \n", time_from_driver);

	bcm_bprintf(b, "\nOBSS Value :             %6d ms   %-d.%-d %% \n",
	            (stats->obss) / 1000,
	            DIV_QUO(stats->obss * 100, stats->usecs),
	            DIV_REM(stats->obss * 100, stats->usecs));
	bcm_bprintf(b, "IBSS Value :             %6d ms   %-d.%-d %% \n",
	            (stats->ibss) / 1000,
	            DIV_QUO(stats->ibss * 100, stats->usecs),
	            DIV_REM(stats->ibss * 100, stats->usecs));
	bcm_bprintf(b, "TXDur Value :            %6d ms   %-d.%-d %% \n",
	            (stats->txdur) / 1000,
	            DIV_QUO(stats->txdur * 100, stats->usecs),
	            DIV_REM(stats->txdur * 100, stats->usecs));
	bcm_bprintf(b, "No Category  :           %6d ms   %-d.%-d %% \n",
	            (stats->noctg) / 1000,
	            DIV_QUO(stats->noctg * 100, stats->usecs),
	            DIV_REM(stats->noctg * 100, stats->usecs));
	bcm_bprintf(b, "No Packet :              %6d ms   %-d.%-d %% \n",
	            (stats->nopkt) / 1000,
	            DIV_QUO(stats->nopkt * 100, stats->usecs),
	            DIV_REM(stats->nopkt * 100, stats->usecs));
	bcm_bprintf(b, "MAC Sleep Time :         %6d ms   %-d.%-d %% \n",
	            (stats->PM) / 1000,
	            DIV_QUO(stats->PM * 100, stats->usecs),
	            DIV_REM(stats->PM * 100, stats->usecs));
	slot_time_ifs = ((stats->slot_time_txop >> 8) & 0xFF) +
	        (stats->slot_time_txop & 0xFF);
	bcm_bprintf(b, "TX Opp(slots):        %9d   \n", stats->txopp);
	bcm_bprintf(b, "TX Opp(time):            %6d ms   %-d.%-d %%\n",
	            (stats->txopp * slot_time_ifs) / 1000,
	            DIV_QUO(stats->txopp * slot_time_ifs * 100, stats->usecs),
	            DIV_REM(stats->txopp * slot_time_ifs * 100, stats->usecs));
	bcm_bprintf(b, "Good Tx Dur :            %6d ms   %-d.%-d %%\n",
	            stats->gdtxdur / 1000,
	            DIV_QUO(stats->gdtxdur * 100, stats->usecs),
	            DIV_REM(stats->gdtxdur * 100, stats->usecs));
	bcm_bprintf(b, "Bad Tx Dur :             %6d ms   %-d.%-d %% \n\n",
	            stats->bdtxdur / 1000,
	            DIV_QUO(stats->bdtxdur * 100, stats->usecs),
	            DIV_REM(stats->bdtxdur * 100, stats->usecs));

	if (!WLC_PROT_OBSS_ENAB(priv->wlc->pub)) {
		return;	/* OBSS stats unsupported */
	}

	/* Print OBSS stats */
	bcm_bprintf(b, "RX CRS Pri    :          %6d ms   %-d.%-d %% \n",
	            stats->rxcrs_pri / 1000,
	            DIV_QUO(stats->rxcrs_pri * 100, stats->usecs),
	            DIV_REM(stats->rxcrs_pri * 100, stats->usecs));
	bcm_bprintf(b, "RX CRS Sec 40 :          %6d ms   %-d.%-d %% \n",
	            stats->rxcrs_sec40 / 1000,
	            DIV_QUO(stats->rxcrs_sec40 * 100, stats->usecs),
	            DIV_REM(stats->rxcrs_sec40 * 100, stats->usecs));
	bcm_bprintf(b, "RX CRS Sec 20 :          %6d ms   %-d.%-d %% \n",
	            stats->rxcrs_sec20 / 1000,
	            DIV_QUO(stats->rxcrs_sec20 * 100, stats->usecs),
	            DIV_REM(stats->rxcrs_sec20 * 100, stats->usecs));
	if (rate_per_sec) {
		bcm_bprintf(b, "RX 20 Sec Drop:          %6d      %-d.%-d per sec\n",
		            stats->rxdrop20s,
		            DIV_QUO(stats->rxdrop20s * 1000, time_from_driver),
		            DIV_REM(stats->rxdrop20s * 1000, time_from_driver));
		bcm_bprintf(b, "RX 20 Sec     :          %6d      %-d.%-d per sec\n",
		            stats->rx20s,
		            DIV_QUO(stats->rx20s * 1000, time_from_driver),
		            DIV_REM(stats->rx20s * 1000, time_from_driver));
	} else {
		bcm_bprintf(b, "RX 20 Sec Drop:          %6d\n", stats->rxdrop20s);
		bcm_bprintf(b, "RX 20 Sec     :          %6d\n", stats->rx20s);
	}
	bcm_bprintf(b, "RSSI Sec Hist :    Hi : %d   Med : %d   Low : %d\n",
	            stats->sec_rssi_hist_hi,
	            stats->sec_rssi_hist_med,
	            stats->sec_rssi_hist_low);
}

static int
wlc_prot_obss_dump(wlc_prot_obss_info_t *prot, void *input, int buf_len, void *output)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	cca_msrmnt_query *q = (cca_msrmnt_query *) input;
	cca_stats_n_flags *results;

	if (!q->msrmnt_query) {
		priv->results->msrmnt_done = 0;
		wlc_prot_obss_stats_sample(prot, q->time_req);
		memset(output, 0, buf_len);
	} else {
		char *buf_ptr;
		struct bcmstrbuf b;

		results = (cca_stats_n_flags *) output;
		buf_ptr = results->buf;
		buf_len = buf_len - OFFSETOF(cca_stats_n_flags, buf);
		buf_len = (buf_len > 0) ? buf_len : 0;

		results->msrmnt_time = priv->results->msrmnt_time;
		results->msrmnt_done = priv->results->msrmnt_done;
		bcm_binit(&b, buf_ptr, buf_len);

		if (results->msrmnt_done) {
			wlc_prot_obss_stats(prot, &b);
		} else {
			bcm_bprintf(&b, "BUSY\n");
		}
	}
	return 0;
}

static int
wlc_prot_obss_dyn_bwsw_dump_statscb(wlc_info_t *wlc, void *ctx, uint32 elapsed_time, void *vstats)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(wlc->prot_obss);
	wlc_bmac_obss_counts_t *delta = (wlc_bmac_obss_counts_t *)vstats;

	memcpy(priv->msrmnt_stored, delta, sizeof(*delta));
	priv->results->msrmnt_done = 1;
	return 0;
}

static int
wlc_prot_obss_dyn_bwsw_dump(wlc_prot_obss_info_t *prot, void *input, int buf_len, void *output)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	cca_msrmnt_query *q = (cca_msrmnt_query *) input;
	cca_stats_n_flags *results;
	wlc_info_t *wlc = priv->wlc;
	wlc_bsscfg_t *cfg;
	int idx;

	if (!q->msrmnt_query) {
		priv->results->msrmnt_done = 0;
		wlc_lq_register_obss_stats_cb(priv->wlc,
			q->time_req, wlc_prot_obss_dyn_bwsw_dump_statscb, NULL);
		bzero(output, buf_len);
	} else {
		char *buf_ptr;
		struct bcmstrbuf b;

		results = (cca_stats_n_flags *) output;
		buf_ptr = results->buf;
		buf_len = buf_len - OFFSETOF(cca_stats_n_flags, buf);
		buf_len = (buf_len > 0) ? buf_len : 0;

		results->msrmnt_time = priv->results->msrmnt_time;
		results->msrmnt_done = priv->results->msrmnt_done;
		bcm_binit(&b, buf_ptr, buf_len);

		if (results->msrmnt_done) {
			wlc_prot_obss_stats(prot, &b);
		}
		FOREACH_AS_BSS(wlc, idx, cfg) {
			protobss_bsscfg_cubby_t *po_bss = PROTOBSS_BSSCFG_CUBBY(priv, cfg);

			bcm_bprintf(&b, "\n\nInterface ID=%d\n", cfg->ID);
			bcm_bprintf(&b, "Obss_activity=%d\n", po_bss->obss_activity);
			bcm_bprintf(&b, "pseudo states=%d\n", po_bss->bwswpsd_state);
			bcm_bprintf(&b, "tx overridden cspec=%x\n", po_bss->bwswpsd_cspec_for_tx);
			bcm_bprintf(&b, "# of secs of OBSS inactivity for bwswitch=%d\n",
				po_bss->obss_bwsw_no_activity_cfm_count);
			bcm_bprintf(&b, "# of secs of OBSS activity for bwswitch=%d\n",
				po_bss->obss_bwsw_activity_cfm_count);
		}

	}
	return 0;
}
#endif 

/* macro for dyn_bwsw_params */
#define DYNBWSW_CONFIG_PARAMS(field, mask, cfg_flag, rst_flag, val, def) \
(cfg_flag & mask) ? val : ((rst_flag & mask) ? def : field);


static int
wlc_prot_obss_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int val_size, struct wlc_if *wlcif)
{
	wlc_prot_obss_info_t *prot = (wlc_prot_obss_info_t *) hdl;
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	int32 int_val = 0;
	uint32 uint_val;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;
	BCM_REFERENCE(ret_int_ptr);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	uint_val = (uint)int_val;
	BCM_REFERENCE(uint_val);
	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	switch (actionid) {
	case IOV_GVAL(IOV_OBSS_PROT): {
		if (WLC_PROT_OBSS_ENAB(priv->wlc->pub)) {
			wl_config_t cfg;

			cfg.config = (uint32) priv->mode;
			if (priv->mode == AUTO) {
				cfg.status = WLC_PROT_OBSS_PROTECTION(prot);
			} else {
				cfg.status = 0;
			}
			memcpy(a, &cfg, sizeof(wl_config_t));
		} else
			err = BCME_UNSUPPORTED;
		break;
	}
	case IOV_SVAL(IOV_OBSS_PROT):
		if (WLC_PROT_OBSS_ENAB(priv->wlc->pub)) {
			/* Set OBSS protection */
			if (int_val == AUTO) {
				/* Set OBSS protection to AUTO and reset timer. */
				priv->mode = AUTO;
				priv->obss_inactivity = 0;
			} else if (int_val == OFF) {
				/* Disable OBSS protection */
				wlc_prot_obss_enable(prot, FALSE);
				priv->mode = FALSE;
			} else if (int_val == ON) {
				/* Enable OBSS protection */
				wlc_prot_obss_enable(prot, TRUE);
				priv->mode = TRUE;
			} else {
				err = BCME_BADARG;
			}
		} else
			err = BCME_UNSUPPORTED;
	        break;
	case IOV_GVAL(IOV_OBSS_DYN_BWSW_ENAB):
		*ret_int_ptr = (int32)priv->dyn_bwsw_enabled;
		break;
	case IOV_SVAL(IOV_OBSS_DYN_BWSW_ENAB):
		priv->dyn_bwsw_enabled = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_OBSS_DYN_BWSW_PARAMS): {
		obss_config_params_t *params = (obss_config_params_t *)a;
		params->version = WL_PROT_OBSS_CONFIG_PARAMS_VERSION;
		params->config_params = *(priv->cfg_dbs);
		break;
	}
	case IOV_SVAL(IOV_OBSS_DYN_BWSW_PARAMS): {
		obss_config_params_t *params = (obss_config_params_t *)p;
		wlc_prot_dynbwsw_config_t *config = priv->cfg_dbs;
		wlc_prot_dynbwsw_config_t *disp_params = &params->config_params;
		uint32 config_flags, reset_flags;

		config_flags = params->config_mask;
		reset_flags = params->reset_mask;

		if (params->version != WL_PROT_OBSS_CONFIG_PARAMS_VERSION) {
			WL_ERROR(("Driver version mismatch. expected = %d, got = %d\n",
				WL_PROT_OBSS_CONFIG_PARAMS_VERSION, params->version));
			return BCME_VERSION;
		}

		if (!priv->dyn_bwsw_enabled)
			return BCME_NOTUP;

		config->obss_bwsw_activity_cfm_count_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_activity_cfm_count_cfg,
			WL_OBSS_DYN_BWSW_FLAG_ACTIVITY_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_activity_cfm_count_cfg,
			OBSS_BWSW_ACTIVITY_CFM_PERIOD_DEFAULT);

		config->obss_bwsw_no_activity_cfm_count_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_no_activity_cfm_count_cfg,
			WL_OBSS_DYN_BWSW_FLAG_NOACTIVITY_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_no_activity_cfm_count_cfg,
			OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_DEFAULT);

		config->obss_bwsw_no_activity_cfm_count_incr_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_no_activity_cfm_count_incr_cfg,
			WL_OBSS_DYN_BWSW_FLAG_NOACTIVITY_INCR_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_no_activity_cfm_count_incr_cfg,
			OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_INCR_DEFAULT);

		config->obss_bwsw_pseudo_sense_count_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_pseudo_sense_count_cfg,
			WL_OBSS_DYN_BWSW_FLAG_PSEUDO_SENSE_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_pseudo_sense_count_cfg,
			OBSS_BWSW_PSEUDO_SENSE_PERIOD_DEFAULT);

		config->obss_bwsw_rx_crs_threshold_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_rx_crs_threshold_cfg,
			WL_OBSS_DYN_BWSW_FLAG_RX_CRS_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_rx_crs_threshold_cfg,
			OBSS_DUR_RXCRS_PRI_THRESHOLD_DEFAULT);

		config->obss_bwsw_dur_thres =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_dur_thres,
			WL_OBSS_DYN_BWSW_FLAG_DUR_THRESHOLD, config_flags, reset_flags,
			disp_params->obss_bwsw_dur_thres,
			OBSS_BWSW_DUR_THRESHOLD_DEFAULT);

		config->obss_bwsw_txop_threshold_cfg =
			DYNBWSW_CONFIG_PARAMS(config->obss_bwsw_txop_threshold_cfg,
			WL_OBSS_DYN_BWSW_FLAG_TXOP_PERIOD, config_flags, reset_flags,
			disp_params->obss_bwsw_txop_threshold_cfg,
			OBSS_TXOP_THRESHOLD_DEFAULT);
	}
	break;
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	case IOV_GVAL(IOV_OBSS_DUMP):
	        err = wlc_prot_obss_dump(prot, p, alen, a);
		break;
	case IOV_GVAL(IOV_OBSS_DYN_BWSW_DUMP):
		err = wlc_prot_obss_dyn_bwsw_dump(prot, p, alen, a);
		break;
#endif
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

wlc_prot_obss_info_t *
BCMATTACHFN(wlc_prot_obss_attach)(wlc_info_t *wlc)
{
	wlc_prot_obss_t *obss;
	wlc_prot_obss_info_t *prot;
	wlc_prot_obss_info_priv_t *priv;

	obss = (wlc_prot_obss_t *) MALLOCZ(wlc->osh, WLC_PROT_OBSS_SIZE);

	if (obss == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit,
		          __FUNCTION__,
		          MALLOCED(wlc->osh)));
		wlc->pub->_prot_obss = FALSE;
		return NULL;
	}

	prot = &obss->pub;
	priv = WLC_PROT_OBSS_INFO_PRIV(prot);

	priv->wlc = wlc;
	priv->config = &obss->config;
	priv->cfg_dbs = &obss->cfg_dbs;
	priv->prev_stats = &obss->prev_stats;
	priv->curr_stats = &obss->curr_stats;
	priv->total_stats = &obss->total_stats;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	priv->msrmnt_stored = &obss->msrmnt_stored;
	priv->results = &obss->results;
	priv->stats_timer = wl_init_timer(wlc->wl, wlc_prot_obss_stats_timeout, prot, "obss_prot");

	if (priv->stats_timer == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif 

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		priv->config->obss_sec_rssi_lim0 = OBSS_SEC_RSSI_LIM0_DEFAULT;
		priv->config->obss_sec_rssi_lim1 = OBSS_SEC_RSSI_LIM1_DEFAULT;
		priv->config->obss_inactivity_period = OBSS_INACTIVITY_PERIOD_DEFAULT;
		priv->config->obss_dur_thres = OBSS_DUR_THRESHOLD_DEFAULT;
		priv->mode = AUTO;
		wlc->pub->_prot_obss = TRUE;
	}

	/* bwsw defaults. Note: by default: disabled., Use the iovar to enable */
	priv->dyn_bwsw_enabled = OBSS_DYN_BWSW_DISABLE;
	priv->cfg_dbs->obss_bwsw_activity_cfm_count_cfg =
		OBSS_BWSW_ACTIVITY_CFM_PERIOD_DEFAULT;
	priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_cfg =
		OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_DEFAULT;
	priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_incr_cfg =
		OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_INCR_DEFAULT;
	priv->cfg_dbs->obss_bwsw_pseudo_sense_count_cfg =
		OBSS_BWSW_PSEUDO_SENSE_PERIOD_DEFAULT;
	priv->cfg_dbs->obss_bwsw_rx_crs_threshold_cfg =
		OBSS_DUR_RXCRS_PRI_THRESHOLD_DEFAULT;
	priv->cfg_dbs->obss_bwsw_dur_thres =
		OBSS_BWSW_DUR_THRESHOLD_DEFAULT;
	priv->cfg_dbs->obss_bwsw_txop_threshold_cfg =
		OBSS_TXOP_THRESHOLD_DEFAULT;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((priv->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(protobss_bsscfg_cubby_t *),
	                wlc_prot_obss_bssinfo_init, wlc_prot_obss_bssinfo_deinit, NULL,
	                (void *)prot)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, wlc_prot_obss_iovars,
	                        "prot_obss", prot, wlc_prot_obss_doiovar,
	                        wlc_prot_obss_watchdog, wlc_prot_obss_init, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub)) {
		if ((wlc->modesw) && (wlc_modesw_notif_cb_register(wlc->modesw,
			wlc_prot_obss_modesw_cb, prot) != BCME_OK)) {
			/* if call bk fails, it just continues, but the dyn_bwsw
			* will be disabled.
			*/
			WL_ERROR(("%s: modesw notif callbk failed, but continuing\n",
			          __FUNCTION__));
			priv->dyn_bwsw_enabled = OBSS_DYN_BWSW_DISABLE;
		}
	} else
#endif /* WL_MODESW */
	{
		priv->dyn_bwsw_enabled = OBSS_DYN_BWSW_DISABLE;
	}
	return prot;
fail:
	ASSERT(1);
	if (prot)
		MFREE(wlc->osh, obss, WLC_PROT_OBSS_SIZE);
	wlc->pub->_prot_obss = FALSE;
	return NULL;
}

void
BCMATTACHFN(wlc_prot_obss_detach)(wlc_prot_obss_info_t *prot)
{
	wlc_prot_obss_info_priv_t *priv;
	wlc_info_t *wlc;

	if (!prot)
		return;

	priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc = priv->wlc;
	wlc->pub->_prot_obss = FALSE;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wl_free_timer(wlc->wl, priv->stats_timer);
#endif 

	wlc_module_unregister(wlc->pub, "prot_obss", prot);

#ifdef WL_MODESW
	if (WLC_MODESW_ENAB(wlc->pub)) {
		if (wlc_modesw_notif_cb_unregister(wlc->modesw,
			wlc_prot_obss_modesw_cb, prot) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_modesw_notif_cb_unregister() failed\n",
				wlc->pub->unit, __FUNCTION__));
		}
	}
#endif /* WL_MODESW */
	MFREE(wlc->osh, prot, WLC_PROT_OBSS_SIZE);
}

static int
wlc_prot_obss_bssinfo_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(ctx);
	wlc_info_t *wlc = priv->wlc;
	protobss_bsscfg_cubby_t **ppo_bss = PROTOBSS_BSSCFG_CUBBY_LOC(priv, cfg);
	protobss_bsscfg_cubby_t *po_bss = NULL;

	/* allocate memory and point bsscfg cubby to it */
	if ((po_bss = MALLOC(wlc->osh, sizeof(protobss_bsscfg_cubby_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bzero((char *)po_bss, sizeof(protobss_bsscfg_cubby_t));
	po_bss->orig_chanspec = INVCHANSPEC;
	po_bss->bwswpsd_cspec_for_tx = INVCHANSPEC;
	*ppo_bss = po_bss;
	return BCME_OK;
}

static void
wlc_prot_obss_bssinfo_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(ctx);
	wlc_info_t *wlc = priv->wlc;
	protobss_bsscfg_cubby_t **ppo_bss = PROTOBSS_BSSCFG_CUBBY_LOC(priv, cfg);
	protobss_bsscfg_cubby_t *po_bss = NULL;

	/* free the Cubby reserve allocated memory  */
	po_bss = *ppo_bss;
	if (po_bss) {
		/* if some obss is still active, but we are releasing bsscfg, reduce
		* the global obss active count
		*/
		if (po_bss->obss_activity != OBSS_INACTIVE) {
			priv->obss_active_cnt--;
		}

		MFREE(wlc->osh, po_bss, sizeof(protobss_bsscfg_cubby_t));
		*ppo_bss = NULL;
	}
}

static int
wlc_prot_obss_manage_bw_switch(wlc_prot_obss_info_t *prot)
{
	int idx;
	wlc_bsscfg_t *cfg;
	protobss_bsscfg_cubby_t *po_bss = NULL;
	int err = BCME_OK;
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	/* if both secondary and primary are interfering */
	if (wlc_prot_obss_interference_detected_for_bwsw(prot, NULL)) {
		chanspec_t original_chspec;
		FOREACH_AS_BSS(wlc, idx, cfg) {
			po_bss = PROTOBSS_BSSCFG_CUBBY(priv, cfg);
			WL_MODE_SWITCH(("cfg:%p, bwswpsd:%d\n", cfg,
				po_bss->bwswpsd_state));
			if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_NOTACTIVE)
			{
				po_bss->obss_bwsw_activity_cfm_count++;
			}
			po_bss->obss_bwsw_no_activity_cfm_count = 0;
			WL_MODE_SWITCH(("%s: IF+: %d of %d; \n", __FUNCTION__,
				po_bss->obss_bwsw_activity_cfm_count,
				priv->cfg_dbs->obss_bwsw_activity_cfm_count_cfg));

			if (po_bss->obss_bwsw_activity_cfm_count <
				priv->cfg_dbs->obss_bwsw_activity_cfm_count_cfg) {
				continue;
			}

			/* pseudoug active/pending, don't do anything, let the
			* timer decide
			*/
			if (po_bss->bwswpsd_state != BWSW_PSEUDOUPGD_NOTACTIVE) {
				continue;
			}

			original_chspec = cfg->current_bss->chanspec;
			WL_MODE_SWITCH(("Orig Chanspec = %x\n", original_chspec));
#ifdef WL_MODESW
			if (WLC_MODESW_ENAB(wlc->pub))
				err = wlc_modesw_bw_switch(wlc->modesw, wlc->chanspec,
					BW_SWITCH_TYPE_DNGRADE, cfg, 0);
			else
				err = BCME_UNSUPPORTED;
#endif /* WL_MODESW */

			po_bss->obss_bwsw_no_activity_cfm_count = 0;

			if (err == BCME_OK) {
				/* If this is the first downg, store the orig BW
				* for use later.
				*/
				if (po_bss->obss_activity == OBSS_INACTIVE) {
					po_bss->orig_chanspec = original_chspec;
					po_bss->obss_bwsw_no_activity_cfm_count_max =
						priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_cfg;
				}
			}
			po_bss->obss_bwsw_activity_cfm_count = 0;
		}
	} else {
		/* OBSS POSSIBLY GONE NOW... if any one of the cfg's were affected? */
		if (priv->obss_active_cnt == 0) {
			return BCME_OK;
		}

		FOREACH_BSS(wlc, idx, cfg) {
			if (!PROTOBSS_BSS_MATCH(wlc, cfg)) {
				WL_INFORM(("BSS_NOT_MATCHED\n"));
				continue;
			}

			po_bss = PROTOBSS_BSSCFG_CUBBY(priv, cfg);
			po_bss->obss_bwsw_no_activity_cfm_count++;
			po_bss->obss_bwsw_activity_cfm_count = 0;
			WL_MODE_SWITCH(("%s:IF-:%d.of.%d;oacnt:%d\n", __FUNCTION__,
				po_bss->obss_bwsw_no_activity_cfm_count,
				po_bss->obss_bwsw_no_activity_cfm_count_max,
				priv->obss_active_cnt));
			if (po_bss->obss_bwsw_no_activity_cfm_count <
				po_bss->obss_bwsw_no_activity_cfm_count_max) {
				continue;
			}

			if (po_bss->bwswpsd_state ==
				BWSW_PSEUDOUPGD_NOTACTIVE) {
				chanspec_t prsnt_chspec = wlc->chanspec;
				po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_PENDING;
#ifdef WL_MODESW
				if (WLC_MODESW_ENAB(wlc->pub))
					err = wlc_modesw_bw_switch(wlc->modesw,
						wlc->chanspec, BW_SWITCH_TYPE_UPGRADE,
						cfg,
						MODESW_CTRL_UP_SILENT_UPGRADE);
				else
					err = BCME_UNSUPPORTED;
#endif /* WL_MODESW */
				if (err == BCME_OK) {
					/* save prsnt cspec to come bk if req */
					po_bss->bwswpsd_cspec_for_tx =
						prsnt_chspec;
					WL_MODE_SWITCH(("pseudo:UP:txchanSpec:x%x!,"
						"pNOTACTIVE->pPENDING\n",
						po_bss->bwswpsd_cspec_for_tx));
				} else {
					/* Since pseudo up failed, reset to pseudo not active and
					* reset no activity count to zero and let it start again
					*/
					WL_MODE_SWITCH(("pseudo:UP:pst:%d,ERR!\n",
						po_bss->bwswpsd_state));
					po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_NOTACTIVE;
					po_bss->obss_bwsw_no_activity_cfm_count_max =
						priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_cfg;
				}
				po_bss->obss_bwsw_no_activity_cfm_count = 0;
			}
		}
	}
	return BCME_OK;
}

/* used by external world to get actual BW for tx, if BW is in pseudo state */
void
wlc_prot_obss_tx_bw_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, uint32 *rspec_bw)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	protobss_bsscfg_cubby_t *po_bss;

	po_bss = PROTOBSS_BSSCFG_CUBBY(priv, bsscfg);

	if (po_bss->bwswpsd_cspec_for_tx != INVCHANSPEC) {
		*rspec_bw = chspec_to_rspec(CHSPEC_BW(po_bss->bwswpsd_cspec_for_tx));
		WL_MODE_SWITCH(("BWSW:chspecORd:st:%d, chanspec:%x,rspec_bw:%x\n",
			po_bss->bwswpsd_state,
			po_bss->bwswpsd_cspec_for_tx,
			*rspec_bw));
	}
}

#ifdef WL_MODESW

static int
wlc_prot_obss_bwswpsd_statscb(wlc_info_t *wlc, void *ctx, uint32 elapsed_time, void *vstats)
{
	wlc_po_cb_ctx_t *cb_ctx = (wlc_po_cb_ctx_t *)ctx;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_ID(wlc, cb_ctx->connID);
	protobss_bsscfg_cubby_t *po_bss;
	wlc_bmac_obss_counts_t *stats = (wlc_bmac_obss_counts_t *)vstats;
	int err = BCME_OK;
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(wlc->prot_obss);

	WL_INFORM(("%s:enter\n", __FUNCTION__));
	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, bsscfg));
		return BCME_ERROR;
	}

	po_bss = PROTOBSS_BSSCFG_CUBBY(priv, bsscfg);

	if (po_bss->bwswpsd_state != BWSW_PSEUDOUPGD_ACTIVE) {
		/* just return */
		return BCME_OK;
	}

	/* is interference still present in higher BW's? */
	if (wlc_prot_obss_interference_detected_for_bwsw(wlc->prot_obss, stats)) {
		/* interference still present... silently dngrade */
		po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_PENDING;
		if ((err = wlc_modesw_bw_switch(wlc->modesw,
			po_bss->bwswpsd_cspec_for_tx, BW_SWITCH_TYPE_DNGRADE, bsscfg,
			MODESW_CTRL_DN_SILENT_DNGRADE)) ==
			BCME_OK) {
			WL_MODE_SWITCH(("pseudo:statscb:INTF_STILL_PRESENT!so,"
				"DN:txchanSpec:x%x!, pACTIVE->pPENDING\n",
				po_bss->bwswpsd_cspec_for_tx));
		} else {
			WL_MODE_SWITCH(("BWSW:statscb:DN:pst:%d, ERROR!\n",
				po_bss->bwswpsd_state));
			po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_NOTACTIVE;
		}
	} else {
		/* upgrade. TBD: a counter required? */
		if ((err = wlc_modesw_bw_switch(wlc->modesw,
			wlc->chanspec, BW_SWITCH_TYPE_UPGRADE, bsscfg,
			MODESW_CTRL_UP_ACTION_FRAMES_ONLY)) ==
			BCME_OK) {
				WL_MODE_SWITCH(("pseudo:statscb:INTF_GONE!so,UP:txchanSpec:"
					"x%x! actionsending!\n",
					po_bss->bwswpsd_cspec_for_tx));
		} else {
			WL_MODE_SWITCH(("BWSW:statscb:DN:pst:%d, ERROR!\n",
				po_bss->bwswpsd_state));
		}
	}
	MFREE(priv->wlc->osh, cb_ctx, sizeof(wlc_po_cb_ctx_t));
	return err;
}

static int
wlc_prot_obss_init_psdstat_cb(wlc_prot_obss_info_priv_t *priv, wlc_bsscfg_t *cfg)
{
	wlc_po_cb_ctx_t *cb_ctx = NULL;

	cb_ctx = (wlc_po_cb_ctx_t *)MALLOCZ(priv->wlc->osh,
		sizeof(wlc_po_cb_ctx_t));

	if (!cb_ctx) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			priv->wlc->pub->unit, __FUNCTION__,
			MALLOCED(priv->wlc->osh)));
		return BCME_NOMEM;
	}
	cb_ctx->connID = cfg->ID;

	WL_MODE_SWITCH(("%s:register_lq_cb!\n", __FUNCTION__));

	return wlc_lq_register_obss_stats_cb(priv->wlc,
		priv->cfg_dbs->obss_bwsw_pseudo_sense_count_cfg,
		wlc_prot_obss_bwswpsd_statscb, cb_ctx);
}

static void
wlc_prot_obss_set_obss_activity_info(wlc_prot_obss_info_priv_t *priv,
	wlc_bsscfg_t * bsscfg, protobss_bsscfg_cubby_t *po_bss)
{
	if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_NOTACTIVE) {
		WL_MODE_SWITCH(("\n orig = %x curr = %x\n",
			po_bss->orig_chanspec, priv->wlc->chanspec));
		if (priv->wlc->chanspec == po_bss->orig_chanspec) {
			po_bss->obss_activity = OBSS_INACTIVE;
			po_bss->orig_chanspec = INVCHANSPEC;
			priv->obss_active_cnt--;
			WL_INFORM(("%s:oa:%d\n", __FUNCTION__, priv->obss_active_cnt));
			WL_MODE_SWITCH(("%s:OBSS inactive\n", __FUNCTION__));
		} else {
				po_bss->obss_activity =
					wlc_modesw_is_connection_vht(priv->wlc, bsscfg) ?
					OBSS_ACTIVE_NONHT : OBSS_ACTIVE_HT;
				WL_MODE_SWITCH(("Value of the actvity in %s = %d\n",
					__FUNCTION__, po_bss->obss_activity));
		}
	}
}

static void
wlc_prot_obss_modesw_cb(void *ctx, wlc_modesw_notif_cb_data_t *notif_data)
{
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(ctx);
	protobss_bsscfg_cubby_t *po_bss = PROTOBSS_BSSCFG_CUBBY(priv, notif_data->cfg);

	WL_MODE_SWITCH(("%s: signal:%d\n", __FUNCTION__, notif_data->signal));

	switch (notif_data->signal) {
	case (MODESW_PHY_UP_COMPLETE):
		WL_MODE_SWITCH(("GOt the PHY_UP status"));
		if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_PENDING) {
			po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_ACTIVE;
			WL_MODE_SWITCH(("pseudo:UP:pPENDING->pACTIVE, NOT sending "
				"ACTION\n"));
			/* register callback for the stats to come for full Bw.
			* TBD: also check return value & act
			*/
			if (BCME_OK != wlc_prot_obss_init_psdstat_cb(priv, notif_data->cfg))
				WL_ERROR(("%s: failed to register psd cb!\n",
					__FUNCTION__));
		}
		break;
	case (MODESW_DN_AP_COMPLETE):
		WL_MODE_SWITCH(("Caught Signal = MODESW_DN_AP_COMPLETE\n"));
		WL_MODE_SWITCH(("value of the psd state = %d\n",
			po_bss->bwswpsd_state));
		WL_MODE_SWITCH(("Value of obss activity = %d\n",
			po_bss->obss_activity));
	/* Fall through */
	case (MODESW_DN_STA_COMPLETE):
		if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_PENDING) {
			po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_NOTACTIVE;
			WL_MODE_SWITCH(("pseudo:DN:NOACT:pPENDING->pNOTACTIVE"
				"oc:%x, cs:%x, txcspec:%x\n",
				po_bss->orig_chanspec, priv->wlc->chanspec,
				po_bss->bwswpsd_cspec_for_tx));
				po_bss->bwswpsd_cspec_for_tx = INVCHANSPEC;
			/* increment max number of seconds to find the OBSS gone */
			if ((po_bss->obss_bwsw_no_activity_cfm_count_max +
				priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_incr_cfg) <
				OBSS_BWSW_NO_ACTIVITY_MAX_INCR_DEFAULT)
				po_bss->obss_bwsw_no_activity_cfm_count_max +=
					priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_incr_cfg;
		} else {
			/* reset counter which used to detect that obss is gone */
			po_bss->obss_bwsw_no_activity_cfm_count_max =
				priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_cfg;
		}
		if (po_bss->obss_activity == OBSS_INACTIVE) {
			wlc_prot_obss_set_obss_activity_info(priv, notif_data->cfg,
				po_bss);
			priv->obss_active_cnt++;
		}
		break;
	case (MODESW_ACTION_FAILURE):
		/* TBD reset pseudo states */
		if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_ACTIVE) {
			po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_NOTACTIVE;
			WL_MODE_SWITCH(("pseudo:ACTTimeout:st:pACTIVE->pNOTACTIVE:\n"));
			po_bss->bwswpsd_cspec_for_tx = INVCHANSPEC;
		} else {
			WL_MODE_SWITCH(("ACT_COMPLETE:badevt in pseudo state:%d,evt:%d!\n",
				po_bss->bwswpsd_state, notif_data->signal));
		}
		break;
	case (MODESW_UP_AP_COMPLETE) :
		WL_MODE_SWITCH(("Caught Signal = MODESW_UP_AP_COMPLETE\n"));
		WL_MODE_SWITCH(("value of the psd state = %d\n",
			po_bss->bwswpsd_state));
		WL_MODE_SWITCH(("Value of obss activity = %d\n",
			po_bss->obss_activity));
	/* Fall through */
	case (MODESW_UP_STA_COMPLETE):
		if (po_bss->bwswpsd_state == BWSW_PSEUDOUPGD_ACTIVE) {
			WL_MODE_SWITCH(("pseudo:ACTCmplt:st:pACTIVE->"
				"pNOTACTIVE:\n"));
			po_bss->bwswpsd_state = BWSW_PSEUDOUPGD_NOTACTIVE;
		} else {
			WL_MODE_SWITCH(("ACT_COMPLETE:non-pseudo"
				"UP-done.,state:%d!,evt:%d\n",
				po_bss->bwswpsd_state, notif_data->signal));
		}
		wlc_prot_obss_set_obss_activity_info(priv, notif_data->cfg,
			po_bss);
		po_bss->bwswpsd_cspec_for_tx = INVCHANSPEC;
		/* reset counter which used to detect that obss is gone */
		po_bss->obss_bwsw_no_activity_cfm_count_max =
			priv->cfg_dbs->obss_bwsw_no_activity_cfm_count_cfg;
		break;
	}
}

#endif /* WL_MODESW */

chanspec_t
wlc_prot_obss_ht_chanspec_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, chanspec_t beacon_chanspec)
{
#ifdef WL_MODESW
	wlc_prot_obss_info_priv_t *priv = WLC_PROT_OBSS_INFO_PRIV(prot);
	protobss_bsscfg_cubby_t *po_bss = PROTOBSS_BSSCFG_CUBBY(priv, bsscfg);
	uint16 bw = WL_CHANSPEC_BW_20;
	uint16 bw_bcn = CHSPEC_BW(beacon_chanspec);
	uint8 ctl_chan = 0;

	if (WLC_MODESW_ENAB(priv->wlc->pub)) {
		if (po_bss->obss_activity == OBSS_ACTIVE_HT) {
				bw = wlc_modesw_get_bw_from_opermode(bsscfg->oper_mode);
			bw = MIN(bw, bw_bcn);

			/* if bw is 20 and bw_bcn is 40, we need to get ctrl channel
			 * and use it for making chanspec
			 */
			if (bw == WL_CHANSPEC_BW_20) {
				ctl_chan = wf_chspec_ctlchan(beacon_chanspec);
				return CH20MHZ_CHSPEC(ctl_chan);
			}
		}
	}
#endif /* WL_MODESW */
	return beacon_chanspec;
}
