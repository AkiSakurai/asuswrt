/*
 * D11 rate and link memory support module for for Broadcom 802.11
 * Networking Adapter Device Drivers
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *
 * $Id$
 */

/**
 * @file
 * @brief
 * XXX Confluence: [Rate Memory and Link Memory support (D11 corerev 128)]
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
#include <wlioctl.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc.h>
#include <wlc_pub.h>
#include <wlc_types.h>
#include <wlc_ratelinkmem.h>
#include <wlc_scb.h>
#include <wl_dbg.h>
#include <wlc_dump.h>
#include <wlc_keymgmt.h>
#include <wlc_tx.h>
#include <wlc_txc.h>
#include <wlc_ampdu.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_scb_ratesel.h>
#include <wlc_txbf.h>
#include <wlc_event_utils.h>
#include <wlc_vht.h>
#include <wlc_he.h>
#include <wlc_macdbg.h>
#include <wlc_macreq.h>
#include <wlc_musched.h>
#include <wlc_scan.h>

#if defined(WL_RATELINKMEM) && !defined(WL_RATELINKMEM_DISABLED) && !defined(HNDBME)
#error "WL_RATELINKMEM requires HNDBME"
#endif /* WL_RATELINKMEM && !WL_RATELINKMEM_DISABLED && !HNDBME */
#include <hndbme.h>
#ifdef BCM_SECURE_DMA
#include <hnddma_priv.h>
#endif /* BCM_SECURE_DMA */

/* Private definitions */

/* number of local rate entries cached per SCB, must be pwr of 2, max 128 */
#define MAX_RATE_ENTRIES			8

/* Maximum nr of entries in RATE and LINK memories (defined by HW) */
/* NOTE: module assumes this value >= AMT_SIZE and >= MAXSCB */
#define D11_MAX_RATELINK_ENTRIES		256
/* Size of entry in HW RATE memory (defined by HW) */
#define D11_RATE_MEM_HW_BLOCK_SIZE		256
/* Size of entry in HW LINK memory (defined by HW) */
#define D11_LINK_MEM_HW_BLOCK_SIZE		64
/* Size of entry in HW LINK memory (defined by HW, increased for rev132) */
#define D11_LINK_MEM_HW_BLOCK_SIZE_REV132	128

#define D11_RATEIDX_UPPER128_MASK		0x80 /* 128 */

#define D11_MAC_MEMORY_REGION_OFFSET		0x00800000
#define D11_RATE_MEM_OFFSET			0x000C0000
#define D11_LINK_MEM_OFFSET			0x000D0000
#define D11_LINK_MEM_OFFSET_REV132		0x000E0000

#define D11_RATE_MEM_BASE_ADDR_OFF (D11_MAC_MEMORY_REGION_OFFSET + D11_RATE_MEM_OFFSET)
#define D11_LINK_MEM_BASE_ADDR_OFF (D11_MAC_MEMORY_REGION_OFFSET + D11_LINK_MEM_OFFSET)
#define D11_LINK_MEM_BASE_ADDR_OFF_REV132 (D11_MAC_MEMORY_REGION_OFFSET + \
	D11_LINK_MEM_OFFSET_REV132)

/** offsets relative to the chipcommon core backplane address */
#define RATE_OFFSET(index) (D11_RATE_MEM_BASE_ADDR_OFF + ((index) * D11_RATE_MEM_HW_BLOCK_SIZE))
#define LINK_OFFSET_REV132(index) (D11_LINK_MEM_BASE_ADDR_OFF_REV132 + \
	((index) * D11_LINK_MEM_HW_BLOCK_SIZE_REV132))
#define LINK_OFFSET(index) (D11_LINK_MEM_BASE_ADDR_OFF + ((index) * D11_LINK_MEM_HW_BLOCK_SIZE))

/** physical address of ratemem entry, from the BME's point of view */
#define D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, index) \
	(SI_ENUM_BASE_BP(wlc->pub->sih) + RATE_OFFSET(index))

/** physical address of linkmem entry, from the BME's point of view */
#define D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, index) (SI_ENUM_BASE_BP(wlc->pub->sih) + \
	(D11REV_IS(wlc->pub->corerev, 132) ? LINK_OFFSET_REV132(index) : \
	LINK_OFFSET(index)))

/* convenience macros for entry sizes */
#define WLC_RLM_RATE_ENTRY_SIZE			(168)
#define WLC_RLM_LINK_ENTRY_SIZE_SW		(48)
#define WLC_RLM_LINK_ENTRY_TXBF_BLOCKSIZE	(16) /* TXBF ucode internal */
#define WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE	(0) /* ucode internal */
#define WLC_RLM_LINK_ENTRY_SIZE_TXBF		(WLC_RLM_LINK_ENTRY_SIZE_SW + \
						WLC_RLM_LINK_ENTRY_TXBF_BLOCKSIZE)
#define WLC_RLM_LINK_ENTRY_SIZE_FULL		(WLC_RLM_LINK_ENTRY_SIZE_TXBF + \
						WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE)

#define RATE_STORE_IDX(idx)			((idx) & (MAX_RATE_ENTRIES-1))

#define C_RTMCTL_RST_NBIT    12 // reset current rmem idx
#define	C_RTMCTL_UNLOCK_NBIT 14 // unlock rmem idx

typedef enum {
	RATE_ENTRY_STATE_UNINITED	= 0,
	RATE_ENTRY_STATE_ACTIVE		= 1
} rate_entry_state_t;

typedef enum {
	LINK_ENTRY_STATE_UNINITED	= 0,
	LINK_ENTRY_STATE_ACTIVE		= 1
} link_entry_state_t;

/** SCB cubby definition */
typedef struct scb_ratelinkmem_info {
	rate_entry_state_t		rate_state;		/** rate mem statemachine */
	link_entry_state_t		link_state;		/** link mem statemachine */
	uint16				rate_link_index;	/** index in rate/link memories */
	bool				cubby_valid;		/** cubby initted */
	uint8				rmem_nupd;		/** # ratemem[] gets updated */
	uint8				rmem_curptr;		/** b0 = latest rate entry posted */
	uint8				rmem_txptr;		/** b0 = rate entry for latest tx */
	uint8				cur_link_count;		/** for internal usage only */
	uint8				rmem_maxdepth;		/** max historic diff cur & txptr */
	wlc_rlm_rate_store_t		rate_store[MAX_RATE_ENTRIES]; /** cached rspec entries */
	d11linkmem_entry_t		link_entry;		/** cached link entry */
	uint16				rmem_mismatch;		/** # mismatch */
} scb_ratelinkmem_info_t;

/* cubby access macros */
#define SCB_RLM_CUBBY(rlmi, scb)	((scb_ratelinkmem_info_t *)SCB_CUBBY(scb, (rlmi)->scbh))

struct wlc_ratelinkmem_info {
	wlc_info_t	*wlc;		/* back pointer to main wlc structure */
	int		scbh;		/* SCB cubby handle */
	scb_t		*scb_index[D11_MAX_RATELINK_ENTRIES];	/* lookup table index -> SCB */
	int		bme_key;	/* bme_key returned for BME_USR_RLM registration */
	int		bme_eng_idx;	/* eng_idx returned from bme_copy for BME_USR_RLM */
	dmaaddr_t	rate_dma_pa;	/* physical address for last DMA-ed rate_entry */
	uint		rate_dma_sz;	/* last DMA'ed size */
	d11ratemem_rev128_entry_t rate_entry; /* single rate entry for filling and copying to HW */
	uint8		null_link_entry[WLC_RLM_LINK_ENTRY_SIZE_FULL];	/* used for erasing */
	bool		block_in_release; /* block new calls into RLM during release of AMT */
};

/* Local function declarations */
static int wlc_ratelinkmem_scb_init(void *ctx, struct scb *scb);
static void wlc_ratelinkmem_scb_deinit(void *ctx, struct scb *scb);
static void wlc_ratelinkmem_reset_special_rate_scb(wlc_ratelinkmem_info_t *rlmi);
static void wlc_ratelinkmem_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#if defined(BCMDBG) || defined(DUMP_RATELINKMEM)
static int wlc_ratelinkmem_module_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif
static bool wlc_ratelinkmem_init_indices(wlc_info_t *wlc, scb_t *scb,
	scb_ratelinkmem_info_t *scbh, int index);
static uint16 wlc_ratelinkmem_get_scb_amt_index(wlc_info_t *wlc, scb_t *scb);
static int wlc_ratelinkmem_update_link_entry_onsize(wlc_info_t *wlc, scb_t *scb,
	uint16 size, bool clr_txbf_stats);
static void wlc_ratelinkmem_delete_link_entry(wlc_info_t *wlc, scb_t *scb);
static int BCMFASTPATH wlc_ratelinkmem_upd_rmem(wlc_ratelinkmem_info_t *rlmi,
	uint8 *buf, uint sz, uint16 rmem_idx);

/* Private functions definitions */

/** Called upon init of an SCB, initialize cubby */
static int
wlc_ratelinkmem_scb_init(void *ctx, struct scb *scb)
{
	wlc_ratelinkmem_info_t *rlmi = ctx;
	wlc_info_t *wlc = rlmi->wlc;
	scb_ratelinkmem_info_t *scbh;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	WL_TRACE(("wl%d: %s, %p, %p\n", wlc->pub->unit, __FUNCTION__, scb, scbh));
	BCM_REFERENCE(wlc);

	/* initialize cubby state for fresh start */
	scbh->rate_state = RATE_ENTRY_STATE_UNINITED;
	scbh->link_state = LINK_ENTRY_STATE_UNINITED;
	scbh->rate_link_index = D11_RATE_LINK_MEM_IDX_INVALID;
	scbh->rmem_nupd = 0;
	scbh->rmem_curptr = 0;
	scbh->rmem_txptr = 0;
	scbh->cur_link_count = 0;
	scbh->cubby_valid = TRUE;
	scbh->rmem_maxdepth = 0;

	RL_RMEM1(("%s init scb %p: ", __FUNCTION__, scb));

	return BCME_OK;
}

/** Called upon deinit of an SCB, cleanup */
static void
wlc_ratelinkmem_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_ratelinkmem_info_t *rlmi = ctx;
	wlc_info_t *wlc = rlmi->wlc;
	scb_ratelinkmem_info_t *scbh;
	int i;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	WL_TRACE(("wl%d: %s, %p, %p\n", wlc->pub->unit, __FUNCTION__, scb, scbh));
	BCM_REFERENCE(wlc);

	scbh->cubby_valid = FALSE;

	/* remove SCB from lookup table */
	for (i = 0; i < D11_MAX_RATELINK_ENTRIES; i++) {
		if (rlmi->scb_index[i] == scb) {
			rlmi->scb_index[i] = NULL;
			break;
		}
	}
	if (scbh->link_state != LINK_ENTRY_STATE_UNINITED) {
		wlc_ratelinkmem_delete_link_entry(wlc, scb);
	}
}

