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
 * $Id$
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
#include <wlc_ampdu_cmn.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif
#include <wlc_hw_priv.h>

static int wlc_btc_mode_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_wire_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2);
static int wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val);
static void wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable);
static void wlc_btc_rssi_threshold_get(wlc_info_t *wlc);

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active);
#endif
static int wlc_btc_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_btcx_watchdog(void *arg);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_dump_btcx(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_clr_btcxdump(wlc_info_t *wlc);
#endif /* BCMDBG || BCMDBG_DUMP */

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
	IOV_BTC_RXGAIN_LEVEL    /* Set the BTC restage rxgain level */
};

const bcm_iovar_t btc_iovars[] = {
	{"btc_mode", IOV_BTC_MODE, 0, IOVT_UINT32, 0},
	{"btc_stuck_war", IOV_BTC_STUCK_WAR, 0, IOVT_BOOL, 0 },
	{"btc_flags", IOV_BTC_FLAGS, (IOVF_SET_UP | IOVF_GET_UP), IOVT_BUFFER, 0 },
	{"btc_params", IOV_BTC_PARAMS, 0, IOVT_BUFFER, 0 },
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
	uint16  agg_off_bm;
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

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	/* register dump stats for btcx */
	wlc_dump_register(wlc->pub, "btcx", (dump_fn_t)wlc_dump_btcx, (void *)wlc);
#endif /* BCMDBG || BCMDBG_DUMP */


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

int
wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_params_set(wlc->hw, int_val, int_val2);
}

int
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
	/*
	BTCX mode is set in wlc_mchan_set_btcx_prot_mode() in wlc_mchan.c when
	MCHAN is active
	*/
	if (MCHAN_ACTIVE(wlc->pub))
		return;

	if ((wlc_btc_wire_get(wlc) >= WL_BTC_3WIRE) &&
		wlc_btc_mode_get(wlc)) {
		wlc_bsscfg_t *pc;
		int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
		uint16 protections = 0;
		uint16 active;
		uint16 ps;

		pc = wlc->cfg;
		BCM_REFERENCE(pc);

		/* if radio is disable, driver may be down, quit here */
		if (wlc->pub->radio_disabled || !wlc->pub->up)
			return;

#if defined(STA) && !defined(BCMNODOWN)
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
			if ((wlc->stas_connected == 1) && (wlc->aps_associated == 0) &&
				!BSSCFG_IBSS(pc))
				protections = active | ps;
			else
				protections = active;
		}
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

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
#endif /* BCMDBG || BCMDBG_DUMP */

/* Read relevant BTC params to determine if aggregation has to be enabled/disabled */
void
wlc_btcx_read_btc_params(wlc_info_t *wlc)
{
	if (BT3P_HW_COEX(wlc))
		wlc_bmac_btc_period_get(wlc->hw, &wlc->btch->bth_period,
			&wlc->btch->bth_active, &wlc->btch->agg_off_bm);
}

