/*
 * wl tpc command module
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
 * $Id: wluc_tpc.c 775089 2019-05-20 01:52:08Z $
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

static cmd_func_t wl_tpc_lm;
static cmd_func_t wl_get_current_power;
static cmd_func_t wl_get_current_txctrl;

static cmd_t wl_tpc_cmds[] = {
	{ "tpc_mode", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable AP TPC.\n"
	"Usage: wl tpc_mode <mode> \n"
	"\t0 - disable, 1 - BSS power control, 2 - AP power control, 3 - Both (1) and (2)"},
	{ "tpc_period", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set AP TPC periodicity in secs.\n"
	"Usage: wl tpc_period <secs> "},
	{ "tpc_lm", wl_tpc_lm, WLC_GET_VAR, -1,
	"Get current link margins."},
	{ "curpower", wl_get_current_power, WLC_CURRENT_PWR, -1,
	"Return current tx power settings.\n"
	"\t-v, --verbose: display the power settings for every "
	"rate even when every rate in a rate group has the same power.\n"
	"\t-b, --brief: display current rate\n"
	"\t-he: display HE SU rates\n"
	"\t-d, --dlofdma: display DL-OFDMA target power setting of CURRENT FRAME\n"
	"\t-ub, --ub_he: display HE UB and LUB rates. Type UB and LUB are for DL-OFDMA\n"
	"\t-u, --ulofdma: display HE RU rates. Type RU is for UL-OFDMA\n"},
	{ "curtxctrl", wl_get_current_txctrl, WLC_CURRENT_TXCTRL, -1,
	"Return current txctrl settings." },
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;
static int8 wl_ppr_get_pwr(ppr_t* pprptr, reg_rate_index_t rate_idx, wl_tx_bw_t bw);
static void wl_txpwr_array_row_print(ppr_t* pprptr, int8 channel_bandwidth,
	reg_rate_index_t rate_idx);
static void wl_txpwr_array_print(ppr_t* pprptr, ppr_ru_t* ppr_ru_ptr, int8 channel_bandwidth,
	bool verbose, bool is5G, uint32 hw_txchain, uint32 curpower_flag);
static void wl_txpwr_ppr_print(ppr_t* pprptr, ppr_ru_t* ppr_ru_ptr, int vb, ppr_rate_type_t type,
	clm_rate_group_id_t gid, int8 bw, reg_rate_index_t *rate_index, bool is5G,
	uint32 hw_txchain, uint32 curpower_flag);
static void wl_txpwr_ppr_print_row(const char* label, int8 chains, int8 bw, bool vb,
	int8** rates, uint rate_index, uint32 curpower_flag);
void wl_txpwr_ppr_get_rateset(ppr_t* pprptr, ppr_rate_type_t type,
	clm_rate_group_id_t gid, wl_tx_bw_t bw, int8* rateset);
static void wl_txpwr_ppr_ru_get_rateset(ppr_ru_t* ppr_ru_ptr, clm_rate_group_id_t gid,
	int8** rateset);
static int wl_array_check_val(int8 *pwr, uint count, int8 val);
static void wl_txctrl_print_cmn(tx_ctrl_rpt_t* txctrl, bool brief);
/* module initialization */
void
wluc_tpc_module_init(void)
{
	(void)g_swap;

	/* get the global buf */
	buf = wl_get_buf();

	/* register tpc commands */
	wl_module_cmds_register(wl_tpc_cmds);
}

static int
wl_tpc_lm(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint16 val;
	int8 aplm, stalm;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_iovar_getint(wl, cmd->name, (int *)(uintptr)&val)) < 0)
		return ret;

	stalm = val & 0xff;
	aplm = (val >> 8) & 0xff;

	printf("TPC: APs link margin:%d\t STAs link margin:%d\n", aplm, stalm);

	return 0;
}

/* This version number must be incremented for every
 * modification to the curpower output format. Minor changes
 * warrant a decimal point increment. Major (potential
 * script-breaking) changes should be met with a major increment.
 */
#define CURPOWER_OUTPUT_FORMAT_VERSION "8"

#define RATE_STR_LEN 64
#define CURPOWER_VER_STR "cptlv-"
#define WL_CAP_CMD	"cap"

static int
wl_get_curpower_tlv_ver(void *wl)
{
	int error;
	int ret = 0;

	/* Get the CAP variable; search for curpower ver */
	strncpy(buf, WL_CAP_CMD, WLC_IOCTL_MAXLEN);
	if ((error = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MEDLEN)) >= 0) {
		char seps[] = " ";
		char *token;
		buf[WLC_IOCTL_MEDLEN] = '\0';
		token = strtok(buf, seps);
		while (token != NULL) {
			if (!memcmp(token, CURPOWER_VER_STR, strlen(CURPOWER_VER_STR))) {
				char *ver = &token[strlen(CURPOWER_VER_STR)];
				ret = atoi(ver);
			}
			token = strtok(NULL, seps);
		}
	}
	return ret;
}

static void
wl_txpwr_print_summary(tx_pwr_rpt_t *ppr_wl, bool verbose)
{
	int divquo, divrem;
	char chanspec_str[CHANSPEC_STR_LEN];
	char rate_str[RATE_STR_LEN];
	uint32 ppr_flag = dtoh32(ppr_wl->flags);
	char* bss_chan;
	int8 temp_val;
	bool neg;

	BCM_REFERENCE(verbose);
	printf("%-23s%s\n", "Output Format Version:", CURPOWER_OUTPUT_FORMAT_VERSION);
	printf("%-23s%s, %s\n", "Power Control:",
		(ppr_flag & WL_TX_POWER_F_ENABLED) ? "On" : "Off",
		(ppr_flag & WL_TX_POWER_F_HW) ? "HW" : "SW");

	printf("%-23s%s\n", "Current Channel:",
	       wf_chspec_ntoa(ppr_wl->chanspec, chanspec_str));

	if (ppr_wl->local_chanspec == 0) {
		bss_chan = "not associated";
	} else {
		bss_chan = wf_chspec_ntoa(ppr_wl->local_chanspec, chanspec_str);
	}
	printf("%-23s%s\n", "BSS Channel:", bss_chan ? bss_chan : "invalid");

	printf("%-23s%d.%d dBm\n", "BSS Local Max:",
	       DIV_QUO(ppr_wl->local_max, 4), DIV_REM(ppr_wl->local_max, 4));
	printf("%-23s%d.%d dB\n", "BSS Local Constraint:",
	       DIV_QUO(ppr_wl->local_constraint, 4), DIV_REM(ppr_wl->local_constraint, 4));

	printf("%-23s", "Channel Width:");
	switch (ppr_wl->channel_bandwidth) {
	case WL_BW_2P5MHZ:
		printf("2.5MHz\n");
		break;
	case WL_BW_5MHZ:
		printf("5MHz\n");
		break;
	case WL_BW_10MHZ:
		printf("10MHz\n");
		break;
	case WL_BW_20MHZ:
		printf("20MHz\n");
		break;
	case WL_BW_40MHZ:
		printf("40MHz\n");
		break;
	case WL_BW_80MHZ:
		printf("80MHz\n");
		break;
	case WL_BW_160MHZ:
		printf("160MHz\n");
		break;
	case WL_BW_8080MHZ:
		printf("80+80MHz\n");
		break;
	default:
		fprintf(stderr, "Error: Unknown bandwidth %d\n", ppr_wl->channel_bandwidth);
		return;
	}

	temp_val = (int8)(ppr_wl->user_target & 0xff);
	divquo = DIV_QUO(temp_val, 4);
	divrem = DIV_REM(temp_val, 4);
	neg = (divrem < 0) || (divquo < 0);
	divrem = ABS(divrem);
	divquo = ABS(divquo);

	printf("%-23s%s%d.%d dBm\n", "User Target:", neg ? "-" : "", divquo, divrem);

	printf("%-23s%d.%d dB\n", "SROM Antgain 2G:",
	       DIV_QUO(ppr_wl->antgain[0], 4), DIV_REM(ppr_wl->antgain[0], 4));
	printf("%-23s%d.%d dB\n", "SROM Antgain 5G:",
	       DIV_QUO(ppr_wl->antgain[1], 4), DIV_REM(ppr_wl->antgain[1], 4));

	printf("%-23s", "SAR:");
	if (ppr_wl->sar != WLC_TXPWR_MAX) {
		printf("%d.%d dB\n", DIV_QUO(ppr_wl->sar, 4), DIV_REM(ppr_wl->sar, 4));
	} else {
		printf("-\n");
	}

	printf("%-23s", "Open loop:");
	if (ppr_flag & WL_TX_POWER_F_OPENLOOP) {
		printf("On\n");
	} else {
		printf("Off\n");
	}

	printf("%-23s", "Current rate:");
	wl_rate_print(rate_str, ppr_wl->last_tx_ratespec);
	printf("[%s] %s\n", get_reg_rate_string_from_ratespec(ppr_wl->last_tx_ratespec),
		rate_str);
	printf("\n");
}

#define CURPOWER_HE 0x1
/* Flags of curpower option */
#define CURPOWER_DLOFDMA 0x2
#define CURPOWER_UB 0x4 /* Display type UB, LUB of DL-OFDMA */
#define CURPOWER_ULOFDMA 0x8 /* Display type RU26, RU52, RU106, RU242, RU484, RU996 of UL-OFDMA */
#define CURPOWER_SU 0x10 /* Display HE SU rates */

