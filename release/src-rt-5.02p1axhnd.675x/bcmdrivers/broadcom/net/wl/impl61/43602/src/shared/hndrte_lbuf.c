/*
 * HND RTE packet buffer routines.
 *
 * No caching,
 * Just a thin packet buffering data structure layer atop hndrte_malloc/free .
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
 * $Id: hndrte_lbuf.c 708088 2017-06-29 21:37:42Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <hndrte.h>
#include <bcmutils.h>
#include <hndrte_lbuf.h>

#ifdef BCMDBG
#define LBUF_MSG(x) printf x
#else
#define LBUF_MSG(x)
#endif // endif

#define LBUF_BIN_SIZE	6

#if defined(BCMLFRAG) && !defined(BCMLFRAG_DISABLED)
bool _bcmlfrag = TRUE;
#else
bool _bcmlfrag = FALSE;
#endif // endif

lbuf_freeinfo_t lbuf_info;
static lbuf_freeinfo_t* lbuf_info_get(void);

static lbuf_freeinfo_t*
BCMRAMFN(lbuf_info_get)(void)
{
	return (&lbuf_info);
}

void lbuf_free_register(lbuf_free_cb_t cb, void* arg)
{
	lbuf_freeinfo_t *lbuf_infoptr;

	lbuf_infoptr = lbuf_info_get();

	lbuf_infoptr->cb = cb;
	lbuf_infoptr->arg = arg;
}
static const uint lbsize[LBUF_BIN_SIZE] = {
	MAXPKTBUFSZ >> 4,
	MAXPKTBUFSZ >> 3,
	MAXPKTBUFSZ >> 2,
	MAXPKTBUFSZ >> 1,
	MAXPKTBUFSZ,
#ifdef HNDLBUFCOMPACT
	/* The compact lbuf scheme is implemented based on imposing limit
	   on max size to LB_BUFSIZE_MAX
	*/
	LB_BUFSIZE_MAX + LBUFSZ
#else
	4096 + LBUFSZ		/* ctrl queries on bus can be 4K */
#endif // endif
};

#if !defined(BCMPKTPOOL) || defined(BCMDBG)
static uint lbuf_hist[LBUF_BIN_SIZE] = {0};
#endif // endif
static uint lbuf_allocfail = 0;

#ifdef BCM_DHDHDR
struct lbuf_dhdhdr_memory {
	uint32 ext_buf_size; /* Size of each extension buffer */
	uint32 ext_num_bufs; /* Number of extension buffers */
	dma64addr_t ext_base_addr; /* Base address of all extension buffers */
} lbuf_dhdhdr_memory = {0U, 0U, {0U, 0U}};

/** Register the host memory region configuration */
void
lbuf_dhdhdr_memory_register(uint32 ext_buf_size, uint32 ext_num_bufs,
	void *mem_base_addr)
{
	dma64addr_t *ext_base_addr;
	ASSERT(ext_num_bufs >= PKT_MAXIMUM_ID);

	ext_base_addr = (dma64addr_t *)mem_base_addr;

	lbuf_dhdhdr_memory.ext_buf_size = ext_buf_size;
	lbuf_dhdhdr_memory.ext_num_bufs = ext_num_bufs;
	lbuf_dhdhdr_memory.ext_base_addr.loaddr = ext_base_addr->loaddr;
	lbuf_dhdhdr_memory.ext_base_addr.hiaddr = ext_base_addr->hiaddr;
}

/** Return the host memory extension region's addr corresponding to an lbuf */
void
lbuf_dhdhdr_memory_extension(struct lbuf *lb, void *rtn_buf_base_addr)
{
	dma64addr_t *buf_base_addr = (dma64addr_t *)rtn_buf_base_addr;
	uint16 pktid;

	ASSERT(lb != NULL);

	pktid = PKTID(lb);
	ASSERT(pktid <= lbuf_dhdhdr_memory.ext_num_bufs);

	buf_base_addr->loaddr = lbuf_dhdhdr_memory.ext_base_addr.loaddr +
		(lbuf_dhdhdr_memory.ext_buf_size * pktid);
	buf_base_addr->hiaddr = lbuf_dhdhdr_memory.ext_base_addr.hiaddr;
}

