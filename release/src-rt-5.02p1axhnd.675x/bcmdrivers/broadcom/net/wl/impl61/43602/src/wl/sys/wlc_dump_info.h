/*
 * Common (OS-independent) info dump definitions for
 * Broadcom 802.11abg Networking Device Driver
 * an extension of wlc.c
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
 * $Id: wlc.h 449873 2014-01-20 07:45:51Z $
 */

#ifndef WLC_DUMP_INFO_H_
#define WLC_DUMP_INFO_H_

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP) || \
	defined(WLTEST) || defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || \
	defined(BCMDBG_TXBF) || defined(BCMDBG_DUMP_RSSI) || defined(MCHAN_MINIDUMP) || \
	defined(BCMDBG_TXSTUCK)
#define IOV_DUMP_REGULAR_IOVAR_ENAB 1
#else
#define IOV_DUMP_REGULAR_IOVAR_ENAB 0
#endif /* defined(BCMDBG) || defined(BCMDBG_DUMP) ... */

/* attach/detach */
extern int wlc_dump_info_attach(wlc_info_t *wlc);
extern void wlc_dump_info_detach(wlc_info_t *wlc);

#if IOV_DUMP_REGULAR_IOVAR_ENAB
/* external APIs moved from wlc.c */
extern int
wlc_iovar_dump(wlc_info_t *wlc, const char *params, int p_len, char *out_buf, int out_len);
#endif /* IOV_DUMP_REGULAR_IOVAR_ENAB */

#ifndef LINUX_POSTMOGRIFY_REMOVAL
extern int wlc_dump_register(wlc_pub_t *pub, const char *name, dump_fn_t dump_fn,
                             void *dump_fn_arg);
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#endif /* WLC_DUMP_INFO_H_ */
