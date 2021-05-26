/*
 * HND Run Time Environment ARM MMU specific.
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
 * $Id$
 */

#ifndef _rte_mmu_h_
#define _rte_mmu_h_

#if defined(__ARM_ARCH_7A__)

/* Debugging macros */
//#define BCMDBG_MMU

#ifdef BCMDBG_MMU
	#define MMU_MSG(x) printf x
#else
	#define MMU_MSG(x)
#endif /* BCMDBG_MMU */

#define MMU_INFO(x) printf x
#define MMU_PRT(x) printf x

/* Force inlining of function. */
#define MMU_INLINE inline  __attribute__ ((always_inline))

#ifdef KB
#undef KB
#endif // endif
#define KB (1024)

#ifdef MB
#undef MB
#endif // endif
#define MB (1024 * 1024)

/* Translation Table Base Address must be aligned to 16KB, and can't be reclaimed
 * after inited. TLB would go back to look up these tables when cache missed
 */
#define L1_PAGETABLE_ENTRIES_NUM	(PAGETABLE_SIZE / sizeof(uint32))
#define L1_16KB_ALIGNMENT_BITS	14u

/* defines for L1 MMU 'section' (describing the properties of a 1MB block of memory) */
#define L1_PXN_SHIFT		0  /**< privileged execute-not bit */
#define L1_SECTION_SHIFT	1  /**< (super)section descriptor */
#define L1_B_SHIFT		2  /**< bufferable */
#define L1_C_SHIFT		3  /**< cachable */
#define L1_XN_SHIFT		4  /**< execute-not bit */
#define L1_DOMAIN_SHIFT		5
#define L1_AP01_SHIFT		10 /**< AP[1:0] access permission */
#define L1_TEX_SHIFT		12 /**< TEX[2:0]region attribute */
#define L1_AP2_SHIFT		15 /**< AP[2] access permission */
#define L1_S_SHIFT		16 /**< sharable */
#define L1_NG_SHIFT		17 /**< non global */
#define L1_BASE_ADDR_SHIFT	20 /**< section base address */
#define L1_PAGE_TABLE_PXN	2  /** privileged execute-not bit for a L1 'page table' entry */

#define L1_ENTRY_PAGE_TABLE(page_table_address, pxn) (\
	(1 << 0) | ((pxn) << L1_PAGE_TABLE_PXN) | (page_table_address))

/**< shareable, TEX 3'b100, AP r/w all region, cached, buffered, no write-alloc */
#define L1_ENTRY_NORMAL_MEM(i_1mb_section) (\
	(1 << L1_SECTION_SHIFT) | (i_1mb_section << 20) | \
	(1 << L1_B_SHIFT) | (1 << L1_C_SHIFT) | (3 << L1_AP01_SHIFT) | \
	(4 << L1_TEX_SHIFT) | (1 << L1_S_SHIFT))

#define L1_ENTRY_DEVICE_MEM(i_1mb_section) (\
	(1 << L1_SECTION_SHIFT) | (i_1mb_section << 20) | \
	(1 << L1_B_SHIFT) | (3 << L1_AP01_SHIFT) | (1 << L1_XN_SHIFT) | (1<< L1_PXN_SHIFT))

/** strongly ordered memory. AP r/w all region, non-cacheable, non-bufferable, and serialized */
#define L1_ENTRY_STRONGLY_MEM(i_1mb_section) (\
	(1 << L1_SECTION_SHIFT) | (i_1mb_section << 20) | \
	(3 << L1_AP01_SHIFT) | (1 << L1_XN_SHIFT) | (1<< L1_PXN_SHIFT))

#define L1_ENTRY_NOTHING(i_1mb_section) 0 /**< exception when ARM tries to access this location */

/* From Cortex-A PG [9.5]: "An L2 translation table has 256 word-sized (4 byte) entries,
 * requires 1KB of memory space and must be aligned to a 1KB boundary"
 */
#define L2_1KB_ALIGNMENT_BITS		(10)

/* SW_PAGING_CODE_VIRTUAL_ADDR_SIZE is not defined for non sw-paging builds */
#ifndef SW_PAGING_CODE_VIRTUAL_ADDR_SIZE
#define SW_PAGING_CODE_VIRTUAL_ADDR_SIZE 0
#endif // endif

