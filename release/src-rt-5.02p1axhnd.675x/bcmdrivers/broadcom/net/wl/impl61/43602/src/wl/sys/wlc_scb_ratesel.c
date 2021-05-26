/*
 * Wrapper to scb rate selection algorithm of Broadcom
 * 802.11 Networking Adapter Device Driver.
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
 * $Id: wlc_scb_ratesel.c 772373 2019-02-21 20:14:21Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlRateSelection]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <proto/802.11.h>
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

#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif
#include <wlc_txc.h>

#include <wl_dbg.h>

#ifdef WL11AC
#include <wlc_vht.h>
#endif // endif

typedef struct ppr_rateset {
	uint8 mcs[PHY_CORE_MAX];	/* supported mcs index bit map */
	uint8 vht_mcsmap;		/* supported vht mcs nss bit map */
} ppr_rateset_t;

/* Supported rates for current chanspec/country */
typedef struct ppr_support_rates {
	chanspec_t chanspec;
	clm_country_t country;
	ppr_rateset_t ppr_20_rates;
#if defined(WL11N) || defined(WL11AC)
	ppr_rateset_t ppr_40_rates;
#endif // endif
#if defined(WL11AC)
	ppr_rateset_t ppr_80_rates;
#endif // endif
} ppr_support_rates_t;

struct wlc_ratesel_info {
	wlc_info_t	*wlc;		/* pointer to main wlc structure */
	wlc_pub_t	*pub;		/* public common code handler */
	ratesel_info_t *rsi;
	int32 scb_handle;
	int32 cubby_sz;
	ppr_support_rates_t *ppr_rates;
};

typedef struct ratesel_cubby ratesel_cubby_t;

/* rcb is per scb per ac rate control block. */
struct ratesel_cubby {
	rcb_t *scb_cubby;
};

#define SCB_RATESEL_INFO(wss, scb) ((SCB_CUBBY((scb), (wrsi)->scb_handle)))

#if defined(WME_PER_AC_TX_PARAMS)
#define SCB_RATESEL_CUBBY(wrsi, scb, ac) 	\
	((void *)(((char*)((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby) + \
		(ac * (wrsi)->cubby_sz)))
#define FID2AC(pub, fid) \
	(WME_PER_AC_MAXRATE_ENAB(pub) ? wme_fifo2ac[(fid) & TXFID_QUEUE_MASK] : 0)
#else /* WME_PER_AC_TX_PARAMS */
#define SCB_RATESEL_CUBBY(wrsi, scb, ac)	\
	(((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby)
#define FID2AC(pub, fid) (0)
#endif /* WME_PER_AC_TX_PARAMS */

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
extern void swap_cubby_ratesel_scb(rcb_t *scb_rate_cubby, struct scb* scb_new);
#endif // endif

static int wlc_scb_ratesel_scb_init(void *context, struct scb *scb);
static void wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb);
#ifdef BCMDBG
extern void wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#endif // endif

static ratespec_t wlc_scb_ratesel_getcurspec(wlc_ratesel_info_t *wrsi,
	struct scb *scb, uint16 *frameid);

static rcb_t *wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint16 frameid);
static int wlc_scb_ratesel_cubby_sz(void);
#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx);
void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx);
int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx);
#endif // endif
/* Get CLM enabled rates bitmap for a bw */
static  ppr_rateset_t *
wlc_scb_ratesel_get_ppr_rates(wlc_info_t *wlc, wl_tx_bw_t bw);

static void wlc_scb_ratesel_ppr_updbmp(wlc_info_t *wlc, ppr_t *target_pwrs);

static void
wlc_scb_ratesel_ppr_filter(wlc_info_t *wlc, ppr_rateset_t *clm_rates,
	wlc_rateset_t *scb_rates, bool scb_VHT);

