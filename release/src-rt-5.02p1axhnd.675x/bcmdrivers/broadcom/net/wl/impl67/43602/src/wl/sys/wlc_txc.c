/**
 * @file
 * @brief
 * tx header caching module source - It caches the d11 header of
 * a packet and copy it to the next packet if possible.
 * This feature saves a significant amount of processing time to
 * build a packet's d11 header from scratch.
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
 * $Id$
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
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#ifdef BCM_HOST_MEM_SCB
#include <wlc_scb_alloc.h>
#endif // endif

#include <wlc_txc.h>

#ifdef BCMDBG
/* iovar table */
enum {
	IOV_TXC,
	IOV_TXC_POLICY,
	IOV_TXC_STICKY,
	IOV_LAST
};

static const bcm_iovar_t txc_iovars[] = {
	{"txc", IOV_TXC, (0), IOVT_BOOL, 0},
	{"txc_policy", IOV_TXC_POLICY, (0), IOVT_BOOL, 0},
	{"txc_sticky", IOV_TXC_STICKY, (0), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};
#else /* !BCMDBG */
#define txc_iovars NULL
#endif /* !BCMDBG */

/* module private states */
typedef struct {
	wlc_info_t *wlc;
	int scbh;
	/* states */
	bool policy;		/* 0:off 1:auto */
	bool sticky;		/* invalidate txc every second or not */
	uint gen;		/* tx header cache generation number */
} wlc_txc_info_priv_t;

/* wlc_txc_info_priv_t offset in module states */
static uint16 wlc_txc_info_priv_offset = sizeof(wlc_txc_info_t);

/* module states layout */
typedef struct {
	wlc_txc_info_t pub;
	wlc_txc_info_priv_t priv;
} wlc_txc_t;
/* module states size */
#define WLC_TXC_INFO_SIZE	(sizeof(wlc_txc_t))
/* moudle states location */
#define WLC_TXC_INFO_PRIV(txc) ((wlc_txc_info_priv_t *) \
	((uintptr)(txc) + wlc_txc_info_priv_offset))

/* scb private states */
typedef struct scb_txc_info {
	uint	txhlen;		/* #bytes txh[] valid, 0=invalid */
	uint	pktlen;		/* tag: original user packet length */
	uint	fifo;           /* fifo for the pkt */
	uint	flags;		/* pkt flags */
	uint8	prio;           /* pkt prio */
	uint8	ps_on;		/* the scb was in power save mode when cache was built */
	uint8	align;		/* alignment of hdrs start in txh array */
	uint16   offset;	/* offset of actual data from txh array start point */
	/* Don't change the order of next two variables, as txh need to start
	 * at a word aligned location
	 */
	uint8 ucache_idx;   /* cache index used by uCode. */
	uint	gen;		/* generation number (compare to priv->gen) */
	uint8	txh[TXOFF];	/* cached tx header */
#ifdef BCMDBG
	/* keep these debug stats at the end of the struct */
	uint hit;
	uint used;
	uint added;
#endif // endif
} scb_txc_info_t;

#define SCB_TXC_CUBBY_LOC(txc, scb) ((scb_txc_info_t **) \
				     SCB_CUBBY(scb, WLC_TXC_INFO_PRIV(txc)->scbh))
#define SCB_TXC_INFO(txc, scb) (*SCB_TXC_CUBBY_LOC(txc, scb))

#if defined(BCM_HOST_MEM_TXCTAG)

#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
#error "TXCTAG unsupported with UCODE Cache"
#endif // endif

/** scbtag_flush_txc_info(): Flush/invalidate a cached TXC cubby in an scbtag */
void
scbtag_flush_txc_info(void)
{
	/* Flush previous cached txc */
	if (scbtag_g.cubbies.host_scb_txc_info != NULL) {

		/* Nearly 256Bytes copied via scbtopcie from Dongle to Host */
		memcpy(scbtag_g.cubbies.host_scb_txc_info,
			scbtag_g.cubbies.cached_scb_txc_info, scbtag_g.context.txc_sz);

		scbtag_g.context.txc_sz = 0;
		scbtag_g.cubbies.host_scb_txc_info = NULL;
	}
} /* scbtag_flush_txc_info */

