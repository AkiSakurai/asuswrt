/*
 * Common interface to the 802.11 Station Control Block (scb) structure
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
 * $Id: wlc_scb.c 785165 2020-03-16 08:54:33Z $
 */

/**
 * @file
 * @brief
 * SCB is a per-station data structure that is stored in the wl driver. SCB container provides a
 * mechanism through which different wl driver modules can each allocate and maintain private space
 * in the scb used for their own purposes. The scb subsystem (wlc_scb.c) does not need to know
 * anything about the different modules that may have allocated space in scb. It can also be used
 * by per-port code immediately after wlc_attach() has been done (but before wlc_up()).
 *
 * - "container" refers to the entire space within scb that can be allocated opaquely to other
 *   modules.
 * - "cubby" refers to the per-module private space in the container.
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
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#ifdef BCMCCX
#include <bcmcrypto/ccx.h>
#endif /* BCMCCX */
#include <bcmwpa.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#ifdef BCM_HOST_MEM_SCB
#include <wlc_scb_alloc.h>
#endif // endif

#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_scb_ratesel.h>
#include <wlc_assoc.h>
#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#include <wlc_apps.h>
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#endif /* WLAMPDU */
#ifdef WLNAR
#include <wlc_nar.h>
#endif /* WLNAR */
#endif /* PROP_TXSTATUS */

#ifdef WL11N
#include <wlc_ampdu_cmn.h>
#endif /* WLAMPDU */

#ifdef TRAFFIC_MGMT
#include <wlc_traffic_mgmt.h>
#endif // endif

#ifdef WL_STAPRIO
#include <wlc_staprio.h>
#endif /* WL_STAPRIO */

#include <wlc_pcb.h>
#include <wlc_txc.h>
#include <wlc_macfltr.h>
#ifdef WL_RELMCAST
#include "wlc_relmcast.h"
#endif // endif
#include <wlc_vht.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif /* WLTDLS */
#include <wlc_airtime.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif /* WLTAF */

#define SCB_MAX_CUBBY		(pub->tunables->maxscbcubbies)
#define SCB_MAGIC 0x0505a5a5

#define INTERNAL_SCB		0x00000001
#define USER_SCB		0x00000002

#define	SCBHASHINDEX(hash, id)	((id[3] ^ id[4] ^ id[5]) % (hash))

#define SCBHANDLE_PS_STATE_MASK (1 << 8)
#define SCBHANDLE_INFORM_PKTPEND_MASK (1 << 9)

#ifdef SCBFREELIST
#ifdef INT_SCB_OPT
#error "SCBFREELIST incompatible with INT_SCB_OPT"
/* To make it compatible, freelist needs to track internal vs external */
#endif /* INT_SCB_OPT */
#endif /* SCBFREELIST */
/** structure for storing per-cubby client info */
typedef struct cubby_info {
	scb_cubby_init_t	fn_init;	/* fn called during scb malloc */
	scb_cubby_deinit_t	fn_deinit;	/* fn called during scb free */
	scb_cubby_dump_t	fn_dump;	/* fn called during scb dump */
	void			*context;	/* context to be passed to all cb fns */
	int			cubby_id;
} cubby_info_t;

/** structure for storing public and private global scb module state */
struct scb_module {
	wlc_info_t	*wlc;			/* global wlc info handle */
	wlc_pub_t	*pub;			/* public part of wlc */
	uint16		nscb;			/* total number of allocated scbs */
	uint		scbtotsize;		/* total scb size including container */
	uint 		ncubby;			/* current num of cubbies */
	cubby_info_t	*cubby_info;		/* cubby client info */

	int		cfgh;			/* scb bsscfg cubby handle */
	bcm_notif_h 	scb_state_notif_hdl;	/* scb state notifier handle. */

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
	struct scb	*free_list; 	/* Free list of SCBs */
	int16		cnt;	/* No of scb memory in free_list */
#endif /* defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB) */
};

/** station control block - one per remote MAC address */
struct scb_info {
	struct scb	*scbpub;        /* public portion of scb */
	struct scb_info *hashnext;      /* pointer to next scb under same hash entry */
	struct scb_info *next;          /* pointer to next allocated scb */
	struct wlcband  *band;          /* pointer to our associated band */
#ifdef MACOSX
	struct scb_info *hashnext_copy;
	struct scb_info *next_copy;
#endif // endif
	struct scb_info *shadow;        /* pointer to the shadow of the scb_info in the host */
};

/* Helper macro for txpath in scb */
/* A feature in Tx path goes through following states:
 * Unregisterd -> Registered [Global state]
 * Registerd -> Configured -> Active -> Configured [Per-scb state]
 */

/* Set the next feature of given feature */
#define SCB_TXMOD_SET(scb, fid, _next_fid) { \
	scb->tx_path[fid].next_tx_fn = wlc->txmod_fns[_next_fid].tx_fn; \
	scb->tx_path[fid].next_handle = wlc->txmod_fns[_next_fid].ctx; \
	scb->tx_path[fid].next_fid = _next_fid; \
}
static void wlc_scb_hash_add(wlc_info_t *wlc, struct scb *scb, int bandunit,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_hash_del(wlc_info_t *wlc, struct scb *scbd, int bandunit,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_list_add(wlc_info_t *wlc, struct scb_info *scbinfo,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_list_del(wlc_info_t *wlc, struct scb *scbd,
	wlc_bsscfg_t *bsscfg);

static struct scb *wlc_scbvictim(wlc_info_t *wlc);
static struct scb *wlc_scb_getnext(struct scb *scb);
static struct wlc_bsscfg *wlc_scb_next_bss(scb_module_t *scbstate, int idx);
static int wlc_scbinit(wlc_info_t *wlc, struct wlcband *band, struct scb_info *scbinfo,
	uint32 scbflags);
static void wlc_scb_reset(scb_module_t *scbstate, struct scb_info *scbinfo);
static struct scb_info *wlc_scb_allocmem(scb_module_t *scbstate);
static void wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo);

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
void wlc_scb_list_dump(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif

#if BCM_HOST_MEM_SCBTAG_INUSE
static void BCMATTACHFN(scbtag_attach)(void);
static void BCMATTACHFN(scbtag_detach)(void);
#endif /* BCM_HOST_MEM_SCBTAG_INUSE */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_dump_scb(wlc_info_t *wlc, struct bcmstrbuf *b);
/** Dump the active txpath for the current SCB */
static int wlc_scb_txpath_dump(wlc_info_t *wlc, struct scb *scb, struct bcmstrbuf *b);
/** SCB Flags Names Initialization */
static const bcm_bit_desc_t scb_flags[] =
{
	{SCB_NONERP, "NonERP"},
	{SCB_LONGSLOT, "LgSlot"},
	{SCB_SHORTPREAMBLE, "ShPre"},
	{SCB_8021XHDR, "1X"},
	{SCB_WPA_SUP, "WPASup"},
	{SCB_DEAUTH, "DeA"},
	{SCB_WMECAP, "WME"},
	{SCB_BRCM, "BRCM"},
	{SCB_WDS_LINKUP, "WDSLinkUP"},
	{SCB_LEGACY_AES, "LegacyAES"},
	{SCB_MYAP, "MyAP"},
	{SCB_PENDING_PROBE, "PendingProbe"},
	{SCB_AMSDUCAP, "AMSDUCAP"},
	{SCB_USEME, "XXX"},
	{SCB_HTCAP, "HT"},
	{SCB_RECV_PM, "RECV_PM"},
	{SCB_AMPDUCAP, "AMPDUCAP"},
	{SCB_IS40, "40MHz"},
	{SCB_NONGF, "NONGFCAP"},
	{SCB_APSDCAP, "APSDCAP"},
	{SCB_PENDING_FREE, "PendingFree"},
	{SCB_PENDING_PSPOLL, "PendingPSPoll"},
	{SCB_RIFSCAP, "RIFSCAP"},
	{SCB_HT40INTOLERANT, "40INTOL"},
	{SCB_WMEPS, "WMEPSOK"},
	{SCB_COEX_MGMT, "OBSSCoex"},
	{SCB_IBSS_PEER, "IBSS Peer"},
	{SCB_STBCCAP, "STBC"},
#ifdef WLBTAMP
	{SCB_11ECAP, "11e"},
#endif // endif
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags2[] =
{
	{SCB2_SGI20_CAP, "SGI20"},
	{SCB2_SGI40_CAP, "SGI40"},
	{SCB2_RX_LARGE_AGG, "LGAGG"},
#ifdef BCMWAPI_WAI
	{SCB2_WAIHDR, "WAI"},
#endif /* BCMWAPI_WAI */
	{SCB2_LDPCCAP, "LDPC"},
	{SCB2_VHTCAP, "VHT"},
	{SCB2_AMSDU_IN_AMPDU_CAP, "AGG^2"},
	{SCB2_P2P, "P2P"},
	{SCB2_DWDS_ACTIVE, "DWDS_ACTIVE"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags3[] =
{
	{SCB3_A4_DATA, "A4_DATA"},
	{SCB3_A4_NULLDATA, "A4_NULLDATA"},
	{SCB3_A4_8021X, "A4_8021X"},
	{SCB3_DWDS_CAP, "DWDS_CAP"},
	{SCB3_MAP_CAP, "MAP_CAP"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_states[] =
{
	{AUTHENTICATED, "AUTH"},
	{ASSOCIATED, "ASSOC"},
	{PENDING_AUTH, "AUTH_PEND"},
	{PENDING_ASSOC, "ASSOC_PEND"},
	{AUTHORIZED, "AUTH_8021X"},
	{TAKEN4IBSS, "IBSS"},
	{0, NULL}
};
#endif /* BCMDBG || BCMDBG_DUMP */

#if defined(PKTC) || defined(PKTC_DONGLE)
static void wlc_scb_pktc_enable(struct scb *scb);
static void wlc_scb_pktc_disable(struct scb *scb);
#endif // endif

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
static void wlc_scbadd_free(scb_module_t *scbstate, struct scb_info *ret);
static void wlc_scbfreelist_free(scb_module_t *scbstate);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b);
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */
#endif /* defined(SCBFREELIST)  || defined(BCM_HOST_MEM_SCB) */

#define SCBINFO(_scb) (_scb ? (struct scb_info *)((_scb)->scb_priv) : NULL)

#ifdef MACOSX

#define SCBSANITYCHECK(_scb)  { \
		if (((_scb) != NULL) &&				\
		    ((((_scb))->magic != SCB_MAGIC) ||	\
		     (SCBINFO(_scb)->hashnext != SCBINFO(_scb)->hashnext_copy) || \
		     (SCBINFO(_scb)->next != SCBINFO(_scb)->next_copy)))	\
			osl_panic("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
			      ((_scb))->magic, SCBINFO(_scb)->hashnext, \
			      SCBINFO(_scb)->hashnext_copy,		\
			      SCBINFO(_scb)->next, SCBINFO(_scb)->next_copy);	\
	}

#define SCBFREESANITYCHECK(_scb)  { \
		if (((_scb) != NULL) &&				\
		    ((((_scb))->magic != ~SCB_MAGIC) || \
		     (SCBINFO(_scb)->next != SCBINFO(_scb)->next_copy)))	\
			osl_panic("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
			      ((_scb))->magic, SCBINFO(_scb)->hashnext, \
			      SCBINFO(_scb)->hashnext_copy,		\
			      SCBINFO(_scb)->next, SCBINFO(_scb)->next_copy);	\
	}

#else

#define SCBSANITYCHECK(_scbinfo)	do {} while (0)
#define SCBFREESANITYCHECK(_scbinfo)	do {} while (0)

#endif /* MACOSX */

/** bsscfg cubby */
typedef struct scb_bsscfg_cubby {
	struct scb	**scbhash[MAXBANDS];	/* scb hash table */
	uint8		nscbhash;		/* scb hash table size */
	struct scb	*scb;			/* station control block link list */

	/* user SCB storage, points of user SCB */
	struct scb	**scbptr[MAXBANDS];

	/* internal SCB storage, pointer of int SCB */
	struct scb	*int_scbptr[MAXBANDS * 2];  /* internal SCB pointers */
} scb_bsscfg_cubby_t;

#define SCB_BSSCFG_CUBBY(ss, cfg) ((scb_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (ss)->cfgh))

static int wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_scb_bsscfg_dump NULL
#endif // endif

static int
wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	uint8 nscbhash, *scbhash;
	wlc_pub_t *pub = scbstate->pub;
	uint32 i, len;
#ifdef WL_SCB_INDIRECT_ACCESS
	struct scb *scbptr = NULL;
#endif /* WL_SCB_INDIRECT_ACCESS */

	nscbhash = ((pub->tunables->maxscb + 7)/8); /* # scb hash buckets */

	len = (sizeof(struct scb *) * MAXBANDS * nscbhash);
	scbhash = MALLOC(pub->osh, len);
	if (scbhash == NULL)
		return BCME_NOMEM;

	bzero((char *)scbhash, len);

	scb_cfg->nscbhash = nscbhash;
	for (i = 0; i < MAXBANDS; i++) {
		scb_cfg->scbhash[i] = (struct scb **)((uintptr)scbhash +
		                      (i * scb_cfg->nscbhash * sizeof(struct scb *)));
	}

#ifdef WL_SCB_INDIRECT_ACCESS
	/* MAlloc array to store SCB pointer.
	 * SCB pointer in PKTTAG points this Location, rather than SCB itself.
	 */
	len = (sizeof(struct scb *) * MAXBANDS * (pub->tunables->maxscb));
	scbptr = MALLOCZ(pub->osh, len);
	if (scbptr == NULL)
		return BCME_NOMEM;

	for (i = 0; i < MAXBANDS; i++) {
		scb_cfg->scbptr[i] = (void *)((uintptr)scbptr +
		                      (i * (pub->tunables->maxscb) * sizeof(struct scb *)));
	}
	WL_INFORM(("scb indirect storage[size:%d] at 0x%p\n", pub->tunables->maxscb, scbptr));
#endif /* WL_SCB_INDIRECT_ACCESS */

	return BCME_OK;
}

static void
wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	uint32 len;

	/* clear all scbs */
	wlc_scb_bsscfg_scbclear(cfg->wlc, cfg, TRUE);

	if (scb_cfg->scbhash[0] != NULL) {
		len = (sizeof(struct scb *) * MAXBANDS * scb_cfg->nscbhash);
		MFREE(scbstate->pub->osh, scb_cfg->scbhash[0], len);
	}
}

scb_module_t *
BCMATTACHFN(wlc_scb_attach)(wlc_info_t *wlc)
{
	scb_module_t *scbstate;
	int len;
	wlc_pub_t *pub = wlc->pub;

	len = sizeof(scb_module_t) + (sizeof(cubby_info_t) * SCB_MAX_CUBBY);
	if ((scbstate = MALLOC(pub->osh, len)) == NULL)
		return NULL;
	bzero((char *)scbstate, len);

	scbstate->wlc = wlc;
	scbstate->pub = pub;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((scbstate->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(scb_bsscfg_cubby_t),
		wlc_scb_bsscfg_init, wlc_scb_bsscfg_deinit, wlc_scb_bsscfg_dump,
		(void *)scbstate)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		MFREE(pub->osh, scbstate, len);
		return NULL;
	}

	scbstate->cubby_info = (cubby_info_t *)((uintptr)scbstate + sizeof(scb_module_t));

	scbstate->scbtotsize = sizeof(struct scb);
	scbstate->scbtotsize += sizeof(int) * MA_WINDOW_SZ; /* sizeof rssi_window */
	scbstate->scbtotsize += sizeof(struct tx_path_node) * TXMOD_LAST;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(pub, "scb", (dump_fn_t)wlc_dump_scb, (void *)wlc);
#endif // endif

	/* create notification list for scb state change. */
	if (bcm_notif_create_list(wlc->notif, &scbstate->scb_state_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: scb bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		MFREE(pub->osh, scbstate, len);
		return NULL;
	}

#if BCM_HOST_MEM_SCBTAG_INUSE
	scbtag_attach();
#endif /* BCM_HOST_MEM_SCBTAG_INUSE */

	return scbstate;
}

void
BCMATTACHFN(wlc_scb_detach)(scb_module_t *scbstate)
{
	wlc_pub_t *pub;
	int len;

	if (!scbstate)
		return;

	if (scbstate->scb_state_notif_hdl != NULL)
		bcm_notif_delete_list(&scbstate->scb_state_notif_hdl);

	pub = scbstate->pub;

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
	wlc_scbfreelist_free(scbstate);
#endif /* defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB) */

	ASSERT(scbstate->nscb == 0);

	len = sizeof(scb_module_t) + (sizeof(cubby_info_t) * SCB_MAX_CUBBY);
	MFREE(scbstate->pub->osh, scbstate, len);

#if BCM_HOST_MEM_SCBTAG_INUSE
	scbtag_detach();
#endif /* BCM_HOST_MEM_SCBTAG_INUSE */
}

/* Methods for iterating along a list of scb */

/** Direct access to the next */
static struct scb *
wlc_scb_getnext(struct scb *scb)
{
	if (scb) {
		SCBSANITYCHECK(scb);
		return (SCBINFO(scb)->next ? SCBINFO(scb)->next->scbpub : NULL);
	}
	return NULL;
}
static struct wlc_bsscfg *
wlc_scb_next_bss(scb_module_t *scbstate, int idx)
{
	wlc_bsscfg_t	*next_bss = NULL;

	/* get next bss walking over hole */
	while (idx < WLC_MAXBSSCFG) {
		next_bss = WLC_BSSCFG(scbstate->wlc, idx);
		if (next_bss != NULL)
			break;
		idx++;
	}
	return next_bss;
}

/** Initialize an iterator keeping memory of the next scb as it moves along the list */
void
wlc_scb_iterinit(scb_module_t *scbstate, struct scb_iter *scbiter, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	ASSERT(scbiter != NULL);

	if (bsscfg == NULL) {
		/* walk scbs of all bss */
		scbiter->all = TRUE;
		scbiter->next_bss = wlc_scb_next_bss(scbstate, 0);
		if (scbiter->next_bss == NULL) {
			/* init next scb pointer also to null */
			scbiter->next = NULL;
			return;
		}
	} else {
		/* walk scbs of specified bss */
		scbiter->all = FALSE;
		scbiter->next_bss = bsscfg;
	}

	ASSERT(scbiter->next_bss != NULL);
	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, scbiter->next_bss);
	SCBSANITYCHECK(scb_cfg->scb);

	/* Prefetch next scb, so caller can free an scb before going on to the next */
	scbiter->next = scb_cfg->scb;
}

/** move the iterator */
struct scb *
wlc_scb_iternext(scb_module_t *scbstate, struct scb_iter *scbiter)
{
	scb_bsscfg_cubby_t *scb_cfg;
	struct scb *scb;

	ASSERT(scbiter != NULL);

	while (scbiter->next_bss) {

		/* get the next scb in the current bsscfg */
		if ((scb = scbiter->next) != NULL) {
			/* get next scb of bss */
			SCBSANITYCHECK(scb);
			scbiter->next = (SCBINFO(scb)->next ? SCBINFO(scb)->next->scbpub : NULL);
			return scb;
		}

		/* get the next bsscfg if we have run out of scbs in the current bsscfg */
		if (scbiter->all) {
			scbiter->next_bss =
			        wlc_scb_next_bss(scbstate, WLC_BSSCFG_IDX(scbiter->next_bss) + 1);
			if (scbiter->next_bss != NULL) {
				scb_cfg = SCB_BSSCFG_CUBBY(scbstate, scbiter->next_bss);
				scbiter->next = scb_cfg->scb;
			}
		} else {
			scbiter->next_bss = NULL;
		}
	}

	/* done with all bsscfgs and scbs */
	scbiter->next = NULL;

	return NULL;
}

/**
 * Multiple modules have the need of reserving some private data storage related to a specific
 * communication partner. During ATTACH time, this function is called multiple times, typically one
 * time per module that requires this storage. This function does not allocate memory, but
 * calculates values to be used for a future memory allocation by wlc_scb_allocmem() instead.
 *
 * Return value: negative values are errors.
 */
int
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
BCMATTACHFN(wlc_scb_cubby_reserve)(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context, int cubby_id)
#else
BCMATTACHFN(wlc_scb_cubby_reserve)(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
        scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context)
#endif // endif
{
	uint offset;
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	wlc_pub_t *pub = wlc->pub;

	ASSERT(scbstate->nscb == 0);
	ASSERT((scbstate->scbtotsize % PTRSZ) == 0);

	if (scbstate->ncubby >= (uint)SCB_MAX_CUBBY) {
		ASSERT(scbstate->ncubby < (uint)SCB_MAX_CUBBY);
		return BCME_NORESOURCE;
	}

	WL_TRACE(("cubby:%d\n", scbstate->ncubby));

	/* housekeeping info is stored in scb_module struct */
	cubby_info = &scbstate->cubby_info[scbstate->ncubby++];
	cubby_info->fn_init = fn_init;
	cubby_info->fn_deinit = fn_deinit;
	cubby_info->fn_dump = fn_dump;
	cubby_info->context = context;
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
	cubby_info->cubby_id = cubby_id;
#endif // endif
	/* actual cubby data is stored at the end of scb's */
	offset = scbstate->scbtotsize;

	/* roundup to pointer boundary */
	scbstate->scbtotsize = ROUNDUP(scbstate->scbtotsize + size, PTRSZ);

	return offset;
}

struct wlcband *
wlc_scbband(struct scb *scb)
{
	return SCBINFO(scb)->band;
}

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
static struct scb_info *
wlc_scbget_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;

#ifdef BCM_HOST_MEM_SCB
	struct scb_info *scbinfo;
	struct scb *scbpub;

	int scb_info_len, scb_len;

	scb_info_len = sizeof(struct scb_info);
	scb_info_len = ROUNDUP(scb_info_len, 16);

	scb_len = scbstate->scbtotsize;
	scb_len = ROUNDUP(scb_len, 16);

	if (SCB_ALLOC_ENAB(scbstate->pub) && !scbstate->free_list) {
		scbinfo = (struct scb_info *)wlc_scb_alloc_mem_get(scbstate->wlc,
			SCB_CUBBY_ID_SCBINFO, scb_len + scb_info_len, 1);

		WL_INFORM(("info:0x%p %d : %d \n", scbinfo, scb_len, scb_info_len));
		while (scbinfo) {
			scbpub = (struct scb *)((char *)scbinfo + scb_info_len);

			scbinfo->scbpub = scbpub;
			WL_INFORM(("scb:0x%p scbinfo:0x%p\n", scbpub, scbinfo));

			wlc_scb_reset(scbstate, scbinfo);
			wlc_scbadd_free(scbstate, scbinfo);
			scbinfo = (struct scb_info *)wlc_scb_alloc_mem_get(scbstate->wlc,
				SCB_CUBBY_ID_SCBINFO, scb_len + scb_info_len, 1);
		}
	}
#endif /* BCM_HOST_MEM_SCB */

	if (scbstate->free_list == NULL) {
		WL_ERROR(("no free scb. cnt:0x%x \n", scbstate->cnt));
		return NULL;
	}
	ret = SCBINFO(scbstate->free_list);
	SCBFREESANITYCHECK(ret->scbpub);
	scbstate->free_list = (ret->next ? ret->next->scbpub : NULL);
#ifdef MACOSX
	ret->next_copy = NULL;
#endif // endif
	ret->next = NULL;
	wlc_scb_reset(scbstate, ret);

	scbstate->cnt--;
	WL_INFORM(("%s: found scb:0x%p[%d]\n", __FUNCTION__, (void *)ret, scbstate->cnt));

	return ret;
}

static void
wlc_scbadd_free(scb_module_t *scbstate, struct scb_info *ret)
{
	SCBFREESANITYCHECK(scbstate->free_list);

	ret->next = SCBINFO(scbstate->free_list);
	scbstate->free_list = ret->scbpub;
#ifdef MACOSX
	ret->scbpub->magic = ~SCB_MAGIC;
	ret->next_copy = ret->next;
#endif // endif

	scbstate->cnt++;
	WL_INFORM(("%s: add scb:0x%p[%d]\n", __FUNCTION__, (void *)ret, scbstate->cnt));
}

static void
wlc_scbfreelist_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;
	ret = SCBINFO(scbstate->free_list);
	while (ret) {
#ifdef MACOSX
		SCBFREESANITYCHECK(ret->scbpub);
#endif // endif
		scbstate->free_list = (ret->next ? ret->next->scbpub : NULL);
		wlc_scb_freemem(scbstate, ret);
		ret = scbstate->free_list ? SCBINFO(scbstate->free_list) : NULL;
	}
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static
void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b)
{
	struct scb_info *entry = NULL;
	int i = 1;

	bcm_bprintf(b, "scbfreelist:\n");
	entry = SCBINFO(scbstate->free_list);
	while (entry) {
#ifdef MACOSX
		SCBFREESANITYCHECK(entry->scbpub);
#endif // endif
		bcm_bprintf(b, "%d: 0x%x\n", i, entry);
		entry = entry->next ? SCBINFO(entry->next->scbpub) : NULL;
		i++;
	}
}
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) */
#endif /* defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB) */