wlc_ratesel_info_t *
BCMATTACHFN(wlc_scb_ratesel_attach)(wlc_info_t *wlc)
{
	wlc_ratesel_info_t *wrsi;
	ppr_support_rates_t *ppr_rates;

	if (!(wrsi = (wlc_ratesel_info_t *)MALLOC(wlc->osh, sizeof(wlc_ratesel_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)wrsi, sizeof(wlc_ratesel_info_t));

	if (!(ppr_rates = (ppr_support_rates_t *)MALLOC(wlc->osh, sizeof(ppr_support_rates_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero(ppr_rates, sizeof(*ppr_rates));
	wrsi->ppr_rates = ppr_rates;
	wrsi->wlc = wlc;
	wrsi->pub = wlc->pub;

	if ((wrsi->rsi = wlc_ratesel_attach(wlc)) == NULL) {
		WL_ERROR(("%s: failed\n", __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the scb container for per-scb-ac private data */
	wrsi->scb_handle = wlc_scb_cubby_reserve(wlc, wlc_scb_ratesel_cubby_sz(),
	                                        wlc_scb_ratesel_scb_init,
	                                        wlc_scb_ratesel_scb_deinit,
#ifdef BCMDBG
	                                        wlc_scb_ratesel_dump_scb,
#else
	                                        NULL,
#endif // endif
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
	                                        (void *)wlc, SCB_CUBBY_ID_RATESEL);
#else
						(void *)wlc);
#endif // endif

	if (wrsi->scb_handle < 0) {
		WL_ERROR(("wl%d: %s:wlc_scb_cubby_reserve failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(WME_PER_AC_TX_PARAMS) && defined(WME_PER_AC_MAXRATE_DISABLE)
	wrsi->cubby_sz = 0;
#else
	wrsi->cubby_sz = wlc_ratesel_rcb_sz();
#endif // endif

#ifdef WL11N
	wlc_ratesel_rssi_attach(wrsi->rsi, wlc_scb_ratesel_rssi_enable,
		wlc_scb_ratesel_rssi_disable, wlc_scb_ratesel_get_rssi);
#endif // endif

	return wrsi;

fail:
	if (wrsi->rsi)
		wlc_ratesel_detach(wrsi->rsi);

	MFREE(wlc->osh, wrsi, sizeof(wlc_ratesel_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_scb_ratesel_detach)(wlc_ratesel_info_t *wrsi)
{
	if (!wrsi)
		return;

	wlc_ratesel_detach(wrsi->rsi);

	MFREE(wrsi->pub->osh, wrsi->ppr_rates, sizeof(ppr_support_rates_t));
	MFREE(wrsi->pub->osh, wrsi, sizeof(wlc_ratesel_info_t));
}

/* alloc per ac cubby space on scb attach. */
static int
wlc_scb_ratesel_scb_init(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	rcb_t *scb_rate_cubby = NULL;
	int cubby_size;

#if defined(WME_PER_AC_TX_PARAMS)
#if defined(WME_PER_AC_MAXRATE_DISABLE)
	cubby_size = wlc_ratesel_rcb_sz();
#else
	cubby_size = AC_COUNT * wrsi->cubby_sz;
#endif // endif
#else
	cubby_size = wrsi->cubby_sz;
#endif /* WME_PER_AC_TX_PARAMS */

	WL_RATE(("%s scb %p allocate cubby space.\n", __FUNCTION__, scb));
	if (scb && !SCB_INTERNAL(scb)) {
#ifdef BCM_HOST_MEM_SCB
		if (SCB_ALLOC_ENAB(wlc->pub) && SCB_HOST(scb)) {
			scb_rate_cubby = (rcb_t *)wlc_scb_alloc_mem_get(wrsi->wlc,
				SCB_CUBBY_ID_RATESEL, cubby_size, 1);
		}
		if (!scb_rate_cubby)
#endif // endif

		scb_rate_cubby = (rcb_t *)MALLOC(wlc->osh, cubby_size);
		if (!scb_rate_cubby)
			return BCME_NOMEM;
		bzero(scb_rate_cubby, cubby_size);
		cubby_info->scb_cubby = scb_rate_cubby;
	}
	return BCME_OK;
}

/* free cubby space after scb detach */
static void
wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	int cubby_size;

#if defined(WME_PER_AC_TX_PARAMS)
#if defined(WME_PER_AC_MAXRATE_DISABLE)
	cubby_size = wlc_ratesel_rcb_sz();
#else
	cubby_size = AC_COUNT * wrsi->cubby_sz;
#endif // endif
#else
	cubby_size = wrsi->cubby_sz;
#endif /* WME_PER_AC_TX_PARAMS */

	WL_RATE(("%s scb %p free cubby space.\n", __FUNCTION__, scb));
	if (wlc && cubby_info && !SCB_INTERNAL(scb) && cubby_info->scb_cubby) {

#ifdef BCM_HOST_MEM_SCB
		if (SCB_ALLOC_ENAB(wlc->pub) && SCB_HOST(scb)) {
			wlc_scb_alloc_mem_free(wlc, SCB_CUBBY_ID_RATESEL,
				(void *)cubby_info->scb_cubby);
		}
		else
#endif // endif
		MFREE(wlc->osh, cubby_info->scb_cubby, cubby_size);

		cubby_info->scb_cubby = NULL;
	}
}

#ifdef BCMDBG
extern void
wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	int32 ac;
	rcb_t *rcb;

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		rcb = SCB_RATESEL_CUBBY(wrsi, scb, ac);
		wlc_ratesel_dump_rcb(rcb, ac, b);
	}
}
#endif // endif

static rcb_t *
wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid)
{
	int ac;

	ASSERT(wrsi);

	ac = FID2AC(wrsi->pub, frameid);
	BCM_REFERENCE(ac);
	ASSERT(ac < AC_COUNT);

	return (SCB_RATESEL_CUBBY(wrsi, scb, ac));
}

extern const uint8 prio2fifo[NUMPRIO];
extern int
wlc_wme_downgrade_fifo(wlc_info_t *wlc, uint* p_fifo, struct scb *scb);

#ifdef WLATF
/* Get the rate selection control block pointer from ratesel cubby */
rcb_t *
wlc_scb_ratesel_getrcb(wlc_info_t *wlc, struct scb *scb, uint ac)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ASSERT(wrsi);

	if (!WME_PER_AC_MAXRATE_ENAB(wlc->pub)) {
		ac = 0;
	}
	ASSERT(ac < (uint)WME_MAX_AC(wlc, scb));

	return (SCB_RATESEL_CUBBY(wrsi, scb, ac));
}
#endif /* WLATF */

#ifdef WL11N
bool
wlc_scb_ratesel_sync(wlc_ratesel_info_t *wrsi, struct scb *scb, uint fifo, uint now, int rssi)
{
	rcb_t *state;
	uint16 frameid;

	frameid = fifo & TXFID_QUEUE_MASK;
	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);

	return wlc_ratesel_sync(state, now, rssi);
}
#endif /* WL11N */

ratespec_t
wlc_scb_ratesel_getcurspec(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 *frameid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, *frameid);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p frameid = %d\n",
			__FUNCTION__, wrsi, scb, *frameid));
		ASSERT(0);
		return (WLC_RATE_6M | RSPEC_BW_20MHZ);
	}
	return wlc_ratesel_getcurspec(state);
}

/* given only wlc and scb, return best guess at the primary rate */
ratespec_t
wlc_scb_ratesel_get_primary(wlc_info_t *wlc, struct scb *scb, void *pkt)
{
	uint16 frameid = 0;
	ratespec_t rspec = 0;
	wlcband_t *scbband;
	wlc_bsscfg_t *bsscfg;
	uint32 scb_flags, scb_flags2;
	bool ismulti;
	uint phyctl1_stf = wlc->stf->ss_opmode;
	uint fifo;
	uint8 prio;
#ifdef WME
	uint tfifo;
#endif /* WME */

#ifdef WL11N
	uint32 mimo_txbw = 0;
#if defined(BCMDBG) || defined(WLTEST)
	uint32 _txbw2rspecbw[] = {
		RSPEC_BW_20MHZ, /* WL_TXBW20L	*/
		RSPEC_BW_20MHZ, /* WL_TXBW20U	*/
		RSPEC_BW_40MHZ, /* WL_TXBW40	*/
		RSPEC_BW_40MHZ, /* WL_TXBW40DUP */
		RSPEC_BW_20MHZ, /* WL_TXBW20LL */
		RSPEC_BW_20MHZ, /* WL_TXBW20LU */
		RSPEC_BW_20MHZ, /* WL_TXBW20UL */
		RSPEC_BW_20MHZ, /* WL_TXBW20UU */
		RSPEC_BW_40MHZ, /* WL_TXBW40L */
		RSPEC_BW_40MHZ, /* WL_TXBW40U */
		RSPEC_BW_80MHZ /* WL_TXBW80 */
	};
#endif /* defined(BCMDBG) || defined(WLTEST) */
#endif /* WL11N */

#if defined(BCM_HOST_MEM_SCBTAG)
	scbband = scbtag_band(scb);
#else
	scbband = wlc_scbband(scb);
#endif // endif

#if defined(BCM_HOST_MEM_SCBTAG)
	if (SCBTAG_IS_CACHED(scb)) {         /* Cached access */
		bsscfg     = SCBTAG_G_BSSCFG;    /* SCBTAG_BSSCFG() */
		scb_flags  = SCBTAG_G_FLAGS;     /* SCBTAG_FLAGS() */
		scb_flags2 = SCBTAG_G_FLAGS2;    /* SCBTAG_FLAGS2() */
		ismulti    = SCBTAG_G_ISMULTI;   /* SCBTAG_ISMULTI() */
		SCBTAG_PMSTATS_ADD(rhits, 4);
	}
	else
#endif /* BCM_HOST_MEM_SCBTAG */
	{
		bsscfg     = SCB_BSSCFG(scb);
		scb_flags  = SCB_FLAGS(scb);
		scb_flags2 = SCB_FLAGS2(scb);
		ismulti    = SCB_ISMULTI(scb);
	}

	prio = 0;
	if ((pkt != NULL) && SCBF_QOS(scb_flags)) {
		prio = (uint8)PKTPRIO(pkt);
		ASSERT(prio <= MAXPRIO);
	}

	fifo = TX_AC_BE_FIFO;

	if (BSSCFG_AP(bsscfg) && ismulti && WLC_BCMC_PSMODE(wlc, bsscfg)) {
		fifo = TX_BCMC_FIFO;
	}
#ifdef WLBTAMP
	else if (SCB_11E(scb)) {
		fifo = prio2fifo[prio];
	}
#endif /* WLBTAMP */
	else if (SCBF_WME(scb_flags)) {
		fifo = prio2fifo[prio];
#ifdef	WME
		tfifo = fifo;
		if (wlc_wme_downgrade_fifo(wlc, &fifo, scb) == BCME_ERROR) {
			/* packet may be tossed; give a best guess anyway */
			fifo = tfifo;
		}
#endif /* WME */
	}

	/* get best guess at frameid */
	frameid = fifo & TXFID_QUEUE_MASK;

	if (scbband == NULL) {
		ASSERT(0);
		return 0;
	}
	/* XXX 4360: the following rspec calc code is now in 3 places;
	 * here, d11achdrs and d11nhdrs.
	 * Need to consolidate this.
	 */
	if (RSPEC_ACTIVE(scbband->rspec_override)) {
		/* get override if active */
		rspec = scbband->rspec_override;
	} else {
		/* let ratesel figure it out if override not present */
		rspec = wlc_scb_ratesel_getcurspec(wlc->wrsi, scb, &frameid);
	}

#ifdef WL11N
	if (N_ENAB(wlc->pub)) {
		/* apply siso/cdd to single stream mcs's or ofdm if rspec is auto selected */
		if (((IS_MCS(rspec) && IS_SINGLE_STREAM(rspec & RSPEC_RATE_MASK)) ||
			IS_OFDM(rspec)) &&
			!(rspec & RSPEC_OVERRIDE_MODE)) {

			rspec &= ~(RSPEC_TXEXP_MASK | RSPEC_STBC);

			/* For SISO MCS use STBC if possible */
			if (IS_MCS(rspec) && (WLC_IS_STBC_TX_FORCED(wlc) ||
				((RSPEC_ISVHT(rspec) && WLC_STF_SS_STBC_VHT_AUTO(wlc, scb)) ||
				(RSPEC_ISHT(rspec) && WLC_STF_SS_STBC_HT_AUTO(wlc, scb))))) {
				ASSERT(WLC_STBC_CAP_PHY(wlc));
				rspec |= RSPEC_STBC;
			} else if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << RSPEC_TXEXP_SHIFT);
			}
		}

		/* bandwidth */
		if (RSPEC_BW(rspec) != RSPEC_BW_UNSPECIFIED) {
			mimo_txbw = RSPEC_BW(rspec);
		} else if (CHSPEC_IS80(wlc->chanspec) && RSPEC_ISVHT(rspec)) {
			mimo_txbw = RSPEC_BW_80MHZ;
		} else if (CHSPEC_IS40(wlc->chanspec) || CHSPEC_IS80(wlc->chanspec)) {
			/* default txbw is 20in40 */
			mimo_txbw = RSPEC_BW_20MHZ;

			if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
				if (SCBF_40_CAP(scb_flags)) {
					mimo_txbw = RSPEC_BW_40MHZ;
#ifdef WLMCHAN
				/* XXX 4360: why would scb->flags indicate is40 if the sta
				 * is associated at a 20MHz? Do we need different flags for
				 * capability (is40) from operational for the current state,
				 * which would be is20?  This same problem needs to be fixed
				 * for the 80MHz case.
				 */
				/* PR95044: if mchan enabled and bsscfg is AP, then must
				 * check the bsscfg chanspec to make sure our AP is
				 * operating on 40MHz channel.
				 */
				if (MCHAN_ENAB(wlc->pub) && BSSCFG_AP(bsscfg) &&
					CHSPEC_IS20(bsscfg->current_bss->chanspec)) {
					mimo_txbw = RSPEC_BW_20MHZ;
				}
#endif /* WLMCHAN */
				}
			}
		} else	{
			mimo_txbw = RSPEC_BW_20MHZ;
		}

#if defined(BCMDBG) || defined(WLTEST)
		if (CHSPEC_IS40(wlc->chanspec) || CHSPEC_IS80(wlc->chanspec)) {
			/* use txbw overrides */
			if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
				if (wlc->mimo_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->mimo_40txbw];
				}
			} else if (IS_OFDM(rspec)) {
				if (wlc->ofdm_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->ofdm_40txbw];
				}
			} else {
				ASSERT(IS_CCK(rspec));
				if (wlc->cck_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->cck_40txbw];
				}
			}
		}
