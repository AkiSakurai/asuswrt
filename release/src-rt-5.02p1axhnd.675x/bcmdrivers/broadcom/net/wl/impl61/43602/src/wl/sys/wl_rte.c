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
 * $Id: wl_rte.c 766505 2018-08-03 11:33:26Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [HndRte]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <pcie_core.h>
#include <epivers.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <bcmdevs.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <proto/802.3.h>
#include <proto/vlan.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom.h>
#include <bcm_ol_msg.h>
#ifdef MSGTRACE
#include <msgtrace.h>
#endif // endif
#ifdef LOGTRACE
#include <logtrace.h>
#endif // endif

#include <dngl_api.h>

#include <wl_export.h>

struct wl_oid;

#if defined(WL_OIDS)
	#error "WL_OIDS defined in firmware!"
#else
	#define wl_oid_attach(a)	(struct wl_oid *)0x0dadbeef
	#define wl_oid_detach(a)	do {} while (0)
	#define wl_oid_reclaim(oid)	1
	#define wl_oid_event(a, b, c)	do {} while (0)
#endif // endif

#include <wlc_led.h>

#ifdef WLPFN
#include <wl_pfn.h>
#endif	/* WLPFN */
#include <wl_toe.h>
#include <wl_arpoe.h>
#ifdef TCPKAOE
#include <wl_tcpkoe.h>
#endif /* TCPKAOE */
#include <wl_keep_alive.h>
#include <wl_eventq.h>
#include <wl_gas.h>
#include <wl_p2po_disc.h>
#include <wl_p2po.h>
#include <wl_anqpo.h>
#include <wlc_pkt_filter.h>
#include <wlc_pcb.h>
#if defined(D0_COALESCING)
#include <wl_d0_filter.h>
#endif /* D0_COALESCING */

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
#include "../exe/wlu_cmd.h"
#endif  /* CONFIG_WLU || ATE_BUILD */

#if (defined(BCM_FD_AGGR) || defined(WLC_LOW_ONLY))
#include <bcm_rpc_tp.h>
#endif // endif

#ifdef WLC_LOW_ONLY
#include <bcm_xdr.h>
#include <bcm_rpc.h>
#include <wlc_rpc.h>

#include <wlc_channel.h>
#endif // endif

#ifdef BCMDBG
#include <bcmutils.h>
#endif // endif

#include <dngl_bus.h>
#include <dngl_wlhdr.h>

#define WL_IFTYPE_BSS	1
#define WL_IFTYPE_WDS	2

#if defined(PROP_TXSTATUS)
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#include <wlc_scb.h>
#endif // endif
#if defined(WLNDOE)
#include <wl_ndoe.h>
#endif // endif

#if defined(BCMAUTH_PSK)
#include <proto/eapol.h>
#include <wlc_auth.h>
#endif // endif

#if defined(NWOE)
#include <wl_nwoe.h>
#endif // endif

#if defined(BCM_OL_DEV)
#include <wlc_pktfilterol.h>
#include <wlc_dngl_ol.h>
#endif // endif
#include <wlc_objregistry.h>

#ifdef	WLAIBSS
#include <wlc_aibss.h>
#endif // endif

#include <wlc_ampdu_rx.h>
#if defined(PROP_TXSTATUS)
#include <flring_fc.h>
#ifdef BCMPCIEDEV
#include <bcmmsgbuf.h>
#endif // endif
#endif /* PROP_TXSTATUS */
#include <wl_nfc.h>

#ifdef WL_TBOW
#include <wlc_tbow.h>
#endif // endif

#include <wlc_key.h>
#include <wlc_scb.h>
#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)
#include <wlc_wapi.h>
#endif // endif
#ifdef WLWNM_AP
#include <wlc_wnm.h>
#endif // endif

#ifdef WL_STA_MONITOR_COMP
#ifdef WL_MONITOR
#include <wlc_stamon.h>
#ifdef WL_RADIOTAP
#include <wlc_ethereal.h>
#include <wl_radiotap.h>
#endif /* WL_RADIOTAP */
#endif /* WL_MONITOR */
#endif /* WL_STA_MONITOR_COMP */

#ifdef ECOUNTERS
#include <ecounters.h>
#endif // endif

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
	struct wl_oid	*oid;		/* oid handler state */
	hndrte_timer_t  dpcTimer;	/* 0 delay timer used to schedule dpc */
#ifdef WLPFN
	wl_pfn_info_t	*pfn;		/* pointer to prefered network data */
#endif /* WLPFN */
	wl_toe_info_t	*toei;		/* pointer to toe specific information */
	wl_arp_info_t	*arpi;		/* pointer to arp agent offload info */
	wl_keep_alive_info_t	*keep_alive_info;	/* pointer to keep-alive offload info */
	wlc_pkt_filter_info_t	*pkt_filter_info;	/* pointer to packet filter info */
#if defined(PROP_TXSTATUS) && !defined(PROP_TXSTATUS_ROM_COMPAT)
	wlfc_info_state_t*	wlfc_info;
	uint8			wlfc_mode;
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
	wlc_obj_registry_t	*objr;		/* Common shared object registry for RSDB */
	wl_nfc_info_t	*nfci;	/* NFC for Secure WiFi */
#if defined(PROP_TXSTATUS) && defined(PROP_TXSTATUS_ROM_COMPAT)
	/* ROM compatibility - relocate struct fields that were excluded in ROMs,
	 * but are required in ROM offload builds.
	 */
	wlfc_info_state_t*	wlfc_info;
	uint8			wlfc_mode;
#endif // endif
#if defined(D0_COALESCING)
	wlc_d0_filter_info_t   *d0_filter_info;   /* pointer to ip filter info */
#endif /* D0_COALESCING */
#ifdef ICMPKAOE
	wl_icmp_info_t	*icmpi;		/* pointer to icmp agent offload info */
#endif /* ICMPKAOE */
#ifdef TCPKAOE
	wl_tcp_keep_info_t	*tcp_keep_info;	/* pointer to tcp keep-alive info */
#endif /* TCPKAOE */
#ifdef WL_MONITOR
	uint32		monitor_type;		/* monitor (MPDU sniffing) mode */
#endif /* WL_MONITOR */
} wl_info_t;

#define WL_IF(wl, dev)	(((hndrte_dev_t *)(dev) == ((wl_info_t *)(wl))->dev) ? \
			 NULL : \
			 *(wl_if_t **)((hndrte_dev_t *)(dev) + 1))

#ifdef WLC_LOW_ONLY

/* Minimal memory requirement to do wlc_dpc. This is critical for BMAC as it cannot
 * lose frames
 * This value is based on perceived DPC need. Note that it accounts for possible
 * fragmentation  where sufficient memory does not mean getting contiguous allocation
 */

#define MIN_DPC_MEM	((RXBND + 6)* 2048)

#endif /* WLC_LOW_ONLY */

/* host wakeup filter flags */
#define HWFFLAG_UCAST	1		/* unicast */
#define HWFFLAG_BCAST	2		/* broadcast */

/* iovar table */
enum {
	IOV_HWFILTER,		/* host wakeup filter */
	IOV_DEEPSLEEP,		/* Deep sleep mode */
	IOV_DNGL_STATS,
#ifdef PROP_TXSTATUS
	IOV_WLFC_MODE,
	IOV_PROPTX_CRTHRESH,	/* PROPTX credit report level */
	IOV_PROPTX_DATATHRESH,	/* PROPTX pending data report level */
#endif /* PROP_TXSTATUS */
	IOV_RTETIMESYNC,
#ifdef ECOUNTERS
	IOV_ECOUNTERS,
#endif // endif
	IOV_LAST		/* In case of a need to check max ID number */
};

static const bcm_iovar_t wl_iovars[] = {
	{"hwfilter", IOV_HWFILTER, 0, IOVT_BUFFER, 0},
	{"deepsleep", IOV_DEEPSLEEP, 0, IOVT_BOOL, 0},
	{"dngl_stats", IOV_DNGL_STATS, 0, IOVT_BUFFER, 0},
#ifdef PROP_TXSTATUS
	{"wlfc_mode", IOV_WLFC_MODE, 0, IOVT_UINT8, 0},
	{"proptx_credit_thresh", IOV_PROPTX_CRTHRESH, 0, IOVT_UINT32, 0},
	{"proptx_data_thresh", IOV_PROPTX_DATATHRESH, 0, IOVT_UINT32, 0},
#endif /* PROP_TXSTATUS */
	{"rte_timesync", IOV_RTETIMESYNC, 0, IOVT_UINT32, 0},
#ifdef ECOUNTERS
	{"ecounters", IOV_ECOUNTERS, 0, IOVT_BUFFER, sizeof(ecounters_config_request_t)},
#endif // endif
	{NULL, 0, 0, 0, 0}
};

#ifdef PROP_TXSTATUS
static void wlfc_sendup_timer(void* arg);
static int wlfc_initialize(wl_info_t *wl, wlc_info_t *wlc);
#endif /* PROP_TXSTATUS */
#ifdef PROP_TXSTATUS_DEBUG
static void wlfc_hostpkt_callback(wlc_info_t *wlc, void *pkt, uint txs);
#endif // endif

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
/* forward prototype */
void do_wl_cmd(uint32 arg, uint argc, char *argv[]);
#endif /* CONFIG_WLU || ATE_BUILD */
#ifdef BCMDBG
static void do_wlmsg_cmd(uint32 arg, uint argc, char *argv[]);
#endif // endif
#ifdef WLC_HIGH
static void wl_statsupd(wl_info_t *wl);
#endif // endif
static void wl_timer_main(hndrte_timer_t *t);
#ifdef WLC_HIGH
static int wl_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                      void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
#endif // endif

/* Driver entry points */
void *wl_probe(hndrte_dev_t *dev, void *regs, uint bus,
	uint16 devid, uint coreid, uint unit);
static void wl_free(wl_info_t *wl, osl_t *osh);
static void wl_isr(hndrte_dev_t *dev);
static void _wl_dpc(hndrte_timer_t *timer);
static void wl_dpc(wl_info_t *wl);
static int wl_open(hndrte_dev_t *dev);
static int wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb);
#ifdef WLC_HIGH
static void wl_tx_pktfetch(wl_info_t *wl, struct lbuf *lb,
	hndrte_dev_t *src, hndrte_dev_t *dev);
static void wl_send_cb(void *lbuf, void *orig_lfrag, void *ctx, bool cancelled);
#endif /* WLC_HIGH */
static int wl_close(hndrte_dev_t *dev);
#ifdef BCMPCIEDEV
static uint32 wl_flowring_update(hndrte_dev_t *dev, uint16 flowid, uint8 op, uint8 * sa,
	uint8 *da, uint8 tid);
#endif /* BCMPCIEDEV */

#if defined(PROP_TXSTATUS) && defined(BCMPCIEDEV)
static int wlfc_push_signal_bus_data(struct wl_info *wl, void* data, uint8 len);
#endif // endif

#ifndef WLC_LOW_ONLY
static int wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed,
	int set);
static bool _wl_rte_oid_check(wl_info_t *wl, uint32 cmd, void *buf, int len, int *used,
	int *needed, bool set, int *status);
#else
#ifdef BCM_OL_DEV
static int wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed,
	int set);
#endif // endif

static void wl_rpc_down(void *wlh);
static void wl_rpc_resync(void *wlh);

static void wl_rpc_tp_txflowctl(hndrte_dev_t *dev, bool state, int prio);
static void wl_rpc_txflowctl(void *wlh, bool on);

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
static void wl_lowmem_free(void *wlh);
#endif // endif
static void wl_rpc_bmac_dispatch(void *ctx, struct rpc_buf* buf);

static void do_wlhist_cmd(uint32 arg, uint argc, char *argv[]);
static void do_wldpcdump_cmd(uint32 arg, uint argc, char *argv[]);
#endif /* WLC_LOW_ONLY */

#ifdef WLC_HIGH
#ifdef TOE
static void _wl_toe_send_proc(wl_info_t *wl, void *p);
static int _wl_toe_recv_proc(wl_info_t *wl, void *p);
#endif /* TOE */
#endif /* WLC_HIGH */

#ifdef ECOUNTERS
static int wl_ecounters_entry_point(ecounters_get_stats fn, uint16 type,
	void *context);
#endif // endif

static hndrte_devfuncs_t wl_funcs = {
#if defined(BCMROMSYMGEN_BUILD) || !defined(BCMROMBUILD)
	probe:		wl_probe,
#endif // endif
	open:		wl_open,
	close:		wl_close,
	xmit:		wl_send,
#ifdef WLC_LOW_ONLY
	txflowcontrol:	wl_rpc_tp_txflowctl,
#ifdef BCM_OL_DEV
	ioctl:		wl_ioctl,
#endif /* PCIDEV */
#else
	ioctl:		wl_ioctl,
#endif /* WLC_LOW_ONLY */
	poll:		wl_isr,
};

hndrte_dev_t bcmwl = {
	name:		"wl",
	funcs:		&wl_funcs,
	softc:		NULL,
	next:		NULL,
	chained:	NULL,
	pdev:		NULL
};

#ifdef WLC_LOW_ONLY
#endif /* WLC_LOW_ONLY */

#ifdef EXT_STA
static void wl_rx_ctxt_push(wl_info_t *wl, void *pkt);

/* rx_ctxt_t is included per packet under EXT_STA
* We push the packet and copy it to the front.
* From host we need to retrieve it from front and
* and pull it back.
*/
static void wl_rx_ctxt_push(wl_info_t *wl, void *pkt)
{
	rx_ctxt_t * rx_ctxt;
	wlc_pkttag_t *pkttag = PKTTAG(pkt);

	/* Push the packet to make space for rx context */
	PKTPUSH(wl->pub->osh, pkt, sizeof(rx_ctxt_t));
	rx_ctxt = (rx_ctxt_t *) PKTDATA(wl->pub->osh, pkt);

	/* convert rpsec to rate in 500Kbps units */
	rx_ctxt->rate = RSPEC2KBPS(pkttag->rspec) / 500;
	rx_ctxt->rssi = pkttag->pktinfo.misc.rssi;
	rx_ctxt->channel = pkttag->rxchannel;
}
#endif /* EXT_STA */

#ifdef WLC_HIGH

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
wl_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
           void *p, uint plen, void *arg, int alen, int vsize, struct wlc_if *wlcif)
{

	wl_info_t *wl = (wl_info_t *)hdl;
	wlc_info_t *wlc = wl->wlc;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;
	int radio;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_HWFILTER):
		*ret_int_ptr = wl->hwfflags;
		break;

	case IOV_SVAL(IOV_HWFILTER):
		wl->hwfflags = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_DEEPSLEEP):
		if ((err = wlc_get(wlc, WLC_GET_RADIO, &radio)))
			break;
		*ret_int_ptr = (radio & WL_RADIO_SW_DISABLE) ? TRUE : FALSE;
		break;

	case IOV_SVAL(IOV_DEEPSLEEP):
		wlc_set(wlc, WLC_SET_RADIO, (WL_RADIO_SW_DISABLE << 16)
		        | (bool_val ? WL_RADIO_SW_DISABLE : 0));
		/* suspend or resume timers */
		if (bool_val)
			hndrte_suspend_timer();
		else
			hndrte_resume_timer();
		break;

	case IOV_GVAL(IOV_DNGL_STATS):
		wl_statsupd(wl);
		bcopy(&wl->stats, arg, MIN(plen, sizeof(wl->stats)));
		break;

#ifdef PROP_TXSTATUS
	case IOV_GVAL(IOV_WLFC_MODE):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			uint32 caps = 0;
			WLFC_SET_AFQ(caps, 1);
			WLFC_SET_REUSESEQ(caps, 1);
			WLFC_SET_REORDERSUPP(caps, 1);
			*ret_int_ptr = caps;
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_WLFC_MODE):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			if (WLFC_IS_OLD_DEF(int_val)) {
				wl->wlfc_mode = 0;
				if (int_val == WLFC_MODE_AFQ) {
					WLFC_SET_AFQ(wl->wlfc_mode, 1);
				} else if (int_val == WLFC_MODE_HANGER) {
					WLFC_SET_AFQ(wl->wlfc_mode, 0);
				} else {
					WL_ERROR(("%s: invalid wlfc mode value = 0x%x\n",
						__FUNCTION__, int_val));
				}
			} else {
				wl->wlfc_mode = int_val;
			}
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_PROPTX_CRTHRESH):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			wlc_tunables_t *tunables = (((wlc_info_t *)(wl->wlc))->pub)->tunables;

			if (wl->wlfc_info == NULL)
				return BCME_NOTREADY;

			wl->wlfc_info->fifo_credit_threshold[TX_AC_BK_FIFO] = MIN(
				MAX((int_val >> 24) & 0xff, 1), tunables->wlfcfifocreditac0);
			wl->wlfc_info->fifo_credit_threshold[TX_AC_BE_FIFO] = MIN(
				MAX((int_val >> 16) & 0xff, 1), tunables->wlfcfifocreditac1);
			wl->wlfc_info->fifo_credit_threshold[TX_AC_VI_FIFO] = MIN(
				MAX((int_val >> 8) & 0xff, 1), tunables->wlfcfifocreditac2);
			wl->wlfc_info->fifo_credit_threshold[TX_AC_VO_FIFO] = MIN(
				MAX((int_val & 0xff), 1), tunables->wlfcfifocreditac3);
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_GVAL(IOV_PROPTX_CRTHRESH):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			if (wl->wlfc_info == NULL)
				return BCME_NOTREADY;

			*ret_int_ptr =
				(wl->wlfc_info->fifo_credit_threshold[TX_AC_BK_FIFO] << 24) |
				(wl->wlfc_info->fifo_credit_threshold[TX_AC_BE_FIFO] << 16) |
				(wl->wlfc_info->fifo_credit_threshold[TX_AC_VI_FIFO] << 8) |
				(wl->wlfc_info->fifo_credit_threshold[TX_AC_VO_FIFO]);
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_PROPTX_DATATHRESH):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			if (wl->wlfc_info == NULL)
				return BCME_NOTREADY;

			wl->wlfc_info->pending_datathresh =
				MIN(MAX(int_val, 1), WLFC_MAX_PENDING_DATALEN);
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_GVAL(IOV_PROPTX_DATATHRESH):
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			if (wl->wlfc_info == NULL)
				return BCME_NOTREADY;

			*ret_int_ptr = wl->wlfc_info->pending_datathresh;
		} else
			err = BCME_UNSUPPORTED;
		break;
#endif /* PROP_TXSTATUS */

#ifdef DONGLEBUILD
	case IOV_SVAL(IOV_RTETIMESYNC):
		hndrte_set_reftime(int_val);
		break;

	case IOV_GVAL(IOV_RTETIMESYNC):
		*ret_int_ptr = hndrte_time();
		break;
#endif /* DONGLEBUILD */
#ifdef ECOUNTERS
	/* Ecounters set routine */
	case IOV_SVAL(IOV_ECOUNTERS):
		if (ECOUNTERS_ENAB())
		{
			/* params = parameters */
			/* p_len = parameter length in bytes? */
			/* arg = destination buffer where some info could be put. */
			/* len = length of destination buffer. */
			*ret_int_ptr = ecounters_config(p, plen);
		}
		else
		{
			err = BCME_UNSUPPORTED;
		}
		break;
#endif /* ECOUNTERS */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static void
BCMINITFN(_wl_init)(wl_info_t *wl)
{
	wl_reset(wl);

	wlc_init(wl->wlc);
}

void
wl_init(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_init\n", wl->unit));

#if defined(DONGLEBUILD) && defined(BCMNODOWN)
	if (!bcmreclaimed)
#endif /* defined(DONGLEBUILD) && defined (BCMNODOWN) */
		_wl_init(wl);
}
#endif /* WLC_HIGH */

uint
BCMINITFN(wl_reset)(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_reset\n", wl->unit));

	wlc_reset(wl->wlc);

	/* if there was a reschedule logic before wl_reinit, delete timer here */
	hndrte_del_timer(&wl->dpcTimer);

	/* if dpc timer is explicitly turned off, mac interrupts needs to be turned on */
	/* it assumes timer function switches on interrupt otherwise */
	if (wl->wlc_hw->up)
		wlc_intrson(wl->wlc);

	return 0;
}

bool
BCMATTACHFN(wl_alloc_dma_resources)(wl_info_t *wl, uint addrwidth)
{
	return TRUE;
}

/*
 * These are interrupt on/off enter points.
 * Since wl_isr is serialized with other drive rentries using spinlock,
 * They are SMP safe, just call common routine directly,
 */