void
wlc_internalscb_free(wlc_info_t *wlc, struct scb *scb)
{
	scb->permanent = FALSE;
	wlc_scbfree(wlc, scb);
}

static void
wlc_scb_reset(scb_module_t *scbstate, struct scb_info *scbinfo)
{
	struct scb *scbpub = scbinfo->scbpub;

	bzero((char*)scbinfo, sizeof(struct scb_info));
	scbinfo->scbpub = scbpub;
	bzero(scbpub, scbstate->scbtotsize);
	scbpub->scb_priv = (void *) scbinfo;
	/* init substructure pointers */
	scbpub->rssi_window = (int *)((char *)scbpub + sizeof(struct scb));
	scbpub->tx_path = (struct tx_path_node *)
	                ((char *)scbpub->rssi_window + (sizeof(int)*MA_WINDOW_SZ));
}

/**
 * After all the modules indicated how much cubby space they need in the scb, the actual scb can be
 * allocated. This happens one time fairly late within the attach phase, but also when e.g.
 * communication with a new remote party is started.
 */
static struct scb_info *
wlc_scb_allocmem(scb_module_t *scbstate)
{
	struct scb_info *scbinfo = NULL;
	struct scb *scbpub;

	scbinfo = MALLOC(scbstate->pub->osh, sizeof(struct scb_info));
	if (!scbinfo) {
		WL_ERROR(("wl%d: %s: Internalscb alloc failure for scb_info %d\n",
			scbstate->pub->unit, __FUNCTION__, (int)sizeof(struct scb_info)));
		return NULL;
	}
	scbpub = MALLOC(scbstate->pub->osh, scbstate->scbtotsize);

	WL_INFORM(("scb_allocmem: scbtotsize=%d, sizeof(scb):%d, scbpub:0x%p scbinfo:0x%p\n",
		scbstate->scbtotsize, (int)(sizeof(struct scb)), (void *)scbpub, (void *)scbinfo));

	scbinfo->scbpub = scbpub;
	if (!scbpub) {
		/* set field to null so freeing mem does */
		/* not cause exception by freeing bad ptr */
		scbinfo->scbpub = NULL;
		wlc_scb_freemem(scbstate, scbinfo);
		WL_ERROR(("wl%d: %s: Internalscb alloc failure for scbtotsize %d\n",
			scbstate->pub->unit, __FUNCTION__, (int)scbstate->scbtotsize));
		return NULL;
	}

	wlc_scb_reset(scbstate, scbinfo);

	return scbinfo;
}

