/*
 * BT Coex module
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_btcx.c 467328 2014-04-03 01:23:40Z $
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_btcx.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif
#include <wlc_hw_priv.h>
#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#endif /* BCMLTECOEX */
static int wlc_btc_mode_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_wire_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2);
static int wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val);
static int wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2);
static int wlc_btc_params_get(wlc_info_t *wlc, int int_val);
static void wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable);
static void wlc_btc_rssi_threshold_get(wlc_info_t *wlc);

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active);
#endif
static int wlc_btc_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static void wlc_btcx_watchdog(void *arg);

#if defined(BCMDBG)
static int wlc_dump_btcx(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_clr_btcxdump(wlc_info_t *wlc);
#endif 

enum {
	IOV_BTC_MODE,           /* BT Coexistence mode */
	IOV_BTC_WIRE,           /* BTC number of wires */
	IOV_BTC_STUCK_WAR,       /* BTC stuck WAR */
	IOV_BTC_FLAGS,          /* BT Coex ucode flags */
	IOV_BTC_PARAMS,         /* BT Coex shared memory parameters */
	IOV_BTCX_CLEAR_DUMP,    /* clear btcx stats */
	IOV_BTC_SISO_ACK,       /* Specify SISO ACK antenna, disabled when 0 */
	IOV_BTC_RXGAIN_THRESH,  /* Specify restage rxgain thresholds */
	IOV_BTC_RXGAIN_FORCE,   /* Force for BTC restage rxgain */
	IOV_BTC_RXGAIN_LEVEL,   /* Set the BTC restage rxgain level */
};

const bcm_iovar_t btc_iovars[] = {
	{"btc_mode", IOV_BTC_MODE, 0, IOVT_UINT32, 0},
	{"btc_stuck_war", IOV_BTC_STUCK_WAR, 0, IOVT_BOOL, 0 },
	{"btc_flags", IOV_BTC_FLAGS, (IOVF_SET_UP | IOVF_GET_UP), IOVT_BUFFER, 0 },
	{"btc_params", IOV_BTC_PARAMS, (IOVF_SET_UP | IOVF_GET_UP), IOVT_BUFFER, 0 },
#if defined(BCMDBG)
	{"btcx_clear_dump", IOV_BTCX_CLEAR_DUMP, (IOVF_SET_CLK), IOVT_VOID, 0 },
#endif
	{"btc_siso_ack", IOV_BTC_SISO_ACK, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_thresh", IOV_BTC_RXGAIN_THRESH, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_force", IOV_BTC_RXGAIN_FORCE, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_level", IOV_BTC_RXGAIN_LEVEL, 0, IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0}
};

/* BTC stuff */

struct wlc_btc_info {
	wlc_info_t *wlc;
	uint16  bth_period;             /* bt coex period. read from shm. */
	bool    bth_active;             /* bt active session */
	uint8   prot_rssi_thresh;       /* rssi threshold for forcing protection */
	uint8   ampdutx_rssi_thresh;    /* rssi threshold to turn off ampdutx */
	uint8   ampdurx_rssi_thresh;    /* rssi threshold to turn off ampdurx */
	uint8   high_threshold;         /* threshold to switch to btc_mode 4 */
	uint8   low_threshold;          /* threshold to switch to btc_mode 1 */
	uint8   host_requested_pm;      /* saved pm state specified by host */
	uint8   mode_overridden;        /* override btc_mode for long range */
	/* cached value for btc in high driver to avoid frequent RPC calls */
	int     mode;
	int     wire;
	int     siso_ack;               /* SISO ACK antenna (specify core mask) */
	int     restage_rxgain_level;
	int     restage_rxgain_force;
	int     restage_rxgain_active;
	uint8   restage_rxgain_on_rssi_thresh;  /* rssi threshold to turn on rxgain restaging */
	uint8   restage_rxgain_off_rssi_thresh; /* rssi threshold to turn off rxgain restaging */
	uint16	agg_off_bm;
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>


wlc_btc_info_t *
BCMATTACHFN(wlc_btc_attach)(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc;

	if ((btc = (wlc_btc_info_t*)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	btc->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, btc_iovars, "btc", btc, wlc_btc_doiovar,
		wlc_btcx_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	/* register dump stats for btcx */
	wlc_dump_register(wlc->pub, "btcx", (dump_fn_t)wlc_dump_btcx, (void *)wlc);
#endif 


	return btc;

	/* error handling */
fail:
	wlc_btc_detach(btc);
	return NULL;
}

void
BCMATTACHFN(wlc_btc_detach)(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc;

	if (btc == NULL)
		return;

	wlc = btc->wlc;
	wlc_module_unregister(wlc->pub, "btc", btc);

	MFREE(wlc->osh, btc, sizeof(wlc_btc_info_t));
}

static int
wlc_btc_mode_set(wlc_info_t *wlc, int int_val)
{
	int err = wlc_bmac_btc_mode_set(wlc->hw, int_val);
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);
	return err;
}

int
wlc_btc_mode_get(wlc_info_t *wlc)
{
	return wlc->btch->mode;
}

static int
wlc_btc_wire_set(wlc_info_t *wlc, int int_val)
{
	int err;
	err = wlc_bmac_btc_wire_set(wlc->hw, int_val);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
	return err;
}

int
wlc_btc_wire_get(wlc_info_t *wlc)
{
	return wlc->btch->wire;
}

void wlc_btc_mode_sync(wlc_info_t *wlc)
{
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
}

uint8 wlc_btc_save_host_requested_pm(wlc_info_t *wlc, uint8 val)
{
	return (wlc->btch->host_requested_pm = val);
}

bool wlc_btc_get_bth_active(wlc_info_t *wlc)
{
	return wlc->btch->bth_active;
}

uint16 wlc_btc_get_bth_period(wlc_info_t *wlc)
{
	return wlc->btch->bth_period;
}

static int
wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_flags_idx_set(wlc->hw, int_val, int_val2);
}

static int
wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_flags_idx_get(wlc->hw, int_val);
}

static int
wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_params_set(wlc->hw, int_val, int_val2);
}

static int
wlc_btc_params_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_params_get(wlc->hw, int_val);
}

