/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * Beamforming support
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_txbf.h 778680 2019-09-06 20:55:37Z $
 */

#ifndef _wlc_txbf_h_
#define _wlc_txbf_h_

#define TXBF_RATE_OFDM_DFT	0	/* disable txbf for all OFDM rate */
#define TXBF_RATE_MCS_DFT	0xFF	/* enable mcs 0-7 */
#define TXBF_RATE_VHT_DFT	0x3FF	/* enable vht 0-9 */

#define HT_CAP_TXBF_CAP_C_BFR_ANT_DEF_VAL	2
#define TXBF_N_DFT_BFE_CAP (HT_CAP_TXBF_CAP_NDP_RX | \
	(HT_CAP_TXBF_FB_TYPE_IMMEDIATE << HT_CAP_TXBF_CAP_EXPLICIT_C_FB_SHIFT) | \
	(HT_CAP_TXBF_CAP_C_BFR_ANT_DEF_VAL << HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT) | \
	(2 << HT_CAP_TXBF_CAP_CHAN_ESTIM_SHIFT))

#define TXBF_N_BFE_CAP_MASK	(HT_CAP_TXBF_CAP_NDP_RX | \
		HT_CAP_TXBF_CAP_EXPLICIT_C_FB_MASK)

#define TXBF_N_DFT_BFR_CAP (HT_CAP_TXBF_CAP_NDP_TX | HT_CAP_TXBF_CAP_EXPLICIT_C_STEERING)

#define TXBF_N_DFT_CAP ((TXBF_N_DFT_BFE_CAP) | (TXBF_N_DFT_BFR_CAP))

#define TXBF_N_SUPPORTED_BFR(txbf_cap) (((txbf_cap) & \
		(TXBF_N_DFT_BFR_CAP)) == (TXBF_N_DFT_BFR_CAP))
#define TXBF_N_SUPPORTED_BFE(txbf_cap) (((txbf_cap) & (TXBF_N_BFE_CAP_MASK)) == \
		(HT_CAP_TXBF_CAP_NDP_RX | \
		(HT_CAP_TXBF_FB_TYPE_IMMEDIATE << HT_CAP_TXBF_CAP_EXPLICIT_C_FB_SHIFT)))
/* invalid shm index */
#define BF_SHM_IDX_INV 0xFF
#define WLC_TXBF_FLAG_IMPBF 1
#define TXBF_MAX_LINK  7
#define TXBF_MAX_LINK_EXT  16
#define TXBF_MU_MAX_LINKS 8
#define TXBF_MAX_SU_USRIDX_GE64	12	/* max su BFI idx block */

#ifdef WL_BEAMFORMING
#define TXBF_ACTIVE(wlc) (TXBF_ENAB((wlc)->pub) && ((wlc)->txbf->mode) &&((wlc)->txbf->active))
extern bool wlc_txbf_mutx_enabled(wlc_txbf_info_t *txbf, uint8 tx_type);
extern bool wlc_txbf_murx_capable(wlc_txbf_info_t *txbf);
#else
#define TXBF_ACTIVE(wlc) (0)
#define wlc_txbf_mutx_enabled(x) FALSE
#define wlc_txbf_murx_capable(x) FALSE
#endif /* WL_BEAMFORMING */

extern void wlc_txbf_mutimer_update(wlc_txbf_info_t *txbf, bool force_disable);

#define TXBF_SU_BFR_CAP		0x01
#define TXBF_MU_BFR_CAP		0x02
#define TXBF_HE_SU_BFR_CAP	0x04
#define TXBF_HE_MU_BFR_CAP	0x08
#define TXBF_HE_CQI_BFR_CAP	0x10
#define TXBF_SU_MU_BFR_CAP	(TXBF_SU_BFR_CAP | TXBF_MU_BFR_CAP) /* 0x03 */
#define TXBF_HE_SU_MU_BFR_CAP	(TXBF_HE_SU_BFR_CAP | TXBF_HE_MU_BFR_CAP) /* 0x0c */

