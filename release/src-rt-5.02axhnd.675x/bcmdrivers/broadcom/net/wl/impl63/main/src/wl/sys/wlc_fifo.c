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
 * $Id: wlc_fifo.c $
 */

/* This module manages configuration for different transmission technology.
 * This is primarily used for configuring maximum number of users for a given MU
 * technology, it can be extended for any other configuration eg: WRR, FIFOs, AC etc
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wlc_types.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc_scb.h>
#include <wlc.h>
#include <wl_dbg.h>
#include <wlc_dump.h>
#include <wlc_txcfg.h>
#include <wlc_tx.h>
#include <wlc_fifo.h>
#include <wlc_musched.h>
#include <wlc_mutx.h>
#include <wlc_twt.h>
#include <wlc_tx.h>
#include <wlc_cfp.h>

#define WAR_COREREV64_65

/* Note on reserved FIFOs: Reserved FIFOs are AC agnostics.
 * For TWT it is required to have 4 AC specific FIFOs. Reserved FIFOs does not include those. But
 * max count take into account of AC specific and any-AC FIFOs. Meaning max count minus 4 and minus
 * reserved count is equal to allowed FIFOs from global unserved space.
 */

/****** FIFO ALLOCATION TABLE *****************************
*		VHT-MUMIO	HE MUMIO	OFDMA	TWT
* Max:
* Reserved (Any AC):
* Per AC:
* Overload Allowed:
*/
typedef struct wlc_fifo_limits {
	uint8	max_per_tx[WL_MU_TX_TYPE_MAX];
	uint8	reserved_any_ac[WL_MU_TX_TYPE_MAX];
	uint8	max_per_ac[WL_MU_TX_TYPE_MAX];
	uint8	overload_allowed[WL_MU_TX_TYPE_MAX];
} wlc_fifo_limits_t;

const wlc_fifo_limits_t fifoLimitTable_70 = {
	{-1, -1, -1, 8},
	{0, 0, 0, 0},
	{8, 8, 16, -1},
	{0, 0, 1, 1},
};

const wlc_fifo_limits_t fifoLimitTable_38 = {
	{-1, -1, -1, 4},
	{0, 0, 0, 0},
	{2, 2, 4, -1},
	{0, 0, 1, 1},
};

const wlc_fifo_limits_t fifoLimitTable_22 = {
	{-1, -1, -1, 4},
	{0, 0, 0, 0},
	{3, 3, 4, -1},
	{0, 0, 1, 1},
};

/* Table for 11ac only chips 4365/4366 (corev 64/65)
* EAP build will have 24 FIFO and non-EAP build will have 32.
* Same table will be used in both cases.
*/
const wlc_fifo_limits_t fifoLimitTable_24_32 = {
	{16, 0, 0, 0},
	{0, 0, 0, 0},
	{8, 0, 0, 0},
	{0, 0, 0, 0},
};

static const uint8 ac2sufifo[AC_COUNT] = {
	TX_AC_BE_FIFO,
	TX_AC_BK_FIFO,
	TX_AC_VI_FIFO,
	TX_AC_VO_FIFO
};
/* iovar table */
enum wlc_fifo_iov {
	IOV_FIFO_CONFIG = 1,
	IOV_FIFO_LAST
};
static const bcm_iovar_t fifo_iovars[] = {
	{"fifo", IOV_FIFO_CONFIG,
	(IOVF_SET_DOWN), 0, IOVT_BUFFER, 0
	},
	{NULL, 0, 0, 0, 0, 0 }
};

/* FIFO indexes array per SCB */
typedef struct fifo_scb_cubby {
	uint8 fifo_idx[AC_COUNT];
} fifo_scb_cubby_t;

/* State structure for the fifo module created by
 * wlc_fifo_module_attach().
 */
typedef struct wlc_tx_fifo_state {
	uint8 num_users;	/* Number of users in this FIFO */
	uint8 ac_mu_type;	/* Bit 0-3 mu types, bit 4-5 ac types */
} wlc_tx_fifo_state_t;

#define FIFO_AC_TYPE_MASK	0x3
#define FIFO_AC_TYPE_SHIFT	4
#define FIFO_MU_TYPE_MASK	0xF
#define FIFO_MU_TYPE_SHIFT	0

#define FIFO_MU_TYPE(state)	(((state) >> FIFO_MU_TYPE_SHIFT) & FIFO_MU_TYPE_MASK)
#define FIFO_AC_TYPE(state)	(((state) >> FIFO_AC_TYPE_SHIFT) & FIFO_AC_TYPE_MASK)

#define FIFO_STATE_SU   0
#define FIFO_STATE_MU   1

struct wlc_fifo_info {
	osl_t *osh;		/* OSL handle */
	wlc_info_t *wlc;	/* wlc_info_t handle */
	wlc_pub_t *pub;		/* wlc_pub_t handle */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */

