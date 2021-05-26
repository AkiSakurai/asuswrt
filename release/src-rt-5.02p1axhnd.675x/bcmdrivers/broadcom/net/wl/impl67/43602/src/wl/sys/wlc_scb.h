/*
 * Common interface to the 802.11 Station Control Block (scb) structure
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
 * $Id: wlc_scb.h 778869 2019-09-12 05:53:46Z $
 */

#ifndef _wlc_scb_h_
#define _wlc_scb_h_

#include <proto/802.1d.h>
#ifdef	BCMCCX
#include <proto/802.11_ccx.h>
#endif	/* BCMCCX */

#ifdef PSTA
/* In Proxy STA mode we support up to max 50 hosts and repeater itself */
#define SCB_BSSCFG_BITSIZE ROUNDUP(51, NBBY)/NBBY
#else /* PSTA */
#define SCB_BSSCFG_BITSIZE ROUNDUP(32, NBBY)/NBBY
#if (WLC_MAXBSSCFG > 32)
#error "auth_bsscfg cannot handle WLC_MAXBSSCFG"
#endif // endif
#endif /* PSTA */

/** Information node for scb packet transmit path */
struct tx_path_node {
	txmod_tx_fn_t next_tx_fn;		/* Next function to be executed */
	void *next_handle;
	uint8 next_fid;			/* Next fid in transmit path */
	bool configured;		/* Whether this feature is configured */
};

#ifdef WLCNTSCB
typedef struct wlc_scb_stats {
	uint32 tx_pkts;			/* # of packets transmitted (ucast) */
	uint32 tx_failures;		/* # of packets failed */
	uint32 rx_ucast_pkts;		/* # of unicast packets received */
	uint32 rx_mcast_pkts;		/* # of multicast packets received */
	ratespec_t tx_rate;		/* Rate of last successful tx frame */
	ratespec_t rx_rate;		/* Rate of last successful rx frame */
	uint32 rx_decrypt_succeeds;	/* # of packet decrypted successfully */
	uint32 rx_decrypt_failures;	/* # of packet decrypted unsuccessfully */
	uint32 tx_mcast_pkts;		/* # of mcast pkts txed */
	uint64 tx_ucast_bytes;		/* data bytes txed (ucast) */
	uint64 tx_mcast_bytes;		/* data bytes txed (mcast) */
	uint64 rx_ucast_bytes;		/* data bytes recvd ucast */
	uint64 rx_mcast_bytes;		/* data bytes recvd mcast */
} wlc_scb_stats_t;

typedef struct wlc_scb_ext_stats {
	uint32 tx_rate_fallback;	/* last used lowest fallback TX rate */
	uint32 rx_pkts_retried;		/* # rx with retry bit set */

	uint32 tx_pkts_total;
	uint32 tx_pkts_retries;
	uint32 tx_pkts_retry_exhausted;
	uint32 tx_pkts_fw_total;
	uint32 tx_pkts_fw_retries;
	uint32 tx_pkts_fw_retry_exhausted;
} wlc_scb_ext_stats_t;

#endif /* WLCNTSCB */

typedef struct wlc_rate_histo {
	uint		vitxrspecidx;	/* Index into the video TX rate array */
	ratespec_t	vitxrspec[NVITXRATE][2];	/* History of Video MPDU's txrate */
	uint32		vitxrspectime[NVITXRATE][2];	/* Timestamp for each Video Tx */
	uint32		txrspectime[NTXRATE][2];	/* Timestamp for each Tx */
	uint		txrspecidx; /* Index into the TX rate array */
	ratespec_t	txrspec[NTXRATE][2];	/* History of MPDU's txrate */
	uint		rxrspecidx; /* Index into the Rx rate array */
	ratespec_t	rxrspec[NTXRATE];	/* History of MPDU's rxrate */
} wlc_rate_histo_t;

#if defined(BCMPCIEDEV)
/* Max number of samples in running buffer:
 * Keep max samples number a power of two, thus
 * all the division would be a single-cycle
 * bit-shifting.
 */
#define RAVG_EXP_PKT 2
#define RAVG_EXP_WGT 2

typedef struct _ravg {
	uint32 sum;
	uint8 exp;
} wlc_ravg_info_t;

#define RAVG_EXP(_obj_) ((_obj_)->exp)
#define RAVG_SUM(_obj_) ((_obj_)->sum)
#define RAVG_AVG(_obj_) ((_obj_)->sum >> (_obj_)->exp)

/* Basic running average algorithm:
 * Keep a running buffer of the last N values, and a running SUM of all the
 * values in the buffer. Each time a new sample comes in, subtract the oldest
 * value in the buffer from SUM, replace it with the new sample, add the new
 * sample to SUM, and output SUM/N.
 */
#define RAVG_ADD(_obj_, _sample_) \
{ \
	uint8 _exp_ = RAVG_EXP((_obj_)); \
	uint32 _sum_ = RAVG_SUM((_obj_)); \
	RAVG_SUM((_obj_)) = ((_sum_ << _exp_) - _sum_) >> _exp_; \
	RAVG_SUM((_obj_)) += (_sample_); \
}

/* Initializing running buffer with value (_sample_) */
#define RAVG_INIT(_obj_, _sample_, _exp_) \
{ \
	RAVG_SUM((_obj_)) = (_sample_) << (_exp_); \
	RAVG_EXP((_obj_)) = (_exp_); \
}

#define PRIOMAP(_wlc_) ((_wlc_)->pciedev_prio_map)

#if defined(FLOW_PRIO_MAP_AC)
/* XXX: FLOW_PRIO_MAP_AC would be defined for PCIE FD router builds.
 * XXX: Router supports only 4 flow-rings per STA, therefore average
 * XXX: buffers would be 4.
 */
#define FLOWRING_PER_SCB_MAX AC_COUNT
#define RAVG_PRIO2FLR(_map_, _prio_) WME_PRIO2AC((_prio_))
#else  /* !FLOW_PRIO_MAP_AC */
/* XXX: Supports maximum up to 8 flow-rings, therefore average
 * XXX: buffers would be maximum 8.
 */
#define FLOWRING_PER_SCB_MAX NUMPRIO
/* XXX: Following the currently used prio mapping
 * XXX: configuration scheme in pcie bus layer.
 */
#define RAVG_PRIO2FLR(_map_, _prio_) \
	((((_map_) == PCIEDEV_AC_PRIO_MAP) ? WME_PRIO2AC((_prio_)) : (_prio_)))
#endif  /* FLOW_PRIO_MAP_AC */
#endif /* BCMPCIEDEV */

/**
 * Information about a specific remote entity, and the relation between the local and that remote
 * entity. Station Control Block.
 */
struct scb {
	void *scb_priv;		/* internal scb data structure */
#ifdef MACOSX
	uint32 magic;
#endif // endif
	uint32	flags;		/* various bit flags as defined below */
	uint32	flags2;		/* various bit flags2 as defined below */
	wsec_key_t	*key;		/* per station WEP key */
	wlc_bsscfg_t	*bsscfg;	/* bsscfg to which this scb belongs */
	struct ether_addr ea;		/* station address, must be aligned */
	uint32	pktc_pps;		/* pps counter for activating pktc */
	uint8   auth_bsscfg[SCB_BSSCFG_BITSIZE]; /* authentication state w/ respect to bsscfg(s) */
	uint8	state; /* current state bitfield of auth/assoc process */
	bool		permanent;	/* scb should not be reclaimed */
	uint		used;		/* time of last use */
	uint32		assoctime;	/* time of association */
	uint		bandunit;	/* tha band it belongs to */
#if defined(IBSS_PEER_GROUP_KEY)
	wsec_key_t	*ibss_grp_keys[WSEC_MAX_DEFAULT_KEYS];	/* Group Keys for IBSS peer */
#endif // endif
#ifdef WL_SAE
	uint32	 WPA_auth;	/* WPA: authenticated key management */
#else
	uint16	 WPA_auth;	/* WPA: authenticated key management */
#endif /* WL_SAE */
	uint32	 wsec;	/* ucast security algo. should match key->algo. Needed before key is set */

	wlc_rateset_t	rateset;	/* operational rates for this remote station */

	void	*fragbuf[NUMPRIO];	/* defragmentation buffer per prio */
	uint	fragresid[NUMPRIO];	/* #bytes unused in frag buffer per prio */

	uint16	 seqctl[NUMPRIO];	/* seqctl of last received frame (for dups) */
	uint16	 seqctl_nonqos;		/* seqctl of last received frame (for dups) for
					 * non-QoS data and management
					 */
	uint16	 seqnum[NUMPRIO];	/* WME: driver maintained sw seqnum per priority */

	/* APSD configuration */
	struct {
		uint16		maxsplen;   /* Maximum Service Period Length from assoc req */
		ac_bitmap_t	ac_defl;    /* Bitmap of ACs enabled for APSD from assoc req */
		ac_bitmap_t	ac_trig;    /* Bitmap of ACs currently trigger-enabled */
		ac_bitmap_t	ac_delv;    /* Bitmap of ACs currently delivery-enabled */
	} apsd;

#ifdef AP
	uint16		aid;		/* association ID */
	uint8		*challenge;	/* pointer to shared key challenge info element */
	uint16		tbtt;		/* count of tbtt intervals since last ageing event */
	uint8		auth_alg;	/* 802.11 authentication mode */
	bool		PS;		/* remote STA in PS mode */
	bool            PS_pend;        /* Pending PS state */
	uint8           ps_pretend;     /* AP pretending STA is in PS mode */
	uint		grace_attempts;	/* Additional attempts made beyond scb_timeout
					 * before scb is removed
					 */
#endif /* AP */
	uint8		*wpaie;		/* WPA IE */
	uint		wpaie_len;	/* Length of wpaie */
	wlc_if_t	*wds;		/* per-port WDS cookie */
	int		*rssi_window;	/* rssi samples */
	int		rssi_index;
	int		rssi_enabled;	/* enable rssi collection */
	uint16		cap;		/* sta's advertized capability field */
	uint16		listen;		/* minimum # bcn's to buffer PS traffic */

	uint16		amsdu_ht_mtu_pref;	/* preferred HT AMSDU mtu in bytes */

#ifdef WL11N
	bool		ht_mimops_enabled;	/* cached state: a mimo ps mode is enabled */
	bool		ht_mimops_rtsmode;	/* cached state: TRUE=RTS mimo, FALSE=no mimo */
	uint16		ht_capabilities;	/* current advertised capability set */
	uint8		ht_ampdu_params;	/* current adverised AMPDU config */
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(DONGLEBUILD)
	uint8		rclen;			/* regulatory class length */
	uint8		rclist[MAXRCLISTSIZE];	/* regulatory class list */
#endif /* BCMDBG || BCMDBG_DUMP */
#endif /* WL11N */

