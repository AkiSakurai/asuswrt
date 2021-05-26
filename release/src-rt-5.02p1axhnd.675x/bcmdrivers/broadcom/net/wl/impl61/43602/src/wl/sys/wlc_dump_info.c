/** \file wlc_dump_info.c
 *
 * Common (OS-independent) info dump portion of Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_dump_info.c 451514 2014-01-27 00:24:19Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <siutils.h>
#include <bcmendian.h>
#include <nicpci.h>
#include <wlioctl.h>
#include <pcie_core.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wl_dbg.h>
#include <wlc_channel.h>
#include <wlc_stf.h>
#include <wlc_bmac.h>
#include <wlc_scan.h>
#include <wlc_rm.h>
#include <wlc_addrmatch.h>
#include <wlc_ampdu.h>
#include <bcmnvram.h>
#ifdef WLC_HIGH_ONLY
#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <bcm_xdr.h>
#include <wlc_rpc.h>
#include <wlc_rpctx.h>
#endif /* WLC_HIGH_ONLY */
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#ifdef BCMDBG_TXSTUCK
#include <wlc_apps.h>
#endif /* BCMDBG_TXSTUCK */

#include <wlc_dump_info.h>

/* Shared memory location index for various AC params */
#define wme_shmemacindex(ac)	wme_ac2fifo[ac]

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
static int wlc_dump_list(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif
#if IOV_DUMP_REGULAR_IOVAR_ENAB
static char* wlc_dump_next_name(char **buf);
static int wlc_dump_registered_name(wlc_info_t *wlc, char *name, struct bcmstrbuf *b);
#endif /* IOV_DUMP_REGULAR_IOVAR_ENAB */

#ifdef WLTINYDUMP
static int wlc_tinydump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* WLTINYDUMP */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_dump_default(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_all(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_bssinfo_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_wlc(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_ratestuff(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_mac(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_pio(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_dma(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_wme(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_stats(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_btc(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_bmc(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_dump_bss_info(const char *name, wlc_bss_info_t *bi, struct bcmstrbuf *b);
static int wlc_dump_obss(wlc_info_t *wlc, struct bcmstrbuf *b);
static void wlc_dump_edcf_acp(wlc_info_t *wlc, struct bcmstrbuf *b,
	edcf_acparam_t *acp_ie, const char *desc);
static void wlc_dump_wme_shm(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */

#if defined(BCMDBG_TXSTUCK)
static void wlc_dump_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* defined(BCMDBG_TXSTUCK) */

extern uint8 wme_ac2fifo[];
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_INFORM) || \
	defined(BCMDBG_DUMP)
extern const char *const aci_names[];
#endif // endif

#if defined(WLTINYDUMP) || IOV_DUMP_REGULAR_IOVAR_ENAB
#define IOV_DUMP_DEF_IOVAR_ENAB 1
#else
#define IOV_DUMP_DEF_IOVAR_ENAB 0
#endif /* defined(WLTINYDUMP) || IOV_DUMP_REGULAR_IOVAR_ENAB */

#if defined(WLTEST)
#define IOV_DUMP_NVM_IOVAR_ENAB 1
#else
#define IOV_DUMP_NVM_IOVAR_ENAB 0
#endif // endif

#if (IOV_DUMP_NVM_IOVAR_ENAB || IOV_DUMP_DEF_IOVAR_ENAB)
#define IOV_DUMP_ANY_IOVAR_ENAB 1
#else
#define IOV_DUMP_ANY_IOVAR_ENAB 0
#endif /* IOV_DUMP_NVM_IOVAR_ENAB || IOV_DUMP_DEF_IOVAR_ENAB */

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
#if defined(BCMDBG_PHYDUMP)
static int
wlc_dump_phy_radioreg(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int err = 0;
	if ((err = wlc_iocregchk(wlc, WLC_BAND_AUTO)))
		return err;

	if ((err = wlc_iocpichk(wlc, wlc->band->phytype)))
		return err;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_RADIOREG_ID);
	return 0;
}

static int
wlc_dump_phy_phyreg(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int err = 0;
	if ((err = wlc_iocregchk(wlc, WLC_BAND_AUTO)))
		return err;

	if ((err = wlc_iocpichk(wlc, wlc->band->phytype)))
		return err;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHYREG_ID);
	return 0;
}
#endif // endif

static int
wlc_dump_phy_cal(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_PHYCAL_ID);
	return 0;
}

static int
wlc_dump_phy_aci(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_ACI_ID);
	return 0;
}

static int
wlc_dump_phy_papd(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_PAPD_ID);

	if (b->size == b->origsize)
		return BCME_UNSUPPORTED;

	return 0;
}

static int
wlc_dump_phy_noise(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_NOISE_ID);
	return 0;
}

static int
wlc_dump_phy_state(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_STATE_ID);
	return 0;
}

static int
wlc_dump_phy_measlo(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!WLCISGPHY(wlc->band))
		return BCME_UNSUPPORTED;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_MEASLO_ID);
	return 0;
}

static int
wlc_dump_phy_lnagain(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_LNAGAIN_ID);
	return 0;
}

static int
wlc_dump_phy_initgain(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_INITGAIN_ID);
	return 0;
}

static int
wlc_dump_phy_hpf1tbl(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!(WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)))
		return BCME_UNSUPPORTED;

	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_HPF1TBL_ID);
	return 0;
}

static int
wlc_dump_phy_lpphytbl0(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!WLCISLPPHY(wlc->band))
		return BCME_UNSUPPORTED;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_LPPHYTBL0_ID);
	return 0;
}

static int
wlc_dump_phy_chanest(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_CHANEST_ID);
	return 0;
}

static int
wlc_dump_suspend(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_SUSPEND_ID);
	return 0;
}

#ifdef ENABLE_FCBS
static int
wlc_dump_phy_fcbs(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_FCBS_ID);
	return 0;
}
#endif /* ENABLE_FCBS */

static int
wlc_dump_phy_txv0(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_TXV0_ID);
	return 0;
}
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP) */

#ifdef WLTEST
static int
wlc_dump_phy_ch4rpcal(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk)
		return BCME_NOCLK;

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PHY_CH4RPCAL_ID);
	return 0;
}
#endif /* WLTEST */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_dump_mempool(void *arg, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *) arg;

	return (bcm_mpm_dump(wlc->mem_pool_mgr, b));
}
#endif   /* BCMDBG || BCMDBG_DUMP */

#if IOV_DUMP_ANY_IOVAR_ENAB
static int
wlc_dump_info_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int vsize, struct wlc_if *wlcif);
#endif /* IOV_DUMP_ANY_IOVAR_ENAB */

void
BCMATTACHFN(wlc_dump_info_detach)(wlc_info_t *wlc)
{
#if IOV_DUMP_ANY_IOVAR_ENAB
	wlc_module_unregister(wlc->pub, "dump_info", wlc);
#endif /* IOV_DUMP_ANY_IOVAR_ENAB */
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
/* Format a general info dump */
static int
wlc_dump_default(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	char *name_list;
	int name_list_len;
	struct bcmstrbuf names;
	char *name;
	char *p;
	int err = 0;

	/* create the name list for a default dump */
	name_list_len = 128;
	name_list = (char*)MALLOC(wlc->osh, name_list_len);
	if (!name_list)
		return BCME_NOMEM;

	bcm_binit(&names, name_list, name_list_len);

	bcm_bprintf(&names, "wlc phystate bsscfg bssinfo ratestuff stats ");

	if (wlc->clk)
		bcm_bprintf(&names, "pio dma ");

	if (EDCF_ENAB(wlc->pub) && wlc->pub->up)
		bcm_bprintf(&names, "wme ");

	if (AMPDU_ENAB(wlc->pub))
		bcm_bprintf(&names, "ampdu ");
#ifdef WET
	if (wlc->wet)
		bcm_bprintf(&names, "wet ");
#endif /* WET */

	if (TOE_ENAB(wlc->pub))
		bcm_bprintf(&names, "toe ");

#ifdef WLLED
	if (wlc->ledh)
		bcm_bprintf(&names, "led ");
#endif // endif

#ifdef WLAMSDU
	/* only dump amsdu if we were handed a large dump buffer */
	if (b->size > 8000 &&
	    (AMSDU_TX_ENAB(wlc->pub) || wlc->_amsdu_rx))
		bcm_bprintf(&names, "amsdu ");
#endif // endif

	if (CAC_ENAB(wlc->pub))
		bcm_bprintf(&names, "cac ");

#ifdef TRAFFIC_MGMT
	if (TRAFFIC_MGMT_ENAB(wlc->pub)) {
		bcm_bprintf(&names, "trfmgmt_stats ");
		bcm_bprintf(&names, "trfmgmt_shaping ");
	}
#endif // endif

	/* dump the list */
	p = name_list;
	while ((name = wlc_dump_next_name(&p)) != NULL) {
		bcm_bprintf(b, "\n%s:------\n", name);
		err = wlc_dump_registered_name(wlc, name, b);
		if (err)
			break;
	}

	MFREE(wlc->osh, name_list, name_list_len);

	return err;
}

/*
* This function calls all dumps.
*/
static int
wlc_dump_all(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int bcmerror = 0;
	dumpcb_t *d;

	for (d = wlc->dumpcb_head; d != NULL; d = d->next) {
		/* don't parse "all", else it will recurse */
		if ((strncmp(d->name, "all", sizeof("all") - 1) == 0))
			continue;
		/* don't print "list" option in "all" */
		if ((strncmp(d->name, "list", sizeof("list") - 1) == 0))
			continue;
		/* don't print "default" option in "all" */
		if ((strncmp(d->name, "default", sizeof("default") - 1) == 0))
			continue;
		/* phytbl & phytbl2 are too big to be included in all */
		if ((strncmp(d->name, "phytbl", sizeof("phytbl") - 1) == 0))
			continue;
		/* phytbl & phytbl2 are too big to be included in all */
		if ((strncmp(d->name, "phytbl2", sizeof("phytbl2") - 1) == 0))
			continue;

		bcm_bprintf(b, "\n%s:------\n", d->name);

		/* Continue if there's no BCME_BUFTOOSHORT error */
		bcmerror = d->dump_fn(d->dump_fn_arg, b);
		if (bcmerror == BCME_BUFTOOSHORT)
			return bcmerror;
	}

	return 0;
}

static int
wlc_bssinfo_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int idx;
	wlc_bsscfg_t *cfg;

	bcm_bprintf(b, "\n");
	wlc_dump_bss_info("default_bss", wlc->default_bss, b);
	bcm_bprintf(b, "\n");
	FOREACH_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "bsscfg %d (0x%p):\n", idx, cfg);
		bcm_bprintf(b, "\n");
		wlc_dump_bss_info("target_bss", cfg->target_bss, b);
		bcm_bprintf(b, "\n");
		wlc_dump_bss_info("current_bss", cfg->current_bss, b);
	}
	return 0;
}

