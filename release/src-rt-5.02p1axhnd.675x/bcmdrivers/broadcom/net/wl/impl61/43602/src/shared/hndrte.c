/** @file hndrte.c
 *
 * Initialization and support routines for self-booting compressed image.
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
 * $Id: hndrte.c 708088 2017-06-29 21:37:42Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <hndcpu.h>
#ifdef SBPCI
#include <pci_core.h>
#include <hndpci.h>
#include <pcicfg.h>
#endif // endif
#include <hndchipc.h>
#include <hndrte.h>
#include <hndrte_trap.h>
#include <hndrte_lbuf.h>
#include <bcmsdpcm.h>
#include <hndrte_debug.h>
#include <bcm_buzzz.h> /* BCM_BUZZZ */
#include <epivers.h>

#ifdef BCM_VUSBD
#include <vusbd_stat.h>
#endif // endif

#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
#include <trxhdr.h>
#include <usbrdl.h>
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */
#ifdef BCM_OL_DEV
#include <bcm_ol_msg.h>
#endif // endif
#ifdef BCMM2MDEV
#include <m2mdma_core.h>
#else
#include <bcmpcie.h>
#endif // endif
#ifdef ATE_BUILD
#include "wl_ate.h"
#endif // endif
#include <event_log.h>

/* debug */
#ifdef BCMDBG
#define RTE_MSG(x) printf x
#else
#define RTE_MSG(x)
#endif // endif

#ifdef EVENT_LOG_COMPILE
#define EVENT_LOG_IF_READY(_tag, _format, _count, _size, _address, _ra, _success) \
	do {                                \
		if (event_log_is_ready()) {             \
			EVENT_LOG(_tag, _format, _count, _size, _address, _ra, _success); \
		}                           \
	}                               \
	while (0)
static uint malloc_event_count = 0;
#else /* EVENT_LOG_COMPILE */
#define EVENT_LOG_IF_READY(_tag, _format, _count, _size, _address, _ra, _success)
#endif /* EVENT_LOG_COMPILE */

#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
extern char *_vars;
extern uint _varsz;
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */
si_t *hndrte_sih = NULL;		/* Backplane handle */
osl_t *hndrte_osh = NULL;		/* Backplane osl handle */
chipcregs_t *hndrte_ccr = NULL;		/* Chipc core regs */
pmuregs_t *hndrte_pmur = NULL;		/* PMU core regs */
sbconfig_t *hndrte_ccsbr = NULL;	/* Chipc core SB config regs */

#if defined(BCMPKTIDMAP)
/* Hierarchical multiword bit map for unique 16bit id allocator */
struct bcm_mwbmap * hndrte_pktid_map = (struct bcm_mwbmap *)NULL;
uint32 hndrte_pktid_max = 0U; /* mwbmap allocator dimension */
uint32 hndrte_pktid_failure_count = 0U; /* pktid alloc failure count */
/* Associative array of pktid to pktptr */
struct lbuf **hndrte_pktptr_map = NULL;
#endif /*   BCMPKTIDMAP */

#if defined(BCMPKTPOOL) || defined(BCMROMOFFLOAD)
pktpool_t *pktpool_shared = NULL;
#endif /* BCMPKTPOOL || BCMROMOFFLOAD */

#ifdef BCMFRAGPOOL
pktpool_t *pktpool_shared_lfrag = NULL;
#endif /* BCMFRAGPOOL */

#if defined(BCMSPLITRX) && !defined(BCMSPLITRX_DISABLED)
bool _bcmsplitrx = TRUE;
#else
bool _bcmsplitrx = FALSE;
#endif // endif

pktpool_t *pktpool_shared_rxlfrag = NULL;
hndrte_txrxstatus_glom_info hndrte_glom_info;

/* HostFetch module related info */
#ifdef BCMPCIEDEV_ENABLED
struct fetch_rqst_q_t *fetch_rqst_q = NULL;
struct fetch_module_info *fetch_info = NULL;
struct pktfetch_rqstpool *pktfetchpool = NULL;
struct pktfetch_q *pktfetchq = NULL;
struct pktq *pktfetch_lbuf_q = NULL;

#ifndef PKTFETCH_MAX_OUTSTANDING
#define PKTFETCH_MAX_OUTSTANDING	(128)
#endif // endif
#ifndef PKTFETCH_MIN_MEM_FOR_HEAPALLOC
#define PKTFETCH_MIN_MEM_FOR_HEAPALLOC	(16*1024)
#endif // endif

#ifdef BCMDBG
pktfetch_debug_stats_t pktfetch_stats;
#if defined(HNDRTE_CONSOLE)
static hndrte_timer_t *pktfetch_stats_timer;
#endif // endif
#define PKTFETCH_STATS_DELAY 4000
#endif /* BCMDBG */

#ifdef DBG_BUS
struct {
	uint32 hndrte_fetch_rqst;
	uint32 hndrte_dispatch_fetch_rqst;
	uint32 hndrte_flush_fetch_rqstq;
	uint32 hndrte_remove_fetch_rqst_from_queue;
	uint32 hndrte_cancel_fetch_rqst;
	uint32 hndrte_pktfetch;
	uint32 hndrte_pktfetch_dispatch;
	uint32 hndrte_pktfetch_completion_handler;
} rte_dbg_bus;
#endif /* DBG_BUS */

#endif /* BCMPCIEDEV_ENABLED */

#ifdef BCM_OL_DEV
pktpool_t *pktpool_shared_msgs = NULL;
olmsg_shared_info_t *ppcie_shared;
uint32 tramsz;
uint32 olmsg_shared_info_sz = OLMSG_SHARED_INFO_SZ;
#endif // endif

/* Global ASSERT type */
uint32 g_assert_type = 0;

/* extern function */

/* misc local function */
static void hndrte_chipc_init(si_t *sih);
#ifdef WLGPIOHLR
static void hndrte_gpio_init(si_t *sih);
#endif // endif
static void hndrte_gci_init(si_t *sih);
#ifdef BCMECICOEX
static void hndrte_eci_init(si_t *sih);
#endif /* BCMECICOEX */
#if defined(HNDRTE_CONSOLE) && !defined(BCM_BOOTLOADER)
static void hndrte_memdmp(void *arg, int argc, char **argv);
#endif /* HNDRTE_CONSOLE && !BCM_BOOTLOADER */

#ifdef	__ARM_ARCH_7R__
extern uint32 _rambottom;
extern uint32 _atcmrambase;
#endif // endif

hndrte_debug_t hndrte_debug_info;
static void hndrte_debug_info_init(void);

#ifdef BCMDBG_CPU
static void hndrte_print_cpuuse(uint32 arg);
static void hndrte_updatetimer(hndrte_timer_t * t);
void hndrte_clear_stats(void);
#endif // endif

#ifdef BCMPCIEDEV_ENABLED
#if defined(BCMDBG) && defined(HNDRTE_CONSOLE)
static void hndrte_update_pktfetch_stats(hndrte_timer_t *t);
static void hndrte_dump_pktfetch_stats(uint32 arg);
#endif /* BDMDBG && HNDRTE_CONSOLE */
static void hndrte_enque_fetch_rqst(struct fetch_rqst *fr, bool enque_to_head);
static int hndrte_dispatch_fetch_rqst(struct fetch_rqst *fr);
static struct fetch_rqst *hndrte_deque_fetch_rqst(void);
static void hndrte_fetch_module_init(void);
static struct fetch_rqst *hndrte_remove_fetch_rqst_from_queue(struct fetch_rqst *fr);

/* PktFetch */
static void hndrte_pktfetch_completion_handler(struct fetch_rqst *fr, bool cancelled);
static struct pktfetch_info* hndrte_deque_pktfetch(void);
static void hndrte_enque_pktfetch(struct pktfetch_info *pinfo, bool enque_to_head);
static void hndrte_pktfetchpool_reclaim(struct fetch_rqst *fr);
static struct fetch_rqst* hndrte_pktfetchpool_get(void);
static int hndrte_pktfetch_dispatch(struct pktfetch_info *pinfo);
#endif /* BCMPCIEDEV_ENABLED */

#ifdef BCMDBG_CPU
static hndrte_timer_t *cu_timer;	/* timer for cpu usage calculation */
#define DEFUPDATEDELAY 5000
#endif // endif

#if defined(DONGLEBUILD) || defined(HNDRTE_TEST)
/*
 * Define reclaimable NVRAM area; startarm.S will copy downloaded NVRAM data to
 * this array and move the _vars pointer to point to this array. The originally
 * downloaded memory area will be released (reused).
 */
#if defined(WLTEST) || defined(BCMDBG_DUMP) || !defined(WLC_HIGH)
uint8 nvram_array[NVRAM_ARRAY_MAXSIZE] DECLSPEC_ALIGN(4) = {0};
#else
uint8 BCMATTACHDATA(nvram_array)[NVRAM_ARRAY_MAXSIZE] DECLSPEC_ALIGN(4) = {0};
#endif // endif
#endif /* DONGLEBUILD || HNDRTE_TEST */

/*
 * ======HNDRTE====== Memory allocation:
 *	hndrte_free(w): Free previously allocated memory at w
 *	hndrte_malloc_align(size, abits): Allocate memory at a 2^abits boundary
 *	hndrte_memavail(): Return a (slightly optimistic) estimate of free memory
 *	hndrte_hwm(): Return a high watermark of allocated memory
 *	hndrte_print_memuse(): Dump memory usage stats.
 *	hndrte_print_memwaste(): Malloc memory to simulate low memory environment
 *	hndrte_print_cpuuse(): Dump cpu idle time stats.
 *	hndrte_print_malloc(): (BCMDBG_MEM) Dump free & inuse lists
 *	hndrte_arena_add(base, size): Add a block of memory to the arena
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

#ifdef BCMDBG_MEM

typedef struct _mem_dbg {
	uint32		magic;
	uchar		*malloc_function;
	uchar		*free_function;
	const char	*file;
	int		line;
	uint32		size;
	struct _mem_dbg	*next;
} mem_t;

static mem_t	free_mem_dbg;		/* Free list head */

/* Note: the dummy_mem structure and pointer is purely to satisfy the rom symbol size check.
 * free_mem is not used.
 */
typedef struct _dummy_mem {
	uint32		size;
	struct _dummy_mem	*next;
} dummy_mem_t;

static dummy_mem_t	free_mem;	/* Free list head */
dummy_mem_t *dummy_mem_p = &free_mem;

#else /* ! BCMDBG_MEM */

typedef struct _mem {
	uint32		size;
	struct _mem	*next;
} mem_t;

static mem_t	free_mem;		/* Free list head */

static mem_t* hndrte_freemem_get(void);
#endif /* BCMDBG_MEM */

static hndrte_ltrsend_cb_info_t* hndrte_gen_ltr_get(void);
static hndrte_devpwrstchg_cb_info_t* hndrte_devpwrstschg_get(void);

static uint	arena_size;		/* Total heap size */
static uint	inuse_size;		/* Current in use */
static uint	inuse_overhead;		/* tally of allocated mem_t blocks */
static uint	inuse_hwm;		/* High watermark of memory - reclaimed memory */
static uint	mf_count;		/* Malloc failure count */

#ifdef BCMDBG_CPU
hndrte_cpu_stats_t cpu_stats;
static uint32 enterwaittime = 0xFFFFFFFF;
#endif // endif

#define	STACK_MAGIC	0x5354414b	/* Magic # for stack protection: 'STAK' */

static	uint32	*bos = NULL;		/* Bottom of the stack */
static	uint32	*tos = NULL;		/* Top of the stack */

#ifdef BCMDBG_MEM
#define	MEM_MAGIC	0x4d4e4743	/* Magic # for mem overwrite check: 'MNGC' */

static mem_t	inuse_mem;		/* In-use list head */

#else /* BCMDBG_MEM */

static mem_t*
BCMRAMFN(hndrte_freemem_get)(void)
{
	return (&free_mem);
}
#endif /* BCMDBG_MEM */

void
BCMATTACHFN(hndrte_arena_init)(uintptr base, uintptr lim, uintptr stackbottom)
{
	mem_t *first;
#ifndef BCMDBG_MEM
	mem_t *free_memptr;
#endif // endif

	ASSERT(base);
	ASSERT(lim > base);

	/*
	 * Mark the stack with STACK_MAGIC here before using any other
	 * automatic variables! The positions of these variables are
	 * compiler + target + optimization dependant, and they can be
	 * overwritten by the code below if they are allocated before
	 * 'p' towards the stack bottom.
	 */
	{
		uint32 *p = (uint32 *)stackbottom;
		uint32 cbos;

		*p = STACK_MAGIC;

		/* Mark the stack */
		while (++p < &cbos - 32)
			*p = STACK_MAGIC;
	}

	/* Align */
	first = (mem_t *)ALIGN(base, MIN_ALIGN);

	arena_size = lim - (uint32)first;
	inuse_size = 0;
	inuse_overhead = 0;
	inuse_hwm = 0;

	/* Mark the bottom of the stack */
	bos = (uint32 *)stackbottom;
	tos = (uint32 *)((uintptr)bos + HNDRTE_STACK_SIZE);
	mf_count = 0;
#ifdef BCMDBG_MEM
	free_mem_dbg.magic = inuse_mem.magic = first->magic = MEM_MAGIC;
	inuse_mem.next = NULL;
#endif /* BCMDBG_MEM */
	first->size = arena_size - sizeof(mem_t);
	first->next = NULL;
#ifdef BCMDBG_MEM
	free_mem_dbg.next = first;
#else
	free_memptr = hndrte_freemem_get();
	free_memptr->next = first;
#endif /* BCMDBG_MEM */
}

uint
hndrte_arena_add(uint32 base, uint size)
{
	uint32 addr;
	mem_t *this;

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
	this = (mem_t *)addr;
	arena_size += size;
	size -= sizeof(mem_t);
	addr += sizeof(mem_t);
	this->size = size;

	/* This chunk was not in use before, make believe it was */
	inuse_size += size;
	inuse_overhead += sizeof(mem_t);

#ifdef BCMDBG_MEM
	this->magic = MEM_MAGIC;
	this->file = NULL;
	this->line = 0;
	this->next = inuse_mem.next;
	inuse_mem.next = this;
	printf("%s: Adding %p: 0x%x(%d) @ 0x%x\n", __FUNCTION__, this, size, size, addr);
#else
	this->next = NULL;
#endif /* BCMDBG_MEM */

	hndrte_free((void *)addr);
	return (size);
}