#define L2_ROM_NUM_PAGE_TABLES		(0)

#define L2_RAM_NUM_PAGE_TABLES		(ROUNDUP(RAMSIZE, MB) / MB)
#define L2_SW_PAGING_NUM_PAGE_TABLES	(ROUNDUP(SW_PAGING_CODE_VIRTUAL_ADDR_SIZE, MB) / MB)

#define L2_VIRT_RAM_NUM_PAGE_TABLES	(0)
#define L2_VIRT_ROM_NUM_PAGE_TABLES	(0)

#define L2_VIRT_MEM_NUM_PAGE_TABLES	(L2_SW_PAGING_NUM_PAGE_TABLES + \
						L2_VIRT_RAM_NUM_PAGE_TABLES + \
						L2_VIRT_ROM_NUM_PAGE_TABLES)

#define L2_NUM_PAGE_TABLES		(L2_ROM_NUM_PAGE_TABLES + \
						L2_RAM_NUM_PAGE_TABLES + \
						L2_VIRT_MEM_NUM_PAGE_TABLES)

#define L2_PAGETABLE_ENTRIES_NUM	(MB / PAGE_SIZE)
#define L2_PAGETABLE_ARRAY_SIZE		(KB * L2_NUM_PAGE_TABLES)

#define L2_MB_TO_PAGES(mb)		(mb << 8) /* mb * L2_PAGETABLE_ENTRIES_NUM */

/* defines for L2 MMU large page (4KB) entries */
#define L2_B_SHIFT		2  /**< bufferable */
#define L2_C_SHIFT		3  /**< cachable */
#define L2_AP_SHIFT		4
#define L2_SBZ_SHIFT		6
#define L2_APX_SHIFT		9
#define L2_S_SHIFT		10 /**< sharable */
#define L2_NG_SHIFT		11 /**< non global */
#define L2_TEX_SHIFT		12
#define L2_XN_SHIFT		15 /**< execute-not bit */
#define L2_ADDR_SHIFT		16 /**< large page base address */

/** defines a L2 MMU entry for 'normal memory', to be used for ROM and RAM */
#define L2_ENTRY_NORMAL_MEM(i_4k_page) (\
	(((i_4k_page) << 12) & 0xffff0000) | \
	(1 << 0) | (1 << L2_B_SHIFT) | (1 << L2_C_SHIFT) | \
	(3 << L2_AP_SHIFT) | (4 << L2_TEX_SHIFT) | (1 << L2_S_SHIFT))

#define L2_ENTRY_DEVICE_MEM(i_4k_page) (\
	(((i_4k_page) << 12) & 0xffff0000) | \
	(1 << 0) | (1 << L2_B_SHIFT) | (3 << L2_AP_SHIFT) | (1 << L2_XN_SHIFT))

#define L2_ENTRY_NOTHING(i_4k_page) 0 /**< exception when ARM tries to access this location */

/* defines for L2 MMU small page (4KB) entries */
#define L2S_XN_SHIFT		0  /**< execute-not bit */
#define L2S_SMALL_PAGE_SHIFT	1  /**< set to 1 to indicate small page format */
#define L2S_B_SHIFT		2  /**< bufferable region attribute */
#define L2S_C_SHIFT		3  /**< cachable region attribute */
#define L2S_AP01_SHIFT		4  /**< AP[1:0] access permission */
#define L2S_TEX_SHIFT		6  /**< TEX[2:0]region attribute */
#define L2S_AP2_SHIFT		9  /**< AP[2] access permission */
#define L2S_S_SHIFT		10 /**< sharable */
#define L2S_NG_SHIFT		11 /**< non global */
#define L2S_ADDR_SHIFT		12 /**< small page base address */

/** defines a L2 MMU small page entry for 'normal memory', to be used for text and data */
#define L2S_ENTRY_NORMAL_MEM(i_4k_page)  (\
	(1 << L2S_SMALL_PAGE_SHIFT) | ((i_4k_page) << L2S_ADDR_SHIFT) | \
	(4 << L2S_TEX_SHIFT) | (1 << L2S_B_SHIFT) | (1 << L2S_C_SHIFT) | \
	(3 << L2S_AP01_SHIFT) | (1 << L2S_S_SHIFT))

