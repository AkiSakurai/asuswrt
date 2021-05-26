/*
 * wl proxd command module
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
 * $Id: wluc_proxd.c 790688 2020-08-31 20:02:59Z $
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
#include <bcmwifi_rspec.h>

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
#include <errno.h>

static cmd_func_t wl_proxd;
static cmd_func_t wl_proxd_tune;
static cmd_func_t wl_proxd_collect;
#if defined(linux)
static cmd_func_t wl_proxd_event_check;
#endif   /* linux */
#define WL_PROXD_PAYLOAD_LEN	1026
#define TOF_DEFAULT_FTMCNT_SEQ		3

#define WL_PROXD_TUNE_VERSION_MAJOR_1	1
#define WL_PROXD_TUNE_VERSION_MAJOR_2	2
#define WL_PROXD_TUNE_VERSION_MAJOR_3	3

#define PROXD_TUNE_USAGE	\
"\tUsage: wl proxd_tune method [operations]\n\n" \
"\tMandatory args:\n"		\
"\t\tmethod: == 2 (TOF) methods are supported \n\n" \
"\tOperations:\n"		\
"\t\t-k K factor     : hardware dependant RTD delay adjustment factor \n" \
"\t\t-b vhtack       : 0:disable VHT ACK, 1:enable VHT ACK\n" \
"\t\t-n minDT        : min time difference of T1 and T4 or T2 and T3 \n" \
"\t\t-x maxDT		 : max time difference of T1 and T4 or T2 and T3 \n" \
"\t\t-t total_frmcnt : total count limit of measurement frames transmitted \n" \
"\t\t-N threshold_log2 : log2 number of simple threshold crossing \n" \
"\t\t-S threshold_scale: scale number of simple threshold crossing \n" \
"\t\t-F ftm_cnt      : number of measurement frames requested by initiator \n" \
"\t\t-r rsv_media_value: reserve media duration value for TOF \n" \
"\t\t-f flags        : TOF state machine control flags\n" \
"\t\t-A timestamp_adj : enable/disable sw/hw/seq assisted timestamp adjustment, " \
"the data format is s[0|1]h[0|1]r[0|1] \n" \
"\t\t-W window_adjust : set search window length and offset, " \
"the data format is bBlLoO, B is bandwidth \n" \
"\t\t-e emu_delay     : emulator delay in tenth of nano-second\n" \
"\t\t                 : with value 20, 40 or 80, L is window length, O is offset"

static cmd_t wl_proxd_cmds[] = {
	{ "proxd", wl_proxd, WLC_GET_VAR, WLC_SET_VAR,
	"Configure Proximity Detection\n"
	"\tftm [<session-id>] <cmd> [<param-name><param-value>...]:"
	" enable FTM, type 'wl proxd -h ftm' for more information\n\n"
	"\tExample: wl proxd ftm enable"},
	{ "proxd_collect", wl_proxd_collect, WLC_GET_VAR, WLC_SET_VAR,
	"collect the debugging informations of Proximity Detection \n\n"
	"Optional parameters is:\n"
	"\tenable to enable the proxd collection.\n"
	"\tdisable to disable the proxd collection.\n"
	"\t-l, dump local collect data and request load remote AP collect data.\n"
	"\t-r, dump remote collect data or request load remote AP collect data.\n"
	"\t-f File name to dump the sample buffer (default \"proxd_collect.dat\")\n"},
	{ "proxd_tune", wl_proxd_tune, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get tune parameters for TOF method of Proximity Detection\n\n"
	PROXD_TUNE_USAGE},
#if defined(linux)
	{ "proxd_event_check", wl_proxd_event_check, -1, -1,
	"Listen and print Location Based Service events\n"
	"\tproxd_event_check syntax is: proxd_event_check ifname"},
#endif /* linux */
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/*
* for FTM
*/

static int wl_proxd_cmd_method_handler(void *wl, cmd_t *cmd, char **argv);
static int ftm_handle_help(char **argv);
#if defined(linux)
static int ftm_event_check(bcm_event_t *p_bcm_event);
#endif // endif
static void ftm_display_config_help();
static void ftm_display_config_options_help();
static void ftm_display_config_avail_help();
static void ftm_unpack_and_display_session_flags(const uint8 *p_data, uint16 tlvid);
static void ftm_unpack_and_display_config_flags(const uint8 *p_data, uint16 tlvid);
static int proxd_tune_display(void *tof_tune, uint16 len);

/*
*for debug: define the flag WL_FTM_DEBUG to enable debug log for FTM
*/
/* #define WL_FTM_DEBUG */
#ifdef WL_FTM_DEBUG
static void ftm_format_event_mask(wl_proxd_event_type_t event, char *p_strbuf, int bufsize);
#endif /* WL_FTM_DEBUG */

int proxd_tune_ver_get(int ver_major)
{
	int  proxd_tune_expected_ver = 0;

	if (ver_major <= 3) { /* Maccabee (9.44) major ver is 3 */
		/* use v1 */
		proxd_tune_expected_ver = WL_PROXD_TUNE_VERSION_MAJOR_1;
#ifdef WL_FTM_DEBUG
		printf("ver_major is %d\n", ver_major);
#endif /* WL_FTM_DEBUG */
	} else if((ver_major > 3) && (ver_major <= 5)) { /* 4364 initiator (9.30) major ver is 5 */
		/* use v2 */
		proxd_tune_expected_ver = WL_PROXD_TUNE_VERSION_MAJOR_2;
#ifdef WL_FTM_DEBUG
		printf("ver_major is %d\n", ver_major);
#endif /* WL_FTM_DEBUG */
	} else {
		/* use v3 */
		proxd_tune_expected_ver = WL_PROXD_TUNE_VERSION_MAJOR_3;
#ifdef WL_FTM_DEBUG
		printf("ver_major is %d\n", ver_major);
#endif /* WL_FTM_DEBUG */
	}
	return proxd_tune_expected_ver;
}

/* module initialization */
void
wluc_proxd_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register proxd commands */
	wl_module_cmds_register(wl_proxd_cmds);
}

static int
wl_proxd(void *wl, cmd_t *cmd, char **argv)
{
	uint16 var[2], *reply;
	uint16 method = 0, role = 0;
	void *ptr;
	int ret;

	/* skip the command name and check if NULL */
	if (!*++argv) {
		/* Get */
		ret = wlu_var_getbuf(wl, cmd->name, &var, sizeof(var), &ptr);
		if (ret != BCME_OK) {
			return ret;
		}

		reply = (uint16 *)ptr;

		method = dtoh16(reply[0]);

		printf("%d\n", method);

		if (method > 0) {
			char c = 'u';
			role = dtoh16(reply[1]);
			if (role & WL_PROXD_RANDOM_WAKEUP) {
				c = 'r';
				role &= ~WL_PROXD_RANDOM_WAKEUP;
			}
			if (role == WL_PROXD_MODE_INITIATOR)
				printf("%s %c\n", "initiator", c);
			else if (role == WL_PROXD_MODE_TARGET)
				printf("%s %c\n", "target", c);
			else if (role == WL_PROXD_MODE_NEUTRAL)
				printf("%s %c\n", "neutral", c);
		}
	} else {
		/* Set */
		if (!isdigit((int)*argv[0]))
			return wl_proxd_cmd_method_handler(wl, cmd, argv);

		/* parse method and role */
		method = (uint16)atoi(argv[0]);
		if (method > 0) {
			if (!argv[1]) {
				/* Default when it is not specified */
				role = WL_PROXD_MODE_NEUTRAL | WL_PROXD_RANDOM_WAKEUP;
			}
			else {
				if (stricmp(argv[1], "initiator") == 0)
					role = WL_PROXD_MODE_INITIATOR;
				else if (stricmp(argv[1], "target") == 0)
					role = WL_PROXD_MODE_TARGET;
				else if (stricmp(argv[1], "neutral") == 0) {
					role = WL_PROXD_MODE_NEUTRAL;
					role |= WL_PROXD_RANDOM_WAKEUP;
				}
				else
					return BCME_USAGE_ERROR;

				if (argv[2]) {
					if (*argv[2] == 'R' || *argv[2] == 'r')
						role |= WL_PROXD_RANDOM_WAKEUP;
					else if (*argv[2] == 'u' || *argv[2] == 'U')
						role &= ~WL_PROXD_RANDOM_WAKEUP;
				}
			}
		}

		var[0] = htod16(method);
		var[1] = htod16(role);

		ret = wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	}

	return ret;
}

static int
wl_proxd_get_debug_data(void *wl, cmd_t *cmd, int index)
{
	int ret;
	void *buff;
	wl_proxd_collect_query_t query;
	wl_proxd_debug_data_t *replay;

	bzero(&query, sizeof(query));
	query.method = htol32(PROXD_TOF_METHOD);
	query.request = PROXD_COLLECT_QUERY_DEBUG;
	query.index = htol16(index);

	ret = wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	if (ret != BCME_OK)
		return ret;

	replay = (wl_proxd_debug_data_t *)buff;

	if (index == 0)
		printf("\n/* Debug Informations */\n");

	printf("%s[%u,%u]: type %u action(%u,%u) token(%u, %u)\n",
		replay->received? "RX" : "TX",
		replay->count, replay->stage,
		replay->paket_type,
		replay->category, replay->action,
		replay->token, replay->follow_token);

	if (replay->tof_cmd == 0 && replay->tof_rsp == 0) {
		printf("\n");
		return BCME_OK;
	}

	printf("Index=%d\n", ltoh16(replay->index));
	printf("M_TOF_CMD=0x%04x\tM_TOF_RSP=0x%04x\tM_TOF_ID=0x%04x\n",
		ltoh16(replay->tof_cmd), ltoh16(replay->tof_rsp), ltoh16(replay->tof_id));
	printf("M_TOF_AVB_RX_L=0x%04x\tM_TOF_AVB_RX_H=0x%04x\t",
		ltoh16(replay->tof_avb_rxl), ltoh16(replay->tof_avb_rxh));
	printf("M_TOF_AVB_TX_L=0x%04x\tM_TOF_AVB_TX_H=0x%04x\n",
		ltoh16(replay->tof_avb_txl), ltoh16(replay->tof_avb_txh));
	printf("M_TOF_STATUS0=0x%04x\tM_TOF_STATUS2=0x%04x\t",
		ltoh16(replay->tof_status0), ltoh16(replay->tof_status2));
	printf("M_TOF_CHNSM_0=0x%04x\tM_TOF_CHNSM_1=0x%04x\n",
		ltoh16(replay->tof_chsm0), 0);
	printf("M_TOF_PHYCTL0=0x%04x\tM_TOF_PHYCTL1=0x%04x\tM_TOF_PHYCTL2=0x%04x\n",
		ltoh16(replay->tof_phyctl0), ltoh16(replay->tof_phyctl1),
		ltoh16(replay->tof_phyctl2));
	printf("M_TOF_LSIG=0x%04x\tM_TOF_VHTA0=0x%04x\tM_TOF_VHTA1=0x%04x\n",
		ltoh16(replay->tof_lsig), ltoh16(replay->tof_vhta0), ltoh16(replay->tof_vhta1));
	printf("M_TOF_VHTA2=0x%04x\tM_TOF_VHTB0=0x%04x\tM_TOF_VHTB1=0x%04x\n",
		ltoh16(replay->tof_vhta2), ltoh16(replay->tof_vhtb0), ltoh16(replay->tof_vhtb1));
	printf("M_TOF_AMPDU_CTL=0x%04x\tM_TOF_AMPDU_DLIM=0x%04x\tM_TOF_AMPDU_LEN=0x%04x\n\n",
		ltoh16(replay->tof_apmductl), ltoh16(replay->tof_apmdudlim),
		ltoh16(replay->tof_apmdulen));

	return BCME_OK;
}

static void
wlc_proxd_collec_header_dump(wl_proxd_collect_header_t *pHdr)
{
	int i;

	printf("total_frames %lu\n", (unsigned long)ltoh16(pHdr->total_frames));
	printf("nfft %lu\n", (unsigned long)ltoh16(pHdr->nfft));
	printf("bandwidth %lu\n", (unsigned long)ltoh16(pHdr->bandwidth));
	printf("channel %lu\n", (unsigned long)ltoh16(pHdr->channel));
	printf("chanspec %lu\n", (unsigned long)ltoh32(pHdr->chanspec));
	printf("fpfactor %lu\n", (unsigned long)ltoh32(pHdr->fpfactor));
	printf("fpfactor_shift %lu\n", (unsigned long)ltoh16(pHdr->fpfactor_shift));
	printf("distance %li\n", (long)ltoh32(pHdr->distance));
	printf("meanrtt %lu\n", (unsigned long)ltoh32(pHdr->meanrtt));
	printf("modertt %lu\n", (unsigned long)ltoh32(pHdr->modertt));
	printf("medianrtt %lu\n", (unsigned long)ltoh32(pHdr->medianrtt));
	printf("sdrtt %lu\n", (unsigned long)ltoh32(pHdr->sdrtt));
	printf("clkdivisor %lu\n", (unsigned long)ltoh32(pHdr->clkdivisor));
	printf("chipnum %lu\n", (unsigned long)ltoh16(pHdr->chipnum));
	printf("chiprev %lu\n", (unsigned long)pHdr->chiprev);
	printf("phyver %lu\n", (unsigned long)pHdr->phyver);
	printf("localMacAddr %s\n", wl_ether_etoa(&(pHdr->localMacAddr)));
	printf("remoteMacAddr %s\n", wl_ether_etoa(&(pHdr->remoteMacAddr)));
	printf("params_Ki %lu\n", (unsigned long)ltoh32(pHdr->params.Ki));
	printf("params_Kt %lu\n", (unsigned long)ltoh32(pHdr->params.Kt));
	printf("params_vhtack %li\n", (long)ltoh16(pHdr->params.vhtack));
	printf("params_N_log2 %d\n", TOF_BW_NUM);
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.N_log2[i]));
	}
	printf("params_N_scale %d\n", TOF_BW_NUM);
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.N_scale[i]));
	}
	printf("params_sw_adj %lu\n", (unsigned long)pHdr->params.sw_adj);
	printf("params_hw_adj %lu\n", (unsigned long)pHdr->params.hw_adj);
	printf("params_seq_en %lu\n", (unsigned long)pHdr->params.seq_en);
	printf("params_core %lu\n", (unsigned long)pHdr->params.core);
	printf("params_N_log2_seq 2\n");
	for (i = 0; i < 2; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.N_log2[i + TOF_BW_NUM]));
	}
	printf("params_N_scale_seq 2\n");
	for (i = 0; i < 2; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.N_scale[i + TOF_BW_NUM]));
	}
	printf("params_w_offset %d\n", TOF_BW_NUM);
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.w_offset[i]));
	};
	printf("params_w_len %d\n", TOF_BW_NUM);
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("%li\n", (long)ltoh16(pHdr->params.w_len[i]));
	};
	printf("params_maxDT %li\n", (long)ltoh32(pHdr->params.maxDT));
	printf("params_minDT %li\n", (long)ltoh32(pHdr->params.minDT));
	printf("params_totalfrmcnt %lu\n", (unsigned long)pHdr->params.totalfrmcnt);
	printf("params_rsv_media %lu\n", (unsigned long)ltoh16(pHdr->params.rsv_media));
}

static int wlc_proxd_collect_data_check_ver_len(uint16 expected_ver,
	uint16 ver, uint16 len)
{
	int ret = BCME_OK;

	switch (expected_ver) {
	case WL_PROXD_COLLECT_DATA_VERSION_1:
	case WL_PROXD_COLLECT_DATA_VERSION_2:
		/* v1 and v2 do not have version and length fields */
		ret = BCME_OK;
		break;
	case WL_PROXD_COLLECT_DATA_VERSION_3:
		if ((ver != expected_ver) ||
			(len != sizeof(wl_proxd_collect_data_t_v3) -
				OFFSETOF(wl_proxd_collect_data_t_v3, info))) {
			ret = BCME_VERSION;
		}
		break;
	default:
		ret = BCME_UNSUPPORTED;
	}

	return ret;
}

static int wlc_proxd_collec_data_dump(void *replay, FILE *fp,
	wl_proxd_rssi_bias_avg_t *rssi_bias_avg, uint16 version)
{
	int i, n, nbytes;
	int ret = BCME_OK;
	wl_proxd_collect_data_t_v1 *replay_v1 = NULL;
	wl_proxd_collect_data_t_v2 *replay_v2 = NULL;
	wl_proxd_collect_data_t_v3 *replay_v3 = NULL;
	wl_proxd_collect_info_t *info = NULL;
	uint32 *H = NULL;
	uint32 *chan = NULL;
	uint8 *ri_rr = NULL;
	int nfft = 0;
#ifdef RSSI_REFINE
	int rssi_dec = 0;
#else
	/* rssi_bias_avg is unused under !RSSI_REFINE. */
	/* Macro to prevent compiler warning under some platforms */
	UNUSED_PARAMETER(rssi_bias_avg);
#endif // endif

	BCM_REFERENCE(H);
	BCM_REFERENCE(chan);
	BCM_REFERENCE(ri_rr);

	/* Depending on version add the corresponding case and pointers to point
	 * to the correct member for use in the function
	 */
	switch (version) {
	case WL_PROXD_COLLECT_DATA_VERSION_1:
		replay_v1 = (wl_proxd_collect_data_t_v1 *)replay;
		info = &replay_v1->info;
		H = replay_v1->H;
		ri_rr = replay_v1->ri_rr;
		nbytes = sizeof(wl_proxd_collect_data_t_v1)
			- (K_TOF_COLLECT_H_SIZE_20MHZ - nfft) * sizeof(uint32);
		break;
	case WL_PROXD_COLLECT_DATA_VERSION_2:
		replay_v2 = (wl_proxd_collect_data_t_v2 *)replay;
		info = &replay_v2->info;
		H = replay_v2->H;
		ri_rr = replay_v2->ri_rr;
		nbytes = sizeof(wl_proxd_collect_data_t_v2)
			- (K_TOF_COLLECT_H_SIZE_20MHZ - nfft) * sizeof(uint32);
		break;
	case WL_PROXD_COLLECT_DATA_VERSION_3:
		replay_v3 = (wl_proxd_collect_data_t_v3 *)replay;
		ret = wlc_proxd_collect_data_check_ver_len(version, replay_v3->version,
			replay_v3->len);
		if (ret != BCME_OK) {
			break;
		}
		info = &replay_v3->info;
		H = replay_v3->H;
		ri_rr = replay_v3->ri_rr;
		chan = replay_v3->chan;
		nbytes = sizeof(wl_proxd_collect_data_t_v3)
			- (K_TOF_COLLECT_H_SIZE_20MHZ - nfft) * sizeof(uint32);
		break;
	default:
		ret = BCME_VERSION;
		break;
	}
	if (ret != BCME_OK) {
		goto done;
	}

	if (info) {
		nfft = (int)ltoh16(info->nfft);
#ifdef RSSI_REFINE
		if (nfft > 0) {
			printf("}\nImpulse Response = {\n");
			for (i = 0; i < nfft; i++) {
			printf("%010d ", ltoh32(info->rssi_bias.imp_resp[i]));
				if ((i & 7) == 7)
					printf("\n");
			}
			printf("RSSI_VERSION = %d\n", ltoh32(info->rssi_bias.version));
			printf("PEAK_OFFSET = %d\n", ltoh32(info->rssi_bias.peak_offset));
			rssi_bias_avg->avg_peak_offset += ltoh32(info->rssi_bias.peak_offset);
			printf("PEAK_TO_AVG = %d", ltoh32(info->rssi_bias.bias));
			rssi_bias_avg->avg_bias += ltoh32(info->rssi_bias.bias);
			printf("\n");
			for (i = 0; i < 10; i++) {
				printf("THRESHOLD_%d = %u", i,
					ltoh32(info->rssi_bias.threshold[i]));
				if ((i+1) % 5)
					printf(", ");
				else
					printf("\n");
				rssi_bias_avg->avg_threshold[i] +=
					ltoh32(info->rssi_bias.threshold[i]);
			}
			printf("SCALAR = %d", info->rssi_bias.threshold[10]);
		}

		/* convert tof_status2 from hex to dec */
		rssi_dec = info->tof_rssi;
		printf("\nRSSI10 = %d", rssi_dec);
		rssi_bias_avg->avg_rssi += rssi_dec;
		printf("\n\n");
#endif /* RSSI_REFINE */

		/* printing the info portion */
		printf("info_type %lu\n", (unsigned long)ltoh16(info->type));
		printf("info_index %lu\n", (unsigned long)ltoh16(info->index));
		printf("info_tof_cmd %lu\n", (unsigned long)ltoh16(info->tof_cmd));
		printf("info_tof_rsp %lu\n", (unsigned long)ltoh16(info->tof_rsp));
		printf("info_tof_avb_rxl %lu\n", (unsigned long)ltoh16(info->tof_avb_rxl));
		printf("info_tof_avb_rxh %lu\n", (unsigned long)ltoh16(info->tof_avb_rxh));
		printf("info_tof_avb_txl %lu\n", (unsigned long)ltoh16(info->tof_avb_txl));
		printf("info_tof_avb_txh %lu\n", (unsigned long)ltoh16(info->tof_avb_txh));
		printf("info_tof_id %lu\n", (unsigned long)ltoh16(info->tof_id));
		printf("info_tof_frame_type %lu\n", (unsigned long)info->tof_frame_type);
		printf("info_tof_frame_bw %lu\n", (unsigned long)info->tof_frame_bw);
		printf("info_tof_rssi %li\n", (long)info->tof_rssi);
		printf("info_tof_cfo %li\n", (long)ltoh32(info->tof_cfo));
		printf("info_gd_adj_ns %li\n", (long)ltoh32(info->gd_adj_ns));
		printf("info_gd_h_adj_ns %li\n", (long)ltoh32(info->gd_h_adj_ns));
		printf("info_nfft %li\n", (long)ltoh16(info->nfft));

		if (H) {
			/* printing the H portion */
			printf("H %d\n", nfft);
			for (i = 0; i < nfft; i++) {
				printf("%lu\n", (unsigned long)ltoh32(H[i]));
			}
		}

		/* printing the chan and ri_rr */
		if (info->index == 1) {
			if (chan && version >= WL_PROXD_COLLECT_DATA_VERSION_2) {
				n = (int)ltoh16((info->num_max_cores + 1)*
					(K_TOF_COLLECT_CHAN_SIZE >> (2 - info->tof_frame_bw)));
				printf("chan_est %d\n", n);
				for (i = 0; i < n; i++) {
					printf("%u\n", chan[i]);
				}
			}
			if (ri_rr && version >= WL_PROXD_COLLECT_DATA_VERSION_2) {
				printf("ri_rr %d\n", FTM_TPK_RI_RR_LEN_SECURE_2_0);
				for (i = 0; i < FTM_TPK_RI_RR_LEN_SECURE_2_0; i++) {
					printf("%u\n", ri_rr[i]);
				}
			} else {
				printf("ri_rr %d\n", FTM_TPK_RI_RR_LEN);
				for (i = 0; i < FTM_TPK_RI_RR_LEN; i++) {
					printf("%u\n", ri_rr[i]);
				}
			}
		}

		ret = fwrite(replay, 1, nbytes, fp);
		if (ret != nbytes) {
			fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				nbytes, ret);
			return BCME_ERROR;
		}
	}

done:
	return ret;
}

static int
wl_proxd_get_collect_data(void *wl, cmd_t *cmd, FILE *fp, int index,
	wl_proxd_rssi_bias_avg_t *rssi_bias_avg)
{
	int ret;
	void *buff;
	wl_proxd_collect_query_t query;
	uint16 expected_collect_data_ver = 0;

	/* check firmware version */
	/* Select collect data structure version based on FW version.
	 * Extend here for future versions
	 */
	if (wlc_ver_major(wl) <= 3) {
		/* use v1 */
		expected_collect_data_ver = WL_PROXD_COLLECT_DATA_VERSION_1;
	} else if (wlc_ver_major(wl) <= 5) {
		/* use v2 */
		expected_collect_data_ver = WL_PROXD_COLLECT_DATA_VERSION_2;
	} else {
		/* use v3 */
		expected_collect_data_ver = WL_PROXD_COLLECT_DATA_VERSION_3;
	}

	bzero(&query, sizeof(query));
	query.method = htol32(PROXD_TOF_METHOD);
	query.request = PROXD_COLLECT_QUERY_DATA;
	query.index = htol16(index);

	ret = wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	if (ret != BCME_OK)
		return ret;

	ret = wlc_proxd_collec_data_dump(buff, fp, rssi_bias_avg,
		expected_collect_data_ver);
	if (ret != BCME_OK) {
		return ret;
	}

	return BCME_OK;
}

static int
wl_proxd_collect(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	void *buff;
	wl_proxd_collect_query_t query, *pStatus;
	wl_proxd_collect_header_t *pHdr;
	const char *fname = "proxd_collect.dat";
	FILE *fp = NULL;
	int   i, collect_method, total_frames, load_request = 0, remote_request = 0;
	char chspec_str[CHANSPEC_STR_LEN];
	chanspec_t chanspec;
	float d_ref = -1;
#ifdef RSSI_REFINE
	wl_proxd_rssi_bias_avg_t rssi_bias_avg;

	bzero(&rssi_bias_avg, sizeof(rssi_bias_avg));
#endif // endif
	bzero(&query, sizeof(query));
	query.method = htol32(PROXD_TOF_METHOD);

	/* Skip the command name */
	argv++;
	while (*argv) {
		if (strcmp(argv[0], "disable") == 0 || *argv[0] == '0') {
			query.request = PROXD_COLLECT_SET_STATUS;
			query.status = 0;
			return wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
		}
		collect_method = (uint16)atoi(argv[0]);
		if (strcmp(argv[0], "enable") == 0) {
			query.request = PROXD_COLLECT_SET_STATUS;
			query.status = 1;
			return wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
		} else if (isdigit((int)*argv[0]) &&
				((collect_method >= WL_PROXD_COLLECT_METHOD_TYPE_DISABLE) &&
				(collect_method <= WL_PROXD_COLLECT_METHOD_TYPE_EVENT_LOG))) {
			query.request = PROXD_COLLECT_SET_STATUS;
			query.status = collect_method;
			return wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
		}
		if (!strcmp(argv[0], "-l")) {
			load_request = 1;
			argv++;
		}
		else if (!strcmp(argv[0], "-r")) {
			remote_request = 1;
			argv++;
		}
		else if (!strcmp(argv[0], "-f")) {
			if (argv[1] == NULL)
				return -1;
			fname = argv[1];
			argv += 2;
		}
		else if (!strcmp(argv[0], "-d")) {
			if (argv[1] == NULL)
				return -1;
			sscanf((const char*)argv[1], "%f", &d_ref);
			argv += 2;
		} else
			return -1;
	}

	query.request = PROXD_COLLECT_GET_STATUS;
	ret = wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	if (ret != BCME_OK)
		return ret;

	pStatus = (wl_proxd_collect_query_t *)buff;
	if (!pStatus->status) {
		printf("Disable\n");
		return BCME_OK;
	}

	if (pStatus->busy) {
		printf("Busy\n");
		return BCME_OK;
	}

	if ((pStatus->mode == WL_PROXD_MODE_TARGET) &&
		(remote_request || load_request)) {
		printf("Unsupport\n");
		return BCME_OK;
	}

	if (remote_request && !pStatus->remote) {
		printf("Remote data have not ready, please run this command again\n");
		load_request = 1;
		goto exit;
	}

	if (load_request && pStatus->remote) {
		printf("Local data have not ready, please run command 'proxd_find' to get it\n");
		goto exit;
	}

	query.request = PROXD_COLLECT_QUERY_HEADER;
	ret = wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	if (ret != BCME_OK)
		return ret;

	pHdr = (wl_proxd_collect_header_t *)buff;
	total_frames = (int)ltoh32(pHdr->total_frames);
	if (!total_frames) {
		printf("Enable\n");
		goto exit;
	}

	chanspec = wl_chspec_from_driver(pHdr->chanspec);
	wf_chspec_ntoa(chanspec, chspec_str);

	printf("d_ref %5.1f\n", d_ref);
	wlc_proxd_collec_header_dump(pHdr);

	if ((fp = fopen(fname, "wb")) == NULL) {
		fprintf(stderr, "Problem opening file %s\n", fname);
		return 0;
	}

	ret = fwrite(buff, 1, sizeof(wl_proxd_collect_header_t), fp);
	if (ret != sizeof(wl_proxd_collect_header_t)) {
		fprintf(stderr, "Error writing to file rc %d\n", ret);
		ret = -1;
		goto exit;
	}

	for (i = 0; i < total_frames; i++) {
#ifdef RSSI_REFINE
		ret = wl_proxd_get_collect_data(wl, cmd, fp, i, &rssi_bias_avg);
#else
		ret = wl_proxd_get_collect_data(wl, cmd, fp, i, NULL);
#endif // endif
		if (ret != BCME_OK)
			goto exit;
	}
#ifdef RSSI_REFINE
	if (total_frames > 0) {
		printf("avg_rssi = %d avg_peak_offset = %d\n",
			rssi_bias_avg.avg_rssi/total_frames,
			rssi_bias_avg.avg_peak_offset/total_frames);
		for (i = 0; i < 10; i++) {
			printf("avg_threshold_%d = %d",
				i, rssi_bias_avg.avg_threshold[i]/total_frames);
			if ((i+1) % 5)
				printf(", ");
			else
				printf("\n");
		}
		printf("avg_bias = %d\n", rssi_bias_avg.avg_bias/total_frames);
	}
#endif // endif
	for (i = 0; i < 256; i++) {
		ret = wl_proxd_get_debug_data(wl, cmd, i);
		if (ret != BCME_OK)
			break;
	}
	if (!load_request) {
		/* FTM remote collect done */
		query.request = PROXD_COLLECT_DONE;
		(void)wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	}
	ret = BCME_OK;
exit:
	if (ret == BCME_OK && load_request) {
		query.request = PROXD_COLLECT_REMOTE_REQUEST;
		ret = wlu_var_getbuf(wl, cmd->name, &query, sizeof(query), &buff);
	}
	if (fp) fclose(fp);
	return ret;
}

