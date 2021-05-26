/*
 * Common code for wl routines
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
 * $Id: wlu_common.c 775302 2019-05-27 11:48:20Z $
 */

#include <typedefs.h>

#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include "wlu_common.h"
#include "wlu.h"
#include <bcmendian.h>

#include <wps.h>

#include <bcmwifi_rspec.h>

#if defined(linux)
#ifndef TARGETENV_android
#include <unistd.h>
#endif // endif
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* linux */

#include <802.11ax.h>

#include <miniopt.h>

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif // endif

extern int wl_get(void *wl, int cmd, void *buf, int len);
extern int wl_set(void *wl, int cmd, void *buf, int len);
static int wl_bssiovar_mkbuf(const char *iovar, int bssidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr);
#if !defined(BCM_DHD_UNUSED)
static int wl_interface_create_action_v3(void *wl, cmd_t *cmd, char **argv);
static int wl_interface_create_action_v2(void *wl, cmd_t *cmd, char **argv);
static int wlu_var_getbuf_param_len(void *wl, const char *iovar, void *param, int param_len,
	void **bufptr);
static void wl_dump_ie_buf(vndr_ie_buf_t *ie_getbuf);
#endif /* BCM_DHD_UNUSED */
static int wl_sta_info_all(void *wl, cmd_t *cmd);

static cmd_func_t wl_var_getint;

wl_cmd_list_t cmd_list;
int cmd_pkt_list_num;
bool cmd_batching_mode;

const char *wlu_av0;

/* global wlc index indentifier for RSDB -W/--wlc option */
int g_wlc_idx = -1;

/* IOCTL swapping mode for Big Endian host with Little Endian dongle.  Default to off */
bool g_swap = FALSE;

#ifdef SERDOWNLOAD
extern int debug;
#endif // endif

#if !defined(BCM_DHD_UNUSED)
static char *buf;

/* module initialization */
void
wluc_common_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();
}

static int
wlu_var_getbuf_param_len(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, param_len);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len) {
		memcpy(&buf[len], param, param_len);
		*bufptr = buf;
		return wlu_get(wl, WLC_GET_VAR, &buf[0], len+param_len);
	}
	return (BCME_BADARG);
}

int
wl_get_rateset_args_info(void *wl, int *rs_len, int *rs_ver)
{
	int err = 0;
	wl_wlc_version_t *ver;
	struct wl_rateset_args_v2 wlrs;
	struct wl_rateset_args_v2 *wlrs2;
	void *ptr;

	/* first query wlc version. */
	err = wlu_var_getbuf_sm(wl, "wlc_ver", NULL, 0, &ptr);
	if (err == BCME_OK) {
		ver = ptr;
		/* rateset args query is available from wlc_ver_major >= 9 */
		if ((ver->wlc_ver_major >= 9)) {
			/* Now, query the wl_rateset_args_t version, by giving version=0 and
			 * length as 4 bytes ssizeof(int32).
			 */
			memset(&wlrs, 0, sizeof(wlrs));

			err = wlu_var_getbuf_param_len(wl, "rateset", &wlrs, sizeof(int32),
				(void *)&wlrs2);

			if (err == BCME_OK) {
				*rs_ver = wlrs2->version;

				switch (*rs_ver) {
				case RATESET_ARGS_V2:
					*rs_len = sizeof(struct wl_rateset_args_v2);
					break;
				/* add new length returning here */
				default:
					*rs_len = 0; /* ERROR */
					err = BCME_UNSUPPORTED;
				}
			}
		} else {
			*rs_ver = RATESET_ARGS_V1;
			*rs_len = sizeof(struct wl_rateset_args_v1);
		}
	} else {
		/* for old branches which doesn't even support wlc_ver */
		*rs_ver = RATESET_ARGS_V1;
		*rs_len = sizeof(struct wl_rateset_args_v1);
		err = BCME_OK;
	}
	return err;
}

int
wl_var_void(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(argv);

	if (cmd->set < 0)
		return -1;

	return wlu_var_setbuf(wl, cmd->name, NULL, 0);
}

int
wl_var_get(void *wl, cmd_t *cmd, char **argv)
{
	char *varname;
	char *p;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("get: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if (*argv) {
		printf("get: error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	strcpy(buf, varname);
	p = buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}
	return (wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN));
}

int
wl_var_getandprintstr(void *wl, cmd_t *cmd, char **argv)
{
	int err;

	if ((err = wl_var_get(wl, cmd, argv)))
		return (err);

	printf("%s\n", buf);
	return (0);
}

int
wl_sta_info_print(void *wl, void *buf)
{
	sta_info_v4_t *sta;
	sta_info_v5_t *sta_v5;
	sta_info_v7_t *sta_v7 = NULL;
	sta_info_v8_t *sta_v8 = NULL;
	int i, err = BCME_OK;
	char buf_chanspec[20];
	uint32 rxdur_total = 0;
	bool have_rxdurtotal = FALSE;
	chanspec_t chanspec;
	bool have_chanspec = FALSE;
	wl_rateset_args_u_t *rateset_adv;
	bool have_rateset_adv = FALSE;
	uint32 wpauth = 0;
	uint8 algo = 0;
	int8 srssi = 0;
	uint8 rrm_cap[DOT11_RRM_CAP_LEN];

	/* display the sta info */
	sta = (sta_info_v4_t *)buf;
	sta->ver = dtoh16(sta->ver);
	sta->len = dtoh16(sta->len);

	/* Report unrecognized version */
	if (sta->ver < WL_STA_VER_4) {
		printf(" ERROR: unsupported driver station info version %d\n", sta->ver);
		return BCME_ERROR;
	}
	else if (sta->ver == WL_STA_VER_4) {
		rxdur_total = dtoh32(sta->rx_dur_total);
		have_rxdurtotal = TRUE;
		if (sta->len >= STRUCT_SIZE_THROUGH(sta, rateset_adv)) {
			chanspec = dtoh16(sta->chanspec);
			wf_chspec_ntoa(chanspec, buf_chanspec);
			have_chanspec = TRUE;

			rateset_adv = (wl_rateset_args_u_t *)&sta->rateset_adv;
			have_rateset_adv = TRUE;
		}
	}
	else if (sta->ver == WL_STA_VER_5) {
		sta_v5 = (sta_info_v5_t *)buf;
		chanspec = dtoh16(sta_v5->chanspec);
		wf_chspec_ntoa(chanspec, buf_chanspec);
		have_chanspec = TRUE;

		rateset_adv = (wl_rateset_args_u_t *)&sta_v5->rateset_adv;
		have_rateset_adv = TRUE;
	}
	else if (sta->ver >= WL_STA_VER_7) {
		sta_v7 = (sta_info_v7_t *)buf;
		sta_v8 = (sta_info_v8_t *)buf;

		rxdur_total = dtoh32(sta_v7->rx_dur_total);
		have_rxdurtotal = TRUE;

		chanspec = dtoh16(sta_v7->chanspec);
		wf_chspec_ntoa(chanspec, buf_chanspec);
		have_chanspec = TRUE;

		if (sta->ver == WL_STA_VER_8) {
			wpauth = dtoh32(sta_v8->wpauth);
			srssi = dtoh32(sta_v8->srssi);
			memcpy(rrm_cap, sta_v8->rrm_capabilities, DOT11_RRM_CAP_LEN);
		}
		else if (sta->ver == WL_STA_VER_7) {
			wpauth = dtoh16(sta_v7->wpauth);
		}
		algo = sta_v7->algo;

		rateset_adv = (wl_rateset_args_u_t *)&sta_v7->rateset_adv;
		have_rateset_adv = TRUE;
		sta_v7->he_flags = dtoh16(sta_v7->he_flags);
	}
	else {
		printf(" ERROR: unknown driver station info version %d\n", sta->ver);
		return BCME_ERROR;
	}

	sta->cap = dtoh16(sta->cap);
	sta->aid = dtoh16(sta->aid);
	sta->flags = dtoh32(sta->flags);
	sta->idle = dtoh32(sta->idle);
	sta->rateset.count = dtoh32(sta->rateset.count);
	sta->in = dtoh32(sta->in);
	sta->listen_interval_inms = dtoh32(sta->listen_interval_inms);
	sta->ht_capabilities = dtoh16(sta->ht_capabilities);
	sta->vht_flags = dtoh16(sta->vht_flags);

	printf("[VER %d] STA %s:\n", sta->ver, wl_ether_etoa(&sta->ea));
	if (have_chanspec) {
		printf("\t chanspec %s (0x%x)\n", buf_chanspec, chanspec);
	}
	printf("\t aid:%d ", WL_STA_AID(sta->aid));
	printf("\n\t rateset ");
	dump_rateset(sta->rateset.rates, sta->rateset.count);
	printf("\n\t idle %d seconds\n", sta->idle);
	printf("\t in network %d seconds\n", sta->in);
	printf("\t state:%s%s%s\n",
	       (sta->flags & WL_STA_AUTHE) ? " AUTHENTICATED" : "",
	       (sta->flags & WL_STA_ASSOC) ? " ASSOCIATED" : "",
	       (sta->flags & WL_STA_AUTHO) ? " AUTHORIZED" : "");

	if (sta->ver >= WL_STA_VER_7) {

		printf("\t connection:%s\n",
				(wpauth > 0x01) ? " SECURED" : "OPEN");

		if (wpauth == 0x00)
			printf("\t auth: %s",  "AUTH-DISABLED");	/* Legacy (i.e., non-WPA) */
		else if (wpauth == 0x1)
			printf("\t auth: %s",  "AUTH-NONE");		/* none (IBSS) */
		else if (wpauth == 0x2)
			printf("\t auth: %s",  "AUTH-UNSPECIFIED");	/* over 802.1x */
		else if (wpauth == 0x4)
			printf("\t auth: %s",  "WPA-PSK");		/* Pre-shared key */
		else if (wpauth == 0x40)
			printf("\t auth: %s",  "WPA-PSK");		/* over 802.1x */
		else if (wpauth == 0x80)
			printf("\t auth: %s",  "WPA2-PSK");		/* Pre-shared key */
		else if (wpauth == 0x84)
			printf("\t auth: %s",  "WPA-PSK + WPA2-PSK");	/* Pre-shared key */
		else if (wpauth == 0x100)
			printf("\t auth: %s",  "BRCM_AUTH_PSK");	/* BRCM specific PSK */
		else if (wpauth == 0x200)
			printf("\t auth: %s",  "BRCM_AUTH_DPT");  /* DPT PSK without group keys */
		else if (wpauth == 0x1000)
			printf("\t auth: %s",  "WPA2_AUTH_MFP");	/* MFP (11w) */
		else if (wpauth == 0x2000)
			printf("\t auth: %s",  "WPA2_AUTH_TPK");	/* TDLS Peer Key */
		else if (wpauth == 0x4000)
			printf("\t auth: %s",  "WPA2_AUTH_FT");		/* Fast Transition */
		else if (wpauth == 0x4080)
			printf("\t auth: %s",  "WPA2-PSK+FT");		/* Fast Transition */
		else if (wpauth == 0x4084)
			printf("\t auth: %s",  "WPA-PSK + WPA2-PSK + FT");  /* Fast Transition */
		else if (wpauth == 0x40000)
			printf("\t auth: %s",  "WPA3-SAE");		/* WPA3-SAE */
		else if (wpauth == 0x40080)
			printf("\t auth: %s",  "WPA2-PSK WPA3-SAE");	/* WPA2-PSK + WPA3-SAE */
		else
			printf("\t auth: %s",  "UNKNOWN AUTH");		/* Unidentified */
		printf("\n\t crypto: %s\n",   bcm_crypto_algo_name(algo));
	}

	printf("\t flags 0x%x:%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
	       sta->flags,
	       (sta->flags & WL_STA_BRCM) ? " BRCM" : "",
	       (sta->flags & WL_STA_WME) ? " WME" : "",
	       (sta->flags & WL_STA_PS) ? " PS" : "",
	       (sta->flags & WL_STA_NONERP) ? " No-ERP" : "",
	       (sta->flags & WL_STA_APSD_BE) ? " APSD_BE" : "",
	       (sta->flags & WL_STA_APSD_BK) ? " APSD_BK" : "",
	       (sta->flags & WL_STA_APSD_VI) ? " APSD_VI" : "",
	       (sta->flags & WL_STA_APSD_VO) ? " APSD_VO" : "",
	       (sta->flags & WL_STA_N_CAP) ? " N_CAP" : "",
	       (sta->flags & WL_STA_VHT_CAP) ? " VHT_CAP" : "",
	       (sta->flags & WL_STA_HE_CAP) ? " HE_CAP" : "",
	       (sta->flags & WL_STA_AMPDU_CAP) ? " AMPDU" : "",
	       (sta->flags & WL_STA_AMSDU_CAP) ? " AMSDU" : "",
	       (sta->flags & WL_STA_MIMO_PS) ? " MIMO-PS" : "",
	       (sta->flags & WL_STA_MIMO_RTS) ? " MIMO-PS-RTS" : "",
	       (sta->flags & WL_STA_RIFS_CAP) ? " RIFS" : "",
	       (sta->flags & WL_STA_WPS) ? " WPS" : "",
	       (sta->flags & WL_STA_GBL_RCLASS) ? " GBL_RCLASS" : "",
	       (sta->flags & WL_STA_DWDS_CAP) ? " DWDS_CAP": "",
	       (sta->flags & WL_STA_DWDS) ? " DWDS_ACTIVE" : "",
	       (sta->flags & WL_STA_WDS) ? " WDS" : "",
	       (sta->flags & WL_STA_WDS_LINKUP) ? " WDS_LINKUP" : "");

	printf("\t HT caps 0x%x:%s%s%s%s%s%s%s%s%s\n",
		sta->ht_capabilities,
	       (sta->ht_capabilities & WL_STA_CAP_LDPC_CODING) ? " LDPC" : "",
	       (sta->ht_capabilities & WL_STA_CAP_40MHZ) ? " 40MHz" : " ",
	       (sta->ht_capabilities & WL_STA_CAP_GF) ? " GF" : "",
	       (sta->ht_capabilities & WL_STA_CAP_SHORT_GI_20) ? " SGI20" : "",
	       (sta->ht_capabilities & WL_STA_CAP_SHORT_GI_40) ? " SGI40" : "",
	       (sta->ht_capabilities & WL_STA_CAP_TX_STBC) ? " STBC-Tx" : "",
	       (sta->ht_capabilities & WL_STA_CAP_RX_STBC_MASK) ? " STBC-Rx" : "",
	       (sta->ht_capabilities & WL_STA_CAP_DELAYED_BA) ? " D-BlockAck" : "",
	       (sta->ht_capabilities & WL_STA_CAP_40MHZ_INTOLERANT) ? " 40-Intl" : "");

	if (sta->flags & WL_STA_VHT_CAP) {
		printf("\t VHT caps 0x%x:%s%s%s%s%s%s%s%s%s%s%s\n",
			sta->vht_flags,
			(sta->vht_flags & WL_STA_VHT_LDPCCAP) ? " LDPC" : "",
			(sta->vht_flags & WL_STA_SGI80) ? " SGI80" : "",
			(sta->vht_flags & WL_STA_SGI160) ? " SGI160" : "",
			(sta->vht_flags & WL_STA_VHT_TX_STBCCAP) ? " STBC-Tx" : "",
			(sta->vht_flags & WL_STA_VHT_RX_STBCCAP) ? " STBC-Rx" : "",
			(sta->vht_flags & WL_STA_SU_BEAMFORMER) ? " SU-BFR" : "",
			(sta->vht_flags & WL_STA_SU_BEAMFORMEE) ? " SU-BFE" : "",
			(sta->vht_flags & WL_STA_MU_BEAMFORMER) ? " MU-BFR" : "",
			(sta->vht_flags & WL_STA_MU_BEAMFORMEE) ? " MU-BFE" : "",
			(sta->vht_flags & WL_STA_VHT_TXOP_PS) ? " TXOPPS" : "",
			(sta->vht_flags & WL_STA_HTC_VHT_CAP) ? " VHT-HTC" : "");
	}

	if (sta_v7 && (sta_v7->flags & WL_STA_HE_CAP)) {
		printf("\t HE caps 0x%x:%s%s%s%s%s%s%s\n",
			sta_v7->he_flags,
			(sta_v7->he_flags & WL_STA_HE_LDPCCAP) ? " LDPC" : "",
			(sta_v7->he_flags & WL_STA_HE_TX_STBCCAP) ? " STBC-Tx" : "",
			(sta_v7->he_flags & WL_STA_HE_RX_STBCCAP) ? " STBC-Rx" : "",
			(sta_v7->he_flags & WL_STA_HE_HTC_CAP) ? " HE-HTC" : "",
			(sta_v7->he_flags & WL_STA_HE_SU_BEAMFORMER) ? " SU-BFR" : "",
			(sta_v7->he_flags & WL_STA_HE_SU_MU_BEAMFORMEE) ? " SU&MU-BFE" : "",
			(sta_v7->he_flags & WL_STA_HE_MU_BEAMFORMER) ? " MU-BFR" : "");
	}

	if (sta->flags & WL_STA_SCBSTATS)
	{
		printf("\t tx total pkts: %d\n", dtoh32(sta->tx_tot_pkts));
		printf("\t tx total bytes: %llu\n", dtoh64(sta->tx_tot_bytes));
		printf("\t tx ucast pkts: %d\n", dtoh32(sta->tx_pkts));
		printf("\t tx ucast bytes: %llu\n", dtoh64(sta->tx_ucast_bytes));
		printf("\t tx mcast/bcast pkts: %d\n", dtoh32(sta->tx_mcast_pkts));
		printf("\t tx mcast/bcast bytes: %llu\n", dtoh64(sta->tx_mcast_bytes));
		printf("\t tx failures: %d\n", dtoh32(sta->tx_failures));
		printf("\t rx data pkts: %d\n", dtoh32(sta->rx_tot_pkts));
		printf("\t rx data bytes: %llu\n", dtoh64(sta->rx_tot_bytes));
		if (have_rxdurtotal) {
			printf("\t rx data dur: %u\n", rxdur_total);
		}
		printf("\t rx ucast pkts: %d\n", dtoh32(sta->rx_ucast_pkts));
		printf("\t rx ucast bytes: %llu\n", dtoh64(sta->rx_ucast_bytes));
		printf("\t rx mcast/bcast pkts: %d\n", dtoh32(sta->rx_mcast_pkts));
		printf("\t rx mcast/bcast bytes: %llu\n", dtoh64(sta->rx_mcast_bytes));
		printf("\t rate of last tx pkt: %d kbps - %d kbps\n",
			dtoh32(sta->tx_rate), dtoh32(sta->tx_rate_fallback));
		printf("\t rate of last rx pkt: %d kbps\n", dtoh32(sta->rx_rate));
		printf("\t rx decrypt succeeds: %d\n", dtoh32(sta->rx_decrypt_succeeds));
		printf("\t rx decrypt failures: %d\n", dtoh32(sta->rx_decrypt_failures));
		printf("\t tx data pkts retried: %d\n", dtoh32(sta->tx_pkts_retried));

		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (i == WL_ANT_IDX_1)
				printf("\t per antenna rssi of last rx data frame:");
			printf(" %d", dtoh32(sta->rx_lastpkt_rssi[i]));
			if (i == WL_RSSI_ANT_MAX-1)
				printf("\n");
		}
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (i == WL_ANT_IDX_1)
				printf("\t per antenna average rssi of rx data frames:");
			printf(" %d", dtoh32(sta->rssi[i]));
			if (i == WL_RSSI_ANT_MAX-1)
				printf("\n");
		}
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (i == WL_ANT_IDX_1)
				printf("\t per antenna noise floor:");
			printf(" %d", dtoh32(sta->nf[i]));
			if (i == WL_RSSI_ANT_MAX-1)
				printf("\n");
		}

		printf("\t tx total pkts sent: %d\n", dtoh32(sta->tx_pkts_total));
		printf("\t tx pkts retries: %d\n", dtoh32(sta->tx_pkts_retries));
		printf("\t tx pkts retry exhausted: %d\n", dtoh32(sta->tx_pkts_retry_exhausted));
		printf("\t tx FW total pkts sent: %d\n", dtoh32(sta->tx_pkts_fw_total));
		printf("\t tx FW pkts retries: %d\n", dtoh32(sta->tx_pkts_fw_retries));
		printf("\t tx FW pkts retry exhausted: %d\n",
			dtoh32(sta->tx_pkts_fw_retry_exhausted));
		printf("\t rx total pkts retried: %d\n", dtoh32(sta->rx_pkts_retried));
	}
	/* Driver didn't return extended station info */
	if (sta->len < sizeof(sta_info_v5_t)) {
		return 0;
	}

	if (have_rateset_adv) {
		int rslen = 0, rsver = 0;
		uint8 *rs_mcs = NULL;
		uint16 *rs_vht_mcs = NULL;
		uint16 *rs_he_mcs = NULL;
		if (sta->ver >= WL_STA_VER_7) {
			rs_mcs = rateset_adv->rsv2.mcs;
			rs_vht_mcs = rateset_adv->rsv2.vht_mcs;
			rs_he_mcs = rateset_adv->rsv2.he_mcs;
		} else {
			if ((err = wl_get_rateset_args_info(wl, &rslen, &rsver)) < 0)
				return (err);
			wl_rateset_get_fields(rateset_adv, rsver, NULL, NULL, &rs_mcs,
				&rs_vht_mcs, NULL);
		}
		wl_print_mcsset((char *)rs_mcs);
		wl_print_vhtmcsset((uint16 *)rs_vht_mcs);
		if (rs_he_mcs != NULL && rs_he_mcs[0] != 0xffff) {
			printf("HE SET  :\n");
			wl_print_hemcsnss((uint16 *)rs_he_mcs);
		}
	}

	if (sta->ver >= WL_STA_VER_7)
	{
		printf("smoothed rssi: %d\n", srssi);
		printf("tx nrate\n");
		wl_nrate_print(dtoh32(sta_v7->tx_rspec), WLC_IOCTL_VERSION);
		printf("rx nrate\n");
		wl_nrate_print(dtoh32(sta_v7->rx_rspec), WLC_IOCTL_VERSION);
		printf("wnm\n");
		wl_wnm_print(sta_v7->wnm_cap);
		if (sta_v7->len >= STRUCT_SIZE_THROUGH(sta_v7, sta_vendor_oui)) {
			for (i = 0; i < sta_v7->sta_vendor_oui.count && i < WLC_MAX_ASSOC_OUI_NUM;
					i ++) {
				printf("VENDOR OUI VALUE[%d] %02X:%02X:%02X \n", i,
						sta_v7->sta_vendor_oui.oui[i][0],
						sta_v7->sta_vendor_oui.oui[i][1],
						sta_v7->sta_vendor_oui.oui[i][2]);
			}
		}
		if (sta_v7->len >= STRUCT_SIZE_THROUGH(sta_v7, link_bw)) {
			switch (sta_v7->link_bw) {
				case 1:
					printf("link bandwidth = 20 MHZ \n");
					break;
				case 2:
					printf("link bandwidth = 40 MHZ \n");
					break;
				case 3:
					printf("link bandwidth = 80 MHZ \n");
					break;
				case 4:
					printf("link bandwidth = 160 MHZ \n");
					break;
			}
		}
		wl_rrm_print(rrm_cap);
	}

	return (0);
}

