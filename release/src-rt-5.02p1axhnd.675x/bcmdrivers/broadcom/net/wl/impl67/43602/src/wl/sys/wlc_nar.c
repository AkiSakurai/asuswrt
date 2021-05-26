/**
 * Non Aggregated Regulation
 *
 * @file
 * @brief
 * This module contains the NAR transmit module. Hooked in the transmit path at the same
 * level as the A-MPDU transmit module, it provides balance amongst MPDU and AMPDU traffic
 * by regulating the number of in-transit packets for non-aggregating stations.
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
 * $Id$
 *
 */

/*
 * Include files.
 */
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <d11.h>
#include <wlioctl.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#ifdef BCM_HOST_MEM_SCB
#include <wlc_scb_alloc.h>
#endif // endif
#include <wlc_scb_ratesel.h>	/* wlc_scb_ratesel_get_primary() */
#include <wlc_tso.h>		/* wlc_tso_hdr_length(), for PKTQ_LOG */
#include <wlc_ampdu.h>
#include <wlc_rate.h>
#include <wlc_nar.h>
#include <wlc_pcb.h>

#ifdef WLATF
#include <wlc_airtime.h>
#include <wlc_prot.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#endif /*  WLATF */

/*
 * Initial parameter settings. Most can be tweaked by wl debug commands. Some we may want to
 * make tuneables, others bcminternal as they should not really be used in production.
 */
#define NAR_QUEUE_LENGTH         128	/* Configurable - size for station pkt queues */
#define NAR_MIN_QUEUE_LEN	  128	/* Fixed - minimum pkt queue length */
#define NAR_MAX_TRANSIT_PACKETS   128	/* Configurable - Limit of transit packets */
#define NAR_MAX_RELEASE           32	/* Configurable - number of pkts to release at once */
#define NAR_TX_STUCK_TIMEOUT	    5	/* Fixed - seconds before reset of in-transit count */

/*
 * Maximum in flight time in microseconds
 */
#define NAR_DEFAULT_TRANSIT_US	4000

#if defined(BCMDBG)		/* Only keep stats if we can display them (via dump cmd) */
#define NAR_STATS (1)
#define WL_NAR_MSG(m) WL_SPARE1(m)	/* WL_TRACE(m) */
#else
#undef NAR_STATS
#define WL_NAR_MSG(m)			/* nada */
#endif /* BCMDBG */

#if defined(NAR_STATS)
/*
 * If we do statistics, store the counters in an array, indexed by the enum below.
 * Makes it easy to display them.
 */
enum {
	NAR_COUNTER_PACKETS_RECEIVED	= 0,	/* packets handed to us for processing */
	NAR_COUNTER_PACKETS_DROPPED	= 1,	/* packets we failed to queue */
	NAR_COUNTER_PACKETS_QUEUED	= 2,	/* packets we successfully queued */
	NAR_COUNTER_PACKETS_PASSED_ON	= 3,	/* packets we passed on for transmission */
	NAR_COUNTER_PACKETS_TRANSMITTED = 4,	/* packets whose transmission was notified */
	NAR_COUNTER_PACKETS_DEQUEUED	= 5,	/* packets dequeued from queue */
	NAR_COUNTER_PACKETS_TX_NO_TRANSIT = 6,	/* xmit status with no packets in transit */
	NAR_COUNTER_AMPDU_TXS_IGNORED	= 7,	/* xmit status for ampdu and ignoring ampdu */
	NAR_COUNTER_KICKSTARTS_IN_WD	= 8,	/* Transmission kickstarted in watchdog */
	NAR_COUNTER_KICKSTARTS_IN_INT	= 9,	/* Transmission kickstarted in interrupt */
	NAR_COUNTER_KICKSTARTS_IN_TX	= 10,	/* Transmission kickstarted in tx function */
	NAR_COUNTER_KICKSTARTS_IN_TX_QF = 11,	/* Transmission kickstarted in tx function */
	NAR_COUNTER_PEAK_TRANSIT	= 12,	/* Peak in-transit packets */
	NAR_COUNTER_TXRESET		= 13,	/* Number of in-transit counter resets */
	NAR_COUNTER_TX_STATUS		= 14,	/* Number of dotxstatus events */
	NAR_COUNTER_NCONS		= 15,	/* Packets consumed */
#if defined(PKTQ_LOG)
	NAR_COUNTER_ACKED		= 16,	/* Packets acked */
	NAR_COUNTER_NOT_ACKED		= 17,	/* Packets not acked */
#endif /* PKTQ_LOG */
	NAR_COUNTER_AGGREGATING_CHANGED = 18,   /* Number of block ack status changes */
	NAR_COUNTER_PACKETS_AGGREGATING = 19,   /* Number of packets passed to AMPDU txmod */
	NAR_COUNTER_MAX       /* end marker. Remember to update cmap[] below */
};

typedef struct {
	uint32          counter[NAR_COUNTER_MAX];
} nar_stats_t;

static void
nar_stats_dump(nar_stats_t * stats, struct bcmstrbuf *b)
{
	static struct {
		int             cval;
		const char     *cname;
	} cmap[NAR_COUNTER_MAX] = {
	    {NAR_COUNTER_PACKETS_RECEIVED, "Packets Received"},
	    {NAR_COUNTER_PACKETS_DROPPED, "Packets Dropped (Q full)"},
	    {NAR_COUNTER_PACKETS_AGGREGATING, "Aggregating passed on"},
	    {NAR_COUNTER_AGGREGATING_CHANGED, "Aggregation changed"},
	    {NAR_COUNTER_PACKETS_QUEUED, "Packets Queued"},
	    {NAR_COUNTER_PACKETS_DEQUEUED, "Packets Dequeued"},
	    {NAR_COUNTER_PACKETS_PASSED_ON, "Packets passed down"},
	    {NAR_COUNTER_PACKETS_TRANSMITTED, "Packets tx done"},
	    {NAR_COUNTER_AMPDU_TXS_IGNORED, "tx done ignored (ampdu)"},
	    {NAR_COUNTER_PACKETS_TX_NO_TRANSIT, "tx done ignored (other)"},
	    {NAR_COUNTER_KICKSTARTS_IN_WD, "kickstarts in watchdog"},
	    {NAR_COUNTER_TXRESET, "tx stuck reset"},
	    {NAR_COUNTER_KICKSTARTS_IN_INT, "kickstarts in tx done"},
	    {NAR_COUNTER_KICKSTARTS_IN_TX, "kickstarts in tx"},
	    {NAR_COUNTER_KICKSTARTS_IN_TX_QF, "kickstarts in tx, Q full"},
	    {NAR_COUNTER_PEAK_TRANSIT, "Peak transit packets"},
	    {NAR_COUNTER_TX_STATUS, "TX Done Events"},
	    {NAR_COUNTER_NCONS, "NCONS"},
#if defined(PKTQ_LOG)
	    {NAR_COUNTER_ACKED, "Packets ACKed"},
	    {NAR_COUNTER_NOT_ACKED, "Packets not ACKed"},
#endif /* PKTQ_LOG */
	};

	int             i;

	for (i = 0; i < NAR_COUNTER_MAX; ++i) {
		bcm_bprintf(b, "%25s : %9u%c", cmap[i].cname,
			stats->counter[cmap[i].cval], (i & 1) ? '\n' : ' ');
	}
	bcm_bprintf(b, "\n");
}

