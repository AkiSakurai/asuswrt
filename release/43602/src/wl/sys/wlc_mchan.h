/*
 * MCHAN related header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_mchan.h 453919 2014-02-06 23:10:30Z $
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
#endif
#define WLC_MCHAN_PROC_TIME_LONG_US(m)	(17000 - WLC_MCHAN_PRETBTT_TIME_US(m))
#define WLC_MCHAN_RAW_PROC_ACPHY_TIME_US (6000 - WLC_MCHAN_PRETBTT_TIME_ACPHY_US)

#ifdef WLC_HIGH_ONLY
#define WLC_MCHAN_CHAN_SWITCH_TIME_US(m)	(8000 - WLC_MCHAN_PRETBTT_TIME_US(m))
#else
#define WLC_MCHAN_CHAN_SWITCH_TIME_US(m)	WLC_MCHAN_PROC_TIME_US(m)
#endif
#ifdef WLC_HIGH_ONLY
#define WLC_MCHAN_CHAN_RAW_SWITCH_TIME_HIGH	8000
#endif


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
extern int
wlc_mchan_update_bss_chan_context(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	chanspec_t chanspec);
#if defined(PROP_TXSTATUS)
extern int wlc_wlfc_mchan_interface_state_update(wlc_info_t *wlc,
	wlc_bsscfg_t *bsscfg,
	uint8 open_close, bool force_open);
#endif
#ifdef WLMCHAN_SOFT_AP
/* These functions are for Soft-AP with Quiet IE / STA combo. Not supported currently */
extern int wlc_mchan_write_ie_quiet_len(wlc_info_t *wlc);
extern int wlc_mchan_write_ie_quiet(wlc_info_t *wlc, uint8 *buf);
#endif /* WLMCHAN_SOFT_AP */

extern 	wlc_bsscfg_t *wlc_mchan_get_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi);
extern 	wlc_bsscfg_t *wlc_mchan_get_other_cfg_frm_q(wlc_info_t *wlc, wlc_txq_info_t *qi);

#else	/* stubs */
#define wlc_mchan_attach(a) (mchan_info_t *)0x0dadbeef
#define	wlc_mchan_detach(a) do {} while (0)
#endif /* WLMCHAN */

#endif /* _wlc_mchan_h_ */