void
wl_intrson(wl_info_t *wl)
{
	wlc_intrson(wl->wlc);
}

uint32
wl_intrsoff(wl_info_t *wl)
{
	return wlc_intrsoff(wl->wlc);
}

void
wl_intrsrestore(wl_info_t *wl, uint32 macintmask)
{
	wlc_intrsrestore(wl->wlc, macintmask);
}

#ifdef PROP_TXSTATUS
static void wl_send_credit_map(wl_info_t *wl)
{
	if (PROP_TXSTATUS_ENAB(wl->pub) && HOST_PROPTXSTATUS_ACTIVATED((wlc_info_t*)(wl->wlc))) {
		int new_total_credit = 0;

		if (POOL_ENAB(wl->pub->pktpool)) {
			new_total_credit = pktpool_len(wl->pub->pktpool);
		}

		if ((wl->wlfc_info->total_credit > 0) && (new_total_credit > 0) &&
			(new_total_credit != wl->wlfc_info->total_credit)) {
			/* re-allocate new total credit among ACs */
			wl->wlfc_info->fifo_credit[TX_AC_BK_FIFO] =
				wl->wlfc_info->fifo_credit[TX_AC_BK_FIFO] * new_total_credit /
				wl->wlfc_info->total_credit;
			wl->wlfc_info->fifo_credit[TX_AC_VI_FIFO] =
				wl->wlfc_info->fifo_credit[TX_AC_VI_FIFO] * new_total_credit /
				wl->wlfc_info->total_credit;
			wl->wlfc_info->fifo_credit[TX_AC_VO_FIFO] =
				wl->wlfc_info->fifo_credit[TX_AC_VO_FIFO] * new_total_credit /
				wl->wlfc_info->total_credit;
			wl->wlfc_info->fifo_credit[TX_BCMC_FIFO] =
				wl->wlfc_info->fifo_credit[TX_BCMC_FIFO] * new_total_credit /
				wl->wlfc_info->total_credit;
			/* give all remainig credits to BE */
			wl->wlfc_info->fifo_credit[TX_AC_BE_FIFO] = new_total_credit -
				wl->wlfc_info->fifo_credit[TX_AC_BK_FIFO] -
				wl->wlfc_info->fifo_credit[TX_AC_VI_FIFO] -
				wl->wlfc_info->fifo_credit[TX_AC_VO_FIFO] -
				wl->wlfc_info->fifo_credit[TX_BCMC_FIFO];

			/* recaculate total credit from actual pool size */
			wl->wlfc_info->total_credit = new_total_credit;
		}

		if (wl->wlfc_info->totalcredittohost != wl->wlfc_info->total_credit) {
			wlc_mac_event(wl->wlc, WLC_E_FIFO_CREDIT_MAP, NULL, 0, 0, 0,
				wl->wlfc_info->fifo_credit, sizeof(wl->wlfc_info->fifo_credit));
			wlc_mac_event(wl->wlc, WLC_E_BCMC_CREDIT_SUPPORT, NULL, 0, 0, 0, NULL, 0);

			wl->wlfc_info->totalcredittohost = wl->wlfc_info->total_credit;
		}
	}
}

void wl_reset_credittohost(struct wl_info *wl)
{
	if (wl && wl->wlfc_info) {
		wl->wlfc_info->totalcredittohost = 0;
	}
}
#endif /* PROP_TXSTATUS */

#ifdef WLC_HIGH
/* BMAC driver has alternative up/down etc. */
int
wl_up(wl_info_t *wl)
{
	int ret;
	wlc_info_t *wlc = (wlc_info_t *) wl->wlc;

	WL_TRACE(("wl%d: wl_up\n", wl->unit));

	if (wl->pub->up)
		return 0;

#if defined(BCMNODOWN)
	if (bcmreclaimed) {
		wlc_minimal_up(wl->wlc);
		return BCME_OK; /* wlc_minimal_up() takes it from here */
	}
#endif /* defined (BCMNODOWN) */

	if (wl->pub->up)
		return 0;

#ifdef BCMUSBDEV_ENABLED
	wl->pub->is_ss = WL_IS_SS_VALID;
	if (wl_dngl_is_ss(wl))
		wl->pub->is_ss |= WL_SS_ENAB;
#endif // endif

	/* Reset the hw to known state */
	ret = wlc_up(wlc);

	if (ret == 0)
		ret = wl_keep_alive_up(wl->keep_alive_info);

#ifndef RSOCK
	if (wl_oid_reclaim(wl->oid))
		hndrte_reclaim();

	if (POOL_ENAB(wl->pub->pktpool)) {
		pktpool_fill(hndrte_osh, wl->pub->pktpool, FALSE);
#ifdef PROP_TXSTATUS
		wl_send_credit_map(wl);
#endif /* PROP_TXSTATUS */
	}

#ifdef BCMFRAGPOOL
	if (POOL_ENAB(wl->pub->pktpool_lfrag))
		pktpool_fill(hndrte_osh, wl->pub->pktpool_lfrag, FALSE);
#endif /* BCMFRAGPOOL */
#ifdef BCMRXFRAGPOOL
	if (POOL_ENAB(wl->pub->pktpool_rxlfrag))
		pktpool_fill(hndrte_osh, wl->pub->pktpool_rxlfrag, FALSE);
#endif /* BCMRXFRAGPOOL */

#endif /* RSOCK */

	return ret;
}

void
wl_down(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_down\n", wl->unit));
	if (!wl->pub->up)
		return;

#ifdef BCMNODOWN
	wlc_minimal_down(wl->wlc);
#else
	wl_keep_alive_down(wl->keep_alive_info);
	wlc_down(wl->wlc);
	wl->pub->hw_up = FALSE;
#endif /* BCMNODOWN */
	wl_indicate_maccore_state(wl, LTR_SLEEP);
}

void
wl_dump_ver(wl_info_t *wl, struct bcmstrbuf *b)
{

#if defined(DONGLEBUILD)
	bcm_bprintf(b, "wl%d: %s %s version %s FWID 01-%x\n", wl->unit,
		__DATE__, __TIME__, EPI_VERSION_STR, gFWID);
#else
	bcm_bprintf(b, "wl%d: %s %s version %s\n", wl->unit,
		__DATE__, __TIME__, EPI_VERSION_STR);
#endif // endif
}

#if defined(BCMDBG) || defined(WLDUMP)
static int
wl_dump(wl_info_t *wl, struct bcmstrbuf *b)
{
	wl_dump_ver(wl, b);

	return 0;
}
#endif /* BCMDBG || WLDUMP */
#endif /* WLC_HIGH */

void
wl_monitor(wl_info_t *wl, wl_rxsts_t *rxsts, void *p)
{
#ifdef WL_MONITOR
	wlc_info_t *wlc = wl->wlc;
	struct lbuf *mon_pkt;
	mon_pkt = (struct lbuf *)p;
#ifdef BCMPCIEDEV
	wlc_d11rxhdr_t *wrxh;
	wlc_pkttag_t * pkttag = WLPKTTAG(mon_pkt);
	PKTPUSH(wlc->osh, mon_pkt, wlc->hwrxoff);
	wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc->osh, mon_pkt);
	pkttag->pktinfo.misc.snr = wlc_phy_noise_avg((wlc_phy_t *)wlc->band->pi);
	pkttag->pktinfo.misc.rssi = wrxh->rssi;

	PKTSET80211(mon_pkt);
	PKTSETMON(mon_pkt);
#else
	wlc_pkttag_t * pkttag = WLPKTTAG(p);
	wlc_pkttag_t * mon_pkttag;
	mon_pkttag = WLPKTTAG(mon_pkt);

	uint len = PKTLEN(wlc->osh, p) - D11_PHY_RXPLCP_LEN(wlc->pub->corerev);

	if ((mon_pkt = PKTGET(wlc->osh, len, FALSE)) == NULL)
		return;

	memcpy(PKTDATA(wlc->osh, mon_pkt),
			PKTDATA(wlc->osh, p) + D11_PHY_RXPLCP_LEN(wlc->pub->corerev), len);

	mon_pkttag->rxchannel = pkttag->rxchannel;
	mon_pkttag->pktinfo.misc.rssi = pkttag->pktinfo.misc.rssi;
	mon_pkttag->rspec = pkttag->rspec;

#endif /* BCMPCIEDEV */
	wl_sendup(wl, NULL, mon_pkt, 1);
#endif /* WL_MONITOR */
}

char *
wl_ifname(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif == NULL)
		return wl->dev->name;
	else
		return wlif->dev->name;
}

#if defined(WLC_HIGH)
static hndrte_devfuncs_t*
get_wl_funcs(void)
{
	return &wl_funcs;
}

/* Allocate wl_if_t, hndrte_dev_t, and wl_if_t * all together */
static wl_if_t *
wl_alloc_if(wl_info_t *wl, int iftype, uint subunit, struct wlc_if *wlcif)
{
	hndrte_dev_t *dev;
	wl_if_t *wlif;
	osl_t *osh = wl->pub->osh;
	hndrte_dev_t *bus = wl->dev->chained;
	uint len;
	int ifindex;
	wl_if_t **priv;

	/* the primary device must be binded to the bus */
	if (bus == NULL) {
		WL_ERROR(("wl%d: %s: device not binded\n", wl->pub->unit, __FUNCTION__));
		return NULL;
	}

	/* allocate wlif struct + dev struct + priv pointer */
	len = sizeof(wl_if_t) + sizeof(hndrte_dev_t) + sizeof(wl_if_t **);
	if ((wlif = MALLOC(osh, len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          (wl->pub)?wl->pub->unit:subunit, __FUNCTION__, MALLOCED(wl->pub->osh)));
		goto err;
	}
	bzero(wlif, len);

	dev = (hndrte_dev_t *)(wlif + 1);
	priv = (wl_if_t **)(dev + 1);

	wlif->dev = dev;
	wlif->wlcif = wlcif;

	dev->funcs = get_wl_funcs();
	dev->softc = wl;
	if (iftype == WL_IFTYPE_WDS) {
		wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlcif);
		ASSERT(cfg != NULL);
		snprintf(dev->name, HNDRTE_DEV_NAME_MAX, "wds%d.%d.%d", wl->pub->unit,
				WLC_BSSCFG_IDX(cfg), subunit);
	} else {
		snprintf(dev->name, HNDRTE_DEV_NAME_MAX, "wl%d.%d", wl->pub->unit, subunit);
	}
	*priv = wlif;

	/* use the return value as the i/f no. in the event to the host */
#ifdef DONGLEBUILD
	if ((ifindex = bus_ops->binddev(bus, dev)) < 1) {
		WL_ERROR(("wl%d: %s: bus_binddev failed\n", wl->pub->unit, __FUNCTION__));
		goto err;
	}

	if (ifindex >= WLC_MAXBSSCFG) {
		WL_ERROR(("wl%d: %s: too many virtual devices: ifidx:%d\n",
				wl->pub->unit, __FUNCTION__, ifindex));

		if (bus_ops->unbinddev(bus, dev) < 1) {
			WL_ERROR(("wl%d: %s: bus_unbinddev failed\n",
					wl->pub->unit, __FUNCTION__));
		}
		goto err;
	}
#else
	ifindex = subunit;
#endif /* DONGLEBUILD */
	wlcif->index = (uint8)ifindex;

	/* create and populate arpi for this IF */
	if (ARPOE_SUPPORT(wl->pub)) {
		wlif->arpi = wl_arp_alloc_ifarpi(wl->arpi, wlcif);
	}

#ifdef WLNDOE
	if (NDOE_ENAB(wl->pub))
		wlif->ndi = wl_nd_alloc_ifndi(wl->ndi, wlcif);
#endif // endif

#ifdef WLNFC
	if (NFC_ENAB(wl->pub)) {
		wlif->nfci = wl_nfc_alloc_ifnfci(wl->nfci, wlcif);
	}
#endif // endif

	return wlif;

err:
	if (wlif != NULL)
		MFREE(osh, wlif, len);
	return NULL;
}

static void
wl_free_if(wl_info_t *wl, wl_if_t *wlif)
{
	/* free arpi for this IF */
	if (ARPOE_SUPPORT(wl->pub)) {
		wl_arp_free_ifarpi(wlif->arpi);
	}

#ifdef WLNDOE
	/* free ndi for this IF */
	if (NDOE_ENAB(wl->pub)) {
		wl_nd_free_ifndi(wlif->ndi);
	}
#endif // endif

#ifdef WLNFC
	/* free nfci for this IF */
	if (NFC_ENAB(wl->pub)) {
		wl_nfc_free_ifnfci(wlif->nfci);
	}
#endif // endif

	MFREE(wl->pub->osh, wlif, sizeof(wl_if_t) + sizeof(hndrte_dev_t) + sizeof(wl_if_t *));
}

struct wl_if *
wl_add_if(wl_info_t *wl, struct wlc_if *wlcif, uint unit, struct ether_addr *remote)
{
	wl_if_t *wlif;

	wlif = wl_alloc_if(wl, remote != NULL ? WL_IFTYPE_WDS : WL_IFTYPE_BSS, unit, wlcif);

	if (wlif == NULL) {
		WL_ERROR(("wl%d: %s: failed to create %s interface %d\n", wl->pub->unit,
			__FUNCTION__, (remote)?"WDS":"BSS", unit));
		return NULL;
	}

	return wlif;
}

void
wl_del_if(wl_info_t *wl, struct wl_if *wlif)
{
#ifdef DONGLEBUILD
	hndrte_dev_t *bus = wl->dev->chained;

	if (bus_ops->unbinddev(bus, wlif->dev) < 1)
		WL_ERROR(("wl%d: %s: bus_unbinddev failed\n", wl->pub->unit, __FUNCTION__));
#endif // endif
	WL_TRACE(("wl%d: %s: bus_unbinddev idx %d\n", wl->pub->unit, __FUNCTION__,
		wlif->wlcif->index));
	wl_free_if(wl, wlif);
}
#endif /* WLC_HIGH */

static void
wl_timer_main(hndrte_timer_t *t)
{
	ASSERT(t->context); ASSERT(t->auxfn);

	t->auxfn(t->data);
}

#undef wl_init_timer

struct wl_timer *
wl_init_timer(wl_info_t *wl, void (*fn)(void* arg), void *arg, const char *name)
{
	return (struct wl_timer *)hndrte_init_timer(wl, arg, wl_timer_main, fn);
}

void
wl_free_timer(wl_info_t *wl, struct wl_timer *t)
{
	hndrte_free_timer((hndrte_timer_t *)t);
}

void
wl_add_timer(wl_info_t *wl, struct wl_timer *t, uint ms, int periodic)
{
	ASSERT(t != NULL);
	hndrte_add_timer((hndrte_timer_t *)t, ms, periodic);
}

bool
wl_del_timer(wl_info_t *wl, struct wl_timer *t)
{
	if (t == NULL)
		return TRUE;
	return hndrte_del_timer((hndrte_timer_t *)t);
}

#ifdef PROP_TXSTATUS
#ifdef PROP_TXSTATUS_DEBUG
static void
hndrte_wlfc_info_dump(uint32 arg, uint argc, char *argv[])
{
	extern void wlfc_display_debug_info(void* _wlc, int hi, int lo);
	wlfc_info_state_t* wlfc = wlfc_state_get(((wlc_info_t*)arg)->wl);
	int hi = 0;
	int lo = 0;
	int i;

	if (argc > 2) {
		hi = atoi(argv[1]);
		lo = atoi(argv[2]);
	}
	printf("packets: (from_host-nost-sig,status_back,stats_other,credit_back,creditin) = "
		"(%d-%d-%d,%d,%d,%d,%d)\n",
		wlfc->stats.packets_from_host,
		wlfc->dbgstats->nost_from_host,
		wlfc->dbgstats->sig_from_host,
		wlfc->stats.txstatus_count,
		wlfc->stats.txstats_other,
		wlfc->dbgstats->creditupdates,
		wlfc->dbgstats->creditin);
	printf("credits for fifo: fifo[0-5] = (");
	for (i = 0; i < NFIFO; i++)
		printf("%d,", wlfc->dbgstats->credits[i]);
	printf(")\n");
	printf("stats: (header_only_alloc, realloc_in_sendup): (%d,%d)\n",
		wlfc->dbgstats->nullpktallocated,
		wlfc->dbgstats->realloc_in_sendup);
	printf("wlc_toss, wlc_sup = (%d, %d)\n",
		wlfc->dbgstats->wlfc_wlfc_toss,
		wlfc->dbgstats->wlfc_wlfc_sup);
	printf("debug counts:for_D11,to_D11,free_exceptions:(%d,%d,%d)\n",
		(wlfc->stats.packets_from_host - (wlfc->dbgstats->wlfc_wlfc_toss +
		wlfc->dbgstats->wlfc_wlfc_sup)
		+ wlfc->stats.txstats_other),
		wlfc->dbgstats->wlfc_to_D11,
		wlfc->dbgstats->wlfc_pktfree_except);
#ifdef AP
	wlfc_display_debug_info((void*)arg, hi, lo);
#endif // endif
	return;
}
#endif /* PROP_TXSTATUS_DEBUG */

static int
BCMATTACHFN(wlfc_initialize)(wl_info_t *wl, wlc_info_t *wlc)
{
	wlc_tunables_t *tunables = wlc->pub->tunables;

#ifdef PROP_TXSTATUS_DEBUG
	if (wlc_pcb_fn_set(wlc->pcb, 2, WLF2_PCB3_WLFC, wlfc_hostpkt_callback) != BCME_OK) {
		WL_ERROR(("wlc_pcb_fn_set(wlfc) failed in %s()", __FUNCTION__));
		goto fail;
	}
#endif // endif
	wl->wlfc_info = (wlfc_info_state_t*)MALLOC(wl->pub->osh, sizeof(wlfc_info_state_t));
	if (wl->wlfc_info == NULL) {
		WL_ERROR(("MALLOC() failed in %s() for wlfc_info", __FUNCTION__));
		goto fail;
	}
	memset(wl->wlfc_info, 0, sizeof(wlfc_info_state_t));

#ifdef PROP_TXSTATUS_DEBUG
	wl->wlfc_info->dbgstats = (wlfc_fw_dbgstats_t*)MALLOC(wl->pub->osh,
	    sizeof(wlfc_fw_dbgstats_t));
	if (wl->wlfc_info->dbgstats == NULL) {
		WL_ERROR(("MALLOC() failed in %s() for dbgstats", __FUNCTION__));
		goto fail;
	}
	memset(wl->wlfc_info->dbgstats, 0, sizeof(wlfc_fw_dbgstats_t));
#endif /* PROP_TXSTATUS_DEBUG */

	wl->wlfc_info->pending_datathresh = WLFC_PENDING_TRIGGER_WATERMARK;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_BE_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_be;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_BK_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_bk;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_VI_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vi;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_VO_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vo;
	wl->wlfc_info->fifo_credit_threshold[TX_BCMC_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_bcmc;
	wl->wlfc_info->fifo_credit_threshold[TX_ATIM_FIFO] = 1;	/* send credit back ASAP */
	wl->wlfc_info->fifo_credit[TX_AC_BE_FIFO] =
		tunables->wlfcfifocreditac1;
	wl->wlfc_info->fifo_credit[TX_AC_BK_FIFO] =
		tunables->wlfcfifocreditac0;
	wl->wlfc_info->fifo_credit[TX_AC_VI_FIFO] =
		tunables->wlfcfifocreditac2;
	wl->wlfc_info->fifo_credit[TX_AC_VO_FIFO] =
		tunables->wlfcfifocreditac3;
	wl->wlfc_info->fifo_credit[TX_BCMC_FIFO] =
		tunables->wlfcfifocreditbcmc;
	wl->wlfc_info->totalcredittohost = 0;
	wl->wlfc_info->total_credit = wl->wlfc_info->fifo_credit[TX_AC_BE_FIFO] +
		wl->wlfc_info->fifo_credit[TX_AC_BK_FIFO] +
		wl->wlfc_info->fifo_credit[TX_AC_VI_FIFO] +
		wl->wlfc_info->fifo_credit[TX_AC_VO_FIFO] +
		wl->wlfc_info->fifo_credit[TX_BCMC_FIFO];
	wl->wlfc_info->wlfc_trigger = tunables->wlfc_trigger;
	wl->wlfc_info->wlfc_fifo_bo_cr_ratio = tunables->wlfc_fifo_bo_cr_ratio;
	wl->wlfc_info->wlfc_comp_txstatus_thresh = tunables->wlfc_comp_txstatus_thresh;

	wlc->wlfc_data = MALLOC(wl->pub->osh, sizeof(wlfc_mac_desc_handle_map_t));
	if (wlc->wlfc_data == NULL) {
		WL_ERROR(("MALLOC() failed in %s() for wlfc_mac_desc_handle_map_t", __FUNCTION__));
		goto fail;
	}
	memset(wlc->wlfc_data, 0, sizeof(wlfc_mac_desc_handle_map_t));

	wlc->wlfc_vqdepth = WLFC_DEFAULT_FWQ_DEPTH;

	/* init and add a timer for periodic wlfc signal sendup */
	wl->wlfc_info->wl_info = wl;
	if (!(((wlfc_info_state_t*)wl->wlfc_info)->fctimer = wl_init_timer(wl,
		wlfc_sendup_timer, wl->wlfc_info, "wlfctimer"))) {
		WL_ERROR(("wl%d: wl_init_timer for wlfc timer failed\n", wl->pub->unit));
		goto fail;
	}
	if (!BCMPCIEDEV_ENAB(((wlc_info_t*)(wl->wlc))->pub)) {
		wlc_eventq_set_ind(wlc->eventq, WLC_E_FIFO_CREDIT_MAP, TRUE);
		wlc_eventq_set_ind(wlc->eventq, WLC_E_BCMC_CREDIT_SUPPORT, TRUE);
	}

#ifdef PROP_TXSTATUS_DEBUG
	wlc->wlfc_flags = WLFC_FLAGS_RSSI_SIGNALS | WLFC_FLAGS_XONXOFF_SIGNALS |
		WLFC_FLAGS_CREDIT_STATUS_SIGNALS;
	hndrte_cons_addcmd("np", (cons_fun_t)hndrte_wlfc_info_dump, (uint32)wlc);
#else
	/* All TLV s are turned off by default */
	wlc->wlfc_flags = 0;
#endif // endif

	/* Enable compressed txstatus by default */
	wlc->comp_stat_enab = TRUE;

	return BCME_OK;

fail:
	return BCME_ERROR;
}

static void
wlfc_sendup_timer(void* arg)
{
	wlfc_info_state_t* wlfc = (wlfc_info_state_t*)arg;

	wlfc->timer_started = 0;
	wlfc_sendup_ctl_info_now(wlfc->wl_info);
}

#ifdef PROP_TXSTATUS_DEBUG
static void
wlfc_hostpkt_callback(wlc_info_t *wlc, void *p, uint txstatus)
{
	wlc_pkttag_t *pkttag;

	ASSERT(p != NULL);
	pkttag = WLPKTTAG(p);
	if (WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST) {
		wlc->wl->wlfc_info->dbgstats->wlfc_pktfree_except++;
	}
}
#endif /* PROP_TXSTATUS_DEBUG */

uint8
wlfc_allocate_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map)
{
	int i;

	for (i = 0; i < NBITS(uint32); i++) {
		if (!(map->bitmap & (1 << i))) {
			map->bitmap |= 1 << i;
			/* we would use 3 bits only */
			map->replay_counter++;
			/* ensure a non-zero replay counter value */
			if (!(map->replay_counter & 7))
				map->replay_counter = 1;
			return i | (map->replay_counter << 5);
		}
	}
	return WLFC_MAC_DESC_ID_INVALID;
}

