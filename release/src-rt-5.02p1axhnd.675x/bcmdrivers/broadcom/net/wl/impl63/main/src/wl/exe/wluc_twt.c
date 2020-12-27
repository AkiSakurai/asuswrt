/*
 * wl twt command module
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
 * $Id: wluc_twt.c 777067 2019-07-18 09:28:30Z $
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

#define TWT_CMD_HELP_STR \
	"TWT protocol control commands\n\n" \
	"\tUsage: wl twt [command] [cmd options]\n\n" \
	"Available commands and command options:\n" \
	"\twl twt enab [0|1] - query or enable/disable TWT feature\n" \
	"\twl twt setup [<flow flags>] [<options>] - "\
	"setup target wake time (TWT)\n" \
	"\t\t<flow flags>:\n" \
	"\t\t\t-b - Broadcast TWT\n" \
	"\t\t\t-i - Implicit TWT\n" \
	"\t\t\t-u - Unannounced\n" \
	"\t\t\t-t - Trigger\n" \
	"\t\t\t-r - Protection\n" \
	"\t\t<options>:\n" \
	"\t\t\t-a <peer MAC address>\n" \
	"\t\t\t-c <request|suggest|demand> - default <request>\n" \
	"\t\t\t-d <wake duration> - 256us unit\n" \
	"\t\t\t-p (wake interval) <mantissa> <exponent>\n" \
	"\t\t\t-I <(broadcast/individual) id>\n" \
	"\twl twt teardown [<flow flags>] [<options>] <id> - teardown flow\n" \
	"\t\t<flow flags>:\n" \
	"\t\t\t-b - Broadcast TWT\n" \
	"\t\t<options>:\n" \
	"\t\t\t-a <peer MAC address>\n" \
	"\twl twt info [<flow flags>] [<options>] <flow id> - request information\n" \
	"\t\t<flow flags>:\n" \
	"\t\t\t-r - response request\n" \
	"\t\t<options>:\n" \
	"\t\t\t-a <peer MAC address>\n" \
	"\t\t\t-w <target wake time>\n" \
	"\twl twt list [<options>] - list all twt info (bcast & indvidual)\n" \
	"\t\t<options>:\n" \
	"\t\t\t-a <peer MAC address>\n"

static cmd_func_t wl_twt_cmd;

/* wl twt top level command list */
static cmd_t wl_twt_cmds[] = {
	{ "twt", wl_twt_cmd, WLC_GET_VAR, WLC_SET_VAR, TWT_CMD_HELP_STR },
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_twt_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register TWT commands */
	wl_module_cmds_register(wl_twt_cmds);
}

/* TWT sub cmd */
typedef struct sub_cmd sub_cmd_t;
typedef int (sub_cmd_func_t)(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv);
struct sub_cmd {
	char *name;
	uint16 id;		/* id for the dongle f/w switch/case  */
	uint16 type;		/* base type of argument IOVT_XXXX */
	sub_cmd_func_t *hdlr;	/* cmd handler  */
};

/*  TWT ioctl sub cmd handler functions  */
static sub_cmd_func_t wl_twt_cmd_uint;
static sub_cmd_func_t wl_twt_cmd_setup;
static sub_cmd_func_t wl_twt_cmd_teardown;
static sub_cmd_func_t wl_twt_cmd_info;
static sub_cmd_func_t wl_twt_cmd_list;

/* wl twt sub cmd list */
static const sub_cmd_t twt_cmd_list[] = {
	/* wl twt enab [0|1] */
	{"enab", WL_TWT_CMD_ENAB, IOVT_UINT8, wl_twt_cmd_uint},
	/* wl twt setup ... */
	{"setup", WL_TWT_CMD_SETUP, IOVT_BUFFER, wl_twt_cmd_setup},
	/* wl twt teardown ... */
	{"teardown", WL_TWT_CMD_TEARDOWN, IOVT_BUFFER, wl_twt_cmd_teardown},
	/* wl twt info ... */
	{"info", WL_TWT_CMD_INFO, IOVT_BUFFER, wl_twt_cmd_info},
	/* wl twt list ... */
	{"list", WL_TWT_CMD_LIST, IOVT_BUFFER, wl_twt_cmd_list},
	{NULL, 0, 0, NULL}
};

