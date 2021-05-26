/*
 * tx config module for Broadcom 802.11 Networking Adapter Device Drivers
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
 * $Id: wlc_txcfg.c $
 */

/* This module manages configuration for different transmission technology.
 * This is primarily used for configuring maximum number of users for a given MU
 * technology, it can be extended for any other configuration eg: FIFOs, AC etc
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wlc_types.h>
#include <siutils.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wl_dbg.h>
#include <wlc_pcb.h>
#include <wlc_scb.h>
#include <wlc_bmac.h>
#include <wlc_tx.h>
#include <wl_export.h>
#include <wlc_scb_ratesel.h>
#include <wlc_dump.h>
#include <wlc_stf.h>
#include <wlc_test.h>
#include <phy_api.h>
#include <wlc_ratelinkmem.h>
#include <wlc_txcfg.h>
#include <wlc_ulmu.h>
#include <wlc_mutx.h>
#include <wlc_txbf.h>

/** The maximum amount of users for one specific d11 core rev */
typedef struct maxusers_s {
	int d11majrev;
	int low_d11minrev;
	int high_d11minrev;
	int maxusers_su;
	int maxusers_dl_mumimo;
	int maxusers_dl_ofdma;
	int maxusers_ul_ofdma;
	int twt;
} maxusers_t;

/** The maximum amount of users-per-ppdu (maxN) for one specific d11 core rev */
typedef struct maxusers_per_pddu_s {
	int d11majrev;
	int low_d11minrev;
	int high_d11minrev;
	int dl_ofdma_bw20;
	int dl_ofdma_bw40;
	int dl_ofdma_bw80;
	int dl_ofdma_bw160;
	int ul_ofdma_bw20;
	int ul_ofdma_bw40;
	int ul_ofdma_bw80;
	int ul_ofdma_bw160;
	int hw_limit;
} maxusers_per_pddu_t;

/*
 * Configuration table for max users that can be admitted per Tx technology (Max Users).
 * D11REV  Chip
 * 64      4365/66 <=B1
 * 65      4365/66 C0
 * 129     43684B/Cx
 * 130     enterprise - EAP: 47622, retail/operator: 63178, 675x
 * 130.2   6756 (160MHz)
 * 131     6710
 * 132     6715A0
 */
static const maxusers_t maxusers_table[] = {
	/*
	 * D11  Low    High   SU      DL-    DL-     UL-
	 * Rev  minrev minrev         MUMIMO OFDMA   OFDMA TWT
	 */
	{64,    0,     31,    MAXSCB, 8,      0,      0,    0},
	{65,    0,     31,    MAXSCB, 8,      0,      0,    0},
	{129,   0,     31,    MAXSCB, 16,     MAXSCB, 8,    16},
	{130,   0,     31,    MAXSCB, 8,      MAXSCB, 8,    4},
	{131,   0,     31,    MAXSCB, 12,     MAXSCB, 8,    4},
	{132,   0,     31,    MAXSCB, 16,     MAXSCB, 8,    16},
	{0,     0,     31,    MAXSCB, 0,      0,      0,    0}
};

/* Configuration table for maximum users per PPDU (MaxN) */
static const maxusers_per_pddu_t maxn_table[] = {
	/*
	 * D11  Low    High   --DL-OFDMA-- --UL-OFDMA-- HW
	 * Rev  minrev minrev 20 40 80 160 20 40 80 160 limit
	 */
	{64,    0,     31,    0, 0, 0, 0,  0, 0, 0, 0,  0},  /* 4365/66 <=B1 */
	{65,    0,     31,    0, 0, 0, 0,  0, 0, 0, 0,  0},  /* 4365/66 C0 */
	{129,   0,     31,    4, 4, 4, 8,  4, 4, 4, 4,  16}, /* 43684B/Cx */
	{130,   0,     1,     4, 4, 4, 0,  4, 4, 4, 0,  4},  /* 63178, 675x */
	{130,   2,     31,   4, 4, 4, 4,  4, 4, 4, 4,  4},  /* 6756 (160Mhz) */
	{131,   0,     31,   4, 4, 4, 0,  4, 4, 4, 0,  8},  /* 6710 */
	{132,   0,     31,   4, 4, 4, 8,  4, 4, 4, 8,  16}, /* 6715A0 */
	{0,     0,     31,   0, 0, 0, 0,  0, 0, 0, 0,  0}   /* default */
};

