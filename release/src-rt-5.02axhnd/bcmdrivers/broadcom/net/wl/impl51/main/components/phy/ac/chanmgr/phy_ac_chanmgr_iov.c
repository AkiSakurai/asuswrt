/*
 * ACPHY Channel Manager module implementation - iovar handlers & registration
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
 * $Id: phy_ac_chanmgr_iov.c 775352 2019-05-28 22:19:51Z $
 */

#include <phy_ac_chanmgr_iov.h>
#include <phy_ac_chanmgr.h>
#include <wlc_iocv_reg.h>
#include <phy_ac_info.h>

/* iovar ids */
enum {
	IOV_PHY_CLBPRIO_2G = 1,
	IOV_PHY_CLBPRIO_5G = 2,
	IOV_PHYMODE = 3,
	IOV_SC_CHAN = 4,
	IOV_PHY_VCORE = 5,
	IOV_TD_SFO = 6,
	IOV_TDCS_160M = 7,
	IOV_PHY_CHANUP = 8,
	IOV_UL_MAC_AIDED = 9,
	IOV_UL_MAC_AIDED_TIMING = 10,
	IOV_PHY_C2C_SYNC = 11,
	IOV_PHY_LOWRATETSSI = 12,
	IOV_PHY_LOWRATETSSI_OVRD = 13
};

static const bcm_iovar_t phy_ac_chanmgr_iovars[] = {
	{"phy_clbprio2g", IOV_PHY_CLBPRIO_2G, 0, 0, IOVT_INT32, 0},
	{"phy_clbprio5g", IOV_PHY_CLBPRIO_5G, 0, 0, IOVT_INT32, 0},
#if defined(BCMDBG) || defined(WLTEST)
	{"phymode", IOV_PHYMODE, 0, 0, IOVT_UINT16, 0},
	{"sc_chan", IOV_SC_CHAN, 0, 0, IOVT_UINT16, 0},
	{"td_sfo", IOV_TD_SFO, 0, 0, IOVT_UINT16, 0},
	{"tdcs_160m", IOV_TDCS_160M, 0, 0, IOVT_UINT32, 0},
	{"phy_chanup", IOV_PHY_CHANUP, 0, 0, IOVT_UINT32, 0},
	{"ul_mac_aided", IOV_UL_MAC_AIDED, 0, 0, IOVT_UINT16, 0},
	{"ul_mac_aided_timing", IOV_UL_MAC_AIDED_TIMING, 0, 0, IOVT_UINT16, 0},
	{"phy_c2c_sync", IOV_PHY_C2C_SYNC, IOVF_SET_UP | IOVF_GET_UP, 0, IOVT_INT16, 0},
	{"phy_lowratetssi", IOV_PHY_LOWRATETSSI, 0, 0, IOVT_INT8, 0},
	{"phy_lowratetssi_ovrd", IOV_PHY_LOWRATETSSI_OVRD, IOVF_SET_DOWN, 0, IOVT_UINT32, 0},
#endif /* BCMDBG  || WLTEST */
	{"phy_vcore", IOV_PHY_VCORE, 0, 0, IOVT_UINT16, 0},
	{NULL, 0, 0, 0, 0, 0}
};

#include <wlc_patch.h>