#endif /* defined(BCMDBG) || defined(WLTEST) */
		rspec &= ~RSPEC_BW_MASK;
		rspec |= mimo_txbw;
	} else
#endif /* WL11N */
	{
		rspec |= RSPEC_BW_20MHZ;
		/* for nphy, stf of ofdm frames must follow policies */
		if ((WLCISNPHY(scbband) || WLCISHTPHY(scbband)) && IS_OFDM(rspec)) {
			rspec &= ~RSPEC_TXEXP_MASK;
			if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << RSPEC_TXEXP_SHIFT);
			}
		}
	}

	if (!RSPEC_ACTIVE(scbband->rspec_override)) {
		if (IS_MCS(rspec) && (wlc->sgi_tx == ON))
			rspec |= RSPEC_SHORT_GI;
		else if (wlc->sgi_tx == OFF)
			rspec &= ~RSPEC_SHORT_GI;

	}

	if (!RSPEC_ACTIVE(scbband->rspec_override)) {
		ASSERT(!(rspec & RSPEC_LDPC_CODING));
		rspec &= ~RSPEC_LDPC_CODING;
		if (wlc->stf->ldpc_tx == ON ||
			(SCBF_LDPC_CAP(scb_flags, scb_flags2) && wlc->stf->ldpc_tx == AUTO)) {
			if (IS_MCS(rspec))
				rspec |= RSPEC_LDPC_CODING;
		}
	}
	return rspec;
}

