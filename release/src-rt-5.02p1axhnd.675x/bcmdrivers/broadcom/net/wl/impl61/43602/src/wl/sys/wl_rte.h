/*
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * $Id: wl_rte.h kshaha $
 */

#ifndef _wl_rte_h_
#define _wl_rte_h_

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <epivers.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <bcmdevs.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_key.h>
#include <wlc_rate.h>
#include <wlc.h>

#ifdef WLPFN
#include <wl_pfn.h>
#endif	/* WLPFN */
#include <wl_toe.h>
#include <wl_keep_alive.h>
#include <wl_gas.h>
#include <wlc_pkt_filter.h>
#include <wl_eventq.h>
#include <wl_p2po_disc.h>
#include <wl_p2po.h>
#include <wl_arpoe.h>
#include <wl_anqpo.h>
#if defined(PROP_TXSTATUS)
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#include <wlc_scb.h>
#endif // endif

#ifdef WLNDOE
#include <wl_ndoe.h>
#endif // endif
#ifdef NWOE
#include <wl_nwoe.h>
#endif // endif
#include <wl_nfc.h>

typedef struct hndrte_timer wl_timer;

struct wl_if {
	struct wlc_if *wlcif;
	hndrte_dev_t *dev;		/* virtual device */
	wl_arp_info_t   *arpi;      /* pointer to arp agent offload info */
#ifdef WLNDOE
	wl_nd_info_t	*ndi;
#endif // endif
	wl_nfc_info_t	*nfci;
};

typedef struct wl_info {
	uint		unit;		/* device instance number */
	wlc_pub_t	*pub;		/* pointer to public wlc state */
	void		*wlc;		/* pointer to private common os-independent data */
	wlc_hw_info_t	*wlc_hw;
	hndrte_dev_t	*dev;		/* primary device */
	bool		link;		/* link state */
	uint8		hwfflags;	/* host wake up filter flags */
	hndrte_stats_t	stats;
	wl_oid_t	*oid;		/* oid handler state */
	hndrte_timer_t  dpcTimer;	/* 0 delay timer used to schedule dpc */
#ifdef WLPFN
	wl_pfn_info_t	*pfn;		/* pointer to prefered network data */
#endif /* WLPFN */
	wl_toe_info_t	*toei;		/* pointer to toe specific information */
	wl_arp_info_t	*arpi;		/* pointer to arp agent offload info */
#ifdef ICMPKAOE
	wl_icmp_info_t	*icmpi;		/* pointer to icmp agent offload info */
#endif	/* ICMPKAOE */
#ifdef TCPKAOE
	wl_tcp_keep_info_t	*tcp_keep_info;	/* pointer to tcp keep-alive info */
#endif /* TCPKAOE */
	wl_keep_alive_info_t	*keep_alive_info;	/* pointer to keep-alive offload info */
	wlc_pkt_filter_info_t	*pkt_filter_info;	/* pointer to packet filter info */
#if defined(PROP_TXSTATUS)
	wlfc_info_state_t*	wlfc_info;
#endif // endif
#ifdef WLC_LOW_ONLY
	rpc_info_t 	*rpc;		/* RPC handle */
	rpc_tp_info_t	*rpc_th;	/* RPC transport handle */
	wlc_rpc_ctx_t	rpc_dispatch_ctx;
	bool dpc_stopped;	/* stop wlc_dpc() flag */
	bool dpc_requested;	/* request to wlc_dpc() */
#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	hndrte_lowmem_free_t lowmem_free_info;
#endif // endif
#endif /* WLC_LOW_ONLY */
#ifdef WLNDOE
	wl_nd_info_t	*ndi; 	/* Neighbor Advertisement Offload for IPv6 */
#endif // endif
#ifdef NWOE
	wl_nwoe_info_t  *nwoei;		/* pointer to the network offload engine info */
#endif /* NWOE */
	wl_p2po_info_t	*p2po;		/* pointer to p2p offload info */
	wl_anqpo_info_t *anqpo;	/* pointer to anqp offload info */

	wl_disc_info_t	*disc;		/* pointer to disc info */
	wl_eventq_info_t	*wlevtq;	/* pointer to wl_eventq info */
	wl_gas_info_t	*gas;		/* pointer to gas info */
#if defined(D0_COALESCING)
	wlc_d0_filter_info_t   *d0_filter_info;   /* pointer to ip filter info */
#endif /* D0_COALESCING */

#ifdef PROP_TXSTATUS
	uint8	wlfc_mode;
#endif /* PROP_TXSTATUS */
	wl_nfc_info_t	*nfci;	/* Secure WiFi thru NFC */
#ifdef WL_STA_MONITOR_COMP
#ifdef WL_MONITOR
	uint32		monitor_type;		/* monitor (MPDU sniffing) mode */
#endif /* WL_MONITOR */
#endif /* WL_STA_MONITOR_COMP */
} wl_info_t;

/* Externally used RTE function */
void do_wl_cmd(uint32 arg, uint argc, char *argv[]);

#endif /* _wl_rte_h_ */