	wlc_fifo_limits_t fifoLimitTable;
	uint8   module_state;          /* Record SU or MU state */
	uint16	mu_fifo_start_idx; /* Base index for FIFOs */
	uint8	fifo_global_max; /* Max number of FIFOs for the give MU type */
	uint8	fifo_remaining_global;
	uint8	fifo_remaining_mu[WL_MU_TX_TYPE_MAX];
	uint8	fifo_remaining_ac[WL_MU_TX_TYPE_MAX][AC_COUNT];
	uint8	start_unreserved; /* Start index of open pool for all MU types */
	uint8	reserved_start_idx[WL_MU_TX_TYPE_MAX]; /* start indexe of all reserved FIFO */
	uint8	reserved_end_idx[WL_MU_TX_TYPE_MAX]; /* end indexes of all reserved FIFO */
	wlc_tx_fifo_state_t	*fifo_state; /* Pointer to an array of size fifo_max. */
};

#define FIFO_SCB_CUBBY(fifo_info, scb) \
	((fifo_scb_cubby_t *)SCB_CUBBY(scb, (fifo_info)->scb_handle))

/* Basic module infrastructure */
static int wlc_fifo_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
static int wlc_fifo_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* SCB cubby management */
static int wlc_fifo_scb_init(void *context, struct scb *scb);
static void wlc_fifo_scb_deinit(void *context, struct scb *scb);
static void wlc_fifo_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b);

static int wlc_fifo_cfg_init(void *ctx);

/* Module Attach/Detach */
/*
 * Create the tx config infrastructure for different technology for the wl driver.
 * wlc_module_register() is called to register the module's
 * handlers. The dump function is also registered. Handlers are only
 * registered if the phy is MU BFR capable and if MU TX is not disabled
 * in the build.
 *
 * Returns
 *     A wlc_fifo_info_t structure, or NULL in case of failure.
 */
wlc_fifo_info_t *
BCMATTACHFN(wlc_fifo_attach)(wlc_info_t *wlc)
{
	wlc_fifo_info_t *fifo_info;
	int err;

	/* allocate the main state structure */
	fifo_info = MALLOCZ(wlc->osh, sizeof(wlc_fifo_info_t));
	if (fifo_info == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));

		return NULL;
	}

	fifo_info->wlc = wlc;
	fifo_info->pub = wlc->pub;
	fifo_info->osh = wlc->osh;

	/* If No MU FIFO available then return null */
	if ((WLC_HW_NFIFO_TOTAL(wlc) - TX_FIFO_HE_MU_START) <= 0) {
		fifo_info->fifo_state = NULL;
		fifo_info->module_state = FIFO_STATE_SU;
	} else {
		fifo_info->module_state = FIFO_STATE_MU;
		if ((fifo_info->fifo_state =
			MALLOCZ(wlc->osh, (WLC_HW_NFIFO_TOTAL(wlc) - TX_FIFO_HE_MU_START) *
			sizeof(wlc_tx_fifo_state_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));

			/* Free module structure */
			MFREE(fifo_info->osh, fifo_info, sizeof(wlc_fifo_info_t));
			return NULL;	/* No Memory */
		}

		if (WLC_HW_NFIFO_TOTAL(wlc) == 70) {
			memcpy(&fifo_info->fifoLimitTable, &fifoLimitTable_70,
				sizeof(wlc_fifo_limits_t));
		} else if (WLC_HW_NFIFO_TOTAL(wlc) == 38) {
			memcpy(&fifo_info->fifoLimitTable, &fifoLimitTable_38,
				sizeof(wlc_fifo_limits_t));
		} else if (WLC_HW_NFIFO_TOTAL(wlc) == 22) {
			memcpy(&fifo_info->fifoLimitTable, &fifoLimitTable_22,
				sizeof(wlc_fifo_limits_t));
		} else if ((WLC_HW_NFIFO_TOTAL(wlc) == 24) || (WLC_HW_NFIFO_TOTAL(wlc) == 32)) {
			memcpy(&fifo_info->fifoLimitTable, &fifoLimitTable_24_32,
				sizeof(wlc_fifo_limits_t));
		} else {
			WL_ERROR(("wl%d: %s: FIFO count %d not supported\n", wlc->pub->unit,
				__FUNCTION__, WLC_HW_NFIFO_TOTAL(wlc)));
			ASSERT(0);
		}

		fifo_info->mu_fifo_start_idx = TX_FIFO_HE_MU_START; /* start of MU FIFOs */

		err = wlc_module_register(fifo_info->pub, fifo_iovars, "fifo", fifo_info,
			wlc_fifo_doiovar, NULL, wlc_fifo_cfg_init, NULL);

		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_module_register() failed with error %d (%s).\n",
				wlc->pub->unit, __FUNCTION__, err, bcmerrorstr(err)));

			/* use detach as a common failure deallocation */
			wlc_fifo_detach(fifo_info);
			return NULL;
		}

		/* reserve scb cubby space for STA-specific data. */
		fifo_info->scb_handle =
			wlc_scb_cubby_reserve(wlc, sizeof(fifo_scb_cubby_t), wlc_fifo_scb_init,
				wlc_fifo_scb_deinit, wlc_fifo_scb_dump, fifo_info);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)
		wlc_dump_add_fns(fifo_info->pub, "txfifo", wlc_fifo_dump, NULL, fifo_info);
