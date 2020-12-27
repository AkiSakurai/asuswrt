/*
 * Interface layer between WL driver and PHY driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_shim.c 453919 2014-02-06 23:10:30Z $
 */

/**
 * @file
 * @brief
 * This is "two-way" interface, acting as the SHIM layer between WL and PHY layer.
 *   WL driver can optinally call this translation layer to do some preprocessing, then reach PHY.
 *   On the PHY->WL driver direction, all calls go through this layer since PHY doesn't have the
 *   access to wlc_hw pointer.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>

#include <proto/802.11.h>
#include <bcmwifi_channels.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <sbconfig.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_pio.h>
#include <bcmsrom.h>
#ifdef WLC_HIGH
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#endif

#include <wlc.h>
#include <wlc_hw_priv.h>
#include <wlc_stf.h>

#include <wlc_bmac.h>
#include <wlc_phy_shim.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>


#ifdef BCMLTECOEX
#include <wlc_btcx.h>
#endif

/* PHY SHIM module specific state */
struct wlc_phy_shim_info {
	wlc_hw_info_t *wlc_hw;	/* pointer to main wlc_hw structure */
	void *wlc;	/* pointer to main wlc structure */
	void *wl;		/* pointer to os-specific private state */
};

wlc_phy_shim_info_t *
BCMATTACHFN(wlc_phy_shim_attach)(wlc_hw_info_t *wlc_hw, void *wl, void *wlc)
{
	wlc_phy_shim_info_t *physhim = NULL;

	if (!(physhim = (wlc_phy_shim_info_t *)MALLOC(wlc_hw->osh, sizeof(wlc_phy_shim_info_t)))) {
		WL_ERROR(("wl%d: wlc_phy_shim_attach: out of mem, malloced %d bytes\n",
			wlc_hw->unit, MALLOCED(wlc_hw->osh)));
		return NULL;
	}
	bzero((char *)physhim, sizeof(wlc_phy_shim_info_t));
	physhim->wlc_hw = wlc_hw;
	physhim->wlc = wlc;
	physhim->wl = wl;

	return physhim;
}

void
BCMATTACHFN(wlc_phy_shim_detach)(wlc_phy_shim_info_t *physhim)
{
	if (!physhim)
		return;

	MFREE(physhim->wlc_hw->osh, physhim, sizeof(wlc_phy_shim_info_t));
}

struct wlapi_timer *
wlapi_init_timer(wlc_phy_shim_info_t *physhim, void (*fn)(void* arg), void *arg, const char *name)
{
	return (struct wlapi_timer *) wl_init_timer(physhim->wl, fn, arg, name);
}

void
wlapi_free_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t)
{
	wl_free_timer(physhim->wl, (struct wl_timer *) t);
}

void
wlapi_add_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t, uint ms, int periodic)
{
	wl_add_timer(physhim->wl, (struct wl_timer *) t, ms, periodic);
}

bool
wlapi_del_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t)
{
	return wl_del_timer(physhim->wl, (struct wl_timer *) t);
}

void
wlapi_intrson(wlc_phy_shim_info_t *physhim)
{
	wl_intrson(physhim->wl);
}

uint32
wlapi_intrsoff(wlc_phy_shim_info_t *physhim)
{
	return wl_intrsoff(physhim->wl);
}

void
wlapi_intrsrestore(wlc_phy_shim_info_t *physhim, uint32 macintmask)
{
	wl_intrsrestore(physhim->wl, macintmask);
}

#ifdef	WLOFFLD
void *wlapi_get_wlc_info(wlc_phy_shim_info_t *physhim)
{

	return ((void *)physhim->wlc);
}
#endif /* WLOFFLD */

void
wlapi_bmac_write_shm(wlc_phy_shim_info_t *physhim, uint offset, uint16 v)
{
	wlc_bmac_write_shm(physhim->wlc_hw, offset, v);
}