static int
wlc_btcx_watchdog(void *arg)
{
	int i;
	wlc_btc_info_t *btc = (wlc_btc_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);
#ifdef BCMLTECOEX
	uint16 ltecx_rx_reaggr_off;
#endif /* BCMLTECOEX */

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
#if defined(BCMECICOEX) && defined(WL11N)
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
#endif 

#ifdef WLAMPDU
	if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
		if (btc_mode && btc_mode != WL_BTC_PARALLEL) {
			/* Make sure STA is on the home channel to avoid changing AMPDU
			 * state during scanning
			 */

			if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
			    wlc->pub->associated) {
#ifdef BCMLTECOEX
				if (BCMLTECOEX_ENAB(wlc->pub))	{
					if (wlc->hw->ltecx_hw->mws_lterx_prot) {
						if (wlc_ltecx_get_lte_status(wlc) &&
							!wlc->hw->ltecx_hw->mws_elna_bypass)
						{
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
								WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
							wlc->hw->ltecx_hw->mws_rx_aggr_off = TRUE;
						}
						if (wlc_ltecx_get_lte_status(wlc))
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
								WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
					}
				}
#endif /* BCMLTECOEX */
				/* process all bt related disabling/enabling here */
				if ((wlc->btch->bth_period &&
				    wlc->btch->bth_period < BT_AMPDU_THRESH) ||
				    wlc->btch->agg_off_bm) {
					/* shutoff one rxchain to avoid steep rate drop */
					if ((btc_mode == WL_BTC_HYBRID) &&
						(CHIPID(wlc->pub->sih->chip) == BCM43225_CHIP_ID)) {
						wlc_stf_rxchain_set(wlc, 1, TRUE);
						wlc->stf->rxchain_restore_delay = 0;
					}
					if (IS_BTCX_FULLTDM(btc_mode)) {
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
							WLC_BAND_2G, AMPDU_UPD_ALL);
					} else
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
				} else {
#ifdef BCMLTECOEX
					if (BCMLTECOEX_ENAB(wlc->pub))	{
						if (((!wlc_ltecx_get_lte_status(wlc) ||
							wlc->hw->ltecx_hw->mws_elna_bypass)	&&
							wlc->hw->ltecx_hw->mws_rx_aggr_off) &&
							!wlc->btch->bth_active) {
							ltecx_rx_reaggr_off = wlc_read_shm(wlc,
								wlc->hw->btc->bt_shm_addr
								+ M_LTECX_RX_REAGGR);
							if (!ltecx_rx_reaggr_off) {
								/* Resume Rx aggregation per
								  * SWWLAN-32809
								  */
								wlc_ampdu_agg_state_upd_ovrd_band
								(wlc, ON, WLC_BAND_2G,
								AMPDU_UPD_RX_ONLY);
								wlc->hw->ltecx_hw->mws_rx_aggr_off =
									FALSE;
							}
						}
						if (!wlc_ltecx_get_lte_status(wlc))
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
								WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
					}
					else
#endif /* BCMLTECOEX */
					{
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_ALL);
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
			if (BCMLTECOEX_ENAB(wlc->pub))	{
				if (wlc_ltecx_get_lte_status(wlc) &&
					!wlc->hw->ltecx_hw->mws_elna_bypass) {
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
						WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
					wlc->hw->ltecx_hw->mws_rx_aggr_off = TRUE;
				} else if (wlc->hw->ltecx_hw->mws_rx_aggr_off &&
				!wlc->btch->bth_active) {
					ltecx_rx_reaggr_off = wlc_read_shm(wlc,
						wlc->hw->btc->bt_shm_addr + M_LTECX_RX_REAGGR);
					if (!ltecx_rx_reaggr_off) {
						/* Resume Rx aggregation per SWWLAN-32809 */
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
						wlc->hw->ltecx_hw->mws_rx_aggr_off = FALSE;
					}
				}
				/* Don't resume tx aggr while LTECX is active */
				if (wlc_ltecx_get_lte_status(wlc))
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
						WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
				else
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
						WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
			}
			else
#endif /* BCMLTECOEX */
			{
				/* Dynamic BTC mode requires this */
				wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
					WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
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
	     (CHIPID(wlc->pub->sih->chip) == BCM43602_CHIP_ID) ||
	     (CHIPID(wlc->pub->sih->chip) == BCM43462_CHIP_ID) ||
	     (CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID))) {
		wlc_write_shm(wlc, M_COREMASK_BTRESP, (uint16)btc->siso_ack);
	}

	return BCME_OK;
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


#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	case IOV_SVAL(IOV_BTCX_CLEAR_DUMP):
		err = wlc_clr_btcxdump(wlc);
		break;
#endif /* BCMDBG || BCMDBG_DUMP */

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


/* TODO: Move the following LTECOEX code to a new file: wlc_ltecx.c */
#ifdef BCMLTECOEX

enum {
	IOV_LTECX_MWS_COEX_BITMAP,
	IOV_LTECX_MWS_WLANRX_PROT,
	IOV_LTECX_WCI2_TXIND,
	IOV_LTECX_MWS_WLANRXPRI_THRESH,
	IOV_LTECX_WCI2_CONFIG,
	IOV_LTECX_MWS_PARAMS,
	IOV_LTECX_MWS_FRAME_CONFIG,
	IOV_LTECX_MWS_COEX_BAUDRATE,
	IOV_LTECX_MWS_SCANJOIN_PROT,
	IOV_LTECX_MWS_LTETX_ADV_PROT,
	IOV_LTECX_MWS_LTERX_PROT,
	IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	IOV_LTECX_MWS_IM3_PROT,
	IOV_LTECX_MWS_WIFI_SENSITIVITY,
	IOV_LTECX_MWS_DEBUG_MSG,
	IOV_LTECX_MWS_DEBUG_MODE,
	IOV_LTECX_WCI2_LOOPBACK
};

/* LTE coex iovars */
const bcm_iovar_t ltecx_iovars[] = {
	{"mws_coex_bitmap", IOV_LTECX_MWS_COEX_BITMAP,
	(0), IOVT_UINT16, 0
	},
	{"mws_wlanrx_prot", IOV_LTECX_MWS_WLANRX_PROT,
	(0), IOVT_UINT16, 0
	},
	{"wci2_txind", IOV_LTECX_WCI2_TXIND,
	(0), IOVT_UINT16, 0
	},
	{"mws_wlanrxpri_thresh", IOV_LTECX_MWS_WLANRXPRI_THRESH,
	(0), IOVT_UINT16, 0
	},
	{"mws_params", IOV_LTECX_MWS_PARAMS,
	(0),
	IOVT_BUFFER, sizeof(mws_params_t)
	},
	{"wci2_config", IOV_LTECX_WCI2_CONFIG,
	(0),
	IOVT_BUFFER, sizeof(wci2_config_t)
	},
	{"mws_frame_config", IOV_LTECX_MWS_FRAME_CONFIG,
	(0),
	IOVT_UINT16, 0
	},
	{"mws_baudrate", IOV_LTECX_MWS_COEX_BAUDRATE,
	(IOVF_SET_DOWN), IOVT_UINT16, 0
	},
	{"mws_scanjoin_prot", IOV_LTECX_MWS_SCANJOIN_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_ltetx_adv", IOV_LTECX_MWS_LTETX_ADV_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_lterx_prot", IOV_LTECX_MWS_LTERX_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_im3_prot", IOV_LTECX_MWS_IM3_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_wifi_sensitivity", IOV_LTECX_MWS_WIFI_SENSITIVITY,
	(0), IOVT_INT32, 0
	},
	{"mws_debug_msg", IOV_LTECX_MWS_DEBUG_MSG,
	(IOVF_SET_UP | IOVF_GET_UP),
	IOVT_BUFFER, sizeof(mws_wci2_msg_t)
	},
	{"mws_debug_mode", IOV_LTECX_MWS_DEBUG_MODE,
	(IOVF_SET_UP | IOVF_GET_UP), IOVT_INT32, 0
	},
	{"mws_elna_rssi_thresh", IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	(0), IOVT_INT32, 0
	},
	{"wci2_loopback", IOV_LTECX_WCI2_LOOPBACK,
	(IOVF_SET_UP | IOVF_GET_UP),
	IOVT_BUFFER, sizeof(wci2_loopback_t)
	},
	{NULL, 0, 0, 0, 0}
};

static int wlc_ltecx_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_ltecx_watchdog(void *arg);

static const char BCMATTACHDATA(rstr_ltecx)[]                    = "ltecx";
static const char BCMATTACHDATA(rstr_ltecx_rssi_thresh_lmt)[]    = "ltecx_rssi_thresh_lmt";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_mode)[]         = "ltecx_20mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_mode)[]         = "ltecx_10mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2390_rssi_th)[] = "ltecx_20mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2385_rssi_th)[] = "ltecx_20mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2380_rssi_th)[] = "ltecx_20mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2375_rssi_th)[] = "ltecx_20mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2370_rssi_th)[] = "ltecx_20mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2395_rssi_th)[] = "ltecx_10mhz_2395_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2390_rssi_th)[] = "ltecx_10mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2385_rssi_th)[] = "ltecx_10mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2380_rssi_th)[] = "ltecx_10mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2375_rssi_th)[] = "ltecx_10mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2370_rssi_th)[] = "ltecx_10mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecxmux)[]                 = "ltecxmux";
#undef  ROM_AUTO_IOCTL_PATCH_IOVARS
#define ROM_AUTO_IOCTL_PATCH_IOVARS    NULL
#undef  ROM_AUTO_IOCTL_PATCH_DOIOVAR
#define ROM_AUTO_IOCTL_PATCH_DOIOVAR   NULL


