/*
 * VASIP related functions
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_vasip.c 791189 2020-09-18 13:46:54Z $
 */

#include <wlc_cfg.h>

#if defined(WLVASIP) || defined(SAVERESTORE)
#include <typedefs.h>
#include <wlc_types.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlioctl.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <phy_vasip_api.h>
#include <phy_stf_api.h>
#include <phy_prephy_api.h>
#include <wlc_hw_priv.h>
#include <d11vasip_code.h>
#include <pcicfg.h>
#include <wl_export.h>
#include <wlc_vasip.h>
#include <wlc_dump.h>
#include <wlc_stf.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_rate.h>
#include <wlc_mutx.h>
#include <wlc_vht.h>
#include <wlc_he.h>

#define VASIP_COUNTERS_LMT              256
#define VASIP_DEFINED_COUNTER_NUM       26

#define SVMP_MEM_OFFSET_MAX_NOT_SUPPORT 0x0
#define SVMP_MEM_OFFSET_MAX_BCM4365C0   0x30000
#define SVMP_MEM_OFFSET_MAX_BCM43684    0x50020
#define SVMP_MEM_OFFSET_MAX_BCM63178    0x28020
#define SVMP_MEM_OFFSET_MAX_BCM6710     0x28020

#define SVMP_MEM_DUMP_LEN_MAX           4096
#define SVMP_ACCESS_BLOCK_SIZE          16

#define SVMP_ACCESS_VIA_PHYTBL

/* local prototypes */
#if defined(BCMDBG)
/* dump vasip counters from vasip program memory */
static int wlc_dump_vasip_counters(wlc_info_t *wlc, struct bcmstrbuf *b);

/* dump vasip status data from vasip program memory */
static int wlc_dump_vasip_status(wlc_info_t *wlc, struct bcmstrbuf *b);

/* dump mu_snr_calib_value from vasip program memory */
static int wlc_dump_mu_snr_calib(wlc_info_t *wlc, struct bcmstrbuf *b);

/* clear vasip counters */
static int wlc_vasip_counters_clear(wlc_hw_info_t *wlc_hw);

/* clear vasip error */
static int wlc_vasip_error_clear(wlc_hw_info_t *wlc_hw);

static void wlc_vasip_verify(wlc_hw_info_t *wlc_hw, const uint32 vasip_code[],
	const uint nbytes, uint32 offset, uint32 offset_tbl);
#endif // endif

#if (defined(BCMDBG) || defined(TESTBED_AP_11AX))
/* set svmp memory with a value from offset of length 'len', len is count of uint16's */
static void wlc_svmp_mem_set1(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 val);
static void wlc_svmp_mem_read1(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 *val);
#endif // endif

static void wlc_vasip_download(wlc_hw_info_t *wlc_hw, const vasip_fw_t *fw,
	uint32 vasipver, bool nopi);

/* get max SVMP address offset  */;
int wlc_svmp_mem_offset_max(wlc_hw_info_t *wlc_hw);

/* iovar table */
enum {
	IOV_VASIP_COUNTERS_CLEAR,
	IOV_SVMP_MEM,
	IOV_MU_RATE,
	IOV_MU_GROUP,
	IOV_MU_MCS_RECMD,
	IOV_MU_MCS_CAPPING,
	IOV_MU_SGI_RECMD,
	IOV_MU_SGI_RECMD_TH,
	IOV_MU_GROUP_DELAY,
	IOV_MU_PRECODER_DELAY,
	IOV_SVMP_DOWNLOAD,
	IOV_VASIP_ERROR_CLEAR,
	IOV_VASIP_LAST
};

static int
wlc_vasip_doiovar(void *context, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint vsize, struct wlc_if *wlcif);

static const bcm_iovar_t vasip_iovars[] = {
#if defined(BCMDBG)
	{"vasip_counters_clear", IOV_VASIP_COUNTERS_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"svmp_mem", IOV_SVMP_MEM,
	(0), 0, IOVT_BUFFER, sizeof(svmp_mem_t),
	},
	{"mu_rate", IOV_MU_RATE,
	(0), 0, IOVT_BUFFER, 0,
	},
#endif // endif
#if defined(BCMDBG) || defined(TESTBED_AP_11AX)
	{"mu_group", IOV_MU_GROUP,
	(0), 0, IOVT_BUFFER, 0,
	},
#endif // endif
#if defined(BCMDBG)
	{"mu_mcs_recmd", IOV_MU_MCS_RECMD,
	(0), 0, IOVT_UINT16, 0
	},
	{"mu_mcs_capping", IOV_MU_MCS_CAPPING,
	(0), 0, IOVT_UINT16, 0
	},
	{"mu_sgi_recmd", IOV_MU_SGI_RECMD,
	(0), 0, IOVT_INT16, 0
	},
	{"mu_sgi_recmd_th", IOV_MU_SGI_RECMD_TH,
	(0), 0, IOVT_UINT16, 0
	},
	{"mu_group_delay", IOV_MU_GROUP_DELAY,
	(0), 0, IOVT_UINT16, 0
	},
	{"mu_precoder_delay", IOV_MU_PRECODER_DELAY,
	(0), 0, IOVT_UINT16, 0
	},
	{"svmp_download", IOV_SVMP_DOWNLOAD,
	(0), 0, IOVT_VOID, 0
	},
	{"vasip_error_clear", IOV_VASIP_ERROR_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

void
BCMATTACHFN(wlc_vasip_detach)(wlc_vasip_info_t *vasip)
{
	wlc_info_t *wlc;

	if (!vasip)
		return;

	wlc = vasip->wlc;
	wlc_module_unregister(wlc->pub, "vasip", vasip);
	MFREE(wlc->osh, vasip, sizeof(*vasip));
}

/* Returns base address of the SVMP memory when accessed internally as if by the Dongle
 * CPU or other chip cores like M2MDMA.
 * Note: This function is a subset of the wlc_base_vasip() function. It uses the
 * DONGLEBUILD only code from that function.
 */
static uint32 *wlc_vasip_base_int(wlc_hw_info_t *wlc_hw)
{
	uint32 *vasip_mem = NULL;
	int idx;
	uint32 vasipaddr;

	/* save current core */
	idx = si_coreidx(wlc_hw->sih);
	if (si_setcore(wlc_hw->sih, ACPHY_CORE_ID, 0) != NULL) {
		/* get the VASIP memory base */
		vasipaddr = si_addrspace(wlc_hw->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0);
		/* restore core */
		(void)si_setcoreidx(wlc_hw->sih, idx);
	} else {
		/* This chip does not have a Vasip */
		WL_INFORM(("%s: wl%d: Failed to find ACPHY core \n",
			__FUNCTION__, wlc_hw->unit));
		return NULL;
	}
	vasip_mem = (uint32 *)(unsigned long)vasipaddr;
	ASSERT(vasip_mem);

	return vasip_mem;
}

/* Returns base address of the SVMP memory.
 * Depends heavily on platform
 * Variant 1 - Dongle
 * Variant 2 - NIC driver
 * Variant 3 - SoC with integrated WLAN
 */
static uint32 *
wlc_vasip_base(wlc_hw_info_t *wlc_hw)
{
#if !defined(DONGLEBUILD)
	osl_t *osh = wlc_hw->osh;
	uchar *bar_va;
	uint32 bar_size;
#endif /* DONGLEBUILD */
	uint32 *vasip_mem = NULL;
	int idx;
	uint32 vasipaddr;

	/* save current core */
	idx = si_coreidx(wlc_hw->sih);
	if (si_setcore(wlc_hw->sih, ACPHY_CORE_ID, 0) != NULL) {
		/* get the VASIP memory base */
		vasipaddr = si_addrspace(wlc_hw->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0);
		/* restore core */
		(void)si_setcoreidx(wlc_hw->sih, idx);
	} else {
		/* This chip does not have a Vasip */
		WL_INFORM(("%s: wl%d: Failed to find ACPHY core \n",
			__FUNCTION__, wlc_hw->unit));
		return NULL;
	}

#if !defined(DONGLEBUILD)
	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		bar_size = wl_pcie_bar2(wlc_hw->wlc->wl, &bar_va);

		if (bar_size) {
			OSL_PCI_WRITE_CONFIG(osh, PCI_BAR2_WIN, sizeof(uint32), vasipaddr);
		} else {
			bar_size = wl_pcie_bar1(wlc_hw->wlc->wl, &bar_va);
			OSL_PCI_WRITE_CONFIG(osh, PCI_BAR1_WIN, sizeof(uint32), vasipaddr);
		}

		BCM_REFERENCE(bar_size);
		ASSERT(bar_va != NULL && bar_size != 0);

		vasip_mem = (uint32 *)bar_va;
	} else if (BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) {
		vasipaddr -= SI_ENUM_BASE(wlc_hw->sih);
		vasip_mem = (uint32 *)(wlc_hw->sih->enum_base_va + vasipaddr);
	}
#else
	vasip_mem = (uint32 *)(unsigned long)vasipaddr;
#endif /* DONGLEBUILD */

	ASSERT(vasip_mem);

	return vasip_mem;
}

wlc_vasip_info_t *
BCMATTACHFN(wlc_vasip_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_vasip_info_t *vasip;
	wlc_hw_info_t *wlc_hw = wlc->hw;

	wlc_hw->vasip_loaded = FALSE;

	if ((vasip = MALLOCZ(wlc->osh, sizeof(wlc_vasip_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: vasip memory alloc. failed\n",
			pub->unit, __FUNCTION__));
		return NULL;
	}

	vasip->wlc = wlc;
	vasip->sym_map = NULL;
	vasip->sym_map_size = NULL;
	vasip->mu_supported_Ntx = 0;

	if ((wlc_module_register(pub, vasip_iovars, "vasip",
		vasip, wlc_vasip_doiovar, NULL, NULL, NULL)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			pub->unit, __FUNCTION__));
		MFREE(wlc->osh, vasip, sizeof(*vasip));
		return NULL;
	}

	pub->_vasip = TRUE;
#ifdef BCMQT
	/* disable vasip function in emulation platform */
	/* note: for 6715, even include vasip function for veloce testing */
	if (!D11REV_IS(wlc_hw->corerev, 132) && !D11REV_IS(wlc_hw->corerev, 130)) {
		pub->_vasip = FALSE;
	}
#endif /* BCMQT */

#if defined(BCMDBG)
	wlc_dump_register(pub, "vasip_counters", (dump_fn_t)wlc_dump_vasip_counters, (void *)wlc);
	wlc_dump_register(pub, "vasip_status", (dump_fn_t)wlc_dump_vasip_status, (void *)wlc);
	wlc_dump_register(pub, "mu_snr_calib", (dump_fn_t)wlc_dump_mu_snr_calib, (void *)wlc);
#endif // endif

	wlc_hw->vasip_addr = (uint16 *)wlc_vasip_base(wlc_hw);
	wlc_hw->vasip_addr_int = (uint16 *)wlc_vasip_base_int(wlc_hw);

	return vasip;
}

#if (defined(BCMDBG) || defined(TESTBED_AP_11AX))
static int
wlc_v2m_grp_check(wlc_info_t *wlc, uint16* v2m_grp, mu_group_t *mugrp,
		uint16 mode, uint16 msg_len) {
	uint16 N_grp;

	N_grp = (v2m_grp[0] - 2) / msg_len;
	if (((N_grp > mugrp->group_number) && (mugrp->group_method == 1)) ||
			((v2m_grp[0] != 0) && (v2m_grp[0] != (N_grp * msg_len + 2)))) {
		N_grp = 0;
		WL_ERROR(("wl%d: %s: mode=%d, unexpected auto_group_num=%d "
			"or v2m_len=%d (should be 2+%d*N)\n",
			wlc->hw->unit, __FUNCTION__, mode, N_grp, v2m_grp[0], msg_len));
	}

	return N_grp;
}

static void
wlc_v2m_grp_method_name(mu_group_t *mugrp)
{
	/* method name */
	if (mugrp->group_method == 0) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"1 group for all admitted users");
	} else if (mugrp->group_method == 1) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"N best THPT groups and equally distributed across all BW");
	} else if (mugrp->group_method == 2) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"greedy-cover non-disjoint grouping");
	} else if (mugrp->group_method == 3) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"disjoint grouping");
	} else if (mugrp->group_method == 4) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"greedy-cover-extension grouping");
	} else if (mugrp->group_method == 5) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"greedy-cover-extension grouping with max-phyrate group");
	} else if (mugrp->group_method == 9) {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"on-the-fly grouping");
	} else {
		snprintf((char *)mugrp->group_method_name,
			sizeof(mugrp->group_method_name),
			"not support yet");
	}
}