static void
wlc_ratelinkmem_reset_special_rate_scb(wlc_ratelinkmem_info_t *rlmi)
{
	wlc_info_t *wlc = rlmi->wlc;
	scb_ratelinkmem_info_t *scbh = SCB_RLM_CUBBY(rlmi, WLC_RLM_SPECIAL_RATE_SCB(wlc));

	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));
	ASSERT(scbh != NULL);
	if ((scbh->rate_state == RATE_ENTRY_STATE_UNINITED) &&
		(scbh->link_state == LINK_ENTRY_STATE_UNINITED)) {
		ASSERT(scbh->rate_link_index == D11_RATE_LINK_MEM_IDX_INVALID);
		/* New SCB is not initted yet, only need to clear pointer (below) */
	} else {
		ASSERT(scbh->rate_link_index == WLC_RLM_SPECIAL_RATE_IDX);

		/* New SCB was already used, need to clear to force update later */
		scbh->rate_link_index = D11_RATE_LINK_MEM_IDX_INVALID;
		scbh->rate_state = RATE_ENTRY_STATE_UNINITED;
		scbh->link_state = LINK_ENTRY_STATE_UNINITED;
	}
	rlmi->scb_index[WLC_RLM_SPECIAL_RATE_IDX] = NULL;
}

/* dump rate entry's rate storage info */
void
wlc_ratelinkmem_rate_store_dump(wlc_info_t *wlc, const wlc_rlm_rate_store_t *rate_store,
	struct bcmstrbuf *b)
{
	ASSERT(rate_store != NULL);

	bcm_bprintf(b, "flags: 0x%x, rspecs: 0x%x 0x%x 0x%x 0x%x, use_rts: 0x%x\n",
		rate_store->flags, rate_store->rspec[0], rate_store->rspec[1],
		rate_store->rspec[2], rate_store->rspec[3], rate_store->use_rts);
}

#if defined(BCMDBG) || defined(DUMP_RATELINKMEM)

/** dump an interpreted single rateenty */
static void
wlc_ratelinkmem_rate_entry_dump(wlc_info_t *wlc, d11ratemem_rev128_entry_t *rate_entry,
	struct bcmstrbuf *b)
{
	int k, i;
	ratespec_t rspec;

	ASSERT(rate_entry != NULL);

	bcm_bprintf(b, "flags: 0x%04x\n", rate_entry->flags);
	for (k = 0; k < D11_REV128_RATEENTRY_NUM_RATES; k++) {
		d11ratemem_rev128_rate_t *rate_info_block = &rate_entry->rate_info_block[k];
		rspec = 0;
		wf_plcp_to_rspec(
			ltoh16(rate_info_block->TxPhyCtl[0]) & D11_REV80_PHY_TXC_FTFMT_MASK,
			rate_info_block->plcp, &rspec);
		bcm_bprintf(b, "\t[%d]\tRateCtl: 0x%x rspec: 0x%x\n", k, rate_info_block->RateCtl,
			rspec);
		bcm_bprintf(b, "\t\tplcp:");
		for (i = 0; i < D11_PHY_HDR_LEN; i++) {
			bcm_bprintf(b, " %02x", rate_info_block->plcp[i]);
		}
		bcm_bprintf(b, "\n\t\tTxPhyCtl:");
		for (i = 0; i < D11_REV128_RATEENTRY_TXPHYCTL_WORDS; i++) {
			bcm_bprintf(b, " %04x", rate_info_block->TxPhyCtl[i]);
		}
		bcm_bprintf(b, "\n\t\tBFM0: 0x%04x", rate_info_block->BFM0);
		bcm_bprintf(b, "\ttxpwr_bw:");
		for (i = 0; i < D11_REV128_RATEENTRY_NUM_BWS; i++) {
			bcm_bprintf(b, " %02x", rate_info_block->txpwr_bw[i]);
		}
		bcm_bprintf(b, "\n\t\tFbwCtl: 0x%x", rate_info_block->FbwCtl);
		bcm_bprintf(b, "\tFbwPwr:");
		for (i = 0; i < D11_REV128_RATEENTRY_FBWPWR_WORDS; i++) {
			bcm_bprintf(b, " %04x", rate_info_block->FbwPwr[i]);
		}
		bcm_bprintf(b, "\n");
	}
}

#define RTLKMEM_DUMP_FLAG_RAWDUMP	0x1
#define RTLKMEM_DUMP_FLAG_LINKONLY	0x2
#define RTLKMEM_DUMP_FLAG_RATEONLY	0x4
#define RTLKMEM_DUMP_FLAG_PARSED	0x8
#define RTLKMEM_DUMP_FLAG_PINGPONG	0x10
/** dump raw link and rate mem for index */
static void
wlc_ratelinkmem_dump_raw_bytes(wlc_info_t *wlc, uint16 index, struct bcmstrbuf *b, uint8 flags)
{
	int  i;
	uint32 bp_addr;
	uint erate[D11_RATE_MEM_HW_BLOCK_SIZE/4];
	uint elink[WLC_RLM_LINK_ENTRY_SIZE_FULL/4];
	char name[48];
	d11linkmem_entry_t * link_entry = (d11linkmem_entry_t *) elink;
	d11ratemem_rev128_entry_t * rate_entry = (d11ratemem_rev128_entry_t *) erate;
	const int link_size = WLC_RLM_LINK_ENTRY_SIZE_FULL;

	if (!wlc->clk) {
		bcm_bprintf(b, "N/A (no clock)\n");
		return;
	}
	if ((flags & RTLKMEM_DUMP_FLAG_LINKONLY) == 0) {
		if ((flags & RTLKMEM_DUMP_FLAG_PINGPONG) != 0) {
			W_REG(wlc->osh, D11_PSM_RATEMEM_DBG(wlc), (uint16)4); // ping
			snprintf(name, sizeof(name), "RateMem Block (Ping): %d Size:%dB",
				(int) index, (int) D11_RATE_MEM_HW_BLOCK_SIZE);
		} else {
			snprintf(name, sizeof(name), "RateMem Block: %d Size:%dB", (int) index,
				(int) D11_RATE_MEM_HW_BLOCK_SIZE);
		}
		bp_addr = D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, index);
		for  (i = 0; i < D11_RATE_MEM_HW_BLOCK_SIZE / 4; i++) {
			si_backplane_access(wlc->pub->sih, bp_addr, 4, erate + i, TRUE);
			bp_addr += 4;
		}
		if ((flags & RTLKMEM_DUMP_FLAG_RAWDUMP) != 0) {
			bcm_bprbytes(b, name,
			             D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, index),
			             (uchar *) erate, D11_RATE_MEM_HW_BLOCK_SIZE);
		}
		if ((flags & RTLKMEM_DUMP_FLAG_PARSED) != 0) {
			wlc_ratelinkmem_rate_entry_dump(wlc, rate_entry, b);
		}
		if ((flags & RTLKMEM_DUMP_FLAG_PINGPONG) != 0) {
			W_REG(wlc->osh, D11_PSM_RATEMEM_DBG(wlc), (uint16)0xc); // pong
			bp_addr = D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, index);
			for  (i = 0; i < D11_RATE_MEM_HW_BLOCK_SIZE / 4; i++) {
				si_backplane_access(wlc->pub->sih, bp_addr, 4, erate + i, TRUE);
				bp_addr += 4;
			}
			snprintf(name, sizeof(name), "RateMem Block (Pong): %d Size:%dB",
				(int) index, (int) D11_RATE_MEM_HW_BLOCK_SIZE);
			if ((flags & RTLKMEM_DUMP_FLAG_RAWDUMP) != 0) {
				bcm_bprbytes(b, name,
					D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, index),
					(uchar *) erate, D11_RATE_MEM_HW_BLOCK_SIZE);
			}
			if ((flags & RTLKMEM_DUMP_FLAG_PARSED) != 0) {
				wlc_ratelinkmem_rate_entry_dump(wlc, rate_entry, b);
			}
		}
	}

	if ((flags & RTLKMEM_DUMP_FLAG_RATEONLY) == 0) {
		bp_addr = D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, index);
		for  (i = 0; i < link_size / 4; i++) {
			si_backplane_access(wlc->pub->sih, bp_addr, 4, elink + i, TRUE);
			bp_addr += 4;
		}
		snprintf(name, sizeof(name), "LinkMem Block: %d Size:%dB", (int) index,
			(int) link_size);
		if ((flags & RTLKMEM_DUMP_FLAG_RAWDUMP) != 0) {
			bcm_bprbytes(b, name,
			             D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, index),
			             (uchar *) elink, link_size);
		}
		if ((flags & RTLKMEM_DUMP_FLAG_PARSED) != 0) {
			wlc_ratelinkmem_link_entry_dump(wlc, link_entry, b);
		}
	}
}
#endif // endif

#if defined(BCMDBG) || defined(WLTEST) || defined(DUMP_TXBF)
void
wlc_ratelinkmem_raw_lmem_read(wlc_info_t *wlc, uint16 index, d11linkmem_entry_t *link_entry,
	int size)
{
	int  i;
	uint32 bp_addr;
	uint *elink = (uint *) link_entry;
	int link_size = WLC_RLM_LINK_ENTRY_SIZE_FULL;

	if (!wlc->clk) {
		return;
	}

	if (size != AUTO) {
		link_size = ROUNDUP(size, 4);
	}
	bp_addr = D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, index);
	for  (i = 0; i < link_size / 4; i++) {
		si_backplane_access(wlc->pub->sih, bp_addr, 4, elink + i, TRUE);
		bp_addr += 4;
	}
}
#endif // endif

/** dump a single link_entry */
void
wlc_ratelinkmem_link_entry_dump(wlc_info_t *wlc, d11linkmem_entry_t *link_entry,
	struct bcmstrbuf *b)
{
	int k;

	ASSERT(link_entry != NULL);

	bcm_bprintf(b, "\tBssIdx: 0x%x, BssColor: 0x%x, StaID: 0x%x, FragTx: 0x%x\n",
		link_entry->BssIdx, link_entry->BssColor_valid, ltoh16(link_entry->StaID_IsAP),
		ltoh16(link_entry->fragTx_minru));

	bcm_bprintf(b, "\tEncryptAlgo: 0x%x, KeyIdx: 0x%x, TkipPhase1KeyIdx: 0x%x\n",
		ltoh16(link_entry->EncryptInfo) & D11_REV128_ENCRYPTALGO_MASK,
		((ltoh16(link_entry->EncryptInfo) & D11_REV128_KEYIDX_MASK)
		>> D11_REV128_KEYIDX_SHIFT),
		((ltoh16(link_entry->EncryptInfo) & D11_REV128_TKIPPHASE1KEYIDX_MASK)
		>> D11_REV128_TKIPPHASE1KEYIDX_SHIFT));

