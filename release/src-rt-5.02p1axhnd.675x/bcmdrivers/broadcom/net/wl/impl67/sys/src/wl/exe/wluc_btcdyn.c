
/*
 * wl dynamic btcoex command module
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
 * $Id: wluc_btcdyn.c 774769 2019-05-07 08:46:22Z $
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

#ifdef LINUX
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* LINUX */

#include <miniopt.h>

static cmd_func_t wl_btcoex_dynctl;
static cmd_func_t wl_btcoex_dynctl_status;
static cmd_func_t wl_btcoex_dynctl_sim;

#define WL_BTCDYNCTL_USAGE \
"Usage:\n" \
"\t [-d dflt_dsns_level] [-l low_dsns_level] [-m mid_dsns_level] [-h high_dsns_level]\n" \
"\t [-c default_btc_mode\n" \
"\t [-s mode_switching_btrssi_hysteresis in dBm\n\n" \
"\t [-f dynctl_flags] : bit0: Dynctl on/off, bit1-desense on/off," \
" bit2-m_switch on/off, bit7-dryrun on/off\n" \
"\t [-j row_idx:btcmode,bt_pwr,wl_rssi_high,wl_rssi_low] : set one row in mode table\n" \
"\t [-k row_idx:btcmode,bt_pwr,wl_rssi_high,wl_rssi_low] : set one row in desense table\n" \
"\t [-n number of active rows in mode switching table\n" \
"\t [-o number of active rows in desense switching table\n\n"

#define WL_BTCDYNCTL_STATUS_USAGE \
"Usage: command doeesn't take any arguments]\n"

#define WL_BTCDYNCTL_SIM_USAGE \
"Usage: wl btc_dynctl_sim [1|0] [-b bt_sim_pwr] [-r bt_sim_rssi] [-w wl_sim_rssi] \n" \

/* bycdyn wl commands  */
static cmd_t wl_btcdyn_cmds[] = {
	{ "btc_dynctl", wl_btcoex_dynctl, WLC_GET_VAR, WLC_SET_VAR, WL_BTCDYNCTL_USAGE},
	{ "btc_dynctl_status", wl_btcoex_dynctl_status, WLC_GET_VAR, -1, WL_BTCDYNCTL_STATUS_USAGE},
	{ "btc_dynctl_sim", wl_btcoex_dynctl_sim, WLC_GET_VAR, WLC_SET_VAR, WL_BTCDYNCTL_SIM_USAGE},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_btcdyn_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register btcdyn commands */
	wl_module_cmds_register(wl_btcdyn_cmds);
}

static int fill_thd_row(btc_thr_data_t *thd, char *str)
{
	char delim[] = ":,\0";
	int err = BCME_OK;
	int idx = atoi(strtok(str, delim));
	char *s_mode, *s_bt_pwr, *s_bt_rssi, *s_wl_rssi_high, *s_wl_rssi_low;

	if (idx >= DCTL_TROWS_MAX) {
		fprintf(stderr, " invalid index:%d the data:%s\n", idx, str);
		err = BCME_BADARG;
		return err;
	}

	s_mode = strtok(NULL, delim);
	s_bt_pwr = strtok(NULL, delim);
	s_bt_rssi = strtok(NULL, delim);
	s_wl_rssi_high = strtok(NULL, delim);
	s_wl_rssi_low = strtok(NULL, delim);
	if (!s_mode || !s_bt_pwr || !s_bt_rssi || !s_wl_rssi_high ||
		!s_wl_rssi_low) {
		fprintf(stderr, " invalid row data format or number of parameters\n");
		err = BCME_BADARG;
		return err;
	}
	/* all present, convert them to int */
	thd[idx].mode = atoi(s_mode);
	thd[idx].bt_pwr = atoi(s_bt_pwr);
	thd[idx].bt_rssi = atoi(s_bt_rssi);
	thd[idx].wl_rssi_high = atoi(s_wl_rssi_high);
	thd[idx].wl_rssi_low = atoi(s_wl_rssi_low);

	printf("written table entry:%d: mode:%d, bt_pwr:%d, bt_rssi:%d,"
		" wl_rssi_high:%d, wl_rssi_low:%d\n",
		idx, thd[idx].mode, thd[idx].bt_pwr, thd[idx].bt_rssi,
		thd[idx].wl_rssi_high, thd[idx].wl_rssi_low);
	return err;
}

