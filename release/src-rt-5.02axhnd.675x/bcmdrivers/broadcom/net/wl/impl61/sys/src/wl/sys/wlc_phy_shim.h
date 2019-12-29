
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
 * $Id: wlc_phy_shim.h 774341 2019-04-19 05:44:51Z $
 */

#ifndef _wlc_phy_shim_h_
#define _wlc_phy_shim_h_

#include <wlioctl.h>
#include <wlc_ppr.h>
#include <wlc_types.h>
#include <wlc_objregistry.h>

#define ANTSEL_NA		0	/**< No boardlevel selection available */
#define ANTSEL_2x4		1	/**< 2x4 boardlevel selection available */
#define ANTSEL_2x3		2	/**< 2x3 SW boardlevel selection available */
#define ANTSEL_1x2_CORE1	3	/**< 1x2 Core 1 based SW boardlevel selection available */
#define ANTSEL_2x3_HWRX		4	/**< 2x3 SWTX + HWRX boardlevel selection available */
#define ANTSEL_1x2_HWRX		5	/**< 1x2 SWTX + HWRX boardlevel selection available */
#define ANTSEL_1x2_CORE0	6	/**< 1x2 Core 0 based SW boardlevel selection available */

/* Rx Antenna diversity control values */
#define	ANT_RX_DIV_FORCE_0		0	/**< Use antenna 0 */
#define	ANT_RX_DIV_FORCE_1		1	/**< Use antenna 1 */
#define	ANT_RX_DIV_START_1		2	/**< Choose starting with 1 */
#define	ANT_RX_DIV_START_0		3	/**< Choose starting with 0 */
#define	ANT_RX_DIV_ENABLE		3	/**< APHY bbConfig Enable RX Diversity */
#define ANT_RX_DIV_DEF		ANT_RX_DIV_START_0	/**< default antdiv setting */
#define DUALMAC_MAIN	1
#define DUALMAC_AUX	2

#define TOF_CLASSIFIER_BPHY_OFF_OFDM_ON 6
#define TOF_CLASSIFIER_BPHY_ON_OFDM_ON 7

/* Forward declarations */
struct wlc_hw_info;
typedef struct wlc_phy_shim_info wlc_phy_shim_info_t;
extern void
wlapi_bmac_pkteng_txcal(wlc_phy_shim_info_t *physhim, bool start, uint numpkts,
	wl_pkteng_t *pktengine);
extern void
wlapi_bmac_service_txstatus(wlc_phy_shim_info_t *physhim);

extern uint8 wlapi_bmac_time_since_bcn_get(wlc_phy_shim_info_t *physhim);
extern wlc_phy_shim_info_t *wlc_phy_shim_attach(struct wlc_hw_info *wlc_hw, void *wl, void *wlc);
extern void wlc_phy_shim_detach(wlc_phy_shim_info_t *physhim);

/* PHY to WL utility functions */
struct wlapi_timer;
extern struct wlapi_timer *wlapi_init_timer(wlc_phy_shim_info_t *physhim, void (*fn)(void* arg),
	void *arg, const char *name);
extern void wlapi_free_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t);
extern void wlapi_add_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t, uint ms,
	int periodic);
extern bool wlapi_del_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t);
extern void wlapi_intrson(wlc_phy_shim_info_t *physhim);
extern uint32 wlapi_intrsoff(wlc_phy_shim_info_t *physhim);
extern void wlapi_intrsrestore(wlc_phy_shim_info_t *physhim, uint32 macintmask);

extern void wlapi_bmac_write_shm(wlc_phy_shim_info_t *physhim, uint offset, uint16 v);
extern uint16 wlapi_bmac_read_shm(wlc_phy_shim_info_t *physhim, uint offset);
extern void wlapi_forcesteer_gerev129(wlc_phy_shim_info_t *physhim, uint8 enable);
extern void* wlapi_bmac_get_fcbs_info(wlc_phy_shim_info_t *physhim);
extern void wlapi_btc_hflg(wlc_phy_shim_info_t *physhim, bool set, uint16 val);

#if defined(WL_PSMX)
extern uint16 wlapi_bmac_read_shmx(wlc_phy_shim_info_t *physhim, uint offset);
extern void wlapi_bmac_write_shmx(wlc_phy_shim_info_t *physhim, uint offset, uint16 v);
extern bool wlapi_psmx_enabled(wlc_phy_shim_info_t *physhim);
extern void wlapi_suspend_macx_and_wait(wlc_phy_shim_info_t *physhim);
extern void wlapi_enable_macx(wlc_phy_shim_info_t *physhim);
#endif /* WL_PSMX */
extern void wlapi_bmac_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val,
	int bands);
