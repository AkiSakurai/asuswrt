/*
 * HND RTE packet buffer definitions.
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
 * $Id: hndrte_lbuf.h 708652 2017-07-04 04:52:34Z $
 */

#ifndef _hndrte_lbuf_h_
#define	_hndrte_lbuf_h_

#include <bcmdefs.h>

/*
 * In BCMPKTIDMAP, an lbuf's 16bit ID is used instead of a 32bit lbuf pointer,
 * with a cost of a ID to pointer conversion using an indexed lookup into
 * hndrte_pktptr_map[].
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
	union {
		uint32  u32;
		struct {
			uint16  pktid;          /* unique packet ID */
			uint8   refcnt;         /* external references to this lbuf */
#if defined(BCMDBG_POOL)
			uint8   poolstate : 4;  /* BCMDBG_POOL stats collection */
			uint8   poolid    : 4;  /* pktpool ID */
#else  /* ! BCMDBG_POOL                byte access is faster than 4bit access */
			uint8   poolid;         /* entire byte is used for performance */
#endif /* ! BCMDBG_POOL */
		};
	} mem;                          /* Lbuf memory management context */

#if defined(HNDLBUFCOMPACT)
    union {
		uint32 offsets;
		struct {
			uint32 head_off : 21;   /* offset from 2MBytes memory base */
			uint32 end_off  : 11;   /* offset from head ptr, max 2KBytes */
		};
	};
#else  /* ! HNDLBUFCOMPACT */
	uchar		*head;		/* fixed start of buffer */
	uchar		*end;		/* fixed end of buffer */
#endif /* ! HNDLBUFCOMPACT */

	uchar		*data;		/* variable start of data */

	uint16		len;		/* nbytes of data */
	uint16		flags;		/* private flags; don't touch */

	/* ----- following fields explicitly bzeroed in lb_resetpool ----- */

	union {
		uint32 reset;       /* recycling to reset pool, zeros these */
		struct {
			union {
				uint16 dmapad;  /* padding to be added for tx dma */
				uint16 rxcpl_id; /* rx completion ID */
			};
			uint8  dataOff; /* offset to beginning of data in 4-byte words */
			uint8  ifidx;
		};
	};
#if defined(BCM_DHDHDR) || defined(BCM_DHDHDR_IN_ROM)
	uint16		fcseq;		/* WLFC SEQ */
	uint16		pad1;
#endif // endif
#if defined(BCMPKTIDMAP)
	union {
		struct {
			uint16 nextid; /* id of next lbuf in a chain of lbufs forming one packet */
			uint16 linkid; /* id of first lbuf of next packet in a list of packets */
		};
		void * freelist;    /* linked list of free lbufs in pktpool */
	};

	/* ROM compatibility. */
#if defined(BCMPKTIDMAP_ROM_COMPAT)
	void *pad;
#endif /* defined(BCMPKTIDMAP_ROM_COMPAT) */

#else  /* ! BCMPKTIDMAP */
	struct lbuf *next;  /* next lbuf in a chain of lbufs forming one packet */
	struct lbuf *link;  /* first lbuf of next packet in a list of packets */
#endif /* ! BCMPKTIDMAP */

	uint32		pkttag[(OSL_PKTTAG_SZ + 3) / 4];  /* 4-byte-aligned packet tag area */
};

#define	LBUFSZ		((int)sizeof(struct lbuf))
#define LBUFFRAGSZ	((int)sizeof(struct lbuf_frag))

/* enough to fit a 1500 MTU plus overhead */
#ifndef MAXPKTDATABUFSZ
#define MAXPKTDATABUFSZ 1920
#endif // endif

/* Total maximum packet buffer size including lbuf header */
#define MAXPKTBUFSZ (MAXPKTDATABUFSZ + LBUFSZ)

