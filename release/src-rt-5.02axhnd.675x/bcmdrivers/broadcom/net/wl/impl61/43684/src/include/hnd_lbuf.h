/*
 * HND generic packet buffer definitions.
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
 *
 * $Id: hnd_lbuf.h 769737 2018-11-27 03:20:47Z $
 */

#ifndef _hnd_lbuf_h_
#define	_hnd_lbuf_h_

#include <typedefs.h>

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

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
 * HNDLBUFCOMPACT: In small memory footprint platforms (2MBytes), the 32bits
 * head and end pointers are compacted using offset from memory base and offset
 * from head pointer, respectively, saving 4 Bytes per lbuf.
 *
 * lb_resetpool, relies on the lbuf structure layout as an optimization to
 * otherwise having to save, bzero and restore persistent or fields that will
 * be explicitly set.
 */
struct lbuf {
#if defined(BCMHWA) && defined(HWA_PKT_MACRO)
	/* 4-byte-aligned packet tag area */
	uint32		pkttag[(OSL_PKTTAG_SZ + 3) / 4];

	/* WAR: B0 TxPost Aggregate issue
	 * CWI translate an SW tx packet in TX BM is
	 * 32-byte(CWI) +12-byte + 32-byte (Zero pkt_tag) = 76-byte.
	 * ACWI translate to a SW packet  in TX BM is
	 * 48-byte(ACWI) + 12-byte + 32-byte (Zero pkt_tag) = 92-byte.
	 */
	uchar		*head;		/**< fixed start of buffer */
	uchar		*end;		/**< fixed end of buffer */
	uchar		*data;		/**< variable start of data */

	uint16		len;		/**< nbytes of data */
	uint16		cfp_flowid;	/**< WLCCFP: Cached Flow Processing FlowId */
#endif /* BCMHWA */

	union {
		uint32  u32;
		struct {
			uint16  pktid;		/**< unique packet ID */
			uint8   refcnt;		/**< external references to this lbuf */
#if defined(BCMDBG_POOL)
			uint8   poolstate : 4;	/**< BCMDBG_POOL stats collection */
			uint8   poolid    : 4;	/**< pktpool ID */
#else  /* ! BCMDBG_POOL                byte access is faster than 4bit access */
			uint8   poolid;		/**< entire byte is used for performance */
#endif /* ! BCMDBG_POOL */
		};
	} mem;					/**< Lbuf memory management context */

#if !defined(BCMHWA) || !defined(HWA_PKT_MACRO)
#if defined(HNDLBUFCOMPACT)
	union {
		uint32 offsets;
		struct {
			uint32 head_off : 21;	/**< offset from 2MBytes memory base */
			uint32 end_off  : 11;	/**< offset from head ptr, max 2KBytes */
		};
	};
#else  /* ! HNDLBUFCOMPACT */
	uchar		*head;		/**< fixed start of buffer */
	uchar		*end;		/**< fixed end of buffer */
#endif /* ! HNDLBUFCOMPACT */

	uchar		*data;		/**< variable start of data */

	uint16		len;		/**< nbytes of data */
	uint16		cfp_flowid;	/**< WLCCFP: Cached Flow Processing FlowId */
#endif /* !BCMHWA */

	uint32		flags;		/**< private flags; don't touch */

	/* ----- following fields explicitly bzeroed in lb_resetpool ----- */