extern uint16 wlapi_bmac_mhf_get(wlc_phy_shim_info_t *physhim, uint8 idx, int bands);
extern void wlapi_bmac_corereset(wlc_phy_shim_info_t *physhim, uint32 flags);
extern void wlapi_suspend_mac_and_wait(wlc_phy_shim_info_t *physhim);
#ifdef STA
extern void wlapi_mimops_pmbcnrx(wlc_phy_shim_info_t *physhim);
#endif /* STA */
extern void wlapi_switch_macfreq(wlc_phy_shim_info_t *physhim, uint8 spurmode);
#ifdef WLSRVSDB
extern void wlapi_tsf_adjust(wlc_phy_shim_info_t * physhim, uint32 delta);
#endif /* WLSRVSDB */
extern void wlapi_enable_mac(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_mctrl(wlc_phy_shim_info_t *physhim, uint32 mask, uint32 val);
extern void wlapi_bmac_phy_reset(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_bw_set(wlc_phy_shim_info_t *physhim, uint16 bw);
extern int wlapi_bmac_bw_check(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_phyclk_fgc(wlc_phy_shim_info_t *physhim, bool clk);
extern void wlapi_bmac_macphyclk_set(wlc_phy_shim_info_t *physhim, bool clk);
extern void wlapi_bmac_core_phypll_ctl(wlc_phy_shim_info_t *physhim, bool on);
extern void wlapi_bmac_core_phypll_reset(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_ucode_wake_override_phyreg_set(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_ucode_wake_override_phyreg_clear(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_write_template_ram(wlc_phy_shim_info_t *physhim, int o, int len,
	const void *buf);
extern void wlapi_bmac_templateptr_wreg(wlc_phy_shim_info_t *physhim, int offset);
extern uint32 wlapi_bmac_templateptr_rreg(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_templatedata_wreg(wlc_phy_shim_info_t *physhim, uint32 word);
extern uint32 wlapi_bmac_templatedata_rreg(wlc_phy_shim_info_t *physhim);
extern uint16 wlapi_bmac_rate_shm_offset(wlc_phy_shim_info_t *physhim, uint8 rate);
extern void wlapi_ucode_sample_init(wlc_phy_shim_info_t *physhim);
extern void wlapi_copyfrom_objmem(wlc_phy_shim_info_t *physhim, uint, void* buf, int, uint32 sel);
extern void wlapi_copyto_objmem(wlc_phy_shim_info_t *physhim, uint, const void* buf, int, uint32);
extern void wlapi_high_update_phy_mode(wlc_phy_shim_info_t *physhim, uint32 phy_mode);
extern void wlapi_noise_cb(wlc_phy_shim_info_t *physhim, uint8 channel, int8 noise_dbm);
extern uint16 wlapi_bmac_get_txant(wlc_phy_shim_info_t *physhim);
extern int wlapi_bmac_btc_mode_get(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_btc_period_get(wlc_phy_shim_info_t *physhim, uint16 *btperiod,
	bool *btactive);
extern void wlapi_high_update_txppr_offset(wlc_phy_shim_info_t *physhim, ppr_t *txpwr,
	ppr_ru_t *ru_txpwr);

#ifdef WL_MUPKTENG
extern uint8 wlapi_is_mutx_pkteng_on(wlc_phy_shim_info_t *physhim);
#endif // endif

#if defined(WL_MU_TX) && defined(WLVASIP)
extern uint32 wlapi_get_svmp_addr_txbf_ppr_tbl(wlc_phy_shim_info_t *physhim);
extern bool wlapi_get_vasip_fw_loaded(wlc_phy_shim_info_t *physhim);
#endif // endif

extern void wlapi_update_bt_chanspec(wlc_phy_shim_info_t *physhim,
	chanspec_t chanspec, bool scan_in_progress,
	bool roam_in_progress);
extern bool wlapi_is_eci_coex_enabled(wlc_phy_shim_info_t *physhim);
extern void wlapi_high_txpwr_limit_update_req(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_pkteng(wlc_phy_shim_info_t *physhim, bool start, uint numpkts);
extern void wlapi_bmac_pkteng_txcal(wlc_phy_shim_info_t *physhim, bool start,
	uint numpkts, wl_pkteng_t *pktengine);
extern void wlapi_bmac_service_txstatus(wlc_phy_shim_info_t *physhim);
extern void wlapi_coex_flush_a2dp_buffers(wlc_phy_shim_info_t *physhim);

#ifdef BCMLTECOEX
extern bool wlapi_ltecx_get_lte_map(wlc_phy_shim_info_t *physhim);
extern int wlapi_ltecx_chk_elna_bypass_mode(wlc_phy_shim_info_t *physhim);
#endif /* BCMLTECOEX */

extern void* wlapi_obj_registry_get(wlc_phy_shim_info_t *physhim, obj_registry_key_t key);
extern void wlapi_obj_registry_set(wlc_phy_shim_info_t *physhim, obj_registry_key_t key,
	void *value);
extern int wlapi_obj_registry_ref(wlc_phy_shim_info_t *physhim, obj_registry_key_t key);
extern int wlapi_obj_registry_unref(wlc_phy_shim_info_t *physhim, obj_registry_key_t key);
extern int wlapi_obj_registry_islast(wlc_phy_shim_info_t *physhim);
extern uint16 wlapi_get_phymode(wlc_phy_shim_info_t *physhim);
extern volatile void* wlapi_si_d11_switch_addrbase(wlc_phy_shim_info_t *physhim, uint coreunit);
extern uint wlapi_si_coreunit(wlc_phy_shim_info_t *physhim);
extern bool wlapi_get_sarenable(wlc_phy_shim_info_t *physhim);

extern void wlapi_exclusive_reg_access_core0(wlc_phy_shim_info_t *physhim, bool set);
extern void wlapi_exclusive_reg_access_core1(wlc_phy_shim_info_t *physhim, bool set);

/* #ifdef WL_PROXDETECT */
void wlapi_fft(wlc_phy_shim_info_t *physhim, int n, void *inBuf, void *outBuf, int oversamp);
int wlapi_tof_pdp_ts(int log2n, void* pIn, int FsMHz, int rx, void* pparams,
	int32* p_ts_thresh, int32* p_thresh_adj, wl_proxd_phy_error_t* tof_phy_error);
extern void wlapi_tof_retrieve_thresh(void* pparams, uint16* bitflip_thresh, uint16* snr_thresh);
/* #endif */

void wlapi_11n_proprietary_rates_enable(wlc_phy_shim_info_t *physhim, bool enable);
bool wlapi_txbf_enab(wlc_phy_shim_info_t *physhim);
void wlapi_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val, int bands);
extern bool wlapi_powercycle_inprogress(wlc_phy_shim_info_t *physhim);
void wlapi_taclear(wlc_phy_shim_info_t *physhim, bool taok);
extern void wlapi_wlc_fatal_error(wlc_phy_shim_info_t *physhim, uint32 reason_code);
extern uint8 wlapi_phy_awdl_is_on(wlc_phy_shim_info_t *physhim);

#ifdef WL_NAP
bool wlapi_nap_enab_check(wlc_phy_shim_info_t *physhim);
#endif /* WL_NAP */
extern void wlapi_openloop_cal(wlc_phy_shim_info_t *physhim, uint16 curtemp);
#ifdef OCL
bool wlapi_ocl_enab_check(wlc_phy_shim_info_t *physhim);
#endif // endif
#ifdef WLC_TXPWRCAP
bool wlapi_txpwrcap_enab_check(wlc_phy_shim_info_t *physhim);
#endif // endif
#if defined(WL_PROXDETECT)
bool wlapi_proxd_enab_check(wlc_phy_shim_info_t *physhim);
#endif /* WL_PROXDETECT */

#ifdef WLC_SW_DIVERSITY
extern void wlapi_swdiv_cellstatus_notif(wlc_phy_shim_info_t *physhim, int cellstatus);
void wlapi_swdiv_antmap_init(wlc_phy_shim_info_t *physhim);
extern uint8 wlapi_swdiv_get_default_ant(wlc_phy_shim_info_t *physhim, int coreidx);
extern int32 wlapi_swdiv_ant_plcy_override(wlc_phy_shim_info_t *physhim,
	uint core, uint8 rxovr, uint8 txovr, uint8 cellon_ovr, uint8 celloff_ovr);
#endif // endif

extern int wlapi_jtag_setbit_128(wlc_phy_shim_info_t *phy_shim, uint32 jtagureg_addr,
	uint8 bit_pos, uint8 bit_val);
extern void wlapi_set_gci_reg6_bit4(wlc_phy_shim_info_t *phy_shim, uint8 val);

extern int wlapi_wlc_ioctl(wlc_phy_shim_info_t *physhim, int cmd, void *arg, int size,
	struct wlc_if *wlcif);

#endif	/* _wlc_phy_shim_h_ */
