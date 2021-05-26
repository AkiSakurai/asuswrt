/*
 * Keep-alive offloading.
 * @file
 * @brief
 * This feature implements periodic keep-alive packet transmission offloading.
 * The intended purpose is to keep an active session within a network address
 * translator (NAT) with a public server. This allows incoming packets sent
 * by the public server to the STA to traverse the NAT.
 *
 * An example application is to keep an active session between the STA and
 * a call control server in order for the STA to be able to receive incoming
 * voice calls.
 *
 * The keep-alive functionality is offloaded from the host processor to the
 * WLAN processor to eliminate the need for the host processor to wake-up while
 * it is idle; therefore, conserving power.
 *
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
 * $Id: wl_keep_alive.c 783417 2020-01-28 10:36:24Z $
 *
 */

/* ---- Include Files ---------------------------------------------------- */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc_pio.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <ethernet.h>

#include <wl_export.h>
#include <wl_keep_alive.h>
#include <wlc_scb.h>
#include <wlc_pcb.h>
#include <wlc_wnm.h>

/* ---- Public Variables ------------------------------------------------- */
/* ---- Private Constants and Types -------------------------------------- */

#define LOCAL_DEBUG	0

#if LOCAL_DEBUG
#undef WL_TRACE
#define WL_TRACE(arg)	printf arg
#endif // endif

/* wlc_pub_t struct access macros */
#define WLCUNIT(info)	((info)->wlc->pub->unit)
#define WLCOSH(info)	((info)->wlc->osh)

/* Lower bound for periodic transmit interval. */
#define MIN_PERIOD_MSEC		300

/* IOVar table */
enum {
	/* Get/set keep-alive packet and retransmission interval. */
	IOV_KEEP_ALIVE,
	IOV_MKEEP_ALIVE
};

typedef struct mkeep_alive_indiv {
	uint32			period_msec;
	/* Size, in bytes, of packet to transmit. */
	uint16			len_bytes;
	/* Flags field used for managing retries and inflight */
	uint8			rflags;
	uint8			pad;
	/* Variable length array of packet to transmit. Packet contents should include */
	/* the entire ethernet packet (ethernet header, IP header, UDP header, and UDP */
	/* payload) specified in network byte order.                                   */
	char			*pkt_data;
	/* Periodic timer used to transmit packet. */
	struct wl_timer		*timer;
	wlc_bsscfg_t* bsscfg;
	uint32			override_period_msec;
} mkeep_alive_indiv_t;

/* Bits/fields for rflags:
 * - TXSWAIT: waiting for txstatus callback
 * - RETRY:   in retry mode (force retry period)
 * - CNTMASK: mask for count of [remaining] retries
 */
#define MKEEP_ALIVE_RFLAG_TXSWAIT	0x80
#define MKEEP_ALIVE_RFLAG_RETRY		0x40
#define MKEEP_ALIVE_RFLAG_CNTMASK	0x0f

/* Retry period (ms) */
#define MKEEP_ALIVE_RETRY_PERIOD	500

typedef struct keep_alive_cmn {
	mkeep_alive_indiv_t *mkeep_alive;
	int keep_alive_count;
} keep_alive_cmn_t;

/* Keep-alive private info structure. */
struct wl_keep_alive_info {
	wlc_info_t *wlc;
	keep_alive_cmn_t *ka_cmn;	/* common structure */
	int apppkt_malloc_failed;
	uint16 txoff;
	uint8 rcnt;
};
/* ---- Private Variables ------------------------------------------------ */

static const bcm_iovar_t keep_alive_iovars[] = {
	{
		"keep_alive",
		IOV_KEEP_ALIVE,
		(0), 0,
		IOVT_BUFFER,
		WL_KEEP_ALIVE_FIXED_LEN
	},
	{
		"mkeep_alive",
		IOV_MKEEP_ALIVE,
		(0), 0,
		IOVT_BUFFER,
		WL_MKEEP_ALIVE_FIXED_LEN
	},

	{NULL, 0, 0, 0, 0, 0 }
};

/* ---- Private Function Prototypes -------------------------------------- */