	struct tx_path_node	*tx_path; /* Function chain for tx path for a pkt */
	uint32		fragtimestamp[NUMPRIO];
#ifdef WLCNTSCB
	wlc_scb_stats_t scb_stats;
#endif /* WLCNTSCB */
	bool		stale_remove;
#ifdef PROP_TXSTATUS
	uint8		mac_address_handle;
#endif // endif
	bool		rssi_upd;		/* per scb rssi is enabled by ... */
#if defined(STA) && defined(DBG_BCN_LOSS)
	struct wlc_scb_dbg_bcn dbg_bcn;
#endif // endif
	uint32  flags3;     /* various bit flags3 as defined below */
	struct	scb *psta_prim;	/* pointer to primary proxy sta */
	int	rssi_chain[WL_RSSI_ANT_MAX][MA_WINDOW_SZ];
#if defined(PROP_TXSTATUS) && !defined(PROP_TXSTATUS_ROM_COMPAT)
	uint16	first_sup_pkt;
#endif // endif
#ifdef WLPKTDLYSTAT
#ifdef WLPKTDLYSTAT_IND
	txdelay_params_t	txdelay_params;
#endif /* WLPKTDLYSTAT_IND */
	scb_delay_stats_t	delay_stats[AC_COUNT];	/* per-AC delay stats */
#endif /* WLPKTDLYSTAT */

#if defined(PSPRETEND) && defined(PSPRETEND_SCB_ROM_COMPAT)
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
#endif /* PSPRETEND */
#if defined(WL_CS_RESTRICT_RELEASE) && defined(WL_CS_RESTRICT_RELEASE_ROM_COMPAT)
	uint16	restrict_txwin;
	uint8	restrict_deadline;
#endif /* WL_CS_RESTRICT_RELEASE */
#ifdef WLTAF
	uint16 traffic_active;
#endif // endif
#ifdef WLCNTSCB
	wlc_scb_ext_stats_t scb_ext_stats;	/* More per SCB statistics */
#endif /* WLCNTSCB */
#ifdef WL11K
	void *scb_rrm_stats;
	void *scb_rrm_tscm;
#endif /* WL11K */
#if defined(PROP_TXSTATUS) && defined(PROP_TXSTATUS_ROM_COMPAT)
	/* ROM compatibility - relocate struct fields that were excluded in ROMs,
	 * but are required in ROM offload builds.
	 */
	uint16	first_sup_pkt;
#endif // endif
#if defined(PSPRETEND) && !defined(PSPRETEND_SCB_ROM_COMPAT)
	/* ROM compatibility - relocate struct fields that were excluded in ROMs,
	 * but are required in ROM offload builds.
	 */
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
#endif /* PSPRETEND */
#if defined(WL_CS_RESTRICT_RELEASE) && !defined(WL_CS_RESTRICT_RELEASE_ROM_COMPAT)
	/* ROM compatibility - relocate struct fields that were excluded in ROMs,
	 * but are required in ROM offload builds.
	 */
	uint16	restrict_txwin;
	uint8	restrict_deadline;
#endif /* WL_CS_RESTRICT_RELEASE */
#if defined(BCMPCIEDEV)
	/* Running average info for tx packet len per flow ring */
	wlc_ravg_info_t flr_txpktlen_ravg[FLOWRING_PER_SCB_MAX];
	/* Running average info for weight per flow ring */
	wlc_ravg_info_t flr_weigth_ravg[FLOWRING_PER_SCB_MAX];
#endif /* BCMPCIEDEV */
#ifdef BCM_HOST_MEM_SCB
	struct scb* shadow;	/* pointer for correspoinding scb in shadow */
#endif // endif
	/* pointer to SCB storage while store SCB addr */
	struct scb	*scb_addr;
#ifdef AP
	uint		wsec_auth_timeout;	/* timeout to handle nas/eap restart */
#endif /* AP */
	uint8 link_bw;
#ifdef WL_SAE
	uint8 pmkid_included; /* PMKID included in assoc request */
#endif /* WL_SAE */
};

#if defined(BCMPCIEDEV)
#define TXPKTLEN_RAVG(_scb_, _ac_) (&(_scb_)->flr_txpktlen_ravg[(_ac_)])
#define WEIGHT_RAVG(_scb_, _ac_) (&(_scb_)->flr_weigth_ravg[(_ac_)])
#endif /* BCMPCIEDEV */

typedef struct {
	struct scb *scb;
	uint8	oldstate;
} scb_state_upd_data_t;

#ifdef PSPRETEND
/* bit flags for (uint8) scb.PS_pretend */
#define PS_PRETEND_NOT_ACTIVE    0

/* PS_PRETEND_PROBING states to do probing to the scb */
#define PS_PRETEND_PROBING       (1 << 0)

/* PS_PRETEND_ACTIVE indicates that ps pretend is currently active */
#define	PS_PRETEND_ACTIVE        (1 << 1)

/* PS_PRETEND_ACTIVE_PMQ indicates that we have had a PPS PMQ entry */
#define	PS_PRETEND_ACTIVE_PMQ    (1 << 2)

/* PS_PRETEND_NO_BLOCK states that we should not expect to see a PPS
 * PMQ entry, hence, not to block ourselves waiting to get one
 */
#define PS_PRETEND_NO_BLOCK      (1 << 3)

/* PS_PRETEND_PREVENT states to not do normal ps pretend for a scb */
#define PS_PRETEND_PREVENT       (1 << 4)

/* PS_PRETEND_RECENT indicates a ps pretend was triggered recently */
#define PS_PRETEND_RECENT        (1 << 5)

/* PS_PRETEND_THRESHOLD indicates that the successive failed TX status
 * count has exceeded the threshold
 */
#define PS_PRETEND_THRESHOLD     (1 << 6)

/* PS_PRETEND_ON is a bit mask of all active states that is used
 * to clear the scb state when ps pretend exits
 */
#define PS_PRETEND_ON	(PS_PRETEND_ACTIVE | PS_PRETEND_PROBING | \
						PS_PRETEND_ACTIVE_PMQ | PS_PRETEND_THRESHOLD)
#endif /* PSPRETEND */

typedef enum {
	RSSI_UPDATE_FOR_WLC = 0,	       /* Driver level */
	RSSI_UPDATE_FOR_TM	       /* Traffic Management */
} scb_rssi_requestor_t;

extern bool wlc_scb_rssi_update_enable(struct scb *scb, bool enable, scb_rssi_requestor_t);

/* Test whether RSSI update is enabled. Made a macro to reduce fn call overhead. */
#define WLC_SCB_RSSI_UPDATE_ENABLED(scb) (scb->rssi_upd != 0)

/** Iterator for scb list */
struct scb_iter {
	struct scb	*next;			/* next scb in bss */
	wlc_bsscfg_t	*next_bss;		/* next bss pointer */
	bool		all;			/* walk all bss or not */
};

#define SCB_BSSCFG(a)           ((a)->bsscfg)

/** Initialize an scb iterator pre-fetching the next scb as it moves along the list */
void wlc_scb_iterinit(scb_module_t *scbstate, struct scb_iter *scbiter,
	wlc_bsscfg_t *bsscfg);
/** move the iterator */
struct scb *wlc_scb_iternext(scb_module_t *scbstate, struct scb_iter *scbiter);

/* Iterate thru' scbs of specified bss */
#define FOREACH_BSS_SCB(scbstate, scbiter, bss, scb) \
	for (wlc_scb_iterinit((scbstate), (scbiter), (bss)); \
	     ((scb) = wlc_scb_iternext((scbstate), (scbiter))) != NULL; )

/* Iterate thru' scbs of all bss. Use this only when needed. For most of
 * the cases above one should suffice.
 */
#define FOREACHSCB(scbstate, scbiter, scb) \
	for (wlc_scb_iterinit((scbstate), (scbiter), NULL); \
	     ((scb) = wlc_scb_iternext((scbstate), (scbiter))) != NULL; )

scb_module_t *wlc_scb_attach(wlc_info_t *wlc);
void wlc_scb_detach(scb_module_t *scbstate);

/* scb cubby cb functions */
typedef int (*scb_cubby_init_t)(void *, struct scb *);
typedef void (*scb_cubby_deinit_t)(void *, struct scb *);
typedef void (*scb_cubby_dump_t)(void *, struct scb *, struct bcmstrbuf *b);

/**
 * This function allocates an opaque cubby of the requested size in the scb container.
 * The cb functions fn_init/fn_deinit are called when a scb is allocated/freed.
 * The functions are called with the context passed in and a scb pointer.
 * It returns a handle that can be used in macro SCB_CUBBY to retrieve the cubby.
 * Function returns a negative number on failure
 */
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
int wlc_scb_cubby_reserve(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context, int cubby_id);
#else
int wlc_scb_cubby_reserve(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
        scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context);
#endif // endif

/* macro to retrieve pointer to module specific opaque data in scb container */
#define SCB_CUBBY(scb, handle)	(void *)(((uint8 *)(scb)) + handle)

/*
 * Accessors
 */

struct wlcband * wlc_scbband(struct scb *scb);

/** Find station control block corresponding to the remote id */
struct scb *wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);

/** Lookup station control for ID. If not found, create a new entry. */
struct scb *wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);

/** Lookup station control for ID. If not found, create a new entry. */
struct scb *wlc_scblookupband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                              const struct ether_addr *ea, int bandunit);

/** Get scb from band */
struct scb *wlc_scbfindband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                            const struct ether_addr *ea, int bandunit);