/** scbtag_cache_txc_info():
 * Cache the scb_txc_info_t into scbtag and return a pointer to the cached
 * scb_txc_info_t object. This allows 232B x 64 = 14.5KB to be in host memory
 * while accesses in wlc_txc_hit(), wlc_txc_iv_upd(), and wlc_txc_cp() to be
 * local. The cached scb_txc_info_t object will be flushed to host when the
 * scbtag is flushed.
 */
static scb_txc_info_t *
scbtag_cache_txc_info(wlc_txc_info_t *txc, struct scb *scb)
{
	scb_txc_info_t * scb_txc_info;

	/* Fetch the pointer to scb_txc_info_t */
	scb_txc_info = SCB_TXC_INFO(txc, scb);

	/* Check if txc cubby is in dongle memory, or does not yet exist */
	if (SCBTAG_IN_DNGL_MEM(scb_txc_info)) {
		return scb_txc_info;
	}

	/* If txc cubby is active in scbtag cache, return cached copy */
	if (scbtag_g.cubbies.host_scb_txc_info == scb_txc_info) {
		SCBTAG_PMSTATS_ADD(chits, 1);
		goto is_cached;
	}

	/* Flush previous txc cubby, if it was cached */
	scbtag_flush_txc_info();

	/* Cache the new scb_txc_info_t into dongle memory
	 * Optimization: May fetch in only part of txh[] and not entire TXOFF
	 * Would require txc_sz to be kept in sync, though.
	 */
	scbtag_g.context.txc_sz = sizeof(scb_txc_info_t);
	scbtag_g.cubbies.host_scb_txc_info = scb_txc_info;

	/* Nearly 256Bytes copied via scbtopcie from Host to Dongle */
	memcpy(scbtag_g.cubbies.cached_scb_txc_info,
		scbtag_g.cubbies.host_scb_txc_info, scbtag_g.context.txc_sz);

	SCBTAG_PMSTATS_ADD(cmiss, 1);

is_cached:
	return (scb_txc_info_t *)scbtag_g.cubbies.cached_scb_txc_info;

} /* scbtag_cache_txc_info() */

#define SCBTAG_FLUSH_TXC_INFO()     scbtag_flush_txc_info()
#define SCBTAG_TXC_INFO(txc, scb)   scbtag_cache_txc_info((txc), (scb))
#else   /* ! BCM_HOST_MEM_TXCTAG */
#define SCBTAG_FLUSH_TXC_INFO()     do { /* noop */ } while (0)
#define SCBTAG_TXC_INFO(txc, scb)   SCB_TXC_INFO((txc), (scb))
#endif  /* ! BCM_HOST_MEM_TXCTAG */

/* d11txh position in the cached entries */
#define SCB_TXC_TXH(sti) ((d11txh_t *)((sti)->txh + (sti)->align + (sti)->offset))
#define SCB_TXC_TXH_NO_OFFST(sti) ((d11txh_t *)((sti)->txh + (sti)->align))

/* handy macros */
#ifdef BCMDBG
#define INCCNTHIT(sti) ((sti)->hit++)
#define INCCNTUSED(sti) ((sti)->used++)
#define INCCNTADDED(sti) ((sti)->added++)
#else
#define INCCNTHIT(sti)
#define INCCNTUSED(sti)
#define INCCNTADDED(sti)
#endif // endif

/* local function */
/* module entries */
#ifdef BCMDBG
static int wlc_txc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
#else
#define wlc_txc_doiovar NULL
#endif // endif
static int wlc_txc_watchdog(void *ctx);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_txc_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
static uint8 wlc_txc_get_free_ucode_cidx(wlc_txc_info_t *txc);
#endif // endif
/* scb cubby */
static int wlc_txc_scb_init(void *ctx, struct scb *scb);
static void wlc_txc_scb_deinit(void *ctx, struct scb *scb);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_txc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_txc_scb_dump NULL
#endif // endif

static void wlc_txc_set_aging(wlc_txc_info_priv_t *priv, void* txhp, bool enable);
#define GET_TXHDRPTR(txc)	((d11txh_t*)(txc->txh + txc->align + txc->offset))