int
wl_sta_info(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea = {{255, 255, 255, 255, 255, 255}};
	char *param;
	int buflen, err;
	bool sta_all = FALSE;

	strcpy(buf, *argv);

	/* convert the ea string into an ea struct */
	if (!*++argv) {
		printf(" ERROR: missing arguments\n");
		return BCME_USAGE_ERROR;
	} else if (strcmp(*argv, "all") == 0) {
		sta_all = TRUE;
	} else if (strcmp(*argv, "wds_all") == 0) {
		err = wl_wds_info_all(wl, cmd);
		return err;
	} else if (!wl_ether_atoe(*argv, &ea)) {
		printf(" ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	if (ETHER_ISBCAST(&ea) || sta_all) {
		err = wl_sta_info_all(wl, cmd);
		return err;
	}

	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&ea, ETHER_ADDR_LEN);

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

	err = wl_sta_info_print(wl, buf);
	return err;
}

static int
wl_sta_info_all(void *wl, cmd_t *cmd)
{
	struct ether_addr ea = {{255, 255, 255, 255, 255, 255}};
	char *param;
	int buflen, err;
	uint i;
	uint16 len, data_offset;

	memset(buf, 0, WLC_IOCTL_MAXLEN);

	strcpy(buf, cmd->name);
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&ea, ETHER_ADDR_LEN);

	err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN);

	if (err != BCME_IOCTL_ERROR) {
		/* Firmware supports sta_info all */
		sta_info_all_t *sta_info_all = (sta_info_all_t *)buf;

		sta_info_all->count = dtoh16(sta_info_all->count);
		sta_info_all->length = dtoh16(sta_info_all->length);

		if (dtoh16(sta_info_all->version) != WL_STA_INFO_ALL_VER_1) {
			err = BCME_VERSION;
			printf("err = %d version = %d\n", err, sta_info_all->version);
			return err;
		} else {
			uint8 *sta_info;
			uint32 sta_info_len = 0;

			if (sta_info_all->sta_info_ver == WL_STA_VER_7) {
				sta_info_len = sizeof(sta_info_v7_t);
			} else if (sta_info_all->sta_info_ver == WL_STA_VER_8) {
				sta_info_len = sizeof(sta_info_v8_t);
			} else {
				err = BCME_VERSION;
				printf("err = %d sta ver = %d\n", err, sta_info_all->sta_info_ver);
				return err;
			}

			data_offset = dtoh16(sta_info_all->data_offset);
			sta_info = (uint8 *)sta_info_all + data_offset;
			len = sizeof(*sta_info_all) + (sta_info_all->count * sta_info_len);

			if (sta_info_all->length != len) {
				err = BCME_BADLEN;
				printf("err = %d sta_info_length = %d  len - %d\n",
					err, sta_info_all->length, len);
				return err;
			}

		       for (i = 0; i < sta_info_all->count; i++) {
			       if ((err = wl_sta_info_print(wl, (void *)sta_info)) < 0) {
				       return err;
			       }
			       sta_info += sta_info_len;
		       }
	       }
	} else {
		/* If sta_info all is not supported then
		 * get assoclist & wdslist and send sta_info for each client
		 */
		struct maclist* maclist = NULL;

		maclist = malloc(WLC_IOCTL_MEDLEN);
		if (!maclist) {
			printf("unable to allocate memory\n");
			return BCME_NOMEM;
		}
		memset(maclist, 0, WLC_IOCTL_MEDLEN);
		maclist->count = htod32((WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN);

		/* Get assoclist */
		if ((err = wlu_get(wl, WLC_GET_ASSOCLIST, maclist, WLC_IOCTL_MEDLEN)) < 0) {
			printf("Cannot get assoclist\n");
			free(maclist);
			return err;
		}

		maclist->count = dtoh32(maclist->count);
		for (i = 0; i < maclist->count; i++) {
			strcpy(buf, cmd->name);
			memcpy(param, (char*)&maclist->ea[i], ETHER_ADDR_LEN);
			if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
				free(maclist);
				return err;
			}

			wl_sta_info_print(wl, buf);
		}
		free(maclist);

		/* Get sta_info of wds clients */
		err = wl_wds_info_all(wl, cmd);
	}
	return err;
}

/*
 * If a host IP address is given, add it to the host-cache, e.g. "wl arp_hostip 192.168.1.1".
 * If no address is given, dump all the addresses.
 */
int
wl_hostip(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ipv4_addr ipa_set, *ipa_get, null_ipa;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;
		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		memset(null_ipa.addr, 0, IPV4_ADDR_LEN);

		for (ipa_get = (struct ipv4_addr *)ptr;
		     memcmp(null_ipa.addr, ipa_get->addr, IPV4_ADDR_LEN) != 0;
		     ipa_get++)
			printf("%s\n", wl_iptoa(ipa_get));

		printf("Total %d host addresses\n", (int)(ipa_get - (struct ipv4_addr *)ptr));
	} else {
		/* Add */
		if (!wl_atoip(*argv, &ipa_set))
			return BCME_USAGE_ERROR;
		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, sizeof(IPV4_ADDR_LEN));
	}

	return ret;
}

/*
 * If a host IP address is given, add it to the host-cache,
 * e.g. "wl nd_hostip fe00:0:0:0:0:290:1fc0:18c0 ".
 * If no address is given, dump all the addresses.
 */
int
wl_hostipv6(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	struct ipv6_addr ipa_set, *ipa_get, null_ipa;
	uint16 *ip_addr;
	if (!*++argv) {
		/* Get */
		void *ptr = NULL;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		ip_addr = (uint16*)ptr;
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (ipa_get = (struct ipv6_addr *)ptr;
			 memcmp(null_ipa.addr, ipa_get->addr, IPV6_ADDR_LEN) != 0;
			 ipa_get++) {
			/* Print ipv6 Addr */
			for (i = 0; i < 8; i++) {
				printf("%x", ntoh16(ip_addr[i]));
				if (i < 7)
					printf(":");
			}
			printf("\r\n");

			ip_addr += 8;
		}
	} else {
		/* Add */
		if (!wl_atoipv6(*argv, &ipa_set))
			return BCME_USAGE_ERROR;

		/* we add one ip-addr at a time */
		return wlu_var_setbuf(wl, cmd->name, &ipa_set, IPV6_ADDR_LEN);
	}
	return ret;
}

static dbg_msg_t toe_cmpnt[] = {
	{TOE_TX_CSUM_OL,	"tx_csum_ol"},
	{TOE_RX_CSUM_OL,	"rx_csum_ol"},
	{0,			NULL}
};

static dbg_msg_t arpoe_cmpnt[] = {
	{ARP_OL_AGENT,		"agent"},
	{ARP_OL_SNOOP,		"snoop"},
	{ARP_OL_HOST_AUTO_REPLY, "host_auto_reply"},
	{ARP_OL_PEER_AUTO_REPLY, "peer_auto_reply"},
	{0,			NULL}
};

/*
 *  Tcpip Offload Component-wise get/set control.
 */