/** Determine if any SCB associated to ap cfg */
bool wlc_scb_associated_to_ap(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/** Move the scb's band info */
void wlc_scb_update_band_for_cfg(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chanspec);

extern struct scb *wlc_scbibssfindband(wlc_info_t *wlc, const struct ether_addr *ea,
	int bandunit, wlc_bsscfg_t **bsscfg);

/** Find the STA acorss all APs */
extern struct scb *wlc_scbapfind(wlc_info_t *wlc, const struct ether_addr *ea,
	wlc_bsscfg_t **bsscfg);

extern struct scb *wlc_scbbssfindband(wlc_info_t *wlc, const struct ether_addr *hwaddr,
	const struct ether_addr *ea, int bandunit, wlc_bsscfg_t **bsscfg);

struct scb *wlc_internalscb_alloc(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *ea, struct wlcband *band);
void wlc_internalscb_free(wlc_info_t *wlc, struct scb *scb);

bool wlc_scbfree(wlc_info_t *wlc, struct scb *remove);

/** * "|" operation */
void wlc_scb_setstatebit(struct scb *scb, uint8 state);

/** * "& ~" operation . */
void wlc_scb_clearstatebit(struct scb *scb, uint8 state);

/** * "|" operation . idx = position of the bsscfg in the wlc array of multi ssids. */

void wlc_scb_setstatebit_bsscfg(struct scb *scb, uint8 state, int idx);

/** * "& ~" operation . idx = position of the bsscfg in the wlc array of multi ssids. */
void wlc_scb_clearstatebit_bsscfg(struct scb *scb, uint8 state, int idx);

/** * reset all state. the multi ssid array is cleared as well. */
void wlc_scb_resetstate(struct scb *scb);

void wlc_scb_reinit(wlc_info_t *wlc);

/** free all scbs of a bsscfg */
void wlc_scb_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm);

/** (de)authorize/(de)authenticate single station */
void wlc_scb_set_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, bool enable,
                      uint32 flag, int rc);

/** sort rates for a single scb */
void wlc_scb_sortrates(wlc_info_t *wlc, struct scb *scb);

/** sort rates for all scb in wlc */
void BCMINITFN(wlc_scblist_validaterates)(wlc_info_t *wlc);

#ifdef PROP_TXSTATUS
extern void wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle, uint8 ta_bmp);
extern bool wlc_flow_ring_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle,
	uint8 tid, bool op);
uint16 wlc_flow_ring_get_scb_handle(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *da);
extern void wlc_flush_flowring_pkts(wlc_info_t *wlc, struct wlc_if *wlcif, uint8 *addr,
	uint16 flowid, uint8 tid);
#if defined(BCMPCIEDEV)
extern uint32 wlc_flow_ring_reset_weight(wlc_info_t *wlc,
	struct wlc_if *wlcif, uint8 *da, uint8 fl);
extern void wlc_scb_upd_all_flr_weight(wlc_info_t *wlc, struct scb *scb);
#endif /* BCMPCIEDEV */
#endif /* PROP_TXSTATUS */

extern void wlc_scb_set_bsscfg(struct scb *scb, wlc_bsscfg_t *cfg);

#if defined(BCMPCIEDEV)
extern void wlc_ravg_add_weight(wlc_info_t *wlc, struct scb *scb, int fl,
	ratespec_t rspec);
extern ratespec_t wlc_ravg_get_scb_cur_rspec(wlc_info_t *wlc, struct scb *scb);
#endif /* BCMPCIEDEV */
extern uint32 wlc_scb_dot11hdrsize(struct scb *scb);

#ifdef WLTAF
extern uint32 wlc_ts2_traffic_shaper(struct scb *scb, uint32 weight);
#endif // endif

/* average rssi over window */
#if defined(AP) || defined(WLDPT) || defined(WLTDLS)
int wlc_scb_rssi(struct scb *scb);
void wlc_scb_rssi_init(struct scb *scb, int rssi);
int8 wlc_scb_rssi_chain(struct scb *scb, int chain);
/* rssi of last received packet per scb and per antenna chain */
int8 wlc_scb_pkt_rssi_chain(struct scb *scb, int chain);
#else
#define wlc_scb_rssi(a) 0
#define wlc_scb_rssi_init(a, b) 0
#define wlc_scb_rssi_chain(a, b) 0
#define wlc_scb_pkt_rssi_chain(a, b) 0
#endif /* AP || WLDPT  || WLTDLS */
#ifdef WLAWDL
extern void wlc_scb_awdl_free(struct wlc_info *wlc);
extern void wlc_awdl_update_all_scb(struct wlc_info *wlc);
#endif // endif

/* SCB flags */
#define SCB_NONERP		0x0001		/* No ERP */
#define SCB_LONGSLOT		0x0002		/* Long Slot */
#define SCB_SHORTPREAMBLE	0x0004		/* Short Preamble ok */
#define SCB_8021XHDR		0x0008		/* 802.1x Header */
#define SCB_WPA_SUP		0x0010		/* 0 - authenticator, 1 - supplicant */
#define SCB_DEAUTH		0x0020		/* 0 - ok to deauth, 1 - no (just did) */
#define SCB_WMECAP		0x0040		/* WME Cap; may ONLY be set if WME_ENAB(wlc) */
#define SCB_USME2		0x0080
#define SCB_BRCM		0x0100		/* BRCM AP or STA */
#define SCB_WDS_LINKUP		0x0200		/* WDS link up */
#define SCB_LEGACY_AES		0x0400		/* legacy AES device */
#define SCB_USME1		0x0800
#define SCB_MYAP		0x1000		/* We are associated to this AP */
#define SCB_PENDING_PROBE	0x2000		/* Probe is pending to this SCB */
#define SCB_AMSDUCAP		0x4000		/* A-MSDU capable */
#define SCB_USEME		0x8000
#define SCB_HTCAP		0x10000		/* HT (MIMO) capable device */
#define SCB_RECV_PM		0x20000		/* state of PM bit in last data frame recv'd */
#define SCB_AMPDUCAP		0x40000		/* A-MPDU capable */
#define SCB_IS40		0x80000		/* 40MHz capable */
#define SCB_NONGF		0x100000	/* Not Green Field capable */
#define SCB_APSDCAP		0x200000	/* APSD capable */
#define SCB_PENDING_FREE	0x400000	/* marked for deletion - clip recursion */
#define SCB_PENDING_PSPOLL	0x800000	/* PS-Poll is pending to this SCB */
#define SCB_RIFSCAP		0x1000000	/* RIFS capable */
#define SCB_HT40INTOLERANT	0x2000000	/* 40 Intolerant */
#define SCB_WMEPS		0x4000000	/* PS + WME w/o APSD capable */
#define SCB_SENT_APSD_TRIG	0x8000000	/* APSD Trigger Null Frame was recently sent */
#define SCB_COEX_MGMT		0x10000000	/* Coexistence Management supported */
#define SCB_IBSS_PEER		0x20000000	/* Station is an IBSS peer */
#define SCB_STBCCAP		0x40000000	/* STBC Capable */
#ifdef WLBTAMP
#define SCB_11ECAP		0x80000000	/* 802.11e Cap; ONLY set if BTA_ENAB(wlc->pub) */
#endif // endif

#define SCB_FLAGS(a)        ((a)->flags)

/* scb flags2 */
#define SCB2_SGI20_CAP          0x00000001      /* 20MHz SGI Capable */
#define SCB2_SGI40_CAP          0x00000002      /* 40MHz SGI Capable */
#define SCB2_RX_LARGE_AGG       0x00000004      /* device can rx large aggs */
#define SCB2_INTERNAL           0x00000008      /* This scb is an internal scb */
#define SCB2_IN_ASSOC           0x00000010      /* Incoming assocation in progress */
#ifdef BCMWAPI_WAI
#define SCB2_WAIHDR             0x00000020      /* WAI Header */
#endif // endif
#define SCB2_P2P                0x00000040      /* WiFi P2P */
#define SCB2_LDPCCAP            0x00000080      /* LDPC Cap */
#define SCB2_BCMDCS             0x00000100      /* BCM_DCS */
#define SCB2_MFP                0x00000200      /* 802.11w MFP_ENABLE */
#define SCB2_SHA256             0x00000400      /* sha256 for AKM */
#define SCB2_VHTCAP             0x00000800      /* VHT (11ac) capable device */
#define SCB2_HT_PROP_RATES_CAP  0x00001000      /* Broadcom proprietary 11n rates */

#ifdef PROP_TXSTATUS
#define SCB2_PROPTXTSTATUS_SUPPR_STATEMASK      0x00001000
#define SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT     12
#define SCB2_PROPTXTSTATUS_SUPPR_GENMASK        0x00002000
#define SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT       13
#define SCB2_PROPTXTSTATUS_PKTWAITING_MASK      0x00004000
#define SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT     14
#define SCB2_PROPTXTSTATUS_POLLRETRY_MASK       0x00008000
#define SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT      15
/* 4 bits for AC[0-3] traffic pending status from the host */
#define SCB2_PROPTXTSTATUS_TIM_SHIFT            16
#define SCB2_PROPTXTSTATUS_TIM_MASK             (0xf << SCB2_PROPTXTSTATUS_TIM_SHIFT)
#endif // endif
#define SCB2_TDLS_PROHIBIT      0x00100000      /* TDLS prohibited */
#define SCB2_TDLS_CHSW_PROHIBIT 0x00200000      /* TDLS channel switch prohibited */
#define SCB2_TDLS_SUPPORT       0x00400000
#define SCB2_TDLS_PU_BUFFER_STA 0x00800000
#define SCB2_TDLS_PEER_PSM      0x01000000
#define SCB2_TDLS_CHSW_SUPPORT  0x02000000
#define SCB2_TDLS_PU_SLEEP_STA  0x04000000
#define SCB2_TDLS_MASK          0x07f00000
#define SCB2_IGN_SMPS		0x08000000 	/* ignore SM PS update */
#define SCB2_IS80               0x10000000      /* 80MHz capable */
#define SCB2_AMSDU_IN_AMPDU_CAP	0x20000000      /* AMSDU over AMPDU */
#define SCB2_CCX_MFP		0x40000000	/* CCX MFP enable */
#define SCB2_DWDS_ACTIVE		0x80000000      /* DWDS is active */

#define SCB_FLAGS2(a)       ((a)->flags2)

/* scb flags3 */
#define SCB3_A4_DATA		0x00000001      /* scb does 4 addr data frames */
#define SCB3_A4_NULLDATA	0x00000002	/* scb does 4-addr null data frames */
#define SCB3_A4_8021X		0x00000004	/* scb does 4-addr 8021x frames */

#ifdef WL_RELMCAST
#define SCB3_RELMCAST		0x00000800		/* Reliable Multicast */
#define SCB3_RELMCAST_NOACK	0x00001000		/* Reliable Multicast No ACK rxed */
#endif // endif

#define SCB3_PKTC		0x00002000      /* Enable packet chaining */
#define SCB3_OPER_MODE_NOTIF    0x00004000      /* 11ac Oper Mode Notif'n */

#ifdef WL11K
#define SCB3_RRM		0x00008000      /* Radio Measurement */
#define SCB_RRM(a)		((a)->flags3 & SCB3_RRM)
#else
#define SCB_RRM(a)		FALSE
#endif /* WL11K */