wlc_ltecx_info_t *
BCMATTACHFN(wlc_ltecx_attach)(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_ltecx_info_t *ltecx;
	uint16 i, array_size;
	/* ltecxmux
	 * #nibble 0: Mode (WCI2/GPIO/...)
	 * #nibble 1: FNSEL(WCI2) / LTECX PIN Configuration
	 * #nibble 2: UART1 GPIO#(WCI2) / Frame Sync GPIO number
	 * #nibble 3: UART2 GPIO#(WCI2) / LTE Rx GPIO number
	 * #nibble 4: LTE Tx GPIO number
	 * #nibble 5: WLAN Priority GPIO number
	 */

	if ((ltecx = (wlc_ltecx_info_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_ltecx_info_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	ltecx->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, ltecx_iovars, rstr_ltecx, ltecx, wlc_ltecx_doiovar,
		wlc_ltecx_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt) != NULL) {
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram =
				(uint8)getintvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt);
	}
	else {
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram = LTE_RSSI_THRESH_LMT;
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_mode) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars, rstr_ltecx_20mhz_mode);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_mode, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_mode) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars, rstr_ltecx_10mhz_mode);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_mode, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2390_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2390_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2390][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2390_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2385_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2385_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2385][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2385_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2380_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2380_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2380][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2380_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2375_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2375_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2375][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2375_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2370_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2370_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2370][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2370_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2395_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2395_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2395][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2395_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2390_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2390_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2390][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2390_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2385_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2385_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2385][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2385_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2380_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2380_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2380][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2380_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2375_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2375_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2375][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2375_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2370_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2370_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2370][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2370_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxmux) != NULL) {
		wlc_hw->ltecx_hw->ltecxmux = (uint32)getintvar(wlc_hw->vars, rstr_ltecxmux);
	}

	wlc->pub->_ltecx = ((wlc_hw->boardflags & BFL_LTECOEX) != 0);
	wlc->pub->_ltecxgci = (wlc_hw->sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT) &&
		(wlc_hw->machwcap & MCAP_BTCX);
	return ltecx;

fail:
	wlc_ltecx_detach(ltecx);
	return NULL;
}

void
BCMATTACHFN(wlc_ltecx_detach)(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc;

	if (ltecx == NULL)
		return;

	wlc = ltecx->wlc;
	wlc->pub->_ltecx = FALSE;
	wlc_module_unregister(wlc->pub, rstr_ltecx, ltecx);
	MFREE(wlc->osh, ltecx, sizeof(wlc_ltecx_info_t));
}

void
wlc_ltecx_init(wlc_hw_info_t *wlc_hw)
{
	bool ucode_up = FALSE;

	if (wlc_hw->up)
		ucode_up = (R_REG(wlc_hw->osh, &wlc_hw->regs->maccontrol) & MCTL_EN_MAC);

	/* Enable LTE coex in ucode */
	if (wlc_hw->up && ucode_up)
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
	/* Clear history */
	wlc_hw->ltecx_hw->ltecx_enabled			= 0;
	wlc_hw->ltecx_hw->mws_wlanrx_prot_prev	= 0;
	wlc_hw->ltecx_hw->mws_lterx_prot_prev	= 0;
	wlc_hw->ltecx_hw->ltetx_adv_prev		= 0;
	wlc_hw->ltecx_hw->adv_tout_prev			= 0;
	wlc_hw->ltecx_hw->scanjoin_prot_prev	= 0;
	wlc_hw->ltecx_hw->mws_ltecx_txind_prev	= 0;
	wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev	= 0;
	wlc_hw->ltecx_hw->mws_rx_aggr_off		= 0;
	wlc_hw->ltecx_hw->mws_wifi_sensi_prev	= 0;
	wlc_hw->ltecx_hw->mws_im3_prot_prev		= 0;
	wlc_hw->ltecx_hw->mws_wlanrxpri_thresh = 2;
	/* If INVALID, set baud_rate to default */
	if (wlc_hw->ltecx_hw->baud_rate == LTECX_WCI2_INVALID_BAUD)
		wlc_hw->ltecx_hw->baud_rate = LTECX_WCI2_DEFAULT_BAUD;

	/* update look-ahead, baud rate and tx_indication from NVRAM variable */
	if (wlc_hw->ltecx_hw->ltecx_flags) {
		wlc_hw->ltecx_hw->ltetx_adv = (wlc_hw->ltecx_hw->ltecx_flags
					    & LTECX_LOOKAHEAD_MASK)>> LTECX_LOOKAHEAD_SHIFT;
		wlc_hw->ltecx_hw->baud_rate = (wlc_hw->ltecx_hw->ltecx_flags
					    & LTECX_BAUDRATE_MASK)>> LTECX_BAUDRATE_SHIFT;
		wlc_hw->ltecx_hw->mws_ltecx_txind = (wlc_hw->ltecx_hw->ltecx_flags
						  & LTECX_TX_IND_MASK)>> LTECX_TX_IND_SHIFT;
	}

	/* update ltecx parameters based on nvram and lte freq */
	wlc_ltecx_update_status(wlc_hw->wlc);
	wlc_ltecx_check_chmap(wlc_hw->wlc);
	/* update elna status based on the rssi_threshold */
	wlc_ltecx_update_wl_rssi_thresh(wlc_hw->wlc);
	/* Set WLAN Rx prot mode in ucode */
	wlc_ltecx_set_wlanrx_prot(wlc_hw);
	/* Allow WLAN TX during LTE TX based on eLNA bypass status */
	wlc_ltecx_update_wlanrx_ack(wlc_hw->wlc);
	/* update look ahead and protection
	 * advance duration
	 */
	wlc_ltecx_update_ltetx_adv(wlc_hw->wlc);
	/* update lterx protection type */
	wlc_ltecx_update_lterx_prot(wlc_hw->wlc);
	/* update scanjoin protection */
	wlc_ltecx_scanjoin_prot(wlc_hw->wlc);
	/* update ltetx indication */
	wlc_ltetx_indication(wlc_hw->wlc);
	/* update wlanrxpri_thresh */
	wlc_ltecx_set_wlanrxpri_thresh(wlc_hw->wlc);
	if (wlc_hw->up && ucode_up)
		wlc_bmac_enable_mac(wlc_hw);
	/* update ltecx interface information */
	wlc_ltecx_host_interface(wlc_hw->wlc);

}

