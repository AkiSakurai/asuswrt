/*
 * Propagate txstatus (also flow control) source
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
 * $Id: wlc_wlfc.c 791041 2020-09-14 18:00:17Z $
 */

#ifndef DONGLEBUILD
#error "This file is for dongle only"
#endif // endif

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_apps.h>
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#endif // endif
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#include <wlc_bsscfg_psq.h>
#include <wlc_event.h>
#ifdef BCMPCIEDEV
#include <flring_fc.h>
#include <bcmmsgbuf.h>
#include <wlc_scb_ratesel.h>
#endif // endif
#include <wlc_pcb.h>
#include <wlc_qoscfg.h>
#include <wlc_hw.h>
#include <wlc_event_utils.h>
#include <wlc_p2p.h>
#include <wlc_tx.h>
#include <wlc_rspec.h>
#ifdef BCMFRWDPOOLREORG
#include <hnd_poolreorg.h>
#endif /* BCMFRWDPOOLREORG */
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif
#ifdef WLCFP
#include <wlc_cfp.h>
#endif // endif
#ifdef WLSQS
#include <wlc_sqs.h>
#endif // endif

#include <wlc_wlfc.h>

#ifdef WLTAF
#include <wlc_taf.h>
#endif /* WLTAF */

#include <wlc_txmod.h>

/* iovar table */
enum {
	IOV_TLV_SIGNAL = 0,
	IOV_COMPSTAT = 1,
	IOV_WLFC_MODE = 2,
	IOV_PROPTX_CRTHRESH = 3,	/* PROPTX credit report level */
	IOV_PROPTX_DATATHRESH = 4,	/* PROPTX pending data report level */
	IOV_LAST
};

static const bcm_iovar_t wlfc_iovars[] = {
	{"tlv", IOV_TLV_SIGNAL,	0, 0, IOVT_UINT8, 0},
	{"compstat", IOV_COMPSTAT, 0, 0, IOVT_BOOL, 0},
	{"wlfc_mode", IOV_WLFC_MODE, 0, 0, IOVT_UINT8, 0},
	{"proptx_credit_thresh", IOV_PROPTX_CRTHRESH, 0, 0, IOVT_UINT32, 0},
	{"proptx_data_thresh", IOV_PROPTX_DATATHRESH, 0, 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* stats */
typedef struct wlfc_fw_stats {
	uint32	packets_from_host;
	uint32	txstatus_count;
	uint32	txstats_other;
} wlfc_fw_stats_t;

typedef struct wlfc_fw_dbgstats {
	/* how many header only packets are allocated */
	uint32	nullpktallocated;
	uint32	realloc_in_sendup;
	uint32	wlfc_wlfc_toss;
	uint32	wlfc_wlfc_sup;
	uint32	wlfc_pktfree_except;
	uint32	creditupdates;
	uint32	creditin;
	uint32	nost_from_host;
	uint32	sig_from_host;
	uint32	credits[NFIFO_LEGACY];
} wlfc_fw_dbgstats_t;

/* module private info */
struct wlc_wlfc_info {
	wlc_info_t *wlc;
	int	scbh;
	bool	comp_stat_enab;
	uint8	wlfc_mode;
	uint8	wlfc_vqdepth;		/**< max elements to store in psq during flow control */
	uint8	replay_counter;		/**< only 3 bits are used */
	uint32	bitmap;			/**< each bit indicates availability, taken if set to 1. */
	uint16	pending_datalen;
	uint8	fifo_credit_back_pending;
	uint8	timer_started;
	struct wl_timer *fctimer;
	uint32	compressed_stat_cnt;
	uint8	total_credit;
	uint8	wlfc_trigger;
	uint8	wlfc_fifo_bo_cr_ratio;
	uint8	wlfc_comp_txstatus_thresh;
	uint16	pending_datathresh;
	uint8	txseqtohost;
	uint8	totalcredittohost;
	uint8	data[WLFC_MAX_PENDING_DATALEN];
	uint8	fifo_credit[WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK];
	uint8	fifo_credit_threshold[WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK];
	uint8	fifo_credit_back[WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK];
	uint8	fifo_credit_in[WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK];
	wlfc_fw_stats_t	stats;
	wlfc_fw_dbgstats_t *dbgstats;
};

/* debug stats access macros */
#define WLFC_COUNTER_TXSTATUS_WLCTOSS(wlfc)	do {(wlfc)->dbgstats->wlfc_wlfc_toss++;} while (0)

/* stats access macros */
#define WLFC_COUNTER_TXSTATUS_COUNT(wlfc)	do {(wlfc)->stats.txstatus_count++;} while (0)
#define WLFC_COUNTER_TXSTATUS_OTHER(wlfc)	do {(wlfc)->stats.txstats_other++;} while (0)

/**
 * Used to prevent firmware from sending certain PropTxStatus related signals to the host when the
 * host did not request them.
 */
#define WLFC_FLAGS_XONXOFF_MASK \
	((1 << WLFC_CTL_TYPE_MAC_OPEN) | \
	(1 << WLFC_CTL_TYPE_MAC_CLOSE) | \
	(1 << WLFC_CTL_TYPE_MACDESC_ADD) | \
	(1 << WLFC_CTL_TYPE_MACDESC_DEL) | \
	(1 << WLFC_CTL_TYPE_INTERFACE_OPEN) | \
	(1 << WLFC_CTL_TYPE_INTERFACE_CLOSE))

/**
 * Used to e.g. prevent PropTxStatus credit signals from reaching the host when the host was not
 * configured (during initialization) to receive credit signals.
 */
#define WLFC_FLAGS_CREDIT_STATUS_MASK \
	((1 << WLFC_CTL_TYPE_FIFO_CREDITBACK) | \
	(1 << WLFC_CTL_TYPE_MAC_REQUEST_CREDIT) | \
	(1 << WLFC_CTL_TYPE_MAC_REQUEST_PACKET) | \
	(1 << WLFC_CTL_TYPE_TXSTATUS))
#define WLFC_FLAGS_PKT_STAMP_MASK \
	((1 << WLFC_CTL_TYPE_RX_STAMP) | \
	(1 << WLFC_CTL_TYPE_TX_ENTRY_STAMP))

#define WLFC_PENDING_TRIGGER_WATERMARK	48
#define WLFC_SENDUP_TIMER_INTERVAL	10
#define WLFC_DEFAULT_FWQ_DEPTH		1
#define WLFC_CREDIT_TRIGGER		1
#define WLFC_TXSTATUS_TRIGGER		2

/* scb cubby */
typedef struct {
	uint8 mac_address_handle;
	uint8 status;
	uint16 first_sup_pkt;
} scb_wlfc_info_t;

/* access macros */
#define SCB_WLFC_INFO(wlfc, scb)	(scb_wlfc_info_t *)SCB_CUBBY(scb, (wlfc)->scbh)

#define SCB2_PROPTXTSTATUS_SUPPR_STATEMASK      0x01
#define SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT     0
#define SCB2_PROPTXTSTATUS_SUPPR_GENMASK        0x02
#define SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT       1
#define SCB2_PROPTXTSTATUS_PKTWAITING_MASK      0x04
#define SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT     2
#define SCB2_PROPTXTSTATUS_POLLRETRY_MASK       0x08
#define SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT      3
/* 4 bits for AC[0-3] traffic pending status from the host */
#define SCB2_PROPTXTSTATUS_TIM_MASK             0xf0
#define SCB2_PROPTXTSTATUS_TIM_SHIFT            4

#define SCB_PROPTXTSTATUS_SUPPR_STATE(s)	(((s)->status & \
	SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) >> SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT)
#define SCB_PROPTXTSTATUS_SUPPR_GEN(s)		(((s)->status & \
	SCB2_PROPTXTSTATUS_SUPPR_GENMASK) >> SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT)
#define SCB_PROPTXTSTATUS_TIM(s)		(((s)->status & \
	SCB2_PROPTXTSTATUS_TIM_MASK) >> SCB2_PROPTXTSTATUS_TIM_SHIFT)
#define SCB_PROPTXTSTATUS_PKTWAITING(s)		(((s)->status & \
	SCB2_PROPTXTSTATUS_PKTWAITING_MASK) >> SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT)
#define SCB_PROPTXTSTATUS_POLLRETRY(s)		(((s)->status & \
	SCB2_PROPTXTSTATUS_POLLRETRY_MASK) >> SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT)

#define SCB_PROPTXTSTATUS_SUPPR_SETSTATE(s, state)	(s)->status = ((s)->status & \
		~SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) | \
		(((state) << SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT) & \
		SCB2_PROPTXTSTATUS_SUPPR_STATEMASK)
#define SCB_PROPTXTSTATUS_SUPPR_SETGEN(s, gen)		(s)->status = ((s)->status & \
		~SCB2_PROPTXTSTATUS_SUPPR_GENMASK) | \
		(((gen) << SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT) & \
		SCB2_PROPTXTSTATUS_SUPPR_GENMASK)
#define SCB_PROPTXTSTATUS_SETPKTWAITING(s, waiting)	(s)->status = ((s)->status & \
		~SCB2_PROPTXTSTATUS_PKTWAITING_MASK) | \
		(((waiting) << SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT) & \
		SCB2_PROPTXTSTATUS_PKTWAITING_MASK)
#define SCB_PROPTXTSTATUS_SETPOLLRETRY(s, retry)	(s)->status = ((s)->status & \
		~SCB2_PROPTXTSTATUS_POLLRETRY_MASK) | \
		(((retry) << SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT) & \
		SCB2_PROPTXTSTATUS_POLLRETRY_MASK)
#define SCB_PROPTXTSTATUS_SETTIM(s, tim)		(s)->status = ((s)->status & \
		~SCB2_PROPTXTSTATUS_TIM_MASK) | \
		(((tim) << SCB2_PROPTXTSTATUS_TIM_SHIFT) & \
		SCB2_PROPTXTSTATUS_TIM_MASK)

/* local prototype declarations */
static bool wlc_apps_pvb_update_from_host(wlc_info_t *wlc, struct scb *scb, bool op);
#ifdef PROP_TXSTATUS_DEBUG
static void wlfc_hostpkt_callback(wlc_info_t *wlc, void *pkt, uint txs);
#endif // endif
#ifdef BCMPCIEDEV
static int wlfc_push_signal_bus_data(wlc_wlfc_info_t *wlfc, uint8 *data, uint8 len);
#ifndef BCM_DHDHDR
static void wlfc_push_pkt_txstatus(wlc_wlfc_info_t *wlfc, void *pkt, void *txs, uint32 sz);
#endif // endif
#endif /* BCMPCIEDEV */
static int wlfc_send_signal_data(wlc_wlfc_info_t *wlfc, bool hold);
static void wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 handle, uint8 ta);
static void wlfc_reset_credittohost(wlc_wlfc_info_t *wlfc);

static int16 wlc_flow_ring_get_maxpkts_for_link(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *da);
/* ***** from wl_rte.c ***** */
/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static uint8
wlfc_allocate_MAC_descriptor_handle(wlc_wlfc_info_t *wlfc)
{
	int i;

	for (i = 0; i < NBITS(uint32); i++) {
		if (!(wlfc->bitmap & (1 << i))) {
			wlfc->bitmap |= 1 << i;
			/* we would use 3 bits only */
			wlfc->replay_counter++;
			/* ensure a non-zero replay counter value */
			if (!(wlfc->replay_counter & 7))
				wlfc->replay_counter = 1;
			return i | (wlfc->replay_counter << 5);
		}
	}
	return WLFC_MAC_DESC_ID_INVALID;
}

static void
wlfc_release_MAC_descriptor_handle(wlc_wlfc_info_t *wlfc, uint8 handle)
{

	if (handle < WLFC_MAC_DESC_ID_INVALID) {
		/* unset the allocation flag in bitmap */
		wlfc->bitmap &= ~(1 << WLFC_MAC_DESC_GET_LOOKUP_INDEX(handle));
	}
	return;
}

void
wlc_send_credit_map(wlc_info_t *wlc)
{
	if (PROP_TXSTATUS_ENAB(wlc->pub) && HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
		wlc_wlfc_info_t *wlfc = wlc->wlfc;
		int new_total_credit = 0;

		if (POOL_ENAB(wlc->pub->pktpool)) {
			new_total_credit = pktpool_tot_pkts(wlc->pub->pktpool);
		}

		if ((wlfc->total_credit > 0) && (new_total_credit > 0) &&
			(new_total_credit != wlfc->total_credit)) {
			/* re-allocate new total credit among ACs */
			wlfc->fifo_credit[TX_AC_BK_FIFO] =
				wlfc->fifo_credit[TX_AC_BK_FIFO] * new_total_credit /
				wlfc->total_credit;
			wlfc->fifo_credit[TX_AC_VI_FIFO] =
				wlfc->fifo_credit[TX_AC_VI_FIFO] * new_total_credit /
				wlfc->total_credit;
			wlfc->fifo_credit[TX_AC_VO_FIFO] =
				wlfc->fifo_credit[TX_AC_VO_FIFO] * new_total_credit /
				wlfc->total_credit;
			wlfc->fifo_credit[TX_BCMC_FIFO] =
				wlfc->fifo_credit[TX_BCMC_FIFO] * new_total_credit /
				wlfc->total_credit;
			/* give all remainig credits to BE */
			wlfc->fifo_credit[TX_AC_BE_FIFO] = new_total_credit -
				wlfc->fifo_credit[TX_AC_BK_FIFO] -
				wlfc->fifo_credit[TX_AC_VI_FIFO] -
				wlfc->fifo_credit[TX_AC_VO_FIFO] -
				wlfc->fifo_credit[TX_BCMC_FIFO];

			/* recaculate total credit from actual pool size */
			wlfc->total_credit = new_total_credit;
		}

		if (wlfc->totalcredittohost != wlfc->total_credit) {
			wlc_mac_event(wlc, WLC_E_FIFO_CREDIT_MAP, NULL, 0, 0, 0,
				wlfc->fifo_credit, sizeof(wlfc->fifo_credit));
			wlc_mac_event(wlc, WLC_E_BCMC_CREDIT_SUPPORT, NULL, 0, 0, 0, NULL, 0);

			wlfc->totalcredittohost = wlfc->total_credit;
		}
	}
}

static void
wlfc_reset_credittohost(wlc_wlfc_info_t *wlfc)
{
	wlfc->totalcredittohost = 0;
}