#define TXBF_SU_BFE_CAP		0x01
#define TXBF_MU_BFE_CAP		0x02
#define TXBF_HE_SU_BFE_CAP	0x04
#define TXBF_HE_MU_BFE_CAP	0x08 /* In HE, MUBFE=1 if SUBFE=1 */
#define TXBF_HE_CQI_BFE_CAP	0x10
#define TXBF_SU_MU_BFE_CAP	(TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP) /* 0x03 */
#define TXBF_HE_SU_MU_BFE_CAP	(TXBF_HE_SU_BFE_CAP | TXBF_HE_MU_BFE_CAP) /* 0x0c */

#define TXBF_MU_SOUND_PERIOD_DFT		50	/* VHT MUMIMO 50ms */
#define TXBF_HE_DLOFDMA_SOUND_PERIOD_DFT	100	/* HE DL OFDMA 100ms */

#define TXBF_OFF	0 /* Disable txbf for all rates */
#define TXBF_AUTO	1 /* Enable transmit frames with TXBF considering
			   * regulatory per rate
			   */
#define TXBF_ON		2 /* Force txbf for all rates */

#define IS_LEGACY_IMPBF_SUP(corerev) (D11REV_IS(corerev, 42) || D11REV_IS(corerev, 49))
#define IS_PHYBW_IMPBF_SUP(wlc, chanspec) \
	!(ACREV_IS(wlc->band->phyrev, 47) && \
	CHSPEC_IS160(chanspec))

#define TXBF_CAP_TYPE_HT	0
#define TXBF_CAP_TYPE_VHT	1
#define TXBF_CAP_TYPE_HE	2
#define TXBF_CAP_IS_HT(tp)	((tp) == TXBF_CAP_TYPE_HT)
#define TXBF_CAP_IS_VHT(tp)	((tp) == TXBF_CAP_TYPE_VHT)
#define TXBF_CAP_IS_HE(tp)	((tp) == TXBF_CAP_TYPE_HE)

#define MUMAX_NSTS_ALLOWED	2

extern wlc_txbf_info_t *wlc_txbf_attach(wlc_info_t *wlc);
extern void wlc_txbf_detach(wlc_txbf_info_t *txbf);
extern void wlc_txbf_init(wlc_txbf_info_t *txbf);
extern void wlc_txbf_delete_link(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_delete_all_link(wlc_txbf_info_t *txbf);
extern int wlc_txbf_init_link(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf);
extern void wlc_txbf_txchain_set(wlc_txbf_info_t *txbf);
extern void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int idx, const struct ether_addr *addr);
extern void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, scb_t *scb);
extern bool wlc_txbf_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, scb_t *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs);
extern bool wlc_txbf_bfen(wlc_txbf_info_t *txbf, scb_t *scb, ratespec_t rspec,
        txpwr204080_t* txpwrs, bool is_imp);
extern void wlc_txbf_fix_rspec_plcp(wlc_txbf_info_t *txbf, ratespec_t *prspec, uint8 *plcp,
wl_tx_chains_t txbf_chains);
extern void wlc_txbf_clear_init_pending(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_set_init_pending(wlc_txbf_info_t *txbf, scb_t *scb);
extern bool wlc_txbf_is_init_pending(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_txpower_target_max_upd(wlc_txbf_info_t *txbf, int8 max_txpwr_limit);
extern void wlc_txbf_applied2ovr_upd(wlc_txbf_info_t *txbf, bool bfen);
extern void wlc_txbf_vht_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap);
extern void wlc_txbf_ht_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap);
extern void wlc_txbf_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_impbf_upd(wlc_txbf_info_t *txbf);
extern uint8 wlc_txbf_get_applied2ovr(wlc_txbf_info_t *txbf);
extern void wlc_txbf_imp_txstatus(wlc_txbf_info_t *txbf, scb_t *scb, tx_status_t *txs);
extern uint8 wlc_txbf_get_bfe_sts_cap(wlc_txbf_info_t *txbf, scb_t *scb);
#ifdef WL_MUPKTENG
extern int wlc_txbf_mupkteng_addsta(wlc_txbf_info_t *txbf, scb_t *scb,
        uint8 idx, uint8 rxchain);
