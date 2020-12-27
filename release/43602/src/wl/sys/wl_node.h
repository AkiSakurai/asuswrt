/*
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_node.h,v 1.1.10.3 2010-09-30 23:07:01 $
 */


#ifndef	_WL_NODE_H
#define	_WL_NODE_H

#define BWL_RSSI_WINDOW_SZ 16 /* size of RSSI sample window */
#define SCB_STBC_CAP(ni)	(((ni)->ni_htcap & IEEE80211_HTCAP_RXSTBC) && \
	  ((ni)->ni_vap->iv_flags_ext & IEEE80211_FEXT_STBC))
#define SCB_VHT_RX_STBC_CAP(a)  FALSE

struct wl_node {
	struct ieee80211_node	ni;
	struct mbuf		*amsdu;	/* AMSDU reassembly q (rx) */
	void			*ampdu;	/* AMPDU state block */
	struct txhdr_cache	*txc;

	/* RSSI data */
	int8 rssi_slot;
	int8 rssi_window[WL_RSSI_ANT_MAX][BWL_RSSI_WINDOW_SZ];

	/* Tx Rate Selection Stuff */
	struct wl_info *wl;
	wlc_rateset_t rateset;
	void *node_ratesel; /* Handle to node ratesel data */
};
#define	WL_NODE(ni)		((struct wl_node *)(ni))

#endif /* _WL_NODE_H */