static int
wl_get_current_power(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int mimo;
	int i;
	int pprtype_count;
	bool verbose = FALSE;
	bool brief = FALSE;
	bool txcap = FALSE;
	int16 power_target, ppr_ver;
	int clk;
	int val;
	chanspec_t chanspec;
	uint8 *ppr_ser;
	uint32 pprsize;
	size_t ppr_rpt_size;
	tx_pwr_rpt_t *ppr_wl = NULL;
	tx_pwr_rpt_t *ppr_wl_buf = NULL;
	tx_ctrl_rpt_t txctrl;
	ppr_t* ppr_board = NULL;
	ppr_t* ppr_target = NULL;
	ppr_t* ppr_reg = NULL;
	ppr_ru_t *clm_he_txpwr_limit = NULL;
	ppr_ru_t *board_he_txpwr_limit = NULL;
	ppr_ru_t *final_he_txpwr_limit = NULL;
	chanspec_t phy_chanspec_targetpower;
	chanspec_t phy_chanspec_boardlimits;
	chanspec_t phy_chanspec_reglimits;
	chanspec_t phy_chanspec_rulimits;
	uint32 ppr_flag = 0;
	uint32 curpower_flag = 0;
	uint32 hw_txchain = 0;
	int divquo, divrem;
	bool neg;
	int wlc_idx = g_wlc_idx;
	int tlv_ver;
	wl_wlc_version_t wlc_ver;
	bool legacy_curpower_mode = FALSE;

	if (argv[1] && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
		return BCME_USAGE_ERROR;
	}

	memset(&wlc_ver, 0, sizeof(wlc_ver));
	err = wlu_iovar_get(wl, "wlc_ver", &wlc_ver, sizeof(wl_wlc_version_t));
	if (err != BCME_OK) {
		/* old firmware may not support it */
		legacy_curpower_mode = TRUE;
	} else {
		if ((wlc_ver.wlc_ver_major < 5) ||
			((wlc_ver.wlc_ver_major == 5) && (wlc_ver.wlc_ver_minor != 0)) ||
			(wlc_ver.wlc_ver_major == 6)) {
			/* older branches, Dingo branches other than 9.30, Iguana.13.10
			 * will use legacy curpower mode
			 */
			legacy_curpower_mode = TRUE;
		}
	}

	/* ppr_version is added to wl_wlc_version_t from version 2 */
	if (wlc_ver.version >= 2) {
		ppr_ver = wlc_ver.ppr_version;
	} else {
		ppr_ver = 0;
	}

	if (argv[1] && (!strcmp(argv[1], "--verbose") || !strcmp(argv[1], "-v"))) {
		verbose = TRUE;
		argv++;
	}
	if (argv[1] && (!strcmp(argv[1], "--brief") || !strcmp(argv[1], "-b"))) {
		brief = TRUE;
		argv++;
	}
	if (argv[1] && !strcmp(argv[1], "-he")) {
		curpower_flag |= CURPOWER_SU;
		argv++;
	} else if (argv[1] && (!strcmp(argv[1], "--dlofdma") || !strcmp(argv[1], "-d"))) {
		curpower_flag |= CURPOWER_DLOFDMA;
		argv++;
	} else if (argv[1] && (!strcmp(argv[1], "--ub_he") || !strcmp(argv[1], "-ub"))) {
		curpower_flag |= (CURPOWER_DLOFDMA | CURPOWER_UB);
		argv++;
	} else if (argv[1] && (!strcmp(argv[1], "--ulofdma") || !strcmp(argv[1], "-u"))) {
		curpower_flag |= CURPOWER_ULOFDMA;
		argv++;
	}
	argv++;
	if (*argv) {
		fprintf(stderr, "Ignoring arguments for %s\n", cmd->name);
	}

	/* Clear the interface index for the iovars which don't support "-w" option */
	g_wlc_idx = -1;
	tlv_ver = wl_get_curpower_tlv_ver(wl);

	/* firmware will crash if clk = 0 while using curpower */
	if ((err = wlu_get(wl, WLC_GET_CLK, &clk, sizeof(int))) < 0)
		goto exit;

	if (!clk) {
		fprintf(stderr, "Error: clock not active, do wl up (if not done already) "
				"and force mpc 0 to active clock\n");
		err = BCME_ERROR;
		goto exit;
	}

	if (wl_is_he(wl)) {
		curpower_flag |= CURPOWER_HE;
	}

	if ((curpower_flag & (CURPOWER_DLOFDMA | CURPOWER_UB | CURPOWER_ULOFDMA | CURPOWER_SU)) &&
		!(curpower_flag & CURPOWER_HE)) {
		fprintf(stderr, "HE rate is not supported.\n");
		err = BCME_BADARG;
		goto exit;
	}

	/* Restore the wlc index before calling ivoars for the provided interface */
	g_wlc_idx = wlc_idx;
	if ((err = wlu_iovar_getint(wl, "chanspec", (int *)&val)) < 0) {
		goto exit;
	}

	chanspec = wl_chspec32_from_driver(val);

	if (tlv_ver) {
		if ((ppr_board = ppr_create(NULL, ppr_chanspec_bw(chanspec))) == NULL) {
			err = BCME_NOMEM;
			goto exit;
		}
		if ((ppr_target = ppr_create(NULL, ppr_chanspec_bw(chanspec))) == NULL) {
			err = BCME_NOMEM;
			goto exit;
		}
		if ((ppr_reg = ppr_create(NULL, ppr_chanspec_bw(chanspec))) == NULL) {
			err = BCME_NOMEM;
			goto exit;
		}
		if (curpower_flag & CURPOWER_ULOFDMA) {
			if ((clm_he_txpwr_limit = ppr_ru_create(NULL)) == NULL) {
				err = BCME_NOMEM;
				goto exit;
			}
			if ((board_he_txpwr_limit = ppr_ru_create(NULL)) == NULL) {
				err = BCME_NOMEM;
				goto exit;
			}
			if ((final_he_txpwr_limit = ppr_ru_create(NULL)) == NULL) {
				err = BCME_NOMEM;
				goto exit;
			}
		}
		pprsize = ppr_get_tlv_size(ppr_target, ppr_chanspec_bw(chanspec),
				PPR_MAX_TX_CHAINS);
		pprsize = MAX(pprsize, ppr_ru_get_size(clm_he_txpwr_limit, PPR_MAX_TX_CHAINS));
	} else {
		pprsize = ppr_ser_size_by_bw(ppr_get_max_bw());
	}

	if (legacy_curpower_mode) {
		if (tlv_ver) {
			pprsize -= ppr_get_he_tlv_size(ppr_target, ppr_chanspec_bw(chanspec),
				PPR_MAX_TX_CHAINS);
		}
		ppr_rpt_size = sizeof(tx_pwr_rpt_t) + pprsize * WL_LEGACY_TXPPR_SER_BUF_NUM;
		ppr_wl_buf = (tx_pwr_rpt_t *)malloc(ppr_rpt_size);
		if (ppr_wl_buf == NULL) {
			fprintf(stderr, "Allocating mem failed for curpower\n");
			err = BCME_NOMEM;
			goto exit;
		}

		ppr_wl = ppr_wl_buf;
		memset(ppr_wl, WL_RATE_DISABLED, ppr_rpt_size);

		if (!tlv_ver) {
			for (i = 0; i < WL_LEGACY_TXPPR_SER_BUF_NUM; i++) {
				ppr_ser = ppr_wl->pprdata + i*pprsize;
				ppr_init_ser_mem_by_bw(ppr_ser, ppr_get_max_bw(), pprsize);
			}
		}

		ppr_wl->ppr_len = pprsize;
		ppr_wl->version = TX_POWER_T_VERSION;

		if ((err = wlu_get(wl, cmd->get, ppr_wl, ppr_rpt_size)) < 0) {
			fprintf(stderr, "Error: Curpower failed. %d ", err);
			fprintf(stderr, "Bring up interface and disable mpc "
				"if necessary (wl mpc 0)\n");
			goto exit;
		}

		/* parse */
		if (ppr_wl->version != TX_POWER_T_VERSION) {
			printf("error: version mismatch - driver %d, "
				"wl executable was expecting %d\n",
				ppr_wl->version, TX_POWER_T_VERSION);
			err = BCME_ERROR;
			goto exit;
		}

		ppr_flag = ppr_wl->flags = dtoh32(ppr_wl->flags);
		ppr_wl->chanspec = wl_chspec_from_driver(ppr_wl->chanspec);
		ppr_wl->local_chanspec = wl_chspec_from_driver(ppr_wl->local_chanspec);

		mimo = (ppr_flag & WL_TX_POWER_F_HT) | (ppr_flag & WL_TX_POWER_F_MIMO) |
			(ppr_flag & WL_TX_POWER_F_SISO);
		ppr_ser = ppr_wl->pprdata;

		if (tlv_ver) {
			(void)ppr_convert_from_tlv(ppr_board, ppr_wl->pprdata, ppr_wl->ppr_len);
			ppr_ser += ppr_wl->ppr_len;
			(void)ppr_convert_from_tlv(ppr_target, ppr_ser, ppr_wl->ppr_len);
			ppr_ser += ppr_wl->ppr_len;
			(void)ppr_convert_from_tlv(ppr_reg, ppr_ser, ppr_wl->ppr_len);
		} else {
			/* Try non TLV decode */
			if ((err = ppr_deserialize_create(NULL, ppr_wl->pprdata, ppr_wl->ppr_len,
				&ppr_board, ppr_ver)) != BCME_OK) {
				fprintf(stderr, "Error: read ppr board limit failed\n");
				goto exit;
			}
			ppr_ser += ppr_wl->ppr_len;
			if ((err = ppr_deserialize_create(NULL, ppr_ser, ppr_wl->ppr_len,
				&ppr_target, ppr_ver)) != BCME_OK) {
				fprintf(stderr, "Error: read ppr target power failed\n");
				goto exit;
			}
			ppr_ser += ppr_wl->ppr_len;
			if ((err = ppr_deserialize_create(NULL, ppr_ser, ppr_wl->ppr_len,
				&ppr_reg, ppr_ver)) != BCME_OK) {
				fprintf(stderr, "Error: read ppr regulatory limits failed\n");
				goto exit;
			}
		}
	} else {
		if (curpower_flag & CURPOWER_ULOFDMA) {
			ppr_rpt_size = (sizeof(tx_pwr_rpt_t) + pprsize) * WL_TXPPR_SER_BUF_NUM;
		} else {
			/* Excluding RU_REGLIMITS, RU_BOARDLIMITS, RU_TARGETPOWER */
			ppr_rpt_size = (sizeof(tx_pwr_rpt_t) + pprsize) *
				(WL_TXPPR_SER_BUF_NUM - 3);
		}
		ppr_wl_buf = (tx_pwr_rpt_t *)malloc(ppr_rpt_size);
		if (ppr_wl_buf == NULL) {
			fprintf(stderr, "Allocating mem failed for curpower\n");
			err = BCME_NOMEM;
			goto exit;
		}

		ppr_wl = ppr_wl_buf;
		memset(ppr_wl, WL_RATE_DISABLED, ppr_rpt_size);
		ppr_rpt_size = (sizeof(tx_pwr_rpt_t) + pprsize);

		for (pprtype_count = 1; pprtype_count <= WL_TXPPR_SER_BUF_NUM; pprtype_count++) {
			if (!(curpower_flag & CURPOWER_ULOFDMA)) {
				/* Excluding RU_REGLIMITS, RU_BOARDLIMITS, RU_TARGETPOWER */
				if (pprtype_count >= PPRTYPE_RU_REGLIMITS) {
					continue;
				}
			}
			if (!tlv_ver) {
				ppr_ser = ppr_wl->pprdata;
				ppr_init_ser_mem_by_bw(ppr_ser, ppr_get_max_bw(), pprsize);
			}

			ppr_wl->ppr_len = pprsize;
			ppr_wl->flags = pprtype_count;
			ppr_wl->version = TX_POWER_T_VERSION;

			if ((err = wlu_get(wl, cmd->get, ppr_wl, ppr_rpt_size)) < 0) {
				fprintf(stderr, "Error: Curpower failed. %d ", err);
				fprintf(stderr, "Bring up interface and disable mpc "
					"if necessary (wl mpc 0)\n");
				goto exit;
			}

			/* parse */
			if (ppr_wl->version != TX_POWER_T_VERSION) {
				printf("error: version mismatch - driver %d, "
					"wl executable was expecting %d\n",
					ppr_wl->version, TX_POWER_T_VERSION);
				err = BCME_ERROR;
				goto exit;
			}

			switch (pprtype_count) {
			case PPRTYPE_TARGETPOWER:
				ppr_flag = dtoh32(ppr_wl->flags);
				phy_chanspec_targetpower = ppr_wl->chanspec =
					wl_chspec_from_driver(ppr_wl->chanspec);

				mimo = (ppr_flag & WL_TX_POWER_F_HT) |
					(ppr_flag & WL_TX_POWER_F_MIMO) |
					(ppr_flag & WL_TX_POWER_F_SISO);

				if (tlv_ver) {
					(void)ppr_convert_from_tlv(ppr_target,
						ppr_wl->pprdata, ppr_wl->ppr_len);
				} else {
					if ((err = ppr_deserialize_create(NULL, ppr_ser,
							ppr_wl->ppr_len, &ppr_target, ppr_ver))
							!= BCME_OK) {
						fprintf(stderr,
							"Error: read ppr target power failed\n");
						goto exit;
					}
				}
				break;
			case PPRTYPE_BOARDLIMITS:
				phy_chanspec_boardlimits = wl_chspec_from_driver(ppr_wl->chanspec);

				if (phy_chanspec_boardlimits != phy_chanspec_targetpower) {
					fprintf(stderr, "Error: chanspec different - "
						"Run wl curpower again\n");
					err = BCME_ERROR;
					goto exit;
				}
				if (tlv_ver) {
					(void)ppr_convert_from_tlv(ppr_board, ppr_wl->pprdata,
						ppr_wl->ppr_len);
				} else {
					/* Try non TLV decode */
					if ((err = ppr_deserialize_create(NULL, ppr_ser,
						ppr_wl->ppr_len, &ppr_board, ppr_ver))
						!= BCME_OK) {
						fprintf(stderr,
							"Error: read ppr board limit failed\n");
						goto exit;
					}
				}
				break;
			case PPRTYPE_REGLIMITS:
				phy_chanspec_reglimits = wl_chspec_from_driver(ppr_wl->chanspec);

				if (phy_chanspec_reglimits != phy_chanspec_targetpower) {
					err = BCME_ERROR;
					fprintf(stderr, "Error: chanspec different - "
						"Run wl curpower again\n");
					goto exit;
				}
				if (tlv_ver) {
					(void)ppr_convert_from_tlv(ppr_reg, ppr_wl->pprdata,
						ppr_wl->ppr_len);
				} else {
					if ((err = ppr_deserialize_create(NULL, ppr_ser,
						ppr_wl->ppr_len, &ppr_reg, ppr_ver)) != BCME_OK) {
						fprintf(stderr, "Error: read ppr regulatory "
							"limits failed\n");
						goto exit;
					}
				}
				break;
			case PPRTYPE_RU_REGLIMITS:
				phy_chanspec_rulimits = wl_chspec_from_driver(ppr_wl->chanspec);
				if (phy_chanspec_rulimits != phy_chanspec_targetpower) {
					fprintf(stderr, "Error: chanspec"
						" different - Run wl"
						" curpower again\n");
					err = BCME_ERROR;
					goto exit;
				}
				/* Only tlv version for HE rates */
				(void)ppr_ru_convert_from_tlv(
					clm_he_txpwr_limit,
					ppr_wl->pprdata,
					ppr_wl->ppr_len);
				break;
			case PPRTYPE_RU_BOARDLIMITS:
				phy_chanspec_rulimits = wl_chspec_from_driver(ppr_wl->chanspec);
				if (phy_chanspec_rulimits != phy_chanspec_targetpower) {
					fprintf(stderr, "Error: chanspec"
						" different - Run wl"
						" curpower again\n");
					err = BCME_ERROR;
					goto exit;
				}
				/* Only tlv version for HE rates */
				(void)ppr_ru_convert_from_tlv(
					board_he_txpwr_limit,
					ppr_wl->pprdata,
					ppr_wl->ppr_len);
				break;
			case PPRTYPE_RU_TARGETPOWER:
				phy_chanspec_rulimits = wl_chspec_from_driver(ppr_wl->chanspec);
				if (phy_chanspec_rulimits != phy_chanspec_targetpower) {
					fprintf(stderr, "Error: chanspec"
						" different - Run wl"
						" curpower again\n");
					err = BCME_ERROR;
					goto exit;
				}
				/* Only tlv version for HE rates */
				(void)ppr_ru_convert_from_tlv(
					final_he_txpwr_limit,
					ppr_wl->pprdata,
					ppr_wl->ppr_len);
				break;
			default:
				fprintf(stderr, "Error: pprtype invalid\n");
				goto exit;
			}

			ppr_wl = (tx_pwr_rpt_t *)(((uint8 *)ppr_wl) + ppr_rpt_size);
		}
		ppr_wl = ppr_wl_buf;
	}

	/* dump */
	wl_txpwr_print_summary(ppr_wl, verbose);

	if ((err = wlu_iovar_get(wl, "hw_txchain", &hw_txchain, sizeof(hw_txchain))) < 0) {
		fprintf(stderr, "Fail to get hw_txchain!\n");
		goto exit;
	}
	hw_txchain = bcm_bitcount((uint8 *)&hw_txchain, sizeof(uint8));

	printf("Regulatory Limits:\n");
	if (brief) {
		wl_txpwr_array_row_print(ppr_reg, ppr_wl->channel_bandwidth,
			get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
	} else {
		wl_txpwr_array_print(ppr_reg, clm_he_txpwr_limit, ppr_wl->channel_bandwidth,
			verbose, CHSPEC_IS5G(chanspec), hw_txchain, curpower_flag);
	}
	printf("\n");

	printf("%-23s%d\n", "Core Index:", ppr_wl->display_core);

	printf("Board Limits:\n");
	if (brief) {
		wl_txpwr_array_row_print(ppr_board, ppr_wl->channel_bandwidth,
			get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
	} else {
		wl_txpwr_array_print(ppr_board, board_he_txpwr_limit, ppr_wl->channel_bandwidth,
			verbose, CHSPEC_IS5G(chanspec), hw_txchain, curpower_flag);
	}
	printf("\n");

	printf("Power Targets:\n");
	if (brief) {
		wl_txpwr_array_row_print(ppr_target, ppr_wl->channel_bandwidth,
			get_reg_rate_index_from_ratespec(ppr_wl->last_tx_ratespec));
	} else {
		wl_txpwr_array_print(ppr_target, final_he_txpwr_limit, ppr_wl->channel_bandwidth,
			verbose, CHSPEC_IS5G(chanspec), hw_txchain, curpower_flag);
	}
	printf("\n");

	/* print the different power estimate combinations */
	if (mimo) {
		txcap = (ppr_flag & WL_TX_POWER_F_TXCAP) != 0;
		if (txcap) {
			printf("Tx pwr Cap                                :\t");

			for (i = 0; i < ppr_wl->rf_cores; i++) {
				printf("%2d.%02d  ",
					DIV_QUO((int8)ppr_wl->SARLIMIT[i], 4),
					DIV_REM(ppr_wl->SARLIMIT[i], 4));
			}
			printf("\n");
		}

		printf("PHY Maximum Power Target among all rates  :\t");
		for (i = 0; i < ppr_wl->rf_cores; i++) {
			power_target = (int8)ppr_wl->tx_power_max[i];
			divquo = DIV_QUO(power_target, 4);
			divrem = DIV_REM(power_target, 4);
			neg = (divrem < 0) || (divquo < 0);
			divrem = ABS(divrem);
			divquo = ABS(divquo);
			divquo = neg ? -divquo : divquo;
			if ((divquo == 0) && neg) {
				printf(" -0.%02d  ", divrem);
			} else {
				printf("%3d.%02d  ", divquo, divrem);
			}
		}
		printf("\n");
		if (curpower_flag & CURPOWER_DLOFDMA) {
			if ((err = wlu_get(wl, WLC_CURRENT_TXCTRL, &txctrl,
				sizeof(tx_ctrl_rpt_t))) < 0) {
				goto exit;
			}
			if (txctrl.txctrlwd_version != TX_CTRLWD_T_VERSION) {
				goto exit;
			}
			printf("PHY Power Target for the current rate	  :\t");
			for (i = 0; i < ppr_wl->rf_cores; i++) {
				if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED) {
					power_target = (int8)ppr_wl->tx_power_max[i] -
						(int8)(txctrl.pwr_offset[i]);

					/*      for ACPHY, clip the power_target if it
					        is larger than the SAR limit for the
					        current path. For non-ACPHY or
					        WLC_SARLIMIT disabled, this threshold is
					        set to be MAX pwr, ie. 127
					*/
					if (power_target > ppr_wl->SARLIMIT[i]) {
						power_target = ppr_wl->SARLIMIT[i];
					}
					divquo = DIV_QUO(power_target, 4);
					divrem = DIV_REM(power_target, 4);
					neg = (divrem < 0) || (divquo < 0);
					divrem = ABS(divrem);
					divquo = ABS(divquo);
					divquo = neg ? -divquo : divquo;

					if ((divquo == 0) && neg) {
						printf(" -0.%02d  ", divrem);
					} else {
						printf("%3d.%02d  ", divquo, divrem);
					}
				} else {
					printf("   -    ");
				}
			}
			printf("\n");
		} else {
			if (txcap) {
				printf("Final Maximum Power Target among all rates:\t");
				for (i = 0; i < ppr_wl->rf_cores; i++) {
					power_target = MIN((int8)ppr_wl->tx_power_max[i],
						ppr_wl->SARLIMIT[i]);
					divquo = DIV_QUO(power_target, 4);
					divrem = DIV_REM(power_target, 4);
					neg = (divrem < 0) || (divquo < 0);
					divrem = ABS(divrem);
					divquo = ABS(divquo);
					divquo = neg ? -divquo : divquo;

					if ((divquo == 0) && neg) {
						printf(" -0.%02d  ", divrem);
					} else {
						printf("%3d.%02d  ", divquo, divrem);
					}
				}
				printf("\n");

				printf("PHY Power Target for the current rate     :\t");
				for (i = 0; i < ppr_wl->rf_cores; i++) {
					if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED) {
						power_target = (int8)ppr_wl->tx_power_max[i] -
							ppr_wl->target_offsets[i];
						divquo = DIV_QUO(power_target, 4);
						divrem = DIV_REM(power_target, 4);
						neg = (divrem < 0) || (divquo < 0);
						divrem = ABS(divrem);
						divquo = ABS(divquo);
						divquo = neg ? -divquo : divquo;

						if ((divquo == 0) && neg) {
							printf(" -0.%02d  ", divrem);
						} else {
							printf("%3d.%02d  ", divquo, divrem);
						}
					} else {
						printf("   -    ");
					}
				}
				printf("\n");
				printf("Final Power Target for the current rate   :\t");
			} else {
				printf("PHY Power Target for the current rate     :\t");
			}

			for (i = 0; i < ppr_wl->rf_cores; i++) {
				if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED) {
					power_target = (int8)ppr_wl->tx_power_max[i] -
						ppr_wl->target_offsets[i];

					/*      for ACPHY, clip the power_target if it
						is larger than the SAR limit for the
						current path. For non-ACPHY or
						WLC_SARLIMIT disabled, this threshold is
						set to be MAX pwr, ie. 127
					*/
					if (power_target > ppr_wl->SARLIMIT[i]) {
						power_target = ppr_wl->SARLIMIT[i];
					}
					divquo = DIV_QUO(power_target, 4);
					divrem = DIV_REM(power_target, 4);
					neg = (divrem < 0) || (divquo < 0);
					divrem = ABS(divrem);
					divquo = ABS(divquo);
					divquo = neg ? -divquo : divquo;

					if ((divquo == 0) && neg) {
						printf(" -0.%02d  ", divrem);
					} else {
						printf("%3d.%02d  ", divquo, divrem);
					}
				} else {
					printf("   -    ");
				}
			}
			printf("\n");
		}

		printf("Last est. power                           :\t");
		for (i = 0; i < ppr_wl->rf_cores; i++) {
			if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED) {
				divquo = DIV_QUO((int8)ppr_wl->est_Pout[i], 4);
				divrem = DIV_REM((int8)ppr_wl->est_Pout[i], 4);
				neg = (divrem < 0) || (divquo < 0);
				divrem = ABS(divrem);
				divquo = ABS(divquo);
				divquo = neg ? -divquo : divquo;

				if ((divquo == 0) && neg) {
					printf(" -0.%02d  ", divrem);
				} else {
					printf("%3d.%02d  ", divquo, divrem);
				}
			} else {
				printf("   -    ");
			}
		}
		printf("\n");

		printf("Last adjusted est. power                  :\t");
		for (i = 0; i < ppr_wl->rf_cores; i++) {
			if (ppr_wl->target_offsets[i] != WL_RATE_DISABLED) {
				divquo = DIV_QUO((int8)ppr_wl->est_Pout_act[i], 4);
				divrem = DIV_REM((int8)ppr_wl->est_Pout_act[i], 4);
				neg = (divrem < 0) || (divquo < 0);
				divrem = ABS(divrem);
				divquo = ABS(divquo);
				divquo = neg ? -divquo : divquo;

				if ((divquo == 0) && neg) {
					printf(" -0.%02d  ", divrem);
				} else {
					printf("%3d.%02d  ", divquo, divrem);
				}
			} else {
				printf("   -    ");
			}
		}
		printf("\n");
		if ((curpower_flag & CURPOWER_DLOFDMA) &&
			(txctrl.txctrlwd_version == TX_CTRLWD_T_VERSION)) {
			reg_rate_index_t rate_index;
			uint nss = 0, mcs = 0;
			int8 min_tp = 127, min_rl = 127, mu_tp = 127, tmp = 127;
			wl_tx_bw_t bw;
			switch (ppr_wl->channel_bandwidth) {
				case WL_BW_20MHZ:
					bw = WL_TX_BW_20;
					break;
				case WL_BW_40MHZ:
					if (txctrl.pktbw == 0) {
						bw = WL_TX_BW_20IN40;
					} else {
						bw = WL_TX_BW_40;
					}
					break;
				case WL_BW_80MHZ:
					if (txctrl.pktbw == 0) {
						bw = WL_TX_BW_20IN80;
					} else if (txctrl.pktbw == 1) {
						bw = WL_TX_BW_40IN80;
					} else {
						bw = WL_TX_BW_80;
					}
					break;
				case WL_BW_160MHZ:
					if (txctrl.pktbw == 0) {
						bw = WL_TX_BW_20IN160;
					} else if (txctrl.pktbw == 1) {
						bw = WL_TX_BW_40IN160;
					} else if (txctrl.pktbw == 2) {
						bw = WL_TX_BW_80IN160;
					} else {
						bw = WL_TX_BW_160;
					}
					break;
			}
			wl_txctrl_print_cmn(&txctrl, TRUE);
			if ((txctrl.frame_type == WL_FRAME_TYPE_HE &&
				txctrl.he_format == WL_HE_FORMAT_MU) ||
				(txctrl.frame_type == WL_FRAME_TYPE_VHT &&
				txctrl.mu == 1)) {
				uint8 n_core;
				bool bfm_en = FALSE;
				n_core = bcm_bitcount((uint8*)&txctrl.core_mask,
						sizeof(txctrl.core_mask));
				printf("n_user			  :\t%d\n", txctrl.n_user);
				for (i = 0; i < txctrl.n_user; i++) {
					nss = (txctrl.mcs_nss_mu[i]>>4) & 0x7;
					nss = nss + 1;
					mcs = (txctrl.mcs_nss_mu[i]) & 0xf;
					bfm_en = (txctrl.bfm_mu[i]>>7) & 0x1;
					printf("   user-%d:\n", i);
					printf("	  bfm_en_mu   :\t%d\n", (uint)(bfm_en));
					printf("	   ruidx_mu   :\t%d\n",
						txctrl.ruidx_mu[i]);
					printf("	 mcs_nss_mu   :\tx%ds%d\n", mcs, nss);
					printf("	    ldpc_mu   :\t%d\n",
						(uint)(txctrl.ldpc_mu[i]));
					if (bfm_en == TRUE) {
						rate_index = get_he_reg_rate_index(mcs,
							nss, n_core-nss, EXP_MODE_ID_TXBF);
					} else {
						rate_index = get_he_reg_rate_index(mcs,
							nss, n_core-nss, EXP_MODE_ID_DEF);
					}
					tmp = wl_ppr_get_pwr(ppr_target, rate_index, bw);
					if (tmp < min_tp) {
						min_tp = tmp;
					}
					printf("	    SU_TP     :\t");
					printf("%2d.%02d\n", DIV_QUO((int8)tmp, 4),
						DIV_REM(tmp, 4));
					tmp = wl_ppr_get_pwr(ppr_reg, rate_index, bw);
					if (tmp < min_rl) {
						min_rl = tmp;
					}
					printf("	    SU_RL     :\t");
					printf("%2d.%02d\n", DIV_QUO((int8)tmp, 4),
						DIV_REM(tmp, 4));
				}
				printf("\n");
				printf("Min TP of each users	  :\t");
				printf("%2d.%02d dBm\n", DIV_QUO((int8)min_tp, 4),
					DIV_REM(min_tp, 4));
				printf("Min RL of each users	  :\t");
				printf("%2d.%02d dBm\n", DIV_QUO((int8)min_rl, 4),
					DIV_REM(min_rl, 4));
				printf("PSD back-off of Min RL	  :\t");
				printf("%2d.%02d dB\n", DIV_QUO((int8)txctrl.rl_backoff, 4),
					DIV_REM(txctrl.rl_backoff, 4));
				min_rl = min_rl - txctrl.rl_backoff - 6;
				mu_tp = (min_tp < min_rl) ? min_tp : min_rl;
				printf("HE MU TP		  :\t");
				printf("%2d.%02d dBm\n", DIV_QUO((int8)mu_tp, 4),
					DIV_REM(mu_tp, 4));
			}
		}
	} else {
		printf("Last est. power:\t%3d.%02d dBm\n",
		       DIV_QUO((int8)ppr_wl->est_Pout[0], 4),
		       DIV_REM(ppr_wl->est_Pout[0], 4));
	}

	if (!mimo && CHSPEC_IS2G(chanspec)) {
		printf("Last CCK est. power                       :\t%3d.%02d dBm\n",
		       DIV_QUO((int8)ppr_wl->est_Pout_cck, 4),
		       DIV_REM(ppr_wl->est_Pout_cck, 4));
	}
