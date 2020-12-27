/*
 * 802.11ax uplink MU scheduler and statistics module.
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
 * $Id$
 */

#ifdef WL_ULMU

/* Enable to turn on driver triggered UTXD mode
 * #define ULMU_DRV
 */
#define ULMU_DRV

/* Enable to turn on infinite triggering
 * #define TAF_INF_UL_TRIGGER
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <802.11.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_dump.h>
#include <wlc_iocv_cmd.h>
#include <wlc_bsscfg.h>
#include <wlc_scb.h>
#include <wlc_ulmu.h>
#include <wlc_ht.h>
#include <wlc_he.h>
#include <wlc_scb_ratesel.h>
#include <wlc_bmac.h>
#include <wlc_hw_priv.h>
#include <wlc_ratelinkmem.h>
#include <wlc_lq.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_ampdu_rx.h>
#include <wlc_txbf.h>
#if defined(TESTBED_AP_11AX) || defined(BCMDBG)
#include <wlc_tx.h>
#endif /* TESTBED_AP_11AX || BCMDBG */
#include <wlc_txcfg.h>
#include <wlc_mutx.h>
#include <wlc_fifo.h>
#include <wlc_musched.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

/* forward declaration */
#define ULMU_TRIG_USRCNT_MAX	8
#define ULMU_TRIG_USRCNT_DEF	8
#define ULMU_SCHPOS_INVLD	(-1)
#define ULMU_TXDUR_MAX		5380
#define ULMU_MCTL_DURCFG_AVG	0
#define ULMU_MCTL_DURCFG_MAX	1
#define ULMU_MCTL_DURCFG_MIN	2
#define ULMU_RMEMIDX_FIRST	AMT_IDX_ULOFDMA_RSVD_START
#define ULMU_RMEMIDX_LAST	(ULMU_RMEMIDX_FIRST + (AMT_IDX_ULOFDMA_RSVD_SIZE - 1))
#define ULMU_USRCNT_MIN		2
#define ULMU_MAX_ULC_SZ		32 /* max number of ULC resources */

/* utxd commands for global update */
#define ULMU_UTXD_GLBUPD	((D11_UCTX_CMD_GLBUPD << D11_UCTX_CMD_TYPE_SHIFT) & \
				D11_UCTX_CMD_TYPE_MASK)

/* utxd commands for user update */
#define ULMU_UTXD_USRUPD	((D11_UCTX_CMD_USRUPD << D11_UCTX_CMD_TYPE_SHIFT) & \
				D11_UCTX_CMD_TYPE_MASK)
#define ULMU_UTXD_USRUPD_TRFC	(ULMU_UTXD_USRUPD | D11_UCTXCMD_USRUPD_TRFC_MASK)
#define ULMU_UTXD_USRUPD_RATE	(ULMU_UTXD_USRUPD | D11_UCTXCMD_USRUPD_RATE_MASK)
#define ULMU_UTXD_USRADD_ALL	(ULMU_UTXD_USRUPD | (D11_UCTXCMD_USRUPD_TRFC_MASK | \
				D11_UCTXCMD_USRUPD_RATE_MASK))
#define ULMU_UTXD_USRUPD_R2C	(ULMU_UTXD_USRUPD_TRFC | D11_UCTXCMD_USRUPD_R2C_MASK | \
				D11_UCTXCMD_USRUPD_RFSH_MASK)

/* utxd commands for user delete */
#define ULMU_UTXD_USRDEL	((D11_UCTX_CMD_USRDEL << D11_UCTX_CMD_TYPE_SHIFT) & \
				D11_UCTX_CMD_TYPE_MASK)
#define ULMU_UTXD_USRDEL_HARD	ULMU_UTXD_USRDEL
#define ULMU_UTXD_USRDEL_SOFT	(ULMU_UTXD_USRDEL | D11_UCTXCMD_USRDEL_SOFT_MASK)

#define ULMU_UTXDQ_PKTS_MAX	64

#define ULMU_NUM_MPDU_DLFT		128
#define ULMU_NUM_MPDU_THRSH		64
#define ULMU_MLEN_KBSZ			3	// mlen in KB unit
#define ULMU_BUFSZ_DFLT			(ULMU_NUM_MPDU_DLFT*ULMU_MLEN_KBSZ)	// in KB unit
#define ULMU_BUFSZ_THRSH		(ULMU_NUM_MPDU_THRSH*ULMU_MLEN_KBSZ)	// in KB unit
#define ULMU_AGGN_HEADROOM		2	// headroom for buffer size est. unit is # mpdus
#define ULMU_STOPTRIG_QNULL_THRSH	3	// # of qnull frames to stop trigger

#define ULMU_LEGACY_MODE		0
#define ULMU_UTXD_MODE			1
#define ULMU_MLEN_INIT			1460
#define ULMU_ON				1
#define ULMU_OFF			0

#define ULMU_IS_UTXD(mode)		((mode) == ULMU_UTXD_MODE)

#define ULMU_FLAGS_USCHED_SHIFT		0	// ucode ul scheculer
#define ULMU_FLAGS_USCHED_MASK		0x0001
#define ULMU_FLAGS_HBRNT_SHIFT		1	// hibernate mode
#define ULMU_FLAGS_HBRNT_MASK		0x0002
#define ULMU_FLAGS_AUTOULC_SHIFT	2	// auto ulc
#define ULMU_FLAGS_AUTOULC_MASK		0x0004
#define ULMU_FLAGS_DSCHED_SHIFT		3	// driver-based scheduler
#define ULMU_FLAGS_DSCHED_MASK		0x0008

#define ULMU_FLAGS_HBRNT_GET(f)		(((f) & ULMU_FLAGS_HBRNT_MASK) >> \
					ULMU_FLAGS_HBRNT_SHIFT)
#define ULMU_FLAGS_HBRNT_SET(f, x)	do {\
	(f) &= ~ULMU_FLAGS_HBRNT_MASK; \
	(f) |= ((x) << ULMU_FLAGS_HBRNT_SHIFT) & ULMU_FLAGS_HBRNT_MASK; \
	} while (0)
#define ULMU_FLAGS_USCHED_GET(f)	(((f) & ULMU_FLAGS_USCHED_MASK) >> \
					ULMU_FLAGS_USCHED_SHIFT)
#define ULMU_FLAGS_USCHED_SET(f, x)	do {\
	(f) &= ~ULMU_FLAGS_USCHED_MASK; \
	(f) |= ((x) << ULMU_FLAGS_USCHED_SHIFT) & ULMU_FLAGS_USCHED_MASK; \
	} while (0)
#define ULMU_FLAGS_AUTOULC_GET(f)	(((f) & ULMU_FLAGS_AUTOULC_MASK) >> \
					ULMU_FLAGS_AUTOULC_SHIFT)
#define ULMU_FLAGS_AUTOULC_SET(f, x)	do {\
	(f) &= ~ULMU_FLAGS_AUTOULC_MASK; \
	(f) |= ((x) << ULMU_FLAGS_AUTOULC_SHIFT) & ULMU_FLAGS_AUTOULC_MASK; \
	} while (0)
#define ULMU_FLAGS_DSCHED_GET(f)	(((f) & ULMU_FLAGS_DSCHED_MASK) >> \
					ULMU_FLAGS_DSCHED_SHIFT)
#define ULMU_FLAGS_DSCHED_SET(f, x)	do {\
	(f) &= ~ULMU_FLAGS_DSCHED_MASK; \
	(f) |= ((x) << ULMU_FLAGS_DSCHED_SHIFT) & ULMU_FLAGS_DSCHED_MASK; \
	} while (0)

#define OPERAND_SHIFT			4
#define ULMU_TRIG_FRMTY			16 // trigger frame types
#define ULMU_SCB_STATS_NUM		16
#define MAX_USRHIST_PERLINE		8

#define ULMU_SCB_IDLEPRD		10
#define ULMU_SCB_RX_PKTCNT_THRSH	100

#define ULMU_SCB_MINRSSI		(-110)

/* This approach avoids overflow of a 32 bits integer */
#define PERCENT(total, part) (((total) > 10000) ? \
	((part) / ((total) / 100)) : \
	(((part) * 100) / (total)))

#define ULMU_SCB_LOWM		40
#define ULMU_SCB_HIWM		80
#define ULMU_SCB_QNCNT_THRSH	50
#define ULMU_SCB_FLCNT_THRSH	30

typedef enum {
	ULMU_SCB_INIT = 0,
	ULMU_SCB_ADMT = 1,
	ULMU_SCB_EVCT = 2
} ulmuScbAdmitStates;

typedef enum {
	ULMU_ADMIT_AUTO = 0,
	ULMU_ADMIT_ALWAYS = 1,
	ULMU_ADMIT_TWT_ONLY = 2
} ulmuAdmitStates;

typedef struct ulmu_gstats {
	uint32 usrhist[ULMU_TRIG_USRCNT_MAX];	/* histogram of N_usr in trigger */
	uint32 lcnt[AMPDU_MAX_HE];		/* total lfifo cnt per mcs and nss */
	uint32 gdfcscnt[AMPDU_MAX_HE];		/* total good FCS cnt per mcs and nss */
	uint32 tx_cnt[MUSCHED_RU_TYPE_NUM];	/* total tx cnt per ru size */
	uint32 txsucc_cnt[MUSCHED_RU_TYPE_NUM];	/* succ tx cnt per ru size */
	uint8 ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
} ulmu_gstats_t;

/* Driver uplink info */
typedef struct ulmu_drv_counters {
	uint32 nutxd;				/* num posted utxd */

	/* Callbacks and completions */
	uint32 nutxd_status;		/* num trigger frame status */
	uint32 ncallback;			/* num callback fn invocation */
	uint32 threshold_callback;	/* bytelimit callback */
	uint32 multi_callback;		/* multi callback threshold */
	uint32 spurious_callback;	/* cb was not expected */
	uint32 no_callback;			/* tx status, no cb invoked */
	uint32 no_request;			/* txstatus with no pending utxd request */
	uint32 zero_bytes;			/* txstatus, zero bytes recvd */
	uint32 invalid_rate;		/* txstatus, unknown rate */
	uint32 byte_overrun;		/* txstatus, more ul bytes than requested */
	uint32 fid_mismatch;		/* Fid from trig status does not match request */

	/* Byte counts */
	uint32 nbytes_triggered;	/* utxd byte request */
	uint32 nbytes_received;		/* bytes received for current request */

	/* UTXD request termination reasons */
	uint32 stop_byte_limit;		/* request terminated, bytelimit reached */
	uint32 stop_qos_null;		/* request terminated, QoS NULL recvd */
	uint32 stop_timeout;		/* request terminated, rxtimeout */
	uint32 stop_trigcnt;		/* request terminated, trig count exceeded */
	uint32 stop_watchdog;		/* request terminated, watchdog fired */
	uint32 stop_evicted;		/* request terminated, peer evicted */
} ulmu_drv_counters_t;

typedef struct ulmu_rssi_stats {
	int16 min_rssi; /* min rssi */
	int16 max_rssi; /* max rssi */
	int32 avg_rssi; /* avg rssi */
} ulmu_rssi_stats_t;
typedef struct ulmu_stats {
	uint32 trigtp_cnt[ULMU_TRIG_FRMTY];	/* placeholder cnts for trigger types */
	uint32 qncnt;				/* total qos-null cnts */
	uint32 agglen;				/* sum mpdu len */
	uint32 nupd;				/* total txstatus upd */
	uint32 nfail;				/* counter for lcnt = 0 */
	uint32 nbadfcs;				/* counter for badfcs > 0 */
	uint32 nvldrssi;			/* counter for rssi being valid */
	uint32 txop;				/* avg TXOP */
	uint32 sum_lcnt;			/* sum of lfifo cnts */
	uint32 lcnt[AMPDU_MAX_HE];		/* total lfifo cnt per mcs and nss */
	uint32 gdfcscnt[AMPDU_MAX_HE];		/* total good FCS cnt per mcs and nss */
	ulmu_rssi_stats_t rssi_stats;
	uint32 tx_cnt[MUSCHED_RU_TYPE_NUM]; /* total tx cnt per ru size */
	uint32 txsucc_cnt[MUSCHED_RU_TYPE_NUM]; /* succ tx cnt per ru size */
	uint8 ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
	uint16 aggn;
	uint16 mlen;
#ifdef ULMU_DRV
	ulmu_drv_counters_t	drv_counters;	/* Driver interface counterblock */
#endif // endif
} ulmu_stats_t;

typedef struct ulmu_drv_config {
	uint32	drv_requested_bytes;	/* Global driver requested byte count */
	uint32	drv_watchdog;			/* Global driver watchdog threshold */
#ifdef BCMDBG
	uint32	drv_autotrigger;		/* Auto trigger flag, enables retriggering */
#endif // endif
} ulmu_drv_config_t;

/* module info */
typedef struct wlc_ulmu_info {
	wlc_info_t *wlc;
	uint16	flags;
	int	scbh;
	int16	policy;
	uint16	maxn[D11_REV128_BW_SZ];	/* maximum number of users triggered for ul ofdma tx */
	uint16	num_usrs;		/* number of users admitted to ul ofdma scheduler */
	uint16	txlmt;
	scb_t	*scb_list[ULMU_USRCNT_MAX];
	bool	is_start;
	int	frameid;
	int8	mode;
	uint8	ulc_sz;
	d11ulo_trig_txcfg_t txd;
	ulmu_gstats_t gstats;
	int8	num_scb_ulstats;
	uint8	min_ulofdma_usrs;	/* minimum number of users to start ul ofdma */
	uint16	csthr0;			/* min lsig len to not clear CS for basic/brp trig */
	uint16	csthr1;			/* min lsig len to not clear CS for all other trig */
	struct	spktq utxdq;		/* utxd q in SW */
	uint16	dl_weight;		/* used for manual setting of ratio dl/ul */
	uint16	ul_weight;		/* used for manual setting of ratio dl/ul */
	uint16	qnull_thrsh;		/* # consecutive rx qnull to stop trigger */

	/* admit / evict params */
	int	min_rssi;
	uint32	rx_pktcnt_thrsh;   /* threshold of num of pkts per second to enable ul-ofdma */
	uint16	idle_prd;
	bool	ban_nonbrcm_160sta; /* No ulofdma admission for non-brcm 160Mhz-capable sta */
	bool	brcm_only;		/* Only admit brcm client for ulofdma */
	uint8	always_admit;		/* 0: use additional admit/evict params like
					 * min_rssi, rx_pktcnt_thrsh, wlc_scb_ampdurx_on();
					 * 1: always admit ulofdma capable station;
					 * 2: admit trig-enab twt users only
					 */
	uint16	g_ucfg;			/* global ucfg info */
	/* driver UTXD trigger global configuration */
	ulmu_drv_config_t drv_gconfig;
} wlc_ulmu_info_t;

#define ULMU_DRV_REQUEST_LISTSZ	8

/* Data structure for UTXD request accmulator */
typedef struct utxd_accmulator {
	uint32 bytes_completed;		/* Number of bytes received */
	uint32 delta_bytecount;		/* Delta bytes to next threshold */
	uint32 next_bytecount;		/* Next bytecount target */
	uint32 overrun_bytes;		/* Extra bytes delivered beyond req */
	uint32 overrun;				/* Num times byte overrun */
	uint32 npkts;				/* Num MPDU per AMPDU */
	uint32 duration;
	uint32 timeout;				/* Num times timeout occured */
} utxd_accmulator_t;

/* Structure of individual linked list elements */
struct trigger_ctx_entry {
	/* Control info */
	struct trigger_ctx_entry *next;	/* Pointer to next request */
	uint32	frameid; /* Request cookie */
	uint32 sequence_number;			/* UTXD request sequence number */
	uint32 slot_number;				/* Storage location slot */
	uint32 state;					/* Processing state of current entry */

	/* UTXD accmulator */
	utxd_accmulator_t accmulator;

	/* Copy of UTXD request */
	packet_trigger_info_t info;
};

enum {
	ULMU_REQ_INACTIVE = 0,
	ULMU_REQ_PENDING = 1,
	ULMU_REQ_ACTIVE = 2,
	ULMU_REQ_COMPLETED = 3
};

/* Circular linked list of pending UTXD requests */
typedef struct trigger_ctx_list {
	uint32 len;						/* Pending trigger requests */
	uint32 next_sequence_number;	/* Next sequence for trigger request */
	uint32 bufsz;					/* Trigger list buffer size */
	trigger_ctx_entry_t *head;		/* Circular req list, head pointer */
	trigger_ctx_entry_t *tail;		/* Circular req list, tail pointer */
	trigger_ctx_entry_t *buffer;	/* List storage poiter */
} trigger_ctx_list_t;

/* Interface per SCB accmulation metadata */
typedef struct ulmu_drv_scbinfo {
	wlc_ulmu_info_t *ulmu;				/* ULMU back pointer */
	struct scb *scb;					/* SCB back pointer */
	trigger_ctx_entry_t *current_req;	/* Current active UTXD req */
	trigger_ctx_list_t *context_list;	/* Pending UTXD request storage */
	uint32 	nutxd;						/* Number of UTXDs posted */
	uint32  qosnull_count;				/* Number of Qos NULLs seen */
	uint32	prev_nutxd_status;			/* Previous UTXD status complete count */
	uint32	nutxd_status;				/* Current UTXD complete count */
	uint32	watchdog_count;				/* Number of watchdog events */
} ulmu_drv_scbinfo;

/* Ucode trigger status report block */
typedef struct ulmu_drv_utxd_status {
	struct scb *scb;					/* SCB for the trigger status */
	ratespec_t rate;					/* Ratespec used */
	uint32 mcs;							/* MCS index */
	uint32 nss;							/* num spatial streams */
	uint32 qn_count;					/* Qos NULL count */
	uint32 ru_idx;						/* RU index */
	uint32 triggered_bytes;				/* Number of bytes received */
	uint32 frameid;						/* Frameid of requesting UTXD */
	uint32 npkts;						/* Num MPDU per AMPDU */
	uint32 duration;
	uint32 return_status;				/* Raw trigger status from frame */
} ulmu_drv_utxd_status_t;

/* scb cubby */
typedef struct scb_ulmu {
	wlc_ulmu_info_t *ulmu; /* backlink to  ulmu */
	struct scb	*scb_bl;	/**< backlink to current scb */
	int8 schpos;		/* HEMU UL scheduler pos */
	/* For UL OFDMA */
	uint16 lmemidx;	/* UL global linkmem idx */
	uint16 rmemidx;	/* ULC ratemem idx */
	uint16 ucfg;	/* per scb ucfg info */
	bool rmem_upd;	/* if need to update ul_rmem */
	d11ulmu_rmem_t *ul_rmem;
	ulmu_stats_t *scb_stats; /* pointer to ru usage stats */
	int  trigcnt;
	uint16 ucfg1;
	uint16 aggn;
	uint16 mlen;
	uint16 bufsize;		/* expect size, i.e., number of KB to trigger */

	/* admit / evict params */
	uint32	last_rx_pkts;
	uint8	idle_cnt;
	uint8	qnullonly_cnt; /* consective number of qosnull only */
	uint8	fail_cnt; /* consective number of qosnull only */
	uint8	state; /* 0: init, 1: admit, 2: evict */
	uint8	lowwm;
	bool	rx_data;

	/* scheduler driver interface uplink info */
	ulmu_drv_scbinfo *ulinfo;
	/* rxmon */
	uint32	aggn_init;
	/* bytes requested to be received */
	uint32 tgs_exprecv_bytes;
	/* bytes tgs_recv_bytes */
	uint32 tgs_recv_bytes;
	uint32 su_recv_bytes;
	uint32 acc_bufsz; /* accumulated bytes */
	uint32 mlenma;		/* sum mpdu len */
	uint32 aggnma;		/* sum mpdu len */
	uint32 aggbytes;
	uint32 tsf_timelo;
	uint32 tsf_timehi;

	/* reserved for future use */
	uint32 reserved[2];

#ifdef WLTAF
	uint32 trig_bytes; /* trigger bytes */
	uint32 trig_units; /* trigger bytes in time units */
	uint32 bytes_recvd;
#endif // endif
} scb_ulmu_t;

#define C_BFIGEN_HESER_NBIT	15

/* cubby access macros */
#define SCB_ULMU_CUBBY(ulmu, scb)	((scb_ulmu_t **)SCB_CUBBY(scb, (ulmu)->scbh))
#define SCB_ULMU(ulmu, scb)		(*SCB_ULMU_CUBBY(ulmu, scb))

/* local declarations */

/* wlc module */
static int wlc_ulmu_wlc_init(void *ctx);

static int wlc_ulmu_doiovar(void *context, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif);

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
/* for ul ofdma scheduler */
static int wlc_ulmu_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_ulmu_dump_clr(void *ctx);
static void wlc_ulmu_print_stats(scb_ulmu_t* ulmu_scb,
	scb_t *scb, bcmstrbuf_t *b);
static void wlc_ulmu_dump_gstats(ulmu_gstats_t* gstats,
	bcmstrbuf_t *b);
static void wlc_ulmu_ru_stats(ulmu_stats_t* ul_stats,
	scb_t *scb, bcmstrbuf_t *b, bool is_160);
static void wlc_ulmu_ru_gstats(ulmu_gstats_t* gstats,
	bcmstrbuf_t *b, bool is_160);
#endif // endif
static int wlc_ulmu_scb_stats_init(wlc_ulmu_info_t *ulmu, scb_t *scb);
static void wlc_ulmu_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
static void wlc_ulmu_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data);
static void wlc_ulmu_set_ulpolicy(wlc_ulmu_info_t *ulmu, int16 ulsch_policy);
static int wlc_ulmu_get_ulpolicy(wlc_ulmu_info_t *ulmu);
static bool wlc_ulmu_scb_is_ulofdma(wlc_ulmu_info_t *ulmu, scb_t* scb);

/* scheduler admit control */
static int wlc_ulmu_wlc_deinit(void *ctx);

/* ul ofdma scheduler */
static void wlc_ulmu_cfg_commit(wlc_ulmu_info_t* ulmu);
static void wlc_ulmu_csreq_commit(wlc_ulmu_info_t* ulmu);
uint16 wlc_ulmu_scb_get_rmemidx(wlc_info_t *wlc, scb_t *scb);
static int wlc_ulmu_maxn_set(wlc_ulmu_info_t *ulmu);

/* scb cubby */
static int wlc_ulmu_scb_init(void *ctx, scb_t *scb);
static void wlc_ulmu_scb_deinit(void *ctx, scb_t *scb);
static uint wlc_ulmu_scb_secsz(void *, scb_t *);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void wlc_ulmu_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b);
#endif // endif
static void wlc_ulmu_scb_sched_init(wlc_ulmu_info_t *ulmu, scb_t *scb);
static bool wlc_ulmu_scb_eligible(wlc_ulmu_info_t *ulmu, scb_t* scb);
static uint32 wlc_ulmu_prep_utxd(wlc_ulmu_info_t *ulmu, scb_t *scb, uint16 cmd);
static int wlc_ulmu_post_utxd(wlc_ulmu_info_t *ulmu);

/* ul ofdma scb cubby */
static int8 wlc_ulmu_scb_lkup(wlc_ulmu_info_t *ulmu, scb_t *scb);
static int8 wlc_ulmu_scb_onaddr_lkup(wlc_ulmu_info_t *ulmu,
	struct ether_addr *ea, scb_t **pp_scb);
static void wlc_ulmu_fill_utxd(wlc_ulmu_info_t *ulmu,
	d11ulmu_txd_t *utxd, scb_t *scb, uint16 cmd);
static void wlc_ulmu_watchdog(void *mi);
/* dump */
static void wlc_ulmu_dump_ulofdma(wlc_ulmu_info_t* ulmu, bcmstrbuf_t *b, bool verbose);

