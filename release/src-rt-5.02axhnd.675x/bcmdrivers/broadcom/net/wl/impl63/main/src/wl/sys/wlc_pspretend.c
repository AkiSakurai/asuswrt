/**
 * PS Pretend feature (Part of Zero Packet Lose).
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_pspretend.c 781043 2019-11-08 13:50:24Z $
 */

/* Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ap.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_tx.h>
#include <wlc_txc.h>
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#include <wlc_csrestrict.h>
#include <wlc_pspretend.h>

/* module private info */
struct wlc_pps_info {
	wlc_info_t *wlc;
	osl_t *osh;
	int scb_handle;
	struct  wl_timer *ps_pretend_probe_timer;
	bool is_ps_pretend_probe_timer_running;
};

/* per scb info */
typedef struct {
	uint32 ps_pretend_start;
	uint32 ps_pretend_probe;
	uint32 ps_pretend_count;
	uint8  ps_pretend_succ_count;
	uint8  ps_pretend_failed_ack_count;
#ifdef BCMDBG
	uint32 ps_pretend_total_time_in_pps;
	uint32 ps_pretend_suppress_count;
	uint32 ps_pretend_suppress_index;
#endif /* BCMDBG */
} scb_pps_info_t;

#define SCB_PPSINFO_LOC(pps, scb) (scb_pps_info_t **)SCB_CUBBY(scb, (pps)->scb_handle)
#define SCB_PPSINFO(pps, scb) *SCB_PPSINFO_LOC(pps, scb)

/* IOVar table */
enum {
	IOV_PSPRETEND_THRESHOLD = 1,
	IOV_PSPRETEND_RETRY_LIMIT = 2,
	IOV_LAST
};

static const bcm_iovar_t pps_iovars[] = {
	{"pspretend_threshold", IOV_PSPRETEND_THRESHOLD, 0, 0, IOVT_UINT8, 0},
	{"pspretend_retry_limit", IOV_PSPRETEND_RETRY_LIMIT, 0, 0, IOVT_UINT8, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* local function declarations */
/* wlc module */
static int wlc_pspretend_down(void *ctx);
static int wlc_pspretend_doiovar(void *hdl, uint32 aid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);

/* scb cubby */
static int wlc_pspretend_scb_init(void *ctx, struct scb *scb);
static void wlc_pspretend_scb_deinit(void *ctx, struct scb *scb);
static uint wlc_pspretend_scb_secsz(void *ctx, struct scb *scb);
#ifdef BCMDBG
static void wlc_pspretend_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_pspretend_scb_dump NULL
#endif // endif

/* others */
static bool wlc_pspretend_doprobe(wlc_pps_info_t *pps, struct scb *scb, uint32 elapsed_time);
static void wlc_pspretend_probe(void *arg);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* attach/detach */
wlc_pps_info_t *
BCMATTACHFN(wlc_pspretend_attach)(wlc_info_t *wlc)
{
	scb_cubby_params_t cubby_params;
	wlc_pps_info_t *pps_info;

	pps_info = MALLOCZ(wlc->osh, sizeof(*pps_info));
	if (pps_info == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	pps_info->wlc = wlc;
	pps_info->osh = wlc->osh;

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = pps_info;
	cubby_params.fn_init = wlc_pspretend_scb_init;
	cubby_params.fn_deinit = wlc_pspretend_scb_deinit;
	cubby_params.fn_dump = wlc_pspretend_scb_dump;
	cubby_params.fn_secsz = wlc_pspretend_scb_secsz;

	pps_info->scb_handle = wlc_scb_cubby_reserve_ext(wlc,
	                                                 sizeof(scb_pps_info_t *),
	                                                 &cubby_params);

	if (pps_info->scb_handle < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	pps_info->ps_pretend_probe_timer =
	        wl_init_timer(wlc->wl, wlc_pspretend_probe, pps_info, "ps_pretend_probe");
	if (pps_info->ps_pretend_probe_timer == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for ps_pretend_probe_timer failed\n",
		           wlc->pub->unit));
		goto fail;
	}
	pps_info->is_ps_pretend_probe_timer_running = FALSE;

	if (wlc_module_register(wlc->pub, pps_iovars, "pspretend", pps_info, wlc_pspretend_doiovar,
	                        NULL, NULL, wlc_pspretend_down) != BCME_OK) {
		WL_ERROR(("%s: wlc_module_register failed\n", __FUNCTION__));
		goto fail;
	}

	return pps_info;

fail:
	wlc_pspretend_detach(pps_info);
	return NULL;
}

void
BCMATTACHFN(wlc_pspretend_detach)(wlc_pps_info_t* pps_info)
{
	wlc_info_t *wlc;

	if (!pps_info) {
		return;
	}

	wlc = pps_info->wlc;

	wlc_module_unregister(wlc->pub, "pspretend", pps_info);

	if (pps_info->ps_pretend_probe_timer) {
		wl_free_timer(wlc->wl, pps_info->ps_pretend_probe_timer);
	}

	MFREE(wlc->osh, pps_info, sizeof(*pps_info));
}

/* module callbacks */
static int
wlc_pspretend_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *) hdl;
	wlc_info_t *wlc = pps->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_PSPRETEND_THRESHOLD):
		*ret_int_ptr = (int32)bsscfg->ps_pretend_threshold;
		break;

	case IOV_SVAL(IOV_PSPRETEND_THRESHOLD):
		bsscfg->ps_pretend_threshold = (uint8)int_val;
		if (WLC_TXC_ENAB(wlc)) {
			wlc_txc_inv_all(wlc->txc);
		}
		break;

	case IOV_GVAL(IOV_PSPRETEND_RETRY_LIMIT):
		*ret_int_ptr = (int32)bsscfg->ps_pretend_retry_limit;
		break;

	case IOV_SVAL(IOV_PSPRETEND_RETRY_LIMIT):
		bsscfg->ps_pretend_retry_limit = (uint8)int_val;
		if (WLC_TXC_ENAB(wlc)) {
			wlc_txc_inv_all(wlc->txc);
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_pspretend_down(void *ctx)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)ctx;
	wlc_info_t *wlc = pps->wlc;
	int callbacks = 0;

	if (pps->is_ps_pretend_probe_timer_running) {
		if (!wl_del_timer(wlc->wl, pps->ps_pretend_probe_timer))
			callbacks++;
		else
			pps->is_ps_pretend_probe_timer_running = FALSE;
	}

	return callbacks;
}

