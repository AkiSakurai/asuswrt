/**
 * @file
 * @brief
 * Chip specific software patches for ROM functions. These are used as a memory optimization for
 * invalidated ROM functions. The patch functions execute a relatively small amount of code in
 * order to correct the behavior of ROM functions and then call the original ROM function to
 * perform the majority of the work. For example, a ROM function could be corrected by running a
 * small amount of new code before the call to the ROM function, after the call to the ROM function,
 * or both before and after.
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
 * $Id$
 */

#include <wlc_patch.h>
#include "wlc_scb.h"
#include "wlc_fbt.h"
#include "wlc_dfs.h"
#include "wlc_ap.h"
#include "wlc_sup.h"
#include "wlc_phy_int.h"
#include "wlc_scb_ratesel.h"

#if defined(WLFBT) && !defined(WLFBT_DISABLED)
int (wlc$wlc_recv_mgmtact)(wlc_info_t *wlc, struct scb *scb, struct dot11_management_header *hdr,
                 uint8 *body, int body_len, wlc_d11rxhdr_t *wrxh, uint8 *plcp, void *p);
int CALLROM_ENTRY(wlc$wlc_recv_mgmtact)(wlc_info_t *wlc, struct scb *scb,
		struct dot11_management_header *hdr, uint8 *body, int body_len,
		wlc_d11rxhdr_t *wrxh, uint8 *plcp, void *p);
int (wlc$wlc_recv_mgmtact)(wlc_info_t *wlc, struct scb *scb, struct dot11_management_header *hdr,
                 uint8 *body, int body_len, wlc_d11rxhdr_t *wrxh, uint8 *plcp, void *p)
{
#if !defined(WLFBT_OVER_DS_DISABLED) || defined(FBT_FDAP)
#if defined(WLFBT)
	uint action_category;
	wlc_bsscfg_t *bsscfg = NULL;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char bss_buf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
#endif /* WLFBT */
#endif /* !WLFBT_OVER_DS_DISABLED || FBT_FDAP */

#if !defined(WLFBT_OVER_DS_DISABLED) || defined(FBT_FDAP)
#ifdef STA
#ifdef WLFBT
	if (body_len < 1) {
		WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		return FALSE;
	}

	action_category = body[DOT11_ACTION_CAT_OFF];

	if (action_category == DOT11_ACTION_CAT_FBT) {
		/* XXX JQL:
		 * action frames could be pre-association and could be from outside of the BSS,
		 * Address 1 could be mcast or ucast, BSSID could be wildcard or sender's BSSID,
		 * therefore lookup of bsscfg inside each action frame handler based on
		 * the action frame specifics and its usage is the way to go...
		 */

		/* bsscfg is looked up mainly for sending event, use it otherwise with caution! */
		if (scb != NULL) {
			bsscfg = SCB_BSSCFG(scb);
			ASSERT(bsscfg != NULL);
		}
		else if ((bsscfg = wlc_bsscfg_find_by_bssid(wlc, &hdr->bssid)) != NULL ||
			(bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, &hdr->da)) != NULL) {
			;	/* empty */
		}

		if (BSSCFG_IS_FBT(bsscfg, wlc->pub) && wlc_fbt_enabled(wlc->fbt, bsscfg)) {
			dot11_ft_req_t *ftreq;
			WL_INFORM(("wl%d: %s: FB ACTION \n", WLCWLUNIT(wlc), __FUNCTION__));
			ftreq = (dot11_ft_req_t*)body;
#ifdef AP
			if (ftreq->action == DOT11_FT_ACTION_FT_REQ)
			{
				if (bsscfg == NULL) {
					WL_ASSOC(("wl%d: %s: FT_RES: no bsscfg for BSS %s\n",
						WLCWLUNIT(wlc), __FUNCTION__,
						bcm_ether_ntoa(&hdr->bssid, bss_buf)));
					/* we couldn't match the incoming frame to a BSS config */
					return FALSE;
				}
				if (body_len < DOT11_FT_REQ_FIXED_LEN)
					goto rxbadproto;
				wlc_fbt_recv_overds_req(wlc->fbt, bsscfg, hdr, body, body_len);
			}
			else
#endif /* AP */
#if defined(STA) && defined(FBT_STA)
			if (ftreq->action == DOT11_FT_ACTION_FT_RES)
			{
				if (bsscfg == NULL) {
					WL_ASSOC(("wl%d: %s: FT_RES: no bsscfg for BSS %s\n",
					WLCWLUNIT(wlc), __FUNCTION__,
					bcm_ether_ntoa(&hdr->bssid, bss_buf)));
					/* we couldn't match the incoming frame to a BSS config */
					return FALSE;
				}
				if (body_len < DOT11_FT_RES_FIXED_LEN)
					goto rxbadproto;
				wlc_fbt_recv_overds_resp(wlc->fbt, bsscfg, hdr, body, body_len);
			}
			else
#endif /* STA && FBT_STA */
				goto rxbadproto;
		}
		return FALSE;

rxbadproto:
		wlc_send_action_err(wlc, hdr, body, body_len);
		WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		return FALSE;
	}