void
lb_set_buf(struct lbuf *lb, void *buf, uint size)
{
	lb->head = lb->data = (uchar *)buf;
	lb->end = lb->head + size;
	lb->len = size;
}
#endif /* BCM_DHDHDR */

void
lb_init()
{
	ASSERT(sizeof(struct lbuf) == LBUFSZ);
}
/* Set flags and size according to pkt type */
static void
set_lbuf_params(enum lbuf_type lbuf_type, uint16* flags, uint* lbufsz)
{
	switch (lbuf_type) {
		case lbuf_basic:
			/* legacy pkt */
			*flags = 0;
			*lbufsz = LBUFSZ;
			break;
		case lbuf_frag:
			/* tx frag */
			*flags = LBF_TX_FRAG;
			 *lbufsz = LBUFFRAGSZ;
			break;
		case lbuf_rxfrag:
			/* rx frag */
			*flags = 0;
			*lbufsz = LBUFFRAGSZ;
			break;
		default:
			break;
	}
}

struct lbuf *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
lb_alloc(uint size, enum lbuf_type lbuf_type, const char *file, int line)
#else
lb_alloc(uint size, enum lbuf_type lbuf_type)
#endif // endif
{
	uchar * head;
	struct lbuf *lb;
	uint tot, lbufsz, end_off;
	uint16 flags;

	if (hndrte_pktid_free_cnt() == 0) {
		LBUF_MSG(("lb_alloc: hndrte_pktid_free_cnt 0\n"));
		hndrte_pktid_inc_fail_cnt();
		goto error;
	}

	tot = 0;
	flags = 0;
	lbufsz = 0;

	/* set lbuf params based on type */
	set_lbuf_params(lbuf_type, &flags, &lbufsz);

#ifdef HNDRTE_PT_GIANT
	if (size > MAXPKTBUFSZ + lbufsz) {	/* giant rx pkt, get from fixed block partition */

		tot = lbufsz + ROUNDUP(size, sizeof(int));
		/* ASSERT(tot <= MEM_PT_BLKSIZE); */
		if ((lb = (struct lbuf*)hndrte_malloc_pt(tot)) != NULL) {
			flags |= LBF_PTBLK;
			goto success;
		}

		LBUF_MSG(("lb_alloc: size (%u); alloc failed;\n", tot));
		goto error;
	}
#endif // endif

#ifdef BCMPKTPOOL
	/* Don't roundup size if PKTPOOL enabled */
	tot = lbufsz + ROUNDUP(size, sizeof(int));
	if (tot > lbsize[ARRAYSIZE(lbsize)-1]) {
		LBUF_MSG(("lb_alloc: size too big (%u); alloc failed;\n",
		       (lbufsz + size)));
		goto error;
	}
#else
	{
		int i;
		for (i = 0; i < ARRAYSIZE(lbsize); i++) {
			if ((lbufsz + ROUNDUP(size, sizeof(int))) <= lbsize[i]) {
				tot = lbsize[i];
				lbuf_hist[i]++;
				break;
			}
		}
	}
#endif /* BCMPKTPOOL */

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	if (!file)
		file = "unknown";

	if (tot == 0) {
		printf("lb_alloc: size too big (%u); alloc failed; file %s; line %d\n",
		       (lbufsz + size), file, line);
		goto error;
	}

	if ((lb = (struct lbuf*)hndrte_malloc_align(tot, 0, file, line)) == NULL) {
		printf("lb_alloc: size (%u); alloc failed; file %s; line %d\n", (lbufsz + size),
		       file, line);
		goto error;
	}

#else
	if (tot == 0) {
		LBUF_MSG(("lb_alloc: size too big (%u); alloc failed;\n",
		       (lbufsz + size)));
		goto error;
	}

	if ((lb = (struct lbuf*)hndrte_malloc_align(tot, 0)) == NULL) {
		LBUF_MSG(("lb_alloc: size (%u); alloc failed;\n", (lbufsz + size)));
		goto error;
	}
#endif /* BCMDBG_MEM || BCMDBG_MEMFAIL */

#ifdef HNDRTE_PT_GIANT
	success:
#endif // endif
	ASSERT(ISALIGNED(lb, sizeof(int)));

	bzero(lb, lbufsz);

	head = (uchar*)((uchar*)lb + lbufsz);
	end_off = (tot - lbufsz);

	lb->data = (head + end_off) - ROUNDUP(size, sizeof(int));

#if defined(HNDLBUFCOMPACT)
	ASSERT((end_off > 0) && (end_off <= LB_BUFSIZE_MAX));
	ASSERT(((uintptr)head & LB_HEADHI_MASK) == ((uintptr)(lb->data) & LB_HEADHI_MASK));
	ASSERT(((uintptr)head & LB_HEADHI_MASK) == ((uintptr)(head + end_off) & LB_HEADHI_MASK));

	lb->head_off = ((uintptr)head) & LB_HEADLO_MASK;
	lb->end_off = (end_off - 1);
#else  /* ! HNDLBUFCOMPACT */
	lb->head = head;
	lb->end = head + end_off;
#endif /* ! HNDLBUFCOMPACT */

	lb->len = size;
	lb->mem.pktid = hndrte_pktid_allocate(lb); /* BCMPKTIDMAP: pktid != 0 */
	ASSERT(lb->mem.pktid != PKT_INVALID_ID);
	lb->mem.refcnt = 1;
	lb->flags = flags;

	return (lb);

error:
	lbuf_allocfail++;
	return NULL;

}

