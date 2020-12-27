/*
 * Transmit Cache block
 * Store all scb pointers required for a fast path in a TCB [Transmit control block]
 * There will be 1:1 mapping among flowring:scb:TCB
 * A new TCB will be created when ever a new tsc entry is added
 * Store all opaque pointers of cubby info and cache info
 * Maintain a list of TCB blocks accesseble by rte layer for lookup during tx path processing
 *
 *  Copyright 2019 Broadcom
 *
 *  This program is the proprietary software of Broadcom and/or
 *  its licensors, and may only be used, duplicated, modified or distributed
 *  pursuant to the terms and conditions of a separate, written license
 *  agreement executed between you and Broadcom (an "Authorized License").
 *  Except as set forth in an Authorized License, Broadcom grants no license
 *  (express or implied), right to use, or waiver of any kind with respect to
 *  the Software, and Broadcom expressly reserves all rights in and to the
 *  Software and all intellectual property rights therein.  IF YOU HAVE NO
 *  AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *  WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *  THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1. This program, including its structure, sequence and organization,
 *  constitutes the valuable trade secrets of Broadcom, and you shall use
 *  all reasonable efforts to protect the confidentiality thereof, and to
 *  use this information only in connection with your use of Broadcom
 *  integrated circuit products.
 *
 *  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *  REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *  OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *  DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *  NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *  ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *  CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *  OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *  BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *  SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *  IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *  IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *  ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *  OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *  NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *  <<Broadcom-WL-IPTag/Proprietary:>>
 *  $Id:$
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11_cfg.h>
#include <wl_export.h>
#include <wlc_types.h>
#include <wlc.h>
#include <wlc_keymgmt.h>
#include <wlc_scb.h>
#include <wlc_txc.h>
#include <wlc_rx.h>
#include <wl_export.h>
#include <wlc_lq.h>
#include <wlc_scb_ratesel.h>
#include <phy_rssi_api.h>
#include <wlc_log.h>
#include <wlc_assoc.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_amsdu.h>
#include <wlfc_proto.h>
#include <wlc_cfp.h>
#include <wlc_cfp_priv.h>
#include <wlc_ap.h>
#if defined(BCMDBG)
#include <wlc_dump.h>
#endif // endif
#ifdef STA
#include <wlc_pm.h>
#endif // endif
#include <wlc_vht.h>
#include <wlc_murx.h>
#include <wlc_murx.h>
#ifdef WLWNM_AP
#include <wlc_wnm.h>
#endif // endif
#include <wlc_addrmatch.h>
#include <wlc_he.h>
#ifdef TINY_PKTJOIN
#error("CFP cant work with TINK PKT JOIN");
#endif // endif
#ifdef WL_BSSCFG_TX_SUP
#error("CFP cant work with WL_BSSCFG_TX_SUP feature");
#endif // endif
#ifdef DONGLEBUILD
#include <wl_rte_priv.h>
#include <hnd_cplt.h>
#else
#include <wl_linux.h>
#endif // endif
#if defined(BCM_PKTFWD)
#include <wl_pktfwd.h>
#endif /* BCM_PKTFWD */
#ifdef L2_FILTER
#include <wlc_l2_filter.h>
#endif /* L2_FILTER */

/**
 * XXX NIC Mode
 *
 * PROP_TXSTATUS dependencies, used for wl to send info to bus layer
 * wlc_cfp_pkt_prepare() Check pktq overflow and drop immediately
 *  exptime = TSFcache(us) + cfglifetime(us) + TSFcache elapsed systime(ms*1000)
 * wl_cfp_rxcmpl_fast() only in dongle build
 */

/** TCB state management for all priorities in one go */
static uint8 tcb_cache[8] = {[0 ... 7]		= SCB_CFP_TCB_CACHE};
static uint8 tcb_paused[8] = {[0 ... 7]		= SCB_CFP_TCB_PAUSED};

/** Init time values for tcb state */
static uint8 tcb_init_state[8] = {[0 ... 7] =
	(SCB_CFP_TCB_CACHE | SCB_CFP_TCB_INI)};

/** Follow a negate logic where '0' represent a state is valid/ready
 * Set init value for tcb states
 */
#define SCB_CFP_TCB_INIT_STATE(hdl) \
	({\
		uint64 *tcb_state = (uint64*)SCB_CFP_TCB_STATE_PTR(hdl); \
		*tcb_state = *((uint64*)tcb_init_state); \
	})

/** Set and Reset pause bit for all priorities */
#define SCB_CFP_TCB_SET_PAUSE(hdl) \
	({\
		uint64 *tcb_state = (uint64*)SCB_CFP_TCB_STATE_PTR(hdl); \
		*tcb_state |= *((uint64*)tcb_paused); \
	})
#define SCB_CFP_TCB_RESET_PAUSE(hdl) \
	({\
		uint64 *tcb_state = (uint64*)SCB_CFP_TCB_STATE_PTR(hdl); \
		*tcb_state &= ~(*((uint64*)tcb_paused)); \
	})

/** Cache Updates */
#define SCB_CFP_TCB_SET_CACHE_VALID(hdl) \
	({\
		uint64 *tcb_state = (uint64*)SCB_CFP_TCB_STATE_PTR(hdl); \
		*tcb_state &= ~(*((uint64*)tcb_cache)); \
		SCB_CFP_CACHE_GEN(hdl)++; \
	})

#define SCB_CFP_TCB_SET_CACHE_INVALID(hdl) \
	({\
		uint64 *tcb_state = (uint64*)SCB_CFP_TCB_STATE_PTR(hdl); \
		*tcb_state |= *((uint64*)tcb_cache); \
	})

/** RCB state management for all priorities in one go */

/** Init time values for rcb state */
uint8 rcb_init_state[8] = {[0 ... 7] =
	(SCB_CFP_RCB_RESPONDER)};

static uint8 rcb_paused[8] = {[0 ... 7]		= SCB_CFP_RCB_PAUSE};

/** Set and Reset pause bit for all priorities */
#define SCB_CFP_RCB_SET_PAUSE_ALL(hdl) \
	({\
		uint64 *rcb_state = (uint64*)SCB_CFP_RCB_STATE_PTR(hdl); \
		*rcb_state |= *((uint64*)rcb_paused); \
	})
#define SCB_CFP_RCB_RESET_PAUSE_ALL(hdl) \
	({\
		uint64 *rcb_state = (uint64*)SCB_CFP_RCB_STATE_PTR(hdl); \
		*rcb_state &= ~(*((uint64*)rcb_paused)); \
	})

/** Follow a negate logic where '0' represent a state is valid/ready
 * Set init value for tcb states
 */
#define SCB_CFP_RCB_INIT_STATE(hdl) \
	({\
		uint64 *rcb_state = (uint64*)SCB_CFP_RCB_STATE_PTR(hdl); \
		*rcb_state = *((uint64*)rcb_init_state); \
	})

/** CFP Module Debug formatting */
#define WLC_CFPMOD_FMT \
	"%-10s %-12s: wl%d cfp_flowid 0x%04x scb_cfp %p prio %u state %u "
#define WLC_CFPMOD_ARG(str) \
	"CFPMODULE", str, wlc->pub->unit, cfp_flowid, scb_cfp, \
	prio, SCB_CFP_TCB_STATE(scb_cfp, prio)

/** Cached Flow Processing SCB Cubby */
#define SCB_CFP_CUBBY_LOC(cfp, scb) \
	((scb_cfp_t **) SCB_CUBBY((scb), WLC_CFP_PRIV(cfp)->scb_hdl))

#define SCB_CFP(cfp, scb)     (*SCB_CFP_CUBBY_LOC((cfp), (scb)))

/** File scoped CFP Global object: Fast access to CFP module and CFP Cubbies. */
struct wlc_cfp_global {
	wlc_cfp_t  *wlc_cfp;
	scb_cfp_t **scb_cfp_list;
	uint16	   *amt_lookup;
} __attribute__ ((__aligned__(64)));

typedef struct wlc_cfp_global wlc_cfp_global_t;

#define CFP_G_INIT { \
	.wlc_cfp = (wlc_cfp_t*)NULL, \
	.scb_cfp_list = (scb_cfp_t **)NULL, \
	.amt_lookup = (uint16 *)NULL \
}

/**
 * In full dongle, a single global CFP instance manages the radio.
 * In NIC mode, upto 4 global instances exist allowing 3 radios (atlas config)
 */
#if defined(DONGLEBUILD)
#define WLC_CFP_UNIT_MAX    1 /* Single radio global CFP instance */
#define _CFP_UNIT(cfp_unit) 0 /* Ignore unit number, default to 0 */
#define WLC_CFP_G_INIT      CFP_G_INIT
#else /* ! DONGLEBUILD */
#define WLC_CFP_UNIT_MAX    4 /* Max 4 global CFP instances for 4 radios */
#define _CFP_UNIT(cfp_unit) (cfp_unit)
#define WLC_CFP_G_INIT      CFP_G_INIT, CFP_G_INIT, CFP_G_INIT, CFP_G_INIT
#endif /* DONGLEBUILD */

/** Unit number used to index the global, defaults to 0 in FullDongle */
#define WLC_UNIT(wlc)               _CFP_UNIT((wlc)->pub->unit)
#define WLC_CFP_UNIT(wlc_cfp_info)  _CFP_UNIT((wlc_cfp_info)->unit)

wlc_cfp_global_t wlc_cfp_global[WLC_CFP_UNIT_MAX] = { WLC_CFP_G_INIT };

/** File scoped CFP Module Global Pointer. */
#define WLC_CFP_G(cfp_unit) \
	wlc_cfp_global[_CFP_UNIT(cfp_unit)].wlc_cfp

/** File scoped CFP list of pointers to all SCB cubbies. */
#define SCB_CFP_LIST_G(cfp_unit) \
	wlc_cfp_global[_CFP_UNIT(cfp_unit)].scb_cfp_list

/** File scoped fetching of a SCB CFP cubby given a CFP flowid. */
#define SCB_CFP_ID_TO_PTR_G(cfp_unit, cfp_flowid) \
	wlc_cfp_global[_CFP_UNIT(cfp_unit)].scb_cfp_list[cfp_flowid]

/** File scoped AMT-CFP lookup table global Pointer */
#define WLC_CFP_AMT_LOOKUP_G(cfp_unit) \
	wlc_cfp_global[_CFP_UNIT(cfp_unit)].amt_lookup

/** File scoped : AMT ID to CFP ID */
#define WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) \
	wlc_cfp_global[_CFP_UNIT(cfp_unit)].amt_lookup[(amt_idx)]

/** File scoped: AMT ID to CFP pointer */
#define WLC_AMT_ID_TO_CFP_PTR_G(cfp_unit, cfp_flowid) \
	SCB_CFP_ID_TO_PTR_G((cfp_unit), (WLC_CFP_AMT_FLOWID_G((cfp_unit), (amt_idx))))

/** Audit CFP globals before accessing. */
#ifdef BCMDBG
#define CFP_AUDIT_G(cfp_unit) \
	({ \
		ASSERT(WLC_CFP_G(cfp_unit) != (wlc_cfp_t *)NULL); \
		ASSERT(SCB_CFP_LIST_G(cfp_unit) != (scb_cfp_t **)NULL); \
		ASSERT(SCB_CFP_LIST_G(cfp_unit) == WLC_CFP_G(cfp_unit)->priv.scb_cfp_list); \
	})
#else   /* ! BCMDBG */
#define CFP_AUDIT_G(cfp_unit)   WLC_CFP_NOOP
#endif  /* ! BCMDBG */

#define FOREACH_CFP_FLOWID_G(cfp_unit, cfp_flowid, cubby) \
	for (cfp_flowid = 0, cubby = SCB_CFP_ID_TO_PTR_G((cfp_unit), (cfp_flowid)); \
		cfp_flowid < CFP_FLOWS_MAX; \
		cfp_flowid++, cubby = SCB_CFP_ID_TO_PTR_G((cfp_unit), (cfp_flowid)))

#define FOREACH_AMT_IDX_G(cfp_unit, amt_size, amt_idx, cfp_flowid) \
	for (amt_idx = 0, cfp_flowid = WLC_CFP_AMT_FLOWID_G((cfp_unit), (amt_idx)); \
		amt_idx < amt_size; \
		amt_idx++, cfp_flowid = WLC_CFP_AMT_FLOWID_G((cfp_unit), (amt_idx)))

#define AMT_IDX_VALID(wlc, idx) \
	(((idx) >= 0) && ((idx) < AMT_SIZE((wlc)->pub->corerev)))

#define ASSERT_AMT_IDX(wlc, idx)    ASSERT(AMT_IDX_VALID((wlc), (idx)))

/** IOVAR table */
enum {
	IOV_CFP_TCB         = 0,
	IOV_CFP_RCB         = 1,
	IOV_CFP_DUMP_ID     = 2,
	IOV_CFP_TCB_INV     = 3,
	IOV_LAST            = 4
};

