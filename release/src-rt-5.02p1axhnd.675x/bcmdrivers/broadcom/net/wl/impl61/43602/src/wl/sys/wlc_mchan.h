/*
 * MCHAN related header file
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
 * $Id: wlc_mchan.h 523621 2014-12-31 11:07:27Z $
 */

#ifndef _wlc_mchan_h_
#define _wlc_mchan_h_

#ifdef WLMCHAN
typedef enum {
	MCHAN_TO_REGULAR,
	MCHAN_TO_SPECIAL,
	MCHAN_TO_ABS,
	MCHAN_TO_PRECLOSE,
	MCHAN_TO_WAITPMTIMEOUT
} mchan_to_type;

struct wlc_mchan_context {
	struct wlc_mchan_context *next; /* Keep this as first member of structure
					 * See comments for mchan_list_elem_t struct
					 */
	wlc_txq_info_t *qi;
	chanspec_t	chanspec;
	bool pm_pending;		/* pending PM transition */
};

typedef enum {
	WLC_MCHAN_SCHED_MODE_FAIR = 0,	/* fair schedule */
	WLC_MCHAN_SCHED_MODE_STA,	/* schedule to favor sta(BSS) interface */
	WLC_MCHAN_SCHED_MODE_P2P,	/* schedule to favor p2p(GO/GC) interface */
	WLC_MCHAN_SCHED_MODE_MAX
} wlc_mchan_sched_mode_t;

/* constants */
#define WLC_MCHAN_PRETBTT_TIME_US(m)	wlc_mchan_pretbtt_time_us(m)
#define WLC_MCHAN_PRETBTT_TIME_ACPHY_US	768

#define WLC_MCHAN_PROC_TIME_BMAC_US(m)	wlc_mchan_proc_time_bmac_us(m)
#ifndef WLC_HIGH_ONLY
#define WLC_MCHAN_PROC_TIME_US(m)		wlc_mchan_proc_time_us(m)
#endif // endif
#define WLC_MCHAN_PROC_TIME_LONG_US(m)	(17000 - WLC_MCHAN_PRETBTT_TIME_US(m))
#define WLC_MCHAN_RAW_PROC_ACPHY_TIME_US (6000 - WLC_MCHAN_PRETBTT_TIME_ACPHY_US)

#ifdef WLC_HIGH_ONLY
#define WLC_MCHAN_CHAN_SWITCH_TIME_US(m)	(8000 - WLC_MCHAN_PRETBTT_TIME_US(m))
#else
#define WLC_MCHAN_CHAN_SWITCH_TIME_US(m)	WLC_MCHAN_PROC_TIME_US(m)
#endif // endif
#ifdef WLC_HIGH_ONLY
#define WLC_MCHAN_CHAN_RAW_SWITCH_TIME_HIGH	8000
#endif // endif

/* macros */
#define WLC_MCHAN_SAME_CTLCHAN(c1, c2) (wf_chspec_ctlchan((c1)) == wf_chspec_ctlchan((c2)))

extern uint32 mchan_test_var;

extern void *wlc_mchan_get_blocking_bsscfg(mchan_info_t *mchan);
extern void wlc_mchan_reset_blocking_bsscfg(mchan_info_t *mchan);
extern bool wlc_mchan_blocking_enab(mchan_info_t *mchan);
extern void wlc_mchan_blocking_set(mchan_info_t *mchan, bool enable);
extern bool wlc_mchan_delay_for_pm_set(mchan_info_t *mchan, bool enable);
extern uint8 wlc_mchan_tbtt_since_bcn_thresh(mchan_info_t *mchan);
extern bool wlc_mchan_stago_is_disabled(mchan_info_t *mchan);
extern mchan_info_t *wlc_mchan_attach(wlc_info_t *wlc);
extern void wlc_mchan_detach(mchan_info_t *mchan);
extern uint16 wlc_mchan_get_pretbtt_time(mchan_info_t *mchan);
extern void
wlc_mchan_recv_process_beacon(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, uint8 *body);

