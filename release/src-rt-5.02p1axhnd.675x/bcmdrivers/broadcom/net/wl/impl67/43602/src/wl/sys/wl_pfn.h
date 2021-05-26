/*
 * Preferred network header file
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
 * $Id: wl_pfn.h 494426 2014-08-01 00:18:05Z $
 */

#ifndef _wl_pfn_h_
#define _wl_pfn_h_

/* This define is to help mogrify the code */
#ifdef WLPFN

/* forward declaration */
typedef struct wl_pfn_info wl_pfn_info_t;
typedef struct wl_pfn_bdcast_list wl_pfn_bdcast_list_t;

/* Function prototype */
wl_pfn_info_t * wl_pfn_attach(wlc_info_t * wlc);
int wl_pfn_detach(wl_pfn_info_t * data);
extern void wl_pfn_event(wl_pfn_info_t * data, wlc_event_t * e);
extern int wl_pfn_scan_in_progress(wl_pfn_info_t * data);
extern void wl_pfn_process_scan_result(wl_pfn_info_t * data, wlc_bss_info_t * bi);
extern bool wl_pfn_scan_state_enabled(wlc_info_t *wlc);
extern bool wl_pfn_is_reporting_enabled(wl_pfn_info_t *pfn_info, uint16 bssid_channel,
             uint32 report_type);

#if defined(EVENT_LOG_COMPILE)

#define WL_PFN_ERR(arg) WL_PFN_EVENT_ERR arg
#define WL_PFN_WARN(arg) WL_PFN_EVENT_WARN arg
#define WL_PFN_INFO(arg) WL_PFN_EVENT_INFO arg
#define WL_PFN_DEBUG(arg) WL_PFN_EVENT_DEBUG arg

#define WL_PFN_EVENT_ERR(args...) EVENT_LOG(EVENT_LOG_TAG_PFN_ERR, args)
#define WL_PFN_EVENT_WARN(args...) EVENT_LOG(EVENT_LOG_TAG_PFN_WARN, args)
#define WL_PFN_EVENT_INFO(args...) EVENT_LOG(EVENT_LOG_TAG_PFN_INFO, args)
#define WL_PFN_EVENT_DEBUG(args...) EVENT_LOG(EVENT_LOG_TAG_PFN_DEBUG, args)

#else /* EVENT_LOG_COMPILE */

#define WL_PFN_ERR(arg) WL_ERROR(arg)
#define WL_PFN_WARN(arg) WL_INFORM(arg)
#define WL_PFN_INFO(arg) WL_INFORM(arg)
#define WL_PFN_DEBUG(arg) WL_TRACE(arg)

#endif /* EVENT_LOG_COMPILE */

#else /* WLPFN */

#define WL_PFN_ERR(arg)
#define WL_PFN_WARN(arg)
#define WL_PFN_INFO(arg)
#define WL_PFN_DEBUG(arg)

#endif /* WLPFN */

#endif /* _wl_pfn_h_ */