static void
wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable)
{
	wlc_bmac_btc_stuck_war50943(wlc->hw, enable);
}

static void
wlc_btc_rssi_threshold_get(wlc_info_t *wlc)
{
	wlc_bmac_btc_rssi_threshold_get(wlc->hw,
		&wlc->btch->prot_rssi_thresh,
		&wlc->btch->high_threshold,
		&wlc->btch->low_threshold);
}

void  wlc_btc_4313_gpioctrl_init(wlc_info_t *wlc)
{
	if (CHIPID(wlc->pub->sih->chip) == BCM4313_CHIP_ID) {
	/* ePA 4313 brds */
		if (wlc->pub->boardflags & BFL_FEM) {
			if (wlc->pub->boardrev >= 0x1250 && (wlc->pub->boardflags & BFL_FEM_BT)) {
				wlc_mhf(wlc, MHF5, MHF5_4313_BTCX_GPIOCTRL, MHF5_4313_BTCX_GPIOCTRL,
					WLC_BAND_ALL);
			} else
				wlc_mhf(wlc, MHF4, MHF4_EXTPA_ENABLE,
				MHF4_EXTPA_ENABLE, WLC_BAND_ALL);
			/* iPA 4313 brds */
			} else {
				if (wlc->pub->boardflags & BFL_FEM_BT)
					wlc_mhf(wlc, MHF5, MHF5_4313_BTCX_GPIOCTRL,
						MHF5_4313_BTCX_GPIOCTRL, WLC_BAND_ALL);
			}
	}
}

uint
wlc_btc_frag_threshold(wlc_info_t *wlc, struct scb *scb)
{
	ratespec_t rspec;
	uint rate, thresh;
	wlc_bsscfg_t *cfg;

	/* Make sure period is known */
	if (wlc->btch->bth_period == 0)
		return 0;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* if BT SCO is ongoing, packet length should not exceed 1/2 of SCO period */
	rspec = wlc_get_rspec_history(cfg);
	rate = RSPEC2KBPS(rspec);

	/*  use one half of the duration as threshold.  convert from usec to bytes */
	/* thresh = (bt_period * rate) / 1000 / 8 / 2  */
	thresh = (wlc->btch->bth_period * rate) >> 14;

	if (thresh < DOT11_MIN_FRAG_LEN)
		thresh = DOT11_MIN_FRAG_LEN;
	return thresh;
}

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void
wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active)
{
	wlc_bsscfg_t *cfg = wlc->cfg;
	/* only bt is not active, set PM to host requested mode */
	if (wlc->btch->host_requested_pm != PM_FORCE_OFF) {
		if (bt_active) {
				if (PM_OFF == wlc->btch->host_requested_pm &&
				cfg->pm->PM != PM_FAST)
				wlc_set_pm_mode(wlc, PM_FAST, cfg);
		} else {
			if (wlc->btch->host_requested_pm != cfg->pm->PM)
				wlc_set_pm_mode(wlc, wlc->btch->host_requested_pm, cfg);
		}
	}
}
#endif /* STA */