/* scb cubby callbacks */
static int
wlc_pspretend_scb_init(void *ctx, struct scb *scb)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)ctx;
	scb_pps_info_t **ppps_scb = SCB_PPSINFO_LOC(pps, scb);
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	ASSERT(pps_scb == NULL);

	if (!BSSCFG_AP(SCB_BSSCFG(scb)))
		return BCME_OK;

	if ((pps_scb = wlc_scb_sec_cubby_alloc(pps->wlc, scb, sizeof(*pps_scb))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          pps->wlc->pub->unit, __FUNCTION__, MALLOCED(pps->osh)));
		return BCME_NOMEM;
	}
	*ppps_scb = pps_scb;

	scb->ps_pretend = PS_PRETEND_NOT_ACTIVE;

	return BCME_OK;
}

static void
wlc_pspretend_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)ctx;
	scb_pps_info_t **ppps_scb = SCB_PPSINFO_LOC(pps, scb);
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	if (pps_scb != NULL) {
		wlc_scb_sec_cubby_free(pps->wlc, scb, pps_scb);
	}
	*ppps_scb = NULL;
}

static uint
wlc_pspretend_scb_secsz(void *ctx, struct scb *scb)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)ctx;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	if (!BSSCFG_AP(SCB_BSSCFG(scb)))
		return 0;

	return (uint)sizeof(*pps_scb);
}

#ifdef BCMDBG
static void
wlc_pspretend_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)ctx;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	if (pps_scb == NULL)
		return;

	bcm_bprintf(b, " scb-PS pretend is %s (0x%x) count %d nack %d time_in_pps %d ms\n",
	            SCB_PS_PRETEND(scb) ? "on" : "off", scb->ps_pretend,
	            pps_scb->ps_pretend_count, pps_scb->ps_pretend_failed_ack_count,
	            (pps_scb->ps_pretend_total_time_in_pps + 500)/1000);
}
#endif // endif

/* wlc_apps_process_pspretend_status runs at the end of every TX status. It operates
 * the logic of the ps pretend feature so that the ps pretend will operate during the
 * next tx status
 */
