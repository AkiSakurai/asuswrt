/*
 * D11 rate and link memory support module for for Broadcom 802.11
 * Networking Adapter Device Drivers
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *
 * $Id$
 */

#ifndef	_WLC_RATELINKMEM_H_
#define	_WLC_RATELINKMEM_H_

struct wlc_ratelinkmem_info; /* opaque module structure */
//typedef struct scb_ratelinkmem_info scb_ratelinkmem_info_t;

/* 'Global' SCB /IDX for broadcast/multicast and fixed rate transmissions */
#define WLC_RLM_SPECIAL_RATE_SCB(wlc)		((wlc)->band->hwrs_scb)
#define WLC_RLM_SPECIAL_RATE_IDX		(AMT_IDX_MAC)
#define WLC_RLM_SPECIAL_LINK_SCB(wlc)		WLC_RLM_SPECIAL_RATE_SCB(wlc)
#define WLC_RLM_SPECIAL_LINK_IDX		WLC_RLM_SPECIAL_RATE_IDX
#define WLC_RLM_BSSCFG_LINK_SCB(wlc, bsscfg)	\
	((bsscfg) != (wlc)->primary_bsscfg && BSSCFG_BCMC_SCB(bsscfg) ? \
		BSSCFG_BCMC_SCB(bsscfg) : WLC_RLM_SPECIAL_LINK_SCB(wlc))
#define WLC_RLM_BSSCFG_LINK_IDX(wlc, bsscfg) \
	((((bsscfg) != (wlc)->primary_bsscfg) && (BSSCFG_BCMC_SCB((bsscfg)) != NULL)) ? \
		wlc_ratelinkmem_vbss_link_index((wlc), (bsscfg)) : \
		WLC_RLM_SPECIAL_LINK_IDX)

#define WLC_RLM_FTM_RATE_IDX		AMT_IDX_FTM_RSVD_START

enum wlc_rlm_special_rate_enum {
	WLC_RLM_SPECIAL_RATE_RSPEC = 0,
	WLC_RLM_SPECIAL_RATE_MRSPEC = 1,
	WLC_RLM_SPECIAL_RATE_BASIC = 2,
	WLC_RLM_SPECIAL_RATE_OFDM = 3
};

typedef struct {
	uint32		flags; /* flags as recorded in HW rate_entry */
	ratespec_t	rspec[D11_REV128_RATEENTRY_NUM_RATES]; /* rspec for rate block */
	uint8		use_rts; /* rts bit per rate block */
} wlc_rlm_rate_store_t;

#define D11_RATEIDX_LOW128_MASK		0x7F /* 127 */

/* helper macro to get RSPEC from store */
#define WLC_RATELINKMEM_GET_RSPEC(rstore, entry)	((rstore) ? \
								(rstore)->rspec[(entry)] : \
								OFDM_RSPEC(WLC_RATE_6M))
/**
 * Module functions
 */
extern wlc_ratelinkmem_info_t *wlc_ratelinkmem_attach(wlc_info_t *wlc);
extern void wlc_ratelinkmem_detach(wlc_ratelinkmem_info_t *ratelinkmem_info);
extern void wlc_ratelinkmem_hw_init(wlc_info_t *wlc);

/**
 * Rate memory related functions
 */
/** return index used by this SCB, return D11_RATE_LINK_MEM_IDX_INVALID in case of error */
extern uint16 wlc_ratelinkmem_get_scb_rate_index(wlc_info_t *wlc, scb_t *scb);
/** return read-only pointer to currently active rate entry */
extern const wlc_rlm_rate_store_t *wlc_ratelinkmem_retrieve_cur_rate_store(wlc_info_t *wlc,
	uint16 index, bool newrate);
/** trigger an update of the rate entry for this SCB, returns error code */
extern int wlc_ratelinkmem_update_rate_entry(wlc_info_t *wlc, scb_t *scb,
	void *rateset, uint16 flags);
/** txstatus indication, used by ratelinkmem to determine current rate_entry, returns error code */
extern int wlc_ratelinkmem_upd_txstatus(wlc_info_t *wlc, uint16 index, uint8 flags);
/** update HW ratemem block for rucfg */
extern int wlc_ratelinkmem_write_rucfg(wlc_info_t *wlc, uint8 *buf, uint sz, uint16 rmem_idx);

/**
 * Link memory related functions
 */
/** return index used by this SCB, return D11_RATE_LINK_MEM_IDX_INVALID in case of error */
extern uint16 wlc_ratelinkmem_get_scb_link_index(wlc_info_t *wlc, scb_t *scb);
/** return read-only pointer to currently active link entry */
extern const d11linkmem_entry_t *wlc_ratelinkmem_retrieve_cur_link_entry(wlc_info_t *wlc,
	uint16 index);
extern scb_t* wlc_ratelinkmem_retrieve_cur_scb(wlc_info_t *wlc, uint16 index);
/** trigger an update of the link entry for this SCB, returns error code */
extern int wlc_ratelinkmem_update_link_entry(wlc_info_t *wlc, scb_t *scb);
/** trigger link entry update for this SCB, including ucode internal data, returns error code */
extern int wlc_ratelinkmem_upd_lmem_int(wlc_info_t *wlc, scb_t *scb, bool clr_txbf_stats);
/** update all link entries, or all link entries for a certain BSSCFG */
extern void wlc_ratelinkmem_update_link_entry_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	bool ampdu_only, bool clr_txbf_stats);
extern uint16 wlc_ratelinkmem_vbss_link_index(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_ratelinkmem_update_link_twtschedblk(wlc_info_t *wlc, uint idx, uint8* twtschedblk,
	uint16 size);
extern uint32 wlc_ratelinkmem_lmem_read_word(wlc_info_t *wlc, uint16 index, int offset);

/**
 * Provided dump functions
 */
/** dump a single link memory entry */
extern void wlc_ratelinkmem_link_entry_dump(wlc_info_t *wlc, d11linkmem_entry_t *link_entry,
	struct bcmstrbuf *b);
/** dump a single rate entry's rate store info */
extern void wlc_ratelinkmem_rate_store_dump(wlc_info_t *wlc,
	const wlc_rlm_rate_store_t *rate_store, struct bcmstrbuf *b);

/**
 * Generic notification functions
 */
/** band set/change notification */
extern void wlc_ratelinkmem_update_band(wlc_info_t *wlc);
extern bool wlc_ratelinkmem_lmem_isvalid(wlc_info_t *wlc, scb_t *scb);
/** SCB AMT index release notification */
extern void wlc_ratelinkmem_scb_amt_release(wlc_info_t *wlc, scb_t *scb, int index);
#if defined(BCMDBG) || defined(WLTEST) || defined(DUMP_TXBF)
extern void wlc_ratelinkmem_raw_lmem_read(wlc_info_t *wlc, uint16 index, d11linkmem_entry_t *lnk,
	int size);
#else
#define wlc_ratelinkmem_raw_lmem_read(a, b, c, d) do {} while (0)
#endif // endif
#endif	/* _WLC_RATELINKMEM_H_ */
