/*
 * P2P Offload
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_p2po.h 343159 2012-07-05 23:48:19Z $
 */


#ifndef _wl_p2po_h_
#define _wl_p2po_h_

#include <wlc_cfg.h>
#include <d11.h>
#include <wlc_types.h>
#include <wlc_bsscfg.h>
#include <wl_eventq.h>
#include <wl_gas.h>

typedef struct wl_p2po_info wl_p2po_info_t;

/*
 * Initialize p2po private context.
 * Returns a pointer to the p2po private context, NULL on failure.
 */
extern wl_p2po_info_t *wl_p2po_attach(wlc_info_t *wlc, wl_eventq_info_t *wlevtq,
	wl_gas_info_t *gas, wl_disc_info_t *p2po_disc);

/* Cleanup p2po private context */
extern void wl_p2po_detach(wl_p2po_info_t *p2po);

#endif /* _wl_p2po_h_ */
