/*
 * PCIEDEV OOB Deepsleep private data structures and macro definitions
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
 * $Id: pciedev_oob_ds.h  $
 */

#ifndef _pciedev_oob_ds_h_
#define _pciedev_oob_ds_h_

#include <typedefs.h>

typedef enum bcmpcie_ob_deepsleep_state {
	DS_INVALID_STATE = -2,
	DS_DISABLED_STATE = -1,
	NO_DS_STATE = 0,
	DS_CHECK_STATE,
	DS_D0_STATE,
	NODS_D3COLD_STATE,
	DS_D3COLD_STATE,
	DS_LAST_STATE
} bcmpcie_ob_deepsleep_state_t;
typedef enum bcmpcie_ob_deepsleep_event {
    DW_ASSRT_EVENT = 0,
    DW_DASSRT_EVENT,
    PERST_ASSRT_EVENT,
    PERST_DEASSRT_EVENT,
    DB_TOH_EVENT,
    DS_ALLOWED_EVENT,
    DS_NOT_ALLOWED_EVENT,
    HOSTWAKE_ASSRT_EVENT,
    DS_LAST_EVENT
} bcmpcie_ob_deepsleep_event_t;

extern void
pciedev_ob_deepsleep_engine(struct dngl_bus * pciedev, bcmpcie_ob_deepsleep_event_t event);
extern const char * pciedev_ob_ds_state_name(bcmpcie_ob_deepsleep_state_t state);
extern const char * pciedev_ob_ds_event_name(bcmpcie_ob_deepsleep_event_t event);

extern void pciedev_deepsleep_enter_req(struct dngl_bus *pciedev);
extern void pciedev_handle_host_deepsleep_ack(struct dngl_bus *pciedev);
extern void pciedev_handle_host_deepsleep_nak(struct dngl_bus *pciedev);
extern void pciedev_deepsleep_exit_notify(struct dngl_bus *pciedev);
extern void pciedev_trigger_deepsleep_dw(struct dngl_bus *pciedev);
extern void pciedev_dw_check_after_bm(struct dngl_bus *pciedev);
extern int pciedev_enable_device_wake(struct dngl_bus *pciedev);

#endif /* _pciedev_priv_h_ */
