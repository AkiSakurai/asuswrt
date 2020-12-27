/*
 * Monitor moude interface
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_monitor.h 467328 2014-04-03 01:23:40Z $
 */


#ifndef _WLC_MONITOR_H_
#define _WLC_MONITOR_H_

#define MONITOR_PROMISC_ENAB(_ctxt_) \
	wlc_monitor_get_mctl_promisc_bits((_ctxt_))

extern wlc_monitor_info_t *wlc_monitor_attach(wlc_info_t *wlc);
extern void wlc_monitor_detach(wlc_monitor_info_t *ctxt);
extern void wlc_monitor_promisc_enable(wlc_monitor_info_t *ctxt, bool enab);
extern uint32 wlc_monitor_get_mctl_promisc_bits(wlc_monitor_info_t *ctxt);

#endif /* _WLC_MONITOR_H_ */
