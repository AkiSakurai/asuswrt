/*
 * ACPHY Miscellaneous module implementation - iovar handlers & registration
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
 * $Id: $
 */

#include <phy_ac_misc_iov.h>
#include <phy_ac_misc.h>
#include <wlc_iocv_reg.h>
#include <phy_ac_info.h>

/* iovar ids */
enum {
	IOV_PHY_RXGAINERR_2G = 1,
	IOV_PHY_RXGAINERR_5GL = 2,
	IOV_PHY_RXGAINERR_5GM = 3,
	IOV_PHY_RXGAINERR_5GH = 4,
	IOV_PHY_RXGAINERR_5GU = 5,
	IOV_PHY_RUD_AGC_ENABLE = 6,
	IOV_PHY_RXFDIQI = 7,
	IOV_PHY_VASIP_SC = 8,
	IOV_PHY_RXGAINERR_6G = 10
};

static const bcm_iovar_t phy_ac_misc_iovars[] = {
#ifdef WLTEST
	{"phy_rxgainerr_2g", IOV_PHY_RXGAINERR_2G,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_vasip_sc", IOV_PHY_VASIP_SC,
	(IOVF_SET_UP), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_rxgainerr_5gl", IOV_PHY_RXGAINERR_5GL,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_rxgainerr_5gm", IOV_PHY_RXGAINERR_5GM,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_rxgainerr_5gh", IOV_PHY_RXGAINERR_5GH,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_rxgainerr_5gu", IOV_PHY_RXGAINERR_5GU,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 4*sizeof(int8)},
	{"phy_force_rxfdiqi", IOV_PHY_RXFDIQI,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, sizeof(int8)},
#endif /* WLTEST */
	{"rud_agc_enable", IOV_PHY_RUD_AGC_ENABLE,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), 0, IOVT_INT16, 0},
	{"phy_rxgainerr_6g", IOV_PHY_RXGAINERR_6G,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), 0, IOVT_BUFFER, 5*sizeof(int8)},
	{NULL, 0, 0, 0, 0, 0}
};

#include <wlc_patch.h>

static int
phy_ac_misc_doiovar(void *ctx, uint32 aid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{

	phy_info_t *pi = (phy_info_t *)ctx;
	int err = BCME_OK;
	int8 *setDeltaValues = p, *getDeltaValues = a;
	uint8 core;
#ifdef WLTEST
	uint8 actvcore;
	phy_stf_data_t *stf_shdata = phy_stf_get_data(pi->stfi);
	uint8 rx_coremask = stf_shdata->hw_phyrxchain;
#endif /* WLTEST */

	BCM_REFERENCE(getDeltaValues);
	BCM_REFERENCE(setDeltaValues);
	BCM_REFERENCE(core);

	switch (aid) {
#ifdef WLTEST
	case IOV_SVAL(IOV_PHY_RXGAINERR_2G):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				pi->rxgainerr_2g[core] = setDeltaValues[actvcore];
				actvcore++;
			} else {  // assign 0dB to inactive core
				pi->rxgainerr_2g[core] = 0;
			}
		}
		pi->rxgainerr2g_isempty = FALSE;
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_2G):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				getDeltaValues[actvcore] = pi->rxgainerr_2g[core];
				actvcore++;
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_RXGAINERR_5GL):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				pi->rxgainerr_5gl[core] = setDeltaValues[actvcore];
				actvcore++;
			} else {  // assign 0dB to inactive core
				pi->rxgainerr_5gl[core] = 0;
			}
		}
		pi->rxgainerr5gl_isempty = FALSE;
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_5GL):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				getDeltaValues[actvcore] = pi->rxgainerr_5gl[core];
				actvcore++;
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_RXGAINERR_5GM):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				pi->rxgainerr_5gm[core] = setDeltaValues[actvcore];
				actvcore++;
			} else {  // assign 0dB to inactive core
				pi->rxgainerr_5gm[core] = 0;
			}
		}
		pi->rxgainerr5gm_isempty = FALSE;
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_5GM):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				getDeltaValues[actvcore] = pi->rxgainerr_5gm[core];
				actvcore++;
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_RXGAINERR_5GH):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				pi->rxgainerr_5gh[core] = setDeltaValues[actvcore];
				actvcore++;
			} else {  // assign 0dB to inactive core
				pi->rxgainerr_5gh[core] = 0;
			}
		}
		pi->rxgainerr5gh_isempty = FALSE;
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_5GH):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				getDeltaValues[actvcore] = pi->rxgainerr_5gh[core];
				actvcore++;
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_RXGAINERR_5GU):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				pi->rxgainerr_5gu[core] = setDeltaValues[actvcore];
				actvcore++;
			} else {  // assign 0dB to inactive core
				pi->rxgainerr_5gu[core] = 0;
			}
		}
		pi->rxgainerr5gu_isempty = FALSE;
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_5GU):
		actvcore = 0;
		FOREACH_CORE(pi, core) {
			 // check if core is active then only assign to active ones
			if ((rx_coremask >> core) & 0x1) {
				getDeltaValues[actvcore] = pi->rxgainerr_5gu[core];
				actvcore++;
			}
		}
		break;
	case IOV_SVAL(IOV_PHY_RXFDIQI):
		pi->u.pi_acphy->rxiqcali->fdiqi->forced_val = (int32) *setDeltaValues;
		pi->u.pi_acphy->rxiqcali->fdiqi->forced = TRUE;
		break;
	case IOV_GVAL(IOV_PHY_RXFDIQI):
		if (pi->u.pi_acphy->rxiqcali->fdiqi->enabled) {
			*getDeltaValues = 1;
		} else {
			*getDeltaValues = 0;
		}
		break;
