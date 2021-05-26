/*
 * Header for the common Pktfetch use cases in WLC
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
 * $Id: wlc_pktfetch.h 782503 2019-12-20 16:20:44Z $
 */

#ifndef _wlc_pktfetch_h_
#define _wlc_pktfetch_h_

#if defined(BCMSPLITRX)
#include <wlc_types.h>
#include <rte_pktfetch.h>
#include <wlc_frmutil.h>
#include <bcmendian.h>
#ifdef WLNDOE
#include <bcmipv6.h>
#endif // endif
#include <wl_export.h>
#ifdef WL_TBOW
#include <wlc_tbow.h>
#endif // endif

typedef struct wlc_eapol_pktfetch_ctx {
	wlc_info_t *wlc;
	wlc_frminfo_t f;
	bool ampdu_path;
	bool ordered;
	struct pktfetch_info *pinfo;
	bool promisc;
	uint32 scb_assoctime;
	uint32 flags;
	struct ether_addr ea; /* Ethernet address of SCB */
} wlc_eapol_pktfetch_ctx_t;
/* this bit shifting is as per RcvHdrConvCtrlSts register */
#define ETHER_HDR_CONV_TYPE     (1<<9)
#define ETHER_TYPE_2_OFFSET 14 /* DA(6) + SA(6)+ PAD(2) */
#define ETHER_TYPE_1_OFFSET 22 /* PAD(2) + DA(6) + SA(6)+ LEN(2) + 6(aa aa 03 00 00 00) */
#define DOT3HDR_OFFSET  16 /* DA(6) + SA(6) + Type(2) */
#define LLC_HDR_LEN             8
#define PKTBODYOFFSZ            4
#define PKTFETCH_FLAG_AMSDU_SUBMSDUS  0x01 /* AMSDU non-first MSDU"S */
#define LLC_START_OFFSET_LEN	(DOT11_LLC_SNAP_HDR_LEN - ETHER_TYPE_LEN)

#ifdef BCMWAPI_WAI
#define	WAPI_PKTFETCH_REQUIRED(ether_type)	(ether_type == ETHER_TYPE_WAI)
#endif // endif

#define LLC_SNAP_HEADER_CHECK(lsh) \
			(lsh->dsap == 0xaa && \
			lsh->ssap == 0xaa && \
			lsh->ctl == 0x03 && \
			lsh->oui[0] == 0 && \
			lsh->oui[1] == 0 && \
			lsh->oui[2] == 0)

#define EAPOL_PKTFETCH_REQUIRED(ether_type)  (ether_type == ETHER_TYPE_802_1X)

#ifdef WLNDOE
#define ICMP6_MIN_BODYLEN	(DOT11_LLC_SNAP_HDR_LEN + sizeof(struct ipv6_hdr)) + \
				sizeof(((struct icmp6_hdr *)0)->icmp6_type)
#define ICMP6_NEXTHDR_OFFSET	(sizeof(struct dot11_llc_snap_header) + \
				OFFSETOF(struct ipv6_hdr, nexthdr))
#define ICMP6_TYPE_OFFSET	(sizeof(struct dot11_llc_snap_header) + \
				sizeof(struct ipv6_hdr) + \
				OFFSETOF(struct icmp6_hdr, icmp6_type))
#define ICMP6_MIN_BODYLEN_SPLIT_MODE4	(sizeof(struct ipv6_hdr) + \
		sizeof(((struct icmp6_hdr *)0)->icmp6_type))
#define ICMP6_NEXTHDR_OFFSET_SPLIT_MODE4	(OFFSETOF(struct ipv6_hdr, nexthdr))
#define ICMP6_TYPE_OFFSET_SPLIT_MODE4	(sizeof(struct ipv6_hdr) + \
		OFFSETOF(struct icmp6_hdr, icmp6_type))
#define NDOE_PKTFETCH_REQUIRED(wlc, lsh, pbody, body_len) \
	(NDOE_ENAB(wlc->pub) && (lsh->type == hton16(ETHER_TYPE_IPV6) && \
	body_len >= (((uint8 *)lsh - (uint8 *)pbody) + ICMP6_MIN_BODYLEN) && \
	*((uint8 *)lsh + ICMP6_NEXTHDR_OFFSET) == ICMPV6_HEADER_TYPE && \
	(*((uint8 *)lsh + ICMP6_TYPE_OFFSET) == ICMPV6_PKT_TYPE_NS || \
	*((uint8 *)lsh + ICMP6_TYPE_OFFSET) == ICMPV6_PKT_TYPE_NA || \
	*((uint8 *)lsh + ICMP6_TYPE_OFFSET) == ICMPV6_PKT_TYPE_RA)))
#define NDOE_PKTFETCH_REQUIRED_MODE4(wlc, pOffset, pbody, body_len) \
	(body_len >= (((uint8 *)pOffset - (uint8 *)pbody) + ICMP6_MIN_BODYLEN_SPLIT_MODE4) && \
	(*((uint8 *)pOffset + ICMP6_NEXTHDR_OFFSET_SPLIT_MODE4) == ICMPV6_HEADER_TYPE) && \
	((*((uint8 *)pOffset + ICMP6_TYPE_OFFSET_SPLIT_MODE4) == ICMPV6_PKT_TYPE_NS) || \
	(*((uint8 *)pOffset + ICMP6_TYPE_OFFSET_SPLIT_MODE4) == ICMPV6_PKT_TYPE_RA)))

#endif /* WLNDOE */

#ifdef WLTDLS
#define	WLTDLS_PKTFETCH_REQUIRED(wlc, ether_type) \
	(TDLS_ENAB(wlc->pub) && ether_type == ETHER_TYPE_89_0D)
#endif // endif

#ifdef ICMP
#define ICMP_PKTFETCH_REQUIRED(wlc, ether_type) \
	(ICMP_ENAB(wlc->pub) && wl_icmp_is_running(wl_get_icmp(wlc->wl, 0)) && \
	(ether_type == ETHER_TYPE_IP || ether_type == ETHER_TYPE_IPV6))
#endif /* ICMP */

#ifdef WL_TBOW
#define WLTBOW_PKTFETCH_REQUIRED(wlc, bsscfg, eth_type) \
	(TBOW_ENAB((wlc)->pub) && (bsscfg)->associated && \
	WLC_TBOW_ETHER_TYPE_MATCH(eth_type) && \
	BSSCFG_IS_TBOW_ACTIVE(bsscfg))
#endif // endif

extern int wlc_recvdata_schedule_pktfetch(wlc_info_t *wlc, struct scb *scb,
        wlc_frminfo_t *f, bool promisc_frame, bool ordered, bool amsdu_sub_msdus);
extern bool wlc_pktfetch_required(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct wlc_frminfo *f,
        wlc_key_info_t *key_info, bool skip_iv);

extern void wlc_pktfetch_queue_flush(wlc_info_t * wlc);
#if defined(PKTC) || defined(PKTC_DONGLE)
extern void wlc_sendup_schedule_pktfetch(wlc_info_t *wlc, void *pkt, uint32 body_offset);
extern bool wlc_sendup_chain_pktfetch_required(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *p,
		uint16 body_offset);
#endif // endif
#endif /* BCMSPLITRX */

#ifdef BCMPCIEDEV
extern void wl_tx_pktfetch(wlc_info_t *wlc, struct lbuf *lb, void *src,
	void *dev, wlc_bsscfg_t *bsscfg);
#endif /* BCMPCIEDEV */
#endif	/* _wlc_pktfetch_h_ */