#ifdef NEED_STRTOULL
static unsigned long long int
strtoull(const char *nptr, char **endptr, int base)
{
	unsigned long long int result;
	unsigned char value;
	bool minus;

	minus = FALSE;

	while (bcm_isspace(*nptr)) {
		nptr++;
	}

	if (nptr[0] == '+') {
		nptr++;
	}
	else if (nptr[0] == '-') {
		minus = TRUE;
		nptr++;
	}

	if (base == 0) {
		if (nptr[0] == '0') {
			if ((nptr[1] == 'x') || (nptr[1] == 'X')) {
				base = 16;
				nptr = &nptr[2];
			} else {
				base = 8;
				nptr = &nptr[1];
			}
		} else {
			base = 10;
		}
	} else if (base == 16 &&
	           (nptr[0] == '0') && ((nptr[1] == 'x') || (nptr[1] == 'X'))) {
		nptr = &nptr[2];
	}

	result = 0;

	while (bcm_isxdigit(*nptr) &&
	       (value = bcm_isdigit(*nptr) ? *nptr - '0' : bcm_toupper(*nptr) - 'A' + 10) <
	       (unsigned char)base) {
		/* TODO: The strtoul() function should only convert the initial part
		 * of the string in nptr to an unsigned long int value according to
		 * the given base...so strtoull() should follow the same rule...
		 */
		result = result * base + value;
		nptr++;
	}

	if (minus) {
		result = result * -1;
	}

	if (endptr) {
		*endptr = DISCARD_QUAL(nptr, char);
	}

	return result;
}
#endif /* NEED_STRTOULL */

/* wl twt command */
static int
wl_twt_cmd(void *wl, cmd_t *cmd, char **argv)
{
	const sub_cmd_t *sub = twt_cmd_list;
	char *twt_query[2] = {"enab", NULL};
	char *twt_en[3] = {"enab", "1", NULL};
	char *twt_dis[3] = {"enab", "0", NULL};
	int ret = BCME_USAGE_ERROR;

	/* skip to cmd name after "he" */
	argv++;

	if (!*argv) {
		/* query twt "enab" state */
		argv = twt_query;
	}
	else if (*argv[0] == '1') {
		argv = twt_en;
	}
	else if (*argv[0] == '0') {
		argv = twt_dis;
	}
	else if (!strcmp(*argv, "-h") || !strcmp(*argv, "help") ||
		!strcmp(*argv, "-help") || !strcmp(*argv, "--help")) {
		/* help , or -h* */
		return BCME_USAGE_ERROR;
	}

	while (sub->name != NULL) {
		if (strcmp(sub->name, *argv) == 0)  {
			/* dispacth subcmd to the handler */
			if (sub->hdlr != NULL) {
				ret = sub->hdlr(wl, cmd, sub, ++argv);
			}
			return ret;
		}
		sub ++;
	}

	return BCME_IOCTL_ERROR;
}

typedef struct {
	uint16 id;
	uint16 len;
	uint32 val;
} twt_xtlv_v32;

static uint
wl_twt_iovt2len(uint iovt)
{
	switch (iovt) {
	case IOVT_BOOL:
	case IOVT_INT8:
	case IOVT_UINT8:
		return sizeof(uint8);
	case IOVT_INT16:
	case IOVT_UINT16:
		return sizeof(uint16);
	case IOVT_INT32:
	case IOVT_UINT32:
		return sizeof(uint32);
	default:
		/* ASSERT(0); */
		return 0;
	}
}

static bool
wl_twt_get_uint_cb(void *ctx, uint16 *id, uint16 *len)
{
	twt_xtlv_v32 *v32 = ctx;

	*id = v32->id;
	*len = v32->len;

	return FALSE;
}

static void
wl_twt_pack_uint_cb(void *ctx, uint16 id, uint16 len, uint8 *buf)
{
	twt_xtlv_v32 *v32 = ctx;

	BCM_REFERENCE(id);
	BCM_REFERENCE(len);

	v32->val = htod32(v32->val);

	switch (v32->len) {
	case sizeof(uint8):
		*buf = (uint8)v32->val;
		break;
	case sizeof(uint16):
		store16_ua(buf, (uint16)v32->val);
		break;
	case sizeof(uint32):
		store32_ua(buf, v32->val);
		break;
	default:
		/* ASSERT(0); */
		break;
	}
}

