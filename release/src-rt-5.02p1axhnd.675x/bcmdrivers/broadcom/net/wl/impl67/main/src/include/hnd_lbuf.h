/*
 * HND generic packet buffer definitions.
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
 * $Id: hnd_lbuf.h 785918 2020-04-09 02:08:19Z $
 */

#ifndef _hnd_lbuf_h_
#define _hnd_lbuf_h_

#include <typedefs.h>
#include <hnd_lbuf_cmn.h>

#ifdef BCMHWAPP

#include <hnd_pp_lbuf.h>

#else

/* Total maximum packet buffer size including lbuf header */
#define MAXPKTBUFSZ         (MAXPKTDATABUFSZ + LBUFSZ)

/**
 * Sample : Refer to per chip .mk for Max packet sizes and data buffer sizing.
 * TxLFrag: [HWA2.1 hwapkt_t 44][Lbuf_frag 96][PSM TxD 36][D11Hdr 50]
 * RxLFrag: [Lbuf_frag 96][RPH 12][SwRxSts 12][HwRxSts 40][PLCP 14][Pad 4]
 *          [D11 50][CopyCnt 16]
 * Notes:
 * - TxLfrag: lbuf_frag and data buffer may be discrete (data buffers are not
 *   contiguous with lbuf_frag. Data buffer may be allocated from a pool
 *   (D11) or heap.
 *   In HWA2.1, a hwapkt_t structure (hwa_defs.h) preceeds a Lfrag. HWA2.1
 *   supported Aggregated CWI based TxPost, requiring the placement of the
 *   pkttag at the start of the Lbuf (i.e. trailing the hwapkt preamble). ACWI
 *   is discontinued.
 *   Chipsets with HW RLMem, will use a shorter PSM TxD (36 B instead of 124 B)
 * - RxLfrag: The LFrag and data buffer are contiguous.
 * - In HWA2.2, both Tx and Rx packets are managed by HW PktPgr, and a single
 *   HW managed Pkt Pool is used for both Tx and Rx, with a Packet including the
 *   Lfrag and the Data buffer, limited to 256 Bytes.
 */

#ifndef MAXPKTFRAGSZ
#define MAXPKTFRAGSZ        342
#endif // endif

#ifndef MAXPKTRXFRAGSZ
#define MAXPKTRXFRAGSZ      256
#endif // endif

/* Packet tag size, in units of 32 bit words  */
#define LBUF_PKTTAG_U32     ((OSL_PKTTAG_SZ + 3) / 4)

/**
 * In BCMPKTIDMAP, an lbuf's 16bit ID is used instead of a 32bit lbuf pointer,
 * with a cost of a ID to pointer conversion using an indexed lookup into
 * hnd_pktptr_map[].
 * When BCMPKTIDMAP is not defined, the lbuf::link pointer is used to link the
 * packets in the free pool, using PKTFREELIST() and PKTSETFRELIST() macros.
 * These macros use the lbuf::link. However, when BCMPKTIDMAP is defined, the
 * use of PKTLINK()/PKTSETLINK() would require an unnecessary mapping of
 * linkidx to the linked pointer. Hence a 32bit freelist pointer
 * (combined 16bit nextid and linkid) is used without any id to ptr mapping.
 *
 * lb_resetpool, relies on the lbuf structure layout as an optimization to
 * otherwise having to save, bzero and restore persistent or fields that will
 * be explicitly set.
 */
struct lbuf {
#if defined(BCMHWA) && defined(HWA_PKT_MACRO)
	uint32      pkttag[LBUF_PKTTAG_U32];

	uchar       *head;      /**< fixed start of buffer */
	uchar       *end;       /**< fixed end of buffer */
	uchar       *data;      /**< variable start of data */
	uint16      len;        /**< nbytes of data */
	uint16      cfp_flowid; /**< WLCCFP: Cached Flow Processing FlowId */
#endif /* BCMHWA && HWA_PKT_MACRO */

