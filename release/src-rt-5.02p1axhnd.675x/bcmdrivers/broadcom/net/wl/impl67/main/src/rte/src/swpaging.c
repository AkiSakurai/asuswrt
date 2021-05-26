/**
 * \file swpaging.c
 * \brief Software paging implementation for MMU based chipsets
 *
 * SW Paging mechanism allows some of the modules/functions/data sections to be
 * offloaded to host memory and paged in on demand.
 *
 * Modules and functions to be offloaded are selected using the
 * configuration file paging_config.txt
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
 * $Id$
 */

#include <swpaging.h>
#include <bcmutils.h>
#include <rte_mmu.h>
#include <rte_fetch.h>
#include <pciedev.h>
#include <bcmlibarm.h>
#include <pcie_core.h>
#include <bcmendian.h>
#include <bcmhme.h>

struct sw_paging sw_paging_g =
{
	.switch_to_dma = 0,
#ifdef SW_PAGING_DEBUG_BUILD
	.page_fault_log_state = PAGE_FAULT_LOG_DISABLE,
	.page_fault_log_mode = PAGE_FAULT_LOG_NOWRAP,
	.page_fault_log_cnt = 0,
#endif // endif
#ifdef SW_PAGING_STATS_BUILD
	.page_fault_cnt = 0,
	.dma_dur_min = 0xffffffff,
	.dma_dur_max = 0
#endif /* SW_PAGING_STATS_BUILD */
};

static uint32 get_free_physical_page(uint32 virt_addr,
		uint32 *evict_virt_addr);

/* One bit per page indicating page load status */
#define PAGE_STATUS_ARRAY_SIZE(num_pages) \
	((((num_pages) >> 5u) + 1u) * sizeof(*sw_paging_g.page_status))

/* Index of the page from virtual address base */
#define PAGE_INDEX(page_base, page_addr) (((page_addr) - (page_base)) / PAGE_SIZE)

/* non zero if page is loaded, zero otherwise */
#define IS_PAGE_LOADED(idx) (sw_paging_g.page_status[(idx) >> 5u] & (1u << ((idx) & 0x1Fu)))

/* Mark page load status to true */
#define SET_PAGE_LOADED(idx) (sw_paging_g.page_status[(idx) >> 5u] |= (1u << ((idx) & 0x1Fu)))

/* Mark page load status to false */
#define CLEAR_PAGE_LOADED(idx) (sw_paging_g.page_status[(idx) >> 5u] &= ~(1u << ((idx) & 0x1Fu)))

#define VIRT_ADDR_MASK (0xFFFu)
#define NULL_VIRT_ADDR (0)

/** Initialize SW paging module */
void BCMATTACHFN(sw_paging_init)(void)
{
	uint32 free_page_base, idx;
	uint32 page_history_cnt;
	uint32 *page_history;
	uint32 *page_status;
	uint32 *free_pages;

	/* Page usage history statistics */
	page_history_cnt = ((uint32)&pageable_end -
		(uint32)&pageable_start) / PAGE_SIZE;
	page_history = MALLOCZ(NULL, page_history_cnt * sizeof(uint32));

	/* Bit map of current loaded pages */
	page_status = MALLOCZ(NULL, PAGE_STATUS_ARRAY_SIZE(page_history_cnt));
	SW_PAGING_INFO(("sw_paging_init: page_status at %p\n", page_status));
	for (idx = 0; idx < PAGE_STATUS_ARRAY_SIZE(page_history_cnt)/sizeof(*page_status); idx++) {
		SW_PAGING_INFO(("page_status[%u] = %08x\n", idx, page_status[idx]));
	}

	/* Free page status */
	free_pages = MALLOC(NULL, NUM_PHY_PAGES * sizeof(uint32));
	SW_PAGING_INFO(("sw_paging_init: free_pages at %p\n", free_pages));

	if ((page_history == NULL) || (page_status == NULL) || (free_pages == NULL)) {
		OSL_SYS_HALT();
	}

	/* Initialize free pages with address of reserved pages */
	free_page_base = (uint32)&paging_reserved_base;
	for (idx = 0; idx < NUM_PHY_PAGES; idx++, free_page_base += PAGE_SIZE) {
		free_pages[idx] = free_page_base;
	}

	sw_paging_g.page_history_cnt = page_history_cnt;
	sw_paging_g.page_history = page_history;
	sw_paging_g.page_status = page_status;
	sw_paging_g.free_pages = free_pages;

#ifdef SW_PAGING_DEBUG_BUILD
	sw_paging_g.page_fault_log = MALLOCZ(NULL, PAGE_FAULT_LOG_NUM * sizeof(uint32));
	if (sw_paging_g.page_fault_log == NULL) {
		SW_PAGING_ERROR(("sw_paging_init: failed to allocate page_fault_log\n"));
	}
#endif // endif
	return;
}

