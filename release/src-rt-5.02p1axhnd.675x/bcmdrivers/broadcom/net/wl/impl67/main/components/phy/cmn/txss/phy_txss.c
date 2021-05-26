/*
 * Transmitter Spectral Shaping module implementation.
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
 * $Id: phy_txss.c 821009 2019-05-21 23:21:20Z $
 */

#include <phy.h>
#include <phy_ac_info.h>
#include <phy_mem.h>
#include <phy_type_txss.h>

/* module private states */
struct phy_txss_info {
	phy_info_t *pi;
	phy_type_txss_fns_t *fns;
};

/* module private states memory layout */
typedef struct {
	phy_txss_info_t txssi;
	phy_type_txss_fns_t fns;
/* add other variable size variables here at the end */
} phy_txss_mem_t;

/* local function declaration */

/* attach/detach */
phy_txss_info_t *
BCMATTACHFN(phy_txss_attach)(phy_info_t *pi)
{
	phy_txss_info_t *txssi;

	PHY_TRACE(("phy_txss_attach\n"));

	/* allocate attach info storage */
	if ((txssi = phy_malloc(pi, sizeof(phy_txss_mem_t))) == NULL) {
		PHY_ERROR(("phy_txss_attach: phy_malloc failed\n"));
		goto fail;
	}
	txssi->pi = pi;
	txssi->fns = &((phy_txss_mem_t *)txssi)->fns;

	return txssi;

	/* error */
fail:
	phy_txss_detach(txssi);
	return NULL;
}

void
BCMATTACHFN(phy_txss_detach)(phy_txss_info_t *txssi)
{
	phy_info_t *pi;

	PHY_TRACE(("phy_txss_detach\n"));

	if (txssi == NULL) {
		PHY_INFORM(("phy_txss_detach: null txss module\n"));
		return;
	}

	pi = txssi->pi;

	phy_mfree(pi, txssi, sizeof(phy_txss_mem_t));
}

/* register phy type specific implementations */
int
BCMATTACHFN(phy_txss_register_impl)(phy_txss_info_t *txssi, phy_type_txss_fns_t *fns)
{
	PHY_TRACE(("phy_txss_register_impl\n"));

	*txssi->fns = *fns;

	return BCME_OK;
}

void
BCMATTACHFN(phy_txss_unregister_impl)(phy_txss_info_t *txssi)
{
	PHY_TRACE(("phy_txss_unregister_impl\n"));
}