#ifdef PROP_TXSTATUS_DEBUG
void
wlc_wlfc_info_dump(wlc_info_t *wlc)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	int i;

	printf("packets: (from_host-nost-sig,status_back,stats_other,credit_back,creditin) = "
		"(%d-%d-%d,%d,%d,%d,%d)\n",
		wlfc->stats.packets_from_host,
		wlfc->dbgstats->nost_from_host,
		wlfc->dbgstats->sig_from_host,
		wlfc->stats.txstatus_count,
		wlfc->stats.txstats_other,
		wlfc->dbgstats->creditupdates,
		wlfc->dbgstats->creditin);
	printf("credits for fifo: fifo[0-5] = (");
	for (i = 0; i < NFIFO_LEGACY; i++)
		printf("%d,", wlfc->dbgstats->credits[i]);
	printf(")\n");
	printf("stats: (header_only_alloc, realloc_in_sendup): (%d,%d)\n",
		wlfc->dbgstats->nullpktallocated,
		wlfc->dbgstats->realloc_in_sendup);
	printf("wlc_toss, wlc_sup = (%d, %d)\n",
		wlfc->dbgstats->wlfc_wlfc_toss,
		wlfc->dbgstats->wlfc_wlfc_sup);
	printf("debug counts:for_D11,free_exceptions:(%d,%d)\n",
		(wlfc->stats.packets_from_host -
	         (wlfc->dbgstats->wlfc_wlfc_toss + wlfc->dbgstats->wlfc_wlfc_sup) +
	         wlfc->stats.txstats_other),
		wlfc->dbgstats->wlfc_pktfree_except);
	return;
}
#endif /* PROP_TXSTATUS_DEBUG */

static void
wlfc_sendup_timer(void *arg)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)arg;

	wlfc->timer_started = 0;
	wlfc_sendup_ctl_info_now(wlfc);
}

#ifdef PROP_TXSTATUS_DEBUG
static void
wlfc_hostpkt_callback(wlc_info_t *wlc, void *p, uint txstatus)
{
	wlc_pkttag_t *pkttag;

	ASSERT(p != NULL);
	pkttag = WLPKTTAG(p);
	if (WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST) {
		wlc->wlfc->dbgstats->wlfc_pktfree_except++;
	}
}
#endif /* PROP_TXSTATUS_DEBUG */

/** generate INTERFACE_OPEN/INTERFACE_CLOSE signal */
static int
wlfc_MAC_table_update(wlc_wlfc_info_t *wlfc, uint8 *ea,
	wlfc_ctl_type_t add_del, uint8 mac_handle, uint8 ifidx)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_MACDESC];

	results[0] = add_del;
	results[1] = WLFC_CTL_VALUE_LEN_MACDESC;
	results[2] = mac_handle;
	results[3] = ifidx;
	memcpy(&results[4], ea, ETHER_ADDR_LEN);

	return wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
}

/** generate FLAG inside the flowing update signal */
static int
wlfc_flags_update(wlc_wlfc_info_t *wlfc, uint8 *ea,
	wlfc_ctl_type_t update, uint8 flags)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_FLAGS];

	results[0] = update;
	results[1] = WLFC_CTL_VALUE_LEN_FLAGS;
	results[2] = flags;
	memcpy(&results[3], ea, ETHER_ADDR_LEN);

	return wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
}

/** generate REQUEST_CREDIT/REQUEST_PACKET signal and flush all pending signals to host */
static int
wlfc_psmode_request(wlc_wlfc_info_t *wlfc, uint8 mac_handle, uint8 count,
	uint8 ac_bitmap, wlfc_ctl_type_t request_type)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_REQUEST_CREDIT];
	int ret;

	results[0] = request_type;
	if (request_type == WLFC_CTL_TYPE_MAC_REQUEST_PACKET)
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_PACKET;
	else
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_CREDIT;
	results[2] = count;
	results[3] = mac_handle;
	results[4] = ac_bitmap;

	ret = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
	if (ret == BCME_OK) {
		ret = wlfc_sendup_ctl_info_now(wlfc);
	}

	return ret;
}

/** flush pending signals to host */
int
wlfc_sendup_ctl_info_now(wlc_wlfc_info_t *wlfc)
{
	wlc_info_t *wlc = wlfc->wlc;
	/*
	typical overhead BCMDONGLEOVERHEAD,
	but aggregated sd packets can take 2*BCMDONGLEOVERHEAD
	octets
	*/
	int header_overhead = BCMDONGLEOVERHEAD*3;
	struct lbuf *wlfc_pkt;

	if (!WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub))
		return BCME_OK;

	/* Two possible reasons for being here - pending data or pending credit */
	if (!wlfc->pending_datalen && !wlfc->fifo_credit_back_pending)
		return BCME_OK;

	/* if necessary reverve space for RPC header and pad bytes to avoid assert failure */
	if ((wlfc_pkt = PKTGET(wlc->osh, wlfc->pending_datalen +
		header_overhead, TRUE)) == NULL) {
		/* what can be done to deal with this?? */
		/* set flag and try later again? */
		WL_ERROR(("PKTGET pkt size %d failed\n", wlfc->pending_datalen));
		return BCME_NOMEM;
	}
	PKTPULL(wlc->osh, wlfc_pkt, wlfc->pending_datalen +
		header_overhead);
	PKTSETLEN(wlc->osh, wlfc_pkt, 0);
	PKTSETNODROP(wlc->osh, wlfc_pkt);
	PKTSETTYPEEVENT(wlc->osh, wlfc_pkt);
	wl_sendup(wlc->wl, NULL, wlfc_pkt, 1);
	if (wlfc->pending_datalen) {
		/* not sent by wl_sendup() due to memory issue? */
		WL_ERROR(("wl_sendup failed to send pending_datalen\n"));
		return BCME_NOMEM;
	}

#ifdef PROP_TXSTATUS_DEBUG
	wlfc->dbgstats->nullpktallocated++;
#endif // endif
	return BCME_OK;
} /* wlfc_sendup_ctl_info_now */

/** flush pending signals to host and start credit monitor */
int
wlfc_push_credit_data(wlc_wlfc_info_t *wlfc, void *p)
{
	wlc_info_t *wlc = wlfc->wlc;
	uint8 ac;
	uint32 threshold;

	ac = WL_TXSTATUS_GET_FIFO(WLPKTTAG(p)->wl_hdr_information);
	WLPKTTAG(p)->flags |= WLF_CREDITED;

#ifdef PROP_TXSTATUS_DEBUG
	wlfc->dbgstats->creditupdates++;

	ASSERT(ac < sizeof(wlfc->dbgstats->credits));
	wlfc->dbgstats->credits[ac]++;
#endif // endif

	ASSERT(ac < sizeof(wlfc->fifo_credit_back));
	wlfc->fifo_credit_back[ac]++;
	wlfc->fifo_credit_back_pending = 1;

	threshold = wlfc->fifo_credit_threshold[ac];

	if (wlfc->wlfc_trigger & WLFC_CREDIT_TRIGGER) {

		if (wlfc->fifo_credit_in[ac] > wlfc->fifo_credit[ac]) {
			/* borrow happened */
			threshold = wlfc->total_credit / wlfc->wlfc_fifo_bo_cr_ratio;
		}

		/*
		monitor how much credit is being gathered here. If credit pending is
		larger than a preset threshold, send_it_now(). The idea is to keep
		the host busy pushing packets to keep the pipeline filled.
		*/
		if (wlfc->fifo_credit_back[ac] >= threshold &&
		    wlfc_sendup_ctl_info_now(wlfc) != BCME_OK &&
		    !wlfc->timer_started) {
			wl_add_timer(wlc->wl, wlfc->fctimer, WLFC_SENDUP_TIMER_INTERVAL, 0);
			wlfc->timer_started = 1;
		}
	}

	return BCME_OK;
}

/** queue signal */
static int
wlfc_queue_signal_data(wlc_wlfc_info_t *wlfc, uint8 *data, uint8 len)
{
	wlfc_ctl_type_t type = data[0];
	bool skip_cp = FALSE;

	ASSERT((wlfc->pending_datalen + len) <= WLFC_MAX_PENDING_DATALEN);

	if ((wlfc->pending_datalen + len) > WLFC_MAX_PENDING_DATALEN) {
		WL_ERROR(("wlfc queue full: %d > %d\n",
			wlfc->pending_datalen + len,
			WLFC_MAX_PENDING_DATALEN));
		return BCME_ERROR;
	}

	if ((type == WLFC_CTL_TYPE_TXSTATUS) && wlfc->comp_stat_enab) {
		uint32 xstatusdata, statusdata = *((uint32 *)(data + TLV_HDR_LEN));
		uint8 xcnt, cnt = WL_TXSTATUS_GET_FREERUNCTR(statusdata);
		uint8 xac, ac = WL_TXSTATUS_GET_FIFO(statusdata);
		uint16 xhslot, hslot = WL_TXSTATUS_GET_HSLOT(statusdata);
		uint8 xstatus, status = WL_TXSTATUS_GET_STATUS(statusdata);
		uint8 cur_pos = 0;
		uint8 bBatched = 0;
		uint32 compcnt_offset = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS;

		uint16 xseq = 0, seq = 0;
		uint8 xseq_fromfw = 0, seq_fromfw = 0;
		uint16 xseq_num = 0, seq_num = 0;

		data[TLV_TAG_OFF] = WLFC_CTL_TYPE_COMP_TXSTATUS;
		data[TLV_LEN_OFF]++;

		if (WLFC_GET_REUSESEQ(wlfc->wlfc_mode)) {
				/* TRUE if d11 seq nums are to be reused */
			compcnt_offset += WLFC_CTL_VALUE_LEN_SEQ;
			seq = *((uint16 *)(data + TLV_HDR_LEN +
				WLFC_CTL_VALUE_LEN_TXSTATUS));
			seq_fromfw = GET_WL_HAS_ASSIGNED_SEQ(seq); /* TRUE or FALSE */
			seq_num = WL_SEQ_GET_NUM(seq);
		}

		while (cur_pos < wlfc->pending_datalen) {
			if ((wlfc->data[cur_pos] == WLFC_CTL_TYPE_COMP_TXSTATUS)) {
				xstatusdata = *((uint32 *)(wlfc->data + cur_pos +
					TLV_HDR_LEN));
				/* next expected free run counter */
				xcnt = (WL_TXSTATUS_GET_FREERUNCTR(xstatusdata) +
					wlfc->data[cur_pos + compcnt_offset]) &
					WL_TXSTATUS_FREERUNCTR_MASK;
				/* next expected fifo number */
				xac = WL_TXSTATUS_GET_FIFO(statusdata);
				/* next expected slot number */
				xhslot = WL_TXSTATUS_GET_HSLOT(xstatusdata);
				if (!WLFC_GET_AFQ(wlfc->wlfc_mode)) {
					/* for hanger, it needs to be consective */
					xhslot = (xhslot + wlfc->data[cur_pos +
						compcnt_offset]) & WL_TXSTATUS_HSLOT_MASK;
				}
				xstatus = WL_TXSTATUS_GET_STATUS(xstatusdata);

				if (WLFC_GET_REUSESEQ(wlfc->wlfc_mode)) {
					xseq = *((uint16 *)(wlfc->data + cur_pos +
						TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS));
					xseq_fromfw = GET_WL_HAS_ASSIGNED_SEQ(xseq);
					/* next expected seq num */
					xseq_num = (WL_SEQ_GET_NUM(xseq) + wlfc->data[
						cur_pos + compcnt_offset]) & WL_SEQ_NUM_MASK;
				}

				if ((cnt == xcnt) && (hslot == xhslot) &&
				    (status == xstatus) && (ac == xac) &&
				    (!WLFC_GET_REUSESEQ(wlfc->wlfc_mode) ||
				     ((seq_fromfw == xseq_fromfw) &&
				      (!seq_fromfw || (seq_num == xseq_num))))) {
					wlfc->data[cur_pos + compcnt_offset]++;
					bBatched = 1;
					wlfc->compressed_stat_cnt++;
					break;
				}
			}
			cur_pos += wlfc->data[cur_pos + TLV_LEN_OFF] + TLV_HDR_LEN;
		}

		if (!bBatched) {
			memcpy(&wlfc->data[wlfc->pending_datalen], data, len);
			wlfc->data[wlfc->pending_datalen + len] = 1;
			wlfc->pending_datalen += len + 1;
		}

		skip_cp = TRUE;
	}

	if (!skip_cp) {
		memcpy(&wlfc->data[wlfc->pending_datalen], data, len);
		wlfc->pending_datalen += len;
	}

	return BCME_OK;
} /* wlfc_queue_signal_data */

/**
 * Called by various wlc_*.c files to signal wlfc events (e.g. MAC_OPEN or
 * MAC_CLOSE) that are ultimately consumed by the host (USB) or
 * by the firmware bus layer (PCIe).
 *     @param [in] data : a single TLV
 *     @param [in] len  : length of TLV in bytes, including 'TL' fields.
 */
int
wlfc_push_signal_data(wlc_wlfc_info_t *wlfc, uint8 *data, uint8 len, bool hold)
{
	wlc_info_t *wlc = wlfc->wlc;
	int rc = BCME_OK;
	uint8 type = data[0]; /* tlv type */
	uint8 tlv_flag;		/* how wlfc between host and fw is configured */
	uint32 tlv_mask;	/* to prevent certain TLV types from reaching the host */

	tlv_flag = wlc->wlfc_flags;

	tlv_mask = ((tlv_flag & WLFC_FLAGS_XONXOFF_SIGNALS) ?
		WLFC_FLAGS_XONXOFF_MASK : 0) |
		((WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) &&
		(tlv_flag & WLFC_FLAGS_CREDIT_STATUS_SIGNALS)) ?
		WLFC_FLAGS_CREDIT_STATUS_MASK : 0) |
		0;

#ifdef BCMPCIEDEV
	if (WLFC_INFO_TO_BUS_ENAB(wlc->pub)) {
		 rc = wlfc_push_signal_bus_data(wlfc, data, len);
	} else
#endif /* BCMPCIEDEV */
	{
		/* if the host does not want these TLV signals, drop it */
		if (!(tlv_mask & (1 << type))) {
			WLFC_DBGMESG(("%s() Dropping signal, type:%d, mask:%08x, flag:%d\n",
				__FUNCTION__, type, tlv_mask, tlv_flag));
			return BCME_OK;
		}

		if ((wlfc->pending_datalen + len) > WLFC_MAX_PENDING_DATALEN) {
			if (BCME_OK != (rc = wlfc_sendup_ctl_info_now(wlfc))) {
				/* at least the caller knows we have failed */
				WL_ERROR(("%s() Dropping %d bytes data\n", __FUNCTION__, len));
				return rc;
			}
		}

		wlfc_queue_signal_data(wlfc, data, len);
		if (!wlfc->comp_stat_enab)
			hold = FALSE;

		rc = wlfc_send_signal_data(wlfc, hold);
	}

	return rc;
} /* wlfc_push_signal_data */

