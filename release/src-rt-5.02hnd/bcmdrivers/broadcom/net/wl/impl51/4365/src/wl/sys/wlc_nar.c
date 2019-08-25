/**
 * Non Aggregated Regulation
 *
 * @file
 * @brief
 * This module contains the NAR transmit module. Hooked in the transmit path at the same
 * level as the A-MPDU transmit module, it provides balance amongst MPDU and AMPDU traffic
 * by regulating the number of in-transit packets for non-aggregating stations.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_nar.c 663426 2016-10-05 09:36:54Z $
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

#if defined(SCB_BS_DATA)
#include <wlc_bs_data.h>
#endif /* SCB_BS_DATA */

/*
 * Initial parameter settings. Most can be tweaked by wl debug commands. Some we may want to
 * make tuneables, others bcminternal as they should not really be used in production.
 */
#define NAR_QUEUE_LENGTH         128	/* Configurable - size for station pkt queues */
#define NAR_MIN_QUEUE_LEN	  128	/* Fixed - minimum pkt queue length */

#ifndef NAR_MAX_TRANSIT_PACKETS
#define NAR_MAX_TRANSIT_PACKETS   64	/* Configurable - Limit of transit packets */
#endif

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
#endif
#ifdef WLATF
	uint		txq_time_allowance_us;	/* Time value of packets in transit */
#endif
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
#endif
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
#endif
} nar_scb_cubby_t;

#define SCB_NAR_CUBBY_PTR(nit, scb) ((nar_scb_cubby_t **)(SCB_CUBBY((scb), (nit)->scb_handle)))
#define SCB_NAR_CUBBY(nit, scb) (*SCB_NAR_CUBBY_PTR(nit, scb))

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

/*
 * Forward declarations.
 */
static int BCMFASTPATH wlc_nar_release_from_queue(nar_scb_cubby_t * cubby, int prec);
static bool BCMFASTPATH wlc_nar_fair_share_reached(nar_scb_cubby_t * cubby);
static void BCMFASTPATH wlc_nar_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs);
static void wlc_nar_scbcubby_exit(void *handle, struct scb *scb);

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

/*
 * Module iovar handling.
 */
enum {
	IOV_NAR,		       /* Global on/off switch */
	IOV_NAR_HANDLE_AMPDU,	       /* on/off switch for handling ampdu-capable STAs */
	IOV_NAR_TRANSIT_LIMIT,	       /* max packets in transit allowed */
#if defined(NAR_STATS)
	IOV_NAR_CLEAR_DUMP	       /* Clear statistics (aka dump, like for ampdu) */
#endif
};

static const bcm_iovar_t nar_iovars[] = {
	{"nar", IOV_NAR, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"nar_handle_ampdu", IOV_NAR_HANDLE_AMPDU, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"nar_transit_limit", IOV_NAR_TRANSIT_LIMIT, 0, IOVT_UINT32, 0},
#if defined(NAR_STATS)
	{"nar_clear_dump", IOV_NAR_CLEAR_DUMP, 0, IOVT_VOID, 0},
#endif
	{NULL, 0, 0, 0, 0}
};

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
static void
wlc_nar_watchdog(void *handle)
{
	wlc_nar_info_t *nit = handle;

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
#endif
}