static int keep_alive_doiovar
(
	void	*hdl,
	uint32	actionid,
	void	*p,
	uint	plen,
	void	*a,
	uint	alen,
	uint	vsize,
	struct wlc_if	*wlcif
);

static void keep_alive_timer_callback(void *arg);
static void keep_alive_timer_update(mkeep_alive_indiv_t *indiv, int period_msec, bool enable);
static int  mkeep_alive_set(mkeep_alive_indiv_t* indiv,
	                    int len_bytes,
	                    int period_msec,
	                    uint8 *pkt_data,
	                    wlc_bsscfg_t *bss);
static void wl_keep_alive_txcb(wlc_info_t *wlc, uint txstatus, void *arg);
static void wlc_mkeepalive_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);

/* ---- Functions -------------------------------------------------------- */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>
#define MKEEPALIVE_CUBBY_CONFIG_SIZE (sizeof(uint8) * (WL_MKEEP_ALIVE_IDMAX + 1))

/* ----------------------------------------------------------------------- */
wl_keep_alive_info_t *
BCMATTACHFN(wl_keep_alive_attach)(wlc_info_t *wlc)
{
	wl_keep_alive_info_t *info;
	keep_alive_cmn_t *ka_cmn = NULL;
	int i;
	int keep_alive_count = WL_MKEEP_ALIVE_IDMAX + 1;

	/* Allocate keep-alive private info struct. */
	info = MALLOCZ(wlc->pub->osh, sizeof(*info));
	if (info == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Init keep-alive private info struct. */
	info->wlc = wlc;
	info->txoff = wlc->txhroff;
	info->rcnt = MKEEP_ALIVE_RFLAG_CNTMASK;
	wlc->keepalive = info;
	ka_cmn = info->ka_cmn = (keep_alive_cmn_t *)MALLOCZ(wlc->pub->osh, sizeof(*ka_cmn));
	if (ka_cmn == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	ka_cmn->keep_alive_count = keep_alive_count;
	ka_cmn->mkeep_alive = (mkeep_alive_indiv_t *)
		MALLOCZ(wlc->pub->osh,
		((keep_alive_count)*sizeof(*ka_cmn->mkeep_alive)));
	if (ka_cmn->mkeep_alive == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Create periodic timer for packet transmission. */
	for (i = 0; i < ka_cmn->keep_alive_count; i++) {
		ka_cmn->mkeep_alive[i].timer = wl_init_timer(wlc->wl,
			keep_alive_timer_callback,
			&ka_cmn->mkeep_alive[i],
			"mkeep_alive");

		if (ka_cmn->mkeep_alive[i].timer == NULL) {
			WL_ERROR(("wl%d: %s: wl_init_timer failed\n", wlc->pub->unit,
			__FUNCTION__));
			goto fail;
		}
	}

	/* Register keep-alive module. */
	if (wlc_module_register(wlc->pub,
	                        keep_alive_iovars,
	                        "keep_alive",
	                        info,
	                        keep_alive_doiovar,
	                        NULL,
	                        NULL,
	                        NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Add client callback to the scb state notification list */
	if ((wlc_scb_state_upd_register(wlc, (scb_state_upd_cb_t)wlc_mkeepalive_scb_state_upd_cb,
		(void *)ka_cmn->mkeep_alive)) != BCME_OK) {

		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
			wlc->pub->unit, __FUNCTION__, wlc_mkeepalive_scb_state_upd_cb));

		goto fail;

	}
	wlc->pub->cmn->_keep_alive = TRUE;
	return (info);

fail:
	MODULE_DETACH(info, wl_keep_alive_detach);
	return NULL;
}
/* ----------------------------------------------------------------------- */
void
BCMATTACHFN(wl_keep_alive_detach)(wl_keep_alive_info_t *info)
{
	int i;
	keep_alive_cmn_t *ka_cmn = NULL;

	if (info == NULL)
		return;
	ka_cmn = info->ka_cmn;
	if (ka_cmn) {
		if (ka_cmn->mkeep_alive) {
			for (i = 0; i < ka_cmn->keep_alive_count; i++) {
				if (ka_cmn->mkeep_alive[i].timer != NULL) {
					wl_free_timer(info->wlc->wl, ka_cmn->mkeep_alive[i].timer);
					ka_cmn->mkeep_alive[i].timer = NULL;
				}
				if (ka_cmn->mkeep_alive[i].pkt_data != NULL) {
					MFREE(WLCOSH(info),
					ka_cmn->mkeep_alive[i].pkt_data,
					ka_cmn->mkeep_alive[i].len_bytes);
				}
			}
			MFREE(WLCOSH(info), ka_cmn->mkeep_alive,
					(ka_cmn->keep_alive_count)*sizeof(mkeep_alive_indiv_t));
		}
		MFREE(WLCOSH(info), ka_cmn, sizeof(*ka_cmn));
		info->wlc->pub->cmn->_keep_alive = FALSE;
	}
	MFREE(WLCOSH(info), info, sizeof(wl_keep_alive_info_t));
}
/* ----------------------------------------------------------------------- */

static void
wlc_mkeepalive_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	mkeep_alive_indiv_t *mkeep_alive = (mkeep_alive_indiv_t*)ctx;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	wlc_info_t *wlc;
	uint8 oldstate = notif_data->oldstate;
	wl_keep_alive_info_t *keepalive;
	int i;
	mkeep_alive_indiv_t *indiv;
	uint32 period;

	scb = notif_data->scb;
	bsscfg = scb->bsscfg;
	wlc = bsscfg->wlc;
	keepalive = wlc->keepalive;
	/* Check if the state transited SCB is internal.
	 * In that case we have to do nothing, just return back
	 */
	if (SCB_INTERNAL(scb))
		return;

	for (i = 0; i < keepalive->ka_cmn->keep_alive_count; i++) {
		indiv = &mkeep_alive[i];
		if (indiv->bsscfg == bsscfg) {
			/* if override is ON configure with that timeout */
			period = indiv->override_period_msec ?
				indiv->override_period_msec : indiv->period_msec;
			if (!(oldstate & ASSOCIATED) && SCB_ASSOCIATED(scb)) {
				if ((period != 0) &&
#ifdef WLWNM
				(WLWNM_ENAB(wlc->pub)&&
				wlc_wnm_maxidle_upd_reqd(wlc, scb)) &&
#endif /* WLWNM */
				TRUE) {
					keep_alive_timer_update(indiv, period, TRUE);
				} else if (indiv->period_msec &&
#ifdef WLWNM
				(WLWNM_ENAB(wlc->pub) &&
				!wlc_wnm_maxidle_upd_reqd(wlc, scb)) &&
#endif /* WLWNM */
				TRUE) {
				/* Reset to default timeout value */
					indiv->override_period_msec = 0;
					keep_alive_timer_update(indiv,
					indiv->period_msec, TRUE);
				}
			}
			else if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb) &&
				!WLC_BSS_CONNECTED(bsscfg)) {
			/* if disconnect .. remove timer and reset over-ride duration */
				wl_del_timer(wlc->wl, indiv->timer);
				indiv->override_period_msec = 0;
			}
		}
	}
}