static int
wlc_v2m_grp_aux_info_header(wlc_info_t *wlc, mu_group_t *mugrp)
{
	int pos, support_agfsnr = 0;
	/* group_option can store up to 15 groups info
		* overload unused group_option to store AGF/SNR
		*   groups:  group_option[0:(grp_num-1)][:]
		*   check:   group_option[grp_num][:]
		*   agf/snr: group_option[(grp_num+1):NGROUP_MAX][:]
		* support_agfsnr: bit0 for SNR and bit1 for AGF
		* after m2v v1.5 (FW v5.25), there is no AGF info in group response
		*/
	pos = mugrp->auto_group_num;
	if (pos <= (WL_MU_GROUP_NGROUP_MAX-2)) {
		if (IS_V2M_FORMAT_V1_5(wlc->hw->corerev) ||
			IS_V2M_FORMAT_V3(wlc->hw->corerev)) {
			support_agfsnr = 1;
		} else {
			support_agfsnr = 3;
		}
		mugrp->group_option[pos][0] = WL_MU_GROUP_PARAMS_VERSION;
		mugrp->group_option[pos][1] = mugrp->group_method;
		mugrp->group_option[pos][2] = mugrp->auto_group_num;
		mugrp->group_option[pos][3] = support_agfsnr;
	}

	return support_agfsnr;
}

static void
wlc_v2m_grp_read(mu_group_t *mugrp, uint16* v2m_grp, int grp_idx, int info_idx,
		uint16 support_agfsnr, uint16 msg_peruser_nword,
		uint16 msg_offset_usr, uint16 msg_offset_mcs) {
	int n;
	int pos_usr, pos_mcs;

	mugrp->group_GID[grp_idx] = v2m_grp[1] & 0x003f;

	pos_usr = msg_offset_usr;
	pos_mcs = msg_offset_mcs;
	for (n = 0; n < 4; n++) {
		// main info for user and rate
		mugrp->group_option[grp_idx][n] = // [Uid/Rate]
			((v2m_grp[pos_usr] & 0x00ff) << 8) +
			(v2m_grp[pos_mcs] & 0x00ff);
		// additional aux info for SNR/AGF
		if (info_idx < WL_MU_GROUP_NGROUP_MAX) {
			mugrp->group_option[info_idx][n] = 0;
			if (support_agfsnr & 0x1) { // SNR
				mugrp->group_option[info_idx][n] |=
					((v2m_grp[pos_mcs] & 0xff00) >> 8);
			}
			if (support_agfsnr & 0x2) { // AGF
				mugrp->group_option[info_idx][n] |=
					(v2m_grp[pos_usr] & 0xff00);
			}
		}
		// update position for next user
		pos_usr += msg_peruser_nword;
		pos_mcs += msg_peruser_nword;
	}
}

static void
wlc_v2m_grp_read_otf(mu_group_t *mugrp, uint16* v2m_grp,
		int grp_idx, int info_idx,
		uint16 support_agfsnr, uint16 msg_peruser_nword,
		uint16 msg_offset_usr, uint16 msg_offset_mcs) {
	int n;
	int pos_usr, pos_mcs;

	mugrp->group_GID[grp_idx] = v2m_grp[1] & 0x003f;

	pos_usr = msg_offset_usr;
	pos_mcs = msg_offset_mcs;
	for (n = 0; n < 4; n++) {
		// main info for user and rate
		if (v2m_grp[pos_usr] != 0xffff) {
			mugrp->group_option[grp_idx][n] = // [Uid/Rate]
				((v2m_grp[pos_usr] & 0x1e00) >> 1) +
				(v2m_grp[pos_mcs] & 0x00ff);
			// additional aux info for SNR/AGF
			if (info_idx < WL_MU_GROUP_NGROUP_MAX) {
				mugrp->group_option[info_idx][n] = 0;
				if (support_agfsnr & 0x1) { // SNR
					mugrp->group_option[info_idx][n] |=
						((v2m_grp[pos_mcs] & 0xff00) >> 8);
				}
			}
		} else {
			mugrp->group_option[grp_idx][n] = 0xffff;
		}
		// update position for next user
		pos_usr += msg_peruser_nword;
		pos_mcs += msg_peruser_nword;
	}
}
#endif // endif

