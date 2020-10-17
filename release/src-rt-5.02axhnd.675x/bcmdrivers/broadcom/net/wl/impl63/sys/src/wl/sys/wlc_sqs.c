/*
 * Single stage Queuing and Scheduling module
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
 * $Id:$
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11_cfg.h>
#include <wl_export.h>
#include <wlc_types.h>
#include <wlc.h>
#include <wlc_keymgmt.h>
#include <wlc_scb.h>
#include <wlc_ampdu.h>
#include <wlc_amsdu.h>
#include <wlc_cfp.h>
#include <wlc_cfp_priv.h>
#include <wlc_sqs.h>
#include <wlc_wlfc.h>
#include <wlc_hw_priv.h>
#if defined(BCMDBG)
#include <wlc_dump.h>
#endif // endif
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

#define SQS_LAZY_FETCH_WATERMARK	128
#define SQS_LAZY_FETCH_DELAYCNT		3

#if defined(BCMDBG)
static int wlc_sqs_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/** File scoped SQS Global object: Fast acecss to SQS module and SQS Cubbies. */
static struct wlc_sqs_global {
	wlc_sqs_info_t  *wlc_sqs;
} wlc_sqs_global = {
	.wlc_sqs = (wlc_sqs_info_t *)NULL,
};

/** File scoped SQS Module Global Pointer. */
#define WLC_SQS_G		((wlc_sqs_info_t*)(wlc_sqs_global.wlc_sqs))

static inline uint16 wlc_sqs_pull_packets_cb(uint16 ringid, uint16 request_cnt);

static inline bool wlc_sqs_flow_ring_status_cb(uint16 ringid);
#define SQS_FLRING_ACTIVE(ringid)	wlc_sqs_flow_ring_status_cb((ringid))

#ifdef WLTAF
void * wlc_sqs_taf_get_handle(wlc_info_t* wlc)
{
	return &wlc_sqs_global.wlc_sqs;
}

void * wlc_sqs_taf_get_scb_info(void *sqsh, struct scb* scb)
{
	wlc_sqs_info_t *sqs_info = (wlc_sqs_info_t *)sqsh;

	return (scb && sqs_info) ? (void*)wlc_scb_cfp_cubby(WLC_SQS_WLC(sqs_info), scb) : NULL;
}

void * wlc_sqs_taf_get_scb_tid_info(void *scb_h, int tid)
{
	return (void*)(uint32)tid;
}

