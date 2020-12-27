
/*
 * Copyright (C) 2019, Broadcom. All Rights Reserved.
 *
 * This source code was modified by Broadcom. It is distributed under the
 * original license terms described below.
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * Copyright (c) 2002-2006 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
/*
 *
 * $Id: wl_mdns.h 596126 2015-10-29 19:53:48Z $
 *
 * Define interface into mdns from wl driver
 *
 */

#ifndef _wl_mdns_h_
#define _wl_mdns_h_

/* This file defines interface to wl driver integration */
extern wlc_mdns_info_t * wlc_mdns_attach(wlc_info_t *wlc);
extern void wl_mdns_detach(wlc_mdns_info_t *mdnsi);
extern bool wl_mDNS_Init(wlc_mdns_info_t *mdnsi, uint8 *dbase, uint32 dbase_len);
extern uint32 mdns_rx(wlc_mdns_info_t *mdnsi, void *pkt, uint16 len);
extern bool wl_mDNS_Exit(wlc_mdns_info_t *mdns);
#endif /* _wl_mdns_h_ */