int
wl_offload_cmpnt(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val, last_val = 0, cmpnt_add = 0, cmpnt_del = 0;
	char *endptr;
	dbg_msg_t *dbg_msg = NULL;
	void *ptr = NULL;
	int cmpnt;

	if (strcmp(cmd->name, "toe_ol") == 0)
		dbg_msg = toe_cmpnt;
	else if (strcmp(cmd->name, "arp_ol") == 0)
		dbg_msg = arpoe_cmpnt;
	else {
		printf("Not a valid command\n");
		return BCME_BADARG;
	}

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return (ret);
	cmpnt = dtoh32(*(int *)ptr);

	if (!*++argv) {
		printf("0x%x ", cmpnt);
		for (i = 0; (val = dbg_msg[i].value); i++) {
			if ((cmpnt & val) && (val != last_val))
				printf(" %s", dbg_msg[i].string);
			last_val = val;
		}
		printf("\n");
		return (0);
	}

	while (*argv) {
		char *s = *argv;
		if (*s == '+' || *s == '-')
			s++;
		else
			cmpnt_del = ~0;	/* make the whole list absolute */
		val = strtoul(s, &endptr, 0);
		/* not a plain integer if not all the string was parsed by strtoul */
		if (*endptr != '\0') {
			for (i = 0; (val = dbg_msg[i].value); i++)
				if (stricmp(dbg_msg[i].string, s) == 0)
					break;
			if (!val)
				goto usage;
		}
		if (**argv == '-')
			cmpnt_del |= val;
		else
			cmpnt_add |= val;
		++argv;
	}

	cmpnt &= ~cmpnt_del;
	cmpnt |= cmpnt_add;

	cmpnt = htod32(cmpnt);
	return (wlu_var_setbuf(wl, cmd->name, &cmpnt, sizeof(int)));

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");

	for (i = 0; (val = dbg_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, dbg_msg[i].string);
		else
			fprintf(stderr, ", %s", dbg_msg[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");

	return 0;
}

/* mkeep-alive : Send a periodic keep-alive packet or null-data at the specificed interval. */
/* wowl_keepalive : Send a periodic keep alive packet the specificed interval in wowl mode. */
int
wl_mkeep_alive(void *wl, cmd_t *cmd, char **argv)
{
	const char 				*str;
	wl_mkeep_alive_pkt_t	mkeep_alive_pkt;
	wl_mkeep_alive_pkt_t	*mkeep_alive_pktp;
	int						buf_len;
	int						str_len;
	int						len_bytes;
	int						i;
	int 					rc;
	void					*ptr = NULL;
	bool					immed_flag = FALSE;

	memset(&mkeep_alive_pkt, 0, sizeof(wl_mkeep_alive_pkt_t));

	str = *argv;  /* mkeep_alive or wowl_keepalive */
	if (*++argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	else {
		/* read the packet index */
		int mkeep_alive_id = htod32(strtoul(*argv, NULL, 0));

		if (*++argv == NULL) {
			/*
			** Get current keep-alive status.
			*/
			if ((rc = wlu_var_getbuf(wl, cmd->name, &mkeep_alive_id,
				sizeof(int), &ptr)) < 0)
				return rc;

			mkeep_alive_pktp = (wl_mkeep_alive_pkt_t *) ptr;

			printf("Id            :%d\n"
				   "Period (msec) :%d\n"
				   "Length        :%d\n"
				   "Packet        :0x",
				   mkeep_alive_pktp->keep_alive_id,
				   dtoh32(mkeep_alive_pktp->period_msec),
				   dtoh16(mkeep_alive_pktp->len_bytes));

			for (i = 0; i < mkeep_alive_pktp->len_bytes; i++)
				printf("%02x", mkeep_alive_pktp->data[i]);

			printf("\n");
			return rc;
		}

		str_len = strlen(str);
		strncpy(buf, str, str_len);
		buf[ str_len ] = '\0';
		mkeep_alive_pktp = (wl_mkeep_alive_pkt_t *) (buf + str_len + 1);
		if (strcmp(*argv, "immediate") == 0) {
			immed_flag = TRUE;
			argv++;
		}
		mkeep_alive_pkt.period_msec = strtoul(*argv, NULL, 0);
		if (mkeep_alive_pkt.period_msec & WL_MKEEP_ALIVE_IMMEDIATE) {
			fprintf(stderr, "Period %d too large\n", mkeep_alive_pkt.period_msec);
			return -1;
		}
		if (immed_flag && mkeep_alive_pkt.period_msec) {
			mkeep_alive_pkt.period_msec |= WL_MKEEP_ALIVE_IMMEDIATE;
		}
		mkeep_alive_pkt.period_msec = htod32(mkeep_alive_pkt.period_msec);
		buf_len = str_len + 1;
		mkeep_alive_pkt.version = htod16(WL_MKEEP_ALIVE_VERSION);
		mkeep_alive_pkt.length = htod16(WL_MKEEP_ALIVE_FIXED_LEN);
		mkeep_alive_pkt.keep_alive_id = mkeep_alive_id;

		len_bytes = 0;

		buf_len += WL_MKEEP_ALIVE_FIXED_LEN;
		if (mkeep_alive_pkt.period_msec != 0) {
			if (NULL != *++argv) {
				len_bytes = wl_pattern_atoh(*argv, (char *) mkeep_alive_pktp->data);
				buf_len += len_bytes;
			}
		}
		mkeep_alive_pkt.len_bytes = htod16(len_bytes);

		/* Keep-alive attributes are set in local	variable (mkeep_alive_pkt), and
		 * then memcpy'ed into buffer (mkeep_alive_pktp) since there is no
		 * guarantee that the buffer is properly aligned.
		 */
		memcpy((char *)mkeep_alive_pktp, &mkeep_alive_pkt, WL_MKEEP_ALIVE_FIXED_LEN);

		rc = wlu_set(wl, WLC_SET_VAR, buf, buf_len);
	}

	return (rc);
}

int
wlu_srwrite(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return 0;
#else
	char *arg;
	char *endptr;
	FILE *fp = NULL;
	int ret = 0, erase, srcrc;
	uint i, len;
	srom_rw_t *srt = (srom_rw_t *)buf;
	char *tempbuf = NULL;

	erase = !strcmp(*argv, "srclear");
	srcrc = !strcmp(*argv, "srcrc");

	/* We need at least one arg */
	if (!*++argv)
		return BCME_USAGE_ERROR;

	arg = *argv++;

	if (erase) {
		if (*argv)
			return BCME_USAGE_ERROR;
		len = strtoul(arg, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "error parsing value \"%s\" as an integer for byte count\n",
			        arg);
			return BCME_USAGE_ERROR;
		}
		srt->byteoff = 0x55aa;
	} else if (!*argv) {  /* srwrite or srcrc */
		/* Only one arg, it better be a file name */
		if (!(fp = fopen(arg, "rb"))) {
			fprintf(stderr, "%s: No such file or directory\n", arg);
			return BCME_BADARG;
		}

		len = fread(srt->buf, 1, SROM_MAX+1, fp);
		if ((ret = ferror(fp))) {
			printf("\nerror %d reading %s\n", ret, arg);
			ret = BCME_ERROR;
			goto out;
		}

		if (!feof(fp)) {
			printf("\nFile %s is too large\n", arg);
			ret = BCME_ERROR;
			goto out;
		}

		if (len == SROM4_WORDS * 2) {
			if ((srt->buf[SROM4_SIGN] != SROM4_SIGNATURE) &&
			    (srt->buf[SROM8_SIGN] != SROM4_SIGNATURE)) {
				printf("\nFile %s is %d bytes but lacks a REV4/ signature\n",
				       arg, SROM4_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		} else if (len == SROM11_WORDS * 2) {
			if (srt->buf[SROM11_SIGN] != SROM11_SIGNATURE) {
				printf("\nFile %s is %d bytes but lacks a REV11/ signature\n",
				       arg, SROM11_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		} else if (len == SROM12_WORDS * 2) {
			if ((srt->buf[SROM11_SIGN] != SROM12_SIGNATURE) &&
			    (srt->buf[SROM16_SIGN] != SROM16_SIGNATURE)) {
				printf("\nFile %s is %d bytes but lacks a REV12/REV16 signature\n",
				       arg, SROM12_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		} else if (len == SROM13_WORDS * 2) {
			if (srt->buf[SROM11_SIGN] != SROM13_SIGNATURE) {
				printf("\nFile %s is %d bytes but lacks a REV13/ signature\n",
				       arg, SROM13_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		} else if (len == SROM15_WORDS * 2) {
			/* SROM15, SROM16 and SROM18 have the same size */
			STATIC_ASSERT(SROM15_WORDS == SROM16_WORDS);
			STATIC_ASSERT(SROM15_WORDS == SROM18_WORDS);

			if ((srt->buf[SROM11_SIGN] != SROM15_SIGNATURE) &&
				(srt->buf[SROM16_SIGN] != SROM16_SIGNATURE) &&
				(srt->buf[SROM18_SIGN] != SROM18_SIGNATURE)) {
				printf("\nFile %s is %d bytes but lacks a REV15/16/18 signature\n",
				       arg, SROM15_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		} else if (len == SROM17_WORDS * 2) {
			if (srt->buf[SROM17_SIGN] != SROM17_SIGNATURE) {
				printf("\nFile %s is %d bytes but lacks a REV17 signature\n",
				       arg, SROM17_WORDS * 2);
				ret = BCME_ERROR;
				goto out;
			}
		}
		else if ((len != SROM_WORDS * 2) && (len != SROM10_WORDS * 2) &&
			(len != SROM_MAX)) {
			printf("\nFile %s is %d bytes, not %d or %d or %d or %d bytes\n", arg, len,
				SROM_WORDS * 2, SROM4_WORDS * 2, SROM10_WORDS, SROM_MAX);
			ret = BCME_ERROR;
			goto out;
		}

		srt->byteoff = 0;
	} else {
		if (srcrc) {
			printf("srcrc only takes one arg\n");
			ret = BCME_USAGE_ERROR;
			goto out;
		}

		/* More than 1 arg, first is offset, rest are data. */
		srt->byteoff = strtoul(arg, &endptr, 0);
		if (*endptr != '\0')
			goto nout;

		i = 0;
		while ((arg = *argv++) != NULL) {
			srt->buf[i++] = (uint16)strtoul(arg, &endptr, 0);
			if (*endptr != '\0') {
nout:
				printf("\n%s is not an integer\n", arg);
				ret = BCME_USAGE_ERROR;
				goto out;
			}
		}

		if (srt->byteoff & 1) {
			printf("Byte offset (%d) is odd or negative\n", srt->byteoff);
			ret = BCME_BADARG;
			goto out;
		}

		len = 2 * i;
		if ((srt->byteoff + len) > SROM_MAX) {
			printf("Data extends past %d bytes\n", SROM_MAX);
			ret = BCME_BUFTOOLONG;
			goto out;
		}
	}
	srt->nbytes = len;

	if (srcrc) {
		srt->byteoff = 0x55ab;	/* Hack for srcrc */
		if ((ret = wlu_get(wl, cmd->get, buf, len + 8)) == 0)
			printf("0x%x\n", (uint8)buf[0]);
	} else {
		tempbuf = malloc(SROM_MAX);
		if (tempbuf == NULL) {
			ret = BCME_NOMEM;
			goto out;
		}
		if ((arg != NULL) && (len > MAX_IOCTL_TXCHUNK_SIZE) && (srt->byteoff != 0x55aa)) {
			if (!(fp = fopen(arg, "rb"))) {
				fprintf(stderr, "%s: No such file or directory\n", arg);
				ret = BCME_BADARG;
				goto out;
			}

			len = fread(tempbuf, 1, SROM_MAX + 1, fp);

			memcpy(srt->buf, tempbuf, MAX_IOCTL_TXCHUNK_SIZE);
			srt->byteoff = htod32(0);
			srt->nbytes = htod32(MAX_IOCTL_TXCHUNK_SIZE);
			ret = wlu_set(wl, cmd->set, buf, MAX_IOCTL_TXCHUNK_SIZE + 8);
			memcpy(srt->buf, tempbuf + MAX_IOCTL_TXCHUNK_SIZE,
				len - MAX_IOCTL_TXCHUNK_SIZE);
			srt->byteoff = htod32(MAX_IOCTL_TXCHUNK_SIZE);
			srt->nbytes = htod32(len - MAX_IOCTL_TXCHUNK_SIZE);
			ret = wlu_set(wl, cmd->set, buf, len - MAX_IOCTL_TXCHUNK_SIZE + 8);
		}
		else {
			ret = wlu_set(wl, cmd->set, buf, len + 8);
		}
	}

out:
	fflush(stdout);
	if (fp)
		fclose(fp);
	if (tempbuf)
		free(tempbuf);
	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

/*
 * wlu_reg2args is a generic function that is used for setting/getting
 * WL_IOVAR variables that require address for read, and
 * address + data for write.
 */
int
wlu_reg2args(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	uint32 len;
	void *ptr = NULL;
	char *endptr;
	int ret = 0;

	if (argv[1]) {
		len = sizeof(int_val);
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		memcpy(var, (char *)&int_val, sizeof(int_val));
	}
	else
		return BCME_USAGE_ERROR;

	if (argv[2]) {
		get = FALSE;
		int_val = htod32(strtoul(argv[2], &endptr, 0));
		memcpy(&var[len], (char *)&int_val, sizeof(int_val));
		len += sizeof(int_val);
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
wl_interface_create_action_v3(void *wl, cmd_t *cmd, char **argv)
{
	wl_interface_create_v3_t wlif;
	wl_interface_info_v2_t *pwlif_info;
	int count, val;
	int err;
	char opt, *p, *valstr, *endptr = NULL;
	/* FW uses wl_interface_create_v3_t version */

	memset(&wlif, 0, sizeof(wlif));
	wlif.ver = WL_INTERFACE_CREATE_VER_3;

	argv++;
	count = ARGCNT(argv);

	/*
	 * We should have atleast one argument for the create command,
	 * whether to start it as AP or STA.
	 */
	if (count < 1)
		return BCME_USAGE_ERROR;
	/*
	 * First Argument:
	 * Find the interface that user need to create and update the flag/iftype
	 * flags field is still used along with iftype inorder to support the old version of the
	 * FW work with the latest app changes.
	 */
	if (stricmp(argv[0], "sta") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_STA;
		wlif.flags |= WL_INTERFACE_CREATE_STA;
	} else if (stricmp(argv[0], "ap") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_AP;
		wlif.flags |= WL_INTERFACE_CREATE_AP;
	} else if (stricmp(argv[0], "p2p_go") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_P2P_GO;
	} else if (stricmp(argv[0], "p2p_gc") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_P2P_GC;
	} else if (stricmp(argv[0], "p2p_dev") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_P2P_DISC;
	}
	  else {
		return BCME_USAGE_ERROR;
	}

	argv++;

	while ((p = *argv) != NULL) {
		argv++;
		opt = '\0';
		valstr = NULL;

		if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr,
						"%s: only single char options, error on "
						"param \"%s\"\n", __FUNCTION__, p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
						"%s: missing value parameter after \"%s\"\n",
						__FUNCTION__, p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			err = BCME_USAGE_ERROR;
			goto exit;
		}

		/* The mac address is optional, if its passed and valid use it. */
		if (opt == 'm') {
			if (wl_ether_atoe(valstr, &wlif.mac_addr)) {
				wlif.flags |= WL_INTERFACE_MAC_USE;
			}
		}
		/* The firmware index is optional, if its passed and valid use it. */
		if (opt == 'f') {
			/* parse valstr as int */
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				wlif.flags |= WL_INTERFACE_IF_INDEX_USE;
				wlif.if_index = val;
			} else {
				fprintf(stderr,
						"wl_interface_create_action: could not "
						"parse \"%s\" as an int", valstr);
				err = BCME_BADARG;
				goto exit;
			}
		}

		/* The wlc_index is optional, if its passed and valid use it. */
		if (opt == 'c') {
			/* parse valstr as int */
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				wlif.flags |= WL_INTERFACE_WLC_INDEX_USE;
				wlif.wlc_index = val;
			} else {
				fprintf(stderr,
					"wl_interface_create_action: could not parse \"%s\" "
					"as an int", valstr);
				err = BCME_BADARG;
				goto exit;
			}
		}

		/* The BSSID is optional, if its passed and valid use it. */
		if (opt == 'b') {
			if (wl_ether_atoe(valstr, &wlif.bssid)) {
				wlif.flags |= WL_INTERFACE_BSSID_INDEX_USE;
			}
		}
	}

	err = wlu_var_getbuf(wl, cmd->name, &wlif, sizeof(wlif), (void *)&pwlif_info);
	if (err < 0) {
		printf("%s(): wlu_var_getbuf failed %d \r\n", __FUNCTION__, err);
	} else {
		printf("ifname: %s bsscfgidx: %d mac_addr %s\r\n",
				pwlif_info->ifname, pwlif_info->bsscfgidx,
				wl_ether_etoa(&pwlif_info->mac_addr));
		printf("if_index allocated: %d \r\n", pwlif_info->if_index);
	}

exit:
	return err;
}

static int
wl_interface_create_action_v2(void *wl, cmd_t *cmd, char **argv)
{
	struct wl_interface_create_v2 wlif;
	struct wl_interface_info_v1 *pwlif_info;
	int count, val;
	int err;
	char opt, *p, *valstr, *endptr = NULL;

	memset(&wlif, 0, sizeof(wlif));
	wlif.ver = WL_INTERFACE_CREATE_VER_2;

	argv++;
	count = ARGCNT(argv);

	/*
	 * We should have atleast one argument for the create command,
	 * whether to start it as AP or STA.
	 */
	if (count < 1)
		return BCME_USAGE_ERROR;
	/*
	 * First Argument:
	 * Find the interface that user need to create and update the flag/iftype
	 * flags field is still used along with iftype inorder to support the old version of the
	 * FW work with the latest app changes.
	 */
	if (stricmp(argv[0], "sta") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_STA;
		wlif.flags |= WL_INTERFACE_CREATE_STA;
	} else if (stricmp(argv[0], "ap") == 0) {
		wlif.iftype = WL_INTERFACE_TYPE_AP;
		wlif.flags |= WL_INTERFACE_CREATE_AP;
	} else {
		return BCME_USAGE_ERROR;
	}

	argv++;

	while ((p = *argv) != NULL) {
		argv++;
		opt = '\0';
		valstr = NULL;

		if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr,
				"%s: only single char options, error on param \"%s\"\n",
				__FUNCTION__, p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
				"%s: missing value parameter after \"%s\"\n",
				__FUNCTION__, p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			err = BCME_USAGE_ERROR;
			goto exit;
		}

		/*
		 * The mac address is optional, if its passed and valid use it.
		 * NOTE: Additional flag check for MAC_USE is done in order to avoid overwriting
		 * already set BSSID through -b option.
		 */
		if ((opt == 'm') && !(wlif.flags & WL_INTERFACE_MAC_USE)) {
			if (wl_ether_atoe(valstr, &wlif.mac_addr)) {
				wlif.flags |= WL_INTERFACE_MAC_USE;
			}
		}

		/* The wlc_index is optional, if its passed and valid use it. */
		if (opt == 'c') {
			/* parse valstr as int */
			val = (int)strtol(valstr, &endptr, 0);
			if (*endptr == '\0') {
				wlif.flags |= WL_INTERFACE_WLC_INDEX_USE;
				wlif.wlc_index = val;
			} else {
				fprintf(stderr,
				"wl_interface_create_action: could not parse \"%s\" as an int",
				valstr);
				err = BCME_BADARG;
				goto exit;
			}
		}
	}

	err = wlu_var_getbuf(wl, cmd->name, &wlif, sizeof(wlif), (void *)&pwlif_info);
	if (err < 0) {
		printf("%s(): wlu_var_getbuf failed %d \r\n", __FUNCTION__, err);
	} else {
		printf("ifname: %s bsscfgidx: %d mac_addr %s\r\n",
			pwlif_info->ifname, pwlif_info->bsscfgidx,
			wl_ether_etoa(&pwlif_info->mac_addr));
	}

exit:
	return err;
}

int
wl_interface_create_action(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	struct wl_interface_create_v2 wlif;
	struct wl_interface_info_v1 *pwlif_info = NULL;
	void *ptr;
	wl_wlc_version_t *ver;

	err = wlu_var_getbuf_sm(wl, "wlc_ver", NULL, 0, &ptr);
	if (err == BCME_OK) {
		ver = ptr;
		/*
		 * interface_create version query is available from wlc_ver_major >= 5
		 */
		printf("ver->wlc_ver_major %d\n", ver->wlc_ver_major);
		if ((ver->wlc_ver_major >= 5)) {
			/*
			 * Query the version of interface_create iovar (ver = 0)
			 */
			memset(&wlif, 0, sizeof(wlif));
			err = wlu_var_getbuf_param_len(wl, cmd->name, &wlif, sizeof(int32),
				(void *)&pwlif_info);

			if ((err == BCME_OK) && (pwlif_info->ver == WL_INTERFACE_CREATE_VER_3)) {
				err = wl_interface_create_action_v3(wl, cmd, argv);
				return err;
			}
		}
	} else {
		printf("get wlc_ver failed %d\n", err);
	}

	/* It means older interface_create version. Hence call v2 function */
	/* FW uses wl_interface_create_v2_t version */
	err = wl_interface_create_action_v2(wl, cmd, argv);

	return err;
}

int
wl_maclist(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct maclist *maclist = (struct maclist *) buf;
	struct ether_addr *ea;
	uint i, max = (WLC_IOCTL_MAXLEN - sizeof(int)) / ETHER_ADDR_LEN;
	uint len;
	struct ether_addr tmp_ea;
	bool found;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		maclist->count = htod32(max);
		if ((ret = wlu_get(wl, cmd->get, maclist, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
		maclist->count = dtoh32(maclist->count);
		for (i = 0, ea = maclist->ea; i < maclist->count && i < max; i++, ea++)
			printf("%s %s\n", cmd->name, wl_ether_etoa(ea));
		return 0;
	} else {
		if (cmd->set < 0)
			return -1;
		/* Clear list */
		maclist->count = htod32(0);
		if (!strncmp(*argv, "none", strlen("none")) ||
			!strncmp(*argv, "clear", strlen("clear")))
			return wlu_set(wl, cmd->set, maclist, sizeof(int));
		/* Get old list */
		maclist->count = htod32(max);
		if ((ret = wlu_get(wl, cmd->get, maclist, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
		/* Append to old list */
		maclist->count = dtoh32(maclist->count);
		if (!strncmp(*argv, "del", strlen("del"))) {
			argv++;
			ea = &tmp_ea;
			while (*argv && maclist->count < max) {

				if (!wl_ether_atoe(*argv, ea)) {
					printf("Problem parsing MAC address \"%s\".\n", *argv);
					return -1;
				}
				found = FALSE;
				for (i = 0; i < maclist->count; i++) {
					if (!memcmp(&maclist->ea[i], ea, ETHER_ADDR_LEN)) {
						memcpy(&maclist->ea[i],
						&maclist->ea[maclist->count-1], ETHER_ADDR_LEN);
						maclist->count--;
						found = TRUE;
					}
				}
				if (!found)
					printf("WARNING: cannot find any matched entry"
					"for deleting %s\n", wl_ether_etoa(ea));
			argv++;
			}
		} else {
			ea = &maclist->ea[maclist->count];
			while (*argv && maclist->count < max) {
				if (!wl_ether_atoe(*argv, ea)) {
					printf("Problem parsing MAC address \"%s\".\n", *argv);
					return BCME_USAGE_ERROR;
				}
				maclist->count++;
				ea++;
				argv++;
			}
		}
		/* Set new list */
		len = sizeof(maclist->count) + maclist->count * sizeof(maclist->ea);
		maclist->count = htod32(maclist->count);
		return wlu_set(wl, cmd->set, maclist, len);
	}
}

int
wl_list_ie(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	void *ptr;
	ie_getbuf_t param;

	BCM_REFERENCE(argv);

	param.pktflag = (uint32) -1;
	param.id = (uint8)DOT11_MNG_VS_ID;
	err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(param), &ptr);
	if (err == 0) {
		wl_dump_ie_buf((vndr_ie_buf_t *)ptr);
	} else {
		fprintf(stderr, "Error %d getting IOVar\n", err);
	}

	return err;
}

static void
wl_dump_ie_buf(vndr_ie_buf_t *ie_getbuf)
{
	uchar *iebuf;
	uchar *data;
	int tot_ie, pktflag, iecount, count, datalen, col;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *ie;

	memcpy(&tot_ie, (void *)&ie_getbuf->iecount, sizeof(int));
	tot_ie = dtoh32(tot_ie);
	printf("Total IEs %d\n", tot_ie);

	iebuf = (uchar *)&ie_getbuf->vndr_ie_list[0];

	for (iecount = 0; iecount < tot_ie; iecount++) {
		ie_info = (vndr_ie_info_t *) iebuf;
		memcpy(&pktflag, (void *)&ie_info->pktflag, sizeof(uint32));
		pktflag = dtoh32(pktflag);
		iebuf += sizeof(uint32);

		printf("\n");

		ie = &ie_info->vndr_ie_data;
		printf("IE index = %d\n", iecount);
		printf("-----------------\n");
		printf("Pkt Flg = 0x%x\n", pktflag);
		printf("Length  = %d\n", ie->len);
		printf("OUI     = %02x:%02x:%02x\n",
			ie->oui[0], ie->oui[1], ie->oui[2]);
		printf("Data:\n");

		data = &ie->data[0];
		datalen = ie->len - VNDR_IE_MIN_LEN;
		for (count = 0; (count < datalen);) {
			for (col = 0; (col < MAX_DATA_COLS) &&
				(count < datalen); col++, count++) {
				printf("%02x ", *data++);
			}
			printf("\n");
		}

		iebuf += ie->len + VNDR_IE_HDR_LEN;
	}
}

#if defined(linux)
int
wl_wait_for_event(void *wl, char **argv, uint event_id, uint evbuf_size,
	void (*event_cb_fn)(int event_type, bcm_event_t *bcm_event))
{
	int err = BCME_OK;
	int fd, octets;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	char ifnames[IFNAMSIZ] = {"eth1"};
	uint8 event_buf[WL_EVENTINT_MAX_GET_SIZE];
	eventmsgs_ext_t *eventmsgs;
	char *data;

	/* Override default ifname explicitly or implicitly */
	if (*++argv) {
		if (strlen(*argv) >= IFNAMSIZ) {
			printf("Interface name %s too long\n", *argv);
			return -1;
		}
		strncpy(ifnames, *argv, IFNAMSIZ);
	} else if (wl) {
		strncpy(ifnames, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	}
	ifnames[IFNAMSIZ - 1] = '\0';

	/* set event bit using 'event_msgs_ext' */
	if ((event_id / 8) >= WL_EVENTING_MASK_MAX_LEN) {
		printf("Event Id %d exceeds max %d event bytes\n",
			event_id, WL_EVENTING_MASK_MAX_LEN);
		goto exit2;
	}
	memset(event_buf, 0, sizeof(event_buf));
	eventmsgs = (eventmsgs_ext_t *)event_buf;
	eventmsgs->ver = EVENTMSGS_VER;
	eventmsgs->command = EVENTMSGS_SET_BIT;
	eventmsgs->len = WL_EVENTING_MASK_MAX_LEN;
	eventmsgs->mask[event_id / 8] |= 1 << (event_id % 8);
	if ((err = wlu_var_setbuf(wl, "event_msgs_ext", eventmsgs,
		EVENTMSGS_EXT_STRUCT_SIZE + eventmsgs->len)) < 0) {
		printf("Failed to set event mask\n");
		goto exit2;
	}

	/* Open a socket to read driver WLC_E_* events */
	memset(&ifr, 0, sizeof(ifr));
	if (wl)
		strncpy(ifr.ifr_name, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	else
		strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		err = -1;
		goto exit2;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("%s: Cannot get index %d\n", __FUNCTION__, err);
		goto exit1;
	}

	/* bind the socket first before starting so we won't miss any event */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		goto exit1;
	}

	data = (char*)malloc(evbuf_size);

	if (data == NULL) {
		printf("Cannot not allocate %u bytes for events receive buffer\n",
			evbuf_size);
		err = -1;
		goto exit1;
	}

	/* Loop forever to receive driver events */
	while (1) {
		bcm_event_t *bcm_event;
		int event_type;

		octets = recv(fd, data, evbuf_size, 0);
		bcm_event = (bcm_event_t *)data;
		event_type = ntoh32(bcm_event->event.event_type);
		if (octets >= (int)sizeof(bcm_event_t)) {
			event_cb_fn(event_type, bcm_event);
		}
	}

	free(data);
exit1:
	close(fd);
exit2:
	return err;
}
#endif	/* linux */

int
wlu_var_getbuf(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN);
}

/* get buffer for smaller sizes upto 256 bytes */
int
wlu_var_getbuf_sm(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_SMLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_SMLEN);
}

/* Get buffer for medium sizes upto 1500 bytes */
int
wlu_var_getbuf_med(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MEDLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MEDLEN);
}

int
wlu_var_setbuf_sm(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_SMLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], WLC_IOCTL_SMLEN);
}

int
wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], len);
}

int
wl_nvdump(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	const char *iovar = "nvram_dump";
	void *p = NULL;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvdump/nvram_dump" command name */
	argv++;

	if (*argv) {
		printf("nvdump error: extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_var_getbuf(wl, iovar, NULL, 0, &p)) < 0) {
		if ((err = wlu_get(wl, WLC_NVRAM_DUMP, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		    return err;
		p = (void *)buf;
	}
	fputs((char *)p, stdout);

	return err;
}

/** Queries the driver for the value of a caller supplied nvram variable */
int
wl_nvget(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	char *varname;
	const char *iovar = "nvram_get";
	void *p;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvget/nvram_get" command name */
	argv++;

	if (!*argv) {
		printf("nvget: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if (*argv) {
		printf("nvget error: extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_var_getbuf(wl, iovar, varname, strlen(varname) + 1, &p)) < 0) {

		strcpy(buf, varname);
		if ((err = wlu_get(wl, WLC_NVRAM_GET, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		    return err;
	}

	printf("%s\n", buf);

	return err;
}

int
wl_nvset(void *wl, cmd_t *cmd, char **argv)
{
	char *varname;

	UNUSED_PARAMETER(cmd);

	/* skip the "nvset" command name if present */
	if (!strcmp("nvset", *argv))
		argv++;

	if (!*argv) {
		printf("nvset: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if (*argv) {
		fprintf(stderr,
		"nvset error: extra arg \"%s\"; format is name=value (no spaces around '=')\n",
			*argv);
		return BCME_USAGE_ERROR;
	}

	if (!strchr(varname, '=')) {
		fprintf(stderr,
		"nvset error: no '=' in \"%s\", format is name=value (no spaces around '=')\n",
			*argv);
		return BCME_USAGE_ERROR;
	}

	strcpy(buf, varname);

	return (wlu_set(wl, WLC_NVRAM_SET, &buf[0], strlen(buf) + 1));
}

int
wl_wlc_ver(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err;
	wl_wlc_version_t *ver;
	char buf[256];

	UNUSED_PARAMETER(argv);

	/* skip the command name */
	argv++;

	/* validate absence of arguments */
	if (*argv) {
		fprintf(stderr,
			"\"%s\" wlc_ver iovar doesn't take any arguments\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, &ptr))) {
		return err;
	}

	ver = (wl_wlc_version_t *)ptr;
	sprintf(buf, "wlc_ver %d.%d\n" "epi_ver %d.%d.%d.%d\n",
		ver->wlc_ver_major, ver->wlc_ver_minor, ver->epi_ver_major,
		ver->epi_ver_minor, ver->epi_rc_num, ver->epi_incr_num);

	fputs(buf, stdout);

	return 0;
}

int
wl_struct_ver(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int rslen = 0, rsver = 0;

	UNUSED_PARAMETER(argv);
	UNUSED_PARAMETER(cmd);

	/* skip the command name */
	argv++;
	/* validate absence of arguments */
	if (*argv) {
		fprintf(stderr,
			"\"%s\" wlc_ver iovar doesn't take any arguments\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if ((err = wl_get_rateset_args_info(wl, &rslen, &rsver)) < 0) {
		return err;
	}

	printf("wl_rateset_args_t: v%d\n", rsver);
	return 0;
}

int
wl_assoc_info(void *wl, cmd_t *cmd, char **argv)
{
	uint i, req_ies_len = 0, resp_ies_len = 0;
	wl_assoc_info_t assoc_info;
	int ret;
	uint8 *pbuf;

	UNUSED_PARAMETER(argv);

	/* get the generic association information */
	strcpy(buf, cmd->name);
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
		return ret;

	memcpy(&assoc_info, buf, sizeof(wl_assoc_info_t));
	assoc_info.req_len = htod32(assoc_info.req_len);
	assoc_info.resp_len = htod32(assoc_info.resp_len);
	assoc_info.flags = htod32(assoc_info.flags);

	printf("Assoc req:\n");
	printf("\tlen 0x%x\n", assoc_info.req_len);
	if (assoc_info.req_len) {
		printf("\tcapab  0x%x\n", ltoh16(assoc_info.req.capability));
		printf("\tlisten 0x%x\n", ltoh16(assoc_info.req.listen));
		req_ies_len = assoc_info.req_len - sizeof(struct dot11_assoc_req);
		if (assoc_info.flags & WLC_ASSOC_REQ_IS_REASSOC) {
			printf("\treassoc bssid %s\n",
				wl_ether_etoa(&assoc_info.reassoc_bssid));
			req_ies_len -= ETHER_ADDR_LEN;
		}
	}

	/* get the association req IE's if there are any */
	if (req_ies_len) {
		strcpy(buf, "assoc_req_ies");
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		printf("assoc req IEs:\n\t");
		for (i = 1, pbuf = (uint8*)buf; i <= req_ies_len; i++) {
			printf("0x%02x ", *pbuf++);
			if (!(i%8))
				printf("\n\t");
		}
	}

	printf("\nAssoc resp:\n");
	printf("\tlen 0x%x\n", assoc_info.resp_len);
	if (assoc_info.resp_len) {
		printf("\tcapab  0x%x\n", ltoh16(assoc_info.resp.capability));
		printf("\tstatus 0x%x\n", ltoh16(assoc_info.resp.status));
		printf("\taid    0x%x\n", ltoh16(assoc_info.resp.aid));
		resp_ies_len = assoc_info.resp_len - sizeof(struct dot11_assoc_resp);
	}

	/* get the association resp IE's if there are any */
	if (resp_ies_len) {
		strcpy(buf, "assoc_resp_ies");
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		printf("assoc resp IEs:\n\t");
		for (i = 1, pbuf = (uint8*)buf; i <= resp_ies_len; i++) {
			printf(" 0x%02x ", *pbuf++);
			if (!(i%8))
				printf("\n\t");

		}
	}
	printf("\n");

	return 0;
}

int
wl_rxfifo_counters(void *wl, cmd_t *cmd, char **argv)
{
	char *statsbuf;
	wl_rxfifo_cnt_t cnt;
	int err;
	void *ptr;
	int i;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_sm (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	statsbuf = (char *)ptr;
	memcpy(&cnt, statsbuf, sizeof(cnt));
	cnt.version = dtoh16(cnt.version);
	cnt.length = dtoh16(cnt.length);

	if (cnt.version != WL_RXFIFO_CNT_VERSION) {
		printf("\tIncorrect version of rxfifo counters struct: expected %d; got %d\n",
		   WL_RXFIFO_CNT_VERSION, cnt.version);
		return -1;
	}

	for (i = 0; i < MAX_RX_FIFO; i++) {
		printf("RXFIFO[%d]: data: %u\tmgmtctl: %u \n",
				i, cnt.rxf_data[i], cnt.rxf_mgmtctl[i]);
	}

	return (0);
}

int
wl_hs20_ie(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	int ret;
	int bsscfg_idx = 0;
	int consumed = 0;
	int length;
	int malloc_size;
	tlv_t *tlv;

	UNUSED_PARAMETER(cmd);

	if (!argv[1]) {
		fprintf(stderr, "Too few arguments\n");
		return -1;
	}

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv + 1, argv[0], &bsscfg_idx, &consumed)) != 0)
		return ret;
	if (consumed)
		argv = argv + consumed;

	length = atoi(argv[1]);

	if (length < 0 || length > 255) {
		fprintf(stderr, "Invalid length\n");
		return -1;
	}
	else if (length > 0) {
		if (!argv[2]) {
			fprintf(stderr,
				"Data bytes should be specified for non-zero length\n");
			return -1;
		}
		else {
			/* Ensure each data byte is 2 characters long */
			if ((int)strlen(argv[2]) != (length * 2)) {
				fprintf(stderr, "Please specify all the data bytes for this IE\n");
				return -1;
			}
		}
	}

	malloc_size = OFFSETOF(tlv_t, data) + length;
	tlv = malloc(malloc_size);
	if (tlv == 0) {
		fprintf(stderr, "Error allocating %d bytes for IE params\n", malloc_size);
		return -1;
	}
	tlv->id = DOT11_MNG_VS_ID;
	tlv->len = length;

	if (length > 0) {
		if ((err = get_ie_data((uchar *)argv[2], tlv->data, length))) {
			fprintf(stderr, "Error parsing data arg\n");
			free(tlv);
			return err;
		}
	}

	if (bsscfg_idx == -1)
		err = wlu_var_setbuf(wl, "hs20_ie", tlv, malloc_size);
	else
		err = wlu_bssiovar_setbuf(wl, "hs20_ie", bsscfg_idx,
			tlv, malloc_size, buf, WLC_IOCTL_MAXLEN);

	free(tlv);
	return (err);
}

bool
wl_is_he(void *wl)
{
	/* Get the CAP variable; search for 11ax */
	strncpy(buf, "cap", sizeof("cap"));
	if (wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MEDLEN) >= 0) {
		buf[WLC_IOCTL_MEDLEN] = '\0';
		if (strstr(buf, "11ax")) {
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}
#endif /* BCM_DHD_UNUSED */

/*
 * format an iovar buffer
 * iovar name is converted to lower case
 */
static uint
wl_iovar_mkbuf(const char *name, char *data, uint datalen, char *iovar_buf, uint buflen, int *perr)
{
	uint iovar_len;
	char *p;

	iovar_len = strlen(name) + 1;

	/* check for overflow */
	if ((iovar_len + datalen) > buflen) {
		*perr = BCME_BUFTOOSHORT;
		return 0;
	}

	/* copy data to the buffer past the end of the iovar name string */
	if (datalen > 0)
		memmove(&iovar_buf[iovar_len], data, datalen);

	/* copy the name to the beginning of the buffer */
	strcpy(iovar_buf, name);

	/* wl command line automatically converts iovar names to lower case for
	 * ease of use
	 */
	p = iovar_buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}

	*perr = 0;
	return (iovar_len + datalen);
}

void
init_cmd_batchingmode(void)
{
	cmd_pkt_list_num = 0;
	cmd_batching_mode = FALSE;
}

void
clean_up_cmd_list(void)
{
	wl_seq_cmd_pkt_t *this_cmd, *next_cmd;

	this_cmd = cmd_list.head;
	while (this_cmd != NULL) {
		next_cmd = this_cmd->next;
		if (this_cmd->data != NULL) {
			free(this_cmd->data);
		}
		free(this_cmd);
		this_cmd = next_cmd;
	}
	cmd_list.head = NULL;
	cmd_list.tail = NULL;
	cmd_pkt_list_num = 0;
}

int
add_one_batched_cmd(int cmd, void *cmdbuf, int len)
{
	wl_seq_cmd_pkt_t *new_cmd;

	new_cmd = malloc(sizeof(wl_seq_cmd_pkt_t));

	if (new_cmd == NULL) {
		printf("malloc(%d) failed, free %d batched commands and exit batching mode\n",
			(int)sizeof(wl_seq_cmd_pkt_t), cmd_pkt_list_num);
		goto free_and_exit;
	} else {
#ifdef SERDOWNLOAD
		if (debug)
#endif /* SERDOWNLOAD */
			printf("batching %dth command %d\n", cmd_pkt_list_num+1, cmd);

	}

	new_cmd->cmd_header.cmd = cmd;
	new_cmd->cmd_header.len = len;
	new_cmd->next  = NULL;

	new_cmd->data = malloc(len);

	if (new_cmd->data == NULL) {
		printf("malloc(%d) failed, free %d batched commands and exit batching mode\n",
			len, cmd_pkt_list_num);
		free(new_cmd);
		goto free_and_exit;
	}

	memcpy(new_cmd->data, cmdbuf, len);

	if (cmd_list.tail != NULL)
		cmd_list.tail->next = new_cmd;
	else
		cmd_list.head = new_cmd;

	cmd_list.tail = new_cmd;

	cmd_pkt_list_num ++;
	return 0;

free_and_exit:

	clean_up_cmd_list();

	if (cmd_batching_mode) {
		cmd_batching_mode = FALSE;
	}
	else {
		printf("calling add_one_batched_cmd() at non-command-batching mode, weird\n");
	}

	return -1;
}

int
wlu_get_req_buflen(int cmd, void *cmdbuf, int len)
{
	int modified_len = len;
	char *cmdstr = (char *)cmdbuf;

	if (len == WLC_IOCTL_MAXLEN) {
		if ((strcmp(cmdstr, "dump") == 0) ||
			(cmd == WLC_SCAN_RESULTS))
			modified_len = WLC_IOCTL_MAXLEN;
		else
			modified_len = WLC_IOCTL_MEDLEN;
	}
	return modified_len;
}

/* Wrapper function that converts -W option in to "wlc:" prefix
 * (1) It converts an existing iovar to the following format
 * wlc:<iovar_name>\0<wlc_idx><param>
 * (2) It converts an existing ioctl to the following format
 * wlc:ioc\0<wlc_idx><ioct_cmd_id><param>
 * NOTE: (2) requires new iovar named "ioc" in driver
*/
static int
wlu_wlc_wrapper(void *wl, bool get, int* cmd, void *cmdbuf, int len, void **outbuf, int *outlen)
{
	void *param = cmdbuf;
	int paramlen = len;
	int wlc_idx = g_wlc_idx;
	char *name = NULL;
	BCM_REFERENCE(wl);
	/* Wrap only if we find a valid WLC index and iovar name */
	if (wlc_idx >= 0) {
		int cmdlen = 0;
		int prefix_len = 0;
		char *lbuf = NULL;
		char *buf = NULL;
		bool ioctl_wrap = FALSE;
		if ((*cmd == WLC_GET_VAR) || (*cmd == WLC_SET_VAR)) {
			/* incoming command is an iovar */
			/* pull out name\0param */
			name = cmdbuf;
			cmdlen = strlen(name);
			param = ((char*)cmdbuf) + cmdlen + 1;
			paramlen = len - cmdlen - 1;
		} else {
			/* we are an ioctl, invoke the common "ioc" iovar and wrap the cmd */
			name = "ioc";
			cmdlen = strlen(name);
			/* additional 4 bytes for storing IOCTL_CMD_ID */
			prefix_len = sizeof(int);
			ioctl_wrap = TRUE;
		}
		prefix_len += strlen("wlc:") + 1 +  cmdlen + sizeof(int);
		/* now create wlc:<name>\0<wlc_idx><param> */
		buf = lbuf = malloc(prefix_len + paramlen);
		if (buf == NULL) {
			printf("%s:malloc(%d) failed\n", __FUNCTION__, prefix_len + paramlen);
			return BCME_NOMEM;
		}
		memcpy(buf, "wlc:", 4); buf += 4;
		strcpy(buf, name); buf += (cmdlen+1);
		wlc_idx = htod32(wlc_idx);
		memcpy(buf, &wlc_idx, sizeof(int32)); buf += sizeof(int32);
		if (ioctl_wrap) {
			/* For IOCTL wlc:ioc\0<wlc_idx><ioctl_id><param> */
			int32 ioctl_cmd = htod32(*cmd);
			memcpy(buf, &ioctl_cmd, sizeof(int32)); buf += sizeof(int32);
		}
		memcpy(buf, param, paramlen);
		*cmd = (get) ? WLC_GET_VAR : WLC_SET_VAR;
		param = lbuf;
		paramlen += prefix_len;
	}
	*outlen = paramlen;
	*outbuf = param;
	return BCME_OK;
}
/* now IOCTL GET commands shall call wlu_get() instead of wl_get() so that the commands
 * can be batched when needed
 */
int
wlu_get(void *wl, int cmd, void *cmdbuf, int len)
{
	void *outbuf = NULL;
	int outlen;
	int err = 0;
	if (cmd_batching_mode) {
		if (!WL_SEQ_CMDS_GET_IOCTL_FILTER(cmd)) {
			printf("IOCTL GET command %d is not supported in batching mode\n", cmd);
			return BCME_UNSUPPORTED;
		}
	}

	err = wlu_wlc_wrapper(wl, TRUE, &cmd, cmdbuf, len, &outbuf, &outlen);
	if (err != BCME_OK) return err;
	err = wl_get(wl, cmd, outbuf, outlen);

	if (outbuf != cmdbuf) {
		memcpy(cmdbuf, outbuf, len);
		free(outbuf);
	}
	return err;
}

/* now IOCTL SET commands shall call wlu_set() instead of wl_set() so that the commands
 * can be batched when needed
 */
int
wlu_set(void *wl, int cmd, void *cmdbuf, int len)
{
	int err = 0;
	void *outbuf = NULL;
	int outlen;

	err = wlu_wlc_wrapper(wl, FALSE, &cmd, cmdbuf, len, &outbuf, &outlen);
	if (err != BCME_OK) return err;

	if (cmd_batching_mode) {
		err = add_one_batched_cmd(cmd, outbuf, outlen);
	}
	else {
		err = wl_set(wl, cmd, outbuf, outlen);
	}
	if (outbuf != cmdbuf) {
		memcpy(cmdbuf, outbuf, len);
		free(outbuf);
	}
	return err;

}

/*
 * get named iovar providing both parameter and i/o buffers
 * iovar name is converted to lower case
 */
int
wlu_iovar_getbuf(void* wl, const char *iovar,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;

	wl_iovar_mkbuf(iovar, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_get(wl, WLC_GET_VAR, bufptr, buflen);
}

/*
 * set named iovar providing both parameter and i/o buffers
 * iovar name is converted to lower case
 */
int
wlu_iovar_setbuf(void* wl, const char *iovar,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;
	int iolen;

	iolen = wl_iovar_mkbuf(iovar, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_set(wl, WLC_SET_VAR, bufptr, iolen);
}

/*
 * get named iovar without parameters into a given buffer
 * iovar name is converted to lower case
 */
int
wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len)
{
	char smbuf[WLC_IOCTL_SMLEN];
	int err;

	/* use the return buffer if it is bigger than what we have on the stack */
	if (len > (int)sizeof(smbuf)) {
		err = wlu_iovar_getbuf(wl, iovar, NULL, 0, outbuf, len);
	} else {
		memset(smbuf, 0, sizeof(smbuf));
		err = wlu_iovar_getbuf(wl, iovar, NULL, 0, smbuf, sizeof(smbuf));
		if (err == 0)
			memcpy(outbuf, smbuf, len);
	}

	return err;
}

/*
 * set named iovar given the parameter buffer
 * iovar name is converted to lower case
 */
int
wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen)
{
	char smbuf[WLC_IOCTL_SMLEN*2];

	memset(smbuf, 0, sizeof(smbuf));

	return wlu_iovar_setbuf(wl, iovar, param, paramlen, smbuf, sizeof(smbuf));
}

/*
 * get named iovar as an integer value
 * iovar name is converted to lower case
 */
int
wlu_iovar_getint(void *wl, const char *iovar, int *pval)
{
	int ret;

	ret = wlu_iovar_get(wl, iovar, pval, sizeof(int));
	if (ret >= 0)
	{
		*pval = dtoh32(*pval);
	}
	return ret;
}

/*
 * set named iovar given an integer parameter
 * iovar name is converted to lower case
 */
int
wlu_iovar_setint(void *wl, const char *iovar, int val)
{
	val = htod32(val);
	return wlu_iovar_set(wl, iovar, &val, sizeof(int));
}
/*
 * Name lookup utility.
 * Given a name table and an ID, will return the name matching the ID,
 * or a string with the raw ID, i.e. "ID:<num>"
 */
const char* wlu_lookup_name(const wlu_name_entry_t* tbl, uint id)
{
	const wlu_name_entry_t *elt = tbl;
	static char unknown[64];

	for (elt = tbl; elt->name != NULL; elt++) {
		if (id == elt->id)
			return elt->name;
	}
	snprintf(unknown, sizeof(unknown), "ID:%d", id);
	return unknown;
}

/*
 * format a "prefix" indexed iovar buffer
 */
int
wl_prefixiovar_mkbuf(const char *iovar, const char *prefix, int prefix_index, void *param,
	int paramlen, void *bufptr, int buflen, int *perr)
{
	int8* p;
	uint prefixlen;
	uint namelen;
	uint iolen;

	prefixlen = strlen(prefix);	/* length of iovar prefix "bsscfg:ssid %d wlc:counter %d" */
	namelen = strlen(iovar) + 1;	/* length of iovar name + null */
	iolen = prefixlen + namelen + sizeof(int) + paramlen;

	/* check for overflow */
	if (buflen < 0 || iolen > (uint)buflen) {
		*perr = BCME_BUFTOOSHORT;
		return 0;
	}

	p = (int8*)bufptr;

	/* copy prefix, no null */
	memcpy(p, prefix, prefixlen);
	p += prefixlen;

	/* copy iovar name including null */
	memcpy(p, iovar, namelen);
	p += namelen;

	/* send index as first param */
	prefix_index = htod32(prefix_index);
	memcpy(p, &prefix_index, sizeof(int32));
	p += sizeof(int32);

	/* parameter buffer follows */
	if (paramlen)
		memcpy(p, param, paramlen);

	*perr = 0;
	return iolen;
}

static int
wl_bssiovar_mkbuf(const char *iovar, int bssidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr)
{
	const char *prefix = "bsscfg:";
	return wl_prefixiovar_mkbuf(iovar, prefix, bssidx,  param, paramlen, bufptr, buflen, perr);
}

/*
 * set named & bss indexed driver iovar providing both parameter and i/o buffers
 */
int
wlu_bssiovar_setbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;
	int iolen;

	iolen = wl_bssiovar_mkbuf(iovar, bssidx, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_set(wl, WLC_SET_VAR, bufptr, iolen);
}

/*
 * get named & bss indexed driver iovar providing both parameter and i/o buffers
 */
int
wl_bssiovar_getbuf(void* wl, const char *iovar, int bssidx,
	void *param, int paramlen, void *bufptr, int buflen)
{
	int err;

	wl_bssiovar_mkbuf(iovar, bssidx, param, paramlen, bufptr, buflen, &err);
	if (err)
		return err;

	return wlu_get(wl, WLC_GET_VAR, bufptr, buflen);
}

/*
 * get named & bss indexed driver variable to buffer value
 */
int
wlu_bssiovar_get(void *wl, const char *iovar, int bssidx, void *outbuf, int len)
{
	char smbuf[WLC_IOCTL_SMLEN];
	int err;

	/* use the return buffer if it is bigger than what we have on the stack */
	if (len > (int)sizeof(smbuf)) {
		err = wl_bssiovar_getbuf(wl, iovar, bssidx, NULL, 0, outbuf, len);
	} else {
		memset(smbuf, 0, sizeof(smbuf));
		err = wl_bssiovar_getbuf(wl, iovar, bssidx, NULL, 0, smbuf, sizeof(smbuf));
		if (err == 0)
			memcpy(outbuf, smbuf, len);
	}

	return err;
}

/*
 * set named & bss indexed driver variable to buffer value
 */
int
wl_bssiovar_set(void *wl, const char *iovar, int bssidx, void *param, int paramlen)
{
	char smbuf[WLC_IOCTL_SMLEN];

	memset(smbuf, 0, sizeof(smbuf));

	return wlu_bssiovar_setbuf(wl, iovar, bssidx, param, paramlen, smbuf, sizeof(smbuf));
}

/*
 * get named & bsscfg indexed driver variable as an int value
 */
int
wl_bssiovar_getint(void *wl, const char *iovar, int bssidx, int *pval)
{
	int ret;

	ret = wlu_bssiovar_get(wl, iovar, bssidx, pval, sizeof(int));
	if (ret == 0)
	{
		*pval = dtoh32(*pval);
	}
	return ret;
}

/*
 * set named & bsscfg indexed driver variable to int value
 */
int
wl_bssiovar_setint(void *wl, const char *iovar, int bssidx, int val)
{
	val = htod32(val);
	return wl_bssiovar_set(wl, iovar, bssidx, &val, sizeof(int));
}

void
wl_rateset_get_fields(wl_rateset_args_u_t* rs, int rsver, uint32 **rscount, uint8 **rsrates,
	uint8 **rsmcs, uint16 **rsvht_mcs, uint16 **rshe_mcs)
{
	switch (rsver) {
		case RATESET_ARGS_V1:
			if (rscount)
				*rscount = &rs->rsv1.count;
			if (rsrates)
				*rsrates = rs->rsv1.rates;
			if (rsmcs)
				*rsmcs = rs->rsv1.mcs;
			if (rsvht_mcs)
				*rsvht_mcs = rs->rsv1.vht_mcs;
			break;
		case RATESET_ARGS_V2:
			if (rscount)
				*rscount = &rs->rsv2.count;
			if (rsrates)
				*rsrates = rs->rsv2.rates;
			if (rsmcs)
				*rsmcs = rs->rsv2.mcs;
			if (rsvht_mcs)
				*rsvht_mcs = rs->rsv2.vht_mcs;
			if (rshe_mcs)
				*rshe_mcs = rs->rsv2.he_mcs;
			break;
		/* add new length returning here */
		default:
			/* nothing needed here */
			break;
	}
}

void
wl_print_vhtmcsset(uint16 *mcsset)
{
	int i, j;

	for (i = 0; i < VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		if (mcsset[i]) {
			if (i == 0)
				printf("VHT SET : ");
			else
				printf("        : ");
			/* std MCS 0-9 and prop MCS 10-11 */
			for (j = 0; j <= 11; j++)
				if (isbitset(mcsset[i], j))
					printf("%dx%d ", j, i + 1);
			printf("\n");
		} else {
			break;
		}
	}
}

int
wl_void(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(argv);

	if (cmd->set < 0)
		return -1;
	return wlu_set(wl, cmd->set, NULL, 0);
}

int
wl_var_setint(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	char *varname;
	char *endptr = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("set: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if (!*argv) {
		printf("set: missing value argument for set of \"%s\"\n", varname);
		return BCME_USAGE_ERROR;
	}

	val = strtoul(*argv, &endptr, 0);
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer for set of \"%s\"\n",
			*argv, varname);
		return BCME_USAGE_ERROR;
	}

	return wlu_iovar_setint(wl, varname, val);
}

int
wl_reg(void *wl, cmd_t *cmd, char **argv)
{
	int reg;
	int ret;
	struct {
		int val;
		int band;
	} x;
	char *endptr = NULL;
	uint argc;
	bool core_cmd;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return BCME_USAGE_ERROR;

	reg = strtol(argv[0], &endptr, 0);

	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	x.val = 0;
	x.band = WLC_BAND_AUTO;
	core_cmd = FALSE;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	/* Second arg: value or band or "radio core" */
	if (argc >= 2) {
		if (!stricmp(argv[1], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[1], "b"))
			x.band = WLC_BAND_2G;
		else {
			/* For NPHY Rev >= 3, the 2nd argument can be
			   the radio core
			 */
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[1], "syn") == 0) {
					reg |= RADIO_CORE_SYN;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "tx0") == 0) {
					reg |= RADIO_CORE_TX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "tx1") == 0) {
					reg |= RADIO_CORE_TX1;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "rx0") == 0) {
					reg |= RADIO_CORE_RX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[1], "rx1") == 0) {
					reg |= RADIO_CORE_RX1;
					core_cmd = TRUE;
				}
			}
			/* For HTPHY/ACPHY, the 2nd argument can be
			   the radio core
			 */
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (phytype == WLC_PHY_TYPE_AC) {
					if (strcmp(argv[1], "cr0") == 0) {
						reg |= RADIO_2069_CORE_CR0;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "cr1") == 0) {
						reg |= RADIO_2069_CORE_CR1;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "cr2") == 0) {
						reg |= RADIO_2069_CORE_CR2;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "pll") == 0) {
						reg |= RADIO_2069_CORE_PLL;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "pll0") == 0) {
						reg |= RADIO_2069_CORE_PLL0;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "pll1") == 0) {
						reg |= RADIO_2069_CORE_PLL1;
						core_cmd = TRUE;
					}
				} else {
					if (strcmp(argv[1], "cr0") == 0) {
						reg |= RADIO_CORE_CR0;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "cr1") == 0) {
						reg |= RADIO_CORE_CR1;
						core_cmd = TRUE;
					} else if (strcmp(argv[1], "cr2") == 0) {
						reg |= RADIO_CORE_CR2;
						core_cmd = TRUE;
					}
				}
			}
			/* If the second argument is a value */
			if (!core_cmd) {
				x.val = strtol(argv[1], &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
			}
		}
	}

	/* Third arg: band OR "radio core" */
	if (argc >= 3) {
		if (!stricmp(argv[2], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[2], "b"))
			x.band = WLC_BAND_2G;
		else {
			/* For NPHY Rev >= 3, the 3rd argument can be
			   the radio core
			 */
			core_cmd = FALSE;
			if (strcmp(cmd->name, "radioreg") == 0) {
				if (strcmp(argv[2], "syn") == 0) {
					reg |= RADIO_CORE_SYN;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "tx0") == 0) {
					reg |= RADIO_CORE_TX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "tx1") == 0) {
					reg |= RADIO_CORE_TX1;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "rx0") == 0) {
					reg |= RADIO_CORE_RX0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "rx1") == 0) {
					reg |= RADIO_CORE_RX1;
					core_cmd = TRUE;
				}
			}
			/* For HTPHY/ACPHY, the 3rd argument can be
			   the radio core
			*/
			if (phytype == WLC_PHY_TYPE_AC) {
				if (strcmp(argv[2], "cr0") == 0) {
					reg |= RADIO_2069_CORE_CR0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "cr1") == 0) {
					reg |= RADIO_2069_CORE_CR1;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "cr2") == 0) {
					reg |= RADIO_2069_CORE_CR2;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "pll") == 0) {
					reg |= RADIO_2069_CORE_PLL;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "pll0") == 0) {
					reg |= RADIO_2069_CORE_PLL0;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "pll1") == 0) {
					reg |= RADIO_2069_CORE_PLL1;
					core_cmd = TRUE;
				} else if (strcmp(argv[2], "all") == 0) {
					reg |= RADIO_2069_CORE_ALL;
					core_cmd = TRUE;
				}
			} else {
				if (strcmp(cmd->name, "radioreg") == 0) {
					if (strcmp(argv[2], "cr0") == 0) {
						reg |= RADIO_CORE_CR0;
						core_cmd = TRUE;
					} else if (strcmp(argv[2], "cr1") == 0) {
						reg |= RADIO_CORE_CR1;
						core_cmd = TRUE;
					} else if (strcmp(argv[2], "cr2") == 0) {
						reg |= RADIO_CORE_CR2;
						core_cmd = TRUE;
					} else if (strcmp(argv[2], "all") == 0) {
						reg |= RADIO_CORE_ALL;
						core_cmd = TRUE;
					}
				}
			}

			if (!core_cmd) {
				return BCME_USAGE_ERROR;
			}
		}
	}

	x.val = (x.val << 16) | (reg & 0xffff);

	/* issue the get or set ioctl */
	if ((argc == 1) || ((argc == 2) && ((x.band != WLC_BAND_AUTO) || core_cmd))) {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		if ((ret = wlu_get(wl, cmd->get, &x, sizeof(x))) < 0)
			return (ret);
		printf("0x%04x\n", (uint16)(dtoh32(x.val)));
	} else {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		ret = wlu_set(wl, cmd->set, &x, sizeof(x));
	}

	return (ret);
}