exit:
	if (ppr_board != NULL) {
		ppr_delete(NULL, ppr_board);
	}
	if (ppr_target != NULL) {
		ppr_delete(NULL, ppr_target);
	}
	if (ppr_reg != NULL) {
		ppr_delete(NULL, ppr_reg);
	}
	if (clm_he_txpwr_limit != NULL) {
		ppr_ru_delete(NULL, clm_he_txpwr_limit);
	}
	if (board_he_txpwr_limit != NULL) {
		ppr_ru_delete(NULL, board_he_txpwr_limit);
	}
	if (final_he_txpwr_limit != NULL) {
		ppr_ru_delete(NULL, final_he_txpwr_limit);
	}
	free(ppr_wl_buf);
	return err;
}

static int
wl_get_current_txctrl(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int i;
	bool is_he = FALSE;
	tx_ctrl_rpt_t txctrl;

	is_he = wl_is_he(wl);
	argv++;

	if (*argv == NULL) {
		if (is_he) {
			if ((err = wlu_get(wl, cmd->get,
				&txctrl, sizeof(tx_ctrl_rpt_t))) < 0) {
				fprintf(stderr, "Error: Curpower failed. %d ", err);
				goto exit;
			}
			if (txctrl.txctrlwd_version != TX_CTRLWD_T_VERSION) {
				printf("!! TXCTRLWD PARSING MAYBE INCORRECT !!\n");
			}
		}
	} else {
		err = BCME_UNSUPPORTED;
		goto exit;
	}
	if (is_he) {
		uint nss = 0, mcs = 0;
		wl_txctrl_print_cmn(&txctrl, FALSE);
		if (txctrl.frame_type == WL_FRAME_TYPE_HE ||
			txctrl.frame_type == WL_FRAME_TYPE_VHT) {
			bool bfm_en = FALSE;
			uint8 bfm_idx = 0;
			bool drop_ind = FALSE;
			uint8 usr_idx = 0;
			uint16 aid_idx = 0;
			bool dcm_en = FALSE;
			uint8 power_loading = 0;
			printf("\n=== PER-User TXCTL_EXT Starts Here (Total 0x%x users) ===\n\n",
			       txctrl.n_user);
			for (i = 0; i < txctrl.n_user; i++) {
				nss = (txctrl.mcs_nss_mu[i]>>4) & 0x7;
				nss = nss + 1;
				mcs = (txctrl.mcs_nss_mu[i]) & 0xf;
				bfm_en = (txctrl.bfm_mu[i]>>7) & 0x1;
				bfm_idx = (txctrl.bfm_mu[i]) & 0x7f;
				aid_idx = (txctrl.aid_mu[i]) & 0x7ff;
				dcm_en = (txctrl.aid_mu[i]>>11) & 0x1;
				power_loading = (txctrl.aid_mu[i]>>12) & 0xf;

				printf("TXCTL_EXT_%d:\n", i);
				printf("	txbfUsrIdx	:\t0x%x\n", bfm_idx);
				printf("	bfmEn		:\t%d\n", (uint)(bfm_en));
				if (txctrl.frame_type == WL_FRAME_TYPE_VHT) {
					printf("	drop_mask	:\t%x\n",
						txctrl.usridx_mu[i]);
				} else {
					drop_ind = (txctrl.usridx_mu[i]>>7) & 0x1;
					usr_idx = (txctrl.usridx_mu[i]) & 0x7f;
					printf("	user_idx	:\t0x%x\n", usr_idx);
					printf("	user_drop	:\t%d\n", (uint)drop_ind);
				}
				printf("	ru_idx		:\t0x%x\n", txctrl.ruidx_mu[i]);
				printf("	mcsNss		:\tx%ds%d\n", mcs, nss);
				printf("	ldpc		:\t%d\n",
					(uint)(txctrl.ldpc_mu[i]));
				printf("	AID		:\t%d\n", aid_idx);
				printf("	DCM		:\t%d\n", (uint)(dcm_en));
				printf("	pwr_atten	:\t0x%x\n", power_loading);
			}
		}
	}
exit:
	return err;
}

