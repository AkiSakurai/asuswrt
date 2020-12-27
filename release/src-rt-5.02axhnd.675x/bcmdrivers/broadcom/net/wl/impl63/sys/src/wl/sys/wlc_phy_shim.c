/*
 * Interface layer between WL driver and PHY driver
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
 * $Id: wlc_phy_shim.c 780862 2019-11-05 07:30:33Z $
 */

/**
 * @file
 * @brief
 * This is "two-way" interface, acting as the SHIM layer between WL and PHY layer.
 *   WL driver can optinally call this translation layer to do some preprocessing, then reach PHY.
 *   On the PHY->WL driver direction, all calls go through this layer since PHY doesn't have the
 *   access to wlc_hw pointer.
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>

#include <802.11.h>
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
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <wlc_stf.h>
#include <wlc_rsdb.h>
#include <wlc_bmac.h>
#include <wlc_phy_shim.h>
#include <wlc_phy_hal.h>
#include <wlc_iocv.h>
#include <wl_export.h>
#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#endif /* BCMLTECOEX */
#ifdef WL_PROXDETECT
#include <wlc_fft.h>
#endif // endif

#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif /* WLC_SW_DIVERSITY */

#include <wlc_lq.h>
#include <wlc_assoc.h>
#include <wlc_test.h>
#include <wlc_txbf.h>
#include <wlc_btcx.h>

#ifdef WL_MUPKTENG
#include <wlc_mutx.h>
#endif // endif

#if defined(WL_MU_TX) && defined(WLVASIP)
#include <wlc_vasip.h>
#include <d11vasip_code.h>
#endif // endif

#include <hndchipc.h>

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
BCMATTACHFN(wlapi_init_timer)(wlc_phy_shim_info_t *physhim,
	void (*fn)(void* arg), void *arg, const char *name)
{
	return (struct wlapi_timer *) wl_init_timer(physhim->wl, fn, arg, name);
}

void
BCMATTACHFN(wlapi_free_timer)(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t)
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
wlapi_forcesteer_gerev129(wlc_phy_shim_info_t *physhim, uint8 enable)
{
	wlc_forcesteer_gerev129(physhim->wlc_hw->wlc, enable);
}

#ifdef BCMFCBS
void*
wlapi_bmac_get_fcbs_info(wlc_phy_shim_info_t *physhim)
{
	return physhim->wlc_hw->fi;
}
#endif /* BCMFCBS */

void
wlapi_btc_hflg(wlc_phy_shim_info_t *physhim, bool set, uint16 val)
{
	wlc_btc_hflg(physhim->wlc, set, val);
}

#if defined(WL_PSMX)
void
wlapi_bmac_write_shmx(wlc_phy_shim_info_t *physhim, uint offset, uint16 v)
{
	wlc_bmac_write_shmx(physhim->wlc_hw, offset, v);
}

uint16
wlapi_bmac_read_shmx(wlc_phy_shim_info_t *physhim, uint offset)
{
	return wlc_bmac_read_shmx(physhim->wlc_hw, offset);
}

bool
wlapi_psmx_enabled(wlc_phy_shim_info_t *physhim)
{
	return PSMX_ENAB(physhim->wlc_hw->wlc->pub);
}

void
wlapi_suspend_macx_and_wait(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_suspend_macx_and_wait(physhim->wlc_hw);
}

void
wlapi_enable_macx(wlc_phy_shim_info_t *physhim)
{
	wlc_bmac_enable_macx(physhim->wlc_hw);
}
#endif /* WL_PSMX */

void
wlapi_bmac_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val, int bands)
{
	wlc_bmac_mhf(physhim->wlc_hw, idx, mask, val, bands);
}

