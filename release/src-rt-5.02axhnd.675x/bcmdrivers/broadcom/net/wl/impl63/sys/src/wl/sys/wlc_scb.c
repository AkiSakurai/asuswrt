/*
 * Common interface to the 802.11 Station Control Block (scb) structure
 *
 * Copyright 2019 Broadcom
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_scb.c 780725 2019-11-01 16:28:20Z $
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
#if defined(WL_DATAPATH_LOG_DUMP)
#include <event_log.h>
#endif /* WL_DATAPATH_LOG_DUMP */
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_macfltr.h>
#include <wlc_cubby.h>
#include <wlc_dump.h>
#include <wlc_stf.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif // endif
#ifdef WLCFP
#include <wlc_cfp.h>
#endif // endif
#include <wlc_ampdu.h>
#include <wlc_tx.h>
#include <phy_chanmgr_api.h>
#include <wlc_assoc.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

#define SCB_MAGIC 0x0505a5a5u

#define SCB_NAME_REG_MAX        4 /* max entries for wlc_scb_cubby_name_register() */

/** structure for storing public and private global scb module state */
struct scb_module {
	wlc_info_t	*wlc;			/**< global wlc info handle */
	uint16		nscb;			/**< total number of allocated scbs */
	uint16		aging_threshold;	/**< min # scbs before starting aging */
	uint16		scbtotsize;		/**< total scb size including scb_info */
	uint16		scbpubsize;		/**< struct scb size */
	wlc_cubby_info_t *cubby_info;
#ifdef SCBFREELIST
	struct scb      *free_list;		/**< Free list of SCBs */
#endif // endif
	int		cfgh;			/**< scb bsscfg cubby handle */
	bcm_notif_h 	scb_state_notif_hdl;	/**< scb state notifier handle. */
#ifdef SCB_MEMDBG
	uint32		scballoced;		/**< how many scb calls to 'wlc_scb_allocmem' */
	uint32		scbfreed;		/**< how many scb calls to 'wlc_scb_freemem' */
	uint32		freelistcount;		/**< how many scb's are in free_list */
#endif /* SCB_MEMDBG */
	const char      **name_reg;             /**< registry of cubby names */
	uint8           name_reg_count;         /**< how many names saved in registry */
};

/** station control block - one per remote MAC address */
struct scb_info {
	struct scb_info *hashnext;	/**< pointer to next scb under same hash entry */
	struct scb_info	*next;		/**< pointer to next allocated scb */
	uint8 *secbase;			/* secondary cubby allocation base pointer */
#ifdef SCB_MEMDBG
	struct scb_info *hashnext_copy;
	struct scb_info *next_copy;
	uint32 magic;
#endif // endif
};

static void wlc_scb_hash_add(scb_module_t *scbstate, wlc_bsscfg_t *cfg, enum wlc_bandunit bandunit,
	struct scb *scb);
static void wlc_scb_hash_del(scb_module_t *scbstate, wlc_bsscfg_t *cfg,
	struct scb *scbd);
static void wlc_scb_list_add(scb_module_t *scbstate, wlc_bsscfg_t *cfg,
	struct scb *scb);
static void wlc_scb_list_del(scb_module_t *scbstate, wlc_bsscfg_t *cfg,
	struct scb *scbd);

static struct scb *wlc_scbvictim(wlc_info_t *wlc);

static int wlc_scbinit(wlc_info_t *wlc, wlc_bsscfg_t *cfg, enum wlc_bandunit bandunit,
	struct scb *scb);
static void wlc_scbdeinit(wlc_info_t *wlc, struct scb *scbd);

static struct scb_info *wlc_scb_allocmem(scb_module_t *scbstate);
static void wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo);

static void wlc_scb_init_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, enum wlc_bandunit bandunit,
	struct scb *scb);
#ifdef WLSCB_HISTO
static void wlc_scb_histo_mem_free(wlc_info_t *wlc, struct scb *scb);
#endif /* WLSCB_HISTO */
#if defined(BCMDBG)
static int wlc_scb_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
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
	{SCB_HTCAP, "HT"},
	{SCB_RECV_PM, "RECV_PM"},
	{SCB_AMPDUCAP, "AMPDUCAP"},
	{SCB_IS40, "40MHz"},
	{SCB_NONGF, "NONGFCAP"},
	{SCB_APSDCAP, "APSDCAP"},
	{SCB_PENDING_PSPOLL, "PendingPSPoll"},
	{SCB_RIFSCAP, "RIFSCAP"},
	{SCB_HT40INTOLERANT, "40INTOL"},
	{SCB_WMEPS, "WMEPSOK"},
	{SCB_COEX_MGMT, "OBSSCoex"},
	{SCB_IBSS_PEER, "IBSS Peer"},
	{SCB_STBCCAP, "STBC"},
	{SCB_DTPCCAP, "DTPC"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags2[] =
{
	{SCB2_SGI20_CAP, "SGI20"},
	{SCB2_SGI40_CAP, "SGI40"},
	{SCB2_RX_LARGE_AGG, "LGAGG"},
	{SCB2_LDPCCAP, "LDPC"},
	{SCB2_VHTCAP, "VHT"},
	{SCB2_AMSDU_IN_AMPDU_CAP, "AGG^2"},
	{SCB2_P2P, "P2P"},
	{SCB2_DWDS_ACTIVE, "DWDS_ACTIVE"},
	{SCB2_HECAP, "HE"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags3[] =
{
	{SCB3_A4_DATA, "A4_DATA"},
	{SCB3_A4_NULLDATA, "A4_NULLDATA"},
	{SCB3_A4_8021X, "A4_8021X"},
	{SCB3_DWDS_CAP, "DWDS_CAP"},
	{SCB3_1024QAM_CAP, "1024QAM_CAP"},
	{SCB3_VHTMU, "VHTMU"},
	{SCB3_HEMMU, "HEMMU"},
	{SCB3_DLOFDMA, "DLOFDMA"},
	{SCB3_ULOFDMA, "ULOFDMA"},
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
	{0, NULL}
};
#endif // endif

#ifdef SCBFREELIST
static void wlc_scbfreelist_free(scb_module_t *scbstate);
#if defined(BCMDBG)
static void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b);
#endif // endif
#endif /* SCBFREELIST */

/* Each scb has the layout:
 * +-----------------+ <<== offset 0
 * | struct scb      |
 * +-----------------+ <<== scbpubsize
 * | struct scb_info |
 * +-----------------+ <<== scbtotsize
 * | cubbies         |
 * +-----------------+
 */
#define SCBINFO(_scbstate, _scb) \
	(_scb ? (struct scb_info *)((uint8 *)(_scb) + (_scbstate)->scbpubsize) : NULL)
#define SCBPUB(_scbstate, _scbinfo) \
	(_scbinfo ? (struct scb *)((uint8 *)(_scbinfo) - (_scbstate)->scbpubsize) : NULL)

#ifdef SCB_MEMDBG

#define SCBSANITYCHECK(_scbstate, _scb) \
	if (((_scb) != NULL) &&	\
	    ((SCBINFO(_scbstate, _scb)->magic != SCB_MAGIC) || \
	     (SCBINFO(_scbstate, _scb)->hashnext != SCBINFO(_scbstate, _scb)->hashnext_copy) || \
	     (SCBINFO(_scbstate, _scb)->next != SCBINFO(_scbstate, _scb)->next_copy))) { \
		WL_ERROR(("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
		          SCBINFO(_scbstate, _scb)->magic, \
			  OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->hashnext), \
		          OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->hashnext_copy), \
		          OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->next), \
			  OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->next_copy))); \
		ASSERT(0); \
	}

#define SCBFREESANITYCHECK(_scbstate, _scb)   \
	if ((SCBINFO(_scbstate, _scb) != NULL) &&	\
	    (((SCBINFO(_scbstate, _scb))->magic != ~SCB_MAGIC) || \
	     (SCBINFO(_scbstate, _scb)->next != SCBINFO(_scbstate, _scb)->next_copy))) { \
		WL_ERROR(("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
			SCBINFO(_scbstate, _scb)->magic, \
			OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->hashnext), \
			OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->hashnext_copy), \
			OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->next), \
			OSL_OBFUSCATE_BUF(SCBINFO(_scbstate, _scb)->next_copy))); \
		ASSERT(0); \
	}
#define SCBBSSCFGSANITYCHECK(_scb, _bsscfg)	\
	if ((_scb != NULL) &&	\
	    (SCB_BSSCFG(_scb) != _bsscfg)) { \
		WL_ERROR(("scb->bsscfg (%p) corrupted, should be (%p)\n", \
			OSL_OBFUSCATE_BUF(SCB_BSSCFG(_scb)), \
			OSL_OBFUSCATE_BUF(_bsscfg))); \
		ASSERT(0); \
	}
#else

#define SCBSANITYCHECK(_scbstate, _scb)		do {} while (0)
#define SCBFREESANITYCHECK(_scbstate, _scb)	do {} while (0)
#define SCBBSSCFGSANITYCHECK(_scb, _bsscfg)	do {} while (0)

#endif /* SCB_MEMDBG */

/** bsscfg cubby */
typedef struct scb_bsscfg_cubby {
	struct scb	**scbhash[MAXBANDS];	/**< scb hash table */
	uint32		nscbhash;		/**< scb hash table size */
	struct scb	*scb;			/**< station control block link list */
} scb_bsscfg_cubby_t;