static int
wlc_ltecx_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)ctx;
	wlc_info_t *wlc = (wlc_info_t *)ltecx->wlc;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	int err = 0;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BITMAP):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->ltecx_chmap;
		else
			err = BCME_BUFTOOSHORT;
	    break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BITMAP):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 bitmap;
			bool ucode_up = FALSE;
			bcopy((char *)params, (char *)&bitmap, sizeof(uint16));

			wlc->hw->ltecx_hw->ltecx_chmap = bitmap;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);

			/* Enable LTE coex in ucode */
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_check_chmap(wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRX_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_wlanrx_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof (uint16)) {
			uint16 val;
			bool ucode_up = FALSE;

			bcopy((char *)params, (char *)&val, sizeof(uint16));
			if (val > C_LTECX_HOST_PROT_TYPE_AUTO) {
				err = BCME_BADARG;
				break;
			}

			wlc->hw->ltecx_hw->mws_wlanrx_prot = val;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);

			/* Enable wlan rx protection in ucode */
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_set_wlanrx_prot(wlc->hw);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_TXIND):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_ltecx_txind = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltetx_indication(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_TXIND):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_ltecx_txind;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_wlanrxpri_thresh;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (p_len >= sizeof (uint16))
		{
			uint16 val;
			bcopy((char *)params, (char *)&val, sizeof(uint16));

			if ((uint16)val > 11) {
				err = BCME_BADARG;
				break;
			}
			wlc->hw->ltecx_hw->mws_wlanrxpri_thresh = val;
			wlc_ltecx_set_wlanrxpri_thresh(wlc->hw->wlc);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy((char *)params, &wlc->hw->ltecx_hw->wci2_config,
		sizeof(wci2_config_t));
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy(&wlc->hw->ltecx_hw->wci2_config, (char *)arg,
		sizeof(wci2_config_t));
		break;
	case IOV_SVAL(IOV_LTECX_MWS_PARAMS):
		bcopy((char *)params, &wlc->hw->ltecx_hw->mws_params,
		sizeof(mws_params_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_PARAMS):
		bcopy(&wlc->hw->ltecx_hw->mws_params, (char *)arg,
		sizeof(mws_params_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_frame_config;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		if (len >= sizeof(uint16))
			/* 16-bit value */
			wlc->hw->ltecx_hw->mws_frame_config = (uint16)int_val;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->baud_rate;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 baudrate;
			bcopy((char *)params, (char *)&baudrate, sizeof(uint16));
			if ((baudrate == 2) || (baudrate == 3))
				wlc->hw->ltecx_hw->baud_rate = baudrate;
			else
				err = BCME_BADARG;
		}
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->scanjoin_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->scanjoin_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_scanjoin_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->ltetx_adv;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 lookahead_dur;
			bool ucode_up = FALSE;

			bcopy((uint16 *)params, &lookahead_dur, sizeof(uint16));
			wlc->hw->ltecx_hw->ltetx_adv = lookahead_dur;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_ltetx_adv(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTERX_PROT):
		if (len >= sizeof(uint16))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_lterx_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTERX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_lterx_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_lterx_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		if (len >= sizeof(int32))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_elna_rssi_thresh;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int32)) {
			int32 rssi_thresh;
			bool ucode_up = FALSE;

			bcopy((int32 *)params, &rssi_thresh, sizeof(int32));
			wlc->hw->ltecx_hw->mws_elna_rssi_thresh = rssi_thresh;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_wl_rssi_thresh(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_IM3_PROT):
		if (len >= sizeof(uint16))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_im3_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_IM3_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_im3_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_im3_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		if (len >= sizeof(int))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_ltecx_wifi_sensitivity;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int)) {
			int wifi_sensitivity;
			bool ucode_up = FALSE;

			bcopy((char *)params, (char *)&wifi_sensitivity, sizeof(int));
			wlc->hw->ltecx_hw->mws_ltecx_wifi_sensitivity = wifi_sensitivity;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_wifi_sensitivity(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);

		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy((char *)params, &wlc->hw->ltecx_hw->mws_wci2_msg,
		sizeof(mws_wci2_msg_t));
		bool ucode_up = FALSE;
		if (wlc->hw->up)
			ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
				MCTL_EN_MAC);
		if (wlc->hw->up && ucode_up) {
			wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_update_debug_msg(wlc->hw->wlc);
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_NOTUP;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy(&wlc->hw->ltecx_hw->mws_wci2_msg, (char *)arg,
		sizeof(mws_wci2_msg_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MODE):
		if (len >= sizeof(int))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_debug_mode;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MODE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int)) {
			int w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(int));
			wlc->hw->ltecx_hw->mws_debug_mode = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up) {
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_update_debug_mode(wlc->hw->wlc);
				wlc_bmac_enable_mac(wlc->hw);
			} else
				err = BCME_NOTUP;
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_LOOPBACK):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(wci2_loopback_t)) {
			wci2_loopback_t wci2_loopback;
			uint16 w = 0;
			bcopy((char *)params, (char *)(&wci2_loopback), sizeof(wci2_loopback));
			if (wci2_loopback.loopback_type) {
				/* prevent setting of incompatible loopback mode */
				w = wlc_bmac_read_shm(wlc->hw,
					wlc->hw->btc->bt_shm_addr + M_LTECX_FLAGS);
				if ((w & LTECX_FLAGS_LPBK_MASK) &&
					((w & LTECX_FLAGS_LPBK_MASK) !=
					wci2_loopback.loopback_type)) {
					err = BCME_BADARG;
					break;
				}
				w |= wci2_loopback.loopback_type;
				if (wci2_loopback.loopback_type != LTECX_FLAGS_LPBK_OFF) {
					/* Reuse CRTI code to start test */
					wlc->hw->ltecx_hw->mws_debug_mode = 1;
					/* Init counters */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_RX,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_TX,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_ERR,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_MSG,
						htol16(0));
					/* CRTI_REPEATS=0 presumes Olympic Rx loopback test */
					/* Initialized for Olympic Tx loopback test further below */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_REPEATS,
						htol16(0));
					/* Suppress scans during lpbk */
					wlc->hw->wlc->scan->state |= SCAN_STATE_SUPPRESS;
				}
				if (wci2_loopback.loopback_type == LTECX_FLAGS_LPBKSRC_MASK) {
					/* Set bit15 of CRTI_MSG to distinguish Olympic Tx
					 * loopback test from RIM test
					 */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_MSG,
						0x8000 | wci2_loopback.packet);
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_INTERVAL,
						20); /* TODO: hardcoded for now */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_REPEATS,
						htol16(wci2_loopback.repeat_ct));
				}
			}
			else {
				/* Lpbk disabled. Reenable scans */
				wlc->hw->wlc->scan->state &= ~SCAN_STATE_SUPPRESS;
				/* CRTI ucode used to start test - now stop test */
				wlc->hw->ltecx_hw->mws_debug_mode = 0;
			}
			wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr + M_LTECX_FLAGS, w);
		} else
			err = BCME_BUFTOOSHORT;
	    break;
	case IOV_GVAL(IOV_LTECX_WCI2_LOOPBACK):
	{
		wci2_loopback_rsp_t rsp;
		rsp.nbytes_rx = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_RX);
		rsp.nbytes_tx = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_TX);
		rsp.nbytes_err = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_ERR);
		bcopy(&rsp, (char *)arg, sizeof(wci2_loopback_rsp_t));
	}
		break;

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

