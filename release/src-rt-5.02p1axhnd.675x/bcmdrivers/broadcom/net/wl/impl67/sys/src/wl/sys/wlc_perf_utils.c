/**
 * @file
 * @brief
 * Performance measurement/analysis facilities
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
 * $Id: wlc_perf_utils.c 790951 2020-09-09 19:16:09Z $
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
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_tx.h>
#ifdef WLNAR
#include <wlc_nar.h>
#endif // endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#endif // endif
#include <wl_export.h>
#include <wlc_lq.h>
#include <wlc_qoscfg.h>
#include <wlc_event_utils.h>
#include <wlc_perf_utils.h>
#include <wlc_dump.h>
#include <hnd_pktq.h>

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
#include <hnddma.h>
#include <wlc_hw.h>
#endif /* BCMPCIEDEV && BUS_TPUT */

#include <wlc_bmac.h>

#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

/* IOVar table */
enum {
	IOV_PKTQ_STATS = 0,
	IOV_TXDELAY_PARAMS = 1,
	IOV_DLYSTATS = 2,
	IOV_DLYSTATS_CLEAR = 3,
	IOV_BMAC_PCIE_BUS_TPUT,
	IOV_DP_STATS,
	IOV_PKTQ_STATUS,
	IOV_LAST
};

static const bcm_iovar_t perf_utils_iovars[] = {
#ifdef PKTQ_LOG
	{"pktq_stats", IOV_PKTQ_STATS, (IOVF_GET_UP), 0, IOVT_BUFFER,
	MAX(sizeof(wl_iov_mac_params_t), sizeof(wl_iov_pktq_log_t))},
#endif // endif
#ifdef WLPKTDLYSTAT
#ifdef WLPKTDLYSTAT_IND
	{"txdelay_params", IOV_TXDELAY_PARAMS, 0, 0, IOVT_BUFFER, sizeof(txdelay_params_t)},
#endif /* WLPKTDLYSTAT_IND */
	{"dlystats", IOV_DLYSTATS, 0, 0, IOVT_BUFFER, WL_TXDELAY_STATS_FIXED_SIZE},
	{"dlystats_clear", IOV_DLYSTATS_CLEAR, 0, 0, IOVT_VOID, 0},
#endif /* WLPKTDLYSTAT */
#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
	{"pcie_bus_tput", IOV_BMAC_PCIE_BUS_TPUT,
	(0), 0, IOVT_BUFFER, sizeof(pcie_bus_tput_params_t)
	},
#endif /* BCMPCIEDEV && BUS_TPUT */
	{"dpstats", IOV_DP_STATS, (IOVF_GET_UP), 0, IOVT_BUFFER,
	MAX(sizeof(wl_iov_mac_params_t), sizeof(wl_iov_pktq_log_t))},
#ifdef PKTQ_STATUS
	{"pktq_status", IOV_PKTQ_STATUS, 0, 0, IOVT_BUFFER, WLC_IOCTL_MEDLEN },
#endif /* PKTQ_STATUS */
	{NULL, 0, 0, 0, 0, 0}
};

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
static void wlc_bmac_hostbus_tpt_prog_d11dma_timer(void *arg);
static void wlc_bmac_hostbus_tpt_prog_d11dma_timer_stop(void *arg);
#endif /* BCMPCIEDEV && BUS_TPUT */

/* local prototypes */
#ifdef WLPKTDLYSTAT
static uint wlc_dlystats_scb_secsz(void *ctx, struct scb *scb);
static int wlc_dlystats_scb_init(void *ctx, struct scb *scb);
static void wlc_dlystats_scb_deinit(void *ctx, struct scb *scb);
static int wlc_dlystats(wlc_info_t *wlc, txdelay_stats_t *dlystats,
	struct ether_addr *scb_addr);
#ifdef WLPKTDLYSTAT_IND
static void wlc_delay_stats_timer(void *arg);
#endif // endif
#endif /* WLPKTDLYSTAT */
#ifdef PKTQ_LOG
static int wlc_pktq_stats(wl_iov_mac_full_params_t* full_params, uint8 params_version,
	wl_iov_pktq_log_t* iov_pktq, wlc_info_t* wlc, wlc_bsscfg_t *cfg);
#endif /* PKTQ_LOG */
#ifdef PKTQ_STATUS
static int wlc_pktq_curstatus(struct pktq *pktq, int prec, struct bcmstrbuf *b);
#endif /* PKTQ_STATUS */

static int wlc_perf_utils_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static int wlc_perf_utils_wlc_up(void *ctx);
static int wlc_perf_utils_wlc_down(void *ctx);

typedef struct wlc_perf_stats wlc_perf_stats_t;

struct wlc_perf_utils {
	wlc_info_t *wlc;
#if defined(WLPKTDLYSTAT)
#if defined(WLPKTDLYSTAT_IND)
	txdelay_params_t *txdelay_params;
	struct wl_timer *pktdlystat_timer;	/* timer to generate event based on delay */
#endif // endif
#endif /* WLPKTDLYSTAT */

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
	bmac_hostbus_tput_info_t *bmac_bus_tput_info; /* bus thoughput measurement info */
#endif /* BCMPCIEDEV && BUS_TPUT */

#ifdef BCMDBG
	wlc_perf_stats_t *perf_stats;
#endif /* BCMDBG */
};

#ifdef WLPKTDLYSTAT
/* scb cubby */
typedef struct {
	scb_delay_stats_t	delay_stats[AC_COUNT];	/**< per-AC delay stats */
#ifdef WLPKTDLYSTAT_IND
	txdelay_params_t	txdelay_params;
#endif /* WLPKTDLYSTAT_IND */
} scb_cubby_t;

#ifdef WLPKTDLYSTAT_IND
#define TXDELAY_SAMPLE_PERIOD	1	/* Txdelay sample period, in sec */
#define TXDELAY_SAMPLE_CNT	4	/* Txdelay sample count number */
#define TXDELAY_RATIO		30	/* Avg Txdelay ratio */
#define TXDELAY_TIMER_PERIOD	1000	/* Txdelay timer period, in msec */
#define TXDELAY_MIN_SAMPLE_DATA	8	/* minimum number of time period units before
					consequent packet delay events can be generated
					*/
#endif /* WLPKTDLYSTAT_IND */
#endif /* WLPKTDLYSTAT */

#ifdef BCMDBG
/* Performance statistics interfaces */
typedef struct {
	uint32 n_counts[32];
#ifdef WLP2P
	uint32 n_p2p[M_P2P_I_BLK_SZ];
#endif // endif
} wlc_isr_stats_t;

struct wlc_perf_stats {
	uint32	n_isr;			/**< no of isrs */
	uint32	n_dpc;			/**< no of dpcs */
	uint32	n_timer_dpc;		/**< no of software timers */
	uint32	n_bcn_isr;		/**< no of ints by beacons (may be coalesced) */
	uint32	n_beacons;		/**< no of beacons received */
	uint32	n_probe_req;		/**< no of probe requests received */
	uint32	n_probe_resp;		/**< no of probe responses received */
	wlc_isr_stats_t isr_stats;
};
#endif /* BCMDBG */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#ifdef BCMDBG
static int
wlc_dump_perf_stats_clr(void *ctx)
{
	wlc_perf_utils_t *pui = (wlc_perf_utils_t *)ctx;
	bzero(pui->perf_stats, sizeof(*(pui->perf_stats)));
	return BCME_OK;
}