/* module entries */
wlc_txc_info_t *
BCMATTACHFN(wlc_txc_attach)(wlc_info_t *wlc)
{
	wlc_txc_info_t *txc;
	wlc_txc_info_priv_t *priv;

	/* module states */
	if ((txc = MALLOCZ(wlc->osh, WLC_TXC_INFO_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	txc->_txc = FALSE;

	wlc_txc_info_priv_offset = OFFSETOF(wlc_txc_t, priv);
	priv = WLC_TXC_INFO_PRIV(txc);
	priv->wlc = wlc;
	priv->policy = ON;
	priv->gen = 0;
	/* debug builds should not invalidate the txc per watchdog */
#ifdef BCMDBG
	priv->sticky = TRUE;
#else
	priv->sticky = FALSE;
#endif // endif

#ifndef WLC_NET80211

#if defined(BCM_HOST_MEM_TXCTAG)
	if (sizeof(scb_txc_info_t) > SCBTAG_TXC_INFO_MAX_SZ) {
		WL_ERROR(("wl%d: %s: scb_txc_info_t size %d > %d\n",
			wlc->pub->unit, __FUNCTION__,
			sizeof(scb_txc_info_t), SCBTAG_TXC_INFO_MAX_SZ));
		goto fail;
	}
#endif /* BCM_HOST_MEM_TXCTAG */

	/* reserve a cubby in the scb */
	priv->scbh =
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		wlc_scb_cubby_reserve(wlc, sizeof(scb_txc_info_t *),
			wlc_txc_scb_init, wlc_txc_scb_deinit,
			wlc_txc_scb_dump, txc, SCB_CUBBY_ID_TXC);
#else  /* !(BCM_HOST_MEM_RESTORE && BCM_HOST_MEM_SCB) */
		wlc_scb_cubby_reserve(wlc, sizeof(scb_txc_info_t *),
			wlc_txc_scb_init, wlc_txc_scb_deinit,
			wlc_txc_scb_dump, txc);
#endif /* !(BCM_HOST_MEM_RESTORE && BCM_HOST_MEM_SCB) */

	if (priv->scbh < 0) {
		WL_ERROR(("wl%d: %s: cubby register for txc failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, txc_iovars, "txc", txc, wlc_txc_doiovar,
	                        wlc_txc_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "txc", wlc_txc_dump, txc);
#endif // endif

	return txc;

fail:
	wlc_txc_detach(txc);
	return NULL;
}

void
BCMATTACHFN(wlc_txc_detach)(wlc_txc_info_t *txc)
{
	wlc_txc_info_priv_t *priv;
	wlc_info_t *wlc;

	if (txc == NULL)
		return;

	priv = WLC_TXC_INFO_PRIV(txc);
	wlc = priv->wlc;

	wlc_module_unregister(wlc->pub, "txc", txc);

	MFREE(wlc->osh, txc, WLC_TXC_INFO_SIZE);
}

#ifdef BCMDBG
/* handle related iovars */
static int
wlc_txc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
	int32 *ret_int_ptr;
	bool bool_val;
	int32 int_val = 0;
	int err = BCME_OK;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* all iovars require mcnx being enabled */
	switch (actionid) {
	case IOV_GVAL(IOV_TXC):
		*ret_int_ptr = (int32)txc->_txc;
		break;

	case IOV_GVAL(IOV_TXC_POLICY):
		*ret_int_ptr = (int32)priv->policy;
		break;

	case IOV_SVAL(IOV_TXC_POLICY):
		priv->policy = bool_val;
		if (!WLC_TXC_ENAB(priv->wlc))
			break;
		wlc_txc_upd(txc);
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

static int
wlc_txc_watchdog(void *ctx)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
#ifdef BCMDBG
	bool old;

	/* check for missing call to wlc_txc_upd() */
	old = txc->_txc;
	if (WLC_TXC_ENAB(priv->wlc))
		wlc_txc_upd(txc);
	ASSERT(txc->_txc == old);
#endif /* BCMDBG */

	/* invalidate tx header cache once per second if not sticky */
	if (!priv->sticky)
		priv->gen++;

	return BCME_OK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_txc_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);

	bcm_bprintf(b, "enab %d txc %d policy %d sticky %d gen %d\n",
	            WLC_TXC_ENAB(priv->wlc),
	            txc->_txc, priv->policy, priv->sticky, priv->gen);

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */

uint16
wlc_txc_get_txh_offset(wlc_txc_info_t *txc, struct scb *scb)
{
	scb_txc_info_t *sti;
	sti = SCBTAG_TXC_INFO(txc, scb);
	return sti->offset;
}
uint32
wlc_txc_get_d11hdr_len(wlc_txc_info_t *txc, struct scb *scb)
{
	scb_txc_info_t *sti;
	sti = SCBTAG_TXC_INFO(txc, scb);
	return (sti->txhlen - sti->offset);
}

#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
uint8* wlc_txc_get_rate_info_shdr(wlc_txc_info_t *txc, int cache_idx)
{
	scb_txc_info_t *sti = (scb_txc_info_t *)txc->ucache_entry_inuse[cache_idx];
	ASSERT(sti != NULL);
	return (sti->txh + sti->txhlen + sti->align);
}
#endif /* defined(WLC_UCODE_CACHE) && D11CONF_GE(42) */

#ifndef WLC_NET80211
/* update the iv field of the tx header in the cache */
void BCMFASTPATH
wlc_txc_iv_upd(wlc_txc_info_t *txc, wlc_bsscfg_t *cfg, struct scb *scb,
	wsec_key_t *key, bool uc_seq)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
	wlc_info_t *wlc = priv->wlc;
	scb_txc_info_t *sti;
	d11txh_t *txh;
	uint8 *iv;

	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	ASSERT(sti != NULL);

	/* go to beginning of IV  */
	iv = sti->txh + sti->align + sti->txhlen - key->iv_len;
	wlc_key_iv_update(wlc, cfg, key, iv, 1, uc_seq);

	txh = SCB_TXC_TXH(sti);
	wlc_txh_iv_upd(wlc, txh, iv, key, uc_seq);
}

/* retrieve tx header from the cache and copy it to the packet.
 * return d11txh pointer in the packet where the copy started.
 */
d11txh_t * BCMFASTPATH
wlc_txc_cp(wlc_txc_info_t *txc, struct scb *scb, void *pkt, uint *flags)
{
	wlc_txc_info_priv_t *priv;
	wlc_info_t *wlc;
	scb_txc_info_t *sti;
	d11txh_t *txh;
	uint txhlen;
	uint8 *cp;

	ASSERT(txc->_txc == TRUE);

	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	ASSERT(sti != NULL);

	txhlen = sti->txhlen;

	priv = WLC_TXC_INFO_PRIV(txc);
	wlc = priv->wlc;
	(void)wlc;

	/* basic sanity check the tx cache */
	ASSERT(txhlen >= (D11_TXH_SHORT_LEN(wlc) + DOT11_A3_HDR_LEN) && txhlen < TXOFF);

	txh = SCB_TXC_TXH_NO_OFFST(sti);
	cp = PKTPUSH(wlc->osh, pkt, txhlen);
	bcopy((uint8 *)txh, cp, txhlen);

	/* return the saved pkttag flags */
	*flags = sti->flags;
	INCCNTUSED(sti);

	return (d11txh_t *)(cp + sti->offset);
}

/* check if we can find anything in the cache */
bool BCMFASTPATH
wlc_txc_hit(wlc_txc_info_t *txc, wlc_bsscfg_t *cfg, struct scb *scb,
	void *sdu, uint pktlen, uint fifo, uint8 prio)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
	wlc_info_t *wlc = priv->wlc;
	scb_txc_info_t *sti;
	uint miss;

	ASSERT(txc->_txc == TRUE);

	ASSERT(cfg != NULL);
	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	if (sti == NULL)
		return FALSE;

	miss = (D11REV_LT(wlc->pub->corerev, 40) && (pktlen != sti->pktlen));
	if (miss)
		return FALSE;
	miss |= (sti->txhlen == 0);
	miss |= (fifo != sti->fifo);
	miss |= (prio != sti->prio);
	miss |= (sti->gen != priv->gen);
	miss |= ((sti->flags & (WLF_EXEMPT_MASK | WLF_AMPDU_MPDU)) !=
		(WLPKTTAG(sdu)->flags & (WLF_EXEMPT_MASK | WLF_AMPDU_MPDU)));
	miss |= ((pktlen >= wlc->fragthresh[fifo]) && ((WLPKTTAG(sdu)->flags & WLF_AMSDU) == 0));
	if (BSSCFG_AP(cfg) || BSS_TDLS_ENAB(wlc, cfg)) {
		miss |= (sti->ps_on != SCBTAG_PS(scb));
	}
	if (!PIO_ENAB(wlc->pub)) {
		/* calc the number of descriptors needed to queue this frame */
		uint ndesc = pktsegcnt(wlc->osh, sdu);
		if (BCM4331_CHIP_ID == CHIPID(wlc->pub->sih->chip))
			ndesc *= 2;
		miss |= (uint)TXAVAIL(wlc, fifo) < ndesc;
	} else
		miss |= !wlc_pio_txavailable(WLC_HW_PIO(wlc, fifo),
			(pktlen + D11_TXH_LEN_EX(wlc) + DOT11_A3_HDR_LEN), 1);
#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	if (miss && WL_PRPKT_ON()) {
		printf("txc missed: scb %p gen %d txcgen %d hdr_len %d body len %d, pkt_len %d\n",
		        scb, sti->gen, priv->gen, sti->txhlen, sti->pktlen, pktlen);
	}
#endif // endif

	if (miss == 0) {
		INCCNTHIT(sti);
		return TRUE;
	}

	return FALSE;
}

/* install tx header from the packet into the cache */
void BCMFASTPATH
wlc_txc_add(wlc_txc_info_t *txc, wlc_bsscfg_t *cfg, struct scb *scb,
	void *pkt, uint txhlen, uint fifo, uint8 prio,
	uint16 txh_off, uint d11hdr_len)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
	wlc_info_t *wlc = priv->wlc;
	scb_txc_info_t *sti;
	d11txh_t *txh;
	osl_t *osh;
#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
	uint8 ucidx;
	d11actxh_t* vhtHdr;
#endif // endif
	ASSERT(txc->_txc == TRUE);

	ASSERT(cfg != NULL);
	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;

	osh = wlc->osh;

	/* cache any pkt with same DA-SA-L(before LLC/SNAP), hdr must be in one buffer */

	sti->align = (uint8)((ulong)PKTDATA(osh, pkt) & 3);
	ASSERT((sti->align == 0) || (sti->align == 2));
	sti->offset = txh_off;

#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
	if (D11REV_GE(wlc->pub->corerev, 42)) {
		if (txc->ucache_entry_inuse[sti->ucache_idx] != sti) {
			ucidx = wlc_txc_get_free_ucode_cidx(txc);
			if (ucidx >= WLC_MAX_UCODE_CACHE_ENTRIES)
				return;
			sti->ucache_idx = ucidx;
			txc->ucache_entry_inuse[ucidx] = sti;
		} else {
			ASSERT(sti->ucache_idx < WLC_MAX_UCODE_CACHE_ENTRIES);
		}
		{
		d11actxh_pkt_t *short_hdr;
		d11ac_tso_t *tsoHdr;
		/*
		* For uCode based TXD cache, txc->txh contain cached header data
		* in the following format: This is to avoid second copy for mac header
		* when there is  cache hit.
		* | d11actxh_pkt_t | MAC Header of length 'd11hdr_len' |
		*  d11actxh_rate_t | d11actxh_cache_t|
		*/
		/* Copy tshoHeader and shot TXD */
		bcopy(PKTDATA(osh, pkt), sti->txh + sti->align,  sti->offset + D11AC_TXH_SHORT_LEN);
		/* Copy mac header parameters. */
		bcopy(PKTDATA(osh, pkt) + D11AC_TXH_LEN + sti->offset,
			sti->txh + sti->align + sti->offset + D11AC_TXH_SHORT_LEN,
			d11hdr_len);
		/* At the end copy the remaining long TXD */
		 bcopy(PKTDATA(osh, pkt) + D11AC_TXH_SHORT_LEN + sti->offset,
		 sti->txh + sti->align + D11AC_TXH_SHORT_LEN + d11hdr_len +
		 sti->offset, D11AC_TXH_LEN - D11AC_TXH_SHORT_LEN);

		tsoHdr = (d11ac_tso_t*)(sti->txh + sti->align);
		/* Update tsoHdr short descriptor */
		short_hdr = (d11actxh_pkt_t *) (sti->txh + sti->align + sti->offset);
		tsoHdr->flag[2] |= TOE_F2_TXD_HEAD_SHORT;
		short_hdr->MacTxControlLow |= htol16(D11AC_TXC_HDR_FMT_SHORT |((sti->ucache_idx <<
			D11AC_TXC_CACHE_IDX_SHIFT) & D11AC_TXC_CACHE_IDX_MASK));
		short_hdr->MacTxControlLow &= htol16(~D11AC_TXC_UPD_CACHE);
		sti->txhlen = D11AC_TXH_SHORT_LEN + sti->offset + d11hdr_len;
		short_hdr->PktCacheLen = 0;
		short_hdr->TxStatus = 0;
		short_hdr->Tstamp = 0;

		wlc_pkt_get_vht_hdr(wlc, pkt, &vhtHdr);
		vhtHdr->PktInfo.MacTxControlLow |=
			htol16(D11AC_TXC_UPD_CACHE |((sti->ucache_idx <<
			D11AC_TXC_CACHE_IDX_SHIFT) & D11AC_TXC_CACHE_IDX_MASK));

		}
	} else {
		sti->txhlen = txhlen;
		bcopy(PKTDATA(osh, pkt), sti->txh + sti->align, txhlen);
	}
#else
	sti->txhlen = txhlen;
	bcopy(PKTDATA(osh, pkt), sti->txh + sti->align, txhlen);
#endif /* defined(WLC_UCODE_CACHE) && D11CONF_GE(42) */

	txh = SCB_TXC_TXH(sti);
	sti->pktlen = pkttotlen(osh, pkt) - txhlen + ETHER_HDR_LEN;
	sti->fifo = fifo;
	sti->prio = prio;
	sti->gen = priv->gen;

	if (BSSCFG_AP(cfg) || BSS_TDLS_ENAB(wlc, cfg)) {
		sti->ps_on = SCB_PS(scb);
	}
	/* need MPDU flag to avoid going back to prep_sdu */
	sti->flags = (WLF_MPDU | WLF_TXHDR |
	              (WLPKTTAG(pkt)->flags & (WLF_MIMO | WLF_VRATE_PROBE | WLF_RATE_AUTO |
	                                       WLF_RIFS | WLF_WME_NOACK | WLF_EXEMPT_MASK |
	                                       WLF_AMPDU_MPDU)));

	/* Clear pkt exptime from the txheader cache */
	wlc_txc_set_aging(priv, txh, FALSE);

#if defined(BCMDBG) || defined(WLMSG_PRPKT)
	if (WL_PRPKT_ON()) {
		printf("install txc: scb %p gen %d hdr_len %d, body len %d\n",
		       scb, sti->gen, sti->txhlen, sti->pktlen);
	}
#endif // endif

	INCCNTADDED(sti);
}

#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
static uint8 wlc_txc_get_free_ucode_cidx(wlc_txc_info_t *txc)
{
	uint8 i;

	for (i = 0; i < WLC_MAX_UCODE_CACHE_ENTRIES; i++) {
		if (!txc->ucache_entry_inuse[i]) {
			return i;
		}
	}
	/*
	 * XXX If we found that all entries are being used, invalidate and reset all entries..
	 */
	wlc_txc_inv_all(txc);
	/* Reset all entries as they use new entry.. */
	for (i = 0; i < WLC_MAX_UCODE_CACHE_ENTRIES; i++) {
		txc->ucache_entry_inuse[i] = NULL;
	}
	/* use ZEROth entry for this txc */
	return 0;
}
#endif /* defined(WLC_UCODE_CACHE)&& D11CONF_GE(42) */

/* update tx header cache enable flag (txc->_txc) */
void
wlc_txc_upd(wlc_txc_info_t *txc)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);

	txc->_txc = (priv->policy == ON) ? TRUE : FALSE;
}
#endif /* !WLC_NET80211 */

