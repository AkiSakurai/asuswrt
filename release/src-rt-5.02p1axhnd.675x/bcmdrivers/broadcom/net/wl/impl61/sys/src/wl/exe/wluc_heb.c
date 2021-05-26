/*
 * wl heb command module
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
 * $Id $
 */

#include <wlioctl.h>

#include <bcmutils.h>
#include <bcmendian.h>

#include <802.11ah.h>

#include <miniopt.h>

#include "wlu_common.h"
#include "wlu.h"

#ifndef bzero
#define bzero(mem, len)	memset(mem, 0, len)
#endif // endif

#define HEB_CMD_HELP_STR \
	"HARDWARE EVENT BLOCK (HEB) control commands\n\n" \
	"\tUsage: wl heb [command] [cmd options]\n\n" \
	"Available commands and command options:\n" \
	"\twl heb enab - query whether HEB is enabled or not\n" \
	"\twl heb num - Displays the number of HEBs present\n" \
	"\twl heb counters - Displays the accumulated Interrupt counters\n" \
	"\t\t of all HEBs \n" \
	"\twl heb clear_counters - Clears the accumulated Interrupt counters\n" \
	"\twl heb config -i <index> -c <count> -d <duration> -p <periodicity> -a <advance>\n" \
	"\t\t-i: HEB Index, Valid range 0-15\n" \
	"\t\t-c: Event Count, 0 - disable, 255 - infinite, valid range 1-254\n" \
	"\t\t-d: Event Duration, Duration In Microseconds\n" \
	"\t\t-p: Event Periodicity, Interval In Microseconds\n" \
	"\t\t-a: Event Advance, Pre-event time in Microseconds\n" \
	"\twl heb status -i <index>\n" \
	"\tPrints the status of the HEB index requested. If no HEB index is given,\n" \
	"\tprints status of all HEBs\n" \
	"\t\t-i: HEB Index, Valid range 0-15\n" \

static cmd_func_t wl_heb_cmd;

/* wl heb top level command list */
static cmd_t wl_heb_cmds[] = {
	{ "heb", wl_heb_cmd, WLC_GET_VAR, WLC_SET_VAR, HEB_CMD_HELP_STR },
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_heb_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register HE commands */
	wl_module_cmds_register(wl_heb_cmds);
}

/* HEB sub cmd */
typedef struct sub_cmd sub_cmd_t;
typedef int (sub_cmd_func_t)(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv);
struct sub_cmd {
	char *name;
	uint16 id;		/* id for the dongle f/w switch/case  */
	uint16 type;		/* base type of argument IOVT_XXXX */
	sub_cmd_func_t *hdlr;	/* cmd handler  */
};

/*  HEB ioctl sub cmd handler functions  */
static sub_cmd_func_t wl_heb_cmd_enab;
static sub_cmd_func_t wl_heb_cmd_num_heb;
static sub_cmd_func_t wl_heb_cmd_counters;
static sub_cmd_func_t wl_heb_cmd_clear_counters;
static sub_cmd_func_t wl_heb_cmd_config;
static sub_cmd_func_t wl_heb_cmd_status;

/* wl he sub cmd list */
static const sub_cmd_t heb_cmd_list[] = {
	/* wl heb enab */
	{"enab", WL_HEB_CMD_ENAB, IOVT_UINT8, wl_heb_cmd_enab},
	/* wl heb num */
	{"num", WL_HEB_CMD_NUM_HEB, IOVT_UINT8, wl_heb_cmd_num_heb},
	/* wl heb counters */
	{"counters", WL_HEB_CMD_COUNTERS, IOVT_BUFFER, wl_heb_cmd_counters},
	/* wl heb clear_counters */
	{"clear_counters", WL_HEB_CMD_CLEAR_COUNTERS, IOVT_VOID, wl_heb_cmd_clear_counters},
	/* wl heb config <params> */
	{"config", WL_HEB_CMD_CONFIG, IOVT_BUFFER, wl_heb_cmd_config},
	/* wl heb status */
	{"status", WL_HEB_CMD_STATUS, IOVT_UINT32, wl_heb_cmd_status},
	{NULL, 0, 0, NULL}
};

