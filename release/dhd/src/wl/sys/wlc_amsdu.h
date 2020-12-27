/*
 * MSDU aggregation related header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_amsdu.h 473076 2014-04-27 07:28:33Z $
*/


#ifndef _wlc_amsdu_h_
#define _wlc_amsdu_h_

extern amsdu_info_t *wlc_amsdu_attach(wlc_info_t *wlc);
extern void wlc_amsdu_detach(amsdu_info_t *ami);
extern bool wlc_amsdutx_cap(amsdu_info_t *ami);
extern bool wlc_amsdurx_cap(amsdu_info_t *ami);
extern uint16 wlc_amsdu_mtu_get(amsdu_info_t *ami);

extern void wlc_amsdu_flush(amsdu_info_t *ami);
extern void *wlc_recvamsdu(amsdu_info_t *ami, wlc_d11rxhdr_t *wrxh, void *p, bool chained_sendup);
extern void wlc_amsdu_deagg_hw(amsdu_info_t *ami, struct scb *scb,
	struct wlc_frminfo *f);
#ifdef WLAMSDU_SWDEAGG
extern void wlc_amsdu_deagg_sw(amsdu_info_t *ami, struct scb *scb,
	struct wlc_frminfo *f);
#endif

#ifdef WLAMSDU_TX
extern int wlc_amsdu_set(amsdu_info_t *ami, bool val);
extern void wlc_amsdu_agglimit_frag_upd(amsdu_info_t *ami);
extern void wlc_amsdu_txop_upd(amsdu_info_t *ami);
extern void wlc_amsdu_scb_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
extern void wlc_amsdu_txpolicy_upd(amsdu_info_t *ami);
extern void wlc_amsdu_pkt_freed(wlc_info_t *wlc, void *pkt, uint s);
extern void wlc_amsdu_agg_flush(amsdu_info_t *ami);
extern void wlc_amsdu_dotxstatus(amsdu_info_t *ami, struct scb *scb, void* p);
#ifdef WL11AC
extern void wlc_amsdu_scb_vht_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
#endif /* WL11AC */
extern void wlc_amsdu_scb_ht_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
#endif /* WLAMSDU_TX */

#if defined(PKTC) || defined(PKTC_TX_DONGLE)
extern void *wlc_amsdu_pktc_agg(amsdu_info_t *ami, struct scb *scb, void *p,
	void *n, uint8 tid, uint32 lifetime);
#endif
#if defined(PKTC) || defined(PKTC_DONGLE)
extern int32 wlc_amsdu_pktc_deagg_hw(amsdu_info_t *ami, void **pp, wlc_rfc_t *rfc,
	uint16 *index, bool *chained_sendup);
#endif
extern bool
wlc_amsdu_is_rxmax_valid(amsdu_info_t *ami);
#endif /* _wlc_amsdu_h_ */