/* wrapper function to select transmit rate given per-scb state */
void BCMFASTPATH
wlc_scb_ratesel_gettxrate(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 *frameid,
	ratesel_txparams_t *cur_rate, uint16 *flags)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, *frameid);
	if (state == NULL) {
		ASSERT(0);
		return;
	}

	wlc_ratesel_gettxrate(state, frameid, cur_rate, flags);
}

#ifdef WL11N
void
wlc_scb_ratesel_probe_ready(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p frameid = %d\n",
			__FUNCTION__, wrsi, scb, frameid));
		ASSERT(0);
		return;
	}
	wlc_ratesel_probe_ready(state, frameid, is_ampdu, ampdu_txretry);
}

void BCMFASTPATH
wlc_scb_ratesel_upd_rxstats(wlc_ratesel_info_t *wrsi, ratespec_t rx_rspec, uint16 rxstatus2)
{
	wlc_ratesel_upd_rxstats(wrsi->rsi, rx_rspec, rxstatus2);
}
#endif /* WL11N */

/* non-AMPDU txstatus rate update, default to use non-mcs rates only */
void
wlc_scb_ratesel_upd_txstatus_normalack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint16 sfbl, uint16 lfbl, uint8 tx_mcs, uint8 antselid, bool fbr)

{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	if (state == NULL) {
		ASSERT(0);
		return;
	}

	wlc_ratesel_upd_txstatus_normalack(state, txs, sfbl, lfbl, tx_mcs, antselid, fbr);
}