#define SCB3_DWDS_CAP	0x00010000      /* DWDS capable */

#define SCB3_HT_BEAMFORMEE      0x00020000      /* Receive NDP Capable */
#define SCB3_HT_BEAMFORMER      0x00040000      /* Transmit NDP Capable */

#define SCB3_TS_MASK            0x00180000	/* Traffic Stream 2 flags */

#define SCB3_TS_ATOS            0x00000000
#define SCB3_TS_ATOS2           0x00080000
#define SCB3_TS_EBOS            0x00100000

#define SCB3_MAP_CAP		0x02000000	/* MultiAP capable */
#define SCB_FLAGS3(a)           ((a)->flags3)

#define SCB_TS_EBOS(a)          (((a)->flags3 & SCB3_TS_MASK) == SCB3_TS_EBOS)
#define SCB_TS_ATOS(a)          (((a)->flags3 & SCB3_TS_MASK) == SCB3_TS_ATOS)
#define SCB_TS_ATOS2(a)         (((a)->flags3 & SCB3_TS_MASK) == SCB3_TS_ATOS2)

#define SCB3_HOST_MEM		0x00200000      /* SCB allocated from host memory */
#define SCB_HOST(a)		((a)->flags3 & SCB3_HOST_MEM)

#define SCB3_GLOBAL_RCLASS_CAP	0X00400000
#define SCB_SUPPORTS_GLOBAL_RCLASS(a)	(((a)->flags3 & SCB3_GLOBAL_RCLASS_CAP) != 0)

#ifdef PROP_TXSTATUS
#define SCB_PROPTXTSTATUS_SUPPR_STATE(s)	(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) >> SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT)
#define SCB_PROPTXTSTATUS_SUPPR_GEN(s)		(((s)->flags2 & SCB2_PROPTXTSTATUS_SUPPR_GENMASK) \
	>> SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT)
#define SCB_PROPTXTSTATUS_TIM(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_TIM_MASK) >> SCB2_PROPTXTSTATUS_TIM_SHIFT)
#define SCB_PROPTXTSTATUS_PKTWAITING(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_PKTWAITING_MASK) >> SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT)
#define SCB_PROPTXTSTATUS_POLLRETRY(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_POLLRETRY_MASK) >> SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT)

#define SCB_PROPTXTSTATUS_SUPPR_SETSTATE(s, state)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) | \
		(((state) << SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT) & \
		SCB2_PROPTXTSTATUS_SUPPR_STATEMASK)
#define SCB_PROPTXTSTATUS_SUPPR_SETGEN(s, gen)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_SUPPR_GENMASK) | \
		(((gen) << SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT) & SCB2_PROPTXTSTATUS_SUPPR_GENMASK)
#define SCB_PROPTXTSTATUS_SETPKTWAITING(s, waiting)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_PKTWAITING_MASK) | \
		(((waiting) << SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT) & \
		SCB2_PROPTXTSTATUS_PKTWAITING_MASK)
#define SCB_PROPTXTSTATUS_SETPOLLRETRY(s, retry)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_POLLRETRY_MASK) | \
		(((retry) << SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT) & \
		SCB2_PROPTXTSTATUS_POLLRETRY_MASK)
#define SCB_PROPTXTSTATUS_SETTIM(s, tim)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_TIM_MASK) | \
		(((tim) << SCB2_PROPTXTSTATUS_TIM_SHIFT) & SCB2_PROPTXTSTATUS_TIM_MASK)
#endif /* PROP_TXSTATUS */

/* scb vht flags */
#define SCB_VHT_LDPCCAP		0x0001
#define SCB_SGI80       0x0002
#define SCB_SGI160		0x0004
#define SCB_VHT_TX_STBCCAP	0x0008
#define SCB_VHT_RX_STBCCAP	0x0010
#define SCB_SU_BEAMFORMER	0x0020
#define SCB_SU_BEAMFORMEE	0x0040
#define SCB_MU_BEAMFORMER	0x0080
#define SCB_MU_BEAMFORMEE	0x0100
#define SCB_VHT_TXOP_PS		0x0200
#define SCB_HTC_VHT_CAP		0x0400

/* scb association state bitfield */
#define UNAUTHENTICATED		0	/* unknown */
#define AUTHENTICATED		1	/* 802.11 authenticated (open or shared key) */
#define ASSOCIATED		2	/* 802.11 associated */
#define PENDING_AUTH		4	/* Waiting for 802.11 authentication response */
#define PENDING_ASSOC		8	/* Waiting for 802.11 association response */
#define AUTHORIZED		0x10	/* 802.1X authorized */
#define MARKED_FOR_DELETION	0x20	/* Delete this scb after timeout */
#define TAKEN4IBSS		0x80	/* Taken */

/* scb association state helpers */
#define SCB_ASSOCIATED(a)	((a)->state & ASSOCIATED)
#define SCB_ASSOCIATING(a)	((a)->state & PENDING_ASSOC)
#define SCB_AUTHENTICATING(a)   ((a)->state & PENDING_AUTH)
#define SCB_AUTHENTICATED(a)	((a)->state & AUTHENTICATED)
#define SCB_AUTHORIZED(a)	((a)->state & AUTHORIZED)
#define SCB_MARKED_FOR_DELETION(a) ((a)->state & MARKED_FOR_DELETION)

/* flag access */
#define SCB_ISMYAP(a)           ((a)->flags & SCB_MYAP)
#define SCB_ISPERMANENT(a)      ((a)->permanent)
#define	SCB_INTERNAL(a) 	((a)->flags2 & SCB2_INTERNAL)
/* scb association state helpers w/ respect to ssid (in case of multi ssids)
 * The bit set in the bit field is relative to the current state (i.e. if
 * the current state is "associated", a 1 at the position "i" means the
 * sta is associated to ssid "i"
 */
#define SCB_ASSOCIATED_BSSCFG(a, i)	\
	(((a)->state & ASSOCIATED) && isset((scb->auth_bsscfg), i))

#define SCB_AUTHENTICATED_BSSCFG(a, i)	\
	(((a)->state & AUTHENTICATED) && isset((scb->auth_bsscfg), i))

#define SCB_AUTHORIZED_BSSCFG(a, i)	\
	(((a)->state & AUTHORIZED) && isset((scb->auth_bsscfg), i))

#define SCB_LONG_TIMEOUT	3600	/* # seconds of idle time after which we proactively
					 * free an authenticated SCB
					 */
#define SCB_SHORT_TIMEOUT	  60	/* # seconds of idle time after which we will reclaim an
					 * authenticated SCB if we would otherwise fail
					 * an SCB allocation.
					 */
#ifdef WLMEDIA_CUSTOMER_1
#define SCB_TIMEOUT		  10	/* # seconds: interval to probe idle STAs */
#else
#define SCB_TIMEOUT		  60	/* # seconds: interval to probe idle STAs */
#endif // endif
#define SCB_ACTIVITY_TIME	   5	/* # seconds: skip probe if activity during this time */
#define SCB_GRACE_ATTEMPTS	   10	/* # attempts to probe sta beyond scb_activity_time */

#define SCB_AUTH_TIMEOUT	   10	/* # seconds: interval to wait for auth reply from
					 * supplicant/authentication server.
					 */

/* scb_info macros */
#ifdef AP
#define SCB_PS(a)		((a) && (a)->PS)
#ifdef WDS
#define SCB_WDS(a)		((a)->wds)
#else
#define SCB_WDS(a)		NULL
#endif // endif
#define SCB_INTERFACE(a)        ((a)->wds ? (a)->wds->wlif : (a)->bsscfg->wlcif->wlif)
#define SCB_WLCIFP(a)           ((a)->wds ? (a)->wds : ((a)->bsscfg->wlcif))
#define WLC_BCMC_PSMODE(wlc, bsscfg) (SCB_PS(WLC_BCMCSCB_GET(wlc, bsscfg)))
#else
#define SCB_PS(a)		FALSE
#define SCB_WDS(a)		NULL
#define SCB_INTERFACE(a)        ((a)->bsscfg->wlcif->wlif)
#define SCB_WLCIFP(a)           (((a)->bsscfg->wlcif))
#define WLC_BCMC_PSMODE(wlc, bsscfg) (TRUE)
#endif /* AP */

#ifdef PSPRETEND
#define	SCB_PS_PRETEND(a)            ((a) && ((a)->ps_pretend & PS_PRETEND_ACTIVE))
#define SCB_PS_PRETEND_NORMALPS(a)   (SCB_PS(a) && !SCB_PS_PRETEND(a))
#define SCB_PS_PRETEND_THRESHOLD(a)  ((a) && ((a)->ps_pretend & PS_PRETEND_THRESHOLD))

/* Threshold mode never expects active PMQ, so do not block waiting for PMQ */
#define	SCB_PS_PRETEND_BLOCKED(a)    \
						(SCB_PS_PRETEND(a) && \
						!SCB_PS_PRETEND_THRESHOLD(a) && \
						!(((a)->ps_pretend & PS_PRETEND_ACTIVE_PMQ) || \
						((a)->ps_pretend & PS_PRETEND_NO_BLOCK)))

#define	SCB_PS_PRETEND_PROBING(a)	 \
						(SCB_PS_PRETEND(a) && \
						((a)->ps_pretend & \
						PS_PRETEND_PROBING))

#define SCB_PS_PRETEND_ENABLED(a)  \
						(PS_PRETEND_ENABLED(SCB_BSSCFG((a))) && \
						!((a)->ps_pretend & PS_PRETEND_PREVENT) && \
						!SCB_ISMULTI(a))

#define SCB_PS_PRETEND_THRESHOLD_ENABLED(a)  \
						(PS_PRETEND_THRESHOLD_ENABLED(SCB_BSSCFG((a))) && \
						!((a)->ps_pretend & PS_PRETEND_PREVENT) && \
						!SCB_ISMULTI(a))

#define SCB_PS_PRETEND_WAS_RECENT(a) \
						(!SCB_PS_PRETEND(a) && \
						(a) && ((a)->ps_pretend & PS_PRETEND_RECENT))
#else
#define	SCB_PS_PRETEND(a)                       (0)
#define SCB_PS_PRETEND_NORMALPS(a)              SCB_PS(a)
#define	SCB_PS_PRETEND_BLOCKED(a)               (0)
#define SCB_PS_PRETEND_THRESHOLD(a)				(0)
#define	SCB_PS_PRETEND_PROBING(a)               (0)
#define SCB_PS_PRETEND_ENABLED(w, a)            (0)
#define SCB_PS_PRETEND_THRESHOLD_ENABLED(w, a)  (0)
#define SCB_PS_PRETEND_WAS_RECENT(a)            (0)
#endif /* PSPRETEND */

