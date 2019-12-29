/*
 * wlc_rx.h
 *
 * Common headers for receive datapath components
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
 * $Id: wlc_rx.h 774257 2019-04-17 10:08:19Z $
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

typedef enum wlc_rx_ststus
{
	WLC_RX_STS_OK				= 0,
	WLC_RX_STS_TOSS_NOT_IN_BSS		= 2,
	WLC_RX_STS_TOSS_AP_BSSCFG		= 3,
	WLC_RX_STS_TOSS_MAC_ADDR_RACE		= 4,
	WLC_RX_STS_TOSS_NON_WDS_NON_BSS		= 5,
	WLC_RX_STS_TOSS_AMSDU_DISABLED		= 6,
	WLC_RX_STS_TOSS_DROP_FRAME		= 7,
	WLC_RX_STS_TOSS_NO_SCB			= 8,
	WLC_RX_STS_TOSS_BAD_DS			= 9,
	WLC_RX_STS_TOSS_MCAST_AP		= 10,
	WLC_RX_STS_TOSS_INV_MCS			= 11,
	WLC_RX_STS_TOSS_BUF_FAIL		= 12,
	WLC_RX_STS_TOSS_RXS_FCSERR		= 13,
	WLC_RX_STS_TOSS_RUNT_FRAME		= 14,
	WLC_RX_STS_TOSS_BAD_PROTO		= 15,
	WLC_RX_STS_TOSS_RXFRAG_ERR		= 16,
	WLC_RX_STS_TOSS_SAFE_MODE_AMSDU		= 17,
	WLC_RX_STS_TOSS_PKT_LEN_SHORT		= 18,
	WLC_RX_STS_TOSS_8021X_FRAME		= 19,
	WLC_RX_STS_TOSS_EAPOL_SUP		= 20,
	WLC_RX_STS_TOSS_EAPOL_AUTH		= 21,
	WLC_RX_STS_TOSS_INV_SRC_MAC		= 22,
	WLC_RX_STS_TOSS_FRAG_NOT_ALLOWED	= 23,
	WLC_RX_STS_TOSS_BAD_DEAGG_LEN		= 24,
	WLC_RX_STS_TOSS_DEAGG_UNALIGNED		= 25,
	WLC_RX_STS_TOSS_BAD_DEAGG_SF_LEN	= 26,
	WLC_RX_STS_TOSS_PROMISC_OTHER		= 27,
	WLC_RX_STS_TOSS_STA_DWDS		= 28,
	WLC_RX_STS_TOSS_DECRYPT_FAIL		= 29,
	WLC_RX_STS_TOSS_MIC_CHECK_FAIL		= 30,
	WLC_RX_STS_TOSS_NO_ROOM_RX_CTX		= 31,
	WLC_RX_STS_TOSS_3_ADDR_FRAME		= 32,
	WLC_RX_STS_TOSS_AMPDU_DUP		= 33,
	WLC_RX_STS_TOSS_ICV_ERR			= 34,
	WLC_RX_STS_TOSS_UNKNOWN			= 35,
	WLC_RX_STS_TOSS_NOT_WAI			= 36,
	WLC_RX_STS_TOSS_RXS_PLCP_INV		= 37,
	WLC_RX_STS_TOSS_NON_TDLS		= 38,
	WLC_RX_STS_TOSS_MCAST_STA		= 39,
	WLC_RX_STS_LAST				= 40
} wlc_rx_status_t;

#define RX_STS_REASON_STRINGS \
	"OK",				/* WLC_RX_STS_OK			= 0 */ \
	"non_AWDL_pkt_on_AWDL_link",	/* WLC_RX_STS_TOSS_RX_NON_AWDL		= 1 */ \
	"promisc_mode_not_in_bss",	/* WLC_RX_STS_TOSS_RX_NOT_IN_BSS	= 2 */ \
	"promisc_mode_AP_config",	/* WLC_RX_STS_TOSS_RX_AP_BSSCFG		= 3 */ \
	"mac_addr_not_updated",		/* WLC_RX_STS_TOSS_RX_MAC_ADDR_RACE	= 4 */ \
	"non_wds_frame_not_in_bss",	/* WLC_RX_STS_TOSS_RX_NON_WDS_NON_BSS	= 5 */ \
	"AMSDU_disabled",		/* WLC_RX_STS_TOSS_RX_AMSDU_DISABLED	= 6 */ \
	"drop_frame",			/* WLC_RX_STS_TOSS_RX_DROP_FRAME	= 7 */ \
	"no_scb",			/* WLC_RX_STS_TOSS_RX_NO_SCB		= 8 */ \
	"bad_ds_field",			/* WLC_RX_STS_TOSS_RX_BAD_DS		= 9 */ \
	"mcast_in_AP_mode",		/* WLC_RX_STS_TOSS_RX_MCAST_AP		= 10 */ \
	"invalid_mcs",			/* WLC_RX_STS_TOSS_RX_INV_MCS		= 11 */ \
	"buf_alloc_fail",		/* WLC_RX_STS_TOSS_RX_BUF_FAIL		= 12 */ \
	"pkt_with_bad_fcs",		/* WLC_RX_STS_TOSS_RX_RXS_FCSERR	= 13 */ \
	"runt_frame",			/* WLC_RX_STS_TOSS_RX_RUNT_FRAME	= 14 */ \
	"pkt_type_invalid",		/* WLC_RX_STS_TOSS_RX_BAD_PROTO		= 15 */ \
	"missed_frags_error",		/* WLC_RX_STS_TOSS_RX_RXFRAG_ERR	= 16 */ \
	"in_safe_mode_an_amsdu",	/* WLC_RX_STS_TOSS_RX_SAFE_MODE_AMSDU	= 17 */ \
	"packet_length_too_short",	/* WLC_RX_STS_TOSS_RX_PKT_LEN_SHORT	= 18 */ \
	"non_8021x_frame",		/* WLC_RX_STS_TOSS_RX_8021X_FRAME	= 19 */ \
	"eapol_supplicant",		/* WLC_RX_STS_TOSS_RX_EAPOL_SUP		= 20 */ \
	"eapol_authunticator",		/* WLC_RX_STS_TOSS_RX_EAPOL_AUTH	= 21 */ \
	"invalid_src_mac",		/* WLC_RX_STS_TOSS_RX_INV_SRC_MAC	= 22 */ \
	"fragments_not_allowed",	/* WLC_RX_STS_TOSS_RX_FRAG_NOT_ALLOWED	= 23 */ \
	"bad_deagg_len",		/* WLC_RX_STS_TOSS_RX_BAD_DEAGG_LEN	= 24 */ \
	"deagg_sf_body_unaligned",	/* WLC_RX_STS_TOSS_RX_DEAGG_UNALIGNED	= 25 */ \
	"deagg_sflen_mismatch",		/* WLC_RX_STS_TOSS_RX_BAD_DEAGG_SF_LEN	= 26 */ \
	"promisc_pkt_other",		/* WLC_RX_STS_TOSS_RX_PROMISC_OTHER	= 27 */ \
	"sta_bsscfg_and_dwds",		/* WLC_RX_STS_TOSS_RX_STA_DWDS		= 28 */ \
	"decrypt_fail",			/* WLC_RX_STS_TOSS_RX_DECRYPT_FAIL	= 29 */ \
	"mic_check_fail",		/* WLC_RX_STS_TOSS_RX_MIC_CHECK_FAIL	= 30 */ \
	"no_room_for_Rx_ctx",		/* WLC_RX_STS_TOSS_RX_NO_ROOM_RX_CTX	= 31 */ \
	"toss_3_addr_frame",		/* WLC_RX_STS_TOSS_RX_3_ADDR_FRAME	= 32 */ \
	"ampdu_seq_invalid",		/* WLC_RX_STS_TOSS_AMPDU_DUP		= 33 */ \
	"icv_error",			/* WLC_RX_STS_TOSS_ICV_ERR		= 34 */ \
	"Unknown_toss_reason"		/* WLC_RX_STS_TOSS_UNKNOWN		= 35 */ \
	"not WAI"			/* WLC_RX_STS_TOSS_NOT_WAI		= 36 */ \
	"plcp_invalid",			/* WLC_RX_STS_TOSS_RXS_PLCP_INV		= 37 */ \
	"tdls_filtered",		/* WLC_RX_STS_TOSS_NON_TDLS		= 38 */ \
	"multicast_rxed_onsta"		/* WLC_RX_STS_TOSS_MCAST_STA		= 39 */ \
	"last"				/* WLC_RX_STS_LAST = 40			*/
