/*
 * ACPHY RadarDetect module implementation
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
 * $Id: phy_ac_radar.c 788019 2020-06-18 08:46:01Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include "phy_type_radar.h"
#include "phy_radar_shared.h"
#include <phy_ac.h>
#include <phy_ac_radar.h>

#include <phy_utils_reg.h>

#include <wlc_phyreg_ac.h>
#include <phy_ac_info.h>

/* module private states */
struct phy_ac_radar_info {
	phy_info_t *pi;
	phy_ac_info_t *aci;
	phy_radar_info_t *ri;
};

/* local functions */
static int phy_ac_radar_init(phy_type_radar_ctx_t *ctx, bool on);
static uint8 phy_ac_radar_run(phy_type_radar_ctx_t *ctx,
	radar_detected_info_t *radar_detected, bool sec_pll, bool bw80_80_mode);
static void phy_radar_init_st(phy_info_t *pi, phy_radar_st_t *st);
static int phy_ac_radar_set_thresholds(phy_type_radar_ctx_t *ctx, wl_radar_thr_t *thresholds);
static void phy_ac_radar_tuning_reset(phy_type_radar_ctx_t *ctx);
static void phy_ac_radar_war(phy_info_t *pi);

/* Register/unregister ACPHY specific implementation to common layer. */
phy_ac_radar_info_t *
BCMATTACHFN(phy_ac_radar_register_impl)(phy_info_t *pi, phy_ac_info_t *aci, phy_radar_info_t *ri)
{
	phy_ac_radar_info_t *info;
	phy_type_radar_fns_t fns;
	phy_radar_st_t *st;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage in once */
	if ((info = phy_malloc(pi, sizeof(phy_ac_radar_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	info->pi = pi;
	info->aci = aci;
	info->ri = ri;

	/* Register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.init = phy_ac_radar_init;
	fns.run = phy_ac_radar_run;
	fns.set_thresholds = phy_ac_radar_set_thresholds;
	fns.reset = phy_ac_radar_tuning_reset;
	fns.ctx = info;

	if (phy_radar_register_impl(ri, &fns) != BCME_OK) {
		PHY_ERROR(("%s: phy_radar_register_impl failed\n", __FUNCTION__));
		goto fail;
	}

	/* init radar states */
	st = phy_radar_get_st(ri);
	ASSERT(st != NULL);

	phy_radar_init_st(pi, st);
	if (PHY_SUPPORT_SCANCORE(pi) || PHY_SUPPORT_BW80P80(pi)) {
		if (!(st->radar_work_lp_sc =
				(radar_lp_info_t *)phy_malloc(pi, sizeof(radar_lp_info_t)))) {
			PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
			goto fail;
		}
		if (!(st->radar_status_sc =
				(wl_radar_status_t *)phy_malloc(pi, sizeof(wl_radar_status_t)))) {
			PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
			goto fail;
		}
	}
	return info;

fail:
	phy_ac_radar_unregister_impl(info);

	return NULL;
}

void
BCMATTACHFN(phy_ac_radar_unregister_impl)(phy_ac_radar_info_t *info)
{
	phy_info_t *pi;
	phy_radar_info_t *ri;
	phy_radar_st_t *st;

	if (info == NULL) {
		return;
	}

	pi = info->pi;
	ri = info->ri;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (PHY_SUPPORT_SCANCORE(pi) || PHY_SUPPORT_BW80P80(pi)) {
		st = phy_radar_get_st(ri);
		if (st->radar_work_lp_sc != NULL)
			phy_mfree(pi, st->radar_work_lp_sc, sizeof(radar_lp_info_t));
		if (st->radar_status_sc != NULL)
			phy_mfree(pi, st->radar_status_sc, sizeof(wl_radar_status_t));
	}
	phy_radar_unregister_impl(ri);

	phy_mfree(pi, info, sizeof(phy_ac_radar_info_t));
}

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_2cores) = {
	WL_RADAR_THR_VERSION,
	0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30,
	0x6c8, 0x30, 0x6d0, 0x30
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_4360) = {
	WL_RADAR_THR_VERSION,
	0x698, 0x20, 0x698, 0x20, 0x698, 0x20, 0x698, 0x20, 0x698, 0x20, 0x698, 0x20,
	0x698, 0x20, 0x698, 0x20
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_2cores_4366) = {
	WL_RADAR_THR_VERSION,
	0x6ac, 0x30, 0x6ac, 0x30, 0x6b4, 0x30, 0x6b4, 0x30, 0x6b4, 0x30, 0x6b4, 0x30,
	0x6b4, 0x30, 0x6b4, 0x30
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_2cores_4366_lesi) = {
	WL_RADAR_THR_VERSION,
	0x6a8, 0x20, 0x6ac, 0x20, 0x6b4, 0x20, 0x6ac, 0x20, 0x6b4, 0x20, 0x6b4, 0x20,
	0x6b4, 0x20, 0x6b4, 0x20,
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_axphy_43684) = {
	WL_RADAR_THR_VERSION,
	0x6b0, 0x30, 0x6b0, 0x30, 0x6b0, 0x30, 0x6a8, 0x30, 0x6b0, 0x30, 0x6a8, 0x30,
	0x6ac, 0x28, 0x6a8, 0x30,
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_axphy_43684_lesi) = {
	WL_RADAR_THR_VERSION,
	0x6a0, 0x20, 0x6a4, 0x20, 0x6a4, 0x20, 0x6a0, 0x20, 0x6a4, 0x20, 0x6a8, 0x20,
	0x6a4, 0x20, 0x6a4, 0x20,
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_axphy_6710_lesi) = {
	WL_RADAR_THR_VERSION,
	0x6a4, 0x20, 0x6a0, 0x20, 0x6a0, 0x20, 0x6a0, 0x20, 0x6a4, 0x20, 0x6a4, 0x20,
	0x6ac, 0x20, 0x6a4, 0x20,
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_1core) = {
	WL_RADAR_THR_VERSION,
	0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30, 0x6a8, 0x30,
	0x6b8, 0x30, 0x6b0, 0x30
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_1core_4350) = {
	WL_RADAR_THR_VERSION,
	0x6a8, 0x30, 0x6a4, 0x30, 0x6a0, 0x30, 0x6ac, 0x30, 0x6a8, 0x30, 0x6a8, 0x30,
	0x6b8, 0x30, 0x6b0, 0x30
	/* Assume 3dB antenna gain, targeting -64dBm at input of antenna port */
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_43602) = {
	WL_RADAR_THR_VERSION,
	0x698, 0x18, 0x698, 0x18, 0x698, 0x18, 0x698, 0x18, 0x698, 0x18, 0x698, 0x18,
	0x6b8, 0x30, 0x6b0, 0x30
};

static const wl_radar_thr_t BCMATTACHDATA(wlc_phy_radar_thresh_acphy_6878) = {
	WL_RADAR_THR_VERSION,
	0x6a8, 0x20, 0x6a4, 0x20, 0x6a0, 0x20, 0x6a4, 0x20, 0x6a4, 0x20, 0x6a4, 0x20,
	0x6b0, 0x30, 0x6b0, 0x30
};

static const wl_radar_thr2_t BCMATTACHDATA(wlc_phy_radar_thresh2_acphy) = {
	WL_RADAR_THR_VERSION,
	0x6b4, 0x30, 0x6b4, 0x30, 0x6b4, 0x30, 0x6b4, 0x30, 0x6b4, 0x30, 0x6b8, 0x30,
	0x6b4, 0x30, 0x6b4, 0x30, //SC core
	0xa, 0xa, 0xa, 0x258, 0x258, 0x258, 0x258, 0x5124, 0xc,
	0x8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
	// fc_varth_sb = 10, fc_varth_bin5_sb = 10
	// notradar_enb = 2'b1010, [3]: normal core lp, [2]: normal core non-lp
	// [1]: scan core lp, [0]: scan core non-lp
	// max_notradar_lp = 600, max_notradar = 600
	// max_notradar_lp_sc = 600, max_notradar_sc = 600
	// highpow_war_enb = 2'b0110, [0]: highpow WAR, [1]: chirp criterion on/off
	// [2-3]: min_fm_lp of scan core
	// [4-6]: good_chirp_tollerance
	// [7-9]: bad chirp count skip limit
	// [10-12]: csect_single_lp_decr
	// [13]: chirp check on/off
	// [14]: fc variation check on/off
	// [15]: upper bw20 chirp relax for the whole 20MHz band instead
	// of just upper band edge(chirp_relax_lv2 == TRUE)
	// highpow_sp_ratio = 5
	// fm_chk_opt = 2'b1000, [0]: fm check debug mode, [1]: absolute fm check,
	// [2]: fm variation check, [3]: ETSI4 absolute fm check
	// fm_chk_pw = 0x0, [0-7]: fm_chk_pw_thrs2 = 0,
	// [8-15]: fm_chk_pw_thrs1 = 0, fm_var_chk_pw = 0
	// fm_thresh_sp1 = 0, fm_thresh_sp2 = 0, fm_thresh_sp3 = 0
	// fm_thresh_etsi4 = 0, fm_thresh_p1c = 0, fm_tol_div = 0
};

static const wl_radar_thr2_t BCMATTACHDATA(wlc_phy_radar_thresh2_axphy_43684) = {
	WL_RADAR_THR_VERSION,
	0x6a4, 0x30, 0x6ac, 0x30, 0x6b0, 0x30, 0x6a4, 0x30, 0x6a4, 0x30, 0x6b0, 0x30,
	0x6b0, 0x30, 0x6ac, 0x30, 0xa, 0xa, 0xa, 0x258, 0x258, 0x258, 0x258, 0x7126, 0xc,
	0xe, 0x1941, 0x19, 0x0, 0x14, 0x64, 0xf0, 0xfffb, 0x5
};

static const wl_radar_thr2_t BCMATTACHDATA(wlc_phy_radar_thresh2_axphy_atlas_2Gp5G) = {
	WL_RADAR_THR_VERSION,
	0x6a4, 0x30, 0x6a0, 0x30, 0x6a8, 0x30, 0x6a4, 0x30, 0x6a4, 0x30, 0x6ac, 0x30,
	0x6b0, 0x30, 0x6ac, 0x30, 0xa, 0xa, 0xa, 0x258, 0x258, 0x258, 0x258, 0x7126, 0xc,
	0xe, 0x1941, 0x19, 0x0, 0x14, 0x64, 0xf0, 0xfffb, 0x5
};

static void
BCMATTACHFN(phy_radar_init_st)(phy_info_t *pi, phy_radar_st_t *st)
{
	bool is43684mch2 = ACMAJORREV_47(pi->pubpi->phy_rev) && CHSPEC_IS2G(pi->radio_chanspec) &&
		!BF_ELNA_5G(pi->u.pi_acphy);

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* 20Mhz channel radar thresholds */
	st->rparams.radar_thrs = (GET_RDR_NANTENNAS(pi) == 1)
							? wlc_phy_radar_thresh_acphy_1core
							: wlc_phy_radar_thresh_acphy_2cores;

	if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
		st->rparams.radar_thrs2 = is43684mch2 ? wlc_phy_radar_thresh2_axphy_atlas_2Gp5G :
			wlc_phy_radar_thresh2_axphy_43684;
	} else {
		st->rparams.radar_thrs2 = wlc_phy_radar_thresh2_acphy;
	}

	if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
		ACMAJORREV_33(pi->pubpi->phy_rev)) {
		if (PHY_LESI_ON(pi))
			st->rparams.radar_thrs = wlc_phy_radar_thresh_acphy_2cores_4366_lesi;
		else
			st->rparams.radar_thrs = (GET_RDR_NANTENNAS(pi) == 1)
							? wlc_phy_radar_thresh_acphy_1core
							: wlc_phy_radar_thresh_acphy_2cores_4366;
	}

	if (ACMAJORREV_0(pi->pubpi->phy_rev) || ACMAJORREV_4(pi->pubpi->phy_rev)) {
		/* 4360 or 53573/47189 */
		st->rparams.radar_thrs = wlc_phy_radar_thresh_acphy_4360;
	} else if (ACMAJORREV_2(pi->pubpi->phy_rev)) {
		st->rparams.radar_thrs = wlc_phy_radar_thresh_acphy_1core_4350;
	} else if (ACMAJORREV_5(pi->pubpi->phy_rev)) {
		st->rparams.radar_thrs = wlc_phy_radar_thresh_acphy_43602;
	} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
		st->rparams.radar_thrs = wlc_phy_radar_thresh_acphy_6878;
	} else if (ACMAJORREV_129(pi->pubpi->phy_rev) && PHY_LESI_ON(pi)) {
		st->rparams.radar_thrs = wlc_phy_radar_thresh_axphy_6710_lesi;
	} else if (ACMAJORREV_GE47(pi->pubpi->phy_rev)) {
		st->rparams.radar_thrs = PHY_LESI_ON(pi) ? wlc_phy_radar_thresh_axphy_43684_lesi :
			wlc_phy_radar_thresh_axphy_43684;
	}

	/* 20Mhz channel radar params */
	st->rparams.min_deltat_lp = 19000;	/* 1e-3*20e6 - small error	*/
	st->rparams.max_deltat_lp = 84000;	/* 2*2e-3*20e6 + small error	*/
	st->rparams.radar_args.nskip_rst_lp = 2;
	st->rparams.radar_args.min_burst_intv_lp = 12000000;
	st->rparams.radar_args.max_burst_intv_lp = 90000000;
	st->rparams.radar_args.quant = 16;
	st->rparams.radar_args.ncontig = 55280; /* 0xd7f0; */

	/* [100 100 1000 100011]=[1001 0010 0010 0011]=0x9223 = 37411
	 * bits 15-13: JP2_1, JP4 npulses = 4
	 * bits 12-10: JP1_2_JP2_3 npulses = 4
	 * bits 9-6: EU-t4 fm tol = 8, (8/16)
	 * bit 5-0: max detection index = 35
	 */

	st->rparams.radar_args.max_pw = 690;  /* 30us + 15% */
	st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_20_lo;
	st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_20_lo;
	st->rparams.radar_args.thresh0_sc = st->rparams.radar_thrs2.thresh0_sc_20_lo;
	st->rparams.radar_args.thresh1_sc = st->rparams.radar_thrs2.thresh1_sc_20_lo;
	st->rparams.radar_args.fmdemodcfg = 0x7f09;
	/* autocorr[0] = ON/OFF 6878 EXT_PW_TOL_SET; autocorr[4:1] = mode for 6878 EXT_PW_TOL_SET */
	/* autocorr[15:5] = PRI var check for stagger radar */
	st->rparams.radar_args.autocorr = 0x191e;
	/* it is used to check pw for acphy. if pw > 30us, then check fm */
	st->rparams.radar_args.st_level_time = 0x8258;
	st->rparams.radar_args.min_pw = 0;
	st->rparams.radar_args.max_pw_tol = 12;
	if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
		ACMAJORREV_33(pi->pubpi->phy_rev)) {
		if (PHY_LESI_ON(pi)) {
			st->rparams.radar_args.npulses = 6; /* 6; */
			st->rparams.radar_args.npulses_lp = 9; /* 8; */
		} else {
			st->rparams.radar_args.npulses = 7; /* 6; */
			st->rparams.radar_args.npulses_lp = 12; /* 8; */
		}
	} else if (ACMAJORREV_GE47(pi->pubpi->phy_rev) && !ACMAJORREV_128(pi->pubpi->phy_rev)) {
		st->rparams.radar_args.npulses = PHY_LESI_ON(pi) ? 6 : 7;
		st->rparams.radar_args.npulses_lp = PHY_LESI_ON(pi) ? 9 : 10;
	} else {
		st->rparams.radar_args.npulses = 7;
		st->rparams.radar_args.npulses_lp = 9;
	}
	st->rparams.radar_args.t2_min = 26432;	/* 0x6740 */