static int
mkeep_alive_set(mkeep_alive_indiv_t* indiv,
	int len_bytes,
	int period_msec,
	uint8 *pkt_data,
	wlc_bsscfg_t *bss)
{
	int err = BCME_OK;
	uint32 override_period = indiv->override_period_msec;
	bool immediate = FALSE;

	/* Validate ethernet packet length. */
	if ((len_bytes != 0) && (len_bytes < ETHER_HDR_LEN)) {
		return BCME_BADARG;
	}

	/* Decompose arg to immediate send flag and period */
	if (period_msec & WL_MKEEP_ALIVE_IMMEDIATE) {
		immediate = TRUE;
		period_msec &= WL_MKEEP_ALIVE_PERIOD_MASK;
	}

	/* Validate transmit period is greater than min bound. */
	if ((period_msec != 0) && (period_msec < MIN_PERIOD_MSEC)) {
		return BCME_RANGE;
	}
	if (bss == NULL)
		return BCME_BADARG;
	if (!BSSCFG_STA(bss))
		return BCME_BADARG;
	if (override_period && (len_bytes == 0))
		return BCME_BADARG;

	indiv->bsscfg = bss;
	/* Disable timer while we modify timer state to avoid race-conditions. */
	keep_alive_timer_update(indiv, indiv->period_msec, FALSE);

	/* disable this entry */
	indiv->period_msec = 0;

	/* Free memory used to store user specified packet data contents. */
	if (indiv->pkt_data != NULL) {
		MFREE(WLCOSH(bss), indiv->pkt_data, indiv->len_bytes);
		indiv->pkt_data = NULL;
	}

	if (0 != len_bytes)
	{
		/* Allocate memory used to store user specified packet data contents. */
		indiv->pkt_data = MALLOC(WLCOSH(bss), len_bytes);
		if (indiv->pkt_data == NULL) {
			WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			WLCUNIT(bss), __FUNCTION__, MALLOCED(WLCOSH(bss))));
			indiv->bsscfg = NULL;
			return BCME_NOMEM;
		}
		bcopy(pkt_data, indiv->pkt_data, len_bytes);
	}

	/* Store new timer and packet attributes. */
	bcopy(&len_bytes, &indiv->len_bytes, sizeof(len_bytes));
	bcopy(&period_msec, &indiv->period_msec, sizeof(period_msec));

	if (override_period)
		period_msec = override_period;

	/* Start the periodic timer with new values if BSS associated */
	if (period_msec && WLC_BSS_CONNECTED(bss)) {
		/* Start the periodic timer with new values. */
		keep_alive_timer_update(indiv, period_msec, TRUE);

		if (immediate) {
			keep_alive_timer_callback(indiv);
		}
	}

	return err;
}

