/*
 * Preferred network header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_pfn.h 453919 2014-02-06 23:10:30Z $
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
