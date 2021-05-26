/*
 * MSDU aggregation protocol source file
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_amsdu.c 788815 2020-07-13 10:02:35Z $
 */

/**
 * C preprocessor flags used within this file:
 * WLAMSDU       : if defined, enables support for AMSDU reception
 * WLAMSDU_TX    : if defined, enables support for AMSDU transmission
 * BCMDBG_AMSDU  : enable AMSDU debug/dump facilities
 * PKTC          : packet chaining support for NIC driver model
 * PKTC_DONGLE   : packet chaining support for dongle firmware
 * WLOVERTHRUSTER: TCP throughput enhancing feature
 * PROP_TXSTATUS : transmit flow control related feature
 * HNDCTF        : Cut Through Forwarding, router specific feature
 * BCM_GMAC3     : Atlas (4709) router chip specific Ethernet interface
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#if !defined(WLAMSDU) && !defined(WLAMSDU_TX)
#error "Neither WLAMSDU nor WLAMSDU_TX is defined"
#endif // endif

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.1d.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_frmutil.h>
#include <wlc_pcb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_rate_sel.h>
#include <wlc_amsdu.h>
#ifdef PROP_TXSTATUS
#include <wlc_wlfc.h>
#endif // endif
#include <wlc_ampdu.h>
#if defined(WLAMPDU) && defined(WLOVERTHRUSTER)
/* headers to enable TCP ACK bypass for Overthruster */
#include <wlc_ampdu.h>
#include <ethernet.h>
#include <bcmip.h>
#include <bcmtcp.h>
#endif /* WLAMPDU && WLOVERTHRUSTER */
#ifdef PSTA
#include <wlc_psta.h>
#endif // endif
#ifdef WL11AC
#include <wlc_vht.h>
#endif /* WL11AC */
#include <wlc_ht.h>
#include <wlc_tx.h>
#include <wlc_rx.h>
#include <wlc_bmac.h>
#include <wlc_txmod.h>
#include <wlc_pktc.h>
#include <wlc_dump.h>
#include <wlc_hw.h>
#include <wlc_ratelinkmem.h>

#ifdef DONGLEBUILD
#include <hnd_cplt.h>
#endif // endif
#if defined(BCMSPLITRX)
#include <wlc_pktfetch.h>
#endif // endif
#include <d11_cfg.h>
#include <wlc_log.h>

#ifdef WL_MU_RX
#include <wlc_murx.h>
#endif /* WL_MU_RX */

#ifdef PKTC_TBL
#include <wl_pktc.h>
#endif // endif

#ifdef WLCFP
#include <wlc_cfp.h>
#endif /* WLCFP */
/*
 * A-MSDU agg flow control
 * if txpend/scb/prio >= WLC_AMSDU_HIGH_WATERMARK, start aggregating
 * if txpend/scb/prio <= WLC_AMSDU_LOW_WATERMARK, stop aggregating
 */
#define WLC_AMSDU_LOW_WATERMARK           1
#define WLC_AMSDU_HIGH_WATERMARK          1

/* default values for tunables, iovars */

#define CCKOFDM_PLCP_MPDU_MAX_LENGTH      8096  /**< max MPDU len: 13 bit len field in PLCP */
#define AMSDU_MAX_MSDU_PKTLEN             VHT_MAX_AMSDU	/**< max pkt length to be aggregated */
#define AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB  1	    /**< use ht agg limits for vht */

#define AMSDU_AGGBYTES_MIN                500   /**< the lowest aggbytes allowed */
#define MAX_TX_SUBFRAMES_LIMIT            16    /**< the highest aggsf allowed */

#define MAX_RX_SUBFRAMES                  100  /**< max A-MSDU rx size/smallest frame bytes */
#ifndef BCMSIM
#define MAX_TX_SUBFRAMES                  14   /**< max num of MSDUs in one A-MSDU */
#else
#define MAX_TX_SUBFRAMES                  5    /**< for linuxsim testing */
#endif // endif
#define AMSDU_RX_SUBFRAMES_BINS           8    /**< number of counters for amsdu subframes */

#ifndef MAX_TX_SUBFRAMES_ACPHY
#define MAX_TX_SUBFRAMES_ACPHY            2    /**< max num of MSDUs in one A-MSDU */
#endif // endif

/* Statistics */

/* Number of length bins in length histogram */
#ifdef WL11AC
#define AMSDU_LENGTH_BINS                 12
#else
#define AMSDU_LENGTH_BINS                 8
#endif // endif

/* Number of bytes in length represented by each bin in length histogram */
#define AMSDU_LENGTH_BIN_BYTES            1024

/* SW RX private states */
#define WLC_AMSDU_DEAGG_IDLE              0    /**< idle */
#define WLC_AMSDU_DEAGG_FIRST             1    /**< deagg first frame received */
#define WLC_AMSDU_DEAGG_LAST              3    /**< deagg last frame received */

#ifdef WLCNT
#define	WLC_AMSDU_CNT_VERSION	3	/**< current version of wlc_amsdu_cnt_t */

/** block ack related stats */
typedef struct wlc_amsdu_cnt {
	/* Transmit aggregation counters */
	uint16 version;               /**< WLC_AMSDU_CNT_VERSION */
	uint16 length;                /**< length of entire structure */

	uint32 agg_openfail;          /**< num of MSDU open failure */
	uint32 agg_passthrough;       /**< num of MSDU pass through w/o A-MSDU agg */
	uint32 agg_block;             /**< num of MSDU blocked in A-MSDU agg */
	uint32 agg_amsdu;             /**< num of A-MSDU released */
	uint32 agg_msdu;              /**< num of MSDU aggregated in A-MSDU */
	uint32 agg_stop_tailroom;     /**< num of MSDU aggs stopped for lack of tailroom */
	uint32 agg_stop_sf;           /**< num of MSDU aggs stopped for sub-frame count limit */
	uint32 agg_stop_len;          /**< num of MSDU aggs stopped for byte length limit */
	uint32 agg_stop_passthrough;  /**< num of MSDU aggs stopped for un-aggregated frame */
	uint32 agg_stop_tcpack;       /**< num of MSDU aggs stopped for encountering a TCP ACK */
	uint32 agg_stop_suppr;        /**< num of MSDU aggs stopped for suppressed packets */

	/* NEW_TXQ */
	uint32	agg_stop_lowwm;      /**< num of MSDU aggs stopped for tx low water mark */

	/* Receive Deaggregation counters */
	uint32 deagg_msdu;           /**< MSDU of deagged A-MSDU(in ucode) */
	uint32 deagg_amsdu;          /**< valid A-MSDU deagged(exclude bad A-MSDU) */
	uint32 deagg_badfmt;         /**< MPDU is bad */
	uint32 deagg_wrongseq;       /**< MPDU of one A-MSDU doesn't follow sequence */
	uint32 deagg_badsflen;       /**< MPDU of one A-MSDU has length mismatch */
	uint32 deagg_badsfalign;     /**< MPDU of one A-MSDU is not aligned to 4 byte boundary */
	uint32 deagg_badtotlen;      /**< A-MSDU tot length doesn't match summation of all sfs */
	uint32 deagg_openfail;       /**< A-MSDU deagg open failures */
	uint32 deagg_swdeagglong;    /**< A-MSDU sw_deagg doesn't handle long pkt */
	uint32 deagg_flush;          /**< A-MSDU deagg flush; deagg errors may result in this */
	uint32 tx_pkt_free_ignored;  /**< tx pkt free ignored due to invalid scb or !amsdutx */
	uint32 tx_padding_in_tail;   /**< 4Byte pad was placed in tail of packet */
	uint32 tx_padding_in_head;   /**< 4Byte pad was placed in head of packet */
	uint32 tx_padding_no_pad;    /**< 4Byte pad was not needed (4B aligned or last in agg) */
	uint32	agg_amsdu_bytes_l;	 /**< num of total msdu bytes successfully transmitted */
	uint32	agg_amsdu_bytes_h;
	uint32	deagg_amsdu_bytes_l; /**< AMSDU bytes deagg successfully */
	uint32	deagg_amsdu_bytes_h;
} wlc_amsdu_cnt_t;
#endif	/* WLCNT */

typedef struct {
	/* tx counters */
	uint32 tx_msdu_histogram[MAX_TX_SUBFRAMES_LIMIT]; /**< mpdus per amsdu histogram */
	uint32 tx_length_histogram[AMSDU_LENGTH_BINS]; /**< amsdu length histogram */
	/* rx counters */
	uint32 rx_msdu_histogram[AMSDU_RX_SUBFRAMES_BINS]; /**< mpdu per amsdu rx */
	uint32 rx_length_histogram[AMSDU_LENGTH_BINS]; /**< amsdu rx length */
} amsdu_dbg_t;

/** iovar table */
enum {
	IOV_AMSDU_SIM,

	IOV_AMSDU_HIWM,       /**< Packet release highwater mark */
	IOV_AMSDU_LOWM,       /**< Packet release lowwater mark */
	IOV_AMSDU_AGGSF,      /**< num of subframes in one A-MSDU for all tids */
	IOV_AMSDU_AGGBYTES,   /**< num of bytes in one A-MSDU for all tids */

	IOV_AMSDU_RXMAX,      /**< get/set HT_CAP_MAX_AMSDU in HT cap field */
	IOV_AMSDU_BLOCK,      /**< block amsdu agg */
	IOV_AMSDU_FLUSH,      /**< flush all amsdu agg queues */
	IOV_AMSDU_DEAGGDUMP,  /**< dump deagg pkt */
	IOV_AMSDU_COUNTERS,   /**< dump A-MSDU counters */
	IOV_AMSDUNOACK,
	IOV_AMSDU,            /**< Enable/disable A-MSDU, GET returns current state */
	IOV_RX_AMSDU_IN_AMPDU,
	IOV_VAP_AMSDU,
	IOV_VAP_AMSDU_TX_ENABLE,
	IOV_VAP_AMSDU_RX_MAX,
	IOV_VAP_RX_AMSDU_IN_AMPDU,
	IOV_VAP_RX_AMSDU_HWDAGG_DIS,
	IOV_AMSDU_TID              /* enable/disable per-tid amsdu */
};

/* Policy of allowed AMSDU priority items */
static const bool amsdu_scb_txpolicy[NUMPRIO] = {
	TRUE,  /* 0 BE - Best-effortBest-effort */
	FALSE, /* 1 BK - Background */
	FALSE, /* 2 None = - */
	FALSE, /* 3 EE - Excellent-effort */
	FALSE, /* 4 CL - Controlled Load */
	FALSE, /* 5 VI - Video */
	FALSE, /* 6 VO - Voice */
	FALSE, /* 7 NC - Network Control */
};

static const bcm_iovar_t amsdu_iovars[] = {
	{"amsdu", IOV_AMSDU, (IOVF_SET_DOWN), 0, IOVT_BOOL, 0},
	{"rx_amsdu_in_ampdu", IOV_RX_AMSDU_IN_AMPDU, (0), 0, IOVT_BOOL, 0},
	{"amsdu_noack", IOV_AMSDUNOACK, (0), 0, IOVT_BOOL, 0},
	{"amsdu_aggsf", IOV_AMSDU_AGGSF, (0), 0, IOVT_UINT16, 0},
	{"amsdu_aggbytes", IOV_AMSDU_AGGBYTES, (0), 0, IOVT_UINT32, 0},
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	{"amsdu_aggblock", IOV_AMSDU_BLOCK, (0), 0, IOVT_BOOL, 0},
#ifdef WLCNT
	{"amsdu_counters", IOV_AMSDU_COUNTERS, (0), 0, IOVT_BUFFER, sizeof(wlc_amsdu_cnt_t)},
#endif /* WLCNT */
#endif /* BCMDBG */
	{"amsdu_tid", IOV_AMSDU_TID, (0), 0, IOVT_BUFFER, sizeof(struct amsdu_tid_control)},
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct amsdu_deagg {
	int amsdu_deagg_state;     /**< A-MSDU deagg statemachine per device */
	void *amsdu_deagg_p;       /**< pointer to first pkt buffer in A-MSDU chain */
	void *amsdu_deagg_ptail;   /**< pointer to last pkt buffer in A-MSDU chain */
	uint16  first_pad;         /**< front padding bytes of A-MSDU first sub frame */
	bool chainable;
} amsdu_deagg_t;

/** default/global settings for A-MSDU module */
typedef struct amsdu_ami_policy {
	uint16 fifo_lowm;			/**< low watermark for tx queue precendence */
	uint16 fifo_hiwm;			/**< high watermark for tx queue precendence */
	bool amsdu_agg_enable[NUMPRIO];		/**< TRUE:agg allowed, FALSE:agg disallowed */
	uint amsdu_max_agg_bytes[NUMPRIO];	/**< Maximum allowed payload bytes per A-MSDU */
	uint8 amsdu_max_sframes;		/**< Maximum allowed subframes per A-MSDU */
} amsdu_ami_policy_t;

/** principal amsdu module local structure per device instance */
struct amsdu_info {
	wlc_info_t *wlc;             /**< pointer to main wlc structure */
	wlc_pub_t *pub;              /**< public common code handler */
	int scb_handle;              /**< scb cubby handle to retrieve data from scb */

	/* RX datapath bits */
	uint mac_rcvfifo_limit;    /**< max rx fifo in bytes */
	uint amsdu_rx_mtu;         /**< amsdu MTU, depend on rx fifo limit */
	bool amsdu_rxcap_big;        /**< TRUE: rx big amsdu capable (HT_MAX_AMSDU) */
	/* rx: streams per device */
	amsdu_deagg_t *amsdu_deagg;  /**< A-MSDU deagg */

	/* TX datapath bits */
	bool amsdu_agg_block;        /**< global override: disable amsdu tx */
	amsdu_ami_policy_t txpolicy; /**< Default ami release policy per prio */

	bool amsdu_deagg_pkt;        /**< dump deagg pkt BCMINTERNAL */
	wlc_amsdu_cnt_t *cnt;        /**< counters/stats WLCNT */
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	amsdu_dbg_t *amdbg;
#endif // endif
};

/* Per-prio SCB A-MSDU policy variables */
typedef struct {
	/*
	 * Below are the private limits as negotiated during association
	 */
	/** max (V)HT AMSDU bytes (negotiated) that remote node can receive */
	uint amsdu_agg_bytes_max;
	bool amsdu_agg_enable;		/**< TRUE:agg allowed, FALSE:agg disallowed */
} amsdu_scb_txpolicy_t;

typedef struct {
	uint amsdu_agg_bytes;      /**< A-MSDU byte count */
	void *amsdu_agg_p;         /**< A-MSDU pkt pointer to first MSDU */
	void *amsdu_agg_ptail;     /**< A-MSDU pkt pointer to last MSDU */
	uint amsdu_agg_padlast;    /**< pad bytes in the agg tail buffer */
	/** # of transmit AMSDUs forwarded to the next software layer but not yet returned */
	uint amsdu_agg_txpending;
	uint8 amsdu_agg_sframes;   /**< A-MSDU max allowed subframe count */
	uint8 headroom_pad_need;   /**< # of bytes (0-3) need fr headroom for pad prev pkt */
} amsdu_txaggstate_t;

/** per scb cubby info */
typedef struct scb_amsduinfo {
	/* NEW_TXQ buffers the frames in A-MSDU aggregate, this is the state metadata */
	amsdu_txaggstate_t aggstate[NUMPRIO];

	/*
	 * This contains the default attach time values as well as any
	 * protocol dependent limits.
	 */
	amsdu_scb_txpolicy_t scb_txpolicy[NUMPRIO];
	uint16 mtu_pref;
	/** Maximum allowed number of subframes per A-MSDU that remote node can receive */
	uint8 amsdu_aggsf;
	uint8 amsdu_max_sframes;	/**< Maximum allowed subframes per A-MSDU */
	/** set when ratesel mem was updated, max tx amsdu agg len then has to be (re)determined */
	bool agglimit_upd;
} scb_amsdu_t;

#define SCB_AMSDU_CUBBY(ami, scb) (scb_amsdu_t *)SCB_CUBBY((scb), (ami)->scb_handle)

/* A-MSDU general */
static int wlc_amsdu_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *arg, uint alen, uint val_size, struct wlc_if *wlcif);

static void wlc_amsdu_mtu_init(amsdu_info_t *ami);
static int wlc_amsdu_down(void *hdl);
static int wlc_amsdu_up(void *hdl);

#ifdef WLAMSDU_TX

static void wlc_amsdu_tx_dotxstatus(amsdu_info_t *ami, struct scb *scb, void *pkt);
static uint32 wlc_amsdu_tx_scb_max_agg_len_upd(amsdu_info_t *ami, struct scb *scb);
static void wlc_amsdu_tx_scb_agglimit_upd(amsdu_info_t *ami, struct scb *scb);
#ifndef WLAMSDU_TX_DISABLED
static int wlc_amsdu_tx_scb_init(void *cubby, struct scb *scb);
static void wlc_amsdu_tx_scb_deinit(void *cubby, struct scb *scb);
static void wlc_amsdu_tx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
static void wlc_amsdu_tx_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_amsdu_tx_dump_scb NULL
#endif // endif

#endif /* WLAMSDU_TX_DISABLED */

/* A-MSDU aggregation */
static void* wlc_amsdu_agg_open(amsdu_info_t *ami, wlc_bsscfg_t *bsscfg,
	struct ether_addr *ea, void *p);
static bool  wlc_amsdu_agg_append(amsdu_info_t *ami, struct scb *scb, void *p,
	uint tid, ratespec_t rspec);
static void  wlc_amsdu_agg_close(amsdu_info_t *ami, struct scb *scb, uint tid);

#if defined(WLOVERTHRUSTER)
static bool wlc_amsdu_is_tcp_ack(amsdu_info_t *ami, void *p);
#endif // endif

static void wlc_amsdu_agg(void *ctx, struct scb *scb, void *p, uint prec);
static void wlc_amsdu_scb_deactive(void *ctx, struct scb *scb);
static uint wlc_amsdu_txpktcnt(void *ctx);
static void wlc_amsdu_flush_pkts(amsdu_info_t *ami, struct scb *scb, uint8 tid);
static void wlc_amsdu_flush_scb_tid(void* ctx, struct scb *scb, uint8 tid);

static txmod_fns_t BCMATTACHDATA(amsdu_txmod_fns) = {
	wlc_amsdu_agg,
	wlc_amsdu_txpktcnt,
	wlc_amsdu_flush_scb_tid,
	wlc_amsdu_scb_deactive,
	NULL
};
#endif /* WLAMSDU_TX */

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
static int wlc_amsdu_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* A-MSDU deaggregation */
static bool wlc_amsdu_deagg_open(amsdu_info_t *ami, int fifo, void *p,
	struct dot11_header *h, uint32 pktlen);
static bool wlc_amsdu_deagg_verify(amsdu_info_t *ami, uint16 fc, void *h);
static void wlc_amsdu_deagg_flush(amsdu_info_t *ami, int fifo);
static int wlc_amsdu_tx_attach(amsdu_info_t *ami, wlc_info_t *wlc);

#if (defined(BCMDBG) || defined(BCMDBG_AMSDU)) && defined(WLCNT)
void wlc_amsdu_dump_cnt(amsdu_info_t *ami, struct bcmstrbuf *b);
#endif	/* defined(BCMDBG) && defined(WLCNT) */

#ifdef WL11K_ALL_MEAS
void wlc_amsdu_get_stats(wlc_info_t *wlc, rrm_stat_group_11_t *g11)
{
	ASSERT(wlc);
	ASSERT(g11);

	g11->txamsdu = wlc->ami->cnt->agg_amsdu;
	g11->amsdufail = wlc->ami->cnt->agg_openfail + wlc->ami->cnt->deagg_openfail;
	g11->amsduretry = 0; /* Not supported */
	g11->amsduretries = 0; /* Not supported */
	g11->txamsdubyte_h = wlc->ami->cnt->agg_amsdu_bytes_h;
	g11->txamsdubyte_l = wlc->ami->cnt->agg_amsdu_bytes_l;
	g11->amsduackfail = wlc->_amsdu_noack;
	g11->rxamsdu = wlc->ami->cnt->deagg_amsdu;
	g11->rxamsdubyte_h = wlc->ami->cnt->deagg_amsdu_bytes_h;
	g11->rxamsdubyte_l = wlc->ami->cnt->deagg_amsdu_bytes_l;
}
#endif /* WL11K_ALL_MEAS */

#ifdef WLAMSDU_TX

static void
wlc_amsdu_set_scb_default_txpolicy(amsdu_info_t *ami, scb_amsdu_t *scb_ami)
{
	uint i;

	scb_ami->amsdu_max_sframes = ami->txpolicy.amsdu_max_sframes;

	for (i = 0; i < NUMPRIO; i++) {
		amsdu_scb_txpolicy_t *scb_txpolicy = &scb_ami->scb_txpolicy[i];

		scb_txpolicy->amsdu_agg_enable = ami->txpolicy.amsdu_agg_enable[i];
	}
}

/** called by IOV */
static void
wlc_amsdu_set_scb_default_txpolicy_all(amsdu_info_t *ami)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(ami->wlc->scbstate, &scbiter, scb) {
		wlc_amsdu_set_scb_default_txpolicy(ami, SCB_AMSDU_CUBBY(ami, scb));
		wlc_amsdu_tx_scb_aggsf_upd(ami, scb);
	}

	/* Update frag threshold on A-MSDU parameter changes */
	wlc_amsdu_agglimit_frag_upd(ami);
}

