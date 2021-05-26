/*
 * MU-MIMO transmit module for Broadcom 802.11 Networking Adapter Device Drivers
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_txcfg.h $
 */

#ifndef _wlc_txcfg_h_
#define _wlc_txcfg_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_rate_sel.h>

/* defines */
typedef enum {
	SU      = 0,
	DLMMU   = 1,
	DLOFDMA = 2,
	ULOFDMA = 3,
	WLC_TXTYPE_MAX
} txcfg_t;

/* Maximum number of MU clients */
#define MUCLIENT_NUM  16

/* VHT and HE MU-MIMO are sub types of DLMMU. In subtype define,
 * bit 4 denotes super type DLMMU and bit 0 denotes subtypes: 0 for VHT  and 1 for HE.
 */
#define VHTMU	(DLMMU << 4 | 0)
#define HEMMU	(DLMMU << 4 | 1)

/* Minimum number of MU clients */
#define MUCLIENT_NUM_MIN	2
#define MUCLIENT_NUM_2		2
#define MUCLIENT_NUM_4		4
#define MUCLIENT_NUM_6		6
#define MUCLIENT_NUM_8		8
#define MUCLIENT_NUM_16		16

/* Standard attach and detach functions */
wlc_txcfg_info_t* BCMATTACHFN(wlc_txcfg_attach)(wlc_info_t *wlc);
void BCMATTACHFN(wlc_txcfg_detach)(wlc_txcfg_info_t *mu_info);

/* public APIs. */
extern int wlc_txcfg_max_clients_set(wlc_txcfg_info_t *txcfg, uint8 type, uint16 val);
extern uint16 wlc_txcfg_max_clients_get(wlc_txcfg_info_t *txcfg, uint8 type);
extern void wlc_txcfg_dlofdma_maxn_init(wlc_info_t *wlc, uint8 *maxn);
extern void wlc_txcfg_ulofdma_maxn_init(wlc_info_t *wlc, uint8 *maxn);
extern uint16 wlc_txcfg_ofdma_maxn_upperbound(wlc_info_t *wlc, uint16 bwidx);

typedef struct {
	uint8	state;
} txcfg_state_upd_data_t;

#endif   /* _wlc_txcfg_h_ */
