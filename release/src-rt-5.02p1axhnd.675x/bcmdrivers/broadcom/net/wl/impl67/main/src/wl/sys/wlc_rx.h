/*
 * wlc_rx.h
 *
 * Common headers for receive datapath components
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
 * $Id: wlc_rx.h 785924 2020-04-09 06:06:27Z $
 *
 */
#ifndef _wlc_rx_h_
#define _wlc_rx_h_

#include <typedefs.h>
#include <osl.h>
#include <d11_cfg.h>
#include <wlc_cfg.h>

#define SPLITRX_DYN_MODE3(osh, p)	(RXFIFO_SPLIT() && !PKTISHDRCONVTD(osh, p))
#define SPLITRX_DYN_MODE4(osh, p)	(PKTISHDRCONVTD(osh, p))

#define PKT_CLASSIFY_EN(x)	((PKT_CLASSIFY()) && (PKT_CLASSIFY_FIFO == (x)))

#define HDRCONV_PAD	2

#define RXS_SHORT_ENAB(rev)	(D11REV_GE(rev, 64) || \
				D11REV_IS(rev, 60) || \
				D11REV_IS(rev, 61) || \
				D11REV_IS(rev, 62))

#define IS_D11RXHDRSHORT(rxh, rev) ((RXS_SHORT_ENAB(rev) && \
	((D11RXHDR_ACCESS_VAL(rxh, rev, dma_flags)) & RXS_SHORT_MASK)) != 0)

#define RXHDR_GET_PAD_LEN(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	(((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs) & \
	RXSS_PBPRES) != 0) ? HDRCONV_PAD : 0) : D11REV_GE((wlc)->pub->corerev, 129) ? \
	(((D11RXHDR_GE129_ACCESS_VAL(rxh, mrxs) & RXSS_PBPRES) != 0) ? HDRCONV_PAD : 0) : \
	(((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus1) & RXS_PBPRES) != 0) ? HDRCONV_PAD : 0))

#define RXHDR_GET_AMSDU(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, \
	mrxs) & RXSS_AMSDU_MASK) != 0) : D11REV_GE((wlc)->pub->corerev, 129) ? \
	((D11RXHDR_GE129_ACCESS_VAL(rxh, mrxs) & RXSS_AMSDU_MASK) != 0) : \
	((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus2) & RXS_AMSDU_MASK) != 0))

#define RXHDR_GET_CONV_TYPE(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, \
	HdrConvSt) & HDRCONV_ETH_FRAME) != 0) : ((D11RXHDR_ACCESS_VAL(rxh, \
	(wlc)->pub->corerev, HdrConvSt) & HDRCONV_ETH_FRAME) != 0))

#define RXHDR_GET_AGG_TYPE(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs) & \
	RXSS_AGGTYPE_MASK) >> RXSS_AGGTYPE_SHIFT) : D11REV_GE((wlc)->pub->corerev, 129) ? \
	((D11RXHDR_GE129_ACCESS_VAL(rxh, mrxs) & RXSS_AGGTYPE_MASK) >> RXSS_AGGTYPE_SHIFT) : \
	((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus2) & RXS_AGGTYPE_MASK) >> RXS_AGGTYPE_SHIFT))

#if defined(BCMDBG) || defined(BCMDBG_AMSDU) || defined(WLSCB_HISTO)
#define RXHDR_GET_MSDU_COUNT(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	(((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs)) & \
	RXSS_MSDU_CNT_MASK) >> RXSS_MSDU_CNT_SHIFT) : D11REV_GE((wlc)->pub->corerev, 129) ? \
	((D11RXHDR_GE129_ACCESS_VAL(rxh, mrxs) & RXSS_MSDU_CNT_MASK) >> RXSS_MSDU_CNT_SHIFT) : 0)
#endif /* BCMDBG | BCMDBG_AMSDU | WLSCB_HISTO */

void BCMFASTPATH
wlc_recv(wlc_info_t *wlc, void *p);

void BCMFASTPATH
wlc_recvdata(wlc_info_t *wlc, osl_t *osh, wlc_d11rxhdr_t *wrxh, void *p);

void BCMFASTPATH
wlc_recvdata_ordered(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f);

void BCMFASTPATH
wlc_recvdata_sendup_msdus(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f);

void BCMFASTPATH
wlc_sendup_msdus(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, void *pkt);

void wlc_sendup_event(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, void *pkt);

extern int BCMFASTPATH wlc_recvdata_sendpkt(wlc_info_t *wlc, void *p, wlc_if_t *wlcif);

extern uint32 wlc_he_sig_a1_from_plcp(uint8 *plcp);
extern uint32 wlc_he_sig_a2_from_plcp(uint8 *plcp);
extern void wlc_tsf_adopt_bcn(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn);

extern void
wlc_bcn_ts_tsf_calc(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh,
	void *plcp, uint32 *tsf_h, uint32 *tsf_l);

extern void wlc_bcn_tsf_diff(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, void *plcp,
	struct dot11_bcn_prb *bcn, int32 *diff_h, int32 *diff_l);
extern int wlc_bcn_tsf_later(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, void *plcp,
	struct dot11_bcn_prb *bcn);
int wlc_arq_pre_parse_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 ft,
	uint8 *ies, uint ies_len, wlc_pre_parse_frame_t *ppf);
int wlc_scan_pre_parse_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 ft,
	uint8 *ies, uint ies_len, wlc_pre_parse_frame_t *ppf);

#if defined(BCMSPLITRX)
extern void wlc_pktfetch_get_scb(wlc_info_t *wlc, wlc_frminfo_t *f,
        wlc_bsscfg_t **bsscfg, struct scb **scb, bool promisc_frame, uint32 ctx_assoctime);
#endif /* BCMSPLITRX */

#if defined(PKTC) || defined(PKTC_DONGLE) || defined(PKTC_TX_DONGLE)
extern bool wlc_rxframe_chainable(wlc_info_t *wlc, void **pp, uint16 index);
extern void wlc_sendup_chain(wlc_info_t *wlc, void *p);
#endif /* PKTC || PKTC_DONGLE */

chanspec_t wlc_recv_mgmt_rx_chspec_get(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh);
extern int wlc_process_eapol_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
		struct scb *scb, struct wlc_frminfo *f, void *pkt);

void
wlc_rxhdr_set_pad_present(d11rxhdr_t *rxh, wlc_info_t *wlc);
void
wlc_rxhdr_clear_pad_present(d11rxhdr_t *rxh, wlc_info_t *wlc);
#define PKTC_MIN_FRMLEN(wlc) (D11_PHY_RXPLCP_LEN(wlc->pub->corerev) + DOT11_A3_HDR_LEN + \
				DOT11_QOS_LEN + DOT11_FCS_LEN)

#endif /* _wlc_rx_h */