/** WLAMSDU_TX specific function, handles callbacks when pkts either tx-ed or freed */
static void BCMFASTPATH
wlc_amsdu_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs)
{
	BCM_REFERENCE(txs);

	if (AMSDU_TX_ENAB(wlc->pub)) {
		int err;
		struct scb *scb = NULL;
		amsdu_info_t *ami = wlc->ami;
		wlc_bsscfg_t *bsscfg = wlc_bsscfg_find(wlc, WLPKTTAGBSSCFGGET(pkt), &err);
		/* if bsscfg or scb are stale or bad, then ignore this pkt for acctg purposes */
		if (!err && bsscfg) {
			scb = WLPKTTAGSCBGET(pkt);
			if (scb && SCB_AMSDU(scb)) {
				amsdu_info_t *scb_ami = wlc->ami;
				wlc_amsdu_tx_dotxstatus(scb_ami, scb, pkt);
			} else {
				WL_AMSDU(("wl%d:%s: not count scb(%p) pkts\n",
					wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
#ifdef WLCNT
				WLCNTINCR(ami->cnt->tx_pkt_free_ignored);
#endif /* WLCNT */
			}
		} else {
			WL_AMSDU(("wl%d:%s: not count bsscfg (%p) pkts\n",
				wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(bsscfg)));
#ifdef WLCNT
			WLCNTINCR(wlc->ami->cnt->tx_pkt_free_ignored);
#endif /* WLCNT */
		}
	}
	/* callback is cleared by default by calling function */
}
#endif /* WLAMSDU_TX */

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
#ifdef WLCNT
static int
wlc_amsdu_dump_clr(void *ctx)
{
	amsdu_info_t *ami = ctx;

	bzero(ami->cnt, sizeof(*ami->cnt));
	bzero(ami->amdbg, sizeof(*ami->amdbg));

	return BCME_OK;
}
#else
#define wlc_amsdu_dump_clr NULL
#endif // endif
#endif // endif

/*
 * This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/** attach function for receive AMSDU support */
amsdu_info_t *
BCMATTACHFN(wlc_amsdu_attach)(wlc_info_t *wlc)
{
	amsdu_info_t *ami;
	if (!(ami = (amsdu_info_t *)MALLOCZ(wlc->osh, sizeof(amsdu_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	ami->wlc = wlc;
	ami->pub = wlc->pub;

	if (!(ami->amsdu_deagg = (amsdu_deagg_t *)MALLOCZ(wlc->osh,
		sizeof(amsdu_deagg_t) * RX_FIFO_NUMBER))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

#ifdef WLCNT
	if (!(ami->cnt = (wlc_amsdu_cnt_t *)MALLOCZ(wlc->osh, sizeof(wlc_amsdu_cnt_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif /* WLCNT */

	/* register module */
	if (wlc_module_register(ami->pub, amsdu_iovars, "amsdu", ami, wlc_amsdu_doiovar,
		NULL, wlc_amsdu_up, wlc_amsdu_down)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	if (!(ami->amdbg = (amsdu_dbg_t *)MALLOCZ(wlc->osh, sizeof(amsdu_dbg_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	wlc_dump_add_fns(ami->pub, "amsdu", wlc_amsdu_dump, wlc_amsdu_dump_clr, ami);
#endif // endif

	ami->txpolicy.fifo_lowm = (uint16)WLC_AMSDU_LOW_WATERMARK;
	ami->txpolicy.fifo_hiwm = (uint16)WLC_AMSDU_HIGH_WATERMARK;

	if (wlc_amsdu_tx_attach(ami, wlc) < 0) {
		WL_ERROR(("wl%d: %s: Error initing the amsdu tx\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc_amsdu_mtu_init(ami);

	/* to be compatible with spec limit */
	if (wlc->pub->tunables->nrxd < MAX_RX_SUBFRAMES) {
		WL_ERROR(("NRXD %d is too small to fit max amsdu rxframe\n",
		          (uint)wlc->pub->tunables->nrxd));
	}

	return ami;
fail:
	MODULE_DETACH(ami, wlc_amsdu_detach);
	return NULL;
} /* wlc_amsdu_attach */

/** attach function for transmit AMSDU support */
static int
BCMATTACHFN(wlc_amsdu_tx_attach)(amsdu_info_t *ami, wlc_info_t *wlc)
{
#ifdef WLAMSDU_TX
#ifndef WLAMSDU_TX_DISABLED
	uint i;
	uint max_agg;
	scb_cubby_params_t cubby_params;
	int err;

	if (WLCISACPHY(wlc->band)) {
#if AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB
		max_agg = HT_MAX_AMSDU;
#else
		max_agg = VHT_MAX_AMSDU;
#endif /* AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB */
	} else {
		max_agg = HT_MAX_AMSDU;
	}

	/* register packet class callback */
	if ((err = wlc_pcb_fn_set(wlc->pcb, 2, WLF2_PCB3_AMSDU, wlc_amsdu_pkt_freed)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_pcb_fn_set err=%d\n", wlc->pub->unit, __FUNCTION__, err));
		return -1;
	}

	/* reserve cubby in the scb container for per-scb private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = ami;
	cubby_params.fn_init = wlc_amsdu_tx_scb_init;
	cubby_params.fn_deinit = wlc_amsdu_tx_scb_deinit;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	cubby_params.fn_dump = wlc_amsdu_tx_dump_scb;
#endif // endif
	ami->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_amsdu_t), &cubby_params);

	if (ami->scb_handle < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	/* register txmod call back */
	wlc_txmod_fn_register(wlc->txmodi, TXMOD_AMSDU, ami, amsdu_txmod_fns);

	/* Add client callback to the scb state notification list */
	if ((err = wlc_scb_state_upd_register(wlc, wlc_amsdu_tx_scb_state_upd, ami)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
		          wlc->pub->unit, __FUNCTION__, wlc_amsdu_tx_scb_state_upd));

		return -1;
	}

	WLCNTSET(ami->cnt->version, WLC_AMSDU_CNT_VERSION);
	WLCNTSET(ami->cnt->length, sizeof(*(ami->cnt)));

	/* init tunables */

	if (WLCISACPHY(wlc->band)) {
		ami->txpolicy.amsdu_max_sframes = MAX_TX_SUBFRAMES_ACPHY;
	} else {
		ami->txpolicy.amsdu_max_sframes = MAX_TX_SUBFRAMES;
	}

	/* DMA: leave empty room for DMA descriptor table */
	if (ami->txpolicy.amsdu_max_sframes >
		(uint)(wlc->pub->tunables->ntxd/3)) {
		WL_ERROR(("NTXD %d is too small to fit max amsdu txframe\n",
			(uint)wlc->pub->tunables->ntxd));
		ASSERT(0);
	}

#if defined(BCMHWA)
	HWA_PKTPGR_EXPR({
		if ((hwa_pktpgr_multi_txlfrag(hwa_dev) == 0) &&
			(ami->txpolicy.amsdu_max_sframes > 4)) {
			WL_ERROR(("%s: Cannot support more then %d subframes in one A-MSDU\n",
				__FUNCTION__, ami->txpolicy.amsdu_max_sframes));
			ami->txpolicy.amsdu_max_sframes = 4;
		}
	});
#endif // endif

	for (i = 0; i < NUMPRIO; i++) {
		uint fifo_size;

		ami->txpolicy.amsdu_agg_enable[i] = amsdu_scb_txpolicy[i];

		/* set agg_bytes_limit to standard maximum if hw fifo allows
		 *  this value can be changed via iovar or fragthreshold later
		 *  it can never exceed hw fifo limit since A-MSDU is not streaming
		 */
		fifo_size = wlc->xmtfifo_szh[prio2fifo[i]];
		/* blocks to bytes */
		fifo_size = fifo_size * TXFIFO_SIZE_UNIT(wlc->pub->corerev);
		ami->txpolicy.amsdu_max_agg_bytes[i] = MIN(max_agg, fifo_size);
		/* TODO: PIO */
	}

	wlc->pub->_amsdu_tx_support = TRUE;
#else
	/* Reference this to prevent Coverity erors */
	BCM_REFERENCE(amsdu_txmod_fns);
#endif /* WLAMSDU_TX_DISABLED */
#endif /* WLAMSDU_TX */

	return 0;
} /* wlc_amsdu_tx_attach */

void
BCMATTACHFN(wlc_amsdu_detach)(amsdu_info_t *ami)
{
	if (!ami)
		return;

	wlc_amsdu_down(ami);

	wlc_module_unregister(ami->pub, "amsdu", ami);

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	if (ami->amdbg) {
		MFREE(ami->pub->osh, ami->amdbg, sizeof(amsdu_dbg_t));
		ami->amdbg = NULL;
	}
#endif // endif

#ifdef WLCNT
	if (ami->cnt)
		MFREE(ami->pub->osh, ami->cnt, sizeof(wlc_amsdu_cnt_t));
#endif /* WLCNT */
	MFREE(ami->pub->osh, ami->amsdu_deagg, sizeof(amsdu_deagg_t) * RX_FIFO_NUMBER);
	MFREE(ami->pub->osh, ami, sizeof(amsdu_info_t));
}

#ifndef DONGLEBUILD
static uint
wlc_rcvfifo_limit_get(wlc_info_t *wlc)
{
	uint rcvfifo;

	/* determine rx fifo. no register/shm, it's hardwired in RTL */

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		rcvfifo = wlc_bmac_rxfifosz_get(wlc->hw);
	} else if (D11REV_GE(wlc->pub->corerev, 22)) {
		/* XXX it should have been D11REV_GE(wlc->pub->corerev, 16) but all chips
		 * with revid older than 22 are EOL'd so start from revid 22.
		 */
		rcvfifo = ((wlc->machwcap & MCAP_RXFSZ_MASK) >> MCAP_RXFSZ_SHIFT) * 512;
	} else {
		/* EOL'd chips or chips that don't exist yet */
		ASSERT(0);
		return 0;
	}
	return rcvfifo;
} /* wlc_rcvfifo_limit_get */
#endif /* !DONGLEBUILD */

static void
BCMATTACHFN(wlc_amsdu_mtu_init)(amsdu_info_t *ami)
{
#ifdef DONGLEBUILD
	ami->amsdu_rxcap_big = FALSE;
#else /* DONGLEBUILD */
	ami->mac_rcvfifo_limit = wlc_rcvfifo_limit_get(ami->wlc);
	if (D11REV_GE(ami->wlc->pub->corerev, 31) && D11REV_LE(ami->wlc->pub->corerev, 38))
		ami->amsdu_rxcap_big = FALSE;
	else
		ami->amsdu_rxcap_big =
		        ((int)(ami->mac_rcvfifo_limit - ami->wlc->hwrxoff - 100) >= HT_MAX_AMSDU);
#endif /* DONGLEBUILD */

	ami->amsdu_rx_mtu = ami->amsdu_rxcap_big ? HT_MAX_AMSDU : HT_MIN_AMSDU;

	/* For A/C enabled chips only */
	if (WLCISACPHY(ami->wlc->band) &&
	    ami->amsdu_rxcap_big &&
	    ((int)(ami->mac_rcvfifo_limit - ami->wlc->hwrxoff - 100) >= VHT_MAX_AMSDU)) {
		ami->amsdu_rx_mtu = VHT_MAX_AMSDU;
	}
	WL_AMSDU(("%s:ami->amsdu_rx_mtu=%d\n", __FUNCTION__, ami->amsdu_rx_mtu));
}

bool
wlc_amsdu_is_rxmax_valid(amsdu_info_t *ami)
{
	if (wlc_amsdu_mtu_get(ami) < HT_MAX_AMSDU) {
		return TRUE;
	} else {
		return FALSE;
	}
}

uint
wlc_amsdu_mtu_get(amsdu_info_t *ami)
{
	return ami->amsdu_rx_mtu;
}

/** Returns TRUE or FALSE. AMSDU tx is optional, sw can turn it on or off even HW supports */
bool
wlc_amsdu_tx_cap(amsdu_info_t *ami)
{
#if defined(WLAMSDU_TX)
	if (AMSDU_TX_SUPPORT(ami->pub))
		return (TRUE);
#endif // endif
	return (FALSE);
}

/** Returns TRUE or FALSE. AMSDU rx is mandatory for NPHY */
bool
wlc_amsdurx_cap(amsdu_info_t *ami)
{
	return (TRUE);
}

#ifdef WLAMSDU_TX

/** WLAMSDU_TX specific function to enable/disable AMSDU transmit */
int
wlc_amsdu_set(amsdu_info_t *ami, bool on)
{
	wlc_info_t *wlc = ami->wlc;
	bool amsdu_enabled = wlc->pub->_amsdu_tx;

	WL_AMSDU(("wlc_amsdu_set val=%d\n", on));

	if (on) {
		if (!N_ENAB(wlc->pub)) {
			WL_AMSDU(("wl%d: driver not nmode enabled\n", wlc->pub->unit));
			return BCME_UNSUPPORTED;
		}
		if (!wlc_amsdu_tx_cap(ami)) {
			WL_AMSDU(("wl%d: device not amsdu capable\n", wlc->pub->unit));
			return BCME_UNSUPPORTED;
		} else if (AMPDU_ENAB(wlc->pub) &&
		           D11REV_LT(wlc->pub->corerev, 40)) {
			/* AMSDU + AMPDU ok for core-rev 40+ with AQM */
			WL_AMSDU(("wl%d: A-MSDU not supported with AMPDU on d11 rev %d\n",
			          wlc->pub->unit, wlc->pub->corerev));
			return BCME_UNSUPPORTED;
		}
	}

	/* This controls AMSDU agg only, AMSDU deagg is on by default per spec */
	wlc->pub->_amsdu_tx = on;
	wlc_update_brcm_ie(ami->wlc);

	/* tx descriptors should be higher -- AMPDU max when both AMSDU and AMPDU set */
	wlc_set_default_txmaxpkts(wlc);

	if (!amsdu_enabled) {
		wlc_amsdu_agg_flush(ami);
	}

	return (0);
} /* wlc_amsdu_set */

#ifndef WLAMSDU_TX_DISABLED

/** WLAMSDU_TX specific function, initializes each priority for a remote party (scb) */
static int
wlc_amsdu_tx_scb_init(void *context, struct scb *scb)
{
	uint i;
	amsdu_info_t *ami = (amsdu_info_t *)context;
	scb_amsdu_t *scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	amsdu_scb_txpolicy_t *scb_txpolicy;

	WL_AMSDU(("wlc_amsdu_tx_scb_init scb %p\n", OSL_OBFUSCATE_BUF(scb)));

	ASSERT(scb_ami);

	/* Setup A-MSDU SCB policy defaults */
	for (i = 0; i < NUMPRIO; i++) {
		scb_txpolicy = &scb_ami->scb_txpolicy[i];

		memset(scb_txpolicy, 0, sizeof(*scb_txpolicy));

		/*
		 * These are negotiated defaults, this is not a public block
		 * The public info is rebuilt each time an iovar is used which
		 * would destroy the negotiated values here, so this is made private.
		 */
		scb_txpolicy->amsdu_agg_bytes_max = HT_MAX_AMSDU;
	}
	scb_ami->amsdu_aggsf = ami->txpolicy.amsdu_max_sframes;

	/* Init the public part with the global ami defaults */
	wlc_amsdu_set_scb_default_txpolicy(ami, scb_ami);

	return 0;
}

static void
wlc_amsdu_tx_scb_deinit(void *context, struct scb *scb)
{
	amsdu_info_t *ami = (amsdu_info_t *)context;

	WL_AMSDU(("wlc_amsdu_tx_scb_deinit scb %p\n", OSL_OBFUSCATE_BUF(scb)));

	/* release tx agg pkts */
	wlc_amsdu_scb_deactive(ami, scb);
}

/* Callback function invoked when a STA's association state changes.
 * Inputs:
 *   ctx -        amsdu_info_t structure
 *   notif_data - information describing the state change
 */
static void
wlc_amsdu_tx_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	amsdu_info_t *ami = (amsdu_info_t*) ctx;
	scb_t *scb;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);

	if (SCB_ASSOCIATED(scb)) {
		wlc_amsdu_tx_scb_aggsf_upd(ami, scb);
	}
}
#endif /* !WLAMSDU_TX_DISABLED */
#endif /* WLAMSDU_TX */

/** handle AMSDU related items when going down */
static int
wlc_amsdu_down(void *hdl)
{
	int fifo;
	amsdu_info_t *ami = (amsdu_info_t *)hdl;

	WL_AMSDU(("wlc_amsdu_down: entered\n"));

	/* Flush the deagg Q, there may be packets there */
	for (fifo = 0; fifo < RX_FIFO_NUMBER; fifo++)
		wlc_amsdu_deagg_flush(ami, fifo);

#ifdef WLAMSDU_TX
	wlc_amsdu_agg_flush(ami);
#endif /* WLAMSDU_TX */

	return 0;
}

static int
wlc_amsdu_up(void *hdl)
{
	/* limit max size pkt ucode lets through to what we use for dma rx descriptors */
	/* else rx of amsdu can cause dma rx errors and potentially impact performance */
	amsdu_info_t *ami = (amsdu_info_t *)hdl;
	wlc_info_t *wlc = ami->wlc;
	hnddma_t *di;
	uint16 rxbufsz;
	uint16 rxoffset;

	if (!PIO_ENAB(wlc->pub)) {
		di = WLC_HW_DI(wlc, 0);
		dma_rxparam_get(di, &rxoffset, &rxbufsz);
		rxbufsz =  rxbufsz - rxoffset;
	}
	else {
		rxbufsz = (uint16)(wlc->pub->tunables->rxbufsz - wlc->hwrxoff);
	}

	wlc_write_shm(wlc, M_MAXRXFRM_LEN(wlc), (uint16)rxbufsz);
	if (D11REV_GE(wlc->pub->corerev, 65)) {
		W_REG(wlc->osh, D11_DAGG_LEN_THR(wlc), (uint16)rxbufsz);
	}

	return BCME_OK;
}

/** handle AMSDU related iovars */
static int
wlc_amsdu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint val_size, struct wlc_if *wlcif)
{
	amsdu_info_t *ami = (amsdu_info_t *)hdl;
	int32 int_val = 0;
	int32 *pval = (int32 *)a;
	bool bool_val;
	int err = 0;
	wlc_info_t *wlc;
#ifdef WLAMSDU_TX
	uint i; /* BCM_REFERENCE() may be more convienient but we save
		 * 1 stack var if the feature is not used.
		*/
#endif // endif

	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(alen);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = ami->wlc;
	ASSERT(ami == wlc->ami);

	switch (actionid) {
	case IOV_GVAL(IOV_AMSDU):
		int_val = wlc->pub->_amsdu_tx;
		bcopy(&int_val, a, val_size);
		break;

#ifdef WLAMSDU_TX
	case IOV_SVAL(IOV_AMSDU):
		if (AMSDU_TX_SUPPORT(wlc->pub))
			err = wlc_amsdu_set(ami, bool_val);
		else
			err = BCME_UNSUPPORTED;
		break;
#endif // endif

	case IOV_GVAL(IOV_AMSDUNOACK):
		*pval = (int32)wlc->_amsdu_noack;
		break;

	case IOV_SVAL(IOV_AMSDUNOACK):
		wlc->_amsdu_noack = bool_val;
		break;

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	case IOV_GVAL(IOV_AMSDU_BLOCK):
		*pval = (int32)ami->amsdu_agg_block;
		break;

	case IOV_SVAL(IOV_AMSDU_BLOCK):
		ami->amsdu_agg_block = bool_val;
		break;

#ifdef WLCNT
	case IOV_GVAL(IOV_AMSDU_COUNTERS):
		bcopy(&ami->cnt, a, sizeof(ami->cnt));
		break;
#endif /* WLCNT */
#endif /* BCMDBG */

#ifdef WLAMSDU_TX
	case IOV_GVAL(IOV_AMSDU_AGGBYTES):
		if (AMSDU_TX_SUPPORT(wlc->pub)) {
			/* TODO, support all priorities ? */
			*pval = (int32)ami->txpolicy.amsdu_max_agg_bytes[PRIO_8021D_BE];
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_AMSDU_AGGBYTES):
		if (AMSDU_TX_SUPPORT(wlc->pub)) {
			struct scb *scb;
			struct scb_iter scbiter;
			uint32 uint_val = (uint)int_val;

			if (WLCISACPHY(wlc->band) && uint_val > VHT_MAX_AMSDU) {
				err = BCME_RANGE;
				break;
			}
			if (!WLCISACPHY(wlc->band) && (uint_val > (uint)HT_MAX_AMSDU)) {
				err = BCME_RANGE;
				break;
			}

			if (uint_val < AMSDU_AGGBYTES_MIN) {
				err = BCME_RANGE;
				break;
			}

			/* if smaller, flush existing aggregation, care only BE for now */
			if (uint_val < ami->txpolicy.amsdu_max_agg_bytes[PRIO_8021D_BE])
				wlc_amsdu_agg_flush(ami);

			for (i = 0; i < NUMPRIO; i++) {
				uint fifo_size;

				fifo_size = wlc->xmtfifo_szh[prio2fifo[i]];
				/* blocks to bytes */
				fifo_size = fifo_size * TXFIFO_SIZE_UNIT(wlc->pub->corerev);
				ami->txpolicy.amsdu_max_agg_bytes[i] =
						MIN(uint_val, fifo_size);
			}
			/* update amsdu agg bytes for ALL scbs */
			FOREACHSCB(wlc->scbstate, &scbiter, scb) {
				wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
			}
		} else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_GVAL(IOV_AMSDU_AGGSF):
		if (AMSDU_TX_SUPPORT(wlc->pub)) {
			/* TODO, support all priorities ? */
			*pval = (int32)ami->txpolicy.amsdu_max_sframes;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_AMSDU_AGGSF):
		if (AMSDU_TX_SUPPORT(wlc->pub)) {

			if ((int_val > MAX_TX_SUBFRAMES_LIMIT) ||
			    (int_val > wlc->pub->tunables->ntxd/2) ||
			    (int_val < 1)) {
				err = BCME_RANGE;
				break;
			}

#if defined(BCMHWA)
			HWA_PKTPGR_EXPR({
				if ((hwa_pktpgr_multi_txlfrag(hwa_dev) == 0) && (int_val > 4)) {
					WL_ERROR(("%s: Cannot support more then %d subframes "
						"in one A-MSDU\n", __FUNCTION__, int_val));
					err = BCME_RANGE;
					break;
				}
			});
#endif // endif
			ami->txpolicy.amsdu_max_sframes = (uint8)int_val;

			/* Update the scbs */
			wlc_amsdu_set_scb_default_txpolicy_all(ami);
		} else
			err = BCME_UNSUPPORTED;
		break;
#endif /* WLAMSDU_TX */

#ifdef WLAMSDU

	case IOV_GVAL(IOV_RX_AMSDU_IN_AMPDU):
		int_val = (int8)(wlc->_rx_amsdu_in_ampdu);
		bcopy(&int_val, a, val_size);
		break;

	case IOV_SVAL(IOV_RX_AMSDU_IN_AMPDU):
		if (bool_val && D11REV_LT(wlc->pub->corerev, 40)) {
			WL_AMSDU(("wl%d: Not supported < corerev (40)\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
		} else {
			wlc->_rx_amsdu_in_ampdu = bool_val;
		}

		break;
#endif /* WLAMSDU */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
} /* wlc_amsdu_doiovar */

#ifdef WLAMSDU_TX
/**
 * Helper function that updates the per-SCB A-MSDU state.
 *
 * @param ami			A-MSDU module handle.
 * @param scb			scb pointer.
 * @param amsdu_agg_enable[]	array of enable states, one for each priority.
 *
 */
static void
wlc_amsdu_tx_scb_agglimit_upd_all(amsdu_info_t *ami, struct scb *scb, bool amsdu_agg_enable[])
{
	uint i;
	amsdu_scb_txpolicy_t *scb_txpolicy = &(SCB_AMSDU_CUBBY(ami, scb))->scb_txpolicy[0];

	for (i = 0; i < NUMPRIO; i++) {
		scb_txpolicy[i].amsdu_agg_enable = amsdu_agg_enable[i];
	}

	wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
}

/**
 * WLAMSDU_TX specific function.
 * called from fragthresh changes ONLY: update agg bytes limit, toss buffered A-MSDU
 * This is expected to happen very rarely since user should use very standard 802.11 fragthreshold
 *  to "disabled" fragmentation when enable A-MSDU. We can even ignore that. But to be
 *  full spec compliant, we reserve this capability.
 *   ??? how to inform user the requirement that not changing FRAGTHRESHOLD to screw up A-MSDU
 */
void
wlc_amsdu_agglimit_frag_upd(amsdu_info_t *ami)
{
	uint i;
	wlc_info_t *wlc = ami->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	bool flush = FALSE;
	bool frag_disabled = FALSE;
	bool amsdu_agg_enable[NUMPRIO];
	uint16 fragthresh;

	WL_AMSDU(("wlc_amsdu_agg_limit_upd\n"));

	if (!AMSDU_TX_SUPPORT(wlc->pub))
		return;

	for (i = 0; i < NUMPRIO; i++) {
		fragthresh = wlc->fragthresh[WME_PRIO2AC(i)];
		frag_disabled = (fragthresh == DOT11_MAX_FRAG_LEN);

		if (!frag_disabled && (fragthresh < ami->txpolicy.amsdu_max_agg_bytes[i])) {
			flush = TRUE;
			ami->txpolicy.amsdu_max_agg_bytes[i] = fragthresh;

			WL_AMSDU(("wlc_amsdu_agg_frag_upd: amsdu_aggbytes[%d] = %d due to frag!\n",
				i, ami->txpolicy.amsdu_max_agg_bytes[i]));
		} else if (frag_disabled || (fragthresh > ami->txpolicy.amsdu_max_agg_bytes[i])) {
			uint max_agg;
			uint fifo_size;
			if (WLCISACPHY(wlc->band)) {
#if AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB
				max_agg = HT_MAX_AMSDU;
#else
				max_agg = VHT_MAX_AMSDU;
#endif /* AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB */
			} else {
				max_agg = HT_MAX_AMSDU;
			}
			fifo_size = wlc->xmtfifo_szh[prio2fifo[i]];
			/* blocks to bytes */
			fifo_size = fifo_size * TXFIFO_SIZE_UNIT(wlc->pub->corerev);

			if (frag_disabled &&
				ami->txpolicy.amsdu_max_agg_bytes[i] == MIN(max_agg, fifo_size)) {
				/*
				 * Copy the A-MSDU  enable state over to
				 * properly initialize amsdu_agg_enable[]
				 */
				amsdu_agg_enable[i] = ami->txpolicy.amsdu_agg_enable[i];
				/* Nothing else to be done. */
				continue;
			}
#ifdef BCMDBG
			if (fragthresh > MIN(max_agg, fifo_size)) {
				WL_AMSDU(("wl%d:%s: MIN(max_agg=%d, fifo_sz=%d)=>amsdu_max_agg\n",
					ami->wlc->pub->unit, __FUNCTION__, max_agg, fifo_size));
			}
#endif /* BCMDBG */
			ami->txpolicy.amsdu_max_agg_bytes[i] = MIN(fifo_size, max_agg);
			/* if frag not disabled, then take into account the fragthresh */
			if (!frag_disabled) {
				ami->txpolicy.amsdu_max_agg_bytes[i] =
					MIN(ami->txpolicy.amsdu_max_agg_bytes[i], fragthresh);
			}
		}
		amsdu_agg_enable[i] = ami->txpolicy.amsdu_agg_enable[i] &&
			(ami->txpolicy.amsdu_max_agg_bytes[i] > AMSDU_AGGBYTES_MIN);

		if (!amsdu_agg_enable[i]) {
			WL_AMSDU(("wlc_amsdu_agg_frag_upd: fragthresh is too small for AMSDU %d\n",
				i));
		}
	}

	/* toss A-MSDU since bust it up is very expensive, can't push through */
	if (flush) {
		wlc_amsdu_agg_flush(ami);
	}

	/* update all scb limit */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_amsdu_tx_scb_agglimit_upd_all(ami, scb, amsdu_agg_enable);
	}
} /* wlc_amsdu_agglimit_frag_upd */

/** deal with WME txop dynamically shrink.  WLAMSDU_TX specific function. */
void
wlc_amsdu_txop_upd(amsdu_info_t *ami)
{
	/* XXX this is tricky.
	 * this can happen dynamically when rate is changed,
	 * how to take remaining txop into account
	 * refer to wme_txop[ac]
	 */
	BCM_REFERENCE(ami);
}

/**
 * WLAMSDU_TX specific function.
 * called when ratelink memory -> ratesel.
 * driver need to update amsdu_agg_bytes_max value.
 */
void BCMFASTPATH
wlc_amsdu_scb_agglimit_calc(amsdu_info_t *ami, struct scb *scb)
{
	scb_amsdu_t *scb_ami;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	scb_ami->agglimit_upd = TRUE;
}

static void
wlc_amsdu_tx_scb_agglimit_upd(amsdu_info_t *ami, struct scb *scb)
{
	uint i;
	scb_amsdu_t *scb_ami;
	uint16 mtu_pref = 0;
	uint16 max_agg_len = 0;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	if (!SCB_AMSDU(scb) || SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		/* no action needed */
		return;
	}

	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	mtu_pref = scb_ami->mtu_pref;
	max_agg_len = wlc_amsdu_tx_scb_max_agg_len_upd(ami, scb);
	mtu_pref = MIN(mtu_pref, max_agg_len);
#ifdef BCMDBG
	WL_AMSDU(("wl%d: %s: scb=%s scb->mtu_pref %d\n",
		ami->wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&(scb->ea), eabuf), mtu_pref));
#endif // endif

	for (i = 0; i < NUMPRIO; i++) {
		WL_AMSDU(("scb_txpolicy[%d].agg_bytes_max: old = %d", i,
			scb_ami->scb_txpolicy[i].amsdu_agg_bytes_max));
#ifndef BCMSIM
		scb_ami->scb_txpolicy[i].amsdu_agg_bytes_max =
			MIN(mtu_pref, ami->txpolicy.amsdu_max_agg_bytes[i]);
#else
		/* BCMSIM has limited 2K buffer size */
		scb_ami->scb_txpolicy[i].amsdu_agg_bytes_max = AMSDU_MAX_MSDU_PKTLEN;
#endif // endif
		WL_AMSDU((" new = %d\n", scb_ami->scb_txpolicy[i].amsdu_agg_bytes_max));
	}
}

void
wlc_amsdu_tx_scb_set_max_agg_size(amsdu_info_t *ami, struct scb *scb, uint16 n_bytes)
{
	scb_amsdu_t *scb_ami;

	if (!SCB_AMSDU(scb) || SCB_MARKED_DEL(scb) || SCB_DEL_IN_PROGRESS(scb)) {
		/* no action needed */
		return;
	}

	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	if (scb_ami->mtu_pref == n_bytes) {
		return;
	}

	scb_ami->mtu_pref = n_bytes;
	wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
}

/**
 * A-MSDU admission control, per-scb-tid. WLAMSDU_TX specific function.
 * called from tx completion, to decrement agg_txpend, compare with LOWM/HIWM
 * - this is called regardless the tx frame is AMSDU or not. the amsdu_agg_txpending
 *   increment/decrement for any traffic for scb-tid.
 * - work on best-effort traffic only for now, can be expanded to other in the future
 * - amsdu_agg_txpending never go below 0
 * - amsdu_agg_txpending may not be accurate before/after A-MSDU agg is added to txmodule
 *   config/unconfig dynamically
 */
static void
wlc_amsdu_tx_dotxstatus(amsdu_info_t *ami, struct scb *scb, void* p)
{

	uint tid;
	scb_amsdu_t *scb_ami;

	WL_AMSDU(("wlc_amsdu_tx_dotxstatus\n"));

	ASSERT(scb && SCB_AMSDU(scb));

	tid = (uint8)PKTPRIO(p);
	if (PRIO_8021D_BE != tid) {
		WL_AMSDU(("wlc_amsdu_tx_dotxstatus, tid %d\n", tid));
		return;
	}
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	ASSERT(scb_ami);
	if (scb_ami->aggstate[tid].amsdu_agg_txpending > 0)
		scb_ami->aggstate[tid].amsdu_agg_txpending--;

	WL_AMSDU(("wlc_amsdu_tx_dotxstatus: scb txpending reduce to %d\n",
		scb_ami->aggstate[tid].amsdu_agg_txpending));

	/* close all aggregation if changed to disable
	 * XXX to optimize and reduce per-pkt callback
	 */
	if ((scb_ami->aggstate[tid].amsdu_agg_txpending < ami->txpolicy.fifo_lowm)) {
		if (scb_ami->aggstate[tid].amsdu_agg_p &&
				!(scb->mark & (SCB_DEL_IN_PROG | SCB_MARK_TO_DEL))) {
			WL_AMSDU(("wlc_amsdu_tx_dotxstatus: release amsdu "
				  "due to low watermark!!\n"));
			wlc_amsdu_agg_close(ami, scb, tid); /* close and send AMSDU */
			WLCNTINCR(ami->cnt->agg_stop_lowwm);
		}
	}
} /* wlc_amsdu_tx_dotxstatus */

/* Check if AMSDU is enabled for given tid */
bool
wlc_amsdu_chk_priority_enable(amsdu_info_t *ami, uint8 tid)
{
	return ami->txpolicy.amsdu_agg_enable[tid];
}

/** centralize A-MSDU tx policy */
void
wlc_amsdu_txpolicy_upd(amsdu_info_t *ami)
{
	WL_AMSDU(("wlc_amsdu_txpolicy_upd\n"));

	if (PIO_ENAB(ami->pub))
		ami->amsdu_agg_block = TRUE;
	else {
		int idx;
		wlc_bsscfg_t *cfg;
		FOREACH_BSS(ami->wlc, idx, cfg) {
			if (!cfg->BSS)
				ami->amsdu_agg_block = TRUE;
		}
	}

	ami->amsdu_agg_block = FALSE;
}

/**
 * Return vht max if to send at vht rates
 * Return ht max if to send at ht rates
 * Return 0 otherwise
 * WLAMSDU_TX specific function.
 */
static uint
wlc_amsdu_get_max_agg_bytes(struct scb *scb, scb_amsdu_t *scb_ami,
	uint tid, void *p, ratespec_t rspec)
{
	uint byte_limit = scb_ami->scb_txpolicy[tid].amsdu_agg_bytes_max;

	WL_AMSDU(("max agg used = %d\n", byte_limit));
	return byte_limit;

}

uint8 BCMFASTPATH
wlc_amsdu_scb_max_sframes(amsdu_info_t *ami, struct scb *scb)
{
	scb_amsdu_t *scb_ami;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	return scb_ami->amsdu_max_sframes;
}

uint32 BCMFASTPATH
wlc_amsdu_scb_max_agg_len(amsdu_info_t *ami, struct scb *scb, uint8 tid)
{
	scb_amsdu_t *scb_ami;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	/* If rate changed, driver need to recalculate
	 * the max aggregation length for vht and he rate.
	 */
	if (scb_ami->agglimit_upd) {
		wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
		scb_ami->agglimit_upd = FALSE;
	}

	return scb_ami->scb_txpolicy[tid].amsdu_agg_bytes_max;
}

void
wlc_amsdu_max_agg_len_upd(amsdu_info_t *ami)
{
	struct scb *scb;
	struct scb_iter scbiter;

	/* update amsdu agg bytes for ALL scbs */
	FOREACHSCB(ami->wlc->scbstate, &scbiter, scb) {
		wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
	}
}

static uint32 BCMFASTPATH
wlc_amsdu_tx_scb_max_agg_len_upd(amsdu_info_t *ami, struct scb *scb)
{
	wlc_info_t *wlc;
	uint8 nss, mcs, i;
	ratespec_t rspec;
	uint32 max_agg;
	ratesel_txparams_t ratesel_rates;
	const wlc_rlm_rate_store_t *rstore = NULL;
	bool updated = TRUE;

	wlc = ami->wlc;

	if (WLCISACPHY(wlc->band)) {
#if AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB
		max_agg = HT_MAX_AMSDU;
#else
		max_agg = VHT_MAX_AMSDU;
#endif /* AMSDU_VHT_USE_HT_AGG_LIMITS_ENAB */
	} else {
		max_agg = HT_MIN_AMSDU;
		goto done;
	}

	if (RSPEC_ACTIVE(wlc->band->rspec_override)) {
		ratesel_rates.num = 1;
		ratesel_rates.rspec[0] = wlc->band->rspec_override;
	} else {
		uint16 flag0, flag1; /* as passed in para but not use it at caller */

		/* Force trigger ratemem update */
		ratesel_rates.num = 4; /* enable multi fallback rate */
		ratesel_rates.ac = 0; /* Use priority BE for reference */
		updated = wlc_scb_ratesel_gettxrate(wlc->wrsi, scb, &flag0, &ratesel_rates, &flag1);
		if (RATELINKMEM_ENAB(wlc->pub) && !updated) {
			uint16 rate_idx;

			rate_idx = wlc_ratelinkmem_get_scb_rate_index(wlc, scb);
			rstore = wlc_ratelinkmem_retrieve_cur_rate_store(wlc, rate_idx, TRUE);
			if (rstore == NULL) {
				WL_ERROR(("wl%d: %s, no rstore for scb: %p ("MACF", idx: %d)\n",
					wlc->pub->unit, __FUNCTION__, scb,
					ETHER_TO_MACF(scb->ea), rate_idx));
				wlc_scb_ratesel_init(wlc, scb);
				updated = wlc_scb_ratesel_gettxrate(wlc->wrsi, scb, &flag0,
					&ratesel_rates, &flag1);
				ASSERT(updated);
			}
		}
	}

	/* XXX
	 * Without the below for loop, the driver would hit "suppressed by continuous AGG0" if
	 * amsdu_aggsf >= 3 when using rate mcs0. To resolve the issue, the driver was modified to
	 * change max amsdu aggregation length dynamically, on a tx rateset change. RB:157173.
	 */
	for (i = 0; i < ratesel_rates.num; i++) {
		if (RATELINKMEM_ENAB(wlc->pub) && !updated) {
			rspec = WLC_RATELINKMEM_GET_RSPEC(rstore, i);
		} else {
			rspec = ratesel_rates.rspec[i];
		}

		if (RSPEC_ISHE(rspec)) {
			mcs = RSPEC_HE_MCS(rspec);
			nss = RSPEC_HE_NSS(rspec);
		} else if (RSPEC_ISVHT(rspec)) {
			mcs = RSPEC_VHT_MCS(rspec);
			nss = RSPEC_VHT_NSS(rspec);
		} else {
			max_agg = HT_MIN_AMSDU;
			WL_AMSDU(("%s: ht or legacy rate\n", __FUNCTION__));
			break;
		}

		if (RSPEC_IS20MHZ(rspec) && (mcs == 0) && (nss == 1)) {
			WL_AMSDU(("%s: Rate:mcs=0 nss=1 bw=20Mhz\n",
				__FUNCTION__));
			max_agg = MIN(max_agg, HT_MIN_AMSDU);
			break;
		}
	}

done:
	WL_AMSDU(("%s: Update max_agg_len=%d for "MACF"\n",
		__FUNCTION__, max_agg, ETHER_TO_MACF(scb->ea)));

	return max_agg;
} /* wlc_amsdu_tx_scb_max_agg_len_upd */

#if defined(HWA_PKTPGR_BUILD)

/* Fixup AMSDU in single TxLfrag */
void
wlc_amsdu_single_txlfrag_fixup(wlc_info_t *wlc, void *head)
{
	osl_t *osh;
	void *p1, *next, *pn, *p, *txlfrag;
	uint8 pkt_count, free_pkt_count, fi;
	uint32 len;

	HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(head, NULL,
		"++single_txlfrag_fixup++", 0, FALSE));

	// Setup local
	osh = wlc->osh;
	fi = 1;
	pkt_count = 0;
	free_pkt_count = 0;
	p1 = PKTNEXT(osh, head);
	PKTSETNEXT(osh, head, NULL);
	txlfrag = head;

	// Should not have link
	ASSERT(PKTLINK(txlfrag) == NULL);

	p = p1;
	while (p) {
		pn = p;
		next = PKTNEXT(osh, p);
		len = PKTFRAGLEN(osh, p, LB_FRAG1);
		PKTSETHOSTPKTID(txlfrag, fi, PKTFRAGPKTID(osh, p));
		PKTSETFRAGLEN(osh, txlfrag, fi, len);

		// Sum of all host length in host_datalen.
		PKTSETFRAGTOTLEN(osh, txlfrag, (PKTFRAGTOTLEN(osh, txlfrag) + len));

		PKTSETFRAGDATA_HI(osh, txlfrag, fi, PKTFRAGDATA_HI(osh, p, LB_FRAG1));
		PKTSETFRAGDATA_LO(osh, txlfrag, fi, PKTFRAGDATA_LO(osh, p, LB_FRAG1));
		PKTSETRDIDX(txlfrag, (fi-1), PKTFRAGRINGINDEX(osh, p));

		// Link free list
		PKTSETLINK(p, next);
		PKTSETNEXT(osh, p, NULL);
		HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(p, NULL,
			"single_txlfrag_fixup", fi, TRUE));

		pkt_count++;
		free_pkt_count++;
		fi++;

		p = next;

		// Use new txlfrag
		if (p && fi == HWA_PP_PKT_FRAGINFO_MAX) {
			// Set current txlfrag Frag Num
			PKTSETFRAGTOTNUM(osh, txlfrag, (PKTFRAGTOTNUM(osh, txlfrag) + pkt_count));
			pkt_count = 0;
			fi = 1;
			// Link txlfrags by next
			PKTSETNEXT(osh, txlfrag, p);
			// New txlfrag
			txlfrag = p;

			// Should not have link
			ASSERT(PKTLINK(txlfrag) == NULL);

			p = PKTNEXT(osh, txlfrag);
			PKTSETNEXT(osh, txlfrag, NULL);

			// Add to free list
			PKTSETLINK(pn, p);
		}
	}

	if (pkt_count) {
		PKTSETFRAGTOTNUM(osh, txlfrag, (PKTFRAGTOTNUM(osh, txlfrag) + pkt_count));
	}
	WL_TRACE(("%s: txlfrag[seq<0x%x>, rindex<%d,%d,%d,%d>]\n",
		__FUNCTION__, (WLPKTTAG(txlfrag)->seq & WL_SEQ_AMSDU_SUPPR_MASK),
		PKTFRAGRINGINDEX(wlc->osh, txlfrag), PKTRDIDX(txlfrag, 0),
		PKTRDIDX(txlfrag, 1), PKTRDIDX(txlfrag, 2)));

	if (free_pkt_count) {
		HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(p1, NULL,
			"free:single_txlfrag_fixup", 0, FALSE));
		hwa_pktpgr_free_tx(hwa_dev, PPLBUF(p1), PPLBUF(pn), free_pkt_count);
	}

	// Mark stop index
	if (fi < HWA_PP_PKT_FRAGINFO_MAX) {
		PKTSETHOSTPKTID(txlfrag, fi, 0);
	}

	HWA_PKT_DUMP_EXPR(hwa_txpost_dump_pkt(head, NULL,
		"--single_txlfrag_fixup--", 0, FALSE));
}

#endif /* HWA_PKTPGR_BUILD */

/**
 * Dynamic adjust amsdu_aggsf based on STA's bandwidth
 */
void
wlc_amsdu_tx_scb_aggsf_upd(amsdu_info_t *ami, struct scb *scb)
{
	wlc_info_t *wlc;
	scb_amsdu_t *scb_ami;
	uint8 bw, aggsf_cfp, aggsf;

	wlc = ami->wlc;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);

	/* For CFP path, limit the max amsdu_aggsf based on STA's bandwidth.
	 * 80Mhz: 4, 40Mhz: 2, 20Mhz: 2
	 */
	if (bw == BW_160MHZ) {
		aggsf_cfp = ami->txpolicy.amsdu_max_sframes;
	} else if (bw == BW_80MHZ) {
		aggsf_cfp = MIN(ami->txpolicy.amsdu_max_sframes, 4);
	} else {
		aggsf_cfp = MIN(ami->txpolicy.amsdu_max_sframes, 2);
	}

#if defined(BCMHWA)
	HWA_PKTPGR_EXPR({
		if ((hwa_pktpgr_multi_txlfrag(hwa_dev) == 0) && (aggsf_cfp > 4)) {
			WL_ERROR(("%s: Cannot support more then %d subframes in one A-MSDU\n",
				__FUNCTION__, aggsf_cfp));
			aggsf_cfp = 4;
		}
	});
#endif // endif

	/* XXX: For legacy path, there is throughput regression for mfgtest driver
	 * when amsdu_aggsf > 4
	 */
	aggsf = MIN(aggsf_cfp, 4);

	scb_ami->amsdu_aggsf = aggsf_cfp;
	scb_ami->amsdu_max_sframes = aggsf;
}

uint32 BCMFASTPATH
wlc_amsdu_scb_aggsf(amsdu_info_t *ami, struct scb *scb, uint8 tid)
{
	scb_amsdu_t *scb_ami;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	return scb_ami->amsdu_aggsf;
}
#endif /* WLAMSDU_TX */

#if defined(PKTC) || defined(PKTC_TX_DONGLE)

#ifdef DUALPKTAMSDU
/**
 * Do AMSDU aggregation for chained packets. Called by AMPDU module (strange?) as in:
 *     txmod -> wlc_ampdu_agg_pktc() -> wlc_amsdu_pktc_agg()
 * Consumes MSDU from head of chain 'n' and append this MSDU to chain 'p'. Does not store MSDUs
 * in-between function calls.
 *
 *     @param[in/out] *ami
 *     @param[in/out] *p    packet chain that is extended with subframes in parameter 'n'
 *     @param[in/out] *n    packet chain containing subframes to aggregate
 *     @param[in]     tid   QoS class
 *     @param[in]     lifetime
 *     @return        new head of chain for 'n' (so points at first msdu that was not yet processed)
 */
void * BCMFASTPATH
wlc_amsdu_pktc_agg(amsdu_info_t *ami, struct scb *scb, void *p, void *n,
	uint8 tid, uint32 lifetime)
{
	wlc_info_t *wlc;
	int32 pad;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	int32 totlen = 0;
#endif /* BCMDBG || BCMDBG_AMSDU */
	scb_amsdu_t *scb_ami;
	void *p1 = NULL;
	uint32 p_len, n_len = 0;

	wlc = ami->wlc;

#if defined(BCMHWA)
	/* Not implement */
	HWA_PKTPGR_EXPR(ASSERT(0));
#endif // endif

#ifdef	WLCFP
	/* AMSDU agg done at AMPDU release.
	 * equivalent fn is wlc_amsdu_agg_attempt()
	 */
	if (CFP_ENAB(wlc->pub) == TRUE) {
		ASSERT(0);
	}
#endif // endif
	pad = 0;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	p_len = pkttotlen(wlc->osh, p);
	n_len = pkttotlen(wlc->osh, n);

	if (scb_ami->amsdu_max_sframes <= 1) {
		WLCNTINCR(ami->cnt->agg_stop_sf);
		return n;
	} else if ((n_len + p_len)
		>= scb_ami->scb_txpolicy[tid].amsdu_agg_bytes_max) {
		WLCNTINCR(ami->cnt->agg_stop_len);
		return n;
	}
#if defined(PROP_TXSTATUS) && defined(BCMPCIEDEV)
	else if (BCMPCIEDEV_ENAB() && PROP_TXSTATUS_ENAB(wlc->pub) &&
		WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc))) {
		/*
		 * This comparison does the following:
		 *  - For suppressed pkts in AMSDU, the mask should be same
		 *      so re-aggregate them
		 *  - For new pkts, pkttag->seq is zero so same
		 *  - Prevents suppressed pkt from being aggregated with non-suppressed pkt
		 *  - Prevents suppressed MPDU from being aggregated with suppressed MSDU
		 */
		if ((WLPKTTAG(p)->seq & WL_SEQ_AMSDU_SUPPR_MASK) !=
			(WLPKTTAG(n)->seq & WL_SEQ_AMSDU_SUPPR_MASK)) {
			WLCNTINCR(ami->cnt->agg_stop_suppr);
			return n;
		}
	}
#endif /* PROP_TXSTATUS && BCMPCIEDEV */

	/* Padding of A-MSDU sub-frame to 4 bytes */
	pad = (uint)((-(int)(p_len)) & 3);

	/* For first msdu init ether header (amsdu header) */
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (PKTISTXFRAG(wlc->osh, p)) {
		/* When BCM_DHDHDR enabled the DHD host driver will prepare the
		 * dot3_mac_llc_snap_header, now we adjust ETHER_HDR_LEN bytes
		 * in host data addr to include the ether header 14B part.
		 * The ether header 14B in dongle D3_BUFFER is going to be used as
		 * amsdu header.
		 * Now, first msdu has all DHDHDR 22B in host.
		 */
		PKTSETFRAGDATA_LO(wlc->osh, p, LB_FRAG1,
			PKTFRAGDATA_LO(wlc->osh, p, LB_FRAG1) - ETHER_HDR_LEN);
		PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
			PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + ETHER_HDR_LEN);
		PKTSETFRAGTOTLEN(wlc->osh, p,
			PKTFRAGTOTLEN(wlc->osh, p) + ETHER_HDR_LEN);
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	WLPKTTAG(p)->flags |= WLF_AMSDU;

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* For second msdu */
	if (PKTISTXFRAG(wlc->osh, n)) {
		/* Before we free the second msdu D3_BUFFER we have to include the
		 * ether header 14B part in host.
		 * Now, second msdu has all DHDHDR 22B in host.
		 */
		PKTSETFRAGDATA_LO(wlc->osh, n, LB_FRAG1,
			PKTFRAGDATA_LO(wlc->osh, n, LB_FRAG1) - ETHER_HDR_LEN);
		PKTSETFRAGLEN(wlc->osh, n, LB_FRAG1,
			PKTFRAGLEN(wlc->osh, n, LB_FRAG1) + ETHER_HDR_LEN);
		PKTSETFRAGTOTLEN(wlc->osh, n,
			PKTFRAGTOTLEN(wlc->osh, n) + ETHER_HDR_LEN);

		/* Free the second msdu D3_BUFFER, we don't need it */
		PKTBUFEARLYFREE(wlc->osh, n);
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	/* Append msdu p1 to p */
	p1 = n;
	n = PKTCLINK(p1);
	PKTSETCLINK(p1, NULL);
	PKTCLRCHAINED(wlc->osh, p1);
	PKTSETNEXT(wlc->osh, pktlast(wlc->osh, p), p1);
	ASSERT(!PKTISCFP(p1));

	if (n)
		wlc_pktc_sdu_prep(wlc, scb, p1, n, lifetime);

	if (pad) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		if (PKTISTXFRAG(wlc->osh, p)) {
			/* pad data will be the garbage at the end of host data */
			PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
				PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + pad);
			PKTSETFRAGTOTLEN(wlc->osh, p, PKTFRAGTOTLEN(wlc->osh, p) + pad);
		}
		else
#endif // endif
		/* If a padding was required for the previous packet, apply it here */
		PKTPUSH(wlc->osh, p1, pad);
	}

	WLCNTINCR(ami->cnt->agg_amsdu);
	WLCNTADD(ami->cnt->agg_msdu, 2);
	WLCNTINCR(ami->cnt->agg_stop_sf);

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	totlen = n_len + p_len + pad;
	/* update statistics histograms */
	ami->amdbg->tx_msdu_histogram[1]++;
	ami->amdbg->tx_length_histogram[MIN(totlen / AMSDU_LENGTH_BIN_BYTES,
		AMSDU_LENGTH_BINS-1)]++;
#endif /* BCMDBG || BCMDBG_AMSDU */

	return n;
} /* wlc_amsdu_pktc_agg */

#else /* DUALPKTAMSDU */

/**
 * Do AMSDU aggregation for chained packets. Called by AMPDU module (strange?) as in:
 *     txmod -> wlc_ampdu_agg_pktc() -> wlc_amsdu_pktc_agg()
 * Consumes MSDU from head of chain 'n' and append this MSDU to chain 'p'. Does not store MSDUs
 * in-between function calls.
 *
 *     @param[in/out] *ami
 *     @param[in/out] *p    packet chain that is extended with subframes in parameter 'n'
 *     @param[in/out] *n    packet chain containing subframes to aggregate
 *     @param[in]     tid   QoS class
 *     @param[in]     lifetime
 *     @return        new head of chain for 'n' (so points at first msdu that was not yet processed)
 */
void * BCMFASTPATH
wlc_amsdu_pktc_agg(amsdu_info_t *ami, struct scb *scb, void *p, void *n,
	uint8 tid, uint32 lifetime)
{
	wlc_info_t *wlc;
	int32 pad, lastpad = 0;
	uint8 i;
	uint32 totlen = 0;
	scb_amsdu_t *scb_ami;
	void *p1 = NULL;
	bool pad_at_head = FALSE;
#if defined(BCMHWA)
	HWA_PKTPGR_EXPR(void *head = p);
#endif // endif

	wlc = ami->wlc;

#ifdef	WLCFP
	/* AMSDU agg done at AMPDU release.
	 * equivalent fn is wlc_amsdu_agg_attempt()
	 */
	if (CFP_ENAB(wlc->pub) == TRUE) {
		ASSERT(0);
	}
#endif // endif

#ifdef BCMDBG
	if (ami->amsdu_agg_block) {
		WLCNTINCR(ami->cnt->agg_passthrough);
		return (n);
	}
#endif /* BCMDBG */

#if defined(BCMHWA)
	/* PKTPGR cannot do AMSDU in fragment packet.
	 * SW WAR: In PKTPGR no AMSDU for fragments.
	 */
	HWA_PKTPGR_EXPR({
		if (PKTNEXT(wlc->osh, p)) {
			WLCNTINCR(ami->cnt->agg_passthrough);
			return (n);
		}
	});
#endif // endif

	pad = 0;
	i = 1;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	totlen = pkttotlen(wlc->osh, p);

#ifdef WLAMSDU_TX
	/* If rate changed, driver need to recalculate
	 * the max aggregation length for vht and he rate.
	 */
	if (scb_ami->agglimit_upd) {
		wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
		scb_ami->agglimit_upd = FALSE;
	}
#endif /* WLAMSDU_TX */

	/* msdu's in packet chain 'n' are aggregated while in this loop */
	while (1) {
#if defined(BCMHWA)
		/* PKTPGR cannot do AMSDU in fragment packet.
		 * SW WAR: In PKTPGR no AMSDU for fragments.
		 */
		HWA_PKTPGR_EXPR({
			if (PKTNEXT(wlc->osh, n)) {
				WLCNTINCR(ami->cnt->agg_passthrough);
				break;
			}
		});
#endif // endif

#ifdef WLOVERTHRUSTER
		if (OVERTHRUST_ENAB(wlc->pub)) {
			/* Check if the next subframe is possibly TCP ACK */
			if (pkttotlen(wlc->osh, n) <= TCPACKSZSDU) {
				WLCNTINCR(ami->cnt->agg_stop_tcpack);
				break;
			}
		}
#endif /* WLOVERTHRUSTER */
		if (i >= scb_ami->amsdu_max_sframes) {
			WLCNTINCR(ami->cnt->agg_stop_sf);
			break;
		} else if ((totlen + pkttotlen(wlc->osh, n))
			>= scb_ami->scb_txpolicy[tid].amsdu_agg_bytes_max) {
			WLCNTINCR(ami->cnt->agg_stop_len);
			break;
		}
#if defined(PROP_TXSTATUS) && defined(BCMPCIEDEV)
		else if (BCMPCIEDEV_ENAB() && PROP_TXSTATUS_ENAB(wlc->pub) &&
			WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc))) {
			/*
			 * This comparison does the following:
			 *  - For suppressed pkts in AMSDU, the mask should be same
			 *      so re-aggregate them
			 *  - For new pkts, pkttag->seq is zero so same
			 *  - Prevents suppressed pkt from being aggregated with non-suppressed pkt
			 *  - Prevents suppressed MPDU from being aggregated with suppressed MSDU
			 */
			if ((WLPKTTAG(p)->seq & WL_SEQ_AMSDU_SUPPR_MASK) !=
				(WLPKTTAG(n)->seq & WL_SEQ_AMSDU_SUPPR_MASK)) {
				WLCNTINCR(ami->cnt->agg_stop_suppr);
				break;
			}
		}
#endif /* PROP_TXSTATUS && BCMPCIEDEV */

		/* Padding of A-MSDU sub-frame to 4 bytes */
		pad = (uint)((-(int)(pkttotlen(wlc->osh, p) - lastpad)) & 3);

		if (i == 1) {
			/* For first msdu init ether header (amsdu header) */
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
			if (PKTISTXFRAG(wlc->osh, p)) {
				/* When BCM_DHDHDR enabled the DHD host driver will prepare the
				 * dot3_mac_llc_snap_header, now we adjust ETHER_HDR_LEN bytes
				 * in host data addr to include the ether header 14B part.
				 * The ether header 14B in dongle D3_BUFFER is going to be used as
				 * amsdu header.
				 * Now, first msdu has all DHDHDR 22B in host.
				 */
				PKTSETFRAGDATA_LO(wlc->osh, p, LB_FRAG1,
					PKTFRAGDATA_LO(wlc->osh, p, LB_FRAG1) - ETHER_HDR_LEN);
				PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
					PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + ETHER_HDR_LEN);
				PKTSETFRAGTOTLEN(wlc->osh, p,
					PKTFRAGTOTLEN(wlc->osh, p) + ETHER_HDR_LEN);
			}
#endif /* BCM_DHDHDR && DONGLEBUILD */
			WLPKTTAG(p)->flags |= WLF_AMSDU;
		}

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		/* For second msdu and later */
		if (PKTISTXFRAG(wlc->osh, n)) {
			/* Before we free the second msdu D3_BUFFER we have to include the
			 * ether header 14B part in host.
			 * Now, second msdu has all DHDHDR 22B in host.
			 */
			PKTSETFRAGDATA_LO(wlc->osh, n, LB_FRAG1,
				PKTFRAGDATA_LO(wlc->osh, n, LB_FRAG1) - ETHER_HDR_LEN);
			PKTSETFRAGLEN(wlc->osh, n, LB_FRAG1,
				PKTFRAGLEN(wlc->osh, n, LB_FRAG1) + ETHER_HDR_LEN);
			PKTSETFRAGTOTLEN(wlc->osh, n,
				PKTFRAGTOTLEN(wlc->osh, n) + ETHER_HDR_LEN);

			/* Free the second msdu D3_BUFFER, we don't need it */
			PKTBUFEARLYFREE(wlc->osh, n);
		}
#endif /* BCM_DHDHDR && DONGLEBUILD */

		/* Add padding to next pkt (at headroom) if current is a lfrag */
		if (BCMLFRAG_ENAB() && PKTISTXFRAG(wlc->osh, p)) {
			/*
			 * Store the padding value. We need this to be accounted for, while
			 * calculating the padding for the subsequent packet.
			 * For example, if we need padding of 3 bytes for the first packet,
			 * we add those 3 bytes padding in the head of second packet. Now,
			 * to check if padding is required for the second packet, we should
			 * calculate the padding without considering these 3 bytes that
			 * we have already put in.
			 */
			lastpad = pad;

			if (pad) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
				/* pad data will be the garbage at the end of host data */
				PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
					PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + pad);
				PKTSETFRAGTOTLEN(wlc->osh, p, PKTFRAGTOTLEN(wlc->osh, p) + pad);
				totlen += pad;
				lastpad = 0;
#else
				/*
				 * Let's just mark that padding is required at the head of next
				 * packet. Actual padding needs to be done after the next sdu
				 * header has been copied from the current sdu.
				 */
				pad_at_head = TRUE;

#ifdef WLCNT
				WLCNTINCR(ami->cnt->tx_padding_in_head);
#endif /* WLCNT */
#endif /* BCM_DHDHDR && DONGLEBUILD */
			}
		} else {
			PKTSETLEN(wlc->osh, p, pkttotlen(wlc->osh, p) + pad);
			totlen += pad;
#ifdef WLCNT
			WLCNTINCR(ami->cnt->tx_padding_in_tail);
#endif /* WLCNT */
		}

		/* Consumes MSDU from head of chain 'n' and append this MSDU to chain 'p' */
		p1 = n;
		n = PKTCLINK(p1); /* advance 'n' by one MSDU */
		PKTSETCLINK(p1, NULL);
		PKTCLRCHAINED(wlc->osh, p1);
		if (BCMLFRAG_ENAB()) {
			PKTSETNEXT(wlc->osh, pktlast(wlc->osh, p), p1);
			PKTSETNEXT(wlc->osh, pktlast(wlc->osh, p1), NULL);
		} else {
			PKTSETNEXT(wlc->osh, p, p1); /* extend 'p' with one MSDU */
			PKTSETNEXT(wlc->osh, p1, NULL);
		}
		totlen += pkttotlen(wlc->osh, p1);
		i++;
		ASSERT(!PKTISCFP(p1));

		/* End of pkt chain */
		if (n == NULL)
			break;

		wlc_pktc_sdu_prep(wlc, scb, p1, n, lifetime);

		if (pad_at_head == TRUE) {
			/* If a padding was required for the previous packet, apply it here */
			PKTPUSH(wlc->osh, p1, pad);
			totlen += pad;
			pad_at_head = FALSE;
		}
		p = p1;
	} /* while */

	WLCNTINCR(ami->cnt->agg_amsdu);
	WLCNTADD(ami->cnt->agg_msdu, i);

	if (pad_at_head == TRUE) {
		/* If a padding was required for the previous/last packet, apply it here */
		PKTPUSH(wlc->osh, p1, pad);
		totlen += pad;
		pad_at_head = FALSE;
	}

#ifdef WL11K
	WLCNTADD(ami->cnt->agg_amsdu_bytes_l, totlen);
	if (ami->cnt->agg_amsdu_bytes_l < totlen)
		WLCNTINCR(ami->cnt->agg_amsdu_bytes_h);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	/* update statistics histograms */
	ami->amdbg->tx_msdu_histogram[MIN(i-1,
		MAX_TX_SUBFRAMES_LIMIT-1)]++;
	ami->amdbg->tx_length_histogram[MIN(totlen / AMSDU_LENGTH_BIN_BYTES,
		AMSDU_LENGTH_BINS-1)]++;
#endif /* BCMDBG || BCMDBG_AMSDU */

#if defined(BCMHWA)
	/* Fixup AMSDU in single TxLfrag */
	HWA_PKTPGR_EXPR(wlc_amsdu_single_txlfrag_fixup(wlc, head));
#endif /* BCMHWA */

	return n;
} /* wlc_amsdu_pktc_agg */
#endif /* DUALPKTAMSDU */
#endif /* PKTC */

#ifdef WLAMSDU_TX
static void
wlc_amsdu_agg(void *ctx, struct scb *scb, void *p, uint prec)
{
	amsdu_info_t *ami;
	osl_t *osh;
	uint tid = 0;
	scb_amsdu_t *scb_ami;
	wlc_bsscfg_t *bsscfg;
	uint totlen;
	uint maxagglen;
	ratespec_t rspec;

	ami = (amsdu_info_t *)ctx;
	osh = ami->pub->osh;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	tid = PKTPRIO(p);
	ASSERT(tid < NUMPRIO);

#if defined(PKTC) || defined(PKTC_TX_DONGLE)
	/* wlc_amsdu_agg can not work with PKTC enabled.
	 * AMSDU agg handled by AMPDU PKTC txmod function in that case.
	 * So passthrough to AMPDU MOD function quickly.
	 */
	if ((PKTC_ENAB(ami->wlc->pub)) && (PKTC_TX_ENAB(ami->wlc->pub)) &&
		(SCB_TXMOD_NEXT_FID(scb, TXMOD_AMSDU) == TXMOD_AMPDU)) {
		WL_NONE(("%s:PASSTHROUGH PKTC Enabled\n", __FUNCTION__));
		goto passthrough;
	}

	/* Caller of AMSDU TXMOD fn should make sure its unchained.
	 * wlc_amsdu_agg can not handle AMSDU agg of chained pkts
	 * Current known callers
	 * 1. wlc_sendpkt
	 * 2. wlc_nar_pass_packet_on
	 */
	ASSERT(PKTCLINK(p) == NULL);
#endif /* PKTC || PKTC_TX_DONGLE */

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (!PKTISTXFRAG(osh, p)) {
		WL_NONE(("%s:PASSTHROUGH !PKTISTXFRAG\n", __FUNCTION__));
		goto passthrough;
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	if (ami->amsdu_agg_block) {
		WL_NONE(("%s:PASSTHROUGH ami->amsdu_agg_block\n", __FUNCTION__));
		goto passthrough;
	}

	/* doesn't handle ctl/mgmt frame */
	if (WLPKTTAG(p)->flags & WLF_CTLMGMT) {
		WL_NONE(("%s:PASSTHROUGH WLPKTTAG(p)->flags & WLF_CTLMGMT\n", __FUNCTION__));
		goto passthrough;
	}

	/* non best-effort, skip for now */
	if (tid != PRIO_8021D_BE) {
		WL_NONE(("%s:PASSTHROUGH tid != PRIO_8021D_BE\n", __FUNCTION__));
		goto passthrough;
	}
	/* admission control */
	if (!scb_ami->scb_txpolicy[tid].amsdu_agg_enable) {
		WL_NONE(("%s:PASSTHROUGH !ami->amsdu_agg_enable[tid]\n", __FUNCTION__));
		goto passthrough;
	}

	if (WLPKTTAG(p)->flags & WLF_8021X) {
		WL_NONE(("%s:PASSTHROUGH WLPKTTAG(p)->flags & WLF_8021X\n", __FUNCTION__));
		goto passthrough;
	}

#ifdef BCMWAPI_WAI
	if (WLPKTTAG(p)->flags & WLF_WAI) {
		WL_NONE(("%s:PASSTHROUGH WLPKTTAG(p)->flags & WLF_WAI\n", __FUNCTION__));
		goto passthrough;
	}
#endif /* BCMWAPI_WAI */

	scb = WLPKTTAGSCBGET(p);
	ASSERT(scb);

	/* the scb must be A-MSDU capable */
	ASSERT(SCB_AMSDU(scb));
	ASSERT(SCB_QOS(scb));

	rspec = wlc_scb_ratesel_get_primary(scb->bsscfg->wlc, scb, p);

#ifdef VHT_TESTBED
	if (RSPEC_ISLEGACY(rspec)) {
		WL_AMSDU(("PASSTHROUGH legacy rate\n"));
		goto passthrough;
	}
#else
	if (WLCISACPHY(scb->bsscfg->wlc->band)) {
		if (!RSPEC_ISVHT(rspec) && !RSPEC_ISHE(rspec)) {
			WL_AMSDU(("PASSTHROUGH non-VHT & non-HE on AC phy\n"));
			goto passthrough;
		}
	}
	else {
		/* add for WNM support (r365273) */
		if (!RSPEC_ISHT(rspec)) {
			WL_AMSDU(("PASSTHROUGH non-HT on HT phy\n"));
			goto passthrough;
		}
	}
#endif /* VHT_TESTBED */

	if (SCB_AMPDU(scb)) {
		/* Bypass AMSDU agg if the destination has AMPDU enabled but does not
		 * advertise AMSDU in AMPDU.
		 * This capability is advertised in the ADDBA response.
		 * This check is not taking into account whether or not the AMSDU will
		 * be encapsulated in an AMPDU, so it is possibly too conservative.
		 */
		if (!SCB_AMSDU_IN_AMPDU(scb)) {
			WL_NONE(("%s:PASSTHROUGH !SCB_AMSDU_IN_AMPDU(scb)\n", __FUNCTION__));
			goto passthrough;
		}

#ifdef WLOVERTHRUSTER
		if (OVERTHRUST_ENAB(ami->wlc->pub)) {
			/* Allow TCP ACKs to flow through to Overthruster if it is enabled */
			if (wlc_ampdu_tx_get_tcp_ack_ratio(ami->wlc->ampdu_tx) > 0 &&
				wlc_amsdu_is_tcp_ack(ami, p)) {
				WL_NONE(("%s:PASSTHROUGH TCP_ACK\n", __FUNCTION__));
				goto passthrough;
			}
		}
#endif /* WLOVERTHRUSTER */
	}

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	if (WSEC_ENABLED(bsscfg->wsec) && !WSEC_AES_ENABLED(bsscfg->wsec)) {
		WL_AMSDU(("%s: target scb %p is has wrong WSEC\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
		goto passthrough;
	}

	WL_AMSDU(("%s: txpend %d\n", __FUNCTION__, scb_ami->aggstate[tid].amsdu_agg_txpending));

	if (scb_ami->aggstate[tid].amsdu_agg_txpending < ami->txpolicy.fifo_hiwm) {
		WL_NONE((
		"%s: scb_ami->aggstate[tid].amsdu_agg_txpending < ami->txpolicy.fifo_hiwm\n",
		__FUNCTION__));
		goto passthrough;
	} else {
		WL_AMSDU(("%s: Starts aggregation due to hiwm %d reached\n",
			__FUNCTION__, ami->txpolicy.fifo_hiwm));
	}

	/* If rate changed, driver need to recalculate
	 * the max aggregation length for vht and he rate.
	 */
	if (scb_ami->agglimit_upd) {
		wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
		scb_ami->agglimit_upd = FALSE;
	}

	totlen = pkttotlen(osh, p) + scb_ami->aggstate[tid].amsdu_agg_bytes +
		scb_ami->aggstate[tid].headroom_pad_need;

	maxagglen = wlc_amsdu_get_max_agg_bytes(scb, scb_ami, tid, p, rspec);

	if ((totlen > maxagglen - ETHER_HDR_LEN) ||
		(scb_ami->aggstate[tid].amsdu_agg_sframes + 1 >
		scb_ami->amsdu_max_sframes)) {
		WL_AMSDU(("%s: terminte A-MSDU for txbyte %d or txframe %d\n",
			__FUNCTION__, maxagglen,
			scb_ami->amsdu_max_sframes));
#ifdef WLCNT
		if (totlen > maxagglen - ETHER_HDR_LEN) {
			WLCNTINCR(ami->cnt->agg_stop_len);
		} else {
			WLCNTINCR(ami->cnt->agg_stop_sf);
		}
#endif /* WLCNT */

		wlc_amsdu_agg_close(ami, scb, tid); /* close and send AMSDU */

		/* if the new pkt itself is more than aggmax, can't continue with agg_append
		 *   add here to avoid per pkt checking for this rare case
		 */
		if (pkttotlen(osh, p) > maxagglen - ETHER_HDR_LEN) {
			WL_AMSDU(("%s: A-MSDU aggmax is smaller than pkt %d, pass\n",
				__FUNCTION__, pkttotlen(osh, p)));

			goto passthrough;
		}
	}

	BCM_REFERENCE(prec);

	/* agg this one and return on success */
	if (wlc_amsdu_agg_append(ami, scb, p, tid, rspec)) {
		return;
	}

passthrough:
	/* A-MSDU agg rejected, pass through to next tx module */
	PKTCNTR_INC(ami->wlc, PKTCNTR_AMSDU_PASSTHROUGH, 1);

	/* release old first before passthrough new one to maintain sequence */
	if (scb_ami->aggstate[tid].amsdu_agg_p) {
		WL_AMSDU(("%s: release amsdu for passthough!!\n", __FUNCTION__));
		wlc_amsdu_agg_close(ami, scb, tid); /* close and send AMSDU */
		WLCNTINCR(ami->cnt->agg_stop_passthrough);
	}
	scb_ami->aggstate[tid].amsdu_agg_txpending++;
	WLCNTINCR(ami->cnt->agg_passthrough);
	WL_AMSDU(("%s: passthrough scb %p txpending %d\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(scb),
		scb_ami->aggstate[tid].amsdu_agg_txpending));
	WLF2_PCB3_REG(p, WLF2_PCB3_AMSDU);
	SCB_TX_NEXT(TXMOD_AMSDU, scb, p, WLC_PRIO_TO_PREC(tid));
} /* wlc_amsdu_agg */

/**
 * Finalizes a transmit A-MSDU and forwards the A-MSDU to the next transmit layer.
 */
static void
wlc_amsdu_agg_close(amsdu_info_t *ami, struct scb *scb, uint tid)
{
	/* ??? cck rate is not supported in hw, how to restrict rate algorithm later */
	scb_amsdu_t *scb_ami;
	void *ptid;
	osl_t *osh;
	amsdu_txaggstate_t *aggstate;
	uint16 amsdu_body_len;

	WL_AMSDU(("wlc_amsdu_agg_close\n"));
	BCM_REFERENCE(amsdu_body_len);

	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	osh = ami->pub->osh;
	aggstate = &scb_ami->aggstate[tid];
	ptid = aggstate->amsdu_agg_p;

	if (ptid == NULL)
		return;

	ASSERT(WLPKTFLAG_AMSDU(WLPKTTAG(ptid)));

	/* wlc_pcb_fn_register(wlc->pcb, wlc_amsdu_tx_complete, ami, ptid); */

	/* check */
	ASSERT(PKTLEN(osh, ptid) >= ETHER_HDR_LEN);
	ASSERT(tid == (uint)PKTPRIO(ptid));

	/* FIXUP lastframe pad --- the last subframe must not be padded,
	 * reset pktlen to the real length(strip off pad) using previous
	 * saved value.
	 * amsdupolicy->amsdu_agg_ptail points to the last buf(not last pkt)
	 */
	if (aggstate->amsdu_agg_padlast) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		BCM_REFERENCE(osh);
		/* pad data will be the garbage at the end of host data */
		PKTSETFRAGLEN(osh, aggstate->amsdu_agg_ptail, LB_FRAG1,
			PKTFRAGLEN(osh, aggstate->amsdu_agg_ptail, LB_FRAG1) -
			aggstate->amsdu_agg_padlast);
		PKTSETFRAGTOTLEN(osh, aggstate->amsdu_agg_ptail,
			PKTFRAGTOTLEN(osh, aggstate->amsdu_agg_ptail) -
			aggstate->amsdu_agg_padlast);
#else
		PKTSETLEN(osh, aggstate->amsdu_agg_ptail,
			PKTLEN(osh, aggstate->amsdu_agg_ptail) -
			aggstate->amsdu_agg_padlast);
#endif /* BCM_DHDHDR && DONGLEBUILD */
		aggstate->amsdu_agg_bytes -= aggstate->amsdu_agg_padlast;
		WL_AMSDU(("wlc_amsdu_agg_close: strip off padlast %d\n",
			aggstate->amsdu_agg_padlast));
#ifdef WLCNT
		WLCNTDECR(ami->cnt->tx_padding_in_tail);
		WLCNTINCR(ami->cnt->tx_padding_no_pad);
#endif /* WLCNT */
	}

	amsdu_body_len = (uint16) aggstate->amsdu_agg_bytes;

	WLCNTINCR(ami->cnt->agg_amsdu);
	WLCNTADD(ami->cnt->agg_msdu, aggstate->amsdu_agg_sframes);
#ifdef WL11K
	WLCNTADD(ami->cnt->agg_amsdu_bytes_l, amsdu_body_len);
	if (ami->cnt->agg_amsdu_bytes_l < amsdu_body_len)
		WLCNTINCR(ami->cnt->agg_amsdu_bytes_h);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	/* update statistics histograms */
	ami->amdbg->tx_msdu_histogram[MIN(aggstate->amsdu_agg_sframes-1,
	                                  MAX_TX_SUBFRAMES_LIMIT-1)]++;
	ami->amdbg->tx_length_histogram[MIN(amsdu_body_len / AMSDU_LENGTH_BIN_BYTES,
	                                    AMSDU_LENGTH_BINS-1)]++;
#endif /* BCMDBG */

	aggstate->amsdu_agg_txpending++;

	WL_AMSDU(("wlc_amsdu_agg_close: valid AMSDU, add to txq %d bytes, scb %p, txpending %d\n",
		aggstate->amsdu_agg_bytes, OSL_OBFUSCATE_BUF(scb),
		scb_ami->aggstate[tid].amsdu_agg_txpending));

	/* clear state prior to calling next txmod, else crash may occur if next mod frees pkt */
	/* due to amsdu use of pcb to track pkt freeing */
	aggstate->amsdu_agg_p = NULL;
	aggstate->amsdu_agg_ptail = NULL;
	aggstate->amsdu_agg_sframes = 0;
	aggstate->amsdu_agg_bytes = 0;
	aggstate->headroom_pad_need = 0;
	WLF2_PCB3_REG(ptid, WLF2_PCB3_AMSDU);
	SCB_TX_NEXT(TXMOD_AMSDU, scb, ptid, WLC_PRIO_TO_PREC(tid)); /* forwards to next layer */

	/* xxx create structure for scb_txpolicy[],
	 * use pointer to address elements, improve performance
	 */
} /* wlc_amsdu_agg_close */

/**
 * 'opens' a new AMSDU for aggregation.
 * For now update the pktflags to indicate AMSDU frame.
 */
static void *
wlc_amsdu_agg_open(amsdu_info_t *ami, wlc_bsscfg_t *bsscfg, struct ether_addr *ea, void *p)
{
	wlc_info_t *wlc = ami->wlc;
	wlc_pkttag_t *pkttag;

	BCM_REFERENCE(wlc);

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* wlc_amsdu_agg has checked PKTISTXFRAG already */

	/* When BCM_DHDHDR enabled the DHD host driver will prepare the
	 * dot3_mac_llc_snap_header, now we adjust ETHER_HDR_LEN bytes
	 * in host data addr to include the ether header 14B part.
	 * The ether header 14B in dongle D3_BUFFER is going to be used as
	 * amsdu header.
	 * Now, this msdu has all DHDHDR 22B in host.
	 */
	PKTSETFRAGDATA_LO(wlc->osh, p, LB_FRAG1,
		PKTFRAGDATA_LO(wlc->osh, p, LB_FRAG1) - ETHER_HDR_LEN);
	PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
		PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + ETHER_HDR_LEN);
	PKTSETFRAGTOTLEN(wlc->osh, p,
		PKTFRAGTOTLEN(wlc->osh, p) + ETHER_HDR_LEN);

	/* This is subframe1 */

#else
	/* check first amsdu subframe have sufficient headroom */
	if ((uint)PKTHEADROOM(wlc->osh, p) < wlc->txhroff) {
		WL_ERROR(("wl%d: %s, First AMSDU subframe headroom %d (need %d)\n",
			wlc->pub->unit, __func__, (uint)PKTHEADROOM(wlc->osh, p), wlc->txhroff));

		return NULL;
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	/* Update the PKTTAG with AMSDU flag */
	pkttag = WLPKTTAG(p);
	ASSERT(!WLPKTFLAG_AMSDU(pkttag));
	pkttag->flags |= WLF_AMSDU;

	return p;

} /* wlc_amsdu_agg_open */

/**
 * called by wlc_amsdu_agg(). May close and forward AMSDU to next software layer.
 *
 * return true on consumed, false others if
 *      -- first header buffer allocation failed
 *      -- no enough tailroom for pad bytes
 *      -- tot size goes beyond A-MSDU limit
 *
 *  amsdu_agg_p[tid] points to the header lbuf, amsdu_agg_ptail[tid] points to the tail lbuf
 *
 * The A-MSDU format typically will be below
 *   | A-MSDU header(ethernet like) |
 *	|subframe1 8023hdr |
 *		|subframe1 body | pad |
 *			|subframe2 8023hdr |
 *				|subframe2 body | pad |
 *					...
 *						|subframeN 8023hdr |
 *							|subframeN body |
 * It's not required to have pad bytes on the last frame
*/
static bool
wlc_amsdu_agg_append(amsdu_info_t *ami, struct scb *scb, void *p, uint tid,
	ratespec_t rspec)
{
	uint len, totlen;
	bool pad_end_supported = TRUE;
	osl_t *osh;
	scb_amsdu_t *scb_ami;
	void *ptid;
	amsdu_txaggstate_t *aggstate;
	uint max_agg_bytes;

	uint pkt_tail_room, pkt_head_room;
	uint8 headroom_pad_need, pad;
	WL_AMSDU(("%s\n", __FUNCTION__));

	osh = ami->pub->osh;
	pkt_tail_room = (uint)PKTTAILROOM(osh, pktlast(osh, p));
	pkt_head_room = (uint)PKTHEADROOM(osh, p);
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	aggstate = &scb_ami->aggstate[tid];
	headroom_pad_need = aggstate->headroom_pad_need;
	max_agg_bytes = wlc_amsdu_get_max_agg_bytes(scb, scb_ami, tid, p, rspec);

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* BCM_DHDHDR always pad at tail in DHD, so we always have tail room */
	pkt_tail_room = 4;
#endif // endif

	/* length of 802.3/LLC frame, equivalent to A-MSDU sub-frame length, DA/SA/Len/Data */
	len = pkttotlen(osh, p);
	/* padding of A-MSDU sub-frame to 4 bytes */
	pad = (uint)((-(int)len) & 3);
	/* START: check ok to append p to queue */
	/* if we have a pkt being agged */
	/* ensure that we can stick any padding needed on front of pkt */
	/* check here, coz later on we will append p on agg_p after adding pad */
	if (aggstate->amsdu_agg_p) {
		if (pkt_head_room < headroom_pad_need ||
			(len + headroom_pad_need + aggstate->amsdu_agg_bytes > max_agg_bytes)) {
				/* start over if we cant' make it */
				/* danger here is if no tailroom or headroom in packets */
				/* we will send out only 1 sdu long amsdu -- slow us down */
#ifdef WLCNT
				if (pkt_head_room < headroom_pad_need) {
					WLCNTINCR(ami->cnt->agg_stop_tailroom);
				} else {
					WLCNTINCR(ami->cnt->agg_stop_len);
				}
#endif /* WLCNT */
				wlc_amsdu_agg_close(ami, scb, tid); /* closes and sends AMPDU */
		}
	}

	/* END: check ok to append pkt to queue */
	/* START: allocate header */
	/* alloc new pack tx buffer if necessary */
	if (aggstate->amsdu_agg_p == NULL) {
		/* catch a common case of a stream of incoming packets with no tailroom */
		/* also throw away if headroom doesn't look like can accomodate */
		/* the assumption is that current pkt headroom is good gauge of next packet */
		/* and if both look insufiicient now, it prob will be insufficient later */
		if (pad > pkt_tail_room && pad > pkt_head_room) {
			WLCNTINCR(ami->cnt->agg_stop_tailroom);
			goto amsdu_agg_false;
		}

		if ((ptid = wlc_amsdu_agg_open(ami, SCB_BSSCFG(scb), &scb->ea, p)) == NULL) {
			WLCNTINCR(ami->cnt->agg_openfail);
			goto amsdu_agg_false;
		}

		aggstate->amsdu_agg_p = ptid;
		aggstate->amsdu_agg_ptail = ptid;
		aggstate->amsdu_agg_sframes = 0;
		aggstate->amsdu_agg_bytes = 0;
		aggstate->amsdu_agg_padlast = 0;
		WL_AMSDU(("%s: open a new AMSDU, hdr %d bytes\n",
			__FUNCTION__, aggstate->amsdu_agg_bytes));
	} else {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		/* wlc_amsdu_agg has checked PKTISTXFRAG already */

		/* When BCM_DHDHDR enabled the DHD host driver will prepare the
		 * dot3_mac_llc_snap_header, now we adjust ETHER_HDR_LEN bytes
		 * in host data addr to include the ether header 14B part.
		 * The ether header 14B in dongle D3_BUFFER is going to be used as
		 * amsdu header.
		 * Now, this msdu has all DHDHDR 22B in host.
		 */
		PKTSETFRAGDATA_LO(osh, p, LB_FRAG1,
			PKTFRAGDATA_LO(osh, p, LB_FRAG1) - ETHER_HDR_LEN);
		PKTSETFRAGLEN(osh, p, LB_FRAG1,
			PKTFRAGLEN(osh, p, LB_FRAG1) + ETHER_HDR_LEN);
		PKTSETFRAGTOTLEN(osh, p,
			PKTFRAGTOTLEN(osh, p) + ETHER_HDR_LEN);
		/* Free D3_BUFFER for not first msdu, we don't need it */
		PKTBUFEARLYFREE(osh, p);

		/* BCM_DHDHDR pad at tail in DHD always */
#else

		/* step 1: mod cur pkt to take care of prev pkt */
		if (headroom_pad_need) {
			PKTPUSH(osh, p, aggstate->headroom_pad_need);
			len += headroom_pad_need;
			aggstate->headroom_pad_need = 0;
#ifdef WLCNT
			WLCNTINCR(ami->cnt->tx_padding_in_head);
#endif /* WLCNT */
		}
#endif /* BCM_DHDHDR && DONGLEBUILD */
	}
	/* END: allocate header */

	/* use short name for convenience */
	ptid = aggstate->amsdu_agg_p;

	/* START: append packet */
	/* step 2: chain the pkts at the end of current one */
	ASSERT(aggstate->amsdu_agg_ptail != NULL);

	/* If the AMSDU header was prepended in the same packet, we have */
	/* only one packet to work with and do not need any linking. */
	if (p != aggstate->amsdu_agg_ptail) {
		/* amsdu_agg_ptail may have a NEXT pkt already
		 * So link to pktlast(amsdu_agg_ptail)
		 */
		PKTSETNEXT(osh, pktlast(osh, aggstate->amsdu_agg_ptail), p);
		aggstate->amsdu_agg_ptail = pktlast(osh, p);

		/* Append any packet callbacks from p to *ptid */
		wlc_pcb_fn_move(ami->wlc->pcb, ptid, p);
	} else {

		/* Update amsdu_agg_ptail to apply the pad to last section of the packet */
		aggstate->amsdu_agg_ptail = pktlast(osh, p);
	}

	/* step 3: update total length in agg queue */
	totlen = len + aggstate->amsdu_agg_bytes;
	/* caller already makes sure this frame fits */
	ASSERT(totlen < max_agg_bytes);

	/* END: append pkt */

	/* START: pad current
	 * If padding for this pkt (for 4 bytes alignment) is needed
	 * and feasible(enough tailroom and
	 * totlen does not exceed limit), then add it, adjust length and continue;
	 * Otherwise, close A-MSDU
	 */
	aggstate->amsdu_agg_padlast = 0;
	if (pad != 0) {
		if (BCMLFRAG_ENAB() && PKTISTXFRAG(osh, aggstate->amsdu_agg_ptail))
			pad_end_supported = FALSE;

		/* first try using tailroom -- append pad immediately */
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		BCM_REFERENCE(pad_end_supported);
		if (totlen + pad < max_agg_bytes) {
			aggstate->amsdu_agg_padlast = pad;
			/* pad data will be the garbage at the end of host data */
			PKTSETFRAGLEN(osh, p, LB_FRAG1, PKTFRAGLEN(osh, p, LB_FRAG1) + pad);
			PKTSETFRAGTOTLEN(osh, p, PKTFRAGTOTLEN(osh, p) + pad);
			totlen += pad;
#ifdef WLCNT
			WLCNTINCR(ami->cnt->tx_padding_in_tail);
#endif /* WLCNT */
		}
#else
		if (((uint)PKTTAILROOM(osh, aggstate->amsdu_agg_ptail) >= pad) &&
		    (totlen + pad < max_agg_bytes) && (pad_end_supported)) {
			aggstate->amsdu_agg_padlast = pad;
			PKTSETLEN(osh, aggstate->amsdu_agg_ptail,
				PKTLEN(osh, aggstate->amsdu_agg_ptail) + pad);
			totlen += pad;
#ifdef WLCNT
			WLCNTINCR(ami->cnt->tx_padding_in_tail);
#endif /* WLCNT */
		}
#endif /* BCM_DHDHDR && DONGLEBUILD */
		else if (totlen + pad < max_agg_bytes) {
			/* next try using headroom -- wait til next pkt to take care of padding */
			aggstate->headroom_pad_need = pad;
		} else {
			WL_AMSDU(("%s: terminate A-MSDU for tailroom/aggmax\n", __FUNCTION__));
			aggstate->amsdu_agg_sframes++;
			aggstate->amsdu_agg_bytes = totlen;

#ifdef WLCNT
			if (totlen + pad < max_agg_bytes) {
				WLCNTINCR(ami->cnt->agg_stop_tailroom);
			} else {
				WLCNTINCR(ami->cnt->agg_stop_len);
			}
#endif /* WLCNT */
			wlc_amsdu_agg_close(ami, scb, tid);
			goto amsdu_agg_true;
		}
	}
#ifdef WLCNT
	else {
		WLCNTINCR(ami->cnt->tx_padding_no_pad);
	}
#endif /* WLCNT */
	/* END: pad current */
	/* sync up agg counter */
	WL_AMSDU(("%s: add one more frame len %d pad %d\n", __FUNCTION__, len, pad));
	aggstate->amsdu_agg_sframes++;
	aggstate->amsdu_agg_bytes = totlen;

amsdu_agg_true:
	return (TRUE);

amsdu_agg_false:
	return (FALSE);
} /* wlc_amsdu_agg_append */

void
wlc_amsdu_agg_flush(amsdu_info_t *ami)
{
	struct scb *scb;
	struct scb_iter scbiter;

	if (!AMSDU_TX_SUPPORT(ami->pub))
		return;

	WL_AMSDU(("wlc_amsdu_agg_flush\n"));

	FOREACHSCB(ami->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMSDU(scb)) {
			wlc_amsdu_scb_deactive(ami, scb);
		}
	}
}

#if defined(WLOVERTHRUSTER)
/**
 * Transmit throughput enhancement, identifies TCP ACK frames to skip AMSDU agg.
 * WLOVERTHRUSTER specific.
 */
static bool
wlc_amsdu_is_tcp_ack(amsdu_info_t *ami, void *p)
{
	uint8 *ip_header;
	uint8 *tcp_header;
	uint32 eth_len;
	uint32 ip_hdr_len;
	uint32 ip_len;
	uint32 pktlen;
	uint16 ethtype;
	wlc_info_t *wlc = ami->wlc;
	osl_t *osh = wlc->osh;

	pktlen = pkttotlen(osh, p);

	/* make sure we have enough room for a minimal IP + TCP header */
	if (pktlen < (ETHER_HDR_LEN +
	              DOT11_LLC_SNAP_HDR_LEN +
	              IPV4_MIN_HEADER_LEN +
	              TCP_MIN_HEADER_LEN)) {
		return FALSE;
	}

	/* find length of ether payload */
	eth_len = pktlen - (ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN);

	/* bail out early if pkt is too big for an ACK
	 *
	 * A TCP ACK has only TCP header and no data.  Max size for both IP header and TCP
	 * header is 15 words, 60 bytes. So if the ether payload is more than 120 bytes, we can't
	 * possibly have a TCP ACK. This test optimizes an early exit for MTU sized TCP.
	 */
	if (eth_len > 120) {
		return FALSE;
	}

	ethtype = wlc_sdu_etype(wlc, p);
	if (ethtype != ntoh16(ETHER_TYPE_IP)) {
		return FALSE;
	}

	/* find protocol headers and actual IP lengths */
	ip_header = wlc_sdu_data(wlc, p);
	ip_hdr_len = IPV4_HLEN(ip_header);
	ip_len = ntoh16_ua(&ip_header[IPV4_PKTLEN_OFFSET]);

	/* check for IP VER4 and carrying TCP */
	if (IP_VER(ip_header) != IP_VER_4 ||
	    IPV4_PROT(ip_header) != IP_PROT_TCP) {
		return FALSE;
	}

	/* verify pkt len in case of ip hdr has options */
	if (eth_len < ip_hdr_len + TCP_MIN_HEADER_LEN) {
		return FALSE;
	}

	tcp_header = ip_header + ip_hdr_len;

	/* fail if no TCP ACK flag or payload bytes present
	 * (payload bytes are present if IP len is not eq to IP header + TCP header)
	 */
	if ((tcp_header[TCP_FLAGS_OFFSET] & TCP_FLAG_ACK) == 0 ||
	    ip_len != ip_hdr_len + 4 * TCP_HDRLEN(tcp_header[TCP_HLEN_OFFSET])) {
		return FALSE;
	}

	return TRUE;
} /* wlc_amsdu_is_tcp_ack */
#endif /* WLOVERTHRUSTER */

/** Return the transmit packets held by AMSDU */
static uint
wlc_amsdu_txpktcnt(void *ctx)
{
	amsdu_info_t *ami = (amsdu_info_t *)ctx;
	uint i;
	scb_amsdu_t *scb_ami;
	int pktcnt = 0;
	struct scb_iter scbiter;
	wlc_info_t *wlc = ami->wlc;
	struct scb *scb;

	FOREACHSCB(wlc->scbstate, &scbiter, scb)
		if (SCB_AMSDU(scb)) {
			scb_ami = SCB_AMSDU_CUBBY(ami, scb);
			for (i = 0; i < NUMPRIO; i++) {
				if (scb_ami->aggstate[i].amsdu_agg_p)
					pktcnt++;
			}
		}

	return pktcnt;
}

/* Return the transmit packets held by AMSDU per BSS */
uint wlc_amsdu_bss_txpktcnt(amsdu_info_t *ami, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	scb_amsdu_t *scb_ami;
	uint pktcnt = 0;
	uint i;
	wlc_info_t *wlc = ami->wlc;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb)
		if (SCB_AMSDU(scb)) {
			scb_ami = SCB_AMSDU_CUBBY(ami, scb);
			for (i = 0; i < NUMPRIO; i++) {
				if (scb_ami->aggstate[i].amsdu_agg_p)
					pktcnt++;
			}
		}
	return pktcnt;
}

static void
wlc_amsdu_flush_pkts(amsdu_info_t *ami, struct scb *scb, uint8 tid)
{
	scb_amsdu_t *scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	wlc_info_t *wlc = ami->wlc;
	amsdu_txaggstate_t *aggstate;
	void *pkt;

	aggstate = &scb_ami->aggstate[tid];
	pkt = aggstate->amsdu_agg_p;
	if (pkt) {
		PKTFREE(wlc->osh, pkt, TRUE);
		/* needs clearing, so subsequent access to this cubby doesn't ASSERT
		 * and/or access bad memory
		 */
		aggstate->amsdu_agg_p = NULL;

		WL_AMSDU(("wl%d.%d: %s flushing 1 packets for "MACF" AID %d tid %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__,
			ETHER_TO_MACF(scb->ea), SCB_AID(scb), tid));
	}
	aggstate->amsdu_agg_ptail = NULL;
	aggstate->amsdu_agg_sframes = 0;
	aggstate->amsdu_agg_bytes = 0;
	aggstate->amsdu_agg_padlast = 0;
	aggstate->headroom_pad_need = 0;
}

/* Flush transmit packets held by AMSDU per SCB/TID */
static void
wlc_amsdu_flush_scb_tid(void* ctx, struct scb *scb, uint8 tid)
{
	amsdu_info_t *ami = (amsdu_info_t *)ctx;

	wlc_amsdu_flush_pkts(ami, scb, tid);
}

static void
wlc_amsdu_scb_deactive(void *ctx, struct scb *scb)
{
	uint8 tid;
	amsdu_info_t *ami = (amsdu_info_t *)ctx;

	WL_AMSDU(("wlc_amsdu_scb_deactive scb %p\n", OSL_OBFUSCATE_BUF(scb)));
	for (tid = 0; tid < NUMPRIO; tid++) {
		wlc_amsdu_flush_scb_tid(ami, scb, tid);
	}
}

#endif /* WLAMSDU_TX */

/**
 * We should not come here typically!!!!
 * if we are here indicates we received a corrupted packet which is tagged as AMSDU by the ucode.
 * So flushing invalid AMSDU chain. When anything other than AMSDU is received when AMSDU state is
 * not idle, flush the collected intermediate amsdu packets.
 */
void BCMFASTPATH
wlc_amsdu_flush(amsdu_info_t *ami)
{
	int fifo;
	amsdu_deagg_t *deagg;

	for (fifo = 0; fifo < RX_FIFO_NUMBER; fifo++) {
		deagg = &ami->amsdu_deagg[fifo];
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_IDLE)
			wlc_amsdu_deagg_flush(ami, fifo);
	}
}

/**
 * return FALSE if filter failed
 *   caller needs to toss all buffered A-MSDUs and p
 *   Enhancement: in case of out of sequences, try to restart to
 *     deal with lost of last MSDU, which can occur frequently due to fcs error
 *   Assumes receive status is in host byte order at this point.
 *   PKTDATA points to start of receive descriptor when called.
 */
void * BCMFASTPATH
wlc_recvamsdu(amsdu_info_t *ami, wlc_d11rxhdr_t *wrxh, void *p, uint16 *padp, bool chained_sendup)
{
	osl_t *osh;
	amsdu_deagg_t *deagg;
	uint aggtype;
	int fifo;
	uint16 pad;                     /* Number of bytes of pad */
	wlc_info_t *wlc = ami->wlc;
	d11rxhdr_t *rxh;

	/* packet length starting at 802.11 mac header (first frag) or eth header (others) */
	uint32 pktlen;

	uint32 pktlen_w_plcp;           /* packet length starting at PLCP */
	struct dot11_header *h;
#ifdef BCMDBG
	int msdu_cnt = -1;              /* just used for debug */
#endif // endif
	wlc_rx_pktdrop_reason_t toss_reason; /* must be set if tossing */

#if !defined(PKTC) && !defined(PKTC_DONGLE)
	BCM_REFERENCE(chained_sendup);
#endif // endif
	osh = ami->pub->osh;

	ASSERT(padp != NULL);

	rxh = &wrxh->rxhdr;
	aggtype = RXHDR_GET_AGG_TYPE(rxh, wlc);
	pad = RXHDR_GET_PAD_LEN(rxh, wlc);

#ifdef BCMDBG
	msdu_cnt = RXHDR_GET_MSDU_COUNT(rxh, wlc);
#endif  /* BCMDBG */

	/* PKTDATA points to rxh. Get length of packet w/o rxh, but incl plcp */
	pktlen = pktlen_w_plcp = PKTLEN(osh, p) - (wlc->hwrxoff + pad);

#ifdef DONGLEBUILD
	/* XXX Manage memory pressure by right sizing the packet since there
	 * can be lots of subframes that are going to be chained together
	 * pending the reception of the final subframe with its FCS for
	 * MPDU.  Note there is a danger of a "low memory deadlock" if you
	 * exhaust the available supply of packets by chaining subframes and
	 * as result you can't rxfill the wlan rx fifo and thus receive the
	 * final AMSDU subframe or even another MPDU to invoke a AMSDU cleanup
	 * action.  Also you might be surprised this can happen, to the extreme,
	 * if you receive a normal, but corrupt, MPDU which looks like a ASMDU
	 * to ucode because it releases frames to the FIFO/driver before it sees
	 * the CRC, i.e. ucode AMSDU deagg.  A misinterpted MPDU with lots of
	 * zero data can look like a large number of 16 byte minimum ASMDU
	 * subframes.  This is more than a theory, this actually happened repeatedly
	 * in open, congested air.
	 */
	if (PKTISRXFRAG(osh, p)) {
		/* for splitRX enabled case, pkt clonning is not valid */
		/* since part of the packet is in host */
	} else {
		uint headroom;
		void *dup_p;
		uint16 pkt_len = 0;          /* packet length, including rxh */

		/* Make virtually an identical copy of the original packet with the same
		 * headroom and data.  Only the tailroom will be diffent, i.e the packet
		 * is right sized.
		 *
		 * What about making sure we have the same alignment if necessary?
		 * If you can't get a "right size" packets,just continue
		 * with the current one. You don't have much choice
		 * because the rest of the AMSDU could be in the
		 * rx dma ring and/or FIFO and be ack'ed already by ucode.
		 */
		pkt_len = PKTLEN(osh, p);

		if (pkt_len < wlc->pub->tunables->amsdu_resize_buflen) {
			headroom = PKTHEADROOM(osh, p);
			if ((dup_p = PKTGET(osh, headroom + pkt_len,
				FALSE)) != NULL) {
#ifdef BCMPCIEDEV
				if (BCMPCIEDEV_ENAB()) {
					rxcpl_info_t *p_rxcpl_info = bcm_alloc_rxcplinfo();
					if (p_rxcpl_info == NULL) {
						WL_ERROR(("%s:  RXCPLID not free\n", __FUNCTION__));
						/* try to send an error to host */
						PKTFREE(osh, dup_p, FALSE);
						ASSERT(p_rxcpl_info);
						goto skip_amsdu_resize;
					}
					PKTSETRXCPLID(osh, dup_p, p_rxcpl_info->rxcpl_id.idx);
				}
#endif /* BCMPCIEDEV */
				PKTPULL(osh, dup_p, headroom);
				bcopy(PKTDATA(osh, p) - headroom, PKTDATA(osh, dup_p) - headroom,
					PKTLEN(osh, p) + headroom);
				PKTFREE(osh, p, FALSE);
				p = dup_p;
			} else {
#ifdef BCMPCIEDEV
				/*
				 * For splitrx cases this is fatal if no more packets are posted
				 * to classified fifo
				*/
				if (BCMPCIEDEV_ENAB() &&
					(dma_rxactive(WLC_HW_DI(wlc, PKT_CLASSIFY_FIFO)) == 0)) {
					WL_ERROR(("%s: AMSDU resizing packet alloc failed \n",
						__FUNCTION__));
					ASSERT(0);
				}
#endif /* BCMPCIEDEV */
			}
		}
	}
#ifdef BCMPCIEDEV
skip_amsdu_resize:
#endif // endif
#endif /* DONGLEBUILD */
	fifo = (D11RXHDR_ACCESS_VAL(&wrxh->rxhdr, ami->pub->corerev, fifo));
	ASSERT((fifo < RX_FIFO_NUMBER));
	deagg = &ami->amsdu_deagg[fifo];

	WLCNTINCR(ami->cnt->deagg_msdu);

	WL_AMSDU(("%s: aggtype %d, msdu count %d\n", __FUNCTION__, aggtype, msdu_cnt));

	h = (struct dot11_header *)(PKTDATA(osh, p) + wlc->hwrxoff + pad +
		D11_PHY_RXPLCP_LEN(wlc->pub->corerev));

	switch (aggtype) {
	case RXS_AMSDU_FIRST:
		/* PKTDATA starts with PLCP */
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_IDLE) {
			WL_AMSDU(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			wlc_amsdu_deagg_flush(ami, fifo);
			/* keep this valid one and reset to improve throughput */
		}

		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_FIRST;

		/* Store the frontpad value of the first subframe */
		deagg->first_pad = *padp;

		if (!wlc_amsdu_deagg_open(ami, fifo, p, h, pktlen_w_plcp)) {
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_OPEN;
			goto abort;
		}

		/* Packet length w/o PLCP */
		pktlen = pktlen_w_plcp - D11_PHY_RXPLCP_LEN(wlc->pub->corerev);

		WL_AMSDU(("%s: first A-MSDU buffer\n", __FUNCTION__));
		break;

	case RXS_AMSDU_INTERMEDIATE:
		/* PKTDATA starts with subframe header */
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_FIRST) {
			WL_AMSDU_ERROR(("%s: wrong A-MSDU deagg sequence, cur_state=%d, agg=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state, aggtype));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SEQ;
			goto abort;
		}

#ifdef ASSERT
		/* intermediate frames should have 2 byte padding if wlc->hwrxoff is aligned
		* on mod 4 address
		*/
		if ((wlc->hwrxoff % 4) == 0) {
			ASSERT(pad != 0);
		} else {
			ASSERT(pad == 0);
		}
#endif /* ASSERT */

		if ((pktlen) < ETHER_HDR_LEN) {
			WL_AMSDU_ERROR(("%s: rxrunt, agg=%d\n", __FUNCTION__, aggtype));
			WLCNTINCR(ami->pub->_cnt->rxrunt);
			toss_reason = RX_PKTDROP_RSN_RUNT_FRAME;
			goto abort;

		}

		ASSERT(deagg->amsdu_deagg_ptail);
		PKTSETNEXT(osh, deagg->amsdu_deagg_ptail, p);
		deagg->amsdu_deagg_ptail = p;
		WL_AMSDU(("%s:   mid A-MSDU buffer\n", __FUNCTION__));
		break;
	case RXS_AMSDU_LAST:
		/* PKTDATA starts with last subframe header */
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_FIRST) {
			WL_AMSDU_ERROR(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SEQ;
			goto abort;
		}

		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_LAST;

#ifdef ASSERT
		/* last frame should have 2 byte padding if wlc->hwrxoff is aligned
		* on mod 4 address
		*/
		if ((wlc->hwrxoff % 4) == 0) {
			ASSERT(pad != 0);
		} else {
			ASSERT(pad == 0);
		}
#endif /* ASSERT */

		if ((pktlen) < (ETHER_HDR_LEN + (PKTISRXFRAG(osh, p) ? 0 :  DOT11_FCS_LEN))) {
			WL_AMSDU_ERROR(("%s: rxrunt\n", __FUNCTION__));
			WLCNTINCR(ami->pub->_cnt->rxrunt);
			toss_reason = RX_PKTDROP_RSN_RUNT_FRAME;
			goto abort;
		}

		ASSERT(deagg->amsdu_deagg_ptail);
		PKTSETNEXT(osh, deagg->amsdu_deagg_ptail, p);
		deagg->amsdu_deagg_ptail = p;
		WL_AMSDU(("%s: last A-MSDU buffer\n", __FUNCTION__));
		break;

	case RXS_AMSDU_N_ONE:
		/* this frame IS AMSDU, checked by caller */

		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_IDLE) {
			WL_AMSDU(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			wlc_amsdu_deagg_flush(ami, fifo);

			/* keep this valid one and reset to improve throughput */
		}

		ASSERT((deagg->amsdu_deagg_p == NULL) && (deagg->amsdu_deagg_ptail == NULL));
		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_LAST;

		/* Store the frontpad value of this single subframe */
		deagg->first_pad = *padp;

		if (!wlc_amsdu_deagg_open(ami, fifo, p, h, pktlen_w_plcp)) {
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_OPEN;
			goto abort;
		}

		/* Packet length w/o PLCP */
		pktlen = pktlen_w_plcp - D11_PHY_RXPLCP_LEN(wlc->pub->corerev);

		break;

	default:
		/* can't be here */
		ASSERT(0);
		toss_reason = RX_PKTDROP_RSN_BAD_AGGTYPE;
		goto abort;
	}

	/* Note that pkttotlen now includes the length of the rxh for each frag */
	WL_AMSDU(("%s: add one more A-MSDU buffer %d bytes, accumulated %d bytes\n",
		__FUNCTION__, pktlen_w_plcp, pkttotlen(osh, deagg->amsdu_deagg_p)));

	if (deagg->amsdu_deagg_state == WLC_AMSDU_DEAGG_LAST) {
		void *pp = deagg->amsdu_deagg_p;
#ifdef WL11K
		uint tot_len = pkttotlen(osh, pp);
#endif // endif
		deagg->amsdu_deagg_p = deagg->amsdu_deagg_ptail = NULL;
		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_IDLE;

		/* ucode/hw deagg happened */

/* XXX This will ASSERT() in wlc_sendup_chain
 * if PKTC or PKTC_DONGLE is not defined
 */
		WLPKTTAG(pp)->flags |= WLF_HWAMSDU;

		/* First frame has fully defined Receive Frame Header,
		 * handle it to normal MPDU process.
		 */
		WLCNTINCR(ami->pub->_cnt->rxfrag);
		WLCNTINCR(ami->cnt->deagg_amsdu);
#ifdef WL11K
		WLCNTADD(ami->cnt->deagg_amsdu_bytes_l, tot_len);
		if (ami->cnt->deagg_amsdu_bytes_l < tot_len)
			WLCNTINCR(ami->cnt->deagg_amsdu_bytes_h);
#endif // endif

#if defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU))

		/* Update the murate to the plcp
		 * last rxhdr has murate information
		 * plcp is in the first packet
		 */
		if (MU_RX_ENAB(ami->wlc) && wlc_murx_active(wlc->murx)) {
			wlc_d11rxhdr_t *frag_wrxh;   /* rx status of an AMSDU frag in chain */
			d11rxhdr_t *_rxh;
			uchar *plcp;
			uint16 pad_cnt;
			frag_wrxh = (wlc_d11rxhdr_t *)PKTDATA(osh, pp);
			_rxh = &frag_wrxh->rxhdr;

			/* Check for short or long format */
			pad_cnt = RXHDR_GET_PAD_LEN(_rxh, wlc);
			plcp = (uchar *)(PKTDATA(wlc->osh, pp) + wlc->hwrxoff + pad_cnt);

			wlc_bmac_upd_murate(ami->wlc, &wrxh->rxhdr, plcp);
		}
#endif /* WL_MU_RX */

#if defined(PKTC) || defined(PKTC_DONGLE)
		/* if chained sendup, return back head pkt and front padding of first sub-frame */
		/* if unchained wlc_recvdata takes pkt till bus layer */
		if (chained_sendup == TRUE) {
			*padp = deagg->first_pad;
			deagg->first_pad = 0;
			return (pp);
		} else
#endif // endif
		{
			/* Strip rxh from all amsdu frags in amsdu chain before send up */
			void *np = pp;
			uint16 pad_cnt;
			wlc_d11rxhdr_t *frag_wrxh = NULL;   /* Subframe rxstatus */
			d11rxhdr_t *_rxh;

			/* Loop through subframes to remove rxstatus */
			while (np) {
				frag_wrxh = (wlc_d11rxhdr_t*) PKTDATA(osh, np);
				_rxh = &frag_wrxh->rxhdr;
				pad_cnt = RXHDR_GET_PAD_LEN(_rxh, wlc);

				PKTPULL(osh, np, wlc->hwrxoff + pad_cnt);
				np = PKTNEXT(osh, np);
			}
			ASSERT(frag_wrxh);
			wlc_recvdata(ami->wlc, ami->pub->osh, frag_wrxh, pp);
		}
		deagg->first_pad = 0;
	}

	/* all other cases needs no more action, just return */
	return  NULL;

abort:
	wlc_amsdu_deagg_flush(ami, fifo);
	RX_PKTDROP_COUNT(wlc, NULL, toss_reason);
	PKTFREE(osh, p, FALSE);
	return  NULL;
} /* wlc_recvamsdu */

/** return FALSE if A-MSDU verification failed */
static bool BCMFASTPATH
wlc_amsdu_deagg_verify(amsdu_info_t *ami, uint16 fc, void *h)
{
	bool is_wds;
	uint16 *pqos;
	uint16 qoscontrol;

	BCM_REFERENCE(ami);

	/* it doesn't make sense to aggregate other type pkts, toss them */
	if ((fc & FC_KIND_MASK) != FC_QOS_DATA) {
		WL_AMSDU(("wlc_amsdu_deagg_verify fail: fc 0x%x is not QoS data type\n", fc));
		return FALSE;
	}

	is_wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	pqos = (uint16*)((uchar*)h + (is_wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN));
	qoscontrol = ltoh16_ua(pqos);

	if (qoscontrol & QOS_AMSDU_MASK)
		return TRUE;

	WL_AMSDU_ERROR(("%s fail: qos field 0x%x\n", __FUNCTION__, *pqos));
	return FALSE;
}

/**
 * Start a new AMSDU receive chain. Verifies that the frame is a data frame
 * with QoS field indicating AMSDU, and that the frame is long enough to
 * include PLCP, 802.11 mac header, QoS field, and AMSDU subframe header.
 * Inputs:
 *   ami    - AMSDU state
 *   fifo   - queue on which current frame was received
 *   p      - first frag in a sequence of AMSDU frags. PKTDATA(p) points to
 *            start of receive descriptor
 *   h      - start of ethernet header in received frame
 *   pktlen - frame length, starting at PLCP
 *
 * Returns:
 *   TRUE if new AMSDU chain is started
 *   FALSE otherwise
 */
static bool BCMFASTPATH
wlc_amsdu_deagg_open(amsdu_info_t *ami, int fifo, void *p, struct dot11_header *h, uint32 pktlen)
{
	osl_t *osh = ami->pub->osh;
	amsdu_deagg_t *deagg = &ami->amsdu_deagg[fifo];
	uint16 fc;

	BCM_REFERENCE(osh);

	if (pktlen < (uint32)(D11_PHY_RXPLCP_LEN(ami->pub->corerev) + DOT11_MAC_HDR_LEN
		+ DOT11_QOS_LEN + ETHER_HDR_LEN)) {
		WL_AMSDU(("%s: rxrunt\n", __FUNCTION__));
		WLCNTINCR(ami->pub->_cnt->rxrunt);
		goto fail;
	}

	fc = ltoh16(h->fc);

	if (!wlc_amsdu_deagg_verify(ami, fc, h)) {
		WL_AMSDU(("%s: AMSDU verification failed, toss\n", __FUNCTION__));
		WLCNTINCR(ami->cnt->deagg_badfmt);
		goto fail;
	}

	/* explicitly test bad src address to avoid sending bad deauth */
	if ((ETHER_ISNULLADDR(&h->a2) || ETHER_ISMULTI(&h->a2))) {
		WL_AMSDU(("%s: wrong address 2\n", __FUNCTION__));
		WLCNTINCR(ami->pub->_cnt->rxbadsrcmac);
		goto fail;
	}

	deagg->amsdu_deagg_p = p;
	deagg->amsdu_deagg_ptail = p;
	return TRUE;

fail:
	WLCNTINCR(ami->cnt->deagg_openfail);
	return FALSE;
} /* wlc_amsdu_deagg_open */

static void BCMFASTPATH
wlc_amsdu_deagg_flush(amsdu_info_t *ami, int fifo)
{
	amsdu_deagg_t *deagg = &ami->amsdu_deagg[fifo];
	WL_AMSDU(("%s\n", __FUNCTION__));

	if (deagg->amsdu_deagg_p)
		PKTFREE(ami->pub->osh, deagg->amsdu_deagg_p, FALSE);

	deagg->first_pad = 0;
	deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_IDLE;
	deagg->amsdu_deagg_p = deagg->amsdu_deagg_ptail = NULL;
#ifdef WLCNT
	WLCNTINCR(ami->cnt->deagg_flush);
#endif /* WLCNT */
}

#if defined(PKTC) || defined(PKTC_DONGLE)
static void
wlc_amsdu_to_dot11(amsdu_info_t *ami, struct scb *scb, struct dot11_header *hdr, void *pkt)
{
	osl_t *osh;

	/* ptr to 802.3 or eh in incoming pkt */
	struct ether_header *eh;
	struct dot11_llc_snap_header *lsh;

	/* ptr to 802.3 or eh in new pkt */
	struct ether_header *neh;
	struct dot11_header *phdr;

	wlc_bsscfg_t *cfg = scb->bsscfg;
	osh = ami->pub->osh;

	/* If we discover an ethernet header, replace it by an 802.3 hdr + SNAP header */
	eh = (struct ether_header *)PKTDATA(osh, pkt);

	if (ntoh16(eh->ether_type) > ETHER_MAX_LEN) {
		neh = (struct ether_header *)PKTPUSH(osh, pkt, DOT11_LLC_SNAP_HDR_LEN);

		/* Avoid constructing 802.3 header as optimization.
		 * 802.3 header(14 bytes) is going to be overwritten by the 802.11 header.
		 * This will save writing 14-bytes for every MSDU.
		 */

		/* Construct LLC SNAP header */
		lsh = (struct dot11_llc_snap_header *)
			((char *)neh + ETHER_HDR_LEN);
		lsh->dsap = 0xaa;
		lsh->ssap = 0xaa;
		lsh->ctl = 0x03;
		lsh->oui[0] = 0;
		lsh->oui[1] = 0;
		lsh->oui[2] = 0;
		/* The snap type code is already in place, inherited from the ethernet header that
		 * is now overlaid.
		 */
	}
	else {
		neh = (struct ether_header *)PKTDATA(osh, pkt);
	}

	if (BSSCFG_AP(cfg)) {
		/* Force the 802.11 a2 address to be the ethernet source address */
		bcopy((char *)neh->ether_shost,
			(char *)&hdr->a2, ETHER_ADDR_LEN);
	} else {
		if (BSSCFG_IBSS(cfg)) {
			/* Force the 802.11 a3 address to be the ethernet source address
			 * IBSS has BSS as a3, so leave a3 alone for win7+
			 */
			bcopy((char *)neh->ether_shost,
				(char *)&hdr->a3, ETHER_ADDR_LEN);
		}
	}

	/* Replace the 802.3 header, if present, by an 802.11 header. The original 802.11 header
	 * was appended to the frame along with the receive data needed by Microsoft.
	 */
	phdr = (struct dot11_header *)
		PKTPUSH(osh, pkt, DOT11_A3_HDR_LEN - ETHER_HDR_LEN);

	bcopy((char *)hdr, (char *)phdr, DOT11_A3_HDR_LEN);

	/* Clear all frame control bits except version, type, data subtype & from-ds/to-ds */
	phdr->fc = htol16(ltoh16(phdr->fc) & (FC_FROMDS | FC_TODS | FC_TYPE_MASK |
		(FC_SUBTYPE_MASK & ~QOS_AMSDU_MASK) | FC_PVER_MASK));
} /* wlc_amsdu_to_dot11 */

#endif /* PKTC || PKTC_DONGLE */

/**
 * Called when the 'WLF_HWAMSDU' flag is set in the PKTTAG of a received frame.
 * A-MSDU decomposition: break A-MSDU(chained buffer) to individual buffers
 *
 *    | 80211 MAC HEADER | subFrame 1 |
 *			               --> | subFrame 2 |
 *			                                 --> | subFrame 3... |
 * where, 80211 MAC header also includes QOS and/or IV fields
 *        f->p, at function entry, has PKTDATA() pointing at the start of the 802.11 mac header.
 *        f->pbody points to beginning of subFrame 1,
 *        f->totlen is the total body len(chained, after mac/qos/iv header) w/o icv and FCS
 *
 *        each subframe is in the form of | 8023hdr | body | pad |
 *                subframe other than the last one may have pad bytes
*/
void
wlc_amsdu_deagg_hw(amsdu_info_t *ami, struct scb *scb, struct wlc_frminfo *f)
{
	osl_t *osh;
	void *sf[MAX_RX_SUBFRAMES], *newpkt;
	struct ether_header *eh;
	uint32 body_offset, sflen = 0, len = 0;
	uint num_sf = 0, i;
	int resid;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	uint32 amsdu_bytes = 0;
#endif /* BCMDBG || BCMDBG_AMSDU */
	wlc_rx_pktdrop_reason_t toss_reason; /* must be set if tossing */

	ASSERT(WLPKTTAG(f->p)->flags & WLF_HWAMSDU);
	osh = ami->pub->osh;

	/* strip mac header, move to start from A-MSDU body */
	body_offset = (uint)(f->pbody - (uchar*)PKTDATA(osh, f->p));
	PKTPULL(osh, f->p, body_offset);

	WL_AMSDU(("wlc_amsdu_deagg_hw: body_len(exclude icv and FCS) %d\n", f->totlen));

	resid = f->totlen;
	newpkt = f->p;

	/* break chained AMSDU into N independent MSDU */
	while (newpkt != NULL) {
		/* there must be a limit to stop in order to prevent memory/stack overflow */
		if (num_sf >= MAX_RX_SUBFRAMES) {
			WL_AMSDU_ERROR(("%s: more than %d MSDUs !\n", __FUNCTION__, num_sf));
			break;
		}

		/* each subframe is 802.3 frame */
		eh = (struct ether_header*) PKTDATA(osh, newpkt);

		len = PKTLEN(osh, newpkt) + PKTFRAGUSEDLEN(osh, newpkt);

#ifdef BCMPCIEDEV
		if (BCMPCIEDEV_ENAB() && PKTISHDRCONVTD(osh, newpkt)) {

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
			amsdu_bytes += (uint16)PKTFRAGUSEDLEN(osh, newpkt);
#endif /* BCMDBG || BCMDBG_AMSDU */

			/* Skip header conversion */
		        goto skip_conv;
		}
#endif /* BCMPCIEDEV */

		sflen = ntoh16(eh->ether_type) + ETHER_HDR_LEN;

		if ((((uintptr)eh + (uint)ETHER_HDR_LEN) % 4)  != 0) {
			WL_AMSDU_ERROR(("%s: sf body is not 4 bytes aligned!\n", __FUNCTION__));
			WLCNTINCR(ami->cnt->deagg_badsfalign);
			toss_reason = RX_PKTDROP_RSN_DEAGG_UNALIGNED;
			goto toss;
		}

		/* last MSDU: has FCS, but no pad, other MSDU: has pad, but no FCS */
		if (len != (PKTNEXT(osh, newpkt) ? ROUNDUP(sflen, 4) : sflen)) {
			WL_AMSDU_ERROR(("%s: len mismatch buflen %d sflen %d, sf %d\n",
				__FUNCTION__, len, sflen, num_sf));
#ifdef RX_DEBUG_ASSERTS
			WL_ERROR(("%s : LEN mismatch : head pkt %p new pkt %p"
				"FC 0x%02x seq 0x%02x \n",
				__FUNCTION__, f->p, newpkt, f->fc, f->seq));
			/* Dump previous RXS info */
			wlc_print_prev_rxs(ami->wlc);
			ASSERT(0);
#endif /* RX_DEBUG_ASSERTS */
			WLCNTINCR(ami->cnt->deagg_badsflen);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SF_LEN;
			goto toss;
		}

		/* strip trailing optional pad */
		if (PKTFRAGUSEDLEN(osh, newpkt)) {
			/* set total length to sflen */
			PKTSETFRAGUSEDLEN(osh, newpkt, (sflen - PKTLEN(osh, newpkt)));
		} else {
			PKTSETLEN(osh, newpkt, sflen);
		}

		{
			/* convert 8023hdr to ethernet if necessary */
			wlc_8023_etherhdr(ami->wlc, osh, newpkt);
		}

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
		amsdu_bytes += (uint16)PKTLEN(osh, newpkt) +
				(uint16)PKTFRAGUSEDLEN(osh, newpkt);
#endif /* BCMDBG || BCMDBG_AMSDU */

#ifdef BCMPCIEDEV
skip_conv:
#endif /* BCMPCIEDEV */
		/* propagate prio, NO need to transfer other tags, it's plain stack packet now */
		PKTSETPRIO(newpkt, f->prio);

		WL_AMSDU(("wlc_amsdu_deagg_hw: deagg MSDU buffer %d, frame %d\n", len, sflen));

		sf[num_sf] = newpkt;
		num_sf++;
		newpkt = PKTNEXT(osh, newpkt);

		resid -= len;
	}

	if (resid != 0) {
		ASSERT(0);
		WLCNTINCR(ami->cnt->deagg_badtotlen);
		toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SF_LEN;
		goto toss;
	}

	/* cut the chain: set PKTNEXT to NULL */
	for (i = 0; i < num_sf; i++)
		PKTSETNEXT(osh, sf[i], NULL);

	/* toss the remaining MSDU, which we couldn't handle */
	if (newpkt != NULL) {
		WL_AMSDU_ERROR(("%s: toss MSDUs > %d !\n", __FUNCTION__, num_sf));
		PKTFREE(osh, newpkt, FALSE);
	}

	/* forward received data in host direction */
	for (i = 0; i < num_sf; i++) {
		struct ether_addr * ea;

		WL_AMSDU(("wlc_amsdu_deagg_hw: sendup subframe %d\n", i));

		ea = (struct ether_addr *) PKTDATA(osh, sf[i]);
		f->da = ea;
		eh = (struct ether_header*)PKTDATA(osh, sf[i]);
		if ((ntoh16(eh->ether_type) == ETHER_TYPE_802_1X)) {
#if defined(BCMSPLITRX)
			if ((BCMSPLITRX_ENAB()) && i &&
				(PKTFRAGUSEDLEN(ami->wlc->osh, sf[i]) > 0)) {
				/* Fetch susequent subframes */
				f->p = sf[i];
				wlc_recvdata_schedule_pktfetch(ami->wlc, scb, f, FALSE, TRUE, TRUE);
				continue;
			}
#endif /* BCMSPLITRX */
			/* Call process EAP frames */
			if (wlc_process_eapol_frame(ami->wlc, scb->bsscfg, scb, f, sf[i])) {
				/* We have consumed the pkt drop and continue; */
				WL_AMSDU(("Processed First fetched msdu %p\n", (void *)sf[i]));
				PKTFREE(osh, sf[i], FALSE);
				continue;
			}
		}

		f->p = sf[i];
		wlc_recvdata_sendup_msdus(ami->wlc, scb, f);
	}

	WL_AMSDU(("wlc_amsdu_deagg_hw: this A-MSDU has %d MSDU, done\n", num_sf));

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	WLCNTINCR(ami->amdbg->rx_msdu_histogram[MIN((num_sf - 1), AMSDU_RX_SUBFRAMES_BINS-1)]);
	WLCNTINCR(ami->amdbg->rx_length_histogram[MIN(amsdu_bytes/AMSDU_LENGTH_BIN_BYTES,
			AMSDU_LENGTH_BINS-1)]);
#endif /* BCMDBG */

#ifdef WLSCB_HISTO
	/* Increment musdu count from 2nd sub-frame onwards
	 * as 1st sub-frame is counted as mpdu
	 */
	if (num_sf) {
#if defined(WLCFP)
		/* If CFP is enabled, amsdu already acounted, skip here */
		if (!CFP_RCB_ENAB(ami->wlc->cfp))
#endif /* WLCFP */
		{
			WLSCB_HISTO_RX_INC_RECENT(scb, (num_sf-1));
		}
	}
#endif /* WLSCB_HISTO */

	return;

toss:
	RX_PKTDROP_COUNT(ami->wlc, scb, toss_reason);
#ifndef WL_PKTDROP_STATS
	wlc_log_unexpected_rx_frame_log_80211hdr(ami->wlc, f->h, toss_reason);
#endif // endif
	/*
	 * toss the whole A-MSDU since we don't know where the error starts
	 *  e.g. a wrong subframe length for mid frame can slip through the ucode
	 *       and the only syptom may be the last MSDU frame has the mismatched length.
	 */
	for (i = 0; i < num_sf; i++)
		sf[i] = NULL;

	WL_AMSDU(("%s: tossing amsdu in deagg -- error seen\n", __FUNCTION__));
	PKTFREE(osh, f->p, FALSE);
} /* wlc_amsdu_deagg_hw */

#if defined(PKTC) || defined(PKTC_DONGLE)
/** Packet chaining (pktc) specific AMSDU receive function */
int32 BCMFASTPATH
wlc_amsdu_pktc_deagg_hw(amsdu_info_t *ami, void **pp, wlc_rfc_t *rfc, uint16 *index,
                        bool *chained_sendup, struct scb *scb, uint16 sec_offset)
{
	osl_t *osh;
	wlc_info_t *wlc = ami->wlc;
	void *newpkt, *head, *tail, *tmp_next;
	struct ether_header *eh;
	uint16 sflen = 0, len = 0;
	uint16 num_sf = 0;
	int resid = 0;
	uint8 *da;
	struct dot11_header hdr_copy;
	char *start;
	wlc_pkttag_t * pkttag  = NULL;
	wlc_bsscfg_t *bsscfg = NULL;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	uint16 amsdu_bytes = 0;
#endif /* BCMDBG || BCMDBG_AMSDU */
	wlc_rx_pktdrop_reason_t toss_reason; /* must be set if tossing */

	pkttag = WLPKTTAG(*pp);
	ASSERT(pkttag->flags & WLF_HWAMSDU);

	osh = ami->pub->osh;
	newpkt = tail = head = *pp;
	resid = pkttotlen(osh, head);

	/* converted frame doesnt have FCS bytes */
	if (!PKTISHDRCONVTD(osh, head))
		resid -= DOT11_FCS_LEN;

	bsscfg = SCB_BSSCFG(scb);

	if (bsscfg->wlcif->if_flags & WLC_IF_PKT_80211) {
		ASSERT((sizeof(hdr_copy) + sec_offset) < (uint16)PKTHEADROOM(osh, newpkt));
		start = (char *)(PKTDATA(osh, newpkt) - sizeof(hdr_copy) - sec_offset);

		/* Save the header before being overwritten */
		bcopy((char *)start + sizeof(rx_ctxt_t), (char *)&hdr_copy, DOT11_A3_HDR_LEN);

	}

	/* insert MSDUs in to current packet chain */
	while (newpkt != NULL) {
		/* strip off FCS in last MSDU */
		if (PKTNEXT(osh, newpkt) == NULL)
		{
			PKTFRAG_TRIM_TAILBYTES(osh, newpkt,
					DOT11_FCS_LEN, TAIL_BYTES_TYPE_FCS);
		}

		/* there must be a limit to stop in order to prevent memory/stack overflow */
		if (num_sf >= MAX_RX_SUBFRAMES) {
			WL_AMSDU_ERROR(("%s: more than %d MSDUs !\n", __FUNCTION__, num_sf));
			break;
		}

		/* Frame buffer still points to the start of the receive descriptor. For each
		 * MPDU in chain, move pointer past receive descriptor.
		 */
		if ((WLPKTTAG(newpkt)->flags & WLF_HWAMSDU) == 0) {
			wlc_d11rxhdr_t *wrxh;
			uint pad;

			/* determine whether packet has 2-byte pad */
			wrxh = (wlc_d11rxhdr_t*) PKTDATA(osh, newpkt);
			pad = RXHDR_GET_PAD_LEN(&wrxh->rxhdr, wlc);

			PKTPULL(wlc->osh, newpkt, wlc->hwrxoff + pad);
			resid -= (wlc->hwrxoff + pad);
		}

		/* each subframe is 802.3 frame */
		eh = (struct ether_header *)PKTDATA(osh, newpkt);
		len = (uint16)PKTLEN(osh, newpkt) + (uint16)PKTFRAGUSEDLEN(osh, newpkt);

#ifdef BCMPCIEDEV
		if (BCMPCIEDEV_ENAB() && PKTISHDRCONVTD(osh, newpkt)) {

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
			amsdu_bytes += (uint16)PKTFRAGUSEDLEN(osh, newpkt);
#endif /* BCMDBG || BCMDBG_AMSDU */

			/* Allready converted */
			goto skip_conv;
		}
#endif /* BCMPCIEDEV */

		sflen = NTOH16(eh->ether_type) + ETHER_HDR_LEN;

		if ((((uintptr)eh + (uint)ETHER_HDR_LEN) % 4) != 0) {
			WL_AMSDU_ERROR(("%s: sf body is not 4b aligned!\n", __FUNCTION__));
			WLCNTINCR(ami->cnt->deagg_badsfalign);
			toss_reason = RX_PKTDROP_RSN_DEAGG_UNALIGNED;
			goto toss;
		}

		/* last MSDU: has FCS, but no pad, other MSDU: has pad, but no FCS */
		if (len != (PKTNEXT(osh, newpkt) ? ROUNDUP(sflen, 4) : sflen)) {
			WL_AMSDU_ERROR(("%s: len mismatch buflen %d sflen %d, sf %d\n",
				__FUNCTION__, len, sflen, num_sf));
			WLCNTINCR(ami->cnt->deagg_badsflen);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SF_LEN;
			goto toss;
		}

		/* strip trailing optional pad */
		if (PKTFRAGUSEDLEN(osh, newpkt)) {
			PKTSETFRAGUSEDLEN(osh, newpkt, (sflen - PKTLEN(osh, newpkt)));
		} else {
			PKTSETLEN(osh, newpkt, sflen);
		}

		if (bsscfg->wlcif->if_flags & WLC_IF_PKT_80211) {
			/* convert 802.3 to 802.11 */
			wlc_amsdu_to_dot11(ami, scb, &hdr_copy, newpkt);
			if (*pp != newpkt) {
				wlc_pkttag_t * new_pkttag = WLPKTTAG(newpkt);

				new_pkttag->rxchannel = pkttag->rxchannel;
				new_pkttag->pktinfo.misc.rssi = pkttag->pktinfo.misc.rssi;
				new_pkttag->rspec = pkttag->rspec;
			}

		} else {
			/* convert 8023hdr to ethernet if necessary */
			wlc_8023_etherhdr(wlc, osh, newpkt);
		}

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
		amsdu_bytes += (uint16)PKTLEN(osh, newpkt) +
				(uint16)PKTFRAGUSEDLEN(osh, newpkt);
#endif /* BCMDBG || BCMDBG_AMSDU */

#ifdef BCMPCIEDEV
skip_conv:
#endif /* BCMPCIEDEV */

		eh = (struct ether_header *)PKTDATA(osh, newpkt);
#ifdef PSTA
		if (BSSCFG_STA(rfc->bsscfg) &&
		    PSTA_IS_REPEATER(wlc) &&
		    TRUE)
			da = (uint8 *)&rfc->ds_ea;
		else
#endif // endif
			da = eh->ether_dhost;

		if (ETHER_ISNULLDEST(da)) {
			toss_reason = RX_PKTDROP_RSN_BAD_MAC_DA;
			goto toss;
		}

		if (*chained_sendup) {
			/* Init DA for the first valid packet of the chain */
			if (!PKTC_HDA_VALID(wlc->pktc_info)) {
				eacopy((char*)(da), PKTC_HDA(wlc->pktc_info));
				PKTC_HDA_VALID_SET(wlc->pktc_info, TRUE);
			}

			*chained_sendup = !ETHER_ISMULTI(da) &&
				!eacmp(PKTC_HDA(wlc->pktc_info), da) &&
#if defined(HNDCTF) && !defined(BCM_GMAC3)
				CTF_HOTBRC_CMP(wlc->pub->brc_hot, da, rfc->wlif_dev) &&
#endif /* HNDCTF && ! BCM_GMAC3 */
#if defined(PKTC_TBL)
#if defined(BCM_PKTFWD)
				wl_pktfwd_match(da, rfc->wlif_dev) &&
#else
#if !defined(WL_EAP_WLAN_ONLY_UL_PKTC)
				/* XXX: With uplink chaining controlled by rfc da and scb till
					the wlan exit, the following check is not needed for this
					feature
				*/
				PKTC_TBL_FN_CMP(wlc->pub->pktc_tbl, da, rfc->wlif_dev) &&
#endif /* !WL_EAP_WLAN_ONLY_UL_PKTC */
#endif /* BCM_PKTFWD */
#endif /* PKTC_TBL */
				((eh->ether_type == HTON16(ETHER_TYPE_IP)) ||
				(eh->ether_type == HTON16(ETHER_TYPE_IPV6)));
		}

		WL_AMSDU(("%s: deagg MSDU buffer %d, frame %d\n",
		          __FUNCTION__, len, sflen));

		/* remove from AMSDU chain and insert in to MPDU chain. skip
		 * the head MSDU since it is already in chain.
		 */
		tmp_next = PKTNEXT(osh, newpkt);
		if (num_sf > 0) {
			/* remove */
			PKTSETNEXT(osh, head, tmp_next);
			PKTSETNEXT(osh, newpkt, NULL);
			/* insert */
			PKTSETCLINK(newpkt, PKTCLINK(tail));
			PKTSETCLINK(tail, newpkt);
			tail = newpkt;
			/* set prio */
			PKTSETPRIO(newpkt, PKTPRIO(head));
			WLPKTTAGSCBSET(newpkt, rfc->scb);
		}

		*pp = newpkt;
		PKTCADDLEN(head, len);
		PKTSETCHAINED(wlc->osh, newpkt);

		num_sf++;
		newpkt = tmp_next;
		resid -= len;
	}

	if (resid != 0) {
		ASSERT(0);
		WLCNTINCR(ami->cnt->deagg_badtotlen);
		toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_LEN;
		goto toss;
	}

	/* toss the remaining MSDU, which we couldn't handle */
	if (newpkt != NULL) {
		WL_AMSDU_ERROR(("%s: toss MSDUs > %d !\n", __FUNCTION__, num_sf));
		PKTFREE(osh, newpkt, FALSE);
	}

	WL_AMSDU(("%s: this A-MSDU has %d MSDUs, done\n", __FUNCTION__, num_sf));

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	WLCNTINCR(ami->amdbg->rx_msdu_histogram[MIN((num_sf - 1), AMSDU_RX_SUBFRAMES_BINS-1)]);
	WLCNTINCR(ami->amdbg->rx_length_histogram[MIN(amsdu_bytes/AMSDU_LENGTH_BIN_BYTES,
			AMSDU_LENGTH_BINS-1)]);
#endif /* BCMDBG || BCMDBG_AMSDU */

#ifdef WLSCB_HISTO
	/* Increment musdu count from 2nd sub-frame onwards
	 * as 1st sub-frame is counted as mpdu
	 */
	if (num_sf) {
#if defined(WLCFP)
		/* If CFP is enabled, amsdu already acounted, skip here */
		if (!CFP_RCB_ENAB(wlc->cfp))
#endif /* WLCFP */
		{
			WLSCB_HISTO_RX_INC_RECENT(rfc->scb, (num_sf-1));
		}
	}
#endif /* WLSCB_HISTO */

	(*index) += num_sf;

	return BCME_OK;

toss:
	RX_PKTDROP_COUNT(wlc, scb, toss_reason);
	/*
	 * toss the whole A-MSDU since we don't know where the error starts
	 *  e.g. a wrong subframe length for mid frame can slip through the ucode
	 *       and the only syptom may be the last MSDU frame has the mismatched length.
	 */
	if (PKTNEXT(osh, head)) {
		PKTFREE(osh, PKTNEXT(osh, head), FALSE);
		PKTSETNEXT(osh, head, NULL);
	}

	if (head != tail) {
		while ((tmp_next = PKTCLINK(head)) != NULL) {
			PKTSETCLINK(head, PKTCLINK(tmp_next));
			PKTSETCLINK(tmp_next, NULL);
			PKTCLRCHAINED(wlc->osh, tmp_next);
			PKTFREE(osh, tmp_next, FALSE);
			if (tmp_next == tail) {
				/* assign *pp to head so that wlc_sendup_chain
				 * does not try to free tmp_next again
				 */
				*pp = head;
				break;
			}
		}
	}

	WL_AMSDU(("%s: tossing amsdu in deagg -- error seen\n", __FUNCTION__));
	return BCME_ERROR;
} /* wlc_amsdu_pktc_deagg_hw */
#endif /* PKTC */

#ifdef WLAMSDU_SWDEAGG
/**
 * A-MSDU sw deaggregation - for testing only due to lower performance to align payload.
 *
 *    | 80211 MAC HEADER | subFrame 1 | subFrame 2 | subFrame 3 | ... |
 * where, 80211 MAC header also includes WDS and/or QOS and/or IV fields
 *        f->pbody points to beginning of subFrame 1,
 *        f->body_len is the total length of all sub frames, exclude ICV and/or FCS
 *
 *        each subframe is in the form of | 8023hdr | body | pad |
 *                subframe other than the last one may have pad bytes
*/
/*
 * Note: This headroom calculation comes out to 10 byte.
 * Arithmetically, this amounts to two 4-byte blocks plus
 * 2. 2 bytes are needed anyway to achieve 4-byte alignment.
 */
#define HEADROOM  DOT11_A3_HDR_LEN-ETHER_HDR_LEN
void
wlc_amsdu_deagg_sw(amsdu_info_t *ami, struct scb *scb, struct wlc_frminfo *f)
{
	wlc_info_t *wlc = ami->wlc;
	osl_t *osh;
	struct ether_header *eh;
	struct ether_addr *ea;
	uchar *data;
	void *newpkt;
	int resid;
	uint16 body_offset, sflen, len;
	void *orig_p;

	BCM_REFERENCE(wlc);

	osh = ami->pub->osh;

	/* all in one buffer, no chain */
	ASSERT(PKTNEXT(osh, f->p) == NULL);

	/* throw away mac header all together, start from A-MSDU body */
	body_offset = (uint)(f->pbody - (uchar*)PKTDATA(osh, f->p));
	PKTPULL(osh, f->p, body_offset);
	ASSERT(f->pbody == (uchar *)PKTDATA(osh, f->p));
	data = f->pbody;
	resid = f->totlen;
	orig_p = f->p;

	WL_AMSDU(("wlc_amsdu_deagg_sw: body_len(exclude ICV and FCS) %d\n", resid));

	/* loop over orig unpacking and copying frames out into new packet buffers */
	while (resid > 0) {
		if (resid < ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN)
			break;

		/* each subframe is 802.3 frame */
		eh = (struct ether_header*) data;
		sflen = ntoh16(eh->ether_type) + ETHER_HDR_LEN;

		/* swdeagg is mainly for testing, not intended to support big buffer.
		 *  there are also the 2K hard limit for rx buffer we posted.
		 *  We can increase to 4K, but it wastes memory and A-MSDU often goes
		 *  up to 8K. HW deagg is the preferred way to handle large A-MSDU.
		 */
		if (sflen > ETHER_MAX_DATA + DOT11_LLC_SNAP_HDR_LEN + ETHER_HDR_LEN) {
			WL_AMSDU_ERROR(("%s: unexpected long pkt, toss!", __FUNCTION__));
			WLCNTINCR(ami->cnt->deagg_swdeagglong);
			RX_PKTDROP_COUNT(wlc, scb, RX_PKTDROP_RSN_LONG_PKT);
			goto done;
		}

		/*
		 * Alloc new rx packet buffer, add headroom bytes to
		 * achieve 4-byte alignment and to allow for changing
		 * the hdr from 802.3 to 802.11 (EXT STA only)
		 */
		if ((newpkt = PKTGET(osh, sflen + HEADROOM, FALSE)) == NULL) {
			WL_ERROR(("wl: %s: pktget error\n", __FUNCTION__));
			WLCNTINCR(ami->pub->_cnt->rxnobuf);
			RX_PKTDROP_COUNT(wlc, scb, RX_PKTDROP_RSN_NO_BUF);
			goto done;
		}
		PKTPULL(osh, newpkt, HEADROOM);
		/* copy next frame into new rx packet buffer, pad bytes are dropped */
		bcopy(data, PKTDATA(osh, newpkt), sflen);
		PKTSETLEN(osh, newpkt, sflen);

		/* convert 8023hdr to ethernet if necessary */
		wlc_8023_etherhdr(ami->wlc, osh, newpkt);

		ea = (struct ether_addr *) PKTDATA(osh, newpkt);

		/* transfer prio, NO need to transfer other tags, it's plain stack packet now */
		PKTSETPRIO(newpkt, f->prio);

		f->da = ea;
		f->p = newpkt;
		wlc_recvdata_sendup_msdus(ami->wlc, scb, f);

		/* account padding bytes */
		len = ROUNDUP(sflen, 4);

		WL_AMSDU(("wlc_amsdu_deagg_sw: deagg one frame datalen=%d, buflen %d\n",
			sflen, len));

		data += len;
		resid -= len;

		/* last MSDU doesn't have pad, may overcount */
		if (resid < -4) {
			WL_AMSDU_ERROR(("wl: %s: error: resid %d\n", __FUNCTION__, resid));
			break;
		}
	}

done:
	/* all data are copied, free the original amsdu frame */
	PKTFREE(osh, orig_p, FALSE);
} /* wlc_amsdu_deagg_sw */
#endif /* WLAMSDU_SWDEAGG */

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)

static int
wlc_amsdu_dump(void *ctx, struct bcmstrbuf *b)
{
	amsdu_info_t *ami = ctx;
	uint i, last;
	uint32 total = 0;

	bcm_bprintf(b, "amsdu_agg_block %d amsdu_rx_mtu %d rcvfifo_limit %d amsdu_rxcap_big %d\n",
		ami->amsdu_agg_block, ami->amsdu_rx_mtu,
		ami->mac_rcvfifo_limit, ami->amsdu_rxcap_big);

	for (i = 0; i < RX_FIFO_NUMBER; i++) {
		amsdu_deagg_t *deagg = &ami->amsdu_deagg[i];
		bcm_bprintf(b, "%d amsdu_deagg_state %d\n", i, deagg->amsdu_deagg_state);
	}

	for (i = 0; i < NUMPRIO; i++) {
		bcm_bprintf(b, "%d agg_allowprio %d agg_bytes_limit %d agg_sf_limit %d",
			i, ami->txpolicy.amsdu_agg_enable[i],
			ami->txpolicy.amsdu_max_agg_bytes[i],
			ami->txpolicy.amsdu_max_sframes);
		bcm_bprintf(b, " fifo_lowm %d fifo_hiwm %d",
			ami->txpolicy.fifo_lowm, ami->txpolicy.fifo_hiwm);
		bcm_bprintf(b, "\n");
	}

#ifdef WLCNT
	wlc_amsdu_dump_cnt(ami, b);
#endif // endif

	for (i = 0, last = 0, total = 0; i < MAX_TX_SUBFRAMES_LIMIT; i++) {
		total += ami->amdbg->tx_msdu_histogram[i];
		if (ami->amdbg->tx_msdu_histogram[i])
			last = i;
	}
	bcm_bprintf(b, "TxMSDUdens:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %6u(%2d%%)", ami->amdbg->tx_msdu_histogram[i],
			(total == 0) ? 0 :
			(ami->amdbg->tx_msdu_histogram[i] * 100 / total));
		if (((i+1) % 8) == 0 && i != last)
			bcm_bprintf(b, "\n        :");
	}
	bcm_bprintf(b, "\n\n");

	for (i = 0, last = 0, total = 0; i < AMSDU_LENGTH_BINS; i++) {
		total += ami->amdbg->tx_length_histogram[i];
		if (ami->amdbg->tx_length_histogram[i])
			last = i;
	}
	bcm_bprintf(b, "TxAMSDU Len:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %2u-%uk%s %6u(%2d%%)", i, i+1, (i < 9)?" ":"",
		            ami->amdbg->tx_length_histogram[i],
		            (total == 0) ? 0 :
		            (ami->amdbg->tx_length_histogram[i] * 100 / total));
		if (((i+1) % 4) == 0 && i != last)
			bcm_bprintf(b, "\n         :");
	}
	bcm_bprintf(b, "\n");

	for (i = 0, last = 0, total = 0; i < AMSDU_RX_SUBFRAMES_BINS; i++) {
		total += ami->amdbg->rx_msdu_histogram[i];
		if (ami->amdbg->rx_msdu_histogram[i])
			last = i;
	}
	bcm_bprintf(b, "RxMSDUdens:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %6u(%2d%%)", ami->amdbg->rx_msdu_histogram[i],
			(total == 0) ? 0 :
			(ami->amdbg->rx_msdu_histogram[i] * 100 / total));
		if (((i+1) % 8) == 0 && i != last)
			bcm_bprintf(b, "\n        :");
	}
	bcm_bprintf(b, "\n\n");

	for (i = 0, last = 0, total = 0; i < AMSDU_LENGTH_BINS; i++) {
		total += ami->amdbg->rx_length_histogram[i];
		if (ami->amdbg->rx_length_histogram[i])
			last = i;
	}
	bcm_bprintf(b, "RxAMSDU Len:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %2u-%uk%s %6u(%2d%%)", i, i+1, (i < 9)?" ":"",
		            ami->amdbg->rx_length_histogram[i],
		            (total == 0) ? 0 :
		            (ami->amdbg->rx_length_histogram[i] * 100 / total));
		if (((i+1) % 4) == 0 && i != last)
			bcm_bprintf(b, "\n         :");
	}
	bcm_bprintf(b, "\n");

	return 0;
} /* wlc_amsdu_dump */