	union {
		uint32      u32;
		struct {
			uint16  pktid;          /**< unique packet ID */
			uint8   refcnt;         /**< external references to this lbuf */
#if defined(BCMDBG_POOL)
			uint8   poolstate : 4;  /**< BCMDBG_POOL stats collection */
			uint8   poolid    : 4;  /**< pktpool ID */
#else
			uint8   poolid;         /**< entire byte is used for performance */
#endif // endif
		};
	} mem;                  /**< Lbuf memory management context */

#if !defined(BCMHWA) || !defined(HWA_PKT_MACRO)
	uchar       *head;      /**< fixed start of buffer */
	uchar       *end;       /**< fixed end of buffer */
	uchar       *data;      /**< variable start of data */
	uint16      len;        /**< nbytes of data */
	uint16      cfp_flowid; /**< WLCCFP: Cached Flow Processing FlowId */
#endif /* !BCMHWA || !HWA_PKT_MACRO */

	uint32      flags;      /**< private flags; don't touch */

	/* ----- following fields explicitly bzeroed in lb_resetpool ----- */

	union {
		uint32  reset;      /**< recycling to reset pool, zeros these */
		struct {
			union {
				uint16 dmapad;      /**< padding to be added for tx dma */
				uint16 rxcpl_id;    /**< rx completion ID for AMPDU reordering */
				uint16 dma_index;   /**< index into DMA ring */
			};
			union {
				uint8  dataOff;     /**< offset to data, in 4-byte words */
				uint8  hwa_rxOff;   /** HWA RX buffer offset */
			};
			uint8  ifidx;
		};
	};

#if defined(BCMPKTIDMAP)
	union {
		struct {
			uint16 nextid;  /**< list of packet fragments, e.g. MSDUs in MPDU */
			uint16 linkid;  /**< list of packets, e.g. MPDUs */
		};
		void * freelist;    /**< linked list of free lbufs in pktpool */
	};
#else  /* ! BCMPKTIDMAP */
	struct lbuf *next;      /**< list of packet fragments, e.g. MSDUs in MPDU */
	struct lbuf *link;      /**< list of packets, e.g. MPDUs */
#endif /* ! BCMPKTIDMAP */

#if defined(HNDPQP) && !defined(BCMHWA)
	struct {                /* INTERNAL NO-HWA mode debug: hwapkt PQP fields */
		uint16 hbm_pktnextid;    /**< HNDPQP: PQP Host BM PKTNEXT */
		uint16 hbm_pktlinkid;    /**< HNDPQP: PQP Host BM PKTLINK */
	} pqp_list;
	union {
		struct {
			uint16 hbm_pktid;   /**< HNDPQP: PQP Host BM PKTID */
			uint16 hbm_lclid;   /**< HNDPQP: PQP Host BM LCLID */
		} pqp_mem;
		uint32 pqp_hbm_page;    /**< HNDPQP: PQP Host BM page ids */
	};
#endif /* HNDPQP && !BCMHWA */
#if !defined(BCMHWA) || !defined(HWA_PKT_MACRO)
	uint32  pkttag[LBUF_PKTTAG_U32];
#endif // endif

}; /* end of struct lbuf */

#define LBP(lb)         ((struct lbuf *)(lb))
#define LBUFSZ          ((int)sizeof(struct lbuf))

/* Accessors in a lbuf */
#define LB_PKTID(lb)    (LBP(lb)->mem.pktid)
#define LB_HEAD(lb)     (LBP(lb)->head)
#define LB_DATA(lb)     (LBP(lb)->data)
#define LB_END(lb)      (LBP(lb)->end)

/* lbuf clone structure
 * if lbuf->flags LBF_CLONE bit is set, the lbuf can be cast to an lbuf_clone
 */
struct lbuf_clone {
	struct lbuf lbuf;
	struct lbuf *orig;  /**< original non-clone lbuf providing the buffer space */
};

/**
 * lbuf with fragment. Related to fragments residing on the host.
 *
 * Fragix=0 implies internally stored data. Fragix=1..LB_FRAG_MAX refers to
 * externally stored data, that is not directly accessible.
 *
 * LB_FRAG_MAX: a fraglist sized for 2 or 4 fragments to serve the typical case when packets are
 * composed of less than 4 fragmented payloads in host memory, may be deployed.
 */
#define LB_FRAG_MAX     2   /**< Typical number of host memory fragments */
#define LB_FRAG_CTX     0   /**< Index 0 in lbuf_flist saves context */
#define LB_FRAG1        1
#define LB_FRAG2        2

/* LB_FRAB_MAX needs to be an even number, so please make sure that is the case */
#if (LB_FRAG_MAX & 1)
#error "LB_FRAG_MAX need to be an even number"
#endif // endif