#ifdef BIN5_RADAR_DETECT_WAR
	st->rparams.radar_args.npulses_lp = 6;
	st->rparams.radar_args.t2_min = 31488;
#endif // endif
#ifdef BIN5_RADAR_DETECT_WAR_J28
	st->rparams.radar_args.npulses_lp = 8;
	st->rparams.radar_args.st_level_time &= 0x0fff;
#endif // endif
	/* t2_min[15:12] = x; if n_non_single >= x && lp_length >
	 * npulses_lp => bin5 detected
	 * t2_min[11:10] = # times combining adjacent pulses < min_pw_lp
	 * t2_min[9] = fm_tol enable
	 * t2_min[8] = not used
	 * t2_min[7:4] = y; bin5 remove pw <= 10*y
	 * t2_min[3:0] = t; non-bin5 remove pw <= 5*y
	 * st_level_time[11:0] =  pw criterion for short pluse noise filter
	 * st_level_time[15:12] =  2^x-1 as FMOFFSET
	 */
	st->rparams.radar_args.min_pw_lp = 960;
#ifdef BIN5_RADAR_DETECT_WAR
	st->rparams.radar_args.min_pw_lp = 50;
#endif // endif
	st->rparams.radar_args.max_pw_lp = 2020;
	if (TONEDETECTION)
		st->rparams.radar_args.min_fm_lp = 500 - 256;
	else