#ifdef ULMU_DRV
static void wlc_ulmu_drv_ulmu_attach(wlc_ulmu_info_t* ulmu);
static void wlc_ulmu_drv_ulmu_detach(wlc_ulmu_info_t* ulmu);
static void wlc_ulmu_drv_watchdog(wlc_ulmu_info_t *ulmu);
static void wlc_ulmu_drv_set_watchdog(wlc_ulmu_info_t *ulmu, uint32 timeout);
static uint32 wlc_ulmu_drv_get_watchdog(wlc_ulmu_info_t *ulmu);
static void wlc_ulmu_drv_set_bytecount(wlc_ulmu_info_t *ulmu, uint32 bytecount);
static uint32 wlc_ulmu_drv_get_bytecount(wlc_ulmu_info_t *ulmu);
static void wlc_ulmu_drv_watchdog(wlc_ulmu_info_t *ulmu);

#ifdef BCMDBG
static void wlc_ulmu_drv_set_autotrigger(wlc_ulmu_info_t *ulmu, uint32 iter);
static uint32 wlc_ulmu_drv_get_autotrigger(wlc_ulmu_info_t *ulmu);
/* TAF Stubs */
static int wlc_ulmu_drv_trigger_scb(wlc_ulmu_info_t* ulmu, uint16 iter);
static int wlc_ulmu_drv_trigger_callback(wlc_info_t *wlc, struct scb *scb,
	packet_trigger_info_t *ti,
	void *arg, uint32 status_code, uint32 bytes_consumed,
	uint32 avg_pktsz, uint32 duration);
#endif /* BCMDBG */

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
static void wlc_ulmu_drv_print_scb_stats(scb_ulmu_t* ulmu_scb,
	struct scb *scb, bcmstrbuf_t *b);
#endif // endif
#else
#define wlc_ulmu_drv_print_scb_stats(ulmu_scb, scb, b)
#define wlc_ulmu_drv_watchdog(ulmu)

static int wlc_ulmu_release_bytes(wlc_ulmu_info_t *ulmu, scb_t *scb, uint16 bufsize);
#endif /* ULMU_DRV */
static bool wlc_ulmu_drv_ucautotrigger(wlc_ulmu_info_t *ulmu, struct scb *scb);
static void wlc_ulmu_scb_reqbytes_eval(wlc_ulmu_info_t *ulmu, scb_t *scb);

/* iovar table */
enum {
	IOV_UMUSCHEDULER	= 0,
	IOV_LAST
};

static const bcm_iovar_t ulmu_iovars[] = {
	{"umsched", IOV_UMUSCHEDULER, IOVF_RSDB_SET, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
*/
#include <wlc_patch.h>

/* ======== iovar dispatch ======== */
static int
wlc_ulmu_cmd_get_dispatch(wlc_ulmu_info_t *ulmu, wl_musched_cmd_params_t *params,
	char *outbuf, int outlen)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	bcmstrbuf_t bstr;
	int i, bw;
	uint16 uval16;
	uint offset;
	int8 direction; /* indicates 1:dl 2:ul 3:bi */

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(direction);

	bcm_binit(&bstr, outbuf, outlen);
	wlc = ulmu->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!ulmu) {
		bcm_bprintf(&bstr, "Ul OFDMA scheduler is not initilized\n");
		return BCME_ERROR;
	}

	if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		bcm_bprintf(&bstr, "maxN ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, ulmu->maxn[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_ULOMAXN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != ulmu->maxn[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "maxclients", strlen("maxclients"))) {
		bcm_bprintf(&bstr, "max num of admitted clients: %d\n",
		wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA));
	} else if (!strncmp(params->keystr, "start", strlen("start"))) {
		bcm_bprintf(&bstr, "%d\n", ulmu->is_start);
	} else if (!strncmp(params->keystr, "ban_nonbrcm_160sta", strlen("ban_nonbrcm_160sta"))) {
		bcm_bprintf(&bstr, "ban nonbrcm 160sta: %d\n", ulmu->ban_nonbrcm_160sta);
	} else if (!strncmp(params->keystr, "brcm_only", strlen("brcm_only"))) {
		bcm_bprintf(&bstr, "brcm only: %d\n", ulmu->brcm_only);
#ifdef ULMU_DRV
	} else if (!strncmp(params->keystr, "drv_requested_kbytes",
		strlen("drv_requested_kbytes"))) {
		bcm_bprintf(&bstr, "%d\n", (wlc_ulmu_drv_get_bytecount(ulmu) >> 10));
	} else if (!strncmp(params->keystr, "drv_watchdog", strlen("drv_watchdog"))) {
		bcm_bprintf(&bstr, "%d\n", wlc_ulmu_drv_get_watchdog(ulmu));
#endif // endif
	} else if (!strncmp(params->keystr, "dl_weight", strlen("dl_weight"))) {
		if (wlc->clk) {
			ulmu->dl_weight = wlc_read_shm(wlc, M_TXTRIGWT0_VAL(wlc));
		}
		bcm_bprintf(&bstr, "%d\n", ulmu->dl_weight);
	} else if (!strncmp(params->keystr, "ul_weight", strlen("ul_weight"))) {
		if (wlc->clk) {
			ulmu->ul_weight = wlc_read_shm(wlc, M_TXTRIGWT1_VAL(wlc));
		}
		bcm_bprintf(&bstr, "%d\n", ulmu->ul_weight);
	} else {
		wlc_ulmu_dump_ulofdma(ulmu, &bstr, WL_MUSCHED_FLAGS_VERBOSE(params));
	}

	return err;
}

static void
wlc_ulmu_start(wlc_ulmu_info_t *ulmu, bool enable)
{
	wlc_info_t *wlc = ulmu->wlc;

	ASSERT(wlc != NULL);

	/* Set host flag to enable UL OFDMA */
	wlc_bmac_mhf(wlc->hw, MXHF0, MXHF0_ULOFDMA,
		enable ? MXHF0_ULOFDMA : 0, WLC_BAND_ALL);
	/* Set host flag to send trigger status to driver. It is on by default */
	wlc_bmac_mhf(wlc->hw, MHF5, MHF5_TRIGTXS,
		enable ? MHF5_TRIGTXS : 0, WLC_BAND_ALL);
}

static int
wlc_ulmu_cmd_set_dispatch(wlc_ulmu_info_t *ulmu, wl_musched_cmd_params_t *params)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	uint offset;
	int i, start, end;
	int16 val16;
	scb_t *scb = NULL;
	scb_ulmu_t* ulmu_scb;
	int8 schpos;
	bool upd = TRUE;
	d11ulo_trig_txcfg_t *txd = &ulmu->txd;
	uint16 max_ulofdma_usrs;

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(txd);

	wlc = ulmu->wlc;
	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!ulmu) {
		return BCME_ERROR;
	}

	max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	val16 = params->vals[0];

	if (!strncmp(params->keystr, "start", strlen("start"))) {
		if (!HE_ULMU_ENAB(wlc->pub)) {
			return BCME_EPERM;
		} else {
			ulmu->is_start = val16 ? TRUE : FALSE;
			wlc_ulmu_start(ulmu, ulmu->is_start);
		}
	} else if (!strncmp(params->keystr, "fb", strlen("fb"))) {
		if ((val16 < 0) || (val16 > 1)) {
			return BCME_RANGE;
		}
		wlc_ulmu_fburst_set(ulmu, val16 ? TRUE : FALSE);
	} else if (!strncmp(params->keystr, "mctl_mode", strlen("mctl_mode"))) {
		txd->macctl &= ~D11_ULOTXD_MACTL_MODE_NBIT;
		txd->macctl |= ((val16 == 0 ? 0 : 1) << D11_ULOTXD_MACTL_MODE_NBIT);
	} else if (!strncmp(params->keystr, "mctl_ptype", strlen("mctl_ptype"))) {
		txd->macctl &= ~D11_ULOTXD_MACTL_PTYPE_MASK;
		txd->macctl |= ((val16 << D11_ULOTXD_MACTL_PTYPE_SHIFT) &
			D11_ULOTXD_MACTL_PTYPE_MASK);
	} else if (!strncmp(params->keystr, "mctl", strlen("mctl"))) {
		txd->macctl = val16;
	} else if (!strncmp(params->keystr, "txcnt", strlen("txcnt"))) {
		txd->txcnt = val16;
	} else if (!strncmp(params->keystr, "interval", strlen("interval"))) {
		txd->interval = val16;
	} else if (!strncmp(params->keystr, "burst", strlen("burst"))) {
		if ((val16 > txd->maxtw)) {
			return BCME_RANGE;
		}
		txd->burst = val16;
	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		if (val16 > ULMU_TRIG_USRCNT_MAX ||
			val16 > max_ulofdma_usrs) {
			return BCME_RANGE;
		}
		val16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			ulmu->maxn[i] = MIN(val16, wlc_txcfg_ofdma_maxn_upperbound(wlc, i));
		}

		wlc_ulmu_maxn_set(ulmu);
	} else if (!strncmp(params->keystr, "maxdur", strlen("maxdur"))) {
		if (val16 > ULMU_TXDUR_MAX || val16 <= 0) {
			return BCME_RANGE;
		}
		txd->maxdur = val16;
	} else if (!strncmp(params->keystr, "maxtw", strlen("maxtw"))) {
		if ((val16 != -1) && (val16 < txd->burst)) {
			return BCME_RANGE;
		}
		txd->maxtw = val16;
	} else if (!strncmp(params->keystr, "minidle", strlen("minidle"))) {
		txd->minidle = val16;
	} else if (!strncmp(params->keystr, "txlowat0", strlen("txlowat0"))) {
		txd->txlowat0 = val16;
	} else if (!strncmp(params->keystr, "txlowat1", strlen("txlowat1"))) {
		txd->txlowat1 = val16;
	} else if (!strncmp(params->keystr, "rxlowat0", strlen("rxlowat0"))) {
		txd->rxlowat0 = val16;
	} else if (!strncmp(params->keystr, "rxlowat1", strlen("rxlowat1"))) {
		txd->rxlowat1 = val16;
	} else if (!strncmp(params->keystr, "autorate", strlen("autorate"))) {
		if (val16 != 1 && val16 != 0) {
			return BCME_RANGE;
		}
		ulmu->g_ucfg &= (val16 == 0) ? (uint16) -1 :
			~D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
			ulmu_scb->ucfg &= (val16 == 0) ? (uint16) -1 :
				~D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "mcs", strlen("mcs"))) {
		if (!(val16 >= 0 && val16 <= 11)) {
			return BCME_RANGE;
		}
		D11_ULOTXD_UCFG_SET_MCS(ulmu->g_ucfg, val16);
		ulmu->g_ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
				D11_ULOTXD_UCFG_SET_MCS(ulmu_scb->ucfg, val16);
				ulmu_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "nss", strlen("nss"))) {
		if (val16 < 1 || val16 > 4) {
			return BCME_RANGE;
		}
		D11_ULOTXD_UCFG_SET_NSS(ulmu->g_ucfg, val16 - 1);
		ulmu->g_ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
			D11_ULOTXD_UCFG_SET_NSS(ulmu_scb->ucfg, val16 - 1);
			ulmu_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
		}
	} else if (!strncmp(params->keystr, "trssi", strlen("trssi"))) {
		D11_ULOTXD_UCFG_SET_TRSSI(ulmu->g_ucfg, val16);
		ulmu->g_ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) && (schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
				/* only update the scb with given ether addr matched */
					continue;
			}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
			D11_ULOTXD_UCFG_SET_TRSSI(ulmu_scb->ucfg, val16);
			ulmu_scb->ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
		}
	} else if (!strncmp(params->keystr, "cpltf", strlen("cpltf"))) {
		if ((val16 < DOT11_HETB_2XLTF_1U6S_GI || val16 >= DOT11_HETB_RSVD_LTF_GI)) {
			return BCME_RANGE;
		}
		txd->txctl &= ~D11_ULOTXD_TXCTL_CPF_MASK;
		txd->txctl |= ((val16 << D11_ULOTXD_TXCTL_CPF_SHIFT) &
			D11_ULOTXD_TXCTL_CPF_MASK);
	} else if (!strncmp(params->keystr, "nltf", strlen("nltf"))) {
		if ((val16 <= DOT11_HETB_1XHELTF_NLTF || val16 >= DOT11_HETB_RSVD_NLTF)) {
			return BCME_RANGE;
		}
		txd->txctl &= ~D11_ULOTXD_TXCTL_NLTF_MASK;
		txd->txctl |= ((val16 << D11_ULOTXD_TXCTL_NLTF_SHIFT) &
			D11_ULOTXD_TXCTL_NLTF_MASK);
	} else if (!strncmp(params->keystr, "txlmt", strlen("txlmt"))) {
		ulmu->txlmt = val16;
	} else if (!strncmp(params->keystr, "mmlen", strlen("mmlen"))) {
		txd->mmlen = val16;
	} else if (!strncmp(params->keystr, "mlen", strlen("mlen"))) {
		txd->mlen = val16;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) &&	(schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
					/* only update the scb with given ether addr matched */
					continue;
				}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
			ulmu_scb->mlen = val16;
			if (val16) {
				ulmu_scb->ucfg1 |= D11_ULOTXD_UCFG1_FIXMLEN_MASK;
			} else {
				ulmu_scb->ucfg1 &= ~D11_ULOTXD_UCFG1_FIXMLEN_MASK;
			}
		}
	} else if (!strncmp(params->keystr, "aggn", strlen("aggn"))) {
		txd->aggnum = val16;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) &&	(schpos !=
				wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
					/* only update the scb with given ether addr matched */
					continue;
				}
			if ((scb = ulmu->scb_list[schpos]) == NULL) {
				continue;
			}
			ulmu_scb = SCB_ULMU(ulmu, scb);
			ulmu_scb->aggn = val16;
			if (val16) {
				ulmu_scb->ucfg1 |= D11_ULOTXD_UCFG1_FIXAGGN_MASK;
			} else {
				ulmu_scb->ucfg1 &= ~D11_ULOTXD_UCFG1_FIXAGGN_MASK;
			}
		}
	} else if (!strncmp(params->keystr, "ban_nonbrcm_160sta", strlen("ban_nonbrcm_160sta"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		ulmu->ban_nonbrcm_160sta = (val16 != 0) ? TRUE : FALSE;
	} else if (!strncmp(params->keystr, "brcm_only", strlen("brcm_only"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		ulmu->brcm_only = (val16 != 0) ? TRUE : FALSE;
	} else if (!strncmp(params->keystr, "minulusers", strlen("minulusers"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		if (val16 >= 0 && (val16 <= (uint8) -1)) {
			ulmu->min_ulofdma_usrs = val16;
		} else {
			return BCME_RANGE;
		}
	} else if (!strncmp(params->keystr, "csthr0", strlen("csthr0"))) {
		ulmu->csthr0 = val16;
	} else if (!strncmp(params->keystr, "csthr1", strlen("csthr1"))) {
		ulmu->csthr1 = val16;
	} else if (!strncmp(params->keystr, "mode", strlen("mode"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		ulmu->mode = val16;
		txd->macctl &= ~D11_ULOTXD_MACTL_UTXD_MASK;
		txd->macctl |= (val16 ? 1 : 0) << D11_ULOTXD_MACTL_UTXD_SHIFT;
	} else if (!strncmp(params->keystr, "qnullthrsh", strlen("qnullthrsh"))) {
		upd = FALSE;
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		if (val16 <= 0) {
			return BCME_BADARG;
		}
		ulmu->qnull_thrsh = val16;
#if defined(BCMDBG)
	} else if (!strncmp(params->keystr, "autoulc", strlen("autoulc"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		val16 = val16 ? ULMU_ON : ULMU_OFF;
		ULMU_FLAGS_AUTOULC_SET(ulmu->flags, val16);
	} else if (!strncmp(params->keystr, "usched", strlen("usched"))) {
		val16 = val16 ? ULMU_ON : ULMU_OFF;
		ULMU_FLAGS_USCHED_SET(ulmu->flags, val16);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_USCHED_MASK;
		txd->macctl1 |= (val16 << D11_ULOTXD_MACTL1_USCHED_SHIFT);
	} else if (!strncmp(params->keystr, "hibernate", strlen("hibernate"))) {
		val16 = val16 ? ULMU_ON : ULMU_OFF;
		ULMU_FLAGS_HBRNT_SET(ulmu->flags, val16);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_HBRNT_MASK;
		txd->macctl1 |= (val16 << D11_ULOTXD_MACTL1_HBRNT_SHIFT);
	} else if (!strncmp(params->keystr, "dsched", strlen("dsched"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		val16 = val16 ? ULMU_ON : ULMU_OFF;
		ULMU_FLAGS_DSCHED_SET(ulmu->flags, val16);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_DSCHED_MASK;
		txd->macctl1 |= (val16 << D11_ULOTXD_MACTL1_DSCHED_SHIFT);
	} else if (!strncmp(params->keystr, "post", strlen("post"))) {
		upd = FALSE;
		for (i = 0; i < val16; i++) {
			wlc_ulmu_prep_utxd(ulmu, NULL, ULMU_UTXD_USRADD_ALL);
		}
		err = wlc_ulmu_post_utxd(ulmu);
	} else if (!strncmp(params->keystr, "enable", strlen("enable"))) {
		upd = FALSE;
		if (WL_MUSCHED_FLAGS_MACADDR(params) && (ULMU_SCHPOS_INVLD !=
			wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb))) {
			wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRADD_ALL);
			err = wlc_ulmu_post_utxd(ulmu);
		}
	} else if (!strncmp(params->keystr, "pos", strlen("pos"))) {
		/* Debug purpose: force a UL OFDMA user in given pos of list */
		upd = FALSE;
		if (WL_MUSCHED_FLAGS_MACADDR(params) && (ULMU_SCHPOS_INVLD !=
			wlc_ulmu_scb_onaddr_lkup(ulmu, &params->ea, &scb)) && scb) {
			ulmu_scb = SCB_ULMU(ulmu, scb);
			ulmu_scb->rmemidx = ULMU_RMEMIDX_FIRST + val16;
			wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRADD_ALL);
			err = wlc_ulmu_post_utxd(ulmu);
		}
	} else if (!strncmp(params->keystr, "test", strlen("test"))) {
		upd = FALSE;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if ((scb = ulmu->scb_list[schpos]) != NULL) {
				ulmu_scb = SCB_ULMU(ulmu, scb);
				ulmu_scb->bufsize = ULMU_BUFSZ_DFLT;
				wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRADD_ALL);
			}
		}
		wlc_ulmu_post_utxd(ulmu);
	} else if (!strncmp(params->keystr, "ulc_sz", strlen("ulc_sz"))) {
		upd = FALSE;
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		if ((val16 > ULMU_MAX_ULC_SZ) || (val16 <= 0)) {
			return BCME_BADARG;
		}
		ulmu->ulc_sz = val16;
	} else if (!strncmp(params->keystr, "reclaim", strlen("reclaim"))) {
		upd = FALSE;
		wlc_ulmu_reclaim_utxd(wlc, NULL);
#endif /* defined(BCMDBG) */
#ifdef ULMU_DRV
#ifdef BCMDBG
	} else if (!strncmp(params->keystr, "drv_trig_test", strlen("drv_trig_test"))) {
		if (val16 < 0) {
			val16 = -val16;
			wlc_ulmu_drv_set_autotrigger(ulmu, val16);
		} else {
			wlc_ulmu_drv_set_autotrigger(ulmu, 0);
		}

		if (val16 > 0) {
			wlc_ulmu_drv_trigger_scb(ulmu, val16);
		}

#endif /* BCMDBG */
	} else if (!strncmp(params->keystr, "drv_requested_kbytes",
			strlen("drv_requested_kbytes"))) {
		/* XXX limitation of this ioctl interface,
		 * operates on 16bit values
		 */
		wlc_ulmu_drv_set_bytecount(ulmu, (uint32)(val16 << 10));
	} else if (!strncmp(params->keystr, "drv_watchdog",
			strlen("drv_watchdog"))) {
		wlc_ulmu_drv_set_watchdog(ulmu, (uint32)val16);
#endif /* ULMU_DRV */
	} else if (!strncmp(params->keystr, "minrssi", strlen("minrssi"))) {
		ulmu->min_rssi = val16;
	} else if (!strncmp(params->keystr, "pktthrsh", strlen("pktthrsh"))) {
		ulmu->rx_pktcnt_thrsh = val16;
	} else if (!strncmp(params->keystr, "idleprd", strlen("idleprd"))) {
		ulmu->idle_prd = val16;
	} else if (!strncmp(params->keystr, "always_admit", strlen("always_admit"))) {
		if ((uint16)val16 > 2) {
			return BCME_RANGE;
		}
		ulmu->always_admit = val16;
	} else if (!strncmp(params->keystr, "dl_weight", strlen("dl_weight"))) {
		ulmu->dl_weight = val16;
		if (wlc->clk) {
			wlc_write_shm(wlc, M_TXTRIGWT0_VAL(wlc), val16);
		} else {
			err = BCME_NOCLK;
		}
	} else if (!strncmp(params->keystr, "ul_weight", strlen("ul_weight"))) {
		ulmu->ul_weight = val16;
		if (wlc->clk) {
			wlc_write_shm(wlc, M_TXTRIGWT1_VAL(wlc), val16);
		} else {
			err = BCME_NOCLK;
		}
	} else {
		upd = FALSE;
		err = BCME_BADARG;
	}

	if (upd && wlc->pub->up) {
		wlc_ulmu_cfg_commit(ulmu);
		wlc_ulmu_csreq_commit(ulmu);
		wlc_ulmu_maxn_set(ulmu);
	}
	return err;
}

static int
wlc_ulmu_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif)
{
	int err = BCME_OK;
	int32 *ret_int_ptr;
	wlc_ulmu_info_t *ulmu = ctx;
	wlc_info_t *wlc = ulmu->wlc;
	wl_musched_cmd_params_t musched_cmd_params;
	int32 int_val = 0;
	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(ret_int_ptr);

	if (D11REV_LT(wlc->pub->corerev, 129)) return BCME_ERROR;

	bcopy(params, &musched_cmd_params, sizeof(wl_musched_cmd_params_t));
	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));
	switch (actionid) {
	case IOV_GVAL(IOV_UMUSCHEDULER):
		err = wlc_ulmu_cmd_get_dispatch(ulmu, &musched_cmd_params, arg, alen);
		break;
	case IOV_SVAL(IOV_UMUSCHEDULER):
		err = wlc_ulmu_cmd_set_dispatch(ulmu, params);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* ======== wlc module hooks ========= */

/* wlc init callback */
static int
wlc_ulmu_wlc_init(void *ctx)
{
	wlc_ulmu_info_t *ulmu = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = ulmu->wlc;

	if (!HE_ENAB(wlc->pub)) {
		return BCME_OK;
	}

	wlc_ulmu_set_ulpolicy(ulmu, ulmu->policy);

	/* start or stop UL scheduler, depending on value of is_start */
	if (HE_ULMU_ENAB(wlc->pub) && ulmu->is_start) {
		wlc_ulmu_start(ulmu, TRUE);
	} else {
		wlc_ulmu_start(ulmu, FALSE);
	}

	wlc->ulmu->txd.chanspec = wlc->home_chanspec;
	wlc_ulmu_cfg_commit(ulmu);
	wlc_ulmu_csreq_commit(ulmu);

	wlc_write_shm(wlc, M_TXTRIGWT0_VAL(wlc), ulmu->dl_weight);
	wlc_write_shm(wlc, M_TXTRIGWT1_VAL(wlc), ulmu->ul_weight);

	return err;
}

/* wlc deinit callback */
static int
wlc_ulmu_wlc_deinit(void *ctx)
{
	wlc_ulmu_info_t *ulmu = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = ulmu->wlc;

	if (ulmu != NULL) {
		void *p;
		while ((p = spktdeq(&ulmu->utxdq))) {
			PKTFREE(wlc->osh, p, TRUE);
		}
	}

	return err;
}

/**
 * ul-ofdma admission control
 *
 * Registered as ulmu module watchdog callback. called once per second.
 * Iterates through each scb and see if there is any new active user that needs to be
 * admitted as ul-ofdma or to evict any existing user
 *
 * @param ulmu		handle to ulmu_info context
 * @return		none
 */
static void
wlc_ulmu_watchdog(void *mi)
{
	wlc_ulmu_info_t *ulmu = (wlc_ulmu_info_t*) mi;
	wlc_info_t *wlc = ulmu->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	int rssi, admit_cnt = 0;
	scb_ulmu_t* ulmu_scb;
	uint32 last_rx_pkts;

	/* bypass scb admit check/change to prevent sending utxd packet during driver mute/reinit */
	if (wlc->hw->mute_ap || wlc->hw->reinit || wlc->down_override) {
		return;
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		ulmu_scb = SCB_ULMU(ulmu, scb);
		if (!ulmu_scb) {
			continue;
		}

		if (!wlc_ulmu_scb_eligible(ulmu, scb)) {
			continue;
		}

		last_rx_pkts = ulmu_scb->last_rx_pkts;
		ulmu_scb->last_rx_pkts = scb->scb_stats.rx_ucast_pkts;
		ulmu_scb->su_recv_bytes = wlc_ampdu_ulmu_reqbytes_get(wlc->ampdu_rx, scb);
		ulmu_scb->rx_data = ((uint32)scb->scb_stats.rx_ucast_pkts - (uint32)last_rx_pkts >=
			ulmu->rx_pktcnt_thrsh);
		/* unconditionally admit if always_admit == 1 or trigger-enabled twt user */
		if ((ulmu->always_admit != ULMU_ADMIT_ALWAYS) &&
			!wlc_twt_scb_is_trig_enab(wlc->twti, scb) &&
			!ulmu_scb->rx_data) {
			continue;
		}
		admit_cnt++;
	}

	if (admit_cnt < ulmu->min_ulofdma_usrs) {
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			ulmu_scb = SCB_ULMU(ulmu, scb);
			if (!ulmu_scb) {
				continue;
			}
			wlc_ulmu_admit_clients(wlc, scb, FALSE);
		}

		return;
	}

	if (ulmu->always_admit != ULMU_ADMIT_ALWAYS) {
		/* eviction */
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb) ||
				SCB_INTERNAL(scb)) {
				continue;
			}

			ulmu_scb = SCB_ULMU(ulmu, scb);
			if (!ulmu_scb) {
				continue;
			}

			if (ulmu_scb->state == ULMU_SCB_INIT) {
				continue;
			}

			if (!SCB_ULOFDMA(scb)) {
				continue;
			}
			ulmu_scb->idle_cnt++;

			if (ulmu_scb->rx_data) {
				ulmu_scb->idle_cnt = 0;
			}

			/* Do not evict trigger-enabled twt user */
			if (wlc_twt_scb_is_trig_enab(wlc->twti, scb)) {
				continue;
			}

			/* if always_admit == 2, admit TWT users only */
			if (ulmu_scb->idle_cnt >= ulmu->idle_prd ||
				ulmu->always_admit == ULMU_ADMIT_TWT_ONLY) {
				if (!wlc_ulmu_admit_clients(wlc, scb, FALSE)) {
					wlc_ulmu_oper_state_upd(wlc->ulmu, scb, ULMU_SCB_INIT);
				}
			}
		}
	}
	/* admission */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb) || SCB_INTERNAL(scb)) {
			continue;
		}

		ulmu_scb = SCB_ULMU(ulmu, scb);
		if (!ulmu_scb) {
			continue;
		}

		if (SCB_ULOFDMA(scb)) {
			continue;
		}

		if (ulmu->always_admit != ULMU_ADMIT_ALWAYS &&
			!wlc_twt_scb_is_trig_enab(wlc->twti, scb) &&
			!wlc_scb_ampdurx_on(scb) && ulmu->rx_pktcnt_thrsh) {
			wlc_ulmu_oper_state_upd(wlc->ulmu, scb, ULMU_SCB_INIT);
			continue;
		}

		rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);
		/* let's admit if not admitted yet */
		/* admit criteria */
		/* 1) a-mpdu traffic meets certain threshold
		   2) rssi > min_rssi
		   3) trigger-enabled twt user
		*/
		if ((ulmu->always_admit == ULMU_ADMIT_ALWAYS) ||
			(ulmu_scb->rx_data && rssi > ulmu->min_rssi) ||
			wlc_twt_scb_is_trig_enab(wlc->twti, scb)) {
			if (!wlc_ulmu_admit_clients(wlc, scb, TRUE)) {
				wlc_ulmu_oper_state_upd(wlc->ulmu, scb, ULMU_SCB_INIT);
			}
		}
	}

	/* Driver UTXD stall check */
	wlc_ulmu_drv_watchdog(ulmu);

	return;
}

