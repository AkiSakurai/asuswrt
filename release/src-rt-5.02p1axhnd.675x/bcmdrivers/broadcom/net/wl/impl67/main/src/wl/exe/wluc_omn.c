/*
 * wl OMN (Oper Mode / Oper Mode Notification related) command module
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
 * $Id: wluc_omn.c 789501 2020-07-30 03:18:50Z $
 */

#ifdef WIN32
#include <windows.h>
#endif // endif

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

/* Because IL_BIGENDIAN was removed there are few warnings that need
 * to be fixed. Windows was not compiled earlier with IL_BIGENDIAN.
 * Hence these warnings were not seen earlier.
 * For now ignore the following warnings
 */
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4761)
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_omn_master;

static cmd_t wl_omn_cmds[] = {
	{"omn_master", wl_omn_master, WLC_GET_VAR, WLC_SET_VAR,
	"Change oper mode master or related locks\n"
	"\t no parameter:   Get\n"
	"\t [module [clear|skip]]\n"
	"\t\t where module (rank high to low) could be one of:\n"
	"\t\t\t 1 / zdfs:       Set ZDFS as the master\n"
	"\t\t\t 2 / airiq:      Set AirIQ as the master\n"
	"\t\t\t 3 / obss_dbs:   Set obss_dbs as the master\n"
	"\t\t\t 4 / bw160:      Set bw160 as the master\n"
	"\t\t clear: will unset the mentioned module as master if currently the master\n"
	"\t\t skip: will avoid locking mentioned module as the master even if a lower "
	"ranking module is the current master\n"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

static char *wl_omnm_mod_str[WL_OMNM_MOD_LAST] = {
	"none",
	"zdfs",
	"airiq",
	"obss_dbs",
	"bw160",
};

#define WL_OMNM_STR_CLEAR		"clear"
#define WL_OMNM_STR_SKIP		"skip"

/* module initialization */
void
wluc_omn_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register omn commands */
	wl_module_cmds_register(wl_omn_cmds);
}

/* given pointer to a buffer of wl_omn_info_t in device endian,
 * displays contained information (without altering the buffer passed)
 */
static int
wl_omn_master_show(wl_omnm_info_t *omnm)
{
	int err = BCME_OK;
	uint16 mod_idx = WL_OMNM_MOD_NONE;
	wl_omnm_info_t omnm_he; /* host endian */

	bzero(&omnm_he, sizeof(omnm_he)); /* since {0} leads to error on some buggy gcc */

	omnm_he.ver = dtoh16(omnm->ver);
	if (omnm_he.ver != WL_OMNM_VER) {
		err = BCME_UNSUPPORTED;
		printf("err=%d unsupported version=%d\n", err, omnm_he.ver);
		return err;
	}

	omnm_he.len = dtoh16(omnm->len);
	if (omnm_he.len < sizeof(omnm_he)) {
		err = BCME_BUFTOOSHORT;
		printf("err=%d buffer too short %d\n", err, omnm_he.len);
		return err;
	}

	omnm_he.act		= dtoh16(omnm->act);
	omnm_he.flags		= dtoh16(omnm->flags);
	omnm_he.mod		= dtoh16(omnm->mod);
	omnm_he.lock_life	= dtoh16(omnm->lock_life);

	if (omnm_he.mod < WL_OMNM_MOD_LAST) {
		mod_idx = omnm_he.mod;
	}

	printf("\tMaster module: %s, ", wl_omnm_mod_str[mod_idx]);
	if (omnm_he.lock_life && mod_idx) {
		printf("lock expires in %d seconds\n", omnm_he.lock_life);
	} else {
		if ((omnm_he.act & WL_OMNM_ACT_SET) != 0) {
			printf("to be locked\n");
		} else if ((omnm_he.act & WL_OMNM_ACT_CLEAR) != 0) {
			printf("to be cleared\n");
		} else {
			printf("lock expired\n");
		}
	}
	printf("\t\t[ver: %d, len: %d, act: %d, flags: %d, mod: %d, lock_life: %d]\n",
			omnm_he.ver, omnm_he.len, omnm_he.act, omnm_he.flags,
			omnm_he.mod, omnm_he.lock_life);

	return err;
}

static int
wl_omn_master(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err;

	/* toss the command name */
	argv++;

	if (*argv && strlen(*argv)) { /* Set */
		char c0 = argv[0][0], c1 = argv[0][1];
		uint16 mod_idx = WL_OMNM_MOD_NONE;
		uint16 act = WL_OMNM_ACT_SET, flags = 0;
		wl_omnm_info_t omnm_de; /* device endian */

		bzero(&omnm_de, sizeof(omnm_de)); /* since {0} leads to err on some buggy gcc */

		if (c0 >= '0' && c0 <= '9' && !c1) { /* single digit input */
			mod_idx = c0 - '0';
		} else {
			for (; mod_idx < WL_OMNM_MOD_LAST; mod_idx++) {
				char *ms = wl_omnm_mod_str[mod_idx];
				if (!strncmp(ms, *argv, strlen(ms))) {
					break;
				}
			}
		}
		if (mod_idx >= WL_OMNM_MOD_LAST) {
			err = BCME_BADARG;
			printf("err=%d bad argument '%s'\n", err, *argv);
			wl_cmd_usage(stdout, cmd);
			return err;
		}
		argv++;
		if (*argv) {
			if (!strncmp(*argv, WL_OMNM_STR_CLEAR, strlen(WL_OMNM_STR_CLEAR))) {
				act = WL_OMNM_ACT_CLEAR; /* change action to clear */
			} else if (!strncmp(*argv, WL_OMNM_STR_SKIP, strlen(WL_OMNM_STR_SKIP))) {
				flags |= WL_OMNM_FLAG_SKIP; /* skip if locked */
			}
		}
		omnm_de.ver   = htod16(WL_OMNM_VER);
		omnm_de.len   = htod16(sizeof(omnm_de));
		omnm_de.flags = htod16(flags);
		omnm_de.act   = htod16(act);
		omnm_de.mod   = htod16(mod_idx);
		printf("Setting:\n");
		wl_omn_master_show(&omnm_de);
		/* using getbuf since this sets and gets return buffer also */
		if ((err = wlu_var_getbuf_sm(wl, cmd->name, &omnm_de,
				sizeof(omnm_de), &ptr)) != BCME_OK) {
			printf("err=%d\n", err);
			return err;
		}
		printf("Returned: (status: %d)\n", err);
		wl_omn_master_show((wl_omnm_info_t *) ptr);
	} else { /* GET */
		if (cmd->get < 0) {
			return -1;
		}

		if ((err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, &ptr))) {
			printf("err=%d \n", err);
			wl_cmd_usage(stdout, cmd);
			return err;
		}

		wl_omn_master_show((wl_omnm_info_t *) ptr);
	}

	return BCME_OK;
}