void
wlfc_release_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map, uint8 handle)
{

	if (handle < WLFC_MAC_DESC_ID_INVALID) {
		/* unset the allocation flag in bitmap */
		map->bitmap &= ~(1 << WLFC_MAC_DESC_GET_LOOKUP_INDEX(handle));
	}
	else {
	}
	return;
}

#endif /* PROP_TXSTATUS */

#define	CID_FMT_HEX	0x9999

#ifdef FWID
static const char BCMATTACHDATA(rstr_fmt_hello)[] =
	"wl%d: Broadcom BCM%s 802.11 Wireless Controller %s FWID 01-%x\n";
#else
static const char BCMATTACHDATA(rstr_fmt_hello)[] =
	"wl%d: Broadcom BCM%s 802.11 Wireless Controller %s\n";
#endif /* FWID */

#if defined(WLC_HIGH) && defined(WLC_LOW)
static void
wl_devpwrstchg_notify(void *wl_p, bool hostmem_access_enabled)
{
	wl_info_t *wl = (wl_info_t *)wl_p;
	WL_PRINT(("%s: hostmem_access_enabled %d\n", __FUNCTION__, hostmem_access_enabled));
	wlc_devpwrstchg_change(wl->wlc, hostmem_access_enabled);
}
static void
wl_generate_pme_to_host(void *wl_p, bool state)
{
	wl_info_t *wl = (wl_info_t *)wl_p;
	wlc_generate_pme_to_host(wl->wlc, state);
}
#endif /* defined(WLC_HIGH) && defined(WLC_LOW) */

/* xxx wl_probe is always a RAM function.  It should be static, but is
 * not because BCMROMBUILD builds say 'defined but not used'.
 */
void *
BCMATTACHFN(wl_probe)(hndrte_dev_t *dev, void *regs, uint bus, uint16 devid,
                      uint coreid, uint unit)
{
	wl_info_t *wl;
	wlc_info_t *wlc;
	osl_t *osh;
	uint err;
	uint orig_unit = unit;
	char cidstr[8];

#ifdef BCMPCIEDEV_ENABLED
	/* When we have two full dongle radios hooked up to the host (as is the case
	 * with router platforms), we need to make sure the instances come up with
	 * different wl units - wl0 and wl1. But, since one dongle is not aware of
	 * the other, we need this information to come from the host.
	 */
	uint wlunit = 0;
	extern char *_vars;
	wlunit = getintvar(_vars, "wlunit");
	unit += wlunit;
#endif /* BCMPCIEDEV_ENABLED */

	/* allocate private info */
	if (!(wl = (wl_info_t *)MALLOC(NULL, sizeof(wl_info_t)))) {
		WL_ERROR(("wl%d: MALLOC failed\n", unit));
		return NULL;
	}
	bzero(wl, sizeof(wl_info_t));

	wl->unit = unit;

#ifdef PROP_TXSTATUS
	/* no wlfc feature default enabled */
	wl->wlfc_mode = 0;
#ifdef BCMPCIEDEV_ENABLED
	WLFC_SET_REUSESEQ(wl->wlfc_mode, 1);
#endif // endif

#endif /* PROP_TXSTATUS */
	osh = osl_attach(dev);
#ifdef ECOUNTERS
	if (ECOUNTERS_ENAB()) {
		ecounters_register_entity_entry_point(ECOUNTERS_TOP_LEVEL_SW_ENTITY_WL,
			wl_ecounters_entry_point);
	}
#endif // endif

#ifdef WLC_LOW_ONLY
	wl->rpc_th = bcm_rpc_tp_attach(osh, dev);
	if (wl->rpc_th == NULL) {
		WL_ERROR(("wl%d: bcm_rpc_tp_attach failed\n", unit));
		goto fail;
	}
	bcm_rpc_tp_txflowctlcb_init(wl->rpc_th, wl, wl_rpc_txflowctl);

	wl->rpc = bcm_rpc_attach(NULL, NULL, wl->rpc_th, NULL);

	if (wl->rpc == NULL) {
		WL_ERROR(("wl%d: bcm_rpc_attach failed\n", unit));
		goto fail;
	}

	bcm_rpc_tp_agg_limit_set(wl->rpc_th,
		BCM_RPC_TP_DNGL_AGG_DEFAULT_SFRAME, BCM_RPC_TP_DNGL_AGG_DEFAULT_BYTE);

#endif /* WLC_LOW_ONLY */

	/* TODO: Registry is to be created only once for RSDB;
	 * Both WLC will share information over wlc->objr
	*/
	wl->objr = wlc_obj_registry_alloc(osh, WLC_OBJR_MAX_KEYS);

	/* common load-time initialization */
	if (!(wlc = wlc_attach(wl,			/* wl */
	                       VENDOR_BROADCOM,		/* vendor */
	                       devid,			/* device */
	                       unit,			/* unit */
	                       FALSE,			/* piomode */
	                       osh,			/* osh */
	                       regs,			/* regsva */
	                       bus,			/* bustype */
#ifdef WLC_LOW_ONLY
	                       wl->rpc,			/* BMAC, overloading, to change */
#else
			       wl,			/* sdh */
#endif // endif
	                       wl->objr,		/* Registry for RSDB usage */
	                       &err))) {		/* perr */
		WL_ERROR(("wl%d: wlc_attach failed with error %d\n", unit, err));
		goto fail;
	}

	wl->wlc = (void *)wlc;
	wl->pub = wlc_pub((void *)wlc);
	wl->wlc_hw = wlc->hw;
	wl->dev = dev;
	wl->dpcTimer.mainfn = _wl_dpc;
	wl->dpcTimer.data = wl;

	snprintf(dev->name, HNDRTE_DEV_NAME_MAX, "wl%d", unit);

	if (si_chipid(wlc->hw->sih) < CID_FMT_HEX)
		snprintf(cidstr, sizeof(cidstr), "%04x", si_chipid(wlc->hw->sih));
	else
		snprintf(cidstr, sizeof(cidstr), "%d", si_chipid(wlc->hw->sih));

#ifdef FWID
	/* print hello string */
	printf(rstr_fmt_hello, unit, cidstr, EPI_VERSION_STR, gFWID);
#else
	/* print hello string */
	printf(rstr_fmt_hello, unit, cidstr, EPI_VERSION_STR);
#endif // endif

#ifndef HNDRTE_POLLING
	if (hndrte_add_isr(0, coreid, orig_unit, (isr_fun_t)wl_isr, dev, bus)) {
		WL_ERROR(("wl%d: hndrte_add_isr failed\n", unit));
		goto fail;
	}
#endif	/* HNDRTE_POLLING */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		if (BCME_OK != wlfc_initialize(wl, wlc)) {
			WL_ERROR(("wl%d: wlfc_initialize failed\n", unit));
			goto fail;
		}
	}
#endif // endif

#ifdef BCMPCIEDEV
	/* Initialize the flowring_link_update at run-time */
	if (BCMPCIEDEV_ENAB(wl->pub)) {
		wl_funcs.flowring_link_update = wl_flowring_update;
	}
#endif /* BCMPCIEDEV */

#ifdef WLC_LOW_ONLY
	wl->rpc_dispatch_ctx.rpc = wl->rpc;
	wl->rpc_dispatch_ctx.wlc = wlc;
	wl->rpc_dispatch_ctx.wlc_hw = wlc->hw;
	bcm_rpc_rxcb_init(wl->rpc, &wl->rpc_dispatch_ctx, wl_rpc_bmac_dispatch, wl,
	                  wl_rpc_down, wl_rpc_resync, NULL);

	hndrte_cons_addcmd("wlhist", do_wlhist_cmd, (uint32)wl);
	hndrte_cons_addcmd("dpcdump", do_wldpcdump_cmd, (uint32)wl);

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	wl->lowmem_free_info.free_fn = wl_lowmem_free;
	wl->lowmem_free_info.free_arg = wl;

	hndrte_pt_lowmem_register(&wl->lowmem_free_info);
#endif /* HNDRTE_PT_GIANT && DMA_TX_FREE */

#else /* WLC_LOW_ONLY */
#ifdef STA
	/* align watchdog with tbtt indication handling in PS mode */
	wl->pub->align_wd_tbtt = TRUE;

	/* Enable TBTT Interrupt */
	wlc_bmac_enable_tbtt(wlc->hw, TBTT_WD_MASK, TBTT_WD_MASK);
