/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * Beamforming support
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_txbf.h 467328 2014-04-03 01:23:40Z $
 */


#ifndef _wlc_txbf_h_
#define _wlc_txbf_h_


#define TXBF_RATE_OFDM_DFT	0	/* disable txbf for all OFDM rate */
#define TXBF_RATE_MCS_DFT	0xFF	/* enable mcs 0-7 */
#define TXBF_RATE_VHT_DFT	0x3FF	/* enable vht 0-9 */

#define TXBF_N_DFT_BFE_CAP (HT_CAP_TXBF_CAP_NDP_RX | \
	(HT_CAP_TXBF_FB_TYPE_IMMEDIATE << HT_CAP_TXBF_CAP_EXPLICIT_C_FB_SHIFT) | \
	(2 << HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT) | (2 << HT_CAP_TXBF_CAP_CHAN_ESTIM_SHIFT))

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
#define WLC_TXBF_BEAMFORMING_MAX_LINK  7

#ifdef WL_BEAMFORMING
#define TXBF_ACTIVE(wlc) (TXBF_ENAB((wlc)->pub) && ((wlc)->txbf->mode) &&((wlc)->txbf->active))
#else
#define TXBF_ACTIVE(wlc) (0)
#endif /* WL_BEAMFORMING */

extern wlc_txbf_info_t *wlc_txbf_attach(wlc_info_t *wlc);
extern void wlc_txbf_detach(wlc_txbf_info_t *txbf);
extern void wlc_txbf_init(wlc_txbf_info_t *txbf);
extern void wlc_txbf_delete_link(wlc_txbf_info_t *txbf, struct scb *scb);
extern int wlc_txbf_init_link(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf);
extern void wlc_txbf_txchain_set(wlc_txbf_info_t *txbf);
extern void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int idx, const struct ether_addr *addr);
extern void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_update_vht_cap(wlc_txbf_info_t *txbf, struct scb *scb, uint32 vht_cap_info);
extern void wlc_txbf_update_ht_cap(wlc_txbf_info_t *txbf, struct scb *scb,
            ht_cap_ie_t *cap_ie);
extern bool wlc_txbf_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs);
extern bool wlc_txbf_bfen(wlc_txbf_info_t *txbf, struct scb *scb, ratespec_t rspec,
        txpwr204080_t* txpwrs, bool is_imp);
/* these two doesn't need to be external */
extern bool wlc_txbf_exp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb,
        uint8 *shm_index, txpwr204080_t* txpwrs);
extern bool wlc_txbf_imp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb,
        txpwr204080_t* txpwrs);
extern void wlc_txbf_fix_rspec_plcp(wlc_txbf_info_t *txbf, ratespec_t *prspec, uint8 *plcp);
extern void wlc_txbf_clear_init_pending(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_set_init_pending(wlc_txbf_info_t *txbf, struct scb *scb);
extern bool wlc_txbf_is_init_pending(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_txpower_target_max_upd(wlc_txbf_info_t *txbf, int8 max_txpwr_limit);
extern void wlc_txbf_applied2ovr_upd(wlc_txbf_info_t *txbf, bool bfen);
extern void wlc_txbf_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf);
extern void wlc_txbf_upd_virtif_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap);
extern void wlc_txbf_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_impbf_upd(wlc_txbf_info_t *txbf);
extern uint8 wlc_txbf_get_applied2ovr(wlc_txbf_info_t *txbf);
extern void wlc_txbf_imp_txstatus(wlc_txbf_info_t *txbf, struct scb *scb, tx_status_t *txs);
#endif /* _wlc_txbf_h_ */
