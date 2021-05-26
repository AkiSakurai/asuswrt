/*
 * Motion ProFiles for WLAN functionality (initially PNO).
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
 * $Id$
 */

#ifndef _wlc_mpf_h_
#define _wlc_mpf_h_

#ifdef WL_MPF
#include <wlc_types.h>

/* Direct external entry points: attach/detach, check state */
extern wlc_mpf_info_t *wlc_mpf_attach(wlc_info_t *wlc);
extern int wlc_mpf_detach(wlc_mpf_info_t *mpf);
extern int wlc_mpf_current_state(wlc_info_t *wlc, uint16 type, uint16 *statep);

/* Direct external entry points: register/unregister; needs callback def */
typedef void (*mpf_callback_t)(void *handle, uintptr subhandle, uint old_state, uint new_state);
extern int wlc_mpf_register(wlc_info_t *wlc, uint type, mpf_callback_t cb_fn,
                            void *handle, uintptr subhandle, uint32 mode_flags);
extern int wlc_mpf_unregister(wlc_info_t *wlc, uint type, mpf_callback_t cb_fn,
                              void *handle, uintptr subhandle);

#if defined(EVENT_LOG_COMPILE)

#define WL_MPF_ERR(arg) WL_MPF_EVENT_ERR arg
#define WL_MPF_WARN(arg) WL_MPF_EVENT_WARN arg
#define WL_MPF_INFO(arg) WL_MPF_EVENT_INFO arg
#define WL_MPF_DEBUG(arg) WL_MPF_EVENT_DEBUG arg

#define WL_MPF_EVENT_ERR(args...) EVENT_LOG(EVENT_LOG_TAG_MPF_ERR, args)
#define WL_MPF_EVENT_WARN(args...) EVENT_LOG(EVENT_LOG_TAG_MPF_WARN, args)
#define WL_MPF_EVENT_INFO(args...) EVENT_LOG(EVENT_LOG_TAG_MPF_INFO, args)
#define WL_MPF_EVENT_DEBUG(args...) EVENT_LOG(EVENT_LOG_TAG_MPF_DEBUG, args)

#else /* EVENT_LOG_COMPILE */

#define WL_MPF_ERR(arg) WL_ERROR(arg)
#define WL_MPF_WARN(arg) WL_INFORM(arg)
#define WL_MPF_INFO(arg) WL_INFORM(arg)
#define WL_MPF_DEBUG(arg) WL_TRACE(arg)

#endif /* EVENT_LOG_COMPILE */

#else /* WL_MPF */

#define WL_MPF_ERR(arg)
#define WL_MPF_WARN(arg)
#define WL_MPF_INFO(arg)
#define WL_MPF_DEBUG(arg)

#endif /* WL_MPF */

#endif  /* _wlc_mpf_h_ */
