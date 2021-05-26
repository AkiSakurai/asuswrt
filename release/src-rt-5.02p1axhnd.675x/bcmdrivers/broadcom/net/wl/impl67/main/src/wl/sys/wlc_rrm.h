/*
 * 802.11k definitions for
 * Broadcom 802.11abgn Networking Device Driver
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
 * $Id: wlc_rrm.h 785429 2020-03-25 06:07:35Z $
 */

#ifndef _wlc_rrm_h_
#define _wlc_rrm_h_

/* rrm report type */
#define WLC_RRM_CLASS_IOCTL		1 /* IOCTL type of report */
#define WLC_RRM_CLASS_11K		3 /* Report for 11K */

extern wlc_rrm_info_t *wlc_rrm_attach(wlc_info_t *wlc);
extern void wlc_rrm_detach(wlc_rrm_info_t *rrm_info);
extern void wlc_frameaction_rrm(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg, struct scb *scb,
	uint action_id, uint8 *body, int body_len, int8 rssi, ratespec_t rspec);
extern void wlc_rrm_pm_pending_complete(wlc_rrm_info_t *rrm_info);
extern void wlc_rrm_terminate(wlc_rrm_info_t *rrm_info);
extern bool wlc_rrm_inprog(wlc_info_t *wlc);
extern bool wlc_rrm_ftm_inprog(wlc_info_t *wlc);
extern void wlc_rrm_stop(wlc_info_t *wlc);
extern bool wlc_rrm_wait_tx_suspend(wlc_info_t *wlc);
extern void wlc_rrm_start_timer(wlc_info_t *wlc);
extern bool wlc_rrm_enabled(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg);
extern bool wlc_rrm_stats_enabled(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg);
#ifdef WLSCANCACHE
extern void wlc_rrm_update_cap(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *bsscfg);
#endif /* WLSCANCACHE */
extern bool wlc_rrm_in_progress(wlc_info_t *wlc);
extern void wlc_rrm_upd_data_activity_ts(wlc_rrm_info_t *ri);
#ifdef WL11K_ALL_MEAS
extern void wlc_rrm_stat_qos_counter(wlc_info_t *wlc, struct scb *scb, int tid, uint cnt_offset);
extern void wlc_rrm_stat_bw_counter(wlc_info_t *wlc, struct scb *scb, bool tx);
extern void wlc_rrm_stat_chanwidthsw_counter(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_rrm_tscm_upd(wlc_info_t *wlc, struct scb *scb, int tid,
	uint cnt_offset, uint cnt_val);
extern void wlc_rrm_delay_upd(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint32 delay);
#else
#define wlc_rrm_stat_qos_counter(wlc, scb, tid, cnt_offset) do {(void)(tid);} while (0)
#define wlc_rrm_stat_bw_counter(wlc, scb, tx) do {} while (0)
#define wlc_rrm_stat_chanwidthsw_counter(wlc, cfg) do {} while (0)
#define wlc_rrm_tscm_upd(wlc, scb, tid, cnt_offset, cnt_val) do {} while (0)
#define wlc_rrm_delay_upd(wlc, scb, tid, delay) do {(void)(delay);} while (0)
#endif /* WL11K_ALL_MEAS */
#ifdef WL11K_AP
extern int wlc_rrm_init_pilot_timer(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg);
extern void wlc_rrm_free_pilot_timer(wlc_rrm_info_t *rrm_info, wlc_bsscfg_t *cfg);
extern void wlc_rrm_add_pilot_timer(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_rrm_del_pilot_timer(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_rrm_get_sta_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
		struct scb *scb, uint8 *rrm_cap);
#endif /* WL11K_AP */
extern int wlc_rrm_get_neighbor_count(wlc_info_t *wlc, wlc_bsscfg_t* bsscfg, int addr_type);
#if defined(WL11K_AP) && (defined(WL11K_NBR_MEAS) || (defined(WL_MBO) && \
	!defined(WL_MBO_DISABLED)))
extern void wlc_rrm_get_nbr_report(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint16 cnt, void* buf);
#endif /* 11K_AP && (11K_NBR_MEAS || WL_MBO) */

#if BAND6G || defined(WL_OCE_AP)
extern int wlc_write_neighbor_ap_info_field(uint8 *pbuffer, wlc_rnr_info_t *nbr_rnr_info,
	uint8 *len_copied);
extern int wlc_rrm_fill_static_rnr_info(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *pbuffer,
	uint8 *len_copied);
extern bool wlc_rrm_ssid_is_in_nbr_list(wlc_bsscfg_t *cfg, wlc_ssid_t *ssid,
	he_short_ssid_list_ie_t *s_ssid);
#endif /* WL_WIFI_6GHZ || WL_OCE_AP */

#endif	/* _wlc_rrm_h_ */