/** PROP_TXSTATUS specific */
static int
wlfc_send_signal_data(wlc_wlfc_info_t *wlfc, bool hold)
{
	wlc_info_t *wlc = wlfc->wlc;
	int rc = BCME_OK;

	if ((wlfc->pending_datalen > wlfc->pending_datathresh) ||
	    (!hold && (wlfc->wlfc_trigger & WLFC_TXSTATUS_TRIGGER) &&
	     (wlfc->compressed_stat_cnt > wlfc->wlfc_comp_txstatus_thresh))) {
		rc = wlfc_sendup_ctl_info_now(wlfc);
		if (rc == BCME_OK)
			return BCME_OK;
	}

	if (!wlfc->timer_started) {
		wl_add_timer(wlc->wl, wlfc->fctimer, WLFC_SENDUP_TIMER_INTERVAL, 0);
		wlfc->timer_started = 1;
	}
	return rc;
}

/** Called when the d11 core signals completion on a transmit */
void
wlfc_process_txstatus(wlc_wlfc_info_t *wlfc, uint8 status_flag, void *p,
	void *ptxs, bool hold)
{
	wlc_info_t *wlc = wlfc->wlc;
	/* space for type(1), length(1) and value */
	uint8	results[TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS + WLFC_CTL_VALUE_LEN_SEQ];
	uint32  statussize = 0, statusdata = 0;
	uint32	wlhinfo;
	bool	amsdu_pkt;
	uint16	amsdu_seq = 0;
	uint16	seq;
	bool metadatabuf_avail = TRUE;
	bool short_status = TRUE;
#ifdef BCM_DHDHDR
	BCM_REFERENCE(short_status);
#endif /* BCM_DHDHDR */

	amsdu_pkt = WLPKTFLAG_AMSDU(WLPKTTAG(p));
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB() && amsdu_pkt) {
		if (WLFC_GET_REUSESEQ(wlfc_query_mode(wlfc))) {
			/*
			 * Use same seq number for all pkts in AMSDU for suppression so we can
			 * re-aggregate them.
			 */
			amsdu_seq = WLPKTTAG(p)->seq;
			if (GET_WL_HAS_ASSIGNED_SEQ(amsdu_seq))
				WL_SEQ_SET_AMSDU(amsdu_seq, 1);
		} else
			amsdu_seq = 0;
	}
#endif /* BCMPCIEDEV */

	if (!TXMETADATA_TO_HOST_ENAB(wlc->pub))
		metadatabuf_avail = FALSE;
#ifdef BCMPCIEDEV
	else if (BCMPCIEDEV_ENAB())
		/* No frag metadata support */
		metadatabuf_avail = FALSE;
#endif /* BCMPCIEDEV */

	do {
		wlhinfo = WLPKTTAG(p)->wl_hdr_information;
		statusdata = 0;
		BCM_REFERENCE(statusdata);

		/* send txstatus only if this packet came from the host */
		if (!(WL_TXSTATUS_GET_FLAGS(wlhinfo) & WLFC_PKTFLAG_PKTFROMHOST)) {
			WLFC_COUNTER_TXSTATUS_OTHER(wlfc);
			continue;
		}

		WL_TXSTATUS_SET_FLAGS(WLPKTTAG(p)->wl_hdr_information,
			WL_TXSTATUS_GET_FLAGS(wlhinfo) | WLFC_PKTFLAG_PKT_SENDTOHOST);

		if ((WLPKTTAG(p)->flags & WLF_PROPTX_PROCESSED) &&
		    (status_flag != WLFC_CTL_PKTFLAG_SUPPRESS_ACKED)) {
			continue;
		}
		WLPKTTAG(p)->flags |= WLF_PROPTX_PROCESSED;
		if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) &&
			!(WL_TXSTATUS_GET_FLAGS(wlhinfo) & WLFC_PKTFLAG_PKT_REQUESTED)) {
			/* if this packet was intended for AC FIFO and ac credit
			 * has not been sent back, push a credit here
			 */
			if (!(WLPKTTAG(p)->flags & WLF_CREDITED)) {
				wlfc_push_credit_data(wlfc, p);
			}
		}

		/* When BCM_DHDHDR enabled, save txs and seq info to
		 * LFRAG_META, because we may not have the
		 * data buffer for the packet.
		 */
#ifdef BCM_DHDHDR
		ASSERT(!metadatabuf_avail);

		/* Save txstatus */
		PKTFRAGSETTXSTATUS(wlc->osh, p, status_flag);
#endif /* BCM_DHDHDR */

		if (metadatabuf_avail) {
			results[TLV_TAG_OFF] = WLFC_CTL_TYPE_TXSTATUS;
			results[TLV_LEN_OFF] = WLFC_CTL_VALUE_LEN_TXSTATUS;

			WL_TXSTATUS_SET_PKTID(statusdata, WL_TXSTATUS_GET_PKTID(wlhinfo));
			WL_TXSTATUS_SET_FIFO(statusdata, WL_TXSTATUS_GET_FIFO(wlhinfo));
			WL_TXSTATUS_SET_FLAGS(statusdata, status_flag);
			WL_TXSTATUS_SET_GENERATION(statusdata,
				WL_TXSTATUS_GET_GENERATION(wlhinfo));
			memcpy(&results[TLV_BODY_OFF + statussize],
				&statusdata, sizeof(uint32));
			statussize += sizeof(uint32);
			short_status = FALSE;
		} else if (!status_flag)
			goto send_shortstatus;
		/* temporarily not return original sequence number
		 * for amsdu packet
		 * XXX: Why? Was this done for SDIO specifically?
		 */
		seq = amsdu_pkt ? amsdu_seq : WLPKTTAG(p)->seq;

		if ((metadatabuf_avail || WLFC_INFO_TO_BUS_ENAB(wlc->pub)) &&
			WLFC_GET_REUSESEQ(wlfc_query_mode(wlfc))) {
			if ((status_flag == WLFC_CTL_PKTFLAG_D11SUPPRESS) ||
			    (status_flag == WLFC_CTL_PKTFLAG_WLSUPPRESS)) {
				if (GET_DRV_HAS_ASSIGNED_SEQ(seq)) {
					SET_WL_HAS_ASSIGNED_SEQ(seq);
					RESET_DRV_HAS_ASSIGNED_SEQ(seq);
				}
#ifndef BCM_DHDHDR /* We did above to save txstatus already */
				if (WLFC_INFO_TO_BUS_ENAB(wlc->pub) && !metadatabuf_avail) {
					results[TLV_TAG_OFF] = WLFC_CTL_TYPE_TXSTATUS;
					results[TLV_LEN_OFF] = WLFC_CTL_VALUE_LEN_TXSTATUS;
					WL_TXSTATUS_SET_FLAGS(statusdata, status_flag);
					memcpy(&results[TLV_BODY_OFF], &statusdata, sizeof(uint32));
					short_status = FALSE;
				}
#endif /* !BCM_DHDHDR */
			} else {
				seq = 0;
			}
#ifdef BCM_DHDHDR
			/* Updata seq */
			PKTSETWLFCSEQ(wlc->osh, p, seq);
#else /* BCM_DHDHDR */
			/* If we could not populate txstatus, but need to
			* update, then do it now
			*/
			if (metadatabuf_avail || (seq && WLFC_INFO_TO_BUS_ENAB(wlc->pub))) {
				memcpy(&results[TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS],
					&seq, WLFC_CTL_VALUE_LEN_SEQ);
				results[TLV_LEN_OFF] += WLFC_CTL_VALUE_LEN_SEQ;
				statussize = results[TLV_LEN_OFF];
			}
#endif /* BCM_DHDHDR */
		}
		statussize += TLV_HDR_LEN;
send_shortstatus:
#ifndef BCM_DHDHDR
		if (short_status) {
			results[0] = status_flag;
			statussize = 1;
		}
#endif /* !BCM_DHDHDR */
#ifdef BCMPCIEDEV
		if (!WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub)) {
#ifdef BCM_DHDHDR
			/* Set state to TXstatus processed */
			PKTSETTXSPROCESSED(wlc->osh, p);
#else
			wlfc_push_pkt_txstatus(wlfc, p, results, statussize);
#endif /* BCM_DHDHDR */
		} else
#endif /* BCMPCIEDEV  */
		{
			wlfc_push_signal_data(wlfc, results, statussize, hold);

		}

		/* Dont clear, we use hdrinfo  even after posting status
		 * The new WLF_PROPTX_PROCESSED flag prevents duplicate posting
		 * WLPKTTAG(p)->wl_hdr_information = 0;
		 */
		if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) &&
			(status_flag == WLFC_CTL_PKTFLAG_D11SUPPRESS)) {
			wlfc_sendup_ctl_info_now(wlfc);
		}
		WLFC_COUNTER_TXSTATUS_COUNT(wlfc);
	} while (amsdu_pkt && ((p = PKTNEXT(wlc->osh, p)) != NULL));

	return;
} /* wlfc_process_wlhdr_complete_txstatus */

int
wlc_sendup_txstatus(wlc_info_t *wlc, void **pp)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	uint8* wlfchp;
	uint8 required_headroom;
	uint8 wl_hdr_words = 0;
	uint8 fillers = 0;
	uint8 rssi_space = 0, tstamp_space = 0;
	uint8 seqnumber_space = 0;
	uint8 fcr_tlv_space = 0;
	uint8 ampdu_reorder_info_space = 0;
	void *p = *pp;
	uint32 datalen, datalen_min;

	wlfc->compressed_stat_cnt = 0;

	/* For DATA packets: plugin a RSSI value that belongs to this packet.
	   RSSI TLV = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_RSSI
	 */
	if (!PKTTYPEEVENT(wlc->osh, p)) {
		/* is the RSSI TLV reporting enabled? */
		if ((RXMETADATA_TO_HOST_ENAB(wlc->pub)) &&
			(wlc->wlfc_flags & WLFC_FLAGS_RSSI_SIGNALS)) {
			rssi_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_RSSI;
		}
#ifdef WLAMPDU_HOSTREORDER
		/* check if the host reordering info needs to be added from pkttag */
		if (AMPDU_HOST_REORDER_ENAB(wlc->pub)) {
			wlc_pkttag_t *pkttag;
			pkttag = WLPKTTAG(p);
			if (pkttag->flags2 & WLF2_HOSTREORDERAMPDU_INFO) {
				ampdu_reorder_info_space = WLHOST_REORDERDATA_LEN + TLV_HDR_LEN;
			}
		}
#endif /* WLAMPDU_HOSTREORDER */
	 }

#ifdef WLFCHOST_TRANSACTION_ID
	seqnumber_space = TLV_HDR_LEN + WLFC_TYPE_TRANS_ID_LEN;
#endif // endif

	if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) && wlfc->fifo_credit_back_pending) {
		fcr_tlv_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
	}

	datalen_min = rssi_space + tstamp_space
		+ ampdu_reorder_info_space + seqnumber_space;

	datalen = wlfc->pending_datalen + fcr_tlv_space + datalen_min;
	fillers = ROUNDUP(datalen, 4) - datalen;
	required_headroom = datalen + fillers;
	wl_hdr_words = required_headroom >> 2;

	if (PKTHEADROOM(wlc->osh, p) < required_headroom) {
		void *p1;
		int plen = PKTLEN(wlc->osh, p);

		/* Allocate a packet that will fit all the data */
		if ((p1 = PKTGET(wlc->osh, (plen + required_headroom), TRUE)) == NULL) {
			WL_ERROR(("PKTGET pkt size %d failed\n", plen));

			/* There should still be enough room for datalen_min */
			datalen = datalen_min;
			fillers = ROUNDUP(datalen, 4) - datalen;
			required_headroom = datalen + fillers;
			ASSERT(PKTHEADROOM(wlc->osh, p) >= required_headroom);
			if (PKTHEADROOM(wlc->osh, p) < required_headroom) {
				PKTFREE(wlc->osh, p, TRUE);
				return TRUE;
			}

			wl_hdr_words = required_headroom >> 2;
			PKTPUSH(wlc->osh, p, required_headroom);
		} else {
			/* Transfer other fields */
			PKTSETPRIO(p1, PKTPRIO(p));
			PKTSETSUMGOOD(p1, PKTSUMGOOD(p));
			bcopy(PKTDATA(wlc->osh, p),
				(PKTDATA(wlc->osh, p1) + required_headroom), plen);
			wlc_pkttag_info_move(wlc, p, p1);
			PKTFREE(wlc->osh, p, TRUE);
			p = p1;
			*pp = p1;
#ifdef PROP_TXSTATUS_DEBUG
			wlfc->dbgstats->realloc_in_sendup++;
#endif // endif
		}
	} else
		PKTPUSH(wlc->osh, p, required_headroom);

	wlfchp = PKTDATA(wlc->osh, p);

#ifdef WLFCHOST_TRANSACTION_ID
	if (seqnumber_space) {
		uint32 timestamp;

		/* byte 0: ver, byte 1: seqnumber, byte2:byte6 timestamps */
		wlfchp[0] = WLFC_CTL_TYPE_TRANS_ID;
		wlfchp[1] = WLFC_TYPE_TRANS_ID_LEN;
		wlfchp += TLV_HDR_LEN;

		wlfchp[0] = 0;
		/* wrap around is fine */
		wlfchp[1] = wlfc->txseqtohost++;

		/* time stamp of the packet */
		timestamp = hnd_get_reftime_ms();
		bcopy(&timestamp, &wlfchp[2], sizeof(uint32));

		wlfchp += WLFC_TYPE_TRANS_ID_LEN;
	}
#endif /* WLFCHOST_TRANSACTION_ID */