/** defines a L2 MMU small page entry for 'execute memory', to be used for text only */
#define L2S_ENTRY_EXEC_NOWR_MEM(i_4k_page)  (\
	(1 << L2S_SMALL_PAGE_SHIFT) | ((i_4k_page) << L2S_ADDR_SHIFT) | \
	(4 << L2S_TEX_SHIFT) | (1 << L2S_B_SHIFT) | (1 << L2S_C_SHIFT) | \
	(1 << L2S_AP2_SHIFT) | (3 << L2S_AP01_SHIFT) | (1 << L2S_S_SHIFT))

/** defines a L2 MMU small page entry for 'data memory', to be used for data only */
#define L2S_ENTRY_NOEXEC_WR_MEM(i_4k_page)  (\
	(1 << L2S_SMALL_PAGE_SHIFT) | ((i_4k_page) << L2S_ADDR_SHIFT) | \
	(1 << L2S_XN_SHIFT) | \
	(4 << L2S_TEX_SHIFT) | (1 << L2S_B_SHIFT) | (1 << L2S_C_SHIFT) | \
	(3 << L2S_AP01_SHIFT) | (1 << L2S_S_SHIFT))

/** defines a L2 MMU small page entry for 'no access' */
#define L2S_PAGE_ATTR_NO_ACC (0)

/** defines a L2 MMU small page entry for 'data memory', to be used for data only */
#define L2S_PAGE_ATTR_NOEXEC_WR_MEM (\
	(1u << L2S_SMALL_PAGE_SHIFT) | \
	(1u << L2S_XN_SHIFT)         | \
	(4u << L2S_TEX_SHIFT)        | \
	(1u << L2S_B_SHIFT)          | \
	(1u << L2S_C_SHIFT)          | \
	(3u << L2S_AP01_SHIFT)       | \
	(1u << L2S_S_SHIFT))

/** defines a L2 MMU small page entry for 'execute memory', to be used for text only */
#define L2S_PAGE_ATTR_EXEC_NOWR (\
	(1u << L2S_SMALL_PAGE_SHIFT) | \
	(4u << L2S_TEX_SHIFT)  | \
	(1u << L2S_B_SHIFT)    | \
	(1u << L2S_C_SHIFT)    | \
	(1u << L2S_AP2_SHIFT)  | \
	(3u << L2S_AP01_SHIFT) | \
	(1u << L2S_S_SHIFT))

/** defines a L2 MMU small page entry for read-only, non-executable memory. */
#define L2S_PAGE_ATTR_NOEXEC_NOWR (\
	L2S_PAGE_ATTR_EXEC_NOWR | \
	(1u << L2S_XN_SHIFT))

/** defines a L2 MMU small page entry for 'normal memory', to be used for text and data */
#define L2S_PAGE_ATTR_NORMAL_MEM (\
	(1u << L2S_SMALL_PAGE_SHIFT)    | \
	(4u << L2S_TEX_SHIFT)           | \
	(1u << L2S_B_SHIFT)             | \
	(1u << L2S_C_SHIFT)             | \
	(3u << L2S_AP01_SHIFT)          | \
	(1u << L2S_S_SHIFT))

/* From ARM reference architecture manual:
 * Therefore, an example instruction sequence for writing a translation table entry,
 * covering changes to the instruction or data mappings in a uniprocessor system is:
 * STR rx, [Translation table entry]		; write new entry to the translation table
 * Clean cache line [Translation table entry]	: This operation is not required with the
 *						; Multiprocessing Extensions.
 * DSB ; ensures visibility of the data cleaned from the D Cache
 * Invalidate TLB entry by MVA (and ASID if non-global) [page address]
 * Invalidate BTC
 * DSB ; ensure completion of the Invalidate TLB operation
 * ISB ; ensure table changes visible to instruction fetch
 */
#define HND_MMU_FLUSH_TLB()							\
	do {									\
		cpu_flush_cache_all();						\
		dsb();								\
										\
		/* Invalidate entire data TLB */				\
		asm volatile("mcr p15, 0, %0, c8, c6, 0" : : "r" (0));		\
		dsb();								\
										\
		/* Invalidate entire instruction TLB */				\
		asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r" (0));		\
		dsb();								\
										\
		/* Branch Predictor Invalidate All */				\
		asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r" (0));		\
		dsb();								\
		isb();								\
	} while (0)