#endif // endif
	}
	return fifo_info;
}

/* Free all resources associated with the tx config module
 * infrastructure. This is done at the cleanup stage when
 * freeing the driver.
 *
 * fifo_info    fifo module state structure
 */
void
BCMATTACHFN(wlc_fifo_detach)(wlc_fifo_info_t *fifo_info)
{
	if (fifo_info == NULL) {
		return;
	}

	if (fifo_info->module_state == FIFO_STATE_MU) {
		/* Module is registered only for MU */
		wlc_module_unregister(fifo_info->pub, "fifo", fifo_info);
	}

	/* Free FIFO state management array */
	if (fifo_info->fifo_state != NULL)
		MFREE(fifo_info->osh, fifo_info->fifo_state, (WLC_HW_NFIFO_TOTAL(fifo_info->wlc)
			- TX_FIFO_HE_MU_START) * sizeof(wlc_tx_fifo_state_t));
	/* Free module structure */
	MFREE(fifo_info->osh, fifo_info, sizeof(wlc_fifo_info_t));
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU) || defined(BCMDBG_MU)

static int wlc_fifo_dump(void *ctx, struct bcmstrbuf *b)
{
	int i;
	struct scb_iter scbiter;
	struct scb *scb;
	char eabuf[ETHER_ADDR_STR_LEN];
	char txTypeStr[WL_MU_TX_TYPE_MAX][10] = {"VHTMU", "HEMMU", "DLOFDMA", "TWT"};
	char acTypeStr[AC_COUNT][3] = {"BE", "BK", "VI", "VO"};
	uint8 donotCare = -1;
	int fifo_idx;
	int num_users;
	int ac;
	int mu;
	fifo_scb_cubby_t* fifo_scb;
	wlc_fifo_info_t *fifo_info = ctx;
	wlc_info_t *wlc = fifo_info->wlc;

	if ((fifo_info->module_state == FIFO_STATE_SU) ||
		(fifo_info->fifo_global_max == 0)) {
		bcm_bprintf(b, "System is in SU only state\n");
		return BCME_OK;
	}

	bcm_bprintf(b, "Global MU FIFO pool: %d, used: %d. ",
		fifo_info->fifo_global_max,
		(fifo_info->fifo_global_max - fifo_info->fifo_remaining_global));

	if (fifo_info->fifoLimitTable.max_per_tx[WL_MU_TWT-1]) {
		bcm_bprintf(b, "TWT Reserved AC Specific: %d\n", AC_COUNT);
	} else {
		bcm_bprintf(b, "\n");
	}
	bcm_bprintf(b, "MUType limit: AllAC perAC used: BE BK VI VO\n");
	for (i = 0; i < WL_MU_TX_TYPE_MAX; i++)
	{
		bcm_bprintf(b, "%-7s\t\t", txTypeStr[i]);
		if (fifo_info->fifoLimitTable.max_per_tx[i] == 0) {
			bcm_bprintf(b, "N/A\n");
		} else {
			if (fifo_info->fifoLimitTable.max_per_tx[i] == donotCare) {
				bcm_bprintf(b, "%-3d  ",
					AC_COUNT*fifo_info->fifoLimitTable.max_per_ac[i]);
			} else {
				bcm_bprintf(b, "%-3d  ", fifo_info->fifoLimitTable.max_per_tx[i]);
			}

			if (fifo_info->fifoLimitTable.max_per_ac[i] == donotCare) {
				bcm_bprintf(b, "%-3d \t", fifo_info->fifoLimitTable.max_per_tx[i]);
			} else {
				bcm_bprintf(b, "%-3d \t", fifo_info->fifoLimitTable.max_per_ac[i]);
			}
			bcm_bprintf(b, "%-2d %-2d %-2d %-2d\n",
				(fifo_info->fifoLimitTable.max_per_ac[i]
				- fifo_info->fifo_remaining_ac[i][0]),
				(fifo_info->fifoLimitTable.max_per_ac[i]
				- fifo_info->fifo_remaining_ac[i][1]),
				(fifo_info->fifoLimitTable.max_per_ac[i]
				- fifo_info->fifo_remaining_ac[i][2]),
				(fifo_info->fifoLimitTable.max_per_ac[i]
				- fifo_info->fifo_remaining_ac[i][3]));
		}
	}
	if (fifo_info->fifo_state == NULL)
		return BCME_OK;
	bcm_bprintf(b, "\nFIFO Usage\n");
	bcm_bprintf(b, "sw/hw-idx MuType  AC STA (aid)\n");
	for (i = 0; i < fifo_info->fifo_global_max; i++)
	{
		num_users = fifo_info->fifo_state[i].num_users;
		if (num_users == 0) {
			continue;
		}
		fifo_idx = i + fifo_info->mu_fifo_start_idx;
		ac = FIFO_AC_TYPE(fifo_info->fifo_state[i].ac_mu_type);
		mu = FIFO_MU_TYPE(fifo_info->fifo_state[i].ac_mu_type);
		bcm_bprintf(b, "\r%2d/%-2d \t  %-7s %2s ", fifo_idx,
				WLC_HW_MAP_TXFIFO(wlc, fifo_idx),
				txTypeStr[mu-1], acTypeStr[ac]);

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!scb || !SCB_ASSOCIATED(scb)) {
				continue;
			}
			fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
			if (fifo_scb) {
				if (fifo_scb->fifo_idx[ac] == fifo_idx) {
					bcm_bprintf(b, "%s (%2d)\n\t\t     ",
						bcm_ether_ntoa(&scb->ea, eabuf), scb->aid);
					num_users--;
					if (num_users == 0) {
						break;
					}
				}
			}
		}
	}
	bcm_bprintf(b, "\r");

	return BCME_OK;
}
#endif // endif