	bcm_bprintf(b, "\tOMI: 0x%x, RtsDurThresh: 0x%x\n",
		ltoh16(link_entry->OMI), ltoh16(link_entry->RtsDurThresh));

	bcm_bprintf(b, "\tMaxAmpduLenExp: 0x%x, AmpduMpduAll: 0x%x\n",
		ltoh16(link_entry->ampdu_info) & D11_REV128_MAXRXFACTOR_MASK,
		(ltoh16(link_entry->ampdu_info) & D11_REV128_AMPDUMPDUALL_MASK)
		>> D11_REV128_AMPDUMPDUALL_SHIFT);

	for (k = 0; k < D11_LINKENTRY_NUM_TIDS; k++) {
		bcm_bprintf(b, "\t\tTID: %d | BAWin: 0x%x, ampdu_mpdu: 0x%x\n", k,
			link_entry->ampdu_ptid[k].BAWin, link_entry->ampdu_ptid[k].ampdu_mpdu);
	}
	bcm_bprintf(b, "\tMultiTIDAggBitmap: 0x%x, MultiTIDAggNum: 0x%x\n",
		link_entry->MultiTIDAggBitmap, link_entry->MultiTIDAggNum);

	bcm_bprintf(b, "\tAmpMaxDurIdx: 0x%x\n", ltoh16(link_entry->AmpMaxDurIdx));

	bcm_bprintf(b, "\tPPET0: 0x%x, AmpMinDur: 0x%x\n",
		ltoh32(link_entry->PPET_AmpMinDur) & D11_REV128_PPETX_MASK,
		((ltoh32(link_entry->PPET_AmpMinDur) & D11_REV128_AMPMINDUR_MASK)
		>> D11_REV128_AMPMINDUR_SHIFT));

#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_link_entry_dump(link_entry, b);
	}
#endif /* WL_BEAMFORMING */

	if (HE_DLMU_ENAB(wlc->pub)) {
		wlc_musched_link_entry_dump(link_entry, b);
	}
}

/** dump the SCB cubby content */
static void
wlc_ratelinkmem_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_ratelinkmem_info_t *rlmi = ctx;
	wlc_info_t *wlc = rlmi->wlc;
	scb_ratelinkmem_info_t *scbh;
	int k;
	char eabuf[ETHER_ADDR_STR_LEN];
	wlc_bsscfg_t* cfg;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	cfg = SCB_BSSCFG(scb);
	if (cfg && (scb == WLC_RLM_BSSCFG_LINK_SCB(wlc, cfg))) {
		bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf);
	} else {
		bcm_ether_ntoa(&scb->ea, eabuf);
	}

	bcm_bprintf(b, "RateLinkMem SCB info (%p, mac: %s, index: %d, valid: %d): \n", scb,
		eabuf, scbh->rate_link_index, scbh->cubby_valid);
	bcm_bprintf(b, "rate_state: %d, rmem_nupd/curptr/txptr/maxdepth: %d %d %d %d\n",
		scbh->rate_state, scbh->rmem_nupd, scbh->rmem_curptr, scbh->rmem_txptr,
		scbh->rmem_maxdepth);
	if (scbh->rate_state != RATE_ENTRY_STATE_UNINITED) {
		for (k = 0; k < MAX_RATE_ENTRIES; k++) {
			bcm_bprintf(b, "\trate_store[%d]: ", k);
			wlc_ratelinkmem_rate_store_dump(wlc, &scbh->rate_store[k], b);
		}
#if defined(BCMDBG)
		k = scbh->rate_link_index;
		if (k != D11_RATE_LINK_MEM_IDX_INVALID) {
			ASSERT(k < D11_MAX_RATELINK_ENTRIES);
			bcm_bprintf(b, "In HW: ");
			wlc_ratelinkmem_dump_raw_bytes(wlc, k, b, RTLKMEM_DUMP_FLAG_RATEONLY |
				RTLKMEM_DUMP_FLAG_PARSED);
		}
#endif // endif
	}
	bcm_bprintf(b, "link_state: %d, cur_link_count: %d\n", scbh->link_state,
		scbh->cur_link_count);
	if (scbh->link_state != LINK_ENTRY_STATE_UNINITED)
		wlc_ratelinkmem_link_entry_dump(wlc, &scbh->link_entry, b);
}

