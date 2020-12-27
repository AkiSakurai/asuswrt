/**
 * @file
 * @brief
 * Code that controls the link quality
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_lq.c 467328 2014-04-03 01:23:40Z $
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
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_lq.h>
#ifdef WLOFFLD
#include <wlc_offloads.h>
#endif

/* iovar table */
enum {
	IOV_RSSI_ANT,
	IOV_SNR,
	IOV_RSSI_EVENT,
	IOV_RSSI_WINDOW_SZ,
	IOV_SNR_WINDOW_SZ,
	IOV_LINKQUAL_ONOFF,
	IOV_GET_LINKQUAL_STATS,
	IOV_CHANIM_ENAB,
	IOV_CHANIM_STATE, /* chan interference detect */
	IOV_CHANIM_MODE,
	IOV_CHANIM_STATS, /* the chanim stats */
	IOV_CCASTATS_THRES,
	IOV_CRSGLITCH_THRES, /* chan interference threshold */
	IOV_BGNOISE_THRES,  /* background noise threshold */
	IOV_SAMPLE_PERIOD,
	IOV_THRESHOLD_TIME,
	IOV_MAX_ACS,
	IOV_LOCKOUT_PERIOD,
	IOV_ACS_RECORD,
	IOV_LQ_LAST
};

static const bcm_iovar_t wlc_lq_iovars[] = {
	{"rssi_event", IOV_RSSI_EVENT,
	(0), IOVT_BUFFER, sizeof(wl_rssi_event_t)
	},
#ifdef STA
	{"rssi_win", IOV_RSSI_WINDOW_SZ,
	(0), IOVT_UINT16, 0
	},
	{"snr_win", IOV_SNR_WINDOW_SZ,
	(0), IOVT_UINT16, 0
	},
#endif /* STA */

	{"phy_rssi_ant", IOV_RSSI_ANT,
	(0), IOVT_BUFFER, sizeof(wl_rssi_ant_t)
	},
	{"snr", IOV_SNR,
	(0), IOVT_INT32, 0
	},


	{"chanim_enab", IOV_CHANIM_ENAB,
	(0), IOVT_UINT32, 0
	},
#ifdef WLCHANIM
	{"chanim_state", IOV_CHANIM_STATE,
	(0), IOVT_BOOL, 0
	},
	{"chanim_mode", IOV_CHANIM_MODE,
	(0), IOVT_UINT8, 0
	},
	{"chanim_ccathres", IOV_CCASTATS_THRES,
	(0), IOVT_UINT8, 0
	},
	{"chanim_glitchthres", IOV_CRSGLITCH_THRES,
	(0), IOVT_UINT32, 0
	},
	{"chanim_bgnoisethres", IOV_BGNOISE_THRES,
	(0), IOVT_INT8, 0
	},
	{"chanim_sample_period", IOV_SAMPLE_PERIOD,
	(0), IOVT_UINT8, 0
	},
	{"chanim_threshold_time", IOV_THRESHOLD_TIME,
	(0), IOVT_UINT8, 0
	},
	{"chanim_max_acs", IOV_MAX_ACS,
	(0), IOVT_UINT8, 0
	},
	{"chanim_lockout_period", IOV_LOCKOUT_PERIOD,
	(0), IOVT_UINT32, 0
	},
	{"chanim_acs_record", IOV_ACS_RECORD,
	(0), IOVT_BUFFER, sizeof(wl_acs_record_t),
	},
	{"chanim_stats", IOV_CHANIM_STATS,
	(0), IOVT_BUFFER, sizeof(wl_chanim_stats_t),
	},
#endif /* WLCHANIM */
	{NULL, 0, 0, 0, 0}
};

struct lq_info {
	wlc_info_t	*wlc;			/* pointer to main wlc structure */
	wlc_pub_t	*pub;			/* public common code handler */
};

static int wlc_lq_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *arg, int alen, int val_size, struct wlc_if *wlcif);

static void wlc_lq_rssi_ant_get(wlc_info_t *wlc, int8 *rssi);
static void wlc_lq_rssi_event_timeout(void *arg);

#ifdef WLCHANIM
static wlc_chanim_stats_t *wlc_lq_chanim_create_stats(wlc_info_t *wlc, chanspec_t chanspec);
static int wlc_lq_chanim_get_stats(chanim_info_t *c_info, wl_chanim_stats_t* iob, int *len, int);
static wlc_chanim_stats_t *wlc_lq_chanim_find_stats(wlc_info_t *wlc, chanspec_t chanspec);
static int wlc_lq_chanim_get_acs_record(chanim_info_t *c_info, int buf_len, void *output);
static void wlc_lq_chanim_insert_stats(wlc_chanim_stats_t **rootp, wlc_chanim_stats_t *new);
static void wlc_lq_chanim_meas(wlc_info_t *wlc, chanim_cnt_t *chanim_cnt);
static void wlc_lq_chanim_glitch_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_badplcp_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_ccastats_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt,
	chanim_accum_t *acc);
static void wlc_lq_chanim_accum(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t *acc);
static void wlc_lq_chanim_clear_acc(wlc_info_t* wlc, chanim_accum_t* acc);
static int8 wlc_lq_chanim_phy_noise(wlc_info_t *wlc);
static void wlc_lq_chanim_close(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t* acc);
static bool wlc_lq_chanim_interfered_glitch(wlc_chanim_stats_t *stats, uint32 thres);
static bool wlc_lq_chanim_interfered_cca(wlc_chanim_stats_t *stats, uint32 thres);
static bool wlc_lq_chanim_interfered_noise(wlc_chanim_stats_t *stats, int8 thres);

#ifdef BCMDBG
static void wlc_lq_chanim_display(wlc_info_t *wlc, chanspec_t chanspec);
#endif

#endif /* WLCHANIM */

#ifdef BCMDBG
static void wlc_dump_lq(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif

#if defined(BCMDBG) || defined(WLTEST)
static int wlc_dump_rssi(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif

#ifdef WLCQ
static int wlc_lq_channel_qa_eval(wlc_info_t *wlc);
static void wlc_lq_channel_qa_sample_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm);
#endif /* WLCQ */


/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>


int
BCMATTACHFN(wlc_lq_attach)(wlc_info_t *wlc)
{
#ifdef WLCHANIM
	chanim_info_t *c_info;
	c_info = wlc->chanim_info;

	ASSERT(wlc->chanim_info != NULL);
#endif
	/* register module */
	if (wlc_module_register(wlc->pub, wlc_lq_iovars, "lq", wlc, wlc_lq_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		return -1;
	}

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "lq", (dump_fn_t)wlc_dump_lq, (void *)wlc);
#endif

#if defined(BCMDBG) || defined(WLTEST)
	wlc_dump_register(wlc->pub, "rssi", (dump_fn_t)wlc_dump_rssi, (void *)wlc);
#endif


#ifdef WLCHANIM
	c_info->config.crsglitch_thres = CRSGLITCH_THRESHOLD_DEFAULT;
	c_info->config.ccastats_thres = CCASTATS_THRESHOLD_DEFAULT;
	c_info->config.bgnoise_thres = BGNOISE_THRESHOLD_DEFAULT;
	c_info->config.mode = CHANIM_DETECT;
	c_info->config.sample_period = SAMPLE_PERIOD_DEFAULT;
	c_info->config.threshold_time = THRESHOLD_TIME_DEFAULT;
	c_info->config.lockout_period = LOCKOUT_PERIOD_DEFAULT;
	c_info->config.max_acs = MAX_ACS_DEFAULT;
	c_info->config.scb_max_probe = CHANIM_SCB_MAX_PROBE;

	c_info->stats = NULL;
#endif /* WLCHANIM */

	return 0;
}

void
BCMATTACHFN(wlc_lq_detach)(wlc_info_t *wlc)
{
	wlc_module_unregister(wlc->pub, "lq", wlc);
}

static int
wlc_lq_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int val_size, struct wlc_if *wlcif)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int32 int_val = 0;
	uint32 uint_val;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	wlc = (wlc_info_t *)hdl;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	uint_val = (uint)int_val;
	BCM_REFERENCE(uint_val);
	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	switch (actionid) {
	case IOV_GVAL(IOV_RSSI_ANT): {
		wl_rssi_ant_t rssi_ant;

		bzero((char *)&rssi_ant, sizeof(wl_rssi_ant_t));
		rssi_ant.version = WL_RSSI_ANT_VERSION;

		/* only get RSSI for one antenna for all SISO PHY */
		if (WLCISAPHY(wlc->band) || WLCISGPHY(wlc->band) || WLCISLPPHY(wlc->band) ||
			WLCISLCNPHY(wlc->band))
		{
			rssi_ant.count = 1;
			rssi_ant.rssi_ant[0] = (int8)(bsscfg->link->rssi);

		} else if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) {
			int8 rssi[WL_RSSI_ANT_MAX] = {0, 0, 0, 0};
			uint8 i;

			wlc_lq_rssi_ant_get(wlc, rssi);
			rssi_ant.count = (WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) ?
			    WL_ANT_HT_RX_MAX : WL_ANT_RX_MAX;

			for (i = WL_ANT_IDX_1; i < rssi_ant.count; i++) {
				if (wlc->stf->rxchain & (1 << i))
					rssi_ant.rssi_ant[i] = rssi[i];
			}
		}
		else {
			rssi_ant.count = 0;
		}

		bcopy(&rssi_ant, a, sizeof(wl_rssi_ant_t));
		break;
	}

	case IOV_GVAL(IOV_RSSI_EVENT):
		memcpy(a, bsscfg->link->rssi_event, sizeof(wl_rssi_event_t));
		break;

	case IOV_SVAL(IOV_RSSI_EVENT): {
		wlc_link_qual_t *link = bsscfg->link;
		if (link->rssi_event_timer) {
			wl_del_timer(wlc->wl, link->rssi_event_timer);
			wl_free_timer(wlc->wl, link->rssi_event_timer);
			link->rssi_event_timer = NULL;
		}
		link->is_rssi_event_timer_active = FALSE;
		link->rssi_level = 0; 	/* reset current rssi level */
		memcpy(link->rssi_event, a, sizeof(wl_rssi_event_t));
		if (link->rssi_event->rate_limit_msec) {
			link->rssi_event_timer = wl_init_timer(wlc->wl,
				wlc_lq_rssi_event_timeout, bsscfg, "rssi_event");
		}
		break;
	}