/* IOVar handler for FIFO managment module */
static int
wlc_fifo_doiovar(void *hdl, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	int err = BCME_OK;
	wlc_fifo_info_t *fifo_info = (wlc_fifo_info_t*) hdl;

	BCM_REFERENCE(fifo_info);

	switch (actionid) {
	case IOV_GVAL(IOV_FIFO_CONFIG): {
		wl_fifo_msg_t	*fifo_msg_in = (wl_fifo_msg_t *)params;
		wl_fifo_msg_t	*fifo_msg_out = (wl_fifo_msg_t *)arg;
		if (!strncmp(fifo_msg_in->keystr, "perac", strlen("perac"))) {
			fifo_msg_out->mutype = fifo_msg_in->mutype;
			fifo_msg_out->value =
			fifo_info->fifoLimitTable.max_per_ac[fifo_msg_in->mutype-1];
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_SVAL(IOV_FIFO_CONFIG): {
		wl_fifo_msg_t	*fifo_msg_in = (wl_fifo_msg_t *)params;
		if (!strncmp(fifo_msg_in->keystr, "perac", strlen("perac"))) {
			if (fifo_msg_in->value > fifo_info->fifo_global_max)
				fifo_msg_in->value = fifo_info->fifo_global_max;
			fifo_info->fifoLimitTable.max_per_ac[fifo_msg_in->mutype-1] =
				fifo_msg_in->value;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* wlc init callback */
static int
wlc_fifo_cfg_init(void *ctx)
{
	int i, j;
	wlc_fifo_info_t *fifo_info = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = fifo_info->wlc;
	uint8 ac_mu_type;
	uint8 mu;

	/* Change of global max FIFO would mean system is in transition between SU<->MU.
	 * In this case we should reinit FIFO state evenif this a reinit case.
	 * In case of SU only chip global max MU FIFO count will always be zero.
	 */
	if (wlc->pub->up) {
		/* In case of reinit, check if total MU FIFO count has changed */
		if ((WLC_HW_NFIFO_INUSE(wlc) - fifo_info->mu_fifo_start_idx)
				== fifo_info->fifo_global_max) {
			/* No change in FIFO count. Return from here */
			return BCME_OK;
		} else {
			/* MU FIFO count has changed. Reset of FIFO state is necessary.
			 * Reset all SCBs' FIFO indexes to SU FIFOs
			 */
			fifo_scb_cubby_t* fifo_scb;
			struct scb_iter scbiter;
			struct scb *scb;
			FOREACHSCB(wlc->scbstate, &scbiter, scb) {
				if (!scb || !SCB_ASSOCIATED(scb)) {
					continue;
				}
				fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
				if (fifo_scb) {
					for (i = 0; i < AC_COUNT; i++)
						fifo_scb->fifo_idx[i] = ac2sufifo[i];
				}
			}
		}
	}

	/* Clear FIFO state memory */
	memset(fifo_info->fifo_state, 0, (WLC_HW_NFIFO_TOTAL(wlc) - TX_FIFO_HE_MU_START)
		* sizeof(wlc_tx_fifo_state_t));

	fifo_info->fifo_global_max = WLC_HW_NFIFO_INUSE(wlc) -
		fifo_info->mu_fifo_start_idx;

	if (fifo_info->fifo_global_max <= 0) {
		/* No FIFO available for MU clients */
		/* Chip is MU capable but configured as SU only */
		return BCME_OK;
	}

	fifo_info->fifo_remaining_global = fifo_info->fifo_global_max;
	for (i = 0; i < WL_MU_TX_TYPE_MAX; i++)
	{
		fifo_info->fifo_remaining_mu[i] = fifo_info->fifoLimitTable.max_per_tx[i];

		for (j = 0; j < AC_COUNT; j++)
			fifo_info->fifo_remaining_ac[i][j] =
					fifo_info->fifoLimitTable.max_per_ac[i];

	}

	/* Reserve 4 AC specific FIFOs to support TWT. Note that these reserved FIFOs are in
	 * access of AC agnostic reserved FIFOs specified in FIFO limit table for TWT
	 */
	i = 0;
	mu = WL_MU_TWT;
	if (fifo_info->fifoLimitTable.max_per_tx[mu-1]) {
		for (i = 0; i < AC_COUNT; i++)
		{
			ac_mu_type = (WL_MU_TWT | (i << FIFO_AC_TYPE_SHIFT));
			fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
			fifo_info->fifo_remaining_ac[mu-1][i]--;
		}

		fifo_info->fifo_remaining_global = fifo_info->fifo_global_max - AC_COUNT;
	}
	/* Reserve FIFOs as specified in FIFO allocation table */
	/* These are AC agnostic reserved FIFOs */
	for (j = 0; j < WL_MU_TX_TYPE_MAX; j++)
	{
		if (fifo_info->fifoLimitTable.reserved_any_ac[j]) {
			if (j == (WL_MU_TWT - 1)) {
				ASSERT((fifo_info->fifoLimitTable.reserved_any_ac[j] + AC_COUNT) <=
					fifo_info->fifoLimitTable.max_per_tx[j]);
			} else {
				ASSERT(fifo_info->fifoLimitTable.reserved_any_ac[j] <=
					fifo_info->fifoLimitTable.max_per_tx[j]);
			}
			fifo_info->reserved_start_idx[j] = i; /* Start index */
			fifo_info->reserved_end_idx[j] = i +
				fifo_info->fifoLimitTable.reserved_any_ac[j]; /* End index+1 */
			/* We don't know AC at this point.
			 * In MU enum definition MU tech starts at 1. Zero is SU.
			 * Add one to get MU enum for given technology.
			 */
			ac_mu_type = j + 1;
			for (; i < fifo_info->reserved_end_idx[j]; i++)
			{
				fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
			}
			fifo_info->fifo_remaining_mu[j] =
				fifo_info->fifoLimitTable.max_per_tx[j]
					- fifo_info->fifoLimitTable.reserved_any_ac[j];
		} else {
			fifo_info->reserved_start_idx[j] = 0; /* Start index */
			fifo_info->reserved_end_idx[j] = 0; /* End index */
		}
	}

	/* Here index i points to start of remaining unreserved global FIFO pool */
#if defined(WAR_COREREV64_65)
		/* FIFO indexed 6,7 are skipped. Refer to wlc_bmac_attach_dmapio() in wlc_bmac.c
		 * These FIFOs corresponds to indexes 0 and 1 in MU FIFO space. Skip those.
		 */
		if ((fifo_info->pub->corerev == 64) || (fifo_info->pub->corerev == 65)) {
			i = 2;
		}
#endif // endif
	fifo_info->start_unreserved = i;
	fifo_info->fifo_remaining_global -= fifo_info->start_unreserved;

	return err;
}

/* Iinitialize this module's scb state. */
static int
wlc_fifo_scb_init(void *ctx, struct scb *scb)
{
	int i;
	wlc_fifo_info_t *fifo_info = (wlc_fifo_info_t*) ctx;
	fifo_scb_cubby_t *fifo_cubby;
	BCM_REFERENCE(fifo_info);
	BCM_REFERENCE(fifo_cubby);

	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);

	for (i = 0; i < AC_COUNT; i++)
	{
		ASSERT(fifo_cubby->fifo_idx[i] == 0);
		fifo_cubby->fifo_idx[i] = ac2sufifo[i];
	}

	return BCME_OK;
}

/* Deinitialize this module's scb state. Remove the STA from the MU client set.
 * Free the module's state structure.
 */
static void
wlc_fifo_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_fifo_info_t *fifo_info = (wlc_fifo_info_t*) ctx;
	wlc_fifo_free_all(fifo_info, scb);
}

static void
wlc_fifo_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_fifo_info_t *fifo_info;
	fifo_scb_cubby_t *fifo_scb;

	ASSERT(context != NULL);
	fifo_info = (wlc_fifo_info_t*) context;
	fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);

	bcm_bprintf(b, "\tFIFO Idx: %d %d %d %d\n", fifo_scb->fifo_idx[0], fifo_scb->fifo_idx[1],
		fifo_scb->fifo_idx[2], fifo_scb->fifo_idx[3]);
}

uint8
wlc_fifo_index_get(wlc_fifo_info_t *fifo_info, struct scb *scb, uint prio)
{
	uint ac;
	mu_type_t mu;
	wlc_info_t *wlc = fifo_info->wlc;
	fifo_scb_cubby_t *fifo_cubby;
	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);
	ac = prio2ac[prio];

	/* If chip is only SU capable or if the chip is MU capable but configured in SU only
	 * mode, return default setting which is SU FIFO index.
	 */
	if ((fifo_info->module_state == FIFO_STATE_SU) ||
		(fifo_info->fifo_global_max == 0)) {
		return ac2sufifo[ac];
	}

	if (wlc_twt_scb_active(wlc->twti, scb)) {
		if (fifo_cubby->fifo_idx[ac] >= fifo_info->mu_fifo_start_idx) {
			/* This must be a TWT FIFO */
			ASSERT(fifo_info->fifo_state[fifo_cubby->fifo_idx[ac] -
				fifo_info->mu_fifo_start_idx].ac_mu_type ==
				(WL_MU_TWT | (ac << FIFO_AC_TYPE_SHIFT)));
		} else {
			/* Allocate a TWT FIFO */
			wlc_fifo_alloc(fifo_info, scb, WL_MU_TWT, ac);
#ifdef WLCFP
			wlc_cfp_cq_fifo_upd(wlc, scb, ac);
#endif // endif
		}
		return fifo_cubby->fifo_idx[ac];
	}

	/* Find out of MU Type of the SCB */
	mu = WL_MU_NONE;
	if (BSSCFG_AP(scb->bsscfg) && !SCB_INTERNAL(scb) && !PIO_ENAB_HW(wlc->hw)) {
		if (MU_TX_ENAB(wlc) && wlc_mutx_is_muclient(wlc->mutx, scb)) {
			mu = WL_MU_VHTMU;
		}
		if (SCB_HE_CAP(scb) && HE_DLMU_ENAB(wlc->pub)) {
			if (SCB_HEMMU(scb)) {
				mu = WL_MU_HEMMU;
			} else if (SCB_DLOFDMA(scb)) {
				mu = WL_MU_DLOFDMA;
			} else {
				// SU
			}
		}
	}

	if (mu == WL_MU_NONE)
		return fifo_cubby->fifo_idx[ac];

	if (fifo_cubby->fifo_idx[ac] < fifo_info->mu_fifo_start_idx) {
		wlc_fifo_alloc(fifo_info, scb, mu, ac);
#ifdef WLCFP
		wlc_cfp_cq_fifo_upd(wlc, scb, ac);
#endif // endif
	}
	return fifo_cubby->fifo_idx[ac];
}

uint8
wlc_fifo_index_peek(wlc_fifo_info_t *fifo_info, struct scb *scb, uint ac)
{
	fifo_scb_cubby_t *fifo_cubby;
	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);
	ASSERT(ac <= TX_AC_VO_FIFO);
	return fifo_cubby->fifo_idx[ac];
}