void
wlc_pspretend_dotxstatus(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	bool pps_recvd_ack)
{
	wlc_info_t *wlc = pps->wlc;
	scb_pps_info_t *pps_scb;

	ASSERT(PS_PRETEND_ENABLED(cfg));

	pps_scb = SCB_PPSINFO(pps, scb);

	if (pps_scb == NULL)
		return;

	if (!SCB_PS_PRETEND(scb)) {
		if (pps_recvd_ack) {
			/* the following piece of code is the case of normal successful
			 * tx status with everything all correct and normal
			*/

			/* Implement some scheme against successive ps pretend. Each time ps pretend
			 * is activated, the successive ps pretend count was incremented.
			 * This is decremented for every tx status with at least one good ack.
			 */
			if (pps_scb->ps_pretend_succ_count > 0) {
				pps_scb->ps_pretend_succ_count--;
			}

			/* If we had hit the ps pretend retry limit, then we are now preventing
			 * ps pretend. Check to see if 100ms has elapsed since the previous
			 * ps pretend and the count has decremented to zero. This is
			 * some measure that the air conditions are back to normal.
			 * Then we stop preventing ps pretend to return to normal operation.
			 */
			if ((scb->ps_pretend & PS_PRETEND_PREVENT) &&
			    (pps_scb->ps_pretend_succ_count == 0)) {
				/* we have received some successive ack, so we can
				* re-enable ucode pretend ps feature for this scb after 100 ms
				*/
				uint32 ps_pretend_time =
				        R_REG(wlc->osh, D11_TSFTimerLow(wlc)) -
				        pps_scb->ps_pretend_start;

				/* if we are more than 100 ms since last ps pretend */
				if (ps_pretend_time > 100*1000) {
					/* invalidate txc */
					if (WLC_TXC_ENAB(wlc))
						wlc_txc_inv(wlc->txc, scb);

					scb->ps_pretend &= ~PS_PRETEND_PREVENT;

					WL_PS(("wl%d.%d: remove ps pretend prevent for "MACF"\n",
					       wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
					       ETHER_TO_MACF(scb->ea)));
				}
			}

			/* This is the count of consecutive bad tx status. As soon as we get
			 * a good tx status, this count is immediately cleared.
			 * It is used for the threshold mode detection.
			 */
			pps_scb->ps_pretend_failed_ack_count = 0;

			/* if we were previously primed to enter threshold ps pretend due to
			 * previously meeting the threshold target, we can relax because we
			 * have just received a good tx status, so clear these flags
			 */
			if (SCB_PS_PRETEND_THRESHOLD(scb)) {
				scb->ps_pretend &= ~PS_PRETEND_THRESHOLD;

				WL_PS(("wl%d.%d: "MACF" received successful txstatus, "
					   "canceling threshold ps pretend pending\n",
					   wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
					   ETHER_TO_MACF(scb->ea)));
			}
		} else { /* !pps_recvd_ack */
			/* successive failure count */
			if (pps_scb->ps_pretend_failed_ack_count < MAXNBVAL(sizeof(uint8))) {
				pps_scb->ps_pretend_failed_ack_count++;
			}

			/* if we are doing threshold, check if limit is reached, then
			 * enable packets to cause PPS PMQ entry addition upon
			 * noack tx status
			 */
			if (PS_PRETEND_THRESHOLD_ENABLED(cfg) &&
			    (pps_scb->ps_pretend_failed_ack_count >= cfg->ps_pretend_threshold) &&
			    !SCB_PS_PRETEND_THRESHOLD(scb)) {
				scb->ps_pretend |= PS_PRETEND_THRESHOLD;

				WL_PS(("wl%d.%d: "MACF" successive failed txstatus (%d) "
					   "is at threshold (%d) for ps pretend\n",
					   wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
					   ETHER_TO_MACF(scb->ea),
					   pps_scb->ps_pretend_failed_ack_count,
					   cfg->ps_pretend_threshold));
			}
		}

		return;
	}

	/*******************************************************************
	 ****** All code following is relevant if ps pretend is active *****
	 *******************************************************************
	 */

	ASSERT(SCB_PS_PRETEND(scb));

	/* if we did receive a good txstatus, and we are current active in threshold
	 * mode of ps pretend, then we can exit. This can happen if we did not flush
	 * the fifo and packets in transit are still trying to go to air and at least
	 * one of them was successful.
	 * So in that case, make a reservation (by pend bit) for PS off, so the scb will
	 * get out of PM in wlc_apps_process_pend_ps() after txfifo draining is done.
	 */
	if (pps_recvd_ack && SCB_PS_PRETEND_THRESHOLD(scb)) {
#ifdef AP
		wlc_apps_ps_trans_upd(wlc, scb);
#endif // endif
		return;
	}

	/* for normal ps pretend using ucode method, check to see
	* if we hit the pspretend_retry_limit. If so, once the whole
	* fifo has been cleared, we then disable ps pretend for
	* this scb until sometime later when the check
	* for successful tx status re-enables ps pretend
	*/
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		/* using ucode method for ps pretend */

		if (!SCB_PS_PRETEND_THRESHOLD(scb) &&
		    (pps_scb->ps_pretend_succ_count >= cfg->ps_pretend_retry_limit) &&
		    (TXPKTPENDTOT(wlc) == 0)) {

			/* invalidate txc */
			if (WLC_TXC_ENAB(wlc))
				wlc_txc_inv(wlc->txc, scb);

			/* prevent this scb from ucode pretend ps in the
			* future (this is cleared when things return to
			* normal)
			* Wait until tx fifo is empty so that all packets
			* are drained first before disabling ps pretend
			*/
			scb->ps_pretend |= PS_PRETEND_PREVENT;

			WL_PS(("wl%d.%d: "MACF" successive ps pretend (%d) "
			       "exceeds limit (%d), fifo empty\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), ETHER_TO_MACF(scb->ea),
			       pps_scb->ps_pretend_succ_count, cfg->ps_pretend_retry_limit));

			wlc_apps_scb_ps_off(wlc, scb, FALSE);

			return;
		}
	}

	/*
	 * All code following is with ps pretend remaining active without
	 * any transition to non active to be taken
	 */

	/* if we aren't probing this scb yet, it means we only just entered ps pretend
	 * So, send a probe now and begin probing
	 */
	if (!SCB_PS_PRETEND_PROBING(scb)) {
#ifdef AP
		wlc_ap_reset_grace_attempt(wlc->ap, scb, 0);
#endif // endif
		scb->ps_pretend |= PS_PRETEND_PROBING;

		if (!(wlc->block_datafifo & DATA_BLOCK_PS)) {
			/* send a probe immediately if the fifo is empty */
			wlc_pspretend_doprobe(pps, scb, 0);
		}
	}

	/* The probing works on the back of a periodic timer. If that timer isn't
	 * running, then start it.
	 * For the first time interval, do 5 milliseconds to send a followup probe
	 * quickly.
	 */
	if (pps->ps_pretend_probe_timer && !pps->is_ps_pretend_probe_timer_running) {
		wl_add_timer(wlc->wl, pps->ps_pretend_probe_timer, 5, FALSE);
		pps->is_ps_pretend_probe_timer_running = TRUE;
	}
}