/* scb cubby */
static int
wlc_txc_scb_init(void *ctx, struct scb *scb)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);
	wlc_info_t *wlc = priv->wlc;
	scb_txc_info_t **psti = SCB_TXC_CUBBY_LOC(txc, scb);
	scb_txc_info_t *sti = NULL;
	size_t scb_txc_info_sz;

	if (SCB_INTERNAL(scb) || !WLC_TXC_ENAB(wlc)) {
		WL_INFORM(("%s: Not allocating the cubby, txc enab %d\n",
		           __FUNCTION__, WLC_TXC_ENAB(wlc)));
		return BCME_OK;
	}

#if defined(BCM_HOST_MEM_SCBTAG_CUBBIES)
	scb_txc_info_sz = SCBTAG_TXC_INFO_MAX_SZ;
#else
	scb_txc_info_sz = sizeof(scb_txc_info_t);
#endif // endif

#ifdef BCM_HOST_MEM_SCB
	if (SCB_ALLOC_ENAB(wlc->pub) && !SCB_INTERNAL(scb) && SCB_HOST(scb)) {
		sti = (scb_txc_info_t *)wlc_scb_alloc_mem_get(wlc,
			SCB_CUBBY_ID_TXC, scb_txc_info_sz, 1);
	}

	if (!sti)