bool wlc_sqs_taf_release(void* sqsh, void* scbh, void* tidh, bool force,
	taf_scheduler_public_t* taf)
{
	wlc_sqs_info_t *sqs_info = (wlc_sqs_info_t *)sqsh;
	scb_cfp_t * scb_cfp = (scb_cfp_t *)scbh;
	int prio = (int)tidh;
	wlc_info_t *wlc = WLC_SQS_WLC(sqs_info);
	int32 taf_pkt_time_units;
	int32 taf_pkt_units_to_fill;
	uint32 virtual_release = 0;
	int32 max_virtual_release;
	int32 real_pkts;
	uint16 pktbytes;
	int32 margin;

	TAF_ASSERT(taf->how == TAF_RELEASE_LIKE_IAS);
	TAF_ASSERT(prio >= 0 && prio < NUMPRIO);

	if (taf->ias.is_ps_mode) {
		/* regardless set emptied flag as the available traffic (ie in PS) is none
		 * so this is effectively empty
		 */
		taf->ias.was_emptied = TRUE;

		taf->complete = TAF_REL_COMPLETE_PS;

		/* do not do virtal scheduling in ps state so return */
		return FALSE;
	}

	taf_pkt_units_to_fill = taf->ias.time_limit_units -
		(taf->ias.actual.released_units + taf->ias.virtual.released_units +
		taf->ias.pending.released_units + taf->ias.total.released_units);

	if ((taf->ias.released_units_limit > 0) &&
		(taf_pkt_units_to_fill > taf->ias.released_units_limit)) {

		taf_pkt_units_to_fill = taf->ias.released_units_limit;
	}

	if (taf_pkt_units_to_fill <= 0) {
		WL_ERROR(("%s: taf_pkt_units_to_fill = %d, actual.released_units = %u, "
			"virtual.released_units = %u, pending.released_units = %u, "
			"total.released_units %u, time_limit_units %u\n",
			__FUNCTION__, taf_pkt_units_to_fill, taf->ias.actual.released_units,
			taf->ias.virtual.released_units, taf->ias.pending.released_units,
			taf->ias.total.released_units, taf->ias.time_limit_units));
		TAF_ASSERT(!(taf_pkt_units_to_fill <= 0));

		taf->complete = TAF_REL_COMPLETE_ERR;

		return FALSE;
	}

	pktbytes = taf->ias.estimated_pkt_size_mean;

	taf_pkt_time_units = TAF_PKTBYTES_TO_UNITS(pktbytes, taf->ias.pkt_rate,
		taf->ias.byte_rate);

	if (taf_pkt_time_units == 0) {
		WL_ERROR(("%s: taf_pkt_time_units = %d, pktbytes = %u, pkt_rate = %u, "
			  "byte_rate = %u \n", __FUNCTION__, taf_pkt_time_units, pktbytes,
			  taf->ias.pkt_rate, taf->ias.byte_rate));
		taf_pkt_time_units = 1;
	}

	max_virtual_release = (taf_pkt_units_to_fill / taf_pkt_time_units);

	if (max_virtual_release * taf_pkt_time_units < taf_pkt_units_to_fill) {
		/* round up */
		max_virtual_release++;
	}

	max_virtual_release = MIN(max_virtual_release, 4095);

	margin = 1 + ((max_virtual_release * taf->ias.margin) >> 8);

	real_pkts = wlc_sqs_ampdu_n_pkts(wlc, scb_cfp, prio);

	if (max_virtual_release + margin - real_pkts > 0) {
		max_virtual_release -= real_pkts;
	} else {
		WL_TAFF(wlc, "enough real packets exist (have %u, require %u, "
			"margin %u)\n", real_pkts, max_virtual_release, margin);

		taf->ias.pending.released_units += taf_pkt_time_units * max_virtual_release;
		taf->ias.pending.release += max_virtual_release;
		max_virtual_release = 0;
		margin = 0;
	}

	if (max_virtual_release + margin > 0) {
		virtual_release = wlc_ampdu_pull_packets(wlc, SCB_CFP_SCB(scb_cfp), prio,
			(uint16)(margin + max_virtual_release), taf);

	} else {
		/* pull a single packet to keep state machine moving */
		taf->ias.virtual.release +=
			wlc_ampdu_pull_packets(wlc, SCB_CFP_SCB(scb_cfp), prio, 1, taf);
	}

	if (virtual_release) {
		uint32 vrel_units;
		uint32 rpend_units = taf_pkt_time_units * real_pkts;

		/* account for real packets already available */
		taf->ias.pending.release += real_pkts;
		taf->ias.pending.released_units += rpend_units;

		if (virtual_release > max_virtual_release) {
			/* only account for what we need, not the extra margin */
			vrel_units = taf_pkt_time_units * max_virtual_release;
		} else {
			vrel_units = taf_pkt_time_units * virtual_release;
		}
		taf->ias.virtual.release += virtual_release;
		taf->ias.virtual.released_units += vrel_units;

		if (taf->ias.released_units_limit &&
				((rpend_units + vrel_units) > taf->ias.released_units_limit)) {
			/* report back if over-scheduled, as window may need to be extended */
			taf->ias.extend.units += (rpend_units + vrel_units) -
				taf->ias.released_units_limit;
		}

		if (taf->ias.traffic_count_available < virtual_release) {
			WL_ERROR(("%s: virtual_release %u, traffic_count_available %u\n",
				__FUNCTION__, virtual_release, taf->ias.traffic_count_available));
			TAF_ASSERT(!(taf->ias.traffic_count_available < virtual_release));
		}
		taf->ias.traffic_count_available -= virtual_release;

		if (taf->ias.traffic_count_available == 0) {
			taf->ias.was_emptied = TRUE;

			taf->complete = TAF_REL_COMPLETE_EMPTIED;

		} else if (virtual_release >= max_virtual_release) {
			taf->ias.was_emptied = FALSE;

			taf->complete = (taf_pkt_units_to_fill == taf->ias.released_units_limit) ?
				TAF_REL_COMPLETE_REL_LIMIT : TAF_REL_COMPLETE_TIME_LIMIT;

		} else {
			/* SQS gave less than what we ask; for IAS purposes, this should be
			 * treated as if "emptied" because we can't get more
			 */
			taf->ias.was_emptied = TRUE;

			taf->complete = TAF_REL_COMPLETE_RESTRICTED;
		}
	} else if (max_virtual_release + margin > 0) {

		taf->ias.was_emptied = TRUE;
		taf->complete = TAF_REL_COMPLETE_RESTRICTED;

		return FALSE;
	} else {
		taf->ias.was_emptied = (taf->ias.traffic_count_available == 0);
		taf->complete = (taf_pkt_units_to_fill == taf->ias.released_units_limit) ?
				TAF_REL_COMPLETE_REL_LIMIT : TAF_REL_COMPLETE_TIME_LIMIT;
	}
	return TRUE;
}

