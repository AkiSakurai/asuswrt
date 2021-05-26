/*
 * WAR-engine module implementation
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
 * $Id: phy_ac_wareng.c 2020-01-31 11:51:41 ql888745 $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <qmath.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_ac.h>
#include <phy_ac_info.h>
#include <wlc_phyreg_ac.h>
#include <phy_utils_reg.h>
#include <phy_ac_wareng.h>

/* module private states */
struct phy_ac_wareng_info {
	phy_info_t *pi;
	phy_ac_info_t *aci;
	phy_wareng_info_t *wareng_info;
};

/* local functions */
static int phy_ac_wareng_download(phy_type_wareng_ctx_t *ctx, uint16 tblLen, const uint32 *tblData);

/* register phy type specific implementation */
phy_ac_wareng_info_t *
BCMATTACHFN(phy_ac_wareng_register_impl)
	(phy_info_t *pi, phy_ac_info_t *aci, phy_wareng_info_t *warengi)
{
	phy_ac_wareng_info_t *ac_wareng_info;
	phy_type_wareng_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage together */
	if ((ac_wareng_info = phy_malloc(pi, sizeof(phy_ac_wareng_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	ac_wareng_info->pi = pi;
	ac_wareng_info->aci = aci;
	ac_wareng_info->wareng_info = warengi;

	/* register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.wareng_download = phy_ac_wareng_download;
	fns.ctx = ac_wareng_info;

	phy_wareng_register_impl(warengi, &fns);

	return ac_wareng_info;

	/* error handling */
fail:
	if (ac_wareng_info != NULL)
		phy_mfree(pi, ac_wareng_info, sizeof(phy_ac_wareng_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_ac_wareng_unregister_impl)(phy_ac_wareng_info_t *ac_wareng_info)
{
	phy_info_t *pi = ac_wareng_info->pi;
	phy_wareng_info_t *mi = ac_wareng_info->wareng_info;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_wareng_unregister_impl(mi);

	phy_mfree(pi, ac_wareng_info, sizeof(phy_ac_wareng_info_t));
}

static int
phy_ac_wareng_download(phy_type_wareng_ctx_t *ctx, uint16 tblLen, const uint32 *tblData)
{
	phy_ac_wareng_info_t *info = (phy_ac_wareng_info_t *)ctx;
	phy_info_t *pi = info->pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_WARENGMEMTBL, tblLen, 0, 32, tblData);

	pi->warengi->download = TRUE;

	return BCME_OK;
}