static int
wlc_vasip_doiovar(void *context, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint vsize, struct wlc_if *wlcif)
{
	wlc_vasip_info_t *vasip = (wlc_vasip_info_t*)context;
	wlc_info_t *wlc = vasip->wlc;
	int err = BCME_OK;

	BCM_REFERENCE(wlc);

	/* IOVAR works only if VASIP is active and FW is loaded */
	if (!VASIP_PRESENT(wlc->hw)) {
		WL_ERROR(("wl%d: %s: VASIP is not present!\n",
			wlc->hw->unit, __FUNCTION__));
		return BCME_UNSUPPORTED;
	}
	if (!wlc->hw->vasip_loaded) {
		WL_ERROR(("wl%d: %s: VASIP FW is not loaded!\n",
			wlc->hw->unit, __FUNCTION__));
		return BCME_UNSUPPORTED;
	}
	if (!wlc->hw->clk) {
		WL_ERROR(("wl%d: %s: no clock!\n",
			wlc->hw->unit, __FUNCTION__));
		return BCME_NOCLK;
	}

	switch (actionid) {
#if defined(BCMDBG)
	case IOV_SVAL(IOV_VASIP_COUNTERS_CLEAR): {
		err = wlc_vasip_counters_clear(wlc->hw);
		break;
	}

	case IOV_GVAL(IOV_SVMP_MEM): {
		svmp_mem_t *mem = (svmp_mem_t *)params;
		uint32 mem_addr;
		uint16 mem_len;

		mem_addr = mem->addr;
		mem_len = mem->len;

		if (len < mem_len) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		// allow to access odd int16 addr
		//if (mem_addr & 1) {
		//	err = BCME_BADADDR;
		//	break;
		//}

		err = wlc_svmp_mem_read(wlc->hw, mem_addr, mem_len, (uint16 *)arg);
		break;
	}

	case IOV_SVAL(IOV_SVMP_MEM): {
		svmp_mem_t *mem = (svmp_mem_t *)params;
		uint32 mem_addr;
		uint16 mem_len;

		mem_addr = mem->addr;
		mem_len = mem->len;

		// allow to access odd int16 addr
		//if (mem_addr & 1) {
		//	err = BCME_BADADDR;
		//	break;
		//}

		err = wlc_svmp_mem_set(wlc->hw, mem_addr, mem_len, mem->val);
		break;
	}

	case IOV_SVAL(IOV_SVMP_DOWNLOAD):
	{
		/* download vasip codes - wlc_vasip_download() */
		break;
	}

	case IOV_GVAL(IOV_MU_RATE): {
		WL_ERROR(("wl%d: %s: wl mu_rate is no longer supported\n",
			wlc->hw->unit, __FUNCTION__));
		err = BCME_UNSUPPORTED;
		break;
	}

	case IOV_SVAL(IOV_MU_RATE): {
		WL_ERROR(("wl%d: %s: wl mu_rate is no longer supported\n",
			wlc->hw->unit, __FUNCTION__));
		err = BCME_UNSUPPORTED;
		break;
	}
#endif // endif
#if (defined(BCMDBG) || defined(TESTBED_AP_11AX))
	case IOV_GVAL(IOV_MU_GROUP): {
		mu_group_t *mugrp = (mu_group_t *)arg;
		uint16 forced_group = 0;
		uint32 mem_addr;
		uint16 mem_len;
		//uint16 v2m_grp[22]; // 2 + 4 * msg_peruser_nword
		//uint16 v2m_grp[2+2*WL_MU_GROUP_NUSER_MAX*14]; // 14 = 2 + 4 * msg_peruser_nword
		//uint16 v2m_grp_he[2+WL_MU_GROUP_NUSER_MAX*14];
		uint16 *v2m_grp, *v2m_grp_he;
		uint16 v2m_grp_hist[128];
		uint16 grp_num, grp_num_vht, grp_num_he;
		uint16 msg_len, msg_peruser_nword;
		uint16 msg_offset_usr, msg_offset_mcs;
		uint16 m, k = 0, support_agfsnr = 0;

		v2m_grp    = &v2m_grp_hist[0];
		v2m_grp_he = &v2m_grp_hist[64];

		/* set WL_MU_GROUP_PARAMS_VERSION */
		mugrp->version = WL_MU_GROUP_PARAMS_VERSION;

		/* check if forced */
		mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced);
		wlc_svmp_mem_read1(wlc->hw, mem_addr, &forced_group);

		if (forced_group > 0) {
			mugrp->forced = 1;
			mugrp->forced_group_num = forced_group;
			/* get forced_group_mcs */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced_mcs);
			wlc_svmp_mem_read1(wlc->hw, mem_addr,
				(uint16*)(&(mugrp->forced_group_mcs)));
			/* get forced_group_option */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grp_forced_buf);
			mem_len = forced_group*WL_MU_GROUP_NUSER_MAX;
#if (WL_MU_GROUP_PARAMS_VERSION < 4)
			wlc_svmp_mem_read(wlc->hw, mem_addr, mem_len,
				(uint16*)(&(mugrp->group_option[0][0])));
#else
			wlc_svmp_mem_read(wlc->hw, mem_addr, mem_len,
				(uint16*)(&(mugrp->forced_group_option[0][0])));
#endif // endif
		}
#if (WL_MU_GROUP_PARAMS_VERSION < 4)
		else {
			mugrp->forced = 0;
			mugrp->forced_group_num = 0;
#else
		{
#endif // endif
			/* get group_method */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_method);
			wlc_svmp_mem_read1(wlc->hw, mem_addr,
				(uint16*)(&(mugrp->group_method)));
			/* get group_number */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_number);
			wlc_svmp_mem_read1(wlc->hw, mem_addr,
				(uint16*)(&(mugrp->group_number)));
			/* method name */
			wlc_v2m_grp_method_name(mugrp);

			/* read out v2m_buf_grp[] to get latest recommend groups */

			if (IS_V2M_FORMAT_V1X(wlc->hw->corerev)) {
				// m2v/v2m interface v1.X
				msg_peruser_nword = 3;
				msg_offset_usr = 2;
				msg_offset_mcs = 3;
			} else if (IS_V2M_FORMAT_V3(wlc->hw->corerev)) {
				// m2v/v2m interface v3 or later
				msg_peruser_nword = 3;
				msg_offset_usr = 3;
				msg_offset_mcs = 4;
			} else {
				WL_ERROR(("wl%d: %s: invalid chip or m2v format\n",
				    wlc->hw->unit, __FUNCTION__));
				//err = BCME_ERROR;
				break;
			}
			msg_len = 4 + 4 * msg_peruser_nword * 2; // in byte
			/* read whole v2m_grp buffer */
			mem_len = 2+WL_MU_GROUP_NUSER_MAX*14;
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp);
			wlc_svmp_mem_read(wlc->hw, mem_addr, mem_len, v2m_grp);
			grp_num_vht = wlc_v2m_grp_check(wlc, v2m_grp, mugrp, 0, msg_len);
			if (IS_AC_ONLY_CHIP(wlc->hw->corerev)) {
				grp_num_he = 0;
			} else {
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp_he);
				wlc_svmp_mem_read(wlc->hw, mem_addr, mem_len, v2m_grp_he);
				grp_num_he = wlc_v2m_grp_check(wlc, v2m_grp_he, mugrp, 1, msg_len);
			}
			grp_num = grp_num_vht + grp_num_he;
			mugrp->auto_group_num = grp_num;

			if (grp_num > 0) {
				support_agfsnr = wlc_v2m_grp_aux_info_header(wlc, mugrp);
				k = grp_num + 1;
				mem_len = msg_len / 2; // byte to word
				for (m = 0; m < grp_num_vht; m++) {
					wlc_v2m_grp_read(mugrp, &v2m_grp[2+m*mem_len], m, k,
						support_agfsnr, msg_peruser_nword,
						msg_offset_usr, msg_offset_mcs);
					k++;
				}
				for (m = 0; m < grp_num_he; m++) {
					wlc_v2m_grp_read(mugrp, &v2m_grp_he[2+m*mem_len],
						m+grp_num_vht, k,
						support_agfsnr, msg_peruser_nword,
						msg_offset_usr, msg_offset_mcs);
					k++;
				}
			}
			// check on-the-fly grouping after v6.38
#if (VASIP_FW_VER >= VASIP_FW_VER_NUM(6, 38))
			else if (grp_num == 0) {
				msg_peruser_nword = 3;
				msg_offset_usr = 3;
				msg_offset_mcs = 4;
				msg_len = 8 + 4 * msg_peruser_nword * 2; // in byte
#if ((WL_MU_GROUP_PARAMS_VERSION >= 4) && (VASIP_FW_VER >= VASIP_FW_VER_NUM(6, 39)))
				/* read grp_history_buf */
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grp_history_buf);
				wlc_svmp_mem_read(wlc->hw, mem_addr, 7*16, v2m_grp_hist);
				grp_num = 0;
				for (m = 0; m < 7; m++) {
					//printf("m=%d, v2m_grp[16*m]=%d\n", m, v2m_grp[16*m]);
					if (v2m_grp_hist[16*m] == (msg_len - 2)) {
						grp_num += 1;
					} else {
						break;
					}
				}
				mugrp->auto_group_num = grp_num;
				if (grp_num > 0) {
					mugrp->group_method = 9;
					wlc_v2m_grp_method_name(mugrp);
					support_agfsnr = wlc_v2m_grp_aux_info_header(wlc, mugrp);
					k = grp_num + 1;
					for (m = 0; m < grp_num; m++) {
						wlc_v2m_grp_read_otf(mugrp, &v2m_grp_hist[16*m+2],
							m, k,
							support_agfsnr, msg_peruser_nword,
							msg_offset_usr, msg_offset_mcs);
						k++;
					}
				}
#else
				/* read v2m_buf_grp_on_the_fly(_he) */
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp_on_the_fly);
				wlc_svmp_mem_read(wlc->hw, mem_addr, msg_len/2, v2m_grp);
				grp_num_vht = (v2m_grp[0] == (msg_len - 2)) ? 1 : 0;

				if (IS_AC_ONLY_CHIP(wlc->hw->corerev)) {
					grp_num_he = 0;
				} else {
					mem_addr = VASIP_SHARED_OFFSET(wlc->hw,
							v2m_buf_grp_on_the_fly_he);
					wlc_svmp_mem_read(wlc->hw, mem_addr, msg_len/2, v2m_grp_he);
					grp_num_he = (v2m_grp_he[0] == (msg_len - 2)) ? 1 : 0;
				}

				grp_num = grp_num_vht + grp_num_he;
				mugrp->auto_group_num = grp_num;
				if (grp_num > 0) {
					mugrp->group_method = 9;
					wlc_v2m_grp_method_name(mugrp);

					support_agfsnr = wlc_v2m_grp_aux_info_header(wlc, mugrp);
					k = grp_num + 1;

					if (grp_num_vht > 0) {
						wlc_v2m_grp_read_otf(mugrp, &v2m_grp[2],
							0, k,
							support_agfsnr, msg_peruser_nword,
							msg_offset_usr, msg_offset_mcs);
						k++;
					}
					if (grp_num_he > 0) {
						wlc_v2m_grp_read_otf(mugrp, &v2m_grp_he[2],
							grp_num_vht, k,
							support_agfsnr, msg_peruser_nword,
							msg_offset_usr, msg_offset_mcs);
					}
				}