#ifndef MAXPKTFRAGSZ
#define MAXPKTFRAGSZ 338 	/* enough to fit the TxHdrs + lbuf_frag overhead */
#endif // endif
#ifndef MAXPKTRXFRAGSZ
#define MAXPKTRXFRAGSZ  224	/* enough to fit management pkt */
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
#error "Invalid MAXPKTBUFSZ"
#endif /* MAXPKTBUFSZ */

#if defined(HNDRTE_PT_GIANT)
#error "HNDRTE_PT_GIANT: may allocate packets larger than MAXPKTBUFSZ"
#endif /* HNDRTE_PT_GIANT */

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
#define	LBF_PRI		0x0007		/* priority (low 3 bits of flags) */
#define LBF_SUM_NEEDED	0x0008	/* host->device */
#define LBF_ALT_INTF	0x0008	/* internal: indicate on alternate interface */
#define LBF_SUM_GOOD	0x0010
#define LBF_MSGTRACE	0x0020
#define LBF_CLONE	0x0040
#define LBF_DOT3_PKT	0x0080
#define LBF_80211_PKT	0x0080  /* Clash ? */
#define LBF_PTBLK	0x0100		/* came from fixed block in a partition, not main malloc */
#define LBF_TX_FRAG	0x0200
#define LBF_PKT_NODROP	0x0400
#define LBF_PKT_EVENT	0x0800
#define LBF_CHAINED	0x1000
#define LBF_RX_FRAG	0x2000
#define LBF_METADATA	0x4000
#define LBF_NORXCPL	0x8000
#define LBF_HEAPBUF	0x8000
#define LBF_MON_PKT     0x10000
#define	MON_PKTTAG_NOISE_IDX	10
#define	MON_PKTTAG_RSSI_IDX	11

/* 2 flags available in most-signif-nibble:  0x4000, 0x8000 */

/* if the packet size is less than 512 bytes, lets try to use shrink buffer */
#define LB_SHRINK_THRESHOLD	512

#define	LBP(lb)		((struct lbuf *)(lb))

/* lbuf clone structure
 * if lbuf->flags LBF_CLONE bit is set, the lbuf can be cast to an lbuf_clone
 */
struct lbuf_clone {
	struct lbuf	lbuf;
	struct lbuf	*orig;	/* original non-clone lbuf providing the buffer space */
};

/* lbuf with fragment
 *
 * Fragix=0 implies internally stored data. Fragix=1..LB_FRAG_MAX refers to
 * externally stored data, that is not directly accessible.
 */
#define LB_FRAG_MAX		2	/* Maximum number of fragments */
#define LB_FRAG_CTX		0	/* Index 0 in lbuf_flist saves context */
/* lfrag_flags first bit is to indicate if LB is txstatus processed */
#define LB_TXS_PROCESSED	0x08

#if (LB_FRAG_MAX & 1)
#error "LB_FRAG_MAX need to be an even number"
#endif // endif

struct lbuf_finfo {
	union {

		struct {			/* element 0 in array of lbuf_finfo */
			uint32 pktid;	/* unique opaque  id reference to foreign packet */
			uint8 fnum;	/* total number of fragments in this lbuf */
			uint8 lfrag_flags; /* generic flag information of lfrag */
			uint16 flowid;	/* To store Flow ring Id for the packet */
		} ctx;

		struct {			/* element 1..LB_FRAGS_MAX */
			uint32 data_lo;
			uint32 data_hi;
		} frag;
#if defined(BCM_DHDHDR) || defined(BCM_DHDHDR_in_ROM)
		struct {
			uint8 txstatus;
			uint8 pad[7];
		} scb_cache;

		uint8 fc_tlv[8];	/* storage for WLFC TLV */
#endif // endif
	};
};

/* Element finfo[0] is context. element flen[0] is total fragment length */
struct lbuf_flist {
	struct lbuf_finfo finfo[LB_FRAG_MAX + 1];
	uint16 flen[LB_FRAG_MAX + 1];
	/* Using 2 byte pad to store index of the flow ring */
	uint16 ring_idx;
};

