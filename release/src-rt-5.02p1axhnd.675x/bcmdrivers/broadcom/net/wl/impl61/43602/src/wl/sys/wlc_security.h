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
 * $Id: wlc_security.h 508336 2014-10-15 12:06:18Z $
 */

#ifndef _wlc_security_h_
#define _wlc_security_h_

#ifndef LINUX_CRYPTO
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/tkhash.h>
#endif // endif

#ifdef BCMCCX
#include <bcmcrypto/bcmccx.h>
#endif // endif

struct wlc_frminfo;

#ifdef WLWSEC
extern void
wlc_decrypt_error(wlc_info_t *wlc, struct ether_addr *addr, int bsscfg_idx,
	uint32 event_type, bool group, wsec_key_t *key, bool flush_txq);

/* Check if the received frame passes security filter */
extern bool wlc_wsec_recvdata(wlc_info_t *wlc, osl_t *osh, struct scb *scb,
	struct wlc_frminfo *frminfo, uint8 prio);

#ifndef LINUX_CRYPTO
extern bool wlc_wsec_sw_encrypt_data(wlc_info_t *wlc, osl_t *osh, void *p,
	wlc_bsscfg_t *cfg, struct scb *scb, wsec_key_t *key);
#endif // endif

extern void wlc_wsec_rxiv_update(wlc_info_t *wlc, struct wlc_frminfo *f);

extern bool wlc_wsec_miccheck(wlc_info_t *wlc, osl_t *osh, struct scb *scb,
	struct wlc_frminfo *f);

#ifndef LINUX_CRYPTO
/* cobble TKIP MIC for an out-bound frag */
extern void wlc_dofrag_tkip(wlc_pub_t *wlp, void *p, uint frag, uint nfrags, osl_t *osh,
	struct wlc_bsscfg *cfg, struct scb *scb, struct ether_header *eh,
	wsec_key_t *key, uint8 prio, uint frag_length);
#endif // endif
/* compute length of tkip fragment
 * flen_hdr is frag_length + ETHER_HDR_LEN
 */
extern uint wlc_wsec_tkip_nextfrag_len(wlc_pub_t *wlp, osl_t *osh,
	void *pkt_curr, void *pkt_next, uint flen_hdr);

extern int wlc_wsec_tx_tkmic_offset(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb);
extern int wlc_wsec_rx_tkmic_offset(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb);

/* 802.1X LLC header */
extern const uint8 wlc_802_1x_hdr[];

#ifdef BCMCCX
#define CKIP_LLC_SNAP_LEN	8 /* SKIP LLC SNAP header length */
#endif /* BCMCCX */

#ifdef WSEC_TEST
void wlc_wsec_tkip_runst(uint32 k0, uint32 k1, uint8 *mref, uint32 cref0, uint32 cref1);
int wlc_wsec_tkip_runtest_mic(void);
int wlc_wsec_tkip_runtest(void);
#endif /* WSEC_TEST */
#else /* WLWSEC */
	#define wlc_wsec_recvdata(a, b, c, d, e) (FALSE)
	#define wlc_wsec_sw_encrypt_data(a, b, c, d, e, f) (FALSE)
	#define wlc_wsec_rxiv_update(a, b) do {} while (0)
	#define wlc_wsec_miccheck(a, b, c, d) (FALSE)
	#define wlc_dofrag_tkip(a, b, c, d, e, f, g, h, i, j, k) do {} while (0)
	#define wlc_wsec_tkip_nextfrag_len(a, b, c, d, e) (0)
	#define wlc_wsec_tx_tkmic_offset(a, b, c) (0)
	#define wlc_wsec_rx_tkmic_offset(a, b, c) (0)
	#define wlc_decrypt_error(a, b, c, d, e, f, g) do {} while (0)

	static const uint8 wlc_802_1x_hdr[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};
#endif /* WLWSEC */

#ifdef WLFIPS
extern int wl_fips_encrypt_pkt(wl_fips_info_t *fi, uint8 key_index,
	const struct dot11_header *h, uint len, uint8 nonce_1st_byte);
extern int wl_fips_decrypt_pkt(wl_fips_info_t *fi, uint8 key_index,
	const struct dot11_header *h, uint len, uint8 nonce_1st_byte);
#endif /* WLFIPS */

#endif /* _wlc_security_h_ */