/* Page table map. */
typedef struct hnd_mmu_page_tbl {
	uint32	mb;	/* MB index. */
	uint32	addr;	/* Level-2 page-table address. */
} hnd_mmu_page_tbl_t;

/* Set L1 page attributes of given memory region:
 *
 * Inputs: virt_addr    - start virtual address of given region
 *         phy_addr     - start physical address of given region
 *         size         - size of region in bytes
 *         attrib       - L1 page attributes
 *         flush        - if TRUE -> apply changes (flush)
 */
#define HND_MMU_SET_L1_PAGE_ATTRIBUTES(virt_addr, phy_addr,				\
	size, attrib, flush)								\
	do {										\
		uint32 _mb, phy_page;							\
		uint32 *vir_page_tbl;							\
		uint32 _size = (size);							\
		uint32 _virt_addr = (uint32)(virt_addr);				\
		uint32 _phy_addr = (uint32)(phy_addr);					\
											\
		MMU_MSG(("L1 region: vaddr 0x%08x paddr 0x%08x size 0x%08x "		\
			"attr 0x%08x\n", virt_addr, phy_addr, size, attrib));	\
		MMU_MSG(("L1 pages: start addr 0x%08x end addr 0x%08x\n",		\
			(_virt_addr >>  L2S_ADDR_SHIFT) << L2S_ADDR_SHIFT,		\
			((_virt_addr + _size) >> L2S_ADDR_SHIFT) << L2S_ADDR_SHIFT)); \
											\
		ASSERT((_size         % MB) == 0);					\
		ASSERT((_virt_addr    % MB) == 0);					\
		ASSERT((_phy_addr     % MB) == 0);					\
		while (_size >= MB) {							\
			_mb = _virt_addr >> L1_BASE_ADDR_SHIFT;				\
			phy_page = (_phy_addr) >> L1_BASE_ADDR_SHIFT;			\
											\
			hnd_mmu_get_l1_page_tbl_entry(&vir_page_tbl, _mb);		\
											\
			*vir_page_tbl = L1_ENTRY(phy_page, attrib);			\
											\
			_size -= MB;							\
			_virt_addr += MB;						\
			_phy_addr += MB;						\
		}									\
											\
		if (flush) {								\
			HND_MMU_FLUSH_TLB();						\
		}									\
	} while (0)

/* Wrapper for HND_MMU_SET_L2_PAGE_ATTRIBUTES() that also allocates L2 page-tables before
 * setting the page-table attributes.
 *
 * Inputs: virt_addr    - start virtual address of given region
 *         phy_addr     - start physical address of given region
 *         size         - size of region in bytes
 *         attrib       - L2 page attributes
 *         flush        - if TRUE -> apply changes (flush)
 */
#define HND_MMU_ALLOC_L2_PAGE_ATTRIBUTES(virt_addr, phys_addr, size, attrib, flush)		\
	do {											\
		uint32             mb_idx;							\
		hnd_mmu_page_tbl_t *page_tblp;							\
		uint32             *l1_page_tbl;						\
												\
		/* Allocate page-tables. */							\
		for (mb_idx = ((uint32)(virt_addr) / MB);					\
			mb_idx <= (((uint32)(virt_addr) + (size) - 1) / MB); mb_idx++) {	\
												\
			page_tblp = hnd_mmu_alloc_l2_page_tbl_entry(mb_idx);			\
												\
			hnd_mmu_get_l1_page_tbl_entry(&l1_page_tbl, mb_idx);			\
			*l1_page_tbl = L1_ENTRY_PAGE_TABLE(page_tblp->addr, 0);			\
			MMU_MSG(("L1 entry [%d] = 0x%08x [L2 ptr]\n", mb_idx, *l1_page_tbl));	\
		}										\
												\
		/* Set page-table attributes. */						\
		HND_MMU_SET_L2_PAGE_ATTRIBUTES((uint32)(virt_addr), (uint32)(phys_addr),	\
			 (size), (attrib), (flush));						\
	} while (0)

