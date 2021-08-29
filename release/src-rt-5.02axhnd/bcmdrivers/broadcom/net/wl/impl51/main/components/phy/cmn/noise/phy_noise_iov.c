/*
 * NOISEmeasure module implementation - iovar handlers & registration
 *
 * Copyright 2019 Broadcom
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
 * $Id: phy_noise_iov.c 775453 2019-05-30 14:48:52Z $
 */

#include <phy_noise_iov.h>
#include <phy_noise.h>
#include <wlc_iocv_reg.h>

/* iovar ids */
enum {
	IOV_PHYNOISE_SROM = 1,
#ifdef WL_EAP_NOISE_MEASUREMENTS
	IOV_PHYNOISE_BIAS2GLO = 2,
	IOV_PHYNOISE_BIAS2GHI = 3,
	IOV_PHYNOISE_BIAS5GLO = 4,
	IOV_PHYNOISE_BIAS5GHI = 5,
	IOV_PHYNOISE_BIASRADARHI = 6,
	IOV_PHYNOISE_BIASRADARLO = 7,
	IOV_PHYNOISE_BIASRXGAINERR = 8,
#endif /* WL_EAP_NOISE_MEASUREMENTS */
	IOV_PHYNOISE_LAST
};

/* iovar table */
static const bcm_iovar_t phy_noise_iovars[] = {
	{"phynoise_srom", IOV_PHYNOISE_SROM, IOVF_GET_UP, 0, IOVT_UINT32, 0},
#ifdef WL_EAP_NOISE_MEASUREMENTS
	/* Noise Bias IOVARs raise or the lower the calculated noise floor */
	{"phynoisebias2glo", IOV_PHYNOISE_BIAS2GLO, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebias2ghi", IOV_PHYNOISE_BIAS2GHI, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebias5glo", IOV_PHYNOISE_BIAS5GLO, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebias5ghi", IOV_PHYNOISE_BIAS5GHI, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebiasradarhi", IOV_PHYNOISE_BIASRADARHI, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebiasradarlo", IOV_PHYNOISE_BIASRADARLO, IOVF_GET_UP, 0, IOVT_INT8, 0},
	{"phynoisebiasrxgainerr", IOV_PHYNOISE_BIASRXGAINERR, IOVF_GET_UP, 0, IOVT_BOOL, 0},
#endif /* WL_EAP_NOISE_MEASUREMENTS */
	{NULL, 0, 0, 0, 0, 0}
};

#include <wlc_patch.h>

static int
phy_noise_doiovar(void *ctx, uint32 aid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	phy_info_t *pi = (phy_info_t *)ctx;
	int32 int_val = 0;
	int err = BCME_OK;
	int32 *ret_int_ptr = (int32 *)a;

	if (plen >= (uint)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (aid) {
	case IOV_GVAL(IOV_PHYNOISE_SROM):
		err = phy_noise_get_srom_level(pi, ret_int_ptr);
		break;

#ifdef WL_EAP_NOISE_MEASUREMENTS
	/* Noise Bias IOVARs raise or the lower the calculated noise floor */
	case IOV_GVAL(IOV_PHYNOISE_BIAS2GLO):
		*ret_int_ptr = phy_noise_get_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_LO, NOISE_BIAS_BAND_2G);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIAS2GLO):
		phy_noise_set_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_LO, NOISE_BIAS_BAND_2G, int_val);
		break;
	case IOV_GVAL(IOV_PHYNOISE_BIAS2GHI):
		*ret_int_ptr = phy_noise_get_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_HI, NOISE_BIAS_BAND_2G);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIAS2GHI):
		phy_noise_set_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_HI, NOISE_BIAS_BAND_2G, int_val);
		break;

	case IOV_GVAL(IOV_PHYNOISE_BIAS5GLO):
		*ret_int_ptr = phy_noise_get_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_LO, NOISE_BIAS_BAND_5G);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIAS5GLO):
		phy_noise_set_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_LO, NOISE_BIAS_BAND_5G, int_val);
		break;
	case IOV_GVAL(IOV_PHYNOISE_BIAS5GHI):
		*ret_int_ptr = phy_noise_get_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_HI, NOISE_BIAS_BAND_5G);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIAS5GHI):
		phy_noise_set_gain_bias(pi,
			NOISE_BIAS_GAINTYPE_HI, NOISE_BIAS_BAND_5G, int_val);
		break;
	case IOV_GVAL(IOV_PHYNOISE_BIASRADARLO):
		*ret_int_ptr = phy_noise_get_radar_gain_bias(pi, NOISE_BIAS_GAINTYPE_LO);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIASRADARLO):
		phy_noise_set_radar_gain_bias(pi, NOISE_BIAS_GAINTYPE_LO, int_val);
		break;
	case IOV_GVAL(IOV_PHYNOISE_BIASRADARHI):
		*ret_int_ptr = phy_noise_get_radar_gain_bias(pi, NOISE_BIAS_GAINTYPE_HI);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIASRADARHI):
		phy_noise_set_radar_gain_bias(pi, NOISE_BIAS_GAINTYPE_HI, int_val);
		break;

	case IOV_GVAL(IOV_PHYNOISE_BIASRXGAINERR):
		*ret_int_ptr = phy_noise_get_rxgainerr_bias(pi);
		break;
	case IOV_SVAL(IOV_PHYNOISE_BIASRXGAINERR):
		phy_noise_set_rxgainerr_bias(pi, int_val);
		break;
#endif /* WL_EAP_NOISE_MEASUREMENTS */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* register iovar table to the system */
int
BCMATTACHFN(phy_noise_register_iovt)(phy_info_t *pi, wlc_iocv_info_t *ii)
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

	wlc_iocv_init_iovd(phy_noise_iovars,
	                   NULL, NULL,
	                   phy_noise_doiovar, disp_fn, patch_table, pi,
	                   &iovd);

	return wlc_iocv_register_iovt(ii, &iovd);
}