#endif // endif

	if ((sti = MALLOCZ(wlc->osh, scb_txc_info_sz)) == NULL) {
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
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	wlc_txc_info_priv_t *priv;
	wlc_info_t *wlc;
	scb_txc_info_t **psti = SCB_TXC_CUBBY_LOC(txc, scb);
	scb_txc_info_t *sti = *psti;
	uint32 scb_txc_info_sz;

	if (sti == NULL)
		return;

	priv = WLC_TXC_INFO_PRIV(txc);
	wlc = priv->wlc;
#if defined(WLC_UCODE_CACHE) && D11CONF_GE(42)
	if (D11REV_GE(wlc->pub->corerev, 42)) {
		txc->ucache_entry_inuse[sti->ucache_idx] = NULL;
	}
#endif /* defined(WLC_UCODE_CACHE) && D11CONF_GE(42) */

#if defined(BCM_HOST_MEM_SCBTAG_CUBBIES)
	scb_txc_info_sz = SCBTAG_TXC_INFO_MAX_SZ;
#else
	scb_txc_info_sz = sizeof(scb_txc_info_t);
#endif // endif
	/* Flush scbtag */
	SCBTAG_FLUSH_TXC_INFO();

#ifdef BCM_HOST_MEM_SCB
	if (SCB_ALLOC_ENAB(wlc->pub) && SCB_HOST(scb)) {
		wlc_scb_alloc_mem_free(wlc, SCB_CUBBY_ID_TXC, (void *)sti);
	}
	else
#endif // endif
	MFREE(wlc->osh, sti, scb_txc_info_sz);
	*psti = NULL;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_txc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)ctx;
	scb_txc_info_t *sti;

	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;

	bcm_bprintf(b, "     txc %d\n", txc->_txc);
	bcm_bprintf(b, "\toffset %u length %u\n", sti->offset, sti->txhlen);
	bcm_bprhex(b, "\ttxh ", TRUE, sti->txh, TXOFF);
	bcm_bprintf(b, "\tpktlen %u gen %u flags 0x%x fifo %u prio %u ps %u\n",
	            sti->pktlen, sti->gen, sti->flags, sti->fifo, sti->prio, sti->ps_on);