	union {
		uint32 reset;		/**< recycling to reset pool, zeros these */
		struct {
			union {
				uint16 dmapad;   /**< padding to be added for tx dma */
				uint16 rxcpl_id; /**< rx completion ID used for AMPDU reordering */
				uint16 dma_index; /**< index into DMA ring */
			};
			union {
				uint8  dataOff; /**< offset to beginning of data in 4-byte words */
				uint8  hwa_rxOff; /** HWA RX buffer offset */
			};
			uint8  ifidx;
		};
	};

#if defined(BCMPKTIDMAP)
	union {
		struct {
			uint16 nextid; /**< id of next lbuf in a chain of lbufs forming one pkt */
			uint16 linkid; /**< id of first lbuf of next packet in a list of packets */
		};
		void * freelist;	/**< linked list of free lbufs in pktpool */
	};
#else  /* ! BCMPKTIDMAP */
	struct lbuf *next;	/**< next lbuf in a chain of lbufs forming one packet */
	struct lbuf *link;	/**< first lbuf of next packet in a list of packets */
#endif /* ! BCMPKTIDMAP */
#if !defined(BCMHWA) || !defined(HWA_PKT_MACRO)
	uint32		pkttag[(OSL_PKTTAG_SZ + 3) / 4];  /* 4-byte-aligned packet tag area */
#endif // endif
};

#define	LBUFSZ		((int)sizeof(struct lbuf))
#define LBUFFRAGSZ	((int)sizeof(struct lbuf_frag))

/* enough to fit a 1500 MTU plus overhead */
#ifndef MAXPKTDATABUFSZ
#define MAXPKTDATABUFSZ 1920
#endif // endif

/* Total maximum packet buffer size including lbuf header */
#define MAXPKTBUFSZ (MAXPKTDATABUFSZ + LBUFSZ)

#ifndef RATE_CACHE_HDRSZ
#define RATE_CACHE_HDRSZ 0
#endif // endif

#ifndef TXPKTFRAGSZ
#define TXPKTFRAGSZ	342
#endif // endif

#ifndef MAXPKTFRAGSZ
/* enough to fit TxHdrs + lbuf_frag overhead */
#define MAXPKTFRAGSZ	(TXPKTFRAGSZ)
#endif // endif

/*
* In MODE4, start of copy count has 802.3 HDR (22) which is not the case in MODE2.
* The maximum possible value is calculated by adding maximum values of below items.
*
* +-----------------------------------------------------------------------+
* | lfrag | sw hdr | rx hdr | pad | plcp | 802.11 hdr |  iv  | copy count |
* | (92)  |  (12)  |  (xx)  | (2) | (xx) |    (36)    | (18) |    (68)    |
* +-----------------------------------------------------------------------+
*
* XXX: define appropriate value depending upon rx header and plcp size from chip specific makefile
*/
#ifndef MAXPKTRXFRAGSZ
#define MAXPKTRXFRAGSZ  256
#endif /* MAXPKTRXFRAGSZ */

#if defined(HNDLBUFCOMPACT)
#define LB_RAMSIZE		(1 << 21) /* 2MBytes */
#define LB_BUFSIZE_MAX  (1 << 11) /* 2KBytes */

#if (LB_RAMSIZE > (1 << 21))
#error "Invalid LB_RAMSIZE, only 21 bits reserved for head_off"
#endif /* LB_RAMSIZE */

#if (LB_BUFSIZE_MAX > (1 << 11))
#error "Invalid LB_BUFSIZE_MAX, only 11 bits reserved for end_off"
#endif /* LB_BUFSIZE_MAX */

#if (MAXPKTDATABUFSZ > LB_BUFSIZE_MAX)
#error "Invalid MAXPKTDATABUFSZ"
#endif /* MAXPKTBUFSZ */

#if defined(HND_PT_GIANT)
#error "HND_PT_GIANT: may allocate packets larger than MAXPKTBUFSZ"
#endif /* HND_PT_GIANT */

/*
 * head composed by clearing lo 21bits of data pointer and adding head_off, and
 * end pointer is composed by adding end_off to computed head pointer.
 * HNDLBUFCOMPACT: warning: LB_HEAD casting uint32 to uchar pointer
 */
#define LB_HEADLO_MASK  (LB_RAMSIZE - 1)
#define LB_HEADHI_MASK  ((uint32)(~LB_HEADLO_MASK))

#define LB_HEAD(lb) (((uchar *)((uintptr)((lb)->data) & (LB_HEADHI_MASK))) \
	                             + (lb)->head_off)