void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hndrte_malloc_align(uint size, uint alignbits, const char *file, int line)
#else
hndrte_malloc_align(uint size, uint alignbits)
#endif /* BCMDBG_MEM */
{
	mem_t	*curr, *last, *this = NULL, *prev = NULL;
	uint	align, rem, waste;
	uintptr	addr = 0, top = 0;
	uint	inuse_total;
#ifdef BCMDBG_MEM
	const char *basename;
#endif /* BCMDBG_MEM */

	ASSERT(size);

	size = ALIGN(size, MIN_ALIGN);

	align = 1 << alignbits;
	if (align <= MIN_ALIGN)
		align = MIN_ALIGN;
	else if (align > MAX_ALIGN)
		align = MAX_ALIGN;

	/* Search for available memory */
#ifdef BCMDBG_MEM
	last = &free_mem_dbg;
#else
	last = hndrte_freemem_get();
#endif /* BCMDBG_MEM */
	waste = arena_size;

	/* Algorithm: best fit */
	while ((curr = last->next) != NULL) {
		if (curr->size >= size) {
			/* Calculate alignment */
			uintptr lowest = (uintptr)curr + sizeof(mem_t);
			uintptr end = lowest + curr->size;
			uintptr highest = end - size;
			uintptr a = ALIGN_DOWN(highest, align);

			/* Find closest sized buffer to avoid fragmentation BUT aligned address
			 * must be greater or equal to the lowest address available in the free
			 * block AND space preceeding a returned block's header is either big
			 * enough to support another free block OR
		         * zero in which case there is no need to create a preceeding free block
			 */
			if (a >= lowest &&
			    ((a-lowest) >= sizeof(mem_t) || (a-lowest) == 0) &&
			    (curr->size - size) < waste)
			{

				waste = curr->size - size;
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
		printf("No memory to satisfy request for %d bytes, inuse %d, file %s, line %d\n",
		       size, (inuse_size + inuse_overhead), file ? file : "unknown", line);
#ifdef BCMDBG_MEM
		hndrte_print_malloc();
#endif // endif
#else
		RTE_MSG(("No memory to satisfy request %d bytes, inuse %d\n", size,
		         (inuse_size + inuse_overhead)));
#endif /* BCMDBG_MEM */
		EVENT_LOG_IF_READY(EVENT_LOG_TAG_MEM_ALLOC_FAIL,
		        "0x%06x ALLC(FAIL) - sz: 0x%08x, ad: 0x%08x, ra: 0x%08x, al: 0x%08x\n",
		        malloc_event_count, 0, (uint32) -1,
		        (uint32)__builtin_return_address(0), alignbits);
		return NULL;
	}

#ifdef BCMDBG_MEM
	ASSERT(this->magic == MEM_MAGIC);
#endif // endif

	if (*bos != STACK_MAGIC)
		RTE_MSG(("Stack bottom has been overwritten\n"));
	ASSERT(*bos == STACK_MAGIC);

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
	if (rem <= (MIN_MEM_SIZE + sizeof(mem_t))) {
		/* take it all */
		size += rem;
	} else {
		/* Split off the top */
		mem_t *new = (mem_t *)(addr + size);

		this->size -= rem;
		new->size = rem - sizeof(mem_t);
#ifdef BCMDBG_MEM
		new->magic = MEM_MAGIC;
#endif /* BCMDBG_MEM */
		new->next = this->next;
		this->next = new;
	}

	/* Anything below? */
	rem = this->size - size;
	if (rem == 0) {
		/* take it all */
		prev->next = this->next;
	} else {
		/* Split this */
		mem_t *new = (mem_t *)((uint32)this + rem);

		if (rem < sizeof(mem_t)) {
			/* Should NOT happen */
			mf_count++;
			RTE_MSG(("Internal malloc error sz:%d, al:%d, tsz:%d, rem:%5d,"
			       " add:0x%x, top:0x%x\n",
			       size, align, this->size, rem, addr, top));
			return NULL;
		}

		new->size = size;
		this->size = rem - sizeof(mem_t);
#ifdef BCMDBG_MEM
		new->magic = MEM_MAGIC;
#endif /* BCMDBG_MEM */

		this = new;
	}

#ifdef BCMDBG_MEM
	this->next = inuse_mem.next;
	inuse_mem.next = this;
	this->line = line;
	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;
	if (!basename)
		basename = file;
	this->file = basename;
	this->malloc_function = __builtin_return_address(0);
#else
	this->next = NULL;
#endif /* BCMDBG_MEM */
	inuse_size += this->size;
	inuse_overhead += sizeof(mem_t);

	/* find the instance where the free memory was the least to calculate
	 * inuse memory hwm
	 */
	inuse_total = inuse_size + inuse_overhead;
	if (inuse_total > inuse_hwm)
		inuse_hwm = inuse_total;

	EVENT_LOG_IF_READY(EVENT_LOG_TAG_MEM_ALLOC_SUCC,
	                   "0x%06x ALLC(SUCC) - sz: 0x%08x, ad: 0x%08x, ra: 0x%08x, al: 0x%08x\n",
	                   malloc_event_count, size, (uint32) ((uint32)this + sizeof(mem_t)),
	                   (uint32)__builtin_return_address(0), alignbits);

#ifdef BCMDBG_MEM
	RTE_MSG(("malloc: 0x%x\n", (uint32) ((void *)((uintptr)this + sizeof(mem_t)))));
#endif // endif

	return ((void *)((uint32)this + sizeof(mem_t)));
}

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
 * (i.e., after hndrte_arena_init() or hndrte_arena_add())
*/
static void
hndrte_lbuf_fixup_2M_tcm(void)
{
	void *dummy_ptr;

	/* Reserving 4 bytes memory at all 2M boundary to create a hole */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	while ((dummy_ptr = hndrte_malloc_align(4, 21, __FILE__, __LINE__)) != NULL);
#else
	while ((dummy_ptr = hndrte_malloc_align(4, 21)) != NULL);
#endif // endif

	mf_count--;	/* decrement malloc failure count to avoid confusion,
			   because this was an intended/expected failure by the logic
			*/
}
#endif /* HNDLBUFCOMPACT */

static int
hndrte_malloc_size(void *where)
{
	uint32 w = (uint32)where;
	mem_t *this;

#ifdef BCMDBG_MEM
	mem_t *prev;

	/* Get it off of the inuse list */
	prev = &inuse_mem;
	while ((this = prev->next) != NULL) {
		if (((uint32)this + sizeof(mem_t)) == w)
			break;
		prev = this;
	}

	if (this == NULL) {
		RTE_MSG(("%s: 0x%x is not in the inuse list\n", __FUNCTION__, w));
		ASSERT(this);
		return -1;
	}

	if (this->magic != MEM_MAGIC) {
		RTE_MSG(("\n%s: Corrupt magic (0x%x) in 0x%x; size %d; file %s, line %d\n\n",
		       __FUNCTION__, this->magic, w, this->size, this->file, this->line));
		ASSERT(this->magic == MEM_MAGIC);
		return -1;
	}

#else
	this = (mem_t *)(w - sizeof(mem_t));
#endif /* BCMDBG_MEM */

	return this->size;
}

void *
hndrte_realloc(void *ptr, uint size)
{
	int osz = hndrte_malloc_size(ptr);

	if (osz < 0)
		return NULL;

	void *new = hndrte_malloc(size);
	if (new == NULL)
		return NULL;
	memcpy(new, ptr, MIN(size, osz));
	hndrte_free(ptr);
	return new;
}

int
hndrte_free(void *where)
{
	uint32 w = (uint32)where;
	mem_t *prev, *next, *this;

#ifdef BCMDBG_MEM
	/* Get it off of the inuse list */
	prev = &inuse_mem;
	while ((this = prev->next) != NULL) {
		if (((uint32)this + sizeof(mem_t)) == w)
			break;
		prev = this;
	}

	if (this == NULL) {
		RTE_MSG(("%s: 0x%x is not in the inuse list\n", __FUNCTION__, w));
		ASSERT(this);
		return -1;
	}

	if (this->magic != MEM_MAGIC) {
		RTE_MSG(("\n%s: Corrupt magic (0x%x) in 0x%x; size %d; file %s, line %d\n\n",
		       __FUNCTION__, this->magic, w, this->size, this->file, this->line));
		ASSERT(this->magic == MEM_MAGIC);
		return -1;
	}

	this->free_function = __builtin_return_address(0);
	prev->next = this->next;
#else
	this = (mem_t *)(w - sizeof(mem_t));
#endif /* BCMDBG_MEM */

	inuse_size -= this->size;
	inuse_overhead -= sizeof(mem_t);

	/* Find the right place in the free list for it */
#ifdef BCMDBG_MEM
	prev = &free_mem_dbg;
#else
	prev = hndrte_freemem_get();
#endif /* BCMDBG_MEM */
	while ((next = prev->next) != NULL) {
		if (next >= this)
			break;
		prev = next;
	}

	/* Coalesce with next if appropriate */
	if ((w + this->size) == (uint32)next) {
		this->size += next->size + sizeof(mem_t);
		this->next = next->next;
#ifdef BCMDBG_MEM
		next->magic = 0;
#endif /* BCMDBG_MEM */
	} else
		this->next = next;

	/* Coalesce with prev if appropriate */
	if (((uint32)prev + sizeof(mem_t) + prev->size) == (uint32)this) {
		prev->size += this->size + sizeof(mem_t);
		prev->next = this->next;
#ifdef BCMDBG_MEM
		this->magic = 0;
#endif /* BCMDBG_MEM */
	} else
		prev->next = this;

	return 0;
}

void *
hndrte_calloc(uint num, uint size)
{
	void *ptr;

	ptr = hndrte_malloc(size*num);
	if (ptr)
		bzero(ptr, size*num);

	return (ptr);
}

uint
hndrte_memavail(void)
{
	return (arena_size - inuse_size - inuse_overhead);
}

uint
hndrte_hwm(void)
{
	return (inuse_hwm);
}

#if defined(BCMRECLAIM) && defined(CONFIG_XIP)
#error "Both XIP and RECLAIM defined"
#endif // endif

#ifdef DONGLEBUILD
bool bcmreclaimed = FALSE;

#ifdef BCMRECLAIM
bool init_part_reclaimed = FALSE;
#endif // endif
bool attach_part_reclaimed = FALSE;
bool preattach_part_reclaimed = FALSE;

void
hndrte_reclaim(void)
{
	uint reclaim_size = 0;
	hndrte_image_info_t info;
	const char *r_fmt_str = "reclaim section %s: Returned %d bytes to the heap\n";

	hndrte_image_info(&info);

#ifdef BCMRECLAIM
	if (!bcmreclaimed && !init_part_reclaimed) {
		reclaim_size = (uint)(info._reclaim1_end - info._reclaim1_start);
		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim1_start, reclaim_size);
			hndrte_arena_add((uint32)info._reclaim1_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "2", reclaim_size);
		bcmreclaimed = TRUE;
		init_part_reclaimed = TRUE;
		goto exit;
	}
#endif /* BCMRECLAIM */

	if (!attach_part_reclaimed) {
		reclaim_size = (uint)(info._reclaim2_end - info._reclaim2_start);

		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim2_start, reclaim_size);
			hndrte_arena_add((uint32)info._reclaim2_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "1", reclaim_size);

		/* Reclaim space reserved for TCAM bootloader patching. Once the bootloader hands
		 * execution off to the firmware, the bootloader patch table is no longer required
		 * and can be reclaimed.
		 */
#if defined(BOOTLOADER_PATCH_RECLAIM)
		reclaim_size = (uint)(info._boot_patch_end - info._boot_patch_start);
		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._boot_patch_start, reclaim_size);
			hndrte_arena_add((uint32)info._boot_patch_start, reclaim_size);
			printf(r_fmt_str, "boot-patch", reclaim_size);
		}
		else {
			/* For non-USB builds, the explicit bootloader patch section may not exist.
			 * However, there may still be a block of unused memory that can be
			 * reclaimed since parts of the memory map are fixed in order to provide
			 * compatibility with builds that require the bootloader patch section.
			 */
			reclaim_size = (uint)(info._reclaim4_end - info._reclaim4_start);
			if (reclaim_size) {
				/* blow away the reclaim region */
				bzero(info._reclaim4_start, reclaim_size);
				hndrte_arena_add((uint32)info._reclaim4_start, reclaim_size);
				printf(r_fmt_str, "4", reclaim_size);
			}
		}
#endif /* BOOTLOADER_PATCH_RECLAIM */

		/* XXX Some ROMs were incorrectly generated with the the ROM 'logstrs' and 'lognums'
		 * sections inserted in between .data and .bss. They should have been linked to
		 * special overlay sections. This means that there is an unused memory block in
		 * ROM/RAM shared memory (shdat) that will be unused. Add special case logic to
		 * reclaim this.
		 */
#if defined(EVENT_LOG_SHDAT_RECLAIM)
		if (EVENT_LOG_SHDAT_SIZE) {
			/* blow away the reclaim region */
			bzero((void *)EVENT_LOG_SHDAT_START, EVENT_LOG_SHDAT_SIZE);
			hndrte_arena_add(EVENT_LOG_SHDAT_START, EVENT_LOG_SHDAT_SIZE);

			printf(r_fmt_str, "EL", EVENT_LOG_SHDAT_SIZE);
		}
#endif /* EVENT_LOG_SHDAT_RECLAIM */

		bcmreclaimed = FALSE;
		attach_part_reclaimed = TRUE;

#ifdef HNDRTE_PT_GIANT
		hndrte_append_ptblk();
#endif // endif
#ifdef HNDLBUFCOMPACT
		/* Fix up 2M boundary before pktpool_fill */
		hndrte_lbuf_fixup_2M_tcm();
#endif // endif

#ifdef BCMPKTPOOL
		if (POOL_ENAB(pktpool_shared)) {
#if defined(WLC_LOW) && !defined(WLC_HIGH)
			/* for bmac, populate all the configured buffers */
			pktpool_fill(hndrte_osh, pktpool_shared, FALSE);
#else
			pktpool_fill(hndrte_osh, pktpool_shared, TRUE);
#endif // endif
		}
#endif /* BCMPKTPOOL */
/* fragpool reclaim */
#ifdef BCMFRAGPOOL
		if (POOL_ENAB(pktpool_shared_lfrag)) {
#if defined(WLC_LOW_ONLY)
			pktpool_fill(hndrte_osh, pktpool_shared_lfrag, FALSE);
#else
			pktpool_fill(hndrte_osh, pktpool_shared_lfrag, TRUE);
#endif // endif
#ifdef BCM_DHDHDR
			lfbufpool_fill(hndrte_osh, d3_lfrag_buf_pool, FALSE);
			lfbufpool_fill(hndrte_osh, d11_lfrag_buf_pool, FALSE);
#endif /* BCM_DHDHDR */
		}
#endif /* BCMFRAGPOOL */
/* rx fragpool reclaim */
#ifdef BCMRXFRAGPOOL
		if (POOL_ENAB(pktpool_shared_rxlfrag)) {
#if defined(WLC_LOW_ONLY)
			pktpool_fill(hndrte_osh, pktpool_shared_rxlfrag, FALSE);
#else
			pktpool_fill(hndrte_osh, pktpool_shared_rxlfrag, TRUE);
#endif // endif
		}
#endif /* BCMRXFRAGPOOL */

		goto exit;
	}

	if (!preattach_part_reclaimed) {
		reclaim_size = (uint)(info._reclaim3_end - info._reclaim3_start);

		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim3_start, reclaim_size);
			hndrte_arena_add((uint32)info._reclaim3_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "0", reclaim_size);

		attach_part_reclaimed = FALSE;
		preattach_part_reclaimed = TRUE;
	}
exit:
#ifdef HNDLBUFCOMPACT
	hndrte_lbuf_fixup_2M_tcm();
#endif // endif
	return;		/* return is needed to avoid compilation error
			   error: label at end of compound statement
			*/
}
#endif /* DONGLEBUILD */

#ifdef HNDRTE_PT_GIANT
static void *mem_pt_get(uint size);
static bool mem_pt_put(void *pblk);
static void mem_pt_printuse(void);
static void mem_pt_append(void);
#else
#ifdef DMA_TX_FREE
#warning "DMA_TX_FREE defined without HNDRTE_PT_GIANT set!"
#endif /* DMA_TX_FREE */
#endif /* HNDRTE_PT_GIANT */

/** This function must be forced into RAM since it uses RAM specific linker symbols.  */
void
BCMRAMFN(hndrte_image_info)(hndrte_image_info_t *i)
{
	memset(i, 0, sizeof(*i));

	i->_text_start = text_start;
	i->_text_end = text_end;
	i->_rodata_start = rodata_start;
	i->_rodata_end = rodata_end;
	i->_data_start = data_start;
	i->_data_end = data_end;
	i->_bss_start = bss_start;
	i->_bss_end = bss_end;

#ifdef BCMRECLAIM
	{
		extern char _rstart1[], _rend1[];
		i->_reclaim1_start = _rstart1;
		i->_reclaim1_end = _rend1;
	}
#endif // endif

#ifdef DONGLEBUILD
	{
		extern char _rstart2[], _rend2[];
		extern char _rstart3[], _rend3[];

		i->_reclaim2_start = _rstart2;
		i->_reclaim2_end = _rend2;
		i->_reclaim3_start = _rstart3;
		i->_reclaim3_end = _rend3;
	}

#if defined(BCMROMOFFLOAD)
	{
		extern char _rstart4[], _rend4[];
		extern char bootloader_patch_start[], bootloader_patch_end[];

		i->_reclaim4_start   = _rstart4;
		i->_reclaim4_end     = _rend4;
		i->_boot_patch_start = bootloader_patch_start;
		i->_boot_patch_end   = bootloader_patch_end;
	}
#endif /* BCMROMOFFLOAD */
#endif /* DONGLEBUILD */
}

#define KB(bytes)	(((bytes) + 1023) / 1024)

/* ROM accessor. If '_memsize' is used directly, the tools think the assembly symbol '_memsize' is
 * a function, which will result in an unexpected entry in the RAM jump table.
 */
uint32
hndrte_get_memsize(void)
{
	return (_memsize);
}

/* ROM accessor.
 */
uint32
hndrte_get_rambase(void)
{
#ifdef  __ARM_ARCH_7R__
	return (_atcmrambase);
#endif // endif
	return 0;
}

static void free_last_memalloc_block(void);
void hndrte_get_memuse(void *arg, int len);
bool hndrte_set_memalloc(int size);
uint hndrte_get_memalloc(void);
static uint* last_memalloc_block = NULL;
static uint total_memalloc_size = 0;
#define MEMWASTE_CHUNK_SIZE	256

void
hndrte_get_memuse(void *arg, int len)
{
	uint inuse_total;
	uint arena_free;

	inuse_total = inuse_size + inuse_overhead;
	arena_free = arena_size - inuse_total;

#ifdef BCMPKTPOOL
	if (POOL_ENAB(pktpool_shared)) {
		int n, pp_avail;
		n = pktpool_len(pktpool_shared);
		pp_avail = pktpool_avail(pktpool_shared);

		sprintf(arg, "Heap Free: %d(%dK), Packet pool: %d of %d available\n",
			arena_free, KB(arena_free), pp_avail, n);
	}
#else /* BCMPKTPOOL */
	sprintf(arg, "Heap Free: %d(%dK)\n",
		arena_free, KB(arena_free));
#endif /* BCMPKTPOOL */
}

static void free_last_memalloc_block(void)
{
	uint* prev_block;

	prev_block = (uint*) *last_memalloc_block;
	hndrte_free((void*)last_memalloc_block);
	last_memalloc_block = prev_block;
}

bool hndrte_set_memalloc(int size)
{
	int i;
	uint* prev_block;

	if (size == 0) {
		/* free previously allocated block chain */
		while (last_memalloc_block)
			free_last_memalloc_block();
		total_memalloc_size = 0;
	}
	else {
		/* allocate requested memory in chunks of 256 bytes */
		for (i = 0; i < (size*4); i++) {
			/* allocate 256 bytes including overhead */
			prev_block = (uint*)hndrte_malloc(MEMWASTE_CHUNK_SIZE - sizeof(mem_t));
			if (prev_block == NULL) {
				printf("Cannot allocate enough memory\n");
				/* free memory allocated by this call only */
				while (last_memalloc_block && (i-- > 0))
					free_last_memalloc_block();
				return FALSE;
			}
			*prev_block = (uint)last_memalloc_block;
			last_memalloc_block = prev_block;
		}
	}
	total_memalloc_size += size;
	return TRUE;
}

uint hndrte_get_memalloc(void)
{
	return total_memalloc_size;
}