static void
wl_txctrl_print_cmn(tx_ctrl_rpt_t* txctrl, bool brief)
{
	uint nss = 0, mcs = 0, legacy_mbps = 1;
	uint n = 0;
	printf("**************TxCtrlWd Parsing**************\n");
	printf("frameType		  :\t");
	switch (txctrl->frame_type) {
		case WL_FRAME_TYPE_CCK:
			printf("0x0 (CCK)\n");
			break;
		case WL_FRAME_TYPE_11AG:
			printf("0x1 (OFDM)\n");
			break;
		case WL_FRAME_TYPE_HT:
			printf("0x2 (HT)\n");
			break;
		case WL_FRAME_TYPE_VHT:
			printf("0x3 (VHT)\n");
			break;
		case WL_FRAME_TYPE_HE:
			printf("0x4 (HE)\n");
			break;
	}
	if (txctrl->frame_type == WL_FRAME_TYPE_HE) {
		printf("heFormat		  :\t");
		switch (txctrl->he_format) {
			case WL_HE_FORMAT_SU:
				printf("0x0 (SU)\n");
				break;
			case WL_HE_FORMAT_ER:
				printf("0x1 (Range extension)\n");
				break;
			case WL_HE_FORMAT_MU:
				printf("0x2 (MU)\n");
				break;
			case WL_HE_FORMAT_TRI:
				printf("0x3 (Tigger)\n");
				break;
		}
	}
	if (brief == FALSE) {
	      printf("notSounding		  :\t0x%x\n", (uint)(txctrl->not_sounding));
	      printf("preambInfo		  :\t0x%x\n", (uint)(txctrl->preamble));
	}
	if (txctrl->frame_type == WL_FRAME_TYPE_VHT ||
		txctrl->frame_type == WL_FRAME_TYPE_HE) {
		char *he = "x";
		nss = ((txctrl->mcs_nss)>>4) & 0x7;
		nss = nss + 1;
		mcs = (txctrl->mcs_nss) & 0xf;
		if (txctrl->frame_type == WL_FRAME_TYPE_VHT)
			he = "c";
		printf("mcs+Nss			  :\t%s%ds%d\n", he, mcs, nss);
	} else if (txctrl->frame_type == WL_FRAME_TYPE_HT) {
		printf("mcs+Nss			  :\tm%d\n", txctrl->mcs_nss);
	} else if (txctrl->frame_type == WL_FRAME_TYPE_11AG) {
		switch (txctrl->mcs_nss) {
			case 11:
				legacy_mbps = 6;
				break;
			case 15:
				legacy_mbps = 9;
				break;
			case 10:
				legacy_mbps = 12;
				break;
			case 14:
				legacy_mbps = 18;
				break;
			case 9:
				legacy_mbps = 24;
				break;
			case 13:
				legacy_mbps = 36;
				break;
			case 8:
				legacy_mbps = 48;
				break;
			case 12:
				legacy_mbps = 54;
				break;
		}
		printf("mcs+Nss			  :\t%d mpbs\n", legacy_mbps);
	} else if (txctrl->frame_type == WL_FRAME_TYPE_CCK) {
		printf("mcs+Nss			  :\t%d mpbs\n", (txctrl->mcs_nss/10));
	}
	if (brief == FALSE) {
	      printf("stbc			  :\t0x%x\n", (uint)(txctrl->stbc));
	}
	if (brief == FALSE) {
		printf("pktbw			  :\t0x%x\n", txctrl->pktbw);
		printf("subband_location	  :\t0x%x\n", txctrl->subband);
		printf("partial_ofdma_subband	  :\t0x%x\n", txctrl->partial_ofdma_subband);
		printf("dynBW_present		  :\t0x%x\n", txctrl->dynBW_present);
		printf("dynBW_mode		  :\t0x%x\n", txctrl->dynBW_mode);
		printf("MU			  :\t0x%x\n", (uint)(txctrl->mu));
	}
	printf("core_mask		  :\t0x%x\n", txctrl->core_mask);
	if (brief == FALSE) {
		printf("antcfg			  :\t0x%x\n", txctrl->ant_cfg);
		printf("scrambler_init_value	  :\t0x%x\n", (txctrl->scrambler >> 1) & 0x7f);
		printf("scrambler_init_en	  :\t0x%x\n", (txctrl->scrambler >> 7) & 0x1);
		printf("CFO_comp_value		  :\t0x%x\n", txctrl->cfo_comp_val);
		printf("CFO_comp_en		  :\t0x%x\n", txctrl->cfo_comp_en);
		printf("has_trigger_info	  :\t0x%x\n", txctrl->has_trigger_info);
		printf("num_txpwr_offset	  :\t0x%x\n", txctrl->n_pwr);

		for (n = 0; n < txctrl->n_pwr; n++) {
			printf("txpwr_offset%d		  :\t0x%x\n",
			       n, txctrl->pwr_offset[0]);
		}
	}
}