struct scb *
wlc_internalscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *ea, struct wlcband *band)
{
	struct scb_info *scbinfo = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	int bcmerror = 0;
	struct scb *scb;
#ifdef WL_SCB_INDIRECT_ACCESS
	int indx;
	scb_bsscfg_cubby_t *scb_cfg;
#endif /* WL_SCB_INDIRECT_ACCESS */

	WL_INFORM(("int: nscb:%d\n", scbstate->nscb));

#ifdef SCBFREELIST
	/* If not found on freelist then allocate a new one */
	if ((scbinfo = wlc_scbget_free(scbstate)) == NULL)
#endif // endif
	{
		scbinfo = wlc_scb_allocmem(scbstate);
		if (!scbinfo) {
			WL_ERROR(("wl%d: %s wlc_scb_allocmem failed\n",
				wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
	}

	scb = scbinfo->scbpub;
	scb->bsscfg = cfg;
	scb->ea = *ea;

	bcmerror = wlc_scbinit(wlc, band, scbinfo, INTERNAL_SCB);
	if (bcmerror) {
		WL_ERROR(("wl%d: %s failed with err %d\n",
			wlc->pub->unit, __FUNCTION__, bcmerror));
		wlc_internalscb_free(wlc, scb);
		return NULL;
	}
	scb->permanent = TRUE;

	/* force wlc_scb_set_bsscfg() */
	scb->bsscfg = NULL;
	wlc_scb_set_bsscfg(scb, cfg);

#ifdef WL_SCB_INDIRECT_ACCESS
	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, cfg);

	/* Find mem loc to store Internal SCB's addr */
	for (indx = 0; indx < MAXBANDS * 2; indx++) {
		if (scb_cfg->int_scbptr[indx] == NULL) {
			scb_cfg->int_scbptr[indx] = scb;
			scb->scb_addr = (struct scb *)&(scb_cfg->int_scbptr[indx]);
			break;
		}
	}
	ASSERT(indx < MAXBANDS * 2);
#endif /* WL_SCB_INDIRECT_ACCESS */
	return scb;
}

static struct scb *
wlc_userscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *ea, struct wlcband *band)
{
	scb_module_t *scbstate = wlc->scbstate;
	struct scb_info *scbinfo = NULL;
	struct scb *oldscb;
	int bcmerror;
	struct scb *scb;

#ifdef WL_SCB_INDIRECT_ACCESS
	int indx, bandunit;
	scb_bsscfg_cubby_t *scb_cfg;
#endif /* WL_SCB_INDIRECT_ACCESS */

#ifdef BCM_HOST_MEM_SCB
	bool ishost = wlc_scb_alloc_ishost(wlc, cfg);

	WL_TRACE(("bsscfg_class:0x%x is_Public:0x%x\n",
		wlc_get_bsscfg_class(wlc->staprio, cfg), ishost));
#endif /* BCM_HOST_MEM_SCB */

	if ((scbstate->nscb < wlc->pub->tunables->maxscb) &&
#ifdef DONGLEBUILD
		/* Make sure free_mem never gets below minimum threshold due to scb_allocs */
		(OSL_MEM_AVAIL() > wlc->pub->tunables->min_scballoc_mem) &&
#endif // endif
		1) {

		/* If not found on freelist then allocate a new one */
#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
#ifdef BCM_HOST_MEM_SCB
		if (SCB_ALLOC_ENAB(wlc->pub) && ishost)
#endif // endif
		scbinfo = wlc_scbget_free(scbstate);
		if (scbinfo == NULL)
#endif /* defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB) */
		{
			scbinfo = wlc_scb_allocmem(scbstate);
#ifdef BCM_HOST_MEM_SCB
			ishost = 0;
#endif // endif
			if (!scbinfo) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				return NULL;
			} else {
				scb = scbinfo->scbpub;
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
				int scb_info_len, scb_len;
				scb_info_len = sizeof(struct scb_info);
				scb_info_len = ROUNDUP(scb_info_len, 16);
				scb_len = scbstate->scbtotsize;
				scb_len = ROUNDUP(scb_len, 16);
				if (SCB_ALLOC_ENAB(scbstate->pub)) {
					scbinfo->shadow = (struct scb_info *)wlc_scb_alloc_mem_get(
						scbstate->wlc,
						SCB_CUBBY_ID_SCBINFO, scb_len + scb_info_len, 1);
					if (scbinfo->shadow) {
						scbinfo->shadow->scbpub = (struct scb *)
							((char *)scbinfo->shadow + scb_info_len);
						scb->shadow = scbinfo->shadow->scbpub;
						scbinfo->shadow->shadow = scbinfo;
						scb->shadow->shadow = scb;
					}
				}
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */
			}
		}
	}
	if (!scbinfo) {
		/* free the oldest entry */
		if (!(oldscb = wlc_scbvictim(wlc))) {
			WL_ERROR(("wl%d: %s: no SCBs available to reclaim\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
		if (!wlc_scbfree(wlc, oldscb)) {
			WL_ERROR(("wl%d: %s: Couldn't free a victimized scb\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
		ASSERT(scbstate->nscb < wlc->pub->tunables->maxscb);
		/* allocate memory for scb */
		if (!(scbinfo = wlc_scb_allocmem(scbstate))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return NULL;
		}
	}

	scbstate->nscb++;

	scb = scbinfo->scbpub;
	scb->bsscfg = cfg;
	scb->ea = *ea;

#ifdef BCM_HOST_MEM_SCB
	/* check if SCB is from host memory or dongle memory */
	if (SCB_ALLOC_ENAB(wlc->pub) && ishost)
		scb->flags3 |= SCB3_HOST_MEM;
#endif /* BCM_HOST_MEM_SCB */

	bcmerror = wlc_scbinit(wlc, band, scbinfo, USER_SCB);
	if (bcmerror) {
		WL_ERROR(("wl%d: %s failed with err %d\n", wlc->pub->unit, __FUNCTION__, bcmerror));
		wlc_scbfree(wlc, scb);
		return NULL;
	}

	/* add it to the link list */
	wlc_scb_list_add(wlc, scbinfo, cfg);

	/* install it in the cache */
	wlc_scb_hash_add(wlc, scb, band->bandunit, cfg);

	/* force wlc_scb_set_bsscfg() */
	scb->bsscfg = NULL;
	wlc_scb_set_bsscfg(scb, cfg);

#ifdef WL_SCB_INDIRECT_ACCESS
	bandunit = band->bandunit;
	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, cfg);

	/* Find mem loc to store user SCB's addr */
	for (indx = 0; indx < wlc->pub->tunables->maxscb; indx++) {
		if (scb_cfg->scbptr[bandunit][indx] == NULL) {
			scb_cfg->scbptr[bandunit][indx] = scb;
			scb->scb_addr = (struct scb *)&(scb_cfg->scbptr[bandunit][indx]);
			break;
		}
	}

	ASSERT(indx < wlc->pub->tunables->maxscb);
#endif /* WL_SCB_INDIRECT_ACCESS */

	return scb;
}

static int
wlc_scbinit(wlc_info_t *wlc, struct wlcband *band, struct scb_info *scbinfo, uint32 scbflags)
{
	struct scb *scb = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	uint i;
	int bcmerror = 0;

	scb = scbinfo->scbpub;
	ASSERT(scb != NULL);

	scb->used = wlc->pub->now;
	scb->bandunit = band->bandunit;
	scbinfo->band = band;

	for (i = 0; i < NUMPRIO; i++)
		scb->seqctl[i] = 0xFFFF;
	scb->seqctl_nonqos = 0xFFFF;

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB(wlc->pub)) {
		for (i = 0; i < FLOWRING_PER_SCB_MAX; i++) {
			RAVG_INIT(TXPKTLEN_RAVG(scb, i), (ETHER_MAX_DATA/2), RAVG_EXP_PKT);
			RAVG_INIT(WEIGHT_RAVG(scb, i), 0, RAVG_EXP_WGT);
		}
	}
#endif /* BCMPCIEDEV */

#ifdef MACOSX
	scb->magic = SCB_MAGIC;
#endif // endif

	/* no other inits are needed for internal scb */
	if (scbflags & INTERNAL_SCB) {
		scb->flags2 |= SCB2_INTERNAL;
#ifdef INT_SCB_OPT
		return BCME_OK;
#endif // endif
	}

	for (i = 0; i < scbstate->ncubby; i++) {
		cubby_info = &scbstate->cubby_info[i];
		if (cubby_info->fn_init) {
			bcmerror = cubby_info->fn_init(cubby_info->context, scb);
			if (bcmerror) {
				WL_ERROR(("wl%d: %s: Cubby failed\n",
				          wlc->pub->unit, __FUNCTION__));
				return bcmerror;
			}
		}
	}

#if defined(AP) || defined(WLDPT)
	wlc_scb_rssi_init(scb, WLC_RSSI_INVALID);
#endif // endif
#ifdef PSPRETEND
	scb->ps_pretend = PS_PRETEND_NOT_ACTIVE;
	scb->ps_pretend_failed_ack_count = 0;
#endif // endif
	return bcmerror;
}

static void
wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo)
{

#ifdef BCM_HOST_MEM_SCB
	if (scbinfo->scbpub && SCB_HOST(scbinfo->scbpub)) {
		wlc_scbadd_free(scbstate, scbinfo);
		return;
	}
#ifdef BCM_HOST_MEM_RESTORE
	struct scb* scb = scbinfo->scbpub;

	if ((unsigned int)scb < HOST_MEM_ADDR_RANGE && scb->shadow != NULL) {
		wlc_scbadd_free(scbstate, scbinfo->shadow);
	}
#endif	/* BCM_HOST_MEM_RESTORE */
#endif /* BCM_HOST_MEM_SCB */

	if (scbinfo->scbpub)
		MFREE(scbstate->pub->osh, scbinfo->scbpub, scbstate->scbtotsize);
	MFREE(scbstate->pub->osh, scbinfo, sizeof(struct scb_info));
}

#ifdef PROP_TXSTATUS
static struct scb * wlc_scbfind_from_wlcif(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr)
{
	struct scb *scb = NULL;
	wlc_bsscfg_t *bsscfg;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	char *bss_addr;

	if (!bsscfg)
		return NULL;

	if (BSSCFG_STA(bsscfg) && bsscfg->BSS) {
#ifdef WLTDLS
		if (TDLS_ENAB(wlc->pub))
			scb = wlc_tdls_scbfind_all(wlc, (struct ether_addr *)addr);
#endif // endif
		if (scb == NULL) {
			bss_addr = (char *)&bsscfg->BSSID;
			if (ETHER_ISNULLADDR(bss_addr))
				bss_addr = (char *)&bsscfg->prev_BSSID;
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)bss_addr);
		}
	} else if (!ETHER_ISMULTI(addr)) {
#ifdef WLAWDL
		scb = wlc_scbfind_dualband(wlc, bsscfg, (struct ether_addr *)addr);
#else
		scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)addr);
#endif /* WLAWDL */
	} else
		scb = bsscfg->bcmc_scb[wlc->band->bandunit];

	return scb;
}

void
wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle, uint8 ta_bmp)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->mac_address_handle &&
			(scb->mac_address_handle == mac_handle)) {
			SCB_PROPTXTSTATUS_SETTIM(scb, ta_bmp);
			if (AP_ENAB(wlc->pub))
				wlc_apps_pvb_update_from_host(wlc, scb);
			break;
		}
	}
}

bool
wlc_flow_ring_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle,
	uint8 tid, bool op)
{
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 ta_bmp;
	bool  ret = TRUE;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->mac_address_handle &&
			(scb->mac_address_handle == mac_handle)) {
			ta_bmp = SCB_PROPTXTSTATUS_TIM(scb);
			ta_bmp = (ta_bmp & ~(0x1 << tid));
			ta_bmp = (ta_bmp | (op << tid));
			SCB_PROPTXTSTATUS_SETTIM(scb, ta_bmp);
#if defined(BCMPCIEDEV) && defined(PROP_TXSTATUS)
			if (BSSCFG_AP(scb->bsscfg) || BSSCFG_IS_TDLS(scb->bsscfg) ||
				(AIBSS_ENAB(wlc->pub) && BSSCFG_IBSS(scb->bsscfg))) {
				ret = wlc_apps_pvb_chkupdate_from_host(wlc, scb, op);
				if (!ret) {
					ta_bmp = (ta_bmp & ~(0x1 << tid));
					SCB_PROPTXTSTATUS_SETTIM(scb, ta_bmp);
				}
			}
#endif // endif
			break;
		}
	}
	return ret;
}
uint16
wlc_flow_ring_get_scb_handle(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *da)
{
	struct scb *scb;
	uint16	ret = 0xff;

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, da);

	if (!scb || !scb->bsscfg)
		return ret;

	if (BSSCFG_AP(scb->bsscfg) || BSSCFG_AWDL(wlc, scb->bsscfg) ||
		BSSCFG_IS_TDLS(scb->bsscfg) || (AIBSS_ENAB(wlc->pub) &&
		BSSCFG_IBSS(scb->bsscfg))) {
		ret = scb->mac_address_handle;
		if (BSSCFG_AP(scb->bsscfg) || BSSCFG_IS_TDLS(scb->bsscfg) ||
			(AIBSS_ENAB(wlc->pub) && BSSCFG_IBSS(scb->bsscfg) &&
			!SCB_ISMULTI(scb))) {
			ret |= SCBHANDLE_INFORM_PKTPEND_MASK;
			if (!SCB_ISMULTI(scb) && SCB_PS(scb))
				ret |= SCBHANDLE_PS_STATE_MASK;
		}
	}
	return ret;
}

void wlc_flush_flowring_pkts(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr,
	uint16 flowid, uint8 tid)
{
	struct pktq tmp_q;
	void *pkt;
	int prec;
	struct scb *scb;
	wlc_txq_info_t *tx_q;

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, addr);

	if (scb && BSSCFG_AP(SCB_BSSCFG(scb))) {
		wlc_apps_ps_flush_flowid(wlc, scb, flowid);
	}
#ifdef WLNAR
	if (scb && wlc->nar_handle) {
		wlc_nar_flush_flowid_pkts(wlc->nar_handle, scb, flowid);
	}
#endif // endif
#ifdef WLAMPDU
	if (scb && SCB_AMPDU(scb)) {
		wlc_ampdu_flush_flowid_pkts(wlc, scb, flowid);
	}
#endif // endif
	pktq_init(&tmp_q, WLC_PREC_COUNT, PKTQ_LEN_MAX);

	/* De-queue the packets from all txq and free them for the flow ring pkts */
	for (tx_q = wlc->tx_queues; tx_q != NULL; tx_q = tx_q->next) {
		while ((pkt = pktq_deq(&tx_q->q, &prec))) {
			if (PKTISTXFRAG(wlc->osh, pkt) &&
				(flowid == PKTFRAGFLOWRINGID(wlc->osh, pkt))) {
				PKTFREE(wlc->pub->osh, pkt, TRUE);
				continue;
			}
			pktq_penq(&tmp_q, prec, pkt);
		}
		/* Enqueue back rest of the packets */
		while ((pkt = pktq_deq(&tmp_q, &prec))) {
			pktq_penq(&tx_q->q, prec, pkt);
		}
	}
}

#if defined(BCMPCIEDEV)
uint32
wlc_flow_ring_reset_weight(wlc_info_t *wlc, struct wlc_if *wlcif,
	uint8 *da, uint8 fl)
{
	struct scb *scb;
	ratespec_t rspec = 0;
	uint32 pktlen_avg = 0;
	uint32 weight_avg = 0;

	ASSERT(fl < FLOWRING_PER_SCB_MAX);

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, da);
	if (scb != NULL && scb->bsscfg != NULL) {
		/* Reseting moving average packet length to default */
		RAVG_INIT(TXPKTLEN_RAVG(scb, fl), (ETHER_MAX_DATA/2), RAVG_EXP_PKT);
		pktlen_avg = RAVG_AVG(TXPKTLEN_RAVG(scb, fl));

		rspec = wlc_ravg_get_scb_cur_rspec(wlc, scb);

		/* Reseting moving average weight to default */
		if (rspec > 0) {
			uint32 weight = wlc_scb_calc_weight(pktlen_avg,
				RSPEC2RATE(rspec), RSPEC_ISLEGACY(rspec));
			RAVG_INIT(WEIGHT_RAVG(scb, fl), weight, RAVG_EXP_WGT);
			weight_avg = RAVG_AVG(WEIGHT_RAVG(scb, fl));
		}
	}
	return weight_avg;
}

/* Updating weight of all flowrings of given scb to the pciedev bus layer.
 * Called from WLC module watchdog.
 */
BCMFASTPATH void
wlc_scb_upd_all_flr_weight(wlc_info_t *wlc, struct scb *scb)
{
	if (BCMPCIEDEV_ENAB(wlc->pub)) {
		uint32 avg_weight = 0;
		uint8 fl;
		for (fl = 0; fl < FLOWRING_PER_SCB_MAX; fl++) {
			avg_weight = RAVG_AVG(WEIGHT_RAVG(scb, fl));
			if (avg_weight > 0)
				wlfc_upd_flr_weigth(wlc->wl, scb->mac_address_handle, fl,
				(void*)&avg_weight);
		}
	}
}
#endif /* BCMPCIEDEV */
#endif /* PROP_TXSTATUS */

#ifdef WLTAF
#define WLTAF_TS2_EBOS_FACTOR           5
#define WLTAF_TS2_ATOS_FACTOR           0
#define WLTAF_TS2_ATOS2_FACTOR		6

/*
 * TS2 shaper will allow EBOS/ATOS to release more data than ATOS2
 * ATOS2 configuration causes more traffic to be released from other traffic stream.
 * EBOS configuration forces more traffic to released from EBOS traffic stream.
 */
BCMFASTPATH uint32
wlc_ts2_traffic_shaper(struct scb *scb, uint32 weight)
{

	/* increase EBOS stream priority */
	if (SCB_TS_EBOS(scb))
		weight = (weight >> WLTAF_TS2_EBOS_FACTOR);
	/* default - no explicit shaping */
	if (SCB_TS_ATOS(scb))
		weight = (weight >> WLTAF_TS2_ATOS_FACTOR);
	/* lower ATOS2 stream priority */
	if (SCB_TS_ATOS2(scb))
		weight = (weight << WLTAF_TS2_ATOS2_FACTOR);

	if (!weight) {
		/* assign a minimum weight for ebos/atos streams */
		weight = 1;
	}

	return weight;
}
#endif /* WLTAF */

#if defined(BCMPCIEDEV)
/* Calculating the weight based on average packet length.
 * Adding weight into the moving average buffer.
 */
