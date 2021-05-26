/*
 * Separate alloc/free module for wlc_xxx.c files. Decouples
 * the code that does alloc/free from other code so data
 * structure changes don't affect ROMMED code as much.
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
 * $Id: wlc_alloc.c 787632 2020-06-06 07:49:55Z $
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
#include <802.11.h>
#include <802.11e.h>
#include <wpa.h>
#include <vlan.h>
#include <wlioctl.h>
#if defined(BCMSUP_PSK)
#include <eapol.h>
#endif /* BCMSUP_PSK */
#include <bcmwpa.h>
#include <bcmdevs.h>
#include <hndd11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_alloc.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc_stf.h>
#ifdef BCMFRWDPOOLREORG
#include <hnd_poolreorg.h>
#endif /* BCMFRWDPOOLREORG */
#ifdef BCMPKTPOOL
#include <wlc_event.h>
#endif // endif

static wlc_pub_t *wlc_pub_malloc(wlc_info_t * wlc, osl_t *osh, uint unit, uint devid);
static void wlc_pub_mfree(wlc_info_t * wlc, osl_t *osh, wlc_pub_t *pub);

static void wlc_tunables_init(wlc_tunables_t *tunables, uint devid, uint unit);

static bool wlc_attach_malloc_high(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid);
static bool wlc_attach_malloc_misc(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid);
static void wlc_detach_mfree_high(wlc_info_t *wlc, osl_t *osh);
static void wlc_detach_mfree_misc(wlc_info_t *wlc, osl_t *osh);

#ifndef DNGL_MEM_RESTRICT_RXDMA /* used only for BMAC low driver */
#define DNGL_MEM_RESTRICT_RXDMA 0
#endif // endif