/* set measurement packet transmit rate */
static int proxd_method_set_vht_rate(miniopt_t *mopt)
{
	char *startp, *endp;
	char c;
	bool legacy_set = FALSE, ht_set = FALSE, vht_set = FALSE;
	int rate, mcs, Nss, tx_exp, bw, val;
	bool stbc, ldpc, sgi;
	uint32 rspec = 0;

	/* set default values */
	rate = 0;
	mcs = 0;
	Nss = 0;
	tx_exp = 0;
	stbc = FALSE;
	ldpc = FALSE;
	sgi = FALSE;
	bw = 0;

	startp = mopt->valstr;
	endp = NULL;
	if (*startp != 'h' && *startp != 'v') {
		if ((rate = (int)strtol(startp, &endp, 10)) == 0)
			return -1;

		rate *= 2;
		if (endp[0] == '.' && endp[1] == '5') {
			rate += 1;
			endp += 2;
		}
		startp = endp;
		legacy_set = TRUE;
	}

	while (startp && ((c = *startp++) != '\0')) {
		if (c == 'h') {
			ht_set = TRUE;
			mcs = (int)strtol(startp, &endp, 10);
			if (mcs < 0 || mcs > 23) {
				printf("HT MCS index %d out of range [0-23].\n", mcs);
				return -1;
			}
			startp = endp;
		}
		else if (c == 'v') {
			vht_set = TRUE;
			mcs = (int)strtol(startp, &endp, 10);
			if (mcs < 0 || mcs > 9) {
				printf("HT MCS index %d out of range [0-9].\n", mcs);
				return -1;
			}
			startp = endp;
		}
		else if (c == 'x') {
			Nss = (int)strtol(startp, &endp, 10);
			if (Nss < 1 || Nss > 8) {
				printf("Nss %d out of range [1-8].\n", Nss);
				return -1;
			}
			startp = endp;
		}
		else if (c == 'e') {
			tx_exp = (int)strtol(startp, &endp, 10);
			if (tx_exp < 0 || tx_exp > 3) {
				printf("tx expansion %d out of range [0-3].\n", tx_exp);
				return -1;
			}
			startp = endp;
		}
		else if (c == 's') {
			stbc = TRUE;
			continue;
		}
		else if (c == 'l') {
			ldpc = TRUE;
			continue;
		}
		else if (c == 'g') {
			sgi = TRUE;
			continue;
		}
		else if (c == 'b') {
			val = (int)strtol(startp, &endp, 10);
			if (val == 20) {
				bw = WL_RSPEC_BW_20MHZ;
			} else if (val == 40) {
				bw = WL_RSPEC_BW_40MHZ;
			} else if (val == 80) {
				bw = WL_RSPEC_BW_80MHZ;
			} else if (val == 160) {
				bw = WL_RSPEC_BW_160MHZ;
			} else {
				printf("unexpected bandwidth specified \"%d\", "
				        "expected 20, 40, 80, or 160\n", val);
				return -1;
			}
			startp = endp;
		}
	}

	if (!legacy_set && !ht_set && !vht_set) {
		printf("must specify one of legacy rate, HT (11n) rate hM, "
			"or VHT (11ac) rate vM[xS]\n");
		return -1;
	}

	if (legacy_set && (ht_set || vht_set)) {
		printf("cannot use legacy rate and HT rate or VHT rate at the same time\n");
		return -1;
	}

	if (ht_set && vht_set) {
		printf("cannot use HT rate hM and HT rate vM[xS] at the same time\n");
		return -1;
	}

	if (!vht_set && Nss != 0) {
		printf("cannot use xS option with non VHT rate\n");
		return -1;
	}

	if ((stbc || ldpc || sgi) && !(ht_set || vht_set)) {
		printf("cannot use STBC/LDPC/SGI options with non HT/VHT rates\n");
		return -1;
	}

	if (legacy_set) {
		rspec = WL_RSPEC_ENCODE_RATE;	/* 11abg */
		rspec |= rate;
	} else if (ht_set) {
		rspec = WL_RSPEC_ENCODE_HT; /* 11n HT */
		rspec |= mcs;
	} else {
		rspec = WL_RSPEC_ENCODE_VHT;	/* 11ac VHT */
		if (Nss == 0) {
			Nss = 1; /* default Nss = 1 if --ss option not given */
		}
		rspec |= (Nss << WL_RSPEC_VHT_NSS_SHIFT) | mcs;
	}

	/* set the other rspec fields */
	rspec |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	rspec |= bw;
	rspec |= (stbc ? WL_RSPEC_STBC : 0);
	rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
	rspec |= (sgi  ? WL_RSPEC_SGI  : 0);

	mopt->uval = rspec;

	return 0;
}

/* proxd set params common cmdl opts */
int proxd_method_set_common_param_from_opt(cmd_t *cmd,
	miniopt_t *mopt, wl_proxd_params_common_t *proxd_params)
{
	chanspec_t chanspec;

	if (mopt->opt == 'c') {
		/* chanspec iovar uses wl_chspec32_to_driver(chanspec), why ? */
		if ((chanspec = wf_chspec_aton(mopt->valstr)) == 0) {
			fprintf(stderr, "%s: could not parse \"%s\" as a channel\n",
				cmd->name, mopt->valstr);
			return BCME_BADARG;
		}

		proxd_params->chanspec
			= wl_chspec_to_driver(chanspec);

		if (proxd_params->chanspec == INVCHANSPEC) {
			fprintf(stderr,
				"%s: wl_chspec_to_driver() error \"%s\" \n",
				cmd->name, mopt->valstr);
			return BCME_BADARG;
		}
	} else if (mopt->opt == 't') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as a timeout\n",
				cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->timeout = htod16(mopt->val);
	}
	else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

/*  proxd  TOF cmdl ops  */
int proxd_method_tof_set_param_from_opt(cmd_t *cmd,
	miniopt_t *mopt, wl_proxd_params_tof_method_t *proxd_params)
{
	 if (mopt->opt == 'g') {
		/* this param is only valid for TOF method only */
		struct ether_addr ea;

		if (!wl_ether_atoe(mopt->valstr, &ea))  {
			fprintf(stderr,
				"%s: could not parse \"%s\" as MAC address\n",
			cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		memcpy(&proxd_params->tgt_mac, &ea, 6);
	} else if (mopt->opt == 'f') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as FTM frame count\n",
			cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->ftm_cnt = htod16(mopt->val);
	} else if (mopt->opt == 'y') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as Retry Count\n",
			cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->retry_cnt = htod16(mopt->val);
	}  else if (mopt->opt == 'r') {
		if (!mopt->good_int) {
			/* special case check for "-r 5.5" */
			if (!strcmp(mopt->valstr, "5.5")) {
				mopt->uval = 11;
			} else if (proxd_method_set_vht_rate(mopt)) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as a rate\n",
					cmd->name, mopt->valstr);
				return BCME_USAGE_ERROR;
			}
		} else
			 mopt->uval = mopt->uval*2;
		proxd_params->tx_rate = htod16((mopt->uval & 0xffff));
		proxd_params->vht_rate = htod16((mopt->uval >> 16));
	} else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

/*   RSSI method specific mdl opts   */
int proxd_method_rssi_set_param_from_opt(cmd_t *cmd,
	miniopt_t *mopt, wl_proxd_params_rssi_method_t *proxd_params)
{

	if (mopt->opt == 'i') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as an interval\n",
				cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->interval = htod16(mopt->val);
	} else if (mopt->opt == 'd') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as a duration\n",
				cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->duration = htod16(mopt->val);
	} else if (mopt->opt == 'p') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as a power\n",
			cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->tx_power = htod16(mopt->val);
	} else if (mopt->opt == 's') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as a RSSI\n",
				cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->rssi_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'm') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as a maxconvergetime\n",
				cmd->name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_params->maxconvergtmo = htod16(mopt->val);
	} else if (mopt->opt == 'r') {
		if (!mopt->good_int) {
			/* special case check for "-r 5.5" */
			if (!strcmp(mopt->valstr, "5.5")) {
				mopt->val = 11;
			} else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as a rate\n",
					cmd->name, mopt->valstr);
				return BCME_USAGE_ERROR;
			}
		} else
			mopt->val = mopt->val*2;
		proxd_params->tx_rate = htod16(mopt->val);
	}
	else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

/* proxd parse mixed param: <str0><val0><str1><val1>... */
static void
proxd_method_tof_parse_mixed_param(char* str, const char** p_name, int* p_val, int* p_map)
{
	char* p;

	/* parse stuff of format <str0><val0><str1><val1>... */
	while (*p_name) {
		p = strstr((const char*)str, (const char*)*p_name);
		if (p) {
			p += strlen((const char*)*p_name);
			*p_map = 1;
			*p_val = strtol(p, NULL, 10);
		} else {
			*p_map = 0;
		}

		p_name++;
		p_map++;
		p_val++;
	}
}

static int proxd_tune_set_params_from_opt_v1(const char* cmd_name, miniopt_t *mopt,
	wl_proxd_params_tof_tune_v1_t *proxd_tune)
{
	if (mopt->opt == 'k') {
		if (!mopt->good_int) {
			char *p = strstr(mopt->valstr, ",");
			if (p) {
				proxd_tune->Ki = htod32(atoi(mopt->valstr));
				proxd_tune->Kt = htod32(atoi(p+1));
			} else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as K\n",
					cmd_name, mopt->valstr);
				return BCME_USAGE_ERROR;
			}
		} else {
			proxd_tune->Ki = htod32(mopt->val);
			proxd_tune->Kt = htod32(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_K;
	} else if (mopt->opt == 'b') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as vhtack\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->vhtack = htod16(mopt->val);
	} else if (mopt->opt == 'c') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as core\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->core = htod16(mopt->val);
	} else if (mopt->opt == 'A') {
		const char* n[4] = {"s", "h", "r", NULL};
		int v[3] = {0, 0, 0};
		int m[3] = {0, 0, 0};

		proxd_method_tof_parse_mixed_param(mopt->valstr, n, v, m);
		if (m[TOF_ADJ_SOFTWARE]) {
			/* sw adj */
			proxd_tune->sw_adj = (int16)v[TOF_ADJ_SOFTWARE];
		}
		if (m[TOF_ADJ_HARDWARE]) {
			/* hw adj */
			proxd_tune->hw_adj = (int16)v[TOF_ADJ_HARDWARE];
		}
		if (m[TOF_ADJ_SEQ]) {
			/* ranging sequence */
			proxd_tune->seq_en = (int16)v[TOF_ADJ_SEQ];
		}

		if ((m[TOF_ADJ_SOFTWARE] | m[TOF_ADJ_HARDWARE] | m[TOF_ADJ_SEQ]) == 0) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as hw/sw adjustment enable params\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
	} else if (mopt->opt == 'n') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as min time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->minDT = htod32(mopt->val);
	} else if (mopt->opt == 'x') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as max time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->maxDT = htod32(mopt->val);
	} else if (mopt->opt == 'N') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_log2[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_log2_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_log2 = htod16(atoi(p));
			}

		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_log2[i] = htod16(mopt->val);
			}
			proxd_tune->N_log2_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_log2 = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_N;
	} else if (mopt->opt == 'S') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_scale[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_scale_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_scale = htod16(atoi(p));
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_scale[i] = htod16(mopt->val);
			}
			proxd_tune->N_scale_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_scale = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_S;
	} else if (mopt->opt == 'F') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->ftm_cnt[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->ftm_cnt[i] = htod16(mopt->val);
			}
		}
	} else if (mopt->opt == 't') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as total frmcnt\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->totalfrmcnt = (mopt->val);
	} else if (mopt->opt == 'r') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as media reserve value\n",
				cmd_name, mopt->valstr);
				return BCME_USAGE_ERROR;
		}
		proxd_tune->rsv_media = (mopt->val);
	} else if (mopt->opt == 'f') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as flags\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->flags = htod16(mopt->val);
	} else if (mopt->opt == 'W') {
		const char* n[4] = {"b", "l", "o", NULL};
		const char* s[4] = {"s", "l", "o", NULL};
		const char **p;
		int v[TOF_BW_NUM] = {0, 0, 0};
		int m[TOF_BW_NUM] = {0, 0, 0};
		int i;
		if (*mopt->valstr == 's')
			p = s;
		else
			p = n;
		proxd_method_tof_parse_mixed_param(mopt->valstr, p, v, m);
		if (m[0]) {
			/* Got bw */
			if (v[0] == TOF_BW_80MHZ)
				i = TOF_BW_80MHZ_INDEX;
			else if (v[0] == TOF_BW_40MHZ)
				i = TOF_BW_40MHZ_INDEX;
			else if (v[0] == TOF_BW_20MHZ)
				i = TOF_BW_20MHZ_INDEX;
			else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as window params\n",
					cmd_name, mopt->valstr);
					return BCME_USAGE_ERROR;
			}
			if (p == n) {
				/* Normal */
				if (m[1]) {
					/* Got length */
					proxd_tune->w_len[i] = (int16)v[1];
				}
				if (m[2]) {
					/* Got offset */
					proxd_tune->w_offset[i] = (int16)v[2];
				}
			} else {
				/* Seq */
				if (m[1] && i == TOF_BW_20MHZ_INDEX) {
					/* Got length */
					proxd_tune->seq_5g20.w_len = (int16)v[1];
					proxd_tune->seq_2g20.w_len = (int16)v[1];
				}
				if (m[2]&& i == TOF_BW_20MHZ_INDEX) {
					/* Got offset */
					proxd_tune->seq_5g20.w_offset = (int16)v[2];
					proxd_tune->seq_2g20.w_offset = (int16)v[2];
				}
			}
		}
	} else if (mopt->opt == 'B') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as bitflip_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->bitflip_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'R') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as snr_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->snr_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'T') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as recv_2g_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->recv_2g_thresh = mopt->val;
	} else if (mopt->opt == 'V') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"Group Delay Variance threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_gdv_thresh = htod32(mopt->val);
	} else if (mopt->opt == 'I') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"RSSI threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->acs_rssi_thresh = mopt->val;
	} else if (mopt->opt == 's') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as smoothing window "
				"enable\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->smooth_win_en = mopt->val;
	} else if (mopt->opt == 'e') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as emulator-delay\n ",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->emu_delay = mopt->val;
	} else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

static int proxd_tune_set_params_from_opt_v2(const char* cmd_name, miniopt_t *mopt,
	wl_proxd_params_tof_tune_v2_t *proxd_tune)
{
	if (mopt->opt == 'k') {
		if (!mopt->good_int) {
			char *p = strstr(mopt->valstr, ",");
			if (p) {
				proxd_tune->Ki = htod32(atoi(mopt->valstr));
				proxd_tune->Kt = htod32(atoi(p+1));
			}
			else {
			fprintf(stderr,
					"%s: could not parse \"%s\" as K\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
			}
		}
		else {
			proxd_tune->Ki = htod32(mopt->val);
			proxd_tune->Kt = htod32(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_K;
	} else if (mopt->opt == 'b') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as vhtack\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->vhtack = htod16(mopt->val);
	} else if (mopt->opt == 'c') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as core\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->core = htod16(mopt->val);
	} else if (mopt->opt == 'A') {
		const char* n[4] = {"s", "h", "r", NULL};
		int v[3] = {0, 0, 0};
		int m[3] = {0, 0, 0};

		proxd_method_tof_parse_mixed_param(mopt->valstr, n, v, m);
		if (m[TOF_ADJ_SOFTWARE]) {
			/* sw adj */
			proxd_tune->sw_adj = (int16)v[TOF_ADJ_SOFTWARE];
		}
		if (m[TOF_ADJ_HARDWARE]) {
			/* hw adj */
			proxd_tune->hw_adj = (int16)v[TOF_ADJ_HARDWARE];
		}
		if (m[TOF_ADJ_SEQ]) {
			/* ranging sequence */
			proxd_tune->seq_en = (int16)v[TOF_ADJ_SEQ];
		}

		if ((m[TOF_ADJ_SOFTWARE] | m[TOF_ADJ_HARDWARE] | m[TOF_ADJ_SEQ]) == 0) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as hw/sw adjustment enable params\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
	} else if (mopt->opt == 'n') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as min time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->minDT = htod32(mopt->val);
	} else if (mopt->opt == 'x') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as max time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->maxDT = htod32(mopt->val);
	} else if (mopt->opt == 'N') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_log2[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_log2_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_log2 = htod16(atoi(p));
			}

		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_log2[i] = htod16(mopt->val);
			}
			proxd_tune->N_log2_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_log2 = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_N;
	} else if (mopt->opt == 'S') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_scale[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_scale_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_scale = htod16(atoi(p));
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_scale[i] = htod16(mopt->val);
			}
			proxd_tune->N_scale_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_scale = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_S;
	} else if (mopt->opt == 'F') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->ftm_cnt[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->ftm_cnt[i] = htod16(mopt->val);
			}
		}
	} else if (mopt->opt == 't') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as total frmcnt\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->totalfrmcnt = (mopt->val);
	} else if (mopt->opt == 'r') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as media reserve value\n",
				cmd_name, mopt->valstr);
				return BCME_USAGE_ERROR;
			}
			proxd_tune->rsv_media = (mopt->val);
	} else if (mopt->opt == 'f') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as flags\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->flags = htod16(mopt->val);
	} else if (mopt->opt == 'W') {
		const char* n[4] = {"b", "l", "o", NULL};
		const char* s[4] = {"s", "l", "o", NULL};
		const char **p;
		int v[TOF_BW_NUM] = {0, 0, 0};
		int m[TOF_BW_NUM] = {0, 0, 0};
		int i;
		if (*mopt->valstr == 's')
			p = s;
		else
			p = n;
		proxd_method_tof_parse_mixed_param(mopt->valstr, p, v, m);
		if (m[0]) {
			/* Got bw */
			if (v[0] == TOF_BW_80MHZ)
				i = TOF_BW_80MHZ_INDEX;
			else if (v[0] == TOF_BW_40MHZ)
				i = TOF_BW_40MHZ_INDEX;
			else if (v[0] == TOF_BW_20MHZ)
				i = TOF_BW_20MHZ_INDEX;
			else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as window params\n",
					cmd_name, mopt->valstr);
					return BCME_USAGE_ERROR;
			}
			if (p == n) {
				/* Normal */
				if (m[1]) {
					/* Got length */
					proxd_tune->w_len[i] = (int16)v[1];
				}
				if (m[2]) {
					/* Got offset */
					proxd_tune->w_offset[i] = (int16)v[2];
				}
			} else {
				/* Seq */
				if (m[1] && i == TOF_BW_20MHZ_INDEX) {
					/* Got length */
					proxd_tune->seq_5g20.w_len = (int16)v[1];
					proxd_tune->seq_2g20.w_len = (int16)v[1];
				}
				if (m[2]&& i == TOF_BW_20MHZ_INDEX) {
					/* Got offset */
					proxd_tune->seq_5g20.w_offset = (int16)v[2];
					proxd_tune->seq_2g20.w_offset = (int16)v[2];
				}
			}
		}
	} else if (mopt->opt == 'B') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as bitflip_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->bitflip_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'R') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as snr_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->snr_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'T') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as recv_2g_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->recv_2g_thresh = mopt->val;
	} else if (mopt->opt == 'V') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"Group Delay Variance threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_gdv_thresh = htod32(mopt->val);
	} else if (mopt->opt == 'I') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"RSSI threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->acs_rssi_thresh = mopt->val;
	} else if (mopt->opt == 's') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as smoothing window "
				"enable\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->smooth_win_en = mopt->val;
	} else if (mopt->opt == 'M') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as auto core select "
				"group delay max - min threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_gdmm_thresh = htod32(mopt->val);
	} else if (mopt->opt == 'd') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"RSSI delta threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_delta_rssi_thresh = mopt->val;
	} else if (mopt->opt == 'e') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as emulator-delay\n ",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->emu_delay = mopt->val;
	} else if (mopt->opt == 'm') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as core-mask\n ",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val > 0x7) {
			fprintf(stderr,
				"proxd ftm tune:  core-mask can't be greater than 7  \n ");
			return BCME_USAGE_ERROR;
		}
		if (mopt->val == 0x0) {
			fprintf(stderr,
				"proxd ftm tune:  All cores can not be disabled, "
				"select at least one core.\n");
			return BCME_USAGE_ERROR;
		}
		proxd_tune->core_mask = mopt->val;
	} else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

static int proxd_tune_set_params_from_opt_v3(const char* cmd_name, miniopt_t *mopt,
	wl_proxd_params_tof_tune_v3_t *proxd_tune)
{
	if (mopt->opt == 'k') {
		if (!mopt->good_int) {
			char *p = strstr(mopt->valstr, ",");
			if (p) {
				proxd_tune->Ki = htod32(atoi(mopt->valstr));
				proxd_tune->Kt = htod32(atoi(p+1));
			}
			else {
			fprintf(stderr,
					"%s: could not parse \"%s\" as K\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
			}
		}
		else {
			proxd_tune->Ki = htod32(mopt->val);
			proxd_tune->Kt = htod32(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_K;
	} else if (mopt->opt == 'b') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as vhtack\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->vhtack = htod16(mopt->val);
	} else if (mopt->opt == 'c') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as core\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->core = htod16(mopt->val);
	} else if (mopt->opt == 'A') {
		const char* n[4] = {"s", "h", "r", NULL};
		int v[3] = {0, 0, 0};
		int m[3] = {0, 0, 0};

		proxd_method_tof_parse_mixed_param(mopt->valstr, n, v, m);
		if (m[TOF_ADJ_SOFTWARE]) {
			/* sw adj */
			proxd_tune->sw_adj = (int16)v[TOF_ADJ_SOFTWARE];
		}
		if (m[TOF_ADJ_HARDWARE]) {
			/* hw adj */
			proxd_tune->hw_adj = (int16)v[TOF_ADJ_HARDWARE];
		}
		if (m[TOF_ADJ_SEQ]) {
			/* ranging sequence */
			proxd_tune->seq_en = (int16)v[TOF_ADJ_SEQ];
		}

		if ((m[TOF_ADJ_SOFTWARE] | m[TOF_ADJ_HARDWARE] | m[TOF_ADJ_SEQ]) == 0) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as hw/sw adjustment enable params\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
	} else if (mopt->opt == 'n') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as min time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->minDT = htod32(mopt->val);
	} else if (mopt->opt == 'x') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as max time difference limitation\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->maxDT = htod32(mopt->val);
	} else if (mopt->opt == 'N') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_log2[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_log2_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_log2 = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_log2 = htod16(atoi(p));
			}

		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_log2[i] = htod16(mopt->val);
			}
			proxd_tune->N_log2_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_tx_log2 = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_log2 = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_N;
	} else if (mopt->opt == 'S') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->N_scale[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->N_scale_2g = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_5g20.N_rx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_tx_scale = htod16(atoi(p));
				p = strstr(p, ",");
				if (p)
					p++;
			}
			if (p) {
				proxd_tune->seq_2g20.N_rx_scale = htod16(atoi(p));
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->N_scale[i] = htod16(mopt->val);
			}
			proxd_tune->N_scale_2g = htod16(mopt->val);
			proxd_tune->seq_5g20.N_tx_scale = htod16(mopt->val);
			proxd_tune->seq_5g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
			proxd_tune->seq_2g20.N_rx_scale = htod16(mopt->val);
		}
		proxd_tune->setflags |= WL_PROXD_SETFLAG_S;
	} else if (mopt->opt == 'F') {
		int i = 0;
		if (!mopt->good_int) {
			char *p = mopt->valstr;
			while (p && i < TOF_BW_SEQ_NUM) {
				if (htod16(atoi(p)))
					proxd_tune->ftm_cnt[i] = htod16(atoi(p));
				i++;
				p = strstr(p, ",");
				if (p)
					p++;
			}
		} else {
			for (; i < TOF_BW_SEQ_NUM; i++) {
				proxd_tune->ftm_cnt[i] = htod16(mopt->val);
			}
		}
	} else if (mopt->opt == 't') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as total frmcnt\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->totalfrmcnt = (mopt->val);
	} else if (mopt->opt == 'r') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as media reserve value\n",
				cmd_name, mopt->valstr);
				return BCME_USAGE_ERROR;
			}
			proxd_tune->rsv_media = (mopt->val);
	} else if (mopt->opt == 'f') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"%s: could not parse \"%s\" as flags\n",
				cmd_name, mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->flags = htod16(mopt->val);
	} else if (mopt->opt == 'W') {
		const char* n[4] = {"b", "l", "o", NULL};
		const char* s[4] = {"s", "l", "o", NULL};
		const char **p;
		int v[TOF_BW_NUM] = {0, 0, 0};
		int m[TOF_BW_NUM] = {0, 0, 0};
		int i;
		if (*mopt->valstr == 's')
			p = s;
		else
			p = n;
		proxd_method_tof_parse_mixed_param(mopt->valstr, p, v, m);
		if (m[0]) {
			/* Got bw */
			if (v[0] == TOF_BW_80MHZ)
				i = TOF_BW_80MHZ_INDEX;
			else if (v[0] == TOF_BW_40MHZ)
				i = TOF_BW_40MHZ_INDEX;
			else if (v[0] == TOF_BW_20MHZ)
				i = TOF_BW_20MHZ_INDEX;
			else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as window params\n",
					cmd_name, mopt->valstr);
					return BCME_USAGE_ERROR;
			}
			if (p == n) {
				/* Normal */
				if (m[1]) {
					/* Got length */
					proxd_tune->w_len[i] = (int16)v[1];
				}
				if (m[2]) {
					/* Got offset */
					proxd_tune->w_offset[i] = (int16)v[2];
				}
			} else {
				/* Seq */
				if (m[1] && i == TOF_BW_20MHZ_INDEX) {
					/* Got length */
					proxd_tune->seq_5g20.w_len = (int16)v[1];
					proxd_tune->seq_2g20.w_len = (int16)v[1];
				}
				if (m[2]&& i == TOF_BW_20MHZ_INDEX) {
					/* Got offset */
					proxd_tune->seq_5g20.w_offset = (int16)v[2];
					proxd_tune->seq_2g20.w_offset = (int16)v[2];
				}
			}
		}
	} else if (mopt->opt == 'B') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as bitflip_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->bitflip_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'R') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as snr_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->snr_thresh = htod16(mopt->val);
	} else if (mopt->opt == 'T') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as recv_2g_threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->recv_2g_thresh = mopt->val;
	} else if (mopt->opt == 'V') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"Group Delay Variance threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_gdv_thresh = htod32(mopt->val);
	} else if (mopt->opt == 'I') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"RSSI threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val >= 0)
			return BCME_RANGE;
		proxd_tune->acs_rssi_thresh = mopt->val;
	} else if (mopt->opt == 's') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as smoothing window "
				"enable\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->smooth_win_en = mopt->val;
	} else if (mopt->opt == 'M') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as auto core select "
				"group delay max - min threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_gdmm_thresh = htod32(mopt->val);
	} else if (mopt->opt == 'd') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as Auto Core Select "
				"RSSI delta threshold\n",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->acs_delta_rssi_thresh = mopt->val;
	} else if (mopt->opt == 'e') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as emulator-delay\n ",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		proxd_tune->emu_delay = mopt->val;
	} else if (mopt->opt == 'm') {
		if (!mopt->good_int) {
			fprintf(stderr,
				"proxd ftm tune: could not parse \"%s\" as core-mask\n ",
				mopt->valstr);
			return BCME_USAGE_ERROR;
		}
		if (mopt->val > 0x7) {
			fprintf(stderr,
				"proxd ftm tune:  core-mask can't be greater than 7  \n ");
			return BCME_USAGE_ERROR;
		}
		if (mopt->val == 0x0) {
			fprintf(stderr,
				"proxd ftm tune:  All cores can not be disabled, "
				"select at least one core.\n");
			return BCME_USAGE_ERROR;
		}
		proxd_tune->core_mask = mopt->val;
	} else
		return BCME_USAGE_ERROR;

	return BCME_OK;
}

/*  proxd  TOF tune ops  */
static int
proxd_tune_set_param_from_opt(const char* cmd_name,
	miniopt_t *mopt, void *proxd_tune, int ver_major)
{
	int proxd_tune_expected_ver;
	int ret = BCME_OK;
	wl_proxd_params_tof_tune_v1_t * proxd_tune_v1 = NULL;
	wl_proxd_params_tof_tune_v2_t * proxd_tune_v2 = NULL;
	wl_proxd_params_tof_tune_v3_t * proxd_tune_v3 = NULL;

	proxd_tune_expected_ver = proxd_tune_ver_get(ver_major);
	switch (proxd_tune_expected_ver) {
	case WL_PROXD_TUNE_VERSION_MAJOR_1:
		proxd_tune_v1 = (wl_proxd_params_tof_tune_v1_t *)proxd_tune;
		ret = proxd_tune_set_params_from_opt_v1(cmd_name, mopt, proxd_tune_v1);
		break;
	case WL_PROXD_TUNE_VERSION_MAJOR_2:
		proxd_tune_v2 = (wl_proxd_params_tof_tune_v2_t *)proxd_tune;
		ret = proxd_tune_set_params_from_opt_v2(cmd_name, mopt, proxd_tune_v2);
		break;
	case WL_PROXD_TUNE_VERSION_MAJOR_3:
		proxd_tune_v3 = (wl_proxd_params_tof_tune_v3_t *)proxd_tune;
		ret = proxd_tune_set_params_from_opt_v3(cmd_name, mopt, proxd_tune_v3);
		break;
	default:
		ret = BCME_UNSUPPORTED;
		break;
	}

	return ret;
}

static int proxd_tune_ver_get_size(int expected_ver)
{
	int ret = 0;

	switch (expected_ver) {
	case WL_PROXD_TUNE_VERSION_MAJOR_1:
		ret = sizeof(wl_proxd_params_tof_tune_v1_t);
		break;
	case WL_PROXD_TUNE_VERSION_MAJOR_2:
		ret = sizeof(wl_proxd_params_tof_tune_v2_t);
		break;
	case WL_PROXD_TUNE_VERSION_MAJOR_3:
		ret = sizeof(wl_proxd_params_tof_tune_v3_t);
		break;
	default:
		ret = BCME_UNSUPPORTED;
		break;
	}
	return ret;
}

