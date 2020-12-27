/*
 * wl ltecx command module
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
 * $Id: wluc_ltecx.c 774769 2019-05-07 08:46:22Z $
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

/* LTE coex funcs */
static cmd_func_t wl_wci2_config;
static cmd_func_t wl_mws_params;
static cmd_func_t wl_mws_wci2_msg;
static cmd_func_t wl_mws_frame_config;
static cmd_func_t wl_mws_antmap;
static cmd_func_t wl_mws_oclmap;
static cmd_func_t wl_mws_scanreq_bm;

static cmd_t wl_ltecx_cmds[] = {
	{ "wci2_config", wl_wci2_config, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set LTE coex MWS signaling config\n"
	"\tUsage: wl wci2_config <rxassert_off> <rxassert_jit> <rxdeassert_off> <rxdeassert_jit> "
	"<txassert_off> <txassert_jit> <txdeassert_off> <txdeassert_jit> "
	"<patassert_off> <patassert_jit> <inactassert_off> <inactassert_jit> "
	"<scanfreqassert_off> <scanfreqassert_jit> <priassert_off_req>"},
	{ "mws_params", wl_mws_params, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set LTE coex MWS channel params\n"
	"\tUsage: wl mws_params <rx_center_freq> <tx_center_freq> "
	"<rx_channel_bw> <tx_channel_bw> <channel_en> <channel_type>"},
	{ "mws_debug_msg", wl_mws_wci2_msg, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set LTE coex BT-SIG message\n"
	"\tUsage: wl mws_debug_msg <Message> <Interval 20us-32000us> "
	"<Repeats>"},
	{ "mws_frame_config", wl_mws_frame_config, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set LTE Frame configuration\n"
	"\tUsage: wl mws_frame_config <mws_frame_dur> <mws_framesync_assert_offset>"
	"<mws_framesync_assert_jitter> <mws_num_periods>"
	"{<mws_period_dur[i]> <mws_period_type>[i]}"},
	{ "mws_antenna_selection", wl_mws_antmap, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Antenna selection params\n"
	"\tUsage: wl mws_antenna_selection <band-AntTx-combo1> <band-AntTx-combo2> "
	"<band-AntTx-combo3> <band-AntTx-combo4>\n"},
	{ "mws_ocl_override", wl_mws_oclmap, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set OCL maps\n"
	"\tUsage: wl mws_ocl_override <bitmap_2G> <bitmap_5G_lo> "
	"<bitmap_5G_mid> <bitmap_5G_high>\n"},
	{ "mws_scanreq_bm", wl_mws_scanreq_bm, WLC_GET_VAR, WLC_SET_VAR,
	"\tusage: wl mws_scanreq_bm [idx 2.4G-bitmap 5G-bitmap-lo 5G-bitmap-mid 5G-bitmap-hi]\n"
	"Set/Get the channel bitmaps corresponding to MWS (cellular) scan index <idx>"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_ltecx_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register ltecx commands */
	wl_module_cmds_register(wl_ltecx_cmds);
}

static int
wl_wci2_config(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	wci2_config_t wci2_config;
	uint16 *configp = (uint16 *)&wci2_config;
	int ret, i;

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;
	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	memset(&wci2_config, '\0', sizeof(wci2_config_t));

	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "wci2_config", &wci2_config, sizeof(wci2_config_t),
		buf, WLC_IOCTL_SMLEN);
		if (ret)
			return ret;

		printf("rxassert_off %d rxassert_jit %d rxdeassert_off %d rxdeassert_jit %d "
			"txassert_off %d txassert_jit %d txdeassert_off %d txdeassert_jit %d "
			"patassert_off %d patassert_jit %d inactassert_off %d inactassert_jit %d "
			"scanfreqassert_off %d scanfreqassert_jit %d priassert_off_req %d\n",
			dtoh16(((uint16 *)buf)[0]), dtoh16(((uint16 *)buf)[1]),
			dtoh16(((uint16 *)buf)[2]), dtoh16(((uint16 *)buf)[3]),
			dtoh16(((uint16 *)buf)[4]), dtoh16(((uint16 *)buf)[5]),
			dtoh16(((uint16 *)buf)[6]), dtoh16(((uint16 *)buf)[7]),
			dtoh16(((uint16 *)buf)[8]), dtoh16(((uint16 *)buf)[9]),
			dtoh16(((uint16 *)buf)[10]), dtoh16(((uint16 *)buf)[11]),
			dtoh16(((uint16 *)buf)[12]), dtoh16(((uint16 *)buf)[13]),
			dtoh16(((uint16 *)buf)[14]));
		return 0;
	}

	if (argc < 15)
		goto usage;

	for (i = 0; i < 15; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		configp[i] = htod16((uint16)val);
	}
	return wlu_iovar_setbuf(wl, "wci2_config", &wci2_config, sizeof(wci2_config_t),
		buf, WLC_IOCTL_SMLEN);

usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_params(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	mws_params_t mws_params;
	uint16 *paramsp = (uint16 *)&mws_params;
	int ret, i;

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	memset(&mws_params, '\0', sizeof(mws_params_t));

	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "mws_params", &mws_params, sizeof(mws_params_t),
		buf, WLC_IOCTL_SMLEN);
		if (ret)
			return ret;

		printf("rx_center_freq %d tx_center_freq %d  rx_channel_bw %d tx_channel_bw %d "
			"channel_en %d channel_type %d\n",
			dtoh16(((uint16 *)buf)[0]), dtoh16(((uint16 *)buf)[1]),
			dtoh16(((uint16 *)buf)[2]), dtoh16(((uint16 *)buf)[3]), buf[8], buf[9]);
		return 0;
	}

	if (argc < 6)
		goto usage;
	for (i = 0; i < 4; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i] = htod16((uint16)val);
	}
	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	mws_params.mws_channel_en = val;
	++i;
	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	mws_params.mws_channel_type = val;

	return wlu_iovar_setbuf(wl, "mws_params", &mws_params, sizeof(mws_params_t),
		buf, WLC_IOCTL_SMLEN);

usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_wci2_msg(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	mws_wci2_msg_t mws_wci2_msg;
	uint16 *paramsp = (uint16 *)&mws_wci2_msg;
	int ret, i = 0;

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	memset(&mws_wci2_msg, '\0', sizeof(mws_wci2_msg_t));

	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "mws_debug_msg", &mws_wci2_msg, sizeof(mws_wci2_msg_t),
		buf, WLC_IOCTL_SMLEN);
		if (ret)
			return ret;

		printf("Message %d Interval %d  Repeats %d \n",
			dtoh16(((uint16 *)buf)[0]), dtoh16(((uint16 *)buf)[1]),
			dtoh16(((uint16 *)buf)[2]));
		return 0;
	}

	if (argc < 3)
		goto usage;

	for (i = 0; i < 3; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i] = htod16((uint16)val);
	}
	if ((paramsp[1] < 20) || (paramsp[1] > 32000))
		goto usage;
	return wlu_iovar_setbuf(wl, "mws_debug_msg", &mws_wci2_msg, sizeof(mws_wci2_msg_t),
		buf, WLC_IOCTL_SMLEN);

usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_frame_config(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	mws_frame_config_t mws_frame_config;
	uint16 *paramsp = (uint16 *)&mws_frame_config;
	int ret, j = 3, k = 4, l = 20;
	uint8 i = 0;

	UNUSED_PARAMETER(cmd);

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	memset(&mws_frame_config, '\0', sizeof(mws_frame_config_t));

	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "mws_frame_config", &mws_frame_config,
		sizeof(mws_frame_config_t), buf, WLC_IOCTL_SMLEN);
		if (ret) {
			return ret;
		}
		printf("mws_frame_dur %d us mws_framesync_assert_offset %d "
		"mws_framesync_assert_jitter %d mws_num_periods %d \n",
		dtoh16(((uint16 *)buf)[0]), dtoh16(((uint16 *)buf)[1]),
		dtoh16(((uint16 *)buf)[2]), buf[27]);
		for (i = 0; i < buf[27]; i++) {
			printf("mws_period_dur[%d] %d us mws_period_type[%d] %d\n",
			i, dtoh16(((uint16 *)buf)[j]), i, buf[l]);
			j++;
			l++;
		}
		return 0;
	}

	if (argc <= 4)
		goto usage;
	/* Loop to populate the first three elements of the structure */
	for (i = 0; i < 3; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i] = htod16((uint16)val);
	}

	val = strtoul(argv[3], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		mws_frame_config.mws_num_periods = val;

	/* Error check for insufficient data entered */
	if ((uint8)argc != ((mws_frame_config.mws_num_periods * 2) + 4)) {
		goto usage;
	}
	/* Loop to populate the arrays of size equal to mws_frame_config.mws_num_periods */
	for (i = 0; i < mws_frame_config.mws_num_periods; i++)
	{
		val = strtoul(argv[k], &endptr, 0);
		if (*endptr != '\0') {
			goto usage;
		}
		mws_frame_config.mws_period_dur[i] = val;
		k++;

		val = strtoul(argv[k], &endptr, 0);
		if (*endptr != '\0') {
			goto usage;
		}
		mws_frame_config.mws_period_type[i] = val;
		k++;
	}
	/* Set the values */
	return wlu_iovar_setbuf(wl, "mws_frame_config", &mws_frame_config,
		sizeof(mws_frame_config_t), buf, WLC_IOCTL_SMLEN);

	usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_antmap(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	mws_ant_map_t mws_antmap;
	uint16 *paramsp = (uint16 *)&mws_antmap;
	int ret, i = 0, j = 0;
	uint16 error_mask = 0xc000;
	uint16 ant_set[4];
	UNUSED_PARAMETER(cmd);
	val = 0;
	/* eat command name */
	argv++;
	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	memset(&mws_antmap, '\0', sizeof(mws_antmap));
	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "mws_antenna_selection",
			&mws_antmap, sizeof(mws_antmap),
			buf, WLC_IOCTL_SMLEN);
		if (ret)
			return ret;
		printf("band_ant_combo1 %x band_ant_combo2 %x "
			"band_ant_combo3 %x band_ant_combo4 %x\n",
			dtoh16(((uint16 *)buf)[0]), dtoh16(((uint16 *)buf)[1]),
			dtoh16(((uint16 *)buf)[2]), dtoh16(((uint16 *)buf)[3]));
		return 0;
	}
	if (argc < 4)
		goto usage;
	for (i = 0; i < 4; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i] = htod16((uint16)val);
		ant_set[i] = (paramsp[i] & error_mask) >> 14;
		for (j = 0; j < i; ++j) {
			if (ant_set[i] == ant_set[j]) {
				printf("duplicate band ant_tx combiniation\n");
				goto usage;
			}
		}
	}
	return wlu_iovar_setbuf(wl, "mws_antenna_selection", paramsp, sizeof(mws_antmap),
		buf, WLC_IOCTL_SMLEN);
usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_oclmap(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *endptr = NULL;
	uint argc;
	wl_mws_ocl_override_t mws_oclmap;
	uint16 *paramsp = (uint16 *)&mws_oclmap;
	int ret, i = 0;
	UNUSED_PARAMETER(cmd);
	val = 0;
	/* eat command name */
	argv++;
	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	memset(&mws_oclmap, '\0', sizeof(mws_oclmap));
	if (argc == 0) {
		/* Get and print the values */
		ret = wlu_iovar_getbuf(wl, "mws_ocl_override",
			&mws_oclmap, sizeof(mws_oclmap),
			buf, WLC_IOCTL_SMLEN);
		if (ret)
			return ret;
		printf(" bitmap_2g %x bitmap_5g_lo %x "
			"babitmap_5g_mid %x bitmap_5g_high %x\n",
			dtoh16(((uint16 *)buf)[1]), dtoh16(((uint16 *)buf)[2]),
			dtoh16(((uint16 *)buf)[3]), dtoh16(((uint16 *)buf)[4]));
		return 0;
	}
	if (argc < 4)
		goto usage;
	paramsp[0] = WL_MWS_OCL_OVERRIDE_VERSION;
	for (i = 0; i < 4; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i+1] = htod16((uint16)val);
	}
	return wlu_iovar_setbuf(wl, "mws_ocl_override", paramsp, sizeof(mws_oclmap),
		buf, WLC_IOCTL_SMLEN);
usage:
	return BCME_USAGE_ERROR;
}

static int
wl_mws_scanreq_bm(void *wl, cmd_t *cmd, char **argv)
{
	mws_scanreq_params_t mws_scanreq_params;
	uint16 *paramsp = (uint16 *)&mws_scanreq_params;
	char *endptr = NULL;
	uint32 val;
	uint argc, i;
	int err;

	UNUSED_PARAMETER(cmd);

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	memset(&mws_scanreq_params, 0, sizeof(mws_scanreq_params_t));

	if (argc == 0) {
		goto usage;
	} else if (argc == 1) {
		val = strtoul(argv[0], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		mws_scanreq_params.idx = htod16((uint16)val);

		if ((mws_scanreq_params.idx < 1) || (mws_scanreq_params.idx > 31)) {
			printf("LTE Index should be in range 1-31\n");
			goto usage;
		}

		err = wlu_iovar_getbuf(wl, cmd->name, &mws_scanreq_params,
			sizeof(mws_scanreq_params_t), buf, WLC_IOCTL_SMLEN);
		if (err < 0)
			return err;
		mws_scanreq_params = *(mws_scanreq_params_t *)buf;
		printf("\n Current LTE Timesharing coex : \n"
		"\t LTE channel index = %d\n \t 2G Channel = %d \n\t 5G low channel= %d\n"
		"\t 5G Middle channel= %d\n\t 5G High channel= %d\n",
		mws_scanreq_params.idx, mws_scanreq_params.bm_2g, mws_scanreq_params.bm_5g_lo,
		mws_scanreq_params.bm_5g_mid, mws_scanreq_params.bm_5g_hi);
		return 0;
	} else if (argc > 1 && argc < 5) {
		goto usage;
	}
	memset(&mws_scanreq_params, 0, sizeof(mws_scanreq_params_t));
	for (i = 0; i < 5; ++i) {
		val = strtoul(argv[i], &endptr, 0);
		if (*endptr != '\0')
			goto usage;
		paramsp[i] = htod16((uint16)val);
	}

	return wlu_iovar_setbuf(wl, "mws_scanreq_bm", &mws_scanreq_params,
		sizeof(mws_scanreq_params_t), buf, WLC_IOCTL_SMLEN);
usage:
	return BCME_USAGE_ERROR;
}