/*	print a single row of the power data.
	convert data from dB to qdB;
	decide if the pwr data is 20 or 40MHz data;
	print "-" in the other channels
 */
void
wl_txpwr_print_row(const char *label, uint8 chains, txpwr_row_t powers,
	int8 unsupported_rate, int8 channel_bandwidth, bool verbose, uint32 curpower_flag)
{
	char tmp[]	  = "-      ";
	char rate2P5[]    = "-      ";
	char rate5[]      = "-       ";
	char rate10[]     = "-      ";
	char rate20[]     = "-      ";
	char rate20in40[] = "-      ";
	char rate40[]     = "-      ";
	char rate80[]     = "-      ";
	char rate20in80[] = "-      ";
	char rate40in80[] = "-      ";
	char rate160[]       = "-      ";
	char rate20in160[]   = "-      ";
	char rate40in160[]   = "-      ";
	char rate80in160[]   = "-      ";
	char rate8080[]      = "-      ";
	char rate8080chan2[] = "-      ";
	char rate20in8080[]  = "-      ";
	char rate40in8080[]  = "-      ";
	char rate80in8080[]  = "-      ";
	char ru26[]          = "-      ";
	char ru52[]          = "-      ";
	char ru106[]         = "-      ";
	char ru242[]         = "-      ";
	char ru484[]         = "-      ";
	char ru996[]         = "-      ";

	if (powers.pwr2p5 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr2p5/4);
		strncpy(rate2P5, tmp, strlen(tmp));
	}
	if (powers.pwr5 != unsupported_rate) {
		sprintf(tmp, "%2.2f ", (float)powers.pwr5/4);
		strncpy(rate5, tmp, strlen(tmp));
	}
	if (powers.pwr10 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr10/4);
		strncpy(rate10, tmp, strlen(tmp));
	}
	if (powers.pwr20 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr20/4);
		strncpy(rate20, tmp, strlen(tmp));
	}
	if (powers.pwr20in40 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr20in40/4);
		strncpy(rate20in40, tmp, strlen(tmp));
	}
	if (powers.pwr40 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr40/4);
		strncpy(rate40, tmp, strlen(tmp));
	}
	if (powers.pwr80 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr80/4);
		strncpy(rate80, tmp, strlen(tmp));
	}
	if (powers.pwr20in80 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr20in80/4);
		strncpy(rate20in80, tmp, strlen(tmp));
	}
	if (powers.pwr40in80 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr40in80/4);
		strncpy(rate40in80, tmp, strlen(tmp));
	}
	if (powers.pwr160 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr160/4);
		strncpy(rate160, tmp, strlen(tmp));
	}
	if (powers.pwr20in160 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr20in160/4);
		strncpy(rate20in160, tmp, strlen(tmp));
	}
	if (powers.pwr40in160 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr40in160/4);
		strncpy(rate40in160, tmp, strlen(tmp));
	}
	if (powers.pwr80in160 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr80in160/4);
		strncpy(rate80in160, tmp, strlen(tmp));
	}
	if (powers.pwr8080 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr8080/4);
		strncpy(rate8080, tmp, strlen(tmp));
	}
	if (powers.pwr8080chan2 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr8080chan2/4);
		strncpy(rate8080chan2, tmp, strlen(tmp));
	}
	if (powers.pwr20in8080 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr20in8080/4);
		strncpy(rate20in8080, tmp, strlen(tmp));
	}
	if (powers.pwr40in8080 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr40in8080/4);
		strncpy(rate40in8080, tmp, strlen(tmp));
	}
	if (powers.pwr80in8080 != unsupported_rate) {
		sprintf(tmp, "%2.2f", (float)powers.pwr80in8080/4);
		strncpy(rate80in8080, tmp, strlen(tmp));
	}

	if (curpower_flag & CURPOWER_ULOFDMA) {
		if (powers.pwrru26 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru26/4);
			strncpy(ru26, tmp, strlen(tmp));
		}
		if (powers.pwrru52 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru52/4);
			strncpy(ru52, tmp, strlen(tmp));
		}
		if (powers.pwrru106 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru106/4);
			strncpy(ru106, tmp, strlen(tmp));
		}
		if (powers.pwrru242 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru242/4);
			strncpy(ru242, tmp, strlen(tmp));
		}
		if (powers.pwrru484 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru484/4);
			strncpy(ru484, tmp, strlen(tmp));
		}
		if (powers.pwrru996 != unsupported_rate) {
			sprintf(tmp, "%2.2f", (float)powers.pwrru996/4);
			strncpy(ru996, tmp, strlen(tmp));
		}
	}

	printf("%-23s%d     ", label, chains);
	if (!verbose) {
		switch (channel_bandwidth) {
		case WL_BW_2P5MHZ:
			printf("%s\n", rate2P5);
			break;
		case WL_BW_5MHZ:
			printf("%s\n", rate5);
			break;
		case WL_BW_10MHZ:
			printf("%s\n", rate10);
			break;
		case WL_BW_20MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("%s%s%s%s\n",
					ru26, ru52, ru106, ru242);
			} else {
				printf("%s\n", rate20);
			}
			break;
		case WL_BW_40MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("%s%s%s%s%s\n",
					ru26, ru52, ru106, ru242, ru484);
			} else if (curpower_flag & CURPOWER_UB) {
				printf("%s\n", rate40);
			} else {
				printf("%s%s\n", rate20in40, rate40);
			}
			break;
		case WL_BW_80MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("%s%s%s%s%s%s\n",
					ru26, ru52, ru106, ru242, ru484, ru996);
			} else if (curpower_flag & CURPOWER_UB) {
				printf("%s\n", rate80);
			} else {
				printf("%s%s%s\n", rate20in80, rate40in80, rate80);
			}
			break;
		case WL_BW_160MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("%s%s%s%s%s%s\n",
					ru26, ru52, ru106, ru242, ru484, ru996);
			} else if (curpower_flag & CURPOWER_UB) {
				printf("%s\n", rate160);
			} else {
				printf("%s%s%s%s\n",
					rate20in160, rate40in160, rate80in160, rate160);
			}
			break;
		case WL_BW_8080MHZ:
			printf("%s%s%s%s%s\n", rate20in8080, rate40in8080, rate80in8080,
				rate8080, rate8080chan2);
			break;
		}
	} else {
		if (curpower_flag & CURPOWER_ULOFDMA) {
			printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
				rate2P5, rate5, rate10,
				rate20, rate20in40, rate40, rate20in80, rate40in80, rate80,
				rate20in160, rate40in160, rate80in160, rate160,
				rate20in8080, rate40in8080, rate80in8080, rate8080, rate8080chan2,
				ru26, ru52, ru106, ru242, ru484, ru996);
		} else {
			printf("%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
				rate2P5, rate5, rate10,
				rate20, rate20in40, rate40, rate20in80, rate40in80, rate80,
				rate20in160, rate40in160, rate80in160, rate160,
				rate20in8080, rate40in8080, rate80in8080, rate8080, rate8080chan2);
		}
	}

}