#ifdef WLCNT
void
wlc_amsdu_dump_cnt(amsdu_info_t *ami, struct bcmstrbuf *b)
{
	wlc_amsdu_cnt_t *cnt = ami->cnt;

	bcm_bprintf(b, "agg_openfail %u\n", cnt->agg_openfail);
	bcm_bprintf(b, "agg_passthrough %u\n", cnt->agg_passthrough);
	bcm_bprintf(b, "agg_block %u\n", cnt->agg_openfail);
	bcm_bprintf(b, "agg_amsdu %u\n", cnt->agg_amsdu);
	bcm_bprintf(b, "agg_msdu %u\n", cnt->agg_msdu);
	bcm_bprintf(b, "agg_stop_tailroom %u\n", cnt->agg_stop_tailroom);
	bcm_bprintf(b, "agg_stop_sf %u\n", cnt->agg_stop_sf);
	bcm_bprintf(b, "agg_stop_len %u\n", cnt->agg_stop_len);
	bcm_bprintf(b, "agg_stop_passthrough %u\n", cnt->agg_stop_passthrough);
	bcm_bprintf(b, "agg_stop_lowwm %u\n", cnt->agg_stop_lowwm);
	bcm_bprintf(b, "agg_stop_tcpack %u\n", cnt->agg_stop_tcpack);
	bcm_bprintf(b, "agg_stop_suppr %u\n", cnt->agg_stop_suppr);
	bcm_bprintf(b, "deagg_msdu %u\n", cnt->deagg_msdu);
	bcm_bprintf(b, "deagg_amsdu %u\n", cnt->deagg_amsdu);
	bcm_bprintf(b, "deagg_badfmt %u\n", cnt->deagg_badfmt);
	bcm_bprintf(b, "deagg_wrongseq %u\n", cnt->deagg_wrongseq);
	bcm_bprintf(b, "deagg_badsflen %u\n", cnt->deagg_badsflen);
	bcm_bprintf(b, "deagg_badsfalign %u\n", cnt->deagg_badsfalign);
	bcm_bprintf(b, "deagg_badtotlen %u\n", cnt->deagg_badtotlen);
	bcm_bprintf(b, "deagg_openfail %u\n", cnt->deagg_openfail);
	bcm_bprintf(b, "deagg_swdeagglong %u\n", cnt->deagg_swdeagglong);
	bcm_bprintf(b, "deagg_flush %u\n", cnt->deagg_flush);
	bcm_bprintf(b, "tx_pkt_free_ignored %u\n", cnt->tx_pkt_free_ignored);
	bcm_bprintf(b, "tx_padding_in_tail %u\n", cnt->tx_padding_in_tail);
	bcm_bprintf(b, "tx_padding_in_head %u\n", cnt->tx_padding_in_head);
	bcm_bprintf(b, "tx_padding_no_pad %u\n", cnt->tx_padding_no_pad);
}
#endif	/* WLCNT */