#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENAB(wlc->pub) && ampdu_reorder_info_space) {

		wlc_pkttag_t *pkttag = WLPKTTAG(p);
		PKTSETNODROP(wlc->osh, p);

		wlfchp[0] = WLFC_CTL_TYPE_HOST_REORDER_RXPKTS;
		wlfchp[1] = WLHOST_REORDERDATA_LEN;
		wlfchp += TLV_HDR_LEN;

		/* zero out the tag value */
		bzero(wlfchp, WLHOST_REORDERDATA_LEN);

		wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET] =
			pkttag->u.ampdu_info_to_host.ampdu_flow_id;
		wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET] =
			AMPDU_BA_MAX_WSIZE -  1;		/* 0 based...so -1 */
		wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET] =
			pkttag->u.ampdu_info_to_host.flags;
		wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET] =
			pkttag->u.ampdu_info_to_host.cur_idx;
		wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET] =
			pkttag->u.ampdu_info_to_host.exp_idx;

		WL_INFORM(("flow:%d idx(%d, %d, %d), flags 0x%02x\n",
			wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET],
			wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET]));
		wlfchp += WLHOST_REORDERDATA_LEN;
	}
#endif /* WLAMPDU_HOSTREORDER */

	if ((RXMETADATA_TO_HOST_ENAB(wlc->pub)) && rssi_space) {
		wlfchp[0] = WLFC_CTL_TYPE_RSSI;
		wlfchp[1] = rssi_space - TLV_HDR_LEN;
		wlfchp[2] = ((wlc_pkttag_t*)WLPKTTAG(p))->pktinfo.misc.rssi;
		wlfchp += rssi_space;
	}

	if (datalen > datalen_min) {
		/* this packet is carrying signals */
		PKTSETNODROP(wlc->osh, p);

		if (wlfc->pending_datalen) {
			memcpy(&wlfchp[0], wlfc->data, wlfc->pending_datalen);
			wlfchp += wlfc->pending_datalen;
			wlfc->pending_datalen = 0;
		}

		/* if there're any fifo credit pending, append it (after pending data) */
		if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) && fcr_tlv_space) {
			int i = 0;
			wlfchp[0] = WLFC_CTL_TYPE_FIFO_CREDITBACK;
			wlfchp[1] = WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
			memcpy(&wlfchp[2], wlfc->fifo_credit_back,
				WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK);

			for (i = 0; i < WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK; i++) {
				if (wlfc->fifo_credit_back[i]) {
					wlfc->fifo_credit_in[i] -=
						wlfc->fifo_credit_back[i];
					wlfc->fifo_credit_back[i] = 0;
				}
			}
			wlfc->fifo_credit_back_pending = 0;
				wlfchp += TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
		}
	}

	if (fillers) {
		memset(&wlfchp[0], WLFC_CTL_TYPE_FILLER, fillers);
	}

	PKTSETDATAOFFSET(p, wl_hdr_words);

	if (wlfc->timer_started) {
		/* cancel timer */
		wl_del_timer(wlc->wl, wlfc->fctimer);
		wlfc->timer_started = 0;
	}
	return FALSE;
} /* wlc_sendup_txstatus */

uint32
wlfc_query_mode(wlc_wlfc_info_t *wlfc)
{
	return (uint32)(wlfc->wlfc_mode);
}

void wlfc_enable_cred_borrow(wlc_info_t *wlc, uint32 bEnable)
{
	wlc_mac_event(wlc, WLC_E_ALLOW_CREDIT_BORROW, NULL, 0, 0, 0,
		&bEnable, sizeof(bEnable));
}

/** PROP_TXSTATUS specific function, called by wl_send(). */
int
wlc_send_txstatus(wlc_info_t *wlc, void *p)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	uint8* wlhdrtodev = NULL;
	wlc_pkttag_t *wlpkttag;
	uint8 wlhdrlen;
	uint8 processed = 0;
	uint32 wl_hdr_information = 0;
	uint16 seq = 0;
#ifdef BCM_DHDHDR
	uint32 pkt_flags = 0, flags = 0;
	uint8 buf[ROUNDUP((TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_PKTTAG + WLFC_CTL_VALUE_LEN_SEQ),
		sizeof(uint32))];
#ifdef WL_REUSE_KEY_SEQ
	uint8 *key_seq;
	int j;
#endif // endif
	if (PKTFRAGTXSTATUS(wlc->osh, p) == WLFC_CTL_TYPE_PKTTAG) {
		buf[TLV_TAG_OFF] = WLFC_CTL_TYPE_PKTTAG;
		buf[TLV_LEN_OFF] = WLFC_CTL_VALUE_LEN_PKTTAG + WLFC_CTL_VALUE_LEN_SEQ;
		flags = PKTFRAGPKTFLAGS(wlc->osh, p);
		WL_TXSTATUS_SET_FLAGS(pkt_flags, flags);
		memcpy(&buf[TLV_HDR_LEN], &pkt_flags, sizeof(uint32));
		seq = PKTWLFCSEQ(wlc->osh, p);
		memcpy(&buf[TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_PKTTAG],
			&seq, sizeof(uint16));
		seq = 0;
		wlhdrtodev = &buf[0];
		PKTFRAGSETTXSTATUS(wlc->osh, p, 0);
		PKTSETWLFCSEQ(wlc->osh, p, 0);
		PKTFRAGRESETPKTFLAGS(wlc->osh, p);
	}
#endif /* BCM_DHDHDR */
	wlhdrlen = PKTDATAOFFSET(p) << 2;
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		/* We do not expect host to set BDC and wl header on PCIEDEV path, So set it now */
		WL_TXSTATUS_SET_FLAGS(wl_hdr_information, WLFC_PKTFLAG_PKTFROMHOST);
	}
#endif /* BCMPCIEDEV */

	if (wlhdrlen != 0) {
#ifdef BCM_DHDHDR
		if (wlhdrtodev == NULL) {
			wlhdrtodev = PKTFRAGFCTLV(wlc->osh, p);
		}
#else
		wlhdrtodev = (uint8*)PKTDATA(wlc->osh, p);
#endif /* BCM_DHDHDR */

		while (processed < wlhdrlen) {
			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_PKTTAG) {
				wl_hdr_information |=
					ltoh32_ua(&wlhdrtodev[processed + TLV_HDR_LEN]);

				if (WLFC_GET_REUSESEQ(wlfc->wlfc_mode)) {
					uint16 reuseseq = ltoh16_ua(&wlhdrtodev[processed +
						TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_TXSTATUS]);
					if (GET_DRV_HAS_ASSIGNED_SEQ(reuseseq)) {
						seq = reuseseq;
#ifdef WL_REUSE_KEY_SEQ
						key_seq = PKTFRAGFCTLV(wlc->osh, p);
						WL_INFORM(("%s reuseseq %x pktlen %d \n",
							__FUNCTION__, WL_SEQ_GET_NUM(seq),
							PKTLEN(wlc->osh, p)));
						if (GET_DRV_HAS_ASSIGNED_KEY_SEQ(key_seq)) {
							WL_INFORM(("%s reuseseq %x key_seq => ",
								__FUNCTION__, WL_SEQ_GET_NUM(seq)));
							for (j = WL_KEY_SEQ_SIZE; j > 0; --j) {
								WL_INFORM(("%02x", key_seq[j - 1]));
							}
							WL_INFORM(("\n"));
						}
#endif // endif
					}
				}

				if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) &&
					!(WL_TXSTATUS_GET_FLAGS(wl_hdr_information) &
					WLFC_PKTFLAG_PKT_REQUESTED)) {
						uint8 ac = WL_TXSTATUS_GET_FIFO
							(wl_hdr_information);
						wlfc->fifo_credit_in[ac]++;
				}
			} else if (wlhdrtodev[processed] == WLFC_CTL_TYPE_PENDING_TRAFFIC_BMP) {
				wlc_scb_update_available_traffic_info(wlc,
					wlhdrtodev[processed+2], wlhdrtodev[processed+3]);
			}

			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_FILLER) {
				/* skip ahead - 1 */
				processed += 1;
			} else {
				/* skip ahead - type[1], len[1], value_len */
				processed += TLV_HDR_LEN + wlhdrtodev[processed + TLV_LEN_OFF];
			}
		}
#ifdef BCM_DHDHDR
		if (wlhdrtodev == PKTFRAGFCTLV(wlc->osh, p)) {
			bzero(PKTFRAGFCTLV(wlc->osh, p), 8);
		}
#else
		PKTPULL(wlc->osh, p, wlhdrlen);
#endif /* BCM_DHDHDR */
		/* Reset DataOffset to 0, since we have consumed the wlhdr */
		PKTSETDATAOFFSET(p, 0);
	} else {
		/* wlhdrlen == 0 */
		if (!BCMPCIEDEV_ENAB()) {
			WL_INFORM(("No pkttag from host.\n"));
		}
	}

	/* update pkttag */
	wlpkttag = WLPKTTAG(p);
	wlpkttag->wl_hdr_information = wl_hdr_information;
	wlpkttag->seq = seq;

	if (wlfc != NULL) {
		wlfc->stats.packets_from_host++;
	}

	if (PKTLEN(wlc->osh, p) == 0) {
		/* a signal-only packet from host */
#ifdef PROP_TXSTATUS_DEBUG
		wlfc->dbgstats->sig_from_host++;
#endif // endif
		ASSERT(0);
		PKTFREE(wlc->osh, p, TRUE);
		return TRUE;
	}

#ifdef PROP_TXSTATUS_DEBUG
	if ((WL_TXSTATUS_GET_FLAGS(wlpkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST) &&
	    (!(WL_TXSTATUS_GET_FLAGS(wlpkttag->wl_hdr_information) & WLFC_PKTFLAG_PKT_REQUESTED))) {
		wlfc->dbgstats->creditin++;
	} else {
		wlfc->dbgstats->nost_from_host++;
	}
	WLF2_PCB1_REG(p, WLF2_PCB1_WLFC);
#endif /* PROP_TXSTATUS_DEBUG */
	return FALSE;
} /* wlc_send_txstatus */

#if defined(BCMPCIEDEV)
/**
 * PROP_TXSTATUS && BCMPCIEDEV specific function. Called when the WL layer wants to report a flow
 * control related event (e.g. MAC_OPEN), this function will lead the event towards a higher
 * firmware layer that consumes the event.
 */
static int
wlfc_push_signal_bus_data(wlc_wlfc_info_t *wlfc, uint8 *data, uint8 len)
{
	wlc_info_t *wlc = wlfc->wlc;
	wlfc_ctl_type_t type;
	flowring_op_data_t op_data;

	ASSERT(data && (len >= BCM_TLV_SIZE((bcm_tlv_t *)data)));
	type = data[0];
	bzero(&op_data, sizeof(flowring_op_data_t));

	switch (type) {
		case WLFC_CTL_TYPE_MAC_OPEN:
		case WLFC_CTL_TYPE_MAC_CLOSE:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_MAC_FLAGS);
			op_data.handle = data[2];
			op_data.flags = data[3];
			break;

		case WLFC_CTL_TYPE_MACDESC_ADD:
		case WLFC_CTL_TYPE_MACDESC_DEL:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_MACDESC);
			op_data.handle = data[2];
			op_data.ifindex = data[3];
			memcpy(op_data.addr, &data[4], ETHER_ADDR_LEN);
			break;

		case WLFC_CTL_TYPE_INTERFACE_OPEN:
		case WLFC_CTL_TYPE_INTERFACE_CLOSE:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_INTERFACE);
			op_data.ifindex = data[2];
			break;

		case WLFC_CTL_TYPE_TID_OPEN:
		case WLFC_CTL_TYPE_TID_CLOSE:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_MAC);
			op_data.tid = data[2];
			break;
		case WLFC_CTL_TYPE_MAC_REQUEST_PACKET:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_REQUEST_CREDIT ||
				data[1] == WLFC_CTL_VALUE_LEN_REQUEST_PACKET);
			op_data.handle = data[3];
			op_data.tid = data[4]; /* ac bit map */
			op_data.minpkts = data[2];
			break;

		case WLFC_CTL_TYPE_UPDATE_FLAGS:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_FLAGS);
			op_data.flags = data[2];
			memcpy(op_data.addr, &data[3], ETHER_ADDR_LEN);
			break;

		case WLFC_CTL_TYPE_CLEAR_SUPPR:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_SUPR);
			op_data.tid = data[2];
			memcpy(op_data.addr, &data[3], ETHER_ADDR_LEN);
			break;

		case WLFC_CTL_TYPE_FLOWID_OPEN:
		case WLFC_CTL_TYPE_FLOWID_CLOSE:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_FLOWID);
			op_data.flowid = data[2] | (((uint16)data[3]) << 8);
			break;

		case WLFC_CTL_TYPE_SQS_STRIDE:
			ASSERT(data[1] == WLFC_CTL_VALUE_LEN_MAC);
			op_data.handle = data[2];
			break;
		default :
			return BCME_ERROR;
	}
	wl_flowring_ctl(wlc->wl, type, (void *)&op_data);
	return BCME_OK;
} /* wlfc_push_signal_bus_data */

#ifndef BCM_DHDHDR
/**
 * PROP_TXSTATUS && BCMPCIEDEV specific function. Copies caller provided status array 'txs' into
 * caller provided packet 'p'.
 */
static void
wlfc_push_pkt_txstatus(wlc_wlfc_info_t *wlfc, void *p, void *txs, uint32 sz)
{
	wlc_info_t *wlc = wlfc->wlc;

	/* Set state to TXstatus processed */
	PKTSETTXSPROCESSED(wlc->osh, p);

	if (sz == 1) {
		*((uint8*)(PKTDATA(wlc->osh, p) + BCMPCIE_D2H_METADATA_HDRLEN)) =
			*((uint8*)txs);
		PKTSETLEN(wlc->osh, p, sz);
		return;
	}

	memcpy(PKTDATA(wlc->osh, p) + BCMPCIE_D2H_METADATA_HDRLEN, txs, sz);
	PKTSETLEN(wlc->osh, p, BCMPCIE_D2H_METADATA_HDRLEN + sz);
	PKTSETDATAOFFSET(p, ROUNDUP(BCMPCIE_D2H_METADATA_HDRLEN + sz, 4) >> 2);
}
#endif /* !BCM_DHDHDR */
#endif /* BCMPCIEDEV */

/* ***** from wlc_scb.c ***** */

#define SCBHANDLE_PS_STATE_MASK (1 << 8)
#define SCBHANDLE_INFORM_PKTPEND_MASK (1 << 9)

static void
wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle, uint8 ta_bmp)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlc->wlfc, scb);

		if (scbi->mac_address_handle &&
		    (scbi->mac_address_handle == mac_handle)) {
			/* Update only for AP, nonAWDL-IBSS, TDLS */
			if (BSSCFG_AP(scb->bsscfg) || BSS_TDLS_ENAB(wlc, scb->bsscfg) ||
			    BSSCFG_IBSS(scb->bsscfg)) {
				SCB_PROPTXTSTATUS_SETTIM(scbi, ta_bmp);
				wlc_apps_pvb_update_from_host(wlc, scb, FALSE);
			}
			break;
		}
	}
}