#endif // endif

	wlc_eventq_set_ind(wlc->eventq, WLC_E_IF, TRUE);

	/* initialize OID handler state */
	if ((wl->oid = wl_oid_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_oid_attach failed\n", unit));
		goto fail;
	}

#if defined(WLPFN) && !defined(WLPFN_DISABLED)
	/* initialize PFN handler state */
	if ((wl->pfn = wl_pfn_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_pfn_attach failed\n", unit));
		goto fail;
	}
	wl->pub->_wlpfn = TRUE;
#endif /* WLPFN */

#if defined(TOE) && !defined(TOE_DISABLED)
	/* allocate the toe info struct */
	if ((wl->toei = wl_toe_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_toe_attach failed\n", unit));
		goto fail;
	}
#endif /* TOE && !TOE_DISABLED */

	/* allocate the keep-alive info struct */
	if ((wl->keep_alive_info = wl_keep_alive_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_keep_alive_attach failed\n", unit));
		goto fail;
	}

#ifdef WL_EVENTQ
	/* allocate wl_eventq info struct */
	if ((wl->wlevtq = wl_eventq_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_eventq_attach failed\n", unit));
		goto fail;
	}
#endif /* WL_EVENTQ */

#if (defined(P2PO) && !defined(P2PO_DISABLED)) || (defined(ANQPO) && \
	!defined(ANQPO_DISABLED))
	/* allocate gas info struct */
	if ((wl->gas = wl_gas_attach(wlc, wl->wlevtq)) == NULL) {
		WL_ERROR(("wl%d: wl_gas_attach failed\n", unit));
		goto fail;
	}
#endif /* P2PO || ANQPO */

#if defined(P2PO) && !defined(P2PO_DISABLED)
	/* allocate the disc info struct */
	if ((wl->disc = wl_disc_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_disc_attach failed\n", unit));
		goto fail;
	}

	/* allocate the p2po info struct */
	if ((wl->p2po = wl_p2po_attach(wlc, wl->wlevtq, wl->gas, wl->disc)) == NULL) {
		WL_ERROR(("wl%d: wl_p2po_attach failed\n", unit));
		goto fail;
	}
#endif /* defined(P2PO) && !defined(P2PO_DISABLED) */

#if defined(ANQPO) && !defined(ANQPO_DISABLED)
	/* allocate the anqpo info struct */
	if ((wl->anqpo = wl_anqpo_attach(wlc, wl->gas)) == NULL) {
		WL_ERROR(("wl%d: wl_anqpo_attach failed\n", unit));
		goto fail;
	}
#endif /* defined(ANQPO) && !defined(ANPO_DISABLED) */

	/* allocate the packet filter info struct */
	if ((wl->pkt_filter_info = wlc_pkt_filter_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wlc_pkt_filter_attach failed\n", unit));
		goto fail;
	}
#if defined(D0_COALESCING)
	/* allocate the packet filter info struct */
	if ((wl->d0_filter_info = wlc_d0_filter_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wlc_pkt_filter_attach failed\n", unit));
		goto fail;
	}
#endif /* D0_COALESCING */

#if defined(NWOE) && !defined(NWOE_DISABLED)
	/* allocate the nwoe info struct */
	if ((wl->nwoei = wl_nwoe_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_nwoe_attach failed\n", unit));
		goto fail;
	}
#endif /* NWOE && !NWOE_DISABLED */

#endif /* WLC_HIGH */

#if defined(BCM_OL_DEV) || defined(WLC_HIGH)
#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	/* allocate the nd info struct */
	if ((wl->ndi = wl_nd_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_nd_attach failed\n", unit));
		goto fail;
	}
#endif // endif
#if defined(WLNFC) && !defined(WLNFC_DISABLED)
	/* allocate the nfc info struct */
	if ((wl->nfci = wl_nfc_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_nfc_attach failed\n", unit));
		goto fail;
	}
#endif /* WLNFC && !WLNFC_DISABLED */
#if defined(ARPOE) && !defined(ARPOE_DISABLED)
	/* allocate the arp info struct */
	if ((wl->arpi = wl_arp_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_arp_attach failed\n", unit));
		goto fail;
	}
	wl->pub->_arpoe_support = TRUE;
#endif /* ARPOE && !ARPOE_DISABLED */
#ifdef ICMPKAOE

	/* allocate the ICMP info struct */
	if ((wl->icmpi = wl_icmp_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_icmp_attach failed\n", unit));
		goto fail;
	}
#endif /* ICMPKAOE */

#ifdef TCPKAOE

	/* allocate the TCP keep-alive info struct */
	if ((wl->tcp_keep_info = wl_tcp_keep_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wlc_tcp_keep_attach failed\n", unit));
		goto fail;
	}
#endif // endif
#endif /* defined(BCM_OL_DEV) || defined(WLC_HIGH) */
#ifdef	CONFIG_WLU
	hndrte_cons_addcmd("wl", do_wl_cmd, (uint32)wl);
#endif /* CONFIG_WLU */

#ifdef BCMDBG
	hndrte_cons_addcmd("wlmsg", do_wlmsg_cmd, (uint32)wl);
#endif // endif
#ifdef WLC_HIGH

	/* register module */
	if (wlc_module_register(wlc->pub, wl_iovars, "wl", wl, wl_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: wlc_module_register() failed\n", unit));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLDUMP)
	wlc_dump_register(wl->pub, "wl", (dump_fn_t)wl_dump, (void *)wl);
#endif // endif

#endif /* WLC_HIGH */

#ifdef MSGTRACE
	/* We set up the event and start the tracing immediately */
	wlc_eventq_set_ind(wlc->eventq, WLC_E_TRACE, TRUE);
	msgtrace_init(osh, wlc, wl->dev, (msgtrace_func_send_t)wlc_event_sendup_trace);
#endif // endif

#ifdef LOGTRACE
	/* We set up the event and start the tracing immediately */
	wlc_eventq_set_ind(wlc->eventq, WLC_E_TRACE, TRUE);
	logtrace_init(osh, wlc, wl->dev, (msgtrace_func_send_t)wlc_event_sendup_trace);
	logtrace_start();
#endif // endif
#if defined(WLC_HIGH) && defined(WLC_LOW)
	/* register callback for the device pwr state change */
	if (hndrte_register_devpwrstchg_callback(wl_devpwrstchg_notify, wl)) {
		WL_ERROR(("wl%d: hndrte_register_devpwrstchg_callback failed\n", unit));
		goto fail;
	}
	/* register callback for the device pwr state change */
	if (hndrte_register_generate_pme_callback(wl_generate_pme_to_host, wl)) {
		WL_ERROR(("wl%d: hndrte_register_generate_pme_callback failed\n", unit));
		goto fail;
	}
#endif /* WLC_HIGH && WLC_LOW */

	return (wl);

fail:
	wl_free(wl, osh);
	return NULL;
}

static void
BCMATTACHFN(wl_free)(wl_info_t *wl, osl_t *osh)
{
#ifndef BCMNODOWN
#if defined(ARPOE) && !defined(ARPOE_DISABLED)
if (wl->arpi)
	wl_arp_detach(wl->arpi);
#endif /* ARPOE && !ARPOE_DISABLED */
#ifdef ICMPKAOE
	if (wl->icmpi)
		wl_icmp_detach(wl->icmpi);
#endif // endif
#ifdef TCPKAOE
	if (wl->tcp_keep_info)
		wl_tcp_keep_detach(wl->tcp_keep_info);
#endif // endif
#ifdef WLC_HIGH
#if defined(D0_COALESCING)
	if (wl->d0_filter_info)
		wlc_d0_filter_detach(wl->d0_filter_info);
#endif /* D0_COALESCING */
	if (wl->pkt_filter_info)
		wlc_pkt_filter_detach(wl->pkt_filter_info);
	if (wl->keep_alive_info)
		wl_keep_alive_detach(wl->keep_alive_info);

#if defined(TOE) && !defined(TOE_DISABLED)
	if (wl->toei)
		wl_toe_detach(wl->toei);
#endif /* TOE && !TOE_DISABLED */

#if defined(NWOE) && !defined(NWOE_DISABLED)
	if (wl->nwoei)
		wl_nwoe_detach(wl->nwoei);
#endif /* NWOE && !NWOE_DISABLED */
#ifdef WLPFN
	if (WLPFN_ENAB(wl->pub) && wl->pfn)
		wl_pfn_detach(wl->pfn);
#endif // endif
#if defined(P2PO) && !defined(P2PO_DISABLED)
	if (wl->p2po)
		wl_p2po_detach(wl->p2po);
	if (wl->disc)
		wl_disc_detach(wl->disc);
#endif /* defined(P2PO) && !defined(P2PO_DISABLED) */
#if defined(ANQPO) && !defined(ANQPO_DISABLED)
	if (wl->anqpo)
		wl_anqpo_detach(wl->anqpo);
#endif /* defined(ANQPO) && !defined(ANQPO_DISABLED) */
#if (defined(P2PO) && !defined(P2PO_DISABLED)) || (defined(ANQPO) && \
	!defined(ANQPO_DISABLED))
	if (wl->gas)
		wl_gas_detach(wl->gas);
#endif	/* P2PO || ANQPO */
#ifdef WL_EVENTQ
	if (wl->wlevtq)
		wl_eventq_detach(wl->wlevtq);
#endif /* WL_EVENTQ */
	if (wl->oid)
		wl_oid_detach(wl->oid);
#endif /* WLC_HIGH */

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	hndrte_pt_lowmem_unregister(&wl->lowmem_free_info);
#endif // endif

#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	if (wl->ndi)
		wl_nd_detach(wl->ndi);
#endif // endif

#if defined(WLNFC) && !defined(WLNFC_DISABLED)
	if (wl->nfci)
		wl_nfc_detach(wl->nfci);
#endif // endif

	/* common code detach */
	if (wl->wlc)
		wlc_detach(wl->wlc);

#ifdef WLC_LOW_ONLY
	/* rpc, rpc_transport detach */
	if (wl->rpc)
		bcm_rpc_detach(wl->rpc);
	if (wl->rpc_th)
		bcm_rpc_tp_detach(wl->rpc_th);
#endif /* WLC_LOG_ONLY */

	if (wl->objr) {
		wlc_obj_registry_free(wl->objr, osh);
	}

#endif /* BCMNODOWN */
	MFREE(osh, wl, sizeof(wl_info_t));
}

static void
wl_isr(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	bool dpc;

	WL_TRACE(("wl%d: wl_isr\n", wl->unit));

	/* call common first level interrupt handler */
	if (wlc_isr(wl->wlc, &dpc)) {
		/* if more to do... */
		if (dpc) {
			wl_dpc(wl);
		}
	}
}

static void
wl_dpc(wl_info_t *wl)
{
	bool resched = 0;
	bool bounded = TRUE;

	/* call the common second level interrupt handler if we have enough memory */
	if (wl->wlc_hw->up) {
		wlc_dpc_info_t dpci = {0};
#ifdef WLC_LOW_ONLY
		if (!wl->dpc_stopped) {
			if (wl->wlc_hw->rpc_dngl_agg & BCM_RPC_TP_DNGL_AGG_DPC) {
				bcm_rpc_tp_agg_set(wl->rpc_th, BCM_RPC_TP_DNGL_AGG_DPC, TRUE);
			}

			resched = wlc_dpc(wl->wlc, bounded, &dpci);

			if (wl->wlc_hw->rpc_dngl_agg & BCM_RPC_TP_DNGL_AGG_DPC) {
				bcm_rpc_tp_agg_set(wl->rpc_th, BCM_RPC_TP_DNGL_AGG_DPC, FALSE);
			}
		} else {
			WL_TRACE(("dpc_stop is set!\n"));
			wl->dpc_requested = TRUE;
			return;
		}
#else
		resched = wlc_dpc(wl->wlc, bounded, &dpci);
#endif /* WLC_LOW_ONLY */
	}

	/* wlc_dpc() may bring the driver down */
	if (!wl->wlc_hw->up)
		return;

	 /* Driver is not down. Flush accumulated txrxstatus here */
	hndrte_flush_glommed_txrxstatus();

	/* re-schedule dpc or re-enable interrupts */
	if (resched) {
		if (!hndrte_add_timer(&wl->dpcTimer, 0, FALSE))
			ASSERT(FALSE);
	} else
		wlc_intrson(wl->wlc);
}

static void
_wl_dpc(hndrte_timer_t *timer)
{
	wl_info_t *wl = (wl_info_t *) timer->data;

	if (wl->wlc_hw->up) {
		wlc_intrsupd(wl->wlc);
		wl_dpc(wl);
	}
}

static int
wl_open(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	int ret;

	WL_ERROR(("wl%d: wl_open\n", wl->unit));

#ifdef BCM_OL_DEV
	ret = wlc_up(wl->wlc);
#endif /* BCM_OL_DEV */

	if ((ret = wlc_ioctl(wl->wlc, WLC_UP, NULL, 0, NULL)))
		return ret;

#ifdef HNDRTE_JOIN_SSID
	/*
	 * Feature useful for repetitious testing: if Make defines HNDRTE_JOIN_SSID
	 * to an SSID string, automatically join that SSID at driver startup.
	 */
	{
		wlc_info_t *wlc = wl->wlc;
		int infra = 1;
		int auth = 0;
		char *ss = HNDRTE_JOIN_SSID;
		wlc_ssid_t ssid;

		printf("Joining %s:\n", ss);
		/* set infrastructure mode */
		printf("  Set Infra\n");
		wlc_ioctl(wlc, WLC_SET_INFRA, &infra, sizeof(int), NULL);
		printf("  Set Auth\n");
		wlc_ioctl(wlc, WLC_SET_AUTH, &auth, sizeof(int), NULL);
		printf("  Set SSID %s\n", ss);
		ssid.SSID_len = strlen(ss);
		bcopy(ss, ssid.SSID, ssid.SSID_len);
		wlc_ioctl(wlc, WLC_SET_SSID, &ssid, sizeof(wlc_ssid_t), NULL);
	}
#endif /* HNDRTE_JOIN_SSID */

#if defined(PROP_TXSTATUS) || defined(BCMPCIEDEV)
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)||
		BCMPCIEDEV_ENAB(((wlc_info_t *)(wl->wlc))->pub))
	{
		wlc_bsscfg_t* bsscfg = wlc_bsscfg_find_by_wlcif(wl->wlc, NULL);
#ifdef PROP_TXSTATUS
		wl_send_credit_map(wl);
#endif // endif
		wlc_if_event(wl->wlc, WLC_E_IF_ADD, bsscfg->wlcif);
	}
#endif /* PROP_TXSTATUS || BCMPCIEDEV */

	return (ret);
}
#ifdef BCM_OL_DEV
void * wl_get_arpi(wl_info_t *wl, struct wl_if *wlif)
{
		return wl->arpi;
}
#ifdef ICMPKAOE
void * wl_get_icmpi(wl_info_t *wl, struct wl_if *wlif)
{
		return wl->icmpi;
}

#endif /* ICMPKAOE */
#endif /* BCM_OL_DEV */

#ifdef TCPKAOE
void * wl_get_tcpkeepi(wl_info_t *wl, struct wl_if *wlif)
{
		return wl->tcp_keep_info;
}
#endif // endif

#ifdef WLC_HIGH
#ifdef TOE
static int
_wl_toe_recv_proc(wl_info_t *wl, void *p)
{
	if (TOE_ENAB(wl->pub))
		(void)wl_toe_recv_proc(wl->toei, p);
	return 0;
}
#endif // endif

static bool
wl_hwfilter(wl_info_t *wl, void *p)
{
	struct ether_header *eh = (struct ether_header *)PKTDATA(wl->pub->osh, p);

	if (((wl->hwfflags & HWFFLAG_UCAST) && !ETHER_ISMULTI(eh->ether_dhost)) ||
	    ((wl->hwfflags & HWFFLAG_BCAST) && ETHER_ISBCAST(eh->ether_dhost)))
		return TRUE;

	return FALSE;
}

#ifdef PROP_TXSTATUS
int
wlfc_MAC_table_update(struct wl_info *wl, uint8* ea,
	uint8 add_del, uint8 mac_handle, uint8 ifidx)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_MACDESC];

	results[0] = add_del;
	results[1] = WLFC_CTL_VALUE_LEN_MACDESC;
	results[2] = mac_handle;
	results[3] = ifidx;
	memcpy(&results[4], ea, ETHER_ADDR_LEN);
	return wlfc_push_signal_data(wl, results, sizeof(results), FALSE);
}

wlfc_info_state_t*
wlfc_state_get(struct wl_info *wl)
{
	if (wl != NULL)
		return wl->wlfc_info;
	return NULL;
}

int
wlfc_psmode_request(struct wl_info *wl, uint8 mac_handle, uint8 count,
	uint8 ac_bitmap, uint8 request_type)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_REQUEST_CREDIT];
	int ret;

	results[0] = request_type;
	if (request_type == WLFC_CTL_TYPE_MAC_REQUEST_PACKET)
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_PACKET;
	else
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_CREDIT;
	results[2] = count;
	results[3] = mac_handle;
	results[4] = ac_bitmap;
	ret = wlfc_push_signal_data(wl, results, sizeof(results), FALSE);

	if (ret == BCME_OK)
		ret = wlfc_sendup_ctl_info_now(wl);

	return ret;
}

int
wlfc_sendup_ctl_info_now(struct wl_info *wl)
{
	/* XXX:
	typical overhead BCMDONGLEOVERHEAD,
	but aggregated sd packets can take 2*BCMDONGLEOVERHEAD
	octets
	*/
	int header_overhead = BCMDONGLEOVERHEAD*3;
	struct lbuf *wlfc_pkt;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);

	/* Two possible reasons for being here - pending data or pending credit */
	if (!(METADATA_TO_HOST_ENAB(wl->pub) && wl->wlfc_info->pending_datalen) &&
		!(CREDIT_INFO_UPDATE_ENAB(wl->pub) && wl->wlfc_info->fifo_credit_back_pending))
		return BCME_OK;

	/* reverve space for RPC header and pad bytes to avoid assert failure */
	if ((wlfc_pkt = PKTGET(wl->pub->osh, (wl->wlfc_info->pending_datalen +
#if (defined(BCM_FD_AGGR) || defined(WLC_LOW_ONLY))
		BCM_RPC_TP_ENCAP_LEN + BCM_RPC_TP_MAXHEADPAD_LEN + BCM_RPC_TP_MAXTAILPAD_LEN +
#endif /* (defined(BCM_FD_AGGR) || defined(WLC_LOW_ONLY)) */
		header_overhead), TRUE)) == NULL) {
		/* what can be done to deal with this?? */
		/* set flag and try later again? */
		WL_ERROR(("PKTGET pkt size %d failed\n", wl->wlfc_info->pending_datalen));
		return BCME_NOMEM;
	}
	PKTPULL(wl->pub->osh, wlfc_pkt, wl->wlfc_info->pending_datalen +
#if (defined(BCM_FD_AGGR) || defined(WLC_LOW_ONLY))
		BCM_RPC_TP_ENCAP_LEN + BCM_RPC_TP_MAXHEADPAD_LEN +
#endif /* (defined(BCM_FD_AGGR) || defined(WLC_LOW_ONLY)) */
		header_overhead);
	PKTSETLEN(wl->pub->osh, wlfc_pkt, 0);
	PKTSETTYPEEVENT(wl->pub->osh, wlfc_pkt);
	wl_sendup(wl, NULL, wlfc_pkt, 1);
	if (wl->wlfc_info->pending_datalen) {
		/* not sent by wl_sendup() due to memory issue? */
		WL_ERROR(("wl_sendup failed to send pending_datalen\n"));
		return BCME_NOMEM;
	}

#ifdef PROP_TXSTATUS_DEBUG
	wl->wlfc_info->dbgstats->nullpktallocated++;
#endif // endif
	return BCME_OK;
}

int
wlfc_push_credit_data(struct wl_info *wl, void* p)
{
	uint8 ac;
	uint32 threshold;

	ac = WL_TXSTATUS_GET_FIFO(WLPKTTAG(p)->wl_hdr_information);
	WLPKTTAG(p)->flags |= WLF_CREDITED;

#ifdef PROP_TXSTATUS_DEBUG
	wl->wlfc_info->dbgstats->creditupdates++;
	wl->wlfc_info->dbgstats->credits[ac]++;
#endif // endif
	wl->wlfc_info->fifo_credit_back[ac]++;
	wl->wlfc_info->fifo_credit_back_pending = 1;
	threshold = wl->wlfc_info->fifo_credit_threshold[ac];

#ifdef WLFCTS
	if (WLFCTS_ENAB(wl->pub)) {
		/* 8-bit credit sequence summed across all ACs to identify credit leaks */
		wl->wlfc_info->creditseqtohost++;

		/* XXX Overload ATIM credit for now since this is unused : revisit
		 * PROP_TXSTATUS_TIMESTAMP - a feature enabled by -wlfcts.
		 */
		wl->wlfc_info->fifo_credit_back[WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK - 1] =
			wl->wlfc_info->creditseqtohost;
	}
#endif /* WLFCTS */

	if (wl->wlfc_info->wlfc_trigger & WLFC_CREDIT_TRIGGER) {

		if (wl->wlfc_info->fifo_credit_in[ac] > wl->wlfc_info->fifo_credit[ac]) {
			/* borrow happened */
			threshold = wl->wlfc_info->total_credit /
				wl->wlfc_info->wlfc_fifo_bo_cr_ratio;
		}

		/*
		monitor how much credit is being gathered here. If credit pending is
		larger than a preset threshold, send_it_now(). The idea is to keep
		the host busy pushing packets to keep the pipeline filled.
		*/
		if (wl->wlfc_info->fifo_credit_back[ac] >= threshold) {
			if ((wlfc_sendup_ctl_info_now(wl) != BCME_OK) &&
				(!wl->wlfc_info->timer_started)) {
				wl_add_timer(wl, wl->wlfc_info->fctimer,
					WLFC_SENDUP_TIMER_INTERVAL, 0);
				wl->wlfc_info->timer_started = 1;
			}
		}
	}

	return BCME_OK;
}

int
wlfc_queue_avail(struct wl_info *wl)
{
	ASSERT(wl->wlfc_info->pending_datalen <= WLFC_MAX_PENDING_DATALEN);

	return (WLFC_MAX_PENDING_DATALEN - wl->wlfc_info->pending_datalen);
}

int
wlfc_queue_signal_data(struct wl_info *wl, void* data, uint8 len)
{
	uint8 type = ((uint8*)data)[0];
	bool skip_cp = FALSE;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);
	ASSERT((wl->wlfc_info->pending_datalen + len) <= WLFC_MAX_PENDING_DATALEN);

	if ((wl->wlfc_info->pending_datalen + len) > WLFC_MAX_PENDING_DATALEN) {
		WL_ERROR(("wlfc queue full: %d > %d\n",
			wl->wlfc_info->pending_datalen + len,
			WLFC_MAX_PENDING_DATALEN));
		return BCME_ERROR;
	}

	if ((type == WLFC_CTL_TYPE_TXSTATUS) && ((wlc_info_t *)wl->wlc)->comp_stat_enab) {
		uint32 xstatusdata, statusdata = *((uint32 *)((uint8 *)data + TLV_HDR_LEN));
		uint8 xcnt, cnt = WL_TXSTATUS_GET_FREERUNCTR(statusdata);
		uint8 xac, ac = WL_TXSTATUS_GET_FIFO(statusdata);
		uint16 xhslot, hslot = WL_TXSTATUS_GET_HSLOT(statusdata);
		uint8 xstatus, status = WL_TXSTATUS_GET_STATUS(statusdata);
		uint8 cur_pos = 0;
		uint8 bBatched = 0;
		uint32 compcnt_offset = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS;

		uint16 xseq = 0, seq = 0;
		uint8 xseq_fromfw = 0, seq_fromfw = 0;
		uint16 xseq_num = 0, seq_num = 0;

		((uint8 *)data)[TLV_TAG_OFF] = WLFC_CTL_TYPE_COMP_TXSTATUS;
		((uint8 *)data)[TLV_LEN_OFF]++;

		if (WLFC_GET_REUSESEQ(wl->wlfc_mode)) {
			compcnt_offset += WLFC_CTL_VALUE_LEN_SEQ;
			seq = *((uint16 *)((uint8 *)data + TLV_HDR_LEN +
				WLFC_CTL_VALUE_LEN_TXSTATUS));
			seq_fromfw = WL_SEQ_GET_FROMFW(seq);
			seq_num = WL_SEQ_GET_NUM(seq);
		}

		while (cur_pos < wl->wlfc_info->pending_datalen) {
			if ((wl->wlfc_info->data[cur_pos] == WLFC_CTL_TYPE_COMP_TXSTATUS)) {
				xstatusdata = *((uint32 *)(wl->wlfc_info->data + cur_pos +
					TLV_HDR_LEN));
				/* next expected free run counter */
				xcnt = (WL_TXSTATUS_GET_FREERUNCTR(xstatusdata) +
					wl->wlfc_info->data[cur_pos + compcnt_offset]) &
					WL_TXSTATUS_FREERUNCTR_MASK;
				/* next expected fifo number */
				xac = WL_TXSTATUS_GET_FIFO(statusdata);
				/* next expected slot number */
				xhslot = WL_TXSTATUS_GET_HSLOT(xstatusdata);
				if (!WLFC_GET_AFQ(wl->wlfc_mode)) {
					/* for hanger, it needs to be consective */
					xhslot = (xhslot + wl->wlfc_info->data[cur_pos +
						compcnt_offset]) & WL_TXSTATUS_HSLOT_MASK;
				}
				xstatus = WL_TXSTATUS_GET_STATUS(xstatusdata);

				if (WLFC_GET_REUSESEQ(wl->wlfc_mode)) {
					xseq = *((uint16 *)(wl->wlfc_info->data + cur_pos +
						TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS));
					xseq_fromfw = WL_SEQ_GET_FROMFW(xseq);
					/* next expected seq num */
					xseq_num = (WL_SEQ_GET_NUM(xseq) + wl->wlfc_info->data[
						cur_pos + compcnt_offset]) & WL_SEQ_NUM_MASK;
				}

				if ((cnt == xcnt) && (hslot == xhslot) &&
					(status == xstatus) && (ac == xac)) {
					if (!WLFC_GET_REUSESEQ(wl->wlfc_mode) ||
						((seq_fromfw == xseq_fromfw) &&
						(!seq_fromfw || (seq_num == xseq_num)))) {
						wl->wlfc_info->data[cur_pos + compcnt_offset]++;
						bBatched = 1;
						wl->wlfc_info->compressed_stat_cnt++;
						break;
					}
				}
			}
			cur_pos += wl->wlfc_info->data[cur_pos + TLV_LEN_OFF] + TLV_HDR_LEN;
		}

		if (!bBatched) {
			memcpy(&wl->wlfc_info->data[wl->wlfc_info->pending_datalen], data, len);
			wl->wlfc_info->data[wl->wlfc_info->pending_datalen + len] = 1;
			wl->wlfc_info->pending_datalen += len + 1;
		}

		skip_cp = TRUE;
	}

	if (!skip_cp) {
		memcpy(&wl->wlfc_info->data[wl->wlfc_info->pending_datalen], data, len);
		wl->wlfc_info->pending_datalen += len;
	}

	return BCME_OK;
}

int
wlfc_push_signal_data(struct wl_info *wl, void* data, uint8 len, bool hold)
{
	int rc = BCME_OK;
	uint8 type = ((uint8*)data)[0];
	uint8 tlv_flag;
	uint32 tlv_mask;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);

	tlv_flag = ((wlc_info_t *)(wl->wlc))->wlfc_flags;

	tlv_mask = (((tlv_flag & WLFC_FLAGS_XONXOFF_SIGNALS) ? 1 : 0) ?
		WLFC_FLAGS_XONXOFF_MASK : 0) |
#ifdef WLFCTS
		(((WLFCTS_ENAB(wl->pub) && (tlv_flag & WLFC_FLAGS_PKT_STAMP_SIGNALS)) ? 1 : 0) ?
		WLFC_FLAGS_PKT_STAMP_MASK : 0) |
#endif /* WLFCTS */
#if defined(CREDIT_INFO_UPDATE)
		(((CREDIT_INFO_UPDATE_ENAB(wl->pub) &&
		(tlv_flag & WLFC_FLAGS_CREDIT_STATUS_SIGNALS)) ? 1 : 0) ?
		WLFC_FLAGS_CREDIT_STATUS_MASK : 0) |
#endif /* CREDIT_INFO_UPDATE */
		0;

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB(((wlc_info_t*)(wl->wlc))->pub)) {
		rc = wlfc_push_signal_bus_data(wl, data, len);
	}
	else
#endif /* BCMPCIEDEV */
	{
		/* if the host does not want these TLV signals, drop it */
		if (!(tlv_mask & (1 << type))) {
			WLFC_DBGMESG(("%s() Dropping signal, type:%d, mask:%08x, flag:%d\n",
				__FUNCTION__, type, tlv_mask, tlv_flag));
			return BCME_OK;
		}

		if ((wl->wlfc_info->pending_datalen + len) > WLFC_MAX_PENDING_DATALEN) {
			if (BCME_OK != (rc = wlfc_sendup_ctl_info_now(wl))) {
				/* at least the caller knows we have failed */
				WL_ERROR(("%s() Dropping %d bytes data\n", __FUNCTION__, len));
				return rc;
			}
		}

		wlfc_queue_signal_data(wl, data, len);
		if (!(((wlc_info_t *)wl->wlc)->comp_stat_enab))
			hold = FALSE;

		rc = wlfc_send_signal_data(wl, hold);
	}
	return rc;
}

int
wlfc_send_signal_data(struct wl_info *wl, bool hold)
{
	int rc = BCME_OK;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);
	if ((wl->wlfc_info->pending_datalen > wl->wlfc_info->pending_datathresh) ||
		(!hold && (wl->wlfc_info->wlfc_trigger & WLFC_TXSTATUS_TRIGGER) &&
		(wl->wlfc_info->compressed_stat_cnt > wl->wlfc_info->wlfc_comp_txstatus_thresh))) {
		rc = wlfc_sendup_ctl_info_now(wl);
		if (rc == BCME_OK)
			return BCME_OK;
	}

	if (!wl->wlfc_info->timer_started) {
		wl_add_timer(wl, wl->wlfc_info->fctimer,
			WLFC_SENDUP_TIMER_INTERVAL, 0);
		wl->wlfc_info->timer_started = 1;
	}
	return rc;
}

static int
wl_sendup_txstatus(wl_info_t *wl, void **pp)
{
	wlfc_info_state_t* wlfc = (wlfc_info_state_t*)wl->wlfc_info;
	uint8* wlfchp;
	uint8 required_headroom;
	uint8 wl_hdr_words = 0;
	uint8 fillers = 0;
	uint8 rssi_space = 0, tstamp_space = 0;
	uint8 seqnumber_space = 0;
	uint8 fcr_tlv_space = 0;
	uint8 ampdu_reorder_info_space = 0;
	void *p = *pp;
	uint32 datalen, datalen_min;

	wl->wlfc_info->compressed_stat_cnt = 0;

	/* For DATA packets: plugin a RSSI value that belongs to this packet.
	   RSSI TLV = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_RSSI
	 */
	if (!PKTTYPEEVENT(wl->pub->osh, p)) {
#if defined(METADATA_TO_HOST) || defined(WLFCTS)
		/* is the RSSI TLV reporting enabled? */
		if ((METADATA_TO_HOST_ENAB(wl->pub) || WLFCTS_ENAB(wl->pub)) &&
			(((wlc_info_t *)(wl->wlc))->wlfc_flags & WLFC_FLAGS_RSSI_SIGNALS)) {
			rssi_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_RSSI;
#ifdef WLFCTS
			if (WLFCTS_ENAB(wl->pub))
				rssi_space += 3;	/* will include SNR and sequence */
#endif /* WLFCTS */
		}
#endif /* METADATA_TO_HOST || WLFCTS */
#ifdef WLFCTS
		/* Send a timestamp to host only if enabled */
		if (WLFCTS_ENAB(wl->pub) &&
			(((wlc_info_t *)(wl->wlc))->wlfc_flags &
			WLFC_FLAGS_PKT_STAMP_SIGNALS)) {
			tstamp_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TIMESTAMP;
		}
#endif /* WLFCTS */
#ifdef WLAMPDU_HOSTREORDER
		/* check if the host reordering info needs to be added from pkttag */
		{
			wlc_pkttag_t *pkttag;
			pkttag = WLPKTTAG(p);
			if (pkttag->flags2 & WLF2_HOSTREORDERAMPDU_INFO) {
				ampdu_reorder_info_space = WLHOST_REORDERDATA_LEN + TLV_HDR_LEN;
			}
		}
#endif /* WLAMPDU_HOSTREORDER */
	 }

#ifdef WLFCHOST_TRANSACTION_ID
	seqnumber_space = TLV_HDR_LEN + WLFC_TYPE_TRANS_ID_LEN;
#endif // endif
#ifdef CREDIT_INFO_UPDATE
	if (CREDIT_INFO_UPDATE_ENAB(wl->pub)) {
		if (wlfc->fifo_credit_back_pending) {
			fcr_tlv_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
		}
	}
#endif /* CREDIT_INFO_UPDATE */
	datalen_min = rssi_space + tstamp_space
				+ ampdu_reorder_info_space + seqnumber_space;
	datalen = wlfc->pending_datalen + fcr_tlv_space + datalen_min;
	fillers = ROUNDUP(datalen, 4) - datalen;
	required_headroom = datalen + fillers;
	wl_hdr_words = required_headroom >> 2;

	if (PKTHEADROOM(wl->pub->osh, p) < required_headroom) {
		void *p1;
		int plen = PKTLEN(wl->pub->osh, p);
		/* Allocate a packet that will fit all the data */
		if ((p1 = PKTGET(wl->pub->osh, (plen + required_headroom), TRUE)) == NULL) {
			WL_ERROR(("PKTGET pkt size %d failed\n", plen));
			/* There should still be enough room for datalen_min */
			datalen = datalen_min;
			fillers = ROUNDUP(datalen, 4) - datalen;
			required_headroom = datalen + fillers;
			ASSERT(PKTHEADROOM(wl->pub->osh, p) >= required_headroom);
			if (PKTHEADROOM(wl->pub->osh, p) < required_headroom) {
				return FALSE;
			}

			wl_hdr_words = required_headroom >> 2;
			PKTPUSH(wl->pub->osh, p, required_headroom);
		} else {
			/* Transfer other fields */
			PKTSETPRIO(p1, PKTPRIO(p));
			PKTSETSUMGOOD(p1, PKTSUMGOOD(p));
			bcopy(PKTDATA(wl->pub->osh, p),
				(PKTDATA(wl->pub->osh, p1) + required_headroom), plen);
			wlc_pkttag_info_move(wl->pub, p, p1);
			PKTFREE(wl->pub->osh, p, TRUE);
			p = p1;
			*pp = p1;
#ifdef PROP_TXSTATUS_DEBUG
			wlfc->dbgstats->realloc_in_sendup++;
#endif // endif
		}
	} else
		PKTPUSH(wl->pub->osh, p, required_headroom);

	wlfchp = PKTDATA(wl->pub->osh, p);

#ifdef WLFCHOST_TRANSACTION_ID
	if (seqnumber_space) {
		uint32 timestamp;

		/* byte 0: ver, byte 1: seqnumber, byte2:byte6 timestamps */
		wlfchp[0] = WLFC_CTL_TYPE_TRANS_ID;
		wlfchp[1] = WLFC_TYPE_TRANS_ID_LEN;
		wlfchp += TLV_HDR_LEN;

		wlfchp[0] = 0;
		wlfchp[1] = wlfc->txseqtohost++;

		/* time stamp of the packet */
		timestamp = hndrte_reftime_ms();
		bcopy(&timestamp, &wlfchp[2], sizeof(uint32));

		wlfchp += WLFC_TYPE_TRANS_ID_LEN;
	}
#endif /* WLFCHOST_TRANSACTION_ID */

#ifdef WLAMPDU_HOSTREORDER
	if (ampdu_reorder_info_space) {

		wlc_pkttag_t *pkttag = WLPKTTAG(p);
		PKTSETNODROP(wl->pub->osh, p);

		wlfchp[0] = WLFC_CTL_TYPE_HOST_REORDER_RXPKTS;
		wlfchp[1] = WLHOST_REORDERDATA_LEN;
		wlfchp += TLV_HDR_LEN;

		/* zero out the tag value */
		bzero(wlfchp, WLHOST_REORDERDATA_LEN);

		wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET] =
			pkttag->u.ampdu_info_to_host.ampdu_flow_id;
		wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET] =
			AMPDU_BA_MAX_WSIZE -  1;		/* 0 based...so -1 */
		wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET] =
			pkttag->u.ampdu_info_to_host.flags;
		wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET] =
			pkttag->u.ampdu_info_to_host.cur_idx;
		wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET] =
			pkttag->u.ampdu_info_to_host.exp_idx;

		WL_INFORM(("flow:%d idx(%d, %d, %d), flags 0x%02x\n",
			wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET],
			wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET]));
		wlfchp += WLHOST_REORDERDATA_LEN;
	}
