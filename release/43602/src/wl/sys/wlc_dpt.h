/*
 * DPT (Direct Packet Transfer) related header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_dpt.h 383589 2013-02-07 04:29:53Z $
*/

#ifndef _wlc_dpt_h_
#define _wlc_dpt_h_

#ifdef WLDPT
extern dpt_info_t *wlc_dpt_attach(wlc_info_t *wlc);
extern void wlc_dpt_detach(dpt_info_t *dpt);
extern bool wlc_dpt_cap(dpt_info_t *dpt);
extern void wlc_dpt_update_pm_all(dpt_info_t *dpt, wlc_bsscfg_t *cfg, bool state);
extern bool wlc_dpt_pm_pending(dpt_info_t *dpt, wlc_bsscfg_t *cfg);
extern struct scb *wlc_dpt_query(dpt_info_t *dpt, wlc_bsscfg_t *cfg,
	void *sdu, struct ether_addr *ea);
extern void wlc_dpt_used(dpt_info_t *dpt, struct scb *scb);
extern bool wlc_dpt_rcv_pkt(dpt_info_t *dpt, struct wlc_frminfo *f);
extern int wlc_dpt_set(dpt_info_t *dpt, bool on);
extern void wlc_dpt_cleanup(dpt_info_t *dpt, wlc_bsscfg_t *parent);
extern void wlc_dpt_free_scb(dpt_info_t *dpt, struct scb *scb);
extern void wlc_dpt_wpa_passhash_done(dpt_info_t *dpt, struct ether_addr *ea);
extern void wlc_dpt_port_open(dpt_info_t *dpt, struct ether_addr *ea);
extern wlc_bsscfg_t *wlc_dpt_get_parent_bsscfg(wlc_info_t *wlc, struct scb *scb);
#else	/* stubs */
#define wlc_dpt_attach(a) (dpt_info_t *)0x0dadbeef
#define	wlc_dpt_detach(a) do {} while (0)
#define	wlc_dpt_cap(a) FALSE
#define	wlc_dpt_update_pm_all(a, b, c) do {} while (0)
#define wlc_dpt_pm_pending(a, b) FALSE
#define wlc_dpt_query(a, b, c, d) NULL
#define wlc_dpt_used(a, b) do {} while (0)
#define wlc_dpt_rcv_pkt(a, b, c, d) do {} (FALSE)
#define wlc_dpt_set(a, b) do {} while (0)
#define wlc_dpt_cleanup(a, b) do {} while (0)
#define wlc_dpt_free_scb(a, b) do {} while (0)
#define wlc_dpt_wpa_passhash_done(a, b) do {} while (0)
#define wlc_dpt_port_open(a, b) do {} while (0)
#define wlc_dpt_get_parent_bsscfg(a, b) NULL
#endif /* WLDPT */

#endif /* _wlc_dpt_h_ */
