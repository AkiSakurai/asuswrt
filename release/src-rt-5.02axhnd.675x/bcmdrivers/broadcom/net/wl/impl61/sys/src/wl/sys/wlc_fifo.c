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

#define FIFO_STATE_SU	0
#define FIFO_STATE_MU	1

struct wlc_fifo_info {
	osl_t *osh;		/* OSL handle */
	wlc_info_t *wlc;	/* wlc_info_t handle */
	wlc_pub_t *pub;		/* wlc_pub_t handle */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */

	uint16	fifo_start_idx; /* Base index for FIFOs */
	uint8	state;		/* Record SU or MU state */
	uint16	fifo_max; /* Max number of FIFOs for the give MU type */
	uint8	fifo_available; /* Number of currently available FIFOs for all ACs of a MU type */
	uint8	fifo_available_ac[AC_COUNT]; /* Per AC FIFO avialable at present for a MU type */
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
		fifo_info->state = FIFO_STATE_SU;
	} else {
		fifo_info->state = FIFO_STATE_MU;
		if ((fifo_info->fifo_state =
			MALLOCZ(wlc->osh, (WLC_HW_NFIFO_TOTAL(wlc) - TX_FIFO_HE_MU_START) *
			sizeof(wlc_tx_fifo_state_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));

			/* Free module structure */
			MFREE(fifo_info->osh, fifo_info, sizeof(wlc_fifo_info_t));
			return NULL;	/* No Memory */
		}

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
		wlc_dump_add_fns(fifo_info->pub, "fifo", wlc_fifo_dump, NULL, fifo_info);
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

	if (fifo_info->state == FIFO_STATE_MU) {
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
	fifo_scb_cubby_t* fifo_scb;
	wlc_fifo_info_t *fifo_info = ctx;
	wlc_info_t *wlc = fifo_info->wlc;
	bcm_bprintf(b, "OFDMA FIFO available: %d, max: %d\n", fifo_info->fifo_available,
		fifo_info->fifo_max);
	bcm_bprintf(b, "Available per AC: ");
	for (i = 0; i < AC_COUNT; i++)
		bcm_bprintf(b, "%d ", fifo_info->fifo_available_ac[i]);
	bcm_bprintf(b, "Max per AC: %d\n", WLC_OFMDMA_MAX_RU);
	if (fifo_info->fifo_state == NULL)
		return BCME_OK;
	bcm_bprintf(b, "(mu, ac, users_count)");
	for (i = 0; i < fifo_info->fifo_max; i++)
	{
		if ((i%4) == 0) bcm_bprintf(b, "\n");
		bcm_bprintf(b, "(%d,%d,%d)\t",
			FIFO_MU_TYPE(fifo_info->fifo_state[i].ac_mu_type),
			FIFO_AC_TYPE(fifo_info->fifo_state[i].ac_mu_type),
			fifo_info->fifo_state[i].num_users);
	}
	bcm_bprintf(b, "\nFIFO indexes for SCBs (AC_BE, AC_BK, AC_VI, AC_VO)\n");

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
		if (fifo_scb) {
			bcm_bprintf(b, "ea %s, aid %d:: ", bcm_ether_ntoa(&scb->ea, eabuf),
				scb->aid & HE_STAID_MASK);
			bcm_bprintf(b, "%d, %d, %d, %d\n", fifo_scb->fifo_idx[AC_BE],
				fifo_scb->fifo_idx[AC_BK], fifo_scb->fifo_idx[AC_VI],
				fifo_scb->fifo_idx[AC_VO]);
		}
	}

	return BCME_OK;
}
#endif // endif

/* IOVar handler for FIFO managment module */
static int
wlc_fifo_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	int32 int_val = 0;
	int err = 0;
	wlc_fifo_info_t *fifo_info = (wlc_fifo_info_t*) hdl;
	BCM_REFERENCE(fifo_info);

	if (plen >= (int)sizeof(int_val))
		memcpy(&int_val, p, sizeof(int_val));

	switch (actionid) {

	case IOV_GVAL(IOV_FIFO_CONFIG): {
		break;
	}

	case IOV_SVAL(IOV_FIFO_CONFIG): {
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
	int i;
	wlc_fifo_info_t *fifo_info = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = fifo_info->wlc;
	uint16 max_mmu_usrs = wlc_txcfg_max_mmu_clients_get(wlc->txcfg);

	if (wlc->pub->up) {
		/* Don't reset FIFO state in case of reinit */
		return BCME_OK;
	}

	/* Clear FIFO state memory */
	memset(fifo_info->fifo_state, 0, (WLC_HW_NFIFO_TOTAL(wlc) - TX_FIFO_HE_MU_START)
		* sizeof(wlc_tx_fifo_state_t));

	fifo_info->fifo_start_idx = TX_FIFO_HE_MU_START + /* start of MU FIFOs */
		(max_mmu_usrs * AC_COUNT); /* skip VHTMUMIMO FIFOs */

	fifo_info->fifo_max = WLC_HW_NFIFO_INUSE(wlc) - fifo_info->fifo_start_idx;
	if (fifo_info->fifo_max <= 0) {
		/* No more FIFO available for OFDMA clients */
		return BCME_OK;
	}

	/* Maximum available FIFO per AC will be same as max RU supported by the chip.
	 * Following constant will be replaced by a chip specific number in future.
	 */
	for (i = 0; i < AC_COUNT; i++)
		fifo_info->fifo_available_ac[i] = WLC_OFMDMA_MAX_RU;

	fifo_info->fifo_available = fifo_info->fifo_max;

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
		fifo_cubby->fifo_idx[i] = 0;
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

	bcm_bprintf(b, "\tOFDMA FIFO: %d %d %d %d\n", fifo_scb->fifo_idx[0], fifo_scb->fifo_idx[1],
		fifo_scb->fifo_idx[2], fifo_scb->fifo_idx[3]);
}

uint8
wlc_fifo_index_get(wlc_fifo_info_t *fifo_info, struct scb *scb, uint ac)
{
	fifo_scb_cubby_t *fifo_cubby;
	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);
	ASSERT(ac <= TX_AC_VO_FIFO);

	/* If AC_BE FIFO is not assigned, then return SU FIFO which is same as AC */
	if (fifo_cubby->fifo_idx[AC_BE] == 0)
		return ac;

	if (fifo_cubby->fifo_idx[ac] == 0) {
		if (SCB_DLOFDMA(scb)) {
			wlc_fifo_alloc(fifo_info, scb, MU_TYPE_OFDMA, ac);
		} else {
			/* In phase 1 only OFDMA client will use this module */
			ASSERT(0);
		}
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

uint8
wlc_fifo_max_get(wlc_fifo_info_t *fifo_info)
{
	return fifo_info->fifo_max;
}

void
wlc_fifo_alloc(wlc_fifo_info_t *fifo_info, struct scb *scb, mutype_t mu, uint ac)
{
	int fifo_idx;
	int i;
	int user_count;
	int min_idx;
	uint8 ac_mu_type = (mu | (ac << FIFO_AC_TYPE_SHIFT));

	fifo_scb_cubby_t *fifo_cubby;
	fifo_cubby = FIFO_SCB_CUBBY(fifo_info, scb);

	/* For now only OFMDA clients will be supported.
	 * In future VHT-MUMIMO and HE-MUMIMO support will be added.
	 */
	ASSERT(mu == MU_TYPE_OFDMA);
	ASSERT(fifo_info->fifo_state != NULL);

	if ((fifo_info->fifo_available_ac[ac] == 0) ||
		(fifo_info->fifo_available == 0)) {
		/* Can't allocate a new FIFO for the given AC.
		 * Overload an existing one
		 */
		user_count = MAXSCB;
		min_idx = -1;
		for (i = 0; i < fifo_info->fifo_max; i++)
		{
			if ((fifo_info->fifo_state[i].ac_mu_type == ac_mu_type) &&
				(fifo_info->fifo_state[i].num_users < user_count)) {
				min_idx = i;
				user_count = fifo_info->fifo_state[i].num_users;
			}
		}

		if (min_idx == -1)
		{
			/* No new FIFO available and no FIFO with same AC was allocated before.
			* Return SU FIFO index of given ac. Which is same AC.
			*/
			fifo_idx = ac;
		} else {
			fifo_info->fifo_state[min_idx].num_users++;
			fifo_idx = min_idx + fifo_info->fifo_start_idx;
		}
	} else {
		/* Acquire a new FIFO for the given AC */
		for (i = 0; i < fifo_info->fifo_max; i++)
		{
			if (fifo_info->fifo_state[i].num_users == 0) {
				/* Index i points to an unallocated FIFO */
				break;
			}
		}
		ASSERT(i < fifo_info->fifo_max);
		fifo_info->fifo_state[i].ac_mu_type = ac_mu_type;
		fifo_info->fifo_state[i].num_users++;
		fifo_info->fifo_available--;
		fifo_info->fifo_available_ac[ac]--;
		fifo_idx = i + fifo_info->fifo_start_idx;
	}
	/* Save SCB FIFO pointer in fifo_info cubby */
	fifo_cubby->fifo_idx[ac] = fifo_idx;
	return;
}

void
wlc_fifo_free(wlc_fifo_info_t *fifo_info, int fifo_index)
{
	if (fifo_index < fifo_info->fifo_start_idx) {
		/* Releasing VHT-MUMIMO or SU FIFOs.
		 * Place holder for accounting of SU FIFO users which will be added later.
		 */
		return;
	}

	ASSERT((fifo_index >= fifo_info->fifo_start_idx) &&
		(fifo_index < WLC_HW_NFIFO_INUSE(fifo_info->wlc)));
	fifo_index = fifo_index - fifo_info->fifo_start_idx;
	if (fifo_info->fifo_state == NULL)
		return;
	if (fifo_info->fifo_state[fifo_index].num_users > 0)
	{
		fifo_info->fifo_state[fifo_index].num_users--;
		if (fifo_info->fifo_state[fifo_index].num_users == 0) {
			/* Clear this FIFO and increase available FIFO count */
			int ac = FIFO_AC_TYPE(fifo_info->fifo_state[fifo_index].ac_mu_type);
			fifo_info->fifo_available_ac[ac]++;
			fifo_info->fifo_available++;
			fifo_info->fifo_state[fifo_index].ac_mu_type = 0;
		}
	}
}

void
wlc_fifo_free_all(wlc_fifo_info_t *fifo_info, struct scb *scb)
{
	int i;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);

	for (i = 0; i < AC_COUNT; i++)
	{
		wlc_fifo_free(fifo_info, fifo_scb->fifo_idx[i]);
		fifo_scb->fifo_idx[i] = 0;
	}
}

void
wlc_fifo_sta_bitmap(wlc_fifo_info_t *fifo_info, struct scb *scb, void *fifo_bitmap, mutype_t mu)
{
	int ac;
	fifo_scb_cubby_t *fifo_scb = FIFO_SCB_CUBBY(fifo_info, scb);
	BCM_REFERENCE(fifo_info);
	if (!fifo_scb) {
		return;
	}

	ASSERT(mu == MU_TYPE_OFDMA);
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
	return (fifo_scb->fifo_idx[ac] >= fifo_info->fifo_start_idx);
}