#if defined(WL11AX) && defined(WL_PSMX)
void
wlc_ulmu_chanspec_upd(wlc_info_t *wlc)
{
	if (!wlc->ulmu || D11REV_LT(wlc->pub->corerev, 128) || !HE_ULMU_ENAB(wlc->pub)) {
		return;
	}

	if (!wlc->clk) {
		return;
	}

	if (!PSMX_ENAB(wlc->pub)) {
		return;
	}

	if (wlc->ulmu->txd.chanspec == wlc->home_chanspec) {
		return;
	}

	wlc_bmac_suspend_macx_and_wait(wlc->hw);
	wlc->ulmu->txd.chanspec = wlc->home_chanspec;
	wlc_write_shmx(wlc, MX_TRIG_TXCFG(wlc) +
		OFFSETOF(d11ulo_trig_txcfg_t, chanspec), wlc->home_chanspec);
	wlc_bmac_enable_macx(wlc->hw);
}
#endif /* defined(WL11AX) && defined(WL_PSMX) */

static void
wlc_ulmu_dump_ulofdma(wlc_ulmu_info_t *ulmu, bcmstrbuf_t *b, bool verbose)
{
	int8 schpos;
	scb_t *scb;
	d11ulo_trig_txcfg_t *txd = &ulmu->txd;
	wlc_info_t *wlc = ulmu->wlc;
	scb_ulmu_t* ulmu_scb;
	uint8 mcs, nss;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);
	int i;
	char ulmu_policy_str[][20] = {
		"DISABLE",
		"AUTO",
	};
	uint ul_policy_idx = MIN((uint) wlc_ulmu_get_ulpolicy(ulmu),
		ARRAYSIZE(ulmu_policy_str)-1);
	uint offset;
	uint16 val16 = 0;
	BCM_REFERENCE(offset);
	BCM_REFERENCE(val16);

	bcm_bprintf(b, "UL OFDMA admitted %d maxclients %d minulusers %d "
		"fb %d mode %d brcm_only %d ban_nonbrcm_160sta %d\n",
		ulmu->num_usrs, max_ulofdma_usrs, ulmu->min_ulofdma_usrs,
		(txd->mctl0 & D11AC_TXC_MBURST) ? 1 : 0, ulmu->mode,
		ulmu->brcm_only, ulmu->ban_nonbrcm_160sta);
	bcm_bprintf(b, "maxn ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bcm_bprintf(b, "bw%d: %d ", 20 << i, ulmu->maxn[i]);
		}
	bcm_bprintf(b, "trigger: %s\n", (ulmu->is_start && (ulmu->num_usrs >=
			ulmu->min_ulofdma_usrs)) ? "ON" : "OFF");
	bcm_bprintf(b, "Scheduler parameters\n"
		"  mctl 0x%04x mctl1 0x%04x txcnt %d burst %d maxtw %d\n"
		"  interval %d maxdur %d minidle %d cpltf %d nltf %d\n"
		"  txlmt 0x%x txlowat0 %d txlowat1 %d rxlowat0 %d rxlowat1 %d\n"
		"  mlen %d mmlen %d aggn %d csthr0 %d csthr1 %d\n"
		"  ulc_sz %d autoulc %d usched %d  dsched %d hibernate %d\n"
		"  minrssi %d pktthrsh %d idleprd %d qnullthrsh %d always_admit %d\n",
		txd->macctl, txd->macctl1, txd->txcnt, txd->burst, txd->maxtw,
		txd->interval, txd->maxdur, txd->minidle,
		(txd->txctl & D11_ULOTXD_TXCTL_CPF_MASK),
		((txd->txctl & D11_ULOTXD_TXCTL_NLTF_MASK) >> D11_ULOTXD_TXCTL_NLTF_SHIFT),
		ulmu->txlmt, txd->txlowat0, txd->txlowat1, txd->rxlowat0, txd->rxlowat1,
		txd->mlen, txd->mmlen, txd->aggnum, ulmu->csthr0, ulmu->csthr1,
		ulmu->ulc_sz, ULMU_FLAGS_AUTOULC_GET(ulmu->flags),
		ULMU_FLAGS_USCHED_GET(ulmu->flags), ULMU_FLAGS_DSCHED_GET(ulmu->flags),
		ULMU_FLAGS_HBRNT_GET(ulmu->flags),
		ulmu->min_rssi, ulmu->rx_pktcnt_thrsh, ulmu->idle_prd, ulmu->qnull_thrsh,
		ulmu->always_admit);

#if defined(BCMDBG)
	/* This block of info is for internal debugging purpose */
#if defined(WL_PSMX)
	if (wlc->clk) {
		offset = MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, tgintvl);
		val16 = wlc_read_shmx(wlc, offset);
	}
#endif /* defined(WL_PSMX) */
	bcm_bprintf(b, "  tgintvl %d\n", val16);
#endif /* BCMDBG */

	bcm_bprintf(b, "UL policy: %s (%d) ",
		ulmu_policy_str[ul_policy_idx], wlc_ulmu_get_ulpolicy(ulmu));
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		bcm_bprintf(b, "bw%d: %d ", 20 << i, ulmu->maxn[i]);
	}
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "num_usrs: %d ", ulmu->num_usrs);
	if (ulmu->num_usrs) {
		bcm_bprintf(b, "List of admitted clients:");
	}
	bcm_bprintf(b, "\n");
	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulmu->scb_list[schpos]) == NULL) {
			continue;
		}
		ulmu_scb = SCB_ULMU(ulmu, scb);
		bcm_bprintf(b, "  "MACF" (aid 0x%x) idx %d rmem %d lmem %d ulmu_allow %d",
			ETHER_TO_MACF(scb->ea), scb->aid,
			schpos,
			wlc_ulmu_scb_get_rmemidx(wlc, scb),
			ulmu_scb->lmemidx,
			wlc_he_get_ulmu_allow(wlc->hei, scb));
			if (ulmu_scb->ucfg & D11_ULOTXD_UCFG_FIXRT_MASK) {
				mcs = D11_ULOTXD_UCFG_GET_MCS(ulmu_scb->ucfg);
				nss = D11_ULOTXD_UCFG_GET_NSS(ulmu_scb->ucfg) + 1;
				bcm_bprintf(b, " fixed rate %dx%d\n", mcs, nss);
			} else {
				bcm_bprintf(b, "\n");
			}
	}
}

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
static void
wlc_ulmu_dump_ul_stats(wlc_ulmu_info_t *ulmu, bcmstrbuf_t *b)
{
	scb_iter_t scbiter;
	scb_t *scb;
	scb_ulmu_t* ulmu_scb;
	uint scb_bw;
	wlc_info_t *wlc = ulmu->wlc;

	/* 1. dump MU histogram and global PER */
	bcm_bprintf(b, "UL stats:\n");
	wlc_ulmu_dump_gstats(&ulmu->gstats, b);

	/* 2. dump UL RU stats */
	wlc_ulmu_ru_gstats(&ulmu->gstats, b,
		CHSPEC_IS160(ulmu->wlc->chanspec));
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		ulmu_scb = SCB_ULMU(ulmu, scb);
		if (ulmu_scb && ulmu_scb->scb_stats) {
			scb_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
			wlc_ulmu_ru_stats(ulmu_scb->scb_stats, scb, b,
				(scb_bw == BW_160MHZ));
		}
	}

	/* 3. dump per user PER info */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		ulmu_scb = SCB_ULMU(ulmu, scb);
		if (ulmu_scb && ulmu_scb->scb_stats) {
			wlc_ulmu_print_stats(ulmu_scb, scb, b);
		}
	}
}

int
wlc_ulmu_rustats_upd(wlc_ulmu_info_t *ulmu, scb_t *scb, tx_status_t *txs)
{
	scb_ulmu_t* ulmu_scb = SCB_ULMU(ulmu, scb);
	wlc_info_t *wlc;
	uint8 ruidx, upper, tx_cnt, txsucc_cnt;
	uint8 rualloc_b12, primary80;
	ru_type_t ru_type;

	wlc = ulmu->wlc;
	ruidx = TGTXS_RUIDX(txs->status.s4);
	rualloc_b12 = TGTXS_RUALLOCLSB(txs->status.s4);
	primary80 = (CHSPEC_SB(wlc->chanspec) > 4)? 1: 0;
	upper = primary80 ^ rualloc_b12;
	tx_cnt = TGTXS_LCNT(txs->status.s5);
	txsucc_cnt = TGTXS_GDFCSCNT(txs->status.s5);

	if (ruidx > HE_MAX_2x996_TONE_RU_INDX) {
		WL_ERROR(("wl%d: %s: Invalid ru type. ru idx %d txs\n"
			"  %08X %08X | %08X %08X | %08X %08X || %08X %08X\n",
			ulmu->wlc->pub->unit, __FUNCTION__, ruidx,
			txs->status.s1, txs->status.s2, txs->status.s3, txs->status.s4,
			txs->status.s5, txs->status.ack_map1, txs->status.ack_map2,
				txs->status.s8));
		ASSERT(!"Invalid ru type");
		return BCME_ERROR;
	} else {
		ru_type = wf_he_ruidx_to_ru_type(ruidx);
	}

	ulmu->gstats.ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
	if (tx_cnt > 0) {
		WLCNTADD(ulmu->gstats.tx_cnt[ru_type], tx_cnt);
		WLCNTADD(ulmu->gstats.txsucc_cnt[ru_type], txsucc_cnt);
	}

	if (ulmu_scb && ulmu_scb->scb_stats) {
		ulmu_scb->scb_stats->ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
		if (tx_cnt > 0) {
			WLCNTADD(ulmu_scb->scb_stats->tx_cnt[ru_type], tx_cnt);
			WLCNTADD(ulmu_scb->scb_stats->txsucc_cnt[ru_type], txsucc_cnt);
		}
	}

	return BCME_OK;
}

static void
wlc_ulmu_ru_gstats(ulmu_gstats_t* gstats, bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += gstats->tx_cnt[i];
	}

	if (!total) {
		return;
	}
	bcm_bprintf(b, "UL RU stats: ");
	cnt = 0;
	for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
		if (!is_160 && i > 0) {
			continue;
		}
		for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
			if ((cnt > 0) && (cnt % 16 == 0)) {
				bcm_bprintf(b, "\n");
			}
			if (getbits(gstats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
				k, 1)) {
				bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
				cnt++;
			}
		}
	}
	bcm_bprintf(b, "\n");

	wlc_print_dump_table(b, "    RX", gstats->tx_cnt, "   PER", gstats->txsucc_cnt,
		MUSCHED_RU_TYPE_NUM, MUSCHED_RU_TYPE_NUM, TABLE_RU);
}

static void
wlc_ulmu_dump_gstats(ulmu_gstats_t* gstats, bcmstrbuf_t *b)
{
	int i, last, num_perline;
	uint32 total = 0;

	for (i = 0, total = 0, last = 0; i < ULMU_TRIG_USRCNT_MAX; i++) {
		total += gstats->usrhist[i];
		if (gstats->usrhist[i]) last = i;
	}
	if (total) {
		num_perline = ULMU_TRIG_USRCNT_MAX < MAX_USRHIST_PERLINE?
			ULMU_TRIG_USRCNT_MAX: MAX_USRHIST_PERLINE;
		last = num_perline * (last/num_perline + 1) - 1;
		bcm_bprintf(b, "  HIST : ");
		for (i = 0; i <= last; i++) {
			bcm_bprintf_val_pcent(b, gstats->usrhist[i],
				(gstats->usrhist[i] * 100) / total, DEFAULT_PADDING);
			if ((i % num_perline) == (num_perline - 1) && i != last)
				bcm_bprintf(b, "\n       : ");
		}
		bcm_bprintf(b, "\n");
	}

	wlc_print_dump_table(b, " RX HE", gstats->lcnt,
		" HE PER", gstats->gdfcscnt, AMPDU_MAX_HE, MAX_HE_RATES, TABLE_MCS);
}

static void
wlc_ulmu_print_stats(scb_ulmu_t* ulmu_scb, scb_t *scb, bcmstrbuf_t *b)
{
	ulmu_stats_t *ul_stats = ulmu_scb->scb_stats;
	int i, last;
	uint32 total = 0, tmp_aggnum, tmp_agglen, tmp_txop;
	uint32 ngoodfcs, nposlcnt;
	int32 tmp;

	if (!ul_stats) {
		return;
	}

	for (i = 0, total = 0, last = 0; i < AMPDU_MAX_HE; i++) {
		total += ul_stats->lcnt[i];
		if (ul_stats->lcnt[i]) last = i;
	}

	if (!total) {
		return;
	}

	bcm_bprintf(b, ""MACF" (aid %d):\n",
		ETHER_TO_MACF(scb->ea), scb->aid);
	last = MAX_HE_RATES * (last/MAX_HE_RATES + 1) - 1;

	wlc_print_dump_table(b, " RX HE", ul_stats->lcnt,
		" HE PER", ul_stats->gdfcscnt, AMPDU_MAX_HE, MAX_HE_RATES, TABLE_MCS);

	if (ul_stats->rssi_stats.min_rssi < 0) {
		tmp = (-ul_stats->rssi_stats.min_rssi) >> OPERAND_SHIFT;
		bcm_bprintf(b, "  RSSI : min(-%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	} else {
		tmp = ul_stats->rssi_stats.min_rssi >> OPERAND_SHIFT;
		bcm_bprintf(b, "  RSSI : min(%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	}
	if (ul_stats->rssi_stats.max_rssi < 0) {
		tmp = (-ul_stats->rssi_stats.max_rssi) >> OPERAND_SHIFT;
		bcm_bprintf(b, "max(-%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	} else {
		tmp = ul_stats->rssi_stats.max_rssi >> OPERAND_SHIFT;
		bcm_bprintf(b, "max(%d.%d) ", tmp >> 2, (tmp & 0x3) * 25);
	}
	ngoodfcs = ul_stats->nupd - ul_stats->nbadfcs;
	nposlcnt = ul_stats->nupd - ul_stats->nfail;
	if (ul_stats->nvldrssi > 0) {
		tmp = ul_stats->rssi_stats.avg_rssi;
		if (tmp < 0) {
			tmp = (-tmp / (int32) ul_stats->nvldrssi) >> OPERAND_SHIFT;
			bcm_bprintf(b, "avg(-%d.%d)\n", tmp >> 2, (tmp & 0x3) * 25);
		} else {
			tmp = (tmp / (int32) ul_stats->nvldrssi) >> OPERAND_SHIFT;
			bcm_bprintf(b, "avg(%d.%d)\n", tmp >> 2, (tmp & 0x3) * 25);
		}
	} else {
		bcm_bprintf(b, "avg(%d.%d)\n", 0, 0);
	}
	bcm_bprintf(b, "  total: qosnull %d ", ul_stats->qncnt);
	bcm_bprintf(b, "lcnt %d ", ul_stats->sum_lcnt);
	bcm_bprintf(b, "nfail %d ", ul_stats->nfail);
	bcm_bprintf(b, "nupd %d\n", ul_stats->nupd);
	if (nposlcnt) {
		tmp_aggnum = ul_stats->sum_lcnt / nposlcnt;
	} else {
		tmp_aggnum = 0;
	}
	if (ngoodfcs) {
		tmp_agglen = ul_stats->agglen / ngoodfcs;
		tmp_txop = ((ul_stats->txop / ngoodfcs) / 3) >> OPERAND_SHIFT;
	} else {
		tmp_agglen = 0;
		tmp_txop = 0;
	}
	bcm_bprintf(b, "  avg  : aggnum %d agglen %d txdur %d\n",
		tmp_aggnum, tmp_agglen, tmp_txop);

	wlc_ulmu_drv_print_scb_stats(ulmu_scb, scb, b);
	bcm_bprintf(b, "  avg rxmpduperampdu %d rxmpdulen %d\n",
		ulmu_scb->aggnma >> ULMU_NF_AGGN,
		ulmu_scb->mlenma >> ULMU_NF_AGGLEN);
	bcm_bprintf(b, "  tgs bytes recvd %d exp %d\n",
		ulmu_scb->tgs_recv_bytes,
		ulmu_scb->tgs_exprecv_bytes);
}

static void
wlc_ulmu_ru_stats(ulmu_stats_t* ul_stats, scb_t *scb,
	bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += ul_stats->tx_cnt[i];
	}

	if (!total) {
		return;
	}

	bcm_bprintf(b, ""MACF" (aid %d): RU ",
		ETHER_TO_MACF(scb->ea), scb->aid);
	cnt = 0;
	for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
		if (!is_160 && i > 0) {
			continue;
		}
		for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
			if ((cnt > 0) && (cnt % 16 == 0)) {
				bcm_bprintf(b, "\n");
			}
			if (getbits(ul_stats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
				k, 1)) {
				bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
				cnt++;
			}
		}
	}
	bcm_bprintf(b, "\n");
	wlc_print_dump_table(b, "    RX", ul_stats->tx_cnt, "   PER", ul_stats->txsucc_cnt,
		MUSCHED_RU_TYPE_NUM, MUSCHED_RU_TYPE_NUM, TABLE_RU);
}

/* debug dump for ul ofdma scheduler */
static int
wlc_ulmu_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_ulmu_info_t *ulmu = ctx;

	if (HE_ULMU_ENAB(ulmu->wlc->pub)) {
		wlc_ulmu_dump_ulofdma(ulmu, b, TRUE);
		wlc_ulmu_dump_ul_stats(ulmu, b);
	}

	return BCME_OK;
}