#define RX_STS_RSN_MAX	(WLC_RX_STS_LAST)

#define RXS_SHORT_ENAB(rev)	(D11REV_GE(rev, 64) || \
				D11REV_IS(rev, 60) || \
				D11REV_IS(rev, 61) || \
				D11REV_IS(rev, 62))

#define IS_D11RXHDRSHORT(rxh, rev) ((RXS_SHORT_ENAB(rev) && \
	((D11RXHDR_ACCESS_VAL(rxh, rev, dma_flags)) & RXS_SHORT_MASK)) != 0)

#define RXHDR_GET_PAD_LEN(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	(((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs) & \
	RXSS_PBPRES) != 0) ? HDRCONV_PAD : 0) : D11REV_GE((wlc)->pub->corerev, 128) ? \
	(((D11RXHDR_GE128_ACCESS_VAL(rxh, mrxs) & RXSS_PBPRES) != 0) ? HDRCONV_PAD : 0) : \
	(((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus1) & RXS_PBPRES) != 0) ? HDRCONV_PAD : 0))

#define RXHDR_GET_AMSDU(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, \
	mrxs) & RXSS_AMSDU_MASK) != 0) : D11REV_GE((wlc)->pub->corerev, 128) ? \
	((D11RXHDR_GE128_ACCESS_VAL(rxh, mrxs) & RXSS_AMSDU_MASK) != 0) : \
	((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus2) & RXS_AMSDU_MASK) != 0))