/* Set or Get the "ssid" iovar, with an optional config index argument:
 *	wl ssid [-C N]|[--cfg=N] ssid
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_SSID ioctl will be used
 */
int
wl_ssid(void *wl, cmd_t *cmd, char **argv)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	wlc_ssid_t ssid = { 0, {0} };
	int bsscfg_idx = 0;
	int consumed;
	int error;

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "ssid", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (*argv == NULL) {
		if (consumed == 0) {
			/* no config index, use WLC_GET_SSID on the interface */
			if (cmd->get == WLC_GET_SSID)
				error = wlu_get(wl, WLC_GET_SSID, &ssid, sizeof(ssid));
			else
				error = wlu_iovar_get(wl, cmd->name, &ssid, sizeof(ssid));
		} else {
			if (cmd->get == WLC_GET_SSID) {
				/* use "ssid" iovar since a config option was given */
				error = wlu_bssiovar_get(wl, "ssid", bsscfg_idx, &ssid,
				                        sizeof(ssid));
			} else {
				error = wlu_bssiovar_get(wl, cmd->name, bsscfg_idx, &ssid,
				                        sizeof(ssid));
			}
		}
		if (error < 0)
			return error;

		ssid.SSID_len = dtoh32(ssid.SSID_len);
		wl_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		printf("Current %s: \"%s\"\n",
		       (cmd->get == WLC_GET_SSID)? "SSID": cmd->name,
		       ssidbuf);
	} else {
		if (strlen(argv[0]) > DOT11_MAX_SSID_LEN) {
			fprintf(stderr, "SSID arg \"%s\" must be 32 chars or less\n", argv[0]);
			return BCME_BADARG;
		}
		ssid.SSID_len = strlen(argv[0]);
		memcpy(ssid.SSID, argv[0], ssid.SSID_len);

		wl_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		printf("Setting %s: \"%s\"\n", (cmd->set == WLC_SET_SSID)? "SSID": cmd->name,
		       ssidbuf);

		ssid.SSID_len = htod32(ssid.SSID_len);
		if (consumed == 0) {
			/* no config index given, use WLC_SET_SSID */
			if (cmd->set == WLC_SET_SSID) {
				error = wlu_set(wl, WLC_SET_SSID, &ssid, sizeof(wlc_ssid_t));
			} else {
				error = wlu_iovar_set(wl, cmd->name, &ssid, sizeof(wlc_ssid_t));
			}
		} else {
			if (cmd->set == WLC_SET_SSID) {
				/* use "ssid" iovar since a config option was given */
				error = wl_bssiovar_set(wl, "ssid", bsscfg_idx, &ssid,
				                        sizeof(wlc_ssid_t));
			} else
				error = wl_bssiovar_set(wl, cmd->name, bsscfg_idx, &ssid,
				                        sizeof(wlc_ssid_t));
		}
	}
	return error;
}

