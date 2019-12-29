/*
 * RandMac method support. See twiki MacAddressRandomization
 * This is external s/w interface to randmac.
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

#ifndef _wlc_randmac_h_
#define _wlc_randmac_h_
#include <typedefs.h>
#include <bcmutils.h>
#include <wlc_types.h>
#include <wlioctl.h>

/* handle to randmac */
struct randmac;
typedef struct randmac wlc_randmac_t;

/* event data  - note: session may not be valid once callback returns */
struct randmac_event_data {
	wlc_randmac_t *randmac;
	wl_randmac_event_type_t event_type;
	wl_randmac_status_t status;
};
typedef struct randmac_event_data wlc_randmac_event_data_t;

typedef void (*wlc_randmac_event_callback_t)(void *cb_ctx,
	wlc_randmac_event_data_t *event_data);

bool wlc_randmac_is_enabled(wlc_randmac_t *randmac);
bool is_randmac_macaddr_change_enabled(wlc_info_t *wlc);

/*
 * This function attaches MAC randomization module to wlc.
 */
wlc_randmac_info_t *wlc_randmac_attach(wlc_info_t *wlc);

/*
 * This function detaches MAC randomization module from wlc
 */
void wlc_randmac_detach(wlc_randmac_info_t *randmac_info);

/* register for events */
int wlc_randmac_event_register(wlc_randmac_info_t *randmac, wl_randmac_event_mask_t events,
	wlc_randmac_event_callback_t cb, void *cb_ctx);

/* unregister for events */
int wlc_randmac_event_unregister(wlc_randmac_info_t *randmac,
	wlc_randmac_event_callback_t cb, void *cb_ctx);

wlc_randmac_t *wlc_randmac_get_handle(wlc_info_t *wlc);

#endif /* _wlc_randmac_h_ */