#endif /* WLFBT */
#endif /* STA */
#endif /* WLFBT_OVER_DS_DISABLED || FBT_FDAP */

	return CALLROM_ENTRY(wlc$wlc_recv_mgmtact)(wlc, scb, hdr, body, body_len, wrxh, plcp, p);
}
#endif /* WLFBT */

void (wlc$wlc_set_phy_chanspec)(wlc_info_t *wlc, chanspec_t chanspec);
void CALLROM_ENTRY(wlc$wlc_set_phy_chanspec)(wlc_info_t *wlc, chanspec_t chanspec);
void (wlc$wlc_set_phy_chanspec)(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_bsscfg_t *cfg;
	int idx;

	CALLROM_ENTRY(wlc$wlc_set_phy_chanspec)(wlc, chanspec);
#if defined(RADAR)
	if (WL11H_ENAB(wlc) && AP_ACTIVE(wlc)) {
		wlc_phy_t *pinfo = wlc->band->pi;

		if (wlc->dfs && wlc_dfs_get_radar(wlc->dfs) &&
		    wlc_radar_chanspec(wlc->cmi, chanspec)) {
			wlc_phy_radar_detect_enable(pinfo, TRUE);
		} else {
			wlc_phy_radar_detect_enable(pinfo, FALSE);
		}
	}
#endif /* RADAR */

	FOREACH_BSS(wlc, idx, cfg) {
		struct scb *scb = WLC_BCMCSCB_GET(wlc, cfg);
		if ((scb != NULL) && (scb->bandunit != wlc->band->bandunit)) {
			wlc_internal_scb_switch_band(wlc, scb, wlc->band->bandunit);
			wlc_rateset_filter(&cfg->current_bss->rateset, &scb->rateset, FALSE,
				WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, cfg));
		}
	}
}

void wlc$wlc_frameburst_limit(wlc_info_t *wlc);
void CALLROM_ENTRY(wlc_init)(wlc_info_t *wlc);
void wlc_init(wlc_info_t *wlc)
{
	CALLROM_ENTRY(wlc_init)(wlc);

	/* limit frameburst txop by country */
	wlc$wlc_frameburst_limit(wlc);
}

void CALLROM_ENTRY(wlc_set_chanspec)(wlc_info_t *wlc, chanspec_t chanspec);
void wlc_set_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
#ifdef WLCHANIM
	uint32 current_tm;
	uint32 tsf_l;

	tsf_l = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
#endif /* WLCHANIM */

	CALLROM_ENTRY(wlc_set_chanspec)(wlc, chanspec);

#ifdef WLCHANIM
	current_tm = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
	wlc->chanim_info->chanswitch_overhead =  (uint32)ABS((int32)(current_tm - tsf_l));
#endif /* WLCHANIM */
}

/*
 * This is a generic structure for power save implementations
 * Defines parameters for packets per second threshold based power save
 */
typedef struct wlc_pwrsave {
	bool    in_power_save;          /* whether we are in power save mode or not */
	uint8   power_save_check;       /* Whether power save mode check need to be done */
	uint8   stas_assoc_check;       /* check for associated STAs before going to power save */
	uint    in_power_save_counter;  /* how many times in the power save mode */
	uint    in_power_save_secs;     /* how many seconds in the power save mode */
	uint    quiet_time_counter;     /* quiet time before we enter the  power save mode */
	uint    prev_pktcount;          /* total pkt count from the previous second */
	uint    quiet_time;             /* quiet time in the network before we go to power save */
	uint    pps_threshold;          /* pps threshold for power save */
} wlc_pwrsave_t;

typedef struct wlc_rxchain_pwrsave {
#ifdef WL11N
	/* need to save rx_stbc HT capability before enter rxchain_pwrsave mode */
	uint8   ht_cap_rx_stbc;         /* configured rx_stbc HT capability */
#endif // endif
	uint    rxchain;                /* configured rxchains */
	wlc_pwrsave_t pwrsave;
} wlc_rxchain_pwrsave_t;