#ifdef STA
	case IOV_SVAL(IOV_RSSI_WINDOW_SZ):
		if (((int_val & RSSI_PKT_WIN_SZ_MASK) == 0) ||
		    ((int_val & RSSI_PKT_WIN_SZ_MASK) > MA_WINDOW_SZ)) {
			err = BCME_RANGE;
			break;
		}

		if ((int_val & (int_val - 1) & RSSI_PKT_WIN_SZ_MASK) != 0) {
			/* Value passed is not power of 2 */
			err = BCME_BADARG;
			break;
		}
		bsscfg->link->rssi_pkt_win_sz = (uint16)int_val;
		wlc_lq_rssi_reset_ma(bsscfg, bsscfg->link->rssi);
		wlc_lq_rssi_snr_noise_reset_ma(wlc, bsscfg, bsscfg->link->rssi,
			bsscfg->link->snr, wlc->noise_lvl);
		break;

	case IOV_GVAL(IOV_RSSI_WINDOW_SZ):
		*ret_int_ptr = (int32)bsscfg->link->rssi_pkt_win_sz;
		break;

	case IOV_SVAL(IOV_SNR_WINDOW_SZ):
		if (((int_val & RSSI_PKT_WIN_SZ_MASK) == 0) ||
		    ((int_val & RSSI_PKT_WIN_SZ_MASK) > MA_WINDOW_SZ)) {
			err = BCME_RANGE;
			break;
		}

		if ((int_val & (int_val - 1) & RSSI_PKT_WIN_SZ_MASK) != 0) {
			/* Value passed is not power of 2 */
			err = BCME_BADARG;
			break;
		}
		bsscfg->link->snr_pkt_win_sz = (uint16)int_val;
		wlc_lq_rssi_snr_noise_reset_ma(wlc, bsscfg, bsscfg->link->rssi,
			bsscfg->link->snr, wlc->noise_lvl);
		break;

	case IOV_GVAL(IOV_SNR_WINDOW_SZ):
		*ret_int_ptr = (int32)bsscfg->link->snr_pkt_win_sz;
		break;
#endif /* STA */

	case IOV_GVAL(IOV_SNR):
		*ret_int_ptr = (int32)bsscfg->link->snr;
		break;


	case IOV_GVAL(IOV_CHANIM_ENAB):
		*ret_int_ptr = (int32)WLC_CHANIM_ENAB(wlc);
		break;

#ifdef WLCHANIM
	case IOV_GVAL(IOV_CHANIM_STATE): {
		chanspec_t chspec;
		wlc_chanim_stats_t *stats;

		if (plen < (int)sizeof(int)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		chspec = (chanspec_t) int_val;

		if (wf_chspec_malformed(chspec)) {
			err = BCME_BADCHAN;
			break;
		}

		if (chspec != wlc->home_chanspec)
			stats = wlc_lq_chanim_chanspec_to_stats(wlc->chanim_info, chspec);
		else
			stats = &wlc->chanim_info->cur_stats;

		if (!stats) {
			err = BCME_RANGE;
			break;
		}

		if (WLC_CHANIM_MODE_EXT(wlc->chanim_info))
			*ret_int_ptr = (int32) chanim_mark(wlc->chanim_info).state;
		else
			*ret_int_ptr = (int32) wlc_lq_chanim_interfered(wlc, chspec);

		break;
	}

	case IOV_SVAL(IOV_CHANIM_STATE):
		if (WLC_CHANIM_MODE_EXT(wlc->chanim_info))
			chanim_mark(wlc->chanim_info).state = (bool)int_val;
		break;

	case IOV_GVAL(IOV_CHANIM_MODE):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).mode;
		break;

	case IOV_GVAL(IOV_SAMPLE_PERIOD):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).sample_period;
		break;

	case IOV_SVAL(IOV_SAMPLE_PERIOD):
		if (int_val < SAMPLE_PERIOD_MIN)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).sample_period = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_CHANIM_MODE):
		if (int_val >= CHANIM_MODE_MAX) {
			err = BCME_RANGE;
			break;
		}

		chanim_config(wlc->chanim_info).mode = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_CCASTATS_THRES):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).ccastats_thres;
		break;

	case IOV_SVAL(IOV_CCASTATS_THRES):
		chanim_config(wlc->chanim_info).ccastats_thres = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_CRSGLITCH_THRES):
		*ret_int_ptr = chanim_config(wlc->chanim_info).crsglitch_thres;
		break;

	case IOV_SVAL(IOV_CRSGLITCH_THRES):
		chanim_config(wlc->chanim_info).crsglitch_thres = int_val;
		break;

	case IOV_GVAL(IOV_BGNOISE_THRES):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).bgnoise_thres;
		break;

	case IOV_SVAL(IOV_BGNOISE_THRES):
		chanim_config(wlc->chanim_info).bgnoise_thres = (int8)int_val;
		break;

	case IOV_GVAL(IOV_THRESHOLD_TIME):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).threshold_time;
		break;

	case IOV_SVAL(IOV_THRESHOLD_TIME):
		if (int_val < THRESHOLD_TIME_MIN)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).threshold_time = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_MAX_ACS):
		*ret_int_ptr = (int32)chanim_config(wlc->chanim_info).max_acs;
		break;

	case IOV_SVAL(IOV_MAX_ACS):
		if (int_val > CHANIM_ACS_RECORD)
			err = BCME_RANGE;
		chanim_config(wlc->chanim_info).max_acs = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_LOCKOUT_PERIOD):
		*ret_int_ptr = chanim_config(wlc->chanim_info).lockout_period;
		break;

	case IOV_SVAL(IOV_LOCKOUT_PERIOD):
		chanim_config(wlc->chanim_info).lockout_period = int_val;
		break;

	case IOV_GVAL(IOV_ACS_RECORD):
		if (alen < (int)sizeof(wl_acs_record_t))
			err = BCME_BUFTOOSHORT;
		else
			err = wlc_lq_chanim_get_acs_record(wlc->chanim_info, alen, a);
		break;

	case IOV_GVAL(IOV_CHANIM_STATS): {
		wl_chanim_stats_t input = *((wl_chanim_stats_t *)p);
		wl_chanim_stats_t *iob = (wl_chanim_stats_t*) a;
		int buflen;

		if ((uint)alen < WL_CHANIM_STATS_FIXED_LEN) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		buflen = (int)input.buflen;

		if ((uint)buflen < WL_CHANIM_STATS_FIXED_LEN) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		err = wlc_lq_chanim_get_stats(wlc->chanim_info, iob, &buflen, input.count);
		break;
	}
