/**
 * Required functions exported by the wlc_aibss.c to common (os-independent) driver code.
 *
 * Advanced IBSS mode
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_aibss.h 462080 2014-03-14 19:38:13Z $
 */

#ifndef _WLC_AIBSS_H_
#define _WLC_AIBSS_H_

#ifdef	WLAIBSS

typedef struct aibss_scb_info {
	int32	tx_noack_count;
	uint16	no_bcn_counter;
	uint16	bcn_count;
	uint32	pend_unicast;
} aibss_scb_info_t;

typedef struct aibss_cfg_info {
	uint32 bcn_timeout;		/* dur in seconds to receive 1 bcn */
	uint32 max_tx_retry;		/* no of consecutive no acks to send txfail event */
	uint32 pend_multicast;
} aibss_cfg_info_t;

struct wlc_aibss_info {
	int32 scb_handle;		/* SCB CUBBY OFFSET */
};

/* access the variables via a macro */
#define WLC_AIBSS_INFO_SCB_HDL(a) ((a)->scb_handle)

extern wlc_aibss_info_t *wlc_aibss_attach(wlc_info_t *wlc);
extern void wlc_aibss_detach(wlc_aibss_info_t *aibss);
extern void wlc_aibss_check_txfail(wlc_aibss_info_t *aibss, wlc_bsscfg_t *cfg, struct scb *scb);
extern void wlc_aibss_recv_convmsg(wlc_aibss_info_t *aibss, struct dot11_management_header *hdr,
	uint8 *body, int body_len, wlc_d11rxhdr_t *wrxh, uint32 rspec);
extern void wlc_aibss_recv_null(wlc_aibss_info_t *aibss, struct scb *scb, struct wlc_frminfo *f);
extern void wlc_aibss_update_bi(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg, int bi);
extern void wlc_aibss_tbtt(wlc_aibss_info_t *aibss);
extern void wlc_aibss_restart_idle_timeout(wlc_aibss_info_t *aibss_info, bool short_timeout);
extern bool wlc_aibss_sendpmnotif(wlc_aibss_info_t *aibss, wlc_bsscfg_t *cfg,
	ratespec_t rate_override, int prio, bool track);
extern void wlc_aibss_notify_ps_pkt(wlc_aibss_info_t *aibss, wlc_bsscfg_t *bsscfg, struct scb *scb,
	void *p);

#endif /* WLAIBSS */
#endif /* _WLC_AIBSS_H_ */