static int
wlc_dump_wlc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlcband_t *band;
	uint fifo_size[NFIFO];
	char perm[32], cur[32];
	char chn[8];
	uint16 maj_rev, min_rev, date, ucodetime;
	uint16 dbgst;
	uint i;
	wlc_bsscfg_t *cfg;
	wlc_txq_info_t *qi;
	wlc_if_t *wlcif;
	wlc_tunables_t *tunables;

	/* read ucode revision info */
	maj_rev = wlc->ucode_rev >> NBITS(uint16);
	min_rev = wlc->ucode_rev & 0xffff;

	/* skip accessing registers if the clock is off */
	if (!wlc->clk) {
		date = ucodetime = dbgst = 0;
	} else {
		date = wlc_read_shm(wlc, M_UCODE_DATE);
		ucodetime = wlc_read_shm(wlc, M_UCODE_TIME);
		/* read ucode debug status */
		dbgst = wlc_read_shm(wlc, M_UCODE_DBGST);
	}

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "wl%d: wlc %p wl %p msglevel 0x%x clk %d up %d hw_off %d now %d\n",
	            wlc->pub->unit, wlc, wlc->wl, wl_msg_level, wlc->clk,
	            wlc->pub->up, wlc->pub->hw_off, wlc->pub->now);

	bcm_bprintf(b, "ucode %d.%d %02d/%02d/%02d %02d:%02d:%02d dbgst 0x%x\n",
	            maj_rev, min_rev,
	            (date >> 8) & 0xf, date & 0xff, (date >> 12) & 0xf,
	            (ucodetime >> 11) & 0x1f, (ucodetime >> 5) & 0x3f, ucodetime & 0x1f, dbgst);

	bcm_bprintf(b, "capabilities: ");
	wlc_cap_bcmstrbuf(wlc, b);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "promisc %d monitor %d piomode %d gmode %d\n",
	            wlc->pub->promisc, wlc->monitor, PIO_ENAB(wlc->pub), wlc->band->gmode);

	bcm_bprintf(b, "ap %d apsta %d wet %d wme %d mac_spoof %d per-ac maxrate %d\n",
	            AP_ENAB(wlc->pub), APSTA_ENAB(wlc->pub), wlc->wet, wlc->pub->_wme,
	            wlc->mac_spoof, WME_PER_AC_MAXRATE_ENAB(wlc->pub));

	bcm_bprintf(b, "vendor 0x%x device 0x%x nbands %d regs %p\n",
	            wlc->vendorid, wlc->deviceid, NBANDS(wlc), wlc->regs);

	bcm_bprintf(b, "chip %s chiprev %d corerev %d maccap 0x%x\n",
	            bcm_chipname(SI_CHIPID(wlc->pub->sih), chn, 8), wlc->pub->sih->chiprev,
	            wlc->pub->corerev, wlc->machwcap);

	bcm_bprintf(b, "boardvendor 0x%x boardtype 0x%x boardrev %s "
	            "boardflags 0x%x boardflags2 0x%x sromrev %d\n",
	            wlc->pub->sih->boardvendor, wlc->pub->sih->boardtype,
	            bcm_brev_str(wlc->pub->boardrev, cur), wlc->pub->boardflags,
	            wlc->pub->boardflags2, wlc->pub->sromrev);
	if (wlc->pub->boardrev == BOARDREV_PROMOTED)
		bcm_bprintf(b, " (actually 0x%02x)", BOARDREV_PROMOTABLE);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "perm_etheraddr %s cur_etheraddr %s\n",
	            bcm_ether_ntoa(&wlc->perm_etheraddr, (char*)perm),
	            bcm_ether_ntoa(&wlc->pub->cur_etheraddr, (char*)cur));

	bcm_bprintf(b, "swdecrypt %d\n",
	            wlc->pub->swdecrypt);

	bcm_bprintf(b, "\nuserfragthresh %d, fragthresh %d/%d/%d/%d RTSThresh %d \n",
	            wlc->usr_fragthresh, wlc->fragthresh[0], wlc->fragthresh[1],
	            wlc->fragthresh[2], wlc->fragthresh[3], wlc->RTSThresh);

	bcm_bprintf(b, "\nSRL %d LRL %d SFBL %d LFBL %d\n",
	            wlc->SRL, wlc->LRL, wlc->SFBL, wlc->LFBL);

	bcm_bprintf(b, "shortslot %d shortslot_ovrrd %d ignore_bcns %d\n",
	            wlc->shortslot, wlc->shortslot_override, wlc->ignore_bcns);

	bcm_bprintf(b, "\nblock_datafifo 0x%x tx_suspended %d\n",
	            wlc->block_datafifo, wlc->tx_suspended);

	bcm_bprintf(b, "bandunit %d bandlocked %d \n", wlc->band->bandunit, wlc->bandlocked);
	bcm_bprintf(b, "radio_disabled 0x%x down_override %d\n", wlc->pub->radio_disabled,
	            wlc->down_override);
#ifdef WL11AC
	bcm_bprintf(b, "vht_features  0x%x\n", wlc->pub->vht_features);
#endif // endif

#ifdef STA
	bcm_bprintf(b, "mpc %d, mpc_scan %d mpc_join %d mpc_oidscan %d mpc_oidjoin %d"
	            " mpc_oidnettype %d mpc_out %d\n",
	            wlc->mpc, wlc->mpc_scan, wlc->mpc_join, wlc->mpc_oidscan, wlc->mpc_oidjoin,
	            wlc->mpc_oidnettype, wlc->mpc_out);
#endif // endif

	bcm_bprintf(b, "5G band: ratespec_override 0x%x mratespec_override 0x%x\n",
	            wlc->bandstate[BAND_5G_INDEX]->rspec_override,
	            wlc->bandstate[BAND_5G_INDEX]->mrspec_override);

	bcm_bprintf(b, "2G band: ratespec_override 0x%x mratespec_override 0x%x\n",
	            wlc->bandstate[BAND_2G_INDEX]->rspec_override,
	            wlc->bandstate[BAND_2G_INDEX]->mrspec_override);
	bcm_bprintf(b, "\n");

	FOREACH_BSS(wlc, i, cfg) {
		bcm_bprintf(b, "PLCPHdr_ovrrd %d\n",
			cfg->PLCPHdr_override);
	}

	bcm_bprintf(b, "CCK_power_boost %d \n", (wlc->pub->boardflags & BFL_CCKHIPWR) ? 1 : 0);

	bcm_bprintf(b, "mhf 0x%02x mhf2 0x%02x mhf3 0x%02x mhf4 0x%02x mhf5 0x%02x\n",
		wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF2, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF3, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF4, WLC_BAND_AUTO),
		wlc_bmac_mhf_get(wlc->hw, MHF5, WLC_BAND_AUTO));

	bcm_bprintf(b, "swdecrypt %d\n", wlc->pub->swdecrypt);

#ifdef STA
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "STAY_AWAKE() %d wake %d PMpending %d\n",
	            STAY_AWAKE(wlc), wlc->wake, wlc->PMpending);
	FOREACH_AS_STA(wlc, i, cfg) {
		bcm_bprintf(b, "join_pref->band %d\n", cfg->join_pref->band);

		bcm_bprintf(b, "assoc->state %d assoc->type %d assoc->flags 0x%x assocroam %d\n",
		            cfg->assoc->state, cfg->assoc->type, cfg->assoc->flags,
		            cfg->roam->assocroam);

		bcm_bprintf(b, "wsec_portopen %d WME_PM_blocked %d\n",
		            cfg->wsec_portopen, cfg->pm->WME_PM_blocked);

		bcm_bprintf(b, "PM mode %d PMenabled %d PM_override %d "
		            "PS_ALLOWED() %d PSpoll %d\n",
		            cfg->pm->PM, cfg->pm->PMenabled, cfg->pm->PM_override,
		            PS_ALLOWED(cfg), cfg->pm->PSpoll);
		bcm_bprintf(b, "WLC_PORTOPEN() %d dtim_programmed %d PMpending %d "
		            "priorPMstate %d\n",
		            WLC_PORTOPEN(cfg), cfg->dtim_programmed,
		            cfg->pm->PMpending, cfg->pm->priorPMstate);

		bcm_bprintf(b, "bcns_lost %d time_since_bcn %d\n",
		            cfg->roam->bcns_lost, cfg->roam->time_since_bcn);

		bcm_bprintf(b, "BSSID %s BSS %d\n",
		            bcm_ether_ntoa(&cfg->BSSID, (char*)cur),
		            cfg->BSS);
		bcm_bprintf(b, "reprate %dkbps\n",
		            RSPEC2KBPS(wlc_get_rspec_history(cfg)));

		bcm_bprintf(b, "\n");
	}
#endif /* STA */

	bcm_bprintf(b, "associated %d stas_associated %d aps_associated %d\n",
	            wlc->pub->associated, wlc->stas_associated, wlc->aps_associated);

	FOREACH_BSS(wlc, i, cfg) {
		if (!BSSCFG_AP(cfg))
			continue;
	        bcm_bprintf(b, "BSSID %s\n", bcm_ether_ntoa(&cfg->BSSID, (char*)perm));
	}
	bcm_bprintf(b, "AID 0x%x\n", wlc->AID);

	if (wlc->pub->up)
		bcm_bprintf(b, "chan %d ", CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC));
	else
		bcm_bprintf(b, "chan N/A ");

	bcm_bprintf(b, "country \"%s\"\n", wlc_channel_country_abbrev(wlc->cmi));

	bcm_bprintf(b, "counter %d\n", wlc->counter & 0xfff);

	bcm_bprintf(b, "\n");

	for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
		bcm_bprintf(b, "txqinfo %p len %d stopped 0x%x\n",
		            qi, pktq_len(&qi->q), qi->stopped);
		bcm_bprintf(b, "associated wlcifs:");

		for (wlcif = wlc->wlcif_list;  wlcif != NULL; wlcif = wlcif->next) {
			char ifname[32];

			if (wlcif->qi != qi)
				continue;

			strncpy(ifname, wl_ifname(wlc->wl, wlcif->wlif), sizeof(ifname));
			ifname[sizeof(ifname) - 1] = '\0';

			bcm_bprintf(b, " \"%s\" 0x%p", ifname, wlcif);
		}
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "malloc_failed %d\n", MALLOC_FAILED(wlc->osh));

#ifdef STA
	bcm_bprintf(b, "freqtrack: wideband %d override %d attempts %d duration %d\n",
	            wlc->freqtrack,
	            wlc->freqtrack_override,
	            wlc->freqtrack_attempts,
	            (wlc->freqtrack_attempts > 0) ? (wlc->pub->now - wlc->freqtrack_starttime) : 0);
#endif /* STA */

	bcm_bprintf(b, "txpktpend AC_BK %d AC_BE %d AC_VI %d AC_VO %d BCMC %d fifo5 %d "
	            "pkt_callback_reg_fail %d tx_prec_map %x total_tx_pkt %d\n",
	            TXPKTPENDGET(wlc, TX_AC_BK_FIFO), TXPKTPENDGET(wlc, TX_AC_BE_FIFO),
	            TXPKTPENDGET(wlc, TX_AC_VI_FIFO), TXPKTPENDGET(wlc, TX_AC_VO_FIFO),
	            TXPKTPENDGET(wlc, TX_BCMC_FIFO), TXPKTPENDGET(wlc, TX_ATIM_FIFO),
	            WLCNTVAL(wlc->pub->_cnt->pkt_callback_reg_fail), wlc->tx_prec_map,
	            wlc_txpktcnt(wlc));

	for (i = 0; i < NFIFO; i++) {
		if (wlc_bmac_xmtfifo_sz_get(wlc->hw, i, &fifo_size[i]))
			fifo_size[i] = 0;
	}
	bcm_bprintf(b, "xmtfifo_sz(in unit of 256B)");
	bcm_bprintf(b, "AC_BK %d AC_BE %d AC_VI %d AC_VO %d 5th %d 6th %d\n",
	            fifo_size[TX_AC_BK_FIFO],
	            fifo_size[TX_AC_BE_FIFO],
	            fifo_size[TX_AC_VI_FIFO],
	            fifo_size[TX_AC_VO_FIFO],
	            fifo_size[4],
	            fifo_size[5]);

	band = wlc->bandstate[IS_SINGLEBAND_5G(wlc->deviceid) ? BAND_5G_INDEX : BAND_2G_INDEX];
	bcm_bprintf(b, "(ROAMThreshold, ROAMDelta) (2.4G) default: %d, %d::current: %d, %d\n",
	            band->roam_trigger_def, band->roam_delta_def,
	            band->roam_trigger, band->roam_delta);

	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[BAND_5G_INDEX];
		bcm_bprintf(b, "(ROAMThreshold, ROAMDelta) (5G) default: %d, %d::current: %d, %d\n",
		            band->roam_trigger_def, band->roam_delta_def,
		            band->roam_trigger, band->roam_delta);
	}

	if (wlc->stf->throttle_state) {
		bcm_bprintf(b, "state:%d duty cycle:%d rxchain: %x txchain: %x\n",
		            wlc->stf->throttle_state,
		            wlc->stf->tx_duty_cycle_pwr,
		            wlc->stf->rxchain,
		            wlc->stf->txchain);
	}

	tunables = wlc->pub->tunables;
	bcm_bprintf(b, "tunables:\n");
	bcm_bprintf(b, "\tntxd = %d, nrxd = %d, rxbufsz = %d, nrxbufpost = %d, maxscb = %d\n",
	            tunables->ntxd, tunables->nrxd, tunables->rxbufsz,
	            tunables->nrxbufpost, tunables->maxscb);
	bcm_bprintf(b, "\tampdunummpdu2streams = %d, ampdunummpdu3streams = %d\n",
	            tunables->ampdunummpdu2streams, tunables->ampdunummpdu3streams);
	bcm_bprintf(b, "\tmaxpktcb = %d, maxdpt = %d, maxucodebss = %d, maxucodebss4 = %d\n",
	            tunables->maxpktcb, tunables->maxdpt,
	            tunables->maxucodebss, tunables->maxucodebss4);
	bcm_bprintf(b, "\tmaxbss = %d, datahiwat = %d, ampdudatahiwat = %d, maxubss = %d\n",
	            tunables->maxbss, tunables->datahiwat, tunables->ampdudatahiwat,
	            tunables->maxubss);
	bcm_bprintf(b, "\trxbnd = %d, txsbnd = %d\n", tunables->rxbnd, tunables->txsbnd);