/*  ******** generic uint8/uint16/uint32   ******** */
static int
wl_twt_cmd_uint(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;

	/* get */
	if (*argv == NULL) {
		bcm_xtlv_t mybuf;
		uint8 *resp;

		mybuf.id = sub->id;
		mybuf.len = 0;

		/*  send getbuf iovar */
		res = wlu_var_getbuf_sm(wl, cmd->name, &mybuf, sizeof(mybuf),
			(void **)&resp);

		/*  check the response buff  */
		if (res == BCME_OK && resp != NULL) {
			uint len = wl_twt_iovt2len(sub->type);
			uint32 v32;

			switch (len) {
			case sizeof(uint8):
				v32 = *resp;
				break;
			case sizeof(uint16):
				v32 = load16_ua(resp);
				break;
			case sizeof(uint32):
				v32 = load32_ua(resp);
				break;
			default:
				v32 = ~0;
				break;
			}
			v32 = dtoh32(v32);
			printf("%u\n", v32);
		}
	}
	/* set */
	else {
		uint8 mybuf[32];
		int mybuf_len = sizeof(mybuf);
		twt_xtlv_v32 v32;

		v32.id = sub->id;
		v32.len = wl_twt_iovt2len(sub->type);
		v32.val = atoi(*argv);

		bzero(mybuf, sizeof(mybuf));
		res = bcm_pack_xtlv_buf((void *)&v32, mybuf, sizeof(mybuf),
			BCM_XTLV_OPTION_ALIGN32, wl_twt_get_uint_cb, wl_twt_pack_uint_cb,
			&mybuf_len);
		if (res != BCME_OK) {
			goto fail;
		}

		res = wlu_var_setbuf(wl, cmd->name, mybuf, mybuf_len);
	}

	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

/* wl twt setup command */
static int
wl_twt_cmd_setup(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_setup_t val;
	miniopt_t opt;
	int opt_err;
	int argc;

	BCM_REFERENCE(wl);

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	bzero(&val, sizeof(val));

	val.version = WL_TWT_SETUP_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));
	val.desc.setup_command = WL_TWT_SETUP_CMD_REQUEST;

	miniopt_init(&opt, __FUNCTION__, "biutr", FALSE);

	while ((opt_err = miniopt(&opt, argv)) != -1) {

		if (opt_err == 1) {
			res = BCME_USAGE_ERROR;
			goto fail;
		}

		/* flags and options */
		if (!opt.positional) {

			/* flags */

			/* -b (broadcast) */
			if (opt.opt == 'b') {
				val.desc.flow_flags |= WL_TWT_FLOW_FLAG_BROADCAST;
			}
			/* -i (implicit) */
			else if (opt.opt == 'i') {
				val.desc.flow_flags |= WL_TWT_FLOW_FLAG_IMPLICIT;
			}
			/* -u (unannounced) */
			else if (opt.opt == 'u') {
				val.desc.flow_flags |= WL_TWT_FLOW_FLAG_UNANNOUNCED;
			}
			/* -t (trigger) */
			else if (opt.opt == 't') {
				val.desc.flow_flags |= WL_TWT_FLOW_FLAG_TRIGGER;
			}
			/* -r (protection) */
			else if (opt.opt == 'r') {
				val.desc.flow_flags |= WL_TWT_FLOW_FLAG_PROTECTION;
			}

			/* options */

			/* -a peer_address */
			else if (opt.opt == 'a') {
				if (!wl_ether_atoe(opt.valstr, &val.peer)) {
					fprintf(stderr, "Malformed TWT peer address '%s'\n",
						opt.valstr);
					res = BCME_BADARG;
					goto fail;
				}
			}
			/* -c setup_command */
			else if (opt.opt == 'c') {
				if (!strcmp(opt.valstr, "request")) {
					val.desc.setup_command = WL_TWT_SETUP_CMD_REQUEST;
				} else if (!strcmp(opt.valstr, "suggest")) {
					val.desc.setup_command = WL_TWT_SETUP_CMD_SUGGEST;
				} else if (!strcmp(opt.valstr, "demand")) {
					val.desc.setup_command = WL_TWT_SETUP_CMD_DEMAND;
				} else  {
					fprintf(stderr, "\nInvalid setup command '%s'\n\n",
						opt.valstr);
					res = BCME_BADARG;
					goto fail;
				}
			}
			/* -d duration */
			else if (opt.opt == 'd') {
				val.desc.wake_duration = htod32(opt.uval);
			}
			/* -p interval */
			else if (opt.opt == 'p') {
				val.desc.wake_interval_mantissa = htod16(opt.uval);
				argv++;
				if (argv[1] == NULL) {
					fprintf(stderr, "\nInvalid interval specified\n\n");
					res = BCME_BADARG;
					goto fail;
				}
				val.desc.wake_interval_exponent = (uint8)strtoul(argv[1], NULL, 0);
			}
			/* -I ID */
			else if (opt.opt == 'I') {
				val.desc.id = (uint8)opt.uval;
			}
			else {
				fprintf(stderr, "Unrecognized option '%s'\n", *argv);
				res = BCME_BADARG;
				goto fail;
			}
		}

		argv += opt.consumed;
		argc -= opt.consumed;
	}

	if (!(val.desc.flow_flags & WL_TWT_FLOW_FLAG_BROADCAST)) {
		/* Individual setup. This requires some options like wake duration, wake
		 * interval and listen interval
		 */
		if (((val.desc.wake_interval_mantissa == 0) &&
		     (val.desc.wake_interval_exponent == 0)) ||
		    (val.desc.wake_duration == 0)) {
			fprintf(stderr, "Missing option. When doing setup for individual TWT\n");
			fprintf(stderr, "make sure wake interval and duration are specified\n\n");
			res = BCME_USAGE_ERROR;
			goto fail;
		}
	}

	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}

	return wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