uint16 wlc_sqs_taf_get_scb_tid_pkts(void *scbh, void *tidh)
{
	scb_cfp_t * scb_cfp = (scb_cfp_t *)scbh;
	int prio = (int)tidh;
	int tot_pkts = 0;

	if (scb_cfp) {
		/* Get Host flowring id */
		uint16 ringid = SCB_CFP_RINGID(scb_cfp, prio);

		if (SQS_FLRING_ACTIVE(ringid)) {
			tot_pkts = wlc_sqs_ampdu_vpkts(WLC_SQS_WLC(WLC_SQS_G), scb_cfp, prio);
		}
	}
	return tot_pkts;
}
#endif /* WLTAF */

/** SQS module attach handler */
wlc_sqs_info_t *
BCMATTACHFN(wlc_sqs_attach)(wlc_info_t *wlc)
{
	wlc_sqs_info_t *sqs_info = NULL;

	if ((sqs_info = MALLOCZ(wlc->osh, sizeof(wlc_sqs_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		         wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Setup a global pointer to SQS module */
	wlc_sqs_global.wlc_sqs = sqs_info; /* WLC_SQS_G */
	sqs_info->wlc = wlc;

	/* Register SQS module up/down, watchdog, and iovar callbacks */
	//if (wlc_module_register(wlc->pub, sqs_iovars, "sqs", sqs_info,
	//		wlc_sqs_doiovar, wlc_sqs_watchdog, NULL, NULL) != BCME_OK) {
	if (wlc_module_register(wlc->pub, NULL, "sqs", sqs_info,
			NULL, NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#if defined(BCMDBG)
	/* Register a Dump utility for SQS */
	wlc_dump_add_fns(wlc->pub, "sqs", wlc_sqs_dump, NULL, (void *)sqs_info);
#endif // endif

	return sqs_info;

fail:
	MODULE_DETACH(sqs_info, wlc_sqs_detach);
	return NULL;
}

/** SQS module detach handler */
void
BCMATTACHFN(wlc_sqs_detach)(wlc_sqs_info_t *sqs_info)
{
	wlc_info_t *wlc;

	if (sqs_info == NULL)
		return;

	wlc = sqs_info->wlc;

	/* Unregister the module */
	wlc_module_unregister(wlc->pub, "sqs", sqs_info);

	/* Free up the SQS module memory */
	MFREE(wlc->osh, sqs_info, WLC_SQS_SIZE);

	/* Reset the global SQS module */
	wlc_sqs_global.wlc_sqs = NULL; /* WLC_SQS_G */
}
#if defined(BCMDBG)
/* SQS dump utility */
static int
wlc_sqs_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_sqs_info_t *sqs_info = (wlc_sqs_info_t*)ctx;

	bcm_bprintf(b, "V2R Inransit \t%d \n", WLC_SQS_V2R_INTRANSIT(sqs_info));
	bcm_bprintf(b, "EoPS Intransit \t%d \n", WLC_SQS_EOPS_INTRANSIT(sqs_info));

	return 0;
}
#endif // endif

/* Register a callback function to pull packets from BUS */
void
wlc_sqs_pull_packets_register(pkt_pull_cb_t cb, void* arg)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Save the fn pointer and arg */
	WLC_SQS_PKTPULL_CB_FN(sqs_info) = cb;
	WLC_SQS_PKTPULL_CB_ARG(sqs_info) = arg;

}
/* Callback function to get Flow ring status from BUS layer */
void
wlc_sqs_flowring_status_register(flow_ring_status_cb_t cb, void* arg)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Save the fn pointer and arg */
	WLC_SQS_FLRING_STS_CB_FN(sqs_info) = cb;
	WLC_SQS_FLRING_STS_CB_ARG(sqs_info) = arg;

}
/* Register a callback function register a End of pull set from TAF */
void
wlc_sqs_eops_rqst_register(eops_rqst_cb_t cb, void* arg)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Save the fn pointer and arg */
	WLC_EOPS_RQST_CB_FN(sqs_info) = cb;
	WLC_EOPS_RQST_CB_ARG(sqs_info) = arg;
}
/* Invoke the callback function registered by BUS layer */
static inline uint16
wlc_sqs_pull_packets_cb(uint16 ringid, uint16 request_cnt)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Make sure calbacks are registered */
	ASSERT(WLC_SQS_PKTPULL_CB_FN(sqs_info));
	ASSERT(WLC_SQS_PKTPULL_CB_ARG(sqs_info));

	/* Call back */
	return ((WLC_SQS_PKTPULL_CB_FN(sqs_info))((WLC_SQS_PKTPULL_CB_ARG(sqs_info)),
		ringid, request_cnt));
}

/* Invoke the callback function registered by BUS layer */
static inline int
wlc_sqs_eops_rqst_cb(void)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Make sure calbacks are registered */
	ASSERT(WLC_EOPS_RQST_CB_FN(sqs_info));
	ASSERT(WLC_EOPS_RQST_CB_ARG(sqs_info));

	/* Call back */
	return ((WLC_EOPS_RQST_CB_FN(sqs_info))(WLC_EOPS_RQST_CB_ARG(sqs_info)));
}
/* Invoke the callback function registered by BUS layer */
static inline bool
wlc_sqs_flow_ring_status_cb(uint16 ringid)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	/* Make sure calbacks are registered */
	ASSERT(WLC_SQS_FLRING_STS_CB_FN(sqs_info));
	ASSERT(WLC_SQS_FLRING_STS_CB_ARG(sqs_info));

	if (ringid == SCB_CFP_RINGID_INVALID)
		return FALSE;

	/* Call back */
	return ((WLC_SQS_FLRING_STS_CB_FN(sqs_info))((WLC_SQS_FLRING_STS_CB_ARG(sqs_info)),
		ringid));
}
bool
wlc_sqs_capable(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	/* Can we get the answer from CFP tcb instead? */
	return wlc_sqs_ampdu_capable(wlc, scb_cfp, prio);
}

