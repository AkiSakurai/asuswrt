/*
 * 802.11h DFS module header file
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
 * $Id: wlc_dfs.h 791038 2020-09-14 16:21:22Z $
 */

#ifndef _wlc_dfs_h_
#define _wlc_dfs_h_

/* check if core revision supports background DFS scan core */
#define DFS_HAS_BACKGROUND_SCAN_CORE(wlc) (D11REV_IS(wlc->pub->corerev, 64) || \
		D11REV_IS(wlc->pub->corerev, 65) || D11REV_IS(wlc->pub->corerev, 129) || \
		D11REV_IS(wlc->pub->corerev, 132))

#define PHYMODE(w) phy_get_phymode(WLC_PI(w))
#define PHY_AC_CHANMGR(p) (p->u.pi_acphy->chanmgri)

#ifdef WLDFS

/* module */
extern wlc_dfs_info_t *wlc_dfs_attach(wlc_info_t *wlc);
extern void wlc_dfs_detach(wlc_dfs_info_t *dfs);

/* others */
extern int wlc_set_dfs_cacstate(wlc_dfs_info_t *dfs, int state, wlc_bsscfg_t *cfg);
extern chanspec_t wlc_dfs_sel_chspec(wlc_dfs_info_t *dfs, bool force, wlc_bsscfg_t *cfg);
extern void wlc_dfs_csa_received(wlc_dfs_info_t *dfs);
extern void wlc_dfs_reset_all(wlc_dfs_info_t *dfs);
extern int wlc_dfs_set_radar(wlc_dfs_info_t *dfs, int radar, uint subband);
extern uint wlc_dfs_get_cactime_ms(wlc_dfs_info_t *dfs);
extern bool wlc_dfs_valid_ap_chanspec(wlc_info_t *wlc, chanspec_t chspec);
extern bool wlc_dfs_monitor_mode(wlc_dfs_info_t *dfs);
extern uint32 wlc_set_dfs_chan_info(wlc_info_t *wlc, wl_set_chan_info_t *chan_info);
#ifdef SLAVE_RADAR
extern void wlc_dfs_send_action_frame_complete(wlc_info_t *wlc, uint txstauts, void *arg);
extern bool wlc_cac_is_clr_chanspec(wlc_dfs_info_t *dfs, chanspec_t chspec);
extern void wlc_dfs_start_radar_report_timer(wlc_info_t *wlc);
#else
#define wlc_cac_is_clr_chanspec(dfs, chspec) TRUE
#endif /* SLAVE_RADAR */
extern void wlc_dfs_save_old_chanspec(wlc_dfs_info_t *dfs, chanspec_t chanspec);
extern void wlc_dfs_reset_old_chanspec(wlc_dfs_info_t *dfs);
#ifdef WL_DFS_TEST_MODE
extern bool wlc_dfs_test_mode(wlc_dfs_info_t *dfs);
#endif /* WL_DFS_TEST_MODE */
/* accessors */
extern uint32 wlc_dfs_get_radar(wlc_dfs_info_t *dfs);
extern bool wlc_dfs_cac_in_progress(wlc_dfs_info_t *dfs);
extern uint32 wlc_dfs_get_chan_info(wlc_dfs_info_t *dfs, uint channel);
#ifdef BGDFS
extern bool wlc_dfs_isbgdfs_active(wlc_dfs_info_t *dfs);
#endif /* BGDFS */
#else /* !WLDFS */

#define wlc_dfs_attach(wlc) NULL
#define wlc_dfs_detach(dfs) do {} while (0)

#define wlc_set_dfs_cacstate(dfs, state, cfg) do {} while (0)
#define wlc_set_dfs_chan_info(wlc, chan_info) \
	({BCM_REFERENCE(wlc); BCM_REFERENCE(chan_info); BCME_UNSUPPORTED;})
#define wlc_dfs_sel_chspec(dfs, force, cfg) \
	({BCM_REFERENCE(dfs); BCM_REFERENCE(force); BCM_REFERENCE(cfg); 0;})
#define wlc_dfs_csa_received(dfs) do {} while (0)
#define wlc_dfs_reset_all(dfs) do {} while (0)
#define wlc_dfs_set_radar(dfs, radar, subband) \
	({BCM_REFERENCE(dfs); BCM_REFERENCE(radar); BCM_REFERENCE(subband); BCME_UNSUPPORTED;})
#define wlc_dfs_valid_ap_chanspec(wlc, chspec) \
	({BCM_REFERENCE(wlc);BCM_REFERENCE(chspec); (TRUE);})

#define wlc_dfs_get_radar(dfs) ({BCM_REFERENCE(dfs); 0;})
#define wlc_dfs_cac_in_progress(dfs) ({BCM_REFERENCE(dfs); 0:})
#define wlc_dfs_get_chan_info(dfs, channel) ({BCM_REFERENCE(dfs);BCM_REFERENCE(channel); 0;})
#define wlc_dfs_test_mode(dfs) ({BCM_REFERENCE(dfs); 0;})
#define wlc_dfs_monitor_mode(dfs) ({BCM_REFERENCE(dfs); (FALSE);})
#ifdef SLAVE_RADAR
#define wlc_dfs_send_action_frame_complete(wlc, txstauts, arg) do {} while (0)
#define wlc_dfs_start_radar_report_timer(wlc_info_t *wlc) do {} while (0)
#endif /* SLAVE_RADAR */
#define wlc_cac_is_clr_chanspec(dfs, chspec) TRUE
#define wlc_dfs_save_old_chanspec(dfs, chspec) ({BCM_REFERENCE(dfs); BCM_REFERENCE(chspec);})
#define wlc_dfs_reset_old_chanspec(dfs) ({BCM_REFERENCE(dfs);})
#endif /* !WLDFS */

#if defined(WLDFS) && defined(BGDFS)
extern int wlc_dfs_scan_in_progress(wlc_dfs_info_t *dfs);
extern void wlc_dfs_scan_abort(wlc_dfs_info_t *dfs);
#else
#define wlc_dfs_scan_in_progress(dfs) ({BCM_REFERENCE(dfs); 0;})
#define wlc_dfs_scan_abort(dfs) do {} while (0)
#endif /* WLDFS && BGDFS */
#endif /* _wlc_dfs_h_ */
