/**
 * AP Powersave state related code
 * This file aims to encapsulating the Power save state of sbc,wlc structure.
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_apps.h 662367 2016-09-29 08:25:24Z $
*/


#ifndef _wlc_apps_h_
#define _wlc_apps_h_

#ifdef WLC_HIGH
#include <wlc_frmutil.h>
#endif

/* an arbitary number for unbonded USP */
#define WLC_APSD_USP_UNB 0xfff

#ifdef AP

/* these flags are used when exchanging messages
 * about PMQ state between BMAC and HIGH
*/
#define PS_SWITCH_OFF		0
#define PS_SWITCH_PMQ_ENTRY     0x01
#define PS_SWITCH_PMQ_SUPPR_PKT 0x02
#define PS_SWITCH_RESERVED	0x0C
#define PS_SWITCH_PMQ_PSPRETEND	0x10
#define PS_SWITCH_STA_REMOVED	0x20
#define PS_SWITCH_MAC_INVALID	0x40
#define PS_SWITCH_FIFO_FLUSHED	0x80

#define AUXPMQ_INVALID_IDX	0xFFFF

extern void wlc_apps_ps_flush(wlc_info_t *wlc, struct scb *scb);
extern int wlc_apps_process_ps_switch(wlc_info_t *wlc, struct ether_addr *ea, int8 ps_on);
extern void wlc_apps_scb_ps_on(wlc_info_t *wlc, struct scb *scb);
extern void wlc_apps_scb_ps_off(wlc_info_t *wlc, struct scb *scb, bool discard);
extern void wlc_apps_process_pend_ps(wlc_info_t *wlc);
extern void wlc_apps_process_pmqdata(wlc_info_t *wlc, uint32 pmqdata);
extern void wlc_apps_pspoll_resp_prepare(wlc_info_t *wlc, struct scb *scb,
                                         void *pkt, struct dot11_header *h, bool last_frag);
extern void wlc_apps_send_psp_response(wlc_info_t *wlc, struct scb *scb, uint16 fc);

extern int wlc_apps_attach(wlc_info_t *wlc);
extern void wlc_apps_detach(wlc_info_t *wlc);

extern int wlc_pspretend_attach(wlc_info_t *wlc);
extern void wlc_pspretend_detach(wlc_pps_info_t* pps_info);

extern void wlc_apps_psq_ageing(wlc_info_t *wlc);
extern bool wlc_apps_psq(wlc_info_t *wlc, void *pkt, int prec);
extern void wlc_apps_tbtt_update(wlc_info_t *wlc);
extern bool wlc_apps_suppr_frame_enq(wlc_info_t *wlc, void *pkt, tx_status_t *txs, bool lastframe);
extern void wlc_apps_ps_prep_mpdu(wlc_info_t *wlc, void *pkt, wlc_txh_info_t *txh_info);
extern void wlc_apps_apsd_trigger(wlc_info_t *wlc, struct scb *scb, int ac);
extern void wlc_apps_apsd_prepare(wlc_info_t *wlc, struct scb *scb, void *pkt,
                                  struct dot11_header *h, bool last_frag);

extern uint8 wlc_apps_apsd_ac_available(wlc_info_t *wlc, struct scb *scb);
extern uint8 wlc_apps_apsd_ac_buffer_status(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_scb_tx_block(wlc_info_t *wlc, struct scb *scb, uint reason, bool block);
extern void wlc_apps_scb_psq_norm(wlc_info_t *wlc, struct scb *scb);
extern bool wlc_apps_scb_supr_enq(wlc_info_t *wlc, struct scb *scb, void *pkt);
extern int wlc_apps_scb_apsd_cnt(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_process_pspretend_status(wlc_info_t *wlc, struct scb *scb,
                                              bool pps_recvd_ack);
extern bool wlc_apps_scb_pspretend_on(wlc_info_t *wlc, struct scb *scb, uint8 flags);

#ifdef PROP_TXSTATUS
extern bool wlc_apps_pvb_update_from_host(wlc_info_t *wlc, struct scb *scb, bool op);
#endif

#if defined(WLTAF)
extern int  wlc_apps_psq_len(wlc_info_t *wlc, struct scb *scb);
#endif

extern void wlc_apps_pvb_update(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_psq_norm(wlc_info_t *wlc, struct scb *scb);

#ifdef WLTDLS
extern void wlc_apps_apsd_tdls_send(wlc_info_t *wlc, struct scb *scb);
#endif
extern int  wlc_apps_psq_len(wlc_info_t *wlc, struct scb *scb);

#ifdef PSPRETEND
struct wlc_pps_info {
	wlc_info_t *wlc;
	struct  wl_timer * ps_pretend_probe_timer;
	bool    is_ps_pretend_probe_timer_running;
};
#endif /* PSPRETEND */

#ifdef WL_AUXPMQ
extern void wlc_apps_clear_auxpmq(wlc_info_t *wlc);
#endif

#else /* AP */

#ifdef PROP_TXSTATUS
#define wlc_apps_pvb_update_from_host(a, b) do {} while (0)
#endif

#define wlc_apps_attach(a) FALSE
#define wlc_apps_psq(a, b, c) FALSE
#define wlc_apps_suppr_frame_enq(a, b, c, d) FALSE

#define wlc_apps_scb_ps_off(a, b, c) do {} while (0)
#define wlc_apps_process_pend_ps(a) do {} while (0)

#define wlc_apps_process_pmqdata(a, b) do {} while (0)
#define wlc_apps_pspoll_resp_prepare(a, b, c, d, e) do {} while (0)
#define wlc_apps_send_psp_response(a, b, c) do {} while (0)

#define wlc_apps_detach(a) do {} while (0)
#define wlc_apps_process_ps_switch(a, b, c) do {} while (0)
#define wlc_apps_psq_ageing(a) do {} while (0)
#define wlc_apps_tbtt_update(a) do {} while (0)
#define wlc_apps_ps_prep_mpdu(a, b, c) do {} while (0)
#define wlc_apps_apsd_trigger(a, b, c) do {} while (0)
#define wlc_apps_apsd_prepare(a, b, c, d, e) do {} while (0)
#define wlc_apps_apsd_ac_available(a, b) 0
#define wlc_apps_apsd_ac_buffer_status(a, b) 0

#define wlc_apps_scb_tx_block(a, b, c, d) do {} while (0)
#define wlc_apps_scb_psq_norm(a, b) do {} while (0)
#define wlc_apps_scb_supr_enq(a, b, c) FALSE

#endif /* AP */

#if defined(WLC_HIGH) && defined(MBSS)
extern void wlc_apps_bss_ps_off_done(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_apps_bss_ps_on_done(wlc_info_t *wlc);
extern int wlc_apps_bcmc_ps_enqueue(wlc_info_t *wlc, struct scb *bcmc_scb, void *pkt);
#else
#define wlc_apps_bss_ps_off_done(wlc, bsscfg)
#define wlc_apps_bss_ps_on_done(wlc)
#endif /* WLC_HIGH && MBSS */

#ifdef PROP_TXSTATUS
extern void wlc_apps_ps_flush_mchan(wlc_info_t *wlc, struct scb *scb);
#endif /* PROP_TXSTATUS && WLMCHAN */
extern void wlc_apps_map_pkts(wlc_info_t *wlc, struct scb *scb, map_pkts_cb_fn cb, void *ctx);
#endif /* _wlc_apps_h_ */
