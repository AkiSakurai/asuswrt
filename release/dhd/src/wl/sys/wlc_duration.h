/*
 * wlc_duration.h
 *
 * This module provides definitions for a basic profiler.
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


#if !defined(__wlc_duration_h__)
#define __wlc_duration_h__

typedef enum {
	DUR_WATCHDOG = 0,
	DUR_DPC,
	DUR_DPC_TXSTATUS,
	DUR_DPC_TXSTATUS_SENDQ,
	DUR_DPC_RXFIFO,
	DUR_SENDQ,
	DUR_LAST
} duration_enum;
/* Update definition of duration_names[] accordingly */

extern duration_info_t *BCMATTACHFN(wlc_duration_attach)(wlc_info_t *);
extern int BCMATTACHFN(wlc_duration_detach)(duration_info_t *);

extern void wlc_duration_enter(wlc_info_t *wlc, duration_enum idx);
extern void wlc_duration_exit(wlc_info_t *wlc, duration_enum idx);

#endif /* __wlc_duration_h__ */