/*
*****************************************************************************
* Function:   keep_alive_doiovar
*
* Purpose:    Handles keep-alive related IOVars.
*
* Parameters:
*
* Returns:    0 on success.
*****************************************************************************
*/
static int
keep_alive_doiovar
(
	void 			*hdl,
	uint32 			actionid,
	void 			*p,
	uint 			plen,
	void 			*a,
	uint 			alen,
	uint 			vsize,
	struct wlc_if 		*wlcif
)
{
	wl_keep_alive_info_t	*info = hdl;
	wl_keep_alive_pkt_t	*keep_alive_pkt;
	int err = BCME_OK;
	wl_mkeep_alive_pkt_t *mkeep_alive_pkt;
	int mkeepalive_index;
	uint32 period_msec;
	uint16 len_bytes;
	uint16  version;
	mkeep_alive_indiv_t *indiv;
	keep_alive_cmn_t *ka_cmn = info->ka_cmn;

	switch (actionid) {

	case IOV_SVAL(IOV_KEEP_ALIVE):
		keep_alive_pkt = a;
		memcpy(&period_msec, &keep_alive_pkt->period_msec, sizeof(period_msec));
		memcpy(&len_bytes, &keep_alive_pkt->len_bytes, sizeof(len_bytes));
		err = mkeep_alive_set(&ka_cmn->mkeep_alive[0], len_bytes, period_msec,
		    keep_alive_pkt->data, wlc_bsscfg_find_by_wlcif(info->wlc, wlcif));
		break;

	case IOV_SVAL(IOV_MKEEP_ALIVE):
		mkeep_alive_pkt = a;
		indiv = &ka_cmn->mkeep_alive[mkeep_alive_pkt->keep_alive_id];
		memcpy(&version, &mkeep_alive_pkt->version, sizeof(version));
		memcpy(&period_msec, &mkeep_alive_pkt->period_msec, sizeof(period_msec));
		memcpy(&len_bytes, &mkeep_alive_pkt->len_bytes, sizeof(len_bytes));

		if (version != WL_MKEEP_ALIVE_VERSION) {
			err = BCME_VERSION;
			break;
		}
		if (mkeep_alive_pkt->keep_alive_id >= ka_cmn->keep_alive_count) {
			err = BCME_BADARG;
			break;
		}

		if (indiv->period_msec > 0) {
			WL_ERROR(("wl%d: %s: Index %d already exist with timeout %d and updated to "
				"timeout = %d\n", WLCUNIT(indiv->bsscfg), __FUNCTION__,
				mkeep_alive_pkt->keep_alive_id, indiv->period_msec, period_msec));
		}

		err = mkeep_alive_set(&ka_cmn->mkeep_alive[mkeep_alive_pkt->keep_alive_id],
			len_bytes,
			period_msec,
			mkeep_alive_pkt->data,
			wlc_bsscfg_find_by_wlcif(info->wlc, wlcif));
		break;

	case IOV_GVAL(IOV_MKEEP_ALIVE):
		mkeepalive_index = *((int*)p);
		mkeep_alive_pkt = a;
		if ((mkeepalive_index >= ka_cmn->keep_alive_count) || (mkeepalive_index < 0)) {
			err = BCME_BADARG;
			break;
		}
		version = WL_MKEEP_ALIVE_VERSION;
		len_bytes = WL_MKEEP_ALIVE_FIXED_LEN;
		memcpy(&mkeep_alive_pkt->length, &len_bytes, sizeof(len_bytes));
		memcpy(&mkeep_alive_pkt->version, &version, sizeof(version));
		memcpy(&mkeep_alive_pkt->len_bytes,
		      &ka_cmn->mkeep_alive[mkeepalive_index].len_bytes,
		      sizeof(mkeep_alive_pkt->len_bytes));

		memcpy(&mkeep_alive_pkt->keep_alive_id,
		       &mkeepalive_index,
		       sizeof(mkeep_alive_pkt->keep_alive_id));

		memcpy(&mkeep_alive_pkt->period_msec,
		       &ka_cmn->mkeep_alive[mkeepalive_index].period_msec,
		       sizeof(mkeep_alive_pkt->period_msec));

		/* Check if the memory provided is sufficient */
		if (alen < (int)(WL_MKEEP_ALIVE_FIXED_LEN +
			ka_cmn->mkeep_alive[mkeepalive_index].len_bytes)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		memcpy(mkeep_alive_pkt->data,
		      ka_cmn->mkeep_alive[mkeepalive_index].pkt_data,
		      ka_cmn->mkeep_alive[mkeepalive_index].len_bytes);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/*
*****************************************************************************
* Function:   keep_alive_timer_update
*
* Purpose:    Enable/disable the periodic keep-alive timer.
*
* Parameters: info   (mod) Keep-alive context data.
*             enable (in)  Enable/disable timer.
*
* Returns:    Nothing.
*****************************************************************************
*/
static void
keep_alive_timer_update(mkeep_alive_indiv_t *indiv, int period_msec, bool enable)
{
	wlc_info_t *wlc = indiv->bsscfg->wlc;
	void *wl = wlc->wl;

	WL_TRACE(("wl%d: %s : state %d, timeout %d\n",
			WLCUNIT(indiv->bsscfg), __FUNCTION__, enable, period_msec));
	wl_del_timer(wl, indiv->timer);

	if (!enable)
		return;

	wl_add_timer(wl, indiv->timer, period_msec, TRUE);
}
/*
 * remaining_time = period - |(now - used)|
 *
 * if (remaining_time <= precision) OK else reschedule(remaining_time)
 *
 */
/* XXX: One possibility for null-packets is to use one-shot timer but
 *      that would require an event/callback from association state
 *      machine to start the timer once the STA is associated.
 */
static bool
keep_alive_idle_time_check(mkeep_alive_indiv_t* indiv)
{
	wlc_info_t *wlc = indiv->bsscfg->wlc;
	struct scb_iter scbiter;
	struct scb *scb;
	uint32 period;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_STA(SCB_BSSCFG(scb)) && SCB_ASSOCIATED(scb) &&
		    (SCB_BSSCFG(scb) == indiv->bsscfg)) {
				return TRUE;
		}
	}
	period = indiv->override_period_msec ? indiv->override_period_msec: indiv->period_msec;
	keep_alive_timer_update(indiv, period, TRUE);
	return FALSE;
}

/*
*****************************************************************************
* Function:   keep_alive_txcb
*
* Purpose:    Callback transmit function.
*             Start/stop/manage retries for failed keepalive transmits.
*
* Parameters: Registered callback arg: individual mkeep_alive config.
*
* Returns:    Nothing.
*****************************************************************************
*/
static void
wl_keep_alive_txcb(wlc_info_t *wlc, uint txstatus, void *arg)
{
	mkeep_alive_indiv_t *indiv = arg;
	const uint good_txs = TX_STATUS_ACK_RCV;
	uint32 period;
	wl_keep_alive_info_t *keepalive = wlc->keepalive;

	WL_INFORM(("KeepAliveCB: indiv %p len %d period %d override %d\n",
			OSL_OBFUSCATE_BUF(indiv), indiv->len_bytes,
			indiv->period_msec, indiv->override_period_msec));
	WL_INFORM(("             txstatus %08x rflags %02x\n", txstatus, indiv->rflags));

	/* Determine intended (configured) period */
	period = indiv->override_period_msec ? indiv->override_period_msec : indiv->period_msec;

	/* If (de/re)configured during tx wait, do not mess with the timer */
	if ((indiv->len_bytes == 0) || (period == 0)) {
		indiv->rflags = 0;
		return;
	}

	/* Clear the waiting-for-txstatus flag */
	indiv->rflags &= ~MKEEP_ALIVE_RFLAG_TXSWAIT;

	if ((txstatus & good_txs) == good_txs) {
		/* Success: if not in retry already, we're done */
		if ((indiv->rflags & MKEEP_ALIVE_RFLAG_RETRY) == 0)
			return;

		/* Otherwise in retry: clear flag and count, restore timer below */
		indiv->rflags &= ~(MKEEP_ALIVE_RFLAG_RETRY | MKEEP_ALIVE_RFLAG_CNTMASK);
	} else {
		/* Failed transmits enter or continue retry state (up to a point) */
		if ((indiv->rflags & MKEEP_ALIVE_RFLAG_RETRY) == 0) {
			/* Don't do anything if retries disabled */
			if (keepalive->rcnt == 0)
				return;

			/* Also, don't start retries if configured period is faster */
			if (period <= MKEEP_ALIVE_RETRY_PERIOD)
				return;

			/* Start retries: set flag and initialize count */
			indiv->rflags |= MKEEP_ALIVE_RFLAG_RETRY;
			indiv->rflags &= ~MKEEP_ALIVE_RFLAG_CNTMASK;
			indiv->rflags |= keepalive->rcnt;

			/* Change timer to retry period */
			period = MKEEP_ALIVE_RETRY_PERIOD;
		} else if ((indiv->rflags & MKEEP_ALIVE_RFLAG_CNTMASK) == 0) {
			/* Retries used up: clear flag and restore timer value */
			indiv->rflags &= ~MKEEP_ALIVE_RFLAG_RETRY;
		}  else {
			/* Continue retries: decrement count, leave timer alone */
			indiv->rflags = (indiv->rflags & ~MKEEP_ALIVE_RFLAG_CNTMASK) |
			        ((indiv->rflags & MKEEP_ALIVE_RFLAG_CNTMASK) - 1);
			return;
		}
	}

	/* Here in order to change timer to period */
	keep_alive_timer_update(indiv, period, TRUE);
}

/*
*****************************************************************************
* Function:   keep_alive_timer_callback
*
* Purpose:    Callback timer function. Send the specified data packet.
*
* Parameters: arg (mode) User registered timer argument.
*
* Returns:    Nothing.
*****************************************************************************
*/
static void
keep_alive_timer_callback(void *arg)
{
	mkeep_alive_indiv_t *indiv;
	void *pkt;
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc;
	wl_keep_alive_info_t *keepalive;

	indiv = arg;
	wlc = indiv->bsscfg->wlc;
	keepalive = wlc->keepalive;
#ifdef WLSTA_KEEP_ALIVE
	struct scb *scb = NULL;
#endif // endif
	WL_TRACE(("wl%d: %s : timeout %d\n",
	          WLCUNIT(indiv->bsscfg), __FUNCTION__, indiv->period_msec));

	/* If previous packet is still pending, don't send again */
	if (indiv->rflags & MKEEP_ALIVE_RFLAG_TXSWAIT)
		return;

	cfg = indiv->bsscfg;
	if (!BSSCFG_AS_STA(cfg)) {
		return;
	}
	/* check idle time and get bss if this callback was for a null-packet */
	if (indiv->len_bytes == 0) {
		if (!keep_alive_idle_time_check(indiv))
			return;
#ifdef WLSTA_KEEP_ALIVE
		if (((scb = wlc_scbfind(wlc, cfg, &cfg->BSSID)) != NULL) && BSSCFG_STA(cfg)) {
			/* check wmm link capability, wmm on = QOS Null, wmm off = Null frame */
			if (SCB_WME(scb)) {
				wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, 0, NULL, NULL);
			}
			else {
				wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0,
					PKTPRIO_NON_QOS_DEFAULT, NULL, NULL);
			}
		}
#else /* WLSTA_KEEP_ALIVE */
		wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, PKTPRIO_NON_QOS_DEFAULT, NULL, NULL);
