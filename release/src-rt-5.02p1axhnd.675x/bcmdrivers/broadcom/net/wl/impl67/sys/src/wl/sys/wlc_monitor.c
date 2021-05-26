/*
 * Monitor modules implementation
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
 * $Id: wlc_monitor.c 785034 2020-03-11 11:53:38Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_monitor.h>
#include <wlc_bsscfg.h>
#include <wl_export.h>
#include <wlc_vht.h>
#include <wlc_ht.h>
#include <wlc_rspec.h>
#include <wlc_chanctxt.h>
#include <phy_chanmgr_api.h>
#include <phy_calmgr_api.h>
#include <phy_hecap_api.h>
#include <phy_ac_info.h>

#if defined(PHYCAL_CACHING)
#include <wlc_phy_hal.h>
#endif // endif
#include <wlc_tx.h>

/* default promisc bits */

#define WL_MONITOR_PROMISC_BITS_DEF \
	(WL_MONPROMISC_PROMISC | WL_MONPROMISC_CTRL)

/* wlc access macros */
#define WLCUNIT(x) ((x)->wlc->pub->unit)
#define WLCPUB(x) ((x)->wlc->pub)
#define WLCOSH(x) ((x)->wlc->osh)
#define WLC(x) ((x)->wlc)

#define RX_AMSDU_IN_AMPDU	0x1

struct wlc_monitor_info {
	wlc_info_t *wlc;
	uint32 promisc_bits; /* monitor promiscuity bitmap */
	chanspec_t chanspec;
	bool timer_active;
	struct wl_timer * mon_cal_timer;
	uint32 monitor_flags;
	struct ether_addr align_ea;
};

/* IOVar table */
enum {
	IOV_MONITOR_PROMISC_LEVEL = 0,
	IOV_MONITOR_CONFIG = 1,
	IOV_LAST
};

static const bcm_iovar_t monitor_iovars[] = {
	{"monitor_promisc_level", IOV_MONITOR_PROMISC_LEVEL,
	(0), 0, IOVT_UINT32, 0
	},
	{"monitor_config", IOV_MONITOR_CONFIG,
	(IOVF_SET_CLK | IOVF_GET_CLK), 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0 }
};

/* **** Private Functions Prototypes *** */

/* Forward declarations for functions registered for this module */
static int wlc_monitor_doiovar(
		    void                *hdl,
		    uint32              actionid,
		    void                *p,
		    uint                plen,
		    void                *a,
		    uint                 alen,
		    uint                 vsize,
		    struct wlc_if       *wlcif);

static void
wlc_monitor_phy_cal_timer(void *arg)
{
	wlc_info_t * wlc = (wlc_info_t *)arg;
	wlc_monitor_info_t * ctxt = wlc->mon_info;

	if (wlc->chanspec == ctxt->chanspec) {
		wlc_clr_quiet_chanspec(wlc->cmi, wlc->chanspec);
		wlc_mute(wlc, OFF, 0);
		wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_JOIN_BSS);
	}
	ctxt->timer_active = FALSE;
}

static int
wlc_monitor_down(void *context)
{
	wlc_monitor_info_t * mon_info = (wlc_monitor_info_t *)context;

	if (mon_info->timer_active) {
		wl_del_timer(WLC(mon_info)->wl, mon_info->mon_cal_timer);
		mon_info->timer_active = FALSE;
	}

	return 0;
}

/* **** Public Functions *** */
/*
 * Initialize the sta monitor private context and resources.
 * Returns a pointer to the sta monitor private context, NULL on failure.
 */