static const bcm_iovar_t cfp_iovars[] = {
	{"cfp_tcb", IOV_CFP_TCB, (0), 0, IOVT_BOOL, 0},
	{"cfp_rcb", IOV_CFP_RCB, (0), 0, IOVT_BOOL, 0},
	{"cfp_dump", IOV_CFP_DUMP_ID, (0), 0, IOVT_UINT16, 0},
	{"cfp_tcb_inv", IOV_CFP_TCB_INV, (0), 0, IOVT_INT16, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/** SCB CFP Cubby functions registered during wlc_scb_cubby_reserve. */
static int wlc_cfp_scb_init(void *ctx, struct scb *scb);
static void wlc_cfp_scb_deinit(void *ctx, struct scb *scb);
static void wlc_cfp_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
static void wlc_cfp_scb_rcb_node_init(scb_cfp_t * scb_cfp);
#ifdef WLSQS
static void wlc_scb_cfp_ringid_init(scb_cfp_t * scb_cfp);
#endif /* WLSQS */
#if defined(BCMDBG)
static int wlc_cfp_dump(void *ctx, struct bcmstrbuf *b);
static int wlc_cfp_dump_clr(void *ctx);
#ifdef WLSQS
static int wlc_cfp_scb_qstats_dump(void *ctx, struct bcmstrbuf *b);
#endif /* WLSQS */
#endif // endif
static int wlc_get_cfp_flowid(wlc_info_t *wlc, struct ether_addr *ea);

/** CFP util functions */
static int wlc_cfp_scb_dump_id(wlc_cfp_t *cfp, uint16 flowid, struct bcmstrbuf *b);
#if defined(BCMDBG)
static int wlc_cfp_scb_clear_stats_id(wlc_cfp_t *cfp, uint16 flowid);
#endif // endif

/** SCB CFP Cubby callback to be invoked on SCB state update. */
static void __wlc_cfp_scb_state_upd(void *ctx, scb_state_upd_data_t *data);

/** CFP Module callbacks */
static int wlc_cfp_doiovar(void *ctx, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static void wlc_cfp_watchdog(void *ctx);

/** CFP capability Checks */
static bool wlc_scb_cfp_allowed(struct scb *scb);
static bool wlc_scb_cfp_rx_allowed(wlc_cfp_info_t* cfp_info, struct scb *scb);
static bool wlc_scb_cfp_tx_allowed(wlc_cfp_info_t* cfp_info, struct scb *scb);

/** CFP capability Disable utils */
static void wlc_scb_cfp_disable(wlc_cfp_info_t* cfp_info, struct scb *scb);
static void wlc_scb_cfp_rx_disable(wlc_cfp_info_t* cfp_info, struct scb *scb);
static void wlc_scb_cfp_tx_disable(wlc_cfp_info_t* cfp_info, struct scb *scb);

/** CFP capability Enable utils */
static void wlc_scb_cfp_rx_enable(wlc_cfp_info_t* cfp_info, struct scb *scb);
static void wlc_scb_cfp_tx_enable(wlc_cfp_info_t* cfp_info, struct scb *scb);

/** Fillup cached pointers in the SCB_CFP cubby */
static void wlc_scb_cfp_fillup(wlc_cfp_info_t* cfp_info, struct scb *scb);
static void wlc_scb_cfp_rcb_fillup(wlc_info_t *wlc, struct scb* scb, scb_cfp_t *scb_cfp);
static void wlc_scb_cfp_tcb_fillup(wlc_info_t *wlc, struct scb* scb, scb_cfp_t *scb_cfp);

#if defined(DONGLEBUILD)
static void wl_cfp_rxcmpl_fast(wl_info_t *wl, void *p, uint8 index, uint32 *pkt_cnt);
#endif /* DONGLEBUILD */

/** CFP RX Fns. */
static bool wlc_cfp_rxframe_hdr_chainable(wlc_info_t *wlc, scb_cfp_t *scb_cfp,
	struct dot11_header *h);
/** 3. CFP RX CHAINED SENDUP */
static void wlc_cfp_scb_chain_sendup(wlc_info_t *wlc, scb_cfp_t * scb_cfp, uint8 prio);
/** 2. CFP RX Sendup wlc_cfp_rx_sendup  */
/** 1. CFP RX CHAINABLE  wlc_cfp_rxframe */

/** CFP module attach handler */
wlc_cfp_info_t *
BCMATTACHFN(wlc_cfp_attach)(wlc_info_t *wlc)
{
	wlc_cfp_t *cfp; /* CFP module */
	wlc_cfp_info_t *cfp_info; /* CFP module public */
	wlc_cfp_priv_t *cfp_priv; /* CFP module private */
	uint16 idx;
	int cfp_unit;

	cfp_info = NULL;
	cfp_unit = WLC_UNIT(wlc);

	/* Allocate the CFP module */
	if ((cfp = MALLOCZ(wlc->osh, WLC_CFP_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		         wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Setup a global pointer to CFP module */
	WLC_CFP_G(cfp_unit) = cfp;

	cfp_info = WLC_CFP_INFO(cfp);
	cfp_priv = WLC_CFP_PRIV(cfp);

	cfp_info->unit = cfp_unit;
	/* Fill up private structure */
	cfp_priv->wlc = wlc;

	/* Initialize the linked list of RCBs holding packets */
	dll_init(CFP_RCB_PEND_LIST(cfp_info));
	dll_init(CFP_RCB_DONE_LIST(cfp_info));

	/* Disabling CFP for corerev < 40
	 * AMT & AMPDU AQM is not supported
	 * Should populate TXD header (Equivalent of wlc_cfp_ampdu_txd_ge40_fixup())
	 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		WL_ERROR(("wl%d: %s: Not enabling CFP support for corerev %d\n",
			wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
		return cfp_info;
	}

	/* Enable caching globally for all scbs */
	CFP_TCB_ENAB(cfp_info) = TRUE;

#if defined(CFP_REVLT80_UCODE_WAR)
	CFP_RCB_ENAB(cfp_info) = TRUE;
#else /* ! CFP_REVLT80_UCODE_WAR */

	/* TODO: For correvs >= 40 && < 80,
	 * Need ucode support to fill AMT index in RxHdr (d11rxhdr_t)
	 */
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		CFP_RCB_ENAB(cfp_info) = TRUE;
	} else {
		WL_ERROR(("wl%d: %s: Not enabling CFP_RCB support for corerev %d\n",
			wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
	}
#endif /* ! CFP_REVLT80_UCODE_WAR */

	/* Allocate a global table for CFP ID to Cubby ptr Lookup */
	cfp_priv->scb_cfp_list = MALLOCZ(wlc->osh,
		((sizeof(scb_cfp_t*)) * (CFP_FLOWS_MAX + 1)));

	if (cfp_priv->scb_cfp_list == NULL) {
		WL_ERROR(("wl%d: %s: Failed to allocate %d bytes\n",
		         wlc->pub->unit, __FUNCTION__,
			(int)((sizeof(scb_cfp_t*)) * (CFP_FLOWS_MAX + 1))));

		goto fail;
	}

	/* Setup a global pointer to scb cfp list */
	SCB_CFP_LIST_G(cfp_unit) = cfp_priv->scb_cfp_list;

	 /* Reserve a CFP cubby in the SCB, for the CFP module */
	cfp_priv->scb_hdl =
		wlc_scb_cubby_reserve(wlc, sizeof(scb_cfp_t *),
			wlc_cfp_scb_init, wlc_cfp_scb_deinit, wlc_cfp_scb_dump, cfp);

	if (cfp_priv->scb_hdl < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Register scb update calbacks for the CFP module */
	if (wlc_scb_state_upd_register(wlc, __wlc_cfp_scb_state_upd, cfp_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Register CFP module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, cfp_iovars, "cfp", cfp, wlc_cfp_doiovar,
			wlc_cfp_watchdog, NULL, NULL) != BCME_OK)
	{
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Construct a 16bit flowid allocator */

	/* scb_flowid_allocator is used to allocate CFP flow ids for unicast traffic
	 * with flowids in the inclusive range [ 1 .. MAXSCB ].
	 *
	 * int_flowid_allocator is used to allocate CFP flow ids for the "internal"
	 * SCBs (BCM per BSS, HWRS and OLPC), and will have value in the inclusive
	 * range [ MAXSCB+1 .. CFP_FLOWS_MAX-1 ], for CFP_FLOWID_INT_TOTAL values
	 */

	cfp_priv->scb_flowid_allocator =
		id16_map_init(wlc->osh, CFP_FLOWID_SCB_TOTAL, CFP_FLOWID_SCB_STARTID);
	if (cfp_priv->scb_flowid_allocator == NULL) {
		WL_ERROR(("wl%d: %s: scb_flowid allocator init failure\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	cfp_priv->int_flowid_allocator =
		id16_map_init(wlc->osh, CFP_FLOWID_INT_TOTAL, CFP_FLOWID_INT_STARTID);
	if (cfp_priv->int_flowid_allocator == NULL) {
		WL_ERROR(("wl%d: %s: int_flowid allocator init failure\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Allocate a lookup table for AMT idx to CFP flow ID convertor */
	cfp_priv->amt_lookup = MALLOCZ(wlc->osh, (sizeof(uint16) * AMT_SIZE(wlc->pub->corerev)));
	if (cfp_priv->amt_lookup == NULL) {
		WL_ERROR(("wl%d: %s: AMT lookup table init failed  \n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Initialize the table with CFP_FLOWID_INVALID */
	for (idx = 0; idx < AMT_SIZE(wlc->pub->corerev); idx++) {
		cfp_priv->amt_lookup[idx] = CFP_FLOWID_INVALID;
	}

	WLC_CFP_DEBUG(("%s: CFP module %p CFP info %p CFP private %p ",
		__FUNCTION__, cfp, cfp_info, cfp_priv));

	/* Setup a global pointer to AMT-CFP lookup table */
	WLC_CFP_AMT_LOOKUP_G(cfp_unit) = cfp_priv->amt_lookup;

#if defined(BCMDBG)
	/* Register dump and dump_clr routine for cfp */
	wlc_dump_add_fns(wlc->pub, "cfp", wlc_cfp_dump, wlc_cfp_dump_clr, (void *)cfp);
#ifdef WLSQS
	wlc_dump_add_fns(wlc->pub, "scb_qstats", wlc_cfp_scb_qstats_dump, NULL, (void *)cfp);
#endif /* WLSQS */
#endif // endif

	wlc->pub->_cfp = TRUE;

	return cfp_info; /* return the CFP module public pointer */

fail:
	MODULE_DETACH(cfp_info, wlc_cfp_detach);
	return NULL;
}

/** CFP module detach handler */
void
BCMATTACHFN(wlc_cfp_detach)(wlc_cfp_info_t *cfp_info)
{
	wlc_info_t *wlc;
	wlc_cfp_t *cfp; /* CFP module */
	wlc_cfp_priv_t *cfp_priv; /* CFP module private */
	int cfp_unit;

	if (cfp_info == NULL)
		return;

	cfp = WLC_CFP(cfp_info);
	cfp_priv = WLC_CFP_PRIV(cfp);

	wlc = cfp_priv->wlc;
	cfp_unit = WLC_UNIT(wlc);
	BCM_REFERENCE(cfp_unit);

	/* Remove AMT Lookup table */
	if (cfp_priv->amt_lookup) {
		MFREE(wlc->osh, cfp_priv->amt_lookup,
			(sizeof(uint16) * AMT_SIZE(wlc->pub->corerev)));
	}

	/* Remove CFP flowid allocator */
	if (cfp_priv->scb_flowid_allocator) {
		id16_map_fini(wlc->osh, cfp_priv->scb_flowid_allocator);
		cfp_priv->scb_flowid_allocator = NULL;
	}
	if (cfp_priv->int_flowid_allocator) {
		id16_map_fini(wlc->osh, cfp_priv->int_flowid_allocator);
		cfp_priv->int_flowid_allocator  = NULL;
	}

	/* Remove CFP flowid to cubby ptr Lookup table */
	if (cfp_priv->scb_cfp_list) {
		MFREE(wlc->osh, cfp_priv->scb_cfp_list,
			((sizeof(scb_cfp_t*)) * (CFP_FLOWS_MAX + 1)));
	}

	wlc->pub->_cfp = FALSE;

	/* Unregister the module */
	wlc_module_unregister(wlc->pub, "cfp", cfp);

	/* Free up the CFP module memory */
	MFREE(wlc->osh, cfp, WLC_CFP_SIZE);

	/* Reset the global CFP module */
	WLC_CFP_AMT_LOOKUP_G(cfp_unit) = (uint16 *)NULL;
	SCB_CFP_LIST_G(cfp_unit)       = (scb_cfp_t**)NULL;
	WLC_CFP_G(cfp_unit)            = (wlc_cfp_t*)NULL;
}

/** SCB CFP cubby init handler */
static int
wlc_cfp_scb_init(void *ctx, struct scb *scb)
{
	uint16 flowid;
	wlc_info_t *wlc;
	wlc_cfp_t *cfp; /* CFP module */
	wlc_cfp_priv_t *cfp_priv; /* CFP module private */
	int cfp_unit;

	ASSERT(ctx != NULL);
	cfp = (wlc_cfp_t *)ctx;
	cfp_priv = WLC_CFP_PRIV(cfp);
	wlc = cfp_priv->wlc;

	cfp_unit = WLC_CFP_UNIT(WLC_CFP_INFO(cfp));
	BCM_REFERENCE(cfp_unit);

	/* Local storage for SCB CFP cubby */
	scb_cfp_t **scb_cfp_ptr = SCB_CFP_CUBBY_LOC(cfp, scb);
	scb_cfp_t *scb_cfp = NULL;

	/* Alloc the SCB CFP cubby */
	if ((scb_cfp = MALLOCZ(wlc->osh, SCB_CFP_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate CFP cubby, size %d\n",
			wlc->pub->unit, __FUNCTION__, (int)SCB_CFP_SIZE));
		goto fail;
	}

	/* Store SCB CFP cubby pointer */
	*scb_cfp_ptr = scb_cfp;

	/* Assign a flowid to the SCB CFP cubby */
	/* this ID will be used for all fast path lookup */
	if (SCB_INTERNAL(scb)) {
		ASSERT(cfp_priv->int_flowid_allocator != NULL);
		flowid = id16_map_alloc(cfp_priv->int_flowid_allocator);
	} else {
		ASSERT(cfp_priv->scb_flowid_allocator != NULL);
		flowid = id16_map_alloc(cfp_priv->scb_flowid_allocator);
	}

	if (flowid == ID16_INVALID) {
		WL_ERROR(("wl%d: %s Failed to allocate internal=%d flowid for CFP cubby\n",
			wlc->pub->unit, __FUNCTION__, SCB_INTERNAL(scb) ? 1 : 0));
		goto fail;
	}

	/* Store flowid in SCB CFP cubby */
	SCB_CFP_FLOWID(scb_cfp) = flowid;

	/* Store scb back pointer in SCB CFP cubby */
	SCB_CFP_SCB(scb_cfp) = scb;

	/* Add SCB CFP cubby to list of fast flows */
	SCB_CFP_ID_TO_PTR_G(cfp_unit, flowid) = scb_cfp;

	/* Initialize per prio state */
	SCB_CFP_TCB_INIT_STATE(scb_cfp);
	SCB_CFP_RCB_INIT_STATE(scb_cfp);
#ifdef WLSQS
	/* Initialze Host flowringds */
	wlc_scb_cfp_ringid_init(scb_cfp);
#endif /* WLSQS */

	/* Initialize RCB chained packet queues */
	pktq_init(SCB_CFP_RCB_CHAINEDQ(scb_cfp), NUMPRIO, PKTQ_LEN_MAX);

	/* Initialize RCB nodes */
	wlc_cfp_scb_rcb_node_init(scb_cfp);

	/* Link CFP & AMT Flow IDs
	 *
	 * Internal SCBs seem to end up with BC/MC address
	 * Dont use them for AMT linkups
	 */
	if (!SCB_INTERNAL(scb)) {
		wlc_cfp_amt_link_init(wlc, flowid, &(scb->ea));
	}

	WLC_CFP_DEBUG(("CUBBY Init for CFP %p size %d scb %p flowid %d  "
		"EA "MACF" \n", scb_cfp, (int) SCB_CFP_SIZE, scb,
		flowid, ETHER_TO_MACF(scb->ea)));

	return BCME_OK;

fail:
	wlc_cfp_scb_deinit(ctx, scb);
	return BCME_NOMEM;
}
#ifdef WLSQS
/** Initialize host flow ringids */
static void
wlc_scb_cfp_ringid_init(scb_cfp_t * scb_cfp)
{
	uint8 idx;

	/* Iterate through priorities */
	for (idx = 0; idx < NUMPRIO; idx++) {
		SCB_CFP_RINGID(scb_cfp, idx) = SCB_CFP_RINGID_INVALID;
	}
}
#endif /* WLSQS */

/** Initialize RCB node with scb_cfp and prio info */
static void
wlc_cfp_scb_rcb_node_init(scb_cfp_t * scb_cfp)
{
	uint8 idx;
	scb_cfp_rcb_node_t	*rcb_node;

	for (idx = 0; idx < NUMPRIO; idx++) {
		rcb_node = SCB_CFP_RCB_DLL_NODE(scb_cfp, idx);

		/* Node to be filled with back pointer to scb_cfp & prio */
		rcb_node->scb_cfp = scb_cfp;
		rcb_node->prio = idx;
		rcb_node->pending = FALSE;
	}
}

/** SCB CFP cubby deinit handler */
static void
wlc_cfp_scb_deinit(void *ctx, struct scb *scb)
{
	int cfp_unit;
	uint16 flowid;
	wlc_info_t *wlc;
	wlc_cfp_t *cfp; /* CFP module */
	wlc_cfp_priv_t *cfp_priv; /* CFP module private */

	ASSERT(ctx != NULL);
	cfp = (wlc_cfp_t *)ctx;
	cfp_priv = WLC_CFP_PRIV(cfp);
	wlc = cfp_priv->wlc;

	cfp_unit = WLC_CFP_UNIT(WLC_CFP_INFO(cfp));
	BCM_REFERENCE(cfp_unit);

	/* SCB CFP cubby pointer */
	scb_cfp_t **scb_cfp_ptr = SCB_CFP_CUBBY_LOC(cfp, scb);
	scb_cfp_t *scb_cfp = *scb_cfp_ptr;

	if (scb_cfp == NULL)
		return;

	/* Release the flow id */
	flowid = scb_cfp->flowid;

#if defined(DONGLEBUILD)
	/* Remove all CFP reference from bus layer */
	wl_bus_cfp_flow_delink(wlc->wl, flowid);
#endif /* DONGLEBUILD */

	/* Remove all CFP reference from amt lookup tbl */
	wlc_cfp_amt_flowid_delink(wlc, AMT_FLOWID_INVALID(wlc->pub->corerev), flowid);

	if (CFP_FLOWID_VALID(flowid)) {
		SCB_CFP_ID_TO_PTR_G(cfp_unit, flowid) = NULL; /* release from scb cfp list */
		if (SCB_INTERNAL(scb)) {
			id16_map_free(cfp_priv->int_flowid_allocator, flowid);
		} else {
			id16_map_free(cfp_priv->scb_flowid_allocator, flowid);
		}
	}

	WLC_CFP_DEBUG(("CUBBY deinit for CFP %p size %d scb %p flowid %d\n",
		scb_cfp, (int) SCB_CFP_SIZE, scb, flowid));

	/* Zero out the SCB CFP cubby contents */
	memset(scb_cfp, 0, sizeof(scb_cfp_t));

	/* Release the SCB CFP cubby memory */
	MFREE(wlc->osh, scb_cfp, sizeof(scb_cfp_t));

	/* Set the SCB CFP cubby pointer to NULL */
	*scb_cfp_ptr = NULL;
}

/** User level SCB CFP state update.
 * Update all scb cfp states on user capability changes.
 */
void
wlc_cfp_state_update(wlc_cfp_info_t * cfp_info)
{
	uint16 idx;
	scb_cfp_t	*scb_cfp; /* SCB CFP Cubby */
	struct scb	*scb;
	wlc_info_t	*wlc;

	wlc = WLC_CFP_PRIV(WLC_CFP_G(WLC_CFP_UNIT(cfp_info)))->wlc;
	BCM_REFERENCE(wlc);

	if (!CFP_ENAB(wlc->pub)) {
		return;
	}

	/* Loop through all flowids */
	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(cfp_info), idx, scb_cfp) {
		/* Update the state */
		if (scb_cfp) {
			scb = SCB_CFP_SCB(scb_cfp);

			/* Update per scb cfp state */
			wlc_cfp_scb_state_upd(cfp_info, scb);
		}
	}
}

/** SCB CFP cubby callback to be invoked on scb state updates */
static void
__wlc_cfp_scb_state_upd(void* ctx, scb_state_upd_data_t *data)
{
	struct scb *scb;
	wlc_cfp_info_t	*cfp_info;

	ASSERT(data != NULL);
	scb = data->scb;
	cfp_info = (wlc_cfp_info_t*)ctx;

	wlc_cfp_scb_state_upd(cfp_info, scb);
}

/**
 * Exported function to update scb-cfp state on scb capability update
 * 1. Check common capabilities first
 * 2. check if CFP TX allowed
 * 3. Check if CFP RX allowed
 * 4. Fillup cfp cubby info if CFP allowed.
 */
void
wlc_cfp_scb_state_upd(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	wlc_cfp_t	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* Cubby */
	wlc_info_t	*wlc;

	wlc = WLC_CFP_PRIV(WLC_CFP_G(WLC_CFP_UNIT(cfp_info)))->wlc;
	BCM_REFERENCE(wlc);
	if (!CFP_ENAB(wlc->pub)) {
		return;
	}

	WLC_CFP_DEBUG(("%s: SCB state update :: scb %p cfp info %p\n",
		__FUNCTION__, scb, cfp_info));

	/* Validate */
	ASSERT(scb);

	/* Initialize */
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from tx cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return;
	}

	/* Check if SCB CFP capable */
	if (wlc_scb_cfp_allowed(scb)) {

		/* Fillup common scb cfp info */
		wlc_scb_cfp_fillup(cfp_info, scb);

		/* Check if scb is capable */
		if (wlc_scb_cfp_tx_allowed(cfp_info, scb)) {
			wlc_scb_cfp_tx_enable(cfp_info, scb);
		} else {
			wlc_scb_cfp_tx_disable(cfp_info, scb);
		}

		/* Check if scb is capable */
		if (wlc_scb_cfp_rx_allowed(cfp_info, scb)) {
			wlc_scb_cfp_rx_enable(cfp_info, scb);
		} else {
			wlc_scb_cfp_rx_disable(cfp_info, scb);
		}
	} else {
		/* Disable CFP TX/RX */
		wlc_scb_cfp_disable(cfp_info, scb);
	}
}

/** Check if Cached Flow Processing is allowed for this SCB
 * Only common checks to be part of this.
 * TX /RX specific checks should be included in appropriate fns
 */
static bool
wlc_scb_cfp_allowed(struct scb *scb)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	wlc_key_info_t key_info;
	bool assoc_and_auth;

	ASSERT(scb != NULL);

	if (SCB_DEL_IN_PROGRESS(scb) || SCB_MARKED_DEL(scb))
		return FALSE;

	/* Initialize */
	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg);

	wlc = bsscfg->wlc;
	ASSERT(wlc);

	/* Check for HW Clock */
	if (!wlc->clk)
		return FALSE;

	/* AMPDU AQM enabled */
	if (!AMPDU_AQM_ENAB(wlc->pub))
		return FALSE;

	if (!PKTC_ENAB(wlc->pub))
		return FALSE;

	/* XXX For now don't enable CFP for following configs.
	 * Will enable as and when functionality is added.
	 */
	if (WET_ENAB(wlc))
		return FALSE;

	/* Disabled for TDLS mode */
	if (TDLS_ENAB(wlc->pub))
		return FALSE;

	/* CFP currently disabled for PSTA */
	if (BSSCFG_PSTA(bsscfg))
		return FALSE;

	/* CFP disabled for IBSS */
	if (BSSCFG_IBSS(bsscfg))
		return FALSE;

	if (BSS_TDLS_BUFFER_STA(bsscfg))
		return FALSE;

	if (SCB_INTERNAL(scb))
		return FALSE;

	if (SCB_PS(scb))
		return FALSE;

	/* Check for ASSOC OR AUTH State
	 * If open mode, check for SCB_ASSOCIATED
	 * If wsec enabled, check for SCB_AUTHORIZED.
	 *
	 * Make sure 802.1X exchange is over before CFP is turned ON.
	 * XXX FIXME Check if SCB state changes during key rotation after the join
	 * Make sure 802.1X exchanges as part of key rotations also skip CFP path
	 */
	assoc_and_auth = (bsscfg->WPA_auth != WPA_AUTH_DISABLED &&
		WSEC_ENABLED(bsscfg->wsec)) ? SCB_AUTHORIZED(scb) : SCB_ASSOCIATED(scb);

	if (!assoc_and_auth)
		return FALSE;

	/* No Fast path for wds, non qos, non ampdu stas */
	if (SCB_A4_DATA(scb) || !SCB_QOS(scb) || !SCB_WME(scb) ||
		!SCB_AMPDU(scb)) {
		return FALSE;
	}

	/* Derive Key info */
	wlc_keymgmt_get_tx_key(wlc->keymgmt, scb, bsscfg, &key_info);

	/* CFP allowed only in open and AES mode */
	if (!WLC_KEY_ALLOWS_PKTC(&key_info, bsscfg))
		return FALSE;

	/* Is there an TWT Individual link active then no CFP */
	if (wlc_twt_scb_active(wlc->twti, scb)) {
		return FALSE;
	}

	/* Cached Flow Processing is allowed */
	WLC_CFP_DEBUG(("%s : CFP Allowed for scb %p\n", __FUNCTION__, scb));

	return TRUE;
}

/** Check if Cached Flow Processing in RX path is allowed for this SCB */
static bool
wlc_scb_cfp_rx_allowed(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	ASSERT(scb != NULL);
	BCM_REFERENCE(wlc);

	/* Initialize */
	bsscfg = SCB_BSSCFG(scb);
	wlc = bsscfg->wlc;

	/* User level control for all SCBs */
	if (!CFP_RCB_ENAB(cfp_info))
		return FALSE;

	if (MONITOR_ENAB(wlc))
		return FALSE;

#if defined(DONGLEBUILD)
	/* Check for RX header conversion */
	if (!HDR_CONV())
		return FALSE;
#endif /* DONGLEBUILD */

#ifdef MCAST_REGEN
	if (MCAST_REGEN_ENAB(bsscfg) && BSSCFG_STA(bsscfg))
		return FALSE;
#endif /* MCAST_REGEN */

	if (bsscfg->wlcif->if_flags & WLC_IF_PKT_80211) {
		/* Disable if interface expects 80211 frames */
		return FALSE;
	}

#ifdef L2_FILTER
	if (L2_FILTER_ENAB(wlc->pub)) {
		if (wlc_l2_filter_block_ping_enab(wlc, bsscfg))
			/* Disable cfp rx since packet filtering is enabled */
			return FALSE;
	}
#endif /* L2_FILTER */

	/* Cached Flow Processing is allowed */
	WLC_CFP_DEBUG(("%s : CFP RX Allowed for scb %p\n", __FUNCTION__, scb));

	return TRUE;
}

/** Check if Cached Flow Processing  in TX path is allowed for this SCB */
static bool
wlc_scb_cfp_tx_allowed(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	ASSERT(scb != NULL);

	/* Initialize */
	bsscfg = SCB_BSSCFG(scb);
	wlc = bsscfg->wlc;

	/* User level control for all SCBs */
	if (!CFP_TCB_ENAB(cfp_info))
		return FALSE;

#if defined(DONGLEBUILD)
	/* Make sure proptxstatus is enabled */
	if (!PROP_TXSTATUS_ENAB(wlc->pub))
		return FALSE;
#endif /* DONGLEBUILD */

	ASSERT(wlc->txc);

	/* TX Cache module available */
	if (!(WLC_TXC_ENAB(wlc) && TXC_CACHE_ENAB(wlc->txc)))
		return FALSE;

	/* Check for TX Fifo Detach Pending */
	if (wlc->txfifo_detach_pending) {
		return FALSE;
	}

	/* Check for TX Fifo Blocked */
	if (wlc->block_datafifo) {
		return FALSE;
	}

	/* Cached Flow Processing is allowed */
	WLC_CFP_DEBUG(("%s : CFP TX  Allowed for scb %p\n", __FUNCTION__, scb));

	return TRUE;
}

/** Disable CFP capabilities for the given SCB */
static void
wlc_scb_cfp_disable(wlc_cfp_info_t* cfp_info, struct scb *scb)
{

	/* Disable CFP TX */
	wlc_scb_cfp_tx_disable(cfp_info, scb);

	/* Disable CFP RX */
	wlc_scb_cfp_rx_disable(cfp_info, scb);

}

/** Disable RX Cached Flow Processing for given SCB.  */
static void
wlc_scb_cfp_rx_disable(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	scb_cfp_t	*scb_cfp;	/* Cubby */
	wlc_cfp_t	*cfp;

	ASSERT(scb != NULL);

	/* Initialize */
	cfp = WLC_CFP(cfp_info);
	ASSERT(cfp);

	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return;
	}

	/* Disable CFP capability */
	SCB_CFP_RX_DISABLE(scb_cfp);

	WLC_CFP_DEBUG(("%s: Disable CFP RX for scb %p\n", __FUNCTION__, scb));
}

/** Disable TX Cached Flow Processing for given SCB. */
static void
wlc_scb_cfp_tx_disable(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	scb_cfp_t	*scb_cfp;	/* Cubby */
	wlc_cfp_t	*cfp;

	ASSERT(scb != NULL);

	/* Initialize */
	cfp = WLC_CFP(cfp_info);
	ASSERT(cfp);

	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from tx cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return;
	}

	/* Disable CFP capability */
	SCB_CFP_TX_DISABLE(scb_cfp);

	WLC_CFP_DEBUG(("%s: Disable CFP TX for scb %p\n", __FUNCTION__, scb));
}

/** CFP RX capable scb.
 * Fillup RCB info
 * Enable CFP RX flags
 */
static void
wlc_scb_cfp_rx_enable(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	wlc_cfp_t	*cfp;
	scb_cfp_t 	*scb_cfp;	/* Cubby */
	wlc_info_t	* wlc;

	ASSERT(scb != NULL);

	/* Initialize */
	cfp = WLC_CFP(cfp_info);
	ASSERT(cfp);

	wlc = WLC_CFP_WLC(cfp);
	scb_cfp = SCB_CFP(cfp, scb);

	ASSERT(scb_cfp);		/* validate the cubby */
	ASSERT(wlc);

	/* Fillup RCB */
	wlc_scb_cfp_rcb_fillup(wlc, scb, scb_cfp);

	/* Enable the CFP finally */
	SCB_CFP_RX_ENABLE(scb_cfp);

	WLC_CFP_DEBUG(("%s: Enable CFP RX for scb  %p : ID  %d\n",
		__FUNCTION__, scb, SCB_CFP_FLOWID(scb_cfp)));
}

/** CFP TX capable scb.
 * Fillup TCb info
 * Enable CFP TX flags
 */
static void
wlc_scb_cfp_tx_enable(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	wlc_cfp_t	*cfp;
	scb_cfp_t 	*scb_cfp;	/* Cubby */
	wlc_info_t	* wlc;

	ASSERT(scb != NULL);

	/* Initialize  scb_cfp */
	cfp = WLC_CFP(cfp_info);
	ASSERT(cfp);

	wlc = WLC_CFP_WLC(cfp);
	scb_cfp = SCB_CFP(cfp, scb);

	ASSERT(scb_cfp);		/* validate the cubby */
	ASSERT(wlc);

	/* Fillup TCB */
	wlc_scb_cfp_tcb_fillup(wlc, scb, scb_cfp);

	/* Enable the CFP finally */
	SCB_CFP_TX_ENABLE(scb_cfp);

	WLC_CFP_DEBUG(("---------------%s: Enable CFP TX for scb  %p : ID  %d  ------\n",
		__FUNCTION__, scb, SCB_CFP_FLOWID(scb_cfp)));
}

/** Fillup CFP area shared by TCB and RCB */
static void
wlc_scb_cfp_fillup(wlc_cfp_info_t* cfp_info, struct scb *scb)
{
	scb_cfp_t 	*scb_cfp;	/* Cubby */
	wlc_info_t	* wlc;
	wlc_cfp_t	*cfp;

	wlc_key_t 	*key;		/* Algorithm specific */
	wlc_key_info_t 	key_info;	/* Keymanagement info */
	wlc_bsscfg_t 	*bsscfg;	/* bss config */

	cfp = WLC_CFP(cfp_info);
	ASSERT(cfp);
	ASSERT(scb);

	/* Initialize */
	wlc = WLC_CFP_WLC(cfp);
	scb_cfp = SCB_CFP(cfp, scb);
	bsscfg = SCB_BSSCFG(scb);

	/* Validate */
	ASSERT(wlc);
	ASSERT(scb_cfp);
	ASSERT(bsscfg);

	/* store bsscfg pointer */
	SCB_CFP_CFG(scb_cfp) = SCB_BSSCFG(scb);

	/* AMSDU Cubby info */
	SCB_CFP_AMSDU_CUBBY(scb_cfp) = GET_AMSDU_CUBBY(wlc, scb);

	/* Fillup keymanagement info in cpf cubby */
	key = wlc_keymgmt_get_tx_key(wlc->keymgmt, scb, bsscfg, &key_info);
	if (key_info.algo == CRYPTO_ALGO_OFF)
	        key = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt,
	                SCB_BSSCFG(scb), FALSE, &key_info);

	ASSERT(key);

	/* Fillup keymanagemnt related info for fast path */
	SCB_CFP_KEY(scb_cfp) = key;
	SCB_CFP_KEY_ALGO(scb_cfp) = key_info.algo;
	SCB_CFP_IV_LEN(scb_cfp) = key_info.iv_len;
	SCB_CFP_ICV_LEN(scb_cfp) = key_info.icv_len;

	WLC_CFP_DEBUG(("%s: Fillup cubby cmn  info  scb %p cfg %p amsdu cubby %p\n",
		__FUNCTION__, scb, SCB_CFP_CFG(scb_cfp), SCB_CFP_AMSDU_CUBBY(scb_cfp)));
}

/** Fillup RX params into cfp cubby */
static void
wlc_scb_cfp_rcb_fillup(wlc_info_t *wlc, struct scb * scb, scb_cfp_t *scb_cfp)
{
	/* Validate */
	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb_cfp);

	/* AMPDU TX cubby info */
	SCB_CFP_AMPDU_RX(scb_cfp) = GET_AMPDU_RX_CUBBY(wlc, scb);

	WLC_CFP_DEBUG(("%s : fillup RX cubby info :ampdu rx %p\n\n",
		__FUNCTION__,  SCB_CFP_AMPDU_RX(scb_cfp)));

}

/** Fillup TX params into cfp cubby */
static void
wlc_scb_cfp_tcb_fillup(wlc_info_t *wlc, struct scb * scb, scb_cfp_t *scb_cfp)
{
	/* Validate */
	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb_cfp);

	/* AMPDU TX cubby info */
	SCB_CFP_AMPDU_TX(scb_cfp) = GET_AMPDU_TX_CUBBY(wlc, scb);

	WLC_CFP_DEBUG(("%s : fillup tx cubby info : ampdu tx %p\n\n",
		__FUNCTION__, SCB_CFP_AMPDU_TX(scb_cfp)));
}

/** CFP watchdog handler */
static void
wlc_cfp_watchdog(void *ctx)
{
	wlc_cfp_t *cfp; /* CFP module */

	ASSERT(ctx != NULL);
	cfp = (wlc_cfp_t *)ctx;

	BCM_REFERENCE(cfp);
}

/** CFP iovar handler */
static int
wlc_cfp_doiovar(void *ctx, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_cfp_t *cfp; /* CFP module */
	wlc_cfp_info_t *cfp_info; /* CFP module public */
	wlc_cfp_priv_t *cfp_priv; /* CFP module private */
	wlc_info_t *wlc; /* WLC module */
	scb_cfp_t *scb_cfp; /* SCB CFP Cubby */

	int32 *ret_int_ptr;
	bool bool_val;
	int32 int_val = 0;
	int err = BCME_OK;
	struct bcmstrbuf b;

	ASSERT(ctx != NULL);
	cfp = (wlc_cfp_t *)ctx;
	cfp_info = WLC_CFP_INFO(cfp);
	cfp_priv = WLC_CFP_PRIV(cfp);
	wlc = WLC_CFP_WLC(cfp);

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(alen);
	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(cfp_priv);
	BCM_REFERENCE(bool_val);
	BCM_REFERENCE(scb_cfp);
	BCM_REFERENCE(wlc);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* all iovars require mcnx being enabled */
	switch (actionid) {
	case IOV_GVAL(IOV_CFP_TCB):
		*ret_int_ptr = (int32)CFP_TCB_ENAB(cfp_info);
		break;
	case IOV_SVAL(IOV_CFP_TCB):
		CFP_TCB_ENAB(cfp_info) = bool_val;
		/* Update scb cfp state */
		wlc_cfp_state_update(cfp_info);
		break;
	case IOV_GVAL(IOV_CFP_RCB):
		*ret_int_ptr = (int32)CFP_RCB_ENAB(cfp_info);
		break;
	case IOV_SVAL(IOV_CFP_RCB):
		CFP_RCB_ENAB(cfp_info) = bool_val;
		/* Update scb cfp state */
		wlc_cfp_state_update(cfp_info);
		break;
	case IOV_GVAL(IOV_CFP_DUMP_ID):
	{
		uint16 cfp_flowid;
		scb_val_t *val_tmp = (scb_val_t *)p;

		if (plen < (int)sizeof(scb_val_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		cfp_flowid = wlc_get_cfp_flowid(wlc, &val_tmp->ea);
		bcm_binit(&b, a, alen);

		/* Dump cubby state for ID */
		err = wlc_cfp_scb_dump_id(cfp, cfp_flowid, &b);
		break;
	}
	case IOV_SVAL(IOV_CFP_TCB_INV):
	{
		err = wlc_cfp_tcb_cache_invalidate(wlc, (int16)int_val);
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	if (!IOV_ISSET(actionid))
		return err;

	return err;
}

/** SCB CFP cubby dump utility */
static void
wlc_cfp_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	uint8 prio;
	wlc_cfp_t *cfp; /* CFP module */
	scb_cfp_t *scb_cfp;
	wlc_cfp_info_t * cfp_info;

	ASSERT(ctx != NULL);

	/* Initialization */
	cfp = (wlc_cfp_t *)ctx;
	cfp_info = WLC_CFP_INFO(cfp);
	scb_cfp = SCB_CFP(cfp, scb);

	ASSERT(scb_cfp);

	/* Dump cubby info */
	bcm_bprintf(b, "\tFlowID	:: %d -  User ctrl :: TCB(%d)  RCB(%d) \n",
		SCB_CFP_FLOWID(scb_cfp), CFP_TCB_ENAB(cfp_info),
		CFP_RCB_ENAB(cfp_info));

	bcm_bprintf(b, "\tTX Enable :: %d  RX Enable  :: %d \n",
		SCB_CFP_TX_ENABLED(scb_cfp) ? 1 : 0,
		SCB_CFP_RX_ENABLED(scb_cfp) ? 1 : 0);

	bcm_bprintf(b, "\tkey Algo   :: %p\n", SCB_CFP_KEY_ALGO(scb_cfp));
	bcm_bprintf(b, "\tIV Len     :: %p\n", SCB_CFP_IV_LEN(scb_cfp));
	bcm_bprintf(b, "\tICV Len    :: %p\n", SCB_CFP_ICV_LEN(scb_cfp));
	bcm_bprintf(b, "\tTX cache ID:: %p\n", SCB_CFP_CACHE_GEN(scb_cfp));

#ifdef WLSQS
	bcm_bprintf(b, "\tFlow Ring IDs :: ");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%d ",
			(SCB_CFP_RINGID(scb_cfp, prio) == SCB_CFP_RINGID_INVALID) ? -1 :
			SCB_CFP_RINGID(scb_cfp, prio));
	}
	bcm_bprintf(b, "\n");
#endif /* WLSQS */
	bcm_bprintf(b, "\tTCB States :: ");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%d ", SCB_CFP_TCB_STATE(scb_cfp, prio));
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tRCB States :: ");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%d ", SCB_CFP_RCB_STATE(scb_cfp, prio));
	}

	/* Dump TCB info */
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCommon Q Pkts \t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%d ", scb_cfp->cq_cnt[prio]);
	}
	if (SCB_CFP_TX_ENABLED(scb_cfp)) {
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB Q Pkts \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%d ", CFP_SCB_QLEN(scb_cfp, prio));
		}
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCFP TX Pkts \t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_PKT_CNT(scb_cfp, prio));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCFP MU Pkt\t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_MUPKT_CNT(scb_cfp, prio));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tLegacy TX Pkts \t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_TX_LEGACYPKT_CNT(scb_cfp, prio));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCFP OOO Pkts \t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_SLOWPATH_CNT(scb_cfp, prio));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tTXq Wait for drain \t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_TXQ_DRAIN_WAIT(scb_cfp, prio));
	}

	/* Dump RCB info */
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCFP RX Chained \t\t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_RX_CHAINED_CNT(scb_cfp, prio));
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "\tCFP RX UN Chained \t ::");
	for (prio = 0; prio < NUMPRIO; prio++) {
		bcm_bprintf(b, "%u ", SCB_CFP_RX_UN_CHAINED_CNT(scb_cfp, prio));
	}
	bcm_bprintf(b, "\n\n");
}