#ifndef BCM_BOOTLOADER
void
hndrte_print_memuse(void)
{
	uint32 *p;
	uint32 cbos;
	hndrte_image_info_t info;
	size_t tot, text_len, rodata_len, data_len, bss_len;
	uint inuse_total;
	uint arena_free;
	uint32 memsize, rambase;
	int tot_plen;
	int tot_overhead;

	hndrte_image_info(&info);
	memsize = hndrte_get_memsize();
	rambase = hndrte_get_rambase();

	text_len = (info._text_end - info._text_start);
	rodata_len = (info._rodata_end - info._rodata_start);
	data_len = (info._data_end - info._data_start);
	bss_len = (info._bss_end - info._bss_start);

	text_len += rodata_len;
	tot = text_len + data_len + bss_len;
	tot += inuse_hwm + HNDRTE_STACK_SIZE;

	/* Add amount of memory in region at the end of memory to total. */
	tot += (rambase + memsize - (int)tos);

	inuse_total = inuse_size + inuse_overhead;
	arena_free = arena_size - inuse_total;

	printf("Memory usage:\n");
	printf("\tText: %ld(%ldK), Data: %ld(%ldK), Bss: %ld(%ldK), Stack: %dK\n",
	       text_len, KB(text_len),
	       data_len, KB(data_len),
	       bss_len, KB(bss_len),
	       KB(HNDRTE_STACK_SIZE));
	printf("\tArena total: %d(%dK), Free: %d(%dK), In use: %d(%dK), HWM: %d(%dK)\n",
	       arena_size, KB(arena_size),
	       arena_free, KB(arena_free),
	       inuse_size, KB(inuse_size),
	       inuse_hwm, KB(inuse_hwm));
	printf("\tIn use + overhead: %d(%dK), Max memory in use: %ld(%ldK)\n",
	       inuse_total, KB(inuse_total),
	       tot, KB(tot));
	printf("\tMalloc failure count: %d\n", mf_count);

	if (*bos != STACK_MAGIC)
		printf("\tStack bottom has been overwritten\n");
	else {
		for (p = bos; p < (uint32 *)(&cbos); p++)
			if (*p != STACK_MAGIC)
				break;

		printf("\tStack bottom: 0x%p, lwm: 0x%p, curr: 0x%p, top: 0x%p\n",
		       bos, p, &p, tos);
		printf("\tFree stack: 0x%x(%d) lwm: 0x%x(%d)\n",
		       ((uintptr)(&p) - (uintptr)bos),
		       ((uintptr)(&p) - (uintptr)bos),
		       ((uintptr)p - (uintptr)bos),
		       ((uintptr)p - (uintptr)bos));
		printf("\tInuse stack: 0x%x(%d) hwm: 0x%x(%d)\n",
		       ((uintptr)tos - (uintptr)(&p)),
		       ((uintptr)tos - (uintptr)(&p)),
		       ((uintptr)tos - (uintptr)p),
		       ((uintptr)tos - (uintptr)p));
	}
	tot_plen = 0;
	tot_overhead = 0;
#ifdef BCMPKTPOOL
	int n, m;
	int plen = 0;
	int overhead = 0;
	if (POOL_ENAB(pktpool_shared)) {

		n = pktpool_len(pktpool_shared);
		m = pktpool_avail(pktpool_shared);
		plen = pktpool_plen(pktpool_shared) * n;
		overhead = (pktpool_plen(pktpool_shared) + LBUFSZ) * n;

		tot_plen = tot_plen + plen;
		tot_overhead = tot_overhead + overhead;

		printf("\tIn use pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen, KB(plen),
		       overhead, KB(overhead));
#if !defined(BCMFRAGPOOL) && !defined(BCMRXFRAGPOOL)
		printf("\tIn use - pool: %d(%dK), w/oh: %d(%dK)\n",
		       inuse_size - tot_plen, KB(inuse_size - tot_plen),
		       (inuse_size + inuse_overhead) - tot_overhead,
		       KB((inuse_size + inuse_overhead) - tot_overhead));
#endif // endif
	}
#endif /* BCMPKTPOOL */
#ifdef BCMFRAGPOOL
	if (POOL_ENAB(pktpool_shared_lfrag)) {
		int n, m;
		int plen_lf, overhead_lf;

		n = pktpool_len(pktpool_shared_lfrag);
		m = pktpool_avail(pktpool_shared_lfrag);
		plen_lf = pktpool_plen(pktpool_shared_lfrag) * n;
		overhead_lf = (pktpool_plen(pktpool_shared_lfrag) + LBUFFRAGSZ) * n;

		tot_plen = tot_plen + plen_lf;
		tot_overhead = tot_overhead + overhead_lf;
		printf("\tIn use Frag pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_lf, KB(plen_lf),
		       overhead_lf, KB(overhead_lf));
#ifndef BCMRXFRAGPOOL
		printf("\tIn use - pools : %d(%dK), w/oh: %d(%dK)\n",
		       inuse_size - (tot_plen),
			KB(inuse_size - (tot_plen)),
		       (inuse_size + inuse_overhead) - (tot_overhead),
		       KB((inuse_size + inuse_overhead) - (tot_overhead)));
#endif // endif
	}

#ifdef BCM_DHDHDR
	hndrte_cons_flush();

	if (d11_lfrag_buf_pool->inited) {
		int n, m;
		int buflen_lf, overhead_lf;

		/* D11_BUFFER */
		n = d11_lfrag_buf_pool->len;
		m = d11_lfrag_buf_pool->avail;
		buflen_lf = d11_lfrag_buf_pool->buflen * n;
		overhead_lf = (d11_lfrag_buf_pool->buflen + LFBUFSZ) * n;
		printf("\tIn use D11_BUF pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
			n, m,
			buflen_lf, KB(buflen_lf),
			overhead_lf, KB(overhead_lf));

		/* D3_BUFFER */
		n = d3_lfrag_buf_pool->len;
		m = d3_lfrag_buf_pool->avail;
		buflen_lf = d3_lfrag_buf_pool->buflen * n;
		overhead_lf = (d3_lfrag_buf_pool->buflen + LFBUFSZ) * n;
		printf("\tIn use D3_BUF pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
			n, m,
			buflen_lf, KB(buflen_lf),
			overhead_lf, KB(overhead_lf));
	}
#endif /* BCM_DHDHDR */
#endif /* BCMFRAGPOOL */
#ifdef BCMRXFRAGPOOL
	if (POOL_ENAB(pktpool_shared_rxlfrag)) {
		int n, m;
		int plen_rxlf, overhead_rxlf;

		n = pktpool_len(pktpool_shared_rxlfrag);
		m = pktpool_avail(pktpool_shared_rxlfrag);
		plen_rxlf = pktpool_plen(pktpool_shared_rxlfrag) * n;
		overhead_rxlf = (pktpool_plen(pktpool_shared_rxlfrag) + LBUFFRAGSZ) * n;

		tot_plen = tot_plen + plen_rxlf;
		tot_overhead = tot_overhead + overhead_rxlf;

		printf("\tIn useRX  Frag pool %d(%d): %d(%dK), w/oh: %d(%dK)\n",
		       n, m,
		       plen_rxlf, KB(plen_rxlf),
		       overhead_rxlf, KB(overhead_rxlf));
		printf("\tIn use - pools : %d(%dK), w/oh: %d(%dK)\n",
		       inuse_size - (tot_plen),
			KB(inuse_size - (tot_plen)),
		       (inuse_size + inuse_overhead) - (tot_overhead),
		       KB((inuse_size + inuse_overhead) - (tot_overhead)));
	}
#endif /* BCMRXFRAGPOOL */

#if defined(BCMPKTIDMAP)
	printf("\tPktId Total: %d, Free: %d, Failed: %d\n",
		hndrte_pktid_max, hndrte_pktid_free_cnt(), hndrte_pktid_fail_cnt());
#endif // endif

#ifdef HNDRTE_PT_GIANT
	mem_pt_printuse();
#endif // endif
}

void
hndrte_dump_test(uint32 arg, uint argc, char *argv[])
{
	char a = 'a';
	uint32 numc = 8;
	uint32 i;

	if (argc == 2)
		numc = atoi(argv[1]);

	for (i = 0; i < numc; i++) {
		putc(a++);
		if ((a - 'a') == 26)
			a = 'a';
	}
	putc('\n');
}
#endif /* BCM_BOOTLOADER */

#ifdef BCM_VUSBD
void
hndrte_print_vusbstats(void)
{
	vusbd_print_stats();
}
#endif // endif

#if defined(HNDRTE_CONSOLE) && defined(BCMDBG_POOL)
static void
hndrte_pool_dump(void)
{
	pktpool_dbg_dump(pktpool_shared);
#ifdef BCMFRAGPOOL
	pktpool_dbg_dump(pktpool_shared_lfrag);
#endif /* BCMFRAGPOOL */

#ifdef BCMRXFRAGPOOL
	pktpool_dbg_dump(pktpool_shared_rxlfrag);
#endif /* BCMRXFRAGPOOL */
}

static void
hndrte_pool_notify(void)
{
	pktpool_dbg_notify(pktpool_shared);
#ifdef BCMFRAGPOOL
	pktpool_dbg_notify(pktpool_shared_lfrag);
#endif /* BCMFRAGPOOL */

#ifdef BCMRXFRAGPOOL
	pktpool_dbg_notify(pktpool_shared_rxlfrag);
#endif /* BCMRXFRAGPOOL */
}
#endif /* HNDRTE_CONSOLE && BCMDBG_POOL */
#if defined(HNDRTE_CONSOLE) && !defined(BCM_BOOTLOADER)
static void
hndrte_memdmp(void *arg, int argc, char **argv)
{
	uint32 addr, size, count;

	if (argc < 3) {
		printf("%s: start len [count]\n", argv[0]);
		return;
	}

	addr = bcm_strtoul(argv[1], NULL, 0);
	size = bcm_strtoul(argv[2], NULL, 0);
	count = argv[3] ? bcm_strtoul(argv[3], NULL, 0) : 1;

	for (; count-- > 0; addr += size)
		if (size == 4)
			printf("%08x: %08x\n", addr, *(uint32 *)addr);
		else if (size == 2)
			printf("%08x: %04x\n", addr, *(uint16 *)addr);
		else
			printf("%08x: %02x\n", addr, *(uint8 *)addr);
}
#endif /* HNDRTE_CONSOLE  && !BCM_BOOTLOADER */

/**
 * Malloc memory to simulate low memory environment
 * Memory waste [kk] in KB usage: mw [kk]
 */
#ifndef BCM_BOOTLOADER
void
hndrte_print_memwaste(uint32 arg, uint argc, char *argv[])
{
	/* Only process if input argv specifies the mw size(in KB) */
	if (argc > 1)
		printf("%p\n", hndrte_malloc(atoi(argv[1]) * 1024));
}
#endif /* BCM_BOOTLOADER */

#ifdef BCMDBG_MEM
int
hndrte_memcheck(char *file, int line)
{
	mem_t *this = inuse_mem.next;

	while (this) {
		if (this->magic != MEM_MAGIC) {
			printf("CORRUPTION: %s %d\n", file, line);
			printf("\n%s: Corrupt magic (0x%x); size %d; file %s, line %d\n\n",
			       __FUNCTION__, this->magic, this->size, this->file, this->line);
			return (1);
		}
		this = this->next;
	}
	return (0);
}

void
hndrte_print_malloc(void)
{
	uint32 inuse = 0, free = 0, total;
	mem_t *this = inuse_mem.next;

	printf("Heap inuse list:\n");
	printf("Addr\t\tSize\tfile:line\t\tmalloc fn\n");
	while (this) {
		printf("%p\t%5d\t%s:%d\t%p\t%p\n",
		       &this[1], this->size,
		       this->file, this->line, this->malloc_function, this->free_function);
		inuse += this->size + sizeof(mem_t);
		this = this->next;
	}
	printf("Heap free list:\n");
	this = free_mem_dbg.next;
	while (this) {
		printf("%p: 0x%x(%d) @ %p\n",
		       this, this->size, this->size, &this[1]);
		free += this->size + sizeof(mem_t);
		this = this->next;
	}
	total = inuse + free;
	printf("Heap Dyn inuse: 0x%x(%d), inuse: 0x%x(%d)\nDyn free: 0x%x(%d), free: 0x%x(%d)\n",
	       inuse, inuse, inuse_size, inuse_size, free, free,
	       (arena_size - inuse_size), (arena_size - inuse_size));
	if (total != arena_size)
		printf("Total (%d) does NOT agree with original %d!\n",
		       total, arena_size);
}
#endif /* BCMDBG_MEM */

/*
 * ======HNDRTE====== Partition allocation:
 */
void *
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
hndrte_malloc_ptblk(uint size, const char *file, int line)
#else
hndrte_malloc_ptblk(uint size)
#endif // endif
{
#ifdef HNDRTE_PT_GIANT
	return mem_pt_get(size);
#else
	return NULL;
#endif // endif
}

int
hndrte_free_pt(void *where)
{
#ifdef HNDRTE_PT_GIANT
	return mem_pt_put(where);
#else
	return 0;
#endif // endif
}

void
hndrte_append_ptblk(void)
{
#ifdef HNDRTE_PT_GIANT
	mem_pt_append();
#endif // endif
}

#ifdef HNDRTE_PT_GIANT

/* Each partition has a overhead management structure, which contains the partition specific
 * attributes. But within the partition, each subblock has no overhead.
 *  subblocks are linked together using the first 4 bytes as pointers, which will be
 *    available to use once assigned
 *  partition link list free pointer always point to the available empty subblock
 */

/* XXX BLKSIZE is a tunable in unit of bytes. It must be multiple of 4 Bytes for alignment.
 * The blocks are intended to be used for usb rx dma buffer, which must support
 *     BCMEXTRAHDROOM + MAXPKTBUFSZ(= PKTBUFSZ + LBUFSZ(60))
 * or  LBUFSZ + BCM_RPC_TP_HOST_AGG_MAX_BYTE since usb rx dma buffer skips BCMEXTRAHDROOM
 */
#ifdef WLC_LOW
#define MEM_PT_BLKSIZE	(LBUFSZ + 3400)
#else
#define MEM_PT_BLKSIZE	(LBUFSZ + BCMEXTRAHDROOM + PKTBUFSZ)
#endif // endif

/* XXX This tunable is in unit of bytes. It depends on desired throughput.
 * For usb rx dma case, the failure is not fatal, but will stall dma receiving.
 * With typical host->dongle->ap uplink tcp traffic experiments(host->dongle agg on)
 *  16(58.5K)->40% request failure, average tput ~110Mbps
 *  18(65.9K)->25% request failure, average tput ~115Mbps
 *  20(73.2K)->10% request failure, average tput ~118Mbps
 * BMAC: Theoretically, high driver can push down 48MPDU(3 AMPDU) or 24*4K buffer
 * avoid call with return failure, one spare one is needed to be robust
 * So 25 is ideal. But it adds stress to other system malloc requests
 * Note, this may exceed certain debug dongle driver image size like 4322-bmac/rom-ag-debug
 */
#ifndef MEM_PT_BLKNUM
#if defined(DMA_TX_FREE)
#ifdef BCMDBG
#define MEM_PT_BLKNUM	5	/* minimal buffers for debug build with less free memory */
#else
#define MEM_PT_BLKNUM	22
#endif /* BCMDBG */
#else
#define MEM_PT_BLKNUM	25
#endif /* DMA_TX_FREE */
#endif /* MEM_PT_BLKNUM */

#define MEM_PT_POOL	(MEM_PT_BLKSIZE * MEM_PT_BLKNUM)
#define MEM_PT_GAINT_THRESHOLD	(MAXPKTBUFSZ + LBUFSZ)	/* only bigger pkt use this method */
char *mem_partition1;

typedef struct {	/* memory control block */
	void *mem_pt_addr;	/* Pointer to beginning of memory partition */
	void *mem_pt_freell; /* free blocks link list head */
	hndrte_lowmem_free_t *lowmem_free_list; /* list of lowmem free functions */
	uint32 mem_pt_blk_size; /* Size (in bytes) of each block of memory */
	uint32 mem_pt_blk_total; /* Total number of blocks in this partition */
	uint32 mem_pt_blk_free; /* Number of memory blocks remaining in this partition */
	uint32 cnt_req;		/* counter: malloc request */
	uint32 cnt_fail;	/* counter: malloc fail */
} pt_mcb_t;

static pt_mcb_t g_mem_pt_tbl;

static void mem_pt_init(void);
static uint16 mem_pt_create(char *addr, int size, uint32 blksize);

/** initialize memory partition manager */
static void
mem_pt_init(void)
{
	pt_mcb_t *pmem;

	pmem = &g_mem_pt_tbl;

	pmem->mem_pt_addr = (void *)0;
	pmem->mem_pt_freell = NULL;
	pmem->mem_pt_blk_size = 0;
	pmem->mem_pt_blk_free = 0;
	pmem->mem_pt_blk_total = 0;

	/* create partitions */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	mem_partition1 = hndrte_malloc_align(MEM_PT_POOL, 4, __FILE__, __LINE__);
#else
	mem_partition1 = hndrte_malloc_align(MEM_PT_POOL, 4);
#endif // endif
	ASSERT(mem_partition1 != NULL);
	mem_pt_create(mem_partition1, MEM_PT_POOL, MEM_PT_BLKSIZE);
}

/**
 * create a fixed-sized memory partition
 * Arguments   : base, is the starting address of the memory partition
 *               blksize  is the size (in bytes) of each block in the memory partition
 * Return: nblks successfully allocated out the memory area
 * NOTE: no alignment adjust for base. assume users knows what they want and have done that
*/
static uint16
mem_pt_create(char *base, int size, uint32 blksize)
{
	pt_mcb_t *pmem;
	uint8 *pblk;
	void **plink;
	uint num;

	if (blksize < sizeof(void *)) { /* Must contain space for at least a pointer */
		return 0;
	}

	pmem = &g_mem_pt_tbl;

	/* Create linked list of free memory blocks */
	plink = (void **)base;
	pblk = (uint8 *)base + blksize;
	num = 0;

	while (size >= blksize) {
		num++;
		*plink = (void *)pblk;
		plink  = (void **)pblk;
		pblk += blksize;
		size -= blksize;
	}
	*plink = NULL;	/* Last memory block points to NULL */

	pmem->mem_pt_addr = base;
	pmem->mem_pt_freell = base;
	pmem->mem_pt_blk_free = num;
	pmem->mem_pt_blk_total = num;
	pmem->mem_pt_blk_size = blksize;

	printf("mem_pt_create: addr %p, blksize %d, totblk %d free %d leftsize %d\n",
	       base, pmem->mem_pt_blk_size, pmem->mem_pt_blk_total,
	       pmem->mem_pt_blk_free, size);
	return num;
}

void
hndrte_pt_lowmem_register(hndrte_lowmem_free_t *lowmem_free_elt)
{
	pt_mcb_t *pmem = &g_mem_pt_tbl;

	lowmem_free_elt->next = pmem->lowmem_free_list;
	pmem->lowmem_free_list = lowmem_free_elt;
}

void
hndrte_pt_lowmem_unregister(hndrte_lowmem_free_t *lowmem_free_elt)
{
	pt_mcb_t *pmem = &g_mem_pt_tbl;
	hndrte_lowmem_free_t **prev_ptr = &pmem->lowmem_free_list;
	hndrte_lowmem_free_t *elt = pmem->lowmem_free_list;

	while (elt) {
		if (elt == lowmem_free_elt) {
			*prev_ptr = elt->next;
			return;
		}
		prev_ptr = &elt->next;
		elt = elt->next;
	}
}

static void
mem_pt_lowmem_run(pt_mcb_t *pmem)
{
	hndrte_lowmem_free_t *free_elt;

	for (free_elt = pmem->lowmem_free_list;
	     free_elt != NULL && pmem->mem_pt_blk_free == 0;
	     free_elt = free_elt->next)
		(free_elt->free_fn)(free_elt->free_arg);
}

static void *
mem_pt_get(uint size)
{
	pt_mcb_t *pmem = &g_mem_pt_tbl;
	void *pblk;

	if (size > pmem->mem_pt_blk_size) {
		printf("request partition fixbuf size too big %d\n", size);
		ASSERT(0);
		return NULL;
	}

	pmem->cnt_req++;
	if (pmem->mem_pt_blk_free == 0)
		mem_pt_lowmem_run(pmem);

	if (pmem->mem_pt_blk_free > 0) {
		pblk = pmem->mem_pt_freell;

		/* Adjust the freeblk pointer to next block */
		pmem->mem_pt_freell = *(void **)pblk;
		pmem->mem_pt_blk_free--;
		/* printf("mem_pt_get %p, freell %p\n", pblk, pmem->mem_pt_freell); */
		return (pblk);
	} else {
		pmem->cnt_fail++;
		return NULL;
	}
}

static bool
mem_pt_put(void *pblk)
{
	pt_mcb_t *pmem = &g_mem_pt_tbl;

	if (pmem->mem_pt_blk_free >= pmem->mem_pt_blk_total) {
		/* Make sure all blocks not already returned */
		return FALSE;
	}

	/* printf("mem_pt_put %p, freell %p\n", pblk, pmem->mem_pt_freell); */

	*(void **)pblk = pmem->mem_pt_freell;
	pmem->mem_pt_freell = pblk;
	pmem->mem_pt_blk_free++;
	return TRUE;
}