#if defined(WLAMSDU_TX) && !defined(WLAMSDU_TX_DISABLED)
static void
wlc_amsdu_tx_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	amsdu_info_t *ami = (amsdu_info_t *)ctx;
	scb_amsdu_t *scb_amsdu = SCB_AMSDU_CUBBY(ami, scb);
	amsdu_scb_txpolicy_t *amsdupolicy;
	/* Not allocating as a static or const, only needed during scope of the dump */
	char ac_name[NUMPRIO][3] = {"BE", "BK", "--", "EE", "CL", "VI", "VO", "NC"};
	uint i;
	amsdu_txaggstate_t *aggstate;

	if (!AMSDU_TX_SUPPORT(ami->pub) || !scb_amsdu || !SCB_AMSDU(scb))
		return;

	bcm_bprintf(b, "AMSDU-MTU pref:%d\n", scb_amsdu->mtu_pref);
	for (i = 0; i < NUMPRIO; i++) {

		amsdupolicy = &scb_amsdu->scb_txpolicy[i];
		if (amsdupolicy->amsdu_agg_enable == FALSE) {
			continue;
		}

		/* add \t to be aligned with other scb stuff */
		bcm_bprintf(b, "\tAMSDU scb prio: %s(%d)\n", ac_name[i], i);

		aggstate = &scb_amsdu->aggstate[i];
		bcm_bprintf(b, "\tamsdu_agg_sframes %u amsdu_agg_bytes %u amsdu_agg_txpending %u\n",
			aggstate->amsdu_agg_sframes, aggstate->amsdu_agg_bytes,
			aggstate->amsdu_agg_txpending);
#if defined(WLCFP)
		bcm_bprintf(b, "\tamsdu_aggsf_max_cfp %d", scb_amsdu->amsdu_aggsf);
#endif // endif
		bcm_bprintf(b, " amsdu_aggsf_max %d", scb_amsdu->amsdu_max_sframes);
		bcm_bprintf(b, " amsdu_agg_bytes_max %d amsdu_agg_enable %d\n",
			amsdupolicy->amsdu_agg_bytes_max, amsdupolicy->amsdu_agg_enable);

		bcm_bprintf(b, "\n");
	}
}
#endif /* WLAMSDU_TX && !WLAMSDU_TX_DISABLED */
#endif // endif