/* Lbuf extended with fragment list */
struct lbuf_frag {
	struct lbuf       lbuf;
	struct lbuf_flist flist;
};

#define	LBFP(lb)				((struct lbuf_frag *)(lb))

enum lbuf_type {
	lbuf_basic = 0,
	lbuf_frag,
	lbuf_rxfrag
};
/* prototypes */
extern void lb_init(void);
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbuf_type, const char *file, int line);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len, const char *file, int line);
#else
extern struct lbuf *lb_alloc(uint size, enum lbuf_type lbtype);
extern struct lbuf *lb_clone(struct lbuf *lb, int offset, int len);
#endif /* BCMDBG_MEM || BCMDBG_MEMFAIL */

extern struct lbuf *lb_dup(struct lbuf *lb);
extern struct lbuf *lb_shrink(struct lbuf *lb);
extern void lb_free(struct lbuf *lb);
extern bool lb_sane(struct lbuf *lb);
extern void lb_audit(struct lbuf *lb);
extern void lb_resetpool(struct lbuf *lb, uint16 len);
#ifdef BCMDBG
extern void lb_dump(void);
#endif // endif
void lb_audit(struct lbuf * lb);
typedef bool (*lbuf_free_cb_t)(void* arg, void* p);

#ifdef BCM_DHDHDR
/**
 * DHD may register a memory region for use as a lbuf extension.
 * Each lbuf may be extended in host memory, for instance to save the dongle
 * constructed Txhdr+D11Hdr (aka D11_BUFFER). Dongle advertizes to host to
 * reserve contiguous host memory for N number of D11_BUFFERs. Number of
 * D11_BUFFERs and size of D11_BUFFERS is defined in bcmpcie.h
 *
 * In Dongle, a pktpool registry maintains a maximum number of dongle packets
 * that may be instantiated and a unique pktid is assigned. For each dongle's
 * packet, a corresponding region in the host is reserved.
 *
 * Given an Lbuf, the pktid in the lbuf will be used to uniquely fetch a region
 * on host, to serve as an extension for the lbuf.
 */
/** Register the host memory region configuration */
extern void lbuf_dhdhdr_memory_register(uint32 ext_buf_size, uint32 ext_num_bufs,
	void *mem_base_addr);

/** Return the host memory extension region's addr corresponding to an lbuf */
extern void lbuf_dhdhdr_memory_extension(struct lbuf *lb,
	void *rtn_buf_base_addr);

/** Attach data buffer to lbuf */
void lb_set_buf(struct lbuf *lb, void *buf, uint size);
#endif /* BCM_DHDHDR */

typedef struct {
	lbuf_free_cb_t cb;
	 void* arg;
} lbuf_freeinfo_t;
extern void lbuf_free_register(lbuf_free_cb_t cb, void* arg);

#ifdef __GNUC__

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

#define lb_setpool(lb, set, _pool) ({ \
	ASSERT(lb_sane(lb)); \
	if (set) { \
		ASSERT(POOLID(_pool) <= PKTPOOL_MAXIMUM_ID); \
		(lb)->mem.poolid = POOLID(_pool); \
	} else { \
		(lb)->mem.poolid = PKTPOOL_INVALID_ID; \
	} \
})

#define lb_getpool(lb) ({ \
	ASSERT(lb_sane(lb)); \
	ASSERT((lb)->mem.poolid <= PKTPOOL_MAXIMUM_ID); \
	PKTPOOL_ID2PTR((lb)->mem.poolid); \
})

#define lb_pool(lb) ({ \
	ASSERT(lb_sane(lb)); \
	((lb)->mem.poolid != PKTPOOL_INVALID_ID); \
})

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

#define lb_80211pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_80211_PKT) != 0); \
})

#define lb_set80211pkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_80211_PKT; \
})