#ifdef NPHYREV7_HTPHY_DFS_WAR
		st->rparams.radar_args.min_fm_lp = 25;
#else
		st->rparams.radar_args.min_fm_lp = 45;
#endif // endif

#ifdef BIN5_RADAR_DETECT_WAR_J28
	st->rparams.radar_args.min_fm_lp  = 20;
#endif // endif

	if (TONEDETECTION)
		if (ACMAJORREV_GE32((pi)->pubpi->phy_rev)) {
			st->rparams.radar_args.max_span_lp = 45324;  /* 0xb10c; 1, 3, 1, 12 */
		} else {	/* 4360 acphy */
			st->rparams.radar_args.max_span_lp = 45588;  /* 0xb214; 1, 3, 1, 20 */
		}
	else
		st->rparams.radar_args.max_span_lp = 62476;  /* 0xf40c; 15, 4, 12 */
	/* max_span_lp[15] = tot_lp_cnt check on/off */
	/* max_span_lp[14:12] = tot_lp_cnt min */
	/* max_span_lp[11:8] = x, x/16 = % alowed fm tollerance bin5 */
	/* max_span_lp[7:0] = alowed pw tollerance bin5 */

	st->rparams.radar_args.min_deltat = 2000;
	st->rparams.radar_args.version = WL_RADAR_ARGS_VERSION;
	if (TONEDETECTION) {
		st->rparams.radar_args.fra_pulse_err = 65283; /* 0xff03, */
		/* bits 15-8: EU-t4 min_fm = 255 */
		/* bits 7-0: time from last det = 2 minute */
	} else {
		st->rparams.radar_args.fra_pulse_err = 4098; /* 0x1002, */
		/* bits 15-8: EU-t4 min_fm = 16 */
		/* bits 7-0: time from last det = 2 minute */
	}

	/* bits 15:12 for New JP, bits 11:8 for EU type 4, */
	/* bits 7:4 = 4 for EU type 2, bits 3:0= 4 for EU type 1 */
	/* 0111 0110 0110 0110 */
	st->rparams.radar_args.npulses_fra = 30310;  /* 0x7666 */
	st->rparams.radar_args.percal_mask = 0x31;
	if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
		ACMAJORREV_33(pi->pubpi->phy_rev)) {
		st->rparams.radar_args.npulses_stg2 = 7; /* 5; */
		if (PHY_LESI_ON(pi)) {
			st->rparams.radar_args.npulses_stg3 = 5; /* 5; */
		} else {
			st->rparams.radar_args.npulses_stg3 = 6; /* 5; */
		}
		st->rparams.radar_args.feature_mask = 0xa800;
		st->rparams.radar_args.blank = 0x6419;
	} else if (ACMAJORREV_GE47(pi->pubpi->phy_rev)) {
		st->rparams.radar_args.npulses_stg2 = 7;
		st->rparams.radar_args.npulses_stg3 = 5;
		st->rparams.radar_args.feature_mask = 0xa800;
		st->rparams.radar_args.blank = 0x6419;
	} else {	/* 11ac phy */
		st->rparams.radar_args.npulses_stg2 = 7;
		st->rparams.radar_args.npulses_stg3 = 6;
		st->rparams.radar_args.feature_mask =
			RADAR_FEATURE_USE_MAX_PW | RADAR_FEATURE_FCC_DETECT;
		/* RadarBlankCtrl has been hard coded to 0x2c19/0x2c32/0x2464 for acphy in init */
		/* bits 15:8: remove bin5 pulses < this value in combined pusles from 2 antenna */
		/* bits 7:0: remove short pulses < this value in combined pusles from 2 antenna */
		st->rparams.radar_args.blank = 0x7f14;
	}
}

