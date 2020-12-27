/*
 * ACPHY RSSI Compute module implementation
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
 * $Id: phy_ac_rssi.c 778561 2019-09-04 15:11:49Z $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <qmath.h>
#include <phy_dbg.h>
#include <phy_misc.h>
#include <phy_mem.h>
#include <phy_temp.h>
#include "phy_type_rssi.h"
#include <phy_ac.h>
#include <phy_ac_rssi.h>
#include <phy_ac_rxgcrs.h>
#include <phy_ac_info.h>
#include <phy_utils_var.h>
#include <phy_rstr.h>
#include <phy_utils_reg.h>
#include <phy_rssi_iov.h>
#include <phy_stf.h>
#include <phy_noise_api.h>

#ifndef ALL_NEW_PHY_MOD
/* < TODO: all these are going away... */
#include <wlc_phyreg_ac.h>
/* TODO: all these are going away... > */
#endif // endif

#define PHYHW_MEAS_RSSI_FOR_INACTIVE	(-128)
/* module private states */
struct phy_ac_rssi_info {
	phy_info_t *pi;
	phy_ac_info_t *aci;
	phy_rssi_info_t *ri;
	phy_ac_rssi_data_t *data; /* shared data */
	uint16	rxstats[NUM_80211_RATES + 1];
	int8	gain_err[PHY_CORE_MAX];
	uint8	rssi_coresel;
	int8	rssi[PHY_CORE_MAX];
	int16	last_rssi;
};

typedef struct {
	phy_ac_rssi_info_t info;
	phy_ac_rssi_data_t data;
} phy_ac_rssi_mem_t;

/* local functions */
static void wlc_phy_nvram_rssioffset_read(phy_info_t *pi);
static void wlc_phy_nvram_rssioffset_read_sub(phy_ac_rssi_info_t *ri, phy_info_t *pi);
static void phy_ac_rssi_compute(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh);
static void phy_ulofdma_per_user_rxstats(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh);
static void phy_tracked_freq_offset(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh);
static uint8 phy_ac_rssi_11b_WAR(phy_ac_info_t *aci, d11rxhdr_t *rxh);
static void _phy_ac_rssi_init_gain_err(phy_type_rssi_ctx_t *ctx);
static bool phy_ac_wd_report_rssi(phy_wd_ctx_t *ctx);
static int8 phy_ac_rssi_get_rssi(phy_type_rssi_ctx_t *ctx, uint8 core);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int phy_ac_rssi_dump(phy_type_rssi_ctx_t *ctx, struct bcmstrbuf *b);
#else
#define phy_ac_rssi_dump NULL
#endif // endif
#if defined(WLTEST)
static void phy_ac_rssi_update_pkteng_rxstats(phy_type_rssi_ctx_t *ctx, uint8 statidx);
static int phy_ac_rssi_get_pkteng_stats(phy_type_rssi_ctx_t *ctx, void *a, int alen,
	wl_pkteng_stats_t stats, int8 *gain_correct);
#endif // endif
static int phy_ac_rssi_set_gain_delta_2g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);
static int phy_ac_rssi_get_gain_delta_2g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);
static int phy_ac_rssi_set_gain_delta_5g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);
static int phy_ac_rssi_get_gain_delta_5g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);

static int phy_ac_rssi_set_gain_delta_2gb(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);
static int phy_ac_rssi_get_gain_delta_2gb(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues);
static int phy_ac_rssi_set_cal_freq_2g(phy_type_rssi_ctx_t *ctx, int8 *nvramValues);
static int phy_ac_rssi_get_cal_freq_2g(phy_type_rssi_ctx_t *ctx, int8 *nvramValues);
static int phy_ac_calc_ru_size(int ru_index);
static void phy_ac_ul_pwr_control(phy_type_rssi_ctx_t *ctx, bool trig_by_wd);
static void phy_ac_align_sniffer(phy_type_rssi_ctx_t *ctx);

/* register phy type specific implementation */
phy_ac_rssi_info_t *
BCMATTACHFN(phy_ac_rssi_register_impl)(phy_info_t *pi, phy_ac_info_t *aci, phy_rssi_info_t *ri)
{
	phy_ac_rssi_info_t *info;
	phy_type_rssi_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage together */
	if ((info = phy_malloc(pi, sizeof(phy_ac_rssi_mem_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	info->pi = pi;
	info->aci = aci;
	info->ri = ri;
	info->data = &(((phy_ac_rssi_mem_t *) info)->data);

	/* register watchdog fn */
	if (phy_wd_add_fn(pi->wdi, phy_ac_wd_report_rssi, info,
		PHY_WD_PRD_1TICK, PHY_WD_1TICK_AC_RSSI_REPORT,
		PHY_WD_FLAG_NONE) != BCME_OK) {
		PHY_ERROR(("%s: phy_wd_add_fn failed\n", __FUNCTION__));
		goto fail;
	}

	/* register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.compute = phy_ac_rssi_compute;
	fns.ulofdma_per_user_rxstats = phy_ulofdma_per_user_rxstats;
	fns.tracked_freq_offset = phy_tracked_freq_offset;
	fns.init_gain_err = _phy_ac_rssi_init_gain_err;
	fns.dump = phy_ac_rssi_dump;
#if defined(WLTEST)
	fns.update_pkteng_rxstats = phy_ac_rssi_update_pkteng_rxstats;
	fns.get_pkteng_stats = phy_ac_rssi_get_pkteng_stats;
#endif // endif
	fns.set_gain_delta_2g = phy_ac_rssi_set_gain_delta_2g;
	fns.get_gain_delta_2g = phy_ac_rssi_get_gain_delta_2g;
	fns.set_gain_delta_5g = phy_ac_rssi_set_gain_delta_5g;
	fns.get_gain_delta_5g = phy_ac_rssi_get_gain_delta_5g;

	fns.set_gain_delta_2gb = phy_ac_rssi_set_gain_delta_2gb;
	fns.get_gain_delta_2gb = phy_ac_rssi_get_gain_delta_2gb;
	fns.set_cal_freq_2g = phy_ac_rssi_set_cal_freq_2g;
	fns.get_cal_freq_2g = phy_ac_rssi_get_cal_freq_2g;

	fns.get_rssi = phy_ac_rssi_get_rssi;
	fns.ctx = info;
	/* RSSI reg reporintg only happens for one core at a time */
	info->rssi_coresel = 0;

	wlc_phy_nvram_rssioffset_read(pi);
	wlc_phy_nvram_rssioffset_read_sub(info, pi);

	phy_rssi_register_impl(ri, &fns);

	return info;

	/* error handling */
fail:
	if (info != NULL)
		phy_mfree(pi, info, sizeof(phy_ac_rssi_mem_t));
	return NULL;
}

void
BCMATTACHFN(phy_ac_rssi_unregister_impl)(phy_ac_rssi_info_t *info)
{
	phy_info_t *pi = info->pi;
	phy_rssi_info_t *ri = info->ri;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_rssi_unregister_impl(ri);

	phy_mfree(pi, info, sizeof(phy_ac_rssi_mem_t));
}

/* inter-module data API */
phy_ac_rssi_data_t *
phy_ac_rssi_get_data(phy_ac_rssi_info_t *rssii)
{
	return rssii->data;
}

static void
BCMATTACHFN(wlc_phy_nvram_rssioffset_read)(phy_info_t *pi)
{
	uint8 i, j, ant;
	uint8 core;
	char phy_var_name[40];
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint8 subband_idx;
	const char *subband_key[4] = {"l", "ml", "mu", "h"};

	pi_ac->sromi->rxgain_tempadj_2g =
		(int16)PHY_GETINTVAR(pi, rstr_rxgaintempcoeff2g);

	pi_ac->sromi->dot11b_opts =
		(int32)PHY_GETINTVAR(pi, rstr_dot11b_opts);

	for (i = 0; i < 3; i++) {
	    pi_ac->sromi->tiny_maxrxgain[i] =
	    (uint8) PHY_GETINTVAR_ARRAY_DEFAULT(pi, rstr_tiny_maxrxgain, i, 0);
	}

	if (PHY_BAND5G_ENAB(pi)) {
		pi_ac->sromi->rxgain_tempadj_5gl =
			(int16)PHY_GETINTVAR(pi, rstr_rxgaintempcoeff5gl);

		pi_ac->sromi->rxgain_tempadj_5gml =
			(int16)PHY_GETINTVAR(pi, rstr_rxgaintempcoeff5gml);

		pi_ac->sromi->rxgain_tempadj_5gmu =
			(int16)PHY_GETINTVAR(pi, rstr_rxgaintempcoeff5gmu);

		pi_ac->sromi->rxgain_tempadj_5gh =
			(int16)PHY_GETINTVAR(pi, rstr_rxgaintempcoeff5gh);
	}

	FOREACH_CORE(pi, core) {
		ant = phy_get_rsdbbrd_corenum(pi, core);
		(void)snprintf(phy_var_name, sizeof(phy_var_name), rstr_rssicorrnorm_cD, ant);
		if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
			for (i = 0; i < ACPHY_NUM_BW_2G; i++) {
				pi_ac->sromi->rssioffset.rssi_corr_normal[ant][i] =
				        (int8)PHY_GETINTVAR_ARRAY(pi, phy_var_name, i);
			}
		} else {
			for (i = 0; i < ACPHY_NUM_BW_2G; i++) {
				pi_ac->sromi->rssioffset.rssi_corr_normal[ant][i] = 0;
			}
		}

		(void)snprintf(phy_var_name, sizeof(phy_var_name), rstr_rssi_delta_2g_cD, ant);
		if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
			for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
				for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS; i++) {
				pi_ac->sromi->rssioffset.rssi_corr_gain_delta_2g[ant][i][j] =
				        (int8)PHY_GETINTVAR_ARRAY(pi, phy_var_name, (i+2*j));
				}
			}
		} else {
			for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
				for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS; i++) {
					pi_ac->sromi->rssioffset.
					        rssi_corr_gain_delta_2g[ant][i][j] = 0;
				}
			}
		}

		for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
			for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS; i++) {
				PHY_INFORM(("%d ", pi_ac->sromi->rssioffset.rssi_corr_gain_delta_2g
				            [ant][i][j]));
			}
		}

		if (PHY_BAND5G_ENAB(pi)) {
			(void)snprintf(phy_var_name, sizeof(phy_var_name),
				rstr_rssicorrnorm5g_cD, ant);
			if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
				for (i = 0; i < ACPHY_RSSIOFFSET_NVRAM_PARAMS; i++) {
					for (j = 0; j < ACPHY_NUM_BW; j++) {
						pi_ac->sromi->rssioffset.
							rssi_corr_normal_5g[ant][i][j] =
							(int8)PHY_GETINTVAR_ARRAY(pi, phy_var_name,
							(3*i+j));
					}
				}
			} else {
				for (i = 0; i < ACPHY_RSSIOFFSET_NVRAM_PARAMS; i++) {
					for (j = 0; j < ACPHY_NUM_BW; j++) {
						pi_ac->sromi->rssioffset.
							rssi_corr_normal_5g[ant][i][j] = 0;
					}
				}
			}

			for (subband_idx = 0; subband_idx < CH_5G_4BAND; subband_idx++) {
				(void)snprintf(phy_var_name, sizeof(phy_var_name),
					rstr_rssi_delta_5gS_cD, subband_key[subband_idx], ant);

				if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
					for (j = 0; j < ACPHY_NUM_BW; j++) {
						for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS; i++) {
							pi_ac->sromi->rssioffset.
								rssi_corr_gain_delta_5g
							    [ant][i][j][subband_idx] = (int8)
							    PHY_GETINTVAR_ARRAY(pi, phy_var_name,
							    (i+2*j));
						}
					}
				} else {
					for (j = 0; j < ACPHY_NUM_BW; j++) {
						for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS; i++) {
							pi_ac->sromi->rssioffset.
								rssi_corr_gain_delta_5g
								[ant][i][j][subband_idx] = 0;
						}
					}
				}
			}

			for (subband_idx = 0; subband_idx < CH_5G_4BAND; subband_idx++) {
				for (j = 0; j < ACPHY_NUM_BW; j++) {
					for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS; i++) {
					PHY_INFORM(("%d ", pi_ac->sromi->rssioffset.
						rssi_corr_gain_delta_5g[ant][i][j][subband_idx]));
					}
				}
				PHY_INFORM(("\n"));
			}
		}
	}
}