uint16
wlapi_bmac_mhf_get(wlc_phy_shim_info_t *physhim, uint8 idx, int bands)
{
	return wlc_bmac_mhf_get(physhim->wlc_hw, idx, bands);
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
#ifdef STA
void
wlapi_mimops_pmbcnrx(wlc_phy_shim_info_t *physhim)
{
	wlc_mimops_pmbcnrx(physhim->wlc);
}
#endif /* STA */
#ifdef WLSRVSDB
void
wlapi_tsf_adjust(wlc_phy_shim_info_t *physhim, uint32 delta)
{
	wlc_bmac_tsf_adjust(physhim->wlc_hw, (int)delta);
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

int
wlapi_bmac_bw_check(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_bw_check(physhim->wlc_hw);
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
bool
wlapi_ltecx_get_lte_map(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	if (BCMLTECOEX_ENAB(wlc->pub)) {
		return wlc_ltecx_get_lte_map(wlc->ltecx);
	} else {
		return FALSE;
	}
}

int
wlapi_ltecx_chk_elna_bypass_mode(wlc_phy_shim_info_t *physhim)
{
	return wlc_ltecx_chk_elna_bypass_mode(((wlc_info_t*)physhim->wlc)->ltecx);
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
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return wlc->cfg->roam->time_since_bcn;
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
wlapi_bmac_write_template_ram(wlc_phy_shim_info_t *physhim, int offset, int len, const void *buf)
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
#endif // endif
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
wlapi_high_update_txppr_offset(wlc_phy_shim_info_t *physhim, ppr_t *txpwr, ppr_ru_t *ru_txpwr)
{
	wlc_update_txppr_offset(physhim->wlc, txpwr, ru_txpwr);
}

#ifdef WL_MUPKTENG
uint8
wlapi_is_mutx_pkteng_on(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return wlc_mutx_pkteng_on(wlc->mutx);
}
#endif // endif

#if defined(WL_MU_TX) && defined(WLVASIP)
uint32
wlapi_get_svmp_addr_txbf_ppr_tbl(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return VASIP_SHARED_OFFSET(wlc_hw, txbf_ppr_tbl);
}

uint32
wlapi_get_svmp_addr_m2v_buf(wlc_phy_shim_info_t *physhim, int id)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	if (id == 0) {
		return VASIP_SHARED_OFFSET(wlc_hw, m2v_buf0);
	} else if (id == 1) {
		return VASIP_SHARED_OFFSET(wlc_hw, m2v_buf1);
	} else {
		return VASIP_ADDR_INVALID;
	}
}

uint32
wlapi_get_svmp_addr_bfd_buffer(wlc_phy_shim_info_t *physhim, int id)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	if (id == 0) {
		return VASIP_SHARED_OFFSET(wlc_hw, bfd_bf_rpt_buffer);
	} else if (id == 1) {
		return VASIP_SHARED_OFFSET(wlc_hw, bfd_cqi_rpt_buffer);
	} else if (id == 2) {
		return VASIP_SHARED_OFFSET(wlc_hw, bfds_log_buffer);
	} else if (id == 3) {
		return VASIP_SHARED_OFFSET(wlc_hw, bfds_mlbf_angle_buffer);
	} else {
		return VASIP_ADDR_INVALID;
	}
}

uint32
wlapi_get_svmp_addr_csi_rpt(wlc_phy_shim_info_t *physhim, int id)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	if (id == 0) {
		return VASIP_SHARED_OFFSET(wlc_hw, csi_rpt0);
	} else if (id == 1) {
		return VASIP_SHARED_OFFSET(wlc_hw, csi_rpt1);
	} else {
		return VASIP_ADDR_INVALID;
	}
}

uint32
wlapi_get_svmp_addr_mu_output(wlc_phy_shim_info_t *physhim, int id)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	uint32 base = VASIP_SHARED_OFFSET(wlc_hw, MU_OUTPUT);
	uint16 size = VASIP_SHARED_SIZE(wlc_hw, MU_OUTPUT) >> 2;
	return (base + (size * id));
}

uint32
wlapi_get_svmp_addr_txv_decompressed_report(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return VASIP_SHARED_OFFSET(wlc_hw, txv_decompressed_report);
}

uint32
wlapi_get_svmp_addr_imbf_rpt(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return VASIP_SHARED_OFFSET(wlc_hw, imbf_rpt);
}

uint32
wlapi_get_svmp_addr_sch_phybw(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return VASIP_SHARED_OFFSET(wlc_hw, sch_phybw);
}

uint16
wlapi_get_svmp_size_txv_decompressed_report(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return VASIP_SHARED_SIZE(wlc_hw, txv_decompressed_report);
}