static void
mem_pt_printuse(void)
{
	printf("Partition: blksize %u totblk %u freeblk %u, malloc_req %u, fail %u (%u%%)\n",
	g_mem_pt_tbl.mem_pt_blk_size, g_mem_pt_tbl.mem_pt_blk_total,
	g_mem_pt_tbl.mem_pt_blk_free, g_mem_pt_tbl.cnt_req, g_mem_pt_tbl.cnt_fail,
	(g_mem_pt_tbl.cnt_req == 0) ? 0 : (100 * g_mem_pt_tbl.cnt_fail) / g_mem_pt_tbl.cnt_req);
}

static void
mem_pt_append(void)
{
#ifdef MEM_PT_BLKNUM_APPEND
#define MEM_PT_SIZE (MEM_PT_BLKSIZE * MEM_PT_BLKNUM_APPEND)

	char *mem_pt = NULL;
	int i = 0;
	pt_mcb_t *pmem = &g_mem_pt_tbl;

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
	mem_pt = hndrte_malloc_align(MEM_PT_SIZE, 4, __FILE__, __LINE__);
#else
	mem_pt = hndrte_malloc_align(MEM_PT_SIZE, 4);
#endif // endif

	if (mem_pt != NULL) {
		pmem->mem_pt_blk_total += MEM_PT_BLKNUM_APPEND;
		for (i = 0; i < MEM_PT_BLKNUM_APPEND; i++)
			mem_pt_put(&mem_pt[i * MEM_PT_BLKSIZE]);
	}
#endif /* MEM_PT_BLKNUM_APPEND */
}

#endif /* HNDRTE_PT_GIANT */

/*
 * ======HNDRTE====== Timer support:
 *
 * All these routines need some interrupt protection if they are shared
 * by ISR and other functions. However since they are all called from
 * timer or other ISRs except at initialization time so it is safe to
 * not have the protection.
 *
 * hndrte_init_timer(void *context, void *data,
 *	void (*mainfn)(hndrte_timer_t*), void (*auxfn)(void*));
 * hndrte_free_timer(hndrte_timer_t *t);
 * hndrte_add_timer(hndrte_timer_t *t, uint ms, int periodic);
 * hndrte_del_timer(hndrte_timer_t *t);
 *
 * hndrte_init_timeout(ctimeout_t *new);
 * hndrte_del_timeout(ctimeout_t *new);
 * hndrte_timeout(ctimeout_t *new, uint32 ms, to_fun_t fun, uint32 arg);
 */

/* Allow to program the h/w timer */
static bool timer_allowed = TRUE;
/* Don't program the h/w timer again if it has been */
static uint32 lasttime = 0;

/**
 * linked list of one shot and/or periodic timers in expiration order. The first item in this list
 * is empty, apart from its next timer pointer.
 */
static ctimeout_t *timers = NULL;

static void hndrte_rte_timer(void *tid);
static void update_timeout_list(void);
static bool in_timeout = FALSE;

static void
BCMATTACHFN(hndrte_timers_init)(void)
{
	timers = hndrte_malloc(sizeof(ctimeout_t));
	ASSERT(timers);
	bzero(timers, sizeof(ctimeout_t));

	hndrte_update_now();
#ifdef BCMDBG_SD_LATENCY
	hndrte_update_now_us();
#endif /* BCMDBG_SD_LATENCY */
}

/**
 * A linked list of timers is maintained by RTE. On a regular basis, each timer in this list needs
 * to be updated to the current situation (= current time). That means that the time until
 * expiration has to be adjusted, and possibly a timer has to be flagged as being 'expired', so a
 * subsequent function can process all expired timers.
 */
static void
update_timeout_list(void)
{
	uint32 delta, current;
	volatile ctimeout_t *head = (volatile ctimeout_t *)timers;
	ctimeout_t *this = head->next;

	current = hndrte_time();
	delta = current - lasttime;
	lasttime = current;
	if (!this)
		return;
	if (this->ms == 0 || delta != 0) {
		while (this != NULL) {
			if (this->ms <= delta) {
				/* timer has expired */
				delta -= this->ms;
				this->ms = 0;
				/* Don't expire from timeout code */
				if (!in_timeout)
					this->expired = TRUE;
				this = this->next;
			} else {
				this->ms -= delta;
				break;
			}
		}
	}
}

/** Remove all expired timers from the timeout list and invoke their callbacks */
static int
run_timeouts(void)
{
	ctimeout_t *this;
	volatile ctimeout_t *head = (volatile ctimeout_t *)timers;
	to_fun_t fun;

	int rc = 0;

	if ((this = head->next) == NULL)
		return 0;

	update_timeout_list();

	in_timeout = TRUE;

	/* Always start from the head. Note that timers are sorted on expiration in linked list */
	while (((this = head->next) != NULL) &&
	       (this->expired == TRUE)) {
		rc = 1;
		head->next = this->next;
		fun = this->fun;
		this->fun = NULL;
		this->expired = FALSE;
		BUZZZ_LVL2(HNDRTE_TIMER_FN, 1, (uint32)fun);
		fun(this->arg);
		BUZZZ_LVL2(HNDRTE_TIMER_FN_RTN, 0);
	}

	in_timeout = FALSE;

	return rc;
}

/** Remove specified timeout from the list */
void
hndrte_del_timeout(ctimeout_t *new)
{
	ctimeout_t *this, *prev = timers;

	while (((this = prev->next) != NULL)) {
		if (this == new) {
			if (this->next != NULL)
				this->next->ms += this->ms;
			prev->next = this->next;
			this->fun = NULL;
			break;
		}
		prev = this;
	}
}

void
hndrte_init_timeout(ctimeout_t *new)
{
	bzero(new, sizeof(ctimeout_t));
}

/** Add caller specified one-off timeout to the list */
bool
hndrte_timeout(ctimeout_t *new, uint32 _ms, to_fun_t fun, void *arg)
{
	ctimeout_t *prev, *this;
	uint32 deltams = _ms;

	if (!new)
		return FALSE;

	if (new->fun != NULL) {
		RTE_MSG(("fun not null in 0x%p, timer already in list?\n", new));
#ifdef	BCMDBG
		hndrte_print_timers(0, 0, NULL);
#endif // endif
		return FALSE;
	}

	update_timeout_list();

	/* find a proper location for the new timeout and update the timer value */
	prev = timers;
	while (((this = prev->next) != NULL) && (this->ms <= deltams)) {
		deltams -= this->ms;
		prev = this;
	}
	if (this != NULL)
		this->ms -= deltams;

	new->fun = fun;
	new->arg = arg;
	new->ms = deltams;

	/* insert the new timeout */
	new->next = this;
	prev->next = new;

	return TRUE;
}

/** This call back function is called when a timer in the linked list of timers expires. */
static void
hndrte_rte_timer(void *tid)
{
	hndrte_timer_t *t = (hndrte_timer_t *)tid;
	bool freedone = FALSE;

	if (t && t->set) {
		if (!t->periodic) {
			t->set = FALSE;
		} else {
			hndrte_timeout(&t->t, t->interval, hndrte_rte_timer, (void *)t);
		}

		/* Store the t->_freedone so that if the timer t is deleted inside the mainfn(),
		* we will have a valid copy of the t->__freedone which is later used to free
		* the timer. This will prevent double free of the same timer.
		*/
		freedone = t->_freedone;
		if (t->mainfn)
			(*t->mainfn)(t);

		if (freedone == TRUE) {
			hndrte_free_timer(t);
		}
	}
}

#ifdef BCMDBG
void
hndrte_print_timers(uint32 arg, uint argc, char *argv[])
{
	ctimeout_t *this;

	if ((this = timers->next) == NULL) {
		printf("No timers\n");
		return;
	}
	while (this != NULL) {
		printf("timer %p, fun %p, arg %p, %d ms\n", this, this->fun, this->arg,
		       this->ms);
		this = this->next;
	}
}
#endif /* BCMDBG */

#ifdef BCMDBG_LOADAVG
static void
hndrte_loadavg_test(void)
{
	uint32 stime;
	uint32 buf[8];

	/* Busy loop for 20 sec; call function in ROM to spend time in both RAM and ROM */

	stime = hndrte_time();

	while (hndrte_time() - stime < 20000)
		memset(buf, 0, sizeof(buf));
}
#endif /* BCMDBG_LOADAVG */

hndrte_timer_t*
hndrte_init_timer(void *context, void *data, void (*mainfn)(hndrte_timer_t*), void (*auxfn)(void*))
{
	hndrte_timer_t *t = (hndrte_timer_t *)hndrte_malloc(sizeof(hndrte_timer_t));

	if (t) {
		bzero(t, sizeof(hndrte_timer_t));
		t->context = context;
		t->mainfn = mainfn;
		t->auxfn = auxfn;
		t->data = data;
		t->set = FALSE;
		t->periodic = FALSE;
	} else {
		RTE_MSG(("hndrte_init_timer: hndrte_malloc failed\n"));
	}

	return t;
}

void
hndrte_free_timer(hndrte_timer_t *t)
{
	if (t) {
		if (t->set) {
			printf("Err: active timer freed\n");
			RTE_MSG(("  t=%p t->t.fun=%p t->t.arg=%p\n", t, t->t.fun, t->t.arg));
			ASSERT(0);
		}
		hndrte_free(t);
	}
}

bool
hndrte_add_timer(hndrte_timer_t *t, uint ms, int periodic)
{
	if (t) {
		t->set = TRUE;
		t->periodic = periodic;
		t->interval = ms;

		return hndrte_timeout(&t->t, ms, hndrte_rte_timer, (void *)t);
	}
	return FALSE;
}

bool
hndrte_del_timer(hndrte_timer_t *t)
{
	if (t->set) {
		t->set = FALSE;
		t->t.expired = FALSE;

		hndrte_del_timeout((ctimeout_t*)&t->t);
	}

	return (TRUE);
}

/** Schedule a completion handler to run at safe time */
int
hndrte_schedule_work(void *context, void *data, void (*taskfn)(hndrte_timer_t *), int delay)
{
	hndrte_timer_t *task;

	if (!(task = hndrte_init_timer(context, data, taskfn, NULL))) {
		return BCME_NORESOURCE;
	}
	task->_freedone = TRUE;

	if (!hndrte_add_timer(task, delay, FALSE)) {
		hndrte_free_timer(task);
		return BCME_NORESOURCE;
	}

	return 0;
}

static uint32 now = 0;

/** Return the up time in miliseconds */
uint32
hndrte_time(void)
{
	now += hndrte_update_now();

	return now;
}

static uint32 host_reftime_ms = 0;
static uint32 dongle_time_ref = 0;

void
hndrte_set_reftime(uint32 reftime_ms)
{
	host_reftime_ms = reftime_ms;
	dongle_time_ref = hndrte_time();
}

uint32
hndrte_reftime_ms(void)
{
	uint32 dongle_time_ms;

	dongle_time_ms = hndrte_time() - dongle_time_ref;
	dongle_time_ms += host_reftime_ms;

	return dongle_time_ms;
}

#ifdef BCMDBG_SD_LATENCY
static uint32 now_us = 0;  /* Used in latency test for micro-second precision */

uint32
hndrte_time_us(void)
{
	now_us += hndrte_update_now_us();

	return now_us;
}
#endif /* BCMDBG_SD_LATENCY */

/** Cancel the h/w timer if it is already armed and ignore any further h/w timer requests */
void
hndrte_suspend_timer(void)
{
	hndrte_ack_irq_timer();
	timer_allowed = FALSE;
}

/** Resume the timer activities */
void
hndrte_resume_timer(void)
{
	hndrte_set_irq_timer(0);
	timer_allowed = TRUE;
}

/*
 * ======HNDRTE====== Device support:
 *	PCI support if included.
 *	hndrte_add_device(dev, coreid, device): Add a device.
 *	hndrte_get_dev(char *name): Get named device struct.
 *	hndrte_isr(): Invoke device ISRs.
 */

#ifdef	SBPCI

static pdev_t *pcidevs = NULL;

static bool
hndrte_read_pci_config(si_t *sih, uint slot, pci_config_regs *pcr)
{
	uint32 *p;
	uint i;

	extpci_read_config(sih, 1, slot, 0, PCI_CFG_VID, pcr, 4);
	if (pcr->vendor == PCI_INVALID_VENDORID)
		return FALSE;

	for (p = (uint32 *)((uint)pcr + 4), i = 4; i < SZPCR; p++, i += 4)
		extpci_read_config(sih, 1, slot, 0, i, p, 4);

	return TRUE;
}

static uint32
BCMATTACHFN(size_bar)(si_t *sih, uint slot, uint bar)
{
	uint32 w, v, s;

	w = 0xffffffff;
	extpci_write_config(sih, 1, slot, 0, (PCI_CFG_BAR0 + (bar * 4)), &w, 4);
	extpci_read_config(sih, 1, slot, 0,  (PCI_CFG_BAR0 + (bar * 4)), &v, 4);

	/* We don't do I/O */
	if (v & 1)
		return 0;

	/* Figure size */
	v &= 0xfffffff0;
	s = 0 - (int32)v;

	return s;
}

#define	MYPCISLOT	0
#define	HNDRTE_PCIADDR	0x20000000
static uint32 pciaddr = HNDRTE_PCIADDR;

static uint32
BCMATTACHFN(set_bar0)(si_t *sih, uint slot)
{
	uint32 w, mask, s, b0;

	/* Get size, give up if zero */
	s = size_bar(sih, slot, 0);
	if (s == 0)
		return 0;

	mask = s - 1;
	if ((pciaddr & mask) != 0)
		pciaddr = (pciaddr + s) & ~mask;
	b0 = pciaddr;
	extpci_write_config(sih, 1, slot, 0, PCI_CFG_BAR0, &b0, 4);
	extpci_read_config(sih, 1, slot, 0, PCI_CFG_BAR0, &w, 4);

	RTE_MSG(("set_bar0: 0x%x\n", w));
	/* Something went wrong */
	if ((w & 0xfffffff0) != b0)
		return 0;

	pciaddr += s;
	return b0;
}

static void
BCMATTACHFN(hndrte_init_pci)(si_t *sih)
{
	pci_config_regs pcr;
	sbpciregs_t *pci;
	pdev_t *pdev;
	uint slot;
	int rc;
	uint32 w, s, b0;
	uint16 hw, hr;

	rc = hndpci_init_pci(sih, 0);
	if (rc < 0) {
		RTE_MSG(("Cannot init PCI.\n"));
		return;
	} else if (rc > 0) {
		RTE_MSG(("PCI strapped for client mode.\n"));
		return;
	}

	if (!(pci = (sbpciregs_t *)si_setcore(sih, PCI_CORE_ID, 0))) {
		RTE_MSG(("Cannot setcore to PCI after init!!!\n"));
		return;
	}

	if (!hndrte_read_pci_config(sih, MYPCISLOT, &pcr) ||
	    (pcr.vendor != VENDOR_BROADCOM) ||
	    (pcr.base_class != PCI_CLASS_BRIDGE) ||
	    ((pcr.base[0] & 1) == 1)) {
		RTE_MSG(("Slot %d is not us!!!\n", MYPCISLOT));
		return;
	}

	/* Change the 64 MB I/O window to memory with base at HNDRTE_PCIADDR */
	W_REG(hndrte_osh, &pci->sbtopci0, SBTOPCI_MEM | HNDRTE_PCIADDR);

	/* Give ourselves a bar0 for the fun of it */
	if ((b0 = set_bar0(sih, MYPCISLOT)) == 0) {
		RTE_MSG(("Cannot set my bar0!!!\n"));
		return;
	}
	/* And point it to our chipc */
	w = SI_ENUM_BASE;
	extpci_write_config(sih, 1, MYPCISLOT, 0, PCI_BAR0_WIN, &w, 4);
	extpci_read_config(sih, 1, MYPCISLOT, 0, PCI_BAR0_WIN, &w, 4);
	if (w != SI_ENUM_BASE) {
		RTE_MSG(("Cannot set my bar0window: 0x%08x should be 0x%08x\n", w, SI_ENUM_BASE));
	}

	/* Now setup our bar1 */
	if ((s = size_bar(sih, 0, 1)) < _memsize) {
		RTE_MSG(("My bar1 is disabled or too small: %d(0x%x)\n", s, s));
		return;
	}

	/* Make sure bar1 maps PCI address 0, and maps to memory */
	w = 0;
	extpci_write_config(sih, 1, MYPCISLOT, 0, PCI_CFG_BAR1, &w, 4);
	extpci_write_config(sih, 1, MYPCISLOT, 0, PCI_BAR1_WIN, &w, 4);

	/* Do we want to record ourselves in the pdev list? */
	RTE_MSG(("My slot %d, device 0x%04x:0x%04x subsys 0x%04x:0x%04x\n",
	       MYPCISLOT, pcr.vendor, pcr.device, pcr.subsys_vendor, pcr.subsys_id));

	/* OK, finally find the pci devices */
	for (slot = 0; slot < PCI_MAX_DEVICES; slot++) {

		if (slot == MYPCISLOT)
			continue;

		if (!hndrte_read_pci_config(sih, slot, &pcr) ||
		    (pcr.vendor == PCI_INVALID_VENDORID))
			continue;

		RTE_MSG(("Slot %d, device 0x%04x:0x%04x subsys 0x%04x:0x%04x ",
		       slot, pcr.vendor, pcr.device, pcr.subsys_vendor, pcr.subsys_id));

		/* Enable memory & master capabilities */
		hw = pcr.command | 6;
		extpci_write_config(sih, 1, slot, 0, PCI_CFG_CMD, &hw, 2);
		extpci_read_config(sih, 1, slot, 0, PCI_CFG_CMD, &hr, 2);
		if ((hr & 6) != 6) {
			RTE_MSG(("does not support master/memory operation (cmd: 0x%x)\n", hr));
			continue;
		}

		/* Give it a bar 0 */
		b0 = set_bar0(sih, slot);
		if ((b0 = set_bar0(sih, slot)) == 0) {
			RTE_MSG(("cannot set its bar0\n"));
			continue;
		}
		RTE_MSG(("\n"));

		pdev = hndrte_malloc(sizeof(pdev_t));
		pdev->sih = sih;
		pdev->vendor = pcr.vendor;
		pdev->device = pcr.device;
		pdev->bus = 1;
		pdev->slot = slot;
		pdev->func = 0;
		pdev->address = (void *)REG_MAP(SI_PCI_MEM | (b0 - HNDRTE_PCIADDR), SI_PCI_MEM_SZ);
#ifdef BCM4336SIM
		if (pdev->device == 0x4328)
			pdev->inuse = TRUE;
		else
#endif // endif
			pdev->inuse = FALSE;
		pdev->next = pcidevs;
		pcidevs = pdev;
	}
}

#endif	/* SBPCI */

hndrte_dev_t *dev_list = NULL;

/**
 * hndrte_add_device calls the 'probe' function of the caller provided device, which in turn calls
 * 'attach' functions.
 */
