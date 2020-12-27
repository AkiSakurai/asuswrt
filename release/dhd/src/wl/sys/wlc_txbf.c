/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * beamforming support
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_txbf.c 467328 2014-04-03 01:23:40Z $
 */


#include <wlc_cfg.h>
#include <osl.h>
#include <typedefs.h>
#include <proto/802.11.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_cfg.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <d11.h>
#include <bcmendian.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_vht.h>
#include <wlc_bmac.h>
#include <wlc_txbf.h>
#include <wlc_stf.h>
#include <wlc_txc.h>
#include <wlc_scb.h>
#include <wlc_keymgmt.h>

#ifdef WL_BEAMFORMING

/* iovar table */
enum {
	IOV_TXBF_BFR_CAP,
	IOV_TXBF_BFE_CAP,
	IOV_TXBF_TIMER,
	IOV_TXBF_TRIGGER,
	IOV_TXBF_RATESET,
	IOV_TXBF_MODE,
	IOV_TXBF_VIRTIF_DISABLE,
	IOV_TXBF_BFE_NRX_OV,
	IOV_TXBF_IMP,
	IOV_TXBF_BF_LAST
};

struct txbf_scb_info {
	struct scb *scb;
	wlc_txbf_info_t *txbf;
	uint32  vht_cap;
	bool	exp_en;    /* explicit */
	bool    imp_en;    /* implicit */
	uint8	shm_index; /* index for template & SHM blocks */
	uint8	amt_index;
	uint32  ht_txbf_cap;
	uint8	bfr_capable;
	uint8	bfe_capable;
	uint8	num_bfr_ant; /* number of BFR antenna supported by BFE */
	bool    init_pending;
	uint16  imp_used;  /* implicit stats indiciated imp used */
	uint16  imp_bad;   /* implicit stats indicating bad */
};

struct txbf_scb_cubby {
	struct txbf_scb_info *txbf_scb_info;
};

struct wlc_txbf_info {
	wlc_info_t		*wlc;		/* pointer to main wlc structure */
	wlc_pub_t		*pub;		/* public common code handler */
	osl_t			*osh;		/* OSL handler */
	int	scb_handle; 	/* scb cubby handle to retrieve data from scb */
	uint8 bfr_capable;
	uint8 bfe_capable;
	uint8 mode; 	/* wl txbf 0: txbf is off
			 * wl txbf 1: txbf on/off depends on CLM and expected TXBF system gain
			 * wl txbf 2: txbf is forced on for all rates
			 */
	uint8 active;
	uint8 bfr_shm_index_bitmap;
	struct scb *bfe_scbs[WLC_TXBF_BEAMFORMING_MAX_LINK];
	uint16	shm_base;
	uint16	sounding_period;
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	/* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; /* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
	uint32	max_link;
	uint8	applied2ovr; /* Status of TxBF on/off for override rspec if mode is 1 */
	bool virtif_disable; /* Disable Beamforming on non primary interfaces like P2P,TDLS,AWDL */
	uint8  bfe_nrx_ov; /* number of bfe rx antenna override */
	uint8	imp; 	/* wl txbf_imp 0: Implicit txbf is off
			 * wl txbf_imp 1: Implicit txbf on/off depends on CLM and
			 * expected TXBF system gain
			 * wl txbf_imp 2: Impilict txbf is forced on all single stream rates
			 */
	uint32 flags;
	int8 max_txpwr_limit;
	bool    imp_nochk;  /* enable/disable detect bad implicit txbf results */
};

#define TXBF_SCB_CUBBY(txbf, scb) (struct txbf_scb_cubby *)SCB_CUBBY(scb, (txbf->scb_handle))
#define TXBF_SCB_INFO(txbf, scb) (TXBF_SCB_CUBBY(txbf, scb))->txbf_scb_info

static const bcm_iovar_t txbf_iovars[] = {
	{"txbf", IOV_TXBF_MODE,
	(0), IOVT_UINT8, 0
	},
	{"txbf_bfr_cap", IOV_TXBF_BFR_CAP,
	(IOVF_SET_DOWN), IOVT_BOOL, 0
	},
	{"txbf_bfe_cap", IOV_TXBF_BFE_CAP,
	(IOVF_SET_DOWN), IOVT_BOOL, 0
	},
	{"txbf_timer", IOV_TXBF_TIMER,
	(IOVF_SET_UP), IOVT_INT32, 0
	},
	{"txbf_rateset", IOV_TXBF_RATESET,
	(0), IOVT_BUFFER, sizeof(wl_txbf_rateset_t)
	},
	{"txbf_virtif_disable", IOV_TXBF_VIRTIF_DISABLE,
	(IOVF_SET_DOWN), IOVT_BOOL, 0
	},
#if defined(WLTEST) || defined(WLPKTENG)
	{"txbf_bfe_nrx_ov", IOV_TXBF_BFE_NRX_OV,
	(0), IOVT_INT32, 0
	},
#endif
	{"txbf_imp", IOV_TXBF_IMP,
	(0), IOVT_UINT8, 0
	},
	{NULL, 0, 0, 0, 0}
};

#define BF_SOUND_PERIOD_DFT	(25 * 1000/4)	/* 25 ms, in 4us unit */
#define BF_SOUND_PERIOD_DISABLED 0xffff
#define BF_SOUND_PERIOD_MIN	5	/* 5ms */
#define BF_SOUND_PERIOD_MAX	128	/* 128ms */

#define BF_NDPA_TYPE_CWRTS	0x1d
#define BF_NDPA_TYPE_VHT	0x15
#define BF_FB_VALID		0x100	/* Sounding successful, Phy cache is valid */

#define BF_AMT_MASK	0xF000	/* bit 12L bfm enabled, bit 13:15 idx to M_BFIx_BLK */
#define BF_AMT_BFM_ENABLED	(1 << 12)
#define BF_AMT_BLK_IDX_SHIFT	13

#define TXBF_BFE_MIMOCTL_VHT 	0x8410
#define TXBF_BFE_MIMOCTL_VHT_RXC_MASK 	0x7
#define TXBF_BFE_MIMOCTL_HT	0x788
#define TXBF_BFE_MIMOCTL_HT_RXC_MASK	0x3

#define TXBF_BFE_CONFIG0_BFE_START			0x1
#define TXBF_BFE_CONFIG0_FB_RPT_TYPE_SHIFT		11
#define TXBF_RPT_TYPE_UNCOMPRESSED	0x1
#define TXBF_RPT_TYPE_COMPRESSED		0x2

#define TXBF_BFE_CONFIG0	(TXBF_BFE_CONFIG0_BFE_START | \
		(TXBF_RPT_TYPE_COMPRESSED << TXBF_BFE_CONFIG0_FB_RPT_TYPE_SHIFT))


#define TXBF_BFR_CONFIG0_BFR_START			0x1
#define TXBF_BFR_CONFIG0_FB_RPT_TYPE_SHIFT		1
#define TXBF_BFR_CONFIG0_FRAME_TYPE_SHIFT		3

#define TXBF_BFR_CONFIG0	(TXBF_BFR_CONFIG0_BFR_START | \
		(TXBF_RPT_TYPE_COMPRESSED << TXBF_BFR_CONFIG0_FB_RPT_TYPE_SHIFT))

static int wlc_txbf_get_amt(wlc_info_t *wlc, struct scb* scb, int *amt_idx);

static int
wlc_txbf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);

static int wlc_txbf_up(void *context);
static int wlc_txbf_down(void *context);

static void wlc_txbf_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b);
static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates);

static int wlc_txbf_init_sta(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info);
static void wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info);
static void wlc_txbf_bfe_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info);

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b);
#endif /*  defined(BCMDBG) || defined(WLTEST) */

static int scb_txbf_init(void *context, struct scb *scb);
static void scb_txbf_deinit(void *context, struct scb *scb);
static uint8 wlc_txbf_system_gain_acphy(uint8 bfr_ntx, uint8 bfe_nrx, uint8 std, uint8 mcs,
	uint8 nss, uint8 rate, uint8 bw, bool is_ldpc, bool is_imp);
static void wlc_txbf_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);
static bool wlc_txbf_init_imp(wlc_txbf_info_t *txbf, struct scb *scb, struct txbf_scb_info *bfi);
static void wlc_txbf_impbf_updall(wlc_txbf_info_t *txbf);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
scb_txbf_init(void *context, struct scb *scb)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	struct txbf_scb_cubby *txbf_scb_cubby = (struct txbf_scb_cubby *)TXBF_SCB_CUBBY(txbf, scb);
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(txbf_scb_cubby);

	txbf_scb_info = (struct txbf_scb_info *)MALLOCZ(txbf->osh, sizeof(struct txbf_scb_info));
	if (!txbf_scb_info)
		return BCME_ERROR;

	txbf_scb_info->txbf = txbf;
	txbf_scb_info->scb = scb;
	txbf_scb_cubby->txbf_scb_info = txbf_scb_info;
	txbf_scb_info->init_pending = TRUE;

	if (!SCB_INTERNAL(scb)) {
		wlc_txbf_init_link(txbf, scb);
	}

	return BCME_OK;
}