#endif /* WLCHANIM */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}




void
wlc_lq_rssi_init(wlc_info_t *wlc, int rssi)
{
	uint i, k, max_ant;

	/* legacy PHY */
	max_ant = (uint32)((WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) ?
	    WL_ANT_HT_RX_MAX : WL_ANT_RX_MAX);
	for (i = 0; i < WLC_RSSI_WINDOW_SZ; i++) {
		for (k = WL_ANT_IDX_1; k < max_ant; k++) {
			if (((wlc->stf->rxchain >> k) & 1) == 0)
				continue;
			wlc->rssi_win_rfchain[k][i] = (int16)rssi;
		}
	}
	wlc->rssi_win_rfchain_idx = 0;
}

/* Reset RSSI moving average */
void
wlc_lq_rssi_snr_noise_reset_ma(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int rssi, int snr, int noise)
{
	int i;
	wlc_link_qual_t *link = cfg->link;

	link->rssi_pkt_tot = 0;
	link->rssi_pkt_count = 0;
	link->rssi_pkt_index = 0;

	link->snr_ma = 0;
	link->snr_no_of_valid_values = 0;
	link->snr_index = 0;

	for (i = 0; i < MA_WINDOW_SZ; i++) {
		link->rssi_pkt_window[i] = 0;
		link->snr_window[i] = 0;
	}

	link->rssi = rssi;
	link->rssi_qdb = 0;
	link->snr = snr;
	wlc->noise_lvl = noise;
}

/* Reset RSSI moving average */
void
wlc_lq_rssi_reset_ma(wlc_bsscfg_t *cfg, int rssi)
{
	int i;
	wlc_link_qual_t *link = cfg->link;

	link->rssi_pkt_tot = 0;
	link->rssi_pkt_count = 0;
	for (i = 0; i < MA_WINDOW_SZ; i++)
		link->rssi_pkt_window[i] = 0;
	link->rssi_pkt_index = 0;

	link->rssi = rssi;
}

int
wlc_lq_rssi_update_ma(wlc_bsscfg_t *cfg, int nval, int qdb, bool unicast)
{
	wlc_link_qual_t *link = cfg->link;
#ifdef DOS
	link->rssi = nval;
	return nval;
#endif

	if (nval != WLC_RSSI_INVALID) {
		bool admit_mcast_only = RSSI_ADMIT_MCAST_ONLY(link->rssi_pkt_win_sz);
		uint16 rssi_pkt_win_sz = link->rssi_pkt_win_sz & RSSI_PKT_WIN_SZ_MASK;

		/* rssi filtering with unicast when possible
		 * (count all before rssi window reaching full and no unicast for a while)
		 */
		if (RSSI_ADMIT_ALL_FRAME(link->rssi_pkt_win_sz) ||
		    (admit_mcast_only && !unicast) ||
		    (!admit_mcast_only && (unicast || !link->last_rssi_is_unicast)) ||
		    (link->rssi_pkt_count < rssi_pkt_win_sz)) {
			/* evict old value */
			link->rssi_pkt_tot -= link->rssi_pkt_window[link->rssi_pkt_index];

			/* admit new value - combine rssi(in nval) and rssi_qdb (in qdb) */
			nval = (nval << 2) + qdb;
			link->rssi_pkt_tot += nval;
			link->rssi_pkt_window[link->rssi_pkt_index] = nval;
			link->rssi_pkt_index = MODINC_POW2(link->rssi_pkt_index,
				rssi_pkt_win_sz);
			if (link->rssi_pkt_count < rssi_pkt_win_sz)
				link->rssi_pkt_count++;
			link->rssi = (link->rssi_pkt_tot / link->rssi_pkt_count);
			link->rssi_qdb = link->rssi & 3;
			link->rssi = link->rssi >> 2;

			if (RSSI_PKT_WIN_DEBUG(link->rssi_pkt_win_sz))
				WL_PRINT(("rssi_flt(0x%04x:%d): new=%d(%c) -> rssi=%d\n",
					link->rssi_pkt_win_sz, link->rssi_pkt_count, nval,
					(unicast ? 'U' : 'M'), link->rssi));
		}

		link->last_rssi_is_unicast = unicast;
	}
	else if (link->rssi_pkt_count == 0)
		link->rssi = WLC_RSSI_INVALID;

	return link->rssi;
}

void
wlc_lq_rssi_event_update(wlc_bsscfg_t *cfg)
{
	int level;
	wlc_info_t *wlc = cfg->wlc;
	wlc_link_qual_t *link = cfg->link;

	/* no update if timer active */
	if (link->is_rssi_event_timer_active)
		return;

	/* find rssi level */
	for (level = 0; level < link->rssi_event->num_rssi_levels; level++) {
		if (link->rssi <= link->rssi_event->rssi_levels[level])
			break;
	}

	if (level != link->rssi_level) {
		/* rssi level changed - post rssi event */
		wl_event_data_rssi_t value;
		value.rssi  = hton32(link->rssi);
		value.snr   = hton32(link->snr);
		value.noise = hton32(wlc->noise_lvl);
		link->rssi_level = (uint8)level;
		wlc_bss_mac_event(wlc, cfg, WLC_E_RSSI, NULL, 0, 0, 0, &value, sizeof(value));
		if (link->rssi_event_timer && link->rssi_event->rate_limit_msec) {
			/* rate limit rssi events */
			link->is_rssi_event_timer_active = TRUE;
			wl_add_timer(wlc->wl, link->rssi_event_timer,
				link->rssi_event->rate_limit_msec, FALSE);
		}
	}
}

/* The rssi compute is done in low level driver and embedded in the rx pkt in wlc_d11rxhdr,
 * per-antenna rssi are also embedded in wlc_d11rxhdr for moving average cal here
 */
int8
wlc_lq_rssi_pktrxh_cal(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh)
{
	int i, max_ant;

	max_ant = (uint32)((WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) ?
	    WL_ANT_HT_RX_MAX : WL_ANT_RX_MAX);
	if (!wrxh->do_rssi_ma && wrxh->rssi != WLC_RSSI_INVALID) {
		/* go through all valid antennas */
		for (i = WL_ANT_IDX_1; i < max_ant; i++) {
			if (((wlc->stf->rxchain >> i) & 1) == 0)
				continue;
			wlc->rssi_win_rfchain[i][wlc->rssi_win_rfchain_idx] = wrxh->rxpwr[i];
		}

		wlc->rssi_win_rfchain_idx = MODINC_POW2(wlc->rssi_win_rfchain_idx,
			WLC_RSSI_WINDOW_SZ);
		wrxh->do_rssi_ma = 1;
	}

	return wrxh->rssi;
}

static void
wlc_lq_rssi_ant_get(wlc_info_t *wlc, int8 *rssi)
{
	uint32 i, k, chains, idx;
	/* use int32 to avoid overflow when accumulate int8 */
	int32 rssi_sum[WL_RSSI_ANT_MAX] = {0, 0, 0, 0};

	idx = wlc->rssi_win_rfchain_idx;

	chains = (uint32)((WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) ?
	    WL_ANT_HT_RX_MAX : WL_ANT_RX_MAX);

#if defined(WLOFFLD) /* RADAR <13563970> */
	/*
	 * Check if we have any packets received in last tick.
	 * if not then return offload engines rssi value else
	 * fall back to original value
	 */
	if (WLOFFLD_CAP(wlc)) {
		if (wlc_ol_bcn_is_enable(wlc->ol)) {
			if (wlc->_wlc_rate_measurement->rxframes_rate < MA_WINDOW_SZ) {
				int8 ol_rssi = wlc_ol_rssi_get_value(wlc->ol);
				if (ol_rssi < 0) {
					for (k = WL_ANT_IDX_1; k < chains; k++) {
						rssi[k] = ol_rssi;
					}
					WL_TRACE(("%s: RSSI (%d) from OFFLOAD Engine\n",
						__FUNCTION__, ol_rssi));
					return;
				} else {
					WL_TRACE(("%s: Garbage RSSI (%d) from OFFLOAD Engine\n",
						__FUNCTION__, ol_rssi));
				}
			} else {
				WL_TRACE(("%s: No RSSI from OFFLOAD Engine: Not enough PKT\n",
				__FUNCTION__));
			}
		} else {
			WL_TRACE(("%s: No RSSI from OFFLOAD Engine: Beacon disabled\n",
				__FUNCTION__));
		}
		wlc_ol_inc_rssi_cnt_host(wlc->ol);
	} else {
		WL_TRACE(("%s: No RSSI from OFFLOAD Engine: No OFFLOAD CAP\n",
			__FUNCTION__));
	}
#endif /* defined(WLOFFLD) */

	for (i = 0; i < WLC_RSSI_WINDOW_SZ; i++) {
		for (k = WL_ANT_IDX_1; k < chains; k++) {
			if (((wlc->stf->rxchain >> k) & 1) == 0)
				continue;
			rssi_sum[k] += wlc->rssi_win_rfchain[k][idx];
		}
		idx = MODINC_POW2(idx, WLC_RSSI_WINDOW_SZ);
	}

	for (k = WL_ANT_IDX_1; k < chains; k++) {
		rssi[k] = (int8)(rssi_sum[k] / WLC_RSSI_WINDOW_SZ);
	}
}