wlc_monitor_info_t *
BCMATTACHFN(wlc_monitor_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_monitor_info_t *ctxt;

	ctxt = (wlc_monitor_info_t*)MALLOCZ(pub->osh, sizeof(wlc_monitor_info_t));
	if (ctxt == NULL) {
		WL_ERROR(("wl%d: %s: ctxt MALLOCZ failed; total mallocs %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	ctxt->wlc = wlc;
	ctxt->promisc_bits = 0;
	ctxt->mon_cal_timer = wl_init_timer(wlc->wl, wlc_monitor_phy_cal_timer, wlc, "monitor");
	ctxt->chanspec = 0;
	ctxt->timer_active = FALSE;

	/* register module */
	if (wlc_module_register(
			    wlc->pub,
			    monitor_iovars,
			    "monitor",
			    ctxt,
			    wlc_monitor_doiovar,
			    NULL,
			    NULL,
			    wlc_monitor_down)) {
				WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
				    WLCUNIT(ctxt),
				    __FUNCTION__));

				goto fail;
			    }

	return ctxt;

fail:
	MFREE(pub->osh, ctxt, sizeof(wlc_monitor_info_t));

	return NULL;
}

/*
 * Release net detect private context and resources.
 */
void
BCMATTACHFN(wlc_monitor_detach)(wlc_monitor_info_t *ctxt)
{
	if (ctxt != NULL) {
		if (ctxt->mon_cal_timer) {
			wl_free_timer(WLC(ctxt)->wl, ctxt->mon_cal_timer);
			ctxt->mon_cal_timer = NULL;
			ctxt->timer_active = FALSE;
		}

		/* Unregister the module */
		wlc_module_unregister(WLCPUB(ctxt), "monitor", ctxt);
		/* Free the all context memory */
	    MFREE(WLCOSH(ctxt), ctxt, sizeof(wlc_monitor_info_t));
	}
}

struct ether_addr*
wlc_monitor_get_align_ea(wlc_monitor_info_t *pmoninfo)
{
	return &pmoninfo->align_ea;
}

void
wlc_monitor_phy_cal_timer_start(wlc_monitor_info_t *ctxt, uint32 tm)
{
	wlc_info_t * wlc = WLC(ctxt);

	if (ctxt->timer_active) {
		wl_del_timer(wlc->wl, ctxt->mon_cal_timer);
	}

	wl_add_timer(wlc->wl, ctxt->mon_cal_timer, tm, 0);
	ctxt->timer_active = TRUE;
}

void
wlc_monitor_phy_cal(wlc_monitor_info_t *ctxt, bool enable)
{
	wlc_info_t * wlc = WLC(ctxt);

#if defined(PHYCAL_CACHING)
	int idx;
	wlc_bsscfg_t *cfg;

	if (ctxt->chanspec == 0)
		goto skip_del_ctx;

	/* Delete previous cal ctx if any */
	FOREACH_BSS(wlc, idx, cfg) {
		if (wlc_shared_chanctxt_on_chan(wlc, cfg, wlc->mon_info->chanspec)) {
			goto skip_del_ctx;
		}
	}

	phy_chanmgr_destroy_ctx(WLC_PI(wlc), ctxt->chanspec);

skip_del_ctx:

#endif /* PHYCAL_CACHING */

	wl_del_timer(wlc->wl, ctxt->mon_cal_timer);
	ctxt->timer_active = FALSE;
	ctxt->chanspec = 0;

	if (enable) {

		/* No need to calibrate the channel when any connections are active */
		if ((wlc->stas_connected != 0) || (wlc->aps_associated != 0)) {
			return;
		}

		ctxt->chanspec = wlc->chanspec;

#if defined(PHYCAL_CACHING)
		phy_chanmgr_create_ctx(WLC_PI(wlc), ctxt->chanspec);
#endif /* PHYCAL_CACHING */

		wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_JOIN_BSS);
	}

	return;
}

void
wlc_monitor_promisc_enable(wlc_monitor_info_t *ctxt, bool enab)
{
	wlc_info_t *wlc;
	if (ctxt == NULL)
		return;

	wlc = WLC(ctxt);
	wlc_monitor_phy_cal(ctxt, enab);
	wlc_mac_bcn_promisc_update(wlc, BCNMISC_MONITOR, enab);
	wlc_mac_promisc(wlc);
}

void
wlc_monitor_set_promisc_bit(wlc_monitor_info_t *ctxt, bool enab)
{
	wlc_info_t *wlc;
		if (ctxt == NULL)
			return;
		wlc = WLC(ctxt);
	/* Add/remove corresponding promisc bits if monitor is enabled/disabled. */
	if (enab) {
		ctxt->promisc_bits |= WL_MONITOR_PROMISC_BITS_DEF;
		if (wlc->_rx_amsdu_in_ampdu) {
			wlc->_rx_amsdu_in_ampdu = FALSE;
			wlc->mon_info->monitor_flags |= RX_AMSDU_IN_AMPDU;
		}
	}
	else {
		ctxt->promisc_bits &= ~WL_MONITOR_PROMISC_BITS_DEF;
		if (wlc->mon_info->monitor_flags & RX_AMSDU_IN_AMPDU) {
			wlc->mon_info->monitor_flags &= ~RX_AMSDU_IN_AMPDU;
			wlc->_rx_amsdu_in_ampdu = TRUE;
		}
	}
}