#endif /* WLAMPDU_HOSTREORDER */

#ifdef WLFCTS
	if (WLFCTS_ENAB(wl->pub) && tstamp_space) {
		wlc_info_t *wlc = (wlc_info_t *)wl->wlc;
		uint32 tsf_l = 0;
		if (si_iscoreup(wlc->pub->sih))
			tsf_l = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);

		wlfchp[0] = WLFC_CTL_TYPE_RX_STAMP;
		wlfchp[1] = WLFC_CTL_VALUE_LEN_TIMESTAMP;

		/* convert RX rate, and keep RX retried flag */
		memcpy(&wlfchp[2], &(WLPKTTAG(p)->rspec), 4);
		memcpy(&wlfchp[6], &tsf_l, 4);
		memcpy(&wlfchp[10], &(((wlc_pkttag_t*)WLPKTTAG(p))->shared.rx_tstamp), 4);
		wlfchp += TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TIMESTAMP;
	}
#endif /* WLFCTS */

#if defined(METADATA_TO_HOST) || defined(WLFCTS)
	if ((METADATA_TO_HOST_ENAB(wl->pub) || WLFCTS_ENAB(wl->pub)) && rssi_space) {
		wlfchp[0] = WLFC_CTL_TYPE_RSSI;
		wlfchp[1] = rssi_space - TLV_HDR_LEN;
		wlfchp[2] = ((wlc_pkttag_t*)WLPKTTAG(p))->pktinfo.misc.rssi;
#ifdef WLFCTS
		if (WLFCTS_ENAB(wl->pub)) {
			wlfchp[3] = ((wlc_pkttag_t*)WLPKTTAG(p))->pktinfo.misc.snr;
			memcpy(&wlfchp[4], &(((wlc_pkttag_t*)WLPKTTAG(p))->seq), 2);
		}
#endif /* WLFCTS */
		wlfchp += rssi_space;
	}
#endif /* METADATA_TO_HOST || WLFCTS */

	if (datalen > datalen_min) {
		/* this packet is carrying signals */
		PKTSETNODROP(wl->pub->osh, p);

		if (wlfc->pending_datalen) {
			memcpy(&wlfchp[0], wlfc->data, wlfc->pending_datalen);
			wlfchp += wlfc->pending_datalen;
			wlfc->pending_datalen = 0;
		}
#ifdef CREDIT_INFO_UPDATE
		if (CREDIT_INFO_UPDATE_ENAB(wl->pub)) {
		/* if there're any fifo credit pending, append it (after pending data) */
			if (fcr_tlv_space) {
				int i = 0;
				wlfchp[0] = WLFC_CTL_TYPE_FIFO_CREDITBACK;
				wlfchp[1] = WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
				memcpy(&wlfchp[2], wlfc->fifo_credit_back,
					WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK);

				for (i = 0; i < WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK; i++) {
					if (wlfc->fifo_credit_back[i]) {
						wlfc->fifo_credit_in[i] -=
							wlfc->fifo_credit_back[i];
						wlfc->fifo_credit_back[i] = 0;
					}
				}
				wlfc->fifo_credit_back_pending = 0;
				wlfchp += TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
			}
		}
#endif /* CREDIT_INFO_UPDATE */
	}

	if (fillers) {
		memset(&wlfchp[0], WLFC_CTL_TYPE_FILLER, fillers);
	}

	PKTSETDATAOFFSET(p, wl_hdr_words);

	if (wlfc->timer_started) {
		/* cancel timer */
		wl_del_timer(wl, wlfc->fctimer);
		wlfc->timer_started = 0;
	}
	return FALSE;
}

uint32 wlfc_query_mode(struct wl_info *wl)
{
	return (uint32)(wl->wlfc_mode);
}

#endif /* PROP_TXSTATUS */

#ifndef BCMPCIEDEV_ENABLED
static void *
wl_pkt_header_push(wl_info_t *wl, void *p, uint8 *wl_hdr_words)
{
	wl_header_t *h;
	osl_t *osh = wl->pub->osh;
	wlc_pkttag_t *pkttag = WLPKTTAG(p);
	int8 rssi = pkttag->pktinfo.misc.rssi;

	if (PKTHEADROOM(osh, p) < WL_HEADER_LEN) {
		void *p1;
		int plen = PKTLEN(osh, p);

		/* Alloc a packet that will fit all the data; chaining the header won't work */
		if ((p1 = PKTGET(osh, plen + WL_HEADER_LEN, TRUE)) == NULL) {
			WL_ERROR(("PKTGET pkt size %d failed\n", plen));
			PKTFREE(osh, p, TRUE);
			return NULL;
		}

		/* Transfer other fields */
		PKTSETPRIO(p1, PKTPRIO(p));
		PKTSETSUMGOOD(p1, PKTSUMGOOD(p));

		bcopy(PKTDATA(osh, p), PKTDATA(osh, p1) + WL_HEADER_LEN, plen);
		PKTFREE(osh, p, TRUE);

		p = p1;
	} else
		PKTPUSH(osh, p, WL_HEADER_LEN);

	h = (wl_header_t *)PKTDATA(osh, p);
	h->type = WL_HEADER_TYPE;
	h->version = WL_HEADER_VER;
	h->rssi = rssi;
	h->pad = 0;
	/* Return header length in words */
	*wl_hdr_words = WL_HEADER_LEN/4;
	return p;
}
#endif /* BCMPCIEDEV_ENABLED */

static void
wl_pkt_header_pull(wl_info_t *wl, void *p)
{
	/* Currently this is a placeholder function. We don't process wl header
	   on Tx side as no meaningful fields defined for tx currently.
	 */
#ifdef BCM_DHDHDR
	bzero(PKTFRAGFCTLV(wl->pub->osh, p), PKTDATAOFFSET(p) << 2);
#else
	PKTPULL(wl->pub->osh, p, PKTDATAOFFSET(p));
#endif /* BCM_DHDHDR */
	return;
}

/* Return the proper arpi pointer for either corr to an IF or
*	default. For IF case, Check if arpi is present. It is possible that, upon a
*	down->arpoe_en->up scenario, interfaces are not reallocated, and
*	so, wl->arpi could be NULL. If so, allocate it and use.
*/
static wl_arp_info_t *
wl_get_arpi(wl_info_t *wl, struct wl_if *wlif)
{
	ASSERT(ARPOE_SUPPORT(wl->pub));

	if (wlif != NULL) {
		if (wlif->arpi == NULL)
			wlif->arpi = wl_arp_alloc_ifarpi(wl->arpi, wlif->wlcif);
		/* note: this could be null if the above wl_arp_alloc_ifarpi fails */
		return wlif->arpi;
	} else
		return wl->arpi;
}

void *
wl_get_ifctx(wl_info_t *wl, int ctx_id, wl_if_t *wlif)
{
	if (ctx_id == IFCTX_ARPI)
		return (void *)wlif->arpi;

#ifdef WLNDOE
	if (ctx_id == IFCTX_NDI)
		return (void *)wlif->ndi;
#endif // endif
	return NULL;
}

#ifdef WLNDOE
/* Return the proper ndi pointer for either corr to an IF or
*	default. For IF case, Check if arpi is present. It is possible that, upon a
*	down->ndoe_en->up scenario, interfaces are not reallocated, and
*	so, wl->ndi could be NULL. If so, allocate it and use.
*/
static wl_nd_info_t *
wl_get_ndi(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif != NULL) {
		if (wlif->ndi == NULL)
			wlif->ndi = wl_nd_alloc_ifndi(wl->ndi, wlif->wlcif);
		/* note: this could be null if the above wl_arp_alloc_ifarpi fails */
		return wlif->ndi;
	} else
		return wl->ndi;
}
#endif /* WLNDOE */

#ifdef WLNFC
static wl_nfc_info_t *
wl_get_nfci(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif != NULL) {
		if (wlif->nfci == NULL) {
			wlif->nfci = wl_nfc_alloc_ifnfci(wl->nfci, wlif->wlcif);
		}
		return wlif->nfci;
	}
	else {
		return wl->nfci;
	}
}
#endif /* WLNFC */

/*
 * The last parameter was added for the build. Caller of
 * this function should pass 1 for now.
 */
void
wl_sendup(wl_info_t *wl, struct wl_if *wlif, void *p, int numpkt)
{
	struct lbuf *lb;
	hndrte_dev_t *dev;
	hndrte_dev_t *chained;
	int ret_val;
	int no_filter;
	uint8 *buf;
	uint16 ether_type;
	bool brcm_specialpkt;
#ifdef TCPKAOE
	wl_tcp_keep_info_t *tcp_keep_info = wl_get_tcpkeepi(wl, wlif);
#endif // endif

	WL_TRACE(("wl%d: wl_sendup: %d bytes\n", wl->unit, PKTLEN(NULL, p)));

	no_filter = 0;
	if (wlif == NULL)
		dev = wl->dev;
	else
		dev = wlif->dev;
	chained = dev->chained;

#ifdef	WL_FRWD_REORDER
	if (FRWD_REORDER_ENAB(((wlc_info_t *)wl->wlc)->pub) &&
		AMPDU_HOST_REORDER_ENAB((wlc_info_t *)wl->wlc)) {
		if ((p = wlc_ampdu_frwd_handle_host_reorder(((wlc_info_t *)(wl->wlc))->ampdu_rx,
			p, FALSE)) == NULL)
			return;
	}
#endif /* WLC_FRWD_PKT_REORDER */

	buf = PKTDATA(wl->pub->osh, p);
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub))
		brcm_specialpkt = !!PKTTYPEEVENT(wl->pub->osh, p);
	else
#endif // endif
	brcm_specialpkt = ntoh16_ua(buf + ETHER_TYPE_OFFSET) == ETHER_TYPE_BRCM;

	if (!brcm_specialpkt) {
#ifdef TOE
		_wl_toe_recv_proc(wl, p);
#endif // endif

#ifdef EXT_STA
		if (WLEXTSTA_ENAB(wl->pub))
			ether_type = ntoh16_ua(buf + DOT11_MAC_HDR_LEN + SNAP_HDR_LEN);
		else
#endif // endif
		ether_type = ntoh16_ua(buf + ETHER_TYPE_OFFSET);

		if (ether_type == ETHER_TYPE_8021Q)
			ether_type = ntoh16_ua((const void *)(buf + VLAN_TAG_LEN));

#ifdef NWOE
		if (NWOE_ENAB(wl->pub))
		{
			ret_val = wl_nwoe_recv_proc(wl->nwoei, wl->pub->osh, p);
			if (ret_val == NWOE_PKT_CONSUMED)
				return;
		}
#endif /* NWOE */

		/* Apply ARP offload only for ETHER_TYPE_ARP */
		if (ARPOE_ENAB(wl->pub) &&
			(ether_type < ETHER_TYPE_MIN || ether_type == ETHER_TYPE_ARP)) {
			wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
			wlc_bsscfg_t *bsscfg = NULL;
			if (wlif && wlif->wlcif)
				bsscfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlif->wlcif);

			if (arpi) {
				ret_val = wl_arp_recv_proc(arpi, p);
				switch (ret_val) {
					case ARP_REQ_SINK:
						if (bsscfg && BSSCFG_AP(bsscfg) &&
							bsscfg->ap_isolate)
							/* for pcie, don't sink, pass it to host */
							break;
						/* fall through is intentional */
					case ARP_REPLY_PEER:
						PKTFREE(wl->pub->osh, p, FALSE);
						return;
					case ARP_FORCE_FORWARD:
						no_filter = 1;
						break;
				}
			}
		}
#ifdef WLNDOE
		/* Apply NS offload only for ETHER_TYPE_IPV6 */
		if (NDOE_ENAB(wl->pub) &&
			(ether_type < ETHER_TYPE_MIN || ether_type == ETHER_TYPE_IPV6)) {
			wl_nd_info_t *ndi = wl_get_ndi(wl, wlif);
			if (ndi) {
				ret_val = wl_nd_recv_proc(ndi, p);
				if ((ret_val == ND_REQ_SINK) || (ret_val == ND_REPLY_PEER)) {
					PKTFREE(wl->pub->osh, p, FALSE);
					return;
				}
				if (ret_val == ND_FORCE_FORWARD) {
					no_filter = 1;
				}
			}
		}
#endif /* WLNDOE */

#ifdef WLNFC
		/* Apply Secure WiFi thru NFC */
		if (NFC_ENAB(wl->pub)) {
			wl_nfc_info_t *nfci = wl_get_nfci(wl, wlif);
			if (nfci) {
				wl_nfc_recv_proc(nfci, p);
			}
		}
#endif // endif

#ifdef WL_TBOW
		if (TBOW_ENAB(wl->pub)) {
			if (!tbow_recv_wlandata(((wlc_info_t *)(wl->wlc))->tbow_info, p)) {
				/* tbow packet, don't send up */
				return;
			}
		}
