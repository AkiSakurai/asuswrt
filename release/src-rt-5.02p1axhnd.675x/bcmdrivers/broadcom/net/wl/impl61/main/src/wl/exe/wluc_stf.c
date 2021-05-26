/*
 * wl stf command module
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
 * $Id: wluc_stf.c 775089 2019-05-20 01:52:08Z $
 */

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

#include "wlu_rates_matrix.h"

#define WL_MIMO_PS_MAX_PARAMS			4
#define WL_MIMO_PS_LEARNING_MAX_PARAMS		3

static void wl_txppr_print(ppr_t *ppr, int cck, uint flags);
static void wl_txppr_print_bw(ppr_t *ppr, int cck, uint flags, wl_tx_bw_t bw);
static void wl_txppr_print_he(ppr_t *ppr, ppr_ru_t *ru_ppr, wl_tx_bw_t bw, wl_he_rate_type_t type,
	uint n, uint offset, bool ru);
static cmd_func_t wl_get_current_txppr;
static cmd_func_t wl_txcore;
static cmd_func_t wl_txcore_pwr_offset;
static int wl_mimo_stf(void *wl, cmd_t *cmd, char **argv);
static cmd_func_t wl_spatial_policy, wl_ratetbl_ppr;
static cmd_func_t wl_mimo_ps_cfg;
static cmd_func_t wl_mimo_ps_learning_cfg;
static int wl_mimo_ps_learning_cfg_get_status(void *wl, const char *iovar);
static cmd_func_t wl_mimo_ps_status;
static int wl_mimo_ps_status_dump(wl_mimo_ps_status_t *status);
static int wl_mimo_ps_status_assoc_status_dump(uint8 assoc_status);
static void wl_mimo_ps_status_hw_state_dump(uint16 hw_state);
static cmd_func_t wl_temp_throttle_control;
static cmd_func_t wl_ocl_status;
static void wl_ocl_status_fw_dump(uint16 fw_state);
static void wl_ocl_status_hw_dump(uint8 hw_state);

static cmd_t wl_stf_cmds[] = {
	{ "curppr", wl_get_current_txppr, WLC_GET_VAR, -1,
	"Return current tx power per rate offset."},
	{ "txcore", wl_txcore, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl txcore -k <CCK core mask> -o <OFDM core mask> -s <1..4> -c <core bitmap>\n"
	"\t-k CCK core mask\n"
	"\t-o OFDM core mask\n"
	"\t-s # of space-time-streams\n"
	"\t-c active core (bitmask) to be used when transmitting frames"
	},
	{ "txcore_override", wl_txcore, WLC_GET_VAR, -1,
	"Usage: wl txcore_override\n"
	"\tget the user override of txcore"
	},
	{ "txchain_pwr_offset", wl_txcore_pwr_offset, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl txchain_pwr_offset [qdBm offsets]\n"
	"\tGet/Set the current offsets for each core in qdBm (quarter dBm)"
	},
	{ "mimo_ss_stf", wl_mimo_stf, WLC_GET_VAR, WLC_SET_VAR,
	"get/set SS STF mode.\n"
	"\tUsage: wl mimo_ss_stf <value> <-b a | b>\n"
	"\tvalue: 0 - SISO; 1 - CDD\n"
	"\t-b(band): a - 5G; b - 2.4G"},
	{ "spatial_policy", wl_spatial_policy, WLC_GET_VAR, WLC_SET_VAR,
	"set/get spatial_policy\n"
	"\tUsage: wl spatial_policy <-1: auto / 0: turn off / 1: turn on>\n"
	"\t       to control individual band/sub-band use\n"
	"\t       wl spatial_policy a b c d e\n"
	"\t       where a is 2.4G band setting\n"
	"\t       where b is 5G lower band setting\n"
	"\t       where c is 5G middle band setting\n"
	"\t       where d is 5G high band setting\n"
	"\t       where e is 5G upper band setting"},
	{ "ratetbl_ppr", wl_ratetbl_ppr, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: For get: wl ratetbl_ppr\n"
	"\t     For set: wl ratetbl_ppr <rate> <ppr>" },
	{ "mrc_rssi_threshold", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"get/set mrc_rssi_threshold for mimo power save,\n "
	"\twhen RSSI is below RSSI level specified by mrc_rssi_threshold then \n"
	"\tall rx chains will be made active for all rx frames to benefit "
	"from Maximal ratio combining (MRC) "
	},
	{ "mimo_ps_cfg_change_wait_time", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"get/set mimo_ps_guard_interval for mimo power save,\n "
	"\tspecifies the time STA will wait before receiving packets"
	"when moving from higher configuration to lower configuration\n"
	},
	{ "mimo_ps_cfg", wl_mimo_ps_cfg, WLC_GET_VAR, WLC_SET_VAR,
	"Description: Configure MIMO Power save configuration.\n"
	"\twhere active_chains: 0 for all, 1 for 1 chain. Only 0 and 1 are supported for now.\n"
	"\tIn future more than 1 can be supported to enable more than 1 but less than max chains.\n"
	"\tmode = static (0) or dynamic (1) or disabled(3)."
	"Mode applies only when active_chains is 0.\n"
	"\tbandwidth = Full (0), 20M (1), 40M (2), 80M (3).\n"
	"\tApply changes after learning donot appy(0) apply(1).\n"
	},
	{ "mimo_ps_status", wl_mimo_ps_status, WLC_GET_VAR, -1,
	"usage: mimo_ps_status\n"
	"Displays MIMO PS related state/information"
	},
	{ "mimo_ps_learning_config", wl_mimo_ps_learning_cfg, WLC_GET_VAR, WLC_SET_VAR,
	"Description: Configure mimo ps learning related parameters\n"
	"\tusage: wl mimo_ps_learning_config <flag> <no_of_packets> \n"
	"\tFlag :\n\t0 - Get status of current learning\n"
	"\t1 - ABORT current learning. \n"
	"\t2 - used to configure only no of packets\n"
	},
	{ "temp_throttle_control", wl_temp_throttle_control, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: temp_throttle_control <enable/disable> <value>\n"
	},
	{ "ocl_status", wl_ocl_status, WLC_GET_VAR, -1,
	"usage: ocl_status\n"
	"Displays ocl fw/hw state/information"
	},
	{ "ocl_rssi_threshold", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"get/set ocl_rssi_threshold such that,\n "
	"\twhen RSSI is below the specified ocl_rssi_threshold then \n"
	"\tocl is disabled.\n"
	},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_stf_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register stf commands */
	wl_module_cmds_register(wl_stf_cmds);
}