#define SCB_BSSCFG_CUBBY(ss, cfg) ((scb_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (ss)->cfgh))

static int wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_scb_bsscfg_dump NULL
#endif // endif

static void wlc_scb_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *cfg, bool perm);

/* # scb hash buckets */
#if defined(DONGLEBUILD)
#define SCB_HASH_N	(uint8)((wlc->pub->tunables->maxscb + 7) >> 3)
#define	SCBHASHINDEX(hash, ea)	(((ea)[3] ^ (ea)[4] ^ (ea)[5]) % (hash))
#else /* !DONGLEBUILD */
#define SCB_HASH_N	256
#define	SCBHASHINDEX(hash, ea) ((ea)[3] ^ (ea)[4] ^ (ea)[5])
#endif /* DONGLEBUILD */
#define SCB_HASH_SZ	(sizeof(struct scb *) * MAXBANDS * SCB_HASH_N)

static int
wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	wlc_info_t *wlc = scbstate->wlc;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	struct scb **scbhash;
	uint32 i;

	ASSERT(scb_cfg->scbhash[0] == NULL);

	scbhash = MALLOCZ(wlc->osh, SCB_HASH_SZ);
	if (scbhash == NULL) {
		WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			(int)SCB_HASH_SZ, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	scb_cfg->nscbhash = SCB_HASH_N;	/* # scb hash buckets */
	for (i = 0; i < MAXBANDS; i++) {
		scb_cfg->scbhash[i] = scbhash + (i * SCB_HASH_N);
	}

	return BCME_OK;
}

static void
wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	wlc_info_t *wlc = scbstate->wlc;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	uint32 i;

	/* clear all scbs */
	wlc_scb_bsscfg_scbclear(wlc, cfg, TRUE);

	scb_cfg->nscbhash = 0;

	ASSERT(scb_cfg->scbhash[0] != NULL);

	/* N.B.: the hash is contiguously allocated across multiple bands */
	MFREE(wlc->osh, scb_cfg->scbhash[0], SCB_HASH_SZ);
	scb_cfg->scbhash[0] = NULL;

	scb_cfg->nscbhash = 0;
	for (i = 0; i < MAXBANDS; i++) {
		scb_cfg->scbhash[i] = NULL;
	}
}

static void
wlc_scb_bss_state_upd(void *ctx, bsscfg_state_upd_data_t *evt_data)
{
	scb_module_t *scbstate = (scb_module_t *)ctx;
	wlc_info_t *wlc = scbstate->wlc;
	wlc_bsscfg_t *cfg = evt_data->cfg;

	/* clear all old non-permanent scbs for IBSS only if assoc recreate is not enabled */
	/* and WLC_BSSCFG_PRESERVE cfg flag is not set */
	if (!evt_data->old_up && cfg->up) {
		if (!cfg->BSS &&
		    !(ASSOC_RECREATE_ENAB(wlc->pub) && (cfg->flags & WLC_BSSCFG_PRESERVE))) {
			wlc_scb_bsscfg_scbclear(wlc, cfg, FALSE);
		}
	}
	/* clear all non-permanent scbs when disabling the bsscfg */
	else if (evt_data->old_enable && !cfg->enable) {
		wlc_scb_bsscfg_scbclear(wlc, cfg, FALSE);
	}
}

/* # scb cubby registry entries */
#define SCB_CUBBY_REG_N  (wlc->pub->tunables->maxscbcubbies)
#define SCB_CUBBY_REG_SZ (sizeof(scb_module_t))

/* Min # scbs to have before starting aging.
 * Set to 1 for now as nop.
 */
#define SCB_AGING_THRESHOLD	1