/* IO variables that take a MAC address */
int
wl_iov_mac(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr ea = {{0, 0, 0, 0, 0, 0}};

	if (argv[1]) { /* set */
		if (!wl_ether_atoe(argv[1], &ea)) {
			printf(" ERROR: no valid ether addr provided\n");
			return BCME_USAGE_ERROR;
		}
		if ((ret = wlu_iovar_set(wl, cmd->name, &ea, ETHER_ADDR_LEN)) < 0) {
			printf("Error setting variable %s\n", argv[0]);
			return ret;
		}
		return 0;
	} else { /* get */
		if ((ret = wlu_iovar_get(wl, cmd->name, &ea, ETHER_ADDR_LEN)) < 0) {
			printf("Error getting variable %s\n", argv[0]);
			return ret;
		}
		printf("%s %s\n", argv[0], wl_ether_etoa(&ea));
	}

	return 0;
}

/* Commands that take a MAC address */
int
wl_macaddr(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr ea;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &ea, ETHER_ADDR_LEN)) < 0)
			return ret;
		printf("%s\n", wl_ether_etoa(&ea));
		return 0;
	} else {
		if (!wl_ether_atoe(*argv, &ea))
			return BCME_USAGE_ERROR;
		return wlu_set(wl, cmd->set, &ea, ETHER_ADDR_LEN);
	}
}