static int
wlc_ulmu_dump_clr(void *ctx)
{
	scb_iter_t scbiter;
	scb_t *scb;
	scb_ulmu_t* ulmu_scb;
	wlc_info_t *wlc;

	wlc_ulmu_info_t *ulmu = ctx;
	BCM_REFERENCE(ulmu);
	wlc = ulmu->wlc;

	/* clear global ulmu_gstats */
	memset(&ulmu->gstats, 0, sizeof(ulmu_gstats_t));

	/* clear ulmu_stats for scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		ulmu_scb = SCB_ULMU(ulmu, scb);
		if (ulmu_scb && ulmu_scb->scb_stats) {
			memset(ulmu_scb->scb_stats, 0, sizeof(ulmu_stats_t));
			ulmu_scb->scb_stats->rssi_stats.min_rssi =
				PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
			ulmu_scb->scb_stats->rssi_stats.max_rssi =
				-PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
		}
	}
	return BCME_OK;
}
#endif // endif

#define ULSTS_FRAMEID_SHIFT		7
#define ULSTS_UTXD_PAD			4

#ifdef ULMU_DRV
#define ULSTS_STOP_BYTECOUNT	0x004
#define ULSTS_STOP_QOSNULL		0x002
#define ULSTS_STOP_TIMEOUT		0x001
#define ULSTS_STOP_TRIGCNT		0x008

/* Driver ULMU UTXD STOP reason macros */
#define ULMU_DRV_STOP_BYTES(s)		((s) & ULSTS_STOP_BYTECOUNT)
#define ULMU_DRV_STOP_QOSNULL(s)	((s) & ULSTS_STOP_QOSNULL)
#define ULMU_DRV_STOP_TIMEOUT(s)	((s) & ULSTS_STOP_TIMEOUT)
#define ULMU_DRV_STOP_TRIGCNT(s)	((s) & ULSTS_STOP_TRIGCNT)
#define ULMU_DRV_STOP(s)			((s) >= ULMU_STATUS_COMPLETE)

#define SCB_ULINFO(ulmu, scb) ((SCB_ULMU((ulmu), (scb)))->ulinfo)
#define DRV_CTX_LIST(ulinfo) ((ulinfo)->context_list)
#define DRV_GCONFIG(ulmu) (&(ulmu)->drv_gconfig)
#define DRV_CTX_SEQNO(ctx) ((ctx)->sequence_number)
#define DRV_CTX_SLOTNO(ctx) ((ctx)->slot_number)
#define DRV_CTX_LISTLEN(ulinfo) ((ulinfo)->context_list->len)
#define DRV_SCB_COUNTERS(ulmu_scb) (&(ulmu_scb)->scb_stats->drv_counters)
#define DRV_SCB_ULINFO(ulmu_scb) ((ulmu_scb)->ulinfo)
#define DRV_CTX_TI(ctx) (&(ctx)->info)
#define DRV_CTX_ACCMULATOR(ctx) (&(ctx)->accmulator)
#define DRV_CTX_FID(ctx) ((ctx)->frameid)

#define ULMU_ACC_BUFSZ_MIN	(64)
#define ULMU_ACC_BUFSZ_MAX	(16 * 1024 * 1024) /* 16MB */
#define ULMU_DRV_DEFAULT_BUFSZ	(2 * 1024 * 1024) /* 2MB */

/* Driver uplink module attach init */
static void
wlc_ulmu_drv_ulmu_attach(wlc_ulmu_info_t *ulmu)
{
	ASSERT(ulmu);

	DRV_GCONFIG(ulmu)->drv_requested_bytes = ULMU_DRV_DEFAULT_BUFSZ;
	DRV_GCONFIG(ulmu)->drv_watchdog = 0;

#ifdef BCMDBG
	DRV_GCONFIG(ulmu)->drv_autotrigger = 0;
#endif // endif
}

/* Driver uplink module attach de-init */
static void
wlc_ulmu_drv_ulmu_detach(wlc_ulmu_info_t *ulmu)
{
	ASSERT(ulmu);

	DRV_GCONFIG(ulmu)->drv_requested_bytes = 0;
	DRV_GCONFIG(ulmu)->drv_watchdog = 0;

#ifdef BCMDBG
	DRV_GCONFIG(ulmu)->drv_autotrigger = 0;
#endif // endif
}

/* Init/deinit context tracking data structures */
static int
wlc_ulmu_drv_req_list_init(wlc_info_t *wlc, ulmu_drv_scbinfo *ulinfo, uint32 nentries)
{
	trigger_ctx_list_t *list;
	trigger_ctx_entry_t *cur = NULL;
	int n;

	ASSERT(ulinfo);

	list = MALLOCZ(wlc->osh, sizeof(*list));
	if (list) {
		list->bufsz = nentries * sizeof(*list->head);
		list->buffer = MALLOCZ(wlc->osh, list->bufsz);
		if (!list->buffer) {
			MFREE(wlc->osh, list, sizeof(*list));
			return BCME_NOMEM;
		}
	} else {
		return BCME_NOMEM;
	}

	list->len = 0;
	list->head = list->buffer;
	list->tail = list->head;
	list->next_sequence_number = 1;

	/* Connect up the next pointers into a circular linked list */
	for (n = 0, cur = list->head; n < nentries; n++, cur++)
	{
		cur->next = cur + 1;
		cur->slot_number = n;
	}

	/* back up current pointer to previous value */
	cur--;
	cur->next = list->head;

	WL_MUTX(("%s: SCB:"MACF" List head=0x%p List tail=0x%p\n\n",
		__FUNCTION__, ETHER_TO_MACF(ulinfo->scb->ea),
		list->head, list->tail));
	DRV_CTX_LIST(ulinfo) = list;

	return BCME_OK;

}

static void
wlc_ulmu_drv_req_list_deinit(wlc_info_t *wlc, ulmu_drv_scbinfo *ulinfo)
{
	if (DRV_CTX_LIST(ulinfo)) {
		if (DRV_CTX_LIST(ulinfo)->buffer) {
			MFREE(wlc->osh, DRV_CTX_LIST(ulinfo)->buffer,
				DRV_CTX_LIST(ulinfo)->bufsz);
		}
		MFREE(wlc->osh, DRV_CTX_LIST(ulinfo),
			(sizeof(*DRV_CTX_LIST(ulinfo))));
	}

	DRV_CTX_LIST(ulinfo) = NULL;
}

/* Save trigger request for accumulation */
static trigger_ctx_entry_t *
wlc_ulmu_drv_save_trigger_context(ulmu_drv_scbinfo *ulinfo,
	packet_trigger_info_t *in_data) {
	trigger_ctx_list_t *list;
	uint32 olen;

	STATIC_ASSERT(sizeof(list->head->info) == sizeof(*in_data));
	ASSERT(ulinfo);
	BCM_REFERENCE(olen);

	list = DRV_CTX_LIST(ulinfo);
	ASSERT(list);

	olen = list->len;
	if (list->len > 0) {
		if (list->head->next == list->tail) {
			return NULL;
		}
		list->head = list->head->next;
	}

	memcpy(&list->head->info, in_data, sizeof(*in_data));

	list->head->sequence_number = list->next_sequence_number;
	list->next_sequence_number++;
	list->len++;

	WL_MUTX(("%s: SCB:"MACF" olen=%d nlen=%d seq=%d slot=%d\n",
		__FUNCTION__, ETHER_TO_MACF(ulinfo->scb->ea),
		olen, list->len, list->head->sequence_number, list->head->slot_number));

	return list->head;
}

/* Returns next saved trigger context */
static trigger_ctx_entry_t *
wlc_ulmu_drv_get_next_trigger(ulmu_drv_scbinfo *ulinfo)
{
	trigger_ctx_list_t *list;
	trigger_ctx_entry_t *entry;

	ASSERT(ulinfo);

	list = DRV_CTX_LIST(ulinfo);
	ASSERT(list);

	if (list->len == 0) {
		return NULL;
	}

	entry = list->tail;

	WL_MUTX(("%s: SCB:"MACF" len=%d seq=%d slot=%d\n",
		__FUNCTION__, ETHER_TO_MACF(ulinfo->scb->ea),
		list->len, entry->sequence_number, entry->slot_number));

	return entry;
}

/* Free and release current pending trigger */
static void
wlc_ulmu_drv_free_trigger(ulmu_drv_scbinfo *ulinfo)
{
	trigger_ctx_list_t *list;
	trigger_ctx_entry_t *entry;
	uint32 olen;

	BCM_REFERENCE(olen);
	ASSERT(ulinfo);

	list = DRV_CTX_LIST(ulinfo);
	ASSERT(list);

	if (list->len == 0) {
		return;
	}

	entry = list->tail;
	entry->state = ULMU_REQ_INACTIVE;
	DRV_CTX_FID(entry) = 0;

	/* Clear UTXD accmulator info */
	memset(&entry->accmulator, 0, sizeof(entry->accmulator));

	olen = list->len;
	list->len--;
	if (list->len > 0) {
		list->tail = list->tail->next;
	}

	WL_MUTX(("%s: SCB:"MACF" seq=%d len=%d->%d slot=%d->%d\n",
		__FUNCTION__, ETHER_TO_MACF(ulinfo->scb->ea),
		entry->sequence_number, olen, list->len,
		entry->slot_number, list->tail->slot_number));
}

/* De-initialize scb uplink context */
static void
wlc_ulmu_drv_scb_deinit(wlc_ulmu_info_t *ulmu, struct scb *scb)
{
	ulmu_drv_scbinfo *ulinfo;

	ASSERT(ulmu);
	ASSERT(scb);

	if (!SCB_ULMU(ulmu, scb)) {
		return;
	}

	ulinfo = SCB_ULINFO(ulmu, scb);

	if (ulinfo == NULL) {
		return;
	}

	wlc_ulmu_drv_req_list_deinit(ulmu->wlc, ulinfo);

	MFREE(ulmu->wlc->osh, ulinfo, sizeof(*ulinfo));
	WL_MUTX(("%s: SCB:"MACF" \n", __FUNCTION__, ETHER_TO_MACF(ulinfo->scb->ea)));
}

/* Initialize scb uplink context */
static int
wlc_ulmu_drv_scb_init(wlc_ulmu_info_t *ulmu, struct scb *scb)
{
	ulmu_drv_scbinfo *ulinfo;
	scb_ulmu_t *ulmu_scb;
	int ret = BCME_OK;

	if (PIO_ENAB_HW(ulmu->wlc->wlc_hw)) {
		return BCME_OK;
	}

	if (SCB_INTERNAL(scb)) {
		return BCME_OK;
	}

	ASSERT(ulmu);
	ASSERT(scb);

	ulmu_scb = SCB_ULMU(ulmu, scb);
	ulinfo = DRV_SCB_ULINFO(ulmu_scb);

	ulinfo = MALLOCZ(ulmu->wlc->osh, sizeof(*ulinfo));
	if (ulinfo == NULL) {
		WL_MUTX(("%s: SCB:"MACF" Failed to allocate %d bytes for ulinfo\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea), (int)sizeof(*ulinfo)));
		return BCME_NOMEM;
	}

	ulinfo->ulmu = ulmu;
	ulinfo->scb = scb;

	ret = wlc_ulmu_drv_req_list_init(ulmu->wlc, ulinfo, ULMU_DRV_REQUEST_LISTSZ);

	if (ret != BCME_OK) {
		MFREE(ulmu->wlc->osh, ulinfo, sizeof(*ulinfo));
		DRV_SCB_ULINFO(ulmu_scb) = NULL;
	} else {
		DRV_SCB_ULINFO(ulmu_scb) = ulinfo;
	}

	WL_MUTX(("%s: SCB:"MACF" \n", __FUNCTION__, ETHER_TO_MACF(scb->ea)));

	return ret;
}

/* Get globally configured bytecount */
static uint32
wlc_ulmu_drv_get_bytecount(wlc_ulmu_info_t *ulmu)
{
	ASSERT(ulmu);
	return DRV_GCONFIG(ulmu)->drv_requested_bytes;
}

/*
 * Set global target bytecount for the UTXD request
 * This is a stub to integrate with future RXMONITOR queue depth functionality
 */
static void
wlc_ulmu_drv_set_bytecount(wlc_ulmu_info_t *ulmu, uint32 bytecount)
{
	ASSERT(ulmu);
	DRV_GCONFIG(ulmu)->drv_requested_bytes = bytecount;
}

/* Get watchdog timeout in seconds */
static uint32
wlc_ulmu_drv_get_watchdog(wlc_ulmu_info_t *ulmu)
{
	ASSERT(ulmu);
	return DRV_GCONFIG(ulmu)->drv_watchdog;
}

/* Set watchdog timeout in seconds */
static void
wlc_ulmu_drv_set_watchdog(wlc_ulmu_info_t *ulmu, uint32 timeout)
{
	ASSERT(ulmu);
	DRV_GCONFIG(ulmu)->drv_watchdog = timeout;
}

/* Calculate next byte count target used for threshold reporting */
static uint32
wlc_ulmu_drv_next_bytecount(uint32 maxcount, uint32 curr_count, uint32 delta)
{
	uint32 newcount;

	/*
	 * (curr_count/delta) is the integral number
	 * of delta slots, add 1 to get to the next
	 * slot value that is (n+1) slots fo delta_bytes
	 */
	newcount = (1 + (curr_count/delta)) * delta;
	newcount = (newcount < maxcount) ? newcount : maxcount;
	WL_MUTX(("%s: max:%d cur:%d new:%d delta:%d\n",
		__FUNCTION__, maxcount, curr_count, newcount, delta));

	return (newcount);
}

/* Apply default config info if caller did not specified them */
static void
wlc_ulmu_drv_request_default_fixups(packet_trigger_info_t *ti)
{
	if (ti->qos_null_threshold == 0) {
		ti->qos_null_threshold = ULMU_QOSNULL_LIMIT;
	}

	if (ti->failed_request_threshold == 0) {
		ti->failed_request_threshold = ULMU_TIMEOUT_LIMIT;
	}

	if (ti->callback_reporting_threshold == 0) {
		ti->callback_reporting_threshold = ULMU_CB_THRESHOLD;
	}
}

/* Init startstate of UTXD request */
static void
wlc_ulmu_drv_ulinfo_init(ulmu_drv_scbinfo *ulinfo, trigger_ctx_entry_t *entry)
{
	packet_trigger_info_t *ti;
	utxd_accmulator_t *accum;

	ASSERT(ulinfo);
	ASSERT(entry);

	ti = DRV_CTX_TI(entry);
	ASSERT(ti);

	accum = DRV_CTX_ACCMULATOR(entry);
	ASSERT(accum);

	/* Apply default request fixups */
	wlc_ulmu_drv_request_default_fixups(ti);

	memset(accum, 0, sizeof(*accum));

	entry->state = ULMU_REQ_PENDING;
	/* Calculate next byte threshold */
	accum->delta_bytecount =
		((ti->trigger_bytes * ti->callback_reporting_threshold)/100);

	accum->next_bytecount =
		wlc_ulmu_drv_next_bytecount(
			ti->trigger_bytes,
			accum->bytes_completed,
			accum->delta_bytecount);
}

static void
wlc_ulmu_drv_del_usr(wlc_info_t *wlc, struct scb *scb, scb_ulmu_t *ulmu_scb)
{
	ulmu_drv_scbinfo *ulinfo;
	trigger_ctx_entry_t *entry;
	packet_trigger_info_t *ti;

	ASSERT(ulmu_scb);

	ulinfo = DRV_SCB_ULINFO(ulmu_scb);
	ASSERT(ulinfo);

	entry = (ulinfo->current_req) ? (ulinfo->current_req) :
		wlc_ulmu_drv_get_next_trigger(ulinfo);

	/* Flush pending UTXD requests */
	while (entry) {
		ti = DRV_CTX_TI(entry);
		if (ti && ti->callback_function)
		{
			/* If current UTXD is being processed, complete and return
			 * ULMU_STATUS_EVICTED code
			 */
			if (ulinfo->current_req) {
				uint32 avglen;
				utxd_accmulator_t *accum = DRV_CTX_ACCMULATOR(entry);

				ASSERT(accum);

				avglen = (accum->npkts) ?
					(accum->bytes_completed/accum->npkts) : 0;

				ti->callback_function(wlc, scb, ti, ti->callback_parameter,
					ULMU_STATUS_EVICTED, accum->bytes_completed,
					avglen, accum->duration);

				/*
				 * From this point on, txtrigger statues
				 * will be dropped for UTXD requests
				 * already queued to ucode
				 */
				ulinfo->current_req = NULL;
			} else {
				ti->callback_function(wlc, scb, ti, ti->callback_parameter,
					ULMU_STATUS_EVICTED, 0, 0, 0);
			}
		}
		entry->state = ULMU_REQ_INACTIVE;
		wlc_ulmu_drv_free_trigger(ulinfo);
		entry = wlc_ulmu_drv_get_next_trigger(ulinfo);
	}

	ASSERT((DRV_CTX_LISTLEN(ulinfo) == 0));

	/* Clear ulinfo counters */
	ulinfo->nutxd = 0;
	ulinfo->qosnull_count = 0;
	ulinfo->prev_nutxd_status = 0;
	ulinfo->nutxd_status = 0;
	ulinfo->watchdog_count = 0;

	/* Reset start sequence number for UTXD requests */
	DRV_CTX_LIST(ulinfo)->next_sequence_number = 1;

	WL_MUTX(("%s: SCB:"MACF"\n", __FUNCTION__, ETHER_TO_MACF(scb->ea)));
}

static void
wlc_ulmu_drv_add_usr(scb_ulmu_t* ulmu_scb)
{
	ASSERT(ulmu_scb);
	ASSERT(DRV_SCB_ULINFO(ulmu_scb));

	ASSERT(DRV_CTX_LISTLEN(DRV_SCB_ULINFO(ulmu_scb)) == 0);
}

/* Enqueue next UTXD, save context for accmulation and callback */
static int
wlc_ulmu_drv_queue_utxd(wlc_ulmu_info_t *ulmu,
	struct scb *scb, packet_trigger_info_t *ti_data)
{
	scb_ulmu_t* ulmu_scb;
	ulmu_drv_scbinfo *ulinfo;
	trigger_ctx_entry_t *entry;

	ASSERT(ulmu);
	ASSERT(scb);
	ASSERT(ti_data);

	ulmu_scb = SCB_ULMU(ulmu, scb);
	ASSERT(ulmu_scb);

	ulinfo = DRV_SCB_ULINFO(ulmu_scb);
	ASSERT(ulinfo);

	/* Save the trigger request for accmulation processing */
	entry = wlc_ulmu_drv_save_trigger_context(ulinfo, ti_data);

	if (!entry || !DRV_CTX_TI(entry)) {
		return BCME_BUSY;
	}

	wlc_ulmu_drv_ulinfo_init(ulinfo, entry);

	ulinfo->nutxd++;
	WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->nutxd);
	WLCNTADD(DRV_SCB_COUNTERS(ulmu_scb)->nbytes_triggered,
		DRV_CTX_TI(entry)->trigger_bytes);

	/* Queue the UTXD to ucode, using run to complete mode */
	ulmu_scb->bufsize = (DRV_CTX_TI(entry)->trigger_bytes >> 10);
	DRV_CTX_FID(entry) = wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRUPD_R2C);

	WL_MUTX(("%s: SCB:"MACF" fid=0x%x request_bytes=%d seqno=%d slot=%d\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea),
			DRV_CTX_FID(entry),
			DRV_CTX_TI(entry)->trigger_bytes, DRV_CTX_SEQNO(entry),
			DRV_CTX_SLOTNO(entry)));

	return wlc_ulmu_post_utxd(ulmu);
}

static int
wlc_ulmu_drv_packet_trigger_request(wlc_info_t *wlc, struct scb *scb,
	wlc_ulmu_trigger_info_t *trigger_info)
{
	wlc_ulmu_info_t *ulmu;
	scb_ulmu_t* ulmu_scb;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(trigger_info);

	ulmu = wlc->ulmu;
	ASSERT(ulmu);

	ulmu_scb = SCB_ULMU(ulmu, scb);
	ASSERT(ulmu_scb);

	if (!ULMU_IS_UTXD(ulmu->mode)) {
		WL_MUTX(("%s: SCB:"MACF" UTXD mode not enabled\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return BCME_DISABLED;
	}

	/* RXMONITOR admits the user, do not proceed unless client is already admitted */
	if (ulmu_scb->state != ULMU_SCB_ADMT) {
		WL_MUTX(("%s: SCB:"MACF" not admitted for uplink\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return BCME_NOT_ADMITTED;
	}

	/* XXX Current ucode has a max of 64MBytes for the request bytesize
	 * Call to uCode routine is in kBytes hence the right shift of 10bits
	 */
	if ((KB(trigger_info->trigger_type.packet_trigger.trigger_bytes)) >
			(uint32)(0xFFFF)) {
		return BCME_BUFTOOLONG;
	}

	/* Post utxd */
	return wlc_ulmu_drv_queue_utxd(ulmu, scb, &trigger_info->trigger_type.packet_trigger);
}

int
wlc_ulmu_drv_trigger_request(wlc_info_t *wlc, struct scb *scb,
	int trigger_type, wlc_ulmu_trigger_info_t *trigger_info)
{
	int ret = BCME_OK;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(trigger_info);

	switch (trigger_type) {
		case ULMU_PACKET_TRIGGER:
			ret = wlc_ulmu_drv_packet_trigger_request(wlc, scb, trigger_info);
			break;

		case ULMU_TWT_TRIGGER:
		default:
			ret = BCME_ERROR;
	}

	return ret;
}

static void
wlc_ulmu_drv_process_utxd_status(wlc_ulmu_info_t *ulmu, ulmu_drv_utxd_status_t *status)
{
	struct scb *scb;
	scb_ulmu_t* ulmu_scb;
	ulmu_drv_scbinfo *ulinfo;
	packet_trigger_info_t *ti;
	uint32 status_code;
	uint32 return_code = ULMU_STATUS_INPROGRESS;
	trigger_ctx_entry_t *entry;
	utxd_accmulator_t *accum;
	bool doCallback;
	bool fm = FALSE;

	BCM_REFERENCE(fm);

	ASSERT(ulmu);
	ASSERT(status);
	status_code = status->return_status;

	scb = status->scb;
	ASSERT(scb);

	ulmu_scb = SCB_ULMU(ulmu, scb);
	ASSERT(ulmu_scb);

	ulinfo = DRV_SCB_ULINFO(ulmu_scb);

	/* These counters contain the same data
	 * counters->nutxd_status is used for dump info and can be cleared
	 * ulinfo->nutxd_status is the internal copy for the watchdog and
	 * is not cleared when dump clear is called.
	 */
	WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->nutxd_status);
	ulinfo->nutxd_status++;

	if (ulinfo->current_req == NULL) {
		ulinfo->current_req = wlc_ulmu_drv_get_next_trigger(ulinfo);
		if (ulinfo->current_req) {
			 ulinfo->current_req->state = ULMU_REQ_ACTIVE;
		}
	}

	/* Spurious status. Ignore.
	 * ulmu_scb->bufsize = 0 is used to suppress
	 * the unwanted autonomous triggering in ucode
	 */
	if (ulinfo->current_req == NULL) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->no_request);
		/* No more saved uplink requests */
		WL_MUTX(("%s: SCB:"MACF" Missing trigger, rc=0x%x qn=%d "
			"fid=0x%x cp=%d pkts=%d rate=(%d/%d)\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea),
			status->return_status, status->qn_count,
			status->frameid, status->triggered_bytes,
			status->npkts, status->mcs, status->nss));
		return;
	}

	entry = ulinfo->current_req;

	ti = DRV_CTX_TI(entry);
	if (ti == NULL) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->spurious_callback);
		/* No more saved uplink requests */
		WL_MUTX(("%s: SCB:"MACF" Spurious request, missing trigger info\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return;
	}

	doCallback = (ti->callback_function != NULL);

	accum = DRV_CTX_ACCMULATOR(entry);
	ASSERT(accum);

	accum->bytes_completed += status->triggered_bytes;
	accum->npkts += status->npkts;
	accum->duration += status->duration;

	if (status->triggered_bytes == 0) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->zero_bytes);
	}

	if (status->frameid != DRV_CTX_FID(entry)) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->fid_mismatch);
		fm = TRUE;
	}

	WLCNTADD(DRV_SCB_COUNTERS(ulmu_scb)->nbytes_received,
		status->triggered_bytes);

	/* Check for the STOP conditions, callback invoked if any of the below is met */
	if (ULMU_DRV_STOP_BYTES(status_code)) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->stop_byte_limit);
		return_code = ULMU_STATUS_COMPLETE;
	} else if (ulmu_scb->state != ULMU_SCB_ADMT) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->stop_evicted);
		return_code = ULMU_STATUS_NOTADMITTED;
	} else if (accum->bytes_completed >= accum->next_bytecount) {
		/* Calculate next threshold if multi-callback */
		if (ti->multi_callback) {
			accum->next_bytecount = wlc_ulmu_drv_next_bytecount(
				ti->trigger_bytes,
				accum->bytes_completed,
				accum->delta_bytecount);
			return_code = ULMU_STATUS_INPROGRESS;
			WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->multi_callback);
		} else {
			return_code = ULMU_STATUS_THRESHOLD;
			WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->threshold_callback);
			/* Set next threshold to above requested in trigger */
			accum->next_bytecount = (uint32)(-1);
		}
	} else if (ULMU_DRV_STOP_QOSNULL(status_code)) {
	/* uCode terminates the UTXD request and
	 * indicates status 0x2
	 */
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->stop_qos_null);
		return_code = ULMU_STATUS_QOSNULL;
	} else if (ULMU_DRV_STOP_TIMEOUT(status_code)) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->stop_timeout);
		return_code = ULMU_STATUS_TIMEOUT;
	} else if (ULMU_DRV_STOP_TRIGCNT(status_code)) {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->stop_trigcnt);
		return_code = ULMU_STATUS_TRIGCNT;
	} else {
		doCallback = FALSE;
		if (accum->bytes_completed >= ti->trigger_bytes) {
			accum->overrun++;
			accum->overrun_bytes += status->triggered_bytes;
		}
	}

	if (ULMU_DRV_STOP(return_code)) {
		ulinfo->current_req->state = ULMU_REQ_COMPLETED;
	}

	if (doCallback) {
		uint32 avglen = (accum->npkts) ?
			(accum->bytes_completed/accum->npkts) : 0;

		WL_MUTX(("%s: SCB:"MACF" Invoke callback cb:0x%p\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea),
			ti->callback_function));

		ti->callback_function(ulmu->wlc, scb, ti,
			ti->callback_parameter, return_code, accum->bytes_completed,
			avglen, accum->duration);

		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->ncallback);
	} else {
		WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->no_callback);
	}

	WL_MUTX(("%s: SCB:"MACF" fid(u/t)=(0x%x/0x%x) fm:%d seq=%d slot=%d rc=%d(0x%x) rate=(%d/%d)"
			" bytes(rq=%d cp=%d  ac=%d nx=%d) qn=%d cb(%d/%d)\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea),
			DRV_CTX_FID(entry), status->frameid, fm, DRV_CTX_SEQNO(entry),
			DRV_CTX_SLOTNO(entry), return_code, status_code,
			status->mcs, status->nss, ti->trigger_bytes, status->triggered_bytes,
			accum->bytes_completed, accum->next_bytecount,
			status->qn_count, doCallback, ti->multi_callback));

	/* UTXD request termination condition, invalidate current request pointer */
	if (ULMU_DRV_STOP(return_code)) {
		if (accum->overrun) {
			WLCNTINCR(DRV_SCB_COUNTERS(ulmu_scb)->byte_overrun);
		}
		WL_MUTX(("%s: SCB:"MACF" UTXD complete. fid(u/t)=(0x%x/0x%x) "
			"Seqno=%d  Overruns=%d (%d bytes)\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea),
			DRV_CTX_FID(entry), status->frameid, DRV_CTX_SEQNO(entry),
			accum->overrun, accum->overrun_bytes));
		wlc_ulmu_drv_free_trigger(ulinfo);
		ulinfo->current_req = NULL;
	}

}