#ifdef BCM_HOST_MEM_SCBTAG
/** Determine if a scb is in host memory addressable within the dongle via the
 * 128MB PCIE Address Match (small region). In mixed mode, when some scbs are in
 * dongle, no caching is employed.
 */
#define SCBTAG_IN_HOST_MEM(addr)        ((uint32)(addr) >= PCIE_ADDR_MATCH1)
#define SCBTAG_IN_DNGL_MEM(addr)        (!SCBTAG_IN_HOST_MEM(addr))
#else
#define SCBTAG_IN_DNGL_MEM(addr)         (TRUE)
#endif /* ! BCM_HOST_MEM_SCBTAG */

#ifdef WME
#define SCBF_WME(f)		((f) & SCB_WMECAP)	/* Also implies WME_ENAB(wlc) */
#define SCB_WME(a)		SCBF_WME((a)->flags)
#else
#define SCBF_WME(f)		((void)(f), FALSE)
#define SCB_WME(a)		((void)(a), FALSE)
#endif // endif

#ifdef WLAMPDU
#define SCBF_AMPDU(f)		((f) & SCB_AMPDUCAP)
#define SCB_AMPDU(a)		SCBF_AMPDU((a)->flags)
#else
#define SCBF_AMPDU(f)		FALSE
#define SCB_AMPDU(a)		FALSE
#endif // endif

#ifdef WLAMSDU
/* Disable amsdu for host SCB, to avoid tput drop caused by amsdu=on */
#define SCB_AMSDU(a)		(SCBTAG_IN_DNGL_MEM(a) && ((a)->flags & SCB_AMSDUCAP))

#define SCB_AMSDU_IN_AMPDU(a) ((a)->flags2 & SCB2_AMSDU_IN_AMPDU_CAP)
#else
#define SCB_AMSDU(a)		FALSE
#define SCB_AMSDU_IN_AMPDU(a) FALSE
#endif // endif

#ifdef WL11N
#define SCBF_HT_CAP(f)		(((f) & SCB_HTCAP) != 0)
#define SCB_HT_CAP(a)		SCBF_HT_CAP((a)->flags)
#define SCB_VHT_CAP(a)		(((a)->flags2 & SCB2_VHTCAP) != 0)
#define SCB_ISGF_CAP(a)		(((a)->flags & (SCB_HTCAP | SCB_NONGF)) == SCB_HTCAP)
#define SCB_NONGF_CAP(a)	(((a)->flags & (SCB_HTCAP | SCB_NONGF)) == \
					(SCB_HTCAP | SCB_NONGF))
#define SCB_COEX_CAP(a)		((a)->flags & SCB_COEX_MGMT)
#define SCB_STBC_CAP(a)		((a)->flags & SCB_STBCCAP)
#define SCBF_LDPC_CAP(f, f2) (SCBF_HT_CAP(f) && ((f2) & SCB2_LDPCCAP))
#define SCB_LDPC_CAP(a)		(SCB_HT_CAP(a) && ((a)->flags2 & SCB2_LDPCCAP))
#define SCB_HT_PROP_RATES_CAP(a) (((a)->flags2 & SCB2_HT_PROP_RATES_CAP) != 0)
#else /* WL11N */
#define SCB_HT_CAP(a)		FALSE
#define SCB_VHT_CAP(a)		FALSE
#define SCB_ISGF_CAP(a)		FALSE
#define SCB_NONGF_CAP(a)	FALSE
#define SCB_COEX_CAP(a)		FALSE
#define SCB_STBC_CAP(a)		FALSE
#define SCB_LDPC_CAP(a)		FALSE
#define SCB_HT_PROP_RATES_CAP(a) FALSE
#endif /* WL11N */

#ifdef WL11AC
#define SCB_VHT_LDPC_CAP(v, a)	(SCB_VHT_CAP(a) && \
	(wlc_vht_get_scb_flags(v, a) & SCB_VHT_LDPCCAP))
#define SCB_VHT_TX_STBC_CAP(v, a)	(SCB_VHT_CAP(a) && \
	(wlc_vht_get_scb_flags(v, a) & SCB_VHT_TX_STBCCAP))
#define SCB_VHT_RX_STBC_CAP(v, a)	(SCB_VHT_CAP(a) && \
	(wlc_vht_get_scb_flags(v, a) & SCB_VHT_RX_STBCCAP))
#define SCB_VHT_SGI80(v, a)	(SCB_VHT_CAP(a) && \
	(wlc_vht_get_scb_flags(v, a) & SCB_SGI80))
#define SCB_OPER_MODE_NOTIF_CAP(a) ((a)->flags3 & SCB3_OPER_MODE_NOTIF)
#else /* WL11AC */
#define SCB_VHT_LDPC_CAP(v, a)		FALSE
#define SCB_VHT_TX_STBC_CAP(v, a)	FALSE
#define SCB_VHT_RX_STBC_CAP(v, a)	FALSE
#define SCB_VHT_SGI80(v, a)		FALSE
#define SCB_OPER_MODE_NOTIF_CAP(a) (0)
#endif /* WL11AC */

#define SCB_IS_IBSS_PEER(a)	((a)->flags & SCB_IBSS_PEER)
#define SCB_SET_IBSS_PEER(a)	((a)->flags |= SCB_IBSS_PEER)
#define SCB_UNSET_IBSS_PEER(a)	((a)->flags &= ~SCB_IBSS_PEER)

#if defined(PKTC) || defined(PKTC_DONGLE)
#define SCB_PKTC_ENABLE(a)	((a)->flags3 |= SCB3_PKTC)
#define SCB_PKTC_DISABLE(a)	((a)->flags3 &= ~SCB3_PKTC)
#define SCB_PKTC_ENABLED(a)	((a)->flags3 & SCB3_PKTC)
#else
#define SCB_PKTC_ENABLE(a)
#define SCB_PKTC_DISABLE(a)
#define SCB_PKTC_ENABLED(a)	FALSE
#endif // endif

#ifdef WLBTAMP
#define SCBF_11E(f)		((f) & SCB_11ECAP)
#define SCB_11E(a)		SCBF_11E((a)->flags)
#else
#define SCBF_11E(a)		FALSE
#define SCB_11E(a)		FALSE
#endif // endif

#ifdef WLBTAMP
#define SCBF_QOS(f)		((f) & (SCB_WMECAP | SCB_HTCAP | SCB_11ECAP))
#define SCB_QOS(a)		SCBF_QOS((a)->flags)
#else
#define SCBF_QOS(f)		((f) & (SCB_WMECAP | SCB_HTCAP))
#define SCB_QOS(a)		SCBF_QOS((a)->flags)
#endif /* WLBTAMP */

#define SCBF_40_CAP(f)	((f) & SCB_IS40)
#define SCB_40_CAP(a)	SCBF_40_CAP((a)->flags)

#ifdef WLP2P
#define SCB_P2P(a)		((a)->flags2 & SCB2_P2P)
#else
#define SCB_P2P(a)		FALSE
#endif // endif

#ifdef DWDS
#define SCB_DWDS_CAP(a)		((a)->flags3 & SCB3_DWDS_CAP)
#define SCB_DWDS(a)		((a)->flags2 & SCB2_DWDS_ACTIVE)
#else
#define SCB_DWDS_CAP(a)		FALSE
#define SCB_DWDS(a)		FALSE
#endif // endif

#ifdef MULTIAP
#define SCB_MAP_CAP(a)		((a)->flags3 & SCB3_MAP_CAP)
#else
#define SCB_MAP_CAP(a)		FALSE
#endif // endif

#define SCB_DWDS_ACTIVATE(a)	((a)->flags2 |= SCB2_DWDS_ACTIVE)
#define SCB_DWDS_DEACTIVATE(a)	((a)->flags2 &= ~SCB2_DWDS_ACTIVE)

#define SCB_LEGACY_WDS(a)	(SCB_WDS(a) && !SCB_DWDS(a))

#define SCBF_A4_DATA(f3)	((f3) & SCB3_A4_DATA)
#define SCB_A4_DATA(a)		SCBF_A4_DATA((a)->flags3)
#define SCB_A4_DATA_ENABLE(a)	((a)->flags3 |= SCB3_A4_DATA)
#define SCB_A4_DATA_DISABLE(a)	((a)->flags3 &= ~SCB3_A4_DATA)

#define SCB_A4_NULLDATA(a)	((a)->flags3 & SCB3_A4_NULLDATA)
#define SCB_A4_8021X(a)		((a)->flags3 & SCB3_A4_8021X)

#define SCB_MFP(a)		((a) && ((a)->flags2 & SCB2_MFP))
#define SCB_SHA256(a)		((a) && ((a)->flags2 & SCB2_SHA256))
#define SCB_CCX_MFP(a)	((a) && ((a)->flags2 & SCB2_CCX_MFP))

#define SCB_MFP_ENABLE(a)       ((a)->flags2 |= SCB2_MFP)
#define SCB_MFP_DISABLE(a)      ((a)->flags2 &= ~SCB2_MFP)

#define SCB_SEQNUM(scb, prio)	(scb)->seqnum[(prio)]

/* returns pointer to struct ether_addr */
#define SCB_EA(a)       (&(a)->ea)
#define SCB_ISMULTI(a)	ETHER_ISMULTI((a)->ea.octet)

#define SCB_KEY(a)		((a)->key)
#define SCB_WPA_AUTH(a)	((a)->WPA_auth)

#ifdef WLCNTSCB
#define WLCNTSCBINCR(a)			((a)++)	/* Increment by 1 */
#define WLCNTSCBDECR(a)			((a)--)	/* Decrement by 1 */
#define WLCNTSCBADD(a,delta)		((a) += (delta)) /* Increment by specified value */
#define WLCNTSCBSET(a,value)		((a) = (value)) /* Set to specific value */
#define WLCNTSCBVAL(a)			(a)	/* Return value */
#define WLCNTSCB_COND_SET(c, a, v)	do { if (c) (a) = (v); } while (0)
#define WLCNTSCB_COND_ADD(c, a, d)	do { if (c) (a) += (d); } while (0)
#define WLCNTSCB_COND_INCR(c, a)	do { if (c) (a) += (1); } while (0)
#else /* WLCNTSCB */
#define WLCNTSCBINCR(a)			/* No stats support */
#define WLCNTSCBDECR(a)			/* No stats support */
#define WLCNTSCBADD(a,delta)		/* No stats support */
#define WLCNTSCBSET(a,value)		/* No stats support */
#define WLCNTSCBVAL(a)		0	/* No stats support */
#define WLCNTSCB_COND_SET(c, a, v)	/* No stats support */
#define WLCNTSCB_COND_ADD(c, a, d) 	/* No stats support */
#define WLCNTSCB_COND_INCR(c, a)	/* No stats support */
#endif /* WLCNTSCB */