static void
scb_txbf_deinit(void *context, struct scb *scb)
{

	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	struct txbf_scb_cubby *txbf_scb_cubby;
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(txbf);
	ASSERT(scb);

	txbf_scb_cubby = (struct txbf_scb_cubby *)TXBF_SCB_CUBBY(txbf, scb);
	if (!txbf_scb_cubby) {
		ASSERT(txbf_scb_cubby);
		return;
	}

	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
	if (!txbf_scb_info) {
		ASSERT(txbf_scb_info);
		return;
	}

	if (!SCB_INTERNAL(scb)) {
		wlc_txbf_delete_link(txbf, scb);
	}

	MFREE(txbf->osh, txbf_scb_info, sizeof(struct txbf_scb_info));
	txbf_scb_cubby->txbf_scb_info = NULL;
	return;
}

const wl_txbf_rateset_t rs = { 	{0xff, 0xff,    0, 0},   /* mcs */
				{0xff, 0xff, 0x7e, 0},   /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0x3ff,    0, 0}, /* vht */
				{0x3ff, 0x3ff, 0x7e, 0}, /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},       /* ofdm */
				{0,0,0,0,0,0,0,0},       /* Broadcom-to-Broadcom ofdm */
				0,                       /* ofdm count */
				0,                       /* Broadcom-to-Broadcom ofdm count */
			     };


wlc_txbf_info_t *
BCMATTACHFN(wlc_txbf_attach)(wlc_info_t *wlc)
{
	wlc_txbf_info_t *txbf;
	int i, err;

	if (!(txbf = (wlc_txbf_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_txbf_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		wlc->pub->_txbf = FALSE;
		return NULL;
	}
	txbf->wlc = wlc;
	txbf->pub = wlc->pub;
	txbf->osh = wlc->osh;

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc->pub->_txbf = FALSE;
		return txbf;
	} else {
		wlc->pub->_txbf = TRUE;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, txbf_iovars, "txbf",
		txbf, wlc_txbf_doiovar, NULL, wlc_txbf_up, wlc_txbf_down)) {
		WL_ERROR(("wl%d: txbf wlc_module_register() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}
	wlc_dump_register(wlc->pub, "txbf", (dump_fn_t)wlc_txbf_dump, (void *)txbf);

	if (!wlc->pub->_txbf)
		return txbf;

	txbf->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct txbf_scb_cubby),
		scb_txbf_init, scb_txbf_deinit, NULL, (void *)txbf);
	if (txbf->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}

	/* Add client callback to the scb state notification list */
	if ((err = wlc_scb_state_upd_register(wlc,
	                (bcm_notif_client_callback)wlc_txbf_scb_state_upd_cb, txbf)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
		          wlc->pub->unit, __FUNCTION__, wlc_txbf_scb_state_upd_cb));
		return NULL;
	}

	/* Copy MCS rateset */
	bcopy(rs.txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
	bcopy(rs.txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
	/* Copy VHT rateset */
	for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
		txbf->txbf_rate_vht[i] = rs.txbf_rate_vht[i];
		txbf->txbf_rate_vht_bcm[i] = rs.txbf_rate_vht_bcm[i];
	}
	/* Copy OFDM rateset */
	txbf->txbf_rate_ofdm_cnt = rs.txbf_rate_ofdm_cnt;
	bcopy(rs.txbf_rate_ofdm, txbf->txbf_rate_ofdm, rs.txbf_rate_ofdm_cnt);
	txbf->txbf_rate_ofdm_cnt_bcm = rs.txbf_rate_ofdm_cnt_bcm;
	bcopy(rs.txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
		rs.txbf_rate_ofdm_cnt_bcm);

	wlc_txbf_init(txbf);
	return txbf;
}

void
BCMATTACHFN(wlc_txbf_detach)(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	if (!txbf)
		return;
	wlc = txbf->wlc;
	ASSERT(wlc);
	txbf->pub->_txbf = FALSE;
	wlc_scb_state_upd_unregister(wlc,
	                (bcm_notif_client_callback)wlc_txbf_scb_state_upd_cb, txbf);
	wlc_module_unregister(txbf->pub, "txbf", txbf);
	MFREE(txbf->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
	return;
}

static int
wlc_txbf_up(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	wlc_info_t *wlc = txbf->wlc;
	int txchains = WLC_BITSCNT(wlc->stf->txchain);
	int rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	uint16 val, i;

	uint16	txbf_MLBF_LUT[] = {0x3475, 0x3475, 0x3475, 0x217c,
		0x237b, 0x217c, 0x217c, 0x217c,
		0x3276, 0x3276, 0x3475, 0x187e,
		0x217c, 0x167e, 0x1d7d, 0x1f7c};

	txbf->shm_base = wlc_read_shm(wlc, M_BFI_BLK_PTR);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_REFRESH_THR_OFFSET) * 2,
		txbf->sounding_period);

	if (txchains == 1) {
		txbf->active = 0;
		WL_TXBF(("wl%d: %s beamforming deactived!(txchains < 2)\n",
			wlc->pub->unit, __FUNCTION__));
	} if (txchains == 2) {
		uint16 ndp2s_phyctl0, antmask;

		antmask = ((wlc->stf->txchain << D11AC_PHY_TXC_CORE_SHIFT)
			& D11AC_PHY_TXC_ANT_MASK);

		ndp2s_phyctl0 = (0x8003 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_VHTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);

		ndp2s_phyctl0 = (0x8002 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);
	}

	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_BFI_NRXC_OFFSET) * 2, (rxchains - 1));

	/*
	 * treat 2ss spatial expansion as special form of beamfomring
	 * use fixed index 5
	*/
	val = wlc_read_shm(wlc, (txbf->shm_base + 5 * M_BFI_BLK_PTR  + C_BFI_BFRIDX_POS) * 2);
	val &= (~(1 << 8));
	wlc_write_shm(wlc, (txbf->shm_base +  5 * M_BFI_BLK_PTR + C_BFI_BFRIDX_POS) * 2, val);

	/* initial MLBF_LUT */
	for (i = 0; i <  ARRAYSIZE(txbf_MLBF_LUT); i++) {
		wlc_write_shm(wlc, (txbf->shm_base +  M_BFI_MLBF_LUT + i) * 2, txbf_MLBF_LUT[i]);
	}

	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PHYCTL_OFFSET + 0) * 2, 0x41c2);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PHYCTL_OFFSET + 1) * 2, 0);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PHYCTL_OFFSET + 2) * 2, 0x17);

	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PLCP_OFFSET + 0) * 2, 0x24b);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PLCP_OFFSET + 1) * 2, 0x97);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PLCP_OFFSET + 2) * 2, 0x500);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP3S_PLCP_OFFSET + 3) * 2, 0x0);

	/* ucode picks up the coremask from new shmem for all types of frames if bfm is
	 * set in txphyctl
	*/
	wlc_write_shm(wlc, M_COREMASK_BFM, wlc->stf->txchain);

	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d beamforming mode %d\n",
		wlc->pub->unit, __FUNCTION__,
		txbf->bfr_capable, txbf->bfe_capable, txbf->mode));
	return BCME_OK;
}

void
wlc_txbf_impbf_upd(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;

	if (!txbf)
		return;

	wlc = txbf->wlc;

	if (D11REV_GT(wlc->pub->corerev, 40)) {
		bool is_caled = wlc_phy_is_txbfcal((wlc_phy_t *)WLC_PI(wlc));
		if (is_caled) {
			txbf->flags |= WLC_TXBF_FLAG_IMPBF;
		} else {
			txbf->flags &= ~WLC_TXBF_FLAG_IMPBF;
		}
	}
}

static void
wlc_txbf_impbf_updall(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	struct txbf_scb_info *bfi;

	if (!txbf)
		return;

	wlc = txbf->wlc;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bfi = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) continue;
		wlc_txbf_init_imp(txbf, scb, bfi);
	}
	/* invalid tx header cache */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
}

void
BCMATTACHFN(wlc_txbf_init)(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;

	ASSERT(txbf);
	if (!txbf) {
		return;
	}
	wlc = txbf->wlc;

	txbf->bfr_shm_index_bitmap = 0;
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		if (WLC_BITSCNT(wlc->stf->hw_txchain) >= 1) {
			txbf->bfr_capable = TRUE;
			txbf->mode = 1;
			if (D11REV_GT(wlc->pub->corerev, 40)) {
					txbf->imp = 1;
			}
		} else {
			txbf->bfr_capable = FALSE;
			txbf->mode = 0;
			txbf->imp = 0;
		}
		txbf->bfe_capable = TRUE;
		txbf->active = FALSE;
		txbf->sounding_period = BF_SOUND_PERIOD_DFT;
		txbf->max_link = (WLC_TXBF_BEAMFORMING_MAX_LINK - 1);
		if (D11REV_IS(wlc->pub->corerev, 43)) {
			txbf->max_link = 1;
		}
	} else {
		txbf->bfr_capable = FALSE;
		txbf->bfe_capable = FALSE;
		txbf->mode = 0;
		txbf->imp = 0;
		txbf->active = FALSE;
		txbf->max_link = 0;
	}
	txbf->applied2ovr = 0;
	txbf->virtif_disable = FALSE;
	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d beamforming mode %d"
		"beamforming active %d Allow txbf on non primaryIF: %d\n",
		wlc->pub->unit, __FUNCTION__, txbf->bfr_capable, txbf->bfe_capable,
		txbf->mode, txbf->active, txbf->virtif_disable ? 0: 1));
}