/* wl heb command */
static int
wl_heb_cmd(void *wl, cmd_t *cmd, char **argv)
{
	const sub_cmd_t *sub = heb_cmd_list;

	int ret = BCME_USAGE_ERROR;

	/* skip to cmd name after "heb" */
	argv++;

	if (!*argv) {
		return BCME_USAGE_ERROR;
	}
	else if (!strcmp(*argv, "-h") || !strcmp(*argv, "help"))  {
		/* help , or -h* */
		return BCME_USAGE_ERROR;
	}

	while (sub->name != NULL) {
		if (strcmp(sub->name, *argv) == 0)  {
			/* dispatch subcmd to the handler */
			if (sub->hdlr != NULL) {
				ret = sub->hdlr(wl, cmd, sub, ++argv);
			}
			return ret;
		}
		sub ++;
	}

	return BCME_IOCTL_ERROR;
}

static int
wl_heb_cmd_enab(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;

	/* get */
	if (*argv == NULL) {
		bcm_xtlv_t mybuf;
		uint8 *resp;

		mybuf.id = sub->id;
		mybuf.len = 0;

		/*	send getbuf iovar */
		res = wlu_var_getbuf_sm(wl, cmd->name, &mybuf, sizeof(mybuf),
			(void **)&resp);

		/*	check the response buff  */
		if (res == BCME_OK && resp != NULL) {
			uint32 v32;
			v32 = *resp;
			v32 = dtoh32(v32);
			printf("%u\n", v32);
		}
		else {
			goto fail;
		}
	}
	/* set */
	else {
		/* unsupported */
		return BCME_USAGE_ERROR;
	}

	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_heb_cmd_num_heb(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;

	/* get */
	if (*argv == NULL) {
		bcm_xtlv_t mybuf;
		uint8 *resp;

		mybuf.id = sub->id;
		mybuf.len = 0;

		/*	send getbuf iovar */
		res = wlu_var_getbuf_sm(wl, cmd->name, &mybuf, sizeof(mybuf),
			(void **)&resp);

		/*	check the response buff  */
		if (res == BCME_OK && resp != NULL) {
			uint32 v32;
			v32 = *resp;
			v32 = dtoh32(v32);
			printf("%u\n", v32);
		}
		else {
			goto fail;
		}
	}
	/* set */
	else {
		return BCME_USAGE_ERROR;
	}
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;

}

static int
wl_heb_cmd_counters(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	wl_heb_cnt_v1_t *resp;
	int16 bytesremaining;
	uint16 heb_idx = 0;

	/* get */
	if (*argv == NULL) {
		bcm_xtlv_t mybuf;
		uint8 *ptr;

		mybuf.id = sub->id;
		mybuf.len = 0;

		/*	send getbuf iovar */
		res = wlu_var_getbuf_med(wl, cmd->name, &mybuf, sizeof(mybuf),
			(void **)&ptr);

		/*	check the response buff  */
		if (res == BCME_OK && ptr != NULL) {
			resp = (wl_heb_cnt_v1_t *)ptr;
			if (resp->version == WL_HEB_VER_1) {
				bytesremaining = dtoh16(resp->length);
				while (bytesremaining > 0) {
					printf("HEB Interrupt counters for HEB index %d\n",
						heb_idx);
					printf("Pre-Event Interrupt %d\t",
						dtoh16(resp->heb_int_cnt[heb_idx].pre_event));
					printf("Start-Event Interrupt %d\t",
						dtoh16(resp->heb_int_cnt[heb_idx].start_event));
					printf("End-Event Interrupt %d\t",
						dtoh16(resp->heb_int_cnt[heb_idx].end_event));
					printf("Missed Event Counter %d\n",
						dtoh16(resp->heb_int_cnt[heb_idx].missed));
					bytesremaining -= sizeof(resp->heb_int_cnt[heb_idx]);
					heb_idx++;
				}
			} else {
				res = BCME_VERSION;
				goto fail;
			}
		}
		else {
			goto fail;
		}
	}
	/* set */
	else {
		return BCME_USAGE_ERROR;
	}
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_heb_cmd_clear_counters(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;

	uint8 mybuf[32];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	UNUSED_PARAMETER(argv);

	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		0, NULL, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}

	res = wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_heb_cmd_config(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_config_heb_fill_v1_t val;
	miniopt_t opt;
	int opt_err;
	int argc;

	if (*argv == NULL) {
		return BCME_OK;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	bzero(&val, sizeof(val));

	val.version = htod16(WL_HEB_VER_1);
	val.length = htod16(sizeof(val.version) + sizeof(val.length));
	/* default values */
	/* Configure HEB index 0 */
	val.heb_idx = htod16(0);
	/* Configure for infinite events */
	val.count = 0xff;
	/* Assign a duration of 1msec */
	val.duration = htod32(1000);
	/* Periodicity of 100msec */
	val.periodicity = htod32(100000);

	miniopt_init(&opt, __FUNCTION__, NULL, FALSE);

	while ((opt_err = miniopt(&opt, argv)) != -1) {
		if (opt_err == 1) {
			res = BCME_USAGE_ERROR;
			goto fail;
		}
		if (opt.opt == 'i') {
			val.heb_idx = htod16((uint16)strtoul(opt.valstr, NULL, 0));
		}
		else if (opt.opt == 'c') {
			val.count = htod16((uint16)strtoul(opt.valstr, NULL, 0));
		}
		else if (opt.opt == 'd') {
			val.duration = htod32((uint32)strtoul(opt.valstr, NULL, 0));
		}
		else if (opt.opt == 'p') {
			val.periodicity = htod32((uint32)strtoul(opt.valstr, NULL, 0));
		}
		else if (opt.opt == 'a') {
			val.preeventtime = htod16((uint16)strtoul(opt.valstr, NULL, 0));
		}
		else {
			fprintf(stderr, "Unrecognized option '%s'\n", *argv);
			res = BCME_BADARG;
			goto fail;
		}
		argv += opt.consumed;
		argc -= opt.consumed;
	}
	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}
	return wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);
	return BCME_OK;
fail:
	fprintf(stderr, "error:%d\n", res);
	return res;

}

static int
wl_heb_cmd_status(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[16];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	uint16 status_idx;
	wl_heb_status_v1_t* resp;
	uint8 *ptr;
	uint16 count = 0;
	int16 bytesremaining;

	if (*argv == NULL) {
		/* Get status of all HEBs */
		status_idx = 0xffff;
	} else {
		char *s = *argv;
		if (!strcmp(s, "-i")) {
			status_idx = atoi(argv[1]);
			status_idx = htod16(status_idx);
		} else {
			fprintf(stderr, "Unrecognized option '%s'\n", *argv);
			res = BCME_BADARG;
			goto fail;
		}
	}

	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		sizeof(uint16), (uint8*)&status_idx, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}
	res = wlu_var_getbuf_med(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len, (void **)&ptr);

	if (res == BCME_OK && ptr != NULL) {
		resp = (wl_heb_status_v1_t *)ptr;
		if (resp->version == WL_HEB_VER_1) {
			bytesremaining = dtoh16(resp->length);
			while (bytesremaining > 0)
			{
				printf("Status of HEB Index %d\n",
					dtoh32(resp->heb_status[count].heb_idx));
				printf("Event Init Val L %x, Event Init Val H %x, \n",
					dtoh32(resp->heb_status[count].blk_params.event_int_val_l),
					dtoh32(resp->heb_status[count].blk_params.event_int_val_l));
				printf("Param2 %d, Param3 %d, \n",
					dtoh32(resp->heb_status[count].blk_params.param2),
					dtoh32(resp->heb_status[count].blk_params.param3));
				printf("Intr Mask BMP Pre-Event %x, Start-Event %x, End-Event %x\n",
					dtoh32(resp->heb_status[count].blk_params.
						pre_event_intmsk_bmp),
						dtoh32(resp->heb_status[count].blk_params.
						start_event_intmsk_bmp),
						dtoh32(resp->heb_status[count].blk_params.
						end_event_intmsk_bmp));
				printf("Event Driver Info %x, Param1 %d, \n",
					dtoh32(resp->heb_status[count].blk_params.
						event_driver_info),
						dtoh16(resp->heb_status[count].blk_params.param1));
				printf("Event Count %x, NOA Invert %d, \n",
					(uint8)(resp->heb_status[count].blk_params.event_count),
					(uint8)(resp->heb_status[count].blk_params.noa_invert));
				count++;
				bytesremaining -= sizeof(resp->heb_status);
			}
		} else {
			res = BCME_VERSION;
			goto fail;
		}
	}

	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}