/* Given the 'feature', invoke the next stage of transmission in tx path */
#define SCB_TX_NEXT(fid, scb, pkt, prec) \
	(scb->tx_path[(fid)].next_tx_fn((scb->tx_path[(fid)].next_handle), (scb), (pkt), (prec)))

/* Is the feature currently in the path to handle transmit. ACTIVE implies CONFIGURED */
#define SCB_TXMOD_ACTIVE(scb, fid) (scb->tx_path[(fid)].next_tx_fn != NULL)

/* Is the feature configured? */
#define SCB_TXMOD_CONFIGURED(scb, fid) (scb->tx_path[(fid)].configured)

/* Next feature configured */
#define SCB_TXMOD_NEXT_FID(scb, fid) (scb->tx_path[(fid)].next_fid)

extern void wlc_scb_txmod_activate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);
extern void wlc_scb_txmod_deactivate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);

extern void wlc_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit,
	wlc_bsscfg_t *bsscfg);

#ifdef WLAWDL
extern struct scb * wlc_scbfind_dualband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct ether_addr *addr);
#endif // endif

extern int wlc_scb_save_wpa_ie(wlc_info_t *wlc, struct scb *scb, bcm_tlv_t *ie);

extern void wlc_internal_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit);
extern int wlc_scb_state_upd_register(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg);
extern int wlc_scb_state_upd_unregister(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg);

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
struct scb_info*
get_scb_info_shadow(struct scb_info* scb_info_in);

struct scb*
wlc_swap_host_to_dongle(wlc_info_t *wlc, struct scb_info *scb_info_in);

struct scb*
wlc_swap_dongle_to_host(wlc_info_t *wlc, struct scb_info *scb_info_in);

int
rrm_cubby_host2dongle(wlc_info_t *wlc, void *context, struct scb *scb_dongle, struct scb *scb_host);
int
rrm_cubby_dongle2host(wlc_info_t *wlc, void *context, struct scb *scb_dongle, struct scb *scb_host);

int
ampdu_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb *scb_dongle, struct scb *scb_host);
int
ampdu_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb *scb_dongle, struct scb *scb_host);

int
ampdu_rx_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb *scb_dongle, struct scb *scb_host);

int
ampdu_rx_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb *scb_dongle, struct scb *scb_host);

int
rate_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
rate_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
taf_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
taf_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
txbf_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
txbf_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
txc_cubby_dongle2host(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);

int
txc_cubby_host2dongle(wlc_info_t *wlc, void *context,
	struct scb* scb_dongle, struct scb* scb_host);
#endif /* (BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB) */

#if defined(SCBFREELIST) || defined(BCM_HOST_MEM_SCB)
extern uint32 wlc_scb_get_dngl_alloc_scb_num(wlc_info_t *wlc);
#endif /* SCBFREELIST || BCM_HOST_MEM_SCB */

/**
 * SCBTAG: Cache SCB fields for access across datapath function call sequence.
 *
 * HOST SCB cached in dongle in a global scbtag object to limit the number of
 * host accesses, on a per field basis. A single field may be accessed multiple
 * times, as a scb pointer is passed to each function requiring access to the
 * field. Host accesses via a backplane address trap and redirection over PCIE
 * to host DDR, not only introduces waiting cycles for a RD to complete but also
 *
 * introduces traffic over the PCIe limiting goodput and latency.
 * A SCBTAG is prepared by copying the fields from the host side SCB. Prior to
 * an access to a cached field, the cached context is used to validate that the
 * cached SCBTAG corresponds to the host SCB. Any modified cached fields need to
 * flushed to the host SCB.
 *
 * Most cubbies in the host SCB are simply pointers to other subsystem objects.
 * The SCBTAG is populated by these pointers on the first host access, reducing
 * subsequent host accesses.
 *
 * SCBTAG_###(host_scb): Cached SCB Field Accessors
 *     Will first ensure that the SCBTAG caches the host_scb and then returns
 *     the cached value.
 *
 * SCBTAG_###_INFO(info, host_scb) : Cubby Object Accessor
 *     if SCBTAG is not caching the host_scb, determine directly from host,
 *     else if not yet cached, then first cache the cubby pointed object, and
 *         then return the cache cubby pointed object.
 *
 */

/** Section: Compiler Preprocessor Directives ------------------------------- */

#if defined(BCM_HOST_MEM_SCBTAG)

/** BCM_HOST_MEM_SCBTAG may only be used alongwith Host Resident SCB feature */
#if !defined(BCM_HOST_MEM_SCB)
#error "BCM_HOST_MEM_SCBTAG: BCM_HOST_MEM_SCB is not defined"
#endif /* BCM_HOST_MEM_SCB */

/** Host Resident SCB addressing via SBTOPCIE 128MByte translation window */
#if !defined(PCIE_ADDR_MATCH1)
#error "BCM_HOST_MEM_SCBTAG: PCIE_ADDR_MATCH1 is not defined"
#endif /* BCM_HOST_MEM_SCB */

/**
 * SCBTAG has been validated for following feature sets, by tracking all
 * host accesses via a host scb. As part of validation, modifications to fields
 * need to be synchronized to host scb.
 */
#if !defined(AP) || !defined(PKTC_TX_DONGLE)
#error "BCM_HOST_MEM_SCBTAG: undefined - one of AP PKTC_TX_DONGLE"
#endif // endif
#if !defined(WDS) || !defined(DWDS) || !defined(WLAMSDU) || !defined(WLAMPDU)
#error "BCM_HOST_MEM_SCBTAG: undefined - one of WDS DWDS WLAMSDU WLAMPDU"
#endif // endif
#if !defined(WL11AC) || !defined(PROP_TXSTATUS)
#error "BCM_HOST_MEM_SCBTAG: undefined - one of WL11AC PROP_TXSTATUS"
#endif // endif

/**
 * In an "assert" firmware target build, the BCM_HOST_MEM_SCBTAG_AUDIT mode will
 * be selected. FOR DESIGN TESTING, to enable AUDIT mode, you may swap the
 * BCM_HOST_MEM_SCBTAG settings for CACHE and AUDIT
 */
/** BCM_HOST_MEM_SCBTAG_INUSE: perform caching into SCBTAG */
#define BCM_HOST_MEM_SCBTAG_INUSE (BCM_HOST_MEM_SCBTAG > 0)

/** BCM_HOST_MEM_SCBTAG_CACHE: Use SCBTAG cached values */
#define BCM_HOST_MEM_SCBTAG_CACHE (BCM_HOST_MEM_SCBTAG == 1)

/** BCM_HOST_MEM_SCBTAG_AUDIT: Fetch from host and audit against SCBTAG value */
#define BCM_HOST_MEM_SCBTAG_AUDIT (BCM_HOST_MEM_SCBTAG == 2)

/** Enable caching of scb_txc_info within a scbtag */
#if defined(BCM_HOST_MEM_TXCTAG)
/** BCM_HOST_MEM_SCBTAG_CUBBIES: Enable on-demand caching of specific cubbies */
#define BCM_HOST_MEM_SCBTAG_CUBBIES
#endif // endif

/** BCM_HOST_MEM_SCBTAG_PMSTATS: Enables statistics collection */
/* #define BCM_HOST_MEM_SCBTAG_PMSTATS */

/** SCBTAG based caching for TxPost processing with AES:
 *  SCBTAG_CACHE: Invokes wlc_scbfindband().
 *
 * When SCBTAG lifetime is enabled,  scbtag_cache will invoke wlc_scbfindband(),
 * and cache the scb, if it was allocated in host memory and not already cached.
 *
 * In PCIEDEV TxPost processing path, the scbtag lifetime will be enabled and
 * and disabled encompassing txPost flowring processing.
 */
#if BCM_HOST_MEM_SCBTAG_INUSE
#define SCBTAG_CACHE(wlc, bsscfg, ea, bandunit) \
	scbtag_cache((wlc), (bsscfg), (ea), (bandunit))
#else /* !BCM_HOST_MEM_SCBTAG_INUSE */
#define SCBTAG_CACHE(wlc, bsscfg, ea, bandunit) \
	wlc_scbfindband((wlc), (bsscfg), (ea), (bandunit))
#endif /* !BCM_HOST_MEM_SCBTAG_INUSE */

#if defined(BCMDBG_ERR)

/** BCM_HOST_MEM_SCBTAG_ASSERT: Enables custom assert() */
/* #define BCM_HOST_MEM_SCBTAG_ASSERT_FN */

/** Enable cached scbtag fields dump */
#define BCM_HOST_MEM_SCBTAG_DUMP_FN
/** Enable scbtag_audit() */
#define BCM_HOST_MEM_SCBTAG_AUDIT_FN

#endif /* BCMDBG_ERR */