/*
 * initialize explicit beamforming
 * return BCME_OK if explicit is enabled, else
 * return BCME_ERROR
 */
static int
wlc_txbf_init_exp(wlc_txbf_info_t *txbf, struct scb *scb, struct txbf_scb_info *bfi)
{
	wlc_info_t *wlc;
	int amt_idx;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 val;
	int status;
	int idx = -1;
	wlc_bsscfg_t *cfg;

	ASSERT(txbf);
	wlc = txbf->wlc;

	ASSERT(wlc);
	ASSERT(bfi);
	BCM_REFERENCE(eabuf);
	cfg = scb->bsscfg;

	if (!(wlc->txbf->bfr_capable || wlc->txbf->bfe_capable)) {
		return BCME_OK;
	}

	if (wlc->txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg)))
		return BCME_OK;

	if (SCB_VHT_CAP(scb)) {
		uint16 vht_flags;
		vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
		if (vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE)) {
			WL_TXBF(("wl%d: %s: sta %s has VHT BFE cap\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			bfi->bfe_capable = TRUE;
		}
		if (vht_flags & (SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER)) {
			WL_TXBF(("wl%d: %s: sta %s has VHT BFR cap\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			bfi->bfr_capable = TRUE;
		}
		bfi->num_bfr_ant = ((bfi->vht_cap  &
			VHT_CAP_INFO_NUM_BMFMR_ANT_MASK) >> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
	}
#ifdef NOT_YET
	else if (SCB_HT_CAP(scb)) {
		if (TXBF_N_SUPPORTED_BFE(bfi->ht_txbf_cap)) {
			WL_TXBF(("wl%d: %s: sta %s has HT BFE cap\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			bfi->bfe_capable = TRUE;
		}
		if (TXBF_N_SUPPORTED_BFR(bfi->ht_txbf_cap)) {
			WL_TXBF(("wl%d: %s: sta %s has HT BFR cap\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			bfi->bfr_capable = TRUE;
		}
		bfi->num_bfr_ant = ((bfi->ht_txbf_cap &
			HT_CAP_TXBF_CAP_C_BFR_ANT_MASK) >> HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT);
	}
#endif /* NOT_YET */

	if (!((wlc->txbf->bfr_capable && bfi->bfe_capable) ||
		(wlc->txbf->bfe_capable && bfi->bfr_capable)))
		return BCME_OK;

	idx = wlc_txbf_init_sta(txbf, bfi);
	if ((idx < 0) || (idx >= (int)txbf->max_link)) {
		WL_ERROR(("wl%d: %s fail to add bfr blk\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (wlc->txbf->bfr_capable && bfi->bfe_capable) {
		ASSERT(wlc->pub->up);
		wlc_txbf_bfr_init(txbf, bfi);
		bfi->exp_en = TRUE;
		bfi->init_pending = FALSE;

	}

	if (!(wlc->txbf->bfe_capable && bfi->bfr_capable)) {
		return BCME_OK;
	}

	wlc_txbf_bfe_init(txbf, bfi);

	status = wlc_txbf_get_amt(wlc, scb, &amt_idx);
	if (status != BCME_OK)
		return status;

	val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2));
	val &= (~BF_AMT_MASK);
	val |= (BF_AMT_BFM_ENABLED | idx << BF_AMT_BLK_IDX_SHIFT);

	bfi->amt_index = (uint8)amt_idx;
	wlc_write_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2), val);

	WL_TXBF(("wl%d: %s sta %s, enabled for txBF. amt %d, idx %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), amt_idx, idx));
	bfi->init_pending = FALSE;
	return BCME_OK;

}

/*
 * initialize implicit beamforming
 */
static bool
wlc_txbf_init_imp(wlc_txbf_info_t *txbf, struct scb *scb, struct txbf_scb_info *bfi)
{
	wlc_bsscfg_t *cfg;
	cfg = scb->bsscfg;

	if ((txbf->flags & WLC_TXBF_FLAG_IMPBF) && txbf->imp &&
		!(txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg))))
		bfi->imp_en = TRUE;
	else
		bfi->imp_en = FALSE;

	/* as iovar also calls this func, reset counters here */
	bfi->imp_used = 0;
	bfi->imp_bad = 0;
	return bfi->imp_en;
}

/*
 * initialize txbf for this link: turn on explicit if can
 * otherwise try implicit
 * return BCME_OK if any of the two is enabled
 */
int
wlc_txbf_init_link(wlc_txbf_info_t *txbf, struct scb *scb)
{
	struct txbf_scb_info *bfi;
	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	wlc_txbf_init_exp(txbf, scb, bfi);
	if (bfi->exp_en)
		return BCME_OK;

	if (wlc_txbf_init_imp(txbf, scb, bfi))
		return BCME_OK;

	return BCME_ERROR;
}

static int
wlc_txbf_init_sta(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info)
{
	wlc_info_t *wlc;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 bf_shm_idx = 0xff, i;
	bool found = 0;
	struct scb *scb = txbf_scb_info->scb;

	ASSERT(txbf);
	wlc = txbf->wlc;
	BCM_REFERENCE(eabuf);

	if (txbf_scb_info->exp_en) {
		WL_ERROR(("wl%d: %s: scb aleady has user index %d\n", wlc->pub->unit, __FUNCTION__,
			txbf_scb_info->shm_index));
	}

	if (!txbf->active && (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_TXBF(("wl%d: %s: Can not active beamforming no. of txchains %d\n",
			wlc->pub->unit, __FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
	} else if (txbf->mode) {
		WL_TXBF(("wl%d: %s: beamforming actived! txchains %d\n", wlc->pub->unit,
			__FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
		txbf->active = TRUE;
	}

	/* find a free index */
	for (i = 0; i < txbf->max_link; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0) {
			if (!found) {
				bf_shm_idx = i;
				found = 1;
			}
		} else {
			/* check if scb match to any exist entrys */
			if (eacmp(&txbf->bfe_scbs[i]->ea, &scb->ea) == 0) {
				WL_TXBF(("wl%d: %s, TxBF link for  %s alreay exist\n",
					wlc->pub->unit, __FUNCTION__,
					bcm_ether_ntoa(&scb->ea, eabuf)));
					txbf_scb_info->shm_index = i;
				/* all PSTA connection use same txBF link */
				if (!(PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb)))) {
					txbf->bfe_scbs[i] = scb;
				}
				return i;
			} else if (txbf->bfe_scbs[i] ==  scb->psta_prim) {
				/*
				* PSTA Aware AP:STA's belong to same PSTA share a single
				* TxBF link.
				*/
				txbf_scb_info->shm_index = i;
				WL_TXBF(("wl%d: %s, TxBF link for  ProxySTA %s shm_index %d\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->shm_index));
				return txbf_scb_info->shm_index;
			}
		}
	}
	if (!found) {
		WL_ERROR(("%d: %s fail to find a free user index\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	txbf_scb_info->shm_index = bf_shm_idx;
	txbf_scb_info->amt_index = AMT_SIZE;
	txbf->bfr_shm_index_bitmap |= (uint8)((1 << bf_shm_idx));
	txbf->bfe_scbs[bf_shm_idx] = scb;

	WL_TXBF(("%s add  0x%p %s index %d map %x\n", __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), bf_shm_idx, txbf->bfr_shm_index_bitmap));

	return bf_shm_idx;
}

void
wlc_txbf_delete_link(wlc_txbf_info_t *txbf, struct scb *scb)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint32 i;
	wlc_info_t *wlc;
	struct txbf_scb_info *txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
	uint16 vht_flags;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(vht_flags);

	ASSERT(txbf);
	wlc = txbf->wlc;
	ASSERT(wlc);
	ASSERT(txbf_scb_info);

	WL_TXBF(("wl:%d %s %s\n",
		wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed for %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return;
	}

	vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
	for (i = 0; i < txbf->max_link; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;
		if ((txbf->bfe_scbs[i] == scb) || (txbf->bfe_scbs[i] == scb->psta_prim)) {
			break;
		}
	}

	if (i == txbf->max_link) {
		return;
	}

	WL_TXBF(("wl%d: %s delete beamforming link %s shm_index %d amt_index %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->shm_index,
		txbf_scb_info->amt_index));

	if (!(txbf_scb_info->bfr_capable || txbf_scb_info->bfe_capable)) {
		WL_ERROR(("%d: %s STA %s don't have TxBF cap %x\n", wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf), vht_flags));
		return;
	}

	if (!txbf_scb_info->exp_en) {
		/* maybe it was disable due to txchain change, but link is still there */
		WL_ERROR(("%d: %s %s not enabled!\n", wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf)));
	}

	txbf_scb_info->exp_en = FALSE;
	txbf_scb_info->init_pending = TRUE;
	if ((PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb))))
		return;

	WL_TXBF(("%d: %s %s deleted amt %d!\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->amt_index));

	ASSERT(txbf->bfr_shm_index_bitmap & (1 << txbf_scb_info->shm_index));

	if (txbf->bfe_scbs[i] == scb)
		txbf->bfr_shm_index_bitmap &= (~((1 << txbf_scb_info->shm_index)));
	if (wlc->pub->up) {
		uint16 val;
		if ((txbf_scb_info->amt_index < AMT_SIZE) && (txbf->bfe_scbs[i] == scb)) {
			val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + txbf_scb_info->amt_index * 2));
			val &= (~BF_AMT_MASK);
			wlc_write_shm(wlc, (M_AMT_INFO_BLK + txbf_scb_info->amt_index * 2), val);
		}
	}

	if (txbf->bfe_scbs[i] == scb)
		txbf->bfe_scbs[txbf_scb_info->shm_index] = NULL;
	if (!txbf->bfr_shm_index_bitmap) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactived!\n", wlc->pub->unit, __FUNCTION__));
	}
}

static
int wlc_txbf_down(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	uint8 i;

	ASSERT(txbf);
	for (i = 0; i < txbf->max_link; i++) {
		txbf->bfe_scbs[i] = NULL;
	}
	txbf->bfr_shm_index_bitmap = 0;
	txbf->active = FALSE;
	WL_TXBF(("wl%d: %s beamforming deactived!\n", txbf->wlc->pub->unit, __FUNCTION__));
	return BCME_OK;
}


static void
wlc_txbf_bfe_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info)
{
	uint16 bfe_config0;
	uint16 bfe_mimoctl;

	wlc_info_t *wlc;
	struct scb *scb = txbf_scb_info->scb;
	uint16	bf_shm_blk_base;
	uint8 idx;
	bool isVHT = FALSE;
	int rxchains;

	ASSERT(scb);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (SCB_VHT_CAP(scb))
		isVHT = TRUE;

	idx = txbf_scb_info->shm_index;
	bf_shm_blk_base = txbf->shm_base + idx * M_BFI_BLK_SIZE;

	rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	bfe_config0 = TXBF_BFE_CONFIG0;
	if (isVHT) {
		bfe_mimoctl = TXBF_BFE_MIMOCTL_VHT;
		if (CHSPEC_IS40(wlc->chanspec)) {
			bfe_mimoctl |= (0x1 << 6);
		} else if  (CHSPEC_IS80(wlc->chanspec)) {
			bfe_mimoctl |= (0x2 << 6);
		}
		bfe_mimoctl &=  ~TXBF_BFE_MIMOCTL_VHT_RXC_MASK;
		bfe_mimoctl |=  rxchains - 1;

	} else {
		bfe_mimoctl = TXBF_BFE_MIMOCTL_HT;
		if (CHSPEC_IS40(wlc->chanspec)) {
			bfe_mimoctl |= (0x1 << 4);
		}
		bfe_mimoctl &=  ~TXBF_BFE_MIMOCTL_HT_RXC_MASK;
		bfe_mimoctl |=  rxchains - 1;
	}

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_CONFIG0_POS) * 2,
		bfe_config0);
	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_MIMOCTL_POS) * 2,
		bfe_mimoctl);
}

static void
wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info)
{
	uint16 bfr_config0;
	wlc_info_t *wlc;
	struct scb *scb = txbf_scb_info->scb;

	uint16	bf_shm_blk_base, bfrctl = 0;
	uint8 idx;
	bool isVHT = FALSE;

	ASSERT(scb);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (SCB_VHT_CAP(scb))
		isVHT = TRUE;

	idx = txbf_scb_info->shm_index;
	bf_shm_blk_base = txbf->shm_base + idx * M_BFI_BLK_SIZE;

	/* bfr_config0: compressed format only for 11n */
	bfr_config0 = (TXBF_BFR_CONFIG0 | (isVHT << (TXBF_BFR_CONFIG0_FRAME_TYPE_SHIFT)));

	if (CHSPEC_IS40(wlc->chanspec)) {
		bfr_config0 |= (0x1 << 13);
	} else if  (CHSPEC_IS80(wlc->chanspec)) {
		bfr_config0 |= (0x2 << 13);
	}

	wlc_suspend_mac_and_wait(wlc);

	/* NDP streams and VHT/HT */
	if (WLC_BITSCNT(wlc->stf->txchain) == 3) {
		if (txbf_scb_info->num_bfr_ant == 2) {
			/* 3 streams */
			bfrctl |= (1 << C_BFI_BFRCTL_POS_NSTS_SHIFT);
		}
	}

	if (isVHT) {
		bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
		wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_NDPA_TYPE_POS) * 2,
			BF_NDPA_TYPE_VHT);
	} else {
		wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_NDPA_TYPE_POS) * 2,
			BF_NDPA_TYPE_CWRTS);
	}

	if (txbf_scb_info->scb->flags & SCB_BRCM) {
		bfrctl |= (1 << C_BFI_BFRCTL_POS_MLBF_SHIFT);
	}

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFRCTL_POS) * 2, bfrctl);

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFR_CONFIG0_POS) * 2,
		bfr_config0);

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID0_POS) * 2,
		((scb->bsscfg->BSSID.octet[1] << 8) | scb->bsscfg->BSSID.octet[0]));

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID1_POS) * 2,
		((scb->bsscfg->BSSID.octet[3] << 8) | scb->bsscfg->BSSID.octet[2]));

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID2_POS) * 2,
		((scb->bsscfg->BSSID.octet[5] << 8) | scb->bsscfg->BSSID.octet[4]));

	wlc_enable_mac(wlc);
	return;
}