void
wlc_enable_btc_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool protect)
{
	if (MCHAN_ACTIVE(wlc->pub))
		return;

	if ((wlc_btc_wire_get(wlc) >= WL_BTC_3WIRE) &&
		wlc_btc_mode_get(wlc)) {
		wlc_bsscfg_t *pc;
		int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
		uint16 protections;
		uint16 active;
		uint16 ps;

		pc = wlc->cfg;
		BCM_REFERENCE(pc);

		/* if radio is disable, driver may be down, quit here */
		if (wlc->pub->radio_disabled || !wlc->pub->up)
			return;

#if defined(STA)
		/* ??? if ismpc, driver should be in down state if up/down is allowed */
		if (wlc->mpc && wlc_ismpc(wlc))
			return;
#endif
		wlc_btc_rssi_threshold_get(wlc);

		/* enable protection for hybrid mode when rssi below certain threshold */
		if ((wlc->btch->prot_rssi_thresh &&
			-wlc->cfg->link->rssi > wlc->btch->prot_rssi_thresh) ||
			(btc_flags & WL_BTC_FLAG_ACTIVE_PROT))
			active = MHF3_BTCX_ACTIVE_PROT;
		else
			active = 0;

		ps = (btc_flags & WL_BTC_FLAG_PS_PROTECT) ? MHF3_BTCX_PS_PROTECT : 0;
		BCM_REFERENCE(ps);

#ifdef STA
		/* Enable PS protection when the primary bsscfg is associated as
		 * an infra STA and is the only connection
		 */
		if (BSSCFG_STA(pc) && pc->current_bss->infra &&
		    WLC_BSS_CONNECTED(pc) && wlc->stas_connected == 1 &&
		    (wlc->aps_associated == 0 || wlc_ap_stas_associated(wlc->ap) == 0)) {
			/* when WPA/PSK security is enabled wait until WLC_PORTOPEN() is TRUE */
			if (pc->WPA_auth == WPA_AUTH_DISABLED || !WSEC_ENABLED(pc->wsec) ||
			    WLC_PORTOPEN(pc))
				protections = active | ps;
			else
				protections = 0;
		}
		/*
		Enable PS protection if there is only one BSS
		associated as STA and there are no APs. All else
		enable CTS2SELF.
		*/
		else if (wlc->stas_connected > 0)
		{
			if ((wlc->stas_connected == 1) && (wlc->aps_associated == 0))
				protections = active | ps;
			else
				protections = active;
		}
		/* Enable CTS-to-self protection when AP(s) are up and there are
		 * STAs associated
		 */
		else
#endif /* STA */
#ifdef AP
		if (wlc->aps_associated > 0 && wlc_ap_stas_associated(wlc->ap) > 0)
			protections = active;
		/* No protection */
		else
#endif /* AP */
			protections = 0;

		wlc_mhf(wlc, MHF3, MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT,
		        protections, WLC_BAND_2G);
#ifdef WLMCNX
		/*
		For non-VSDB the only time we turn on PS protection is when there is only
		one STA associated - primary or GC. In this case, set the BSS index in
		designated SHM location as well.
		*/
		if ((MCNX_ENAB(wlc->pub)) && (protections & ps)) {
			uint idx;
			wlc_bsscfg_t *cfg;
			int bss_idx;

			FOREACH_AS_STA(wlc, idx, cfg) {
				if (!cfg->BSS)
					continue;
				bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
				wlc_mcnx_shm_bss_idx_set(wlc->mcnx, bss_idx);
				break;
			}
		}
#endif /* WLMCNX */
	}
}