static void
wl_txpwr_array_row_print(ppr_t *pprptr, int8 channel_bandwidth,	reg_rate_index_t rate_index)
{
	const char *label;
	txpwr_row_t powers;
	memset(&powers, (unsigned char)WL_RATE_DISABLED, sizeof(txpwr_row_t));

	if (rate_index == NO_RATE)
	{
		printf("(NO_RATE)             -      -      -      -      "
			"-      -      -      -      "
			"-      -      -      -      -      -      -      -      "
			"-      -      -    \n");
	}
	else
	{
		clm_rate_group_id_t group_id = ppr_table[rate_index].id;
		label = ppr_table[rate_index].label;

		switch (channel_bandwidth) {
		case WL_BW_2P5MHZ:
			powers.pwr2p5    = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_2P5);
			break;

		case WL_BW_5MHZ:
			powers.pwr5      = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_5);
			break;

		case WL_BW_10MHZ:
			powers.pwr10     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_10);
			break;

		case WL_BW_20MHZ:
			powers.pwr20     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20);
			break;

		case WL_BW_40MHZ:
			powers.pwr20in40 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN40);
			powers.pwr40     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40);
			break;

		case WL_BW_80MHZ:
			powers.pwr80     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_80);
			powers.pwr20in80 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN80);
			powers.pwr40in80 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40IN80);
			break;

		case WL_BW_160MHZ:
			powers.pwr160     = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_160);
			powers.pwr20in160 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN160);
			powers.pwr40in160 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40IN160);
			powers.pwr80in160 = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_80IN160);
			break;

		case WL_BW_8080MHZ:
			powers.pwr8080      = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_8080);
			powers.pwr8080chan2 = wl_ppr_get_pwr(pprptr, rate_index,
				WL_TX_BW_8080CHAN2);
			powers.pwr20in8080  = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_20IN8080);
			powers.pwr40in8080  = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_40IN8080);
			powers.pwr80in8080  = wl_ppr_get_pwr(pprptr, rate_index, WL_TX_BW_80IN8080);
			break;
		}

		wl_txpwr_print_row(label, ppr_group_table[group_id].chain, powers,
			WL_RATE_DISABLED, channel_bandwidth, TRUE, 0);
	}
}

void
wl_txpwr_print_header(int8 channel_bandwidth, bool verbose, uint32 curpower_flag)
{
	if (!verbose)
	{
		switch (channel_bandwidth) {
		case WL_BW_2P5MHZ:
			printf("Rate                  Chains 2.5MHz\n");
			break;
		case WL_BW_5MHZ:
			printf("Rate                  Chains 5MHz\n");
			break;
		case WL_BW_10MHZ:
			printf("Rate                  Chains 10MHz\n");
			break;
		case WL_BW_20MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("Rate                  "
					"Chains RU26   RU52   RU106  RU242\n");
			} else {
				printf("Rate                  Chains 20MHz\n");
			}
			break;
		case WL_BW_40MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("Rate                  "
					"Chains RU26   RU52   RU106  RU242  RU484\n");
			} else if (curpower_flag & CURPOWER_UB) {
				printf("Rate                  Chains 40MHz\n");
			} else {
				printf("Rate                  Chains 20in40 40MHz\n");
			}
			break;
		case WL_BW_80MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("Rate                  "
					"Chains RU26   RU52   RU106  RU242  RU484  RU996\n");
			} else if (curpower_flag & CURPOWER_UB) {
				printf("Rate                  Chains 80MHz\n");
			} else {
				printf("Rate                  Chains 20in80 40in80 80MHz\n");
			}
			break;
		case WL_BW_160MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				printf("                             "
					"RU     RU     RU     RU     RU     RU\n");
				printf("Rate                  Chains "
					"26     52     106    242    484    996\n");
			} else if (curpower_flag & CURPOWER_UB) {
				printf("Rate                  Chains 160MHz\n");
			} else {
				printf("                             20in   40in   80in\n");
				printf("Rate                  Chains 160    160    160    160\n");
			}
			break;
		case WL_BW_8080MHZ:
			printf("                             20in   40in   80in   chan1  chan2\n");
			printf("Rate                  Chains 80+80  80+80  80+80  80+80  80+80\n");
			break;

		}
	} else {
		if (curpower_flag & CURPOWER_ULOFDMA) {
		printf("                                                            20in"
			"          20in   40in          20in   40in   80in          20in   40in   "
			"80in   chan1  chan2  RU     RU     RU     RU     RU     RU\n");
		printf("Rate                  Chains 2.5      5      10      20     40     40"
			"     80     80     80     160    160    160    160    80+80"
			"  80+80  80+80  80+80  80+80  26     52     106    242    484    996\n");
		} else {
		printf("                                                            20in"
			"          20in   40in          20in   40in   80in          20in   40in   "
			"80in   chan1  chan2\n");
		printf("Rate                  Chains 2.5      5      10      20     40     40"
				"     80     80     80     160    160    160    160    80+80"
				"  80+80  80+80  80+80  80+80\n");
		}
	}
}