#ifdef WLC_HIGH_ONLY
	bcm_bprintf(b, "\trpctxbufpost = %d\n", tunables->rpctxbufpost);
#endif // endif
#ifdef WLC_LOW_ONLY
	bcm_bprintf(b, "\tdngl_mem_restrict_rxdma = %d\n", tunables->dngl_mem_restrict_rxdma);
#endif // endif
	bcm_bprintf(b, "\tampdu_pktq_size = %d, ampdu_pktq_fav_size = %d\n",
		tunables->ampdu_pktq_size, tunables->ampdu_pktq_fav_size);

#ifdef PROP_TXSTATUS
	bcm_bprintf(b, "\twlfcfifocreditac0 = %d, wlfcfifocreditac1 = %d, wlfcfifocreditac2 = %d, "
	            "wlfcfifocreditac3 = %d\n",
	            tunables->wlfcfifocreditac0, tunables->wlfcfifocreditac1,
	            tunables->wlfcfifocreditac2, tunables->wlfcfifocreditac3);
	bcm_bprintf(b, "\twlfcfifocreditbcmc = %d, wlfcfifocreditother = %d\n",
	            tunables->wlfcfifocreditbcmc, tunables->wlfcfifocreditother);
#endif // endif
	return 0;
}

static int
wlc_dump_ratestuff(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;
	uint r;

# if defined(WL11AC)
	wlc_dump_vht_mcsmap("\nhw_vhtmap ", wlc->band->hw_rateset.vht_mcsmap, b);
# endif /* WL11AC */

#ifdef WL11N
	wlc_dump_mcsset("\nhw_mcsset ", &wlc->band->hw_rateset.mcs[0], b);
#endif // endif
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "basic_rate: ");
	for (i = 0; i < sizeof(wlc->band->basic_rate); i++)
		if ((r = wlc->band->basic_rate[i]))
			bcm_bprintf(b, "%d%s->%d%s ",
				(i / 2), (i % 2)?".5":"",
				(r / 2), (r % 2)?".5":"");
	bcm_bprintf(b, "\n");

	return 0;
}

#define	PRVAL(name)	bcm_bprintf(b, "%s %d ", #name, WLCNTVAL(wlc->pub->_cnt->name))
#define	PRNL()		bcm_bprintf(b, "\n")

#define	PRREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(wlc->osh, &regs->name))
#define PRREG_INDEX(name, reg) bcm_bprintf(b, #name " 0x%x ", R_REG(wlc->osh, &reg))

static int
wlc_dump_mac(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	d11regs_t *regs;
	char ea[ETHER_ADDR_STR_LEN];

	regs = wlc->regs;

	if (!wlc->clk)
		return BCME_NOCLK;

	PRREG(maccontrol); PRREG(maccommand); PRREG(macintstatus); PRREG(macintmask);
	bcm_bprintf(b, "\n");
	PRREG(chnstatus);
	PRREG(psmdebug); PRREG(phydebug);
	bcm_bprintf(b, "\n");

	PRREG(psm_gpio_in); PRREG(psm_gpio_out); PRREG(psm_gpio_oe);
	bcm_bprintf(b, "\n\n");

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		uint16 match_data[6];
		char *p;
		int i;

		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			W_REG(wlc->osh, &regs->u_rcv.d11regs.rcm_ctl, i);
			match_data[i] = R_REG(wlc->osh, &regs->u_rcv.d11regs.rcm_mat_data);
			match_data[i] = htol16(match_data[i]);
		}

		PRREG(biststatus);
		bcm_bprintf(b, "\n");

		/* read mac addr words from match_data[0, 2] */
		p = (char*)&match_data[0];
		bcm_bprintf(b, "MAC addr %s ", bcm_ether_ntoa((struct ether_addr*)p, ea));

		/* read mac addr words from match_data[3, 5] */
		p = (char*)&match_data[3];
		bcm_bprintf(b, "BSSID %s\n\n", bcm_ether_ntoa((struct ether_addr*)p, ea));
	} else {
		bcm_bprintf(b, "toe_capable %d toe_bypass %d ToECTL 0x%x\n",
		            wlc->toe_capable, wlc->toe_bypass,
		            R_REG(wlc->osh, &regs->u.d11acregs.ToECTL));
		/* ifsstat has the same addr as in pre-ac. it's not defined in d11acregs yet */
		bcm_bprintf(b, "aqm_rdy 0x%x framecnt 0x%x ifsstat 0x%x\n",
		            R_REG(wlc->osh, &regs->u.d11acregs.AQMFifoReady),
		            R_REG(wlc->osh, &regs->u.d11acregs.XmtFifoFrameCnt),
		            R_REG(wlc->osh, &regs->u.d11regs.ifsstat));
	}

	return 0;
}

static int
wlc_dump_pio(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i;

	if (!wlc->clk)
		return BCME_NOCLK;

	if (!PIO_ENAB(wlc->pub))
		return 0;

	for (i = 0; i < NFIFO; i++) {
		pio_t *pio = WLC_HW_PIO(wlc, i);
		bcm_bprintf(b, "PIO %d: ", i);
		if (pio != NULL)
			wlc_pio_dump(pio, b);
		bcm_bprintf(b, "\n");
	}

	return 0;
}

static int
wlc_dump_dma(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i;

	if (!wlc->clk)
		return BCME_NOCLK;

	for (i = 0; i < NFIFO; i++) {
		PRREG_INDEX(intstatus, wlc->regs->intctrlregs[i].intstatus);
		PRREG_INDEX(intmask, wlc->regs->intctrlregs[i].intmask);
		bcm_bprintf(b, "\n");
		if (!PIO_ENAB(wlc->pub)) {
			hnddma_t *di = WLC_HW_DI(wlc, i);
			bcm_bprintf(b, "DMA %d: ", i);
			if (di != NULL) {
				dma_dumptx(di, b, TRUE);
				if ((i == RX_FIFO) ||
				    (D11REV_IS(wlc->pub->corerev, 4) && (i == RX_TXSTATUS_FIFO))) {
					dma_dumprx(di, b, TRUE);
					PRVAL(rxuflo[i]);
				} else
					PRVAL(txuflo);
				PRNL();
			}
		}
		bcm_bprintf(b, "\n");
	}

	PRVAL(dmada); PRVAL(dmade); PRVAL(rxoflo); PRVAL(dmape);
	bcm_bprintf(b, "\n");

	return 0;
}

static int
wlc_dump_wme(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	d11regs_t *regs = wlc->regs;
	uint32 cwcur, cwmin, cwmax;
	uint8 qi;
	osl_t *osh;
	int idx;
	wlc_bsscfg_t *bsscfg;

	bcm_bprintf(b, "up %d EDCF %d WME %d dp 0x%x\n",
	            wlc->pub->up, EDCF_ENAB(wlc->pub), WME_ENAB(wlc->pub), wlc->wme_dp);

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_wme_t *wme = bsscfg->wme;

		qi = wme->wme_param_ie.qosinfo;

		bcm_bprintf(b, "\ncfg %d WME %d apsd %d count %d admctl 0x%x\n",
		            WLC_BSSCFG_IDX(bsscfg), BSS_WME_ENAB(wlc, bsscfg),
		            (qi & WME_QI_AP_APSD_MASK) >> WME_QI_AP_APSD_SHIFT,
		            (qi & WME_QI_AP_COUNT_MASK) >> WME_QI_AP_COUNT_SHIFT,
		            wme->wme_admctl);

		wlc_dump_edcf_acp(wlc, b, wme->wme_param_ie.acparam,
		                BSSCFG_AP(bsscfg) ? "AP" : "STA");

		if (BSSCFG_AP(bsscfg))
			wlc_dump_edcf_acp(wlc, b, bsscfg->wme->wme_param_ie_ad->acparam,
			                  "BCN/PRBRSP");
	}

	if (!EDCF_ENAB(wlc->pub))
		return BCME_OK;

	if (!wlc->pub->up)
		return BCME_OK;

	wlc_dump_wme_shm(wlc, b);

	osh = wlc->osh;

	/* read current cwcur, cwmin, cwmax */
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWMIN << 2, &cwmin,
		sizeof(cwmin), OBJADDR_SCR_SEL);
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWMAX << 2, &cwmax,
		sizeof(cwmax), OBJADDR_SCR_SEL);
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWCUR << 2, &cwcur,
		sizeof(cwcur), OBJADDR_SCR_SEL);

	bcm_bprintf(b, "xfifordy 0x%x psm_b0 0x%x txf_cur_idx 0x%x\n",
		R_REG(osh, &regs->u.d11regs.xmtfifordy), R_REG(osh, &regs->psm_base_0),
		wlc_read_shm(wlc, M_TXF_CUR_INDEX));
	bcm_bprintf(b, "cwcur 0x%x cwmin 0x%x cwmax 0x%x\n", cwcur, cwmin, cwmax);

	bcm_bprintf(b, "\n");

	return BCME_OK;
}