#define LB_END(lb)  (LB_HEAD(lb) + ((lb)->end_off + 1))

#else  /* ! HNDLBUFCOMPACT */

#define LB_HEAD(lb) ((lb)->head)
#define LB_END(lb)  ((lb)->end)

#endif /* ! HNDLBUFCOMPACT */
/* private flags - don't reference directly */
#define	LBF_PRI		0x00000007	/**< priority (low 3 bits of flags) */
#define LBF_SUM_NEEDED	0x00000008	/**< host->device */
#define LBF_ALT_INTF	0x00000008	/**< internal: indicate on alternate interface */
#define LBF_SUM_GOOD	0x00000010
#define LBF_MSGTRACE	0x00000020
#define LBF_CLONE	0x00000040
#define LBF_DOT3_PKT	0x00000080
#define LBF_80211_PKT	0x00000080	/* Clash ? */
#define LBF_PTBLK	0x00000100	/**< came from fixed block in partition, not main malloc */
#define LBF_TX_FRAG	0x00000200
#define LBF_PKT_NODROP	0x00000400	/**< for PROP_TXSTATUS */
#define LBF_PKT_EVENT	0x00000800	/**< for PROP_TXSTATUS */
#define LBF_CHAINED	0x00001000
#define LBF_RX_FRAG	0x00002000
#define LBF_METADATA	0x00004000
#define LBF_NORXCPL	0x00008000
#define LBF_EXEMPT_MASK	0x00030000
#define LBF_EXEMPT_SHIFT 16
#define LBF_PKTFETCHED	0x00040000	/* indicates pktfetched packet */
#define LBF_FRWD_PKT	0x00080000
#ifdef WL_MONITOR
#define LBF_MON_PKT	0x00100000
#endif // endif
#define LBF_HEAPBUF	0x00200000
/*			0x00400000	available */
#define LBF_CFP_PKT	0x00800000	/**< Cache Flow Processing capable packet */
#define LBF_BUF_ALLOC	0x01000000	/**< indicates packet has external buffer allocated */
#define LBF_HWA_PKT	0x02000000	/**< Packet owned by HWA */
#define LBF_HWA_3BPKT	0x04000000	/**< Packet is HWA 3b SWPKT */
#define LBF_MGMT_TX_PKT	0x08000000	/**< indicates mgmt tx packet with extra HWA 3b header */
#define LBF_HWA_HOSTREORDER 0x10000000 /* indicates packet has AMPDU_SET_HOST_HASPKT */
/*			0x20000000	available */
/*			0x40000000	available */
/*			0x80000000	available */

#ifdef WL_MONITOR
#define	MON_PKTTAG_NOISE_IDX	11
#define	MON_PKTTAG_RSSI_IDX	12
#endif /* WL_MONITOR */

/* if the packet size is less than 512 bytes, lets try to use shrink buffer */
#define LB_SHRINK_THRESHOLD	512

#define	LBP(lb)		((struct lbuf *)(lb))

/* lbuf clone structure
 * if lbuf->flags LBF_CLONE bit is set, the lbuf can be cast to an lbuf_clone
 */
struct lbuf_clone {
	struct lbuf	lbuf;
	struct lbuf	*orig;	/**< original non-clone lbuf providing the buffer space */
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
#define LB_FRAG_MAX		2	/**< Typical number of host memory fragments */
#define LB_FRAG_CTX		0	/**< Index 0 in lbuf_flist saves context */
#define LB_FRAG1                1
#define LB_FRAG2                2

#define LB_FIFO0_INT		0x01
#define LB_FIFO1_INT		0x02
#define LB_HDR_CONVERTED        0x04
#define	LB_TXS_PROCESSED	0x08 /**< Indicate lbuf is txs processed */
#define	LB_TXTS_INSERTED	0x10 /* Indicate that Tx Timestamp inserted */
#define	LB_TXS_HOLD		0x20 /* Indicate Tx status in the middle of AMPDU */
#define LB_RX_CORRUPTED		0x40

/* LB_FRAB_MAX needs to be an even number, so please make sure that is the case */
#if (LB_FRAG_MAX & 1)
#error "LB_FRAG_MAX need to be an even number"
#endif // endif

/** information on a packet fragment residing in host memory */
struct lbuf_finfo {
	union {