/** information on a packet fragment residing in host memory */
struct lbuf_finfo {
	union {
		struct {                /**< element 0 in array of lbuf_finfo */
			uint32  pktid;      /**< unique opaque id to host packet */
			uint8   fnum;       /**< total number of fragments in this lbuf */
			uint8   lfrag_flags; /**< generic flag information of lfrag */
			uint16  flowid;
		} ctx;

		struct {                /**< element 1..LB_FRAGS_MAX */
			uint32  data_lo;
			uint32  data_hi;
		} frag;                 /**< 64b address of host data fragment */

		struct {
			uint8   key_seq[6];
			uint8   pktflags;
			uint8   txstatus;
		};
		uint8       fc_tlv[FC_TLV_SIZE];      /**< storage for WLFC TLV */
	};
};

/**
 * The fraglist is an array of elements describing the fragments in the host. A set of PKTFRAG
 * macros are provided to access the lbuf_flist fields.
 */
struct lbuf_flist {
	struct lbuf_finfo finfo[LB_FRAG_MAX + 1]; /**< Element finfo[0] is context */
	uint16 flen[LB_FRAG_MAX + 1];             /**< Element flen[0] is total fragment length */
	uint16 ring_idx; /**< TxLfrag: flowring index */
};

/**
 * Lbuf extended with fragment list, introduced to save dongle RAM. Without lbuf_frag, each packet
 * constituted of 2 fragments would have consumed 2 Lbufs linked together for total memory of
 * 3800 bytes.
 */
struct lbuf_frag {
	/**
	 * lbuf: allows lbuf_frags to be chained in case a packet on the hosts occupies more than
	 * LB_FRAG_MAX fragments. In a chain of lbuf_frag, only the first lbuf_frag will include an
	 * internal storage. Linked lbuf_frag(s) need not have internal storage (headroom/data) and
	 * have the lbuf::data=NULL and lbuf:len = 0.
	 */
	struct lbuf         lbuf;
	/** flist: array of information on fragments in host memory */
	struct lbuf_flist   flist;
};

#define LBFP(lb)    ((struct lbuf_frag *)(lb))
#define LBUFFRAGSZ  ((int)sizeof(struct lbuf_frag))

#if defined(BCMHWA) && defined(HWA_PKT_MACRO)
#define LBFCTX(lb)  (LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx)
#endif // endif

/* prototypes */
extern struct lbuf *lb_init(struct lbuf *lb, enum lbuf_type lb_type, uint lb_sz);
#if defined(BCMDBG_MEMFAIL)
extern struct lbuf *lb_alloc_header(void *data, uint size, enum lbuf_type lbuf_type,
    void *call_site);
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbuf_type, void *call_site);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len, void *call_site);
extern struct lbuf *lb_dup(struct lbuf *lb, void *call_site);
#else
extern struct lbuf *lb_alloc_header(void *data, uint size, enum lbuf_type lbuf_type);
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbtype);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len);
extern struct lbuf *lb_dup(struct lbuf *lb);
#endif // endif

extern void lb_free(struct lbuf *lb);
extern bool lb_sane(struct lbuf *lb);
extern void lb_audit(struct lbuf *lb);
extern void lb_resetpool(struct lbuf *lb, uint16 len);
#ifdef BCMDBG
extern void lb_dump(void);
#endif // endif
void lb_audit(struct lbuf * lb);

typedef bool (*lbuf_free_cb_t)(void* arg, void* p);
extern void lbuf_free_register(lbuf_free_cb_t cb, void* arg);

typedef void (*lbuf_free_global_cb_t)(struct lbuf *lb);
extern void lbuf_free_cb_set(lbuf_free_global_cb_t lbuf_free_cb);
extern void lb_clear_pkttag(struct lbuf *lb);

#ifdef BCM_DHDHDR
/* Attach data buffer to lbuf */
extern void lb_set_buf(struct lbuf *lb, void *buf, uint size);
#endif /* BCM_DHDHDR */

#ifdef HNDLBUF_USE_MACROS
/* GNU macro versions avoid the -fno-inline used in ROM builds. */
#define lb_head(lb) ({ \
	ASSERT(lb_sane(lb)); \
	LB_HEAD(lb); \
})