void
BCMATTACHFN(wlc_tunables_init)(wlc_tunables_t *tunables, uint devid, uint unit)
{
	/* tx/rx ring size for DMAs with 512 descriptor ring size max */
	tunables->ntxd = NTXD;
	tunables->nrxd = NRXD;

	/* tx/rx ring size for DMAs with 4096 descriptor ring size max */
	tunables->ntxd_large = NTXD_LARGE;
	tunables->nrxd_large = NRXD_LARGE;
	tunables->ntxd_bcmc = NTXD_BCMC;
	tunables->rxbufsz = PKTBUFSZ;
	tunables->monrxbufsz = MONPKTBUFSZ;
	tunables->nrxbufpost = NRXBUFPOST;
	tunables->maxscb = DEFMAXSCB;
	tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU;
	tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3STREAMS;
	tunables->maxpktcb = MAXPKTCB;
	tunables->maxucodebss = WLC_MAX_UCODE_BSS;
	tunables->maxucodebss4 = WLC_MAX_UCODE_BSS4;
	tunables->maxbss = MAXBSS;
	tunables->maxubss = MAXUSCANBSS;
	tunables->datahiwat = WLC_DATAHIWAT;
	tunables->ampdudatahiwat = WLC_AMPDUDATAHIWAT;
	tunables->rxbnd = RXBND;
	tunables->txsbnd = TXSBND;
	tunables->pktcbnd = PKTCBND;
	tunables->txmr = TXMR;
	tunables->txpft = TXPREFTHRESH;
	tunables->txpfc = TXPREFCTL;
	tunables->txblen = TXBURSTLEN;
	tunables->rxpft = RXPREFTHRESH;
	tunables->rxpfc = RXPREFCTL;
	tunables->rxblen = RXBURSTLEN;
	tunables->mrrs = MRRS;
#ifdef DONGLEBUILD
	tunables->pkt_maxsegs = 1;
#else /* DONGLEBUILD */
	tunables->pkt_maxsegs = MAX_DMA_SEGS;
#endif /* DONGLEBUILD */
	tunables->maxscbcubbies = MAXSCBCUBBIES;
	tunables->maxbsscfgcubbies = MAXBSSCFGCUBBIES;

	tunables->max_notif_clients = MAX_NOTIF_CLIENTS;
	tunables->max_notif_servers = MAX_NOTIF_SERVERS;
	tunables->max_mempools = MAX_MEMPOOLS;
	tunables->amsdu_resize_buflen = PKTBUFSZ/4;
	tunables->ampdu_pktq_size = AMPDU_PKTQ_LEN;
	tunables->maxpcbcds = MAXPCBCDS;

#ifdef PROP_TXSTATUS
	tunables->wlfcfifocreditac0 = WLFCFIFOCREDITAC0;
	tunables->wlfcfifocreditac1 = WLFCFIFOCREDITAC1;
	tunables->wlfcfifocreditac2 = WLFCFIFOCREDITAC2;
	tunables->wlfcfifocreditac3 = WLFCFIFOCREDITAC3;
	tunables->wlfcfifocreditbcmc = WLFCFIFOCREDITBCMC;
	tunables->wlfcfifocreditother = WLFCFIFOCREDITOTHER;
	tunables->wlfc_fifo_cr_pending_thresh_ac_bk = WLFC_FIFO_CR_PENDING_THRESH_AC_BK;
	tunables->wlfc_fifo_cr_pending_thresh_ac_be = WLFC_FIFO_CR_PENDING_THRESH_AC_BE;
	tunables->wlfc_fifo_cr_pending_thresh_ac_vi = WLFC_FIFO_CR_PENDING_THRESH_AC_VI;
	tunables->wlfc_fifo_cr_pending_thresh_ac_vo = WLFC_FIFO_CR_PENDING_THRESH_AC_VO;
	tunables->wlfc_fifo_cr_pending_thresh_bcmc = WLFC_FIFO_CR_PENDING_THRESH_BCMC;
	tunables->wlfc_trigger = WLFC_INDICATION_TRIGGER;
	tunables->wlfc_fifo_bo_cr_ratio = WLFC_FIFO_BO_CR_RATIO;
	tunables->wlfc_comp_txstatus_thresh = WLFC_COMP_TXSTATUS_THRESH;
#endif /* PROP_TXSTATUS */

	/* set 4360 specific tunables */
	if (IS_AC2_DEV(devid) ||
		IS_DEV_AC3X3(devid) || IS_DEV_AC2X2(devid) ||
		IS_HE_DEV(devid)) {
		tunables->ntxd = NTXD_AC3X3;
		tunables->nrxd = NRXD_AC3X3;
		tunables->rxbnd = RXBND_AC3X3;
		tunables->nrxbufpost = NRXBUFPOST_AC3X3;
		tunables->pktcbnd = PKTCBND_AC3X3;

		/* tx/rx ring size for DMAs with 4096 descriptor ring size max */
		tunables->ntxd_large = NTXD_LARGE_AC3X3;
		tunables->nrxd_large = NRXD_LARGE_AC3X3;
	}
	if (IS_AC2_DEV(devid)) {
		tunables->txmr = TXMR_AC2;
		tunables->txpft = TXPREFTHRESH_AC2;
		tunables->txpfc = TXPREFCTL_AC2;
		tunables->txblen = TXBURSTLEN_AC2;
		tunables->txblen = TXBURSTLEN_AC2;
		tunables->rxpft = RXPREFTHRESH_AC2;
		tunables->rxpfc = RXPREFCTL_AC2;
		tunables->rxblen = RXBURSTLEN_AC2;
		tunables->mrrs = MRRS_AC2;
	}

	if (IS_HE_DEV(devid)) {
		tunables->txmr = TXMR_HE;
		tunables->txpfc = TXPREFCTL_HE;
		tunables->txblen = TXBURSTLEN_HE;
		tunables->rxblen = RXBURSTLEN_HE;
		tunables->mrrs = MRRS_HE;
	}

	/* Key management */
	tunables->max_keys = MAXSCB +
		WLC_MAXBSSCFG*(WLC_KEYMGMT_NUM_GROUP_KEYS + WLC_KEYMGMT_NUM_BSS_IGTK);

	/* IE mgmt */
	tunables->max_ie_build_cbs = MAXIEBUILDCBS;
	tunables->max_vs_ie_build_cbs = MAXVSIEBUILDCBS;
	tunables->max_ie_parse_cbs = MAXIEPARSECBS;
	tunables->max_vs_ie_parse_cbs = MAXVSIEPARSECBS;
	tunables->max_ie_regs = MAXIEREGS;

	tunables->num_rxivs = WLC_KEY_NUM_RX_SEQ;

	tunables->maxbestn = BESTN_MAX;
	tunables->maxmscan = MSCAN_MAX;
	tunables->nrxbufpost_fifo1 = NRXBUFPOST_FIFO1;
	tunables->ntxd_lfrag =	NTXD_LFRAG;
	tunables->nrxd_fifo1 =	NRXD_FIFO1;
	tunables->maxroamthresh = MAX_ROAM_TIME_THRESH;
	tunables->copycount = COPY_CNT_BYTES;
	tunables->nrxd_classified_fifo =	NRXD_CLASSIFIED_FIFO;
	tunables->bufpost_classified_fifo = NRXBUFPOST_CLASSIFIED_FIFO;

	tunables->scan_settle_time = WLC_DEFAULT_SETTLE_TIME;
	tunables->min_scballoc_mem = MIN_SCBALLOC_MEM;
#ifdef BCMRESVFRAGPOOL
	tunables->rpool_disabled_ampdu_mpdu = RPOOL_DISABLED_AMPDU_MPDU;
#endif /* BCMRESVFRAGPOOL */
	tunables->amsdu_rxpost_threshold = WLC_AMSDU_RXPOST_THRESH;
	tunables->max_wait_for_ctxt_delete = WLC_MAX_WAIT_CTXT_DELETE;
#if defined(BCMPKTPOOL)
	tunables->evpool_maxdata = EVPOOL_MAXDATA;
	tunables->evpool_size = EVPOOL_SIZE;
#endif /* BCMPKTPOOL */
	tunables->max_assoc_scan_results = WLC_MAX_ASSOC_SCAN_RESULTS;
#ifdef WLSCANCACHE
	tunables->max_scancache_results = WLC_MAX_SCANCACHE_RESULTS;
#endif /* WLSCANCACHE */

	tunables->nrxd_sts = NRXD_STS;
	tunables->bufpost_sts = NRXBUFPOST_STS;
	tunables->rxbufsz_sts = RXBUFSZ_STS;
	tunables->max_maclist = MAXMACLIST;
}

