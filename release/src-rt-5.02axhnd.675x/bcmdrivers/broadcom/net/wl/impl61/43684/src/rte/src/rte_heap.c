/** @file hnd_heap.c
 *
 * HND heap management.
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
 * $Id: rte_heap.c 775419 2019-05-29 19:55:17Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>

#include <rte_mem.h>
#include "rte_mem_priv.h"
#include <rte_heap.h>
#include "rte_heap_priv.h"
#include <rte_cons.h>
#include <rte_trap.h>

#include <osl.h>
#include <osl_ext.h>
#include <bcm_buzzz.h>

#include <awd.h>

/* debug */
#ifdef BCMDBG
#define HND_MSG(x) printf x
#else
#define HND_MSG(x)
#endif // endif
/*
 * ======RTE====== Memory allocation:
 *	hnd_free(w): Free previously allocated memory at w
 *	hnd_malloc_align(size, abits): Allocate memory at a 2^abits boundary
 *	hnd_memavail(): Return a (slightly optimistic) estimate of free memory
 *	hnd_hwm(): Return a high watermark of allocated memory
 *	hnd_print_heapuse(): Dump heap usage stats.
 *	hnd_print_memwaste(): Malloc memory to simulate low memory environment
 *	hnd_print_malloc(): (BCMDBG_MEM) Dump free & inuse lists
 *	hnd_arena_add(base, size): Add a block of memory to the arena
 */
#define	MIN_MEM_SIZE	8	/* Min. memory size is 8 bytes */
#define	MIN_ALIGN	4	/* Alignment at 4 bytes */
#ifdef HNDLBUFCOMPACT
#define	MAX_ALIGN	LB_RAMSIZE
#else
#define	MAX_ALIGN	16384	/* Max alignment at 16k */
#endif // endif
#define	ALIGN(ad, al)	(((ad) + ((al) - 1)) & ~((al) - 1))
#define	ALIGN_DOWN(ad, al)	((ad) & ~((al) - 1))

#if defined(BCMDBG_MEM)

typedef struct _dbg_mem {
	uint32		magic;
	uchar		*malloc_function;
	union {
		uchar	*free_function;
		uint32	malloc_sequence;
	} u;
	int32		size;
	struct _dbg_mem	*next;
} dbg_mem_t;

typedef dbg_mem_t mem_t;
typedef dbg_mem_t mem_use_t;

#ifdef HNDLBUFCOMPACT
int lbufcompact_fixup_2M;
#endif // endif

#else /* !defined(BCMDBG_MEM) */

typedef struct _mem {
	uint32		size;
	struct _mem	*next;
} base_mem_t;

typedef base_mem_t mem_t;

typedef struct _mem_use {
	uint32		size;
} mem_use_t;

#endif /*  defined(BCMDBG_MEM) */

#if defined(BCMDBG_MEM)
static mem_t	dbg_free_mem;		/* Free list head */
#else
static mem_t	free_mem;		/* Free list head */
#endif /*  defined(BCMDBG_MEM) */
static mem_t	persist_mem;		/* Persistent list head */

/* The signature field is packed into the unused bottom two bits of the
 * size field, thus the enumeration space is completely full with the
 * following definitions.
 */
enum mem_t_signature_flags {
	MEM_T_FREE_BLOCK = 0x0,
	MEM_T_FREE_BLOCK_SMALL = 0x1, /* Not currently used */
	MEM_T_ALLOCATED_BLOCK = 0x2,
	MEM_T_PERSISTENT_CONTAINER = 0x3
};
#define MEM_T_SIGNATURE_MASK 0x3
#define MEM_T_SIZE_MASK (~MEM_T_SIGNATURE_MASK)

#define GET_MEM_T_SIGNATURE(_header_ptr)	\
	((_header_ptr)->size & MEM_T_SIGNATURE_MASK)
#define SET_MEM_T_SIGNATURE(_header_ptr, _signature)	\
	((_header_ptr)->size = GET_MEM_T_SIZE(_header_ptr) | ((_signature) & MEM_T_SIGNATURE_MASK))
#define GET_MEM_T_SIZE(_header_ptr)	\
	((_header_ptr)->size & MEM_T_SIZE_MASK)
#define SET_MEM_T_SIZE(_header_ptr, _size)	\
	((_header_ptr)->size = ((_size) & MEM_T_SIZE_MASK) | GET_MEM_T_SIGNATURE(_header_ptr))
#define GET_MEM_T_NEXT(_header_ptr)	\
	((_header_ptr)->next)
#define SET_MEM_T_NEXT(_header_ptr, _next)	\
	((_header_ptr)->next = (_next))
#define GET_MEM_T_HDR_SIZE(_header_ptr)		\
	(GET_MEM_T_SIGNATURE(_header_ptr) ==	\
	 MEM_T_ALLOCATED_BLOCK ? sizeof(mem_use_t) : sizeof(mem_t))
#define IS_A_FREE_BLOCK(_header_ptr)	((GET_MEM_T_SIGNATURE(_header_ptr) == MEM_T_FREE_BLOCK))

/* Here we must write the "next" field first. This is necessary in the case of initialising a
 * structure from a source structure 4-bytes earlier in memory as we can tromp the source
 * structure's size field if we don't do this correctly. This case occurs in the
 * hnd_malloc_persist_align function when a block size of 4-byte is requested.
 */
#define INIT_MEM_T(_header_ptr, _size, _signature, _next)	\
do {								\
	(_header_ptr)->next = (_next);				\
	(_header_ptr)->size = (((_size) & MEM_T_SIZE_MASK) |	\
	                       ((_signature) & MEM_T_SIGNATURE_MASK));	\
} while (0)

static mem_t* hnd_persistmem_get(void);
static mem_t *find_preceeding_list_block(mem_t *list, const mem_t *curr);
static mem_t *get_viable_persist_segment(uint32 size);

static mem_t* hnd_freemem_get(void);
static void trap_align_err(uint32 addr, uint align);

static uint	arena_size;		/* Total heap size */
static uint	inuse_size;		/* Current in use */
static uint	inuse_overhead;		/* tally of allocated mem_t blocks */
static uint	inuse_hwm;		/* High watermark of memory - reclaimed memory */
static uint	mf_count;		/* Malloc failure count */
static uint	arena_lwm = -1;	/* Least Heap size since load */
#ifdef MEM_ALLOC_STATS
static uint	max_flowring_alloc;
static uint	max_bsscfg_alloc;
static uint	max_scb_alloc;
#endif /* MEM_ALLOC_STATS */

