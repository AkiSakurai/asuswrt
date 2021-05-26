/*
 * Public H/W info of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_hw.c 783499 2020-01-30 09:28:27Z $
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
#include <wlioctl.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_dump.h>

/* local functions */
#if defined(BCMDBG)
static int wlc_hw_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

wlc_hw_info_t *
BCMATTACHFN(wlc_hw_attach)(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err)
{
	wlc_hw_info_t *wlc_hw;
	int i;

	if ((wlc_hw = (wlc_hw_info_t *)
	     MALLOCZ(osh, sizeof(wlc_hw_info_t))) == NULL) {
		*err = 1010;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for wlc_hw\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}
	wlc_hw->wlc = wlc;
	wlc_hw->osh = osh;
	wlc_hw->vars_table_accessor[0] = 0;
	wlc_hw->unit = unit;

	if ((wlc_hw->btc = (wlc_hw_btc_info_t*)
	     MALLOCZ(osh, sizeof(wlc_hw_btc_info_t))) == NULL) {
		*err = 1011;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for btc\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}

	if ((wlc_hw->bandstate[0] = (wlc_hwband_t*)
	     MALLOCZ(osh, sizeof(wlc_hwband_t) * MAXBANDS)) == NULL) {
		*err = 1012;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for bandstate\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}

	for (i = 1; i < MAXBANDS; i++) {
		wlc_hw->bandstate[i] = (wlc_hwband_t *)
		        ((uintptr)wlc_hw->bandstate[0] + sizeof(wlc_hwband_t) * i);
	}

	for (i = 0; i < MAXBANDS; i++) {
		if ((wlc_hw->bandstate[i]->mhfs = (uint16 *)
			MALLOCZ(osh, sizeof(uint16)*(MXHF0+MXHFMAX))) == NULL) {
			*err = 1016;
			goto fail;
		}
	}
	if ((wlc_hw->pub = MALLOCZ(osh, sizeof(wlc_hw_t))) == NULL) {
		*err = 1013;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for pub\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}

#ifdef PKTENG_TXREQ_CACHE
	if ((wlc_hw->pkteng_cache = MALLOCZ(osh,
		sizeof(struct wl_pkteng_cache))) == NULL) {
		*err = 1014;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for pub\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}
#endif /* PKTENG_TXREQ_CACHE */

	if ((wlc_hw->pkteng_async = MALLOCZ_PERSIST(osh,
		sizeof(struct wl_pkteng_async))) == NULL) {
		*err = 1014;
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for pub\n",
				unit, __FUNCTION__, MALLOCED(osh)));
		goto fail;
	}

#if defined(BCMDBG)
	if (wlc_dump_register(wlc->pub, "hw", wlc_hw_dump, wlc_hw) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          unit, __FUNCTION__));
		*err = 1014;
		goto fail;
	}
#endif // endif

	if ((wlc_hw->txavail = MALLOCZ(osh, (NFIFO_EXT_MAX * sizeof(*wlc_hw->txavail)))) == NULL) {
		*err = 1015;
		goto fail;
	}

	*err = 0;

	/* XXX do we have a better place for the assignment?
	 * maybe at the caller but that requires an API to return wlc_hw->pub...
	 */
	wlc->hw_pub = wlc_hw->pub;
	return wlc_hw;

fail:
	MODULE_DETACH(wlc_hw, wlc_hw_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_hw_detach)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh;
	wlc_info_t *wlc;
	int i;

	if (wlc_hw == NULL)
		return;

	osh = wlc_hw->osh;
	wlc = wlc_hw->wlc;

	wlc->hw_pub = NULL;

	if (wlc_hw->btc != NULL) {
		MFREE(osh, wlc_hw->btc, sizeof(wlc_hw_btc_info_t));
	}

	if (wlc_hw->bandstate[0] != NULL) {
		for (i = 0; i < MAXBANDS; i++) {
			if (wlc_hw->bandstate[i]->mhfs != NULL)
				MFREE(osh, wlc_hw->bandstate[i]->mhfs,
					sizeof(uint16)*(MXHF0+MXHFMAX));
		}
		MFREE(osh, wlc_hw->bandstate[0], sizeof(wlc_hwband_t) * MAXBANDS);
	}

	if (wlc_hw->txavail != NULL)
		MFREE(osh, wlc_hw->txavail, NFIFO_EXT_MAX * sizeof(*wlc_hw->txavail));

	if (wlc_hw->pkteng_async != NULL)
		MFREE(osh, wlc_hw->pkteng_async, sizeof(struct wl_pkteng_async));

	/* free hw struct */
	if (wlc_hw->pub != NULL)
		MFREE(osh, wlc_hw->pub, sizeof(wlc_hw_t));

	/* free hw struct */
	MFREE(osh, wlc_hw, sizeof(wlc_hw_info_t));
}