extern bool wlc_mchan_context_in_use(mchan_info_t *mchan, wlc_mchan_context_t *chan_ctxt,
                                     wlc_bsscfg_t *filter_cfg);
extern bool mchan_chan_context_has_40mhz_cfg(wlc_info_t *wlc,
                                             wlc_mchan_context_t *chan_ctxt,
                                             chanspec_t *chspec20);
extern int wlc_mchan_create_bss_chan_context(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
                                             chanspec_t chanspec);
extern int wlc_mchan_delete_bss_chan_context(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_mchan_abs_proc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 tsf_l);
extern void wlc_mchan_psc_proc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 tsf_l);
extern void wlc_mchan_pm_pending_complete(mchan_info_t *mchan);
extern void wlc_mchan_start_timer(mchan_info_t *mchan, uint32 to, mchan_to_type type);
extern void wlc_mchan_stop_timer(mchan_info_t *mchan, mchan_to_type type);
extern int wlc_mchan_sched_add(mchan_info_t *mchan, uint8 bss_idx,
                               uint32 duration, uint32 end_tsf_l);
extern void wlc_mchan_sched_delete_all(mchan_info_t *mchan);
extern void wlc_mchan_client_noa_clear(mchan_info_t *mchan, wlc_bsscfg_t *cfg);
extern void wlc_mchan_bss_prepare_pm_mode(mchan_info_t *mchan, wlc_bsscfg_t *target_cfg);
extern void wlc_mchan_reset_params(mchan_info_t * mchan);
extern int
wlc_mchan_update_bss_chan_context(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	chanspec_t chanspec);
#if defined(PROP_TXSTATUS)
extern int wlc_wlfc_mchan_interface_state_update(wlc_info_t *wlc,
	wlc_bsscfg_t *bsscfg,
	uint8 open_close, bool force_open);
#endif // endif
#ifdef WLMCHAN_SOFT_AP
/* These functions are for Soft-AP with Quiet IE / STA combo. Not supported currently */
extern int wlc_mchan_write_ie_quiet_len(wlc_info_t *wlc);
extern int wlc_mchan_write_ie_quiet(wlc_info_t *wlc, uint8 *buf);
#endif /* WLMCHAN_SOFT_AP */

extern wlc_bsscfg_t *wlc_mchan_get_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi);
extern wlc_bsscfg_t *wlc_mchan_get_other_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi);
#ifdef WLTDLS
extern void wlc_mchan_stop_tdls_timer(mchan_info_t *mchan);
extern void wlc_mchan_start_tdls_timer(mchan_info_t *mchan, wlc_bsscfg_t *parent,
	struct scb *scb, bool force);
#endif // endif
typedef struct wlc_mchan_notif_cb_data {
	wlc_bsscfg_t *cfg;
	int			status;
	int			signal;
} wlc_mchan_notif_cb_data_t;

enum MHAN_NOTIF_SIGNALS {
	MCHAN_BW_UPDATE_COMPLETE = 1,
	MCHAN_ACTIVITY_STATE_CHANGE = 2,
	MCHAN_ACTIVITY_STATE_CHANGE_CONCURRENT = 3
};

typedef void (*wlc_mchan_notif_cb_fn_t)(void *ctx, wlc_mchan_notif_cb_data_t *notif_data);
extern int wlc_mchan_notif_cb_register(mchan_info_t *mchan,
	wlc_mchan_notif_cb_fn_t cb, void *arg);
extern int wlc_mchan_notif_cb_unregister(mchan_info_t *mchan,
	wlc_mchan_notif_cb_fn_t cb, void *arg);
#else	/* stubs */
#define wlc_mchan_attach(a) (mchan_info_t *)0x0dadbeef
#define	wlc_mchan_detach(a) do {} while (0)
#endif /* WLMCHAN */

#endif /* _wlc_mchan_h_ */