#if defined(BCM_HOST_MEM_SCBTAG_ASSERT_FN)
/* In non-assert builds, enable custom assert */
#define SCBTAG_ASSERT(exp) \
	({ \
		if (!(exp)) { \
			printf("%s %d: %s\n", __FUNCTION__, __LINE__, #exp); \
			scbtag_dump(TRUE); /* verbose dump */ \
			ASSERT(exp); \
			while (1); \
		} \
	})
#else  /* !BCM_HOST_MEM_SCBTAG_ASSERT_FN */
#define SCBTAG_ASSERT(exp)      ASSERT(exp)
#endif /* !BCM_HOST_MEM_SCBTAG_ASSERT_FN */

/** Section: Forward Declarations ------------------------------------------- */

struct wlc_info;
struct scb_info;
struct wlcband;

/** Section: Typedefs ------------------------------------------------------- */

/** Miscellaneous contexts for validating a SCBTAG */
typedef struct scbtag_context {
	struct scb              *host_scb;     /* host scb object */
	                                        /* contexts associated with scb */
	uint32                  assoctime;
	wlc_bsscfg_t            *bsscfg;
	struct wlc_info         *wlc;
	wlc_if_t                *wlcif;
	struct ether_addr       ea;
	uint32                  bandunit;

	uint16                  txc_sz;         /* scb_txc_info cached size */
	bool                    lifetime;       /* control lifetime of scbtag */
} scbtag_context_t;

/** Miscellaneous SCB Read Only fields cached in dongle memory */
typedef struct scbtag_rcached {
	uint32                  flags;
	uint32                  flags2;
	uint32                  flags3;
	wsec_key_t              *key;
	wlc_if_t                *wds;
	bool                    PS;
	bool                    ismulti;

	struct scb_info         *host_scb_info; /* scb_info related fields */
	struct wlcband          *band;
} scbtag_rcached_t;

/** Miscellaneous host allocated cubby objects, whose pointers are cached */
#if defined(BCM_HOST_MEM_SCBTAG_CUBBIES)
struct scb_txc_info;  /* scb_txc_info cubby is a pointer to scb_txc_info_t */

#define SCBTAG_TXC_INFO_MAX_SZ (256) /* 232Byte */

/* Presently only txc is cached in its entirety.
 * Another likely candidate is ini but this is too large (+500B) and sparsely
 * accessed.
 */
typedef struct scbtag_cubbies {
	struct scb_txc_info     * host_scb_txc_info; /* scb_txc_info_t */
	uint8                   cached_scb_txc_info[SCBTAG_TXC_INFO_MAX_SZ];
} scbtag_cubbies_t;
#endif /* BCM_HOST_MEM_SCBTAG_CUBBIES */

#if defined(BCM_HOST_MEM_SCBTAG_PMSTATS)
/** Miscellaneous performance monitoring statistics */
typedef struct scbtag_pmstats {
	uint32                  finds;    /* count of scbtag found locally */
	uint32                  fills;    /* count of scbtag fill from host */
	uint32                  wback;    /* count of scbtag write back */
	uint32                  rhits;    /* count of scbtag RD field access */
	uint32                  chits;    /* count of scbtag cubby hits */
	uint32                  cmiss;    /* count of scbtag cubby misses */
} scbtag_pmstats_t;
#endif /* BCM_HOST_MEM_SCBTAG_STATS */

/** SCBTAG structure to be singleton instantiated. */
typedef struct scbtag {
	scbtag_context_t context;
	scbtag_rcached_t rcached;
#if defined(BCM_HOST_MEM_SCBTAG_CUBBIES)
	scbtag_cubbies_t cubbies;
#endif // endif
#if defined(BCM_HOST_MEM_SCBTAG_PMSTATS)
	scbtag_pmstats_t pmstats;
#endif /* BCM_HOST_MEM_SCBTAG_STATS */
	txmod_info_t txmod_fns[TXMOD_LAST];
} scbtag_t;

/** Section: Externs -------------------------------------------------------- */

/** Global single instance of a scbtag: for easy access from any function */
extern scbtag_t scbtag_g;

/** Section: Macros --------------------------------------------------------- */

#define SCBTAG_NOOP()                   do { /* noop */ } while (0)

/** SCBTAG is actively caching "a" host_scb */
#define SCBTAG_IS_ACTIVE()              (scbtag_g.context.host_scb != NULL)

/** SCBTAG caching is enabled */
#define SCBTAG_IS_ENABLED()             (scbtag_g.context.lifetime == TRUE)

/** SCBTAG is caching the requested host_scb */
#define SCBTAG_IS_CACHED(scb)           (scbtag_g.context.host_scb == (scb))

/** Performance monitoring statistics */
#if defined(BCM_HOST_MEM_SCBTAG_PMSTATS)
#define SCBTAG_PMSTATS_ADD(ctr_name, cnt) \
	(scbtag_g.pmstats.ctr_name += (cnt))
#else  /* ! BCM_HOST_MEM_SCBTAG_PMSTATS */
#define SCBTAG_PMSTATS_ADD(ctr_name, cnt)   SCBTAG_NOOP()
#endif /* ! BCM_HOST_MEM_SCBTAG_PMSTATS */

/** SCBTAG Accessor for Cached Mode */

#define SCBTAG_G_BSSCFG         (scbtag_g.context.bsscfg)
#define SCBTAG_G_WLCIFP         (scbtag_g.context.wlcif)
#define SCBTAG_G_FLAGS          (scbtag_g.rcached.flags)
#define SCBTAG_G_FLAGS2         (scbtag_g.rcached.flags2)
#define SCBTAG_G_FLAGS3         (scbtag_g.rcached.flags3)
#define SCBTAG_G_KEY            (scbtag_g.rcached.key)
#define SCBTAG_G_ISMULTI        (scbtag_g.rcached.ismulti)

/** SCBTAG Accessor for Cached Mode and Audit Mode */

#if BCM_HOST_MEM_SCBTAG_CACHE /* --- scbtag cached accessors (w stats) --- */

#define SCBTAG_BSSCFG(scb) \
	({  wlc_bsscfg_t *bsscfg; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			bsscfg = SCB_BSSCFG(&scbtag_g.context); \
		} else { bsscfg = SCB_BSSCFG(scb); } \
		bsscfg; \
	})
#define SCBTAG_WLCIFP(scb) \
	({  wlc_if_t * wlcif; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			wlcif = scbtag_g.context.wlcif; \
		} else { wlcif = SCB_WLCIFP(scb); } \
		wlcif; \
	})
#define SCBTAG_EA(scb) \
	({  struct ether_addr *ea; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 3); \
			ea = SCB_EA(&scbtag_g.context); \
		} else { ea = SCB_EA(scb); } \
		ea; \
	})
#define SCBTAG_ISMULTI(scb) \
	({  bool ismulti; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			ismulti = scbtag_g.rcached.ismulti; \
		} else { ismulti = ETHER_ISMULTI(SCB_EA(scb)); } \
		ismulti; \
	})

#define SCBTAG_FLAGS(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_FLAGS(&scbtag_g.rcached); \
		} else { flags = SCB_FLAGS(scb); } \
		flags; \
	})
#define SCBTAG_FLAGS2(scb) \
	({  uint32 flags2; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags2 = SCB_FLAGS2(&scbtag_g.rcached); \
		} else { flags2 = SCB_FLAGS2(scb); } \
		flags2; \
	})
#define SCBTAG_FLAGS3(scb) \
	({  uint32 flags3; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags3 = SCB_FLAGS3(&scbtag_g.rcached); \
		} else { flags3 = SCB_FLAGS3(scb); } \
		flags3; \
	})
#define SCBTAG_QOS(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_QOS(&scbtag_g.rcached); \
		} else { flags = SCB_QOS(scb); } \
		flags; \
	})
#define SCBTAG_40_CAP(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_40_CAP(&scbtag_g.rcached); \
		} else { flags = SCB_40_CAP(scb); } \
		flags; \
	})
#define SCBTAG_AMPDU(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_AMPDU(&scbtag_g.rcached); \
		} else { flags = SCB_AMPDU(scb); } \
		flags; \
	})
#define SCBTAG_AMSDU(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_AMSDU(&scbtag_g.rcached); \
		} else { flags = SCB_AMSDU(scb); } \
		flags; \
	})
#define SCBTAG_WME(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags = SCB_WME(&scbtag_g.rcached); \
		} else { flags = SCB_WME(scb); } \
		flags; \
	})
#define SCBTAG_LDPC_CAP(scb) \
	({  uint32 flags; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 2); \
			flags = ((SCB_HT_CAP(&scbtag_g.rcached)) && \
				(scbtag_g.rcached.flags2 & SCB2_LDPCCAP)); \
		} else { \
			flags = ((SCB_HT_CAP(scb)) && ((scb)->flags2 & SCB2_LDPCCAP)); \
		} \
		flags; \
	})
#define SCBTAG_A4_DATA(scb) \
	({  uint32 flags3; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			flags3 = SCB_A4_DATA(&scbtag_g.rcached); \
		} else { flags3 = SCB_A4_DATA(scb); } \
		flags3; \
	})
#define SCBTAG_KEY(scb) \
	({  wsec_key_t *key; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			key = SCB_KEY(&scbtag_g.rcached); \
		} else { key = SCB_KEY(scb); } \
		key; \
	})
#define SCBTAG_WDS(scb) \
	({  wlc_if_t *wds; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			wds = SCB_WDS(&scbtag_g.rcached); \
		} else { wds = SCB_WDS(scb); } \
		wds; \
	})
#define SCBTAG_PS(scb) \
	({  bool PS; \
		if (SCBTAG_IS_CACHED(scb)) { \
			SCBTAG_PMSTATS_ADD(rhits, 1); \
			PS = SCB_PS(&scbtag_g.rcached); \
		} else { PS = SCB_PS(scb); } \
		PS; \
	})

#elif BCM_HOST_MEM_SCBTAG_AUDIT /* --- Audit Host and Cached scbtag --- */

#define SCBTAG_BSSCFG(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT(SCB_BSSCFG(scb) == SCB_BSSCFG(&scbtag_g.context)); } \
		SCB_BSSCFG(scb); \
	})
#define SCBTAG_WLCIFP(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT(SCB_WLCIFP(scb) == scbtag_g.context.wlcif); } \
		SCB_WLCIFP(scb); \
	})
#define SCBTAG_EA(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 3); \
			SCBTAG_ASSERT(eacmp(SCB_EA(scb), SCB_EA(&scbtag_g.context)) == 0); } \
		SCB_EA(scb); \
	})
#define SCBTAG_ISMULTI(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT(eacmp(SCB_EA(scb), SCB_EA(&scbtag_g.context)) == 0); } \
		scbtag_g.rcached.ismulti; \
	})

#define SCBTAG_FLAGS(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_FLAGS(scb); \
	})
#define SCBTAG_FLAGS2(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags2 == scbtag_g.rcached.flags2); } \
		SCB_FLAGS2(scb); \
	})
#define SCBTAG_FLAGS3(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags3 == scbtag_g.rcached.flags3); } \
		SCB_FLAGS3(scb); \
	})
#define SCBTAG_QOS(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_QOS(scb); \
	})
#define SCBTAG_40_CAP(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_40_CAP(scb); \
	})
#define SCBTAG_AMPDU(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_AMPDU(scb); \
	})
#define SCBTAG_AMSDU(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_AMSDU(scb); \
	})
#define SCBTAG_WME(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); } \
		SCB_WME(scb); \
	})
#define SCBTAG_LDPC_CAP(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 2); \
			SCBTAG_ASSERT((scb)->flags == scbtag_g.rcached.flags); \
			SCBTAG_ASSERT((scb)->flags2 == scbtag_g.rcached.flags2); } \
		SCB_LDPC_CAP(scb); \
	})
#define SCBTAG_A4_DATA(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->flags3 == scbtag_g.rcached.flags3); } \
		SCB_A4_DATA(scb); \
	})