BCMFASTPATH void
wlc_ravg_add_weight(wlc_info_t *wlc, struct scb *scb, int fl,
	ratespec_t rspec)
{
	uint32 weight = 0;
	uint32 avg_pktlen = 0;
#ifdef WLTAF
	struct wlc_taf_info* taf_handle = wlc->taf_handle;
	bool taf_enable = wlc_taf_enabled(taf_handle);
#endif // endif
	ASSERT(fl < FLOWRING_PER_SCB_MAX);

	/* calculating the average packet length  */
	avg_pktlen = RAVG_AVG(TXPKTLEN_RAVG(scb, fl));

	/* calculating the weight based on avg packet length and rate spec */
	weight = wlc_scb_calc_weight(avg_pktlen, RSPEC2RATE(rspec),
		RSPEC_ISLEGACY(rspec));
#ifdef WLTAF
	if (taf_enable) {
		/* apply ts2 traffic shaper to weight */
		weight = wlc_ts2_traffic_shaper(scb, weight);
	}
#endif // endif
	/* adding weight into the moving average buffer */
	RAVG_ADD(WEIGHT_RAVG(scb, fl), weight);
}

BCMFASTPATH ratespec_t
wlc_ravg_get_scb_cur_rspec(wlc_info_t *wlc, struct scb *scb)
{
	ratespec_t cur_rspec = 0;
	if (SCB_ISMULTI(scb) || SCB_INTERNAL(scb)) {
		if (RSPEC_ACTIVE(wlc->band->mrspec_override))
			cur_rspec = wlc->band->mrspec_override;
		else
			cur_rspec = scb->rateset.rates[0];
	} else {
		cur_rspec = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
	}

	return cur_rspec;
}
#endif /* BCMPCIEDEV */

uint32 BCMFASTPATH
wlc_scb_dot11hdrsize(struct scb *scb)
{
	wsec_key_t *scb_key;
	uint32 len = DOT11_MAC_HDR_LEN + DOT11_FCS_LEN;
	uint32 scb_flags, scb_flags3;

#if defined(BCM_HOST_MEM_SCBTAG)
	if (SCBTAG_IS_CACHED(scb)) {       /* Cached access */
		scb_flags  = SCBTAG_G_FLAGS;   /* SCBTAG_FLAGS() */
		scb_flags3 = SCBTAG_G_FLAGS3;  /* SCBTAG_FLAGS3() */
		scb_key    = SCBTAG_G_KEY;     /* SCBTAG_KEY() */
		SCBTAG_PMSTATS_ADD(rhits, 3);
	}
	else
#endif /* BCM_HOST_MEM_SCBTAG */
	{
		scb_flags  = SCB_FLAGS(scb);
		scb_flags3 = SCB_FLAGS3(scb);
		scb_key    = SCB_KEY(scb);
	}

	if (SCBF_QOS(scb_flags))
		len += DOT11_QOS_LEN;

	if (SCBF_A4_DATA(scb_flags3))
		len += ETHER_ADDR_LEN;

	if (scb_key) {
		len += scb_key->iv_len;
		len += scb_key->icv_len;
		if (scb_key->algo == CRYPTO_ALGO_TKIP)
			len += TKIP_MIC_SIZE;
	}

	return len;
}

bool
wlc_scbfree(wlc_info_t *wlc, struct scb *scbd)
{
	struct scb_info *remove = SCBINFO(scbd);
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	uint i;
	uint8 prio;
#ifdef WL_SCB_INDIRECT_ACCESS
	struct scb **scb_addr;
#endif /* WL_SCB_INDIRECT_ACCESS */

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */

	if (scbd->permanent)
		return FALSE;

	/* Return if SCB is already being deleted else mark it */
	if (scbd->flags & SCB_PENDING_FREE)
		return FALSE;

	scbd->flags |= SCB_PENDING_FREE;

#ifdef INT_SCB_OPT
	/* no other cleanups are needed for internal scb */
	if (SCB_INTERNAL(scbd)) {
		goto free;
	}
#endif // endif

	/* free the per station key if one exists */
	if (scbd->key) {
		WL_WSEC(("wl%d: %s: deleting pairwise key for %s\n", wlc->pub->unit,
		        __FUNCTION__, bcm_ether_ntoa(&scbd->ea, eabuf)));
		ASSERT(!bcmp((char*)&scbd->key->ea, (char*)&scbd->ea, ETHER_ADDR_LEN));
		wlc_key_scb_delete(wlc, scbd);

	}

	for (i = 0; i < scbstate->ncubby; i++) {
		uint j = scbstate->ncubby - 1 - i;
		cubby_info = &scbstate->cubby_info[j];
		if (cubby_info->fn_deinit)
			cubby_info->fn_deinit(cubby_info->context, scbd);
	}

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		/* release MAC handle back to the pool, if applicable */
		if (scbd->mac_address_handle) {
			wlfc_MAC_table_update(wlc->wl, &scbd->ea.octet[0],
				WLFC_CTL_TYPE_MACDESC_DEL,
				scbd->mac_address_handle, ((scbd->bsscfg->wlcif == NULL) ?
				0 : scbd->bsscfg->wlcif->index));
			wlfc_release_MAC_descriptor_handle(wlc->wlfc_data,
				scbd->mac_address_handle);
			WLFC_DBGMESG(("STA: MAC-DEL for [%02x:%02x:%02x:%02x:%02x:%02x], "
				"handle: [%d], if:%d, t_idx:%d..\n",
				scbd->ea.octet[0], scbd->ea.octet[1], scbd->ea.octet[2],
				scbd->ea.octet[3], scbd->ea.octet[4], scbd->ea.octet[5],
				scbd->mac_address_handle,
				((scbd->bsscfg->wlcif == NULL) ? 0 : scbd->bsscfg->wlcif->index),
				WLFC_MAC_DESC_GET_LOOKUP_INDEX(scbd->mac_address_handle)));
		}
	}
#endif /* PROP_TXSTATUS */

#ifdef AP
	/* free any leftover authentication state */
	if (scbd->challenge) {
		MFREE(wlc->osh, scbd->challenge, 2 + scbd->challenge[1]);
		scbd->challenge = NULL;
	}
	/* free WDS state */
	if (scbd->wds != NULL) {
#ifdef AIRTIES_MESH
		/* process event queue */
		wlc_eventq_flush(wlc->eventq);
#endif /* AIRTIES_MESH */
		if (scbd->wds->wlif) {
			wlc_if_event(wlc, WLC_E_IF_DEL, scbd->wds);
			wl_del_if(wlc->wl, scbd->wds->wlif);
			scbd->wds->wlif = NULL;
			SCB_DWDS_DEACTIVATE(scbd);
		}
		wlc_wlcif_free(wlc, wlc->osh, scbd->wds);
		scbd->wds = NULL;
	}
	/* free wpaie if stored */
	if (scbd->wpaie) {
		MFREE(wlc->osh, scbd->wpaie, scbd->wpaie_len);
		scbd->wpaie_len = 0;
		scbd->wpaie = NULL;
	}
#endif /* AP */

	/* free any frame reassembly buffer */
	for (prio = 0; prio < NUMPRIO; prio++) {
		if (scbd->fragbuf[prio]) {
			PKTFREE(wlc->osh, scbd->fragbuf[prio], FALSE);
			scbd->fragbuf[prio] = NULL;
			scbd->fragresid[prio] = 0;
		}
	}

#ifdef AP
	/* mark the aid unused */
	if (scbd->aid) {
		ASSERT(AID2AIDMAP(scbd->aid) < wlc->pub->tunables->maxscb);
		clrbit(scbd->bsscfg->aidmap, AID2AIDMAP(scbd->aid));
	}
#endif /* AP */

	scbd->state = 0;

#if defined(PKTC) || defined(PKTC_DONGLE)
	/* Clear scb pointer in rfc */
	wlc_scb_pktc_disable(scbd);
#endif // endif

#ifndef INT_SCB_OPT
	if (SCB_INTERNAL(scbd)) {
		goto free;
	}
#endif // endif
	if (!ETHER_ISMULTI(scbd->ea.octet)) {
		wlc_scb_hash_del(wlc, scbd, remove->band->bandunit, SCB_BSSCFG(scbd));
	}

	/* delete it from the link list */
	wlc_scb_list_del(wlc, scbd, SCB_BSSCFG(scbd));

	/* update total allocated scb number */
	scbstate->nscb--;

free:
#ifdef WL_SCB_INDIRECT_ACCESS
	/* Release SCB pointer storage */
	scb_addr = (struct scb **)scbd->scb_addr;
	ASSERT(scb_addr);

	scbd->scb_addr = NULL;
	*scb_addr = NULL;
#endif /*  WL_SCB_INDIRECT_ACCESS */

#ifdef BCM_HOST_MEM_SCB
	WL_INFORM(("%s SCB_HOST[0x%x] @ %p\n", __FUNCTION__,
		SCB_HOST(remove->scbpub), (void *)(remove->scbpub)));

	if (SCB_ALLOC_ENAB(wlc->pub) && remove->scbpub && SCB_HOST(remove->scbpub))
		 wlc_scbadd_free(scbstate, remove);
	else
		/* free scb memory */
		wlc_scb_freemem(scbstate, remove);
#else
#ifdef SCBFREELIST
		wlc_scbadd_free(scbstate, remove);
#else
		/* free scb memory */
		wlc_scb_freemem(scbstate, remove);
#endif /* SCBFREELIST */
#endif /* BCM_HOST_MEM_SCB */

	return TRUE;
}

static void
wlc_scb_list_add(wlc_info_t *wlc, struct scb_info *scbinfo, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);

	SCBSANITYCHECK((scb_cfg)->scb);

	/* update scb link list */
	scbinfo->next = SCBINFO(scb_cfg->scb);
#ifdef MACOSX
	scbinfo->next_copy = scbinfo->next;
#endif // endif
	scb_cfg->scb = scbinfo->scbpub;
}

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
void wlc_scb_list_dump(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{

	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

	struct scb_info *scbinfo;

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);

	if (scb_cfg->scb == NULL) return;
	scbinfo = SCBINFO(scb_cfg->scb)->next;
	while (scbinfo) {
		scbinfo = scbinfo->next;
	}

}
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */

static void
wlc_scb_list_del(wlc_info_t *wlc, struct scb *scbd, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbd);

	ASSERT(bsscfg != NULL);

	/* delete it from the link list */

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	scbinfo = SCBINFO(scb_cfg->scb);
	if (scbinfo == remove) {
		scb_cfg->scb = wlc_scb_getnext(scbd);
	} else {
		while (scbinfo) {
			SCBSANITYCHECK(scbinfo->scbpub);
			if (scbinfo->next == remove) {
				scbinfo->next = remove->next;
#ifdef MACOSX
				scbinfo->next_copy = scbinfo->next;
#endif // endif
				break;
			}
			scbinfo = scbinfo->next;
		}
		ASSERT(scbinfo != NULL);
	}
}

/** free all scbs of a bsscfg */
void
wlc_scb_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm)
{
	struct scb_iter scbiter;
	struct scb *scb;

	if (wlc->scbstate == NULL)
		return;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->permanent) {
			if (!perm)
				continue;
			scb->permanent = FALSE;
		}
		wlc_scbfree(wlc, scb);
	}
}

static struct scb *
wlc_scbvictim(wlc_info_t *wlc)
{
	uint oldest;
	struct scb *scb;
	struct scb *oldscb;
	uint now, age;
	struct scb_iter scbiter;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wlc_bsscfg_t *bsscfg = NULL;

#ifdef AP
	/* search for an unauthenticated scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb->permanent && (scb->state == UNAUTHENTICATED))
			return scb;
	}
#endif /* AP */

	/* free the oldest scb */
	now = wlc->pub->now;
	oldest = 0;
	oldscb = NULL;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bsscfg = SCB_BSSCFG(scb);
		ASSERT(bsscfg != NULL);
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS && SCB_ASSOCIATED(scb))
			continue;
		if (!scb->permanent && ((age = (now - scb->used)) >= oldest)) {
			oldest = age;
			oldscb = scb;
		}
	}
	/* handle extreme case(s): all are permanent ... or there are no scb's at all */
	if (oldscb == NULL)
		return NULL;

#ifdef AP
	bsscfg = SCB_BSSCFG(oldscb);

	if (BSSCFG_AP(bsscfg)) {
		/* if the oldest authenticated SCB has only been idle a short time then
		 * it is not a candidate to reclaim
		 */
		if (oldest < SCB_SHORT_TIMEOUT)
			return NULL;

		/* notify the station that we are deauthenticating it */
		(void)wlc_senddeauth(wlc, bsscfg, oldscb, &oldscb->ea, &bsscfg->BSSID,
		                     &bsscfg->cur_etheraddr, DOT11_RC_INACTIVITY);
		wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &oldscb->ea,
		              DOT11_RC_INACTIVITY, 0);
	}
#endif /* AP */

	WL_ASSOC(("wl%d: %s: relcaim scb %s, idle %d sec\n",  wlc->pub->unit, __FUNCTION__,
	          bcm_ether_ntoa(&oldscb->ea, eabuf), oldest));

	return oldscb;
}

#if defined(PKTC) || defined(PKTC_DONGLE)
static void
wlc_scb_pktc_enable(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = bsscfg->wlc;

	SCB_PKTC_DISABLE(scb);

	/* XXX For now don't enable chaining for following configs.
	 * Will enable as and when functionality is added.
	 */
	if (wlc->wet && BSSCFG_STA(bsscfg))
		return;

	/* No chaining for wds, non qos, non ampdu stas */
	if (SCB_A4_DATA(scb) || !SCB_QOS(scb) || !SCB_WME(scb) || !SCB_AMPDU(scb))
		return;

	if (!SCB_ASSOCIATED(scb) && !SCB_AUTHORIZED(scb))
		return;

#ifdef PKTC_DONGLE
	if (BSS_TDLS_BUFFER_STA(scb->bsscfg))
		return;
#endif // endif

	WL_NONE(("wl%d: auth %d openshared %d WPA_auth %d wsec 0x%x "
	         "scb wsec 0x%x scb key %p algo %d\n",
	         wlc->pub->unit, bsscfg->auth, bsscfg->openshared,
	         bsscfg->WPA_auth, bsscfg->wsec, scb->wsec, scb->key,
	         scb->key ? scb->key->algo : 0));

	/* Enable packet chaining for open auth or wpa2 aes only for now */
	if (((bsscfg->WPA_auth == WPA_AUTH_DISABLED) && !WSEC_WEP_ENABLED(bsscfg->wsec)) ||
	    (WSEC_ENABLED(bsscfg->wsec) && WSEC_AES_ENABLED(scb->wsec) &&
	    (scb->key != NULL) && (scb->key->algo == CRYPTO_ALGO_AES_CCM) &&
	    !WLC_SW_KEYS(bsscfg->wlc, bsscfg)))
		SCB_PKTC_ENABLE(scb);

	return;
}

static void
wlc_scb_pktc_disable(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);

	if (bsscfg) {
		bool cidx;
		wlc_info_t *wlc = bsscfg->wlc;
		/* Invalidate rfc entry if scb is in it */
		cidx = (BSSCFG_STA(bsscfg) && !(SCB_DWDS_CAP(scb) || SCB_MAP_CAP(scb))) ? 0 : 1;
		if (wlc->pktc_info->rfc[cidx].scb == scb) {
			WL_NONE(("wl%d: %s: Invalidate rfc %d before freeing scb %p\n",
			         wlc->pub->unit, __FUNCTION__, cidx, scb));
			wlc->pktc_info->rfc[cidx].scb = NULL;
		}

#if defined(DWDS)
		/* For DWDS/MAP SCB's, if capabilities are reset before cleaning up chaining_info
		 * then wrong cidx is used to clean pktc_info->rfc.
		 * For sanity, checking for SCB in all entries.
		 */
		if (wlc->pktc_info->rfc[cidx ^ 1].scb == scb) {
			WL_ERROR(("wl%d: %s: ERROR: SCB capabilities are RESET\n",
				wlc->pub->unit, __FUNCTION__));
			WL_NONE(("wl%d: %s: Invalidate rfc %d before freeing scb %p\n",
			         wlc->pub->unit, __FUNCTION__, cidx, scb));
			wlc->pktc_info->rfc[cidx].scb = NULL;
		}
#endif /* DWDS */
	}

	SCB_PKTC_DISABLE(scb);
}
#endif /* PKTC || PKTC_DONGLE */