/* mutex macros for thread safe */
#ifdef HND_HEAP_THREAD_SAFE
#define HND_HEAP_MUTEX_DECL(mutex)		OSL_EXT_MUTEX_DECL(mutex)
#define HND_HEAP_MUTEX_CREATE(name, mutex)	osl_ext_mutex_create(name, mutex)
#define HND_HEAP_MUTEX_DELETE(mutex)		osl_ext_mutex_delete(mutex)
#define HND_HEAP_MUTEX_ACQUIRE(mutex, msec)	osl_ext_mutex_acquire(mutex, msec)
#define HND_HEAP_MUTEX_RELEASE(mutex)	osl_ext_mutex_release(mutex)
#else
#define HND_HEAP_MUTEX_DECL(mutex)
#define HND_HEAP_MUTEX_CREATE(name, mutex)	OSL_EXT_SUCCESS
#define HND_HEAP_MUTEX_DELETE(mutex)		OSL_EXT_SUCCESS
#define HND_HEAP_MUTEX_ACQUIRE(mutex, msec)	OSL_EXT_SUCCESS
#define HND_HEAP_MUTEX_RELEASE(mutex)	OSL_EXT_SUCCESS
#endif	/* HND_HEAP_THREAD_SAFE */

HND_HEAP_MUTEX_DECL(heap_mutex);

#ifdef HNDLBUFCOMPACT
static void hnd_lbuf_fixup_2M_tcm(void);
#endif // endif

#if defined(BCMDBG_MEM)
#define	MEM_MAGIC	0x4d4e4743	/* Magic # for mem overwrite check: 'MNGC' */
static mem_t	inuse_mem;		/* In-use list head */
uint32 mem_dbg_malloc_cnt;		/* Free running counter of allocation. Useful to track
					 * as to when a particular mem is allocated.
					 */
#if defined(RTE_CONS) && !defined(BCM_BOOTLOADER)
static void hnd_print_malloc(void *arg, int argc, char *argv[]);
#endif /* defined(RTE_CONS) && !defined(BCM_BOOTLOADER) */
#endif /* defined(BCMDBG_MEM) */

static mem_t*
BCMRAMFN(hnd_persistmem_get)(void)
{
	return (&persist_mem);
}

static mem_t*
BCMRAMFN(hnd_freemem_get)(void)
{
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEM2)
	return (&dbg_free_mem);
#else /* defined(BCMDBG_MEM) || defined(BCMDBG_MEM2) */
	return (&free_mem);
#endif /* defined(BCMDBG_MEM) || defined(BCMDBG_MEM2) */
}

/* Check memory alignment and trap on failure */
static void
trap_align_err(uint32 addr, uint align)
{
	if (!ISALIGNED(addr, align)) {
		printf("%s: Invalid pointer 0x%08x which is not %d bytes aligned. \n",
			__FUNCTION__, addr, align);
		HND_DIE();
	}
}

typedef struct _arena_segment {
	uint32	size;
	void	*base;
} arena_segment_t;

#define MAX_ARENA_SEGMENTS 8
static arena_segment_t arena_segments[MAX_ARENA_SEGMENTS];
static uint32 arena_segment_count = 0;

/* To prevent ROMming shdat issue because of ROMmed functions accessing RAM */
static arena_segment_t* BCMRAMFN(get_arena_segments)(void)
{
	return arena_segments;
}

static uint32* BCMRAMFN(get_arena_segment_count)(void)
{
	return &arena_segment_count;
}

static void
BCMRAMFN(record_arena_segment)(void *base, uint32 size)
{
	if (arena_segment_count < MAX_ARENA_SEGMENTS) {
		/* Add new element */
		arena_segments[arena_segment_count].base = base;
		arena_segments[arena_segment_count].size = size;
		arena_segment_count++;

#ifdef BCMDBG_HEAPCHECK
		arena_merge_segment();
#endif /* BCMDBG_HEAPCHECK */
	}
	else
	{
		/* If this check fires, it just means that there are more arena segments than slots
		 * in the tracking array. Increment MAX_ARENA_SEGMENTS above to permit tracking of
		 * all segments.
		 */
		printf("%s: Error Number of arena segments exceed MAX_ARENA_SEGMENTS\n",
		       __FUNCTION__);
		HND_DIE();
	}
}
#ifdef BCMDBG_HEAPCHECK
void
arena_merge_segment()
{

	uint i, j;
	arena_segment_t tmp;
	uint32 *countptr;
	arena_segment_t *segments;

	countptr = get_arena_segment_count();
	segments = get_arena_segments();

	/* Sort the array */
	for (i = 1; i < *countptr; i++)
	{
		j = i;
		while (j > 0 && segments[j - 1].base > segments[j].base)
		{
			/* Swap A[j] and A[j - 1] */
			tmp = segments[j];
			segments[j] = segments[j - 1];
			segments[j - 1] = tmp;
			j--;
		}
	}

	/* Combine contiguous */
	for (i = 1; i < *countptr; i++)
	{
		arena_segment_t prev = segments[i - 1];
		arena_segment_t curr = segments[i];
		if ((uint32)prev.base + (uint32)prev.size == (uint32)curr.base)
		{
			/* Combine contiguous block */
			segments[i - 1].size += segments[i].size;
			/* Shuffle the rest down */
			for (j = i + 1; j < *countptr; j++)
			{
				segments[j - 1] = segments[j];
			}
			(*countptr)--;
			i--;
		}
	}
}
#endif /* BCMDBG_HEAPCHECK */

bool
BCMATTACHFN(hnd_arena_init)(uintptr base, uintptr lim)
{
	mem_t *first;
	mem_t *free_memptr;

	ASSERT(base);
	ASSERT(lim > base);

	/* create mutex for critical section locking */
	if (HND_HEAP_MUTEX_CREATE("heap_mutex", &heap_mutex) != OSL_EXT_SUCCESS) {
		return FALSE;
	}

	/* Align */
	first = (mem_t *)ALIGN(base, MIN_ALIGN);

	arena_size = lim - (uint32)first;
	inuse_size = 0;
	inuse_overhead = 0;
	inuse_hwm = 0;

	mf_count = 0;
	free_memptr = hnd_freemem_get();
#if defined(BCMDBG_MEM)
	free_memptr->magic = inuse_mem.magic = first->magic = MEM_MAGIC;
	inuse_mem.next = NULL;
	first->malloc_function = (uchar*)NULL;
	first->u.free_function = CALL_SITE;
#endif /* defined(BCMDBG_MEM) */

#ifdef MEM_ALLOC_STATS
	max_flowring_alloc = 0;
	max_bsscfg_alloc = 0;
	max_scb_alloc = 0;
#endif /* MEM_ALLOC_STATS */

	INIT_MEM_T(first, arena_size - sizeof(mem_t), MEM_T_FREE_BLOCK, NULL);
	SET_MEM_T_NEXT(free_memptr, first);
	record_arena_segment(first, arena_size);

#ifdef HNDLBUFCOMPACT
	hnd_lbuf_fixup_2M_tcm();
#endif // endif
	return TRUE;
}