#if defined(BCMDBG) || defined(DUMP_RATELINKMEM)
#define RLM_DUMP_ARGV_MAX		64
static int
wlc_ratelinkmem_dump_parse_args(wlc_info_t *wlc, uint8 bmp[], bool *dump_all, uint8* flags)
{
	int err = BCME_OK;
	char *args = wlc->dump_args;
	char *p, **argv = NULL;
	uint argc = 0;
	uint val, strt, end, i;

	if (args == NULL || bmp == NULL || dump_all == NULL || flags == NULL) {
		err = BCME_BADARG;
		goto exit;
	}

	/* allocate argv */
	if ((argv = MALLOC(wlc->osh, sizeof(*argv) * RLM_DUMP_ARGV_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate the argv buffer\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	/* get each token */
	p = bcmstrtok(&args, " ", 0);
	while (p && argc < RLM_DUMP_ARGV_MAX-1) {
		argv[argc++] = p;
		p = bcmstrtok(&args, " ", 0);
	}
	argv[argc] = NULL;

	/* parse argv */
	argc = 0;
	while ((p = argv[argc++]) != NULL) {
		if (*p == '-') {
			switch (*++p) {
				case 'h':
					err = BCME_BADARG; /* invoke help */
					goto exit;
				case 'r':
					*flags |= RTLKMEM_DUMP_FLAG_RAWDUMP;
					break;
				case 'L':
					*flags |= RTLKMEM_DUMP_FLAG_LINKONLY;
					break;
				case 'R':
					*flags |= RTLKMEM_DUMP_FLAG_RATEONLY;
					break;
				case 'P':
					*flags |= RTLKMEM_DUMP_FLAG_PARSED;
					break;
				case 'p':
					*flags |= RTLKMEM_DUMP_FLAG_PINGPONG;
					break;
				case 'i':
					while (*++p != '\0') {
						val = bcm_strtoul(p, &p, 0);
						if (val >= D11_MAX_RATELINK_ENTRIES) {
							err = BCME_BADARG;
							goto exit;
						}
						setbit(bmp, val);
					}
					*dump_all = FALSE; /* selected dump(s) only */
					break;
				case 'I':
					/* -Ix,y inclusively prints indexes x through y */
					if (*(p+1) == '\0' || *(p+2) == '\0') {
						err = BCME_BADARG;
						goto exit;
					}
					strt = bcm_strtoul(++p, &p, 0);
					end = bcm_strtoul(++p, &p, 0);
					if (end >= strt && end < D11_MAX_RATELINK_ENTRIES) {
						for (i = strt; i <= end; i++) {
							setbit(bmp, i);
						}
					} else {
						err = BCME_BADARG;
						goto exit;
					}
					*dump_all = FALSE; /* selected dump(s) only */
					break;
				default:
					err = BCME_BADARG;
					goto exit;
			}
		} else {
			err = BCME_BADARG;
			goto exit;
		}
	}

exit:
	if (argv) {
		MFREE(wlc->osh, argv, sizeof(*argv) * RLM_DUMP_ARGV_MAX);
	}

	return err;
}

/** dump ratelinkmem info */
static int
wlc_ratelinkmem_module_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_ratelinkmem_info_t *rlmi = ctx;
	int i;
	uint8 entry_bitmap[D11_MAX_RATELINK_ENTRIES/NBBY + 1] = { 0 };
	bool dump_all = TRUE; /* default to dump all */
	wlc_info_t *wlc = rlmi->wlc;
	uint8 flags = 0;

	bcm_bprintf(b, "RateLinkMem Module DUMP\n");

	/* Parse args if needed */
	if (rlmi->wlc->dump_args) {
		int err = wlc_ratelinkmem_dump_parse_args(rlmi->wlc, entry_bitmap, &dump_all,
			&flags);
		if (err != BCME_OK) {
			bcm_bprintf(b, "RateLinkMem dump optional params (space separated): ");
			bcm_bprintf(b, "[-h -r -P -R -L -i<num1,num2,...> -I<start,finish>]\n");
			bcm_bprintf(b, "-h\t(this) help output\n");
			bcm_bprintf(b, "-i0,63\tSelect indices 0 and 63 (comma separated)\n");
			bcm_bprintf(b, "-I0,63\tSelect indices 0 through 63 (comma separated)\n");
			bcm_bprintf(b, "-r\tDump raw HW content only\n");
			bcm_bprintf(b, "-P\tDump parsed version of HW content\n");
			bcm_bprintf(b, "-R\tDump rateentry only (i.e. don't dump link entry)\n");
			bcm_bprintf(b, "-L\tDump linkentry only (i.e. don't dump rate entry)\n");
			bcm_bprintf(b, "-p\tDump both ping and pong ratemem entry (For macdbg only,"
					" avoid using runtime)\n");
			return BCME_OK;
		}
	}

	if (flags & RTLKMEM_DUMP_FLAG_RAWDUMP && wlc->pub->up) {
		wlc_suspend_mac_and_wait(wlc);
	}

	for (i = 0; i < D11_MAX_RATELINK_ENTRIES; i++) {
		if (dump_all || isset(entry_bitmap, i)) {
			if (flags & RTLKMEM_DUMP_FLAG_RAWDUMP) {
				if (isset(entry_bitmap, i) || (rlmi->scb_index[i] != NULL)) {
					wlc_ratelinkmem_dump_raw_bytes(wlc, i, b, flags);
				}
			} else if (rlmi->scb_index[i] != NULL) {
				wlc_ratelinkmem_scb_dump(ctx, rlmi->scb_index[i], b);
			} else if (!dump_all) {
				/* only print unavailability for indices explicitly requested */
				bcm_bprintf(b, "RateLinkMem index: %d not valid\n", i);
			}
		}
	}

	if (flags & RTLKMEM_DUMP_FLAG_PINGPONG && wlc->clk) {
		W_REG(wlc->osh, D11_PSM_RATEMEM_DBG(wlc), (uint16)0); // reset
	}

	if (flags & RTLKMEM_DUMP_FLAG_RAWDUMP && wlc->pub->up) {
		wlc_enable_mac(wlc);
	}

	return BCME_OK;
}
#endif // endif

/** Restore after Down/Reinit/BigHammer */
static int
wlc_ratelinkmem_up(void *ctx)
{
	wlc_ratelinkmem_info_t *rlmi = ctx;
	wlc_info_t *wlc = rlmi->wlc;
	int i;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	RL_RMEM1(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	wlc_ratelinkmem_reset_special_rate_scb(rlmi);

	for (i = 0; i < D11_MAX_RATELINK_ENTRIES; i++) {
		scb_t *scb = rlmi->scb_index[i];
		scb_ratelinkmem_info_t *scbh;

		if (!scb) {
			continue;
		}

		scbh = SCB_RLM_CUBBY(rlmi, scb);
		ASSERT(scbh != NULL);
		/* key management module cares about reinit only if there is valid key,
		 * otherwise it is upto other module who triggered AMT entry creation to
		 * reset it upon wl up/reinit
		 */

		if (scbh->link_state == LINK_ENTRY_STATE_ACTIVE) {
			wlc_keymgmt_restore_scb_amt_entry(wlc->keymgmt, scb);
		}
	}

	return BCME_OK;
}

/** Initialize Indices and lookup table. Return TRUE if successfull */
static bool
wlc_ratelinkmem_init_indices(wlc_info_t *wlc, scb_t *scb, scb_ratelinkmem_info_t *scbh,
	int amt_index)
{
	wlc_rlm_event_t rlm_evdata;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	ASSERT(scb != NULL);
	ASSERT(scbh != NULL);
	ASSERT(scbh->rate_state == RATE_ENTRY_STATE_UNINITED);
	ASSERT(scbh->link_state == LINK_ENTRY_STATE_UNINITED);
	ASSERT(scbh->rate_link_index == D11_RATE_LINK_MEM_IDX_INVALID);

	if (amt_index == D11_RATE_LINK_MEM_IDX_INVALID) {
		/* use SCB AMT idx; keymgmt currently owns amt A2/TA amt allocations */
		amt_index = wlc_keymgmt_get_scb_amt_idx(wlc->keymgmt, scb);
	}
	if (amt_index < 0) {
		WL_INFORM(("wl%d: %s: Invalid AMT index for SCB: %d, %p "MACF" \n",
			wlc->pub->unit, __FUNCTION__, amt_index, scb, ETHER_TO_MACF(scb->ea)));
		return FALSE;
	}
	ASSERT(amt_index < D11_MAX_RATELINK_ENTRIES);
	if (wlc->rlmi->scb_index[amt_index] != NULL) {
		scb_t *scb_other = wlc->rlmi->scb_index[amt_index];
		scb_ratelinkmem_info_t *scbh_other = SCB_RLM_CUBBY(wlc->rlmi, scb_other);
		WL_ERROR(("wl%d:%s:Unexpected SCB:%p(valid:%d;index:%d) in table idx:%d\n",
			wlc->pub->unit, __FUNCTION__, scb_other, scbh_other->cubby_valid,
			scbh_other->rate_link_index, amt_index));
		BCM_REFERENCE(scbh_other);
		wlc_ratelinkmem_scb_amt_release(wlc, scb_other, amt_index);
		ASSERT(wlc->rlmi->scb_index[amt_index] == NULL);
	}

	scbh->rate_link_index = amt_index;
	wlc->rlmi->scb_index[amt_index] = scb;

	if (wlc->clk) {
		wlc_suspend_mac_and_wait(wlc);
		W_REG(wlc->osh, D11_PSM_RATEMEM_CTL(wlc),
		      (uint16) ((1 << C_RTMCTL_RST_NBIT) | (1 << C_RTMCTL_UNLOCK_NBIT) |
				scbh->rate_link_index));
		wlc_enable_mac(wlc);
	}

#ifdef DONGLEBUILD
	memset(&rlm_evdata, 0, sizeof(rlm_evdata));
	rlm_evdata.version = WLC_E_MACDBG_RLM_VERSION;
	rlm_evdata.length = sizeof(rlm_evdata) - OFFSETOF(wlc_rlm_event_t, entry);
	rlm_evdata.entry = amt_index;

	wlc_bss_mac_event(wlc, scb->bsscfg, WLC_E_MACDBG, &scb->ea, 0,
		WLC_E_MACDBG_RATELINKMEM, 0, &rlm_evdata, sizeof(rlm_evdata));
#else
	BCM_REFERENCE(rlm_evdata);
#endif /* DONGLEBUILD */

	RL_RMEM(("wl%d: %s scb %p index %d rate_state= %d\n",
		wlc->pub->unit, __FUNCTION__, scb, amt_index, scbh->rate_state));

	return TRUE;
}

/**
 * Global Module functions
 */
wlc_ratelinkmem_info_t *
BCMATTACHFN(wlc_ratelinkmem_attach)(wlc_info_t *wlc)
{
	wlc_ratelinkmem_info_t *rlmi;
	bme_set_t bme_set;
	uint32 hi_src = 0;
	uint32 hi_dst = 0;

	ASSERT(wlc != NULL);

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	/* Allocate info structure */
	if ((rlmi = (wlc_ratelinkmem_info_t *)MALLOCZ(wlc->osh,
		sizeof(*rlmi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	rlmi->wlc = wlc;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		WL_ERROR(("wl%d: %s: Not enabling RateLinkMem support\n",
			wlc->pub->unit, __FUNCTION__));
		return rlmi;
	}

	/* reserve cubby in the SCB container for per-SCB storage */
	if ((rlmi->scbh = wlc_scb_cubby_reserve(wlc, sizeof(scb_ratelinkmem_info_t),
		wlc_ratelinkmem_scb_init, wlc_ratelinkmem_scb_deinit,
		wlc_ratelinkmem_scb_dump, rlmi)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* no need to memset other fields like null_link_entry due to MALLOCZ */

	/* register module */
	if (wlc_module_register(wlc->pub, NULL /* iovars */, "ratelinkmem", rlmi,
		NULL /* doiovar */, NULL /* watchdog */, NULL /* up */, NULL /* down */)) {
		WL_ERROR(("wl%d: ratelinkmem module register failed\n", wlc->pub->unit));
		goto fail;
	}

	STATIC_ASSERT(sizeof(d11ratemem_rev128_entry_t) <= D11_RATE_MEM_HW_BLOCK_SIZE);
	STATIC_ASSERT(sizeof(d11linkmem_entry_t) <= D11_LINK_MEM_HW_BLOCK_SIZE);
	STATIC_ASSERT(sizeof(d11ratemem_rev128_entry_t) == WLC_RLM_RATE_ENTRY_SIZE);
	STATIC_ASSERT(sizeof(d11linkmem_entry_t) == WLC_RLM_LINK_ENTRY_SIZE_TXBF);
	STATIC_ASSERT((WLC_RLM_RATE_ENTRY_SIZE % 4) == 0); /* ensure DWORD aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_SW % 4) == 0); /* ensure DWORD aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_TXBF % 4) == 0); /* ensure DWORD aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_FULL % 4) == 0); /* ensure DWORD aligned */
	STATIC_ASSERT((WLC_RLM_RATE_ENTRY_SIZE % 8) == 0); /* ensure 8-byte aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_SW % 8) == 0); /* ensure 8-byte aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_TXBF % 8) == 0); /* ensure 8-byte aligned */
	STATIC_ASSERT((WLC_RLM_LINK_ENTRY_SIZE_FULL % 8) == 0); /* ensure 8-byte aligned */
	STATIC_ASSERT(WLC_RLM_LINK_ENTRY_SIZE_FULL == D11_LINK_MEM_HW_BLOCK_SIZE);
	STATIC_ASSERT(D11_LINKENTRY_NUM_TIDS >= AMPDU_MAX_SCB_TID);
	ASSERT(D11_MAX_RATELINK_ENTRIES >= AMT_SIZE(wlc->pub->corerev));

#if defined(BCMDBG) || defined(DUMP_RATELINKMEM)
	wlc_dump_register(wlc->pub, "ratelinkmem", wlc_ratelinkmem_module_dump, (void *)rlmi);
#endif // endif

	bme_set.idx = 0; /* request BME engine 0 */
#ifdef DONGLEBUILD
	rlmi->bme_key = bme_register_user(wlc->osh, BME_USR_RLM,
		BME_SEL_IDX, bme_set, BME_MEM_DNGL, BME_MEM_CHIP, hi_src, hi_dst);
#else
	rlmi->bme_key = bme_register_user(wlc->osh, BME_USR_RLM,
		BME_SEL_IDX, bme_set,
		BUSTYPE(wlc->pub->sih->bustype) == SI_BUS ? BME_MEM_UBUS : BME_MEM_PCIE,
		BME_MEM_CHIP, hi_src, hi_dst);
#endif /* DONGLEBUILD */
	if (rlmi->bme_key < 0) {
		WL_ERROR(("wl%d: %s: Unable to register BME for USR RLM\n",
			wlc->pub->unit, __FUNCTION__));
		ASSERT(0);
		return rlmi;
	}

	wlc->pub->_ratelinkmem = TRUE;

	return rlmi;

fail:
	MODULE_DETACH(rlmi, wlc_ratelinkmem_detach);
	return NULL;
}
void
BCMATTACHFN(wlc_ratelinkmem_detach)(wlc_ratelinkmem_info_t *rlmi)
{
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	if (rlmi == NULL) {
		return;
	}

	WL_TRACE(("wl%d: %s\n", rlmi->wlc->pub->unit, __FUNCTION__));

	rlmi->wlc->pub->_ratelinkmem = FALSE;

	if (!PHYSADDRISZERO(rlmi->rate_dma_pa)) {

#ifndef BCM_SECURE_DMA
		DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz, DMA_TX,
			NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz,
			DMA_TX, NULL, NULL, &di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
	}

	if (rlmi->bme_key != 0) {
		bme_unregister_user(rlmi->wlc->osh, BME_USR_RLM);
		rlmi->bme_key = 0;
	}

	wlc_module_unregister(rlmi->wlc->pub, "ratelinkmem", rlmi);
	MFREE(rlmi->wlc->osh, rlmi, sizeof(*rlmi));
}

void
wlc_ratelinkmem_hw_init(wlc_info_t *wlc)
{
	const int link_size = WLC_RLM_LINK_ENTRY_SIZE_FULL;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));
	ASSERT(wlc->rlmi->bme_key != 0);

	/* Inform HW of entry sizes in DWORD units */
	W_REG(wlc->osh, D11_PSM_RATEXFER_SIZE(wlc), (uint16)(WLC_RLM_RATE_ENTRY_SIZE / 4));
	W_REG(wlc->osh, D11_PSM_LINKXFER_SIZE(wlc), (uint16)(link_size / 4));
	wlc_ratelinkmem_up(wlc->rlmi);
}

/** return index used by this SCB, return D11_RATE_LINK_MEM_IDX_INVALID in case of error */
static uint16
wlc_ratelinkmem_get_scb_amt_index(wlc_info_t *wlc, scb_t *scb)
{
	int amt_index;
	scb_ratelinkmem_info_t *scbh;

	if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		WL_INFORM(("wl%d: %s: SCB is or will be deleted: %p\n",
			wlc->pub->unit, __FUNCTION__, scb));
		return D11_RATE_LINK_MEM_IDX_INVALID;
	}

	scbh = SCB_RLM_CUBBY(wlc->rlmi, scb);
	ASSERT(scbh != NULL);
	if (scbh->cubby_valid == FALSE) {
		WL_ERROR(("wl%d: %s: SCB cubby not yet initialized: %p\n",
			wlc->pub->unit, __FUNCTION__, scb));
		return D11_RATE_LINK_MEM_IDX_INVALID;
	}

	/* use AMT idx; keymgmt currently owns amt A2/TA amt allocations */
	amt_index = wlc_keymgmt_get_scb_amt_idx(wlc->keymgmt, scb);
	if (amt_index < 0) {
		WL_INFORM(("wl%d: %s: Invalid AMT index for SCB: %d, %p\n",
			wlc->pub->unit, __FUNCTION__, amt_index, scb));
		return D11_RATE_LINK_MEM_IDX_INVALID;
	}
	ASSERT(amt_index < D11_MAX_RATELINK_ENTRIES);

	ASSERT((scbh->rate_link_index == D11_RATE_LINK_MEM_IDX_INVALID) ||
		(scbh->rate_link_index == amt_index));
	BCM_REFERENCE(scbh);

	return (uint16) amt_index;
}

/**
 * Global rate memory related functions
 */
/** return index used by this SCB, return D11_RATE_LINK_MEM_IDX_INVALID in case of error */
uint16
wlc_ratelinkmem_get_scb_rate_index(wlc_info_t *wlc, scb_t *scb)
{

	if (SCB_PROXD(scb)) {
		return WLC_RLM_FTM_RATE_IDX;
	} else if ((SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) || ETHER_ISNULLADDR(&scb->ea))) {
		/* map all non-SCB traffic to single, 'global' rate entry */
		return WLC_RLM_SPECIAL_RATE_IDX;
	} else {
		return wlc_ratelinkmem_get_scb_amt_index(wlc, scb);
	}
}

/** return read-only pointer to rate entry
 *  when newrate is TRUE, return latest rate as configured (from curptr; use for TXH creation)
 *  when newrate is FALSE, return latest rate that was acknowledged (from txptr; use in TXS path)
 */
const wlc_rlm_rate_store_t *
wlc_ratelinkmem_retrieve_cur_rate_store(wlc_info_t *wlc, uint16 index, bool newrate)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_t* scb;
	scb_ratelinkmem_info_t *scbh;

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		return NULL;
	}

	if (index >= D11_MAX_RATELINK_ENTRIES) {
		WL_ERROR(("wl%d: %s: Invalid index: %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		ASSERT(0);
		return NULL;
	}

	scb = rlmi->scb_index[index];
	if (scb == NULL) {
		WL_TRACE(("wl%d: %s: No SCB for rate index %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		return NULL;
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);
	ASSERT(scbh->rate_link_index == index);
	ASSERT(scbh->cubby_valid == TRUE);

	if (scbh->rate_state == RATE_ENTRY_STATE_UNINITED) {
		return NULL;
	}

	RL_RMEM1(("wl%d: %s index %d ret entry[%d] rmem_nupd/curptr/txptr/maxdepth %d %d %d %d\n",
		wlc->pub->unit, __FUNCTION__, index,
		scbh->rmem_txptr,
		scbh->rmem_nupd, scbh->rmem_curptr, scbh->rmem_txptr, scbh->rmem_maxdepth));

	if (newrate) {
		return &scbh->rate_store[scbh->rmem_curptr];
	} else {
		return &scbh->rate_store[scbh->rmem_txptr];
	}
}

/** External API to update rucfg in ratemem
 */
int BCMFASTPATH
wlc_ratelinkmem_write_rucfg(wlc_info_t *wlc, uint8 *buf, uint sz, uint16 rmem_idx)
{
	int ret;
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	if (sz != WLC_RLM_RATE_ENTRY_SIZE) {
		W_REG(wlc->osh, D11_PSM_RATEXFER_SIZE(wlc), (uint16)(sz/ 4));
	}

	/* ensure previous transfer (if any) completed before we overwrite memory */
	if (!PHYSADDRISZERO(rlmi->rate_dma_pa)) {
		/* spin until previous transfer completed */
		bme_sync_eng(wlc->osh, rlmi->bme_eng_idx);
#ifndef BCM_SECURE_DMA
		DMA_UNMAP(wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz, DMA_TX, NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz,
			DMA_TX, NULL, NULL, &di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
	}

	ret = wlc_ratelinkmem_upd_rmem(rlmi, buf, sz, rmem_idx);

#ifdef BCM_SECURE_DMA
	if (!PHYSADDRISZERO(rlmi->rate_dma_pa)) {
		bme_sync_eng(wlc->osh, rlmi->bme_eng_idx);
		SECURE_DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz,
			DMA_TX, NULL, NULL, &di->sec_cma_info_tx, 0);
		PHYSADDRHISET(rlmi->rate_dma_pa, 0);
		PHYSADDRLOSET(rlmi->rate_dma_pa, 0);
	}
#endif /* BCM_SECURE_DMA */

	ret = wlc_ratelinkmem_upd_rmem(rlmi, buf, sz, rmem_idx);

	/* ensure transfer is completed before returning to caller where buffer will be free'ed */
	if (!PHYSADDRISZERO(rlmi->rate_dma_pa)) {
		bme_sync_eng(wlc->osh, rlmi->bme_eng_idx);
#ifndef BCM_SECURE_DMA
		DMA_UNMAP(wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz, DMA_TX, NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz,
		DMA_TX, NULL, NULL, &di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
		PHYSADDRHISET(rlmi->rate_dma_pa, 0);
		PHYSADDRLOSET(rlmi->rate_dma_pa, 0);
	}

	if (sz != WLC_RLM_RATE_ENTRY_SIZE) {
		W_REG(wlc->osh, D11_PSM_RATEXFER_SIZE(wlc), (uint16)(WLC_RLM_RATE_ENTRY_SIZE / 4));
	}
	return ret;
}

/** update HW ratemem memory through mem2mem xfer
 *  src : buf
 *  sz  : size in byte
 *  dest: block <rmem_idx>. <rmem_idx> is hw index.
 */
static int BCMFASTPATH
wlc_ratelinkmem_upd_rmem(wlc_ratelinkmem_info_t *rlmi, uint8 *buf, uint sz, uint16 rmem_idx)
{
	wlc_info_t *wlc = rlmi->wlc;
#ifdef BCMDMA64OSL
		unsigned long long paddr;
#endif /* BCMDMA64OSL */
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	BCM_REFERENCE(wlc);

	ASSERT(rlmi && buf);
	ASSERT(sz <= D11_RATE_MEM_HW_BLOCK_SIZE);
	ASSERT(rmem_idx < D11_MAX_RATELINK_ENTRIES);

	/* Use DMA to transfer the entry to the D11 Rate Memory */
#ifndef BCM_SECURE_DMA
	rlmi->rate_dma_pa = DMA_MAP(wlc->osh, buf, sz, DMA_TX, NULL, NULL);
#else
	rlmi->rate_dma_pa = SECURE_DMA_MAP(wlc->osh, buf, sz, DMA_TX, NULL, NULL,
		&di->sec_cma_info_tx, 0, SECDMA_TXBUF_POST);
#endif /* BCM_SECURE_DMA */
	rlmi->rate_dma_sz = sz;
#ifdef BCMDMA64OSL
	PHYSADDRTOULONG(rlmi->rate_dma_pa, paddr);
	rlmi->bme_eng_idx = bme_copy64(wlc->osh, rlmi->bme_key,
		(uint64)paddr,
		(uint64)D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, rmem_idx),
		sz);
#else
	rlmi->bme_eng_idx = bme_copy(wlc->osh, rlmi->bme_key,
		(const uint8*) rlmi->rate_dma_pa,
		(uint8 *)(uintptr)D11_RATE_MEM_ENTRY_ADDR_BME_VIEW(wlc, rmem_idx),
		sz);
#endif /* BCMDMA64OSL */
	return BCME_OK;
}

/** Called by rate selection to commit new rate set into ratemem
 *  Return okey if succeeds. Should be always successful.
 *  Input:
 *    if <rs> is NULL, then it's for static rate;
 *    else it's called for auto rate
 *    <flags> contains the info to be updated into ratemem structure header
 *
 *    In case of static ratemem update, retrieve rspec from various rspec_overrides
 *    In case of auto rate, rate sel has computed rate set and passed in rs.
 *  Steps:
 *  1) rmem_curptr == rmem_txptr should be true if they have been used
 *  2) increment remem_nupd,
 *  3) fill up rate_entry[rmem_txptr.b0].blocks with cur_rate + scb info
 *  4) fill up rate_entry[rmem_curptr.b0].flags with flags
 *  5) transfer rate_entry to rtmem in mac
 */
int BCMFASTPATH
wlc_ratelinkmem_update_rate_entry(wlc_info_t *wlc, scb_t *scb,
	void *rs, uint16 flags)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_ratelinkmem_info_t *scbh;
	d11ratemem_rev128_entry_t *rate_entry;
	wlc_rlm_rate_store_t *rstore;
	ratesel_txparams_t *ratesel_rates = NULL;
	uint depth;
	int index = D11_RATE_LINK_MEM_IDX_INVALID;
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		return BCME_ERROR;
	}
	if (!wlc->pub->up) {
		return BCME_NOTUP;
	}
	ASSERT(scb);

	if (rs) {
		ratesel_rates = (ratesel_txparams_t*)rs;
	} else {
		/* if rs is NULL, then it's not passed from auto rate and
		 * it must be using the static ratemem
		 */
		scb = WLC_RLM_SPECIAL_RATE_SCB(wlc);
		index = WLC_RLM_SPECIAL_RATE_IDX;
	}

	if (SCB_PROXD(scb)) {
		index = WLC_RLM_FTM_RATE_IDX;
	} else if ((SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) || ETHER_ISNULLADDR(&scb->ea))) {
		/* map all non-SCB traffic to single, 'global' rate entry */
		ASSERT(scb == WLC_RLM_SPECIAL_RATE_SCB(wlc));
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	WL_TRACE(("wl%d: %s %p, %p rate_state %d link_state %d flags 0x%x\n",
		wlc->pub->unit, __FUNCTION__, scb, scbh,
		scbh->rate_state, scbh->link_state, flags));

	if (scbh->cubby_valid == FALSE) {
		/* this may happen while bringing down an SCB, avoid trying to reinit */
		WL_INFORM(("wl%d: %s, %p invalid; ignore request\n", wlc->pub->unit, __FUNCTION__,
			scb));
		return BCME_NOTFOUND;
	}

	if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating ratemem that"
			" is or will be deleted\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_ERROR;
	}

	if (BSSCFG_AP(scb->bsscfg) &&
		SCAN_IN_PROGRESS(wlc->scan) &&
		(scb != WLC_RLM_SPECIAL_RATE_SCB(wlc))) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating ratemem"
			" during scan\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_ERROR;
	}

	if (rlmi->block_in_release) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating ratemem"
			" while in release\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_NOTREADY;
	}

	if ((scbh->rate_state == RATE_ENTRY_STATE_UNINITED) &&
		(scbh->link_state == LINK_ENTRY_STATE_UNINITED)) {

		if (wlc_ratelinkmem_init_indices(wlc, scb, scbh, index) == FALSE) {
			return BCME_NOTFOUND;
		}
	}
	scbh->rate_state = RATE_ENTRY_STATE_ACTIVE;

	/* check if entry in rate store available */
	if (RATE_STORE_IDX(scbh->rmem_curptr + 1) == scbh->rmem_txptr) {
		if (scb != WLC_RLM_SPECIAL_RATE_SCB(wlc)) {
			WL_ERROR(("wl%d: %s index %d WRAP! rmem_nupd/curptr/txptr/maxdepth"
				" %d %d %d %d\n", wlc->pub->unit, __FUNCTION__,
				scbh->rate_link_index, scbh->rmem_nupd, scbh->rmem_curptr,
				scbh->rmem_txptr, scbh->rmem_maxdepth));
		} else {
			/* in case of Special SCB, this may happen often as there may be little
			 * 'special' traffic
			 */
			RL_RMEM1(("wl%d: %s index %d WRAP? rmem_nupd/curptr/txptr/maxdepth"
				" %d %d %d %d\n", wlc->pub->unit, __FUNCTION__,
				scbh->rate_link_index, scbh->rmem_nupd, scbh->rmem_curptr,
				scbh->rmem_txptr, scbh->rmem_maxdepth));
		}
		/* Push forward TX ptr. If no traffic outstanding, no harm. If traffic
		 * outstanding we may cause some error messages in upd_txstatus() and
		 * rspec statistics may be off for a while, but we'll resync eventually
		 */
		scbh->rmem_txptr = RATE_STORE_IDX(scbh->rmem_txptr + 1);
	}

	/* update ptr before fill up rate_entry */
	scbh->rmem_curptr = RATE_STORE_IDX(scbh->rmem_nupd);
	/* curptr should never be equal to txptr, except for first ever update */
	ASSERT((scbh->rmem_curptr != scbh->rmem_txptr) || !scbh->rmem_nupd);
	/* increase number of updates */
	scbh->rmem_nupd++;

	/* update maxdepth if needed */
	if (scbh->rmem_curptr >= scbh->rmem_txptr) {
		depth = scbh->rmem_curptr - scbh->rmem_txptr;
	} else {
		depth = scbh->rmem_curptr + MAX_RATE_ENTRIES - scbh->rmem_txptr;
	}
	if (depth > scbh->rmem_maxdepth)
		scbh->rmem_maxdepth = depth;

	rstore = &scbh->rate_store[scbh->rmem_curptr];

	/* ensure previous transfer (if any) completed before we overwrite memory */
	if (!PHYSADDRISZERO(rlmi->rate_dma_pa)) {
		/* spin until previous transfer completed */
		bme_sync_eng(wlc->osh, rlmi->bme_eng_idx);
#ifndef BCM_SECURE_DMA
		DMA_UNMAP(wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz, DMA_TX,
			NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, rlmi->rate_dma_pa, rlmi->rate_dma_sz, DMA_TX,
		NULL, NULL, &di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
		/* could clear rate_dma_pa here, but there is no way the code below can exit, so
		 * commit_rmem() is garanteed to be called which will always overwrite rate_dma_pa
		 * so will skip setting it here
		 */
	}
	rate_entry = &rlmi->rate_entry;

	/* fill rate_entry contents, also fill rate_store; except flags */
	wlc_tx_fill_rate_entry(wlc, scb, rate_entry, rstore, ratesel_rates);

	/* record nupd in rate_entry->flags, keep ratesel fields as is */
	rate_entry->flags = flags |
		((scbh->rmem_nupd << D11_REV128_RATE_NUPD_SHIFT) & D11_REV128_RATE_NUPD_MASK);
	/* keep local storage as well */
	rstore->flags = rate_entry->flags;

	wlc_macdbg_dtrace_log_txr(wlc->macdbg, scb, scbh->rate_link_index, rate_entry);

	/* write rate_entry info into hw rmem block */
	ASSERT(scbh->rate_link_index < D11_MAX_RATELINK_ENTRIES);
	wlc_ratelinkmem_upd_rmem(rlmi, (uint8 *)rate_entry, WLC_RLM_RATE_ENTRY_SIZE,
		scbh->rate_link_index);

	RL_RMEM1(("wl%d: %s index %d state %d flags 0x%02x "
		"rmem_nupd/curptr/txptr/maxdepth %d %d %d %d\n",
		wlc->pub->unit, __FUNCTION__,
		scbh->rate_link_index, scbh->rate_state,
		rate_entry->flags, scbh->rmem_nupd, scbh->rmem_curptr, scbh->rmem_txptr,
		scbh->rmem_maxdepth));

	return BCME_OK;
}

/** txstatus indication, used by ratelinkmem to determine current ratelinkmem, returns error code */
int
wlc_ratelinkmem_upd_txstatus(wlc_info_t *wlc, uint16 index, uint8 flags)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_t *scb;
	scb_ratelinkmem_info_t *scbh;
	uint16 k = index;
	uint16 prev_txptr;

	RL_RMEM1(("%s: index 0x%x (%d) flags 0x%02x\n",
		__FUNCTION__, index, index, flags));

	if (index >= D11_MAX_RATELINK_ENTRIES) {
		WL_ERROR(("wl%d: %s: Invalid index: %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		ASSERT(0);
		return BCME_BADARG;
	}

	scb = rlmi->scb_index[k];
	if (scb == NULL) {
		WL_INFORM(("wl%d: %s, SCB not found for index %d\n",
			wlc->pub->unit, __FUNCTION__, k));
		return BCME_NOTFOUND;
	}

	if (flags & D11_REV128_RATE_PROBE_FLAG) {
		wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, 0 /* frameid */, FALSE, 0, 0 /* ac */);
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh);
	ASSERT(scbh->cubby_valid);

	WL_TRACE(("wl%d: %s %p, %p\n", wlc->pub->unit, __FUNCTION__, scb, scbh));

	if (scbh->rate_state == RATE_ENTRY_STATE_UNINITED) {
		WL_ERROR(("wl%d: %s, Invalid state (uninited, index:%d)\n",
			wlc->pub->unit, __FUNCTION__, index));
		return BCME_ERROR;
	}

	prev_txptr = scbh->rmem_txptr;
	BCM_REFERENCE(prev_txptr);
	while (1) {
		if ((scbh->rate_store[scbh->rmem_txptr].flags & D11_REV128_RATE_TXS_MASK)
			== flags) {
			break;
		}
		if (scbh->rmem_txptr == scbh->rmem_curptr) {
			if (!scbh->rmem_mismatch) {
				WL_ERROR(("wl%d: %s scb %p state %#x txs->flags= 0x%x mismatch "
					"curptr.flags 0x%x index %d rmem_nupd/curptr/txptr/maxdepth"
					" %d %d %d %d\n", wlc->pub->unit, __FUNCTION__, scb,
					scb->state, flags, scbh->rate_store[scbh->rmem_txptr].flags,
					index, scbh->rmem_nupd, scbh->rmem_curptr, prev_txptr,
					scbh->rmem_maxdepth));
			}
			scbh->rmem_mismatch ++;
			return BCME_ERROR;
		}
		scbh->rmem_txptr = RATE_STORE_IDX(scbh->rmem_txptr + 1);
		if (scbh->rmem_mismatch) {
			WL_ERROR(("wl%d: %s scb %p rmem_mismatch %d\n",
				wlc->pub->unit, __FUNCTION__, scb, scbh->rmem_mismatch));
			scbh->rmem_mismatch = 0;
		}
	}

	RL_RMEM1(("%s: txs->index %d flags 0x%x | index %d state %d "
		"nupd/curptr/txptr/maxdepth %d %d %d %d entry.flags 0x%02x\n",
		__FUNCTION__, index, flags, scbh->rate_link_index,
		scbh->rate_state, scbh->rmem_nupd, scbh->rmem_curptr, scbh->rmem_txptr,
		scbh->rmem_maxdepth, scbh->rate_store[scbh->rmem_txptr].flags));

	return BCME_OK;
}

/**
 * Global link memory related functions
 */

/** return index used by this SCB, return D11_RATE_LINK_MEM_IDX_INVALID in case of error */
uint16
wlc_ratelinkmem_get_scb_link_index(wlc_info_t *wlc, scb_t *scb)
{
	if (SCB_PROXD(scb)) {
		/* use special link index; rate and link index will be different */
		return WLC_RLM_SPECIAL_LINK_IDX;
	} else if (SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) || ETHER_ISNULLADDR(&scb->ea)) {
		wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
		/* map all non-SCB traffic to common link entry */
		return WLC_RLM_BSSCFG_LINK_IDX(wlc, bsscfg);
	}
	return wlc_ratelinkmem_get_scb_amt_index(wlc, scb);
}

uint16
wlc_ratelinkmem_vbss_link_index(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	ASSERT(WLC_BSSCFG_IDX(bsscfg) < AMT_IDX_MBSS_RSVD_SIZE);
	ASSERT(!BSSCFG_IS_PRIMARY(bsscfg));
	ASSERT(BSSCFG_BCMC_SCB(bsscfg) != NULL);
	return AMT_IDX_MBSS_RSVD_START + WLC_BSSCFG_IDX(bsscfg);
}

scb_t *
wlc_ratelinkmem_retrieve_cur_scb(wlc_info_t *wlc, uint16 index)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_t* scb;

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		return NULL;
	}

	if (index > D11_MAX_RATELINK_ENTRIES) {
		WL_ERROR(("wl%d: %s: Invalid index: %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		ASSERT(0);
		return NULL;
	}

	scb = rlmi->scb_index[index];
	if (scb == NULL) {
		WL_TRACE(("wl%d: %s, No SCB for link index %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		return NULL;
	}

	return scb;
}

/** return read-only pointer to currently active link entry */
const d11linkmem_entry_t *
wlc_ratelinkmem_retrieve_cur_link_entry(wlc_info_t *wlc, uint16 index)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_t* scb;
	scb_ratelinkmem_info_t *scbh;

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		return NULL;
	}

	if (index >= D11_MAX_RATELINK_ENTRIES) {
		WL_ERROR(("wl%d: %s: Invalid index: %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		ASSERT(0);
		return NULL;
	}

	scb = rlmi->scb_index[index];
	if (scb == NULL) {
		WL_TRACE(("wl%d: %s, No SCB for link index %d\n",
			wlc->pub->unit, __FUNCTION__, index));
		return NULL;
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);
	ASSERT(scbh->rate_link_index == index);
	ASSERT(scbh->cubby_valid == TRUE);

	WL_TRACE(("wl%d: %s, %p, %p\n", wlc->pub->unit, __FUNCTION__, scb, scbh));

	if (scbh->link_state == LINK_ENTRY_STATE_UNINITED)
		return NULL;

	return &scbh->link_entry;
}

/** clear a link entry so that ucode can't access stale data */
static void
wlc_ratelinkmem_delete_link_entry(wlc_info_t *wlc, scb_t *scb)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_ratelinkmem_info_t *scbh;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	if (!wlc->pub->up) {
		return;
	}
	if (wlc->psm_watchdog_debug) {
		return;
	}

	ASSERT(scb != NULL);

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	/* skip any double linkmem delete requests */
	if (!(scbh->link_entry.BssColor_valid & (1 << D11_REV128_LMEMVLD_NBIT))) {
		WL_ERROR(("wl%d: %s Skip MAC command to delete linkmem"
		" for STA "MACF" index %d\n", wlc->pub->unit, __FUNCTION__,
		ETHER_TO_MACF(scb->ea), scbh->rate_link_index));
		return;
	}

	wlc_suspend_mac_and_wait(wlc);

	scbh->link_entry.BssColor_valid &= ~(1 << D11_REV128_LMEMVLD_NBIT);
	/* Valid idx */
	ASSERT(scbh->rate_link_index < D11_MAX_RATELINK_ENTRIES);
	BCM_REFERENCE(scbh);

	/* Notify mac that the link is getting invalidated */
	if (wlc_macreq_upd_lmem(wlc, scbh->rate_link_index, FALSE /* true = update */,
		TRUE /* clr_txbf_stats=1 */) != BCME_OK) {
		WL_ERROR(("wl%d: %s MAC command failed to delete linkmem"
		" for STA "MACF" index %d\n", wlc->pub->unit, __FUNCTION__,
		ETHER_TO_MACF(scb->ea), scbh->rate_link_index));
	}

	if (wlc->clk) {
		W_REG(wlc->osh, D11_PSM_RATEMEM_CTL(wlc),
		      (uint16) ((1 << C_RTMCTL_RST_NBIT) | (1 << C_RTMCTL_UNLOCK_NBIT) |
				scbh->rate_link_index));
	}

	wlc_enable_mac(wlc);

	return;
}

/** trigger an update of the link entry for this SCB, returns error code */
int
wlc_ratelinkmem_update_link_entry(wlc_info_t *wlc, scb_t *scb)
{
	return wlc_ratelinkmem_update_link_entry_onsize(wlc, scb,
			WLC_RLM_LINK_ENTRY_SIZE_SW, FALSE /* clr_txbf_stats=0 in mreq */);
}

/** trigger link entry update for this SCB, including ucode internal data, returns error code */
int
wlc_ratelinkmem_upd_lmem_int(wlc_info_t *wlc, scb_t *scb, bool clr_txbf_stats)
{
	return wlc_ratelinkmem_update_link_entry_onsize(wlc, scb,
			WLC_RLM_LINK_ENTRY_SIZE_TXBF, clr_txbf_stats);
}

/** trigger an update of the link entry of a certain size for this SCB, return error code */
static int
wlc_ratelinkmem_update_link_entry_onsize(wlc_info_t *wlc, scb_t *scb, uint16 size,
	bool clr_txbf_stats)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_ratelinkmem_info_t *scbh;
	bool add_lmem = FALSE;
	uint8 epoch;
	d11linkmem_entry_t link_entry;
	dmaaddr_t pa;
	int index = D11_RATE_LINK_MEM_IDX_INVALID;
	int bme_eng_idx = 0;
#ifdef BCMDMA64OSL
	unsigned long long paddr;
#endif /* BCMDMA64OSL */
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	BCM_REFERENCE(pa);

	if (!RATELINKMEM_ENAB(wlc->pub) || !wlc->pub->up) {
		return BCME_OK;
	}

	ASSERT(scb != NULL);

	if (SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) || ETHER_ISNULLADDR(&scb->ea)) {
		wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
		/* map all non-SCB traffic to common link entry */
		scb = WLC_RLM_BSSCFG_LINK_SCB(wlc, bsscfg);
		index = WLC_RLM_BSSCFG_LINK_IDX(wlc, bsscfg);
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	WL_TRACE(("wl%d: %s, %p, %p\n", wlc->pub->unit, __FUNCTION__, scb, scbh));

	if (scbh->cubby_valid == FALSE) {
		/* this may happen while bringing down an SCB, avoid trying to reinit */
		WL_INFORM(("wl%d: %s, %p invalid; ignore request\n", wlc->pub->unit, __FUNCTION__,
			scb));
		return BCME_NOTFOUND;
	}

	if (SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating linkmem that"
			" is or will be deleted\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_ERROR;
	}

	if (wlc->psm_watchdog_debug) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating linkmem during"
			" psmwd\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_ERROR;
	}

	if (rlmi->block_in_release) {
		WL_INFORM(("wl%d: %s, %p, %p, STA: "MACF" Skip updating linkmem"
			" while in release\n", wlc->pub->unit, __FUNCTION__, scb,
			scbh, ETHER_TO_MACF(scb->ea)));
		return BCME_NOTREADY;
	}

	if ((scbh->rate_state == RATE_ENTRY_STATE_UNINITED) &&
		(scbh->link_state == LINK_ENTRY_STATE_UNINITED)) {
		add_lmem = TRUE;
		if (wlc_ratelinkmem_init_indices(wlc, scb, scbh, index) == FALSE) {
			return BCME_NOTFOUND;
		}
	}

	BCM_REFERENCE(rlmi);

	/* Suspend D11 core before copying link content, to avoid ucode from using partial data */
	wlc_suspend_mac_and_wait(wlc);

	if (scbh->link_state == LINK_ENTRY_STATE_UNINITED) {
		/* overrule size request, always initialize full link entry */
		size = WLC_RLM_LINK_ENTRY_SIZE_TXBF;
		clr_txbf_stats = TRUE;
	}

	if (add_lmem == TRUE) {
		/* Notify mac that the link is getting created */
		if (wlc_macreq_add_lmem(wlc, scbh->rate_link_index) != BCME_OK) {
			WL_ERROR(("wl%d: %s mac command failed to create linkmem"
			" for STA "MACF" index %d %d\n", wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea), scbh->rate_link_index, clr_txbf_stats));
		}
	} else {
		/* Notify mac that the link is getting initialized */
		if (wlc_macreq_upd_lmem(wlc, scbh->rate_link_index, TRUE /* update */,
			clr_txbf_stats) != BCME_OK) {
			WL_ERROR(("wl%d: %s mac command failed to update linkmem"
			" for STA "MACF" index %d %d\n", wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea), scbh->rate_link_index, clr_txbf_stats));
		}
	}

	/* psmx waits for driver to complete writing linkmem */

	/* fill link_entry contents */
	wlc_tx_fill_link_entry(wlc, scb, &scbh->link_entry);

	/* mark it as valid */
	scbh->link_entry.BssColor_valid |= (1 <<D11_REV128_LMEMVLD_NBIT);
	link_entry.BssIdx = 0; /* initialize this as GCC 8.2 and up take issue otherwise */
	wlc_ratelinkmem_raw_lmem_read(wlc, scbh->rate_link_index, &link_entry, 4);
	epoch = (link_entry.BssIdx & D11_REV128_LMEM_EPOCH_MASK) >> D11_REV128_LMEM_EPOCH_SHIFT;
	if (add_lmem == TRUE) {
		epoch = MODINC_POW2(epoch, 4);
	}
	scbh->link_entry.BssIdx &= ~(D11_REV128_LMEM_EPOCH_MASK);
	scbh->link_entry.BssIdx |= epoch << D11_REV128_LMEM_EPOCH_SHIFT;

	/* XXX: ucode internally uses the 4 bytes from offset 44 in linkmem,
	 * but unfortunately DMA write is 8B aligned so that driver overwrites
	 * the field when it needs to write something to offset 40 or beyond.
	 * In order to preserve the value ucode wrote, read it and re-write.
	 * Reading the 4B at offset 44 is unnecessary if (size <= 40) but it doesn't harm.
	 */
	scbh->link_entry.reserved = wlc_ratelinkmem_lmem_read_word(wlc,
		scbh->rate_link_index,
		(OFFSETOF(d11linkmem_entry_t, reserved) / sizeof(uint32)));

	/* Use DMA to transfer the entry to the D11 Link Memory */
