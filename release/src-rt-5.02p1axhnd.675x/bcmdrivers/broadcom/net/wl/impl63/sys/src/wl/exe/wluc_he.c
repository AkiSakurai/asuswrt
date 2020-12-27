/*
 * wl he command module
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
 * $Id: wluc_he.c 779875 2019-10-09 10:31:01Z $
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

#define MIN_BSSCOLOR		1
#define MAX_BSSCOLOR		63
#define MIN_SWTICHCOUNT		0
#define MAX_SWITCHCOUNT		255
#define DEF_SWITCHCOUNT		10

#define HE_CMD_HELP_STR \
	"HE (802.11ax) protocol control commands\n\n" \
	"\tUsage: wl he [command] [cmd options]\n\n" \
	"Available commands and command options:\n" \
	"\twl he enab [0|1] - query or enable/disable HE feature\n" \
	"\twl he features [<features mask>] - query or enable/disable HE sub-features\n" \
	"\t\t<features mask> flags:\n" \
	"\t\t\t0x01: HE 5G support\n" \
	"\t\t\t0x02: HE 2G support\n" \
	"\t\t\t0x04: HE DL OFDMA support\n" \
	"\t\t\t0x08: HE UL OFDMA support\n" \
	"\t\t\t0x10: HE DL MUMIMO support\n" \
	"\twl he bsscolor [<color> [-w(ait) <beacon_count>]] - set or get color value\n" \
	"\t\tset color 0 to disable, wait is used by Color Change IE, use 0 for immediate update\n"\
	"\twl he muedca [<aci> <aifsn> <ecw min> <ecw max> <timer>] - \n" \
	"\t\tquery or set HE MU EDCA parameters\n" \
	"\twl he ppet [0|8|16|auto] - override ppet settings for all. (test only)\n" \
	"\twl he htc <code> - transmit HTC code. (test only)\n" \
	"\twl he omi [-a address] [<options>]\n" \
	"\t\t<options>:\n" \
	"\t\t\t-t <tx nsts> (1..8)\n" \
	"\t\t\t-r <rx nss> (1..8)\n" \
	"\t\t\t-b <20|40|80|160>\n" \
	"\t\t\t-e <er su disable> (0|1)\n" \
	"\t\t\t-s <dl mu-mimo resounding recommendation> (0|1)\n" \
	"\t\t\t-u <ul mu disable> (0|1)\n" \
	"\t\t\t-d <ul mu data disable> (0|1)\n"

static cmd_func_t wl_he_cmd;

/* wl he top level command list */
static cmd_t wl_he_cmds[] = {
	{ "he", wl_he_cmd, WLC_GET_VAR, WLC_SET_VAR, HE_CMD_HELP_STR },
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_he_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register HE commands */
	wl_module_cmds_register(wl_he_cmds);
}

/* HE sub cmd */
typedef struct sub_cmd sub_cmd_t;
typedef int (sub_cmd_func_t)(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv);
struct sub_cmd {
	char *name;
	uint16 id;		/* id for the dongle f/w switch/case  */
	uint16 type;		/* base type of argument IOVT_XXXX */
	sub_cmd_func_t *hdlr;	/* cmd handler  */
};

/*  HE ioctl sub cmd handler functions  */
static sub_cmd_func_t wl_he_cmd_uint;
static sub_cmd_func_t wl_he_cmd_bsscolor;
static sub_cmd_func_t wl_he_cmd_muedca;
static sub_cmd_func_t wl_he_cmd_ppet;
static sub_cmd_func_t wl_he_cmd_axmode;
static sub_cmd_func_t wl_he_cmd_omi;