		struct {			/**< element 0 in array of lbuf_finfo */
			uint32 pktid;	/**< unique opaque  id reference to foreign packet */
			uint8 fnum;		/**< total number of fragments in this lbuf */
			uint8 lfrag_flags;	/**< generic flag information of lfrag */
			uint16 flowid;
		} ctx;

		struct {			/**< element 1..LB_FRAGS_MAX */
			uint32 data_lo;
			uint32 data_hi;
		} frag; /* used e.g. to store a fragments 64 bit host memory address  */

		uint8 txstatus;
		uint8 fc_tlv[8];	/* storage for WLFC TLV */
	};
};

/**
 * The fraglist is an array of elements describing the fragments in the host. A set of PKTFRAG
 * macros are provided to access the lbuf_flist fields.
 */
struct lbuf_flist {
	struct lbuf_finfo finfo[LB_FRAG_MAX + 1]; /**< Element finfo[0] is context */
	uint16 flen[LB_FRAG_MAX + 1];             /**< Element flen[0] is total fragment length */
	/* Using 2 byte pad to store index of the flow ring */
	/* be careful or move it to proper place on removing pad */
	uint16 ring_idx;
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
	struct lbuf       lbuf;
	/** flist: array of information on fragments in host memory */
	struct lbuf_flist flist;
};

#define	LBFP(lb)				((struct lbuf_frag *)(lb))

#if defined(BCMHWA) && defined(HWA_PKT_MACRO)
#define LBFCTX(lb)	(LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx)
#endif // endif

enum lbuf_type {
	lbuf_basic = 0,
	lbuf_frag,	/**< tx frag */
	lbuf_rxfrag,
	lbuf_mgmt_tx	/* Same as basic type but extra hwa3b header reserved in the head */
};

/* prototypes */
extern struct lbuf *lb_init(struct lbuf *lb, enum lbuf_type lb_type, uint lb_sz);
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
extern struct lbuf *lb_alloc_header(void *data, uint size, enum lbuf_type lbuf_type,
	void *call_site);
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbuf_type, void *call_site);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len, void *call_site);
extern struct lbuf *lb_dup(struct lbuf *lb, void *call_site);
extern struct lbuf *lb_shrink(struct lbuf *lb, void *call_site);

#else
extern struct lbuf *lb_alloc_header(void *data, uint size, enum lbuf_type lbuf_type);
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbtype);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len);
extern struct lbuf *lb_dup(struct lbuf *lb);
extern struct lbuf *lb_shrink(struct lbuf *lb);
#endif /* BCMDBG_MEM || BCMDBG_MEMFAIL */

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

#define lb_has_heapbuf(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_HEAPBUF) != 0); \
})

#define lb_set_heapbuf(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_HEAPBUF; \
})

#define lb_clr_heapbuf(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_HEAPBUF; \
})

#define lb_set_norxcpl(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_NORXCPL; \
})

#define lb_clr_norxcpl(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_NORXCPL; \
})

#define lb_need_rxcpl(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_NORXCPL) == 0); \
})

#define lb_altinterface(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_ALT_INTF) != 0); \
})

#define lb_set_pktfetched(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_PKTFETCHED; \
})

#define lb_reset_pktfetched(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_PKTFETCHED; \
})

#define lb_is_pktfetched(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_PKTFETCHED) != 0); \
})

#define lb_set_frwd_pkt(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_FRWD_PKT; \
})

#define lb_reset_frwd_pkt(lb) ({	\
	ASSERT(lb_sane(lb)); \
	(lb)->flags &= ~LBF_FRWD_PKT; \
})

#define lb_is_frwd_pkt(lb) ({	\
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

#endif	/* _hnd_lbuf_h_ */
