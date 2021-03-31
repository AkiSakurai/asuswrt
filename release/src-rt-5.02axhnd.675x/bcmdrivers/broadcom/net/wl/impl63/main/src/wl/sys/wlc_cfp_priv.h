/*
 * Cached Flow Processing module internal header file
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#ifndef _WLC_CFP_PRIV_H_
#define _WLC_CFP_PRIV_H_

struct scb_cfp; /** < forward Declaration */
/**
 * RCB node used to link up scbs with pending packets
 */
typedef struct scb_cfp_rcb_node {
	dll_t	node;  /** < Manage a list of active RCBs in fast path */
	struct scb_cfp	*scb_cfp; /** Back pointer to scb_cfp */
	uint8 prio;
	bool	pending;	/** < RCB on pending list */
} scb_cfp_rcb_node_t;

/**
 * CFP Stats Collection
 */
typedef struct scb_cfp_stats {
	uint32 cfp_pkt_cnt[NUMPRIO];	/**< pkt count processed in CFP path */
	uint32 cfp_mupkt_cnt[NUMPRIO];	/**< MU pkt count processed in CFP path */
	uint32 cfp_tx_legacypkt_cnt[NUMPRIO];	/**< Tx legacy pkt count */
	uint32 cfp_slowpath_cnt[NUMPRIO]; /**< No of times diverted to slow path from CFP path */
	uint32 txq_drain_wait[NUMPRIO];	/**< count of return from CFP path waiting for TXQ drain */
	uint32 rx_chained_pktcnt[NUMPRIO]; /** < Packet Stats */
	uint32 rx_un_chained_pktcnt[NUMPRIO]; /** < Packet Stats */
} scb_cfp_stats_t;
/**
 * Cached Flow Processing Transmit Control Block.
 * Runtime TCB state and cached pointers to various SCB Tx cubbies.
 */
typedef struct scb_cfp_tcb {
	uint8	state[NUMPRIO]; /**< Current per prio state of TCB */
	void	*scb_txc_info;  /**< SCB TxCache cubby [scb_txc_info_t] */
	void	*scb_ampdu_tx;  /**< SCB-AMPDU cubby [scb_ampdu_tx_t] */
	                            /**< Pointer to Initiators goes here */
} scb_cfp_tcb_t;

/**
 * Cached Flow Processing Recieve Control Block
 * Runtime RCB state and cached pointers to various SCB Rx cubbies.
 */
typedef struct scb_cfp_rcb {
	uint8		state[NUMPRIO];		/**< Current per prio state of RCB */
	scb_cfp_rcb_node_t rcb_node[NUMPRIO];	/** < Per prio RCB node */
	void		*scb_ampdu_rx;		/**< SCB-AMPDU Rx cubby [scb_ampdu_rx_t] */
	struct pktq	chained_pktq;		/**< Per priority chained packet Q */
} scb_cfp_rcb_t;

/** SCB-CFP cubby */
typedef struct scb_cfp {
	wlc_key_algo_t key_algo;    /**< keymgmt algorithm used [8 bits] */
	uint8	    iv_len;	    /**< keymgmt iv length */
	uint8	    icv_len;	    /**< keymgmt icv length */
	uint8	    cache_gen;	    /**< unique number incremented everytime cache is updated */
	uint16      flags;          /**< Generic flags */
	void        *scb;           /**< Cached scb pointer [struct scb ] */
	void        *cfg;           /**< Store cfg pointer [wlc_bsscfg_t] */
	void        *scb_amsdu;     /**< SCB-AMSDU cubby [scb_amsdu_t] */
	void	    *key;	    /**< Pvt key info pulled from keymgmt */
	scb_cfp_tcb_t tcb;          /**< Transmit Control Block in scb-cfp */
	scb_cfp_rcb_t rcb;          /**< Recieve Control Block in scb-cfp */
	scb_cfp_stats_t stats;	    /**< stats collection in CFP */
	bool        ps_send;        /**< Transmit permission when PS is on */
} scb_cfp_t;