static int
wlc_ltecx_watchdog(void *arg)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)ltecx->wlc;
	if (BCMLTECOEX_ENAB(wlc->pub)) {
		/* update ltecx parameters based on nvram and lte freq */
		wlc_ltecx_update_status(wlc);
		/* enable/disable ltecx based on channel map */
		wlc_ltecx_check_chmap(wlc);
		/* update elna status based on rssi_thresh
		 * First update the elna status and
		 * update the other flags
		 */
		wlc_ltecx_update_wl_rssi_thresh(wlc);
		/* update protection type */
		wlc_ltecx_set_wlanrx_prot(wlc->hw);
		/* Allow WLAN TX during LTE TX based on eLNA bypass status */
		wlc_ltecx_update_wlanrx_ack(wlc);
		/* update look ahead and protection
		 * advance duration
		 */
		wlc_ltecx_update_ltetx_adv(wlc);
		/* update lterx protection type */
		wlc_ltecx_update_lterx_prot(wlc);
		/* update im3 protection type */
		wlc_ltecx_update_im3_prot(wlc);
		/* update scanjoin protection */
		wlc_ltecx_scanjoin_prot(wlc);
		/* update ltetx indication */
		wlc_ltetx_indication(wlc);
		/* update wlanrxpri_thresh */
		wlc_ltecx_set_wlanrxpri_thresh(wlc);
		/* Check RSSI with WIFI Sensitivity */
		wlc_ltecx_wifi_sensitivity(wlc);
		wlc_ltecx_update_debug_mode(wlc);
	}
	return BCME_OK;
}

void wlc_ltecx_check_chmap(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_hflags, ltecx_state;
	uint16 ltecx_en;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	ltecx_en = 0;	/* IMP: Initialize to disable */
	/* Decide if ltecx algo needs to be enabled */
	if (BAND_2G(wlc->band->bandtype)) {
		/* enable ltecx algo as per ltecx_chmap */
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if (wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1))) {
			ltecx_en	= 1;
		}
	}

	if (wlc_hw->btc->bt_shm_addr == NULL)
		return;

	ltecx_state = wlc_bmac_read_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_STATE);

	/* Update ucode ltecx flags */
	if (wlc_hw->ltecx_hw->ltecx_enabled != ltecx_en)	{
		if (wlc_hw->btc->bt_shm_addr) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (ltecx_en == 1) {
				ltecx_hflags |= (1<<C_LTECX_HOST_COEX_EN);
			}
			else  {
				ltecx_hflags &= ~(1<<C_LTECX_HOST_COEX_EN);
				ltecx_state  |= (1<<C_LTECX_ST_IDLE);
				wlc_bmac_write_shm(wlc_hw,
					wlc_hw->btc->bt_shm_addr + M_LTECX_STATE, ltecx_state);
			}
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->ltecx_enabled = ltecx_en;
		}
	}
	wlc_hw->ltecx_hw->ltecx_idle = (ltecx_state >> C_LTECX_ST_IDLE) & 0x1;
}