#endif /* WL_MU_GROUP_PARAMS_VERSION >= 4 && after v6.39 */
			}
#endif /* after v6.38 */
		}
		break;
	}

	case IOV_SVAL(IOV_MU_GROUP): {
		mu_group_t *mugrp = (mu_group_t *)arg;
		uint16 forced_group = 0;
		uint32 mem_addr;
		uint16 mem_len;

		/* check WL_MU_GROUP_PARAMS_VERSION */
		if (mugrp->version != WL_MU_GROUP_PARAMS_VERSION) {
			err = BCME_BADARG;
			break;
		}

		/* forced grouping */
		if (mugrp->forced == WL_MU_GROUP_MODE_FORCED) {
			/* set forced_group with forced_group_num */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced);
			wlc_svmp_mem_set1(wlc->hw, mem_addr, mugrp->forced_group_num);
			/* set forced_group mcs */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced_mcs);
			wlc_svmp_mem_set1(wlc->hw, mem_addr, mugrp->forced_group_mcs);
			/* store forced grouping options into SVMP */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grp_forced_buf);
			mem_len = mugrp->forced_group_num*WL_MU_GROUP_NUSER_MAX;
			if ((err = wlc_svmp_mem_blk_set(wlc->hw, mem_addr, mem_len,
					(uint16*)(&(mugrp->group_option[0][0])))) != BCME_OK) {
				break;
			}
			/* set fix_rate=0 for forced_group==1 && forced_group_num!=1 */
			if (mugrp->forced_group_num != 1) {
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mcs_overwrite_flag);
				wlc_svmp_mem_set1(wlc->hw, mem_addr, 0);
			}
		} else if (mugrp->forced == WL_MU_GROUP_MODE_AUTO) {
			/* disable forced_group */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced);
			wlc_svmp_mem_set1(wlc->hw, mem_addr, 0);
			/* clean first forced grouping option in SVMP */
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grp_forced_buf);
			mem_len = WL_MU_GROUP_NGROUP_MAX*WL_MU_GROUP_NUSER_MAX;
			if ((err = wlc_svmp_mem_set(wlc->hw,
					mem_addr, mem_len, 0xffff)) != BCME_OK) {
				break;
			}
		} /* mugrp->forced can be WL_MU_GROUP_ENTRY_EMPTY for no '-g' option */

		/* auto grouping parameters only when not-forced-group */
		mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_forced);
		wlc_svmp_mem_read1(wlc->hw, mem_addr, &forced_group);

		if (forced_group == WL_MU_GROUP_MODE_AUTO) {
			if (mugrp->group_number != WL_MU_GROUP_ENTRY_EMPTY) {
				/* update group_number */
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_number);
				wlc_svmp_mem_set1(wlc->hw, mem_addr, mugrp->group_number);
			}
			if (mugrp->group_method != WL_MU_GROUP_ENTRY_EMPTY) {
				/* update group method */
				mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_method);
				wlc_svmp_mem_set1(wlc->hw, mem_addr, mugrp->group_method);
				/* set group number */
				if (mugrp->group_method == WL_MU_GROUP_METHOD_OLD) {
					/*  old method: it should be 1 */
					mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_number);
					wlc_svmp_mem_set1(wlc->hw, mem_addr, 1);
				} else if (mugrp->group_number == WL_MU_GROUP_ENTRY_EMPTY) {
					/* set if not specified group_number
					*   method 1: set 1 if not specified group_number
					*   method 2&3: don't-care (set to 4)
					*/
					mem_addr = VASIP_SHARED_OFFSET(wlc->hw, grouping_number);
					if (mugrp->group_method == 1) {
						mugrp->group_number = 1;
					} else if ((mugrp->group_method == 2) ||
							(mugrp->group_method == 3)) {
						mugrp->group_number = 4;
					}
					wlc_svmp_mem_set1(wlc->hw, mem_addr, mugrp->group_number);
				}
				/* set fix_rate=0 for forced_group==0 && old mathod */
				if (mugrp->group_method != WL_MU_GROUP_METHOD_OLD) {
					mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mcs_overwrite_flag);
					wlc_svmp_mem_set1(wlc->hw, mem_addr, 0);
				}
			}
		}
		break;
	}
#endif // endif
#if defined(BCMDBG)
	/* VASIP FW knobs: unsigned int16 */
	case IOV_GVAL(IOV_MU_MCS_RECMD):
	case IOV_GVAL(IOV_MU_MCS_CAPPING):
	case IOV_GVAL(IOV_MU_SGI_RECMD_TH):
	case IOV_GVAL(IOV_MU_GROUP_DELAY):
	case IOV_GVAL(IOV_MU_PRECODER_DELAY): {
		uint16 value = 0;
		uint32 mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mi_mcs_recommend_enable);
		uint32 tmp_val;

		if (len < 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (IOV_ID(actionid) == IOV_MU_MCS_RECMD) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mi_mcs_recommend_enable);
		} else if (IOV_ID(actionid) == IOV_MU_MCS_CAPPING) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mcs_capping_enable);
		} else if (IOV_ID(actionid) == IOV_MU_SGI_RECMD_TH) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_threshold_80M);
		} else if (IOV_ID(actionid) == IOV_MU_GROUP_DELAY) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, delay_grouping_us);
		} else if (IOV_ID(actionid) == IOV_MU_PRECODER_DELAY) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, delay_precoding_us);
		}

		wlc_svmp_mem_read1(wlc->hw, mem_addr, &value);
		tmp_val = (uint32)value;
		memcpy(arg, &tmp_val, sizeof(uint32));
		break;
	}

	case IOV_SVAL(IOV_MU_MCS_RECMD):
	case IOV_SVAL(IOV_MU_MCS_CAPPING):
	case IOV_SVAL(IOV_MU_SGI_RECMD_TH):
	case IOV_SVAL(IOV_MU_GROUP_DELAY):
	case IOV_SVAL(IOV_MU_PRECODER_DELAY): {
		uint16 value;
		uint32 mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mi_mcs_recommend_enable);

		if (len < 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		memcpy(&value, params, sizeof(uint16));

		if (IOV_ID(actionid) == IOV_MU_MCS_RECMD) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mi_mcs_recommend_enable);
			if ((value != 0) && (value != 1)) {
				return BCME_USAGE_ERROR;
			}
		} else if (IOV_ID(actionid) == IOV_MU_MCS_CAPPING) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, mcs_capping_enable);
			if ((value != 0) && (value != 1)) {
				return BCME_USAGE_ERROR;
			}
		} else if (IOV_ID(actionid) == IOV_MU_SGI_RECMD_TH) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_threshold_80M);
		} else if (IOV_ID(actionid) == IOV_MU_GROUP_DELAY) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, delay_grouping_us);
		} else if (IOV_ID(actionid) == IOV_MU_PRECODER_DELAY) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, delay_precoding_us);
		}

		wlc_svmp_mem_set1(wlc->hw, mem_addr, value);
		break;
	}

	/* VASIP FW knobs: signed int16 */
	case IOV_GVAL(IOV_MU_SGI_RECMD): {
		int16 value = 0;
		uint32 tmp_val;
		uint32 mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_method);

		if (len < 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (IOV_ID(actionid) == IOV_MU_SGI_RECMD) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_method);
		}

		wlc_svmp_mem_read1(wlc->hw, mem_addr, (uint16*)&value);
		tmp_val = (uint32)value;
		memcpy(arg, &tmp_val, sizeof(uint32));
		break;
	}

	case IOV_SVAL(IOV_MU_SGI_RECMD): {
		int16 value;
		uint32 mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_method);

		if (len < 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		memcpy(&value, params, sizeof(uint16));

		if (IOV_ID(actionid) == IOV_MU_SGI_RECMD) {
			mem_addr = VASIP_SHARED_OFFSET(wlc->hw, sgi_method);
			if ((value != 0) && (value != 1) && (value != -1) && (value != 2)) {
				return BCME_USAGE_ERROR;
			}
		}

		wlc_svmp_mem_set1(wlc->hw, mem_addr, (uint16)value);
		break;
	}

	case IOV_SVAL(IOV_VASIP_ERROR_CLEAR): {
		err = wlc_vasip_error_clear(wlc->hw);
		break;
	}
#endif // endif

	default :
		err = BCME_ERROR;
		break;
	}
	return err;
}

/**
 * Returns the internal address where the VASIP window starts
 * @param offset[in]    Word count
 */
uint32 *
wlc_vasip_addr_int(wlc_hw_info_t *wlc_hw, uint32 offset)
{
	return (uint32 *)(wlc_hw->vasip_addr_int + offset);
}

/**
 * Returns the virtual address where the VASIP window starts
 * @param offset[in]    Word count
 */
uint32 *
wlc_vasip_addr(wlc_hw_info_t *wlc_hw, uint32 offset)
{
	return (uint32 *)(wlc_hw->vasip_addr + offset);
}

