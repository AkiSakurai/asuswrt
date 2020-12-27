/*
 * RandMac internal interface
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
 * $Id$
 */

#ifndef _randmac_h_
#define _randmac_h_

#include <typedefs.h>
#include <bcmutils.h>
#include <wlioctl.h>

#include <wlc_types.h>
#include <wlc_pcb.h>

typedef wlc_randmac_info_t randmac_t;

/* notifications supported */
enum {
	RANDMAC_NOTIF_NONE		= 0,
	RANDMAC_NOTIF_BSS_UP		= 1,
	RANDMAC_NOTIF_BSS_DOWN		= 2,
	RANDMAC_NOTIF_EVENT_TYPE	= 3,
	RANDMAC_NOTIF_MAX
};
typedef int randmac_notify_t;

/* Notification information */
enum {
	RANDMAC_INFO_NONE	= 0x00,
	RANDMAC_INFO_DISABLED	= 0x01,
	RANDMAC_INFO_ENABLED	= 0x02,
	RANDMAC_INFO_DELETED	= 0x03
};
typedef int16 randmac_notif_info_t;

struct randmac_notify_info {
	int8 bsscfg_idx;
	uint8 reserve; /* pad for alignment */
	wl_randmac_event_type_t event_type;
	const uint8 *data;
	int data_len;
};
typedef struct randmac_notify_info randmac_notify_info_t;

typedef wlc_randmac_event_callback_t randmac_event_callback_t;

/* initialize event */
void randmac_init_event(randmac_t *randmac, wl_randmac_event_type_t type,
	wl_randmac_method_t method, wl_randmac_event_t *event);

/* send event; len includes event header */
void randmac_send_event(randmac_t *randmac, wlc_bsscfg_t *bsscfg, wl_randmac_status_t status,
	wl_randmac_event_t *event, uint16 len);

void randmac_notify(wlc_randmac_info_t *randmac, wlc_bsscfg_t *bsscfg,
	randmac_notify_t notif, randmac_notify_info_t *info);
#endif /* _randmac_h_ */