static bool
wlc_flow_ring_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle,
	uint8 tid, bool op)
{
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 ta_bmp;
	bool  ret = TRUE;
	uint8 ac;

#ifdef WLSQS
	/* SQS maintains per tid flow. But TIM is based on AC and
	 * only 4 bits are reserved in SCB2_PROPTXTSTATUS_TIM_MASK
	 * So convert tid to ac before setting the scb status
	 */
	ac = WME_PRIO2AC(tid);
#else
	ac = tid;
#endif /* WLSQS */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlc->wlfc, scb);

		if (scbi->mac_address_handle &&
		    (scbi->mac_address_handle == mac_handle)) {
			wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

			ta_bmp = SCB_PROPTXTSTATUS_TIM(scbi);
			ta_bmp = (ta_bmp & ~(0x1 << ac));
			ta_bmp = (ta_bmp | (op << ac));
			SCB_PROPTXTSTATUS_SETTIM(scbi, ta_bmp);
			if (BSSCFG_AP(cfg) || BSSCFG_IBSS(cfg) || BSSCFG_IS_TDLS(cfg)) {
				ret = wlc_apps_pvb_update_from_host(wlc, scb, op);
				if (!ret) {
					ta_bmp = (ta_bmp & ~(0x1 << ac));
					SCB_PROPTXTSTATUS_SETTIM(scbi, ta_bmp);
				}
			}
			break;
		}
	}
	return ret;
}

static int16
wlc_flow_ring_get_scb_handle(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *da)
{
	struct scb *scb;
	scb_wlfc_info_t *scbi;
	int16	ret = BCME_BADARG;
	wlc_bsscfg_t *cfg;

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, da);
	if (!scb)
		return ret;

	scbi = SCB_WLFC_INFO(wlc->wlfc, scb);

	cfg = SCB_BSSCFG(scb);
	BCM_REFERENCE(cfg);
	ret = scbi->mac_address_handle;

	if (BSSCFG_AP(cfg) || BSSCFG_IBSS(cfg) || BSSCFG_IS_TDLS(cfg)) {
		ret |= SCBHANDLE_INFORM_PKTPEND_MASK;
		if (!SCB_ISMULTI(scb) && (SCB_PS(scb) || SCB_TWTPS(scb))) {
			ret |= SCBHANDLE_PS_STATE_MASK;
		}
	}

	return ret;
}

#ifdef BCMPCIEDEV
#ifdef WLTDLS
static struct scb *
wlc_scbfind_from_tdlslist(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr)
{
	int32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	FOREACH_BSS(wlc, idx, cfg) {
		if (BSSCFG_IS_TDLS(cfg)) {
			scb = wlc_scbfind(wlc, cfg, (struct ether_addr *)addr);
			if (scb != NULL)
				break;
		}
	}
	return scb;
}
#endif /* WLTDLS */
#endif /* BCMPCIEDEV */

static void
wlc_flush_flowring_pkts(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr, uint8 tid)
{
	struct scb *scb;

	scb = wlc_scbfind_from_wlcif(wlc, wlcif, addr);

#ifdef BCMPCIEDEV
#ifdef WLTDLS
	if (TDLS_ENAB(wlc->pub)) {
		struct scb *tdls_scb = wlc_scbfind_from_tdlslist(wlc, wlcif, addr);
		if (tdls_scb && (tdls_scb != scb)) {
			wlc_txmod_flush_pkts(wlc->txmodi, tdls_scb, tid);
		}
	}
#endif /* WLTDLS */
#endif /* BCMPCIEDEV */

	if (!scb)
		return;

	wlc_txmod_flush_pkts(wlc->txmodi, scb, tid);
}

#if defined(BCMPCIEDEV) && defined(BCMLFRAG) && defined(PROP_TXSTATUS)
/* Suppress newer packets in txq to make way for fragmented pkts in the q */
uint32 wlc_suppress_recent_for_fragmentation(wlc_info_t *wlc, void *sdu, uint nfrags)
{
	struct scb *scb;
	void *p;
	int prec;
	uint i = 0, qlen;
	struct pktq tmp_q;			/**< Multi-priority packet queue */
	pktq_init(&tmp_q, WLC_PREC_COUNT, PKTQ_LEN_MAX);
	scb = WLPKTTAGSCBGET(sdu);

	qlen = pktq_n_pkts_tot(WLC_GET_CQ(wlc->active_queue));
	while (qlen && (i < nfrags)) {
		qlen--;
		p = cpktq_deq_tail(&wlc->active_queue->cpktq, &prec);
		if (!p)
			break;
		if (scb != WLPKTTAGSCBGET(p)) {
			pktq_penq(&tmp_q, prec, p);
			continue;
		}
		wlc_process_wlhdr_txstatus(wlc, WLFC_CTL_PKTFLAG_WLSUPPRESS, p, FALSE);
		PKTFREE(wlc->pub->osh, p, TRUE);
		i++;
	}
	/* Enqueue back the frames generated in dongle */
	while ((p = pktq_deq(&tmp_q, &prec)))
		cpktq_penq_head(wlc, &wlc->active_queue->cpktq, prec, p);
	return i;

}
#endif /* BCMPCIEDEV && BCMLFRAG && PROP_TXSTATUS */

/* scb cubby init/deinit callbacks */
static void
wlc_wlfc_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;

	BCM_REFERENCE(wlc);

	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

		if (scbi->mac_address_handle != 0) {
			wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

			WLFC_DBGMESG(("MAC-DEL for [%02x:%02x:%02x:%02x:%02x:%02x], "
				"handle: [%d], if:%d, t_idx:%d..\n",
				scb->ea.octet[0], scb->ea.octet[1], scb->ea.octet[2],
				scb->ea.octet[3], scb->ea.octet[4], scb->ea.octet[5],
				scbi->mac_address_handle,
				((cfg->wlcif == NULL) ? 0 : cfg->wlcif->index),
				WLFC_MAC_DESC_GET_LOOKUP_INDEX(scbi->mac_address_handle)));

			wlfc_flags_update(wlfc, &scb->ea.octet[0], WLFC_CTL_TYPE_UPDATE_FLAGS,
				WLFC_RESET_ALL_FLAGS);

			/* Do not update the mac handle of the TDLS flowring because that will
			 * make STA and TDLS flowring alike which have same interface index.
			 * So when the TDLS goes down at WL side and interface open is called
			 * on the STA flowring then TDLS remains closed.
			 */
			if (!BSS_TDLS_ENAB(wlc, SCB_BSSCFG(scb)))
				wlfc_MAC_table_update(wlfc, &scb->ea.octet[0],
					WLFC_CTL_TYPE_MACDESC_DEL,
					scbi->mac_address_handle,
					((cfg->wlcif == NULL) ? 0 : cfg->wlcif->index));
			wlfc_release_MAC_descriptor_handle(wlfc, scbi->mac_address_handle);
			scbi->mac_address_handle = 0;
		}
	}
}

static int
wlc_wlfc_scb_init(void *ctx, struct scb *scb)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;

	BCM_REFERENCE(wlc);

	/* 1. hwrs and bcmc scbs don't have flow control needs */
	/* 2. we have to workaround the init sequence where wlc->wl
	 * are only available after the wlc_attach is done but
	 * hwrs scbs and primary bsscfg's bcmc scbs are created
	 * however in wlc_attach()!
	 */
	if (SCB_INTERNAL(scb)) {
		return BCME_OK;
	}

	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

		if (scbi->mac_address_handle == 0) {
			wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
			int err;

			/* allocate a handle from bitmap */
			scbi->mac_address_handle = wlfc_allocate_MAC_descriptor_handle(wlfc);
			if (scbi->mac_address_handle == 0) {
				WL_ERROR(("wl%d.%d: %s wlfc_allocate_MAC_descriptor_handle() "
				          "failed\n",
				          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
				return BCME_NORESOURCE;
			}
			if (!BSSCFG_INFRA_STA(cfg)) {
				/* For STA/GC flowring handle should not be updated as STA/GC uses
				 * pciedev_upd_flr_if_state() for flow control and expects their
				 * flowrings to have handle as 0xff always for flow control
				 * to work.
				 */
				err = wlfc_MAC_table_update(wlfc, &scb->ea.octet[0],
						WLFC_CTL_TYPE_MACDESC_ADD, scbi->mac_address_handle,
						(cfg->wlcif == NULL) ? 0 : cfg->wlcif->index);
				if (err != BCME_OK) {
					WL_ERROR(("wl%d.%d: %s() wlfc_MAC_table_update()"
						"failed %d\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
						__FUNCTION__, err));
					return err;
				}
			}

			WLFC_DBGMESG(("MAC-ADD for ["MACF"], "
				"handle: [%d], if:%d, t_idx:%d..\n",
				ETHER_TO_MACF(scb->ea),
				scbi->mac_address_handle,
				((cfg->wlcif == NULL) ? 0 : cfg->wlcif->index),
				WLFC_MAC_DESC_GET_LOOKUP_INDEX(scbi->mac_address_handle)));
		}
	}

	return BCME_OK;
}

/* scb state change callback (mainly for AP) */
static void
wlc_wlfc_scb_state_upd(void *ctx, scb_state_upd_data_t *data)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	struct scb *scb = data->scb;
	wlc_bsscfg_t *cfg;

	cfg = SCB_BSSCFG(scb);
	BCM_REFERENCE(cfg);

	if (PROP_TXSTATUS_ENAB(wlfc->wlc->pub) && BSSCFG_AP(cfg) &&
	    (data->oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		wlc_update_scb_bus_flctl(wlfc->wlc, scb, WLFC_CTL_TYPE_MAC_CLOSE);
	}
}

/* ***** from wlc_p2p.c ***** */

int
wlc_wlfc_interface_state_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlfc_ctl_type_t open_close)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	int rc = BCME_OK;
	uint8 if_id;
	/* space for type(1), length(1) and value */
	uint8 results[1+1+WLFC_CTL_VALUE_LEN_MAC_FLAGS];

	ASSERT(cfg != NULL);

	ASSERT((open_close == WLFC_CTL_TYPE_MAC_OPEN) ||
		(open_close == WLFC_CTL_TYPE_MAC_CLOSE) ||
		(open_close == WLFC_CTL_TYPE_INTERFACE_OPEN) ||
		(open_close == WLFC_CTL_TYPE_INTERFACE_CLOSE));

#ifdef WLP2P
	/* Is this a p2p GO? */
	if (P2P_GO(wlc, cfg)) {
		/* XXX:
		If this is a p2p GO interface,
		- at WLFC_CTL_TYPE_INTERFACE_CLOSE go through all the associated
		clients and indicate WLFC_CTL_TYPE_MAC_CLOSE for them.

		- at WLFC_CTL_TYPE_INTERFACE_OPEN  go through all the associated
		clients and indicate WLFC_CTL_TYPE_MAC_OPEN if the client in question
		is indeed in open state (i.e. not in powersave).
		*/
		struct scb_iter scbiter;
		struct scb *scb;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

				results[1] = WLFC_CTL_VALUE_LEN_MAC_FLAGS;
				results[2] = scbi->mac_address_handle;
				results[3] = WLFC_MAC_OPEN_CLOSE_NON_PS;
				if (open_close == WLFC_CTL_TYPE_INTERFACE_OPEN) {
					if (!SCB_PS(scb)) {
						results[0] = WLFC_CTL_TYPE_MAC_OPEN;
						rc = wlfc_push_signal_data(wlfc, results,
							sizeof(results), FALSE);
						if (rc != BCME_OK)
							break;
					}
				}
				else {
					results[0] = WLFC_CTL_TYPE_MAC_CLOSE;
					rc = wlfc_push_signal_data(wlfc, results,
						sizeof(results), FALSE);
					if (rc != BCME_OK)
						break;
				}
			}
		}
	}
	else
#endif /* WLP2P */
	{
		if_id = cfg->wlcif->index;
		results[0] = open_close;
		results[1] = WLFC_CTL_VALUE_LEN_INTERFACE;
		results[2] = if_id;
		rc = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
	}
	wlfc_sendup_ctl_info_now(wlfc);
	return rc;
}

/** @param q   Multi-priority packet queue */
void
wlc_wlfc_flush_queue(wlc_info_t *wlc, struct pktq *q)
{
	void *pkt;
	int prec;
	struct scb *scb;
	struct pktq tmp_q;

	pktq_init(&tmp_q, WLC_PREC_COUNT, PKTQ_LEN_MAX);

	/* De-queue the packets and release them if it is from host */
	while ((pkt = pktq_deq(q, &prec))) {
		scb = WLPKTTAGSCBGET(pkt);
		if (!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(pkt)->wl_hdr_information) &
			WLFC_PKTFLAG_PKTFROMHOST) || !scb || SCB_ISMULTI(scb)) {
			pktq_penq(&tmp_q, prec, pkt);
			continue;
		}
		/* Release pkt - Host will resend */
		wlc_suppress_sync_fsm(wlc, scb, pkt, TRUE);
		wlc_process_wlhdr_txstatus(wlc, WLFC_CTL_PKTFLAG_WLSUPPRESS, pkt, FALSE);
		PKTFREE(wlc->osh, pkt, TRUE);
	}

	/* Enqueue back the frames generated in dongle */
	while ((pkt = pktq_deq(&tmp_q, &prec))) {
		scb = WLPKTTAGSCBGET(pkt);
		if (!scb) {
			PKTFREE(wlc->osh, pkt, TRUE);
#ifdef BCMFRWDPOOLREORG
		} else if (BCMFRWDPOOLREORG_ENAB() && PKTISFRWDPKT(wlc->pub->osh, pkt) &&
				PKTISFRAG(wlc->pub->osh, pkt)) {
			PKTFREE(wlc->pub->osh, pkt, TRUE);
#endif /* BCMFRWDPOOLREORG */
		} else {
			pktq_penq(q, prec, pkt);
		}
	}
}

static void
wlc_wlfc_flush_cpkt_queue(wlc_info_t *wlc, struct cpktq *cpktq)
{
	void *pkt;
	int prec;
	struct scb *scb;
	struct pktq tmp_q;			/**< Multi-priority packet queue */

	pktq_init(&tmp_q, WLC_PREC_COUNT, PKTQ_LEN_MAX);

	/* De-queue the packets and release them if it is from host */
	while ((pkt = cpktq_deq(cpktq, &prec))) {
		scb = WLPKTTAGSCBGET(pkt);
		if (!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(pkt)->wl_hdr_information) &
			WLFC_PKTFLAG_PKTFROMHOST) || !scb || SCB_ISMULTI(scb)) {
			pktq_penq(&tmp_q, prec, pkt);
			continue;
		}
		/* Release pkt - Host will resend */
		wlc_suppress_sync_fsm(wlc, scb, pkt, TRUE);
		wlc_process_wlhdr_txstatus(wlc, WLFC_CTL_PKTFLAG_WLSUPPRESS, pkt, FALSE);
		PKTFREE(wlc->osh, pkt, TRUE);
	}

	/* Enqueue back the frames generated in dongle */
	while ((pkt = pktq_deq(&tmp_q, &prec))) {
		scb = WLPKTTAGSCBGET(pkt);
		if (!scb) {
			PKTFREE(wlc->osh, pkt, TRUE);
#ifdef BCMFRWDPOOLREORG
		} else if (BCMFRWDPOOLREORG_ENAB() && PKTISFRWDPKT(wlc->pub->osh, pkt) &&
				PKTISFRAG(wlc->pub->osh, pkt)) {
			PKTFREE(wlc->pub->osh, pkt, TRUE);
#endif /* BCMFRWDPOOLREORG */
		} else {
			cpktq_penq(wlc, cpktq, prec, pkt);
		}
	}
}
/**
 * This function suppress back the host generated packets from various queues to
 * host.
 */
void
wlc_wlfc_flush_pkts_to_host(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	BCM_REFERENCE(scb);
	BCM_REFERENCE(scbiter);

	wlc_txfifo_suppress(wlc, NULL);

	/* Flush bsscfg psq */
	wlc_bsscfg_tx_flush(wlc->psqi, cfg);

	/* Flush txq */
	wlc_wlfc_flush_cpkt_queue(wlc, &cfg->wlcif->qi->cpktq);
	wlc_check_txq_fc(wlc, cfg->wlcif->qi);

#if defined(AP)
	/* TODO: a loop for SCB_TX_MOD_PREV(last_to_first) with get_q function ?? */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		wlc_apps_ps_flush_mchan(wlc, scb);
	}
#endif /* AP */
#if defined(WLAMPDU)
	if (AMPDU_ENAB(wlc->pub)) {
		wlc_ampdu_flush_ampdu_q(wlc->ampdu_tx, cfg);
	}
#endif /* WLAMPDU */
}

/* ***** from wlc.c ***** */

/**
 * PROP_TXSTATUS related function. Implements a per-remote-party (per-scb) state machine containing
 * just 2 states ('suppress' and 'accept'). Is called by various transmit related functions. This
 * fsm can get invoked due to:
 *     - D11_suppress [suppress = 1]
 *     - p2p_suppress on NOA or outside ctw [suppress = 1]
 *     - any packet from host [suppress = 0]
 *
 * Parameters:
 *     @param sc for unicast: [in/out] destination MAC scb, for for bc/mc packet:  bsscfg scb
 *     @param p  [in] packet that was just suppressed or received from the host
 *     @param suppress  [in] indicates if packet was just received from host (suppress=0)
 * Returns TRUE if the packet should be wlc_suppressed. Is not called for PCIe full dongle firmware.
 */
bool
wlc_suppress_sync_fsm(wlc_info_t *wlc, struct scb *scb, void *p, bool suppress)
{
	uint8 rc = FALSE;
	bool	amsdu_pkt = FALSE;
	uint32 *pwlhinfo;

	/* XXX:
	If in "accept" state [0], go to "suppress" [1] upon encountering any suppressed packets.
	Stay in "suppress" until a packet is seen from the host with correct generation
	number. Generation number toggles whenever an "accept" to "suppress" transition occurs.

	if none of the proptxtstatus signals are enabled by the host, do not do the generation check
	*/
	if (!HOST_PROPTXSTATUS_ACTIVATED(wlc))
		return FALSE;

	if ((WLPKTTAG(p)->flags & WLF_PROPTX_PROCESSED))
		return FALSE;

#ifdef BCMPCIEDEV
	/* Gen bits are not handled in PCIe code as suppression will terminate in dongle itself */
	if (BCMPCIEDEV_ENAB() && !WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub))
		return FALSE;
#endif /* BCMPCIEDEV */

	amsdu_pkt = WLPKTFLAG_AMSDU(WLPKTTAG(p));

	do {
		wlc_wlfc_info_t *wlfc = wlc->wlfc;
		scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

		pwlhinfo = &(WLPKTTAG(p)->wl_hdr_information);

		if (SCB_PROPTXTSTATUS_SUPPR_STATE(scbi)) {
			/* "suppress" state */
			if (!suppress &&
				WL_TXSTATUS_GET_GENERATION(*pwlhinfo) ==
				SCB_PROPTXTSTATUS_SUPPR_GEN(scbi)) {
				/* now the host is in sync, since the generation is a match */
				SCB_PROPTXTSTATUS_SUPPR_SETSTATE(scbi, 0);
				scbi->first_sup_pkt = 0xffff;
			}
			/* XXX If we receive a packet which was already a suppressed PKT,
			 * again next time, toggle the GEN bit such that this PKT should be
			 * accepted next time.
			 */
			else if (!WLFC_GET_REORDERSUPP(wlfc_query_mode(wlfc))&&
				(scbi->first_sup_pkt == (*pwlhinfo & 0xff))) {
				SCB_PROPTXTSTATUS_SUPPR_SETGEN(scbi,
					(SCB_PROPTXTSTATUS_SUPPR_GEN(scbi) ^ 1));
			} else if (WLFC_GET_REORDERSUPP(wlfc_query_mode(wlfc)) &&
				((WL_TXSTATUS_GET_FLAGS(*pwlhinfo) & WLFC_PKTFLAG_PKTFROMHOST) &&
				(WL_TXSTATUS_GET_GENERATION(*pwlhinfo) ==
				SCB_PROPTXTSTATUS_SUPPR_GEN(scbi)))) {
				SCB_PROPTXTSTATUS_SUPPR_SETGEN(scbi,
					(SCB_PROPTXTSTATUS_SUPPR_GEN(scbi) ^ 1));
			} else {
				rc = TRUE;
			}
		} else {
			/* "accept" state */
			if (suppress && (WL_TXSTATUS_GET_FLAGS(*pwlhinfo) &
				WLFC_PKTFLAG_PKTFROMHOST)) {
				/* A suppress occurred while in "accept" state;
				* switch state, update gen # ONLY when packet is from HOST
				*/
				SCB_PROPTXTSTATUS_SUPPR_SETSTATE(scbi, 1);
				/*
				* XXX Store the first suppressed pkt id to toggle the GEN bit,
				* if the same pkt gets suppressed again without going to accept
				* state. This is neccesary in some conditions, when no pkts are
				* accepted and they got re-supressed.
				*/
				scbi->first_sup_pkt = (*pwlhinfo & 0xff);
				SCB_PROPTXTSTATUS_SUPPR_SETGEN(scbi,
					(SCB_PROPTXTSTATUS_SUPPR_GEN(scbi) ^ 1));
			}
		}
		WL_TXSTATUS_SET_GENERATION(*pwlhinfo, SCB_PROPTXTSTATUS_SUPPR_GEN(scbi));
	}  while (amsdu_pkt && ((p = PKTNEXT(wlc->osh, p)) != NULL));
	return rc;
} /* wlc_suppress_sync_fsm */

void
wlc_process_wlhdr_txstatus(wlc_info_t *wlc, uint8 status_flag, void *p, bool hold)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;

	wlfc_process_txstatus(wlfc, status_flag, p, NULL, hold);
#ifdef PROP_TXSTATUS_DEBUG
	switch (status_flag) {
	case WLFC_CTL_PKTFLAG_TOSSED_BYWLC:
		WLFC_COUNTER_TXSTATUS_WLCTOSS(wlfc);
		break;
	case WLFC_CTL_PKTFLAG_WLSUPPRESS:
		wlfc->dbgstats->wlfc_wlfc_sup++;
		break;
	default:
		/* do nothing */
		break;
	}
#endif // endif
}

uint8
wlc_txstatus_interpret(tx_status_macinfo_t *txstatus, int8 acked)
{
	if (acked)
		return WLFC_CTL_PKTFLAG_DISCARD;
	if (txstatus->suppr_ind == TX_STATUS_SUPR_EXPTIME)
		return WLFC_CTL_PKTFLAG_EXPIRED;
	if (TXS_SUPR_MAGG_DONE(txstatus->suppr_ind))
		return WLFC_CTL_PKTFLAG_DISCARD_NOACK;
	else
		return WLFC_CTL_PKTFLAG_D11SUPPRESS;
}

void
wlc_txfifo_suppress(wlc_info_t *wlc, struct scb* pmq_scb)
{
#ifdef BCMPCIEDEV
	/* this function just sends Txstatus for suppressed pakets in
	 * dma rings proptxtstaus txstatus handling in PCIe FD overwrites pkt data
	 * skipping for PCIe FD to prevent pkt corruption as data will be accessed in
	 * wlc_dotxstatus also as it takes care or OOO packets at flowring level
	 */
	if (BCMPCIEDEV_ENAB())
		return;
#endif // endif

	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		/* valid SCB (SCB_PS) :Peek and suppress only those pkts
		 * that belong to PMQ-ed scb.
		 */
		/* null SCB (BSSCFG_ABS) :all pkts in TxFIFO will be suppressed */
		void **phdr_list = NULL;
		hnddma_t* di = NULL;
		int i, j;
		int size;

		int fifo_list[] = { TX_AC_BE_FIFO, TX_AC_BK_FIFO, TX_AC_VI_FIFO, TX_AC_VO_FIFO };
		for (j = 0; j < (sizeof(fifo_list)/sizeof(int)); j++) {
			int fifo = fifo_list[j];
			int n = TXPKTPENDGET(wlc, fifo);

			if (n == 0)
				continue;

			size = (n * sizeof(void*));
			phdr_list = MALLOCZ(wlc->osh, size);
			if (phdr_list == NULL)
				continue;

			di = WLC_HW_DI(wlc, fifo);
			if (di != NULL &&
			    dma_peekntxp(di, &n, phdr_list, HNDDMA_RANGE_ALL) == BCME_OK) {
				for (i = 0; i < n; i++) {
					wlc_pkttag_t *tag;
					struct scb *scb;
					uint flags;
					void *p;

					p = phdr_list[i];
					ASSERT(p);

					tag = WLPKTTAG(p);
					scb = WLPKTTAGSCBGET(p);
					flags = WL_TXSTATUS_GET_FLAGS(tag->wl_hdr_information);

					if ((pmq_scb == NULL) ||
					    (pmq_scb && scb && (pmq_scb == scb) &&
					     (!(scb && SCB_ISMULTI(scb)) &&
					      (flags & WLFC_PKTFLAG_PKTFROMHOST)))) {
						wlc_suppress_sync_fsm(wlc, scb, p, TRUE);
						wlc_process_wlhdr_txstatus(wlc,
							WLFC_CTL_PKTFLAG_WLSUPPRESS,
							p, i < (n-1) ? TRUE : FALSE);
					}
				} /* for each p */
			} /* if dma_peek */
			MFREE(wlc->osh, phdr_list, size);
		} /* for each fifo */
	} /* PROP_TX ENAB */
} /* wlc_txfifo_suppress */

#ifdef WLCFP
/**
 * Link a bus layer flow with CFP flow. Return
 * 1. cfp flow ID
 * 2. Pointer to TCB state
 */
int
wlc_bus_cfp_link_update(wlc_info_t *wlc, struct wlc_if *wlcif,
	uint16 ringid, uint8 tid, uint8* da, uint8 op,
	uint8** tcb_state, uint16* flowid)
{
	struct scb * scb;
	int ret = BCME_ERROR;

	if (*flowid == SCB_FLOWID_INVALID) {
		/* Flow ring Link process; Find scb based on da */
		scb = wlc_scbfind_from_wlcif(wlc, wlcif, da);
	} else {
		/* Flow ring delink process; Lookup scb from the flowid */
		scb = wlc_scb_flowid_lookup(wlc, *flowid);
	}

	if (scb == NULL) {
		return ret;
	}

	switch (op) {
		case CFP_FLOWID_LINK :
			/* Link Host ringid with SCB flow id */
			wlc_scb_host_ring_link(wlc, ringid, tid, scb, flowid);

			/* Return back the CFP TCB ptr */
			ret = wlc_scb_cfp_tcb_link(wlc, scb, tcb_state);
			break;
		case CFP_FLOWID_DELINK :
			/* Delink the FLOW ring and CFP flows */
			ret = wlc_scb_flow_ring_tid_delink(wlc, scb, tid);
			break;
		default:
			return BCME_ERROR;
			break;
	}
	return ret;

}
#endif /* WLCFP */

#ifdef WLSQS
void
wlc_wlfc_sqs_stride_resume(wlc_info_t *wlc, struct scb *scb, uint8 prio)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi;
	/* space for type(1), length(1) and value */
	uint8 results[1+1+WLFC_CTL_VALUE_LEN_MAC];
	int rc = BCME_OK;

	/* Do we want to reuse 'WLFC_CTL_TYPE_MAC_REQUEST_PACKET'? */
	results[0] = WLFC_CTL_TYPE_SQS_STRIDE;
	results[1] = WLFC_CTL_VALUE_LEN_MAC;
	if (scb) {
		scbi = SCB_WLFC_INFO(wlfc, scb);
		results[2] = scbi->mac_address_handle;
	} else
		results[2] = NULL;

	rc = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
	if (rc != BCME_OK) {
		WL_ERROR(("%s: wlfc_push_signal_data failed\n", __FUNCTION__));
	}
}
#endif /* WLSQS */