/** "|" operation. */
void
wlc_scb_setstatebit(struct scb *scb, uint8 state)
{
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	scb_module_t *scbstate;
	uint8	oldstate;

	WL_NONE(("set state %x\n", state));
	ASSERT(scb != NULL);

	bsscfg = SCB_BSSCFG(scb);
	wlc = bsscfg->wlc;
	scbstate = wlc->scbstate;
	oldstate = scb->state;

	if (state & AUTHENTICATED)
	{
		scb->state &= ~PENDING_AUTH;
	}
	if (state & ASSOCIATED)
	{
		ASSERT((scb->state | state) & AUTHENTICATED);
		scb->state &= ~PENDING_ASSOC;
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	if (state & AUTHORIZED)
	{
		if (!((scb->state | state) & ASSOCIATED) && !SCB_LEGACY_WDS(scb) &&
#ifdef WLDPT
		    !BSSCFG_IS_DPT(scb->bsscfg) &&
#endif // endif
		    !SCB_IS_IBSS_PEER(scb)) {
			char eabuf[ETHER_ADDR_STR_LEN];
			WL_ASSOC(("wlc_scb : authorized %s is not a associated station, "
				"state = %x\n", bcm_ether_ntoa(&scb->ea, eabuf),
				scb->state));
		}
	}
#endif /* BCMDBG || WLMSG_ASSOC */

	scb->state |= state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));

#if defined(PKTC) || defined(PKTC_DONGLE)
	/* When transitioning to ASSOCIATED/AUTHORIZED state try if we can
	 * enable packet chaining for this SCB.
	 */
	if (SCB_BSSCFG(scb))
		wlc_scb_pktc_enable(scb);
#endif // endif

	if (oldstate != scb->state)
	{
		scb_state_upd_data_t data;
		data.scb = scb;
		data.oldstate = oldstate;
		bcm_notif_signal(scbstate->scb_state_notif_hdl, &data);
	}
}

/** "& ~" operation */
void
wlc_scb_clearstatebit(struct scb *scb, uint8 state)
{
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	scb_module_t *scbstate;
	uint8	oldstate;

	ASSERT(scb != NULL);
	WL_NONE(("clear state %x\n", state));
	bsscfg = SCB_BSSCFG(scb);
	wlc = bsscfg->wlc;
	scbstate = wlc->scbstate;
	oldstate = scb->state;
	scb->state &= ~state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
#if defined(PKTC) || defined(PKTC_DONGLE)
	/* Clear scb pointer in rfc */
	wlc_scb_pktc_disable(scb);
#endif // endif
	if (oldstate != scb->state)
	{
		scb_state_upd_data_t data;
		data.scb = scb;
		data.oldstate = oldstate;
		bcm_notif_signal(scbstate->scb_state_notif_hdl, &data);
	}
}

/**
 * "|" operation . idx = position of the bsscfg in the wlc array of multi ssids.
 */
void
wlc_scb_setstatebit_bsscfg(struct scb *scb, uint8 state, int idx)
{
	ASSERT(scb != NULL);
	WL_NONE(("set state : %x   bsscfg idx : %d\n", state, idx));
	if (state & ASSOCIATED)
	{

		ASSERT(SCB_AUTHENTICATED_BSSCFG(scb, idx));
		/* clear all bits (idx is set below) */
		memset(&scb->auth_bsscfg, 0, SCB_BSSCFG_BITSIZE);
		scb->state &= ~PENDING_ASSOC;
	}

	if (state & AUTHORIZED)
	{
		ASSERT(SCB_ASSOCIATED_BSSCFG(scb, idx));
	}
	setbit(scb->auth_bsscfg, idx);
	scb->state |= state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
}

/**
 * "& ~" operation .
 * idx = position of the bsscfg in the wlc array of multi ssids.
 */
void
wlc_scb_clearstatebit_bsscfg(struct scb *scb, uint8 state, int idx)

{
	int i;
	ASSERT(scb != NULL);
	WL_NONE(("clear state : %x   bsscfg idx : %d\n", state, idx));
	/*
	   any clear of a stable state should lead to clear a bit
	   Warning though : this implies that, if we want to switch from
	   associated to authenticated, the clear happens before the set
	   otherwise this bit will be clear in authenticated state.
	*/
	if ((state & AUTHENTICATED) || (state & ASSOCIATED) || (state & AUTHORIZED))
	{
		clrbit(scb->auth_bsscfg, idx);
	}
	/* quik hack .. clear first ... */
	scb->state &= ~state;
	for (i = 0; i < SCB_BSSCFG_BITSIZE; i++)
	{
		/* reset if needed */
		if (scb->auth_bsscfg[i])
		{
			scb->state |= state;
			break;
		}
	}
}

/** reset all state. */
void
wlc_scb_resetstate(struct scb *scb)
{
	WL_NONE(("reset state\n"));
	ASSERT(scb != NULL);
	memset(&scb->auth_bsscfg, 0, SCB_BSSCFG_BITSIZE);
	scb->state = 0;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
}

/** set/change bsscfg */
void
wlc_scb_set_bsscfg(struct scb *scb, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_t *oldcfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc;
	uint *invtxc;

	ASSERT(cfg != NULL);

	wlc = cfg->wlc;

	scb->bsscfg = cfg;

	/* when assigning the owner the first time or when assigning a different owner */
	if (oldcfg == NULL || oldcfg != cfg) {
		wlcband_t *band = wlc_scbband(scb);
		wlc_rateset_t *rs;

		/* changing bsscfg */
		if (oldcfg != NULL) {
			/* delete scb from hash table and scb list of old bsscfg */
			wlc_scb_hash_del(wlc, scb, band->bandunit, oldcfg);
			wlc_scb_list_del(wlc, scb, oldcfg);
			/* add scb to hash table and scb list of new bsscfg */
			wlc_scb_hash_add(wlc, scb, band->bandunit, cfg);
			wlc_scb_list_add(wlc, SCBINFO(scb), cfg);
		}

		/* flag the scb is used by IBSS */
		if (cfg->BSS)
			wlc_scb_clearstatebit(scb, TAKEN4IBSS);
		else {
			wlc_scb_resetstate(scb);
			if (!BSSCFG_IS_DPT(cfg) && !BSSCFG_IS_TDLS(cfg))
				wlc_scb_setstatebit(scb, TAKEN4IBSS);
		}
		/* invalidate txc */
		if (WLC_TXC_ENAB(wlc) &&
		    (invtxc = wlc_txc_inv_ptr(wlc->txc, scb)) != NULL)
			*invtxc = 0;

		/* use current, target, or per-band default rateset? */
		if (wlc->pub->up &&
#ifdef WLAWDL
			!BSSCFG_AWDL(wlc, cfg) &&
#endif // endif
			wlc_valid_chanspec(wlc->cmi, cfg->target_bss->chanspec))
			if (cfg->associated)
				rs = &cfg->current_bss->rateset;
			else
				rs = &cfg->target_bss->rateset;
		else
			rs = &band->defrateset;

		/*
		 * Initialize the per-scb rateset:
		 * - if we are AP, start with only the basic subset of the
		 *	network rates.  It will be updated when receive the next
		 *	probe request or association request.
		 * - if we are IBSS and gmode, special case:
		 *	start with B-only subset of network rates and probe for ofdm rates
		 * - else start with the network rates.
		 *	It will be updated on join attempts.
		 */
		/* initialize the scb rateset */
		if (BSSCFG_AP(cfg)) {
			uint8 mcsallow = 0;
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_OFDM,
				                       RATE_MASK, wlc_get_mcsallow(wlc, cfg));
			else
#endif // endif
			/* XXX Does not match with the comment above. Remove the
			 * HT rates and possibly OFDM rates if not needed. If there
			 * is a valid reason to add the HT rates, then check if we have to
			 * add VHT rates as well.
			 */
			if (BSS_N_ENAB(wlc, cfg))
				mcsallow = WLC_MCS_ALLOW;
			wlc_rateset_filter(rs, &scb->rateset, TRUE, WLC_RATES_CCK_OFDM,
			                   RATE_MASK, mcsallow);
		}
		else if (!cfg->BSS && band->gmode) {
			wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK,
			                   RATE_MASK, 0);
			/* if resulting set is empty, then take all network rates instead */
			if (scb->rateset.count == 0)
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK_OFDM,
				                   RATE_MASK, 0);
		}
		else {
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_OFDM,
				                   RATE_MASK, wlc_get_mcsallow(wlc, cfg));
			else
#endif // endif
			wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK_OFDM,
			                   RATE_MASK, 0);
		}

		if (!SCB_INTERNAL(scb)) {
			wlc_scb_ratesel_init(wlc, scb);
#ifdef STA
			/* send ofdm rate probe */
			if (BSSCFG_STA(cfg) && !cfg->BSS &&
#ifdef WLAWDL
				!BSSCFG_AWDL(wlc, cfg) &&
#endif // endif
			    band->gmode && wlc->pub->up)
				wlc_rateprobe(wlc, cfg, &scb->ea, WLC_RATEPROBE_RATE);
#endif  /* STA */
		}
	}
}

static void
wlc_scb_bsscfg_reinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	uint prev_count;
	const wlc_rateset_t *rs;
	wlcband_t *band;
	struct scb *scb;
	struct scb_iter scbiter;
	bool cck_only;
	bool reinit_forced;

	WL_INFORM(("wl%d: %s: bandunit 0x%x phy_type 0x%x gmode 0x%x\n", wlc->pub->unit,
		__FUNCTION__, wlc->band->bandunit, wlc->band->phytype, wlc->band->gmode));

	/* sanitize any existing scb rates against the current hardware rates */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		/* XXX SCB : should the following be done only if scb->bandunit matches
		 * wlc->band->bandunit?
		 */
		prev_count = scb->rateset.count;
		/* Keep only CCK if gmode == GMODE_LEGACY_B */
		band = SCBINFO(scb)->band;
		if (BAND_2G(band->bandtype) && (band->gmode == GMODE_LEGACY_B)) {
			rs = &cck_rates;
			cck_only = TRUE;
		} else {
			rs = &band->hw_rateset;
			cck_only = FALSE;
		}
		if (!wlc_rate_hwrs_filter_sort_validate(&scb->rateset, rs, FALSE,
			wlc->stf->txstreams)) {
			/* continue with default rateset.
			 * since scb rateset does not carry basic rate indication,
			 * clear basic rate bit.
			 */
			WL_RATE(("wl%d: %s: invalid rateset in scb 0x%p bandunit 0x%x "
				"phy_type 0x%x gmode 0x%x\n", wlc->pub->unit, __FUNCTION__,
				scb, band->bandunit, band->phytype, band->gmode));
#ifdef BCMDBG
			wlc_rateset_show(wlc, &scb->rateset, &scb->ea);
#endif // endif

			wlc_rateset_default(&scb->rateset, &band->hw_rateset,
			                    band->phytype, band->bandtype, cck_only, RATE_MASK,
			                    wlc_get_mcsallow(wlc, scb->bsscfg),
			                    CHSPEC_WLC_BW(scb->bsscfg->current_bss->chanspec),
			                    wlc->stf->txstreams);
			reinit_forced = TRUE;
		}
		else
			reinit_forced = FALSE;

		/* if the count of rates is different, then the rate state
		 * needs to be reinitialized
		 */
		if (reinit_forced || (scb->rateset.count != prev_count))
			wlc_scb_ratesel_init(wlc, scb);

		WL_RATE(("wl%d: %s: bandunit 0x%x, phy_type 0x%x gmode 0x%x. final rateset is\n",
			wlc->pub->unit, __FUNCTION__,
			band->bandunit, band->phytype, band->gmode));
#ifdef BCMDBG
		wlc_rateset_show(wlc, &scb->rateset, &scb->ea);
#endif // endif
	}
}

void
wlc_scb_reinit(wlc_info_t *wlc)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_scb_bsscfg_reinit(wlc, bsscfg);
	}
}

static INLINE struct scb* BCMFASTPATH
_wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	int indx;
	struct scb_info *scbinfo;
	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

#if BCM_HOST_MEM_SCBTAG_INUSE
	scbtag_flush();
#endif // endif

	/* All callers of wlc_scbfind() should first be checking to see
	 * if the SCB they're looking for is a BC/MC address.  Because we're
	 * using per bsscfg BCMC SCBs, we can't "find" BCMC SCBs without
	 * knowing which bsscfg.
	 */
	ASSERT(ea && !ETHER_ISMULTI(ea));

	/* search for the scb which corresponds to the remote station ea */
	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	if (scb_cfg) {
		indx = SCBHASHINDEX(scb_cfg->nscbhash, ea->octet);
		scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
			SCBINFO(scb_cfg->scbhash[bandunit][indx]) : NULL);
		for (; scbinfo; scbinfo = scbinfo->hashnext) {
			SCBSANITYCHECK(scbinfo->scbpub);

			if (eacmp((const char*)ea, (const char*)&(scbinfo->scbpub->ea)) == 0)
				break;
		}

		return (scbinfo ? scbinfo->scbpub : NULL);
	}
	return (NULL);
}

/** Find station control block corresponding to the remote id */
struct scb * BCMFASTPATH
wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	struct scb *scb = NULL;

	scb = _wlc_scbfind(wlc, bsscfg, ea, wlc->band->bandunit);

#ifdef WLMCHAN
/* current band could be different, so search again for all scb's */
	if (!scb && MCHAN_ACTIVE(wlc->pub) && NBANDS(wlc) > 1)
		scb = wlc_scbfindband(wlc, bsscfg, ea, OTHERBANDUNIT(wlc));
#endif /* WLMCHAN */
	return scb;
}

/**
 * Lookup station control block corresponding to the remote id.
 * If not found, create a new entry.
 */
static INLINE struct scb *
_wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	struct scb *scb;
	struct wlcband *band;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char sa[ETHER_ADDR_STR_LEN];