#ifdef BCMDBG
	bcm_bprintf(b, "\thit %u used %u added %u\n",
	            sti->hit, sti->used, sti->added);
#endif // endif
}
#endif /* BCMDBG || BCMDBG_DUMP */

#ifndef WLC_NET80211
/* invalidate the cache entry */
void
wlc_txc_inv(wlc_txc_info_t *txc, struct scb *scb)
{
	scb_txc_info_t *sti;

	ASSERT(scb != NULL);

	sti = SCBTAG_TXC_INFO(txc, scb);
	if (sti == NULL)
		return;

	sti->txhlen = 0;
}

/* give out the location where others can use to invalidate the entry */
uint *
wlc_txc_inv_ptr(wlc_txc_info_t *txc, struct scb *scb)
{
	scb_txc_info_t *sti;

	ASSERT(scb != NULL);

	sti = SCB_TXC_INFO(txc, scb);
	if (sti == NULL)
		return NULL;

	/* XXX
	 * Even when BCM_HOST_MEM_TXCTAG is defined, the pointer returned is to
	 * host side scb_txc_info::txhlen field.
	 * When the caller eventually updates this field, caller needs to ensure
	 * that the scb_txc_info is flush/invalidated.
	 *
	 * See INVALIDATE_TXH_CACHE() in wlc_rate_sel.c
	 */
	SCBTAG_FLUSH_TXC_INFO();

	return (uint *)&sti->txhlen;
}
#endif /* !WLC_NET80211 */