/* wl twt teardown command */
static int
wl_twt_cmd_teardown(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_teardown_t val;
	miniopt_t opt;
	int opt_err;
	int argc;
	bool got_mandatory = FALSE;

	BCM_REFERENCE(wl);

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	bzero(&val, sizeof(val));

	val.version = WL_TWT_TEARDOWN_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));

	miniopt_init(&opt, __FUNCTION__, "b", FALSE);

	while ((opt_err = miniopt(&opt, argv)) != -1) {

		if (opt_err == 1) {
			res = BCME_USAGE_ERROR;
			goto fail;
		}

		/* flags and options */
		if (!opt.positional) {

			/* flags */

			/* -b (broadcast) */
			if (opt.opt == 'b') {
				val.flow_flags |= WL_TWT_FLOW_FLAG_BROADCAST;
			}

			/* options */

			/* -a peer_address */
			else if (opt.opt == 'a') {
				if (!wl_ether_atoe(opt.valstr, &val.peer)) {
					res = BCME_BADARG;
					goto fail;
				}
			}
		}

		/* positionals */
		else {
			if (argc < 1) {
				res = BCME_USAGE_ERROR;
				goto fail;
			}

			/* flow_id */
			val.id = (uint8)strtoul(*argv, NULL, 0);
			argv++;

			got_mandatory = TRUE;

			break;
		}

		argv += opt.consumed;
	}

	if (!got_mandatory) {
		res = BCME_USAGE_ERROR;
		goto fail;
	}

	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}

	return wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

