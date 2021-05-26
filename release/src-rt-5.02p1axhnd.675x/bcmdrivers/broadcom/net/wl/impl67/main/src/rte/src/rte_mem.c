/** @file hnd_mem.c
 *
 * HND memory/image layout.
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
 * $Id: rte_mem.c 683666 2017-02-08 19:32:58Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <hnd_pktpool.h>
#include <rte_mem.h>
#include "rte_mem_priv.h"
#include <rte_cons.h>
#include "rte_heap_priv.h"

/* debug */
#ifdef BCMDBG
#define HND_MSG(x) printf x
#else
#define HND_MSG(x)
#endif // endif

/** This function must be forced into RAM since it uses RAM specific linker symbols.  */
void
BCMRAMFN(hnd_image_info)(hnd_image_info_t *i)
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

#ifdef BCM_RECLAIM_INIT_FN_DATA
	{
		extern char _rstart1[], _rend1[];
		i->_reclaim1_start = _rstart1;
		i->_reclaim1_end = _rend1;
	}
#endif // endif

#if defined(BCM_RECLAIM)
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

	{
		extern char _rstart5[], _rend5[];
		i->_reclaim5_start = _rstart5;
		i->_reclaim5_end = _rend5;
	}
#endif /* BCM_RECLAIM */
}

#ifdef _RTE_SIM_
uint32 _memsize = RAMSZ;
#endif // endif

/* ROM accessor. If '_memsize' is used directly, the tools think the assembly symbol '_memsize' is
 * a function, which will result in an unexpected entry in the RAM jump table.
 */
uint32
BCMRAMFN(hnd_get_memsize)(void)
{
	return (_memsize);
}

/* ROM accessor.
 */
uint32
BCMRAMFN(hnd_get_rambase)(void)
{
#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	/* CA7 also use the same variable though there is no tcm;
	 * revisit later
	 */
	return (_atcmrambase);
#endif // endif
	return 0;
}

uint32
BCMRAMFN(hnd_get_rambottom)(void)
{
#if defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
	/* CA7 also use the same variable though there is no tcm;
	 * revisit later
	 */
	return (_rambottom);
#else
	return (_memsize);
#endif // endif
}

/* ========================= reclaim ========================= */

#ifdef BCM_RECLAIM

#if defined(BCM_RECLAIM_INIT_FN_DATA) && defined(CONFIG_XIP)
#error "Both XIP and BCM_RECLAIM_INIT_FN_DATA defined"
#endif // endif

bool bcm_reclaimed = FALSE;

#ifdef BCM_RECLAIM_INIT_FN_DATA
bool bcm_init_part_reclaimed = FALSE;
#endif // endif
bool bcm_attach_part_reclaimed = FALSE;
bool bcm_preattach_part_reclaimed = FALSE;
bool bcm_postattach_part_reclaimed = FALSE;

void
hnd_reclaim(void)
{
	uint reclaim_size = 0;
	hnd_image_info_t info;
	uint memavail;
	const char *r_fmt_str = "reclaim section %s: Returned %d bytes (pre-reclaim: %d)\n";

	hnd_image_info(&info);

#ifdef BCM_RECLAIM_INIT_FN_DATA
	if (!bcm_reclaimed && !bcm_init_part_reclaimed) {
	        memavail = hnd_memavail();
		reclaim_size = (uint)(info._reclaim1_end - info._reclaim1_start);
		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim1_start, reclaim_size);
			hnd_arena_add((uint32)info._reclaim1_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "2", reclaim_size, memavail);
		bcm_reclaimed = TRUE;
		bcm_init_part_reclaimed = TRUE;
		goto exit;
	}
#endif /* BCM_RECLAIM_INIT_FN_DATA */

	if (!bcm_attach_part_reclaimed) {
	        memavail = hnd_memavail();
		reclaim_size = (uint)(info._reclaim2_end - info._reclaim2_start);

		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim2_start, reclaim_size);
			hnd_arena_add((uint32)info._reclaim2_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "1", reclaim_size, memavail);

		/* Reclaim space reserved for TCAM bootloader patching. Once the bootloader hands
		 * execution off to the firmware, the bootloader patch table is no longer required
		 * and can be reclaimed.
		 */
#if defined(BOOTLOADER_PATCH_RECLAIM)
		memavail = hnd_memavail();
		reclaim_size = (uint)(info._boot_patch_end - info._boot_patch_start);
		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._boot_patch_start, reclaim_size);
			hnd_arena_add((uint32)info._boot_patch_start, reclaim_size);
			printf(r_fmt_str, "boot-patch", reclaim_size, memavail);
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
				hnd_arena_add((uint32)info._reclaim4_start, reclaim_size);
				printf(r_fmt_str, "4", reclaim_size, memavail);
			}
		}
#endif /* BOOTLOADER_PATCH_RECLAIM */

		bcm_reclaimed = FALSE;
		bcm_attach_part_reclaimed = TRUE;

#ifdef HND_PT_GIANT
		hnd_append_ptblk();
#endif // endif

#ifdef BCMPKTPOOL
		{
		bool minimal = TRUE;
		hnd_pktpool_refill(minimal);
		}
#endif /* BCMPKTPOOL */
		goto exit;
	}

	if (!bcm_preattach_part_reclaimed) {
	        memavail = hnd_memavail();
		reclaim_size = (uint)(info._reclaim3_end - info._reclaim3_start);

		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim3_start, reclaim_size);
			hnd_arena_add((uint32)info._reclaim3_start, reclaim_size);
		}

		/* Nightly dongle test searches output for "Returned (.*) bytes to the heap" */
		printf(r_fmt_str, "0", reclaim_size, memavail);

		bcm_attach_part_reclaimed = FALSE;
		bcm_preattach_part_reclaimed = TRUE;
	}

	if (!bcm_postattach_part_reclaimed) {
	        memavail = hnd_memavail();
		reclaim_size = (uint)(info._reclaim5_end - info._reclaim5_start);

		if (reclaim_size) {
			/* blow away the reclaim region */
			bzero(info._reclaim5_start, reclaim_size);
			hnd_arena_add((uint32)info._reclaim5_start, reclaim_size);
			printf(r_fmt_str, "postattach", reclaim_size, memavail);
		}

		bcm_postattach_part_reclaimed = TRUE;
	}
exit:

	return;		/* return is needed to avoid compilation error
			   error: label at end of compound statement
			*/
}
#endif /* BCM_RECLAIM */

/* ==================== stack ==================== */

#ifndef BCM_BOOTLOADER
#if defined(RTE_CONS)
static void
hnd_print_memuse(void *arg, int argc, char *argv[])
{
	process_ccmd("hu", 2);
	process_ccmd("su", 2);
	process_ccmd("pu", 2);
}
#endif /* RTE_CONS */
#endif /* !BCM_BOOTLOADER */

/* ==================== cli ==================== */

void
hnd_mem_cli_init(void)
{
#ifndef BCM_BOOTLOADER
#if defined(RTE_CONS)
	if (!hnd_cons_add_cmd("mu", hnd_print_memuse, 0))
		return;
#endif /* !RTE_CONS */
#endif /* BCM_BOOTLOADER */
}