void wlc_txbf_update_vht_cap(wlc_txbf_info_t *txbf, struct scb *scb, uint32 vht_cap_info)
{
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_info_t *wlc;
	struct txbf_scb_info *txbf_scb_info =
		(struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	wlc = txbf->wlc;
	bsscfg = scb->bsscfg;

	BCM_REFERENCE(bsscfg);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed\n", txbf->wlc->pub->unit, __FUNCTION__));
		return;
	}
	txbf_scb_info->vht_cap = vht_cap_info;

	if (BSSCFG_IBSS(bsscfg) ||
#ifdef WLTDLS
	BSSCFG_IS_TDLS(bsscfg)) {
#else
	FALSE) {
#endif
		if (!(txbf->bfr_capable || txbf->bfe_capable)) {
			return;
		}

		if (SCB_VHT_CAP(scb)) {
			uint16 vht_flags;
			vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
			if ((vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE |
				SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER)) &&
				txbf_scb_info->init_pending) {
				wlc_txbf_init_link(txbf, scb);
			}
		}
	}
}

static void
wlc_txbf_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)ctx;
	struct scb *scb;
	uint8 oldstate;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);
	oldstate = notif_data->oldstate;

	/* when transitioning to ASSOCIATED state
	* intitialize txbf link for this SCB.
	*/
	if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb))
		wlc_txbf_delete_link(txbf, scb);
	else if (SCB_ASSOCIATED(scb))
		wlc_txbf_init_link(txbf, scb);
}

void wlc_txbf_applied2ovr_upd(wlc_txbf_info_t *txbf, bool bfen)
{
	txbf->applied2ovr = bfen;
}

uint8 wlc_txbf_get_applied2ovr(wlc_txbf_info_t *txbf)
{
	return txbf->applied2ovr;
}

void
wlc_txbf_upd(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	int txchains;

	if (!txbf)
		return;

	wlc = txbf->wlc;
	ASSERT(wlc);

	txchains = WLC_BITSCNT(txbf->wlc->stf->txchain);

	if ((txbf->mode ||
		txbf->imp) &&
		(txchains > 1) &&
		(wlc->stf->allow_txbf == TRUE))
		wlc->allow_txbf = TRUE;
	else
		wlc->allow_txbf = FALSE;

}

void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf)
{
	int rxchains;
	wlc_info_t * wlc;

	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!txbf->mode)
		return;

	if (!wlc->clk)
		return;

	rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_BFI_NRXC_OFFSET) * 2, (rxchains - 1));
}