/* channel info structure */
typedef struct {
	uint	chan;		/* channel number */
	uint	freq;		/* in Mhz */
} chan_info_t;

static chan_info_t chan_info[] = {
	/* B channels */
	{ 1,	2412},
	{ 2,	2417},
	{ 3,	2422},
	{ 4,	2427},
	{ 5,	2432},
	{ 6,	2437},
	{ 7,	2442},
	{ 8,	2447},
	{ 9,	2452},
	{ 10,	2457},
	{ 11,	2462},
	{ 12,	2467},
	{ 13,	2472},
	{ 14,	2484},

	/* A channels */
	/* 11a usa low */
	{ 36,	5180},
	{ 40,	5200},
	{ 44,	5220},
	{ 48,	5240},
	{ 52,	5260},
	{ 56,	5280},
	{ 60,	5300},
	{ 64,	5320},

	/* 11a Europe */
	{ 100,	5500},
	{ 104,	5520},
	{ 108,	5540},
	{ 112,	5560},
	{ 116,	5580},
	{ 120,	5600},
	{ 124,	5620},
	{ 128,	5640},
	{ 132,	5660},
	{ 136,	5680},
	{ 140,	5700},
	{ 144,	5720},

	/* 11a usa high */
	{ 149,	5745},
	{ 153,	5765},
	{ 157,	5785},
	{ 161,	5805},

	/* 11a japan */
	{ 184,	4920},
	{ 188,	4940},
	{ 192,	4960},
	{ 196,	4980},
	{ 200,	5000},
	{ 204,	5020},
	{ 208,	5040},
	{ 212,	5060},
	{ 216,	5080}
};

uint
freq2channel(uint freq)
{
	int i;

	for (i = 0; i < (int)ARRAYSIZE(chan_info); i++) {
		if (chan_info[i].freq == freq)
			return (chan_info[i].chan);
	}
	return (0);
}

void
dump_rateset(uint8 *rates, uint count)
{
	uint i;
	uint r;
	bool b;
	bool sel = FALSE;	/* flag indicating BSS Membership Selector(s) */

	printf("[ ");
	for (i = 0; i < count; i++) {
		r = rates[i] & 0x7f;
		b = rates[i] & 0x80;
		/* Assuming any "rate" above 54 Mbps is a BSS Membership Selector value */
		if (r > WLC_MAXRATE) {
			sel = TRUE;
			continue;
		}
		if (r == 0)
			break;
		printf("%d%s%s ", (r / 2), (r % 2)?".5":"", b?"(b)":"");
	}
	/* Now print the BSS Membership Selector values (r standars for raw value) */
	if (sel) {
		for (i = 0; i < count && rates[i] != 0; i ++) {
			if ((rates[i] & 0x7f) <= WLC_MAXRATE) {
				continue;
			}
			printf("%02X(r) ", rates[i]);
		}
	}
	printf("]");
}

int
wl_ether_atoe(const char *a, struct ether_addr *n)
{
	char *c = NULL;
	int i = 0;

	memset(n, 0, ETHER_ADDR_LEN);
	for (;;) {
		n->octet[i++] = (uint8)strtoul(a, &c, 16);
		if (!*c++ || i == ETHER_ADDR_LEN)
			break;
		a = c;
	}
	return (i == ETHER_ADDR_LEN);
}
char *
wl_ether_etoa(const struct ether_addr *n)
{
	static char etoa_buf[ETHER_ADDR_LEN * 3];
	char *c = etoa_buf;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", n->octet[i] & 0xff);
	}
	return etoa_buf;
}

int
wl_atoip(const char *a, struct ipv4_addr *n)
{
	char *c = NULL;
	int i = 0;

	for (;;) {
		n->addr[i++] = (uint8)strtoul(a, &c, 0);
		if (*c++ != '.' || i == IPV4_ADDR_LEN)
			break;
		a = c;
	}
	return (i == IPV4_ADDR_LEN);
}

char *
wl_iptoa(const struct ipv4_addr *n)
{
	static char iptoa_buf[IPV4_ADDR_LEN * 4];

	sprintf(iptoa_buf, "%u.%u.%u.%u",
	        n->addr[0], n->addr[1], n->addr[2], n->addr[3]);

	return iptoa_buf;
}