static void
wlc_lq_rssi_event_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t*)arg;
	cfg->link->is_rssi_event_timer_active = FALSE;
}

int
wlc_lq_snr_update_ma(wlc_bsscfg_t *cfg, int nval, bool unicast)
{
	wlc_link_qual_t *link = cfg->link;
#ifdef DOS
	link->snr = nval;
	return nval;
#endif

	if (nval != WLC_SNR_INVALID) {
		bool admit_mcast_only = RSSI_ADMIT_MCAST_ONLY(link->snr_pkt_win_sz);
		uint16 snr_pkt_win_sz = link->snr_pkt_win_sz & RSSI_PKT_WIN_SZ_MASK;

		/* snr filtering with unicast when possible
		 * (count all before snr window reaching full and no unicast for a while)
		 */
		if (RSSI_ADMIT_ALL_FRAME(link->snr_pkt_win_sz) ||
		    (admit_mcast_only && !unicast) ||
		    (!admit_mcast_only && (unicast || !link->last_snr_is_unicast)) ||
		    (link->snr_no_of_valid_values < snr_pkt_win_sz)) {
			/* evict old value */
			link->snr_ma -= (int32)link->snr_window[link->snr_index];

			/* admit new value */
			link->snr_ma += (int32)nval;
			link->snr_window[link->snr_index] = (uint8)nval;
			link->snr_index = MODINC_POW2(link->snr_index, snr_pkt_win_sz);

			if (link->snr_no_of_valid_values < snr_pkt_win_sz) {
				link->snr_no_of_valid_values++;
			}

			link->snr = ((link->snr_ma + (link->snr_no_of_valid_values>>1)) /
				link->snr_no_of_valid_values);

			if (RSSI_PKT_WIN_DEBUG(link->snr_pkt_win_sz))
				WL_PRINT(("snr_flt(0x%04x:%d): new=%d(%c) -> snr=%d\n",
					link->snr_pkt_win_sz, link->snr_no_of_valid_values, nval,
					(unicast ? 'U' : 'M'), link->snr));
		}

		link->last_snr_is_unicast = unicast;
	}
	else if (link->snr_no_of_valid_values == 0)
		link->snr = WLC_SNR_INVALID;

	return link->snr;
}

int
wlc_lq_noise_update_ma(wlc_info_t *wlc, int nval)
{
#ifdef DOS
	wlc->noise_lvl = nval;
	return nval;
#endif

	/* Asymmetric noise floor filter:
	 *	Going up slowly by only +1
	 *	Coming down faster by diff/2
	 */
	if (nval != WLC_NOISE_INVALID) {
		if (wlc->noise_lvl == WLC_NOISE_INVALID)
			wlc->noise_lvl = nval;
		else if (nval > wlc->noise_lvl)
			wlc->noise_lvl ++;
		else if (nval < wlc->noise_lvl)
			wlc->noise_lvl += (nval - wlc->noise_lvl - 1) / 2;
	}

	return wlc->noise_lvl;
}

/*
This function returns SNR during the recently received frame.
SNR is computed by PHY during frame reception and keeps it in the
D11 frame header. This function reads that value from the D11 frame header
and returns it.
For CCK frame SNR in the D11 frame is in dB in Q.2 format.
For OFDM frames SNR in the D11 frame is 9.30139866 * SNR dB in Q.0 format.
This function returns the SNR for both the frames in dB in Q.0 format.
Brief documentation is available about signalQuality parameter in D11 frame is
available at http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/MAC-PhyInterface.
*/
int
wlc_lq_recv_snr_compute(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, int noise_lvl)
{
	int frameType;
	int signalQuality, snr = WLC_SNR_INVALID;
	int32 snr32;
	int16 multiplication_coefficient;

	if (WLCISLPPHY(wlc->band)) {
		frameType = wrxh->rxhdr.PhyRxStatus_0 & PRXS0_FT_MASK;
		signalQuality = wrxh->rxhdr.PhyRxStatus_1 & PRXS1_SQ_MASK;
		signalQuality = signalQuality >> PRXS1_SQ_SHIFT;
		if (frameType == PRXS0_CCK) {
			snr = signalQuality;		/* Q.2 format */
			snr = ((snr + 2) >> 2);		/* bring to Q.0 format and round */
		}
		else {
			/* signalQuality = 9.30139866 * SNR dB. So convert it into
			* SNR in db at Q.0 format.
			*/
			multiplication_coefficient = (int16)((1.0/9.30139866) * (1<<18)); /* Q.18 */
			snr32 = signalQuality * multiplication_coefficient;	/* snr dB in Q.18 */
			snr = (snr32 + (1 << 17)) >> 18;		/* snr in dB in Q.0 */
		}
	} else if (WLCISLCN40PHY(wlc->band)) {
		if (wrxh->rssi != WLC_RSSI_INVALID) {
			frameType = wrxh->rxhdr.PhyRxStatus_0 & PRXS0_FT_MASK;
			if (frameType != PRXS0_CCK) {
				snr = (ltoh16(wrxh->rxhdr.PhyRxStatus_1) & PRXS1_SQ_MASK)
					>> PRXS1_SQ_SHIFT;
			}
			else {
				if (!noise_lvl)
					noise_lvl = WLC_RSSI_NO_SIGNAL;
				snr = wrxh->rssi - noise_lvl;

				/* Backup just in case Noise Est is incorrect */
				if (snr < 0)
					snr = 3;
				if (snr >= WLC_SNR_EXCELLENT) {
					snr = WLC_SNR_EXCELLENT;
				}
			}
			WL_INFORM(("frtyp=%d, snr=%d, rssi=%d, noise=%d\n",
				frameType, snr, wrxh->rssi,
				noise_lvl));
		}
	}

	/* return SNR */
	return snr;
}

#ifdef WLCQ
static int
wlc_lq_channel_qa_eval(wlc_info_t *wlc)
{
	int k;
	int sample_count;
	int rssi_avg;
	int noise_est;
	int quality_metric;

	sample_count = (int)wlc->channel_qa_sample_num;
	rssi_avg = 0;
	for (k = 0; k < sample_count; k++)
		rssi_avg += wlc->channel_qa_sample[k];
	rssi_avg = (rssi_avg + sample_count/2) / sample_count;

	noise_est = rssi_avg;

	if (noise_est < -85)
		quality_metric = 3;
	else if (noise_est < -75)
		quality_metric = 2;
	else if (noise_est < -65)
		quality_metric = 1;
	else
		quality_metric = 0;

	WL_INFORM(("wl%d: %s: samples rssi {%d %d} avg %d qa %d\n",
		wlc->pub->unit, __FUNCTION__,
		wlc->channel_qa_sample[0], wlc->channel_qa_sample[1],
		rssi_avg, quality_metric));

	return (quality_metric);
}

/* this callback chain must defer calling phy_noise_sample_request */
static void
wlc_lq_channel_qa_sample_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm)
{
	bool moretest = FALSE;

	if (!wlc->channel_qa_active)
		return;

	if (channel != wlc->channel_qa_channel) {
		/* bad channel, try again */
		WL_INFORM(("wl%d: %s: retry, samples from channel %d instead of channel %d\n",
		           wlc->pub->unit, __FUNCTION__, channel, wlc->channel_qa_channel));
		moretest = TRUE;
	} else {
		/* save the sample */
		wlc->channel_qa_sample[wlc->channel_qa_sample_num++] = (int8)noise_dbm;
		if (wlc->channel_qa_sample_num < WLC_CHANNEL_QA_NSAMP) {
			/* still need more samples */
			moretest = TRUE;
		} else {
			/* done with the channel quality measurement */
			wlc->channel_qa_active = FALSE;

			/* evaluate the samples to a quality metric */
			wlc->channel_quality = wlc_lq_channel_qa_eval(wlc);
		}
	}

	if (moretest)
		wlc_lq_channel_qa_sample_req(wlc);

}