static void
wlc_ulmu_drv_watchdog(wlc_ulmu_info_t *ulmu)
{
	uint max_ulofdma_usrs;
	uint schpos;

	ASSERT(ulmu);

	if (wlc_ulmu_drv_get_watchdog(ulmu) == 0) {
		return;
	}

	max_ulofdma_usrs = wlc_txcfg_max_clients_get(ulmu->wlc->txcfg, ULOFDMA);

	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		struct scb *scb = ulmu->scb_list[schpos];

		if (scb) {
			scb_ulmu_t *ulmu_scb = SCB_ULMU(ulmu, scb);
			ulmu_drv_scbinfo *ulinfo;
			packet_trigger_info_t *ti;
			bool clear_watchdog = TRUE;

			if (!ulmu_scb) {
				continue;
			}

			ulinfo = DRV_SCB_ULINFO(ulmu_scb);

			if (ulmu_scb->state != ULMU_SCB_ADMT) {
				continue;
			}

			if (!ulinfo->current_req) {
				continue;
			}

			ti = DRV_CTX_TI(ulinfo->current_req);

			if (!ti) {
				continue;
			}

			if (ti->watchdog_timeout == 0) {
				continue;
			}

			WL_MUTX(("%s: SCB:"MACF" watchdog_count=%d. curr_utxd=%d prev_utxd=%d\n",
					__FUNCTION__, ETHER_TO_MACF(scb->ea),
					ulinfo->watchdog_count,
					ulinfo->nutxd_status, ulinfo->prev_nutxd_status));

			if (ulinfo->nutxd_status == ulinfo->prev_nutxd_status) {
				ulinfo->watchdog_count++;
				clear_watchdog = FALSE;
				WL_MUTX(("%s: SCB:"MACF" watchdog_count=%d.\n",
					__FUNCTION__, ETHER_TO_MACF(scb->ea),
					ulinfo->watchdog_count));
			} else {
				ulinfo->prev_nutxd_status = ulinfo->nutxd_status;
			}

			if (ulinfo->watchdog_count >= ti->watchdog_timeout) {
				/* Terminate UTXD request if the watchdog
				 * count threshold is reached
				 */
				ulinfo->current_req->state = ULMU_REQ_COMPLETED;
				if (ti->callback_function) {
					utxd_accmulator_t *accum;
					uint32 avglen;

					accum = DRV_CTX_ACCMULATOR(ulinfo->current_req);
					avglen = (accum->npkts) ?
						(accum->bytes_completed/accum->npkts) : 0;

					ti->callback_function(ulmu->wlc,
						scb, ti, ti->callback_parameter,
						ULMU_STATUS_WATCHDOG, accum->bytes_completed,
						avglen, accum->duration);

					WL_MUTX(("%s: SCB:"MACF" Terminating UTXD request,"
						"watchdog_count=%d.\n",
						__FUNCTION__, ETHER_TO_MACF(scb->ea),
						ulinfo->watchdog_count));
				}

				/* if ulinfo->watchdog_count was incremented
				 * clear_watchdog will be FALSE
				 */
				clear_watchdog = TRUE;
				WLCNTINCR(ulmu_scb->scb_stats->drv_counters.stop_watchdog);
				wlc_ulmu_drv_free_trigger(ulinfo);
				ulinfo->current_req = NULL;
			}

			if (clear_watchdog) {
				ulinfo->watchdog_count = 0;
			}
		}
	}
}
#ifdef BCMDBG
/* Apply global user confgured overides when making new UTXD request */
static void
wlc_ulmu_drv_apply_iovar_settings(wlc_ulmu_info_t *ulmu, wlc_ulmu_trigger_info_t *trigger_info)
{
	packet_trigger_info_t *ti;

	ASSERT(ulmu);
	ASSERT(trigger_info);

	ti = &trigger_info->trigger_type.packet_trigger;

	ti->trigger_bytes = wlc_ulmu_drv_get_bytecount(ulmu);
	ti->watchdog_timeout = wlc_ulmu_drv_get_watchdog(ulmu);
}

/* Prepare a generic trigger request */
static void
wlc_ulmu_drv_prepare_trigger_request(wlc_ulmu_info_t *ulmu,
		wlc_ulmu_trigger_info_t *ti_data, ulmu_cb_t callback_function)
{
	packet_trigger_info_t *ti;

	ASSERT(ti_data);

	/* Apply IOVAR override values */
	wlc_ulmu_drv_apply_iovar_settings(ulmu, ti_data);
	ti = &ti_data->trigger_type.packet_trigger;

	ti->callback_function = callback_function;
	ti->callback_parameter = (void *)ti;
	ti->callback_reporting_threshold = ULMU_CB_TEST_THRESHOLD;
	ti->multi_callback = FALSE;
	ti->qos_null_threshold = ULMU_QOSNULL_LIMIT;
	ti->failed_request_threshold = ULMU_TIMEOUT_LIMIT;
}

/* Get auto trigger state */
static uint32
wlc_ulmu_drv_get_autotrigger(wlc_ulmu_info_t *ulmu)
{
	ASSERT(ulmu);
	return DRV_GCONFIG(ulmu)->drv_autotrigger;
}

/* Set trigger auto state */
static void
wlc_ulmu_drv_set_autotrigger(wlc_ulmu_info_t *ulmu, uint32 iter)
{
	ASSERT(ulmu);
	DRV_GCONFIG(ulmu)->drv_autotrigger = iter;
}

/* TAF callback stub, this stub queues 1 UTXD per callback */
static int wlc_ulmu_drv_trigger_callback(wlc_info_t *, struct scb *,
	packet_trigger_info_t *ti,
	void *, uint32, uint32, uint32, uint32);