void
wlc_fifo_alloc(wlc_fifo_info_t *fifo_info, struct scb *scb, mu_type_t mu, uint ac)
{
	int fifo_idx;
	int i;
	int user_count;
	int min_idx;
	uint8 ac_mu_type;

	fifo_scb_cubby_t *fifo_cubby;
	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);

	/* FIFO index will be set to SU FIFO if one of these are true
	* 1. SCB's tx technology in use is SU (MU_NONE)
	* 2. If the system does not any MU technology.
	* 3. If the system supports MU technologies but configured as SU only mode.
	*/
	if ((mu == WL_MU_NONE) || (fifo_info->fifo_state == NULL) ||
		(fifo_info->fifo_global_max == 0))
	{
		fifo_cubby->fifo_idx[ac] = ac2sufifo[ac];
		return;
	}
#if defined(BCMDBG) || defined(BCMDBG_MU)
	if (!wlc_mutx_ac(fifo_info->wlc->txqi, ac, 1 << (mu - 1))) {
		fifo_cubby->fifo_idx[ac] = ac2sufifo[ac];
		return;
	}
#endif // endif
	ac_mu_type = (mu | (ac << FIFO_AC_TYPE_SHIFT));
	if (fifo_cubby->fifo_idx[ac] >= fifo_info->mu_fifo_start_idx)
	{
		fifo_idx = fifo_cubby->fifo_idx[ac];
		if (fifo_info->fifo_state[fifo_idx].ac_mu_type == ac_mu_type) {
			/* Client already has a FIFO that matches with AC and MU type.
			* No need to allocate a new one.
			*/
			return;
		} else {
			/* Client has a FIFO for different AC and mu type.
			* release old FIFO and then alloc a new one.
			*/
			wlc_fifo_free(fifo_info, scb, fifo_idx);
		}
	}

	/* For TWT, if reserved FIFO is not in use allocate that */
	/* First 4 FIFOs are TWT AC specific FIFOs */
	if ((mu == WL_MU_TWT) && (fifo_info->fifo_state[ac].num_users == 0)) {
		fifo_info->fifo_state[ac].num_users++;
		fifo_idx = ac + fifo_info->mu_fifo_start_idx;
		/* Save SCB FIFO pointer in fifo_info cubby */
		fifo_cubby->fifo_idx[ac] = fifo_idx;
		return;
	}

	/* Now search in global MU FIFO pool */
	if ((fifo_info->fifo_remaining_global == 0) ||
		(fifo_info->fifo_remaining_mu[mu-1] == 0) ||
		(fifo_info->fifo_remaining_ac[mu-1][ac] == 0)) {
		/* Can't allocate a new FIFO for the given AC.
		 * Overload an existing one if allowed and available (same AC).
		 * Otherwise return SU FIFO.
		 */
		user_count = MAXSCB;
		min_idx = -1;
		if (fifo_info->fifoLimitTable.overload_allowed[mu-1]) {
			for (i = 0; i < fifo_info->fifo_global_max; i++)
			{
				if ((fifo_info->fifo_state[i].ac_mu_type == ac_mu_type) &&
					(fifo_info->fifo_state[i].num_users < user_count)) {
					min_idx = i;
					user_count = fifo_info->fifo_state[i].num_users;
				}
			}
		}
		if (min_idx == -1)
		{
			/* FIFO overloading is not allowed
			* and no FIFO with same <AC,mu_type> was allocated
			* before. Return SU FIFO index of given ac.
			*/
			fifo_idx = ac2sufifo[ac];
		} else {
			fifo_info->fifo_state[min_idx].num_users++;
			fifo_idx = min_idx + fifo_info->mu_fifo_start_idx;
		}
	} else {
		/* Allocate a new FIFO for the given AC */
		/* First look for MU specific reserved FIFO */
		i = 0;
		if (fifo_info->fifoLimitTable.reserved_any_ac[mu-1]) {
			for (i = fifo_info->reserved_start_idx[mu-1];
				i < fifo_info->reserved_end_idx[mu-1]; i++)
			{
				if (fifo_info->fifo_state[i].num_users == 0) {
					/* This is FIFO is available. */
					break;
				}
			}
		}

		if (i == fifo_info->reserved_end_idx[mu-1]) {
			/* Did not find a unused FIFO in reserved space.
			 * Now search unserved global MU FIFO space.
			 * In case there is no reserved FIFO variable i will be 0 and
			 * fifo_info->reserved_end_idx[mu-1] is also be zero.
			 * Above test will succeed.
			*/
			for (i = fifo_info->start_unreserved; i < fifo_info->fifo_global_max; i++)
			{
				if (fifo_info->fifo_state[i].num_users == 0) {
					/* Index i points to an unallocated FIFO */
					break;
				}
			}
		}
#if defined(WAR_COREREV64_65)
		if ((fifo_info->pub->corerev == 64) || (fifo_info->pub->corerev == 65)) {
			/* Allocate same FIFO for BE, BK. VO, VI has unique FIFOs */
			ac = AC_BE;
			ASSERT(i < fifo_info->fifo_global_max);
			ac_mu_type = (mu | (ac << FIFO_AC_TYPE_SHIFT));
			fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
			fifo_info->fifo_state[i].num_users++;
			fifo_info->fifo_remaining_global--;
			fifo_info->fifo_remaining_mu[mu-1]--;
			fifo_info->fifo_remaining_ac[mu-1][ac]--;
			fifo_idx = i + fifo_info->mu_fifo_start_idx;
			fifo_cubby->fifo_idx[ac] = fifo_idx;
			for (ac = AC_BK; ac < AC_COUNT; ac++)
			{
				ASSERT(i < fifo_info->fifo_global_max);
				ac_mu_type = (mu | (ac << FIFO_AC_TYPE_SHIFT));
				fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
				fifo_info->fifo_state[i].num_users++;
				fifo_info->fifo_remaining_global--;
				fifo_info->fifo_remaining_mu[mu-1]--;
				fifo_info->fifo_remaining_ac[mu-1][ac]--;
				fifo_idx = i + fifo_info->mu_fifo_start_idx;
				fifo_cubby->fifo_idx[ac] = fifo_idx;
				i++;
			}
			return;
		} else
#endif /* defined(WAR_COREREV64_65) */
		{
			ASSERT(i < fifo_info->fifo_global_max);
			fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
			fifo_info->fifo_state[i].num_users++;
			/* Decrement global and mu specific remaining FIFO if it is not reserved */
			if (i >= fifo_info->start_unreserved) {
				fifo_info->fifo_remaining_global--;
				fifo_info->fifo_remaining_mu[mu-1]--;
			}
			fifo_info->fifo_remaining_ac[mu-1][ac]--;
			fifo_idx = i + fifo_info->mu_fifo_start_idx;
		}
	}
	/* Save SCB FIFO pointer in fifo_info cubby */
	fifo_cubby->fifo_idx[ac] = fifo_idx;
	return;
}