uint32
wlc_monitor_get_mctl_promisc_bits(wlc_monitor_info_t *ctxt)
{
	uint32 promisc_bits = 0;

	if (ctxt != NULL) {
		if (ctxt->promisc_bits & WL_MONPROMISC_PROMISC)
			promisc_bits |= MCTL_PROMISC;
		if (ctxt->promisc_bits & WL_MONPROMISC_CTRL)
			promisc_bits |= MCTL_KEEPCONTROL;
		if (ctxt->promisc_bits & WL_MONPROMISC_FCS)
			promisc_bits |= MCTL_KEEPBADFCS;
		if (ctxt->promisc_bits & WL_MONPROMISC_HETB)
			promisc_bits |= MCTL_HETB;
	}
	return promisc_bits;
}

/* **** Private Functions *** */

static int
wlc_monitor_doiovar(
	void                *hdl,
	uint32              actionid,
	void                *p,
	uint                plen,
	void                *a,
	uint                 alen,
	uint                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_monitor_info_t *ctxt = hdl;
	wlc_info_t *wlc = ctxt->wlc;
	int32 *ret_int_ptr = (int32 *)a;
	int32 int_val = 0;
	int err = BCME_OK;
	wl_monitor_config_params_t *mntcfg_params = NULL;
	printf("%s\n", __FUNCTION__);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(alen);
	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(mntcfg_params);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_MONITOR_PROMISC_LEVEL):
		*ret_int_ptr = (int32)ctxt->promisc_bits;
		break;

	case IOV_SVAL(IOV_MONITOR_PROMISC_LEVEL):
		ctxt->promisc_bits = (uint32)int_val;
		if (MONITOR_ENAB(WLC(ctxt)))
			wlc_mac_promisc(WLC(ctxt));
		break;

	case IOV_GVAL(IOV_MONITOR_CONFIG):
		*ret_int_ptr = 0;
		break;

	case IOV_SVAL(IOV_MONITOR_CONFIG):
#ifdef WL11AX
		mntcfg_params = (wl_monitor_config_params_t *) p;
		if (mntcfg_params->flags & WL_MONITOR_CONFIG_FLAG_HE_AID) {
			phy_info_t *pi = WLC_PI(wlc);
			wlc_bsscolor_t bsscolor;
			int i, bssidx, bssbmp;
			if (mntcfg_params->flags & WL_MONITOR_CONFIG_FLAG_BC) {
				bsscolor.color = mntcfg_params->bsscolor;
				bsscolor.disable = FALSE;
			} else {
				bsscolor.disable = TRUE;
			}
			if (mntcfg_params->flags & WL_MONITOR_CONFIG_FLAG_BSSBMP) {
				bssbmp = mntcfg_params->he_bssidx_bmp;
			} else {
				/* if bss bmp is not set, configure both bss */
				bssbmp = ((1 << HE_BSSCOLOR_IDX0) | (1 << HE_BSSCOLOR_IDX1));
			}
			for (bssidx = 0; bssidx <= HE_BSSCOLOR_IDX1; bssidx++) {
				if ((bssbmp & (1 << bssidx)) == 0) {
					continue;
				}
				bsscolor.index = bssidx;
				for (i = 0; i < MIN(mntcfg_params->he_num_aids,
					HE_BSSCOLOR_MAX_STAID); i++) {
					bsscolor.staid[i] = mntcfg_params->he_aid[i];
				}
				phy_hecap_write_bsscolor(pi, &bsscolor);
				/* XXX: may need to program phyreg HESigParseCtrl.he_mybss_color_len
				 * based on bssbmp
				 */
			}
		}
		if (mntcfg_params->flags & WL_MONITOR_CONFIG_FLAG_HETB) {
			phy_info_t *pi = WLC_PI(wlc);
			pi->u.pi_acphy->sniffer_aligner.enabled = FALSE;
			pi->u.pi_acphy->sniffer_aligner.active = FALSE;
			if (mntcfg_params->hetb_en) {
				wlc->mon_info->promisc_bits |= WL_MONPROMISC_HETB;
				if (mntcfg_params->flags & WL_MONITOR_CONFIG_FLAG_HETB_MACADDR) {
					pi->u.pi_acphy->sniffer_aligner.enabled = TRUE;
					pi->u.pi_acphy->sniffer_aligner.active = TRUE;
					memcpy(&wlc->mon_info->align_ea,
						&mntcfg_params->ea, ETHER_ADDR_LEN);
				}
			} else {
				wlc->mon_info->promisc_bits &= ~WL_MONPROMISC_HETB;
			}
			wlc_mac_promisc(wlc);
		}