#define NAR_STATS_DUMP(sptr, bptr) nar_stats_dump(sptr, bptr)
#define NAR_STATS_GET(ctx, ctr) (ctx)->stats.counter[NAR_COUNTER_##ctr]
#define NAR_STATS_PEAK(ctx, ctr, val) do { \
    if ((val) > (ctx)->stats.counter[NAR_COUNTER_##ctr]) { \
	(ctx)->stats.counter[NAR_COUNTER_##ctr] = (val); \
	} \
} while (0)

#define NAR_STATS_INC(scb, ctr) do { \
	(scb)->stats.counter[NAR_COUNTER_##ctr]++; \
	(scb)->nit->stats.counter[NAR_COUNTER_##ctr]++; \
} while (0)

#else /* not NAR_STATS */

#define NAR_STATS_DUMP(sptr, b)	/* nada */
#define NAR_STATS_INC(scb, ctr)	/* nada */
#define NAR_STATS_GET(ctx, ctr)	/* nada */
#define NAR_STATS_PEAK(ctx, ctr, val)	/* nada */

#endif /* NAR_STATS */

/*
 * The next two are not for statistics, they are used for control
 * purposes: do not comment out!
 */

#define NAR_COUNTER_INC(scb, ctr) do { (scb)->ctr++; ASSERT((scb)->ctr != 0); \
	(scb)->nit->ctr++; } while (0)

#define NAR_COUNTER_DEC(scb, ctr) do { ASSERT((scb)->ctr > 0); (scb)->ctr--; \
	(scb)->nit->ctr--; } while (0)

/* Find basic rate for a given rate */
#define NAR_BASIC_RATE(band, rspec)	((band)->basic_rate[RSPEC_REFERENCE_RATE(rspec)])
/*
 * Driver level data.
 */
struct wlc_nar_info {

	wlc_info_t     *wlc;		/* backlink to wlc */
	int             scb_handle;	/* handle to scb structure (offset within scb) */

	uint32          transit_packet_limit;	/* per-scb transit limit */
	uint32          packets_in_queue;	/* #packets in SCB queues, waiting to be sent */
	uint32          packets_in_transit;	/* #packets in transit, waiting for tx done */
	uint16          release_at_once;	/* per-scb release limit */
	uint16          queue_length;		/* Length of per-scb pktq */
	bool            nar_allowed;		/* feature is allowed */
	bool            nar_enabled;		/* feature is enabled (active) */
	bool		ampdu_scb_present;	/* One or more SCBs became AMPDU capable */
	bool		nar_handle_ampdu;	/* Do we handle AMPDU-capable stations ? */
#if defined(NAR_STATS)
	nar_stats_t    stats;		/* various counters */
#endif // endif
#ifdef WLATF
	uint		txq_time_allowance_us;	/* Time value of packets in transit */
#endif // endif
};

/*
 * Per SCB data, malloced, to which a pointer is stored in the SCB cubby.
 * Note these are only allocated if the SCB is not ignored (ie, internal or AMPDU) and
 * the feature is enabled - in all other cases the pointer will be null. Ye be warned.
 */
typedef struct {

	wlc_nar_info_t *nit;		/* backlink to nit */
	struct scb     *scb_bl;		/* backlink to current scb */
	osl_t		*osh;		/* backlink to osh */

	struct pktq     tx_queue;
	uint32          tx_stuck_time;	/* in seconds - used for watchdog cleanup */

	uint32          packets_in_transit;	/* not just stats - used for control */
	uint32          packets_in_queue;	/* not just stats - used for control */

#if defined(NAR_STATS)
	nar_stats_t    stats;		/* various counters */
#endif // endif
	uint8		aggr_prio_mask;	/* mask of aggregating priorities (TIDs) */

#ifdef WLATF
	uint		packet_airtime_us;	/* Released and inflight airtime */
	uint		txq_time_allowance_us;	/* Packet airtime release limit (us) */

	/* Cached vars used to reduce calculation effort */
	uint		last_packet_rspec;	/* Estimated rspec of last packet */
	uint		last_packet_nbytes;	/* Size of last released packet in bytes */
	uint		last_packet_airtime_us;	/* Estimated airtime of last released packet */
	uint		last_packet_overhead_us;
	uint		last_ack_rspec;	/* Most recent lookup of ACK packet rate */
	uint		last_ctrl_rspec;
	bool		last_state_nProt;
	bool		last_state_gProt;
	uint		last_state_flags;
#endif // endif
} nar_scb_cubby_t;

#define SCB_NAR_CUBBY_PTR(nit, scb) ((nar_scb_cubby_t **)(SCB_CUBBY((scb), (nit)->scb_handle)))
#define SCB_NAR_CUBBY(nit, scb) (*SCB_NAR_CUBBY_PTR(nit, scb))

/*
 * Forward declarations.
 */
static int BCMFASTPATH wlc_nar_release_from_queue(nar_scb_cubby_t * cubby, int prec);
static bool BCMFASTPATH wlc_nar_fair_share_reached(nar_scb_cubby_t * cubby);
static void BCMFASTPATH wlc_nar_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs);
static void wlc_nar_scbcubby_exit(void *handle, struct scb *scb);

/*
 * Module iovar handling.
 */
enum {
	IOV_NAR,		       /* Global on/off switch */
	IOV_NAR_HANDLE_AMPDU,	       /* on/off switch for handling ampdu-capable STAs */
	IOV_NAR_TRANSIT_LIMIT,	       /* max packets in transit allowed */
#if defined(NAR_STATS)
	IOV_NAR_CLEAR_DUMP	       /* Clear statistics (aka dump, like for ampdu) */
#endif // endif
};

