/*
 * Motion ProFiles for WLAN functionality (initially PNO).
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
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
