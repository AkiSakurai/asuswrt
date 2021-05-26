/*
 * Host Clock Sync using TSF counter.
 *
 *      This feature implements host clock sync using TSF.
 *
 *
 *   Copyright 2020 Broadcom
 *
 *   This program is the proprietary software of Broadcom and/or
 *   its licensors, and may only be used, duplicated, modified or distributed
 *   pursuant to the terms and conditions of a separate, written license
 *   agreement executed between you and Broadcom (an "Authorized License").
 *   Except as set forth in an Authorized License, Broadcom grants no license
 *   (express or implied), right to use, or waiver of any kind with respect to
 *   the Software, and Broadcom expressly reserves all rights in and to the
 *   Software and all intellectual property rights therein.  IF YOU HAVE NO
 *   AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *   WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *   THE SOFTWARE.
 *
 *   Except as expressly set forth in the Authorized License,
 *
 *   1. This program, including its structure, sequence and organization,
 *   constitutes the valuable trade secrets of Broadcom, and you shall use
 *   all reasonable efforts to protect the confidentiality thereof, and to
 *   use this information only in connection with your use of Broadcom
 *   integrated circuit products.
 *
 *   2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *   "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *   REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *   OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *   DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *   NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *   ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *   CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *   OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *   3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *   BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *   SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *   IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *   IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *   ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *   OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *   NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
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