#if defined(BCMDBG)
static int wlc_dump_btcx(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint8 idx, offset;
	uint16 hi, lo;
	uint32 buff[C_BTCX_DBGBLK_SZ/2];
	uint16 base = D11REV_LT(wlc->pub->corerev, 40) ?
	M_BTCX_DBGBLK: M_BTCX_DBGBLK_11AC;

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	for (idx = 0; idx < C_BTCX_DBGBLK_SZ; idx += 2) {
		offset = idx*2;
		lo = wlc_bmac_read_shm(wlc->hw, base+offset);
		hi = wlc_bmac_read_shm(wlc->hw, base+offset+2);
		buff[idx>>1] = (hi<<16) | lo;
	}

	bcm_bprintf(b, "nrfact: %u, ntxconf: %u (%u%%), txconf_durn(us): %u\n",
		buff[0], buff[1], buff[0] ? (buff[1]*100)/buff[0]: 0, buff[2]);
	return 0;
}

static int wlc_clr_btcxdump(wlc_info_t *wlc)
{
	uint16 base = D11REV_LT(wlc->pub->corerev, 40) ?
	M_BTCX_DBGBLK: M_BTCX_DBGBLK_11AC;

	if (!wlc->clk) {
	return BCME_NOCLK;
	}

	wlc_bmac_set_shm(wlc->hw, base, 0, C_BTCX_DBGBLK_SZ*2);
	return 0;
}
#endif 

/* Read relevant BTC params to determine if aggregation has to be enabled/disabled */
void
wlc_btcx_read_btc_params(wlc_info_t *wlc)
{
	if (BT3P_HW_COEX(wlc))
		wlc_bmac_btc_period_get(wlc->hw, &wlc->btch->bth_period,
			&wlc->btch->bth_active, &wlc->btch->agg_off_bm);
}

static void
wlc_btcx_watchdog(void *arg)
{
	int i;
	wlc_btc_info_t *btc = (wlc_btc_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);
	/* update critical BT state, only for 2G band */
	if (btc_mode && BAND_2G(wlc->band->bandtype)) {
		wlc_enable_btc_ps_protection(wlc, wlc->cfg, TRUE);
#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
		wlc_btc_pm_adjust(wlc, wlc->btch->bth_active);
#endif /* STA */
		/* rssi too low, give up */
		if (wlc->btch->low_threshold &&
			-wlc->cfg->link->rssi >
			wlc->btch->low_threshold) {
			if (!IS_BTCX_FULLTDM(btc_mode)) {
				wlc->btch->mode_overridden = (uint8)btc_mode;
				wlc_btc_mode_set(wlc, WL_BTC_FULLTDM);
			}
		} else if (wlc->btch->high_threshold &&
			-wlc->cfg->link->rssi <
			wlc->btch->high_threshold) {
			if (btc_mode != WL_BTC_PARALLEL) {
				wlc->btch->mode_overridden = (uint8)btc_mode;
				wlc_btc_mode_set(wlc, WL_BTC_PARALLEL);
			}
		} else {
			if (wlc->btch->mode_overridden) {
				wlc_btc_mode_set(wlc, wlc->btch->mode_overridden);
				wlc->btch->mode_overridden = 0;
			}
		}
	}
#if !defined(BCMDONGLEHOST) && defined(BCMECICOEX) && defined(WL11N)
	if (CHIPID(wlc->pub->sih->chip) == BCM4331_CHIP_ID &&
		(wlc->pub->boardflags & BFL_FEM_BT)) {
		/* for X28, middle chain has to be disabeld when bt is active */
		/* to assure smooth rate policy */
		if ((btc_mode == WL_BTC_LITE || btc_mode == WL_BTC_HYBRID) &&
			BAND_2G(wlc->band->bandtype) && wlc->btch->bth_active) {
			if (btc_mode == WL_BTC_LITE)
				wlc_stf_txchain_set(wlc, 0x5, TRUE,
					WLC_TXCHAIN_ID_BTCOEX);
		} else {
			wlc_stf_txchain_set(wlc, wlc->stf->hw_txchain,
				FALSE, WLC_TXCHAIN_ID_BTCOEX);
		}
	}
#endif /* !defined(BCMDONGLEHOST) && defined(BCMECICOEX) && defined (WL11N) */

#ifdef WLAMPDU
	if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
		if (wlc_btc_mode_not_parallel(btc_mode)) {
			/* Make sure STA is on the home channel to avoid changing AMPDU
			 * state during scanning
			 */
			if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
			    wlc->pub->associated &&
			    (wlc->chanspec == wlc->cfg->current_bss->chanspec))  {
				/* process all bt related disabling/enabling here */
				if (wlc_btc_turnoff_aggr(wlc)) {
					/* shutoff one rxchain to avoid steep rate drop */
					if ((btc_mode == WL_BTC_HYBRID) &&
						(CHIPID(wlc->pub->sih->chip) == BCM43225_CHIP_ID)) {
						wlc_stf_rxchain_set(wlc, 1, TRUE);
						wlc->stf->rxchain_restore_delay = 0;
					}
					if (IS_BTCX_FULLTDM(btc_mode)) {
						wlc_ampdu_agg_state_update_all(wlc, OFF);
					} else
						wlc_ampdu_agg_state_update_tx_all(wlc, ON);

				} else {
#ifdef BCMLTECOEX
					/* If LTECX is enabled,
					  * Aggregation is resumed in LTECX Watchdog
					  */
					if (!BCMLTECOEX_ENAB(wlc->pub))
#endif /* BCMLTECOEX */
					{
						wlc_ampdu_agg_state_update_tx_all(wlc, ON);
					}
					if ((btc_mode == WL_BTC_HYBRID) &&
						(CHIPID(wlc->pub->sih->chip) == BCM43225_CHIP_ID) &&
						(++wlc->stf->rxchain_restore_delay > 5)) {
						/* restore rxchain. */
						wlc_stf_rxchain_set(wlc,
							wlc->stf->hw_rxchain, TRUE);
						wlc->stf->rxchain_restore_delay = 0;
					}
				}
			}
		} else {
#ifdef BCMLTECOEX
			/* If LTECX is enabled,
			  * Aggregation is resumed in LTECX Watchdog
			  */
			if (!BCMLTECOEX_ENAB(wlc->pub))
#endif /* BCMLTECOEX */
			{
				/* Dynamic BTC mode requires this */
				wlc_ampdu_agg_state_update_tx_all(wlc, ON);
			}
		}
	}