#endif /* WLTEST */
	case IOV_SVAL(IOV_PHY_RUD_AGC_ENABLE):
		{
			int32 int_val = 0;
			if (plen >= (uint)sizeof(int_val)) {
				bcopy(p, &int_val, sizeof(int_val));
			}
			err = phy_ac_misc_set_rud_agc_enable(pi->u.pi_acphy->misci, int_val);
		}
		break;

	case IOV_GVAL(IOV_PHY_RUD_AGC_ENABLE):
		{
			int32 *ret_int_ptr = (int32 *)a;
			err = phy_ac_misc_get_rud_agc_enable(pi->u.pi_acphy->misci, ret_int_ptr);
		}
		break;
	case IOV_SVAL(IOV_PHY_VASIP_SC):
		{
			int32 int_val = 0;
			if (plen >= (uint)sizeof(int_val)) {
				bcopy(p, &int_val, sizeof(int_val));
			}
			wlc_vasip_sample_capture_set(pi, int_val);
		}
		break;

	case IOV_SVAL(IOV_PHY_RXGAINERR_6G):
		{
			uint8 sb = setDeltaValues[0]; // args: subband core0 core1 core2 core3
			FOREACH_CORE(pi, core) {
				pi->rxgainerr_6g[core][sb] = setDeltaValues[core+1];
			}
			pi->rxgainerr6g_isempty[sb] = FALSE;
		}
		break;

	case IOV_GVAL(IOV_PHY_RXGAINERR_6G):
		{
			uint8 sb = (CHSPEC_CHANNEL(pi->radio_chanspec) < 33) ? 0:
			     (CHSPEC_CHANNEL(pi->radio_chanspec) < 65) ? 1:
			     (CHSPEC_CHANNEL(pi->radio_chanspec) < 97) ? 2:
			     (CHSPEC_CHANNEL(pi->radio_chanspec) < 129) ? 3:
			     (CHSPEC_CHANNEL(pi->radio_chanspec) < 161) ? 4:
			     (CHSPEC_CHANNEL(pi->radio_chanspec) < 193) ? 5: 6;
			getDeltaValues[0] = sb;
			FOREACH_CORE(pi, core) {
				getDeltaValues[core+1] = pi->rxgainerr_6g[core][sb];
			}
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* register iovar table to the system */
int
BCMATTACHFN(phy_ac_misc_register_iovt)(phy_info_t *pi, wlc_iocv_info_t *ii)
{
	wlc_iovt_desc_t iovd;
#if defined(WLC_PATCH_IOCTL)
	wlc_iov_disp_fn_t disp_fn = IOV_PATCH_FN;
	const bcm_iovar_t *patch_table = IOV_PATCH_TBL;
#else
	wlc_iov_disp_fn_t disp_fn = NULL;
	const bcm_iovar_t* patch_table = NULL;
#endif /* WLC_PATCH_IOCTL */

	ASSERT(ii != NULL);

	wlc_iocv_init_iovd(phy_ac_misc_iovars,
	                   NULL, NULL,
	                   phy_ac_misc_doiovar, disp_fn, patch_table, pi,
	                   &iovd);

	return wlc_iocv_register_iovt(ii, &iovd);
}
