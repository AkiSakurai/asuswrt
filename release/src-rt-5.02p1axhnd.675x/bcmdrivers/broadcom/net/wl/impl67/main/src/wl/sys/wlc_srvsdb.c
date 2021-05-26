/*
 * SRVSDB feature.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_srvsdb.c 785034 2020-03-11 11:53:38Z $
 */

/* XXX: As is it seems only support NPHY.
 * A good candidate feature to remove when NPHY is EOL'd.
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_stf.h>
#include <wlc_phy_hal.h>
#include <wlc_srvsdb.h>

/* IOVar table - please number the enumerators explicity */
enum {
	IOV_FORCE_VSDB = 0,
	IOV_FORCE_VSDB_CHANS = 1,
	IOV_LAST
};

static const bcm_iovar_t srvsdb_iovars[] = {
	{"force_vsdb", IOV_FORCE_VSDB, 0, 0, IOVT_UINT8, 0},
	{"force_vsdb_chans", IOV_FORCE_VSDB_CHANS,
	IOVF_SET_UP | IOVF_GET_UP, 0, IOVT_BUFFER,  2 * sizeof(uint16)},
	{NULL, 0, 0, 0, 0, 0}
};

/** hardware assisted mechanism to alternate fast(er) between two bands */
struct wlc_srvsdb_info {
	wlc_info_t *wlc;
	uint8  sr_vsdb_enabled;       /**< is set when hardware was enabled for srvsdb operation */
	uint8  iovar_force_vsdb;      /**< debug mechanism to force vsdb using wl utility */
	uint16 vsdb_chans[2];	     /**< the two alternated channels */
	uint8 vsdb_save_valid[2];    /**< is set for channel when hw+sw context was saved */
	uint8 ss_algo_save_valid[2]; /**< switch faster by skipping stf related calls if possible */
	uint16 ss_algosaved[2];
	ppr_t*	vsdb_txpwr_ctl[2];   /**< save/restore power per rate context per channel */
};

static int wlc_srvsdb_force_set(wlc_info_t *wlc, uint8 val);

/**
 * Speed up band switch time by avoiding full stf initialization when possible, performing save or
 * restore of stf settings instead.
 */
void
wlc_srvsdb_stf_ss_algo_chan_get(wlc_info_t *wlc, chanspec_t chanspec)
{

	uint8 i_ppr = 0;
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;

	/* In VSDB, if ss algo chan was saved before, just restore it now */
	if ((srvsdb->iovar_force_vsdb) ||
#ifdef WLMCHAN
		(MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) ||
#endif /* WLMCHAN */
		FALSE) {
		i_ppr = (chanspec == srvsdb->vsdb_chans[0]) ? 0 : 1;

		if (srvsdb->ss_algo_save_valid[i_ppr] == 0) {
			/* Save */
			if (wlc->stf->ss_algosel_auto) {
				wlc_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel,
					chanspec);
			}
			srvsdb->ss_algo_save_valid[i_ppr] = 1;
			srvsdb->ss_algosaved[i_ppr] = wlc->stf->ss_algo_channel;
		} else {
			/* Restore */
			wlc->stf->ss_algo_channel = srvsdb->ss_algosaved[i_ppr];
		}
	} else {
		/* MCHAN not active, call full initialisation */
		if (wlc->stf->ss_algosel_auto) {
			wlc_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel, chanspec);
		}
	}
}

static int
wlc_srvsdb_wlc_down(void *ctx)
{
	wlc_srvsdb_info_t *srvsdb = ctx;
	wlc_info_t *wlc = srvsdb->wlc;

	if (SRHWVSDB_ENAB(wlc->pub)) {
		wlc_srvsdb_force_set(wlc, 0);
	}

	return BCME_OK;
}