/**
 * Function to find a free physical page.
 * @param virt_addr: Virtual address for which a physical page is requested
 * @param evict_virt_addr:
 *	Input  - Virtual page which should not be evicted to make free space
 *	Output - Virtual page evicted to make a free page
 * @return
 *	0 - If virtual address is already loaded
 *	Non-zero - Physical address available for loading the virtual page
 */
static uint32
get_free_physical_page(uint32 virt_addr, uint32 *evict_virt_addr)
{
	uint32 idx;

	uint32 phy_page;		/* Physical addr of the allocated page */
	uint32 page_idx;		/* Index of above physical page        */

	uint32 min_cnt;			/* Lowest load count amoung all pages  */
	uint32 cnt;			/* Temp variable                       */
	uint32 max_cnt;			/* Maximum load count amoung all pages  */

	uint32 virt_page_idx;		/* Idx of input virtual address        */
	uint32 virt_addr_4kb;		/* Input virtual addr, in units of 4KB */
	uint32 pageable_start_4kb;	/* Start of virtual address space      */

	uint32 in_use_virt_addr_4kb;	/* Virtual addr of Page to be evicted  */
	uint32 hist_idx;

	uint32 free_page_found = FALSE;
	uint32 *free_pages = sw_paging_g.free_pages;
	uint32 watchdog_pages_4kb;

	/* Index of the page corresponding to virtual page to be loaded */
	pageable_start_4kb = ((uint32)&pageable_start >> 12);
	virt_addr_4kb = (virt_addr >> 12);
	virt_page_idx = virt_addr_4kb - pageable_start_4kb;

	watchdog_pages_4kb = (uint32)&watchdog_start >> 12;

	min_cnt = 0xFFFFFFFFu;
	max_cnt = 0;
	page_idx = 0;

	/* Debug only.
	 * In some cases page fault is generated before MMU initialization
	 */
	if (free_pages == NULL) {
		OSL_SYS_HALT();
	}

	/* Iterate free page list looking for a free page.
	 * Also look for a allocated page with least usage count while iterating
	 * for free page list.
	 */
	/* TODO: Phase 2: switch to non iterative algorithms or run the search
	 * in idle task and keep the next candidates ready in advance.
	 */
	for (idx = 0; idx < NUM_PHY_PAGES; idx++) {
		/* Free pages entry format
		 * [12 - 31] - Physical address of reserved page
		 * [0  - 11] - Virtual address (4KB unit) mapped at this page
		 */
		phy_page = free_pages[idx];
		in_use_virt_addr_4kb = phy_page & VIRT_ADDR_MASK;

		/* No virtual address mapping indicates it is a free page */
		if (in_use_virt_addr_4kb == NULL_VIRT_ADDR) {
			page_idx = idx;
			free_page_found = TRUE;
			break;
		} else if (in_use_virt_addr_4kb < watchdog_pages_4kb) {
			/* Index in the history table corresponding to this virtual addr */
			hist_idx = in_use_virt_addr_4kb - pageable_start_4kb;

			/* Usage count of page corresponding to this virtual address */
			cnt = sw_paging_g.page_history[hist_idx];

			/* Record the max count to tirgger a page_history reset
			 * to avoid ping-pong problem after an interval
			 */
			if (cnt > max_cnt) {
				max_cnt = cnt;
			}

			/* Record a page with least usage count
			 * amoung currently located pages
			 */
			if (cnt < min_cnt) {
				/* Possible eviction candidate only if caller specifically
				 * wants to avoid eviction of this page.
				 */
				/* TODO: Avoid evicting least recently used page too */
				if (*evict_virt_addr != (in_use_virt_addr_4kb << 12)) {
					min_cnt = cnt;
					page_idx = idx;
				}
			}
		}
	}

#ifdef SW_PAGING_DEBUG_BUILD
	sw_paging_g.page_history_max = max_cnt;
	sw_paging_g.page_history_min = min_cnt;
#endif /* SW_PAGING_DEBUG_BUILD */
	if (max_cnt > PAGE_HISTORY_MAXCNT) {
		/* Reset paging history counts to avoid ping-pong */
		memset(sw_paging_g.page_history, 0, sw_paging_g.page_history_cnt * sizeof(uint32));
	}

	if (free_page_found == FALSE) {
		in_use_virt_addr_4kb = free_pages[page_idx] & VIRT_ADDR_MASK;
		*evict_virt_addr = in_use_virt_addr_4kb << 12;
	} else {
		*evict_virt_addr = 0;
	}

	/* Get address of page */
	phy_page = free_pages[page_idx] & ~VIRT_ADDR_MASK;

	/* Record new page mapping */
	free_pages[page_idx] = phy_page | virt_addr_4kb;

	/* Increment usage count */
	sw_paging_g.page_history[virt_page_idx]++;
	return phy_page;
}

