/*
 * MBO+OCE IE management implementation for
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id$
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

/**
 * @file
 * @brief
 * This file co-ordinates WFA MBO OCE IE management
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wl_dbg.h>
#include <wlioctl.h>
#include <wlc_bsscfg.h>

#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_types.h>
#include <wlc_ie_mgmt_types.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_vs.h>
#include <mbo_oce.h>
#include <wlc_mbo_oce.h>
#include <wlc_mbo.h>
#include "wlc_mbo_oce_priv.h"
#include <wlc_ie_mgmt_ft.h>

#ifndef MBO_OCE_MAX_BUILD_CBS
	#define MBO_OCE_MAX_BUILD_CBS 8
#endif /* MBO_OCE_MAX_BUILD_CBS */
#ifndef MBO_OCE_MAX_PARSE_CBS
	#define MBO_OCE_MAX_PARSE_CBS 8
#endif /* MBO_OCE_MAX_PARSE_CBS */

uint16 ie_build_fstbmp = FT2BMP(FC_ASSOC_REQ) |
	FT2BMP(FC_REASSOC_REQ) |
	FT2BMP(FC_ASSOC_RESP) |
	FT2BMP(FC_REASSOC_RESP) |
	FT2BMP(FC_PROBE_RESP) |
	FT2BMP(FC_PROBE_REQ);

uint16 ie_parse_fstbmp = FT2BMP(FC_BEACON) |
	FT2BMP(FC_PROBE_RESP) |
	FT2BMP(FC_ASSOC_RESP) |
	FT2BMP(FC_REASSOC_RESP) |
	FT2BMP(WLC_IEM_FC_SCAN_BCN) |
	FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);

typedef struct wlc_mbo_oce_ie_build_entry {
	void *ctx;
	uint16 fstbmp;
	wlc_mbo_oce_attr_build_fn_t build_fn;
} wlc_mbo_oce_ie_build_entry_t;

typedef struct wlc_mbo_oce_ie_parse_entry {
	void *ctx;
	uint16 fstbmp;
	wlc_mbo_oce_attr_parse_fn_t parse_fn;
} wlc_mbo_oce_ie_parse_entry_t;

typedef struct wlc_mbo_oce_data wlc_mbo_oce_data_t;
struct wlc_mbo_oce_data {
};

struct wlc_mbo_oce_info {
	osl_t    *osh;
	wlc_cmn_info_t *cmn;
	wlc_pub_t *pub;  /* update before calling module detach */
	wlc_pub_cmn_t *pub_cmn;
	obj_registry_t *objr;
	uint16 ie_build_fstbmp;
	uint16 ie_parse_fstbmp;
	uint8 max_build_cbs;
	uint8 count_build_cbs;
	uint8 max_parse_cbs;
	uint8 count_parse_cbs;
	wlc_mbo_oce_ie_build_entry_t *build_cbs;
	wlc_mbo_oce_ie_parse_entry_t *parse_cbs;
};