static void
lb_free_one(struct lbuf *lb)
{
	ASSERT(lb_sane(lb));
	ASSERT(hndrte_pktid_sane(lb));

	if (!lb_isclone(lb)) {
		ASSERT(lb->mem.refcnt > 0);
		if (--lb->mem.refcnt == 0) {
			if (lb_pool(lb)) {
				pktpool_t *pool = lb_getpool(lb);

				ASSERT(pool);
				ASSERT(lb->mem.poolid == POOLID(pool));
				lb_resetpool(lb, pool->plen);
				pktpool_free(pool, lb); /* put back to pool */
			} else	if (!lb_isptblk(lb)) {
					lb->data = (uchar*) 0xdeadbeef;
					hndrte_pktid_release(lb, lb->mem.pktid); /* BCMPKTIDMAP */
					hndrte_free(lb);
			} else {
#ifdef HNDRTE_PT_GIANT
				hndrte_pktid_release(lb, lb->mem.pktid); /* BCMPKTIDMAP */
				hndrte_free_pt(lb);
#else
				ASSERT(0);
#endif // endif
			}
		}
	} else {
		struct lbuf *orig = ((struct lbuf_clone*)lb)->orig;

		ASSERT(!lb_pool(lb)); /* clone is not from pool */

		/* clones do not get refs, just originals */
		ASSERT(lb->mem.refcnt == 0);
		lb->data = (uchar*) 0xdeadbeef;
		hndrte_pktid_release(lb, lb->mem.pktid); /* BCMPKTIDMAP */
		hndrte_free(lb);

		lb_free_one(orig);
	}
}

void
lb_free(struct lbuf *lb)
{
	struct lbuf *next;
	bool snarf;

	while (lb) {
		ASSERT(PKTLINK(lb) == NULL);

		next = PKTNEXT(OSH_NULL, lb);

		if (BCMLFRAG_ENAB()) {
			/* if a tx frag or rx frag , go to callback functions before free up */
			if (lbuf_info.cb) {
				PKTSETNEXT(OSH_NULL, lb, NULL);
				snarf = lbuf_info.cb(lbuf_info.arg, lb);
				if (snarf) {
					lb = next;
					continue;
				}
			}
		}
#ifdef BCMPCIEDEV_ENABLED
		/* If freed lbuf belongs to HostFetch pool, invoke callback */
		if (fetch_info && ((lb_getpool(lb) == fetch_info->pool) ||
			(PKTISTXFRAG(OSH_NULL, lb) && PKTHASHEAPBUF(OSH_NULL, lb))))
			hndrte_lb_free_cb();
#endif /* BCMPCIEDEV */

		lb_free_one(lb);
		lb = next;
	}
}

struct lbuf *
lb_dup(struct lbuf *lb)
{
	struct lbuf *lb_new;

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	if (!(lb_new = lb_alloc(lb->len, lbuf_basic, __FILE__, __LINE__)))
#else
	if (!(lb_new = lb_alloc(lb->len, lbuf_basic)))
#endif // endif
		return (NULL);

	bcopy(lb->data, lb_new->data, lb->len);

	return (lb_new);
}