static void
wl_txppr_print_ru(ppr_ru_t *ru_pprptr, uint flags, chanspec_t chanspec)
{
	uint n = WL_NUM_2x2_ELEMENTS;
	uint offset = 0;
	bool siso = ((flags & WL_TX_POWER_F_MIMO) == 0);
	wl_he_rate_type_t type, type_end;

	switch (CHSPEC_BW(chanspec)) {

	case WL_CHANSPEC_BW_20:
		type_end = WL_HE_RT_RU242;
		break;
	case WL_CHANSPEC_BW_40:
		type_end = WL_HE_RT_RU484;
		break;
	case WL_CHANSPEC_BW_80:
	case WL_CHANSPEC_BW_160:
		type_end = WL_HE_RT_RU996;
		break;
	default:
		type_end = 0;
		printf("Error: Incorrect BW\n");
		break;
	}
	if (!siso) {
		offset = WL_NUM_3x3_ELEMENTS;
		n = WL_NUM_4x4_ELEMENTS;
	}

	for (type = WL_HE_RT_RU26; type <= type_end; type++) {
		switch (type) {
		case WL_HE_RT_RU26:
			printf("\nRU26:\n");
			break;
		case WL_HE_RT_RU52:
			printf("\nRU52:\n");
			break;
		case WL_HE_RT_RU106:
			printf("\nRU106:\n");
			break;
		case WL_HE_RT_RU242:
			printf("\nRU242:\n");
			break;
		case WL_HE_RT_RU484:
			printf("\nRU484:\n");
			break;
		case WL_HE_RT_RU996:
			printf("\nRU996:\n");
			break;
		default:
			printf("Error: incorrect RU type %d\n", type);
			return;
		}
		wl_txppr_print_he(NULL, ru_pprptr, WL_TX_BW_20, type, n, offset, TRUE);
	}
}