/** free all pkts asscoated with the given scb on the pktq for given precedences */
void
wlc_nar_flush_scb_pqueues(wlc_info_t *wlc, uint prec_bmp, struct pktq *pq1, struct scb *scb)
{
	wlc_nar_info_t *nit = wlc->nar_handle;
	nar_scb_cubby_t *cubby;
	struct pktq *pq = NULL;
	uint prec;
	int prec_cnt = PKTQ_MAX_PREC-1;
	uint packet_count = 0;

	/* Get the packet queue for this scb */
	cubby = SCB_NAR_CUBBY(nit, scb);
	if (!cubby) {
		return;
	}
	pq = &cubby->tx_queue;
	packet_count = pq->len;

	/* Loop over all precedences set in the bitmap, and flush the target prec */
	while (prec_cnt >= 0) {
		prec = (prec_bmp & (1 << prec_cnt));
		if ((prec) && (!pktq_pempty(pq, prec_cnt))) {
			WL_PRINT(("wl%d: filter %d packets of prec=%d for scb:0x%p\n",
				wlc->pub->unit, pktq_plen(pq, prec_cnt), prec_cnt, scb));
			pktq_pflush(wlc->osh, pq, prec_cnt, TRUE, NULL, 0);
		}
		prec_cnt--;
	}

	packet_count -= pq->len;
	cubby->packets_in_queue -= packet_count;
	cubby->packets_in_transit -= packet_count;
	nit->packets_in_queue -= cubby->packets_in_queue;
	nit->packets_in_transit -= cubby->packets_in_transit;
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
#endif
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
			    _VPRINT(i, "txq_time_allowance_us(us)", cubby->txq_time_allowance_us);
#ifdef WLATF
			    _VPRINT(i, "released airtime (us)", cubby->packet_airtime_us);
			    _VPRINT(i, "last packet rspec", cubby->last_packet_rspec);
			    _VPRINT(i, "last packet nbytes", cubby->last_packet_nbytes);
			    _VPRINT(i, "last packet airtime(us)", cubby->last_packet_airtime_us);
#endif
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

	if (nit->nar_enabled &&	/* Only register txmod if enabled */
	    !SCB_INTERNAL(scb)) { /* Ignore internal SCBs */

		nar_scb_cubby_t *cubby = NULL;

		cubby = MALLOCZ(nit->wlc->osh, sizeof(nar_scb_cubby_t));
		if (cubby == NULL) {
		    return BCME_NOMEM;
		}
		/*
		 * Initialise the cubby.
		 */
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
#endif
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
	nit = MALLOCZ(wlc->pub->osh, sizeof(wlc_nar_info_t));
	if (!nit) {
		return NULL;
	}

	nit->wlc = wlc;		       /* save backlink to wlc */

	wlc_nar_module_init(nit);     /* set up module parameters */


	/* register packet class callback */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_NAR, wlc_nar_pkt_freed) != BCME_OK) {
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
			wlc_nar_scbcubby_init, wlc_nar_scbcubby_exit, NULL, nit)) < 0) {

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
	bool enabled  = cubby->nit->wlc->atf;

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
	bool enabled = cubby->nit->wlc->atf;
	uint airtime_us = 0;

	if (enabled) {
		airtime_us = wlc_nar_packet_airtime(cubby, prec, pkt);
		WLPKTTAG(pkt)->pktinfo.atf.pkt_time = (uint16)airtime_us;
		cubby->packet_airtime_us += airtime_us;
	}
}

static INLINE bool
wlc_nar_airtime_share_reached(nar_scb_cubby_t * cubby)
{
	bool enabled = cubby->nit->wlc->atf;
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
	return (
#ifdef WLATF
		wlc_nar_airtime_share_reached(cubby) ||
#endif
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
#ifdef WLATF
		wlc_nar_dec_packet_airtime(cubby, pkt);
#endif
	} else {
		/*
		 * Transmit complete, but no packets in transit.
		 */
		NAR_STATS_INC(cubby, PACKETS_TX_NO_TRANSIT);
		if (SCB_AMPDU(scb)) {
			nit->ampdu_scb_present = TRUE;
		}
	}
}

/*
 * Pass a packet to the next txmod below, incrementing the in transit count.
 */
static void     BCMFASTPATH
wlc_nar_pass_packet_on(nar_scb_cubby_t * cubby, struct scb *scb, void *pkt, uint prec)
{
	WL_NAR_MSG(("%30s: prec %d transit count %d\n", __FUNCTION__,
		prec, cubby->packets_in_transit));

	WLF2_PCB1_REG(pkt, WLF2_PCB1_NAR);

	NAR_COUNTER_INC(cubby, packets_in_transit);

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
	uint32 dot11hdrsize;
	wlc_ravg_info_t *ravg_info;
	uint16 *txpktlen_rbuf;

	scb = cubby->scb_bl;
	ASSERT(scb->bsscfg);
	wlc = scb->bsscfg->wlc;
	cur_rspec = wlc_ravg_get_scb_cur_rspec(wlc, scb);
	fl = RAVG_PRIO2FLR(PRIOMAP(wlc), WLC_PREC_TO_PRIO(prec));
	dot11hdrsize = wlc_scb_dot11hdrsize(scb);
	ravg_info = TXPKTLEN_RAVG(scb, fl);
	txpktlen_rbuf = TXPKTLEN_RBUF(scb, fl);
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

		void           *p;

		p = wlc_nar_dequeue_packet(cubby, prec);

		if (p) {
			++nreleased;
#ifdef WLATF
			wlc_nar_add_packet_airtime(cubby, prec, p);
#endif
#if defined(BCMPCIEDEV)
			if (BCMPCIEDEV_ENAB()) {
				uint16 frmbytes = pkttotlen(wlc->osh, p) + dot11hdrsize;
				/* adding pktlen into the moving average buffer */
				RAVG_ADD(ravg_info, txpktlen_rbuf, frmbytes, RAVG_EXP_PKT);
			}
#endif /* BCMPCIEDEV */
			wlc_nar_pass_packet_on(cubby, cubby->scb_bl, p, prec);
		}
	}

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
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