/* XXX:
 * Should be used at the dma_rx_getnextp, DMA is given a bigger buffer but the actual packet data
 * len is small, if this is used with in the driver, there are lots of error cases one need to
 * deal with..(pktcallbacks, stored packetcallback arguments, pkttag info etc)
*/

struct lbuf *
lb_shrink(struct lbuf *lb)
{
	ASSERT(PKTLINK(lb) == NULL);

	if (lb->len < LB_SHRINK_THRESHOLD) {

		struct lbuf *lb_new;
		uchar * head = lb_head(lb);

		/* original packet headroom */
		uint headroom = (uintptr)lb->data - (uintptr)head;

		/* preserve the alignment of the old data */
		headroom += (uintptr)lb->data & 0x3;

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
		lb_new = lb_alloc(lb->len + headroom, lbuf_basic, __FILE__, __LINE__);
#else
		lb_new = lb_alloc(lb->len + headroom, lbuf_basic);
#endif // endif
		if (lb_new != NULL) {
			lb_pull(lb_new, headroom);
			bcopy(lb->data, lb_new->data, lb->len);
			lb_new->flags = lb->flags & ~(LBF_CLONE);
			lb_free(lb);
			lb = lb_new;
		}
	}
	return lb;
}

struct lbuf *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
lb_clone(struct lbuf *lb, int offset, int len, const char *file, int line)
#else
lb_clone(struct lbuf *lb, int offset, int len)
#endif // endif
{
	struct lbuf *clone;
	struct lbuf *orig;

	ASSERT(offset >= 0 && len >= 0);

	if (hndrte_pktid_free_cnt() == 0) {
		LBUF_MSG(("lb_clone: hndrte_pktid_free_cnt 0\n"));
		hndrte_pktid_inc_fail_cnt();
		return (NULL);
	}

	if (offset < 0 || len < 0)
	        return (NULL);

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	if (!file)
		file = "unknown";

	clone = (struct lbuf*)hndrte_malloc_align(sizeof(struct lbuf_clone), 0, file, line);

	if (clone == NULL) {
		printf("lb_clone: size (%u); alloc failed; file %s; line %d\n",
		       LBUFSZ, file, line);
		return (NULL);
	}
#else
	if ((clone = (struct lbuf*)hndrte_malloc_align(sizeof(struct lbuf_clone), 0)) == NULL) {
		LBUF_MSG(("lb_clone: size (%u); alloc failed;\n", LBUFSZ));
		return (NULL);
	}
#endif /* BCMDBG_MEM || BCMDBG_MEMFAIL */

	/* find the actual non-clone original lbuf */
	if (lb_isclone(lb))
		orig = ((struct lbuf_clone*)lb)->orig;
	else
		orig = lb;

	ASSERT(orig->mem.refcnt < 255);
	orig->mem.refcnt++;

	ASSERT(ISALIGNED(clone, sizeof(int)));
	bzero(clone, sizeof(struct lbuf_clone));

	/* clone's data extent is a subset of the current data of lb */
	ASSERT(offset + len <= lb->len);

	clone->data = lb->data + offset;

#if defined(HNDLBUFCOMPACT)
	clone->head_off = ((uintptr)(clone->data) & LB_HEADLO_MASK);
	clone->end_off = len;
	ASSERT(((uintptr)(clone->data + len) & LB_HEADHI_MASK) ==
	       ((uintptr)(clone->data) & LB_HEADHI_MASK));
#else  /* ! HNDLBUFCOMPACT */
	clone->head = clone->data;
	clone->end = clone->head + len;
#endif /* ! HNDLBUFCOMPACT */

	clone->len = len;
	clone->flags |= LBF_CLONE;
	clone->mem.pktid = hndrte_pktid_allocate(clone); /* BCMPKTIDMAP: id!=0 */
	ASSERT(clone->mem.pktid != PKT_INVALID_ID);

	((struct lbuf_clone*)clone)->orig = orig;

	return (clone);
}

/*
 * lb_sane() is invoked in various lb_xyz() functions. In the implementation of
 * lb_sane(), please do not include any calls to lb_xyz() to avoid stack
 * overflow from recursive calls.
 */