#if defined(BCMDBG)
/** Dump minimal stats for all cfp flows */
static int
wlc_cfp_dump(void *ctx, struct bcmstrbuf *b)
{
	scb_cfp_t	*scb_cfp;	/* Cubby pointer */
	uint16		idx;		/* Flowid iterator */
	int prio;
	wlc_cfp_t *wlc_cfp = (wlc_cfp_t *)ctx;
	BCM_REFERENCE(wlc_cfp);

	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(WLC_CFP_INFO(wlc_cfp)), idx, scb_cfp) {

		struct scb *scb;

		if (!scb_cfp)
			continue;

		scb = SCB_CFP_SCB(scb_cfp);

		if (SCB_INTERNAL(scb))
			continue;

		bcm_bprintf(b, "Link "MACF" [flowid: %d]",
			ETHERP_TO_MACF(&scb->ea), idx);

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tCFP TX Pkts \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", SCB_CFP_PKT_CNT(scb_cfp, prio));
		}

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tCFP MU Pkt\t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", SCB_CFP_MUPKT_CNT(scb_cfp, prio));
		}

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tLegacy TX Pkts \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", SCB_CFP_TX_LEGACYPKT_CNT(scb_cfp, prio));
		}

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tCFP RX Chained Pkts \t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", SCB_CFP_RX_CHAINED_CNT(scb_cfp, prio));
		}

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tRX Unchained Pkts \t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", SCB_CFP_RX_UN_CHAINED_CNT(scb_cfp, prio));
		}
		bcm_bprintf(b, "\n");
	}
	return 0;
}