void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf)
{
	int txchains = WLC_BITSCNT(txbf->wlc->stf->txchain);
	uint32 i;
	wlc_info_t * wlc;

	wlc = txbf->wlc;
	ASSERT(wlc);

	wlc_txbf_upd(txbf);

	if (!wlc->clk)
		return;

	if (!txbf->mode)
		return;

	if (txchains < 2) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactived!(txchains < 2)\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if ((!txbf->active) && (txbf->bfr_shm_index_bitmap)) {
		txbf->active = TRUE;
		WL_TXBF(("wl%d: %s beamforming reactived! txchain %d\n", wlc->pub->unit,
			__FUNCTION__, wlc->stf->txchain));
	}

	if ((!wlc->pub->up) || (txbf->bfr_shm_index_bitmap == 0))
		return;

	/* ucode picks up the coremask from new shmem for all types of frames if bfm is
	 * set in txphyctl
	 */
	wlc_write_shm(wlc, M_COREMASK_BFM, wlc->stf->txchain);

	if (txchains == 2) {
		uint16 ndp2s_phyctl0, antmask;

		antmask = ((wlc->stf->txchain << D11AC_PHY_TXC_CORE_SHIFT)
			& D11AC_PHY_TXC_ANT_MASK);

		ndp2s_phyctl0 = (0x8003 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_VHTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);

		ndp2s_phyctl0 = (0x8002 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);
	}

	wlc_suspend_mac_and_wait(wlc);
	for (i = 0; i < txbf->max_link; i++) {
		uint16 val, shm_blk_base, bfrctl = 0;
		struct scb *scb;
		struct txbf_scb_info *txbf_scb_info;

		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;

		shm_blk_base = txbf->shm_base + i * M_BFI_BLK_SIZE;

		scb = txbf->bfe_scbs[i];
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

		/* NDP streams and VHT/HT */
		if ((txchains == 3) && (txbf_scb_info->num_bfr_ant == 2)) {
			/* 3 streams */
			bfrctl = (1 << C_BFI_BFRCTL_POS_NSTS_SHIFT);
		}
		if (SCB_VHT_CAP(scb)) {
			bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
		}

		wlc_write_shm(wlc, (shm_blk_base + C_BFI_BFRCTL_POS) * 2, bfrctl);

		/* clean up phy cache */
		val = wlc_read_shm(wlc, (shm_blk_base + C_BFI_BFRIDX_POS) * 2);
		val &= (~(1 << 8));
		wlc_write_shm(wlc, (shm_blk_base + C_BFI_BFRIDX_POS) * 2, val);
	}

	wlc_enable_mac(wlc);

	/* invalidate txcache since rates are changing */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);

}

void
wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf)
{
	uint16 val;
	uint16 bf_shm_blk_base, bf_shm_base;
	uint32 i;
	wlc_info_t * wlc;

	wlc = txbf->wlc;
	ASSERT(wlc);

	bf_shm_base = txbf->shm_base;
	for (i = 0; i < txbf->max_link; i++) {
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		bf_shm_blk_base = bf_shm_base + i * M_BFI_BLK_SIZE;
		val = wlc_read_shm(wlc, (bf_shm_blk_base + C_BFI_BFRIDX_POS) * 2);
		val &= (~(1 << 8));
		wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFRIDX_POS) * 2, val);
	}
}

static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates)
{
	int i;
	for (i = 0; i < num_of_rates; i++) {
		if (rate == (supported_rates[i] & RSPEC_RATE_MASK))
			return TRUE;
	}
	return FALSE;
}
void
wlc_txbf_txpower_target_max_upd(wlc_txbf_info_t *txbf, int8 max_txpwr_limit)
{
	if (!txbf)
		return;

	txbf->max_txpwr_limit = max_txpwr_limit;
}

/* Note: we haven't used bw and is_ldpc information yet */
static uint8 wlc_txbf_system_gain_acphy(uint8 bfr_ntx, uint8 bfe_nrx, uint8 std, uint8 mcs,
	uint8 nss, uint8 rate, uint8 bw, bool is_ldpc, bool is_imp)
{
	/* Input format
	 *   std:
	 *      0:  dot11ag
	 *      1:  11n
	 *      2:  11ac
	 *
	 *   rate:  phyrate of dot11ag format, e.g. 6/9/12/18/24/36/48/54 Mbps
	 *
	 *   bw:  20/40/80 MHz
	 *
	 * Output format:
	 *   expected TXBF system gain in 1/4dB step
	 */

	uint8 gain_3x1[10] = {0, 6, 16, 18, 18, 18, 18, 18, 18, 18};
	uint8 gain_3x2[20] = {0, 6, 14, 16, 16, 16, 16, 16, 16, 16,
	4, 4,  6,  6,  6,  6,  6,  6,  6,  6};
	uint8 gain_3x3[30] = {0, 6, 12, 14, 14, 14, 14, 14, 14, 14,
	4, 4,  6,  6,  6,  6,  6,  6,  6,  6,
	0, 2,  2,  6,  2,  6,  4,  0,  0,  0};
	uint8 gain_2x1[10] = {0, 6, 12, 12, 12, 12, 12, 12, 12, 12};
	uint8 gain_2x2[20] = {0, 6, 10, 10, 10, 10, 10, 10, 10, 10,
	0, 2,  2,  6,  2,  6,  4,  0,  0,  0};
	uint8 gain_imp_3x1[10] = {0, 4, 10, 12, 12, 12, 12, 12, 12, 12};
	uint8 gain_imp_3x2[10] = {0, 4,  4,  5,  5,  5,  5,  5,  5,  5};
	uint8 gain_imp_3x3[10] = {0, 0,  0,  0,  0,  0,  0,  0,  0,  0};
	uint8 gain_imp_2x1[10] = {0, 2,  4,  6,  8,  8,  8,  8,  8,  8};
	uint8 gain_imp_2x2[10] = {0, 0,  0,  0,  0,  0,  0,  0,  0,  0};
	uint8 *gain = gain_3x3, idx = 0, expected_gain = 0;

	switch (std) {
	case 0: /* dot11ag */
		switch (rate) {
		case 6:
			idx = 0; break;
		case 9:
			idx = 0; break;
		case 12:
			idx = 1; break;
		case 18:
			idx = 2; break;
		case 24:
			idx = 3; break;
		case 36:
			idx = 4; break;
		case 48:
			idx = 5; break;
		case 54:
			idx = 6; break;
		}
		break;
	case 1: /* 11n */
		idx = (mcs % 8) + (mcs >> 3) * 10;
		break;
	case 2: /* 11ac */
		idx = mcs + (nss - 1) * 10;
		break;
	}

	if (bfr_ntx == 3 && bfe_nrx == 1) {
		gain = is_imp ? gain_imp_3x1 : gain_3x1;
	} else if (bfr_ntx == 3 && bfe_nrx == 2) {
		gain = is_imp ? gain_imp_3x2 : gain_3x2;
	} else if (bfr_ntx == 3 && bfe_nrx == 3) {
		gain = is_imp ? gain_imp_3x3 : gain_3x3;
	} else if (bfr_ntx == 2 && bfe_nrx == 1) {
		gain = is_imp ? gain_imp_2x1 : gain_2x1;
	} else if (bfr_ntx == 2 && bfe_nrx == 2) {
		gain = is_imp ? gain_imp_2x2 : gain_2x2;
	} else 	{
		gain = is_imp ? gain_imp_3x3 : gain_3x3;
	}

	expected_gain = gain[idx];

	return expected_gain;
}

/* Txbf on/off based on CLM and expected TXBF system gain:
 * In some locales and some channels, TXBF on provides a worse performance than TXBF off.
 * This is mainly because the CLM power of TXBF on is much lower than TXBF off rates, and
 * offset the TXBF system gain leading to a negative net gain.
 * wlc_txbf_system_gain_acphy() function provides the expected TXBF system gain.
 * wlc_check_expected_txbf_system_gain() uses the above function and compare with the CLM power
 * back off of TXBF rates vs  non-TXBF rates, and make a decision to turn on/off TXBF for each
 * rate.
 */