#define RXHDR_GET_CONV_TYPE(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, \
	HdrConvSt) & HDRCONV_ETH_FRAME) != 0) : ((D11RXHDR_ACCESS_VAL(rxh, \
	(wlc)->pub->corerev, HdrConvSt) & HDRCONV_ETH_FRAME) != 0))

#define RXHDR_GET_AGG_TYPE(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs) & \
	RXSS_AGGTYPE_MASK) >> RXSS_AGGTYPE_SHIFT) : D11REV_GE((wlc)->pub->corerev, 128) ? \
	((D11RXHDR_GE128_ACCESS_VAL(rxh, mrxs) & RXSS_AGGTYPE_MASK) >> RXSS_AGGTYPE_SHIFT) : \
	((D11RXHDR_LT80_ACCESS_VAL(rxh, RxStatus2) & RXS_AGGTYPE_MASK) >> RXS_AGGTYPE_SHIFT))

#if defined(BCMDBG) || defined(BCMDBG_AMSDU) || defined(WLSCB_HISTO)
#define RXHDR_GET_MSDU_COUNT(rxh, wlc) (IS_D11RXHDRSHORT(rxh, (wlc)->pub->corerev) ? \
	(((D11RXHDRSHORT_ACCESS_VAL(rxh, (wlc)->pub->corerev, (wlc)->pub->corerev_minor, mrxs)) & \
	RXSS_MSDU_CNT_MASK) >> RXSS_MSDU_CNT_SHIFT) : D11REV_GE((wlc)->pub->corerev, 128) ? \
	((D11RXHDR_GE128_ACCESS_VAL(rxh, mrxs) & RXSS_MSDU_CNT_MASK) >> RXSS_MSDU_CNT_SHIFT) : 0)
#endif /* BCMDBG | BCMDBG_AMSDU | WLSCB_HISTO */

#ifdef WL_RX_STALL

typedef enum {
	RX_HC_NOSCB =	0,
	RX_HC_ICVERR =	1,
	RX_HC_REPLAY =	2,
	RX_HC_AMPDU_DUP = 3,
	RX_HC_DROPPED_LAST
} wlc_rx_hc_drop_reason;

#define RX_HC_REASON_STRINGS \
	"No SCB",		/* RX_HC_NOSCB	*/ \
	"ICV error",		/* RX_HC_ICVERR	*/ \
	"Replay detected",	/* RX_HC_REPLAY */ \
	"AMPDU dup"		/* RX_HC_AMPDU_DUP */

#ifndef RX_HC_FRAMECNT
#define RX_HC_FRAMECNT 100
#endif // endif