/* print aggregate (since driver load time) driver and macstat counter values */
static int
wlc_dump_stats(wlc_info_t *wlc, struct bcmstrbuf * b)
{
#ifdef WLCNT
	int i;
	wl_cnt_t *cnt = wlc->pub->_cnt;

	if (WLC_UPDATE_STATS(wlc)) {
		wlc_statsupd(wlc);
	}

	/* summary stat counter line */
	PRVAL(txframe); PRVAL(txbyte); PRVAL(txretrans); PRVAL(txerror);
	PRVAL(rxframe); PRVAL(rxbyte); PRVAL(rxerror); PRNL();

	PRVAL(txprshort); PRVAL(txdmawar); PRVAL(txnobuf); PRVAL(txnoassoc);
	PRVAL(txchit); PRVAL(txcmiss); PRVAL(txchanrej); PRVAL(txexptime); PRNL();

	PRVAL(reset); PRVAL(txserr); PRVAL(txphyerr); PRVAL(txphycrs);
	PRVAL(txfail); PRVAL(psmwds); PRVAL(rfdisable); PRNL();

	bcm_bprintf(b, "d11_txfrag %d d11_txmulti %d d11_txretry %d d11_txretrie %d\n",
		cnt->txfrag, cnt->txmulti, cnt->txretry, cnt->txretrie);

	bcm_bprintf(b, "d11_txrts %d d11_txnocts %d d11_txnoack %d d11_txfrmsnt %d\n",
		cnt->txrts, cnt->txnocts, cnt->txnoack, cnt->txfrmsnt);

	PRVAL(rxcrc); PRVAL(rxnobuf); PRVAL(rxnondata); PRVAL(rxbadds);
	PRVAL(rxbadcm); PRVAL(rxdup); PRVAL(rxrtry); PRVAL(rxfragerr); PRNL();

	PRVAL(rxrunt); PRVAL(rxgiant); PRVAL(rxnoscb); PRVAL(rxbadproto);
	PRVAL(rxbadsrcmac); PRNL();

	bcm_bprintf(b, "d11_rxfrag %d d11_rxmulti %d d11_rxundec %d d11_rxundec_mcst %d\n",
		cnt->rxfrag, cnt->rxmulti, cnt->rxundec, cnt->rxundec_mcst);

	PRVAL(rxctl); PRVAL(rxbadda); PRVAL(rxfilter); PRNL();

	bcm_bprintf(b, "rxuflo: ");
	for (i = 0; i < NFIFO; i++)
		bcm_bprintf(b, "%d ", cnt->rxuflo[i]);
	bcm_bprintf(b, "\n");
	PRVAL(txallfrm); PRVAL(txrtsfrm); PRVAL(txctsfrm); PRVAL(txackfrm); PRNL();
	PRVAL(txdnlfrm); PRVAL(txbcnfrm); PRVAL(txtplunfl); PRVAL(txphyerr); PRNL();
	bcm_bprintf(b, "txfunfl: ");
	for (i = 0; i < NFIFO; i++)
		bcm_bprintf(b, "%d ", cnt->txfunfl[i]);
	bcm_bprintf(b, "\n");

	/* WPA2 counters */
	PRVAL(tkipmicfaill); PRVAL(tkipicverr); PRVAL(tkipcntrmsr); PRNL();
	PRVAL(tkipreplay); PRVAL(ccmpfmterr); PRVAL(ccmpreplay); PRNL();
	PRVAL(ccmpundec); PRVAL(fourwayfail); PRVAL(wepundec); PRNL();
	PRVAL(wepicverr); PRVAL(decsuccess); PRVAL(rxundec); PRNL();

	PRVAL(tkipmicfaill_mcst); PRVAL(tkipicverr_mcst); PRVAL(tkipcntrmsr_mcst); PRNL();
	PRVAL(tkipreplay_mcst); PRVAL(ccmpfmterr_mcst); PRVAL(ccmpreplay_mcst); PRNL();
	PRVAL(ccmpundec_mcst); PRVAL(fourwayfail_mcst); PRVAL(wepundec_mcst); PRNL();
	PRVAL(wepicverr_mcst); PRVAL(decsuccess_mcst); PRVAL(rxundec_mcst); PRNL();

	PRVAL(rxfrmtoolong); PRVAL(rxfrmtooshrt); PRVAL(rxinvmachdr); PRVAL(rxbadfcs); PRNL();
	PRVAL(rxbadplcp); PRVAL(rxcrsglitch); PRVAL(rxstrt); PRVAL(rxdfrmucastmbss); PRNL();
	PRVAL(rxmfrmucastmbss); PRVAL(rxcfrmucast); PRVAL(rxrtsucast); PRVAL(rxctsucast); PRNL();
	PRVAL(rxackucast); PRVAL(rxdfrmocast); PRVAL(rxmfrmocast); PRVAL(rxcfrmocast); PRNL();
	PRVAL(rxrtsocast); PRVAL(rxctsocast); PRVAL(rxdfrmmcast); PRVAL(rxmfrmmcast); PRNL();
	PRVAL(rxcfrmmcast); PRVAL(rxbeaconmbss); PRVAL(rxdfrmucastobss); PRVAL(rxbeaconobss);
		PRNL();
	PRVAL(rxrsptmout); PRVAL(bcntxcancl); PRVAL(rxf0ovfl); PRVAL(rxf1ovfl); PRNL();
	PRVAL(rxf2ovfl); PRVAL(txsfovfl); PRVAL(pmqovfl); PRNL();
	PRVAL(rxcgprqfrm); PRVAL(rxcgprsqovfl); PRVAL(txcgprsfail); PRVAL(txcgprssuc); PRNL();
	PRVAL(prs_timeout); PRVAL(rxnack); PRVAL(frmscons); PRVAL(txnack); PRNL();
	PRVAL(txphyerror); PRVAL(txchanrej); PRNL();
	PRVAL(pktengrxducast); PRVAL(pktengrxdmcast); PRNL();
	PRVAL(txmpdu_sgi); PRVAL(rxmpdu_sgi); PRVAL(txmpdu_stbc); PRVAL(rxmpdu_stbc); PRNL();
	PRVAL(dma_hang); PRVAL(reinit); PRVAL(cso_normal); PRVAL(cso_passthrough); PRNL();

#endif /* WLCNT */
	return 0;
}

static int
wlc_dump_btc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_BTC_ID);
	return 0;
}

static int
wlc_dump_bmc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_BMC_ID);
	return 0;
}

static int
wlc_dump_obss(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int idx;
	wlc_bsscfg_t *cfg;

	bcm_bprintf(b, "num chans: %u\n", wlc->obss->num_chan);
	bcm_bprhex(b, "chanvec: ", TRUE, wlc->obss->chanvec, OBSS_CHANVEC_SIZE);
	bcm_bprhex(b, "map: ", TRUE, wlc->obss->coex_map, CH_MAX_2G_CHANNEL);

	FOREACH_AS_BSS(wlc, idx, cfg) {
		wlc_obss_info_t *obss = cfg->obss;

		bcm_bprintf(b, "\nbsscfg: %d\n", WLC_BSSCFG_IDX(cfg));
		bcm_bprintf(b, "enab: %d permit: %d detected: %u det ovrd: %u "
		            "switch_bw_def: %d bits_buf: %u "
		            "next scan: %u secs fid: %u te mask: 0x%x\n",
		            obss->coex_enab, obss->coex_permit, obss->coex_det,
		            obss->coex_ovrd,
		            obss->switch_bw_deferred, obss->coex_bits_buffered,
		            obss->scan_countdown, obss->fid_time, obss->coex_te_mask);
		bcm_bprintf(b, "param: passive dwell %u active dwell %u bss width scan %u "
		            "passive total %u active total %u chan width tran dly %u "
		            "activity threshold %u\n",
		            obss->params.passive_dwell, obss->params.active_dwell,
		            obss->params.bss_widthscan_interval,
		            obss->params.passive_total, obss->params.active_total,
		            obss->params.chanwidth_transition_dly,
		            obss->params.activity_threshold);
#ifdef STA
		/* AID */
		bcm_bprintf(b, "\nAID = 0x%x\n", cfg->AID);
#endif // endif
	}

	return 0;
}