/** Clear stats for all CFP flows */
static int
wlc_cfp_dump_clr(void *ctx)
{
	scb_cfp_t	*scb_cfp;	/* Cubby pointer */
	uint16		idx;		/* Flowid iterator */
	wlc_cfp_t *wlc_cfp = (wlc_cfp_t *)ctx;

	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(WLC_CFP_INFO(wlc_cfp)), idx, scb_cfp) {
		if (scb_cfp)
			wlc_cfp_scb_clear_stats_id(wlc_cfp, idx);
	}

	return 0;
}
#ifdef WLSQS
/* SQS dump utility for SCB Q stats */
static int
wlc_cfp_scb_qstats_dump(void *ctx, struct bcmstrbuf *b)
{
	scb_cfp_t	*scb_cfp;	/* Cubby pointer */
	uint16		idx;		/* Flowid iterator */
	int prio;
	wlc_cfp_t *wlc_cfp = (wlc_cfp_t *)ctx;
	BCM_REFERENCE(wlc_cfp);

	/* Loop through each available flowid */
	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(WLC_CFP_INFO(wlc_cfp)), idx, scb_cfp) {

		struct scb *scb;

		if (!scb_cfp)
			continue;

		scb = SCB_CFP_SCB(scb_cfp);

		/* Skip internal SCBs for the dump */
		if (SCB_INTERNAL(scb))
			continue;

		bcm_bprintf(b, "Link "MACF" [flowid: %d]:", ETHERP_TO_MACF(&scb->ea), idx);

		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB Q Virtual Packets \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", wlc_sqs_vpkts(idx, prio));
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB Q V2R Packets \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", wlc_sqs_v2r_pkts(idx, prio));
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB Q Tbr Packets \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", wlc_sqs_tbr_pkts(idx, prio));
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB Q Real Packets \t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", wlc_sqs_n_pkts(idx, prio));
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\tSCB In transit \t\t\t ::");
		for (prio = 0; prio < NUMPRIO; prio++) {
			bcm_bprintf(b, "%u ", wlc_sqs_in_transit_pkts(idx, prio));
		}
		bcm_bprintf(b, "\n");
	}
	return 0;
}
#endif /* WLSQS */
#endif // endif

/** Update legacy TX counters for the given scb */
void
wlc_cfp_incr_legacy_tx_cnt(wlc_info_t *wlc, struct scb *scb, uint8 tid)
{
	scb_cfp_t	*scb_cfp = SCB_CFP(WLC_CFP(wlc->cfp), scb);

	/* Check for scb_cfp validity */
	if (scb_cfp)
		SCB_CFP_TX_LEGACYPKT_CNT(scb_cfp, tid)++;
}

/** Get CFP flowid for a matching ea */
static int
wlc_get_cfp_flowid(wlc_info_t *wlc, struct ether_addr *ea)
{
	scb_cfp_t	*scb_cfp;	/* Cubby pointer */
	uint16		idx;		/* Flowid iterator */

	FOREACH_CFP_FLOWID_G(WLC_UNIT(wlc), idx, scb_cfp) {
		if (scb_cfp) {
			struct scb *scb = SCB_CFP_SCB(scb_cfp);
			if (!memcmp(&ea->octet, &scb->ea.octet, ETHER_ADDR_LEN)) {
				return idx;
			}
		}
	}

	return -1;
}

/** Dump scb cubby given an CFP flowid. Strictly Debug only */
static int
wlc_cfp_scb_dump_id(wlc_cfp_t *cfp, uint16 flowid, struct bcmstrbuf *b)
{
	scb_cfp_t *scb_cfp;
	int cfp_unit;

	ASSERT(cfp != NULL);

	cfp_unit = WLC_CFP_UNIT(WLC_CFP_INFO(cfp));
	BCM_REFERENCE(cfp_unit);

	/* Check if valid flow id */
	if (!CFP_FLOWID_VALID(flowid)) {
		WLC_CFP_ERROR(("CFP flowid %d : Out of range\n", flowid));
		return BCME_RANGE;
	}

	/* Extract cubby pointer */
	scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, flowid);

	if (scb_cfp) {
		/* dump the state */
		wlc_cfp_scb_dump(cfp, SCB_CFP_SCB(scb_cfp), b);
	} else {
		WLC_CFP_ERROR(("CFP flowid %d : Not yet alloced\n", flowid));
	}

	return BCME_OK;
}

#if defined(BCMDBG)
/** Given a CFP flowid, clear packet stats */
static int
wlc_cfp_scb_clear_stats_id(wlc_cfp_t *cfp, uint16 flowid)
{
	scb_cfp_t *scb_cfp;
	scb_cfp_stats_t *stats;
	int cfp_unit;

	ASSERT(cfp != NULL);

	cfp_unit = WLC_CFP_UNIT(WLC_CFP_INFO(cfp));
	BCM_REFERENCE(cfp_unit);

	/* Check if valid flow id */
	if (!CFP_FLOWID_VALID(flowid)) {
		WLC_CFP_ERROR(("CFP flowid %d : Out of range\n", flowid));
		return BCME_RANGE;
	}

	/* Extract cubby pointer */
	scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, flowid);

	if (scb_cfp) {
		/* clear the packet stats */
		stats = &SCB_CFP_STATS(scb_cfp);
		memset((void*)stats, 0, sizeof(scb_cfp_stats_t));
	} else {
		WLC_CFP_ERROR(("CFP flowid %d : Not yet alloced\n", flowid));
	}

	return BCME_OK;
}
#endif // endif

/** Return if TCB state is established
 * Used by AMPDU layer [ampdu_tx_eval] to decide to release
 * through slow path or fast path. Different entry points possible
 * 1. wlc_ampdu_ini_move_window_aqm
 * 2. wlc_ampdu_recv_addba_resp
 * 3. wlc_ampdu_watchdog
 * 4. wlc_send_bar_complete
 * 5. wlc_ampdu_dotxstatus_aqm_complete
 *
 */
bool
wlc_cfp_tcb_is_EST(wlc_info_t *wlc, struct scb *scb, uint8 prio, scb_cfp_t **scb_cfp_out)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t 	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* SCB cubby */

	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* Can be called from scb cubby deinit context.
	 * So scb_cfp can be NULL then. Return FALSE
	 * in such case so that it does not take CFP path
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return FALSE;
	}

	/* Return CFP cubby pointer */
	if (scb_cfp_out)
		*scb_cfp_out = scb_cfp;

	return (SCB_CFP_TX_ENABLED(scb_cfp) ?
		(SCB_CFP_TCB_IS_EST(scb_cfp, prio)) : FALSE);
}

/**
 * Exported function to change per SCB CFP TCB state
 * Should be tied to ini creation and deletion
 */
void
wlc_cfp_tcb_upd_ini_state(wlc_info_t *wlc, struct scb *scb, int prio, bool valid)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t 	*cfp;		/* Module */
	scb_cfp_t 	*scb_cfp;	/* Cubby */

	ASSERT(scb);
	ASSERT(wlc);

	WLC_CFP_DEBUG(("%s : Set TCB state : scb %p prio %d state %d\n",
		__FUNCTION__, scb, prio, valid));

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* This function will be called from ampdu_scb_cubby deinit
	 * context also. So scb_scp might be NULL since cfp Cubby
	 * deinit will be called before ampdu cubby deinit
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return;
	}

	/* Update the state */
	if (valid)
		SCB_CFP_TCB_SET_INI_VALID(scb_cfp, prio);
	else
		SCB_CFP_TCB_SET_INI_INVALID(scb_cfp, prio);
}

/**
 * Exported function to update cache state for given scb
 * Updated when
 * 1. new txc gets created
 * 2. cache becomes invalid for given scb
 */
void
wlc_cfp_tcb_upd_cache_state(wlc_info_t *wlc, struct scb *scb, bool valid)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t 	*cfp;		/* Module */
	scb_cfp_t 	*scb_cfp;	/* Cubby */

	ASSERT(scb);
	ASSERT(wlc);

	WLC_CFP_DEBUG(("%s : Set TCB state : scb %p state %d\n",
		__FUNCTION__, scb, valid));

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from tx cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return;
	}

	/* Update cache state for the scb */
	if (valid)
		SCB_CFP_TCB_SET_CACHE_VALID(scb_cfp);
	else
		SCB_CFP_TCB_SET_CACHE_INVALID(scb_cfp);
}

/**
 * Exported function to update pause state for given scb
 * Updated by
 * 1. user iovar
 */
int
wlc_cfp_tcb_upd_pause_state(wlc_info_t *wlc, struct scb *scb, bool valid)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t 	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* Cubby */

	ASSERT(scb);
	ASSERT(wlc);

	WLC_CFP_DEBUG(("%s : Set TCB state : scb %p state %d\n",
		__FUNCTION__, scb, valid));

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from tx cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return BCME_OK;
	}

	/* Update cache state for the scb */
	if (valid)
		SCB_CFP_TCB_SET_PAUSE(scb_cfp);
	else
		SCB_CFP_TCB_RESET_PAUSE(scb_cfp);

	return BCME_OK;
}

/**
 * Exported Function to update TCB state
 *
 * Required when Transmit parameters of device changes like below
 * 1. RSDB
 * 2. Chanspec
 * 3. BF
 * 4. Frameburst
 * 5. TXcore/Txchain/Txant etc
 * 6. rspec overrides
 */
int
wlc_cfp_tcb_cache_invalidate(wlc_info_t *wlc, int16 flowid)
{
	scb_cfp_t 	*scb_cfp;	/* Cubby pointer */
	uint16 		idx;		/* Flowid iterator */
	int			cfp_unit;

	cfp_unit = WLC_UNIT(wlc);
	BCM_REFERENCE(cfp_unit);

	if (flowid == CFP_ALL_FLOWS) {
		WLC_CFP_DEBUG(("%s : Invalidate ALL TCB flows\n", __FUNCTION__));

		/* Invalidate all flows */
		FOREACH_CFP_FLOWID_G(cfp_unit, idx, scb_cfp) {
			/* Update the state */
			if (scb_cfp) {
				SCB_CFP_TCB_SET_CACHE_INVALID(scb_cfp);
			}
		}
	} else {
		/* Check if valid flow id */
		if (!CFP_FLOWID_VALID(flowid)) {
			WLC_CFP_ERROR(("CFP flowid %d : Out of range\n", flowid));
			return BCME_RANGE;
		}

		/* Extract cubby pointer */
		scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, flowid);

		/* Invalidate the cubby */
		if (scb_cfp) {
			SCB_CFP_TCB_SET_CACHE_INVALID(scb_cfp);
		} else {
			WLC_CFP_ERROR(("CFP flowid %d : not yet inited\n", flowid));
		}
	}

	return BCME_OK;
}

/**
 * Exported function to update pause state for given scb
 * Updated by
 * 1. user iovar
 */
int
wlc_cfp_rcb_upd_pause_state(wlc_info_t *wlc, struct scb *scb, bool valid)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* Cubby */

	ASSERT(scb);
	ASSERT(wlc);

	if (!CFP_ENAB(wlc->pub)) {
		return BCME_OK;
	}

	WLC_CFP_DEBUG(("%s : Set TCB state : scb %p state %d\n",
		__FUNCTION__, scb, valid));

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	if (!scb_cfp)
		return BCME_OK;

	/* Update cache state for the scb */
	if (valid)
		SCB_CFP_RCB_SET_PAUSE_ALL(scb_cfp);
	else
		SCB_CFP_RCB_RESET_PAUSE_ALL(scb_cfp);

	return BCME_OK;
}

/**
 * Exported function to change per SCB CFP RCB state
 *
 * Set to Valid when BA req comes in
 * Set to Invalid when AMPDU responder is cleaned
 */
void
wlc_cfp_rcb_upd_responder_state(wlc_info_t *wlc, struct scb *scb, int prio, bool valid)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* Cubby */

	ASSERT(scb);
	ASSERT(wlc);

	WLC_CFP_DEBUG(("%s : Set RCB state : scb %p prio %d state %d\n",
		__FUNCTION__, scb, prio, valid));

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	if (!scb_cfp)
		return;

	/* Update the state */
	if (valid)
		SCB_CFP_RCB_SET_RESP_VALID(scb_cfp, prio);
	else
		SCB_CFP_RCB_SET_RESP_INVALID(scb_cfp, prio);
}

/** Exported function to check if RCB is established for a fast path */
bool
wlc_cfp_rcb_is_EST(wlc_info_t *wlc, struct scb *scb, uint8 prio,
	scb_cfp_t **scb_cfp_out)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* SCB cubby */

	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	if (!scb_cfp)
		return FALSE;

	/* Return CFP cubby pointer */
	if (scb_cfp_out)
		*scb_cfp_out = scb_cfp;

	return (SCB_CFP_RX_ENABLED(scb_cfp) ?
		(SCB_CFP_RCB_IS_EST(scb_cfp, prio)) : FALSE);
}

/**
 * Link a bus layer flow with CFP flow. Return
 * 1. cfp flow ID
 * 2. Pointer to TCB state
 */
int
wlc_scb_cfp_tcb_link(wlc_info_t *wlc, uint16 ringid, uint8 tid,
	struct scb *scb, uint8** tcb_state, uint16* cfp_flowid)
{
	wlc_cfp_info_t *cfp_info; /* CFP public info */
	scb_cfp_t *scb_cfp; /* SCB CFP cubby */
	wlc_cfp_t *cfp; /* CFP Module */

	ASSERT(scb);
	ASSERT(wlc);

	/* Initialilze */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);

	ASSERT(cfp);

	/* continue Initialization */
	scb_cfp = SCB_CFP(cfp, scb);

	/* Return the flow id */
	if (scb_cfp) {
		*cfp_flowid = SCB_CFP_FLOWID(scb_cfp);
		if (tcb_state)
			*tcb_state = SCB_CFP_TCB_STATE_PTR(scb_cfp);

		WLC_CFP_DEBUG(("CFP Linkup with Bus layer : scb %p scb_cfp %p CFP flowid %d\n",
			scb, scb_cfp, *cfp_flowid));
#ifdef WLSQS
		SCB_CFP_RINGID(scb_cfp, tid) = ringid;
#endif /* WLSQS */
		return BCME_OK;
	}

	return BCME_ERROR;
}