int
BCMATTACHFN(hndrte_add_device)(hndrte_dev_t *dev, uint16 coreid, uint16 device)
{
	int err = -1;
	void *softc = NULL;
	void *regs = NULL;
	uint unit = 0;

	if (coreid == NODEV_CORE_ID) {
		/* "Soft" device driver, just call its probe */
		if ((softc = dev->funcs->probe(dev, NULL, SI_BUS, device,
		                               coreid, unit)) != NULL)
			err = 0;
	} else if ((regs = si_setcore(hndrte_sih, coreid, unit)) != NULL) {
		/* Its a core in the SB */
		if ((softc = dev->funcs->probe(dev, regs, SI_BUS, device,
		                               coreid, unit)) != NULL)
			err = 0;
	}
#ifdef	SBPCI
	else {
		/* Try PCI devices */
		pdev_t *pdev = pcidevs;
		while (pdev != NULL) {
			if (!pdev->inuse) {
				dev->pdev = pdev;
				softc = dev->funcs->probe(dev, pdev->address, PCI_BUS,
				                          pdev->device, coreid, unit);
				if (softc != NULL) {
					pdev->inuse = TRUE;
					regs = si_setcore(hndrte_sih, PCI_CORE_ID, 0);
					err = 0;
					break;
				}
			}
			pdev = pdev->next;
		}
		if (softc == NULL)
			dev->pdev = NULL;
	}
#endif	/* SBPCI */

	if (err != 0)
		return err;

	dev->devid = device;
	dev->softc = softc;

	/* Add device to the head of devices list */
	dev->next = dev_list;
	dev_list = dev;

	return 0;
}

hndrte_dev_t *
hndrte_get_dev(char *name)
{
	hndrte_dev_t *dev;

	/* Loop through each dev, looking for a match */
	for (dev = dev_list; dev != NULL; dev = dev->next)
		if (strcmp(dev->name, name) == 0)
			break;

	return dev;
}

#ifdef HNDRTE_POLLING
/** run the poll routines for all devices for interfaces, it means isrs */
static void
hndrte_dev_poll(void)
{
	hndrte_dev_t *dev = dev_list;

	/* Loop through each dev's isr routine until one of them claims the interrupt */
	while (dev) {
		/* call isr routine if one is registered */
		if (dev->funcs->poll)
			dev->funcs->poll(dev);

		dev = dev->next;
	}
}

#else	/* !HNDRTE_POLLING */

/*
 * ======HNDRTE====== ISR support:
 *
 * hndrte_add_isr(action, irq, coreid, isr, cbdata): Add a ISR
 * hndrte_isr(): run the interrupt service routines for all devices
 */
/* ISR instance */
typedef struct hndrte_irq_action hndrte_irq_action_t;

struct hndrte_irq_action {
	hndrte_irq_action_t *next;
	isr_fun_t isr;
	void *cbdata;
	uint32 sbtpsflag;
};

static hndrte_irq_action_t *action_list = NULL;
static uint32 action_flags = 0;

/* irq is for future use */
int
BCMATTACHFN(hndrte_add_isr)(uint irq, uint coreid, uint unit,
                          isr_fun_t isr, void *cbdata, uint bus)
{
	void	*regs = NULL;
	uint	origidx;
	hndrte_irq_action_t *action;

	if ((action = hndrte_malloc(sizeof(hndrte_irq_action_t))) == NULL) {
		RTE_MSG(("hndrte_add_isr: hndrte_malloc failed\n"));
		return BCME_NOMEM;
	}

	origidx = si_coreidx(hndrte_sih);
	if (bus == SI_BUS)
		regs = si_setcore(hndrte_sih, coreid, unit);
#ifdef SBPCI
	else if (bus == PCI_BUS)
		regs = si_setcore(hndrte_sih, PCI_CORE_ID, 0);
#endif // endif
	BCM_REFERENCE(regs);
	ASSERT(regs);

	action->sbtpsflag = 1 << si_flag(hndrte_sih);
#ifdef REROUTE_OOBINT
	if (coreid == PMU_CORE_ID) {
		pmuregs_t *pmu = (pmuregs_t *)regs;
		pmu->pmuintmask0 = 1;
	}
#endif // endif
#ifdef BCM_OL_DEV
	if (coreid == D11_CORE_ID)
		action->sbtpsflag = 1 << si_flag_alt(hndrte_sih);
#endif // endif
	action->isr = isr;
	action->cbdata = cbdata;
	action->next = action_list;
	action_list = action;
	action_flags |= action->sbtpsflag;

	/* restore core original idx */
	si_setcoreidx(hndrte_sih, origidx);

	return BCME_OK;
}

/* add a specific isr that doesn't belong to any core */
int
BCMATTACHFN(hndrte_add_isr_n)(uint irq, uint isr_num,
                          isr_fun_t isr, void *cbdata)
{
	hndrte_irq_action_t *action;

	if ((action = hndrte_malloc(sizeof(hndrte_irq_action_t))) == NULL) {
		RTE_MSG(("hndrte_add_isr_n: hndrte_malloc failed\n"));
		return BCME_NOMEM;
	}

	action->sbtpsflag = 1 << isr_num;
	action->isr = isr;
	action->cbdata = cbdata;
	action->next = action_list;
	action_list = action;
	action_flags |= action->sbtpsflag;

	return BCME_OK;
}

/**
 * Generic isr handler. Handles interrupts generated by the hardware cores, amongst this the timer
 * interrupt that RTE uses to maintain its linked list of (software based) timers.
 */
void
hndrte_isr(void)
{
	hndrte_irq_action_t *action;
	uint32 sbflagst;
	int timeouts_ran = 1;
#ifdef BCMDBG_CPU
		/* Keep the values that could be used for computation after the ISR
		 * Computation at the end of ISR is profiled and happens to consume 60 cycles in
		 * case of armcm3
			 */
		uint32 start_time = 0;
		uint32 current_time = 0;
		uint32 totalcpu_cycles = 0;
		uint32 usedcpu_cycles = 0;
		uint32 cpuwait_cycles = 0;
	again:
		start_time = get_arm_inttimer();
		set_arm_inttimer(enterwaittime);
#else
	again:
#endif // endif

	/* Loop until nothing happens giving priority to interrupts */
	BUZZZ_LVL1(HNDRTE_ISR, 0);

	while ((sbflagst = si_intflag(hndrte_sih) & action_flags) ||
	       timeouts_ran) {
		/* Loop through each registered isr routine until one of
		 * them claims the interrupt
		 */
		action = action_list;
		while (sbflagst && action) {
			if (sbflagst & action->sbtpsflag) {
				sbflagst &= ~action->sbtpsflag;
				if (action->isr) {
					BUZZZ_LVL2(HNDRTE_ISR_ACTION, 1, (uint32)action->isr);
					(action->isr)(action->cbdata);
					BUZZZ_LVL2(HNDRTE_ISR_ACTION_RTN, 0);
				}
			}

			action = action->next;
		}

		timeouts_ran = run_timeouts();
	}

	/*
	 * Schedule the next timer interrupt here.  Do this once per interrupt instead of the old
	 * version which could schedule it each time a new timer is added. Note that the first item
	 * in the linked list of timers is always empty except for its '->next' value. Also note
	 * that the linked list is sorted on expiration time.
	 */
	if (timers && timers->next) {
		/* set hardware timer to generate an interrupt for the timer that expires first. */
		hndrte_set_irq_timer(timers->next->ms);
	}

#ifdef BCMDBG_CPU
	/* get total cpu cycles */
	totalcpu_cycles = (start_time == 0)
		? 0 : (enterwaittime - start_time);
	cpu_stats.totalcpu_cycles += totalcpu_cycles;

	/* get used cpu cycles */
	current_time = get_arm_inttimer();
	usedcpu_cycles = (current_time == 0)
		? 0 : (enterwaittime - current_time);
	cpu_stats.usedcpu_cycles += usedcpu_cycles;

	/* get sleep cpu cycle */
	cpuwait_cycles = (cpu_stats.last == 0)
	   ? 0 : (cpu_stats.last - start_time);
	cpu_stats.cpusleep_cycles += cpuwait_cycles;

	/* update last cpu usage time */
	cpu_stats.last = current_time;

	if (cpu_stats.num_wfi_hit == 0) {
		cpu_stats.min_cpusleep_cycles = cpuwait_cycles;
		cpu_stats.max_cpusleep_cycles = cpuwait_cycles;
	}

	/* update min max cycles in sleep state */
	cpu_stats.min_cpusleep_cycles =
		((cpuwait_cycles < cpu_stats.min_cpusleep_cycles))
		? cpuwait_cycles : cpu_stats.min_cpusleep_cycles;
	cpu_stats.max_cpusleep_cycles =
		((cpuwait_cycles > cpu_stats.max_cpusleep_cycles))
		? cpuwait_cycles : cpu_stats.max_cpusleep_cycles;
	cpu_stats.num_wfi_hit++;
#endif /* BCMDBG_CPU */

	/* Still has zero-length timer? Go back to the start of isr */
	if (timers && timers->next && timers->next->ms == 0) {
		goto again;
	}

	BUZZZ_LVL2(HNDRTE_ISR_RTN, 0);
}
#endif	/* !HNDRTE_POLLING */

/* Support for 32bit pktptr to 16bit pktid for memory conservation */
#if defined(BCMPKTIDMAP)
/*
 * Prior to constructing packet pools, a packet ID to packet PTR mapping service
 * must be initialized.
 * Instantiate a hierarchical multiword bit map for 16bit unique pktid allocator
 * Instantiate a reverse pktid to pktptr map.
 */
static void
BCMATTACHFN(hndrte_pktid_init)(osl_t * hndrte_osh, uint32 pktids_total)
{
	uint32 mapsz;
	pktids_total += 1; /* pktid 0 is reserved */

	ASSERT(PKT_MAXIMUM_ID < 0xFFFF);
	ASSERT(pktids_total <= PKT_MAXIMUM_ID);
	ASSERT((uint16)(BCM_MWBMAP_INVALID_IDX) == PKT_INVALID_ID);

	/* Instantiate a hierarchical multiword bitmap for unique pktid allocator */
	hndrte_pktid_map = bcm_mwbmap_init(hndrte_osh, pktids_total);
	if (hndrte_pktid_map == (struct bcm_mwbmap *)NULL) {
		ASSERT(0);
		return;
	}

	/* Instantiate a pktid to pointer associative array */
	mapsz = sizeof(struct lbuf *) * pktids_total;
	if ((hndrte_pktptr_map = (struct lbuf **)hndrte_malloc(mapsz)) == NULL) {
		ASSERT(0);
		goto error;
	}
	bzero(hndrte_pktptr_map, sizeof(struct lbuf *) * pktids_total);

	/* reserve pktid #0 and setup mapping of pktid#0 to NULL pktptr */
	ASSERT(PKT_NULL_ID == (uint16)0);
	bcm_mwbmap_force(hndrte_pktid_map, PKT_NULL_ID);

	ASSERT(!bcm_mwbmap_isfree(hndrte_pktid_map, PKT_NULL_ID));
	hndrte_pktptr_map[PKT_NULL_ID] = (struct lbuf *)(NULL);

	/* pktid to pktptr mapping successfully setup, with pktid#0 reserved */
	hndrte_pktid_max = pktids_total;

	return;

error:
	bcm_mwbmap_fini(hndrte_osh, hndrte_pktid_map);
	hndrte_pktid_max = 0U;
	hndrte_pktid_map = (struct bcm_mwbmap *)NULL;
	return;
}

void /* Increment pktid alloc failure count */
hndrte_pktid_inc_fail_cnt(void)
{
	hndrte_pktid_failure_count++;
}

uint32 /* Fetch total number of pktid alloc failures */
hndrte_pktid_fail_cnt(void)
{
	return hndrte_pktid_failure_count;
}

uint32 /* Fetch total number of free pktids */
hndrte_pktid_free_cnt(void)
{
	return bcm_mwbmap_free_cnt(hndrte_pktid_map);
}

/* Allocate a unique pktid and associate the pktptr to it */
uint16
hndrte_pktid_allocate(const struct lbuf * pktptr)
{
	uint32 pktid;

	pktid = bcm_mwbmap_alloc(hndrte_pktid_map); /* allocate unique id */

	ASSERT(pktid < hndrte_pktid_max);
	if (pktid < hndrte_pktid_max) { /* valid unique id allocated */
		ASSERT(pktid != 0U);
		/* map pktptr @ pktid */
		hndrte_pktptr_map[pktid] = (struct lbuf *)pktptr;
	}

	return (uint16)(pktid);
}

/* Release a previously allocated unique pktid */
void
hndrte_pktid_release(const struct lbuf * pktptr, const uint16 pktid)
{
	ASSERT(pktid != 0U);
	ASSERT(pktid < hndrte_pktid_max);
	ASSERT(hndrte_pktptr_map[pktid] == (struct lbuf *)pktptr);

	hndrte_pktptr_map[pktid] = (struct lbuf *)NULL; /* unmap pktptr @ pktid */
	/* BCMDBG:
	 * hndrte_pktptr_map[pktid] = (struct lbuf *) (0xdead0000 | pktid);
	 */
	bcm_mwbmap_free(hndrte_pktid_map, (uint32)pktid);
}

bool
hndrte_pktid_sane(const struct lbuf * pktptr)
{
	int insane = 0;
	uint16 pktid = 0;
	struct lbuf * lb = (struct lbuf *)(pktptr);

	pktid = PKTID(lb);

	insane |= pktid >= hndrte_pktid_max;
	insane |= (hndrte_pktptr_map[pktid] != lb);

	if (insane) {
		ASSERT(pktid < hndrte_pktid_max);
		ASSERT(hndrte_pktptr_map[pktid] == lb);
		RTE_MSG(("hndrte_pktid_sane pktptr<%p> pktid<%u>\n", pktptr, pktid));
	}

	return (!insane);
}
#endif /*   BCMPKTIDMAP */

#ifdef BCMPCIEDEV_ENABLED
static void
BCMATTACHFN(hndrte_fetch_module_init)(void)
{
	fetch_info = hndrte_malloc(sizeof(struct fetch_module_info));
	if (fetch_info == NULL) {
		RTE_MSG(("%s: Unable to init fetch module info!\n", __FUNCTION__));
		return;
	}
	bzero(fetch_info, sizeof(struct fetch_module_info));

	fetch_rqst_q = hndrte_malloc(sizeof(struct fetch_rqst_q_t));
	if (fetch_rqst_q == NULL) {
		RTE_MSG(("%s : Unable to initialize fetch_rqst_q!\n", __FUNCTION__));
		hndrte_free(fetch_info);
		return;
	}
	bzero(fetch_rqst_q, sizeof(struct fetch_rqst_q_t));
	fetch_info->pool = SHARED_POOL;
}
#endif /* BCMPCIEDEV_ENABLED */

#if defined(BCMPKTPOOL) && defined(BCMPKTPOOL_ENABLED)
static void
BCMATTACHFN(hndrte_pktpool_init)(void)
{
	int n;

	/* Construct a packet pool registry before initializing packet pools */
	n = pktpool_attach(hndrte_osh, PKTPOOL_MAXIMUM_ID);
	if (n != PKTPOOL_MAXIMUM_ID) {
		ASSERT(0);
		return;
	}

	pktpool_shared = hndrte_malloc(sizeof(pktpool_t));
	if (pktpool_shared == NULL) {
		ASSERT(0);
		goto error1;
	}
	bzero(pktpool_shared, sizeof(pktpool_t));

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
	pktpool_shared_lfrag = hndrte_malloc(sizeof(pktpool_t));
	if (pktpool_shared_lfrag == NULL) {
		ASSERT(0);
		goto error2;
	}
	bzero(pktpool_shared_lfrag, sizeof(pktpool_t));
#ifdef BCM_DHDHDR
	d3_lfrag_buf_pool = hndrte_malloc(sizeof(lfrag_buf_pool_t) * LFRAG_BUF_POOL_NUM);
	if (d3_lfrag_buf_pool == NULL) {
		ASSERT(0);
		goto error3;
	}
	d11_lfrag_buf_pool = d3_lfrag_buf_pool + 1;
#endif /* BCM_DHDHDR */
#endif /* BCMFRAGPOOL && !BCMFRAGPOOL_DISABLED */

#if defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)
	pktpool_shared_rxlfrag = hndrte_malloc(sizeof(pktpool_t));
	if (pktpool_shared_rxlfrag == NULL) {
		ASSERT(0);
		goto error3;
		return;
	}
	bzero(pktpool_shared_rxlfrag, sizeof(pktpool_t));
#endif // endif

	/*
	 * At this early stage, there's not enough memory to allocate all
	 * requested pkts in the shared pool.  Need to add to the pool
	 * after reclaim
	 *
	 * n = NRXBUFPOST + SDPCMD_RXBUFS;
	 *
	 * Initialization of packet pools may fail (BCME_ERROR), if the packet pool
	 * registry is not initialized or the registry is depleted.
	 *
	 * A BCME_NOMEM error only indicates that the requested number of packets
	 * were not filled into the pool.
	 */
	n = 1;
	if (pktpool_init(hndrte_osh, pktpool_shared,
	                 &n, PKTBUFSZ, FALSE, lbuf_basic) == BCME_ERROR) {
		ASSERT(0);
		goto error4;
	}
	pktpool_setmaxlen(pktpool_shared, SHARED_POOL_LEN);

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
#ifdef BCM_DHDHDR
	/* tx_lfrag header initialization */
	n = 1;
	if (pktpool_init(hndrte_osh, pktpool_shared_lfrag, &n, 0, TRUE, lbuf_frag) == BCME_ERROR) {
		ASSERT(0);
		goto error5;
	}
	pktpool_setmaxlen(pktpool_shared_lfrag, SHARED_FRAG_POOL_LEN);

	/* D3_BUFFER pool initialization */
	n = 1;
	if (lfbufpool_init(hndrte_osh, d3_lfrag_buf_pool, &n, D3LFBUF_SZ) == BCME_ERROR) {
		ASSERT(0);
		goto error6;
	}
	lfbufpool_setmaxlen(d3_lfrag_buf_pool, D3_BUFFER_LEN);

	/* D11_BUFFER pool initialization */
	n = 1;
	if (lfbufpool_init(hndrte_osh, d11_lfrag_buf_pool, &n, PKTFRAGSZ) == BCME_ERROR) {
		ASSERT(0);
		goto error6;
	}
	lfbufpool_setmaxlen(d11_lfrag_buf_pool, D11_BUFFER_LEN);
#else /* BCM_DHDHDR */

	n = 1;
	if (pktpool_init(hndrte_osh, pktpool_shared_lfrag,
	                 &n, PKTFRAGSZ, TRUE, lbuf_frag) == BCME_ERROR) {
		ASSERT(0);
		goto error5;
	}
	pktpool_setmaxlen(pktpool_shared_lfrag, SHARED_FRAG_POOL_LEN);
#endif /* BCM_DHDHDR */
#endif /* BCMFRAGPOOL && !BCMFRAGPOOL_DISABLED */
#if defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)
	n = 1;
	if (pktpool_init(hndrte_osh, pktpool_shared_rxlfrag,
		&n, PKTRXFRAGSZ, TRUE, lbuf_rxfrag) == BCME_ERROR) {
		ASSERT(0);
		goto error6;
	}
	pktpool_setmaxlen(pktpool_shared_rxlfrag, SHARED_RXFRAG_POOL_LEN);
#endif // endif

	return;
#if (defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)) || defined(BCM_DHDHDR)
error6:
	pktpool_deinit(hndrte_osh, pktpool_shared_lfrag);
#ifdef BCM_DHDHDR
	if (d3_lfrag_buf_pool)
		lfbufpool_deinit(hndrte_osh, d3_lfrag_buf_pool);
	if (d11_lfrag_buf_pool)
		lfbufpool_deinit(hndrte_osh, d11_lfrag_buf_pool);
#endif /* BCM_DHDHDR */
#endif /* (BCMRXFRAGPOOL && !BCMRXFRAGPOOL_DISABLED) ||
	* (BCM_DHDHDR)
	*/

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
error5:
	pktpool_deinit(hndrte_osh, pktpool_shared);
#endif // endif

error4:
#if (defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)) || defined(BCM_DHDHDR)
	hndrte_free(pktpool_shared_rxlfrag);
	pktpool_shared_rxlfrag = (pktpool_t *)NULL;
error3:
#endif /* BCMRXFRAGPOOL */

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
	hndrte_free(pktpool_shared_lfrag);
	pktpool_shared_lfrag = (pktpool_t *)NULL;
#ifdef BCM_DHDHDR
	if (d3_lfrag_buf_pool) {
		hndrte_free(d3_lfrag_buf_pool);
		d3_lfrag_buf_pool = (lfrag_buf_pool_t *)NULL;
		d11_lfrag_buf_pool = (lfrag_buf_pool_t *)NULL;
	}
#endif /* BCM_DHDHDR */
error2:
#endif /* BCMFRAGPOOL */
	hndrte_free(pktpool_shared);
	pktpool_shared = (pktpool_t *)NULL;

error1:
	pktpool_dettach(hndrte_osh);
}
#endif /* BCMPKTPOOL && BCMPKTPOOL_ENABLED */

