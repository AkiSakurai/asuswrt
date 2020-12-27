/*
 * Wrapper to scb rate selection algorithm of Broadcom
 * algorithm of Broadcom 802.11b DCF-only Networking Adapter.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 *
 * $Id: wlc_scb_powersel.h $
 */

/** Link Margin Transmit Power Control feature. Twiki: [LinkPowerControl] */

#ifndef	_WLC_SCB_POWERSEL_H_
#define	_WLC_SCB_POWERSEL_H_

#include <wlc_power_sel.h>

extern wlc_lpc_info_t *wlc_scb_lpc_attach(wlc_info_t *wlc);
extern void wlc_scb_lpc_detach(wlc_lpc_info_t *wlpci);
extern void wlc_scb_lpc_init(wlc_lpc_info_t *wlpci, struct scb *scb);
extern void wlc_scb_lpc_init_all(wlc_lpc_info_t *wlpci);
extern void wlc_scb_lpc_init_bss(wlc_lpc_info_t *wlpci, wlc_bsscfg_t *bsscfg);
extern uint8 wlc_scb_lpc_getcurrpwr(wlc_lpc_info_t *wlpci, struct scb *scb,
	uint16 frameid);
extern void wlc_scb_lpc_update_pwr(wlc_lpc_info_t *wlpci, struct scb *scb, uint16 frameid,
	uint16 PhyTxControlWord0, uint16 PhyTxControlWord1);
extern void wlc_scb_lpc_store_pcw(wlc_lpc_info_t *wlpci, struct scb *scb, uint16 frameid,
	uint16 phy_ctrl_word);
#endif	/* _WLC_SCB_POWERSEL_H_ */
