/*
 * Linux-specific portion of
 * Broadcom HWA driver
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
 * $Id:$
 */

#include <bcmutils.h>
#include <hndsoc.h>
#include <ethernet.h>
#include <bcmevent.h>
#include <hwa_lib.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <pcicfg.h>
#include <wl_export.h>

bool BCMFASTPATH
hwa_isr(wlc_info_t *wlc, bool *wantdpc)
{
	hwa_dev_t *dev = wlc->hwa_dev;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 hwaintstatus;

	if (wl_powercycle_inprogress(wlc->wl)) {
		return FALSE;
	}

	if (dev->intmask == 0x00000000) {
		return (FALSE);
	}

	/* detect cardbus removed, in power down(suspend) and in reset */
	if (DEVICEREMOVED(wlc)) {
		return (FALSE);
	}

	/* read and clear hwaintstatus and intstatus registers */
	hwaintstatus = hwa_intstatus(dev);

	/* d11 and hwa are either both enabled or both disabled */
	if (hwaintstatus || wlc_hw->macintstatus) {
		hwa_intrsoff(dev);
		if (hwaintstatus && (wlc_hw->macintstatus == 0)) {
			(void)wlc_intrsoff(wlc);
		}
	}

	/* it is not for us */
	if (hwaintstatus == 0) {
		return (FALSE);
	}

	*wantdpc |= TRUE;

	/* save interrupt status bits */
	ASSERT(dev->intstatus == 0);
	dev->intstatus = hwaintstatus;
	HWA_TRACE(("%s: HWA[%d] instatus 0x%x \n",
		__FUNCTION__, dev->unit, dev->intstatus));

	if (dev->intstatus & HWA_COMMON_INTSTATUS_TXSWRINDUPD_INT_CORE0_MASK) {
		wlc_hw->macintstatus |= MI_TFS;
	} else {
		/* Only 4a interrupt is enabled for NIC mode */
		ASSERT(0);
	}

	return (TRUE);
}

void BCMFASTPATH
hwa_wl_intrsupd(wlc_info_t *wlc)
{
	hwa_dev_t *dev = wlc->hwa_dev;
	wlc_hw_info_t *wlc_hw = wlc->hw;

	hwa_intrsupd(dev);
	if (dev->intstatus & HWA_COMMON_INTSTATUS_TXSWRINDUPD_INT_CORE0_MASK) {
		wlc_hw->macintstatus |= MI_TFS;
	}
}

uint32
hwa_si_flag(struct hwa_dev *dev)
{
	return dev->si_flag;
}

bool BCMFASTPATH
hwa_wlc_txstat_process(struct hwa_dev *dev)
{
	dev->intstatus = 0;

	(void)hwa_txstat_process(dev, 0, HWA_PROCESS_NOBOUND);

	return (dev->intstatus) ? TRUE : FALSE;
}

int
BCMATTACHFN(hwa_probe)(struct hwa_dev *dev, uint irq, uint coreid, uint unit)
{
	int ret = BCME_OK;

	HWA_FTRACE(HWA00);

	HWA_ASSERT(dev != (hwa_dev_t*)NULL);

	if (!dev)
		return BCME_BADARG;

	if (hwa_wlc_module_register(dev)) {
		HWA_ERROR(("%s wlc_module_register() failed\n", HWA00));
		ret = BCME_ERROR;
	}

	/* Initialize HWA core */
	hwa_config(dev);

	return ret;
}

void
BCMATTACHFN(hwa_osl_detach)(struct hwa_dev *dev)
{
	HWA_FTRACE(HWA00);
	// ... placeholder
}
