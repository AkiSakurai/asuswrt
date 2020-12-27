/*
 * Exposed interfaces of wlc_modesw.c
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_modesw.h 449355 2014-02-06 21:41:18Z  $
 */

#ifndef _wlc_modesw_h_
#define _wlc_modesw_h_

#define BW_SWITCH_TYPE_UPGRADE	1
#define BW_SWITCH_TYPE_DNGRADE	2

/* Notification call back data */
enum notif_events {
	MODESW_DN_STA_COMPLETE = 1,
	MODESW_UP_STA_COMPLETE,
	MODESW_DN_AP_COMPLETE,
	MODESW_UP_AP_COMPLETE,
	MODESW_PHY_UP_COMPLETE,
	MODESW_ACTION_FAILURE,
	MODESW_LAST
};

typedef struct {
	wlc_bsscfg_t *cfg;
	uint8 		opmode;
	int 		status;
	int 		signal;
} wlc_modesw_notif_cb_data_t;

/* Time Measurement Structure   */
typedef struct modesw_time_calc {
/* Normal Downgrade  */
	uint32 ActDN_SeqCnt;
	uint32 DN_start_time;
	uint32 DN_ActionFrame_time;
	uint32 DN_PHY_BW_UPDTime;
	uint32 DN_CompTime;
/* Only PHY Upgrade */
	uint32 PHY_UP_SeqCnt;
	uint32 PHY_UP_start_time;
	uint32 PHY_UP_PM1_time;
	uint32 PHY_UP_BW_UPDTime;
	uint32 PHY_UP_CompTime;
/* Only PHY Downgrade */
	uint32 PHY_DN_SeqCnt;
	uint32 PHY_DN_start_time;
	uint32 PHY_DN_PM1_time;
	uint32 PHY_DN_BW_UPDTime;
	uint32 PHY_DN_CompTime;
/* Action Frame sending only  */
	uint32 ACTFrame_SeqCnt;
	uint32 ACTFrame_start;
	uint32 ACTFrame_complete;
} modesw_time_calc_t;

/* Call back registration */
typedef void (*wlc_modesw_notif_cb_fn_t)(void *ctx, wlc_modesw_notif_cb_data_t *notif_data);
extern int wlc_modesw_notif_cb_register(wlc_modesw_info_t *modeswinfo,
	wlc_modesw_notif_cb_fn_t cb, void *arg);
extern int wlc_modesw_notif_cb_unregister(wlc_modesw_info_t *modeswinfo,
	wlc_modesw_notif_cb_fn_t cb, void *arg);

#define MODESW_CTRL_UP_SILENT_UPGRADE			(0x0001)
#define MODESW_CTRL_DN_SILENT_DNGRADE			(0x0002)
#define MODESW_CTRL_UP_ACTION_FRAMES_ONLY		(0x0004)

extern wlc_modesw_info_t * wlc_modesw_attach(wlc_info_t *wlc);

extern void wlc_modesw_detach(wlc_modesw_info_t *sup_info);

extern int wlc_modesw_set_max_chanspec(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
	chanspec_t chanspec);

extern bool wlc_modesw_is_req_valid(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);

extern void wlc_modesw_bss_tbtt(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);

extern void wlc_modesw_pm_pending_complete(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg);

extern chanspec_t wlc_modesw_ht_chanspec_override(wlc_modesw_info_t *modesw_info, wlc_bsscfg_t *cfg,
	chanspec_t beacon_chanspec);

extern uint8 wlc_modesw_derive_opermode(wlc_modesw_info_t *modesw_info,
	chanspec_t chanspec, wlc_bsscfg_t *bsscfg);

extern bool wlc_modesw_is_downgrade(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 oper_mode_old,
	uint8 oper_mode_new);

extern uint16 wlc_modesw_get_bw_from_opermode(uint8 oper_mode);

extern int wlc_modesw_handle_oper_mode_notif_request(wlc_modesw_info_t *modesw_info,
	wlc_bsscfg_t *bsscfg, uint8 oper_mode, uint8 enabled, uint32 ctrl_flags);

extern bool
wlc_modesw_is_obss_active(wlc_modesw_info_t *modesw_info);

int
wlc_modesw_bw_switch(wlc_modesw_info_t *modesw_info,
	chanspec_t chanspec, uint8 switch_type, wlc_bsscfg_t *cfg, uint32 ctrl_flags);

extern bool
wlc_modesw_is_connection_vht(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

extern void
wlc_modesw_clear_phy_chanctx(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#endif  /* _WLC_MODESW_H_ */