scb_module_t *
BCMATTACHFN(wlc_scb_attach)(wlc_info_t *wlc)
{
	scb_module_t *scbstate;
	int name_reg_len;

	BCM_REFERENCE(name_reg_len);

	if ((scbstate = MALLOCZ(wlc->osh, SCB_CUBBY_REG_SZ)) == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, (int)SCB_CUBBY_REG_SZ,
			MALLOCED(wlc->osh)));
		goto fail;
	}
	scbstate->wlc = wlc;

	if ((scbstate->cubby_info = wlc_cubby_attach(wlc->osh, wlc->pub->unit, wlc->objr,
	                                             OBJR_SCB_CUBBY, SCB_CUBBY_REG_N)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_cubby_attach failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* TODO: maybe need a tunable here. */
	/* This is to limit the scb aging in low memory situation to happen
	 * less often in cases like aging out the only scb.
	 * For SCBFREELIST build we can set it to wlc->pub->tunables->maxscb
	 * to bypass the low memory processing.
	 */
	scbstate->aging_threshold = SCB_AGING_THRESHOLD;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((scbstate->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(scb_bsscfg_cubby_t),
			wlc_scb_bsscfg_init, wlc_scb_bsscfg_deinit, wlc_scb_bsscfg_dump,
			(void *)scbstate)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	scbstate->scbpubsize = (uint16)sizeof(struct scb);
	scbstate->scbtotsize = scbstate->scbpubsize;
	scbstate->scbtotsize += (uint16)sizeof(struct scb_info);

	if (wlc_bsscfg_state_upd_register(wlc, wlc_scb_bss_state_upd, scbstate) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(WL_DATAPATH_LOG_DUMP)
	name_reg_len = SCB_NAME_REG_MAX * sizeof(*scbstate->name_reg);
	scbstate->name_reg = MALLOCZ_PERSIST(wlc->osh, name_reg_len);
	if (scbstate->name_reg == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n",
		          WLCWLUNIT(wlc), __FUNCTION__, name_reg_len, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* WL_DATAPATH_LOG_DUMP */

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "scb", (dump_fn_t)wlc_scb_dump, (void *)wlc);
#endif // endif

	/* create notification list for scb state change. */
	if (bcm_notif_create_list(wlc->notif, &scbstate->scb_state_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: scb bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return scbstate;

fail:
	MODULE_DETACH(scbstate, wlc_scb_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_scb_detach)(scb_module_t *scbstate)
{
	wlc_info_t *wlc;

	if (!scbstate)
		return;

	wlc = scbstate->wlc;

	if (scbstate->scb_state_notif_hdl != NULL)
		bcm_notif_delete_list(&scbstate->scb_state_notif_hdl);

	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_scb_bss_state_upd, scbstate);

#ifdef SCBFREELIST
	wlc_scbfreelist_free(scbstate);
#endif // endif

	wlc_cubby_detach(scbstate->cubby_info);

#if defined(WL_DATAPATH_LOG_DUMP)
	if (scbstate->name_reg != NULL) {
		MFREE_PERSIST(wlc->osh, scbstate->name_reg,
		              SCB_NAME_REG_MAX * sizeof(*scbstate->name_reg));
	}
#endif /* WL_DATAPATH_LOG_DUMP */

	MFREE(wlc->osh, scbstate, SCB_CUBBY_REG_SZ);
}

/* Methods for iterating along a list of scb */

/**
 * Direct access to the next
 * @param scbstate   This parameter was added to reduce scb size by 2 pointers
 */
static struct scb *
wlc_scb_next(scb_module_t *scbstate, struct scb *scb)
{
	if (scb) {
		struct scb_info *scbinfo = SCBINFO(scbstate, scb);
		SCBSANITYCHECK(scbstate, scb);
		return SCBPUB(scbstate, scbinfo->next);
	}
	return NULL;
}

static struct wlc_bsscfg *
wlc_scb_next_bss(scb_module_t *scbstate, int idx)
{
	wlc_info_t *wlc = scbstate->wlc;
	wlc_bsscfg_t *next_bss = NULL;

	/* get next bss walking over hole */
	while (idx < WLC_MAXBSSCFG) {
		next_bss = WLC_BSSCFG(wlc, idx);
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
	SCBSANITYCHECK(scbstate, scb_cfg->scb);
	SCBBSSCFGSANITYCHECK(scb_cfg->scb, scbiter->next_bss);

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
			scbiter->next = wlc_scb_next(scbstate, scb);
			SCBBSSCFGSANITYCHECK(scb, scbiter->next_bss);
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

#ifdef BCMDBG
/* undefine the BCMDBG helper macros so they will not interfere with the function definitions */
#undef wlc_scb_cubby_reserve
#undef wlc_scb_cubby_reserve_ext
#endif // endif

/**
 * Reduced parameter version of wlc_scb_cubby_reserve_ext().
 *
 * Return value: negative values are errors, non-negative are cubby offsets
 */
#ifdef BCMDBG
int
BCMATTACHFN(wlc_scb_cubby_reserve)(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context, const char *func)
#else
int
BCMATTACHFN(wlc_scb_cubby_reserve)(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context)
#endif /* BCMDBG */
{
	scb_cubby_params_t params;
	int ret;

	bzero(&params, sizeof(params));

	params.fn_init = fn_init;
	params.fn_deinit = fn_deinit;
	params.fn_dump = fn_dump;
	params.context = context;

#ifdef BCMDBG
	ret = wlc_scb_cubby_reserve_ext(wlc, size, &params, func);
#else
	ret = wlc_scb_cubby_reserve_ext(wlc, size, &params);
#endif // endif
	return ret;
}

/**
 * Multiple modules have the need of reserving some private data storage related to a specific
 * communication partner. During ATTACH time, this function is called multiple times, typically one
 * time per module that requires this storage. This function does not allocate memory, but
 * calculates values to be used for a future memory allocation by wlc_scb_allocmem() instead.
 *
 * Return value: negative values are errors, non-negative are cubby offsets.
 */
#ifdef BCMDBG
int
BCMATTACHFN(wlc_scb_cubby_reserve_ext)(wlc_info_t *wlc, uint size, scb_cubby_params_t *params,
	const char *func)
#else
int
BCMATTACHFN(wlc_scb_cubby_reserve_ext)(wlc_info_t *wlc, uint size, scb_cubby_params_t *params)
#endif /* BCMDBG */
{
	scb_module_t *scbstate = wlc->scbstate;
	wlc_cubby_fn_t fn;
#ifdef WLRSDB
	wlc_cubby_cp_fn_t cp_fn;
#endif /* WLRSDB */
	int offset;

	ASSERT(scbstate->nscb == 0);

	bzero(&fn, sizeof(fn));
	fn.fn_init = (cubby_init_fn_t)params->fn_init;
	fn.fn_deinit = (cubby_deinit_fn_t)params->fn_deinit;
	fn.fn_secsz = (cubby_secsz_fn_t)params->fn_secsz;
	fn.fn_dump = (cubby_dump_fn_t)params->fn_dump;
#ifdef WL_DATAPATH_LOG_DUMP
	fn.fn_data_log_dump = (cubby_datapath_log_dump_fn_t)params->fn_data_log_dump;
#endif /* WL_DATAPATH_LOG_DUMP */
#ifdef BCMDBG
	fn.name = func;
#endif // endif

#ifdef WLRSDB
	/* Optional Cubby copy function. Currently we dont support
	* get/set api's for SCB cubby copy. Only update
	* function should be used by the callers to allow
	* for single approach for SCB copy implementation.
	*/
	bzero(&cp_fn, sizeof(cp_fn));

	cp_fn.fn_update = (cubby_update_fn_t)params->fn_update;
#endif /* WLRSDB */

	offset = wlc_cubby_reserve(scbstate->cubby_info, size, &fn,
#ifdef WLRSDB
		0, &cp_fn,
#endif /* WLRSDB */
		params->context);

	if (offset < 0) {
		WL_ERROR(("wl%d: %s: wlc_cubby_reserve failed with err %d\n",
		          wlc->pub->unit, __FUNCTION__, offset));
		return offset;
	}

	return (int)scbstate->scbtotsize + offset;
}

/**
 * Cubby Name ID Registration
 * This fn associates a string with a unique ID (uint8). The ID is returned.
 * The string pointer is stored, so the memory for 'name' needs to be
 * a literal or an allocation that remains until driver detach.
 * An ID of WLC_SCB_NAME_ID_INVALID is an error indication.
 *
 * @param scbstate      A pointer to the state structure for the SCB module
 *                      created by wlc_scb_attach().
 * @param name          A pointer to a string that is the name to be registered.
 *                      The string is not copied as noted above.
 * @return              A uint8 as a unique ID associated with the name.
 *                      A value of WLC_SCB_NAME_ID_INVALID indicated the name
 *                      was not registered.
 */
uint8
BCMATTACHFN(wlc_scb_cubby_name_register)(scb_module_t *scbstate, const char *name)
{
	uint8 current_count = scbstate->name_reg_count;

	if (current_count >= SCB_NAME_REG_MAX || scbstate->name_reg == NULL) {
		return WLC_SCB_NAME_ID_INVALID;
	}

	/* save the string pointer in the next table entry, and increment the count */
	scbstate->name_reg[current_count] = name;
	scbstate->name_reg_count++;

	/* return the table entry used */
	return current_count;
}

#if defined(WL_DATAPATH_LOG_DUMP)
/**
 * This fn dumps the Cubby Name registry built with wlc_scb_cubby_name_register().
 * In dongle builds, EVENT_LOG() is used to dump the name registry.
 *
 * @param scbstate      A pointer to the state structure for the SCB module
 *                      created by wlc_scb_attach().
 * @param tag           EVENT_LOG tag for output
 */
void
wlc_scb_cubby_name_dump(scb_module_t *scbstate, int tag)
{
	int i;
	uint8 current_count = scbstate->name_reg_count;
	struct str_log {
		uint16 type;
		uint16 len;
		char str[32];
	} str_log;

	/* early return if nothing to report */
	if (current_count == 0) {
		return;
	}

	EVENT_LOG(tag, "SCB Cubby IDs:\n");

	str_log.type = EVENT_LOG_XTLV_ID_STR;

	for (i = 0; i < current_count; i++) {
		/* xtlv strings are not null terminated, len is string length */
		str_log.len = (uint16)snprintf(str_log.str, 32, "%d: %s",
		                               i, scbstate->name_reg[i]);
		/* if the snprintf is truncated, just set len to the buffer size */
		if (str_log.len > 32) {
			str_log.len = 32;
		}

		EVENT_LOG_BUFFER(tag, (uint8*)&str_log, str_log.len + BCM_XTLV_HDR_SIZE);
	}
}
#endif /* WL_DATAPATH_LOG_DUMP */

struct wlcband *
wlc_scbband(wlc_info_t *wlc, struct scb *scb)
{
	return wlc->bandstate[scb->bandunit];
}

#ifdef SCBFREELIST
static struct scb_info *
wlc_scbget_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;

	if (scbstate->free_list == NULL)
		return NULL;

	ret = SCBINFO(scbstate, scbstate->free_list);
	SCBFREESANITYCHECK(scbstate, scbstate->free_list);
	scbstate->free_list = SCBPUB(scbstate, ret->next);
#ifdef SCB_MEMDBG
	ret->next_copy = NULL;
#endif // endif
	ret->next = NULL;

#ifdef SCB_MEMDBG
	scbstate->freelistcount = (scbstate->freelistcount > 0) ? (scbstate->freelistcount - 1) : 0;
#endif /* SCB_MEMDBG */

	return ret;
}

static void
wlc_scbadd_free(scb_module_t *scbstate, struct scb_info *ret)
{
	SCBFREESANITYCHECK(scbstate, scbstate->free_list);
	ret->next = SCBINFO(scbstate, scbstate->free_list);
	scbstate->free_list = SCBPUB(scbstate, ret);
#ifdef SCB_MEMDBG
	ret->magic = ~SCB_MAGIC;
	ret->next_copy = ret->next;
#endif // endif

#ifdef SCB_MEMDBG
	scbstate->freelistcount += 1;
#endif /* SCB_MEMDBG */

	return;
}

static void
wlc_scbfreelist_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;

	ret = SCBINFO(scbstate, scbstate->free_list);
	while (ret) {
		SCBFREESANITYCHECK(scbstate, SCBPUB(scbstate, ret));
		scbstate->free_list = SCBPUB(scbstate, ret->next);
		wlc_scb_freemem(scbstate, ret);
		ret = SCBINFO(scbstate, scbstate->free_list);

#ifdef SCB_MEMDBG
		scbstate->freelistcount = (scbstate->freelistcount > 0) ?
			(scbstate->freelistcount - 1) : 0;
#endif /* SCB_MEMDBG */
	}
}

#if defined(BCMDBG)
static
void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b)
{
	struct scb_info *entry = NULL;
	int i = 1;

#ifdef SCB_MEMDBG
	bcm_bprintf(b, "scbfreelist (count: %7u):\n", scbstate->freelistcount);
#else
	bcm_bprintf(b, "scbfreelist:\n");
#endif /* SCB_MEMDBG */

	entry = SCBINFO(scbstate, scbstate->free_list);
	while (entry) {
		SCBFREESANITYCHECK(scbstate, SCBPUB(scbstate, entry));
		bcm_bprintf(b, "%d: 0x%x\n", i, entry);
		entry = entry->next;
		i++;
	}
}
#endif // endif
#endif /* SCBFREELIST */

static void
wlc_scb_reset(scb_module_t *scbstate, struct scb_info *scbinfo)
{
	struct scb *scb = SCBPUB(scbstate, scbinfo);
	uint len;

	len = scbstate->scbtotsize + wlc_cubby_totsize(scbstate->cubby_info);
	bzero(scb, len);

#ifdef SCB_MEMDBG
	scbinfo->magic = SCB_MAGIC;
#endif // endif
}

/**
 * After all the modules indicated how much cubby space they need in the scb, the actual scb can be
 * allocated. This happens one time fairly late within the attach phase, but also when e.g.
 * communication with a new remote party is started.
 */
static struct scb_info *
wlc_scb_allocmem(scb_module_t *scbstate)
{
	wlc_info_t *wlc = scbstate->wlc;
	struct scb_info *scbinfo;
	struct scb *scb;
	uint len;

	/* Make sure free_mem never gets below minimum threshold due to scb_allocs */
	if (OSL_MEM_AVAIL() <= (uint)wlc->pub->tunables->min_scballoc_mem) {
		WL_ERROR(("wl%d: %s: low memory. %d bytes left.\n",
		          wlc->pub->unit, __FUNCTION__, OSL_MEM_AVAIL()));
		return NULL;
	}

	len = scbstate->scbtotsize + wlc_cubby_totsize(scbstate->cubby_info);
	/* Per the above comment, can be freed dynamically */
	scb = MALLOCZ_NOPERSIST(wlc->osh, len);
	if (scb == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
		          len, MALLOCED(wlc->osh)));
		return NULL;
	}

	scbinfo = SCBINFO(scbstate, scb);

#ifdef SCB_MEMDBG
	scbstate->scballoced++;
#endif /* SCB_MEMDBG */

	return scbinfo;
}

#define _wlc_internalscb_free(wlc, scb) wlc_scbfree(wlc, scb)

/**
 * Internal scbs don't participate in scb hash and lookup i.e. you can't find internal scbs by
 * ethernet address. There are two internal scbs types: hwrs and bcmc.
 */
static struct scb *
_wlc_internalscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *ea, struct wlcband *band, uint32 flags2)
{
	struct scb_info *scbinfo = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	int bcmerror = 0;
	struct scb *scb;
#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
	uint32 freemem_before, bytes_allocated;

	hnd_get_heapuse(&mu);
	freemem_before = mu.arena_free;
#endif /* MEM_ALLOC_STATS */

	if (TRUE &&
#ifdef SCBFREELIST
	    /* If not found on freelist then allocate a new one */
	    (scbinfo = wlc_scbget_free(scbstate)) == NULL &&
#endif // endif
	    (scbinfo = wlc_scb_allocmem(scbstate)) == NULL) {
		WL_ERROR(("wl%d: %s: Couldn't alloc internal scb\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}
	wlc_scb_reset(scbstate, scbinfo);

	scb = SCBPUB(scbstate, scbinfo);
	scb->bsscfg = cfg;
	scb->if_stats = cfg->wlcif->_cnt;
	scb->ea = *ea;

	/* used by hwrs and bcmc scbs */
	scb->flags2 = flags2;

	bcmerror = wlc_scbinit(wlc, cfg, band->bandunit, scb);
	if (bcmerror) {
		WL_ERROR(("wl%d: %s failed with err %d\n",
			wlc->pub->unit, __FUNCTION__, bcmerror));
		_wlc_internalscb_free(wlc, scb);
		return NULL;
	}

	wlc_scb_init_rates(wlc, cfg, band->bandunit, scb);

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	bytes_allocated = freemem_before - mu.arena_free;
	scb->mem_bytes = bytes_allocated;
	if (bytes_allocated > mu.max_scb_alloc) {
		mu.max_scb_alloc = bytes_allocated;
		hnd_update_mem_alloc_stats(&mu);
	}
#endif /* MEM_ALLOC_STATS */

	return scb;
}

/** allocs broadcast/multicast internal scb */
struct scb *
wlc_bcmcscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct wlcband *band)
{
	return _wlc_internalscb_alloc(wlc, cfg, &ether_bcast, band, SCB2_BCMC);
}

/** hardware rates scb contains the rates that the local hardware supports */
struct scb *
wlc_hwrsscb_alloc(wlc_info_t *wlc, struct wlcband *band)
{
	const struct ether_addr ether_local = {{2, 0, 0, 0, 0, 0}};
	wlc_bsscfg_t *cfg = wlc_bsscfg_primary(wlc);

	/* TODO: pass NULL as cfg as hwrs scb doesn't belong to
	 * any bsscfg.
	 */
	return _wlc_internalscb_alloc(wlc, cfg, &ether_local, band, SCB2_HWRS);
}

/** a 'user' scb as opposed to an 'internal' scb */
static struct scb *
wlc_userscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *ea, struct wlcband *band)
{
	scb_module_t *scbstate = wlc->scbstate;
	struct scb_info *scbinfo = NULL;
	struct scb *oldscb;
	int bcmerror;
	struct scb *scb;
#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
	uint32 freemem_before, bytes_allocated;

	hnd_get_heapuse(&mu);
	freemem_before = mu.arena_free;
#endif /* MEM_ALLOC_STATS */

	/* Make sure we live within our budget, and kick someone out if needed. */
	if (scbstate->nscb >= wlc->pub->tunables->maxscb ||
	    /* age scb in low memory situation as well */
	    (OSL_MEM_AVAIL() <= (uint)wlc->pub->tunables->min_scballoc_mem &&
	     /* apply scb aging in low memory situation in a limited way
	      * to prevent it ages too often.
	      */
	     scbstate->nscb >= scbstate->aging_threshold)) {
		/* free the oldest entry */
		if (!(oldscb = wlc_scbvictim(wlc))) {
			WL_ERROR(("wl%d: %s: no SCBs available to reclaim\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
		SCB_MARK_DEAUTH(oldscb);
		if (!wlc_scbfree(wlc, oldscb)) {
			WL_ERROR(("wl%d: %s: Couldn't free a victimized scb\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
	}
	ASSERT(scbstate->nscb <= wlc->pub->tunables->maxscb);

	if (TRUE &&
#ifdef SCBFREELIST
	    /* If not found on freelist then allocate a new one */
	    (scbinfo = wlc_scbget_free(scbstate)) == NULL &&
#endif // endif
	    (scbinfo = wlc_scb_allocmem(scbstate)) == NULL) {
		WL_ERROR(("wl%d: %s: Couldn't alloc user scb\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}
	wlc_scb_reset(scbstate, scbinfo);

	scbstate->nscb++;

	scb = SCBPUB(scbstate, scbinfo);
	scb->bsscfg = cfg;
	scb->if_stats = cfg->wlcif->_cnt;
	scb->ea = *ea;
	scb->multimac_idx = MULTIMAC_ENTRY_INVALID;

	bcmerror = wlc_scbinit(wlc, cfg, band->bandunit, scb);
	if (bcmerror) {
		WL_ERROR(("wl%d: %s failed with err %d\n", wlc->pub->unit, __FUNCTION__, bcmerror));
		wlc_scbfree(wlc, scb);
		return NULL;
	}

	/* add it to the link list */
	wlc_scb_list_add(scbstate, cfg, scb);

	/* install it in the cache */
	wlc_scb_hash_add(scbstate, cfg, band->bandunit, scb);

	wlc_scb_init_rates(wlc, cfg, band->bandunit, scb);

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	bytes_allocated = freemem_before - mu.arena_free;
	scb->mem_bytes = bytes_allocated;
	if (bytes_allocated > mu.max_scb_alloc) {
		mu.max_scb_alloc = bytes_allocated;
		hnd_update_mem_alloc_stats(&mu);
	}
#endif /* MEM_ALLOC_STATS */

	return scb;
}

/* secondary cubby total size. return 0 to skip secondary cubby init. */
static uint
wlc_scb_sec_sz(void *ctx, void *obj)
{
	scb_module_t *scbstate = (scb_module_t *)ctx;
	struct scb *scb = (struct scb *)obj;

	return wlc_cubby_sec_totsize(scbstate->cubby_info, scb);
}

/** saves the base address of the secondary (cubby) container */
static void
wlc_scb_sec_set(void *ctx, void *obj, void *base)
{
	scb_module_t *scbstate = (scb_module_t *)ctx;
	struct scb *scb = (struct scb *)obj;
	struct scb_info *scbinfo = SCBINFO(scbstate, scb);

	scbinfo->secbase = base;
}

/** returns the base address of the secondary (cubby) container */
static void *
wlc_scb_sec_get(void *ctx, void *obj)
{
	scb_module_t *scbstate = (scb_module_t *)ctx;
	struct scb *scb = (struct scb *)obj;
	struct scb_info *scbinfo = SCBINFO(scbstate, scb);

	return scbinfo->secbase;
}

static int
wlc_scbinit(wlc_info_t *wlc, wlc_bsscfg_t *cfg, enum wlc_bandunit bandunit, struct scb *scb)
{
	scb_module_t *scbstate = wlc->scbstate;
	uint i;

	BCM_REFERENCE(cfg);
	ASSERT(scb != NULL);

	scb->used = wlc->pub->now;
	scb->bandunit = bandunit;

	for (i = 0; i < NUMPRIO; i++)
		scb->seqctl[i] = 0xFFFF;
	scb->seqctl_nonqos = 0xFFFF;

#if defined(WLATF) && defined(WLATF_PERC)
	scb->staperc = 0;
	scb->sched_staperc = 0;
#endif // endif

#ifdef WLCNTSCB
	bzero((char*)&scb->scb_stats, sizeof(scb->scb_stats));
#endif // endif
	return wlc_cubby_init(scbstate->cubby_info, scb, wlc_scb_sec_sz, wlc_scb_sec_set, scbstate);
}

static void
wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo)
{
	wlc_info_t *wlc = scbstate->wlc;
	struct scb *scb = SCBPUB(scbstate, scbinfo);
	uint len;

	BCM_REFERENCE(wlc);

	if (scbinfo == NULL)
		return;

#ifdef SCB_MEMDBG
	scbinfo->magic = ~SCB_MAGIC;
#endif // endif
	len = scbstate->scbtotsize + wlc_cubby_totsize(scbstate->cubby_info);
	MFREE(wlc->osh, scb, len);

#ifdef SCB_MEMDBG
	scbstate->scbfreed++;
#endif /* SCB_MEMDBG */
}

#ifdef WLSCB_HISTO
static void
wlc_scb_histo_mem_free(wlc_info_t *wlc, struct scb *scb)
{
	if (scb->histo) {
		MFREE(wlc->osh, scb->histo, sizeof(wl_rate_histo_maps1_t));
	}
}
#endif /* WLSCB_HISTO */

bool
wlc_scbfree(wlc_info_t *wlc, struct scb *scbd)
{
	scb_module_t *scbstate = wlc->scbstate;
	struct scb_info *remove = SCBINFO(scbstate, scbd);

	if (scbd->permanent)
		return FALSE;

	/* Return if SCB is already being deleted else mark it */
	if (scbd->mark & SCB_DEL_IN_PROG)
		return FALSE;

	scbd->mark |= SCB_DEL_IN_PROG;

	/* cancel potential dangling sendcomplete callbacks */
	wlc_deauth_sendcomplete_clean(wlc, scbd);

	wlc_tx_fifo_scb_flush(wlc, scbd);

#ifdef WLSCB_HISTO
	wlc_scb_histo_mem_free(wlc, scbd);
#endif /* WLSCB_HISTO */

	wlc_scbdeinit(wlc, scbd);

	wlc_scb_resetstate(wlc, scbd);

	if (SCB_INTERNAL(scbd)) {
		goto free;
	}

	/* delete it from the hash */
	wlc_scb_hash_del(scbstate, SCB_BSSCFG(scbd), scbd);

	/* delete it from the link list */
	wlc_scb_list_del(scbstate, SCB_BSSCFG(scbd), scbd);

	/* update total allocated scb number */
	scbstate->nscb--;

free:

#ifdef SCBFREELIST
	wlc_scbadd_free(scbstate, remove);
#else
	/* free scb memory */
	wlc_scb_freemem(scbstate, remove);
#endif // endif

	return TRUE;
}

static void
wlc_scbdeinit(wlc_info_t *wlc, struct scb *scbd)
{
	scb_module_t *scbstate = wlc->scbstate;

	wlc_cubby_deinit(scbstate->cubby_info, scbd, wlc_scb_sec_sz, wlc_scb_sec_get, scbstate);
}

static void
wlc_scb_list_add(scb_module_t *scbstate, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	struct scb_info *scbinfo = SCBINFO(scbstate, scb);
	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, bsscfg);

	SCBSANITYCHECK(scbstate, scb_cfg->scb);

	/* update scb link list */
	scbinfo->next = SCBINFO(scbstate, scb_cfg->scb);
#ifdef SCB_MEMDBG
	scbinfo->next_copy = scbinfo->next;
#endif // endif
	scb_cfg->scb = scb;
}

static void
wlc_scb_list_del(scb_module_t *scbstate, wlc_bsscfg_t *bsscfg, struct scb *scbd)
{
	scb_bsscfg_cubby_t *scb_cfg;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbstate, scbd);

	ASSERT(bsscfg != NULL);

	/* delete it from the link list */

	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, bsscfg);
	scbinfo = SCBINFO(scbstate, scb_cfg->scb);
	if (scbinfo == remove) {
		scb_cfg->scb = wlc_scb_next(scbstate, scbd);
		return;
	}

	while (scbinfo) {
		SCBSANITYCHECK(scbstate, SCBPUB(scbstate, scbinfo));
		if (scbinfo->next == remove) {
			scbinfo->next = remove->next;
#ifdef SCB_MEMDBG
			scbinfo->next_copy = scbinfo->next;
#endif // endif
			break;
		}
		scbinfo = scbinfo->next;
	}

	if (scbinfo == NULL) {
		WL_ERROR(("wl%d.%d: Unable to find scbinfo, scb = %p in the linked list",
			WLCWLUNIT(bsscfg->wlc), WLC_BSSCFG_IDX(bsscfg), OSL_OBFUSCATE_BUF(scbd)));
	}
}

/** free all scbs of a bsscfg */
static void
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
		if (!scb->permanent && (scb->state == 0))
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
	}
#endif /* AP */

	WL_ASSOC(("wl%d: %s: reclaim scb %s, idle %d sec\n",  wlc->pub->unit, __FUNCTION__,
	          bcm_ether_ntoa(&oldscb->ea, eabuf), oldest));

	return oldscb;
}

/* Only notify registered clients of the following states' change. */
/* XXX make it readonly/rom?
 * XXX then need access fn in case of rom invalidation when
 * value changes after tapeout...worth it?
 */
static uint8 scb_state_change_notif_mask = AUTHENTICATED | ASSOCIATED | AUTHORIZED;

/** "|" operation. */
void
wlc_scb_setstatebit(wlc_info_t *wlc, struct scb *scb, uint8 state)
{
	scb_module_t *scbstate;
	uint8	oldstate;

	WL_NONE(("set state %x\n", state));
	ASSERT(scb != NULL);

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

	scb->state |= state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));

	if ((BSSCFG_AP(SCB_BSSCFG(scb)) && (state & scb_state_change_notif_mask) != 0) ||
	    (oldstate & scb_state_change_notif_mask) != (state & scb_state_change_notif_mask))
	{
		scb_state_upd_data_t data;
		data.scb = scb;
		data.oldstate = oldstate;
		bcm_notif_signal(scbstate->scb_state_notif_hdl, &data);
	}
}

/** "& ~" operation */
void
wlc_scb_clearstatebit(wlc_info_t *wlc, struct scb *scb, uint8 state)
{
	scb_module_t *scbstate;
	uint8	oldstate;

	ASSERT(scb != NULL);
	WL_NONE(("clear state %x\n", state));

	scbstate = wlc->scbstate;
	oldstate = scb->state;

	scb->state &= ~state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));

	if ((oldstate & scb_state_change_notif_mask) != (scb->state & scb_state_change_notif_mask))
	{
		scb_state_upd_data_t data;
		data.scb = scb;
		data.oldstate = oldstate;
		bcm_notif_signal(scbstate->scb_state_notif_hdl, &data);
	}
}

/** reset all state. */
void
wlc_scb_resetstate(wlc_info_t *wlc, struct scb *scb)
{
	WL_NONE(("reset state\n"));

	wlc_scb_clearstatebit(wlc, scb, ~0);
}

/** set/change rateset and init/reset ratesel */
static void
wlc_scb_init_rates(wlc_info_t *wlc, wlc_bsscfg_t *cfg, enum wlc_bandunit bandunit, struct scb *scb)
{
	wlcband_t *band = wlc->bandstate[bandunit];
	wlc_rateset_t *rs;

	/* use current, target, or per-band default rateset? */
	if (wlc->pub->up &&
	    wlc_valid_chanspec_db(wlc->cmi, cfg->target_bss->chanspec))
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
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_rateset_filter(rs /* src */, &scb->rateset /* dst */,
		                   FALSE, WLC_RATES_OFDM, RATE_MASK,
		                   wlc_get_mcsallow(wlc, cfg));
	}
	else if (BSSCFG_AP(cfg)) {
		/* XXX Does not match with the comment above. Remove the
		 * HT rates and possibly OFDM rates if not needed. If there
		 * is a valid reason to add the HT rates, then check if we have to
		 * add VHT rates as well.
		 */
		uint8 mcsallow = BSS_N_ENAB(wlc, cfg) ? WLC_MCS_ALLOW : 0;
		wlc_rateset_filter(rs /* src */, &scb->rateset /* dst */,
		                   TRUE, WLC_RATES_CCK_OFDM, RATE_MASK,
		                   mcsallow);
	}
	else if (!cfg->BSS && band->gmode) {
		wlc_rateset_filter(rs /* src */, &scb->rateset /* dst */,
				FALSE, WLC_RATES_CCK, RATE_MASK, 0);
		/* if resulting set is empty, then take all network rates instead */
		if (scb->rateset.count == 0) {
			wlc_rateset_filter(rs /* src */, &scb->rateset /* dst */,
					FALSE, WLC_RATES_CCK_OFDM, RATE_MASK, 0);
		}
	}
	else {
		wlc_rateset_filter(rs /* src */, &scb->rateset /* dst */,
				FALSE, WLC_RATES_CCK_OFDM, RATE_MASK, 0);
	}

	if (!SCB_INTERNAL(scb)) {
		wlc_scb_ratesel_init(wlc, scb);
#ifdef STA
		/* send ofdm rate probe */
		if (BSSCFG_STA(cfg) && !cfg->BSS && band->gmode &&
		    wlc->pub->up)
			wlc_rateprobe(wlc, cfg, &scb->ea, WLC_RATEPROBE_RSPEC);
#endif /* STA */
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
		band = wlc_scbband(wlc, scb);
		if (BAND_2G(band->bandtype) && (band->gmode == GMODE_LEGACY_B)) {
			rs = &cck_rates;
			cck_only = TRUE;
		} else {
			rs = &band->hw_rateset;
			cck_only = FALSE;
		}
		if (!wlc_rate_hwrs_filter_sort_validate(&scb->rateset /* [in+out] */, rs /* [in] */,
			FALSE, wlc->stf->op_txstreams)) {
			/* continue with default rateset.
			 * since scb rateset does not carry basic rate indication,
			 * clear basic rate bit.
			 */
			WL_RATE(("wl%d: %s: invalid rateset in scb 0x%p bandunit 0x%x "
				"phy_type 0x%x gmode 0x%x\n", wlc->pub->unit, __FUNCTION__,
				OSL_OBFUSCATE_BUF(scb), band->bandunit,
				band->phytype, band->gmode));
#ifdef BCMDBG
			wlc_rateset_show(wlc, &scb->rateset, &scb->ea);
#endif // endif

			wlc_rateset_default(wlc, &scb->rateset, &band->hw_rateset,
			                    band->phytype, band->bandtype, cck_only, RATE_MASK,
			                    wlc_get_mcsallow(wlc, scb->bsscfg),
			                    CHSPEC_WLC_BW(scb->bsscfg->current_bss->chanspec),
			                    wlc->stf->op_txstreams);
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

/** @param[in] bandunit   BAND_2G_INDEX,BAND_5G_INDEX,... */
static INLINE struct scb* BCMFASTPATH
_wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
             enum wlc_bandunit bandunit)
{
	scb_module_t *scbstate = wlc->scbstate;
	int indx;
	struct scb_info *scbinfo;
	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

	/* All callers of wlc_scbfind() should first be checking to see
	 * if the SCB they're looking for is a BC/MC address.  Because we're
	 * using per bsscfg BCMC SCBs, we can't "find" BCMC SCBs without
	 * knowing which bsscfg.
	 */
	ASSERT(ea && !ETHER_ISMULTI(ea));

	/* search for the scb which corresponds to the remote station ea */
	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, bsscfg);
	if (scb_cfg && scb_cfg->scbhash[bandunit]) {
		indx = SCBHASHINDEX(scb_cfg->nscbhash, ea->octet);
		scbinfo =
			SCBINFO(scbstate, scb_cfg->scbhash[bandunit][indx]);
		for (; scbinfo; scbinfo = scbinfo->hashnext) {
			SCBSANITYCHECK(scbstate, SCBPUB(scbstate, scbinfo));
			if (memcmp((const char*)ea,
				(const char*)&(SCBPUB(scbstate, scbinfo)->ea),
				sizeof(*ea)) == 0)
				break;
		}

		return SCBPUB(scbstate, scbinfo);
	}
	return (NULL);
}

/** Find station control block corresponding to the remote id */
struct scb * BCMFASTPATH
wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	struct scb *scb = NULL;

	scb = _wlc_scbfind(wlc, bsscfg, ea, wlc->band->bandunit);

#if defined(WLMCHAN)
/* current band could be different, so search again for all scb's */
	if MCHAN_ACTIVE(wlc->pub) {
		enum wlc_bandunit bandunit;
		FOREACH_WLC_BAND(wlc, bandunit) {
			if (scb != NULL) {
				break;
			}
			scb = wlc_scbfindband(wlc, bsscfg, ea, bandunit);
		}
	}
#endif /* WLMCHAN */
	return scb;
}