#define lb_end(lb) ({ \
	ASSERT(lb_sane(lb)); \
	LB_END(lb); \
})

#define lb_push(lb, _len) ({ \
	uint __len = (_len); \
	ASSERT(lb_sane(lb)); \
	ASSERT(((lb)->data - __len) >= LB_HEAD(lb)); \
	(lb)->data -= __len; \
	(lb)->len += __len; \
	(lb)->data; \
})

#define lb_pull(lb, _len) ({ \
	uint __len = (_len); \
	ASSERT(lb_sane(lb)); \
	ASSERT(__len <= (lb)->len); \
	(lb)->data += __len; \
	(lb)->len -= __len; \
	(lb)->data; \
})

#define lb_setlen(lb, _len) ({ \
	uint __len = (_len); \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->data + __len <= LB_END(lb)); \
	(lb)->len = (__len); \
})

#define lb_pri(lb) ({ \
	ASSERT(lb_sane(lb)); \
	((lb)->flags & LBF_PRI); \
})

#define lb_setpri(lb, pri) ({ \
	uint _pri = (pri); \
	ASSERT(lb_sane(lb)); \
	ASSERT((_pri & LBF_PRI) == _pri); \
	(lb)->flags = ((lb)->flags & ~LBF_PRI) | (_pri & LBF_PRI); \
})

#define lb_sumneeded(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_SUM_NEEDED) != 0); \
})

#define lb_setsumneeded(lb, summed) ({ \
	ASSERT(lb_sane(lb)); \
	if (summed) \
		(lb)->flags |= LBF_SUM_NEEDED; \
	else \
		(lb)->flags &= ~LBF_SUM_NEEDED; \
})

#define lb_sumgood(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_SUM_GOOD) != 0); \
})

#define lb_setsumgood(lb, summed) ({ \
	ASSERT(lb_sane(lb)); \
	if (summed) \
		(lb)->flags |= LBF_SUM_GOOD; \
	else \
		(lb)->flags &= ~LBF_SUM_GOOD; \
})

#define lb_msgtrace(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_MSGTRACE) != 0); \
})

#define lb_setmsgtrace(lb, set) ({ \
	ASSERT(lb_sane(lb)); \
	if (set) \
		(lb)->flags |= LBF_MSGTRACE; \
	else \
		(lb)->flags &= ~LBF_MSGTRACE; \
})

#define lb_dataoff(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->dataOff; \
})

#define lb_setdataoff(lb, _dataOff) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->dataOff = _dataOff; \
})

#define lb_isclone(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_CLONE) != 0); \
})

#define lb_is_txfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_TX_FRAG) != 0); \
})

#define lb_set_txfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_TX_FRAG; \
})

#define lb_reset_txfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_TX_FRAG; \
})

#define lb_is_rxfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_RX_FRAG) != 0); \
})

#define lb_set_rxfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_RX_FRAG; \
})
#define lb_reset_rxfrag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_RX_FRAG; \
})
#define lb_is_frag(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & (LBF_TX_FRAG | LBF_RX_FRAG)) != 0); \
})

/** Cache Flow Processing lbuf macros */
#define lb_is_cfp(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_CFP_PKT) != 0); \
})

#define lb_get_cfp_flowid(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->cfp_flowid; \
})

#define lb_set_cfp_flowid(lb, flowid) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_CFP_PKT; \
	(lb)->cfp_flowid = (flowid); \
})

#define lb_clr_cfp_flowid(lb, flowid) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_CFP_PKT; \
	(lb)->cfp_flowid = (flowid); \
})

#define lb_isptblk(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_PTBLK) != 0); \
})

#define lb_setpktid(lb, id) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((id) <= PKT_MAXIMUM_ID); \
	(lb)->mem.pktid = (id); \
})

#define lb_getpktid(lb) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->mem.pktid <= PKT_MAXIMUM_ID); \
	(lb)->mem.pktid; \
})

#define lb_nodrop(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_PKT_NODROP) != 0); \
})

#define lb_setnodrop(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_PKT_NODROP; \
})

#define lb_typeevent(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_PKT_EVENT) != 0); \
})

#define lb_settypeevent(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_PKT_EVENT; \
})

#define lb_dot3_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_DOT3_PKT) != 0); \
})

#define lb_set_dot3_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_DOT3_PKT; \
})

#define lb_ischained(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_CHAINED) != 0); \
})