#ifdef WL11N
void
wlc_scb_ratesel_aci_change(wlc_ratesel_info_t *wrsi, bool aci_state)
{
	wlc_ratesel_aci_change(wrsi->rsi, aci_state);
}

/*
 * Return the fallback rate of the specified mcs rate.
 * Ensure that is a mcs rate too.
 */
ratespec_t
wlc_scb_ratesel_getmcsfbr(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid, uint8 mcs)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	ASSERT(state);

	return (wlc_ratesel_getmcsfbr(state, frameid, mcs));
}

#ifdef WLAMPDU_MAC
/*
 * The case that (mrt+fbr) == 0 is handled as RTS transmission failure.
 */
void
wlc_scb_ratesel_upd_txs_ampdu(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint mrt, uint mrt_succ, uint fbr, uint fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	ASSERT(state);

	wlc_ratesel_upd_txs_ampdu(state, frameid, mrt, mrt_succ, fbr, fbr_succ, tx_error,
		tx_mcs, antselid);
}
#endif /* WLAMPDU_MAC */

/* update state upon received BA */
void BCMFASTPATH
wlc_scb_ratesel_upd_txs_blockack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint8 suc_mpdu, uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 mcs, uint8 antselid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	ASSERT(state);

	wlc_ratesel_upd_txs_blockack(state, txs, suc_mpdu, tot_mpdu, ba_lost, retry, fb_lim,
		tx_error, mcs, antselid);
}
#endif /* WL11N */

bool
wlc_scb_ratesel_minrate(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	ASSERT(state);

	return (wlc_ratesel_minrate(state, txs));
}

static void
wlc_scb_ratesel_ppr_filter(wlc_info_t *wlc, ppr_rateset_t *clm_rates,
	wlc_rateset_t *scb_rates, bool scb_VHT)
{
	uint8 i;

	for (i = 0; i < PHYCORENUM(wlc->stf->txstreams); i++) {
		scb_rates->mcs[i] &= clm_rates->mcs[i];
#ifdef WL11AC
		if (WLCISACPHY(wlc->band) && scb_VHT) {
			uint8 clm_vht_rate = VHT_MCS_MAP_GET_MCS_PER_SS(i+1, clm_rates->vht_mcsmap);
			/* If VHT8-9 are disabled in clm rates, only enable VHT0-7 */
			if (clm_vht_rate == VHT_CAP_MCS_MAP_0_7)
				VHT_MCS_MAP_SET_MCS_PER_SS(i+1, VHT_CAP_MCS_MAP_0_7,
					scb_rates->vht_mcsmap);
		}
#endif // endif
	}
}

/* initialize per-scb state utilized by rate selection
 *   ATTEN: this fcn can be called to "reinit", avoid dup MALLOC
 *   this new design makes this function the single entry points for any select_rates changes
 *   this function should be called when any its parameters changed: like bw or stream
 *   this function will build select_rspec[] with all constraint and rateselection will
 *      be operating on this constant array with reference to known_rspec[] for threshold
 */