#endif /* WLAMPDU */

	/* Dynamic restaging of rxgain for BTCoex */
	if (!SCAN_IN_PROGRESS(wlc->scan) &&
	    btc->bth_active &&
	    (wlc->cfg->link->rssi != WLC_RSSI_INVALID)) {
		if (!btc->restage_rxgain_active &&
		    ((BAND_5G(wlc->band->bandtype) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_5G_ON)) ||
		     (BAND_2G(wlc->band->bandtype) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_2G_ON) &&
		      (!btc->restage_rxgain_on_rssi_thresh ||
		       (btc_mode == WL_BTC_DISABLE) ||
		       (btc->restage_rxgain_on_rssi_thresh &&
			(btc_mode == WL_BTC_HYBRID) &&
			(-wlc->cfg->link->rssi < btc->restage_rxgain_on_rssi_thresh)))))) {
			if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) == BCME_OK) {
				btc->restage_rxgain_active = 1;
			}
			WL_ASSOC(("wl%d: BTC restage rxgain (%x) ON: RSSI %d Thresh -%d, bt %d, "
				"(err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
				btc->restage_rxgain_on_rssi_thresh,
				btc->bth_active, i));
		}
		else if (btc->restage_rxgain_active &&
			((BAND_5G(wlc->band->bandtype) &&
			((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_OFF)) ||
			(BAND_2G(wlc->band->bandtype) &&
			(((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_OFF) ||
			(btc->restage_rxgain_off_rssi_thresh &&
			(btc_mode == WL_BTC_HYBRID) &&
			(-wlc->cfg->link->rssi > btc->restage_rxgain_off_rssi_thresh)))))) {
			  if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
				btc->restage_rxgain_active = 0;
			  }
			  WL_ASSOC(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d Thresh -%d, bt %d, "
				"(err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
				btc->restage_rxgain_off_rssi_thresh,
				btc->bth_active, i));
		}
	} else if (btc->restage_rxgain_active) {
		if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
			btc->restage_rxgain_active = 0;
		}
		WL_ASSOC(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d bt %d (err %d)\n",
			wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
			btc->bth_active, i));
	}

	if ((wlc->pub->sih->boardvendor == VENDOR_APPLE) &&
	    ((CHIPID(wlc->pub->sih->chip) == BCM4331_CHIP_ID) ||
	     (CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID))) {
		wlc_write_shm(wlc, M_COREMASK_BTRESP, (uint16)btc->siso_ack);
	}
}