int
wlc_lq_channel_qa_start(wlc_info_t *wlc)
{
	/* do nothing if there is already a request for a measurement */
	if (wlc->channel_qa_active)
		return 0;

	WL_INFORM(("wl%d: %s: starting qa measure\n", wlc->pub->unit, __FUNCTION__));

	wlc->channel_qa_active = TRUE;

	wlc->channel_quality = -1;	/* clear to invalid value */
	wlc->channel_qa_sample_num = 0;	/* clear the sample array */

	wlc_lq_channel_qa_sample_req(wlc);

	return 0;
}

void
wlc_lq_channel_qa_sample_req(wlc_info_t *wlc)
{
	/* wait until after a scan if one is in progress */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_NONE(("wl%d: %s: deferring sample request until after scan\n", wlc->pub->unit,
			__FUNCTION__));
		return;
	}

	wlc->channel_qa_channel = CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC);

	WL_NONE(("wl%d: %s(): requesting samples for channel %d\n", wlc->pub->unit,
	         __FUNCTION__, wlc->channel_qa_channel));

	WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_CQ.\n"));

	wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_CQ, wlc->channel_qa_channel);

}
#endif /* defined(WLCQ)  */

void
wlc_lq_noise_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm)
{
	if (wlc->noise_req & WLC_NOISE_REQUEST_SCAN) {
		if (wlc->phynoise_chan_scan == channel)
			wlc->phy_noise_list[channel] = noise_dbm;

		/* TODO - probe responses may have been constructed, fixup those dummy values
		 *  if being blocked by CQRM sampling at different channels, make another request
		 *     if we are still in the requested scan channel and scan hasn't finished yet
		 */
		wlc->noise_req &= ~WLC_NOISE_REQUEST_SCAN;
	}

#ifdef WLCQ
	if (wlc->noise_req & WLC_NOISE_REQUEST_CQ) {
		wlc->noise_req &= ~WLC_NOISE_REQUEST_CQ;
		WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_CQ.\n"));
		wlc_lq_channel_qa_sample_cb(wlc, channel, noise_dbm);
	}
#endif

#if defined(STA) && defined(WLRM)
	if (wlc->noise_req & WLC_NOISE_REQUEST_RM) {
		wlc->noise_req &= ~WLC_NOISE_REQUEST_RM;
		WL_INFORM(("wlc_noise_cb(): WLC_NOISE_REQUEST_RM.\n"));
		if (WLRM_ENAB(wlc->pub) && wlc->rm_info->rm_state->rpi_active) {
			if (wlc_rm_rpi_sample(wlc->rm_info, noise_dbm))
				wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_RM,
				                       CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC));
		}
	}
#endif

	return;

}

void
wlc_lq_noise_sample_request(wlc_info_t *wlc, uint8 request, uint8 channel)
{
	bool sampling_in_progress = (wlc->noise_req != 0);

	WL_TRACE(("%s(): request=%d, channel=%d\n", __FUNCTION__, request, channel));

	switch (request) {
	case WLC_NOISE_REQUEST_SCAN:

		/* fill in dummy value in case the sampling failed or channel mismatch */
		wlc->phy_noise_list[channel] = PHY_NOISE_FIXED_VAL_NPHY;
		wlc->phynoise_chan_scan = channel;

		wlc->noise_req |= WLC_NOISE_REQUEST_SCAN;
		break;

	case WLC_NOISE_REQUEST_CQ:

		wlc->noise_req |= WLC_NOISE_REQUEST_CQ;
		break;

	case WLC_NOISE_REQUEST_RM:

		wlc->noise_req |= WLC_NOISE_REQUEST_RM;
		break;

	default:
		ASSERT(0);
		break;
	}

	if (sampling_in_progress)
		return;

	wlc_phy_noise_sample_request_external(WLC_PI(wlc));

	return;
}

#ifdef BCMDBG
static void
wlc_dump_lq(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "LQ dump:\n");
}
#endif

#if defined(BCMDBG) || defined(WLTEST)
static int
wlc_dump_rssi(wlc_info_t *wlc, struct bcmstrbuf * b)
{
	uint32 i, idx, antidx, max_ant;
	int32 tot;

	if (!wlc->pub->up)
		return BCME_NOTUP;

	bcm_bprintf(b, "History and average of latest %d RSSI values:\n", WLC_RSSI_WINDOW_SZ);

	max_ant = (uint32)((WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) ?
	    WL_ANT_HT_RX_MAX : WL_ANT_RX_MAX);
	for (antidx = WL_ANT_IDX_1; antidx < max_ant; antidx++) {
		if (((wlc->stf->rxchain >> antidx) & 1) == 0)
			continue;
		tot = 0;
		bcm_bprintf(b, "Ant%d: [", antidx);

		idx = wlc->rssi_win_rfchain_idx;
		for (i = 0; i < WLC_RSSI_WINDOW_SZ; i++) {
			bcm_bprintf(b, "%3d ", wlc->rssi_win_rfchain[antidx][idx]);
			tot += wlc->rssi_win_rfchain[antidx][idx];
			idx = MODINC_POW2(idx, WLC_RSSI_WINDOW_SZ);
		}
		bcm_bprintf(b, "]");

		tot /= WLC_RSSI_WINDOW_SZ;
		bcm_bprintf(b, "avg [%4d]\n", tot);
	}

	return 0;
}
#endif 

#ifdef WLCHANIM

static wlc_chanim_stats_t *
wlc_lq_chanim_create_stats(wlc_info_t *wlc, chanspec_t chanspec)
{

	wlc_chanim_stats_t *new_stats = NULL;

	new_stats = (wlc_chanim_stats_t *) MALLOCZ(wlc->osh, sizeof(wlc_chanim_stats_t));

	if (!new_stats) {
		WL_ERROR(("wl%d: %s: out of memory %d bytes\n",
			wlc->pub->unit, __FUNCTION__, (uint)sizeof(wlc_chanim_stats_t)));
	}
	else {
		new_stats->chanim_stats.chanspec = chanspec;
		new_stats->next = NULL;
	}
	return new_stats;
}

static void
wlc_lq_chanim_insert_stats(wlc_chanim_stats_t **rootp, wlc_chanim_stats_t *new)
{
	wlc_chanim_stats_t *curptr;
	wlc_chanim_stats_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr &&
		(curptr->chanim_stats.chanspec < new->chanim_stats.chanspec)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL)
		*rootp = new;
	else
		previous->next = new;
}

wlc_chanim_stats_t *
wlc_lq_chanim_chanspec_to_stats(chanim_info_t *c_info, chanspec_t chanspec)
{
	wlc_chanim_stats_t *cur_stats = c_info->stats;

	while (cur_stats) {
		if (cur_stats->chanim_stats.chanspec == chanspec)
			return cur_stats;
		cur_stats = cur_stats->next;
	}
	return cur_stats;
}

static wlc_chanim_stats_t *
wlc_lq_chanim_find_stats(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_chanim_stats_t *stats = NULL;
	chanim_info_t *c_info = wlc->chanim_info;

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		/* find the stats element if it exists */
		stats = wlc_lq_chanim_chanspec_to_stats(c_info, chanspec);

		if (!stats) {
			stats = wlc_lq_chanim_create_stats(wlc, chanspec);
			if (stats)
				wlc_lq_chanim_insert_stats(&c_info->stats, stats);
		}
	}
	else {
		stats = &c_info->cur_stats;
		stats->chanim_stats.chanspec = chanspec;
		stats->next = NULL;
	}

	return stats;
}