static int
wl_var_getint(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 val;
	char *varname;

	UNUSED_PARAMETER(cmd);

	if (!*argv) {
		printf("get: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	varname = *argv++;

	if ((err = wlu_iovar_getint(wl, varname, &val)))
		return (err);

	if (val < 10)
		printf("%d\n", val);
	else
		printf("%d (0x%x)\n", val, val);

	return (0);
}

/* just issue a wl_var_setint() or a wl_var_getint() if there is a 2nd arg */
int
wl_varint(void *wl, cmd_t *cmd, char *argv[])
{
	if (argv[1])
		return (wl_var_setint(wl, cmd, argv));
	else
		return (wl_var_getint(wl, cmd, argv));
}

void
wl_dump_raw_ie(bcm_tlv_t *ie, uint len)
{
	uint dump_len;

	if (len == 0) {
		return;
	} else if (len == 1) {
		printf("IE header truncated: ID: 0x%02X\n", ie->id);
		return;
	} else if (len < (uint)(ie->len + TLV_HDR_LEN)) {
		printf("IE data truncated: ID: 0x%02X Len: %d\n", ie->id, ie->len);
		dump_len = len - TLV_HDR_LEN;
	} else {
		printf("ID: 0x%02X Len: %d\n", ie->id, ie->len);
		dump_len = ie->len;
	}

	/* choose how to format the data based on data len */
	if (dump_len > 16)
		printf("Data:\n");
	else if (dump_len > 0)
		printf("Data: ");

	if (dump_len > 0)
		wl_hexdump(ie->data, dump_len);

	if (dump_len < ie->len)
		printf("<missing %d bytes>\n", ie->len - dump_len);

	return;
}

int
wl_mk_ie_setbuf(const char *command, uint32 pktflag_ok, char **argv,
	vndr_ie_setbuf_t **buf, int *buf_len)
{
	vndr_ie_setbuf_t *ie_setbuf;
	uint32 pktflag;
	int ielen, datalen, buflen, iecount;
	int err = 0;

	if (!argv[1] || !argv[2] || !argv[3]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	pktflag = (uint)strtol(argv[1], 0, 0);

	if (pktflag & ~pktflag_ok) {
		fprintf(stderr, "Invalid packet flag 0x%x (%d)\n", pktflag, pktflag);
		return BCME_BADARG;
	}

	ielen = atoi(argv[2]);
	if (ielen > VNDR_IE_MAX_LEN) {
		fprintf(stderr, "IE length is %d, should be <= %d\n", ielen, VNDR_IE_MAX_LEN);
		return BCME_BADARG;
	}
	else if (ielen < VNDR_IE_MIN_LEN) {
		fprintf(stderr, "IE length is %d, should be >= %d\n", ielen, VNDR_IE_MIN_LEN);
		return BCME_BADARG;
	}

	if (strlen(argv[3]) != OUI_STR_SIZE) {
		fprintf(stderr, "Invalid OUI length %d\n", (int)strlen(argv[3]));
		return BCME_BADARG;
	}

	datalen = ielen - VNDR_IE_MIN_LEN;
	if (datalen > 0) {
		if (!argv[4]) {
			fprintf(stderr, "Data bytes should be specified for IE of length %d\n",
			        ielen);
			return BCME_USAGE_ERROR;
		}
		else {
			/* Ensure each data byte is 2 characters long */
			if ((int)strlen (argv[4]) < (datalen * 2)) {
				fprintf(stderr, "Please specify all the data bytes for this IE\n");
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (datalen == 0 && (argv[4] != NULL))
		fprintf(stderr, "Ignoring data bytes for IE of length %d", ielen);

	buflen = sizeof(vndr_ie_setbuf_t) + datalen - 1;

	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);

	if (ie_setbuf == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return BCME_NOMEM;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strncpy(ie_setbuf->cmd, command, VNDR_IE_CMD_LEN - 1);
	ie_setbuf->cmd[VNDR_IE_CMD_LEN - 1] = '\0';

	/* Buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/*
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	pktflag = htod32(pktflag);
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag,
	       &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.id = (uchar)DOT11_MNG_VS_ID;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uchar) ielen;

	if ((err = get_oui_bytes ((uchar *)argv[3],
		&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0]))) {
		free(ie_setbuf);
		fprintf(stderr, "Error parsing OUI arg\n");
		return BCME_BADARG;
	}

	if (datalen > 0) {
		if ((err = get_ie_data ((uchar *)argv[4],
			&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0],
			datalen))) {
			free(ie_setbuf);
			fprintf(stderr, "Error parsing data arg\n");
			return BCME_BADARG;
		}
	}

	/* Copy-out */
	if (buf) {
		*buf = ie_setbuf;
		ie_setbuf = NULL;
	}
	if (buf_len)
		*buf_len = buflen;

	/* Clean-up */
	if (ie_setbuf)
		free(ie_setbuf);

	return (err);
}

int
wl_format_ssid(char* ssid_buf, uint8* ssid, int ssid_len)
{
	int i, c;
	char *p = ssid_buf;

	if (ssid_len > 32)
		ssid_len = 32;

	for (i = 0; i < ssid_len; i++) {
		c = (int)ssid[i];
		if (c == '\\') {
			*p++ = '\\';
			*p++ = '\\';
		} else if (isprint((uchar)c)) {
			*p++ = (char)c;
		} else {
			p += sprintf(p, "\\x%02X", c);
		}
	}
	*p = '\0';

	return p - ssid_buf;
}

/* pretty hex print a contiguous buffer */
void
wl_hexdump(uchar *dump_buf, uint nbytes)
{
	char line[256];
	char* p;
	uint i;

	if (nbytes == 0) {
		printf("\n");
		return;
	}

	p = line;
	for (i = 0; i < nbytes; i++) {
		if (i % 16 == 0 && nbytes > 16) {
			p += sprintf(p, "%04d: ", i);	/* line prefix */
		}
		p += sprintf(p, "%02x ", dump_buf[i]);
		if (i % 16 == 15) {
			printf("%s\n", line);		/* flush line */
			p = line;
		}
	}

	/* flush last partial line */
	if (p != line)
		printf("%s\n", line);
}

/* wl scan
 * -s --ssid=ssid_list
 * -t T --scan_type=T : [active|passive]
 * --bss_type=T : [infra|bss|adhoc|ibss]
 * -b --bssid=
 * -n --nprobes=
 * -a --active=
 * -p --passive=
 * -h --home=
 * -c --channels=
 * ssid_list
 */

/* Parse a comma-separated list from list_str into ssid array, starting
 * at index idx.  Max specifies size of the ssid array.  Parses ssids
 * and returns updated idx; if idx >= max not all fit, the excess have
 * not been copied.  Returns -1 on empty string, or on ssid too long.
 */
int
wl_parse_ssid_list(char* list_str, wlc_ssid_t* ssid, int idx, int max)
{
	char *str, *ptr;

	if (list_str == NULL)
		return -1;

	for (str = list_str; str != NULL; str = ptr) {
		if ((ptr = strchr(str, ',')) != NULL)
			*ptr++ = '\0';

		if (strlen(str) > DOT11_MAX_SSID_LEN) {
			fprintf(stderr, "ssid <%s> exceeds %d\n", str, DOT11_MAX_SSID_LEN);
			return -1;
		}
		if (strlen(str) == 0)
			ssid[idx].SSID_len = 0;

		if (idx < max) {
			strcpy((char*)ssid[idx].SSID, str);
			ssid[idx].SSID_len = strlen(str);
		}
		idx++;
	}

	return idx;
}

int
wlu_bcmp(const void *b1, const void *b2, int len)
{
	return (memcmp(b1, b2, len));
}

/* Common routine to check for an option arg specifying the configuration index.
 * Takes the syntax -C num, --cfg=num, --config=num, or --configuration=num
 * Returns BCME_BADARG if there is a command line parsing error.
 * Returns 0 if no error, and sets *consumed to the number of argv strings
 * used. Sets *bsscfg_idx to the index to use. Will set *bsscfg_idx to zero if there
 * was no config arg.
 */
int
wl_cfg_option(char **argv, const char *fn_name, int *bsscfg_idx, int *consumed)
{
	miniopt_t mo;
	int opt_err;

	*bsscfg_idx = 0;
	*consumed = 0;

	miniopt_init(&mo, fn_name, NULL, FALSE);

	/* process the first option */
	opt_err = miniopt(&mo, argv);

	/* check for no args or end of options */
	if (opt_err == -1)
		return 0;

	/* check for no options, just a positional arg encountered */
	if (mo.positional)
		return 0;

	/* check for error parsing options */
	if (opt_err == 1)
		return BCME_USAGE_ERROR;

	/* check for -C, --cfg=X, --config=X, --configuration=X */
	if (mo.opt == 'C' ||
	    !strcmp(mo.key, "cfg") ||
	    !strcmp(mo.key, "config") ||
	    !strcmp(mo.key, "configuration")) {
		if (!mo.good_int) {
			fprintf(stderr,
			"%s: could not parse \"%s\" as an integer for the configuartion index\n",
			fn_name, mo.valstr);
			return BCME_BADARG;
		}
		*bsscfg_idx = mo.val;
		*consumed = mo.consumed;
	}

	return 0;
}

int
wl_scan_prep(void *wl, cmd_t *cmd, char **argv, wl_scan_params_t *params, int *params_size)
{
	int val = 0;
	char key[64];
	int keylen;
	char *p, *eq, *valstr, *endptr = NULL;
	char opt;
	bool positional_param;
	bool good_int;
	bool opt_end;
	int err = 0;
	int i;

	int nchan = 0;
	int nssid = 0;
	wlc_ssid_t ssids[WL_SCAN_PARAMS_SSID_MAX];

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->bss_type = DOT11_BSSTYPE_ANY;
	params->scan_type = 0;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;
	memset(ssids, 0, WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t));

	/* skip the command name */
	argv++;

	opt_end = FALSE;
	while ((p = *argv) != NULL) {
		argv++;
		positional_param = FALSE;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;
		good_int = FALSE;

		if (opt_end) {
			positional_param = TRUE;
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr,
				"wl_scan: missing \" = \" in long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63)
				keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr,
				"wl_scan: missing value after \" = \" in long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr,
				"wl_scan: only single char options, error on param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr,
				"wl_scan: missing value parameter after \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			valstr = *argv;
			argv++;
		} else {
			positional_param = TRUE;
			valstr = p;
		}

		/* parse valstr as int just in case */
		val = (int)strtol(valstr, &endptr, 0);
		if (*endptr == '\0') {
			/* not all the value string was parsed by strtol */
			good_int = TRUE;
		}

		if (opt == 's' || !strcmp(key, "ssid") || positional_param) {
			nssid = wl_parse_ssid_list(valstr, ssids, nssid, WL_SCAN_PARAMS_SSID_MAX);
			if (nssid < 0) {
				err = BCME_BADARG;
				goto exit;
			}
		}

		/* scan_type is a bitmap value and can have multiple options */
		if (opt == 't' || !strcmp(key, "scan_type")) {
			if (!strcmp(valstr, "active")) {
				/* do nothing - scan_type is initialized outside of while loop */
			} else if (!strcmp(valstr, "passive")) {
				params->scan_type |= WL_SCANFLAGS_PASSIVE;
			} else if (!strcmp(valstr, "prohibit")) {
				params->scan_type |= WL_SCANFLAGS_PROHIBITED;
			} else if (!strcmp(valstr, "offchan")) {
				params->scan_type |= WL_SCANFLAGS_OFFCHAN;
			} else if (!strcmp(valstr, "hotspot")) {
				params->scan_type |= WL_SCANFLAGS_HOTSPOT;
			} else if (!strcmp(valstr, "lowpriority")) {
				params->scan_type |= WL_SCANFLAGS_LOW_PRIO;
			} else {
				fprintf(stderr,
					"scan_type value should be \"active\", "
					"\"passive\", \"prohibit\", \"offchan\", "
					"\"hotspot\" or \"lowpriority\", but got \"%s\"\n", valstr);
				err = BCME_USAGE_ERROR;
				goto exit;
			}

		}
		if (!strcmp(key, "bss_type")) {
			if (!strcmp(valstr, "bss") || !strcmp(valstr, "infra")) {
				params->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
			} else if (!strcmp(valstr, "ibss") || !strcmp(valstr, "adhoc")) {
				params->bss_type = DOT11_BSSTYPE_INDEPENDENT;
			} else if (!strcmp(valstr, "any")) {
				params->bss_type = DOT11_BSSTYPE_ANY;
			} else {
				fprintf(stderr,
				"bss_type value should be "
				"\"bss\", \"ibss\", or \"any\", but got \"%s\"\n", valstr);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		if (opt == 'b' || !strcmp(key, "bssid")) {
			if (!wl_ether_atoe(valstr, &params->bssid)) {
				fprintf(stderr,
				"could not parse \"%s\" as an ethernet MAC address\n", valstr);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		if (opt == 'n' || !strcmp(key, "nprobes")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for value nprobes\n", valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->nprobes = val;
		}
		if (opt == 'a' || !strcmp(key, "active")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for active dwell time\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->active_time = val;
		}
		if (opt == 'p' || !strcmp(key, "passive")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for passive dwell time\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->passive_time = val;
		}
		if (opt == 'h' || !strcmp(key, "home")) {
			if (!good_int) {
				fprintf(stderr,
				"could not parse \"%s\" as an int for home channel dwell time\n",
					valstr);
				err = BCME_BADARG;
				goto exit;
			}
			params->home_time = val;
		}
		if (opt == 'c' || !strcmp(key, "chanspecs")) {
			nchan = wl_parse_chanspec_list(valstr, params->channel_list,
			                              WL_NUMCHANNELS);
			if (nchan == -1) {
				fprintf(stderr, "error parsing chanspec list arg\n");
				err = BCME_BADARG;
				goto exit;
			}
		}
	}

	if (nssid > WL_SCAN_PARAMS_SSID_MAX) {
		fprintf(stderr, "ssid count %d exceeds max of %d\n",
		        nssid, WL_SCAN_PARAMS_SSID_MAX);
		err = BCME_BADARG;
		goto exit;
	}

	params->nprobes = htod32(params->nprobes);
	params->active_time = htod32(params->active_time);
	params->passive_time = htod32(params->passive_time);
	params->home_time = htod32(params->home_time);

	for (i = 0; i < nchan; i++) {
		params->channel_list[i] = htodchanspec(params->channel_list[i]);
	}

	for (i = 0; i < nssid; i++) {
		ssids[i].SSID_len = htod32(ssids[i].SSID_len);
	}

	/* For a single ssid, use the single fixed field */
	if (nssid == 1) {
		nssid = 0;
		memcpy(&params->ssid, &ssids[0], sizeof(ssids[0]));
	}

	/* Copy ssid array if applicable */
	if (nssid > 0) {
		i = OFFSETOF(wl_scan_params_t, channel_list) + nchan * sizeof(uint16);
		i = ROUNDUP(i, sizeof(uint32));
		if (i + nssid * sizeof(wlc_ssid_t) > (uint)*params_size) {
			fprintf(stderr, "additional ssids exceed params_size\n");
			err = BCME_BADARG;
			goto exit;
		}

		p = (char*)params + i;
		memcpy(p, ssids, nssid * sizeof(wlc_ssid_t));
		p += nssid * sizeof(wlc_ssid_t);
	} else {
		p = (char*)params->channel_list + nchan * sizeof(uint16);
	}

	params->channel_num = htod32((nssid << WL_SCAN_PARAMS_NSSID_SHIFT) |
	                             (nchan & WL_SCAN_PARAMS_COUNT_MASK));
	*params_size = p - (char*)params + nssid * sizeof(wlc_ssid_t);
exit:
	return err;
}

int
wl_parse_channel_list(char* list_str, uint16* channel_list, int channel_num)
{
	int num;
	int val;
	char* str;
	char* endptr = NULL;

	if (list_str == NULL)
		return -1;

	str = list_str;
	num = 0;
	while (*str != '\0') {
		val = (int)strtol(str, &endptr, 0);
		if (endptr == str) {
			fprintf(stderr,
				"could not parse channel number starting at"
				" substring \"%s\" in list:\n%s\n",
				str, list_str);
			return -1;
		}
		str = endptr + strspn(endptr, " ,");

		if (num == channel_num) {
			fprintf(stderr, "too many channels (more than %d) in channel list:\n%s\n",
				channel_num, list_str);
			return -1;
		}

		channel_list[num++] = (uint16)val;
	}

	return num;
}

int
get_ie_data(uchar *data_str, uchar *ie_data, int len)
{
	uchar *src, *dest;
	uchar val;
	int idx;
	char hexstr[3];

	src = data_str;
	dest = ie_data;

	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;
	}

	return 0;
}

int
get_oui_bytes(uchar *oui_str, uchar *oui)
{
	int idx;
	uchar val;
	uchar *src, *dest;
	char hexstr[3];

	src = oui_str;
	dest = oui;

	for (idx = 0; idx < MAX_OUI_SIZE; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;

		if ((idx < (MAX_OUI_SIZE - 1)) && (*src++ != ':'))
			return -1;
	}

	return 0;
}

void
wl_print_mcsset(char *mcsset)
{
	int i;

	printf("MCS SET : [ ");
	for (i = 0; i < (MCSSET_LEN * 8); i++)
		if (isset(mcsset, i))
			printf("%d ", i);
	printf("]\n");
}

int wl_ipv6_colon(const char *a, char *x)
{
	int     i;
	const char	*t;
	int     colons = 0;
	int     double_colons = 0;
	int     zero_req = 0;

	if (*a == ':' && *(a+1) != ':')
		return 1;		/* Illegal */
	t = a;
	while ((t = strstr(t, "::")) != NULL) {
		++t;
	++double_colons;
	}

	if (double_colons == 0) {
	strcpy(x, a);			/* No double colon in the address */
	return 0;
	}

	if (double_colons > 1) {
	return 1;			/* Illegal */
	}
	t = a;
	while ((t = strchr(t, ':')) != NULL) {
		++t;
		++colons;
	}
	zero_req = 8 - colons;
	if (zero_req) {
	t = a;
		while (*t) {
			if (*t == ':' && *(t+1) == ':') {
				if (t == a) {
					*x++ = '0';
				}
				*x++ = *t++;
				for (i = 0; i < zero_req; i++) {
					*x++ = '0';
					*x++ = ':';
				}
				t++;
				} else {
				*x++ = *t++;
			}
		}
	} else {
		strcpy(x, a);
	}
		return 0;
}

int
wl_atoipv6(const char *a, struct ipv6_addr *n)
{
	char *c = NULL;
	int i = 0;
	uint16 *addr16;
	char x[64];
	char *t = x;

	memset(x, 0, 64);

	if (wl_ipv6_colon(a, x) == 1) {
		return 0;
	}

	for (;;) {
		addr16 = (uint16 *)&n->addr[i];
		*addr16 = hton16((uint16)strtoul((char *)t, &c, 16));
		i += 2;
		if (*c++ != ':' || i == IPV6_ADDR_LEN)
			break;
		t = c;
	}

	return (i == IPV6_ADDR_LEN);
}

/* Convert user's input in hex pattern to byte-size mask */
int
wl_pattern_atoh(char *src, char *dst)
{
	int i;
	if (strncmp(src, "0x", 2) != 0 &&
	    strncmp(src, "0X", 2) != 0) {
		printf("Data invalid format. Needs to start with 0x\n");
		return -1;
	}
	src = src + 2; /* Skip past 0x */
	if (strlen(src) % 2 != 0) {
		printf("Data invalid format. Needs to be of even length\n");
		return -1;
	}
	for (i = 0; *src != '\0'; i++) {
		char num[3];
		strncpy(num, src, 2);
		num[2] = '\0';
		dst[i] = (uint8)strtoul(num, NULL, 16);
		src += 2;
	}
	return i;
}

int
ARGCNT(char **argv)
{
	int i;

	for (i = 0; argv[i] != NULL; i ++)
		;
	return i;
}

int
wl_parse_chanspec_list(char *list_str, chanspec_t *chanspec_list, int chanspec_num)
{
	int num = 0;
	chanspec_t chanspec;
	char *next, str[8];
	size_t len;

	if ((next = list_str) == NULL)
		return BCME_ERROR;

	while ((len = strcspn(next, " ,")) > 0) {
		if (len >= sizeof(str)) {
			fprintf(stderr, "string \"%s\" before ',' or ' ' is too long\n", next);
			return BCME_ERROR;
		}
		strncpy(str, next, len);
		str[len] = 0;
		chanspec = wf_chspec_aton(str);
		if (chanspec == 0) {
			fprintf(stderr, "could not parse chanspec starting at "
			        "\"%s\" in list:\n%s\n", str, list_str);
			return BCME_ERROR;
		}
		if (num == chanspec_num) {
			fprintf(stderr, "too many chanspecs (more than %d) in chanspec list:\n%s\n",
				chanspec_num, list_str);
			return BCME_ERROR;
		}
		chanspec_list[num++] = chanspec;
		next += len;
		next += strspn(next, " ,");
	}

	return num;
}

int
hexstr2hex(char *str)
{
	int i, len;
	char hexstr[3];
	char *src;

	src = str;
	len = strlen(str)/2;

	for (i = 0; i < len; i++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';
		str[i] = strtoul(hexstr, NULL, 16);
		src += 2;
	}

	return i;
}

/* Find an entry in argv[][] in this form
 *	name=value, could be pattern=(0x)1234 or pattern=ABC
 *
 * If *val is NULL, return the pointer to value.
 * If *val is not NULL, fill the value into val, return the pointer to name if found,
 * return NULL if no match found.
 */
char*
find_pattern(char **argv, const char *pattern, uint *val)
{
	char *ret = NULL, *name = NULL, **pargv = argv;

	/* clear val first */
	if (val)	*val = 0;

	while ((name = *pargv++)) {
		if ((ret = strstr(name, pattern))) {
			char *p = ret, *q = NULL;

			/* Extracting the content */
			p += strlen(pattern);

			/* var name could have same prefix */
			if (*p++ != '=') {
				ret = NULL;
				continue;
			}
			if (!val)
				return (ret+strlen(pattern)+1);

			*val = strtoul(p, &q, 0);
			if (p == q) {
				printf("Bad value: %s\n", ret);
				return NULL;
			}

			break;
		}
	}
	return ret;
}

/* Find an entry in argv[][] in this form
 *	name=value1,value2,...,value(n)
 *		n is indicated by vnum
 * 		could be pattern=(0x)1234,... or pattern=ABC,...
 *
 * If *val is NULL, return the pointer to value.
 * If *val is not NULL, fill the value into val, return the pointer to name if found,
 * return NULL if no match found.
 */
char*
find_pattern2(char **argv, const char *pattern, uint *val, int vnum)
{
	char *ret = NULL, *name = NULL, **pargv = argv;
	int i;

	while ((name = *pargv++)) {
		if ((ret = strstr(name, pattern))) {
			char *p = ret, *q = NULL;

			/* Extracting the content */
			p += strlen(pattern);

			/* var name could have same prefix */
			if (*p++ != '=') {
				ret = NULL;
				continue;
			}
			if (!val)
				return (ret+strlen(pattern)+1);

			for (i = 0; i < vnum; i ++)
			{
				val[i] = strtoul(p, &q, 0);

				if (p == q) {
					printf("Bad value: %s\n", ret);
					return NULL;
				}
				p = q + 1;	/* skip ',' */
			}
			break;
		}
	}
	return ret;
}

char *
wl_ipv6toa(const void *ipv6)
{
	/* Implementing RFC 5952 Sections 4 + 5 */
	/* Not thoroughly tested */
	uint16 *a = (uint16 *)ipv6;

	/* Returned buffer is from a static circular pool to permit several calls in a printf */
#define IPV6_BUFFER_CNT	4
	static char buffer[IPV6_BUFFER_CNT][IPV6_ADDR_LEN * 4];
	static int idx = 0;

	char *p = buffer[idx++ % IPV6_BUFFER_CNT];
	int i, i_max = -1, cnt = 0, cnt_max = 1;
	uint8 *a4 = NULL;

	for (i = 0; i < IPV6_ADDR_LEN/2; i++) {
		if (a[i]) {
			if (cnt > cnt_max) {
				cnt_max = cnt;
				i_max = i - cnt;
			}
			cnt = 0;
		} else
			cnt++;
	}
	if (cnt > cnt_max) {
		cnt_max = cnt;
		i_max = i - cnt;
	}
	if (i_max == 0 &&
		/* IPv4-translated: ::ffff:0:a.b.c.d */
		((cnt_max == 4 && a[4] == 0xffff && a[5] == 0) ||
		/* IPv4-mapped: ::ffff:a.b.c.d */
		(cnt_max == 5 && a[5] == 0xffff)))
		a4 = (uint8*) (a + 6);

	for (i = 0; i < IPV6_ADDR_LEN/2; i++) {
		if ((uint8*) (a + i) == a4) {
			sprintf(p, ":%u.%u.%u.%u", a4[0], a4[1], a4[2], a4[3]);
			break;
		} else if (i == i_max) {
			*p++ = ':';
			i += cnt_max - 1;
			p[0] = ':';
			p[1] = '\0';
		} else {
			if (i)
				*p++ = ':';
			p += sprintf(p, "%x", ntoh16(a[i]));
		}
	}

	/* Sub-buffer start is found back by rounding p with the sub-buffer size */
	return buffer[(p - buffer[0]) / sizeof(buffer[0])];
}

int
parse_wep(char **argv, wl_wsec_key_t *key, bool options)
{
	char hex[] = "XX";
	unsigned char *data = key->data;
	char *keystr = *argv;

	switch (strlen(keystr)) {
	case 5:
	case 13:
	case 16:
		key->len = strlen(keystr);
		memcpy(data, keystr, key->len + 1);
		break;
	case 12:
	case 28:
	case 34:
	case 66:
		/* strip leading 0x */
		if (!strnicmp(keystr, "0x", 2))
			keystr += 2;
		else
			return -1;
		/* fall through */
	case 10:
	case 26:
	case 32:
	case 64:
		key->len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		return -1;
	}

	switch (key->len) {
	case 5:
		key->algo = CRYPTO_ALGO_WEP1;
		break;
	case 13:
		key->algo = CRYPTO_ALGO_WEP128;
		break;
	case 16:
		/* default to AES-CCM */
		key->algo = CRYPTO_ALGO_AES_CCM;
		break;
	case 32:
		key->algo = CRYPTO_ALGO_TKIP;
		break;
	default:
		return -1;
	}

	/* Set as primary key by default */
	key->flags |= WL_PRIMARY_KEY;

	if (options) {
		/* Get options */
		while (*++argv) {
			if (!strnicmp("ccm", *argv, 3) && key->len == 16)
				key->algo = CRYPTO_ALGO_AES_CCM;
			else if (!strnicmp("ocb", *argv, 3) && key->len == 16)
				key->algo = CRYPTO_ALGO_AES_OCB_MPDU;
			else if (!strnicmp("notx", *argv, 4))
				key->flags &= ~WL_PRIMARY_KEY;
			else if (!wl_ether_atoe(*argv, &key->ea))
				memset(&key->ea, 0, ETHER_ADDR_LEN);
		}
	}

	return 0;
}

void
wl_cmd_usage(FILE *fid, cmd_t *cmd)
{
	if (strlen(cmd->name) >= 8)
		fprintf(fid, "%s\n\t%s\n\n", cmd->name, cmd->help);
	else
		fprintf(fid, "%s\t%s\n\n", cmd->name, cmd->help);
}

int
wl_print_deprecate(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(argv);

	wl_cmd_usage(stderr, cmd); /* warning string is in command table */
	return 0;
}

/* Command may or may not take a MAC address */
int
wl_rssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	scb_val_t scb_val;
	int32 rssi;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &rssi, sizeof(rssi))) < 0)
			return ret;
		printf("%d\n", (int8) dtoh32(rssi));
		return 0;
	} else {
		if (!wl_ether_atoe(*argv, &scb_val.ea))
			return BCME_USAGE_ERROR;
		if ((ret = wlu_get(wl, cmd->get, &scb_val, sizeof(scb_val))) < 0)
			return ret;
		printf("%d\n", dtoh32(scb_val.val));
		return 0;
	}
}

/* Get/Set the gmode config */
int
wl_gmode(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr = NULL;
	int ret = 0, val;

	if (!*++argv) {
		const char *gconfig;

		/* Get the current G mode */
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(val))) < 0)
			return ret;

		val = dtoh32(val);
		switch (val) {
		case GMODE_LEGACY_B:
			gconfig = "54g Legacy B";
			break;
		case GMODE_AUTO:
			gconfig = "54g Auto";
			break;
		case GMODE_ONLY:
			gconfig = "54g Only";
			break;
		case GMODE_PERFORMANCE:
			gconfig = "54g Performance";
			break;
		case GMODE_LRS:
			gconfig = "54g LRS";
			break;
		default:
			gconfig = "unknown";
			break;
		}

		printf("%s (%d)\n", gconfig, val);

	} else {
		/* Set the new G mode */

		if (!strnicmp(*argv, "legacy", 6))
			val = GMODE_LEGACY_B;
		else if (!strnicmp(*argv, "auto", 4))
			val = GMODE_AUTO;
		else if (!strnicmp(*argv, "gonly", 5))
			val = GMODE_ONLY;
		else if (!strnicmp(*argv, "perf", 4))
			val = GMODE_PERFORMANCE;
		else if (!strnicmp(*argv, "lrs", 3))
			val = GMODE_LRS;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
		}

		/* Set the gmode configration */
		val = htod32(val);
		if ((ret = wlu_set(wl, cmd->set, &val, sizeof(val))))
			goto done;

	}