typedef struct wlc_radio_pwrsave {
	uint8	level;			/* Low, Medium or High Power Savings */
	uint16	on_time;		/* number of  TUs radio is 'on' */
	uint16	off_time;		/* number of  TUs radio is 'off' */
	int	radio_disabled;		/* Whether the radio needs to be disabled now */
	uint    pwrsave_state;		/* radio pwr save state */
	int32   tbtt_skip;		/* num of tbtt to skip */
	bool    cncl_bcn;		/* whether to stop bcn or not in level 1 & 2. */
	struct wl_timer *timer;		/* timer to keep track of duty cycle */
	wlc_pwrsave_t pwrsave;
} wlc_radio_pwrsave_t;

/* Private AP data structure */
typedef struct
{
	struct wlc_ap_info      appub;		/* Public AP interface: MUST BE FIRST */
	wlc_info_t              *wlc;
	wlc_pub_t               *pub;
	bool                    goto_longslot;	/* Goto long slot on next beacon */
	uint                    cs_scan_timer;	/* periodic auto channel scan timer (seconds) */
	uint32                  maxassoc;	/* Max # associations to allow */
	wlc_radio_pwrsave_t	radio_pwrsave;	/* radio duty cycle power save structure */
	uint16                  txbcn_inactivity; /* txbcn inactivity counter */
	uint16                  txbcn_snapshot;	/* snapshot of txbcnfrm register */
	int                     cfgh;		/* ap bsscfg cubby handle */
	bool                    ap_sta_onradar;	/* local AP and STA are on overlapping
						 * radar channel?
						 */
#if defined(EXT_STA) || defined(SPLIT_ASSOC)
	int     scb_handle;			/* scb cubby handle to retrieve data from scb */
#endif /* EXT_STA || SPLIT_ASSOC */
	uint32          pre_tbtt_min_thresh_us;	/* Minimum threshold time before TBTT */
	wlc_rxchain_pwrsave_t	rxchain_pwrsave; /* rxchain reduction power save structure */
	ratespec_t      force_bcn_rspec;	/* force setup beacon ratespec (in unit of 500kbps)
						 */
	bool            wlancoex;		/* flags to save WLAN dual 2G radios coex status */
} wlc_ap_info_pvt_t;

#define PWRSAVE_RXCHAIN 1

void (wlc_ap$wlc_pwrsave_mode_check)(wlc_ap_info_t *ap, int type);
void CALLROM_ENTRY(wlc_ap$wlc_pwrsave_mode_check)(wlc_ap_info_t *ap, int type);
void (wlc_ap$wlc_pwrsave_mode_check)(wlc_ap_info_t *ap, int type)
{
#ifdef RXCHAIN_PWRSAVE
	wlc_ap_info_pvt_t* appvt = (wlc_ap_info_pvt_t*) ap;
	wlc_info_t *wlc = appvt->wlc;

	if (type == PWRSAVE_RXCHAIN) {
		/* Exit power save mode if channel is quiet */
		if (wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) {
			wlc_reset_rxchain_pwrsave_mode(ap);
			return;
		}
	}
#endif /* RXCHAIN_PWRSAVE */

	CALLROM_ENTRY(wlc_ap$wlc_pwrsave_mode_check)(ap, type);
}

#ifndef WL_SAE
void wlc_assoc$wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint assoc_status, bool reassoc, uint bss_type);
void CALLROM_ENTRY(wlc_assoc$wlc_assoc_complete)(wlc_bsscfg_t *cfg, uint status,
	struct ether_addr* addr, uint assoc_status, bool reassoc, uint bss_type);
void wlc_assoc$wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint assoc_status, bool reassoc, uint bss_type)
{
	WL_ASSOC_LT(("assoc cmplt\n"));
	CALLROM_ENTRY(wlc_assoc$wlc_assoc_complete)(cfg, status, addr, assoc_status, reassoc,
		bss_type);
}
#endif /* WL_SAE */
#ifndef BCMSUP_PSK_DISABLED
void CALLROM_ENTRY(wlc_sup_clear_replay)(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);
void wlc_sup_clear_replay(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
#if defined(WLFBT) && !defined(WLFBT_DISABLED)  /* ROM friendly */
	wlc_info_t *wlc = cfg->wlc;

	if (BSSCFG_IS_FBT(cfg, wlc->pub))
#endif /* WLFBT && !WLFBT_DISABLED */
		CALLROM_ENTRY(wlc_sup_clear_replay)(sup_info, cfg);
}
#endif /* BCMSUP_PSK_DISABLED */