int
wlc_dump_bss_info(const char *name, wlc_bss_info_t *bi, struct bcmstrbuf *b)
{
	char bssid[DOT11_MAX_SSID_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];

	wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	bcm_bprintf(b, "%s: %s BSSID %s\n", name, (bi->infra == 1?"Infra":(bi->infra ==
		0?"IBSS":"auto")),
		bcm_ether_ntoa(&bi->BSSID, bssid));
	bcm_bprintf(b, "SSID len %d \"%s\" ISBRCM %d ISHT %d ISVHT %d\n",
		bi->SSID_len, ssidbuf, ((bi->flags & WLC_BSS_BRCM) != 0),
		((bi->flags & WLC_BSS_HT) != 0), ((bi->flags2 & WLC_BSS_VHT) != 0));
	bcm_bprintf(b, "channel %d chanspec 0x%x beacon %d dtim %d atim %d capability 0x%04x"
		" flags 0x%02x RSSI %d\n",
		CHSPEC_CHANNEL(bi->chanspec), bi->chanspec, bi->beacon_period, bi->dtim_period,
		bi->atim_window, bi->capability, bi->flags, bi->RSSI);
	wlc_dump_rateset("rateset", &bi->rateset, b);
	if (bi->rateset.htphy_membership)
		bcm_bprintf(b, "\nmembership %d(b)",
			(bi->rateset.htphy_membership & RATE_MASK));
	if (bi->flags & WLC_BSS_HT) {
		wlc_dump_mcsset("\nmcs", &bi->rateset.mcs[0], b);
	}

# if defined(WL11AC)
	if (bi->flags2 & WLC_BSS_VHT) {
		wlc_dump_vht_mcsmap("\nvht", bi->rateset.vht_mcsmap, b);
		bcm_bprintf(b, "(rx %04x tx %04x)\n", bi->vht_rxmcsmap, bi->vht_txmcsmap);
	}
#endif /* WL11AC */

	bcm_bprintf(b, "\n");

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */

#if IOV_DUMP_REGULAR_IOVAR_ENAB
int
wlc_iovar_dump(wlc_info_t *wlc, const char *params, int p_len, char *out_buf, int out_len)
{
	struct bcmstrbuf b;
	char *name_list;
	int name_list_len;
	char *name1;
	char *name;
	const char *p;
	const char *endp;
	int err = 0;
	char *name_list_ptr;

	bcm_binit(&b, out_buf, out_len);
	p = params;
	endp = p + p_len;

	/* find the dump name list length to make a copy */
	while (p != endp && *p != '\0')
		p++;

	/* return an err if the name list was not null terminated */
	if (p == endp)
		return BCME_BADARG;

	/* copy the dump name list to a new buffer since the output buffer
	 * may be the same memory as the dump name list
	 */
	name_list_len = (int) ((const uint8*)p - (const uint8*)params + 1);
	name_list = (char*)MALLOC(wlc->osh, name_list_len);
	if (!name_list)
	      return BCME_NOMEM;
	bcopy(params, name_list, name_list_len);

	name_list_ptr = name_list;

	/* get the first two dump names */
	name1 = wlc_dump_next_name(&name_list_ptr);
	name = wlc_dump_next_name(&name_list_ptr);

	/* if the dump list was empty, return the default dump */
	if (name1 == NULL) {
		WL_ERROR(("doing default dump\n"));
#if defined(WLTEST) && defined(WLTINYDUMP)
		err = wlc_tinydump(wlc, &b);
#elif defined(BCMDBG) || defined(BCMDBG_DUMP)
		err = wlc_dump_default(wlc, &b);
#endif /* defined(WLTEST) && defined(WLTINYDUMP) */
		goto exit;
	}

	/* dump the first section
	 * only print the separator if there are more than one dump
	 */
	if (name != NULL)
		bcm_bprintf(&b, "\n%s:------\n", name1);
	err = wlc_dump_registered_name(wlc, name1, &b);
	if (err)
		goto exit;

	/* dump the rest */
	while (name != NULL) {
		bcm_bprintf(&b, "\n%s:------\n", name);
		err = wlc_dump_registered_name(wlc, name, &b);
		if (err)
			break;

		name = wlc_dump_next_name(&name_list_ptr);
	}

exit:
	MFREE(wlc->osh, name_list, name_list_len);

	/* make sure the output is at least a null terminated empty string */
	if (b.origbuf == b.buf && b.size > 0)
		b.buf[0] = '\0';

	return err;
}

static char*
wlc_dump_next_name(char **buf)
{
	char *p;
	char *name;

	p = *buf;

	if (p == NULL)
		return NULL;

	/* skip leading space */
	while (bcm_isspace(*p) && *p != '\0')
		p++;

	/* remember the name start position */
	if (*p != '\0')
		name = p;
	else
		name = NULL;

	/* find the end of the name
	 * name is terminated by space or null
	 */
	while (!bcm_isspace(*p) && *p != '\0')
		p++;

	/* replace the delimiter (or '\0' character) with '\0'
	 * and set the buffer pointer to the character past the delimiter
	 * (or to NULL if the end of the string was reached)
	 */
	if (*p != '\0') {
		*p++ = '\0';
		*buf = p;
	} else {
		*buf = NULL;
	}

	/* return the pointer to the name */
	return name;
}

/* Dump all matching the given name */
static int
wlc_dump_registered_name(wlc_info_t *wlc, char *name, struct bcmstrbuf *b)
{
	dumpcb_t *dumpcb;
	int err = 0;
	int rv = BCME_UNSUPPORTED; /* If nothing found, return this. */

	/* find the given dump name */
	for (dumpcb = wlc->dumpcb_head; dumpcb != NULL; dumpcb = dumpcb->next) {
		if (!strcmp(name, dumpcb->name)) {
			if (rv == BCME_UNSUPPORTED) { /* Found one */
				rv = BCME_OK;
			}
			err = dumpcb->dump_fn(dumpcb->dump_fn_arg, b);
			if (b->size == 0) { /* check for output buffer overflow */
				rv = BCME_BUFTOOSHORT;
				break;
			}
			if (err != 0) { /* Record last non successful error code */
				rv = err;
			}
		}
	}

	return rv;
}
#endif /* IOV_DUMP_REGULAR_IOVAR_ENAB */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
/* Print out the AC Params in an IE */
static void
wlc_dump_edcf_acp(wlc_info_t *wlc, struct bcmstrbuf *b, edcf_acparam_t *acp_ie, const char *desc)
{
	int ac;

	bcm_bprintf(b, "\nEDCF params for %s\n", desc);

	for (ac = 0; ac < AC_COUNT; ac++, acp_ie++) {
		bcm_bprintf(b,
		               "%s: ACI 0x%02x ECW 0x%02x "
		               "(aci %d acm %d aifsn %d ecwmin %d ecwmax %d txop 0x%x)\n",
		               aci_names[ac],
		               acp_ie->ACI, acp_ie->ECW,
		               (acp_ie->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT,
		               (acp_ie->ACI & EDCF_ACM_MASK) ? 1 : 0,
		               acp_ie->ACI & EDCF_AIFSN_MASK,
		               acp_ie->ECW & EDCF_ECWMIN_MASK,
		               (acp_ie->ECW & EDCF_ECWMAX_MASK) >> EDCF_ECWMAX_SHIFT,
		               ltoh16(acp_ie->TXOP));
	}

	bcm_bprintf(b, "\n");
}

/* Print out the AC Params in use by ucode */
static void
wlc_dump_wme_shm(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int ac;

	bcm_bprintf(b, "\nEDCF params in shared memory\n");

	for (ac = 0; ac < AC_COUNT; ac++) {
		shm_acparams_t shm_acp;
		uint16 *element;
		int j;

		element = (uint16 *)&shm_acp;

		/* fill in the ac param element from the shm locations */
		for (j = 0; j < (int)sizeof(shm_acparams_t); j += 2) {
			uint offset = M_EDCF_QINFO + wme_shmemacindex(ac) * M_EDCF_QLEN + j;
			*element++ = wlc_read_shm(wlc, offset);
		}

		bcm_bprintf(b, "%s: txop 0x%x cwmin 0x%x cwmax 0x%x cwcur 0x%x\n"
		               "       aifs 0x%x bslots 0x%x reggap 0x%x status 0x%x\n",
		               aci_names[ac],
		               shm_acp.txop, shm_acp.cwmin, shm_acp.cwmax, shm_acp.cwcur,
		               shm_acp.aifs, shm_acp.bslots, shm_acp.reggap, shm_acp.status);
	}

	bcm_bprintf(b, "\n");
}
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */

#if defined(BCMDBG_TXSTUCK)
static void
wlc_dump_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "ampdu:\n");
	wlc_ampdu_print_txstuck(wlc, b);
	bcm_bprintf(b, "wlc:\n");
	bcm_bprintf(b, "wlc active q: %d, PM pending %d\n", pktq_len(&(wlc->active_queue->q)),
		wlc->PMpending);
	bcm_bprintf(b, "total tx: %d, hw fifo tx pending: %d\n", wlc_txpktcnt(wlc),
		TXPKTPENDTOT(wlc));
	bcm_bprintf(b, "block 0x%x block_D3 0x%x\n", wlc->block_datafifo, wlc->block_datafifo_D3);
#ifdef WL_MULTIQUEUE
	if (wlc->txfifo_detach_pending) {
		bcm_bprintf(b, "wlc->txfifo_detach_pending %d\n",
			wlc->txfifo_detach_pending);
	}
	if (wlc->primary_queue == wlc->active_queue) {
		bcm_bprintf(b, "primary queue == active_queue\n");
	}
	bcm_bprintf(b, "excursion active: %d\n", wlc->excursion_active);
#endif /* WL_MULTIQUEUE */
	bcm_bprintf(b, "wlc_bsscfg:\n");
	wlc_bsscfg_print_txstuck(wlc, b);
	bcm_bprintf(b, "wlc_apps:\n");
	wlc_apps_print_scb_psinfo_txstuck(wlc, b);
	bcm_bprintf(b, "wlc_bmac:\n");
	wlc_bmac_print_muted(wlc, b);
}
#endif /* defined(BCMDBG_TXSTUCK) */

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
/*
* This function lists all dump option.
*/
static int
wlc_dump_list(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	dumpcb_t *ptr;

	bcm_bprintf(b, "\nRegistered dumps:\n");

	for (ptr = wlc->dumpcb_head; ptr != NULL; ptr = ptr->next) {
		bcm_bprintf(b, "%s\n", ptr->name);
	}

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */
#if defined(WLTINYDUMP)
static int
wlc_tinydump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	char perm[32], cur[32];
	char ssidbuf[SSID_FMT_BUF_LEN];
	int i;
	wlc_bsscfg_t *bsscfg;

	/* XXX: Remove after 5/20/06 when wl-apps are upgraded to latest.
	 * There's a separate path (IOV_VER) for version.
	 */
	wl_dump_ver(wlc->wl, b);

	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "resets %d\n", WLCNTVAL(wlc->pub->_cnt->reset));

	bcm_bprintf(b, "perm_etheraddr %s cur_etheraddr %s\n",
		bcm_ether_ntoa(&wlc->perm_etheraddr, perm),
		bcm_ether_ntoa(&wlc->pub->cur_etheraddr, cur));

	bcm_bprintf(b, "board 0x%x, board rev %s", wlc->pub->sih->boardtype,
	            bcm_brev_str(wlc->pub->boardrev, cur));
	if (wlc->pub->boardrev == BOARDREV_PROMOTED)
		bcm_bprintf(b, " (actually 0x%02x)", BOARDREV_PROMOTABLE);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "rate_override: A %d, B %d\n",
		wlc->bandstate[BAND_5G_INDEX]->rspec_override,
		wlc->bandstate[BAND_2G_INDEX]->rspec_override);

	bcm_bprintf(b, "ant_rx_ovr %d txant %d\n", wlc->stf->ant_rx_ovr, wlc->stf->txant);

	FOREACH_BSS(wlc, i, bsscfg) {
		char ifname[32];

		bcm_bprintf(b, "\n");

		wlc_format_ssid(ssidbuf, bsscfg->SSID, bsscfg->SSID_len);
		strncpy(ifname, wl_ifname(wlc->wl, bsscfg->wlcif->wlif), sizeof(ifname));
		ifname[sizeof(ifname) - 1] = '\0';
		bcm_bprintf(b, "BSS Config %d: \"%s\"\n", i, ssidbuf);

		bcm_bprintf(b, "enable %d up %d wlif 0x%p \"%s\"\n",
		            bsscfg->enable,
		            bsscfg->up, bsscfg->wlcif->wlif, ifname);
		bcm_bprintf(b, "wsec 0x%x auth %d wsec_index %d wep_algo %d\n",
		            bsscfg->wsec,
		            bsscfg->auth, bsscfg->wsec_index,
		            WSEC_BSS_DEFAULT_KEY(bsscfg) ?
		            WSEC_BSS_DEFAULT_KEY(bsscfg)->algo : 0);

		bcm_bprintf(b, "current_bss->BSSID %s\n",
		            bcm_ether_ntoa(&bsscfg->current_bss->BSSID, (char*)perm));

		wlc_format_ssid(ssidbuf, bsscfg->current_bss->SSID,
		                bsscfg->current_bss->SSID_len);
		bcm_bprintf(b, "current_bss->SSID \"%s\"\n", ssidbuf);

#ifdef STA
		/* STA ONLY */
		if (!BSSCFG_STA(bsscfg))
			continue;

		bcm_bprintf(b, "bsscfg %d assoc_state %d\n", WLC_BSSCFG_IDX(bsscfg),
		            bsscfg->assoc->state);
#endif /* STA */
	}
	bcm_bprintf(b, "\n");

#ifdef STA
	bcm_bprintf(b, "AS_IN_PROGRESS() %d stas_associated %d\n", AS_IN_PROGRESS(wlc),
	            wlc->stas_associated);
#endif /* STA */

	bcm_bprintf(b, "aps_associated %d\n", wlc->aps_associated);
	FOREACH_UP_AP(wlc, i, bsscfg)
	        bcm_bprintf(b, "BSSID %s\n", bcm_ether_ntoa(&bsscfg->BSSID, (char*)perm));

	return 0;
}
#endif /* WLTINYDUMP */

#if defined(BCMDBG_DUMP) || defined(WLTEST)
static int
wlc_pcieinfo_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PCIEINFO_ID);
	return 0;
}
#endif // endif

#if defined(BCMDBG_DUMP)
static int
wlc_gpio_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_GPIO_ID);
	return 0;
}

static int
wlc_dump_siid(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_SI_ID);
	return 0;
}

static int
wlc_dump_siclk(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_SICLK_ID);
	return 0;
}

static int
wlc_dump_sireg(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_SIREG_ID);
	return 0;
}

static int
wlc_dump_ccreg(wlc_info_t *wlc, struct bcmstrbuf *b)
{

	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_CCREG_ID);
	return 0;
}

static int
wlc_pciereg_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_bmac_dump(wlc->hw, b, BMAC_DUMP_PCIEREG_ID);
	return 0;
}

static int
wlc_dump_secalgo(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;

	if (!wlc->clk)
		return BCME_NOCLK;

	for (i = 0; i < WLC_MAX_WSEC_HW_KEYS(wlc); i ++) {
		uint16 v16 = wlc_read_shm(wlc, M_SECKINDXALGO_BLK + (i * 2));
		if ((v16 & SKL_ALGO_MASK) != CRYPTO_ALGO_OFF)
			bcm_bprintf(b, "%d %04x\n", i, v16);
	}
	return 0;
}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#if defined(WLC_HIGH_ONLY) && defined(BCMDBG)
static int
wlc_dump_rpcpktlog(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint32 buf[RPC_PKTLOG_DUMP_SIZE];
	int ret;
	int i;

	ASSERT(wlc->rpc);

	ret = bcm_rpc_pktlog_get(wlc->rpc, buf, RPC_PKTLOG_DUMP_SIZE, TRUE);
	if (ret < 0)
		return ret;

	bcm_bprintf(b, "Transmit log %d:\n", ret);
	for (i = 0; i < ret; i++) {
		bcm_bprintf(b, "[%d] trans 0x%x len %d ID %s\n", i,
			buf[i*RPC_PKTLOG_RD_LEN+1], buf[i*RPC_PKTLOG_RD_LEN+2],
			WLC_RPC_ID_LOOKUP(rpc_name_tbl, buf[i*RPC_PKTLOG_RD_LEN]));
	}

	ret = bcm_rpc_pktlog_get(wlc->rpc, buf, RPC_PKTLOG_DUMP_SIZE, FALSE);

	if (ret <= 0)
		return ret;

	bcm_bprintf(b, "Recv log %d:\n", ret);
	for (i = 0; i < ret; i++) {
		bcm_bprintf(b, "[%d] trans 0x%x len %d ID %s\n", i,
			buf[i*RPC_PKTLOG_RD_LEN+1], buf[i*RPC_PKTLOG_RD_LEN+2],
			WLC_RPC_ID_LOOKUP(rpc_name_tbl, buf[i*RPC_PKTLOG_RD_LEN]));
	}

	return 0;
}

static int
wlc_dump_rpc(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	return bcm_rpc_dump(wlc->rpc, b);
}
#endif /* WLC_HIGH_ONLY && BCMDBG */

static int
wlc_dump_pm(wlc_info_t *wlc, struct bcmstrbuf *b)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
	bcm_bprintf(b, "STAY_AWAKE() %d "
	            "SCAN_IN_PROGRESS() %d WLC_RM_IN_PROGRESS() %d AS_IN_PROGRESS() %d "
	            "wlc->wake %d wlc->PMpending %d wlc->PSpoll %d wlc->apsd_sta_usp %d\n",
	            STAY_AWAKE(wlc),
	            SCAN_IN_PROGRESS(wlc->scan), WLC_RM_IN_PROGRESS(wlc), AS_IN_PROGRESS(wlc),
	            wlc->wake, wlc->PMpending, wlc->PSpoll, wlc->apsd_sta_usp);
	bcm_bprintf(b, "wlc->PMawakebcn %d wlc->txpend16165war %d "
	            "wlc->check_for_unaligned_tbtt %d\n",
	            wlc->PMawakebcn, wlc->txpend16165war,
	            wlc->check_for_unaligned_tbtt);
	bcm_bprintf(b, "wlc->gptimer_stay_awake_req %d wlc->pm2_radio_shutoff_pending %d"
	            " wlc->user_wake_req %d BTA_ACTIVE() %d\n",
	            wlc->gptimer_stay_awake_req, wlc->pm2_radio_shutoff_pending, wlc->user_wake_req,
	            BTA_ACTIVE(wlc));