void
wlc_fifo_free(wlc_fifo_info_t *fifo_info, struct scb *scb, int fifo_index)
{
	uint ac;
	mu_type_t mu;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);

	if ((fifo_index < fifo_info->mu_fifo_start_idx) ||
		(fifo_info->fifo_state == NULL)) {
		/* SU FIFO */
		return;
	}

	ASSERT((fifo_index >= fifo_info->mu_fifo_start_idx) &&
		(fifo_index < WLC_HW_NFIFO_INUSE(fifo_info->wlc)));
	fifo_index = fifo_index - fifo_info->mu_fifo_start_idx;
	ASSERT(fifo_info->fifo_state[fifo_index].num_users > 0);
	fifo_info->fifo_state[fifo_index].num_users--;
	ac = FIFO_AC_TYPE(fifo_info->fifo_state[fifo_index].ac_mu_type);

	/* Release FIFO if num_users is zero and FIFO is not reserved */
	if (fifo_info->fifo_state[fifo_index].num_users == 0) {
		mu = FIFO_MU_TYPE(fifo_info->fifo_state[fifo_index].ac_mu_type);
		if (fifo_index < AC_COUNT) {
			/* First 4 FIFOs are TWT AC specific reserved FIFOs */
			ASSERT(mu == WL_MU_TWT);
		} else {
			fifo_info->fifo_remaining_ac[mu-1][ac]++;
			fifo_info->fifo_state[fifo_index].ac_mu_type = mu; /* Clear AC type */
			if (fifo_index >= (fifo_info->start_unreserved)) {
				fifo_info->fifo_state[fifo_index].ac_mu_type = 0;
				fifo_info->fifo_remaining_global++;
				fifo_info->fifo_remaining_mu[mu-1]++;
			}
		}
	}
	fifo_scb->fifo_idx[ac] = ac2sufifo[ac];  /* Reset to SU FIFO */
	return;
}