static int
wlc_ulmu_drv_trigger_callback(wlc_info_t *wlc, struct scb *scb,
	packet_trigger_info_t *ti,
	void *arg, uint32 status_code, uint32 bytes_consumed,
	uint32 avg_pktsz, uint32 duration)
{
	wlc_ulmu_info_t *ulmu;
	bool newTrigger = FALSE;
	uint32 iter;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(arg);

	ulmu = wlc->ulmu;

	if (ti == NULL) {
			WL_MUTX(("%s: SCB:"MACF". ti is NULL\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea)));
			return BCME_ERROR;
	}

	switch (status_code) {
		case ULMU_STATUS_INPROGRESS:
			WL_MUTX(("%s: SCB:"MACF" Inprog. %d%% bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				ti->callback_reporting_threshold,
				bytes_consumed, avg_pktsz, duration));
			break;
		case ULMU_STATUS_THRESHOLD:
			WL_MUTX(("%s: SCB:"MACF" Thres. %d%% bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				ti->callback_reporting_threshold,
				bytes_consumed, avg_pktsz, duration));
			break;
		case ULMU_STATUS_COMPLETE:
			WL_MUTX(("%s: SCB:"MACF" Done. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			newTrigger = TRUE;
			break;
		case ULMU_STATUS_QOSNULL:
			WL_MUTX(("%s: SCB:"MACF" ABRT:QN. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			newTrigger = TRUE;
			break;
		case ULMU_STATUS_TIMEOUT:
			WL_MUTX(("%s: SCB:"MACF" ABRT:TO. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			newTrigger = TRUE;
			break;
		case ULMU_STATUS_TRIGCNT:
			WL_MUTX(("%s: SCB:"MACF" ABRT:TC. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			newTrigger = TRUE;
			break;
		case ULMU_STATUS_WATCHDOG:
			WL_MUTX(("%s: SCB:"MACF" ABRT:WD. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			newTrigger = TRUE;
			break;
		case ULMU_STATUS_EVICTED:
			WL_MUTX(("%s: SCB:"MACF" ABRT:EV. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			/* User got kicked out of UL schedule , no new trigger */
			break;
		case ULMU_STATUS_NOTADMITTED:
			WL_MUTX(("%s: SCB:"MACF" ABRT:NA. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				bytes_consumed, avg_pktsz, duration));
			/* User got kicked out of UL schedule , no new trigger */
			break;
		default:
			WL_MUTX(("%s: SCB:"MACF" UNK:%d. bytes=%d avglen=%d dur=%d\n",
				__FUNCTION__, ETHER_TO_MACF(scb->ea), status_code,
				bytes_consumed, avg_pktsz, duration));
			break;
	}

	iter = wlc_ulmu_drv_get_autotrigger(ulmu);
	newTrigger &= (iter > 0);

	if (newTrigger) {
		wlc_ulmu_trigger_info_t trigger_info;
		uint32 n;
		int rc;

		/* Prepare trigger request */
		wlc_ulmu_drv_prepare_trigger_request(ulmu, &trigger_info,
			wlc_ulmu_drv_trigger_callback);

		for (n = 0; n < iter; n++) {

			rc = wlc_ulmu_drv_trigger_request(ulmu->wlc, scb,
					ULMU_PACKET_TRIGGER, &trigger_info);

			if (rc == BCME_OK) {
				WL_MUTX(("%s: SCB:"MACF" triggered.\n",
					__FUNCTION__, ETHER_TO_MACF(scb->ea)));
			} else {
				WL_MUTX(("%s: SCB:"MACF" error rc=0x%x.\n",
					__FUNCTION__, ETHER_TO_MACF(scb->ea), rc));
				break;
			}
		}
		return (rc);
	} else {
		return BCME_OK;
	}

}

/* TAF Stub */
static int
wlc_ulmu_drv_trigger_scb(wlc_ulmu_info_t *ulmu, uint16 niter)
{
	uint max_ulofdma_usrs;
	uint schpos;
	wlc_ulmu_trigger_info_t trigger_info;
	int rc, iter = 0;

	if (niter < 1) {
		return BCME_ERROR;
	};

	ASSERT(ulmu);
	max_ulofdma_usrs = wlc_txcfg_max_clients_get(ulmu->wlc->txcfg, ULOFDMA);

	/* Prepare trigger request */
	wlc_ulmu_drv_prepare_trigger_request(ulmu, &trigger_info,
		wlc_ulmu_drv_trigger_callback);

	do {
		iter++;
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			struct scb *scb = ulmu->scb_list[schpos];

			if (scb) {
				rc = wlc_ulmu_drv_trigger_request(ulmu->wlc, scb,
					ULMU_PACKET_TRIGGER, &trigger_info);
				WL_MUTX(("%s: SCB:"MACF" triggered.\n",
					__FUNCTION__, ETHER_TO_MACF(scb->ea)));
				if (rc != BCME_OK) {
						break;
				} /* if rc... */
			} /* if (scb)... */
		} /* for(...) */
	} while ((iter < niter) && (rc == BCME_OK));

	return rc;
}

/* UTXD Request list debugging routines */
/* Return current active UTXD request pointer */
trigger_ctx_entry_t *
wlc_ulmu_active_utxd_request(wlc_info_t *wlc, struct scb *scb)
{
	scb_ulmu_t *ulmu_scb;

	if (!wlc || !wlc->ulmu || !scb) {
		return NULL;
	}

	ulmu_scb = SCB_ULMU(wlc->ulmu, scb);

	if (!ulmu_scb) {
		return NULL;
	}

	return (DRV_SCB_ULINFO(ulmu_scb))->current_req;
}

/* Return current pending UTXD request pointer */
trigger_ctx_entry_t *
wlc_ulmu_pending_utxd_request(wlc_info_t *wlc, struct scb *scb)
{
	scb_ulmu_t *ulmu_scb;
	ulmu_drv_scbinfo *ulinfo;
	trigger_ctx_list_t *list;

	if (!wlc || !wlc->ulmu || !scb) {
		return NULL;
	}

	ulmu_scb = SCB_ULMU(wlc->ulmu, scb);

	if (!ulmu_scb) {
		return NULL;
	}

	list = DRV_CTX_LIST(ulinfo);
	if (list && list->head &&
			(list->head->state != ULMU_REQ_INACTIVE)) {
		return list->head;
	}

	return NULL;
}

/* Return next pending UTXD request pointer */
trigger_ctx_entry_t *wlc_ulmu_next_utxd_request(
	trigger_ctx_entry_t *utxd_request)
{
	if (utxd_request && utxd_request->next) {
		if (utxd_request->next->state != ULMU_REQ_INACTIVE) {
			return utxd_request->next;
		}
	}
	return NULL;
}

/* Returns trigger setup info form the UTXD request */
packet_trigger_info_t *wlc_ulmu_trigger_info(
	trigger_ctx_entry_t *utxd_request)
{
	if (utxd_request) {
		return (DRV_CTX_TI(utxd_request));
	} else {
		return NULL;
	}
}

#endif /* BCMDBG */

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
static void
wlc_ulmu_drv_print_scb_stats(scb_ulmu_t *ulmu_scb, struct scb *scb, bcmstrbuf_t *b)
{
	ulmu_drv_counters_t *counters;
	ulmu_drv_scbinfo *ulinfo;

	ASSERT(ulmu_scb);
	ASSERT(scb);
	ASSERT(b);
	ASSERT(ulmu_scb->scb_stats);

	counters = DRV_SCB_COUNTERS(ulmu_scb);
	ASSERT(counters);

	ulinfo = DRV_SCB_ULINFO(ulmu_scb);
	ASSERT(ulinfo);

	bcm_bprintf(b, "\nDriver trigger stats summary:\n");
	bcm_bprintf(b, "  nutxd=%d ntxststus=%d ncallback=%d npending=%d fid_mismatch=%d\n",
		counters->nutxd, counters->nutxd_status, counters->ncallback,
		DRV_CTX_LIST(ulinfo)->len, counters->fid_mismatch);
	bcm_bprintf(b, "  bytes: triggered=%d received=%d zero=%d ovr=%d\n",
		counters->nbytes_triggered, counters->nbytes_received,
		counters->zero_bytes, counters->byte_overrun);
	bcm_bprintf(b, "Driver UTXD completion reasons:\n");
	bcm_bprintf(b, "  bytelimit=%d qosnull=%d timeout=%d"
		" trigcnt=%d watchdog=%d evicted=%d\n",
		counters->stop_byte_limit, counters->stop_qos_null, counters->stop_timeout,
		counters->stop_trigcnt, counters->stop_watchdog, counters->stop_evicted);
	bcm_bprintf(b, "Driver Callback counts:\n");
	bcm_bprintf(b, "  thres=%d mult=%d spur=%d nocb=%d noreq=%d\n",
		counters->threshold_callback, counters->multi_callback,
		counters->spurious_callback, counters->no_callback, counters->no_request);

	if (ulinfo->current_req && DRV_CTX_TI(ulinfo->current_req)) {
		packet_trigger_info_t *ti = DRV_CTX_TI(ulinfo->current_req);

		bcm_bprintf(b, "Settings:\n");
		bcm_bprintf(b, "  bytes=%d threshold(cb=%d qn=%d tout=%d) wd=%d mc=%d\n",
			ti->trigger_bytes, ti->callback_reporting_threshold,
			ti->qos_null_threshold, ti->failed_request_threshold,
			ti->watchdog_timeout, ti->multi_callback);
	};
	bcm_bprintf(b, "\n");
}
#endif /* BCMDBG */
#else
#define wlc_ulmu_drv_print_scb_stats(ulmu_scb, scb, b)
#define wlc_ulmu_drv_scb_init(ulmu, scb)	(BCME_OK)
#define wlc_ulmu_drv_scb_deinit(ulmu, scb)
#define wlc_ulmu_drv_ulmu_attach(ulmu)
#define wlc_ulmu_drv_ulmu_detach(ulmu)
#define wlc_ulmu_drv_add_usr(ulmu_scb)
#define wlc_ulmu_drv_del_usr(wlc, scb, ulmu_scb)

static int
wlc_ulmu_release_bytes(wlc_ulmu_info_t *ulmu, scb_t *scb, uint16 bufsize)
{
	int ret;
	scb_ulmu_t* ulmu_scb = SCB_ULMU(ulmu, scb);

	ulmu_scb->bufsize = bufsize; // in KB unit

	wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRUPD_TRFC);
	ret = wlc_ulmu_post_utxd(ulmu);

	return ret;
}

#endif /* ULMU_DRV */

static bool
wlc_ulmu_drv_ucautotrigger(wlc_ulmu_info_t *ulmu, struct scb *scb)
{
	bool ret = TRUE;
	int err;

	SCB_ULMU(ulmu, scb)->bufsize = ULMU_BUFSZ_DFLT;
	if (ULMU_FLAGS_DSCHED_GET(ulmu->flags) == ULMU_ON) {
		wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRUPD_RATE);
	} else {
		wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRADD_ALL);
	}
	if ((err = wlc_ulmu_post_utxd(ulmu)) != BCME_OK) {
		ret = FALSE;
		WL_ERROR(("wl%d: %s: fail to post utxd for "MACF" err %d schpos %d "
			"state 0x%x into ul ofdma list ulmu_state:%d\n",
			ulmu->wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea), err,
			SCB_ULMU(ulmu, scb)->schpos, scb->state, SCB_ULMU(ulmu, scb)->state));
	} else {
		WL_MUTX(("wl%d: %s: add sta "MACF" schpos %d state 0x%x "
			"into ul ofdma list ulmu_state:%d\n",
			ulmu->wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea),
			SCB_ULMU(ulmu, scb)->schpos, scb->state, SCB_ULMU(ulmu, scb)->state));
	}
	return ret;
}

int
wlc_ulmu_stats_upd(wlc_ulmu_info_t *ulmu, scb_t *scb, tx_status_t *txs)
{
	uint8 nusrs, rx_cnt, rxsucc_cnt;
	uint8 mcs, nss, rateidx, last, reason;
	int32 phyrssi;
	uint32 txop, qncnt, agglen;
	scb_ulmu_t* ulmu_scb;
	ulmu_stats_t *ulstats;
	uint32 cur_mlen = 0;
	ru_type_t ru_type;
	ratespec_t rspec;
	ulmu_scb = SCB_ULMU(ulmu, scb);

	if (ulmu_scb == NULL) {
		return BCME_ERROR;
	}

	if ((ulstats = ulmu_scb->scb_stats) == NULL) {
		return BCME_ERROR;
	}

	/* 3*txop = ((lsig + 5) << 2) + 51 */
	txop = (((TGTXS_LSIG(txs->status.s2) + 5) << 2) + 51) << OPERAND_SHIFT;
	mcs = TGTXS_MCS(txs->status.s4);
	nss = TGTXS_NSS(txs->status.s4);
	rateidx = nss * MAX_HE_RATES + mcs;
	ASSERT(rateidx < AMPDU_MAX_HE);
	qncnt = TGTXS_QNCNT(txs->status.ack_map1);
	agglen = TGTXS_AGGLEN(txs->status.ack_map1);
	phyrssi = TGTXS_PHYRSSI(txs->status.s8);
	ASSERT(phyrssi < PHYRSSI_2SCOMPLEMENT);
	phyrssi = (phyrssi - ((phyrssi >= PHYRSSI_SIGN_MASK) <<
		PHYRSSI_2SCOMPLEMENT_SHIFT)) << OPERAND_SHIFT;
	nusrs = TGTXS_NUSR(txs->status.s1);
	ASSERT(nusrs > 0);
	last = TGTXS_LAST(txs->status.s5);
	rx_cnt = TGTXS_LCNT(txs->status.s5);
	rxsucc_cnt = TGTXS_GDFCSCNT(txs->status.s5);

	/* update ul ofdma global stats */
	if (rx_cnt > 0) {
		WLCNTADD(ulmu->gstats.lcnt[rateidx], rx_cnt);
		WLCNTADD(ulmu->gstats.gdfcscnt[rateidx], rxsucc_cnt);
	}
	if (last == 1)
		WLCNTADD(ulmu->gstats.usrhist[nusrs-1], 1);

	/* update ul ofdma per user stats */
	WLCNTADD(ulstats->nupd, 1);

	if (rx_cnt == 0) {
		WLCNTADD(ulstats->nfail, 1);
		ulmu_scb->fail_cnt++;
	} else {
		WLCNTADD(ulstats->sum_lcnt, rx_cnt);
		WLCNTADD(ulstats->lcnt[rateidx], rx_cnt);
		WLCNTADD(ulstats->gdfcscnt[rateidx], rxsucc_cnt);
		ulmu_scb->fail_cnt = 0;
	}

	if (rxsucc_cnt == 0) {
		WLCNTADD(ulstats->nbadfcs, 1);
	} else {
		/* skip rssi update if it is a pre-defined corrupted value */
		if (phyrssi != ((ULMU_PHYRSSI_MAX) << OPERAND_SHIFT)) {
			WLCNTADD(ulstats->nvldrssi, 1);
			if (ulstats->rssi_stats.min_rssi > phyrssi) {
				ulstats->rssi_stats.min_rssi = phyrssi;
			}

			if (ulstats->rssi_stats.max_rssi < phyrssi) {
				ulstats->rssi_stats.max_rssi = phyrssi;
			}
			WLCNTADD(ulstats->rssi_stats.avg_rssi, phyrssi);
			WLCNTADD(ulstats->txop, txop);
			WLCNTADD(ulstats->agglen, agglen);
			WLCNTADD(ulstats->qncnt, qncnt);
		}

		if (rxsucc_cnt == qncnt) {
			ulmu_scb->qnullonly_cnt++;
		} else {
			ulmu_scb->qnullonly_cnt = 0;
		}
	}

	if (ulstats->nupd > ulstats->nfail) {
		ulstats->aggn = ulstats->sum_lcnt / (ulstats->nupd - ulstats->nfail);
	}

	if (ulstats->sum_lcnt) {
		ulstats->mlen = ulstats->agglen / ulstats->sum_lcnt;
	}

#ifdef ULMU_DRV
	reason = TGTXS_REASON(txs->status.s1);
	if ((ulmu->mode == ULMU_UTXD_MODE) &&
		(ULMU_FLAGS_DSCHED_GET(ulmu->flags) == ULMU_ON)) {
		ulmu_drv_utxd_status_t status;

		status.scb = scb;
		status.triggered_bytes = agglen;
		status.npkts = rxsucc_cnt;
		status.duration = (txop / 3) >> OPERAND_SHIFT;
		status.frameid =
			TGTXS_REMKB(txs->status.ack_map2) >> ULSTS_FRAMEID_SHIFT;
		status.ru_idx = TGTXS_RUIDX(txs->status.s4);
		status.mcs = mcs;
		status.nss = nss + 1; /* NSS in uCode is zero based */
		status.qn_count = ulmu_scb->qnullonly_cnt;
		status.return_status = reason;

		/* Update the utxd stats and schedule the next UL trigger */
		wlc_ulmu_drv_process_utxd_status(ulmu, &status);
	}
#else
	if ((reason = TGTXS_REASON(txs->status.s1))) {
		WL_INFORM(("wl%d: %s: Stop trigger for "MACF" reason %x\n", ulmu->wlc->pub->unit,
			__FUNCTION__, ETHER_TO_MACF(scb->ea), reason));
		if (ULMU_FLAGS_DSCHED_GET(ulmu->flags) == ULMU_ON) {
			wlc_ulmu_release_bytes(ulmu, scb, ULMU_BUFSZ_DFLT);
		}
	}

	if (ULMU_IS_UTXD(ulmu->mode) && ULMU_FLAGS_DSCHED_GET(ulmu->flags) == ULMU_ON) {
		if (TGTXS_REMKB(txs->status.ack_map2) < ULMU_BUFSZ_THRSH) {
			WL_MUTX(("wl%d: %s:  "MACF" remaining %d \n", ulmu->wlc->pub->unit,
				__FUNCTION__, ETHER_TO_MACF(scb->ea),
				TGTXS_REMKB(txs->status.ack_map2)));
			wlc_ulmu_release_bytes(ulmu, scb, ULMU_BUFSZ_DFLT);
		}
	}
#endif /* ULMU_DRV */

	if (rxsucc_cnt && rxsucc_cnt != qncnt) {
		uint16 cur_aggn = rx_cnt - qncnt;
		cur_aggn = (cur_aggn << ULMU_NF_AGGN);
		if (ulmu_scb->aggnma != 0) {
			ULMU_MOVING_AVG(&ulmu_scb->aggnma, cur_aggn,
				ULMU_EMA_ALPHA);
		} else {
			ulmu_scb->aggnma = cur_aggn;
		}
	}

	if (rxsucc_cnt > qncnt) {
		cur_mlen = (agglen << ULMU_NF_AGGLEN) / (rxsucc_cnt - qncnt);
		if (ulmu_scb->mlenma != 0) {
			ULMU_MOVING_AVG(&ulmu_scb->mlenma, cur_mlen, ULMU_EMA_ALPHA);
		} else {
			ulmu_scb->mlenma = cur_mlen;
		}
	}

	rspec = HE_RSPEC(mcs, nss+1);
	ru_type = wf_he_ruidx_to_ru_type(TGTXS_RUIDX(txs->status.s4));

	ulmu_scb->tgs_exprecv_bytes = wf_he_rspec_ru_type_to_rate(rspec, ru_type);
	/* compute expected bytes in trigger status based on lsig
	 * i.e., exp_bytes = (phy_rate * ((lsig * 8) / 6)) / (1000 * 8)
	 * i.e., exp_bytes = (phy_rate * lsig / (1000 * 6), /1000 is to convert phyrate
	 * KBPS to BPS
	 */
	ulmu_scb->tgs_exprecv_bytes = (ulmu_scb->tgs_exprecv_bytes *
		TGTXS_LSIG(txs->status.s2)) / (1000 * 6);
	if (ulmu_scb->tgs_exprecv_bytes) {
		wlc_ulmu_scb_reqbytes_eval(ulmu, scb);
	}

	return BCME_OK;
}

static void
wlc_ulmu_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_ulmu_info_t *ulmu = (wlc_ulmu_info_t *) ctx;
	wlc_info_t *wlc = ulmu->wlc;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	uint8 oldstate;
	scb_ulmu_t *ulmu_scb;
	ASSERT(notif_data);

	scb = notif_data->scb;
	ASSERT(scb);
	oldstate = notif_data->oldstate;
	bsscfg = scb->bsscfg;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(oldstate);

	ulmu_scb = SCB_ULMU(ulmu, scb);

	if (D11REV_LT(wlc->pub->corerev, 129) ||
		!SCB_HE_CAP(scb) || !BSSCFG_AP(bsscfg) ||
#ifndef WL11AX
		/* force to return if WL11AX compilation flag is off */
		TRUE ||
#endif /* WL11AX */
		SCB_INTERNAL(scb) || !ulmu_scb || !HE_ULMU_ENAB(wlc->pub)) {
		return;
	}

	if ((!WSEC_ENABLED(bsscfg->wsec) && !(oldstate & ASSOCIATED) &&
		SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) ||
		(WSEC_ENABLED(bsscfg->wsec) && SCB_AUTHENTICATED(scb) &&
		SCB_ASSOCIATED(scb) && SCB_AUTHORIZED(scb) && !(oldstate & AUTHORIZED))) {
		/* Let the rx monitoring trigger the UL MU admission */
	} else if ((oldstate & ASSOCIATED) && !(SCB_ASSOCIATED(scb) && SCB_AUTHENTICATED(scb))) {
		// Associated -> disassoc or deauth
		wlc_ulmu_admit_clients(wlc, scb, FALSE);
		wlc_ulmu_oper_state_upd(wlc->ulmu, scb, ULMU_SCB_INIT);
	} else {
		/* pass */
	}

}

static void
wlc_ulmu_set_ulpolicy(wlc_ulmu_info_t *ulmu, int16 policy)
{
	if (policy > ULMU_POLICY_MAX) {
		ulmu->policy = ULMU_POLICY_AUTO;
	} else {
		ulmu->policy = policy;
	}
}

static int
wlc_ulmu_get_ulpolicy(wlc_ulmu_info_t *ulmu)
{
	return ulmu->policy;
}

/* ======== scb cubby ======== */
static int
wlc_ulmu_scb_init(void *ctx, scb_t *scb)
{
	wlc_ulmu_info_t *ulmu = ctx;
	wlc_info_t *wlc = ulmu->wlc;
	scb_ulmu_t **psh = SCB_ULMU_CUBBY(ulmu, scb);
	int ret = BCME_OK;

	ASSERT(*psh == NULL);

	*psh = wlc_scb_sec_cubby_alloc(wlc, scb, wlc_ulmu_scb_secsz(wlc, scb));

	if (*psh != NULL) {
		wlc_ulmu_scb_sched_init(ulmu, scb);

		if (HE_ULMU_ENAB(wlc->pub)) {
			ret = wlc_ulmu_scb_stats_init(ulmu, scb);
			if (ret == BCME_OK) {
				ret = wlc_ulmu_drv_scb_init(ulmu, scb);
			} else {
				if ((*psh)->scb_stats != NULL) {
					MFREE(wlc->osh, (*psh)->scb_stats, sizeof(ulmu_stats_t));
				}
			}
		}
	}

	return BCME_OK;
}

static void
wlc_ulmu_scb_deinit(void *ctx, scb_t *scb)
{
	wlc_ulmu_info_t *ulmu = ctx;
	wlc_info_t *wlc = ulmu->wlc;
	scb_ulmu_t **psh = SCB_ULMU_CUBBY(ulmu, scb);
	scb_ulmu_t *sh = SCB_ULMU(ulmu, scb);
	int i;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	/* Memory not allocated for scb, return */
	if (!sh) {
		return;
	}

	for (i = 0; i < max_ulofdma_usrs; i++) {
		if (ulmu->scb_list[i] == scb) {
			ASSERT(ulmu->num_usrs >= 1);
			ulmu->num_usrs--;
			ulmu->scb_list[i] = NULL;
		}
	}

	wlc_ulmu_drv_scb_deinit(ulmu, scb);

	if (sh->scb_stats != NULL) {
		MFREE(wlc->osh, sh->scb_stats, sizeof(ulmu_stats_t));
		ASSERT(ulmu != NULL);
		--ulmu->num_scb_ulstats;
		ASSERT(ulmu->num_scb_ulstats >= 0);
	}

	if (sh->ul_rmem != NULL) {
		MFREE(wlc->osh, sh->ul_rmem, sizeof(d11ulmu_rmem_t));
		sh->ul_rmem = NULL;
	}

	wlc_scb_sec_cubby_free(wlc, scb, sh);
	*psh = NULL;
}

static uint
wlc_ulmu_scb_secsz(void *ctx, scb_t *scb)
{
	if (scb && !SCB_INTERNAL(scb)) {
		return sizeof(scb_ulmu_t);
	} else {
		return 0;
	}
}

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void
wlc_ulmu_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b)
{
	wlc_ulmu_info_t *ulmu = ctx;
	scb_ulmu_t *sh = SCB_ULMU(ulmu, scb);

	if (sh == NULL) {
		return;
	}

	bcm_bprintf(b, "     UL schpos %d\n", sh->schpos);
}

#endif // endif

static int
wlc_ulmu_scb_stats_init(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	scb_ulmu_t *ulmu_scb;
	wlc_info_t *wlc;

	wlc = ulmu->wlc;

	if (PIO_ENAB_HW(ulmu->wlc->wlc_hw)) {
		return BCME_OK;
	}

	if (SCB_INTERNAL(scb)) {
		return BCME_OK;
	}

	ulmu_scb = SCB_ULMU(ulmu, scb);

	ASSERT(ulmu != NULL);
	if ((ulmu_scb->scb_stats =
		MALLOCZ(ulmu->wlc->osh, sizeof(ulmu_stats_t))) != NULL) {
		++ulmu->num_scb_ulstats;
		ulmu_scb->scb_stats->rssi_stats.min_rssi =
			PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
		ulmu_scb->scb_stats->rssi_stats.max_rssi =
			-PHYRSSI_2SCOMPLEMENT << OPERAND_SHIFT;
		ulmu_scb->scb_stats->aggn = wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx);
		ulmu_scb->scb_stats->mlen = ULMU_MLEN_INIT;
	} else {
		WL_ERROR(("wl%d: %s: Fail to get scb_stats STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return BCME_NOMEM;
	}
	return BCME_OK;
}

static void
wlc_ulmu_scb_sched_init(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	scb_ulmu_t *ulmu_scb;

	if (PIO_ENAB_HW(ulmu->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	ulmu_scb = SCB_ULMU(ulmu, scb);
	if (!ulmu_scb) {
		return;
	}

	ulmu_scb->schpos = ULMU_SCHPOS_INVLD;
	ulmu_scb->trigcnt = ulmu->txd.txcnt;
	ulmu_scb->scb_bl = scb;	/* save the scb back link */
	ulmu_scb->ulmu = ulmu;	/* save the ulmu back link */
	ulmu_scb->aggnma = wlc_ampdu_rx_get_ba_max_rx_wsize(ulmu->wlc->ampdu_rx);
	ulmu_scb->mlenma = ULMU_MLEN_INIT;
}

/* Function to get empty slot in ul_usr_list
 * If there is empty slot return the index
 * Otherwise, return -1
 */
static int8
wlc_ulmu_ulofdma_get_empty_schpos(wlc_ulmu_info_t *ulmu)
{
	int8 schpos;
	wlc_info_t *wlc = ulmu->wlc;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	if (ulmu->num_usrs >= max_ulofdma_usrs) {
		return ULMU_SCHPOS_INVLD;
	}
	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if (ulmu->scb_list[schpos] == NULL) {
			return schpos;
		}
	}
	ASSERT(!"ULOFDMA NO EMPTY SLOT");
	return ULMU_SCHPOS_INVLD;
}

/* Function to find if a given scb in ul_usr_list
 * If yes, then return the schpos; Otherwise, return -1
 */
static int8
wlc_ulmu_scb_lkup(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	int8 i, schpos = ULMU_SCHPOS_INVLD;
	wlc_info_t *wlc = ulmu->wlc;
	for (i = 0; i < wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA); i++) {
		if (ulmu->scb_list[i] == scb) {
			schpos = i;
			break;
		}
	}
	return schpos;
}

/* Function to find if a given ether addr in ul_usr_list
 * If yes, then return the schpos and set scb pointer; Otherwise, return -1 and NULL scb pointer
 */
static int8
wlc_ulmu_scb_onaddr_lkup(wlc_ulmu_info_t *ulmu, struct ether_addr *ea,
	scb_t **pp_scb)
{
	int8 idx, schpos = ULMU_SCHPOS_INVLD;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = ulmu->wlc;
	char eabuf[ETHER_ADDR_STR_LEN];
	scb_t *scb = NULL;

	BCM_REFERENCE(eabuf);

	FOREACH_BSS(wlc, idx, bsscfg) {
		if ((scb = wlc_scbfind(wlc, bsscfg, ea))) {
			schpos = wlc_ulmu_scb_lkup(ulmu, scb);
			break;
		}
	}
	*pp_scb = scb;

	WL_MUTX(("wl%d: %s: addr %s schpos %d scb %p\n",
		wlc->pub->unit, __FUNCTION__,
		scb ? bcm_ether_ntoa(&scb->ea, eabuf) : "null", schpos, scb));
	return schpos;
}

/* Add the new user to ul ofdma scheduler. Return TRUE if added; otherwise FALSE */
static bool
wlc_ulmu_ulofdma_add_usr(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	scb_ulmu_t *ulmu_scb;

	wlc_info_t *wlc = ulmu->wlc;
	bool ret = FALSE;
	d11ulmu_rmem_t *rmem;
	int i;
	uint8 nss;
	int8 schpos;

	STATIC_ASSERT((ULMU_RMEMIDX_FIRST >= AMT_IDX_RLM_RSVD_SIZE) &&
		(ULMU_RMEMIDX_FIRST < AMT_IDX_SIZE_11AX));
	STATIC_ASSERT((ULMU_RMEMIDX_LAST >= AMT_IDX_RLM_RSVD_SIZE) &&
		(ULMU_RMEMIDX_LAST < AMT_IDX_SIZE_11AX));
	STATIC_ASSERT(ULMU_RMEMIDX_FIRST <= ULMU_RMEMIDX_LAST);
	STATIC_ASSERT(ULMU_RMEMIDX_LAST < AMT_IDX_DLOFDMA_RSVD_START);

	if (ulmu->num_usrs >= wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA)) {
			WL_MUTX(("wl%d: %s: num_usrs exceeded(%d) STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__, ulmu->num_usrs,
			ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	if ((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	ASSERT(ulmu_scb->ul_rmem == NULL);
	schpos = wlc_ulmu_ulofdma_get_empty_schpos(ulmu);
	if (schpos == ULMU_SCHPOS_INVLD) {
		WL_MUTX(("wl%d: %s: Invalid schpos STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	if (!(rmem = MALLOCZ(wlc->osh, sizeof(d11ulmu_rmem_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		ASSERT(1);
		return ret;
	}

	ulmu_scb->schpos = schpos;
	ulmu_scb->rmemidx = ULMU_RMEMIDX_FIRST + schpos;
	SCB_ULOFDMA_ENABLE(scb);

	/* populate rate block to ucfg: autorate by default */
	D11_ULOTXD_UCFG_SET_MCSNSS(ulmu_scb->ucfg, 0x17);
	D11_ULOTXD_UCFG_SET_TRSSI(ulmu_scb->ucfg, ULMU_OFDMA_TRSSI_INIT);

	/* check if need to override with global fixed settings */
	if ((ulmu->g_ucfg & D11_ULOTXD_UCFG_FIXRT_MASK)) {
		D11_ULOTXD_UCFG_SET_MCSNSS(ulmu_scb->ucfg,
			D11_ULOTXD_UCFG_GET_MCSNSS(ulmu->g_ucfg));
		ulmu_scb->ucfg |= D11_ULOTXD_UCFG_FIXRT_MASK;
	}

	if ((ulmu->g_ucfg & D11_ULOTXD_UCFG_FIXRSSI_MASK)) {
		D11_ULOTXD_UCFG_SET_TRSSI(ulmu_scb->ucfg,
			D11_ULOTXD_UCFG_GET_TRSSI(ulmu->g_ucfg));
		ulmu_scb->ucfg |= D11_ULOTXD_UCFG_FIXRSSI_MASK;
	}

	/* TODO: now just use hardcoded bw80 mcsmap */
	for (i = 0; i < ARRAYSIZE(rmem->mcsbmp); i++) {
		rmem->mcsbmp[i] = HE_MAX_MCS_TO_MCS_MAP(
			(((scb->rateset.he_bw80_tx_mcs_nss >> (i*2)) & 0x3)));
	}
	nss = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80_tx_mcs_nss); /* 1-based NSS */
	D11_ULORMEM_RTCTL_SET_MCS(rmem->rtctl,
		HE_MAX_MCS_TO_INDEX(HE_MCS_MAP_TO_MAX_MCS(rmem->mcsbmp[0])));
	D11_ULORMEM_RTCTL_SET_NSS(rmem->rtctl, nss == 0 ? 0 : nss-1);
	WL_MUTX(("wl%d: %s: rtctl 0x%x mcsmap 0x%x sta addr "MACF"\n",
		wlc->pub->unit, __FUNCTION__,
		rmem->rtctl, scb->rateset.he_bw80_tx_mcs_nss,
		ETHER_TO_MACF(scb->ea)));
	rmem->aggnma = (64 << 4); // XXX: get the BAwin from linkentry
	rmem->mlenma[0] = 0;
	rmem->mlenma[1] = 0x30; // AMSDU byte size 3072 << 10

	ulmu->scb_list[schpos] = scb;
	ulmu_scb->ul_rmem = rmem;
	ulmu_scb->rmem_upd = TRUE;
	ulmu->num_usrs++;
	ret = TRUE;

	if (ULMU_IS_UTXD(ulmu->mode)) {
		ret = wlc_ulmu_drv_ucautotrigger(ulmu, scb);
	}

	ulmu_scb->state = ULMU_SCB_ADMT;

	wlc_ulmu_drv_add_usr(ulmu_scb);

#if defined WLTAF && defined ULMU_DRV
	if (wlc_taf_ul_enabled(wlc->taf_handle)) {
		WL_TAFF(ulmu->wlc, " called\n");
		wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_PARAM(TRUE),
			TAF_SCBSTATE_MU_UL_OFDMA);

		wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_UL, TAF_SCBSTATE_SOURCE_ENABLE);

		wlc_taf_link_state(ulmu->wlc->taf_handle, scb, PRIO_8021D_BE, TAF_UL,
			TAF_LINKSTATE_ACTIVE);

		wlc_taf_schedule(wlc->taf_handle, PRIO_8021D_BE, scb, FALSE);
	}
#endif // endif
	return ret;
}

/* Remove a given user from ul ofdma scheduler. Return TRUE if removed; otherwise FALSE */
bool
wlc_ulmu_del_usr(wlc_ulmu_info_t *ulmu, scb_t *scb, bool is_bss_up)
{
	wlc_info_t *wlc = ulmu->wlc;
	bool ret = FALSE;
	scb_ulmu_t *ulmu_scb;
	int8 schpos;

	if (SCB_INTERNAL(scb)) {
		return ret;
	}

	if ((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	schpos = wlc_ulmu_scb_lkup(ulmu, scb);
	if (schpos == ULMU_SCHPOS_INVLD) {
		return ret;
	}

	ulmu_scb->state = ULMU_SCB_EVCT;
	if (is_bss_up && ULMU_IS_UTXD(ulmu->mode)) {
		wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRDEL);
		wlc_ulmu_post_utxd(ulmu);
	}
	SCB_ULOFDMA_DISABLE(scb);
	ulmu_scb->schpos = ULMU_SCHPOS_INVLD;
	ulmu->scb_list[schpos] = NULL;
	ASSERT(ulmu->num_usrs >= 1);
	ulmu->num_usrs--;
	if (ulmu_scb->ul_rmem != NULL) {
		MFREE(wlc->osh, ulmu_scb->ul_rmem, sizeof(d11ulmu_rmem_t));
		ulmu_scb->ul_rmem = NULL;
	}
	WL_MUTX(("wl%d: %s: Remove sta "MACF" schpos %d state %x from ul ofdma list\n",
		wlc->pub->unit, __FUNCTION__,
		ETHER_TO_MACF(scb->ea), schpos, scb->state));
	ret = TRUE;

	wlc_ulmu_drv_del_usr(ulmu->wlc, scb, ulmu_scb);

#if defined WLTAF && defined ULMU_DRV
	if (wlc_taf_ul_enabled(wlc->taf_handle)) {
		WL_TAFF(ulmu->wlc, " called\n");
		wlc_taf_link_state(ulmu->wlc->taf_handle, scb, PRIO_8021D_BE, TAF_UL,
			TAF_LINKSTATE_NOT_ACTIVE);

		wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_UL, TAF_SCBSTATE_SOURCE_DISABLE);

		wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_PARAM(FALSE),
			TAF_SCBSTATE_MU_UL_OFDMA);
	}
#endif // endif
	return ret;
}

/* Determine if a STA is eligible to be admitted into ul ofdma list */
static bool
wlc_ulmu_scb_eligible(wlc_ulmu_info_t *ulmu, scb_t* scb)
{
	bool ret;
	wlc_info_t* wlc = ulmu->wlc;
	ret = (HE_ULMU_ENAB(wlc->pub) && scb && !SCB_INTERNAL(scb) &&
		SCB_HE_CAP(scb) && SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb) &&
		(SCB_IS_BRCM(scb) || (!ulmu->brcm_only)) &&
		(!ulmu->ban_nonbrcm_160sta || !wlc_he_is_nonbrcm_160sta(wlc->hei, scb)) &&
		(!WSEC_ENABLED(SCB_BSSCFG(scb)->wsec) ||
		(WSEC_ENABLED(SCB_BSSCFG(scb)->wsec) && SCB_AUTHORIZED(scb))) &&
		BSSCFG_AP(SCB_BSSCFG(scb)) &&
		!SCB_DWDS(scb) && wlc_he_get_ulmu_allow(wlc->hei, scb) &&
		(ulmu->always_admit != ULMU_ADMIT_TWT_ONLY ||
		wlc_twt_scb_is_trig_enab(wlc->twti, scb)) &&
		(wlc_he_get_omi_tx_nsts(wlc->hei, scb) <= MUMAX_NSTS_ALLOWED));
	if (ret) {
		WL_INFORM(("wl%d: %s: ul ofdma capable STA "MACF" rssi %d "
			"state 0x%x txnss %d ulmu_disabled %d\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea),
			wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb), scb->state,
			wlc_he_get_omi_tx_nsts(wlc->hei, scb),
			!wlc_he_get_ulmu_allow(wlc->hei, scb)));
	}
	return ret;
}

/* Function to try to add or delete a usr from ULOFDMA usr list
 * If a user is successfully added or deleted, return TRUE
 * otherwise FALSE
 */
static bool
wlc_scbulmu_set_ulofdma(wlc_ulmu_info_t *ulmu, scb_t* scb, bool ulofdma)
{
	scb_ulmu_t *ulmu_scb;
	wlc_info_t *wlc;
	bool ret = FALSE;

	wlc = ulmu->wlc;
	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb)) {
		return ret;
	}

	if ((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL) {
		WL_MUTX(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return ret;
	}

	if (wlc_ulmu_scb_is_ulofdma(ulmu, scb) == ulofdma) {
		WL_MUTX(("wl%d: %s: SCB is OFDMA STA "MACF", ulofdma %d\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea), ulofdma));
		/* no change */
		return ret;
	}

	if (ulofdma) {
		ulmu_scb->lmemidx = wlc_ratelinkmem_get_scb_link_index(wlc, scb);
		if (wlc_ulmu_scb_eligible(ulmu, scb)) {
			ret = wlc_ulmu_ulofdma_add_usr(ulmu, scb);
		} else if (SCB_HE_CAP(scb) && HE_ULMU_ENAB(wlc->pub) &&
			BSSCFG_AP(SCB_BSSCFG(scb))) {
			WL_MUTX(("wl%d: %s: Fail to enable ul ofdma STA "MACF" "
				"max clients %d num clients %d txnss %d ulmu_disabled %d\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea),
				wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA),
				ulmu->num_usrs, wlc_he_get_omi_tx_nsts(wlc->hei, scb),
				!wlc_he_get_ulmu_allow(wlc->hei, scb)));
			SCB_ULOFDMA_DISABLE(scb);
			ulmu_scb->schpos = ULMU_SCHPOS_INVLD;
			ulmu_scb->state = ULMU_SCB_EVCT;
		} else {
			/* pass */
		}
	} else {
		ret = wlc_ulmu_del_usr(ulmu, scb, TRUE);
		WL_MUTX(("wl%d: %s: Disable ul ofdma STA "MACF" state 0x%x ret:%d\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea), scb->state, ret));
	}
	return ret;
}

uint16
wlc_ulmu_scb_get_rmemidx(wlc_info_t *wlc, scb_t *scb)
{
	scb_ulmu_t* ulmu_scb;

	if (scb == NULL || wlc->ulmu == NULL ||
		((ulmu_scb = SCB_ULMU(wlc->ulmu, scb)) == NULL)) {
		WL_ERROR(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return D11_RATE_LINK_MEM_IDX_INVALID;
	}

	return ((ulmu_scb->schpos == ULMU_SCHPOS_INVLD) ?
		D11_RATE_LINK_MEM_IDX_INVALID :	ulmu_scb->rmemidx);
}

static void
wlc_ulmu_csreq_commit(wlc_ulmu_info_t* ulmu)
{
	wlc_info_t *wlc = ulmu->wlc;
	wlc_write_shm(wlc, M_HETB_CSTHRSH_LO(wlc), ulmu->csthr0);
	wlc_write_shm(wlc, M_HETB_CSTHRSH_HI(wlc), ulmu->csthr1);
}

static int
wlc_ulmu_utxd_reinit(wlc_ulmu_info_t* ulmu)
{
	int schpos, err, num_usrs = 0;
	wlc_info_t *wlc = ulmu->wlc;
	scb_t *scb;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	err = BCME_OK;

	if (!ulmu) {
		return err;
	}

	if (!HE_ULMU_ENAB(wlc->pub) || !ULMU_IS_UTXD(ulmu->mode)) {
		return err;
	}

	for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulmu->scb_list[schpos]) == NULL) {
			continue;
		}
		num_usrs++;
		wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRADD_ALL);
	}

	if ((err = wlc_ulmu_post_utxd(ulmu)) != BCME_OK) {
		err = BCME_ERROR;
		WL_ERROR(("wl%d: %s: fail to post user config utxd\n",
			wlc->pub->unit, __FUNCTION__));
	}
	if (num_usrs) {
		wlc_ulmu_prep_utxd(ulmu, NULL, ULMU_UTXD_GLBUPD);
		if ((err = wlc_ulmu_post_utxd(ulmu)) != BCME_OK) {
			err = BCME_ERROR;
			WL_ERROR(("wl%d: %s: fail to post global config utxd\n",
				wlc->pub->unit, __FUNCTION__));
		}
	}
	return err;
}

static void
wlc_ulmu_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data)
{
	wlc_ulmu_info_t *ulmu = (wlc_ulmu_info_t *) ctx;
	wlc_bsscfg_t *cfg = notif_data->cfg;

	if (!BSSCFG_AP(cfg)) {
		return;
	}

	if (notif_data->old_up && !cfg->up) {
		/* up -> down */
	} else if (notif_data->old_up && cfg->up) {
		/* reinit */
		wlc_ulmu_utxd_reinit(ulmu);
	}
}

/* Function to commit ulofdma scheduler changes
 * 1. populate ul ofdma trig txcfg block
 * 2. update the ratemem block for the newly added user
 */
static void
wlc_ulmu_cfg_commit(wlc_ulmu_info_t* ulmu)
{
	int schpos, idx;
	wlc_info_t *wlc = ulmu->wlc;
	d11ulo_trig_txcfg_t *txd = &ulmu->txd;
	scb_t *scb;
	uint16 lmem_idx;
	uint16 rmem_idx;
	uint16 *ptr;
	uint offset;
	scb_ulmu_t* ulmu_scb;
	uint16 max_ulofdma_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, ULOFDMA);

	BCM_REFERENCE(ptr);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(ulmu_scb);

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: %s: Fail to fill up ul ofdma txd. ratelinkmem_enab %x\n",
			wlc->pub->unit, __FUNCTION__, RATELINKMEM_ENAB(wlc->pub)));
		return;
	}

	for (schpos = 0, idx = 0; schpos < max_ulofdma_usrs; schpos++) {
		if ((scb = ulmu->scb_list[schpos]) == NULL) {
			continue;
		}
		ulmu_scb = SCB_ULMU(ulmu, scb);
		lmem_idx = ulmu_scb->lmemidx;
		rmem_idx = wlc_ulmu_scb_get_rmemidx(wlc, scb);
		if ((lmem_idx == D11_RATE_LINK_MEM_IDX_INVALID) ||
			(rmem_idx == D11_RATE_LINK_MEM_IDX_INVALID)) {
			WL_ERROR(("wl%d: %s: Fail to fill up ul ofdma txd. schpos %d addr "MACF"\n",
				wlc->pub->unit, __FUNCTION__, schpos,
				ETHER_TO_MACF(scb->ea)));
			ASSERT(0);
			return;
		}
		txd->rlmem[idx] = ((rmem_idx << 8) | lmem_idx);
		txd->ucfg[idx] = ulmu_scb->ucfg;
		idx++;
	}

	ASSERT(idx == ulmu->num_usrs);

	for (; idx < D11_ULOFDMA_MAX_NUSERS; idx++) {
		txd->rlmem[idx] = 0;
	}

	/* Don't enable UL OFDMA if the number of users is less than minimum */
	if (ulmu->num_usrs < ulmu->min_ulofdma_usrs) {
		txd->nvld = 0;
	} else {
		txd->nvld = ulmu->num_usrs;
	}

	/* set init bit */
	txd->nvld |= ((ulmu->num_usrs) && ulmu->is_start) ? (1 << 15) : 0;

	/* set min users to start ul ofdma */
	txd->minn = ulmu->min_ulofdma_usrs;

#if defined(WL_PSMX)
	/* suspend psmx */
	wlc_bmac_suspend_macx_and_wait(wlc->hw);

	if (ulmu->mode == ULMU_LEGACY_MODE) {
		/* update rate mem for the usr got changed */
		for (schpos = 0; schpos < max_ulofdma_usrs; schpos++) {
			if ((scb = ulmu->scb_list[schpos]) != NULL) {
				ulmu_scb = SCB_ULMU(ulmu, scb);
				if (!ulmu_scb->rmem_upd) {
					continue;
				}
				wlc_ratelinkmem_write_rucfg(wlc,
					(uint8*)ulmu_scb->ul_rmem,
					sizeof(d11ulmu_rmem_t),
					ulmu_scb->rmemidx);
				ulmu_scb->rmem_upd = FALSE;
			}
		}

		/* copy the info to MX_TRIG_TXCFG block */
		for (offset = MX_TRIG_TXCFG(wlc), ptr = (uint16 *) &ulmu->txd;
			offset < MX_TRIG_TXCFG(wlc) + sizeof(d11ulo_trig_txcfg_t);
			offset += 2, ptr++) {
			wlc_write_shmx(wlc, offset, *ptr);
		}
	} else {
		/* selectively copy the info to MX_TRIG_TXCFG block */
		for (offset = MX_TRIG_TXCFG(wlc), ptr = (uint16 *) &ulmu->txd;
			offset <= MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, aggnum);
			offset += 2, ptr++) {
			wlc_write_shmx(wlc, offset, *ptr);
		}
		for (offset = MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, ucfg),
			ptr = (uint16 *) ((uint8 *) &ulmu->txd +
				OFFSETOF(d11ulo_trig_txcfg_t, ucfg));
			offset < MX_TRIG_TXCFG(wlc) + sizeof(d11ulo_trig_txcfg_t);
			offset += 2, ptr++) {
			wlc_write_shmx(wlc, offset, *ptr);
		}
		offset = MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, mctl0);
		ptr = (uint16 *) ((uint8 *)&ulmu->txd+OFFSETOF(d11ulo_trig_txcfg_t, mctl0));
		wlc_write_shmx(wlc, offset, *ptr);

		offset = MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, chanspec);
		ptr = (uint16 *) ((uint8 *)&ulmu->txd+OFFSETOF(d11ulo_trig_txcfg_t, chanspec));
		wlc_write_shmx(wlc, offset, *ptr);

		offset = MX_TRIG_TXCFG(wlc) + OFFSETOF(d11ulo_trig_txcfg_t, minn);
		ptr = (uint16 *) ((uint8 *)&ulmu->txd+OFFSETOF(d11ulo_trig_txcfg_t, minn));
		wlc_write_shmx(wlc, offset, *ptr);

		wlc_write_shmx(wlc, MX_ULO_QNULLTHRSH(wlc), ulmu->qnull_thrsh);

		wlc_write_shmx(wlc, MX_ULC_NUM(wlc), ulmu->ulc_sz);
	}

	/* write trigger txlmt value */
	wlc_write_shmx(wlc, MX_TRIG_TXLMT(wlc), ulmu->txlmt);

	wlc_bmac_enable_macx(wlc->hw);
#endif /* defined(WL_PSMX) */
}