/* returns TRUE if we transitioned ps pretend off > on */
bool
wlc_pspretend_on(wlc_pps_info_t *pps, struct scb *scb, uint8 flags)
{
	wlc_info_t *wlc = pps->wlc;
	bool ps_retry = FALSE;

	if (SCB_INTERNAL(scb)) {
		return ps_retry;
	}

	ASSERT(!SCB_PS(scb));

	if (SCB_LEGACY_WDS(scb) && !(scb->flags & SCB_WDS_LINKUP)) {
		/* Do not enter into pspretend if WDS link is down */
		WL_PS(("wl%d.%d: "MACF" skipping pspretend_on since WDS link is down\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), ETHER_TO_MACF(scb->ea)));
		return ps_retry;
	}

#ifdef AP
	wlc_apps_scb_ps_on(wlc, scb);
#endif // endif

	/* check PS status in case transition did not work */
	if (SCB_PS(scb)) {
		scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

		ps_retry = TRUE;

		/* record entry time for pps */
		pps_scb->ps_pretend_start = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

		scb->ps_pretend |= (PS_PRETEND_ACTIVE | PS_PRETEND_RECENT | flags);

		/* count of successive ps pretend transitions */
		pps_scb->ps_pretend_count++;
		pps_scb->ps_pretend_succ_count++;

		WL_PS(("wl%d.%d: "MACF" pretend PS retry mode now "
		       "active %s current PMQ PPS entry, count %d/%d\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), ETHER_TO_MACF(scb->ea),
		       (scb->ps_pretend & PS_PRETEND_ACTIVE_PMQ) ? "with" : "without",
		       pps_scb->ps_pretend_succ_count, pps_scb->ps_pretend_count));
	}
	return ps_retry;
}

/* send pspretend probe (null data) if necessary */
static bool
wlc_pspretend_doprobe(wlc_pps_info_t *pps, struct scb *scb, uint32 elapsed_time)
{
	wlc_info_t *wlc = pps->wlc;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	uint32 listen_in_ms = wlc_apps_get_listen_prd(wlc, scb) *
	        cfg->current_bss->beacon_period;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	/* Ensure we are going to probe during up to 5s */
	listen_in_ms = MAX(listen_in_ms, 5000);

	/* this increments each time we are called from the pretend probe timer,
	 * approximately once per 10ms
	 */
	pps_scb->ps_pretend_probe++;

	/* If a probe is still pending, don't send another one */
	if (scb->flags & SCB_PSPRETEND_PROBE) {
		return FALSE;
	}

	/* As time goes by, we probe less often - this function is called
	 * once per timer interval (10ms) per SCB in pps state, so
	 * we return early if we decide to skip
	 */
	if ((elapsed_time >= listen_in_ms) &&
#ifdef BCMDBG
	    !(wlc->block_datafifo & DATA_BLOCK_PS) &&
#endif // endif
	    TRUE) {
		/* there's no more point to sending probes, the
		 * destination has probably died. Note that the TIM
		 * is set indicating ps state, so after pretend probing
		 * has completed, there is still the beacon to awaken the STA
		 */
		scb->ps_pretend &= ~PS_PRETEND_PROBING;
#ifdef AP
		/* reset grace_attempts but indicate we have done some probing */
		wlc_ap_reset_grace_attempt(wlc->ap, scb, 1);
#endif // endif
		return FALSE;
	} else if (wlc_scb_restrict_do_probe(scb)) {
		/* probing requested  */
	} else if (elapsed_time > 600) {
		/* after 0.6s, only probe every 5th timer, approx 50 ms */
		if ((pps_scb->ps_pretend_probe % 5) != 0) {
			return FALSE;
		}
	} else if (elapsed_time > 300) {
		/* after 0.3s, only probe every 3rd timer, approx 30ms */
		if ((pps_scb->ps_pretend_probe % 3) != 0) {
			return FALSE;
		}
	} else if (elapsed_time > 150) {
		/* after 0.15s, only probe every 2nd timer, approx 20ms */
		if ((pps_scb->ps_pretend_probe % 2) != 0) {
			return FALSE;
		}
	}
	/* else we probe each time (10ms) */

	WL_PS(("time since pps is %dms\n", elapsed_time));
#ifdef AP
	return wlc_ap_sendnulldata(wlc->ap, cfg, scb);
#else
	return FALSE;
#endif // endif
}

/* pspretend probe timer callback */
static void
wlc_pspretend_probe(void *arg)
{
	wlc_pps_info_t *pps = (wlc_pps_info_t *)arg;
	wlc_info_t *wlc = pps->wlc;

	pps->is_ps_pretend_probe_timer_running = FALSE;

	/* double check in case driver went down since timer was scheduled */
	if (wlc->pub->up && wlc->state != WLC_STATE_GOING_DOWN) {
		struct scb_iter scbiter;
		struct scb *scb;
		bool        ps_pretend_probe_continue = FALSE;
		uint32      tsf_low = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
			if (pps_scb == NULL)
				continue;
			if (SCB_PS_PRETEND_PROBING(scb)) {
				uint32 pretend_elapsed = (tsf_low - pps_scb->ps_pretend_start)/1000;
				ps_pretend_probe_continue = TRUE;
				wlc_pspretend_doprobe(pps, scb, pretend_elapsed);
			} else {
				pps_scb->ps_pretend_probe = 0;
			}
		}

		/* retstart timer as long as at least one scb is in pretend state */
		if (ps_pretend_probe_continue) {
			/* 10 milliseconds */
			wl_add_timer(wlc->wl, pps->ps_pretend_probe_timer,
			             10, FALSE);
			pps->is_ps_pretend_probe_timer_running = TRUE;
		}
	}
}

/* schedule pspretend probe at a future time */
void
wlc_pspretend_probe_sched(wlc_pps_info_t *pps, struct scb *scb)
{
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
	wlc_info_t *wlc = pps->wlc;
	uint32 elapsed;

	if (pps_scb == NULL)
		return;

	elapsed = (R_REG(wlc->osh, D11_TSFTimerLow(wlc)) - pps_scb->ps_pretend_start)/1000;
	(void)wlc_pspretend_doprobe(pps, scb, elapsed);
}

bool
wlc_pspretend_pkt_retry(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	void *pkt, tx_status_t *txs, d11txhdr_t *txh, uint16 macCtlHigh)
{
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
	wlc_pkttag_t *pkttag = WLPKTTAG(pkt);

	/* in case we haven't yet processed PMQ status change because of
	 * previously blocked data path or delayed PMQ interrupt
	 */
	if (!SCB_PS(scb)) {
		wlc_pspretend_on(pps, scb, 0);
	}
	ASSERT(SCB_PS(scb));

	/* check to see if successive pps transitions exceed limit */
	if (pps_scb->ps_pretend_succ_count >= cfg->ps_pretend_retry_limit) {
		uint16 *machigh = D11_TXH_GET_MACHIGH_PTR(pps->wlc, txh);

		/* The retry limit has been hit, so clear the PPS bit so
		 * they no longer trigger another ps pretend.
		 */

		macCtlHigh &= ~D11AC_TXC_PPS;
		*machigh = htol16(macCtlHigh);
	}
	if ((pkttag->flags & WLF_TXHDR) &&
	    !(macCtlHigh & D11AC_TXC_FIX_RATE)) {
		/* XXX To do. Re-calculate (or else remove completely)
		 * the tx hdr, because rate information may no longer
		 * be appropriate
		 */
	}

	return wlc_apps_suppr_frame_enq(pps->wlc, pkt, txs, TRUE);
}

void
wlc_pspretend_rate_reset(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	uint8 ac)
{
	wlc_info_t *wlc = pps->wlc;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	/* XXX if we were reprogramming tx header between ps pretend transmissions,
	 * we would do this rate reset before reaching the retry limit
	 */
	if (pps_scb->ps_pretend_succ_count >= cfg->ps_pretend_retry_limit) {

		WL_PS(("wl%d.%d: "MACF" ps pretend rate adaptation (count %d)\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		       ETHER_TO_MACF(scb->ea),
		       pps_scb->ps_pretend_succ_count));
		wlc_scb_ratesel_rfbr(wlc->wrsi, scb, ac);
	}
}

bool
wlc_pspretend_pkt_relist(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	int retry_limit, int txretry)
{
	int max_ps_retry = retry_limit + cfg->ps_pretend_retry_limit;

	if (max_ps_retry > (uint8)(-1)) {
		max_ps_retry = (uint8)(-1);
	}

	return (((txretry >= retry_limit) && (txretry < max_ps_retry)) ||
	        SCB_PS_PRETEND_THRESHOLD(scb)) &&
	       !SCB_PS_PRETEND_NORMALPS(scb);
}

#ifdef BCMDBG
void
wlc_pspretend_supr_upd(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	uint supr_status)
{
	wlc_info_t *wlc = pps->wlc;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	if (supr_status == TX_STATUS_SUPR_PPS) {
		/* if this is the first packet to be suppressed, record the
		 * ps pretend instance where we start suppressing packets
		 */
		if (pps_scb->ps_pretend_suppress_count == 0) {
			pps_scb->ps_pretend_suppress_index = pps_scb->ps_pretend_count;
		}
		/* increment packet suppression counter */
		pps_scb->ps_pretend_suppress_count++;
	} else if (supr_status) {
		/* other suppression started while pspretend is still active */
	} else if (pps_scb->ps_pretend_suppress_count > 0) {
		if (SCB_PS_PRETEND(scb) && (wlc->block_datafifo & DATA_BLOCK_PS) &&
		    (pps_scb->ps_pretend_suppress_index == pps_scb->ps_pretend_count)) {
			WL_INFORM(("wl%d.%d: "MACF" packet not suppressed when fifo still draining "
			          "(%d drained/%d remaining)%s\n", wlc->pub->unit,
			          WLC_BSSCFG_IDX(cfg),
			          ETHER_TO_MACF(scb->ea), pps_scb->ps_pretend_suppress_count,
			          TXPKTPENDTOT(wlc),
			          SCB_PS_PRETEND_BLOCKED(scb) ? "  BLOCKED":""));
		}
		pps_scb->ps_pretend_suppress_count = 0;
	}
}
#endif /* BCMDBG */

/* scb->ps_pretend_start contains the TSF time for when ps pretend was
 * last activated. In poor link conditions, there is a high chance that
 * ps pretend will be re-triggered.
 * Within a short time window following ps pretend, this code is trying to
 * constrain the number of packets in transit and so avoid a large number of
 * packets repetitively going between transmit and ps mode.
 */
bool
wlc_pspretend_limit_transit(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg, struct scb *scb,
	int in_transit, bool agg_enab)
{
	wlc_info_t *wlc = pps->wlc;
	bool ps_pretend_limit_transit = FALSE;
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
	/* time elapsed in us since last pretend ps event */
	uint32 ps_pretend_time =
	        R_REG(wlc->osh, D11_TSFTimerLow(wlc)) - pps_scb->ps_pretend_start;

	/* arbitrary calculation, roughly we don't allow in_transit to be big
	 * when we recently have done a pretend ps. As time goes by, we gradually
	 * allow in_transit to increase. There could be some tuning to do for this.
	 * This is to prevent a large in_transit in the case where we are performing
	 * pretend ps regularly (of order every second or faster), as the time
	 * required to suppress a large number of packets in transit can cause
	 * the data path to choke.
	 * When pretend ps is currently active, we always prevent packet release.
	 */
	if (agg_enab) {
		if (in_transit > (40 + (ps_pretend_time >> 13 /* divide 8192 */))) {
			ps_pretend_limit_transit = TRUE;
		}
	} else {
		 /* Tuning mechanism is same as Aggegrated traffic but tuning parameters are
		 * 8 times lower.
		 */
		if (in_transit > (5 + (ps_pretend_time >> 16 /* divide 65536 */))) {
			ps_pretend_limit_transit = TRUE;
		}
	}

	if ((ps_pretend_time >> 10 /* divide 1024 */) > 2000) {
		/* After 2 seconds (approx) elapsed since last ps pretend, clear
		 * recent flag to avoid doing this calculation
		 * This is the only place where PS_PRETEND_RECENT is cleared.
		 */
		scb->ps_pretend &= ~PS_PRETEND_RECENT;
	}

	return ps_pretend_limit_transit;
}

void
wlc_pspretend_scb_ps_off(wlc_pps_info_t *pps, struct scb *scb)
{
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

	if (pps_scb == NULL)
		return;

	scb->ps_pretend &= ~PS_PRETEND_ON;
	pps_scb->ps_pretend_failed_ack_count = 0;
}

#ifdef BCMDBG
void
wlc_pspretend_scb_time_upd(wlc_pps_info_t *pps, struct scb *scb)
{
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
	wlc_info_t *wlc = pps->wlc;
	uint32 time_in_pretend;

	if (pps_scb == NULL)
		return;

	time_in_pretend = R_REG(wlc->osh, D11_TSFTimerLow(wlc)) - pps_scb->ps_pretend_start;
	pps_scb->ps_pretend_total_time_in_pps += time_in_pretend;

	WL_PS(("wl%d.%d: ps pretend state about to exit, %d ms in pretend state\n",
	       wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), (time_in_pretend + 500)/1000));
}