#ifndef BCM_SECURE_DMA
	pa = DMA_MAP(wlc->osh, &scbh->link_entry, size, DMA_TX, NULL, NULL);
#else
	pa = SECURE_DMA_MAP(wlc->osh, &scbh->link_entry, size, DMA_TX,
			NULL, NULL, &di->sec_cma_info_tx, 0, SECDMA_TXBUF_POST);
#endif /* BCM_SECURE_DMA */

#ifdef BCMDMA64OSL
	PHYSADDRTOULONG(pa, paddr);
	bme_eng_idx = bme_copy64(wlc->osh, rlmi->bme_key,
		(uint64)paddr,
		(uint64)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, scbh->rate_link_index),
		size);
#else
	bme_eng_idx = bme_copy(wlc->osh, rlmi->bme_key,
		(const uint8*) pa,
		(uint8 *)(uintptr)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, scbh->rate_link_index),
		size);
#endif /* BCMDMA64OSL */

	/* ensure the copy is completed prior to unmapping */
	bme_sync_eng(wlc->osh, bme_eng_idx);

#ifndef BCM_SECURE_DMA
	DMA_UNMAP(wlc->osh, pa, size, DMA_TX, NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, pa, size, DMA_TX, NULL, NULL,
			&di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */

	if (0 && (scbh->link_state == LINK_ENTRY_STATE_UNINITED)) {
		/* clear bottom part of link entry too */
#ifndef BCM_SECURE_DMA
		pa = DMA_MAP(wlc->osh, &rlmi->null_link_entry, WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE,
			DMA_TX, NULL, NULL);
#else
	pa = SECURE_DMA_MAP(wlc->osh, &rlmi->null_link_entry, WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE,
			DMA_TX,	NULL, NULL, &di->sec_cma_info_tx, 0, SECDMA_TXBUF_POST);
#endif /* BCM_SECURE_DMA */
#ifdef BCMDMA64OSL
		PHYSADDRTOULONG(pa, paddr);
		bme_eng_idx = bme_copy64(wlc->osh, rlmi->bme_key,
			(uint64)paddr,
			(uint64)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc,
				scbh->rate_link_index) + WLC_RLM_LINK_ENTRY_SIZE_TXBF,
			WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE);
#else
		bme_eng_idx = bme_copy(wlc->osh, rlmi->bme_key,
			(const uint8*) pa,
			(uint8 *)(uintptr)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc,
				scbh->rate_link_index) + WLC_RLM_LINK_ENTRY_SIZE_TXBF,
			WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE);