/* iovar dispatcher */
static int
wlc_srvsdb_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_srvsdb_info_t *srvsdb = hdl;
	wlc_info_t *wlc = srvsdb->wlc;
	int32 *ret_int_ptr;
	int32 int_val = 0;
	int err = BCME_OK;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_SVAL(IOV_FORCE_VSDB):
		if (SRHWVSDB_ENAB(wlc->pub)) {
			wlc_srvsdb_force_set(wlc, (uint8)int_val);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_GVAL(IOV_FORCE_VSDB):
		if (SRHWVSDB_ENAB(wlc->pub)) {
			*ret_int_ptr = srvsdb->iovar_force_vsdb;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_SVAL(IOV_FORCE_VSDB_CHANS):
		if (SRHWVSDB_ENAB(wlc->pub)) {
			/* Reset SRVSDB engine so that previous saves are flushed out */
			wlc_srvsdb_reset_engine(wlc);

			memcpy(srvsdb->vsdb_chans, arg, sizeof(srvsdb->vsdb_chans));

			if (srvsdb->vsdb_chans[0] && srvsdb->vsdb_chans[1]) {
				/* Set vsdb chans */
				phy_chanmgr_vsdb_force_chans(WLC_PI(wlc), srvsdb->vsdb_chans, 1);
			} else {
				/* Reset vsdb chans */
				phy_chanmgr_vsdb_force_chans(WLC_PI(wlc), srvsdb->vsdb_chans, 0);
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_GVAL(IOV_FORCE_VSDB_CHANS):
		if (SRHWVSDB_ENAB(wlc->pub)) {
			memcpy(arg, srvsdb->vsdb_chans,
				sizeof(srvsdb->vsdb_chans));
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

wlc_srvsdb_info_t *
BCMATTACHFN(wlc_srvsdb_attach)(wlc_info_t *wlc)
{
	wlc_srvsdb_info_t *srvsdb;

	/* allocate module info */
	if ((srvsdb = MALLOCZ(wlc->osh, sizeof(*srvsdb))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	srvsdb->wlc = wlc;

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, srvsdb_iovars, "srvsdb", srvsdb, wlc_srvsdb_doiovar,
			NULL, NULL, wlc_srvsdb_wlc_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return srvsdb;

fail:
	wlc_srvsdb_detach(srvsdb);
	return NULL;
}

void
BCMATTACHFN(wlc_srvsdb_detach)(wlc_srvsdb_info_t *srvsdb)
{
	wlc_info_t *wlc;

	if (srvsdb == NULL)
		return;

	wlc = srvsdb->wlc;

	if (SRHWVSDB_ENAB(wlc->pub)) {
		wlc_srvsdb_reset_engine(wlc); /* resets software context, not hardware */
		wlc_bmac_deactivate_srvsdb(wlc->hw);
	}
}

/**
 * On e.g. a change of home channel, srvsdb context has to be zapped. This function resets data
 * members and frees ppr member. Despite the function name, it does not touch hardware.
 */
void
wlc_srvsdb_reset_engine(wlc_info_t *wlc)
{
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;

	bzero(srvsdb->vsdb_save_valid,    sizeof(srvsdb->vsdb_save_valid));
	bzero(srvsdb->ss_algo_save_valid, sizeof(srvsdb->ss_algo_save_valid));
	bzero(srvsdb->ss_algosaved,       sizeof(srvsdb->ss_algosaved));
	if (srvsdb->vsdb_txpwr_ctl[0] != NULL) {
		ppr_delete(wlc->osh, srvsdb->vsdb_txpwr_ctl[0]);
		srvsdb->vsdb_txpwr_ctl[0] = NULL;
	}
	if (srvsdb->vsdb_txpwr_ctl[1] != NULL) {
		ppr_delete(wlc->osh, srvsdb->vsdb_txpwr_ctl[1]);
		srvsdb->vsdb_txpwr_ctl[1] = NULL;
	}
	bzero(srvsdb->vsdb_chans, sizeof(srvsdb->vsdb_chans));
}

/**
 * Power-per-rate context has to be saved in software, to be restored later when the
 * software switches back to the channel.
 */
static void wlc_srvsdb_save_ppr(wlc_info_t *wlc, int i_ppr)
{
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;
	ppr_t **ppr;

	ppr = &srvsdb->vsdb_txpwr_ctl[i_ppr];

	srvsdb->vsdb_save_valid[i_ppr] = 1;
	if ((*ppr != NULL) &&
		(ppr_get_ch_bw(*ppr) !=	ppr_get_ch_bw(wlc->stf->txpwr_ctl))) {
		ppr_delete(wlc->osh, *ppr);
		*ppr = NULL;
	}
	if (*ppr == NULL) {
		*ppr = ppr_create(wlc->osh, ppr_get_ch_bw(wlc->stf->txpwr_ctl));
	}
	if (*ppr != NULL) {
		ppr_copy_struct(wlc->stf->txpwr_ctl, *ppr);
	}
} /* wlc_srvsdb_save_ppr */

static void wlc_srvsdb_restore_ppr(wlc_info_t *wlc, int i_ppr)
{
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;
	ppr_t *ppr = srvsdb->vsdb_txpwr_ctl[i_ppr];

	if (!srvsdb->vsdb_save_valid[i_ppr] || ppr == NULL) {
		return;
	}

	wlc_update_txppr_offset(wlc, ppr);
} /* wlc_srvsdb_restore_ppr */

/**
 * Is called by the BMAC after an (attempted) srvsdb channel change. Tx power related power-per-rate
 * context is not saved/restored by the PHY, so it has to be saved/restored by software. The PHY
 * indicated both if the previous channel was saved ('last_chan_saved') and if the SRVSDB switch
 * succeeded ('vsdb_status'). Depending on the result, channel context is saved and/or restored.
 * Note that despite a switch not being succesfull, the PHY can still have saved channel context.
 */
void wlc_srvsdb_switch_ppr(wlc_info_t *wlc, chanspec_t new_chanspec, bool last_chan_saved,
	bool switched)
{
	uint i_ppr = 0;			/* bank (0 or 1) containing previous channel context */
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;

	/* If phy and radio context were saved by BMAC, initiate a (software) save of ppr also */
	if (last_chan_saved) {
		i_ppr = (new_chanspec == srvsdb->vsdb_chans[0]) ? 1 : 0;
		wlc_srvsdb_save_ppr(wlc, i_ppr);
	}
	if (switched) {
		wlc_srvsdb_restore_ppr(wlc, !i_ppr); /* restore opposite bank on channel switch */
	}
}

/**
 * When srvsdb operation is forced by a higher layer (e.g. from wl utility), both driver and dongle
 * firmware have to store the new setting.
 */
static int
wlc_srvsdb_force_set(wlc_info_t *wlc, uint8 val)
{
	int err;
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;

	err = wlc_bmac_srvsdb_force_set(wlc->hw, val);
	if (!err) {
		srvsdb->iovar_force_vsdb = val;
	}
	return err;
}

bool
wlc_srvsdb_save_valid(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_srvsdb_info_t *srvsdb = wlc->srvsdb_info;

	if (FALSE ||
#ifdef WLMCHAN
	    (MCHAN_ENAB(wlc->pub) && MCHAN_ACTIVE(wlc->pub)) ||
#endif // endif
	    srvsdb->iovar_force_vsdb) {
		/* Check if power offsets are already saved */
		uint8 offset = (chanspec == srvsdb->vsdb_chans[0]) ? 0 : 1;

		if (srvsdb->vsdb_save_valid[offset]) {
			return TRUE;
		}
	}

	return FALSE;
}

void
wlc_srvsdb_set_chanspec(wlc_info_t *wlc, chanspec_t cur, chanspec_t next)
{
	struct wlc_srvsdb_info *srvsdb = wlc->srvsdb_info;

	srvsdb->vsdb_chans[0] = cur;
	srvsdb->vsdb_chans[1] = next;
}