/** @param[in] op	e.g. FLOW_RING_CREATE, FLOW_RING_FLUSH, FLOW_RING_TIM_RESET */
int16
wlc_link_txflow_scb(wlc_info_t *wlc, struct wlc_if *wlcif, uint16 flowid, uint8 op, uint8 *sa,
	uint8 *da, uint8 tid)
{
	int16	ret = BCME_BADARG;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	switch (op) {
	case FLOW_RING_CREATE :
		ret = wlc_flow_ring_get_scb_handle(wlc, wlcif, da);
		break;
	case FLOW_RING_DELETE :
		/* Fall through to flush the pkts on delete flowring */
	case FLOW_RING_FLUSH :
		wlc_flush_flowring_pkts(wlc, wlcif, da, tid);
		break;
	case FLOW_RING_TIM_SET:
	case FLOW_RING_TIM_RESET:
		if (!flowid)
			break;
		ret = wlc_flow_ring_scb_update_available_traffic_info(wlc, flowid,
				tid, (op == FLOW_RING_TIM_SET));
		break;
	case FLOW_RING_FLUSH_TXFIFO:
		{
			/* bsscfg can not be NULL */
			ASSERT(bsscfg);
			if (!wlc->pub->up) {
				WL_ERROR(("wl%d: is not up, cannot fifo flush:%d\n",
					wlc->pub->unit, flowid));
			} else {
				/* Flush the SCB from the DMA fifo */
				wlc->fifoflush_id = flowid;
				wlc_sync_txfifo_all(wlc, bsscfg->wlcif->qi, FLUSHFIFO_FLUSHID);
			}
			break;
		}
	case FLOW_RING_GET_PKT_MAX:
		{
			ret = wlc_flow_ring_get_maxpkts_for_link(wlc, wlcif, da);
			break;
		}
	default :
		break;
	}
	return ret;
}

int
wlc_update_scb_bus_flctl(wlc_info_t *wlc, struct scb *scb, wlfc_ctl_type_t open_close)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
	int rc = BCME_OK;
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_MAC_FLAGS];
	ASSERT((open_close == WLFC_CTL_TYPE_MAC_OPEN) ||
			(open_close == WLFC_CTL_TYPE_MAC_CLOSE));

	/* wlfc handle is already reset for this scb
	 * So do not proceed further.
	 * Passing handle as zero will disable BCMC flow
	 * in pciedev layer
	 */
	if (scbi->mac_address_handle == 0) {
		return rc;
	}

	results[0] = open_close;
	results[1] = WLFC_CTL_VALUE_LEN_MAC_FLAGS;
	results[2] = scbi->mac_address_handle;
	results[3] = WLFC_MAC_OPEN_CLOSE_NON_PS;

	if ((rc = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE)) == BCME_OK) {
		rc = wlfc_sendup_ctl_info_now(wlfc);
	}

	/* Also prevent pciedev sending a packet for tim purposes.
	 * This is also needed to prevent HWA from sending packets after flowring has been closed.
	 */
	if (open_close == WLFC_CTL_TYPE_MAC_CLOSE) {
		WLFC_DBGMESG(("Closing flowrings for scb "MACF"\n", ETHER_TO_MACF(scb->ea)));
		wlfc_flags_update(wlfc, &scb->ea.octet[0], WLFC_CTL_TYPE_UPDATE_FLAGS,
			WLFC_RESET_ALL_FLAGS);
	} else {
		WLFC_DBGMESG(("Opening flowrings for scb "MACF"\n", ETHER_TO_MACF(scb->ea)));
		wlfc_flags_update(wlfc, &scb->ea.octet[0], WLFC_CTL_TYPE_UPDATE_FLAGS,
			WLFC_SET_FLAG_INFORM_PKTPEND);
	}

	return rc;
}

/* ***** from wlc_apps.c ***** */

static int
wlc_apps_push_wlfc_psmode_update(wlc_info_t *wlc, uint8 mac_handle, wlfc_ctl_type_t open_close)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	int rc = BCME_OK;
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_MAC_FLAGS];
	ASSERT((open_close == WLFC_CTL_TYPE_MAC_OPEN) ||
			(open_close == WLFC_CTL_TYPE_MAC_CLOSE));

	results[0] = open_close;
	results[1] = WLFC_CTL_VALUE_LEN_MAC_FLAGS;
	results[2] = mac_handle;
	results[3] = WLFC_MAC_OPEN_CLOSE_FROM_PS;

	if ((rc = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE)) == BCME_OK) {
		rc = wlfc_sendup_ctl_info_now(wlfc);
	}
	return rc;
}

void
wlc_wlfc_scb_ps_off(wlc_info_t *wlc, struct scb *scb)
{
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlc->wlfc, scb);

	wlc_apps_push_wlfc_psmode_update(wlc, scbi->mac_address_handle,
			WLFC_CTL_TYPE_MAC_OPEN);
	/* reset the traffic availability map */
	SCB_PROPTXTSTATUS_SETTIM(scbi, 0);
	SCB_PROPTXTSTATUS_SETPKTWAITING(scbi, 0);
#ifdef WLAMPDU
	/* We might have suppressed pkts during Power-Save ON */
	/* Reset AMPDU Seqcnt with a BAR */
	if (AMPDU_ENAB(wlc->pub)) {
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
		struct scb_iter scbiter;
		struct scb *scb_i;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb_i) {
			wlc_ampdu_send_bar_cfg(wlc->ampdu_tx, scb_i);
		}
	}
#endif /* AMPDU */
}

void
wlc_wlfc_scb_ps_on(wlc_info_t *wlc, struct scb *scb)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
	uint8 vqdepth = 0;

#ifdef WLTDLS
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);

	if (BSS_TDLS_ENAB(wlc, cfg)) {
		if ((scb->apsd.maxsplen == WLC_APSD_USP_UNB) || (scb->apsd.maxsplen == 0))
			vqdepth = TDLS_WLFC_DEFAULT_FWQ_DEPTH;
		else
			vqdepth = scb->apsd.maxsplen;
	} else
#endif // endif
	{
		vqdepth = wlfc->wlfc_vqdepth;
	}

	wlc_apps_push_wlfc_psmode_update(wlc, scbi->mac_address_handle,
			WLFC_CTL_TYPE_MAC_CLOSE);
	/* Leaving bitmap open for all ACs for now */
	if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub) && (wlfc->wlfc_vqdepth > 0)) {
		wlfc_psmode_request(wlfc, scbi->mac_address_handle,
		                    vqdepth, 0xff, WLFC_CTL_TYPE_MAC_REQUEST_CREDIT);
	}

	/* TODO: Suppress pkts of this scb to host */
}

/* send response if possible */
void
wlc_wlfc_scb_psq_resp(wlc_info_t *wlc, struct scb *scb)
{
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlc->wlfc, scb);
	uint16 fc = 0;

	if (SCB_PROPTXTSTATUS_PKTWAITING(scbi)) {
		/* XXX PSPoll retry bit tracking is not needed. Obsolete with PS_PSP_ONRESP logic.
		 * Also, PSPoll frames are not supposed to set the retry bit as
		 * a Control Frame (802.11-2012 8.3.1.1)
		 */
		if (SCB_PROPTXTSTATUS_POLLRETRY(scbi)) {
			fc |= FC_RETRY;
			SCB_PROPTXTSTATUS_SETPOLLRETRY(scbi, 0);
		}
		if (HOST_PROPTXSTATUS_ACTIVATED(wlc)) {
			wlc_apps_send_psp_response(wlc, scb, fc);
		}
	}
}

/* Set/clear PVB entry according to current state of power save queues at the host */
static bool
wlc_apps_pvb_update_from_host(wlc_info_t *wlc, struct scb *scb, bool op)
{
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
#if defined(BCMPCIEDEV)
		if (BCMPCIEDEV_ENAB()) {
			wlc_wlfc_info_t *wlfc = wlc->wlfc;
			scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
			if (SCB_PROPTXTSTATUS_TIM(scbi) &&
			    !wlc_apps_pvb_upd_in_transit(wlc, scb, wlfc->wlfc_vqdepth, op)) {
				return FALSE;
			}

		if (!op && SCB_PROPTXTSTATUS_PKTWAITING(scbi)) {
				SCB_PROPTXTSTATUS_SETPKTWAITING(scbi, 0);
		}
	}
#endif /* BCMPCIEDEV */
	}
	if (!BSSCFG_IBSS(scb->bsscfg)) {
		wlc_apps_pvb_update(wlc, scb);
	}
	return TRUE;
}

/* Check if packets are available in Host flow ring */
bool
wlc_wlfc_scb_host_pkt_avail(wlc_info_t *wlc, struct scb *scb)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

	ASSERT(scb);
	scbi = SCB_WLFC_INFO(wlfc, scb);

	ASSERT(scbi);

	return (SCB_PROPTXTSTATUS_TIM(scbi) ? TRUE: FALSE);
}
/**
 * Decide if we respond to pspoll
 *
 * @param pktq   Multi-priority packet queue
 */
bool
wlc_wlfc_scb_psq_chk(wlc_info_t *wlc, struct scb *scb, uint16 fc, struct pktq *pktq)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
	int psq_len;

	if (SCB_PROPTXTSTATUS_PKTWAITING(scbi)) {
		SCB_PROPTXTSTATUS_SETPKTWAITING(scbi, 0);
	}

	psq_len = pktq_n_pkts_tot(pktq);
#ifdef WLCFP
	if (CFP_ENAB(wlc->pub)) {
		psq_len += wlc_ampdu_scb_txpktcnt(wlc->ampdu_tx, scb);
	}
#endif /* WLCFP */

	if ((psq_len < wlfc->wlfc_vqdepth) && SCB_PROPTXTSTATUS_TIM(scbi)) {
		/* request the host for a packet */
		wlfc_psmode_request(wlfc, scbi->mac_address_handle,
		                    1, AC_BITMAP_ALL, WLFC_CTL_TYPE_MAC_REQUEST_PACKET);
		/* Obsolete with PS_PSP_ONRESP logic -- no need to track PSPoll Retry
		 * for PS_PSP_REQ_PEND
		 */
		if (fc & FC_RETRY) {
			/* remember the fc retry bit */
			SCB_PROPTXTSTATUS_SETPOLLRETRY(scbi, 1);
		}

		if ((psq_len == 0) && SCB_PROPTXTSTATUS_TIM(scbi)) {
			SCB_PROPTXTSTATUS_SETPKTWAITING(scbi, 1);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Decide if there are any packets on the host and also request a packet from host if force_request
 * is true
 *
 * @param pktq   Multi-priority packet queue
 */
uint
wlc_wlfc_scb_ps_count(wlc_info_t *wlc, struct scb *scb, bool bss_ps,
		struct pktq *pktq, bool force_request)
{
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlc->wlfc, scb);
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	uint ps_count;
	/*
	 * If there is no packet locally, check the traffic availability flags from the
	 * host.
	 * <Section 3.6.1.4 WMM spec 1.2.0>
	 * At every beacon interval, the U-APSD-capable WMM AP shall assemble the Partial Virtual
	 * Bitmap containing the buffer status of non delivery-enabled ACs (if there exists
	 * at least one non delivery-enabled AC) per destination for WMM STAs in PS mode, and shall
	 * send this out in the TIM field of the beacon. In case all ACs are delivery-enabled ACs,
	 * the U-APSD-capable WMM AP shall assemble the Partial Virtual Bitmap containing
	 * the buffer status for all ACs per destination for WMM STAs.
	 */

	if ((scb->apsd.ac_delv == AC_BITMAP_NONE) ||
			(scb->apsd.ac_delv == AC_BITMAP_ALL)) {
		ps_count = (SCB_PROPTXTSTATUS_TIM(scbi) ? 1 : 0);
	} else {
		ac_bitmap_t ac_to_request = ~scb->apsd.ac_delv & AC_BITMAP_ALL;
		ps_count = ((SCB_PROPTXTSTATUS_TIM(scbi) &
			ac_to_request) &&
			(bss_ps) ? 1 : 0);
		/* Requesting a packet from host */
		if (ps_count > 0 && force_request) {
			wlfc_psmode_request(wlfc, scbi->mac_address_handle, 1,
					ac_to_request,
					WLFC_CTL_TYPE_MAC_REQUEST_PACKET);
		}

	}

	return ps_count;
}

/* ***** from wlc_mchan.c ***** */

int
wlc_wlfc_mchan_interface_state_update(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlfc_ctl_type_t open_close, bool force_open)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	int rc = BCME_OK;
	uint8 if_id;
	struct scb_iter scbiter;
	struct scb *scb;
	/* space for type(1), length(1) and value */
	uint8 results[1+1+WLFC_CTL_VALUE_LEN_MAC_FLAGS];

	ASSERT((open_close == WLFC_CTL_TYPE_MAC_OPEN) ||
		(open_close == WLFC_CTL_TYPE_MAC_CLOSE) ||
		(open_close == WLFC_CTL_TYPE_INTERFACE_OPEN) ||
		(open_close == WLFC_CTL_TYPE_INTERFACE_CLOSE));

	if (bsscfg == NULL)
		return rc;

	if (BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

				results[1] = WLFC_CTL_VALUE_LEN_MAC_FLAGS;
				results[2] = scbi->mac_address_handle;
				results[3] = WLFC_MAC_OPEN_CLOSE_NON_PS;
				if (open_close == WLFC_CTL_TYPE_INTERFACE_OPEN) {
					if (BSS_P2P_ENAB(wlc, bsscfg) && force_open == FALSE)
						return rc;
					if (!SCB_PS(scb)) {
						results[0] = WLFC_CTL_TYPE_MAC_OPEN;
						if (PROP_TXSTATUS_ENAB(wlc->pub)) {
							rc = wlfc_push_signal_data(wlfc, results,
									sizeof(results), FALSE);
						}
						if (rc != BCME_OK)
							break;
					}
				}
				else {
					results[0] = WLFC_CTL_TYPE_MAC_CLOSE;
					if (PROP_TXSTATUS_ENAB(wlc->pub)) {
						rc = wlfc_push_signal_data(wlfc, results,
								sizeof(results), FALSE);
					}
					if (rc != BCME_OK)
						break;
				}
			}
		}
	}
	else {
		if_id = bsscfg->wlcif->index;
		results[0] = open_close;
		results[1] = WLFC_CTL_VALUE_LEN_INTERFACE;
		results[2] = if_id;
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			rc = wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
		}
	}
	/* convey an OPEN / CLOSE signal as soon as possible to the host */
	wlfc_sendup_ctl_info_now(wlfc);
	return rc;
}

int
wlc_wlfc_mac_status_upd(wlc_wlfc_info_t *wlfc, struct scb *scb, uint8 open_close)
{
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
	/* space for type(1), length(1) and value */
	uint8 results[1 + 1 + WLFC_CTL_VALUE_LEN_MAC_FLAGS];

	results[0] = open_close;
	results[1] = WLFC_CTL_VALUE_LEN_MAC_FLAGS;
	results[2] = scbi->mac_address_handle;
	results[3] = WLFC_MAC_OPEN_CLOSE_NON_PS;

	return wlfc_push_signal_data(wlfc, results, sizeof(results), FALSE);
}