/* handle BTC related iovars */

static int
wlc_btc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {

	case IOV_SVAL(IOV_BTC_FLAGS):
		err = wlc_btc_flags_idx_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_FLAGS): {
		*ret_int_ptr = wlc_btc_flags_idx_get(wlc, int_val);
		break;
		}

	case IOV_SVAL(IOV_BTC_PARAMS):
		err = wlc_btc_params_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_PARAMS):
		*ret_int_ptr = wlc_btc_params_get(wlc, int_val);
		break;

	case IOV_SVAL(IOV_BTC_MODE):
		err = wlc_btc_mode_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_MODE):
		*ret_int_ptr = wlc_btc_mode_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_WIRE):
		err = wlc_btc_wire_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_WIRE):
		*ret_int_ptr = wlc_btc_wire_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_STUCK_WAR):
		wlc_btc_stuck_war50943(wlc, bool_val);
		break;


#if defined(BCMDBG)
	case IOV_SVAL(IOV_BTCX_CLEAR_DUMP):
		err = wlc_clr_btcxdump(wlc);
		break;
#endif 

	case IOV_GVAL(IOV_BTC_SISO_ACK):
		if (wlc->clk) {
			*ret_int_ptr = wlc_read_shm(wlc, M_COREMASK_BTRESP);
		} else {
			*ret_int_ptr = btc->siso_ack;
		}
		break;

	case IOV_SVAL(IOV_BTC_SISO_ACK):
		btc->siso_ack = int_val;
		if (wlc->clk) {
			wlc_write_shm(wlc, M_COREMASK_BTRESP, (uint16)int_val);
		}
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_THRESH):
		*ret_int_ptr = ((uint32)btc->restage_rxgain_on_rssi_thresh |
			((uint32)btc->restage_rxgain_off_rssi_thresh << 8));
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_THRESH):
		if (int_val == 0) {
			err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0);
			if (err == BCME_OK) {
				btc->restage_rxgain_on_rssi_thresh = 0;
				btc->restage_rxgain_off_rssi_thresh = 0;
				btc->restage_rxgain_active = 0;
				WL_ASSOC(("wl%d: BTC restage rxgain disabled\n", wlc->pub->unit));
			} else {
				err = BCME_NOTREADY;
			}
		} else {
			btc->restage_rxgain_on_rssi_thresh = (uint8)(int_val & 0xFF);
			btc->restage_rxgain_off_rssi_thresh = (uint8)((int_val >> 8) & 0xFF);
			WL_ASSOC(("wl%d: BTC restage rxgain enabled\n", wlc->pub->unit));
		}
		WL_ASSOC(("wl%d: BTC restage rxgain thresh ON: -%d, OFF -%d\n",
			wlc->pub->unit,
			btc->restage_rxgain_on_rssi_thresh,
			btc->restage_rxgain_off_rssi_thresh));
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_FORCE):
		*ret_int_ptr = btc->restage_rxgain_force;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_FORCE):
		btc->restage_rxgain_force = int_val;
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_LEVEL):
		*ret_int_ptr = btc->restage_rxgain_level;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_LEVEL):
		btc->restage_rxgain_level = int_val;
		if (btc->restage_rxgain_active) {
			if ((err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) != BCME_OK) {
				/* Need to apply new level on next update */
				btc->restage_rxgain_active = 0;
				err = BCME_NOTREADY;
			}
			WL_ASSOC(("wl%d: set BTC rxgain level %d (active %d)\n",
				wlc->pub->unit,
				btc->restage_rxgain_level,
				btc->restage_rxgain_active));
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}


/* Returns true if Aggregation needs to be turned off for BTCX */
bool
wlc_btc_turnoff_aggr(wlc_info_t *wlc)
{
	if ((wlc == NULL) || (wlc->btch == NULL)) {
		return FALSE;
	}
	return (wlc->btch->bth_period && (wlc->btch->bth_period < BT_AMPDU_THRESH));
}

bool
wlc_btc_mode_not_parallel(int btc_mode)
{
	return (btc_mode && (btc_mode != WL_BTC_PARALLEL));
}

bool
wlc_btc_active(wlc_info_t *wlc)
{
	return (wlc->btch->bth_active);
}