uint
wlc_pspretend_scb_time_get(wlc_pps_info_t *pps, struct scb *scb)
{
	scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);
	return pps_scb->ps_pretend_total_time_in_pps;
}
#endif /* BCMDBG */

/* When sending the CSA, packets are going to be dropped somewhere in the process as
 * the radio channel changes and the packets in transit are still set up for the
 * original channel.
 * PS pretend would try to save these packets but this is difficult to coordinate.
 * For now, it is better to disable ps pretend in the short time window around CSA.
 * It is useful to 'fake' a ps pretend event hitting the ps pretend retry limit,
 * as the ps pretend will be reactivated when the channel conditions become
 * normal again.
 */
void
wlc_pspretend_csa_upd(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = pps->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	ASSERT(PS_PRETEND_ENABLED(cfg));

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {

		if (!SCB_INTERNAL(scb)) {
			scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

			/* set 'now' time for ps pretend so that we can delay it being
			 * re-enabled. Note that ps pretend may not actually have happened.
			 */
			pps_scb->ps_pretend_start = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

			/* disable ps pretend (if not current engaged) */
			if (!SCB_PS_PRETEND(scb)) {
				/* invalidate txc */
				if (WLC_TXC_ENAB(wlc))
					wlc_txc_inv(wlc->txc, scb);

				/* also set the PS_PRETEND_RECENT flag. This is going to
				 * limit packet release prior to the CSA.
				 */
				scb->ps_pretend |= (PS_PRETEND_PREVENT | PS_PRETEND_RECENT);

				WL_PS(("wl%d.%d: %s: preventing ps pretend for "MACF"\n",
				       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				       ETHER_TO_MACF(scb->ea)));
			}
			/* move us to the retry limit for the scb, so packets will be no
			 * longer saved. If ps pretend was already triggered, this will
			 * add the PS_PRETEND_PREVENT flag as the retry limit is now hit.
			 */
			pps_scb->ps_pretend_succ_count = cfg->ps_pretend_retry_limit;
		}
	}
}