/* write vasip code to vasip program memory */
void
wlc_vasip_write(wlc_hw_info_t *wlc_hw, const uint32 vasip_code[],
	const uint count, uint32 offset, uint32 offset_tbl)
{
	uint32 *vasip_mem;
	int i;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	vasip_mem = wlc_vasip_addr(wlc_hw, offset);

	/* write vasip code to program memory */
	for (i = 0; i < count; i++) {
		vasip_mem[i + offset_tbl] = vasip_code[i];
	}
}

void
wlc_vasip_read(wlc_hw_info_t *wlc_hw, uint32 vasip_code[],
	const uint count, uint32 offset)
{
	uint32 *vasip_mem;
	int i;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	vasip_mem = wlc_vasip_addr(wlc_hw, offset);

	/* write vasip code to program memory */
	for (i = 0; i < count; i++) {
		vasip_code[i] = vasip_mem[i];
	}
}

bool
wlc_vasip_present(wlc_hw_info_t *wlc_hw)
{
	if (si_findcoreidx(wlc_hw->sih, ACPHY_CORE_ID, 0) == BADIDX) {
		return FALSE;
	} else {
		return TRUE;
	}
}

#if defined(BCMDBG)
/* read from vasip program memory and compare it with vasip code */
static void
wlc_vasip_verify(wlc_hw_info_t *wlc_hw, const uint32 vasip_code[], const uint count,
	uint32 offset, uint32 offset_tbl)
{
	uint32 *vasip_mem;
	uint32 rd_data, err_cnt = 0;
	int i;

#ifdef BCMQT
	return; // skip this verification for QT due to slowness
#endif // endif

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	vasip_mem = wlc_vasip_addr(wlc_hw, offset);

	/* write vasip code to program memory */
	for (i = 0; i < count; i++) {
		rd_data = vasip_mem[i + offset_tbl];
		if (rd_data != vasip_code[i]) {
			err_cnt++;
		}
	}
	if (err_cnt == 0) {
		WL_TRACE(("wl%d: %s, download success, count %d, vasip_mem %p\n",
			wlc_hw->unit, __FUNCTION__, count, vasip_mem));
	} else {
		WL_ERROR(("wl%d: %s, download error, err_cnt %d\n",
			wlc_hw->unit, __FUNCTION__, err_cnt));
	}
}
#endif // endif

bool
BCMRAMFN(wlc_vasip_support)(wlc_hw_info_t *wlc_hw, uint32 *vasipver, bool nopi)
{
	d11regs_t *regs = wlc_hw->regs;

	if (!wlc_vasip_present(wlc_hw)) {
		WL_ERROR(("%s: wl%d: vasip hw is not present\n", __FUNCTION__, wlc_hw->unit));
		return FALSE;
	}

	if (nopi) {
		phy_prephy_vasip_ver_get(wlc_hw->prepi, regs, vasipver);
	} else {
		*vasipver = phy_vasip_get_ver(wlc_hw->band->pi);
	}

	if (*vasipver == 0) {
		WL_TRACE(("%s: wl%d: vasipver %d\n",
			__FUNCTION__, wlc_hw->unit, *vasipver));
		return TRUE;
	} else if (*vasipver == 3) {
		if (D11REV_GE(wlc_hw->corerev, 65) && D11REV_LE(wlc_hw->corerev, 132)) {
			WL_TRACE(("%s: wl%d: vasipver %d\n",
				__FUNCTION__, wlc_hw->unit, *vasipver));
			return TRUE;
		} else {
			/* TBD : need to be updated once vasip image is ready */
			WL_ERROR(("%s: wl%d: unsupported vasipver %d\n",
				__FUNCTION__, wlc_hw->unit, *vasipver));
			return FALSE;
		}
	} else {
		WL_ERROR(("%s: wl%d: unsupported vasipver %d\n",
			__FUNCTION__, wlc_hw->unit, *vasipver));
		return FALSE;
	}
}

/* initialize the vasip data memory */
static void
wlc_vasip_init_data(wlc_hw_info_t *wlc_hw, const uint32* data, const uint data_words)
{
	uint word = 0;
	uint32 addr, segment_len;

	while (word + 1 < data_words) {
		segment_len = data[word++];

		ASSERT(segment_len);

		if (segment_len > 0) {
			addr = data[word++];

			ASSERT(data_words >= (segment_len + word));

			if (data_words < (segment_len + word)) {
				WL_ERROR(("wl%d: wlc_vasip_init_data overrun: %d %d %d\n",
						wlc_hw->unit, data_words, segment_len, word));
				break;
			}

			wlc_vasip_write(wlc_hw, &data[word], segment_len, VASIP_CODE_OFFSET,
				VASIP_OFFSET_32(addr));

#if defined(BCMDBG)
			wlc_vasip_verify(wlc_hw, &data[word], segment_len, VASIP_CODE_OFFSET,
				VASIP_OFFSET_32(addr));
#endif // endif
			word += segment_len;

		} else {
			WL_ERROR(("wl%d: wlc_vasip_init_data 0 block: %d of %d\n",
				wlc_hw->unit, word, data_words));
			break;
		}
	}
}

/* vasip code download */
static void
wlc_vasip_download(wlc_hw_info_t *wlc_hw, const vasip_fw_t *fw,
	uint32 vasipver, bool nopi)
{
	d11regs_t *regs = wlc_hw->regs;
	wlc_vasip_info_t *vasip = wlc_hw->wlc->vasip;

	ASSERT(vasip);
	ASSERT(fw);

	/* stop the vasip processor */
	if (nopi) {
		phy_prephy_vasip_clk_set(wlc_hw->prepi, regs, TRUE);
		phy_prephy_vasip_proc_reset(wlc_hw->prepi, regs, TRUE);
	} else {
		phy_vasip_set_clk(wlc_hw->band->pi, TRUE);
		phy_vasip_reset_proc(wlc_hw->band->pi, TRUE);
	}

	/* write binary to the vasip program memory */
	if (!wlc_hw->vasip_loaded) {
#ifdef DONGLEBUILD
		/* VASIP FW is in postattach data, which is reclaimed. It cannot be
		 * referenced after reclaim.
		 */
		ASSERT(! POSTATTACH_PART_RECLAIMED());
#endif /* DONGLEBUILD */
		/* write binary to the vasip program memory */
		wlc_vasip_write(wlc_hw, fw->code, fw->code_size, VASIP_CODE_OFFSET, 0);
#if defined(BCMDBG)
		wlc_vasip_verify(wlc_hw, fw->code, fw->code_size, VASIP_CODE_OFFSET, 0);
#endif // endif

		/* Initialize data memory */
		wlc_vasip_init_data(wlc_hw, fw->data, fw->data_size);
		vasip->sym_map = fw->map;
		vasip->sym_map_size = fw->size;
		vasip->mu_supported_Ntx = fw->ntx;

		wlc_hw->vasip_loaded = TRUE;
#ifdef WL_MU_TX
		wlc_mutx_nrx_policy_upd(wlc_hw->wlc, vasip->mu_supported_Ntx);
#endif // endif
	}

	/* reset and start the vasip processor */
	if (nopi) {
		phy_prephy_vasip_proc_reset(wlc_hw->prepi, regs, FALSE);
	} else {
		phy_vasip_reset_proc(wlc_hw->band->pi, 0);
	}
} /* wlc_vasip_download */