static int
wl_proxd_tune(void *wl, cmd_t *cmd, char **argv)
{
	wl_proxd_params_iovar_t proxd_tune, *reply;
	uint16 method;
	void *ptr = NULL;
	int ret, opt_err;
	miniopt_t to;
	int sizeof_tof_tune, proxd_tune_expected_ver;

	/* skip the command name and check if mandatory exists */
	if (!*++argv) {
		fprintf(stderr, "missing mandatory parameter \'method\'\n");
		return BCME_USAGE_ERROR;
	}

	/* parse method */
	method = (uint16)atoi(argv[0]);
	if (method == 0) {
		fprintf(stderr, "invalid parameter \'method\'\n");
		return BCME_USAGE_ERROR;
	}

	bzero(&proxd_tune, sizeof(proxd_tune));

	proxd_tune.method = htod16(method);
	ret = wlu_var_getbuf_sm(wl, cmd->name, &proxd_tune, sizeof(proxd_tune), &ptr);
	if (ret != BCME_OK) {
		return ret;
	}

	if (!*++argv) {
		/* get */
		proxd_tune_expected_ver = proxd_tune_ver_get(wlc_ver_major(wl));
		sizeof_tof_tune = proxd_tune_ver_get_size(proxd_tune_expected_ver);

		/* display proxd_params got */
		reply = (wl_proxd_params_iovar_t *)ptr;

		switch (proxd_tune.method) {
		case PROXD_RSSI_METHOD:
			break;

		case PROXD_TOF_METHOD:
			ret = proxd_tune_display(&reply->u.tof_tune, sizeof_tof_tune);
			break;

		default:
			fprintf(stderr,
				"%s: ERROR undefined method \n", cmd->name);
			return BCME_BADARG;
		}
	} else {
		/* set */
		memcpy((void *)&proxd_tune, (void *)ptr, sizeof(proxd_tune));
		proxd_tune.method = method;

		miniopt_init(&to, cmd->name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			int meth_res = BCME_USAGE_ERROR;

			if (opt_err == 1) {
				return BCME_USAGE_ERROR;
			}
			argv += to.consumed;

			/* method specific opts */
			switch (method) {
				case PROXD_RSSI_METHOD:
					break;

				case PROXD_TOF_METHOD:
					meth_res = proxd_tune_set_param_from_opt(cmd->name,
						&to, &proxd_tune.u.tof_tune, wlc_ver_major(wl));
					break;

				default:
					printf("ERROR: unsupported method\n");
					return BCME_USAGE_ERROR;
			}

			/*  if option is unknown to tune specific */
			if (meth_res != BCME_OK) {
				printf(">>>> Method:%d doesn't support cmd option:'%c'\n",
					method, to.opt);
				return meth_res;
			}
		}
		ret = wlu_var_setbuf(wl, cmd->name, &proxd_tune, sizeof(proxd_tune));
	}

	return ret;
}

/*   print or calculate & print location info   */
void wl_proxd_tof_host_calc(wl_proxd_event_data_t* evp)
{
	uint32 distance;
	uint32 meanrtt, modertt, medianrtt, dst_sigma;	/* standard deviation */
	int ftm_cnt;
	int16 avg_rssi, validfrmcnt;
	int32 var1, var2, var3;
	char diststr[40];

	distance = ntoh32(evp->distance);
	dst_sigma = ntoh32(evp->sdrtt);
	ftm_cnt = ntoh16(evp->ftm_cnt);
	avg_rssi = ntoh16(evp->avg_rssi);
	validfrmcnt = ntoh16(evp->validfrmcnt);
	meanrtt = ntoh32(evp->meanrtt);
	modertt = ntoh32(evp->modertt);
	medianrtt = ntoh32(evp->medianrtt);
	var1 = ntoh32(evp->var1);
	var2 = ntoh32(evp->var2);
	var3 = ntoh32(evp->var3);

	bzero(diststr, sizeof(diststr));
	if (distance == 0xffffffff)
		sprintf(diststr, "distance=-1m\n");
	else
		sprintf(diststr, "distance=%d.%04dm\n", distance>>4, (distance & 0xf) * 625);

	if (ntoh16(evp->mode) == WL_PROXD_MODE_TARGET) {
		if (evp->TOF_type == TOF_TYPE_REPORT) {
			printf("Report:(%s) and ", wl_ether_etoa(&evp->peer_mac));
			printf("(%s); %s", wl_ether_etoa((struct ether_addr *)
				&evp->peer_router_info), diststr);
		}
		else
		printf("Target:(%s); %s; ", wl_ether_etoa(&evp->peer_mac), diststr);
		printf("mean %d mode %d median %d\n", meanrtt, modertt, medianrtt);
	}
	else
		printf("Initiator:(%s); %s; ", wl_ether_etoa(&evp->peer_mac), diststr);

	printf("sigma:%d.%d;", dst_sigma/10, dst_sigma % 10);
	printf("rssi:%d validfrmcnt %d\n", avg_rssi, validfrmcnt);

	if (evp->TOF_type == TOF_TYPE_REPORT)
		printf("var3: %d\n", var3);
	else
	printf("var: %d %d %d\n", var1, var2, var3);

	if (ftm_cnt > 1) {
		int i;
		printf("event contains %d rtd samples for host side calculation:\n",
			ftm_cnt);

		for (i = 0; i < ftm_cnt; i++) {
			printf("ftm[%d] --> value:%d rssi:%d\n", i,
				ntoh32(evp->ftm_buff[i].value), evp->ftm_buff[i].rssi);
		}

		printf("host side calculation result: TBD\n");
		/* TODO: process raw samples */
		/*  e.g:
			1)  mean Mn = (S1+S2+... Sn)/N
			2)  variatin: Xn = (Sn - M)^2;
			3)  sqrt((X1 + X2 + .. Xn)/N)
		*/
	}
}

#if defined(linux)
/*   print timestamp info   */
static void wl_proxd_tof_ts_results(wl_proxd_event_ts_results_t* tsp)
{
	int16 ver;
	int tscnt;
	int i;

	ver = ntoh16(tsp->ver);
	tscnt = ntoh16(tsp->ts_cnt);
	if (ver == 2) {
		printf("Frame Count %d\n Timestamps:\n", tscnt);
		if (tscnt > 0) {
			for (i = 0; i < tscnt; i++) {
				printf("t1[%d]=%u : t2[%d]=%u : t3[%d]=%u : t4[%d]=%u\n",
					i, ntoh32(tsp->ts_buff[i].t1),
					i, ntoh32(tsp->ts_buff[i].t2),
					i, ntoh32(tsp->ts_buff[i].t3),
					i, ntoh32(tsp->ts_buff[i].t4));
			}
		}
	}
}

#define PROXD_EVENTS_BUFFER_SIZE 2048
static int
wl_proxd_event_check(void *wl, cmd_t *cmd, char **argv)
{
	bool exit_on1stresult = FALSE;
	int fd, err, octets;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	char ifnames[IFNAMSIZ] = {"eth0"};
	bcm_event_t *event;
	uint32 reason;
	uint16 mode; /* target or initiator */
	char* data;
	int event_type;
	uint8 event_inds_mask[WL_EVENTING_MASK_LEN];	/* 128-bit mask */
	wl_proxd_event_data_t* evp = NULL;
	wl_proxd_event_ts_results_t* tsp = NULL;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (argv[1] == NULL) {
		printf("<ifname> param is missing\n");
		return -1;
	}

	if (*++argv) {
		strncpy(ifnames, *argv, (IFNAMSIZ - 1));
	}

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	/*  read current mask state  */
	if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		printf("couldn't read event_msgs\n");
		return (err);
	}
	event_inds_mask[WLC_E_PROXD / 8] |= 1 << (WLC_E_PROXD % 8);
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);

	if (*++argv) {
		if (strcmp(*argv, "osh") == 0) {
			/* exit after processing 1st proximity result */;
			exit_on1stresult = TRUE;
		}
	}

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get iface:%s index \n", ifr.ifr_name);
		goto exit1;
	}

	bzero(&sll, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		goto exit1;
	}

	data = (char*)malloc(PROXD_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			PROXD_EVENTS_BUFFER_SIZE);
		goto exit1;
	}

	printf("waiting for LBS events :%s\n", ifr.ifr_name);

	while (1) {
		fflush(stdout);
		octets = recv(fd, data, PROXD_EVENTS_BUFFER_SIZE, 0);

		if (octets <= 0)  {
			/* sigterm */
			err = -1;
			break;
		}

		event = (bcm_event_t *)data;
		event_type = ntoh32(event->event.event_type);
		reason = ntoh32(event->event.reason);

#ifdef WL_FTM_DEBUG
		printf("%s: event_type 0x%x, ntoh32()=0x%x, ltoh32()=0x%x\n",
			__FUNCTION__, event->event.event_type, event_type,
			ltoh32(event->event.event_type));
#endif /* WL_FTM_DEBUG */

		if ((event_type != WLC_E_PROXD)) {
		/* may be other BCM events we are not interested in */
#ifdef WL_FTM_DEBUG
			printf("WARNING: not a proxd BCM_EVENT:%d\n", event_type);
#endif /* WL_FTM_DEBUG */
				continue;
		}

		/* check 'version' for FTM events */
		if (ftm_event_check((bcm_event_t *) data) == 0)
			continue;		/* continue to rx event */
		/* continue to handle non-FTM events */

		if (reason == WLC_E_PROXD_TS_RESULTS) {
			tsp = (wl_proxd_event_ts_results_t*)&data[sizeof(bcm_event_t)];
			mode = ntoh16(tsp->mode);
		} else {
		/* move to bcm event payload, which is proxd event structure */
		evp = (wl_proxd_event_data_t*)&data[sizeof(bcm_event_t)];
		mode = ntoh16(evp->mode);
		}

		printf("mode:%s; event:",
			(mode == WL_PROXD_MODE_INITIATOR)?"initiator":"target");

		switch (reason) {
		case WLC_E_PROXD_FOUND:
			printf("WLC_E_PROXD_FOUND; ");
			wl_proxd_tof_host_calc(evp); /* backward compatibility with RSSI method */
			break;
		case WLC_E_PROXD_GONE:
			printf("WLC_E_PROXD_GONE; ");
			break;
		case WLC_E_PROXD_START:
			/* event for targets / accesspoints  */
			printf("WLC_E_PROXD_START; ");
			break;
		case WLC_E_PROXD_STOP:
			printf("WLC_E_PROXD_STOP; ");
		break;
		case WLC_E_PROXD_COMPLETED:
			printf("WLC_E_PROXD_COMPLETED; ");
			/* all new method results should land here */
			wl_proxd_tof_host_calc(evp);
			if (exit_on1stresult)
				goto exit0;
			break;
		case WLC_E_PROXD_TS_RESULTS:
			printf("WLC_E_PROXD_TS_RESULTS; ");
			wl_proxd_tof_ts_results(tsp);
			if (exit_on1stresult)
				goto exit0;
			break;
		case WLC_E_PROXD_ERROR:
			printf("WLC_E_PROXD_ERROR:%d;", evp->err_code);
			/* all new method results should land here */
			wl_proxd_tof_host_calc(evp);
			if (exit_on1stresult)
				goto exit0;
			break;
		case WLC_E_PROXD_COLLECT_START:
			printf("WLC_E_PROXD_COLLECT_START; ");
			break;
		case WLC_E_PROXD_COLLECT_STOP:
			printf("WLC_E_PROXD_COLLECT_STOP; ");
			break;
		case WLC_E_PROXD_COLLECT_COMPLETED:
			printf("WLC_E_PROXD_COLLECT_COMPLETED; ");
			break;
		case WLC_E_PROXD_COLLECT_ERROR:
			printf("WLC_E_PROXD_COLLECT_ERROR; ");
			break;
		default:
			printf("ERROR: unsupported EVENT reason code:%d; ",
				reason);
			err = -1;
			break;
		}

		printf("\n");
	}
exit0:
	/* if we ever reach here */
	free(data);
exit1:
	close(fd);

	/* Read the event mask from driver and mask the event WLC_E_PROXD */
	if (!(err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		event_inds_mask[WLC_E_PROXD / 8] &= (~(1 << (WLC_E_PROXD % 8)));
		err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN);
	}

	fflush(stdout);
	return (err);
}
#endif /* linux */

/*
*  proxd ftm sub-commands info and handlers
*/
typedef struct ftm_subcmd_info ftm_subcmd_info_t;
typedef int (ftm_cmd_handler_t)(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv);

/* bitmask indicating which command groups; */
typedef enum {
	FTM_SUBCMD_FLAG_METHOD	= 0x01,	/* FTM method command */
	FTM_SUBCMD_FLAG_SESSION = 0x02,	/* FTM session command */
	FTM_SUBCMD_FLAG_ALL = FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION
} ftm_subcmd_flag_t;

struct ftm_subcmd_info {
	char				*name;		/* cmd-name string as cmdline input */
	wl_proxd_cmd_t		cmdid;		/* cmd-id */
	ftm_cmd_handler_t	*handler;	/* cmd-handler */
	ftm_subcmd_flag_t	cmdflag;
	char				*helpmsg;	/* messages for 'wl proxd -h ftm' */
};

#define FTM_IOC_BUFSZ  2048	/* ioc buffsize for our module (> BCM_XTLV_HDR_SIZE) */

/* declare proxd ftm-method command handlers ftm_subcmd_xxxx() */
/* e.g. static ftm_cmd_handler_t ftm_subcmd_get_version; */
#define FTM_SUBCMD_FUNC(suffix) ftm_subcmd_ ##suffix
#define DECL_CMDHANDLER(X) static ftm_cmd_handler_t ftm_subcmd_##X

/* get */
DECL_CMDHANDLER(get_version);		/* method-only */
DECL_CMDHANDLER(get_result);		/* session-only */
DECL_CMDHANDLER(get_info);			/* method + session */
DECL_CMDHANDLER(get_status);		/* method + session */
DECL_CMDHANDLER(get_sessions);		/* method-only */
DECL_CMDHANDLER(get_counters);		/* method + session */
DECL_CMDHANDLER(dump);				/* method + session */
DECL_CMDHANDLER(get_ranging_info);		/* method-only */

/* set */
DECL_CMDHANDLER(enable);			/* method-only */
DECL_CMDHANDLER(disable);			/* method-only */
DECL_CMDHANDLER(config);			/* method + session */
DECL_CMDHANDLER(start_session);		/* session-only */
DECL_CMDHANDLER(burst_request);		/* session-only */
DECL_CMDHANDLER(stop_session);		/* session-only */
DECL_CMDHANDLER(delete_session);	/* session-only */
DECL_CMDHANDLER(clear_counters);	/* method + session */
DECL_CMDHANDLER(start_ranging);		/* method-only */
DECL_CMDHANDLER(stop_ranging);		/* method-only */

DECL_CMDHANDLER(tune);			/* method-only both set/get */

static const ftm_subcmd_info_t ftm_cmdlist[] = {
	/* (get) wl proxd ftm ver */
	{"ver", WL_PROXD_CMD_GET_VERSION, FTM_SUBCMD_FUNC(get_version),
	FTM_SUBCMD_FLAG_METHOD, "Get the proxd FTM method API version information" },
	/* (get) wl proxd ftm <session-id> result */
	{"result", WL_PROXD_CMD_GET_RESULT, FTM_SUBCMD_FUNC(get_result),
	FTM_SUBCMD_FLAG_SESSION, "Get a session result" },
	/* (get) wl proxd ftm [<session-id>] info */
	{"info", WL_PROXD_CMD_GET_INFO, FTM_SUBCMD_FUNC(get_info),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Get the detail information for the FTM method or a specific session" },
	/* (get) wl proxd ftm [<session-id>] status */
	{"status", WL_PROXD_CMD_GET_STATUS, FTM_SUBCMD_FUNC(get_status),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Get the status for the FTM method or a specific session" },
	/* (get) wl proxd ftm sessions */
	{"sessions", WL_PROXD_CMD_GET_SESSIONS, FTM_SUBCMD_FUNC(get_sessions),
	FTM_SUBCMD_FLAG_METHOD, "List all sessions" },
	/* (get) wl proxd ftm [<session-id>] counters */
	{"counters", WL_PROXD_CMD_GET_COUNTERS, FTM_SUBCMD_FUNC(get_counters),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Get the counters for the FTM method or a specific session" },
	/* (get) wl proxd ftm ranging-info */
	{ "ranging-info", WL_PROXD_CMD_GET_RANGING_INFO, FTM_SUBCMD_FUNC(get_ranging_info),
	FTM_SUBCMD_FLAG_METHOD,
	"Get the ranging information for the FTM method" },
	/* (get) wl proxd ftm [<session-id>] dump */
	{"dump", WL_PROXD_CMD_DUMP, FTM_SUBCMD_FUNC(dump),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Dump all information for the FTM method or a specific session" },
	/* (set) wl proxd ftm enable */
	{"enable", WL_PROXD_CMD_ENABLE, FTM_SUBCMD_FUNC(enable),
	FTM_SUBCMD_FLAG_METHOD, "Enable proximity detection using FTM method" },
	/* (set) wl proxd ftm disable */
	{"disable", WL_PROXD_CMD_DISABLE, FTM_SUBCMD_FUNC(disable),
	FTM_SUBCMD_FLAG_METHOD, "Disable proximity detection using FTM method" },
	/* wl proxd ftm [<session-id>] config .... */
	{"config", WL_PROXD_CMD_CONFIG, FTM_SUBCMD_FUNC(config),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Set the configuration for the FTM method or a specific session" },
	/* (set) wl proxd ftm <session-id> start */
	{"start", WL_PROXD_CMD_START_SESSION, FTM_SUBCMD_FUNC(start_session),
	FTM_SUBCMD_FLAG_SESSION, "Start scheduling the burst(s) for a session" },
	/* (set) wl proxd ftm <session-id> burst-request */
	{"burst-request", WL_PROXD_CMD_BURST_REQUEST, FTM_SUBCMD_FUNC(burst_request),
	FTM_SUBCMD_FLAG_SESSION,
	"On initiator, send burst requests and process FTM frames. On target, send FTM frames" },
	/* (set) wl proxd ftm <session-id> stop */
	{"stop", WL_PROXD_CMD_STOP_SESSION, FTM_SUBCMD_FUNC(stop_session),
	FTM_SUBCMD_FLAG_SESSION, "Stop a session" },
	/* (set) wl proxd ftm <session-id> delete */
	{"delete", WL_PROXD_CMD_DELETE_SESSION, FTM_SUBCMD_FUNC(delete_session),
	FTM_SUBCMD_FLAG_SESSION, "Delete a session" },
	/* (set) wl proxd ftm [<session-id>] clear_counters */
	{"clear-counters", WL_PROXD_CMD_CLEAR_COUNTERS, FTM_SUBCMD_FUNC(clear_counters),
	FTM_SUBCMD_FLAG_METHOD | FTM_SUBCMD_FLAG_SESSION,
	"Clear the counters for the FTM method or a specific session" },
	{"start-ranging", WL_PROXD_CMD_START_RANGING, FTM_SUBCMD_FUNC(start_ranging),
	FTM_SUBCMD_FLAG_METHOD, "Start ranging for sessions" },
	{ "stop-ranging", WL_PROXD_CMD_STOP_RANGING, FTM_SUBCMD_FUNC(stop_ranging),
	FTM_SUBCMD_FLAG_METHOD, "Stop ranging" },
	{ "tune", WL_PROXD_CMD_TUNE, FTM_SUBCMD_FUNC(tune),
	FTM_SUBCMD_FLAG_METHOD, "proxd_tune" },
};

/*
* get subcmd info for a specific FTM 'cmdname'
* return NULL if specified <cmdname> is not supported.
*/
const ftm_subcmd_info_t *
ftm_get_subcmd_info(char *cmdname)
{
	int i;
	const ftm_subcmd_info_t *p_subcmd_info;

	/* search for the specified cmdname from the pre-defined supported cmd list */
	p_subcmd_info = &ftm_cmdlist[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_cmdlist); i++) {
		if (stricmp(p_subcmd_info->name, cmdname) == 0)
			return p_subcmd_info;
		p_subcmd_info++;
	}

	return NULL; /* cmd not supported */
}

/*
* 'wl proxd ascii-method' handler
*     handle 'wl proxd ftm [<session-id>] <cmdname> [<param-name><param-value>]*'
*/
static int
wl_proxd_cmd_method_handler(void *wl, cmd_t *cmd, char **argv)
{
	wl_proxd_method_t method;
	wl_proxd_session_id_t	session_id;
	const ftm_subcmd_info_t *p_subcmd_info;

	/* ignore the command name 'proxd' */
	UNUSED_PARAMETER(cmd);

	/* check for "wl proxd -h ftm" or "wl proxd help ftm" --> display the usage */
	if (!stricmp(*argv, "-h") || !stricmp(*argv, "help"))  {
		return ftm_handle_help(++argv); /* return BCME_USAGE_ERROR; */
	}

	/* parse proxd-method, currently only support 'ftm' */
	if (stricmp(*argv, "ftm") != 0) {			/* un-supported proxd-method */
		printf("error: proxd-method '%s' is not supported\n", *argv);
		return BCME_USAGE_ERROR;
	}
	method = WL_PROXD_METHOD_FTM;

	/* skip 'ftm-method' and parse session-id if specified */
	session_id = WL_PROXD_SESSION_ID_GLOBAL;
	if (*++argv) {
		if (isdigit((int)*argv[0])) {
			session_id = (wl_proxd_session_id_t) atoi(argv[0]);
			argv++;			/* skip to 'cmdname' */
		}
	}

	/* parse 'cmd' in "wl proxd ftm [<session-id>] <cmd> [<param-name><param-value>]*" */
	if (!*argv) {
		printf("error: proxd ftm-method cmdname is not specified\n");
		return BCME_USAGE_ERROR;
	}

	/* search for the specified <cmdname> from the pre-defined supported cmd list */
	p_subcmd_info = ftm_get_subcmd_info(*argv);
	if (p_subcmd_info == NULL) {
		/* cannot find the cmd in the ftm_cmdlist */
		printf("error: invalid proxd ftm command '%s'\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* dispacth cmd to appropriate handler */
	if (p_subcmd_info->handler) {
		/* For 'method-only' commands, 'session-id' can be omitted */
		if (((p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_METHOD) != 0) &&
			((p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_SESSION) == 0)) {
			if (session_id != WL_PROXD_SESSION_ID_GLOBAL) {
				printf("proxd ftm %s: error, does not accept non-zero session-id\n",
					p_subcmd_info->name);
				return BCME_USAGE_ERROR;
			}
		}

		/* For 'session-only' commands, 'session-id' must be provided */
		if (((p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_METHOD) == 0) &&
			((p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_SESSION) != 0)) {
			if (session_id == WL_PROXD_SESSION_ID_GLOBAL) {
				printf("proxd ftm %s: error, please specify a valid session-id\n",
					p_subcmd_info->name);
				return BCME_USAGE_ERROR;
			}
		}

		return p_subcmd_info->handler(wl, p_subcmd_info, method, session_id, ++argv);
	}

	return BCME_OK;

}

/*
* definition for id-string mapping.
*   This is used to map an id (can be cmd-id, tlv-id, ....) to a text-string
*   for debug-display or cmd-log-display
*/
typedef struct ftm_strmap_entry {
	int32		id;
	char		*text;
} ftm_strmap_entry_t;

/*
* lookup 'id' (as a key) from a table
* if found, return the entry pointer, otherwise return NULL
*/
static const ftm_strmap_entry_t*
ftm_get_strmap_info(int32 id, const ftm_strmap_entry_t *p_table, uint32 num_entries)
{
	int i;
	const ftm_strmap_entry_t *p_entry;

	/* scan thru the table till end */
	p_entry = p_table;
	for (i = 0; i < (int) num_entries; i++)
	{
		if (p_entry->id == id)
			return p_entry;
		p_entry++;		/* next entry */
	}

	return NULL;			/* not found */
}

/*
* lookup 'str' (as a key) from a table
* if found, return the entry pointer, otherwise return NULL
*/
static const ftm_strmap_entry_t*
ftm_get_strmap_info_strkey(char *strkey, const ftm_strmap_entry_t *p_table, uint32 num_entries)
{
	int i;
	const ftm_strmap_entry_t *p_entry;

	/* scan thru the table till end */
	p_entry = p_table;
	for (i = 0; i < (int) num_entries; i++)
	{
		if (stricmp(p_entry->text, strkey) == 0)
			return p_entry;
		p_entry++;		/* next entry */
	}

	return NULL;			/* not found */
}

/*
* map enum to a text-string for display, this function is called by the following:
* For debug/trace:
*     ftm_[cmdid|tlvid]_to_str()
* For TLV-output log for 'get' commands
*     ftm_[method|tmu|caps|status|state]_value_to_logstr()
* Input:
*     pTable -- point to a 'enum to string' table.
*/
static const char *
ftm_map_id_to_str(int32 id, const ftm_strmap_entry_t *p_table, uint32 num_entries)
{
	const ftm_strmap_entry_t*p_entry = ftm_get_strmap_info(id, p_table, num_entries);
	if (p_entry)
		return (p_entry->text);

	return "invalid";
}

/* define entry, e.g. { WL_PROXD_CMD_xxx, "WL_PROXD_CMD_xxx" } */
#define DEF_STRMAP_ENTRY(id) { (id), #id }

#ifdef WL_FTM_DEBUG
/* ftm cmd-id mapping */
static const ftm_strmap_entry_t ftm_cmdid_map[] = {
	/* {wl_proxd_cmd_t(WL_PROXD_CMD_xxx), "WL_PROXD_CMD_xxx" }, */
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_NONE),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_VERSION),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_ENABLE),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_DISABLE),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_CONFIG),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_START_SESSION),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_BURST_REQUEST),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_STOP_SESSION),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_DELETE_SESSION),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_RESULT),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_INFO),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_STATUS),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_SESSIONS),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_COUNTERS),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_CLEAR_COUNTERS),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_DUMP),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_START_RANGING),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_STOP_RANGING),
	DEF_STRMAP_ENTRY(WL_PROXD_CMD_GET_RANGING_INFO),
};

/*
* map a ftm cmd-id to a text-string for display
*/
static const char *
ftm_cmdid_to_str(uint16 cmdid)
{
	return ftm_map_id_to_str((int32) cmdid, &ftm_cmdid_map[0], ARRAYSIZE(ftm_cmdid_map));
}
#endif /* WL_FTM_DEBUG */

/* ftm tlv-id mapping */
static const ftm_strmap_entry_t ftm_tlvid_loginfo[] = {
	/* { WL_PROXD_TLV_ID_xxx,			"text for WL_PROXD_TLV_ID_xxx" }, */
	{ WL_PROXD_TLV_ID_NONE,				"none" },
	{ WL_PROXD_TLV_ID_METHOD,			"method" },
	{ WL_PROXD_TLV_ID_FLAGS,			"flags" },
	{ WL_PROXD_TLV_ID_CHANSPEC,			"chanspec" },
	{ WL_PROXD_TLV_ID_TX_POWER,			"tx power" },
	{ WL_PROXD_TLV_ID_RATESPEC,			"ratespec" },
	{ WL_PROXD_TLV_ID_BURST_DURATION,		"burst duration" },
	{ WL_PROXD_TLV_ID_BURST_PERIOD,			"burst period" },
	{ WL_PROXD_TLV_ID_BURST_FTM_SEP,		"burst ftm sep" },
	{ WL_PROXD_TLV_ID_BURST_NUM_FTM,		"burst num ftm" },
	{ WL_PROXD_TLV_ID_NUM_BURST,			"num burst" },
	{ WL_PROXD_TLV_ID_FTM_RETRIES,			"ftm retries" },
	{ WL_PROXD_TLV_ID_BSS_INDEX,			"BSS index" },
	{ WL_PROXD_TLV_ID_BSSID,			"bssid" },
	{ WL_PROXD_TLV_ID_INIT_DELAY,			"burst init delay" },
	{ WL_PROXD_TLV_ID_BURST_TIMEOUT,		"burst timeout" },
	{ WL_PROXD_TLV_ID_EVENT_MASK,			"event mask" },
	{ WL_PROXD_TLV_ID_FLAGS_MASK,			"flags mask" },
	{ WL_PROXD_TLV_ID_PEER_MAC,			"peer addr" },
	{ WL_PROXD_TLV_ID_FTM_REQ,			"ftm req" },
	{ WL_PROXD_TLV_ID_LCI_REQ,			"lci req" },
	{ WL_PROXD_TLV_ID_LCI,				"lci" },
	{ WL_PROXD_TLV_ID_CIVIC_REQ,			"civic req" },
	{ WL_PROXD_TLV_ID_CIVIC,			"civic" },
	{ WL_PROXD_TLV_ID_AVAIL24,			"availability" },
	{ WL_PROXD_TLV_ID_SESSION_FLAGS,		"session flags" },
	{ WL_PROXD_TLV_ID_SESSION_FLAGS_MASK,		"session flags mask" },
	{ WL_PROXD_TLV_ID_RX_MAX_BURST,			"rx max bursts" },
	{ WL_PROXD_TLV_ID_RANGING_INFO,			"ranging info" },
	{ WL_PROXD_TLV_ID_RANGING_FLAGS,		"ranging flags" },
	{ WL_PROXD_TLV_ID_RANGING_FLAGS_MASK,		"ranging flags mask" },
	{ WL_PROXD_TLV_ID_DEV_ADDR,			"dev addr" },
	{ WL_PROXD_TLV_ID_CUR_ETHER_ADDR,		"source addr for Tx" },
	{ WL_PROXD_TLV_ID_AVAIL,			"availability" },
	{ WL_PROXD_TLV_ID_FTM_REQ_RETRIES,		"ftm request retries" },
	{ WL_PROXD_TLV_ID_TPK,				"FTM TPK" },

	/* output - 512 + x */
	{ WL_PROXD_TLV_ID_STATUS,			"status" },
	{ WL_PROXD_TLV_ID_COUNTERS,			"counters" },
	{ WL_PROXD_TLV_ID_INFO,				"info" },
	{ WL_PROXD_TLV_ID_RTT_RESULT,			"rtt result" },
	{ WL_PROXD_TLV_ID_AOA_RESULT,			"aoa result" },
	{ WL_PROXD_TLV_ID_SESSION_INFO,			"session info" },
	{ WL_PROXD_TLV_ID_SESSION_STATUS,		"session status" },
	{ WL_PROXD_TLV_ID_SESSION_ID_LIST,		"session ids" },
	{ WL_PROXD_TLV_ID_RTT_RESULT_V2,		"rtt result" },

	/* debug tlvs can be added starting 1024 */
	{ WL_PROXD_TLV_ID_DEBUG_MASK,			"debug mask" },
	{ WL_PROXD_TLV_ID_COLLECT,			"collect" },
	{ WL_PROXD_TLV_ID_STRBUF,			"result" }
};

/*
* map a ftm TLV-id to a text-string for display as a 'heading'
*/
static const char *
ftm_tlvid_to_logstr(uint16 tlvid)
{
	return ftm_map_id_to_str((int32) tlvid,
		&ftm_tlvid_loginfo[0], ARRAYSIZE(ftm_tlvid_loginfo));
}

/*
* The following string mapping tables are used for TLVs display received from 'get' commands
*/
/*
* proximity detection method --> text string mapping
*/
static const ftm_strmap_entry_t ftm_method_value_loginfo[] = {
	/* wl_proxd_method_t,		text-string */
	{ WL_PROXD_METHOD_RSVD1,	"RSSI not supported" },
	{ WL_PROXD_METHOD_TOF,		"11v+BCM proprietary" },
	{ WL_PROXD_METHOD_RSVD2,	"11v only" },
	{ WL_PROXD_METHOD_FTM,		"FTM" },		/* IEEE rev mc/2014 */
	{ WL_PROXD_METHOD_NONE,		"none" }
};
static const char *
ftm_method_value_to_logstr(wl_proxd_method_t method)
{
	return ftm_map_id_to_str((int32)method,
		&ftm_method_value_loginfo[0], ARRAYSIZE(ftm_method_value_loginfo));
}