/**
 * admit / evict a ul-ofdma user
 *
 * Called from admission control component / omi transition
 *
 * @param wlc		handle to wlc_info context
 * @param scb		pointer to scb
 * @param admit		true to admit, false to evict
 * @return		true if successful, false otherwise
 */
bool
wlc_ulmu_admit_clients(wlc_info_t *wlc, scb_t *scb, bool admit)
{
	wlc_ulmu_info_t *ulmu = wlc->ulmu;
	bool ret = FALSE;

	if (!HE_ULMU_ENAB(wlc->pub)) {
		WL_MUTX(("wl%d: %s: UL OFDMA feature is not enabled\n",
			wlc->pub->unit, __FUNCTION__));
		return ret;
	}

	if ((ret = wlc_scbulmu_set_ulofdma(ulmu, scb, admit)) == TRUE) {
		/* if a user has been added or deleted, update the scheduler block */
		if (ULMU_IS_UTXD(ulmu->mode)) {
			wlc_ulmu_prep_utxd(ulmu, NULL, ULMU_UTXD_GLBUPD);
			wlc_ulmu_post_utxd(ulmu);
		}
		wlc_ulmu_cfg_commit(ulmu);
	} else {
		/* no change on ul ofdma user list, pass */
	}
	return ret;
}

static bool
wlc_ulmu_scb_is_ulofdma(wlc_ulmu_info_t *ulmu, scb_t* scb)
{
	if (ulmu->policy == ULMU_POLICY_DISABLE ||
		SCB_INTERNAL(scb)) {
		return FALSE;
	}

	return (SCB_ULOFDMA(scb));
}

void
wlc_ulmu_ul_nss_upd(wlc_ulmu_info_t *ulmu, scb_t* scb, uint8 tx_nss)
{
	scb_ulmu_t* ulmu_scb;

	ulmu_scb = SCB_ULMU(ulmu, scb);

	ulmu_scb->rmem_upd = TRUE;
	D11_ULORMEM_RTCTL_SET_INIT(ulmu_scb->ul_rmem->rtctl, 0);
	D11_ULORMEM_RTCTL_SET_NSS(ulmu_scb->ul_rmem->rtctl, tx_nss);
	D11_ULOTXD_UCFG_SET_NSS(ulmu_scb->ucfg, tx_nss);

	if (ULMU_IS_UTXD(ulmu->mode)) {
		wlc_ulmu_prep_utxd(ulmu, scb, ULMU_UTXD_USRUPD_RATE);
		wlc_ulmu_post_utxd(ulmu);
	} else {
		wlc_ulmu_cfg_commit(ulmu);
	}
	return;
}

static void
wlc_ulmu_fill_utxd(wlc_ulmu_info_t *ulmu, d11ulmu_txd_t *utxd,
	scb_t *scb, uint16 cmd)
{
	wlc_info_t *wlc = ulmu->wlc;
	uint8 mcs, nss;
	scb_ulmu_t* ulmu_scb;
	int i;
	uint16 aggnum = 0, mlen = 0, ucfg, trssi;
	ratespec_t rspec;

	if (!RATELINKMEM_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: %s: Fail to fill up ul ofdma utxd. ratelinkmem_enab %x\n",
			wlc->pub->unit, __FUNCTION__, RATELINKMEM_ENAB(wlc->pub)));
		return;
	}

	if (!scb) {
		return;
	}

	ulmu_scb = SCB_ULMU(ulmu, scb);
	utxd->glmem = ulmu_scb->lmemidx;
	if (ULMU_FLAGS_AUTOULC_GET(ulmu->flags) == ULMU_ON) {
		utxd->urmem = (uint16) -1;
	} else {
		utxd->urmem = wlc_ulmu_scb_get_rmemidx(wlc, scb);
	}
	utxd->bufsize = ulmu_scb->bufsize;
	utxd->trigcnt = ulmu_scb->trigcnt;
	ucfg = ulmu_scb->ucfg;

	/* TODO: now just use hardcoded bw80 mcsmap */
	for (i = 0; i < ARRAYSIZE(utxd->mcsbmp); i++) {
		utxd->mcsbmp[i] = HE_MAX_MCS_TO_MCS_MAP(
			(((scb->rateset.he_bw80_tx_mcs_nss >> (i*2)) & 0x3)));
	}
	if ((rspec = wlc_scb_ratesel_get_ulrt_rspec(wlc->wrsi, scb, 0)) != ULMU_RSPEC_INVD) {
		uint8 omi_nss;
		omi_nss = wlc_he_get_omi_tx_nsts(wlc->hei, scb); /* 1-based NSS */
		mcs = RSPEC_HE_MCS(rspec);
		nss = RSPEC_HE_NSS(rspec);
		if (nss != omi_nss) {
			WL_MUTX(("wl%d: %s: txnss %d omi txnss %d mismatch\n",
				wlc->pub->unit, __FUNCTION__, nss, omi_nss));
			nss = omi_nss;
		}
	} else {
		mcs = HE_MAX_MCS_TO_INDEX(HE_MCS_MAP_TO_MAX_MCS(utxd->mcsbmp[0]));
		nss = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80_tx_mcs_nss); /* 1-based NSS */
		rspec = HE_RSPEC(mcs, nss);
	}
	trssi = wlc_scb_ratesel_get_ulrt_trssi(wlc->wrsi, scb);
	D11_ULOTXD_UCFG_SET_TRSSI(ucfg, trssi);

	D11_ULORMEM_RTCTL_SET_MCS(utxd->rtctl, mcs);
	D11_ULORMEM_RTCTL_SET_NSS(utxd->rtctl, nss == 0 ? 0 : nss-1);
	if (ulmu_scb->state == ULMU_SCB_EVCT) {
		aggnum = wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx);
		mlen = ULMU_MLEN_INIT;
	} else if (ulmu_scb->state == ULMU_SCB_ADMT || ulmu_scb->state == ULMU_SCB_INIT) {
		aggnum = MIN(ulmu_scb->aggnma + ULMU_AGGN_HEADROOM,
			wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx));
		mlen = ulmu_scb->mlenma;
	} else {
		ASSERT(0);
	}
	ulmu_scb->ucfg = ucfg;
	WL_MUTX(("wl%d: %s: lmem %d rspec 0x%x mcs %d nss %d aggn %d mlen %d bufsz %d"
		" ucfg 0x%x rssi %d state %d rtctl 0x%x\n",
		wlc->pub->unit, __FUNCTION__, utxd->glmem,
		rspec, mcs, nss, aggnum, mlen, utxd->bufsize, ucfg,
		wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb), ulmu_scb->state, utxd->rtctl));
	utxd->aggn = aggnum;
	utxd->mlen = mlen;
	utxd->ucfg = ucfg;
}

static uint32
wlc_ulmu_prep_utxd(wlc_ulmu_info_t *ulmu, scb_t *scb, uint16 cmd)
{
	wlc_info_t *wlc = ulmu->wlc;
	void *p;
	osl_t *osh;
	int buflen, body_len;
	d11ulmu_txd_t *utxd;
	uint32 ts;
	int pad_sz = 4;

	osh = wlc->osh;
	body_len = sizeof(d11ulmu_txd_t) + pad_sz;
	buflen = wlc->txhroff + body_len;
	p = PKTGET(osh, buflen, TRUE);
	if (!p) {
		WL_ERROR(("wl%d: %s: pktget error for len %d \n",
			wlc->pub->unit, __FUNCTION__, buflen));
		return 0;
	}
	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

	/* reserve tx headroom offset */
	PKTPULL(osh, p, wlc->txhroff);
	PKTSETLEN(osh, p, buflen - wlc->txhroff);
	memset(PKTDATA(osh, p), 0, pad_sz + sizeof(d11ulmu_txd_t));

	/* construct the utxd packet */
	utxd = (d11ulmu_txd_t*) ((uint8 *)PKTDATA(osh, p) + pad_sz);
	utxd->frameid = ((ulmu->frameid++ << D11_REV128_TXFID_SEQ_SHIFT) &
		D11_REV128_TXFID_SEQ_MASK) |
		((TX_FIFO_SU_OFFSET(wlc->pub->corerev) + ULMU_TRIG_FIFO)
		& D11_REV128_TXFID_FIFO_MASK);
	utxd->chanspec = wlc->home_chanspec;
	utxd->utxdcmd = cmd;
	ts = R_REG(wlc->osh, D11_TSFTimerLow(wlc));
	utxd->ts_l = ts;
	utxd->ts_h = (ts >> 16);
	utxd->trigci = (DOT11_HETB_2XLTF_1U6S_GI << D11_ULOTXD_TXCTL_CPF_SHIFT) |
		(DOT11_HETB_2XHELTF_NLTF << D11_ULOTXD_TXCTL_NLTF_SHIFT);
	utxd->mctl0 = ulmu->txd.mctl0;
	/* populate per user info */
	wlc_ulmu_fill_utxd(ulmu, utxd, scb, cmd);
	if (WL_MUTX_ON()) {
		prhex("utxd", PKTDATA(osh, p), 40);
	}
	spktenq(&ulmu->utxdq, p);

	return (utxd->frameid >> 7);
}

static int
wlc_ulmu_post_utxd(wlc_ulmu_info_t *ulmu)
{
	wlc_info_t *wlc = ulmu->wlc;
	void *p;
	int ret = BCME_OK;

	if (!HE_ULMU_ENAB(wlc->pub) || !ULMU_IS_UTXD(ulmu->mode)) {
		WL_ERROR(("wl%d: %s: ulmu_enab %d ulmu mode %d\n",
			wlc->pub->unit, __FUNCTION__, HE_ULMU_ENAB(wlc->pub), ulmu->mode));
		return BCME_ERROR;
	}

	if (wlc->down_override) {
		WL_ERROR(("wl%d: %s: disable utxdq transmit due to down_override set to TRUE\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	while ((p = spktdeq(&ulmu->utxdq))) {
		if ((ret = wlc_bmac_dma_txfast(wlc, ULMU_TRIG_FIFO, p, TRUE))
			!= BCME_OK) {
			PKTFREE(wlc->osh, p, TRUE);
			WL_ERROR(("wl%d: %s: fatal, toss frames !!!\n",
				wlc->pub->unit, __FUNCTION__));
			break;
		}
	}
	return ret;
}

int
wlc_ulmu_reclaim_utxd(wlc_info_t *wlc, tx_status_t *txs)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_ulmu_info_t *ulmu = wlc->ulmu;
	void *txp;
	int ncons, cnt = 0;

	if (!ULMU_IS_UTXD(ulmu->mode)) {
		return cnt;
	}

	if (txs) {
		ncons = (txs->status.raw_bits & TX_STATUS40_NCONS) >>
			TX_STATUS40_NCONS_SHIFT;
		if (ncons == 0) {
			WL_ERROR(("wl%d: %s: ncons %d raw_bits %x s1 %x\n",
				wlc->pub->unit, __FUNCTION__,
				ncons, txs->status.raw_bits, txs->status.s1));
			ASSERT("ncons == 0");
		}
	} else {
		/* reclaim all utxd in hwfifo */
		ncons = -1;
	}
	while (((cnt < ncons) || (ncons == -1)) && (txp = GETNEXTTXP(wlc, ULMU_TRIG_FIFO))) {
		d11ulmu_txd_t *utxd =
			(d11ulmu_txd_t*) ((uint8 *)PKTDATA(wlc->osh, txp) + ULSTS_UTXD_PAD);

		BCM_REFERENCE(utxd);
		WL_MUTX(("wl%d: %s: UL OFDMA freeing idx %d txp %p fid 0x%x\n",
			wlc->pub->unit, __FUNCTION__,  cnt, txp,
			(utxd->frameid >> ULSTS_FRAMEID_SHIFT)));
		PKTFREE(wlc_hw->osh, txp, TRUE);
		cnt++;
	}

	WL_MUTX(("wl%d: %s: UL OFDMA freed cnt %d expect ncons %d\n",
		wlc->pub->unit, __FUNCTION__, cnt, ncons));
	return cnt;
}

void
wlc_ulmu_fburst_set(wlc_ulmu_info_t *ulmu, bool enable)
{
	d11ulo_trig_txcfg_t *txd = &ulmu->txd;
	txd->mctl0 &= ~D11AC_TXC_MBURST;
	txd->mctl0 |= (enable ? D11AC_TXC_MBURST: 0);
}

void
wlc_ulmu_maxn_uplimit_set(wlc_ulmu_info_t *ulmu, uint16 val)
{
	int bw;
	if (!ulmu) {
		return;
	}

	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		ulmu->maxn[bw] = MIN(ulmu->maxn[bw], val);
	}
}

static int
wlc_ulmu_maxn_set(wlc_ulmu_info_t *ulmu)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = ulmu->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_ULOMAXN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), ulmu->maxn[bw]);
	}
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