static wlc_pub_t *
BCMATTACHFN(wlc_pub_malloc)(wlc_info_t * wlc, osl_t *osh, uint unit, uint devid)
{
	wlc_pub_t *pub;
	BCM_REFERENCE(unit);

	if ((pub = (wlc_pub_t*) MALLOCZ(osh, sizeof(wlc_pub_t))) == NULL) {
		goto fail;
	}

	if ((pub->cmn = (wlc_pub_cmn_t*) MALLOCZ(osh, sizeof(wlc_pub_cmn_t))) == NULL) {
		goto fail;
	}

	if ((pub->tunables = (wlc_tunables_t *)
	     MALLOCZ(osh, sizeof(wlc_tunables_t))) == NULL) {
		goto fail;
	}

	/* need to init the tunables now */
	wlc_tunables_init(pub->tunables, devid, unit);

#ifdef WLCNT
	if ((pub->_cnt = (wl_cnt_wlc_t *)MALLOCZ(osh, sizeof(wl_cnt_wlc_t))) == NULL) {
		goto fail;
	}

	if ((pub->reinitrsn = MALLOCZ(osh, sizeof(reinit_rsns_t))) == NULL) {
		goto fail;
	}

	if ((pub->_mcst_cnt = MALLOCZ(osh, WL_CNT_MCST_STRUCT_SZ)) == NULL) {
		goto fail;
	}

#if defined(WL_PSMX)
	if ((pub->_mcxst_cnt = MALLOCZ(osh, WL_CNT_MCXST_STRUCT_SZ)) == NULL) {
		goto fail;
	}
#endif /* WL_PSMX */

	if ((pub->_ap_cnt = (wl_ap_cnt_t*)
		MALLOCZ(osh, sizeof(wl_ap_cnt_t))) == NULL) {
		goto fail;
	}
#endif /* WLCNT */
	if ((pub->_rxfifo_cnt = (wl_rxfifo_cnt_t*)
		 MALLOCZ(osh, sizeof(wl_rxfifo_cnt_t))) == NULL) {
		goto fail;
	}

	return pub;

fail:
	wlc_pub_mfree(wlc, osh, pub);
	return NULL;
}