#endif /* BCMDMA64OSL */
		/* ensure the copy is completed prior to unmapping */
		bme_sync_eng(wlc->osh, bme_eng_idx);
#ifndef BCM_SECURE_DMA
		DMA_UNMAP(wlc->osh, pa, WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE, DMA_TX, NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, pa, WLC_RLM_LINK_ENTRY_UCODE_BLOCKSIZE, DMA_TX,
			NULL, NULL,	&di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
	}

	/* notify PSMx that update linkmem is done, resume psmx */
	wlc_write_shmx(wlc, MX_MRQ_UPDPEND(wlc), 1);

	/* Transfer finished, so safe to reenable D11 core now */
	wlc_enable_mac(wlc);

	scbh->link_state = LINK_ENTRY_STATE_ACTIVE;
	scbh->cur_link_count++;
	return BCME_OK;
}

void
wlc_ratelinkmem_update_link_twtschedblk(wlc_info_t *wlc, uint idx, uint8 *twtschedblk, uint16 size)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	int bme_eng_idx = 0;
	dmaaddr_t pa;
#ifdef BCMDMA64OSL
	unsigned long long paddr;
#endif /* BCMDMA64OSL */
#ifdef BCM_SECURE_DMA
	dma_info_t *di;
	di = DI_INFO(rlmi->wlc->hw);