/* per-BW DFS channel MaxN limit BW20:BW40:BW80:BW160 */
static const int dfs_maxn_limit[D11_REV128_BW_SZ] = {4, 8, 16, 16};

enum txcfg_dump {
	TXCFG_DUMP_NONE   = 0,
	TXCFG_DUMP_SIMPLE = 1,
	TXCFG_DUMP_DETAIL = 2
};

/* iovar table */
enum wlc_txcfg_iov {
	IOV_MAX_MUCLIENTS = 1,
	IOV_TXCFG_LAST
};

static const bcm_iovar_t txcfg_iovars[] = {
	{"max_muclients", IOV_MAX_MUCLIENTS,
	(IOVF_SET_DOWN), 0, IOVT_BUFFER, 0
	},
	{NULL, 0, 0, 0, 0, 0 }
};

/* State structure for the txcfg module created by
 * wlc_txcfg_module_attach().
 */
struct wlc_txcfg_info {
	osl_t *osh;              /* OSL handle */
	wlc_info_t *wlc;         /* wlc_info_t handle */
	wlc_pub_t *pub;          /* wlc_pub_t handle */

	uint16 max_clients[WLC_TXTYPE_MAX];
	uint8 maxusers_table_idx; /* idx into the maxusers_table */
	uint8 maxn_table_idx; /* idx into the maxn_table */
};

/* Basic module infrastructure */
static void wlc_txcfg_max_clients_init(wlc_txcfg_info_t *txcfg);
static int wlc_txcfg_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
static int wlc_txcfg_dump(void *ctx, struct bcmstrbuf *b);
static int wlc_txcfg_dump_clr(void *ctx);
#endif // endif
static int wlc_txcfg_up(void *mi);
static int wlc_txcfg_iov_max_clients_set(wlc_txcfg_info_t *txcfg, max_clients_t *cfg_in);
static int wlc_txcfg_iov_max_clients_get(wlc_txcfg_info_t *txcfg, max_clients_t *cfg_out);

/* SCB cubby management */
static void wlc_txcfg_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);

/* Module Attach/Detach */
/*
 * Create the tx config infrastructure for different technology for the wl driver.
 * wlc_module_register() is called to register the module's
 * handlers. The dump function is also registered. Handlers are only
 * registered if the phy is MU BFR capable and if MU TX is not disabled
 * in the build.
 *
 * Returns
 *     A wlc_txcfg_info_t structure, or NULL in case of failure.
 */
wlc_txcfg_info_t*
BCMATTACHFN(wlc_txcfg_attach)(wlc_info_t *wlc)
{
	wlc_txcfg_info_t *txcfg_info;
	int err;

	/* allocate the main state structure */
	txcfg_info = MALLOCZ(wlc->osh, sizeof(wlc_txcfg_info_t));
	if (txcfg_info == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));

		return NULL;
	}

	txcfg_info->wlc = wlc;
	txcfg_info->pub = wlc->pub;
	txcfg_info->osh = wlc->osh;

	err = wlc_module_register(txcfg_info->pub, txcfg_iovars, "txcfg", txcfg_info,
	                          wlc_txcfg_doiovar, NULL, wlc_txcfg_up, NULL);

	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed with error %d (%s).\n",
		          wlc->pub->unit, __FUNCTION__, err, bcmerrorstr(err)));

		/* use detach as a common failure deallocation */
		wlc_txcfg_detach(txcfg_info);
		return NULL;
	}

	/* Add client callback to the scb state notification list */
	if ((err = wlc_scb_state_upd_register(wlc, wlc_txcfg_scb_state_upd, txcfg_info))
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
		          wlc->pub->unit, __FUNCTION__, wlc_txcfg_scb_state_upd));

		wlc_txcfg_detach(txcfg_info);
		return NULL;
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
	wlc_dump_add_fns(txcfg_info->pub, "txcfg", wlc_txcfg_dump, wlc_txcfg_dump_clr, txcfg_info);
#endif // endif
	wlc_txcfg_max_clients_init(txcfg_info);

	return txcfg_info;
}