void
wlc_fifo_free_all(wlc_fifo_info_t *fifo_info, struct scb *scb)
{
	int i;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);

	/* If chip is only SU capable or chip is MU capable but configured as SU mode */
	if ((fifo_info->module_state == FIFO_STATE_SU) ||
		(fifo_info->fifo_global_max == 0)) {
		return;
	}

	for (i = 0; i < AC_COUNT; i++) {
		wlc_fifo_free(fifo_info, scb, fifo_scb->fifo_idx[i]);
#ifdef WLCFP
		wlc_cfp_cq_fifo_upd(fifo_info->wlc, scb, i);
#endif // endif
	}
}

void
wlc_fifo_sta_bitmap(wlc_fifo_info_t *fifo_info, struct scb *scb, void *fifo_bitmap)
{
	int ac;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
	BCM_REFERENCE(fifo_info);
	/* If chip is only SU capable or chip is MU capable but configured as SU mode */
	if ((fifo_info->module_state == FIFO_STATE_SU) ||
		(fifo_info->fifo_global_max == 0)) {
		/* Set bit maps for SU FIFOs. Four lowest indexed FIFOs are SU FIFOs */
		((uint8 *)fifo_bitmap)[0] = 0xF;
		return;
	}

	for (ac = 0; ac < AC_COUNT; ac++)
	{
		ASSERT(fifo_scb->fifo_idx[ac] < WLC_HW_NFIFO_INUSE(fifo_info->wlc));
		setbit(fifo_bitmap, fifo_scb->fifo_idx[ac]);
	}
}