/* initialize vasip */
void
BCMINITFN(wlc_vasip_init)(wlc_hw_info_t *wlc_hw, uint32 vasipver, bool nopi)
{
	const vasip_fw_t *vasip_fw = NULL;
	int txchains;
	int err = 0;
	wlc_info_t *wlc = wlc_hw->wlc;

	if (wlc == NULL) {
		WL_INFORM(("%s: wl%d: Failure to load VASIP FW: need wlc.\n",
			__FUNCTION__, wlc_hw->unit));
		return;
	} else if (nopi) {
		WL_ERROR(("%s: wl%d: Failure to load VASIP FW: need phy up.\n",
			__FUNCTION__, wlc_hw->unit));
		return;
	} else {
		txchains = WLC_BITSCNT(wlc->stf->hw_txchain);
	}

	if (D11REV_IS(wlc_hw->corerev, 129)) {
		vasip_fw = &vasip_11ax_4x4_fw;
		WL_INFORM(("wl%d: vasip 4x4 11ax ver %d.%d\n",
		    wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		vasip_fw = &vasip_11ax_2x2_fw;
		WL_INFORM(("wl%d: vasip 2x2 11ax ver %d.%d\n",
		    wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));
	} else if (D11REV_IS(wlc_hw->corerev, 131)) {
		vasip_fw = &vasip_11ax_3x3_fw;
		WL_INFORM(("wl%d: vasip 3x3 11ax ver %d.%d\n",
		    wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));
	} else if (D11REV_IS(wlc_hw->corerev, 132)) {
		vasip_fw = &vasip_11ax_4x4_wav2_fw;
		WL_INFORM(("wl%d: vasip 4x4 11ax wave2 ver %d.%d\n",
		    wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));
	} else {
		if (txchains == 4) {
			vasip_fw = &vasip_11ac_4x4_fw;
			WL_INFORM(("wl%d: vasip 4x4 11ac ver %d.%d\n",
				wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));
		} else if (txchains == 3) {
			vasip_fw = &vasip_11ac_3x3_fw;
			WL_INFORM(("wl%d: vasip 3x3 11ac ver %d.%d\n",
				wlc_hw->unit, d11vasipcode_major, d11vasipcode_minor));

			if (ACREV_IS(wlc->band->phyrev, 129)) {
				err = 2;
			}
		} else {
			vasip_fw = &vasip_11ac_4x4_fw;
			err = 1;
		}
	}

	if (err == 1) {
		WL_ERROR(("%s: wl%d: not downloading VASIP FW!\n",
			__FUNCTION__, wlc_hw->unit));
		return;
	} else if (err == 2) {
		WL_ERROR(("%s: wl%d: not downloading VASIP FW! but enable VASIP clock\n",
			__FUNCTION__, wlc_hw->unit));
		phy_vasip_set_clk(wlc_hw->band->pi, TRUE);
		return;
	}

	wlc_vasip_download(wlc_hw, vasip_fw, vasipver, nopi);
}

#if defined(BCMDBG)
/* dump vasip code info */
void
wlc_vasip_code_info(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "vasip code ver %d.%d\n", d11vasipcode_major, d11vasipcode_minor);
	bcm_bprintf(b, "vasip hw rev %d\n",
		phy_vasip_get_ver(wlc->hw->band->pi));
	bcm_bprintf(b, "vasip mem addr %p\n", wlc_vasip_addr(wlc->hw, VASIP_CODE_OFFSET));
}

/* dump vasip status data from vasip program memory */
int
wlc_dump_vasip_status(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint16 status[VASIP_COUNTERS_LMT];
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 offset;
	int i;

	/* IOVAR works only if VASIP is present && VASIP FW is loaded */
	if (!VASIP_PRESENT(wlc_hw)) {
		bcm_bprintf(b, "VASIP is not present!\n");
		return BCME_UNSUPPORTED;
	}
	if (!wlc_hw->vasip_loaded) {
		bcm_bprintf(b, "VASIP FW is not loaded!\n");
		return BCME_UNSUPPORTED;
	}

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = VASIP_SHARED_OFFSET(wlc_hw, interrupt);
	wlc_svmp_mem_read(wlc_hw, offset, VASIP_SHARED_SIZE(wlc_hw, interrupt), status);

	for (i = 0; i < VASIP_SHARED_SIZE(wlc_hw, interrupt); i++) {
		bcm_bprintf(b, "status[%d] %u\n", i, status[i]);
	}

	return BCME_OK;
}

/* dump vasip error status */
#if (VASIP_FW_VER >= VASIP_FW_VER_NUM(6, 1))
static int
wlc_vasip_error_clear(wlc_hw_info_t *wlc_hw)
{
	uint32 offset_error_cnt, offset_error_code[4];

	if (!VASIP_PRESENT(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}
	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	offset_error_cnt = VASIP_SHARED_OFFSET(wlc_hw, err_count);
	offset_error_code[0] = VASIP_SHARED_OFFSET(wlc_hw, err_code_group);
	offset_error_code[1] = VASIP_SHARED_OFFSET(wlc_hw, err_code_precoder);

	wlc_svmp_mem_set1(wlc_hw, offset_error_cnt, 0);
	wlc_svmp_mem_set1(wlc_hw, offset_error_code[0], 0);
	wlc_svmp_mem_set1(wlc_hw, offset_error_code[1], 0);

	return BCME_OK;
}

static void
wlc_dump_vasip_error_mu_general(struct bcmstrbuf *b, int err_code)
{
	if (err_code & 0x1) {
		bcm_bprintf(b, "  ERROR: invalid number of users.\n");
	}
	if (err_code & 0x2) {
		bcm_bprintf(b, "  ERROR: sounding report type is incorrect.\n");
	}
	if (err_code & 0x4) {
		bcm_bprintf(b, "  ERROR: TXV index is out of range.\n");
	}
	if (err_code & 0x8) {
		bcm_bprintf(b, "  ERROR: TXV index is repeated.\n");
	}
	if (err_code & 0x10) {
		bcm_bprintf(b, "  ERROR: TXV index points to empty TxV slot.\n");
	}
	if (err_code & 0x20) {
		bcm_bprintf(b, "  ERROR: number of TX antenna is incorrect.\n");
	}
	if (err_code & 0x40) {
		bcm_bprintf(b, "  ERROR: users' BW are mismatched.\n");
	}
	if (err_code & 0x80) {
		bcm_bprintf(b, "  ERROR: user's BW is beyond 80MHz.\n");
	}
}

static void
wlc_dump_vasip_error_mu(struct bcmstrbuf *b, int err_cnt,
    int err_code_group, int err_code_precoder) {
	if (err_cnt > 0) {
		bcm_bprintf(b, "\nERROR_cnt: %d.\n", err_cnt);
	}
	if (err_code_group != 0) {
		bcm_bprintf(b, "ERROR in group request:\n");
		wlc_dump_vasip_error_mu_general(b, err_code_group);
	}
	if (err_code_precoder != 0) {
		bcm_bprintf(b, "ERROR in precoder request:\n");
		wlc_dump_vasip_error_mu_general(b, err_code_precoder);
		if (err_code_precoder & 0x1000) {
			bcm_bprintf(b, "  ERROR: user has more than 2 streams.\n");
		}
		if (err_code_precoder & 0x2000) {
			bcm_bprintf(b, "  ERROR: total stream is more than 4.\n");
		}
		if (err_code_precoder & 0x4000) {
			bcm_bprintf(b, "  ERROR: output address is out of range.\n");
		}
	}
}

#else

static int
wlc_vasip_error_clear(wlc_hw_info_t *wlc_hw)
{
	uint32 offset_error;

	if (!VASIP_PRESENT(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}
	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	offset_error = VASIP_SHARED_OFFSET(wlc_hw, err_code);

	wlc_svmp_mem_set1(wlc_hw, offset_error, 0);
	wlc_svmp_mem_set1(wlc_hw, offset_error+1, 0);

	return BCME_OK;
}

static void
wlc_dump_vasip_error_mu(struct bcmstrbuf *b, int err_cnt, int err_code)
{
	if ((err_cnt > 0) && (err_code != 0)) {
		bcm_bprintf(b, "\nERROR_cnt: %d.\n", err_cnt);
	}
	if (err_code & 0x1) {
		bcm_bprintf(b, "ERROR: more than 4 users in group selecton.\n");
	}
	if (err_code & 0x2) {
		bcm_bprintf(b, "ERROR: user BW mismatch in group selecton.\n");
	}
	if (err_code & 0x4) {
		bcm_bprintf(b, "ERROR: user BW is beyond 80MHz in group selecton.\n");
	}
	if (err_code & 0x8) {
		bcm_bprintf(b, "ERROR: BFM index is out of range in group selecton.\n");
	}
	if (err_code & 0x10) {
		bcm_bprintf(b, "ERROR: BFM index is repeated in group selecton.\n");
	}
	if (err_code & 0x20) {
		bcm_bprintf(b, "ERROR: output address is out of range in precoder.\n");
	}
	if (err_code & 0x40) {
		bcm_bprintf(b, "ERROR: BFM index is out of range in precoder.\n");
	}
	if (err_code & 0x80) {
		bcm_bprintf(b, "ERROR: one user has more than 2 streams in precoder.\n");
	}
	if (err_code & 0x100) {
		bcm_bprintf(b, "ERROR: more than 4 streams in precoder.\n");
	}
	if (err_code & 0x200) {
		bcm_bprintf(b, "ERROR: user BW mismatch in precoder.\n");
	}
	if (err_code & 0x400) {
		bcm_bprintf(b, "ERROR: user BW is beyond 80MHz in precoder.\n");
	}
	if (err_code & 0x800) {
		bcm_bprintf(b, "ERROR: number of TX antenna is not 4.\n");
	}
	if (err_code & 0x1000) {
		bcm_bprintf(b, "ERROR: sounding report type is not correct.\n");
	}
	if (err_code & 0x2000) {
		bcm_bprintf(b, "ERROR: BFM index is repeated in precoder.\n");
	}
	if (err_code & 0x4000) {
		bcm_bprintf(b, "ERROR: Empty TxV slot in group selecton.\n");
	}
}
#endif /* VASIP_FW_VER >= VASIP_FW_VER_NUM(6,1) */

/* dump vasip counters from vasip program memory */
int
wlc_dump_vasip_counters(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint16 counter[VASIP_COUNTERS_LMT], error[7];
	uint16 mcs[16], mcs1[16], c[4], s[4], c1[4], s1[4], N_user;
	uint32 offset, offset_steered_mcs, offset_recommended_mcs, offset_error;

	wlc_hw_info_t *wlc_hw = wlc->hw;
	int i;

	/* IOVAR works only if VASIP is present && VASIP FW is loaded */
	if (!VASIP_PRESENT(wlc_hw)) {
		bcm_bprintf(b, "VASIP is not present!\n");
		return BCME_UNSUPPORTED;
	}
	if (!wlc_hw->vasip_loaded) {
		bcm_bprintf(b, "VASIP FW is not loaded!\n");
		return BCME_UNSUPPORTED;
	}

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = VASIP_SHARED_OFFSET(wlc_hw, interrupt);
	offset_steered_mcs = VASIP_SHARED_OFFSET(wlc_hw, steering_mcs);
	offset_recommended_mcs = VASIP_SHARED_OFFSET(wlc_hw, recommend_mcs);
	offset_error = VASIP_SHARED_OFFSET(wlc_hw, err_code);

	/* print for any non-zero values */
	wlc_svmp_mem_read(wlc_hw, offset, VASIP_SHARED_SIZE(wlc_hw, interrupt), counter);
	wlc_svmp_mem_read(wlc_hw, offset_steered_mcs, 16, mcs);
	wlc_svmp_mem_read(wlc_hw, offset_recommended_mcs, 4, mcs1);
	wlc_svmp_mem_read(wlc_hw, offset_error, 7, error);

	for (i = 0; i < 4; i++) {
		s[i] = ((mcs[i] & 0xf0) >> 4) + 1;
		c[i] = mcs[i] & 0xf;
	}
	for (i = 0; i < 4; i++) {
		s1[i] = ((mcs1[i] & 0xf0) >> 4) + 1;
		c1[i] = mcs1[i] & 0xf;
	}

	if (D11REV_IS(wlc_hw->corerev, 129) || D11REV_IS(wlc_hw->corerev, 132)) {
		bcm_bprintf(b, "Received Interrupts:\n"
			"      bfe_module_done:0x%x     bfe_imp_done:0x%x     bfd_interrupt:0x%x\n"
			"      m2v_transfer_done:0x%x   v2m_transfder_done:0x%x\n\n",
			counter[11], counter[12], counter[10]+counter[13]+counter[14]+counter[15],
			counter[19], counter[20]);
	} else {
		bcm_bprintf(b, "Received Interrupts:\n"
		"      bfr_module_done:0x%x     bfe_module_done:0x%x     bfe_imp_done:0x%x\n"
		"      m2v_transfer_done:0x%x   v2m_transfder_done:0x%x\n\n",
		counter[10], counter[11], counter[12], counter[19], counter[20]);
	}

	N_user = (mcs[9] > 4) ? 0 : mcs[9];
	for (i = 0; i < N_user; i++) {
		bcm_bprintf(b, "user%d: txvIdx %d, steered rate c%ds%d, "
				"recommended rate c%ds%d, precoderSNR %ddB\n",
				i, mcs[10+i], c[i], s[i], c1[i], s1[i], mcs[5+i]/4);
	}

	bcm_bprintf(b,
			"\nImportant SVMP address:\n"
			"      M2V buf0/1 address:          0x%x / 0x%x\n"
			"      grouping M2V/V2M address:    0x%x / 0x%x",
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf0),
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf1),
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_grp_sel),
			VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp));
	if (!IS_AC_ONLY_CHIP(wlc->hw->corerev)) {
		bcm_bprintf(b, " & 0x%x", VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp_he));
	}
	bcm_bprintf(b,  "\n      precoder M2V/V2M address:    0x%x / 0x%x\n",
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_precoder),
			VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_mvp));
	if (IS_ON_THE_FLY_GROUPING(wlc->hw->corerev)) {
		bcm_bprintf(b,  "      on-the-fly M2V/V2M address:  0x%x & 0x%x / 0x%x & 0x%x\n",
				VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_grp_on_the_fly),
				VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_grp_on_the_fly_he),
				VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp_on_the_fly),
				VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_grp_on_the_fly_he));
		bcm_bprintf(b,  "      sounding M2V/V2M address:    0x%x / 0x%x\n",
				VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_snd_update),
				VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_snd_update));
	}
	if (IS_VASIP_OFDMA_SCH(wlc->hw->corerev)) {
		bcm_bprintf(b,
			"      CQI report M2V/V2M address:  0x%x / 0x%x\n"
			"      RU alloc M2V/V2M address:    0x%x / 0x%x\n",
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_cqi),
			VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_cqi),
			VASIP_SHARED_OFFSET(wlc->hw, m2v_buf_ru_alloc),
			VASIP_SHARED_OFFSET(wlc->hw, v2m_buf_ru_alloc));
	}
	bcm_bprintf(b,
			"      PPR table address:           0x%x\n"
			"      STA rate_cap address:        0x%x\n",
			VASIP_SHARED_OFFSET(wlc->hw, txbf_ppr_tbl),
			VASIP_SHARED_OFFSET(wlc->hw, mcs_map));