#define SCB_CFP_SIZE          sizeof(scb_cfp_t)

/** SCB CFP Cubby Utility Macros */

/** SCB CFP Cubby Accessors given a scb_cfp_t handle. Use these Accessors! */
#define SCB_CFP_FLAGS(hdl)              ((hdl)->flags)
#define SCB_CFP_SCB(hdl)                ((hdl)->scb)
#define SCB_CFP_CFG(hdl)                ((hdl)->cfg)
#define SCB_CFP_AMSDU_CUBBY(hdl)        ((hdl)->scb_amsdu)
#define SCB_CFP_TCB(hdl)                (&((hdl)->tcb))
#define SCB_CFP_RCB(hdl)                (&((hdl)->rcb))
#define SCB_CFP_KEY(hdl)		((hdl)->key)
#define SCB_CFP_KEY_ALGO(hdl)		((hdl)->key_algo)
#define SCB_CFP_IV_LEN(hdl)		((hdl)->iv_len)
#define SCB_CFP_ICV_LEN(hdl)		((hdl)->icv_len)
#define SCB_CFP_CACHE_GEN(hdl)		((hdl)->cache_gen)
#define SCB_CFP_STATS(hdl)		((hdl)->stats)
#define SCB_CFP_PS_SEND(hdl)		((hdl)->ps_send)
#define _SCB_CFP_STATE(hdl, prio)       ((hdl)->state[prio])

/** SCB CFP STATS Accessors */
#define SCB_CFP_PKT_CNT(hdl, prio)	(SCB_CFP_STATS(hdl).cfp_pkt_cnt[(prio)])
#define SCB_CFP_MUPKT_CNT(hdl, prio)	(SCB_CFP_STATS(hdl).cfp_mupkt_cnt[prio])
#define SCB_CFP_TX_LEGACYPKT_CNT(hdl, prio)	(SCB_CFP_STATS(hdl).cfp_tx_legacypkt_cnt[prio])
#define SCB_CFP_SLOWPATH_CNT(hdl, prio)	(SCB_CFP_STATS(hdl).cfp_slowpath_cnt[(prio)])
#define SCB_CFP_TXQ_DRAIN_WAIT(hdl, prio) (SCB_CFP_STATS(hdl).txq_drain_wait[(prio)])
#define SCB_CFP_RX_CHAINED_CNT(hdl, prio) (SCB_CFP_STATS(hdl).rx_chained_pktcnt[(prio)])
#define SCB_CFP_RX_UN_CHAINED_CNT(hdl, prio) (SCB_CFP_STATS(hdl).rx_un_chained_pktcnt[(prio)])
#define SCB_CFP_STATS_INCR(item)	((item)++)

/** SCB CFP TCB Accessors */
#define SCB_CFP_TCB_STATE(hdl, prio)    (_SCB_CFP_STATE(SCB_CFP_TCB(hdl), prio))
#define SCB_CFP_AMPDU_TX(hdl)           ((SCB_CFP_TCB(hdl))->scb_ampdu_tx)
#define SCB_CFP_TXC_INFO(hdl)           ((SCB_CFP_TCB(hdl))->scb_txc_info)
#define SCB_CFP_TCB_STATE_PTR(hdl)      ((SCB_CFP_TCB(hdl))->state)

/** SCB CFP RCB Accessors */
#define SCB_CFP_RCB_STATE(hdl, prio)    (_SCB_CFP_STATE(SCB_CFP_RCB(hdl), (prio)))
#define SCB_CFP_AMPDU_RX(hdl)           ((SCB_CFP_RCB(hdl))->scb_ampdu_rx)
#define SCB_CFP_RCB_STATE_PTR(hdl)	((SCB_CFP_RCB(hdl))->state)
#define SCB_CFP_RCB_CHAINEDQ(hdl)	(&((SCB_CFP_RCB(hdl))->chained_pktq))
#define SCB_CFP_RCB_DLL_NODE(hdl, prio)	(&((SCB_CFP_RCB(hdl))->rcb_node[(prio)]))
#define SCB_CFP_RCB_NODE_IS_PENDING(hdl, prio) ((SCB_CFP_RCB_DLL_NODE((hdl), (prio)))->pending)