/* invalidate all cache entries */
void
wlc_txc_inv_all(wlc_txc_info_t *txc)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);

	priv->gen++;
}

#ifdef WLC_NET80211
/* accessors */
int
wlc_txc_get_gen(wlc_txc_info_t *txc)
{
	wlc_txc_info_priv_t *priv = WLC_TXC_INFO_PRIV(txc);

	return priv->gen;
}
#endif // endif

static void
wlc_txc_set_aging(wlc_txc_info_priv_t *priv, void* txhp, bool enable)
{
	wlc_txh_set_aging(priv->wlc, txhp, enable);
}

/* following functions implement the process that when SCB */
/* migration from dongle to host or host to dongle */
/* memory of txc_cubby have been allocated in the target space */
/* and content of original one is copied to target */
/* all the embedded SCB in the txc_cubby have been updated into the new SCB */
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
int
txc_cubby_dongle2host(wlc_info_t *wlc, void *context, struct scb* scb_dongle, struct scb* scb_host)
{
		wlc_txc_info_t *txc = (wlc_txc_info_t *)context;
		scb_txc_info_t **scb_info_txc = SCB_TXC_CUBBY_LOC(txc, scb_dongle);
		scb_txc_info_t **shadow_scb_info_txc = SCB_TXC_CUBBY_LOC(txc, scb_host);
		if (scb_info_txc != NULL) {
			scb_txc_info_t *sti_host = NULL;
			scb_txc_info_t *sti_dongle = *scb_info_txc;
			if (sti_dongle != NULL) {
				sti_host = (scb_txc_info_t *)wlc_scb_alloc_mem_get(wlc,
					SCB_CUBBY_ID_TXC, sizeof(scb_txc_info_t), 1);
			if (!sti_host)
				return BCME_NOMEM;
			memcpy(sti_host, sti_dongle, sizeof(scb_txc_info_t));
			*shadow_scb_info_txc = sti_host;
			MFREE(wlc->osh, sti_dongle, sizeof(scb_txc_info_t));
			return BCME_OK;
			}
		}
		return BCME_ERROR;
}

int
txc_cubby_host2dongle(wlc_info_t *wlc, void *context, struct scb* scb_dongle, struct scb* scb_host)
{
	wlc_txc_info_t *txc = (wlc_txc_info_t *)context;
	scb_txc_info_t **psti_dongle = SCB_TXC_CUBBY_LOC(txc, scb_dongle);
	scb_txc_info_t **psti_host = SCB_TXC_CUBBY_LOC(txc, scb_host);
	scb_txc_info_t *sti_dongle = NULL;

	if (psti_host != NULL) {
		scb_txc_info_t *sti_host = *psti_host;

		if (sti_host != NULL) {
			sti_dongle = MALLOCZ(wlc->osh, sizeof(scb_txc_info_t));
			if (sti_dongle == NULL) {
				WL_ERROR(("wl%d: %s: failed allocate txhdr cache cubby space\n",
					wlc->pub->unit, __FUNCTION__));
				return NULL;
		}

		memcpy(sti_dongle, sti_host, sizeof(scb_txc_info_t));
		*psti_dongle = sti_dongle;

		wlc_scb_alloc_mem_free(wlc, SCB_CUBBY_ID_TXC, (void *)sti_host);
		return BCME_OK;
		}
	}
	return BCME_ERROR;
}
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */
