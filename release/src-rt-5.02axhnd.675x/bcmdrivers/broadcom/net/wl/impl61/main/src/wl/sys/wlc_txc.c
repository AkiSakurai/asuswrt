/**
 * @file
 * @brief
 * tx header caching module source - It caches the d11 header of
 * a packet and copy it to the next packet if possible.
 * This feature saves a significant amount of processing time to
 * build a packet's d11 header from scratch.
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
 * $Id: wlc_txc.c 774782 2019-05-07 12:44:04Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_txc.h>
#ifdef WLTOEHW
#include <wlc_tso.h>
#endif // endif
#include <wlc_hw.h>
#include <wlc_tx.h>
#include <wlc_dump.h>
#ifdef WLCFP
#include <d11_cfg.h>
#include <wlc_cfp.h>
#endif // endif
#include <wlc_he.h>

#ifdef BCMDBG
/* iovar table */
enum {
	IOV_TXC,
	IOV_TXC_POLICY,
	IOV_TXC_STICKY,
	IOV_LAST
};

static const bcm_iovar_t txc_iovars[] = {
	{"txc", IOV_TXC, (0), 0, IOVT_BOOL, 0},
	{"txc_policy", IOV_TXC_POLICY, (0), 0, IOVT_BOOL, 0},
	{"txc_sticky", IOV_TXC_STICKY, (0), 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};
#else /* !BCMDBG */
#define txc_iovars NULL
#endif /* !BCMDBG */

typedef struct scb_txc_info scb_txc_info_t;

/* module private states */
typedef struct {
	wlc_info_t *wlc;
	int scbh;
	/* states */
	bool policy;		/* 0:off 1:auto */
	bool sticky;		/* invalidate txc every second or not */
	uint gen;		/* tx header cache generation number */
} wlc_txc_info_priv_t;

/* module states layout */
typedef struct {
	wlc_txc_info_t pub;
	wlc_txc_info_priv_t priv;
} wlc_txc_t;

/* scb private states */
struct scb_txc_info {
	uint	txhlen;		/* #bytes txh[] valid, 0=invalid */
	uint	pktlen;		/* tag: original user packet length */
	uint	fifo;		/* fifo for the pkt */
	uint	flags;		/* pkt flags */
	uint8	prio;		/* pkt prio */
	uint8	ps_on;		/* the scb was in power save mode when cache was built */
	uint8	align;		/* alignment of hdrs start in txh array */
	uint16	offset;		/* offset of actual data from txh array start point */
	uint16	d11hdr_len;	/* lenght of d11 mac hdr */
	/* Don't change the order of next two variables, as txh need to start
	 * at a word aligned location
	 */
	uint	gen;		/* generation number (compare to priv->gen) */
	uint8	txh[TXOFF];	/* cached tx header */
#ifdef BCMDBG
	/* keep these debug stats at the end of the struct */
	uint	hit;
	uint	used;
	uint	added;
	uint	missed;
#endif // endif
};

#define SCB_TXC_CUBBY_LOC(txc, scb) ((scb_txc_info_t **) \
				     SCB_CUBBY(scb, txc->priv.scbh))
#define SCB_TXC_INFO(txc, scb) (*SCB_TXC_CUBBY_LOC(txc, scb))

/* d11txh position in the cached entries */
#define SCB_TXC_TXH(sti) ((d11txhdr_t *)((sti)->txh + (sti)->align + (sti)->offset))
#define SCB_TXC_TXH_NO_OFFST(sti) ((d11txhdr_t *)((sti)->txh + (sti)->align))

/* handy macros */
#ifdef BCMDBG
#define INCCNTHIT(sti) ((sti)->hit++)
#define INCCNTUSED(sti) ((sti)->used++)
#define INCCNTADDED(sti) ((sti)->added++)
#define INCCNTMISSED(sti) ((sti)->missed++)
#else
#define INCCNTHIT(sti)
#define INCCNTUSED(sti)
#define INCCNTADDED(sti)
#define INCCNTMISSED(sti)
#endif // endif

/* WLF_ flags treated as miss when toggling */
#define WLPKTF_TOGGLE_MASK	(WLF_EXEMPT_MASK | WLF_AMPDU_MPDU)
/* WLF_ flags to save in cache */
#define WLPKTF_STORE_MASK	(WLPKTF_TOGGLE_MASK | \
				 WLF_MIMO | WLF_VRATE_PROBE | WLF_RATE_AUTO | \
				 WLF_RIFS | WLF_WME_NOACK)

/* local function */
/* module entries */
#ifdef BCMDBG
static int wlc_txc_doiovar(void *ctx, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
#else
#define wlc_txc_doiovar NULL
#endif // endif
static void wlc_txc_watchdog(void *ctx);
#if defined(BCMDBG)
static int wlc_txc_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* scb cubby */
static int wlc_txc_scb_init(void *ctx, struct scb *scb);
static void wlc_txc_scb_deinit(void *ctx, struct scb *scb);
#if defined(BCMDBG)
static void wlc_txc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_txc_scb_dump NULL
#endif // endif

/* module entries */
wlc_txc_info_t *
BCMATTACHFN(wlc_txc_attach)(wlc_info_t *wlc)
{
	wlc_txc_t *txc;
	wlc_txc_info_t *txc_info;
	wlc_txc_info_priv_t *priv;

	txc_info = NULL;
	/* module states */
	if ((txc = MALLOCZ(wlc->osh, sizeof(*txc))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	txc_info = &txc->pub;
	txc_info->_txc = FALSE;

	priv = &txc->priv;
	priv->wlc = wlc;
	priv->policy = ON;
	priv->gen = 0;
	/* debug builds should not invalidate the txc per watchdog */
#ifdef BCMDBG
	priv->sticky = TRUE;
#else
	priv->sticky = FALSE;
#endif // endif

	/* reserve a cubby in the scb */
	if ((priv->scbh =
	     wlc_scb_cubby_reserve(wlc, sizeof(scb_txc_info_t *),
	                           wlc_txc_scb_init, wlc_txc_scb_deinit,
	                           wlc_txc_scb_dump, txc)) < 0) {
		WL_ERROR(("wl%d: %s: cubby register for txc failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, txc_iovars, "txc", txc, wlc_txc_doiovar,
	                        wlc_txc_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "txc", wlc_txc_dump, txc);
#endif // endif

	return txc_info;

fail:
	MODULE_DETACH(txc_info, wlc_txc_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_txc_detach)(wlc_txc_info_t *txc_info)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_info_t *wlc;

	if (txc == NULL)
		return;

	wlc = txc->priv.wlc;

	wlc_module_unregister(wlc->pub, "txc", txc);

	MFREE(wlc->osh, txc, sizeof(*txc));
}

#ifdef BCMDBG
/* handle related iovars */
static int
wlc_txc_doiovar(void *ctx, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	wlc_txc_info_priv_t *priv = &txc->priv;
	int32 *ret_int_ptr;
	bool bool_val;
	int32 int_val = 0;
	int err = BCME_OK;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(alen);
	BCM_REFERENCE(wlcif);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* all iovars require mcnx being enabled */
	switch (actionid) {
	case IOV_GVAL(IOV_TXC):
		*ret_int_ptr = (int32)txc->pub._txc;
		break;

	case IOV_GVAL(IOV_TXC_POLICY):
		*ret_int_ptr = (int32)priv->policy;
		break;

	case IOV_SVAL(IOV_TXC_POLICY):
		priv->policy = bool_val;
		if (!WLC_TXC_ENAB(priv->wlc))
			break;
		wlc_txc_upd(&txc->pub);
		break;

	case IOV_GVAL(IOV_TXC_STICKY):
		*ret_int_ptr = (int32)priv->sticky;
		break;

	case IOV_SVAL(IOV_TXC_STICKY):
		priv->sticky = bool_val;
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	if (!IOV_ISSET(actionid))
		return err;

	return err;
}
#endif /* BCMDBG */

static void
wlc_txc_watchdog(void *ctx)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	wlc_txc_info_priv_t *priv = &txc->priv;
#if defined(BCMDBG)
	bool old;

	/* check for missing call to wlc_txc_upd() */
	old = txc->pub._txc;
	if (WLC_TXC_ENAB(priv->wlc))
		wlc_txc_upd(&txc->pub);
	ASSERT(txc->pub._txc == old);
#endif /* BCMDBG */

	/* invalidate tx header cache once per second if not sticky */
	if (!priv->sticky)
		priv->gen++;
}

#if defined(BCMDBG)
static int
wlc_txc_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	wlc_txc_info_priv_t *priv = &txc->priv;

	bcm_bprintf(b, "enab %d txc %d policy %d sticky %d gen %d\n",
	            WLC_TXC_ENAB(priv->wlc),
	            txc->pub._txc, priv->policy, priv->sticky, priv->gen);

	return BCME_OK;
}
#endif // endif

void wlc_txc_prep_sdu(wlc_txc_info_t *txc_info, struct scb *scb, wlc_key_t *key,
	const wlc_key_info_t *key_info, void *p)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_txc_info_priv_t *priv;
	scb_txc_info_t *sti;
	wlc_info_t *wlc;
	d11txhdr_t *txd;
	int d11_off, d11TxdLen;
	BCM_REFERENCE(key_info);

	ASSERT(txc != NULL && scb != NULL && key != NULL);

	priv = &txc->priv;
	sti = SCB_TXC_INFO(txc, scb);

	wlc = priv->wlc;

	/* prepare pkt for tx - this updates the txh */
	WLPKTTAGSCBSET(p, scb);

	d11TxdLen = D11_TXH_LEN_EX(wlc);
	d11_off = sti->offset + d11TxdLen;
	txd = (d11txhdr_t *)((uint8 *)PKTDATA(wlc->osh, p) + sti->offset);
	BCM_REFERENCE(txd);
	PKTPULL(wlc->osh, p, d11_off);
	(void)wlc_key_prep_tx_mpdu(key, p, txd);

	PKTPUSH(wlc->osh, p, d11_off);
}

/* retrieve tx header from the cache and copy it to the packet.
 * return d11txh pointer in the packet where the copy started.
 */
d11txhdr_t * BCMFASTPATH
wlc_txc_cp(wlc_txc_info_t *txc_info, struct scb *scb, void *pkt, uint *flags)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_info_t *wlc = txc->priv.wlc;
	scb_txc_info_t *sti;
	d11txhdr_t *txh;
	uint txhlen;
	uint8 *cp;
	uint32 htc_code;
	struct dot11_header *h;
	uint32 *phtc;
	uint8 *data;
	uint16 d11hdr_len;
	uint16 fc;
	uint16 offset;

	ASSERT(txc->pub._txc == TRUE);

	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	ASSERT(sti != NULL);

	txhlen = sti->txhlen;

	/* Possibly append HTC+ if needed */
	htc_code = 0;
	if (SCB_QOS(scb)) {
		if (wlc_he_htc_tx(wlc, scb, pkt, &htc_code)) {
			txhlen += DOT11_HTC_LEN;
		}
	}

	/* basic sanity check the tx cache */
	ASSERT(txhlen >= (D11_TXH_SHORT_LEN(wlc) + DOT11_A3_HDR_LEN) && txhlen < TXOFF);

	txh = SCB_TXC_TXH_NO_OFFST(sti);
	cp = PKTPUSH(wlc->osh, pkt, txhlen);
	bcopy((uint8 *)txh, cp, txhlen);

	txh = (d11txhdr_t *)(cp + sti->offset);
	if (htc_code) {
		d11hdr_len = sti->d11hdr_len;
		h = (struct dot11_header *)((uint8 *)cp + txhlen - d11hdr_len - DOT11_HTC_LEN);
		fc = ltoh16(h->fc);
		/* Update ORDER bit in hdr */
		h->fc |= htol16(FC_ORDER);
		/* create rooom to insert htc+, move reset of hdr */
		offset = DOT11_A3_HDR_LEN + DOT11_QOS_LEN;
		if ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS)) {
			offset += ETHER_ADDR_LEN;
		}
		data = (uint8 *)h;
		while (offset < d11hdr_len) {
			d11hdr_len--;
			data[d11hdr_len + DOT11_HTC_LEN] = data[d11hdr_len];
		}

		phtc = (uint32 *)(((uint8 *)h) + offset);
		*phtc = htol32(htc_code);

		/* For now only rev128 and higher will come here (!!AX supported chips). So
		 * use this knowledge to address the IVOffset update directly.
		 */
		txh->rev128.IVOffset_Lifetime_lo += DOT11_HTC_LEN;
		ASSERT(txh->rev128.IVOffset_Lifetime_lo < (1 << D11_REV128_LIFETIME_LO_SHIFT));
	}

	/* return the saved pkttag flags */
	*flags = sti->flags;
	INCCNTUSED(sti);

	return txh;
}

/* check if we can find anything in the cache */
bool BCMFASTPATH
wlc_txc_hit(wlc_txc_info_t *txc_info, struct scb *scb, void *sdu, uint pktlen, uint fifo,
	uint8 prio)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_txc_info_priv_t *priv = &txc->priv;
	wlc_info_t *wlc = priv->wlc;
	wlc_bsscfg_t *cfg;
	scb_txc_info_t *sti;
	uint miss;

	ASSERT(txc->pub._txc == TRUE);

	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	if (sti == NULL)
		return FALSE;

	cfg = SCB_BSSCFG(scb);
	BCM_REFERENCE(cfg);
	ASSERT(cfg != NULL);

	miss = (D11REV_LT(wlc->pub->corerev, 40) && (pktlen != sti->pktlen));
	if (miss)
		return FALSE;
	miss |= (sti->txhlen == 0);
	miss |= (fifo != sti->fifo);
	miss |= (prio != sti->prio);
	miss |= (sti->gen != priv->gen);
	miss |= ((sti->flags & WLPKTF_TOGGLE_MASK) != (WLPKTTAG(sdu)->flags & WLPKTF_TOGGLE_MASK));
	miss |= ((pktlen >= wlc->fragthresh[WME_PRIO2AC(prio)]) &&
		((WLPKTTAG(sdu)->flags & WLF_AMSDU) == 0));
	if (BSSCFG_AP(cfg) || BSS_TDLS_ENAB(wlc, cfg)) {
		miss |= (sti->ps_on != SCB_PS(scb));
	}