/*
 * ======HNDRTE======  Initialize and background:
 *
 *	hndrte_init: Initialize the world.
 *	hndrte_poll: Run background work once.
 *	hndrte_idle: Run background work forever.
 */

#ifdef _HNDRTE_SIM_
extern uchar *sdrambuf;
uint32 _memsize = RAMSZ;
#endif // endif

#if defined(BCMPCIEDEV_ENABLED)
pciedev_shared_t pciedev_shared = { };
#endif // endif
#if defined(BCMSDIODEV_ENABLED)
sdpcm_shared_t sdpcm_shared = { };
#endif // endif

#if defined(BCMM2MDEV_ENABLED)
m2m_shared_t m2m_shared = { };
#endif // endif

uint32 gFWID;

#ifdef   __ARM_ARCH_7R__
extern uint32 _rambottom;
#endif // endif
#if defined(BCMPCIEDEV_ENABLED)
void
BCMATTACHFN(hndrte_shared_init)(void)
{

	uint32 asm_stk_bottom = 0;
#ifndef	__ARM_ARCH_7R__
	asm_stk_bottom = _memsize;
#else
	asm_stk_bottom = _rambottom;
#endif // endif
#if defined(__arm__)	/* Pointer supported only in startarm.S */
	*(uint32*)(asm_stk_bottom - 4) = (uint32)&pciedev_shared;
#endif // endif
}
#endif /* BCMPCIEDEV_ENABLED */
void *
BCMATTACHFN(hndrte_init)(void)
{
	uint32 stackBottom = 0xdeaddead;
	uchar *ramStart, *ramLimit;
	uint32 asm_stk_bottom = 0;

	BCM_REFERENCE(asm_stk_bottom);
#if defined(BCM_OL_DEV) || defined(BCMSDIODEV_ENABLED) || defined(BCMM2MDEV_ENABLED)
#ifndef  __ARM_ARCH_7R__
	asm_stk_bottom = _memsize;
#else
	asm_stk_bottom = _rambottom;
#endif /* __ARM_ARCH_7R__ */

#endif	/* BCM_OL_DEV || BCMSDIODEV_ENABLED || BCMM2MDEV_ENABLED */
#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
	asm_stk_bottom = _rambottom;

	struct trx_header *trx;
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */

#if defined(BCMSDIODEV_ENABLED)
	uint32 assert_line = sdpcm_shared.assert_line;
#elif defined(BCMPCIEDEV_ENABLED)
	uint32 assert_line = pciedev_shared.assert_line;
#elif defined(BCMM2MDEV_ENABLED)
	uint32 assert_line = m2m_shared.assert_line;
#endif // endif

#if defined(BCMPCIEDEV_ENABLED)
	/* Initialize the structure shared between the host and dongle
	 * over the PCIE bus.  This structure is used for console output
	 * and trap/assert information to the host.  The last word in
	 * memory is overwritten with a pointer to the shared structure.
	 */
	memset(&pciedev_shared, 0, sizeof(pciedev_shared) - 4);
	pciedev_shared.flags = PCIE_SHARED_VERSION;
	pciedev_shared.assert_line = assert_line;
	pciedev_shared.fwid = gFWID;

#if defined(BCMDBG_ASSERT)
	pciedev_shared.flags |= PCIE_SHARED_ASSERT_BUILT;
#endif // endif
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	pciedev_shared.flags |= PCIE_SHARED_TXPUSH_SPRT;
#endif // endif
#endif /* BCMPCIEDEV_ENABLED */

#if defined(BCMSDIODEV_ENABLED)
	/* Initialize the structure shared between the host and dongle
	 * over the SDIO bus.  This structure is used for console output
	 * and trap/assert information to the host.  The last word in
	 * memory is overwritten with a pointer to the shared structure.
	 */

	memset(&sdpcm_shared, 0, sizeof(sdpcm_shared));
	sdpcm_shared.flags = SDPCM_SHARED_VERSION;
	sdpcm_shared.assert_line = assert_line;
	sdpcm_shared.fwid = gFWID;
#if defined(BCMDBG_ASSERT)
	sdpcm_shared.flags |= SDPCM_SHARED_ASSERT_BUILT;
#endif // endif
#if defined(__arm__)	/* Pointer supported only in startarm.S */
	*(uint32*)(asm_stk_bottom - 4) = (uint32)&sdpcm_shared;
#endif // endif
#endif /* BCMSDIODEV_ENABLED */

#if defined(BCM_OL_DEV)
	/* Initialize the structure shared between the host and dongle
	 * over the PCIE  bus.  This structure is used for read host message buffer
	 * and trap/assert information to the host.  The last word in
	 * memory is overwritten with a pointer to the shared structure.
	 */
	 tramsz = asm_stk_bottom;

#if defined(__arm__)	/* Pointer supported only in startarm.S */
	ppcie_shared = (olmsg_shared_info_t *)(asm_stk_bottom - OLMSG_SHARED_INFO_SZ);
	memset((void *)&ppcie_shared->stats, 0, sizeof(olmsg_dump_stats));

#endif // endif

#endif /* BCM_OL_DEV */

#if defined(BCMM2MDEV_ENABLED)
	/* Initialize the structure shared between the apps and wlan
	 * over the M2M bus.  This structure is used for console output
	 * and trap/assert information to the host.  The last word in
	 * memory is overwritten with a pointer to the shared structure.
	 */

	memset(&m2m_shared, 0, sizeof(m2m_shared));
	m2m_shared.flags = M2M_SHARED_VERSION;
	m2m_shared.assert_line = assert_line;
	m2m_shared.fwid = gFWID;
#if defined(BCMDBG_ASSERT)
	m2m_shared.flags |= M2M_SHARED_ASSERT_BUILT;
#endif // endif
#if defined(__arm__)	/* Pointer supported only in startarm.S */
	*(uint32*)(asm_stk_bottom - 4) = (uint32)&m2m_shared;
#endif // endif
#endif /* BCMM2MDEV_ENABLED */

	/* Initialize trap handling */
	hndrte_trap_init();

	/* Initialize malloc arena */
#if defined(EXT_CBALL)
	ramStart = (uchar *)RAMADDRESS;
	ramLimit = ramStart + RAMSZ;
#elif defined(_HNDRTE_SIM_)
	ramStart = sdrambuf;
	ramLimit = ramStart + RAMSZ;
#elif defined(DEVRAM_REMAP)
{
	extern char _heap_start[], _heap_end[];
	ramStart = (uchar *)_heap_start;
	ramLimit = (uchar *)_heap_end;
}
#else
	ramStart = (uchar *)_end;
#if defined(BCMUSBDEV) && defined(BCMUSBDEV_ENABLED) && defined(BCMTRXV2) && \
	!defined(BCM_BOOTLOADER)
	/* Check for NVRAM parameters.
	 * If found, initialize _vars and _varsz. Also, update ramStart
	 * last 4 bytes at the end of RAM is left untouched.
	 */
	trx = (struct trx_header *) (asm_stk_bottom -(SIZEOF_TRXHDR_V2 + 4));
	/* sanity checks */
	if (trx->magic == TRX_MAGIC && ISTRX_V2(trx) && trx->offsets[TRX_OFFSETS_NVM_LEN_IDX]) {
		_varsz = trx->offsets[TRX_OFFSETS_NVM_LEN_IDX];
		_vars = (char *)(text_start + trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]);
		/* overriding ramStart initialization */
		ramStart = (uchar *)_vars + trx->offsets[TRX_OFFSETS_NVM_LEN_IDX] +
			trx->offsets[TRX_OFFSETS_DSG_LEN_IDX] +
			trx->offsets[TRX_OFFSETS_CFG_LEN_IDX];
	}
#endif /* BCMUSBDEV && BCMUSBDEV_ENABLED && BCMTRXV2 && !BCM_BOOTLOADER */

	ramLimit = (uchar *)&stackBottom - HNDRTE_STACK_SIZE;
#endif /* !EXT_CBALL && !_HNDRTE_SIM_ && !DEVRAM_REMAP */
	hndrte_arena_init((uintptr)ramStart,
	                  (uintptr)ramLimit,
	                  ((uintptr)&stackBottom) - HNDRTE_STACK_SIZE);

#ifdef HNDLBUFCOMPACT
	hndrte_lbuf_fixup_2M_tcm();
#endif // endif

	hndrte_disable_interrupts();

#if defined(BCM_OL_DEV) || defined(HNDRTE_CONSOLE)
	/* Create a logbuf separately from a console. This logbuf will be
	 * dumped and reused when the console is created later on.
	 */
	hndrte_log_init();
#endif // endif

	/* Initialise the debug struct that sits in a known location */
	hndrte_debug_info_init();

#ifdef HNDRTE_PT_GIANT
	mem_pt_init();
#endif // endif

#ifdef STACK_PROT_TRAP
	hndrte_stack_prot(((uchar *) &stackBottom) - HNDRTE_STACK_SIZE);
#endif // endif

	/* Now that we have initialized memory management let's allocate the osh */
	hndrte_osh = osl_attach(NULL);
	ASSERT(hndrte_osh);

#ifdef	__ARM_ARCH_7R__
	enable_arm_cyclecount();
#endif // endif

	/* Scan backplane */
	hndrte_sih = si_kattach(hndrte_osh);
	ASSERT(hndrte_sih);

	/* Initialize chipcommon related stuff */
	hndrte_chipc_init(hndrte_sih);

	/* Initialize timer */
	hndrte_timers_init();

	/* Initialize CPU related stuff */
	hndrte_cpu_init(hndrte_sih);

#ifdef HNDRTE_CONSOLE
	/* No printf's go to the UART earlier than this */
#ifndef BCM4350_FPGA
	hndrte_cons_init(hndrte_sih);
#endif // endif
#endif /* HNDRTE_CONSOLE */

#ifdef BCMDBG_CPU
	cu_timer = hndrte_init_timer((void *)hndrte_sih, NULL, hndrte_updatetimer, NULL);
#endif // endif
#if defined(BCMDBG) && defined(HNDRTE_CONSOLE) && defined(BCMPCIEDEV_ENABLED)
	pktfetch_stats_timer = hndrte_init_timer((void*)hndrte_sih, NULL,
		hndrte_update_pktfetch_stats, NULL);
#endif /* BCMDBG && HNDRTE_CONSOLE */
	/* Add a few commands */
#if (defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)) && !defined(BCM_BOOTLOADER)
#ifdef BCM_OL_DEV
	hndrte_cons_addcmd("?", (cons_fun_t)hndrte_cons_showcmds, 0);
#endif /* BCM_OL_DEV */
	hndrte_cons_addcmd("mu", (cons_fun_t)hndrte_print_memuse, 0);
	hndrte_cons_addcmd("dump", (cons_fun_t)hndrte_dump_test, 2);
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV && !BCM_BOOTLOADER */

#if defined(HNDRTE_CONSOLE) && !defined(BCM_BOOTLOADER)
	hndrte_cons_addcmd("mw", (cons_fun_t)hndrte_print_memwaste, 0);
	hndrte_cons_addcmd("md", (cons_fun_t)hndrte_memdmp, 0);
#ifdef BCMDBG_POOL
	hndrte_cons_addcmd("d", (cons_fun_t)hndrte_pool_notify, 0);
	hndrte_cons_addcmd("p", (cons_fun_t)hndrte_pool_dump, 0);
#endif // endif
#ifdef BCMDBG_MEM
	hndrte_cons_addcmd("ar", (cons_fun_t)hndrte_print_malloc, 0);
#endif // endif
#ifdef BCMDBG
	hndrte_cons_addcmd("tim", hndrte_print_timers, 0);
#endif // endif
#ifdef BCMTRAPTEST
	hndrte_cons_addcmd("tr", (cons_fun_t)traptest, 0);
#endif // endif
#ifdef BCMDBG_LOADAVG
	hndrte_cons_addcmd("lat", (cons_fun_t)hndrte_loadavg_test, 0);
#endif // endif
#ifdef BCMDBG_CPU
	hndrte_cons_addcmd("cu", (cons_fun_t)hndrte_print_cpuuse, (uint32)hndrte_sih);
#endif // endif
#if defined(BCMDBG) && defined(BCMPCIEDEV_ENABLED) && defined(HNDRTE_CONSOLE)
	hndrte_cons_addcmd("pfstats", (cons_fun_t)hndrte_dump_pktfetch_stats, NULL);
#endif // endif
#ifdef BCM_VUSBD
	hndrte_cons_addcmd("cusbstats", (cons_fun_t)hndrte_print_vusbstats, 0);
#endif // endif
#endif /* HNDRTE_CONSOLE  && ! BCM_BOOTLOADER */

#ifdef	SBPCI
	/* Init pci core if there is one */
	hndrte_init_pci((void *)hndrte_sih);
#endif // endif

#ifdef BCMECICOEX
	/* Initialize ECI registers */
	hndrte_eci_init(hndrte_sih);
#endif // endif

#ifdef WLGPIOHLR
	/* Initialize GPIO */
	hndrte_gpio_init(hndrte_sih);
#endif // endif
	hndrte_gci_init(hndrte_sih);
#if defined(BCMPKTIDMAP)
	/*
	 * Initialize the pktid to pktptr map prior to constructing pktpools,
	 * As part of constructing the pktpools a few packets will ne allocated
	 * an placed into the pools. Each of these packets must have a packet ID.
	 */
	hndrte_pktid_init(hndrte_osh, PKT_MAXIMUM_ID - 1);
#endif // endif

#if defined(BCMPKTPOOL) && defined(BCMPKTPOOL_ENABLED)
	hndrte_pktpool_init();
#endif // endif
#ifdef BCMPCIEDEV_ENABLED
	hndrte_fetch_module_init();
	hndrte_pktfetch_module_init();
#endif // endif

	return ((void *)hndrte_sih);
}

void
hndrte_poll(si_t *sih)
{
#ifdef HNDRTE_POLLING
	(void) run_timeouts();
	hndrte_dev_poll();
#else
	hndrte_wait_irq(sih);
#endif /* HNDRTE_POLLING */
}

void
hndrte_idle(si_t *sih)
{
#ifndef HNDRTE_POLLING
	hndrte_set_irq_timer(0);	/* kick start timer interrupts driven by hardware */
	hndrte_enable_interrupts();
#endif // endif
	hndrte_idle_init(sih);
#ifdef BCMDBG_CPU
	set_arm_inttimer(enterwaittime);
#endif // endif
	while (TRUE) {
#ifdef ATE_BUILD
		wl_ate_cmd_proc();
#else
		hndrte_poll(sih);
#endif // endif
	}
}

void
hndrte_unimpl(void)
{
	printf("UNIMPL: ra=%p\n", __builtin_return_address(0));
	hndrte_die();
}

/* ======HNDRTE====== misc
 *     assert
 *     chipc init
 *     gpio init
 */

#if defined(BCMDBG_ASSERT)

void
hndrte_assert(const char *file, int line)
{
	/* Format of ASSERT message standardized for automated parsing */
	printf("ASSERT in file %s line %d (ra %p, fa %p)\n",
	       file, line,
	       __builtin_return_address(0), __builtin_frame_address(0));

#ifdef BCMDBG_ASSERT_TYPE
	if (g_assert_type != 0)
		return;
#endif // endif

#ifdef BCMDBG_ASSERT

#if defined(BCMSDIODEV_ENABLED)
	/* Fill in structure that be downloaded by the host */
	sdpcm_shared.flags           |= SDPCM_SHARED_ASSERT;
	sdpcm_shared.assert_exp_addr  = 0;
	sdpcm_shared.assert_file_addr = (uint32)file;
	sdpcm_shared.assert_line      = (uint32)line;
#endif /* BCMSDIODEV_ENABLED */
#if defined(PCIEDEV_ENABLED)
	/* Fill in structure that be downloaded by the host */
	pciedev_shared.flags           |= PCIE_SHARED_ASSERT;
	pciedev_shared.assert_exp_addr  = 0;
	pciedev_shared.assert_file_addr = (uint32)file;
	pciedev_shared.assert_line      = (uint32)line;
#endif /* BCMSDIODEV_ENABLED */

#if defined(BCMM2MDEV_ENABLED)
	/* Fill in structure that be downloaded by the host */
	m2m_shared.flags           |= M2M_SHARED_ASSERT;
	m2m_shared.assert_exp_addr  = 0;
	m2m_shared.assert_file_addr = (uint32)file;
	m2m_shared.assert_line      = (uint32)line;
#endif /* BCMM2MDEV_ENABLED */
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__)
	asm volatile("SVC #0");
#endif // endif

	hndrte_die();