#ifdef SW_PAGING_STATS_BUILD
/**
 * Utility API to log page fault statistics.
 * @param fault_addr: Address triggering page fault
 * @param evict_addr: Page evicted to make room in physical memory
 * @param dma_cycles: Total CPU cycles to fetch a page from host memory
 * @param total_cycles: Total CPU cycles to handle page fault
 * @return none
 */
static void
log_page_load_statistics(uint32 fault_addr, uint32 evict_addr,
	uint32 dma_cycles, uint32 total_cycles)
{
	uint32 log_idx;
	page_fault_history_t *page_fault_hist;

	log_idx = sw_paging_g.page_fault_cnt & (PAGE_FAULT_HIST_COUNT - 1);

	page_fault_hist = &sw_paging_g.page_fault_hist[log_idx];
	page_fault_hist->addr = fault_addr;
	page_fault_hist->evict_addr = evict_addr;
	page_fault_hist->total_dur = total_cycles;
	page_fault_hist->dma_dur = dma_cycles;
	page_fault_hist->tsf = OSL_SYSUPTIME_US();

	/* Record min and max DMA transfer duration */
	if (sw_paging_g.switch_to_dma) {
		if (dma_cycles < sw_paging_g.dma_dur_min) {
			sw_paging_g.dma_dur_min = dma_cycles;
		}

		if (dma_cycles > sw_paging_g.dma_dur_max) {
			sw_paging_g.dma_dur_max = dma_cycles;
		}
	}

	sw_paging_g.page_fault_cnt++;
}
#endif /* SW_PAGING_STATS_BUILD */

/**
 * Utility API to validate -
 *  - Fault address is in valid virtual address space
 *  - Virtual page corresponding to fault address is not already loaded
 *  - If memory access is crossing a page boundry and need to load next page
 * @param fault_addr: Pointer to address triggering page fault
 * @param org_fault_addr: Original fault address, same as fault_addr on entry
 * @return
 *  - BCME_OK - fault address is valid
 *              fault_addr may have been modified to load next page
 *  - BCME_RANGE - fault_addr is outside of valid virtual address space
 *  - BCME_BADADDR - Requested virtual page is already loaded
 */
static int
validate_fault_addr(uint32 *fault_addr, uint32 org_fault_addr)
{
	uint32 ret = BCME_OK;

	SW_PAGING_INFO(("validate_fault_addr: fault_addr %08x org_fault_addr %08x\n",
		*fault_addr, org_fault_addr));
	do {
		/* Validate fault address to be in valid virtual address space */
		if ((*fault_addr < (uint32)&pageable_start) ||
			(*fault_addr > (uint32)&pageable_end)) {
			ret = BCME_RANGE;
			break;
		}

		/* Check if the page corresponding to fault address is already loaded */
		if (IS_PAGE_LOADED(PAGE_INDEX((uint32)&pageable_start, *fault_addr))) {
			SW_PAGING_INFO(("validate_fault_addr: fault_addr 0x%08x "
				"org_fault_addr 0x%08x\n",
				*fault_addr, org_fault_addr));
			/* Requested page is already loaded.
			 * Probably current fault is on an access crossing the page boundary.
			 * Request loading of next page if current page fault is
			 * around page boundary
			 */
			*fault_addr += 64u;
			if (org_fault_addr == (*fault_addr & ~(PAGE_SIZE - 1u))) {
				/* fault_addr + cache_line_size is still in the same page.
				 * Which means page is already loaded but still got fault,
				 * probably access permissions are violated.
				 */
				ret = BCME_BADADDR;
			}
		} else {
		    /* Page is not loaded, ok to load the requested page */
		    break;
		}
	} while (TRUE);

	return ret;
}