uint
hnd_arena_add(uint32 base, uint size)
{
	uint32 addr;
	mem_use_t *this;

	addr = ALIGN(base, MIN_ALIGN);
	if ((addr - base) > size) {
		/* Ignore this miniscule thing,
		 * otherwise size below will go negative!
		 */
		return 0;
	}
	size -= (addr - base);
	size = ALIGN_DOWN(size, MIN_ALIGN);

	if (size < (sizeof(mem_t) + MIN_MEM_SIZE)) {
		return 0;
	}

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return 0;

	this = (mem_use_t *)addr;
	arena_size += size;
	record_arena_segment(this, size);
	size -= sizeof(mem_use_t);
	addr += sizeof(mem_use_t);
	SET_MEM_T_SIZE(this, size);
	SET_MEM_T_SIGNATURE(this, MEM_T_ALLOCATED_BLOCK);

#if defined(BCMDBG_MEM)
	this->magic = MEM_MAGIC;
	SET_MEM_T_NEXT(this, inuse_mem.next);
	inuse_mem.next = this;
	printf("%s: Adding %p: 0x%x(%d) @ 0x%x\n", __FUNCTION__, this, size, size, addr);
#endif /* defined(BCMDBG_MEM) */

	/* This chunk was not in use before, make believe it was */
	inuse_size += GET_MEM_T_SIZE(this);
	inuse_overhead += GET_MEM_T_HDR_SIZE(this);

	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return 0;

	hnd_free((void *)addr);

#ifdef HNDLBUFCOMPACT
	hnd_lbuf_fixup_2M_tcm();
#endif // endif

	/* Reset available memory low water mark after reclaim */
	arena_lwm = arena_size - inuse_size - inuse_overhead;

	return (size);
}

void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hnd_malloc_persist_attach_align(uint32 size, uint alignbits, void *call_site)
#else
hnd_malloc_persist_attach_align(uint32 size, uint alignbits)
#endif /* BCMDBG_MEM */
{
	void *ret;
	if (ATTACH_PART_RECLAIMED()) {
#if defined(BCMDBG_MEM)
		ret = hnd_malloc_align(size, alignbits, call_site);
#else
		ret = hnd_malloc_align(size, alignbits);
#endif /* defined(BCMDBG_MEM) */
	} else {
#if defined(BCMDBG_MEM)
		ret = hnd_malloc_align(size, alignbits, call_site);
#else
		ret = hnd_malloc_persist_align(size, alignbits);
#endif /* defined(BCMDBG_MEM) */
	}
	return ret;
}

static mem_t *
find_preceeding_list_block(mem_t *list, const mem_t *curr)
{
	mem_t *next, *prev;

	/* Find the free block in the free list */
	prev = list;
	while ((next = GET_MEM_T_NEXT(prev)) != NULL) {
		if (next >= curr)
			break;
		prev = next;
	}
	return prev;
}

static mem_t *
get_viable_persist_segment(uint32 size)
{
	mem_t	*persist_mem_ptr;
	mem_t	*last;
	mem_t   *following_mem_ptr;
	mem_t	*curr;
	mem_t	*prev;
	uint32	curr_size;
	int32	max_size = 0;
	mem_t	*max_region = NULL;
	int32	remainder;
	uint32 header_offset = 0;
	persist_mem_ptr = hnd_persistmem_get();
	uint waste;
	mem_t *this = NULL;

#if defined(BCMDBG_MEM)
	/* For debugging purposes we can insert a mem_t structure to wrap allocation units
	 * and check for memory pool corruption in the persist region.
	 */
	header_offset = sizeof(mem_use_t);
#endif /* defined(BCMDBG_MEM) */

	/* Viable persistent segment:
	 * The persistent malloc algorithm allocates a persistent blocks by extending a persistent
	 * meta container by the size of the allocation requested. This requires that a free block
	 * that is large enough to be shrunk by the allocation size immediately follows the
	 * persistent meta container. The memory is essentially traded out of the following free
	 * block into the persistent meta container in order to make the allocation. If there are
	 * no persistent blocks with a sufficiently large free block following, a new persistent
	 * region is created in the largest free area in the complete arena. If no viable free
	 * region can be found to fit the requested allocation, the allocation fails.
	 */

	/* Go through each of the persist segments */
	last = persist_mem_ptr;
	waste = arena_size;
	while ((curr = GET_MEM_T_NEXT(last)) != NULL) {
		curr_size = GET_MEM_T_SIZE(curr);
		ASSERT(GET_MEM_T_SIGNATURE(curr) == MEM_T_PERSISTENT_CONTAINER);
		following_mem_ptr = (mem_t *) ((uint32) curr + sizeof(mem_t) + curr_size);
		if (IS_A_FREE_BLOCK(following_mem_ptr) &&
		    GET_MEM_T_SIZE(following_mem_ptr) > (size + header_offset) &&
		    (GET_MEM_T_SIZE(following_mem_ptr) - size) < waste)
		{
			/* found a viable candidate */
			waste = GET_MEM_T_SIZE(following_mem_ptr) - size;
			this = curr;

			if (waste == 0) {
				break;
			}
		}
		last = curr;
	}
	if (this != NULL) {
		curr = this;
	}
	else {
		/* No viable segments found - find largest free segments to create new persist
		 * region.
		 */
		last = hnd_freemem_get();
		while ((curr = GET_MEM_T_NEXT(last)) != NULL) {
			/*
			 * Check so that we return the largest available viable segment - reduces
			 * segment instances.
			 * Here we add sizeof(mem_t) so we can always fit a following free region
			 */
			remainder = GET_MEM_T_SIZE(curr) - (size + sizeof(mem_t) + header_offset);
			ASSERT(GET_MEM_T_SIGNATURE(curr) == MEM_T_FREE_BLOCK);
			if (remainder > max_size) {
				max_size = remainder;
				max_region = curr;
			}
			last = curr;
		}
		curr = max_region;

		if (curr != NULL)
		{
			mem_t *new = (mem_t *) ((uint32)curr + sizeof(mem_t));

			/* create new free region */
			INIT_MEM_T(new, GET_MEM_T_SIZE(curr) - sizeof(mem_t),
			           MEM_T_FREE_BLOCK, GET_MEM_T_NEXT(curr));
#if defined(BCMDBG_MEM)
			new->magic = MEM_MAGIC;
#endif /* defined(BCMDBG_MEM) */
			/* link new free region to list */
			prev = find_preceeding_list_block(hnd_freemem_get(), curr);
			SET_MEM_T_NEXT(prev, new);
			/* create new persist header and link into monotonical list ordering */
			prev = find_preceeding_list_block(hnd_persistmem_get(), curr);
			INIT_MEM_T(curr, 0, MEM_T_PERSISTENT_CONTAINER, GET_MEM_T_NEXT(prev));
			SET_MEM_T_NEXT(prev, curr);
			inuse_overhead += GET_MEM_T_HDR_SIZE(curr);
		}
	}

	return curr;
}