#define lb_setchained(lb) ({ \
	ASSERT(lb_sane(lb)); \
	((lb)->flags |= LBF_CHAINED); \
})

#define lb_clearchained(lb) ({ \
	ASSERT(lb_sane(lb)); \
	((lb)->flags &= ~LBF_CHAINED); \
})

#define lb_has_metadata(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_METADATA) != 0); \
})

#define lb_set_has_metadata(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_METADATA; \
})
#define lb_reset_has_metadata(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_METADATA; \
})

#define lb_has_heapbuf(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_HEAPBUF) != 0); \
})

#define lb_set_heapbuf(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_HEAPBUF; \
})

#define lb_clr_heapbuf(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_HEAPBUF; \
})

#define lb_set_norxcpl(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_NORXCPL; \
})

#define lb_clr_norxcpl(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_NORXCPL; \
})

#define lb_need_rxcpl(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_NORXCPL) == 0); \
})

#define lb_altinterface(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_ALT_INTF) != 0); \
})

#define lb_set_pktfetched(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_PKTFETCHED; \
})

#define lb_reset_pktfetched(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_PKTFETCHED; \
})

#define lb_is_pktfetched(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_PKTFETCHED) != 0); \
})

#define lb_set_frwd_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_FRWD_PKT; \
})

#define lb_reset_frwd_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_FRWD_PKT; \
})

#define lb_is_frwd_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_FRWD_PKT) != 0); \
})

#define lb_setaltinterface(lb, set) ({ \
	ASSERT(lb_sane(lb)); \
	if (set) { \
		(lb)->flags |= LBF_ALT_INTF; \
	} else { \
		(lb)->flags &= ~LBF_ALT_INTF; \
	} \
})

#define lb_is_hwa_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_HWA_PKT) != 0); \
})

#define lb_set_hwa_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_HWA_PKT; \
})

#define lb_reset_hwa_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_HWA_PKT; \
})

#define lb_hwa_rxOff(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->hwa_rxOff; \
})

#define lb_sethwa_rxOff(lb, offset) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->hwa_rxOff = offset; \
})

#define lb_is_buf_alloc(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_BUF_ALLOC) != 0); \
})

#define lb_set_buf_alloc(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_BUF_ALLOC; \
})

#define lb_reset_buf_alloc(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_BUF_ALLOC; \
})

#define lb_is_hwa_3bpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_HWA_3BPKT) != 0); \
})

#define lb_set_hwa_3bpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_HWA_3BPKT; \
})

#define lb_reset_hwa_3bpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_HWA_3BPKT; \
})

#define lb_is_mgmt_tx_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_MGMT_TX_PKT) != 0); \
})

#define lb_set_mgmt_tx_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_MGMT_TX_PKT; \
})

#define lb_reset_mgmt_tx_pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_MGMT_TX_PKT; \
})

#define lb_is_hwa_hostreorder(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_HWA_HOSTREORDER) != 0); \
})

#define lb_set_hwa_hostreorder(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_HWA_HOSTREORDER; \
})

#define lb_reset_hwa_hostreorder(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_HWA_HOSTREORDER; \
})

#else /* !HNDLBUF_USE_MACROS */

/* GNU macro versions avoid the -fno-inline used in ROM builds. */
static inline uchar *
lb_head(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return LB_HEAD(lb);
}

static inline uchar *
lb_end(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return LB_END(lb);
}

static inline uchar *
lb_push(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT((lb->data - len) >= LB_HEAD(lb));
	lb->data -= len;
	lb->len += len;
	return (lb->data);
}

static inline uchar *
lb_pull(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT(len <= lb->len);
	lb->data += len;
	lb->len -= len;
	return (lb->data);
}

static inline void
lb_setlen(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->data + len <= LB_END(lb));
	lb->len = len;
}

static inline uint
lb_pri(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PRI);
}

static inline void
lb_setpri(struct lbuf *lb, uint pri)
{
	ASSERT(lb_sane(lb));
	ASSERT((pri & LBF_PRI) == pri);
	lb->flags = (lb->flags & ~LBF_PRI) | (pri & LBF_PRI);
}

static inline bool
lb_sumneeded(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_SUM_NEEDED) != 0);
}

