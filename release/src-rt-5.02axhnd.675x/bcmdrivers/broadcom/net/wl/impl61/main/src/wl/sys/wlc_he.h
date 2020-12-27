/*
 * 802.11ax HE (High Efficiency) STA signaling and d11 h/w manipulation.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_he.h 777844 2019-08-13 07:36:57Z $
 */

#ifndef _wlc_he_h_
#define _wlc_he_h_

#include <wlc_types.h>
#include <wlc_twt.h>

#include <802.11ax.h>
#include <phy_ac_hecap.h>

#define HE_FC_IS_HTC(fc)	(FC_SUBTYPE_ANY_QOS(FC_SUBTYPE(fc)) && (fc & FC_ORDER))

/* attach/detach */
wlc_he_info_t *wlc_he_attach(wlc_info_t *wlc);
void wlc_he_detach(wlc_he_info_t *hei);

int wlc_he_init_defaults(wlc_he_info_t *hei);

#ifdef WL11AX

/* scb HE flags */
#define SCB_HE_LDPCCAP			0x0001
#define SCB_HE_TX_STBCCAP		0x0002
#define SCB_HE_RX_STBCCAP		0x0004
#define SCB_HE_HTC_CAP			0x0008
#define SCB_HE_SU_BFR			0x0010
#define SCB_HE_SU_MU_BFE		0x0020	/* in HE, MUBFE must be supported if SUBFE is 1 */
#define SCB_HE_MU_BFR			0x0040
#define SCB_HE_OMI			0x0080
#define SCB_HE_OMI_UL_MU_DATA_DIS	0x0100
#define SCB_HE_DL_QAM1024_LT242		0x0200 /* DL supports 1024 QAM using < 242 tone RU */
#define SCB_HE_UL_QAM1024_LT242		0x0400 /* UL supports 1024 QAM using < 242 tone RU */
#define SCB_HE_5G_20MHZ_ONLY		0x0800
#define SCB_HE_BSR_CAPABLE		0x1000 /* Supports BSR */
#define SCB_HE_DEVICE_CLASS		0x2000 /* 1: Class A; 0: Class B */
#define SCB_HE_DL_242TONE		0x4000 /* 242 Tone RU */

#define SCB_HE_LDPC_CAP(v, a)		(SCB_HE_CAP(a) && \
	(wlc_he_get_peer_caps(v, a) & SCB_HE_LDPCCAP))
#define SCB_HE_TX_STBC_CAP(v, a)	(SCB_HE_CAP(a) && \
	(wlc_he_get_peer_caps(v, a) & SCB_HE_TX_STBCCAP))
#define SCB_HE_RX_STBC_CAP(v, a)	(SCB_HE_CAP(a) && \
	(wlc_he_get_peer_caps(v, a) & SCB_HE_RX_STBCCAP))

#define WLC_HE_SU_BFR_CAP_PHY(w) \
		((wlc_phy_ac_hecaps1((phy_info_t *)WLC_PI((w))) & PHY_CAP_HE_SU_BFR) != 0)
#define WLC_HE_SU_MU_BFR_CAP_PHY(w) \
		((wlc_phy_ac_hecaps1((phy_info_t *)WLC_PI((w))) & PHY_CAP_HE_SU_MU_BFR) != 0)
#define WLC_HE_SU_BFE_CAP_PHY(w) \
		((wlc_phy_ac_hecaps1((phy_info_t *)WLC_PI((w))) & PHY_CAP_HE_SU_BFE) != 0)
#define WLC_HE_SU_MU_BFE_CAP_PHY(w) \
		((wlc_phy_ac_hecaps1((phy_info_t *)WLC_PI((w))) & PHY_CAP_HE_SU_MU_BFE) != 0)

typedef void (*he_htc_cb_fn_t)(wlc_info_t *wlc, struct scb *scb, uint32 code);