void
wlc_wlfc_psmode_request(wlc_wlfc_info_t *wlfc, struct scb *scb,
	uint8 credit, uint8 ac_bitmap, wlfc_ctl_type_t request_type)
{
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);

	wlfc_psmode_request(wlfc, scbi->mac_address_handle,
	                    credit, ac_bitmap, request_type);
}

bool wlc_wlfc_suppr_status_query(wlc_info_t *wlc, struct scb *scb)
{
	wlc_wlfc_info_t *wlfc = wlc->wlfc;
	scb_wlfc_info_t *scbi = SCB_WLFC_INFO(wlfc, scb);
	return SCB_PROPTXTSTATUS_SUPPR_STATE(scbi);
}

static void
wlc_wlfc_send_if_event(void *ctx)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;

	if (PROP_TXSTATUS_ENAB(wlc->pub) ||
#if defined(BCMPCIEDEV)
		BCMPCIEDEV_ENAB() ||
#endif // endif
		FALSE) {
			wlc_bsscfg_t* bsscfg = wlc_bsscfg_find_by_wlcif(wlc, NULL);
			wlc_if_event(wlc, WLC_E_IF_ADD, bsscfg->wlcif);
		}
}

/* wlc module hooks */
static int
wlc_wlfc_wlc_up(void *ctx)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;

	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_send_credit_map(wlc);
	}
	return BCME_OK;
}

static int
wlc_wlfc_wlc_down(void *ctx)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;

	/* cancel timer */
	if (wlfc->timer_started) {
		wl_del_timer(wlc->wl, wlfc->fctimer);
		wlfc->timer_started = 0;
	}

	return BCME_OK;
}

/* iovar dispatch */
static int
wlc_wlfc_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_wlfc_info_t *wlfc = (wlc_wlfc_info_t *)ctx;
	wlc_info_t *wlc = wlfc->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	if (!PROP_TXSTATUS_ENAB(wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_COMPSTAT):
		*ret_int_ptr = wlfc->comp_stat_enab;
		break;
	case IOV_SVAL(IOV_COMPSTAT):
		if (int_val)
			wlfc->comp_stat_enab = TRUE;
		else
			wlfc->comp_stat_enab = FALSE;
		break;
	case IOV_GVAL(IOV_TLV_SIGNAL):
		*ret_int_ptr = wlc->wlfc_flags;
		break;
	case IOV_SVAL(IOV_TLV_SIGNAL):
		wlc->wlfc_flags = int_val;
		if (!HOST_PROPTXSTATUS_ACTIVATED(wlc))
			wlfc_reset_credittohost(wlfc);
		break;

	case IOV_GVAL(IOV_WLFC_MODE): {
		uint32 caps = 0;

		WLFC_SET_AFQ(caps, 1);
		WLFC_SET_REUSESEQ(caps, 1);
		WLFC_SET_REORDERSUPP(caps, 1);
		*ret_int_ptr = caps;
		break;
	}
	case IOV_SVAL(IOV_WLFC_MODE):
		if (WLFC_IS_OLD_DEF(int_val)) {
			wlfc->wlfc_mode = 0;
			if (int_val == WLFC_MODE_AFQ) {
				WLFC_SET_AFQ(wlfc->wlfc_mode, 1);
			} else if (int_val == WLFC_MODE_HANGER) {
				WLFC_SET_AFQ(wlfc->wlfc_mode, 0);
			} else {
				WL_ERROR(("%s: invalid wlfc mode value = 0x%x\n",
				          __FUNCTION__, int_val));
			}
		} else {
			wlfc->wlfc_mode = int_val;
		}
		break;

	case IOV_SVAL(IOV_PROPTX_CRTHRESH): {
		wlc_tunables_t *tunables = wlc->pub->tunables;

		wlfc->fifo_credit_threshold[TX_AC_BK_FIFO] = MIN(
			MAX((int_val >> 24) & 0xff, 1), tunables->wlfcfifocreditac0);
		wlfc->fifo_credit_threshold[TX_AC_BE_FIFO] = MIN(
			MAX((int_val >> 16) & 0xff, 1), tunables->wlfcfifocreditac1);
		wlfc->fifo_credit_threshold[TX_AC_VI_FIFO] = MIN(
			MAX((int_val >> 8) & 0xff, 1), tunables->wlfcfifocreditac2);
		wlfc->fifo_credit_threshold[TX_AC_VO_FIFO] = MIN(
			MAX((int_val & 0xff), 1), tunables->wlfcfifocreditac3);
		break;
	}
	case IOV_GVAL(IOV_PROPTX_CRTHRESH):
		*ret_int_ptr =
		        (wlfc->fifo_credit_threshold[TX_AC_BK_FIFO] << 24) |
		        (wlfc->fifo_credit_threshold[TX_AC_BE_FIFO] << 16) |
		        (wlfc->fifo_credit_threshold[TX_AC_VI_FIFO] << 8) |
		        (wlfc->fifo_credit_threshold[TX_AC_VO_FIFO]);
		break;

	case IOV_SVAL(IOV_PROPTX_DATATHRESH):
		wlfc->pending_datathresh =
		        MIN(MAX(int_val, 1), WLFC_MAX_PENDING_DATALEN);
		break;

	case IOV_GVAL(IOV_PROPTX_DATATHRESH):
		*ret_int_ptr = wlfc->pending_datathresh;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* attach/detach */
wlc_wlfc_info_t *
BCMATTACHFN(wlc_wlfc_attach)(wlc_info_t *wlc)
{
	wlc_tunables_t *tunables = wlc->pub->tunables;
	wlc_wlfc_info_t *wlfc;

	/* allocate module private info */
	if ((wlfc = MALLOCZ(wlc->osh, sizeof(*wlfc))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	wlfc->wlc = wlc;

	wlfc->wlfc_vqdepth = WLFC_DEFAULT_FWQ_DEPTH;

	/* default use hanger */
	wlfc->wlfc_mode = WLFC_MODE_HANGER;
#ifdef BCMPCIEDEV_ENABLED
	WLFC_SET_REUSESEQ(wlfc->wlfc_mode, 1);
#endif // endif
	if (wl_if_event_send_cb_fn_register(wlfc->wlc->wl, wlc_wlfc_send_if_event, wlfc)) {
		WL_ERROR(("wl%d: %s: unable to register if_event callback function \n",
				wlc->pub->unit, __FUNCTION__));
	}

	/* use scb cubby mechanism to register scb init/deinit callbacks */
	if ((wlfc->scbh = wlc_scb_cubby_reserve(wlc, sizeof(scb_wlfc_info_t),
			wlc_wlfc_scb_init, wlc_wlfc_scb_deinit, NULL, wlfc)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register scb state change callback */
	if (wlc_scb_state_upd_register(wlc, wlc_wlfc_scb_state_upd, wlfc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_scb_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, wlfc_iovars, "wlfc", wlfc, wlc_wlfc_doiovar,
	                        NULL, wlc_wlfc_wlc_up, wlc_wlfc_wlc_down) != BCME_OK) {
		WL_ERROR(("wl%d: ampdu_tx wlc_module_register() failed\n",
		          wlc->pub->unit));
		goto fail;
	}

	/* init and add a timer for periodic wlfc signal sendup */
	if (!(wlfc->fctimer =
	      wl_init_timer(wlc->wl, wlfc_sendup_timer, wlfc, "wlfctimer"))) {
		WL_ERROR(("wl%d: wl_init_timer for wlfc timer failed\n", wlc->pub->unit));
		goto fail;
	}

#ifdef PROP_TXSTATUS_DEBUG
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_WLFC, wlfc_hostpkt_callback) != BCME_OK) {
		WL_ERROR(("wlc_pcb_fn_set(wlfc) failed in %s()", __FUNCTION__));
		goto fail;
	}

	wlfc->dbgstats = MALLOCZ(wlc->osh, sizeof(wlfc_fw_dbgstats_t));
	if (wlfc->dbgstats == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(wlfc_fw_dbgstats_t), MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* PROP_TXSTATUS_DEBUG */

	wlfc->pending_datathresh = WLFC_PENDING_TRIGGER_WATERMARK;
	wlfc->fifo_credit_threshold[TX_AC_BE_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_be;
	wlfc->fifo_credit_threshold[TX_AC_BK_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_bk;
	wlfc->fifo_credit_threshold[TX_AC_VI_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vi;
	wlfc->fifo_credit_threshold[TX_AC_VO_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vo;
	wlfc->fifo_credit_threshold[TX_BCMC_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_bcmc;
	wlfc->fifo_credit_threshold[TX_ATIM_FIFO] = 1;	/* send credit back ASAP */
	wlfc->fifo_credit[TX_AC_BE_FIFO] =
		tunables->wlfcfifocreditac1;
	wlfc->fifo_credit[TX_AC_BK_FIFO] =
		tunables->wlfcfifocreditac0;
	wlfc->fifo_credit[TX_AC_VI_FIFO] =
		tunables->wlfcfifocreditac2;
	wlfc->fifo_credit[TX_AC_VO_FIFO] =
		tunables->wlfcfifocreditac3;
	wlfc->fifo_credit[TX_BCMC_FIFO] =
		tunables->wlfcfifocreditbcmc;
	wlfc->totalcredittohost = 0;
	wlfc->total_credit = wlfc->fifo_credit[TX_AC_BE_FIFO] +
		wlfc->fifo_credit[TX_AC_BK_FIFO] +
		wlfc->fifo_credit[TX_AC_VI_FIFO] +
		wlfc->fifo_credit[TX_AC_VO_FIFO] +
		wlfc->fifo_credit[TX_BCMC_FIFO];
	wlfc->wlfc_trigger = tunables->wlfc_trigger;
	wlfc->wlfc_fifo_bo_cr_ratio = tunables->wlfc_fifo_bo_cr_ratio;
	wlfc->wlfc_comp_txstatus_thresh = tunables->wlfc_comp_txstatus_thresh;

	/* Enable compressed txstatus by default */
	wlfc->comp_stat_enab = TRUE;

	if (!BCMPCIEDEV_ENAB()) {
		wlc_eventq_set_ind(wlc->eventq, WLC_E_FIFO_CREDIT_MAP, TRUE);
		wlc_eventq_set_ind(wlc->eventq, WLC_E_BCMC_CREDIT_SUPPORT, TRUE);
	}

#ifdef PROP_TXSTATUS_DEBUG
	wlc->wlfc_flags = WLFC_FLAGS_RSSI_SIGNALS | WLFC_FLAGS_XONXOFF_SIGNALS |
		WLFC_FLAGS_CREDIT_STATUS_SIGNALS;
#else
	/* All TLV s are turned off by default */
	wlc->wlfc_flags = 0;
#endif // endif

	wlc->pub->_proptxstatus = TRUE;

	return wlfc;

fail:
	MODULE_DETACH(wlfc, wlc_wlfc_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_wlfc_detach)(wlc_wlfc_info_t *wlfc)
{
	wlc_info_t *wlc;

	if (wlfc == NULL) {
		return;
	}

	wlc = wlfc->wlc;

#ifdef PROP_TXSTATUS_DEBUG
	if (wlfc->dbgstats != NULL) {
		MFREE(wlc->osh, wlfc->dbgstats, sizeof(wlfc_fw_dbgstats_t));
	}
#endif // endif
	if (wlfc->fctimer != NULL) {
		wl_free_timer(wlc->wl, wlfc->fctimer);
	}

	if (wl_if_event_send_cb_fn_unregister(wlfc->wlc->wl, wlc_wlfc_send_if_event, wlfc)) {
		WL_ERROR(("wl%d: %s: unable to register if_event callback function \n",
				wlc->pub->unit, __FUNCTION__));
	}
	wlc_module_unregister(wlc->pub, "wlfc", wlfc);

	(void)wlc_scb_state_upd_unregister(wlc, wlc_wlfc_scb_state_upd, wlfc);

	MFREE(wlc->osh, wlfc, sizeof(*wlfc));
}

void
wlc_wlfc_dotxstatus(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	void *pkt, tx_status_t *txs, bool pps_retry)
{
	if (!SCB_ISMULTI(scb)) {
		uint8 wlfc_status = wlc_txstatus_interpret(&txs->status, txs->status.was_acked);
#ifdef PSPRETEND
		if (pps_retry && (wlfc_status == WLFC_CTL_PKTFLAG_DISCARD_NOACK)) {
			wlfc_status = WLFC_CTL_PKTFLAG_D11SUPPRESS;
		}
#endif /* PSPRETEND */

		if (wlfc_status == WLFC_CTL_PKTFLAG_D11SUPPRESS) {
			wlc_pkttag_t *pkttag = WLPKTTAG(pkt);
			BCM_REFERENCE(pkttag);
			if (!WLPKTFLAG_AMPDU(pkttag)) {
				if (WLFC_CONTROL_SIGNALS_TO_HOST_ENAB(wlc->pub))
					wlc_suppress_sync_fsm(wlc, scb, pkt, TRUE);
				wlc_process_wlhdr_txstatus(wlc, WLFC_CTL_PKTFLAG_WLSUPPRESS,
					pkt, FALSE);
			}

#if defined(WL_BSSCFG_TX_SUPR) && defined(WLP2P)
			if (P2P_ABS_SUPR(wlc, txs->status.suppr_ind))  {
				/* We have ABS_NACK for GO/GC which is _NOT_IN_ABS yet
				 * flush pkts and stop tx
				 */
				if (cfg && !BSS_TX_SUPR(cfg)) {
					if (wlc_p2p_noa_valid(wlc->p2p, cfg) ||
					    wlc_p2p_ops_valid(wlc->p2p, cfg))
						wlc_wlfc_interface_state_update(wlc, cfg,
							WLFC_CTL_TYPE_INTERFACE_CLOSE);
					wlc_wlfc_flush_pkts_to_host(wlc, cfg);
				}
			}
#endif /* WL_BSSCFG_TX_SUPR && WLP2P */
		} else {
			wlc_process_wlhdr_txstatus(wlc, wlfc_status, pkt, FALSE);
		}
	}
}
static int16
wlc_flow_ring_get_maxpkts_for_link(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *da)
{
	struct scb *scb;
	uint16  rate = 0xff;
	ratespec_t rspec;
	scb = wlc_scbfind_from_wlcif(wlc, wlcif, da);

	if (!scb || !scb->bsscfg) {
		return BCME_ERROR;
	}

	/* Get the highest rate supported by scb */
	rspec = wlc_get_current_highest_rate(scb->bsscfg);
	rate = RSPEC2KBPS(rspec) / 500;

	return rate;
}