/*
* time interval unit --> text string mapping
*/
static const ftm_strmap_entry_t ftm_tmu_value_loginfo[] = {
	/* wl_proxd_tmu_t,		text-string */
	{ WL_PROXD_TMU_TU,		"TU" },
	{ WL_PROXD_TMU_SEC,		"sec" },
	{ WL_PROXD_TMU_MILLI_SEC,	"ms" },
	{ WL_PROXD_TMU_MICRO_SEC,	"us" },
	{ WL_PROXD_TMU_NANO_SEC,	"ns" },
	{ WL_PROXD_TMU_PICO_SEC,	"ps" }
};

static const char *
ftm_tmu_value_to_logstr(wl_proxd_tmu_t tmu)
{
	return ftm_map_id_to_str((int32)tmu,
		&ftm_tmu_value_loginfo[0], ARRAYSIZE(ftm_tmu_value_loginfo));
}

/*
* proxd FTM method capabilities --> text string mapping
*/
static const ftm_strmap_entry_t ftm_caps_value_loginfo[] = {
	/* wl_proxd_ftm_capts_t,	text-string */
	{ WL_PROXD_FTM_CAP_FTM1,	"FTM" },
	{ WL_PROXD_FTM_CAP_NONE,	"none" }
};

static const char *
ftm_caps_value_to_logstr(wl_proxd_ftm_caps_t caps)
{
	return ftm_map_id_to_str((int32)caps,
		&ftm_caps_value_loginfo[0], ARRAYSIZE(ftm_caps_value_loginfo));
}

/*
* status --> text string mapping
*/
static const ftm_strmap_entry_t ftm_status_value_loginfo[] = {
	/* wl_proxd_status_t,			text-string */
	{ WL_PROXD_E_NOT_BCM,			"different vendor" },
	{ WL_PROXD_E_FRAME_TYPE,		"invalid frame type" },
	{ WL_PROXD_E_VERNOSUPPORT,		"unsupported version" },
	{ WL_PROXD_E_SEC_NOKEY,			"no key" },
	{ WL_PROXD_E_SEC_POLICY,		"security policy violation" },
	{ WL_PROXD_E_SCAN_INPROCESS,		"scan in process" },
	{ WL_PROXD_E_BAD_PARTIAL_TSF,		"bad partial TSF" },
	{ WL_PROXD_E_SCANFAIL,			"scan failed" },
	{ WL_PROXD_E_NOTSF,			"no TSF" },
	{ WL_PROXD_E_POLICY,			"policy failed" },
	{ WL_PROXD_E_INCOMPLETE,		"incomplete" },
	{ WL_PROXD_E_OVERRIDDEN,		"overridden" },
	{ WL_PROXD_E_ASAP_FAILED,		"ASAP failed" },
	{ WL_PROXD_E_NOTSTARTED,		"not started" },
	{ WL_PROXD_E_INVALIDMEAS,		"invalid measurement" },
	{ WL_PROXD_E_INCAPABLE,			"incapable" },
	{ WL_PROXD_E_MISMATCH,			"mismatch"},
	{ WL_PROXD_E_DUP_SESSION,		"dup session" },
	{ WL_PROXD_E_REMOTE_FAIL,		"remote fail" },
	{ WL_PROXD_E_REMOTE_INCAPABLE,		"remote incapable" },
	{ WL_PROXD_E_SCHED_FAIL,		"sched failure" },
	{ WL_PROXD_E_PROTO,			"protocol error" },
	{ WL_PROXD_E_EXPIRED,			"expired" },
	{ WL_PROXD_E_TIMEOUT,			"timeout" },
	{ WL_PROXD_E_NOACK,			"no ack" },
	{ WL_PROXD_E_DEFERRED,			"deferred" },
	{ WL_PROXD_E_INVALID_SID,		"invalid session id" },
	{ WL_PROXD_E_REMOTE_CANCEL,		"remote cancel" },
	{ WL_PROXD_E_CANCELED,			"canceled" },
	{ WL_PROXD_E_INVALID_SESSION,		"invalid session" },
	{ WL_PROXD_E_BAD_STATE,			"bad state" },
	{ WL_PROXD_E_ERROR,			"error" },
	{ WL_PROXD_E_OK,			"OK" }
};

/*
* convert BCME_xxx error codes into related error strings
* note, bcmerrorstr() defined in bcmutils is for BCMDRIVER only,
*       this duplicate copy is for WL access and may need to clean up later
*/
static const char *ftm_bcmerrorstrtable[] = BCMERRSTRINGTABLE;
static const char *
ftm_status_value_to_logstr(wl_proxd_status_t status)
{
	static char ftm_msgbuf_status_undef[32];
	const ftm_strmap_entry_t *p_loginfo;
	int bcmerror;

	/* check if within BCME_xxx error range */
	bcmerror = (int) status;
	if (VALID_BCMERROR(bcmerror))
		return ftm_bcmerrorstrtable[-bcmerror];

	/* otherwise, look for 'proxd ftm status' range */
	p_loginfo = ftm_get_strmap_info((int32) status,
		&ftm_status_value_loginfo[0], ARRAYSIZE(ftm_status_value_loginfo));
	if (p_loginfo)
		return p_loginfo->text;

	/* report for 'out of range' FTM-status error code */
	memset(ftm_msgbuf_status_undef, 0, sizeof(ftm_msgbuf_status_undef));
	snprintf(ftm_msgbuf_status_undef, sizeof(ftm_msgbuf_status_undef),
		"Undefined status %d", status);
	return &ftm_msgbuf_status_undef[0];
}

/*
* session-state --> text string mapping
*/
static const ftm_strmap_entry_t ftm_session_state_value_loginfo[] = {
	/* wl_proxd_session_state_t,			text string */
	{ WL_PROXD_SESSION_STATE_CREATED,		"created" },
	{ WL_PROXD_SESSION_STATE_CONFIGURED,		"configured" },
	{ WL_PROXD_SESSION_STATE_STARTED,		"started" },
	{ WL_PROXD_SESSION_STATE_DELAY,			"delay" },
	{ WL_PROXD_SESSION_STATE_USER_WAIT,		"user-wait" },
	{ WL_PROXD_SESSION_STATE_SCHED_WAIT,		"sched-wait" },
	{ WL_PROXD_SESSION_STATE_BURST,			"burst" },
	{ WL_PROXD_SESSION_STATE_STOPPING,		"stopping" },
	{ WL_PROXD_SESSION_STATE_ENDED,			"ended" },
	{ WL_PROXD_SESSION_STATE_START_WAIT,		"start-wait" },
	{ WL_PROXD_SESSION_STATE_DESTROYING,		"destroying" },
	{ WL_PROXD_SESSION_STATE_NONE,			"none" }
};

static const char *
ftm_session_state_value_to_logstr(wl_proxd_session_state_t state)
{
	return ftm_map_id_to_str((int32)state, &ftm_session_state_value_loginfo[0],
		ARRAYSIZE(ftm_session_state_value_loginfo));
}

/*
* ranging-state --> text string mapping
*/
static const ftm_strmap_entry_t ftm_ranging_state_value_loginfo [] = {
	/* wl_proxd_ranging_state_t,		text string */
	{ WL_PROXD_RANGING_STATE_NONE,		"none" },
	{ WL_PROXD_RANGING_STATE_NOTSTARTED,	"nonstarted" },
	{ WL_PROXD_RANGING_STATE_INPROGRESS,	"inprogress" },
	{ WL_PROXD_RANGING_STATE_DONE,		"done" },
};

static const char *
ftm_ranging_state_value_to_logstr(wl_proxd_ranging_state_t state)
{
	return ftm_map_id_to_str((int32) state, &ftm_ranging_state_value_loginfo[0],
		ARRAYSIZE(ftm_ranging_state_value_loginfo));
}

/*
* ranging-flags --> text string mapping
*/
static const ftm_strmap_entry_t ftm_ranging_flags_value_loginfo [] = {
	/* wl_proxd_ranging_flags_t,			text string */
	{ WL_PROXD_RANGING_FLAG_NONE,			"none" },  /* no flags */
	{ WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP,	"del sessions on stop" },
	{ WL_PROXD_RANGING_FLAG_ALL,			"all" },
};

static const char *
ftm_ranging_flags_value_to_logstr(wl_proxd_ranging_flags_t flags)
{
	return ftm_map_id_to_str((int32) flags, &ftm_ranging_flags_value_loginfo[0],
		ARRAYSIZE(ftm_ranging_flags_value_loginfo));
}

/*
* availability flag --> text string mapping
*/
static const ftm_strmap_entry_t ftm_avail_flags_value_loginfo [] = {
	/* wl_proxd_avail_flags_t,		text string */
	{ WL_PROXD_AVAIL_NONE,			"none" },
	{ WL_PROXD_AVAIL_SCHEDULED,		"scheduled" }	/* scheduled by proxd */
};

static const char *
ftm_avail_flags_value_to_logstr(wl_proxd_avail_flags_t flags)
{
	return ftm_map_id_to_str((int32) flags, &ftm_avail_flags_value_loginfo[0],
			ARRAYSIZE(ftm_avail_flags_value_loginfo));
}

/*
* availability time-ref --> text string mapping
* (use for logging and cmd-line params parsing)
*/
static const ftm_strmap_entry_t ftm_avail_timeref_value_loginfo [] = {
	/* wl_proxd_time_ref_t,			text string */
	{ WL_PROXD_TREF_NONE,			"none" },
	{ WL_PROXD_TREF_DEV_TSF,		"dev-tsf" },
	{ WL_PROXD_TREF_TBTT,			"tbtt" }
};

static const char *
ftm_avail_timeref_value_to_logstr(wl_proxd_time_ref_t timeref)
{
	return ftm_map_id_to_str((int32) timeref, &ftm_avail_timeref_value_loginfo[0],
			ARRAYSIZE(ftm_avail_timeref_value_loginfo));
}

/*
* allocate a buffer for get/set 'proxd ftm' iovar
*
* Input:
*       tlvs_bufsize: specify the max-size of all TLVs reserved for this buffer
*       The following fields will be used for setting the proxd-method iovar header:
*             method, session_id, cmdid
*
* if succeeds, this function allocates a buffer, setup proxd-method iovar header
*              then returns the pointer to allocated buffer and the bufsize(in bytes)
*              to caller. Note, the 'len' and a dummy-TLV will be set in the iovar header,
*              caller may adjust the 'len' based on #of valid TLVs later.
* if failed, return 'NULL' to indicate error (e.g. no memory).
*/
static wl_proxd_iov_t *
ftm_alloc_getset_buf(wl_proxd_method_t method, wl_proxd_session_id_t session_id,
	wl_proxd_cmd_t cmdid, uint16 tlvs_bufsize, uint16 *p_out_bufsize)
{
	uint16 proxd_iovsize;
	wl_proxd_tlv_t *p_tlv;
	wl_proxd_iov_t *p_proxd_iov = (wl_proxd_iov_t *) NULL;

	*p_out_bufsize = 0;	/* init */

	/* calculate the whole buffer size, including one reserve-tlv entry in the header */
	proxd_iovsize = sizeof(wl_proxd_iov_t) + tlvs_bufsize;

	p_proxd_iov = calloc(1, proxd_iovsize);
	if (p_proxd_iov == NULL) {
		printf("error: failed to allocate %d bytes of memory\n", proxd_iovsize);
		return NULL;
	}

	/* setup proxd-FTM-method iovar header */
	p_proxd_iov->version = htol16(WL_PROXD_API_VERSION);
	p_proxd_iov->len = htol16(proxd_iovsize); /* caller may adjust it based on #of TLVs */
	p_proxd_iov->cmd = htol16(cmdid);
	p_proxd_iov->method = htol16(method);
	p_proxd_iov->sid = htol16(session_id);

	/* initialize the reserved/dummy-TLV in iovar header */
	p_tlv = p_proxd_iov->tlvs;
	p_tlv->id = htol16(WL_PROXD_TLV_ID_NONE);
	p_tlv->len = htol16(0);

	*p_out_bufsize = proxd_iovsize;	/* for caller's reference */

	return p_proxd_iov;
}

/*
* unpack and display rtt_result TLV
*/

#define PDFTM_BURST_STATE_NAMES \
	{"INVALID", \
	"FTM2/M1/RESPONSE", \
	"FTM3/M2/LTFTRIGGER", \
	"FTM4/M3/DONE", \
	}

#define FTM_FRAME_TYPES \
	{"SETUP", "TRIGGER", "TIMESTAMP"}

static void
ftm_unpack_and_display_rtt_result_v1(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	int		i;
	char	dispbuf[256];
	char	*p_dest;
	wl_proxd_result_flags_t flags;
	wl_proxd_session_state_t session_state;
	int32 avg_dist;
	wl_proxd_rtt_result_v1_t *p_data_info;
	wl_proxd_rtt_sample_v1_t *p_sample;
	wl_proxd_status_t status;
	uint16 num_rtt;
	char* pstatestr[] = PDFTM_BURST_STATE_NAMES;
	char *ftm_frame_types[] =  FTM_FRAME_TYPES;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_rtt_result_v1_t *) p_data;

	/* unpack and format 'flags' for display */
	flags = ltoh16_ua(&p_data_info->flags);
	memset(dispbuf, 0, sizeof(dispbuf));
	p_dest = &dispbuf[0];
	if (flags & WL_PROXD_RESULT_FLAG_NLOS) {
		strncpy(p_dest, "NLOS", sizeof(dispbuf) - strlen(dispbuf) - 1);
		p_dest = &dispbuf[strlen(dispbuf)];
	}
	if (flags & WL_PROXD_RESULT_FLAG_LOS)
		strncpy(p_dest, " LOS", sizeof(dispbuf) - strlen(dispbuf) - 1);
	if (flags & WL_PROXD_RESULT_FLAG_FATAL)
		strncpy(p_dest, " Fatal error", sizeof(dispbuf) - strlen(dispbuf) - 1);

	printf("> %s:\n>\tsessionId=%d, flags=0x%04x('%s'), peer=%s\n",
		ftm_tlvid_to_logstr(tlvid),
		ltoh16_ua(&p_data_info->sid),
		flags,
		dispbuf,	/* flags */
		wl_ether_etoa(&p_data_info->peer));

	/* session state and status */
	session_state = ltoh16_ua(&p_data_info->state);
	status = ltoh32_ua(&p_data_info->status);
	printf(">\tsession state=%d(%s) status=%d(%s)\n",
		session_state, ftm_session_state_value_to_logstr(session_state),
		status, ftm_status_value_to_logstr(status));

	printf(">\tburst_duration: %d%s\n",
		ltoh32_ua(&p_data_info->u.burst_duration.intvl),
		ftm_tmu_value_to_logstr(ltoh16_ua(&p_data_info->u.burst_duration.tmu)));

	/* show avg_dist (1/256m units), burst_num */
	avg_dist = ltoh32_ua(&p_data_info->avg_dist);
	if ((uint32)avg_dist == 0xffffffff) {	/* report 'failure' case */
		printf(">\tavg_dist=-1m, burst_num=%d, valid_measure_cnt=%d num_ftm=%d\n",
		ltoh16_ua(&p_data_info->burst_num),
		p_data_info->num_valid_rtt, p_data_info->num_ftm); /* in a session */
	}
	else {
		printf(">\tavg_dist=%d.%04dm, burst_num=%d, valid_measure_cnt=%d num_ftm=%d\n",
			avg_dist >> 8, /* 1/256m units */
			((avg_dist & 0xff) * 625) >> 4,
			ltoh16_ua(&p_data_info->burst_num),
			p_data_info->num_valid_rtt, p_data_info->num_ftm); /* in a session */
	}

	/* show 'avg_rtt' sample */
	p_sample = &p_data_info->avg_rtt;
	printf(">\tavg_rtt sample: rssi=%d snr=%d bitflips=%d rtt=%d%s "
		"std_deviation = %d.%d ratespec=0x%08x\n",
		(wl_proxd_rssi_t) ltoh16_ua(&p_sample->rssi),
		(wl_proxd_snr_t) ltoh16_ua(&p_sample->snr),
		(wl_proxd_bitflips_t) ltoh16_ua(&p_sample->bitflips),
		ltoh32_ua(&p_sample->rtt.intvl),
		ftm_tmu_value_to_logstr(ltoh16_ua(&p_sample->rtt.tmu)),
		ltoh16_ua(&p_data_info->sd_rtt)/10, ltoh16_ua(&p_data_info->sd_rtt)%10,
		ltoh32_ua(&p_sample->ratespec));

	printf(">\tnum_measurements: %d ", p_data_info->num_meas);
	printf(" Flags:");
	if (p_data_info->flags & WL_PROXD_REQUEST_SENT) {
		if (p_data_info->flags & WL_PROXD_REQUEST_ACKED) {
			if (p_data_info->num_meas)
				printf("(%s)", pstatestr[p_data_info->num_meas]);
			else
				printf("(FTM1/REQSENT/ACKED)");
		} else {
			printf("(FTM1/REQSENT/NOACK)");
		}
	} else {
		printf("(NO_REQ_SENT)");
	}
	printf("(LTFSEQ %sSTARTED)\n", (p_data_info->flags & WL_PROXD_LTFSEQ_STARTED)? "":"not ");

	/* display detail if available */
	num_rtt = ltoh16_ua(&p_data_info->num_rtt);
	if (num_rtt > 0)
	{
		printf(">\tnum rtt: %d samples\n", num_rtt);
		p_sample = &p_data_info->rtt[0];
		for (i = 0; i < num_rtt; i++)
		{
			uint16 snr = 0, bitflips = 0;
			wl_proxd_phy_error_t tof_phy_error = 0;
			wl_proxd_phy_error_t tof_phy_tgt_error = 0;
			wl_proxd_snr_t tof_target_snr = 0;
			wl_proxd_bitflips_t tof_target_bitflips = 0;
			int16 rssi = 0;
			int32 dist = 0;
			/* FTM frames 1,4,7,11 have valid snr, rssi and bitflips */
			if ((i%TOF_DEFAULT_FTMCNT_SEQ) == 1) {
				rssi = (wl_proxd_rssi_t) ltoh16_ua(&p_sample->rssi);
				snr = (wl_proxd_snr_t) ltoh16_ua(&p_sample->snr);
				bitflips = (wl_proxd_bitflips_t) ltoh16_ua(&p_sample->bitflips);
				tof_phy_error =
					(wl_proxd_phy_error_t) ltoh32_ua(&p_sample->tof_phy_error);
				tof_phy_tgt_error =
				(wl_proxd_phy_error_t) ltoh32_ua(&p_sample->tof_tgt_phy_error);
				tof_target_snr =
				(wl_proxd_snr_t) ltoh16_ua(&p_sample->tof_tgt_snr);
				tof_target_bitflips =
				(wl_proxd_bitflips_t) ltoh16_ua(&p_sample->tof_tgt_bitflips);
				dist = ltoh32_ua(&p_sample->distance);
			} else {
				rssi = -1;
				snr = 0;
				bitflips = 0;
				dist = 0;
				tof_target_bitflips = 0;
				tof_target_snr = 0;
				tof_phy_tgt_error = 0;
			}
			printf(">\t sample[%d]: id=%d rssi=%d snr=%f bitflips=%d tof_phy_error %x "
				" tof_phy_tgt_error %x target_snr=%f target_bitflips=%d"
				" dist=%d rtt=%d%s status %s Type %s coreid=%d\n",
				i, p_sample->id, rssi, ((double)snr),
				bitflips, tof_phy_error, tof_phy_tgt_error,
				((double)tof_target_snr),
				tof_target_bitflips, dist,
				ltoh32_ua(&p_sample->rtt.intvl),
				ftm_tmu_value_to_logstr(ltoh16_ua(&p_sample->rtt.tmu)),
				ftm_status_value_to_logstr(ltoh32_ua(&p_sample->status)),
				ftm_frame_types[i % TOF_DEFAULT_FTMCNT_SEQ], p_sample->coreid);
			p_sample++;
		}
	}
	return;
}

static void
ftm_unpack_and_display_rtt_result_v2(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	int	i;
	char	dispbuf[256];
	char	*p_dest;
	wl_proxd_result_flags_t flags;
	wl_proxd_session_state_t session_state;
	int32 avg_dist;
	wl_proxd_rtt_result_v2_t *p_data_info;
	wl_proxd_rtt_sample_v2_t *p_sample;
	wl_proxd_status_t status;
	uint16 version = 0;
	uint16 length = 0;
	uint16 num_rtt;
	char *ftm_frame_types[] =  FTM_FRAME_TYPES;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_rtt_result_v2_t *) p_data;

	/* read version and length */
	version = ltoh16_ua(&p_data_info->version);
	length = ltoh16_ua(&p_data_info->length);

	/* unpack and format 'flags' for display */
	flags = ltoh16_ua(&p_data_info->flags);
	memset(dispbuf, 0, sizeof(dispbuf));
	p_dest = &dispbuf[0];
	if (flags & WL_PROXD_RESULT_FLAG_NLOS) {
		strncpy(p_dest, "NLOS", sizeof(dispbuf) - strlen(dispbuf) - 1);
		p_dest = &dispbuf[strlen(dispbuf)];
	}
	if (flags & WL_PROXD_RESULT_FLAG_LOS)
		strncpy(p_dest, " LOS", sizeof(dispbuf) - strlen(dispbuf) - 1);
	if (flags & WL_PROXD_RESULT_FLAG_FATAL)
		strncpy(p_dest, " Fatal error", sizeof(dispbuf) - strlen(dispbuf) - 1);

	printf("> %s:\n>\tsessionId=%d, flags=0x%04x('%s'), peer=%s\n",
		ftm_tlvid_to_logstr(tlvid),
		ltoh16_ua(&p_data_info->sid),
		flags,
		dispbuf,	/* flags */
		wl_ether_etoa(&p_data_info->peer));

	/* session state and status */
	session_state = ltoh16_ua(&p_data_info->state);
	status = ltoh32_ua(&p_data_info->status);
	printf(">\tsession state=%d(%s) status=%d(%s)\n",
		session_state, ftm_session_state_value_to_logstr(session_state),
		status, ftm_status_value_to_logstr(status));

	printf(">\tburst_duration: %d%s\n",
		ltoh32_ua(&p_data_info->u.burst_duration.intvl),
		ftm_tmu_value_to_logstr(ltoh16_ua(&p_data_info->u.burst_duration.tmu)));

	/* show avg_dist (1/256m units), burst_num */
	avg_dist = ltoh32_ua(&p_data_info->avg_dist);
	if ((uint32)avg_dist == 0xffffffff) {	/* report 'failure' case */
		printf(">\tavg_dist=-1m, burst_num=%d, valid_measure_cnt=%d num_ftm=%d\n",
		ltoh16_ua(&p_data_info->burst_num),
		p_data_info->num_valid_rtt, p_data_info->num_ftm); /* in a session */
	}
	else {
		printf(">\tavg_dist=%d.%04dm, burst_num=%d, valid_measure_cnt=%d num_ftm=%d\n",
			avg_dist >> 8, /* 1/256m units */
			((avg_dist & 0xff) * 625) >> 4,
			ltoh16_ua(&p_data_info->burst_num),
			p_data_info->num_valid_rtt, p_data_info->num_ftm); /* in a session */
	}

	/* show 'avg_rtt' sample */
	/* For v2, avg_rtt is the first element of rtt[] */
	p_sample = &p_data_info->rtt[0];
	printf(">\tavg_rtt sample: rssi=%d snr=%d bitflips=%d rtt=%d%s "
		"std_deviation = %d.%d ratespec=0x%08x chanspec=0x%08x\n",
		(wl_proxd_rssi_t) ltoh16_ua(&p_sample->rssi),
		(wl_proxd_snr_t) ltoh16_ua(&p_sample->snr),
		(wl_proxd_bitflips_t) ltoh16_ua(&p_sample->bitflips),
		ltoh32_ua(&p_sample->rtt.intvl),
		ftm_tmu_value_to_logstr(ltoh16_ua(&p_sample->rtt.tmu)),
		ltoh16_ua(&p_data_info->sd_rtt)/10, ltoh16_ua(&p_data_info->sd_rtt)%10,
		ltoh32_ua(&p_sample->ratespec), ltoh32_ua(&p_sample->chanspec));

	printf(">\tnum_measurements: %d ", p_data_info->num_meas);
	printf(" Flags:");
	if (flags & WL_PROXD_REQUEST_SENT) {
		if (flags & WL_PROXD_REQUEST_ACKED) {
			if (flags & WL_PROXD_RESULT_FLAG_VHTACK)
				printf("(REQSENT/ACKED/VHT ACK)");
			else
				printf("(REQSENT/ACKED/LEGACY ACK)");
		} else {
			printf("(REQSENT/NOACK)");
		}
	} else {
		printf("(NO_REQ_SENT)");
	}
	printf("\n");

	if (version <= WL_PROXD_RTT_RESULT_VERSION_2) {
		if (length >
			(sizeof(wl_proxd_rtt_result_v2_t) -
			OFFSETOF(wl_proxd_rtt_result_v2_t, sid))) {
			printf("WARN: potential version mismatch:"
				"version and length do not match\n");
		}
	}

	/* varaible */
	/* display detail if available */
	num_rtt = ltoh16_ua(&p_data_info->num_rtt);
	if (num_rtt > 0)
	{
		printf(">\tnum rtt: %d samples\n", num_rtt);
		p_sample = &p_data_info->rtt[1];
		for (i = 0; i < num_rtt; i++)
		{
			uint16 snr = 0, bitflips = 0;
			wl_proxd_phy_error_t tof_phy_error = 0;
			wl_proxd_phy_error_t tof_phy_tgt_error = 0;
			wl_proxd_snr_t tof_target_snr = 0;
			wl_proxd_bitflips_t tof_target_bitflips = 0;
			int16 rssi = 0;
			int32 dist = 0;
			/* FTM frames 1,4,7,11 have valid snr, rssi and bitflips */
			if ((i%TOF_DEFAULT_FTMCNT_SEQ) == 1) {
				rssi = (wl_proxd_rssi_t) ltoh16_ua(&p_sample->rssi);
				snr = (wl_proxd_snr_t) ltoh16_ua(&p_sample->snr);
				bitflips = (wl_proxd_bitflips_t) ltoh16_ua(&p_sample->bitflips);
				tof_phy_error =
					(wl_proxd_phy_error_t) ltoh32_ua(&p_sample->tof_phy_error);
				tof_phy_tgt_error =
				(wl_proxd_phy_error_t) ltoh32_ua(&p_sample->tof_tgt_phy_error);
				tof_target_snr =
				(wl_proxd_snr_t) ltoh16_ua(&p_sample->tof_tgt_snr);
				tof_target_bitflips =
				(wl_proxd_bitflips_t) ltoh16_ua(&p_sample->tof_tgt_bitflips);
				dist = ltoh32_ua(&p_sample->distance);
			} else {
				rssi = -1;
				snr = 0;
				bitflips = 0;
				dist = 0;
				tof_target_bitflips = 0;
				tof_target_snr = 0;
				tof_phy_tgt_error = 0;
			}
			printf(">\t sample[%d]: id=%d rssi=%d snr=%f bitflips=%d tof_phy_error %x "
				" tof_phy_tgt_error %x target_snr=%f target_bitflips=%d"
				" dist=%d rtt=%d%s status %s Type %s coreid=%d chanspec=0x%08x\n",
				i, p_sample->id, rssi, ((double)snr),
				bitflips, tof_phy_error, tof_phy_tgt_error,
				((double)tof_target_snr),
				tof_target_bitflips, dist,
				ltoh32_ua(&p_sample->rtt.intvl),
				ftm_tmu_value_to_logstr(ltoh16_ua(&p_sample->rtt.tmu)),
				ftm_status_value_to_logstr(ltoh32_ua(&p_sample->status)),
				ftm_frame_types[i % TOF_DEFAULT_FTMCNT_SEQ], p_sample->coreid,
				ltoh32_ua(&p_sample->chanspec));
			p_sample++;
		}
	}
	return;
}

/*
* unpack and display session_info TLV (WL_PROXD_TLV_ID_SESSION_INFO)
*/
static void
ftm_unpack_and_display_session_info(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_ftm_session_info_t *p_data_info;
	wl_proxd_session_state_t session_state;
	wl_proxd_status_t proxd_status;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_ftm_session_info_t *) p_data;

	printf("> %s: bssidx=%d, bssid=%s\n",
		ftm_tlvid_to_logstr(tlvid),
		p_data_info->bss_index,
		wl_ether_etoa(&p_data_info->bssid));

	session_state = ltoh16_ua(&p_data_info->state);
	proxd_status = ltoh32_ua(&p_data_info->status);

	printf("\tsessionId=%d, state=%d(%s), status=%d(%s), burst_num=%d, "
		"meas_start %u.%u\n",
		ltoh16_ua(&p_data_info->sid),
		session_state,
		ftm_session_state_value_to_logstr(session_state),
		proxd_status,
		ftm_status_value_to_logstr(proxd_status),
		ltoh16_ua(&p_data_info->burst_num),
		ltoh32_ua(&p_data_info->meas_start_hi), ltoh32_ua(&p_data_info->meas_start_lo));

	return;
}

/*
* unpack and display session status TLV (WL_PROXD_TLV_ID_SESSION_STATUS)
*/
static void
ftm_unpack_and_display_session_status(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_ftm_session_status_t *p_data_info;
	wl_proxd_session_state_t session_state;
	wl_proxd_status_t proxd_status;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_ftm_session_status_t *) p_data;
	session_state = ltoh16_ua(&p_data_info->state);
	proxd_status = ltoh32_ua(&p_data_info->status);
	printf("> %s: sessionId=%d, state=%d(%s), status=%d(%s), burst_num=%d\n",
		ftm_tlvid_to_logstr(tlvid),
		ltoh16_ua(&p_data_info->sid),
		session_state,
		ftm_session_state_value_to_logstr(session_state),
		proxd_status,
		ftm_status_value_to_logstr(proxd_status),
		ltoh16_ua(&p_data_info->burst_num));

	return;
}