void
wlc_scb_ratesel_init(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	rcb_t *state;
	uint8 bw = BW_20MHZ;
	int8 sgi_tx = OFF;
	int8 ldpc_tx = OFF;
	int8 vht_ldpc_tx = OFF;
	uint8 active_antcfg_num = 0;
	uint8 antselid_init = 0;
	int32 ac;
	uint *txc_ptr = NULL;
	wlc_rateset_t new_rateset;
	ppr_rateset_t *clm_rateset;
	chanspec_t chanspec;
	uint8 i;

	/* Use configured chanspec rather than the instantaneous phy chanspec for WDS links */
	if (SCB_LEGACY_WDS(scb)) {
		chanspec = wlc->home_chanspec;
	} else {
		chanspec = wlc->chanspec;
	}

	if (SCB_INTERNAL(scb))
		return;
#ifdef WL11N
	if (WLANTSEL_ENAB(wlc))
		wlc_antsel_ratesel(wlc->asi, &active_antcfg_num, &antselid_init);

#ifdef WL11AC
	if (CHSPEC_IS80(chanspec) && SCB_VHT_CAP(scb))
		bw = BW_80MHZ;
	else
#endif /* WL11AC */
	if (((scb->flags & SCB_IS40) ? TRUE : FALSE) &&
	    (CHSPEC_IS40(chanspec) || CHSPEC_IS80(chanspec)))
		bw = BW_40MHZ;

	/* here bw derived from chanspec and capabilities */

#ifdef WL11AC
	/* process operating mode notification for channel bw */

	if ((SCB_HT_CAP(scb) || SCB_VHT_CAP(scb)) &&
		wlc_vht_get_scb_opermode_enab(wlc->vhti, scb) &&
		!DOT11_OPER_MODE_RXNSS_TYPE(wlc_vht_get_scb_opermode(wlc->vhti, scb))) {
		if (DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(wlc_vht_get_scb_opermode(wlc->vhti, scb)))
			bw = BW_20MHZ;
		else if (DOT11_OPER_MODE_CHANNEL_WIDTH_40MHZ(
			wlc_vht_get_scb_opermode(wlc->vhti, scb)) && bw == BW_80MHZ)
			bw = BW_40MHZ;

		/* here if bw == 40 && oper_mode_bw != 20 -> bw = 40
				if bw == 80 && oper_mode_bw != 20 && oper_mode_bw != 40 -> bw = 80
				if bw == 20 -> bw = 20
		*/
	}

#endif /* WL11AC */

	if (wlc->stf->ldpc_tx == AUTO) {
		if (bw != BW_80MHZ && SCB_LDPC_CAP(scb))
			ldpc_tx = AUTO;
#ifdef WL11AC
		if (SCB_VHT_LDPC_CAP(wlc->vhti, scb))
			vht_ldpc_tx = AUTO;
#endif /* WL11AC */
	} else if (wlc->stf->ldpc_tx == ON) {
		if (SCB_LDPC_CAP(scb))
			ldpc_tx = ON;
		if (SCB_VHT_LDPC_CAP(wlc->vhti, scb))
			vht_ldpc_tx = ON;
	}

	if (wlc->sgi_tx == AUTO) {
		if ((bw == BW_40MHZ && (scb->flags2 & SCB2_SGI40_CAP)) ||
		    (bw == BW_20MHZ && (scb->flags2 & SCB2_SGI20_CAP)) ||
		    (bw == BW_80MHZ && SCB_VHT_SGI80(wlc->vhti, scb)))
			sgi_tx = AUTO;

		/* Disable SGI Tx in 20MHz on IPA chips */
		if (bw == BW_20MHZ && wlc->stf->ipaon)
			sgi_tx = OFF;
	}
#endif /* WL11N */

	if (WLC_TXC_ENAB(wlc))
		txc_ptr = wlc_txc_inv_ptr(wlc->txc, scb);

#ifdef WL11AC
	/* Set up the mcsmap in scb->rateset.vht_mcsmap */
	if (SCB_VHT_CAP(scb))
	{
		if (wlc->stf->txstream_value == 0) {
			wlc_vht_upd_rate_mcsmap(wlc->vhti, scb);
		}
		else  {
			/* vht rate override
			* for 3 stream the value 0x 11 11 11 11 1110 10 10
			* for 2 stream the value 0x 11 11 11 11 11 11 1010
			* for 1 stream the value 0x 11 11 11 11 11 11 1110
			*/
			if (wlc->stf->txstream_value == 2) {
				scb->rateset.vht_mcsmap = 0xfffa;
			}
			else if (wlc->stf->txstream_value == 1) {
				scb->rateset.vht_mcsmap = 0xfffe;
			}
		}
	}
#endif /* WL11AC */

	/* HT rate overide for BTCOEX */
	if ((SCB_HT_CAP(scb) && wlc->stf->txstream_value)) {
		for (i = 1; i < 4; i++) {
			if (i >= wlc->stf->txstream_value) {
				scb->rateset.mcs[i] = 0;
			}
		}
#if defined(WLPROPRIETARY_11N_RATES)
		for (i = WLC_11N_FIRST_PROP_MCS; i <= WLC_MAXMCS; i++) {
			if (GET_PROPRIETARY_11N_MCS_NSS(i) > wlc->stf->txstream_value)
				clrbit(scb->rateset.mcs, i);
		}
#endif /* WLPROPRIETARY_11N_RATES */
	}

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		uint8 vht_ratemask = 0;
		uint32 max_rate;
		state = SCB_RATESEL_CUBBY(wrsi, scb, ac);

		if (state == NULL) {
			ASSERT(0);
			return;
		}

		bcopy(&scb->rateset, &new_rateset, sizeof(wlc_rateset_t));

#ifdef WL11N
		if (BSS_N_ENAB(wlc, scb->bsscfg)) {
			if (((scb->ht_mimops_enabled && !scb->ht_mimops_rtsmode) ||
				(wlc->stf->txstreams == 1) || (wlc->stf->siso_tx == 1))) {
				new_rateset.mcs[1] = 0;
				new_rateset.mcs[2] = 0;
			} else if (wlc->stf->txstreams == 2)
				new_rateset.mcs[2] = 0;
		}
#endif // endif

#ifdef WL11AC
		vht_ratemask = wlc_vht_get_scb_ratemask_per_band(wlc->vhti, scb);
#endif // endif
		WL_RATE(("%s: scb 0x%p ac %d state 0x%p bw %s txstreams %d"
			" active_ant %d band %d vht:%u vht_rm:0x%x\n",
			__FUNCTION__, scb, ac, state, (bw == BW_20MHZ) ?
			"20" : ((bw == BW_40MHZ) ? "40" : "80"),
			wlc->stf->txstreams, active_antcfg_num,
			wlc->band->bandtype, SCB_VHT_CAP(scb), vht_ratemask));

		max_rate = 0;
#if defined(WME_PER_AC_TX_PARAMS)
		if (WME_PER_AC_MAXRATE_ENAB(wrsi->pub) && SCB_WME(scb))
			max_rate = (uint32)wrsi->wlc->wme_max_rate[ac];
#endif // endif
		if (WLCISACPHY(wlc->band)) {
			/* WL_TX_BW_XX starts from 0 */
			clm_rateset = wlc_scb_ratesel_get_ppr_rates(wlc, bw-BW_20MHZ);
			if (clm_rateset)
				wlc_scb_ratesel_ppr_filter(wlc, clm_rateset, &new_rateset,
					SCB_VHT_CAP(scb));
		}
		wlc_ratesel_init(wrsi->rsi, state, scb, txc_ptr, &new_rateset, bw, sgi_tx,
			ldpc_tx, vht_ldpc_tx, vht_ratemask, active_antcfg_num, antselid_init,
			max_rate, 0);
	}