void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hnd_malloc_persist_align(uint32 size, uint alignbits, void *call_site)
#else
hnd_malloc_persist_align(uint32 size, uint alignbits)
#endif /* BCMDBG_MEM */
{
	mem_t	*this = NULL, *prev;
	mem_t   *free_follow;
	mem_t   *free_reloc;
	mem_t   *persist;
	uint32 persist_size;
	uint32 header_offset = 0;
#ifdef BCMDBG_MEM
	/* For debugging purposes we can insert a mem_t structure to wrap allocation units
	 * and check for memory pool corruption in the persist region.
	 */
	header_offset = sizeof(mem_t);
#endif /* BCMDBG_MEM */

	if ((1 << alignbits) > MIN_ALIGN)
	{
		/* If not word alignment just default to normal allocation. */
#ifndef BCMDBG_MEM
		this = hnd_malloc_align(size, alignbits);
#else /* BCMDBG_MEM */
		this = hnd_malloc_align(size, alignbits, call_site);
#endif /* !BCMDBG_MEM */
	}
	else
	{
		size = ALIGN(size, MIN_ALIGN);

		if ((persist = get_viable_persist_segment(size)) != NULL) {
			persist_size = GET_MEM_T_SIZE(persist);
			ASSERT(GET_MEM_T_SIGNATURE(persist) == MEM_T_PERSISTENT_CONTAINER);

			free_follow = (mem_t *)((uint32)persist + sizeof(mem_t) + persist_size);

			/* Find the previous free block in the free list */
			prev = find_preceeding_list_block(hnd_freemem_get(), free_follow);
			ASSERT(free_follow == GET_MEM_T_NEXT(prev));
			ASSERT(GET_MEM_T_SIZE(free_follow) >= (size + header_offset));

			free_reloc = (mem_t *)((uint32)free_follow + size + header_offset);
			INIT_MEM_T(free_reloc, GET_MEM_T_SIZE(free_follow) - size - header_offset,
			           MEM_T_FREE_BLOCK, GET_MEM_T_NEXT(free_follow));

#ifdef BCMDBG_MEM
			free_reloc->magic = MEM_MAGIC;

			/* Fill tracking mem_t fields. */
			free_follow->magic = MEM_MAGIC;
			INIT_MEM_T(free_follow, size, MEM_T_PERSISTENT_CONTAINER, NULL);

			free_follow->malloc_function = call_site;
			free_follow->u.free_function = NULL;
			SET_MEM_T_NEXT(free_follow, inuse_mem.next);
			inuse_mem.next = free_follow;

#endif /* BCMDBG_MEM */

			/* Link new free block into the list */
			SET_MEM_T_NEXT(prev, free_reloc);

			/* Return pointer to the now-used memory */
			this = (mem_t *)((uint32)free_follow + header_offset);

			/* Increase the size of persistent meta container */
			SET_MEM_T_SIZE(persist, GET_MEM_T_SIZE(persist) + size + header_offset);

			inuse_overhead += header_offset;
			inuse_size += size;
		}
	}

	if (this != NULL) {
		memset(this, 0, size);
	}

	trap_align_err((uint32)this, MIN_ALIGN);

	return this;
}

void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hnd_malloc_align(uint size, uint alignbits, void *call_site)
#else
hnd_malloc_align(uint size, uint alignbits)
#endif /* BCMDBG_MEM */
{
	mem_t	*curr, *last, *this = NULL, *prev = NULL;
	uint	align, waste;
	int	rem;
	uintptr	addr = 0, top = 0;
	uint	inuse_total;
	uint	size_orig;
	void	*ptr = NULL;
	uint	memavail;

	ASSERT(size);
	size_orig = size;

	size = ALIGN(size_orig, MIN_ALIGN);
#if defined(BCMDBG_HEAPCHECK)
	hnd_memcheck("malloc", 0);
#endif /* BCMDBG_HEAPCHECK */
	BUZZZ_LVL4(HND_MALLOC, 2, (uint32)CALL_SITE, size);

	align = 1 << alignbits;
	if (align <= MIN_ALIGN)
		align = MIN_ALIGN;
	else if (align > MAX_ALIGN)
		align = MAX_ALIGN;

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return  NULL;

	/* Search for available memory */
	last = hnd_freemem_get();
	waste = arena_size;

	/* Algorithm: best fit */
	while ((curr = GET_MEM_T_NEXT(last)) != NULL) {
		if ((GET_MEM_T_SIZE(curr) + sizeof(mem_t) - sizeof(mem_use_t)) >= size) {
			/* Calculate alignment */
			uintptr lowest = (uintptr)curr + sizeof(mem_use_t);
			uintptr end = (uintptr)curr + GET_MEM_T_SIZE(curr) + sizeof(mem_t);
			uintptr highest = end - size;
			uintptr a = ALIGN_DOWN(highest, align);
			/* This segment deals with the case where we overlap the containing free
			 * header with the allocated block header. In this case we need to
			 * try the next aligned value lower in the hope it will match 'lowest'.
			 */
			if ((a != lowest) &&
			    (a - (uintptr)curr - sizeof(mem_use_t) < sizeof(mem_t)))
			{
				a = ALIGN_DOWN(a - 1, align);
			}

			/* Find closest sized buffer to avoid fragmentation BUT aligned address
			 * must be greater or equal to the lowest address available in the free
			 * block AND lowest address is aligned with "align" bytes OR
			 * space preceeding a returned block's header is either big
			 * enough to support another free block
			 */
			if (a >= lowest &&
			    (lowest == a || (a-lowest) >= sizeof(mem_t)) &&
			    (GET_MEM_T_SIZE(curr) + sizeof(mem_t) - size - sizeof(mem_use_t)) <
			    waste)
			{

				waste = GET_MEM_T_SIZE(curr) + sizeof(mem_t) - size -
				        sizeof(mem_use_t);
				this = curr;
				prev = last;
				top = end;
				addr = a;

				if (waste == 0)
					break;
			}
		}
		last = curr;
	}

	if (this == NULL) {
		mf_count++; /* Increment malloc failure count */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)

#ifdef HNDLBUFCOMPACT
		/* Don't consider the allocation failure when 2M boundary hole is being made. */
		if (lbufcompact_fixup_2M == FALSE)
#endif /* HNDLBUFCOMPACT */
		{
			printf("No memory to satisfy request for %d bytes, inuse %d, caller 0x%p\n",
				size, (inuse_size + inuse_overhead), call_site);
			/* A failing memory allocation doesn't necessarily mean that something
			 * unexpected has happened and that the firmware will crash. Proper
			 * firmware design tries to deal with this to a certain extend. Hence, the
			 * OSL_SYS_HALT() below was commented out.
			 */
			// OSL_SYS_HALT();
		}
#else
		HND_MSG(("No memory to satisfy request %d bytes, inuse %d\n", size,
		         (inuse_size + inuse_overhead)));

#endif /* BCMDBG_MEM */
		goto done;
	}

	/* Ensure either perfect fit, or enough remainder for mem_t + mem_use_t */
	ASSERT(((int)this + (int) sizeof(mem_use_t) == addr) ||
	       ((int)GET_MEM_T_SIZE(this) - (int) size >= (int)sizeof(mem_use_t)));