extern int wlc_txbf_mupkteng_clrsta(wlc_txbf_info_t *txbf, scb_t *scb);
#endif // endif
#if defined(WL_BEAMFORMING) && !defined(WLTXBF_DISABLED)
extern bool wlc_txbf_bfmspexp_enable(wlc_txbf_info_t *txbf);
#endif /* WL_BEAMFORMING && !defined(WLTXBF_DISABLED) */
extern bool wlc_txbf_bfrspexp_enable(wlc_txbf_info_t *txbf);
extern uint8 wlc_txbf_get_bfi_idx(wlc_txbf_info_t *txbf, scb_t *scb);
#if defined(WLTEST) || defined(WLPKTENG)
extern int8 wlc_txbf_get_mu_txvidx(wlc_info_t *wlc, scb_t *scb);
#endif // endif
#ifdef WL_PSMX
extern uint8 wlc_txbf_get_mubfi_idx(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_scb_ps_notify(wlc_txbf_info_t *txbf, scb_t *scb, bool ps_on);
#else
#define wlc_txbf_get_mubfi_idx(a, b) BF_SHM_IDX_INV
#define wlc_txbf_scb_ps_notify(a, b, c) do {} while (0)
#endif // endif
#ifdef WL_MU_TX
extern bool wlc_txbf_is_mu_bfe(wlc_txbf_info_t *txbf, scb_t *scb);
extern uint8 wlc_txbf_get_free_su_bfr_links(wlc_txbf_info_t *txbf);
#endif /* WL_MU_TX */
extern void wlc_txbf_scb_state_upd(wlc_txbf_info_t *txbf, scb_t *scb,
	uint8* cap_ptr, uint cap_len, int8 cap_tp);
extern void wlc_txbf_link_upd(wlc_txbf_info_t *txbf, scb_t *scb);
extern int wlc_txbf_mu_max_links_upd(wlc_txbf_info_t *txbf, uint8 num);
extern void wlc_txbf_chanspec_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_fill_link_entry(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, scb_t *scb,
	d11linkmem_entry_t *link_entry);
extern uint8 wlc_txbf_get_txbf_tx(wlc_txbf_info_t *txbf);
extern void wlc_txbf_link_entry_dump(d11linkmem_entry_t *link_entry, struct bcmstrbuf *b);
extern void wlc_forcesteer_gerev129(wlc_info_t *wlc, uint8 enable);
extern void wlc_txbf_set_mu_sounding_period(wlc_txbf_info_t *txbf, uint16 val);
uint16 wlc_txbf_get_mu_sounding_period(wlc_txbf_info_t *txbf);
extern uint8 wlc_txbf_get_bfr_cap(wlc_txbf_info_t *txbf);
extern uint8 wlc_txbf_get_bfe_cap(wlc_txbf_info_t *txbf);
extern void wlc_txbf_bfi_init(wlc_txbf_info_t *txbf);
extern bool wlc_txbf_is_hemmu_enab(wlc_txbf_info_t *txbf, scb_t *scb);
extern void wlc_txbf_tbcap_update(wlc_txbf_info_t *txbf, scb_t *scb);

#if defined(BCMDBG)
extern int wlc_txbf_tbcap_check(wlc_txbf_info_t *txbf, scb_t *scb, uint8 mu_type);
#else
#define wlc_txbf_tbcap_check(a, b, c) (BCME_OK)
#endif // endif
extern bool wlc_txbf_autotxvcfg_get(wlc_txbf_info_t *txbf, bool add);
#endif /* _wlc_txbf_h_ */