#endif // endif

	/* Don't allocate/find a BC/MC SCB this way. */
	ASSERT(!ETHER_ISMULTI(ea));
	if (ETHER_ISMULTI(ea))
		return NULL;

	/* apply mac filter */
	switch (wlc_macfltr_addr_match(wlc->macfltr, bsscfg, ea)) {
	case WLC_MACFLTR_ADDR_DENY:
		WL_ASSOC(("wl%d.%d mac restrict: Denying %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		          bcm_ether_ntoa(ea, sa)));
		return NULL;
	case WLC_MACFLTR_ADDR_NOT_ALLOW:
		WL_ASSOC(("wl%d.%d mac restrict: Not allowing %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		          bcm_ether_ntoa(ea, sa)));
		return NULL;
#ifdef BCMDBG
	case WLC_MACFLTR_ADDR_ALLOW:
		WL_ASSOC(("wl%d.%d mac restrict: Allowing %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		          bcm_ether_ntoa(ea, sa)));
		break;
	case WLC_MACFLTR_ADDR_NOT_DENY:
		WL_ASSOC(("wl%d.%d mac restrict: Not denying %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		          bcm_ether_ntoa(ea, sa)));
		break;
	case WLC_MACFLTR_DISABLED:
		WL_NONE(("wl%d.%d no mac restrict: lookup %s\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		         bcm_ether_ntoa(ea, sa)));
		break;
#endif /* BCMDBG */
	}

	if ((scb = _wlc_scbfind(wlc, bsscfg, ea, bandunit)))
		return (scb);

	/* no scb match, allocate one for the desired bandunit */
	band = wlc->bandstate[bandunit];
	return wlc_userscb_alloc(wlc, bsscfg, ea, band);
}

struct scb *
wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	return (_wlc_scblookup(wlc, bsscfg, ea, wlc->band->bandunit));
}

struct scb *
wlc_scblookupband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	/* assert that the band is the current band, or we are dual band and it is the other band */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	return (_wlc_scblookup(wlc, bsscfg, ea, bandunit));
}

/** Get scb from band */
struct scb * BCMFASTPATH
wlc_scbfindband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	/* assert that the band is the current band, or we are dual band and it is the other band */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	return (_wlc_scbfind(wlc, bsscfg, ea, bandunit));
}

/**
 * Determine if any SCB associated to ap cfg
 * cfg specifies a specific ap cfg to compare to.
 * If cfg is NULL, then compare to any ap cfg.
 */
bool
wlc_scb_associated_to_ap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb;
	bool associated = FALSE;

	ASSERT((cfg == NULL) || BSSCFG_AP(cfg));

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_ASSOCIATED(scb) && BSSCFG_AP(scb->bsscfg)) {
			if ((cfg == NULL) || (cfg == scb->bsscfg)) {
				associated = TRUE;
			}
		}
	}

	return (associated);
}

void wlc_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit,
	wlc_bsscfg_t *bsscfg)
{
	struct scb_info *scbinfo = SCBINFO(scb);

	/* first, del scb from hash table in old band */
	wlc_scb_hash_del(wlc, scb, scb->bandunit, bsscfg);
	/* next add scb to hash table in new band */
	wlc_scb_hash_add(wlc, scb, new_bandunit, bsscfg);
	/* update the scb's band */
	scb->bandunit = (uint)new_bandunit;
	scbinfo->band = wlc->bandstate[new_bandunit];

	return;
}

void wlc_internal_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit)
{
	struct scb_info *scbinfo = SCBINFO(scb);
	/* update the scb's band */
	scb->bandunit = (uint)new_bandunit;
	scbinfo->band = wlc->bandstate[new_bandunit];
}

/**
 * Move the scb's band info.
 * Parameter description:
 *
 * wlc - global wlc_info structure
 * bsscfg - the bsscfg that is about to move to a new chanspec
 * chanspec - the new chanspec the bsscfg is moving to
 *
 */
void
wlc_scb_update_band_for_cfg(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	struct scb_iter scbiter;
	struct scb *scb, *stale_scb;
	int bandunit;
	bool reinit = FALSE;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			bandunit = CHSPEC_WLCBANDUNIT(chanspec);
			if (scb->bandunit != (uint)bandunit) {
				/* We're about to move our scb to the new band.
				 * Check to make sure there isn't an scb entry for us there.
				 * If there is one for us, delete it first.
				 */
				if ((stale_scb = _wlc_scbfind(wlc, bsscfg,
				                      &bsscfg->BSSID, bandunit)) &&
				    (stale_scb->permanent == FALSE)) {
					WL_ASSOC(("wl%d.%d: %s: found stale scb %p on %s band, "
					          "remove it\n",
					          wlc->pub->unit, bsscfg->_idx, __FUNCTION__,
					          stale_scb,
					          (bandunit == BAND_5G_INDEX) ? "5G" : "2G"));
					/* mark the scb for removal */
					stale_scb->stale_remove = TRUE;
				}
				/* Now perform the move of our scb to the new band */
				wlc_scb_switch_band(wlc, scb, bandunit, bsscfg);
				reinit = TRUE;
			}
		}
	}
	/* remove stale scb's marked for removal */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->stale_remove == TRUE) {
			WL_ASSOC(("remove stale scb %p\n", scb));
			scb->stale_remove = FALSE;
			wlc_scbfree(wlc, scb);
		}
	}

	if (reinit) {
		wlc_scb_reinit(wlc);
	}
}

struct scb *
wlc_scbibssfindband(wlc_info_t *wlc, const struct ether_addr *ea, int bandunit,
                    wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	/* assert that the band is the current band, or we are dual band
	 * and it is the other band.
	 */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	FOREACH_IBSS(wlc, idx, cfg) {
		/* Find the bsscfg and scb matching specified peer mac */
		scb = _wlc_scbfind(wlc, cfg, ea, bandunit);
		if (scb != NULL) {
			*bsscfg = cfg;
			break;
		}
	}

	return scb;
}

struct scb *
wlc_scbapfind(wlc_info_t *wlc, const struct ether_addr *ea, wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	*bsscfg = NULL;

	FOREACH_UP_AP(wlc, idx, cfg) {
		/* Find the bsscfg and scb matching specified peer mac */
		scb = wlc_scbfind(wlc, cfg, ea);
		if (scb != NULL) {
			*bsscfg = cfg;
			break;
		}
	}

	return scb;
}

struct scb * BCMFASTPATH
wlc_scbbssfindband(wlc_info_t *wlc, const struct ether_addr *hwaddr,
                   const struct ether_addr *ea, int bandunit, wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	/* assert that the band is the current band, or we are dual band
	 * and it is the other band.
	 */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	*bsscfg = NULL;

	FOREACH_BSS(wlc, idx, cfg) {
		/* Find the bsscfg and scb matching specified hwaddr and peer mac */
		if (eacmp(cfg->cur_etheraddr.octet, hwaddr->octet) == 0) {
			scb = _wlc_scbfind(wlc, cfg, ea, bandunit);
			if (scb != NULL) {
				*bsscfg = cfg;
				break;
			}
		}
	}

	return scb;
}

/**
 * (de)authorize/(de)authenticate single station
 * 'enable' TRUE means authorize, FLASE means deauthorize/deauthenticate
 * 'flag' is AUTHORIZED or AUTHENICATED for the type of operation
 * 'rc' is the reason code for a deauthenticate packet
 */
void
wlc_scb_set_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, bool enable, uint32 flag,
                 int rc)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	void *pkt = NULL;

	if (SCB_LEGACY_WDS(scb)) {
		WL_ERROR(("wl%d.%d %s: WDS=" MACF " enable=%d flag=%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				__FUNCTION__, ETHERP_TO_MACF(&scb->ea), enable, flag));
	}

	if (enable) {
		if (flag == AUTHORIZED) {
			wlc_scb_setstatebit(scb, AUTHORIZED);
			scb->flags &= ~SCB_DEAUTH;

			if (BSSCFG_AP(bsscfg) && wlc_eventq_test_ind(wlc->eventq, WLC_E_AUTHORIZED))
				wlc_bss_mac_event(wlc, bsscfg, WLC_E_AUTHORIZED,
					(struct ether_addr *)&scb->ea,
					WLC_E_AUTHORIZED, 0, 0, 0, 0);

#ifdef WL11N
			if (SCB_MFP(scb) && N_ENAB(wlc->pub) && SCB_AMPDU(scb) &&
				(scb->wsec == AES_ENABLED)) {
				wlc_scb_ampdu_enable(wlc, scb);
			}
#endif /* WL11N */
#ifdef TRAFFIC_MGMT
		if (BSSCFG_AP(bsscfg)) {
			wlc_scb_trf_mgmt(wlc, bsscfg, scb);
		}
#endif // endif
		} else {
			wlc_scb_setstatebit(scb, AUTHENTICATED);
		}
	} else {
		if (flag == AUTHORIZED) {

			wlc_scb_clearstatebit(scb, AUTHORIZED);
		} else {

			if (wlc->pub->up && (SCB_AUTHENTICATED(scb) || SCB_LEGACY_WDS(scb))) {
				if (rc == DOT11_RC_STALE_DETECTION) {
					/* Clear states and mark the scb for deletion. SCB free
					 * will happen from the inactivity timeout context in
					 * wlc_ap_stastimeout()
					 */
					wlc_scb_clearstatebit(scb, AUTHENTICATED | ASSOCIATED
							| AUTHORIZED);
					wlc_scb_setstatebit(scb, MARKED_FOR_DELETION);
				} else {
					pkt = wlc_senddeauth(wlc, bsscfg, scb, &scb->ea, &bsscfg->BSSID,
						&bsscfg->cur_etheraddr, (uint16)rc);
				}
			}
			if (pkt != NULL) {
				wlc_deauth_send_cbargs_t *args;

				if ((args = MALLOC(wlc->osh, sizeof(wlc_deauth_send_cbargs_t)))) {
					bcopy(&scb->ea, &args->ea, sizeof(struct ether_addr));
					args->_idx = WLC_BSSCFG_IDX(bsscfg);
					args->pkt = pkt;
					if (wlc_pcb_fn_register(wlc->pcb,
						wlc_deauth_sendcomplete, (void *)args, pkt))
						WL_ERROR(("wl%d: wlc_scb_set_auth: could not "
							"register callback\n", wlc->pub->unit));
				}
			}
		}
	}
	WL_ASSOC(("wl%d: %s: %s %s%s\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf),
		(enable ? "" : "de"),
		((flag == AUTHORIZED) ? "authorized" : "authenticated")));
}

static void
wlc_scb_hash_add(wlc_info_t *wlc, struct scb *scb, int bandunit, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	int indx;
	struct scb_info *scbinfo;

	ASSERT(bsscfg != NULL);

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, scb->ea.octet);
	scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
	           SCBINFO(scb_cfg->scbhash[bandunit][indx]) : NULL);

	SCBINFO(scb)->hashnext = scbinfo;
#ifdef MACOSX
	SCBINFO(scb)->hashnext_copy = SCBINFO(scb)->hashnext;
#endif // endif

	scb_cfg->scbhash[bandunit][indx] = scb;
}

static void
wlc_scb_hash_del(wlc_info_t *wlc, struct scb *scbd, int bandunit, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	int indx;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbd);

	ASSERT(bsscfg != NULL);

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, scbd->ea.octet);

	/* delete it from the hash */
	scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
	           SCBINFO(scb_cfg->scbhash[bandunit][indx]) : NULL);
	ASSERT(scbinfo != NULL);
	SCBSANITYCHECK(scbinfo->scbpub);
	/* special case for the first */
	if (scbinfo == remove) {
		if (scbinfo->hashnext)
		    SCBSANITYCHECK(scbinfo->hashnext->scbpub);
		scb_cfg->scbhash[bandunit][indx] =
		        (scbinfo->hashnext ? scbinfo->hashnext->scbpub : NULL);
	} else {
		for (; scbinfo; scbinfo = scbinfo->hashnext) {
			SCBSANITYCHECK(scbinfo->hashnext->scbpub);
			if (scbinfo->hashnext == remove) {
				scbinfo->hashnext = remove->hashnext;
#ifdef MACOSX
				scbinfo->hashnext_copy = scbinfo->hashnext;
#endif // endif
				break;
			}
		}
		ASSERT(scbinfo != NULL);
	}
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	uint k, i;
	struct scb *scb;
	char eabuf[ETHER_ADDR_STR_LEN];
	char flagstr[64];
	char flagstr2[64];
	char flagstr3[64];
	char statestr[64];
	struct scb_iter scbiter;
	cubby_info_t *cubby_info;
	scb_module_t *scbstate = (scb_module_t *)context;
#ifdef AP
	char ssidbuf[SSID_FMT_BUF_LEN] = "";