static const bcm_iovar_t nar_iovars[] = {
	{"nar", IOV_NAR, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"nar_handle_ampdu", IOV_NAR_HANDLE_AMPDU, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"nar_transit_limit", IOV_NAR_TRANSIT_LIMIT, 0, IOVT_UINT32, 0},
#if defined(NAR_STATS)
	{"nar_clear_dump", IOV_NAR_CLEAR_DUMP, 0, IOVT_VOID, 0},
#endif // endif
	{NULL, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#if defined(NAR_STATS)
static void
nar_stats_clear_all(wlc_nar_info_t * nit)
{
	struct scb     *scb;
	struct scb_iter scbiter;

	memset(&nit->stats, 0, sizeof(nit->stats));

	if (nit->nar_enabled) {
	    FOREACHSCB(nit->wlc->scbstate, &scbiter, scb) {
		nar_scb_cubby_t *cubby;

		cubby = SCB_NAR_CUBBY(nit, scb);

		if (cubby) {
		    memset(&cubby->stats, 0, sizeof(cubby->stats));
		}
	    }
	}
}
#endif /* NAR_STATS */

#if defined(PKTQ_LOG)
/*
 * PKTQ_LOG helper function, called by the "wl pktq_stats -m<macaddress>" command.
 * Returns the address of the pktq, or NULL if no pktq exists for the passed scb.
 */
struct pktq *
wlc_nar_prec_pktq(wlc_info_t * wlc, struct scb *scb)
{
	wlc_nar_info_t *nit;
	nar_scb_cubby_t *cubby;

	nit = wlc->nar_handle;

	if (!nit->nar_enabled) {
		return NULL;
	}

	cubby = SCB_NAR_CUBBY(nit, scb);

	return ((cubby) ? &cubby->tx_queue : NULL);

}
#endif /* PKTQ_LOG */

static int
wlc_nar_doiovar(void *handle, const bcm_iovar_t * vi, uint32 actionid,
	const char *name, void *params, uint plen, void *arg,
	int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_nar_info_t *nit = handle;
	int32           int_val = 0;
	int32          *ret_int_ptr = arg;
	int             status = BCME_OK;

	if (plen >= sizeof(int_val)) {
		memcpy(&int_val, params, sizeof(int_val));
	}

	switch (actionid) {

		case IOV_GVAL(IOV_NAR):
			*ret_int_ptr = nit->nar_allowed;
			break;

		case IOV_SVAL(IOV_NAR): /* can only be set when down, so no cleanup is needed */
			nit->nar_allowed = (int_val) ? 1 : 0;
			break;

		case IOV_GVAL(IOV_NAR_HANDLE_AMPDU):
			*ret_int_ptr = nit->nar_handle_ampdu;
			break;

		case IOV_SVAL(IOV_NAR_HANDLE_AMPDU): /* can only be set when down */
			nit->nar_handle_ampdu = (int_val) ? 1 : 0;
			break;

		case IOV_GVAL(IOV_NAR_TRANSIT_LIMIT):
			*ret_int_ptr = nit->transit_packet_limit;
			break;

		case IOV_SVAL(IOV_NAR_TRANSIT_LIMIT):
			nit->transit_packet_limit = MAX(1, int_val);
			break;

#if defined(NAR_STATS)
		case IOV_SVAL(IOV_NAR_CLEAR_DUMP):
			nar_stats_clear_all(nit);
			break;
#endif /* NAR_STATS */

		default:
			status = BCME_UNSUPPORTED;
			break;

	}
	return status;
}

/*
 * Module attach and detach functions.
 */
static int
wlc_nar_watchdog(void *handle)
{
	wlc_nar_info_t *nit = handle;
	/*
	 * XXX Better safe than sorry, or am I being too paranoid here ?
	 * If there are packets in one or more SCB queues, this may be normal, or
	 * they may have been forgotten somehow. Kickstart the queue emptying.
	 * Likewise, if an SCB has changed to aggregating, clean up and forget it.
	 */

	if (nit->nar_enabled && (nit->packets_in_queue || nit->ampdu_scb_present)) {

	    struct scb     *scb;
	    struct scb_iter scbiter;

	    FOREACHSCB(nit->wlc->scbstate, &scbiter, scb) {
		nar_scb_cubby_t *cubby;

		cubby = SCB_NAR_CUBBY(nit, scb);

		if (cubby) {

		    if (cubby->packets_in_queue) {

			int             prec;

			/*
			 * If there has been no tx activity for a while, we
			 * may have some stuck in transit counts (ie, because
			 * the lower layers dropped the packets). Clean up
			 * before trying to dequeue.
			 */
			if (wlc_nar_fair_share_reached(cubby)) {
			    ++cubby->tx_stuck_time;
			    if (cubby->tx_stuck_time > NAR_TX_STUCK_TIMEOUT) {
				NAR_STATS_INC(cubby, TXRESET);
				nit->packets_in_transit -= cubby->packets_in_transit;
				cubby->packets_in_transit = 0;
#ifdef WLATF
				cubby->packet_airtime_us = 0;
#endif /* WLATF */
			    }
			} else {
			    /*
			     * Below fair share, so not stuck.
			     */
			    cubby->tx_stuck_time = 0;
			}

			for (prec = 0; prec < WLC_PREC_COUNT; ++prec) {

			    if (wlc_nar_release_from_queue(cubby, prec)) {
				/*
				 * Released some, so not stuck.
				 */
				cubby->tx_stuck_time = 0;
				NAR_STATS_INC(cubby, KICKSTARTS_IN_WD);
			    } /* released some packets */
			} /* for each precedence */
		    } /* packets in queue */

		    if (SCB_AMPDU(scb)) {
			/*
			 * The link is AMPDU-capable, see if we handle these.
			 */
			if (nit->nar_handle_ampdu) {

			    /* update mask, it may have changed. */

			    int apm = wlc_ampdu_ba_on_tidmask(scb);
			    if (apm != cubby->aggr_prio_mask) {
				WL_NAR_MSG(("%s: Agg mask changed from %02x to %02x for scb %p\n",
					__FUNCTION__, cubby->aggr_prio_mask, apm, scb));
				NAR_STATS_INC(cubby, AGGREGATING_CHANGED);
				cubby->aggr_prio_mask = (uint8)apm;
			    }
			} else {
			    /*
			     * We are not handling AMPDU-capable stations, and the link has become
			     * AMPDU-capable. Forget it, releasing associated memory. This will
			     * clear the cubby pointer, and thus we will not get here ever again.
			     */
			    WL_NAR_MSG(("%s: Releasing AMPDU Cubby for scb %p\n",
			        __FUNCTION__, scb));
			    wlc_nar_scbcubby_exit(handle, scb);
			}

		    } else { /* SCB is not AMPDU, clear the mask */
			cubby->aggr_prio_mask = 0;
		    }

		} /* cubby != NULL */
	    } /* for each SCB */

	    nit->ampdu_scb_present = FALSE;   /* We released all of those in the loop */

	} /* enabled and something to do */

	return BCME_OK;
}

static int
wlc_nar_up(void *handle)
{
	wlc_nar_info_t *nit = handle;

	nit->nar_enabled = nit->nar_allowed;

	return BCME_OK;
}

static void
wlc_nar_reset_release_state(nar_scb_cubby_t *cubby)
{
	    cubby->packets_in_queue = 0;
	    cubby->packets_in_transit = 0;
#ifdef WLATF
	    cubby->last_packet_rspec = 0;
	    cubby->last_packet_nbytes = 0;
	    cubby->last_packet_airtime_us = 0;
	    cubby->packet_airtime_us = 0;
	    cubby->last_ack_rspec = 0;
	    cubby->last_ctrl_rspec = 0;
	    cubby->last_state_nProt = FALSE;
	    cubby->last_state_gProt = FALSE;
	    cubby->last_state_flags = 0;
#endif // endif
}

static void
wlc_nar_flush_scb_queues(wlc_nar_info_t * nit, struct scb *scb)
{
	nar_scb_cubby_t *cubby;

	cubby = SCB_NAR_CUBBY(nit, scb);

	if (cubby) {
	    /* Flush queue */
	    pktq_flush(cubby->osh, &cubby->tx_queue, TRUE, NULL, 0);

	    /* Reset counters */
	    nit->packets_in_queue -= cubby->packets_in_queue;
	    nit->packets_in_transit -= cubby->packets_in_transit;
	    wlc_nar_reset_release_state(cubby);
	}
}

static void
wlc_nar_flush_all_queues(wlc_nar_info_t * nit)
{
	struct scb     *scb;
	struct scb_iter scbiter;
	FOREACHSCB(nit->wlc->scbstate, &scbiter, scb) {

	    wlc_nar_flush_scb_queues(nit, scb);

	}
}

/*
 * Interface going down. Flush queues and clean up a.
 */
static int
wlc_nar_down(void *handle)
{
	wlc_nar_info_t *nit = handle;

	if (nit->nar_enabled) {

		nit->nar_enabled = FALSE;

		wlc_nar_flush_all_queues(nit);

		nit->packets_in_queue = 0;
		nit->packets_in_transit = 0;
	}
	return BCME_OK;
}

#if defined(BCMDBG)
static int
wlc_nar_dump(void *handle, struct bcmstrbuf *b)
{
	wlc_nar_info_t *nit = handle;
	struct scb     *scb;
	struct scb_iter scbiter;
	int             i;

#define _VPRINT(nl, s, v) bcm_bprintf(b, "%25s : %9u%c", s, v, (nl++&1) ? '\n':' ')

	i = 0;
	_VPRINT(i, "nar regulation enabled", nit->nar_allowed);
	_VPRINT(i, "nar regulation active", nit->nar_enabled);
	_VPRINT(i, "ampdu capable STAs seen", nit->ampdu_scb_present);
	_VPRINT(i, "handle ampdu regulation", nit->nar_handle_ampdu);
	_VPRINT(i, "station in-transit limit", nit->transit_packet_limit);
	_VPRINT(i, "packet queue length", nit->queue_length);
	_VPRINT(i, "packets now in queues", nit->packets_in_queue);
	_VPRINT(i, "packets now in transit", nit->packets_in_transit);
#ifdef WLATF
	_VPRINT(i, "txq_time_allowance_us(us)", nit->txq_time_allowance_us);
#endif // endif
	bcm_bprintf(b, "\n");

	NAR_STATS_DUMP(&nit->stats, b);

	/*
	 * Dump all SCBs.
	 */
	FOREACHSCB(nit->wlc->scbstate, &scbiter, scb) {

		char            eabuf[ETHER_ADDR_STR_LEN];

		bcm_bprintf(b, "-------- Station : %s, %s, rate %dkbps\n",
			bcm_ether_ntoa(&scb->ea, eabuf),
			SCB_AMPDU(scb) ? "AMPDU Capable" : "Legacy",
			wlc_rate_rspec2rate(wlc_scb_ratesel_get_primary(nit->wlc, scb, NULL)));

		if (nit->nar_enabled) {

			nar_scb_cubby_t *cubby;

			cubby = SCB_NAR_CUBBY(nit, scb);

			if (cubby) {
			    i = 0;
			    _VPRINT(i, "packets in station queues", cubby->packets_in_queue);
			    _VPRINT(i, "packets in transit", cubby->packets_in_transit);
			    _VPRINT(i, "seconds in transit", cubby->tx_stuck_time);
			    _VPRINT(i, "aggregation prec mask", cubby->aggr_prio_mask);
#ifdef WLATF
			    _VPRINT(i, "txq_time_allowance_us(us)", cubby->txq_time_allowance_us);
			    _VPRINT(i, "released airtime (us)", cubby->packet_airtime_us);
			    _VPRINT(i, "last packet rspec", cubby->last_packet_rspec);
			    _VPRINT(i, "last packet nbytes", cubby->last_packet_nbytes);
			    _VPRINT(i, "last packet airtime(us)", cubby->last_packet_airtime_us);
#endif // endif
			    bcm_bprintf(b, "\n");

			    NAR_STATS_DUMP(&cubby->stats, b);
			}
		}
	}

#undef _VPRINT

	return BCME_OK;
}
#endif /* BCMDBG */

/*
 * Initialise module parameters.
 */

static void
wlc_nar_module_init(wlc_nar_info_t * nit)
{
	nit->queue_length = NAR_QUEUE_LENGTH;

	/*
	 * Set the transit limit to be the same as the AMPDU limit.
	 */

	nit->transit_packet_limit = NAR_MAX_TRANSIT_PACKETS;

	nit->release_at_once = NAR_MAX_RELEASE;

	nit->nar_allowed = TRUE;

	nit->nar_handle_ampdu = TRUE;

#ifdef WLATF
	nit->txq_time_allowance_us = NAR_DEFAULT_TRANSIT_US;
#endif /* WLATF */
}

/*
 * SCB cubby functions.
 */

static int
wlc_nar_scbcubby_init(void *handle, struct scb *scb)
{
	wlc_nar_info_t *nit = handle;
	nar_scb_cubby_t *cubby;

	if (nit->nar_enabled &&	/* Only register txmod if enabled */
	    !SCB_INTERNAL(scb)) { /* Ignore internal SCBs */
#ifdef BCM_HOST_MEM_SCB
		if (SCB_ALLOC_ENAB(nit->wlc->pub) && !SCB_INTERNAL(scb) && SCB_HOST(scb)) {
			cubby = (nar_scb_cubby_t *)wlc_scb_alloc_mem_get(nit->wlc,
				SCB_CUBBY_ID_NAR, sizeof(nar_scb_cubby_t), 1);
		}
		else
#endif /* BCM_HOST_MEM_SCB */
		cubby = MALLOC(nit->wlc->osh, sizeof(nar_scb_cubby_t));
		if (cubby == NULL) {
		    return BCME_NOMEM;
		}
		/*
		 * Initialise the cubby.
		 */
		memset(cubby, 0, sizeof(nar_scb_cubby_t));
		cubby->nit = nit;
		cubby->osh = nit->wlc->osh;
		cubby->scb_bl = scb;	/* save the scb back link */

#ifdef WLATF
		cubby->txq_time_allowance_us = nit->txq_time_allowance_us;
#endif /* WLATF */

		wlc_nar_reset_release_state(cubby);

		pktq_init(&cubby->tx_queue, WLC_PREC_COUNT, nit->queue_length);

		*SCB_NAR_CUBBY_PTR(nit, scb) = cubby;

		wlc_txmod_config(nit->wlc, scb, TXMOD_NAR);
	} else {
	    *SCB_NAR_CUBBY_PTR(nit, scb) = NULL;
	}
	return BCME_OK;
}

/*
 * wlc_nar_scbcubby_exit() - clean up and strip a cubby from an scb.
 *
 * Called either by scb functions, or from our internal watchdog when an SCB has become
 * aggregating, to release anything associated with the cubby and the cubby itself, and
 * clear the scb cubby pointer.
 *
 */
static void
wlc_nar_scbcubby_exit(void *handle, struct scb *scb)
{
	wlc_nar_info_t *nit = handle;
	nar_scb_cubby_t* cubby = *SCB_NAR_CUBBY_PTR(nit, scb);

	if (cubby != NULL) {

		wlc_txmod_unconfig(nit->wlc, scb, TXMOD_NAR);

		wlc_nar_flush_scb_queues(nit, scb);

#ifdef PKTQ_LOG
		wlc_pktq_stats_free(nit->wlc, &cubby->tx_queue);
#endif // endif
#ifdef BCM_HOST_MEM_SCB
		if (SCB_ALLOC_ENAB(nit->wlc->pub) && SCB_HOST(scb)) {
			wlc_scb_alloc_mem_free(nit->wlc, SCB_CUBBY_ID_NAR, (void *)cubby);
		}
		else
#endif /* BCM_HOST_MEM_SCB */
		MFREE(nit->wlc->osh, cubby, sizeof(*cubby));

		*SCB_NAR_CUBBY_PTR(nit, scb) = NULL;
	}
}

/*
 * Module initialisation and cleanup.
 */

static void BCMFASTPATH wlc_nar_transmit_packet(void *handle, struct scb *, void *pkt, uint prec);
static uint BCMFASTPATH wlc_nar_count_queued_packets(void *handle);

static const txmod_fns_t nar_txmod_fns = {
	wlc_nar_transmit_packet,
	wlc_nar_count_queued_packets,
	NULL, NULL
};

wlc_nar_info_t *
BCMATTACHFN(wlc_nar_attach) (wlc_info_t * wlc)
{
	int             status;
	wlc_nar_info_t *nit;

	/*
	 * Allocate and initialise our main structure.
	 */
	nit = MALLOC(wlc->pub->osh, sizeof(wlc_nar_info_t));
	if (!nit) {
		return NULL;
	}

	memset(nit, 0, sizeof(*nit));

	nit->wlc = wlc;		       /* save backlink to wlc */

	wlc_nar_module_init(nit);     /* set up module parameters */

	/* register packet class callback */
	if (wlc_pcb_fn_set(wlc->pcb, 2, WLF2_PCB3_NAR, wlc_nar_pkt_freed) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_pcb_fn_set() failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(wlc->pub->osh, nit, sizeof(*nit));
		return NULL;
	}

	status = wlc_module_register(wlc->pub, nar_iovars, "nar", nit,
		wlc_nar_doiovar, wlc_nar_watchdog, wlc_nar_up, wlc_nar_down);

	if (status == BCME_OK) {
		/*
		 * set up an scb cubby - returns an offset or -1 on failure.
		 */
		if ((nit->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(nar_scb_cubby_t *),
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
			wlc_nar_scbcubby_init, wlc_nar_scbcubby_exit, NULL, nit, 0)) < 0) {
#else
			wlc_nar_scbcubby_init, wlc_nar_scbcubby_exit, NULL, nit)) < 0) {
#endif // endif
			status = BCME_NORESOURCE;

			wlc_module_unregister(wlc->pub, "nar", nit);

		}
	}

	if (status != BCME_OK) {
		MFREE(wlc->pub->osh, nit, sizeof(*nit));
		return NULL;
	}
