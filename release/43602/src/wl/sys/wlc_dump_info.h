/*
 * Common (OS-independent) info dump definitions for
 * Broadcom 802.11abg Networking Device Driver
 * an extension of wlc.c
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc.h 449873 2014-01-20 07:45:51Z $
 */

#ifndef WLC_DUMP_INFO_H_
#define WLC_DUMP_INFO_H_

/* attach/detach */
extern int wlc_dump_info_attach(wlc_info_t *wlc);
extern void wlc_dump_info_detach(wlc_info_t *wlc);

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP) || \
	defined(WLTEST) || defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || \
	defined(BCMDBG_DUMP_RSSI) || defined(MCHAN_MINIDUMP) || defined(BCMDBG_TXBF)
/* external APIs moved from wlc.c */
extern int
wlc_iovar_dump(wlc_info_t *wlc, const char *params, int p_len, char *out_buf, int out_len);
#endif 
#ifndef LINUX_POSTMOGRIFY_REMOVAL
extern int wlc_dump_register(wlc_pub_t *pub, const char *name, dump_fn_t dump_fn,
                             void *dump_fn_arg);
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#endif /* WLC_DUMP_INFO_H_ */