static int
wlc_dump_perf_stats(void *ctx, struct bcmstrbuf *b)
{
	wlc_perf_utils_t *pui = (wlc_perf_utils_t *)ctx;
	wlc_perf_stats_t *cnt = pui->perf_stats;
	wlc_isr_stats_t *ints = &cnt->isr_stats;
	uint32 i;

	const char * int_names[32] =
	{
		"MACSSPNDD     ",
		"BCNTPL        ",
		"TBTT          ",
		"BCNSUCCESS    ",
		"BCNCANCLD     ",
		"ATIMWINEND    ",
		"PMQ           ",
		"NSPECGEN_0    ",
		"NSPECGEN_1    ",
		"MACTXERR      ",
		"NSPECGEN_3    ",
		"PHYTXERR      ",
		"PME           ",
		"GP0           ",
		"GP1           ",
		"DMAINT        ",
		"TXSTOP        ",
		"CCA           ",
		"BG_NOISE      ",
		"DTIM_TBTT     ",
		"PRQ           ",
		"PWRUP         ",
		"BT_RFACT_STUCK",
		"BT_PRED_REQ   ",
		"INT_24        ",
		"P2P           ",
		"INT_26        ",
		"INT_27        ",
		"RFDISABLE     ",
		"TFS           ",
		"PHYCHANGED    ",
		"TO            "
	};

#ifdef WLP2P
	const char * p2p_int_names[32] =
	{
		"PRE_TBTT",
		"CTW_END ",
		"ABS     ",
		"PRS     "
	};
#endif // endif
	/* Print perf stats */

	bcm_bprintf(b, "\nGeneral Performance Stats:-\n");

	bcm_bprintf(b,
		"\nisr        : %d"
		"\ndpc        : %d"
		"\ntimer dpc  : %d"
		"\nbcn isr    : %d"
		"\nbeacons    : %d"
		"\nprobe req  : %d"
		"\nprobe resp : %d\n",
			cnt->n_isr, cnt->n_dpc, cnt->n_timer_dpc,
			cnt->n_bcn_isr, cnt->n_beacons, cnt->n_probe_req, cnt->n_probe_resp);

	bcm_bprintf(b, "\nInterrupt       num  percent");

	for (i = 0; i < 32; i++) {
		if (ints->n_counts[i]) {
			bcm_bprintf(b, "\n%s	%d	%d", int_names[i], ints->n_counts[i],
				((ints->n_counts[i])*100)/(cnt->n_isr));
		}
	}
	bcm_bprintf(b, "\n");

#ifdef WLP2P
	if (P2P_ENAB(pui->wlc->pub) && ints->n_counts[25]) {
		bcm_bprintf(b, "\nP2P Interrupt   num  percent");

		for (i = 0; i < M_P2P_I_BLK_SZ; i++) {
			bcm_bprintf(b, "\n%s	%d	%d", p2p_int_names[i], ints->n_p2p[i],
				((ints->n_p2p[i])*100)/(ints->n_counts[25]));
		}
		bcm_bprintf(b, "\n");
	}
#endif // endif

	return BCME_OK;
}
#endif /* BCMDBG */

