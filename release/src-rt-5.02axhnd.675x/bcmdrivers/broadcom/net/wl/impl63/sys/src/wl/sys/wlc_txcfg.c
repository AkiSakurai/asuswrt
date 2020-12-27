/*
 * tx config module for Broadcom 802.11 Networking Adapter Device Drivers
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
 * $Id: wlc_txcfg.c $
 */

/* This module manages configuration for different transmission technology.
 * This is primarily used for configuring maximum number of users for a given MU
 * technology, it can be extended for any other configuration eg: WRR, FIFOs, AC etc
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

#define WLC_TXTYPE_MAX		5

/* Configuration table for max users that can be admitted per Tx technology (Max Users) */
static const int maxusers_table[][8] = {
	/* Max Users config table :
	* D11 Rev:  SU: VHT-MUMIMO: HE-MUMIMO: DLOFDMA: ULOFDMA: TWT: HW limit
	*/
	{64, MAXSCB, 8, 0, 0, 0, 0, 8},	/* 4365/66 <=B1 */
	{65, MAXSCB, 8, 0, 0, 0, 0, 8},	/* 4365/66 C0 */
	{129, MAXSCB, 8, 8, MAXSCB, 8, 16, 16},	/* 43684B/Cx */
	{130, MAXSCB, 2, 2, MAXSCB, 4, 4, 8},	/* 63178, 675x (retail/operator) */
	{131, MAXSCB, 3, 3, MAXSCB, 4, 4, 4},	/* 6710 */
	{132, MAXSCB, 8, 4, MAXSCB, 8, 16, 12},	/* 6715A0 */
	{0, MAXSCB, 0, 0, 0, 0, 0, 0}	/* default */
};

/* macros for indexes into maxusers table */
#define MAXUSERS_TBL_D11REV_IDX		0	/* idx of D11 Rev */
#define MAXUSERS_TBL_TXTYPE_START_IDX	1	/* start idx of max users for different tx tech */
#define MAXUSERS_TBL_HWLIMIT_IDX	7	/* idx of HW limit for max users (VHT,HE)  */

