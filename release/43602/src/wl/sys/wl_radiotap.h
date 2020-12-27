/*
 * RadioTap utility routines for WL and Apps
 * This header file housing the define and function prototype use by
 * both the wl driver, tools & Apps.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id:$
 */


#ifndef _WL_RADIOTAP_H_
#define _WL_RADIOTAP_H_

#include <proto/ieee80211_radiotap.h>
/*
 * RadioTap header specific implementation
 */
struct wl_radiotap_hdr {
	struct ieee80211_radiotap_header ieee_radiotap;
	uint64 tsft;
	uint8 flags;
	union {
		uint8 rate;
		uint8 pad;
	} u;
	uint16 channel_freq;
	uint16 channel_flags;
} __attribute__ ((packed));

struct wl_radiotap_sna {
	uint8 signal;
	uint8 noise;
	uint8 antenna;
} __attribute__ ((packed));

struct wl_radiotap_xchan {
	uint32 xchannel_flags;
	uint16 xchannel_freq;
	uint8 xchannel_channel;
	uint8 xchannel_maxpower;
} __attribute__ ((packed));

struct wl_radiotap_ampdu {
	uint32 ref_num;
	uint16 flags;
	uint8 delimiter_crc;
	uint8 reserved;
} __attribute__ ((packed));

struct wl_htmcs {
	uint8 mcs_known;
	uint8 mcs_flags;
	uint8 mcs_index;
} __attribute__ ((packed));

struct wl_vhtmcs {
	uint16 vht_known;	/* IEEE80211_RADIOTAP_VHT */
	uint8 vht_flags;
	uint8 vht_bw;
	uint8 vht_mcs_nss[4];
	uint8 vht_coding;
	uint8 vht_group_id;
	uint16 vht_partial_aid;
} __attribute__ ((packed));

struct wl_radiotap_ht_tail {
	struct wl_radiotap_xchan xc;
	struct wl_radiotap_ampdu ampdu;
	union {
		struct wl_htmcs ht;
		struct wl_vhtmcs vht;
	} u;
} __attribute__ ((packed));

#define WL_RADIOTAP_PRESENT_RX				\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_RATE) |		\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |	\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |	\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

#define WL_RADIOTAP_PRESENT_RX_HT			\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |	\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |	\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |		\
	 (1 << IEEE80211_RADIOTAP_XCHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_AMPDU) |		\
	 (1 << IEEE80211_RADIOTAP_MCS))

#define WL_RADIOTAP_PRESENT_RX_VHT			\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |	\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |	\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |		\
	 (1 << IEEE80211_RADIOTAP_XCHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_AMPDU) |		\
	 (1 << IEEE80211_RADIOTAP_VHT))

#ifdef WLTXMONITOR
struct wl_radiotap_hdr_tx {
	struct ieee80211_radiotap_header        ieee_radiotap;
	uint64 tsft;
	uint8 flags;
	union {
		uint8 rate;
		uint8 pad;
	} u;
	uint16 channel_freq;
	uint16 channel_flags;
	uint16 txflags;
	uint8 retries;
	uint8 pad[3];
} __attribute__ ((packed));

#define WL_RADIOTAP_TXHDR				\
	((1 << IEEE80211_RADIOTAP_TXFLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_RETRIES))

#define WL_RADIOTAP_PRESENT_TX				\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_RATE) |		\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	 WL_RADIOTAP_TXHDR)