#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "nar", wlc_nar_dump, nit);
#endif /* BCMDBG */

	nit->nar_enabled = nit->nar_allowed;

	wlc_txmod_fn_register(wlc, TXMOD_NAR, nit, nar_txmod_fns);

	return nit;					/* all fine, return handle */
}

int
BCMATTACHFN(wlc_nar_detach) (wlc_nar_info_t *nit)
{
	/*
	 * Flush scb queues
	 */
	if (nit->nar_enabled) {
		wlc_nar_flush_all_queues(nit);
	}

	wlc_module_unregister(nit->wlc->pub, "nar", nit);
	MFREE(nit->wlc->pub->osh, nit, sizeof(*nit));

	return BCME_OK;
}

/*
 * The actual txmod packet processing functions.
 */

/*
 * Test whether the nar tx queue is full.
 */
static bool     BCMFASTPATH
wlc_nar_queue_is_full(nar_scb_cubby_t * cubby)
{
	return (pktq_len(&cubby->tx_queue) >= cubby->nit->queue_length);
}

/*
 * Place a packet on the corresponding nar precedence queue.
 * Updates the count of packets in queue and returns true or false to indicate
 * success or failure.
 */
static bool     BCMFASTPATH
wlc_nar_enqueue_packet(nar_scb_cubby_t * cubby, void *pkt, uint prec)
{

	WL_NAR_MSG(("%30s: prec %d\n", __FUNCTION__, prec));

	if (wlc_prec_enq(cubby->nit->wlc, &cubby->tx_queue, pkt, prec)) {
		NAR_STATS_INC(cubby, PACKETS_QUEUED);
		NAR_COUNTER_INC(cubby, packets_in_queue);
		cubby->tx_stuck_time = 0;
		return TRUE;
	}
	return FALSE;
}

