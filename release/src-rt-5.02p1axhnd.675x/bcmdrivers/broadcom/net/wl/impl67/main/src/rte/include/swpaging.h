/**
 * \file swpaging.h
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
#ifndef __SWPAGING_H
#define __SWPAGING_H

#include <osl.h>

#define SW_PAGING_DEBUG_BUILD
//#define SW_PAGING_STATS_BUILD

#ifdef SW_PAGING_DEBUG_BUILD
#define SW_PAGING_ERROR(x)	printf x
#else
#define SW_PAGING_ERROR(x)
#endif /* SW_PAGING_DEBUG_BUILD */

#define SW_PAGING_INFO(x)

/* 16KB of page fault log buffer for 4K entries */
#define PAGE_FAULT_LOG_NUM	(4 * 1024)

enum {
	PAGE_FAULT_LOG_DISABLE = 0,
	PAGE_FAULT_LOG_ENABLE = 1
};

enum {
	PAGE_FAULT_LOG_NOWRAP = 1,
	PAGE_FAULT_LOG_WRAP = 2
};

#define PAGE_FAULT_HIST_COUNT	64u

/** Debug info per page fault */
typedef struct page_fault_history
{
	uint32 addr;			/** Page fault address */
	uint32 evict_addr;		/** Evicted page address */
	uint16 total_dur;		/** Total page-fault handling duration */
	uint16 dma_dur;			/** DMA transfer duration */
	uint32 tsf;			/** Fage fault TSF */
} page_fault_history_t;

/* Global variables for SW_Paging */
typedef struct sw_paging
{
	/**
	 * Page load statistics of each virtual page.
	 * This info is used to find least recently used page during page eviction.
	 */
	uint32 page_history_cnt;	/** Total number of virtual pages    */
	uint32 *page_history;		/** Page load count per virtual page */
	uint32 *page_status;		/** Bit map of current loaded pages  */
	/**
	 * free_pages is a pointer to an array of physical page info structure.
	 * Since physical pages are 4K aligned first 12bits are always zero
	 * and same can be used to save virtual addres mapped to this free page.
	 * Bits 0-11: Virtual address in units of 4kb.
	 * Bits 12-31: Upper 24 bits of physical page address.
	 */
	uint32 *free_pages;

	uint32 switch_to_dma;		/** Use SW memcopy till DMAs are setup */
#ifdef SW_PAGING_DEBUG_BUILD
	uint32 page_fault_log_state;	/** Page fault log state: disable, enable */
	uint32 page_fault_log_mode;	/** Page fault log mode: no-wrap, wrap */
	uint32 page_fault_log_cnt;	/** Number of page faults since logged */
	uint32 *page_fault_log;		/** Log buffer for page fault addresses */
	uint32 page_history_max;	/** max paging cnt with a virtual page */
	uint32 page_history_min;	/** min paging cnt with a virtual page */
#endif /* SW_PAGING_DEBUG_BUILD */

#ifdef SW_PAGING_STATS_BUILD
	/** Debug statistics */
	uint32 page_fault_cnt;		/** Total page faults */
	uint32 dma_dur_min;		/** Fastest DMA transfer duration */
	uint32 dma_dur_max;		/** Slowest DMA transfer duration */

	/** History of last PAGE_FAULT_HIST_COUNT number of page faults */
	page_fault_history_t page_fault_hist[PAGE_FAULT_HIST_COUNT];
#endif /* SW_PAGING_STATS_BUILD */
} sw_paging_t;

#define PAGE_HISTORY_MAXCNT	256

/** Linker symbols defined in rtecdc.lds */
extern const uint32 pageable_end;
extern const uint32 pageable_start;
extern const uint32 paging_reserved_base;
extern const uint32 paging_reserved_end;
extern const uint32 host_page_info[];
extern const uint32 watchdog_start;

/**
 * Initialize SW paging module
 */
void BCMATTACHFN(sw_paging_init)(void);

/**
 * API to handle the page fault.
 * @param fault_addr: Address corresponding to access violation.
 * @return
 *  - BCME_OK - page loaded successfully
 *  - BCME_RANGE - fault_addr is outside of valid virtual address space
 *  - BCME_BADADDR - Requested virtual page is already loaded
 */
int sw_paging_handle_page_fault(uint32 fault_addr);

/**
 * DMA channel is initialized.
 * From this point DMA will be used to download pages from host memory.
 */
void sw_paging_dma_ready_indication(void);

/** swp debug tool functions */
extern void hnd_cons_swp_dump(void *arg, int argc, char *argv[]);
extern void sw_paging_log_start(void);
extern void sw_paging_log_stop(void);
extern void sw_paging_log_clear(void);
extern void sw_paging_log_dump(void);
extern void sw_paging_log_usage(void);

#endif /* __SWPAGING_H */