#endif /* BCMDBG_ASSERT */
}
#endif // endif

static void
hndrte_chipc_isr(hndrte_dev_t *dev)
{
	si_cc_isr(hndrte_sih, hndrte_ccr);
}

#ifdef HNDRTE_POLLING
static void *
BCMATTACHFN(hndrte_chipc_probe)(hndrte_dev_t *dev, void *regs, uint bus,
                                uint16 device, uint coreid, uint unit)
{
	return regs;
}

static hndrte_devfuncs_t chipc_funcs = {
	probe:		hndrte_chipc_probe,
	poll:		hndrte_chipc_isr
};

static hndrte_dev_t chipc_dev = {
	name:		"cc",
	funcs:		&chipc_funcs
};
#endif	/* HNDRTE_POLLING */

static void
BCMATTACHFN(hndrte_chipc_init)(si_t *sih)
{
	/* get chipcommon and its sbconfig addr */
	hndrte_ccr = si_setcoreidx(sih, SI_CC_IDX);
	/* only support chips that have chipcommon */
	ASSERT(hndrte_ccr);
	hndrte_ccsbr = (sbconfig_t *)((ulong)hndrte_ccr + SBCONFIGOFF);

	/* get pmu addr */
	if (AOB_ENAB(sih)) {
		hndrte_pmur = si_setcoreidx(sih, si_findcoreidx(sih, PMU_CORE_ID, 0));
		ASSERT(hndrte_pmur);
		/* Restore to CC */
		si_setcoreidx(sih, SI_CC_IDX);
	}

	/* register polling dev or isr */
#ifdef HNDRTE_POLLING
	hndrte_add_device(&chipc_dev, CC_CORE_ID, BCM4710_DEVICE_ID);
#else
	if (hndrte_add_isr(0, CC_CORE_ID, 0, (isr_fun_t)hndrte_chipc_isr, NULL, SI_BUS) != BCME_OK)
		hndrte_die(__LINE__);
#endif	/* !HNDRTE_POLLING */
}

#ifdef WLGPIOHLR
static void
hndrte_gpio_isr(void* cbdata, uint32 ccintst)
{
#ifdef ATE_BUILD
	ate_params.cmd_proceed = TRUE;
#else
	si_t *sih = (si_t *)cbdata;
	si_gpio_handler_process(sih);
#endif // endif
}

static void
BCMATTACHFN(hndrte_gpio_init)(si_t *sih)
{
	if (sih->ccrev < 11)
		return;
	si_cc_register_isr(sih, hndrte_gpio_isr, CI_GPIO, (void *)sih);
	si_gpio_int_enable(sih, TRUE);
}
#endif	/* WLGPIOHLR */

static void
hndrte_gci_isr(void* cbdata, uint32 ccintst)
{
	si_t *sih = (si_t *)cbdata;
	si_gci_handler_process(sih);
}

static void
BCMATTACHFN(hndrte_gci_init)(si_t *sih)
{
	if (!(sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT))
		return;

	si_gci_uart_init(sih, hndrte_osh, 0);

	si_cc_register_isr(sih, hndrte_gci_isr, CI_ECI, (void *)sih);
}

#ifdef BCMECICOEX
static void
BCMATTACHFN(hndrte_eci_init)(si_t *sih)
{
	if (sih->ccrev < 21)
		return;
	si_eci_init(sih);
}
#endif	/* BCMECICOEX */

#ifdef BCMDBG_CPU
void
hndrte_update_stats(hndrte_cpu_stats_t *cpustats)
{
	cpustats->totalcpu_cycles = cpu_stats.totalcpu_cycles;
	cpustats->usedcpu_cycles = cpu_stats.usedcpu_cycles;
	cpustats->cpusleep_cycles = cpu_stats.cpusleep_cycles;
	cpustats->min_cpusleep_cycles = cpu_stats.min_cpusleep_cycles;
	cpustats->max_cpusleep_cycles = cpu_stats.max_cpusleep_cycles;
	cpustats->num_wfi_hit = cpu_stats.num_wfi_hit;

	/* clean it off */
	bzero(&cpu_stats, sizeof(cpu_stats));
}

void hndrte_clear_stats(void)	/* Require separate routine for only stats clearance? */
{
	bzero(&cpu_stats, sizeof(cpu_stats));
}
#endif /* BCMDBG_CPU */

static void
BCMATTACHFN(hndrte_debug_info_init)(void)
{
	memset(&hndrte_debug_info, 0, sizeof(hndrte_debug_info));

	/* Initialize the debug area */
	hndrte_debug_info.magic = HNDRTE_DEBUG_MAGIC;
	hndrte_debug_info.version = HNDRTE_DEBUG_VERSION;
	strcpy(hndrte_debug_info.epivers, EPI_VERSION_STR);

#if defined(BCM_OL_DEV) || defined(HNDRTE_CONSOLE)
	hndrte_debug_info.console = hndrte_get_active_cons_state();
#endif /* BCM_OL_DEV || HNDRTE_CONSOLE */

#if defined(RAMBASE)
	hndrte_debug_info.ram_base = RAMBASE;
	hndrte_debug_info.ram_size = RAMSIZE;
#endif // endif

#ifdef ROMBASE
	hndrte_debug_info.rom_base = ROMBASE;
	hndrte_debug_info.rom_size = ROMEND-ROMBASE;
#else
	hndrte_debug_info.rom_base = 0;
	hndrte_debug_info.rom_size = 0;
#endif /* ROMBASE */

	hndrte_debug_info.event_log_top = NULL;

#if defined(BCMSDIODEV_ENABLED)
	hndrte_debug_info.fwid = sdpcm_shared.fwid;
#else
	hndrte_debug_info.fwid = 0;
#endif // endif
}
void
hndrte_update_shared_struct(void * sh)
{
#if defined(BCMPCIEDEV_ENABLED)
	*(uint32*)sh = (uint32)&pciedev_shared;
#endif // endif
}

hndrte_ltrsend_cb_info_t hndrte_gen_ltr;

static hndrte_ltrsend_cb_info_t*
BCMRAMFN(hndrte_gen_ltr_get)(void)
{
	return (&hndrte_gen_ltr);
}

int
hndrte_register_ltrsend_callback(hndrte_ltrsend_cb_t rtn, void *arg)
{
	hndrte_ltrsend_cb_info_t* ltrsend_cb_infoptr;

	ltrsend_cb_infoptr = hndrte_gen_ltr_get();

	if (ltrsend_cb_infoptr->cb != NULL) {
		printf("LTR gen callback is already registered 0x%04x, arg is 0x%04x\n",
			(uint32)(ltrsend_cb_infoptr->cb), (uint32)(ltrsend_cb_infoptr->arg));
		return -1;
	}
	ltrsend_cb_infoptr->cb = rtn;
	ltrsend_cb_infoptr->arg = arg;
	return 0;
}

int
hndrte_set_ltrstate(uint8 coreidx, uint8 state)
{
	hndrte_ltrsend_cb_info_t* ltrsend_cb_infoptr;

	ltrsend_cb_infoptr = hndrte_gen_ltr_get();

	if (ltrsend_cb_infoptr->cb != NULL)
		ltrsend_cb_infoptr ->cb(ltrsend_cb_infoptr->arg, state);
	return 0;
}

hndrte_devpwrstchg_cb_info_t hndrte_devpwrstschg;

static hndrte_devpwrstchg_cb_info_t*
BCMRAMFN(hndrte_devpwrstschg_get)(void)
{
	return (&hndrte_devpwrstschg);
}

int
hndrte_register_devpwrstchg_callback(hndrte_devpwrstchg_cb_t rtn, void *arg)
{
	hndrte_devpwrstchg_cb_info_t *devpwrstchg_cb_info_ptr;

	devpwrstchg_cb_info_ptr = hndrte_devpwrstschg_get();

	if (devpwrstchg_cb_info_ptr->cb != NULL) {
		RTE_MSG(("device pwrstatechage callback is already  0x%04x, arg is 0x%04x\n",
			(uint32)devpwrstchg_cb_info_ptr->cb, (uint32)devpwrstchg_cb_info_ptr->arg));
		return -1;
	}
	devpwrstchg_cb_info_ptr->cb = rtn;
	devpwrstchg_cb_info_ptr->arg = arg;
	return 0;
}

int
hndrte_notify_devpwrstchg(bool hostmem_acccess_enabled)
{
	hndrte_devpwrstchg_cb_info_t *devpwrstchg_cb_info_ptr;

	devpwrstchg_cb_info_ptr = hndrte_devpwrstschg_get();

	if (devpwrstchg_cb_info_ptr->cb != NULL)
		devpwrstchg_cb_info_ptr->cb(devpwrstchg_cb_info_ptr->arg, hostmem_acccess_enabled);
	return 0;
}

hndrte_generate_pme_cb_info_t hndrte_generatepme = {NULL, NULL};

static hndrte_generate_pme_cb_info_t*
BCMRAMFN(hndrte_generatepme_info_get)(void)
{
	return (&hndrte_generatepme);
}

int
hndrte_register_generate_pme_callback(hndrte_generate_pme_cb_t rtn, void *arg)
{
	hndrte_generate_pme_cb_info_t *generatepme_cb_info_ptr;

	generatepme_cb_info_ptr = hndrte_generatepme_info_get();

	/* ASSERT if multiple call backs are registered */
	ASSERT(generatepme_cb_info_ptr->cb == NULL);

	if (generatepme_cb_info_ptr->cb != NULL) {
		RTE_MSG(("generatepme callback is already  0x%04x, arg is 0x%04x\n",
			(uint32)generatepme_cb_info_ptr->cb, (uint32)generatepme_cb_info_ptr->arg));
		return -1;
	}
	generatepme_cb_info_ptr->cb = rtn;
	generatepme_cb_info_ptr->arg = arg;
	return 0;
}

int
hndrte_generate_pme(bool pme_state)
{
	hndrte_generate_pme_cb_info_t *generatepme_cb_info_ptr;

	generatepme_cb_info_ptr = hndrte_generatepme_info_get();
	/* invoke call back to toggle PME# */
	if (generatepme_cb_info_ptr->cb != NULL)
		generatepme_cb_info_ptr->cb(generatepme_cb_info_ptr->arg, pme_state);
	return 0;
}

hndrte_rx_reorder_qeueue_flush_cb_info_t rx_reorderqueue_flush = {NULL, NULL};

static hndrte_rx_reorder_qeueue_flush_cb_info_t *
BCMRAMFN(hndrte_rxreorder_queue_flush_get)(void)
{
	return (&rx_reorderqueue_flush);
}

int
hndrte_register_rxreorder_queue_flush_cb(hndrte_rx_reorder_qeueue_flush_cb_t rtn, void *arg)
{
	hndrte_rx_reorder_qeueue_flush_cb_info_t *rx_reorderqueue_flush_ptr;

	rx_reorderqueue_flush_ptr = hndrte_rxreorder_queue_flush_get();

	if (rx_reorderqueue_flush_ptr->cb != NULL) {
		RTE_MSG(("rxreorder queue callback is already  0x%04x, arg is 0x%04x\n",
			(uint32)rx_reorderqueue_flush_ptr->cb,
			(uint32)rx_reorderqueue_flush_ptr->arg));
		return -1;
	}
	rx_reorderqueue_flush_ptr->cb = rtn;
	rx_reorderqueue_flush_ptr->arg = arg;
	return 0;
}

int
hndrte_flush_rxreorderqeue(uint16 start, uint32 cnt)
{
	hndrte_rx_reorder_qeueue_flush_cb_info_t *rx_reorderqueue_flush_ptr;

	rx_reorderqueue_flush_ptr = hndrte_rxreorder_queue_flush_get();

	if (rx_reorderqueue_flush_ptr->cb != NULL)
		rx_reorderqueue_flush_ptr->cb(rx_reorderqueue_flush_ptr->arg, start, cnt);
	return 0;
}

/* API for cases where the device need to be out of deep sleep to make the host access better */
void
hndrte_disable_deepsleep(bool force_disable)
{
	/* this could be done in different ways for now using the ARM to control it */
	si_arm_disable_deepsleep(force_disable);
}

void*
BCMATTACHFN(hndrte_enable_gci_gpioint)(uint8 gpio, uint8 sts, gci_gpio_handler_t hdlr, void *arg)
{
	return (si_gci_gpioint_handler_register(hndrte_sih, gpio, sts, hdlr, arg));
}

#ifdef BCMDBG_CPU
static void hndrte_print_cpuuse(uint32 arg)
{
	si_t *sih = (si_t *)arg;
	printf("CPU stats to be displayed in %d msecs\n", DEFUPDATEDELAY);

	/* Force HT on */
	si_clkctl_cc(sih, CLK_FAST);

	/* Schedule timer for DEFUPDATEDELAY ms */
	hndrte_add_timer(cu_timer, DEFUPDATEDELAY, 0);

	/* Clear stats and restart counters */
	hndrte_clear_stats();
}

static void hndrte_updatetimer(hndrte_timer_t *t)
{
	si_t *sih = (si_t *)t->context;
	hndrte_cpu_stats_t cpustats;

	hndrte_update_stats(&cpustats);

	/* Disable FORCE HT, which was enabled at hndrte_print_cpuuse */
	si_clkctl_cc(sih, CLK_DYNAMIC);

	printf("Total cpu cycles : %u\n"
			"Used cpu cycles : %u\n"
			"Total sleep cycles (+.05%%): %u\n"
			"Average sleep cycles: %u\n"
			"Min sleep cycles: %u, Max sleep cycles: %u\n"
			"Total number of wfi hit %u\n",
			cpustats.totalcpu_cycles,
			cpustats.usedcpu_cycles,
			cpustats.cpusleep_cycles,
			cpustats.cpusleep_cycles/cpustats.num_wfi_hit,
			cpustats.min_cpusleep_cycles, cpustats.max_cpusleep_cycles,
			cpustats.num_wfi_hit);
}
#endif	/* BCMDBG_CPU */

#ifdef BCMPCIEDEV_ENABLED
/* HostFetch and PktFetch modules for PCIe Full Dongle */
#if defined(BCMDBG) && defined(HNDRTE_CONSOLE)
static void hndrte_dump_pktfetch_stats(uint32 arg)
{
	hndrte_add_timer(pktfetch_stats_timer, PKTFETCH_STATS_DELAY, 0);
	bzero(&pktfetch_stats, sizeof(pktfetch_debug_stats_t));
}
static void hndrte_update_pktfetch_stats(hndrte_timer_t *t)
{
	printf("pktfetch_rqsts_rcvd = %d\n", pktfetch_stats.pktfetch_rqsts_rcvd);
	printf("pktfetch_rqsts_rejected = %d\n", pktfetch_stats.pktfetch_rqsts_rejected);
	printf("pktfetch_rqsts_enqued = %d\n", pktfetch_stats.pktfetch_rqsts_enqued);
	printf("pktfetch_dispatch_failed_nomem = %d\n",
		pktfetch_stats.pktfetch_dispatch_failed_nomem);
	printf("pktfetch_dispatch_failed_no_fetch_rqst = %d\n",
		pktfetch_stats.pktfetch_dispatch_failed_no_fetch_rqst);
	printf("pktfetch_succesfully_dispatched = %d\n",
		pktfetch_stats.pktfetch_succesfully_dispatched);
	printf("pktfetch_wake_pktfetchq_dispatches = %d\n",
		pktfetch_stats.pktfetch_wake_pktfetchq_dispatches);
	printf("average pktfetch Q size in %d secs = %d\n", PKTFETCH_STATS_DELAY,
	(pktfetch_stats.pktfetch_qsize_accumulator/pktfetch_stats.pktfetch_qsize_poll_count));
	bzero(&pktfetch_stats, sizeof(pktfetch_debug_stats_t));
}
#endif /* BCMDBG && HNDRTE_CONSOLE */

/* Description:
 * HostFetch module, a pcie layer callback is registered
 * to which the requests are dispatched for mem2mem DMA scheduling
 * Currently registered: pciedev_dispatch_fetch_rqst
 */
inline void
hndrte_fetch_bus_dispatch_cb_register(bus_dispatch_cb_t cb, void *arg)
{
	if (fetch_info == NULL) {
		RTE_MSG(("fetch info not inited! Cannot register callback!\n"));
		ASSERT(0);
		return;
	}
	fetch_info->cb = cb;
	fetch_info->arg = arg;
}
/* Description:
 * Primary HostFetch access API
 * Use cases requiring dispatch of a request should call this directly
 */
void
hndrte_fetch_rqst(struct fetch_rqst *fr)
{
	if (hndrte_fetch_rqstq_empty()) {
		if (hndrte_dispatch_fetch_rqst(fr) == BCME_OK)
			return;
		else
			DBG_BUS_INC(fetch_info, hndrte_fetch_rqst);
	}
	hndrte_enque_fetch_rqst(fr, FALSE);
}
/* Description:
 * The fetch_rqst_q is hndrte's backup queue for requests that
 * fail to dispatch successfully to bus layer
 */
static void
hndrte_enque_fetch_rqst(struct fetch_rqst *fr, bool enque_to_head)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;

	fr->next = NULL;

	if (frq->head == NULL)
		frq->head = frq->tail = fr;
	else {
		if (!enque_to_head) {
			frq->tail->next = fr;
			frq->tail = fr;
		} else {
			fr->next = frq->head;
			frq->head = fr;
		}
	}
}
/* Description:
 * Try to dispatch to pcie for mem2mem dma scheduling,
 * if failed, return appropriate error code
 */
static int
hndrte_dispatch_fetch_rqst(struct fetch_rqst *fr)
{
	int ret;

	/* Fetch module assumes a dest buffer is always allocated */
	ASSERT(fr->dest != NULL);

	/* Bus layer DMA dispatch */
	FETCH_RQST_FLAG_SET(fr, FETCH_RQST_IN_BUS_LAYER);
	if (fetch_info && fetch_info->cb)
		ret = fetch_info->cb(fr, fetch_info->arg);
	else {
		RTE_MSG(("fetch_info or fetch_info callback is NULL!\n"));
		ASSERT(0);
		DBG_BUS_INC(fetch_info, hndrte_dispatch_fetch_rqst);
		return BCME_ERROR;
	}

	return ret;
}
static struct fetch_rqst *
hndrte_deque_fetch_rqst(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;
	struct fetch_rqst *fr;

	if (frq->head == NULL)
		fr = NULL;
	else if (frq->head == frq->tail) {
		fr = frq->head;
		frq->head = NULL;
		frq->tail = NULL;
	} else {
		fr = frq->head;
		frq->head = frq->head->next;
		fr->next = NULL;
	}
	return fr;
}
bool
hndrte_fetch_rqstq_empty(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;

	return (frq->head == NULL);
}
/* Description:
 *
 */