#if (VASIP_FW_VER >= VASIP_FW_VER_NUM(6, 1))
	// error[]: err_code, err_count, err_hang_mask
	//          err_code_group, err_code_precoder, err_code_cqi, err_code_ru_alloc
	wlc_dump_vasip_error_mu(b, error[1], error[3], error[4]);
	//wlc_dump_vasip_error_cqi
	//wlc_dump_vasip_error_ru_alloc
#else
	// error[]: err_code, err_count
	wlc_dump_vasip_error_mu(b, error[1], error[0]);
#endif // endif

	bcm_bprintf(b, "\n");
	return BCME_OK;
}

int
wlc_dump_mu_snr_calib(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint16 snr_calib_val[16];
	uint16 snr_calib_en;
	uint16 m, n, k, unit;
	uint16 size_val, size_val_per_ss;
	uint32 addr_val, addr_en;
	int value;

	wlc_hw_info_t *wlc_hw = wlc->hw;

	/* IOVAR works only if VASIP is present && VASIP FW is loaded */
	if (!VASIP_PRESENT(wlc_hw)) {
		bcm_bprintf(b, "VASIP is not present!\n");
		return BCME_UNSUPPORTED;
	}
	if (!wlc_hw->vasip_loaded) {
		bcm_bprintf(b, "VASIP FW is not loaded!\n");
		return BCME_UNSUPPORTED;
	}

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	addr_en  = VASIP_SHARED_OFFSET(wlc->hw, snr_calib_en);
	addr_val = VASIP_SHARED_OFFSET(wlc->hw, snr_calib_value);
	size_val = VASIP_SHARED_SIZE(wlc->hw, snr_calib_value);

	if ((size_val == 0) || (addr_val == VASIP_ADDR_INVALID) ||
			(addr_en == VASIP_ADDR_INVALID)) {
		bcm_bprintf(b, "MU_snr_calib is not supported!\n");
		return BCME_OK;
	}

	wlc_svmp_mem_read1(wlc_hw, addr_en, &snr_calib_en);
	if (snr_calib_en == 0) {
		bcm_bprintf(b, "MU_snr_calib is not active! "
			"Please check 'wl mu_snr_calib' or start MU traffic\n");
		return BCME_OK;
	}

	unit = 16;

	bcm_bprintf(b, "MU_snr_calib value (dB):\n");
	size_val_per_ss = size_val >> 1;
	for (m = 0; m < 2; m++) {
		bcm_bprintf(b, "%dss:\n", m+1);
		for (n = 0; n < size_val_per_ss; n += unit) {
			bcm_bprintf(b, "  user %2d~%2d: ", n, n+unit-1);
			wlc_svmp_mem_read(wlc_hw, addr_val, unit, snr_calib_val);
			addr_val += unit;
			for (k = 0; k < unit; k++) {
				value = (snr_calib_val[k] > 32767) ?
					(snr_calib_val[k] - 65536) : snr_calib_val[k];
				bcm_bprintf(b, " %3d,", value>>2);
			}
			bcm_bprintf(b, "\n");
		}
	}
	return BCME_OK;
}

/* clear vasip counters */
int
wlc_vasip_counters_clear(wlc_hw_info_t *wlc_hw)
{
	int i;
	uint32 offset;

	// checked in the beginning of wlc_vasip_doiovar
	//if (!VASIP_PRESENT(wlc_hw)) {
	//	return BCME_UNSUPPORTED;
	//}
	//if (!wlc_hw->clk) {
	//	return BCME_NOCLK;
	//}

	offset = VASIP_SHARED_OFFSET(wlc_hw, interrupt);
	for (i = 0; i < VASIP_SHARED_SIZE(wlc_hw, interrupt); i++) {
		wlc_svmp_mem_set1(wlc_hw, offset+i, 0);
	}
	return BCME_OK;
}
#endif // endif

#if defined(BCMDBG) || defined(TESTBED_AP_11AX)
/* for internal debug IOVAR use, assume that upper function has checked vasip_present/clk */
static void
wlc_svmp_mem_set1(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 val)
{
#ifndef SVMP_ACCESS_VIA_PHYTBL
	volatile uint16 * svmp_addr = (volatile uint16 *)wlc_vasip_addr(wlc_hw, offset);
	*svmp_addr = val;
#else
	phy_vasip_write_svmp(wlc_hw->band->pi, offset, val);
#endif // endif
}

static void
wlc_svmp_mem_read1(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 *val)
{
#ifndef SVMP_ACCESS_VIA_PHYTBL
	volatile uint16 *svmp_addr = (uint16 *)wlc_vasip_addr(wlc_hw, offset);
	*val = *svmp_addr;
#else
	phy_vasip_read_svmp(wlc_hw->band->pi, offset, val);
#endif // endif
}
#endif // endif