#ifdef WL11K
	bcm_bprintf(b, "wlc_rrm_inprog %d\n", wlc_rrm_inprog(wlc));
#endif // endif
	FOREACH_BSS(wlc, idx, cfg) {
		wlc_pm_st_t *pm = cfg->pm;
		if (BSSCFG_STA(cfg) && (cfg->associated || BSS_TDLS_ENAB(wlc, cfg))) {
			bcm_bprintf(b, "bsscfg %d BSS %d PS_ALLOWED() %d WLC_PORTOPEN() %d "
		            "dtim_programmed %d PMpending %d priorPMstate %d PMawakebcn %d "
		            "WME_PM_blocked %d PM %d PMenabled %d PSpoll %d apsd_sta_usp %d "
		            "check_for_unaligned_tbtt %d PMblocked 0x%x\n",
		            WLC_BSSCFG_IDX(cfg), cfg->BSS, PS_ALLOWED(cfg), WLC_PORTOPEN(cfg),
		            cfg->dtim_programmed, pm->PMpending, pm->priorPMstate, pm->PMawakebcn,
		            pm->WME_PM_blocked, pm->PM, pm->PMenabled, pm->PSpoll, pm->PMpending,
		            pm->check_for_unaligned_tbtt, pm->PMblocked);
		}
	}
#endif /* STA */

	return BCME_OK;
}

static int
wlc_dump_htcap(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint16 val = wlc->ht_cap.cap;
	uint16 stbc_val, mimo_ps_val;

	if ((wlc_channel_locale_flags(wlc->cmi) & WLC_NO_40MHZ) ||
	    (!WL_BW_CAP_40MHZ(wlc->band->bw_cap)) ||
	    (wlc->cfg != NULL &&
	     (BSSCFG_AP(wlc->cfg) || (!wlc->cfg->BSS && !BSSCFG_IS_TDLS(wlc->cfg))) &&
	     CHSPEC_IS20(wlc->cfg->current_bss->chanspec))) {
		val &= ~HT_CAP_40MHZ;
		val &= ~HT_CAP_SHORT_GI_40;
	}

	bcm_bprintf(b, "HT dump:\n");

	bcm_bprintf(b, "HT Cap 0X%04x\n", wlc->ht_cap.cap);
	if (val & HT_CAP_LDPC_CODING)
		bcm_bprintf(b, "LDPC ");
	if (val & HT_CAP_40MHZ)
		bcm_bprintf(b, "40MHz ");

	mimo_ps_val = (val & HT_CAP_MIMO_PS_MASK) >> HT_CAP_MIMO_PS_SHIFT;
	if (mimo_ps_val == HT_CAP_MIMO_PS_ON)
		bcm_bprintf(b, "MIMO-PS-ON ");
	else if (mimo_ps_val == HT_CAP_MIMO_PS_RTS)
		bcm_bprintf(b, "MIMO-PS-RTS ");
	else if (mimo_ps_val == HT_CAP_MIMO_PS_OFF)
		bcm_bprintf(b, "MIMO-PS-OFF ");

	if (val & HT_CAP_GF)
		bcm_bprintf(b, "GF ");
	if (val & HT_CAP_SHORT_GI_20)
		bcm_bprintf(b, "SGI-20 ");
	if (val & HT_CAP_SHORT_GI_40)
		bcm_bprintf(b, "SGI-40 ");
	if (val & HT_CAP_TX_STBC)
		bcm_bprintf(b, "STBC-TX ");

	stbc_val = (val & HT_CAP_RX_STBC_MASK) >> HT_CAP_RX_STBC_SHIFT;
	if (stbc_val == HT_CAP_RX_STBC_ONE_STREAM)
		bcm_bprintf(b, "STBC-RX-1SS ");
	else if (stbc_val == HT_CAP_RX_STBC_TWO_STREAM)
		bcm_bprintf(b, "STBC-RX-2SS ");
	else if (stbc_val == HT_CAP_RX_STBC_THREE_STREAM)
		bcm_bprintf(b, "STBC-RX-3SS ");

	if (val & HT_CAP_DELAYED_BA)
		bcm_bprintf(b, "Delay-BA ");
	if (val & HT_CAP_MAX_AMSDU)
		bcm_bprintf(b, "AMSDU-Max ");
	if (val & HT_CAP_DSSS_CCK)
		bcm_bprintf(b, "DSSS-CCK ");
	if (val & HT_CAP_PSMP)
		bcm_bprintf(b, "PSMP ");
	if (val & HT_CAP_40MHZ_INTOLERANT)
		bcm_bprintf(b, "40-Intol ");
	if (val & HT_CAP_LSIG_TXOP)
		bcm_bprintf(b, "LSIG-TXOP ");
	wlc_dump_mcsset("\nhw_mcsset ", &wlc->band->hw_rateset.mcs[0], b);
	bcm_bprintf(b, "\n");
	return 0;
}

#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */

#if defined(WLC_LOW) && defined(WLC_HIGH) && (defined(BCMDBG) || defined(BCMDBG_DUMP) \
	|| defined(MCHAN_MINIDUMP))
static int
wlc_dump_chanswitch(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i, j, nsamps, contexts;
	uint band;
	chanswitch_times_t *history = NULL;
	uint32 diff, total;
	uint us, ms, ts;
	char chanbuf[CHANSPEC_STR_LEN];
	char chanbuf1[CHANSPEC_STR_LEN];
	const char *chanswitch_history_names[] = {
		"wlc_set_chanspec",
		"wlc_adopt_context",
		"wlc_preppm_context"
	};

	if (ARRAYSIZE(chanswitch_history_names) != CHANSWITCH_LAST) {
		WL_ERROR(("%s: num_labels needs to match num of events!\n", __FUNCTION__));
		return -1;
	}

	for (contexts = 0; contexts < CHANSWITCH_LAST; contexts++) {
		bcm_bprintf(b, "**** %s  **** \n", chanswitch_history_names[contexts]);

		for (band = 0; band < NBANDS(wlc); band++) {
			if (band == 0) {
				history = wlc->chansw_hist[contexts];
				bcm_bprintf(b, "Channelswitch:      Duration"
					"          timestamp\n");
			} else {
				history = wlc->bandsw_hist[contexts];
				bcm_bprintf(b, "Bandswitch:         Duration"
					"          timestamp\n");
			}
			ASSERT(history);
			j = history->index % CHANSWITCH_TIMES_HISTORY;
			total = 0;
			nsamps = 0;
			for  (i = 0; i < CHANSWITCH_TIMES_HISTORY; i++) {
				diff = history->entry[j].end - history->entry[j].start;
				if (diff) {
					us = (diff % TSF_TICKS_PER_MS) * 1000 / TSF_TICKS_PER_MS;
					ms = diff / TSF_TICKS_PER_MS;
					total += diff;
					nsamps++;

					ts = history->entry[j].start / TSF_TICKS_PER_MS;

					bcm_bprintf(b, "%-6s => %-6s"
					"      %2.2d.%03u             %03u\n",
					wf_chspec_ntoa(history->entry[j].from, chanbuf),
					wf_chspec_ntoa(history->entry[j].to, chanbuf1),
					ms, us, ts);
				}
				j = (j + 1) % CHANSWITCH_TIMES_HISTORY;
			}
			if (nsamps) {
				total /= nsamps;
				us = (total % TSF_TICKS_PER_MS) * 1000 / TSF_TICKS_PER_MS;
				ms = total / TSF_TICKS_PER_MS;
				bcm_bprintf(b, "%s: %s: Avg %d.%03u Millisecs, %d Samples\n\n",
					chanswitch_history_names[contexts],
					band ? "Bandswitch" : "Channelswitch",
					ms, us, nsamps);
			} else {
				bcm_bprintf(b, "  -                   -                   -\n");
			}
		}
	}
	return 0;
}
#endif // endif

#ifdef BCMDBG
static int
wlc_dump_perf_stats(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_perf_stats_t *cnt = &wlc->perf_stats;
	wlc_isr_stats_t *ints = &cnt->isr_stats;
	uint32 i;

	const char * int_names[32] =
	{
		"MACSSPNDD     ",
		"BCNTPL        ",
		"TBTT          ",
		"BCNSUCCESS    ",
		"BCNCANCLD     ",
		"ATIMWINEND    ",
		"PMQ           ",
		"NSPECGEN_0    ",
		"NSPECGEN_1    ",
		"MACTXERR      ",
		"NSPECGEN_3    ",
		"PHYTXERR      ",
		"PME           ",
		"GP0           ",
		"GP1           ",
		"DMAINT        ",
		"TXSTOP        ",
		"CCA           ",
		"BG_NOISE      ",
		"DTIM_TBTT     ",
		"PRQ           ",
		"PWRUP         ",
		"BT_RFACT_STUCK",
		"BT_PRED_REQ   ",
		"INT_24        ",
		"P2P           ",
		"INT_26        ",
		"INT_27        ",
		"RFDISABLE     ",
		"TFS           ",
		"PHYCHANGED    ",
		"TO            "
	};

#ifdef WLP2P
	const char * p2p_int_names[32] =
	{
		"PRE_TBTT",
		"CTW_END ",
		"ABS     ",
		"PRS     "
	};
#endif // endif
	/* Print perf stats */

	bcm_bprintf(b, "\nGeneral Performance Stats:-\n");

	bcm_bprintf(b,
		"\nisr        : %d"
		"\ndpc        : %d"
		"\ntimer dpc  : %d"
		"\nbcn isr    : %d"
		"\nbeacons    : %d"
		"\nprobe req  : %d"
		"\nprobe resp : %d\n",
			cnt->n_isr, cnt->n_dpc, cnt->n_timer_dpc,
			cnt->n_bcn_isr, cnt->n_beacons, cnt->n_probe_req, cnt->n_probe_resp);

	bcm_bprintf(b, "\nInterrupt       num  percent");

	for (i = 0; i < 32; i++) {
		if (ints->n_counts[i]) {
			bcm_bprintf(b, "\n%s	%d	%d", int_names[i], ints->n_counts[i],
				((ints->n_counts[i])*100)/(cnt->n_isr));
		}
	}
	bcm_bprintf(b, "\n");

#ifdef WLP2P
	if (P2P_ENAB(wlc->pub) && ints->n_counts[25]) {
		bcm_bprintf(b, "\nP2P Interrupt   num  percent");

		for (i = 0; i < M_P2P_I_BLK_SZ; i++) {
			bcm_bprintf(b, "\n%s	%d	%d", p2p_int_names[i], ints->n_p2p[i],
				((ints->n_p2p[i])*100)/(ints->n_counts[25]));
		}
		bcm_bprintf(b, "\n");
	}
#endif // endif

	return BCME_OK;
}

#endif /* BCMDBG */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
#define	PRVAL(name)	bcm_bprintf(b, "%s %d ", #name, WLCNTVAL(wlc->pub->_cnt->name))
#define	PRNL()		bcm_bprintf(b, "\n")

#define	PRREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(wlc->osh, &regs->name))
#define PRREG_INDEX(name, reg) bcm_bprintf(b, #name " 0x%x ", R_REG(wlc->osh, &reg))

static void
wlc_dump_bcntpl(wlc_info_t *wlc, struct bcmstrbuf *b, int offset, int len)
{
	d11regs_t *regs;
	osl_t *osh;
	uint16 val;
	uint i;

	regs = wlc->regs;
	osh = wlc->osh;

	len = (len + 3) & ~3;

	bcm_bprintf(b, "tpl: offset %d len %d\n", offset, len);

	while ((val = R_REG(osh, &regs->u.d11regs.xmttplateptr)) & 3)
		printf("read_txe_ram: polling xmttplateptr 0x%x\n", val);

	for (i = 0; i < (uint)len; i += 4) {
		W_REG(osh, &regs->u.d11regs.xmttplateptr, (offset + i) | 2);
		while ((val = R_REG(osh, &regs->u.d11regs.xmttplateptr)) & 3)
			printf("read_txe_ram: polling xmttplateptr 0x%x\n", val);
		bcm_bprintf(b, "%04X: %04X%04X\n", i,
		            R_REG(osh, &regs->u.d11regs.xmttplatedatahi),
		            R_REG(osh, &regs->u.d11regs.xmttplatedatalo));
	}
}