#if defined(BCMDBG_MEM)
	ASSERT(this->magic == MEM_MAGIC);
#endif // endif

	/* best fit has been found as below
	 *  - split above or below if tht's big enough
	 *  - otherwise adjust size to absorb those tiny gap
	 *
	 *      ----------------  <-- this
	 *          mem_t
	 *      ----------------
	 *
	 *       waste(not used)
	 *
	 *      ----------------  <-- addr
	 *      alignment offset
	 *      ----------------
	 *	    size
	 *
	 *      ----------------  <-- top
	 */

	/* Anything above? */
	rem = (top - addr) - size;
	if (rem < sizeof(mem_t)) {
		/* take it all */
		size += rem;
	} else {
		/* Split off the top */
		mem_t *new = (mem_t *)(addr + size);

		SET_MEM_T_SIZE(this, GET_MEM_T_SIZE(this) - rem);
		SET_MEM_T_SIZE(new, rem - sizeof(mem_t));
		SET_MEM_T_SIGNATURE(new, MEM_T_FREE_BLOCK);
#if defined(BCMDBG_MEM)
		new->magic = MEM_MAGIC;
#endif /* defined(BCMDBG_MEM) */
		SET_MEM_T_NEXT(new, GET_MEM_T_NEXT(this));
		SET_MEM_T_NEXT(this, new);
	}

	/* Anything below? */
	rem = GET_MEM_T_SIZE(this) - size;
	ASSERT(rem == (sizeof(mem_use_t) - sizeof(mem_t)) || rem >= (int)sizeof(mem_use_t));
#if defined(BCMDBG_MEM)
	if (rem < (int)(sizeof(mem_t)))
#else
	if (rem < (int)(sizeof(mem_t) - sizeof(mem_use_t)))
#endif // endif
	{
		/* perfect fit case */
		SET_MEM_T_NEXT(prev, GET_MEM_T_NEXT(this));
		SET_MEM_T_SIZE(this, GET_MEM_T_SIZE(this) + sizeof(mem_t) - sizeof(mem_use_t));
	} else {
		/* Split this */
		mem_t *new = (mem_t *)((uint32)this + sizeof(mem_t) + rem - sizeof(mem_use_t));

		SET_MEM_T_SIZE(new, size);
		SET_MEM_T_SIZE(this, rem - sizeof(mem_use_t));
#if defined(BCMDBG_MEM)
		new->magic = MEM_MAGIC;
#endif /* BCMDBG_MEM */

		this = new;
	}

	SET_MEM_T_SIGNATURE(this, MEM_T_ALLOCATED_BLOCK);
#if defined(BCMDBG_MEM)
	SET_MEM_T_NEXT(this, inuse_mem.next);
	inuse_mem.next = this;
	this->malloc_function = call_site;
	this->u.malloc_sequence = mem_dbg_malloc_cnt++;
#else
	SET_MEM_T_NEXT(this, (struct _mem *) NULL);
#endif /* defined(BCMDBG_MEM) */

	inuse_size += GET_MEM_T_SIZE(this);
	inuse_overhead += GET_MEM_T_HDR_SIZE(this);

	/* find the instance where the free memory was the least to calculate
	 * inuse memory hwm
	 */
	inuse_total = inuse_size + inuse_overhead;
	if (inuse_total > inuse_hwm)
		inuse_hwm = inuse_total;

	memavail = arena_size - inuse_total;
	if (memavail < arena_lwm)
		arena_lwm = memavail;

	ptr = (void *)((uint32)this + sizeof(mem_use_t));

	memset(ptr, 0, size);

	trap_align_err((uint32)ptr, align);

done:
	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS) {
		if (ptr)
			hnd_free(ptr);
		return NULL;
	}

	return ptr;
} /* hnd_malloc_align */

#ifdef HNDLBUFCOMPACT
/* HNDLBUFCOMPACT is implemented based on an assumption that
 * lbuf head and end addresses falls into the same 2M bytes address boundary.
 *
 * However, ATCM and BTCM are spanning over 2M address boundary in many dongle chips.
 * So, if lbuf is allocated at the end of ATCM, it could cross 2M boundary over to BTCM.
 *
 * In order to avoid this kind of situation, we make a hole of 4 bytes memory at 2M address.
 * This function allocates 4 bytes memory at all possible 2M aligned addresses.
 *
 * This function must be called whenever new memory region is added to arena.
 * (i.e., in hnd_arena_init() and hnd_arena_add())
*/
void
hnd_lbuf_fixup_2M_tcm(void)
{
	/* Reserving 4 bytes memory at all 2M boundary to create a hole */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	lbufcompact_fixup_2M = TRUE;
	while (hnd_malloc_align(4, 21, CALL_SITE) != NULL)
#else
	while (hnd_malloc_align(4, 21) != NULL)
#endif // endif
		;	/* empty */

	mf_count--;	/* decrement malloc failure count to avoid confusion,
			   because this was an intended/expected failure by the logic
			*/
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	lbufcompact_fixup_2M = FALSE;
#endif // endif
}
#endif /* HNDLBUFCOMPACT */

static int
hnd_malloc_size(void *where)
{
	uint32 w = (uint32)where;
	mem_t *this;

#ifdef BCMDBG_MEM
	mem_t *prev;

	/* Get it off of the inuse list */
	prev = &inuse_mem;
	while ((this = GET_MEM_T_NEXT(prev)) != NULL) {
		if (((uint32)this + sizeof(mem_t)) == w)
			break;
		prev = this;
	}

	if (this == NULL) {
		HND_MSG(("%s: 0x%x is not in the inuse list\n", __FUNCTION__, w));
		ASSERT(this);
		return -1;
	}

	if (this->magic != MEM_MAGIC) {
		HND_MSG(("\n%s: Corrupt magic (0x%x) in 0x%x; size %d; allocd by 0x%p 0x\n\n",
		       __FUNCTION__, this->magic, w, GET_MEM_T_SIZE(this), this->malloc_function));
		ASSERT(this->magic == MEM_MAGIC);
		return -1;
	}

#else
	this = (mem_t *)(w - sizeof(mem_use_t));
#endif /* BCMDBG_MEM */

	return GET_MEM_T_SIZE(this);
}

void *
hnd_realloc(void *ptr, uint size)
{
	int osz = hnd_malloc_size(ptr);

	if (osz < 0)
		return NULL;

	void *new = hnd_malloc(size);
	if (new == NULL)
		return NULL;
	memcpy(new, ptr, MIN(size, osz));
	hnd_free(ptr);
	return new;
}

int
hnd_free_persist(void *where)
{
	HND_DIE();
	return 0;
}