uint16
wlc_sqs_vpkts(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	return wlc_sqs_ampdu_vpkts(wlc, scb_cfp, prio);
}

uint16
wlc_sqs_v2r_pkts(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	return wlc_sqs_ampdu_v2r_pkts(wlc, scb_cfp, prio);
}
/* Return scb ampdu in transit packets */
uint16
wlc_sqs_in_transit_pkts(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	return wlc_sqs_ampdu_in_transit_pkts(wlc, scb_cfp, prio);
}
/* SCB ampdu real packets */
uint16
wlc_sqs_n_pkts(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	return wlc_sqs_ampdu_n_pkts(wlc, scb_cfp, prio);
}
/* SCB ampdu to be released peackets [waiting for real packets] */
uint16
wlc_sqs_tbr_pkts(uint16 cfp_flowid, uint8 prio)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	return wlc_sqs_ampdu_tbr_pkts(wlc, scb_cfp, prio);
}
#ifdef HWA_TXPOST_BUILD
void
wlc_sqs_v2r_enqueue(uint16 cfp_flowid, uint8 prio, uint16 v2r_count)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;
	struct scb *scb;

	ASSERT(sqs_info);

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);
	ASSERT(scb_cfp);

	scb = SCB_CFP_SCB(scb_cfp);

	if (SCB_INTERNAL(scb)) {
		return;
	}
	/* Increment total outstanding V2R request in system */
	WLC_SQS_V2R_INTRANSIT(sqs_info) += v2r_count;

	return wlc_sqs_ampdu_v2r_enqueue(wlc, scb_cfp, prio, v2r_count);
}

