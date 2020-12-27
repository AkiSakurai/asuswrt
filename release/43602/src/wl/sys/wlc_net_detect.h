/*
 * Common (OS-independent) portion of
 * Broadcom support for Intel NetDetect interface.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_net_detect.h 456199 2014-02-18 03:06:41Z $
 */

#ifndef _wlc_net_detect_h_
#define _wlc_net_detect_h_

#ifdef NET_DETECT

/*
 * Initialize net detect private context.
 * Returns a pointer to the net detect private context, NULL on failure.
 */
extern wlc_net_detect_ctxt_t *wlc_net_detect_attach(wlc_info_t *wlc);

/* Cleanup net detect private context */
extern void wlc_net_detect_detach(wlc_net_detect_ctxt_t *net_detect_ctxt);

#endif	/* NET_DETECT */

#endif  /* _wlc_net_detect_h_ */