#endif // endif
		return;
	}

	/* Allocate packet to send. */
	pkt = PKTGET(WLCOSH(indiv->bsscfg),
	             indiv->len_bytes + keepalive->txoff,
	             TRUE);

	if (pkt == NULL) {
		keepalive->apppkt_malloc_failed++;
		WL_ERROR(("wl%d: %s : failed to allocate tx pkt\n",
		          WLCUNIT(indiv->bsscfg), __FUNCTION__));

		return;
	}

	/* Populate packet with user specified data contents. */
	PKTPULL(WLCOSH(indiv->bsscfg), pkt, keepalive->txoff);
	bcopy(indiv->pkt_data, PKTDATA(WLCOSH(indiv->bsscfg), pkt), indiv->len_bytes);
	WLPKTTAG(pkt)->flags3 |= WLF3_NO_PMCHANGE;

	/* Best effort retries: if packet callback fails, don't risk lockup */
	if (wlc_pcb_fn_register(wlc->pcb, wl_keep_alive_txcb, indiv, pkt) == BCME_OK) {
		indiv->rflags |= MKEEP_ALIVE_RFLAG_TXSWAIT;
	} else {
		WL_ERROR(("wl%d: failed keepalive callback register, cfg %p\n",
		          WLCUNIT(indiv->bsscfg), OSL_OBFUSCATE_BUF((void *)indiv)));
	}
	WL_INFORM(("KeepAliveSend: indiv %p rflags %02x\n",
		OSL_OBFUSCATE_BUF(indiv), indiv->rflags));
	wlc_sendpkt(wlc, pkt, cfg->wlcif);
}