void
wlc_sqs_v2r_dequeue(uint16 cfp_flowid, uint8 prio, uint16 pkt_count, bool sqs_force)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;
	struct scb *scb;

	ASSERT(sqs_info);

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	ASSERT(scb_cfp);

	scb = SCB_CFP_SCB(scb_cfp);

	if (SCB_INTERNAL(scb)) {
		return;
	}
	/* Decrement total outstanding V2R request in system */
	ASSERT(WLC_SQS_V2R_INTRANSIT(sqs_info) >= pkt_count);

	WLC_SQS_V2R_INTRANSIT(sqs_info) -= pkt_count;

#ifdef WLTAF
	/* check if TAF is enabled and not in bypass state */
	if (wlc_taf_in_use(wlc->taf_handle) && !sqs_force) {
		wlc_taf_pkts_dequeue(wlc->taf_handle, SCB_CFP_SCB(scb_cfp),
			prio, pkt_count);
	}
#endif /* WLTAF */
	return wlc_sqs_ampdu_v2r_dequeue(wlc, scb_cfp, prio, pkt_count);
}
#endif /* HWA_TXPOST_BUILD */

/** Wireless SQS entry point. */
int
wlc_sqs_sendup(uint16 cfp_flowid, uint8 prio, uint16 v_pkts)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;
	struct scb *scb;
	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);
	scb = SCB_CFP_SCB(scb_cfp);

	BCM_REFERENCE(scb);

	ASSERT(!ETHER_ISBCAST(&scb->ea));
#ifdef WLTAF
	/* check if TAF is enabled and not in bypass state */
	if (wlc_taf_in_use(wlc->taf_handle)) {
		return wlc_sqs_taf_admit(wlc, scb, prio, v_pkts);
	}
#endif /* WLTAF */
	/* admit virtual offered load into WL layer */
	return wlc_sqs_ampdu_admit(wlc, scb_cfp, prio, v_pkts);
}

/**
 * wlc_sqs_pktq_release() - Release packets from a SQS extended precedence
 * queue.
 *
 * If there are no v2r in progress, and there are real packets to service a
 * release request, then real packets are dequeue from the precedence queue and
 * a packet list is returned (head packet pointer).
 *
 * Otherwise, virtual packets (if any) are assigned for v2r conversion. A NULL
 * pktlist is returned and the caller is informed that it needs to proceed with
 * V2R packet conversions via the PCI bus layer, for v2r_request number of pkts.
 * NOTE: v2r_request may be 0. No actual conversion is performed at this point.
 *
 * The caller may revert a v2r_request by invoking wlc_sqs_pktq_v2r_revert().
 *
 * @param pktq   Multi-priority packet queue
 */
void *
wlc_sqs_pktq_release(struct scb *scb, struct pktq *pktq, uint8 prio, int pkts_release,
	int *v2r_request, bool amsdu_in_ampdu, int max_pdu)
{
	struct pktq_prec *pktqp = &pktq->q[prio];	/**< single precedence packet queue */
	wlc_info_t *wlc;
	int release;

	ASSERT(pkts_release > 0);

	wlc = WLC_SQS_WLC(WLC_SQS_G);

	/* Check whether there are sufficient real packets to release */
	if (pktqp->n_pkts >= pkts_release) {
		/* Just return the head pkt to notify for CFP release */
		*v2r_request = 0;
		return pktqp->head;
	}

	/* Account for the real packets if required */
	pkts_release = SQS_AMPDU_RELEASE_LEN(pktq, prio, pkts_release);

	/* Caller may request more than available virtual packets */
	release = MIN(pktqp->v_pkts, pkts_release);

	/* Lazy fetching to accumulate more packets for aggregation */
	if (
#if defined(WLATF)
		(wlc_ampdu_tx_intransit_get(wlc) > SQS_LAZY_FETCH_WATERMARK) &&
#endif // endif
		(release == pktqp->v_pkts) && (release < max_pdu) &&
		(pktqp->skip_cnt < SQS_LAZY_FETCH_DELAYCNT)) {
		pktqp->skip_cnt++;
		release = 0;
	} else
		pktqp->skip_cnt = 0;

	if (!amsdu_in_ampdu) {
		/* Try requesting more packets in max_pdu multiples */
		if (max_pdu == AMPDU_NUM_MPDU_1SS_D11AQM || max_pdu == AMPDU_NUM_MPDU_3SS_D11AQM) {
			/* Use logical operations instead of multiplication/division
			 * that really hurt performance per the test results.
			 */
			release += ((pktqp->v_pkts - release) & ~(max_pdu - 1));
		}
	}
	WL_TRACE(("wlc_sqs_pktq_release: v_pkts<%d> n_pkts<%d> release<%d>\n",
		pktqp->v_pkts, pktqp->n_pkts, release));

	*v2r_request = release;
	ASSERT(*v2r_request >= 0);

	/* Caller may proceed with v2r conversion ONLY if v2r_request != 0 */
	return NULL;
}