#endif // endif
	}

#ifdef TCPKAOE
	if (tcp_keep_info) {
		ret_val = wl_tcpkeep_recv_proc(tcp_keep_info, p);
		if (ret_val >= 0) {
			PKTFREE(wl->pub->osh, p, FALSE);
			return;
		}
	}
#endif	/* TCPKAOE */

	if (chained) {

		/* Internally generated events have the special ether-type of
		 * ETHER_TYPE_BRCM; do not run these events through data packet filters.
		 */
		if (!brcm_specialpkt) {
			/* Apply packet filter */
			if ((chained->flags & RTEDEVFLAG_HOSTASLEEP) &&
			    wl->hwfflags && !wl_hwfilter(wl, p)) {
				PKTFREE(wl->pub->osh, p, FALSE);
				return;
			}

			/* Apply packet filtering. */
			if (!no_filter && PKT_FILTER_ENAB(wl->pub)) {
				if (!wlc_pkt_filter_recv_proc(wl->pkt_filter_info, p)) {
					/* Discard received packet. */
					PKTFREE(wl->pub->osh, p, FALSE);
					return;
				}
			}
#if defined(D0_COALESCING)
			/* Apply D0 packet filtering. */
			if (D0_FILTER_ENAB(wl->pub)) {
				if (!wlc_d0_filter_recv_proc(wl->d0_filter_info, p)) {
					return;
				}
			}
#endif /* D0_COALESCING */

		}

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wl->pub) && !brcm_specialpkt) {
		wl_rx_ctxt_push(wl, p);
	}
#endif /* EXT_STA */

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
#ifdef BCMPCIEDEV_ENABLED
			 if (!PKTTYPEEVENT(wl->pub->osh, p))
#endif // endif
			if (METADATA_TO_HOST_ENAB(wl->pub) && wl_sendup_txstatus(wl, &p)) {
				return;
			}
		} else
#endif /* PROP_TXSTATUS */
		{
#ifndef BCMPCIEDEV_ENABLED
			uint8 wl_hdr_words = 0;
			if ((p = wl_pkt_header_push(wl, p, &wl_hdr_words)) == NULL) {
				return;
			}

			PKTSETDATAOFFSET(p, wl_hdr_words);
#endif // endif
		}
		lb = PKTTONATIVE(wl->pub->osh, p);
		if (chained->funcs->xmit(dev, chained, lb) != 0) {
			WL_ERROR(("%s: xmit failed; free pkt 0x%p\n", __FUNCTION__, lb));
			lb_free(lb);
		}
	} else {
		/* only AP mode can be non chained */
		ASSERT(AP_ENAB(wl->pub));
		PKTFREE(wl->pub->osh, p, FALSE);
	}
}
#endif /* WLC_HIGH */

#if defined(D0_COALESCING) || defined(WLAWDL) || defined(WL_MONITOR)
void
wl_sendup_no_filter(wl_info_t *wl, struct wl_if *wlif, void *p, int numpkt)
{
	struct lbuf *lb;
	hndrte_dev_t *dev;
	hndrte_dev_t *chained;

	WL_TRACE(("wl%d: wl_sendup: %d bytes\n", wl->unit, PKTLEN(NULL, p)));

	if (wlif == NULL)
		dev = wl->dev;
	else
		dev = wlif->dev;
	chained = dev->chained;

	if (chained) {
#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wl->pub)) {
		wl_rx_ctxt_push(wl, p);
	}
#endif /* EXT_STA */

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
			if (METADATA_TO_HOST_ENAB(wl->pub) && wl_sendup_txstatus(wl, &p)) {
				return;
			}
		}
#endif /* PROP_TXSTATUS */

		lb = PKTTONATIVE(wl->pub->osh, p);
		if (chained->funcs->xmit(dev, chained, lb) != 0) {
			WL_ERROR(("wl_sendup: xmit failed; free pkt 0x%p\n", lb));
			lb_free(lb);
		}
	} else {
		/* only AP mode can be non chained */
		ASSERT(AP_ENAB(wl->pub));
		PKTFREE(wl->pub->osh, p, FALSE);
	}
}
#endif /* defined(D0_COALESCING) || defined(WLAWDL) || defined(WL_MONITOR) */

/* buffer received from BUS driver(e.g USB, SDIO) in dongle framework
 *   For normal driver, push it to common driver sendpkt
 *   For BMAC driver, forward to RPC layer to process
 */
#ifdef WLC_HIGH

#ifdef TOE
static void
_wl_toe_send_proc(wl_info_t *wl, void *p)
{
	if (TOE_ENAB(wl->pub))
		wl_toe_send_proc(wl->toei, p);
}
#endif /* TOE */

#ifdef PROP_TXSTATUS
static int
wl_send_txstatus(wl_info_t *wl, void *p)
{
	uint8* wlhdrtodev;
	wlc_pkttag_t *wlpkttag;
	uint8 wlhdrlen;
	uint8 processed = 0;
	uint32 wl_hdr_information = 0;
	uint16 seq = 0;
#ifdef WLFCTS
	uint32 tx_entry_tstamp = 0;
#endif // endif

	ASSERT(wl != NULL);

	wlhdrlen = PKTDATAOFFSET(p) << 2;

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB(((wlc_info_t*)(wl->wlc))->pub)) {
		/* We do not expect host to set BDC and wl header on PCIEDEV path, So set it now */
		WL_TXSTATUS_SET_FLAGS(wl_hdr_information, WLFC_PKTFLAG_PKTFROMHOST);
	}
#endif /* BCMPCIEDEV */

	if (wlhdrlen != 0) {
#ifdef BCM_DHDHDR
		wlhdrtodev = PKTFRAGFCTLV(wl->pub->osh, p);
#else
		wlhdrtodev = (uint8*)PKTDATA(wl->pub->osh, p);
#endif /* BCM_DHDHDR */
		while (processed < wlhdrlen) {
			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_PKTTAG) {
				wl_hdr_information |=
					ltoh32_ua(&wlhdrtodev[processed + TLV_HDR_LEN]);

				if (WLFC_GET_REUSESEQ(wl->wlfc_mode))
				{
					uint16 reuseseq  = ltoh16_ua(&wlhdrtodev[processed +
						TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS]);
					if (WL_SEQ_GET_FROMDRV(reuseseq)) {
						seq = reuseseq;
					}
				}

#ifdef CREDIT_INFO_UPDATE
				if (CREDIT_INFO_UPDATE_ENAB(wl->pub)) {
				if (!(WL_TXSTATUS_GET_FLAGS(wl_hdr_information) &
					WLFC_PKTFLAG_PKT_REQUESTED)) {
					uint8 ac = WL_TXSTATUS_GET_FIFO(wl_hdr_information);
					wl->wlfc_info->fifo_credit_in[ac]++;
				}
				}
#endif /* CREDIT_INFO_UPDATE */
#ifdef WLFCTS
				if (WLFCTS_ENAB(wl->pub)) {
					/* Send a timestamp back to host only if enabled */
					if ((WL_TXSTATUS_GET_FLAGS(wl_hdr_information) &
						WLFC_PKTFLAG_PKTFROMHOST) &&
					    (((wlc_info_t *)(wl->wlc))->wlfc_flags &
						WLFC_FLAGS_PKT_STAMP_SIGNALS)) {
						wlc_process_wlfc_dbg_update((wlc_info_t *)(wl->wlc),
							WLFC_CTL_TYPE_TX_ENTRY_STAMP, p);
					}
				}
#endif /* WLFCTS */
			}
			else if (wlhdrtodev[processed] ==
				WLFC_CTL_TYPE_PENDING_TRAFFIC_BMP) {
				wlc_scb_update_available_traffic_info(wl->wlc,
					wlhdrtodev[processed+2], wlhdrtodev[processed+3]);
			}
			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_FILLER) {
				/* skip ahead - 1 */
				processed += 1;
			}
			else {
				/* skip ahead - type[1], len[1], value_len */
				processed += TLV_HDR_LEN + wlhdrtodev[processed + TLV_LEN_OFF];
			}
		}
#ifdef BCM_DHDHDR
		bzero(PKTFRAGFCTLV(wl->pub->osh, p), wlhdrlen);
#else
		PKTPULL(wl->pub->osh, p, wlhdrlen);
#endif /* BCM_DHDHDR */
		/* Reset DataOffset to 0, since we have consumed the wlhdr */
		PKTSETDATAOFFSET(p, 0);
	}
	else {
		if (BCMPCIEDEV_ENAB(((wlc_info_t*)(wl->wlc))->pub)) {
#ifdef WLFCTS
			if (WLFCTS_ENAB(wl->pub)) {
				/* Send a timestamp back to host only if enabled */
				wlc_info_t *wlc = (wlc_info_t *)wl->wlc;
				if (wlc->wlfc_flags & WLFC_FLAGS_PKT_STAMP_SIGNALS) {
					if (si_iscoreup(wlc->pub->sih)) {
						tx_entry_tstamp =
							R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
					}
				}
			}
#endif	/* WLFCTS */
		} else {
			WL_INFORM(("No pkttag from host.\n"));
		}
	}

	/* update pkttag */
	wlpkttag = WLPKTTAG(p);
	wlpkttag->wl_hdr_information = wl_hdr_information;
	wlpkttag->seq = seq;
#ifdef WLFCTS
	wlpkttag->shared.tx_entry_tstamp = tx_entry_tstamp;
#endif // endif
	if (wl->wlfc_info != NULL) {
		((wlfc_info_state_t*)wl->wlfc_info)->stats.packets_from_host++;
	}

	if (PKTLEN(wl->pub->osh, p) == 0) {
		/* a signal-only packet from host */
#ifdef PROP_TXSTATUS_DEBUG
		((wlfc_info_state_t*)wl->wlfc_info)->dbgstats->sig_from_host++;
#endif // endif
		PKTFREE(wl->pub->osh, p, TRUE);
		return TRUE;
	}
#ifdef PROP_TXSTATUS_DEBUG
	if ((WL_TXSTATUS_GET_FLAGS(wlpkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST) &&
	    (!(WL_TXSTATUS_GET_FLAGS(wlpkttag->wl_hdr_information) & WLFC_PKTFLAG_PKT_REQUESTED))) {
		((wlfc_info_state_t*)wl->wlfc_info)->dbgstats->creditin++;
	} else {
		((wlfc_info_state_t*)wl->wlfc_info)->dbgstats->nost_from_host++;
	}
	WLF2_PCB3_REG(p, WLF2_PCB3_WLFC);
#endif /* PROP_TXSTATUS_DEBUG */
	return FALSE;
}
#endif /* PROP_TXSTATUS */

#ifdef PKTC_TX_DONGLE
static bool
wlconfig_tx_chainable(wl_info_t *wl, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = (wlc_info_t *)wl->wlc;

	if ((WLEXTSTA_ENAB(wl->pub) && BSSCFG_SAFEMODE(bsscfg)) ||
	    (wlc->wet && BSSCFG_STA(bsscfg)) ||
	    wlc->mac_spoof ||
#ifdef WLWNM_AP
		(BSSCFG_AP(bsscfg) && WLWNM_ENAB(wlc->pub) &&
			!wlc_wnm_pkt_chainable(wlc->wnm_info, bsscfg)) ||
#endif /* WLWNM_AP */
	    CAC_ENAB(wl->pub) ||
	    BSSCFG_AWDL(wlc, bsscfg)) {
		return FALSE;
	}
	return TRUE;
}

static bool
wl_txframe_chainable(wl_info_t *wl, wlc_bsscfg_t *bsscfg, void *p, void *head)
{
	wlc_info_t *wlc = (wlc_info_t *)wl->wlc;
	bool chainable = FALSE;
	struct ether_header *eh, *head_eh;
	void *iph;

	eh = (struct ether_header *) PKTDATA(wlc->osh, p);
	iph = (void *)(eh + 1);

	if (BCMLFRAG_ENAB() && PKTISTXFRAG(wlc->osh, p)) {
		/* For LFRAG packets, we have only the ethernet header. IP header + Payload */
		/* is sitting in the host. So, don't bother to look into the IP Prot field */
		if ((ntoh16(eh->ether_type) == ETHER_TYPE_IP) ||
			(ntoh16(eh->ether_type) == ETHER_TYPE_IPV6))
			chainable = TRUE;
	} else if (ntoh16(eh->ether_type) == ETHER_TYPE_IP) {
		ASSERT(IP_VER(iph) == IP_VER_4);

		if (IPV4_PROT(iph) == IP_PROT_TCP ||
		    IPV4_PROT(iph) == IP_PROT_UDP) {
			chainable = TRUE;
		}
	} else if (ntoh16(eh->ether_type) == ETHER_TYPE_IPV6) {
		ASSERT(IP_VER(iph) == IP_VER_6);

		if (IPV6_PROT(iph) == IP_PROT_TCP ||
		    IPV6_PROT(iph) == IP_PROT_UDP) {
			chainable = TRUE;
		}
	}

	if (!chainable)
		goto exit;

	chainable = !ETHER_ISNULLDEST(eh->ether_dhost) &&
			!ETHER_ISMULTI(eh->ether_dhost);

	/* For PCIe Dev,
	 * In AP mode, all the packets in chain would have the same DA and PRIO
	 * In STA mode, all the packets in chain have same PRIO and would be transmitted to AP
	 * We don't need DA comparison.
	 */
	if ((head != NULL) && chainable) {
		if (!BCMPCIEDEV_ENAB(((wlc_info_t*)(wl->wlc))->pub)) {
			head_eh = (struct ether_header *) PKTDATA(wlc->osh, head);
			chainable = !eacmp(eh->ether_dhost, head_eh->ether_dhost) &&
				(PKTPRIO(p) == PKTPRIO(head));
		} else {
			/* In DHD, different PRIO pkts might be mapped to
			 * same flowring, different PRIO pkts should not be chained
			 */
			chainable = (PKTPRIO(p) == PKTPRIO(head));
		}
	}

exit:
	return chainable;
}
#endif /* PKTC_TX_DONGLE */

static void
wl_tx_pktfetch(wl_info_t *wl, struct lbuf *lb, hndrte_dev_t *src, hndrte_dev_t *dev)
{
	struct pktfetch_info *pinfo = NULL;
	struct pktfetch_generic_ctx *pctx = NULL;

	pinfo = MALLOC(wl->pub->osh, sizeof(struct pktfetch_info));
	if (!pinfo) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch pinfo!\n", __FUNCTION__));
		goto error;
	}

	pctx = MALLOC(wl->pub->osh, sizeof(struct pktfetch_generic_ctx) + 4*sizeof(uint32));
	if (!pctx) {
		WL_ERROR(("%s: Out of mem: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		goto error;
	}

	/* Fill up context */
	pctx->ctx_count = 4;
	pctx->ctx[0] = (uint32)wl;
	pctx->ctx[1] = (uint32)src;
	pctx->ctx[2] = (uint32)dev;
	pctx->ctx[3] = (uint32)pinfo;

	/* Fill up pktfetch info */
#ifdef BCM_DHDHDR
	pinfo->host_offset = (-DOT11_LLC_SNAP_HDR_LEN);
#else
	pinfo->host_offset = 0;
#endif /* BCM_DHDHDR */

	pinfo->headroom = TXOFF + DOT11_IV_MAX_LEN;
	pinfo->lfrag = (void*)lb;
	pinfo->cb = wl_send_cb;
	pinfo->ctx = (void*)pctx;
	pinfo->next = NULL;
	pinfo->osh = wl->pub->osh;
#ifdef BCMPCIEDEV_ENABLED
	if (hndrte_pktfetch(pinfo) != BCME_OK) {
		WL_ERROR(("%s: pktfetch request rejected\n", __FUNCTION__));
		goto error;
	}
#endif // endif
	return;

error:
	if (pinfo)
		MFREE(wl->pub->osh, pinfo, sizeof(struct pktfetch_info));

	if (pctx)
		MFREE(wl->pub->osh, pctx, sizeof(struct pktfetch_generic_ctx) + 4*sizeof(uint32));

	if (lb)
		PKTFREE(wl->pub->osh, lb, TRUE);
}

static void
wl_send_cb(void *lbuf, void *orig_lfrag, void *ctx, bool cancelled)
{
	wl_info_t *wl;
	struct pktfetch_info *pinfo;
	hndrte_dev_t *src, *dev;
	struct pktfetch_generic_ctx *pctx = (struct pktfetch_generic_ctx *)ctx;
	uint16 flush_pend_buf = 0;

#ifdef BCM_DHDHDR
	ASSERT(orig_lfrag == lbuf);
#endif // endif

	/* Retrieve contexts */
	wl = (wl_info_t *)pctx->ctx[0];
	src = (hndrte_dev_t *)pctx->ctx[1];
	dev = (hndrte_dev_t *)pctx->ctx[2];
	pinfo = (struct pktfetch_info *)pctx->ctx[3];

#ifndef BCM_DHDHDR
	PKTSETNEXT(wl->pub->osh, orig_lfrag, lbuf);
#endif // endif
	PKTSETFRAGTOTLEN(wl->pub->osh, orig_lfrag, 0);
	PKTSETFRAGLEN(wl->pub->osh, orig_lfrag, 1, 0);

	/* When BCM_DHDHDR is enabled, all tx packets that need to be fetched will
	 * include llc snap 8B header at start of lbuf.
	 * So we can do PKTSETFRAGTOTNUM here as well.
	 */
	PKTSETFRAGTOTNUM(wl->pub->osh, orig_lfrag, 0);

	/* Free the original pktfetch_info and generic ctx  */
	MFREE(wl->pub->osh, pinfo, sizeof(struct pktfetch_info));
	MFREE(wl->pub->osh, pctx, sizeof(struct pktfetch_generic_ctx)
		+ pctx->ctx_count*sizeof(uint32));

	/* The hnd_pktfetch_dispatch may get lbuf from PKTALLOC and the pktalloced counter
	 * will be increased by 1, later in the wl_send the PKTFROMNATIVE will increase 1 again
	 * for !lb_pool lbuf. (dobule increment)
	 * Here do PKTTONATIVE to decrease it before wl_send.
	 */
	if (!PKTPOOL(wl->pub->osh, lbuf)) {
		PKTTONATIVE(wl->pub->osh, lbuf);
	}

	flush_pend_buf = PKTFRAGFLOWRINGID(wl->pub->osh, orig_lfrag);
	if (wl_busioctl(wl, BUS_FLOW_FLUSH_PEND, (void *)&flush_pend_buf, sizeof(flush_pend_buf),
		NULL, NULL, 0) == BCME_OK) {
		if (flush_pend_buf) {
			PKTFREE(wl->pub->osh, orig_lfrag, TRUE);
			return;
		}
	}
#ifdef DONGLEBUILD
	/* Interface (dev) might be deleted between fetch request and complete,
	 * Validate it before forwarding packet.
	 */
	if (bus_ops->validatedev((void *)wl->dev->chained, dev) == BCME_OK) {
		wl_send(src, dev, orig_lfrag);
	} else {
		/* Interface doesn't exist, discard packet */
		void *p;

		p = PKTFRMNATIVE(wl->pub->osh, orig_lfrag);
		PKTFREE(wl->pub->osh, p, TRUE);
	}
#else
	wl_send(src, dev, orig_lfrag);
#endif /* DONGLEBUILD */
}

/* Per packet key check for SW TKIP MIC requirement, code largely borrowed from wlc_sendpkt */
static bool
wl_sw_encrypt_mic(wl_info_t *wl, struct wlc_if *wlcif, wlc_bsscfg_t *bsscfg, struct lbuf *lb)
{
	struct scb *scb = NULL;
	struct ether_header *eh;
	struct ether_addr *dst;
#ifdef WDS
	struct ether_addr *wds = NULL;
#endif // endif
	wsec_key_t *key;
	uint bandunit;
	bool tkip_enab = FALSE;
	wlc_info_t *wlc = (wlc_info_t *)wl->wlc;

	/* WLDPT, WLTDLS, IAPP, WLAWDL cases currently not handled */

	/* Get dest. */
	eh = (struct ether_header*) PKTDATA(wl->pub->osh, lb);

#ifdef WDS
	if (wlcif && wlcif->type == WLC_IFTYPE_WDS) {
		scb = wlcif->u.scb;
		wds = &scb->ea;
	}

	if (wds)
		dst = wds;
	else
#endif /* WDS */
	if (BSSCFG_AP(bsscfg)) {
#ifdef WLWNM_AP
		/* Do the WNM processing */
		if (WLWNM_ENAB(wlc->pub) && wlc_wnm_dms_amsdu_on(wlc, bsscfg) &&
		    WLPKTTAGSCBGET(lb) != NULL) {
			dst = &(WLPKTTAGSCBGET(lb)->ea);
		}
		else
#endif /* WLWNM_AP */
		dst = (struct ether_addr*)eh->ether_dhost;
	}
	else {
		dst = bsscfg->BSS ? &bsscfg->BSSID : (struct ether_addr*)eh->ether_dhost;
	}

	/* Get key */
	bandunit = CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec);

	/* Class 3 (BSS) frame */
	if (TRUE &&
#ifdef WDS
		!wds &&
#endif // endif
		bsscfg->BSS && !ETHER_ISMULTI(dst)) {

		/* In PCIEDEV TxPost processing, SCBTAG is ENABLED and DISABLED in
		 * pciedev TxPost flowring processing and Host SCB caching can occur.
		 *
		 * In all other invocations of wl_sw_encrypt_mic(), SCBTAG lifetime
		 * will not be ENABLED and no caching occurs.
		 */
		scb = SCBTAG_CACHE(wlc, bsscfg, dst, bandunit);

	}
	/* Class 1 (IBSS/DPT) or 4 (WDS) frame */
	else {
		if (ETHER_ISMULTI(dst))
			scb = WLC_BCMCSCB_GET(wlc, bsscfg);
		else
			scb = wlc_scblookupband(wlc, bsscfg, dst, bandunit);
	}

	if (scb) {
		key = SCBTAG_KEY(scb);
		if (!key)
			key = WSEC_BSS_DEFAULT_KEY(bsscfg);

		if (key == NULL || key->algo == CRYPTO_ALGO_TKIP ||
			(WSEC_SOFTKEY(wlc, key, bsscfg)))
			tkip_enab = TRUE;
	}
	else
		tkip_enab = TRUE;

	return tkip_enab;
}