#endif /* BCM_SECURE_DMA */

	BCM_REFERENCE(rlmi);

	/* Use DMA to transfer the entry to the D11 Link Memory */
#ifndef BCM_SECURE_DMA
	pa = DMA_MAP(wlc->osh, twtschedblk, size, DMA_TX, NULL, NULL);
#else
	pa = SECURE_DMA_MAP(wlc->osh, twtschedblk, size, DMA_TX,
			NULL, NULL, &di->sec_cma_info_tx, 0, SECDMA_TXBUF_POST);
#endif /* BCM_SECURE_DMA */

#ifdef BCMDMA64OSL
	PHYSADDRTOULONG(pa, paddr);
	bme_eng_idx = bme_copy64(wlc->osh, rlmi->bme_key,
		(uint64)paddr,
		(uint64)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc,
		(AMT_IDX_TWT_RSVD_START + idx)),
		size);
#else
	bme_eng_idx = bme_copy(wlc->osh, rlmi->bme_key,
		(const uint8*) pa,
		(uint8 *)(uintptr)D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc,
		(AMT_IDX_TWT_RSVD_START + idx)), size);
#endif /* BCMDMA64OSL */

	/* ensure the copy is completed prior to unmapping */
	bme_sync_eng(wlc->osh, bme_eng_idx);