#if defined(WL_MONITOR)
#define lb_monpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(((lb)->flags & LBF_MON_PKT) != 0); \
})

#define lb_setmonpkt(lb) ({ \
	ASSERT(lb_sane(lb)); \
	(lb)->flags |= LBF_MON_PKT; \
})
#endif /* WL_MONITOR */

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
	ASSERT(lb_is_txfrag(lb)); \
	(((lb)->flags & LBF_HEAPBUF) != 0); \
})

#define lb_set_heapbuf(lb) ({	\
	ASSERT(lb_sane(lb)); \
	ASSERT(lb_is_txfrag(lb)); \
	(lb)->flags |= LBF_HEAPBUF; \
})

#define lb_clr_heapbuf(lb) ({	\
	ASSERT(lb_sane(lb)); \
	ASSERT(lb_is_txfrag(lb)); \
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

#define lb_setaltinterface(lb, set) ({ \
	ASSERT(lb_sane(lb)); \
	if (set) { \
		(lb)->flags |= LBF_ALT_INTF; \
	} else { \
		(lb)->flags &= ~LBF_ALT_INTF; \
	} \
})

#else /* !__GNUC__ */

static INLINE uchar *
lb_head(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return LB_HEAD(lb);
}

static INLINE uchar *
lb_end(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return LB_END(lb);
}

static INLINE uchar *
lb_push(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT((lb->data - len) >= LB_HEAD(lb));
	lb->data -= len;
	lb->len += len;
	return (lb->data);
}

static INLINE uchar *
lb_pull(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT(len <= lb->len);
	lb->data += len;
	lb->len -= len;
	return (lb->data);
}

static INLINE void
lb_setlen(struct lbuf *lb, uint len)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->data + len <= LB_END(lb));
	lb->len = len;
}

static INLINE uint
lb_pri(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PRI);
}

static INLINE void
lb_setpri(struct lbuf *lb, uint pri)
{
	ASSERT(lb_sane(lb));
	ASSERT((pri & LBF_PRI) == pri);
	lb->flags = (lb->flags & ~LBF_PRI) | (pri & LBF_PRI);
}

static INLINE bool
lb_sumneeded(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_SUM_NEEDED) != 0);
}

static INLINE void
lb_setsumneeded(struct lbuf *lb, bool summed)
{
	ASSERT(lb_sane(lb));
	if (summed)
		lb->flags |= LBF_SUM_NEEDED;
	else
		lb->flags &= ~LBF_SUM_NEEDED;
}

static INLINE bool
lb_sumgood(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_SUM_GOOD) != 0);
}

static INLINE void
lb_setsumgood(struct lbuf *lb, bool summed)
{
	ASSERT(lb_sane(lb));
	if (summed)
		lb->flags |= LBF_SUM_GOOD;
	else
		lb->flags &= ~LBF_SUM_GOOD;
}

static INLINE bool
lb_msgtrace(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_MSGTRACE) != 0);
}

static INLINE void
lb_setmsgtrace(struct lbuf *lb, bool set)
{
	ASSERT(lb_sane(lb));
	if (set)
		lb->flags |= LBF_MSGTRACE;
	else
		lb->flags &= ~LBF_MSGTRACE;
}

/* get Data Offset */
static INLINE uint8
lb_dataoff(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->dataOff);
}

/* set Data Offset */
static INLINE void
lb_setdataoff(struct lbuf *lb, uint8 dataOff)
{
	ASSERT(lb_sane(lb));
	lb->dataOff = dataOff;
}

static INLINE int
lb_isclone(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_CLONE) != 0);
}

static INLINE int
lb_is_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_TX_FRAG) != 0);
}

static INLINE void
lb_set_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_TX_FRAG;
}

static INLINE void
lb_reset_txfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_TX_FRAG;
}