/*
 * Dequeue a packet from one of the nar precedence queues, and adjust the count of packets.
 * Returns the dequeued packet, or NULL.
 */
static void    *BCMFASTPATH
wlc_nar_dequeue_packet(nar_scb_cubby_t * cubby, uint prec)
{
	void           *p;

	WL_NAR_MSG(("%30s: prec %d\n", __FUNCTION__, prec));

	p = pktq_pdeq(&cubby->tx_queue, prec);

	if (p) {
		cubby->tx_stuck_time = 0;
		NAR_STATS_INC(cubby, PACKETS_DEQUEUED);
		NAR_COUNTER_DEC(cubby, packets_in_queue);
	}
	return p;
}

#ifdef WLATF
static uint BCMFASTPATH
wlc_nar_packet_airtime(nar_scb_cubby_t * cubby, uint prec, void *pkt)
{
	struct scb *scb = cubby->scb_bl;
	ratespec_t rspec;
	bool useRTS;
	bool gProt = FALSE;
	bool nProt = FALSE;
	bool shortPreamble = FALSE;
	uint ctl_rspec;
	uint ack_rspec;
	uint pkt_overhead_us = 0;
	uint payload_us = 0;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc;
	wlcband_t *band;
	uint nbytes;
	uint flags = 0;

	ASSERT(scb->bsscfg);

	bsscfg = scb->bsscfg;
	wlc = bsscfg->wlc;

	rspec =  wlc_scb_ratesel_get_primary(wlc, scb, NULL);
	nbytes = pkttotlen(cubby->osh, pkt) + wlc_airtime_dot11hdrsize(scb->wsec);
	gProt = (wlc->band->gmode && !IS_CCK(rspec) && WLC_PROT_G_CFG_G(wlc->prot_g, bsscfg));
	nProt = ((WLC_PROT_N_CFG_N(wlc->prot_n, bsscfg) == WLC_N_PROTECTION_20IN40) &&
		RSPEC_IS40MHZ(rspec));

	if ((rspec == cubby->last_packet_rspec) &&
		(nbytes == cubby->last_packet_nbytes) &&
		(gProt == cubby->last_state_gProt) &&
		(nProt == cubby->last_state_nProt))
	{
		return cubby->last_packet_airtime_us;
	}

	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec)];

	/* XXX
	 * Assume Mixed mode as the cost of calculating  is too high
	 * Can change if we find an advantage to GF calculation
	 */
	flags |= (WLC_AIRTIME_MIXEDMODE);

	/* Frame protection and RTS/CTS */

	useRTS = !SCB_ISMULTI(scb) &&
		((nbytes > wlc->RTSThresh) || (WLPKTTAG(pkt)->flags & WLF_USERTS));

	if (useRTS) {
		/* RTS/CTS mode */
		flags |= (WLC_AIRTIME_RTSCTS);
	}

	/*
	 * Note if useRTS is also set, we will do RTS CTS and if g prot is set,
	 * release time is calculated for 11Mbit/s or lower (non ERP rates)
	 */
	if (gProt || nProt) {
		/* CTS to self  */
		flags |= (WLC_AIRTIME_CTS2SELF);
	}

	/*
	 * Short slot for ERP STAs
	 * The AP bsscfg will keep track of all sta's shortslot/longslot cap,
	 * and keep current_bss->capability up to date.
	 */
	if (BAND_5G(band->bandtype) || bsscfg->current_bss->capability & DOT11_CAP_SHORTSLOT)
			flags |= (WLC_AIRTIME_SHORTSLOT);

	/*
	 * Short preamble:
	 * Specified for the ERP CCK rates
	 * If SCB is short preamble capable and and shortpreamble is enabled
	 * and rate is NOT 1M or Override is NOT set to LONG preamble then use SHORT preamble
	 * else use LONG preamble
	 * OFDM should bypass below preamble set, but old chips are OK since they ignore that bit
	 * XXX before 4331A0 PR79339 is fixed, restrict to CCK, which is actually correct
	 */
	shortPreamble = (WLC_PROT_CFG_SHORTPREAMBLE(wlc->prot, bsscfg) &&
		(scb->flags & SCB_SHORTPREAMBLE) && IS_CCK(rspec)) &&
		(!((RSPEC2RATE(rspec) == WLC_RATE_1M) ||
		(bsscfg->PLCPHdr_override == WLC_PLCP_LONG)));

	/* Calculate the ACK rate */
	if ((cubby->last_packet_rspec == rspec) &&
		(cubby->last_state_gProt == gProt) &&
		(cubby->last_state_nProt == nProt)) {

		/*
		 * If settings except the frame size remain the same,
		 * can reuse the overhead time previously.
		 */
		ack_rspec = cubby->last_ack_rspec;
		ctl_rspec = cubby->last_ctrl_rspec;
		if (flags == cubby->last_state_flags) {
			pkt_overhead_us = cubby->last_packet_overhead_us;
		} else {
			cubby->last_state_flags = flags;
		}
	} else {
		cubby->last_packet_rspec = rspec;

		ack_rspec = NAR_BASIC_RATE(band, rspec);
		if (shortPreamble)
			ack_rspec |= RSPEC_SHORT_PREAMBLE;
		cubby->last_ack_rspec = ack_rspec;

		/* Calculate  overhead time */
		if (gProt) {
			/* Use 11Mbits or lower as the control rate for 2.4G */
			ctl_rspec = NAR_BASIC_RATE(band, LEGACY_RSPEC(WLC_RATE_11M));
		} else {
			ctl_rspec = ack_rspec;
		}
		cubby->last_ctrl_rspec = ctl_rspec;
		cubby->last_state_flags = flags;
		cubby->last_packet_rspec = rspec;
		cubby->last_state_gProt = gProt;
		cubby->last_state_nProt = nProt;
	}

	if (!pkt_overhead_us) {
		/* The prec to AC converson should be converted to a macro if it is used
		 * more than once
		 */
		pkt_overhead_us = wlc_airtime_pkt_overhead_us(flags,
			ctl_rspec, ack_rspec, bsscfg, wme_fifo2ac[prec >> 2]);
		cubby->last_packet_overhead_us = pkt_overhead_us;
	}

	/* Calculate payload time including PLCP */
	payload_us = wlc_airtime_packet_time_us(flags, rspec, nbytes);

	cubby->last_packet_nbytes = nbytes;
	cubby->last_packet_airtime_us = payload_us + pkt_overhead_us;

	return cubby->last_packet_airtime_us;
}