static void
BCMATTACHFN(wlc_phy_nvram_rssioffset_read_sub)(phy_ac_rssi_info_t *ri, phy_info_t * pi)
{
	uint8 i, j, k, flag;
	uint8 core, ant;
	char phy_var_name[40];
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint8 subband_idx;
	const char *subband_key[4] = {"l", "ml", "mu", "h"};
	const char *subband_key_2g[5] = {"b0", "b1", "b2", "b3", "b4"};

	/* if gain_cal_temp is not set in nvram, set it to 255 by default
	 * to disable temperature correction for rssi
	 */
	pi->srom_gain_cal_temp = (int16)PHY_GETINTVAR_DEFAULT(pi, "gain_cal_temp", 255);

	ri->data->rssi_cal_rev = (bool)PHY_GETINTVAR(pi, rstr_rssi_cal_rev);

	if (ACMAJORREV_40_128(pi->pubpi->phy_rev)) {
		/* enable rssi_qdB_en by default for REV 40 & 44 */
		ri->data->rssi_qdB_en = (bool)PHY_GETINTVAR_DEFAULT(pi, rstr_rssi_qdB_en, 1);
	} else
		ri->data->rssi_qdB_en = (bool)PHY_GETINTVAR(pi, rstr_rssi_qdB_en);

	ri->data->rxgaincal_rssical = (bool)PHY_GETINTVAR(pi, rstr_rxgaincal_rssical);

	(void)snprintf(phy_var_name, sizeof(phy_var_name), "%s",
	               rstr_num_rssi_cal_gi_2g);
	pi_ac->sromi->num_rssi_cal_gi_2g =
		(bool)PHY_GETINTVAR_DEFAULT(pi, phy_var_name, NUM_RSSI_CAL_GI_2G);

	(void)snprintf(phy_var_name, sizeof(phy_var_name), "%s",
	               rstr_num_rssi_cal_gi_5g);
	pi_ac->sromi->num_rssi_cal_gi_5g =
		(bool)PHY_GETINTVAR_DEFAULT(pi, phy_var_name, NUM_RSSI_CAL_GI_5G);
	(void)snprintf(phy_var_name, sizeof(phy_var_name), "%s",
	               rstr_rssi_lna1_routadj_en_5g);
	pi_ac->sromi->rssi_lna1_routadj_en_5g =
		(bool)PHY_GETINTVAR_DEFAULT(pi, phy_var_name, 0);

	FOREACH_CORE(pi, core) {
		ant = phy_get_rsdbbrd_corenum(pi, core);

		for (subband_idx = 0; subband_idx < CH_2G_GROUP_NEW; subband_idx++) {
			(void)snprintf(phy_var_name, sizeof(phy_var_name),
			               rstr_rssi_delta_2gS, subband_key_2g[subband_idx]);

			if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
			  for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
			    for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS_EXT; i++) {
			      k = ant * ACPHY_NUM_BW_2G * ACPHY_GAIN_DELTA_2G_PARAMS_EXT;
			      pi_ac->sromi->rssioffset.rssi_corr_gain_delta_2g_sub[ant]
			      [i][j][subband_idx] =
			        (int8)PHY_GETINTVAR_ARRAY(pi, phy_var_name, (i+4*j+k));
			    }
			  }
			} else {
			  for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
			    for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS_EXT; i++) {
			      pi_ac->sromi->rssioffset.rssi_corr_gain_delta_2g_sub[ant]
				  [i][j][subband_idx] = 0;
			    }
			  }
			}
		}
		for (subband_idx = 0; subband_idx < CH_2G_GROUP_NEW; subband_idx++) {
		  for (j = 0; j < ACPHY_NUM_BW_2G; j++) {
		    for (i = 0; i < ACPHY_GAIN_DELTA_2G_PARAMS_EXT; i++) {
		      PHY_INFORM(("%d ", pi_ac->sromi->rssioffset.rssi_corr_gain_delta_2g_sub
				  [ant][i][j][subband_idx]));
		    }
		  }
		}
		if (PHY_BAND5G_ENAB(pi)) {
			flag = 1;
			for (subband_idx = 0; subband_idx < CH_5G_4BAND; subband_idx++) {
				(void)snprintf(phy_var_name, sizeof(phy_var_name),
				               rstr_rssi_delta_5gS, subband_key[subband_idx]);

				if ((PHY_GETVAR(pi, phy_var_name)) != NULL) {
				  for (j = 0; j < ACPHY_NUM_BW; j++) {
				    for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS_EXT; i++) {
				      k = ant * ACPHY_NUM_BW * ACPHY_GAIN_DELTA_5G_PARAMS_EXT;
				      pi_ac->sromi->rssioffset.rssi_corr_gain_delta_5g_sub
					[ant][i][j][subband_idx] = (int8)
					PHY_GETINTVAR_ARRAY(pi, phy_var_name, (i+4*j+k));
				    /* Check if all the 40/80MHz rssi delta coefficients are zero */
				    if ((j > 0) && (flag == 1)) {
						if (pi_ac->sromi->rssioffset.
							rssi_corr_gain_delta_5g_sub
					    [core][i][j][subband_idx] != 0)
						flag = 0;
				      }
				    }
				  }
				} else {
				  for (j = 0; j < ACPHY_NUM_BW; j++) {
				    for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS_EXT; i++) {
				      pi_ac->sromi->rssioffset.rssi_corr_gain_delta_5g_sub
					[ant][i][j][subband_idx] = 0;
				    }
				  }
				}
			}
			/* If all the 40/80MHz rssi delta coefficients are zero */
			/* Copy 20MHz rssi delta coefficients for 40/80MHz coefficients */
			for (subband_idx = 0; subband_idx < CH_5G_4BAND; subband_idx++) {
				if (flag == 1) {
					for (j = 1; j < ACPHY_NUM_BW; j++) {
					  for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS_EXT; i++) {
						  pi_ac->sromi->rssioffset.
							rssi_corr_gain_delta_5g_sub
							  [core][i][j][subband_idx] =
							  pi_ac->sromi->rssioffset.
							  rssi_corr_gain_delta_5g_sub
							  [core][i][0][subband_idx];
					  }
					}
				}
			}
			for (subband_idx = 0; subband_idx < CH_5G_4BAND; subband_idx++) {
			  for (j = 0; j < ACPHY_NUM_BW; j++) {
			    for (i = 0; i < ACPHY_GAIN_DELTA_5G_PARAMS_EXT; i++) {
			      PHY_INFORM(("%d ", pi_ac->sromi->rssioffset.
					rssi_corr_gain_delta_5g_sub
					  [ant][i][j][subband_idx]));
			    }
			  }
			  PHY_INFORM(("\n"));
			}
		}
	}

	(void)snprintf(phy_var_name, sizeof(phy_var_name), "%s",
	               rstr_rssi_cal_freq_grp_2g);
	j = 0;
	for (i = 0; i < 7; i++) {
		k = (uint8)
		        PHY_GETINTVAR_ARRAY(pi, phy_var_name, i);
		pi_ac->sromi->rssi_cal_freq_grp[j] = (k >> 4) & 0xf;
		j++;
		pi_ac->sromi->rssi_cal_freq_grp[j] = k & 0xf;
		j++;
	}

}

static bool
phy_ac_wd_report_rssi(phy_wd_ctx_t *ctx)
{
	phy_ac_rssi_info_t *ac_info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = ac_info->pi;

	/* Change cores for RSSI reporting every sec to get an idea of all cores */
	if ((pi->sh->interference_mode & ACPHY_ACI_W2NB_PKTGAINLMT) != 0 ||
	        !(ACMAJORREV_32(pi->pubpi->phy_rev) ||
	          ACMAJORREV_33(pi->pubpi->phy_rev) ||
	          ACMAJORREV_37(pi->pubpi->phy_rev) ||
	          (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev)))) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		MOD_PHYREG(pi, RssiStatusControl, coreSel, ac_info->rssi_coresel);
		ac_info->rssi_coresel = (ac_info->rssi_coresel + 1) %
			PHYCORENUM(pi->pubpi->phy_corenum);
		wlapi_enable_mac(pi->sh->physhim);
	}

	if (ACMAJORREV_47(pi->pubpi->phy_rev) && ACMINORREV_GE(pi, 2)) {
		phy_ac_ul_pwr_control(ctx, 1);
		phy_ac_align_sniffer(ctx);
	}

	return TRUE;
}