/**
 * Lookup station control block corresponding to the remote id.
 * If not found, create a new entry.
 */
static INLINE struct scb *
_wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
               enum wlc_bandunit bandunit)
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
wlc_scblookupband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
                  enum wlc_bandunit bandunit)
{
	ASSERT(is_valid_bandunit(wlc, bandunit));

	return (_wlc_scblookup(wlc, bsscfg, ea, bandunit));
}

/**
 * Get scb from band
 *
 * @param[in] bandunit   BAND_2G_INDEX,BAND_5G_INDEX, ...
 */
struct scb * BCMFASTPATH
wlc_scbfindband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
                enum wlc_bandunit bandunit)
{
	ASSERT(is_valid_bandunit(wlc, bandunit));

	return (_wlc_scbfind(wlc, bsscfg, ea, bandunit));
}

void
wlc_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit,
	wlc_bsscfg_t *bsscfg)
{
	scb_module_t *scbstate = wlc->scbstate;

	/* first, del scb from hash table in old band */
	wlc_scb_hash_del(scbstate, bsscfg, scb);
	/* next add scb to hash table in new band */
	wlc_scb_hash_add(scbstate, bsscfg, new_bandunit, scb);
	/* XXX JQL: we should move/add at least rateset/ratesel init
	 * code here as band change definitely needs it.
	 */
	return;
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
	enum wlc_bandunit bandunit;
	bool reinit = FALSE;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			bandunit = CHSPEC_BANDUNIT(chanspec);
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
					          OSL_OBFUSCATE_BUF(stale_scb),
					          wlc_bandunit_name(bandunit)));
					/* mark the scb for removal */
					stale_scb->mark |= SCB_MARK_TO_REM;
				}
				/* Now perform the move of our scb to the new band */
				wlc_scb_switch_band(wlc, scb, bandunit, bsscfg);
				reinit = TRUE;
			}
		}
	}
	/* remove stale scb's marked for removal */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->mark & SCB_MARK_TO_REM) {
			WL_ASSOC(("remove stale scb %p\n", OSL_OBFUSCATE_BUF(scb)));
			scb->mark &= ~SCB_MARK_TO_REM;
			wlc_scbfree(wlc, scb);
		}
	}

	if (reinit) {
		wlc_scb_reinit(wlc);
	}
}