#ifndef BCM_SECURE_DMA
	DMA_UNMAP(wlc->osh, pa, size, DMA_TX, NULL, NULL);
#else
		SECURE_DMA_UNMAP(rlmi->wlc->osh, pa, size, DMA_TX, NULL, NULL,
			&di->sec_cma_info_tx, 0);
#endif /* BCM_SECURE_DMA */
}

uint32
wlc_ratelinkmem_lmem_read_word(wlc_info_t *wlc, uint16 index, int offset)
{
	uint32 bp_addr;
	uint32 ret_val;

	if (!wlc->clk) {
		return -1;
	}

	bp_addr = D11_LINK_MEM_ENTRY_ADDR_BME_VIEW(wlc, index);
	si_backplane_access(wlc->pub->sih, bp_addr + (offset * 4), 4, &ret_val, TRUE);
	return ret_val;
}

/* inform RateLinkMem of a band set/change */
void
wlc_ratelinkmem_update_band(wlc_info_t *wlc)
{
	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	WL_TRACE(("wl%d: %s, New band: %d\n", wlc->pub->unit, __FUNCTION__, wlc->band->bandunit));

	if (!IS_SINGLEBAND(wlc)) {
		/* SPECIAL SCB is band-specific, each SCB keeps its own administration (in cubby),
		 * so on a band change we need to update the global admininistration to be in sync
		 * with the new special SCB's cubby, also potentially need to force update to ucode
		 * by resetting state, so that it gets rewritten on next packet transmission
		 */
		wlc_ratelinkmem_reset_special_rate_scb(wlc->rlmi);
	} /* else: nothing to do for single band operation */
} /* wlc_ratelinkmem_update_band */

void
wlc_ratelinkmem_scb_amt_release(wlc_info_t *wlc, scb_t *scb, int index)
{
	scb_ratelinkmem_info_t *scbh;
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	WL_TRACE(("wl%d: %s, SCB %p AMT index %d release\n", wlc->pub->unit, __FUNCTION__,
		scb, index));

	if (scb == WLC_RLM_SPECIAL_RATE_SCB(wlc)) {
		/* 'global' rate entry, don't care */
		WL_ERROR(("wl%d: %s: Ignore AMT index release for SCB: %d, %p\n",
			wlc->pub->unit, __FUNCTION__, index, scb));
		return;
	}

	if ((index < 0) || (index >= D11_MAX_RATELINK_ENTRIES)) {
		WL_ERROR(("wl%d: %s: Invalid AMT index release for SCB: %d, %p\n",
			wlc->pub->unit, __FUNCTION__, index, scb));
		return;
	}

	/* Valid AMT idx release */
	ASSERT(index != WLC_RLM_SPECIAL_RATE_IDX);

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);
	if (scbh->cubby_valid == FALSE) {
		/* avoid writing to mem */
		WL_INFORM(("wl%d: %s, %p invalid; index %d ignore request\n", wlc->pub->unit,
			__FUNCTION__, scb, index));
		return;
	}
	ASSERT((scbh->rate_link_index == D11_RATE_LINK_MEM_IDX_INVALID) ||
		(scbh->rate_link_index == index));

	if (scbh->rate_link_index == index) {
		if (rlmi->scb_index[index] && (rlmi->scb_index[index] != scb)) {
			scb_t *scb_other = rlmi->scb_index[index];
			scb_ratelinkmem_info_t *scbh_other = SCB_RLM_CUBBY(rlmi, scb_other);
			WL_ERROR(("wl%d:%s:Unexpected SCB:%p(valid:%d;index:%d) in table idx:%d\n",
				wlc->pub->unit, __FUNCTION__, scb_other, scbh_other->cubby_valid,
				scbh_other->rate_link_index, index));
			BCM_REFERENCE(scbh_other);
			wlc_ratelinkmem_scb_amt_release(wlc, scb_other, index);
		}

		if (scbh->link_state != LINK_ENTRY_STATE_UNINITED) {
			wlc_ratelinkmem_delete_link_entry(wlc, scb);
		}

		rlmi->scb_index[index] = NULL;
		scbh->rate_link_index = D11_RATE_LINK_MEM_IDX_INVALID;
		scbh->rate_state = RATE_ENTRY_STATE_UNINITED;
		scbh->link_state = LINK_ENTRY_STATE_UNINITED;

		/* avoid scb_ratesel_init from triggering new setup of rate or link mem */
		rlmi->block_in_release = TRUE;
		/* notify RATESEL (and TXC) that existing rateentry is no longer valid */
		wlc_scb_ratesel_init(wlc, scb);
		rlmi->block_in_release = FALSE;
	} /* else nothing to do */
} /* wlc_ratelinkmem_scb_amt_release */

/** update all link entries, or all link entries for a certain BSSCFG */
void
wlc_ratelinkmem_update_link_entry_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool ampdu_only,
	bool clr_txbf_stats)
{
	struct scb *scb;
	struct scb_iter scbiter;
	int idx;
	uint16 size = WLC_RLM_LINK_ENTRY_SIZE_SW;

	ASSERT(RATELINKMEM_ENAB(wlc->pub));

	if (!wlc->pub->up) {
		return;
	}

	wlc_suspend_mac_and_wait(wlc);

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (((bsscfg == NULL) || (SCB_BSSCFG(scb) == bsscfg)) &&
			(!ampdu_only || SCB_AMPDU(scb))) {
			wlc_ratelinkmem_update_link_entry_onsize(wlc, scb, size, clr_txbf_stats);
		}
	}
	if (bsscfg) {
		wlc_ratelinkmem_update_link_entry_onsize(wlc,
			WLC_RLM_BSSCFG_LINK_SCB(wlc, bsscfg), size, clr_txbf_stats);
	} else {
		FOREACH_BSS(wlc, idx, bsscfg) {
			wlc_ratelinkmem_update_link_entry_onsize(wlc,
				WLC_RLM_BSSCFG_LINK_SCB(wlc, bsscfg), size, clr_txbf_stats);
		}
	}
	wlc_enable_mac(wlc);

} /* wlc_ratelinkmem_update_link_entry_all */

/* This function returns true if the linkmem state of the scb is valid
 * This can be called before updating linkmem, to avoid unwanted linkmem
 * creation if already deleted.
 */
bool
wlc_ratelinkmem_lmem_isvalid(wlc_info_t *wlc, scb_t *scb)
{
	wlc_ratelinkmem_info_t *rlmi = wlc->rlmi;
	scb_ratelinkmem_info_t *scbh;

	if (!RATELINKMEM_ENAB(wlc->pub) || !wlc->pub->up) {
		return FALSE;
	}

	ASSERT(scb != NULL);

	if (SCB_INTERNAL(scb) || ETHER_ISMULTI(&scb->ea) || ETHER_ISNULLADDR(&scb->ea)) {
		return FALSE;
	}

	scbh = SCB_RLM_CUBBY(rlmi, scb);
	ASSERT(scbh != NULL);

	WL_TRACE(("wl%d: %s, %p, %p "MACF"\n", wlc->pub->unit, __FUNCTION__, scb, scbh,
		ETHER_TO_MACF(scb->ea)));

	if (scbh->cubby_valid == FALSE) {
		/* this may happen while bringing down an SCB, avoid trying to reinit */
		WL_INFORM(("wl%d: %s, %p invalid; ignore request\n", wlc->pub->unit, __FUNCTION__,
			scb));
		return FALSE;
	}

	if (scbh->link_state == LINK_ENTRY_STATE_UNINITED) {
		WL_INFORM(("wl%d: %s, %p linkmem is not valid \n", wlc->pub->unit, __FUNCTION__,
			scb));
		return FALSE;
	}

	return TRUE;
}
