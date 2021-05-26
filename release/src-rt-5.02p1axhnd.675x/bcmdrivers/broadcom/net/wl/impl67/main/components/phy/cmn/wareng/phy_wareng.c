/*
 * WAR-engine phy module implementation
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
 * $Id: phy_wareng.c 2020-01-31 11:51:41 ql888745 $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_wareng.h>
#include <phy_wareng_api.h>

/* module private states */
struct phy_wareng_info {
	phy_info_t *pi;
	phy_type_wareng_fns_t *fns;
	bool download;
};

/* module private states memory layout */
typedef struct {
	phy_wareng_info_t wareng_info;
	phy_type_wareng_fns_t fns;
/* add other variable size variables here at the end */
} phy_wareng_mem_t;

/* attach/detach */
phy_wareng_info_t *
BCMATTACHFN(phy_wareng_attach)(phy_info_t *pi)
{
	phy_wareng_info_t *wareng_info;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate attach info storage */
	if ((wareng_info = phy_malloc(pi, sizeof(phy_wareng_mem_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	wareng_info->pi = pi;
	wareng_info->fns = &((phy_wareng_mem_t *)wareng_info)->fns;

	return wareng_info;

	/* error */
fail:
	phy_wareng_detach(wareng_info);
	return NULL;
}

void
BCMATTACHFN(phy_wareng_detach)(phy_wareng_info_t *wareng_info)
{
	phy_info_t *pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (wareng_info == NULL) {
		PHY_INFORM(("%s: null wareng module\n", __FUNCTION__));
		return;
	}

	pi = wareng_info->pi;

	phy_mfree(pi, wareng_info, sizeof(phy_wareng_mem_t));
}

/* register phy type specific implementations */
int
BCMATTACHFN(phy_wareng_register_impl)(phy_wareng_info_t *wareng_info, phy_type_wareng_fns_t *fns)
{
	PHY_TRACE(("%s\n", __FUNCTION__));

	*wareng_info->fns = *fns;
	return BCME_OK;
}

void
BCMATTACHFN(phy_wareng_unregister_impl)(phy_wareng_info_t *wareng_info)
{
	PHY_TRACE(("%s\n", __FUNCTION__));
}

int
phy_wareng_download(phy_info_t *pi, uint16 tblLen, const uint32 *tblData)
{
	phy_type_wareng_fns_t *fns = pi->warengi->fns;

	PHY_TRACE(("%s\n", __FUNCTION__));
	/* redirect the request to PHY type specific implementation */
	return (fns->wareng_download)(fns->ctx, tblLen, tblData);
}