/* Free all resources associated with the tx config module
 * infrastructure. This is done at the cleanup stage when
 * freeing the driver.
 *
 * txcfg_info    txcfg module state structure
 */
void
BCMATTACHFN(wlc_txcfg_detach)(wlc_txcfg_info_t *txcfg_info)
{
	if (txcfg_info == NULL) {
		return;
	}

	wlc_scb_state_upd_unregister(txcfg_info->wlc, wlc_txcfg_scb_state_upd, txcfg_info);

	wlc_module_unregister(txcfg_info->pub, "txcfg", txcfg_info);

	MFREE(txcfg_info->osh, txcfg_info, sizeof(wlc_txcfg_info_t));
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
/* Dump tx config state information. */
static int
wlc_txcfg_info_dump(wlc_txcfg_info_t *txcfg_info, struct bcmstrbuf *b, uint8 option)
{
	BCM_REFERENCE(txcfg_info);

	if (txcfg_info == NULL) {
		return BCME_OK;
	}

	if (!txcfg_info->wlc) {
		bcm_bprintf(b, "wlc pointer is NULL on txcfg_info\n");
		return BCME_OK;
	}

	return BCME_OK;
}

/* Dump tx config state information. */
static int
wlc_txcfg_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_txcfg_info_t *txcfg_info = ctx;
	return wlc_txcfg_info_dump(txcfg_info, b, TXCFG_DUMP_DETAIL);
}

static int
wlc_txcfg_dump_clr(void *ctx)
{
	wlc_txcfg_info_t *txcfg_info = ctx;
	BCM_REFERENCE(txcfg_info);

	return BCME_OK;
}
#endif // endif