done:
	return (ret);
}

/* Format a ratespec for "nrate" output
 * Will handle both current wl_ratespec and legacy (ioctl_version 1) nrate ratespec
 */
void
wl_nrate_print(uint32 rspec, int ioctl_version)
{
	const char * rspec_auto = "auto";
	uint encode, rate, txexp = 0, bw_val;
	const char* stbc = "";
	const char* ldpc = "";
	const char* bw = "";
	int stf;

	if (rspec == 0) {
		encode = WL_RSPEC_ENCODE_RATE;
	} else if (ioctl_version == 1) {
		encode = (rspec & OLD_NRATE_MCS_INUSE) ? WL_RSPEC_ENCODE_HT : WL_RSPEC_ENCODE_RATE;
		stf = (int)((rspec & OLD_NRATE_STF_MASK) >> OLD_NRATE_STF_SHIFT);
		rate = (rspec & OLD_NRATE_RATE_MASK);

		if (rspec & OLD_NRATE_OVERRIDE) {
			if (rspec & OLD_NRATE_OVERRIDE_MCS_ONLY)
				rspec_auto = "fixed mcs only";
			else
				rspec_auto = "fixed";
		}
	} else {
		int siso;
		encode = (rspec & WL_RSPEC_ENCODING_MASK);
		rate = (rspec & WL_RSPEC_RATE_MASK);
		txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;
		stbc  = ((rspec & WL_RSPEC_STBC) != 0) ? " stbc" : "";
		ldpc  = ((rspec & WL_RSPEC_LDPC) != 0) ? " ldpc" : "";
		bw_val = (rspec & WL_RSPEC_BW_MASK);

		if (bw_val == WL_RSPEC_BW_20MHZ) {
			bw = "bw20";
		} else if (bw_val == WL_RSPEC_BW_40MHZ) {
			bw = "bw40";
		} else if (bw_val == WL_RSPEC_BW_80MHZ) {
			bw = "bw80";
		} else if (bw_val == WL_RSPEC_BW_160MHZ) {
			bw = "bw160";
		}
		else if (bw_val == WL_RSPEC_BW_10MHZ) {
			bw = "bw10";
		} else if (bw_val == WL_RSPEC_BW_5MHZ) {
			bw = "bw5";
		} else if (bw_val == WL_RSPEC_BW_2P5MHZ) {
			bw = "bw2.5";
		}

		/* initialize stf mode to an illegal value and
		 * fix to a backward compatable value if possible
		 */
		stf = -1;
		/* for stf calculation, determine if the rate is single stream.
		 * Legacy rates WL_RSPEC_ENCODE_RATE are single stream, and
		 * HT rates for mcs 0-7 are single stream
		 */
		siso = (encode == WL_RSPEC_ENCODE_RATE) ||
			((encode == WL_RSPEC_ENCODE_HT) && rate < 8);

		/* calc a value for nrate stf mode */
		if (txexp == 0) {
			if ((rspec & WL_RSPEC_STBC) && siso) {
				/* STF mode STBC */
				stf = OLD_NRATE_STF_STBC;
			} else {
				/* STF mode SISO or SDM */
				stf = (siso) ? OLD_NRATE_STF_SISO : OLD_NRATE_STF_SDM;
			}
		} else if (txexp == 1 && siso) {
			/* STF mode CDD */
			stf = OLD_NRATE_STF_CDD;
		}

		if (rspec & WL_RSPEC_OVERRIDE_RATE) {
			rspec_auto = "fixed";
		}
	}

	if (encode == WL_RSPEC_ENCODE_RATE) {
		if (rspec == 0) {
			printf("auto\n");
		} else {
			printf("legacy rate %d%s Mbps stf mode %d %s\n",
			       rate/2, (rate % 2)?".5":"", stf, rspec_auto);
		}
	} else if (encode == WL_RSPEC_ENCODE_HT) {
		printf("mcs index %d stf mode %d %s\n",
		       rate, stf, rspec_auto);
	} else if (encode == WL_RSPEC_ENCODE_VHT) {
		const char* sgi = "";
		uint vht = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		sgi   = ((rspec & WL_RSPEC_SGI)  != 0) ? " sgi"  : "";

		printf("vht mcs %d Nss %d Tx Exp %d %s%s%s%s %s\n",
		       vht, Nss, txexp, bw, stbc, ldpc, sgi, rspec_auto);
	} else if (encode == WL_RSPEC_ENCODE_HE) {
		const char* gi_ltf[] = {" 1xLTF GI 0.8us", " 2xLTF GI 0.8us",
			" 2xLTF GI 1.6us", " 4xLTF GI 3.2us"};
		uint8 gi_int = RSPEC_HE_LTF_GI(rspec);
		uint he = (rspec & WL_RSPEC_HE_MCS_MASK);
		uint Nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;

		printf("he mcs %d Nss %d Tx Exp %d %s%s%s%s %s\n",
		       he, Nss, txexp, bw, stbc, ldpc, gi_ltf[gi_int], rspec_auto);
	}
}

static dbg_msg_t wl_wnm_msgs[] = {
	{WL_WNM_BSSTRANS,	"BSS-Transition"},
	{WL_WNM_PROXYARP,	"Proxy-ARP"},
	{WL_WNM_MAXIDLE,	"BSS-Max-Idle-Period"},
	{WL_WNM_TIMBC,		"TIM-Broadcast"},
	{WL_WNM_TFS,		"Traffic-Filtering"},
	{WL_WNM_SLEEP,		"WNM-Sleep-Mode"},
	{WL_WNM_DMS,		"Directed-Multicast"},
	{WL_WNM_FMS,		"Flexible-Multicast"},
	{WL_WNM_NOTIF,		"Notification"},
	{0, ""}
};

void
wl_wnm_print(uint32 wnmmask)
{
	int i;
	printf("0x%x:", wnmmask);
	for (i = 0; wl_wnm_msgs[i].value; i++) {
		if (wnmmask & wl_wnm_msgs[i].value) {
			printf("  %s", wl_wnm_msgs[i].string);
		}
	}
	printf("\n");
	return;

}

/* RM Enable Capabilities */
static dbg_msg_t rrm_msgs[] = {
	{DOT11_RRM_CAP_LINK,	"Link_Measurement"},				/* bit0 */
	{DOT11_RRM_CAP_NEIGHBOR_REPORT,	"Neighbor_Report"},			/* bit1 */
	{DOT11_RRM_CAP_PARALLEL,	"Parallel_Measurement"},		/* bit2 */
	{DOT11_RRM_CAP_REPEATED,	"Repeated_Measurement"},		/* bit3 */
	{DOT11_RRM_CAP_BCN_PASSIVE,	"Beacon_Passive"},			/* bit4 */
	{DOT11_RRM_CAP_BCN_ACTIVE,	"Beacon_Active"},			/* bit5 */
	{DOT11_RRM_CAP_BCN_TABLE,	"Beacon_Table"},			/* bit6 */
	{DOT11_RRM_CAP_BCN_REP_COND,	"Beacon_measurement_Reporting_Condition"}, /* bit7 */
	{DOT11_RRM_CAP_FM,	"Frame_Measurement"},				/* bit8 */
	{DOT11_RRM_CAP_CLM,	"Channel_load_Measurement"},			/* bit9 */
	{DOT11_RRM_CAP_NHM,	"Noise_Histogram_measurement"},			/* bit10 */
	{DOT11_RRM_CAP_SM,	"Statistics_Measurement"},			/* bit11 */
	{DOT11_RRM_CAP_LCIM,	"LCI_Measurement"},				/* bit12 */
	{DOT11_RRM_CAP_LCIA,	"LCI_Azimuth"},					/* bit13 */
	{DOT11_RRM_CAP_TSCM,	"Tx_Stream_Category_Measurement"},		/* bit14 */
	{DOT11_RRM_CAP_TTSCM,	"Triggered_Tx_stream_Category_Measurement"},	/* bit15 */
	{DOT11_RRM_CAP_AP_CHANREP,	"AP_Channel_Report"},			/* bit16 */
	{DOT11_RRM_CAP_RMMIB,	"RM_MIB"},					/* bit17 */
	/* bit 18-23, unused */
	{DOT11_RRM_CAP_MPC0,	"Measurement_Pilot_Capability_Bit0"},		/* bit24 */
	{DOT11_RRM_CAP_MPC1,	"Measurement_Pilot_Capability_Bit1"},		/* bit25 */
	{DOT11_RRM_CAP_MPC2,	"Measurement_Pilot_Capability_Bit2"},		/* bit26 */
	{DOT11_RRM_CAP_MPTI,	"Measurement_Pilot_Transmission_Information"},	/* bit27 */
	{DOT11_RRM_CAP_NBRTSFO,	"Neighbor_Report_TSF_Offset"},			/* bit28 */
	{DOT11_RRM_CAP_RCPI,	"RCPI_Measurement"},				/* bit29 */
	{DOT11_RRM_CAP_RSNI,	"RSNI_Measurement"},				/* bit30 */
	{DOT11_RRM_CAP_BSSAAD,	"BSS_Average_Access_Delay"},			/* bit31 */
	{DOT11_RRM_CAP_BSSAAC,	"BSS_Available_Admission_Capacity"},		/* bit32 */
	{DOT11_RRM_CAP_AI,	"Antenna_Information"},				/* bit33 */
	{DOT11_RRM_CAP_FTM_RANGE,	"FTM_Range_Reporting"},			/* bit34 */
	{DOT11_RRM_CAP_CIVIC_LOC,	"Civic_Location_Measurement"},		/* bit35 */
	{DOT11_RRM_CAP_IDENT_LOC,	"Identifier_Location_Measurement"},	/* bit36 */
	{0,		NULL}
};

void
wl_rrm_print(uint8 *rrm_cap)
{
	int i;
	uint high = 0, low = 0, bit = 0, hbit = 0;
	dbg_msg_t *dbg_msg = rrm_msgs;

	high = rrm_cap[4];
	low = rrm_cap[0] | (rrm_cap[1] << 8) | (rrm_cap[2] << 16) | (rrm_cap[3] << 24);

	printf("RRM capability = ");
	if (high != 0)
		printf("0x%x%08x", high, low);
	else
		printf("0x%x ", low);

	for (i = 0; ((bit = dbg_msg[i].value) <= DOT11_RRM_CAP_BSSAAD); i++) {
		if (low & (1 << bit))
			printf(" %s", dbg_msg[i].string);
	}

	for (; (hbit = dbg_msg[i].value); i++) {
		if (high & (1 << (hbit - DOT11_RRM_CAP_BSSAAC)))
			printf(" %s", dbg_msg[i].string);
	}
	printf("\n");

	return;
}

void
wl_print_hemcsset(uint16 *mcsset)
{
	int i, j;
	static const char zero[sizeof(uint16) * WL_HE_CAP_MCS_MAP_NSS_MAX] = { 0 };

	if (mcsset == NULL ||
			!memcmp(mcsset, zero, sizeof(uint16) * WL_HE_CAP_MCS_MAP_NSS_MAX)) {
		return;
	}
	for (i = 0; i < WL_HE_CAP_MCS_MAP_NSS_MAX; i++) {
		if (mcsset[i]) {
			if (i == 0)
				printf("HE SET  : ");
			else
				printf("        : ");
			/* std MCS 10-11 */
			for (j = 0; j <= 11; j++)
				if (isbitset(mcsset[i], j))
					printf("%dx%d ", j, i + 1);
			printf("\n");
		} else {
			break;
		}
	}
}

void
wl_print_hemcsnss(uint16 *mcsset)
{
	int i, nss;
	static const char zero[sizeof(uint16) * WL_HE_CAP_MCS_MAP_NSS_MAX] = { 0 };

	uint rx_mcs, tx_mcs;
	char *rx_mcs_str, *tx_mcs_str, *bw_str;
	uint16 he_txmcsmap, he_rxmcsmap;

	if (mcsset == NULL || !memcmp(mcsset, zero, sizeof(uint16) * WL_HE_CAP_MCS_MAP_NSS_MAX)) {
		return;
	}

	for (i = 0; i < 3; i++) {
		if (i == 0) {
			bw_str = "80 Mhz";
		} else if (i == 1) {
			bw_str = "160 Mhz";
		} else {
			bw_str = "80+80 Mhz";
		}

		/* get he bw80, bw160, bw80p80 tx mcs from mcsset[0], mcsset[2], and mcsset[4] */
		he_txmcsmap = dtoh16(mcsset[i * 2]);
		/* get he bw80, bw160, bw80p80 rx mcs from mcsset[1], mcsset[3], and mcsset[5] */
		he_rxmcsmap = dtoh16(mcsset[(i * 2) + 1]);

		for (nss = 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
			tx_mcs = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, he_txmcsmap);
			rx_mcs = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, he_rxmcsmap);
			tx_mcs_str =
				(tx_mcs == HE_CAP_MAX_MCS_0_11 ? "0-11      " :
				(tx_mcs == HE_CAP_MAX_MCS_0_9 ? "0-9       " :
				(tx_mcs == HE_CAP_MAX_MCS_0_7 ? "0-7       " :
					"---       ")));
			rx_mcs_str =
				(rx_mcs == HE_CAP_MAX_MCS_0_11 ? "0-11" :
				(rx_mcs == HE_CAP_MAX_MCS_0_9 ? "0-9" :
				(rx_mcs == HE_CAP_MAX_MCS_0_7 ? "0-7" :
				"---")));
			if ((tx_mcs != HE_CAP_MAX_MCS_NONE) ||
				(rx_mcs != HE_CAP_MAX_MCS_NONE)) {
				if (nss == 1)
					printf("\t    %s:\n", bw_str);
				printf("\t\tNSS%d Tx: %s  Rx: %s\n", nss,
					tx_mcs_str, rx_mcs_str);
			}
		}
	}
}

/* Set or Get the "cur_etheraddr" iovar, with an optional config index argument:
 *	wl cur_etheraddr [-C N]|[--cfg=N] <MAC addr>
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the IOV_CUR_ETHERADDR ioctl will be used
 */
int
wlu_cur_etheraddr(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int consumed;
	int error;
	struct ether_addr ea = {{0, 0, 0, 0, 0, 0}};

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "cur_etheraddr", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;
	if (*argv == NULL) {
		if (consumed == 0) {
			/* no config index, use IOV_CUR_ETHERADDR on the interface */
			error = wlu_iovar_get(wl, cmd->name, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "cur_etheraddr" iovar since a config option was given */
			error = wlu_bssiovar_get(wl, cmd->name, bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
		if (error < 0)
			return error;

		printf("%s: %s\n", cmd->name, wl_ether_etoa(&ea));
	} else {
		if (!wl_ether_atoe(argv[0], &ea)) {
			printf(" ERROR: no valid ether addr provided\n");
			return BCME_USAGE_ERROR;
		}

		if (consumed == 0) {
			/* no config index given, use IOV_CUR_ETHERADDR */
			error = wlu_iovar_set(wl, cmd->name, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "cur_etheraddr" iovar since a config option was given */
			error = wl_bssiovar_set(wl, cmd->name, bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
	}
	return error;
}