#ifdef WL_LPC
	wlc_scb_lpc_init(wlc->wlpci, scb);
#endif // endif
	scb->link_bw = bw;
}

void
wlc_scb_ratesel_init_all(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb)
		wlc_scb_ratesel_init(wlc, scb);

#ifdef WL_LPC
	wlc_scb_lpc_init_all(wlc->wlpci);
#endif // endif
}

void
wlc_scb_ratesel_init_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		wlc_scb_ratesel_init(wlc, scb);
	}
#ifdef WL_LPC
	wlc_scb_lpc_init_bss(wlc->wlpci, cfg);
#endif // endif
}

void
wlc_scb_ratesel_rfbr(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	ASSERT(state);

	wlc_ratesel_rfbr(state);
}

void
wlc_scb_ratesel_rfbr_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	rcb_t *state;
	int32 ac;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
			state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
			ASSERT(state);
			wlc_ratesel_rfbr(state);
		}
	}
}

static int wlc_scb_ratesel_cubby_sz(void)
{
	return (sizeof(struct ratesel_cubby));
}

#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	scb->rssi_enabled++;
}

void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	scb->rssi_enabled--;
}

int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	if (BSSCFG_STA(scb->bsscfg))
		return scb->bsscfg->link->rssi;
#if defined(AP) || defined(WLDPT) || defined(WLTDLS) || defined(WLAWDL)
	if (scb->rssi_enabled < 0)
		WL_ERROR(("%s: scb %p rssi_enabled %d\n",
			__FUNCTION__, scb, scb->rssi_enabled));
	ASSERT(scb->rssi_enabled >= 0);
	return wlc_scb_rssi(scb);
#endif // endif
	return WLC_RSSI_INVALID;
}
#endif /* WL11N */

#ifdef WL_LPC
/* External functions */
void
wlc_scb_ratesel_get_info(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint8 rate_stab_thresh, uint32 *new_rate_kbps, bool *rate_stable,
	rate_lcb_info_t *lcb_info)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	wlc_ratesel_get_info(state, rate_stab_thresh, new_rate_kbps, rate_stable, lcb_info);
	return;
}

void
wlc_scb_ratesel_reset_vals(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state = NULL;

	if (!scb)
		return;

	state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
	wlc_ratesel_lpc_init(state);
	return;
}

void
wlc_scb_ratesel_clr_cache(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	wlc_ratesel_clr_cache(state);
	return;
}
#endif /* WL_LPC */

/* Get current CLM enabled rates bitmap */
static ppr_rateset_t *
wlc_scb_ratesel_get_ppr_rates(wlc_info_t *wlc, wl_tx_bw_t bw)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	if (wrsi->ppr_rates->chanspec != wlc->chanspec ||
		wrsi->ppr_rates->country != wlc_get_country(wlc)) {
		wlc_scb_ratesel_ppr_upd(wlc);
	}

	switch (bw) {
	case WL_TX_BW_20:
		return &wrsi->ppr_rates->ppr_20_rates;
#if defined(WL11AC) || defined(WL11N)
	case WL_TX_BW_40:
		return &wrsi->ppr_rates->ppr_40_rates;
#endif // endif
#ifdef WL11AC
	case WL_TX_BW_80:
		return &wrsi->ppr_rates->ppr_80_rates;
#endif // endif
	default:
		ASSERT(0);
		return NULL;
	}
}

static void
wlc_scb_ratesel_get_ppr_rates_bitmp(wlc_info_t *wlc, ppr_t *target_pwrs, wl_tx_bw_t bw,
	ppr_rateset_t *rates)
{
	uint8 chain;
	ppr_vht_mcs_rateset_t mcs_limits;

	for (chain = 0; chain < PHYCORENUM(wlc->stf->txstreams); chain++) {
		ppr_get_vht_mcs(target_pwrs, bw, chain+1, WL_TX_MODE_NONE, chain+1, &mcs_limits);
		if (mcs_limits.pwr[0] != WL_RATE_DISABLED) {
			rates->mcs[chain] = 0xff; /* Rates enabled for this block */
			/* Check VHT rate [8-9] */
#ifdef WL11AC
			if (WLCISACPHY(wlc->band)) {
				if (mcs_limits.pwr[8] != WL_RATE_DISABLED) {
					/* All VHT rates are enabled */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1, VHT_CAP_MCS_MAP_0_9,
						rates->vht_mcsmap);
				} else {
					/* VHT 8-9 are disabled in this case */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1, VHT_CAP_MCS_MAP_0_7,
						rates->vht_mcsmap);
				}
			}
#endif // endif
		}
	}

}

static void
wlc_scb_ratesel_ppr_updbmp(wlc_info_t *wlc, ppr_t *target_pwrs)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_20,
		&wrsi->ppr_rates->ppr_20_rates);
#if defined(WL11N) || defined(WL11AC)
	wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_40,
		&wrsi->ppr_rates->ppr_40_rates);
#endif // endif
#if defined(WL11AC)
	wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_80,
		&wrsi->ppr_rates->ppr_80_rates);