/* attach/detach */
wlc_perf_utils_t *
BCMATTACHFN(wlc_perf_utils_attach)(wlc_info_t *wlc)
{
	wlc_perf_utils_t *pui;

	if ((pui = MALLOCZ(wlc->osh, sizeof(*pui))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed. allocated %d bytes.\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	pui->wlc = wlc;

#if defined(BCMDBG)
	if ((pui->perf_stats = MALLOCZ(wlc->osh, sizeof(*(pui->perf_stats)))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed. allocated %d bytes.\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	wlc_dump_add_fns(wlc->pub, "perf_stats", wlc_dump_perf_stats, wlc_dump_perf_stats_clr, pui);
#endif /* BCMDBG */

	if (wlc_module_register(wlc->pub, perf_utils_iovars, "perf_utils", pui,
			wlc_perf_utils_doiovar,
			NULL, wlc_perf_utils_wlc_up, wlc_perf_utils_wlc_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#if defined(WLPKTDLYSTAT)
	{
		scb_cubby_params_t cubby_params;
		/* utilize scb cubby facility for stats storage allocation/free */
		bzero(&cubby_params, sizeof(cubby_params));

		cubby_params.context = wlc;
		cubby_params.fn_init = wlc_dlystats_scb_init;
		cubby_params.fn_deinit = wlc_dlystats_scb_deinit;
		cubby_params.fn_secsz = wlc_dlystats_scb_secsz;
		if (wlc_scb_cubby_reserve_ext(wlc, 0, &cubby_params) < 0) {
			WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed.\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}

#if defined(WLPKTDLYSTAT_IND)
	if ((pui->txdelay_params =
	     MALLOCZ(wlc->osh, sizeof(*(pui->txdelay_params)))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed. allocated %d bytes.\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	pui->txdelay_params->period = TXDELAY_SAMPLE_PERIOD;
	pui->txdelay_params->cnt = TXDELAY_SAMPLE_CNT;
	pui->txdelay_params->ratio = TXDELAY_RATIO;

	if (pui->txdelay_params->period > 0) {
		/* Init the timer */
		pui->pktdlystat_timer =
		        wl_init_timer(wlc->wl, wlc_delay_stats_timer, pui,
		                      "pkt_delay_stats_timer");
		if (pui->pktdlystat_timer == NULL) {
			WL_ERROR(("wl%d: %s: wl_init_timer failed.\n",
			          wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* defined(WLPKTDLYSTAT_IND) */
#endif /* defined(WLPKTDLYSTAT) */

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
	if (BCMPCIEDEV_ENAB()) {
		pui->bmac_bus_tput_info = MALLOCZ(wlc->osh, sizeof(bmac_hostbus_tput_info_t));
		if (pui->bmac_bus_tput_info == NULL) {
			WL_ERROR(("wl%d: %s: bmac_bus_tput_info malloc failed\n",
					wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}

	if (BCMPCIEDEV_ENAB() && pui->bmac_bus_tput_info != NULL) {
		if (!(pui->bmac_bus_tput_info->dma_timer = wl_init_timer(pui->wlc->wl,
				wlc_bmac_hostbus_tpt_prog_d11dma_timer, pui, NULL))) {
			ASSERT(pui->bmac_bus_tput_info->dma_timer);
			goto fail;
		}

		if (!(pui->bmac_bus_tput_info->end_timer = wl_init_timer(pui->wlc->wl,
				wlc_bmac_hostbus_tpt_prog_d11dma_timer_stop, pui, NULL))) {
			ASSERT(pui->bmac_bus_tput_info->end_timer);
			goto fail;
		}
	}
#endif /* BCMPCIEDEV && BUS_TPUT */

	return pui;

fail:
	MODULE_DETACH(pui, wlc_perf_utils_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_perf_utils_detach)(wlc_perf_utils_t *pui)
{
	wlc_info_t *wlc;

	if (pui == NULL)
		return;

	wlc = pui->wlc;

#if defined(WLPKTDLYSTAT)
#if defined(WLPKTDLYSTAT_IND)
	/* Cancel the delay stats timer */
	if (pui->pktdlystat_timer != NULL) {
		wl_free_timer(wlc->wl, pui->pktdlystat_timer);
	}
	if (pui->txdelay_params != NULL) {
		MFREE(wlc->osh, pui->txdelay_params, sizeof(*(pui->txdelay_params)));
	}
#endif /* WLPKTDLYSTAT_IND */
#endif /* WLPKTDLYSTAT */
	(void)wlc_module_unregister(wlc->pub, "perf_utils", pui);

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
	if (BCMPCIEDEV_ENAB() && pui->bmac_bus_tput_info != NULL) {
		MFREE(wlc->osh, pui->bmac_bus_tput_info, sizeof(*pui->bmac_bus_tput_info));
		pui->bmac_bus_tput_info = NULL;
	}
#endif /* BCMPCIEDEV && BUS_TPUT */

#if defined(BCMDBG)
	if (pui->perf_stats != NULL) {
		MFREE(wlc->osh, pui->perf_stats, sizeof(*(pui->perf_stats)));
	}
#endif // endif

	MFREE(wlc->osh, pui, sizeof(*pui));
}

#ifdef PKTQ_LOG
/**
 * De-allocate pktq stats log data
 *
 * @param q   Multi-priority packet queue
 */
void
wlc_pktq_stats_free(wlc_info_t* wlc, struct pktq* q)
{
	if (q->pktqlog) {
		uint32 prec_mask = q->pktqlog->_prec_log;
		uint32 index;

		for (index = 0; index < PKTQ_MAX_PREC; index++) {

			pktq_counters_t* counter = q->pktqlog->_prec_cnt[index];

			if (prec_mask & (1 << index)) {
				ASSERT(counter != 0);
				MFREE(wlc->osh, counter, sizeof(*counter));
			} else {
				ASSERT(counter == 0);
			}
		}
		MFREE(wlc->osh, q->pktqlog, sizeof(*q->pktqlog));
		q->pktqlog = 0;
	}
}

/**
 * enable PKTQ_LOG for a precedence
 *
 * @param q   Multi-priority packet queue
 */
pktq_counters_t*
wlc_txq_prec_log_enable(wlc_info_t* wlc, struct pktq* q, uint32 i, bool en_pair)
{
	pktq_counters_t* new = MALLOC(wlc->osh, sizeof(*new));
	/* check not already allocated */
	ASSERT(q->pktqlog);
	ASSERT(q->pktqlog->_prec_cnt[i] == NULL);
	ASSERT(i < q->num_prec);

	if (new) {
		bzero(new, sizeof(*new));

		new->max_used = pktqprec_n_pkts(q, i);
#ifdef WLSQS
		new->sqsv_max_used = 0;
#endif // endif

		q->pktqlog->_prec_cnt[i] = new;
		q->pktqlog->_prec_log |= (1 << i);
	}

	/* if this is HI-PREC, ensure standard prec is enabled */
	if (new && en_pair && (q->num_prec > NUMPRIO) && (i & 1)) {
		i &= ~1;
		if (q->pktqlog->_prec_cnt[i] == NULL) {
			wlc_txq_prec_log_enable(wlc, q, i, FALSE);
		}
	}
	return new;
}

/**
 * format data according to v06 of the interface
 *
 * @param q   Multi-priority packet queue
 */
static void
wlc_txq_prec_dump(wlc_info_t* wlc, struct pktq* q, wl_iov_pktq_log_t* pktq_log, uint8 index,
	bool clear, uint32 timelo, uint32 timehi, uint32 prec_mask, uint32 pps_time, uint32 flags,
        struct scb* scb)
{
	uint32 i;
	uint32 result_mask = flags;
	uint32 wme_ac = 0;

	ASSERT((flags & PKTQ_LOG_PREC_MASK) == 0);

	if (pktq_log->version != 0x06) {
		WL_ERROR(("%s unsupported version %u\n", __FUNCTION__, pktq_log->version));
		return;
	}

	pktq_log->pktq_log.v06.num_prec[index] = 0;

	if (prec_mask & PKTQ_LOG_AUTO) {
		result_mask |= PKTQ_LOG_AUTO;
	}

	if (!q) {
		/* Handle non-existent queues */
		pktq_log->pktq_log.v06.counter_info[index] = result_mask;
	    return;
	}

	if (!q->pktqlog) {
		/* Dynamically allocate memory for pktq stats logging */
		q->pktqlog = MALLOCZ(wlc->osh, sizeof(*q->pktqlog));

		if (!q->pktqlog) {
			return;
		}
	}

	pktq_log->pktq_log.v06.num_prec[index] = (uint8)q->num_prec;

	/* this is the 'auto' prec setting */
	if (prec_mask & PKTQ_LOG_AUTO) {
		q->pktqlog->_prec_log |= PKTQ_LOG_AUTO;

		if (prec_mask & PKTQ_LOG_DEF_PREC) {
			/* if was default (all prec) and AUTO mode is set,
			 * we can remove the "all prec"
			 */
			prec_mask &= ~0xFFFF;
		} else if (prec_mask & 0xFFFF) {
			/* this was not default, there is actual specified
			 * prec_mask, so use it rather than automatic mode
			 */
			prec_mask &= ~PKTQ_LOG_AUTO;
		}
	}

	for (i = 0; i < q->num_prec; i++) {
		if (!(flags & PKTQ_LOG_PS_QUEUE)) {
			uint8 tid = (q->num_prec <= NUMPRIO) ? i : wlc_prec2prio_map[i >> 1];
			uint8 ac = WME_PRIO2AC(tid);

			wme_ac |= (ac << (i * 2));
		}

		/* check pktq stats data was requested and also available */
		if ((((prec_mask & (1 << i)) || (prec_mask & PKTQ_LOG_AUTO)) &&
		                             (q->pktqlog->_prec_log & (1 << i)))) {
			pktq_log_counters_v06_t* counters =
			                    &pktq_log->pktq_log.v06.counters[index].pktq[i];
			pktq_counters_t*         pktq_count = q->pktqlog->_prec_cnt[i];
			uint64 time_delta = 0;
			int mcs_index;
			uint32 total_mcs = 0;
			int8 mcs_l = 127;
			int8 mcs_h = -1;
			uint16 mcs_bf = 0;

			if (pktq_count->_logtimelo != 0 || pktq_count->_logtimehi != 0) {
				time_delta = (uint64)timehi - (uint64)pktq_count->_logtimehi;
				time_delta <<= 32;
				time_delta += (uint64)timelo - (uint64)pktq_count->_logtimelo;
			}

			ASSERT(pktq_count);

			result_mask |= (1 << i);

			for (mcs_index = 0; mcs_index < 16; mcs_index++) {
				if (pktq_count->mcs[mcs_index] > 0) {
					total_mcs += pktq_count->mcs[mcs_index];
					if (mcs_index < mcs_l) {
						mcs_l = mcs_index;
					}
					if (mcs_index > mcs_h) {
						mcs_h = mcs_index;
					}
				}
			}
			if (total_mcs > 0) {
				uint32 cumul_total_mcs = 0;
				uint8 mcs_m = 0;
				total_mcs = (total_mcs + 1) / 2;

				for (mcs_index = 15; mcs_index > 0; mcs_index--) {
					cumul_total_mcs += pktq_count->mcs[mcs_index];
					if (cumul_total_mcs >= total_mcs) {
						mcs_m = mcs_index;
						break;
					}
				}
				/* set bit 15 to indicate valid data */
				mcs_bf = (mcs_l << 0) | (mcs_m << 5) | (mcs_h << 10) | (1 << 15);
			}

			if (flags & PKTQ_LOG_SCB_QUEUE) {
				counters->bandwidth      = pktq_count->txbw_succ;
				counters->txrate_main    = pktq_count->txrate_main;
				counters->suppress       = pktq_count->suppress;
#ifdef WLSQS
				counters->sqsv_requested = pktq_count->sqsv_requested;
				counters->sqsv_max_used  = pktq_count->sqsv_max_used;
#endif // endif
			} else {
				counters->saved          = pktq_count->saved;
				counters->selfsaved      = pktq_count->selfsaved;
				counters->full_dropped   = pktq_count->full_dropped;
				counters->sacrificed     = pktq_count->sacrificed;
			}
			counters->throughput     = pktq_count->throughput;
			counters->requested      = pktq_count->requested;
			counters->stored         = pktq_count->stored;
			counters->dropped        = pktq_count->dropped;
			counters->busy           = pktq_count->busy;
			counters->retry          = pktq_count->retry;
			counters->ps_retry       = pktq_count->ps_retry;
			counters->retry_drop     = pktq_count->retry_drop;
			counters->max_used       = pktq_count->max_used;
			counters->rtsfail        = pktq_count->rtsfail;
			counters->acked          = pktq_count->acked;
			counters->queue_capacity = pktqprec_max_pkts(q, i);
			counters->txrate_succ    = pktq_count->txrate_succ;
			counters->airtime        = pktq_count->airtime;
			counters->nss[0]         = pktq_count->nss[0];
			counters->nss[1]         = pktq_count->nss[1];
			counters->nss[2]         = pktq_count->nss[2];
			counters->nss[3]         = pktq_count->nss[3];
			counters->mcs_bitfield   = mcs_bf;
			counters->sgi_count      = pktq_count->sgi;
			counters->time_delta     = time_delta;

			if (clear) {
				bzero(pktq_count, sizeof(*pktq_count));
				pktq_count->max_used = pktqprec_n_pkts(q, i);
#ifdef WLSQS
				if (scb) {
					uint8 tid = (q->num_prec <= NUMPRIO) ?
						i : wlc_prec2prio_map[i >> 1];

					pktq_count->sqsv_max_used =
						wlc_sqs_vpkts(SCB_FLOWID(scb), tid);
				} else {
					pktq_count->sqsv_max_used = 0;
				}
#endif // endif
				pktq_count->_logtimelo = timelo;
				pktq_count->_logtimehi = timehi;
			}
		}
		/* else check pktq stats data was requested but not yet allocated */
		else if (((prec_mask & (1 << i)) && !(q->pktqlog->_prec_log & (1 << i))) ||
			((prec_mask & PKTQ_LOG_AUTO) && (!pktqprec_empty(q, i)))) {
			pktq_counters_t* prec_cnt =
				wlc_txq_prec_log_enable(wlc, q, i, (q->num_prec > NUMPRIO));
			if (prec_cnt) {
				prec_cnt->_logtimelo = timelo;
				prec_cnt->_logtimehi = timehi;
			}
		}
	}
	pktq_log->pktq_log.v06.counter_info[index] = result_mask;
#if defined(BCMDBG) && defined(PSPRETEND)
	if ((flags & PKTQ_LOG_PS_QUEUE) && PSPRETEND_ENAB(wlc->pub) && pps_time != (uint32)-1) {
		pktq_log->pktq_log.v06.pspretend_time_delta[index] =
			pps_time - q->pktqlog->pps_time;
		if (clear) {
			q->pktqlog->pps_time = pps_time;
		}
	} else
#endif /* PSPRETEND */
	if (flags & PKTQ_LOG_PS_QUEUE) {
		pktq_log->pktq_log.v06.pspretend_time_delta[index] = (uint32)-1;
	} else {
		pktq_log->pktq_log.v06.wme_ac[index] = wme_ac;
	}
} /* wlc_txq_prec_dump */

/** wlc_pktq_stats - return data according to revision v06 of the API */
static int
wlc_pktq_stats(wl_iov_mac_full_params_t* full_params, uint8 params_version,
               wl_iov_pktq_log_t* iov_pktq, wlc_info_t* wlc, wlc_bsscfg_t *cfg)
{
	wl_iov_mac_params_t* params	= &full_params->params;
	wl_iov_mac_extra_params_t* extra_params = &full_params->extra_params;
	struct bcmstrbuf b_local;
	struct bcmstrbuf* b = &b_local;

	uint32 addrs_todo = (params->num_addrs <= WL_IOV_MAC_PARAM_LEN ?
	                     params->num_addrs : WL_IOV_MAC_PARAM_LEN);
	struct ether_addr* ea = params->ea;

	wlc_txq_info_t* qi = wlc->active_queue;

	uint32 tsf_timelo;
	uint32 tsf_timehi;

#ifdef WLTAF
	const char* taf_status = wlc_taf_in_use(wlc->taf_handle) ? "" : " (taf 0)";
#else
	const char* taf_status = "";
#endif // endif

	if (wl_msg_level2 & WL_TIMESTAMP_VAL) {
		iov_pktq->pktq_log.v06.timestamp = wlc_bmac_read_usec_timer(wlc->hw);
	}

	tsf_timelo = (R_REG(wlc->osh, D11_TSFTimerLow(wlc)));
	tsf_timehi = (R_REG(wlc->osh, D11_TSFTimerHigh(wlc)));

	while (addrs_todo) {
		typedef struct pktq * pktq_get_fn_t(wlc_info_t*, struct scb*);
		pktq_get_fn_t* pktq_func = NULL;
		char* queue_type = "";
		uint8 index = (uint8)(ea - params->ea);
		bool resetstats = TRUE;
		uint32 prec_mask = 0;
		uint32 flags = 0;

		if (wl_msg_level2 & WL_TIMESTAMP_VAL) {
			flags |= PKTQ_LOG_DISPLAY_TIMESTAMP;
		}

#ifdef DONGLEBUILD
		flags |= PKTQ_LOG_DONGLE;
#endif // endif
#ifdef WLAMPDU
		flags |= AMPDU_AQM_ENAB(wlc->pub) ? PKTQ_LOG_AQM : 0;
#endif // endif
#ifdef BCMDBG
		flags |= PKTQ_LOG_DBG;
#endif // endif

		switch (params_version) {
			case 6:
				prec_mask = extra_params->addr_info[index];
				if (prec_mask & PKTQ_LOG_NO_RESET) {
					prec_mask &= ~PKTQ_LOG_NO_RESET;
					resetstats = FALSE;
				}
			case 4:
				prec_mask = extra_params->addr_info[index];
				break;
			default:
				return BCME_VERSION;
				break;
		}

		bcm_binit(b, (char*)(& iov_pktq->pktq_log.v06.counters[index].heading),
			PKTQ_LOG_V06_HEADINGS_SIZE);

		/* bit 7 (0x80) is used as a flag, so mask it off */
		switch (params->addr_type[index] & 0x7F) {
			case 'C':
			{
				bcm_bprintf(b, "common queue%s", taf_status);
				wlc_txq_prec_dump(wlc, WLC_GET_CQ(qi),
					iov_pktq, index, resetstats, tsf_timelo, tsf_timehi,
					prec_mask, (uint32)-1, flags, NULL);
				break;
			}
			default:
			{
				bcm_bprintf(b, "unknown addr prefix");
				iov_pktq->pktq_log.v06.num_prec[index] = 0;
				break;
			}
			/* destroy / delete pktq_stats logging for a station, release the memory */
			case 'D':
			{
				struct scb *scb = NULL;
				bool addr_OK = (!ETHER_ISMULTI(ea) &&
					(scb = wlc_scbfind(wlc, cfg, ea)));
				struct pktq* q;

				if ((q = WLC_GET_CQ(qi))) {
					wlc_pktq_stats_free(wlc, q);
				}

				if (addr_OK) {
#ifdef AP
					if ((q = wlc_apps_get_psq(wlc, scb))) {
						wlc_pktq_stats_free(wlc, q);
					}
#endif // endif
#ifdef WLAMPDU
					if ((q = scb_ampdu_prec_pktq(wlc, scb))) {
						wlc_pktq_stats_free(wlc, q);
					}
					wlc_ampdu_dpstats_free(wlc, scb);
#endif // endif
#ifdef WLNAR
					if ((q = wlc_nar_prec_pktq(wlc, scb))) {
						wlc_pktq_stats_free(wlc, q);
					}
#endif // endif
#ifdef WLTAF
					wlc_taf_dpstats_free(wlc, scb);
#endif // endif
				}
#ifdef MBSS
				else if ((scb = WLC_BCMCSCB_GET(wlc, cfg)) &&
					(q = wlc_apps_get_psq(wlc, scb))) {
					wlc_pktq_stats_free(wlc, q);
				}
#endif // endif
				iov_pktq->pktq_log.v06.num_prec[index] = 0xFF;

				break;
			}
#ifdef MBSS
			case 'B':
			{
				struct scb * scb = WLC_BCMCSCB_GET(wlc, cfg);

				if (scb) {
					flags |= PKTQ_LOG_PS_QUEUE;

					bcm_bprintf(b, "broadcast queue%s", taf_status);
					wlc_txq_prec_dump(wlc, wlc_apps_get_psq(wlc, scb),
						iov_pktq, index, resetstats, tsf_timelo, tsf_timehi,
						prec_mask, (uint32)-1, flags, NULL);
				}
				break;
			}
#endif // endif
#ifdef AP
			case 'P':
			{
				flags |= PKTQ_LOG_PS_QUEUE;

				pktq_func = wlc_apps_get_psq;
				queue_type = "power save";
				break;
			}
#endif // endif
#ifdef WLAMPDU
			case 'M':
			{
				struct scb *scb = NULL;
				bool addr_OK = (!ETHER_ISMULTI(ea) &&
						(scb = wlc_scbfind(wlc, cfg, ea)));

				if (addr_OK) {
					flags |= PKTQ_LOG_DISPLAY_ETHERADDR;

					wlc_ampdu_dpstats_dump(wlc, scb, iov_pktq, index,
						resetstats, tsf_timelo, tsf_timehi,
						prec_mask, flags);

					bcm_bprintf(b, "%s%s%s%s",
						SCB_VHTMU(scb) ? " vhtmu" : "",
						SCB_HEMMU(scb) ? " hemmu" : "",
						SCB_DLOFDMA(scb) ? " dlofdma" : "",
						SCB_ULOFDMA(scb) ? " ulofdma" : "");
				} else {
					iov_pktq->pktq_log.v06.num_prec[index] = 0;
				}
				break;
			}

			case 'A':
			{
				flags |= PKTQ_LOG_SCB_QUEUE;
#ifdef WLSQS
				flags |= PKTQ_LOG_SQS;
#endif // endif
				pktq_func = scb_ampdu_prec_pktq;
				queue_type = "AMPDU";
				break;
			}
#endif /* WLAMPDU */
#ifdef WLNAR
			case 'N':
			{
				flags |= PKTQ_LOG_SCB_QUEUE;
				pktq_func = wlc_nar_prec_pktq;
				queue_type = "NAR";
				break;
			}
#endif /* WLNAR */
#ifdef WLTAF
			case 'S': /* Scheduler */
			{
				struct scb *scb = NULL;
				bool addr_OK = (!ETHER_ISMULTI(ea) &&
					(scb = wlc_scbfind(wlc, cfg, ea)));

				if (addr_OK) {
					const char * label = NULL;
					flags |= PKTQ_LOG_DISPLAY_ETHERADDR;
#ifdef WLSQS
					flags |= PKTQ_LOG_SQS;
#endif // endif

					wlc_taf_dpstats_dump(wlc, scb, iov_pktq, index,
						resetstats, tsf_timelo, tsf_timehi,
						prec_mask, flags, &label);

					bcm_bprintf(b, "%s", label ? label : "");
				} else {
					iov_pktq->pktq_log.v06.num_prec[index] = 0;
				}
				break;
			}
#endif /* WLTAF */
		}

		if (pktq_func) {
			struct scb *scb = NULL;

			bool addr_OK = (!ETHER_ISMULTI(ea) && (scb = wlc_scbfind(wlc, cfg, ea)));

			if (addr_OK) {
				uint32  pps_time = (uint32)-1;

				flags |= PKTQ_LOG_DISPLAY_ETHERADDR;

				bcm_bprintf(b, "%s%s queue%s", queue_type,
					SCB_DWDS(scb) ? " (dwds)":"", taf_status);

#if defined(BCMDBG) && defined(PSPRETEND)
				if (SCB_PS_PRETEND_ENABLED(cfg, scb) &&
				             (params->addr_type[index] & 0x7F) == 'P') {
					pps_time = wlc_pspretend_scb_time_get(wlc->pps_info, scb);
				}
#endif /* PSPRETEND */
				wlc_txq_prec_dump(wlc, pktq_func(wlc, scb), iov_pktq, index,
				        resetstats, tsf_timelo, tsf_timehi, prec_mask,
					pps_time, flags, scb);

				if ((params->addr_type[index] & 0x7F) == 'A' &&
					(prec_mask & PKTQ_LOG_AUTO)) {

					/* scan active ampdu contexts and auto-enable logging */
					wlc_ampdu_pktqlog_enable(wlc, scb);
				}
			} else {
				iov_pktq->pktq_log.v06.num_prec[index] = 0;
			}
		}

		addrs_todo--;
		ea++;
	}
	return BCME_OK;
} /* wlc_pktq_stats */
#endif /* PKTQ_LOG */

#ifdef PKTQ_STATUS
int wlc_pktq_curstatus(struct pktq *pktq, int prec, struct bcmstrbuf *b)
{
	struct pktq_prec *q;
	int i;

	for (i = 0; i < prec; i++) {
		q = &pktq->q[i];
		if (q && q->n_pkts) {
			bcm_bprintf(b, "\t %d\t %d\t %d\t %d\n", i,
			            q->n_pkts, q->max_pkts, q->stall_count);
		}
	}
	return BCME_OK;
}
#endif /* PKTQ_STATUS */

/* wlc up/down */
static int
wlc_perf_utils_wlc_up(void *ctx)
{
#if defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND)
	wlc_perf_utils_t *pui = ctx;
	wlc_info_t *wlc = pui->wlc;

	/* Start the Packet Delay stats' periodic sampling timer */
	if (pui->pktdlystat_timer != NULL) {
		wl_add_timer(wlc->wl, pui->pktdlystat_timer, TXDELAY_TIMER_PERIOD, TRUE);
	}
#endif /* defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND) */
	return BCME_OK;
}

static int
wlc_perf_utils_wlc_down(void *ctx)
{
#if defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND)
	wlc_perf_utils_t *pui = ctx;
	wlc_info_t *wlc = pui->wlc;

	/* Start the Packet Delay stats' periodic sampling timer */
	if (pui->pktdlystat_timer != NULL) {
		wl_del_timer(wlc->wl, pui->pktdlystat_timer);
	}
#endif /* defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND) */
	return BCME_OK;
}

/* iovar dispatcher */
static int
wlc_perf_utils_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_perf_utils_t *pui = hdl;
	wlc_info_t *wlc = pui->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	BCM_REFERENCE(bsscfg);

	/* Do the actual parameter implementation */
	switch (actionid) {

#ifdef PKTQ_LOG
	case IOV_GVAL(IOV_DP_STATS): {
		uint8 params_version = (((wl_iov_mac_params_t*) params)->num_addrs) >> 8;

		if (params_version != 6) {
			wl_iov_pktq_log_t*  iov_pktq_log_p  =  (wl_iov_pktq_log_t*) arg;

			iov_pktq_log_p->version = 0x06;
			/* only support param block v6 for dpstats */
			return BCME_VERSION;
		}

		/* fall through to IOV_PKTQ_STATS */
	}
	case IOV_GVAL(IOV_PKTQ_STATS): {
		uint8 params_version;
		wl_iov_pktq_log_t*  iov_pktq_log_p  =  (wl_iov_pktq_log_t*) arg;
		wl_iov_mac_full_params_t iov_full_params;

		/* keep a local copy of the input parameter block */
		iov_full_params.params = *(wl_iov_mac_params_t*) params;
		params_version = (iov_full_params.params.num_addrs >> 8) & 0xFF;

		if (params_version != 4 && params_version != 6) {
			iov_pktq_log_p->version = 0x06;
			return BCME_VERSION;
		}

		iov_full_params.params.num_addrs &= ~(0xFF << 8);

		iov_full_params.extra_params =
			((wl_iov_mac_full_params_t*) params)->extra_params;

		/* clear return parameter block */
		bzero(iov_pktq_log_p, sizeof(*iov_pktq_log_p));
		iov_pktq_log_p->version = 0x06;

		/* Copy back the input parameter block */
		iov_pktq_log_p->params = iov_full_params.params;

		err = wlc_pktq_stats(&iov_full_params, params_version, iov_pktq_log_p,
		                     wlc, bsscfg);
		break;
	}
#endif /* PKTQ_LOG */
#ifdef PKTQ_STATUS
	case IOV_GVAL(IOV_PKTQ_STATUS):	{
		scb_pktq_status_t *p = (scb_pktq_status_t *)params;
		struct ether_addr ea;
		struct scb *scb = NULL;
		struct pktq *pktq = NULL;
		struct bcmstrbuf b;
		wlc_txq_info_t* qi = wlc->active_queue;
		bool addr_OK;

		if (p->ver != WL_PKTQ_STATUS_VERSION) {
			err = BCME_VERSION;
			break;
		}

		memcpy(&ea, &p->ea, sizeof(ea));
		addr_OK = (!ETHER_ISMULTI(&ea) && (scb = wlc_scbfind(wlc, bsscfg, &ea)));

		bcm_binit(&b, (char*)arg, len);
		bcm_bprintf(&b, "QUEUE\t Prec\t Qued\t Max\t Stalls\n");
		bcm_bprintf(&b, "-----\t ----\t ----\t ---\t ------\n");
		if (addr_OK) {
			pktq = scb_ampdu_prec_pktq(wlc, scb);
			bcm_bprintf(&b, "AMPDU:\n");
			wlc_pktq_curstatus(pktq, WLC_PREC_COUNT >> 1, &b);
#ifdef WLNAR
			pktq = wlc_nar_prec_pktq(wlc, scb);
			bcm_bprintf(&b, "NAR:\n");
			wlc_pktq_curstatus(pktq, WLC_PREC_COUNT, &b);
#endif // endif
			pktq = wlc_apps_get_psq(wlc, scb);
			bcm_bprintf(&b, "PS:\n");
			wlc_pktq_curstatus(pktq, WLC_PREC_COUNT, &b);

			pktq = WLC_GET_CQ(qi);
			bcm_bprintf(&b, "Common:\n");
			wlc_pktq_curstatus(pktq, WLC_PREC_COUNT, &b);

			bcm_bprintf(&b, "-------------------------------\nLowTxQ:\n");
			wlc_get_lowtxq_pktcount(wlc, scb, &b);
#ifdef BULK_PKTLIST
			bcm_bprintf(&b, "HWFifo:\n");
			wlc_hwfifo_pktcount(wlc, scb, &b);
#endif // endif
		}
		break;
	}
#endif /* PKTQ_STATUS */
#ifdef WLPKTDLYSTAT
#ifdef WLPKTDLYSTAT_IND
	case IOV_GVAL(IOV_TXDELAY_PARAMS): {
		txdelay_params_t *txdelay_params = (txdelay_params_t *)arg;
		txdelay_params->ratio = pui->txdelay_params->ratio;
		txdelay_params->period = pui->txdelay_params->period;
		txdelay_params->cnt = pui->txdelay_params->cnt;
		txdelay_params->tune = pui->txdelay_params->tune;
		break;
	}
	case IOV_SVAL(IOV_TXDELAY_PARAMS): {
		txdelay_params_t *txdelay_params = (txdelay_params_t *)arg;
		pui->txdelay_params->ratio = txdelay_params->ratio;
		pui->txdelay_params->period = txdelay_params->period;
		pui->txdelay_params->cnt = txdelay_params->cnt;
		pui->txdelay_params->tune = txdelay_params->tune;
		break;
	}
#endif /* WLPKTDLYSTAT_IND */
	case IOV_GVAL(IOV_DLYSTATS): {
		txdelay_stats_t *p = (txdelay_stats_t *)params;
		struct ether_addr ea;

		if (p->version != TXDELAY_STATS_VERSION) {
			err = BCME_VERSION;
			break;
		}
		memcpy(&ea, &p->scb_delay_stats[0].ea, sizeof(ea));
		err = wlc_dlystats(wlc, arg, &ea);
		break;
	}
	case IOV_SVAL(IOV_DLYSTATS_CLEAR):
		wlc_dlystats_clear(wlc);
		break;
#endif /* WLPKTDLYSTAT */

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
	case IOV_GVAL(IOV_BMAC_PCIE_BUS_TPUT): {
		pcie_bus_tput_stats_t *stats = (pcie_bus_tput_stats_t *)arg;

		if (!BCMPCIEDEV_ENAB())
			break;

		if (pui->bmac_bus_tput_info->test_running) {
			stats->count = 0;
			break;
		}
		stats->time_taken = 5;
		stats->nbytes_per_descriptor = pui->bmac_bus_tput_info->host_buf_len;
		stats->count = pui->bmac_bus_tput_info->pktcnt_tot;
		break;
	}

	case IOV_SVAL(IOV_BMAC_PCIE_BUS_TPUT): {
		pcie_bus_tput_params_t *tput_params = (pcie_bus_tput_params_t*)params;

		if (!BCMPCIEDEV_ENAB())
			break;

		if (pui->bmac_bus_tput_info->test_running)
			break;

		pui->bmac_bus_tput_info->pktcnt_tot = 0;
		pui->bmac_bus_tput_info->max_dma_descriptors =
			tput_params->max_dma_descriptors;
		pui->bmac_bus_tput_info->pktcnt_cur = tput_params->max_dma_descriptors;
		bcopy(&tput_params->host_buf_addr, &pui->bmac_bus_tput_info->host_buf_addr,
			sizeof(pui->bmac_bus_tput_info->host_buf_addr));
		pui->bmac_bus_tput_info->host_buf_len = tput_params->host_buf_len;

		wlc_bmac_suspend_mac_and_wait(wlc->hw);
		pui->bmac_bus_tput_info->test_running = TRUE;
		wl_add_timer(wlc->wl, pui->bmac_bus_tput_info->dma_timer, 0, FALSE);
		wl_add_timer(wlc->wl, pui->bmac_bus_tput_info->end_timer,
			5000, FALSE);
		break;
	}
#endif /* BCMPCIDEV && BUS_TPUT */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_perf_utils_doiovar */

#ifdef BCMDBG
void
wlc_update_perf_stats(wlc_info_t *wlc, uint32 mask)
{
	wlc_perf_utils_t *pui = wlc->pui;
	wlc_perf_stats_t *stats = pui->perf_stats;

	if (mask & WLC_PERF_STATS_ISR) stats->n_isr++;
	if (mask & WLC_PERF_STATS_DPC) stats->n_dpc++;
	if (mask & WLC_PERF_STATS_TMR_DPC) stats->n_timer_dpc++;
	if (mask & WLC_PERF_STATS_BCN_ISR) stats->n_bcn_isr++;
	if (mask & WLC_PERF_STATS_BCNS) stats->n_beacons++;
	if (mask & WLC_PERF_STATS_PRB_REQ) stats->n_probe_req++;
	if (mask & WLC_PERF_STATS_PRB_RESP) stats->n_probe_resp++;
}

void
wlc_update_isr_stats(wlc_info_t *wlc, uint32 macintstatus)
{
	wlc_perf_utils_t *pui = wlc->pui;
	wlc_isr_stats_t *stats = &pui->perf_stats->isr_stats;
	uint32 i;

	/* Update the overall Stats Count as well */
	if (macintstatus)
		wlc_update_perf_stats(wlc, WLC_PERF_STATS_ISR);

	for (i = 0; i < 32; i++)
		if (macintstatus & (1<<i)) {
			stats->n_counts[i]++;
		}

}

void
wlc_update_p2p_stats(wlc_info_t *wlc, uint32 bss)
{
#ifdef WLP2P
	wlc_perf_utils_t *pui = wlc->pui;
	wlc_isr_stats_t *stats = &pui->perf_stats->isr_stats;

	stats->n_p2p[bss]++;
#endif // endif
}
#endif /* BCMDBG */

#ifdef WLPKTDLYSTAT
static uint
wlc_dlystats_scb_secsz(void *ctx, struct scb *scb)
{
#ifdef NOT_YET
	if (SCB_INTERNAL(scb))
		return 0;
#endif // endif
	return sizeof(scb_cubby_t);
}

static int
wlc_dlystats_scb_init(void *ctx, struct scb *scb)
{
	wlc_info_t *wlc = ctx;

	scb->delay_stats = wlc_scb_sec_cubby_alloc(wlc, scb,
		wlc_dlystats_scb_secsz(ctx, scb));
#ifdef WLPKTDLYSTAT_IND
	if (scb->delay_stats != NULL) {
		scb->txdelay_params = (txdelay_params_t *)&scb->delay_stats[AC_COUNT];
	}
#endif // endif
	return BCME_OK;
}

static void
wlc_dlystats_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_info_t *wlc = ctx;

	if (scb->delay_stats != NULL) {
		wlc_scb_sec_cubby_free(wlc, scb, scb->delay_stats);
		scb->delay_stats = NULL;
#ifdef WLPKTDLYSTAT_IND
		scb->txdelay_params = NULL;
#endif // endif
	}
}

void
wlc_scb_dlystat_dump(struct scb *scb, struct bcmstrbuf *b)
{
	int i, last;
	uint ac;
	uint32 sum;
	uint32 max_val, total = 0;
	scb_delay_stats_t *delay_stats;

	/* Report Latency and packet loss statistics (for each AC) */
	for (ac = 0; ac < AC_COUNT; ac++) {

		delay_stats = scb->delay_stats + ac;
		for (i = 0, total = 0, sum = 0; i < RETRY_SHORT_DEF; i++) {
			total += delay_stats->txmpdu_cnt[i];
			sum += delay_stats->delay_sum[i];
		}
		if (total) {
			bcm_bprintf(b, "%s:  PLoss %d/%d (%d%%)\n",
					aci_names[ac], delay_stats->txmpdu_lost, total,
					(delay_stats->txmpdu_lost * 100) / total);
			bcm_bprintf(b, "\tDelay stats in usec (avg/min/max): %d %d %d\n",
					CEIL(sum, total), delay_stats->delay_min,
					delay_stats->delay_max);
			bcm_bprintf(b, "\tTxcnt Hist    :");
			for (i = 0; i < RETRY_SHORT_DEF; i++) {
				bcm_bprintf(b, "  %d", delay_stats->txmpdu_cnt[i]);
			}
			bcm_bprintf(b, "\n\tDelay vs Txcnt:");
			for (i = 0; i < RETRY_SHORT_DEF; i++) {
				if (delay_stats->txmpdu_cnt[i])
					bcm_bprintf(b, "  %d", delay_stats->delay_sum[i] /
							delay_stats->txmpdu_cnt[i]);
				else
					bcm_bprintf(b, "  -1");
			}
			bcm_bprintf(b, "\n");

			for (i = 0, max_val = 0, last = 0; i < WLPKTDLY_HIST_NBINS; i++) {
				max_val += delay_stats->delay_hist[i];
				if (delay_stats->delay_hist[i]) last = i;
			}
			last = 8 * (last/8 + 1) - 1;
			if (max_val) {
				bcm_bprintf(b, "\tDelay Hist \n\t\t:");
				for (i = 0; i <= last; i++) {
					bcm_bprintf(b, " %d(%d%%)", delay_stats->
							delay_hist[i],
							(delay_stats->delay_hist[i] * 100)
							/ max_val);
					if ((i % 8) == 7 && i != last)
						bcm_bprintf(b, "\n\t\t:");
				}
			}
			bcm_bprintf(b, "\n");
		}
	}
	bcm_bprintf(b, "\n");
}

static int
wlc_dlystats(wlc_info_t *wlc, txdelay_stats_t *dlystats, struct ether_addr *scb_addr)
{
	uint32 ret = 0, scb_idx = 0;

	struct scb *scb;
	struct scb_iter scbiter;

	scb_total_delay_stats_t *scb_stats;
	uint32 specified_macaddr;
	struct ether_addr zmac;

	memset(&zmac, 0, sizeof(zmac));
	specified_macaddr = memcmp(scb_addr, &zmac, sizeof(zmac));
	memset(dlystats, 0, sizeof(*dlystats));

	dlystats->full_result = TXDELAY_STATS_FULL_RESULT;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_stats = dlystats->scb_delay_stats+scb_idx;
		if (specified_macaddr) {
			if (!memcmp(scb_addr, &scb->ea, sizeof(*scb_addr))) {
				dlystats->scb_cnt = 1;
				memcpy(scb_stats->dlystats, scb->delay_stats,
				       sizeof(*(scb->delay_stats)) * AC_COUNT);
				memcpy(&scb_stats->ea, &scb->ea, sizeof(scb_stats->ea));
				break;
			}
		} else {
			scb_idx = dlystats->scb_cnt++;
			if (MAX_TXDELAY_STATS_SCBS < dlystats->scb_cnt) {
				dlystats->scb_cnt = MAX_TXDELAY_STATS_SCBS;
				dlystats->full_result = TXDELAY_STATS_PARTIAL_RESULT;
				break;
			}

			memcpy(scb_stats->dlystats, scb->delay_stats,
			       sizeof(*(scb->delay_stats)) * AC_COUNT);
			memcpy(&scb_stats->ea, &scb->ea, sizeof(scb_stats->ea));
		}
	}

	return ret;
}

void
wlc_dlystats_clear(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		memset(scb->delay_stats, 0, sizeof(*(scb->delay_stats)) * AC_COUNT);
	}
}

void
wlc_delay_stats_upd(scb_delay_stats_t *delay_stats, uint32 delay, uint tr, bool ack_recd)
{
	WLCNTSCBINCR(delay_stats->txmpdu_cnt[tr]);
	WLCNTSCBADD(delay_stats->delay_sum[tr], delay);
	if (delay_stats->delay_min == 0) {
		delay_stats->delay_min = delay;
		delay_stats->delay_max = delay;
	} else {
		if (delay < delay_stats->delay_min)
			delay_stats->delay_min = delay;
		else if (delay > delay_stats->delay_max)
			delay_stats->delay_max = delay;
	}
	WLCNTSCBINCR(delay_stats->delay_hist[MIN(delay/500, WLPKTDLY_HIST_NBINS-1)]);
	if (!ack_recd)
		WLCNTSCBINCR(delay_stats->txmpdu_lost);
}

#ifdef WLPKTDLYSTAT_IND
/** This function calculate avg txdly and trigger */
static void
wlc_delay_stats_timer(void *arg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_perf_utils_t *pui = arg;
	wlc_info_t *wlc = pui->wlc;
	txdelay_event_t txdelay;
	int rssi;
	uint8 ac, i;
	uint32 sum_1s, total_1s, avg_1s;
	uint32 avg, delta;
	scb_delay_stats_t *delay_stats;
	txdelay_params_t *txdelay_params;
	char eabuf[ETHER_ADDR_STR_LEN];

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_ASSOCIATED(scb)) {
			txdelay_params = scb->txdelay_params;

			txdelay_params->period += 1;
			txdelay_params->period %=
				pui->txdelay_params->period;

			if (txdelay_params->period)
				continue;

			ac = AC_VI;
			delay_stats = scb->delay_stats + ac;

			/* Calculate txdelay avg */
			total_1s = 0, sum_1s = 0;
			for (i = 0; i < RETRY_SHORT_DEF; i++) {
				total_1s += delay_stats->txmpdu_cnt[i];
				sum_1s += delay_stats->delay_sum[i];
			}
			/* total_1s or sum_1s variable overflow check */
			if (total_1s >= delay_stats->prev_txmpdu_cnt)
				total_1s -= delay_stats->prev_txmpdu_cnt;
			else
				total_1s += BCM_UINT32_MAX - delay_stats->prev_txmpdu_cnt;

			if (sum_1s >= delay_stats->prev_delay_sum)
				sum_1s -= delay_stats->prev_delay_sum;
			else
				sum_1s += BCM_UINT32_MAX - delay_stats->prev_delay_sum;

			/* Store current txmpdu count and delay_sum for next second */
			delay_stats->prev_txmpdu_cnt += total_1s;
			delay_stats->prev_delay_sum += sum_1s;

			if (total_1s != 0) {
				avg_1s = sum_1s / total_1s;
				/* for the first calculation */
				if (delay_stats->delay_avg == 0) {
					delay_stats->delay_count = TXDELAY_MIN_SAMPLE_DATA - 1;
					delay_stats->delay_avg = avg_1s;
					continue;
				}

				avg = delay_stats->delay_avg;
				delta = (avg * pui->txdelay_params->ratio)/100;

				/* Collect SCB rssi */
				wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_WLC, TRUE);
				rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);

				if (pui->txdelay_params->tune) {
					bcm_ether_ntoa(&scb->ea, eabuf);
					WL_ERROR(("TXDLY:[%s] rssi=%d avg_1s(%5d)-avg(%d)=%d "
						"(avg*ratio)/100=%d\n", eabuf, rssi,
						avg_1s, avg, avg_1s - avg, delta));
				}
				/* Collect stats for atleast delay_count times before next event */
				if (delay_stats->delay_count) {
					delay_stats->delay_count--;
					delay_stats->delay_avg =
					(delay_stats->delay_avg *
					(TXDELAY_MIN_SAMPLE_DATA - 1))/ TXDELAY_MIN_SAMPLE_DATA +
					avg_1s / TXDELAY_MIN_SAMPLE_DATA;
					continue;
				}
				avg = delay_stats->delay_avg;
				delta = (avg * pui->txdelay_params->ratio)/100;

				/* Check if need to trigger event */
				if ((avg_1s > avg) && ((avg_1s - avg) > delta)) {
					txdelay_params->cnt++;

					if (txdelay_params->cnt >=
						pui->txdelay_params->cnt) {
						if (pui->txdelay_params->tune)
							WL_ERROR(("TXDLY:Event Triggered\n"));

						txdelay_params->cnt = 0;

						/* Issue PKT TX Delay Stats Event */
						memset(&txdelay, 0, sizeof(txdelay_event_t));
						memcpy(&(txdelay.delay_stats), scb->delay_stats,
						       sizeof(*(scb->delay_stats)) * AC_COUNT);

						(void)wlc_lq_chanim_stats_get(wlc->chanim_info,
							wlc->home_chanspec, &txdelay.chanim_stats);
						txdelay.rssi = rssi;

						wlc_bss_mac_event(wlc, scb->bsscfg,
							WLC_E_PKTDELAY_IND, &scb->ea, 0, 0, 0,
							&txdelay, sizeof(txdelay_event_t));
						WL_INFORM(("WLC_E_PKTDELAY_IND:Event Triggered\n"));

						delay_stats->delay_avg = 0;
						delay_stats->delay_count =
							TXDELAY_MIN_SAMPLE_DATA - 1;
					}
				}
				else
					txdelay_params->cnt = 0;
				delay_stats->delay_avg =
					(delay_stats->delay_avg * (TXDELAY_MIN_SAMPLE_DATA - 1))/
					TXDELAY_MIN_SAMPLE_DATA + avg_1s / TXDELAY_MIN_SAMPLE_DATA;
			}
		}
	}
} /* wlc_delay_stats_timer */
#endif /* WLPKTDLYSTAT_IND */
#endif /* WLPKTDLYSTAT */

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
void
wlc_bmac_hostbus_tpt_prog_d11dma_timer(void *arg)
{
	wlc_info_t *wlc;
	hnddma_t *di;
	int i;

	wlc_perf_utils_t *pui = (wlc_perf_utils_t *)arg;
	dma64addr_t addr = { .low_addr = 0, .high_addr = 0};

	if (pui == NULL)
		return;

	wlc = pui->wlc;
	di = WLC_HW_DI(wlc, 1);

	/* Reclaim all the dma descriptors programmed
	 * Update the number of completed dma
	 */
	while (GETNEXTTXP(wlc, 1)) {
		if (pui->bmac_bus_tput_info->test_running)
			pui->bmac_bus_tput_info->pktcnt_tot++;
		pui->bmac_bus_tput_info->pktcnt_cur++;
	}

	/* If all the descriptors programmed are not reclaimed
	 * wait until all dma is completed
	 */
	if (pui->bmac_bus_tput_info->pktcnt_cur < pui->bmac_bus_tput_info->max_dma_descriptors) {
		wl_add_timer(wlc->wl, pui->bmac_bus_tput_info->dma_timer, 0, FALSE);
		return;
	}

	/* reset pkt current counter to 0 */
	pui->bmac_bus_tput_info->pktcnt_cur = 0;

	/* ucode is suspended and data dmaed to fifo is not valid.
	 * So flush the fifo to clear the data so that fifo does not become full.
	 */
	wlc_bmac_tx_fifos_flush(wlc->hw, TX_AC_VI_FIFO);

	/* wlc_bmac_tx_fifos_flush will keep fifo supsended, hence resume it back */
	dma_txresume(di);

	/* sync the tx fifo if test is not running */
	if (!pui->bmac_bus_tput_info->test_running) {
		uint8 fifo_bitmap[TX_FIFO_BITMAP_SIZE_MAX] = { 0 };

		setbit(fifo_bitmap, TX_AC_VI_FIFO);
		wlc_bmac_tx_fifo_sync(wlc->hw, fifo_bitmap, FLUSHFIFO);
		return;
	}

	/* Program one set of descriptors */
	PHYSADDR64HISET(addr, (uint32) ((1<<31) |
			(PHYSADDRHI(pui->bmac_bus_tput_info->host_buf_addr))));
	PHYSADDR64LOSET(addr, (uint32)PHYSADDRLO(pui->bmac_bus_tput_info->host_buf_addr));

	/* max - 1 descriptors are programmed without commiting */
	for (i = 1; i < pui->bmac_bus_tput_info->max_dma_descriptors; i++) {
		if (dma_msgbuf_txfast(di, addr, FALSE,
			pui->bmac_bus_tput_info->host_buf_len, TRUE, TRUE)) {
			WL_ERROR(("%s:%d Programming descriptors failed\n",
				__FUNCTION__, __LINE__));
		}
	}
	/* commit to trigger dma of all descriptors programmed */
	if (dma_msgbuf_txfast(di, addr, TRUE,
		pui->bmac_bus_tput_info->host_buf_len, TRUE, TRUE)) {
		WL_ERROR(("%s:%d Programming descriptors failed\n",
			__FUNCTION__, __LINE__));
	}
	wl_add_timer(wlc->wl, pui->bmac_bus_tput_info->dma_timer, 0, FALSE);
}

void
wlc_bmac_hostbus_tpt_prog_d11dma_timer_stop(void *arg)
{
	wlc_perf_utils_t *pui = (wlc_perf_utils_t *)arg;
	wlc_info_t *wlc;

	if (pui == NULL)
		return;

	wlc = pui->wlc;
	pui->bmac_bus_tput_info->test_running = FALSE;
	wl_del_timer(wlc->wl, pui->bmac_bus_tput_info->dma_timer);
}
#endif /* BCMPCIEDEV && BUS_TPUT */