static INLINE int
lb_is_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_RX_FRAG) != 0);
}
static INLINE void
lb_set_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_RX_FRAG;
}
static INLINE void
lb_reset_rxfrag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_RX_FRAG;
}
static INLINE int
lb_is_frag(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & (LBF_TX_FRAG | LBF_RX_FRAG)) != 0);
}
static INLINE int
lb_isptblk(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_PTBLK) != 0);
}

static INLINE void
lb_setpktid(struct lbuf *lb, uint16 pktid)
{
	ASSERT(lb_sane(lb));
	ASSERT(pktid <= PKT_MAXIMUM_ID);
	lb->mem.pktid = pktid;
}

static INLINE uint16
lb_getpktid(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->mem.pktid <= PKT_MAXIMUM_ID);
	return lb->mem.pktid;
}

/* if set, lb_free() skips de-alloc */
static INLINE void
lb_setpool(struct lbuf *lb, int8 set, void *pool)
{
	ASSERT(lb_sane(lb));

	if (set) {
		ASSERT(POOLID(pool) <= PKTPOOL_MAXIMUM_ID);
		lb->mem.poolid = POOLID(pool);
	} else {
		lb->mem.poolid = PKTPOOL_INVALID_ID;
	}
}

static INLINE void *
lb_getpool(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->mem.poolid <= PKTPOOL_MAXIMUM_ID);
	return PKTPOOL_ID2PTR(lb->mem.poolid);
}

static INLINE bool
lb_pool(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->mem.poolid != PKTPOOL_INVALID_ID);
}

static INLINE int8
lb_poolstate(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->mem.poolid != PKTPOOL_INVALID_ID);
	return lb->mem.poolstate;
}

static INLINE void
lb_setpoolstate(struct lbuf *lb, int8 state)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb->mem.poolid != PKTPOOL_INVALID_ID);
	lb->mem.poolstate = state;
}

static INLINE uint
lb_nodrop(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PKT_NODROP);
}

static INLINE void
lb_setnodrop(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_PKT_NODROP;
}
static INLINE uint
lb_typeevent(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (lb->flags & LBF_PKT_EVENT);
}

static INLINE void
lb_settypeevent(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_PKT_EVENT;
}

static INLINE bool
lb_dot3_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_DOT3_PKT) != 0);
}

static INLINE void
lb_set_dot3_pkt(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_DOT3_PKT;
}

static INLINE bool
lb_ischained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_CHAINED) != 0);
}

static INLINE void
lb_setchained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_CHAINED;
}

static INLINE void
lb_clearchained(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags &= ~LBF_CHAINED;
}

static INLINE bool
lb_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_METADATA) != 0);
}

static INLINE void
lb_set_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	lb->flags |= LBF_METADATA;
}
static INLINE void
lb_reset_has_metadata(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_METADATA;
}

static INLINE void
lb_has_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb_is_txfrag(lb));
	return (((lb)->flags & LBF_HEAPBUF) != 0);
}

static INLINE void
lb_set_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb_is_txfrag(lb));
	(lb)->flags |= LBF_HEAPBUF;
}

static INLINE void
lb_clr_heapbuf(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(lb_is_txfrag(lb));
	(lb)->flags &= ~LBF_HEAPBUF;
}

static INLINE void
lb_set_norxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags |= LBF_NORXCPL;
}

static INLINE void
lb_clr_norxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	(lb)->flags &= ~LBF_NORXCPL;
}

static INLINE bool
lb_need_rxcpl(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return ((lb->flags & LBF_NORXCPL) == 0);
}

static INLINE bool
lb_altinterface(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	return (((lb)->flags & LBF_ALT_INTF) != 0);
}

static INLINE void
lb_setaltinterface(lb, set)
{
	ASSERT(lb_sane(lb));
	if (set)
		(lb)->flags |= LBF_ALT_INTF;
	else
		(lb)->flags &= ~LBF_ALT_INTF;
}

#endif	/* !__GNUC__ */
#endif	/* !_hndrte_lbuf_h_ */