/*
*  get/set dynamic BTCOEX profile (desense, modesw etc)
*/
static int
wl_btcoex_dynctl(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, opt_err;
	miniopt_t to;
	void *ptr;
	dctl_prof_t *profile = NULL;

	UNUSED_PARAMETER(cmd);

	/* get current profile */
	if ((err = wlu_var_getbuf(wl,
			"btc_dynctl", NULL, 0, &ptr) < 0)) {
		fprintf(stderr, "btc_dynctl: getbuf ioctl failed\n");
		return err;
	}

	if (*++argv) {
		/* some arguments are present, do set or help  */

		if (argv[0][0] == 'h' ||
			argv[0][0] == '?') {
			err = BCME_USAGE_ERROR;
			goto exit;
		}

		profile = calloc(1, sizeof(dctl_prof_t) + sizeof(dynctl_status_t));
		if (profile == NULL)
			return BCME_NOMEM;

		/* copy current profile values into buf allocated for set ioctl */
		bcopy(ptr, profile, sizeof(dctl_prof_t));

		miniopt_init(&to, __FUNCTION__, NULL, FALSE);

		while ((opt_err = miniopt(&to, argv)) != -1) {
				if (opt_err == 1) {
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				argv += to.consumed;

				if (to.opt == 'c') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" default_btc_mode\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->default_btc_mode = to.val;
				}

				if (to.opt == 'd') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" dflt_dsns_level\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->dflt_dsns_level = to.val;
				}

				if (to.opt == 'l') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" low_dsns_level\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->low_dsns_level = to.val;
				}
				if (to.opt == 'm') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" mid_dsns_level\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->mid_dsns_level = to.val;
				}
				if (to.opt == 'h') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" high_dsns_level\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->high_dsns_level = to.val;
				}

				if (to.opt == 'f') {
					/*   set dynctl flags  */
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" flags \n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->flags = to.val;
				}

				/*   modify mode switching table  */
				if (to.opt == 'j') {
					if (fill_thd_row(profile->msw_data, to.valstr) != BCME_OK)
							goto exit;
				}

				/*   modify desense level table */
				if (to.opt == 'k') {
					if (fill_thd_row(profile->dsns_data, to.valstr) != BCME_OK)
							goto exit;
				}

				/*  set number of active rows in msw table  */
				if (to.opt == 'n') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" msw_rows\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
						/* check for array bounds  */
					if (to.val > DCTL_TROWS_MAX) {
						fprintf(stderr, "array index:%d >= MAX:%d !\n",
							to.val, DCTL_TROWS_MAX);
						err = BCME_BADARG;
						goto exit;
					}
					profile->msw_rows = to.val;
				}
				/*  set number of active rows in desense table  */
				if (to.opt == 'o') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" dsns_rows\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					/* check for array bounds  */
					if (to.val > DCTL_TROWS_MAX) {
						fprintf(stderr, "array index;%d >= MAX:%d !\n",
							to.val, DCTL_TROWS_MAX);
						err = BCME_BADARG;
						goto exit;
					}
					profile->dsns_rows = to.val;
				}
				if (to.opt == 's') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" btrssi_hysteresis \n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					profile->msw_btrssi_hyster = to.val;
				}
		} /* end of miniopt while loop */

		/* write back modified dctl_prof_t structure */
		if ((err = wlu_var_setbuf(wl, "btc_dynctl", profile,
			sizeof(dctl_prof_t)) < 0)) {
			printf("btc_dynctl: fail to set %d\n", err);
		}

	} else { /* display current dynctl profile */
		int i;
		profile = ptr;

		/* get:  just display the current profile values  */
		printf(" === btc dynctl: ===\n");
		printf("DYNCTL(0x%x): %s, Desense:%s, Mode:%s\n",
			profile->flags,
			IS_DYNCTL_ON(profile)?"On":"Off",
			IS_DESENSE_ON(profile)?"On":"Off",
			IS_MSWITCH_ON(profile)?"On":"Off");
			printf("dflt_dsns_level:%d\n", profile->dflt_dsns_level);
		printf("low_dsns_level:%d\n", profile->low_dsns_level);
		printf("mid_dsns_level:%d\n", profile->mid_dsns_level);
		printf("high_dsns_level:%d\n", profile->high_dsns_level);
		printf("btrssi msw hysteresis %d dBm\n", profile->msw_btrssi_hyster);
		printf("default_btc_mode:%d\n", profile->default_btc_mode);

		printf("--- coex mode switch data table "
			"msw_rows:%d ---\n", profile->msw_rows);

		for (i = 0; i < MIN(DCTL_TROWS_MAX, profile->msw_rows); i++) {
			printf("row:%d: btcmode:%d, bt_pwr:%d, bt_rssi:%d,"
				" wl_rssi_high:%d, wl_rssi_low:%d\n",
				i,
				profile->msw_data[i].mode,
				profile->msw_data[i].bt_pwr,
				profile->msw_data[i].bt_rssi,
				profile->msw_data[i].wl_rssi_high,
				profile->msw_data[i].wl_rssi_low);
		}
		printf("--- wl desense data table "
			"dsns_rows:%d ---\n", profile->dsns_rows);
		for (i = 0; i < MIN(DCTL_TROWS_MAX, profile->dsns_rows); i++) {
			printf("row:%d: btcmode:%d, bt_pwr:%d, bt_rssi:%d,"
				" wl_rssi_high:%d, wl_rssi_low:%d\n",
				i,
				profile->dsns_data[i].mode,
				profile->dsns_data[i].bt_pwr,
				profile->dsns_data[i].bt_rssi,
				profile->dsns_data[i].wl_rssi_high,
				profile->dsns_data[i].wl_rssi_low);
		}
		profile = NULL;
	}