static bool
wl_tx_pktfetch_required(wl_info_t *wl, wl_if_t *wlif, wlc_bsscfg_t *bsscfg, struct lbuf *lb)
{
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;

	if (WSEC_ENABLED(bsscfg->wsec) && (WLPKTFLAG_PMF(WLPKTTAG(lb)) ||
	   ((WSEC_TKIP_ENABLED(bsscfg->wsec) || WSEC_AES_ENABLED(bsscfg->wsec)) &&
		wl_sw_encrypt_mic(wl, wlcif, bsscfg, lb)))) {
		return TRUE;
	}

	if (ARPOE_ENAB(wl->pub)) {
		wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
		if (arpi) {
			if (wl_arp_send_pktfetch_required(arpi, lb))
				return TRUE;
		}
	}

	if (ntoh16_ua((const void *)(PKTDATA(wl->pub->osh, lb) + ETHER_TYPE_OFFSET))
		== ETHER_TYPE_802_1X) {
		return TRUE;
	}

	return FALSE;
}

static int
wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb)
{
	wl_info_t *wl = dev->softc;
	wl_if_t *wlif = WL_IF(wl, dev);
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;
	void *p;
	struct lbuf *n;
	bool discarded = FALSE;
	bool pktfetch = FALSE;
	wlc_bsscfg_t *bsscfg;
#ifdef PKTC_TX_DONGLE
	bool chainable = FALSE;
	bool cfg_chainable = FALSE;
	void *head = NULL, *tail = NULL;
#endif // endif

	bsscfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (BCMPCIEDEV_ENAB(wl->pub) && PKTISTXFRAG(wl->pub->osh, lb) &&
		(PKTFRAGTOTLEN(wl->pub->osh, lb) > 0))
		pktfetch = TRUE;

#ifdef PKTC_TX_DONGLE
	cfg_chainable = wlconfig_tx_chainable(wl, bsscfg);
#endif // endif

	while (lb != NULL) {
		n = PKTLINK(lb);
		PKTSETLINK(lb, NULL);
		p = PKTFRMNATIVE(wl->pub->osh, lb);
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
			/*
			 * proptxstatus header should be processed only once when
			 * pktfetch is true
			 */
			if (pktfetch && wl_send_txstatus(wl, p)) {
				goto nextlb;
			}
		} else
#endif /* PROP_TXSTATUS */
		{
			/* Pull wl header. Currently no information is included on transmit side */
			wl_pkt_header_pull(wl, p);
		}

		/* For TKIP, MAC layer fragmentation does not work with current split-tx
		 * approach. We need to pull down the remaining payload and recreate the
		 * original 802.3 packet. Packet chaining, if any, is broken here anyway
		 * Need to check wl_sw_encrypt_mic call for all pkts in chain at this point
		 */
		if (pktfetch && wl_tx_pktfetch_required(wl, wlif, bsscfg, p)) {
			wl_tx_pktfetch(wl, p, src, dev);
			goto nextlb;
		}

		WL_TRACE(("wl%d: wl_send: len %d\n", wl->unit, PKTLEN(wl->pub->osh, p)));

		/* Apply ARP offload */
		if (ARPOE_ENAB(wl->pub)) {
			wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
			if (arpi) {
				if (wl_arp_send_proc(arpi, p) ==
					ARP_REPLY_HOST) {
					PKTFREE(wl->pub->osh, p, TRUE);
					goto nextlb;
				}
			}
		}
#ifdef WLNDOE
		/* Apply NS offload */
		if (NDOE_ENAB(wl->pub)) {
			wl_nd_info_t *ndi = wl_get_ndi(wl, wlif);
			if (ndi) {
				wl_nd_send_proc(ndi, p);
			}
		}
#endif // endif

#ifdef WLNFC
		/* Apply NFC offload */
		if (NFC_ENAB(wl->pub)) {
			wl_nfc_info_t *nfci = wl_get_nfci(wl, wlif);
			wl_nfc_send_proc(nfci, p);
		}
#endif // endif

#ifdef TOE
		_wl_toe_send_proc(wl, p);
#endif // endif

#ifdef PKTC_TX_DONGLE
		if (PKTC_ENAB(wl->pub)) {
			chainable = cfg_chainable && wl_txframe_chainable(wl, bsscfg, p, head);

			if (chainable) {
				if (n) {
					PKTSETCHAINED(wl->pub->osh, p);
				}
				PKTCENQTAIL(head, tail, p);
			}

			if (head != NULL &&
			    (n == NULL || !chainable)) {
				if (wlc_sendpkt(wl->wlc, head, wlcif))
					discarded = TRUE;
				head = tail = NULL;
			}
		}

		if (!chainable)
#endif /* PKTC_TX_DONGLE */
		{

			if (wlc_sendpkt(wl->wlc, p, wlcif))
				discarded = TRUE;
		}

nextlb:
		lb = n;
	}

#ifdef PKTC_TX_DONGLE
	/* if last pkt was header only, send remaining chain */
	if (PKTC_ENAB(wl->pub) && (head != NULL)) {
		if (wlc_sendpkt(wl->wlc, head, wlcif))
			discarded = TRUE;
	}
#endif // endif

	return discarded;
}
#else
static int
wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb)
{
	wl_info_t *wl = dev->softc;

	WL_TRACE(("wl%d: wl_send: len %d\n", wl->unit, lb->len));

	bcm_rpc_tp_rx_from_dnglbus(wl->rpc_th, lb);

	return FALSE;
}
#endif /* WLC_HIGH */

#if defined(BCM_OL_DEV) && defined(WLNDOE)
void * wl_get_ndi(wl_info_t *wl, struct wl_if *wlif)
{
	return wl->ndi;
}
#endif /* defined (BCM_OL_DEV) && defined (WLNDOE) */

int wl_busioctl(wl_info_t *wl, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	hndrte_dev_t *chained = wl->dev->chained;
	if (chained && chained->funcs->ioctl)
		return chained->funcs->ioctl(chained, cmd, buf, len, used, needed, set);
	else
		return BCME_ERROR;
}

#ifdef WLC_HIGH
void
wl_txflowcontrol(wl_info_t *wl, struct wl_if *wlif, bool state, int prio)
{
	hndrte_dev_t *chained = wl->dev->chained;

	/* sta mode must be chained */
	if (chained && chained->funcs->txflowcontrol)
		chained->funcs->txflowcontrol(chained, state, prio);
	else
		ASSERT(AP_ENAB(wl->pub));
}

void
wl_event(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
	wl_oid_event(wl->oid, &e->event, e->data);

#ifdef WLPFN
	/* Tunnel events into PFN for analysis */
	if (WLPFN_ENAB(wl->pub))
		wl_pfn_event(wl->pfn, e);
#endif /* WLPFN */
	WL_INFORM((" %s reason: %x status %x wake_event_enable %x\n",
		bcmevent_get_name(e->event.event_type),
		e->event.status, e->event.reason, wl->pub->wake_event_enable));

	switch (e->event.event_type) {
#ifdef EXT_STA
	case WLC_E_DISASSOC:
		if (wl->pub->wake_event_enable & WAKE_EVENT_AP_ASSOCIATION_LOST_BIT) {
			wl->pub->wake_event_status = WAKE_EVENT_AP_ASSOCIATION_LOST_BIT;
		}
		break;
	case WLC_E_PFN_NET_FOUND:
		if (wl->pub->wake_event_enable & WAKE_EVENT_NLO_DISCOVERY_BIT) {
			wl->pub->wake_event_status = WAKE_EVENT_NLO_DISCOVERY_BIT;
		}
		break;
	case WLC_E_EAPOL_MSG:
		if (wl->pub->wake_event_enable & WAKE_EVENT_4WAY_HANDSHAKE_REQUEST_BIT) {
			wl->pub->wake_event_status = WAKE_EVENT_4WAY_HANDSHAKE_REQUEST_BIT;
		}
		break;
	case WLC_E_PSK_SUP:
		if ((wl->pub->wake_event_enable & WAKE_EVENT_GTK_HANDSHAKE_ERROR_BIT) &&
			(e->event.reason == WLC_E_SUP_DECRYPT_KEY_DATA)) {
			/* trigger wake up interrupt */
			wl->pub->wake_event_status = WAKE_EVENT_GTK_HANDSHAKE_ERROR_BIT;
		}
		break;
#endif /* EXT_STA */
	case WLC_E_LINK:
	case WLC_E_NDIS_LINK:
		wl->link = e->event.flags&WLC_EVENT_MSG_LINK;
		if (wl->link)
			WL_ERROR(("wl%d: link up (%s)\n", wl->unit, ifname));
/* Getting too many */
#ifndef EXT_STA
		else
			WL_ERROR(("wl%d: link down (%s)\n", wl->unit, ifname));
#endif /* EXT_STA */
		break;
#if ((defined(STA) && defined(BCMSUP_PSK)) || (defined(AP) && defined(BCMAUTH_PSK)))
	case WLC_E_MIC_ERROR:
		{
			int err = BCME_OK;
			wlc_bsscfg_t *cfg = wlc_bsscfg_find(wl->wlc, e->event.bsscfgidx, &err);
			if (cfg != NULL) {
#if defined(STA) && defined(BCMSUP_PSK)
				if (SUP_ENAB(wl->pub) && BSSCFG_STA(cfg))
					wlc_sup_mic_error(cfg,
						(e->event.flags&WLC_EVENT_MSG_GROUP) ==
						WLC_EVENT_MSG_GROUP);
#endif /* STA && BCMSUP_PSK */
#if defined(AP) && defined(BCMAUTH_PSK)
				if (BCMAUTH_PSK_ENAB(wl->pub) && BSSCFG_AP(cfg))
					wlc_auth_tkip_micerr_handle(wl->wlc, cfg);
#endif /* AP && BCMAUTH_PSK */
			}
		}
		break;
#endif /* (STA && BCMSUP_PSK) || (AP && BCMAUTH_PSK) */
	}
}
#endif /* WLC_HIGH */

void
wl_event_sync(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
#ifdef WL_EVENTQ
	/* duplicate event for local event q */
	wl_eventq_dup_event(wl->wlevtq, e);
#endif /* WL_EVENTQ */
}

void
wl_event_sendup(wl_info_t *wl, const wlc_event_t *e, uint8 *data, uint32 len)
{
}

#ifndef WLC_LOW_ONLY
#ifdef WLOTA_EN
static int
wlc_iovar_wlota_filter(wlc_info_t * wlc, char * name, uint32 cmd)
{

	int bcmerror = BCME_UNSUPPORTED;
	char allowed[5][25] = {
			"ota_trigger",
			"ota_loadtest",
			"ota_teststatus",
			"ota_teststop",
			"bcmerrorstr"
			};
	uint8 i;
	if (wlc->iov_block == NULL)
		return BCME_OK;

	if (*wlc->iov_block != WL_OTA_TEST_ACTIVE)
		return BCME_OK;

	if (cmd <= 1)
		return BCME_OK;

	for (i = 0; i < 5; i++) {
		if (strcmp(name, allowed[i]) == 0) {
			bcmerror = BCME_OK;
			break;
		}
	}
	return bcmerror;
}
#endif /* WLOTA */

static int
wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	wl_info_t *wl = dev->softc;
	wl_if_t *wlif = WL_IF(wl, dev);
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;
	wlc_bsscfg_t *cfg = NULL;
	int ret = 0;
	int origcmd = cmd;
	int status = 0;
	uint32 *ret_int_ptr = (uint32 *)buf;

	WL_TRACE(("wl%d: wl_ioctl: cmd 0x%x\n", wl->unit, cmd));

	cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlcif);
	ASSERT(cfg != NULL);
	switch (cmd) {
	case RTEGHWADDR:
		ret = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, buf, len, IOV_GET, wlcif);
		break;
	case RTESHWADDR:
		ret = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, buf, len, IOV_SET, wlcif);
		break;
	case RTEGPERMADDR:
		ret = wlc_iovar_op(wl->wlc, "perm_etheraddr", NULL, 0, buf, len, IOV_GET, wlcif);
		break;
	case RTEGMTU:
		*ret_int_ptr = ETHER_MAX_DATA;
		break;
#ifdef WLC_HIGH
	case RTEGSTATS:
		wl_statsupd(wl);
		bcopy(&wl->stats, buf, MIN(len, sizeof(wl->stats)));
		break;

	case RTEGALLMULTI:
		*ret_int_ptr = cfg->allmulti;
		break;
	case RTESALLMULTI:
		cfg->allmulti = *((uint32 *) buf);
		break;
#endif /* WLC_HIGH */
	case RTEGPROMISC:
		cmd = WLC_GET_PROMISC;
		break;
	case RTESPROMISC:
		cmd = WLC_SET_PROMISC;
		break;
#ifdef WLC_HIGH
	case RTESMULTILIST: {
		int i;

		/* copy the list of multicasts into our private table */
		cfg->nmulticast = len / ETHER_ADDR_LEN;
		for (i = 0; i < cfg->nmulticast; i++)
			cfg->multicast[i] = ((struct ether_addr *)buf)[i];
		break;
	}
#endif /* WLC_HIGH */
	case RTEGUP:
		cmd = WLC_GET_UP;
		break;
	default:
		/* force call to wlc ioctl handler */
		origcmd = -1;
		break;
	}

#ifdef WLOTA_EN
	if ((ret = wlc_iovar_wlota_filter(wl->wlc, buf, cmd)) != BCME_OK) {
		return ret;
	}
#endif /* WLOTA */

	if (cmd != origcmd) {
		if (!_wl_rte_oid_check(wl, cmd, buf, len, used, needed, set, &status))
			ret = wlc_ioctl(wl->wlc, cmd, buf, len, wlcif);
	}

	if (status)
		return status;

	return (ret);
}
#else

#ifdef BCM_OL_DEV

void
wl_watchdog(wl_info_t *wl)
{
	wlc_info_t *wlc = wl->wlc;

	wlc_dngl_ol_watchdog(wlc->wlc_dngl_ol);

	wlc_scanol_watchdog(wl->wlc_hw);
	wlc_macol_watchdog(wl->wlc_hw);
}

/* Message to HOST */
void wl_msgup(wl_info_t *wl, osl_t *osh, void* resp)
{

	hndrte_dev_t *dev;
	hndrte_dev_t *chained;
	struct lbuf *lb;

	dev = wl->dev;
	chained = dev->chained;

	lb = PKTTONATIVE(osh, resp);

	if (chained->funcs->xmit(dev, chained, lb) != 0) {
		WL_ERROR(("%s: xmit failed; free pkt 0x%p\n", __FUNCTION__, lb));
		lb_free(lb);
	}

}

void
wl_sendup(wl_info_t *wl, struct wl_if *wlif, void *p, int numpkt)
{

	int ret_val = 0;
#ifdef ARPOE
	bool suppressed = FALSE;
	wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
#endif // endif
#ifdef ICMPKAOE
	wl_icmp_info_t *icmpi = wl_get_icmpi(wl, wlif);
#endif // endif
#ifdef TCPKAOE
	wl_tcp_keep_info_t *tcp_keep_info = wl_get_tcpkeepi(wl, wlif);
#endif // endif
#ifdef WLNDOE
	wl_nd_info_t *ndi = wl_get_ndi(wl, wlif);
#endif // endif

#ifdef ARPOE
	if (arpi) {
		ret_val = wl_arp_recv_proc(arpi, p);
		if (ret_val >= 0) {
			if ((ret_val == ARP_REQ_SINK) || (ret_val == ARP_REPLY_PEER)) {
				suppressed = TRUE;
				if (wlc_dngl_ol_supr_frame(wl->wlc, WLPKTTAG(p)->frameptr)) {
					RXOEINC(rxoe_arpsupresscnt);
				}
			} else {
				wlc_dngl_ol_push_to_host(wl->wlc);
			}

			wl_arp_update_stats(arpi, suppressed);
			return;
		}
	}
#endif /* ARPOE */

#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	if (ndi) {
		ret_val = wl_nd_recv_proc(ndi, p);
		if (ret_val >= 0) {
			if ((ret_val == ND_REQ_SINK) || (ret_val == ND_REPLY_PEER)) {
				suppressed = TRUE;
				if (wlc_dngl_ol_supr_frame(wl->wlc, WLPKTTAG(p)->frameptr)) {
					RXOEINC(rxoe_nssupresscnt);
				}
			} else {
				wlc_dngl_ol_push_to_host(wl->wlc);
			}

			wl_nd_update_stats(ndi, suppressed);
			return;
		}
	}
#endif /* WLNDOE */
#ifdef ICMPKAOE

	if (icmpi) {
		ret_val = wl_icmp_recv_proc(icmpi, p);
		if (ret_val >= 0) {
			return;
		}
	}
#endif // endif
#ifdef TCPKAOE
	if (tcp_keep_info) {
		ret_val = wl_tcpkeep_recv_proc(tcp_keep_info, p);
		if (ret_val >= 0) {
			return;
		}
	}
#endif	/* TCPKAOE */

#ifdef PACKET_FILTER
	ret_val = wlc_pkt_filter_ol_process(wl->wlc, p); /* MAGIC PKT */
	if (ret_val >= 0) {
		if (wlc_dngl_ol_supr_frame(wl->wlc, WLPKTTAG(p)->frameptr)) {
			RXOEINC(rxoe_pkt_filter_supresscnt);
		} else {
			wlc_dngl_ol_push_to_host(wl->wlc);
		}

		return;
	}
#endif	/* PACKET_FILTER */

	wlc_dngl_ol_push_to_host(wl->wlc);
	return;
}

/* We may register different msg recepients with callbacks */

static int
wl_handle_msg(hndrte_dev_t *dev, void *buf, int len)
{
	wl_info_t *wl = dev->softc;
	wlc_info_t *wlc = wl->wlc;

	olmsg_header *msg_hdr = buf;

	BCM_REFERENCE(wl);
	BCM_REFERENCE(msg_hdr);

	WL_INFORM(("wl%d: wl_handle_msg for msg type %d\n", wl->unit, msg_hdr->type));
#ifdef BCM_OL_DEV
	wlc_dngl_ol_process_msg(wlc->wlc_dngl_ol, buf, len);
#endif // endif
	return 0;
}

static int
wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	wl_info_t *wl = dev->softc;
	int status = 0;

	BCM_REFERENCE(wl);

	WL_INFORM(("wl%d: wl_ioctl: cmd 0x%x\n", wl->unit, cmd));

	switch (cmd) {
	case 0:	/* PCI msg */
		wl_handle_msg(dev, buf, len);
		break;
	default:
		WL_ERROR(("wl%d: wl_ioctl: cmd 0x%x\n", wl->unit, cmd));
		break;
	}

	return status;
}
#endif /* BCM_OL_DEV */

#endif /* WLC_LOW_ONLY */