struct scb *
wlc_scbibssfindband(wlc_info_t *wlc, const struct ether_addr *ea, enum wlc_bandunit bandunit,
                    wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	ASSERT(is_valid_bandunit(wlc, bandunit));

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
                   const struct ether_addr *ea, enum wlc_bandunit bandunit, wlc_bsscfg_t **bsscfg)
{
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;
	int idx;

	ASSERT(is_valid_bandunit(wlc, bandunit));
	*bsscfg = NULL;
	FOREACH_ALL_WLC_BSS(wlc, idx, cfg) {
		if (wlc_bsscfg_get_multmac_entry_idx(cfg, hwaddr) != MULTIMAC_ENTRY_INVALID) {
			scb = _wlc_scbfind(wlc, cfg, ea, bandunit);
#ifdef WLSLOTTED_BSS
			if (BSSCFG_SLOTTED_BSS(cfg)) {
				enum wlc_bandunit bu;
				FOREACH_WLC_BAND(wlc, bu) {
					if (scb != NULL) {
						break;
					}
					scb = _wlc_scbfind(wlc, cfg, ea, bu);
				}
			}
#endif /* WLSLOTTED_BSS */
			if (scb != NULL) {
				*bsscfg = cfg;
				break;
			}
		}
	}

	return scb;
}