static int8
phy_ac_rssi_get_rssi(phy_type_rssi_ctx_t *ctx, uint8 core)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	ASSERT(core <= PHY_CORE_MAX);
	return info->rssi[core];
}

/* calculate rssi */
static void BCMFASTPATH
phy_ac_rssi_compute(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;
	phy_ac_info_t *aci = info->aci;
	d11rxhdr_t *rxh = &wrxh->rxhdr;
	int16 rxpwr;
	int16 rxpwr_core[PHY_CORE_MAX];
	int8 int8_rxpwr_core[PHY_CORE_MAX];
	int16 is_status_hacked;
	int core;
	bool db_qdb = FALSE;
	int16 rxpwr_qdBm;

	BCM_REFERENCE(pi);
	/* mode = 0: rxpwr = max(rxpwr0, rxpwr1)
	 * mode = 1: rxpwr = min(rxpwr0, rxpwr1)
	 * mode = 2: rxpwr = (rxpwr0+rxpwr1)/2
	 */
	bzero(int8_rxpwr_core, sizeof(int8)*PHY_CORE_MAX);
	/* For PHY-MAC interface 3.10 and above RXPWR locations changed.
	 * TCL checkin: 656131
	 */
	int8_rxpwr_core[0] = (int8)PHY_RXPWR_ANT0(pi->sh->corerev, rxh);
	if (PHYCORENUM(pi->pubpi->phy_corenum) > 1)
		int8_rxpwr_core[1] = (int8)PHY_RXPWR_ANT1(pi->sh->corerev, rxh);
	if (PHYCORENUM(pi->pubpi->phy_corenum) > 2)
		int8_rxpwr_core[2] = (int8)PHY_RXPWR_ANT2(pi->sh->corerev, rxh);
	if (PHYCORENUM(pi->pubpi->phy_corenum) > 3)
		int8_rxpwr_core[3] = (int8)PHY_RXPWR_ANT3(pi->sh->corerev, rxh);
	is_status_hacked = ACPHY_HACK_PWR_STATUS(rxh);

	FOREACH_CORE(pi, core) {
		/* If the returned value is equal to -128dBm
		  make it as INVALID
		*/
		if (int8_rxpwr_core[core] == PHYHW_MEAS_RSSI_FOR_INACTIVE) {
			int8_rxpwr_core[core] = WLC_RSSI_INVALID;
		}

		rxpwr_core[core] = (int16)int8_rxpwr_core[core];
	}

	/* If the GRANTBT is set to 1 for that particular core, set the value as invalid */
	if ((ltoh16(PHY_RXSTATUS1(pi->sh->corerev, rxh)) & RXS_GRANTBT)) {
		/* Setting shared core RSSI as invalid if Bt is active */
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_FEM_BT) {
			/* setting the core RSSI as invalid only in shared antenna case */
			rxpwr_core[wlc_phy_sharedant_acphy(pi)] = WLC_RSSI_INVALID;
		}
	}

	if ((ACMAJORREV_1(pi->pubpi->phy_rev)) && (is_status_hacked == 1)) {
		FOREACH_CORE(pi, core) {
			rxpwr_core[core] = WLC_RSSI_INVALID;
		}
		rxpwr_core[0] = phy_ac_rssi_11b_WAR(aci, rxh);
	}

	/* Applying dB_qdb to REV36 and later */
	if (ACMAJORREV_GE36(pi->pubpi->phy_rev)) {
		db_qdb = TRUE;
	}

	if (db_qdb) {
		FOREACH_CORE(pi, core) {
			rxpwr_core[core] = (4*rxpwr_core[core]);}
	}

	/* Sign extend */
	FOREACH_CORE(pi, core) {
	  if (rxpwr_core[core] > 127)
	    rxpwr_core[core] -= 256;
	}
	rxpwr = phy_ac_rssi_compute_compensation(ctx, rxpwr_core, db_qdb);

	if (!db_qdb) {
		rxpwr_qdBm = 0;
	} else {
		rxpwr_qdBm = rxpwr;
		rxpwr = rxpwr >> 2;
	}

	/* only 3 antennas are valid for now */
	FOREACH_CORE(pi, core) {
		if (rxpwr_core[core] != WLC_RSSI_INVALID) {
			/* Cap Max/Min RSSI to 0/-128 */
			rxpwr_core[core] = MAX(-128, rxpwr_core[core]);
			rxpwr_core[core] = MIN(0, rxpwr_core[core]);
			wrxh->rxpwr[core] = (int8)rxpwr_core[core];
			info->rssi[core] = (int8)rxpwr_core[core];
		}
	}

	rxpwr = MIN(MAX(-128, rxpwr), 0);
	wrxh->rssi = (int8)rxpwr;
	wrxh->rssi_qdb = rxpwr_qdBm & 3;

	PHY_TRACE(("%s: rssi %d\n", __FUNCTION__, (int8)rxpwr));
	/* Report last_rssi in qdB resolution for pkteng_stats
	 * last_rssi only used for pkteng_stats reporting
	 */
	info->last_rssi = (rxpwr << 2) + (rxpwr_qdBm & 3);

}

/* extract ulofdma per user status bytes */
static void BCMFASTPATH
phy_ulofdma_per_user_rxstats(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;
	phy_ulofdma_per_user_rxstats_t *per_user_stat = &info->data->per_user_stats;
	d11rxhdr_t *rxh = &wrxh->rxhdr;
	uint8 num_users, mu_fstr_error, mu_error_code;
	uint8 user_mac_idx[MAX_NUM_ULOFDMA_USERS];
	uint16 user_rssi[MAX_NUM_ULOFDMA_USERS];
	uint8 user_snr[MAX_NUM_ULOFDMA_USERS];
	int16 user_freq_error[MAX_NUM_ULOFDMA_USERS];
	uint8 user, he_format, frame_type, core;
	int16 gain_err_temp_adj_for_rssi, temp_adj, gain_adj, tmp, temp_gain_adj;
	bool temp_comp, gain_comp;

	BCM_REFERENCE(pi);

	frame_type = (uint8)D11PPDU_FT(rxh, pi->sh->corerev);
	he_format = (uint8)D11PPDU_HEF(rxh, pi->sh->corerev);

	/* Gain error and temp compensation
	   43684 reports max rssi over cores, decided to comp avg gain error over cores
	*/
	gain_comp = !(per_user_stat->ulofdma_rssi_cal_disable & 0x1);
	temp_comp = !((per_user_stat->ulofdma_rssi_cal_disable & 0x2) >> 1);

	wlc_phy_upd_gain_wrt_temp_phy(pi, &gain_err_temp_adj_for_rssi);
	temp_adj = (temp_comp == TRUE) ? gain_err_temp_adj_for_rssi : 0;

	tmp = 0;
	FOREACH_CORE(pi, core) {
		tmp += info->gain_err[core];
	}
	gain_adj = (gain_comp == TRUE) ? (tmp >> 1) : 0;

	if (D11REV_GE(pi->sh->corerev, 128) && (frame_type == 4) && (he_format == 3)) {
		bzero(user_rssi, sizeof(int16)*MAX_NUM_ULOFDMA_USERS);
		bzero(user_snr, sizeof(int8)*MAX_NUM_ULOFDMA_USERS);
		bzero(user_freq_error, sizeof(int16)*MAX_NUM_ULOFDMA_USERS);
		bzero(user_mac_idx, sizeof(uint8)*MAX_NUM_ULOFDMA_USERS);

		/* Common fields */
		num_users = MIN((uint8)PRXS_NUM_ULOFDMA_USERS_REV_GE129(rxh),
			MAX_NUM_ULOFDMA_USERS);
		mu_fstr_error = (uint8)PRXS_MU_FSTR_ERROR_REV_GE129(rxh);
		mu_error_code = (uint8)PRXS_MU_ERROR_CODE_REV_GE129(rxh);

		/* Per user fields */
		for (user = 0; user < num_users; user++) {
			user_mac_idx[user] = (uint8)PRXS_USER_MAC_IDX_REV_GE129(rxh, user);
			user_rssi[user] = (uint16)PRXS_USER_RSSI_REV_GE129(rxh, user);
			user_snr[user] = (uint8)PRXS_USER_SNR_REV_GE129(rxh, user);
			user_freq_error[user] = (int16)PRXS_USER_FREQ_ERROR_REV_GE129(rxh, user);
		}

		per_user_stat->num_users = num_users;
		per_user_stat->mu_fstr_error = mu_fstr_error;
		per_user_stat->mu_error_code = mu_error_code;
		for (user = 0; user < MAX_NUM_ULOFDMA_USERS; user++) {
			per_user_stat->user_mac_idx[user] = user_mac_idx[user];
			temp_gain_adj = (user_rssi[user] != 0) ? (temp_adj - gain_adj) : 0;
			per_user_stat->user_rssi[user] = ((user_rssi[user] > 511) ?
				((int16)user_rssi[user] - 1024) : (int16)user_rssi[user])
				+ temp_gain_adj;
			per_user_stat->user_snr[user] = user_snr[user];
			per_user_stat->user_freq_error[user] = user_freq_error[user];
		}

		if (ACMAJORREV_47(pi->pubpi->phy_rev) && ACMINORREV_GE(pi, 2))
			phy_ac_ul_pwr_control(ctx, 0);
	}
}