#ifndef WL_SAE
void wlc_assoc$wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg);
void CALLROM_ENTRY(wlc_assoc$wlc_assoc_scan_complete)(void *arg, int status, wlc_bsscfg_t *cfg);
void wlc_assoc$wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg)
{
	WL_ASSOC_LT(("assoc scan cmplt\n"));
	CALLROM_ENTRY(wlc_assoc$wlc_assoc_scan_complete)(arg, status, cfg);
}
#endif /* WL_SAE */

void CALLROM_ENTRY(wlc_pmkid_build_cand_list)(struct wlc_bsscfg *cfg, bool check_SSID);
void wlc_pmkid_build_cand_list(struct wlc_bsscfg *cfg, bool check_SSID)
{
#if defined(WLFBT) && !defined(WLFBT_DISABLED)  /* ROM friendly */
	wlc_info_t *wlc = cfg->wlc;

	if (!BSSCFG_IS_FBT(cfg, wlc->pub) || !(cfg->WPA_auth & WPA2_AUTH_FT))
#endif /* WLFBT && !WLFBT_DISABLED */
		CALLROM_ENTRY(wlc_pmkid_build_cand_list)(cfg, check_SSID);
}

void wlc_phy_cmn$wlc_phy_radar_detect_init_acphy(phy_info_t *pi, bool on);
void CALLROM_ENTRY(wlc_phy_cmn$wlc_phy_radar_detect_init_acphy)(phy_info_t *pi, bool on);
void wlc_phy_cmn$wlc_phy_radar_detect_init_acphy(phy_info_t *pi, bool on)
{
	/* Update radar_args according to the chanspec */
	if (CHSPEC_CHANNEL(pi->radio_chanspec) <= WL_THRESHOLD_LO_BAND) {
		if (CHSPEC_IS20(pi->radio_chanspec)) {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_20_lo;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_20_lo;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_40_lo;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_40_lo;
		} else {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_80_lo;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_80_lo;
		}
	} else {
		if (CHSPEC_IS20(pi->radio_chanspec)) {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_20_hi;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_20_hi;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_40_hi;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_40_hi;
		} else {
			pi->ri->rparams.radar_args.thresh0 =
				pi->ri->rparams.radar_thrs.thresh0_80_hi;
			pi->ri->rparams.radar_args.thresh1 =
				pi->ri->rparams.radar_thrs.thresh1_80_hi;
		}
	}
	CALLROM_ENTRY(wlc_phy_cmn$wlc_phy_radar_detect_init_acphy)(pi, on);
}

#include "bcmdevs.h"
#include "wlc_phy_ac.h"
#include "wlc_phy_radio.h"
#if defined(BCMDBG_PHYREGS_TRACE)
#define _MOD_RADIO_REG(pi, reg, mask, val)	mod_radio_reg_debug(pi, reg, mask, val, #reg)
#else
#define _MOD_RADIO_REG(pi, reg, mask, val)	mod_radio_reg(pi, reg, mask, val)
#endif /* BCMDBG_PHYREGS_TRACE */

#define MOD_RADIO_REG(pi, regpfx, regnm, fldname, value) \
	_MOD_RADIO_REG(pi, \
		regpfx##_2069_##regnm, \
		RF_2069_##regnm##_##fldname##_MASK, \
		((value) << RF_2069_##regnm##_##fldname##_SHIFT))

void wlc_phy_ac$wlc_phy_chanspec_radio2069_setup(phy_info_t *pi, const void *chan_info,
	uint8 toggle_logen_reset);
void CALLROM_ENTRY(wlc_phy_ac$wlc_phy_chanspec_radio2069_setup)(phy_info_t *pi,
	const void *chan_info, uint8 toggle_logen_reset);
void wlc_phy_ac$wlc_phy_chanspec_radio2069_setup(phy_info_t *pi, const void *chan_info,
	uint8 toggle_logen_reset)
{
	CALLROM_ENTRY(wlc_phy_ac$wlc_phy_chanspec_radio2069_setup)(pi, chan_info,
		toggle_logen_reset);
	if (!(RADIOMAJORREV(pi->pubpi.radiomajorrev) == 2) &&
		!(RADIOMAJORREV(pi->pubpi.radiomajorrev) == 1) &&
		(pi->u.pi_acphy->srom.gainboosta01 == 1) &&
		!(ACREV_IS(pi->pubpi.phy_rev, 0))) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_RADIO_REG(pi, RFX, PAD2G_IDAC, pad2g_idac_cascode, 0xe);
		}
	}
}