void wlc_ltecx_set_wlanrx_prot(wlc_hw_info_t *wlc_hw)
{
	uint16 prot, shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	prot = wlc_hw->ltecx_hw->mws_wlanrx_prot;
	/* Set protection to NONE if !2G, or 2G but eLNA bypass */
	if (!BAND_2G(wlc_hw->wlc->band->bandtype) || (wlc_hw->ltecx_hw->mws_elna_bypass)) {
		prot = C_LTECX_MWS_WLANRX_PROT_NONE;
	}
	if (prot != wlc_hw->ltecx_hw->mws_wlanrx_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr) {
			shm_val = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			/* Clear all protection type bits  */
			shm_val = (shm_val &
				~((1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP)
				|(1 << C_LTECX_HOST_PROT_TYPE_PM_CTS)
				|(1 << C_LTECX_HOST_PROT_TYPE_AUTO)));
			/* Set appropriate protection type bit */
			if (prot == C_LTECX_MWS_WLANRX_PROT_NONE) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_CTS) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_PM) {
				shm_val = shm_val | (0 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_AUTO) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_AUTO);
			}
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, shm_val);
			wlc_hw->ltecx_hw->mws_wlanrx_prot_prev = prot;
		}
	}
}

void wlc_ltecx_update_ltetx_adv(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 adv_tout = 0;
	uint16 prot_type;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	/* Update ltetx_adv and adv_tout for 2G band only */
	if (BAND_2G(wlc->band->bandtype))	{
		if (wlc_hw->btc->bt_shm_addr)	{
			if (wlc_hw->ltecx_hw->ltetx_adv != wlc_hw->ltecx_hw->ltetx_adv_prev)	{
				wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
					M_LTECX_TX_LOOKAHEAD_DUR, wlc_hw->ltecx_hw->ltetx_adv);
				wlc_hw->ltecx_hw->ltetx_adv_prev = wlc_hw->ltecx_hw->ltetx_adv;
			}
			/* NOTE: C_LTECX_HOST_PROT_TYPE_CTS may be changed by ucode in AUTO mode */
			prot_type = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
		    if (prot_type & (1 << C_LTECX_HOST_PROT_TYPE_CTS)) {
				if (wlc_hw->ltecx_hw->ltetx_adv >= 500)
				    adv_tout = wlc_hw->ltecx_hw->ltetx_adv - 500;
				else
				    adv_tout = wlc_hw->ltecx_hw->ltetx_adv;
			} else {
			    if (wlc_hw->ltecx_hw->ltetx_adv >= 800)
					adv_tout = wlc_hw->ltecx_hw->ltetx_adv - 800;
				else
					adv_tout = wlc_hw->ltecx_hw->ltetx_adv;
			}
			if (adv_tout != wlc_hw->ltecx_hw->adv_tout_prev)	{
				wlc_bmac_write_shm(wlc_hw,
					wlc_hw->btc->bt_shm_addr + M_LTECX_PROT_ADV_TIME, adv_tout);
				wlc_hw->ltecx_hw->adv_tout_prev = adv_tout;
			}
		}
	}
}

void
wlc_ltecx_update_lterx_prot(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 lterx_prot, ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	lterx_prot = 0;
	if (BAND_2G(wlc->band->bandtype))	{
		lterx_prot	= wlc_hw->ltecx_hw->mws_lterx_prot;
	}
	if (lterx_prot != wlc_hw->ltecx_hw->mws_lterx_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (lterx_prot == 0)
				ltecx_hflags |= (1 << C_LTECX_HOST_TX_ALWAYS);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TX_ALWAYS);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_lterx_prot_prev = lterx_prot;
		}
	}
}

void wlc_ltecx_scanjoin_prot(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 scanjoin_prot, ltecx_hflags;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	scanjoin_prot = 0;
	if (BAND_2G(wlc->band->bandtype)) {
		scanjoin_prot = wlc_hw->ltecx_hw->scanjoin_prot;
	}
	if (scanjoin_prot != wlc_hw->ltecx_hw->scanjoin_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (scanjoin_prot == 0)
				ltecx_hflags &= ~(1 << C_LTECX_HOST_SCANJOIN_PROT);
			else
				ltecx_hflags |= (1 << C_LTECX_HOST_SCANJOIN_PROT);
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->scanjoin_prot_prev = scanjoin_prot;
		}
	}
}

void wlc_ltetx_indication(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_txind, ltecx_hflags;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	ltecx_txind = 0;
	if (BAND_2G(wlc->band->bandtype)) {
		ltecx_txind = wlc_hw->ltecx_hw->mws_ltecx_txind;
	}
	if (ltecx_txind != wlc_hw->ltecx_hw->mws_ltecx_txind_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS);
			if (ltecx_txind == 0)
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TXIND);
			else
				ltecx_hflags |= (1 << C_LTECX_HOST_TXIND);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
					+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_ltecx_txind_prev = ltecx_txind;
		}
	}
}

void wlc_ltecx_host_interface(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_mode, ltecx_hflags;

	ltecx_mode = wlc_hw->ltecx_hw->ltecxmux &
		LTECX_MUX_MODE_MASK >> LTECX_MUX_MODE_SHIFT;

	ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);

	if (BCMLTECOEXGCI_ENAB(wlc_hw->wlc->pub))
	{
		if (ltecx_mode == LTECX_MUX_MODE_GPIO) {
			/* Enabling ERCX */
			si_ercx_init(wlc_hw->sih, wlc_hw->ltecx_hw->ltecxmux);
			ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
		} else {
			/* Enable LTECX WCI-2 UART interface */
			si_wci2_init(wlc_hw->sih, wlc_hw->ltecx_hw->baud_rate,
				wlc_hw->ltecx_hw->ltecxmux);
			ltecx_hflags &= ~(1 << C_LTECX_HOST_INTERFACE);
		}
	}
	else {
		/* Enable LTECX ERCX interface */
		si_ercx_init(wlc_hw->sih, wlc_hw->ltecx_hw->ltecxmux);
		ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
	}
	/* Run Time Interface Config */
	wlc_bmac_write_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);

}