/**
 * wlc_sqs_v2r_revert() - Revert a v2r_request.
 */
void
wlc_sqs_v2r_revert(uint16 cfp_flowid, uint8 prio, uint16 v2r_reverts)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	wlc_sqs_pktq_v2r_revert(wlc, scb_cfp, prio, v2r_reverts);
}

/**
 * wlc_sqs_vpkts_rewind() - Rewind the fetch_ptr and revert/increase the vpkts.
 */
void
wlc_sqs_vpkts_rewind(uint16 cfp_flowid, uint8 prio, uint16 count)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	wlc_sqs_pktq_vpkts_rewind(wlc, scb_cfp, prio, count);
#ifdef WLTAF
	wlc_taf_sched_state(wlc->taf_handle, (struct scb*)SCB_CFP_SCB(scb_cfp), prio, count,
		TAF_SQSHOST, TAF_SCHED_STATE_REWIND);
#endif // endif
}

/**
 * wlc_sqs_vpkts_forward() - Forward the fetch_ptr and decrease the vpkts.
 */
void
wlc_sqs_vpkts_forward(uint16 cfp_flowid, uint8 prio, uint16 count)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	wlc_sqs_pktq_vpkts_forward(wlc, scb_cfp, prio, count);
}

/**
 * wlc_sqs_vpkts_enqueue () - admit virtual packets into a precedence Queue.
 * There is no drop threshold on a precedence queue. Host performs push into
 * a flowring and is responsible for all host side traffic management.
 *
 * @param cfp_flowid	CFP flow ID
 * @param prio		Packet priority
 * @paarm v_pkts	Virtual packets added in current iteration
 */
void
wlc_sqs_vpkts_enqueue(uint16 cfp_flowid, uint8 prio, uint16 v_pkts)
{
	scb_cfp_t *scb_cfp;
	wlc_info_t *wlc;

	ASSERT_CFP_FLOWID(cfp_flowid);
	ASSERT(WLC_SQS_G);

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	scb_cfp = wlc_scb_cfp_id2ptr(cfp_flowid);

	ASSERT(scb_cfp);
	wlc_sqs_pktq_vpkts_enqueue(wlc, scb_cfp, prio, v_pkts);
#ifdef WLTAF
	/* check if TAF is enabled and not in bypass state */
	if (wlc_taf_in_use(wlc->taf_handle)) {
		wlc_taf_pkts_enqueue(wlc->taf_handle, scb_cfp->scb,
			prio, TAF_SQSHOST, v_pkts);
	}
#endif // endif
}