static void
wlc_lq_chanim_meas(wlc_info_t *wlc, chanim_cnt_t *chanim_cnt)
{
	uint16 rxcrsglitch = 0;
	uint16 rxbadplcp = 0;
	uint32 ccastats_cnt[CCASTATS_MAX];
	int i, offset;

	/* Read rxcrsglitch count from shared memory */
	rxcrsglitch = wlc_bmac_read_shm(wlc->hw, M_UCODE_MACSTAT +
	  OFFSETOF(macstat_t, rxcrsglitch));
	chanim_cnt->glitch_cnt = rxcrsglitch;

	chanim_cnt->bphy_glitch_cnt = wlc_bmac_read_shm(wlc->hw, M_UCODE_MACSTAT +
	  OFFSETOF(macstat_t, bphy_rxcrsglitch));

	rxbadplcp = wlc_bmac_read_shm(wlc->hw, M_UCODE_MACSTAT +
	  OFFSETOF(macstat_t, rxbadplcp));
	chanim_cnt->badplcp_cnt = rxbadplcp;

	chanim_cnt->bphy_badplcp_cnt = wlc_bmac_read_shm(wlc->hw, M_UCODE_MACSTAT +
	  OFFSETOF(macstat_t, bphy_badplcp));

	if (WLC_CCASTATS_CAP(wlc)) {
		for (i = 0; i < CCASTATS_MAX; i++) {
			uint16 low, high;
			if (D11REV_GE(wlc->pub->corerev, 40))
				offset = M_CCA_STATS_BLK + 4 * i;
			else
				offset = M_CCA_STATS_BLK_PRE40 + 4 * i;

			low = wlc_bmac_read_shm(wlc->hw, offset);
			high = wlc_bmac_read_shm(wlc->hw, offset+2);
			ccastats_cnt[i] = (high << 16) | low;
			chanim_cnt->ccastats_cnt[i] = ccastats_cnt[i];
		}
	}

	chanim_cnt->timestamp = OSL_SYSUPTIME();
}

static void
wlc_lq_chanim_glitch_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	uint16 glitch_delta = 0;
	uint16 bphy_glitch_delta = 0;
	chanim_cnt_t *last_cnt;

	last_cnt = &c_info->last_cnt;

	/* The statistics glitch_delta are computed when there is a non zero value of
	   last_cnt->glitch_cnt. Bphy statistics are also being updated here because,
	   last_cnt->glitch_cnt is the sum of both OFDM and BPHY glitch counts.
	   So, if there is a non zero value of total glitch count, it is a good idea
	   to update both OFDM and BPHY glitch counts.
	 */
	if (last_cnt->glitch_cnt || acc->chanspec) {
		glitch_delta = cur_cnt->glitch_cnt - last_cnt->glitch_cnt;
		bphy_glitch_delta = cur_cnt->bphy_glitch_cnt - last_cnt->bphy_glitch_cnt;
		acc->glitch_cnt += glitch_delta;
		acc->bphy_glitch_cnt += bphy_glitch_delta;
	}
	last_cnt->glitch_cnt = cur_cnt->glitch_cnt;
	last_cnt->bphy_glitch_cnt = cur_cnt->bphy_glitch_cnt;
}


static void
wlc_lq_chanim_badplcp_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	uint16 badplcp_delta = 0;
	uint16 bphy_badplcp_delta = 0;
	chanim_cnt_t *last_cnt;

	last_cnt = &c_info->last_cnt;

	/* The statistics badplcp_delta are computed when there is a non zero value of
	   last_cnt->badplcp_cnt. Bphy statistics are also being updated here because,
	   last_cnt->badplcp_cnt is the sum of both OFDM and BPHY badplcp counts.
	   So, if there is a non zero value of total badplcp count, it is a good idea
	   to update both OFDM and BPHY badplcp counts.
	 */
	if (last_cnt->badplcp_cnt) {
		badplcp_delta = cur_cnt->badplcp_cnt - last_cnt->badplcp_cnt;
		bphy_badplcp_delta = cur_cnt->bphy_badplcp_cnt - last_cnt->bphy_badplcp_cnt;
		acc->badplcp_cnt += badplcp_delta;
		acc->bphy_badplcp_cnt += bphy_badplcp_delta;
	}
	last_cnt->badplcp_cnt = cur_cnt->badplcp_cnt;
	last_cnt->bphy_badplcp_cnt = cur_cnt->bphy_badplcp_cnt;
}

static void
wlc_lq_chanim_ccastats_accum(chanim_info_t* c_info, chanim_cnt_t *cur_cnt, chanim_accum_t *acc)
{
	int i;
	uint32 ccastats_delta = 0;
	chanim_cnt_t *last_cnt;

	last_cnt = &c_info->last_cnt;

	for (i = 0; i < CCASTATS_MAX; i++) {
		if (last_cnt->ccastats_cnt[i] || acc->chanspec) {
			ccastats_delta = cur_cnt->ccastats_cnt[i] - last_cnt->ccastats_cnt[i];
			acc->ccastats_us[i] += ccastats_delta;
		}
		last_cnt->ccastats_cnt[i] = cur_cnt->ccastats_cnt[i];
	}
}

/*
 * based on current read, accumulate the count
 * also, update the last
 */
static void
wlc_lq_chanim_accum(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t *acc)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_cnt_t cur_cnt, *last_cnt;
	uint cur_time;
	uint interval = 0;

	/* read the current measurement counters */
	wlc_lq_chanim_meas(wlc, &cur_cnt);

	last_cnt = &c_info->last_cnt;
	cur_time = OSL_SYSUPTIME();
	if (last_cnt->timestamp)
		interval = cur_time - last_cnt->timestamp;

	/* update the accumulator with current deltas */
	wlc_lq_chanim_glitch_accum(c_info, &cur_cnt, acc);
	wlc_lq_chanim_badplcp_accum(c_info, &cur_cnt, acc);
	if (WLC_CCASTATS_CAP(wlc)) {
		wlc_lq_chanim_ccastats_accum(c_info, &cur_cnt, acc);
	}
	last_cnt->timestamp = cur_time;
	acc->stats_ms += interval;
	acc->chanspec = chanspec;
}

static void
wlc_lq_chanim_clear_acc(wlc_info_t* wlc, chanim_accum_t* acc)
{
	int i;

	if (acc) {
		acc->glitch_cnt = 0;
		acc->badplcp_cnt = 0;
		acc->bphy_glitch_cnt = 0;
		acc->bphy_badplcp_cnt = 0;

		if (WLC_CCASTATS_CAP(wlc))
			for (i = 0; i < CCASTATS_MAX; i++)
				acc->ccastats_us[i] = 0;

		acc->stats_ms = 0;
	}
}

static int8
wlc_lq_chanim_phy_noise(wlc_info_t *wlc)
{

	int32 rxiq = 0;
	int8 result = 0;
	int err = 0;

	if (!WLCISSSLPNPHY(wlc->band) && !WLCISAPHY(wlc->band) &&
		!WLCISLCNPHY(wlc->band) && SCAN_IN_PROGRESS(wlc->scan)) {
		int cnt = 10, valid_cnt = 0;
		int i;
		int sum = 0;

		rxiq = 10 << 8 | 3; /* default: samples = 1024 (2^10) and antenna = 3 */

		if ((err = wlc_iovar_setint(wlc, "phy_rxiqest", rxiq)) < 0) {
			WL_ERROR(("failed to set phy_rxiqest\n"));
		}

		for (i =  0; i < cnt; i++) {

			if ((err = wlc_iovar_getint(wlc, "phy_rxiqest", &rxiq)) < 0) {
				WL_ERROR(("failed to get phy_rxiqest\n"));
			}

			if (rxiq >> 8)
				result = (int8)MAX((rxiq >> 8) & 0xff, (rxiq & 0xff));
			else
				result = (int8)(rxiq & 0xff);

			if (result) {
				sum += result;
				valid_cnt++;
			}
		}

		if (valid_cnt)
			result = sum/valid_cnt;
	}

	if (!SCAN_IN_PROGRESS(wlc->scan))
		result = wlc_phy_noise_avg(WLC_PI(wlc));

	WL_CHANINT(("bgnoise: %d dBm\n", result));

	return result;
}

/*
 * convert the stats from the accumulative counters to the final stats
 * also clear the accumulative counter.
 */