/**
 * TCB State Management ; Current states available are
 * 1. AMPDU initiator
 * 2. TX Cache
 * 3. SCB CFP Pause
 * 4. DATA Fifo Block
 */
#define SCB_CFP_TCB_INI		(0x01)		/* Bit position in state for ini */
#define SCB_CFP_TCB_CACHE	(0x02)		/* Bit position in state for cache */
#define SCB_CFP_TCB_PAUSED	(0x04)		/* Bit position in state for paused state
						 * This could be any used as a generic way
						 * to suspend CFP flow operation for a scb
						 * from any layer or from user
						 */
/**
 * Check if cache valid for cfp
 * Check on any prio [0 .. 7] since cache state is common across all prio
 */
#define SCB_CFP_TCB_IS_CACHE_VALID(hdl) \
	((SCB_CFP_TCB_STATE(hdl, 0) & SCB_CFP_TCB_CACHE) == 0)

/* Ini Updates */
#define SCB_CFP_TCB_SET_INI_VALID(hdl, prio) \
	SCB_CFP_TCB_STATE(hdl, prio) &= ~SCB_CFP_TCB_INI
#define SCB_CFP_TCB_SET_INI_INVALID(hdl, prio) \
	({\
		SCB_CFP_TCB_STATE(hdl, prio) |= SCB_CFP_TCB_INI; \
	})
#define SCB_CFP_TCB_IS_INI_VALID(hdl, prio) \
	((SCB_CFP_TCB_STATE(hdl, prio) & SCB_CFP_TCB_INI) == 0)

/** Various states of a Cached Flow Processing Transmit Control Block */
typedef enum wlc_cfp_cubby_tcb_state {
	CFP_TCB_STATE_ESTABLISHED = 0,
	CFP_TCB_STATE_NEW = (SCB_CFP_TCB_CACHE | SCB_CFP_TCB_INI),
	CFP_TCB_STATE_LAST
} wlc_cfp_cubby_tcb_state_t;

/* TCB EST state */
#define SCB_CFP_TCB_IS_EST(scb_cfp, prio) \
	((SCB_CFP_TCB_STATE((scb_cfp), (prio))) == CFP_TCB_STATE_ESTABLISHED)

/**
 * RCB STATE MANAGEMENT
 * Available states for RCB is as follows
 * 1. Responder available
 */
#define SCB_CFP_RCB_RESPONDER	(0x1)		/* Bit position in state for resp */
#define SCB_CFP_RCB_PAUSE	(0x2)		/* Temporary pause state */

/* Responder Updates */
#define SCB_CFP_RCB_SET_RESP_VALID(hdl, prio) \
	SCB_CFP_RCB_STATE((hdl), (prio)) &= (~SCB_CFP_RCB_RESPONDER)
#define SCB_CFP_RCB_SET_RESP_INVALID(hdl, prio) \
	SCB_CFP_RCB_STATE((hdl), (prio)) |= (SCB_CFP_RCB_RESPONDER)

/* RCB Pause states */
#define SCB_CFP_RCB_SET_PAUSE(hdl, prio) \
	SCB_CFP_RCB_STATE((hdl), (prio)) |= (SCB_CFP_RCB_PAUSE)
#define SCB_CFP_RCB_RESET_PAUSE(hdl, prio) \
	SCB_CFP_RCB_STATE((hdl), (prio)) &= (~SCB_CFP_RCB_PAUSE)