/* wl he sub cmd list */
static const sub_cmd_t he_cmd_list[] = {
	/* wl he enab [0|1] */
	{"enab", WL_HE_CMD_ENAB, IOVT_UINT8, wl_he_cmd_uint},
	/* wl he features [<features>] */
	{"features", WL_HE_CMD_FEATURES, IOVT_UINT32, wl_he_cmd_uint},
	{"bsscolor", WL_HE_CMD_BSSCOLOR, IOVT_BUFFER, wl_he_cmd_bsscolor},
	/* wl he partialbsscolor [0|1] */
	{"partialbsscolor", WL_HE_CMD_PARTIAL_BSSCOLOR, IOVT_UINT8, wl_he_cmd_uint},
	{"cap", WL_HE_CMD_CAP, IOVT_UINT8, wl_he_cmd_uint},
	{"range_ext", WL_HE_CMD_RANGE_EXT, IOVT_UINT8, wl_he_cmd_uint},
	{"rtsdurthresh", WL_HE_CMD_RTSDURTHRESH, IOVT_UINT16, wl_he_cmd_uint},
	{"peduration", WL_HE_CMD_PEDURATION, IOVT_UINT8, wl_he_cmd_uint},
	{"muedca", WL_HE_CMD_MUEDCA, IOVT_BUFFER, wl_he_cmd_muedca},
	/* wl he dynfrag [0|1|2|3] */
	{"dynfrag", WL_HE_CMD_DYNFRAG, IOVT_UINT8, wl_he_cmd_uint},
	{"ppet", WL_HE_CMD_PPET, IOVT_UINT8, wl_he_cmd_ppet},
	{"htc", WL_HE_CMD_HTC, IOVT_UINT32, wl_he_cmd_uint},
	{"bssaxmode", WL_HE_CMD_AXMODE, IOVT_BOOL, wl_he_cmd_axmode},
	{"omi", WL_HE_CMD_OMI, IOVT_BUFFER, wl_he_cmd_omi},
	{"bsr", WL_HE_CMD_BSR_SUPPORT, IOVT_BOOL, wl_he_cmd_uint},
	{"testbed", WL_HE_CMD_TESTBED, IOVT_BOOL, wl_he_cmd_uint},
	{"fragtx", WL_HE_CMD_FRAGTX, IOVT_UINT8, wl_he_cmd_uint},
	{NULL, 0, 0, NULL}
};