struct scb *
wlc_scbfind_from_wlcif(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr)
{
	struct scb *scb = NULL;
	wlc_bsscfg_t *bsscfg;
	char *bss_addr;

	if (wlcif && (wlcif->type == WLC_IFTYPE_WDS)) {
		return (wlcif->u.scb);
	}

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (BSSCFG_STA(bsscfg) && bsscfg->BSS) {
#ifdef WLTDLS
		if (TDLS_ENAB(wlc->pub)) {
			/* When a TDLS is active and running we will have an SCB specified
			 * to the peer MAC address. In that case we should find the directed
			 * SCB rather than going with the scb of the AP
			 */
			if (!ETHER_ISMULTI(addr) && !ETHER_ISNULLADDR(addr))
				scb = _wlc_tdls_scbfind_all(wlc, (struct ether_addr *)addr);
		}
#endif // endif
		if (scb == NULL) {
			bss_addr = (char *)&bsscfg->BSSID;
			/* We may have zeroed bssid, get previos one */
			if (ETHER_ISNULLADDR(bss_addr))
				bss_addr = (char *)&bsscfg->prev_BSSID;
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)bss_addr);
		}
	} else if (!ETHER_ISMULTI(addr)) {
		if (BSSCFG_SLOTTED_BSS(bsscfg)) {
			scb = wlc_scbfind_dualband(wlc, bsscfg, (struct ether_addr *)addr);
		} else {
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)addr);
		}
	} else {
		scb = bsscfg->bcmc_scb;
	}
	return scb;
}

static void
wlc_scb_hash_add(scb_module_t *scbstate, wlc_bsscfg_t *bsscfg, enum wlc_bandunit bandunit,
                 struct scb *scb)
{
	scb_bsscfg_cubby_t *scb_cfg;
	int indx;
	struct scb_info *scbinfo;

	ASSERT(bsscfg != NULL);

	scb->bandunit = bandunit;

	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, scb->ea.octet);
	scbinfo =
	           SCBINFO(scbstate, scb_cfg->scbhash[bandunit][indx]);

	SCBINFO(scbstate, scb)->hashnext = scbinfo;
#ifdef SCB_MEMDBG
	SCBINFO(scbstate, scb)->hashnext_copy = SCBINFO(scbstate, scb)->hashnext;
#endif // endif

	scb_cfg->scbhash[bandunit][indx] = scb;
}

static void
wlc_scb_hash_del(scb_module_t *scbstate, wlc_bsscfg_t *bsscfg, struct scb *scbd)
{
	scb_bsscfg_cubby_t *scb_cfg;
	int indx;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbstate, scbd);
	enum wlc_bandunit bandunit = scbd->bandunit;

	ASSERT(bsscfg != NULL);

	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, scbd->ea.octet);

	/* delete it from the hash */
	scbinfo = SCBINFO(scbstate, scb_cfg->scbhash[bandunit][indx]);
	/* If SCB is not in the hash table, return */
	if (scbinfo == NULL) {
		goto done;
	}

	SCBSANITYCHECK(scbstate, SCBPUB(scbstate, scbinfo));
	/* special case for the first */
	if (scbinfo == remove) {
		if (scbinfo->hashnext)
			SCBSANITYCHECK(scbstate, SCBPUB(scbstate, scbinfo->hashnext));
		scb_cfg->scbhash[bandunit][indx] =
		        SCBPUB(scbstate, scbinfo->hashnext);
	} else {
		for (; scbinfo; scbinfo = scbinfo->hashnext) {
			SCBSANITYCHECK(scbstate, SCBPUB(scbstate, scbinfo->hashnext));
			if (scbinfo->hashnext == remove) {
				scbinfo->hashnext = remove->hashnext;
#ifdef SCB_MEMDBG
				scbinfo->hashnext_copy = scbinfo->hashnext;
#endif // endif
				break;
			}
		}
	done:
		if (scbinfo == NULL) {
			WL_ERROR(("wl%d.%d: Unable to find scbinfo, scb = %p in the hash table",
				WLCWLUNIT(bsscfg->wlc), WLC_BSSCFG_IDX(bsscfg),
				OSL_OBFUSCATE_BUF(scbd)));
		}
	}
}