static int
WLBANDINITFN(phy_ac_radar_init)(phy_type_radar_ctx_t *ctx, bool on)
{
	phy_ac_radar_info_t *info = (phy_ac_radar_info_t *)ctx;
	phy_radar_info_t *ri = info->ri;
	phy_info_t *pi = info->pi;
	phy_radar_st_t *st;
	uint16 k, bw_idx;

	uint16 radar_phyreg_vals0[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xac19, 0xac32, 0xac64},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0x494, 0x494, 0x494},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3206, 0x3206, 0x3206},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0xc8, 0x190, 0x320},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x141, 0x141}};

	uint16 radar_phyreg_vals32_33[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val/bw80p80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xac19, 0xac32, 0xac64},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0x34a3, 0x34a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3237, 0x3237, 0x32f7},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x141, 0x141},
		{ACPHY_RadarBlankCtrl_SC(pi->pubpi->phy_rev), 0x8019, 0x8032, 0x8064},
		{ACPHY_RadarMaLength_SC(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_adc_to_dbm_SC(pi->pubpi->phy_rev), 0x34a0, 0x34a0, 0x34a0},
		{ACPHY_RadarBlankCtrl2_SC(pi->pubpi->phy_rev), 0xa000, 0xa000, 0xa000},
		{ACPHY_RadarDetectConfig1_SC(pi->pubpi->phy_rev), 0x3207, 0x3207, 0x3207},
		{ACPHY_RadarT3BelowMin_SC(pi->pubpi->phy_rev), 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout_SC(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay_SC(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarGainOverride_SC(pi->pubpi->phy_rev), 0x43d, 0x43d, 0x43d}};

	uint16 radar_phyreg_vals47[][5] =
		/* {regname, bw20_val, bw40_val, bw80_val, bw160_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xac19, 0xac32, 0xac64, 0xa4c8},
		{ACPHY_RadarMisc(pi->pubpi->phy_rev), 0x1, 0x1, 0x1, 0x1},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f, 0x0},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0xb4a3, 0xb4a3, 0xb4a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88, 0x5c88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3237, 0x3237, 0x3237, 0x3237},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960, 0x12c0},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64, 0xc8},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x341, 0x541, 0x741},
		{ACPHY_RadarBlankCtrl_SC(pi->pubpi->phy_rev), 0x8019, 0x8032, 0x8064, 0x80c8},
		{ACPHY_RadarMaLength_SC(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f, 0x0},
		{ACPHY_Radar_adc_to_dbm_SC(pi->pubpi->phy_rev), 0x34a0, 0x34a0, 0x34a0, 0x34a0},
		{ACPHY_RadarBlankCtrl2_SC(pi->pubpi->phy_rev), 0xa000, 0xa000, 0xa000, 0xa000},
		{ACPHY_RadarDetectConfig1_SC(pi->pubpi->phy_rev), 0x3207, 0x3207, 0x3207, 0x3207},
		{ACPHY_RadarT3BelowMin_SC(pi->pubpi->phy_rev), 0x14, 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout_SC(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960, 0x12c0},
		{ACPHY_RadarResetBlankingDelay_SC(pi->pubpi->phy_rev), 0x19, 0x32, 0x64, 0xc8},
		{ACPHY_RadarGainOverride_SC(pi->pubpi->phy_rev), 0x43d, 0x43d, 0x43d, 0x43d}};

	uint16 radar_phyreg_vals51[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xa419, 0xac32, 0xac64},
		{ACPHY_RadarMisc(pi->pubpi->phy_rev), 0x1, 0x1, 0x1},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0x34a3, 0x34a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3237, 0x3237, 0x3237},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x341, 0x541}};

	uint16 radar_phyreg_vals128[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xac19, 0xac32, 0xa432},
		{ACPHY_RadarMisc(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0xb4a3, 0x34a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3237, 0x3237, 0x3237},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x28, 0x50},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x341, 0x541}};

	uint16 radar_phyreg_vals129[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xa419, 0xa432, 0xac64},
		{ACPHY_RadarMisc(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0x34a3, 0x34a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3237, 0x3237, 0x3237},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x341, 0x541}};

	uint16 radar_phyreg_vals_others[][4] =
		/* {regname, bw20_val, bw40_val, bw80_val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xac19, 0xac32, 0xac64},
		{ACPHY_RadarMisc(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_RadarMaLength(pi->pubpi->phy_rev), 0x08, 0x10, 0x1f},
		{ACPHY_Radar_t2_min(pi->pubpi->phy_rev), 0x0, 0x0, 0x0},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0x34a3, 0x34a3, 0x34a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5f88, 0x5f88, 0x5f88},
		{ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev), 0x3206, 0x3206, 0x3206},
		{ACPHY_RadarT3BelowMin(pi->pubpi->phy_rev), 0x14, 0x14, 0x14},
		{ACPHY_RadarT3Timeout(pi->pubpi->phy_rev), 0x258, 0x4b0, 0x960},
		{ACPHY_RadarResetBlankingDelay(pi->pubpi->phy_rev), 0x19, 0x32, 0x64},
		{ACPHY_RadarDetectConfig2(pi->pubpi->phy_rev), 0x141, 0x141, 0x141}};

	PHY_TRACE(("%s: init %d\n", __FUNCTION__, on));

	st = phy_radar_get_st(ri);
	ASSERT(st != NULL);

	bw_idx = CHSPEC_IS20(pi->radio_chanspec) ? 1 : CHSPEC_IS40(pi->radio_chanspec) ? 2 :
		(CHSPEC_IS80(pi->radio_chanspec) || PHY_AS_80P80(pi, pi->radio_chanspec)) ? 3 : 4;

	if (ACMAJORREV_47_130(pi->pubpi->phy_rev))
		ASSERT(bw_idx < 5);
	else
		ASSERT(bw_idx < 4);

	/* Update radar_args according to the chanspec */
	if (CHSPEC_CHANNEL(pi->radio_chanspec) <= WL_THRESHOLD_LO_BAND) {
		if (CHSPEC_BW_LE20(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_20_lo;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_20_lo;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_40_lo;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_40_lo;
		} else if (CHSPEC_IS80(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_80_lo;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_80_lo;
		} else {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_160_lo;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_160_lo;
		}
	} else {
		if (CHSPEC_BW_LE20(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_20_hi;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_20_hi;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_40_hi;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_40_hi;
		} else if (CHSPEC_IS80(pi->radio_chanspec)) {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_80_hi;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_80_hi;
		} else {
			st->rparams.radar_args.thresh0 = st->rparams.radar_thrs.thresh0_160_hi;
			st->rparams.radar_args.thresh1 = st->rparams.radar_thrs.thresh1_160_hi;
		}
	}
	if (on) {
		phy_utils_write_phyreg(pi, ACPHY_RadarThresh0(pi->pubpi->phy_rev),
			(uint16)((int16)st->rparams.radar_args.thresh0));
		phy_utils_write_phyreg(pi, ACPHY_RadarThresh1(pi->pubpi->phy_rev),
			(uint16)((int16)st->rparams.radar_args.thresh1));
		phy_utils_write_phyreg(pi, ACPHY_RadarThresh0_core1(pi->pubpi->phy_rev),
			(uint16)((int16)st->rparams.radar_args.thresh0));
		phy_utils_write_phyreg(pi, ACPHY_RadarThresh1_core1(pi->pubpi->phy_rev),
			(uint16)((int16)st->rparams.radar_args.thresh1));
		phy_utils_write_phyreg(pi, ACPHY_FMDemodConfig(pi->pubpi->phy_rev),
			st->rparams.radar_args.fmdemodcfg);

		wlapi_bmac_write_shm(pi->sh->physhim,
			M_RADAR_REG(pi), st->rparams.radar_args.thresh1);

		/* percal_mask to disable radar detection during selected period cals */
		pi->radar_percal_mask = st->rparams.radar_args.percal_mask;

		phy_utils_write_phyreg(pi, ACPHY_RadarSearchCtrl(pi->pubpi->phy_rev), 0x1);
	} else { /* handling radar disable request */
		phy_utils_write_phyreg(pi, ACPHY_RadarSearchCtrl(pi->pubpi->phy_rev), 0x0);
	}

	if (ACMAJORREV_0(pi->pubpi->phy_rev) ||
		ACMAJORREV_4(pi->pubpi->phy_rev)) {
		/* 4360 or 53573/47189 */
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals0); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals0[k][0],
				radar_phyreg_vals0[k][bw_idx]);
		}
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals32_33); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals32_33[k][0],
				radar_phyreg_vals32_33[k][bw_idx]);
		}
	} else if (ACMAJORREV_47_130(pi->pubpi->phy_rev)) {
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals47); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals47[k][0],
				radar_phyreg_vals47[k][bw_idx]);
		}
	} else if (ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals51); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals51[k][0],
				radar_phyreg_vals51[k][bw_idx]);
		}
	} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals128); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals128[k][0],
				radar_phyreg_vals128[k][bw_idx]);
		}
	} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals129); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals129[k][0],
				radar_phyreg_vals129[k][bw_idx]);
		}
	} else if (ACMAJORREV_GE47(pi->pubpi->phy_rev)) { /* for majorrev > 47 chips */
		for (k = 0; k < ARRAYSIZE(radar_phyreg_vals_others); k++) {
			phy_utils_write_phyreg(pi, radar_phyreg_vals_others[k][0],
				radar_phyreg_vals_others[k][bw_idx]);
		}
	} else if (TINY_RADIO(pi)) { /* for tiny radio chips */
		phy_utils_write_phyreg(pi, ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev),
			(ROUTER_4349(pi) && CHSPEC_CHANNEL(pi->radio_chanspec) <=
			WL_THRESHOLD_LO_BAND) ? 0x4a4 : 0x4ac);
		phy_utils_write_phyreg(pi, ACPHY_RadarDetectConfig1(pi->pubpi->phy_rev),
			0x2c06);
	}
	/* specific tunning */
	phy_ac_radar_war(pi);

	wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_RADARWAR, (on ? MHF1_RADARWAR : 0), FALSE);

	return BCME_OK;
}