static int
BCMUNINITFN(wl_close)(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	BCM_REFERENCE(wl);

	WL_TRACE(("wl%d: wl_close\n", wl->unit));

#ifdef WLC_HIGH
	/* BMAC_NOTE: ? */
	wl_down(wl);
#endif // endif

	return 0;
}

#ifdef WLC_HIGH
static void
wl_statsupd(wl_info_t *wl)
{
	hndrte_stats_t *stats;

	WL_TRACE(("wl%d: wl_get_stats\n", wl->unit));

	stats = &wl->stats;

	/* refresh stats */
	if (wl->pub->up)
		wlc_statsupd(wl->wlc);

	stats->rx_packets = WLCNTVAL(wl->pub->_cnt->rxframe);
	stats->tx_packets = WLCNTVAL(wl->pub->_cnt->txframe);
	stats->rx_bytes = WLCNTVAL(wl->pub->_cnt->rxbyte);
	stats->tx_bytes = WLCNTVAL(wl->pub->_cnt->txbyte);
	stats->rx_errors = WLCNTVAL(wl->pub->_cnt->rxerror);
	stats->tx_errors = WLCNTVAL(wl->pub->_cnt->txerror);
	stats->rx_dropped = 0;
	stats->tx_dropped = 0;
	stats->multicast = WLCNTVAL(wl->pub->_cnt->rxmulti);
}
#endif /* WLC_HIGH */

void
BCMATTACHFN(wl_reclaim)(void)
{
#ifdef DONGLEBUILD
#ifdef BCMRECLAIM
	bcmreclaimed = TRUE;
#endif /* BCMRECLAIM */
	attach_part_reclaimed = TRUE;
	hndrte_reclaim();
#endif /* DONGLEBUILD */
}

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
int
wl_get(void *wlc, int cmd, void *buf, int len)
{
	return wlc_ioctl(wlc, cmd, buf, len, NULL);
}

int
wl_set(void *wlc, int cmd, void *buf, int len)
{
	return wlc_ioctl(wlc, cmd, buf, len, NULL);
}

void
do_wl_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;
	cmd_t *cmd;
	int ret = 0;

	if (argc < 2)
		printf("missing subcmd\n");
	else {
		bool supported = TRUE;

		/* search for command */
		for (cmd = wl_cmds; cmd->name && strcmp(cmd->name, argv[1]); cmd++);

		/* defaults to using the set_var and get_var commands */
		if (cmd->name == NULL) {
			supported = FALSE;
			cmd = &wl_varcmd;
		}

		/* do command */
		ret = (*cmd->func)(wl->wlc, cmd, argv + 1);
#ifdef ATE_BUILD
		if ((ret != BCME_OK) && (supported == FALSE))
			printf("ATE: Command not supported!!!\n");
#endif // endif
		printf("ret=%d (%s)\n", ret, bcmerrorstr(ret));
	}
}

#endif  /* CONFIG_WLU || ATE_BUILD */

#ifdef WLC_LOW_ONLY
static void
do_wlhist_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;

	if (strcmp(argv[1], "clear") == 0) {
		wlc_rpc_bmac_dump_txfifohist(wl->wlc_hw, FALSE);
		return;
	}

	wlc_rpc_bmac_dump_txfifohist(wl->wlc_hw, TRUE);
}

static void
do_wldpcdump_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;

	printf("wlc_dpc(): stopped = %d, requested = %d\n", wl->dpc_stopped, wl->dpc_requested);
	printf("\n");
}
#endif /* WLC_LOW_ONLY */
#ifdef BCMDBG
/* Mini command to control msglevel for BCMDBG builds */
static void
do_wlmsg_cmd(uint32 arg, uint argc, char *argv[])
{
	switch (argc) {
	case 3:
		/* Set both msglevel and msglevel2 */
		wl_msg_level2 = strtoul(argv[2], 0, 0);
		/* fall through */
	case 2:
		/* Set msglevel */
		wl_msg_level = strtoul(argv[1], 0, 0);
		break;
	case 1:
		/* Display msglevel and msglevel2 */
		printf("msglvl1=0x%x msglvl2=0x%x\n", wl_msg_level, wl_msg_level2);
		break;
	}
}
#endif /* BCMDBG */

#ifdef NOT_YET
static int
BCMATTACHFN(wl_module_init)(si_t *sih)
{
	uint16 id;

	WL_TRACE(("wl_module_init: add WL device\n"));

	if ((id = si_d11_devid(sih)) == 0xffff)
		id = BCM4318_D11G_ID;

	return hndrte_add_device(&bcmwl, D11_CORE_ID, id);
}

HNDRTE_MODULE_INIT(wl_module_init);

#endif /* NOT_YET */

#ifdef WLC_LOW_ONLY

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
static void
wl_lowmem_free(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)wlh;
	wlc_info_t *wlc = wl->wlc;
	int i;

	/* process any tx reclaims */
	for (i = 0; i < NFIFO; i++) {
		hnddma_t *di = WLC_HW_DI(wlc, i);
		if (di == NULL)
			continue;
		dma_txreclaim(di, HNDDMA_RANGE_TRANSFERED);
	}
}
#endif /* HNDRTE_PT_GIANT && DMA_TX_FREE */

static void
wl_rpc_tp_txflowctl(hndrte_dev_t *dev, bool state, int prio)
{
	wl_info_t *wl = dev->softc;

	bcm_rpc_tp_txflowctl(wl->rpc_th, state, prio);
}

static void
wl_rpc_down(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)(wlh);

	(void)wl;

	if (wlc_bmac_down_prep(wl->wlc_hw) == 0)
		(void)wlc_bmac_down_finish(wl->wlc_hw);
}

static void
wl_rpc_resync(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)(wlh);

	/* reinit to all the default values */
	wlc_bmac_info_init(wl->wlc_hw);

	/* reload original  macaddr */
	wlc_bmac_reload_mac(wl->wlc_hw);
}

/* CLIENT dongle driver RPC dispatch routine, called by bcm_rpc_buf_recv()
 *  Based on request, push to common driver or send back result
 */
static void
wl_rpc_bmac_dispatch(void *ctx, struct rpc_buf* buf)
{
	wlc_rpc_ctx_t *rpc_ctx = (wlc_rpc_ctx_t *)ctx;

	wlc_rpc_bmac_dispatch(rpc_ctx, buf);
}

static void
wl_rpc_txflowctl(void *wlh, bool on)
{
	wl_info_t *wl = (wl_info_t *)(wlh);

	if (!wl->wlc_hw->up) {
		wl->dpc_stopped = FALSE;
		wl->dpc_requested = FALSE;
		return;
	}

	if (on) {	/* flowcontrol activated */
		if (!wl->dpc_stopped) {
			WL_TRACE(("dpc_stopped set!\n"));
			wl->dpc_stopped = TRUE;
		}
	} else {	/* flowcontrol released */

		if (!wl->dpc_stopped)
			return;

		WL_TRACE(("dpc_stopped cleared!\n"));
		wl->dpc_stopped = FALSE;

		/* if there is dpc requeset pending, run it */
		if (wl->dpc_requested) {
			wl->dpc_requested = FALSE;
			wl_dpc(wl);
		}
	}
}
#endif /* WLC_LOW_ONLY */

/*
 * XXX: Force this routine to be always in RAM, so that OID handling decisions could be
 * done in RAM, with out invaliding other bigger routines in ROM
 * WL_OID won't be defined for cases where the OIDS are either handled on the host or
 * OID support is not needed at all.
*/

#ifndef WLC_LOW_ONLY
static bool
_wl_rte_oid_check(wl_info_t *wl, uint32 cmd, void *buf, int len, int *used, int *needed,
	bool set, int *status)
{
#ifdef WL_OIDS
	/* treat as an OID */
	if (cmd >= OID_GEN_SUPPORTED_LIST) {
		ULONG bytesneeded, bytesused;

		if (set)
			*status = wl_set_oid(wl->oid, (NDIS_OID) cmd, (PVOID) buf, (ULONG) len,
				&bytesused, &bytesneeded);
		else
			*status = wl_query_oid(wl->oid, (NDIS_OID) cmd, (PVOID) buf, (ULONG) len,
				&bytesused, &bytesneeded);
		if (needed)
			*needed = bytesneeded;
		if (used)
			*used = bytesused;
		return TRUE;
	}
#endif /* WL_OIDS */
	return FALSE;
}
#endif /* WLC_LOW_ONLY */

#if defined(WL_WOWL_MEDIA) || defined(WOWLPF)
void wl_wowl_dngldown(struct wl_info *wl, int up_flag)
{
	if (WOWLPF_ENAB(wl->pub)) {
		hndrte_dev_t *chained = NULL;
		hndrte_dev_t *dev = NULL;

		dev = wl->dev;
		if (dev)
			chained = dev->chained;

		if (chained && chained->funcs->wowldown) {
			chained->funcs->wowldown(chained, up_flag);
		}
	}
}
#endif  /* #if defined(WL_WOWL_MEDIA) || defined(WOWLPF) */

#if defined(BCMUSBDEV) || defined(BCMUSBDEV_BMAC)

bool wl_dngl_is_ss(struct wl_info *wl)
{
	hndrte_dev_t *chained = NULL;
	hndrte_dev_t *dev = NULL;
	uint val;
	int ret = BCME_ERROR;

	dev = wl->dev;
	if (dev)
	  chained = dev->chained;

	if (chained && chained->funcs->ioctl) {
		ret = chained->funcs->ioctl(chained, HNDRTE_DNGL_IS_SS, &val,
			sizeof(val), NULL, NULL, FALSE);
	}
	if (ret == BCME_OK && val == TRUE)
		return TRUE;
	return FALSE;
}
#endif /* BCMUSBDEV || BCMUSBDEV_BMAC */

#ifdef BCMPCIEDEV
static uint32 wl_flowring_update(hndrte_dev_t *dev, uint16 flowid, uint8 op, uint8 * sa,
	uint8 *da, uint8 tid)
{
#if defined(PROP_TXSTATUS)
	wl_info_t *wl = dev->softc;
	wl_if_t *wlif = WL_IF(wl, dev);
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
		return wlc_link_txflow_scb(wl->wlc, wlcif, flowid, op, sa, da, tid);
	}
	else
#endif /* (PROP_TXSTATUS) */
	{
		return 0xFF;
	}
}
#endif /* BCMPCIEDEV */

/* Send wl traffic control information to bus level */
void wl_flowring_ctl(wl_info_t *wl, uint32 op, void *opdata)
{
#ifdef BCMPCIEDEV_ENABLED
	bus_ops->flowring_ctl((void *)wl->dev->chained, op, (void *)opdata);
#endif // endif
}

#ifdef BCM_HOST_MEM_SCB
/* Read SB to PCIe translation addr */
int wl_sbaddr(wl_info_t *wl, uint32 *addr, uint32 *len)
{
	uint32 ret = BCME_OK;
#ifdef BCMPCIEDEV_ENABLED
	uint32 buf[16];

	strcpy((char *)buf, "bus:sbaddr");
	ret = wl_busioctl(wl, BUS_GET_VAR, (void *)buf, sizeof(buf), NULL, NULL, 0);

	*addr = buf[0];
	*len = buf[1];
#endif // endif
	return ret;
}
#endif /* BCM_HOST_MEM_SCB */

#if defined(PROP_TXSTATUS) && defined(BCMPCIEDEV)
static int wlfc_push_signal_bus_data(struct wl_info *wl, void* data, uint8 len)
{
	uint8 type = ((uint8*)data)[0];
	flowring_op_data_t	op_data;

	bzero(&op_data, sizeof(flowring_op_data_t));

	switch (type) {
		case WLFC_CTL_TYPE_MAC_OPEN:
		case WLFC_CTL_TYPE_MAC_CLOSE:
			op_data.handle = ((uint8*)data)[2];
			break;

		case WLFC_CTL_TYPE_MACDESC_ADD:
		case WLFC_CTL_TYPE_MACDESC_DEL:
			op_data.handle = ((uint8*)data)[2];
			op_data.ifindex = ((uint8*)data)[3];
			memcpy(op_data.addr, (char *)&((uint8*)data)[4], ETHER_ADDR_LEN);
			break;

		case WLFC_CTL_TYPE_INTERFACE_OPEN:
		case WLFC_CTL_TYPE_INTERFACE_CLOSE:
			op_data.ifindex = ((uint8*)data)[2];
			break;

		case WLFC_CTL_TYPE_TID_OPEN:
		case WLFC_CTL_TYPE_TID_CLOSE:
			op_data.tid = ((uint8*)data)[2];
			break;
		case WLFC_CTL_TYPE_MAC_REQUEST_PACKET:
			op_data.handle = ((uint8*)data)[3];
			op_data.tid = ((uint8*)data)[4]; /* ac bit map */
			op_data.minpkts = ((uint8*)data)[2];
			break;
		default :
			return BCME_ERROR;
	}
	wl_flowring_ctl(wl, type, (void *)&op_data);
	return BCME_OK;
}

#ifdef BCMPCIEDEV
void wlfc_push_pkt_txstatus(struct wl_info *wl, void* p, void *txs, uint32 sz)
{
#ifdef BCM_DHDHDR
	/* Caller has handled what this function will do,
	 * So when BCM_DHDHDR enabled we should run into here
	 */
	ASSERT(0);
#endif // endif
	/* Set state to TXstatus processed becauses
	 * txmetadata processing required later on in pcie layer
	 */
	PKTSETTXSPROCESSED(wl->pub->osh, p);

	if (sz == 1) {
		*((uint8*)(PKTDATA(wl->pub->osh, p) + BCMPCIE_D2H_METADATA_HDRLEN)) =
			*((uint8*)txs);
		PKTSETLEN(wl->pub->osh, p, sz);
		return;
	}

	memcpy(PKTDATA(wl->pub->osh, p) + BCMPCIE_D2H_METADATA_HDRLEN, txs, sz);
	PKTSETLEN(wl->pub->osh, p, BCMPCIE_D2H_METADATA_HDRLEN + sz);
	PKTSETDATAOFFSET(p, ROUNDUP(BCMPCIE_D2H_METADATA_HDRLEN + sz, 4) >> 2);
}
#endif /* BCMPCIEDEV */

int
wlfc_upd_flr_weigth(struct wl_info *wl, uint8 mac_handle, uint8 tid, void* params)
{
#ifdef BCMPCIEDEV_ENABLED
	flowring_op_data_t	op_data;

	bzero(&op_data, sizeof(flowring_op_data_t));

	op_data.tid = tid;
	op_data.handle = mac_handle;
	op_data.extra_params = params;

	wl_flowring_ctl(wl, WLFC_CTL_TYPE_UPD_FLR_WEIGHT, (void *)&op_data);
#endif /* BCMPCIEDEV_ENABLED */
	return BCME_OK;
}

/** Enable/Disable Fair Fetch Scheduling in pciedev layer */
int
wlfc_enab_fair_fetch_scheduling(struct wl_info *wl, void* params)
{
#ifdef BCMPCIEDEV
	flowring_op_data_t	op_data;

	if (BCMPCIEDEV_ENAB(wl->pub)) {
		bzero(&op_data, sizeof(flowring_op_data_t));
		op_data.extra_params = params;
		wl_flowring_ctl(wl, WLFC_CTL_TYPE_ENAB_FFSCH, (void *)&op_data);
	}
#endif /* BCMPCIEDEV */
	return BCME_OK;
}

/** Get Fair Fetch Scheduling status from pciedev layer.
 * status - is the status (1 - on, 0 - off)
 */
int
wlfc_get_fair_fetch_scheduling(struct wl_info *wl, uint32 *status)
{
	int rv = BCME_OK;
#ifdef BCMPCIEDEV
	/* Need to generate an ioctl/iovar request to the bus */
	int cmd_len = strlen("bus:ffsched");
	int cmd_buf_len = cmd_len + 1 + sizeof(uint32);
	char* cmd_buf = (char*)MALLOC(wl->pub->osh, cmd_buf_len);
	if (cmd_buf != NULL) {
		strncpy(cmd_buf, "bus:ffsched", cmd_len);
		cmd_buf[cmd_len] = '\0';
		rv = wl_busioctl(wl, BUS_GET_VAR, cmd_buf,
			cmd_buf_len, NULL, NULL, FALSE);
		if (rv == BCME_OK)
			*status = *((uint32*)cmd_buf);
		else
			WL_ERROR(("wl%d: %s: BUS IOCTL failed, error %d\n",
			wl->unit, __FUNCTION__, rv));
		MFREE(wl->pub->osh, cmd_buf, cmd_buf_len);
	} else {
		WL_ERROR(("wl%d: %s: MALLOC failed\n",
			wl->unit, __FUNCTION__));
		rv = BCME_NOMEM;
	}
#endif /* BCMPCIEDEV */
	return rv;
}
#endif /* PROP_TXSTATUS */

void
wl_flush_rxreorderqeue_flow(struct reorder_rxcpl_id_list *list)
{
#ifdef BCMPCIEDEV_ENABLED
	hndrte_flush_rxreorderqeue(list->head, list->cnt);
	list->cnt = 0;
	list->head = list->tail = 0;
#endif // endif
}

uint32
wl_chain_rxcomplete_id(struct reorder_rxcpl_id_list *list, uint16 id, bool head)
{
#ifdef BCMPCIEDEV_ENABLED
	if (list->cnt == 0) {
		list->head = list->tail = id;
	}
	else {
		if (head) {
			bcm_chain_rxcplid(id, list->head);
			list->head = id;
		} else {
			bcm_chain_rxcplid(list->tail, id);
			list->tail = id;
		}
	}
	list->cnt++;
#endif // endif
	return 0;
}
void
wl_chain_rxcompletions_amsdu(osl_t *osh, void *p, bool norxcpl)
{
#ifdef BCMPCIEDEV_ENABLED
	void *p1;
	uint16 head,  current;

	head = PKTRXCPLID(osh, p);
	p1 = PKTNEXT(osh, p);
	while (p1 != NULL) {
		current = PKTRXCPLID(osh, p1);
		if (current == 0) {
			return;
		}
		bcm_chain_rxcplid(head, current);
		head = current;
		if (norxcpl)
			PKTSETNORXCPL(osh, p1);
		p1 = PKTNEXT(osh, p1);
	}
#endif /* BCMPCIEDEV_ENABLED */
}

void
wl_indicate_maccore_state(struct wl_info *wl, uint8 state)
{
#ifdef BCMPCIEDEV_ENABLED
	/* XXX: need to clean this up, rte does the cache check,
	 *  so call it even if we are setting the same state
	 */
	hndrte_set_ltrstate(0, state);
#endif /* BCMPCIEDEV_ENABLED */
}
#ifdef ECOUNTERS
int
BCMATTACHFN(wl_ecounters_register_source)(uint16 stats_type, wl_ecounters_get_stats some_fn,
	void *context)
{
	return  ecounters_register_source(stats_type, ECOUNTERS_TOP_LEVEL_SW_ENTITY_WL,
		(ecounters_get_stats) some_fn, context);
}

/* The wl_ecounters code know that fn is actually pointer to a
 * funciton of one of its modules.
 * So cast it to ecounters_get_stats fn and run it.
 */
static int
wl_ecounters_entry_point(ecounters_get_stats fn, uint16 stats_type,
	void *context)
{
	wl_ecounters_get_stats wfn = (wl_ecounters_get_stats) fn;
	return (wfn) ? wfn(stats_type, context) : BCME_ERROR;
}

#endif /* ECOUNTERS */
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
void
wl_set_rxoffset_bytes(struct wl_info *wl, uint16 d11rxoffset)
{
	uint32 buf = 0;
	buf = d11rxoffset;

	wl_busioctl(wl, BUS_SET_RX_OFFSET, (void *)&buf,
			sizeof(uint32), NULL, NULL, FALSE);
}

void
wl_set_monitor_mode(struct wl_info *wl, uint32  monitor_mode)
{
	uint32 buf = 0;
	buf = monitor_mode;

	wl_busioctl(wl, BUS_SET_MONITOR_MODE, (void *)&buf,
			sizeof(uint32), NULL, NULL, FALSE);
}
#endif /* WL_MONITOR */

void
wl_set_monitor(wl_info_t *wl, int val)
{
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	wlc_info_t *wlc = wl->wlc;
	wl_set_monitor_mode(wl, val);
	/* wlc->hwrxoff is used by host to strip the rxhdr after forming the radiotap header */
	wl_set_rxoffset_bytes(wl, wlc->hwrxoff);
	wl->monitor_type = (uint32)val;
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */
}