/**
 * API to handle the page fault.
 * @param fault_addr: Address corresponding to access violation.
 * @return
 *  - BCME_OK - page loaded successfully
 *  - BCME_RANGE - fault_addr is outside of valid virtual address space
 *  - BCME_BADADDR - Requested virtual page is already loaded
 */
int
sw_paging_handle_page_fault(uint32 fault_addr)
{
	uint32 phy_addr;
	uint32 virt_addr;
	uint32 offset;
	uint32 evict_virt_addr;
	haddr64_t host_addr;
	uint32 host_addr_lo = host_page_info[0];
	uint32 host_addr_hi = host_page_info[1];
	int ret = BCME_OK;
#ifdef SW_PAGING_STATS_BUILD
	uint32 t1 = 0, t2 = 0, t3 = 0, t4 = 0;

	OSL_GETCYCLES(t1);
#endif /* SW_PAGING_STATS_BUILD */

	/* Page start address corresponding to fault address. */
	evict_virt_addr = fault_addr & ~(PAGE_SIZE-1);

	/* Validate fault address.
	 *  - Fault address is in valid virtual address space
	 *  - Virtual page corresponding to fault address is not already loaded
	 *  - If memory access is crossing a page boundry and need to load next page
	 */
	ret = validate_fault_addr(&fault_addr, evict_virt_addr);
	if (ret != BCME_OK) {
		goto end;
	}

	/* fault_addr may be modified in validate_fault_addr.
	 * Align to page start address
	 */
	virt_addr = fault_addr & ~(PAGE_SIZE-1);

	/* Get a free physical page block.
	 * If all pages are in use evict least frequently used block
	 */
	phy_addr = get_free_physical_page(virt_addr, &evict_virt_addr);

	/* Fetch page corresponding to virtual address from host memory */
	offset = virt_addr - (uint32)&pageable_start;
	host_addr_lo += offset;

#ifdef SW_PAGING_STATS_BUILD
	OSL_GETCYCLES(t2);
#endif // endif
	if (sw_paging_g.switch_to_dma == 0) {
		/* The 3rd 32MB sbtopcie window */
		host_addr_lo = (host_addr_lo & ~SBTOPCIE_32M_MASK) | CCI400_SBTOPCIE1_BASE;

		/* This page may be marked as NA or RO.
		 * Change permission to enable write to this location
		 */
		HND_MMU_SET_L2_PAGE_ATTRIBUTES(phy_addr, phy_addr, PAGE_SIZE,
			L2S_PAGE_ATTR_NOEXEC_WR_MEM, 1);

		rte_memcpy32((void *)phy_addr, (void *)host_addr_lo, (PAGE_SIZE/32));
	} else {
		SW_PAGING_INFO(("%s: fault_addr 0x%08x virt_addr 0x%08x host_addr_lo 0x%08x\n",
			__FUNCTION__, fault_addr, virt_addr, host_addr_lo));
		/* Use HME/BME to copy the page */
		HADDR64_LO_SET(host_addr, host_addr_lo);
		HADDR64_HI_SET(host_addr, host_addr_hi);

		ret = hme_h2d_xfer(&host_addr, (void *)phy_addr, PAGE_SIZE, TRUE,
			HME_USER_NOOP);
		if (ret < 0) {
			SW_PAGING_ERROR(("%s: hme_h2d_xfer failed\n", __FUNCTION__));
			goto end;
		} else {
			ret = BCME_OK;
		}
	}
#ifdef SW_PAGING_STATS_BUILD
	OSL_GETCYCLES(t3);
#endif // endif

	/* Make physical address corresponding to this page as NA */
	HND_MMU_SET_L2_PAGE_ATTRIBUTES(phy_addr, phy_addr, PAGE_SIZE,
		L2S_PAGE_ATTR_NO_ACC, 0);

	if (evict_virt_addr) {
		/* Mark evicted virtual address space as NA */
		HND_MMU_SET_L2_PAGE_ATTRIBUTES(evict_virt_addr, phy_addr, PAGE_SIZE,
			L2S_PAGE_ATTR_NO_ACC, 0);
		CLEAR_PAGE_LOADED(PAGE_INDEX((uint32)&pageable_start, evict_virt_addr));
	}

	/* Mark virtual address space as RO + Executable */
	HND_MMU_SET_L2_PAGE_ATTRIBUTES(virt_addr, phy_addr, PAGE_SIZE,
		L2S_PAGE_ATTR_EXEC_NOWR, 1);
	SET_PAGE_LOADED(PAGE_INDEX((uint32)&pageable_start, virt_addr));
	SW_PAGING_INFO(("After SET_PAGE_LOADED: sw_paging_g.page_status[%u] = %08x\n",
		PAGE_INDEX((uint32)&pageable_start, virt_addr) >> 5,
		sw_paging_g.page_status[PAGE_INDEX((uint32)&pageable_start, virt_addr) >> 5]));

end:
#ifdef SW_PAGING_DEBUG_BUILD
	if (sw_paging_g.page_fault_log_state == PAGE_FAULT_LOG_ENABLE) {
		uint32 idx;
		if (sw_paging_g.page_fault_log_mode == PAGE_FAULT_LOG_WRAP) {
			/* Wrapping */
			idx = sw_paging_g.page_fault_log_cnt & (PAGE_FAULT_LOG_NUM - 1);
			sw_paging_g.page_fault_log[idx] = fault_addr;
			sw_paging_g.page_fault_log_cnt++;
		} else {
			/* No Wrapping */
			idx = sw_paging_g.page_fault_log_cnt;
			if (idx < PAGE_FAULT_LOG_NUM) {
				sw_paging_g.page_fault_log[idx] = fault_addr;
				sw_paging_g.page_fault_log_cnt++;
			}
		}
	}
#endif /* SW_PAGING_DEBUG_BUILD */
#ifdef SW_PAGING_STATS_BUILD
	OSL_GETCYCLES(t4);
	log_page_load_statistics(fault_addr, evict_virt_addr, (t3 - t2), (t4 - t1));
#endif /* SW_PAGING_STATS_BUILD */
	if (ret) {
		SW_PAGING_ERROR(("%s: fault_addr 0x%08x evict_virt_addr 0x%08x, return %d\n",
			__FUNCTION__, fault_addr, evict_virt_addr, ret));
	}
	return ret;
}