#ifdef WLAMSDU_TX
/* Configure amsdu txmod if allowed */
void
wlc_amsdu_tx_scb_enable(wlc_info_t *wlc, struct scb *scb)
{
	/* Return if 11n not enabled */
	if (!N_ENAB(wlc->pub)) {
		return;
	}

	/* Return if amsdu not supported */
	if (!AMSDU_TX_SUPPORT(wlc->pub) || !AMSDU_TX_ENAB(wlc->pub)) {
		return;
	}

	/* set amsdu txmod functions depending on cur state */
	if (SCB_AMSDU(scb) && SCB_QOS(scb)) {
		wlc_txmod_config(wlc->txmodi, scb, TXMOD_AMSDU);
	}
}

/* Unconfigure amsdu txmod */
void
wlc_amsdu_tx_scb_disable(wlc_info_t *wlc, struct scb *scb)
{
	/* reset amsdu txmod functions */
	if (SCB_AMSDU(scb)) {
		wlc_txmod_unconfig(wlc->txmodi, scb, TXMOD_AMSDU);
	}
}

/* Return number of TX packets queued for given TID.
 */
uint8 wlc_amsdu_tx_queued_pkts(amsdu_info_t *ami,
	struct scb * scb, int tid)
{
	scb_amsdu_t *scb_ami;

