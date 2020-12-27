
/**
 * @file
 * @brief
 * Common (OS-independent) portion of Broadcom debug crash validation
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_debug_crash.h 614820 2016-01-23 17:16:17Z $
 */

#ifndef __WLC_DEBUG_CRASH_H
#define __WLC_DEBUG_CRASH_H

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <wlc_pub.h>

#define DBG_CRSH_TYPE_RD_RANDOM		0x00
#define DBG_CRSH_TYPE_RD_INV_CORE	0x01
#define DBG_CRSH_TYPE_WR_INV_CORE	0x02
#define DBG_CRSH_TYPE_RD_INV_WRAP	0x03
#define DBG_CRSH_TYPE_WR_INV_WRAP	0x04
#define DBG_CRSH_TYPE_RD_RES_CORE	0x05
#define DBG_CRSH_TYPE_WR_RES_CORE	0x06
#define DBG_CRSH_TYPE_RD_RES_WRAP	0x07
#define DBG_CRSH_TYPE_WR_RES_WRAP	0x08
#define DBG_CRSH_TYPE_RD_CORE_NO_CLK	0x09
#define DBG_CRSH_TYPE_WR_CORE_NO_CLK	0x0A
#define DBG_CRSH_TYPE_RD_CORE_NO_PWR	0x0B
#define DBG_CRSH_TYPE_WR_CORE_NO_PWR	0x0C
#define DBG_CRSH_TYPE_PCIe_AER		0x0D
#define DBG_CRSH_TYPE_POWERCYCLE	0x0E
#define DBG_CRSH_TYPE_TRAP		0x0E
#define DBG_CRSH_TYPE_HANG		0x0F
#define DBG_CRSH_TYPE_PHYREAD		0x10
#define DBG_CRSH_TYPE_PHYWRITE		0x11
#define DBG_CRSH_TYPE_INV_PHYREAD	0x12
#define DBG_CRSH_TYPE_INV_PHYWRITE	0x13
#define DBG_CRSH_TYPE_DUMP_STATE	0x14

/* Radio/PHY health check crash scenarios - reserved 0x16 to 0x30 */
#define DBG_CRSH_TYPE_RADIO_HEALTHCHECK_START	0x16
#define DBG_CRSH_TYPE_DESENSE_LIMITS		0x17
#define DBG_CRSH_TYPE_BASEINDEX_LIMITS		0x18
#define DBG_CRSH_TYPE_TXCHAIN_INVALID		0x19
#define DBG_CRSH_TYPE_RADIO_HEALTHCHECK_LAST	0x30
#define DBG_CRSH_TYPE_BTCOEX_RFACTIVE		0x31
#define DBG_CRSH_TYPE_BTCOEX_TXCONF_DELAY	0x32
#define DBG_CRSH_TYPE_BTCOEX_ANT_DELAY		0x33
#define DBG_CRSH_TYPE_BTCOEX_INVLD_TASKID	0x34
#define DBG_CRSH_TYPE_LAST		0x35

#define DBG_CRASH_SRC_DRV		0x00
#define DBG_CRASH_SRC_FW		0x01
#define DBG_CRASH_SRC_UCODE		0x02

enum {
	IOV_DEBUG_CRASH = 0,
	IOV_DEBUG_LAST
	};

wlc_debug_crash_info_t * wlc_debug_crash_attach(wlc_info_t *wlc);
void wlc_debug_crash_detach(wlc_debug_crash_info_t *ctxt);
extern uint32 wlc_debug_crash_execute_crash(wlc_info_t* wlc, uint32 type, uint32 delay, int * err);

#endif /* __WLC_DEBUG_CRASH_H */