static INLINE void
wlc_nar_dec_packet_airtime(nar_scb_cubby_t *cubby, void * pkt)
{
	uint airtime_us = WLPKTTAG(pkt)->pktinfo.atf.pkt_time;
	bool enabled  = (cubby->nit->wlc->atf != 0);

	WLPKTTAG(pkt)->pktinfo.atf.pkt_time = 0;

	if  ((enabled) && (airtime_us < cubby->packet_airtime_us)) {
		cubby->packet_airtime_us -= airtime_us;
	} else {
		cubby->packet_airtime_us = 0;
	}
}

static INLINE void
wlc_nar_add_packet_airtime(nar_scb_cubby_t *cubby, uint prec, void * pkt)
{
	bool enabled = (cubby->nit->wlc->atf != 0);
	uint airtime_us = (enabled) ? wlc_nar_packet_airtime(cubby, prec, pkt) : 0;

	WLPKTTAG(pkt)->pktinfo.atf.pkt_time = (uint16)airtime_us;
	cubby->packet_airtime_us += airtime_us;
}

static INLINE bool
wlc_nar_airtime_share_reached(nar_scb_cubby_t * cubby)
{
	bool enabled = (cubby->nit->wlc->atf != 0);
	return ((enabled) && (cubby->packet_airtime_us >= cubby->txq_time_allowance_us));
}
#endif /* WLATF */

/*
 * Test whether our fair share of transmits has been reached.
 *
 * Fair share is only based on the following:
 * a)Released packet airtime.
 * b)Number of packets in transit.
 */
static bool     BCMFASTPATH
wlc_nar_fair_share_reached(nar_scb_cubby_t * cubby)
{
#ifdef PSPRETEND
	wlc_info_t *wlc = cubby->nit->wlc;
	struct scb * scb = cubby->scb_bl;
	bool ps_pretend_limit_transit = SCB_PS_PRETEND(scb);
	/* scb->ps_pretend_start contains the TSF time for when ps pretend was
	 * last activated. In poor link conditions, there is a high chance that
	 * ps pretend will be re-triggered.
	 * Within a short time window following ps pretend, this code is trying to
	 * constrain the number of packets in transit and so avoid a large number of
	 * packets repetitively going between transmit and ps mode.
	 */
	if (!ps_pretend_limit_transit && SCB_PS_PRETEND_WAS_RECENT(scb)) {
		/* time elapsed in us since last pretend ps event */
		uint32 ps_pretend_time = R_REG(wlc->osh, &wlc->regs->tsf_timerlow) -
			scb->ps_pretend_start;

		/* arbitrary calculation, roughly we don't allow in_transit to be big
		 * when we recently have done a pretend ps. As time goes by, we gradually
		 * allow in_transit to increase. There could be some tuning to do for this.
		 * This is to prevent a large in_transit in the case where we are performing
		 * pretend ps regularly (of order every second or faster), as the time
		 * required to suppress a large number of packets in transit can cause
		 * the data path to choke.
		 * Tuning mechanism is same as AMPDU traffic but tuning parameters are
		 * 8 times lower.
		 * When pretend ps is currently active, we always prevent packet release.
		 */
		if (cubby->packets_in_transit > (5 + (ps_pretend_time >> 16 /* divide 65536 */))) {
			ps_pretend_limit_transit = TRUE;
		}

		if ((ps_pretend_time >> 10 /* divide 1024 */) > 2000) {
			/* After 2 seconds (approx) elapsed since last ps pretend, clear
			 * recent flag to avoid doing this calculation
			 */
			scb->ps_pretend &= ~PS_PRETEND_RECENT;
		}
	}
#ifdef PROP_TXSTATUS
	if (SCB_TXMOD_ACTIVE(scb, TXMOD_APPS) && PROP_TXSTATUS_ENAB(wlc->pub) &&
		HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
		/* If TXMOD_APPS is active, and proptxstatus is enabled, force release.
		 * With proptxstatus enabled, they are meant to go to wlc_apps_ps_enq()
		 * to get their pktbufs freed in dongle, and instead stored in the host.
		 */
		ASSERT(SCB_PS(scb));
		ps_pretend_limit_transit = FALSE;
	}
#endif /* PROP_TXSTATUS */
	/*
	 * Unless forced, ensure that we do not have too many packets in transit
	 * for this priority.
	 */
	if (ps_pretend_limit_transit) {
		return TRUE;
	}
#endif /* PSPRETEND */

	return (
#ifdef WLATF
		wlc_nar_airtime_share_reached(cubby) ||
#endif // endif
		(cubby->packets_in_transit >= cubby->nit->transit_packet_limit));
}

static void BCMFASTPATH
wlc_nar_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs)
{
	wlc_nar_info_t *nit = wlc->nar_handle;
	struct scb *scb;
	nar_scb_cubby_t *cubby;

	/* no packet */
	if (!pkt)
		return;

	scb = WLPKTTAGSCBGET(pkt);

	/*
	 * Return if we are not set up to handle the tx status for this scb.
	 */
	if (!scb ||		       /* no SCB */
	    !nit->nar_enabled ) {      /* Feature is not enabled */
		return;
	} else {
		struct scb     *newscb;
		struct scb_iter scbiter;
		int match = 0;

		/* Before using the scb, need to check if this is stale first */
		FOREACHSCB(nit->wlc->scbstate, &scbiter, newscb) {
			if (newscb == scb) {
				match = 1;
				break;
			}
		}
		if (!match) {
			/* If no matching scb found, ignore and return */
			WL_NAR_MSG(("%s: stale scb(%p), not accounting\n", __FUNCTION__, scb));
			return;
		}
	}

	cubby = SCB_NAR_CUBBY(nit, scb);
	if (!cubby) {
		return;
	}

	WL_NAR_MSG(("%30s: txs %08x, %d in transit\n",
		__FUNCTION__, txs, cubby->packets_in_transit));

	if (cubby->packets_in_transit) {
		NAR_STATS_INC(cubby, PACKETS_TRANSMITTED);

		NAR_COUNTER_DEC(cubby, packets_in_transit);
	} else {
		/*
		 * Transmit complete, but no packets in transit.
		 */
		NAR_STATS_INC(cubby, PACKETS_TX_NO_TRANSIT);
		if (SCB_AMPDU(scb)) {
			nit->ampdu_scb_present = TRUE;
		}
	}
#ifdef WLATF
	wlc_nar_dec_packet_airtime(cubby, pkt);
#endif // endif
}