static void
wl_txppr_print(ppr_t *ppr, int cck, uint flags)
{

	switch (ppr_get_ch_bw(ppr)) {
	case WL_TX_BW_20:
		printf("\n20MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20);
		break;
	case WL_TX_BW_40:
		printf("\n20 in 40MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20IN40);
		printf("\n40MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_40);
		break;
	case WL_TX_BW_80:
		printf("\n20 in 80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20IN80);
		printf("\n40 in 80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_40IN80);
		printf("\n80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_80);
		break;
	case WL_TX_BW_160:
		printf("\n20 in 160MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20IN160);
		printf("\n40 in 160MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_40IN160);
		printf("\n80 in 160MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_80IN160);
		printf("\n160MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_160);
		break;
	case WL_TX_BW_8080:
		printf("\n20 in 80+80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_20IN8080);
		printf("\n40 in 80+80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_40IN8080);
		printf("\n80 in 80+80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_80IN8080);
		printf("\nchan1 80+80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_8080);
		printf("\nchan2 80+80MHz:\n");
		wl_txppr_print_bw(ppr, cck, flags, WL_TX_BW_8080CHAN2);
		break;
	default:
		break;
	}
	/* MCS32 value is obsoleted */
	/* printf("MCS32        %2d\n", ppr->mcs32); */
	printf("\n");
}

#define PRINT_PPR_RATE_LOOP(idx, len, rates)		  \
			for (idx = 0; idx < len; idx++) { \
				if (rates[idx] == WL_RATE_DISABLED) \
					printf("  -"); \
				else \
					printf(" %2d", rates[idx]); \
			}

static void
wl_txppr_print_cck(ppr_t *ppr, wl_tx_bw_t bw, bool siso)
{
	uint i, j;

	ppr_dsss_rateset_t rate;
	ppr_get_dsss(ppr, bw, WL_TX_CHAINS_1, &rate);
	printf("CCK            ");
	PRINT_PPR_RATE_LOOP(j, WL_RATESET_SZ_DSSS, rate.pwr);
	if (!siso) {
		for (i = WL_TX_CHAINS_2; i <= WL_TX_CHAINS_4; i++) {
			ppr_get_dsss(ppr, bw, i, &rate);
			printf("\nCCK CDD 1x%d    ", i);
			PRINT_PPR_RATE_LOOP(j, WL_RATESET_SZ_DSSS, rate.pwr);
		}
	}
}

static void
wl_txppr_print_ofdm(ppr_t *ppr, wl_tx_bw_t bw)
{
	uint i, j;
	ppr_ofdm_rateset_t ofdm_rate;

	ppr_get_ofdm(ppr, bw, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_rate);
	printf("\nOFDM           ");
	PRINT_PPR_RATE_LOOP(j, WL_RATESET_SZ_OFDM, ofdm_rate.pwr);
	for (i = WL_TX_CHAINS_2; i <= WL_TX_CHAINS_4; i++) {
		ppr_get_ofdm(ppr, bw, WL_TX_MODE_CDD, i, &ofdm_rate);
		printf("\nOFDM-CDD 1x%d   ", i);
		PRINT_PPR_RATE_LOOP(j, WL_RATESET_SZ_OFDM, ofdm_rate.pwr);
	}
	printf("\n");
}

static void
wl_txppr_print_vht(ppr_t *ppr, wl_tx_bw_t bw, uint n, uint offset, bool vht)
{
	uint i, j, rlen;
	int8 *ptr, *vhtptr;
	const char *str = "";
	ppr_vht_mcs_rateset_t vhtrate;

	for (i = 0; i < n; i++) {
		wl_tx_nss_t nss;
		wl_tx_mode_t mode;
		wl_tx_chains_t chains;
		switch (i + offset) {
			case 0:
				str = "MCS-SISO      ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 1:
				str = "MCS-CDD       ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 2:
				str = "MCS STBC      ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_STBC;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 3:
				str = "MCS 8~15      ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = NULL;
				break;
			case 4:
			case 5:
				ptr = NULL;
				vhtptr = NULL;
				break;
			case 6:
				str = "1 Nsts 1 Tx   ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 7:
				str = "1 Nsts 2 Tx   ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 8:
				str = "1 Nsts 3 Tx   ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 9:
				str = "1 Nsts 4 Tx   ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_4;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;

			case 10:
				str = "2 Nsts 2 Tx   ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_2;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 11:
				str = "2 Nsts 3 Tx   ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 12:
				str = "2 Nsts 4 Tx   ";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 13:
				str = "3 Nsts 3 Tx   ";
				nss = WL_TX_NSS_3;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 14:
				str = "3 Nsts 4 Tx   ";
				nss = WL_TX_NSS_3;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			case 15:
				str = "4 Nsts 4 Tx   ";
				nss = WL_TX_NSS_4;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = vhtrate.pwr;
				vhtptr = &vhtrate.pwr[8];
				break;
			default:
				ptr = NULL;
				vhtptr = NULL;
				break;
		}
		if (ptr == NULL)
			continue;
		ppr_get_vht_mcs(ppr, bw, nss, mode, chains, &vhtrate);
		printf("%s ", str);
		if (vht && vhtptr)
			rlen = WL_RATESET_SZ_VHT_MCS_P;
		else
			rlen = WL_RATESET_SZ_HT_MCS;
		PRINT_PPR_RATE_LOOP(j, rlen, ptr);
		printf("\n");
	}
}

static void
wl_txppr_print_he(ppr_t *ppr, ppr_ru_t *ru_ppr, wl_tx_bw_t bw, wl_he_rate_type_t type,
	uint n, uint offset, bool ru)
{
	uint i, j, rlen;
	int8 *ptr;
	const char *str = "";
	ppr_he_mcs_rateset_t herate;

	if (ru == TRUE) {
		if (ru_ppr == NULL) {
			printf("Error: ru_ppr == NULL.\n");
			return;
		}
	} else {
		if (ppr == NULL) {
			printf("Error: ppr == NULL.\n");
			return;
		}
	}

	for (i = 0; i < n; i++) {
		wl_tx_nss_t nss;
		wl_tx_mode_t mode;
		wl_tx_chains_t chains;
		switch (i + offset) {
			case 0:
				str = "HE MCS-SISO   ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = herate.pwr;
				break;
			case 1:
				str = "HE MCS-CDD    ";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = herate.pwr;
				break;
			case 2:
			case 3:
			case 4:
			case 5:
				ptr = NULL;
				break;
			case 6:
				str = "HE 1 Nsts 1 Tx";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_1;
				ptr = herate.pwr;
				break;
			case 7:
				str = "HE 1 Nsts 2 Tx";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_2;
				ptr = herate.pwr;
				break;
			case 8:
				str = "HE 1 Nsts 3 Tx";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_3;
				ptr = herate.pwr;
				break;
			case 9:
				str = "HE 1 Nsts 4 Tx";
				nss = WL_TX_NSS_1;
				mode = WL_TX_MODE_CDD;
				chains = WL_TX_CHAINS_4;
				ptr = herate.pwr;
				break;
			case 10:
				str = "HE 2 Nsts 2 Tx";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_2;
				ptr = herate.pwr;
				break;
			case 11:
				str = "HE 2 Nsts 3 Tx";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = herate.pwr;
				break;
			case 12:
				str = "HE 2 Nsts 4 Tx";
				nss = WL_TX_NSS_2;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = herate.pwr;
				break;
			case 13:
				str = "HE 3 Nsts 3 Tx";
				nss = WL_TX_NSS_3;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_3;
				ptr = herate.pwr;
				break;
			case 14:
				str = "HE 3 Nsts 4 Tx";
				nss = WL_TX_NSS_3;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = herate.pwr;
				break;
			case 15:
				str = "HE 4 Nsts 4 Tx";
				nss = WL_TX_NSS_4;
				mode = WL_TX_MODE_NONE;
				chains = WL_TX_CHAINS_4;
				ptr = herate.pwr;
				break;
			default:
				ptr = NULL;
				break;
		}
		if (ptr == NULL)
			continue;
		if (ru == TRUE) {
			ppr_get_he_ru_mcs(ru_ppr, type, nss, mode, chains, &herate);
		} else {
			ppr_get_he_mcs(ppr, bw, nss, mode, chains, &herate, WL_HE_RT_SU);
		}
		printf("%s ", str);
		rlen = WL_RATESET_SZ_HE_MCS;
		PRINT_PPR_RATE_LOOP(j, rlen, ptr);
		printf("\n");
	}
}

/* print power offset for for a given bandwidth */
static void
wl_txppr_print_bw(ppr_t *ppr, int cck, uint flags, wl_tx_bw_t bw)
{
	uint n = WL_NUM_2x2_ELEMENTS;
	uint offset = 0;
	bool siso = ((flags & WL_TX_POWER_F_MIMO) == 0);
	bool vht = ((flags & WL_TX_POWER_F_VHT) != 0);
	bool he = ((flags & WL_TX_POWER_F_HE) != 0);
	bool he_ru = ((flags & WL_TX_POWER_F_HE_RU) != 0);

	if (!siso) {
		offset = WL_NUM_3x3_ELEMENTS;
		n = WL_NUM_4x4_ELEMENTS;
	}

	if (!he_ru) {
		if (cck) {
			wl_txppr_print_cck(ppr, bw, siso);
		}
		wl_txppr_print_ofdm(ppr, bw);
		wl_txppr_print_vht(ppr, bw, n, offset, vht);
	}

	if (he) {
		wl_txppr_print_he(ppr, NULL, bw, WL_HE_RT_SU, n, offset, FALSE);
	}
}

static int
wl_get_current_txppr(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint flags;
	uint32 curppr_flag = 0;
	chanspec_t chanspec;
	char chanspec_str[CHANSPEC_STR_LEN];
	uint pprsize = ppr_ser_size_by_bw(ppr_get_max_bw());
	wl_txppr_t *wl_txppr = NULL;
	wl_txppr_t *wl_txppr_buf = NULL;
	ppr_t *pprptr = NULL;
	ppr_ru_t *ru_pprptr = NULL;
	wl_wlc_version_t wlc_ver;
	uint16 ppr_ver;

	memset(&wlc_ver, 0, sizeof(wlc_ver));
	wlu_iovar_get(wl, "wlc_ver", &wlc_ver, sizeof(wl_wlc_version_t));

	/* ppr_version is added to wl_wlc_version_t from version 2 */
	if (wlc_ver.version >= 2) {
		ppr_ver = wlc_ver.ppr_version;
	} else {
		ppr_ver = 0;
	}

	if (argv[1] && (!strcmp(argv[1], "--ulofdma") || !strcmp(argv[1], "-u"))) {
		curppr_flag |= WL_TX_POWER_F_HE_RU;
		argv++;
	}

	if (wl_is_he(wl)) {
		curppr_flag |= WL_TX_POWER_F_HE;
	}

	if ((curppr_flag & WL_TX_POWER_F_HE_RU) &&
		!(curppr_flag & WL_TX_POWER_F_HE)) {
		fprintf(stderr, "HE rate is not supported.\n");
		err = BCME_BADARG;
		goto exit;
	}

	if (curppr_flag & WL_TX_POWER_F_HE_RU) {
		if ((ru_pprptr = ppr_ru_create(NULL)) == NULL) {
			err = BCME_NOMEM;
			goto exit;
		}
		pprsize += ppr_ru_get_size(ru_pprptr, PPR_MAX_TX_CHAINS);
	}

	if ((wl_txppr = (wl_txppr_t *)malloc(sizeof(*wl_txppr) + pprsize)) == NULL) {
		fprintf(stderr, "Error allocating memory failed for curppr");
		err = BCME_NOMEM;
		goto exit;
	}

	memset(wl_txppr, 0, sizeof(*wl_txppr));
	wl_txppr->buflen = pprsize;
	if ((err = ppr_init_ser_mem_by_bw(wl_txppr->pprbuf, ppr_get_max_bw(), pprsize))
		!= BCME_OK) {
		goto exit;
	}

	if (WLC_IOCTL_MAXLEN < sizeof(wl_txppr_t) + pprsize) {
		err = BCME_ERROR;
		goto exit;
	}

	argv++;
	if (*argv)
		fprintf(stderr, "Ignoring arguments for %s\n", cmd->name);

	wl_txppr->ver = WL_TXPPR_VERSION;
	wl_txppr->len = WL_TXPPR_LENGTH;
	if (curppr_flag & WL_TX_POWER_F_HE_RU) {
		wl_txppr->flags |= WL_TX_POWER_F_HE_RU;
	}
	if ((err = wlu_iovar_getbuf(wl, "curppr", wl_txppr, sizeof(*wl_txppr) + pprsize,
		buf, WLC_IOCTL_MAXLEN)) < 0) {
		goto exit;
	}

	/* the input buffer is no longer needed, output results are in buf */
	wl_txppr_buf = (wl_txppr_t *)buf;

	/* parse */
	wl_txppr_buf->flags = dtoh32(wl_txppr_buf->flags);
	wl_txppr_buf->chanspec = wl_chspec_from_driver(wl_txppr_buf->chanspec);
	wl_txppr_buf->local_chanspec = wl_chspec_from_driver(wl_txppr_buf->local_chanspec);

	chanspec = wl_txppr_buf->chanspec;
	flags = (wl_txppr_buf->flags & WL_TX_POWER_F_HE_RU) |
		(wl_txppr_buf->flags & WL_TX_POWER_F_HE) |
		(wl_txppr_buf->flags & WL_TX_POWER_F_VHT) |
		(wl_txppr_buf->flags & WL_TX_POWER_F_HT) |
		(wl_txppr_buf->flags & WL_TX_POWER_F_MIMO) |
		(wl_txppr_buf->flags & WL_TX_POWER_F_SISO);

	/* dump */
	printf("Current channel:\t %s\n",
	       wf_chspec_ntoa(wl_txppr_buf->chanspec, chanspec_str));
	printf("BSS channel:\t\t %s\n",
	       wf_chspec_ntoa(wl_txppr_buf->local_chanspec, chanspec_str));

	printf("Power/Rate Dump (in %s): Channel %d\n",
		(wl_txppr_buf->flags & WL_TX_POWER_F_UNIT_QDBM) ? "1/4dB":"1/2dB",
		CHSPEC_CHANNEL(chanspec));
	if ((err = ppr_deserialize_create(NULL, wl_txppr_buf->pprbuf, pprsize, &pprptr, ppr_ver))
		== BCME_OK) {
		if (flags & WL_TX_POWER_F_HE_RU) {
			uint8 *pserbuf;
			uint32 ru_len;

			pserbuf = wl_txppr_buf->pprbuf + ppr_ser_size(pprptr);
			ru_len = wl_txppr_buf->buflen - ppr_ser_size(pprptr);
			(void)ppr_ru_convert_from_tlv(ru_pprptr, pserbuf, ru_len);
			wl_txppr_print_ru(ru_pprptr, flags, chanspec);
		} else {
			wl_txppr_print(pprptr, CHSPEC_IS2G(chanspec), flags);
		}
	}
exit:
	if (wl_txppr != NULL)
		free(wl_txppr);
	if (pprptr != NULL)
		ppr_delete(NULL, pprptr);
	if (ru_pprptr != NULL)
		ppr_ru_delete(NULL, ru_pprptr);

	return err;
}

static int
wl_txcore_pwr_offset(void *wl, cmd_t *cmd, char **argv)
{
	wl_txchain_pwr_offsets_t offsets;
	char *endptr;
	int i;
	long val;
	int err;

	/* toss the command name */
	argv++;

	if (!*argv) {
		err = wlu_iovar_get(wl, cmd->name, &offsets, sizeof(wl_txchain_pwr_offsets_t));

		if (err < 0)
			return err;

		printf("txcore offsets qdBm: %d %d %d %d\n",
		       offsets.offset[0], offsets.offset[1],
		       offsets.offset[2], offsets.offset[3]);

		return 0;
	}

	memset(&offsets, 0, sizeof(wl_txchain_pwr_offsets_t));

	for (i = 0; i < WL_NUM_TXCHAIN_MAX; i++, argv++) {
		if (!*argv)
			break;

		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return BCME_USAGE_ERROR;

		if (val > 0)
			return BCME_BADARG;

		offsets.offset[i] = (int8)val;
	}

	err = wlu_iovar_set(wl, cmd->name, &offsets, sizeof(wl_txchain_pwr_offsets_t));

	return err;
}

static int
wl_txcore(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_txcore";
	int err = 0, opt_err, val;
	uint8 streams = 0;
	bool streams_set = FALSE;
	uint8 core = 0;
	bool core_set = FALSE;
	uint8 cck_mask = 0;
	bool cck_set = FALSE;
	uint8 ofdm_mask = 0;
	bool ofdm_set = FALSE;
	uint8 mcs_mask[4] = {0, 0, 0, 0}; /* pre-initialize # of streams {core:4 | stream:4} */
	bool mcs_set = FALSE;
	uint8 idx;
	uint32 coremask[2] = {0, 0};

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_get(wl, cmd->name, &coremask, sizeof(uint32)*2)) < 0)
			return err;

		printf("txcore enabled bitmap (Nsts {4..1}) 0x%02x 0x%02x 0x%02x 0x%02x\n",
			(coremask[0] >> 24) & 0xff, (coremask[0] >> 16) & 0xff,
			(coremask[0] >> 8) & 0xff, coremask[0] & 0xff);
		printf("txcore mask OFDM 0x%02x  CCK 0x%02x\n",
			(coremask[1] >> 8) & 0xff, coremask[1] & 0xff);
		return 0;
	}

	val = atoi(*argv);
	if (val == -1)
		goto next;

	miniopt_init(&to, fn_name, "w", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 's') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			streams_set = TRUE;
			streams = (to.val & 0x0f);
			if (streams > 4)
				fprintf(stderr, "%s: Nsts > %d\n", fn_name, to.val);
		}
		if (to.opt == 'c') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for stf core\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			core_set = TRUE;
			core = (to.val & 0x0f) << 4;
			if (core == 0) {
				fprintf(stderr, "%s: %1d-stream core cannot be zero\n",
					fn_name, streams);
				err = BCME_BADARG;
				goto exit;
			}
		}
		if (to.opt == 'o') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			ofdm_set = TRUE;
			ofdm_mask = (to.val & 0x0f);
			if (ofdm_mask == 0) {
				fprintf(stderr, "%s: OFDM core cannot be zero\n", fn_name);
				err = BCME_BADARG;
				goto exit;
			}
		}
		if (to.opt == 'k') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for streams\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			cck_set = TRUE;
			cck_mask = (to.val & 0x0f);
			if (cck_mask == 0) {
				fprintf(stderr, "%s: CCK core cannot be zero\n", fn_name);
				err = BCME_BADARG;
				goto exit;
			}
		}

		if (streams_set && core_set) {
			streams_set = core_set = FALSE;
			mcs_set = TRUE;
			idx = streams - 1;
			mcs_mask[idx] = (uint8)(core|streams);
		}
	}

	if (streams_set != core_set) {
		fprintf(stderr, "%s: require to set both -s x -c y\n", fn_name);
		err = BCME_BADARG;
		goto exit;
	}

	if (mcs_set) {
		coremask[0] |= mcs_mask[0] << 0;
		coremask[0] |= mcs_mask[1] << 8;
		coremask[0] |= mcs_mask[2] << 16;
		coremask[0] |= mcs_mask[3] << 24;
	}
	if (cck_set)
		coremask[1] |= cck_mask;
	if (ofdm_set)
		coremask[1] |= ofdm_mask << 8;
next:
	err = wlu_var_setbuf(wl, cmd->name, coremask, sizeof(uint32)*2);
exit:
	return err;
}

static int
wl_mimo_stf(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	int32 int_val;
	bool get = TRUE;
	uint32 len = 0;
	void *ptr = NULL;
	char *endptr;
	int i = 0, j = 0;
	int ret = 0;

	while (argv[i])
		i++;

	if (i > 4)
		return BCME_USAGE_ERROR;

	/* toss the command name */
	argv++;
	j = 1;

	if (i == 1) {
		int_val = -1;
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
	}
	else {
		if (isdigit((unsigned char)(**argv))) {
			get = FALSE;
			int_val = htod32(strtoul(*argv, &endptr, 0));
			if ((int_val != 0) && (int_val != 1)) {
				fprintf(stderr, "wl mimo_ss_stf: bad stf mode.\n");
				return BCME_BADARG;
			}
			memcpy(var, (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
			argv++;
			j++;
		}

		 if (j == i) {
			int_val = -1;
			memcpy(&var[len], (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
		}
		else if (!strncmp(*argv, "-b", 2)) {
			argv++;
			j++;
			if (j == i)
				return BCME_BADARG;

			if (!strcmp(*argv, "a"))
				int_val = 1;
			else if (!strcmp(*argv, "b"))
				int_val = 0;
			else {
				fprintf(stderr,
					"wl mimo_ss_stf: wrong -b option, \"-b a\" or \"-b b\"\n");
				return BCME_USAGE_ERROR;
			}
			j++;
			if (j < i)
				return BCME_BADARG;
			memcpy(&var[len], (char *)&int_val, sizeof(int_val));
			len += sizeof(int_val);
		}
	}

	if (get) {
		if ((ret = wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr)) < 0)
			return ret;

		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return ret;
}

static int
wl_spatial_policy(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err, i, *reply;
	int mode[SPATIAL_MODE_MAX_IDX] = {-1, -1, -1, -1, -1};

	/* Order is 2G, 5G-LOW, 5G-MID, 5G-HIGH, 5G-UPPER
	 * if only one argument given, than all band or sub-band take the
	 * same value
	 */
	if (!*++argv) {
		bool all_same = TRUE;
		if ((err = wlu_var_getbuf(wl, cmd->name, &mode, sizeof(mode), &ptr)) < 0)
			return err;
		reply = (int *)ptr;
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++) {
			 /* check if return values for each band/sub-band is same or not */
			 if (reply[i-1] != reply[i])
				 all_same = FALSE;
		}
		if (all_same)
			printf("%2d\n", reply[0]);
		else {
			printf("2.4GHz       : %2d\n", reply[SPATIAL_MODE_2G_IDX]);
			printf("5GHz (lower) : %2d\n", reply[SPATIAL_MODE_5G_LOW_IDX]);
			printf("5GHz (middle): %2d\n", reply[SPATIAL_MODE_5G_MID_IDX]);
			printf("5GHz (high)  : %2d\n", reply[SPATIAL_MODE_5G_HIGH_IDX]);
			printf("5GHz (upper) : %2d\n", reply[SPATIAL_MODE_5G_UPPER_IDX]);
		}
		return 0;
	}
	mode[0] = atoi(*argv);
	if (!*++argv) {
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++)
			mode[i] = mode[0];
	} else {
		for (i = 1; i < SPATIAL_MODE_MAX_IDX; i++) {
			mode[i] = atoi(*argv);
			if (!*++argv && i < (SPATIAL_MODE_MAX_IDX - 1)) {
				printf("error: missing arguments\n");
				return BCME_USAGE_ERROR;
			}
		}
	}
	err = wlu_var_setbuf(wl, cmd->name, &mode, sizeof(mode));
	return err;
}

static int
wl_ratetbl_ppr(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err, i, *reply;
	int val[12];

	/* Order is 2G, 5G-LOW, 5G-MID, 5G-HIGH, 5G-UPPER
	 * if only one argument given, than all band or sub-band take the
	 * same value
	 */
	memset(&val, 0, sizeof(val));
	if (!*++argv) {
		if ((err = wlu_var_getbuf(wl, cmd->name, &val, sizeof(val), &ptr)) < 0)
			return err;
		reply = (int *)ptr;
		for (i = 0; i < 12; i++)
			printf("%s: %2d\n", (reply[i] & 0x80) ? "OFDM" : "CCK ", (reply[i] & 0x7f));
		return 0;
	}
	val[0] = atoi(*argv++);
	val[1] = atoi(*argv++);
	err = wlu_var_setbuf(wl, cmd->name, &val, sizeof(val));
	return err;
}

static int
wl_mimo_ps_learning_cfg(void *wl, cmd_t *cmd, char **argv)
{
	wl_mimops_learning_cfg_t mimo_ps_learning_cfg;
	char *endptr;
	int i;
	long val;
	int err;

	/* toss the command name */
	argv++;
	if (!*argv) {
		err = wl_mimo_ps_learning_cfg_get_status(wl, cmd->name);
		return err;
	}
	memset(&mimo_ps_learning_cfg, 0, sizeof(wl_mimops_learning_cfg_t));
	for (i = 0; i < WL_MIMO_PS_LEARNING_MAX_PARAMS; i++, argv++) {
		if (!*argv) {
			break;
		}
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			return BCME_BADARG;
		}

		switch (i) {
			case 0:
				if (val == 0)
					mimo_ps_learning_cfg.flag =
						(uint8)WL_MIMO_PS_PS_LEARNING_CFG_STATUS;
				else if (val == 1)
					mimo_ps_learning_cfg.flag =
						(uint8)WL_MIMO_PS_PS_LEARNING_CFG_ABORT;
				else if (val == 2)
					mimo_ps_learning_cfg.flag =
						(uint8)WL_MIMO_PS_PS_LEARNING_CFG_CONFIG;
				else
					return BCME_BADARG;
				break;
			case 1:
				mimo_ps_learning_cfg.no_of_packets_for_learning = (uint32)val;
				mimo_ps_learning_cfg.learning_rssi_threshold = 0;
				break;
			case 2:
				mimo_ps_learning_cfg.learning_rssi_threshold = (int8)val;
				break;

			default:
				return BCME_BADARG;
		}
	}
	/* At the end get the configuration and display */
	mimo_ps_learning_cfg.version = WL_MIMO_PS_PS_LEARNING_CFG_V1;
	err = wlu_iovar_set(wl, cmd->name,
		(void*)&mimo_ps_learning_cfg, sizeof(wl_mimops_learning_cfg_t));
	return err;
}

static int
wl_mimo_ps_learning_cfg_get_status(void *wl, const char *iovar)
{
	wl_mimops_learning_cfg_t mimo_ps_learning_data;
	int err;
	err = wlu_iovar_get(wl, iovar, &mimo_ps_learning_data, sizeof(wl_mimops_learning_cfg_t));
	if (err < 0)
		return err;

	printf("\n Current MIMO PS Learning data :-  \n");
	printf("\t BSSID %s\n",
		wl_ether_etoa(&mimo_ps_learning_data.mimops_learning_data.BSSID));
	printf("\t Start time stamp %d\n",
		mimo_ps_learning_data.mimops_learning_data.startTimeStamp);
	printf("\t End time stamp %d\n",
		mimo_ps_learning_data.mimops_learning_data.endTimeStamp);
	printf("\t Total MIMO above threshold %d\n",
		mimo_ps_learning_data.mimops_learning_data.totalMIMO_above_rssi_threshold);
	printf("\t Total MIMO below threshold %d\n",
		mimo_ps_learning_data.mimops_learning_data.totalMIMO_below_rssi_threshold);
	printf("\t Total SISO above threshold %d\n",
		mimo_ps_learning_data.mimops_learning_data.totalSISO_above_rssi_threshold);
	printf("\t Total SISO below threshold %d\n",
		mimo_ps_learning_data.mimops_learning_data.totalSISO_below_rssi_threshold);
	if (mimo_ps_learning_data.version == WL_MIMO_PS_PS_LEARNING_CFG_V1) {
		printf("\t mimo_ps learning version %d\n",
				mimo_ps_learning_data.version);
		printf("\t mimo_ps learning rssi threshold value %d\n",
				mimo_ps_learning_data.learning_rssi_threshold);
	}
	printf("\t mimo_ps no of packets to wait for  %d\n",
		mimo_ps_learning_data.no_of_packets_for_learning);
	if (mimo_ps_learning_data.mimops_learning_data.reason ==
		WL_MIMO_PS_PS_LEARNING_ABORTED)
			printf("\t REASON : MIMO PS LEARNING ABORTED\n");
	else if (mimo_ps_learning_data.mimops_learning_data.reason ==
		WL_MIMO_PS_PS_LEARNING_COMPLETED)
			printf("\t REASON :  MIMO PS LEARNING COMPLETED\n");
	else if (mimo_ps_learning_data.mimops_learning_data.reason ==
		WL_MIMO_PS_PS_LEARNING_ONGOING)
			printf("\t REASON :  MIMO PS LEARNING IN PRPGRESS\n");
	else
		printf("\t REASON :  UNKNOWN \n");
	return err;
}

static int
wl_mimo_ps_cfg(void *wl, cmd_t *cmd, char **argv)
{
	wl_mimops_cfg_t mimo_ps_cfg;
	char *endptr;
	int i;
	long val;
	int err;

	/* toss the command name */
	argv++;
	if (!*argv) {
		err = wlu_iovar_get(wl, cmd->name, &mimo_ps_cfg, sizeof(wl_mimops_cfg_t));

		if (err < 0)
			return err;
		printf("\n Current MIMO PS CFG :-  \n"
		"\t active chains = %d\n \t mode = %d \n\t bandwidth = %d "
		"\t apply changes after learning = %d\n",
		mimo_ps_cfg.active_chains, mimo_ps_cfg.mode, mimo_ps_cfg.bandwidth,
		mimo_ps_cfg.applychangesafterlearning);
		return 0;
	}
	memset(&mimo_ps_cfg, 0, sizeof(wl_mimops_cfg_t));
	for (i = 0; i < WL_MIMO_PS_MAX_PARAMS; i++, argv++) {
		if (!*argv) {
			break;
		}
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			return BCME_BADARG;
		}

		switch (i) {
			case 0:
				mimo_ps_cfg.active_chains = (int8)val;
				break;
			case 1:
				mimo_ps_cfg.mode = (int8)val;
				break;
			case 2:
				mimo_ps_cfg.bandwidth = (int8)val;
				break;
			case 3:
				mimo_ps_cfg.applychangesafterlearning = (int8) val;
				break;
			default:
				return BCME_BADARG;
		}
	}
	mimo_ps_cfg.version = WL_MIMO_PS_CFG_VERSION_1;
	err = wlu_iovar_set(wl, cmd->name, (void*)&mimo_ps_cfg, sizeof(wl_mimops_cfg_t));
	return err;
}

static int
wl_mimo_ps_status(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_mimo_ps_status_t mimo_ps_status;

	/* toss the command name */
	argv++;

	err = wlu_iovar_get(wl, cmd->name, &mimo_ps_status, sizeof(wl_mimo_ps_status_t));
	if (err < 0)
		return err;
	/* Check version */
	if ((mimo_ps_status.version != WL_MIMO_PS_STATUS_VERSION_1) &&
	    (mimo_ps_status.version != WL_MIMO_PS_STATUS_VERSION_2) &&
	    (mimo_ps_status.version != WL_MIMO_PS_STATUS_VERSION_3)) {
		printf("Unexpected version: %u\n", mimo_ps_status.version);
		return BCME_VERSION;
	}
	/* dump */
	err = wl_mimo_ps_status_dump(&mimo_ps_status);
	return err;
}

static const char *mimo_ps_status_state_str_tbl[] = {
	"STATE_NONE",
	"INFORM_AP_IN_PROGRESS",
	"INFORM_AP_DONE",
	"LEARNING",
	"HW_CONFIGURE",
	"INFORM_AP_PENDING"
};

static const char *mimo_ps_status_bw_str_tbl[] = {
	"FULL",
	"20MHz",
	"40MHz",
	"80MHz",
	"160MHz",
	"8080MHz"
};

static const char *mimo_ps_status_hw_state_str_tbl[] = {
	"NONE",
	"LTE_COEX",
	"MIMOPS_BSS",
	"AWDL_BSS",
	"SCAN",
	"TXPPR",
	"PWRTHOTTLE",
	"TEMPSENSE",
	"IOVAR",
	"AP_BSS"
};

static const char *mimo_ps_status_mhf_flag[] = {
	"Disabled",
	"Enabled",
	"Core Down",
	"Unsupported"
};

static int
wl_mimo_ps_status_dump(wl_mimo_ps_status_t *status)
{
	int err = BCME_OK;
	uint8 ap_cap_bw;

	if (status == NULL)
		return BCME_ERROR;

	ap_cap_bw = WL_MIMO_PS_STATUS_AP_CAP_BW(status->ap_cap);

	/* AP capability (MIMO,SISO)/BW */
	printf("AP capability: ");
	err = wl_mimo_ps_status_assoc_status_dump(WL_MIMO_PS_STATUS_AP_CAP(status->ap_cap));
	if (ap_cap_bw > (ARRAYSIZE(mimo_ps_status_bw_str_tbl)-1)) {
		printf("/Invalid BW value=%d\n", ap_cap_bw);
		err = BCME_BADARG;
	} else {
		if (WL_MIMO_PS_STATUS_AP_CAP(status->ap_cap) != WL_MIMO_PS_STATUS_ASSOC_NONE)
			printf("/%s", mimo_ps_status_bw_str_tbl[ap_cap_bw]);
		printf("\n");
	}

	/* Association */
	printf("Association: ");
	err = wl_mimo_ps_status_assoc_status_dump(status->association_status &
		WL_MIMO_PS_STATUS_ASSOC_STATUS_MASK);
	if (status->association_status & WL_MIMO_PS_STATUS_ASSOC_STATUS_VHT_WITHOUT_OMN)
		printf("\t(AP without OMN)");
	printf("\n");

	/* mimo_ps_cfg state */
	printf("MIMO PS state: ");
	if (status->mimo_ps_state > (ARRAYSIZE(mimo_ps_status_state_str_tbl)-1)) {
		printf("\tInvalid MIMO PS state=%d\n", status->mimo_ps_state);
		err = BCME_BADARG;
	} else {
		printf("\t%s\n", mimo_ps_status_state_str_tbl[status->mimo_ps_state]);
	}

	/* mrc_state */
	printf("MRC state: ");
	if (status->mrc_state == WL_MIMO_PS_STATUS_MRC_NONE)
		printf("\tNot active\n");
	else if (status->mrc_state == WL_MIMO_PS_STATUS_MRC_ACTIVE)
		printf("\tACTIVE\n");
	else {
		printf("\tInvalid MRC state\n");
		err = BCME_BADARG;
	}
	/* bss information */
	printf("bss_rxchain:\t0x%02x\n", status->bss_rxchain);
	printf("bss_txchain:\t0x%02x\n", status->bss_txchain);
	printf("bss_bw: ");
	if (status->bss_bw > (ARRAYSIZE(mimo_ps_status_bw_str_tbl)-1)) {
		printf("\tInvalid bandwidth value=%d\n", status->bss_bw);
		err = BCME_BADARG;
	} else
		printf("\t%s\n", mimo_ps_status_bw_str_tbl[status->bss_bw]);

	/* hw_state */
	wl_mimo_ps_status_hw_state_dump(status->hw_state);

	/* hw information */
	printf("hw_rxchain:\t0x%02x\n", status->hw_rxchain);
	printf("hw_txchain:\t0x%02x\n", status->hw_txchain);
	printf("hw_bw: ");
	if (status->hw_bw > (ARRAYSIZE(mimo_ps_status_bw_str_tbl)-1)) {
		printf("\tInvalid bandwidth value=%d\n", status->hw_bw);
		err = BCME_BADARG;
	} else
		printf("\t\t%s\n", mimo_ps_status_bw_str_tbl[status->hw_bw]);

	if (status->version == WL_MIMO_PS_STATUS_VERSION_1)
		return err;

	printf("PM_BCNRX state: ");
	if (status->pm_bcnrx_state > (ARRAYSIZE(mimo_ps_status_mhf_flag) - 1)) {
		printf("\tInvalid value %d\n", status->pm_bcnrx_state);
	} else {
		printf("\t%s\n", mimo_ps_status_mhf_flag[status->pm_bcnrx_state]);
	}

	printf("SISO_BCMC_RX state: ");
	if (status->siso_bcmc_rx_state > (ARRAYSIZE(mimo_ps_status_mhf_flag) - 1)) {
		printf("\tInvalid value %d\n", status->siso_bcmc_rx_state);
	} else {
		char *astr = "";
		if (status->siso_bcmc_rx_state == WL_MIMO_PS_STATUS_MHF_FLAG_ACTIVE) {
			if ((status->pm_bcnrx_state == WL_MIMO_PS_STATUS_MHF_FLAG_NONE) ||
			    (status->pm_bcnrx_state == WL_MIMO_PS_STATUS_MHF_FLAG_INVALID))
				astr = " (Inactive)";
			else if (status->pm_bcnrx_state == WL_MIMO_PS_STATUS_MHF_FLAG_ACTIVE)
				astr = " (Active)";
		}
		printf("\t%s%s\n", mimo_ps_status_mhf_flag[status->siso_bcmc_rx_state], astr);
	}

	printf("BSSBasicRateSet: %s\n", (status->basic_rates_present ? "Yes" : "No"));

	return err;
}

static void
wl_mimo_ps_status_hw_state_dump(uint16 hw_state)
{
	uint i, n;

	printf("HW state: \t0x%04x\n", hw_state);
	if (hw_state == 0) {
		printf("\t\t%s\n", mimo_ps_status_hw_state_str_tbl[hw_state]);
		return;
	}
	n = MIN(ARRAYSIZE(mimo_ps_status_hw_state_str_tbl), NBITS(hw_state));
	for (i = 0; i < n; i++) {
		if (hw_state & (0x1 << (i-1))) {
			printf("\t\t%s\n", mimo_ps_status_hw_state_str_tbl[i]);
		}
	}
}

static int
wl_mimo_ps_status_assoc_status_dump(uint8 assoc_status)
{
	switch (assoc_status) {
	case WL_MIMO_PS_STATUS_ASSOC_NONE:
		printf("\tNot associated");
		break;
	case WL_MIMO_PS_STATUS_ASSOC_SISO:
		printf("\tSISO");
		break;
	case WL_MIMO_PS_STATUS_ASSOC_MIMO:
		printf("\tMIMO");
		break;
	case WL_MIMO_PS_STATUS_ASSOC_LEGACY:
		printf("\tLEGACY");
		break;
	default:
		printf("Invalid AP cap/association status\n");
		break;
	}
	return BCME_OK;
}

static int
wl_temp_throttle_control(void *wl, cmd_t *cmd, char **argv)
{
	wl_temp_control_t temp_control;
	char *endptr;
	long val;
	int err;

	memset(&temp_control, 0, sizeof(wl_temp_control_t));

	if (!*++argv) {
		/* Get */
		err = wlu_iovar_get(wl, cmd->name, &temp_control, sizeof(wl_temp_control_t));

		if (err < 0) {
			return err;
		}

		if (temp_control.enable == 0) {
			printf("Temp control is off\n");
			return BCME_OK;
		}

		printf("Current mode : 0x%x\n", temp_control.control_bit);
		return BCME_OK;
	} else {
		/* Enable / Disable */
		val = atoi(*argv);
		temp_control.enable = val;
		argv++;

		if (temp_control.enable == 1) {
			if (!*argv) {
				return BCME_ERROR;
			}

			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0')
				return BCME_USAGE_ERROR;

			if (val > 0xFF) {
				printf("Out of range\n");
				return BCME_OUTOFRANGECHAN;
			}

			temp_control.control_bit = val;
		}

		err = wlu_iovar_set(wl, cmd->name, &temp_control, sizeof(wl_temp_control_t));

		return err;
	}
}
static int
wl_ocl_status(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK;
	ocl_status_info_t ocl_status;

	/* toss the command name */
	argv++;

	err = wlu_iovar_get(wl, cmd->name, &ocl_status, sizeof(ocl_status));
	if (err < 0)
		return err;
	/* Check version */
	if (ocl_status.version != WL_OCL_STATUS_VERSION) {
		printf("Unexpected version: %u\n", ocl_status.version);
		return BCME_VERSION;
	}
	/* validate length */
	if (ocl_status.len < (uint8) sizeof(ocl_status)) {
		fprintf(stderr, "OCL Status: short len %d < %d\n",
		        ocl_status.len, (int)sizeof(ocl_status));
		return BCME_ERROR;
	}
	/* dump fw/hw state */
	wl_ocl_status_fw_dump(ocl_status.fw_status);
	wl_ocl_status_hw_dump(ocl_status.hw_status);
	printf("OCL Coremask      : 0x%02x   (Core-%d)\n",
			ocl_status.coremask, ocl_status.coremask-1);

	return err;
}

static const char *ocl_status_fw_state_str_tbl[] = {
	"HOST",
	"RSSI",
	"LTEC",
	"SISO",
	"CAL",
	"CHAN",
	"ASPND"
};
static const char *ocl_status_hw_state_str_tbl[] = {
	"OCL",
	"MIMO",
	"", "", "", "", "",
	"Core Down"
};

static void
wl_ocl_status_fw_dump(uint16 fw_state)
{
	uint i, n;
	bool first = TRUE;

	printf("FW Disable Reason : 0x%04x (", fw_state);
	if (fw_state == 0) {
		printf("%s)\n", "NONE");
		return;
	}
	n = MIN(ARRAYSIZE(ocl_status_fw_state_str_tbl), NBITS(fw_state));
	for (i = 0; i < n; i++) {
		if (fw_state & (0x1 << (i))) {
			printf("%s%s", first ? "": ", ", ocl_status_fw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf(")\n");
}

static void
wl_ocl_status_hw_dump(uint8 hw_state)
{
	uint i, n;
	bool first = TRUE;

	printf("HW Status Bits    : 0x%02x   ", hw_state);
	n = MIN(ARRAYSIZE(ocl_status_hw_state_str_tbl), NBITS(hw_state));
	for (i = 0; i < n; i++) {
		if (hw_state & (0x1 << (i))) {
			printf("%s%s", first ? "(": ", ", ocl_status_hw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf("%s\n", hw_state ? ")" : "");
}