bool
wlapi_get_vasip_fw_loaded(wlc_phy_shim_info_t *physhim)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)physhim->wlc_hw;
	return wlc_hw->vasip_loaded;
}
#endif /* WL_MU_TX && WLVASIP */

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
	wlc_channel_update_txpwr_limit(physhim->wlc);
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
#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
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
		pkt = wlc_tx_testframe(physhim->wlc, &pkteng.dest, &sa, 0,
			pkteng.length, NULL, FALSE /* doampdu */, NULL,
			NULL, 0);	/* no user-data */
		if (pkt == NULL)
			return;
		wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, &pkt, 1);
	} else {
		pkteng.flags |= WL_PKTENG_PER_TX_STOP;
		wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, NULL, 0);
	}

#endif // endif
}

void
wlapi_bmac_pkteng_txcal(wlc_phy_shim_info_t *physhim, bool start, uint numpkts,
	wl_pkteng_t *pktengine)
{
#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
	wl_pkteng_t pkteng;
	void *pkt = NULL;
	struct ether_addr sa = {{00, 11, 22, 33, 44, 55}};
	if (pktengine) {
		if (pktengine->flags & WL_PKTENG_PER_TX_START) {
			bcopy(&sa, &pktengine->src, sizeof(struct ether_addr)); /* Src addr */
			/* pkt will be freed in wlc_bmac_pkteng() */
			pkt = wlc_tx_testframe(physhim->wlc, &pktengine->dest,
				&sa, 0, pktengine->length, NULL, FALSE /* doampdu */, NULL,
				NULL, 0); /* no user-data */
			if (pkt == NULL)
				return;
			wlc_bmac_pkteng(physhim->wlc_hw, pktengine, &pkt, 1);
		} else {
			wlc_bmac_pkteng(physhim->wlc_hw, pktengine, NULL, 0);
		}
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
		pkt = wlc_tx_testframe(physhim->wlc, &pkteng.dest, &sa, 0,
			pkteng.length, NULL, FALSE /* doampdu */, NULL,
			NULL, 0); /* no user-data */
		if (pkt == NULL)
			return;
		wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, &pkt, 1);
	} else {
		pkteng.flags |= WL_PKTENG_PER_TX_STOP;
		wlc_bmac_pkteng(physhim->wlc_hw, &pkteng, NULL, 0);
	}

#endif // endif
}

/* Grant antenna to BT to drain out A2DP buffers in ECI and GCI chips */
void
wlapi_coex_flush_a2dp_buffers(wlc_phy_shim_info_t *physhim)
{
#if defined(BCMECICOEX)
	wlc_bmac_coex_flush_a2dp_buffers(physhim->wlc_hw);
#endif // endif
}

/* object registry api's */
int
BCMATTACHFN(wlapi_obj_registry_ref)(wlc_phy_shim_info_t *physhim, obj_registry_key_t key)
{
	BCM_REFERENCE(physhim);
	BCM_REFERENCE(key);
	return obj_registry_ref(physhim->wlc_hw->wlc->objr, key);
}
int
BCMATTACHFN(wlapi_obj_registry_unref)(wlc_phy_shim_info_t *physhim, obj_registry_key_t key)
{
	BCM_REFERENCE(physhim);
	BCM_REFERENCE(key);
	return obj_registry_unref(physhim->wlc_hw->wlc->objr, key);
}
void*
BCMATTACHFN(wlapi_obj_registry_get)(wlc_phy_shim_info_t *physhim, obj_registry_key_t key)
{
	BCM_REFERENCE(physhim);
	BCM_REFERENCE(key);
	return obj_registry_get(physhim->wlc_hw->wlc->objr, key);
}
void
BCMATTACHFN(wlapi_obj_registry_set)(wlc_phy_shim_info_t *physhim,
	obj_registry_key_t key, void *value)
{
	BCM_REFERENCE(physhim);
	BCM_REFERENCE(key);
	BCM_REFERENCE(value);
	obj_registry_set(physhim->wlc_hw->wlc->objr, key, value);
}
int
wlapi_obj_registry_islast(wlc_phy_shim_info_t *physhim)
{
	return obj_registry_islast(physhim->wlc_hw->wlc->objr);
}
uint16
wlapi_get_phymode(wlc_phy_shim_info_t *physhim)
{
	BCM_REFERENCE(physhim);
	return RSDB_ENAB(physhim->wlc_hw->wlc->pub) ?
		wlc_rsdb_mode(physhim->wlc_hw->wlc) : PHYMODE_MIMO;
}

