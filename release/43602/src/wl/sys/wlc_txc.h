/*
 * d11 tx header caching module APIs - It caches the d11 header of
 * a packet and copy it to the next packet if possible.
 * This feature saves a significant amount of processing time to
 * build a packet's d11 header from scratch.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _wlc_txc_h_
#define _wlc_txc_h_

#ifndef WLC_MAX_UCODE_CACHE_ENTRIES
#define WLC_MAX_UCODE_CACHE_ENTRIES 8
#endif
#define WLC_TX_CACHE_INVALID 0xff

/* data APIs */
/* module states */
struct wlc_txc_info {
	bool _txc;	/* runtime enable check (non-user-configurable) */
#if D11CONF_GE(42) && defined(WLC_UCODE_CACHE)
	void* ucache_entry_inuse[WLC_MAX_UCODE_CACHE_ENTRIES];
#endif
};
/* return the enab value */
#define TXC_CACHE_ENAB(txc) ((txc)->_txc)

/* function APIs */
/* module entries */
extern wlc_txc_info_t *wlc_txc_attach(wlc_info_t *wlc);
extern void wlc_txc_detach(wlc_txc_info_t *txc);

#ifndef WLC_NET80211
/* update feature's runtime enab state */
extern void wlc_txc_upd(wlc_txc_info_t *txc);

/* invalidate the entry */
extern void wlc_txc_inv(wlc_txc_info_t *txc, struct scb *scb);

/* return invalidation control location */
extern uint *wlc_txc_inv_ptr(wlc_txc_info_t *txc, struct scb *scb);

/* update the iv field of the txh struct in the cache */
extern void wlc_txc_iv_upd(wlc_txc_info_t *txc, struct scb *scb, wsec_key_t *key, bool uc_seq);

/* check if the cache has the entry */
extern bool wlc_txc_hit(wlc_txc_info_t *txc, struct scb *scb,
	void *pkt, uint pktlen, uint fifo, uint8 prio);
/* copy the header to the packet and return the address of the copy */
extern d11txh_t *wlc_txc_cp(wlc_txc_info_t *txc, struct scb *scb,
	void *pkt, uint *flags);
/* install an entry into the cache */
extern void wlc_txc_add(wlc_txc_info_t *txc, struct scb *scb,
	void *pkt, uint txhlen, uint fifo, uint8 prio, uint16 txh_off, uint d11hdr_len);
#endif /* !WLC_NET80211 */

#ifdef WLC_NET80211
/* accessors */
extern int wlc_txc_get_gen(wlc_txc_info_t *txc);
#endif

/* invalidate all entries */
extern void wlc_txc_inv_all(wlc_txc_info_t *txc);

/* get the offset of tx header start */
extern uint16 wlc_txc_get_txh_offset(wlc_txc_info_t *txc, struct scb *scb);
extern uint32 wlc_txc_get_d11hdr_len(wlc_txc_info_t *txc, struct scb *scb);

/* Get the rateinfo location in the txh when short header is enabled. */
uint8* wlc_txc_get_rate_info_shdr(wlc_txc_info_t *txc, int cache_idx);


#endif /* _wlc_txc_h_ */