static inline void
lb_setsumneeded(struct lbuf *lb, bool summed)
{
	ASSERT(lb_sane(lb));
	if (summed)
		lb->flags |= LBF_SUM_NEEDED;
	else
		lb->flags &= ~LBF_SUM_NEEDED;
}

static inline bool
lb_sumgood(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_SUM_GOOD) != 0);
}

static inline void
lb_setsumgood(struct lbuf *lb, bool summed)
{
	ASSERT(lb_sane(lb));
	if (summed)
		lb->flags |= LBF_SUM_GOOD;
	else
		lb->flags &= ~LBF_SUM_GOOD;
}

static inline bool
lb_msgtrace(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_MSGTRACE) != 0);
}

static inline void
lb_setmsgtrace(struct lbuf *lb, bool set)
{
	ASSERT(lb_sane(lb));
	if (set)
		lb->flags |= LBF_MSGTRACE;
	else
		lb->flags &= ~LBF_MSGTRACE;
}

/* get Data Offset */
static inline uint8
lb_dataoff(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->dataOff);
}

/* set Data Offset */
static inline void
lb_setdataoff(struct lbuf *lb, uint8 dataOff)
{
	ASSERT(lb_sane(lb));
	lb->dataOff = dataOff;
}

static inline int
lb_isclone(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_CLONE) != 0);
}

static inline int
lb_is_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_TX_FRAG) != 0);
}

static inline void
lb_set_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_TX_FRAG;
}

static inline void
lb_reset_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_TX_FRAG;
}

static inline int
lb_is_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_RX_FRAG) != 0);
}
static inline void
lb_set_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_RX_FRAG;
}
static inline void
lb_reset_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_RX_FRAG;
}
static inline int
lb_is_frag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & (LBF_TX_FRAG | LBF_RX_FRAG)) != 0);
}

/** Cache Flow Processing lbuf inline functions */
static inline int
lb_is_cfp(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_CFP_PKT) != 0);
}

static inline uint16
lb_get_cfp_flowid(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return lb->cfp_flowid;
}

static inline void
lb_set_cfp_flowid(struct lbuf *lb, uint16 flowid)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_CFP_PKT;
	lb->cfp_flowid = flowid;
}

static inline void
lb_clr_cfp_flowid(struct lbuf *lb, uint16 flowid)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_CFP_PKT;
	lb->cfp_flowid = flowid;
}

static inline int
lb_isptblk(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_PTBLK) != 0);
}

#if defined(BCMPKTIDMAP)
static inline void
lb_setpktid(struct lbuf *lb, uint16 pktid)
{
	ASSERT(lb_sane(lb));
	ASSERT(pktid <= PKT_MAXIMUM_ID);
	lb->mem.pktid = pktid;
}

static inline uint16
lb_getpktid(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->mem.pktid <= PKT_MAXIMUM_ID);
	return lb->mem.pktid;
}
#endif /* BCMPKTIDMAP */

static inline uint
lb_nodrop(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PKT_NODROP);
}

static inline void
lb_setnodrop(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_PKT_NODROP;
}
static inline uint
lb_typeevent(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PKT_EVENT);
}

static inline void
lb_settypeevent(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_PKT_EVENT;
}

static inline bool
lb_dot3_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_DOT3_PKT) != 0);
}

static inline void
lb_set_dot3_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_DOT3_PKT;
}

static inline bool
lb_ischained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_CHAINED) != 0);
}

static inline void
lb_setchained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_CHAINED;
}

static inline void
lb_clearchained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_CHAINED;
}

static inline bool
lb_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_METADATA) != 0);
}

static inline void
lb_set_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_METADATA;
}

static inline void
lb_reset_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_METADATA;
}

static inline bool
lb_has_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (((lb)->flags & LBF_HEAPBUF) != 0);
}

static inline void
lb_set_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags |= LBF_HEAPBUF;
}

static inline void
lb_clr_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_HEAPBUF;
}

static inline void
lb_set_norxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags |= LBF_NORXCPL;
}

static inline void
lb_clr_norxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_NORXCPL;
}

static inline bool
lb_need_rxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_NORXCPL) == 0);
}

static inline bool
lb_set_pktfetched(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb)->flags |= LBF_PKTFETCHED);
}

static inline bool
lb_reset_pktfetched(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb)->flags &= ~LBF_PKTFETCHED);
}