/* calculate tracked frequency offset */
static void BCMFASTPATH
phy_tracked_freq_offset(phy_type_rssi_ctx_t *ctx, wlc_d11rxhdr_t *wrxh)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;
	phy_tracked_freq_offset_t *tracked_freq_stat = &info->data->tracked_freq_stats;
	d11rxhdr_t *rxh = &wrxh->rxhdr;
	uint16 low_bit;
	uint16 high_bit;
	uint32 total;
	bool   pos_freq;

	BCM_REFERENCE(pi);

	if (D11REV_GE(pi->sh->corerev, 128)) {
		low_bit = (uint16)PRXS_TRACKED_FREQ_L_REV_GE129(rxh);
	        high_bit = (uint16)PRXS_TRACKED_FREQ_H_REV_GE129(rxh);
		total = (uint32)((high_bit << 6) + low_bit);
		/* take away the sign bit so that we have one more bit for scaling factor */
		pos_freq = TRUE;
		if (total > (1 << 19)) {
			total = (1 << 20) - total;
			pos_freq = FALSE;
		}

		tracked_freq_stat->freq_offset_l = low_bit;
		tracked_freq_stat->freq_offset_h = high_bit;
		tracked_freq_stat->freq_offset_total = total;
		tracked_freq_stat->pos_freq = pos_freq;

		/* Scaling factor selection
		   freq_offset_total is 19 bits maximum (unsigned).
		   The scaling factor should be smaller than 8192 (13 bits)
		   freq_offset_total to Hz (add 2^13 scaling for integer operation):
		       4883 ~= 1/(3.2e-6*2^19)*2^13
		   freq_offset_total to pll_wild_base_offset
		   A band: 7716 ~= 1/(3.2e-6*2^19)/1.5/108*2^21
		   G band: 4340 ~= 1/(3.2e-6*2^19)/(4/3)/108*2^20 (overflow with 2^21)
		*/

		tracked_freq_stat->tracked_freq_offsetHz = total * 4883;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tracked_freq_stat->pll_wild_base_offset = (uint32)(total * 4340) / 500000;
		} else {
			tracked_freq_stat->pll_wild_base_offset = (uint32)(total * 7716) / 1000000;
		}
	}
}

static int phy_ac_calc_ru_size(int ru_index)
{
	int ru_size;

	if (ru_index < 37)
		ru_size = 0;
	else if (ru_index < 53)
		ru_size = 1;
	else if (ru_index < 61)
		ru_size = 2;
	else if (ru_index < 65)
		ru_size = 3;
	else
		ru_size = 4;

	return ru_size;
}

static void phy_ac_ul_pwr_control(phy_type_rssi_ctx_t *ctx, bool trig_by_wd)
{
	/*
	 * 1. measured_rssi: Per user RSSI in 1/4dB resoluiton.
	 *    Signed 10 bits number covering range -128dBm to 127dBm in 0.25dB steps.
	 * 2. target_rssi_field in trigger frame: values 0 to 90 map to -110 dBm to -20 dBm
	*/
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;
	phy_ulofdma_per_user_rxstats_t *per_user_stat = &info->data->per_user_stats;
	int m, psd, ru_size, psd_maxmcs, N_user, ru_index, signed_rssi, mcs_thres_offset;
	int trig_delta, ofdma_delta, rssi_maxmcs;
	uint16 user_info_0, user_info_1, user_info_2, target_rssi, user_mac_idx;
	uint32 time_passed, cur_time;
	uint8 next_mcs, nss, ldpc;
	//MCS threshold(single stream) for 26 tone RU in dBm.
	int mcs_thres[12] = {-98, -94, -92, -88, -86, -82, -80, -78, -74, -72, -66, -64};

	//RSSI for 11x1 of 26 tone RU in dBm
	psd_maxmcs = -61;

	/*
	 * ul_pwr_ctrl = 0: both ul power control and ul rate recommendation are off
	 * ul_pwr_ctrl = 1: ul power control is on, ul rate recommendation is off
	 * ul_pwr_ctrl = 2: both ul power control and ul rate recommendation are on
	 */
	if (per_user_stat->ul_pwr_ctrl == 0)
		return;

	if (trig_by_wd == 1) {
		//In phywatchdog period, if number of received ofdma frame is much less than number
		//of transmitted trigger frame, we reset mcs and target RSSI to default value.
		trig_delta =  wlapi_bmac_read_shm(pi->sh->physhim, M_TXTRIG_CNT(pi))
			- per_user_stat->txtrig_cnt;
		ofdma_delta = per_user_stat->rxofdma_cnt - per_user_stat->rxofdma_cnt_last;

		per_user_stat->txtrig_cnt = wlapi_bmac_read_shm(pi->sh->physhim, M_TXTRIG_CNT(pi));
		per_user_stat->rxofdma_cnt_last = per_user_stat->rxofdma_cnt;
		//PHY_ERROR(("\n\nphy_ac_ul_pwr_control ===============================\n"));
		//PHY_ERROR(("trig_delta is %d, ofdam_delta is %d \n", trig_delta, ofdma_delta));

		if ((trig_delta > 0) && (ofdma_delta >= 0) && (trig_delta > 32*ofdma_delta)) {
			N_user = wlapi_bmac_read_shm(pi->sh->physhim, M_TXTRIG_SRXCTL(pi));
			N_user = N_user & 0xff;
			for (m = 0; m < N_user; m++) {
				user_mac_idx = per_user_stat->user_mac_idx[m];
				user_info_0 = wlapi_bmac_read_shm(pi->sh->physhim,
					(M_TXTRIG_CMNINFO(pi) + 8 + 6 * user_mac_idx));
				user_info_1 = wlapi_bmac_read_shm(pi->sh->physhim,
					(M_TXTRIG_CMNINFO(pi) + 10 + 6 * user_mac_idx));
				ru_index = ((user_info_0 >> 13) & 0x7) + ((user_info_1 & 0xf) << 3);
				ru_size = phy_ac_calc_ru_size(ru_index);
				wlapi_bmac_write_shm(pi->sh->physhim, (M_TXTRIG_CMNINFO(pi) + 12 +
					6 * user_mac_idx), psd_maxmcs + 3 * ru_size + 110);
				if (per_user_stat->ul_pwr_ctrl == 2) {
					//set MCS to be 0
					user_info_1 = user_info_1 & ~(0xf << 5);
					wlapi_bmac_write_shm(pi->sh->physhim,
					(M_TXTRIG_CMNINFO(pi) + 10 + 6*user_mac_idx), user_info_1);
				}
			}
		}
		return;
	}

	per_user_stat->rxofdma_cnt++;
	cur_time = R_REG(GENERIC_PHY_INFO(pi)->osh, D11_TSFTimerLow(pi));
	per_user_stat->timestamp = (per_user_stat->timestamp < cur_time)
		? per_user_stat->timestamp : cur_time;
	time_passed = (uint32)(cur_time - per_user_stat->timestamp);

	//After setting target RSSI, we wait at least 50ms to measure MU RSSI.
	if (time_passed < 50000) {
		return;
	}

	//PHY_ERROR(("\n\nphy_ac_ul_pwr_control =======================\n"));
	N_user = wlapi_bmac_read_shm(pi->sh->physhim, M_TXTRIG_SRXCTL(pi));
	N_user = N_user & 0xff;
	for (m = 0; m < N_user; m++) {
		user_mac_idx = per_user_stat->user_mac_idx[m];
		user_info_0 = wlapi_bmac_read_shm(pi->sh->physhim,
			(M_TXTRIG_CMNINFO(pi) + 8 + 6 * user_mac_idx));
		user_info_1 = wlapi_bmac_read_shm(pi->sh->physhim,
			(M_TXTRIG_CMNINFO(pi) + 10 + 6 * user_mac_idx));
		user_info_2 = wlapi_bmac_read_shm(pi->sh->physhim,
			(M_TXTRIG_CMNINFO(pi) + 12 + 6 * user_mac_idx));
		//PHY_ERROR((" user index is  %d ...\n", user_mac_idx));

		signed_rssi = (per_user_stat->user_rssi[m] & 0x1ff)
				- (per_user_stat->user_rssi[m] & 0x200);
		if ((signed_rssi == 0) || (signed_rssi < -360))
			continue;

		ru_index = ((user_info_0 >> 13) & 0x7) + ((user_info_1 & 0xf) << 3);
		ru_size = phy_ac_calc_ru_size(ru_index);
		nss = (user_info_1 >> 13) & 0x7;
		ldpc = (user_info_1 >> 4) & 1;
		//increase mcs_thres by 3dB with LDPC off;
		mcs_thres_offset = ldpc ? 0 : 3;

		//nss = num_stream - 1;
		if (nss == 1)
			mcs_thres_offset += 3;
		if (nss == 2)
			mcs_thres_offset += 5;
		if (nss == 3)
			mcs_thres_offset += 6;

		//signed_rssi is in qdB unit, psd is in dB unit
		psd = signed_rssi/4 - 3*ru_size;
		rssi_maxmcs = psd_maxmcs + mcs_thres_offset + 3*ru_size;
		target_rssi = user_info_2 & 0x7f;
		target_rssi += rssi_maxmcs - signed_rssi/4;
		target_rssi = (target_rssi > (rssi_maxmcs+120)) ? (rssi_maxmcs+120) : target_rssi;
		target_rssi = (target_rssi < (rssi_maxmcs+100)) ? (rssi_maxmcs+100) : target_rssi;

		wlapi_bmac_write_shm(pi->sh->physhim,
			(M_TXTRIG_CMNINFO(pi) + 12 + 6 * user_mac_idx), target_rssi);
		//PHY_ERROR(("ru_index is %d, user_rssi is %d, new target_rssi is %d\n",
		//	ru_index, signed_rssi/4, target_rssi-110));

		if (per_user_stat->ul_pwr_ctrl == 2) {
			for (next_mcs = 11; next_mcs > 0; next_mcs--) {
				if (psd > (mcs_thres[next_mcs] + mcs_thres_offset)) {
					break;
				}
			}
			user_info_1 = user_info_1 & ~(0xf << 5);
			user_info_1 |= (next_mcs << 5);
			wlapi_bmac_write_shm(pi->sh->physhim, (M_TXTRIG_CMNINFO(pi)
					+ 10 + 6 * user_mac_idx), user_info_1);
			//PHY_ERROR(("ldpc is %d, nss is %d, next MCS is %d \n",
			//		ldpc, nss, next_mcs));
		}
	}
	per_user_stat->timestamp = cur_time;
}