bool wlc_txbf_bfen(wlc_txbf_info_t *txbf, struct scb *scb,
	ratespec_t rspec, txpwr204080_t* txpwrs, bool is_imp)
{
	wlc_info_t *wlc;
	uint8 ntx_txbf,  bfe_nrx, std,  mcs,  nss_txbf,  rate,  bw, ntx;
	int8 pwr, pwr_txbf, gain, gain_comp, txbf_target_pwr;
	bool is_ldpc = FALSE;
	ratespec_t txbf_rspec;
	wlc = txbf->wlc;

	ASSERT(!(rspec & RSPEC_TXBF));

	txbf_rspec = rspec;
	txbf_rspec &= ~(RSPEC_TXEXP_MASK | RSPEC_STBC);
	txbf_rspec |= RSPEC_TXBF;
	/* fill TXEXP bits to indicate TXBF0,1 or 2 */
	txbf_rspec |= ((wlc_stf_txchain_get(wlc, txbf_rspec)
		   - wlc_ratespec_nsts(txbf_rspec)) << RSPEC_TXEXP_SHIFT);

	ntx_txbf = wlc_stf_txchain_get(wlc, txbf_rspec);
	nss_txbf = (uint8) wlc_ratespec_nss(txbf_rspec);

	/* bfe number of rx antenna override set? */
	if (txbf->bfe_nrx_ov)
		bfe_nrx = txbf->bfe_nrx_ov;
	else {
		/* Number of BFE's rxchain is derived from its mcs map */
		if (SCB_VHT_CAP(scb))
			/* VHT capable peer */
			bfe_nrx = VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap) ? 3 :
			(VHT_MCS_SS_SUPPORTED(2, scb->rateset.vht_mcsmap) ? 2 : 1);

		else if (SCB_HT_CAP(scb))
			/* HT Peer */
			bfe_nrx = (scb->rateset.mcs[2] != 0) ?
				3 : ((scb->rateset.mcs[1] != 0) ? 2 : 1);
		else
			/* Legacy */
			bfe_nrx = 1;
	}

	/* Disable txbf for ntx_bfr = nrx_bfe = nss = 2 or 3 */
	if ((nss_txbf == 2 && ntx_txbf == 2 && bfe_nrx == 2) ||
		(nss_txbf == 3 && ntx_txbf == 3 && bfe_nrx == 3))
		return FALSE;

	ntx = wlc_stf_txchain_get(wlc, rspec);
	ASSERT((ntx_txbf > 1) && (ntx_txbf >= ntx));

	is_ldpc = RSPEC_ISLDPC(txbf_rspec);
	bw =  RSPEC_IS80MHZ(rspec) ? BW_80MHZ : (RSPEC_IS40MHZ(rspec) ?  BW_40MHZ : BW_20MHZ);
	/* retrieve power offset on per packet basis */
	pwr = txpwrs->pbw[bw - BW_20MHZ][TXBF_OFF_IDX];
	/* Sign Extend */
	pwr <<= 2;
	pwr >>= 2;
	/* convert power offset from 0.5 dB to 0.25 dB */
	pwr = (pwr * WLC_TXPWR_DB_FACTOR) / 2;

	/* gain compensation code */
	if ((ntx == 1) && (ntx_txbf == 2))
		gain_comp = 12;
	else if ((ntx == 1) && (ntx_txbf == 3))
		gain_comp = 19;
	else if ((ntx == 2) && (ntx_txbf == 3))
		gain_comp = 7;
	else
		gain_comp = 0;

	pwr += gain_comp;

	if (RSPEC_ISVHT(txbf_rspec)) {
		mcs = txbf_rspec & RSPEC_VHT_MCS_MASK;
		rate = 0;
	        std = 2; /* 11AC */
	} else if (RSPEC_ISHT(txbf_rspec)) {
		mcs = txbf_rspec & RSPEC_RATE_MASK;
		rate = 0;
		std = 1; /* 11N */
	} else {
		rate = RSPEC2RATE(txbf_rspec);
		mcs = 0;
		std = 0; /* Legacy */
	}

	gain = wlc_txbf_system_gain_acphy(ntx_txbf, bfe_nrx, std, mcs, nss_txbf,
		rate, bw, is_ldpc, is_imp);

	/* retrieve txbf power offset on per packet basis */
	pwr_txbf = txpwrs->pbw[bw-BW_20MHZ][TXBF_ON_IDX];
	/* Sign Extend */
	pwr_txbf <<= 2;
	pwr_txbf >>= 2;
	/* convert power offset from 0.5 dB to 0.25 dB */
	pwr_txbf = (pwr_txbf * WLC_TXPWR_DB_FACTOR) / 2;

	txbf_target_pwr =  txbf->max_txpwr_limit - pwr_txbf;

	WL_TXBF(("%s: ntx %u ntx_txbf %u bfe_nrx %u std %u mcs %u nss_txbf %u rate %u"
		" bw %u is_ldpc %u\n", __FUNCTION__, ntx, ntx_txbf, bfe_nrx, std, mcs,
		nss_txbf, rate, bw, is_ldpc));

	WL_TXBF(("%s: %s TxBF: txbf_target_pwr %d qdB pwr %d qdB pwr_txbf %d qdB gain %d qdB"
		" gain_comp %d qdB\n", __FUNCTION__, ((txbf_target_pwr > 4) &&
		((pwr - pwr_txbf + gain) >= 0) ? "Enable" : "Disable"), txbf_target_pwr,
		(pwr - gain_comp), pwr_txbf, gain, gain_comp));

	/* Turn off TXBF when target power <= 1.25dBm */
	if ((txbf_target_pwr > ((BCM94360_MINTXPOWER * WLC_TXPWR_DB_FACTOR) + 1)) &&
		((pwr - pwr_txbf + gain) >= 0)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool
wlc_txbf_imp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb,
	txpwr204080_t* txpwrs)
{
	wlc_info_t *wlc = txbf->wlc;
	struct txbf_scb_info *bfi;
	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (bfi->imp_en == FALSE || IS_CCK(rspec) || (wlc_ratespec_nss(rspec) > 1) ||
#ifdef WLTXBF_2G_DISABLED
		(BAND_2G(wlc->band->bandtype)) ||
#endif /* WLTXBF_2G_DISABLED */
		/* 4350 can not beamform a dot11ag(OFDM) frame */
		(BCM4350_CHIP(wlc->pub->sih->chip) && IS_OFDM(rspec)))
		return FALSE;

	/* wl txbf_imp 2: imp txbf is forced on for all rates */
	if (wlc->txbf->imp == 2) {
		return TRUE;
	} else
		return wlc_txbf_bfen(txbf, scb, rspec, txpwrs, TRUE);
}

bool wlc_txbf_exp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs)
{
	uint nss, rate;
	struct txbf_scb_info *txbf_scb_info;
	int is_brcm_sta = (scb->flags & SCB_BRCM);
	bool is_valid = FALSE;

	txbf_scb_info = TXBF_SCB_INFO(txbf, scb);
	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed\n", txbf->wlc->pub->unit, __FUNCTION__));
		*shm_index = BF_SHM_IDX_INV;
		return FALSE;
	}

	if (!txbf_scb_info->exp_en) {
		*shm_index = BF_SHM_IDX_INV;
		return FALSE;
	}

	*shm_index = txbf_scb_info->shm_index;

	if (IS_CCK(rspec) || (txbf->mode == 0))
		return FALSE;

	if (IS_OFDM(rspec)) {
		if (is_brcm_sta) {
			if (txbf->txbf_rate_ofdm_cnt_bcm == TXBF_RATE_OFDM_ALL)
				is_valid = TRUE;
			else if (txbf->txbf_rate_ofdm_cnt_bcm)
				is_valid = wlc_txbf_check_ofdm_rate((rspec & RSPEC_RATE_MASK),
					txbf->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_cnt_bcm);
		} else {
			if (txbf->txbf_rate_ofdm_cnt == TXBF_RATE_OFDM_ALL)
				is_valid = TRUE;
			else if (txbf->txbf_rate_ofdm_cnt)
				is_valid = wlc_txbf_check_ofdm_rate((rspec & RSPEC_RATE_MASK),
					txbf->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		}
	}

	nss = wlc_ratespec_nss(rspec);
	ASSERT(nss >= 1);
	nss -= 1;
	if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & RSPEC_VHT_MCS_MASK);
		if (is_brcm_sta)
			 is_valid = (((1 << rate) & txbf->txbf_rate_vht_bcm[nss]) != 0);
		else
			is_valid =  (((1 << rate) & txbf->txbf_rate_vht[nss]) != 0);

	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & RSPEC_RATE_MASK) - (nss) * 8;

		if (is_brcm_sta)
			is_valid = (((1 << rate) & txbf->txbf_rate_mcs_bcm[nss]) != 0);
		else
			is_valid = (((1 << rate) & txbf->txbf_rate_mcs[nss]) != 0);
	}

	if (!is_valid)
		return FALSE;

	if (txbf->mode == 2)
		return TRUE;
	else
		return wlc_txbf_bfen(txbf, scb, rspec, txpwrs, FALSE);
}

bool wlc_txbf_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs)
{
	bool ret = FALSE;
	ASSERT(!(rspec & RSPEC_TXBF));

	ret = wlc_txbf_exp_sel(txbf, rspec, scb, shm_index, txpwrs);

	if (*shm_index != BF_SHM_IDX_INV)
		return ret;
	else
		ret = wlc_txbf_imp_sel(txbf, rspec, scb, txpwrs);

	return ret;
}

void
wlc_txbf_imp_txstatus(wlc_txbf_info_t *txbf, struct scb *scb, tx_status_t *txs)
{
	wlc_info_t *wlc = txbf->wlc;
	struct txbf_scb_info *bfi;

	if (SCB_INTERNAL(scb)) {
		return;
	}

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (!bfi->imp_en || txbf->imp_nochk)
		return;

	if (txs->status.s5 & TX_STATUS40_IMPBF_LOW_MASK)
		return;

	bfi->imp_used ++;
	if (txs->status.s5 & TX_STATUS40_IMPBF_BAD_MASK) {
			bfi->imp_bad ++;
	}

	if (bfi->imp_used >= 200 && ((bfi->imp_bad * 3) > bfi->imp_used)) {
		/* if estimated bad percentage > 1/3, disable implicit */
		bfi->imp_en = FALSE;
		if (WLC_TXC_ENAB(wlc))
			wlc_txc_inv_all(wlc->txc);
		WL_TXBF(("%s: disable implicit txbf. used %d bad %d\n",
			__FUNCTION__, bfi->imp_used, bfi->imp_bad));
	}

	if (bfi->imp_used >= 200) {
		WL_INFORM(("%s stats: used %d bad %d\n",
			__FUNCTION__, bfi->imp_used, bfi->imp_bad));
		/* reset it to avoid stale info. some rate might survive. */
		bfi->imp_used = 0;
		bfi->imp_bad = 0;
	}
	return;
}