volatile void*
wlapi_si_d11_switch_addrbase(wlc_phy_shim_info_t *physhim, uint coreunit)
{
	return si_d11_switch_addrbase(physhim->wlc_hw->sih, coreunit);
}

uint
wlapi_si_coreunit(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_coreunit(physhim->wlc_hw->wlc);
}

bool
wlapi_get_sarenable(wlc_phy_shim_info_t *physhim)
{
	return wlc_channel_sarenable_get(physhim->wlc_hw->wlc->cmi);
}

void
wlapi_exclusive_reg_access_core0(wlc_phy_shim_info_t *physhim, bool set)
{
	wlc_bmac_exclusive_reg_access_core0(physhim->wlc_hw, set);
}
void
wlapi_exclusive_reg_access_core1(wlc_phy_shim_info_t *physhim, bool set)
{
	wlc_bmac_exclusive_reg_access_core1(physhim->wlc_hw, set);
}

#ifdef WL_PROXDETECT
void
wlapi_fft(wlc_phy_shim_info_t *physhim, int n, void *inBuf, void *outBuf, int oversamp)
{
	wlc_hw_info_t *wlc_hw = physhim->wlc_hw;

	wlapi_pdtof_fft(wlc_hw->osh, n, inBuf, outBuf, oversamp);
}

int
wlapi_tof_pdp_ts(int log2n, void* pIn, int FsMHz, int rx, void* pparams,
	int32* p_ts_thresh, int32* p_thresh_adj, wl_proxd_phy_error_t* tof_phy_error) {

	return tof_pdp_ts(log2n, pIn, FsMHz, rx, pparams, p_ts_thresh, p_thresh_adj, tof_phy_error);
}
void
wlapi_tof_retrieve_thresh(void* pparams, uint16* bitflip_thresh, uint16* snr_thresh)
{
	tof_retrieve_thresh(pparams, bitflip_thresh, snr_thresh);
}
#endif /* WL_PROXDETECT */
void
wlapi_11n_proprietary_rates_enable(wlc_phy_shim_info_t *physhim, bool enable)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	wlc->pub->_ht_prop_rates_capable = enable;
}

bool
wlapi_txbf_enab(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	BCM_REFERENCE(wlc);

	return TXBF_ENAB(wlc->pub);
}

void
wlapi_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val, int bands)
{
	wlc_mhf(physhim->wlc, idx, mask, val, bands);
}

#ifdef WAR4360_UCODE
uint32
wlapi_need_reinit(wlc_phy_shim_info_t *physhim)
{
	return wlc_bmac_need_reinit(physhim->wlc_hw);
}
#endif /* WAR4360_UCODE */

bool
wlapi_powercycle_inprogress(wlc_phy_shim_info_t *physhim)
{
	return wl_powercycle_inprogress(physhim->wl);
}

void
wlapi_taclear(wlc_phy_shim_info_t *physhim, bool taok)
{
	wlc_info_t *wlc = physhim->wlc;

	/* The sequence is significant.
	 *   Only if sbclk is TRUE, we can proceed with register access.
	 *   Even though ta_ok is TRUE, we still want to check(and clear) target abort
	 *   si_taclear returns TRUE if there was a target abort, In this case, ta_ok must be TRUE
	 *   to avoid assert
	 *   ASSERT and si_taclear are both under ifdef BCMDBG
	 */
	/* BMAC_NOTE: for HIGH_ONLY driver, this seems being called after RPC bus failed */
	/* In hw_off condition, IOCTLs that reach here are deemed safe but taclear would
	 * certainly result in getting -1 for register reads. So skip ta_clear altogether
	 */
	if (!(wlc->pub->hw_off)) {
		ASSERT(wlc_bmac_taclear(wlc->hw, taok) || !taok);
	}
}