exit:
	if (profile)
		free(profile);
	return err;
}

/*
*  get dynamic BTCOEX status
*/
static int
wl_btcoex_dynctl_status(void *wl, cmd_t *cmd, char **argv)
{
	dynctl_status_t	status;
	void *ptr;
	int err;

	UNUSED_PARAMETER(cmd);

	if (*++argv != NULL) {
		fprintf(stderr, "command doesn't accept any params\n");
		return BCME_USAGE_ERROR;
	}

	/* get current profile */
	if ((err = wlu_var_getbuf(wl,
			"btc_dynctl_status", NULL, 0, &ptr) < 0)) {
		printf("btc_dynctl: getbuf ioctl failed\n");
			return err;
	}
	/* copy the status from ioctl buffer */
	bcopy(ptr, &status, sizeof(dynctl_status_t));

	printf("--- btc dynctl status ---:\n");
	printf("simulation mode:%s\n", status.sim_on?"On":"Off");
	printf("bt_pwr_shm %x\n", dtoh16(status.bt_pwr_shm));
	printf("bt_pwr:%d dBm\n", status.bt_pwr);
	printf("bt_rssi:%d dBm\n", status.bt_rssi);
	printf("wl_rssi:%d dBm\n", status.wl_rssi);
	printf("dsns_level:%d\n", status.dsns_level);
	printf("btc_mode:%d\n", status.btc_mode);

	return err;
}

/*
*  get/set dynamic BTCOEX simulation mode
*/
static int
wl_btcoex_dynctl_sim(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, opt_err;
	miniopt_t to;
	void *ptr;
	dynctl_sim_t *sim = NULL;

	UNUSED_PARAMETER(cmd);

	/* get current profile */
	if ((err = wlu_var_getbuf(wl,
			"btc_dynctl_sim", NULL, 0, &ptr) < 0)) {
		printf("btc_dynctl: getbuf ioctl failed\n");
			return err;
	}

	if (*++argv) {
		/* some arguments are present, do set or help  */

		if (argv[0][0] == 'h' ||
			argv[0][0] == '?') {
			err = BCME_USAGE_ERROR;
			goto exit;
		}

		sim = calloc(1, sizeof(dynctl_sim_t));
		if (sim == NULL)
			return BCME_NOMEM;

		/* copy current ioctl buf into local var */
		bcopy(ptr, sim, sizeof(dynctl_sim_t));

		/* 1st parameter is on/off ( 0 | 1)  */
		if (*argv[0] == '1') {
			/* sim enabled */
			sim->sim_on = TRUE;
		}
		else  if (*argv[0] == '0') {
			/* sim disabled */
			sim->sim_on = FALSE;
		}

		++argv;

		miniopt_init(&to, __FUNCTION__, NULL, FALSE);

		while ((opt_err = miniopt(&to, argv)) != -1) {
				if (opt_err == 1) {
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				argv += to.consumed;
			if (to.opt == 'b') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" sim_btpwr\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					sim->btpwr = to.val;
				}

				if (to.opt == 'r') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" sim_btrssi\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					sim->btrssi = to.val;
				}

				if (to.opt == 'w') {
					if (!to.good_int) {
						fprintf(stderr, "could not parse %s as an int for"
							" sim_wlrssi\n", to.valstr);
						err = BCME_BADARG;
						goto exit;
					}
					sim->wlrssi = to.val;
				}
		}

			/* write back modified structure */
		if ((err = wlu_var_setbuf(wl, "btc_dynctl_sim", sim,
			sizeof(dynctl_sim_t)) < 0)) {
			printf("%s: failed to set %d\n", cmd->name, err);
		}
	} else {
		/* get */
		sim = ptr;
		printf("btc dynctl simulation mode:%s\n BT pwr:%ddBm\n"
			" BT rssi:%ddBm\n WL ssi:%ddBm\n",
			sim->sim_on?"On":"Off",
			sim->btpwr, sim->btrssi, sim->wlrssi);
		sim = NULL;
	}

exit:
	if (sim)
		free(sim);
	return err;
}