/**
 * DMA channel is initialized.
 * From this point DMA will be used to download pages from host memory.
 */
void
sw_paging_dma_ready_indication(void)
{
	sw_paging_g.switch_to_dma = TRUE;
}

#ifdef SW_PAGING_DEBUG_BUILD
void
sw_paging_log_start(void)
{
	/* Do not start logging in case the log buf is not allocated properly */
	if (sw_paging_g.page_fault_log == NULL) {
		printf("sw_paging_log_start: log buffer not allocated\n");
		return;
	}
	sw_paging_g.page_fault_log_cnt = 0;
	sw_paging_g.page_fault_log_state = PAGE_FAULT_LOG_ENABLE;
	printf("sw_paging_log_start: mode<%u>\n", sw_paging_g.page_fault_log_mode);
}

void
sw_paging_log_stop(void)
{
	sw_paging_g.page_fault_log_state = PAGE_FAULT_LOG_DISABLE;
	printf("sw_paging_log_stop: mode<%u> cnt<%u>\n",
		sw_paging_g.page_fault_log_mode,
		sw_paging_g.page_fault_log_cnt);
}

void
sw_paging_log_clear(void)
{
	sw_paging_g.page_fault_log_cnt = 0;
	printf("sw_paging_log_clear: mode<%u>\n", sw_paging_g.page_fault_log_mode);
}