/* copy svmp memory to a buffer starting from offset of length 'len', len is count of uint16's */
int
wlc_svmp_mem_read(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 *val)
{
#ifndef SVMP_ACCESS_VIA_PHYTBL
	uint16 * svmp_addr;
	uint16 i;
#endif // endif
	uint32 svmp_mem_offset_max;

	if (!VASIP_PRESENT(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}

	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	svmp_mem_offset_max = wlc_svmp_mem_offset_max(wlc_hw);
	if ((offset + len) >= svmp_mem_offset_max) {
		return BCME_RANGE;
	}

#ifndef SVMP_ACCESS_VIA_PHYTBL
	svmp_addr = (uint16 *)wlc_vasip_addr(wlc_hw, offset);
	for (i = 0; i < len; i++) {
		val[i] = svmp_addr[i];
	}
#else
	phy_vasip_read_svmp_blk(wlc_hw->band->pi, offset, len, val);
#endif // endif

	return BCME_OK;
}

/* set svmp memory with a value from offset of length 'len', len is count of uint16's */
int
wlc_svmp_mem_set(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 val)
{
#ifndef SVMP_ACCESS_VIA_PHYTBL
	volatile uint16 * svmp_addr;
#endif // endif
	uint16 i;
	uint32 svmp_mem_offset_max;

	if (!VASIP_PRESENT(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}

	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	svmp_mem_offset_max = wlc_svmp_mem_offset_max(wlc_hw);
	if ((offset + len) >= svmp_mem_offset_max) {
		return BCME_RANGE;
	}

#ifndef SVMP_ACCESS_VIA_PHYTBL
	svmp_addr = (volatile uint16 *)wlc_vasip_addr(wlc_hw, offset);
	for (i = 0; i < len; i++) {
		svmp_addr[i] = val;
	}
#else
	for (i = 0; i < len; i++) {
		phy_vasip_write_svmp(wlc_hw->band->pi, offset+i, val);
	}
#endif // endif

	return BCME_OK;
}

/* set svmp memory with a value from offset of length 'len', len is count of uint16's */
int
wlc_svmp_mem_blk_set(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 *val)
{
#ifndef SVMP_ACCESS_VIA_PHYTBL
	volatile uint16 * svmp_addr;
#endif // endif
	uint16 i;
	uint32 svmp_mem_offset_max;

	if (!VASIP_PRESENT(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}

	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	svmp_mem_offset_max = wlc_svmp_mem_offset_max(wlc_hw);
	if ((offset + len) >= svmp_mem_offset_max) {
		return BCME_RANGE;
	}

#ifndef SVMP_ACCESS_VIA_PHYTBL
	svmp_addr = (volatile uint16 *)wlc_vasip_addr(wlc_hw, offset);
#endif // endif

	for (i = 0; i < len; i++) {
		if (val[i] != 0xffff) {
#ifdef SVMP_ACCESS_VIA_PHYTBL
			phy_vasip_write_svmp(wlc_hw->band->pi, offset+i, val[i]);
#else
			svmp_addr[i] = val[i];
#endif // endif
		}
	}
	return BCME_OK;
}

#ifdef WL_AIR_IQ
/* copy svmp memory to a buffer starting from offset of length 'len', len is
 * count of uint64's
 */
int
wlc_svmp_mem_read64(wlc_hw_info_t *wlc_hw, uint64 *val, uint32 offset, uint16 len)
{
	uint64 * svmp_addr;
	uint16 i;

	if (((offset + (len * sizeof(*val))) > SVMP_MEM_OFFSET_MAX_BCM4365C0) ||
		(len * sizeof(*val)) > SVMP_MEM_DUMP_LEN_MAX) {
		return BCME_RANGE;
	}
	offset = offset >> 2;
	svmp_addr = (uint64 *)((uint64 *)wlc_vasip_addr(wlc_hw, offset));

	for (i = 0; i < len; i++) {
		val[i] = svmp_addr[i];
	}

	return BCME_OK;
}
/* set svmp memory with a value from offset of length 'len', len is count of uint16's */
int
wlc_svmp_mem_set_axi(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 val)
{
	volatile uint16 * svmp_addr;
	uint16 i;

	svmp_addr = (volatile uint16 *)wlc_vasip_addr(wlc_hw, offset);

	for (i = 0; i < len; i++) {
		svmp_addr[i] = val;
	}
	return BCME_OK;
}

int
wlc_svmp_mem_read_axi(wlc_hw_info_t *wlc_hw, uint16 *val, uint32 offset, uint16 len)
{
	uint16 * svmp_addr;
	uint16 i;

	svmp_addr = (uint16 *)wlc_vasip_addr(wlc_hw, offset);

	for (i = 0; i < len; i++) {
		val[i] = svmp_addr[i];
	}
	return BCME_OK;
}
#endif /* WL_AIR_IQ */

int
wlc_svmp_mem_offset_max(wlc_hw_info_t *wlc_hw)
{
	uint32 svmp_mem_offset_max;

	if D11REV_IS(wlc_hw->corerev, 65) {
		svmp_mem_offset_max = SVMP_MEM_OFFSET_MAX_BCM4365C0;
	} else if (D11REV_IS(wlc_hw->corerev, 129) || D11REV_IS(wlc_hw->corerev, 132)) {
		svmp_mem_offset_max = SVMP_MEM_OFFSET_MAX_BCM43684;
	} else if D11REV_IS(wlc_hw->corerev, 130) {
		svmp_mem_offset_max = SVMP_MEM_OFFSET_MAX_BCM63178;
	} else if D11REV_IS(wlc_hw->corerev, 131) {
		svmp_mem_offset_max = SVMP_MEM_OFFSET_MAX_BCM6710;
	} else {
		svmp_mem_offset_max = SVMP_MEM_OFFSET_MAX_NOT_SUPPORT;
	}

	return svmp_mem_offset_max;
}

/* **DO NOT CALL DIRECTLY. USE VASIP_SHARED_OFFSET() macro
 * This function is intended to be accessed by VASIP_SHARED_OFFSET()
 * 'sym' input is the enumerated symbol defined in d11vasip_code.h
 * This indexes the sym_map table to retrieve the offset for the
 * particular chip.
 */
uint32
vasip_shared_offset(wlc_hw_info_t *wlc_hw, unsigned int sym)
{
	wlc_vasip_info_t *vasip = wlc_hw->wlc->vasip;

	ASSERT(sym < vasipfw_symbol_count);
	ASSERT(vasip->sym_map);
	ASSERT(vasip->sym_map[sym] < wlc_svmp_mem_offset_max(wlc_hw));

	return vasip->sym_map[sym];
}

/* **DO NOT CALL DIRECTLY. USE VASIP_SHARED_SIZE() macro
 * The vasip_shared_offset input is the enumerated input from
 * 'sym' input is the enumerated symbol defined in d11vasip_code.h
 * This indexes the sym_map table to retrieve the offset for the
 * particular chip.
 */
uint32
vasip_shared_size(wlc_hw_info_t *wlc_hw, unsigned int sym)
{
	wlc_vasip_info_t *vasip = wlc_hw->wlc->vasip;

	ASSERT(sym < vasipfw_symbol_count);
	ASSERT(vasip->sym_map_size);
	ASSERT(vasip->sym_map_size[sym] > 0);

	return vasip->sym_map_size[sym];
}

/* Update rate capabilites in svmp for MU user indexed by bfm index */
void
wlc_svmp_update_ratecap(wlc_info_t *wlc, scb_t *scb, uint8 bfm_index, uint8 bfe_nr)
{
	uint16 mcs_bitmap[MCSSET_LEN];
	uint16 rate_cap = 0, mcscap_sz, mcscap_offset;
	uint8 sgi;
	uint32 offset;

	wlc_scb_ratesel_get_ratecap(wlc->wrsi, scb, &sgi, mcs_bitmap, 0 /* AC_BE */);

	/* unused(8) | nrx(2) | brcmSTA(1) | ldpc(1) | sgi(2) | bw(2) |
	 * mcs_nss0(16) | mcs_nss1(16) | mcs_nss2(16) | mcs_nss3(16)
	 */
	rate_cap = (wlc_scb_ratesel_get_link_bw(wlc, scb) - 1);
	rate_cap |= sgi << VASIP_RTCAP_SGI_NBIT;
	if (SCB_HT_LDPC_CAP(scb) || SCB_VHT_LDPC_CAP(wlc->vhti, scb) ||
		SCB_HE_LDPC_CAP(wlc->hei, scb)) {
		rate_cap |= 1 << VASIP_RTCAP_LDPC_NBIT;
	}
	if (scb->flags & SCB_BRCM) {
		rate_cap |= 1 << VASIP_RTCAP_BCMSTA_NBIT;
	}
	rate_cap |= bfe_nr << VASIP_RTCAP_NRX_NBIT;

	offset = VASIP_SHARED_OFFSET(wlc->hw, mcs_map);
	if (IS_V2M_FORMAT_V3(wlc->hw->corerev)) {
		mcscap_offset = 3;
		mcscap_sz = 2;
	} else {
		mcscap_offset = 5;
		mcscap_sz = 4;
	}

	wlc_svmp_mem_blk_set(wlc->hw, offset + (bfm_index * mcscap_offset), 1, &rate_cap);
	wlc_svmp_mem_blk_set(wlc->hw, offset + 1 + (bfm_index * mcscap_offset),
		mcscap_sz, mcs_bitmap);

	WL_MUTX(("wl%d: %s Update rate_cap:%04x for STA "MACF" bfmidx %d\n",
		wlc->pub->unit, __FUNCTION__, rate_cap, ETHER_TO_MACF(scb->ea), bfm_index));
}

#endif /* WLVASIP */