#if defined(BCA_HNDROUTER) && defined(PKTC_TBL)
/**
 * Link CFP to the wfd->pktc layer (via the PT table).
 * In NIC mode, wfd->pktc is effectively the "bus" layer.
 *
 * This function is invoked, when the Linux bridge creates a new entry into
 * the PT table via PKTC_TBL_UPDATE.
 */
int
wlc_cfp_link_update(wlc_info_t *wlc, wlc_if_t *wlcif, uint8 *addr,
	uint16 *cfp_flowid)
{
	struct scb *scb;

	if (!CFP_ENAB(wlc->pub)) {
		return BCME_ERROR;
	}

	ASSERT(WLC_CFP_UNIT(wlc->cfp) == WLC_UNIT(wlc));

	*cfp_flowid = CFP_FLOWID_INVALID;

	if (ETHER_ISMULTI(addr) || ETHER_ISNULLADDR(addr)) {
		WLC_CFP_DEBUG(("CFP Linkup multi or NULL addr\n"));
		return BCME_ERROR;
	}

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, addr);

	if ((scb == NULL) || !(SCB_ASSOCIATED(scb) || SCB_LEGACY_WDS(scb))) {
		WLC_CFP_DEBUG(("CFP Linkup cannot find scb\n"));
		return BCME_ERROR;
	}

	/* tcb_state is not linked */
	return wlc_scb_cfp_tcb_link(wlc, 0, 0, scb, NULL, cfp_flowid);
}
#endif /* BCA_HNDROUTER && PKTC_TBL */

/** Accessors for SCB common queue counter block */
bool BCMFASTPATH
wlc_cfp_scb_cq_empty(wlc_info_t *wlc, struct scb *scb, uint8 prio)
{
	wlc_cfp_info_t *cfp_info; /* CFP public info */
	scb_cfp_t *scb_cfp; /* SCB CFP cubby */
	wlc_cfp_t *cfp; /* CFP Module */

	ASSERT(prio < NUMPRIO);
	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);

	ASSERT(cfp);

	/* Continue initialization */
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed as part of scb deinit
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CFP CUBBY already freed up for scb %p\n",
			__FUNCTION__, scb));
		return TRUE;
	}

	/* Return the SCB CQ counter block */
	return (scb_cfp->cq_cnt[prio] == 0);
}

void BCMFASTPATH
wlc_cfp_scb_cq_inc(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint32 cnt)
{
	wlc_cfp_info_t *cfp_info; /* CFP public info */
	scb_cfp_t *scb_cfp; /* SCB CFP cubby */
	wlc_cfp_t *cfp; /* CFP Module */

	ASSERT(prio < NUMPRIO);
	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);

	ASSERT(cfp);

	/* Continue initialization */
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed as part of scb deinit
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CFP CUBBY already freed up for scb %p\n",
			__FUNCTION__, scb));
		return;
	}
	scb_cfp->cq_cnt[prio] += cnt;
}

void BCMFASTPATH
wlc_cfp_scb_cq_dec(wlc_info_t *wlc, struct scb *scb, uint8 prio, uint32 cnt)
{
	wlc_cfp_info_t *cfp_info; /* CFP public info */
	scb_cfp_t *scb_cfp; /* SCB CFP cubby */
	wlc_cfp_t *cfp; /* CFP Module */

	ASSERT(prio < NUMPRIO);
	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);

	ASSERT(cfp);

	/* Continue initialization */
	scb_cfp = SCB_CFP(cfp, scb);

	/* check if cubby exists. It could have been
	 * already freed from tx cubby free context
	 */
	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n",
			__FUNCTION__, scb));
		return;
	}

	ASSERT(scb_cfp->cq_cnt[prio] >= cnt);
	scb_cfp->cq_cnt[prio] -= cnt;
}

/** Miscellaneous Wireless CFP helper functions for ingress device|bus layer. */

static inline uint32
_wlc_cfp_exptime(wlc_info_t *wlc)
{
#if !defined(DONGLEBUILD)
	return wlc_lifetime_now(wlc);
#else
	return R_REG(wlc->osh, D11_TSFTimerLow(wlc));
#endif // endif
}

#if !defined(DONGLEBUILD)
uint32
wlc_cfp_exptime(wlc_info_t *wlc)
{
	return _wlc_cfp_exptime(wlc);
}

/**
 * PKTLIST_FREE: Free a CFP packet list
 */
void
wlc_cfp_pktlist_free(wlc_info_t *wlc, void *p)
{
	void *prev;

	while (p != NULL) {
		prev = PKTLINK(p);
		PKTSETLINK(p, NULL);
		PKTFREE(wlc->osh, p, TRUE);
		p = prev;
	}
	return;
}
#endif /* !DONGLEBUILD */

bool /* Fetch wireless tx packet exptime. */
#if defined(DONGLEBUILD)
wlc_cfp_tx_enabled(int cfp_unit, uint16 cfp_flowid, uint32* cfp_exptime)
#else
wlc_cfp_tx_enabled(int cfp_unit, uint16 cfp_flowid, uint32 prio)
#endif // endif
{
	wlc_info_t	*wlc;
	wlc_hw_info_t	*wlc_hw;
	scb_cfp_t	*scb_cfp;	/* Cubby */

	wlc = WLC_CFP_PRIV(WLC_CFP_G(cfp_unit))->wlc;
	wlc_hw = wlc->hw;

	if (!CFP_ENAB(wlc->pub)) {
		return FALSE;
	}

	/* Check if fifo suspended, possible due to channel switch to DFS channel */
	if (wlc_hw->suspended_fifo_count) {
		return FALSE;
	}

	CFP_AUDIT_G(cfp_unit);

	/* Check if valid flow id */
	if (!CFP_FLOWID_VALID(cfp_flowid)) {
		return FALSE;
	}

	/* Extract cubby pointer */
	scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, cfp_flowid);

#if defined(DONGLEBUILD)
	ASSERT(scb_cfp);
#else /* !DONGLEBUILD */
	/* BCM_PKTFWD: Station might be disassociated after incarnation check. */
	if (scb_cfp == NULL) {
		return FALSE;
	}
#endif /* DONGLEBUILD */

#if defined(DONGLEBUILD)
	if (SCB_CFP_TX_ENABLED(scb_cfp))
#else
	if (SCB_CFP_TX_ENABLED(scb_cfp) && SCB_CFP_TCB_IS_EST(scb_cfp, prio))
#endif /* ! DONGLEBUILD */
	{
#if defined(DONGLEBUILD)
		*cfp_exptime = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
#endif // endif
		return TRUE;
	} else {
		return FALSE;
	}
}

void /** Prepare a CFP capable packet. */
wlc_cfp_pkt_prepare(int cfp_unit, uint16 cfp_flowid, uint8 prio,
	void *pkt, uint32 cfp_exptime)
{
	wlc_info_t *wlc;
	struct scb *scb;
	scb_cfp_t  *scb_cfp; /* SCB's CFP cubby */

	BCM_REFERENCE(wlc);
	ASSERT_CFP_FLOWID(cfp_flowid);
	CFP_AUDIT_G(cfp_unit);

	/* Fetch contexts from cfp_flowid */
	wlc = WLC_CFP_PRIV(WLC_CFP_G(cfp_unit))->wlc;
	ASSERT(wlc != NULL);

	scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, cfp_flowid);
	ASSERT(scb_cfp != NULL);

	scb = SCB_CFP_SCB(scb_cfp);
	ASSERT(scb != NULL);
	ASSERT(WLPKTTAG(pkt)->flags == 0);

	/* OSL To/From NATIVE packet accounting must be performed by caller... */
	WLPKTTAG(pkt)->_scb = scb;
	// WLPKTTAG(pkt)->u.frame_info.aid = scb->aid;
	WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(scb->bsscfg));
	WLPKTTAG(pkt)->flags |= (WLF_DATA | WLF_NON8023);

#if defined(PROP_TXSTATUS)
	/* Indicate its a host packet */
	WL_TXSTATUS_SET_FLAGS(WLPKTTAG(pkt)->wl_hdr_information, WLFC_PKTFLAG_PKTFROMHOST);
#endif // endif

	{
		uint32 lifetime;
		uint8 ac = SCB_WME(scb) ? WME_PRIO2AC(prio) : AC_BE;

		lifetime = wlc->lifetime[ac];
		if (lifetime != 0) {
			WLPKTTAG(pkt)->flags |= WLF_EXPTIME;
			WLPKTTAG(pkt)->u.exptime = cfp_exptime + lifetime;
		}
	}

	WLC_CFP_DEBUG((WLC_CFPMOD_FMT "scb %p\n",
		WLC_CFPMOD_ARG("PKTPREPARE"), scb));
}

void /* Wireless CFP capable Transmit fastpath entry point. */
wlc_cfp_tx_sendup(int cfp_unit, uint16 cfp_flowid, uint8 prio,
	void *pktlist_head, void *pktlist_tail, uint16 pkt_count)
{
	wlc_info_t *wlc;
	scb_cfp_t *scb_cfp; /* SCB CFP cubby */
	struct scb* scb;

	ASSERT_CFP_FLOWID(cfp_flowid);
	CFP_AUDIT_G(cfp_unit);

	wlc = WLC_CFP_WLC(WLC_CFP_G(cfp_unit));

	/* Fetch contexts from cfp_flowid */
	scb_cfp = SCB_CFP_ID_TO_PTR_G(cfp_unit, cfp_flowid);
#if defined(DONGLEBUILD)
	ASSERT(scb_cfp);
#else /* !DONGLEBUILD */
	/* BCM_PKTFWD: Station might be disassociated after incarnation check. */
	if (scb_cfp == NULL) {
		wlc_cfp_pktlist_free(wlc, pktlist_head);
		return;
	}
#endif /* DONGLEBUILD */
	scb = (struct scb*)SCB_CFP_SCB(scb_cfp);
	BCM_REFERENCE(scb);
	ASSERT(!SCB_DEL_IN_PROGRESS(scb) && !SCB_MARKED_DEL(scb));
	ASSERT(SCB_CFP_TCB_STATE(scb_cfp, prio) == CFP_TCB_STATE_ESTABLISHED);

	WLC_CFP_DEBUG((WLC_CFPMOD_FMT " pktlist[ h=0x%p t=0x%p l=%u ]\n",
		WLC_CFPMOD_ARG("SENDUP"), pktlist_head, pktlist_tail, pkt_count));

#ifdef WLSQS
	wlc_sqs_v2r_ampdu_sendup(wlc, prio, pktlist_head, pktlist_tail, pkt_count, scb_cfp);
#else
	/* Enqueue entire packet list into SCB's ampdu queue, procceed to release */
	wlc_cfp_ampdu_entry(wlc, prio, pktlist_head, pktlist_tail, pkt_count, scb_cfp);
#endif // endif
}

/**
 * Link AMT A2[Transmitter] index with CFP flow ID
 *
 * Loop through available CFP flows to do address comparison.
 * Bind CFP and AMT flow ids if address is found.
 */
void
wlc_cfp_amt_link_reinit(wlc_info_t *wlc, int amt_idx,
	const struct ether_addr *amt_addr)
{
	scb_cfp_t	*scb_cfp;	/* Cubby pointer */
	uint16		cfp_flowid;	/* Flowid iterator */
	struct scb	*scb;		/* SCB */
	bool found;
#ifdef CFP_DEBUG
	char addr_str[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */
	int cfp_unit;

	ASSERT_AMT_IDX(wlc, amt_idx);

	/* Initialize */
	found = FALSE;
	BCM_REFERENCE(found);

	WLC_CFP_DEBUG(("CFP AMT LINKUP :: amtid %d wlc bandunit %d \n",
		amt_idx, wlc->band->bandunit));

	cfp_unit = WLC_UNIT(wlc);
	BCM_REFERENCE(cfp_unit);

	FOREACH_CFP_FLOWID_G(cfp_unit, cfp_flowid, scb_cfp) {
		if (!scb_cfp)
			continue;

		scb = (struct scb*)SCB_CFP_SCB(scb_cfp);

		/* Internal SCBs seem to end up with BC/MC address
		 * Dont use them for AMT linkups
		 */
		if (SCB_INTERNAL(scb))
			continue;

		WLC_CFP_DEBUG(("idx %d scb_cfp %p scb %p  AMT ea %s scb ea %s  scb band unit %d \n",
			cfp_flowid, scb_cfp, scb,
			bcm_ether_ntoa(amt_addr, addr_str),
			bcm_ether_ntoa(&scb->ea, addr_str), scb->bandunit));

		if ((eacmp((const char*)amt_addr, (const char*)&scb->ea) == 0) &&
			(wlc->band->bandunit == scb->bandunit))
		{
			/* Check for unique Transmitter Address.
			 *
			 * Assumption is every AMT TA would map to unique scb.
			 * If there are multiple scbs poitning to same TA,
			 * whole flow lookup based on AMT fails.
			 *
			 */
			ASSERT(found == FALSE);
			found = TRUE;

			WLC_CFP_DEBUG(("%s : Found CFP entry ::::: amt idx %d cfp flow id %d "
				"addr %s current tbl entry %d \n", __FUNCTION__,
				amt_idx, cfp_flowid, bcm_ether_ntoa(amt_addr, addr_str),
				WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx)));

			ASSERT(WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) == CFP_FLOWID_INVALID);
			WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) = cfp_flowid;
		}
	}
}

/**
 * De-Link CFP-AMT
 *
 * Reset AMT lookup table entry for given AMT index
 * For a given cfp flowid loop through amt lookup table and delink
 */
void
wlc_cfp_amt_flowid_delink(wlc_info_t *wlc, int amt_idx, uint16 cfp_flowid)
{
	uint16 cfp_id;
	int cfp_unit;

	WLC_CFP_DEBUG(("%s : Delink AMT idx %d \n",
		__FUNCTION__, amt_idx));

	cfp_unit = WLC_UNIT(wlc);
	BCM_REFERENCE(cfp_unit);

	if (amt_idx == AMT_FLOWID_INVALID(wlc->pub->corerev)) {
		int amt_size;
		/* Triggered from a CFP cubby deinit */
		/* Search for the given cfp_flowid and delink */
		ASSERT_CFP_FLOWID(cfp_flowid);

		amt_size = AMT_SIZE(wlc->pub->corerev);
		FOREACH_AMT_IDX_G(cfp_unit, amt_size, amt_idx, cfp_id) {
			if (cfp_id == cfp_flowid) {
				WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) = CFP_FLOWID_INVALID;
			}
		}
	} else {
		/* Triggered from AMT delink */
		ASSERT_AMT_IDX(wlc, amt_idx);
		ASSERT(cfp_flowid == CFP_FLOWID_INVALID);
		WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) = CFP_FLOWID_INVALID;
	}
}

/** Return Linked CFP flowid for given amt idx */
uint16
wlc_cfp_amt_linkid_get(wlc_info_t *wlc, int amt_idx)
{
	if (AMT_IDX_VALID(wlc, amt_idx))
		return WLC_CFP_AMT_FLOWID_G(WLC_UNIT(wlc), amt_idx);
	else
		return CFP_FLOWID_INVALID;
}

/** Link CFP & AMT flow IDs. */
void
wlc_cfp_amt_linkid_set(wlc_info_t *wlc, int amt_idx, uint16 cfp_flowid)
{
	int cfp_unit;
	uint16 cur_amt_idx_cfp_flowid;

	/* Sanity Checks */
	ASSERT(AMT_IDX_VALID(wlc, amt_idx));
	ASSERT_CFP_FLOWID(cfp_flowid);

	cfp_unit = WLC_UNIT(wlc);
	BCM_REFERENCE(cfp_unit);

	cur_amt_idx_cfp_flowid = WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx);

	if (cur_amt_idx_cfp_flowid != CFP_FLOWID_INVALID) {
		/* AMT Link already present
		 *
		 * This should be a case of roaming across bands.
		 * While roaming across the bands, previous SCB/AMT is retained
		 * unitl new link is up. So atleast for a brief amount of time
		 * there are two SCBS with same ethernet address.
		 *
		 * Make sure the new link corresponds to a different band/CFP flow.
		 */

		scb_cfp_t * scb_cfp;
		struct scb	*scb;		/* SCB */

		BCM_REFERENCE(scb);
		scb_cfp = WLC_AMT_ID_TO_CFP_PTR_G(cfp_unit, amt_idx);
		scb = (struct scb*)SCB_CFP_SCB(scb_cfp);

		ASSERT(scb->bandunit != wlc->band->bandunit);
		ASSERT(cur_amt_idx_cfp_flowid != cfp_flowid);

		WLC_CFP_DEBUG(("%s : Roaming case. Previous flowid %d SCB %p band unit %d \n",
			__FUNCTION__, cur_amt_idx_cfp_flowid, scb, scb->bandunit));

		return;
	}

	/* Setup the link */
	WLC_CFP_AMT_FLOWID_G(cfp_unit, amt_idx) = cfp_flowid;
}

/** Lookup scb_cfp for a given amt index */
static scb_cfp_t *
wlc_cfp_amt_id_lookup(wlc_info_t *wlc, uint16 amt_idx)
{
	scb_cfp_t * scb_cfp;
	if (amt_idx >=  AMT_SIZE(wlc->pub->corerev))
		return NULL;

	scb_cfp = WLC_AMT_ID_TO_CFP_PTR_G(WLC_UNIT(wlc), amt_idx);

	return (SCB_CFP_RX_ENABLED(scb_cfp) ?
		scb_cfp : NULL);
}

/*
 * Check Frame headers to see if its chainable.
 *
 * Every where where PKTCLASS field informs its not similar to previous frame,
 * go through the header decode and verification to tag as chainable or not.
 * In 1 interrupt context every scb-tid will get one chance to tag as chainable.
 * On any frame failure to pass checks, capability is disabled for that interrupt context
 */