/*
 * Pass a packet to the next txmod below, incrementing the in transit count.
 */
static void     BCMFASTPATH
wlc_nar_pass_packet_on(nar_scb_cubby_t * cubby, struct scb *scb, void *pkt, uint prec)
{
	WL_NAR_MSG(("%30s: prec %d transit count %d\n", __FUNCTION__,
		prec, cubby->packets_in_transit));

	WLF2_PCB3_REG(pkt, WLF2_PCB3_NAR);

	NAR_COUNTER_INC(cubby, packets_in_transit);

#ifdef WLATF
	wlc_nar_add_packet_airtime(cubby, prec, pkt);
#endif /* WLATF */

	NAR_STATS_PEAK(cubby, PEAK_TRANSIT, cubby->packets_in_transit);

	SCB_TX_NEXT(TXMOD_NAR, scb, pkt, prec);	/* pass to next module */

	NAR_STATS_INC(cubby, PACKETS_PASSED_ON);
}

/*
 * Release one (or more) packets from the queue and pass it on.
 *
 * Returns the number of packets dequeued.
 *
 */
static int      BCMFASTPATH
wlc_nar_release_from_queue(nar_scb_cubby_t * cubby, int prec)
{
	int             ntorelease = cubby->nit->release_at_once;
	int             nreleased = 0;
#if defined(BCMPCIEDEV)
	wlc_info_t *wlc = NULL;
	struct scb *scb = NULL;
	ratespec_t cur_rspec = 0;
	uint8 fl = 0;

	scb = cubby->scb_bl;
	ASSERT(scb->bsscfg);
	wlc = scb->bsscfg->wlc;
	if (BCMPCIEDEV_ENAB(wlc->pub)) {
		cur_rspec = wlc_ravg_get_scb_cur_rspec(wlc, scb);
		fl = RAVG_PRIO2FLR(PRIOMAP(wlc), WLC_PREC_TO_PRIO(prec));
	}
#endif /* BCMPCIEDEV */

	WL_NAR_MSG(("%30s: Prec %d can release %d, %d(%d) in queue, fair share %sreached.\n",
		__FUNCTION__, prec, ntorelease, cubby->packets_in_queue,
		pktq_len(&cubby->tx_queue), wlc_nar_fair_share_reached(cubby) ? "" : "NOT "));
	/*
	 * See if this SCB has packets queued, if so, reinject the next few in line.
	 */
	while (ntorelease-- &&	       /* we can release some more */
		cubby->packets_in_queue &&	/* we have some to release */
		!wlc_nar_fair_share_reached(cubby)) {	/* and have not reached fair share */

		void *p;

		p = wlc_nar_dequeue_packet(cubby, prec);
		if (p) {
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
			WLPKTTAGSCBSET(p, scb);
#endif // endif
			++nreleased;
#if defined(BCMPCIEDEV)
			if (BCMPCIEDEV_ENAB(wlc->pub)) {
				uint16 frmbytes = pkttotlen(wlc->osh, p) +
					wlc_scb_dot11hdrsize(scb);
				/* adding pktlen into the moving average buffer */
				RAVG_ADD(TXPKTLEN_RAVG(scb, fl), frmbytes);
			}
#endif /* BCMPCIEDEV */
			wlc_nar_pass_packet_on(cubby, cubby->scb_bl, p, prec);
		}
	}

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB(wlc->pub)) {
		if (nreleased)
			/* adding weight into the moving average buffer */
			wlc_ravg_add_weight(wlc, scb, fl, cur_rspec);
	}
#endif /* BCMPCIEDEV */

	return nreleased;
}

#if defined(PKTQ_LOG)

/*
 * Helper function for dotxstatus to test whether one of several packets was acked.
 */
static bool     BCMFASTPATH
wlc_nar_txs_was_acked(tx_status_macinfo_t * status, uint16 idx)
{
	return (idx < 32) ? (status->ack_map1 & (1 << idx)) : (status->ack_map2 & (1 << (idx-32)));
}

/*
 * PKTQ_LOG helper function to determine the packet payload length.
 */
static INLINE int BCMFASTPATH
wlc_nar_payload_length(wlc_info_t * wlc, struct scb *scb, void *pkt)
{
	int             pktlen;
	int             d11len;

	d11len = D11_TXH_LEN_EX(wlc)
		+ DOT11_MAC_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN + (SCB_QOS(scb) ? DOT11_QOS_LEN : 0);

#if defined(WLTOEHW)
	if (WLCISACPHY(wlc->band) && wlc->toe_capable && !wlc->toe_bypass) {
		/*
		 * take the toe header into account
		 */
		d11len += wlc_tso_hdr_length((d11ac_tso_t *) PKTDATA(wlc->osh, pkt));
	}
#endif /* WLTOEHW */

	pktlen = pkttotlen(wlc->osh, pkt);

	return ((pktlen < d11len) ? 0 : pktlen - d11len);
}
#endif /* PKTQ_LOG */

/*
 * Callback for once a packet has been transmitted. Here we simply decrement the count of in
 * transit packets, and trigger transmission of more packets of the same precedence.
 *
 * XXX Problem: If the lower layers decide not to transmit the packet (ie, drop it), we do not
 * XXX get any indication of what happened. Hence the tx_stuck_time cleanup in the watchdog. Sigh.
 *
 */