#endif /* AP */

	bcm_bprintf(b, "# of scbs: %u\n", scbstate->nscb);
	bcm_bprintf(b, "# of cubbies: %u, scb size: %u\n",
	            scbstate->ncubby, scbstate->scbtotsize);

	bcm_bprintf(b, "idx  ether_addr\n");
	k = 0;
	FOREACH_BSS_SCB(scbstate, &scbiter, cfg, scb) {
		bcm_format_flags(scb_flags, scb->flags, flagstr, 64);
		bcm_format_flags(scb_flags2, scb->flags2, flagstr2, 64);
		bcm_format_flags(scb_flags3, scb->flags3, flagstr3, 64);
		bcm_format_flags(scb_states, scb->state, statestr, 64);

		bcm_bprintf(b, "%3d%s %s\n", k, (scb->permanent? "*":" "),
			bcm_ether_ntoa(&scb->ea, eabuf));

		bcm_bprintf(b, "     State:0x%02x (%s) Used:%d(%d)\n",
		            scb->state, statestr, scb->used,
		            (int)(scb->used - scbstate->pub->now));
		bcm_bprintf(b, "     Band:%s",
		            ((scb->bandunit == BAND_2G_INDEX) ? BAND_2G_NAME :
		             BAND_5G_NAME));
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags:0x%x", scb->flags);
		if (flagstr[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags2:0x%x", scb->flags2);
		if (flagstr2[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr2);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags3:0x%x", scb->flags3);
		if (flagstr3[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr3);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Cfg:%d(%p)", WLC_BSSCFG_IDX(cfg), cfg);
		if (scb->flags & SCB_AMSDUCAP)
			bcm_bprintf(b, " AMSDU-MTU ht:%d vht:%d", scb->amsdu_ht_mtu_pref,
				wlc_vht_get_scb_amsdu_mtu_pref(cfg->wlc->vhti, scb));
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Prop HT rates support:%d\n",
			(scb->flags2 & SCB2_HT_PROP_RATES_CAP) ? 1: 0);

		if (scb->key) {
			bcm_bprintf(b, "     Key:%d", scb->key->idx);
			bcm_bprintf(b, " Key ID:%d algo:%s length:%d data:",
				scb->key->id,
			        bcm_crypto_algo_name(scb->key->algo),
				scb->key->idx, scb->key->len);
			if (scb->key->len)
				bcm_bprintf(b, "0x");
			for (i = 0; i < scb->key->len; i++)
				bcm_bprintf(b, "%02X", scb->key->data[i]);
			for (i = 0; i < scb->key->len; i++)
				if (!bcm_isprint(scb->key->data[i]))
					break;
			if (i == scb->key->len)
				bcm_bprintf(b, " (%.*s)", scb->key->len, scb->key->data);
			bcm_bprintf(b, "\n");
		}

		wlc_dump_rateset("     rates", &scb->rateset, b);
		bcm_bprintf(b, "\n");

		if (scb->rateset.htphy_membership) {
			bcm_bprintf(b, "     membership %d(b)",
				(scb->rateset.htphy_membership & RATE_MASK));
			bcm_bprintf(b, "\n");
		}
#ifdef AP
		if (BSSCFG_AP(cfg)) {
			bcm_bprintf(b, "     AID:0x%x PS:%d Listen:%d WDS:%d(%p) RSSI:%d",
			               scb->aid, scb->PS, scb->listen, (scb->wds ? 1 : 0),
			               scb->wds, wlc_scb_rssi(scb));
			wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
			bcm_bprintf(b, " BSS %d \"%s\"\n",
			            WLC_BSSCFG_IDX(cfg), ssidbuf);
		}
#endif // endif
#ifdef STA
		if (BSSCFG_STA(cfg)) {
			bcm_bprintf(b, "     MAXSP:%u DEFL:0x%x TRIG:0x%x DELV:0x%x\n",
			            scb->apsd.maxsplen, scb->apsd.ac_defl,
			            scb->apsd.ac_trig, scb->apsd.ac_delv);
		}
#endif // endif

#ifdef WL11N
		if (N_ENAB(scbstate->pub) && SCB_HT_CAP(scb)) {
			wlc_dump_mcsset("     HT mcsset :", &scb->rateset.mcs[0], b);
			bcm_bprintf(b,  "\n     HT capabilites 0x%04x ampdu_params 0x%02x "
			    "mimops_enabled %d mimops_rtsmode %d",
			    scb->ht_capabilities, scb->ht_ampdu_params, scb->ht_mimops_enabled,
			    scb->ht_mimops_rtsmode);
			bcm_bprintf(b, "\n");
		}
		wlc_dump_rclist("     rclist", scb->rclist, scb->rclen, b);
#endif  /* WL11N */
		bcm_bprintf(b,  "     WPA_auth 0x%x wsec 0x%x\n", scb->WPA_auth, scb->wsec);

		wlc_dump_rspec(context, wlc_scb_ratesel_get_primary(cfg->wlc, scb, NULL), b);

#if defined(STA) && defined(DBG_BCN_LOSS)
		bcm_bprintf(b,	"	  last_rx:%d last_rx_rssi:%d last_bcn_rssi: "
			"%d last_tx: %d\n",
			scb->dbg_bcn.last_rx, scb->dbg_bcn.last_rx_rssi, scb->dbg_bcn.last_bcn_rssi,
			scb->dbg_bcn.last_tx);
#endif // endif

		for (i = 0; i < scbstate->ncubby; i++) {
			cubby_info = &scbstate->cubby_info[i];
			if (cubby_info->fn_dump)
				cubby_info->fn_dump(cubby_info->context, scb, b);
		}

		wlc_scb_txpath_dump(cfg->wlc, scb, b);

		k++;
	}

	return;
}

static int
wlc_dump_scb(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_scb_bsscfg_dump(wlc->scbstate, bsscfg, b);
	}

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
	wlc_scbfreelist_dump(wlc->scbstate, b);
#endif /* defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB) */
	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */

void
wlc_scb_sortrates(wlc_info_t *wlc, struct scb *scb)
{
	struct scb_info *scbinfo = SCBINFO(scb);
	wlc_rate_hwrs_filter_sort_validate(&scb->rateset, &scbinfo->band->hw_rateset, FALSE,
		wlc->stf->txstreams);
}

void
BCMINITFN(wlc_scblist_validaterates)(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_scb_sortrates(wlc, scb);
		if (scb->rateset.count == 0)
			wlc_scbfree(wlc, scb);
	}
}

#if defined(AP) || defined(WLDPT) || defined(WLTDLS)
int
wlc_scb_rssi(struct scb *scb)
{
	int rssi = 0, cnt;
	int i;

	for (i = 0, cnt = 0; i < MA_WINDOW_SZ; i++)
		if (scb->rssi_window[i] != WLC_RSSI_INVALID)
		{
			rssi += scb->rssi_window[i];
			cnt++;
		}
	if (cnt > 1) rssi /= cnt;

	return (rssi);
}

int8
wlc_scb_rssi_chain(struct scb *scb, int chain)
{
	int8 rssi_avg = WLC_RSSI_INVALID, cnt;
	int32 rssi = 0;
	int i;

	for (i = 0, cnt = 0; i < MA_WINDOW_SZ; i++) {
		if (scb->rssi_chain[chain][i] != WLC_RSSI_INVALID) {
			rssi += scb->rssi_chain[chain][i];
			cnt++;
		}
	}

	if (cnt >= 1) {
		rssi_avg = rssi/cnt;
	}

	return (rssi_avg);
}

/* return the rssi of last received packet per scb and
 * per antenna chain.
 */
int8
wlc_scb_pkt_rssi_chain(struct scb *scb, int chain)
{
	int last_rssi_index;
	int8 rssi = 0;

	last_rssi_index = MODDEC_POW2(scb->rssi_index, MA_WINDOW_SZ);
	if ((chain >= WL_ANT_IDX_1) && (chain < WL_RSSI_ANT_MAX) &&
		(scb->rssi_chain[chain][last_rssi_index] != WLC_RSSI_INVALID))
		rssi = (int8)scb->rssi_chain[chain][last_rssi_index];

	return rssi;
}

void
wlc_scb_rssi_init(struct scb *scb, int rssi)
{
	int i, j;
	scb->rssi_enabled = 1;
	for (i = 0; i < MA_WINDOW_SZ; i++) {
		scb->rssi_window[i] = rssi;
		for (j = 0; j < WL_RSSI_ANT_MAX; j++)
			scb->rssi_chain[j][i] = rssi;
	}

	scb->rssi_index = 0;
}

/** Enable or disable RSSI update for a particular requestor module */
bool
wlc_scb_rssi_update_enable(struct scb *scb, bool enable, scb_rssi_requestor_t rid)
{
	if (enable) {
		scb->rssi_upd |= (1<<rid);
	} else {
		scb->rssi_upd &= ~(1<<rid);
	}
	return (scb->rssi_upd != 0);
}

#endif /* AP || WLDPT || WLTDLS */

/**
 * Give then tx_fn, return the feature id from txmod_fns array.
 * If tx_fn is NULL, 0 will be returned
 * If entry is not found, it's an ERROR!
 */
static INLINE scb_txmod_t
wlc_scb_txmod_fid(wlc_info_t *wlc, txmod_tx_fn_t tx_fn)
{
	scb_txmod_t txmod;

	for (txmod = TXMOD_START; txmod < TXMOD_LAST; txmod++)
		if (tx_fn == wlc->txmod_fns[txmod].tx_fn)
			return txmod;

	/* Should not reach here */
	ASSERT(txmod < TXMOD_LAST);
	return txmod;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_scb_txpath_dump(wlc_info_t *wlc, struct scb *scb, struct bcmstrbuf *b)
{
	static const char *txmod_names[TXMOD_LAST] = {
		"Start",
		"DPT",
		"TDLS",
		"APPS",
		"Traffic Mgmt",
		"NAR",
		"A-MSDU",
		"A-MPDU",
		"Transmit"
	};
	scb_txmod_t fid, next_fid;

	bcm_bprintf(b, "     Tx Path: ");
	fid = TXMOD_START;
	do {
		next_fid = wlc_scb_txmod_fid(wlc, scb->tx_path[fid].next_tx_fn);
		/* for each txmod print out name and # total pkts held fr all scbs */
		bcm_bprintf(b, "-> %s (allscb pkts=%u)",
			txmod_names[next_fid],
			(wlc->txmod_fns[next_fid].pktcnt_fn) ?
			wlc_txmod_get_pkts_pending(wlc, next_fid) : -1);
		fid = next_fid;
	} while (fid != TXMOD_TRANSMIT && fid != 0);
	bcm_bprintf(b, "\n");
	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */

/**
 * Add a feature to the path. It should not be already on the path and should be configured
 * Does not take care of evicting anybody
 */
void
wlc_scb_txmod_activate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid)
{
	/* Numeric value designating this feature's position in tx_path */
	static const uint8 txmod_position[TXMOD_LAST] = {
		0, /* TXMOD_START */
		1, /* TXMOD_DPT */
		1, /* TXMOD_TDLS */
		6, /* TXMOD_APPS */
		2, /* TXMOD_TRF_MGMT */
		3, /* TXMOD_NAR */
		4, /* TXMOD_AMSDU */
		5, /* TXMOD_AMPDU */
		7, /* TXMOD_TRANSMIT */
	};

	uint curr_mod_position;
	scb_txmod_t prev, next;
	txmod_info_t curr_mod_info = wlc->txmod_fns[fid];

	ASSERT(SCB_TXMOD_CONFIGURED(scb, fid) &&
	       !SCB_TXMOD_ACTIVE(scb, fid));

	curr_mod_position = txmod_position[fid];

	prev = TXMOD_START;

	while ((next = wlc_scb_txmod_fid(wlc, scb->tx_path[prev].next_tx_fn)) != 0 &&
	       txmod_position[next] < curr_mod_position)
		prev = next;

	/* next == 0 indicate this is the first addition to the path
	 * it HAS to be TXMOD_TRANSMIT as it's the one that puts the packet in
	 * txq. If this changes, then assert will need to be removed.
	 */
	ASSERT(next != 0 || fid == TXMOD_TRANSMIT);
	ASSERT(txmod_position[next] != curr_mod_position);

	SCB_TXMOD_SET(scb, prev, fid);
	SCB_TXMOD_SET(scb, fid, next);

	/* invoke any activate notify functions now that it's in the path */
	if (curr_mod_info.activate_notify_fn)
		curr_mod_info.activate_notify_fn(curr_mod_info.ctx, scb);
}

/**
 * Remove a fid from the path. It should be already on the path
 * Does not take care of replacing it with any other feature.
 */
void
wlc_scb_txmod_deactivate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid)
{
	scb_txmod_t prev, next;
	txmod_info_t curr_mod_info = wlc->txmod_fns[fid];

	/* If not active, do nothing */
	if (!SCB_TXMOD_ACTIVE(scb, fid))
		return;

	/* if deactivate notify function is present, call it */
	if (curr_mod_info.deactivate_notify_fn)
		curr_mod_info.deactivate_notify_fn(curr_mod_info.ctx, scb);

	prev = TXMOD_START;

	while ((next = wlc_scb_txmod_fid(wlc, scb->tx_path[prev].next_tx_fn))
	       != fid)
		prev = next;

	SCB_TXMOD_SET(scb, prev, wlc_scb_txmod_fid(wlc, scb->tx_path[fid].next_tx_fn));
	scb->tx_path[fid].next_tx_fn = NULL;
}

#ifdef WLAWDL
void
wlc_scb_awdl_free(struct wlc_info *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->bsscfg && BSSCFG_AWDL(wlc, scb->bsscfg)) {
			scb->permanent = FALSE;
			wlc_scbfree(wlc, scb);
		}
	}
}
struct scb * wlc_scbfind_dualband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct ether_addr *addr)
{
	struct scb *scb;
	scb = wlc_scbfind(wlc, bsscfg, addr);
	if (NBANDS(wlc) == 1)
		return scb;
	if (!scb)
		scb = wlc_scbfindband(wlc, bsscfg, addr, OTHERBANDUNIT(wlc));
	return scb;
}
#endif /* WLAWDL */

int
wlc_scb_save_wpa_ie(wlc_info_t *wlc, struct scb *scb, bcm_tlv_t *ie)
{
	uint ie_len;

	ASSERT(scb != NULL);
	ASSERT(ie != NULL);

	ie_len = TLV_HDR_LEN + ie->len;

	/* Optimization */
	if (scb->wpaie != NULL && ie != NULL &&
	    scb->wpaie_len == ie_len)
		goto cp;

	/* Free old WPA IE if one exists */
	if (scb->wpaie != NULL) {
	        MFREE(wlc->osh, scb->wpaie, scb->wpaie_len);
	        scb->wpaie_len = 0;
	        scb->wpaie = NULL;
	}

	/* Store the WPA IE for later retrieval */
	if ((scb->wpaie = MALLOC(wlc->osh, ie_len)) == NULL) {
		WL_ERROR(("wl%d: %s: unable to allocate memory\n",
		          wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

cp:	/* copy */
	bcopy(ie, scb->wpaie, ie_len);
	scb->wpaie_len = ie_len;

	return BCME_OK;
}

int
wlc_scb_state_upd_register(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg)
{
	bcm_notif_h hdl = wlc->scbstate->scb_state_notif_hdl;

	return bcm_notif_add_interest(hdl, fn, arg);
}

int
wlc_scb_state_upd_unregister(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg)
{
	bcm_notif_h hdl = wlc->scbstate->scb_state_notif_hdl;

	return bcm_notif_remove_interest(hdl, fn, arg);
}

#ifdef WL_CS_RESTRICT_RELEASE
void
wlc_scb_restrict_wd(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->restrict_deadline) {
			scb->restrict_deadline--;
		}
		if (!scb->restrict_deadline) {
			scb->restrict_txwin = 0;
		}
	}
}

void
wlc_scb_restrict_start(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_ISMULTI(scb) && SCB_AMPDU(scb)) {
			scb->restrict_txwin = SCB_RESTRICT_MIN_TXWIN;
			scb->restrict_deadline = SCB_RESTRICT_WD_TIMEOUT + 1;
		}
	}
}
#endif /* WL_CS_RESTRICT_RELEASE */

#ifdef PROP_TXSTATUS
int
wlc_scb_wlfc_entry_add(wlc_info_t *wlc, struct scb *scb)
{
	int err = BCME_OK;

	if (wlc == NULL || scb == NULL) {
		err = BCME_BADARG;
		goto end;
	}

	if (!PROP_TXSTATUS_ENAB(wlc->pub)) {
		err = BCME_UNSUPPORTED;
		goto end;
	}

	/* allocate a handle from bitmap f we haven't already done so */
	if (scb->mac_address_handle == 0)
		scb->mac_address_handle = wlfc_allocate_MAC_descriptor_handle(
			wlc->wlfc_data);
	err = wlfc_MAC_table_update(wlc->wl, &scb->ea.octet[0],
		WLFC_CTL_TYPE_MACDESC_ADD, scb->mac_address_handle,
		((SCB_BSSCFG(scb) == NULL) ? 0 : SCB_BSSCFG(scb)->wlcif->index));

	if (err != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s() wlfc_MAC_table_update() failed %d\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			__FUNCTION__, err));
	}

end:
	return err;
}
#endif /* PROP_TXSTATUS */

void
wlc_scbfind_delete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *ea)
{
	int i;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	struct scb *scb;

	for (i = 0; i < (int)NBANDS(wlc); i++) {
		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid))
			i = BAND_5G_INDEX;

		scb = wlc_scbfindband(wlc, bsscfg, ea, i);
		if (scb) {
			WL_ASSOC(("wl%d: %s: scb for the STA-%s"
				" already exists\n", wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(ea, eabuf)));
			wlc_scbfree(wlc, scb);
		}
	}
}
/* API for host_to_dongle SCB memory copy and corresponding cubby memory copy */
/* each cubby memory copy is implemented in the corresponding function */
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
struct scb*
wlc_swap_host_to_dongle(wlc_info_t *wlc, struct scb_info *scb_info_in)
{
	struct scb *scb = NULL;
	struct scb *scb_host = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	int i;
	void* context;
	cubby_info_t *cubby_info;

	if (scb_info_in == NULL || (unsigned)scb_info_in < HOST_MEM_ADDR_RANGE)
		return 0;
	scb_host = scb_info_in->scbpub;

	struct scb_info *scbinfo_dongle = MALLOC(scbstate->pub->osh, sizeof(struct scb_info));
	scb = MALLOC(scbstate->pub->osh, scbstate->scbtotsize);
	scbinfo_dongle->scbpub = scb;

	ASSERT(scb);
	ASSERT(scbinfo_dongle);

	scbinfo_dongle->band = scb_info_in->band;
	memcpy(scb, scb_info_in->scbpub, scbstate->scbtotsize);
	scb->scb_priv = (void *)scbinfo_dongle;
	scb->rssi_window = (int *)((char *)scb + sizeof(struct scb));
	scb->tx_path = (struct tx_path_node *)
		((char *)scb->rssi_window + (sizeof(int)*MA_WINDOW_SZ));

	scb_info_in->shadow = scbinfo_dongle;
	scb_info_in->scbpub->shadow = scbinfo_dongle->scbpub;
	scbinfo_dongle->shadow = scb_info_in;
	scbinfo_dongle->scbpub->shadow = scb_info_in->scbpub;

	wlc_bsscfg_t *oldcfg = scb_host->bsscfg;

	wlc_scb_hash_del(wlc, scb_host, scb_host->bandunit, oldcfg);
	wlc_scb_list_del(wlc, scb_host, oldcfg);

	wlc_scb_list_dump(wlc, oldcfg);

	wlc_scb_hash_add(wlc, scb, scb->bandunit, oldcfg);
	wlc_scb_list_add(wlc, scbinfo_dongle, oldcfg);

	wlc_scb_list_dump(wlc, oldcfg);

	for (i = 0; i < scbstate->ncubby; i++) {
		cubby_info = &scbstate->cubby_info[i];
		context = cubby_info->context;

		switch (cubby_info->cubby_id) {
			case SCB_CUBBY_ID_RRM:
				rrm_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_AMPDU:
				ampdu_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_AMPDU_RX:
				ampdu_rx_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_RATESEL:
				rate_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_TAF:
				taf_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_TXBF:
				txbf_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			case SCB_CUBBY_ID_TXC:
				txc_cubby_host2dongle(wlc, context, scb, scb_host);
				break;
			default:
				break;
		}
	}
	return scb;
}