void
sw_paging_log_dump(void)
{
	int start, end, idx;
	uint32 cnt = sw_paging_g.page_fault_log_cnt;

	printf("sw_paging_log_dump: mode<%u> cnt<%u>\n",
		sw_paging_g.page_fault_log_mode,
		cnt);
	if (cnt == 0) {
		printf("<Log is empty>\n");
		return;
	}

	if ((sw_paging_g.page_fault_log_mode == PAGE_FAULT_LOG_NOWRAP) ||
		(cnt <= PAGE_FAULT_LOG_NUM)) {
		/* wrapping not occurred */
		start = 0;
		end = cnt - 1;
	} else {
		/* wrapping occurred */
		start = cnt & (PAGE_FAULT_LOG_NUM - 1);
		end = (cnt - 1) & (PAGE_FAULT_LOG_NUM - 1);
	}

	if (start > end) {
		for (idx = start; idx < PAGE_FAULT_LOG_NUM; idx++) {
			printf("fn[@%08x]\n", sw_paging_g.page_fault_log[idx]);
		}
		for (idx = 0; idx <= end; idx++) {
			printf("fn[@%08x]\n", sw_paging_g.page_fault_log[idx]);
		}
	} else {
		/* start <= end */
		for (idx = start; idx <= end; idx++) {
			printf("fn[@%08x]\n", sw_paging_g.page_fault_log[idx]);
		}
	}
}

void
sw_paging_log_usage(void)
{
	printf("Usage: dhd -i <ifname> cons \"swp -[m<mode>|b|e|d]\"\n"
		"\t-m<mode>: mode 1 = nowrap (default), 2 = wrap\n"
		"\t-b: start logging\n"
		"\t-e: stop logging\n"
		"\t-c: clear the logs\n"
		"\t-d: dump the logs\n");

}

/* RTE cons utility for SW_PAGING logging */
void
hnd_cons_swp_dump(void *arg, int argc, char *argv[])
{
	int start = 0, stop = 0, dump = 0, clear = 0;
	char *p;
	int val;

	printf("page_history max_cnt<%u> min_cnt<%u>\n",
		sw_paging_g.page_history_max,
		sw_paging_g.page_history_min);

	if (argc < 2) {
		goto usage;
		sw_paging_log_usage();
	}

	p = argv[1];
	if (!strncmp(p, "-", 1)) {
		switch (*++p) {
			case 'm':
				if (sw_paging_g.page_fault_log_state != PAGE_FAULT_LOG_DISABLE) {
					printf("Cannot change the mode while swp is logging\n");
					goto usage;
				}
				/* in case there is a space between 'm' and the value */
				if (*++p == '\0' && argc > 2) {
					p = argv[2];
				}
				val = bcm_strtoul(p, &p, 0);
				if (val != PAGE_FAULT_LOG_NOWRAP && val != PAGE_FAULT_LOG_WRAP) {
					goto usage;
				}
				if (sw_paging_g.page_fault_log_mode != val) {
					/* Clear the log to prevent the confusion for dump */
					sw_paging_g.page_fault_log_cnt = 0;
					sw_paging_g.page_fault_log_mode = val;
				}
				printf("swp logging mode<%u>\n", sw_paging_g.page_fault_log_mode);
				break;
			case 'b':
				if (sw_paging_g.page_fault_log_state != PAGE_FAULT_LOG_DISABLE) {
					printf("swp is already logging\n");
					goto usage;
				}
				start = 1;
				break;
			case 'e':
				if (sw_paging_g.page_fault_log_state == PAGE_FAULT_LOG_DISABLE) {
					printf("swp is not logging\n");
					goto usage;
				}
				stop = 1;
				break;
			case 'd':
				if (sw_paging_g.page_fault_log_state != PAGE_FAULT_LOG_DISABLE) {
					printf("Cannot dump the log while swp is logging\n");
					goto usage;
				}
				dump = 1;
				break;
			case 'c':
				if (sw_paging_g.page_fault_log_state != PAGE_FAULT_LOG_DISABLE) {
					printf("Cannot clear the log while swp is logging\n");
					goto usage;
				}
				clear = 1;
				break;
			default:
				goto usage;
		}
	} else {
		goto usage;
	}

	if (start) {
		sw_paging_log_start();
	} else if (stop) {
		sw_paging_log_stop();
	} else if (dump) {
		sw_paging_log_dump();
	} else if (clear) {
		sw_paging_log_clear();
	}

	return;
usage:
	sw_paging_log_usage();
}
#endif /* SW_PAGING_DEBUG_BUILD */
