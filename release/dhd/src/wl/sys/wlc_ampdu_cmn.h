/*
 * A-MPDU (with extended Block Ack) related header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ampdu_cmn.h 470703 2014-04-16 02:25:28Z $
*/


#ifndef _wlc_ampdu_ctl_h_
#define _wlc_ampdu_ctl_h_

extern int wlc_ampdu_init(wlc_info_t *wlc);
extern void wlc_ampdu_deinit(wlc_info_t *wlc);

extern void scb_ampdu_cleanup(wlc_info_t *wlc, struct scb *scb);
extern void scb_ampdu_cleanup_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_frameaction_ampdu(wlc_info_t *wlc, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len);

extern void wlc_scb_ampdu_enable(wlc_info_t *wlc, struct scb *scb);
extern void wlc_scb_ampdu_disable(wlc_info_t *wlc, struct scb *scb);

extern void wlc_ampdu_set_aggr_support(wlc_info_t *wlc, bool txaggr, int8 rxaggr);

#define AMPDU_MAX_SCB_TID	(NUMPRIO)	/* max tid; currently 8; future 16 */
#define AMPDU_ALL_TID_BITMAP	NBITMASK(AMPDU_MAX_SCB_TID)

#define AMPDU_MAX_MCS 23                        /* we don't deal with mcs 32 */
#define AMPDU_MAX_VHT 30			/* VHT rate 0-9, 3 streams for now */

#ifdef BCMDBG
#define AMPDUSCBCNTADD(cnt, upd) ((cnt) += (upd))
#define AMPDUSCBCNTINCR(cnt) ((cnt)++)
#else
#define AMPDUSCBCNTADD(a, b) do { } while (0)
#define AMPDUSCBCNTINCR(a)  do { } while (0)
#endif

#define AMPDU_VALIDATE_TID(ampdu, tid, str) \
	if (tid >= AMPDU_MAX_SCB_TID) { \
		WL_ERROR(("wl%d: %s: invalid tid %d\n", ampdu->wlc->pub->unit, str, tid)); \
		WLCNTINCR(ampdu->cnt->rxunexp); \
		return; \
	}

extern int wlc_send_addba_req(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 wsize,
	uint8 ba_policy, uint8 delba_timeout);
extern void *wlc_send_bar(wlc_info_t *wlc, struct scb *scb, uint8 tid,
	uint16 start_seq, uint16 cf_policy, bool enq_only, bool *blocked);
extern int wlc_send_delba(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 initiator,
	uint16 reason);

extern void wlc_ampdu_recv_ctl(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len, uint16 fk);
extern void wlc_ampdu_recv_delba(wlc_info_t *wlc, struct scb *scb,
	uint8 *body, int body_len);
extern void wlc_ampdu_recv_addba_req(wlc_info_t *wlc, struct scb *scb,
	uint8 *body, int body_len);

extern void wlc_ampdu_agg_state_update_all(wlc_info_t *wlc, bool aggr);

#endif /* _wlc_ampdu_ctl_h_ */