void
wlc_scb_sortrates(wlc_info_t *wlc, struct scb *scb)
{
	wlcband_t *band = wlc_scbband(wlc, scb);

	wlc_rate_hwrs_filter_sort_validate(&scb->rateset /* [in+out] */,
		&band->hw_rateset /* [in] */, FALSE,
		wlc->stf->op_txstreams);
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

struct scb *
wlc_scbfind_dualband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *addr)
{
	struct scb *scb;
	enum wlc_bandunit bandunit;

	scb = wlc_scbfind(wlc, bsscfg, addr);
	if (scb) {
		return scb;
	}
#ifdef WLRSDB
	if (RSDB_ACTIVE(wlc->pub) &&
		((bsscfg = wlc_rsdb_bsscfg_get_linked_cfg(wlc->rsdbinfo, bsscfg)) != NULL)) {
		wlc = bsscfg->wlc;
		return wlc_scbfind(wlc, bsscfg, addr);
	}
#endif /* RSDB */
	if (IS_SINGLEBAND(wlc))
		return scb;

	FOREACH_WLC_BAND(wlc, bandunit) {
		if (scb != NULL) {
			break;
		}
		scb = wlc_scbfindband(wlc, bsscfg, addr, bandunit);
	}

	return scb;
}

#if defined(BCMDBG)
void
wlc_scb_dump_scb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb, struct bcmstrbuf *b, int idx)
{
	int i;
	char eabuf[ETHER_ADDR_STR_LEN];
	char flagstr[64];
	char flagstr2[64];
	char flagstr3[64];
	char statestr[64];
#ifdef AP
	char ssidbuf[SSID_FMT_BUF_LEN] = "";
#endif /* AP */
	scb_module_t *scbstate = wlc->scbstate;

	bcm_format_flags(scb_flags, scb->flags, flagstr, 64);
	bcm_format_flags(scb_flags2, scb->flags2, flagstr2, 64);
	bcm_format_flags(scb_flags3, scb->flags3, flagstr3, 64);
	bcm_format_flags(scb_states, scb->state, statestr, 64);

	if (SCB_INTERNAL(scb))
		bcm_bprintf(b, "  I");
	else
		bcm_bprintf(b, "%3d", idx);
	bcm_bprintf(b, "%s %s\n", (scb->permanent? "*":" "),
	            bcm_ether_ntoa(&scb->ea, eabuf));

	bcm_bprintf(b, "     State:0x%02x (%s) Used:%d(%d)\n",
	            scb->state, statestr, scb->used,
	            (int)(scb->used - wlc->pub->now));
	bcm_bprintf(b, "     Band:%s", wlc_bandunit_name(scb->bandunit));
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
#ifdef WL_HAPD_WDS
	if (SCB_LEGACY_WDS(scb)) {
		bcm_bprintf(b, "     link_event %d", scb->wds->link_event);
		bcm_bprintf(b, "\n");
	}
#endif /* WL_HAPD_WDS */
	if (cfg != NULL)
		bcm_bprintf(b, "     Cfg:%d(%p)", WLC_BSSCFG_IDX(cfg), OSL_OBFUSCATE_BUF(cfg));

	bcm_bprintf(b, "\n");

	wlc_dump_rateset("     rates", &scb->rateset, b);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "     Prop HT rates support:%d\n",
	            SCB_HT_PROP_RATES_CAP(scb));

#ifdef AP
	if (cfg != NULL && BSSCFG_AP(cfg)) {
		bcm_bprintf(b, "     AID:0x%x PS:%d WDS:%d(%p)",
		            scb->aid, scb->PS, (scb->wds ? 1 : 0),
		            OSL_OBFUSCATE_BUF(scb->wds));
		wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
		bcm_bprintf(b, " BSS %d \"%s\"\n",
		            WLC_BSSCFG_IDX(cfg), ssidbuf);
	}
#endif // endif
#ifdef STA
	if (cfg != NULL && BSSCFG_STA(cfg)) {
		bcm_bprintf(b, "     MAXSP:%u DEFL:0x%x TRIG:0x%x DELV:0x%x\n",
		            scb->apsd.maxsplen, scb->apsd.ac_defl,
		            scb->apsd.ac_trig, scb->apsd.ac_delv);
	}
#endif // endif
	bcm_bprintf(b,  "     WPA_auth 0x%x wsec 0x%x\n", scb->WPA_auth, scb->wsec);

#if defined(STA) && defined(DBG_BCN_LOSS)
	bcm_bprintf(b,	"	  last_rx:%d last_rx_rssi:%d last_bcn_rssi: "
	            "%d last_tx: %d\n",
	            scb->dbg_bcn.last_rx, scb->dbg_bcn.last_rx_rssi, scb->dbg_bcn.last_bcn_rssi,
	            scb->dbg_bcn.last_tx);
#endif // endif
	for (i = 0; i < NUMPRIO; i++) {
		if (SCB_PKTS_INFLT_FIFOCNT_VAL(scb, i)) {
			bcm_bprintf(b, "pkts_inflt_fifocnt: prio-%d =>%d\n", i,
				SCB_PKTS_INFLT_FIFOCNT_VAL(scb, i));
		}
	}

	for (i = 0; i < (NUMPRIO * 2); i++) {
		if (SCB_PKTS_INFLT_CQCNT_VAL(scb, i)) {
			bcm_bprintf(b, "pkts_inflt_cqcnt: prec-%d =>%d\n", i,
				SCB_PKTS_INFLT_CQCNT_VAL(scb, i));
		}
	}

	bcm_bprintf(b, "scb base size: %u\n", (uint)sizeof(struct scb));
	bcm_bprintf(b, "-------- scb cubbies --------\n");

	wlc_cubby_dump(scbstate->cubby_info, scb, wlc_scb_sec_sz, scbstate, b);

	/* display scb state change callbacks */
	bcm_bprintf(b, "-------- state change notify list --------\n");
	bcm_notif_dump_list(scbstate->scb_state_notif_hdl, b);

#ifdef MEM_ALLOC_STATS
	/* display memory usage information */
	bcm_bprintf(b, "SCB Memory Allocated: %d bytes\n", scb->mem_bytes);
#endif // endif
}

static void
wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	wlc_info_t *wlc = scbstate->wlc;
	int k;
	struct scb *scb;
	struct scb_iter scbiter;

	bcm_bprintf(b, "idx  ether_addr\n");
	k = 0;
	FOREACH_BSS_SCB(scbstate, &scbiter, cfg, scb) {
		wlc_scb_dump_scb(wlc, cfg, scb, b, k);
		k++;
	}

	return;
}

static int
wlc_scb_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;
	scb_module_t *scbstate = wlc->scbstate;

#ifdef SCB_MEMDBG
	bcm_bprintf(b, "# of scbs: %u, scballoced[%u] scbfreed[%u] freelistcount[%u]\n",
		scbstate->nscb, scbstate->scballoced, scbstate->scbfreed,
		scbstate->freelistcount);
#else
	bcm_bprintf(b, "# of scbs: %u\n", scbstate->nscb);
#endif /* SCB_MEMDBG */

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_scb_bsscfg_dump(scbstate, bsscfg, b);
	}

#ifdef SCBFREELIST
	wlc_scbfreelist_dump(scbstate, b);
#endif /* SCBFREELIST */
	return 0;
}
#endif // endif

#if defined(WL_DATAPATH_LOG_DUMP)
void
wlc_scb_datapath_log_dump(wlc_info_t *wlc, struct scb *scb, int tag)
{
	/* Log a message to indiate what SCB is being dumped */
	EVENT_LOG(tag, "SCB "MACF" bsscfg %d\n",
	          scb->ea.octet[0], scb->ea.octet[1], scb->ea.octet[2],
	          scb->ea.octet[3], scb->ea.octet[4], scb->ea.octet[5],
	          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)));

	/* Call all the registered log dump fns */
	wlc_cubby_datapath_log_dump(wlc->scbstate->cubby_info, scb, tag);
}
#endif /* WL_DATAPATH_LOG_DUMP */

int
wlc_scb_state_upd_register(wlc_info_t *wlc, scb_state_upd_cb_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->scbstate->scb_state_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_scb_state_upd_unregister(wlc_info_t *wlc, scb_state_upd_cb_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->scbstate->scb_state_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

void wlc_scb_cleanup_unused(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t *bsscfg = NULL;

	if (!wlc->cleanup_unused_scbs)
		return;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bsscfg = SCB_BSSCFG(scb);
		if (!BSSCFG_STA(bsscfg) || (bsscfg->BSS && SCB_ASSOCIATED(scb)) ||
			(bsscfg->wlc != wlc) || BSSCFG_IS_TDLS(bsscfg))
			continue;
		/* we release the SCB if it was idle for more then 10 seconds */
		/* we could start a join process with scb that was idle for almost 10 seocnds */
		/* and in a rare case the watchdog for 10 seconds expiration would come */
		/* before the join complete and before we update "scb->used". to prevent this */
		/* problem we do not release SCB that is in the middle of join */
		if (!scb->permanent &&
			((wlc->pub->now - scb->used) >= 10) && !SCB_DURING_JOIN(scb))
			{
				wlc_scbfree(wlc, scb);
			}
	}
}

void
wlc_scbfind_delete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *ea)
{
	uint i;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	struct scb *scb;

	FOREACH_WLC_BAND(wlc, i) {
		scb = wlc_scbfindband(wlc, bsscfg, ea, i);
		if (scb) {
			WL_ASSOC(("wl%d: %s: scb for the STA-%s"
				" already exists\n", wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(ea, eabuf)));
			wlc_scbfree(wlc, scb);
		}
	}
}