bool
lb_sane(struct lbuf *lb)
{
	int insane = 0;

#if defined(BCMPKTIDMAP)
	if ((lb->mem.pktid == PKT_NULL_ID) || (lb->mem.pktid > PKT_MAXIMUM_ID)) {
		insane |= 1;
	}
#endif /*   BCMPKTIDMAP */

	if (lb->mem.poolid != PKTPOOL_INVALID_ID) { /* From a pktpool */
		if (lb->mem.poolid > PKTPOOL_MAXIMUM_ID)
			insane |= 1;
		else
			insane |= (PKTPOOL_ID2PTR(lb->mem.poolid)->id != lb->mem.poolid);
	}

	insane |= (lb->data < LB_HEAD(lb));
	insane |= (lb->data + lb->len > LB_END(lb));

	if (insane) {
		LBUF_MSG(("lb_sane:\n"
		          "lbuf %p data %p head %p end %p len %d flags %d"
		          "pktid %u poolid %u\n",
		          lb, lb->data, LB_HEAD(lb), LB_END(lb), lb->len, lb->flags,
		          lb->mem.pktid, lb->mem.poolid));
	}

	return (!insane);
}

void
lb_audit(struct lbuf * lb)
{
	ASSERT(lb != NULL);

	if (lb_is_frag(lb)) {
		do {
			uint16 fragix, flen = 0;

			fragix = LBFP(lb)->flist.finfo[LB_FRAG_CTX].ctx.fnum;
			ASSERT((fragix >= 1) && (fragix <= LB_FRAG_MAX));
			while (fragix > 0) {
				ASSERT(LBFP(lb)->flist.finfo[fragix].frag.data_lo != 0);
				ASSERT(LBFP(lb)->flist.flen[fragix] > 0);
				flen += LBFP(lb)->flist.flen[fragix];
				fragix--;
			}
			ASSERT(flen == LBFP(lb)->flist.flen[LB_FRAG_CTX]);
			lb = PKTNEXT(OSH_NULL, lb);
			if (lb)
				ASSERT((lb->data == NULL) && (lb->len == 0));	/* disallow? */
		} while (lb != NULL);
	} else 	{
		do {
			ASSERT(lb->data != NULL);
			ASSERT(lb_sane(lb) == 0);
			lb = PKTNEXT(OSH_NULL, lb);
		} while (lb != NULL);
	}
}

/*
 * reset lbuf before recycling to pool
 *      PRESERVE                        FIX-UP
 *      mem.pktid, mem.poolid           mem.refcnt, mem.poolstate
 *      offsets | head, end
 *                                      data
 *                                      len, flags
 *                                      reset all other fields
 */
void
lb_resetpool(struct lbuf *lb, uint16 len)
{
#ifdef BCMPCIEDEV_ENABLED
	uint16 rxcpl_id = 0;

	rxcpl_id = lb->rxcpl_id;
#endif // endif

	ASSERT(lb_sane(lb));
	ASSERT(lb_pool(lb));

	lb->mem.refcnt = 1;
#ifdef BCMDBG_POOL
	lb_setpoolstate(lb, POOL_IDLE); /* mem.poolstate */
#endif // endif

	lb->data = lb_end(lb) - ROUNDUP(len, sizeof(int));
	lb->len = len;
	if (lb->flags & LBF_TX_FRAG) {
		lb->flags = lb->flags & (LBF_TX_FRAG | LBF_PTBLK | LBF_RX_FRAG | LBF_HEAPBUF);
	} else {
		lb->flags = lb->flags & (LBF_TX_FRAG | LBF_PTBLK | LBF_RX_FRAG);
	}

	bzero(&lb->reset, LBUFSZ - OFFSETOF(struct lbuf, reset));
#ifdef BCMPCIEDEV_ENABLED
	lb->rxcpl_id = rxcpl_id;
#endif // endif
}

#ifdef BCMDBG
void
lb_dump(void)
{
	uint i;
	LBUF_MSG(("allocfail %d\n", lbuf_allocfail));
	for (i = 0; i < LBUF_BIN_SIZE; i++) {
		LBUF_MSG(("bin[%d] %d ", i, lbuf_hist[i]));
	}
	LBUF_MSG(("\n"));
}
#endif	/* BCMDBG */