bool
wlc_fifo_isMU(wlc_fifo_info_t *fifo_info, struct scb *scb, uint ac)
{
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
	return (fifo_scb->fifo_idx[ac] >= fifo_info->mu_fifo_start_idx);
}

bool
wlc_check_fifo_type(wlc_fifo_info_t *fifo_info, struct scb *scb, uint ac, mu_type_t mu)
{
	uint8 ac_mu_type;
	int fifo_idx;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);

	BCM_REFERENCE(mu);
	BCM_REFERENCE(ac_mu_type);

#if defined(WAR_COREREV64_65)
	/* In this case BE and BK use same FIFO. FIFO state labels that FIFO as BK.
	 * if ac is BE then chnage it to BK.
	 */
	if ((fifo_info->pub->corerev == 64) || (fifo_info->pub->corerev == 65)) {
		if (ac == AC_BE) {
			ac = AC_BK;
		}
	}
#endif // endif

	ac_mu_type = (mu | (ac << FIFO_AC_TYPE_SHIFT));
	fifo_idx = fifo_scb->fifo_idx[ac] - fifo_info->mu_fifo_start_idx;
	if (fifo_scb->fifo_idx[ac] < fifo_info->mu_fifo_start_idx) {
		/* MU clients may has SU FIFO. Valid case. */
		return TRUE;
	}

	ASSERT(ac_mu_type == fifo_info->fifo_state[fifo_idx].ac_mu_type);
	return (ac_mu_type == fifo_info->fifo_state[fifo_idx].ac_mu_type);
}

void
wlc_fifo_mutx_ac_release(wlc_fifo_info_t *fifo_info, wl_mutx_ac_mg_t *mutx_ac_mask)
{
	int mu;
	int mu_bitfield;
	int ac = mutx_ac_mask->ac;
	struct scb_iter scbiter;
	struct scb *scb;
	fifo_scb_cubby_t* fifo_scb;
	wlc_info_t *wlc = fifo_info->wlc;
	int fifo_index;
	/* FIFO will be freed if MU is disabled. */
	uint8 mu_disable_mask = (MU_AC_MASK & ~mutx_ac_mask->mask);

	if (mu_disable_mask == 0)
		return;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb)) {
			continue;
		}
		fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
		if (fifo_scb) {
			if (fifo_scb->fifo_idx[ac] < fifo_info->mu_fifo_start_idx)
				continue;
		}
		fifo_index = fifo_scb->fifo_idx[ac] - fifo_info->mu_fifo_start_idx;
		mu = FIFO_MU_TYPE(fifo_info->fifo_state[fifo_index].ac_mu_type);
		mu_bitfield = 1 << (mu-1);
		/* Free FIFOs if MU is disabled */
		if (mu_disable_mask & mu_bitfield) {
			wlc_fifo_free(fifo_info, scb, fifo_scb->fifo_idx[ac]);
#ifdef WLCFP
			wlc_cfp_cq_fifo_upd(wlc, scb, ac);
#endif // endif
		}
	}
}