/* wl he command */
static int
wl_he_cmd(void *wl, cmd_t *cmd, char **argv)
{
	const sub_cmd_t *sub = he_cmd_list;
	char *he_query[2] = {"enab", NULL};
	char *he_en[3] = {"enab", "1", NULL};
	char *he_dis[3] = {"enab", "0", NULL};
	int ret = BCME_USAGE_ERROR;

	/* skip to cmd name after "he" */
	argv++;

	if (!*argv) {
		/* query he "enab" state */
		argv = he_query;
	}
	else if (*argv[0] == '1') {
		argv = he_en;
	}
	else if (*argv[0] == '0') {
		argv = he_dis;
	}
	else if (!strcmp(*argv, "-h") || !strcmp(*argv, "help"))  {
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
} he_xtlv_v32;

static uint
wl_he_iovt2len(uint iovt)
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
wl_he_get_uint_cb(void *ctx, uint16 *id, uint16 *len)
{
	he_xtlv_v32 *v32 = ctx;

	*id = v32->id;
	*len = v32->len;

	return FALSE;
}

static void
wl_he_pack_uint_cb(void *ctx, uint16 id, uint16 len, uint8 *buf)
{
	he_xtlv_v32 *v32 = ctx;

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
wl_he_cmd_uint(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
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
			uint len = wl_he_iovt2len(sub->type);
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
		he_xtlv_v32 v32;

		v32.id = sub->id;
		v32.len = wl_he_iovt2len(sub->type);
		v32.val = strtol(*argv, NULL, 0);

		bzero(mybuf, sizeof(mybuf));
		res = bcm_pack_xtlv_buf((void *)&v32, mybuf, sizeof(mybuf),
			BCM_XTLV_OPTION_ALIGN32, wl_he_get_uint_cb, wl_he_pack_uint_cb,
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

static int
wl_he_cmd_bsscolor(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	bcm_xtlv_t getbuf;
	uint8 *ptr;
	wl_he_bsscolor_t *bsscolor;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	int color;
	int wait;
	int i;

	/* send getbuf iovar */
	getbuf.id = sub->id;
	getbuf.len = 0;
	res = wlu_var_getbuf_med(wl, cmd->name, &getbuf, sizeof(getbuf), (void **)&ptr);
	/* check the response buff  */
	if ((res != BCME_OK) || (ptr == NULL)) {
		goto fail;
	}
	bsscolor = (wl_he_bsscolor_t *)ptr;

	if (*argv == NULL) { /* get */
		printf("\n\tHE BSS Color settings:\n\n");
		printf("Color: %d\n", bsscolor->color);
		printf("Disabled: %s\n", bsscolor->disabled ? "TRUE" : "FALSE");
		printf("Color Switch Countdown: %d\n", bsscolor->switch_count);
	} else { /* set */
		/* Update the received values, with the params given. If color is set to 0
		 * then disabled becomes true, otherwise false.
		 */
		color = -1;
		wait = bsscolor->switch_count;
		if ((wait < MIN_BSSCOLOR) || (wait > MAX_BSSCOLOR)) {
			wait = DEF_SWITCHCOUNT;
		}
		for (i = 0; ((i < 2) && (*argv != NULL)); i++) {
			if (((*argv)[0] == '-') && ((*argv)[1] == 'w')) {
				argv++;
				if (*argv == NULL) {
					fprintf(stderr, "No wait value specified\n");
					res = BCME_USAGE_ERROR;
					goto fail;
				}
				wait = strtol(*argv, NULL, 0);
			} else {
				color = strtol(*argv, NULL, 0);
			}
			argv++;
		}
		if (((color < MIN_BSSCOLOR) || (color > MAX_BSSCOLOR)) && (color != 0)) {
			fprintf(stderr, "Invalid color specified\n");
			res = BCME_USAGE_ERROR;
			goto fail;
		}
		if ((wait < MIN_SWTICHCOUNT) || (wait > MAX_SWITCHCOUNT)) {
			fprintf(stderr, "Invalid switch count specified\n");
			res = BCME_USAGE_ERROR;
			goto fail;
		}

		if (color == 0) {
			bsscolor->disabled = 1;
		} else {
			bsscolor->disabled = 0;
			bsscolor->color = (uint8)color;
		}
		bsscolor->switch_count = wait;

		res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id, sizeof(*bsscolor),
			(uint8 *)bsscolor, BCM_XTLV_OPTION_ALIGN32);
		if (res != BCME_OK) {
			goto fail;
		}
		res = wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);
	}
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static const char ac_names[AC_COUNT][6] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};

static int
wl_he_cmd_muedca(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	bcm_xtlv_t getbuf;
	uint8 *ptr;
	wl_he_muedca_v1_t *muedca;
	wl_he_edca_v1_t *edca;
	int aci;

	getbuf.id = sub->id;
	getbuf.len = 0;

	/* send getbuf iovar */
	res = wlu_var_getbuf_med(wl, cmd->name, &getbuf, sizeof(getbuf), (void **)&ptr);
	/* check the response buff  */
	if ((res != BCME_OK) || (ptr == NULL)) {
		goto fail;
	}
	muedca = (wl_he_muedca_v1_t *)ptr;
	if (muedca->version != WL_HE_VER_1) {
		res = BCME_VERSION;
		goto fail;
	}

	if (*argv == NULL) { /* get */
		printf("\n\tHE MU EDCA paramaters STA:\n\n");
		edca = &muedca->ac_param_sta[0];

		printf("   ACI      AIFSN   CW Min   CW Max   Timer\n");
		for (aci = AC_BE; aci < AC_COUNT; aci++) {
			printf("%s:%d)     %2d       %2d       %2d      %d\n",
				ac_names[aci], aci,
				edca[aci].aci_aifsn & EDCF_AIFSN_MASK,
				edca[aci].ecw_min_max & EDCF_ECWMIN_MASK,
				(edca[aci].ecw_min_max & EDCF_ECWMAX_MASK) >>
				EDCF_ECWMAX_SHIFT, edca[aci].muedca_timer);
		}
	} else { /* set */
		/* fetch complete structure for all ACIs, update the one which is to be updated
		 * and send the comple structure back.
		 */
		int aifsn, ecw_min, ecw_max, timer;

		res = BCME_USAGE_ERROR;
		edca = &muedca->ac_param_sta[0];

		/* ACI */
		aci = strtol(*argv, NULL, 0);
		if ((aci < AC_BE) || (aci >= AC_COUNT)) {
			fprintf(stderr, "invalid ACI:%d\n", aci);
			goto fail;
		}
		argv++;
		if (*argv == NULL)
			goto fail;

		/* AIFSN */
		aifsn = strtol(*argv, NULL, 0);
		if ((aifsn < 0) || (aifsn > EDCF_AIFSN_MAX)) {
			fprintf(stderr, "invalid AIFSN:%d\n", aifsn);
			goto fail;
		}
		argv++;
		if (*argv == NULL)
			goto fail;

		/* ECW_MIN */
		ecw_min = strtol(*argv, NULL, 0);
		if ((ecw_min < EDCF_ECW_MIN) || (ecw_min > EDCF_ECW_MAX)) {
			fprintf(stderr, "invalid ECW_MIN:%d\n", ecw_min);
			goto fail;
		}
		argv++;
		if (*argv == NULL)
			goto fail;

		/* ECW_MAX */
		ecw_max = strtol(*argv, NULL, 0);
		if ((ecw_max < EDCF_ECW_MIN) || (ecw_max > EDCF_ECW_MAX)) {
			fprintf(stderr, "invalid ECW_MAX:%d\n", ecw_max);
			goto fail;
		}
		argv++;
		if (*argv == NULL)
			goto fail;

		/* TIMER */
		timer = strtol(*argv, NULL, 0);
		if ((timer < 0) || (timer > 255)) {
			fprintf(stderr, "invalid TIMER:%d\n", timer);
			goto fail;
		}

		edca[aci].aci_aifsn &= ~EDCF_AIFSN_MASK,
		edca[aci].aci_aifsn |= aifsn;
		edca[aci].ecw_min_max = (ecw_max << EDCF_ECWMAX_SHIFT) | ecw_min;
		edca[aci].muedca_timer = timer;

		res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id, sizeof(*muedca), (uint8 *)muedca,
			BCM_XTLV_OPTION_ALIGN32);
		if (res != BCME_OK) {
			goto fail;
		}
		res = wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);
	}
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_he_cmd_axmode(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	bcm_xtlv_t getbuf;
	uint8 *resp;

	/* get */
	if (*argv == NULL) {
		getbuf.id = sub->id;
		getbuf.len = 0;

		/* send getbuf iovar */
		res = wlu_var_getbuf_sm(wl, cmd->name, &getbuf, sizeof(getbuf), (void **)&resp);

		/*  check the response buff  */
		if ((res != BCME_OK) || (resp == NULL)) {
			goto fail;
		}
		/*  check the response buff  */
		if (res == BCME_OK && resp != NULL) {
			uint len = wl_he_iovt2len(sub->type);
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
		he_xtlv_v32 v32;

		v32.id = sub->id;
		v32.len = wl_he_iovt2len(sub->type);
		v32.val = strtol(*argv, NULL, 0);
		bzero(mybuf, sizeof(mybuf));
		res = bcm_pack_xtlv_buf((void *)&v32, mybuf, sizeof(mybuf),
			BCM_XTLV_OPTION_ALIGN32, wl_he_get_uint_cb, wl_he_pack_uint_cb,
			&mybuf_len);
		if (res != BCME_OK) {
			goto fail;
		}
		res = wlu_var_setbuf(wl, cmd->name, mybuf, mybuf_len);
		if (res != BCME_OK) {
			goto fail;
		}
	}
	return res;
fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_he_cmd_ppet(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	bcm_xtlv_t getbuf;
	he_xtlv_v32 v32;
	uint8 mybuf[64];
	int mybuf_len;
	uint8 *resp;
	uint8 ppet;

	/* get */
	if (*argv == NULL) {
		getbuf.id = sub->id;
		getbuf.len = 0;

		/* send getbuf iovar */
		res = wlu_var_getbuf_sm(wl, cmd->name, &getbuf, sizeof(getbuf), (void **)&resp);

		/*  check the response buff  */
		if ((res != BCME_OK) || (resp == NULL)) {
			goto fail;
		}
		ppet = *resp;
		switch (*resp) {
		case WL_HE_PPET_0US:
			ppet = 0;
			break;
		case WL_HE_PPET_8US:
			ppet = 8;
			break;
		case WL_HE_PPET_16US:
			ppet = 16;
			break;
		case WL_HE_PPET_AUTO:
			printf("auto\n");
			goto done;
			break;
		default:
			printf("Invalid (%u)\n", *resp);
			res = BCME_RANGE;
			goto fail;
		}
		printf("%u\n", ppet);
	} else { /* set */
		if (!strcmp(*argv, "auto")) {
			ppet = WL_HE_PPET_AUTO;
		} else if (*argv[0] == '0') {
			ppet = WL_HE_PPET_0US;
		} else if (*argv[0] == '8') {
			ppet = WL_HE_PPET_8US;
		} else if ((*argv[0] == '1') && ((*argv)[1] == '6')) {
			ppet = WL_HE_PPET_16US;
		} else {
			fprintf(stderr, "invalid ppet value\n");
			res = BCME_USAGE_ERROR;
			goto fail;
		}

		v32.id = sub->id;
		v32.len = wl_he_iovt2len(sub->type);
		v32.val = ppet;

		mybuf_len = sizeof(mybuf);
		bzero(mybuf, mybuf_len);
		res = bcm_pack_xtlv_buf((void *)&v32, mybuf, sizeof(mybuf), BCM_XTLV_OPTION_ALIGN32,
			wl_he_get_uint_cb, wl_he_pack_uint_cb, &mybuf_len);
		if (res == BCME_OK) {
			res = wlu_var_setbuf(wl, cmd->name, mybuf, mybuf_len);
		}
		if (res != BCME_OK) {
			goto fail;
		}
	}
done:
	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}

static int
wl_he_cmd_omi(void *wl, const cmd_t *cmd, const sub_cmd_t *sub, char **argv)
{
	int res = BCME_OK;
	wl_he_omi_t get_omi;
	wl_he_omi_t *omi;
	uint8 mybuf[64];
	uint8 *rem = mybuf;
	uint16 rem_len = sizeof(mybuf);
	uint8 command;
	int txnsts;
	int rxnss;
	int bw;
	int er_su_disable;
	int dl_mu_mimo_resound;
	int ul_mu_disable;
	int ul_mu_data_disable;
	bool override;

	memset(&get_omi, 0, sizeof(get_omi));
	txnsts = 0;
	rxnss = 0;
	bw = 0;
	er_su_disable = -1;
	dl_mu_mimo_resound = -1;
	ul_mu_disable = -1;
	ul_mu_data_disable = -1;
	override = FALSE;
	while (*argv != NULL) {
		if ((*argv)[0] == '-') {
			command = (*argv)[1];
			argv++;
			if (command == 'o') {
				override = TRUE;
				continue;
			}
			if (*argv == NULL) {
				res = BCME_USAGE_ERROR;
				goto fail;
			}
			if (command == 'a') {
				if (!wl_ether_atoe(*argv, &get_omi.peer)) {
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 't') {
				txnsts = strtol(*argv, NULL, 0);
				if ((txnsts < 1) || (txnsts > 8)) {
					fprintf(stderr, "invalid txnsts:%d\n", txnsts);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 'r') {
				rxnss = strtol(*argv, NULL, 0);
				if ((rxnss < 1) || (rxnss > 8)) {
					fprintf(stderr, "invalid rxnss:%d\n", rxnss);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 'b') {
				bw = strtol(*argv, NULL, 0);
				if ((bw != 20) && (bw != 40) && (bw != 80) && (bw != 160)) {
					fprintf(stderr, "invalid bw:%d\n", bw);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 'e') {
				er_su_disable = strtol(*argv, NULL, 0);
				if ((er_su_disable < 0) || (er_su_disable > 1)) {
					fprintf(stderr, "invalid er su disable:%d\n",
						er_su_disable);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 's') {
				dl_mu_mimo_resound = strtol(*argv, NULL, 0);
				if ((dl_mu_mimo_resound < 0) || (dl_mu_mimo_resound > 1)) {
					fprintf(stderr,
						"invalid dl mu-mimo resounding recommendation:%d\n",
						dl_mu_mimo_resound);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 'u') {
				ul_mu_disable = strtol(*argv, NULL, 0);
				if ((ul_mu_disable < 0) || (ul_mu_disable > 1)) {
					fprintf(stderr, "invalid ul mu disable:%d\n",
						ul_mu_disable);
					res = BCME_BADARG;
					goto fail;
				}
			} else if (command == 'd') {
				ul_mu_data_disable = strtol(*argv, NULL, 0);
				if ((ul_mu_data_disable < 0) || (ul_mu_data_disable > 1)) {
					fprintf(stderr, "invalid ul mu data disable:%d\n",
						ul_mu_data_disable);
					res = BCME_BADARG;
					goto fail;
				}
			} else {
				res = BCME_USAGE_ERROR;
				goto fail;
			}
		} else {
			res = BCME_USAGE_ERROR;
			goto fail;
		}
		argv++;
	}

	/* build getbuf iovar */
	res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id, sizeof(get_omi), (uint8 *)&get_omi,
		BCM_XTLV_OPTION_ALIGN32);
	if (res != BCME_OK) {
		goto fail;
	}

	res = wlu_var_getbuf_sm(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len, (void **)&omi);
	/* check the response buff  */
	if ((res != BCME_OK) || (omi == NULL)) {
		goto fail;
	}

	if ((txnsts) || (rxnss) ||(bw) || (er_su_disable != -1) || (dl_mu_mimo_resound != -1) ||
		(ul_mu_disable != -1) || (ul_mu_data_disable != -1)) {
		/* Set operation. Modify fields with info and set it */
		memcpy(&get_omi, omi, sizeof(get_omi));
		if (txnsts) {
			get_omi.tx_nsts = txnsts - 1;
		}
		if (rxnss) {
			get_omi.rx_nss = rxnss - 1;
		}
		if (bw) {
			get_omi.channel_width =
				(bw == 20) ? 0 : (bw == 40) ? 1 : (bw == 80) ? 2 : 3;
		}
		if (er_su_disable != -1) {
			get_omi.er_su_disable = er_su_disable;
		}
		if (dl_mu_mimo_resound != -1) {
			get_omi.dl_mumimo_resound = dl_mu_mimo_resound;
		}
		if (ul_mu_disable != -1) {
			get_omi.ul_mu_disable = ul_mu_disable;
		}
		if (ul_mu_data_disable != -1) {
			get_omi.ul_mu_data_disable = ul_mu_data_disable;
		}
		if (override) {
			get_omi.tx_override = 1;
		} else {
			get_omi.tx_override = 0;
		}

		rem = mybuf;
		rem_len = sizeof(mybuf);
		res = bcm_pack_xtlv_entry(&rem, &rem_len, sub->id, sizeof(get_omi),
			(uint8 *)&get_omi, BCM_XTLV_OPTION_ALIGN32);
		if (res != BCME_OK) {
			goto fail;
		}
		res = wlu_var_setbuf(wl, cmd->name, mybuf, sizeof(mybuf) - rem_len);
	} else {
		/* Get operation. Print info */
		printf("\nCurrent OM control information:\n\n");
		printf("\tRx NSS: %d\n", omi->rx_nss + 1);
		printf("\tTx NSTS: %d\n", omi->tx_nsts + 1);
		printf("\tChannel Width: %s\n", (omi->channel_width == 0) ? "20" :
			(omi->channel_width == 1) ? "40" : (omi->channel_width == 2) ? "80" :
			"160");
		printf("\tER SU Disable: %d\n", omi->er_su_disable);
		printf("\tDL MU-MIMO Resound Recommendation: %d\n", omi->dl_mumimo_resound);
		printf("\tUL MU Disable: %d\n", omi->ul_mu_disable);
		printf("\tUL MU Data Disable: %d\n", omi->ul_mu_data_disable);
		printf("\n");
	}

	return res;

fail:
	fprintf(stderr, "error:%d\n", res);
	return res;
}