/* After data path is enabled after CSA, enable psPretend */
void
wlc_pspretend_csa_start(wlc_pps_info_t *pps, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = pps->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	ASSERT(PS_PRETEND_ENABLED(cfg));

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {

		if (!SCB_INTERNAL(scb)) {
			scb_pps_info_t *pps_scb = SCB_PPSINFO(pps, scb);

			pps_scb->ps_pretend_start = R_REG(wlc->osh, D11_TSFTimerLow(wlc));

			/* Enable ps pretend (if not current engaged) */
			if (!SCB_PS_PRETEND(scb)) {
				/* invalidate txc */
				if (WLC_TXC_ENAB(wlc))
					wlc_txc_inv(wlc->txc, scb);

				scb->ps_pretend &= ~PS_PRETEND_PREVENT;

				WL_PS(("wl%d.%d: %s: Enabling ps pretend for "MACF"\n",
				       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				       ETHER_TO_MACF(scb->ea)));
			}
			pps_scb->ps_pretend_succ_count = 0;
		}
	}
}

bool
wlc_pspretend_pps_retry(wlc_info_t *wlc, struct scb *scb, void *p, tx_status_t *txs)
{
	bool pps_retry = FALSE;
	uint supr_status = txs->status.suppr_ind;
	d11txhdr_t* txh = NULL;
	uint16	macCtlHigh = 0;

	/* quite complex logic to decide to save the packet with ps pretend
	 *
	 * Test to save the packet with ps pretend (pps_retry flag).....
	 * 1. we are not in ordinary PS mode
	 * 2. suppressed for reasons other ps pretend, forget about saving it unless
	 *    we are already in PS pretend active state, so we might as well save it
	 * 3. ps pretend has got to be globally enabled & the scb has not to be excluded
	 *    from ps pretend
	 * 4. the packet has to have the D11AC_TXC_PPS bit set if we doing normal ps
	 *    pretend. This is regardless of whether TX_STATUS_SUPR_PPS is true; if
	 *    not suppressed it means this is the first tx status with the initial
	 *    transmission failure.
	 * 5. or we have reached the threshold in case of threshold mode
	 * ...... then we save the packet with ps pretend, and mark it for retry
	 *
	 * This 'if..else if' construct is designed to test the most common cases
	 * first and then break out of the logic at the earliest case. Even in the
	 * case we set pps_retry to FALSE, we are needing that case to prevent
	 * moving through the rest of the logic.
	 *
	 */
	if (SCB_PS_PRETEND_NORMALPS(scb)) {
		/* 1 */
		pps_retry = FALSE;
	}
	else if (supr_status && (supr_status != TX_STATUS_SUPR_PPS) &&
			!SCB_PS_PRETEND(scb)) {
		/* 2 */
		pps_retry = FALSE;
	}
	else if (SCB_PS_PRETEND_ENABLED(SCB_BSSCFG(scb), scb)) {
		/* 3 */
		wlc_pkt_get_txh_hdr(wlc, p, &txh);
		macCtlHigh = ltoh16(*D11_TXH_GET_MACHIGH_PTR(wlc, txh));

		if (macCtlHigh & D11AC_TXC_PPS) {
			/* 4 */
			pps_retry = TRUE;
		}
		else {
			/* packet does not have PPS bit set.
			 * We normally get here if we previously
			 * exceeded the pspretend_retry_limit and the packet subsequently
			 * fails to be sent
			 */
			pps_retry = FALSE;
		}
	}
	else if (SCB_PS_PRETEND_THRESHOLD(scb)) {
		/* 5 */
		pps_retry = TRUE;
	} else if (SCB_PS_PRETEND_CSA_PREVENT(scb, SCB_BSSCFG(scb)) && !supr_status) {
		/* During the period around CSA, PSP is explicitly turned off.
		 * Many packets during this time may get dropped due to no
		 * suppression and no ack. To meet ZPL requirements, these
		 * packets should not be dropped but rather explicitly re-fetched
		 * and re-tried.
		 */
		pps_retry = TRUE;
	}
	return pps_retry;
}