void wlc_ltecx_set_wlanrxpri_thresh(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 thrsh, shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	thrsh = wlc_hw->ltecx_hw->mws_wlanrxpri_thresh;
	shm_val = wlc_bmac_read_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				    + M_LTECX_RXPRI_THRESH);
	/* Set to NONE if !2G */
	if (!BAND_2G(wlc->band->bandtype)) {
		shm_val = 0;
	}
	else {
		shm_val = thrsh;
	}
	wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr + M_LTECX_RXPRI_THRESH, shm_val);
}

int wlc_ltecx_get_lte_status(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (wlc_hw->ltecx_hw->ltecx_enabled && !wlc_hw->ltecx_hw->ltecx_idle)
		return 1;
	else
		return 0;
}
int wlc_ltecx_get_lte_map(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (BAND_2G(wlc->band->bandtype) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((wlc_hw->ltecx_hw->ltecx_enabled) &&
			(wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1))))
			return 1;
	}
	return 0;
}
void wlc_ltecx_update_wl_rssi_thresh(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int ltecx_rssi_thresh, thresh_lmt;
	if (!si_iscoreup (wlc_hw->sih))
		return;
	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc_hw->btc->bt_shm_addr && wlc_ltecx_get_lte_status(wlc) &&
			(wlc->cfg->link->rssi != WLC_RSSI_INVALID)) {
			ltecx_rssi_thresh = wlc_hw->ltecx_hw->mws_elna_rssi_thresh;
			/* mws_ltecx_rssi_thresh_lmt will be zero the very first time
			 * to make the init time decision if we are entering the hysterisis from
			 * lower RSSI side or higher RSSI side (w.r.t. RSSI thresh)
			 */
			thresh_lmt = wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt;
			if (wlc->cfg->link->rssi >= (ltecx_rssi_thresh + thresh_lmt)) {
				wlc_hw->ltecx_hw->mws_elna_bypass = TRUE;
				wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt =
					wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram;
			} else if (wlc->cfg->link->rssi < (ltecx_rssi_thresh - thresh_lmt)) {
				wlc_hw->ltecx_hw->mws_elna_bypass = FALSE;
				wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt =
					wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram;
			}
		} else
			wlc_hw->ltecx_hw->mws_elna_bypass = FALSE;
	}
}

void
wlc_ltecx_update_wlanrx_ack(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 wlan_rx_ack, ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	wlan_rx_ack = 0;
	if (BAND_2G(wlc->band->bandtype))   {
		wlan_rx_ack  = wlc_hw->ltecx_hw->mws_elna_bypass;
	}
	if (wlan_rx_ack != wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev)    {
		if (wlc_hw->btc->bt_shm_addr)   {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (wlan_rx_ack == 1)
				ltecx_hflags |= (1 << C_LTECX_HOST_RX_ACK);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_RX_ACK);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev = wlan_rx_ack;
		}
	}
}
int wlc_ltecx_chk_elna_bypass_mode(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (BAND_2G(wlc->band->bandtype) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((wlc_hw->ltecx_hw->ltecx_enabled) &&
			(wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1)))) {
			if (wlc_hw->ltecx_hw->mws_elna_bypass == TRUE)
				return 1;
			else
				return 0;
		}
	}
	return 0;
}