/* IOVar handler for the tx config infrastructure module */
static int
wlc_txcfg_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_txcfg_info_t *txcfg_info = (wlc_txcfg_info_t*) hdl;
	int32 int_val = 0;
	int err = 0;

	if (plen >= (int)sizeof(int_val))
		memcpy(&int_val, p, sizeof(int_val));

	switch (actionid) {

	case IOV_GVAL(IOV_MAX_MUCLIENTS): {
		max_clients_t *m = (max_clients_t *)a;
		m->version = WL_MU_CLIENTS_PARAMS_VERSION;
		m->length = sizeof(max_clients_t);

		wlc_txcfg_iov_max_clients_get(txcfg_info, m);
		break;
	}

	case IOV_SVAL(IOV_MAX_MUCLIENTS): {
		max_clients_t *m = (max_clients_t *)a;

		if (m->version != WL_MU_CLIENTS_PARAMS_VERSION) {
			err = BCME_BADARG;
			break;
		}

		err = wlc_txcfg_iov_max_clients_set(txcfg_info, m);
		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void wlc_txcfg_max_clients_init(wlc_txcfg_info_t *txcfg)
{
	uint8 i = 0;
	int corerev = 0;
	const maxusers_t *row = &maxusers_table[0];

	/* parse the max users table to locate the config */
	/* Table format # D11 Rev:  SU: DL-MUMIMO: DLOFDMA: ULOFDMA */
	while (TRUE) {
		if ((D11REV_IS(txcfg->pub->corerev, row->d11majrev) &&
		     D11MINORREV_GE(txcfg->pub->corerev_minor, row->low_d11minrev) &&
		     D11MINORREV_LE(txcfg->pub->corerev_minor, row->high_d11minrev)) ||
		     row->d11majrev == 0) {
			/* assert if no entry has been added yet to the max
			* users table for this NEW chip
			*/
			if (row->d11majrev == 0 && D11REV_GT(txcfg->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip max users config\n",
					txcfg->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			txcfg->max_clients[SU] = row->maxusers_su;
			txcfg->max_clients[DLMMU] = row->maxusers_dl_mumimo;
			txcfg->max_clients[DLOFDMA] = row->maxusers_dl_ofdma;
			txcfg->max_clients[ULOFDMA] = row->maxusers_ul_ofdma;
			txcfg->maxusers_table_idx = i;

			return;
		}

		i++;
		row++;
		/* save the latest d11rev entry found in the table */
		if (row->d11majrev > corerev) {
			corerev = row->d11majrev;
		}
	}
}

int wlc_txcfg_max_clients_set(wlc_txcfg_info_t *txcfg, uint8 type, uint16 val)
{
	int err = BCME_OK;

	if (!txcfg) {
		err = BCME_BADARG;
		goto done;
	}
	switch (type) {
	case SU:
		if (val > MAXSCB) {
			err = BCME_RANGE;
		}
		break;

	case DLMMU:
		if ((val && val < MUCLIENT_NUM_MIN) ||
			(val > MUCLIENT_NUM)) {
			err = BCME_RANGE;
		}
		break;
	case DLOFDMA:
		if (val > MAXSCB) {
			err = BCME_RANGE;
		}
		break;

	case ULOFDMA:
		if (val > ULMU_USRCNT_MAX)
			err = BCME_RANGE;

		break;

	default:
		err = BCME_ERROR;
	}

	if (err != BCME_OK)
		goto done;

	if (D11REV_LE(txcfg->wlc->pub->corerev, 128) &&
		type == DLMMU && val != txcfg->max_clients[DLMMU]) {
#ifdef WL_BEAMFORMING
		if ((err = wlc_txbf_mu_max_links_upd(txcfg->wlc->txbf, (uint8)val)) != BCME_OK)
			goto done;
#endif // endif
	}

	txcfg->max_clients[type] = val;

done:
	return err;
}

uint16 wlc_txcfg_max_clients_get(wlc_txcfg_info_t *txcfg, uint8 type)
{
	if (!txcfg) {
		return 0;
	}
	if ((type == VHTMU) || (type == HEMMU))
		type = DLMMU;
	return txcfg->max_clients[type];
}

int wlc_txcfg_iov_max_clients_set(wlc_txcfg_info_t *txcfg, max_clients_t *cfg_in)
{
	int err = BCME_OK;

	ASSERT(txcfg);

	if (((err = wlc_txcfg_max_clients_set(txcfg, SU, (uint8)cfg_in->su)) != BCME_OK) ||
		((err = wlc_txcfg_max_clients_set(txcfg, DLMMU, (uint8)cfg_in->dlmmu)) != BCME_OK))
		goto done;

	if (D11REV_LT(txcfg->wlc->pub->corerev, 128))
		goto done;

	if (((err = wlc_txcfg_max_clients_set(txcfg, DLOFDMA, cfg_in->dlofdma)) != BCME_OK) ||
		((err = wlc_txcfg_max_clients_set(txcfg, ULOFDMA, cfg_in->ulofdma)) != BCME_OK))
		goto done;

	WL_MUTX(("%s:%d %d %d %d %d\n", __FUNCTION__, __LINE__,
		cfg_in->su, cfg_in->dlmmu, cfg_in->dlofdma, cfg_in->ulofdma));

done:
	return err;
}

int wlc_txcfg_iov_max_clients_get(wlc_txcfg_info_t *txcfg, max_clients_t *cfg_out)
{
	int err = BCME_OK;

	if (!txcfg) {
		err = BCME_BADARG;
		goto done;
	}

	cfg_out->su = txcfg->max_clients[SU];
	cfg_out->dlmmu = txcfg->max_clients[DLMMU];

	if (D11REV_LT(txcfg->wlc->pub->corerev, 128))
		goto done;

	cfg_out->dlofdma = txcfg->max_clients[DLOFDMA];
	cfg_out->ulofdma = txcfg->max_clients[ULOFDMA];

	WL_MUTX(("%s:%d %d %d %d %d\n", __FUNCTION__, __LINE__,
		cfg_out->su, cfg_out->dlmmu, cfg_out->dlofdma, cfg_out->ulofdma));

done:
	return err;
}

/* Registered callback when radio comes up. */
static int
wlc_txcfg_up(void *mi)
{
	wlc_txcfg_info_t *txcfg_info = (wlc_txcfg_info_t*) mi;
	wlc_info_t *wlc = txcfg_info->wlc;

	BCM_REFERENCE(txcfg_info);
	BCM_REFERENCE(wlc);

	return BCME_OK;
}

/* Callback function invoked when a STA's association state changes.
 * Inputs:
 *   ctx -        txcfg state structure
 *   notif_data - information describing the state change
 */
static void
wlc_txcfg_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	return;
}

void
wlc_txcfg_dlofdma_maxn_init(wlc_info_t *wlc, uint8 *maxn)
{
	uint8 i = 0;
	wlc_txcfg_info_t *txcfg = wlc->txcfg;
	int corerev = 0;
	const maxusers_per_pddu_t *row = &maxn_table[0];

	/* parse the max users per PPDU (MaxN) table to locate the DL-OFDMA config */
	/* Table format : D11 Rev:  DL-OFDMA(BW20): DL-OFDMA(BW40): DL-OFDMA(BW80):
	 *  DL-OFDMA(BW160): UL-OFDMA(BW20): UL-OFDMA(BW40): UL-OFDMA(BW80): UL-OFDMA(BW160)
	 */
	while (TRUE) {
		if ((D11REV_IS(wlc->pub->corerev, row->d11majrev) &&
		     D11MINORREV_GE(txcfg->pub->corerev_minor, row->low_d11minrev) &&
		     D11MINORREV_LE(txcfg->pub->corerev_minor, row->high_d11minrev)) ||
		    row->d11majrev == 0) {
			/* assert if no entry has been added yet to the maxn
			* table for this NEW chip
			*/
			if (row->d11majrev == 0 && D11REV_GT(wlc->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip DLOFDMA MaxN  config\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			maxn[D11_REV128_BW_20MHZ] = row->dl_ofdma_bw20;
			maxn[D11_REV128_BW_40MHZ] = row->dl_ofdma_bw40;
			maxn[D11_REV128_BW_80MHZ] = row->dl_ofdma_bw80;
			maxn[D11_REV128_BW_160MHZ] = row->dl_ofdma_bw160;
			txcfg->maxn_table_idx = i;

			return;
		}

		i++;
		row++;

		/* save the latest d11rev entry found in the table */
		if (row->d11majrev > corerev)
			corerev = row->d11majrev;
	}
}

void
wlc_txcfg_ulofdma_maxn_init(wlc_info_t *wlc, uint8 *maxn)
{
	uint8 i = 0;
	wlc_txcfg_info_t *txcfg = wlc->txcfg;
	int corerev = 0;
	const maxusers_per_pddu_t *row = &maxn_table[0];

	/* parse the max users per PPDU (MaxN) table to locate the UL-OFDMA config */
	/* Table format : D11 Rev:  DL-OFDMA(BW20): DL-OFDMA(BW40): DL-OFDMA(BW80): DL-OFDMA(BW160):
	 *  UL-OFDMA(BW20): UL-OFDMA(BW40): UL-OFDMA(BW80): UL-OFDMA(BW160)
	 */
	while (row->d11majrev != 0) {
		if (D11REV_IS(wlc->pub->corerev, row->d11majrev) &&
		    D11MINORREV_GE(txcfg->pub->corerev_minor, row->low_d11minrev) &&
		    D11MINORREV_LE(txcfg->pub->corerev_minor, row->high_d11minrev)) {
			/* assert if no entry has been added yet to the maxn
			* table for this NEW chip
			*/
			if (row->d11majrev == 0 && D11REV_GT(wlc->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip ULOFDMA MaxN  config\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			maxn[D11_REV128_BW_20MHZ] = row->ul_ofdma_bw20;
			maxn[D11_REV128_BW_40MHZ] = row->ul_ofdma_bw40;
			maxn[D11_REV128_BW_80MHZ] = row->ul_ofdma_bw80;
			maxn[D11_REV128_BW_160MHZ] = row->ul_ofdma_bw160;

			txcfg->maxn_table_idx = i;
			return;
		}

		i++;
		row++;

		/* save the latest d11rev entry found in the table */
		if (row->d11majrev > corerev)
			corerev = row->d11majrev;
	}
}

uint16
wlc_txcfg_ofdma_maxn_upperbound(wlc_info_t *wlc, uint16 bwidx)
{
	uint8 tblidx = wlc->txcfg->maxn_table_idx;

	/* BW160 is not supported on 63178/675x (minorrev <= 2) and 6710 */
	if ((D11REV_IS(wlc->pub->corerev, 130) && D11MINORREV_LE(wlc->pub->corerev_minor, 2)) ||
		D11REV_IS(wlc->pub->corerev, 131)) {
		if (bwidx == D11_REV128_BW_160MHZ) {
			return 0;
		}
	}

	return MIN(maxn_table[tblidx].hw_limit, dfs_maxn_limit[bwidx]);
}