#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	if (miss && WL_PRPKT_ON()) {
		printf("txc missed: scb %p gen %d txcgen %d hdr_len %d body len %d, pkt_len %d\n",
		        OSL_OBFUSCATE_BUF(scb), sti->gen, priv->gen,
				sti->txhlen, sti->pktlen, pktlen);
	}
#endif // endif

	if (miss == 0) {
		INCCNTHIT(sti);
		return TRUE;
	}

	INCCNTMISSED(sti);
	return FALSE;
}

/* install tx header from the packet into the cache */
void BCMFASTPATH
wlc_txc_add(wlc_txc_info_t *txc_info, struct scb *scb, void *pkt, uint txhlen, uint fifo,
	uint8 prio, uint16 txh_off, uint d11hdr_len)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_txc_info_priv_t *priv = &txc->priv;
	wlc_info_t *wlc = priv->wlc;
	wlc_bsscfg_t *cfg;
	scb_txc_info_t *sti;
	d11txhdr_t *txh;
	osl_t *osh;
	uint8 ethHdrLen = ETHER_HDR_LEN;
	struct dot11_header *h;
	uint16 fc;
	uint32 offset;
	uint8 *data;

	BCM_REFERENCE(d11hdr_len);

	ASSERT(txc->pub._txc == TRUE);
	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;

	cfg = SCB_BSSCFG(scb);
	BCM_REFERENCE(cfg);
	ASSERT(cfg != NULL);

	osh = wlc->osh;

	/* cache any pkt with same DA-SA-L(before LLC/SNAP), hdr must be in one buffer */

	sti->align = (uint8)((uintptr)PKTDATA(osh, pkt) & 3);
	ASSERT((sti->align == 0) || (sti->align == 2));
	sti->offset = txh_off;
	sti->txhlen = txhlen;
	bcopy(PKTDATA(osh, pkt), sti->txh + sti->align, txhlen);
	/* With 11ax a dynamic field HTC+ got introduced. This field should not be
	 * cached, nor should the mac hdr default identify it to be included. So strip
	 * it away here if the machdr holds this field
	 */
	h = (struct dot11_header *)((sti->txh + sti->align) + txhlen - d11hdr_len);
	fc = ltoh16(h->fc);
	sti->d11hdr_len = d11hdr_len;
	if ((fc & FC_ORDER) && FC_SUBTYPE_ANY_QOS(FC_SUBTYPE(fc))) {
		/* Strip away HTC+ */
		h->fc = htol16(fc & ~FC_ORDER);
		/* move the data from after HTC to begin HTC (overwriting HTC) */
		offset = DOT11_A3_HDR_LEN + DOT11_QOS_LEN;
		if ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS)) {
			offset += ETHER_ADDR_LEN;
		}
		data = (uint8 *)h;
		sti->d11hdr_len -= DOT11_HTC_LEN;
		while (offset < sti->d11hdr_len) {
			data[offset] = data[offset + DOT11_HTC_LEN];
			offset++;
		}
		sti->txhlen -= DOT11_HTC_LEN;
		txh = SCB_TXC_TXH(sti);
		/* For now only rev128 and higher will come here (11AX supported chips). So
		 * use this knowledge to address the IVOffset update directly.
		 */
		txh->rev128.IVOffset_Lifetime_lo -= DOT11_HTC_LEN;
		ASSERT(txh->rev128.IVOffset_Lifetime_lo < (1 << D11_REV128_LIFETIME_LO_SHIFT));
	}

	txh = SCB_TXC_TXH(sti);
	sti->pktlen = pkttotlen(osh, pkt) - txhlen + ethHdrLen;
	sti->fifo = fifo;
	sti->prio = prio;
	sti->gen = priv->gen;

	if (BSSCFG_AP(cfg) || BSS_TDLS_ENAB(wlc, cfg)) {
		sti->ps_on = SCB_PS(scb);
	}
	/* need TXHDR flag to avoid going back to prep_sdu */
	sti->flags = WLF_TXHDR;
	sti->flags |= (WLPKTTAG(pkt)->flags & WLPKTF_STORE_MASK);