/*
* unpack and display session_id lists TLV (WL_PROXD_TLV_ID_COUNTERS)
*/
static void
ftm_unpack_and_display_counters(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_counters_t *p_data_info;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_counters_t *) p_data;
	printf("> %s:\n\ttx-frame-count=%d, rx-frame_count=%d\n",
		ftm_tlvid_to_logstr(tlvid),
		ltoh32_ua(&p_data_info->tx),			/* tx frame count */
		ltoh32_ua(&p_data_info->rx));			/* rx frame count */

	printf("\tnoack=%d, txfail=%d num_meas=%d\n",
		ltoh32_ua(&p_data_info->noack), 		/* tx w/o ack */
		ltoh32_ua(&p_data_info->txfail),		/* tx failures */
		ltoh32_ua(&p_data_info->num_meas));		/* tx failures */

	printf("\tburst=%d, sessions=%d, max_sessions=%d\n",
		ltoh32_ua(&p_data_info->burst),			/* # of burst */
		ltoh32_ua(&p_data_info->sessions),		/* # of sessions */
		ltoh32_ua(&p_data_info->max_sessions));	/* max concurrency */

	printf("\tsched_fail=%d, timeouts=%d, protocol errors=%d\n",
		ltoh32_ua(&p_data_info->sched_fail), /* scheduling failures */
		ltoh32_ua(&p_data_info->timeouts),	/* timeouts */
		ltoh32_ua(&p_data_info->protoerr));	/* protocol err */

	printf("\tLCI: tx request=%d, rx request=%d, tx report=%d, rx report=%d\n",
		ltoh32_ua(&p_data_info->lci_req_tx), /*  tx LCI requests */
		ltoh32_ua(&p_data_info->lci_req_rx), /*  rx LCI requests */
		ltoh32_ua(&p_data_info->lci_rep_tx), /*  tx LCI reports */
		ltoh32_ua(&p_data_info->lci_rep_rx)); /*  rx LCI reports */

	printf("\tcivic: tx request=%d, rx request=%d, tx report=%d, "
		"rx report=%d\n",
		ltoh32_ua(&p_data_info->civic_req_tx), /* tx civic requests */
		ltoh32_ua(&p_data_info->civic_req_rx), /* rx civic requests */
		ltoh32_ua(&p_data_info->civic_rep_tx), /* tx civic reports */
		ltoh32_ua(&p_data_info->civic_rep_rx)); /* rx civic reports */

	printf("\tranging: created=%d, done=%d\n",
		ltoh32_ua(&p_data_info->rctx), ltoh32_ua(&p_data_info->rctx_done));

	printf("\tpublish errors=%d\n", ltoh32_ua(&p_data_info->publish_err));
	printf("\tsched on_chan=%d off_chan=%d\n", ltoh32_ua(&p_data_info->on_chan),
		ltoh32_ua(&p_data_info->off_chan));
	printf("\ttsf %u.%u\n",
		ltoh32_ua(&p_data_info->tsf_hi),
		ltoh32_ua(&p_data_info->tsf_lo));
	return;
}

/*
* unpack and display session_id lists TLV (WL_PROXD_TLV_ID_SESSION_ID_LIST)
*/
static void
ftm_unpack_and_display_session_idlist(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_session_id_list_t *p_data_info;
	int i;
	uint16 num_ids;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_session_id_list_t *) p_data;
	num_ids = ltoh16_ua(&p_data_info->num_ids);
	printf("> %s: total %d id(s)\n", ftm_tlvid_to_logstr(tlvid), num_ids);

	for (i = 0; i < num_ids; i++) {
		printf(">\tsession[%d]: %d\n", i, ltoh16_ua(&p_data_info->ids[i]));
	}

	return;
}

/*
* unpack and display availability TLV
*/
static void
ftm_unpack_and_display_avail(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_avail_t		*avail;
	wl_proxd_avail24_t		*avail24;
	wl_proxd_avail_t		l_avail;
	wl_proxd_avail_flags_t 	flags;
	wl_proxd_time_ref_t 	time_ref;
	uint16					num_slots;
	wl_proxd_time_slot_t	*avail_slot;
	chanspec_t				chanspec;
	int						i;
	char					dispbuf[256];

	UNUSED_PARAMETER(len);

	/* unpack and format the avail-header for display */
	if (tlvid == WL_PROXD_TLV_ID_AVAIL24) {
		avail24 = (wl_proxd_avail24_t *) p_data;
		/* convert the old version */
		l_avail.flags = avail24->flags;
		l_avail.time_ref = avail24->time_ref;
		l_avail.num_slots = avail24->num_slots;
		l_avail.max_slots = avail24->max_slots;
		l_avail.repeat = avail24->repeat;
		avail = &l_avail;

		avail_slot = &avail24->ts0[0];
	}
	else { /* WL_PROXD_TLV_ID_AVAIL */
		avail = (wl_proxd_avail_t *) p_data;
		avail_slot = WL_PROXD_AVAIL_TIMESLOTS(avail);
	}

	flags = ltoh16_ua(&avail->flags);
	time_ref = ltoh16_ua(&avail->time_ref);
	num_slots = ltoh16_ua(&avail->num_slots);

	printf("> %s:\n>\tflags=0x%04x('%s'), time-ref=0x%04x('%s'), "
		"period=%d%s, num_slots=%d, max_slots=%d\n",
		ftm_tlvid_to_logstr(tlvid),
		flags, ftm_avail_flags_value_to_logstr(flags),
		time_ref, ftm_avail_timeref_value_to_logstr(time_ref),
		ltoh32_ua(&avail->repeat.intvl),
		ftm_tmu_value_to_logstr(ltoh16_ua(&avail->repeat.tmu)),
		num_slots, ltoh16_ua(&avail->max_slots));

	/* unpack and format the time slots for display */
	for (i = 0; i < num_slots; i++) {
		printf(">\tslots[%d]: start: %d%s duration: %d%s",
			i, ltoh32_ua(&avail_slot->start.intvl),
			ftm_tmu_value_to_logstr(ltoh16_ua(&avail_slot->start.tmu)),
			ltoh32_ua(&avail_slot->duration.intvl),
			ftm_tmu_value_to_logstr(ltoh16_ua(&avail_slot->duration.tmu)));

		chanspec = (chanspec_t) ltoh32_ua(&avail_slot->chanspec);
		if (wf_chspec_valid(chanspec)) {
			memset(dispbuf, 0, sizeof(dispbuf));
			wf_chspec_ntoa(chanspec, dispbuf);
			printf(" chanspec: 0x%04x(%s)\n", chanspec, dispbuf);
		}
		else {
			printf(" chanspec: 0x%04x(invalid)\n", chanspec);
		}
		avail_slot++;
	}
}

/*
* unpack and display session_info TLV (WL_PROXD_TLV_ID_RANGING_INFO)
*/
static void
ftm_unpack_and_display_ranging_info(const uint8 *p_data, uint16 tlvid, uint16 len)
{
	wl_proxd_ranging_info_t *p_data_info;
	wl_proxd_status_t proxd_status;
	wl_proxd_ranging_state_t	state;
	wl_proxd_ranging_flags_t	flags;

	UNUSED_PARAMETER(len);

	p_data_info = (wl_proxd_ranging_info_t *) p_data;

	printf("> %s:\n", ftm_tlvid_to_logstr(tlvid));

	proxd_status = ltoh32_ua(&p_data_info->status);
	state = ltoh16_ua(&p_data_info->state);
	flags = ltoh16_ua(&p_data_info->flags);

	printf("\tstatus=%d(%s), state=%d(%s), flags=0x%04x(%s), num_sids=%d, num_done=%d\n",
		proxd_status,
		ftm_status_value_to_logstr(proxd_status),
		state,
		ftm_ranging_state_value_to_logstr(state),
		flags,
		ftm_ranging_flags_value_to_logstr(flags),
		ltoh16_ua(&p_data_info->num_sids),
		ltoh16_ua(&p_data_info->num_done));

	return;
}

/*
 * a callback function, displays (wl_proxd_tlv_t) bcm_xtlv variables rcvd in
 * get ioctl's xtlv buffer.
 *  -- This function processes GET result for all 'proxd ftm' commands, provided
 *     that XTLV types (AKA the explicit xtlv types) packed into the ioctl buff
 *     are unique across all 'proxd ftm' ioctl commands
 * -- This function is also used to display rx FTM events (content in XTLVs),
 *    see ftm_event_check().
 */