static int
phy_ac_chanmgr_doiovar(void *ctx, uint32 aid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	phy_info_t *pi = (phy_info_t *)ctx;
	int32 int_val = 0;
	int err = BCME_OK;
	int32 *ret_int_ptr = (int32 *)a;
	phy_ac_chanmgr_info_t *chanmgri = pi->u.pi_acphy->chanmgri;
	phy_ac_info_t *aci = pi->u.pi_acphy;
	BCM_REFERENCE(chanmgri);
	BCM_REFERENCE(aci);

	if (plen >= (uint)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (aid) {
		case IOV_GVAL(IOV_PHY_CLBPRIO_2G):
			*ret_int_ptr = wlc_phy_femctrl_clb_prio_2g_acphy(pi, FALSE, 0);
			break;
		case IOV_SVAL(IOV_PHY_CLBPRIO_2G):
			*ret_int_ptr = wlc_phy_femctrl_clb_prio_2g_acphy(pi, TRUE, int_val);
			break;
		case IOV_GVAL(IOV_PHY_CLBPRIO_5G):
			*ret_int_ptr = wlc_phy_femctrl_clb_prio_5g_acphy(pi, FALSE, 0);
			break;
		case IOV_SVAL(IOV_PHY_CLBPRIO_5G):
			*ret_int_ptr = wlc_phy_femctrl_clb_prio_5g_acphy(pi, TRUE, int_val);
			break;
#if defined(BCMDBG) || defined(WLTEST)
#if ACCONF || ACCONF2
		case IOV_GVAL(IOV_PHYMODE): {
			err = phy_ac_chanmgr_get_val_phymode(chanmgri, ret_int_ptr);
			break;
		}
		case IOV_SVAL(IOV_PHYMODE): {
			err = phy_ac_chanmgr_set_val_phymode(chanmgri, int_val);
			break;
		}
		case IOV_GVAL(IOV_SC_CHAN): {
			err = phy_ac_chanmgr_get_val_sc_chspec(chanmgri, ret_int_ptr);
			break;
		}
		case IOV_SVAL(IOV_SC_CHAN): {
			err = phy_ac_chanmgr_set_val_sc_chspec(chanmgri, int_val);
			break;
		}
		case IOV_GVAL(IOV_PHY_VCORE): {
			err = phy_ac_chanmgr_get_val_phy_vcore(chanmgri, ret_int_ptr);
			break;
		}
		case IOV_SVAL(IOV_TD_SFO): {
			*ret_int_ptr = phy_ac_chanmgr_force_td_sfo(pi, TRUE, int_val);
			break;
		}
		case IOV_GVAL(IOV_TD_SFO): {
			*ret_int_ptr = phy_ac_chanmgr_force_td_sfo(pi, FALSE, 0);
			break;
		}
		case IOV_SVAL(IOV_TDCS_160M): {
			*ret_int_ptr = phy_ac_chanmgr_force_tdcs_160m(pi, TRUE, int_val);
			break;
		}
		case IOV_GVAL(IOV_TDCS_160M): {
			*ret_int_ptr = phy_ac_chanmgr_force_tdcs_160m(pi, FALSE, 0);
			break;
		}
		case IOV_SVAL(IOV_PHY_CHANUP): {
			err = phy_ac_chanmgr_iovar_set_chanup_ovrd(pi, int_val);
			break;
		}
		case IOV_GVAL(IOV_PHY_CHANUP): {
			err = phy_ac_chanmgr_iovar_get_chanup_ovrd(pi, ret_int_ptr);
			break;
		}
		case IOV_SVAL(IOV_UL_MAC_AIDED): {
			*ret_int_ptr = phy_ac_chanmgr_enable_mac_aided(pi, TRUE, int_val);
			break;
		}
		case IOV_GVAL(IOV_UL_MAC_AIDED): {
			*ret_int_ptr = phy_ac_chanmgr_enable_mac_aided(pi, FALSE, 0);
			break;
		}
		case IOV_SVAL(IOV_UL_MAC_AIDED_TIMING): {
			*ret_int_ptr = phy_ac_chanmgr_enable_mac_aided_timing(pi, TRUE, int_val);
			break;
		}
		case IOV_GVAL(IOV_UL_MAC_AIDED_TIMING): {
			*ret_int_ptr = phy_ac_chanmgr_enable_mac_aided_timing(pi, FALSE, 0);
			break;
		}
		case IOV_SVAL(IOV_PHY_C2C_SYNC): {
			if ((ACMAJORREV_32(pi->pubpi->phy_rev) && ACMINORREV_2(pi)) ||
			    ACMAJORREV_33(pi->pubpi->phy_rev) ||
			    ACMAJORREV_GE47(pi->pubpi->phy_rev)) {
				switch (int_val) {
				case 0:
				case 1:
					if (!aci->c2c_sync_override) {
						aci->c2c_sync_saved = aci->c2c_sync_en;
						aci->c2c_sync_override = TRUE;
					}
					aci->c2c_sync_en = int_val;
					phy_ac_chanmgr_core2core_sync_setup(chanmgri,
					                                    (bool) int_val);
					break;
				case AUTO:
					if (aci->c2c_sync_override) {
						aci->c2c_sync_en = aci->c2c_sync_saved;
						aci->c2c_sync_override = FALSE;
						phy_ac_chanmgr_core2core_sync_setup(
						        chanmgri, (bool)(aci->c2c_sync_en));
					}
					break;
				default:
					err = BCME_RANGE;
					break;
				}
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}
		case IOV_GVAL(IOV_PHY_C2C_SYNC): {
			*ret_int_ptr = ((uint8)(aci->c2c_sync_override) << 4) + aci->c2c_sync_en;
			break;
		}
		case IOV_GVAL(IOV_PHY_LOWRATETSSI):
			err = phy_ac_chanmgr_iovar_get_lowratetssi(chanmgri, ret_int_ptr);
			break;
		case IOV_GVAL(IOV_PHY_LOWRATETSSI_OVRD):
			err = phy_ac_chanmgr_iovar_get_lowratetssi_ovrd(chanmgri, ret_int_ptr);
		case IOV_SVAL(IOV_PHY_LOWRATETSSI_OVRD):
			err = phy_ac_chanmgr_iovar_set_lowratetssi_ovrd(chanmgri, int_val);
			break;
#endif /* ACCONF || ACCONF2 */
#endif /* defined(BCMDBG) ||  defined(WLTEST) */

		default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

/* register iovar table to the system */
int
BCMATTACHFN(phy_ac_chanmgr_register_iovt)(phy_info_t *pi, wlc_iocv_info_t *ii)
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

	wlc_iocv_init_iovd(phy_ac_chanmgr_iovars,
	                   NULL, NULL,
	                   phy_ac_chanmgr_doiovar, disp_fn, patch_table, pi,
	                   &iovd);

	return wlc_iocv_register_iovt(ii, &iovd);
}