#endif /* WL11AX */
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#ifdef WLTXMONITOR
/**
 * Convert TX hardware status to standard format and send to wl_tx_monitor assume p points to plcp
 * header.
 */
void
wlc_tx_monitor(wlc_info_t *wlc, d11txhdr_t *txh, tx_status_t *txs,
               void *p, struct wlc_if *wlcif)
{
	struct wl_txsts sts;
	ratespec_t rspec;
	uint16 chan_bw, chan_band, chan_num;
	uint16 xfts;
	uint8 frametype, *plcp;
	uint16 phytxctl1, phytxctl;
	struct dot11_header *h;

	/* XXX this routine doesn't appear support core revid >= 40 at all
	 */
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		WL_ERROR(("wl%d: %s: need update for revid %d\n",
		          wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
		return;
	}

	if (p == NULL)
		WL_ERROR(("%s: null ptr2", __FUNCTION__));

	ASSERT(p != NULL);

	bzero((void *)&sts, sizeof(wl_txsts_t));

	sts.wlif = wlcif ? wlcif->wlif : NULL;

	sts.mactime = txs->lasttxtime;
	sts.retries = (txs->status.frag_tx_cnt);

	plcp = (uint8 *)txh + D11_TXH_LEN;
	PKTPULL(wlc->osh, p, D11_TXH_LEN);

	xfts = ltoh16(txh->pre40.XtraFrameTypes);
	chan_num = xfts >> XFTS_CHANNEL_SHIFT;
	chan_band = (chan_num > CH_MAX_2G_CHANNEL) ? WL_CHANSPEC_BAND_5G : WL_CHANSPEC_BAND_2G;
	phytxctl = ltoh16(txh->pre40.PhyTxControlWord);
	phytxctl1 = ltoh16(txh->pre40.PhyTxControlWord_1);
	wlc_ht_txmon_chspec(wlc->hti, phytxctl, phytxctl1,
			chan_band, chan_num,
			&sts, &chan_bw);

	frametype = phytxctl & PHY_TXC_FT_MASK;
	if (frametype == FT_HT) {
		wlc_ht_txmon_htflags(wlc->hti, phytxctl, phytxctl1, plcp, chan_bw,
			chan_band, chan_num, &rspec, &sts);
	} else if (frametype == FT_OFDM) {
		rspec = ofdm_plcp_to_rspec(plcp[0]);
		sts.datarate = RSPEC2KBPS(rspec)/500;
		sts.encoding = WL_RXS_ENCODING_OFDM;
		sts.preamble = WL_RXS_PREAMBLE_SHORT;

		sts.phytype = WL_RXS_PHY_G;
	} else {
		ASSERT(frametype == FT_CCK);
		sts.datarate = plcp[0]/5;
		sts.encoding = WL_RXS_ENCODING_DSSS_CCK;
		sts.preamble = (phytxctl & PHY_TXC_SHORT_HDR) ?
		        WL_RXS_PREAMBLE_SHORT : WL_RXS_PREAMBLE_LONG;

		sts.phytype = WL_RXS_PHY_B;
	}

	sts.pktlength = PKTLEN(wlc->pub->osh, p) - D11_PHY_HDR_LEN;

	h = (struct dot11_header *)(plcp + D11_PHY_HDR_LEN);

	if (!ETHER_ISMULTI(txh->pre40.TxFrameRA) &&
	    !txs->status.was_acked)
	        sts.txflags |= WL_TXS_TXF_FAIL;
	if (ltoh16(txh->pre40.MacTxControlLow) & TXC_SENDCTS)
		sts.txflags |= WL_TXS_TXF_CTS;
	else if (ltoh16(txh->pre40.MacTxControlLow) & TXC_SENDRTS)
		sts.txflags |= WL_TXS_TXF_RTSCTS;

	wlc_ht_txmon_agg_ft(wlc->hti, p, h, frametype, &sts);

	wl_tx_monitor(wlc->wl, &sts, p);
} /* wlc_tx_monitor */
#endif /* WLTXMONITOR */