int
hnd_free(void *where)
{
	uint32 w = (uint32)where;
	mem_t *prev, *next, *this;
	uint32 free_size, block_start, block_end, block_size, w_start, w_end, i;
	uint32 *countptr;
	arena_segment_t *segments;
	int err = 0;

	BUZZZ_LVL4(HND_FREE, 1, (uint32)CALL_SITE);

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return -1;

	/* Check if location is null */
	if (where == NULL) {
		printf("%s: Free null pointer %p: ra: %p.\n", __FUNCTION__, where,
			(void *)__builtin_return_address(0));
		return 0;
	}

	/* Check if location is within arena block */
	countptr = get_arena_segment_count();
	segments = get_arena_segments();
	for (i = 0; i < *countptr; i++) {
		block_start = (uint32)segments[i].base;
		block_size = segments[i].size;
		if ((w >= block_start) && (w < (block_start + block_size))) {
			break;
		}
	}
	if (i == *countptr) {
		printf("%s: Free invalid pointer %p which is not in heap.\n",
			__FUNCTION__, where);
		HND_DIE();
	}

	/* Check if location is within the free block */
	w_start = w - sizeof(mem_use_t);
	w_end = w + GET_MEM_T_SIZE((mem_t *)w_start);
	prev = hnd_freemem_get();
	while ((this = GET_MEM_T_NEXT(prev)) != NULL) {
		block_start = (uint32)this;
		block_size = GET_MEM_T_SIZE(this);
		block_end = block_start + block_size + sizeof(mem_t);
		if (!(w_end <= block_start || w_start >= block_end)) {
			printf("%s: Free invalid pointer %p [0x%x, 0x%x], which is already freed"
				"in: [0x%x, 0x%x] sz: 0x%08x ra: %p.\n", __FUNCTION__, where,
				w_start, w_end, block_start, block_end, block_size,
				(void *)__builtin_return_address(0));
			HND_DIE();
		}
		prev = this;
	}

	/* Check if location is within a persist structure */
	prev = hnd_persistmem_get();
	while ((this = GET_MEM_T_NEXT(prev)) != NULL) {
		/* Deallocating within a persist region is a bug */
		if (((uint32)where >= (uint32)this) &&
			((uint32)where <
			((uint32)this + GET_MEM_T_SIZE(this) + sizeof(mem_t))))
		{
			/* XXX: Trying to free a persistent allocation in the attach path
			 * (i.e. on cleanup) is a no-op. After attach, it is an error.
			 */
			printf("pointer %p in persist block: %p sz: 0x%08x ra: %p\n", where, this,
				GET_MEM_T_SIZE(this), (void *)CALL_SITE);
			if (ATTACH_PART_RECLAIMED())
				HND_DIE();
			return 0;
		}
		prev = this;
	}

	trap_align_err(w, MIN_ALIGN);

#if defined(BCMDBG_HEAPCHECK)
	hnd_memcheck("free", 0);
#endif // endif

#if defined(BCMDBG_MEM)
	/* Get it off of the inuse list */
	prev = &inuse_mem;
	while ((this = prev->next) != NULL) {
		if (((uint32)this + sizeof(mem_t)) == w)
			break;
		prev = this;
	}

	if (this == NULL) {
		HND_MSG(("%s: 0x%x is not in the inuse list\n", __FUNCTION__, w));
		ASSERT(this);
		err = -1;
		goto done;
	}

	if (this->magic != MEM_MAGIC) {
		HND_MSG(("\n%s: Corrupt magic (0x%x) in 0x%x; size %d; allocd by 0x%p\n\n",
		       __FUNCTION__, this->magic, w, GET_MEM_T_SIZE(this), this->malloc_function));
		ASSERT(this->magic == MEM_MAGIC);
		err = -1;
		goto done;
	}

	this->u.free_function = CALL_SITE;
	SET_MEM_T_NEXT(prev, GET_MEM_T_NEXT(this));
#else
	this = (mem_t *)(w - sizeof(mem_use_t));
	ASSERT(GET_MEM_T_SIGNATURE(this) == MEM_T_ALLOCATED_BLOCK);
#endif /* BCMDBG_MEM */

	free_size = GET_MEM_T_SIZE(this);
	inuse_size -= free_size;
	inuse_overhead -= GET_MEM_T_HDR_SIZE(this);
	SET_MEM_T_SIGNATURE(this, MEM_T_FREE_BLOCK);

	/* Find the right place in the free list for it */
	prev = hnd_freemem_get();
	while ((next = GET_MEM_T_NEXT(prev)) != NULL) {
		if (next >= this)
			break;
		prev = next;
	}

	/* Coalesce with next if appropriate */
	if ((w + GET_MEM_T_SIZE(this)) == (uint32)next) {
		/* Switch from allocated block to free block (extend header) */
		SET_MEM_T_SIZE(this, GET_MEM_T_SIZE(this) + sizeof(mem_use_t) - sizeof(mem_t));
		/* Absorb next block and header into this block. */
		SET_MEM_T_SIZE(this, GET_MEM_T_SIZE(this) + GET_MEM_T_SIZE(next) + sizeof(mem_t));
		SET_MEM_T_NEXT(this, GET_MEM_T_NEXT(next));
#if defined(BCMDBG_MEM)
		next->magic = 0;
#endif /* BCMDBG_MEM */
	} else {
		/* Switch from allocated block to free block (extend header) */
		SET_MEM_T_SIZE(this, GET_MEM_T_SIZE(this) + sizeof(mem_use_t) - sizeof(mem_t));
		SET_MEM_T_NEXT(this, next);
	}

	/* Coalesce with prev if appropriate */
	if (((uint32)prev + sizeof(mem_t) + GET_MEM_T_SIZE(prev)) == (uint32)this) {
		SET_MEM_T_SIZE(prev, GET_MEM_T_SIZE(prev) + GET_MEM_T_SIZE(this) + sizeof(mem_t));
		SET_MEM_T_NEXT(prev, GET_MEM_T_NEXT(this));
#if defined(BCMDBG_MEM)
		this->magic = 0;
#endif /* defined(BCMDBG_MEM) */
	} else
		SET_MEM_T_NEXT(prev, this);

	err = 0;
#ifdef BCMDBG_MEM
done:
#endif /* BCMDBG_MEM */
	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return -1;
	return err;
}

void *
hnd_calloc(uint num, uint size)
{
	void *ptr;

	ptr = hnd_malloc(size*num);
	if (ptr) {
		memset(ptr, 0, size*num);
	}

	return (ptr);
}

uint
hnd_memavail(void)
{
	uint mem_avail;

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return 0;
	mem_avail = arena_size - inuse_size - inuse_overhead;
	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return 0;
	return (mem_avail);
}

void
hnd_meminuse(uint *inuse, uint *inuse_oh)
{
	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return;
	if (inuse != NULL)
		*inuse = inuse_size;
	if (inuse_oh != NULL)
		*inuse_oh = inuse_overhead;
	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return;
}

uint
hnd_hwm(void)
{
	return (inuse_hwm);
}