static void phy_ac_align_sniffer(phy_type_rssi_ctx_t *ctx)
{
	phy_ac_rssi_info_t *ac_info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = ac_info->pi;
	uint32 wild_base_high;
	uint32 wild_base_low;
	uint32 wild_base;

	if (pi->u.pi_acphy->sniffer_aligner.enabled &&
		!pi->u.pi_acphy->sniffer_aligner.active) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

		wild_base_high = READ_RADIO_PLLREGFLD_20698(pi, PLL_FRCT2,
				0, rfpll_frct_wild_base_high);
		wild_base_low  = READ_RADIO_PLLREGFLD_20698(pi, PLL_FRCT3,
				0, rfpll_frct_wild_base_low);
		wild_base = (wild_base_high << 16) + wild_base_low;
		printf("\nBefore adj wild_base = %d\n", wild_base);
		printf("\nwild_base_offset = %d\n",
				pi->u.pi_acphy->sniffer_aligner.wild_base_offset);
		wild_base = wild_base + pi->u.pi_acphy->sniffer_aligner.wild_base_offset;
		printf("\nAfter adj wild_base = %d\n", wild_base);
		wild_base_high = (wild_base >> 16);
		wild_base_low  = (wild_base & 0xFFFF);
		MOD_RADIO_PLLREG_20698(pi, PLL_FRCT2, 0,
				rfpll_frct_wild_base_high, (uint16) wild_base_high);
		MOD_RADIO_PLLREG_20698(pi, PLL_FRCT3, 0,
				rfpll_frct_wild_base_low,  (uint16) wild_base_low);
		pi->u.pi_acphy->sniffer_aligner.active = TRUE;

		wlapi_enable_mac(pi->sh->physhim);
	}
}

int16
phy_ac_rssi_compute_compensation(phy_type_rssi_ctx_t *ctx, int16 *rxpwr_core, bool db_qdb)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;
	phy_ac_info_t *aci = info->aci;
	uint8 core, ant, core_freq_segment_map;
	int8  bw_idx, subband_idx;
	int16 rxpwr_core_qdBm[PHY_CORE_MAX] = {0};
	int16 rxpwr, rxpwr_qdBm = 0;
		int16 gain_err_temp_adj_for_rssi;

	/*
	 * 22nd Oct
	 * if (rxgaincal_rssical == false)
	 * 	include the current implementation
	 * else (rxgaincal_rssical == true)
	 * 	have only rssi_gain_cal
	 * 	in true condition
	 * 	if rssi_cal_rev == 1
	 *		use gain_cal_temp
	 * 	else
	 * 		use raw_tempsense (as it is currently
	 *              being populated for rssigain delta calibration)
	 * 	fi
	 * fi
	 * Eventually, rssi_cal_rev == 0 condition has to be deprecated.
	 */
	if (info->data->rxgaincal_rssical == FALSE) {
		wlc_phy_upd_gain_wrt_temp_phy(pi, &gain_err_temp_adj_for_rssi);

		/* Apply gain-error correction with temperature compensation: */
		FOREACH_CORE(pi, core) {
			if (rxpwr_core[core] != WLC_RSSI_INVALID) {
				int16 tmp;

				tmp = info->gain_err[core] * 2 -
				    gain_err_temp_adj_for_rssi;
				if (!db_qdb) {
					tmp = ((tmp >= 0) ? ((tmp + 2) >> 2) : -1 *
						((-1 * tmp + 2) >> 2));
					rxpwr_core[core] -= tmp;
					rxpwr_core_qdBm[core] = 0;
				} else {
					rxpwr_core_qdBm[core] = rxpwr_core[core] - tmp;
					tmp = rxpwr_core_qdBm[core];
					rxpwr_core[core] = (tmp > 0) ? ((tmp + 2) >> 2) :
					(-1*((-1*tmp + 2)>> 2));
				}

				if (CHSPEC_IS2G(pi->radio_chanspec)) {
					bw_idx = (CHSPEC_IS40(pi->radio_chanspec)) ? 1 : 0;
				} else {
					bw_idx = (CHSPEC_IS80(pi->radio_chanspec) ||
						CHSPEC_IS160(pi->radio_chanspec)) ? 2 :
						(CHSPEC_IS40(pi->radio_chanspec)) ? 1 : 0;
				}

				/* Apply nvram based offset: */
				ant = phy_get_rsdbbrd_corenum(pi, core);
				if (CHSPEC_IS2G(pi->radio_chanspec)) {
					rxpwr_core[core] +=
					aci->sromi->rssioffset.rssi_corr_normal[ant][bw_idx];
					rxpwr_core[core] +=
					aci->sromi->rssioffset.rssi_corr_gain_delta_2g
					[core][0][bw_idx];

					rxpwr_core_qdBm[core] +=
					4*(aci->sromi->rssioffset.rssi_corr_normal[ant][bw_idx]);
					rxpwr_core_qdBm[core] +=
					4*(aci->sromi->rssioffset.rssi_corr_gain_delta_2g
					[core][0][bw_idx]);
				} else {
					uint8 bands[NUM_CHANS_IN_CHAN_BONDING];
					if (PHY_AS_80P80(pi, pi->radio_chanspec)) {
						phy_ac_chanmgr_get_chan_freq_range_80p80(pi, 0,
							bands);
						if (ACMAJORREV_33(pi->pubpi->phy_rev)) {
							subband_idx = (core <= 1) ? (bands[0] - 1)
								: (bands[1] - 1);
						} else {
							subband_idx = bands[0] - 1;
							ASSERT(0); // FIXME
						}
					} else {
					/* core_freq_segment_map is only required for 80P80 mode.
					For other modes, it is ignored
					*/
						core_freq_segment_map = phy_ac_chanmgr_get_data
						(pi->u.pi_acphy->chanmgri)->core_freq_mapping[core];
						subband_idx = phy_ac_chanmgr_get_chan_freq_range(pi,
							pi->radio_chanspec,
							core_freq_segment_map)-1;
					}

					if (!ACMAJORREV_5(pi->pubpi->phy_rev)) {
						rxpwr_core[core] +=
						  aci->sromi->rssioffset.rssi_corr_normal_5g
						  [ant][subband_idx][bw_idx];
						rxpwr_core_qdBm[core] +=
						  4*(aci->sromi->rssioffset.rssi_corr_normal_5g
						  [ant][subband_idx][bw_idx]);
					}
					rxpwr_core[core] +=
						aci->sromi->rssioffset.rssi_corr_gain_delta_5g
						[core][0][bw_idx][subband_idx];
					rxpwr_core_qdBm[core] +=
						4*(aci->sromi->rssioffset.rssi_corr_gain_delta_5g
						[core][0][bw_idx][subband_idx]);
				}
			}
		}
	} else {
		uint8 bands[NUM_CHANS_IN_CHAN_BONDING];

		if (info->data->rssi_cal_rev == FALSE) {
			wlc_phy_upd_gain_wrt_temp_phy(pi, &gain_err_temp_adj_for_rssi);
		} else {
			wlc_phy_upd_gain_wrt_gain_cal_temp_phy(pi, &gain_err_temp_adj_for_rssi);
		}

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			bw_idx = (CHSPEC_IS40(pi->radio_chanspec)) ? 1 : 0;
		} else {
			bw_idx = (CHSPEC_IS80(pi->radio_chanspec) ||
					PHY_AS_80P80(pi, pi->radio_chanspec)) ? 2 :
					CHSPEC_IS160(pi->radio_chanspec) ? 3 :
			        (CHSPEC_IS40(pi->radio_chanspec)) ? 1 : 0;
		}

		/* Apply nvram based offset: */
		FOREACH_CORE(pi, core) {
			if (rxpwr_core[core] != WLC_RSSI_INVALID) {
				int16 tmp;
				/* core_freq_segment_map is only required for 80P80 mode.
				For other modes, it is ignored
				*/
				core_freq_segment_map = phy_ac_chanmgr_get_data
					(pi->u.pi_acphy->chanmgri)->core_freq_mapping[core];
				if (info->data->rssi_cal_rev == FALSE) {
					if (PHY_AS_80P80(pi, pi->radio_chanspec)) {
						phy_ac_chanmgr_get_chan_freq_range_80p80(pi, 0,
							bands);
						if (ACMAJORREV_33(pi->pubpi->phy_rev)) {
							subband_idx = (core <= 1) ? (bands[0] - 1)
								: (bands[1] - 1);
						} else {
							subband_idx = bands[0] - 1;
							ASSERT(0); // FIXME
						}
					} else {
						subband_idx = phy_ac_chanmgr_get_chan_freq_range(pi,
							pi->radio_chanspec,
							core_freq_segment_map)-1;
					}
				} else {
					subband_idx = wlc_phy_rssi_get_chan_freq_range_acphy(pi,
						core_freq_segment_map, core);
				}
				ant = phy_get_rsdbbrd_corenum(pi, core);
				if (CHSPEC_IS2G(pi->radio_chanspec)) {
					int8 rssi_corr_gain_delta_2g;
					if (info->data->rssi_cal_rev == FALSE) {
						rssi_corr_gain_delta_2g = aci->sromi->rssioffset
						.rssi_corr_gain_delta_2g[core][0][bw_idx];
					} else {
						rssi_corr_gain_delta_2g = aci->sromi->rssioffset
						.rssi_corr_gain_delta_2g_sub
						[core][0][bw_idx][subband_idx];
					}
					rxpwr_core[core] +=
					aci->sromi->rssioffset.rssi_corr_normal
					[ant][bw_idx];
					rxpwr_core[core] += rssi_corr_gain_delta_2g -
					gain_err_temp_adj_for_rssi;
				} else {
					if (!ACMAJORREV_5(pi->pubpi->phy_rev)) {
						rxpwr_core[core] +=
						aci->sromi->rssioffset.rssi_corr_normal_5g
						[ant][subband_idx][bw_idx];
					}
					rxpwr_core[core] +=
					aci->sromi->rssioffset.rssi_corr_gain_delta_5g_sub
					[core][0][bw_idx][subband_idx] - gain_err_temp_adj_for_rssi;
				}
				if (!ACMAJORREV_32(pi->pubpi->phy_rev) &&
				    !ACMAJORREV_33(pi->pubpi->phy_rev) &&
				    !ACMAJORREV_37(pi->pubpi->phy_rev) &&
				    !(ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
				    !ACMAJORREV_128(pi->pubpi->phy_rev))) {
					/* By this point, both temp and rssi_corr_gain_delta's are
					 * converted into 0.25 dB steps.
					 * So, convert irrespective of conditions to 1 dB steps.
					 */
					if (db_qdb == 0) {
						tmp = 4*rxpwr_core[core];
					} else {
						/* Here, rxpwr_core is in qdB, but in a few lines it
						 * is converted to dB steps
						 */
						tmp = rxpwr_core[core];
					}
					rxpwr_core_qdBm[core] = tmp;
					tmp = ((tmp >= 0) ? ((tmp + 2) >> 2) : -1 *
					       ((-1 * tmp + 2) >> 2));
					rxpwr_core[core] = tmp;
				}
			}
		}
	}
	/* legacy interface */
	if (PHYCORENUM(pi->pubpi->phy_corenum) == 1) {
		rxpwr = rxpwr_core[0];
		rxpwr_qdBm = rxpwr_core_qdBm[0];
	} else {
		uint8 num_activecores = 0;
		phy_stf_data_t *stf_shdata = phy_stf_get_data(pi->stfi);

		BCM_REFERENCE(stf_shdata);

		rxpwr = 0;
		FOREACH_ACTV_CORE(pi, stf_shdata->phyrxchain, core) {
			if (rxpwr_core[core] != WLC_RSSI_INVALID) {
				if (num_activecores++ == 0) {
					rxpwr = rxpwr_core[core];
					rxpwr_qdBm = rxpwr_core_qdBm[core];
				} else {
					switch (pi->sh->rssi_mode) {
						case RSSI_ANT_MERGE_MAX:
							rxpwr = MAX(rxpwr, rxpwr_core[core]);
							rxpwr_qdBm = MAX(rxpwr_qdBm,
									rxpwr_core_qdBm[core]);
							break;
						case RSSI_ANT_MERGE_MIN:
							rxpwr = MIN(rxpwr, rxpwr_core[core]);
							rxpwr_qdBm = MIN(rxpwr_qdBm,
									rxpwr_core_qdBm[core]);
							break;
						case RSSI_ANT_MERGE_AVG:
							rxpwr += rxpwr_core[core];
							rxpwr_qdBm += rxpwr_core_qdBm[core];
							break;
						default:
							ASSERT(0);
					}
				}
			}
		}

		if (pi->sh->rssi_mode == RSSI_ANT_MERGE_AVG) {
			int16 qrxpwr;

			ASSERT(num_activecores > 0);

			rxpwr = (int8)qm_div16(rxpwr, num_activecores, &qrxpwr);
			rxpwr_qdBm = rxpwr_qdBm/num_activecores;
		}
	}
	if (db_qdb == 0) {
	    return rxpwr;
	} else {
	    return rxpwr_qdBm;
	}

}