static uint8
phy_ac_radar_run(phy_type_radar_ctx_t *ctx, radar_detected_info_t *radar_detected,
bool sec_pll, bool bw80_80_mode)
{
	phy_ac_radar_info_t *info = (phy_ac_radar_info_t *)ctx;
	phy_info_t *pi = info->pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	return phy_radar_run_nphy(pi, radar_detected, sec_pll, bw80_80_mode);
}

static int
phy_ac_radar_set_thresholds(phy_type_radar_ctx_t *ctx, wl_radar_thr_t *thresholds)
{
	phy_ac_radar_info_t *radari = (phy_ac_radar_info_t *)ctx;
	phy_radar_st_t *st = phy_radar_get_st(radari->ri);
	st->rparams.radar_thrs.thresh0_40_lo = thresholds->thresh0_40_lo;
	st->rparams.radar_thrs.thresh1_40_lo = thresholds->thresh1_40_lo;
	st->rparams.radar_thrs.thresh0_40_hi = thresholds->thresh0_40_hi;
	st->rparams.radar_thrs.thresh1_40_hi = thresholds->thresh1_40_hi;
	st->rparams.radar_thrs.thresh0_80_lo = thresholds->thresh0_80_lo;
	st->rparams.radar_thrs.thresh1_80_lo = thresholds->thresh1_80_lo;
	st->rparams.radar_thrs.thresh0_80_hi = thresholds->thresh0_80_hi;
	st->rparams.radar_thrs.thresh1_80_hi = thresholds->thresh1_80_hi;
	st->rparams.radar_thrs.thresh0_160_lo = thresholds->thresh0_160_lo;
	st->rparams.radar_thrs.thresh1_160_lo = thresholds->thresh1_160_lo;
	st->rparams.radar_thrs.thresh0_160_hi = thresholds->thresh0_160_hi;
	st->rparams.radar_thrs.thresh1_160_hi = thresholds->thresh1_160_hi;
	return BCME_OK;
}