static bool
wlc_cfp_rxframe_hdr_chainable(wlc_info_t *wlc, scb_cfp_t *scb_cfp, struct dot11_header *h)
{
	wlc_bsscfg_t * bsscfg;
	uint8 *da;
	uint16 fc;
	bool chainable;

	/* Initialize */
	bsscfg = SCB_CFP_CFG(scb_cfp);
	fc = ltoh16(h->fc);
	chainable = TRUE;

	/* XXX FIXME, check for scb->bsscfg->pm->PM == PM_OFF scb->bsscfg->pm->PM
	 * in control path. Make sure cfp is disabled under that condition
	 */

	da = (uchar *)(BSSCFG_STA(bsscfg) ? &h->a1 : &h->a3);

	chainable &= ((FC_TYPE(fc) == FC_TYPE_DATA) &	/* DATA Frame */
		((fc & (FC_TODS | FC_FROMDS)) != (FC_TODS | FC_FROMDS)) & /* Non-WDS */
		(!ETHER_ISNULLDEST(da)) &	/* NUll address */
		(!ETHER_ISMULTI(da)) &		/* Multicast address */
		(FC_SUBTYPE_ANY_QOS((fc & FC_SUBTYPE_MASK) >>
			FC_SUBTYPE_SHIFT)) & /* QoS Frame */
		((fc & FC_RETRY) == 0) &	/* Retried Frame */
		(!FC_SUBTYPE_ANY_NULL((fc & FC_SUBTYPE_MASK) >>
			FC_SUBTYPE_SHIFT))); /* Null frame */

#if !(defined(HWA_REVISION_GE_131) && HWA_REVISION_GE_131) && \
	!defined(WAR_HWA2A_SW_MONITOR)
	chainable = (chainable && !eacmp(&h->a1, &bsscfg->cur_etheraddr));
#endif /* !HWA_REVISION_GE_131 && !WAR_HWA2A_SW_MONITOR */

	return chainable;
}

#if defined(DONGLEBUILD)
/** Populate and chain rx completion info */
static void
wl_cfp_rxcmpl_fast(wl_info_t *wl, void *p, uint8 index, uint32 *pkt_cnt)
{

	uint16	msdu_count = 0;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	uint16	amsdu_bytes = 0;
	void	* head = p;
#endif /* BCMDBG || BCMDBG_AMSDU */

	/* Loop through AMSDU sub frames if any */
	for (; p; p = PKTNEXT(wl->pub->osh, p)) {
		rxcpl_info_t *p_rxcpl_info = bcm_id2rxcplinfo(PKTRXCPLID(wl->pub->osh, p));

		ASSERT(p_rxcpl_info);

		BCM_RXCPL_CLR_IN_TRANSIT(p_rxcpl_info);
		BCM_RXCPL_SET_VALID_INFO(p_rxcpl_info);

		msdu_count++;

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
		amsdu_bytes += (uint16)PKTFRAGUSEDLEN(wl->pub->osh, p);
#endif /* BCMDBG || BCMDBG_AMSDU */

		/* fill up rxcl info */
		p_rxcpl_info->host_pktref = PKTFRAGPKTID(wl->pub->osh, p);
		p_rxcpl_info->rxcpl_len.dataoffset =
		    ((wlc_info_t *)wl->wlc)->datafiforxoff + HW_HDR_CONV_PAD;
		p_rxcpl_info->rxcpl_len.datalen = PKTFRAGUSEDLEN(wl->pub->osh, p);
		p_rxcpl_info->rxcpl_id.ifidx = index;
		p_rxcpl_info->rxcpl_id.dot11 = (PKT80211(p)) ? 1 : 0;

		/* Chain Rx completions */
		wl_chain_rxcomplete_id_tail(&wl->rxcpl_list, PKTRXCPLID(wl->pub->osh, p));

		/* Reset Rx flags */
		PKTRESETRXCPLID(wl->pub->osh, p);
		PKTRESETRXFRAG(wl->pub->osh, p);
		PKTRESETHDRCONVTD(wl->pub->osh, p);
	}

	(*pkt_cnt) += msdu_count;

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	if (WLPKTTAG(head)->flags & WLF_HWAMSDU) {
		wlc_cfp_amsdu_rx_histogram_upd(wl->wlc, msdu_count, amsdu_bytes);
	}
#endif /* BCMDBG || BCMDBG_AMSDU */

}

#endif /* DONGLEBUILD */

/**
 * 4. CFP RX UN CHAINED SENDUP
 *
 * Slow path RX processing
 * Sendup each packet one by one.
 */
static void BCMFASTPATH
wlc_cfp_unchained_sendup(wlc_info_t *wlc, void * p)
{
	wlc_d11rxhdr_t *wrxh;

	/* Packet stats */
	WLCNTINCR(wlc->pub->_cnt->unchained);

	wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc->osh, p);

	if (!(IS_D11RXHDRSHORT(&wrxh->rxhdr, wlc->pub->corerev))) {
		/* compute the RSSI from d11rxhdr and record it in wlc_rxd11hr */
		phy_rssi_compute_rssi((phy_info_t *)WLC_PI(wlc), wrxh);
	}

	/* Slow Path RX processing */
	wlc_recv(wlc, p);
}

/**
 * 3. CFP RX CHAINED SENDUP
 *
 * Check for replay frames
 * Update, rspec, rssi, snr, packet stats
 * Update AMPDU dump counters
 * Fill up rx completion info and link them up
 * Free the packet finally
 *
 */

static void BCMFASTPATH
wlc_cfp_scb_chain_sendup(wlc_info_t *wlc, scb_cfp_t * scb_cfp, uint8 prio)
{
	void * p, *amsdu_next;
	struct pktq  *chained_q;	/* Peri SCB- prio Chained Q */
	wlc_d11rxhdr_t *wrxh;
	uint32 pkt_cnt;
	uint32 pkt_len, byte_count;
	struct scb * scb;
	wlc_bsscfg_t *bsscfg;
	d11rxhdr_t	*rxh;
	struct dot11_header *h;
	uint16 pad;
	uint8 *plcp;
	uint8 *plcp_orig;
	uint32 pending_pkt_cnt;
	wlc_rx_status_t toss_reason;
	int err;
	ratespec_t rspec;
	bool is_first_mpdu;
#if !defined(DONGLEBUILD)
	uint16 body_offset;
	wl_if_t *wlif;
#endif /* ! DONGLEBUILD */

	/* Initialize */
	h = NULL;
	pkt_cnt = 0;
	byte_count = 0;
	scb = SCB_CFP_SCB(scb_cfp);
	bsscfg = SCB_CFP_CFG(scb_cfp);
	toss_reason = WLC_RX_STS_TOSS_UNKNOWN;
	is_first_mpdu = FALSE;

#if !defined(DONGLEBUILD)
	wlif = SCB_WLCIFP(scb)->wlif;
#endif /* ! DONGLEBUILD */

	chained_q = SCB_CFP_RCB_CHAINEDQ(scb_cfp);

	/* Available pkts in scb Q */
	pending_pkt_cnt = pktqprec_n_pkts(chained_q, prio);

	BCM_REFERENCE(plcp);

	/* Drain chanied Q */
	while ((p = pktq_pdeq(chained_q, prio))) {
		/* Check for packet priority */
		ASSERT(prio == PKTPRIO(p));

		wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc->osh, p);

		/* dot11header from the head MSDU and ucode/phyrxstatus from last MSDU */
		pad = RXHDR_GET_PAD_LEN(&wrxh->rxhdr, wlc);

		/* If short rx status, assume start of an AMSDU chain. Walk the AMSDU chain
		 * to the end. Last frag has long rx status. Use that rx status to process
		 * AMSDU chain.
		 */
		if (IS_D11RXHDRSHORT(&wrxh->rxhdr, wlc->pub->corerev)) {
			/* When the first packet of AMSDU chain is with short rx status, the
			 * Rx status header of first packet is wrong that make code to look at
			 * wrong offsets and results in "scb" to be not found and that causes
			 * the NULL access. In order to find a long rx status in last frag, the
			 * next packet must be present.
			 */
			amsdu_next = PKTNEXT(wlc->osh, p);

			/* Next subframe should be always there for short rx status */
			if (amsdu_next == NULL) {
				WL_ERROR(("ERROR: AMSDU chain with short rx status "
					"has not next packet! pkt %p prio %d \n", p, prio));
				OSL_SYS_HALT();
			}

			while (amsdu_next) {
				PKTSETPRIO(amsdu_next, prio);
				WLPKTTAGSCBSET(amsdu_next, scb);
				wrxh = (wlc_d11rxhdr_t*) PKTDATA(wlc->osh, amsdu_next);
				amsdu_next = PKTNEXT(wlc->osh, amsdu_next);
			}
		}

		rxh = &wrxh->rxhdr;
		plcp = PKTPULL(wlc->osh, p, wlc->hwrxoff + pad);
		plcp_orig = plcp;

		/* Estimate rspec */
		rspec = wlc_recv_compute_rspec(wlc->pub->corerev, rxh, plcp);

		/* Save the rspec in pkttag */
		WLPKTTAG(p)->rspec = rspec;

#ifdef WL11N
		/* HT/VHT/HE-SIG-A start from plcp[4] in rev128 */
		plcp += D11_PHY_RXPLCP_OFF(wlc->pub->corerev);
		/* for first MPDU in an A-MPDU collect ratesel stats for antsel */
		if ((pkt_cnt == 0) && (plcp[0] | plcp[1] | plcp[2])) {
			wlc_scb_ratesel_upd_rxstats(wlc->wrsi, rspec, D11RXHDR_ACCESS_VAL(rxh,
				wlc->pub->corerev, RxStatus2));
			WLCNTSCBSET(scb->scb_stats.rx_rate, rspec);
			is_first_mpdu = TRUE;

#if defined(WL11AC) && defined(WL_MU_RX) && defined(WLCNT)
			if (MU_RX_ENAB(wlc)) {
				uint8 gid;

				/* In case of AMPDU, since PLCP is valid only on first MPDU in an
				 * AMPDU, a MU-PLCP marks the start of an MU-AMPDU, and end is
				 * determined my another SU/MU PLCP. Till this duration we count
				 * all MPDUs recieved as MU MPDUs.
				 */
				if (D11PPDU_FT(rxh, wlc->pub->corerev) == FT_VHT) {
					gid = wlc_vht_get_gid(plcp);
					if ((gid > VHT_SIGA1_GID_TO_AP) &&
					    (gid < VHT_SIGA1_GID_NOT_TO_AP)) {
						WLC_UPDATE_MURX_INPROG(wlc, TRUE);
					} else {
						WLC_UPDATE_MURX_INPROG(wlc, FALSE);
					}

					if (WLC_GET_MURX_INPROG(wlc))
						WLCNTINCR(wlc->pub->_cnt->rxmpdu_mu);
				} else {
					WLC_UPDATE_MURX_INPROG(wlc, FALSE);
				}
			}
#endif  /* defined(WL11AC) && defined(WL_MU_RX) && defined(WLCNT) */
		} else {
			is_first_mpdu = FALSE;
		}
#else
		WLCNTSCBSET(scb->scb_stats.rx_rate, rspec);
#endif /* WL11N */
		/* compute the RSSI from d11rxhdr and record it in wlc_rxd11hr */
		phy_rssi_compute_rssi((phy_info_t *)WLC_PI(wlc), wrxh);

		/* LQ stat update on head frame in the chain */
		if ((WLC_RX_LQ_SAMP_ENAB(scb)) && (wrxh->rssi != WLC_RSSI_INVALID)) {
			rx_lq_samp_t lq_samp;
			wlc_lq_sample(wlc, bsscfg, scb, wrxh, TRUE, &lq_samp);

			/* Only SDIO/USB proptxstatus report RSSI/SNR */
			if (!BCMPCIEDEV_ENAB()) {
				WLPKTTAG(p)->pktinfo.misc.rssi = lq_samp.rssi;
				WLPKTTAG(p)->pktinfo.misc.snr = lq_samp.snr;
			}
		}

#if defined(DONGLEBUILD)
		/* Get to dot11 header */
		h = (struct dot11_header *)PKTPULL(wlc->osh, p, D11_RXPLCP_LEN_GE128);
#else /* ! DONGLEBUILD */
		/* Get to dot11 header */
		h = (struct dot11_header *)PKTPULL(wlc->osh, p,
			D11_PHY_RXPLCP_LEN(wlc->pub->corerev));
#endif /* ! DONGLEBUILD */

		pkt_len = pkttotlen(wlc->osh, p);

		 /* Dont expect fragmented packets here in CFP path */
		ASSERT(!(ltoh16(h->seq) & FRAGNUM_MASK));
		ASSERT(!(ltoh16(h->fc) & FC_MOREFRAG));

#if !defined(DONGLEBUILD)
		body_offset = DOT11_A3_HDR_LEN + DOT11_QOS_LEN;

#if defined(WL11N)
		if ((D11PPDU_FT(rxh, wlc->pub->corerev) >= FT_HT) &&
			(h->fc & HTOL16(FC_ORDER))) {
			body_offset += DOT11_HTC_LEN;
		}
#endif /* WL11N */
#endif /* ! DONGLEBUILD */

		/* Key management layer processing */
		if ((h->fc & htol16(FC_WEP)) ||
		    (WSEC_ENABLED(bsscfg->wsec) && bsscfg->wsec_restrict)) {
			if (pkt_cnt)
				WLPKTTAG(p)->flags |= WLF_RX_PKTC_NOTFIRST;
			if (pending_pkt_cnt)
				WLPKTTAG(p)->flags |= WLF_RX_PKTC_NOTLAST;

			err = wlc_key_rx_mpdu(SCB_CFP_KEY(scb_cfp), p, rxh);

			/* clear pktc pkt tags regardless */
			WLPKTTAG(p)->flags &= ~(WLF_RX_PKTC_NOTFIRST |
				WLF_RX_PKTC_NOTLAST);

			if (err == BCME_OK) {
#if !defined(DONGLEBUILD)
				body_offset += SCB_CFP_IV_LEN(scb_cfp);  /* strip iv from body */
#endif /* ! DONGLEBUILD */
				WLCNTSCBINCR(scb->scb_stats.rx_decrypt_succeeds);
			} else {
				WLCNTSCBINCR(scb->scb_stats.rx_decrypt_failures);
				toss_reason = WLC_RX_STS_TOSS_DECRYPT_FAIL;
				goto toss;
			}
		}

		/* update cached seqctl */
		scb->seqctl[prio] = ltoh16(h->seq);

		/* Handle HTC field of 802.11 header if available */
		wlc_he_htc_recv(wlc, scb_cfp->scb, rxh, h);
		wlc_twt_rx_pkt_trigger(wlc->twti, scb_cfp->scb);

		/* Check for phy rxstatus valid
		 * PHY rx status valid only for last MPDU in AMPDU
		 */
#ifdef WLC_SW_DIVERSITY
		if (WLSWDIV_ENAB(wlc) && WLSWDIV_BSSCFG_SUPPORT(bsscfg)) {
			wlc_swdiv_rxpkt_recv(wlc->swdiv, scb, wrxh,
				RSPEC_ISCCK(WLPKTTAG(p)->rspec));
		}
#endif /* WLC_SW_DIVERSITY */

#ifdef WL_LEAKY_AP_STATS
		if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
			wlc_leakyapstats_pkt_stats_upd(wlc, bsscfg, scb->seqctl[rfc->prio],
				wlc_recover_tsf32(wlc, wrxh), WLPKTTAG(p)->rspec,
				wrxh->rssi, rfc->prio, pkt_len);
		}
#endif /* WL_LEAKY_AP_STATS */

#ifdef WL_MIMOPS_CFG
		if (WLC_MIMOPS_ENAB(wlc->pub)) {
			wlc_mimo_ps_cfg_t *mimo_ps_cfg = wlc_stf_mimo_ps_cfg_get(bsscfg);
			if (WLC_STF_LEARNING_ENABLED(mimo_ps_cfg))
				wlc_stf_update_mimo_ps_cfg_data(bsscfg, rspec);
		}
#endif /* WL_MIMOPS_CFG */

		/* AMPDU counters
		 * XXX broken with invalid frame type
		 */
		wlc_ampdu_update_rxcounters(wlc->ampdu_rx,
			D11PPDU_FT(rxh, wlc->pub->corerev),
			scb, plcp_orig, p, prio, rxh, (WLPKTTAG(p))->rspec, is_first_mpdu);
#ifdef STA
		if (BSSCFG_STA(bsscfg) && bsscfg->pm->pspoll_prd) {
			wlc_pm_update_rxdata(bsscfg, scb, h, prio);
		}
#endif // endif

#if (defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU) || defined(DUMP_MURX)))
		if (MU_RX_ENAB(wlc)) {
			wlc_murx_update_rxcounters(wlc->murx,
				D11PPDU_FT(rxh, wlc->pub->corerev), scb, plcp_orig);
		}
#endif // endif

#if defined(DONGLEBUILD)

		ASSERT(PKTISHDRCONVTD(wlc->osh, p));