#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	if (WL_PRPKT_ON()) {
		printf("install txc: scb %p gen %d hdr_len %d, body len %d\n",
		       OSL_OBFUSCATE_BUF(scb), sti->gen, sti->txhlen, sti->pktlen);
	}
#endif // endif

	INCCNTADDED(sti);
#ifdef WLCFP
	/* Enable CFP Txcache state only for AMPDU capable rates
	 * CFP bypass assumes its AMPDU path and always sets up txheaders
	 * for AMPDU transmissions
	 */
	if ((CFP_ENAB(wlc->pub) == TRUE) && WLPKTTAG(pkt)->flags & WLF_MIMO) {
		wlc_cfp_tcb_upd_cache_state(wlc, scb, TRUE);
	}
#endif // endif
}

/* update tx header cache enable flag (txc->_txc) */
void
wlc_txc_upd(wlc_txc_info_t *txc_info)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_txc_info_priv_t *priv = &txc->priv;
	wlc_info_t *wlc = priv->wlc;

	BCM_REFERENCE(wlc);

	txc->pub._txc = (priv->policy == ON) ? TRUE : FALSE;
}

/* scb cubby */
static int
wlc_txc_scb_init(void *ctx, struct scb *scb)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	wlc_txc_info_priv_t *priv = &txc->priv;
	wlc_info_t *wlc = priv->wlc;
	scb_txc_info_t **psti = SCB_TXC_CUBBY_LOC(txc, scb);
	scb_txc_info_t *sti;

	if (SCB_INTERNAL(scb) || !WLC_TXC_ENAB(wlc)) {
		WL_INFORM(("%s: Not allocating the cubby, txc enab %d\n",
		           __FUNCTION__, WLC_TXC_ENAB(wlc)));
		return BCME_OK;
	}

	if ((sti = MALLOCZ(wlc->osh, sizeof(scb_txc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate cubby space for txheader cache\n",
		          wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}
	*psti = sti;

	return BCME_OK;
}

static void
wlc_txc_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	scb_txc_info_t **psti = SCB_TXC_CUBBY_LOC(txc, scb);
	scb_txc_info_t *sti = *psti;

	if (sti == NULL)
		return;

	MFREE(txc->priv.wlc->osh, sti, sizeof(*sti));
	*psti = NULL;
}

#if defined(BCMDBG)
static void
wlc_txc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_txc_t *txc = (wlc_txc_t *)ctx;
	scb_txc_info_t *sti;

	if (!WL_ERROR_ON())
		return;

	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;

	bcm_bprintf(b, "     txc %d\n", txc->pub._txc);
	bcm_bprintf(b, "\toffset %u length %u\n", sti->offset, sti->txhlen);
	bcm_bprhex(b, "\ttxh ", TRUE, sti->txh, TXOFF);
	bcm_bprintf(b, "\tpktlen %u gen %u flags 0x%x fifo %u prio %u ps %u\n",
	            sti->pktlen, sti->gen, sti->flags, sti->fifo, sti->prio, sti->ps_on);
#ifdef BCMDBG
	bcm_bprintf(b, "\thit %u used %u added %u missed %u\n",
	            sti->hit, sti->used, sti->added, sti->missed);
#endif // endif
}
#endif // endif

/* invalidate the cache entry */
void
wlc_txc_inv(wlc_txc_info_t *txc_info, struct scb *scb)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	scb_txc_info_t *sti;
	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;
#ifdef WLCFP
	if (CFP_ENAB(txc->priv.wlc->pub) == TRUE)
	{
		/* Invalidate TCB state for given scb */
		wlc_cfp_tcb_upd_cache_state(txc->priv.wlc, scb, FALSE);
	}
#endif // endif
	sti->txhlen = 0;
}

/* invalidate all cache entries */
void
wlc_txc_inv_all(wlc_txc_info_t *txc_info)
{
	wlc_txc_t *txc = (wlc_txc_t *)txc_info;
	wlc_txc_info_priv_t *priv = &txc->priv;

	priv->gen++;

#ifdef WLCFP
	if (CFP_ENAB(priv->wlc->pub) == TRUE) {
		/* Invalidate all cubbies in CFP module too */
		/* Update the state */
		wlc_cfp_tcb_cache_invalidate(priv->wlc, CFP_ALL_FLOWS);
	}
#endif // endif
}