static void
phy_ac_radar_tuning_reset(phy_type_radar_ctx_t *ctx)
{
	phy_ac_radar_info_t *info = (phy_ac_radar_info_t *)ctx;
	phy_info_t *pi = info->pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (ACMAJORREV_47(pi->pubpi->phy_rev) || ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
		phy_utils_write_phyreg(pi, ACPHY_RadarMisc(pi->pubpi->phy_rev), 0);
	}
	phy_utils_write_phyreg(pi, ACPHY_RadarSearchCtrl(pi->pubpi->phy_rev), 0);
}

static void
phy_ac_radar_war(phy_info_t *pi)
{
	uint16 radar_phyreg_war[][2] =
		/* {regname, val} */
		{{ACPHY_RadarBlankCtrl(pi->pubpi->phy_rev), 0xa419},
		{ACPHY_Radar_adc_to_dbm(pi->pubpi->phy_rev), 0xb4a3},
		{ACPHY_RadarBlankCtrl2(pi->pubpi->phy_rev), 0x5e88}};

	PHY_TRACE(("%s\n", __FUNCTION__));

	if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
		if (CHSPEC_IS20(pi->radio_chanspec) &&
			(CHSPEC_CHANNEL(pi->radio_chanspec) > WL_THRESHOLD_LO_BAND))
			phy_utils_write_phyreg(pi, radar_phyreg_war[0][0], radar_phyreg_war[0][1]);
	} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
		if (CHSPEC_IS20(pi->radio_chanspec) &&
			(CHSPEC_CHANNEL(pi->radio_chanspec) > WL_THRESHOLD_LO_BAND))
			phy_utils_write_phyreg(pi, radar_phyreg_war[0][0], radar_phyreg_war[0][1]);

		if (CHSPEC_IS80(pi->radio_chanspec) && (CHSPEC_CHANNEL(pi->radio_chanspec) <=
			WL_THRESHOLD_LO_BAND) && (READ_PHYREGFLD(pi, RadarBlankCtrl2,
			radarSigDecode1BlankEn) == 0))
			phy_utils_write_phyreg(pi, radar_phyreg_war[2][0], radar_phyreg_war[2][1]);
	} else if (ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
		if (CHSPEC_IS20(pi->radio_chanspec) &&
			(CHSPEC_CHANNEL(pi->radio_chanspec) > WL_THRESHOLD_LO_BAND))
			phy_utils_write_phyreg(pi, radar_phyreg_war[1][0], radar_phyreg_war[1][1]);

		if (CHSPEC_IS80(pi->radio_chanspec) &&
			(CHSPEC_CHANNEL(pi->radio_chanspec) > WL_THRESHOLD_LO_BAND))
			phy_utils_write_phyreg(pi, radar_phyreg_war[2][0], radar_phyreg_war[2][1]);
	}
}