/* done with papd cal */

uint8
wlc_phy_rssi_get_chan_freq_range_acphy(phy_info_t *pi, uint8 core_segment_mapping, uint8 core)
{
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	uint8 chans[NUM_CHANS_IN_CHAN_BONDING];

	if (ACMAJORREV_33(pi->pubpi->phy_rev)) {
		if (PHY_AS_80P80(pi, pi->radio_chanspec)) {
			wf_chspec_get_80p80_channels(pi->radio_chanspec, chans);
			channel = (core <= 1) ? chans[0] : chans[1];
			PHY_INFORM(("wl%d: 80P80 channel %s\n", pi->sh->unit, __FUNCTION__));
		}
	} else {
		if (phy_get_phymode(pi) == PHYMODE_80P80) {

			if (PRIMARY_FREQ_SEGMENT == core_segment_mapping)
				channel = wf_chspec_primary80_channel(pi->radio_chanspec);

			if (SECONDARY_FREQ_SEGMENT == core_segment_mapping)
				channel = wf_chspec_secondary80_channel(pi->radio_chanspec);
		}
	}

	PHY_TRACE(("wl%d: %s | channel = %d \n", pi->sh->unit, __FUNCTION__, channel));

	if (channel <= CH_MAX_2G_CHANNEL) {
		phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

		return pi_ac->sromi->rssi_cal_freq_grp[channel-1] & 0x7;
	} else {
		int freq;

		if (RADIOID_IS(pi->pubpi->radioid, BCM2069_ID)) {
			const void *chan_info;

			freq = wlc_phy_chan2freq_acphy(pi, channel, &chan_info);
		} else if (RADIOID_IS(pi->pubpi->radioid, BCM20693_ID)) {
			const chan_info_radio20693_pll_t *chan_info_20693_pll;
			const chan_info_radio20693_rffe_t *chan_info_20693_rffe;
			const chan_info_radio20693_pll_wave2_t *chan_info_20693_pll_wave2;

			freq = wlc_phy_chan2freq_20693(pi, channel, &chan_info_20693_pll,
				&chan_info_20693_rffe,  &chan_info_20693_pll_wave2);
		} else if (RADIOID_IS(pi->pubpi->radioid, BCM20694_ID)) {
			const chan_info_radio20694_rffe_t *chan_info;
			freq = wlc_phy_chan2freq_20694(pi, channel,
				&chan_info);
		} else if (RADIOID_IS(pi->pubpi->radioid, BCM20698_ID)) {
			const chan_info_radio20698_rffe_t *chan_info;
			freq = wlc_phy_chan2freq_20698(pi, channel,
				&chan_info);
		} else if (RADIOID_IS(pi->pubpi->radioid, BCM20704_ID)) {
			const chan_info_radio20704_rffe_t *chan_info;
			freq = wlc_phy_chan2freq_20704(pi, pi->radio_chanspec,
				&chan_info);
		} else if (RADIOID_IS(pi->pubpi->radioid, BCM20707_ID)) {
			const chan_info_radio20707_rffe_t *chan_info;
			freq = wlc_phy_chan2freq_20707(pi, channel,
				&chan_info);
		} else {
			const chan_info_radio20691_t *chan_info_20691;

			freq = wlc_phy_chan2freq_20691(pi, channel, &chan_info_20691);
		}

		if ((freq >= PHY_RSSI_SUBBAND_4BAND_BAND0) &&
			(freq < PHY_RSSI_SUBBAND_4BAND_BAND1))
			return WL_CHAN_FREQ_RANGE_5G_BAND0 - 1;
		else if ((freq >= PHY_RSSI_SUBBAND_4BAND_BAND1) &&
			(freq < PHY_RSSI_SUBBAND_4BAND_BAND2))
			return WL_CHAN_FREQ_RANGE_5G_BAND1 - 1;
		else if ((freq >= PHY_RSSI_SUBBAND_4BAND_BAND2) &&
			(freq < PHY_RSSI_SUBBAND_4BAND_BAND3))
			return WL_CHAN_FREQ_RANGE_5G_BAND2 - 1;
		else
			return WL_CHAN_FREQ_RANGE_5G_BAND3 - 1;
	}
}

static uint8
phy_ac_rssi_11b_WAR(phy_ac_info_t *aci, d11rxhdr_t *rxh)
{
	int16 PhyStatsGainInfo0, Auxphystats0;
	int8 lna1, lna2, mixer, biq0, biq1, trpos, dvga;
	int8 elna;
	int8 trloss;
	int8 elna_byp_tr;
	int8 rx_gains[5], input_param[5];
	int8 lna1_gain, lna2_gain, rxmix_gain,
	        biq0_gain, biq1_gain, dvga_gain, fem_gain, total_rx_gain;
	phy_info_t *pi = aci->pi;
	/* For PHY-MAC interface 3.10 and above RXPWR locations changed.
	 *  TCL checkin: 656131
	 */
	PhyStatsGainInfo0 = ((PHY_RXPWR_ANT2(pi->sh->corerev, rxh) << 8)
			    | (PHY_RXPWR_ANT1(pi->sh->corerev, rxh)));
	Auxphystats0 = ((PHY_RXPWR_ANT0(pi->sh->corerev, rxh) << 8)
		       | (PHY_RXPWR_ANT3(pi->sh->corerev, rxh)));
	/* Parsing the gaininfo */
	lna1 = (PhyStatsGainInfo0 >> 0) & 0x7;
	lna2 = (PhyStatsGainInfo0 >> 3) & 0x7;
	mixer = (PhyStatsGainInfo0 >> 6) & 0xf;
	biq0 = (PhyStatsGainInfo0 >> 10) & 0x7;
	biq1 = (PhyStatsGainInfo0 >> 13) & 0x7;

	trpos = (Auxphystats0 >> 0) & 0x1;
	dvga = (Auxphystats0 >> 2) & 0xf;

	phy_ac_get_fem_rxgains(pi, rx_gains);
	elna = rx_gains[0];
	trloss = rx_gains[1];
	elna_byp_tr = rx_gains[2];

	/* get gains of each block */
	dvga_gain  = 3*dvga;

	input_param[0] = lna1;
	input_param[1] = lna2;
	input_param[2] = mixer;
	input_param[3] = biq0;
	input_param[4] = biq1;

	phy_ac_get_rxgains_ctrl(pi, rx_gains, input_param);

	lna1_gain  = rx_gains[0];
	lna2_gain  = rx_gains[1];
	rxmix_gain = rx_gains[2];
	biq0_gain = rx_gains[3];
	biq1_gain = rx_gains[4];

	/* Get fem gain */
	if (elna_byp_tr == 1) {
		if (trpos == 0) {
			fem_gain = elna;
		} else {
			fem_gain = elna - trloss;
		}
	} else {
		if (trpos == 0) {
			fem_gain = 0;
		} else {
			fem_gain = (-1*trloss);
		}
	}

	/* Total Rx gain */
	total_rx_gain = (lna1_gain + lna2_gain + rxmix_gain
		 + biq0_gain + biq1_gain + dvga_gain + fem_gain);

	return (2 - total_rx_gain + 256);
}

/* init gain error table. */
static void
_phy_ac_rssi_init_gain_err(phy_type_rssi_ctx_t *ctx)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = info->pi;

	/* XXX
	 * Gain error computed as follows:
	 * 1) Backoff subband dependent init_gain from gaintable,
	 * 2) add back fixed init-gain assumed by rxiqest,
	 * 3) add subband-dependent rxiqest gain error (retrieved from srom)
	 */
	int16 gainerr[PHY_CORE_MAX];
	int16 initgain_dB[PHY_CORE_MAX];
	int16 rxiqest_gain;
	uint8 core;
	bool srom_isempty = FALSE;
	uint8 dummy[ACPHY_MAX_RX_GAIN_STAGES];
	int8 init_gain_to_program = (ACMAJORREV_40_128(pi->pubpi->phy_rev)) ?
			ACPHY_INIT_GAIN_28NM : ACPHY_INIT_GAIN;

	/* Retrieve rxiqest gain error: */
	srom_isempty = wlc_phy_get_rxgainerr_phy(pi, gainerr);
	if (srom_isempty) {
		/* XXX
		 * Do not apply gain error correction
		 * if nothing was written to SROM
		 */
		FOREACH_CORE(pi, core) {
			info->gain_err[core] = 0;
		}
		return;
	}

	/* Retrieve rxiqest gain: */
	/* XXX
	 * Must sync with rxiqest if gain assumed changes there
	 */
	if (BFCTL(pi->u.pi_acphy) == 2) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN_X29_2G);
		} else {
			rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN_X29_5G);
		}
	} else {
		rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN);
	}
	/* Compute correction */
	FOREACH_CORE(pi, core) {
		/* Retrieve initgains in dB */
		initgain_dB[core] = wlc_phy_rxgainctrl_encode_gain_acphy(pi, core,
				init_gain_to_program, FALSE, FALSE, INIT_GAIN, dummy) - 2;

		/* report rssi gainerr in 0.5dB steps */
		info->gain_err[core] =
		        (int8)((rxiqest_gain << 1) - (initgain_dB[core] << 1) + gainerr[core]);
		/* JIRA:SWWLAN-71620 gainerr is calibrated using iqest module, which sees 2dB less
			power than the phy_rxpwr at 2G and 1dB at 5G (BW20/40). Put this adjustment
			for 43602 only now
		*/
		if (ACMAJORREV_5(pi->pubpi->phy_rev) &&
				(info->data->rxgaincal_rssical == FALSE)) {
			pi->phy_rssi_gain_error[core] += CHSPEC_IS2G(pi->radio_chanspec) ? 4 :
			(CHSPEC_IS80(pi->radio_chanspec) ? 0 : 2);
		}

	}
}

void
phy_ac_rssi_init_gain_err(phy_ac_rssi_info_t *info)
{
	_phy_ac_rssi_init_gain_err(info);
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
phy_ac_rssi_dump(phy_type_rssi_ctx_t *ctx, struct bcmstrbuf *b)
{
	phy_ac_rssi_info_t *info = (phy_ac_rssi_info_t *)ctx;
	uint i;

	/* DUMP gain_err... */

	bcm_bprintf(b, "gain err:\n");
	for (i = 0; i < ARRAYSIZE(info->gain_err); i ++)
		bcm_bprintf(b, "  core %u: %u\n", i, info->gain_err[i]);

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */

#if defined(WLTEST)
static void
phy_ac_rssi_update_pkteng_rxstats(phy_type_rssi_ctx_t *ctx, uint8 statidx)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	if (rssii->pi->measure_hold & PHY_HOLD_FOR_PKT_ENG) {
		rssii->rxstats[statidx] += 1;
	}
}

static int
phy_ac_rssi_get_pkteng_stats(phy_type_rssi_ctx_t *ctx, void *a, int alen,
wl_pkteng_stats_t stats, int8 *gain_correct)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = rssii->pi;
	int i;

	if (((rssii->data->rssi_cal_rev == FALSE) &&
		!D11REV_IS(pi->sh->corerev, 49))) {
#ifdef WL11AC
		int8 core;
		int16 temp_rssi = -128;
		int16 gain_err_temp_adj;
		uint8 phyrxchain;

		BCM_REFERENCE(phyrxchain);

		stats.rssi = -128;

		/* Read and (implicitly) store current temperature */
		wlc_phy_tempsense_acphy(pi);
		wlc_phy_upd_gain_wrt_temp_phy(pi, &gain_err_temp_adj);

		phyrxchain = phy_stf_get_data(pi->stfi)->phyrxchain;
		FOREACH_ACTV_CORE(pi, phyrxchain, core) {
			int16 tmp;
			tmp = pi->phy_rssi_gain_error[core] * 2 -
				gain_err_temp_adj;
			tmp = ((tmp >= 0) ? ((tmp + 2) >> 2) : -1 *
				((-1 * tmp + 2) >> 2));

			if (core == 0) {
				temp_rssi = (R_REG(pi->sh->osh,
					D11_RXE_PHYRS_2(pi))
					>> 8) & 0xff;
			} else if (core == 1) {
				temp_rssi = R_REG(pi->sh->osh,
					D11_RXE_PHYRS_3(pi))
					& 0xff;
			} else if (core == 2) {
				temp_rssi = (R_REG(pi->sh->osh,
					D11_RXE_PHYRS_3(pi))
					>> 8) & 0xff;
			}
		    if (temp_rssi > 127)
				temp_rssi -= 256;
		    /* convert to 0.25dB since rssi delta and
		     * temp delta is in 0.25dB steps
		    */
		    /* Apply RSSI delta */
		    temp_rssi -= tmp;
			stats.rssi = MAX(stats.rssi, temp_rssi);
		}

#endif /* WL11AC */
	} else if (D11REV_IS(pi->sh->corerev, 47) ||
		D11REV_IS(pi->sh->corerev, 48) ||
		D11REV_IS(pi->sh->corerev, 49) ||
		D11REV_IS(pi->sh->corerev, 51) ||
		D11REV_IS(pi->sh->corerev, 58) ||
		D11REV_IS(pi->sh->corerev, 61) ||
		D11REV_IS(pi->sh->corerev, 80) ||
		PHY_MAC_REV_CHECK(pi, 36) ||
		D11REV_IS(pi->sh->corerev, 54)) {
		int core;
		int16 rxpwr_core[PHY_CORE_MAX], rssi_comp;

		/* Update details: refer to rb 111171 */
		FOREACH_CORE(pi, core) {
			/* Supports up to three cores */
			if (core < 3) {
				rxpwr_core[core] =
				wlapi_bmac_read_shm(pi->sh->physhim,
				   (M_MFGTEST_RXAVGPWR_ANT0(pi) + core * 2));
			}
		}
		/* Sign extend */
		FOREACH_CORE(pi, core) {
			if (rxpwr_core[core] > (127 * 4))
				rxpwr_core[core] -= (256 * 4);
		}
		if (*gain_correct == 0) {
			wlc_phy_tempsense_acphy(pi);
		}
		rssi_comp = phy_ac_rssi_compute_compensation(pi->u.pi_acphy->rssii,
			rxpwr_core, 1);

		if (rssii->data->rssi_qdB_en == TRUE) {
			stats.rssi = rssi_comp >> 2;
			stats.rssi_qdb = rssi_comp & 0x3;
		} else {
			stats.rssi =
			((rssi_comp >= 0) ?
			((rssi_comp + 2) >> 2) : -1 * ((-1 * rssi_comp + 2) >> 2));
			stats.rssi_qdb = 0;
		}
	} else {
		/* rssi->last_rssi reported in qdB */
		/* pkteng_stats rssi and rssi_qdb follow format for lq report */
		int16 last_rssi = rssii->last_rssi;
		if (rssii->data->rssi_qdB_en == TRUE) {
			stats.rssi = last_rssi >> 2;
			stats.rssi_qdb = last_rssi & 0x3;
		} else {
			stats.rssi =
			((last_rssi >= 0) ?
			((last_rssi + 2) >> 2) : -1 * ((-1 * last_rssi + 2) >> 2));
			stats.rssi_qdb = 0;
		}
	}
	stats.snr = stats.rssi - PHY_NOISE_FIXED_VAL_NPHY;
	/* The format for rssi and rssi_qdb is similar to lq rssi reporting
	   stats.rssi = rssi_comp >> 2;
	   stats.rssi_qdb = rssi_comp & 0x3
	   Set version to 1 to distinguish with format using in other branches
	   See RB 110951
	*/
	stats.version = 1;

	/* rx pkt stats */
	for (i = 0; i <= NUM_80211_RATES; i++) {
		stats.rxpktcnt[i] = rssii->rxstats[i];
	}

	bcopy(&stats, a,
		(sizeof(wl_pkteng_stats_t) < (uint)alen) ? sizeof(wl_pkteng_stats_t) : (uint)alen);

	return BCME_OK;
}
#endif // endif

static int
phy_ac_rssi_set_gain_delta_2g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = rssii->pi;
	acphy_rssioffset_t *pi_ac_rssioffset = &rssii->aci->sromi->rssioffset;
	uint8 core = deltaValues[0];
	uint8 gain_idx, bw_idx;
	bool lna1byp;
	lna1byp = CHSPEC_IS2G(pi->radio_chanspec) ?
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR2G) != 0) :
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR5G) != 0);

	if (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_2G)) {
		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW_2G; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_2G_PARAMS; gain_idx++) {
				pi_ac_rssioffset->rssi_corr_gain_delta_2g[core][gain_idx][bw_idx] =
					deltaValues[gain_idx + 2*bw_idx + 1];
			}
		}
		if (pi->sh->clk) {
			wlc_phy_set_trloss_reg_acphy(pi, core);
			if (lna1byp) {
				wlc_phy_set_lna1byp_reg_acphy(pi, core);
			}
		}
		return BCME_OK;
	} else {
		PHY_ERROR(("Unsupported RSSI_GAIN_DELTA_2G type!\n"));
		return BCME_UNSUPPORTED;
	}
}