#endif // endif
}

/* Update ppr enabled rates bitmap */
extern void
wlc_scb_ratesel_ppr_upd(wlc_info_t *wlc)
{
	phy_tx_power_t power;
	wl_tx_bw_t ppr_bw;
	ppr_t* reg_limits = NULL;

	wlc_cm_info_t *wlc_cm = wlc->cmi;
	clm_country_t country = wlc_get_country(wlc);

	bzero(&power, sizeof(power));

	ppr_bw = ppr_get_max_bw();
	if ((power.ppr_target_powers = ppr_create(wlc->osh, ppr_bw)) == NULL) {
		goto free_power;
	}
	if ((power.ppr_board_limits = ppr_create(wlc->osh, ppr_bw)) == NULL) {
		goto free_power;
	}
	if ((reg_limits = ppr_create(wlc->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		goto free_power;
	}
	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, reg_limits);

	wlc_phy_txpower_get_current(wlc->band->pi, reg_limits, &power);

	/* update the rate bitmap with retrieved target power */
	wlc_scb_ratesel_ppr_updbmp(wlc, power.ppr_target_powers);

	wlc->wrsi->ppr_rates->country = country;
	wlc->wrsi->ppr_rates->chanspec = wlc->chanspec;
free_power:
	if (power.ppr_board_limits)
		ppr_delete(wlc->osh, power.ppr_board_limits);
	if (power.ppr_target_powers)
		ppr_delete(wlc->osh, power.ppr_target_powers);
	if (reg_limits)
		ppr_delete(wlc->osh, reg_limits);
}

/* following functions implement the process that when SCB */
/* migration from dongle to host or host to dongle */
/* memory of rate_cubby have been allocated in the target space */
/* and content of the original rate_cubby is copied to target */
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
int rate_cubby_dongle2host(wlc_info_t *wlc, void *context,
struct scb* scb_dongle, struct scb* scb_host)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	int cubby_size;
	ratesel_cubby_t *cubby_info_dongle = SCB_RATESEL_INFO(wrsi, scb_dongle);
	if (cubby_info_dongle != NULL) {
		rcb_t *scb_rate_cubby_dongle = cubby_info_dongle->scb_cubby;
		if (scb_rate_cubby_dongle != NULL) {
			swap_cubby_ratesel_scb(scb_rate_cubby_dongle, scb_host);
#if defined(WME_PER_AC_TX_PARAMS)
#if defined(WME_PER_AC_MAXRATE_DISABLE)
			cubby_size = wlc_ratesel_rcb_sz();
#else
			cubby_size = AC_COUNT * wrsi->cubby_sz;
#endif // endif
#else
			cubby_size = wrsi->cubby_sz;
#endif /* WME_PER_AC_TX_PARAMS */
			ratesel_cubby_t *cubbyinfo_ratesel = SCB_RATESEL_INFO(wrsi, scb_host);
			rcb_t *scb_rate_cubby_host = NULL;

			scb_rate_cubby_host = (rcb_t *)wlc_scb_alloc_mem_get
				(wrsi->wlc, SCB_CUBBY_ID_RATESEL, cubby_size, 1);
			if (!scb_rate_cubby_host)
				return BCME_NOMEM;
			memcpy(scb_rate_cubby_host, scb_rate_cubby_dongle, cubby_size);

			cubbyinfo_ratesel->scb_cubby = scb_rate_cubby_host;

			MFREE(wlc->osh, scb_rate_cubby_dongle, cubby_size);
			cubby_info_dongle->scb_cubby = NULL;
			return BCME_OK;
		}
	}
	return BCME_ERROR;
}

int rate_cubby_host2dongle(wlc_info_t *wlc, void *context,
struct scb* scb_dongle, struct scb* scb_host)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info_host = SCB_RATESEL_INFO(wrsi, scb_host);

	if (cubby_info_host != NULL) {
		rcb_t *scb_rate_cubby_host = cubby_info_host->scb_cubby;
		if (scb_rate_cubby_host != NULL) {
			swap_cubby_ratesel_scb(scb_rate_cubby_host, scb_dongle);
			ratesel_cubby_t *cubby_info_dongle = SCB_RATESEL_INFO(wrsi, scb_dongle);
			rcb_t *scb_rate_cubby_dongle = NULL;
			int cubby_size;
#if defined(WME_PER_AC_TX_PARAMS)
#if defined(WME_PER_AC_MAXRATE_DISABLE)
			cubby_size = wlc_ratesel_rcb_sz();
#else
			cubby_size = AC_COUNT * wrsi->cubby_sz;
#endif // endif
#else
			cubby_size = wrsi->cubby_sz;
#endif /* WME_PER_AC_TX_PARAMS */
			scb_rate_cubby_dongle = (rcb_t *)MALLOC(wlc->osh, cubby_size);
			if (!scb_rate_cubby_dongle)
				return BCME_NOMEM;

			memcpy(scb_rate_cubby_dongle, scb_rate_cubby_host, cubby_size);
			cubby_info_dongle->scb_cubby = scb_rate_cubby_dongle;

			wlc_scb_alloc_mem_free(wlc, SCB_CUBBY_ID_RATESEL,
				(void *)cubby_info_host->scb_cubby);
			cubby_info_host->scb_cubby = NULL;
			return BCME_OK;
		}
	}
	return BCME_ERROR;
}
#endif /* defined(BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */
