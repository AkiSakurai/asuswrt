/*
 * Common cubby control library interface.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_cubby.h 783348 2020-01-24 10:25:17Z $
 */

#ifndef _wlc_cubby_h_
#define _wlc_cubby_h_

/**
 * @file @brief
 * Used by modules such as bsscfg and scb to provide cubby (feature specific
 * private data storage) control, and to provide secondary cubby (cubby is
 * a pointer, it points to another memory block) control and storage.
 *
 * Modules use the following interface to interact with this library:
 * 1. Use wlc_cubby_reserve() interface to register cubby size and
 *    client callbacks.
 * 2. Use wlc_cubby_init()/wlc_cubby_deinit()/wlc_cubby_dump() to invoke
 *    client callbacks when initializing/deinitializing/dumping cubby
 *    contents.
 *
 * This library uses the following client supplied callbacks to communicate
 * information back to clients:
 * 1. Use cubby_sec_set_fn_t callback to inform the client of the allocated
 *    memory for the secondary cubbies. The client must save this information.
 * 2. Use cubby_sec_get_fn_t callback to query the client of the secondary
 *    cubby memory. The secondary cubby memory is allocated once by this
 *    library for all participating clients; it's freed also by this libray.
 */

#include <typedefs.h>
#include <bcmutils.h>
#include <wlc_types.h>

typedef struct wlc_cubby_info wlc_cubby_info_t;

/* module attach/detach interface */
wlc_cubby_info_t *wlc_cubby_attach(osl_t *osh, uint unit, uint num);
void wlc_cubby_detach(wlc_cubby_info_t *cubby_info);

/* cubby callback functions */
typedef int (*cubby_init_fn_t)(void *ctx, void *obj);
typedef void (*cubby_deinit_fn_t)(void *ctx, void *obj);
typedef uint (*cubby_secsz_fn_t)(void *ctx, void *obj);
typedef void (*cubby_dump_fn_t)(void *ctx, void *obj, struct bcmstrbuf *b);

/** structure for registering per-cubby client info */
/* Note: if cubby init/deinit callbacks are invoked directly in other parts of
 * the code explicitly don't register fn_secsz callback and don't use secondary cubby
 * alloc/free interfaces.
 */
typedef struct wlc_cubby_fn {
	cubby_init_fn_t	fn_init;	/**< fn called during object alloc */
	cubby_deinit_fn_t fn_deinit;	/**< fn called during object free */
	cubby_secsz_fn_t fn_secsz;	/**< fn called during object alloc and free,
					 * optional and registered when the secondary cubby
					 * allocation is expected
					 */
	cubby_dump_fn_t fn_dump;	/**< fn called during object dump - BCMDBG only */
	const char *name;
} wlc_cubby_fn_t;

/* client registration interface */
int wlc_cubby_reserve(wlc_cubby_info_t *cubby_info, uint size, wlc_cubby_fn_t *fn, void *ctx);

/* secondary cubby callback functions */
typedef uint (*cubby_sec_sz_fn_t)(void *ctx, void *obj);
typedef void (*cubby_sec_set_fn_t)(void *ctx, void *obj, void *base);
typedef void *(*cubby_sec_get_fn_t)(void *ctx, void *obj);

/* init/deinit all cubbies */
int wlc_cubby_init(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, cubby_sec_set_fn_t fn, void *set_ctx);
void wlc_cubby_deinit(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, cubby_sec_get_fn_t fn, void *sec_ctx);

/* query total cubby size */
uint wlc_cubby_totsize(wlc_cubby_info_t *cubby_info);
/* query total secondary cubby size */
uint wlc_cubby_sec_totsize(wlc_cubby_info_t *cubby_info, void *obj);

/* secondary cubby alloc/free */
void *wlc_cubby_sec_alloc(wlc_cubby_info_t *cubby_info, void *obj, uint secsz);
void wlc_cubby_sec_free(wlc_cubby_info_t *cubby_info, void *obj, void *secptr);

/* debug/dump interface */
#if defined(BCMDBG)
void wlc_cubby_dump(wlc_cubby_info_t *cubby_info, void *obj,
	cubby_sec_sz_fn_t secsz_fn, void *sec_ctx, struct bcmstrbuf *b);
#endif // endif

#endif /* _wlc_cubby_h_ */