static int
phy_ac_rssi_get_gain_delta_2g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	acphy_rssioffset_t *pi_ac_rssioffset = &rssii->aci->sromi->rssioffset;
	uint8 core, ant;
	uint8 gain_idx, bw_idx, core_idx = 0;

	if (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_2G)) {
	   FOREACH_CORE(rssii->pi, core) {
		  ant = phy_get_rsdbbrd_corenum(rssii->pi, core);
		  deltaValues[5*core_idx] = ant;
		  for (bw_idx = 0; bw_idx < ACPHY_NUM_BW_2G; bw_idx++) {
			 for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_2G_PARAMS; gain_idx++) {
				deltaValues[gain_idx + 2*bw_idx + 5*core_idx +1] =
				  pi_ac_rssioffset->rssi_corr_gain_delta_2g[ant][gain_idx][bw_idx];
			 }
		  }
		  core_idx++;
	   }
	   /* set core to -1 after the last valid entry */
	   deltaValues[core_idx*5] = -1;

	   for (bw_idx = 0; bw_idx < core_idx*5; bw_idx++) {
		printf("%d ", deltaValues[bw_idx]);
	   }
	   printf("\n");
	   return BCME_OK;
	} else {
		PHY_ERROR(("Unsupported RSSI_GAIN_DELTA_2G type!\n"));
		return BCME_UNSUPPORTED;
	}
}

static int
phy_ac_rssi_set_gain_delta_5g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	phy_info_acphy_t *pi_ac = rssii->aci;
	phy_info_t *pi = rssii->pi;
	acphy_rssioffset_t *pi_ac_rssioffset = &pi_ac->sromi->rssioffset;
	uint8 core = deltaValues[0];
	uint8 gain_idx, bw_idx;
	bool lna1byp = CHSPEC_IS2G(pi->radio_chanspec) ?
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR2G) != 0) :
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR5G) != 0);
	uint8 subband_idx = (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_5GL)) ? 0:
			(aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_5GML)) ? 1:
			(aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_5GMU)) ? 2:3;

	if (rssii->data->rssi_cal_rev == FALSE) {
		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS;
				 gain_idx++) {
				pi_ac_rssioffset->rssi_corr_gain_delta_5g
					[core][gain_idx][bw_idx][subband_idx]
					= deltaValues[gain_idx + 2*bw_idx + 1];
			}
		}
		if (pi->sh->clk) {
			wlc_phy_set_trloss_reg_acphy(pi, core);
			if (lna1byp) {
				wlc_phy_set_lna1byp_reg_acphy(pi, core);
			}
		}

		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS;
				 gain_idx++) {
				printf("%d ", pi_ac_rssioffset->rssi_corr_gain_delta_5g
					[core][gain_idx][bw_idx][subband_idx]);
			}
		}
		printf("\n");
	} else if (rssii->data->rssi_cal_rev == TRUE) {
		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS_EXT;
				 gain_idx++) {
				pi_ac_rssioffset->rssi_corr_gain_delta_5g_sub
					[core][gain_idx][bw_idx][subband_idx]
					= deltaValues[gain_idx + 4*bw_idx + 1];
			}
		}
		if (pi->sh->clk) {
			if (ACMAJORREV_40_128(pi->pubpi->phy_rev) &&
			    (pi->u.pi_acphy->sromi->num_rssi_cal_gi_5g == 3)) {
					/* Adjust gaindB table with RSSI CAL results */
					if (core == 0) {
						/* Load the default lna1 gain before adj */
					    wlc_phy_upd_lna1_lna2_gains_acphy(pi);
					}
					wlc_phy_rxgain_adj_forrssi_acphy(pi, core, 3);
			}
			wlc_phy_set_trloss_reg_acphy(pi, core);
			if (lna1byp) {
				wlc_phy_set_lna1byp_reg_acphy(pi, core);
			}
		}

		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS_EXT;
				 gain_idx++) {
				printf("%d ", pi_ac_rssioffset->rssi_corr_gain_delta_5g_sub
					[core][gain_idx][bw_idx][subband_idx]);
			}
		}
		printf("\n");
	}
	return BCME_OK;
}

static int
phy_ac_rssi_get_gain_delta_5g(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	acphy_rssioffset_t *pi_ac_rssioffset = &rssii->aci->sromi->rssioffset;
	uint8 core, ant, core_idx = 0;
	uint8 gain_idx, bw_idx;
	uint8 subband_idx = (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_5GL)) ? 0:
			(aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_5GML)) ? 1:
			(aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_5GMU)) ? 2:3;

	if (rssii->data->rssi_cal_rev == FALSE) {
		FOREACH_CORE(rssii->pi, core) {
			ant = phy_get_rsdbbrd_corenum(rssii->pi, core);
			deltaValues[7*core_idx] =  ant;
			for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
				for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS;
					 gain_idx++) {
					deltaValues[gain_idx + 2*bw_idx + 7*core_idx +1]=
						pi_ac_rssioffset->rssi_corr_gain_delta_5g
						[ant][gain_idx][bw_idx][subband_idx];
				}
			}
			core_idx++;
		}
		deltaValues[core_idx*7] = -1;
	} else if (rssii->data->rssi_cal_rev == TRUE) {
		FOREACH_CORE(rssii->pi, core) {
			deltaValues[13*core_idx] = core;
			for (bw_idx = 0; bw_idx < ACPHY_NUM_BW; bw_idx++) {
			  for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_5G_PARAMS_EXT;
				   gain_idx++) {
				deltaValues[gain_idx + 4*bw_idx + 13*core_idx +1 ]=
				  pi_ac_rssioffset->rssi_corr_gain_delta_5g_sub
				  [core][gain_idx][bw_idx][subband_idx];
			  }
			}
			core_idx++;
		}
		/* set core to -1 after the last valid entry */
		deltaValues[core_idx*13] = -1;
	}
	return BCME_OK;
}

static int
phy_ac_rssi_set_gain_delta_2gb(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	phy_info_t *pi = rssii->pi;
	acphy_rssioffset_t *pi_ac_rssioffset = &rssii->aci->sromi->rssioffset;
	uint8 core = deltaValues[0];
	uint8 gain_idx, bw_idx, subband_idx;
	bool lna1byp = CHSPEC_IS2G(pi->radio_chanspec) ?
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR2G) != 0) :
		((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2)
		& BFL2_LNA1BYPFORTR5G) != 0);

	if (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB0)) {
		subband_idx = 0;
	} else if (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB1)) {
		subband_idx = 1;
	} else if (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB2)) {
		subband_idx = 2;
	} else if (aid == IOV_SVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB3)) {
		subband_idx = 3;
	} else {
		subband_idx = 4;
	}

	for (bw_idx = 0; bw_idx < ACPHY_NUM_BW_2G; bw_idx++) {
		for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_2G_PARAMS_EXT; gain_idx++) {
			pi_ac_rssioffset->rssi_corr_gain_delta_2g_sub[core]
					[gain_idx][bw_idx][subband_idx]
					= deltaValues[gain_idx + 4*bw_idx + 1];
		}
	}
	if (pi->sh->clk) {
		if (ACMAJORREV_40_128(pi->pubpi->phy_rev) &&
		    (pi->u.pi_acphy->sromi->num_rssi_cal_gi_2g == 3)) {
					/* Adjust gaindB table with RSSI CAL results */
					if (core == 0) {
					    wlc_phy_upd_lna1_lna2_gains_acphy(pi);
					}
					wlc_phy_rxgain_adj_forrssi_acphy(pi, core, 3);
		}
		wlc_phy_set_trloss_reg_acphy(pi, core);
		if (lna1byp) {
			wlc_phy_set_lna1byp_reg_acphy(pi, core);
		}
	}

	return BCME_OK;
}

static int
phy_ac_rssi_get_gain_delta_2gb(phy_type_rssi_ctx_t *ctx, uint32 aid, int8 *deltaValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	acphy_rssioffset_t *pi_ac_rssioffset = &rssii->aci->sromi->rssioffset;
	uint8 core, ant;
	uint8 gain_idx, bw_idx, core_idx = 0, subband_idx;

	if (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB0)) {
		subband_idx = 0;
	} else if (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB1)) {
		subband_idx = 1;
	} else if (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB2)) {
		subband_idx = 2;
	} else if (aid == IOV_GVAL(IOV_PHY_RSSI_GAIN_DELTA_2GB3)) {
		subband_idx = 3;
	} else {
		subband_idx = 4;
	}

	FOREACH_CORE(rssii->pi, core) {
		ant = phy_get_rsdbbrd_corenum(rssii->pi, core);
		deltaValues[9*core_idx] = ant;
		for (bw_idx = 0; bw_idx < ACPHY_NUM_BW_2G; bw_idx++) {
			for (gain_idx = 0; gain_idx < ACPHY_GAIN_DELTA_2G_PARAMS_EXT; gain_idx++) {
				deltaValues[gain_idx + 4*bw_idx + 9*core_idx +1] =
				  pi_ac_rssioffset->rssi_corr_gain_delta_2g_sub[ant]
				  [gain_idx][bw_idx][subband_idx];
			}
		}
		core_idx++;
	}
	deltaValues[9*core_idx] = -1;

	return BCME_OK;
}

static int
phy_ac_rssi_set_cal_freq_2g(phy_type_rssi_ctx_t *ctx, int8 *nvramValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	int i;

	for (i = 0; i < 14; i++) {
		rssii->aci->sromi->rssi_cal_freq_grp[i] = nvramValues[i];
	}
	return BCME_OK;
}

static int
phy_ac_rssi_get_cal_freq_2g(phy_type_rssi_ctx_t *ctx, int8 *nvramValues)
{
	phy_ac_rssi_info_t *rssii = (phy_ac_rssi_info_t *)ctx;
	int i;

	for (i = 0; i < 14; i++) {
		nvramValues[i] = rssii->aci->sromi->rssi_cal_freq_grp[i];
	}
	return BCME_OK;
}

/* intra-module data API */
int
phy_ac_rssi_set_qdb_en(phy_ac_rssi_info_t *ri, bool set_val)
{
	ri->data->rssi_qdB_en = set_val;
	return BCME_OK;
}

int
phy_ac_rssi_set_cal_rxgain(phy_ac_rssi_info_t *ri, bool set_val)
{
	ri->data->rxgaincal_rssical = set_val;
	return BCME_OK;
}

int
phy_ac_rssi_set_cal_rev(phy_ac_rssi_info_t *ri, bool set_val)
{
	ri->data->rssi_cal_rev = set_val;
	return BCME_OK;
}