void
wlapi_wlc_fatal_error(wlc_phy_shim_info_t *physhim, uint32 reason_code)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	WL_ERROR(("wl%d: Phy Fatal Error. Reason Code %d\n",
		wlc->pub->unit, reason_code));
	wlc->hw->need_reinit = WL_REINIT_RC_PHY_CRASH;
	WLC_FATAL_ERROR(wlc);
}

uint8
wlapi_phy_awdl_is_on(wlc_phy_shim_info_t *physhim)
{

	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	BCM_REFERENCE(wlc);

	return FALSE;
}

int
wlapi_wlc_ioctl(wlc_phy_shim_info_t *physhim, int cmd, void *arg, int size, struct wlc_if *wlcif)
{
	return wlc_ioctl(physhim->wlc, cmd, arg, size, wlcif);
}

#ifdef WL_NAP
bool
wlapi_nap_enab_check(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return NAP_ENAB(wlc->pub);
}
#endif /* WL_NAP */

#ifdef OCL
bool
wlapi_ocl_enab_check(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	BCM_REFERENCE(wlc);
	return OCL_ENAB(wlc->pub);
}
#endif /* OCL */

#ifdef WLC_TXPWRCAP
bool
wlapi_txpwrcap_enab_check(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	BCM_REFERENCE(wlc);
	return WLTXPWRCAP_ENAB(wlc);
}
#endif /* WLC_TXPWRCAP */

#if defined(WL_PROXDETECT)
bool
wlapi_proxd_enab_check(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	BCM_REFERENCE(wlc);
	return PROXD_ENAB(wlc->pub);
}
#endif /* WL_PROXDETECT */

void
wlapi_openloop_cal(wlc_phy_shim_info_t *physhim, uint16 curtemp)
{
	si_pmu_openloop_cal(physhim->wlc_hw->sih, curtemp);
}

#ifdef WLC_SW_DIVERSITY
void
wlapi_swdiv_cellstatus_notif(wlc_phy_shim_info_t *physhim, int cellstatus)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	wlc_swdiv_cellstatus_notif(wlc->swdiv, cellstatus);
}

int32
wlapi_swdiv_ant_plcy_override(wlc_phy_shim_info_t *physhim,
	uint core, uint8 rxovr, uint8 txovr, uint8 cellon_ovr, uint8 celloff_ovr)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return wlc_swdiv_ant_plcy_override(wlc->swdiv, core, rxovr, txovr, cellon_ovr, celloff_ovr);
}

void
wlapi_swdiv_antmap_init(wlc_phy_shim_info_t *physhim)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	wlc_swdiv_antmap_init(wlc->swdiv);
}

uint8
wlapi_swdiv_get_default_ant(wlc_phy_shim_info_t *physhim, int coreidx)
{
	wlc_info_t *wlc = (wlc_info_t *)physhim->wlc;
	return wlc_swdiv_plcybased_ant_get(wlc->swdiv, coreidx);
}

#endif /* WLC_SW_DIVERSITY */

int
wlapi_jtag_setbit_128(wlc_phy_shim_info_t *phy_shim, uint32 jtagureg_addr,
	uint8 bit_pos, uint8 bit_val)
{
#if defined(BCM4369) || defined(BCM4377)
	si_t *sih = (si_t *)phy_shim->wlc_hw->sih;
	int ret;
	wlc_intrsoff(phy_shim->wlc_hw->wlc);
	ret = jtag_setbit_128(sih, jtagureg_addr, bit_pos, bit_val);
	wlc_intrson(phy_shim->wlc_hw->wlc);
	return ret;
#else
	return BCME_OK;
#endif /* BCM4369 || BCM4377 */
}

void
wlapi_set_gci_reg6_bit4(wlc_phy_shim_info_t *phy_shim, uint8 val)
{
	si_t *sih = (si_t *)phy_shim->wlc_hw->sih;
	uint origidx;

	/* Remember original core before switch to chipc/pmu */
	origidx = si_coreidx(sih);

	si_setcoreidx(sih, SI_CC_IDX);

	si_gci_chipcontrol(sih, CC_GCI_CHIPCTRL_06,
		CC_GCI_06_JTAG_SEL_MASK,
		(val << CC_GCI_06_JTAG_SEL_SHIFT));

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}