int
wl_keep_alive_upd_override_period(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint32 override_period, uint8 mkeepalive_index)
{
	uint32 period_msec;
	mkeep_alive_indiv_t *indiv;
	int i;
	wl_keep_alive_info_t *keepalive = wlc->keepalive;
	keep_alive_cmn_t *ka_cmn;

	if (!keepalive)
		return BCME_ERROR;
	ka_cmn = keepalive->ka_cmn;
	if (mkeepalive_index >= ka_cmn->keep_alive_count)
		return BCME_BADARG;
	indiv = &ka_cmn->mkeep_alive[mkeepalive_index];

	if (!WLC_BSS_CONNECTED(indiv->bsscfg)) {
		WL_ERROR(("wl%d: %s BSS unassociated \n", WLCUNIT(indiv->bsscfg),
			__FUNCTION__));
		return BCME_NOTASSOCIATED;
	}

	/* Disable timer while we modify timer state to avoid race-conditions */
	keep_alive_timer_update(indiv, indiv->period_msec, FALSE);

	/* Disable override period from any earlier configuration */
	for (i = 0; i < ka_cmn->keep_alive_count; i++) {
		mkeep_alive_indiv_t *temp = &ka_cmn->mkeep_alive[i];
		if (i != mkeepalive_index && temp->override_period_msec != 0 &&
			indiv->bsscfg == temp->bsscfg) {
			/* Disable timer while we modify timer state to avoid race-conditions */
			keep_alive_timer_update(temp, temp->period_msec, FALSE);
			temp->override_period_msec = 0;
			/* Start the periodic timer with new values. */
			keep_alive_timer_update(temp, temp->period_msec, TRUE);
			break;
		}
	}

	indiv->override_period_msec = override_period;

	period_msec = indiv->period_msec;
	if (override_period)
		period_msec = override_period;

	if (period_msec) {
		/* Start the periodic timer with new values. */
		keep_alive_timer_update(indiv, period_msec, TRUE);
	}
	return BCME_OK;
}