static inline bool
lb_is_pktfetched(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_PKTFETCHED) != 0);
}

static inline void
lb_set_frwd_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags |= LBF_FRWD_PKT;
}

static inline void
lb_reset_frwd_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_FRWD_PKT;
}

static inline bool
lb_is_frwd_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_FRWD_PKT) != 0);
}

static inline bool
lb_altinterface(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (((lb)->flags & LBF_ALT_INTF) != 0);
}

static inline void
lb_setaltinterface(struct lbuf * lb, bool set)
{
	ASSERT(lb_sane(lb));
	if (set)
		(lb)->flags |= LBF_ALT_INTF;
	else
		(lb)->flags &= ~LBF_ALT_INTF;
}

static inline bool
lb_is_hwa_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_HWA_PKT) != 0);
}

static inline void
lb_set_hwa_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_HWA_PKT;
}

static inline void
lb_reset_hwa_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_HWA_PKT;
}

static inline uint8
lb_hwa_rxOff(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->hwa_rxOff);
}

static inline void
lb_sethwa_rxOff(struct lbuf *lb, uint8 offset)
{
	ASSERT(lb_sane(lb));
	lb->hwa_rxOff = offset;
}

static inline bool
lb_is_buf_alloc(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_BUF_ALLOC) != 0);
}

static inline void
lb_set_buf_alloc(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_BUF_ALLOC;
}

static inline void
lb_reset_buf_alloc(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_BUF_ALLOC;
}

static inline bool
lb_is_hwa_3bpkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_HWA_3BPKT) != 0);
}

static inline void
lb_set_hwa_3bpkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_HWA_3BPKT;
}

static inline void
lb_reset_hwa_3bpkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_HWA_3BPKT;
}

static inline bool
lb_is_mgmt_tx_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_MGMT_TX_PKT) != 0);
}

static inline void
lb_set_mgmt_tx_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_MGMT_TX_PKT;
}

static inline void
lb_reset_mgmt_tx_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_MGMT_TX_PKT;
}

static inline bool
lb_is_hwa_hostreorder(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_HWA_HOSTREORDER) != 0);
}

static inline void
lb_set_hwa_hostreorder(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_HWA_HOSTREORDER;
}

static inline void
lb_reset_hwa_hostreorder(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_HWA_HOSTREORDER;
}
#endif /* !HNDLBUF_USE_MACROS */

/* if set, lb_free() skips de-alloc
 * Keep the below as macros for now, as PKTPOOL macros are not defined here
 */
#define lb_setpool(lb, set, _pool) ({ \
	ASSERT(lb_sane(lb)); \
	if (set) { \
		ASSERT(POOLID(_pool) <= PKTPOOL_MAXIMUM_ID); \
		(lb)->mem.poolid = POOLID(_pool); \
	} else { \
		(lb)->mem.poolid = PKTPOOL_INVALID_ID; \
	} \
})

static inline void
lb_setexempt(struct lbuf *lb, uint8 exempt)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_EXEMPT_MASK;
	(lb)->flags |= exempt << LBF_EXEMPT_SHIFT;
}

static inline uint8
lb_getexempt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb)->flags & LBF_EXEMPT_MASK) >> LBF_EXEMPT_SHIFT;
}

#define lb_getpool(lb) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->mem.poolid <= PKTPOOL_MAXIMUM_ID); \
	PKTPOOL_ID2PTR((lb)->mem.poolid); \
})

#define lb_pool(lb) ({ \
	ASSERT(lb_sane(lb)); \
	((lb)->mem.poolid != PKTPOOL_INVALID_ID); \
})

#ifdef BCMDBG_POOL
#define lb_poolstate(lb) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->mem.poolid != PKTPOOL_INVALID_ID); \
	(lb)->mem.poolstate; \
})

#define lb_setpoolstate(lb, state) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->mem.poolid != PKTPOOL_INVALID_ID); \
	(lb)->mem.poolstate = (state); \
})
#endif // endif

#define lb_80211pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_80211_PKT) != 0); \
})

#define lb_set80211pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_80211_PKT; \
})

#ifdef WL_MONITOR
#define lb_monpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_MON_PKT) != 0); \
})

#define lb_setmonpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_MON_PKT; \
})
#endif /* WL_MONITOR */

#endif /* BCMHWAPP */

#endif /* _hnd_lbuf_h_ */