#else /* ! DONGLEBUILD */
		/* RXSM4 mode (HW header conversion) is not supported in NIC mode.
		 * WLAN driver does 802.11 -> 802.3/Ethernet header conversion
		 */

		if (WLPKTTAG(p)->flags & WLF_HWAMSDU) {
			/* Strip MAC header, move to start of AMSDU body */
			PKTPULL(wlc->osh, p, body_offset);
			if (wlc_cfp_amsdu_deagg_hw(wlc->ami, p, &pkt_cnt, scb)) {
				toss_reason = WLC_RX_STS_TOSS_BAD_DEAGG_SF_LEN;
				goto toss;
			}
		} else { /* !WLF_HWAMSDU */

			struct ether_addr *sa, *da;
			struct ether_header *eh = NULL;
			struct dot11_llc_snap_header *lsh;

			/* Strip off FCS bytes */
			PKTFRAG_TRIM_TAILBYTES(wlc->osh, p, DOT11_FCS_LEN, TAIL_BYTES_TYPE_FCS);

			/* 802.11 -> 802.3/Ethernet header conversion */
			lsh = (struct dot11_llc_snap_header *)(PKTDATA(wlc->osh, p) + body_offset);

			if (rfc894chk(wlc, lsh)) {
				/* RFC894 */
				eh = (struct ether_header *)PKTPULL(wlc->osh, p,
					body_offset + DOT11_LLC_SNAP_HDR_LEN - ETHER_HDR_LEN);
			} else {
				/* RFC1042 */
				uint body_len = PKTLEN(wlc->osh, p) - body_offset
					+(uint16)PKTFRAGUSEDLEN(wlc->osh, p);
				eh = (struct ether_header *)PKTPULL(wlc->osh, p,
						body_offset - ETHER_HDR_LEN);
				eh->ether_type = HTON16((uint16)body_len);
			}

			/* For reference, DS bits on Data frames and addresses:
			 *
			 *         ToDS FromDS   a1    a2    a3    a4
			 * IBSS      0      0    DA    SA   BSSID  --
			 * To STA    0      1    DA   BSSID  SA    --
			 * To AP     1      0   BSSID  SA    DA    --
			 * WDS       1      1    RA    TA    DA    SA
			 */
			if ((h->fc & htol16(FC_TODS)) == 0) {
				da = &h->a1;
				sa = &h->a3;
			} else {
				da = &h->a3;
				sa = &h->a2;
			}

			ether_copy(sa, eh->ether_shost);
			ether_rcopy(da, eh->ether_dhost);

			pkt_cnt++;
		}
#endif /* ! DONGLEBUILD */

		/* Packet stats */
		byte_count += pkt_len;
		pending_pkt_cnt--;

		PKTCLRCHAINED(wlc->osh, p);
		PKTCCLRFLAGS(p);

#if defined(DONGLEBUILD)
		/* Prepare the rxcompletion to be sent to bus layer */
		wl_cfp_rxcmpl_fast(wlc->wl, p, bsscfg->wlcif->index, &pkt_cnt);
		PKTFREE(wlc->osh, p, FALSE);
#else /* ! DONGLEBUILD */
		wl_cfp_sendup(wlc->wl, wlif, p, scb_cfp->flowid);
#endif /* ! DONGLEBUILD */

		continue;
toss:
		/* Packet stats */
		pending_pkt_cnt--;

		/* Failed RX packet */
		WL_ERROR(("wl%d: %s, tossing; wlc: %p, h: %p, reason: %d\n",
			wlc->pub->unit, __FUNCTION__, wlc, h, toss_reason));
		wlc_log_unexpected_rx_frame_log_80211hdr(wlc, h, toss_reason);
#ifdef WL_RX_STALL
		wlc_rx_healthcheck_update_counters(wlc, WME_PRIO2AC(prio),
			scb, bsscfg, toss_reason, 1);
#endif // endif
		WLCNTINCR(wlc->pub->_wme_cnt->rx_failed[WME_PRIO2AC(prio)].packets);
		WLCNTADD(wlc->pub->_wme_cnt->rx_failed[WME_PRIO2AC(prio)].bytes,
			pkttotlen(wlc->osh, p));
		PKTFREE(wlc->osh, p, FALSE);
	}

	/* Make sure CFP RX Q is empty */
	ASSERT(pending_pkt_cnt == 0);

	SCB_CFP_RX_CHAINED_CNT(scb_cfp, prio) += pkt_cnt;
	/* update per-scb stat counters */
	WLCNTSCBADD(scb->scb_stats.rx_ucast_pkts, pkt_cnt);
	WLCNTSCBADD(scb->scb_stats.rx_ucast_bytes, byte_count);

#ifdef WL_RX_STALL
	wlc_rx_healthcheck_update_counters(wlc, prio,
		scb, bsscfg, WLC_RX_STS_OK, pkt_cnt);
#endif // endif

#ifdef PKTC_DONGLE
	/* for pktc dongle only */
	if (bsscfg->roam->time_since_bcn != 0) {
#ifdef EVENT_LOG_COMPILE
		EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG, "Beacon miss reset by RXC");
#endif // endif
		wlc_roam_update_time_since_bcn(bsscfg, 0);
	}
#endif /* PKTC_DONGLE */

	/* Update chain stats */
	WLCNTADD(wlc->pub->_cnt->chained, pkt_cnt);
	WLCNTSET(wlc->pub->_cnt->currchainsz, pkt_cnt);
	WLCNTSET(wlc->pub->_cnt->maxchainsz,
	         MAX(pkt_cnt, wlc->pub->_cnt->maxchainsz));

	WLCNTADD(wlc->pub->_cnt->rxframe, pkt_cnt);
	WLCNTADD(wlc->pub->_cnt->rxbyte, byte_count);
	WLCNTADD(wlc->pub->_wme_cnt->rx[WME_PRIO2AC(prio)].packets, pkt_cnt);
	WLCNTADD(wlc->pub->_wme_cnt->rx[WME_PRIO2AC(prio)].bytes, byte_count);
	WLPWRSAVERXFADD(wlc, pkt_cnt);

	/* update interface stat counters */
	WLCNTADD(bsscfg->wlcif->_cnt->rxframe, pkt_cnt);
	WLCNTADD(bsscfg->wlcif->_cnt->rxbyte, byte_count);

	scb->used = wlc->pub->now;

#ifdef WLWNM_AP
	/* update rx time stamp for BSS Max Idle Period */
	if (BSSCFG_AP(bsscfg) && WLWNM_ENAB(wlc->pub) && scb && SCB_ASSOCIATED(scb))
		wlc_wnm_rx_tstamp_update(wlc, scb);
#endif // endif
}

/**
 * 2. CFP RX SENDUP
 *
 * Loop through all pending packet lists.
 * Service chained from each scb.
 * chained packets take a fast path and latter takes a slower one.
 * Remove scbs from pending list once sendup is done.
 * Flush  tx completions to bus layer
 * Send unchainable packet.
 *
 */
INLINE void BCMFASTPATH
wlc_cfp_rx_sendup(wlc_info_t *wlc, void *p)
{
	dll_t *item, *next;
	scb_cfp_t * scb_cfp;
	scb_cfp_rcb_node_t * rcb_node;
	uint8 prio;
#ifdef BCMDBG
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	uint16 idx;
#endif /* BCMDBG */

	/* loop through pending list of scbs */
	for (item = dll_head_p(CFP_RCB_PEND_LIST(wlc->cfp));
		!dll_end(CFP_RCB_PEND_LIST(wlc->cfp), item); item = next) {

		next = dll_next_p(item);
		rcb_node = BCM_CONTAINER_OF(item, scb_cfp_rcb_node_t, node);
		scb_cfp = rcb_node->scb_cfp;
		prio = rcb_node->prio;

		/* Sendup Chained packets */
		wlc_cfp_scb_chain_sendup(wlc, scb_cfp, prio);

		/* Remove node from pending list */
		dll_delete(&(SCB_CFP_RCB_DLL_NODE(scb_cfp, prio)->node));
		SCB_CFP_RCB_NODE_IS_PENDING(scb_cfp, prio) =  FALSE;
	}

#ifdef BCMDBG
	cfp_info = wlc->cfp;
	BCM_REFERENCE(cfp_info);
	/* Audit for CFP RX Qs in Debug builds */
	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(cfp_info), idx, scb_cfp) {
		if (scb_cfp) {
			if (pktq_n_pkts_tot(SCB_CFP_RCB_CHAINEDQ(scb_cfp)) != 0) {
				WLC_CFP_ERROR(("ERROR:: Packets pending in CFP RX Q:"
					"scb cfp %p cnt %d \n",
					scb_cfp, pktq_n_pkts_tot(SCB_CFP_RCB_CHAINEDQ(scb_cfp))));
				ASSERT(0);
			}
		}
	}
#endif /* BCMDBG */

	/* Flush rx complete info to bus layer */
	wl_flush_rxreorderqeue_flow(wlc->wl, &(wlc->wl->rxcpl_list));

	if (p) {
		wlc_cfp_unchained_sendup(wlc, p);
	}

#if defined(BCM_PKTFWD)
	/* Flush packets accumulated in pktqueues. */
	wl_pktfwd_flush_pktqueues(wlc->wl);
#endif /* BCM_PKTFWD */

}

/**
 * 1. CFP RX FRAME
 * Process incoming packets to check if chainable.
 * Look at
 * 1. flowid
 * 2. pktclass
 * 3. filtermap
 * 4. amsdu deagg state
 * to decide the rx packet chaining.
 *
 * Enqueue to per prio scb cfp queues if chainable. If not
 * then release chain, and recv packet via legacy path.
 *
 */
INLINE void BCMFASTPATH
wlc_cfp_rxframe(wlc_info_t *wlc, void* p)
{
	uint16 flowid, pktclass;
	uint16 filtermap;
	wlc_d11rxhdr_t	*wrxh;
	d11rxhdr_t	*rxh;
	struct dot11_header *h;
	void *head_pkt;
	scb_cfp_t *scb_cfp = NULL;
	struct scb *scb = NULL;
	uchar	*pbody;
	uint16	prio, pad;
	uint16 seq, RxStatus1;
	bool amsdu;
	bool amsdu_control;
	bool chainable;
	bool pkt_similar;
	bool sf_chainabale;	/* sub frame chainable */
	uint16 min_frame_len;
#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
	uint32 udpv6_filter;
	uint32 udpv4_filter;
	uint32 tcp_filter;
#endif // endif

#ifdef CFP_DEBUG
	char addr_str1[ETHER_ADDR_STR_LEN];
	char addr_str2[ETHER_ADDR_STR_LEN];
	char addr_str3[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	ASSERT(PKTLINK(p) == NULL);
	ASSERT(PKTNEXT(wlc->osh, p) == NULL);

#if defined(DONGLEBUILD)
	/* Check if packet is HDR converted
	 * Packet should reach here only if its compiled for mode 4
	 * Now if HW header conversion fails in Mode-4, take slow path
	 */
	if (!PKTISHDRCONVTD(wlc->osh, p)) {
		wlc_cfp_rx_sendup(wlc, p);
		return;
	}
#endif /* DONGLEBUILD */

	/* Initialilze */
	h = NULL;
	sf_chainabale = chainable = TRUE;
	wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc->osh, p);
	rxh = &wrxh->rxhdr;

#if defined(DONGLEBUILD)
	/* Check for recieved fifo */
	ASSERT(D11RXHDR_GE128_ACCESS_VAL(rxh, fifo) == RX_FIFO1);

#else /* ! DONGLEBUILD */
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		/* Decode rxstatus */
		amsdu = RXHDR_GET_AMSDU(rxh, wlc);

		/* XXX: ucode doesn't provide "flowid" in RxStatus for corerev < 80
		 * Fetching scb_cfp afer AMSDU deagg, using D3LUT lookup for TA in D11
		 */
		//flowid = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, flowid);

		/* Get status from last packet (in case of AMSDU) as
		 * this is the one which is valid
		 */
		RxStatus1 = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, RxStatus1);

	} else
#endif /* ! DONGLEBUILD */
	{
		/* Decode rxstatus */
		amsdu = (D11RXHDR_GE128_ACCESS_VAL(rxh, mrxs) & RXSS_AMSDU_MASK);

		/* Flowid from AMT table */
		flowid = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, flowid));

		/* Get status from last packet (in case of AMSDU) as
		 * this is the one which is valid
		 */
		RxStatus1 = D11RXHDR_GE128_ACCESS_VAL(rxh, RxStatus1);

		/* Lookup CFP cubby from the AMT index */
		scb_cfp = wlc_cfp_amt_id_lookup(wlc, flowid);

		if (!scb_cfp) {
			/* Packet not enqueud; SCB not found.
			 * Release all chained packets and recv current packet.
			 */
			wlc_cfp_rx_sendup(wlc, p);
			return;
		}

		scb = SCB_CFP_SCB(scb_cfp);
	}

	/* AMSDU RX state management */
	if (amsdu) {
		/* check, qualify and chain MSDUs */
		head_pkt = wlc_cfp_recvamsdu(wlc->ami, wrxh, p, &sf_chainabale, scb);

		if (head_pkt != NULL) {
			/* amsdu deagg successful */
			p = head_pkt;
		} else {
			/* non-tail or out-of-order sub-frames */
			return;
		}

	} else {
		wlc_amsdu_flush(wlc->ami);
	}

	/* Only head frame in AMSDU should reach here */
	wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc->osh, p);
	rxh = &wrxh->rxhdr;
	pad = RXHDR_GET_PAD_LEN(rxh, wlc);
	min_frame_len = (uint)PKTC_MIN_FRMLEN(wlc) + pad;

	/* Read HWA 2.a rxstatus output */

	/* PKTCLASS from HWA 2.a */
#if (defined(HWA_REVISION_GE_131) && HWA_REVISION_GE_131) || \
	defined(WAR_HWA2A_SW_MONITOR)
	pktclass = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, pktclass));
#else /* ! (HWA_REVISION_GE_131 || WAR_HWA2A_SW_MONITOR) */
	pktclass = PKTCLASS_FAST_PATH_MASK;
#endif /* ! (HWA_REVISION_GE_131 || WAR_HWA2A_SW_MONITOR) */

#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
	/* Filtermap from HWA 2.a */
	filtermap = ltoh16(D11RXHDR_GE129_ACCESS_VAL(rxh, filtermap16));
#else /* ! (BCMHWA && HWA_RXDATA_BUILD) */
	filtermap = 0;
#endif /* ! (BCMHWA && HWA_RXDATA_BUILD) */

#if defined(DONGLEBUILD)
	/* Get to dot11 header */
	h = (struct dot11_header *)(((uint8*)(PKTDATA(wlc->osh, p))) +
		wlc->hwrxoff + pad +
		D11_RXPLCP_LEN_GE128);

	/* Only Data packets should reach here */
	ASSERT(FC_TYPE(ltoh16(h->fc)) == FC_TYPE_DATA);
	/* CFP RX doesnt handle Fragmented packets */
	ASSERT((ltoh16(h->fc) & FC_MOREFRAG) == 0);

#else /* ! DONGLEBUILD */

	/* Get to dot11 header */
	h = (struct dot11_header *)(((uint8*)(PKTDATA(wlc->osh, p))) +
		wlc->hwrxoff + pad + D11_PHY_RXPLCP_LEN(wlc->pub->corerev));

	if ((FC_TYPE(ltoh16(h->fc)) != FC_TYPE_DATA) ||
		((ltoh16(h->fc) & FC_MOREFRAG) != 0) || ((ltoh16(h->seq) & FRAGNUM_MASK) != 0)) {
		/** If received
		 * - Management/Control packet
		 * - Fragmented Data packet
		 * Release all chained packets and recv current packet.
		 */
		wlc_cfp_rx_sendup(wlc, p);
		return;
	}
#endif /* ! DONGLEBUILD */

#if defined(CFP_REVLT80_UCODE_WAR)
	if (D11REV_LT(wlc->pub->corerev, 128)) {
		uint16 cfp_flowid;

		/* Flowid from PKTFWD Lookup Table */
		cfp_flowid = wl_pktfwd_get_cfp_flowid(wlc->wl, (uint8 *)(&h->a2));

		/* Fetch contexts from cfp_flowid */
		if (cfp_flowid != ID16_INVALID)
			scb_cfp = SCB_CFP_ID_TO_PTR_G(WLC_UNIT(wlc), cfp_flowid);

		if (!scb_cfp || !SCB_CFP_RX_ENABLED(scb_cfp)) {
			/* Packet not enqueud; SCB not found.
			 * Release all chained packets and recv current packet.
			 */
			wlc_cfp_rx_sendup(wlc, p);
			return;
		}

#ifdef WLSCB_HISTO
		scb = SCB_CFP_SCB(scb_cfp);
		if (amsdu)
			WLSCB_HISTO_RX(scb, (WLPKTTAG(p))->rspec, RXHDR_GET_MSDU_COUNT(rxh, wlc));
#endif /* WLSCB_HISTO */

	}
#else /* ! CFP_REVLT80_UCODE_WAR */

	/* CFP is supported only for MAC rev >= 128 */
	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

#endif /* ! CFP_REVLT80_UCODE_WAR */

	/* Dump the address and Frame control */
#ifdef BCMDBG
	WLC_CFP_DEBUG(("pktclass %x filtermap %x \n ",
		pktclass, filtermap));
	WLC_CFP_DEBUG(("%s FC %x A1 %s A2 %s A3 %s \n",
		__FUNCTION__, h->fc,
		bcm_ether_ntoa(&h->a1, addr_str1),
		bcm_ether_ntoa(&h->a2, addr_str2),
		bcm_ether_ntoa(&h->a3, addr_str3)));
#endif // endif

