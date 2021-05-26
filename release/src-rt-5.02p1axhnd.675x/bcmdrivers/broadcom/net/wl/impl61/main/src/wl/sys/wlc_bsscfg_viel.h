/*
 * Per-BSS vendor IE list management interface.
 * Used to manage the user plumbed IEs in the BSS.
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
 * <<Broadcom-WL-IPTag/Proprietary:.*>>
 *
 * $Id: wlc_bsscfg_viel.h 532292 2015-02-05 13:59:45Z $
 */

#ifndef _wlc_bsscfg_viel_h_
#define _wlc_bsscfg_viel_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_vndr_ie_list.h>

/* attach/detach */

wlc_bsscfg_viel_info_t *wlc_bsscfg_viel_attach(wlc_info_t *wlc);
void wlc_bsscfg_viel_detach(wlc_bsscfg_viel_info_t *vieli);

/* APIs */

int wlc_vndr_ie_getlen_ext(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	vndr_ie_list_filter_fn_t filter, uint32 pktflag, int *totie);
#define wlc_vndr_ie_getlen(vieli, cfg, pktflag, totie) \
	wlc_vndr_ie_getlen_ext(vieli, cfg, NULL, pktflag, totie)
uint8 *wlc_vndr_ie_write_ext(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	vndr_ie_list_write_filter_fn_t filter, uint type, uint8 *cp, int buflen,
	uint32 pktflag);
#define wlc_vndr_ie_write(vieli, cfg, cp, buflen, pktflag) \
	wlc_vndr_ie_write_ext(vieli, cfg, NULL, -1, cp, buflen, pktflag)

vndr_ie_listel_t *wlc_vndr_ie_add_elem(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	uint32 pktflag,	vndr_ie_t *vndr_iep);
vndr_ie_listel_t *wlc_vndr_ie_mod_elem(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	vndr_ie_listel_t *old_listel, uint32 pktflag, vndr_ie_t *vndr_iep);

int wlc_vndr_ie_add(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	const vndr_ie_buf_t *ie_buf, int len);
int wlc_vndr_ie_del(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	const vndr_ie_buf_t *ie_buf, int len);
int wlc_vndr_ie_get(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	vndr_ie_buf_t *ie_buf, int len, uint32 pktflag);

int wlc_vndr_ie_mod_elem_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	uint8 type, uint32 pktflag, vndr_ie_t *vndr_iep);
int wlc_vndr_ie_del_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	uint8 type);
uint8 *wlc_vndr_ie_find_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *cfg,
	uint8 type);

#endif	/* _wlc_bsscfg_viel_h_ */