void
wlc_hw_update_nfifo(wlc_hw_info_t *wlc_hw)
{
	ASSERT(wlc_hw->corerev != 0);

	/* Determine the number of TX FIFOs supported by the hardware */
	wlc_hw->pub->nfifo_total = (D11REV_IS(wlc_hw->corerev, 132) ? NFIFO_EXT_REV132 :
		/* */		   (D11REV_IS(wlc_hw->corerev, 131) ? NFIFO_EXT_REV131 :
		/* */		   (D11REV_IS(wlc_hw->corerev, 130) ? NFIFO_EXT_REV130 :
		/* */		   (D11REV_GE(wlc_hw->corerev, 128) ? NFIFO_EXT_REV128 :
		/* */		   (D11REV_GE(wlc_hw->corerev, 64)  ? NFIFO_EXT_REV64 :
		/* */						      NFIFO_LEGACY)))));
	ASSERT(wlc_hw->pub->nfifo_total <= NFIFO_EXT_MAX);

	/* Determine the number of TX FIFOs to use, depending on settings */
	wlc_hw->pub->nfifo_inuse = NFIFO_LEGACY;

	if (D11REV_GE(wlc_hw->corerev, 128) && (HE_DLMU_ENAB(wlc_hw->wlc->pub) ||
	   (HE_DLMMU_ENAB(wlc_hw->wlc->pub)))) {
		/* 11ax MU-MIMO/OFDMA */
		wlc_hw->pub->nfifo_inuse = wlc_hw->pub->nfifo_total;

		ASSERT(TX_FIFO_SU_OFFSET(wlc_hw->corerev) == 0 ||
			TX_FIFO_SU_OFFSET(wlc_hw->corerev) ==
			wlc_hw->pub->nfifo_inuse - NFIFO_LEGACY);
	} else if (D11REV_GE(wlc_hw->corerev, 64) && MU_TX_ENAB(wlc_hw->wlc)) {
		/* 11ac MU-MIMO */
		wlc_hw->pub->nfifo_inuse = wlc_hw->pub->nfifo_total;
	}

	if (PIO_ENAB_HW(wlc_hw)) {
		/* PIO mode, only use legacy queues */
		wlc_hw->pub->nfifo_inuse = NFIFO_LEGACY;
	}

	WL_INFORM(("wl%d: d11 rev %u, using %u of %u TX FIFOs, SU offset %u\n",
		wlc_hw->wlc->pub->unit,
		wlc_hw->corerev,
		wlc_hw->pub->nfifo_inuse, wlc_hw->pub->nfifo_total,
		TX_FIFO_SU_OFFSET(wlc_hw->corerev)));

	ASSERT(wlc_hw->pub->nfifo_inuse <= wlc_hw->pub->nfifo_total);
}

int
wlc_hw_verify_fifo_layout(wlc_hw_info_t *wlc_hw)
{
	if (D11REV_GE(wlc_hw->corerev, 128)) {
		/* Verify configured SU FIFOs offset matches ucode */
		uint features, su_offset;

		ASSERT(wlc_hw->clk);

		features = (uint)wlc_read_shm(wlc_hw->wlc, M_UCODE_FEATURES(wlc_hw->wlc));
		WL_INFORM(("%s: ucode features %x\n", __FUNCTION__, features));

		su_offset = ((features & UCFEAT_SUBANK_MASK) >> UCFEAT_SUBANK_SHIFT) * 16;
		if (su_offset != TX_FIFO_SU_OFFSET(wlc_hw->corerev)) {
			WL_ERROR(("wl%d: TX FIFO layout mismatch (%u/%u)\n",
				wlc_hw->wlc->pub->unit,
				su_offset, TX_FIFO_SU_OFFSET(wlc_hw->corerev)));
			ASSERT(FALSE);
			return BCME_ERROR;
		}
	}

	return BCME_OK;
}

uint
wlc_hw_map_txfifo(wlc_hw_info_t *wlc_hw, uint fifo)
{
	uint physical_fifo;
	uint su_offset = TX_FIFO_SU_OFFSET(wlc_hw->corerev);

	ASSERT(su_offset != 0);
	ASSERT(fifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));

	/* Map logical FIFO index to HW FIFO index */
	physical_fifo = (fifo < NFIFO_LEGACY ?
		fifo + su_offset :
		fifo - NFIFO_LEGACY);

	WL_TRACE(("%s: %u->%u\n", __FUNCTION__, fifo, physical_fifo));
	return physical_fifo;
}

uint
wlc_hw_unmap_txfifo(wlc_hw_info_t *wlc_hw, uint fifo)
{
	uint logical_fifo;
	uint su_offset = TX_FIFO_SU_OFFSET(wlc_hw->corerev);

	ASSERT(su_offset != 0);
	ASSERT(fifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));

	/* Map HW FIFO index to logical FIFO index */
	logical_fifo = (fifo >= su_offset ?
		fifo - su_offset :
		fifo + NFIFO_LEGACY);

	WL_TRACE(("%s: %u->%u\n", __FUNCTION__, fifo, logical_fifo));

	return logical_fifo;
}

void
wlc_hw_set_piomode(wlc_hw_info_t *wlc_hw, bool piomode)
{
	wlc_hw->_piomode = piomode;
	wlc_hw_update_nfifo(wlc_hw);
}

bool
wlc_hw_get_piomode(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->_piomode;
}

void
wlc_hw_set_dmacommon(wlc_hw_info_t *wlc_hw, dma_common_t *dmacommon)
{
	wlc_hw->dmacommon = dmacommon;
}

/* GCC Value Range Propagation issue */
/* Can't use push/pop as it is not supported on older compilers */
#pragma GCC diagnostic ignored "-Warray-bounds"

void
wlc_hw_set_di(wlc_hw_info_t *wlc_hw, uint fifo, hnddma_t *di)
{
	ASSERT(fifo < ARRAYSIZE(wlc_hw->di));
	wlc_hw->di[fifo] = di;

	ASSERT(fifo < ARRAYSIZE(wlc_hw->pub->di));
	wlc_hw->pub->di[fifo] = di;
}

void
wlc_hw_set_aqm_di(wlc_hw_info_t *wlc_hw, uint fifo, hnddma_t *di)
{
#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	ASSERT(fifo < ARRAYSIZE(wlc_hw->aqm_di));
	wlc_hw->aqm_di[fifo] = di;

	ASSERT(fifo < ARRAYSIZE(wlc_hw->pub->aqm_di));
	wlc_hw->pub->aqm_di[fifo] = di;
#endif // endif
}

void
wlc_hw_set_pio(wlc_hw_info_t *wlc_hw, uint fifo, pio_t *pio)
{
	ASSERT(fifo < ARRAYSIZE(wlc_hw->pio));
	wlc_hw->pio[fifo] = pio;

	ASSERT(fifo < ARRAYSIZE(wlc_hw->pub->pio));
	wlc_hw->pub->pio[fifo] = pio;
}

/* GCC Value Range Propagation issue */
#pragma GCC diagnostic warning "-Warray-bounds"

bool
wlc_hw_deviceremoved(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->clk && SRPWR_ENAB()) {
		wlc_srpwr_request_on(wlc_hw->wlc);
	}

	return wlc_hw->clk ?
	        (R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) &
	         (MCTL_PSM_JMP_0 | MCTL_IHR_EN)) != MCTL_IHR_EN :
	        si_deviceremoved(wlc_hw->sih);
}

uint32
wlc_hw_get_wake_override(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->wake_override;
}

uint
wlc_hw_get_bandunit(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->band->bandunit;
}
uint32
wlc_hw_get_macintmask(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->macintmask;
}

uint32
wlc_hw_get_macintstatus(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->macintstatus;
}

/* MHF2_SKIP_ADJTSF muxing, clear the flag when no one requests to skip the ucode
 * TSF adjustment.
 */
void
wlc_skip_adjtsf(wlc_info_t *wlc, bool skip, wlc_bsscfg_t *cfg, uint32 user, int bands)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint global_start = NBITS(wlc_hw->skip_adjtsf) - WLC_SKIP_ADJTSF_USER_MAX;
	int b;

	ASSERT(cfg != NULL || user < WLC_SKIP_ADJTSF_USER_MAX);
	ASSERT(cfg == NULL || (uint32)WLC_BSSCFG_IDX(cfg) < global_start);

	b = (cfg == NULL) ? user + global_start : (uint32)WLC_BSSCFG_IDX(cfg);
	if (skip)
		setbit(&wlc_hw->skip_adjtsf, b);
	else
		clrbit(&wlc_hw->skip_adjtsf, b);