static void
BCMATTACHFN(wlc_pub_mfree)(wlc_info_t * wlc, osl_t *osh, wlc_pub_t *pub)
{
	BCM_REFERENCE(wlc);
	if (pub == NULL)
		return;

	if (pub->tunables) {
		MFREE(osh, pub->tunables, sizeof(wlc_tunables_t));
		pub->tunables = NULL;
	}

#ifdef WLCNT
	if (pub->_cnt) {
		MFREE(osh, pub->_cnt, sizeof(wl_cnt_wlc_t));
		pub->_cnt = NULL;
	}

	if (pub->reinitrsn) {
		MFREE(osh, pub->reinitrsn, (sizeof(reinit_rsns_t)));
		pub->reinitrsn = NULL;
	}

	if (pub->_mcst_cnt) {
		MFREE(osh, pub->_mcst_cnt, WL_CNT_MCST_STRUCT_SZ);
		pub->_mcst_cnt = NULL;
	}

#if defined(WL_PSMX)
	if (pub->_mcxst_cnt) {
		MFREE(osh, pub->_mcxst_cnt, WL_CNT_MCXST_STRUCT_SZ);
		pub->_mcxst_cnt = NULL;
	}
#endif /* WL_PSMX */
#endif /* WLCNT */
	if (pub->_rxfifo_cnt) {
		MFREE(osh, pub->_rxfifo_cnt, sizeof(wl_rxfifo_cnt_t));
		pub->_rxfifo_cnt = NULL;
	}

	if (pub->_ap_cnt) {
		MFREE(osh, pub->_ap_cnt, sizeof(wl_ap_cnt_t));
		pub->_ap_cnt = NULL;
	}

	if (pub->cmn) {
		MFREE(osh, pub->cmn, sizeof(wlc_pub_cmn_t));
	}

	MFREE(osh, pub, sizeof(wlc_pub_t));
}

static bool
BCMATTACHFN(wlc_attach_malloc_high)(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid)
{
	int i;

	BCM_REFERENCE(unit);
	BCM_REFERENCE(devid);

	if ((wlc->cmn =  (wlc_cmn_info_t*) MALLOCZ(osh, sizeof(wlc_cmn_info_t))) == NULL) {
		*err = 1035;
		goto fail;
	}

	if ((wlc->cmn->lifetime_mg = (wl_lifetime_mg_t*) MALLOCZ(osh,
		sizeof(wl_lifetime_mg_t))) == NULL) {
		*err = 1050;
		goto fail;
	}

	if ((wlc->bandstate[0] = (wlcband_t*)
		MALLOCZ(osh, (sizeof(wlcband_t) * MAXBANDS))) == NULL) {
		*err = 1010;
		goto fail;
	}

	for (i = 1; i < MAXBANDS; i++) {
		wlc->bandstate[i] =
		        (wlcband_t *)((uintptr)wlc->bandstate[0] + (sizeof(wlcband_t) * i));
	}

	if ((wlc->bandinst = (wlcband_inst_t **)
		MALLOCZ(osh, sizeof(wlcband_inst_t *) * MAXBANDS)) == NULL) {
		*err = 1016;
		goto fail;
	}

	for (i = 0; i < MAXBANDS; i++) {
		if ((wlc->bandinst[i] = (wlcband_inst_t *)
			MALLOCZ(osh, sizeof(wlcband_inst_t))) == NULL) {
			*err = 1018;
			goto fail;
		}
	}

	if ((wlc->modulecb = (modulecb_t*) MALLOCZ(osh,
		sizeof(modulecb_t) * wlc->pub->max_modules)) == NULL) {
		*err = 1012;
		goto fail;
	}

	if ((wlc->modulecb_data = (modulecb_data_t*)
	     MALLOCZ(osh, sizeof(modulecb_data_t) * wlc->pub->max_modules)) == NULL) {
		*err = 1013;
		goto fail;
	}

	if ((wlc->bsscfg = (wlc_bsscfg_t**) MALLOCZ(osh,
		sizeof(wlc_bsscfg_t*) * WLC_MAXBSSCFG)) == NULL) {
		*err = 1016;
		goto fail;
	}

	if ((wlc->default_bss = (wlc_bss_info_t*)
	     MALLOCZ(osh, sizeof(wlc_bss_info_t))) == NULL) {
		*err = 1010;
		goto fail;
	}

	if ((wlc->stf = (wlc_stf_info_t*)
	     MALLOCZ(osh, sizeof(wlc_stf_info_t))) == NULL) {
		*err = 1017;
		goto fail;
	}

	if ((wlc->corestate->macstat_snapshot = (uint16*)
	     MALLOCZ(osh, sizeof(uint16) * MACSTAT_OFFSET_SZ)) == NULL) {
		*err = 1027;
		goto fail;
	}

#if defined(WL_PSMX)
	if ((wlc->corestate->macxstat_snapshot = (uint16*)
	     MALLOCZ(osh, sizeof(uint16) * MACXSTAT_OFFSET_SZ)) == NULL) {
		*err = 1034;
		goto fail;
	}
#endif /* WL_PSMX */

#if defined(DELTASTATS) && !defined(DELTASTATS_DISABLED)
	if ((wlc->delta_stats = (delta_stats_info_t*)
	     MALLOCZ(osh, sizeof(delta_stats_info_t))) == NULL) {
		*err = 1023;
		goto fail;
	}
	wlc->pub->_deltastats = TRUE;
#endif /* DELTASTATS */

#ifdef WLROAMPROF
	if (MAXBANDS > 0) {
		if ((wlc->bandstate[0]->roam_prof = (wl_roam_prof_t *)
		     MALLOCZ(osh, sizeof(wl_roam_prof_t) *
		     WL_MAX_ROAM_PROF_BRACKETS * MAXBANDS)) == NULL) {
			*err = 1032;
			goto fail;
		}

		for (i = 1; i < MAXBANDS; i++) {
			wlc->bandstate[i]->roam_prof =
				&wlc->bandstate[0]->roam_prof[i * WL_MAX_ROAM_PROF_BRACKETS];
		}
	}
#endif /* WLROAMPROF */
	return TRUE;

fail:
	return FALSE;
}

static bool
BCMATTACHFN(wlc_attach_malloc_misc)(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid)
{
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(osh);
	BCM_REFERENCE(unit);
	BCM_REFERENCE(err);
	BCM_REFERENCE(devid);

	return TRUE;
}

/*
 * The common driver entry routine. Error codes should be unique
 */
wlc_info_t *
BCMATTACHFN(wlc_attach_malloc)(osl_t *osh, uint unit, uint *err, uint devid)
{
	wlc_info_t *wlc;

	if ((wlc = (wlc_info_t*) MALLOCZ(osh, sizeof(wlc_info_t))) == NULL) {
		*err = 1002;
		goto fail;
	}

	/* allocate wlc_pub_t state structure */
	if ((wlc->pub = wlc_pub_malloc(wlc, osh, unit, devid)) == NULL) {
		*err = 1003;
		goto fail;
	}
	wlc->pub->wlc = wlc;

	wlc->pub->max_modules = WLC_MAXMODULES;

#ifdef BCMPKTPOOL
	wlc->pub->pktpool = SHARED_POOL;
#endif /* BCMPKTPOOL */

#ifdef BCMFRAGPOOL
	wlc->pub->pktpool_lfrag = SHARED_FRAG_POOL;
#endif /* BCMFRAGPOOL */

#ifdef BCMRESVFRAGPOOL
	wlc->pub->pktpool_resv_lfrag = RESV_FRAG_POOL;
	wlc->pub->resv_pool_info = RESV_POOL_INFO;
#endif // endif

#ifdef BCMRXFRAGPOOL
	wlc->pub->pktpool_rxlfrag = SHARED_RXFRAG_POOL;
#endif /* BCMRXFRAGPOOL */

#ifdef UTXD_POOL
	wlc->pub->pktpool_utxd = SHARED_UTXD_POOL;
#endif /* BCMRXFRAGPOOL */

#ifdef BCMFRWDPOOLREORG
	wlc->pub->poolreorg_info = FRWD_POOLREORG_INFO;
#endif /* BCMFRWDPOOLREORG */

	if ((wlc->corestate = (wlccore_t*)
	     MALLOCZ(osh, sizeof(wlccore_t))) == NULL) {
		*err = 1011;
		goto fail;
	}

	if (!wlc_attach_malloc_high(wlc, osh, unit, err, devid))
		goto fail;

	if (!wlc_attach_malloc_misc(wlc, osh, unit, err, devid))
		goto fail;

	return wlc;

fail:
	MODULE_DETACH_2(wlc, osh, wlc_detach_mfree);
	return NULL;
}

static void
BCMATTACHFN(wlc_detach_mfree_high)(wlc_info_t *wlc, osl_t *osh)
{
	if (wlc->modulecb) {
		MFREE(osh, wlc->modulecb, sizeof(modulecb_t) * wlc->pub->max_modules);
		wlc->modulecb = NULL;
	}

	if (wlc->modulecb_data) {
		MFREE(osh, wlc->modulecb_data, sizeof(modulecb_data_t) * wlc->pub->max_modules);
		wlc->modulecb_data = NULL;
	}

	if (wlc->bsscfg) {
		MFREE(osh, wlc->bsscfg, sizeof(wlc_bsscfg_t*) * WLC_MAXBSSCFG);
		wlc->bsscfg = NULL;
	}

	if (wlc->default_bss) {
		MFREE(osh, wlc->default_bss, sizeof(wlc_bss_info_t));
		wlc->default_bss = NULL;
	}

	if (wlc->stf) {
		MFREE(osh, wlc->stf, sizeof(wlc_stf_info_t));
		wlc->stf = NULL;
	}

#if defined(DELTASTATS) && !defined(DELTASTATS_DISABLED)
	if (wlc->delta_stats) {
		MFREE(osh, wlc->delta_stats, sizeof(delta_stats_info_t));
		wlc->delta_stats = NULL;
	}
#endif /* DELTASTATS && !defined(DELTASTATS_DISABLED) */

	if (wlc->cmn != NULL) {
		if (wlc->cmn->lifetime_mg) {
			MFREE(osh, wlc->cmn->lifetime_mg, sizeof(wl_lifetime_mg_t));
			wlc->cmn->lifetime_mg = NULL;
		}
		MFREE(osh, wlc->cmn, sizeof(wlc_cmn_info_t));
	}

	if (wlc->bandstate[0]) {
#ifdef WLROAMPROF
		if (wlc->bandstate[0]->roam_prof) {
			MFREE(osh, wlc->bandstate[0]->roam_prof,
				sizeof(wl_roam_prof_t) * MAXBANDS * WL_MAX_ROAM_PROF_BRACKETS);
		}
#endif /* WLROAMPROF */
		MFREE(osh, wlc->bandstate[0], (sizeof(wlcband_t) * MAXBANDS));
	}

	if (wlc->bandinst) {
		int i = 0;
		for (i = 0; i < MAXBANDS; i++) {
			if (wlc->bandinst[i]) {
				MFREE(osh, wlc->bandinst[i], sizeof(wlcband_inst_t));
				wlc->bandinst[i] = NULL;
			}
		}
		MFREE(osh, wlc->bandinst, sizeof(wlcband_inst_t *) * MAXBANDS);
		wlc->bandinst = NULL;
	}

#ifdef WL_PKTDROP_STATS
	wlc_pktdrop_full_counters_disable(wlc);
#endif /* WL_PKTDROP_STATS */
}

static void
BCMATTACHFN(wlc_detach_mfree_misc)(wlc_info_t *wlc, osl_t *osh)
{
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(osh);

}

void
BCMATTACHFN(wlc_detach_mfree)(wlc_info_t *wlc, osl_t *osh)
{
	if (wlc == NULL)
		return;

	if (wlc->pub == NULL)
		goto free_wlc;

	wlc_detach_mfree_misc(wlc, osh);

	wlc_detach_mfree_high(wlc, osh);

	if (wlc->corestate) {
		if (wlc->corestate->macstat_snapshot) {
			MFREE(osh, wlc->corestate->macstat_snapshot,
				sizeof(uint16) * MACSTAT_OFFSET_SZ);
			wlc->corestate->macstat_snapshot = NULL;
		}

#if defined(WL_PSMX)
		if (wlc->corestate->macxstat_snapshot) {
			MFREE(osh, wlc->corestate->macxstat_snapshot,
				sizeof(uint16) * MACXSTAT_OFFSET_SZ);
			wlc->corestate->macxstat_snapshot = NULL;
		}
#endif // endif

		MFREE(osh, wlc->corestate, sizeof(wlccore_t));
		wlc->corestate = NULL;
	}

	/* free pub struct */
	wlc_pub_mfree(wlc, osh, wlc->pub);
	wlc->pub = NULL;

free_wlc:
	/* free the wlc */
	MFREE(osh, wlc, sizeof(wlc_info_t));
	wlc = NULL;
}