/** Various states of a Cached Flow Processing Recieve Control Block */
typedef enum wlc_cfp_cubby_rcb_state {
	CFP_RCB_STATE_ESTABLISHED = 0,
	CFP_RCB_STATE_NEW = (SCB_CFP_RCB_RESPONDER),
	CFP_RCB_STATE_LAST
} wlc_cfp_cubby_rcb_state_t;

/* RCB Established state */
#define SCB_CFP_RCB_IS_EST(scb_cfp, prio) \
	((SCB_CFP_RCB_STATE((scb_cfp), (prio))) == CFP_RCB_STATE_ESTABLISHED)

#define SCB_CFP_RCB_IS_OOO(scb_cfp, prio) \
	((SCB_CFP_RCB_STATE((scb_cfp), (prio))) == CFP_RCB_STATE_OOO)

/**
 * SCB_CFP flag definitions
 * to remain private to wlc_cfp.c  for various state management
 */
#define SCB_CFP_FLAGS_TX_ENABLE		(0x0001)	/* CFP TX capable scb cubby */
#define SCB_CFP_FLAGS_RX_ENABLE		(0x0002)	/* CFP TX capable scb cubby */

/* CFP TX capable flow */
#define SCB_CFP_TX_ENABLE(hdl)		\
		((SCB_CFP_FLAGS(hdl)) |= SCB_CFP_FLAGS_TX_ENABLE)
#define SCB_CFP_TX_DISABLE(hdl)		\
		((SCB_CFP_FLAGS(hdl)) &= (~SCB_CFP_FLAGS_TX_ENABLE))
#define	SCB_CFP_TX_ENABLED(hdl)		\
		((hdl) ? ((SCB_CFP_FLAGS(hdl)) & (SCB_CFP_FLAGS_TX_ENABLE)) : FALSE)

/* CFP RX capable flow */
#define SCB_CFP_RX_ENABLE(hdl)		\
		((SCB_CFP_FLAGS(hdl)) |= SCB_CFP_FLAGS_RX_ENABLE)
#define SCB_CFP_RX_DISABLE(hdl)		\
		((SCB_CFP_FLAGS(hdl)) &= (~SCB_CFP_FLAGS_RX_ENABLE))
#define	SCB_CFP_RX_ENABLED(hdl)		\
		((hdl) ? ((SCB_CFP_FLAGS(hdl)) & (SCB_CFP_FLAGS_RX_ENABLE)) : FALSE)

/** Cached Flow Processing module's private structure */
typedef struct wlc_cfp_priv {
	wlc_info_t      *wlc;       /**< Back pointer to wl */
	int             scb_hdl;    /**< SCB cubby handle */
} wlc_cfp_priv_t;

/** Cached Flow Processing module */
typedef struct wlc_cfp {
	wlc_cfp_info_t info;        /**< Public exported structure */
	wlc_cfp_priv_t priv;        /**< Private structure */
} wlc_cfp_t;

#define WLC_CFP_SIZE          (sizeof(wlc_cfp_t))

#define BCM_CONTAINER_OF(ptr, type, member) \
	({ \
		typeof(((type *)0)->member) *_mptr = (ptr); \
		(type *)((char *)_mptr - OFFSETOF(type, member)); \
	})

/** Cached Flow Processing module public and private structures */
#define WLC_CFP_INFO(cfp)     (&(((wlc_cfp_t *)cfp)->info))
#define WLC_CFP_PRIV(cfp)     (&(((wlc_cfp_t *)cfp)->priv))

/** Cached flow processing module private structures */
#define WLC_CFP_WLC(cfp)      (WLC_CFP_PRIV(cfp)->wlc)

/** Fetch CFP module pointer from pointer to module's public structure. */
#define WLC_CFP(cfp_info) \
	({ \
		struct wlc_cfp *cfp_module = \
		    (struct wlc_cfp *)BCM_CONTAINER_OF((cfp_info), wlc_cfp_t, info); \
		cfp_module; \
	})

#endif /* _WLC_CFP_PRIV_H_ */
