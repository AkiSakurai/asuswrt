/*
 * 802.11h DFS module header file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_dfs.h 467328 2014-04-03 01:23:40Z $
 */


#ifndef _wlc_dfs_h_
#define _wlc_dfs_h_

#ifdef WLDFS

/* module */
extern wlc_dfs_info_t *wlc_dfs_attach(wlc_info_t *wlc);
extern void wlc_dfs_detach(wlc_dfs_info_t *dfs);

/* others */
extern void wlc_set_dfs_cacstate(wlc_dfs_info_t *dfs, int state);
extern chanspec_t wlc_dfs_sel_chspec(wlc_dfs_info_t *dfs, bool force);
extern void wlc_dfs_reset_all(wlc_dfs_info_t *dfs);
extern int wlc_dfs_set_radar(wlc_dfs_info_t *dfs, int radar);

/* accessors */
extern uint32 wlc_dfs_get_radar(wlc_dfs_info_t *dfs);

extern uint32 wlc_dfs_get_chan_info(wlc_dfs_info_t *dfs, uint channel);

#else /* !WLDFS */

#define wlc_dfs_attach(wlc) NULL
#define wlc_dfs_detach(dfs) do {} while (0)

#define wlc_set_dfs_cacstate(dfs, state) do {} while (0)
#define wlc_dfs_sel_chspec(dfs, force) 0
#define wlc_dfs_reset_all(dfs) do {} while (0)
#define wlc_dfs_set_radar(dfs, radar)  BCME_UNSUPPORTED

#define wlc_dfs_get_radar(dfs) 0
#define wlc_dfs_get_chan_info(dfs, channel) 0

#endif /* !WLDFS */

#endif /* _wlc_dfs_h_ */