void BCMFASTPATH
wlc_nar_dotxstatus(wlc_nar_info_t *nit, struct scb *scb, void *pkt, tx_status_t * txs)
{
	nar_scb_cubby_t *cubby;
	int             prec;
	uint32          pcount;		/* number of packets for which we get tx status */
	uint32          pcounted;

#if defined(PKTQ_LOG)
	pktq_counters_t *pq_counters = NULL;
#endif /* PKTQ_LOG */
#if defined(SCB_BS_DATA)
	wlc_bs_data_counters_t *bs_data_counters = NULL;
#endif

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

	SCB_BS_DATA_CONDFIND(bs_data_counters, nit->wlc, scb);

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
#ifdef BCMPCIEDEV
		uint8 ac;
#endif
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

		if (acked) {
			WLCNTCONDINCR(pq_counters, pq_counters->acked);
			SCB_BS_DATA_CONDINCR(bs_data_counters, acked);
			WLCNTCONDADD(pq_counters, pq_counters->throughput, pktlen);
			SCB_BS_DATA_CONDADD(bs_data_counters, throughput, pktlen);
			if (tx_frame_count) {
				WLCNTCONDADD(pq_counters, pq_counters->retry, (tx_frame_count - 1));
				SCB_BS_DATA_CONDADD(bs_data_counters, retry, (tx_frame_count - 1));
			}
			NAR_STATS_INC(cubby, ACKED);
#ifdef BCMPCIEDEV
			ac = WME_PRIO2AC(PKTPRIO(pkt));
			WLCNTSCBINCR(scb->flr_txpkt[ac]);
#endif
		} else {
			WLCNTCONDINCR(pq_counters, pq_counters->retry_drop);
			SCB_BS_DATA_CONDINCR(bs_data_counters, retry_drop);
			WLCNTCONDADD(pq_counters, pq_counters->retry, tx_frame_count);
			SCB_BS_DATA_CONDADD(bs_data_counters, retry, tx_frame_count);

			NAR_STATS_INC(cubby, NOT_ACKED);

		} /* not acked */
#endif /* PKTQ_LOG */
	} /* for all packets */

	if (wlc_nar_release_from_queue(cubby, prec)) {
		NAR_STATS_INC(cubby, KICKSTARTS_IN_INT);
	}

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
#if defined(PKTC) || defined(PKTC_TX_DONGLE)
	void *pkt1 = NULL;
	uint32 lifetime = 0;
#endif /* #if defined(PKTC) */


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

	prio_is_aggregating = (SCB_AMPDU(scb) && (cubby->aggr_prio_mask & (1<<PKTPRIO(pkt))));

	/* If ampdu is enabled, pass on the packets */
	if (SCB_AMPDU(scb)) {
		nit->ampdu_scb_present = TRUE;   /* Ask watchdog to keep an eye on this scb */

		if (prio_is_aggregating) {
			/* We have not queued this packet, pass it to the next layer (ampdu) */
			NAR_STATS_INC(cubby, PACKETS_AGGREGATING);
			SCB_TX_NEXT(TXMOD_NAR, scb, pkt, prec);   /* Pass the packet on */
			return;
		}
	}
#if defined(PKTC) || defined(PKTC_TX_DONGLE)
	lifetime = nit->wlc->lifetime[(SCB_WME(scb) ? WME_PRIO2AC(PKTPRIO(pkt)) : AC_BE)];

	FOREACH_CHAINED_PKT(pkt, pkt1) {
		PKTCLRCHAINED(nit->wlc->osh, pkt);
		if (pkt1 != NULL) {
			wlc_pktc_sdu_prep(nit->wlc, scb, pkt, pkt1, lifetime);
		}
#endif /* PKTC || PKTC_TX_DONGLE  */

	/*
	 * If the queue is already full, try to dequeue some packets before queueing this one.
	 */
	if (wlc_nar_queue_is_full(cubby)) {

		if (wlc_nar_release_from_queue(cubby, prec)) {
			NAR_STATS_INC(cubby, KICKSTARTS_IN_TX_QF);
		}
	}

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

#if defined(PKTC) || defined(PKTC_TX_DONGLE)
	}
#endif /*  PKTC || PKTC_TX_DONGLE */

	if (wlc_nar_release_from_queue(cubby, prec)) {
		NAR_STATS_INC(cubby, KICKSTARTS_IN_TX);
	}

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

#ifdef PROP_TXSTATUS
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
#endif /* PROP_TXSTATUS */