void
wlc_txbf_fix_rspec_plcp(wlc_txbf_info_t *txbf, ratespec_t *prspec, uint8 *plcp)
{
	uint nsts;
	wlc_info_t *wlc = txbf->wlc;
	ratespec_t txbf0_rspec = *prspec;

	*prspec &= ~(RSPEC_TXEXP_MASK | RSPEC_STBC);
	/* Enable TxBF in rspec */
	*prspec |= RSPEC_TXBF;
	/* fill TXEXP bits to indicate TXBF0,1 or 2 */
	nsts = wlc_ratespec_nsts(*prspec);
	*prspec |= ((wlc_stf_txchain_get(wlc, *prspec) - nsts) << RSPEC_TXEXP_SHIFT);

	if (!RSPEC_ISSTBC(txbf0_rspec))
		return;

	if (RSPEC_ISVHT(*prspec)) {
		uint16 plcp0 = 0;
			/* put plcp0 in plcp */
		plcp0 = (plcp[1] << 8) | (plcp[0]);

		/* clear bit 3 stbc coding */
		plcp0 &= ~VHT_SIGA1_STBC;
		/* update NSTS */
		plcp0 &= ~ VHT_SIGA1_NSTS_SHIFT_MASK_USER0;
		plcp0 |= ((uint16)(nsts - 1) << VHT_SIGA1_NSTS_SHIFT);

		/* put plcp0 in plcp */
		plcp[0] = (uint8)plcp0;
		plcp[1] = (uint8)(plcp0 >> 8);

	} else if (RSPEC_ISHT(*prspec))
		plcp[3] &= ~PLCP3_STC_MASK;

	return;
}

static void wlc_txbf_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b)
{
	uint32 i;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct scb *scb;
	struct txbf_scb_info *txbf_scb_info;
	wlc_info_t *wlc = txbf->wlc;
	struct scb_iter scbiter;

	BCM_REFERENCE(eabuf);

	if (!TXBF_ENAB(wlc->pub)) {
		bcm_bprintf(b, "Beamforming is not supported!\n");
	}

	bcm_bprintf(b, "TxBF bfr capable:%d bfe capable:%d, mode:%d, active:%d,"
		" Allowed by country code: %d Allow txbf on non primaryIF: %d\n",
		txbf->bfr_capable, txbf->bfe_capable,
		txbf->mode, txbf->active, wlc->stf->allow_txbf, (txbf->virtif_disable ? 0 : 1));
	bcm_bprintf(b, "%d links with beamforming enabled\n",
		WLC_BITSCNT(txbf->bfr_shm_index_bitmap));

	for (i = 0; i < txbf->max_link; i++) {
		bool valid;
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		scb = txbf->bfe_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

		valid = (wlc_read_shm(wlc, (txbf->shm_base + i * M_BFI_BLK_SIZE) * 2)
				& BF_FB_VALID) ? TRUE : FALSE;

		bcm_bprintf(b, "%d:   %s\n     VHT cap: 0x%x exp_en: %d index: %d amt: %d"
			" Sounding successful: %d\n",
			(i + 1), bcm_ether_ntoa(&scb->ea, eabuf),
			txbf_scb_info->vht_cap, txbf_scb_info->exp_en,
			txbf_scb_info->shm_index, txbf_scb_info->amt_index, valid);
	}
#if defined(BCMDBG) || defined(WLTEST)
	wlc_txbf_shm_dump(txbf, b);
#endif /* defined(BCMDBG) || defined(WLTEST) */

	/*
	 * dump implicit info
	 */
	bcm_bprintf(b, "Implicit txbf: mode %d calibr_flag %d\n",
		txbf->imp, (txbf->flags & WLC_TXBF_FLAG_IMPBF) ? 1 : 0);
	if (!((txbf->flags & WLC_TXBF_FLAG_IMPBF) && txbf->imp))
	    return;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
		if (!txbf_scb_info) continue;
		bcm_bprintf(b, "%s: en %d imp_used %d imp_bad %d\n",
			bcm_ether_ntoa(&scb->ea, eabuf),
			txbf_scb_info->imp_en, txbf_scb_info->imp_used, txbf_scb_info->imp_bad);
	}
}

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b)
{
	uint32 i, offset;
	uint16 bf_shm_base, val;
	wlc_info_t *wlc = txbf->wlc;
	char* stats[11] = {"txndpa", "txndp", "txsf", "txcwrts", "txcwcts", "txbfm",
			   "rxndpa", "bferptrdy", "rxsf", "rxcwrts", "rxcwcts"};

	if (!wlc->clk)
		return;


	for (i = 0; i < AMT_SIZE; i++) {
		val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + i * 2));
		if (val)
			bcm_bprintf(b, "AMT_INFO_BLK[%d] = 0x%x\n", i, val);
	}

	txbf->shm_base = wlc_read_shm(wlc, M_BFI_BLK_PTR);
	bf_shm_base = txbf->shm_base;

	for (i = 0; i < txbf->max_link; i++) {
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		bcm_bprintf(b, "Block %d:\n", i);
		bf_shm_base = txbf->shm_base + i * M_BFI_BLK_SIZE;
		for (offset = 0; offset < M_BFI_BLK_SIZE; offset += 4) {
			bcm_bprintf(b, "offset %2d: %04x %04x %04x %04x\n", offset,
				wlc_read_shm(wlc, (bf_shm_base + offset)*2),
				wlc_read_shm(wlc, (bf_shm_base + offset + 1)*2),
				wlc_read_shm(wlc, (bf_shm_base + offset + 2)*2),
				wlc_read_shm(wlc, (bf_shm_base + offset + 3)*2));
		}
	}

	bcm_bprintf(b, "Statistics:\n");
	bcm_bprintf(b, "%s %d %s %d %s %d %s %d %s %d %s %d\n",
		stats[0], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txndpa)),
		stats[1], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txndp)),
		stats[2], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txsf)),
		stats[3], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txcwrts)),
		stats[4], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txcwcts)),
		stats[5], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, txbfm)));
	bcm_bprintf(b, "%s %d %s %d %s %d %s %d %s %d\n",
		stats[6], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, rxndpaucast)),
		stats[7], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, bferptrdy)),
		stats[8], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, rxsfucast)),
		stats[9], wlc_read_shm(wlc, M_UCODE_MACSTAT1 + OFFSETOF(macstat1_t, rxcwrtsucast)),
		stats[10], wlc_read_shm(wlc, M_UCODE_MACSTAT1+ OFFSETOF(macstat1_t, rxcwctsucast)));
}
#endif 
/*
 * query keymgmt for the entry to use
 */
static int
wlc_txbf_get_amt(wlc_info_t *wlc, struct scb *scb, int *amt_idx)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	const struct ether_addr *ea;
	int err = BCME_OK;

	ASSERT(amt_idx != NULL && scb != NULL);

	BCM_REFERENCE(eabuf);

	ea = &scb->ea;
	WL_TXBF(("wl%d: %s: ea: %s\n", WLCWLUNIT(wlc), __FUNCTION__,
		bcm_ether_ntoa(ea, eabuf)));
	BCM_REFERENCE(ea);

	if (D11REV_LT(wlc->pub->corerev, 40))
		return BCME_UNSUPPORTED;

	if (!wlc->clk)
		return BCME_NOCLK;

	/* keymgmt currently owns amt A2/TA amt allocations */
	*amt_idx = wlc_keymgmt_get_scb_amt_idx(wlc->keymgmt, scb);
	if (*amt_idx < 0)
		err = *amt_idx;

	WL_TXBF(("wl%d: %s, amt entry %d status %d\n",
		WLCWLUNIT(wlc), __FUNCTION__, *amt_idx, err));
	return err;
}

void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int amt_idx, const struct ether_addr *addr)
{
	uint32 i, shm_idx;
	uint16 val;
	wlc_info_t *wlc;

	uint16 attr;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct ether_addr tmp;
	struct scb *scb;
	struct txbf_scb_info *txbf_scb_info;
	int32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *psta_scb = NULL;

	BCM_REFERENCE(eabuf);
	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!TXBF_ENAB(wlc->pub)) {
		return;
	}

	if (txbf->bfr_shm_index_bitmap == 0)
		return;

	wlc_bmac_read_amt(wlc->hw, amt_idx, &tmp, &attr);
	if ((attr & ((AMT_ATTR_VALID) | (AMT_ATTR_A2)))
		!= ((AMT_ATTR_VALID) | (AMT_ATTR_A2))) {
		return;
	}

	/* PSTA AWARE AP: Look for PSTA SCB */
	FOREACH_BSS(wlc, idx, cfg) {
		psta_scb = wlc_scbfind(wlc, cfg, addr);
		if (psta_scb != NULL)
			break;
	}

	for (i = 0; i < txbf->max_link; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;

		scb = txbf->bfe_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
		ASSERT(txbf_scb_info);
		if (!txbf_scb_info) {
			WL_ERROR(("wl:%d %s update amt %x for %s failed\n",
				wlc->pub->unit, __FUNCTION__, amt_idx,
				bcm_ether_ntoa(addr, eabuf)));
			return;
		}

		if ((eacmp(&txbf->bfe_scbs[i]->ea, addr) == 0) ||
			((psta_scb != NULL) && (txbf->bfe_scbs[i] == psta_scb->psta_prim))) {
			shm_idx = txbf_scb_info->shm_index;
			val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2));
			val &= (~BF_AMT_MASK);
			val |= (BF_AMT_BFM_ENABLED | (shm_idx << BF_AMT_BLK_IDX_SHIFT));
			wlc_write_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2), val);
			txbf_scb_info->exp_en = TRUE;
			WL_TXBF(("update amt idx %d %s for shm idx %d, %x\n", amt_idx,
				bcm_ether_ntoa(addr, eabuf), shm_idx, val));
			return;
		}
	}
}