	if ((ami == NULL) ||
		(scb == NULL) ||
		(tid < 0) ||
		(tid >= NUMPRIO)) {
		return 0;
	}

	scb_ami = SCB_AMSDU_CUBBY(ami, scb);

	return scb_ami->aggstate[tid].amsdu_agg_sframes;
}
#endif /* WLAMSDU_TX */
#ifdef WLCFP
/* Return a opaque pointer to amsdu scb cubby
 * This will be store in tcb block for an easy access during fast TX path
 * members of the cubby wont be touched
 */
void*
wlc_cfp_get_amsdu_cubby(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);
	ASSERT(wlc->ami);
	return (SCB_AMSDU(scb) ? SCB_AMSDU_CUBBY(wlc->ami, scb) : NULL);
}

/**
 *   CFP equivalent of wlc_recvamsdu.
 *
 *   Maintains AMSDU deagg state machine.
 *   Check if subframes are chainable.
 *   Returns NULL for intermediate subframes.
 *   Return head frame on last subframe.
 *
 *   Assumes receive status is in host byte order at this point.
 *   PKTDATA points to start of receive descriptor when called.
 *
 *   CAUTION : With CFP_REVLT80_UCODE_WAR, SCB will be NULL for corerev < 128
 */
void * BCMFASTPATH
wlc_cfp_recvamsdu(amsdu_info_t *ami, wlc_d11rxhdr_t *wrxh, void *p, bool* chainable,
	struct scb *scb)
{
	osl_t *osh;
	amsdu_deagg_t *deagg;
	uint aggtype;
	int fifo;
	wlc_info_t *wlc = ami->wlc;
	d11rxhdr_t *rxh;
	wlc_rx_pktdrop_reason_t toss_reason; /* must be set if tossing */

#if defined(BCMSPLITRX)
	uint16 filtermap;
#endif /* BCMSPLITRX */

#if defined(BCMDBG) || defined(WLSCB_HISTO)
	int msdu_cnt = -1;              /* just used for debug */
#endif  /* BCMDBG | WLSCB_HISTO */