/* Configuration table for maximum users per PPDU (MaxN) */
static const int maxn_table[][10] = {
	/* Max Users per PPDU (MaxN) config table : D11 Rev:  DL-OFDMA(BW20): DL-OFDMA(BW40):
	 *  DL-OFDMA(BW80): DL-OFDMA(BW160):
	 *  UL-OFDMA(BW20): UL-OFDMA(BW40): UL-OFDMA(BW80): UL-OFDMA(BW160): MaxN (HW limit)
	 */
	{64, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* 4365/66 <=B1 */
	{65, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* 4365/66 C0 */
	{128, 4, 4, 8, 8, 4, 4, 8, 8, 16}, /* 43684A0 */
	{129, 4, 4, 4, 8, 4, 4, 8, 8, 16}, /* 43684B/Cx */
	{130, 4, 4, 4, 4, 4, 4, 4, 4, 4}, /* 63178, 675x */
	{131, 4, 4, 4, 4, 4, 4, 4, 4, 8}, /* 6710 */
	{132, 4, 4, 8, 8, 4, 4, 8, 8, 16}, /* 6715A0 */
	{0, 0, 0, 0, 0, 0, 0, 0, 0} /* default */
};

/* macros for indexes into maxn table */
#define MAXN_TBL_D11REV_IDX		0	/* idx of D11 Rev */
#define MAXN_TBL_DL_OFDMA_START_IDX	1	/* start idx of per-BW maxn for DL-OFDMA */
#define MAXN_TBL_UL_OFDMA_START_IDX	5	/* start idx of per-BW maxn for UL-OFDMA */
#define MAXN_TBL_HW_LIMIT_IDX		9	/* idx of HW limit of maxn */

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

	/* parse the max users table to locate the config */
	/* Table format # D11 Rev:  SU: VHT-MUMIMO: HE-MUMIMO: DLOFDMA: ULOFDMA */
	while (TRUE) {
		if (D11REV_IS(txcfg->pub->corerev, maxusers_table[i][MAXUSERS_TBL_D11REV_IDX]) ||
			!maxusers_table[i][MAXUSERS_TBL_D11REV_IDX]) {

			/* assert if no entry has been added yet to the max
			* users table for this NEW chip
			*/
			if ((maxusers_table[i][MAXUSERS_TBL_D11REV_IDX] == 0) &&
				D11REV_GT(txcfg->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip max users config\n",
					txcfg->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			txcfg->max_clients[SU] =
				maxusers_table[i][MAXUSERS_TBL_TXTYPE_START_IDX + SU];
			txcfg->max_clients[VHTMU] =
				maxusers_table[i][MAXUSERS_TBL_TXTYPE_START_IDX + VHTMU];
			txcfg->max_clients[HEMMU] =
				HE_MMU_ENAB(txcfg->wlc->pub) ?
				maxusers_table[i][MAXUSERS_TBL_TXTYPE_START_IDX + HEMMU] : 0;
			txcfg->max_clients[DLOFDMA] =
				maxusers_table[i][MAXUSERS_TBL_TXTYPE_START_IDX + DLOFDMA];
			txcfg->max_clients[ULOFDMA] =
				maxusers_table[i][MAXUSERS_TBL_TXTYPE_START_IDX + ULOFDMA];

			txcfg->maxusers_table_idx = i;

			return;
		}
		i++;

		/* save the latest d11rev entry found in the table */
		if (maxusers_table[i][MAXUSERS_TBL_D11REV_IDX] > corerev)
			corerev = maxusers_table[i][MAXUSERS_TBL_D11REV_IDX];
	}
}

int wlc_txcfg_max_clients_set(wlc_txcfg_info_t *txcfg, uint8 type, uint16 val)
{
	int err = BCME_OK;
	uint8 idx = txcfg->maxusers_table_idx;
	uint32 hwlimit = maxusers_table[idx][MAXUSERS_TBL_HWLIMIT_IDX];

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

	case VHTMU:
		if ((val && val < VHTMU_USRCNT_MIN) ||
			val > hwlimit ||
			(val + txcfg->max_clients[HEMMU] > hwlimit)) {
			err = BCME_RANGE;
		}
		break;

	case HEMMU:
		if ((val && val < HEMU_USRCNT_MIN) ||
			val > hwlimit ||
			(txcfg->max_clients[VHTMU] + val > hwlimit)) {
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

	if (type == VHTMU && val != txcfg->max_clients[VHTMU]) {
#ifdef WL_BEAMFORMING
		if ((err = wlc_txbf_mu_max_links_upd(txcfg->wlc->txbf, (uint8)val)) != BCME_OK)
			goto done;
#endif // endif
	}

	txcfg->max_clients[type] = val;
	WL_MUTX(("%s:%d %d %d\n", __FUNCTION__, __LINE__, type, val));

done:
	return err;
}

uint16 wlc_txcfg_max_clients_get(wlc_txcfg_info_t *txcfg, uint8 type)
{
	if (!txcfg) {
		return 0;
	}
	return txcfg->max_clients[type];
}

uint16 wlc_txcfg_max_mmu_clients_get(wlc_txcfg_info_t *txcfg)
{
	uint16 max_mmu_usrs;

	if (!txcfg) {
		return 0;
	}

	max_mmu_usrs = txcfg->max_clients[VHTMU];
	max_mmu_usrs += txcfg->max_clients[HEMMU];

	return max_mmu_usrs;
}

int wlc_txcfg_iov_max_clients_set(wlc_txcfg_info_t *txcfg, max_clients_t *cfg_in)
{
	int err = BCME_OK;

	ASSERT(txcfg);

	if (((err = wlc_txcfg_max_clients_set(txcfg, SU, (uint8)cfg_in->su)) != BCME_OK) ||
		((err = wlc_txcfg_max_clients_set(txcfg, VHTMU, (uint8)cfg_in->vhtmu)) != BCME_OK))
		goto done;

	if (D11REV_LT(txcfg->wlc->pub->corerev, 128))
		goto done;

	if (((err = wlc_txcfg_max_clients_set(txcfg, HEMMU, cfg_in->hemmu)) != BCME_OK) ||
		((err = wlc_txcfg_max_clients_set(txcfg, DLOFDMA, cfg_in->dlofdma)) != BCME_OK) ||
		((err = wlc_txcfg_max_clients_set(txcfg, ULOFDMA, cfg_in->ulofdma)) != BCME_OK))
		goto done;

	WL_MUTX(("%s:%d %d %d %d %d %d\n", __FUNCTION__, __LINE__,
		cfg_in->su, cfg_in->vhtmu, cfg_in->hemmu, cfg_in->dlofdma, cfg_in->ulofdma));

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
	cfg_out->vhtmu = txcfg->max_clients[VHTMU];

	if (D11REV_LT(txcfg->wlc->pub->corerev, 128))
		goto done;

	cfg_out->hemmu = txcfg->max_clients[HEMMU];
	cfg_out->dlofdma = txcfg->max_clients[DLOFDMA];
	cfg_out->ulofdma = txcfg->max_clients[ULOFDMA];

	WL_MUTX(("%s:%d %d %d %d %d %d\n", __FUNCTION__, __LINE__,
		cfg_out->su, cfg_out->vhtmu, cfg_out->hemmu, cfg_out->dlofdma, cfg_out->ulofdma));

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

	/* parse the max users per PPDU (MaxN) table to locate the DL-OFDMA config */
	/* Table format : D11 Rev:  DL-OFDMA(BW20): DL-OFDMA(BW40): DL-OFDMA(BW80):
	 *  DL-OFDMA(BW160): UL-OFDMA(BW20): UL-OFDMA(BW40): UL-OFDMA(BW80): UL-OFDMA(BW160)
	 */
	while (TRUE) {
		if (D11REV_IS(wlc->pub->corerev, maxn_table[i][MAXN_TBL_D11REV_IDX]) ||
			!maxn_table[i][MAXN_TBL_D11REV_IDX]) {

			/* assert if no entry has been added yet to the maxn
			* table for this NEW chip
			*/
			if ((maxn_table[i][MAXN_TBL_D11REV_IDX] == 0) &&
				D11REV_GT(wlc->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip DLOFDMA MaxN  config\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			maxn[D11_REV128_BW_20MHZ] =
				maxn_table[i][MAXN_TBL_DL_OFDMA_START_IDX + D11_REV128_BW_20MHZ];
			maxn[D11_REV128_BW_40MHZ] =
				maxn_table[i][MAXN_TBL_DL_OFDMA_START_IDX + D11_REV128_BW_40MHZ];
			maxn[D11_REV128_BW_80MHZ] =
				maxn_table[i][MAXN_TBL_DL_OFDMA_START_IDX + D11_REV128_BW_80MHZ];
			maxn[D11_REV128_BW_160MHZ] =
				maxn_table[i][MAXN_TBL_DL_OFDMA_START_IDX + D11_REV128_BW_160MHZ];

			txcfg->maxn_table_idx = i;
			return;
		}
		i++;

		/* save the latest d11rev entry found in the table */
		if (maxn_table[i][MAXN_TBL_D11REV_IDX] > corerev)
			corerev = maxn_table[i][MAXN_TBL_D11REV_IDX];
	}
}

void
wlc_txcfg_ulofdma_maxn_init(wlc_info_t *wlc, uint16 *maxn)
{
	uint8 i = 0;
	wlc_txcfg_info_t *txcfg = wlc->txcfg;
	int corerev = 0;

	/* parse the max users per PPDU (MaxN) table to locate the UL-OFDMA config */
	/* Table format : D11 Rev:  DL-OFDMA(BW20): DL-OFDMA(BW40): DL-OFDMA(BW80): DL-OFDMA(BW160):
	 *  UL-OFDMA(BW20): UL-OFDMA(BW40): UL-OFDMA(BW80): UL-OFDMA(BW160)
	 */
	while (maxn_table[i][MAXN_TBL_D11REV_IDX]) {
		if (D11REV_IS(wlc->pub->corerev, maxn_table[i][MAXN_TBL_D11REV_IDX])) {

			/* assert if no entry has been added yet to the maxn
			* table for this NEW chip
			*/
			if ((maxn_table[i][MAXN_TBL_D11REV_IDX] == 0) &&
				D11REV_GT(wlc->pub->corerev, corerev)) {
				WL_ERROR(("wl%d: %s: unable to locate chip ULOFDMA MaxN  config\n",
					wlc->pub->unit, __FUNCTION__));
				ASSERT(0);
			}

			/* found an entry */
			maxn[D11_REV128_BW_20MHZ] =
				maxn_table[i][MAXN_TBL_UL_OFDMA_START_IDX + D11_REV128_BW_20MHZ];
			maxn[D11_REV128_BW_40MHZ] =
				maxn_table[i][MAXN_TBL_UL_OFDMA_START_IDX + D11_REV128_BW_40MHZ];
			maxn[D11_REV128_BW_80MHZ] =
				maxn_table[i][MAXN_TBL_UL_OFDMA_START_IDX + D11_REV128_BW_80MHZ];
			maxn[D11_REV128_BW_160MHZ] =
				maxn_table[i][MAXN_TBL_UL_OFDMA_START_IDX + D11_REV128_BW_160MHZ];

			txcfg->maxn_table_idx = i;
			return;
		}
		i++;

		/* save the latest d11rev entry found in the table */
		if (maxn_table[i][MAXN_TBL_D11REV_IDX] > corerev)
			corerev = maxn_table[i][MAXN_TBL_D11REV_IDX];
	}
}

uint16
wlc_txcfg_ofdma_maxn_upperbound(wlc_info_t *wlc, uint16 bwidx)
{
	uint8 tblidx = wlc->txcfg->maxn_table_idx;
	return MIN(maxn_table[tblidx][MAXN_TBL_HW_LIMIT_IDX], dfs_maxn_limit[bwidx]);
}