/* check if stride should be paused */
bool
wlc_sqs_fifo_paused(uint8 prio)
{
	wlc_info_t *wlc;
	uint8 fifo;

	wlc = WLC_SQS_WLC(WLC_SQS_G);
	fifo = prio2fifo[prio];

	if (wlc->block_datafifo | isset(wlc->hw->suspended_fifos, fifo)) {
		return TRUE;
	}
	return FALSE;
}
/* Submit request to convert virtual to real packets */
uint16
wlc_sqs_ampdu_pull_packets(wlc_info_t* wlc, struct scb* scb, struct pktq *pktq,
	uint8 tid, uint16 request_cnt)
{
	scb_cfp_t *scb_cfp;
	uint16 ringid;
	int v2r_request = request_cnt;
	uint16 ret;

	ASSERT(scb);
	ASSERT(pktq);

	/* Account for the real packets if required */
#ifdef WLTAF
	if (!wlc_taf_in_use(wlc->taf_handle))
#endif /* WLTAF */
	{
		v2r_request = SQS_AMPDU_RELEASE_LEN(pktq, tid, v2r_request);
	}

	v2r_request = MIN(pktq->q[tid].v_pkts, v2r_request);

	ASSERT(v2r_request >= 0);

	scb_cfp = wlc_scb_cfp_cubby(wlc, scb);

	/* Check for valid scb cfp cubby */
	if (scb_cfp == NULL) {
		ASSERT(0);
		return 0;
	}

	/* Get Host flowring id */
	ringid = SCB_CFP_RINGID(scb_cfp, tid);

	ASSERT_CFP_RINGID(ringid);

	WLC_SQS_DEBUG(("%s : SQS V2R submit : flowid %d cnt %d \n",
		__FUNCTION__, SCB_CFP_FLOWID(scb_cfp), v2r_request));

	/* V2R request from BUS Layer [sqs_v2r_request]  */
	ret = wlc_sqs_pull_packets_cb(ringid, v2r_request);

	/* Return successfully submitted v2r count */
	return ret;
}

/** TAF evaluation entry point. */
void
wlc_sqs_taf_txeval_trigger(void)
{
	WLC_SQS_DEBUG(("%s : Kick the TAF scheduler \n", __FUNCTION__));

#ifdef WLTAF
	/* check if TAF is enabled and not in bypass state */
	if (wlc_taf_in_use(WLC_SQS_WLC(WLC_SQS_G)->taf_handle)) {
		wlc_taf_schedule(WLC_SQS_WLC(WLC_SQS_G)->taf_handle, TAF_SQS_TRIGGER_TID, NULL,
			FALSE);
	}
#endif // endif
}

/* TAF End of pull set request
 * Triggered by TAF packet evaluation routine at the end of a schedule cycle.
 *
 * 1. If there are pending V2R requests, submit a request to BUS layer
 * 	to mark a PENDING_RESPONSE signal.
 * 2. If EoPS request raised multiple times without a new V2R submission,
 * 	EoPS request is dropped.
 * 3. If no V2R, send EoPS response immediately
 */
void
wlc_sqs_eops_rqst(void)
{
	int ret = -1;
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;
	ASSERT(sqs_info);

	WLC_SQS_DEBUG(("%s : Trigger EoPS request \n", __FUNCTION__));

	if (WLC_SQS_V2R_INTRANSIT(sqs_info)) {
		/* Submit a end of pull pull set request to BUS layer */
		ret = wlc_sqs_eops_rqst_cb();

		if (ret == BCME_OK) {
			/* Outstanding EoPS request count */
			WLC_SQS_EOPS_INTRANSIT(sqs_info)++;
		}
	} else {
		/* Outstanding EoPS request count */
		WLC_SQS_EOPS_INTRANSIT(sqs_info)++;

		/* No outstanding V2r requests. Send the EOPS response */
		wlc_sqs_eops_response();
	}
}
/* TAF End of pull set request */
void
wlc_sqs_eops_response(void)
{
	wlc_sqs_info_t *sqs_info = WLC_SQS_G;

	ASSERT(sqs_info);

	WLC_SQS_DEBUG(("%s : EoPS response \n", __FUNCTION__));

	/* Outstanding EoPS request count */
	ASSERT(WLC_SQS_EOPS_INTRANSIT(sqs_info));

	WLC_SQS_EOPS_INTRANSIT(sqs_info)--;

#ifdef WLTAF
	/* check if TAF is enabled and not in bypass state */
	if (wlc_taf_in_use(WLC_SQS_WLC(sqs_info)->taf_handle)) {
		/* TAF EOPS response callback */
		wlc_taf_v2r_complete(WLC_SQS_WLC(sqs_info)->taf_handle);
	}
#endif /* WLTAF */
}
