/*
 * Selected Management Frame Stats feature interface
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_smfs.h 599296 2015-11-13 06:36:13Z $
 */

#ifndef _wlc_smfs_h_
#define _wlc_smfs_h_

#include <typedefs.h>
#include <wlc_types.h>

/* Per-BSS FEATURE_ENAB() macro */
/* XXX It's used only in management path hence leave the runtime check
 * inside the module i.e. the performance is not an issue.
 */
#ifdef SMF_STATS
#define BSS_SMFS_ENAB(wlc, cfg) SMFS_ENAB((wlc)->pub) && wlc_smfs_enab((wlc)->smfs, cfg)
#else
#define BSS_SMFS_ENAB(wlc, cfg) FALSE
#endif /* SMF_STATS */

/* Module attach/detach interface */
wlc_smfs_info_t *wlc_smfs_attach(wlc_info_t *wlc);
void wlc_smfs_detach(wlc_smfs_info_t *smfs);

/* Functional interfaces */

#ifdef SMF_STATS

/* Check if the feature is enabled */
bool wlc_smfs_enab(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg);

/* Update the stats.
 *
 * Input Params
 * - 'type' is defined in wlioctl.h. See smfs_type enum.
 * - 'code' is defined in wlioctl.h. See SMFS_CODETYPE_XXX.
 *
 * Return BCME_XXXX.
 */
int wlc_smfs_update(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg, uint8 type, uint16 code);

#else /* !SMF_STATS */

/* For compilers that don't eliminate unused code */

#define wlc_smfs_enab(smfs, cfg) FALSE

static int wlc_smfs_update(wlc_smfs_info_t *smfs, wlc_bsscfg_t *cfg, uint8 type, uint16 code)
{
	BCM_REFERENCE(smfs);
	BCM_REFERENCE(cfg);
	BCM_REFERENCE(type);
	BCM_REFERENCE(code);
	return BCME_OK;
}

#endif /* !SMF_STATS */

#endif	/* _wlc_smfs_h_ */