	osh = ami->pub->osh;

	ASSERT(chainable != NULL);

	rxh = &wrxh->rxhdr;

#if defined(DONGLEBUILD)
	aggtype = (D11RXHDR_GE129_ACCESS_VAL(rxh, mrxs) & RXSS_AGGTYPE_MASK) >>
		RXSS_AGGTYPE_SHIFT;
#else /* ! DONGLEBUILD */
	aggtype = RXHDR_GET_AGG_TYPE(rxh, wlc);
#endif /* ! DONGLEBUILD */

#if defined(BCMDBG) || defined(WLSCB_HISTO)
	msdu_cnt = RXHDR_GET_MSDU_COUNT(rxh, wlc);
#endif  /* BCMDBG | WLSCB_HISTO */

	/* Retrieve per fifo deagg info */
	fifo = (D11RXHDR_GE129_ACCESS_VAL(rxh, fifo));
	ASSERT((fifo < RX_FIFO_NUMBER));
	deagg = &ami->amsdu_deagg[fifo];

	WLCNTINCR(ami->cnt->deagg_msdu);

	WL_AMSDU(("%s: aggtype %d, msdu count %d\n", __FUNCTION__, aggtype, msdu_cnt));

	/* Decode the aggtype */
	switch (aggtype) {
	case RXS_AMSDU_FIRST:
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_IDLE) {
			WL_AMSDU(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			wlc_amsdu_deagg_flush(ami, fifo);
			/* keep this valid one and reset to improve throughput */
		}

		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_FIRST;
		deagg->amsdu_deagg_p = p;
		deagg->amsdu_deagg_ptail = p;
		deagg->chainable = TRUE;

		WL_AMSDU(("%s: first A-MSDU buffer\n", __FUNCTION__));
		break;

	case RXS_AMSDU_INTERMEDIATE:
		/* PKTDATA starts with subframe header */
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_FIRST) {
			WL_AMSDU_ERROR(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SEQ;
			goto abort;
		}

#if defined(BCMSPLITRX)
		/* Do chainable checks only for non-head subframes;
		 * head frame is checked by parent fn
		 */
		/* PKTCLASS from HWA 2.a */
		/* NOTE: Only PKTCLASS_AMSDU_DA_MASK and
		 * PKTCLASS_AMSDU_SA_MASK are valid in non-first MSDU.
		 * So we don't need to check pktclass A1,A2 and FC for non-first
		 * sub frame.
		 */

		/* Filtermap from HWA 2.a */
		filtermap = ltoh16(D11RXHDR_GE129_ACCESS_VAL(rxh, filtermap16));
#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
		filtermap = hwa_rxdata_fhr_unchainable(filtermap);
#endif // endif

		deagg->chainable &= (filtermap == 0);
#else /* ! BCMSPLITRX */
		deagg->chainable &= TRUE;
#endif /* ! BCMSPLITRX */

		ASSERT(deagg->amsdu_deagg_ptail);
		PKTSETNEXT(osh, deagg->amsdu_deagg_ptail, p);
		deagg->amsdu_deagg_ptail = p;
		WL_AMSDU(("%s:   mid A-MSDU buffer\n", __FUNCTION__));
		break;
	case RXS_AMSDU_LAST:
		/* PKTDATA starts with last subframe header */
		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_FIRST) {
			WL_AMSDU_ERROR(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			toss_reason = RX_PKTDROP_RSN_BAD_DEAGG_SEQ;
			goto abort;
		}

		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_LAST;

#ifdef ASSERT
		/* last frame should have 2 byte padding if wlc->hwrxoff is aligned
		* on mod 4 address
		*/
		if ((wlc->hwrxoff % 4) == 0) {
			ASSERT(pad != 0);
		} else {
			ASSERT(pad == 0);
		}
#endif /* ASSERT */

#if defined(BCMSPLITRX)
		/* Do chainable checks only for non-head subframes;
		 * head frame is checked by parent fn
		 */

		/* PKTCLASS from HWA 2.a */
		/* NOTE: Only PKTCLASS_AMSDU_DA_MASK and
		 * PKTCLASS_AMSDU_SA_MASK are valid in non-first MSDU.
		 * So we don't need to check pktclass A1,A2 and FC for non-first
		 * sub frame.
		 */

		/* Filtermap from HWA 2.a */
		filtermap = ltoh16(D11RXHDR_GE129_ACCESS_VAL(rxh, filtermap16));
#if defined(BCMHWA) && defined(HWA_RXDATA_BUILD)
		filtermap = hwa_rxdata_fhr_unchainable(filtermap);
#endif // endif

		deagg->chainable &= (filtermap == 0);
#else /* ! BCMSPLITRX */
		deagg->chainable &= TRUE;
#endif /* ! BCMSPLITRX */

		ASSERT(deagg->amsdu_deagg_ptail);
		PKTSETNEXT(osh, deagg->amsdu_deagg_ptail, p);
		deagg->amsdu_deagg_ptail = p;
		WL_AMSDU(("%s: last A-MSDU buffer\n", __FUNCTION__));
		break;

	case RXS_AMSDU_N_ONE:
		/* this frame IS AMSDU, checked by caller */

		if (deagg->amsdu_deagg_state != WLC_AMSDU_DEAGG_IDLE) {
			WL_AMSDU(("%s: wrong A-MSDU deagg sequence, cur_state=%d\n",
				__FUNCTION__, deagg->amsdu_deagg_state));
			WLCNTINCR(ami->cnt->deagg_wrongseq);
			wlc_amsdu_deagg_flush(ami, fifo);

			/* keep this valid one and reset to improve throughput */
		}

		ASSERT((deagg->amsdu_deagg_p == NULL) && (deagg->amsdu_deagg_ptail == NULL));
		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_LAST;
		deagg->amsdu_deagg_p = p;
		deagg->amsdu_deagg_ptail = p;
		deagg->chainable = TRUE;

		break;

	default:
		/* can't be here */
		ASSERT(0);
		toss_reason = RX_PKTDROP_RSN_BAD_AGGTYPE;
		goto abort;
	}