static void
wlc_lq_chanim_close(wlc_info_t* wlc, chanspec_t chanspec, chanim_accum_t* acc)
{
	int i;
	uint8 stats_frac = 0;
	wlc_chanim_stats_t *cur_stats = NULL;
	uint32 temp;

	if (!(cur_stats = wlc_lq_chanim_find_stats(wlc, chanspec))) {
		WL_ERROR(("failed to allocate chanim_stats\n"));

		return;
	}

	temp = acc->ccastats_us[CCASTATS_INBSS] + acc->ccastats_us[CCASTATS_OBSS] +
		acc->ccastats_us[CCASTATS_NOCTG] + acc->ccastats_us[CCASTATS_DOZE] +
		acc->ccastats_us[CCASTATS_TXDUR];

	/* normalized to per second count */
	if ((acc->stats_ms) && (((acc->stats_ms * 1000) - temp + 500) > 1000)) {

		uint32 aci_chan_idle_dur;

		aci_chan_idle_dur = (acc->stats_ms * 1000 - temp + 500) / 1000;

		cur_stats->chanim_stats.glitchcnt = acc->glitch_cnt * 1000 / aci_chan_idle_dur;

		cur_stats->chanim_stats.bphy_glitchcnt = acc->bphy_glitch_cnt *
			1000 / aci_chan_idle_dur;

		cur_stats->chanim_stats.badplcp = acc->badplcp_cnt * 1000 / aci_chan_idle_dur;

		cur_stats->chanim_stats.bphy_badplcp = acc->bphy_badplcp_cnt *
		        1000 / aci_chan_idle_dur;

		cur_stats->is_valid = TRUE;

	} else {
		cur_stats->chanim_stats.glitchcnt = 0;

		cur_stats->chanim_stats.bphy_glitchcnt = 0;

		cur_stats->chanim_stats.badplcp = 0;

		cur_stats->chanim_stats.bphy_badplcp = 0;

		cur_stats->is_valid = FALSE;
	}


	if (WLC_CCASTATS_CAP(wlc)) {
		uint txop_us = 0;
		uint slottime = APHY_SLOT_TIME;
		uint txop = 0, txop_nom = 0;
		uint8 txop_percent = 0;

		if (wlc->band->gmode && !wlc->shortslot)
			slottime = BPHY_SLOT_TIME;

		for (i = 0; i < CCASTATS_MAX; i++) {
			/* normalize to be 0-100 */

			if (acc->stats_ms) {
				if (i == CCASTATS_TXOP)
					stats_frac = (uint8)CEIL(acc->ccastats_us[i] * slottime,
					  acc->stats_ms * 10);
				else
					stats_frac = (uint8)CEIL(100 * acc->ccastats_us[i],
					  acc->stats_ms * 1000);
			}

			if (stats_frac > 100) {
				WL_INFORM(("stats(%d) > 100: ccastats_us: %d, acc->statss_ms: %d\n",
					stats_frac, acc->ccastats_us[i], acc->stats_ms));
				stats_frac = 100;
			}
			cur_stats->chanim_stats.ccastats[i] = stats_frac;
		}

		/* calc chan_idle */
		txop_us = acc->stats_ms * 1000 -
			acc->ccastats_us[CCASTATS_INBSS] - acc->ccastats_us[CCASTATS_OBSS] -
			acc->ccastats_us[CCASTATS_NOCTG] - acc->ccastats_us[CCASTATS_DOZE] +
			acc->ccastats_us[CCASTATS_BDTXDUR];


		txop_nom = txop_us / slottime;
		txop = acc->ccastats_us[CCASTATS_TXOP] +
			(acc->ccastats_us[CCASTATS_TXDUR] -
			acc->ccastats_us[CCASTATS_BDTXDUR]) / slottime;
		if (txop_nom) {
			 txop_percent = (uint8)CEIL(100 * txop, txop_nom);
			 txop_percent = MIN(100, txop_percent);
		}
		cur_stats->chanim_stats.chan_idle = txop_percent;

	}
	cur_stats->chanim_stats.bgnoise = wlc_lq_chanim_phy_noise(wlc);

	cur_stats->chanim_stats.timestamp = OSL_SYSUPTIME();
	wlc_lq_chanim_clear_acc(wlc, acc);
}

#ifdef BCMDBG
static void
wlc_lq_chanim_display(wlc_info_t *wlc, chanspec_t chanspec)
{
	chanim_info_t *c_info = wlc->chanim_info;
	wlc_chanim_stats_t *cur_stats;

	if (chanspec != wlc->home_chanspec)
		cur_stats = wlc_lq_chanim_chanspec_to_stats(c_info, chanspec);
	else
		cur_stats = &c_info->cur_stats;

	if (!cur_stats)
		return;

	WL_CHANINT(("**intf: %d glitch cnt: %d badplcp: %d noise: %d chanspec: 0x%x \n",
		chanim_mark(c_info).state, cur_stats->chanim_stats.glitchcnt,
		cur_stats->chanim_stats.badplcp, cur_stats->chanim_stats.bgnoise, chanspec));

	if (WLC_CCASTATS_CAP(wlc)) {
		WL_CHANINT(("***cca stats: txdur: %d, inbss: %d, obss: %d,"
		  "nocat: %d, nopkt: %d, doze: %d\n",
		  cur_stats->chanim_stats.ccastats[CCASTATS_TXDUR],
		  cur_stats->chanim_stats.ccastats[CCASTATS_INBSS],
		  cur_stats->chanim_stats.ccastats[CCASTATS_OBSS],
		  cur_stats->chanim_stats.ccastats[CCASTATS_NOCTG],
		  cur_stats->chanim_stats.ccastats[CCASTATS_NOPKT],
		  cur_stats->chanim_stats.ccastats[CCASTATS_DOZE]));
	}
}
#endif /* BCMDBG */

/*
 * the main function for chanim information update
 * it could occur 1) on watchdog 2) on channel switch
 * based on the flag.
 */
void
wlc_lq_chanim_update(wlc_info_t *wlc, chanspec_t chanspec, uint32 flags)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_accum_t* home_acc = &c_info->accum_cnt[CHANIM_HOME_CHAN];

	if (!WLC_CHANIM_ENAB(wlc))
		return;

	/* on watchdog trigger */
	if (flags & CHANIM_WD) {

		if (!WLC_CHANIM_MODE_DETECT(c_info) || SCAN_IN_PROGRESS(wlc->scan))
			return;

		/* need to initialize if chanspec is not set yet */
		if (home_acc->chanspec == 0)
			home_acc->chanspec = wlc->chanspec;

		if (wlc->chanspec == home_acc->chanspec)
			wlc_lq_chanim_accum(wlc, chanspec, home_acc);

		if (!(wlc->pub->now % chanim_config(c_info).sample_period)) {
			wlc_lq_chanim_close(wlc, home_acc->chanspec, home_acc);

#ifdef BCMDBG
			wlc_lq_chanim_display(wlc, home_acc->chanspec);
#endif
		}
	}

	/* on channel switch */
	WL_CHANLOG(wlc, __FUNCTION__, TS_ENTER, 0);
	if (flags & CHANIM_CHANSPEC) {
		if (home_acc->chanspec == 0)
			home_acc->chanspec = chanspec;

		if (chanspec == home_acc->chanspec) {
			wlc_lq_chanim_accum(wlc, chanspec, home_acc);
			wlc_lq_chanim_close(wlc, home_acc->chanspec, home_acc);

			if (wlc->home_chanspec != home_acc->chanspec)
				home_acc->chanspec = wlc->home_chanspec;
		} else {
			chanim_accum_t *off_chan_acc = &c_info->accum_cnt[CHANIM_OFF_CHAN];

			wlc_lq_chanim_accum(wlc, chanspec, off_chan_acc);
			wlc_lq_chanim_close(wlc, chanspec, off_chan_acc);
		}
#ifdef BCMDBG
		wlc_lq_chanim_display(wlc, chanspec);
#endif
	}
	WL_CHANLOG(wlc, __FUNCTION__, TS_EXIT, 0);
}

void
wlc_lq_chanim_acc_reset(wlc_info_t *wlc)
{
	chanim_info_t* c_info = wlc->chanim_info;
	chanim_accum_t* home_acc = &c_info->accum_cnt[CHANIM_HOME_CHAN];

	wlc_lq_chanim_clear_acc(wlc, home_acc);
	bzero((char*)&c_info->last_cnt, sizeof(chanim_cnt_t));
	home_acc->chanspec = 0;
}

static bool
wlc_lq_chanim_interfered_glitch(wlc_chanim_stats_t *stats, uint32 thres)
{
	bool interfered = FALSE;

	interfered = stats->chanim_stats.glitchcnt > thres;
	return interfered;
}

static bool
wlc_lq_chanim_interfered_cca(wlc_chanim_stats_t *stats, uint32 thres)
{
	bool interfered = FALSE;
	uint8 stats_sum;

	stats_sum = stats->chanim_stats.ccastats[CCASTATS_NOPKT];
	interfered = stats_sum > (uint8)thres;

	return interfered;
}