/* Set L2 page attributes of given memory region:
 *
 * Inputs: virt_addr    - start virtual address of given region
 *         phy_addr     - start physical address of given region
 *         size         - size of region in bytes
 *         attrib       - L2 page attributes
 *         flush        - if TRUE -> apply changes (flush)
 */
#define HND_MMU_SET_L2_PAGE_ATTRIBUTES(virt_addr, phy_addr,				\
	size, attrib, flush)								\
	do {										\
		uint32 _mb, vir_page, phy_page;						\
		uint32 *vir_page_tbl;							\
		uint32 _size = size;							\
		uint32 _virt_addr = (uint32)(virt_addr);				\
		uint32 _phy_addr = (uint32)(phy_addr);					\
		hnd_mmu_page_tbl_t *l2_page_tbl;					\
											\
	MMU_MSG(("L2 region: vaddr 0x%08x paddr 0x%08x size 0x%08x attr 0x%08x\n",	\
			virt_addr, phy_addr, size, attrib));				\
		MMU_MSG(("L2 pages: start addr 0x%08x end addr 0x%08x\n",		\
			(_virt_addr >>  L2S_ADDR_SHIFT) << L2S_ADDR_SHIFT,		\
			((_virt_addr + _size) >> L2S_ADDR_SHIFT) << L2S_ADDR_SHIFT));	\
											\
		ASSERT((_size         % PAGE_SIZE) == 0);				\
		ASSERT((_virt_addr    % PAGE_SIZE) == 0);				\
		ASSERT((_phy_addr     % PAGE_SIZE) == 0);				\
		while (_size >= PAGE_SIZE) {						\
			_mb = _virt_addr / MB;						\
			vir_page = (_virt_addr & (MB - 1)) >> L2S_ADDR_SHIFT;		\
			phy_page = (_phy_addr) >> L2S_ADDR_SHIFT;			\
											\
			l2_page_tbl = hnd_mmu_get_l2_page_tbl_entry(_mb);		\
			ASSERT((l2_page_tbl != NULL));					\
			vir_page_tbl = (uint32 *)l2_page_tbl->addr;			\
											\
			vir_page_tbl[vir_page] = attrib |				\
				(phy_page << L2S_ADDR_SHIFT);				\
											\
			_size -= PAGE_SIZE;						\
			_virt_addr += PAGE_SIZE;					\
			_phy_addr += PAGE_SIZE;						\
		}									\
											\
		if (flush) {								\
			HND_MMU_FLUSH_TLB();						\
		}									\
	} while (0)

extern hnd_mmu_page_tbl_t hnd_mmu_page_tbl_ptrs[L2_NUM_PAGE_TABLES];
extern uint8 *hnd_mmu_l1_page_table;
extern uint8 *hnd_mmu_l2_page_table;

/* Get MMU level-1 translation table descriptor.
 *
 * Inputs: vpt - Returned pointer to descriptor.
 *         mb  - Retrieve descriptor for this MB.
 */
static MMU_INLINE void
hnd_mmu_get_l1_page_tbl_entry(uint32 **vpt, uint32 mb)
{
	uint32 *ptb = (uint32 *)hnd_mmu_l1_page_table;
	*(vpt) = (uint32 *)&ptb[(mb)];
}

/* Get MMU level-2 page table.
 *
 * Inputs: mb  - Retrieve page table for this MB.
 *
 * Return: Page table state.
 */
static MMU_INLINE hnd_mmu_page_tbl_t*
hnd_mmu_get_l2_page_tbl_entry(uint32 mb)
{
	hnd_mmu_page_tbl_t *page_tbl = NULL;
	uint               idx;

	for (idx = 0; idx < ARRAYSIZE(hnd_mmu_page_tbl_ptrs); idx++) {
		if ((hnd_mmu_page_tbl_ptrs[idx].addr != NULL) &&
		    (hnd_mmu_page_tbl_ptrs[idx].mb == mb)) {
			page_tbl = &hnd_mmu_page_tbl_ptrs[idx];
			break;
		}
	}

	return (page_tbl);
}

hnd_mmu_page_tbl_t* hnd_mmu_alloc_l2_page_tbl_entry(uint32 mb);

#endif /* __ARM_ARCH_7A__ */

#endif	/* _rte_arm_h_ */
