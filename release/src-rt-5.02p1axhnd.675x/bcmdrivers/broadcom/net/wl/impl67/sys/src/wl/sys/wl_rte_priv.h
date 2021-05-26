/*
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
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
 * $Id: wl_rte_priv.h 787482 2020-06-01 09:20:40Z $
 */

#ifndef _wl_rte_priv_h_
#define _wl_rte_priv_h_

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <ethernet.h>
#include <bcmdevs.h>
#include <wlioctl.h>
#include <802.11.h>
#include <d11.h>
#include <wlc_key.h>
#include <wlc_rate.h>
#include <wlc.h>
#ifdef ATE_BUILD
#include <wlu_ate.h>
#endif // endif

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

#if defined(D0_COALESCING)
#include <wl_d0_filter.h>
#endif /* D0_COALESCING */

#ifdef WLNDOE
#include <wl_ndoe.h>
#endif // endif
#include <wl_icmp.h>

#include <wl_nfc.h>
#include <rte_dev.h>
#include <rte_timer.h>

#include <wl_avs.h>

#include <hnd_cplt.h>

#include <dngl_bus.h>

typedef hnd_timer_t wl_timer;
struct wl_if_event_cb_info
{
	void *ctx;
	void *cb;
};

struct wl_if {
	struct wlc_if *wlcif;
	hnd_dev_t *dev;		/**< virtual device */
	wl_arp_info_t   *arpi;      /**< pointer to arp agent offload info */
#ifdef WLNDOE
	wl_nd_info_t	*ndi;
#endif // endif
	wl_nfc_info_t	*nfci;
	int wds_index;		/* wds interface indexing */
};

typedef struct wl_info {
	uint		unit;		/**< device instance number */
	wlc_pub_t	*pub;		/**< pointer to public wlc state */
	wlc_info_t	*wlc;
	wlc_hw_info_t	*wlc_hw;
	hnd_dev_t	*dev;		/**< primary device */
	struct dngl_bus_ops *bus_ops;
	bool		link;		/**< link state */
	uint8		hwfflags;	/**< host wake up filter flags */
	hnd_dev_stats_t	stats;
	hnd_timer_t	*workletTimer;	/**< 0 delay timer used to schedule worklet */
	bool		worklettimer_armed;
#ifdef WLPFN
	wl_pfn_info_t	*pfn;		/**< pointer to prefered network data */
#endif /* WLPFN */
	wl_toe_info_t	*toei;		/**< pointer to toe specific information */
	wl_arp_info_t	*arpi;		/**< pointer to arp agent offload info */
	wl_keep_alive_info_t	*keep_alive_info;	/**< pointer to keep-alive offload info */
	wlc_pkt_filter_info_t	*pkt_filter_info;	/**< pointer to packet filter info */
#ifdef WLNDOE
	wl_nd_info_t	*ndi;	/**< Neighbor Advertisement Offload for IPv6 */
#endif // endif
	wl_p2po_info_t	*p2po;		/**< pointer to p2p offload info */
	wl_anqpo_info_t *anqpo;	/**< pointer to anqp offload info */

	wl_disc_info_t	*disc;		/**< pointer to disc info */
	wl_eventq_info_t	*wlevtq;	/**< pointer to wl_eventq info */
	wl_gas_info_t	*gas;		/**< pointer to gas info */
	wl_nfc_info_t	*nfci;	/**< NFC for Secure WiFi */
#if defined(D0_COALESCING)
	wlc_d0_filter_info_t   *d0_filter_info;   /**< pointer to ip filter info */
#endif /* D0_COALESCING */
#ifdef WL_MONITOR
	uint32		monitor_type;	/* monitor (MPDU sniffing) mode */
#endif /* WL_MONITOR */
	health_check_info_t     *hc;    /* Descriptor: unique and mutually exclusive */
	wl_shub_info_t *shub_info; /* Sensor Hub Interworking instance */
	struct wl_natoe_info *natoe_info; /* Pointer to natoe structure */

#ifdef BCMPCIEDEV
	reorder_rxcpl_id_list_t rxcpl_list;
#endif // endif

	wl_avs_t		*avs;	/* Adaptive Voltage Scaling */
	wl_icmp_info_t *icmp;		/* pointer to icmp info */
	struct wl_if_event_cb_info *cb_fn_info; /* if event send handle */
} wl_info_t;

/** Externally used RTE function */
void do_wl_cmd(uint32 arg, uint argc, char *argv[]);

int wl_send_fwdpkt(struct wl_info *wl, void *p, struct wl_if *wlif);
int wl_rx_pktfetch(struct wl_info *wl, struct wl_if *wlif, struct lbuf *lb);
#if defined(DWDS)
int wlc_get_wlif_wdsindex(struct wl_if *wlif);
#endif // endif

#endif /* _wl_rte_priv_h_ */