#define SCBTAG_KEY(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->key == scbtag_g.rcached.key); } \
		SCB_KEY(scb); \
	})
#define SCBTAG_WDS(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->wds == scbtag_g.rcached.wds); } \
		SCB_WDS(scb); \
	})
#define SCBTAG_PS(scb) \
	({ \
		if (SCBTAG_IS_CACHED(scb)) { SCBTAG_PMSTATS_ADD(rhits, 1); \
			SCBTAG_ASSERT((scb)->PS == scbtag_g.rcached.PS); } \
		SCB_PS(scb); \
	})

#endif /* !BCM_HOST_MEM_SCBTAG_CACHE && !BCM_HOST_MEM_SCBTAG_AUDIT */

/** Sync a field in a SCBTAG section from/to the host scb */
#define SCBTAG_H2D_SYNC(scb, SECTION, FIELD) \
	({ if (SCBTAG_IS_CACHED(scb)) scbtag_g.SECTION.FIELD = (scb)->FIELD; })

#define SCBTAG_D2H_SYNC(scb, SECTION, FIELD) \
	({ if (SCBTAG_IS_CACHED(scb)) (scb)->FIELD = scbtag_g.SECTION.FIELD; })

/** Cached version of feature_id contexts */
#define SCBTAG_TX_NEXT(fid, scb, pkt, prec) \
	({ \
		scb_txmod_t feature_id = (scb)->tx_path[(fid)].next_fid; \
		txmod_info_t *txmod_info = &scbtag_g.txmod_fns[feature_id]; \
		txmod_info->tx_fn(txmod_info->ctx, (scb), (pkt), (prec)); \
	})

/** Section: Exported Interface --------------------------------------------- */

/** Dump the contents of a scbtag:
 *  verbose = -1: clear stats on dump
 *  verbose >  0: dump detailed statistics
 */
void scbtag_dump(int verbose);

#if defined(BCM_HOST_MEM_SCBTAG_AUDIT_FN)
/** Audit the contents of a scbtag: BCME_OK, BCME_NOTREADY,AuditFail __LINE__ */
int scbtag_audit(void);
#define SCBTAG_AUDIT()          scbtag_audit()
#else
#define SCBTAG_AUDIT()          (BCME_OK)
#endif /* BCM_HOST_MEM_SCBTAG_AUDIT_FN */

/** Demarcates the lifetime of a SCBTAG, permitting caching to occur or not */
void scbtag_lifetime(bool control);

/** Flush a cache SCB back to host (only dirty fields need be flushed) */
void scbtag_flush(void);

/** Find a SCB and populate scbtag if the scb was in host and not cached */
struct scb *scbtag_cache(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea, int bandunit);

struct wlcband *scbtag_band(struct scb *scb);

#else  /* ! BCM_HOST_MEM_SCBTAG */

#define SCBTAG_IS_ACTIVE()              (FALSE)
#define SCBTAG_IS_ENABLED()             (FALSE)
#define SCBTAG_IS_CACHED(scb)           (FALSE)

#define SCBTAG_BSSCFG(scb)              SCB_BSSCFG(scb)
#define SCBTAG_WLCIFP(scb)              SCB_WLCIFP(scb)
#define SCBTAG_INTERFACE(scb)           SCB_INTERFACE(scb)
#define SCBTAG_ISMULTI(scb)             SCB_ISMULTI(scb)
#define SCBTAG_FLAGS(scb)               SCB_FLAGS(scb)
#define SCBTAG_FLAGS2(scb)              SCB_FLAGS2(scb)
#define SCBTAG_FLAGS3(scb)              SCB_FLAGS3(scb)
#define SCBTAG_QOS(scb)                 SCB_QOS(scb)
#define SCBTAG_40_CAP(scb)              SCB_40_CAP(scb)
#define SCBTAG_AMPDU(scb)               SCB_AMPDU(scb)
#define SCBTAG_AMSDU(scb)               SCB_AMSDU(scb)
#define SCBTAG_WME(scb)                 SCB_WME(scb)
#define SCBTAG_LDPC_CAP(scb)            SCB_LDPC_CAP(scb)
#define SCBTAG_A4_DATA(scb)             SCB_A4_DATA(scb)
#define SCBTAG_KEY(scb)                 SCB_KEY(scb)
#define SCBTAG_WDS(scb)                 SCB_WDS(scb)
#define SCBTAG_PS(scb)                  SCB_PS(scb)
#define SCBTAG_EA(scb)			SCB_EA(scb)
#define SCBTAG_CACHE(wlc, bsscfg, ea, bandunit) \
	wlc_scbfindband((wlc), (bsscfg), (ea), (bandunit))

#define SCBTAG_H2D_SYNC(scb, s, f)      SCBTAG_NOOP()
#define SCBTAG_D2H_SYNC(scb, s, f)      SCBTAG_NOOP()

#define SCBTAG_TX_NEXT(fid, scb, pkt, prec) \
	SCB_TX_NEXT((fid), (scb), (pkt), (prec))

#define BCM_HOST_MEM_SCBTAG_INUSE 0

#endif /* ! BCM_HOST_MEM_SCBTAG */

#ifdef WL_CS_RESTRICT_RELEASE
/**
 * Limit number of packets in transit, starting from minimal number.
 * Each time packet sent successfully using primary rate limit is
 * exponentially grow till some number, then unlimited.
 * In case of failure while limit is growing, it fall back to
 * original minimal number.
 */

extern void wlc_scb_restrict_start(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_scb_restrict_wd(wlc_info_t *wlc);

#define SCB_RESTRICT_WD_TIMEOUT		1
#define SCB_RESTRICT_MIN_TXWIN_SHIFT	1
#define SCB_RESTRICT_MAX_TXWIN_SHIFT	6

#define SCB_RESTRICT_MIN_TXWIN		(1 << (SCB_RESTRICT_MIN_TXWIN_SHIFT))
#define SCB_RESTRICT_MAX_TXWIN		(1 << (SCB_RESTRICT_MAX_TXWIN_SHIFT))

static INLINE void
wlc_scb_restrict_txstatus(struct scb *scb, bool success)
{
	if (!scb->restrict_txwin) {
		/* Disabled. Most likely case. */
	} else if (!success) {
		scb->restrict_txwin = SCB_RESTRICT_MIN_TXWIN;
	} else {
		scb->restrict_txwin = scb->restrict_txwin << 1;
		if (scb->restrict_txwin >= SCB_RESTRICT_MAX_TXWIN) {
			scb->restrict_txwin = 0;
		}
	}
}

static INLINE bool
wlc_scb_restrict_can_txq(wlc_info_t *wlc, uint16 restrict_txwin)
{
	if (!restrict_txwin) {
		/* Disabled. Most likely case. Can release single packet. */
		return TRUE;
	} else {
		/*
		 * Return TRUE if number of packets in transit is less then restriction window.
		 * If TRUE caller can release single packet.
		 */
		return (TXPKTPENDTOT(wlc) < restrict_txwin);
	}
}

static INLINE uint16
wlc_scb_restrict_can_ampduq(wlc_info_t *wlc, struct scb *scb, uint16 in_transit, uint16 release)
{
	uint16  restrict_txwin;
	restrict_txwin = scb->restrict_txwin;
	if (!restrict_txwin) {
		/* Disabled. Most likely case. Can release same number of packets as queried. */
		return release;
	} else if (in_transit >= restrict_txwin) {
		/* Already too many packets in transit. Release denied. */
		return 0;
	} else {
		/* Return how many packets can be released. */
		return MIN(release, restrict_txwin - in_transit);
	}
}

static INLINE bool
wlc_scb_restrict_can_txeval(wlc_info_t *wlc)
{
	/*
	 * Whether AMPDU txeval function can proceed or not.
	 * Prevents to release packets into txq if DATA_BLOCK_QUIET set
	 * (preparations to channel switch are in progress).
	 * Idea is in case of point-to-multipoint traffic
	 * better to have restrictions on boundary between AMPDU queue
	 * and txq, so single bad link would not affect much other good links.
	 * And to have this boundary efficient we need nothing at txq
	 * after channel switch, so control between AMPDU queue and txq
	 * would work.
	 */
	return ((wlc->block_datafifo & DATA_BLOCK_QUIET) == 0);
}

static INLINE bool
wlc_scb_restrict_do_probe(struct scb *scb)
{
	/* If restriction is not disabled yet, then frequent probing should be used. */
	return (scb->restrict_txwin != 0);
}

#if defined(BCMPCIEDEV)
static INLINE uint32
wlc_scb_calc_weight(uint32 pktlen_bytes, uint32 rate, bool legacy)
{
#define SCALE 100000
	uint32 rate_KBps;
	uint32 weight;

	if (legacy) {
		/* For legacy rates the input rate unit is
		 * in Mbits/sec multiplied by 2.
		 * Converting to KBytes/sec.
		 * Formula is : rate * 1024 / 16
		 */
		rate_KBps = rate << 6;

	} else {
	   /* For HT and VHT rates the input rate unit is in
		* Kbits/sec. Converting to KBytes/sec.
		* Formula is: rate / 8
		*/
		rate_KBps = rate >> 3;
	}

	pktlen_bytes *= SCALE;
	weight = pktlen_bytes / rate_KBps;
	ASSERT(weight);
	return weight;
#undef SCALE
}
#endif /* BCMPCIEDEV */

#else
static INLINE void wlc_scb_restrict_start(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg) {}
static INLINE void wlc_scb_restrict_wd(wlc_info_t *wlc) {}
static INLINE void wlc_scb_restrict_txstatus(struct scb *scb, bool success) {}
static INLINE bool wlc_scb_restrict_can_txq(wlc_info_t *wlc, uint16 win) {return TRUE;}
static INLINE uint16 wlc_scb_restrict_can_ampduq(wlc_info_t *wlc,
	struct scb *scb, uint16 in_transit, uint16 release) {return release;}
static INLINE bool wlc_scb_restrict_can_txeval(wlc_info_t *wlc) {return TRUE;}
static INLINE bool wlc_scb_restrict_do_probe(struct scb *scb) {return FALSE;}
static INLINE uint32 wlc_scb_calc_weight(uint32 pktlen_bytes, uint32 rate, bool legacy) {return 0;}
#endif /* WL_CS_RESTRICT_RELEASE */

#ifdef PROP_TXSTATUS
extern int wlc_scb_wlfc_entry_add(wlc_info_t *wlc, struct scb *scb);
#endif /* PROP_TXSTATUS */
void wlc_scbfind_delete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,	struct ether_addr *ea);
#endif /* _wlc_scb_h_ */
