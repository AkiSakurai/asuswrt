/*
 * mac filter module header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_macfltr.h 467328 2014-04-03 01:23:40Z $
 */

#ifndef _wlc_macflter_h_
#define _wlc_macflter_h_

/* This module provides association control:
 * - for AP to decide if an association request is granted
 * - for STA to decide if a join target is considered
 * - for others to control the peer list
 * - ...
 */

#include <wlc_types.h>
#include <wlioctl.h>

/* module entries */
/* attach/detach */
extern wlc_macfltr_info_t *wlc_macfltr_attach(wlc_info_t *wlc);
extern void wlc_macfltr_detach(wlc_macfltr_info_t *mfi);

/* APIs */
/* check if the 'addr' is in the allow/deny list by the return code defined below */
extern int wlc_macfltr_addr_match(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg,
	const struct ether_addr *addr);

/* address match return code */
/* mac filter mode is DISABLE */
#define WLC_MACFLTR_DISABLED		0
/* mac filter mode is DENY */
#define WLC_MACFLTR_ADDR_DENY		1	/* addr is in mac list */
#define WLC_MACFLTR_ADDR_NOT_DENY	2	/* addr is not in mac list */
/* mac filter mode is ALLOW */
#define WLC_MACFLTR_ADDR_ALLOW		3	/* addr is in mac list */
#define WLC_MACFLTR_ADDR_NOT_ALLOW	4	/* addr is not in mac list */

/* set/get mac allow/deny list based on mode */
extern int wlc_macfltr_list_set(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg,
	struct maclist *maclist, uint len);
extern int wlc_macfltr_list_get(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg,
	struct maclist *maclist, uint len);

/* set/get mac list mode */
#define MFIWLC(mfi) (*(wlc_info_t **)mfi)	/* expect wlc to be the first field */
#define wlc_macfltr_mode_set(mfi, cfg, mode) \
	(wlc_ioctl(MFIWLC(mfi), WLC_SET_MACMODE, &(mode), sizeof(mode), (cfg)->wlcif))
#define wlc_macfltr_mode_get(mfi, cfg, mode) \
	(wlc_ioctl(MFIWLC(mfi), WLC_GET_MACMODE, mode, sizeof(*(mode)), (cfg)->wlcif))

#endif /* _wlc_macflter_h_ */