/* API for dongle_to_host SCB memory copy and corresponding cubby memory copy */
/* memory in dongle is released */
/* each cubby memory copy is implemented in the corresponding function */
struct scb*
wlc_swap_dongle_to_host(wlc_info_t *wlc, struct scb_info *scb_info_in)
{
	struct scb *scb_dongle = NULL;
	struct scb *scb_host = NULL;
	struct scb_info* scbinfo_host = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	int i;
	void* context;
	cubby_info_t* cubby_info;

	if ((scb_info_in == NULL) || ((unsigned)scb_info_in > HOST_MEM_ADDR_RANGE))
		return 0;
	scb_dongle = scb_info_in->scbpub;

	if ((scb_info_in->shadow == NULL)||((unsigned)scb_info_in->shadow < HOST_MEM_ADDR_RANGE)) {
		return 0;
	}

	scbinfo_host = scb_info_in->shadow;
	scb_host = scbinfo_host->scbpub;
	scbinfo_host->band = scb_info_in->band;
	memcpy(scb_host, scb_dongle, scbstate->scbtotsize);
	scb_host->scb_priv = (void *)scbinfo_host;
	scb_host->rssi_window = (int *)((char *)scb_host + sizeof(struct scb));
	scb_host->tx_path = (struct tx_path_node *)
			((char *)scb_host->rssi_window + (sizeof(int)*MA_WINDOW_SZ));
	scbinfo_host->shadow = NULL;
	scbinfo_host->scbpub->shadow = NULL;
	wlc_bsscfg_t *oldcfg = scb_dongle->bsscfg;

	wlc_scb_hash_del(wlc, scb_dongle, scb_dongle->bandunit, oldcfg);
	wlc_scb_list_del(wlc, scb_dongle, oldcfg);

	wlc_scb_list_dump(wlc, oldcfg);

	wlc_scb_hash_add(wlc, scb_host, scb_host->bandunit, oldcfg);
	wlc_scb_list_add(wlc, scbinfo_host, oldcfg);

	wlc_scb_list_dump(wlc, oldcfg);

	for (i = 0; i < scbstate->ncubby; i++) {
		cubby_info = &scbstate->cubby_info[i];
		context = cubby_info->context;

		switch (cubby_info->cubby_id) {
			case SCB_CUBBY_ID_AMPDU:
				ampdu_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_AMPDU_RX:
				ampdu_rx_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_RRM:
				rrm_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_RATESEL:
				rate_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_TAF:
				taf_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_TXBF:
				txbf_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			case SCB_CUBBY_ID_TXC:
				txc_cubby_dongle2host(wlc, context, scb_dongle, scb_host);
				break;
			default:
				break;
		}
	}
	MFREE(scbstate->pub->osh, scb_info_in->scbpub, scbstate->scbtotsize);
	MFREE(scbstate->pub->osh, scb_info_in, sizeof(struct scb_info));
	return scb_host;
}

struct scb_info*
get_scb_info_shadow(struct scb_info* scb_info_in)
{
	return scb_info_in->shadow;
}
#endif /* (BCM_HOST_MEM_RESTORE) && (BCM_HOST_MEM_SCB) */

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
/** Calculate the number of SCBs allocated in dongle memory */
uint32
wlc_scb_get_dngl_alloc_scb_num(wlc_info_t *wlc)
{
	scb_module_t* scbstate = wlc->scbstate;
	uint32 nhscb = 0; /* number of allocated host SCBs */

	if (scbstate->free_list != NULL)
		nhscb = HOST_SCB_CNT - scbstate->cnt;

	return scbstate->nscb - nhscb;
}
#endif /* SCBFREELIST || BCM_HOST_MEM_SCB */

#if BCM_HOST_MEM_SCBTAG_INUSE

/**
 * Global instance of Host SCB cached, independent of interface/bss/radio that
 * is accessible across function sequence in datapath
 */
scbtag_t scbtag_g;

static void
BCMATTACHFN(scbtag_attach)(void)
{
	memset(&scbtag_g, 0, sizeof(scbtag_t));
}

static void
BCMATTACHFN(scbtag_detach)(void)
{
	memset(&scbtag_g, 0, sizeof(scbtag_t));
}

void
scbtag_dump(int verbose)
{
#if defined(BCM_HOST_MEM_SCBTAG_PMSTATS)
	{
		scbtag_pmstats_t * p = &scbtag_g.pmstats;
		printf("\tPMSTATS: finds<%u> fills<%u> wback<%u> RD hits<%u>"
			" cubby[hits<%u> miss<%u>]\n",
			p->finds, p->fills, p->wback, p->rhits, p->chits, p->cmiss);

		/* zero out stats on RD */
		if (verbose == -1)
			memset(p, 0, sizeof(scbtag_pmstats_t));
	}
#endif /* BCM_HOST_MEM_SCBTAG_PMSTATS */

	if (!SCBTAG_IS_ACTIVE()) {
		printf("SCBTAG not active\n");
	}

	{
		scbtag_context_t * p = &scbtag_g.context;
		uint8 * e = (uint8 *)&p->ea;
		printf("SCBTAG<0x%p> lifetime<%d> assoctime<%u> bsscfg<0x%p> wlc<0x%p>\n"
			"\twlcif<%p> ea[%02x:%02x:%02x:%02x:%02x:%02x] txc_sz<%u>\n",
			p->host_scb, p->lifetime, p->assoctime, p->bsscfg, p->wlc,
			p->wlcif, e[0], e[1], e[2], e[3], e[4], e[5], p->txc_sz);
	}

#if defined(BCM_HOST_MEM_SCBTAG_DUMP_FN)
	if (verbose <= 0)
		return;

	{
		scbtag_rcached_t * p = &scbtag_g.rcached;
		printf("\tRD: flags<0x%08x> flags2<0x%08x> flags3<0x%08x>\n"
			"\t\tkey<0x%p> wds<0x%p> PS<%u> ISMULTI<%u>\n",
			p->flags, p->flags2, p->flags3,
			p->key, p->wds, p->PS, p->ismulti);
		printf("\t\tscb_info<0x%p> band<0x%p>\n", p->host_scb_info, p->band);
	}

#if defined(BCM_HOST_MEM_SCBTAG_CUBBIES)
	{
		scbtag_cubbies_t * p = &scbtag_g.cubbies;
		printf("\tCB: txc<0x%p>\n", p->host_scb_txc_info);
	}
#endif /* BCM_HOST_MEM_SCBTAG_CUBBIES */
#endif /* BCM_HOST_MEM_SCBTAG_DUMP_FN */
}

#if defined(BCM_HOST_MEM_SCBTAG_AUDIT_FN)

#define INIT_AUDIT() do { error_num = 0; } while (0)
#define NEXT_AUDIT() do { error_num++; } while (0)

#define FAIL_AUDIT() \
	do { \
		error_line = __LINE__; /* overrides previous error line */ \
		errors += (1 << error_num); \
	} while (0)

#define FIELD_AUDIT(sec, field) \
		({ \
			NEXT_AUDIT(); \
			if (host_scb->field != scbtag_g.sec.field) { \
				FAIL_AUDIT(); \
			} \
		})

int
scbtag_audit(void)
{
	uint32 error_line, error_num, errors = 0U;
	struct scb * host_scb;

	if (!SCBTAG_IS_ACTIVE()) {
		return BCME_NOTREADY;
	}

	host_scb = scbtag_g.context.host_scb;

	/* Verify that SCBTAG fields match that of host_scb */
	INIT_AUDIT();
	if (eacmp(&scbtag_g.context.ea, &host_scb->ea) != 0) {
		FAIL_AUDIT();
	}

	NEXT_AUDIT();
	if (scbtag_g.rcached.ismulti && !ETHER_ISMULTI(&scbtag_g.context.ea)) {
		FAIL_AUDIT();
	}

	FIELD_AUDIT(context, assoctime);
	FIELD_AUDIT(context, bsscfg);

	FIELD_AUDIT(rcached, flags);
	FIELD_AUDIT(rcached, flags2);
	FIELD_AUDIT(rcached, flags3);
	FIELD_AUDIT(rcached, key);
	FIELD_AUDIT(rcached, wds);
	FIELD_AUDIT(rcached, PS);

	NEXT_AUDIT();
	if (host_scb->scb_priv != (void*)scbtag_g.rcached.host_scb_info) {
		FAIL_AUDIT();
	}

	NEXT_AUDIT();
	if (((struct scb_info*)host_scb->scb_priv)->band != scbtag_g.rcached.band) {
		FAIL_AUDIT();
	}

	if (errors) {
		scbtag_dump(TRUE);
		printf("%s: errors<0x%08x> error_line<%d>\n",
			__FUNCTION__, errors, error_line);
	}

	return errors;
}
#endif /* BCM_HOST_MEM_SCBTAG_AUDIT_FN */

/**
 * scbtag_lifetime()
 * As the scbtag_cache() function may be invoked in a location common to various
 * datapath and control path, a mechanism to control whether caching should
 * occur or not is provided by this API. Caching can be hence permitted only for
 * say TX datapath and txstatus processing datapath and excluded for other paths
 * such as Rx processing or miscellaneous control paths.
 */
void BCMFASTPATH
scbtag_lifetime(bool control)
{
	SCBTAG_ASSERT((control == FALSE) ||
		((control == TRUE) && (scbtag_g.context.lifetime == FALSE)));

	/* Ensure paired <cache,flush> */
	if (control == FALSE) { /* Alltempt a flush on end of lifetime request */
		scbtag_flush();
	}

	scbtag_g.context.lifetime = control;
}

/**
 * scbtag_flush()
 * Flush all dirty (modified) cached fields back to host and invalidate the
 * SCBTAG. SCBTAG may be reused for another host_scb.
 *
 */
void BCMFASTPATH
scbtag_flush(void)
{
	if (!SCBTAG_IS_ACTIVE()) {
		return; /* Not active and nothing to flush */
	}

	/* Only host resident SCBs may be cached */
	SCBTAG_ASSERT(SCBTAG_IN_HOST_MEM(scbtag_g.context.host_scb));

	/* Audit Read Only fields between cached and host */
	SCBTAG_ASSERT(SCBTAG_AUDIT() == BCME_OK);

	/*
	 * XXX Presently, no modified fields are supported.
	 * Some modified fields will be immediately synched on WRITE
	 * All other modified fields will be flushed at the end of lifetime
	 *
	 * Caching and flushing of scb_txc_info is done independently of active SCB
	 */
	scbtag_g.context.host_scb = NULL;
	scbtag_g.context.assoctime = 0U;
	scbtag_g.context.bsscfg = (wlc_bsscfg_t *)(~0);

	SCBTAG_PMSTATS_ADD(wback, 1);
}

/**
 * scbtag_cache()
 * Find a SCB without a hash lookup of the scb_info. The last hit scbtag is
 * first looked up, if it matches, then the cached host scb is returned.
 * Otherwise, the scbtag is flushed to host and a lookup is performed using
 * the host scb_info list. The found host scb is then cached into SCBTAG.
 * Cubbies which are pointers to cubby info objects will be cached on demand.
 */
#define SCBTAG_MISMATCH(field) \
	((uint32)scbtag_g.context.field ^ (uint32)field)

struct scb* BCMFASTPATH
scbtag_cache(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea, int bandunit)
{
	struct scb * found_scb;
	bool cache_active = SCBTAG_IS_ACTIVE();
	bool lifetime_enabled = (scbtag_g.context.lifetime == TRUE);

	/* XXX: reduce multiple branch conditionals using bitwise operators
	 * Reduce impact to datapath when found SCB is actually in dongle.
	 */
	/* Check if an active cached scbtag is valid, and return it as a find hit */
	if (cache_active & lifetime_enabled) {

		bool cached_scbtag_miss =
			SCBTAG_MISMATCH(wlc) |
			SCBTAG_MISMATCH(bsscfg) |
			SCBTAG_MISMATCH(bandunit) |
			(eacmp(ea, &scbtag_g.context.ea)) |
			(scbtag_g.context.assoctime ^ scbtag_g.context.host_scb->assoctime);

		if (!cached_scbtag_miss) { /* Find parameters had no mismatches */
			SCBTAG_PMSTATS_ADD(finds, 1);

			/* Skip performing explicit find, and use cached host scb pointer */
			found_scb = scbtag_g.context.host_scb;

			/* Audit Read Only fields and context */
			SCBTAG_ASSERT(SCBTAG_AUDIT() == BCME_OK);

			return found_scb;
		}
	}

	/* Locate scb by walking scb_info hash collision list in host memory */
	found_scb = wlc_scbfindband(wlc, bsscfg, ea, bandunit);

	/* wlc_scbfindband -> _wlc_scbfind will flush scbtag if it is active */

	if (SCBTAG_IN_DNGL_MEM(found_scb) | (scbtag_g.context.lifetime == FALSE)) {
		return found_scb;
	}

	/* Pre-Populate SCBTAG with host scb fields */
	SCBTAG_ASSERT(bsscfg == found_scb->bsscfg);
	SCBTAG_ASSERT(eacmp(ea, &found_scb->ea) == 0);

	/* --- Populate Context --- */
	scbtag_g.context.assoctime          = found_scb->assoctime;
	scbtag_g.context.host_scb           = found_scb;
	scbtag_g.context.bsscfg             = bsscfg; /* Use passed bsscfg */
	eacopy(ea, &scbtag_g.context.ea); /* Use passed ether_addr */
	scbtag_g.context.wlc	            = wlc;
	scbtag_g.context.bandunit           = bandunit;

	/* --- Populate READ cached fields --- */
	scbtag_g.rcached.flags              = found_scb->flags;
	scbtag_g.rcached.flags2             = found_scb->flags2;
	scbtag_g.rcached.flags3             = found_scb->flags3;
	scbtag_g.rcached.key                = found_scb->key;
	scbtag_g.rcached.wds                = found_scb->wds;
	scbtag_g.rcached.PS                 = found_scb->PS;
	scbtag_g.rcached.ismulti            = ETHER_ISMULTI(ea);

	if (scbtag_g.rcached.wds) {
		scbtag_g.context.wlcif          = scbtag_g.rcached.wds;
	} else {
		scbtag_g.context.wlcif          = bsscfg->wlcif;
	}

	/* scb_info fields cached */
	scbtag_g.rcached.host_scb_info = (struct scb_info *)found_scb->scb_priv;
	scbtag_g.rcached.band = scbtag_g.rcached.host_scb_info->band;

	/* --- Dongle cache cubby info pointers populated on demand --- */
	/* E.g. txc_sz and host_scb_txc_info are managed independently of scbtag */

	SCBTAG_PMSTATS_ADD(fills, 1);

	SCBTAG_ASSERT(SCBTAG_AUDIT() == BCME_OK);

	return found_scb;
}

struct wlcband *
scbtag_band(struct scb *scb)
{
	struct wlcband *band;

	if (SCBTAG_IS_CACHED(scb)) {
		band = scbtag_g.rcached.band;

		SCBTAG_ASSERT(band == wlc_scbband(scb));
	} else {
		band = wlc_scbband(scb);
	}

	return band;
}

#endif /* BCM_HOST_MEM_SCBTAG_INUSE */
