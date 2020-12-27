/*
 * Host Clock Sync using TSF counter.
 *
 *      This feature implements host clock sync using TSF.
 *
 *
 *   Copyright (C) 2014, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 *   $Id: wlc_hostclksync.h, pavank Exp $
 */

#ifndef _wlc_hostclksync_h_
#define _wlc_hostclksync_h_

/* ---- Include Files ---------------------------------------------------- */
#include <wlc_types.h>

/* ---- Constants and Types ---------------------------------------------- */
#define WL_HOST_CLK_SYNC_ATTACH_ERR			124
#ifndef WL_HOST_CLK_SYNC
#define wlc_host_clk_sync_update(hostclksync_info)	do { } while (0)
#define wlc_host_clk_sync_reset(hostclksync_info)	do { } while (0)
#else

/*
*****************************************************************************
* Function:   wlc_host_clk_sync_attach
*
* Purpose:    Initialize Host Clock Sync private context.
*
* Parameters: wlc       (mod)   Common driver context.
*
* Returns:    Pointer to the TSF Sync private context. Returns NULL on error.
*****************************************************************************
*/
extern wlc_host_clk_sync_info_t *wlc_host_clk_sync_attach(wlc_info_t *wlc);


/*
*****************************************************************************
* Function:   wlc_host_clk_sync_detach
*
* Purpose:    Cleanup Host Clock Sync private context.
*
* Parameters: info      (mod)   Host Clock Sync private context.
*
* Returns:    Nothing.
*****************************************************************************
*/
extern void wlc_host_clk_sync_detach(wlc_host_clk_sync_info_t *info);

extern void wlc_host_clk_sync_update(wlc_host_clk_sync_info_t *info);
extern void wlc_host_clk_sync_reset(wlc_host_clk_sync_info_t *info);
#endif /* WL_HOST_CLK_SYNC */

#endif  /* _wlc_hostclksync_h_ */