void BCMFASTPATH
wlc_nar_dotxstatus(wlc_nar_info_t *nit, struct scb *scb, void *pkt, tx_status_t * txs,
	bool pps_retry)
{
	nar_scb_cubby_t *cubby;
	int             prec;
	uint32          pcount;		/* number of packets for which we get tx status */
	uint32          pcounted;

#if defined(PKTQ_LOG)
	pktq_counters_t *pq_counters = NULL;

#ifdef PSPRETEND
	uint supr_status = txs->status.suppr_ind;
#endif /* PSPRETEND */
#endif /* PKTQ_LOG */

	ASSERT(nit != NULL);
	ASSERT(txs != NULL);

	/*
	 * Return if we are not set up to handle the tx status for this scb.
	 */
	if (!scb ||		       /* no SCB */
	    !pkt ||		       /* no packet */
	    !nit->nar_enabled ) {      /* Feature is not enabled */

		return;
	}

	cubby = SCB_NAR_CUBBY(nit, scb);
	if (!cubby) {
	    return;
	}

	NAR_STATS_INC(cubby, TX_STATUS);

	if (WLPKTTAG(pkt)->flags & WLF_AMPDU_MPDU) {
		/*
		 * Ignore aggregating stations
		 */
		NAR_STATS_INC(cubby, AMPDU_TXS_IGNORED);
		return;
	}

	/*
	 * Figure out for which precedence this packet was
	 */

	prec = WLC_PRIO_TO_PREC(PKTPRIO(pkt));

	/*
	 * AQM can interrupt once for a number of packets sent - handle that.
	 */

	if (D11REV_LT(nit->wlc->pub->corerev, 40)) {
		pcount = 1;
	} else {
		pcount = ((txs->status.raw_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);

#if defined(NAR_STATS)
		cubby->stats.counter[NAR_COUNTER_NCONS]
			+= ((txs->status.raw_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);
#endif /* NAR_STATS */
	}

	WL_NAR_MSG(("%30s: Status %08x [%s %s] for %d %s packet(s), prec %d, %d in transit\n",
		__FUNCTION__, txs->status.raw_bits, txs->status.was_acked ? "acked" : "",
		txs->status.is_intermediate ? "intermediate" : "", pcount,
		(WLPKTTAG(pkt)->flags & WLF_AMPDU_MPDU) ? "AMPDU" : "MPDU", prec,
		cubby->packets_in_transit));

	for (pcounted = 0; pcounted < pcount; ++pcounted) {

#if defined(PKTQ_LOG)
		bool acked;
		int pktlen, tx_frame_count;

		/*
		 * Update pktq_stats counters
		 */

		pktlen = wlc_nar_payload_length(nit->wlc, scb, pkt);

		WL_NAR_MSG(("%30s: pkt %d/%d, len %d\n", __FUNCTION__, pcounted, pcount, pktlen));
		if (pcount == 1) {
			acked = txs->status.was_acked;
		} else {
			acked = wlc_nar_txs_was_acked(&(txs->status), (uint16)pcounted);
		}

		if (cubby->tx_queue.pktqlog) {
			pq_counters = cubby->tx_queue.pktqlog->_prec_cnt[prec];
		}

		tx_frame_count = txs->status.frag_tx_cnt;

		if (D11REV_GE(nit->wlc->pub->corerev, 40)) {
			WLCNTCONDADD(pq_counters, pq_counters->airtime,
			             TX_STATUS40_TX_MEDIUM_DELAY(txs));
		}

#ifdef PSPRETEND
		if (pps_retry) {

			if (!supr_status) {
				WLCNTCONDINCR(pq_counters, pq_counters->ps_retry);
			}
		} else if (acked) {
#else
		if (acked) {
#endif /* PSPRETEND */
			WLCNTCONDINCR(pq_counters, pq_counters->acked);
			WLCNTCONDADD(pq_counters, pq_counters->throughput, pktlen);
			if (tx_frame_count) {
				WLCNTCONDADD(pq_counters, pq_counters->retry, (tx_frame_count - 1));
			}
			NAR_STATS_INC(cubby, ACKED);

		} else {
			WLCNTCONDINCR(pq_counters, pq_counters->retry_drop);
			WLCNTCONDADD(pq_counters, pq_counters->retry, tx_frame_count);

			NAR_STATS_INC(cubby, NOT_ACKED);

		} /* not acked */
#endif /* PKTQ_LOG */
	} /* for all packets */

	if (wlc_nar_release_from_queue(cubby, prec)) {
		NAR_STATS_INC(cubby, KICKSTARTS_IN_INT);
	}

}

bool
nar_check_aggregation(struct scb *scb, void *pkt, uint prec)
{

	bool prio_is_aggregating;
	int apm = wlc_ampdu_ba_on_tidmask(scb);

	prio_is_aggregating = (SCB_AMPDU(scb) && (apm & (1<<PKTPRIO(pkt))));
	return prio_is_aggregating;

}
/*
 * Packet TXMOD handler.
 *
 * Here we simply pass on the packets unless we already have a certain amount in transit, in
 * which case we queue the packet for later transmission.
 *
 */

static void BCMFASTPATH
wlc_nar_transmit_packet(void *handle, struct scb *scb, void *pkt, uint prec)
{
	wlc_nar_info_t *nit = handle;
	nar_scb_cubby_t *cubby;
	bool prio_is_aggregating;

	ASSERT(scb != NULL);		/* Why would we get called with no SCB ? */

	if (!scb)
		return;

	if (!nit->nar_enabled) {
	    SCB_TX_NEXT(TXMOD_NAR, scb, pkt, prec);   /* Pass the packet on */
	    return;
	}

	cubby = SCB_NAR_CUBBY(nit, scb);

	if (!cubby) {
	    SCB_TX_NEXT(TXMOD_NAR, scb, pkt, prec);   /* Pass the packet on */
	    return;
	}

	NAR_STATS_INC(cubby, PACKETS_RECEIVED);

	/*
	 * If the queue is already full, try to dequeue some packets before queueing this one.
	 */
	if (wlc_nar_queue_is_full(cubby)) {

		if (wlc_nar_release_from_queue(cubby, prec)) {
			NAR_STATS_INC(cubby, KICKSTARTS_IN_TX_QF);
		}
	}

	prio_is_aggregating = nar_check_aggregation(scb, pkt, prec);

	/*
	 * Enqueue packet, then try to dequeue up to fair share.
	 */
	if (!prio_is_aggregating && !wlc_nar_enqueue_packet(cubby, pkt, prec)) {
		/*
		 * Failed to queue packet. Drop it on the floor.
		 */
		PKTFREE(nit->wlc->osh, pkt, TRUE);
		NAR_STATS_INC(cubby, PACKETS_DROPPED);
		WLCNTINCR(nit->wlc->pub->_cnt->txnobuf);
	}

	if (wlc_nar_release_from_queue(cubby, prec)) {
		NAR_STATS_INC(cubby, KICKSTARTS_IN_TX);
	}

	#if !defined(PKTC)
	if (SCB_AMPDU(scb)) {
		nit->ampdu_scb_present = TRUE;   /* Ask watchdog to keep an eye on this scb */

		if (prio_is_aggregating) {
			/* We have not queued this packet, pass it to the next layer (ampdu) */
			NAR_STATS_INC(cubby, PACKETS_AGGREGATING);
			SCB_TX_NEXT(TXMOD_NAR, scb, pkt, prec);   /* Pass the packet on */
		}
	}
	#endif
	return;

}

/*
 * txmod callback to return the count of queued packets.
 */
static uint BCMFASTPATH
wlc_nar_count_queued_packets(void *handle)
{
	wlc_nar_info_t *nit = handle;

	return nit->packets_in_queue;
}

void
wlc_nar_flush_flowid_pkts(wlc_nar_info_t * nit, struct scb *scb, uint16 flowid)
{
	int prec;
	void *p = NULL;
	struct pktq *q;
	nar_scb_cubby_t *cubby = SCB_NAR_CUBBY(nit, scb);

	if (cubby == NULL)
		return;

	q = &cubby->tx_queue;
	PKTQ_PREC_ITER(q, prec) {
		if (pktq_plen(q, prec) == 0)
			continue;

		p = pktq_ppeek(q, prec);
		if (p == NULL || flowid != PKTFRAGFLOWRINGID(cubby->osh, p))
			continue;

		p = pktq_pdeq(q, prec);
		while (p) {
			PKTFREE(cubby->osh, p, TRUE);
			NAR_STATS_INC(cubby, PACKETS_DEQUEUED);
			NAR_COUNTER_DEC(cubby, packets_in_queue);
			p = pktq_pdeq(q, prec);
		}
	}
}

#ifdef PROP_TXSTATUS
/** returns tx queue to use for a caller supplied scb (= one remote party) */
struct pktq *
wlc_nar_txq(wlc_nar_info_t * nit, struct scb *scb)
{
	nar_scb_cubby_t *cubby;

	if (!nit->nar_enabled) {
		return NULL;
	}

	cubby = SCB_NAR_CUBBY(nit, scb);
	return ((cubby) ? &cubby->tx_queue : NULL);

}
#endif /* PROP_TXSTATUS */