#define WL_RADIOTAP_PRESENT_HT_TX			\
	 ((1 << IEEE80211_RADIOTAP_TSFT) |		\
	  (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	  (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	  WL_RADIOTAP_TXHDR |				\
	  (1 << IEEE80211_RADIOTAP_XCHANNEL) |		\
	  (1 << IEEE80211_RADIOTAP_MCS))
#endif /* WLTXMONITOR */

typedef struct bsd_header_tx {
	struct wl_radiotap_hdr_tx hdr;
	uint8 ht[sizeof(struct wl_radiotap_ht_tail)];
} bsd_header_tx_t;

typedef struct bsd_header_rx {
	struct wl_radiotap_hdr hdr;
	/*
	 * include extra space beyond wl_radiotap_ht size
	 * (larger of two structs in union):
	 *   signal/noise/ant plus max of 3 pad for xchannel
	 *   tail struct (xchannel and MCS info)
	 */
	uint8 pad[3];
	uint8 ht[sizeof(struct wl_radiotap_ht_tail)];
} bsd_header_rx_t;

typedef struct radiotap_parse {
	struct ieee80211_radiotap_header *hdr;
	void *fields;
	uint fields_len;
	uint idx;
	uint offset;
} radiotap_parse_t;

struct rtap_field {
	uint len;
	uint align;
};

const struct rtap_field rtap_parse_info[] = {
	{8, 8}, /* 0:  IEEE80211_RADIOTAP_TSFT */
	{1, 1}, /* 1:  IEEE80211_RADIOTAP_FLAGS */
	{2, 2}, /* 2:  IEEE80211_RADIOTAP_RATE */
	{4, 2}, /* 3:  IEEE80211_RADIOTAP_CHANNEL */
	{2, 2}, /* 4:  IEEE80211_RADIOTAP_FHSS */
	{1, 1}, /* 5:  IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
	{1, 1}, /* 6:  IEEE80211_RADIOTAP_DBM_ANTNOISE */
	{2, 2}, /* 7:  IEEE80211_RADIOTAP_LOCK_QUALITY */
	{2, 2}, /* 8:  IEEE80211_RADIOTAP_TX_ATTENUATION */
	{2, 2}, /* 9:  IEEE80211_RADIOTAP_DB_TX_ATTENUATION */
	{1, 1}, /* 10: IEEE80211_RADIOTAP_DBM_TX_POWER */
	{1, 1}, /* 11: IEEE80211_RADIOTAP_ANTENNA */
	{1, 1}, /* 12: IEEE80211_RADIOTAP_DB_ANTSIGNAL */
	{1, 1}, /* 13: IEEE80211_RADIOTAP_DB_ANTNOISE */
	{0, 0}, /* 14: netbsd */
	{2, 2}, /* 15: IEEE80211_RADIOTAP_TXFLAGS */
	{0, 0}, /* 16: missing */
	{1, 1}, /* 17: IEEE80211_RADIOTAP_RETRIES */
	{8, 4}, /* 18: IEEE80211_RADIOTAP_XCHANNEL */
	{3, 1}, /* 19: IEEE80211_RADIOTAP_MCS */
	{8, 4}, /* 20: IEEE80211_RADIOTAP_AMPDU_STATUS */
	{12, 2}, /* 21: IEEE80211_RADIOTAP_VHT */
	{0, 0}, /* 22: */
	{0, 0}, /* 23: */
	{0, 0}, /* 24: */
	{0, 0}, /* 25: */
	{0, 0}, /* 26: */
	{0, 0}, /* 27: */
	{0, 0}, /* 28: */
	{0, 0}, /* 29: IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE */
	{6, 2}, /* 30: IEEE80211_RADIOTAP_VENDOR_NAMESPACE */
	{0, 0}  /* 31: IEEE80211_RADIOTAP_EXT */
};

extern void wl_rtapParseInit(radiotap_parse_t *rtap, uint8 *rtap_header);
extern ratespec_t wl_calcRspecFromRTap(uint8 *rtap_header);
extern bool wl_rtapFlags(uint8 *rtap_header, uint8* flags);
extern uint wl_radiotap_rx(struct dot11_header *mac_header, wl_rxsts_t *rxsts,
	bsd_header_rx_t *bsd_header);
extern uint wl_radiotap_tx(struct dot11_header *mac_header, wl_txsts_t *txsts,
	bsd_header_tx_t *bsd_header);
#endif	/* _WL_RADIOTAP_H_ */