uint16
wlapi_bmac_read_shm(wlc_phy_shim_info_t *physhim, uint offset)
{
	return wlc_bmac_read_shm(physhim->wlc_hw, offset);
}

void
wlapi_bmac_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val, int bands)
{
	wlc_bmac_mhf(physhim->wlc_hw, idx, mask, val, bands);
}

void
wlapi_bmac_corereset(wlc_phy_shim_info_t *physhim, uint32 flags)
{
	wlc_bmac_corereset(physhim->wlc_hw, flags);
}

void
wlapi_suspend_mac_and_wait(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_suspend_mac_and_wait(physhim->wlc_hw);
}
#ifdef WLSRVSDB
void
wlapi_tsf_adjust(wlc_phy_shim_info_t *physhim, uint32 delta)
{
	wlc_bmac_tsf_adjust(physhim->wlc_hw, delta);
}
#endif /* WLSRVSDB */

void
wlapi_switch_macfreq(wlc_phy_shim_info_t *physhim, uint8 spurmode)
{
	wlc_bmac_switch_macfreq(physhim->wlc_hw, spurmode);
}

void
wlapi_enable_mac(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_enable_mac(physhim->wlc_hw);
}

void
wlapi_bmac_mctrl(wlc_phy_shim_info_t *physhim, uint32 mask, uint32 val)
{
	wlc_bmac_mctrl(physhim->wlc_hw, mask, val);
}

void
wlapi_bmac_phy_reset(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_phy_reset(physhim->wlc_hw);
}

void
wlapi_bmac_bw_set(wlc_phy_shim_info_t *physhim, uint16 bw)
{
	wlc_bmac_bw_set(physhim->wlc_hw, bw);
}

uint16
wlapi_bmac_get_txant(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_get_txant(physhim->wlc_hw);
}

int
wlapi_bmac_btc_mode_get(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_btc_mode_get(physhim->wlc_hw);
}

#ifdef BCMLTECOEX
int
wlapi_ltecx_get_lte_map(wlc_phy_shim_info_t *physhim)
{
	return wlc_ltecx_get_lte_map(physhim->wlc);
}

int
wlapi_ltecx_chk_elna_bypass_mode(wlc_phy_shim_info_t *physhim)
{
	return wlc_ltecx_chk_elna_bypass_mode(physhim->wlc);
}
#endif /* BCMLTECOEX */
void
wlapi_bmac_btc_period_get(wlc_phy_shim_info_t *physhim, uint16 *btperiod, bool *btactive)
{
	*btperiod = physhim->wlc_hw->btc->bt_period;
	*btactive = physhim->wlc_hw->btc->bt_active;
}

uint8
wlapi_bmac_time_since_bcn_get(wlc_phy_shim_info_t *physhim)
{
#if defined(WLC_HIGH)
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return wlc->cfg->roam->time_since_bcn;
#else
	return 0;
#endif /* WLC_HIGH */
}


void
wlapi_bmac_phyclk_fgc(wlc_phy_shim_info_t *physhim, bool clk)
{
	wlc_bmac_phyclk_fgc(physhim->wlc_hw, clk);
}

void
wlapi_bmac_macphyclk_set(wlc_phy_shim_info_t *physhim, bool clk)
{
	wlc_bmac_macphyclk_set(physhim->wlc_hw, clk);
}

void
wlapi_bmac_core_phypll_ctl(wlc_phy_shim_info_t *physhim, bool on)
{
	wlc_bmac_core_phypll_ctl(physhim->wlc_hw, on);
}

void
wlapi_bmac_core_phypll_reset(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_core_phypll_reset(physhim->wlc_hw);
}

void
wlapi_bmac_ucode_wake_override_phyreg_set(wlc_phy_shim_info_t *physhim)
{
	wlc_ucode_wake_override_set(physhim->wlc_hw, WLC_WAKE_OVERRIDE_PHYREG);
}