void
hndrte_wake_fetch_rqstq(void)
{
	struct fetch_rqst *fr;
	int ret;

	/* While frq not empty */
	while (!hndrte_fetch_rqstq_empty())
	{
		/* Deque an fr */
		fr = hndrte_deque_fetch_rqst();
		ret = hndrte_dispatch_fetch_rqst(fr);

		if (ret != BCME_OK) {
			/* Reque fr to head of Q */
			hndrte_enque_fetch_rqst(fr, TRUE);
			break;
		}
	}
}
/* Description:
 * Used to flush all requests from the hndrte fetch_rqst_q
 */
void
hndrte_flush_fetch_rqstq(void)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;
	struct fetch_rqst *fr;

	while (frq->head != NULL) {
		fr = frq->head;
		frq->head = frq->head->next;
		if (fr->cb)
			fr->cb(fr, TRUE);
		else {
			RTE_MSG(("%s: No callback registered for fetch_rqst!\n", __FUNCTION__));
			ASSERT(0);
			DBG_BUS_INC(fetch_info, hndrte_flush_fetch_rqstq);
		}
	}
	frq->tail = NULL;
}
/* Description:
 * Generic callback invoked from the bus layer signifying
 * that a DMA descr was freed up / space available in the DMA queue
 */
void
hndrte_dmadesc_avail_cb(void)
{
	if ((fetch_rqst_q)->head != NULL)
		hndrte_wake_fetch_rqstq();
}
static struct fetch_rqst *
hndrte_remove_fetch_rqst_from_queue(struct fetch_rqst *fr)
{
	struct fetch_rqst_q_t *frq = fetch_rqst_q;
	struct fetch_rqst *cur_fr, *prev_fr;

	if (hndrte_fetch_rqstq_empty())
		goto not_found;

	/* Single element case */
	if (frq->head == frq->tail)
	{
		if (frq->head == fr) {
			frq->head = frq->tail = NULL;
			return fr;
		} else
			goto not_found;
	}

	/* Atleast 2 elements in queue */
	for (cur_fr = frq->head, prev_fr = NULL;
		cur_fr != NULL; prev_fr = cur_fr, cur_fr = cur_fr->next)
		{
			if (cur_fr == fr) {
				if (prev_fr == NULL)
					frq->head = cur_fr->next;
				else if (cur_fr == frq->tail) {
					prev_fr->next = NULL;
					frq->tail = prev_fr;
				} else
					prev_fr->next = cur_fr->next;
				return fr;
			}
		}

not_found:
	RTE_MSG(("%s: fetch_rqst not found in fetch_rqst queue!\n", __FUNCTION__));
	DBG_BUS_INC(fetch_info, hndrte_remove_fetch_rqst_from_queue);
	return NULL;
}
int
hndrte_cancel_fetch_rqst(struct fetch_rqst *fr)
{
	if (hndrte_remove_fetch_rqst_from_queue(fr)) {
		if (fr->cb)
			fr->cb(fr, TRUE);
		else {
			RTE_MSG(("%s: No callback registered for fetch_rqst!\n", __FUNCTION__));
			ASSERT(0);
			DBG_BUS_INC(fetch_info, hndrte_cancel_fetch_rqst);
		}
		return BCME_OK;
	} else if (FETCH_RQST_FLAG_GET(fr, FETCH_RQST_IN_BUS_LAYER)) {
		FETCH_RQST_FLAG_SET(fr, FETCH_RQST_CANCELLED);
		DBG_BUS_INC(fetch_info, hndrte_cancel_fetch_rqst);
		return BCME_NOTFOUND;
	}
	return BCME_ERROR;
}
/* PktFetch module */
void
BCMATTACHFN(hndrte_pktfetch_module_init)(void)
{
	/* Initialize the pktfetch queue */
	pktfetchq = hndrte_malloc(sizeof(struct pktfetch_q));
	if (pktfetchq == NULL) {
		RTE_MSG(("%s: Unable to alloc pktfetch queue: Out of mem?\n", __FUNCTION__));
		return;
	}
	bzero(pktfetchq, sizeof(struct pktfetch_q));

	/* Initialize the pktfetch rqstpool */
	pktfetchpool = hndrte_malloc(sizeof(struct pktfetch_rqstpool));
	if (pktfetchpool == NULL) {
		RTE_MSG(("%s: Unable to alloc rqstpool: Out of mem?\n", __FUNCTION__));
		hndrte_free(pktfetchq);
		return;
	}

	bzero(pktfetchpool, sizeof(struct pktfetch_rqstpool));

	pktfetch_lbuf_q = hndrte_malloc(sizeof(struct pktq));
	if (pktfetch_lbuf_q == NULL) {
		RTE_MSG(("%s: Unable to alloc pktfetch_lbuf_q: Out of mem?\n", __FUNCTION__));
		hndrte_free(pktfetchq);
		hndrte_free(pktfetchpool);
		return;
	}

	bzero(pktfetch_lbuf_q, sizeof(struct pktq));

	hndrte_pktfetchpool_init();

	/* Initialize the pktfetch lbuf Q */
	pktq_init(pktfetch_lbuf_q, 1, PKTFETCH_LBUF_QSIZE);
}
void
BCMATTACHFN(hndrte_pktfetchpool_init)(void)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;
	ASSERT(pktfetchpool);

	struct fetch_rqst *fr;
	int i;

	for (i = 0; i < PKTFETCH_MAX_FETCH_RQST; i++) {
		fr = hndrte_malloc(sizeof(struct fetch_rqst));
		if (fr == NULL) {
			RTE_MSG(("%s: Out of memory. Alloced only %d fetch_rqsts!\n",
				__FUNCTION__, pool->len));
			break;
		}
		bzero(fr, sizeof(struct fetch_rqst));
		if (pool->head == NULL)
			pool->head = pool->tail = fr;
		else {
			pool->tail->next = fr;
			pool->tail = fr;
		}
		pool->len++;
	}
	pool->max = pool->len;
}
int
hndrte_pktfetch(struct pktfetch_info *pinfo)
{
	int ret;

#ifdef BCMDBG
	if ((pktfetch_stats.pktfetch_rqsts_rcvd & (PKTFETCH_STATS_SKIP_CNT-1)) == 0) {
		pktfetch_stats.pktfetch_qsize_poll_count++;
		pktfetch_stats.pktfetch_qsize_accumulator += pktfetchq->count;
	}
	pktfetch_stats.pktfetch_rqsts_rcvd++;
#endif // endif
	if (pktfetchq->count >= PKTFETCH_MAX_OUTSTANDING) {
		/* check if we can dispatch any pending item from the queue first */
		hndrte_wake_pktfetchq();

		if (pktfetchq->count >= PKTFETCH_MAX_OUTSTANDING) {
#ifdef BCMDBG
			pktfetch_stats.pktfetch_rqsts_rejected++;
#endif // endif
			DBG_BUS_INC(fetch_info, hndrte_pktfetch);
			return BCME_ERROR;
		}
	}

	ASSERT(pinfo->lfrag);

	if (pktfetchq->head == NULL) {
		ret = hndrte_pktfetch_dispatch(pinfo);
		switch (ret) {
			case BCME_OK:
#ifdef BCMDBG
				pktfetch_stats.pktfetch_succesfully_dispatched++;
#endif // endif
				return BCME_OK;
			case BCME_NOMEM:
				pktfetchq->state = STOPPED_UNABLE_TO_GET_LBUF;
#ifdef BCMDBG
				pktfetch_stats.pktfetch_dispatch_failed_nomem++;
#endif // endif
				DBG_BUS_INC(fetch_info, hndrte_pktfetch);
				break;
			case BCME_NORESOURCE:
				pktfetchq->state = STOPPED_UNABLE_TO_GET_FETCH_RQST;
#ifdef BCMDBG
				pktfetch_stats.pktfetch_dispatch_failed_no_fetch_rqst++;
#endif // endif
				DBG_BUS_INC(fetch_info, hndrte_pktfetch);
				break;
			default:
				printf("Unrecognised return value from hndrte_pktfetch_dispatch\n");
		}
	}
	hndrte_enque_pktfetch(pinfo, FALSE);
#ifdef BCMDBG
	pktfetch_stats.pktfetch_rqsts_enqued++;
#endif // endif
	return BCME_OK;
}
static int
hndrte_pktfetch_dispatch(struct pktfetch_info *pinfo)
{
	struct fetch_rqst *fr;
	void *lfrag = pinfo->lfrag;
#ifdef BCM_DHDHDR
	void *heap_buf = NULL;
	uint32 header_len;
#endif /* BCM_DHDHDR */
	void *lbuf = NULL;
	uint32 totlen;
	struct pktfetch_module_ctx *ctx;
	osl_t *osh = pinfo->osh;

	fr = hndrte_pktfetchpool_get();

	if (fr == NULL) {
		DBG_BUS_INC(fetch_info, hndrte_pktfetch_dispatch);
		return BCME_NORESOURCE;
	}

	ASSERT(fetch_info->pool);

	if (!PKTISTXFRAG(osh, lfrag)) {
		lbuf = pktpool_get(fetch_info->pool);
	}

	/* If pool_get fails, but we have sufficient memory in the heap, let's alloc from heap */
	if ((lbuf == NULL) &&
	  (OSL_MEM_AVAIL() >
	  (PKTFETCH_MIN_MEM_FOR_HEAPALLOC + sizeof(struct pktfetch_module_ctx)))) {
#ifdef BCM_DHDHDR
		if (PKTISTXFRAG(osh, lfrag)) {
			heap_buf = hndrte_malloc(MAXPKTDATABUFSZ);
		} else
#endif /* BCM_DHDHDR */
		{
			lbuf = PKTALLOC(osh, MAXPKTDATABUFSZ, lbuf_basic);
		}
	}

	if ((lbuf == NULL) &&
#ifdef BCM_DHDHDR
		(heap_buf == NULL) &&
#endif /* BCM_DHDHDR */
		TRUE) {
		hndrte_pktfetchpool_reclaim(fr);
		DBG_BUS_INC(fetch_info, hndrte_pktfetch_dispatch);
		return BCME_NOMEM;
	}

	/* Unset Hi bit if High bit set */
	PHYSADDR64HISET(fr->haddr, (uint32)(PKTFRAGDATA_HI(hndrte_osh, lfrag, 1) & 0x7fffffff));

	/* User provides its own context and callback
	 * But for pktfetch module, callback needs to be hndrte_pktfetch_completion_handler(),
	 * so wrap original context and callback over fr->ctx,
	 * so that the fetch_rqst is returned to the pktfetch module
	 * after HostFetch is done with it (either fetch completed or cancelled)
	 * and pktfetch module takes over original cb/ctx handling. We assume
	 * this malloc (8 bytes) will ideally never fail.
	 * If it does, this failure case is handled the
	 * same way as failure for lbuf_get
	*/
	fr->cb = hndrte_pktfetch_completion_handler;
	ctx = hndrte_malloc(sizeof(struct pktfetch_module_ctx));
	if (ctx == NULL) {
		RTE_MSG(("%s: Unable to alloc pktfetch ctx!\n", __FUNCTION__));
		hndrte_pktfetchpool_reclaim(fr);
#ifdef BCM_DHDHDR
		if (heap_buf) {
			hndrte_free(heap_buf);
		} else
#endif /* BCM_DHDHDR */
		{
			PKTFREE(osh, lbuf, TRUE);
		}

		DBG_BUS_INC(fetch_info, hndrte_pktfetch_dispatch);
		return BCME_NOMEM;
	}
	ctx->cb = pinfo->cb;
	ctx->orig_ctx = pinfo->ctx;
	fr->ctx = (void *)ctx;

#ifdef BCM_DHDHDR
	if (heap_buf) {
		header_len = PKTLEN(osh, lfrag);
		bcopy((char*)PKTDATA(osh, lfrag), (char*)heap_buf + pinfo->headroom - header_len,
			header_len);
		PKTBUFEARLYFREE(osh, lfrag);
		PKTSETBUF(osh, lfrag, heap_buf, MAXPKTDATABUFSZ);
		PKTSETHEAPBUF(osh, lfrag);
		lbuf = lfrag;
	}
#endif /* BCM_DHDHDR */

	totlen = PKTFRAGTOTLEN(hndrte_osh, lfrag);
	totlen -= pinfo->host_offset;
	fr->size = totlen;

	totlen += pinfo->headroom;
	PKTSETLEN(hndrte_osh, lbuf, totlen);
	PKTPULL(hndrte_osh, lbuf, pinfo->headroom);
	PKTSETIFINDEX(hndrte_osh, lbuf, PKTIFINDEX(hndrte_osh, lfrag));
	fr->dest = PKTDATA(hndrte_osh, lbuf);
#ifdef BCM_DHDHDR
	if (heap_buf) {
		PKTPUSH(hndrte_osh, lbuf, header_len);
	}
#endif /* BCM_DHDHDR */
	PHYSADDR64LOSET(fr->haddr, (uint32)(PKTFRAGDATA_LO(hndrte_osh, lfrag, 1)
		+ pinfo->host_offset));
	/* NOTE: This will not fail */
	pktq_penq(pktfetch_lbuf_q, 0, lbuf);

	/* Store the lfrag pointer in the lbuf */
	PKTTAG_SET_VALUE(lbuf, lfrag);
	hndrte_fetch_rqst(fr);
	return BCME_OK;
}
static struct fetch_rqst *
hndrte_pktfetchpool_get(void)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;

	ASSERT(pktfetchpool);
	struct fetch_rqst *fr;
	if (pool->head == NULL) {
		ASSERT(pool->len == 0);
		return NULL;
	}
	else {
		fr = pool->head;
		pool->head = pool->head->next;
		pool->len--;
		fr->next = NULL;
	}
	return fr;
}
static void
hndrte_pktfetchpool_reclaim(struct fetch_rqst *fr)
{
	struct pktfetch_rqstpool *pool = pktfetchpool;
	ASSERT(pktfetchpool);
	bzero(fr, sizeof(struct fetch_rqst));

	if (pool->head == NULL) {
		ASSERT(pool->len == 0);
		pool->head = pool->tail = fr;
		pool->len++;
	}
	else {
		pool->tail->next = fr;
		pool->tail = fr;
		pool->len++;
	}
}
static void
hndrte_enque_pktfetch(struct pktfetch_info *pinfo, bool enque_to_head)
{
	struct pktfetch_q *pfq = pktfetchq;

	if (pfq->head == NULL)
		pfq->head = pfq->tail = pinfo;
	else {
		if (!enque_to_head) {
			pfq->tail->next = pinfo;
			pfq->tail = pinfo;
		}
		else {
			pinfo->next = pfq->head;
			pfq->head = pinfo;
		}
	}
	pfq->count++;
	pfq->tail->next = NULL;
}
static struct pktfetch_info *
hndrte_deque_pktfetch(void)
{
	struct pktfetch_q *pfq = pktfetchq;
	struct pktfetch_info *pinfo;

	if (pfq->head == NULL)
		return NULL;
	else if (pfq->head == pfq->tail) {
		pinfo = pfq->head;
		pfq->head = NULL;
		pfq->tail = NULL;
		pfq->count--;
	} else {
		pinfo = pfq->head;
		pfq->head = pinfo->next;
		pfq->count--;
	}
	pinfo->next = NULL;
	return pinfo;
}
void
hndrte_wake_pktfetchq(void)
{
	struct pktfetch_q *pfq = pktfetchq;
	struct pktfetch_info *pinfo;
	int ret;

	while (pfq->head != NULL) {
		pinfo = hndrte_deque_pktfetch();
		ret = hndrte_pktfetch_dispatch(pinfo);

		if (ret < 0) {
			/* Reque fr to head of Q and breakout */
			hndrte_enque_pktfetch(pinfo, TRUE);
			pfq->state = (ret == BCME_NORESOURCE ?
				STOPPED_UNABLE_TO_GET_FETCH_RQST : STOPPED_UNABLE_TO_GET_LBUF);
			return;
		}
#ifdef BCMDBG
		else
			pktfetch_stats.pktfetch_wake_pktfetchq_dispatches++;
#endif // endif
	}
	/* All requests dispatched. Reset Q state */
	pfq->state = INITED;
}
/* Callback for every PKTFREE from pktpool registered with the fetch_module */
void
hndrte_lb_free_cb(void)
{
	if (pktfetchq->state != INITED)
		hndrte_wake_pktfetchq();
}
/* After HostFetch module is done, the fetch_rqst is returned to pktfetch module
 * pktfetch module does necessary processing and the actual callback

 * This handler frees up the original lfrag
 * and returns the fr to user via the original user callback
 * Also frees up the intermediate pktfetch_module_ctx mallloced by pktfetch module
 * and returns the fetch_rqst to pktfetch_pool
 * NOTE: PKTFETCH User must never free fetch_rqst!!!
 */
static void
hndrte_pktfetch_completion_handler(struct fetch_rqst *fr, bool cancelled)
{
	struct pktfetch_module_ctx *ctx = fr->ctx;
	pktfetch_cmplt_cb_t pktfetch_cb = ctx->cb;
	void *orig_ctx = ctx->orig_ctx;
	void *lfrag, *lbuf;

	/* Free up the intermediate pktfetch context */
	hndrte_free(ctx);

	lbuf = pktq_pdeq(pktfetch_lbuf_q, 0);

	if (lbuf == NULL) {
		RTE_MSG(("%s: Expected lbuf not found in queue!\n", __FUNCTION__));
		ASSERT(0);
		DBG_BUS_INC(fetch_info, hndrte_pktfetch_completion_handler);
		return;
	}

	lfrag = (void *)PKTTAG_GET_VALUE(lbuf);
	PKTTAG_SET_VALUE(lbuf, 0);	/* Reset PKTTAG */

	hndrte_pktfetchpool_reclaim(fr);

	/* Do not free the original lfrag; Leave it to the user callback */
	if (pktfetch_cb)
		pktfetch_cb(lbuf, lfrag, orig_ctx, cancelled);
	else {
		RTE_MSG(("%s: No callback registered for pktfetch!\n", __FUNCTION__));
		ASSERT(0);
		DBG_BUS_INC(fetch_info, hndrte_pktfetch_completion_handler);
	}

	/* schedule new reqst from pktfetchq */
	if (pktfetchq->state != INITED)
		hndrte_wake_pktfetchq();
}

/* Flush out tx/rx pkts from bus layer at every wlc_dpc. */
/* Gives a chaining effect and reduces cpu cycles */
void
hndrte_register_cb_flush_chainedpkts(txrxstatus_glom_cb cb, void *ctx)
{
	/* Register callback function */
	 hndrte_glom_info.cb = cb;
	 hndrte_glom_info.ctx = ctx;
}
#endif /* BCMPCIEDEV_ENABLED */
void
hndrte_flush_glommed_txrxstatus(void)
{
#ifdef BCMPCIEDEV_ENABLED
	/* Invoke call back function */
	  if (hndrte_glom_info.cb)
		 hndrte_glom_info.cb(hndrte_glom_info.ctx);
#endif /* BCMPCIEDEV_ENABLED */
}