#ifndef BCM_BOOTLOADER
#if defined(RTE_CONS)

/*
* Function to get heap memory usage
* for both dhd cons hu and wl memuse commands
*/

int
hnd_get_heapuse(memuse_info_t *mu)
{
	hnd_image_info_t info;
	size_t rodata_len;

	if (mu == NULL)
		return -1;

	hnd_image_info(&info);

	mu->ver = RTE_MEMUSEINFO_VER;
	mu->len = sizeof(memuse_info_t);

	mu->text_len = (info._text_end - info._text_start);
	rodata_len = (info._rodata_end - info._rodata_start);
	mu->data_len = (info._data_end - info._data_start);
	mu->bss_len = (info._bss_end - info._bss_start);

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return -1;

	mu->arena_size = arena_size;
	mu->inuse_size = inuse_size;
	mu->inuse_hwm = inuse_hwm;
	mu->inuse_overhead = inuse_overhead;

	mu->text_len += rodata_len;
	mu->tot = mu->text_len + mu->data_len + mu->bss_len;

	mu->tot += mu->inuse_hwm;

	mu->inuse_total = mu->inuse_size + mu->inuse_overhead;
	mu->arena_free = mu->arena_size - mu->inuse_total;

	mu->free_lwm = arena_lwm;
	mu->mf_count = mf_count;

#ifdef MEM_ALLOC_STATS
	mu->max_flowring_alloc = max_flowring_alloc;
	mu->max_bsscfg_alloc = max_bsscfg_alloc;
	mu->max_scb_alloc = max_scb_alloc;
#endif /* MEM_ALLOC_STATS */

	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return -1;
	return 0;
}

/*
* Function to print heap memory usage for dhd cons hu command
*/

static void
hnd_print_heapuse(void *arg, int argc, char *argv[])
{
	memuse_info_t mu;
	int ret;

	if ((ret = hnd_get_heapuse(&mu)) != 0)
		return;

	printf("Memory usage:\n");
	printf("\tText: %d(%dK), Data: %d(%dK), Bss: %d(%dK)\n",
	       mu.text_len, KB(mu.text_len),
	       mu.data_len, KB(mu.data_len),
	       mu.bss_len, KB(mu.bss_len));
	printf("\tArena total: %d(%dK), Free: %d(%dK), In use: %d(%dK), HWM: %d(%dK)\n",
	       mu.arena_size, KB(mu.arena_size),
	       mu.arena_free, KB(mu.arena_free),
	       mu.inuse_size, KB(mu.inuse_size),
	       mu.inuse_hwm, KB(mu.inuse_hwm));
	printf("\tIn use + overhead: %d(%dK), Max memory in use: %d(%dK)\n",
	       mu.inuse_total, KB(mu.inuse_total),
	       mu.tot, KB(mu.tot));
	printf("\tMalloc failure count: %d\n", mf_count);
	printf("\tMin heap value is %d\n", arena_lwm);
#ifdef MEM_ALLOC_STATS
	printf("\tMax memory allocated per flowring is %d\n", mu.max_flowring_alloc);
	printf("\tMax memory allocated per bsscfg is %d\n", mu.max_bsscfg_alloc);
	printf("\tMax memory allocated per scb is %d\n", mu.max_scb_alloc);
#endif /* MEM_ALLOC_STATS */
}
#endif /* RTE_CONS */
#endif /* BCM_BOOTLOADER */

#if defined(BCMDBG_MEM) || defined(BCMDBG_HEAPCHECK)
int
hnd_memcheck(const char *file, int line)
{
	mem_t *this = NULL;
	uint i;
	uint32 *countptr;
	arena_segment_t *segments;
	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return 1;

	countptr = get_arena_segment_count();
	segments = get_arena_segments();
	for (i = 0; i < *countptr; i++)
	{
		bool done = FALSE;
		uint size_expected = segments[i].size;
		uint max_segment_address = (uint32)segments[i].base + segments[i].size;
		uint size_calculated = 0;
		int last_block_type = -1;
		this = segments[i].base;
		while (!done)
		{
#ifdef BCMDBG_MEM
			if (this->magic != MEM_MAGIC || this->size < 0) {
				printf("CORRUPTION: %s %d\n", file, line);
				printf("\n%s: Corrupt magic (0x%x); size %d; allocfn 0x%p allocseq"
					" 0x%x\n\n", __FUNCTION__, this->magic,
					GET_MEM_T_SIZE(this),
					this->malloc_function, this->u.malloc_sequence);
				HND_DIE();
			}
#endif /* BCMDBG_MEM */
			if ((uint32)this > max_segment_address)
			{
				printf("%s:%d CORRUPTION: ", file, line);
				printf("this pointer exceeds segment max address\n");
				printf("this %p, max 0x%06x, seg_base %p, seg_size %d\n",
				       this, max_segment_address, segments[i].base,
				       segments[i].size);
				HND_DIE();
			}
			if ((uint32)this + GET_MEM_T_SIZE(this) > max_segment_address)
			{
				printf("%s:%d CORRUPTION: ", file, line);
				printf("block extends beyond segment max address\n");
				printf("this %p, size %d max 0x%06x, seg_base %p, seg_size %d\n",
				       this, GET_MEM_T_SIZE(this), max_segment_address,
				       segments[i].base, segments[i].size);
				HND_DIE();
			}
			if (GET_MEM_T_SIGNATURE(this) == MEM_T_FREE_BLOCK_SMALL)
			{
				printf("%s:%d CORRUPTION: ", file, line);
				printf("invalid signature field in mem_t structure\n");
				printf("this %p, size %d max 0x%06x, seg_base %p, seg_size %d\n",
				       this, GET_MEM_T_SIZE(this), max_segment_address,
				       segments[i].base, segments[i].size);
				HND_DIE();
			}
			if (GET_MEM_T_SIGNATURE(this) == MEM_T_FREE_BLOCK)
			{
				mem_t *free_ptr = hnd_freemem_get();
				bool found = FALSE;
				/* Check that free block is in free list */
				while (free_ptr)
				{
					if (this == free_ptr)
					{
						found = TRUE;
						break;
					}
#ifdef BCMDBG_MEM
					if (this->magic != MEM_MAGIC || this->size < 0) {
						printf("CORRUPTION: %s %d\n", file, line);
						printf("\n%s: Corrupt magic (0x%x); size "
						"%d; allocfn 0x%p, freefn 0x%p\n\n",
							__FUNCTION__, this->magic,
							GET_MEM_T_SIZE(this), this->malloc_function,
							this->u.free_function);
						HND_DIE();
					}
#endif /* BCMDBG_MEM */
					free_ptr = GET_MEM_T_NEXT(free_ptr);
				}
				if (!found)
				{
					printf("%s:%d CORRUPTION: ", file, line);
					printf("free block is not in free list\n");
					printf("this %p, seg_base %p, seg_size %d\n",
					       this, segments[i].base,
					       segments[i].size);
					HND_DIE();
				}
			}
			size_calculated += GET_MEM_T_HDR_SIZE(this);
			size_calculated += GET_MEM_T_SIZE(this);
			if (last_block_type == MEM_T_FREE_BLOCK &&
			    GET_MEM_T_SIGNATURE(this) == MEM_T_FREE_BLOCK)
			{
				printf("%s:%d CORRUPTION: ", file, line);
				printf("free block follows free block\n");
				printf("this %p, seg_base %p, seg_size %d\n",
				       this, segments[i].base,
				       segments[i].size);
				HND_DIE();
			}
			last_block_type = GET_MEM_T_SIGNATURE(this);
			this = (mem_t*)((uint32)this + GET_MEM_T_SIZE(this) +
			GET_MEM_T_HDR_SIZE(this));
			if ((uint32)this >= max_segment_address)
			{
				done = TRUE;
			}
		}
		if (size_calculated != size_expected)
		{
			printf("%s:%d CORRUPTION: ", file, line);
			printf("calculated segment size does not equal segment size\n");
			printf("seg_base %p, seg_size %d, calulated_size %d\n",
			       segments[i].base, segments[i].size, size_calculated);
			HND_DIE();
		}
	}
	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return 1;
	return 0;
}
#endif /* BCMDBG_MEM || BCMDBG_HEAPCHECK */