void wlc_ltecx_update_status(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int i, ch, index, lte_freq_half_bw = 0, freq, coex_chmap = 0, freq_index = 0;
	int	wlanrx_prot_min_ch = 0, lterx_prot_min_ch = 0, scanjoin_prot_min_ch = 0;
	int wlanrx_prot_info = 0, lterx_prot_info = 0, scan_join_prot_info = 0;
	chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);

	if (!wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_LTERX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_LTERX_PROT])
		return;

	if ((wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw !=
		wlc_hw->ltecx_hw->lte_channel_bw_prev)||
		(wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq !=
		wlc_hw->ltecx_hw->lte_center_freq_prev))
	{
		wlc_hw->ltecx_hw->lte_channel_bw_prev =
			wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw;
		wlc_hw->ltecx_hw->lte_center_freq_prev =
			wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq;

		if (wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_20MHZ)	{
			wlanrx_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_20MHZ_INIT_STEP;
		} else if (wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_10MHZ) {
			wlanrx_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_10MHZ_INIT_STEP;
		} else
			return;

		for (freq = (LTE_BAND40_MAX_FREQ - lte_freq_half_bw);
					freq > LTE_BAND40_MIN_FREQ;
						freq = freq- LTE_FREQ_STEP_SIZE) {
			if ((wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq +
				LTE_MAX_FREQ_DEVIATION) >= freq)
				break;
			freq_index ++;
		}
		if (freq_index < LTE_FREQ_STEP_MAX) {
			wlanrx_prot_min_ch =  (wlanrx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			lterx_prot_min_ch =  (lterx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			scanjoin_prot_min_ch =  (scan_join_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
		}
		wlc_hw->ltecx_hw->mws_wlanrx_prot_min_ch = wlanrx_prot_min_ch;
		wlc_hw->ltecx_hw->mws_lterx_prot_min_ch = lterx_prot_min_ch;
		wlc_hw->ltecx_hw->mws_scanjoin_prot_min_ch = scanjoin_prot_min_ch;
		wlc_hw->ltecx_hw->mws_lte_freq_index = freq_index;

		ch = (wlanrx_prot_min_ch >= lterx_prot_min_ch)
					? wlanrx_prot_min_ch: lterx_prot_min_ch;
		for (i = 0; i < ch; i++)
			coex_chmap |= (1<<i);
		/* update coex_bitmap */
		wlc_hw->ltecx_hw->ltecx_chmap = coex_chmap;
	}

	wlanrx_prot_min_ch = wlc_hw->ltecx_hw->mws_wlanrx_prot_min_ch;
	lterx_prot_min_ch = wlc_hw->ltecx_hw->mws_lterx_prot_min_ch;
	scanjoin_prot_min_ch = wlc_hw->ltecx_hw->mws_scanjoin_prot_min_ch;

	/* update wlanrx protection */
	if (wlanrx_prot_min_ch && chan <= wlanrx_prot_min_ch)
		wlc_hw->ltecx_hw->mws_wlanrx_prot = 1;
	else
		wlc_hw->ltecx_hw->mws_wlanrx_prot = 0;
	/* update lterx protection */
	if (lterx_prot_min_ch && chan <= lterx_prot_min_ch)
		wlc_hw->ltecx_hw->mws_lterx_prot = 1;
	else
		wlc_hw->ltecx_hw->mws_lterx_prot = 0;
	/* update scanjoin protection */
	if (scanjoin_prot_min_ch && chan <= scanjoin_prot_min_ch)
		wlc_hw->ltecx_hw->scanjoin_prot = 1;
	else
		wlc_hw->ltecx_hw->scanjoin_prot = 0;
	/* update wl rssi threshold */
	index = wlc_hw->ltecx_hw->mws_lte_freq_index;
	if (wlc_hw->ltecx_hw->lte_channel_bw_prev == LTE_CHANNEL_BW_20MHZ) {
		if (index < LTECX_NVRAM_MAX_RSSI_PARAMS)
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh =
				wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[index][chan-1];
		else
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh = 0;
	} else if (wlc_hw->ltecx_hw->lte_channel_bw_prev == LTE_CHANNEL_BW_10MHZ) {
		if (index < LTECX_NVRAM_MAX_RSSI_PARAMS)
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh =
				wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[index][chan-1];
		else
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh = 0;
	}
}

void
wlc_ltecx_update_im3_prot(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_hflags = 0;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr) &&
		(wlc_hw->ltecx_hw->mws_im3_prot != wlc_hw->ltecx_hw->mws_im3_prot_prev)) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (wlc_hw->ltecx_hw->mws_im3_prot)
				ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TXRX);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TXRX);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
			+ M_LTECX_HOST_FLAGS, ltecx_hflags);
		wlc_hw->ltecx_hw->mws_im3_prot_prev = wlc_hw->ltecx_hw->mws_im3_prot;
	}
}

void
wlc_ltecx_wifi_sensitivity(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
	return;

	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc_hw->btc->bt_shm_addr && wlc_hw->ltecx_hw->mws_wlanrx_prot) {

			if (wlc->cfg->link->rssi > wlc_hw->ltecx_hw->mws_ltecx_wifi_sensitivity)
				wlc_hw->ltecx_hw->mws_wifi_sensi = 1;
			else
				wlc_hw->ltecx_hw->mws_wifi_sensi = 0;

			if (wlc_hw->ltecx_hw->mws_wifi_sensi !=
				wlc_hw->ltecx_hw->mws_wifi_sensi_prev) {
				ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
				if (wlc_hw->ltecx_hw->mws_wifi_sensi)
					ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
				else
					ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
				wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
				wlc_hw->ltecx_hw->mws_wifi_sensi_prev =
					wlc_hw->ltecx_hw->mws_wifi_sensi;
			}
		}
	}
}

void wlc_ltecx_update_debug_msg(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr)) {
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_MSG, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_data);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_INTERVAL, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_interval);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_REPEATS, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_repeat);
	}
}

#define GCI_INTMASK_RXFIFO_NOTEMPTY 0x4000
void wlc_ltecx_update_debug_mode(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_state;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr)) {
		ltecx_state = wlc_bmac_read_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_STATE);
		if (wlc_hw->ltecx_hw->mws_debug_mode) {
			/* Disable Inband interrupt for FrmSync */
			si_gci_indirect(wlc_hw->sih, 0x00010,
			OFFSETOF(chipcregs_t, gci_inbandeventintmask),
			0x00000001, 0x00000000);
			/* Enable Inband interrupt for Aux Valid bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, GCI_INTMASK_RXFIFO_NOTEMPTY);
			/* Route Rx-data through RXFIFO */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF00);
			ltecx_state |= (1 << C_LTECX_ST_CRTI_DEBUG_MODE_TMP);
		} else {
			/* Enable Inband interrupt for FrmSync */
			si_gci_indirect(wlc_hw->sih, 0x00010,
			OFFSETOF(chipcregs_t, gci_inbandeventintmask),
			0x00000001, 0x00000001);
			/* Disable Inband interrupt for Aux Valid bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, 0);
			/* Route Rx-data through AUX register */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF);
			ltecx_state &= ~(1 << C_LTECX_ST_CRTI_DEBUG_MODE_TMP);
		}
		wlc_bmac_write_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_STATE, ltecx_state);
	}
}
/* LTE coex definitions */

void
wlc_ltecx_assoc_in_prog(wlc_info_t *wlc, int val)
{
	uint16 bt_shm_addr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR);
	uint16 ltecxHostFlag = wlc_bmac_read_shm(wlc->hw, bt_shm_addr + M_LTECX_HOST_FLAGS);
	if (val == TRUE)
		ltecxHostFlag |= (1 << C_LTECX_HOST_ASSOC_PROG);
	else
		ltecxHostFlag &= (~(1 << C_LTECX_HOST_ASSOC_PROG));
	wlc_bmac_write_shm(wlc->hw, bt_shm_addr + M_LTECX_HOST_FLAGS, ltecxHostFlag);
}

#endif /* BCMLTECOEX */