static int
ftm_unpack_xtlv_cbfn(void *ctx, const uint8 *p_data, uint16 tlvid, uint16 len)
{
	int res = BCME_OK;
	char dispbuf[256];
	wl_proxd_event_type_t event_data;
	char event_dispbuf[256];
	chanspec_t chanspec;
	wl_proxd_intvl_t *p_data_intvl;
	wl_proxd_ftm_info_t *p_data_info;
	wl_proxd_tpk_t *tpk_info;

#ifdef WL_FTM_DEBUG
	/* for 'get' commands :
			wl_proxd_iov_t *p_proxd_iov = (wl_proxd_iov_t *) ctx;
	   for event_check:
			ctx is NULL
	*/
	printf("enter %s(ctx=%p, data=%p, tlvid=%d, len=%d)...\n",
		__FUNCTION__, ctx, p_data, tlvid, len);
#else
	UNUSED_PARAMETER(ctx);
#endif /* WL_FTM_DEBUG */

	memset(dispbuf, 0, sizeof(dispbuf));	/* clear the buffer for display */

	switch (tlvid) {
		/* data=uint8 as a number, display in decimal */
		case WL_PROXD_TLV_ID_BSS_INDEX:
		case WL_PROXD_TLV_ID_FTM_RETRIES:	/* at FTM level */
		case WL_PROXD_TLV_ID_FTM_REQ_RETRIES:
			printf("> %s: %d\n", ftm_tlvid_to_logstr(tlvid), *p_data);
		break;

		/* data=uint16 as a number, display in decimal */
		case WL_PROXD_TLV_ID_BURST_NUM_FTM:	/* per burst */
		case WL_PROXD_TLV_ID_NUM_BURST:
		case WL_PROXD_TLV_ID_RX_MAX_BURST:
			printf("> %s: %d\n", ftm_tlvid_to_logstr(tlvid), ltoh16_ua(p_data));
		break;

		/* data=uin32 as a number, display in decimal */
		case WL_PROXD_TLV_ID_NONE:
		case WL_PROXD_TLV_ID_TX_POWER:
		case WL_PROXD_TLV_ID_STATUS:			/* not used ? SJL_FIXME */
			printf("> %s: %d\n", ftm_tlvid_to_logstr(tlvid), ltoh32_ua(p_data));
		break;

		/* data=uint32 as a number, display in hex */
		case WL_PROXD_TLV_ID_RATESPEC:
		case WL_PROXD_TLV_ID_DEBUG_MASK:
		case WL_PROXD_TLV_ID_RANGING_FLAGS:
		case WL_PROXD_TLV_ID_RANGING_FLAGS_MASK:
			printf("> %s: 0x%x\n", ftm_tlvid_to_logstr(tlvid), ltoh32_ua(p_data));
		break;

		/* data=uint32 as a number, display in hex */
		case WL_PROXD_TLV_ID_FLAGS:
		case WL_PROXD_TLV_ID_FLAGS_MASK:
			ftm_unpack_and_display_config_flags(p_data, tlvid);
		break;

		/* data=uint32 as a number, display in hex */
		case WL_PROXD_TLV_ID_SESSION_FLAGS:
		case WL_PROXD_TLV_ID_SESSION_FLAGS_MASK:
			ftm_unpack_and_display_session_flags(p_data, tlvid);
			break;

		/* data=uint32 as a number, display in hex */
		case WL_PROXD_TLV_ID_EVENT_MASK:
		{
			event_data = ltoh32_ua(p_data);
			event_dispbuf[0] = '\0';
#ifdef WL_FTM_DEBUG
			/* convert to a readable text-string */
			ftm_format_event_mask(event_data, event_dispbuf, sizeof(event_dispbuf));
#endif /* WL_FTM_DEBUG */
			printf("> %s: 0x%x %s\n",
				ftm_tlvid_to_logstr(tlvid), event_data, event_dispbuf);
		}
		break;

		/* data=uin32 as a number, convert and display a text-string */
		case WL_PROXD_TLV_ID_METHOD:
			printf("> %s: %s\n", ftm_tlvid_to_logstr(tlvid),
				ftm_method_value_to_logstr((wl_proxd_method_t) ltoh32_ua(p_data)));
		break;

		/* data=uint32 as a chanspec */
		case WL_PROXD_TLV_ID_CHANSPEC:
		{
			chanspec = (chanspec_t) ltoh32_ua(p_data);
			if (wf_chspec_valid(chanspec)) {
				wf_chspec_ntoa(chanspec, dispbuf);
				printf("> %s: %s 0x%04x\n",
					ftm_tlvid_to_logstr(tlvid), dispbuf, chanspec);
			} else {
				printf("> %s: invalid 0x%04x\n",
					ftm_tlvid_to_logstr(tlvid), chanspec);
			}
		}
		break;

		/* data=intvl */
		case WL_PROXD_TLV_ID_BURST_DURATION:
		case WL_PROXD_TLV_ID_BURST_PERIOD:
		case WL_PROXD_TLV_ID_BURST_FTM_SEP:
		case WL_PROXD_TLV_ID_INIT_DELAY:
		case WL_PROXD_TLV_ID_BURST_TIMEOUT:
		{
			p_data_intvl = (wl_proxd_intvl_t *) p_data;
			printf("> %s: %d%s\n", ftm_tlvid_to_logstr(tlvid),
				ltoh32_ua(&p_data_intvl->intvl),
				ftm_tmu_value_to_logstr(ltoh16_ua(&p_data_intvl->tmu)));
		}
		break;

		case WL_PROXD_TLV_ID_BSSID:
		case WL_PROXD_TLV_ID_PEER_MAC:
		case WL_PROXD_TLV_ID_DEV_ADDR:
		case WL_PROXD_TLV_ID_CUR_ETHER_ADDR:
			printf("> %s: %s\n", ftm_tlvid_to_logstr(tlvid),
				wl_ether_etoa((struct ether_addr *)p_data));
		break;

		case WL_PROXD_TLV_ID_TPK:
		{
			tpk_info = (wl_proxd_tpk_t *)p_data;
			printf("> %s: mac addr%s \n", ftm_tlvid_to_logstr(tlvid),
				wl_ether_etoa((struct ether_addr *)&tpk_info->peer));
		}
		break;

		case WL_PROXD_TLV_ID_INFO:			/* data=wl_proxd_ftm_info_t */
		{
			p_data_info = (wl_proxd_ftm_info_t *) p_data;
			printf("> %s: capabilities=%s, max sessions=%d, num sessions=%d,"
				"rx_max_burst=%d\n",
				ftm_tlvid_to_logstr(tlvid),
				ftm_caps_value_to_logstr(ltoh16_ua(&p_data_info->caps)),
				ltoh16_ua(&p_data_info->max_sessions),
				ltoh16_ua(&p_data_info->num_sessions),
				ltoh16_ua(&p_data_info->rx_max_burst));
		}
		break;

		case WL_PROXD_TLV_ID_FTM_REQ:		/* data=dot11_ftm_req_t, var-len */
			printf("> %s: data-len=%d byte(s)\n",
				ftm_tlvid_to_logstr(tlvid), len);
			if (len > 0)
				prhex(NULL, (uint8 *)p_data, len);
		break;

		case WL_PROXD_TLV_ID_RTT_RESULT:	/* data=wl_proxd_rtt_result_v1_t */
			ftm_unpack_and_display_rtt_result_v1(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_RTT_RESULT_V2:	/* data=wl_proxd_rtt_result_v2_t */
			ftm_unpack_and_display_rtt_result_v2(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_SESSION_INFO:	/* data=wl_proxd_ftm_session_info_t */
			ftm_unpack_and_display_session_info(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_SESSION_STATUS:	/* data=wl_proxd_ftm_session_status_t */
			ftm_unpack_and_display_session_status(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_COUNTERS:			/* data=wl_proxd_counters_t */
			ftm_unpack_and_display_counters(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_SESSION_ID_LIST:	/* data=wl_proxd_session_id_list */
			ftm_unpack_and_display_session_idlist(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_RANGING_INFO:	/* data=wl_proxd_ranging_info_t */
			ftm_unpack_and_display_ranging_info(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_STRBUF:
			if (len > 0)
				printf("> %s:\n%s\n", ftm_tlvid_to_logstr(tlvid), p_data);
		break;

		case WL_PROXD_TLV_ID_AVAIL24:
		case WL_PROXD_TLV_ID_AVAIL:
			ftm_unpack_and_display_avail(p_data, tlvid, len);
		break;

		case WL_PROXD_TLV_ID_LCI:
		case WL_PROXD_TLV_ID_CIVIC:
			printf("> %s: data-len=%d byte(s)\n",
				ftm_tlvid_to_logstr(tlvid), len);
			if (len > 0)
				prhex(NULL, (uint8*)p_data, len);
		break;
		case WL_PROXD_TLV_ID_TUNE:
			res = proxd_tune_display((void *)p_data, len);
		break;
		case WL_PROXD_TLV_ID_LCI_REQ:		/* SJL_FIXME */
		case WL_PROXD_TLV_ID_CIVIC_REQ:	/* SJL_FIXME */
			/* fall thru (not used/supported now) */
		default:
			printf("> Unsupported %s: %d\n", ftm_tlvid_to_logstr(tlvid), tlvid);
			res = BCME_ERROR;
		break;
	}

	return res;
}

/* pack TLVs for checking if a specific tlv-id is supported or not
* for WL_PROXD_CMD_IS_TLV_SUPPORTED
*/
static int
ftm_pack_tlv_id_support(uint8 *buf, uint16 bufsize, void *ctx, uint16 *all_tlvsize)
{
	int err = BCME_OK;
	uint16	buf_space_left;
	uint16	tlv_id;
	wl_proxd_tlv_t *p_tlv = (wl_proxd_tlv_t *) buf;

	*all_tlvsize = 0;

	if (!ctx)
		return BCME_BADARG;

	/* TLV buffer starts with a full size, will decrement for each packed TLV */
	buf_space_left = bufsize;

	tlv_id = htol16(*((uint16 *) ctx));
	err = bcm_pack_xtlv_entry((uint8 **) &p_tlv, &buf_space_left,
		WL_PROXD_TLV_ID_TLV_ID, sizeof(uint16), (void *) &tlv_id,
		BCM_XTLV_OPTION_ALIGN32);
	if (err != BCME_OK) {
		printf("%s: failed to pack TLVs, err=%d\n",
			__FUNCTION__, err);
		goto done;
	}

	/* return the len to include all TLVs(including TLV header) */
	*all_tlvsize = (bufsize - buf_space_left);

done:
	return err;
}

/*
* check if a tlv-id is supported:
* return BCME_OK if tlv-id is supported
*/
static int
ftm_is_tlv_id_supported(void *wl, uint16 tlv_id)
{
	int 			err;
	wl_proxd_iov_t	*p_proxd_iov;
	uint16 			proxd_iovsize;
	uint16 			all_tlvsize = 0;
	wl_proxd_iov_t *p_iovresp = NULL;

	/*  alloc mem for ioctl header + reserved bufsize for tlvs (initialize to zero) */
	p_proxd_iov = ftm_alloc_getset_buf(WL_PROXD_METHOD_FTM,
		WL_PROXD_SESSION_ID_GLOBAL, WL_PROXD_CMD_IS_TLV_SUPPORTED,
		FTM_IOC_BUFSZ, &proxd_iovsize);
	if (p_proxd_iov == NULL) {
		err = BCME_NOMEM;
		goto done;
	}

	/* Setup TLVs -- pack a TLV_ID for support checking */
	err = ftm_pack_tlv_id_support((uint8 *) &p_proxd_iov->tlvs[0],
		proxd_iovsize - WL_PROXD_IOV_HDR_SIZE,
		(void *) &tlv_id, &all_tlvsize);
	if (err != BCME_OK)
		goto done;

	/* update the iov header, set len to include all TLVs + header */
	proxd_iovsize = all_tlvsize + WL_PROXD_IOV_HDR_SIZE;
	p_proxd_iov->len = htol16(proxd_iovsize);

	/* submit a 'get' request to see if the tlv-id is supported or not */
	err = wlu_var_getbuf(wl, "proxd", p_proxd_iov, proxd_iovsize, (void *) &p_iovresp);

done:
	/* clean up */
	free(p_proxd_iov);

	return err;

}

/*
* send 'proxd' iovar for all ftm get-related commands
*/
static int
ftm_do_get_ioctl(void *wl, wl_proxd_iov_t *p_proxd_iov, uint16 proxd_iovsize,
	const ftm_subcmd_info_t *p_subcmd_info)
{

	/* for gets we only need to pass ioc header */
	wl_proxd_iov_t *p_iovresp = NULL;
	int status;

	/*  send getbuf proxd iovar */
	status = wlu_var_getbuf(wl, "proxd", p_proxd_iov, proxd_iovsize, (void *)&p_iovresp);
	if (status != BCME_OK) {
#ifdef WL_FTM_DEBUG
		printf("%s: failed to send getbuf proxd iovar, status=%d\n",
			__FUNCTION__, status);
#endif /* WL_FTM_DEBUG */
		return status;
	}

	if (p_iovresp != NULL) {
		int tlvs_len = ltoh16(p_iovresp->len) - WL_PROXD_IOV_HDR_SIZE;
		if (tlvs_len < 0)
		{
#ifdef WL_FTM_DEBUG
			printf("%s: alert, p_iovresp->len(%d) should not be smaller than %d\n",
				__FUNCTION__, ltoh16(p_iovresp->len), (int) WL_PROXD_IOV_HDR_SIZE);
#endif /* WL_FTM_DEBUG */
			tlvs_len = 0;
		}

		if (p_subcmd_info->cmdid == WL_PROXD_CMD_GET_VERSION) /* 'wl proxd ftm version' */
			printf("> version: 0x%x\n", ltoh16(p_iovresp->version));

#ifdef WL_FTM_DEBUG
		printf("%s: p_iovresp->(tlvs=%p,len=%d), tlvs_len=%d, cmdid=%d(%s) ver=0x%x\n",
			__FUNCTION__, p_iovresp->tlvs, ltoh16(p_iovresp->len), tlvs_len,
			ltoh16(p_iovresp->cmd), ftm_cmdid_to_str(ltoh16(p_iovresp->cmd)),
			ltoh16(p_iovresp->version));
#endif /* WL_FTM_DEBUG */

		if (tlvs_len > 0)
		{
			/* unpack TLVs and invokes the cbfn for processing */
			status = bcm_unpack_xtlv_buf(p_proxd_iov, (uint8 *)p_iovresp->tlvs,
				tlvs_len, BCM_XTLV_OPTION_ALIGN32, ftm_unpack_xtlv_cbfn);
			printf("\n");
		}
	}

	return status;
}

/*
* common handler for all get-related proxd method commands (no TLVs params for these commands)
*   wl proxd ftm [session-id] <get-subcmd>
*      where <get-subcmd> can be "ver", "result", "info", "status",
*                              "sessions", "counters", "dump"
* Note, this <get-subcmd> does not accept any parameters (i.e. no <param-name><param-value>)
*/
static int
ftm_common_getcmd_handler(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	int status = BCME_OK;
	uint16 proxd_iovsize = 0;

#ifdef WL_FTM_DEBUG
	printf("enter %s: method=%d, session_id=%d, cmdid=%d(%s)\n",
		__FUNCTION__, method, session_id, p_subcmd_info->cmdid,
		ftm_cmdid_to_str(p_subcmd_info->cmdid));
#endif /* WL_FTM_DEBUG */

	/*  alloc mem for ioctl headr + reserved 0 bufsize for tlvs (initialize to zero) */
	wl_proxd_iov_t *p_proxd_iov;
	p_proxd_iov = ftm_alloc_getset_buf(method, session_id, p_subcmd_info->cmdid,
		0, &proxd_iovsize);
	if (p_proxd_iov == NULL)
		return BCME_NOMEM;

	if (*argv == NULL) { /* get  */
		status = ftm_do_get_ioctl(wl, p_proxd_iov, proxd_iovsize, p_subcmd_info);
	} else {
		printf("error: proxd ftm %s cmd doesn't accept any parameters\n",
			p_subcmd_info->name);
		status = BCME_ERROR;
	}

	/* clean up */
	free(p_proxd_iov);

#ifdef WL_FTM_DEBUG
	if (status != BCME_OK)
		printf("%s failed: status=%d\n", __FUNCTION__, status);
#endif /* WL_FTM_DEBUG */

	return status;
}

/*
* get proxd ftm-method version (API version)
* Usage: wl proxd ftm ver
*
* Note, 'session-id' is ignore
*/
static int
ftm_subcmd_get_version(void *wl, const ftm_subcmd_info_t *p_subcmd_info, wl_proxd_method_t method,
	wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'version' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* get session result - may be in progress. Both rtt and aoa results returned (as available).
*                      Also lci and civic location if available.
* Usage: wl proxd ftm <session-id> result
* Note, caller should have verified the session-id before this call.
*/
static int
ftm_subcmd_get_result(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'session result' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* get the global ftm-method info or session-info if session-id is specified.
* Usage:
*       wl proxd ftm [<session-id>] info
*/
static int
ftm_subcmd_get_info(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'method/session info' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* get the ftm-method status or session status if session-id is specified.
* Usage: wl proxd ftm [<session-id>] status
*/
static int
ftm_subcmd_get_status(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'method/session status' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* list all sessions for the ftm method
* Usage: wl proxd ftm sessions
*/
static int
ftm_subcmd_get_sessions(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'sessions' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* get the ftm-method counters or sessions counters if session-id is specified.
* Usage:wl proxd ftm [<session-id>] counters
*/
static int
ftm_subcmd_get_counters(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'method/session counters' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* get the global ftm-method ranging-info
* Usage:
*       wl proxd ftm ranging-info
*/
static int
ftm_subcmd_get_ranging_info(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'ranging info' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/*
* dump the ftm-method or a session if session-id is specified.
* Usage: wl proxd ftm [<session-id>] dump
*/
static int
ftm_subcmd_dump(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar and display the 'dump result' */
	return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
}

/* ***************************** set commands ********************* */

/*
* common handler for set-related proxd method commands which require no TLV as input
*   wl proxd ftm [session-id] <set-subcmd>
* e.g.
*   wl proxd ftm enable -- to enable ftm
*   wl proxd ftm disable -- to disable ftm
*   wl proxd ftm <session-id> start -- to start a specified session
*   wl proxd ftm <session-id> stop  -- to cancel a specified session;
*                                    state is maintained till session is delete.
*   wl proxd ftm <session-id> delete -- to delete a specified session
*   wl proxd ftm [<session-id>] clear-counters -- to clear counters
*   wl proxd ftm <session-id> burst-request -- on initiator: to send burst request;
*                                              on target: send FTM frame
*   wl proxd ftm <session-id> collect
*   wl proxd ftm tune     (TBD)
*/
static int
ftm_subcmd_setiov_no_tlv(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	uint16 proxd_iovsize;
	wl_proxd_iov_t *p_proxd_iov;
	int res;

	UNUSED_PARAMETER(wl);

#ifdef WL_FTM_DEBUG
	printf("enter %s: method=%d, session_id=%d, cmdid=%d(%s)\n",
		__FUNCTION__, method, session_id,
		p_subcmd_info->cmdid, ftm_cmdid_to_str(p_subcmd_info->cmdid));
#endif /* WL_FTM_DEBUG */

	/* do not accept any parameters */
	if (*argv != NULL)
	{
		printf("error: proxd ftm %s cmd doesn't accept any parameters\n",
			p_subcmd_info->name);
		return BCME_USAGE_ERROR;
	}

	/* allocate and initialize a temp buffer for 'set proxd' iovar */
	proxd_iovsize = 0;
	p_proxd_iov = ftm_alloc_getset_buf(method, session_id, p_subcmd_info->cmdid,
							0, &proxd_iovsize);		/* no TLV */
	if (p_proxd_iov == NULL)
		return BCME_NOMEM;

	/* no TLV to pack, simply issue a set-proxd iovar */
	res = wlu_var_setbuf(wl, "proxd", (void *) p_proxd_iov, proxd_iovsize);
	if (res != BCME_OK) {
		printf("error: IOVAR failed, status=%d\n", res);
	}

	/* clean up */
	free(p_proxd_iov);

	return res;
}

/*
* enable FTM method
* Usage: wl proxd ftm enable
*/
static int
ftm_subcmd_enable(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* disable FTM method
* Usage: wl proxd ftm disable
*/
static int
ftm_subcmd_disable(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* start scheduling the burst(s) for the session. Initiates FTM request or await requests on target.
* Usage: wl proxd ftm <session-id> start
*
* Note, caller should have verified the session-id before this call.
*/
static int
ftm_subcmd_start_session(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* cancel the session. State maintained until session is deleted.
* Usage: wl proxd ftm <session-id> stop
*
* Note, caller should have verify the session-id before this call.
*/
static int ftm_subcmd_stop_session(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* burst request: on initiator send burst request and process FTM frames.
*                On target, send FTM frames
* Usage: wl proxd ftm <session-id> burst-request
*
* Note, caller should have verified the session-id before this call.
*/
static int
ftm_subcmd_burst_request(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* delete the session.
* Usage: wl proxd ftm <session-id> delete
*
* Note, caller should have verify the session-id before this call.
*/
static int
ftm_subcmd_delete_session(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* clear counters
* Usage: wl proxd ftm [<session-id>] clear_counters
*
* Note, caller should have verify the session-id before this call.
*/
static int
ftm_subcmd_clear_counters(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* define param-info for global/session 'non-options' configuration for
*    'wl proxd ftm [<session-id>] config [<param-name> <param-value>]*'
*/
typedef struct ftm_config_param_info {
	char		*name;	/* <param-name> string to identify a configurable item */
	uint16		tlvid;	/* mapping TLV id for the item */
	char		*name_helpmsg;	/* help message for the <param-name> config-item */
	ftm_subcmd_flag_t	cmdflag;	/* supported group(s) */
} ftm_config_param_info_t;

static const ftm_config_param_info_t ftm_config_param_info[] = {
	/* "name",			tlv_id				param-name helpmsg */
	{ "bssidx", WL_PROXD_TLV_ID_BSS_INDEX, "BSS index to use for the session",
	FTM_SUBCMD_FLAG_ALL },
	{ "chanspec", WL_PROXD_TLV_ID_CHANSPEC, "channel spec to use",
	FTM_SUBCMD_FLAG_ALL },
	{ "tx-power", WL_PROXD_TLV_ID_TX_POWER, "tx power to use in dbm",
	FTM_SUBCMD_FLAG_ALL },
	{ "ratespec", WL_PROXD_TLV_ID_RATESPEC, "rate to use",
	FTM_SUBCMD_FLAG_ALL },
	{ "num-ftm", WL_PROXD_TLV_ID_BURST_NUM_FTM, "number of FTM frames in a burst",
	FTM_SUBCMD_FLAG_ALL },
	{ "num-burst", WL_PROXD_TLV_ID_NUM_BURST, "number of bursts in the session",
	FTM_SUBCMD_FLAG_ALL },
	{ "retries", WL_PROXD_TLV_ID_FTM_RETRIES, "number of retries; not 802.11 mac level retries",
	FTM_SUBCMD_FLAG_ALL },
	{ "event-mask", WL_PROXD_TLV_ID_EVENT_MASK, "bitmask of subscribed events",
	FTM_SUBCMD_FLAG_ALL },
	{ "debug-mask", WL_PROXD_TLV_ID_DEBUG_MASK, "bitmask for logging FTM messages",
	FTM_SUBCMD_FLAG_ALL },
	{ "burst-duration", WL_PROXD_TLV_ID_BURST_DURATION, "duration for a single burst",
	FTM_SUBCMD_FLAG_ALL },
	{ "burst-period", WL_PROXD_TLV_ID_BURST_PERIOD, "time between bursts",
	FTM_SUBCMD_FLAG_ALL },
	{ "ftm-sep", WL_PROXD_TLV_ID_BURST_FTM_SEP, "time between FTM frames in a burst",
	FTM_SUBCMD_FLAG_ALL },
	{ "burst-timeout", WL_PROXD_TLV_ID_BURST_TIMEOUT, "timeout",
	FTM_SUBCMD_FLAG_ALL },
	{ "init-delay", WL_PROXD_TLV_ID_INIT_DELAY, "delay after start",
	FTM_SUBCMD_FLAG_ALL },
	{ "bssid", WL_PROXD_TLV_ID_BSSID, "BSSID used for the session",
	FTM_SUBCMD_FLAG_ALL },
	{ "peer", WL_PROXD_TLV_ID_PEER_MAC, "peer mac address",
	FTM_SUBCMD_FLAG_ALL },
	{ "rx-max-burst", WL_PROXD_TLV_ID_RX_MAX_BURST, "limit bursts for rx(method only)",
	FTM_SUBCMD_FLAG_METHOD },
	/* special 'config options', no matching TLV */
	{ "options", WL_PROXD_TLV_ID_NONE,
	"type 'wl proxd -h ftm config options' for more information",
	FTM_SUBCMD_FLAG_ALL },
	{ "session-options", WL_PROXD_TLV_ID_NONE,
	"type 'wl proxd -h ftm config options' for more information",
	FTM_SUBCMD_FLAG_ALL },
	{ "avail", WL_PROXD_TLV_ID_NONE,
	"type 'wl proxd -h ftm config avail' for more information",
	FTM_SUBCMD_FLAG_ALL },
	{ "dev-addr", WL_PROXD_TLV_ID_DEV_ADDR, "deivce address(method only)",
	FTM_SUBCMD_FLAG_METHOD },
	{ "req-retries", WL_PROXD_TLV_ID_FTM_REQ_RETRIES, "number of FTM request retries",
	FTM_SUBCMD_FLAG_ALL },
	{ "tpk", WL_PROXD_TLV_ID_TPK, "tpk to be configured", FTM_SUBCMD_FLAG_ALL },
	{ "cur-etheraddr", WL_PROXD_TLV_ID_CUR_ETHER_ADDR, "ether address for tx",
	FTM_SUBCMD_FLAG_ALL}
};

/* map a specified text-string to a proxd time unit
*
*  return true if succeeds, otherwise return false.
*/
static bool
ftm_get_tmu_from_str(char *str, wl_proxd_tmu_t *p_tmu)
{
	if (stricmp(str, "tu") == 0)
		*p_tmu = WL_PROXD_TMU_TU;
	else if (stricmp(str, "s") == 0)
		*p_tmu = WL_PROXD_TMU_SEC;
	else if (stricmp(str, "ms") == 0)
		*p_tmu = WL_PROXD_TMU_MILLI_SEC;
	else if (stricmp(str, "us") == 0)
		*p_tmu = WL_PROXD_TMU_MICRO_SEC;
	else if (stricmp(str, "ns") == 0)
		*p_tmu = WL_PROXD_TMU_NANO_SEC;
	else if (stricmp(str, "ps") == 0)
		*p_tmu = WL_PROXD_TMU_PICO_SEC;
	else
		return FALSE;

	return TRUE;
}

/*
* get the config_param_info from the table based on 'param-name' for 'wl proxd ftm config' command
*
* return NULL if 'param-name' is not supported
*/
static const ftm_config_param_info_t *
ftm_get_config_param_info(char *p_param_name, wl_proxd_session_id_t session_id)
{
	int	i;
	ftm_subcmd_flag_t		search_cmdflag;
	const ftm_config_param_info_t	*p_config_param_info;

	/* determine if this is for 'session' or 'method' config */
	search_cmdflag = (session_id == WL_PROXD_SESSION_ID_GLOBAL) ? FTM_SUBCMD_FLAG_METHOD
		: FTM_SUBCMD_FLAG_SESSION;

	p_config_param_info = &ftm_config_param_info[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_config_param_info); i++) {
		if (stricmp(p_param_name, p_config_param_info->name) == 0) {
			/* check if this config-param is supported for method/session */
			if ((p_config_param_info->cmdflag & search_cmdflag) == 0) {
				/* not supported for this method/session command */
				return (ftm_config_param_info_t *) NULL;
			}

			return p_config_param_info;
		}
		p_config_param_info++;		/* next */
	}

	return (ftm_config_param_info_t *) NULL;		/* 'invalid param name' */
}

/*
* define param-value info for global/session 'options' configuration for
*    'wl proxd ftm [<session-id>] config options {[+|-]<param-value>}*'
*/
typedef struct ftm_config_options_info {
	char	*param_value_str;	/* <param-value> str to identify an options-flag */
	uint32	flags;				/* wl_proxd_flags_t/wl_proxd_session_flags_t */
	char	*helpmsg;			/* help message for the param-value */
} ftm_config_options_info_t;

/* global/method config options/flags */
static const ftm_config_options_info_t ftm_method_options_info[] = {
	/* param-value-string	wl_proxd_flags_t			helpmsg */
	{ "rx-enable", WL_PROXD_FLAG_RX_ENABLED,
	"If enabled, process requests; If disabled, requests will be ignored"},
	{ "rx-range-req", WL_PROXD_FLAG_RX_RANGE_REQ,
	"If enabled, process 11mc range requests; If disabled, requests will be ignored" },
	{ "tx-lci", WL_PROXD_FLAG_TX_LCI, "transmit LCI, if available" },
	{ "tx-civic", WL_PROXD_FLAG_TX_CIVIC, "tx civic location, if available" },
	{ "rx-auto-burst", WL_PROXD_FLAG_RX_AUTO_BURST,
	"If enabled, respond to requests without host action.\n"
	"\t\t                                     "
	"If disabled, the request is forwarded to host via an event.\n"
	"\t\t                                     "
	"If the event is masked, the request is dropped" },
	{ "tx-auto-burst", WL_PROXD_FLAG_TX_AUTO_BURST,
	"If enabled, continue an initiated session with a new burst without host action\n"
	"\t\t                                     "
	"until the number of bursts for the session is completed" },
	{ "avail-publish", WL_PROXD_FLAG_AVAIL_PUBLISH, "publish local availability" },
	{ "avail-schedule", WL_PROXD_FLAG_AVAIL_SCHEDULE, "schedule local availability" },
	{ "asap-capable", WL_PROXD_FLAG_ASAP_CAPABLE, "capable of ASAP scheduling"},
	{ "mburst-followup", WL_PROXD_FLAG_MBURST_FOLLOWUP,
	"enable multi-burst followup algorithm " },
	{ "secure", WL_PROXD_FLAG_SECURE,
	"Enable security for ftm - per BSS" },
	{ "all", WL_PROXD_FLAG_ALL, "all of the above" }
};

/* config session options/flags */
static const ftm_config_options_info_t ftm_session_options_info[] = {
	/* param-value-string	wl_proxd_session_flags_t	helpmsg */
	{ "initiator",	WL_PROXD_SESSION_FLAG_INITIATOR, "local device is an initiator" },
	{ "target",		WL_PROXD_SESSION_FLAG_TARGET, "local device is a target" },
	{ "one-way",	WL_PROXD_SESSION_FLAG_ONE_WAY, "(initiated) 1-way rtt " },
	{ "auto-burst",	WL_PROXD_SESSION_FLAG_AUTO_BURST, "created with rx_auto_burst" },
	{ "immediate", WL_PROXD_SESSION_FLAG_MBURST_NODELAY, "immediate next burst" },
	{ "rtt-detail",	WL_PROXD_SESSION_FLAG_RTT_DETAIL, "provide rtt detail in results" },
	{ "secure", WL_PROXD_SESSION_FLAG_SECURE, "secure this session" },
	{ "rx-auto-burst",	WL_PROXD_SESSION_FLAG_RX_AUTO_BURST,
	"Same as proxd flags above, applies to the session" },
	{ "tx-auto-burst",	WL_PROXD_SESSION_FLAG_TX_AUTO_BURST,
	"Same as proxd flags above, applies to the session" },
	{ "ts1", WL_PROXD_SESSION_FLAG_TS1, "FTM1 - ASAP-capable" }, /* readonly */
	{ "randmac", WL_PROXD_SESSION_FLAG_RANDMAC, "Use random mac on initiator" },
	{ "initiator-rpt", WL_PROXD_SESSION_FLAG_INITIATOR_RPT,
	"tx initiator-report to target" },
	{ "neutral", WL_PROXD_SESSION_FLAG_NETRUAL, "neutral mode" },
	{"no-param-override", WL_PROXD_SESSION_FLAG_NO_PARAM_OVRD,
	"disallow override from target"},
	{"asap", WL_PROXD_SESSION_FLAG_ASAP, "use ASAP scheduling"},
	{ "tx-lci-req", WL_PROXD_SESSION_FLAG_REQ_LCI, "transmit LCI request" },
	{ "tx-civic-req", WL_PROXD_SESSION_FLAG_REQ_CIV, "tx civic location request" },
	{ "pre-scan", WL_PROXD_SESSION_FLAG_PRE_SCAN, "enable asap pre-scan on initiator" },
	{ "auto-vhtack", WL_PROXD_SESSION_FLAG_AUTO_VHTACK, "use vhtack based on brcm ie" },
	{ "vhtack", WL_PROXD_SESSION_FLAG_VHTACK, "vht ack is in use - output only" },
	{ "burst-duration-nopref", WL_PROXD_SESSION_FLAG_BDUR_NOPREF,
	"duration for a single burst - no preference" },
	{ "num-ftm-nopref", WL_PROXD_SESSION_FLAG_NUM_FTM_NOPREF,
	"number of FTM frames in a burst - no preference" },
	{ "ftm-sep-nopref", WL_PROXD_SESSION_FLAG_FTM_SEP_NOPREF,
	"time between FTM frames in a burst - no preference " },
	{ "num-burst-nopref", WL_PROXD_SESSION_FLAG_NUM_BURST_NOPREF,
	"number of bursts in a session - no preference " },
	{ "burst-period-nopref", WL_PROXD_SESSION_FLAG_BURST_PERIOD_NOPREF,
	"time between bursts - no preference " },
	{ "all",		WL_PROXD_SESSION_FLAG_ALL, "all of the above" }
};

static void
ftm_cmn_display_config_options_value(uint32 flags,
	const ftm_config_options_info_t *p_table, uint32 num_entries)
{
	const ftm_config_options_info_t *p_entry;
	uint32		i;

	if (flags) {
		printf(" (");

		/* walk thru the table to find a match indicating
		the specified param_value_str is valid
		*/
		p_entry = p_table;
		for (i = 0; i < num_entries; i++) {
			if ((p_entry->flags & flags) == p_entry->flags) {
				printf(" %s", p_entry->param_value_str);
			}

			p_entry++;		/* next */
		}

		printf(" )");

	}
	printf("\n");
}

static void
ftm_unpack_and_display_session_flags(const uint8 *p_data, uint16 tlvid)
{
	wl_proxd_session_flags_t flags = ltoh32_ua(p_data);

	printf("> %s: 0x%x", ftm_tlvid_to_logstr(tlvid), flags);
	/* display the value in a readable format */
	ftm_cmn_display_config_options_value((uint32) flags,
		&ftm_session_options_info[0], ARRAYSIZE(ftm_session_options_info));
}

static void
ftm_unpack_and_display_config_flags(const uint8 *p_data, uint16 tlvid)
{
	wl_proxd_flags_t flags = ltoh32_ua(p_data);

	printf("> %s: 0x%x", ftm_tlvid_to_logstr(tlvid), flags);
	/* display the value in a readable format */
	return ftm_cmn_display_config_options_value((uint32) flags,
		&ftm_method_options_info[0], ARRAYSIZE(ftm_method_options_info));
}

/* get the method/session config-options info from the table based on 'options param_value_str'
* Input:
*       param_value_str -- a 'config options' param-value string to identify a config-options flag
*       useMethod -- set to true to look up 'config options' table for FTM method
*                    set to false to look up 'config options' table for a session
* if succeeds, return a pointer to the config_options_info, otherwise return NULL
*/
static const ftm_config_options_info_t *
ftm_get_config_options_info(char *param_value_str, bool useMethod)
{
	int	i;
	const ftm_config_options_info_t	*p_entry;
	int	num_entries;
	if (useMethod) {		/* choose an options table for method command */
		p_entry = &ftm_method_options_info[0];
		num_entries = ARRAYSIZE(ftm_method_options_info);
	}
	else {					/* choose an options table for sessions command */
		p_entry = &ftm_session_options_info[0];
		num_entries = ARRAYSIZE(ftm_session_options_info);
	}

	/* walk thru the table to find a match indicating the specified param_value_str is valid */
	for (i = 0; i < num_entries; i++) {
		if (stricmp(param_value_str, p_entry->param_value_str) == 0)
			return p_entry;
		p_entry++;		/* next */
	}

	return (ftm_config_options_info_t *) NULL;		/* 'invalid param value' */
}

/*
* handle 'wl proxd ftm [<session-id>] config options [+|-]param-value'
*   parse cmd-line, setup method/sessions options/flags TLVs in caller
*   provided buffer and adjusted-buffer-space if applies.
* Note, input '<session-id>' is used to determine if config-options is
*   for 'global/method' or 'sessions'.
*
* This function is invoked from ftm_subcmd_config().
*/
static int
ftm_handle_config_options(wl_proxd_session_id_t session_id, char **argv,
	wl_proxd_tlv_t **p_tlv, uint16 *p_buf_space_left, bool useMethod)
{
	bool	bEnable;
	int		status;
	char	*p_param_value;
	uint32 flags = WL_PROXD_FLAG_NONE;
	uint32 flags_mask = WL_PROXD_FLAG_NONE;
	uint32 new_mask;		/* cmdline input */
	const ftm_config_options_info_t *p_options_info;

	UNUSED_PARAMETER(session_id);

	/* check if user provides param-values for 'config options' command */
	if (*argv == NULL)
		return BCME_USAGE_ERROR;

	/* <param-value>  followed 'wl proxd ftm [session-id] config options' */
	while (*argv != NULL) {
		bEnable = TRUE;		/* default: set the flag if '+/-' is omitted */
		if ((*argv)[0] == '-' || (*argv)[0] == '+') {
			bEnable = ((*argv)[0] == '-') ? FALSE : TRUE;
			p_param_value = *argv + 1;			/* skip the prefix */
			if (*p_param_value == '\0')
				return BCME_USAGE_ERROR;
		}
		else
			p_param_value = *argv;

		/* check if 'param-value' (specified in ascii-string) is valid */
		p_options_info = ftm_get_config_options_info(p_param_value, useMethod);
		if (p_options_info != (ftm_config_options_info_t *) NULL) {
			new_mask = p_options_info->flags;
		}
		else {
			/* check if specifed as an intergral number string */
			new_mask = strtoul(p_param_value, NULL, 0);
			if (new_mask == 0) {			/* conversion error, abort */
				printf("error: invalid param-value(%s)\n", p_param_value);
				return BCME_USAGE_ERROR;	/* param-value is invalid  */
			}
		}

		/* update flags mask */
		flags_mask = flags_mask | new_mask;
		if (bEnable)
			flags |= new_mask;	/* set the bit on */
		else
			flags &= ~new_mask;	/* set the bit off */

		argv++;					/* continue on next <param-value> */
	}

#ifdef WL_FTM_DEBUG
	printf("%s: about to set TLVs for %s-options flags_mask=0x%x flags=0x%x\n",
		__FUNCTION__, useMethod ? "method" : "sessions", flags_mask, flags);
#endif /* WL_FTM_DEBUG */

	flags = htol32(flags);
	flags_mask = htol32(flags_mask);

	/* setup flags_mask TLV */
	status = bcm_pack_xtlv_entry((uint8 **)p_tlv, p_buf_space_left,
		useMethod ? WL_PROXD_TLV_ID_FLAGS_MASK : WL_PROXD_TLV_ID_SESSION_FLAGS_MASK,
		sizeof(uint32), (uint8 *)&flags_mask, BCM_XTLV_OPTION_ALIGN32);
	if (status != BCME_OK) {
#ifdef WL_FTM_DEBUG
		printf("%s: bcm_pack_xltv_entry() for flags_mask failed, status=%d\n",
			__FUNCTION__, status);
#endif /* WL_FTM_DEBUG */
		return status;				/* abort */
	}

	/* setup flags TLV */
	status = bcm_pack_xtlv_entry((uint8 **)p_tlv, p_buf_space_left,
		useMethod ? WL_PROXD_TLV_ID_FLAGS : WL_PROXD_TLV_ID_SESSION_FLAGS,
		sizeof(uint32), (uint8 *)&flags, BCM_XTLV_OPTION_ALIGN32);
	if (status != BCME_OK) {
#ifdef WL_FTM_DEBUG
		printf("%s: bcm_pack_xltv_entry() for flags failed, status=%d\n",
			__FUNCTION__, status);
#endif /* WL_FTM_DEBUG */
		return status;				/* abort */
	}

	return BCME_OK;
}

/*
	num_burst must be power of 2 (max exponent is 14)
*/
static int
ftm_validate_num_burst(uint16 num_burst, const ftm_config_param_info_t *p_config_param_info)
{
#define WL_FTM_MAX_NUM_BURST_EXP	14
	uint16 exponent;
	uint64 x_num_burst;
	int i;

	/* get the exponent value */
	exponent = 0;
	x_num_burst = (uint64) num_burst;
	for (i = 0; i < (int) (sizeof(num_burst) * NBBY); ++i, x_num_burst >>= 1) {
		if (x_num_burst & 0x1)
			exponent = i;
	}

	if (exponent > WL_FTM_MAX_NUM_BURST_EXP) {
		printf("error:' %s' value is out of range (should not exceed 0x%x)\n",
			p_config_param_info->name, 1 << WL_FTM_MAX_NUM_BURST_EXP);
		return BCME_RANGE;
	}

	if (num_burst != (1 << exponent)) {
		printf("error: '%s' value must be power of 2\n",
			p_config_param_info->name);
		return BCME_RANGE;	/* not power of 2 */
	}

	return BCME_OK;
}

/*
	parse 'param-value' for 'ftm config' command
*/
static int
ftm_config_parse_ul(char **argv, const ftm_config_param_info_t *p_config_param_info,
	unsigned long int min_value, unsigned long int max_value,
	unsigned long int *out_data)
{
	int status = BCME_OK;
	unsigned long int	tmp_data_ul;

	tmp_data_ul = strtoul(*argv, NULL, 0);
	if (tmp_data_ul < min_value || tmp_data_ul > max_value) {
		printf("error: '%s' value is out of range\n",
			p_config_param_info->name);
		return BCME_RANGE;
	}

	/* validate the arguments */
	if (p_config_param_info->tlvid == WL_PROXD_TLV_ID_NUM_BURST ||
		p_config_param_info->tlvid == WL_PROXD_TLV_ID_RX_MAX_BURST) {
		status = ftm_validate_num_burst((uint16) tmp_data_ul, p_config_param_info);
		if (status != BCME_OK)
			return status;
	}

	/* param-value parsing: success */
	*out_data = tmp_data_ul;

	return BCME_OK;
}

/*
* parse and pack 'chanspec' from a command-line argument
* Input:
*	arg_channel: a channel-string
*	out_channel: buffer to store the result
*/
static int
ftm_config_parse_channel(char *arg_channel, uint32 *out_channel)
{
	int	status = BCME_OK;
	uint32 src_data_channel = 0;	/* default: invalid */

	/* note, chanespec_t is currently defined as 16-bit, however */
	/* wl-interface use 'uint32' to allow future change for 32-bit */
	if (arg_channel != (char *) NULL)
		src_data_channel = (uint32) wf_chspec_aton(arg_channel);

	if (src_data_channel == 0) {
		printf("error: invalid chanspec\n");
		status = BCME_BADARG;
	}

	*out_channel = htol32(src_data_channel);
	return status;
}

/*
* parse and pack 'intvl' param-value from a command-line argument
* Input:
*	arg_intvl: a time-intvl string
*	out_intvl: buffer to store the result
*/
static int
ftm_config_parse_intvl(char *arg_intvl, wl_proxd_intvl_t *out_intvl)
{
	wl_proxd_intvl_t	src_data_intvl;
	char *p_end;

	/* initialize */
	memset(out_intvl, 0, sizeof(*out_intvl));

	if (arg_intvl == (char *) NULL) {
		printf("error: time-interval value is not specified\n");
		return BCME_BADARG;
	}

	errno = 0;
	memset(&src_data_intvl, 0, sizeof(src_data_intvl));
	/* time interval e.g. 10ns */
	/* get the number */
	p_end = NULL;
	src_data_intvl.intvl = htol32(strtoul(arg_intvl, &p_end, 10));
	if (errno) {
		printf("error: invalid time interval (errno=%d)\n", errno);
		return BCME_BADARG;
	}

	/* get time-unit */
	src_data_intvl.tmu = WL_PROXD_TMU_TU; /* default */
	if (*p_end != '\0') {
		if (!ftm_get_tmu_from_str(p_end, &src_data_intvl.tmu)) {
			printf("error: invalid time-unit %s\n", p_end);
			return BCME_BADARG;
		}
	}
	src_data_intvl.tmu = htol16(src_data_intvl.tmu);

	/* return to caller */
	memcpy(out_intvl, &src_data_intvl, sizeof(*out_intvl));

	return BCME_OK;
}

/*
* parse and pack 'intvl' param-value from a command-line argument
* Input:
*	arg_tpk: tpk param-value argument string
*	out_tpk: buffer to store tpk and mac address
*/
static int
ftm_config_parse_tpk_peer(char *arg_tpk, wl_proxd_tpk_t *out_tpk)
{
	wl_proxd_tpk_t	src_data_tpk;

	/* initialize */
	memset(out_tpk, 0, sizeof(*out_tpk));

	if (arg_tpk == (char *) NULL) {
		printf("error: tpk value is not specified\n");
		return BCME_BADARG;
	}

	errno = 0;
	memset(&src_data_tpk, 0, sizeof(src_data_tpk));
	/* get link mac address */
	if (!wl_ether_atoe(arg_tpk, &src_data_tpk.peer))
		return BCME_USAGE_ERROR;
	/* get TPK */
	if (*arg_tpk) {
		memcpy(&src_data_tpk.peer, arg_tpk, ETHER_ADDR_LEN);
	}

	/* return to caller */
	memcpy(out_tpk, &src_data_tpk, sizeof(*out_tpk));

	return BCME_OK;
}

/*
* parse and pack one 'config avail slot' param-value from a command-line
* Input:
*	arg_slot: 'slot' param-value argument string
*             in "channel:start-tmu:duration-tmu" format
*	out_avail_slot: buffer to store the result
*/
static int
ftm_config_avail_parse_slot(char *arg_slot, wl_proxd_time_slot_t *out_avail_slot)
{
	int arg_idx;
	const char *tmp_start, *tmp_end;
	char tmpbuf[128];
	int len;
	int	status = BCME_OK;

	if (arg_slot == (char *) NULL) {
		printf("error: slot value is not specified\n");
		return BCME_BADARG;
	}

	/* parse channel:start-tmu:duration-tmu */
	tmp_start = arg_slot;
	for (arg_idx = 0; arg_idx < 3; arg_idx++) {
		tmp_end = strchr(tmp_start, ':');
		if (tmp_end == NULL) {
			if (arg_idx != 2 || *tmp_start == '\0') {
				status = BCME_BADARG;
				goto done;
			}
			/* for last 'duration intvl' */
			tmp_end = tmp_start + strlen(tmp_start);
		}

		/* create a temp null-terminated substring */
		if ((len = tmp_end - tmp_start) >= (int) sizeof(tmpbuf)) {
			status = BCME_BADARG;
			goto done;
		}

		memcpy(tmpbuf, tmp_start, len);
		tmpbuf[len] = '\0';	/* null-terminate */

		if (arg_idx == 0)
			status = ftm_config_parse_channel(tmpbuf, &out_avail_slot->chanspec);
		else if (arg_idx == 1)
			status = ftm_config_parse_intvl(tmpbuf, &out_avail_slot->start);
		else /* arg_idx == 2 */
			status = ftm_config_parse_intvl(tmpbuf, &out_avail_slot->duration);

		if (status != BCME_OK)
			goto done;
		/* continue on next element */
		tmp_start = tmp_end + 1;

	}
	/* make sure no string beyond 'channel:start-tmu:duration-tmu' */
	if (*tmp_end != '\0') {
		printf("error: invalid 'slot' value '%s'\n", tmp_end);
		status = BCME_BADARG;
	}

done:
	if (status == BCME_BADARG)
		printf("error: invalid value for slot\n");

	return status;
}

/*
* parse and pack 'config avail time-ref' param-value from a command-line
* Input:
*	arg_tref: 'time-ref' param-value argument string
*                    in "none|dev-tsf|nan-dw|tbtt" format
*      out_tref: buffer to store the result
*/
static int
ftm_config_avail_parse_tref(char *arg_tref, wl_proxd_time_ref_t *out_tref)
{
	wl_proxd_time_ref_t src_data_tref;
	const ftm_strmap_entry_t	*p_entry;

	if (arg_tref == (char *) NULL) {
		printf("error: time-ref value is not specified\n");
		return BCME_BADARG;
	}

	/* loop up */
	p_entry = ftm_get_strmap_info_strkey(arg_tref, &ftm_avail_timeref_value_loginfo[0],
		ARRAYSIZE(ftm_avail_timeref_value_loginfo));
	if (p_entry)
		src_data_tref = p_entry->id;
	else {
		printf("error: invalid time-ref value\n");
		return BCME_BADARG;
	}

	*out_tref = htol16((uint16) src_data_tref);

	return BCME_OK;

}

#define FTM_AVAIL_MAX_SLOTS		32	/* also in pdftmpvt.h */
/*
* parse and pack a list of 'slot-value' for availability
* Input:
*	argv -- point to something like "ch1:start-tmu1:duration-tmu1
*                                    ch2:start-tmu2:duration-tmu2 ..."
*/
static int
ftm_config_avail_parse_all_slots(char **argv, wl_proxd_avail_t *avail, uint16 *out_num_slots)
{
	int	err = BCME_OK;
	uint16	num_slots;
	wl_proxd_time_slot_t	*avail_slot;

	*out_num_slots = 0;

	num_slots = 0;
	avail_slot = WL_PROXD_AVAIL_TIMESLOTS(avail);
	while (*argv != NULL) {	/* parse each slot-value */
		if (num_slots >= FTM_AVAIL_MAX_SLOTS) {
			printf("error: number of slots exceed the limit (%d)\n",
				FTM_AVAIL_MAX_SLOTS);
			err = BCME_BADARG;	/* too many time-slots */
			goto done;
		}

		/* parse channel:start-tmu:duration-tmu */
		err = ftm_config_avail_parse_slot(*argv, avail_slot);
		if (err != BCME_OK)
			goto done;

		num_slots++;
		avail_slot++;

		++argv;		/* continue on next slot-value */
	}

	if (num_slots == 0) {
		printf("error: slot-value is not specified\n");
		err = BCME_BADARG;
		goto done;
	}

	*out_num_slots = num_slots;

done:
	return err;
}

/*
* allocate the availability info
* if succeeds, an avail-buffer with 'max_slots' will be allocated
*/
static int
ftm_config_avail_alloc(wl_proxd_avail_t **out_avail)
{
	uint16 bufsize;
	wl_proxd_avail_t	*avail;

	/* init */
	*out_avail = (wl_proxd_avail_t *) NULL;

	bufsize = WL_PROXD_AVAIL_SIZE(avail, FTM_AVAIL_MAX_SLOTS);
	avail = calloc(1, bufsize);
	if (avail == NULL) {
		printf("error: failed to allocate %d bytes of memory for avail\n",
			bufsize);
		return BCME_NOMEM;
	}

	/* initialize */
	avail->flags = htol16(WL_PROXD_AVAIL_NONE); 	/* don't care, for query only */
	avail->time_ref = htol16(WL_PROXD_TREF_NONE);
	avail->max_slots = htol16(FTM_AVAIL_MAX_SLOTS);
	avail->num_slots = htol16(0);

#define WLU_PROXD_AVAIL_REPEAT_TU 512 /* default interval */
	avail->repeat.intvl = htol32(WLU_PROXD_AVAIL_REPEAT_TU);
	avail->repeat.tmu = htol16(WL_PROXD_TMU_TU);

	*out_avail = avail;

	return BCME_OK;
}

/* Support AVAIL24 */
/*
* allocate and convert the availability info to AVAIL24 format
* if succeeds, an avail-buffer with 'in_avail_num_slots' time slot in AVAIL24
* format will be allocated.
*/
static int
ftm_config_avail_to_avail24(wl_proxd_avail_t *in_avail,
	uint16		in_avail_num_slots, wl_proxd_avail24_t **out_avail)
{
	uint16 bufsize;
	wl_proxd_avail24_t	*avail24 = NULL;
	wl_proxd_time_slot_t *avail_slot, *avail24_slot;

	/* init */
	*out_avail = (wl_proxd_avail24_t *) NULL;

	bufsize = WL_PROXD_AVAIL24_SIZE(avail24, in_avail_num_slots);
	avail24 = calloc(1, bufsize);
	if (avail24 == NULL) {
		printf("error: failed to allocate %d bytes of memory for avail\n",
			bufsize);
		return BCME_NOMEM;
	}

	/* convert the header */
	avail24->flags = in_avail->flags;
	avail24->time_ref = in_avail->time_ref;
	avail24->max_slots = htol16(in_avail_num_slots);	/* don't care, for query only */
	avail24->num_slots = avail24->max_slots;
	avail24->repeat.intvl = in_avail->repeat.intvl;
	avail24->repeat.tmu = in_avail->repeat.tmu;

	/* convert time-slot */
	avail_slot = WL_PROXD_AVAIL_TIMESLOTS(in_avail);
	avail24_slot = WL_PROXD_AVAIL24_TIMESLOTS(avail24); /* &avail24->ts0[0]; */

	memcpy(avail24_slot, avail_slot, in_avail_num_slots * sizeof(avail24->ts0[0]));

	*out_avail = avail24;

	return BCME_OK;
}

/*
* parse cmd-line for 'wl proxd ftm [<session-id>] config avail' command
*	{ [ftm-ref] none |
*     time-ref dev-tsf|nan-adv|tbtt repeat repeat-tmu slot {channel:start-tmu:duration-tmu}+ }
* if succeeds, an avail info is allocated (data is packed) and returned.
* Also, number of time-slots associated with this buffer will be returned
* in 'out_num_slots'.
*/
static int
ftm_pack_config_avail_from_cmdarg(char **argv,
	wl_proxd_avail_t **out_avail, uint16 *out_num_slots)
{
	wl_proxd_avail_t		*avail = NULL;
	wl_proxd_time_ref_t		time_ref;
	uint16					num_slots = 0;
	int err = BCME_OK;

	/* init */
	*out_avail = NULL;
	*out_num_slots = 0;

	/* check if user provides <param-name><param-value> for 'config avail' command */
	if (*argv == NULL) {
		err = BCME_USAGE_ERROR;
		goto done;
	}

	/* allocate a buffer for parsing cmd-args */
	err = ftm_config_avail_alloc(&avail);
	if (err != BCME_OK)
		goto done;

	/* parse input arguments */
	num_slots = 0;
	time_ref = htol16(WL_PROXD_TREF_NONE);	/* default */
	while (*argv != NULL) {
		if (stricmp(*argv, "none") == 0) {
			time_ref = htol16(WL_PROXD_TREF_NONE);
		}
		else if (stricmp(*argv, "time-ref") == 0) {
			++argv;	/* advance to 'param-value' */
			err = ftm_config_avail_parse_tref(*argv, &time_ref);
			if (err != BCME_OK)
				goto done;
		}
		else if (stricmp(*argv, "repeat") == 0) {
			++argv;	/* advance to 'param-value' */
			err = ftm_config_parse_intvl(*argv, &avail->repeat);
			if (err != BCME_OK)
				goto done;
		}
		else if (stricmp(*argv, "slot") == 0) {
			if (time_ref == htol16(WL_PROXD_TREF_NONE)) {
				printf("error: time-ref is missing or invalid for slot\n");
				err = BCME_BADARG;
				goto done;
			}
			++argv;	/* advance to 'param-value' */

			err = ftm_config_avail_parse_all_slots(argv, avail, &num_slots);
			if (err != BCME_OK)
				goto done;
			break; /* done parsing */
		}
		else {
			printf("error: invalid param-name (%s)\n", *argv);
			err = BCME_USAGE_ERROR;	/* param-name is not specified */
			goto done;
		}

		++argv;	/* continue on next 'param-name' */
	}

	avail->time_ref = time_ref;
	avail->num_slots = htol16(num_slots);

	*out_avail = avail;
	*out_num_slots = num_slots;
	avail = (wl_proxd_avail_t *) NULL;

done:
	if (err != BCME_OK) {
		if (avail)		/* cleanup */
			free(avail);
	}

	return err;

}

/*
* handle 'wl proxd ftm [<session-id>] config avail
*	{ [ftm-ref] none |
*	  time-ref dev-tsf|nan-adv|tbtt repeat repeat-tmu slot {channel:start-tmu:duration-tmu}+ }
* parse cmd-line, setup local/peer(method/sessions) availability TLVs in caller
* provided buffer and adjusted-buffer-space if applies.
* Note, input '<session-id>' is used to determine if config-avail is
*   for 'local' or 'peer'.
*
* This function is invoked from ftm_subcmd_config().
*/
static int
ftm_do_config_avail_iovar(void *wl, wl_proxd_iov_t *p_proxd_iov, uint16 proxd_iovsize,
	uint16 tlvid, void *avail, int avail_size)
{
	int err = BCME_OK;
	uint16 bufsize;
	wl_proxd_tlv_t *p_tlv;
	uint16	buf_space_left;
	uint16 all_tlvsize;

	/* setup TLVs */
	bufsize = proxd_iovsize - WL_PROXD_IOV_HDR_SIZE;	/* adjust available size for TLVs */
	p_tlv = &p_proxd_iov->tlvs[0];

	/* TLV buffer starts with a full size, will decrement for each packed TLV */
	buf_space_left = bufsize;

	err = bcm_pack_xtlv_entry((uint8 **) &p_tlv, &buf_space_left,
		tlvid, avail_size, (void *) avail,
		BCM_XTLV_OPTION_ALIGN32);
	if (err != BCME_OK) {
		printf("%s: failed to pack TLVs for availability, status=%d\n",
			__FUNCTION__, err);
		goto done;
	}

	/* update the iov header, set len to include all TLVs + header */
	all_tlvsize = (bufsize - buf_space_left);
	p_proxd_iov->len = htol16(all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
	err = wlu_var_setbuf(wl, "proxd", p_proxd_iov,
		all_tlvsize + WL_PROXD_IOV_HDR_SIZE);

done:
	return err;
}

static int
ftm_handle_config_avail(void *wl, wl_proxd_iov_t *p_proxd_iov, uint16 proxd_iovsize,
wl_proxd_session_id_t session_id, char **argv)
{
	int	err = BCME_OK;
	wl_proxd_avail_t	*avail = NULL;
	wl_proxd_avail24_t	*avail24 = NULL;
	uint16				num_slots;
	int					avail_size;

	UNUSED_PARAMETER(session_id);

	/* parse cmd line and alloc/pack the avail info */
	err = ftm_pack_config_avail_from_cmdarg(argv,
		&avail, &num_slots);
	if (err != BCME_OK)
		goto done;

	/* pack and request to config the avail using the latest AVAIL if supported */
	err = ftm_is_tlv_id_supported(wl, WL_PROXD_TLV_ID_AVAIL);
	if (err == BCME_OK) {
		/* adjust size based on num-slots available */
		avail_size = WL_PROXD_AVAIL_SIZE(avail, num_slots);
		err = ftm_do_config_avail_iovar(wl, p_proxd_iov, proxd_iovsize,
			WL_PROXD_TLV_ID_AVAIL, (void *) avail, avail_size);
		goto done;
	}

	/* try AVAIL24 */
	/* convert to AVAIL24 format */
	err = ftm_config_avail_to_avail24(avail,
		num_slots, &avail24);
	if (err != BCME_OK)
		goto done;

	/* adjust size based on num-slots available */
	avail_size = WL_PROXD_AVAIL24_SIZE(avail24, num_slots);
	err = ftm_do_config_avail_iovar(wl, p_proxd_iov, proxd_iovsize,
		WL_PROXD_TLV_ID_AVAIL24, (void *) avail24, avail_size);

done:
	if (avail) /* clean up */
		free(avail);
	if (avail24)
		free(avail24);

	return err;
}

/* proxd ftm config-category definition */
typedef enum {
	FTM_CONFIG_CAT_GENERAL = 1,	/* generial configuration */
	FTM_CONFIG_CAT_OPTIONS = 2,	/* 'config options' */
	FTM_CONFIG_CAT_AVAIL = 3,		/* 'config avail' */
	FTM_CONFIG_CAT_SESSION_OPTIONS  = 4
} ftm_config_category_t;

/*
* parse the config-category from a command-line
* Input:
*   arg_type: a config-category string after 'wl proxd ftm config' from the command-line
*/
static ftm_config_category_t
ftm_parse_config_category(char *arg_category)
{
	if (stricmp(arg_category, "options") == 0)
		return FTM_CONFIG_CAT_OPTIONS;
	if (stricmp(arg_category, "avail") == 0)
		return FTM_CONFIG_CAT_AVAIL;
	if (stricmp(arg_category, "session-options") == 0)
		return FTM_CONFIG_CAT_SESSION_OPTIONS;

	return FTM_CONFIG_CAT_GENERAL;
}

/*
* 'wl proxd ftm config' handler for general-configuration:
*        wl proxd ftm [<session-id>] config [<param-name> <param-value>]+
*/
static int
ftm_handle_config_general(wl_proxd_session_id_t session_id, char **argv,
wl_proxd_tlv_t **p_tlv, uint16 *p_buf_space_left)
{
	int					status = BCME_OK;
	/* data from command line as 'src' */
	unsigned long int	tmp_data_ul;
	uint8				src_data_uint8;
	uint16				src_data_uint16;
	uint32				src_data_uint32;
	struct ether_addr	src_data_mac = ether_null;
	wl_proxd_intvl_t	src_data_intvl;
	wl_proxd_tpk_t		src_data_tpk;

	void		*p_src_data;
	uint16	src_data_size;	/* size of data pointed by p_src_data as 'source' */
	const ftm_config_param_info_t *p_config_param_info;

	/* parse the cmd-line, scan thru all <param-name> <param-value> */
	while (*argv != NULL) {
		/* look up 'config-param-info' based on 'param-name' */
		p_config_param_info = ftm_get_config_param_info(*argv, session_id);
		if (p_config_param_info == (ftm_config_param_info_t *) NULL) {
			printf("error: invalid param-name (%s)\n", *argv);
			status = BCME_USAGE_ERROR;	/* param-name is not specified */
			break;
		}

		/* parse 'param-value' */
		if (*(argv + 1) == NULL) {
			printf("error: invalid param-value\n");
			status = BCME_USAGE_ERROR;	/* param-value is not specified */
			break;
		}
		++argv;

		/* parse param-value, setup tlv-data */
		p_src_data = (void *) NULL;
		src_data_size = 0;
		switch (p_config_param_info->tlvid) {
		case WL_PROXD_TLV_ID_BSS_INDEX:		/* uint8 */
		case WL_PROXD_TLV_ID_FTM_REQ_RETRIES:
		case WL_PROXD_TLV_ID_FTM_RETRIES:
			src_data_uint8 = atoi(*argv);
			p_src_data = (void *) &src_data_uint8;
			src_data_size = sizeof(uint8);
			break;

		case WL_PROXD_TLV_ID_BURST_NUM_FTM:	/* uint16 */
		case WL_PROXD_TLV_ID_NUM_BURST:
		case WL_PROXD_TLV_ID_RX_MAX_BURST:
			if ((status = ftm_config_parse_ul(argv, p_config_param_info,
				(unsigned long int) 0, (unsigned long int) 0xffff,
				&tmp_data_ul)) != BCME_OK)
				break;
			src_data_uint16 = htol16((uint16) tmp_data_ul);
			p_src_data = (void *) &src_data_uint16;
			src_data_size = sizeof(uint16);
			break;

		case WL_PROXD_TLV_ID_TX_POWER:		/* uint32 */
		case WL_PROXD_TLV_ID_RATESPEC:
		case WL_PROXD_TLV_ID_EVENT_MASK:	/* wl_proxd_event_mask_t/uint32 */
		case WL_PROXD_TLV_ID_DEBUG_MASK:	/* allow in 0x######## format */
			if ((status = ftm_config_parse_ul(argv, p_config_param_info,
				(unsigned long int) 0, (unsigned long int) 0xffffffff,
				&tmp_data_ul)) != BCME_OK)
				break;
			/* get the bitmask */
			src_data_uint32 = htol32((uint32) tmp_data_ul);
			p_src_data = &src_data_uint32;
			src_data_size = sizeof(uint32);
			break;

		case WL_PROXD_TLV_ID_CHANSPEC:		/* chanspec_t --> 32bit */
			status = ftm_config_parse_channel(*argv, &src_data_uint32);
			if (status == BCME_OK) {
				p_src_data = (void *) &src_data_uint32;
				src_data_size = sizeof(uint32);
			}
			break;

		case WL_PROXD_TLV_ID_BSSID:		/* mac address */
		case WL_PROXD_TLV_ID_PEER_MAC:
		case WL_PROXD_TLV_ID_DEV_ADDR:
		case WL_PROXD_TLV_ID_CUR_ETHER_ADDR:
			src_data_mac = ether_null;
			if (!wl_ether_atoe(*argv, &src_data_mac)) {
				printf("error: invalid MAC address parameter\n");
				status = BCME_USAGE_ERROR;
				break;
			}
			p_src_data = &src_data_mac;
			src_data_size = sizeof(src_data_mac);
			break;

		case WL_PROXD_TLV_ID_BURST_DURATION:	/* wl_proxd_intvl_t */
		case WL_PROXD_TLV_ID_BURST_PERIOD:
		case WL_PROXD_TLV_ID_BURST_FTM_SEP:
		case WL_PROXD_TLV_ID_BURST_TIMEOUT:
		case WL_PROXD_TLV_ID_INIT_DELAY:
			status = ftm_config_parse_intvl(*argv, &src_data_intvl);
			if (status == BCME_OK) {
				p_src_data = (void *) &src_data_intvl;
				src_data_size = sizeof(src_data_intvl);
			}
			break;

		case WL_PROXD_TLV_ID_TPK:
			status = ftm_config_parse_tpk_peer(*argv, &src_data_tpk);
			if (status == BCME_OK) {
				p_src_data = (void *) &src_data_tpk;
				src_data_size = sizeof(src_data_tpk);
			}
			break;

		default:
			/* not supported now */
			status = BCME_USAGE_ERROR;
			break;
		}

		if (status != BCME_OK)
			break;				/* abort */

		status = bcm_pack_xtlv_entry((uint8 **) p_tlv, p_buf_space_left,
			p_config_param_info->tlvid, src_data_size, p_src_data,
			BCM_XTLV_OPTION_ALIGN32);
		if (status != BCME_OK)
		{
#ifdef WL_FTM_DEBUG
			printf("%s: bcm_pack_xltv_entry() failed, status=%d\n",
				__FUNCTION__, status);
#endif /* WL_FTM_DEBUG */
			break;				/* abort */
		}

		argv++;	/* continue on next <param-name><param-value> */
	}

	return status;

}

/*
* 'wl proxd ftm config' handler, there are two formats:
* For options/flags config, use
*        wl proxd ftm [<session-id>] config options { [+|-]<param-value> }+
* for non-options/flags config, use
*        wl proxd ftm [<session-id>] config [<param-name> <param-value>]+
*/
static int
ftm_subcmd_config(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	ftm_config_category_t	category;
	uint16 proxd_iovsize;
	wl_proxd_iov_t *p_proxd_iov;
	int status = BCME_OK;
	uint16 bufsize;
	wl_proxd_tlv_t *p_tlv;
	uint16	buf_space_left;
	uint16 all_tlvsize;

	if (*argv == NULL) {
		printf("error: config command requires parameters\n");
		/* display 'proxd -h ftm config' helpmsg */
		ftm_display_config_help();
		return BCME_OK;	/* no need to show 'proxd'-level help */
	}

	/* allocate a buffer for proxd-ftm config via 'set' iovar */
	p_proxd_iov = ftm_alloc_getset_buf(method, session_id,
		p_subcmd_info->cmdid, FTM_IOC_BUFSZ, &proxd_iovsize);
	if (p_proxd_iov == (wl_proxd_iov_t *) NULL)
		return BCME_NOMEM;

	/* setup TLVs */
	bufsize = proxd_iovsize - WL_PROXD_IOV_HDR_SIZE;	/* adjust available size for TLVs */
	p_tlv = &p_proxd_iov->tlvs[0];

	/* TLV buffer starts with a full size, will decrement for each packed TLV */
	buf_space_left = bufsize;

	/* parse the cmd-line,  get the config-category and dispatch to the handler */
	/* to parse the parameters based on the 'config-category' */
	category = ftm_parse_config_category(*argv);
	if (category == FTM_CONFIG_CAT_OPTIONS) {
		/* for 'wl proxd ftm [session-id] config options' */
		/* dispatch to setup TLVs for method/session options/flags */
		status = ftm_handle_config_options(session_id, ++argv, &p_tlv,
			&buf_space_left, session_id != WL_PROXD_SESSION_ID_GLOBAL ? FALSE : TRUE);
	} else if (category == FTM_CONFIG_CAT_SESSION_OPTIONS) {
		status = ftm_handle_config_options(session_id, ++argv, &p_tlv,
			&buf_space_left, FALSE);
	} else if (category == FTM_CONFIG_CAT_AVAIL) {
		status = ftm_handle_config_avail(wl, p_proxd_iov, proxd_iovsize,
			session_id, ++argv);
	}
	else
		status = ftm_handle_config_general(session_id, argv, &p_tlv,
			&buf_space_left);

	if (status == BCME_OK && category != FTM_CONFIG_CAT_AVAIL) {
		/* update the iov header, set len to include all TLVs + header */
		all_tlvsize = (bufsize - buf_space_left);
		p_proxd_iov->len = htol16(all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
		status = wlu_var_setbuf(wl, "proxd", p_proxd_iov,
			all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
	}

	if (status == BCME_USAGE_ERROR) {
		if (category == FTM_CONFIG_CAT_OPTIONS)
			ftm_display_config_options_help();
		else if (category == FTM_CONFIG_CAT_AVAIL)
			ftm_display_config_avail_help();
		else
			ftm_display_config_help();
		status = BCME_OK;	/* reset to avoid showing 'proxd'-level help */
	}

	/* clean up */
	free(p_proxd_iov);

#ifdef WL_FTM_DEBUG
	if (status != BCME_OK)
		printf("error: exit %s, status = %d\n", __FUNCTION__, status);
#endif /* WL_FTM_DEBUG */

	return status;
}

static void
ftm_display_cmd_help(const ftm_subcmd_info_t *p_subcmd_info,
const char *cmd_params)
{
	if (p_subcmd_info == (ftm_subcmd_info_t *) NULL)
		return;

	if (p_subcmd_info->helpmsg == (char *) NULL)
		return;

	/* print help messages for a specific FTM sub-command */
	printf("\n\t%s\n", p_subcmd_info->helpmsg);

	/* display cmd usage (these commands require no parameters) */
	printf("\tUsage: wl proxd ftm ");
	if (p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_SESSION) {
		if (p_subcmd_info->cmdflag & FTM_SUBCMD_FLAG_METHOD)
			printf("[<session-id>] ");
		else
			printf("<session-id> ");
	}
	printf("%s%s\n\n",
		p_subcmd_info->name, cmd_params ? cmd_params : "");

}

/*
* pack tlvs for 'wl proxd ftm start-ranging [-d] <sid1> <sid2> ...'
*   parse cmd-line, setup TLVs in caller provided buffer,
*   adjusted buffer-space if applies.
* This function is invoked from ftm_subcmd_start_ranging().
*/
static int
ftm_pack_sids_from_cmdarg(char **argv,
	wl_proxd_tlv_t **p_tlv, uint16 *p_buf_space_left)
{
	char		**tmp_argv;
	uint16		num_sids;
	int			err = BCME_OK;
	uint16		ranging_sids_size;
	wl_proxd_session_id_list_t *ranging_sids = NULL;
	wl_proxd_session_id_t sid;
	uint16		count;

	/* figout out number of session-id from the input */
	tmp_argv = argv;
	num_sids = 0;
	while (*tmp_argv != NULL) {
		tmp_argv++;
		num_sids++;
	}
	if (num_sids == 0) {
		err = BCME_USAGE_ERROR;
		goto done;
	}

	/* allocate a temp buffer for parsing cmd-args */
	ranging_sids_size = OFFSETOF(wl_proxd_session_id_list_t, ids) +
		num_sids * sizeof(wl_proxd_session_id_t);
	ranging_sids = calloc(1, ranging_sids_size);
	if (ranging_sids == (wl_proxd_session_id_list_t *) NULL) {
		printf("error: failed to allocate %d bytes of memory for ranging\n",
			ranging_sids_size);
		err = BCME_NOMEM;
		goto done;
	}

	/* get session-ids followed 'wl proxd ftm start-ranging [-d]' */
	count = 0;
	while (*argv != NULL) {
		sid = (uint16) atoi(*argv);
		ranging_sids->ids[count++] = htol16(sid);
		argv++;			/* continue on next session-id */
	}

	ranging_sids->num_ids = htol16(num_sids);
	err = bcm_pack_xtlv_entry((uint8 **) p_tlv, p_buf_space_left,
		WL_PROXD_TLV_ID_SESSION_ID_LIST, ranging_sids_size,
		(uint8 *)ranging_sids, BCM_XTLV_OPTION_ALIGN32);

	if (err != BCME_OK) {
		printf("%s: failed to pack ranging-ids in xtlv, err=%d\n",
			__FUNCTION__, err);
		goto done;
	}

done:
	/* clean up */
	if (ranging_sids)
		free(ranging_sids);

	return err;
}

/*
* pack tlvs for 'wl proxd ftm start-ranging [-d] <sid1> <sid2> ...'
*   parse cmd-line, setup TLVs in caller provided buffer,
*   adjusted buffer-space if applies.
* This function is invoked from ftm_subcmd_start_ranging().
*/
static int
ftm_pack_ranging_config_from_cmdarg(char **argv,
wl_proxd_tlv_t **p_tlv, uint16 *p_buf_space_left)
{
	int			err = BCME_OK;
	wl_proxd_ranging_flags_t		flags;
	wl_proxd_ranging_flags_t		flags_mask;

	/* parse the ranging-flags '-d' followed 'wl proxd ftm start-ranging' if available */
	flags = WL_PROXD_RANGING_FLAG_NONE;
	while (*argv != NULL) {
		if (stricmp(*argv, "-d") != 0)
			break;	/* skip to handle 'sids' arguments */

		flags = WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP;
		argv++;			/* continue on next session-id */
	}

	/* pack ranging-flags/mask in xTLVs if provided */
	if (flags != WL_PROXD_RANGING_FLAG_NONE) {
		/* setup ranging flags TLV */
		err = bcm_pack_xtlv_entry((uint8 **) p_tlv, p_buf_space_left,
			WL_PROXD_TLV_ID_RANGING_FLAGS,
			sizeof(uint16), (uint8 *)&flags, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			printf("%s: failed to pack ranging-flags in xtlv, err=%d\n",
				__FUNCTION__, err);
			goto done;				/* abort */
		}

		/* setup ranging flags_mask TLV */
		flags_mask = WL_PROXD_RANGING_FLAG_ALL;
		err = bcm_pack_xtlv_entry((uint8 **) p_tlv, p_buf_space_left,
			WL_PROXD_TLV_ID_RANGING_FLAGS_MASK,
			sizeof(uint16), (uint8 *)&flags_mask, BCM_XTLV_OPTION_ALIGN32);
		if (err != BCME_OK) {
			printf("%s: failed to pack ranging flags_mask in xtlv, err=%d\n",
				__FUNCTION__, err);
			goto done;				/* abort */
		}
	}

	/* parse the 'sids' followed 'wl proxd ftm start-ranging [-d]' and
	* pack the parameters in xTLVs if available
	*/
	err = ftm_pack_sids_from_cmdarg(argv, p_tlv, p_buf_space_left);

done:
	if (err != BCME_OK)
		printf("%s failed, err = %d\n",
		__FUNCTION__, err);
	return err;
}

/*
* 'wl proxd ftm start-ranging' handler
*        wl proxd ftm start-ranging [-d] <sid1> <sid2> ...
*/
static int
ftm_subcmd_start_ranging(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	wl_proxd_iov_t *p_proxd_iov = NULL;
	uint16 proxd_iovsize;
	uint16 bufsize;
	wl_proxd_tlv_t *p_tlv;
	uint16	buf_space_left;
	int err = BCME_OK;
	uint16 all_tlvsize;

	UNUSED_PARAMETER(wl);

	/* this should apply to a method command */
	if (*argv == NULL || session_id != WL_PROXD_SESSION_ID_GLOBAL) {
		printf("error: start-ranging command requires parameters\n");
		err = BCME_USAGE_ERROR;
		goto done;
	}

	/* allocate a buffer for proxd-ftm config via 'set' iovar */
	p_proxd_iov = ftm_alloc_getset_buf(method, session_id,
		p_subcmd_info->cmdid, FTM_IOC_BUFSZ, &proxd_iovsize);
	if (p_proxd_iov == (wl_proxd_iov_t *) NULL) {
		err = BCME_NOMEM;
		goto done;
	}

	/* setup TLVs */
	bufsize = proxd_iovsize - WL_PROXD_IOV_HDR_SIZE;	/* adjust available size for TLVs */
	p_tlv = &p_proxd_iov->tlvs[0];

	/* TLV buffer starts with a full size, will decrement for each packed TLV */
	buf_space_left = bufsize;

	/* parse the cmd-line, pack the parameters in TLVs */
	err = ftm_pack_ranging_config_from_cmdarg(argv, &p_tlv,
		&buf_space_left);
	if (err != BCME_OK)
		goto done;

	/* update the iov header, set len to include all TLVs + header */
	all_tlvsize = (bufsize - buf_space_left);
	p_proxd_iov->len = htol16(all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
	err = wlu_var_setbuf(wl, "proxd", p_proxd_iov,
		all_tlvsize + WL_PROXD_IOV_HDR_SIZE);

done:
	/* clean up */
	if (p_proxd_iov)
		free(p_proxd_iov);

	if (err == BCME_USAGE_ERROR) {
		/* display 'proxd -h ftm' helpmsg */
		ftm_display_cmd_help(p_subcmd_info, "[-d] <sid1> <sid2> ...");
		err = BCME_OK;	/* reset to avoid showing 'proxd'-level help */
	}

	return err;
}

/*
* 'wl proxd ftm stop-ranging' handler,
*        wl proxd ftm stop-ranging
*/
static int
ftm_subcmd_stop_ranging(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	/* this should apply to a method command */
	if (session_id != WL_PROXD_SESSION_ID_GLOBAL) {
		printf("error: no session-id is allowed for stop-ranging\n");
		/* display 'proxd -h ftm stop-ranging ' helpmsg */
		ftm_display_cmd_help(p_subcmd_info, "");
		return BCME_OK;	/* no need to show 'proxd'-level help */
	}

	/* issue an iovar call */
	return ftm_subcmd_setiov_no_tlv(wl, p_subcmd_info, method, session_id, argv);
}

/*
* display help-messages for 'wl proxd -h ftm'
*/
static void
ftm_display_method_help()
{
	int i;
	int max_cmdname_len = 20;
	const ftm_subcmd_info_t *p_subcmd_info;

	/* a header */
	printf("\n\tProximity Detection using FTM\n");
	printf("\tUsage: wl proxd ftm [<session-id>] <cmd> [<param-name> <param-value>]*\n");
	printf("\t\t<session-id>: specify a session; If omitted, <cmd> applies to FTM method\n");
	printf("\t\t<cmd>: specify a command from the following list,\n");

	/* show the command list and the help messages for each FTM command */
	p_subcmd_info = &ftm_cmdlist[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_cmdlist); i++) {
		printf("\t\t  %*s: %s\n", max_cmdname_len, p_subcmd_info->name,
			p_subcmd_info->helpmsg);
		p_subcmd_info++;
	}

	printf("\t\t<param-name> <param-value>: <cmd>-specific parameters\n\n");

	printf("\t\ttype 'wl proxd -h ftm config' for configuration command usage.\n");
	printf("\t\ttype 'wl proxd -h ftm config options' "
		"for options-configuration command usage.\n");
	printf("\t\ttype 'wl proxd -h ftm config avail' "
		"for availability-configuration command usage.\n");
	printf("\t\ttype 'wl proxd -h ftm <cmd>' for others command usage.\n\n");

	/* show examples */
	printf("\tExample: wl proxd ftm enable\n\n");

	return;
}

/*
* display help-messages for 'wl proxd -h ftm config'
*/
static void
ftm_display_config_help()
{
	int i;
	int max_param_name_len = 20;
	const ftm_config_param_info_t *p_config_param_info;

	/* a header */
	printf("\n\tConfigure Proximity Detection using FTM\n");
	printf("\tUsage: wl proxd ftm [<session-id>] config { <param-name> <param-value> }+ \n");
	printf("\t\t<session-id>: specify a session id; If omitted, configure global items\n");
	/* print helpmsg for each config-item */
	printf("\t\t<param-name> <param-value>: configuration parameters from the following\n");
	p_config_param_info = &ftm_config_param_info[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_config_param_info); i++) {
		printf("\t\t  %*s: %s\n", max_param_name_len, p_config_param_info->name,
			p_config_param_info->name_helpmsg);

		p_config_param_info++;
	}

	/* show examples */
	printf("\n\tExample: wl proxd ftm config retries 10\n");
	printf("\t         wl proxd ftm 1 config retries 10\n");
}

/*
* display help-messages for 'wl proxd -h ftm config options'
*/
static void
ftm_display_config_options_help()
{
	int i;
	int max_option_name_len = 20;
	const ftm_config_options_info_t	*p_config_options_info;

	/* a header */
	printf("\n\tConfigure-Options Proximity Detection using FTM\n");
	printf("\tUsage: wl proxd ftm [<session-id>] config options { [+|-]<param-value> }+ \n");
	printf("\t\t<session-id>: specify a session id; If omitted, set the global options\n");
	printf("\t\t+|- prefix: to add or remove an option\n");
	printf("\t\t<param-value>: specify an option\n");

	/* print helpmsg for each global option flag */
	printf("\t\t\tConfigurable options for a FTM method:\n");
	p_config_options_info = &ftm_method_options_info[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_method_options_info); i++) {
		printf("\t\t  %*s (0x%08x): %s\n", max_option_name_len,
			p_config_options_info->param_value_str,
			p_config_options_info->flags, p_config_options_info->helpmsg);
		p_config_options_info++;
	}

	/* print helpmsg for each session option flag */
	printf("\n\t\t\tConfigurable options for a specific session:\n");
	p_config_options_info = &ftm_session_options_info[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_session_options_info); i++) {
		printf("\t\t  %*s (0x%08x): %s\n", max_option_name_len,
			p_config_options_info->param_value_str,
			p_config_options_info->flags, p_config_options_info->helpmsg);
		p_config_options_info++;
	}

	/* show examples */
	printf("\n\tExample: wl proxd ftm config options +rx-enable\n");
	printf("\t         wl proxd ftm config options +0x00000001\n");
	printf("\t         wl proxd ftm 1 config options +rx-auto-burst -rtt-detail\n\n");

	return;
}

/*
* display help-messages for 'wl proxd -h ftm config avail'
*/
static void
ftm_display_config_avail_help()
{
	/* a header */
	printf("\n\tConfigure-availability Proximity Detection using FTM\n");
	printf("\t\tUsage: wl proxd ftm [<session-id>] config avail\n");
	printf("\t\t      { [time-ref] none |\n");
	printf("\t\t        time-ref <timeref-value> repeat <repeat-tmu> slot {<slot-value>}+ }\n");
	printf("\t\t<session-id>: a session id; If omitted, set the local availability\n");
	printf("\t\t              otherwise, set the peer availability\n");
	printf("\t\tnone or time-ref none: to clear availability\n");
	printf("\t\t<timeref-value>: a timer reference. Possible values are:\n");
	printf("\t\t              dev-tsf | nan-dw | tbtt\n");
	printf("\t\t<repeat-tmu>: repeat period in time-interval format\n");
	printf("\t\t<slot-value>: a time slot in channel:start-tmu:duration-tmu\n");
	printf("\t\t              channel   -- a channel (see 'wl chanspec')\n");
	printf("\t\t              start-tmu -- start time in time-interval format\n");
	printf("\t\t                           (a number followed by a time unit)\n");
	printf("\t\t              duration-tmu -- duration in time-interval format\n");
	printf("\t\t              Time uints are:\n");
	printf("\t\t                s (seconds) | ms (milliseconds) | us (microseconds)\n");
	printf("\t\t                ns (nanoseconds) | ps (picoseconds) | tu (for TUs,default)\n");
	printf("\t\tMore than one <slot-value>' can be specified for setting\n");
	printf("\t\tmultiple time slots\n");

	/* show examples */
	printf("\n\tExample: wl proxd ftm config avail none\n");
	printf("\t         wl proxd ftm config avail time-ref dev-tsf slot 11:100s:100tu\n");
	printf("\t         wl proxd ftm 1 config avail time-ref dev-tsf slot 11:200s:300tu");
	printf(" 11:600s:400tu\n");

	return;
}

/*
* handle
*        wl proxd -h
*        wl proxd -h ftm
*        wl proxd -h ftm <cmd>
*        wl proxd -h ftm config options
* Output:
*        BCME_USAGE_ERROR -- caller should bring up 'proxd' level help messages
*        BCME_OK          -- this func displays context-help for FTM method,
*                            caller does not need to show any help messages.
*/
static int
ftm_handle_help(char **argv)
{
	const ftm_subcmd_info_t *p_subcmd_info;

	if (!*argv)
		return BCME_USAGE_ERROR;

	if (stricmp(*argv, "ftm") != 0)	/* check 'wl proxd -h ftm' */
		return BCME_USAGE_ERROR;

	++argv;
	if (*argv == NULL) {
		/* show help messages for 'wl proxd -h ftm' */
		ftm_display_method_help();
		return BCME_OK;
	}

	/* parse 'wl proxd -h ftm <cmd>' */
	p_subcmd_info = ftm_get_subcmd_info(*argv);
	if (p_subcmd_info != NULL) {
		if (p_subcmd_info->cmdid == WL_PROXD_CMD_CONFIG) {
			/* help for 'wl proxd -h ftm config' */
			++argv;
			if (*argv != NULL) {		/* parse 'wl proxd -h ftm config options' */
				if (stricmp(*argv, "options") == 0) {
					/* display help for 'wl proxd -h ftm config options' */
					ftm_display_config_options_help();
					return BCME_OK;
				}
				if (stricmp(*argv, "avail") == 0) {
					/* display help for 'wl proxd -h ftm config avail' */
					ftm_display_config_avail_help();
					return BCME_OK;
				}
			}

			/* display help for 'wl proxd -h ftm config' */
			ftm_display_config_help();
			return BCME_OK;

		}
		else {
			/* print help messages for a specific FTM sub-command */
			ftm_display_cmd_help(p_subcmd_info, "");
			return BCME_OK;
		}
	}

	/* invalid <cmd>, display help messages for FTM method */
	ftm_display_method_help();
	return BCME_OK;
}

static const ftm_strmap_entry_t ftm_event_type_loginfo[] = {
	/* wl_proxd_event_type_t,			text-string */
	{ WL_PROXD_EVENT_NONE,				"none" },
	{ WL_PROXD_EVENT_SESSION_CREATE,	"session create" },
	{ WL_PROXD_EVENT_SESSION_START,		"session start" },
	{ WL_PROXD_EVENT_FTM_REQ,			"FTM req" },
	{ WL_PROXD_EVENT_BURST_START,		"burst start" },
	{ WL_PROXD_EVENT_BURST_END,			"burst end" },
	{ WL_PROXD_EVENT_SESSION_END,		"session end" },
	{ WL_PROXD_EVENT_SESSION_RESTART,	"session restart" },
	{ WL_PROXD_EVENT_BURST_RESCHED,		"burst rescheduled" },
	{ WL_PROXD_EVENT_SESSION_DESTROY,	"session destroy" },
	{ WL_PROXD_EVENT_RANGE_REQ,			"range request" },
	{ WL_PROXD_EVENT_FTM_FRAME,			"FTM frame" },
	{ WL_PROXD_EVENT_DELAY,				"delay" },
	{ WL_PROXD_EVENT_VS_INITIATOR_RPT,	"initiator-report " }, /* rx initiator-rpt */
	{ WL_PROXD_EVENT_RANGING,			"ranging " },
	{ WL_PROXD_EVENT_LCI_MEAS_REP,		"LCI measurement report" },
	{ WL_PROXD_EVENT_CIVIC_MEAS_REP,	"civic measurement report" },
	{ WL_PROXD_EVENT_START_WAIT,		"start wait" },
};

#if defined(linux)
static const ftm_strmap_entry_t*
ftm_get_event_type_loginfo(wl_proxd_event_type_t	event_type)
{
	/* look up 'event-type' from a predefined table  */
	return ftm_get_strmap_info((int32) event_type,
		ftm_event_type_loginfo, ARRAYSIZE(ftm_event_type_loginfo));
}

/*
* check FTM event
*    return -1: if event's version does not match. In this case,
*				caller should continue the event checking.
*    return 0: this func reports receving a new-version FTM-event and
*				caller should skip the checking for this event.
*/
static int
ftm_event_check(bcm_event_t *p_bcm_event)
{
	int status;
	wl_proxd_event_t	*p_event;
	uint16 version;
	wl_proxd_event_type_t event_type;
	const ftm_strmap_entry_t *p_loginfo;
	int tlvs_len;

	/* move to bcm event payload, which is proxd event structure */
	p_event = (wl_proxd_event_t *) (p_bcm_event + 1);
	version = ltoh16(p_event->version);
	if (version < WL_PROXD_API_VERSION) {
#ifdef WL_FTM_DEBUG
		printf("ignore non-ftm event version = 0x%x < WL_PROXD_API_VERSION (0x%x)\n",
			version, WL_PROXD_API_VERSION);
#endif /* WL_FTM_DEBUG */
		return -1;	/* let caller handle the old version */
	}

	event_type = (wl_proxd_event_type_t) ltoh16(p_event->type);
#ifdef WL_FTM_DEBUG
	printf("event_type=0x%x, ntoh16()=0x%x, ltoh16()=0x%x",
		p_event->type, ntoh16(p_event->type), ltoh16(p_event->type));
#endif /* WL_FTM_DEBUG */
	p_loginfo = ftm_get_event_type_loginfo(event_type);
	if (p_loginfo == NULL) {
		printf("receive an invalid FTM event %d\n", event_type);
		return 0;					/* ignore this event */
	}

	/* get TLVs len, skip over event header */
	tlvs_len = ltoh16(p_event->len) - OFFSETOF(wl_proxd_event_t, tlvs);
	printf("receive '%s' event: version=0x%x len=%d method=%s sid=%d tlvs_len=%d\n",
		p_loginfo->text,
		version,
		ltoh16(p_event->len),
		ftm_method_value_to_logstr(ltoh16(p_event->method)),
		ltoh16(p_event->sid),
		tlvs_len);

	/* print event contents TLVs */
	if (tlvs_len > 0) {
		/* unpack TLVs and invokes the cbfn to print the event content TLVs */
		status = bcm_unpack_xtlv_buf((void *) NULL, (uint8 *)&p_event->tlvs[0],
			tlvs_len, BCM_XTLV_OPTION_ALIGN32, ftm_unpack_xtlv_cbfn);
		if (status != BCME_OK)
			printf("Failed to unpack xtlv for an event\n");
	}

	return 0; /* indicate we have processed this FTM event */
}
#endif /* linux */
#ifdef WL_FTM_DEBUG
/*
* Format an 'event' bitmask to a text-string to caller-provided buffer
*/
static void
ftm_format_event_mask(wl_proxd_event_type_t event, char *p_strbuf, int bufsize)
{
	int i, nChars;
	int event_enabled_count = 0;

	memset(p_strbuf, 0, bufsize);

	char *p_tmpbuf = p_strbuf;
	const ftm_strmap_entry_t *p_loginfo = &ftm_event_type_loginfo[0];
	for (i = 0; i < (int) ARRAYSIZE(ftm_event_type_loginfo); i++) {
		if (WL_PROXD_EVENT_ENABLED(event, (wl_proxd_event_type_t) p_loginfo->id)) {
			if (event_enabled_count == 0) {
				if ((nChars = snprintf(p_tmpbuf, bufsize, "(")) < 0)
					return;	/* error, ignore */
				bufsize -= nChars;
				p_tmpbuf += nChars;
			}

			nChars = snprintf(p_tmpbuf, bufsize, "'%s' ", p_loginfo->text);
			if (nChars < 0)
				return;			/* abort if error */
			bufsize -= nChars;
			p_tmpbuf += nChars;
			event_enabled_count++;
		}
		p_loginfo++;
	}

	if (event_enabled_count > 0)
		snprintf(p_tmpbuf, bufsize, ")");
}
#endif /* WL_FTM_DEBUG */

/*
* 'wl proxd ftm tune' handler,
*	wl proxd ftm tune
*/
static int
ftm_handle_tune_options(void *wl, char **argv, wl_proxd_tlv_t **p_tlv, uint16 *p_buf_space_left,
	void *tof_tune, int sizeof_tof_tune)
{
	miniopt_t to;
	int opt_err;
	int ret = BCME_USAGE_ERROR;

	if (*argv == NULL)
		return BCME_USAGE_ERROR;

	miniopt_init(&to, "tune", NULL, FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			return BCME_USAGE_ERROR;
		}
		argv += to.consumed;
		ret = proxd_tune_set_param_from_opt("proxd ftm tune", &to, tof_tune,
			wlc_ver_major(wl));
	}
#ifdef WL_FTM_DEBUG
	prhex("tune_opt", (uint8*)tof_tune, sizeof_tof_tune);
#endif /* WL_FTM_DEBUG */
	ret = bcm_pack_xtlv_entry((uint8 **)p_tlv, p_buf_space_left,
		WL_PROXD_TLV_ID_TUNE, sizeof_tof_tune, (uint8*)tof_tune,
		BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		printf("%s: failed to pack proxd ftm tune in xtlv, err=%d\n",
			__FUNCTION__, ret);
		return ret;
	}

	return BCME_OK;
}

/* Used for getting the current options before setting the new ones only */
static int
ftm_tune_cbfn(void *ctx, const uint8 *p_data, uint16 tlvid, uint16 len)
{
	if (tlvid == WL_PROXD_TLV_ID_TUNE) {
#ifdef WL_FTM_DEBUG
		prhex("ftm_tune_cbbn", p_data, len);
#endif /* WL_FTM_DEBUG */
		memcpy(ctx, p_data, len);
	}
	return BCME_OK;
}

static int
ftm_subcmd_tune(void *wl, const ftm_subcmd_info_t *p_subcmd_info,
	wl_proxd_method_t method, wl_proxd_session_id_t session_id, char **argv)
{
	uint16 proxd_tune_method;
	uint16 proxd_iovsize;
	wl_proxd_iov_t *p_proxd_iov, *p_iovresp;
	uint16 bufsize;
	wl_proxd_tlv_t *p_tlv;
	uint16 buf_space_left;
	uint16 all_tlvsize = 0;
	int ret = BCME_OK;
	int proxd_tune_expected_ver;
	int sizeof_proxd_tune;
	void *selected_proxd_tune = NULL;
	wl_proxd_params_tof_tune_v1_t tof_tune_v1;
	wl_proxd_params_tof_tune_v2_t tof_tune_v2;
	wl_proxd_params_tof_tune_v3_t tof_tune_v3;

	proxd_tune_expected_ver = proxd_tune_ver_get(wlc_ver_major(wl));
	sizeof_proxd_tune = proxd_tune_ver_get_size(proxd_tune_expected_ver);

	memset(&tof_tune_v1, 0, sizeof(tof_tune_v1));
	memset(&tof_tune_v2, 0, sizeof(tof_tune_v2));
	memset(&tof_tune_v3, 0, sizeof(tof_tune_v3));

	/* this should apply to a method command */
	if (session_id != WL_PROXD_SESSION_ID_GLOBAL) {
		printf("error: no session-id is allowed for tune\n");
		return BCME_OK;
	}

	/* skip the command name and check if mandatory exists */
	if (!*argv) {
		fprintf(stderr, "missing mandatory parameter \'method\'\n");
		return BCME_USAGE_ERROR;
	}

	/* parse method */
	proxd_tune_method = (uint16)atoi(argv[0]);
	if (proxd_tune_method == 0) {
		fprintf(stderr, "invalid parameter \'method\'\n");
		return BCME_USAGE_ERROR;
	}

	/* only supports PROXD_TOF_METHOD */
	if (proxd_tune_method == PROXD_RSSI_METHOD)
		return BCME_OK;

	if (!*++argv) {
		/* get */
		return ftm_common_getcmd_handler(wl, p_subcmd_info, method, session_id, argv);
	} else {
		/* set */
		/* allocate iovar getset buffer */
		p_proxd_iov = ftm_alloc_getset_buf(method, session_id,
			p_subcmd_info->cmdid, sizeof_proxd_tune,
			&proxd_iovsize);
		if (p_proxd_iov == (wl_proxd_iov_t *) NULL)
			return BCME_NOMEM;
		bufsize = proxd_iovsize - WL_PROXD_IOV_HDR_SIZE;
		p_tlv = &p_proxd_iov->tlvs[0];
		buf_space_left = bufsize;

		/* get current tune params */
		ret = wlu_var_getbuf(wl, "proxd", p_proxd_iov, proxd_iovsize,
			(void *)&p_iovresp);
		if (ret != BCME_OK) {
#ifdef WL_FTM_DEBUG
			printf("%s: failed to send getbuf proxd iovar, status=%d\n",
				__FUNCTION__, ret);
#endif /* WL_FTM_DEBUG */
			goto done;
		}

		if (proxd_tune_expected_ver ==
			WL_PROXD_TUNE_VERSION_MAJOR_1) {
			selected_proxd_tune = &tof_tune_v1;
		} else if (proxd_tune_expected_ver ==
			WL_PROXD_TUNE_VERSION_MAJOR_2) {
			selected_proxd_tune = &tof_tune_v2;
		} else if (proxd_tune_expected_ver ==
			WL_PROXD_TUNE_VERSION_MAJOR_3) {
			selected_proxd_tune = &tof_tune_v3;
		}
		if (p_iovresp != NULL) {
			int tlvs_len = ltoh16(p_iovresp->len) - WL_PROXD_IOV_HDR_SIZE;
			if (tlvs_len > 0 && selected_proxd_tune) {
				ret = bcm_unpack_xtlv_buf(selected_proxd_tune,
					(uint8 *)p_iovresp->tlvs,
					tlvs_len, BCM_XTLV_OPTION_ALIGN32,
					ftm_tune_cbfn);
			}
		}

		/* handle options */
		if (selected_proxd_tune) {
			ret = ftm_handle_tune_options(wl, argv, &p_tlv, &buf_space_left,
				selected_proxd_tune, sizeof_proxd_tune);
		}

		if (ret == BCME_OK) {
			/* prep to transport xtlv */
			all_tlvsize = bufsize - buf_space_left;
			p_proxd_iov->len = htol16(all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
			ret = wlu_var_setbuf(wl, "proxd", p_proxd_iov,
				all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
		}
	}

done:
	/* clean up */
	free(p_proxd_iov);
#ifdef WL_FTM_DEBUG
	if (ret != BCME_OK)
		printf("error: exit %s, status = %d\n", __FUNCTION__, ret);
#endif /* WL_FTM_DEBUG */

	return ret;
}

static void proxd_tune_display_v1(wl_proxd_params_tof_tune_v1_t *tof_tune_v1)
{
	int i = 0;

	if (!tof_tune_v1)
		return;

#ifdef WL_FTM_DEBUG
	printf("%s: Displaying wl_proxd_params_tof_tune_v1_t\n", __FUNCTION__);
#endif /* WL_FTM_DEBUG */

	printf("Ki=%d \n", dtoh32(tof_tune_v1->Ki));
	printf("Kt=%d \n", dtoh32(tof_tune_v1->Kt));
	printf("vhtack=%d \n", dtoh16(tof_tune_v1->vhtack));
	printf("seq_en=%d\n", dtoh16(tof_tune_v1->seq_en));
	printf("core=%d\n", tof_tune_v1->core);
	printf("sw_adj=%d\n", dtoh16(tof_tune_v1->sw_adj));
	printf("hw_adj=%d\n", dtoh16(tof_tune_v1->hw_adj));
	printf("minDT = %d\n", tof_tune_v1->minDT);
	printf("maxDT = %d\n", tof_tune_v1->maxDT);
	printf("threshold_log2=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v1->N_log2[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_log2[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_log2[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_log2[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v1->N_log2[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v1->N_log2_2g),
		dtoh16(tof_tune_v1->seq_5g20.N_tx_log2),
		dtoh16(tof_tune_v1->seq_5g20.N_rx_log2),
		dtoh16(tof_tune_v1->seq_2g20.N_tx_log2),
		dtoh16(tof_tune_v1->seq_2g20.N_rx_log2));
	printf("threshold_scale=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v1->N_scale[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_scale[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_scale[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v1->N_scale[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v1->N_scale[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v1->N_scale_2g),
		dtoh16(tof_tune_v1->seq_5g20.N_tx_scale),
		dtoh16(tof_tune_v1->seq_5g20.N_rx_scale),
		dtoh16(tof_tune_v1->seq_2g20.N_tx_scale),
		dtoh16(tof_tune_v1->seq_2g20.N_rx_scale));
	printf("total_frmcnt=%d \n", tof_tune_v1->totalfrmcnt);
	printf("reserve_media=%d \n", tof_tune_v1->rsv_media);
	printf("flags=0x%x \n", dtoh16(tof_tune_v1->flags));
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("window length %dMHz = %d\n",
			(20 << i), tof_tune_v1->w_len[i]);
		printf("window offset %dMHz = %d\n",
			(20 << i), tof_tune_v1->w_offset[i]);
	}
	printf("seq window length 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v1->seq_5g20.w_len,
		tof_tune_v1->seq_2g20.w_len);
	printf("seq window offset 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v1->seq_5g20.w_offset,
		tof_tune_v1->seq_2g20.w_offset);
	printf("frame count=%d %d %d seq %d\n",
		tof_tune_v1->ftm_cnt[TOF_BW_20MHZ_INDEX],
		tof_tune_v1->ftm_cnt[TOF_BW_40MHZ_INDEX],
		tof_tune_v1->ftm_cnt[TOF_BW_80MHZ_INDEX],
		tof_tune_v1->ftm_cnt[TOF_BW_SEQTX_INDEX]);

	printf("bitflip threshold=%u\n", dtoh16(tof_tune_v1->bitflip_thresh));
	printf("SNR threshold=%u\n", dtoh16(tof_tune_v1->snr_thresh));
	printf("2G receive sensitivity threshold=%d\n", tof_tune_v1->recv_2g_thresh);
	printf("ACS GDV threshold=%u\n", dtoh32(tof_tune_v1->acs_gdv_thresh));
	printf("ACS RSSI threshold=%d\n", tof_tune_v1->acs_rssi_thresh);
	printf("Smoothing window enable=%u\n", tof_tune_v1->smooth_win_en);
	printf("emulator delay=%u\n", dtoh32(tof_tune_v1->emu_delay));
	return;
}

static void proxd_tune_display_v2(wl_proxd_params_tof_tune_v2_t *tof_tune_v2)
{
	int i = 0;

	if (!tof_tune_v2)
		return;

#ifdef WL_FTM_DEBUG
	printf("%s: Displaying wl_proxd_params_tof_tune_v2_t\n", __FUNCTION__);
#endif /* WL_FTM_DEBUG */

	printf("Ki=%d \n", dtoh32(tof_tune_v2->Ki));
	printf("Kt=%d \n", dtoh32(tof_tune_v2->Kt));
	printf("vhtack=%d \n", dtoh16(tof_tune_v2->vhtack));
	printf("seq_en=%d\n", dtoh16(tof_tune_v2->seq_en));
	printf("core=%d\n", tof_tune_v2->core);
	printf("sw_adj=%d\n", dtoh16(tof_tune_v2->sw_adj));
	printf("hw_adj=%d\n", dtoh16(tof_tune_v2->hw_adj));
	printf("minDT = %d\n", tof_tune_v2->minDT);
	printf("maxDT = %d\n", tof_tune_v2->maxDT);
	printf("threshold_log2=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v2->N_log2[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_log2[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_log2[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_log2[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v2->N_log2[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v2->N_log2_2g),
		dtoh16(tof_tune_v2->seq_5g20.N_tx_log2),
		dtoh16(tof_tune_v2->seq_5g20.N_rx_log2),
		dtoh16(tof_tune_v2->seq_2g20.N_tx_log2),
		dtoh16(tof_tune_v2->seq_2g20.N_rx_log2));
	printf("threshold_scale=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v2->N_scale[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_scale[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_scale[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v2->N_scale[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v2->N_scale[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v2->N_scale_2g),
		dtoh16(tof_tune_v2->seq_5g20.N_tx_scale),
		dtoh16(tof_tune_v2->seq_5g20.N_rx_scale),
		dtoh16(tof_tune_v2->seq_2g20.N_tx_scale),
		dtoh16(tof_tune_v2->seq_2g20.N_rx_scale));
	printf("total_frmcnt=%d \n", tof_tune_v2->totalfrmcnt);
	printf("reserve_media=%d \n", tof_tune_v2->rsv_media);
	printf("flags=0x%x \n", dtoh16(tof_tune_v2->flags));
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("window length %dMHz = %d\n",
			(20 << i), tof_tune_v2->w_len[i]);
		printf("window offset %dMHz = %d\n",
			(20 << i), tof_tune_v2->w_offset[i]);
	}
	printf("seq window length 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v2->seq_5g20.w_len,
		tof_tune_v2->seq_2g20.w_len);
	printf("seq window offset 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v2->seq_5g20.w_offset,
		tof_tune_v2->seq_2g20.w_offset);
	printf("frame count=%d %d %d seq %d\n",
		tof_tune_v2->ftm_cnt[TOF_BW_20MHZ_INDEX],
		tof_tune_v2->ftm_cnt[TOF_BW_40MHZ_INDEX],
		tof_tune_v2->ftm_cnt[TOF_BW_80MHZ_INDEX],
		tof_tune_v2->ftm_cnt[TOF_BW_SEQTX_INDEX]);

	printf("bitflip threshold=%u\n", dtoh16(tof_tune_v2->bitflip_thresh));
	printf("SNR threshold=%u\n", dtoh16(tof_tune_v2->snr_thresh));
	printf("2G receive sensitivity threshold=%d\n", tof_tune_v2->recv_2g_thresh);
	printf("ACS GDV threshold=%u\n", dtoh32(tof_tune_v2->acs_gdv_thresh));
	printf("ACS RSSI threshold=%d\n", tof_tune_v2->acs_rssi_thresh);
	printf("Smoothing window enable=%u\n", tof_tune_v2->smooth_win_en);

	printf("ACS GDMM threshold=%u\n", dtoh32(tof_tune_v2->acs_gdmm_thresh));
	printf("ACS RSSI delta threshold=%d\n", tof_tune_v2->acs_delta_rssi_thresh);
	printf("emulator delay=%u\n", dtoh32(tof_tune_v2->emu_delay));
	printf("Core Mask = 0x%x\n", tof_tune_v2->core_mask);

	return;
}

static void proxd_tune_display_v3(wl_proxd_params_tof_tune_v3_t *tof_tune_v3)
{
	int i = 0, internal_version = 0;

	if (!tof_tune_v3)
		return;

#ifdef WL_FTM_DEBUG
	printf("%s: Displaying wl_proxd_params_tof_tune_v3_t\n", __FUNCTION__);
#endif /* WL_FTM_DEBUG */

	printf("Ki=%d \n", dtoh32(tof_tune_v3->Ki));
	printf("Kt=%d \n", dtoh32(tof_tune_v3->Kt));
	printf("vhtack=%d \n", dtoh16(tof_tune_v3->vhtack));
	printf("seq_en=%d\n", dtoh16(tof_tune_v3->seq_en));
	printf("core=%d\n", tof_tune_v3->core);
	printf("sw_adj=%d\n", dtoh16(tof_tune_v3->sw_adj));
	printf("hw_adj=%d\n", dtoh16(tof_tune_v3->hw_adj));
	printf("minDT = %d\n", tof_tune_v3->minDT);
	printf("maxDT = %d\n", tof_tune_v3->maxDT);
	printf("threshold_log2=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v3->N_log2[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_log2[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_log2[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_log2[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v3->N_log2[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v3->N_log2_2g),
		dtoh16(tof_tune_v3->seq_5g20.N_tx_log2),
		dtoh16(tof_tune_v3->seq_5g20.N_rx_log2),
		dtoh16(tof_tune_v3->seq_2g20.N_tx_log2),
		dtoh16(tof_tune_v3->seq_2g20.N_rx_log2));
	printf("threshold_scale=%d %d %d seqtx %d seqrx %d 2g %d seqtx5g20 %d"
		" seqrx5g20 %d seqtx2g20 %d seqrx2g20 %d\n",
		dtoh16(tof_tune_v3->N_scale[TOF_BW_20MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_scale[TOF_BW_40MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_scale[TOF_BW_80MHZ_INDEX]),
		dtoh16(tof_tune_v3->N_scale[TOF_BW_SEQTX_INDEX]),
		dtoh16(tof_tune_v3->N_scale[TOF_BW_SEQRX_INDEX]),
		dtoh16(tof_tune_v3->N_scale_2g),
		dtoh16(tof_tune_v3->seq_5g20.N_tx_scale),
		dtoh16(tof_tune_v3->seq_5g20.N_rx_scale),
		dtoh16(tof_tune_v3->seq_2g20.N_tx_scale),
		dtoh16(tof_tune_v3->seq_2g20.N_rx_scale));
	printf("total_frmcnt=%d \n", tof_tune_v3->totalfrmcnt);
	printf("reserve_media=%d \n", tof_tune_v3->rsv_media);
	printf("flags=0x%x \n", dtoh16(tof_tune_v3->flags));
	for (i = 0; i < TOF_BW_NUM; i++) {
		printf("window length %dMHz = %d\n",
			(20 << i), tof_tune_v3->w_len[i]);
		printf("window offset %dMHz = %d\n",
			(20 << i), tof_tune_v3->w_offset[i]);
	}
	printf("seq window length 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v3->seq_5g20.w_len,
		tof_tune_v3->seq_2g20.w_len);
	printf("seq window offset 5G-20MHz = %2d 2G-20MHz = %2d\n",
		tof_tune_v3->seq_5g20.w_offset,
		tof_tune_v3->seq_2g20.w_offset);
	printf("frame count=%d %d %d seq %d\n",
		tof_tune_v3->ftm_cnt[TOF_BW_20MHZ_INDEX],
		tof_tune_v3->ftm_cnt[TOF_BW_40MHZ_INDEX],
		tof_tune_v3->ftm_cnt[TOF_BW_80MHZ_INDEX],
		tof_tune_v3->ftm_cnt[TOF_BW_SEQTX_INDEX]);

	printf("bitflip threshold=%u\n", dtoh16(tof_tune_v3->bitflip_thresh));
	printf("SNR threshold=%u\n", dtoh16(tof_tune_v3->snr_thresh));
	printf("2G receive sensitivity threshold=%d\n", tof_tune_v3->recv_2g_thresh);
	printf("ACS GDV threshold=%u\n", dtoh32(tof_tune_v3->acs_gdv_thresh));
	printf("ACS RSSI threshold=%d\n", tof_tune_v3->acs_rssi_thresh);
	printf("Smoothing window enable=%u\n", tof_tune_v3->smooth_win_en);

	internal_version = dtoh32(tof_tune_v3->version);
#ifdef WL_FTM_DEBUG
	printf("internal_version %d\n", internal_version);
#endif /* WL_FTM_DEBUG */
	switch (internal_version) {
	case WL_PROXD_TUNE_VERSION_1: /* target */
		printf("emulator delay=%u\n", dtoh32(tof_tune_v3->emu_delay));
		break;
	case WL_PROXD_TUNE_VERSION_2: /* initiator */
	case WL_PROXD_TUNE_VERSION_3: /* initiator */
		printf("ACS GDMM threshold=%u\n", dtoh32(tof_tune_v3->acs_gdmm_thresh));
		printf("ACS RSSI delta threshold=%d\n", tof_tune_v3->acs_delta_rssi_thresh);
		printf("emulator delay=%u\n", dtoh32(tof_tune_v3->emu_delay));
		printf("Core Mask = 0x%x\n", tof_tune_v3->core_mask);
		break;
	default:
		break;
	}

	return;
}

static int
proxd_tune_display(void *tof_tune, uint16 len)
{
	int err = BCME_OK;
	wl_proxd_params_tof_tune_v1_t * tof_tune_v1 = NULL;
	wl_proxd_params_tof_tune_v2_t * tof_tune_v2 = NULL;
	wl_proxd_params_tof_tune_v3_t * tof_tune_v3 = NULL;

	if (!tof_tune) {
		err = BCME_BADADDR;
		goto done;
	}

#ifdef WL_FTM_DEBUG
	printf("len is %d\n", len);
	printf("sizeof(wl_proxd_params_tof_tune_v1_t) is %d \n",
		(int) sizeof(wl_proxd_params_tof_tune_v1_t));
	printf("sizeof(wl_proxd_params_tof_tune_v2_t) is %d \n",
		(int) sizeof(wl_proxd_params_tof_tune_v2_t));
	printf("sizeof(wl_proxd_params_tof_tune_v3_t) is %d \n",
		(int) sizeof(wl_proxd_params_tof_tune_v3_t));
#endif /* WL_FTM_DEBUG */

	if (len == sizeof(wl_proxd_params_tof_tune_v1_t)) {
		tof_tune_v1 = (wl_proxd_params_tof_tune_v1_t *)tof_tune;
		proxd_tune_display_v1(tof_tune_v1);
	} else if (len == sizeof(wl_proxd_params_tof_tune_v2_t)) {
		tof_tune_v2 = (wl_proxd_params_tof_tune_v2_t *)tof_tune;
		proxd_tune_display_v2(tof_tune_v2);
	} else {
		tof_tune_v3 = (wl_proxd_params_tof_tune_v3_t *)tof_tune;
		proxd_tune_display_v3(tof_tune_v3);
	}
done:
	return err;
}