#ifdef BCMDBG
	if (cfg != NULL)
		WL_NONE(("wl%d.%d: wlc->skip_adjtsf 0x%x (skip %d)\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc_hw->skip_adjtsf, skip));
	else
		WL_NONE(("wl%d: wlc->skip_adjtsf 0x%x (user %d skip %d)\n",
		         wlc->pub->unit, wlc_hw->skip_adjtsf, user, skip));
#endif // endif

	wlc_bmac_mhf(wlc_hw, MHF2, MHF2_SKIP_ADJTSF,
	        wlc_hw->skip_adjtsf ? MHF2_SKIP_ADJTSF : 0, bands);
}

/* MCTL_AP muxing, set the bit when no one requests to stop the AP functions (beacon, prbrsp) */
void
wlc_ap_ctrl(wlc_info_t *wlc, bool on, wlc_bsscfg_t *cfg, uint32 user)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;

	BCM_REFERENCE(cfg);
	BCM_REFERENCE(user);

	if (on && wlc_hw->mute_ap != 0) {
		WL_INFORM(("wl%d: ignore %s %d request %d mute_ap 0x%08x\n",
		           wlc->pub->unit, cfg != NULL ? "bsscfg" : "user",
		           cfg != NULL ? WLC_BSSCFG_IDX(cfg) : user, on, wlc_hw->mute_ap));
		return;
	}

	wlc_hw->mute_ap = 0;

	WL_INFORM(("wl%d: %s %d MCTL_AP %d\n",
	           wlc->pub->unit, cfg != NULL ? "bsscfg" : "user",
	           cfg != NULL ? WLC_BSSCFG_IDX(cfg) : user, on));

	wlc_bmac_mctrl(wlc_hw, MCTL_AP, on ? MCTL_AP : 0);
}

#ifdef AP
void
wlc_ap_mute(wlc_info_t *wlc, bool mute, wlc_bsscfg_t *cfg, uint32 user)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint global_start = NBITS(wlc_hw->mute_ap) - WLC_AP_MUTE_USER_MAX;
	int b;
	bool ap;

	ASSERT(cfg != NULL || user < WLC_AP_MUTE_USER_MAX);
	ASSERT(cfg == NULL || (uint32)WLC_BSSCFG_IDX(cfg) < global_start);

	b = (cfg == NULL) ? user + global_start : (uint32)WLC_BSSCFG_IDX(cfg);
	if (mute)
		setbit(&wlc_hw->mute_ap, b);
	else
		clrbit(&wlc_hw->mute_ap, b);

	/* Need to enable beacon TX only if an active AP or IBSS connection is present on this
	 * core
	 */
	ap = (AP_ACTIVE(wlc) ||
#ifdef STA
		wlc_ibss_active(wlc) ||
#endif /* STA */
		FALSE) && (wlc_hw->mute_ap == 0);

	WL_INFORM(("wl%d: %s %d mute %d mute_ap 0x%x AP_ACTIVE() %d MCTL_AP %d\n",
	           wlc->pub->unit, cfg != NULL ? "bsscfg" : "user",
	           cfg != NULL ? WLC_BSSCFG_IDX(cfg) : user,
	           mute, wlc_hw->mute_ap, AP_ACTIVE(wlc), ap));

	wlc_bmac_mctrl(wlc_hw, MCTL_AP, ap ? MCTL_AP : 0);
}
#endif /* AP */

#if defined(BCMDBG)
static int
wlc_hw_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)ctx;

	bcm_bprintf(b, "defmacintmask 0x%08x macintmask 0x%08x\n",
	            wlc_hw->defmacintmask, wlc_hw->macintmask);
	bcm_bprintf(b, "forcefastclk %d wake_override 0x%x\n",
	            wlc_hw->forcefastclk, wlc_hw->wake_override);
	bcm_bprintf(b, "fastpwrup_dly %d\n", wlc_hw->fastpwrup_dly);

	bcm_bprintf(b, "skipadjtsf 0x%08x muteap 0x%08x\n", wlc_hw->skip_adjtsf, wlc_hw->mute_ap);
	bcm_bprintf(b, "p2p: %d\n", wlc_hw->_p2p);

	return BCME_OK;
}
#endif // endif

char*
wlc_hw_get_vars_table(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	return wlc_hw->vars_table_accessor;
}
