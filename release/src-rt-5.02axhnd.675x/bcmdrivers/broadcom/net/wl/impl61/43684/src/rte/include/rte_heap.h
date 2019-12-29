/*
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
 * $Id: rte_heap.h 775419 2019-05-29 19:55:17Z $
 */

#ifndef	_RTE_HEAP_H
#define	_RTE_HEAP_H

#include <typedefs.h>
#include <rte_ioctl.h>

/* malloc, free */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
extern void *hnd_malloc_align(uint size, uint alignbits, void *call_site);
extern void *hnd_malloc_persist_align(uint size, uint alignbits, void *call_site);
extern void *hnd_malloc_persist_attach_align(uint size, uint alignbits, void *call_site);
#define hnd_malloc(size)	hnd_malloc_align(size, 0, CALL_SITE)
#else
#define hnd_malloc(size)	hnd_malloc_align(size, 0)
extern void *hnd_malloc_align(uint size, uint alignbits);
extern void *hnd_malloc_persist_align(uint size, uint alignbits);
extern void *hnd_malloc_persist_attach_align(uint size, uint alignbits);
#endif /* BCMDBG_MEM */

#if defined(BCMDBG_MEM) || defined(BCMDBG_HEAPCHECK)
extern int hnd_memcheck(const char *file, int line);
#endif /* BCMDBG_MEM  || BCMDBG_HEAPCHECK */

extern int hnd_free_persist(void *ptr);
extern void *hnd_realloc(void *ptr, uint size);
extern void *hnd_calloc(uint num, uint size);
extern int hnd_free(void *ptr);
extern uint hnd_memavail(void);

extern uint hnd_arena_add(uint32 base, uint size);

#ifndef BCM_BOOTLOADER
#if defined(RTE_CONS)
extern int hnd_get_heapuse(memuse_info_t *hu);
#endif /* RTE_CONS */
#endif /* BCM_BOOTLOADER */

#ifdef AWD_EXT_TRAP
#define HEAP_HISTOGRAM_DUMP_LEN 6
#define HEAP_MAX_SZ_BLKS_LEN 2

// Forward declaration
typedef struct _trap_struct trap_t;

uint8 *awd_handle_trap_heap(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen);
#endif /* AWD_EXT_TRAP */

#ifdef MEM_ALLOC_STATS
void hnd_update_mem_alloc_stats(memuse_info_t *mu);
#endif // endif

#endif	/* _RTE_HEAP_H */