static void
wl_txpwr_array_print(ppr_t* pprptr, ppr_ru_t* ppr_ru_ptr, int8 bw, bool verbose, bool is5G,
	uint32 hw_txchain, uint32 curpower_flag)
{
	clm_rate_group_id_t i;
	clm_rate_group_id_t start = RATE_GROUP_ID_DSSS;
	clm_rate_group_id_t end = RATE_GROUP_ID_COUNT;
	reg_rate_index_t rate_index = DSSS1;

	if (curpower_flag & CURPOWER_UB) {
		start = RATE_GROUP_ID_HE_UBSS1;
		rate_index = HEUB0SS1;
	} else if ((curpower_flag & CURPOWER_ULOFDMA) || ((curpower_flag & CURPOWER_SU))) {
		start = RATE_GROUP_ID_HESS1;
		end = RATE_GROUP_ID_HE_UBSS1;
		rate_index = HE0SS1;
	} else if (curpower_flag & CURPOWER_HE) {
		end = RATE_GROUP_ID_HE_UBSS1;
	} else {
		end = RATE_GROUP_ID_HESS1;
	}
	wl_txpwr_print_header(bw, verbose, curpower_flag);
	for (i = start; i < end; i++) {
		wl_txpwr_ppr_print(pprptr, ppr_ru_ptr, verbose, ppr_group_table[i].rate_type, i, bw,
			&rate_index, is5G, hw_txchain, curpower_flag);
		/* VHT rates are printed in three parts: MCS + VHT8_9 and VHT10_11 */
		if (ppr_group_table[i].rate_type == PPR_RATE_VHT)
			i+=2; /* Skip VHT groups because it is alread printed */
	}
}

#define SU_MAX_SUBCHAN	(5)
#define NUM_RU_TYPES	(6)
#define MAX_SUB_CHANS (SU_MAX_SUBCHAN + NUM_RU_TYPES)	/* 5 sub chans + 6 ru sizes for TBPPDU */

/* Print power values for a group of rates. If not in verbose mode and rates
 * are uniform, only one power value per channel is printed for the whole group
 */
static void
wl_txpwr_ppr_print(ppr_t* pprptr, ppr_ru_t* ppr_ru_ptr, int vb, ppr_rate_type_t type,
	clm_rate_group_id_t gid, int8 bw, reg_rate_index_t *rate_index, bool is5G,
	uint32 hw_txchain, uint32 curpower_flag)
{
	int8 *rates[MAX_SUB_CHANS] = {0}; /* Dynamic array of ratesets for each channel in bw */
	uint nchannels, rateset_sz;
	uint vht_extra_rateset_sz = WL_RATESET_SZ_VHT_MCS - WL_RATESET_SZ_HT_MCS;
	uint vht_prop_sz = WL_RATESET_SZ_VHT_MCS_P - WL_RATESET_SZ_VHT_MCS;
	uint i, j;
	const char *label;
	uint buniform;
	uint8 chains = ppr_group_table[gid].chain;

	if (curpower_flag & CURPOWER_ULOFDMA) {
		nchannels = NUM_RU_TYPES;
	} else if (curpower_flag & CURPOWER_UB) {
		/* CLM Bolb format 23.0 supports only main channel for type UB and LUB */
		nchannels = 1;
	} else {
		switch (bw) {
			case WL_BW_2P5MHZ:
			case WL_BW_5MHZ:
			case WL_BW_10MHZ:
			case WL_BW_20MHZ:
				/* BW20 */
				nchannels = 1;
				break;
			case WL_BW_40MHZ:
				/* BW20in40, BW40 */
				nchannels = 2;
				break;
			case WL_BW_80MHZ:
				/* BW20in80, BW40in80, BW80 */
				nchannels = 3;
				break;
			case WL_BW_160MHZ:
				/* BW20in160, BW40in160, BW80in160, BW160 */
				nchannels = 4;
				break;
			case WL_BW_8080MHZ:
				/* BW20in8080, BW40in8080, BW80in8080, BW8080CHAN1, BW8080CHAN2 */
				nchannels = 5;
				break;
			default:
				fprintf(stderr, "Error: Unknown bandwidth %d\n", bw);
				return;
		}
	}

	switch (type) {
		case PPR_RATE_DSSS:
			rateset_sz = sizeof(ppr_dsss_rateset_t);
			break;
		case PPR_RATE_OFDM:
			rateset_sz = sizeof(ppr_ofdm_rateset_t);
			break;
		case PPR_RATE_HT:
			rateset_sz = sizeof(ppr_ht_mcs_rateset_t);
			break;
		case PPR_RATE_VHT:
			rateset_sz = sizeof(ppr_vht_mcs_rateset_t);
			break;
		case PPR_RATE_HE:
		case PPR_RATE_HE_UB:
		case PPR_RATE_HE_LUB:
			rateset_sz = sizeof(ppr_he_mcs_rateset_t);
			break;
		default:
			fprintf(stderr, "Error: Unknown rate %d\n", type);
			return;
	}

	if (chains > hw_txchain) {
		/* Not display the power limits that chains are more than HW txchain */
		*rate_index += rateset_sz;
		return;
	}

	/* Allocate nchannel * rateset_sz array of powers */
	for (i = 0; i < nchannels; i++) {
		if ((rates[i] = (int8*)malloc(sizeof(int8) * rateset_sz)) == NULL) {
			fprintf(stderr, "Error allocating rates array\n");
			for (j = 0; j < i; j++) free(rates[j]);
			return;
		}
	}

	/* Load channel ratesets for specific type and group id into rate array */
	switch (bw) {
		case WL_BW_2P5MHZ:
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_2P5, rates[0]);
			break;
		case WL_BW_5MHZ:
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_5, rates[0]);
			break;
		case WL_BW_10MHZ:
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_10, rates[0]);
			break;
		case WL_BW_20MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_ppr_ru_get_rateset(ppr_ru_ptr, gid, &rates[0]);
			} else {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_20, rates[0]);
			}
			break;
		case WL_BW_40MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_ppr_ru_get_rateset(ppr_ru_ptr, gid, &rates[0]);
			} else if (curpower_flag & CURPOWER_UB) {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_40, rates[0]);
			} else {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_40, rates[0]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_20IN40,
					rates[1]);
			}
			break;
		case WL_BW_80MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_ppr_ru_get_rateset(ppr_ru_ptr, gid, &rates[0]);
			} else if (curpower_flag & CURPOWER_UB) {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_80, rates[0]);
			} else {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_80, rates[0]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_40IN80,
					rates[1]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_20IN80,
					rates[2]);
			}
			break;
		case WL_BW_160MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_ppr_ru_get_rateset(ppr_ru_ptr, gid, &rates[0]);
			} else if (curpower_flag & CURPOWER_UB) {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_160, rates[0]);
			} else {
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_160, rates[0]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_80IN160,
					rates[1]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_40IN160,
					rates[2]);
				wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_20IN160,
					rates[3]);
			}
			break;
		case WL_BW_8080MHZ:
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_8080, rates[0]);
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_80IN8080, rates[1]);
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_40IN8080, rates[2]);
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_20IN8080, rates[3]);
			wl_txpwr_ppr_get_rateset(pprptr, type, gid, WL_TX_BW_8080CHAN2, rates[4]);
			break;
	}

	if (type == PPR_RATE_DSSS && is5G) {
		int tx_pwr_min = ppr_get_min(pprptr, WL_RATE_DISABLED);
		for (i = 0; i < nchannels; i++) {
			for (j = 0; j < rateset_sz; j++) {
				if (rates[i][j] == tx_pwr_min)
					rates[i][j] = WL_RATE_DISABLED;
			}
		}
	}

	/* Split VHT rates into different groups. */
	if (type == PPR_RATE_VHT) {
		rateset_sz -= (sizeof(ppr_vht_mcs_rateset_t) - sizeof(ppr_ht_mcs_rateset_t));

	}

	/* Uniform group if for each channel, all rates are equal */
	buniform = !vb;
	for (i = 0; i < nchannels && buniform; i++) {
		buniform &= wl_array_check_val(rates[i], rateset_sz, rates[i][0]);
	}

	if (buniform) {
		/* Uniform, so just print first rate */
		label = get_clm_rate_group_label(gid);
		if (strcmp(label, ""))
			wl_txpwr_ppr_print_row(label, chains, bw, vb, rates, 0, curpower_flag);
	} else {
		for (i = 0; i < rateset_sz; i++) {
			label = ppr_table[*rate_index + i].label;
			if (strcmp(label, ""))
				wl_txpwr_ppr_print_row(label, chains, bw, vb, rates, i,
					curpower_flag);
		}
	}

	/* Print VHT8-9 and VHT10-11 as seperate groups */
	if (type == PPR_RATE_VHT) {
		/* VHT8-9 */
		buniform = !vb;
		for (i = 0; i < nchannels && buniform; i++) {
			int8* vht_extra_rateset = &rates[i][rateset_sz];
			buniform &= wl_array_check_val(vht_extra_rateset, vht_extra_rateset_sz,
				vht_extra_rateset[0]);
		}

		if (buniform) {
			/* Uniform, so just print first extra rate */
			label = get_clm_rate_group_label(gid+1); /* VHT8-9 label */
			if (strcmp(label, ""))
				wl_txpwr_ppr_print_row(label, chains, bw, vb, rates, rateset_sz,
					curpower_flag);
		} else {
			for (i = rateset_sz; i < (rateset_sz + vht_extra_rateset_sz); i++) {
				label = ppr_table[*rate_index + i].label;
				if (strcmp(label, ""))
					wl_txpwr_ppr_print_row(label, chains, bw, vb, rates, i,
						curpower_flag);
			}
		}

		/* VHT10-11 */
		buniform = !vb;
		for (i = 0; i < nchannels && buniform; i++) {
			int8* vht_prop_rateset = &rates[i][rateset_sz + vht_extra_rateset_sz];
			buniform &= wl_array_check_val(vht_prop_rateset, vht_prop_sz,
				vht_prop_rateset[0]);
		}

		if (buniform) {
			/* Uniform, so just print first extra rate */
			label = get_clm_rate_group_label(gid+2); /* VHT10-11 label */
			if (strcmp(label, ""))
				wl_txpwr_ppr_print_row(label, chains, bw, vb, rates,
					rateset_sz + vht_extra_rateset_sz, curpower_flag);
		} else {
			i = rateset_sz + vht_extra_rateset_sz;
			for (; i < (rateset_sz + vht_extra_rateset_sz + vht_prop_sz);
				i++) {
				label = ppr_table[*rate_index + i].label;
				if (strcmp(label, ""))
					wl_txpwr_ppr_print_row(label, chains, bw, vb, rates, i,
						curpower_flag);
			}
		}
	}

	*rate_index += rateset_sz;
	if (type == PPR_RATE_VHT) {
		*rate_index += (vht_extra_rateset_sz + vht_prop_sz);
	}

	for (i = 0; i < nchannels; i++) {
		free(rates[i]);
	}
}