#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
	/* LLC SNAP MAC address is known to be a sta side AMSDU corruption.
	 * So toss the frame and return.
	 */
	if ((filtermap) && (hwa_rxdata_fhr_is_llc_snap_da(filtermap))) {
		WLCNTINCR(wlc->pub->_cnt->rxbadda);

		WL_ERROR(("%s : pkt %p  Invalid MAC DA frame  filtermap %x  pkt corrupted  %d\n",
			__FUNCTION__, p, filtermap,
			PKTISRXCORRUPTED(wlc->osh, p)));

		/* Dump the packet */
		if (WL_ERROR_ON())
		prhex("Pkt dump", PKTDATA(wlc->osh, p), PKTLEN(wlc->osh, p));

		PKTFREE(wlc->osh, p, FALSE);
		return;
	}

	/* Filters for dynamic frameburst.
	 * Clear the filtermap after read to keep the packet flow through
	 * the CFP fast path.
	 */
	if (filtermap) {
		if ((udpv6_filter = hwa_rxdata_fhr_is_udpv6(filtermap)) != 0) {
			filtermap &= ~udpv6_filter;
			wlc->active_udpv6 = TRUE;
			if (wlc->active_bidir_thresh == ACTIVE_BIDIR_THRESH_AUTO) {
				wlc->active_bidir_thresh = ACTIVE_BIDIR_DFLT_THRESH;
				WL_ERROR(("Enabling dynamic frameburst\n"));
			}
			if (wlc->active_bidir_thresh > 0)
				wlc->active_bidir = TRUE;
		} else if ((udpv4_filter = hwa_rxdata_fhr_is_udpv4(filtermap)) != 0) {
			filtermap &= ~udpv4_filter;
			wlc->active_udpv4 = TRUE;
			if (wlc->active_bidir_thresh == ACTIVE_BIDIR_THRESH_AUTO) {
				wlc->active_bidir_thresh = ACTIVE_BIDIR_DFLT_THRESH;
				WL_ERROR(("Enabling dynamic frameburst\n"));
			}
			if (wlc->active_bidir_thresh > 0)
				wlc->active_bidir = TRUE;
		} else if ((tcp_filter = hwa_rxdata_fhr_is_tcp(filtermap)) != 0) {
			filtermap &= ~tcp_filter;
			wlc->active_tcp = TRUE;
			wlc->active_bidir = FALSE;
			wlc->bidir_countdown = 0;
		} else if ((wlc->active_bidir_thresh > 0) && !wlc->active_tcp &&
			!wlc->active_udpv4 && !wlc->active_udpv6) {
			wlc->active_bidir = TRUE;
			wlc->bidir_countdown = ACTIVE_BIDIR_LEARN_DELAY; /* learning time */
		}
	}
#endif /* BCMHWA && HWA_RXDATA_BUILD */

#if defined(DONGLEBUILD)
	/* All other RX corruption should dump and TRAP here for internal builds.
	 * Toss the frame and return on external builds.
	 */
	if (PKTISRXCORRUPTED(wlc->osh, p)) {
		WL_ERROR(("%s : Corrupted RX packet: Drop the frame %p\n",
			__FUNCTION__, p));
#ifdef RX_DEBUG_ASSERTS
		wlc_rx_invalid_length_handle(wlc, p, rxh);
#endif /* RX_DEBUG_ASSERTS */
		PKTFREE(wlc->osh, p, FALSE);
		return;
	}
#endif /* DONGLEBUILD */

	pbody = (uchar*)h + DOT11_A3_HDR_LEN;

	/* check for AMSDU contorl from QoS */
	if (amsdu) {
		amsdu_control = (((ltoh16_ua(pbody)) & QOS_AMSDU_MASK) != 0);
	} else {
		amsdu_control = TRUE;
	}

	/* Assume a non wds connection for now.
	 * CFP checks below would fail for WDS connection.
	 * Proceed in good faith that its non WDS connection for now
	 */
	prio = QOS_PRIO(ltoh16_ua(pbody));
	seq = ltoh16(h->seq);

	/* Check for decrypt error and decrypt attempted */
	if (h->fc & htol16(FC_WEP)) {
		/* Include IV and ICV length to min frame computation */
		min_frame_len += SCB_CFP_IV_LEN(scb_cfp) + SCB_CFP_ICV_LEN(scb_cfp);

		/* Decrypt error */
		chainable &= ((RxStatus1 & (RXS_DECATMPT | RXS_DECERR)) == RXS_DECATMPT);
	}

	/* Check if Packet is chainable
	 * 1. Filter hit
	 * 2. CFP RCB is established
	 * 3. AMPDU seq number in order
	 * 4. Got minimum pktlength
	 * 5. AMSDU subframe sare chainable
	 * 6. AMSDU control fields are proper.
	 */
	chainable &= ((filtermap == 0) &
		sf_chainabale &
		amsdu_control &
		(SCB_CFP_RCB_IS_EST(scb_cfp, prio)) &
		((PKTLEN(wlc->osh, p) - wlc->hwrxoff) > (min_frame_len)) &
		((ltoh16(h->fc) & FC_RETRY) == 0));

	/* Check for packet similarity */
	pkt_similar = ((pktclass & PKTCLASS_FAST_PATH_MASK) == 0);

	if (chainable && !pkt_similar) {
		/* If packet is not similar to the one before,
		 * go through full frame control and address check to make sure
		 * packet is allowed to take fast path.
		 *
		 * First frames of a flow typically comes with pktclass non zero.
		 * Do a full frame decode just for 1st frame to see if its still chainable.
		 */
		chainable = wlc_cfp_rxframe_hdr_chainable(wlc, scb_cfp, h);
	}

	/* make sure AMPDU seq number is checked at the last.
	 * AMPDU expected seq number is advanced here.
	 * So cant take an unchianed path after passing AMPDU seq check.
	 */
	chainable = (chainable &&
		wlc_cfp_ampdu_chainable(SCB_CFP_AMPDU_RX(scb_cfp), seq, prio));

	if (chainable) {
		/* Set the priority and SCB: For AMSDU subframes parameters are set in release fn */
		PKTSETPRIO(p, prio);
		WLPKTTAGSCBSET(p, scb);

		/* Fast path capable: Enqueue to per prio scb chained Q */
		pktq_penq(SCB_CFP_RCB_CHAINEDQ(scb_cfp), prio, p);
	} else {
		SCB_CFP_RX_UN_CHAINED_CNT(scb_cfp, prio)++;
		/* Packet not enqueud; packet is unchainable. Release chain, and sendup packet */
		wlc_cfp_rx_sendup(wlc, p);
		return;
	}

	WLC_CFP_DEBUG(("Total Enqueud pkts scb cfp %p cnt %d \n",
		scb_cfp, pktq_n_pkts_tot(SCB_CFP_RCB_CHAINEDQ(scb_cfp))));

	/* If RCB not yet in pending List. Add RCB to pending list */
	if (SCB_CFP_RCB_NODE_IS_PENDING(scb_cfp, prio) == FALSE) {
		dll_prepend(CFP_RCB_PEND_LIST(wlc->cfp),
			&(SCB_CFP_RCB_DLL_NODE(scb_cfp, prio)->node));

		/* Mark this SCB is now in pending list */
		SCB_CFP_RCB_NODE_IS_PENDING(scb_cfp, prio) =  TRUE;
	}
}

/**
 * 0. CFP RX ENTRY Function
 * slow path equivalent : wlc_bmac_recv()
 * 1. Retrieve packets from hnddma layer
 * 2. Refill packets to RX DMA
 * 3. Classiffy and aggregate packets across scbs.
 * 4. Sendup packets to bus layer
 */
bool BCMFASTPATH
wlc_cfp_bmac_recv(wlc_hw_info_t *wlc_hw, uint fifo, wlc_dpc_info_t *dpc)
{
	void *p;
	uint n = 0;
	uint32 tsf_l;
	d11rxhdr_t *rxh;
	wlc_d11rxhdr_t *wrxh;
	wlc_info_t *wlc = wlc_hw->wlc;
	uint bound_limit = wlc->pub->tunables->pktcbnd;

#ifdef WLC_RXFIFO_CNT_ENABLE
	wl_rxfifo_cnt_t *rxcnt = wlc->pub->_rxfifo_cnt;
#endif /* WLC_RXFIFO_CNT_ENABLE */

#ifdef BULKRX_PKTLIST
	rx_list_t rx_list = {0};
#ifdef STS_FIFO_RXEN
	rx_list_t rx_sts_list = {0};
#endif // endif
#endif /* BULKRX_PKTLIST */

	/* split_fifo will always hold orginal fifo number.
	   Only in the case of mode4, it will hold FIFO-1
	*/
	uint split_fifo = fifo;
	BCM_REFERENCE(n);

#if defined(DONGLEBUILD)
	ASSERT(fifo != RX_FIFO2);
#endif /* DONGLEBUILD */

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	/* get the TSF REG reading */
	tsf_l = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc));

	/* Pull out packets form RX dma layer */
#ifdef BULKRX_PKTLIST
#ifdef STS_FIFO_RXEN
	dma_rx(wlc_hw->di[fifo], &rx_list, &rx_sts_list, bound_limit);
#else
	dma_rx(wlc_hw->di[fifo], &rx_list, NULL, bound_limit);
#endif // endif
	ASSERT(rx_list.rxfifocnt <= bound_limit);
#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		if (rx_sts_list.rx_head != NULL) {
			wlc_bmac_dma_rxfill(wlc->hw, STS_FIFO);
		}
		wlc_bmac_recv_process_sts(wlc_hw, fifo, &rx_list,
				&rx_sts_list, 0);
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
	while (rx_list.rx_head != NULL) {
		p = rx_list.rx_head;
		rx_list.rx_head = PKTLINK(p);
		PKTSETLINK(p, NULL);

#else
	while ((p = dma_rx(wlc_hw->di[fifo]))) {
#endif /* BULKRX_PKTLIST */

#if defined(BCMPCIEDEV)
		/* Make sure it a data packet */
		ASSERT(PKTISRXFRAG(wlc_hw->osh, p));

#ifdef BULKRX_PKTLIST
		wlc_bmac_process_split_fifo_pkt(wlc_hw, p, 0);
#else
		/* For fifo-split rx , fifo-0/1 has to be synced up */
		if (!wlc_bmac_process_split_fifo_pkt(wlc_hw, fifo, p))
			continue;
#endif // endif
		/* In mode4 it's known that non-converted copy count data will
		 * arrive in fif0-1 so setting split_fifo to RX_FIFO1
		 */
		split_fifo = RX_FIFO1;
#endif /* BCMPCIEDEV */

		/* MAC/uCode/PHY RXHDR */
		rxh = (d11rxhdr_t *)PKTDATA(wlc_hw->osh, p);

#if !defined(DONGLEBUILD)
#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
		if ((STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) &&
			(D11RXHDR_GE129_ACCESS_VAL(rxh, dma_flags) & RXS_DMAFLAGS_MASK) !=
			RXS_MAC_UCODE_PHY) {
			rxh->ge129.hw_if = wlc_hw->unit;
		};
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
#endif /* !DONGLEBUILD */

		/* reserve room for SW RXHDR */
		wrxh = (wlc_d11rxhdr_t *)PKTPUSH(wlc_hw->osh, p, WLC_RXHDR_LEN);
		/* record the tsf_l in wlc_rxd11hdr */
		wrxh->tsf_l = tsf_l;

#ifdef CTFMAP
		/* sanity check */
		if (CTF_ENAB(kcih)) {
			ASSERT(((uintptr)wrxh & 31) == 0);
		}
#endif /* CTFMAP */

		/* fixup the "fifo" field in d11rxhdr_t */
		*(D11RXHDR_GE128_ACCESS_REF(rxh, fifo)) = split_fifo;

#ifdef WLC_TSYNC
		if (TSYNC_ACTIVE(wlc->pub))
			wlc_tsync_update_ts(wlc->tsync, p, D11RXHDR_ACCESS_VAL(rxh,
				wlc->pub->corerev, RxTSFTime),
				(D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev,
				AvbRxTimeH) << 16) |
				D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev,
				AvbRxTimeL), 0);
#endif // endif

#if (defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU)))

		if (MU_RX_ENAB(wlc) && wlc_murx_active(wlc->murx))
			wlc_bmac_save_murate2plcp(wlc, rxh, p);
#endif /* WL_MU_RX */

#if defined(DONGLEBUILD) && defined(WLC_RXFIFO_CNT_ENABLE)
		WLCNTINCR(rxcnt->rxf_data[fifo]);
#endif /* DONGLEBUILD && WLC_RXFIFO_CNT_ENABLE */

		ASSERT(PKTCLINK(p) == NULL);

#ifdef BCMDBG_POOL
		PKTPOOLSETSTATE(p, POOL_RXD11);
#endif // endif

		/* Classify the packets based on CFP RCB states and per packet info
		 * On the very first unchained packet sendup all chained packet and the
		 * unchainable packet and continue.
		 */
		wlc_cfp_rxframe(wlc, p);

#ifndef BULKRX_PKTLIST
		/* !give others some time to run! */
		if (++n >= bound_limit)
			break;
#endif /* BULKRX_PKTLIST */

	}

#ifdef BULKRX_PKTLIST
	ASSERT(rx_list.rx_head == NULL);
	rx_list.rx_tail = NULL;
#endif // endif
	/* Re-fill DMA descriptors */
	wlc_bmac_dma_rxfill(wlc_hw, fifo);
	/* Sendup chained packets */
	wlc_cfp_rx_sendup(wlc, NULL);
#ifdef BULKRX_PKTLIST
	dpc->processed += rx_list.rxfifocnt;
	return (rx_list.rxfifocnt >= bound_limit);
#else
	dpc->processed += n;
	return (n >= bound_limit);
#endif /* BULKRX_PKTLIST */
} /* wlc_cfp_bmac_recv */

#if defined(DONGLEBUILD)
/*
 * Return the enqued RX packet count across all prios in SCB CFP Q
 */
int
wlc_cfp_scb_rx_queue_count(wlc_info_t* wlc, struct scb *scb)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* SCB cubby */

	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	if (!scb_cfp) {
		WLC_CFP_DEBUG(("%s CUBBY already freed up for scb %p\n", __FUNCTION__, scb));
		return 0;
	}

	return (pktq_n_pkts_tot(SCB_CFP_RCB_CHAINEDQ(scb_cfp)));
}

/* XXX Debug audit code till a better audit for HWA packets added.
 * Not enabled currently.
 *
 * Do an audit of incoming Rx lfrag for duplicate rxbuffer issue
 */
void
wlc_cfp_rx_hwa_pkt_audit(wlc_info_t* wlc, void* p)
{
	scb_cfp_t * scb_cfp;
	struct pktq  *chained_q;	/* Per SCB- prio Chained Q */
	void* cur_pkt = NULL;
	void* next_pkt = NULL;
	uint16 idx = 0;
	uint8 prio = 0;

	/* Loop through each cfp cubby */
	FOREACH_CFP_FLOWID_G(WLC_CFP_UNIT(cfp_info), idx, scb_cfp) {
		if (scb_cfp == NULL)
			continue;

		/* Check for packet availability */
		if (pktq_n_pkts_tot(SCB_CFP_RCB_CHAINEDQ(scb_cfp)) == 0)
			continue;

		/* CFP RCB packet Q */
		chained_q = SCB_CFP_RCB_CHAINEDQ(scb_cfp);

		/* Loop through each priority */
		for (prio = 0; prio < NUMPRIO; prio++) {
			if (pktqprec_n_pkts(chained_q, prio) == 0)
				continue;

			/* Peek Head packet from the scb/cfp/prio queue */
			cur_pkt = pktqprec_peek(chained_q, prio);

			/* Loop through the PKTLINK */
			while (cur_pkt) {
				/* Compare against incoming pkt */
				if (PKTDATA(wlc->osh, cur_pkt) == PKTDATA(wlc->osh, p)) {
					WLC_CFP_ERROR(("ERROR:::: HWA RX packet reused \n"));
					WLC_CFP_ERROR(("scb cfp %p prio %x Saved pkt %p pktdata %p"
						"New  pkt  %p New pkt data %p \n",
						scb_cfp, prio, cur_pkt,
						PKTDATA(wlc->osh, cur_pkt), p,
						PKTDATA(wlc->osh, p)));
					ASSERT(0);
				}

				/* Check for the PKTNEXT [AMSDU case ] */
				if ((next_pkt = PKTNEXT(wlc->osh, cur_pkt)) != NULL) {
					/* Compare against incoming pkt */
					if (PKTDATA(wlc->osh, next_pkt) == PKTDATA(wlc->osh, p)) {
						WLC_CFP_ERROR(("ERROR: HWA RX packet reused "
							"[AMSDU Case] \n"));
						WLC_CFP_ERROR(("scb cfp %p prio %x Saved pkt %p"
							"pktdata %p  Newpkt %p Newpkt data %p\n",
							scb_cfp, prio, next_pkt,
							PKTDATA(wlc->osh, next_pkt), p,
							PKTDATA(wlc->osh, p)));
						ASSERT(0);
					}
				}

				/* Proceed to next Linked Packet */
				cur_pkt = PKTLINK(cur_pkt);
			}
		}
	}
}
#endif /* DONGLEBUILD */

#ifdef DONGLEBUILD
/** Exported functions to SQS uses cfp_unit = 0 in full dongle */
scb_cfp_t *
wlc_scb_cfp_id2ptr(uint16 cfp_flowid)
{
	scb_cfp_t *scb_cfp;

	ASSERT(CFP_FLOWID_VALID(cfp_flowid));
	scb_cfp = SCB_CFP_ID_TO_PTR_G(0, cfp_flowid);

	return scb_cfp;
	return NULL; /* No SQS in NIC mode */
}
#endif // endif
/** Return the scb cfp cubby given the scb ptr */
void*
wlc_scb_cfp_cubby(wlc_info_t *wlc, struct scb *scb)
{
	wlc_cfp_info_t	*cfp_info;	/* Public Info */
	wlc_cfp_t 	*cfp;		/* Module */
	scb_cfp_t	*scb_cfp;	/* SCB cubby */

	ASSERT(scb);
	ASSERT(wlc);

	/* Initialize */
	cfp_info = wlc->cfp;
	cfp = WLC_CFP(cfp_info);
	scb_cfp = SCB_CFP(cfp, scb);

	/* Can be called from scb cubby deinit context.
	 * So we could potentially return NULL here.
	 * Caller to check for validity.
	 */
	return scb_cfp;
}