void
wlapi_bmac_ucode_wake_override_phyreg_clear(wlc_phy_shim_info_t *physhim)
{
	wlc_ucode_wake_override_clear(physhim->wlc_hw, WLC_WAKE_OVERRIDE_PHYREG);
}

void
wlapi_bmac_templateptr_wreg(wlc_phy_shim_info_t *physhim, int offset)
{
	wlc_bmac_templateptr_wreg(physhim->wlc_hw, offset);
}

uint32
wlapi_bmac_templateptr_rreg(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_templateptr_rreg(physhim->wlc_hw);
}

void
wlapi_bmac_templatedata_wreg(wlc_phy_shim_info_t *physhim, uint32 word)
{
	wlc_bmac_templatedata_wreg(physhim->wlc_hw, word);
}

uint32
wlapi_bmac_templatedata_rreg(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_templatedata_rreg(physhim->wlc_hw);
}

void
wlapi_bmac_write_template_ram(wlc_phy_shim_info_t *physhim, int offset, int len, void *buf)
{
	wlc_bmac_write_template_ram(physhim->wlc_hw, offset, len, buf);
}

uint16
wlapi_bmac_rate_shm_offset(wlc_phy_shim_info_t *physhim, uint8 rate)
{
	return wlc_bmac_rate_shm_offset(physhim->wlc_hw, rate);
}

void
wlapi_high_update_phy_mode(wlc_phy_shim_info_t *physhim, uint32 phy_mode)
{
	wlc_update_phy_mode(physhim->wlc, phy_mode);
}

void
wlapi_noise_cb(wlc_phy_shim_info_t *physhim, uint8 channel, int8 noise_dbm)
{
	wlc_lq_noise_cb(physhim->wlc, channel, noise_dbm);
}

void
wlapi_ucode_sample_init(wlc_phy_shim_info_t *physhim)
{
#ifdef SAMPLE_COLLECT
	wlc_ucode_sample_init(physhim->wlc_hw);
#endif
}

void
wlapi_copyfrom_objmem(wlc_phy_shim_info_t *physhim, uint offset, void* buf, int len, uint32 sel)
{
	wlc_bmac_copyfrom_objmem(physhim->wlc_hw, offset, buf, len, sel);
}

void
wlapi_copyto_objmem(wlc_phy_shim_info_t *physhim, uint offset, const void* buf, int l, uint32 sel)
{
	wlc_bmac_copyto_objmem(physhim->wlc_hw, offset, buf, l, sel);
}


void
wlapi_high_update_txppr_offset(wlc_phy_shim_info_t *physhim, ppr_t *txpwr)
{
#ifndef SCANOL
	wlc_update_txppr_offset(physhim->wlc, txpwr);
#endif /* SCANOL */
}


void
wlapi_update_bt_chanspec(wlc_phy_shim_info_t *physhim, chanspec_t chanspec,
	bool scan_in_progress, bool roam_in_progress)
{
	wlc_bmac_update_bt_chanspec(physhim->wlc_hw, chanspec,
		scan_in_progress, roam_in_progress);
}

bool
wlapi_is_eci_coex_enabled(wlc_phy_shim_info_t *physhim)
{
	return (BCMECICOEX_ENAB_BMAC(physhim->wlc_hw));
}

void wlapi_high_txpwr_limit_update_req(wlc_phy_shim_info_t *physhim)
{
/* The function is currently has no split MAC support yet */
#if defined(WLC_HIGH)
	wlc_channel_update_txpwr_limit(physhim->wlc);
#endif /* WLC_HIGH */
}

void
wlapi_bmac_service_txstatus(wlc_phy_shim_info_t *physhim)
{
	bool fatal = FALSE;

	wlc_bmac_txstatus(physhim->wlc_hw, FALSE, &fatal);
}