static int
wlc_dump_bcntpls(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint16 len;
	uint shm_bcn0_tpl_base, shm_bcn1_tpl_base;

	/* if multiband, band must be set */
	if (IS_MBAND_UNLOCKED(wlc)) {
		WL_ERROR(("wl%d: %s: band must be locked\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOTBANDLOCKED;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: clock must be on\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOCLK;
	}
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		shm_bcn0_tpl_base = D11AC_T_BCN0_TPL_BASE;
		shm_bcn1_tpl_base = D11AC_T_BCN1_TPL_BASE;
	} else {
		shm_bcn0_tpl_base = D11_T_BCN0_TPL_BASE;
		shm_bcn1_tpl_base = D11_T_BCN1_TPL_BASE;
	}

	len = wlc_read_shm(wlc, M_BCN0_FRM_BYTESZ);
	bcm_bprintf(b, "bcn 0: len %u\n", len);
	wlc_dump_bcntpl(wlc, b, shm_bcn0_tpl_base, len);
	len = wlc_read_shm(wlc, M_BCN1_FRM_BYTESZ);
	bcm_bprintf(b, "bcn 1: len %u\n", len);
	wlc_dump_bcntpl(wlc, b, shm_bcn1_tpl_base, len);

	return 0;
}
#endif	/* BCMDBG || BCMDBG_DUMP */

#if defined(BCMDBG_DUMP)
typedef struct _shmem_list {
	uint16	start;
	uint16	end;
} shmem_list_t;

static int
wlc_dump_shmem(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;
	uint16 val, addr;
	uint max_hw_keys;

	static const shmem_list_t shmem_list[] = {
		{0x0,	0x80},
		{0x100, 0x500},
		{0xbb6, 0xbc4}
	};

	/* if multiband, band must be set */
	if (IS_MBAND_UNLOCKED(wlc)) {
		WL_ERROR(("wl%d: %s: band must be locked\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOTBANDLOCKED;
	}
	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: clock must be on\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOCLK;
	}
	for (i = 0; i < ARRAYSIZE(shmem_list); i++) {
		for (addr = shmem_list[i].start; addr < shmem_list[i].end; addr += 2) {
			val = wlc_read_shm(wlc, addr);
			bcm_bprintf(b, "0x%03x * 2: 0x%03x 0x%04x\n", addr >> 1,
			            addr, val);
		}
	}

	/* Dump key block and the preceeding 64 bytes */
	max_hw_keys = WLC_MAX_WSEC_HW_KEYS(wlc) + 4;
	for (addr = wlc->seckeys - 64;
		addr < wlc->seckeys + max_hw_keys * D11_MAX_KEY_SIZE; addr += 2) {
		val = wlc_read_shm(wlc, addr);
		if (!val)
			continue;
		bcm_bprintf(b, "0x%03x * 2: 0x%03x 0x%04x\n", addr >> 1, addr, val);
	}

	return 0;
}

static int
wlc_dump_sctpl(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	d11regs_t *regs;
	osl_t *osh;
	uint16 val;
	uint i;
	int gpio_sel;
	uint16 phyctl, addr0, addr1, curptr, len, offset;

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		WL_ERROR(("wl%d: %s only supported for corerev >=40\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: clock must be on\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOCLK;
	}

	regs = wlc->regs;
	osh = wlc->osh;
	phyctl = R_REG(osh, &regs->psm_phy_hdr_param);

	/* stop sample capture */
	W_REG(osh, &regs->psm_phy_hdr_param, phyctl & ~(1 << 4));

	gpio_sel = R_REG(osh, &regs->maccontrol1);
	addr0 = R_REG(osh, &regs->u.d11acregs.SampleCollectStartPtr);
	addr1 = R_REG(osh, &regs->u.d11acregs.SampleCollectStopPtr);
	curptr = R_REG(osh, &regs->u.d11acregs.SampleCollectCurPtr);
	len = (addr1 - addr0 + 1) * 4;
	offset = addr0 * 4;

	if (b) {
		bcm_bprintf(b, "Capture mode: maccontrol1 0x%02x phyctl 0x%02x\n",
			gpio_sel, phyctl);
		bcm_bprintf(b, "Start/stop/cur 0x%04x 0x%04x 0x%04x byt_offset 0x%04x entries %u\n",
			addr0, addr1, curptr, 4 *(curptr - addr0), len>>2);
		bcm_bprintf(b, "offset: low high\n");
	} else {
		printf("Capture mode: maccontrol1 0x%02x phyctl 0x%02x\n", gpio_sel, phyctl);
		printf("Start/stop/cur 0x%04x 0x%04x 0x%04x byt_offset 0x%04x entries %u\n",
		       addr0, addr1, curptr, 4 *(curptr - addr0), len>>2);
		printf("offset: low high\n");
	}

	while ((val = R_REG(osh, &regs->u.d11acregs.XmtTemplatePtr)) & 3)
		printf("read_txe_ram: polling XmtTemplatePtr 0x%x\n", val);

	for (i = 0; i < (uint)len; i += 4) {
		uint16 low, hiw;
		W_REG(osh, &regs->u.d11acregs.XmtTemplatePtr, (offset + i) | 2);
		while ((val = R_REG(osh, &regs->u.d11acregs.XmtTemplatePtr)) & 3)
			printf("read_txe_ram: polling XmtTemplatePtr 0x%x\n", val);
		hiw = R_REG(osh, &regs->u.d11acregs.XmtTemplateDataHi);
		low = R_REG(osh, &regs->u.d11acregs.XmtTemplateDataLo);
		if (b)
			bcm_bprintf(b, "%04X: %04X %04X\n", i, low, hiw);
		else
			printf("%04X: %04X %04X\n", i, low, hiw);
	}
	return BCME_OK;
}
#endif // endif

#if defined(BCMDBG_DUMP)
static int
wlc_tsf_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	d11regs_t *regs;
	uint32 tsf_hi, tsf_lo;
	uint32 tsf_cfprep, tsf_cfpstart;
	osl_t *osh;

	/* Entry function checks clk due to IOVF_GET_CLK flag */
	if (!wlc->clk)
		return BCME_NOCLK;

	regs = wlc->regs;
	osh = wlc->osh;

	wlc_read_tsf(wlc, &tsf_lo, &tsf_hi);

	tsf_cfprep = R_REG(osh, &regs->tsf_cfprep);
	tsf_cfpstart = R_REG(osh, &regs->tsf_cfpstart);
	bcm_bprintf(b, "TSF: 0x%08x 0x%08x CFPSTART: 0x%08x CFPREP: 0x%08x\n",
		tsf_hi, tsf_lo, tsf_cfpstart, tsf_cfprep);

	return 0;
}
#endif // endif

#if defined(WLTEST) || defined(BCMDBG_DUMP)
static int
wlc_nvram_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	char *nvram_vars;
	const char *q = NULL;
	int err;

	/* per-device vars first, if any */
	if (wlc->pub->vars) {
		q = wlc->pub->vars;
		/* loop to copy vars which contain null separated strings */
		while (*q != '\0') {
			bcm_bprintf(b, "%s\n", q);
			q += strlen(q) + 1;
		}
	}
	/* followed by global nvram vars second, if any */
	if ((nvram_vars = MALLOC(wlc->osh, MAXSZ_NVRAM_VARS)) == NULL) {
		err = BCME_NOMEM;
		goto exit;
	}
	if ((err = nvram_getall(nvram_vars, MAXSZ_NVRAM_VARS)) != BCME_OK)
		goto exit;
	if (nvram_vars[0]) {
		q = nvram_vars;
		/* loop to copy vars which contain null separated strings */
		while (((q - nvram_vars) < MAXSZ_NVRAM_VARS) && *q != '\0') {
			bcm_bprintf(b, "%s\n", q);
			q += strlen(q) + 1;
		}
	}

	/* check empty nvram */
	if (q == NULL)
		err = BCME_NOTFOUND;
exit:
	if (nvram_vars)
		MFREE(wlc->osh, nvram_vars, MAXSZ_NVRAM_VARS);

	return err;
}
#endif // endif

#if defined(BCMDBG_DUMP)
#ifdef SMF_STATS
static void
wlc_dump_smfs_type(wlc_smf_stats_t *smf_stats, struct bcmstrbuf *b)
{
	static const struct {uint8 type; char name[32];} type_names[] = {
		{SMFS_TYPE_AUTH, "Authentication_Request"},
		{SMFS_TYPE_ASSOC, "Association_Request"},
		{SMFS_TYPE_REASSOC, "Reassociation_Request"},
		{SMFS_TYPE_DISASSOC_TX, "Disassociation_Request_TX"},
		{SMFS_TYPE_DISASSOC_RX, "Disassociation_Request_RX"},
		{SMFS_TYPE_DEAUTH_TX, "Deauthentication_Request_TX"},
		{SMFS_TYPE_DEAUTH_RX, "Deauthentication_Request_RX"}
	};
	const char *tname = "UNKNOWN";
	uint i;

	for (i = 0; i < ARRAYSIZE(type_names); i++) {
		if (type_names[i].type == smf_stats->smfs_main.type)
		    tname = type_names[i].name;
	}

	bcm_bprintf(b, "\tFrame Type: ");
	bcm_bprintf(b, "%s\n", tname);
}

static void
wlc_dump_smf_stats(wlc_smf_stats_t *smf_stats, struct bcmstrbuf *b)
{
	wlc_smfs_elem_t *elemt;

	ASSERT(smf_stats);

	wlc_dump_smfs_type(smf_stats, b);
	bcm_bprintf(b, "\tIgnored Count: %d\n", smf_stats->smfs_main.ignored_cnt);
	bcm_bprintf(b, "\tMalformed Count: %d\n", smf_stats->smfs_main.malformed_cnt);
	bcm_bprintf(b, "\tSuccessful/Failed Count with status or reason code:\n");

	elemt = smf_stats->stats;

	while (elemt) {
		bcm_bprintf(b, "\t\t SC/RC: %d Count: %d\n",
		  elemt->smfs_elem.code, elemt->smfs_elem.count);
		elemt = elemt->next;
	}
	bcm_bprintf(b, "\n");
}

static void
wlc_dump_bss_smfs(wlc_bsscfg_t *cfg, int bsscfg_idx, struct bcmstrbuf *b)
{
	char bssbuf[ETHER_ADDR_STR_LEN];
	int i;

	bcm_bprintf(b, "BSS Config %d: BSSID: %s\n", bsscfg_idx,
		bcm_ether_ntoa(&cfg->BSSID, bssbuf));

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		wlc_dump_smf_stats(&cfg->smfs_info->smf_stats[i], b);
	}
}

static int
wlc_dump_smfs(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;

	bcm_bprintf(b, "Selected Management Frame Stats for each BSS:\n");
	for (i = 0; i < WLC_MAXBSSCFG; i++) {
		if (wlc->bsscfg[i] == NULL)
			continue;
		bcm_bprintf(b, "\n");
		wlc_dump_bss_smfs(wlc->bsscfg[i], i, b);
	}

	return BCME_OK;
}
#endif /* SMF_STATS */
#endif // endif

#if defined(BCMDBG_MEM) && defined(BCMDBG)
static int
wlc_malloc_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	MALLOC_DUMP(wlc->osh, b);

	return 0;
}
#endif /* defined(BCMDBG_MEM) && defined(BCMDBG) */

