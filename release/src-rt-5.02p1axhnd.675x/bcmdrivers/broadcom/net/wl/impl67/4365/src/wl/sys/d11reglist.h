/*
 * D11reglist for Broadcom 802.11abgn
 * Networking Adapter Device Drivers.
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
 * $Id: d11reglist.h 708017 2017-06-29 14:11:45Z $
 */
#ifndef _d11reglist_h_
#define _d11reglist_h_

#include "d11reglist_proto.h"

#ifdef WLC_MINMACLIST
extern CONST d11regs_list_t d11regsmin_pre40[];
extern CONST d11regs_list_t d11regsmin_ge40[];
extern CONST uint d11regsmin_pre40sz;
extern CONST uint d11regsmin_ge40sz;
#else
extern CONST d11regs_list_t d11regs23[];
extern CONST d11regs_list_t d11regs42[];
extern CONST d11regs_list_t d11regs48[];
extern CONST d11regs_list_t d11regs49[];
extern CONST d11regs_list_t d11regs_pre40[];
extern CONST d11regs_list_t d11regs_ge40[];
extern CONST d11regs_list_t d11regs64[];
extern CONST d11regs_list_t d11regsx64[];
extern CONST d11regs_list_t d11regs65[];
extern CONST d11regs_list_t d11regsx65[];
extern CONST uint d11regs23sz;
extern CONST uint d11regs42sz;
extern CONST uint d11regs48sz;
extern CONST uint d11regs49sz;
extern CONST uint d11regs_pre40sz;
extern CONST uint d11regs_ge40sz;
extern CONST uint d11regs64sz;
extern CONST uint d11regsx64sz;
extern CONST uint d11regs65sz;
extern CONST uint d11regsx65sz;
#endif /* WLC_MINMACLIST */

#endif /* _d11reglist_h_ */
