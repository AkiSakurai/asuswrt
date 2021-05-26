/**
 * ioctl/iovar HIGH driver wrapper module interface
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
 * $Id: wlc_iocv.h 765723 2018-07-13 11:57:53Z $
 */
#ifndef _wlc_iocv_h_
#define _wlc_iocv_h_

#include <wlc_types.h>
#include <wlc_iocv_reg.h>
#include <wlc_iocv_high.h>

/* attach/detach as a wlc module */
wlc_iocv_info_t *wlc_iocv_attach(wlc_info_t *wlc);
void wlc_iocv_detach(wlc_iocv_info_t *iocvi);

/* register iovar/ioctl table & dispatcher */
#ifdef WLC_PATCH_IOCTL
#define wlc_iocv_add_iov_fn(iocvi, tbl, fn, ctx) \
	wlc_iocv_high_register_iovt(iocvi, tbl, fn, IOV_PATCH_TBL, IOV_PATCH_FN, ctx)
#define wlc_iocv_add_ioc_fn(iocvi, tbl, tbl_sz, fn, ctx) \
	wlc_iocv_high_register_ioct(iocvi, tbl, tbl_sz, fn, IOC_PATCH_FN, ctx)
#else /* !WLC_PATCH_IOCTL */
#define wlc_iocv_add_iov_fn(iocvi, tbl, fn, ctx) \
	wlc_iocv_high_register_iovt(iocvi, tbl, fn, ctx)
#define wlc_iocv_add_ioc_fn(iocvi, tbl, tbl_sz, fn, ctx) \
	wlc_iocv_high_register_ioct(iocvi, tbl, tbl_sz, fn, ctx)
#endif /* !WLC_PATCH_IOCTL */

/* ioctl compatibility interface */
#define wlc_module_add_ioctl_fn(pub, hdl, fn, sz, tbl) \
	wlc_iocv_add_ioc_fn((pub)->wlc->iocvi, tbl, sz, fn, hdl)
#define wlc_module_remove_ioctl_fn(pub, hdl) BCME_OK

/* ioctl utilities */
int wlc_ioctl(wlc_info_t *wlc, int cmd, void *arg, int len, wlc_if_t *wlcif);
int wlc_set(wlc_info_t *wlc, int cmd, int arg);
int wlc_get(wlc_info_t *wlc, int cmd, int *arg);

int wlc_iocregchk(wlc_info_t *wlc, uint band);
int wlc_iocbandchk(wlc_info_t *wlc, int *arg, int len, uint *bands, bool clkchk);
int wlc_iocpichk(wlc_info_t *wlc, uint phytype);

/* iovar utilities */
int wlc_iovar_op(wlc_info_t *wlc, const char *name, void *params, int p_len, void *arg,
	int len, bool set, wlc_if_t *wlcif);
int wlc_iovar_getint(wlc_info_t *wlc, const char *name, int *arg);
#define wlc_iovar_getuint(wlc, name, arg) wlc_iovar_getint(wlc, name, (int *)(arg))
int wlc_iovar_setint(wlc_info_t *wlc, const char *name, int arg);
#define wlc_iovar_setuint(wlc, name, arg) wlc_iovar_setint(wlc, name, (int)(arg))
int wlc_iovar_getint8(wlc_info_t *wlc, const char *name, int8 *arg);
#define wlc_iovar_getuint8(wlc, name, arg) wlc_iovar_getint8(wlc, name, (int8 *)(arg))
int wlc_iovar_getbool(wlc_info_t *wlc, const char *name, bool *arg);
int wlc_iovar_check(wlc_info_t *wlc, const bcm_iovar_t *vi, void *arg, int len, bool set,
	wlc_if_t *wlcif);

#endif /* _wlc_iocv_h_ */