static void wl_txpwr_assign_ru_pwr(txpwr_row_t *pwr, int8** rates, uint rate_index)
{
	pwr->pwrru26 = rates[0][rate_index];
	pwr->pwrru52 = rates[1][rate_index];
	pwr->pwrru106 = rates[2][rate_index];
	pwr->pwrru242 = rates[3][rate_index];
	pwr->pwrru484 = rates[4][rate_index];
	pwr->pwrru996 = rates[5][rate_index];
}

/* Print row of power values for a specific rate. */
void wl_txpwr_ppr_print_row(const char* label, int8 chains, int8 bw, bool vb,
	int8** rates, uint rate_index, uint32 curpower_flag)
{
	txpwr_row_t powers;
	memset(&powers, (unsigned char)WL_RATE_DISABLED, sizeof(txpwr_row_t));

	/* Set relevant power values based on bandwidth */
	switch (bw) {
		case WL_BW_2P5MHZ:
			powers.pwr2p5 = rates[0][rate_index];
			break;
		case WL_BW_5MHZ:
			powers.pwr5 = rates[0][rate_index];
			break;
		case WL_BW_10MHZ:
			powers.pwr10 = rates[0][rate_index];
			break;
		case WL_BW_20MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_assign_ru_pwr(&powers, &rates[0], rate_index);
			} else {
				powers.pwr20 = rates[0][rate_index];
			}
			break;
		case WL_BW_40MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_assign_ru_pwr(&powers, &rates[0], rate_index);
			} else if (curpower_flag & CURPOWER_UB) {
				powers.pwr40 = rates[0][rate_index];
			} else {
				powers.pwr40 = rates[0][rate_index];
				powers.pwr20in40 = rates[1][rate_index];
			}
			break;
		case WL_BW_80MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_assign_ru_pwr(&powers, &rates[0], rate_index);
			} else if (curpower_flag & CURPOWER_UB) {
				powers.pwr80 = rates[0][rate_index];
			} else {
				powers.pwr80 = rates[0][rate_index];
				powers.pwr40in80 = rates[1][rate_index];
				powers.pwr20in80 = rates[2][rate_index];
			}
			break;
		case WL_BW_160MHZ:
			if (curpower_flag & CURPOWER_ULOFDMA) {
				wl_txpwr_assign_ru_pwr(&powers, &rates[0], rate_index);
			} else if (curpower_flag & CURPOWER_UB) {
				powers.pwr160 = rates[0][rate_index];
			} else {
				powers.pwr160 = rates[0][rate_index];
				powers.pwr80in160 = rates[1][rate_index];
				powers.pwr40in160 = rates[2][rate_index];
				powers.pwr20in160 = rates[3][rate_index];
			}
			break;
		case WL_BW_8080MHZ:
			powers.pwr8080 = rates[0][rate_index];
			powers.pwr80in8080 = rates[1][rate_index];
			powers.pwr40in8080 = rates[2][rate_index];
			powers.pwr20in8080 = rates[3][rate_index];
			powers.pwr8080chan2 = rates[4][rate_index];
			break;
		default:
			break;
	}

	wl_txpwr_print_row(label, chains, powers, WL_RATE_DISABLED, bw, vb, curpower_flag);
}

static void wl_txpwr_ppr_ru_get_rateset(ppr_ru_t* ppr_ru_ptr, clm_rate_group_id_t gid,
	int8** rateset)
{
	wl_he_rate_type_t t;
	const ppr_group_t* group = &ppr_group_table[gid];
	int i = 0;

	for (t = WL_HE_RT_RU26; t <= WL_HE_RT_RU996; t++) {
		ppr_get_he_ru_mcs(ppr_ru_ptr, t, group->nss, group->mode, group->chain,
				(ppr_he_mcs_rateset_t*)rateset[i]);
		i++;
	}
}

/* Helper function which gets arbitrary rateset as a function of rate_type.
 * Returns rateset into a int8 array.
 */
void wl_txpwr_ppr_get_rateset(ppr_t* pprptr, ppr_rate_type_t type,
	clm_rate_group_id_t gid, wl_tx_bw_t bw, int8* rateset)
{
	const ppr_group_t* group = &ppr_group_table[gid];
	switch (type) {
		case PPR_RATE_DSSS:
			/* ASSERT(rateset_sz == sizeof(ppr_dsss_rateset_t)) */
			ppr_get_dsss(pprptr, bw, group->chain,
				(ppr_dsss_rateset_t*)rateset);
			break;
		case PPR_RATE_OFDM:
			/* ASSERT(rateset_sz == sizeof(ppr_ofdm_rateset_t)) */
			ppr_get_ofdm(pprptr, bw, group->mode, group->chain,
				(ppr_ofdm_rateset_t*)rateset);
			break;
		case PPR_RATE_HT:
			/* ASSERT(rateset_sz == sizeof(ppr_ht_mcs_rateset_t)) */
			ppr_get_ht_mcs(pprptr, bw, group->nss, group->mode, group->chain,
				(ppr_ht_mcs_rateset_t*)rateset);
			break;
		case PPR_RATE_VHT:
			/* ASSERT(rateset_sz == sizeof(ppr_vht_mcs_rateset_t)) */
			ppr_get_vht_mcs(pprptr, bw, group->nss, group->mode, group->chain,
				(ppr_vht_mcs_rateset_t*)rateset);
			break;
		case PPR_RATE_HE:
			/* ASSERT(rateset_sz == sizeof(ppr_he_mcs_rateset_t)) */
			ppr_get_he_mcs(pprptr, bw, group->nss, group->mode, group->chain,
				(ppr_he_mcs_rateset_t*)rateset, WL_HE_RT_SU);
			break;
		case PPR_RATE_HE_UB:
			ppr_get_he_mcs(pprptr, bw, group->nss, group->mode, group->chain,
				(ppr_he_mcs_rateset_t*)rateset, WL_HE_RT_UB);
			break;
		case PPR_RATE_HE_LUB:
			ppr_get_he_mcs(pprptr, bw, group->nss, group->mode, group->chain,
				(ppr_he_mcs_rateset_t*)rateset, WL_HE_RT_LUB);
			break;
		default:
			/* ASSERT(0) */
			break;
	}
}

/* helper function to check if the array are uniformly same as the input value */
static int wl_array_check_val(int8 *pwr, uint count, int8 val)
{
	uint i;
	for (i = 0; i < count; i++) {
		if (pwr[i] != val)
			return FALSE;
	}
	return TRUE;
}

/* get a power value from the opaque ppr structure */
static int8 wl_ppr_get_pwr(ppr_t* pprptr, reg_rate_index_t rate_idx, wl_tx_bw_t bw)
{
	clm_rate_group_id_t group_id = ppr_table[rate_idx].id;
	int8 power = WL_RATE_DISABLED;
	switch (ppr_group_table[group_id].rate_type) {
		case PPR_RATE_DSSS:
			{
				ppr_dsss_rateset_t rateset;
				ppr_get_dsss(pprptr, bw, ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_OFDM:
			{
				ppr_ofdm_rateset_t rateset;
				ppr_get_ofdm(pprptr, bw, ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_HT:
			{
				ppr_ht_mcs_rateset_t rateset;
				ppr_get_ht_mcs(pprptr, bw, ppr_group_table[group_id].nss,
					ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_VHT:
			{
				ppr_vht_mcs_rateset_t rateset;
				ppr_get_vht_mcs(pprptr, bw, ppr_group_table[group_id].nss,
					ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		case PPR_RATE_HE:
		case PPR_RATE_HE_UB:
		case PPR_RATE_HE_LUB:
			{
				ppr_he_mcs_rateset_t rateset;
				wl_he_rate_type_t type;

				if (ppr_group_table[group_id].rate_type == PPR_RATE_HE) {
					type = WL_HE_RT_SU;
				} else if (ppr_group_table[group_id].rate_type == PPR_RATE_HE_UB) {
					type = WL_HE_RT_UB;
				} else if (ppr_group_table[group_id].rate_type == PPR_RATE_HE_LUB) {
					type = WL_HE_RT_LUB;
				}
				ppr_get_he_mcs(pprptr, bw, ppr_group_table[group_id].nss,
					ppr_group_table[group_id].mode,
					ppr_group_table[group_id].chain, &rateset, type);
				power = rateset.pwr[rate_idx-ppr_group_table[group_id].first_rate];
			}
			break;
		default:
			break;
	}

	return power;
}