void
wlapi_bmac_pkteng(wlc_phy_shim_info_t *physhim, bool start, uint numpkts)
{
#if defined(WLC_HIGH) && (defined(WLTEST) || defined(WLPKTENG))
	wl_pkteng_t pkteng;
	void *pkt = NULL;
	struct ether_addr sa = {{00, 11, 22, 33, 44, 55}};
	/* Stuff values into pkteng parameters */
	/* bcopy(params, &pkteng, sizeof(wl_pkteng_t)) */
	bzero(&pkteng, sizeof(wl_pkteng_t));
	if (start) {
		pkteng.flags |= WL_PKTENG_PER_TX_START | WL_PKTENG_SYNCHRONOUS;
		pkteng.delay = 16;      /* Inter-packet delay */
		pkteng.nframes = numpkts;       /* Number of frames */
		pkteng.length = 8;              /* Packet length */
		/* pkteng.seqno;        Enable/disable sequence no. */
		/* dest;                Destination address */
		bcopy(&sa, &pkteng.src, sizeof(struct ether_addr));   /* Source address */

		/* pkt will be freed in wlc_bmac_pkteng() */
		pkt = wlc_tx_testframe(physhim->wlc, &pkteng.dest, &sa, 0, pkteng.length);
		if (pkt == NULL)
			return;
	} else {
		pkteng.flags |= WL_PKTENG_PER_TX_STOP;
	}

	wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, pkt);
#endif 
}

void
wlapi_bmac_pkteng_txcal(wlc_phy_shim_info_t *physhim, bool start, uint numpkts,
	wl_pkteng_t *pktengine)
{
#if defined(WLC_HIGH) && (defined(WLTEST) || defined(WLPKTENG))
	wl_pkteng_t pkteng;
	void *pkt = NULL;
	struct ether_addr sa = {{00, 11, 22, 33, 44, 55}};
	if (pktengine) {
		if (pktengine->flags & WL_PKTENG_PER_TX_START) {
			bcopy(&sa, &pktengine->src, sizeof(struct ether_addr)); /* Src addr */
			/* pkt will be freed in wlc_bmac_pkteng() */
			pkt = wlc_tx_testframe(physhim->wlc, &pktengine->dest,
				&sa, 0, pktengine->length);
			if (pkt == NULL)
				return;
		}
		wlc_bmac_pkteng(physhim->wlc_hw, pktengine, pkt);
		return;
	}

	/* Stuff values into pkteng parameters */
	/* bcopy(params, &pkteng, sizeof(wl_pkteng_t)) */
	bzero(&pkteng, sizeof(wl_pkteng_t));
	if (start) {
		pkteng.flags |= WL_PKTENG_PER_TX_START | WL_PKTENG_SYNCHRONOUS;
		pkteng.delay = 16;      /* Inter-packet delay */
		pkteng.nframes = numpkts;       /* Number of frames */
		pkteng.length = 8;              /* Packet length */
		/* pkteng.seqno;        Enable/disable sequence no. */
		/* dest;                Destination address */
		bcopy(&sa, &pkteng.src, sizeof(struct ether_addr));   /* Source address */

		/* pkt will be freed in wlc_bmac_pkteng() */
		pkt = wlc_tx_testframe(physhim->wlc, &pkteng.dest, &sa, 0, pkteng.length);
		if (pkt == NULL)
			return;
	} else {
		pkteng.flags |= WL_PKTENG_PER_TX_STOP;
	}

	wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, pkt);
#endif 
}

/* Grant antenna to BT to drain out A2DP buffers in ECI and GCI chips */
void
wlapi_coex_flush_a2dp_buffers(wlc_phy_shim_info_t *physhim)
{
#if defined(BCMECICOEX) && defined(WLC_LOW)
	wlc_bmac_coex_flush_a2dp_buffers(physhim->wlc_hw);
#endif
}
#ifndef WLC_DISABLE_ACI
uint32
wlapi_get_accum_pmdur(wlc_phy_shim_info_t *physhim)
{
	return wlc_get_accum_pmdur(physhim->wlc);
}
#endif
uint32
wlapi_current_pmu_time(void)
{
	return wlc_current_pmu_time();
}
