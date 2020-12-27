/*
 * wlc_dngl_ol	definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_dngl_ol.h Harishv $
 */


#ifndef _wlc_dngl_ol_h_
#define _wlc_dngl_ol_h_

#define WAKE_FOR_PMMODE		(1<<0)
#define WAKE_FOR_PSPOLL		(1<<1)
#define WAKE_FOR_UATBTT		(1<<2)

struct wlc_dngl_ol_info {
	wlc_info_t	*wlc;			/* pointer to os-specific private state */
	uint unit;		/* device instance number */
	osl_t *osh;
	hndrte_dev_t *dev;
	wlc_hw_info_t	*wlc_hw;			/* HW module (private h/w states) */
	void *regs;
	uint16 pso_blk;
	pktpool_t *shared_msgpool;
	ol_tx_info txinfo;
	struct ether_addr cur_etheraddr;
	uint8 TX;
	uint8 pme_asserted;
	uint8 radio_hw_disabled;
	uint32 counter;
	uint32 stay_awake;
	uint32 tkip_mic_fail_detect;

#if defined(WL_LTR) || defined(BCM43602A0)
	wlc_ltr_info_t ltr_info;
#endif /* WL_LTR || BCM43602A0 */

	/* WoWL cfg info */
	wowl_cfg_t  wowl_cfg;

	wlc_dngl_ol_bcn_info_t *bcn_ol;
	wlc_dngl_ol_pkt_filter_info_t *pkt_filter_ol;
	wlc_dngl_ol_wowl_info_t *wowl_ol;
	wlc_dngl_ol_l2keepalive_info_t *l2keepalive_ol;
	wlc_dngl_ol_gtk_info_t *ol_gtk;
	wlc_dngl_ol_mdns_info_t *mdns_ol;
	wlc_dngl_ol_rssi_info_t *rssi_ol;
	wlc_dngl_ol_eventlog_info_t *eventlog_ol;

	/* Fragment state information */

	uint16	seqctl[NUMPRIO];
	void	*fragbuf[NUMPRIO];	/* defragmentation buffer per prio */
	uint	fragresid[NUMPRIO];	/* #bytes unused in frag buffer per prio */
	uint32	fragtimestamp[NUMPRIO];
};

extern olmsg_shared_info_t *ppcie_shared;

/* Counter amoung various offload modules */
enum counter_index {
	TXSUPPRESS,
	TXACKED,
	TXPROBEREQ
};

typedef void (*wlc_dngl_ol_event_handler_t)(wlc_dngl_ol_info_t *wlc_dngl_ol,
	uint32 event,
	void *event_data);

extern wlc_dngl_ol_info_t *wlc_dngl_ol_attach(wlc_info_t *wlc);
extern void wlc_dngl_ol_sendup(wlc_dngl_ol_info_t *wlc_dngl_ol, void* resp);
extern bool wlc_dngl_ol_sendpkt(wlc_dngl_ol_info_t *wlc_dngl_ol, void *sdu);
extern void wlc_dngl_ol_watchdog(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_event(wlc_dngl_ol_info_t *wlc_dngl_ol, uint32 event, void *event_data);

extern void wlc_dngl_ol_push_to_host(wlc_info_t *wlc);
extern bool wlc_dngl_ol_supr_frame(wlc_info_t	*wlc, uint16 frame_ptr);
extern void wlc_dngl_ol_recv(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p);
extern void wlc_dngl_ol_armtx(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len);
extern void wlc_dngl_ol_reset(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void* wlc_dngl_ol_frame_get_ctl(wlc_dngl_ol_info_t *wlc_dngl_ol, uint len);
extern void* wlc_dngl_ol_frame_get_ps_ctl(wlc_dngl_ol_info_t *wlc_dngl_ol,
	const struct ether_addr *bssid,
	const struct ether_addr *sa);

extern bool wlc_dngl_ol_sendpspoll(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_intstatus(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern bool wlc_dngl_arm_dotx(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern int wlc_dngl_ol_process_msg(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len);
extern bool arm_dotx(wlc_info_t *wlc);
extern void *wlc_dngl_ol_sendnulldata(wlc_dngl_ol_info_t *wlc_dngl_ol, int prio);
extern uint16 wlc_dngl_ol_d11hdrs(wlc_dngl_ol_info_t *wlc_dngl_ol, void *p,
	ratespec_t rspec_override, int fifo);
#ifdef WL_LTR
extern void wlc_dngl_ol_ltr_proc_msg(wlc_dngl_ol_info_t *wlc_dngl_ol, void *buf, int len);
#endif /* WL_LTR */
extern int generic_send_packet(wlc_dngl_ol_info_t *ol_info, uchar *params, uint p_len);
extern void wlc_dngl_cntinc(uint counter);


extern wlc_dngl_ol_rssi_info_t *wlc_dngl_ol_rssi_attach(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_rssi_send_proc(wlc_dngl_ol_rssi_info_t *rssi_ol, void *buf, int len);
extern int wlc_dngl_ol_phy_rssi_compute_offload(wlc_dngl_ol_rssi_info_t *rssi_ol,
	wlc_d11rxhdr_t *wlc_rxh);
extern wlc_dngl_ol_eventlog_info_t *wlc_dngl_ol_eventlog_attach(wlc_dngl_ol_info_t *wlc_dngl_ol);
extern void wlc_dngl_ol_staywake_check(wlc_dngl_ol_info_t *wlc_dngl_ol, bool tim_set);

#if defined(BCMDBG) || defined(BCMDBG_ERR)
extern const char *bcm_ol_event_str[];
#endif

#ifdef BCMDBG
#define ENTER() WL_TRACE(("%s: Enter\n", __FUNCTION__));
#define EXIT()  WL_TRACE(("%s: line (%d) Exit\n", __FUNCTION__, __LINE__));
#else
#define ENTER()
#define EXIT()
#endif
#endif /* _wlc_dngl_ol_h_ */