#if defined(RTE_CONS) && defined(BCMDBG_MEM) && !defined(BCM_BOOTLOADER)
static void
hnd_print_malloc(void *arg, int argc, char *argv[])
{
	uint32 inuse = 0, free = 0, total;
	mem_t *this;

	printf("Heap inuse list:\n");
	printf("Addr\t\tSize\tfile:line\t\tmalloc fn\n");

	if (HND_HEAP_MUTEX_ACQUIRE(&heap_mutex, OSL_EXT_TIME_FOREVER) != OSL_EXT_SUCCESS)
		return;

	this = inuse_mem.next;

	while (this) {
		printf("%p\t%5d\t%p\t0x%x\n",
		       &this[1], GET_MEM_T_SIZE(this),
		       this->malloc_function, this->u.malloc_sequence);
		inuse += GET_MEM_T_SIZE(this) + sizeof(mem_t);
		this = this->next;
	}
	printf("Heap free list:\n");
	this = hnd_freemem_get()->next;
	while (this) {
		printf("%p: 0x%x(%d) @ %p\n",
		       this, GET_MEM_T_SIZE(this), GET_MEM_T_SIZE(this), &this[1]);
		free += GET_MEM_T_SIZE(this) + sizeof(mem_t);
		this = this->next;
	}
	total = inuse + free;
	printf("Heap Dyn inuse: 0x%x(%d), inuse: 0x%x(%d)\nDyn free: 0x%x(%d), free: 0x%x(%d)\n",
	       inuse, inuse, inuse_size, inuse_size, free, free,
	       (arena_size - inuse_size), (arena_size - inuse_size));
	if (total != arena_size)
		printf("Total (%d) does NOT agree with original %d!\n",
		       total, arena_size);

	if (HND_HEAP_MUTEX_RELEASE(&heap_mutex) != OSL_EXT_SUCCESS)
		return;
}
#endif /* defined(BCMDBG_MEM) && RTE_CONS && !BCM_BOOTLOADER */

/* Must be called after hnd_cons_init() which depends on heap to be available */
void
BCMATTACHFN(hnd_heap_cli_init)(void)
{
#ifndef BCM_BOOTLOADER
#if defined(RTE_CONS)
	if (!hnd_cons_add_cmd("hu", hnd_print_heapuse, 0))
		return;
#endif // endif
#if defined(RTE_CONS) && defined(BCMDBG_MEM)
	if (!hnd_cons_add_cmd("ar", hnd_print_malloc, 0))
		return;
#endif // endif
#endif /* BCM_BOOTLOADER */
}

#if defined(SRMEM)
void
hnd_update_sr_size(void)
{
	/* The save-restore memory feature re-uses the save-restore memory block for pktpool
	 * buffers when not in deep-sleep. Normally, the pktpool buffers are allocated from the
	 * heap. Update the heap memory stats to pretend that the save-restore memory block was
	 * added as a heap arena and that the pktpool buffers are allocated from this arena.
	 * This is necessary to provide accurate memory stats in hndrte_print_memuse().
	 */
	arena_size += SR_MEMSIZE;
	inuse_size += SR_MEMSIZE;
	inuse_hwm  += SR_MEMSIZE;
}
#endif /* SRMEM */

#ifdef AWD_EXT_TRAP
uint8 *awd_handle_trap_heap(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen)
{
	uint8 *new_dst = dst;
	hnd_ext_trap_heap_err_t heap_err = { 0 };
	mem_t *free_memptr;
	mem_t *last;
	uint size, i, j;

	if (awd_get_trap_ext_swflags() & AWD_EXT_TRAP_SW_FLAG_MEM) {

		heap_err.arena_total = arena_size;
		heap_err.heap_inuse = inuse_size;
		heap_err.heap_inuse_plus_ohd = inuse_size + inuse_overhead;
		heap_err.heap_free = arena_size - heap_err.heap_inuse_plus_ohd;
		heap_err.mf_count = mf_count;
		heap_err.stack_lwm = 0;

		last = hnd_freemem_get();

		/* For the moment, assume we grab just the two biggest */
		while ((free_memptr = GET_MEM_T_NEXT(last)) != NULL) {
			size = GET_MEM_T_SIZE(free_memptr);
			for (i = 0; i < HEAP_MAX_SZ_BLKS_LEN; i++) {
				if (size > (heap_err.max_sz_free_blk[i] << 2)) {
					for (j = HEAP_MAX_SZ_BLKS_LEN - 1; j > i; j--) {
						heap_err.max_sz_free_blk[j] =
							heap_err.max_sz_free_blk[j - 1];
					}
					heap_err.max_sz_free_blk[i] = size >> 2;
					break;
				}
			}
			last = free_memptr;
		}

// XXX		heap_err->heap_histogm[HEAP_HISTOGRAM_DUMP_LEN * 2] = 0;

		new_dst = bcm_write_tlv_safe(TAG_TRAP_MEMORY, &heap_err, sizeof(heap_err), dst,
			*dst_maxlen);

		if (new_dst != dst)
			*dst_maxlen -= (sizeof(heap_err) + BCM_TLV_HDR_SIZE);
	}
	return new_dst;
}
#endif /* AWD_EXT_TRAP */

#ifdef MEM_ALLOC_STATS
void
hnd_update_mem_alloc_stats(memuse_info_t *mu)
{
	max_flowring_alloc = mu->max_flowring_alloc;
	max_bsscfg_alloc = mu->max_bsscfg_alloc;
	max_scb_alloc  = mu->max_scb_alloc;
}
#endif /* MEM_ALLOC_STATS */