#if defined(STA) && defined(DBG_BCN_LOSS)
int
wlc_get_bcn_dbg_info(wlc_bsscfg_t *cfg, struct ether_addr *addr,
	struct wlc_scb_dbg_bcn *dbg_bcn)
{
	wlc_info_t *wlc = cfg->wlc;

	if (cfg->BSS) {
		struct scb *scb = wlc_scbfindband(wlc, cfg, addr,
			CHSPEC_BANDUNIT(cfg->current_bss->chanspec));
		if (scb) {
			memcpy(dbg_bcn, &(scb->dbg_bcn), sizeof(struct wlc_scb_dbg_bcn));
			return BCME_OK;
		}
	}
	return BCME_ERROR;
}
#endif /* defined(STA) && defined(DBG_BCN_LOSS) */

/**
 * These function allocates/frees a secondary cubby in the secondary cubby area.
 */
void *
wlc_scb_sec_cubby_alloc(wlc_info_t *wlc, struct scb *scb, uint secsz)
{
	scb_module_t *scbstate = wlc->scbstate;

	return wlc_cubby_sec_alloc(scbstate->cubby_info, scb, secsz);
}

void
wlc_scb_sec_cubby_free(wlc_info_t *wlc, struct scb *scb, void *secptr)
{
	scb_module_t *scbstate = wlc->scbstate;

	wlc_cubby_sec_free(scbstate->cubby_info, scb, secptr);
}

#ifdef WLRSDB
int
wlc_scb_wlc_cfg_update(wlc_info_t *wlc_from, wlc_info_t *wlc_to, wlc_bsscfg_t *cfg_to,
	struct scb *scb)
{
	scb_module_t *to_scbstate = wlc_to->scbstate;
	wlc_bsscfg_t *bsscfg_from = scb->bsscfg;
	scb_module_t *from_scbstate = wlc_from->scbstate;

	/* Call the update function of the registered cubbies */
	wlc_cubby_update(from_scbstate->cubby_info, scb, cfg_to);

	wlc_scb_hash_del(wlc_from->scbstate, SCB_BSSCFG(scb), scb);
	/* delete it from the link list */
	wlc_scb_list_del(wlc_from->scbstate, bsscfg_from, scb);

	/* update total allocated scb number */
	from_scbstate->nscb--;
	to_scbstate->nscb++;

	/* Update band and bandunit */
	wlc_to->band->bandunit = CHSPEC_BANDUNIT(cfg_to->current_bss->chanspec);
	scb->bandunit = wlc_to->band->bandunit;

	/* add it to the link list */
	wlc_scb_list_add(wlc_to->scbstate, cfg_to, scb);
	/* install it in the cache */
	wlc_scb_hash_add(wlc_to->scbstate, cfg_to, wlc_to->band->bandunit, scb);

	/* force wlc_scb_set_bsscfg() */
	scb->bsscfg = cfg_to;
	scb->if_stats = cfg_to->wlcif->_cnt;

	/* update the last used time stamp from new wlc */
	scb->used = wlc_to->pub->now;

	wlc_scb_switch_band(wlc_to, scb, scb->bandunit, cfg_to);
	return BCME_OK;
}
#endif /* WLRSDB */

struct ether_addr *
wlc_scb_get_cfg_etheraddr(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = scb->bsscfg;
	if (scb->multimac_idx == MULTIMAC_ENTRY_INVALID) {
		return &bsscfg->cur_etheraddr;
	} else {
		return wlc_bsscfg_multmac_etheraddr(bsscfg, scb->multimac_idx);
	}
}
struct ether_addr *
wlc_scb_get_cfg_bssid(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = scb->bsscfg;
	if (scb->multimac_idx == MULTIMAC_ENTRY_INVALID) {
		return &bsscfg->BSSID;
	} else {
		return wlc_bsscfg_multmac_bssid(bsscfg, scb->multimac_idx);
	}
}
void
wlc_scb_get_cfg_etheraddr_bssid(struct scb *scb, struct ether_addr **ea,
	struct ether_addr **bssid)
{
	*ea = wlc_scb_get_cfg_etheraddr(scb);
	*bssid = wlc_scb_get_cfg_bssid(scb);
}

void
wlc_scb_update_multmac_idx(wlc_bsscfg_t *cfg_to, struct scb *scb)
{
	struct ether_addr *cfg_mac;

	if (scb->multimac_idx == MULTIMAC_ENTRY_INVALID) {
		return;
	}
	cfg_mac = wlc_bsscfg_multmac_etheraddr(scb->bsscfg, scb->multimac_idx);
	if (cfg_mac != NULL) {
		scb->multimac_idx = wlc_bsscfg_get_multmac_entry_idx(cfg_to, cfg_mac);
	}
}

void
wlc_scb_cq_inc(void *p, int prec, uint32 cnt)
{
	struct scb *scb;
	if (p == NULL) {
		return;
	}

	scb = WLPKTTAGSCBGET(p);

	if (scb == NULL) {
		return;
	}

#ifdef WLCFP
	if (CFP_ENAB(SCB_WLC(scb)->pub) == TRUE) {
		/* Note: Caller is expected to check there is enough space in the queue */
		wlc_cfp_cq_fifo_inc(SCB_WLC(scb), scb, PKTPRIO(p), cnt);
	}
#endif // endif
	SCB_PKTS_INFLT_CQCNT_ADD(scb, prec, cnt);

}

void
wlc_scb_cq_dec(void *p, int prec, uint32 cnt)
{
	struct scb *scb;

	if (p == NULL) {
		return;
	}

	scb = WLPKTTAGSCBGET(p);

	if (scb == NULL) {
		return;
	}

#ifdef WLCFP
	if (CFP_ENAB(SCB_WLC(scb)->pub) == TRUE) {
		wlc_cfp_cq_fifo_dec(SCB_WLC(scb), scb, PKTPRIO(p), cnt);
	}
#endif // endif
	SCB_PKTS_INFLT_CQCNT_SUB(scb, prec, cnt);
}

/** @param pq   Multi-priority packet queue */
void
wlc_scb_cq_flush_queue(wlc_info_t *wlc, struct pktq *pq)
{
	void *p;
	int prec;

	PKTQ_PREC_ITER(pq, prec) {
		/* assuming single threaded operation,
		 * otherwise a mutex is needed.
		 */

		/* start with the head of the list */
		while ((p = pktq_pdeq(pq, prec)) != NULL) {
			/* delete this packet */
			wlc_scb_cq_dec(p, prec, 1);
#ifdef WLTAF
			wlc_taf_txpkt_status(wlc->taf_handle, NULL, TAF_PREC(prec), p,
				TAF_TXPKT_STATUS_PKTFREE_DROP);
#endif // endif
			PKTFREE(wlc->osh, p, TRUE);
		}
	}
}

#if defined(BCM_PKTFWD_FLCTL)

/* Get SCB queue lengths of a station. */
void
wlc_scb_get_link_credits(wlc_info_t *wlc, wlc_if_t *wlcif, uint8 *addr,
	uint16 cfp_flowid, int32 *credits)
{
	struct scb *scb;

#if defined(WLCFP)
	if (cfp_flowid != ID16_INVALID) {
		scb = wlc_cfp_flowid_2_scb(wlc, cfp_flowid);
	} else
#endif /* WLCFP */
	{
		scb = wlc_scbfind_from_wlcif(wlc, wlcif, addr);
	}

	ASSERT(scb != NULL);
	/* TODO: Check for station capability (AMPDU, NAR, etc) and update credits accordingly */
	wlc_ampdu_get_link_credits(wlc, scb, credits);
}

/**
 * Link SCB to the wfd->pktc layer (via the PT table).
 * In NIC mode, wfd->pktc is effectively the "bus" layer.
 *
 * This function is invoked, when the Linux bridge creates a new entry into
 * the PT table via PKTC_TBL_UPDATE.
 */
int
wlc_scb_link_update(wlc_info_t *wlc, wlc_if_t *wlcif, uint8 *addr,
	uint16 *cfp_flowid)
{
	int ret = BCME_OK;
	struct scb *scb;

	if (ETHER_ISMULTI(addr) || ETHER_ISNULLADDR(addr)) {
		WL_INFORM(("SCB linkup multi or NULL addr:"MACF"\n", ETHERP_TO_MACF(addr)));
		return BCME_ERROR;
	}

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, addr);

	if ((scb == NULL) || !(SCB_ASSOCIATED(scb) || SCB_LEGACY_WDS(scb))) {
		WL_INFORM(("SCB linkup cannot find scb for MAC:"MACF"\n", ETHERP_TO_MACF(addr)));
		return BCME_ERROR;
	}

	*cfp_flowid = ID16_INVALID;

#if defined(WLCFP)
	if (CFP_ENAB(wlc->pub)) {
		/* tcb_state is not linked */
		ret = wlc_scb_cfp_tcb_link(wlc, 0, 0, scb, NULL, cfp_flowid);
	}
#endif /* WLCFP */

	return ret;
} /* wlc_scb_link_update() */

#endif /* BCM_PKTFWD_FLCTL */