#ifndef RX_HC_TIMES_HISTORY
#define RX_HC_TIMES_HISTORY 0
#endif /* RX_HC_TIMES_HISTORY */

#ifndef RX_HC_ALERT_THRESHOLD
#define RX_HC_ALERT_THRESHOLD 90
#endif // endif

#ifndef RX_HC_STALL_CNT
#define RX_HC_STALL_CNT 9
#endif // endif

typedef struct wlc_rx_hc_err_info {
	uint32 sum;
	uint32 dropped;
	uint32 fail_rate;
	uint32 ac;
	uint32 stall_bitmap0;
	uint32 stall_bitmap1;
	uint32 threshold;
	uint32 sample_size;
	char  prefix[ETHER_ADDR_STR_LEN+1];
} wlc_rx_hc_err_info_t;

typedef struct wlc_rx_hc_counters {
	uint32  rx_pkts[AC_COUNT][WLC_RX_STS_LAST];
	uint32  dropped_all[AC_COUNT];
	uint32  ts[AC_COUNT];
	uint32  stall_cnt[AC_COUNT];
} wlc_rx_hc_counters_t;

struct wlc_rx_hc {
	wlc_info_t * wlc;
	int scb_handle;
	int cfg_handle;
	wlc_rx_hc_counters_t counters;
	uint32  rx_hc_pkts;
	uint32  rx_hc_dropped_all;
	uint32  rx_hc_dropped[RX_HC_DROPPED_LAST];
	uint32  rx_hc_ts;
	uint32  rx_hc_alert_th;
	uint32  rx_hc_cnt;
	uint32	rx_hc_stall_cnt;
#if RX_HC_TIMES_HISTORY > 0
	struct	{
		uint32  sum;
		uint32	start_ts;
		uint32	end_ts;
		uint32	dropped;
		uint32  ac;
		char  prefix[ETHER_ADDR_STR_LEN];
		int	reason;
	} entry[RX_HC_TIMES_HISTORY];
	int curr_index;
#endif // endif
	wlc_rx_hc_err_info_t error;
};

#endif /* WL_RX_STALL */

void BCMFASTPATH
wlc_recv(wlc_info_t *wlc, void *p);

void BCMFASTPATH
wlc_recvdata(wlc_info_t *wlc, osl_t *osh, wlc_d11rxhdr_t *wrxh, void *p);

void BCMFASTPATH
wlc_recvdata_ordered(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f);

void BCMFASTPATH
wlc_recvdata_sendup(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f);

void BCMFASTPATH
wlc_sendup(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, void *pkt);

void wlc_sendup_event(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, void *pkt);

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

uint16 wlc_recv_mgmt_rx_channel_get(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh);
extern int wlc_process_eapol_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
		struct scb *scb, struct wlc_frminfo *f, void *pkt);

#ifdef WL_RX_STALL
#if defined(BCMDBG)
int wlc_rx_activity_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif
wlc_rx_hc_t *wlc_rx_hc_attach(wlc_info_t * wlc);
void wlc_rx_hc_detach(wlc_rx_hc_t * rx_hc);
int wlc_rx_healthcheck_update_counters(wlc_info_t * wlc, int ac, scb_t * scb,
	wlc_bsscfg_t * bsscfg, int rx_status, int count);
int wlc_rx_healthcheck_verify(wlc_info_t *wlc,
	wlc_rx_hc_counters_t *counters, int ac, const char * prefix);
void wlc_rx_healthcheck_report(wlc_info_t *wlc);
int wlc_rx_healthcheck(uint8* buffer_ptr, uint16 remaining_len,
	void* context, int16* bytes_written);
const char *wlc_rx_dropped_get_name(int reason);
void wlc_rx_healthcheck_force_fail(wlc_info_t *wlc);
#endif /* WL_RX_STALL */

void
wlc_rxhdr_set_pad_present(d11rxhdr_t *rxh, wlc_info_t *wlc);
void
wlc_rxhdr_clear_pad_present(d11rxhdr_t *rxh, wlc_info_t *wlc);
#define PKTC_MIN_FRMLEN(wlc) (D11_PHY_RXPLCP_LEN(wlc->pub->corerev) + DOT11_A3_HDR_LEN + \
				DOT11_QOS_LEN + DOT11_FCS_LEN)

#endif /* _wlc_rx_h */
