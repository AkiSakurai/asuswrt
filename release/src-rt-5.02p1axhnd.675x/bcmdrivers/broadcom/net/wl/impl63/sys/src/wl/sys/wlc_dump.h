/**
 * @file
 * @brief
 * Named dump callback registration for WLC (excluding BMAC)
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
 * $Id: wlc_dump.h 614820 2016-01-23 17:16:17Z $
 */

#ifndef _wlc_dump_h_
#define _wlc_dump_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_dump_reg.h>

/* register dump and/or dump clear callbacks */
typedef wlc_dump_reg_dump_fn_t dump_fn_t;
typedef wlc_dump_reg_clr_fn_t clr_fn_t;
/* XXX keep using pub in the interface as in per port code
 * accessing to wlc_info field used to be prohibited.
 */
int wlc_dump_add_fns(wlc_pub_t *pub, const char *name,
	dump_fn_t dump_fn, clr_fn_t clr_fn, void *ctx);

/* API to invoke the local dump */
int
wlc_dump_local(wlc_dump_info_t *dumpi,
	char * name, int dump_len);

#define wlc_dump_register(pub, name, fn, ctx) \
	wlc_dump_add_fns(pub, name, fn, NULL, ctx)

/* early attach/late detach for as many others to use the dump module */
wlc_dump_info_t *wlc_dump_pre_attach(wlc_info_t *wlc);
void wlc_dump_post_detach(wlc_dump_info_t *dumpi);

/* attach/detach as a wlc module */
int wlc_dump_attach(wlc_dump_info_t *dumpi);
void wlc_dump_detach(wlc_dump_info_t *dumpi);

#endif /* _wlc_dump_h_ */
