/*
 * Michael Messge Integrity Check (MIC) algorithm and other security
 * interface definitions
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
 * $Id: wlc_security.h,v 1.2 2009-10-07 21:14:02 $
 */

#ifndef _wlc_security_h_
#define _wlc_security_h_

#ifndef LINUX_CRYPTO
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/tkhash.h>
#endif // endif

#include <bcmcrypto/bcmccx.h>

typedef struct wsec_tkip_info {
	bool	TKIP;		/* enabled/disabled flag */
#ifdef NOTYET
	uint8	mic_key[8];	/* key for MIC */
#else
	uint32	mic_key0;	/* key for MIC */
	uint32	mic_key1;	/* key for MIC */
#endif /* NOTYET */
} wsec_tkip_info_t;

struct wlc_frminfo;

/* Check if the received frame passes security filter */
#ifdef WLWSEC
extern bool BCMROMFN(wlc_wsec_recvdata)(wlc_info_t *wlc, osl_t *osh, struct scb *scb,
	struct wlc_frminfo *frminfo, uint8 prio);

#ifndef LINUX_CRYPTO
extern bool BCMROMFN(wlc_wsec_sw_encrypt_data)(wlc_info_t *wlc, osl_t *osh, void *p,
	wlc_bsscfg_t *cfg, struct scb *scb, wsec_key_t *key);
#endif // endif

extern void BCMROMFN(wlc_wsec_rxiv_update)(wlc_info_t *wlc, struct wlc_frminfo *f);

#ifdef BCMWAPI_WPI
void wlc_wsec_wapi_rxiv_update(wlc_info_t *wlc, struct wlc_frminfo *f);
#endif /* BCMWAPI_WPI */

extern bool wlc_wsec_miccheck(wlc_info_t *wlc, osl_t *osh, struct scb *scb,
	struct wlc_frminfo *f);

#ifndef LINUX_CRYPTO
/* cobble TKIP MIC for an out-bound frag */
extern void BCMROMFN(wlc_dofrag_tkip)(wlc_pub_t *wlp, void *p, uint frag, uint nfrags, osl_t *osh,
	struct wlc_bsscfg *cfg, struct scb *scb, struct ether_header *eh,
	wsec_key_t *key, uint8 prio, uint frag_length);
#endif // endif
/* compute length of tkip fragment
 * flen_hdr is frag_length + ETHER_HDR_LEN
 */
extern uint BCMROMFN(wlc_wsec_tkip_nextfrag_len)(wlc_pub_t *wlp, osl_t *osh,
	void *pkt_curr, void *pkt_next, uint flen_hdr);

extern int BCMROMFN(wlc_wsec_tx_tkmic_offset)(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb);
extern int BCMROMFN(wlc_wsec_rx_tkmic_offset)(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb);

#else
	#define wlc_wsec_recvdata(a, b, c, d, e) (FALSE)
	#define wlc_wsec_sw_encrypt_data(a, b, c, d, e, f) (FALSE)
	#define wlc_wsec_rxiv_update(a, b) do {} while (0)
	#define wlc_wsec_miccheck(a, b, c, d) (FALSE)
	#define wlc_dofrag_tkip(a, b, c, d, e, f, g, h, i, j, k) do {} while (0)
	#define wlc_wsec_tkip_nextfrag_len(a, b, c, d, e) (0)
	#define wlc_wsec_tx_tkmic_offset(a, b, c) (0)
	#define wlc_wsec_rx_tkmic_offset(a, b, c) (0)
#ifdef BCMWAPI_WPI
	#define wlc_wsec_wapi_rxiv_update(a, b)  do {} while (0)
#endif /* BCMWAPI_WPI */
#endif   /* WL_WSEC */

/* 802.1X LLC header */
extern const uint8 BCMROMDATA(wlc_802_1x_hdr)[];

#ifdef BCMWAPI_WPI
extern const uint8 wlc_wapi_wai_hdr[];
#endif // endif

#ifdef BCMCCX
#define CKIP_LLC_SNAP_LEN	8 /* SKIP LLC SNAP header length */
#endif /* BCMCCX */

#ifdef WSEC_TEST
void wlc_wsec_tkip_runst(uint32 k0, uint32 k1, uint8 *mref, uint32 cref0, uint32 cref1);
int wlc_wsec_tkip_runtest_mic(void);
int wlc_wsec_tkip_runtest(void);
#endif /* WSEC_TEST */

#endif /* _wlc_security_h_ */