/* update scb */
void wlc_he_set_ldpc_cap(wlc_he_info_t *hei, bool enab);
/* tbtt interrupt handler for AP mode */
void wlc_he_ap_tbtt(wlc_he_info_t *hei);
/* parse a received frame to see if the mac hdr holds a 11ax HTC+ code */
void wlc_he_htc_recv(wlc_info_t* wlc, scb_t *scb, d11rxhdr_t *rxh, struct dot11_header *hdr);
/* if there is a HTC+ code to transmit return true; htc will never be 0 in that case */
bool wlc_he_htc_tx(wlc_info_t* wlc, scb_t *scb, void *pkt, uint32 *htc);
/* Add a htc code to queue of htc codes to be transmitted. */
void wlc_he_htc_send_code(wlc_info_t *wlc, scb_t *scb, uint32 htc_code, he_htc_cb_fn_t cb_fn);
/* Report the received PMQ OMI code on address ea */
bool wlc_he_omi_pmq_code(wlc_info_t *wlc, scb_t *scb, uint8 rx_nss, uint8 bw);

#else /* !WL11AX */

#define SCB_HE_LDPC_CAP(v, a)			FALSE
#define SCB_HE_TX_STBC_CAP(v, a)		FALSE
#define SCB_HE_RX_STBC_CAP(v, a)		FALSE

#define wlc_he_set_ldpc_cap(x, y)

#define WLC_HE_SU_BFR_CAP_PHY(w)		FALSE
#define WLC_HE_SU_MU_BFR_CAP_PHY(w)		FALSE
#define WLC_HE_SU_BFE_CAP_PHY(w)		FALSE
#define WLC_HE_SU_MU_BFE_CAP_PHY(w)		FALSE

#define wlc_he_ap_tbtt(a)

#define wlc_he_htc_recv(a, b, c, d)
#define wlc_he_htc_tx(a, b, c, d)		0
#define wlc_he_htc_send_code(a, b, c, d)

#define wlc_he_omi_pmq_code(a, b, c, d)		FALSE

/* update scb */
#endif /* WL11AX */

#define HE_FORMAT_MASK				0x0018	/* HE FORMAT is stored in phyctl  */
#define HE_FORMAT_SHIFT				3
#define HE_FORMAT_MAX				3
#define HE_FORMAT_SU				0
#define HE_FORMAT_RANGE_EXT			1
#define HE_FORMAT_MU				2
#define HE_FORMAT_TRIGGER			3

#define RSPEC_HE_FORMAT(r)			(HE_FORMAT_MASK & ((r) << HE_FORMAT_SHIFT))

#define HE_106_TONE_RANGE_EXT			1
#define HE_242_TONE_RANGE_EXT			2

/* update scb */
void wlc_he_update_scb_state(wlc_he_info_t *hei, int bandtype, struct scb *scb,
	he_cap_ie_t *capie, he_op_ie_t *opie);
uint16 wlc_he_get_peer_caps(wlc_he_info_t *hei, struct scb *scb);
bool wlc_he_partial_bss_color_get(wlc_he_info_t *hei, wlc_bsscfg_t *cfg, uint8 *color);
uint8 wlc_get_heformat(wlc_info_t *wlc);
uint8 wlc_get_hebsscolor(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/** Compute and fill the link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry. */
void wlc_he_fill_link_entry(wlc_he_info_t *hei, wlc_bsscfg_t *cfg, struct scb *scb,
	d11linkmem_entry_t* link_entry);

void wlc_he_update_mcs_cap(wlc_he_info_t *hei);
void wlc_he_set_rateset_filter(wlc_he_info_t *hei, wlc_rateset_t *rateset);
void wlc_he_default_rateset(wlc_he_info_t *hei, wlc_rateset_t *rateset);

extern uint8 wlc_he_get_bfe_ndp_recvstreams(wlc_he_info_t *hei);
extern uint8 wlc_he_get_bfr_ndp_sts(wlc_he_info_t *hei, bool is_bw160);
extern uint8 wlc_he_scb_get_bfr_nr(wlc_he_info_t *hei, scb_t *scb);
extern uint8 wlc_he_get_omi_tx_nsts(wlc_he_info_t *hei, scb_t *scb);
extern uint16 wlc_he_get_scb_flags(wlc_he_info_t *hei, struct scb *scb);
extern bool wlc_he_get_ulmu_allow(wlc_he_info_t *hei, struct scb *scb);

void wlc_he_recv_af_oper_mode(wlc_info_t* wlc, scb_t *scb, uint8 oper_mode);
void wlc_he_upd_scb_rateset_mcs(wlc_he_info_t *hei, struct scb *scb, uint8 link_bw);

#endif /* _wlc_he_h_ */