static void
wlc_dump_register_phy(wlc_info_t *wlc)
{
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
#if defined(BCMDBG_PHYDUMP)
	wlc_dump_register(wlc->pub, "phyreg", (dump_fn_t)wlc_dump_phy_phyreg, (void *)wlc);
	/* "wl dump phytbl" and "wl dump phytbl2" requires more than 1024 bytes, */
	/* which is the current limitation for BMAC dump through RPC call. */
	/* Don't support them for now. */
	wlc_dump_register(wlc->pub, "radioreg",	(dump_fn_t)wlc_dump_phy_radioreg, (void *)wlc);
#endif // endif

	wlc_dump_register(wlc->pub, "phycal", (dump_fn_t)wlc_dump_phy_cal, (void *)wlc);
	wlc_dump_register(wlc->pub, "phyaci", (dump_fn_t)wlc_dump_phy_aci, (void *)wlc);
	wlc_dump_register(wlc->pub, "phypapd",	(dump_fn_t)wlc_dump_phy_papd, (void *)wlc);
	wlc_dump_register(wlc->pub, "phynoise", (dump_fn_t)wlc_dump_phy_noise, (void *)wlc);
	wlc_dump_register(wlc->pub, "phystate",	(dump_fn_t)wlc_dump_phy_state, (void *)wlc);
	wlc_dump_register(wlc->pub, "phylo", (dump_fn_t)wlc_dump_phy_measlo, (void *)wlc);
	wlc_dump_register(wlc->pub, "phylnagain", (dump_fn_t)wlc_dump_phy_lnagain, (void *)wlc);
	wlc_dump_register(wlc->pub, "phyinitgain", (dump_fn_t)wlc_dump_phy_initgain, (void *)wlc);
	wlc_dump_register(wlc->pub, "phyhpf1tbl", (dump_fn_t)wlc_dump_phy_hpf1tbl, (void *)wlc);
	wlc_dump_register(wlc->pub, "phylpphytbl0", (dump_fn_t)wlc_dump_phy_lpphytbl0, (void *)wlc);
	wlc_dump_register(wlc->pub, "phychanest", (dump_fn_t)wlc_dump_phy_chanest, (void *)wlc);
	wlc_dump_register(wlc->pub, "macsuspend", (dump_fn_t)wlc_dump_suspend, (void *)wlc);
#ifdef ENABLE_FCBS
	wlc_dump_register(wlc->pub, "phyfcbs", (dump_fn_t)wlc_dump_phy_fcbs, (void *)wlc);
#endif /* ENABLE_FCBS */
	wlc_dump_register(wlc->pub, "phytxv0", (dump_fn_t)wlc_dump_phy_txv0, (void *)wlc);
#endif /* BCMDBG || BCMDBG_DUMP */
#ifdef WLTEST
	wlc_dump_register(wlc->pub, "phych4rpcal", (dump_fn_t)wlc_dump_phy_ch4rpcal, (void *)wlc);
#endif /* WLTEST */
}

#if IOV_DUMP_ANY_IOVAR_ENAB
enum {
	IOV_DUMP = 0,
	IOV_NVRAM_DUMP = 1,
	IOV_SMF_STATS = 2,
	IOV_SMF_STATS_ENABLE = 3,
	IOV_DUMP_LAST
};

static const bcm_iovar_t dump_info_iovars[] = {
#if IOV_DUMP_DEF_IOVAR_ENAB
	{"dump", IOV_DUMP,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, 0
	},
#endif /* IOV_DUMP_DEF_IOVAR_ENAB */
#if IOV_DUMP_NVM_IOVAR_ENAB
	{"nvram_dump", IOV_NVRAM_DUMP,
	(IOVF_MFG), IOVT_BUFFER, 0
	},
#endif /* IOV_DUMP_NVM_IOVAR_ENAB */
	{NULL, 0, 0, 0, 0}
};

static int
wlc_dump_info_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int vsize, struct wlc_if *wlcif)
{
	int err = BCME_OK;
	struct bcmstrbuf b;
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_bsscfg_t *bsscfg;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	BCM_REFERENCE(b);
	BCM_REFERENCE(bsscfg);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(int_val);
	BCM_REFERENCE(ret_int_ptr);

	switch (actionid) {
		case IOV_GVAL(IOV_DUMP):
#if IOV_DUMP_REGULAR_IOVAR_ENAB
			if (wl_msg_level & WL_LOG_VAL || WL_CHANLOG_ON())
#if defined(BCMTSTAMPEDLOGS)
				bcmdumptslog((char*)arg, len);
#else
				bcmdumplog((char*)arg, len);
#endif // endif
			else
				err = wlc_iovar_dump(wlc, (const char *)params, p_len,
					(char*)arg, len);
#elif defined(WLTINYDUMP)
			bcm_binit(&b, arg, len);
			err = wlc_tinydump(wlc, &b);
#endif /* IOV_DUMP_REGULAR_IOVAR_ENAB */
			break;
#if defined(WLTEST) || defined(WLPKTENG)
		case IOV_GVAL(IOV_NVRAM_DUMP):
			bcm_binit(&b, (char*)arg, len);
			err = wlc_nvram_dump(wlc, &b);
		break;
#endif // endif
		default:
			err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}
#endif /* IOV_DUMP_ANY_IOVAR_ENAB */

/* XXX this module is designed to be an extension of wlc.c, so module handles are meaningless
	as all information is in the wlc_info_t struct and not desirable to move it out
*/
int
BCMATTACHFN(wlc_dump_info_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	int err = BCME_OK;
	BCM_REFERENCE(pub);
#if IOV_DUMP_ANY_IOVAR_ENAB
	if (wlc_module_register(pub, dump_info_iovars, "dump_info",
		wlc, wlc_dump_info_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		err = BCME_ERROR;
	}
#endif /* IOV_DUMP_ANY_IOVAR_ENAB */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "wlc", (dump_fn_t)wlc_dump_wlc, (void *)wlc);
	wlc_dump_register(pub, "default", (dump_fn_t)wlc_dump_default, (void *)wlc);
	wlc_dump_register(pub, "ratestuff", (dump_fn_t)wlc_dump_ratestuff, (void *)wlc);
	wlc_dump_register(pub, "all", (dump_fn_t)wlc_dump_all, (void *)wlc);
	wlc_dump_register(pub, "bssinfo", (dump_fn_t)wlc_bssinfo_dump, (void *)wlc);
	wlc_dump_register(pub, "mac", (dump_fn_t)wlc_dump_mac, (void *)wlc);
	wlc_dump_register(pub, "wme", (dump_fn_t)wlc_dump_wme, (void *)wlc);
	wlc_dump_register(pub, "pio", (dump_fn_t)wlc_dump_pio, (void *)wlc);
	wlc_dump_register(pub, "dma", (dump_fn_t)wlc_dump_dma, (void *)wlc);
	wlc_dump_register(pub, "stats", (dump_fn_t)wlc_dump_stats, (void *)wlc);
	wlc_dump_register(pub, "btc", (dump_fn_t)wlc_dump_btc, (void *)wlc);
	wlc_dump_register(pub, "bmc", (dump_fn_t)wlc_dump_bmc, (void *)wlc);
	wlc_dump_register(pub, "obss", (dump_fn_t)wlc_dump_obss, (void *)wlc);
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
	wlc_dump_register(pub, "list", (dump_fn_t)wlc_dump_list, (void *)wlc);
#endif // endif
#ifdef WLTINYDUMP
	wlc_dump_register(pub, "tiny", (dump_fn_t)wlc_tinydump, (void *)wlc);
#endif /* WLTINYDUMP */
	wlc_dump_register_phy(wlc);

/* Register dump handler for memory pool manager. */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "mempool", wlc_dump_mempool, wlc);
#endif   /* BCMDBG || BCMDBG_DUMP */
#if defined(WL_OBJ_REGISTRY) && (defined(BCMDBG) || defined(BCMDBG_DUMP))
	wlc_dump_register(pub, "objreg", (dump_fn_t)wlc_dump_objr, (void *)(wlc->objr));
#endif /* WL_OBJ_REGISTRY */
#if defined(BCMDBG_DUMP) || defined(WLTEST)
	wlc_dump_register(pub, "pcieinfo", (dump_fn_t)wlc_pcieinfo_dump, (void *)wlc);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "locale", wlc_channel_dump_locale, wlc);
#if defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "shmem", (dump_fn_t)wlc_dump_shmem, (void *)wlc);
	wlc_dump_register(pub, "sctpl", (dump_fn_t)wlc_dump_sctpl, (void *)wlc);
	wlc_dump_register(pub, "hwkeys", (dump_fn_t)wlc_key_dump_hw, (void *)wlc);
	wlc_dump_register(pub, "swkeys", (dump_fn_t)wlc_key_dump_sw, (void *)wlc);
	wlc_dump_register(pub, "gpio", (dump_fn_t)wlc_gpio_dump, (void *)wlc);
	wlc_dump_register(pub, "siid", (dump_fn_t)wlc_dump_siid, (void *)wlc);
	wlc_dump_register(pub, "sireg", (dump_fn_t)wlc_dump_sireg, (void *)wlc);
	wlc_dump_register(pub, "siclk", (dump_fn_t)wlc_dump_siclk, (void *)wlc);
	wlc_dump_register(pub, "regcc", (dump_fn_t)wlc_dump_ccreg, (void *)wlc);
	wlc_dump_register(pub, "rcmta", (dump_fn_t)wlc_dump_addrmatch, (void *)wlc);
	wlc_dump_register(pub, "amt", (dump_fn_t)wlc_dump_addrmatch, (void *)wlc);
	wlc_dump_register(pub, "secalgo", (dump_fn_t)wlc_dump_secalgo, (void *)wlc);
	wlc_dump_register(pub, "pcieregs", (dump_fn_t)wlc_pciereg_dump, (void *)wlc);

#ifdef WLAMPDU_MAC
	wlc_dump_register(pub, "aggfifo", (dump_fn_t)wlc_dump_aggfifo, (void*)wlc);
#endif // endif
#endif // endif
#ifdef BCMDBG_MEM
	wlc_dump_register(pub, "malloc", (dump_fn_t)wlc_malloc_dump, (void *)wlc);
#endif // endif
#ifdef BCMDBG_CTRACE
	wlc_dump_register(pub, "ctrace", (dump_fn_t)wlc_pkt_ctrace_dump, (void *)wlc);
#endif // endif
	wlc_dump_register(pub, "bcntpl", (dump_fn_t)wlc_dump_bcntpls, (void *)wlc);
#endif /* BCMDBG || BCMDBG_DUMP */

#if defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "tsf", (dump_fn_t)wlc_tsf_dump, (void *)wlc);
#endif // endif

#if defined(WLTEST) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "nvram", (dump_fn_t)wlc_nvram_dump, (void *)wlc);
#endif // endif
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "pm", (dump_fn_t)wlc_dump_pm, (void *)wlc);
	wlc_dump_register(pub, "htcap", (dump_fn_t)wlc_dump_htcap, (void *)wlc);
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */

#if defined(WLC_LOW) && defined(WLC_HIGH) && (defined(BCMDBG) || defined(BCMDBG_DUMP) \
	|| defined(MCHAN_MINIDUMP))
	wlc_dump_register(pub, "chanswitch", (dump_fn_t)wlc_dump_chanswitch, (void *)wlc);
#endif // endif

#if defined(WLC_HIGH_ONLY) && defined(BCMDBG)
	wlc_dump_register(pub, "rpcpkt", (dump_fn_t)wlc_dump_rpcpktlog, (void *)wlc);
	wlc_dump_register(pub, "rpc", (dump_fn_t)wlc_dump_rpc, (void *)wlc);
#endif /* WLC_HIGH_ONLY && BCMDBG */

#if defined(BCMDBG_DUMP)
#ifdef SMF_STATS
	wlc_dump_register(pub, "smfstats", (dump_fn_t)wlc_dump_smfs, (void *)wlc);
#endif // endif
#endif // endif
#if defined(BCMDBG)
	wlc_dump_register(pub, "perf_stats", (dump_fn_t)wlc_dump_perf_stats, (void *)wlc);
#endif /* BCMDBG */
#if defined(BCMDBG_TXSTUCK)
	wlc_dump_register(pub, "txstuck", (dump_fn_t)wlc_dump_txstuck, (void *)wlc);
#endif /* defined(BCMDBG_TXSTUCK) */

	return err;
}