	/* Note that pkttotlen now includes the length of the rxh for each frag */
	WL_AMSDU(("%s: add one more A-MSDU buffer  accumulated %d bytes\n", __FUNCTION__,
		pkttotlen(osh, deagg->amsdu_deagg_p)));

	if (deagg->amsdu_deagg_state == WLC_AMSDU_DEAGG_LAST) {
		void *pp = deagg->amsdu_deagg_p;
#if defined(WL11K)
		uint tot_len = pkttotlen(osh, pp);
#endif /* WL11K */

		deagg->amsdu_deagg_p = deagg->amsdu_deagg_ptail = NULL;
		deagg->amsdu_deagg_state = WLC_AMSDU_DEAGG_IDLE;

		*chainable = deagg->chainable;
		deagg->chainable = TRUE;

		/* ucode/hw deagg happened */
/* XXX This will ASSERT() in wlc_sendup_chain()
 * if PKTC or PKTC_DONGLE is not defined
 */
		WLPKTTAG(pp)->flags |= WLF_HWAMSDU;

		/* First frame has fully defined Receive Frame Header,
		 * handle it to normal MPDU process.
		 */
		WLCNTINCR(ami->pub->_cnt->rxfrag);
		WLCNTINCR(ami->cnt->deagg_amsdu);

#if defined(WL11K)
		WLCNTADD(ami->cnt->deagg_amsdu_bytes_l, tot_len);
		if (ami->cnt->deagg_amsdu_bytes_l < tot_len)
			WLCNTINCR(ami->cnt->deagg_amsdu_bytes_h);
#endif /* WL11K */

		deagg->first_pad = 0;
#ifdef WLSCB_HISTO
		/* At the end of AMSDU de-agg update rx counters
		 * for each AMSDU, counter is updated once
		 */
#if defined(CFP_REVLT80_UCODE_WAR)
		/* For corerev < 128, SCB stats are updated in wlc_cfp_rxframe */
		if (scb)
#endif /* CFP_REVLT80_UCODE_WAR */
		{
			WLSCB_HISTO_RX(scb, (WLPKTTAG(p))->rspec, msdu_cnt);
		}
#endif /* WLSCB_HISTO */
		return (pp);
	}

	/* all other cases needs no more action, just return */
	return  NULL;
abort:
	wlc_amsdu_deagg_flush(ami, fifo);
	RX_PKTDROP_COUNT(wlc, scb, toss_reason);
	PKTFREE(osh, p, FALSE);
	return  NULL;
} /* wlc_cfp_recvamsdu */

#if !defined(BCMDBG) && !defined(BCMDBG_AMSDU)
/* Wrapper to update ampsdu counter at the end of entire release loop
 * This routine doesn't update histogram. So should be called when
 * debug option is not enabled
 */
void
wlc_cfp_amsdu_tx_counter_upd(wlc_info_t *wlc, uint32 tot_nmsdu, uint32 tot_namsdu)
{
	amsdu_info_t *ami = wlc->ami;

	WLCNTADD(ami->cnt->agg_amsdu, tot_namsdu);
	WLCNTADD(ami->cnt->agg_msdu, tot_nmsdu);
}
#endif /* !BCMDBG && !BCMDBG_AMSDU */

/* amsdu counter update for CFP Tx path. most of the counters
 * do not make sense for CFP amsdu path. Updating only few
 */
void
wlc_cfp_amsdu_tx_counter_histogram_upd(wlc_info_t *wlc, uint32 nmsdu, uint32 totlen)
{
	amsdu_info_t *ami = wlc->ami;

	WLCNTINCR(ami->cnt->agg_amsdu);
	WLCNTADD(ami->cnt->agg_msdu, nmsdu);

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	/* update statistics histograms */
	ami->amdbg->tx_msdu_histogram[(nmsdu - 1)]++;
	ami->amdbg->tx_length_histogram[MIN(totlen / AMSDU_LENGTH_BIN_BYTES,
		AMSDU_LENGTH_BINS-1)]++;
#endif /* BCMDBG || BCMDBG_AMSDU */
}

#if defined(DONGLEBUILD)
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
void
wlc_cfp_amsdu_rx_histogram_upd(wlc_info_t *wlc, uint16 msdu_count, uint16 amsdu_bytes)
{
	amsdu_info_t *ami = wlc->ami;

	WLCNTINCR(ami->amdbg->rx_msdu_histogram[MIN((msdu_count - 1), AMSDU_RX_SUBFRAMES_BINS-1)]);
	WLCNTINCR(ami->amdbg->rx_length_histogram[MIN(amsdu_bytes/AMSDU_LENGTH_BIN_BYTES,
			AMSDU_LENGTH_BINS-1)]);
}
#endif /* BCMDBG || BCMDBG_AMSDU */

#else /* ! DONGLEBUILD */
/**
 *   CFP equivalent of wlc_amsdu_pktc_deagg_hw.
 *
 *   Called when the 'WLF_HWAMSDU' flag is set in the PKTTAG of a received frame.
 *   A-MSDU decomposition:
 *
 *    | 80211 MAC HEADER | subFrame 1 |
 *			               --> | subFrame 2 |
 *			                                 --> | subFrame 3... |
 *   each subframe is in the form of | 8023hdr | body | pad |
 *   subframe other than the last one may have pad bytes
 *
 *   Convert each subframe from 8023hdr to ethernet form
 */
int32 BCMFASTPATH
wlc_cfp_amsdu_deagg_hw(amsdu_info_t *ami, void *p, uint32 *index, struct scb *scb)
{
	osl_t			* osh;
	wlc_info_t		* wlc = ami->wlc;
	void			* newpkt;
	void			* prev_pkt;
	struct ether_header	* eh;
	int			rem_len;
	uint16			sflen = 0;
	uint16			len = 0;
	uint16			num_sf = 0;
#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	uint16			amsdu_bytes = 0;
#endif /* BCMDBG || BCMDBG_AMSDU */

	osh = ami->pub->osh;
	newpkt = p;
	rem_len = pkttotlen(osh, p) - DOT11_FCS_LEN;

	while (newpkt != NULL) {
		/* strip off FCS in last MSDU */
		if (PKTNEXT(osh, newpkt) == NULL) {
			PKTFRAG_TRIM_TAILBYTES(osh, newpkt, DOT11_FCS_LEN, TAIL_BYTES_TYPE_FCS);
		}

		/* there must be a limit to stop to prevent stack overflow */
		if (num_sf >= MAX_RX_SUBFRAMES) {
			WL_AMSDU_ERROR(("%s: more than %d MSDUs !\n", __FUNCTION__, num_sf));
			break;
		}

		/* Frame buffer still points to the start of the receive descriptor.
		 * For each MPDU in chain, move pointer past receive descriptor.
		 */
		if ((WLPKTTAG(newpkt)->flags & WLF_HWAMSDU) == 0) {
			wlc_d11rxhdr_t *wrxh;
			uint pad;

			/* determine whether packet has 2-byte pad */
			wrxh = (wlc_d11rxhdr_t*) PKTDATA(osh, newpkt);
			pad = RXHDR_GET_PAD_LEN(&wrxh->rxhdr, wlc);

			PKTPULL(wlc->osh, newpkt, wlc->hwrxoff + pad);
			rem_len -= (wlc->hwrxoff + pad);
		}

		/* each subframe is 802.3 frame */
		eh = (struct ether_header*) PKTDATA(osh, newpkt);
		len = (uint16)PKTLEN(osh, newpkt);

		sflen = NTOH16(eh->ether_type) + ETHER_HDR_LEN;

		if ((((uintptr)eh + (uint)ETHER_HDR_LEN) % 4) != 0) {
			WL_AMSDU_ERROR(("%s: sf body is not 4b aligned!\n", __FUNCTION__));
			WLCNTINCR(ami->cnt->deagg_badsfalign);
			goto abort;
		}

		/* last MSDU: has FCS, but no pad, other MSDU: has pad, but no FCS */
		if (len != (PKTNEXT(osh, newpkt) ? ROUNDUP(sflen, 4) : sflen)) {
			WL_AMSDU_ERROR(("%s: len mismatch buflen %d sflen %d, sf %d\n",
				__FUNCTION__, len, sflen, num_sf));
			WLCNTINCR(ami->cnt->deagg_badsflen);
			goto abort;
		}

		PKTSETLEN(osh, newpkt, sflen);

		/* convert 8023hdr to ethernet if necessary */
		wlc_8023_etherhdr(wlc, osh, newpkt);
		WL_AMSDU(("%s: deagg MSDU buffer %d, frame %d\n", __FUNCTION__, len, sflen));

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
		amsdu_bytes += PKTLEN(osh, newpkt);
#endif /* BCMDBG || BCMDBG_AMSDU */

		num_sf++;
		rem_len -= len;
		prev_pkt = newpkt;
		newpkt = PKTNEXT(osh, newpkt);
	}

	if (rem_len != 0) {
		ASSERT(0);
		WLCNTINCR(ami->cnt->deagg_badtotlen);
		goto abort;
	}

	/* toss the remaining MSDU, which we couldn't handle */
	if (newpkt != NULL) {
		WL_AMSDU_ERROR(("%s: toss MSDUs > %d !\n", __FUNCTION__, num_sf));
		PKTSETNEXT(osh, prev_pkt, NULL);
		PKTFREE(osh, newpkt, FALSE);
	}

	WL_AMSDU(("%s: this A-MSDU has %d MSDUs, done\n", __FUNCTION__, num_sf));

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	WLCNTINCR(ami->amdbg->rx_msdu_histogram[MIN((num_sf - 1), AMSDU_RX_SUBFRAMES_BINS-1)]);
	WLCNTINCR(ami->amdbg->rx_length_histogram[MIN(amsdu_bytes/AMSDU_LENGTH_BIN_BYTES,
			AMSDU_LENGTH_BINS-1)]);
#endif /* BCMDBG || BCMDBG_AMSDU */

	(*index) += num_sf;
	return BCME_OK;

abort:
	/* Packet had to be freed by Caller */
	return BCME_ERROR;
}
#endif /* ! DONGLEBUILD */

#endif /* WLCFP */

#ifdef	WLCFP
/* In CFP mode AMSDU agg is done at AMPDU release time.
 *
 * Subframes are pulled out from AMPDU prec queues.
 * If can't fit in, should make sure its enqueued back to the prec queues.
 */
uint8 BCMFASTPATH
wlc_amsdu_agg_attempt(amsdu_info_t *ami, struct scb *scb, void *p, uint8 tid)
{
	wlc_info_t *wlc;
	int32 pad, lastpad = 0;
	uint8 i = 0;
	uint32 totlen = 0;
	scb_amsdu_t *scb_ami;
	void *p1 = NULL;
	bool pad_at_head = FALSE;
	void *n;
#if defined(BCMHWA)
	HWA_PKTPGR_EXPR(void *head = p);
#endif // endif

#ifdef BCMDBG
	if (ami->amsdu_agg_block) {
		WLCNTINCR(ami->cnt->agg_passthrough);
		return 0;
	}
#endif /* BCMDBG */

#if defined(BCMHWA)
	/* PKTPGR cannot do AMSDU in fragment packet.
	 * SW WAR: In PKTPGR no AMSDU for fragments.
	 */
	HWA_PKTPGR_EXPR({
		if (PKTNEXT(wlc->osh, p)) {
			return 0;
		}
	});
#endif // endif

	wlc = ami->wlc;

#if !defined(DONGLEBUILD)
	/* In following cases, packet will be fragmented and linked with PKTNEXT
	 * - No enough headroom - TXOFF (wlc_hdr_proc())
	 * - Shared packets (wlc_hdr_proc()/ WMF)
	 *  Skip AMSDU agg for these packets.
	 */
	if (PKTNEXT(PKT_OSH_NA, p) != NULL) {
		return 0;
	}
#endif /* ! DONGLEBUILD */

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	/* Skip AMSDU agg for PKTFETCHED packet */
	if (PKTISPKTFETCHED(wlc->osh, p))
		return 0;
#endif // endif

	/* Get the next packet from ampdu precedence Queue */
	n = wlc_ampdu_pktq_pdeq(wlc, scb, tid);

	/* Return if no packets available to do AMSDU agg */
	if (n == NULL) {
		return 0;
	}

	pad = 0;
	i = 1;
	scb_ami = SCB_AMSDU_CUBBY(ami, scb);
	totlen = pkttotlen(wlc->osh, p);

#ifdef WLAMSDU_TX
	/* If rate changed, driver need to recalculate
	 * the max aggregation length for vht and he rate.
	 */
	if (scb_ami->agglimit_upd) {
		wlc_amsdu_tx_scb_agglimit_upd(ami, scb);
		scb_ami->agglimit_upd = FALSE;
	}
#endif /* WLAMSDU_TX */

	/* msdu's in packet chain 'n' are aggregated while in this loop */
	while (1) {
#if defined(BCMHWA)
		/* PKTPGR cannot do AMSDU in fragment packet.
		 * SW WAR: In PKTPGR no AMSDU for fragments.
		 */
		HWA_PKTPGR_EXPR({
			if (PKTNEXT(wlc->osh, n)) {
				break;
			}
		});
#endif /* BCMHWA */

#ifdef WLOVERTHRUSTER
		if (OVERTHRUST_ENAB(wlc->pub)) {
			/* Check if the next subframe is possibly TCP ACK */
			if (pkttotlen(wlc->osh, n) <= TCPACKSZSDU) {
				WLCNTINCR(ami->cnt->agg_stop_tcpack);
				break;
			}
		}
#endif /* WLOVERTHRUSTER */

#if !defined(DONGLEBUILD)
		if (PKTNEXT(PKT_OSH_NA, n) != NULL) {
			break;
		}
#endif /* ! DONGLEBUILD */

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		/* Skip AMSDU agg for PKTFETCHED packet */
		if (PKTISPKTFETCHED(wlc->osh, n))
			break;
#endif // endif

		if (i >= scb_ami->amsdu_max_sframes) {
			WLCNTINCR(ami->cnt->agg_stop_sf);
			break;
		} else if ((totlen + pkttotlen(wlc->osh, n))
			>= scb_ami->scb_txpolicy[tid].amsdu_agg_bytes_max) {
			WLCNTINCR(ami->cnt->agg_stop_len);
			break;
		}
#if defined(PROP_TXSTATUS) && defined(BCMPCIEDEV)
		else if (BCMPCIEDEV_ENAB() && PROP_TXSTATUS_ENAB(wlc->pub) &&
			WLFC_GET_REUSESEQ(wlfc_query_mode(wlc->wlfc))) {
			/*
			 * This comparison does the following:
			 *  - For suppressed pkts in AMSDU, the mask should be same
			 *      so re-aggregate them
			 *  - For new pkts, pkttag->seq is zero so same
			 *  - Prevents suppressed pkt from being aggregated with non-suppressed pkt
			 *  - Prevents suppressed MPDU from being aggregated with suppressed MSDU
			 */
			if ((WLPKTTAG(p)->seq & WL_SEQ_AMSDU_SUPPR_MASK) !=
				(WLPKTTAG(n)->seq & WL_SEQ_AMSDU_SUPPR_MASK)) {
				WLCNTINCR(ami->cnt->agg_stop_suppr);
				break;
			}
		}
#endif /* PROP_TXSTATUS && BCMPCIEDEV */

		/* Padding of A-MSDU sub-frame to 4 bytes */
		pad = (uint)((-(int)(pkttotlen(wlc->osh, p) - lastpad)) & 3);

		if (i == 1) {
			/* For first msdu init ether header (amsdu header) */
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
			if (PKTISTXFRAG(wlc->osh, p)) {
				/* When BCM_DHDHDR enabled the DHD host driver will prepare the
				 * dot3_mac_llc_snap_header, now we adjust ETHER_HDR_LEN bytes
				 * in host data addr to include the ether header 14B part.
				 * The ether header 14B in dongle D3_BUFFER is going to be used as
				 * amsdu header.
				 * Now, first msdu has all DHDHDR 22B in host.
				 */
				PKTSETFRAGDATA_LO(wlc->osh, p, LB_FRAG1,
					PKTFRAGDATA_LO(wlc->osh, p, LB_FRAG1) - ETHER_HDR_LEN);
				PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
					PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + ETHER_HDR_LEN);
				PKTSETFRAGTOTLEN(wlc->osh, p,
					PKTFRAGTOTLEN(wlc->osh, p) + ETHER_HDR_LEN);
			}
#endif /* BCM_DHDHDR && DONGLEBUILD */
			WLPKTTAG(p)->flags |= WLF_AMSDU;
		}

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
		/* For second msdu and later */
		if (PKTISTXFRAG(wlc->osh, n)) {
			/* Before we free the second msdu D3_BUFFER we have to include the
			 * ether header 14B part in host.
			 * Now, second msdu has all DHDHDR 22B in host.
			 */
			PKTSETFRAGDATA_LO(wlc->osh, n, LB_FRAG1,
				PKTFRAGDATA_LO(wlc->osh, n, LB_FRAG1) - ETHER_HDR_LEN);
			PKTSETFRAGLEN(wlc->osh, n, LB_FRAG1,
				PKTFRAGLEN(wlc->osh, n, LB_FRAG1) + ETHER_HDR_LEN);
			PKTSETFRAGTOTLEN(wlc->osh, n,
				PKTFRAGTOTLEN(wlc->osh, n) + ETHER_HDR_LEN);

			/* Free the second msdu D3_BUFFER, we don't need it */
			PKTBUFEARLYFREE(wlc->osh, n);
		}
#endif /* BCM_DHDHDR && DONGLEBUILD */

		/* Add padding to next pkt (at headroom) if current is a lfrag */
		if (BCMLFRAG_ENAB() && PKTISTXFRAG(wlc->osh, p)) {
			/*
			 * Store the padding value. We need this to be accounted for, while
			 * calculating the padding for the subsequent packet.
			 * For example, if we need padding of 3 bytes for the first packet,
			 * we add those 3 bytes padding in the head of second packet. Now,
			 * to check if padding is required for the second packet, we should
			 * calculate the padding without considering these 3 bytes that
			 * we have already put in.
			 */
			lastpad = pad;

			if (pad) {
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
				/* pad data will be the garbage at the end of host data */
				PKTSETFRAGLEN(wlc->osh, p, LB_FRAG1,
					PKTFRAGLEN(wlc->osh, p, LB_FRAG1) + pad);
				PKTSETFRAGTOTLEN(wlc->osh, p, PKTFRAGTOTLEN(wlc->osh, p) + pad);
				totlen += pad;
				lastpad = 0;
#else
				/*
				 * Let's just mark that padding is required at the head of next
				 * packet. Actual padding needs to be done after the next sdu
				 * header has been copied from the current sdu.
				 */
				pad_at_head = TRUE;

#ifdef WLCNT
				WLCNTINCR(ami->cnt->tx_padding_in_head);
#endif /* WLCNT */
#endif /* BCM_DHDHDR && DONGLEBUILD */
			}
		} else {
			PKTSETLEN(wlc->osh, p, pkttotlen(wlc->osh, p) + pad);
			totlen += pad;
#ifdef WLCNT
			WLCNTINCR(ami->cnt->tx_padding_in_tail);
#endif /* WLCNT */
		}

		/* Consumes MSDU from head of chain 'n' and append this MSDU to chain 'p' */
		p1 = n;
		/* Get the next packet from ampdu precedence Queue */
		n = wlc_ampdu_pktq_pdeq(wlc, scb, tid);

		if (BCMLFRAG_ENAB()) {
			PKTSETNEXT(wlc->osh, pktlast(wlc->osh, p), p1);
			PKTSETNEXT(wlc->osh, pktlast(wlc->osh, p1), NULL);
		} else {
			PKTSETNEXT(wlc->osh, p, p1); /* extend 'p' with one MSDU */
			PKTSETNEXT(wlc->osh, p1, NULL);
		}
		totlen += pkttotlen(wlc->osh, p1);
		i++;

		/* End of pkt chain */
		if (n == NULL)
			break;

		if (pad_at_head == TRUE) {
			/* If a padding was required for the previous packet, apply it here */
			PKTPUSH(wlc->osh, p1, pad);
			totlen += pad;
			pad_at_head = FALSE;
		}
		p = p1;
	} /* while */

	WLCNTINCR(ami->cnt->agg_amsdu);
	WLCNTADD(ami->cnt->agg_msdu, i);

	if (pad_at_head == TRUE) {
		/* If a padding was required for the previous/last packet, apply it here */
		PKTPUSH(wlc->osh, p1, pad);
		totlen += pad;
		pad_at_head = FALSE;
	}

#ifdef WL11K
	WLCNTADD(ami->cnt->agg_amsdu_bytes_l, totlen);
	if (ami->cnt->agg_amsdu_bytes_l < totlen)
		WLCNTINCR(ami->cnt->agg_amsdu_bytes_h);
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_AMSDU)
	/* update statistics histograms */
	ami->amdbg->tx_msdu_histogram[MIN(i-1,
		MAX_TX_SUBFRAMES_LIMIT-1)]++;
	ami->amdbg->tx_length_histogram[MIN(totlen / AMSDU_LENGTH_BIN_BYTES,
		AMSDU_LENGTH_BINS-1)]++;
#endif /* BCMDBG || BCMDBG_AMSDU */

#if defined(BCMHWA)
	/* Fixup AMSDU in single TxLfrag */
	HWA_PKTPGR_EXPR(wlc_amsdu_single_txlfrag_fixup(wlc, head));
#endif /* BCMHWA */

	if (n) {
		/* Enque the left over packet back to AMPDU prec queue */
		wlc_ampdu_pktq_penq_head(wlc, scb, tid, n);
	}

	return i - 1;
} /* wlc_amsdu_agg_attempt */

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
/* With DHDHDR enabled, amsdu subframe header is fully formed at host.
 * So update host frag length and start address to account for
 * ETHER_HDR_LEN bytes length in host.
 *
 */
void
wlc_amsdu_subframe_insert(wlc_info_t* wlc, void* head)
{
	uint16 cnt = 0;
	void* pkt = head;

	while (pkt) {
		ASSERT(PKTISTXFRAG(wlc->osh, pkt));
		PKTSETFRAGDATA_LO(wlc->osh, pkt, LB_FRAG1,
			PKTFRAGDATA_LO(wlc->osh, pkt, LB_FRAG1) - ETHER_HDR_LEN);
		PKTSETFRAGLEN(wlc->osh, pkt, LB_FRAG1,
			PKTFRAGLEN(wlc->osh, pkt, LB_FRAG1) + ETHER_HDR_LEN);
		PKTSETFRAGTOTLEN(wlc->osh, pkt,
			PKTFRAGTOTLEN(wlc->osh, pkt) + ETHER_HDR_LEN);

		if (cnt != 0) {
			/* Free the D3_BUFFER for non head subframe
			 * D11 headers are required only for head subframe.
			 * Free up local storage for all other subframes.
			 */
			PKTBUFEARLYFREE(wlc->osh, pkt);
		}

		cnt++;
		pkt = PKTNEXT(wlc->osh, pkt);
	}
}
#endif /* BCM_DHDHDR && DONGLEBUILD */
#endif /* WLCFP */