static int
wlc_txbf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)hdl;
	wlc_info_t *wlc = txbf->wlc;

	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int32 *ret_int_ptr;
	int err = 0;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_uint_ptr = (uint32 *)a;
	ret_int_ptr = (int32 *)a;

	if (!TXBF_ENAB(wlc->pub))
		return BCME_UNSUPPORTED;

	switch (actionid) {
	case IOV_GVAL(IOV_TXBF_MODE):
		if (txbf->bfr_capable && txbf->mode)
			*ret_uint_ptr = txbf->mode;
		else
			*ret_uint_ptr = 0;
		break;

	case IOV_SVAL(IOV_TXBF_MODE):
			if (txbf->bfr_capable) {
			txbf->mode = (uint8) int_val;
			if (txbf->mode == 0) {
				txbf->active = FALSE;
				WL_TXBF(("%s:TxBF deactived\n", __FUNCTION__));
			} else if (txbf->mode && (WLC_BITSCNT(wlc->stf->txchain) > 1) &&
				txbf->bfr_shm_index_bitmap) {
				txbf->active = TRUE;
				WL_TXBF(("TxBF actived \n"));
			}

			wlc_txbf_upd(txbf);
			/* invalidate txcache since rates are changing */
			if (WLC_TXC_ENAB(wlc))
				wlc_txc_inv_all(wlc->txc);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_GVAL(IOV_TXBF_BFR_CAP):
		*ret_int_ptr = (int32)txbf->bfr_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFR_CAP):
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			txbf->bfr_capable = bool_val;
#ifdef WL11AC
			wlc_txbf_upd_bfr_bfe_cap(txbf);
#endif
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_TXBF_BFE_CAP):
		*ret_int_ptr = (int32)txbf->bfe_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFE_CAP):
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			txbf->bfe_capable = bool_val;
#ifdef WL11AC
			wlc_txbf_upd_bfr_bfe_cap(txbf);
#endif
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;


	case IOV_GVAL(IOV_TXBF_TIMER):
		if (txbf->sounding_period == 0)
			*ret_uint_ptr = (uint32) -1; /* -1 auto */
		else if (txbf->sounding_period == BF_SOUND_PERIOD_DISABLED)
			*ret_uint_ptr = 0; /* 0: disabled */
		else
			*ret_uint_ptr = (uint32)txbf->sounding_period * 4/ 1000;
		break;

	case IOV_SVAL(IOV_TXBF_TIMER):
		if (int_val == -1) /* -1 auto */
			txbf->sounding_period = 0;
		else if (int_val == 0) /* 0: disabled */
			txbf->sounding_period = BF_SOUND_PERIOD_DISABLED;
		else if ((int_val < BF_SOUND_PERIOD_MIN) || (int_val > BF_SOUND_PERIOD_MAX))
			return BCME_BADARG;
		else
			txbf->sounding_period = (uint16)int_val * (1000 / 4);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_REFRESH_THR_OFFSET) * 2,
			txbf->sounding_period);
		break;

	case IOV_GVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *ret_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(txbf->txbf_rate_mcs, ret_rs->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(txbf->txbf_rate_mcs_bcm, ret_rs->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			ret_rs->txbf_rate_vht[i] = txbf->txbf_rate_vht[i];
			ret_rs->txbf_rate_vht_bcm[i] = txbf->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		ret_rs->txbf_rate_ofdm_cnt = txbf->txbf_rate_ofdm_cnt;
		bcopy(txbf->txbf_rate_ofdm, ret_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		ret_rs->txbf_rate_ofdm_cnt_bcm = txbf->txbf_rate_ofdm_cnt_bcm;
		bcopy(txbf->txbf_rate_ofdm_bcm, ret_rs->txbf_rate_ofdm_bcm,
			txbf->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	case IOV_SVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *in_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(in_rs->txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(in_rs->txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			txbf->txbf_rate_vht[i] = in_rs->txbf_rate_vht[i];
			txbf->txbf_rate_vht_bcm[i] = in_rs->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		txbf->txbf_rate_ofdm_cnt = in_rs->txbf_rate_ofdm_cnt;
		bcopy(in_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm, in_rs->txbf_rate_ofdm_cnt);
		txbf->txbf_rate_ofdm_cnt_bcm = in_rs->txbf_rate_ofdm_cnt_bcm;
		bcopy(in_rs->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
			in_rs->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	case IOV_GVAL(IOV_TXBF_VIRTIF_DISABLE):
		*ret_int_ptr = (int32)txbf->virtif_disable;
		break;

	case IOV_SVAL(IOV_TXBF_VIRTIF_DISABLE):
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			txbf->virtif_disable = bool_val;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

#if defined(WLTEST) || defined(WLPKTENG)
	case IOV_GVAL(IOV_TXBF_BFE_NRX_OV):
		*ret_int_ptr = (int32)txbf->bfe_nrx_ov;
		break;

	case IOV_SVAL(IOV_TXBF_BFE_NRX_OV):
		if ((int_val < 0) || (int_val > 3))
			return BCME_BADARG;
		else
			txbf->bfe_nrx_ov = (uint8)int_val;
		break;
#endif
	case IOV_GVAL(IOV_TXBF_IMP):
		*ret_uint_ptr = txbf->imp;
		break;

	case IOV_SVAL(IOV_TXBF_IMP):
		{
			uint8 imp = (uint8) int_val & 0x3;
			txbf->imp_nochk = (int_val & 0x4) ? TRUE : FALSE;
			if (txbf->imp != imp) {
				txbf->imp = imp;
				/* need to call to update imp_en in all scb */
				wlc_txbf_impbf_updall(txbf);
			}
		}
		break;
	default:
		WL_ERROR(("wl%d %s %x not supported\n", wlc->pub->unit, __FUNCTION__, actionid));
		return BCME_UNSUPPORTED;
	}
	return err;
}

void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, struct scb *scb)
{

	uint16 val;
	wlc_info_t * wlc;
	struct txbf_scb_info *txbf_scb_info;

	wlc = txbf->wlc;
	ASSERT(wlc);
	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}

	wlc_suspend_mac_and_wait(wlc);
	/* borrow shm block 0 for pkteng */
	val = wlc_read_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2);
	/* fake valid bit */
	val |= (1 << 8);
	/* use highest bw */
	val |= (3 << 12);
	wlc_write_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2, val);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_REFRESH_THR_OFFSET) * 2, -1);
	wlc_enable_mac(wlc);

	txbf_scb_info->shm_index = 0;
	txbf_scb_info->exp_en = TRUE;

	if (!txbf->active)
		txbf->active  = 1;

}

void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, struct scb *scb)
{
	wlc_info_t * wlc;
	uint16 val;
	struct txbf_scb_info *txbf_scb_info;

	wlc = txbf->wlc;
	ASSERT(wlc);

	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}
	txbf_scb_info->exp_en = FALSE;

	/* clear the valid bit */
	wlc_suspend_mac_and_wait(wlc);
	val = wlc_read_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2);
	val &= (~(1 << 8));
	wlc_write_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2, val);
	wlc_enable_mac(wlc);

	if ((txbf->bfr_shm_index_bitmap == 0) && txbf->active)
		txbf->active  = 0;
}

void
wlc_txbf_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;

	wlc = txbf->wlc;
	ASSERT(wlc);

	wlc_vht_upd_txbf_cap(wlc->vhti, txbf->bfr_capable, txbf->bfe_capable);
}

void
wlc_txbf_upd_virtif_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap)
{
	wlc_info_t *wlc;
	bool bfr, bfe;

	wlc = txbf->wlc;
	if (wlc->txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg)))
		bfr = bfe = FALSE;
	else {
		bfr = txbf->bfr_capable;
		bfe = txbf->bfe_capable;
	}
	wlc_vht_upd_txbf_virtif_cap(wlc->vhti, bfr, bfe, cap);
}


void wlc_txbf_update_ht_cap(wlc_txbf_info_t *txbf, struct scb *scb, ht_cap_ie_t *cap_ie)
{
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(scb);
	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}
	txbf_scb_info->ht_txbf_cap = ltoh32_ua(&cap_ie->txbf_cap);

	return;
}
#endif /*  WL_BEAMFORMING */