static bool
wlc_lq_chanim_interfered_noise(wlc_chanim_stats_t *stats, int8 thres)
{
	bool interfered = FALSE;
	int8 bgnoise;

	bgnoise = stats->chanim_stats.bgnoise;
	interfered = bgnoise > (uint8)thres;

	return interfered;
}

bool
wlc_lq_chanim_interfered(wlc_info_t *wlc, chanspec_t chanspec)
{
	bool interfered = FALSE;
	wlc_chanim_stats_t *cur_stats;
	chanim_info_t *c_info = wlc->chanim_info;

	if (chanspec != wlc->home_chanspec)
		cur_stats = wlc_lq_chanim_chanspec_to_stats(wlc->chanim_info, chanspec);
	else
		cur_stats = &c_info->cur_stats;

	if (!cur_stats)  {
		WL_INFORM(("%s: no stats allocated for chanspec 0x%x\n",
			__FUNCTION__, chanspec));
		return interfered;
	}

	if (wlc_lq_chanim_interfered_glitch(cur_stats, chanim_config(c_info).crsglitch_thres) ||
		wlc_lq_chanim_interfered_cca(cur_stats, chanim_config(c_info).ccastats_thres) ||
		wlc_lq_chanim_interfered_noise(cur_stats, chanim_config(c_info).bgnoise_thres))
		interfered = TRUE;

	if (chanspec == wlc->home_chanspec)
		chanim_mark(c_info).state = interfered;

	return interfered;
}

static void
wlc_lq_chanim_scb_probe(wlc_info_t *wlc, bool activate)
{
	chanim_info_t * c_info = wlc->chanim_info;

	ASSERT(AP_ENAB(wlc->pub));

	if (activate) {
		/* store the original values, and replace with the chanim values */
		chanim_mark(c_info).scb_timeout = wlc->ap->scb_timeout;
		chanim_mark(c_info).scb_max_probe = wlc->ap->scb_max_probe;
		wlc->ap->scb_timeout = chanim_config(c_info).sample_period;
		wlc->ap->scb_max_probe = chanim_config(c_info).scb_max_probe;
	}
	else {
		/* swap back on exit */
		wlc->ap->scb_timeout = chanim_mark(c_info).scb_timeout;
		wlc->ap->scb_max_probe = chanim_mark(c_info).scb_max_probe;
	}
}

void
wlc_lq_chanim_upd_act(wlc_info_t *wlc)
{
	chanim_info_t * c_info = wlc->chanim_info;

	if (wlc_lq_chanim_interfered(wlc, wlc->home_chanspec) &&
		(wlc->chanspec == wlc->home_chanspec)) {
		if (chanim_mark(c_info).detecttime && !WLC_CHANIM_ACT(c_info)) {
			if ((wlc->pub->now - chanim_mark(c_info).detecttime) >
				(uint)chanim_act_delay(c_info)) {
			    chanim_mark(c_info).flags |= CHANIM_ACTON;
				WL_CHANINT(("***chanim action set\n"));
			}
		}
		else if (!WLC_CHANIM_ACT(c_info)) {
			chanim_mark(c_info).detecttime = wlc->pub->now;
#ifdef AP
			/* start to probe */
			wlc_lq_chanim_scb_probe(wlc, TRUE);
#endif /* AP */
		}
	}
	else {
#ifdef AP
		if (chanim_mark(c_info).detecttime)
			wlc_lq_chanim_scb_probe(wlc, FALSE);
#endif /* AP */
		chanim_mark(c_info).detecttime = 0;
		chanim_mark(c_info).flags &= ~CHANIM_ACTON;
	}
}

void
wlc_lq_chanim_upd_acs_record(chanim_info_t *c_info, chanspec_t home_chspc,
	chanspec_t selected, uint8 trigger)
{
	chanim_acs_record_t* cur_record = &c_info->record[chanim_mark(c_info).record_idx];
	wlc_chanim_stats_t *cur_stats;
	cur_stats = wlc_lq_chanim_chanspec_to_stats(c_info, home_chspc);

	if (WLC_CHANIM_MODE_EXT(c_info))
		return;

	bzero(cur_record, sizeof(chanim_acs_record_t));

	cur_record->trigger = trigger;
	cur_record->timestamp = OSL_SYSUPTIME();
	cur_record->selected_chspc = selected;
	cur_record->valid = TRUE;

	if (cur_stats) {
		cur_record->glitch_cnt = cur_stats->chanim_stats.glitchcnt;
		cur_record->ccastats = cur_stats->chanim_stats.ccastats[CCASTATS_NOPKT];
	}

	chanim_mark(c_info).record_idx ++;
	if (chanim_mark(c_info).record_idx == CHANIM_ACS_RECORD)
		chanim_mark(c_info).record_idx = 0;
}

static int
wlc_lq_chanim_get_acs_record(chanim_info_t *c_info, int buf_len, void *output)
{
	wl_acs_record_t *record = (wl_acs_record_t *)output;
	uint8 idx = chanim_mark(c_info).record_idx;
	int i, count = 0;

	if (WLC_CHANIM_MODE_EXT(c_info))
		return BCME_OK;

	for (i = 0; i < CHANIM_ACS_RECORD; i++) {
		if (c_info->record[idx].valid) {
			bcopy(&c_info->record[idx], &record->acs_record[i],
				sizeof(chanim_acs_record_t));
			count++;
		}
		idx = (idx + 1) % CHANIM_ACS_RECORD;
	}

	record->count = (uint8)count;
	record->timestamp = OSL_SYSUPTIME();
	return BCME_OK;
}

static int
wlc_lq_chanim_get_stats(chanim_info_t *c_info, wl_chanim_stats_t* iob, int *len, int cnt)
{
	uint32 count = 0;
	uint32 datalen;
	wlc_chanim_stats_t* stats = c_info->stats;
	int bcmerror = BCME_OK;
	int buflen = *len;

	iob->version = WL_CHANIM_STATS_VERSION;
	datalen = WL_CHANIM_STATS_FIXED_LEN;

	if (cnt == WL_CHANIM_COUNT_ALL)
		stats = c_info->stats;
	else
		stats = &c_info->cur_stats;

	while (stats) {
		if (buflen < (int)sizeof(chanim_stats_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(&stats->chanim_stats, &iob->stats[count],
			sizeof(chanim_stats_t));

		count++;
		stats = stats->next;
		datalen += sizeof(chanim_stats_t);
		buflen -= sizeof(chanim_stats_t);
	}

	iob->count = count;
	iob->buflen = datalen;

	return bcmerror;
}

#ifdef APCS
static bool
chanim_chk_lockout(chanim_info_t *c_info)
{
	uint8 cur_idx = chanim_mark(c_info).record_idx;
	uint8 start_idx;
	chanim_acs_record_t *start_record;
	uint32 cur_time;

	if (!chanim_config(c_info).max_acs)
		return TRUE;

	start_idx = MODSUB(cur_idx, chanim_config(c_info).max_acs, CHANIM_ACS_RECORD);
	start_record = &c_info->record[start_idx];
	cur_time = OSL_SYSUPTIME();

	if (start_record->valid && ((cur_time - start_record->timestamp) <
			chanim_config(c_info).lockout_period * 1000)) {
		WL_CHANINT(("***chanim lockout true\n"));
		return TRUE;
	}

	return FALSE;
}

/* function for chanim mitigation (action) */
void
wlc_lq_chanim_action(wlc_info_t *wlc)
{
	chanim_info_t *c_info = wlc->chanim_info;
	struct scb *scb;
	struct scb_iter scbiter;
	/* clear the action flag and reset detecttime */
	chanim_mark(c_info).flags &= ~CHANIM_ACTON;
	chanim_mark(c_info).detecttime = 0;
#ifdef AP
	wlc_lq_chanim_scb_probe(wlc, FALSE);
#endif /* AP */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_ASSOCIATED(scb) &&
		    (wlc->pub->now - scb->used < (uint)chanim_act_delay(c_info)))
			break;
	}

	if (!scb) {
		wl_uint32_list_t request;

		if (chanim_chk_lockout(c_info)) {
			WL_CHANINT(("***chanim scan is not allowed due to lockout\n"));
			return;
		}

		request.count = 0;

		if (APCS_ENAB(wlc->pub)) {
			(void)wlc_cs_scan_start(wlc->cfg, &request, TRUE, FALSE, TRUE,
				wlc->band->bandtype, APCS_CHANIM, NULL, NULL);
		}
	}
	return;
}
#endif /* APCS */
#endif /* WLCHANIM */