void
wlc_ulmu_oper_state_upd(wlc_ulmu_info_t* ulmu, scb_t *scb, uint8 state)
{
	scb_ulmu_t* ulmu_scb;

	if (scb == NULL || ulmu == NULL ||
		((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL)) {
		WL_ERROR(("wl%d: %s: Fail to get ulmu scb cubby STA "MACF"\n",
			ulmu->wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return;
	}

	ulmu_scb->state = state;
	switch (state) {
		case ULMU_SCB_INIT:
			ulmu_scb->idle_cnt = 0;
			ulmu_scb->last_rx_pkts = 0;
			ulmu_scb->qnullonly_cnt = 0;
			break;

		case ULMU_SCB_ADMT:
		case ULMU_SCB_EVCT:
			/* do nothing for now */
			break;

		default:
			break;
	}
}

static void
wlc_ulmu_scb_reqbytes_eval(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	scb_ulmu_t* ulmu_scb;
	uint16 wm;

	if (scb == NULL || ulmu == NULL ||
		((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL)) {
		return;
	}

	BCM_REFERENCE(wm);
	ulmu_scb->tgs_recv_bytes = (ulmu_scb->mlenma >> ULMU_NF_AGGLEN) *
		(ulmu_scb->aggnma >> ULMU_NF_AGGN);

	wm = PERCENT(ulmu_scb->tgs_exprecv_bytes, ulmu_scb->tgs_recv_bytes);
	if (wm <= ULMU_SCB_LOWM ||
		ulmu_scb->qnullonly_cnt >= ULMU_SCB_QNCNT_THRSH ||
		ulmu_scb->fail_cnt >= ULMU_SCB_FLCNT_THRSH) {
		ulmu_scb->tgs_recv_bytes >>= 1;
		ulmu_scb->lowwm = TRUE;
	} else if (ulmu_scb->lowwm == TRUE &&
		wm >= ULMU_SCB_HIWM) {
		ulmu_scb->tgs_recv_bytes += ULMU_MLEN_INIT;
	} else {
		ulmu_scb->lowwm = FALSE;
	}

	ulmu_scb->acc_bufsz += ulmu_scb->tgs_recv_bytes;
	ulmu_scb->acc_bufsz = LIMIT_TO_MAX(ulmu_scb->acc_bufsz,
		ULMU_ACC_BUFSZ_MAX);
}

/* Get the predicted bytes that can be requested
 * Inputs:
 *   mu_info  - ulmu info pointer
 *   scb      - STA in question
 * Returns:
 *   non-zero value in bytes that can be requested to post utxd
 */
uint32
wlc_ulmu_scb_reqbytes_get(wlc_ulmu_info_t *ulmu, scb_t *scb)
{
	scb_ulmu_t* ulmu_scb;

	if (scb == NULL || ulmu == NULL ||
		((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL)) {
		return 0;
	}

	if (ulmu_scb->state == ULMU_SCB_INIT || !ulmu_scb->scb_stats->nupd) {
		return ulmu_scb->su_recv_bytes;
	}
	return ulmu_scb->acc_bufsz;
}

void
wlc_ulmu_scb_reqbytes_decr(wlc_ulmu_info_t *ulmu, scb_t *scb, uint32 cnt)
{
	scb_ulmu_t* ulmu_scb;

	if (scb == NULL || ulmu == NULL ||
		((ulmu_scb = SCB_ULMU(ulmu, scb)) == NULL)) {
		return;
	}

	if (ulmu_scb->state == ULMU_SCB_INIT || !ulmu_scb->scb_stats->nupd) {
		ulmu_scb->su_recv_bytes = (ulmu_scb->su_recv_bytes > cnt) ?
			(ulmu_scb->su_recv_bytes - cnt) : ULMU_ACC_BUFSZ_MIN;
		return;
	}
	ulmu_scb->acc_bufsz = (ulmu_scb->acc_bufsz > cnt) ?
		(ulmu_scb->acc_bufsz - cnt) : ULMU_ACC_BUFSZ_MIN;
}

/* ======== attach/detach ======== */

wlc_ulmu_info_t *
BCMATTACHFN(wlc_ulmu_attach)(wlc_info_t *wlc)
{
	wlc_ulmu_info_t *ulmu;
	scb_cubby_params_t cubby_params;

	/* allocate private module info */
	if ((ulmu = MALLOCZ(wlc->osh, sizeof(*ulmu))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	ulmu->wlc = wlc;

	/* HE is attached before this module, so can rely on HE_ENAB here */
	if (!HE_ENAB(wlc->pub)) {
		WL_INFORM(("wl%d: %s: MU scheduler disabled\n", wlc->pub->unit,	__FUNCTION__));
		return ulmu;
	}

	ulmu->policy = ULMU_POLICY_AUTO;
	/* Init ulmu sched */
	wlc_txcfg_ulofdma_maxn_init(wlc, ulmu->maxn);

	ulmu->mode = ULMU_UTXD_MODE;
	ULMU_FLAGS_USCHED_SET(ulmu->flags, ULMU_ON);
	ULMU_FLAGS_HBRNT_SET(ulmu->flags, ULMU_ON);
	ULMU_FLAGS_AUTOULC_SET(ulmu->flags, ULMU_ON);
	ULMU_FLAGS_DSCHED_SET(ulmu->flags, ULMU_OFF);

	ulmu->txd.macctl = ((0 << D11_ULOTXD_MACTL_MODE_NBIT) |
		(ulmu->mode << D11_ULOTXD_MACTL_UTXD_SHIFT) |
		(0 << D11_ULOTXD_MACTL_FIXDUR_NBIT) |
		(HE_TRIG_TYPE_BASIC_FRM << D11_ULOTXD_MACTL_PTYPE_SHIFT) |
		((1 << AC_BK) << D11_ULOTXD_MACTL_ACBMP_SHIFT) |
		(ULMU_MCTL_DURCFG_AVG << D11_ULOTXD_MACTL_DURCFG_SHIFT)); /* 0x20 */
	ulmu->txd.macctl1 = ((ULMU_FLAGS_HBRNT_GET(ulmu->flags) <<
		D11_ULOTXD_MACTL1_HBRNT_SHIFT) |
		(ULMU_FLAGS_USCHED_GET(ulmu->flags) << D11_ULOTXD_MACTL1_USCHED_SHIFT) |
		(ULMU_FLAGS_DSCHED_GET(ulmu->flags) << D11_ULOTXD_MACTL1_DSCHED_SHIFT));
	ulmu->txd.maxdur = ULMU_TXDUR_MAX;
	ulmu->txd.burst = 2;
	ulmu->txd.maxtw = 5;
	ulmu->txd.txcnt = (uint16) -1;
	ulmu->txd.interval = (uint16) -1; /* Turn on auto trigger interval scheme */
	ulmu->txd.minidle = 100; /* Init to 100us */
	ulmu->txd.rxlowat0 = 3;
	ulmu->txd.rxlowat1 = 30;
	wlc_ulmu_fburst_set(ulmu, WLC_HT_GET_FRAMEBURST(wlc->hti));
	ulmu->txd.txctl = (DOT11_HETB_2XLTF_1U6S_GI << D11_ULOTXD_TXCTL_CPF_SHIFT) |
		(DOT11_HETB_2XHELTF_NLTF << D11_ULOTXD_TXCTL_NLTF_SHIFT);
	ulmu->txlmt = 0x206;
	ulmu->min_ulofdma_usrs = ULMU_USRCNT_MIN;
	ulmu->qnull_thrsh = ULMU_STOPTRIG_QNULL_THRSH;
	ulmu->csthr0 = 76;
	ulmu->csthr1 = 418;
	ulmu->ulc_sz = ULMU_MAX_ULC_SZ;
	ulmu->rx_pktcnt_thrsh = ULMU_SCB_RX_PKTCNT_THRSH;
	ulmu->idle_prd = ULMU_SCB_IDLEPRD;
	ulmu->min_rssi = ULMU_SCB_MINRSSI;
	ulmu->always_admit = ULMU_ADMIT_AUTO;

	wlc_ulmu_drv_ulmu_attach(ulmu);

	spktq_init(&ulmu->utxdq, ULMU_UTXDQ_PKTS_MAX);

	ulmu->is_start = TRUE;
	ulmu->brcm_only = FALSE;
	ulmu->ban_nonbrcm_160sta = FALSE;

	wlc->ulmu = ulmu;
	D11_ULOTXD_UCFG_SET_MCSNSS(ulmu->g_ucfg, 0x17); /* auto rate by default */
	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, ulmu_iovars, "ulmu", ulmu,
		wlc_ulmu_doiovar, wlc_ulmu_watchdog, wlc_ulmu_wlc_init, wlc_ulmu_wlc_deinit)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve some space in scb for private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = ulmu;
	cubby_params.fn_init = wlc_ulmu_scb_init;
	cubby_params.fn_deinit = wlc_ulmu_scb_deinit;
	cubby_params.fn_secsz = wlc_ulmu_scb_secsz;
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	cubby_params.fn_dump = wlc_ulmu_scb_dump;
#endif // endif

	if ((ulmu->scbh =
		wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_ulmu_t *), &cubby_params)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	if (wlc_scb_state_upd_register(wlc, wlc_ulmu_scb_state_upd, ulmu) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback wlc_ulmu_scb_state_upd\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if ((wlc_bsscfg_state_upd_register(wlc, wlc_ulmu_bsscfg_state_upd, ulmu)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
			goto fail;
	}

	/* debug dump */
#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
	wlc_dump_add_fns(wlc->pub, "umsched", wlc_ulmu_dump, wlc_ulmu_dump_clr, ulmu);
#endif // endif

	return ulmu;

fail:
	wlc_ulmu_detach(ulmu);
	return NULL;
}

void
BCMATTACHFN(wlc_ulmu_detach)(wlc_ulmu_info_t *ulmu)
{
	wlc_info_t *wlc;

	if (ulmu == NULL) {
		return;
	}

	wlc = ulmu->wlc;

	wlc_scb_state_upd_unregister(wlc, wlc_ulmu_scb_state_upd, ulmu);

	wlc_ulmu_drv_ulmu_detach(ulmu);

	wlc_bsscfg_state_upd_unregister(wlc, wlc_ulmu_bsscfg_state_upd, ulmu);

	wlc->ulmu = NULL;

	wlc_module_unregister(wlc->pub, "ulmu", ulmu);

	if (ulmu != NULL) {
		void *p;
		while ((p = spktdeq(&ulmu->utxdq))) {
			PKTFREE(wlc->osh, p, TRUE);
		}
		spktqdeinit(&ulmu->utxdq);
		MFREE(wlc->osh, ulmu, sizeof(wlc_ulmu_info_t));
	}

	ulmu = NULL;
	MFREE(wlc->osh, ulmu, sizeof(*ulmu));
}

int wlc_ulmu_sw_trig_enable(wlc_info_t *wlc, uint32 enable)
{
	wlc_ulmu_info_t *ulmu = wlc->ulmu;
	d11ulo_trig_txcfg_t *txd = &ulmu->txd;

	if (wlc->pub->up) {
		return BCME_NOTDOWN;
	}

	if (enable == 1) {
		/* check if UL OFDMA is enabled */
		if (!HE_ULMU_ENAB(wlc->pub)) {
			printf("wl%d: %s: HE UL-OFDMA not enabled\n",
				wlc->pub->unit, __FUNCTION__);
			return BCME_UNSUPPORTED;
		}

		/* umsched usched 0 */
		ULMU_FLAGS_USCHED_SET(ulmu->flags, ULMU_OFF);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_USCHED_MASK;
		txd->macctl1 |= (ULMU_OFF << D11_ULOTXD_MACTL1_USCHED_SHIFT);

		/* umsched dsched 1 */
		ULMU_FLAGS_DSCHED_SET(ulmu->flags, ULMU_ON);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_DSCHED_MASK;
		txd->macctl1 |= (ULMU_ON << D11_ULOTXD_MACTL1_DSCHED_SHIFT);

		/* umsched hibernate 0 */
		ULMU_FLAGS_HBRNT_SET(ulmu->flags, ULMU_OFF);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_HBRNT_MASK;
		txd->macctl1 |= (ULMU_OFF << D11_ULOTXD_MACTL1_HBRNT_SHIFT);
	}
	else if (enable == 0) {

		/* umsched usched 1 */
		ULMU_FLAGS_USCHED_SET(ulmu->flags, ULMU_ON);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_USCHED_MASK;
		txd->macctl1 |= (ULMU_ON << D11_ULOTXD_MACTL1_USCHED_SHIFT);

		/* umsched dsched 0 */
		ULMU_FLAGS_DSCHED_SET(ulmu->flags, ULMU_OFF);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_DSCHED_MASK;
		txd->macctl1 |= (ULMU_OFF << D11_ULOTXD_MACTL1_DSCHED_SHIFT);

		/* umsched hibernate 1 */
		ULMU_FLAGS_HBRNT_SET(ulmu->flags, ULMU_ON);
		txd->macctl1 &= ~D11_ULOTXD_MACTL1_HBRNT_MASK;
		txd->macctl1 |= (ULMU_ON << D11_ULOTXD_MACTL1_HBRNT_SHIFT);
	}
	else {
		return BCME_RANGE;
	}
	return BCME_OK;
}

#ifdef WLTAF
#ifdef TAF_INF_UL_TRIGGER /* define it for infinite triggering */
static uint32 BCMFASTPATH
wlc_ulmu_rxbyte_cnt(scb_ulmu_t * ulmu_scb)
{
	/* rx monitor byte cnt API, return some large value for now */
	return 0xA00000;
}
#endif /* TAF_INF_UL_TRIGGER */

void * BCMFASTPATH wlc_ulmu_taf_get_scb_info(void *ulmuh, struct scb* scb)
{
	wlc_ulmu_info_t *ulmu = (wlc_ulmu_info_t *)ulmuh;

	return (ulmu && scb) ? (void*)*SCB_ULMU_CUBBY(ulmu, scb) : NULL;
}

void * BCMFASTPATH wlc_ulmu_taf_get_scb_tid_info(void *scb_h, int tid)
{
	return 0;
}

uint16 BCMFASTPATH wlc_ulmu_taf_get_scb_pktlen(void *scbh, void *tidh)
{
	scb_ulmu_t * ulmu_scb = (scb_ulmu_t *)scbh;
	uint32 byte_cnt;

#ifdef TAF_INF_UL_TRIGGER
	byte_cnt = wlc_ulmu_rxbyte_cnt(ulmu_scb);
#else
	byte_cnt = wlc_ulmu_scb_reqbytes_get(ulmu_scb->ulmu, ulmu_scb->scb_bl);
#endif /* TAF_INF_UL_TRIGGER */

	return (ulmu_scb) ? (byte_cnt / TAF_PKT_SIZE_DEFAULT): 0;
}

static int wlc_ulmu_trigger_request_callback(wlc_info_t * wlc, struct scb *scb,
	packet_trigger_info_t *ti, void *arg, uint32 status_code, uint32 bytes_consumed,
	uint32 avg_pktsz, uint32 duration)
{
	wlc_ulmu_info_t *ulmu = wlc->ulmu;
	scb_ulmu_t *ulmu_scb;
	wlc_ulpkttag_t *tag = (wlc_ulpkttag_t *)arg;

	BCM_REFERENCE(tag);
	TAF_ASSERT(tag->index <= TAF_MAX_PKT_INDEX);

	/*
	 * Return if we are not set up to handle the tx status for this scb.
	 */
	if (!scb) { /* Feature is not enabled */
		WL_ERROR(("wl%d: %s: null scb \n",
				WLCWLUNIT(wlc), __FUNCTION__));
		TAF_ASSERT(0);
		return BCME_ERROR;
	}

	if (!SCB_ULOFDMA(scb)) { /* Feature is not enabled */
		WL_ERROR(("wl%d: %s: ul ofdma feature not enabled\n",
				WLCWLUNIT(wlc), __FUNCTION__));
	}

	ulmu_scb = SCB_ULMU(ulmu, scb);
	if (!ulmu_scb) {
		WL_ERROR(("wl%d: %s: null ulmu_scb\n",
				WLCWLUNIT(wlc), __FUNCTION__));
		TAF_ASSERT(0);
		return BCME_ERROR;
	}

	if (ulmu_scb->state != ULMU_SCB_ADMT) { /* not admitted */
		WL_ERROR(("wl%d: %s: not admitted\n",
				WLCWLUNIT(wlc), __FUNCTION__));
	}

	WL_TAFF(wlc, MACF" index %d status %d bytes consumed %d avg_pktsz %d dur %d"
		" units %d flag %d\n",
		ETHER_TO_MACF(scb->ea), tag->index, status_code, bytes_consumed, avg_pktsz,
		duration, tag->units, tag->flags3);

	if ((status_code == ULMU_STATUS_INPROGRESS) ||
		(status_code == ULMU_STATUS_THRESHOLD)) {
		/* wait for COMPLETE */
		return BCME_OK;
	}

	ulmu_scb->bytes_recvd += bytes_consumed;

	wlc_taf_txpkt_status(wlc->taf_handle, scb, PRIO_8021D_BE, arg,
		TAF_TXPKT_STATUS_TRIGGER_COMPLETE);

	wlc_taf_schedule(wlc->taf_handle, PRIO_8021D_BE, scb, FALSE);

	ULTRIGPENDDEC(wlc);

	MFREE(wlc->osh, arg, sizeof(wlc_ulpkttag_t));

	return BCME_OK;
}

static void BCMFASTPATH
wlc_ulmu_send_trigger(wlc_ulmu_info_t* ulmu, struct scb *scb, int bytes, wlc_ulpkttag_t *tag)
{
	wlc_ulmu_trigger_info_t trig_info;

	BCM_REFERENCE(trig_info);

	if (tag->index > TAF_MAX_PKT_INDEX) {
		WL_ERROR(("wl%d: %s: SCB:"MACF" index %d units %d flag %d\n",
			WLCWLUNIT(ulmu->wlc), __FUNCTION__,
			ETHER_TO_MACF(scb->ea), tag->index, tag->units,
			tag->flags3));
		TAF_ASSERT(tag->index <= TAF_MAX_PKT_INDEX);
	}

	WL_TAFF(ulmu->wlc, " SCB:"MACF" index %d units %d flag %d\n",
		ETHER_TO_MACF(scb->ea), tag->index, tag->units, tag->flags3);

	if (bytes) {
		TAF_ASSERT(tag != NULL);

		trig_info.trigger_type.packet_trigger.trigger_bytes = bytes;
		trig_info.trigger_type.packet_trigger.callback_function =
			wlc_ulmu_trigger_request_callback;
		trig_info.trigger_type.packet_trigger.callback_parameter =
			(void *)tag;
		trig_info.trigger_type.packet_trigger.callback_reporting_threshold =
			ULMU_CB_THRESHOLD;
		trig_info.trigger_type.packet_trigger.multi_callback = FALSE;
		trig_info.trigger_type.packet_trigger.qos_null_threshold =
			ULMU_QOSNULL_LIMIT;
		trig_info.trigger_type.packet_trigger.failed_request_threshold =
			ULMU_TIMEOUT_LIMIT;
#ifdef ULMU_DRV
		wlc_ulmu_drv_trigger_request(ulmu->wlc, scb, ULMU_PACKET_TRIGGER, &trig_info);
#endif // endif

		ULTRIGPENDINC(ulmu->wlc);

		wlc_ulmu_scb_reqbytes_decr(ulmu, scb, bytes);
	}
}

bool wlc_ulmu_taf_release(void* ulmuh, void* scbh, void* tidh, bool force,
	taf_scheduler_public_t* taf)
{
	wlc_ulmu_info_t *ulmu = (wlc_ulmu_info_t *)ulmuh;
	int nreleased = 0;
	scb_ulmu_t * ulmu_scb = (scb_ulmu_t *)scbh;
	bool finished = FALSE;
	struct scb * scb;
	uint32 taf_cum_units = 0;
	uint32 rx_byte_cnt = 0;
	wlc_ulpkttag_t *tag = NULL;

	if (!ulmu_scb) {
		WL_ERROR(("wl%d: %s: no cubby!\n",  WLCWLUNIT(ulmu->wlc), __FUNCTION__));
		return FALSE;
	}

	if (taf->how != TAF_RELEASE_LIKE_IAS) {
		WL_TAFF(ulmu->wlc, "release != TAF_RELEASE_LIKE_IAS\n");
		ASSERT(0);
		taf->complete = TAF_REL_COMPLETE_ERR;
		return FALSE;
	}

	scb = ulmu_scb->scb_bl;
	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", WLCWLUNIT(ulmu->wlc), __FUNCTION__));
		TAF_ASSERT(0);
	}

	if (!SCB_ULOFDMA(scb) || ulmu_scb->state != ULMU_SCB_ADMT) {
		WL_ERROR(("wl%d: %s: SCB:"MACF" not admitted for uplink, scb state %d\n",
				WLCWLUNIT(ulmu->wlc), __FUNCTION__, ETHER_TO_MACF(scb->ea),
				ulmu_scb->state));
		return FALSE;
	}

	if (taf->how == TAF_RELEASE_LIKE_IAS && taf->ias.is_ps_mode) {
		/* TODO:  terminate the request for now */
		return FALSE;
	}

#ifdef TAF_INF_UL_TRIGGER
	/* infinite trigger API */
	rx_byte_cnt = wlc_ulmu_rxbyte_cnt(ulmu_scb);
#else
	/* Rx monitor API */
	rx_byte_cnt = wlc_ulmu_scb_reqbytes_get(ulmu, scb);
#endif /* TAF_INF_UL_TRIGGER */

	if (rx_byte_cnt == 0)
		return FALSE;

	while (!finished) {
		uint32 qlen;
		qlen = rx_byte_cnt > TAF_PKT_SIZE_DEFAULT ?
			TAF_PKT_SIZE_DEFAULT : rx_byte_cnt;
		if (!qlen) {
			finished = TRUE;
			if (taf->how == TAF_RELEASE_LIKE_IAS) {
				taf->ias.was_emptied = TRUE;
			}
			if (taf->how == TAF_RELEASE_LIKE_DEFAULT) {
				taf->def.was_emptied = TRUE;
			}
			taf->complete = TAF_REL_COMPLETE_EMPTIED;
			WL_TAFF(ulmu->wlc, MACF" 0 qlen\n", ETHER_TO_MACF(scb->ea));
			break;
		}
		rx_byte_cnt -= qlen;
		++nreleased;

		if (taf->how == TAF_RELEASE_LIKE_IAS) {

			if (!taf->ias.is_ps_mode)  {
				uint32 pktbytes = qlen;
				uint32 taf_pkt_time_units =
					TAF_PKTBYTES_TO_UNITS((uint16)pktbytes,
					taf->ias.pkt_rate, taf->ias.byte_rate);

				if (taf_pkt_time_units == 0) {
					taf_pkt_time_units = 1;
				}

				taf->ias.actual.released_bytes += (uint16)pktbytes;

				taf_cum_units += taf_pkt_time_units;

				if ((taf_cum_units +
					taf->ias.total.released_units) >=
					taf->ias.time_limit_units) {

					WL_TAFF(ulmu->wlc, "time_limit_units reached\n");
					finished = TRUE;

					taf->complete = TAF_REL_COMPLETE_TIME_LIMIT;
				}
				if ((taf->ias.released_units_limit > 0) &&
					((taf_cum_units + taf->ias.actual.released_units) >=
					taf->ias.released_units_limit)) {

					WL_TAFF(ulmu->wlc, "released_units_limit reached\n");
					finished = TRUE;

					taf->complete = TAF_REL_COMPLETE_REL_LIMIT;

				}
			} else {
				/* terminate this release ?? Add support for PS mode later */
				TAF_ASSERT(0);
				//taf_pkt_tag = TAF_PKTTAG_PS;
			}
			taf->ias.actual.release++;
		}
		else if (taf->how == TAF_RELEASE_LIKE_DEFAULT) {
			taf->def.actual.release++;
		}
	}

	if (nreleased) {

		taf->ias.actual.released_units += taf_cum_units;

		WL_TAFF(ulmu->wlc,
			MACF" rate: pkt %d byte %d release units: actual %d"
			" total %d limit %d\n",
			ETHER_TO_MACF(scb->ea), taf->ias.pkt_rate, taf->ias.byte_rate,
			taf->ias.actual.released_units, taf->ias.total.released_units,
			taf->ias.released_units_limit);

		if ((tag = MALLOCZ(ulmu->wlc->osh, sizeof(wlc_ulpkttag_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				WLCWLUNIT(ulmu->wlc), __FUNCTION__, MALLOCED(ulmu->wlc->osh)));
			TAF_ASSERT(0);
		}
		tag->index = taf->ias.index;
		tag->units = taf_cum_units;
		tag->flags3 = WLF3_TAF_TAGGED;

		ulmu_scb->bytes_recvd = 0;
		ulmu_scb->trig_bytes = nreleased * TAF_PKT_SIZE_DEFAULT;
		ulmu_scb->trig_units = taf_cum_units;

		wlc_ulmu_send_trigger(ulmu, scb, ulmu_scb->trig_bytes, tag);

		WL_TAFF(ulmu->wlc, MACF" released %u bytes %d index %d"
			" units %d dur %d pend %d\n",
			ETHER_TO_MACF(scb->ea), nreleased, ulmu_scb->trig_bytes,
			taf->ias.index, taf_cum_units, TAF_UNITS_TO_MICROSEC(taf_cum_units),
			ULTRIGPENDTOT(ulmu->wlc));
	}
	else {
		WL_TAFF(ulmu->wlc, MACF" nreleased %d\n",
			ETHER_TO_MACF(scb->ea), nreleased);
	}
	return nreleased > 0;
}
#endif /* WLTAF */

/* set ul ofdma admission params for TWT setting */
void
wlc_ulmu_twt_params(wlc_ulmu_info_t *ulmu, bool on)
{

	WL_MUTX(("wl%d: %s: on is %d\n",
		ulmu->wlc->pub->unit, __FUNCTION__, on));
	if (on) {
		/* For TWT
		 * - admit even 1 user as ul ofdma
		 * - set the trigger interval to 1ms
		 * - guarantee to tx at least 1 trigger per interval
		 * - tx max 2 triggers per interval
		 */
		ulmu->min_ulofdma_usrs = 1;
	} else {
		/* there is no trig based twt user so restore default settings */
		ulmu->min_ulofdma_usrs = ULMU_USRCNT_MIN;
	}
	if (ulmu->wlc->pub->up) {
		wlc_ulmu_cfg_commit(ulmu);
	}
}

#endif /* WL_ULMU */