/* wl twt info command */
static int
wl_twt_cmd_info(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_info_t val;
	miniopt_t opt;
	int opt_err;
	int argc;
	bool got_mandatory = FALSE;

	BCM_REFERENCE(wl);

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	bzero(&val, sizeof(val));

	val.version = WL_TWT_INFO_VER;
	val.length = sizeof(val.version) + sizeof(val.length);

	miniopt_init(&opt, __FUNCTION__, "r", FALSE);

	while ((opt_err = miniopt(&opt, argv)) != -1) {

		if (opt_err == 1) {
			res = BCME_USAGE_ERROR;
			goto fail;
		}

		/* flags and options */
		if (!opt.positional) {

			/* flags */

			/* -r (response request) */
			if (opt.opt == 'r') {
				val.desc.flow_flags |= WL_TWT_INFO_FLAG_RESP_REQ;
			}

			/* options */

			/* -a peer_address */
			else if (opt.opt == 'a') {
				if (!wl_ether_atoe(opt.valstr, &val.peer)) {
					res = BCME_BADARG;
					goto fail;
				}
			}
			/* -w target_wake_time (twt) */
			else if (opt.opt == 'w') {
				uint64 twt = strtoull(opt.valstr, NULL, 0);
				val.desc.next_twt_h = htod32((uint32)(twt >> 32));
				val.desc.next_twt_l = htod32((uint32)twt);
			}
		}

		/* positionals */
		else {
			if (argc < 1) {
				res = BCME_USAGE_ERROR;
				goto fail;
			}

			/* flow_id */
			val.desc.id = (uint8)strtoul(*argv, NULL, 0);
			argv++;

			got_mandatory = TRUE;

			break;
		}

		argv += opt.consumed;
	}

	if (!got_mandatory) {
		res = BCME_USAGE_ERROR;
		goto fail;
	}

	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id,
		sizeof(val), (uint8 *)&val, BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}

	return wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

/* wl twt list command */
static int
wl_twt_cmd_list(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[WLC_IOCTL_SMLEN];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	wl_twt_list_t val;
	wl_twt_list_t *list;
	miniopt_t opt;
	int opt_err;
	int argc;
	int count;
	wl_twt_sdesc_t *desc;

	BCM_REFERENCE(wl);

	bzero(&val, sizeof(val));

	val.version = WL_TWT_LIST_VER;
	val.length = sizeof(val) - (sizeof(val.version) + sizeof(val.length));

	if (*argv == NULL) {
		goto skip_arg_parsing;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	miniopt_init(&opt, __FUNCTION__, "", FALSE);

	while ((opt_err = miniopt(&opt, argv)) != -1) {

		if (opt_err == 1) {
			/* No options, which is fine for this command */
			break;
		}

		/* options */
		if (!opt.positional) {
			/* -a peer_address */
			if (opt.opt == 'a') {
				if (!wl_ether_atoe(opt.valstr, &val.peer)) {
					res = BCME_BADARG;
					goto fail;
				}
			}
		} else {
			res = BCME_USAGE_ERROR;
			goto fail;
		}
		argv += opt.consumed;
	}

skip_arg_parsing:
	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id, sizeof(val), (uint8 *)&val,
		BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}
	res = wlu_var_getbuf_med(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len, (void **)&list);
	if ((res != BCME_OK) || (list == NULL)) {
		goto fail;
	}
	if (list->version != WL_TWT_LIST_VER) {
		res = BCME_VERSION;
		goto fail;
	}
	if ((list->bcast_count == 0) && (list->indv_count == 0)) {
		printf("\nNo TWT schedules available.\n");
		return 0;
	}
	if (list->bcast_count) {
		printf("\n\t%d Broadcast TWT schedules available:\n\n", list->bcast_count);
		printf("ID    Interval (usec)  Duration (usec)  Channel  Unannounced  Trigger"
			"  Protection\n");
	}
	for (count = 0; count < list->bcast_count + list->indv_count; count++) {
		if (count == list->bcast_count) {
			printf("\n\t%d Individual TWT schedules available:\n\n", list->indv_count);
			printf("ID    Interval (usec)  Duration (usec)  Channel  Unannounced"
				"  Trigger  Protection\n");
		}
		desc = &list->desc[count];
		printf("%2d)     %6d           %5d           0x%02x         %s       %s"
			"         %s\n",
			desc->id,
			desc->wake_interval_mantissa * (1 << desc->wake_interval_exponent),
			256 * desc->wake_duration, desc->channel,
			(desc->flow_flags & WL_TWT_FLOW_FLAG_UNANNOUNCED) ? "YES" : "NO ",
			(desc->flow_flags & WL_TWT_FLOW_FLAG_TRIGGER) ? "YES" : "NO ",
			(desc->flow_flags & WL_TWT_FLOW_FLAG_PROTECTION) ? "YES" : "NO ");
	}
	printf("\n");

	return 0;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}
