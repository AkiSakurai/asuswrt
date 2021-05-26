/*
 * Wlm (Wireless LAN Manufacturing) test library.
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
 * $Id: wlm.c 783700 2020-02-06 10:16:19Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <typedefs.h>

#include <bcmcdc.h>      // cdc_ioctl_t used in wlu_remote.h
#include <bcmendian.h>
#include <bcmutils.h>    // ARRAYSIZE, bcmerrorstr()
#include <bcmsrom_fmt.h> // SROM4_WORDS
#include <bcmsrom_tbl.h> // pavars_t
#include <wlioctl.h>
#include <wlioctl_utils.h>
#include <ethernet.h>	 // ETHER_ADDR_LEN

#if defined(LINUX)
#include <sys/socket.h>
#include <bcmip.h> // ipv4_addr
#include <arpa/inet.h>	// struct sockaddr_in
#include <string.h>
#include <signal.h>
#endif // endif

#include <epivers.h>
#include "wlu_remote.h"  // wl remote type defines (ex: NO_REMOTE)
#include "wlu_pipe.h"    // rwl_open_pipe()
#include "wlu.h"         // wl_ether_atoe()
#include "wlm.h"
#include "wlc_ppr.h"
#include "wluc_otp.h"
#include "wlu_common.h"

static void * irh;
#define HANDLE void *

#if !defined(TARGETOS_nucleus)
#define MAX_INTERFACE_NAME_LENGTH     128
static char interfaceName[MAX_INTERFACE_NAME_LENGTH + 1] = {0};
#endif // endif
static WLM_BAND curBand = WLM_BAND_AUTO;
static int curPhyType = PHY_TYPE_NULL;
static int ioctl_version = WLC_IOCTL_VERSION;
#if defined(linux)
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif /* linux */
extern char g_rem_ifname[IFNAMSIZ];

extern int wl_os_type_get_rwl(void);
extern void wl_os_type_set_rwl(int os_type);
extern int wl_interface_create_action(void *wl, cmd_t *cmd, char **argv);
extern int wl_ir_init_rwl(HANDLE *irh);
extern int wl_ir_init_adapter_rwl(HANDLE *irh, int adapter);
extern void wl_close_rwl(int remote_type, HANDLE irh);
extern int rwl_init_socket(void);

extern int wlu_get(void *wl, int cmd, void *buf, int len);
extern int wlu_set(void *wl, int cmd, void *buf, int len);

extern int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len);
extern int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
extern int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
extern int wlu_iovar_setint(void *wl, const char *iovar, int val);
extern int wlu_iovar_getbuf(void *wl, const char *iovar, void *param, int paramlen,
    void *bufptr, int buflen);
extern int wlu_iovar_setbuf(void *wl, const char *iovar, void *param, int paramlen,
    void *bufptr, int buflen);

extern int wlu_var_getbuf(void *wl, const char *iovar, void *param, int param_len, void **bufptr);
extern int wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len);
extern int wlu_var_getbuf_med(void *wl, const char *iovar, void *param, int parmlen, void **bufptr);
extern int wlu_var_setbuf_med(void *wl, const char *iovar, void *param, int param_len);
extern int wlu_var_getbuf_sm(void *wl, const char *iovar, void *param, int parmlen, void **bufptr);
extern int wlu_var_setbuf_sm(void *wl, const char *iovar, void *param, int param_len);
extern int wlu_var_getbuf_minimal(void *wl, const char *iovar, void *param, int param_len,
		void **bufptr);

extern int wl_seq_batch_in_client(bool enable);
extern int wl_seq_start(void *wl, cmd_t *cmd, char **argv);
extern int wl_seq_stop(void *wl, cmd_t *cmd, char **argv);

extern char *ver2str(unsigned int vms, unsigned int vls);
static int wlmPhyTypeGet(void);
extern chanspec_t wl_chspec_to_legacy(chanspec_t chspec);
extern int process_clm_data(void *wl, char *clmfilename, int ds_id);
extern int process_txcap_data(void *wl, char *txcapfilename);
extern int process_cal_data(void *wl, char *calfilename);
extern int process_cal_dump(void *wl, char *fname_txcal, char *fname_rxcal);
extern int ota_loadtest(void *wl, char *command, char **argv);
extern void wl_otatest_display_skip_test_reason(int8 skip_test_reason);
extern void wl_ota_display_test_option(wl_ota_test_args_t *test_arg, int16 cnt);
extern void wl_ota_display_test_init_info(wl_ota_test_status_t *init_info);
extern int wlu_cisupdate(void *wl, cmd_t *cmd, char **argv);
extern int wlu_ciswrite(void *wl, cmd_t *cmd, char **argv);
extern int wl_otpraw(void *wl, cmd_t *cmd, char **argv);
extern int wl_otpw(void *wl, cmd_t *cmd, char **argv);
extern int8 wlu_get_crc_config(void *wl, otpcrc_config_t *otpcrc_config);
extern uint8* wlu_get_otp_read_buf(void *wl, uint32 *size);
extern int wlu_read_otp_data(void *wl, uint8 *buf, int nbytes);
extern int wlu_check_otp_integrity(uint8 *buf, otpcrc_config_t *otpcrc_config);
extern int8 wlu_update_crc(void *wl, uint8 *buf, uint32 preview, otpcrc_config_t *otpcrc_config);
extern int wlu_otpcrcconfig(void *wl, cmd_t *cmd, char **argv);
extern int8 wl_txcal_mode(void *wl);
extern int wl_ampdu_send_addba(void *wl, cmd_t *cmd, char **argv);

#define WLM_NUM_ARGS 32
#define WLM_MAX_ARG_LEN 32
/* max size of int after converted to str. 32-bit int max 10 digits plus NULL */
#define WLM_MAX_INT2STR_SIZE 11

#define CALLOC_ARG_MEM(ptr, size) \
	ptr = CALLOC(size); \
	if (ptr == NULL) { \
		ret = BCME_NOMEM; \
		printf("Malloc failed\n"); \
		goto cleanup; \
	}

#define CHECK_SNP_BUF_OVERRUN(buffer, req_size, buf_size) \
	buffer += req_size; \
	buf_size -= req_size; \
	if (buf_size <= 0) { \
		printf("Buffer too short\n"); \
		return FALSE; \
	} \

#define TXCAL_DETECT_MODE \
	mode = wl_txcal_mode(irh); \
	if (mode < 0) { \
		printf("BPHY or OFDM rate needs to be specified properly!\n"); \
		goto fail; \
	} \

#define TXCAL_PRINT_MODE \
	if (txcal_tssi->ofdm) \
		printf("Mode = OFDM\n"); \
	else \
		printf("Mode = BPHY\n"); \

#define WLMPRVALV1(name) stats += sprintf(stats, "%s %u ", #name, dtoh32(cnt_v1->name))
#define WLMPRVAL(name) stats += sprintf(stats, "%s %u ", #name, dtoh32(cnt->name))
#define WLMPRNL() stats += sprintf(stats, "\n")

#define MASK(sbit, nbits) ((uint32)(((1 << (nbits)) - 1) << (sbit)))
#define OFF_MASK(sbit, nbits) (~MASK(sbit, nbits))
#define MASKED_VAL(val, sbit, nbits) (((uint32)(val)) & OFF_MASK(sbit, nbits))
#define TRIMMED_VAL(val, sbit, nbits) (((uint32)(val)) & MASK(sbit, nbits))
#define SHIFT_VAL(val, sbit, nbits) (TRIMMED_VAL(val, 0, nbits) << (sbit))
#define BIT_OR(reg, val, sbit, nbits) (MASKED_VAL(reg, sbit, nbits) | SHIFT_VAL(val, sbit, nbits))

static char errorString[256];
static const char *wlm_version_string = WLM_VERSION_STR;
extern bool cmd_batching_mode;
extern int g_rwl_device_baud;

#define BCM_CONFIG_ARRAY_SIZE 10
#define NAME_SIZE 20
struct nv_s {
	char *name;
	uint32 value;
};
struct config_iovar_s {
	char *iovar_name;
	struct nv_s params[BCM_CONFIG_ARRAY_SIZE];
};

struct config_iovar_s config_iovar_list[] = {
	/* XXX: If you are adding a new config iovar entry to this array, keep it above this line
	 */
	{ NULL, {{"auto", 0}, {"off", 0}, {"disable", 0}, {"on", 0},
	{"enable", 0}, {NULL, 0}}},
};

static struct config_iovar_s *get_config_for_iovar(const char *iovar_name);

static const char *
wlmLastError(void)
{
	static const char *bcmerrorstrtable[] = BCMERRSTRINGTABLE;
	static char errorString[256];
	int bcmerror;

	memset(errorString, 0, sizeof(errorString));

	if (wlu_iovar_getint(irh, "bcmerror", &bcmerror)) {
		sprintf(errorString, "%s", "Failed to retrieve error");
		return errorString;
	}

	if (bcmerror > 0 || bcmerror < BCME_LAST) {
		sprintf(errorString, "Undefined error (%d)", bcmerror);
	} else {
		sprintf(errorString, "%s (%d)", bcmerrorstrtable[-bcmerror], bcmerror);
	}

	return errorString;
}

const char *
wlmGetLastError(void)
{
	return errorString;
}

int wlmWLMVersionGet(const char **buffer)
{
	*buffer = wlm_version_string;
	return TRUE;
}

int wlmApiInit(void)
{
	curBand = WLM_BAND_AUTO;
	return TRUE;
}

int wlmApiCleanup(void)
{
#if !defined(TARGETOS_nucleus)
	wl_close_rwl(rwl_get_remote_type(), irh);
#endif // endif
	irh = 0;
	strncpy(g_rem_ifname, "", IFNAMSIZ);
	return TRUE;
}

int wlmSelectInterface(WLM_DUT_INTERFACE ifType, char *ifName,
	WLM_DUT_SERVER_PORT dutServerPort, WLM_DUT_OS dutOs)
{
#if !defined(TARGETOS_nucleus)
	/* close previous handle */
	if (irh != NULL) {
		wlmApiCleanup();
	}

	switch (ifType) {
		case WLM_DUT_LOCAL:
			rwl_set_remote_type(NO_REMOTE);
			break;
		case WLM_DUT_SERIAL:
			rwl_set_remote_type(REMOTE_SERIAL);
			break;
		case WLM_DUT_SOCKET:
			rwl_set_remote_type(REMOTE_SOCKET);
			break;
		case WLM_DUT_WIFI:
			rwl_set_remote_type(REMOTE_WIFI);
			break;
		case WLM_DUT_DONGLE:
			rwl_set_remote_type(REMOTE_DONGLE);
			break;
		default:
			/* ERROR! Unknown interface! */
			return FALSE;
	}

	if (ifName) {
		strncpy(interfaceName, ifName, MAX_INTERFACE_NAME_LENGTH);
		interfaceName[MAX_INTERFACE_NAME_LENGTH] = 0;
	}

	switch (dutOs) {
		case WLM_DUT_OS_LINUX:
			wl_os_type_set_rwl(LINUX_OS);
			break;
		default:
			/* ERROR! Unknown OS! */
			return FALSE;
	}

	switch (rwl_get_remote_type()) {
		struct ipv4_addr temp;
		case REMOTE_SOCKET:
			if (!wl_atoip(interfaceName, &temp)) {
				printf("wlmSelectInterface: IP address invalid\n");
				return FALSE;
			}
			rwl_set_server_ip(interfaceName);
			rwl_set_server_port(dutServerPort);
			rwl_init_socket();
			break;
		case REMOTE_SERIAL:
			rwl_set_serial_port_name(interfaceName); /* x (port number) or /dev/ttySx */
			if ((irh = rwl_open_pipe(rwl_get_remote_type(),
				rwl_get_serial_port_name(), 0, 0)) == NULL) {
				printf("wlmSelectInterface: rwl_open_pipe failed\n");
				return FALSE;
			}
			break;
		case REMOTE_DONGLE:
			rwl_set_serial_port_name(interfaceName); /* COMx or /dev/ttySx */
			if ((irh = rwl_open_pipe(rwl_get_remote_type(), "\0", 0, 0)) == NULL) {
				printf("wlmSelectInterface: rwl_open_pipe failed\n");
				return FALSE;
			}
			break;
		case REMOTE_WIFI:
			if (!wl_ether_atoe(interfaceName,
				(struct ether_addr *)rwl_get_wifi_mac())) {
				printf("wlmSelectInterface: ethernet MAC address invalid\n");
				return FALSE;
			}
			/* intentionally no break here to pass through to NO_REMOTE case */
		case NO_REMOTE:
			break;
		default:
			/* ERROR! Invalid interface!
			 * NOTE: API should not allow code to come here.
			 */
			return FALSE;
	}
#endif /* !defined(TARGETOS_nucleus) */

	/* Query the IOCTL API version */
	/* The following code would fail before firmware has been downloaded */
	/*
	if (wlu_get(irh, WLC_GET_VERSION, &val, sizeof(int)) < 0) {
		printf("wlmSelectInterface: IOCTL Version query failed\n");
		return FALSE;
	}

	ioctl_version = dtoh32(val);
	if (ioctl_version != WLC_IOCTL_VERSION &&
	    ioctl_version != 1) {
		printf("wlmSelectInterface: Version mismatch, please upgrade."
		       "Got %d, expected %d or 1\n",
		        ioctl_version, WLC_IOCTL_VERSION);
		return FALSE;
	}
	*/

	return TRUE;
}

int wlmCreateSecondaryInterface(char *ifType, char *ifTypeArgs)
{
	int ret = BCME_ERROR;
	int i = 0;
	cmd_t command = { "interface_create", wl_interface_create_action, -1, WLC_GET_VAR, "" };
	char *args[WLM_NUM_ARGS] = {0};
	char *pch;

	if (ifType == NULL) {
		printf("wlmCreateSecondaryInterface: Usage error\n");
		return FALSE;
	}
	else {
		/* Copy the interface type and interface flags to args */
		args[i++] = "interface_create";
		CALLOC_ARG_MEM(args[i], WLM_MAX_ARG_LEN)
		snprintf(args[i++], WLM_MAX_ARG_LEN, "%s", ifType);
		if (ifTypeArgs != NULL) {
		/* Arguments are separated by space */
			pch = strtok(ifTypeArgs, " ");
			while (pch != NULL && i < WLM_NUM_ARGS) {
				CALLOC_ARG_MEM(args[i], WLM_MAX_ARG_LEN)
				snprintf(args[i], WLM_MAX_ARG_LEN, "%s", pch);
				pch = strtok(NULL, " ");
				i++;
			}
		}
	}

	/* CreateSecondaryInterface through wlu fucntion */
	ret = wl_interface_create_action(irh, &command, args);

	if (ret != BCME_OK) {
		printf("wlmCreateSecondaryInterface: failed with ret = %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	i = 1;
	while (args[i] != NULL) {
		free(args[i]);
		i++;
	}

	return ret;
}

int wlmSelectIntfidx(char *opt, char *idx)
{
	if (opt && !idx) {
		printf("error: expected wlc integer index after option %s\n", opt);
		return FALSE;
	} else if (!strcmp(opt, "-a") || !strcmp(opt, "-i")) {
		strncpy(g_rem_ifname, idx, IFNAMSIZ);
	} else {
		strncpy(g_rem_ifname, "", IFNAMSIZ);
	}
	return TRUE;
}

int wlmSelectInterfaceAndBaud(WLM_DUT_INTERFACE ifType, char *ifName,
	WLM_DUT_SERVER_PORT dutServerPort, WLM_DUT_OS dutOs, int baud)
{
	g_rwl_device_baud = baud;
	return wlmSelectInterface(ifType, ifName, dutServerPort, dutOs);
}

int wlmVersionGet(char *buf, int len)
{
	char fwVersionStr[256];
	int n = 0;

	if (buf == 0) {
		printf("wlmVersionGet: buffer invalid\n");
		return FALSE;
	}

	memset(buf, 0, sizeof(*buf));

	n += sprintf(buf, "wlm: ");
	n += sprintf(buf + n, "%s",
	     ver2str(((EPI_MAJOR_VERSION) << 16) |
		EPI_MINOR_VERSION, (EPI_RC_NUMBER << 16) |
		EPI_INCREMENTAL_NUMBER));
	n += sprintf(buf + n, " ");

	/* query for 'ver' to get version info */
	if (wlu_iovar_get(irh, "ver", fwVersionStr,
		(len < WLC_IOCTL_SMLEN) ? len : WLC_IOCTL_SMLEN)) {
		printf("wlmVersionGet: %s\n", wlmLastError());
		return FALSE;
	}

	n += sprintf(buf + n, "%s", fwVersionStr);
	return TRUE;
}

int wlmEnableAdapterUp(int enable)
{
	/*  Enable/disable adapter  */
	if (enable)
	{
		if (wlu_set(irh, WLC_UP, NULL, 0)) {
			printf("wlmEnableAdapterUp: %s\n", wlmLastError());
			return FALSE;
		}
	}
	else {
		if (wlu_set(irh, WLC_DOWN, NULL, 0)) {
			printf("wlmEnableAdapterUp: %s\n", wlmLastError());
			return FALSE;
		}
	}

	return TRUE;
}

int wlmIsAdapterUp(int *up)
{
	/*  Get 'isup' - check if adapter is up */
	up = (int *)dtoh32((uint32)up);
	if (wlu_get(irh, WLC_GET_UP, up, sizeof(int))) {
		printf("wlmIsAdapterUp: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmMinPowerConsumption(int enable)
{
	if (wlu_iovar_setint(irh, "mpc", enable)) {
		printf("wlmMinPowerConsumption: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmMimoPreambleGet(int* type)
{
	if (wlu_iovar_getint(irh, "mimo_preamble", type)) {
		printf("wlmMimoPreambleGet(): %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmMimoPreambleSet(int type)
{
	if (wlu_iovar_setint(irh, "mimo_preamble", type)) {
		printf("wlmMimoPreambleSet(): %s\n", wlmLastError());
		return FALSE;
	}
	return  TRUE;
}

int wlmChannelSet(int channel)
{

	/* Check band lock first before set  channel */
	if ((channel <= 14) && (curBand != WLM_BAND_2G)) {
		curBand = WLM_BAND_2G;
	} else if ((channel > 14) && (curBand != WLM_BAND_5G)) {
		curBand = WLM_BAND_5G;
	}

	/* Set 'channel' */
	channel = htod32(channel);
	if (wlu_set(irh, WLC_SET_CHANNEL, &channel, sizeof(channel))) {
		printf("wlmChannelSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRateSet(WLM_RATE rate)
{
	char aname[] = "a_rate";
	char bgname[] = "bg_rate";
	char *name;

	switch (curBand) {
	        case WLM_BAND_AUTO :
			printf("wlmRateSet: must set channel or band lock first\n");
			return FALSE;
	        case WLM_BAND_DUAL :
			printf("wlmRateSet: must set channel or band lock first\n");
			return FALSE;
		case WLM_BAND_5G :
			name = (char *)aname;
			break;
		case WLM_BAND_2G :
			name = (char *)bgname;
			break;
		default :
			return FALSE;
	}

	rate = htod32(rate);
	if (wlu_iovar_setint(irh, name, rate)) {
		printf("wlmRateSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmLegacyRateSet(WLM_RATE rate)
{
	uint32 nrate = 0;

	if (ioctl_version == 1) {
		nrate |= (rate & OLD_NRATE_RATE_MASK);
		nrate |= (OLD_NRATE_STF_SISO << OLD_NRATE_STF_SHIFT) & OLD_NRATE_STF_MASK;
	} else {
		nrate = WL_RSPEC_ENCODE_RATE;	/* 11abg */
		nrate |= (rate & WL_RSPEC_RATE_MASK);
	}

	if (wlu_iovar_setint(irh, "nrate", (int)nrate)) {
		printf("wlmLegacyRateSet: %s\n", wlmLastError());
		return FALSE;
	}

	return  TRUE;
}

int wlmMcsRateSet(WLM_MCS_RATE mcs_rate, WLM_STF_MODE stf_mode)
{
	uint32 nrate = 0;
	uint stf;

	if (mcs_rate > 32) {
		printf("wlmMcsRateSet: MCS %d out of range\n", mcs_rate);
		return FALSE;
	}

	if (ioctl_version == 1) {
		nrate |= mcs_rate;
		nrate |= OLD_NRATE_MCS_INUSE;

		if (!stf_mode) {
			stf = 0;
			if (mcs_rate <= HIGHEST_SINGLE_STREAM_MCS ||
			    mcs_rate == 32)
				stf = OLD_NRATE_STF_SISO;	/* SISO */
			else
				stf = OLD_NRATE_STF_SDM;	/* SDM */
		} else
			stf = stf_mode;

		nrate |= (stf << OLD_NRATE_STF_SHIFT) & OLD_NRATE_STF_MASK;
	} else {
		nrate = WL_RSPEC_ENCODE_HT;	/* 11n HT */
		nrate |= mcs_rate;

		/* decode WLM stf value into tx expansion and STBC */
		if (stf_mode == WLM_STF_MODE_CDD) {
			nrate |= (1 << WL_RSPEC_TXEXP_SHIFT);
		} else if (stf_mode == WLM_STF_MODE_STBC) {
			nrate |= WL_RSPEC_STBC;
		}
	}

	if (wlu_iovar_setint(irh, "nrate", (int)nrate)) {
		printf("wlmMcsRateSet: %s\n", wlmLastError());
		return FALSE;
	}
	return  TRUE;
}

int wlmHTRateSet(WLM_MCS_RATE mcs_rate, int stbc, int ldpc, int sgi, int tx_exp, int bw)
{
	uint32 rspec = 0;
	char iov_2g[] = "2g_rate";
	char iov_5g[] = "5g_rate";
	char *name;

	switch (curBand) {
	        case WLM_BAND_AUTO :
			printf("wlmHTRateSet: must set channel or band lock first \n");
			return FALSE;
	        case WLM_BAND_DUAL :
			printf("wlmHTRateSet: must set channel or band lock first\n");
			return FALSE;
		case WLM_BAND_5G :
			name = (char *)iov_5g;
			break;
		case WLM_BAND_2G :
			name = (char *)iov_2g;
			break;
		default :
			printf("wlmHTRateSet: band setting unknown\n");
			return FALSE;
	}

	/* set rate to auto ??? */
	if (mcs_rate < 0)
		rspec = 0;

	/* 11n HT rate */
	rspec = WL_RSPEC_ENCODE_HT;
	rspec |= mcs_rate;

	/* set bandwidth */
	if (bw == 20) {
		bw = WL_RSPEC_BW_20MHZ;
	} else if (bw == 40) {
		bw = WL_RSPEC_BW_40MHZ;
	} else if (bw == 80) {
		bw = WL_RSPEC_BW_80MHZ;
	} else if (bw == 160) {
		bw = WL_RSPEC_BW_160MHZ;
	} else {
		printf("wlmHTRateSet: unexpected bandwidth specified \"%d\", "
		       "expected 20, 40, 80, or 160\n", bw);
		return FALSE;
	}

	if (tx_exp < 0 || tx_exp > 3) {
		printf("wlmHTRataSet: tx expansion %d out of range [0-3]\n", tx_exp);
		return FALSE;
	}

	/* set the other rspec fields */
	rspec |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	rspec |= bw;
	rspec |= (stbc ? WL_RSPEC_STBC : 0);
	rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
	rspec |= (sgi  ? WL_RSPEC_SGI  : 0);

	if (wlu_iovar_setint(irh, name, rspec)) {
		printf("wlmHTRateSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmVHTRateSet(WLM_MCS_RATE mcs_rate, int nss, int stbc, int ldpc, int sgi, int tx_exp, int bw)
{
	uint32 rspec = 0;
	char iov_2g[] = "2g_rate";
	char iov_5g[] = "5g_rate";
	char *name;

	switch (curBand) {
	        case WLM_BAND_AUTO :
			printf("wlmVHTRateSet: must set channel or band lock first \n");
			return FALSE;
	        case WLM_BAND_DUAL :
			printf("wlmVHTRateSet: must set channel or band lock first\n");
			return FALSE;
		case WLM_BAND_5G :
			name = (char *)iov_5g;
			break;
		case WLM_BAND_2G :
			name = (char *)iov_2g;
			break;
		default :
			printf("wlmVHTRateSet: band setting unknown\n");
			return FALSE;
	}

	/* set rate to auto */
	if (mcs_rate < 0)
		rspec = 0;

	/* 11n VHT rate */
	rspec = WL_RSPEC_ENCODE_VHT;

	/* default NSS = 1 */
	if (nss == 0)
		nss = 1;

	rspec |= (nss << WL_RSPEC_VHT_NSS_SHIFT) | mcs_rate;

	/* set bandwidth */
	if (bw == 20) {
		bw = WL_RSPEC_BW_20MHZ;
	} else if (bw == 40) {
		bw = WL_RSPEC_BW_40MHZ;
	} else if (bw == 80) {
		bw = WL_RSPEC_BW_80MHZ;
	} else if (bw == 160) {
		bw = WL_RSPEC_BW_160MHZ;
	} else {
		printf("wlmVHTRateSet: unexpected bandwidth specified \"%d\", "
		       "expected 20, 40, 80, or 160\n", bw);
		return FALSE;
	}

	if (tx_exp < 0 || tx_exp > 3) {
		printf("wlmVHTRataSet: tx expansion %d out of range [0-3]\n", tx_exp);
		return FALSE;
	}

	/* set the other rspec fields */
	rspec |= tx_exp << WL_RSPEC_TXEXP_SHIFT;
	rspec |= bw;
	rspec |= (stbc ? WL_RSPEC_STBC : 0);
	rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
	rspec |= (sgi  ? WL_RSPEC_SGI  : 0);

	if (wlu_iovar_setint(irh, name, rspec)) {
		printf("wlmVHTRateSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPreambleSet(WLM_PREAMBLE preamble)
{
	preamble = htod32(preamble);

	if (wlu_set(irh, WLC_SET_PLCPHDR, &preamble, sizeof(preamble))) {
		printf("wlmPreambleSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmBandSet(WLM_BAND band)
{
	band = htod32(band);

	if (wlu_set(irh, WLC_SET_BAND, (void *)&band, sizeof(band))) {
		printf("wlmBandSet: %s\n", wlmLastError());
		return FALSE;
	}

	curBand = band;

	return TRUE;
}

int wlmBandGet(WLM_BAND *band)
{
	if (wlu_get(irh, WLC_GET_BAND, band, sizeof(band))) {
		printf("wlmBandGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* set curBand to make sure it matches the current band status */
	curBand = *band;
	return TRUE;
}

int wlmGetBandList(WLM_BAND * bands)
{
	unsigned int list[3];
	unsigned int i;

	if (wlu_get(irh, WLC_GET_BANDLIST, list, sizeof(list))) {
		printf("wlmGetBandList: %s\n", wlmLastError());
		return FALSE;
	}

	list[0] = dtoh32(list[0]);
	list[1] = dtoh32(list[1]);
	list[2] = dtoh32(list[2]);

	/* list[0] is count, followed by 'count' bands */

	if (list[0] > 2)
		list[0] = 2;

	for (i = 1, *bands = (WLM_BAND)0; i <= list[0]; i++)
		*bands |= list[i];

	return TRUE;
}

int wlmGmodeSet(WLM_GMODE gmode)
{
	/*  Set 'gmode' - select mode in 2.4G band */
	gmode = htod32(gmode);

	if (wlu_set(irh, WLC_SET_GMODE, (void *)&gmode, sizeof(gmode))) {
		printf("wlmGmodeSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRxAntSet(int antenna)
{
	/*  Set 'antdiv' - select receive antenna */
	antenna = htod32(antenna);

	if (wlu_set(irh, WLC_SET_ANTDIV, &antenna, sizeof(antenna))) {
		printf("wlmRxAntSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTxAntSet(int antenna)
{
	/*  Set 'txant' - select transmit antenna */
	antenna = htod32(antenna);

	if (wlu_set(irh, WLC_SET_TXANT, &antenna, sizeof(antenna))) {
		printf("wlmTxAntSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmEstimatedPowerGet(int *estPower, int chain)
{
	int mimo;
	int is2g;

	size_t pprsize = ppr_ser_size_by_bw(ppr_get_max_bw());
	size_t ppr_rpt_size = sizeof(tx_pwr_rpt_t) + pprsize*WL_TXPPR_SER_BUF_NUM;
	/* Allocate memory for structure and WL_TXPPR_SER_BUF_NUM (3) serialisation buffer */
	tx_pwr_rpt_t *ppr_wl = (tx_pwr_rpt_t *)CALLOC(ppr_rpt_size);
	if (ppr_wl == NULL)
		return FALSE;

	ppr_wl->ppr_len  = pprsize;
	ppr_wl->version  = TX_POWER_T_VERSION;
	/* No need to init ppr buffer since it is not used here */
	if (wlu_get(irh, WLC_CURRENT_PWR, ppr_wl, ppr_rpt_size) < 0) {
		printf("wlmEstimatedPowerGet: %s\n", wlmLastError());
		free(ppr_wl);
		return FALSE;
	}

	ppr_wl->flags = dtoh32(ppr_wl->flags);
	ppr_wl->chanspec = dtohchanspec(ppr_wl->chanspec);
	mimo = (ppr_wl->flags & WL_TX_POWER_F_MIMO);

	/* value returned is in units of quarter dBm, need to multiply by 250 to get milli-dBm */
	if (mimo) {
		*estPower = ppr_wl->est_Pout[chain] * 250;
	} else {
		*estPower = ppr_wl->est_Pout[0] * 250;
	}

	if (ioctl_version == 1) {
		is2g = LCHSPEC_IS2G(ppr_wl->chanspec);
	} else {
		is2g = CHSPEC_IS2G(ppr_wl->chanspec);
	}

	if (!mimo && is2g) {
		*estPower = ppr_wl->est_Pout_cck * 250;
	}

	free(ppr_wl);
	return TRUE;
}

int wlmTxPowerGet(int *power)
{
	int val;
	int8 temp_val;

	if ((wlu_iovar_getint(irh, "qtxpower", &val)) < 0) {
		printf("wlmTxPowerGet: %s\n", wlmLastError());
		return FALSE;
	}

	val &= ~WL_TXPWR_OVERRIDE;
	temp_val = (int8)(val & 0xff);

	/* value returned is in units of quarter dBm, need to multiply by 250 to get milli-dBm */
	*power = temp_val * 250;
	return TRUE;
}

int wlmTxPowerSet(int powerValue)
{
	int newValue = 0;
	int8 temp_val;

	if (powerValue == -1) {
		newValue = WLC_TXPWR_MAX ; 	/* Max val of 127 qdbm */
	} else {
		/* expected input param to be in units of milli dBm */
		/* convert to milli quater dBm */
		newValue = powerValue / 250;

		if (newValue > WLC_TXPWR_MAX) {
			printf("wlmTxPowerSet: %d is invalid target power,"
			       "should be within [-128, 127] qdBm.\n", newValue);
			return FALSE;
		}

		if (newValue < WL_RATE_DISABLED) {
			newValue = WL_RATE_DISABLED;
		}
		temp_val = newValue;
		newValue = (uint8) temp_val;
		newValue |= WL_TXPWR_OVERRIDE;
	}

	if (wlu_iovar_setint(irh, "qtxpower", newValue)) {
		printf("wlmTxPowerSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}
static int wlmPhyTypeGet(void)
{

	int phytype = PHY_TYPE_NULL;

	if (wlu_get(irh, WLC_GET_PHYTYPE, &phytype, sizeof(int)) < 0) {
	        printf("wlmPhyTypeGet: %s\n", wlmLastError());
		return FALSE;
	}

	return phytype;
}

int wlmPaParametersGet(WLM_BANDRANGE bandrange,
	unsigned int *a1, unsigned int *b0, unsigned int *b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN];
	void *ptr = NULL;
	uint16 *outpa;
	int i = 0;
	int phytype = PHY_TYPE_NULL;

	*a1 = 0;
	*b0 = 0;
	*b1 = 0;

	/* Do not rely on user to have knowledge of phytype */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_NULL) {
		inpa[i++] = phytype;
		inpa[i++] = bandrange;
		inpa[i++] = 0;  /* Fix me: default with chain 0 for all SISO system */
	} else {
		printf("wlmPaParametersGet: unknown Phy type\n");
		return FALSE;
	}

	if (wlu_var_getbuf_sm(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) {
		printf("wlmPaParametersGet: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *)ptr;
	*b0 = outpa[i++];
	*b1 = outpa[i++];
	*a1 = outpa[i++];

	return TRUE;
}

int wlmPaParametersSet(WLM_BANDRANGE bandrange,
	unsigned int a1, unsigned int b0, unsigned int b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN];
	int i = 0;
	int phytype = PHY_TYPE_NULL;

	/* Do not rely on user to have knowledge of phy type */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_NULL) {
		inpa[i++] = phytype;
		inpa[i++] = bandrange;
		inpa[i++] = 0;  /* Fix me: default with chain 0 for all SISO system */
	} else {
	        printf("wlmPaParametersSet: unknown Phy type\n");
		return FALSE;
	}

	inpa[i++] = b0;
	inpa[i++] = b1;
	inpa[i++] = a1;

	if (wlu_var_setbuf(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16))) {
		printf("wlmPaParametersSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmMIMOPaParametersGet(WLM_BANDRANGE bandrange, int chain,
	unsigned int *a1, unsigned int *b0, unsigned int *b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN];
	void *ptr = NULL;
	uint16 *outpa;
	int i = 0;
	int phytype = PHY_TYPE_NULL;

	/* Do not rely on user to have knowledge of phytype */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_NULL) {
		inpa[i++] = phytype;
		inpa[i++] = bandrange;
		inpa[i++] = chain;
	} else {
		printf("wlmMIMOPaParametersGet: unknown Phy type\n");
		return FALSE;
	}

	if (wlu_var_getbuf_sm(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) {
		printf("wlmMIMOPaParametersGet: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *)ptr;
	*a1 = outpa[i++];
	*b0 = outpa[i++];
	*b1 = outpa[i++];

	return TRUE;
}

int wlmMIMOPaParametersSet(WLM_BANDRANGE bandrange, int chain,
	unsigned int a1, unsigned int b0, unsigned int b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN];
	int i = 0;
	int phytype = PHY_TYPE_NULL;

	/* Do not rely on user to have knowledge of phy type */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_NULL) {
	        inpa[i++] = phytype;
		inpa[i++] = bandrange;
		inpa[i++] = chain;
	} else {
	        printf("wlmMIMOPaParametersSet: unknown Phy type\n");
		return FALSE;
	}

	inpa[i++] = a1;
	inpa[i++] = b0;
	inpa[i++] = b1;

	if (wlu_var_setbuf(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16))) {
		printf("wlmMIMOPaParametersSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmACPaParametersGet(WLM_BANDRANGE bandrange, int modetype,
	unsigned int *a1, unsigned int *b0, unsigned int *b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN] = {0};
	void *ptr = NULL;
	uint16 *outpa;
	int i = 0;
	int phytype = PHY_TYPE_NULL;

	/* Do not rely on user to have knowledge of phytype */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersGet: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if (bandrange == WL_CHAN_FREQ_RANGE_2G) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G;
	} else if ((bandrange > WL_CHAN_FREQ_RANGE_2G) &&
		(bandrange < WL_CHAN_FREQ_RANGE_5G_4BAND)) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_4BAND;
	} else {
		printf("wlmACPaParametersGet: Wrong bandrange [0-4]\n");
		return FALSE;
	}

	if (modetype > 2) {
		printf("wlmACPaParametersGet: Wrong modetype number [0-2]\n");
		return FALSE;
	}
	inpa[i++] = modetype;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersGet: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *)ptr;

	if (bandrange > 0)
		i += (bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * 3;
	/*
	  printf("0x%x\n", outpa[i++]);
	  printf("0x%x\n", outpa[i++]);
	  printf("0x%x\n", outpa[i++]);
	*/
	 *a1 = outpa[i++];
	 *b0 = outpa[i++];
	 *b1 = outpa[i++];

	return TRUE;
}

int wlmACPaParametersSet(WLM_BANDRANGE bandrange, int modetype,
	unsigned int a1, unsigned int b0, unsigned int b1)
{
	uint16 inpa[WL_PHY_PAVARS_LEN] = {0};
	int i = 0;
	int phytype = PHY_TYPE_NULL;
	void *ptr = NULL;
	uint16 *outpa;
	int pnum = 0, n = 0;

	/* Do not rely on user to have knowledge of phy type */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersSet: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if (bandrange == WL_CHAN_FREQ_RANGE_2G) {
		pnum = 3;
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G;
	} else if ((bandrange > WL_CHAN_FREQ_RANGE_2G) &&
		(bandrange < WL_CHAN_FREQ_RANGE_5G_4BAND)) {
		pnum = 12;
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_4BAND;
	} else {
		printf("wlmACPaParametersSet: Wrong bandrange [0-4]\n");
		return FALSE;
	}

	if (modetype > 2) {
		printf("wlmACPaParametersSet: Wrong modetype number [0-2]\n");
		return FALSE;
	}
	inpa[i++] = modetype;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersSet: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *) ptr;

	for (n = 0; n < pnum + 3; n++) {
		if ((bandrange > 0) && (n == 3 +((bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * 3))) {
			inpa[n++] = a1;
			/* printf("n: %d 0x%hx 0x%hx\n", n -1, inpa[n - 1], outpa[n - 1]); */
			inpa[n++] = b0;
			/* printf("n: %d 0x%hx 0x%hx\n", n - 1, inpa[n -1], outpa[n - 1]); */
			inpa[n] = b1;
			/* printf("n: %d 0x%hx 0x%hx\n", n, inpa[n], outpa[n]); */
		} else if ((bandrange == WL_CHAN_FREQ_RANGE_2G) && (n > 2)) {
			inpa[n++] = a1;
			/* printf("n: %d 0x%hx 0x%hx\n", n - 1, inpa[n -1], outpa[n - 1]); */
			inpa[n++] = b0;
			/* printf("n: %d 0x%hx 0x%hx\n", n - 1, inpa[n - 1], outpa[n - 1]); */
			inpa[n] = b1;
			/* printf("n: %d 0x%hx 0x%hx\n", n, inpa[n], outpa[n]); */
		} else {
			inpa[n] = outpa[n];
			/* printf("n: %d 0x%hx 0x%hx\n", n, inpa[n], outpa[n]); */
		}
	}

	if (wlu_var_setbuf(irh, "pavars", inpa, WL_PHY_PAVARS_LEN * sizeof(uint16))) {
		printf("wlmACPaParametersSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

#define WL_PHY_PAVARS_REV12_LEN          48
#define WL_CHAN_FREQ_RANGE_5G_5BAND      18
int wlmACPaParametersRev12Get(WLM_BANDRANGE bandrange, int chain, WLM_BANDWIDTH bandwidth,
	unsigned int *b0, unsigned int *b1, unsigned int *b2, unsigned int *b3)
{
	uint16 inpa[WL_PHY_PAVARS_REV12_LEN] = {0};
	void *ptr = NULL;
	uint16 *outpa;
	int i = 0;
	int pnum = 4;   /* number of parameters per band */
	int totalbands =  1 + 5; /* 1 2g band and 5 5g sub bands */
	int phytype = PHY_TYPE_NULL;
	int sromrev = 0;

	if (wlu_iovar_getint(irh, "sromrev", &sromrev) < 0) {
		printf("wlmACPaParametersSRRev12Get: Cannot find any srom version\n");
		return FALSE;
	}

	if (!(sromrev == 12 || sromrev == 13 || sromrev == 14 || sromrev == 15)) {
		printf("wlmACPaParametersSRRev12Get: Srom revision is not supported\n");
		return FALSE;
	}

	/* Do not rely on user to have knowledge of phytype */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersSRRev12Get: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if ((bandrange == WLM_CHAN_FREQ_RANGE_2G) && (bandwidth < WLM_BANDWIDTH_80MHZ)) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G  + totalbands * bandwidth;
	} else {
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_5BAND + bandwidth;
	}

	if (bandwidth > WLM_BANDWIDTH_80MHZ) {
		printf("wlmACPaParametersSRRev12Get: Wrong bandwidth\n");
		return FALSE;
	}
	inpa[i++] = chain;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa,
		WL_PHY_PAVARS_REV12_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersRev12Get: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *)ptr;

	if (bandrange > WL_CHAN_FREQ_RANGE_2G)
		i += (bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * pnum;

	*b0 = outpa[i++];
	*b1 = outpa[i++];
	*b2 = outpa[i++];
	*b3 = outpa[i++];

	return TRUE;
}

int wlmACPaParametersRev12Set(WLM_BANDRANGE bandrange, int chain, WLM_BANDWIDTH bandwidth,
	unsigned int b0, unsigned int b1, unsigned int b2, unsigned int b3)
{
	uint16 inpa[WL_PHY_PAVARS_REV12_LEN] = {0};
	int i = 0;
	int n = 0;
	int phytype = PHY_TYPE_NULL;
	void *ptr = NULL;
	uint16 *outpa;
	int pnum = 4;   /* number of parameters per band */
	int totalbands = 6; /* 1 2g band and 5 5g sub bands */
	int sromrev = 0;

	if (wlu_iovar_getint(irh, "sromrev", &sromrev) < 0) {
		printf("wlmACPaParametersSRRev12Get: Cannot find any srom version\n");
		return FALSE;
	}

	if (!(sromrev == 12 || sromrev == 13 || sromrev == 14 || sromrev == 15)) {
		printf("wlmACPaParametersSRRev12Get: Srom revision is not supported\n");
		return FALSE;
	}

	/* Do not rely on user to have knowledge of phy type */
	phytype = wlmPhyTypeGet();
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersRev12Set: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if ((bandrange == WLM_CHAN_FREQ_RANGE_2G) && (bandwidth < WLM_BANDWIDTH_80MHZ)) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G  + totalbands * bandwidth;
	} else {
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_5BAND + bandwidth;
	}

	if (bandwidth > WLM_BANDWIDTH_80MHZ) {
		printf("wlmACPaParametersSRRev12Get: Wrong bandwidth\n");
		return FALSE;
	}
	inpa[i++] = chain;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa,
		WL_PHY_PAVARS_REV12_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersRev12Set: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *) ptr;

	for (n = 0; n < pnum * totalbands; n++)
		inpa[n] = outpa[n];

	if (bandrange > WL_CHAN_FREQ_RANGE_2G)
		i += (bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * pnum;

	inpa[i++] = b0;
	inpa[i++] = b1;
	inpa[i++] = b2;
	inpa[i++] = b3;

	if (wlu_var_setbuf(irh, "pavars", inpa, WL_PHY_PAVARS_REV12_LEN * sizeof(uint16))) {
		printf("wlmACPaParametersRev12Set: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

#define WL_PHY_PAVARS_REV17_LEN          40
#define WL_CHAN_FREQ_RANGE_5G_4BAND      5
int wlmACPaParametersRev17Get(WLM_BANDRANGE bandrange, int chain, WLM_BANDWIDTH bandwidth,
	unsigned int *b0, unsigned int *b1, unsigned int *b2, unsigned int *b3)
{
	uint16 inpa[WL_PHY_PAVARS_REV17_LEN] = {0};
	void *ptr = NULL;
	uint16 *outpa;
	int i = 0;
	int pnum = 4;   /* number of parameters per band */
	int totalbands = 5; /* 1 2g band and 4 5g sub bands */
	int phytype;
	int sromrev = 0;

	if (wlu_iovar_getint(irh, "sromrev", &sromrev) < 0) {
		printf("wlmACPaParametersRev17Get: Cannot find any srom version\n");
		return FALSE;
	}

	if (!(sromrev == 17)) {
		printf("wlmACPaParametersRev17Get: Srom revision is not supported\n");
		return FALSE;
	}

	/* Do not rely on user to have knowledge of phytype */
	phytype = (curPhyType == PHY_TYPE_NULL) ? wlmPhyTypeGet() : curPhyType;
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersRev17Get: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if ((bandrange == WLM_CHAN_FREQ_RANGE_2G) && (bandwidth <= WLM_BANDWIDTH_40MHZ)) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G  + totalbands * bandwidth;
	} else {
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_4BAND + bandwidth;
	}

	if (bandwidth > WLM_BANDWIDTH_80MHZ) {
		printf("wlmACPaParametersRev17Get: Wrong bandwidth\n");
		return FALSE;
	}
	inpa[i++] = chain;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa,
		WL_PHY_PAVARS_REV17_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersRev17Get: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *)ptr;

	if (bandrange > WL_CHAN_FREQ_RANGE_2G)
		i += (bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * pnum;

	*b0 = outpa[i++];
	*b1 = outpa[i++];
	*b2 = outpa[i++];
	*b3 = outpa[i++];

	return TRUE;
}

int wlmACPaParametersRev17Set(WLM_BANDRANGE bandrange, int chain, WLM_BANDWIDTH bandwidth,
	unsigned int b0, unsigned int b1, unsigned int b2, unsigned int b3)
{
	uint16 inpa[WL_PHY_PAVARS_REV17_LEN] = {0};
	int i = 0;
	int n = 0;
	int phytype;
	void *ptr = NULL;
	uint16 *outpa;
	int pnum = 4;   /* number of parameters per band */
	int totalbands = 5; /* 1 2g band and 4 5g sub bands */
	int sromrev = 0;

	if (wlu_iovar_getint(irh, "sromrev", &sromrev) < 0) {
		printf("wlmACPaParametersRev17Get: Cannot find any srom version\n");
		return FALSE;
	}

	if (!(sromrev == 17)) {
		printf("wlmACPaParametersRev17Get: Srom revision is not supported\n");
		return FALSE;
	}

	/* Do not rely on user to have knowledge of phy type */
	phytype = (curPhyType == PHY_TYPE_NULL) ? wlmPhyTypeGet() : curPhyType;
	if (phytype != PHY_TYPE_AC) {
		printf("wlmACPaParametersRev17Set: Wrong PHY type\n");
		return FALSE;
	}
	inpa[i++] = phytype;

	if ((bandrange == WLM_CHAN_FREQ_RANGE_2G) && (bandwidth <= WLM_BANDWIDTH_40MHZ)) {
		inpa[i++] = WL_CHAN_FREQ_RANGE_2G  + totalbands * bandwidth;
	} else {
		inpa[i++] = WL_CHAN_FREQ_RANGE_5G_4BAND + bandwidth;
	}

	if (bandwidth > WLM_BANDWIDTH_80MHZ) {
		printf("wlmACPaParametersRev17Get: Wrong bandwidth\n");
		return FALSE;
	}
	inpa[i++] = chain;

	if (wlu_var_getbuf_sm(irh, "pavars", inpa,
		WL_PHY_PAVARS_REV17_LEN * sizeof(uint16), &ptr)) {
		printf("wlmACPaParametersRev17Set: %s\n", wlmLastError());
		return FALSE;
	}

	outpa = (uint16 *) ptr;

	for (n = 0; n < pnum * totalbands; n++)
		inpa[n] = outpa[n];
	if (bandrange > WL_CHAN_FREQ_RANGE_2G)
		i += (bandrange - WL_CHAN_FREQ_RANGE_5G_BAND0) * pnum;

	inpa[i++] = b0;
	inpa[i++] = b1;
	inpa[i++] = b2;
	inpa[i++] = b3;

	if (wlu_var_setbuf(irh, "pavars", inpa, WL_PHY_PAVARS_REV17_LEN * sizeof(uint16))) {
		printf("wlmACPaParametersRev17Set: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmMacAddrGet(char *macAddr, int length)
{
	struct ether_addr ea = {{0, 0, 0, 0, 0, 0}};

	/* query for 'cur_etheraddr' to get MAC address */
	if (wlu_iovar_get(irh, "cur_etheraddr", &ea, ETHER_ADDR_LEN) < 0) {
		printf("wlmMacAddrGet: %s\n", wlmLastError());
		return FALSE;
	}

	strncpy(macAddr, wl_ether_etoa(&ea), length);

	return TRUE;
}

int wlmMacAddrSet(const char* macAddr)
{
	struct ether_addr ea;

	if (!wl_ether_atoe(macAddr, &ea)) {
		printf("wlmMacAddrSet: MAC address invalid: %s\n", macAddr);
		return FALSE;
	}

	/*  Set 'cur_etheraddr' to set MAC address */
	if (wlu_iovar_set(irh, "cur_etheraddr", (void *)&ea, ETHER_ADDR_LEN)) {
		printf("wlmMacAddrSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmEnableCarrierTone(int enable, int channel)
{
	int val = channel;

	if (!enable) {
		val = 0;
	}
	else {
		wlmEnableAdapterUp(1);
		if (wlu_set(irh, WLC_OUT, NULL, 0) < 0) {
		printf("wlmEnableCarrierTone: %s\n", wlmLastError());
		return FALSE;
		}
	}
	val = htod32(val);
	if (wlu_set(irh, WLC_FREQ_ACCURACY, &val, sizeof(int)) < 0) {
		printf("wlmEnableCarrierTone: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmEnableEVMTest(int enable, WLM_RATE rate, int channel)
{
	int val[3] = {0};
	val[1] = WLM_RATE_1M; /* default value */
	if (enable) {
		val[0] = htod32(channel);
		val[1] = htod32(rate);
		wlmEnableAdapterUp(1);
		if (wlu_set(irh, WLC_OUT, NULL, 0) < 0) {
			printf("wlmEnableEVMTest: %s\n", wlmLastError());
			return FALSE;
		}
	}
	if (wlu_set(irh, WLC_EVM, val, sizeof(val)) < 0) {
		printf("wlmEnableEVMTest: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmTxPacketStart(unsigned int interPacketDelay,
	unsigned int numPackets, unsigned int packetLength,
	const char* destMac, int withAck, int syncMode)
{
	wl_pkteng_t pkteng;

	if (!wl_ether_atoe(destMac, (struct ether_addr *)&pkteng.dest)) {
		printf("wlmTxPacketStart: destMac invalid\n");
		return FALSE;
	}

	pkteng.flags = withAck ? WL_PKTENG_PER_TX_WITH_ACK_START : WL_PKTENG_PER_TX_START;

	if (syncMode == 1) {
		pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
		pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS_UNBLK;
	}
	else if (syncMode == 2) {
		pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;
		pkteng.flags |= WL_PKTENG_SYNCHRONOUS_UNBLK;
	}
	else if (syncMode == 0) {
		pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;
		pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS_UNBLK;
	}
	else {
		printf("Valid values for syncMode are 0, 1, 2 only\n");
		return FALSE;
	}

	pkteng.delay = interPacketDelay;
	pkteng.length = packetLength;
	pkteng.nframes = numPackets;

	pkteng.seqno = 0;			/* not used */
	pkteng.src = ether_null;	/* implies current ether addr */

	if (wlu_var_setbuf(irh, "pkteng", &pkteng, sizeof(pkteng))) {
		printf("wlmTxPacketStart: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTxPacketStop(void)
{
	wl_pkteng_t pkteng;

	memset(&pkteng, 0, sizeof(pkteng));
	pkteng.flags = WL_PKTENG_PER_TX_STOP;

	if (wlu_var_setbuf(irh, "pkteng", &pkteng, sizeof(pkteng)) < 0) {
		printf("wlmTxPacketStop: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRxPacketStart(const char* srcMac, int withAck,
	int syncMode, unsigned int numPackets, unsigned int timeout)
{
	wl_pkteng_t pkteng;

	if (!wl_ether_atoe(srcMac, (struct ether_addr *)&pkteng.dest)) {
		printf("wlmRxPacketStart: srcMac invalid\n");
		return FALSE;
	}

	pkteng.flags = withAck ? WL_PKTENG_PER_RX_WITH_ACK_START : WL_PKTENG_PER_RX_START;

	if (syncMode) {
		pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
		pkteng.nframes = numPackets;
		pkteng.delay = timeout;
	}
	else
		pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;

	pkteng.length = 0;

	if (wlu_var_setbuf(irh, "pkteng", &pkteng, sizeof(pkteng))) {
		printf("wlmRxPacketStart: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRxPacketStop(void)
{
	wl_pkteng_t pkteng;

	memset(&pkteng, 0, sizeof(pkteng));
	pkteng.flags = WL_PKTENG_PER_RX_STOP;

	if (wlu_var_setbuf(irh, "pkteng", &pkteng, sizeof(pkteng)) < 0) {
		printf("wlmRxPacketStop: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

#define MCSTCNT_LE10_FROM_CNTBUF(cntbuf) (const wl_cnt_v_le10_mcst_t *) \
		bcm_get_data_from_xtlv_buf(((wl_cnt_info_t *)cntbuf)->data,	\
		((wl_cnt_info_t *)cntbuf)->datalen,			\
		WL_CNT_XTLV_CNTV_LE10_UCODE, NULL,			\
		BCM_XTLV_OPTION_ALIGN32)

#define MCSTCNT_GT10_FROM_CNTBUF(cntbuf) (const wl_cnt_v_le10_mcst_t *) \
		bcm_get_data_from_xtlv_buf(((wl_cnt_info_t *)cntbuf)->data,	\
		((wl_cnt_info_t *)cntbuf)->datalen,			\
		WL_CNT_XTLV_LT40_UCODE_V1, NULL,			\
		BCM_XTLV_OPTION_ALIGN32)

#define MCSTCNT_GE40_FROM_CNTBUF(cntbuf) (const wl_cnt_ge40mcst_v1_t *) \
		bcm_get_data_from_xtlv_buf(((wl_cnt_info_t *)cntbuf)->data,	\
		((wl_cnt_info_t *)cntbuf)->datalen,			\
		WL_CNT_XTLV_GE40_UCODE_V1, NULL,			\
		BCM_XTLV_OPTION_ALIGN32)

int wlmTxGetAckedPackets(unsigned int *count)
{
	void *cntbuf;
	/* XXX: rxackucast is at the same offset in wl_cnt_v_le10_mcst_t
	 * and wl_cnt_lt40mcst_v1_t.
	 */
	const wl_cnt_v_le10_mcst_t *macstat_cnt;
	const wl_cnt_ge40mcst_v1_t *macstat_cnt_ge40;
	int res;

	if (wlu_var_getbuf_med(irh, "counters", NULL, 0, &cntbuf)) {
		printf("wlmTxGetAckedPackets: %s\n", wlmLastError());
		return FALSE;
	}
	CHK_CNTBUF_DATALEN(cntbuf, WLC_IOCTL_MEDLEN);

	/* Translate traditional (ver <= 10) counters struct to new xtlv type struct */
	/* XXX: As we only need rxackucast which is at the same offset over all corerev,
	 * no need to input corerev.
	 */
	res = wl_cntbuf_to_xtlv_format(NULL, cntbuf, WLC_IOCTL_MEDLEN, 0);
	if (res != BCME_OK) {
		printf("wlmTxGetAckedPackets: wl_cntbuf_to_xtlv_format failed %d\n",
			res);
		return FALSE;
	}

	if ((macstat_cnt = MCSTCNT_LE10_FROM_CNTBUF(cntbuf)) == NULL) {
		if ((macstat_cnt = MCSTCNT_GT10_FROM_CNTBUF(cntbuf)) == NULL) {
			if ((macstat_cnt_ge40 = MCSTCNT_GE40_FROM_CNTBUF(cntbuf)) == NULL) {
				printf("wlmTxGetAckedPackets: macstat_cnt NULL!\n");
				return FALSE;
			} else {
				*count = dtoh32(macstat_cnt_ge40->rxackucast);
			}
		} else {
			*count = dtoh32(macstat_cnt->rxackucast);
		}
	} else {
		*count = dtoh32(macstat_cnt->rxackucast);
	}

	return TRUE;
}

int wlmRxGetReceivedPackets(unsigned int *count)
{
	void *cntbuf;
	wl_cnt_info_t *cntinfo;
	/* XXX: pktengrxducast and rxdfrmucastmbss (renamed to rxdtucastmbss)
	 * are at the same offset in wl_cnt_v_le10_mcst_t and wl_cnt_lt40mcst_v1_t.
	 */
	const wl_cnt_v_le10_mcst_t *macstat_cnt;
	const wl_cnt_ge40mcst_v1_t *macstat_cnt_ge40;
	int res;
	uint16 ver;

	if (wlu_var_getbuf_med(irh, "counters", NULL, 0, &cntbuf)) {
		printf("wlmRxGetReceivedPackets: %s\n", wlmLastError());
		return FALSE;
	}

	cntinfo = (wl_cnt_info_t *)cntbuf;
	cntinfo->version = dtoh16(cntinfo->version);
	cntinfo->datalen = dtoh16(cntinfo->datalen);
	CHK_CNTBUF_DATALEN(cntbuf, WLC_IOCTL_MEDLEN);
	ver = cntinfo->version;

	/* Translate traditional (ver <= 10) counters struct to new xtlv type struct */
	/* XXX: As we only need rxackucast which is at the same offset over all corerev,
	 * no need to input corerev.
	 */
	res = wl_cntbuf_to_xtlv_format(NULL, cntbuf, WLC_IOCTL_MEDLEN, 0);
	if (res != BCME_OK) {
		printf("wlmRxGetReceivedPackets: wl_cntbuf_to_xtlv_format failed %d\n",
			res);
		return FALSE;
	}

	if ((macstat_cnt = MCSTCNT_LE10_FROM_CNTBUF(cntbuf)) == NULL) {
		if ((macstat_cnt = MCSTCNT_GT10_FROM_CNTBUF(cntbuf)) == NULL) {
			if ((macstat_cnt_ge40 = MCSTCNT_GE40_FROM_CNTBUF(cntbuf)) == NULL) {
				printf("wlmRxGetReceivedPackets: macstat_cnt NULL!\n");
				return FALSE;
			} else {
				*count = dtoh32(macstat_cnt_ge40->pktengrxducast);
			}
		} else {
			/* Support it from version 7 */
			*count = (ver > WL_CNT_VERSION_6) ? dtoh32(macstat_cnt->pktengrxducast):
				dtoh32(macstat_cnt->rxdfrmucastmbss);
		}
	} else {
		/* Support it from version 7 */
		*count = (ver > WL_CNT_VERSION_6) ? dtoh32(macstat_cnt->pktengrxducast):
			dtoh32(macstat_cnt->rxdfrmucastmbss);
	}

	return TRUE;
}

static int
wlm_free_args_mem(char *args[])
{
	int index = 0;
	while (args[index] != NULL) {
		free(args[index]);
		index++;
	}
	return TRUE;
}

/* args array should be sufficient enough to hold max_args. The last argument populated
 * in args will always be null to represent end of arguments. So cmd_line should not
 * contain more than (max_args - 1) arguments.
 */
static int
wlm_subcntr_buf_to_args(char *cmd_line, char * args[], uint32 max_args)
{
	char *token;
	uint32 argc = 0;
	int retval = BCME_OK;

	if (max_args == 0)
		return BCME_BADARG;

	/* Arguments are separated by space */
	while ((token = strtok(argc ? NULL : cmd_line, " ")) != NULL) {
		if (argc >= max_args - 1) {
			printf("Max allowed args %d\n", max_args);
			retval = BCME_BADARG;
			args[max_args - 1] = NULL;
			goto cleanup;
		}
		args[argc] = CALLOC(strlen(token) + 1);
		if (args[argc] == NULL) {
			printf("Malloc Failed\n");
			retval = BCME_NOMEM;
			goto cleanup;
		}
		strncpy(args[argc], token, strlen(token) + 1);
		argc++;
	}
	args[argc] = NULL;
	return retval;
cleanup:
	/* Free all allocated memory in case of error */
	wlm_free_args_mem(args);
	return retval;
}

int wlmGetSubCounters(char *cmd_line, char *stats, int length)
{
	/* Extra 3 args are for command_name, counter_ver and last NULL terminated arg. */
	char *tmp_argv[MAX_SUBCOUNTER_SUPPORTED + 3];
	char **argv = tmp_argv;
	int retval;
	int written = 0;
	int err = TRUE;
	void *ptr;
	char *endptr;
	uint8 subcntdata[(MAX_SUBCOUNTER_SUPPORTED * sizeof(uint32)) +
		OFFSETOF(wl_subcnt_info_t, data)];
	int32 cntr_ver = -1;
	uint32 cntr_num = 0;
	uint32 req_len = 0;
	uint32 arg_index = 1;	/* 0th arg is command_name */
	wl_subcnt_info_t *subcntinfo;

	retval = wlm_subcntr_buf_to_args(cmd_line, argv, (MAX_SUBCOUNTER_SUPPORTED + 3));
	if (retval != BCME_OK) {
		return retval;
	}

	memset(subcntdata, 0, sizeof(subcntdata));

	if (argv[arg_index] != NULL) {
		cntr_ver = strtoul(argv[arg_index], &endptr, 0);
		if (*endptr != '\0') {
			written = snprintf(stats, length, "Invalid cntr ver %s\n", argv[arg_index]);
			stats += written;
			length -= written;
			if (length <= 0) {
				err = BCME_BUFTOOSHORT;
				goto error;
			}
			printf("Invalid cntr ver %s\n", argv[arg_index]);
			return BCME_BADARG;
		}
		arg_index++;
	}

	subcntinfo = (wl_subcnt_info_t *)subcntdata;
	while (argv[arg_index] != NULL)	{
		if (cntr_num >= MAX_SUBCOUNTER_SUPPORTED) {
			printf("Max counters supported: %d\n", MAX_SUBCOUNTER_SUPPORTED);
			return BCME_BADARG;
		}
		if (get_counter_offset(argv[arg_index],
				&subcntinfo->data[cntr_num], cntr_ver)) {
			return FALSE;
		}
		arg_index++;
		cntr_num++;
	}

	if ((cntr_ver != -1) && (cntr_num == 0)) {
		print_counter_help(cntr_ver);
		return TRUE;
	}

	req_len = OFFSETOF(wl_subcnt_info_t, data) + (cntr_num * sizeof(subcntinfo->data[0]));
	subcntinfo->version = htod16(WL_SUBCNTR_IOV_VER);
	subcntinfo->counters_version = htod16(cntr_ver);
	subcntinfo->num_subcounters = htod16(cntr_num);
	subcntinfo->length = htod16(req_len);

	if ((retval = wlu_var_getbuf_minimal(irh, "subcounters", subcntdata, req_len, &ptr))) {
		return (retval);
	}

	subcntinfo = ptr;
	subcntinfo->version = dtoh16(subcntinfo->version);
	subcntinfo->counters_version = dtoh16(subcntinfo->counters_version);
	subcntinfo->num_subcounters = dtoh16(subcntinfo->num_subcounters);
	subcntinfo->length = dtoh16(subcntinfo->length);

	if ((cntr_ver == -1) || (cntr_num == 0) || (subcntinfo->num_subcounters == 0)) {
		written = snprintf(stats, length, "FW counter Version %d\n",
			subcntinfo->counters_version);
		stats += written;
		length -= written;
		if (length <= 0) {
			err = BCME_BUFTOOSHORT;
			goto error;
		}
		printf("FW counter Version %d\n", subcntinfo->counters_version);
		return TRUE;
	}

	if ((subcntinfo->num_subcounters > MAX_SUBCOUNTER_SUPPORTED) ||
			(subcntinfo->length != req_len) ||
			(subcntinfo->length != OFFSETOF(wl_subcnt_info_t, data) +
			(subcntinfo->num_subcounters * sizeof(subcntinfo->data[0])))) {
		written = snprintf(stats, length,
			"Mismatch App:FW cntr_ver %d:%d iov_ver %d:%d num_cntr %d:%d len %d:%d\n",
			cntr_ver, subcntinfo->counters_version,
			WL_SUBCNTR_IOV_VER, subcntinfo->version, cntr_num,
			subcntinfo->num_subcounters, req_len, subcntinfo->length);
			stats += written;
			length -= written;
			if (length <= 0) {
				err = BCME_BUFTOOSHORT;
				goto error;
			}
		printf("Mismatch App:FW cntr_ver %d:%d iov_ver %d:%d num_cntr %d:%d len %d:%d\n",
			cntr_ver, subcntinfo->counters_version,
			WL_SUBCNTR_IOV_VER, subcntinfo->version, cntr_num,
			subcntinfo->num_subcounters, req_len, subcntinfo->length);
		return TRUE;
	}

	for (cntr_num = 0; cntr_num < subcntinfo->num_subcounters; cntr_num++)
	{
		/* Print max 5 counters value in a single row */
		if (cntr_num % 5 == 0) {
			written = snprintf(stats, length, "\n");
			stats += written;
			length -= written;
			if (length <= 0) {
				err = BCME_BUFTOOSHORT;
				goto error;
			}
		}
		written = snprintf(stats, length, "%s %d ", argv[cntr_num + 2],
			dtoh32(subcntinfo->data[cntr_num]));
		stats += written;
		length -= written;
		if (length <= 0) {
			err = BCME_BUFTOOSHORT;
			goto error;
		}
	}
	written = snprintf(stats, length, "\n");
	stats += written;
	length -= written;
	if (length <= 0) {
		err = BCME_BUFTOOSHORT;
		goto error;
	}
error:
	if (err == BCME_BUFTOOSHORT) {
		printf("Buff Too Short\n");
	}
	wlm_free_args_mem(argv);
	return err;
}

int wlmGetCounters(char *stats, int length)
{
	char *statsbuf;
	wl_cnt_ver_11_t *cnt;
	int err;
	uint i;
	void *ptr;
	uint16 ver;

	if (length < ((int)sizeof(wl_cnt_ver_11_t) * 2)) {
		printf("wlmGetCounters: counter stats buffer length too short\n");
		return FALSE;
	}

	if ((err = wlu_var_getbuf_med(irh, "counters", NULL, 0, &ptr))) {
		printf("wlmGetCounters: %s\n", wlmLastError());
		return FALSE;
	}

	statsbuf = (char *)ptr;

	ver = *(uint16*)statsbuf;

	if (ver > WL_CNT_T_VERSION) {
		printf("\tIncorrect version of counters struct: expected %d; got %d\n",
			WL_CNT_T_VERSION, ver);
		return FALSE;
	}

	printf("counters_version %2d\n", ver);

	cnt = (wl_cnt_ver_11_t*)CALLOC(sizeof(wl_cnt_ver_11_t));
	if (cnt == NULL) {
		printf("wlmGetCounters: Can not allocate %d bytes for counters struct\n",
			(int)sizeof(wl_cnt_ver_11_t));
		return FALSE;
	}
	else
		memcpy(cnt, statsbuf, sizeof(wl_cnt_ver_11_t));

	/* summary stat counter line */
	WLMPRVAL(txframe); WLMPRVAL(txbyte); WLMPRVAL(txretrans); WLMPRVAL(txerror);
	WLMPRVAL(rxframe); WLMPRVAL(rxbyte); WLMPRVAL(rxerror); WLMPRNL();

	WLMPRVAL(txprshort); WLMPRVAL(txdmawar); WLMPRVAL(txnobuf); WLMPRVAL(txnoassoc);
	WLMPRVAL(txchit); WLMPRVAL(txcmiss); WLMPRNL();

	WLMPRVAL(reset); WLMPRVAL(txserr); WLMPRVAL(txphyerr); WLMPRVAL(txphycrs);
	WLMPRVAL(txfail); WLMPRVAL(tbtt); WLMPRNL();

	stats += sprintf(stats, "d11_txfrag %u d11_txmulti %u d11_txretry %u d11_txretrie %u\n",
		dtoh32(cnt->txfrag), dtoh32(cnt->txmulti), dtoh32(cnt->txretry),
		dtoh32(cnt->txretrie));

	stats += sprintf(stats, "d11_txrts %u d11_txnocts %u d11_txnoack %u d11_txfrmsnt %u\n",
		dtoh32(cnt->txrts), dtoh32(cnt->txnocts), dtoh32(cnt->txnoack),
		dtoh32(cnt->txfrmsnt));

	WLMPRVAL(rxcrc); WLMPRVAL(rxnobuf); WLMPRVAL(rxnondata); WLMPRVAL(rxbadds);
	WLMPRVAL(rxbadcm); WLMPRVAL(rxdup);
	if (cnt->version == 7) {
		if (cnt->length >= OFFSETOF(wl_cnt_ver_11_t, dma_hang) + sizeof(uint32))
			WLMPRVAL(rxrtry);
	}
	if (cnt->version >= 10) {
		if (cnt->length >= OFFSETOF(wl_cnt_ver_11_t,
			reinitreason[NREINITREASONCOUNT - 1]) + sizeof(uint32))
			WLMPRVAL(rxrtry);
	}
	WLMPRVAL(rxfragerr); WLMPRNL();
	WLMPRVAL(rxrunt); WLMPRVAL(rxgiant); WLMPRVAL(rxnoscb); WLMPRVAL(rxbadproto);
	WLMPRVAL(rxbadsrcmac); WLMPRNL();

	stats += sprintf(stats, "d11_rxfrag %u d11_rxmulti %u d11_rxundec %u\n",
		dtoh32(cnt->rxfrag), dtoh32(cnt->rxmulti), dtoh32(cnt->rxundec));

	WLMPRVAL(rxctl); WLMPRVAL(rxbadda); WLMPRVAL(rxfilter); WLMPRNL();

	stats += sprintf(stats, "rxuflo: ");
	for (i = 0; i < NFIFO_LEGACY; i++)
		stats += sprintf(stats, "%u ", dtoh32(cnt->rxuflo[i]));
	stats += sprintf(stats, "\n");
	WLMPRVAL(txallfrm); WLMPRVAL(txrtsfrm); WLMPRVAL(txctsfrm); WLMPRVAL(txackfrm); WLMPRNL();
	WLMPRVAL(txdnlfrm); WLMPRVAL(txbcnfrm); WLMPRVAL(txtplunfl); WLMPRVAL(txphyerr); WLMPRNL();
	stats += sprintf(stats, "txfunfl: ");
	for (i = 0; i < NFIFO_LEGACY; i++)
		stats += sprintf(stats, "%u ", dtoh32(cnt->txfunfl[i]));
	stats += sprintf(stats, "\n");

	/* WPA2 counters */
	WLMPRNL();
	WLMPRVAL(tkipmicfaill); WLMPRVAL(tkipicverr); WLMPRVAL(tkipcntrmsr); WLMPRNL();
	WLMPRVAL(tkipreplay); WLMPRVAL(ccmpfmterr); WLMPRVAL(ccmpreplay); WLMPRNL();
	WLMPRVAL(ccmpundec); WLMPRVAL(fourwayfail); WLMPRVAL(wepundec); WLMPRNL();
	WLMPRVAL(wepicverr); WLMPRVAL(decsuccess); WLMPRVAL(rxundec); WLMPRNL();

	WLMPRNL();
	WLMPRVAL(rxfrmtoolong); WLMPRVAL(rxfrmtooshrt);
	WLMPRVAL(rxinvmachdr); WLMPRVAL(rxbadfcs); WLMPRNL();
	WLMPRVAL(rxbadplcp); WLMPRVAL(rxcrsglitch);
	WLMPRVAL(rxstrt); WLMPRVAL(rxdfrmucastmbss); WLMPRNL();
	WLMPRVAL(rxmfrmucastmbss); WLMPRVAL(rxcfrmucast);
	WLMPRVAL(rxrtsucast); WLMPRVAL(rxctsucast); WLMPRNL();
	WLMPRVAL(rxackucast); WLMPRVAL(rxdfrmocast);
	WLMPRVAL(rxmfrmocast); WLMPRVAL(rxcfrmocast); WLMPRNL();
	WLMPRVAL(rxrtsocast); WLMPRVAL(rxctsocast);
	WLMPRVAL(rxdfrmmcast); WLMPRVAL(rxmfrmmcast); WLMPRNL();
	WLMPRVAL(rxcfrmmcast); WLMPRVAL(rxbeaconmbss);
	WLMPRVAL(rxdfrmucastobss); WLMPRVAL(rxbeaconobss); WLMPRNL();
	WLMPRVAL(rxrsptmout); WLMPRVAL(bcntxcancl);
	WLMPRVAL(rxf0ovfl); WLMPRVAL(rxf1ovfl); WLMPRNL();
	WLMPRVAL(rxf2ovfl); WLMPRVAL(txsfovfl); WLMPRVAL(pmqovfl); WLMPRNL();
	WLMPRVAL(rxcgprqfrm); WLMPRVAL(rxcgprsqovfl);
	WLMPRVAL(txcgprsfail); WLMPRVAL(txcgprssuc); WLMPRNL();
	WLMPRVAL(prs_timeout); WLMPRVAL(rxnack); WLMPRVAL(frmscons);
	WLMPRVAL(txnack); WLMPRVAL(txphyerror); WLMPRNL();

	WLMPRVAL(txchanrej); WLMPRNL();

	/* per-rate receive counters */
	WLMPRVAL(rx1mbps); WLMPRVAL(rx2mbps); WLMPRVAL(rx5mbps5); WLMPRNL();
	WLMPRVAL(rx6mbps); WLMPRVAL(rx9mbps); WLMPRVAL(rx11mbps); WLMPRNL();
	WLMPRVAL(rx12mbps); WLMPRVAL(rx18mbps); WLMPRVAL(rx24mbps); WLMPRNL();
	WLMPRVAL(rx36mbps); WLMPRVAL(rx48mbps); WLMPRVAL(rx54mbps); WLMPRNL();

	WLMPRVAL(pktengrxducast); WLMPRVAL(pktengrxdmcast); WLMPRNL();

	if (cnt->version >= 8) {
		WLMPRNL();
		if (cnt->length >= OFFSETOF(wl_cnt_ver_11_t, cso_passthrough) + sizeof(uint32)) {
			WLMPRVAL(cso_normal);
			WLMPRVAL(cso_passthrough);
			WLMPRNL();
		}
		WLMPRVAL(chained); WLMPRVAL(chainedsz1); WLMPRVAL(unchained); WLMPRVAL(maxchainsz);
		WLMPRVAL(currchainsz); WLMPRNL();
	}

	if (cnt->version >= 9) {
		WLMPRVAL(pciereset); WLMPRVAL(cfgrestore); WLMPRNL();
	}

	stats += sprintf(stats, "\n");

	if (cnt)
		free(cnt);

	return TRUE;
}

int wlmRssiGet(int *rssi)
{
	wl_pkteng_stats_t *cnt;

	if (wlu_var_getbuf_sm(irh, "pkteng_stats", NULL, 0, (void **)&cnt)) {
		printf("wlmRssiGet: %s\n", wlmLastError());
		return FALSE;
	}

	*rssi = dtoh32(cnt->rssi);
	return TRUE;
}

int wlmUnmodRssiGet(int* rssi)
{
	if (wlu_iovar_getint(irh, "unmod_rssi", rssi)) {
		printf("wlmUnmodRssiGet: %s\n", wlmLastError());
		return FALSE;
	}
	*rssi = dtoh32(*rssi);

	return TRUE;
}

int wlmSequenceStart(int clientBatching)
{
	if (wl_seq_batch_in_client((bool)clientBatching)) {
		printf("wlmSequenceStart: %s\n", wlmLastError());
		return FALSE;
	}

	if (wl_seq_start(irh, 0, 0)) {
		printf("wlmSequenceStart: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSequenceStop(void)
{
	if (wl_seq_stop(irh, 0, 0)) {
		printf("wlmSequenceStop: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSequenceDelay(int msec)
{
	if (wlu_iovar_setint(irh, "seq_delay", msec)) {
		printf("wlmSequenceDelay: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSequenceErrorIndex(int *index)
{
	if (wlu_iovar_getint(irh, "seq_error_index", index)) {
		printf("wlmSequenceErrorIndex: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmDeviceImageWrite(const char* byteStream, int length, WLM_IMAGE_TYPE imageType)
{
	srom_rw_t *srt;
	char buffer[WLC_IOCTL_MAXLEN] = {0};

	char *bufp;
	char *cisp, *cisdata;
	cis_rw_t cish;

	if (byteStream == NULL) {
		printf("wlmDeviceImageWrite: Buffer is invalid!\n");
		return FALSE;
	}
	if (length > SROM_MAX+1) {
	    printf("wlmDeviceImageWrite: Data length should be less than %d bytes\n", SROM_MAX);
	    return FALSE;
	}

	switch (imageType) {
	case WLM_TYPE_SROM:
		srt = (srom_rw_t *)buffer;
		memcpy(srt->buf, byteStream, length);

		if (length == SROM4_WORDS * 2) {
			if ((srt->buf[SROM4_SIGN] != SROM4_SIGNATURE) &&
			    (srt->buf[SROM8_SIGN] != SROM4_SIGNATURE)) {
				printf("wlmDeviceImageWrite: Data lacks a REV4 signature!\n");
			    return FALSE;
			}
		} else if ((length != SROM_WORDS * 2) && (length != SROM_MAX)) {
		    printf("wlmDeviceImageWrite: Data length is invalid!\n");
		    return FALSE;
		}

		srt->nbytes = length;
		if (wlu_set(irh, WLC_SET_SROM, buffer, length + 8)) {
		    printf("wlmDeviceImageWrite: %s\n", wlmLastError());
		    return FALSE;
		}

		break;
	case WLM_TYPE_OTP:
		bufp = buffer;
		strcpy(bufp, "ciswrite");
		bufp += strlen("ciswrite") + 1;
		cisp = bufp;
		cisdata = cisp + sizeof(cish);
		cish.source = htod16(0);
		cish.flags = htod16(0);
		memcpy(cisdata, byteStream, length);

		cish.byteoff = htod32(0);
		cish.nbytes = htod32(length);
		memcpy(cisp, (char*)&cish, sizeof(cish));

		if (wl_set(irh, WLC_SET_VAR, buffer, (cisp - buffer) + sizeof(cish) + length) < 0) {
		    printf("wlmDeviceImageWrite: %s\n", wlmLastError());
		    return FALSE;
		 }
		break;
	case WLM_TYPE_AUTO:
		if (!wlmDeviceImageWrite(byteStream, length, WLM_TYPE_SROM) &&
			!wlmDeviceImageWrite(byteStream, length, WLM_TYPE_OTP)) {
		    printf("wlmDeviceImageWrite: %s\n", wlmLastError());
		    return FALSE;
	    }
	    break;
	default:
		printf("wlmDeviceImageWrite: Invalid image type!\n");
		return FALSE;
	}
	return TRUE;
}

int wlmDeviceImageRead(char* byteStream, unsigned int len, WLM_IMAGE_TYPE imageType)
{
	srom_rw_t *srt;
	cis_rw_t *cish;
	char buf[WLC_IOCTL_MAXLEN] = {0};
	unsigned int numRead = 0;

	if (byteStream == NULL) {
		printf("wlmDeviceImageRead: Buffer is invalid!\n");
		return FALSE;
	}

	if (len > SROM_MAX) {
	    printf("wlmDeviceImageRead: byteStream should be less than %d bytes!\n", SROM_MAX);
	    return FALSE;
	}

	if (len & 1) {
			printf("wlmDeviceImageRead: Invalid byte count %d, must be even\n", len);
			return FALSE;
	}

	switch (imageType) {
	case WLM_TYPE_SROM:
		if (len < 2*SROM4_WORDS) {
			printf("wlmDeviceImageRead: Buffer not large enough!\n");
			return FALSE;
		}

		srt = (srom_rw_t *)buf;
		srt->byteoff = 0;
		srt->nbytes = htod32(2 * SROM4_WORDS);
		/* strlen("cisdump ") = 9 */
		if (wlu_get(irh, WLC_GET_SROM, buf, 9 + (len < SROM_MAX ? len : SROM_MAX)) < 0) {
			printf("wlmDeviceImageRead: %s\n", wlmLastError());
			return FALSE;
		}
		memcpy(byteStream, buf + 8, srt->nbytes);
		numRead = srt->nbytes;
		break;
	case WLM_TYPE_OTP:
#define CISDUMP "cisdump"
		strcpy(buf, CISDUMP);
		if (wl_get(irh, WLC_GET_VAR, buf, MAX(strlen(CISDUMP) + 1, sizeof(cis_rw_t)) +
			MIN(len, SROM_MAX)) < 0) {
		    printf("wlmDeviceImageRead: %s\n", wlmLastError());
		    return FALSE;
		}

		cish = (cis_rw_t *)buf;
		cish->source = dtoh16(cish->source);
		cish->flags = dtoh16(cish->flags);
		cish->byteoff = dtoh32(cish->byteoff);
		cish->nbytes = dtoh32(cish->nbytes);

		if (len < cish->nbytes) {
			printf("wlmDeviceImageRead: Buffer not large enough!\n");
			return FALSE;
		}
		memcpy(byteStream, buf + sizeof(cis_rw_t), cish->nbytes);
		numRead = cish->nbytes;
		break;
	case WLM_TYPE_AUTO:
	  numRead = wlmDeviceImageRead(byteStream, len, WLM_TYPE_SROM);
		if (!numRead) {
			numRead = wlmDeviceImageRead(byteStream, len, WLM_TYPE_OTP);
		    printf("wlmDeviceImageRead: %s\n", wlmLastError());
		    return FALSE;
	    }
	    break;
	default:
		printf("wlmDeviceImageRead: Invalid image type!\n");
		return FALSE;
	}
	return numRead;
}

int
wlmCisUpdate(int offset, char *byteStream, int preview)
{
	int ret = BCME_ERROR;
	int i = 0;
	cmd_t command = {"cisupdate", wlu_cisupdate, -1, WLC_SET_VAR, " "};
	char *args[WLM_NUM_ARGS] = {0};
	wluc_otp_module_init();

	if (preview < 0 || preview > 1) {
		printf("wlmCisUpdate: Bad argument\n");
		return FALSE;
	}

	args[i++] = "cisupdate";

	CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
	sprintf(args[i], "%d", offset);
	i++;

	args[i++] = byteStream;

	if (preview)
		args[i++] = "--preview";

	args[i] = NULL;

	ret = wlu_cisupdate(irh, &command, (char **)args);

	if (ret != BCME_OK) {
		printf("wlmCisUpdate: failed with ret = %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	if (args[1] != NULL)
		free(args[1]);
	return ret;
}

int
wlmCisWrite(char *fileName, int pciecis, int preview)
{
	int ret = BCME_ERROR;
	int i = 0;
	char *args[WLM_NUM_ARGS] = {0};
	cmd_t command = {"ciswrite", wlu_ciswrite, -1, WLC_SET_VAR, " "};
	wluc_otp_module_init();

	if (preview < 0 || preview > 1) {
		printf("wlmCisWrite: Bad argument\n");
		return FALSE;
	}

	args[i++] = "ciswrite";

	if (pciecis)
		args[i++] = "--pciecis";

	args[i++] = fileName;

	if (preview)
		args[i++] = "--preview";
	args[i] = NULL;

	ret = wlu_ciswrite(irh, &command, (char **)args);

	if (ret != BCME_OK) {
		printf("wlmCisWrite: failed with ret = %d\n", ret);
		return FALSE;
	}

	return TRUE;
}

int
wlmOtpRaw(int offset, int num_bits, char *data)
{
	int ret = BCME_ERROR;
	int i = 0;
	cmd_t command = {"otpraw", wl_otpraw, WLC_GET_VAR, WLC_SET_VAR, " "};
	char *args[WLM_NUM_ARGS] = {0};

	args[i++] = "otpraw";

	CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
	sprintf(args[i], "%d", offset);
	i++;

	CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
	sprintf(args[i], "%d", num_bits);
	i++;

	args[i++] = data;

	args[i] = NULL;

	ret = wl_otpraw(irh, &command, (char **)args);

	if (ret != BCME_OK) {
		printf("wlmOtpRaw: failed with ret = %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	if (args[1] != NULL)
		free(args[1]);
	if (args[2] != NULL)
		free(args[2]);
	return ret;
}

int
wlmOtpW(char *fileName)
{
	int ret = BCME_ERROR;
	int i = 0;
	char *args[WLM_NUM_ARGS] = {0};
	cmd_t command = {"otpw", wl_otpw, -1, WLC_OTPW, " "};

	args[i++] = "otpw";

	args[i++] = fileName;

	args[i] = NULL;

	ret = wl_otpw(irh, &command, (char **)args);

	if (ret != BCME_OK) {
		printf("wlmOtpW: failed with ret = %d\n", ret);
		return FALSE;
	}
	return TRUE;
}

int wlmOtpCrc(int update_crc, int *result)
{
	uint8 *tmp_buf = NULL;
	otpcrc_config_t otpcrc_config;
	uint32 otpsize;
	int8 ret = TRUE;
	*result = 0;

	if (update_crc < 0 || update_crc > 1) {
		printf("wlmOtpCrc: Bad argument\n");
		return FALSE;
	}

	if ((ret = wlu_get_crc_config(irh, &otpcrc_config)) == BCME_NO_CRCCONFIG) {
		printf("wlmOtpCrc: OTP_CRC_Config not programmed\n");
		return TRUE;
	} else {
		if (ret != BCME_VALID_CRCCONFIG) {
			printf("wlmOtpCrc: bad OTP_CRC_Config\n");
			return FALSE;
		} else {
			ret = TRUE;
		}
	}

	/* Whenever control reaches here the value of ret will always be
	 * BCME_VALID_CRCCONFIG which is equivalent to BCME_OK.
	 */
	if ((tmp_buf = wlu_get_otp_read_buf(irh, &otpsize)) == NULL) {
		printf("wlmOtpCrc: no memory");
		return FALSE;
	}

	memset(tmp_buf, 0, otpsize);

	if (wlu_read_otp_data(irh, tmp_buf, 0) != BCME_OK) {
		printf("wlmOtpCrc: OTP read failed\n");
		ret =  FALSE;
		goto done;
	}
	if (wlu_check_otp_integrity(tmp_buf, &otpcrc_config) != BCME_OK) {
		printf("wlmOtpCrc: CRC check failed\n");
		if (update_crc) {
			printf("wlmOtpCrc: Updating CRC ... \n");
			if ((wlu_update_crc(irh, tmp_buf, 0, &otpcrc_config)) != BCME_OK) {
				printf("wlmOtpCrc: CRC update failed\n");
				ret = FALSE;
				goto done;
			} else
				*result = 1;
		}
	} else {
		printf("wlmOtpCrc: CRC check Pass\n");
		*result = 1;
	}

done:
	if (tmp_buf)
		free(tmp_buf);
	return ret;
}

int wlmOtpCrcConfigGet(int *start_addr, int *end_addr, int *num_crc, int *crc_ver)
{
	otpcrc_config_t otpcrc_config;
	int8 ret = TRUE;

	if ((ret = wlu_get_crc_config(irh, &otpcrc_config)) == BCME_NO_CRCCONFIG) {
		printf("wlmOtpCrcConfigGet: OTP_CRC_Config not programmed\n");
		return TRUE;
	} else {
		if (ret == BCME_VALID_CRCCONFIG) {
			*crc_ver = otpcrc_config.crc_ver;
			*num_crc = otpcrc_config.num_crc;
			*end_addr = otpcrc_config.end_addr;
			*start_addr = otpcrc_config.end_addr - otpcrc_config.num_crc + 1;
			return TRUE;
		} else {
			return FALSE;
		}
	}
}

int wlmOtpCrcConfigSet(int address, int size, int ver, char *config)
{
	int ret = BCME_ERROR;
	int i = 0;
	int use_config = 0;
	cmd_t command = {"otpcrcconfig", wlu_otpcrcconfig, WLC_GET_VAR, -1, " "};
	char *args[WLM_NUM_ARGS] = {0};

	args[i++] = "otpcrcconfig";

	if (!strncmp(config, "NULL", 4)) {
		args[i++] = "-a";

		CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
		sprintf(args[i], "%d", address);
		i++;

		args[i++] = "-s";

		CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
		sprintf(args[i], "%d", size);
		i++;

		args[i++] = "-v";

		CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
		sprintf(args[i], "%d", ver);
		i++;
	} else {
		use_config = 1;
		args[i++] = config;
	}

	args[i] = NULL;

	ret = wlu_otpcrcconfig(irh, &command, (char **)args);

	if (ret != BCME_OK) {
		printf("wlmOtpCrcConfigSet: failed with ret = %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	if (!use_config) {
		if (args[2] != NULL)
			free(args[2]);
		if (args[4] != NULL)
			free(args[4]);
		if (args[6] != NULL)
			free(args[6]);
	}
	return ret;
}

int wlmCisDump(char *output, int length)
{
	char *bufp;
	int i, ret = 0;
	cis_rw_t cish;
	uint nbytes = 0;
	char buf[WLC_IOCTL_MAXLEN] = {0};
	int required_length = 0;

	/* Prepare the read info */
	memset((char*)&cish, 0, sizeof(cish));
	cish.nbytes = htod32(nbytes);

	/* set up the buffer and do the get */
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "cisdump");
	bufp = buf + strlen("cisdump") + 1;
	memcpy(bufp, (char*)&cish, sizeof(cish));
	bufp += sizeof(cish);
	ret = wl_get(irh, WLC_GET_VAR, buf, (bufp - buf) + (nbytes ? nbytes : SROM_MAX));
	if (ret != BCME_OK) {
		printf("wlmCisDump: Failed cisdump request: %d\n", ret);
		goto done;
	}

	/* pull off the cis_rw_t */
	bufp = buf;
	memcpy((char*)&cish, bufp, sizeof(cish));
	cish.source = dtoh16(cish.source);
	cish.flags = dtoh16(cish.flags);
	cish.byteoff = dtoh32(cish.byteoff);
	cish.nbytes = dtoh32(cish.nbytes);

	/* move past to the data */
	bufp += sizeof(cish);

	required_length = snprintf(output, length, "Source: %d (%s)", cish.source,
	       (cish.source == WLC_CIS_DEFAULT) ? "Built-in default" :
	       (cish.source == WLC_CIS_SROM) ? "External SPROM" :
	       (cish.source == WLC_CIS_OTP) ? "Internal OTP" : "Unknown?");
	CHECK_SNP_BUF_OVERRUN(output, required_length, length)

	if (!nbytes) {
		required_length = snprintf(output, length, "\nMaximum length: %d bytes",
				cish.nbytes);
		CHECK_SNP_BUF_OVERRUN(output, required_length, length)
	}

	for (i = 0; i < (int)cish.nbytes; i++) {
		if ((i % 8) == 0) {
			required_length = snprintf(output, length, "\nByte 0x%3X: ", i);
			CHECK_SNP_BUF_OVERRUN(output, required_length, length)
		}
		required_length = snprintf(output, length, "0x%02x ", (uint8)bufp[i]);
		CHECK_SNP_BUF_OVERRUN(output, required_length, length)
	}

	return TRUE;

done:
	return ret;

}

int wlmSecuritySet(WLM_AUTH_TYPE authType, WLM_AUTH_MODE authMode,
	WLM_ENCRYPTION encryption, const char *key)
{
	int length = 0;
	int wpa_auth;
	int primary_key = 0;
	wl_wsec_key_t wepKey[4];
	wsec_pmk_t psk;
	int wsec;

	if (encryption != WLM_ENCRYPT_NONE && key == 0) {
		printf("wlmSecuritySet: invalid key\n");
		return FALSE;
	}

	if (key) {
		length = strlen(key);
	}

	switch (encryption) {

	case WLM_ENCRYPT_NONE:
		wpa_auth = WPA_AUTH_DISABLED;
		break;

	case WLM_ENCRYPT_WEP: {
		int i;
		int len = length / 4;

		wpa_auth = WPA_AUTH_DISABLED;

		if (!(length == 40 || length == 104 || length == 128 || length == 256)) {
			printf("wlmSecuritySet: invalid WEP key length %d"
			"       - expect 40, 104, 128, or 256"
			" (i.e. 10, 26, 32, or 64 for each of 4 keys)\n", length);
			return FALSE;
		}

		/* convert hex key string to 4 binary keys */
		for (i = 0; i < 4; i++) {
			wl_wsec_key_t *k = &wepKey[i];
			const char *data = &key[i * len];
			unsigned int j;

			memset(k, 0, sizeof(*k));
			k->index = i;
			k->len = len / 2;

			for (j = 0; j < k->len; j++) {
				char hex[] = "XX";
				char *end = NULL;
				strncpy(hex, &data[j * 2], 2);
				k->data[j] = (char)strtoul(hex, &end, 16);
				if (*end != 0) {
					printf("wlmSecuritySet: invalid WEP key"
					"       - expect hex values\n");
					return FALSE;
				}
			}

			switch (k->len) {
			case 5:
				k->algo = CRYPTO_ALGO_WEP1;
				break;
			case 13:
				k->algo = CRYPTO_ALGO_WEP128;
				break;
			case 16:
				k->algo = CRYPTO_ALGO_AES_CCM;
				break;
			case 32:
				k->algo = CRYPTO_ALGO_TKIP;
				break;
			default:
				/* invalid */
				return FALSE;
			}

			k->flags |= WL_PRIMARY_KEY;
		}

		break;
	}

	case WLM_ENCRYPT_TKIP:
	case WLM_ENCRYPT_AES: {

		if (authMode != WLM_WPA_AUTH_PSK && authMode != WLM_WPA2_AUTH_PSK) {
			printf("wlmSecuritySet: authentication mode must be WPA PSK or WPA2 PSK\n");
			return FALSE;
		}

		wpa_auth = authMode;

		if (length < WSEC_MIN_PSK_LEN || length > WSEC_MAX_PSK_LEN) {
			printf("wlmSecuritySet: passphrase must be between %d and %d characters\n",
			WSEC_MIN_PSK_LEN, WSEC_MAX_PSK_LEN);
			return FALSE;
		}

		psk.key_len = length;
		psk.flags = WSEC_PASSPHRASE;
		memcpy(psk.key, key, length);

		break;
	}

	case WLM_ENCRYPT_WSEC:
	case WLM_ENCRYPT_FIPS:
	default:
		printf("wlmSecuritySet: encryption not supported\n");
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "auth", authType)) {
		printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "wpa_auth", wpa_auth)) {
		printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	if (encryption == WLM_ENCRYPT_WEP) {
		int i;
		for (i = 0; i < 4; i++) {
			wl_wsec_key_t *k = &wepKey[i];
			k->index = htod32(k->index);
			k->len = htod32(k->len);
			k->algo = htod32(k->algo);
			k->flags = htod32(k->flags);

			if (wlu_set(irh, WLC_SET_KEY, k, sizeof(*k))) {
				printf("wlmSecuritySet: %s\n", wlmLastError());
				return FALSE;
			}
		}

		primary_key = htod32(primary_key);
		if (wlu_set(irh, WLC_SET_KEY_PRIMARY, &primary_key, sizeof(primary_key)) < 0) {
			printf("wlmSecuritySet: %s\n", wlmLastError());
			return FALSE;
		}
	}
	else if (encryption == WLM_ENCRYPT_TKIP || encryption == WLM_ENCRYPT_AES) {
		psk.key_len = htod16(psk.key_len);
		psk.flags = htod16(psk.flags);

		if (wlu_set(irh, WLC_SET_WSEC_PMK, &psk, sizeof(psk))) {
			printf("wlmSecuritySet: %s\n", wlmLastError());
			return FALSE;
		}
	}

	wsec = htod32(encryption);
	/*
	if (wlu_set(irh, WLC_SET_WSEC, &wsec, sizeof(wsec)) < 0) {
		printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}
	*/
	if (wlu_iovar_setint(irh, "wsec", wsec) < 0) {
		printf("wlmSecuritySet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmJoinNetwork(const char* ssid, WLM_JOIN_MODE mode)
{
	wlc_ssid_t wlcSsid;
	int infra = htod32(mode);
	if (wlu_set(irh, WLC_SET_INFRA, &infra, sizeof(int)) < 0) {
	    printf("wlmJoinNetwork: %s\n", wlmLastError());
	    return FALSE;
	}

	wlcSsid.SSID_len = htod32(strlen(ssid));
	memcpy(wlcSsid.SSID, ssid, wlcSsid.SSID_len);

	if (wlu_set(irh, WLC_SET_SSID, &wlcSsid, sizeof(wlc_ssid_t)) < 0) {
		printf("wlmJoinNetwork: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmDisassociateNetwork(void)
{
	if (wlu_set(irh, WLC_DISASSOC, NULL, 0) < 0) {
		printf("wlmDisassociateNetwork: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmSsidGet(char *ssid, int length)
{
	wlc_ssid_t wlc_ssid;

	if (length < SSID_FMT_BUF_LEN) {
		printf("wlmSsidGet: Ssid buffer too short - %d bytes at least\n",
		SSID_FMT_BUF_LEN);
		return FALSE;
	}

	/* query for 'ssid' */
	if (wlu_get(irh, WLC_GET_SSID, &wlc_ssid, sizeof(wlc_ssid_t))) {
		printf("wlmSsidGet: %s\n", wlmLastError());
		return FALSE;
	}

	wl_format_ssid(ssid, wlc_ssid.SSID, dtoh32(wlc_ssid.SSID_len));

	return TRUE;
}

int wlmBssidGet(char *bssid, int length)
{
	struct ether_addr ea;
	int etoa_buf_len = ETHER_ADDR_LEN * 3;

	if (length < etoa_buf_len) {
		printf("wlmBssiGet: bssid requires %d bytes\n", etoa_buf_len);
		return FALSE;
	}

	if (wlu_get(irh, WLC_GET_BSSID, &ea, ETHER_ADDR_LEN) == 0) {
		/* associated - format and return bssid */
		strncpy(bssid, wl_ether_etoa(&ea), length);
	}
	else {
		/* not associated - return empty string */
		memset(bssid, 0, length);
	}

	return TRUE;
}

int wlmGlacialTimerSet(int val)
{
	if (wlu_iovar_setint(irh, "glacial_timer", val)) {
		printf("wlmGlacialTimerSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmFastTimerSet(int val)
{
	if (wlu_iovar_setint(irh, "fast_timer", val)) {
		printf("wlmFastTimerSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmSlowTimerSet(int val)
{
	if (wlu_iovar_setint(irh, "slow_timer", val)) {
		printf("wlmGlacialTimerSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmScanSuppress(int on)
{
	int val;
	if (on)
		val = 1;
	else
		val = 0;

	if (wlu_set(irh, WLC_SET_SCANSUPPRESS, &val, sizeof(int))) {
		printf("wlmScansuppress: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmCountryCodeSet(const char * country_name)
{
	wl_country_t cspec;
	int err;

	memset(&cspec, 0, sizeof(cspec));
	cspec.rev = -1;

	/* arg matched a country name */
	memcpy(cspec.country_abbrev, country_name, WLC_CNTRY_BUF_SZ);
	err = 0;

	/* first try the country iovar */
	if (cspec.rev == -1 && cspec.ccode[0] == '\0')
		err = wlu_iovar_set(irh, "country", &cspec, WLC_CNTRY_BUF_SZ);
	else
		err = wlu_iovar_set(irh, "country", &cspec, sizeof(cspec));

	if (err == 0)
		return TRUE;
	return FALSE;
}

int wlmFullCal(void)
{
	return TRUE;
}

int wlmIoctlGet(int cmd, void *buf, int len)
{
	if (wlu_get(irh, cmd, buf, len)) {
		printf("wlmIoctlGet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIoctlSet(int cmd, void *buf, int len)
{
	if (wlu_set(irh, cmd, buf, len)) {
		printf("wlmIoctlSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarGet(const char *iovar, void *buf, int len)
{
	if (wlu_iovar_get(irh, iovar, buf, len)) {
		printf("wlmIovarGet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarSet(const char *iovar, void *buf, int len)
{
	if (wlu_iovar_set(irh, iovar, buf, len)) {
		printf("wlmIovarSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarIntegerGet(const char *iovar, int *val)
{
	if (wlu_iovar_getint(irh, iovar, val)) {
		printf("wlmIovarIntegerGet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarIntegerSet(const char *iovar, int val)
{
	if (wlu_iovar_setint(irh, iovar, val)) {
		printf("wlmIovarIntegerSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarBufferGet(const char *iovar, void *param, int param_len, void **bufptr)
{
	if (wlu_var_getbuf(irh, iovar, param, param_len, bufptr)) {
		printf("wlmIovarBufferGet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmIovarBufferSet(const char *iovar, void *param, int param_len)
{
	if (wlu_var_setbuf(irh, iovar, param, param_len)) {
		printf("wlmIovarBufferSet: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmCga5gOffsetsSet(char* values, int len)
{
	if (len != CGA_5G_OFFSETS_LEN) {
		printf("wlmCga5gOffsetsSet() requires a %d-value array as a parameter\n",
		      CGA_5G_OFFSETS_LEN);
		return FALSE;
	}

	if ((wlu_var_setbuf(irh, "sslpnphy_cga_5g", values,
	                    CGA_5G_OFFSETS_LEN * sizeof(int8))) < 0) {
		printf("wlmCga5gOffsetsSet(): Error setting offset values (%s)\n",  wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmCga5gOffsetsGet(char* buf, int len)
{
	if (len != CGA_5G_OFFSETS_LEN) {
		printf("wlmCga5gOffsetsGet() requires a %d-value array as a parameter\n",
		       CGA_5G_OFFSETS_LEN);
		return FALSE;
	}

	if ((wlu_iovar_get(irh, "sslpnphy_cga_5g", buf, CGA_5G_OFFSETS_LEN * sizeof(int8))) < 0) {
		printf("wlmCga5gOffsetsGet(): Error setting offset values (%s)\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int wlmCga2gOffsetsSet(char* values, int len)
{
	if (len != CGA_2G_OFFSETS_LEN) {
		printf("wlmCga2gOffsetsSet(): requires a %d-value array as a parameter\n",
		       CGA_2G_OFFSETS_LEN);
		return FALSE;
	}

	if ((wlu_var_setbuf(irh, "sslpnphy_cga_2g", values,
	                    CGA_2G_OFFSETS_LEN * sizeof(int8))) < 0) {
		printf("wlmCga2gOffsetsSet(): Error setting offset values (%s)\n", wlmLastError());
		return FALSE;
	}

	return TRUE;

}

int wlmCga2gOffsetsGet(char* buf, int len)
{
	if (len != CGA_2G_OFFSETS_LEN) {
		printf("wlmCga2gOffsetsGet(): requires a %d-value array as a parameter\n",
		       CGA_2G_OFFSETS_LEN);
		return FALSE;
	}

	if ((wlu_iovar_get(irh, "sslpnphy_cga_2g", buf, CGA_2G_OFFSETS_LEN * sizeof(int8))) < 0) {
		printf("wlmCga2gOffsetsGet(): Error setting offset values (%s)\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmNvramVarGet(const char *varname, char *buffer, int len)
{
	strcpy(buffer, varname);
	if (wlu_get(irh, WLC_NVRAM_GET, buffer, len) < 0) {
		printf("wlmNvramVarGet: Error getting \"%s\" value(%s)\n",
		       varname, wlmLastError());
		return FALSE;
	}
	return TRUE;
}
#ifdef SERDOWNLOAD
/* parsed values are the filename of the firmware and the args file */
int wlmDhdDownload(const char* firmware, const char* vars)
{
	char *args[3] = {0};
	bool ret;

	if (!firmware) {
		printf("Missing firmware path/filename\n");
		return FALSE;
	}

	if (!vars) {
		printf("Missing vars file\n");
		return FALSE;
	}

	args[0] = CALLOC(sizeof(char*) * (strlen(" ")+1));
	args[1] = CALLOC(sizeof(char*) * (strlen(firmware)+1));
	args[2] = CALLOC(sizeof(char*) * (strlen(vars)+1));

	if (args[0] == NULL || args[1] == NULL || args[2] == NULL) {
		ret = FALSE;
		printf("Malloc failures, aborting download\n");
		goto cleanup;
	}

	strncpy(args[0], " ", strlen(" "));
	strncpy(args[1], firmware, strlen(firmware));
	strncpy(args[2], vars, strlen(vars));

	printf("downloading firmware...\n");

	if (rwl_download(irh, 0, (char **) args)) {
		printf("wlmDhdDownload for firmware %s and vars file %s failed\n", firmware, vars);
		ret = FALSE;
	} else {
		printf("firmware download complete\n");
		ret = TRUE;
	}

cleanup:
	    if (args[0] != NULL)
		    free(args[0]);
	    if (args[1] != NULL)
		    free(args[1]);
	    if (args[2] != NULL)
		    free(args[2]);

	    return ret;

}

/* only parsed argv value is the chip string */

int wlmDhdInit(const char *chip)
{
	bool ret;
	char *args[2] = {0};

	args[0] = CALLOC(sizeof(char*) * (strlen(" ")+1));
	args[1] = CALLOC(sizeof(char*) * (strlen(chip)+1));

	if (args[0] == NULL || args[1] == NULL) {
		ret = FALSE;
		printf("wlmDhdInit: Malloc failures, aborting download\n");
		goto cleanup;
	}

	strncpy(args[0], " ", strlen(" "));
	strncpy(args[1], chip, strlen(chip));

	printf("wlmDhdInit: initializing firmware download...\n");

	if (dhd_init(irh, 0, (char **)args)) {
		printf("wlmDhdInit for chip %s failed\n", chip);
		ret = FALSE;
	}
	    ret = TRUE;

cleanup:
	    if (args[0] != NULL)
		    free(args[0]);
	    if (args[1] != NULL)
		    free(args[1]);

	    return ret;
}
#endif /* SERDOWNLOAD */

int wlmRadioOn(void)
{
	int val;

	/* val = WL_RADIO_SW_DISABLE << 16; */
	val = (1<<0) << 16;

	if (wlu_set(irh, WLC_SET_RADIO, &val, sizeof(int)) < 0) {
		printf("wlmRadioOn: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRadioOff(void)
{
	int val;

	/* val = WL_RADIO_SW_DISABLE << 16 | WL_RADIO_SW_DISABLE; */
	val = (1<<0) << 16 | (1<<0);

	if (wlu_set(irh, WLC_SET_RADIO, &val, sizeof(int)) < 0) {
		 printf("wlmRadioOff: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPMmode(int val)
{
	if (val < 0 || val > 2) {
	        printf("wlmPMmode: setting for PM mode out of range [0,2].\n");
		/* 0: CAM constant awake mode */
		/* 1: PS (Power save) mode */
		/* 2: Fast PS mode */
	}

	if (wlu_set(irh, WLC_SET_PM, &val, sizeof(int)) < 0) {
		printf("wlmPMmode: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamingOn(void)
{
	if (wlu_iovar_setint(irh, "roam_off", 0) < 0) {
		printf("wlmRoamingOn: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamingOff(void)
{
	if (wlu_iovar_setint(irh, "roam_off", 1) < 0) {
		printf("wlmRoamingOff: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRoamTriggerLevelGet(int *val, WLM_BAND band)
{
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = -1;

	if (wlu_get(irh, WLC_GET_ROAM_TRIGGER, &x, sizeof(x)) < 0) {
		printf("wlmRoamTriggerLevelGet: %s\n", wlmLastError());
		return FALSE;
	}

	*val = htod32(x.val);

	return TRUE;
}

int wlmRoamTriggerLevelSet(int val, WLM_BAND band)
{
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = htod32(val);

	if (wlu_set(irh, WLC_SET_ROAM_TRIGGER, &x, sizeof(x)) < 0) {
		printf("wlmRoamTriggerLevelSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmFrameBurstOn(void)
{
	int val = 1;

	val = htod32(val);
	if (wlu_set(irh, WLC_SET_FAKEFRAG, &val, sizeof(int)) < 0) {
		printf("wlmFrameBurstOn: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmFrameBurstOff(void)
{
	int val = 0;

	val = htod32(val);
	if (wlu_set(irh, WLC_SET_FAKEFRAG, &val, sizeof(int)) < 0) {
		printf("wlmFrameBurstOff: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmBeaconIntervalSet(int val)
{
	val = htod32(val);
	if (wlu_set(irh, WLC_SET_BCNPRD, &val, sizeof(int)) < 0) {
		printf("wlmBeaconIntervalSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmAMPDUModeSet(int val)
{
	val = htod32(val);
	if (wlu_iovar_setint(irh, "ampdu", val) < 0) {
		printf("wlmAMPDUModeSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmAMPDUSendAddBaSet(int tid, const char* macAddr)
{
	int i = 0;
	int ret = BCME_ERROR;
	char *args[WLM_NUM_ARGS] = {0};
	cmd_t command = { "ampdu_send_addba", wl_ampdu_send_addba, WLC_GET_VAR, WLC_SET_VAR, "" };

	args[i++] = "ampdu_send_addba";
	CALLOC_ARG_MEM(args[i], WLM_MAX_INT2STR_SIZE)
	sprintf(args[i++], "%d", tid);
	CALLOC_ARG_MEM(args[i], WLM_MAX_ARG_LEN)
	snprintf(args[i], WLM_MAX_ARG_LEN, "%s", macAddr);

	ret = wl_ampdu_send_addba(irh, &command, args);
	if (ret != BCME_OK) {
		printf("wlmAMPDUSendAddBaSet: failed with ret = %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	i = 1;
	while (args[i] != NULL) {
		free(args[i]);
		i++;
	}

	return ret;
}

int wlmMIMOBandwidthCapabilitySet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "mimo_bw_cap", val) < 0) {
		printf("wlmMIMOBandwidthCapabilitySet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmInterferenceSet(int val)
{
	val = htod32(val);

	if (val < 0 || val > 4) {
		printf("wlmInterferenceSet: interference setting out of range [0, 4]\n");
		return FALSE;
	}

	if (wlu_set(irh, WLC_SET_INTERFERENCE_MODE, &val, sizeof(int)) < 0) {
		printf("wlmInterferenceSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmInterferenceOverrideSet(int val)
{
	val = htod32(val);

	if (val < 0 || val > 4) {
		printf("wlmInterferenceOverrideSet: interference setting out of range [0, 4]\n");
		return FALSE;
	}

	if (wlu_set(irh, WLC_SET_INTERFERENCE_OVERRIDE_MODE, &val, sizeof(int)) < 0) {
		printf("wlmInterferenceOverrideSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTransmitBandwidthSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "mimo_txbw", val) < 0) {
		printf("wlmTransmitBandwidthSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmShortGuardIntervalSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "sgi_tx", val) < 0) {
		printf("wlmShortGuardIntervalSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmShortGuardIntervalRxSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "sgi_rx", val) < 0) {
		printf("wlmShortGuardIntervalRxSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmObssCoexSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "obss_coex", val) < 0) {
		printf("wlmObssCoexSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYPeriodicalCalSet(void)
{
	if (wlu_iovar_setint(irh, "phy_percal", 0) < 0) {
		printf("wlmPHYPeriodicalCalSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYForceCalSet(void)
{
	if (wlu_iovar_setint(irh, "phy_forcecal", 0) < 0) {
		printf("wlmPHYForceCalSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYForceCalNoiseSet(void)
{
	/* does only noise cal */
	if (wlu_iovar_setint(irh, "phy_forcecal_noise", 1) < 0) {
		printf("wlmPHYForceCalNoiseSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYScramblerUpdateDisable(void)
{
	if (wlu_iovar_setint(irh, "phy_scraminit", 0x7f) < 0) {
		printf("wlmPHYScramblerUpdateDisable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYScramblerUpdateEnable(void)
{
	if (wlu_iovar_setint(irh, "phy_scraminit", -1) < 0) {
		printf("wlmPHYScramblerUpdateEnable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPHYWatchdogSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "phy_watchdog", val) < 0) {
	        printf("wlmPHYWatchdogSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTemperatureSensorDisable(void)
{
	int val = 1; /* 0 = temp sensor enabled; 1 = temp sensor disabled */
	if (wlu_iovar_setint(irh, "tempsense_disable", val) < 0) {
		printf("wlmTempSensorDisable %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTemperatureSensorEnable(void)
{
	int val = 0; /* 0 = temp sensor enabled; 1 = temp sensor disabled */
	if (wlu_iovar_setint(irh, "tempsense_disable", val) < 0) {
		printf("wlmTempSensorEnable %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTransmitCoreSet(int core, int streams)
{
	uint32 coremask[2] = {0, 0};
	uint8 mcs_mask[4] = {0, 0, 0, 0}; /* pre-initialize # of streams {core:4 | stream:4} */
	uint8 idx;
	uint8 cck_mask = 0;
	uint8 ofdm_mask = 0;

	core = htod32(core);

	core = core & 0x0f;
	cck_mask = ofdm_mask = core;  /* assume cck, ofdm and mcs core mask are the same */
	core = core << 4;
	if (core == 0) {
		printf("wlmTransitcoreSet, %1d-stream core cannot be zero\n",  streams);
		return TRUE;
	}

	streams = (streams & 0x0f);
	if (streams > 4)  {
		printf("wlmTransmitCoreSet: Nsts > 4\n");
		return FALSE;
	}

	idx = streams - 1;
	mcs_mask[idx] = (uint8)(core|streams);

	coremask[0] |= mcs_mask[0] << 0;
	coremask[0] |= mcs_mask[1] << 8;
	coremask[0] |= mcs_mask[2] << 16;
	coremask[0] |= mcs_mask[3] << 24;

	coremask[1] |= cck_mask;

	coremask[1] |= ofdm_mask << 8;

	if (wlu_var_setbuf(irh, "txcore", coremask, sizeof(uint32) * 2) < 0) {
		printf("wlmTransmitCoreSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPhyTempSenseGet(int *val)
{
	if (wlu_iovar_getint(irh, "phy_tempsense", val) < 0) {
		printf("wlmPhyTempSenseGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmOtpFabidGet(int *val)
{
	if (wlu_iovar_getint(irh, "otp_fabid", val)) {
		printf("wlmOtpFabid: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

/* Defined located in src/wl/bcmwifi/src/bcmwifi_channels.c */
/* 40MHz channels in 5GHz band */
static const uint8 wf_5g_40m_chans[] =
	{38, 46, 54, 62, 102, 110, 118, 126, 134, 142, 151, 159};
#define WF_NUM_5G_40M_CHANS \
	(sizeof(wf_5g_40m_chans)/sizeof(uint8))

/* 80MHz channels in 5GHz band */
static const uint8 wf_5g_80m_chans[] =
	{42, 58, 106, 122, 138, 155};
#define WF_NUM_5G_80M_CHANS \
	(sizeof(wf_5g_80m_chans)/sizeof(uint8))

/* 160MHz channels in 5GHz band */
static const uint8 wf_5g_160m_chans[] =
	{50, 114};
#define WF_NUM_5G_160M_CHANS \
	(sizeof(wf_5g_160m_chans)/sizeof(uint8))

static uint8
center_chan_to_edge(uint bw)
{
	/* edge channels separated by BW - 10MHz on each side
	 * delta from cf to edge is half of that,
	 * MHz to channel num conversion is 5MHz/channel
	 */
	return (uint8)(((bw - 20) / 2) / 5);
}

/* return channel number of the low edge of the band
 * given the center channel and BW
 */
static uint8
channel_low_edge(uint center_ch, uint bw)
{
	return (uint8)(center_ch - center_chan_to_edge(bw));
}

/* return side band number given center channel and control channel
 * return -1 on error
 */
static int
channel_to_sb(uint center_ch, uint ctl_ch, uint bw)
{
	uint lowest = channel_low_edge(center_ch, bw);
	uint sb;

	if ((ctl_ch - lowest) % 4) {
		/* bad ctl channel, not mult 4 */
		return -1;
	}

	sb = ((ctl_ch - lowest) / 4);

	/* sb must be a index to a 20MHz channel in range */
	if (sb >= (bw / 20)) {
		/* ctl_ch must have been too high for the center_ch */
		return -1;
	}

	return sb;
}

int
wlmChannelSpecSet(int channel, int bandwidth, int sideband)
{
	chanspec_t chanspec = 0;
	uint chspec_ch, chspec_band, chspec_bw, chspec_sb;
	uint ctl_ch;

	/* set current band status */
	if ((channel <= 14) && (curBand != WLM_BAND_2G)) {
		curBand = WLM_BAND_2G;
	} else if ((channel > 14) && (curBand != WLM_BAND_5G)) {
		curBand = WLM_BAND_5G;
	}

	/* Set control channel */
	if ((uint)channel > MAXCHANNEL) {
		printf("wlmChannelSpecSet: %d ivalid channel exceed %d\n",
		       channel, MAXCHANNEL);
		return FALSE;
	}

	ctl_ch = channel;

	/* Set band */
	if ((uint)channel > CH_MAX_2G_CHANNEL)
		chspec_band = WL_CHANSPEC_BAND_5G;
	else
		chspec_band = WL_CHANSPEC_BAND_2G;

	/* Set bandwidth */
	if (bandwidth == 20)
		chspec_bw = WL_CHANSPEC_BW_20;
	else if (bandwidth == 40)
		chspec_bw = WL_CHANSPEC_BW_40;
	else if (bandwidth == 80)
		chspec_bw = WL_CHANSPEC_BW_80;
	else if (bandwidth == 160)
		chspec_bw = WL_CHANSPEC_BW_160;
	else if (bandwidth == 8080)
		chspec_bw = WL_CHANSPEC_BW_8080;
	else {
		printf("wlmChannelSpecSet: %d is invalid channel bandwith,"
		       "must be 20, 40, 80, 160 or 8080\n", bandwidth);
		return FALSE;
	}

	/* Set side band  */
	/* Sideband 1 or -1, gurantee 40Mhz channel */
	if ((bandwidth == 20) || (bandwidth == 10)) {
		chspec_sb = 0;
		chspec_ch = ctl_ch;
	}
	else if ((bandwidth == 40) && (sideband == -1)) {
		chspec_sb = WL_CHANSPEC_CTL_SB_LLL;
		chspec_ch = UPPER_20_SB(ctl_ch);
	}
	else if ((bandwidth == 40) && (sideband == 1)) {
		chspec_sb = WL_CHANSPEC_CTL_SB_LLU;
		chspec_ch = LOWER_20_SB(ctl_ch);
	}
	else if ((bandwidth == 80) || (bandwidth == 160)) {
		/* figure out ctl sideband based on ctl channel and bandwidth */
		const uint8 *center_ch = NULL;
		int num_ch = 0;
		int sb = -1;
		int i;

		if (chspec_bw == WL_CHANSPEC_BW_80) {
			center_ch = wf_5g_80m_chans;
			num_ch = WF_NUM_5G_80M_CHANS;
		}
		else if (chspec_bw == WL_CHANSPEC_BW_160) {
			center_ch = wf_5g_160m_chans;
			num_ch = WF_NUM_5G_160M_CHANS;
		} else {
			printf("wlmChannelSpecSet: unsupported channel bandwidth %d\n", bandwidth);
			return FALSE;
		}

		for (i = 0; i < num_ch; i++) {
			sb = channel_to_sb(center_ch[i], channel, bandwidth);
			if (sb >= 0) {
				chspec_ch = center_ch[i];
				chspec_sb = sb << WL_CHANSPEC_CTL_SB_SHIFT;
				break;
			}
		}

		if (sb < 0) {
			printf("wlmChannelSpecSet:"
			       "Can't find valid side band for channel %d\n", channel);
			return FALSE;
		}
	}
	else {
		/* Otherwise, 80+80 */
		printf("wlmChannelSpecSet: Wrong bandwdith."
		       "For 80+80 chanspec, call wlm80Plus80ChannelSpecSet() instead.\n");
		return FALSE;
	}

	/* Set channel */
	chanspec = chspec_band | chspec_ch |chspec_bw | chspec_sb;

	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			printf("wlmChannelSpecSet: Invalid chanspec 0x%4x\n", chanspec);
			return FALSE;
		}
	}

	if (wlu_iovar_setint(irh, "chanspec", (int) chanspec) < 0) {
		printf("wlmChannelSpecSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

static int
channel_80mhz_to_id(uint ch)
{
	uint i;
	for (i = 0; i < WF_NUM_5G_80M_CHANS; i ++) {
		if (ch == wf_5g_80m_chans[i])
			return i;
	}

	return -1;
}

int
wlm80Plus80ChannelSpecSet(int ch1, int ch2)
{
	chanspec_t chanspec = 0;
	uint chspec_ch, chspec_band, chspec_bw, chspec_sb, ctl_ch;
	int ch1_id = 0, ch2_id = 0;
	int bw, sb;

	if ((ch1 < ch2) && ((uint)ch1 > CH_MAX_2G_CHANNEL))
		chspec_band = WL_CHANSPEC_BAND_5G;
	else {
		printf("wlm80Plus80ChannelSpecSet: Invalid channels. "
		       "Both channels must be on 5G band"
		       "and first channel should be lower than second channel\n");
		return FALSE;
	}

	ch1_id = channel_80mhz_to_id(ch1);
	ch2_id = channel_80mhz_to_id(ch2);
	ctl_ch = ch1; /* first channel is control channel? */

	/* validate channels */
	if (ch1 >= ch2 || ch1_id < 0 || ch2_id < 0) {
		printf("wlm80Plus80ChannelSpecSet: Invalid channels.\n");
		return FALSE;
	}

	/* combined channel in chspec */
	chspec_ch = (((uint16)ch1_id << WL_CHANSPEC_CHAN0_SHIFT) |
	    ((uint16)ch2_id << WL_CHANSPEC_CHAN1_SHIFT));

	/* set channel bandwidth */
	bw = 80;
	chspec_bw = WL_CHANSPEC_BW_8080;

	/* does the primary channel fit with the 1st 80MHz channel ? */
	sb = channel_to_sb(ch1, ctl_ch, bw);
	if (sb < 0) {
		/* no, so does the primary channel fit with the 2nd 80MHz channel ? */
		sb = channel_to_sb(ch2, ctl_ch, bw);
		if (sb < 0) {
			/* no match for ctl_ch to either 80MHz center channel */
			return 0;
		}
		/* sb index is 0-3 for the low 80MHz channel, and 4-7 for
		 * the high 80MHz channel. Add 4 to to shift to high set.
		 */
		sb += 4;
	}

	chspec_sb = sb << WL_CHANSPEC_CTL_SB_SHIFT;
	chanspec = chspec_band | chspec_ch |chspec_bw | chspec_sb;

	if (wlu_iovar_setint(irh, "chanspec", (int) chanspec) < 0) {
		printf("wlmChannelSpecSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRtsThresholdOverride(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "rtsthresh", val) < 0) {
	        printf("wlmRtsThresholdOverride: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSTBCTxSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "stbc_tx", val) < 0) {
	        printf("wlmSTBCTxSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmSTBCRxSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "stbc_rx", val) < 0) {
	        printf("wlmSTBCRxSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmTxChainSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "txchain", val) < 0) {
	        printf("wlmTxChainSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRxChainSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "rxchain", val) < 0) {
	        printf("wlmRxChainSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmRxIQEstGet(float *val, int sampleCount, int ant)
{
	uint32 rxiq;
	int sample_count = sampleCount;  /* [0, 16], default: maximum 15 sample counts */
	int antenna = ant ;       /* [0, 3], default: antenna 0 */
	uint8 resolution = 1;     /* resolution default to 0.25dB */
	uint8 lpf_hpc = 1;
	uint8 gain_correct = 0;
	uint8 elna = 0;
	float x, y;

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * elna (shared LSB with lpf_hpc) = 1
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 * index = 0x78
	 */
	/* BitMap:
	   7:0 Antenna for all phy's except lcn/lcn40
	   6:0 Antenna for Lcn40
	   7   Gain Index Valid for Lcn/Lcn40
	   15:8 Samples
	   19:16 Resolution
	   23:20 lpf_hpc
	   20: Elna On/off for Lcn/Lcn40
	   23:21 Index[2:0] for Lcn/Lcn40
	   27:24 gain_correct
	   31:28 Index[6:3] for Lcn/Lcn40
	*/

	rxiq = (0xF << 28) | (gain_correct << 24) | (lpf_hpc << 20) |
		(resolution << 16) | (10 << 8) | 3;
	/* printf("wlmRxIQGet: initial rxiq = 0x%x\n.", rxiq); */

	if (gain_correct > 1) {
		printf("wlmRxIQEstGet: invalid gain-correction select [0|1]\n");
		return FALSE;
	} else {
		gain_correct = gain_correct & 0xf;
		rxiq = ((gain_correct << 24) | (rxiq & 0xf0ffffff));
	}
	/* printf("wlmRxIQEstGet: rxiq after gain_correct = 0x%x\n.", rxiq); */

	if (lpf_hpc > 1) {
		printf("wlmRXIQEstGet: invalid lpf-hpc override select [0|1].\n");
		return FALSE;
	} else {
		lpf_hpc = lpf_hpc & 0xf;
		rxiq = ((lpf_hpc << 20) | (rxiq & 0xff0fffff));
	}

	if (elna > 1) {
		printf("wlmRXIQEstGet: invalid elna on/off select [0|1].\n");
		return FALSE;
	} else {
		elna = elna & 0x1;
		rxiq = ((elna << 20) | (rxiq & 0xffefffff));
	}
	/* printf("wlmRxIQGet: rxiq after lpf-hpc = 0x%x\n.", rxiq); */

	if (resolution > 1) {
		printf("wlmRxIQEstGet: invalid resolution [0|1].\n");
		return FALSE;
	} else {
		resolution = resolution & 0xf;
		rxiq = ((resolution << 16) | (rxiq & 0xfff0ffff));
	}
	/* printf("wlmRxIQGet: rxiq after resolution = 0x%x\n.", rxiq); */

	if ((sample_count < 0) || (sample_count > 15)) {
		printf("wlmRxIQEstGet: SampleCount out of range of [0, 15].\n");
		return FALSE;
	} else {
		/* printf("wlmRxIQEstGet: SampleCount %d\n", sample_count); */
		rxiq = (((sample_count & 0xff) << 8) | (rxiq & 0xffff00ff));
	}
	/* printf("wlmRxIQGet: rxiq after sample count = 0x%x\n.", rxiq); */

	if ((antenna < 0) || (antenna > 3)) {
		printf("wlmRxIQEstGet: Antenna out of range of [0, 3].\n");
		return FALSE;
	} else {
		rxiq = ((rxiq & 0xffffff00) | (antenna & 0xff));
	}

	/*
	printf("wlmRxIQGet: rxiq after antenna = 0x%x\n.", rxiq);
	printf("wlmRxIQGet: rxiq = 0x%x before wlu_iovar_setint().\n", rxiq);
	*/
	if ((wlu_iovar_setint(irh, "phy_rxiqest", (int) rxiq) < 0)) {
		printf("wlmRxIQEstGet: %s\n", wlmLastError());
		return FALSE;
	}

	if ((wlu_iovar_getint(irh, "phy_rxiqest", (int*)&rxiq) < 0)) {
		printf("wlmRxIQEstGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* printf("wlmRxIQGet: rxiq = 0x%x after wlu_iovar_getint\n.", rxiq); */
	if (resolution == 1) {
		/* fine resolutin power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;
		/*
		  printf("wlmRxIQGet: inside resulution == 1 block, rxiq = 0x%x\n", rxiq);
		*/
		if (rxiq >> 20) {
			/* Three chains: return the core matches the antenna number */
			for (core = 0; core < 3; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >>6;  /* sign extension */
					break;
				}
			}
		} else if (rxiq >> 10) {
			/* Two chains: return the core matches the antenna number */
			for (core = 0; core < 2; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else {
			/* 1-chain specific */
			tmp = (rxiq & 0x3ff);
			tmp = ((int16)(tmp << 6)) >> 6; /* signed extension */
			/*
			  printf("wlmRxIQGet: single core, tmp 0x%x\n", tmp);
			  printf("wlmRxIQGet: tmp before processing 0x%x\n", tmp);
			*/
		}

		if (tmp < 0) {
			tmp = -1 * tmp;
		}
		x = (float) (tmp >> 2);
		y = (float) (tmp & 0x3);
		*val = (float)(x + (y * 25) /100) * (-1);
	} else {
		/* return the core matches the antenna number */
		*val = (float)((rxiq >> (8 *antenna)) & 0xff);
	}

	return TRUE;
}

int wlmRxIQEstExtGet(float *val, int sampleCount, int ant, int elna, int gi)
{
	uint32 rxiq = 0;
	int sample_count = sampleCount;  /* [0, 16], default: maximum 15 sample counts */
	int antenna = ant ;       /* [0, 3], default: antenna 0 */
	uint8 resolution = 1;     /* resolution default to 0.25dB */
	uint8 lpf_hpc = 1;
	uint8 dig_lpf = 1;
	uint8 gain_correct = 0;
	uint8 extra_gain_3dBsteps = 0;
	uint8 force_gain_type = 0;
	float x, y;

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * dig_lpf = 1; (sets to ltrn_lpf mode)
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 * force_gain_type = 0 (init gain mode)
	 */

	rxiq = (extra_gain_3dBsteps << 28) | (gain_correct << 24) | (dig_lpf << 22)
		| (lpf_hpc << 20) | (resolution << 16) | (10 << 8) | (force_gain_type << 4)
		| ant;
	/* printf("wlmRxIQGet: initial rxiq = 0x%x\n.", rxiq); */

	if (gain_correct > 1) {
		printf("wlmRxIQEstExtGet: invalid gain-correction select [0|1]\n");
		return FALSE;
	} else {
		gain_correct = gain_correct & 0xf;
		rxiq = ((gain_correct << 24) | (rxiq & 0xf0ffffff));
	}
	/* printf("wlmRxIQGet: rxiq after gain_correct = 0x%x\n.", rxiq); */

	if (lpf_hpc > 1) {
		printf("wlmRXIQEstExtGet: invalid lpf-hpc override select [0|1].\n");
		return FALSE;
	} else {
		lpf_hpc = lpf_hpc & 0xf;
		rxiq = ((lpf_hpc << 20) | (rxiq & 0xff0fffff));
	}
	/* printf("wlmRxIQGet: rxiq after lpf_hpc = 0x%x\n.", rxiq); */

	if (dig_lpf > 2) {
		printf("wlmRXIQEstExtGet: invalid dig_lpf override select [0|1].\n");
		return FALSE;
	} else {
		dig_lpf = dig_lpf & 0x3;
		rxiq = ((dig_lpf << 22) | (rxiq & 0xff3fffff));
	}
	/* printf("wlmRxIQGet: rxiq after dig_lpf = 0x%x\n.", rxiq); */

	if (resolution > 1) {
		printf("wlmRxIQEstExtGet: invalid resolution [0|1].\n");
		return FALSE;
	} else {
		resolution = resolution & 0xf;
		rxiq = ((resolution << 16) | (rxiq & 0xfff0ffff));
	}
	/* printf("wlmRxIQGet: rxiq after resolution = 0x%x\n.", rxiq); */

	if ((sample_count < 0) || (sample_count > 16)) {
		printf("wlmRxIQEstExtGet: SampleCount out of range of [0, 15].\n");
		return FALSE;
	} else {
		/* printf("wlmRxIQGet: sample_count %d\n.", sample_count); */
		rxiq = (((sample_count & 0xff) << 8) | (rxiq & 0xffff00ff));
	}
	/* printf("wlmRxIQGet: rxiq after sample count = 0x%x\n.", rxiq); */

	if ((antenna < 0) || (antenna > 3)) {
		printf("wlmRxIQEstExtGet: Antenna out of range of [0, 3].\n");
		return FALSE;
	} else {
		rxiq = ((rxiq & 0xfffffff0) | (antenna & 0xf));
	}
	/* printf("wlmRxIQGet: rxiq after antenna = 0x%x\n.", rxiq); */

	if ((extra_gain_3dBsteps > 24) || (extra_gain_3dBsteps % 3 != 0)) {
		printf("wlmRXIQESTExtGet: invalid extra INITgain, should be {0, 3, .., 21, 24}\n");
		return FALSE;
	} else {
		rxiq = ((((extra_gain_3dBsteps /3) & 0xf) << 28) | (rxiq & 0x0fffffff));
	}

	if (elna > 1) {
		printf("wlmRXIQEstExtGet: invalid elna on/off select [0|1].\n");
		return FALSE;
	} else {
		elna = elna & 0x1;
		rxiq = ((elna << 20) | (rxiq & 0xffefffff));
	}
	/* printf("wlmRxIQGet: rxiq after elna = 0x%x\n.", rxiq); */

	if ((gi != 0) && (gi != 1) && (gi != 4)) {
		printf("wlmRXIQEstExtGet: gain index option {0,1, 4}\n");
		return FALSE;
	} else {
		rxiq = ((rxiq & 0xffffff0f) | ((gi << 4) & 0xf0));
	}

	/* printf("wlmRxIQGet: rxiq before set gain index = 0x%x\n.", rxiq); */
	if ((wlu_iovar_setint(irh, "phy_rxiqest", (int) rxiq) < 0)) {
		printf("wlmRxIQEstExtGet: %s\n", wlmLastError());
		return FALSE;
	}

	if ((wlu_iovar_getint(irh, "phy_rxiqest", (int*)&rxiq) < 0)) {
		printf("wlmRxIQEstExtGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* printf("wlmRxIQGet: after getint 0x%x\n.", rxiq); */

	if (resolution == 1) {
		/* fine resolutin power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;

		/* printf("wlmRxIQGet: inside resulution == 1 block, rxiq = 0x%x\n", rxiq); */

		if (rxiq >> 20) {
			/* Three chains: return the core matches the antenna number */
			for (core = 0; core < 3; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else if (rxiq >> 10) {
			/* Two chains: return the core matches the antenna number */
			for (core = 0; core < 2; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else {
			/* 1-chain specific */
			tmp = (rxiq & 0x3ff);
			tmp = ((int16)(tmp << 6)) >> 6; /* signed extension */
		}

		/* printf("wlmRxIQGet: core %d, tmp 0x%x\n", core, tmp); */
		if (tmp < 0) {
			tmp = -1 * tmp;
		}
		x = (float) (tmp >> 2);
		y = (float) (tmp & 0x3);
		*val = (float)(x + (y * 25) /100) * (-1);
	} else {
		/* return the core matches the antenna number */
		*val = (float)((rxiq >> (8 *antenna)) & 0xff);
	}

	return TRUE;
}

int wlmRxIQEstACGet(float *val, int sampleCount, int ant, int extragain, int gi, int gain_correct)
{
	uint32 rxiq = 0;
	int sample_count = sampleCount;  /* [0, 16], default: maximum 15 sample counts */
	uint8 antenna = (uint8)ant ;       /* [0, 3], default: antenna 0 */
	uint8 resolution = 1;     /* resolution default to 0.25dB */
	uint8 lpf_hpc = 1;
	uint8 dig_lpf = 1;
	uint8 extra_gain_3dBsteps = 0;
	uint8 force_gain_type = 0;
	float x, y;

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * dig_lpf = 1; (sets to ltrn_lpf mode)
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 * force_gain_type = 0 (init gain mode)
	 */

	rxiq = (extra_gain_3dBsteps << 28) | (gain_correct << 24) | (dig_lpf << 22)
		| (lpf_hpc << 20) | (resolution << 16) | (10 << 8) | (force_gain_type << 4)
		| ant;
	/* printf("wlmRxIQEstACGet: initial rxiq = 0x%x\n.", rxiq); */

	if (gain_correct > 8) {
		printf("wlmRxIQEstACGet: invalid gain-correction select [0|1|2|3|4|7|8]\n");
		return FALSE;
	} else {
		gain_correct = gain_correct & 0xf;
		rxiq = ((gain_correct << 24) | (rxiq & 0xf0ffffff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after gain_correct = 0x%x\n.", rxiq); */

	if (lpf_hpc > 1) {
		printf("wlmRxIQEstACGet: invalid lpf-hpc override select [0|1].\n");
		return FALSE;
	} else {
		lpf_hpc = lpf_hpc & 0xf;
		rxiq = ((lpf_hpc << 20) | (rxiq & 0xff0fffff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after lpf_hpc = 0x%x\n.", rxiq); */

	if (dig_lpf > 2) {
		printf("wlmRxIQEstACGet: invalid dig_lpf override select [0|1].\n");
		return FALSE;
	} else {
		dig_lpf = dig_lpf & 0x3;
		rxiq = ((dig_lpf << 22) | (rxiq & 0xff3fffff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after dig_lpf = 0x%x\n.", rxiq); */

	if (resolution > 1) {
		printf("wlmRxIQEstACGet: invalid resolution [0|1].\n");
		return FALSE;
	} else {
		resolution = resolution & 0xf;
		rxiq = ((resolution << 16) | (rxiq & 0xfff0ffff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after resolution = 0x%x\n.", rxiq); */

	if ((sample_count < 0) || (sample_count > 16)) {
		printf("wlmRxIQEstACGetwlmRxIQEstExtGet: SampleCount out of range of [0, 15].\n");
		return FALSE;
	} else {
		/* printf("wlmRxIQEstACGet: sample_count %d\n.", sample_count); */
		rxiq = (((sample_count & 0xff) << 8) | (rxiq & 0xffff00ff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after sample count = 0x%x\n.", rxiq); */

	if (antenna > 3) {
		printf("wlmRxIQEstACGet: Antenna out of range of [0, 3].\n");
		return FALSE;
	} else {
		rxiq = ((rxiq & 0xffffff00) | (antenna & 0xff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after antenna = 0x%x\n.", rxiq); */

	if ((extragain < 0) || (extragain > 24) || (extragain % 3 != 0)) {
		printf("wlmRXIQEstACGet: valid extra INITgain ={0,3, ..., 21,24}\n");
		return FALSE;
	} else {
		rxiq = ((((extragain/3) & 0xf) << 28) | (rxiq & 0x0fffffff));
	}
	/* printf("wlmRxIQEstACGet: rxiq after elna = 0x%x\n.", rxiq); */

	if ((gi < 0) || (gi > 9))  {
		printf("wlmRxIQEstACGet: gain index option {0,1, 4}\n");
		return FALSE;
	} else {
		rxiq = ((rxiq & 0xffffff0f) | ((gi << 4) & 0xf0));
	}

	/* printf("wlmRxIQEstACGet: rxiq before set gain index = 0x%x\n.", rxiq); */
	if ((wlu_iovar_setint(irh, "phy_rxiqest", (int) rxiq) < 0)) {
		printf("wlmRxIQEstACGet: %s\n", wlmLastError());
		return FALSE;
	}

	if ((wlu_iovar_getint(irh, "phy_rxiqest", (int*)&rxiq) < 0)) {
		printf("wlmRxIQEstACGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* printf("wlmRxIQEstACGet: after getint 0x%x\n.", rxiq); */

	if (resolution == 1) {
		/* fine resolutin power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;

		/* printf("wlmRxIQEstACGet: inside resulution == 1 block, rxiq = 0x%x\n", rxiq); */

		if (rxiq >> 20) {
			/* Three chains: return the core matches the antenna number */
			if (antenna > 2)
				antenna = 2;
			for (core = 0; core < 3; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else if (rxiq >> 10) {
			/* Two chains: return the core matches the antenna number */
			for (core = 0; core < 2; core++) {
				if (core == antenna) {
					tmp = (rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else {
			/* 1-chain specific */
			tmp = (rxiq & 0x3ff);
			tmp = ((int16)(tmp << 6)) >> 6; /* signed extension */
		}

		/* printf("wlmRxIQEstACGet: core %d, tmp 0x%x\n", core, tmp); */
		if (tmp < 0) {
			tmp = -1 * tmp;
		}
		x = (float) (tmp >> 2);
		y = (float) (tmp & 0x3);
		*val = (float)(x + (y * 25) /100) * (-1);
	} else {
		/* return the core matches the antenna number */
		*val = (float)((rxiq >> (8 *antenna)) & 0xff);
	}

	return TRUE;
}

#define NO_EXTRA_GAIN 0
#define NO_GAIN_CORRECTION 0
#define LPF_HPC_OVERRIDE 1
#define LTRN_LPF_MODE 1
#define COARSE_RESOLUTION 1
#define TEN_SAMPLES 10
#define INIT_GAIN_MODE 0
#define THIRD_ANTENA 3
#define SWEEP_API_ERROR "wlmRxIQEstSweepGet Error: "
#define SWEEP_IOVAR "phy_rxiqest_sweep"
static int wlmRxIQEstSweepGetFull(wlmRxIQEstSweepReult_t **result, int *channels, int niter,
	int delay, int sampleCount, int antenna, int extraGain, int isForLcn, int gainIndex,
	int gainCorrect, int elna)
{
	int i, j;
	char buf[sizeof(wl_iqest_result_t) + WL_NUMCHANNELS * sizeof(wl_iqest_value_t)];
	wl_iqest_sweep_params_t *sweep_params = (wl_iqest_sweep_params_t *)buf;
	wl_iqest_params_t *params = &sweep_params->params;
	wl_iqest_result_t *estimate;
	int params_size;
	float x, y;

	params->niter = niter;
	params->delay = delay;

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * dig_lpf = 1; (sets to ltrn_lpf mode)
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 * force_gain_type = 0 (init gain mode)
	 */

	params->rxiq = SHIFT_VAL(NO_EXTRA_GAIN, 28, 4) | SHIFT_VAL(NO_GAIN_CORRECTION, 24, 4) |
		SHIFT_VAL(LTRN_LPF_MODE, 22, 2) | SHIFT_VAL(LPF_HPC_OVERRIDE, 20, 4) |
		SHIFT_VAL(COARSE_RESOLUTION, 16, 4) | SHIFT_VAL(TEN_SAMPLES, 8, 8) |
		SHIFT_VAL(INIT_GAIN_MODE, 4, 4) | SHIFT_VAL(THIRD_ANTENA, 0, 2);

	if (isForLcn)
		params->rxiq = BIT_OR(params->rxiq, 0x0f, 28, 4);
	else {
		if (extraGain < 0 || extraGain > 24 || extraGain % 3 != 0) {
			printf(SWEEP_API_ERROR "valid extra INITgain ={0,3, ..., 21,24}\n");
			return FALSE;
		}
		params->rxiq = BIT_OR(params->rxiq, extraGain/3, 28, 4);
	}

	if (gainCorrect < 0 || gainCorrect > 8) {
		printf(SWEEP_API_ERROR "invalid gain-correction. select [0 - 8]\n");
		return FALSE;
	}
	params->rxiq = BIT_OR(params->rxiq, gainCorrect, 24, 4);

	if (elna < 0 || elna > 1) {
		printf(SWEEP_API_ERROR "Invalid elna on/off select [0|1].\n");
		return FALSE;
	}
	params->rxiq = BIT_OR(params->rxiq, elna, 20, 1);

	if (sampleCount < 0 || sampleCount > 16) {
		printf(SWEEP_API_ERROR "SampleCount out of range of [0 - 16].\n");
		return FALSE;
	}
	params->rxiq = BIT_OR(params->rxiq, sampleCount, 8, 8);

	if ((gainIndex < 0) || (gainIndex > 9))  {
		printf(SWEEP_API_ERROR "Valid gain index options are: {0,1,4}\n");
		return FALSE;
	}
	params->rxiq = BIT_OR(params->rxiq, gainIndex, 4, 4);

	if (antenna < 0 || antenna > 3) {
		printf(SWEEP_API_ERROR "Antenna out of range of [0, 3].\n");
		return FALSE;
	}
	params->rxiq = BIT_OR(params->rxiq, antenna, 0, 4);

	if (!channels) {
		sweep_params->nchannels = 1;
		sweep_params->channel[0] = 0;
	} else {
		for (i = 0, sweep_params->nchannels = 0; i < WL_NUMCHANNELS && channels[i]; ++i) {
			for (j = 0; j < sweep_params->nchannels; ++j) {
				if (channels[i] == sweep_params->channel[j]) {
					printf(SWEEP_API_ERROR "Duplicate channels\n");
					return FALSE;
				}
			}
			sweep_params->channel[sweep_params->nchannels++] = (uint8) channels[i];
		}
		if (channels[i]) {
			printf(SWEEP_API_ERROR "Maximum of %d channels allowed\n", WL_NUMCHANNELS);
			return FALSE;
		}
	}
	if ((sweep_params->nchannels == 0 || sweep_params->nchannels > WL_NUMCHANNELS_MANY_CHAN) &&
		sweep_params->params.niter > WL_ITER_LIMIT_MANY_CHAN) {
		printf(SWEEP_API_ERROR "Maximum %d averaging iterations allowed if number"
				" of channel is greater than %d\n", WL_ITER_LIMIT_MANY_CHAN,
				WL_NUMCHANNELS_MANY_CHAN);
		return FALSE;
	}

	params_size = sizeof(wl_iqest_sweep_params_t) + (sweep_params->nchannels - 1)
		* sizeof(uint8);
	if (wlu_var_getbuf_med(irh, SWEEP_IOVAR, sweep_params, params_size, (void **)result) < 0) {
		printf(SWEEP_API_ERROR "%s\n", wlmLastError());
		return FALSE;
	}
	estimate = (wl_iqest_result_t *)*result;
	memcpy(buf, estimate, sizeof(wl_iqest_result_t) + (estimate->nvalues -1) *
		sizeof(wl_iqest_value_t));
	estimate = (wl_iqest_result_t *)buf;

	(*result)->numChannels = estimate->nvalues;
	for (i = 0; i < estimate->nvalues; ++i) {
		/* fine resolutin power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;

		(*result)->value[i].channel = estimate->value[i].channel;
		if (estimate->value[i].rxiq >> 20) {
			/* Three chains: return the core matches the antenna number */
			if (antenna > 2)
				antenna = 2;
			for (core = 0; core < 3; core++) {
				if (core == antenna) {
					tmp = (estimate->value[i].rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else if (estimate->value[i].rxiq >> 10) {
			/* Two chains: return the core matches the antenna number */
			for (core = 0; core < 2; core++) {
				if (core == antenna) {
					tmp = (estimate->value[i].rxiq >>(10*core)) & 0x3ff;
					tmp = ((int16)(tmp << 6)) >> 6;  /* sign extension */
					break;
				}
			}
		} else {
			/* 1-chain specific */
			tmp = (estimate->value[i].rxiq & 0x3ff);
			tmp = ((int16)(tmp << 6)) >> 6; /* signed extension */
		}

		if (tmp < 0) {
			tmp = -1 * tmp;
		}
		x = (float) (tmp >> 2);
		y = (float) (tmp & 0x3);
		(*result)->value[i].rxIqEst = (float)(x + (y * 25) /100) * (-1);
	}

	return TRUE;
}

int wlmRxIQEstSweepGet(wlmRxIQEstSweepReult_t **result, int *channels, int sampleCount, int antenna)
{
	return wlmRxIQEstSweepGetFull(result, channels, 1, PHY_RXIQEST_AVERAGING_DELAY,
		sampleCount, antenna, 0, 1, 0, 0, 0);
}

int wlmRxIQEstExtSweepGet(wlmRxIQEstSweepReult_t **result, int *channels, int sampleCount,
	int antenna, int elna, int gainIndex)
{
	return wlmRxIQEstSweepGetFull(result, channels, 1, PHY_RXIQEST_AVERAGING_DELAY,
		sampleCount, antenna, 0, 0, gainIndex, 0, elna);
}

int wlmRxIQEstACSweepGet(wlmRxIQEstSweepReult_t **result, int *channels, int sampleCount,
        int antenna, int extraGain, int gainIndex, int gainCorrect, int niter, int delay)
{
	return wlmRxIQEstSweepGetFull(result, channels, niter, delay, sampleCount, antenna,
		extraGain, 0, gainIndex, gainCorrect, 0);
}

int wlmPHYTxPowerIndexGet(unsigned int *val, const char *chipid)
{
	uint32 power_index = (uint32)-1;
	uint32 txpwridx[4] = {0};
	int chip = atoi(chipid);

	switch (chip) {
	        case 4329:
		case 43291:
		  if (wlu_iovar_getint(irh, "sslpnphy_txpwrindex", (int*)&power_index) < 0) {
				printf("wlmPHYTxPowerIndexGet: %s\n", wlmLastError());
				return FALSE;
			}
			*val = power_index;
			break;
	        case 4325:
			if (wlu_iovar_getint(irh, "lppphy_txpwrindex", (int*)&power_index) < 0) {
				printf("wlmPHYTxPowerIndexGet: %s\n", wlmLastError());
				return FALSE;
			}
			*val = power_index;
			break;
	        default:
			if (wlu_iovar_getint(irh, "phy_txpwrindex", (int*)&txpwridx[0]) < 0) {
				printf("wlmPHYTxPowerIndexGet: %s\n", wlmLastError());
				return FALSE;
			}
			*val = dtoh32(txpwridx[0]);
			break;
	}

	return TRUE;
}

int wlmPHYTxPowerIndexSet(unsigned int val, const char *chipid)
{
	uint32 power_index;
	uint32 txpwridx[4] = {0};
	int chip = atoi(chipid);

	power_index = dtoh32(val);
	switch (chip) {
	        case 4329:
	        case 43291:
		  if (wlu_iovar_setint(irh, "sslpnphy_txpwrindex", (int)power_index) < 0) {
				printf("wlmPHYTxPowerIndexSet: %s\n", wlmLastError());
				return FALSE;
			}
			break;
	        case 4325:
		  if (wlu_iovar_setint(irh, "lppphy_txpwrindex", (int)power_index) < 0) {
				printf("wlmPHYTxPowerIndexSet: %s\n", wlmLastError());
				return FALSE;
			}
			break;
	        default:
			txpwridx[0] = (int)(power_index & 0xff);
			txpwridx[1] = (int)((power_index >> 8) & 0xff);
			txpwridx[2] = (int)((power_index >> 16) & 0xff);
			txpwridx[3] = (int)((power_index >> 24) & 0xff);

			if (wlu_var_setbuf(irh,
				"phy_txpwrindex", txpwridx, 4*sizeof(uint32)) < 0) {
				printf("wlmPHYTxPowerIndexSet: %s\n", wlmLastError());
				return FALSE;
			}
			break;
	}

	return TRUE;
}

int wlmRIFSEnable(int enable)
{
	int val, rifs;

	val = rifs = htod32(enable);
	if (rifs != 0 && rifs != 1) {
		printf("wlmRIFSEnable: Usage: input must be 0 or 1\n");
		return FALSE;
	}

	if (wlu_set(irh, WLC_SET_FAKEFRAG, &val, sizeof(int)) < 0) {
		printf("wlmRIFSEnable: %s\n", wlmLastError());
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "rifs", (int)rifs) < 0) {
		printf("wlmRIFSEnable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmGpioOut(unsigned int mask, unsigned int val)
{
	uint32 *int_ptr;
	char buf[32] = "\0";

	mask = htod32(mask);

	val = htod32(val);

	if ((~mask & val) != 0) {
		printf("wlmGpioOut: mask and val don't matcch");
		return FALSE;
	}

	int_ptr = (uint32 *)buf;
	memcpy(int_ptr, (const void *)&mask, sizeof(mask));
	int_ptr++;
	memcpy(int_ptr, (const void *)&val, sizeof(val));

	if (wlu_iovar_set(irh, "gpioout", buf, sizeof(uint32) *2) < 0) {
		printf("wlmGpioOut: failed toggle gpio %d to %d\n", mask, val);
		return FALSE;
	} else
		return TRUE;
}

int
wlmPhyRssiGainDelta2gGet(char *varname, int *deltaValues, int core)
{
	int i, index;
	int8 vals[18];
	uint32 phytype;
	uint8 N = 5;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta2gGet: phy type %d is not suppported\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		if (wlu_iovar_get(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainDelta2gGet: failed to get delta values.\n");
			return FALSE;
		}

		memset(deltaValues, 0, N * sizeof(int));
		for (i = 0; i < N * 3; i++) {
			if (i == core * N) {
				core = (int)vals[i++];
				for (index = 0; index < N -1; index++)
					deltaValues[index] = (int)vals[i++];
				break;
			}
		}
	}

	return TRUE;
}

int
wlmPhyRssiGainDelta2gSet(char *varname, int *deltaValues, int core)
{
	int i, index;
	int8 vals[18];
	uint32 phytype;
	uint8 N = 5;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta2gSet: phy type %d is not suppported\n", phytype);
		return FALSE;
	} else {
		for (i = 0; i < N * 3; i++) {
			if (i == core * N) {
				vals[i++] = core;
				for (index = 0; index < N -1; index++) {
					if (deltaValues[index] > 63 || deltaValues[index] < -64) {
						printf("wlmPhyRssiGainDelta2gSet:"
						       "input data out of range (-64,63)\n");
						return FALSE;
					}
					vals[i++] = deltaValues[index];
				}
				break;
			}
		}

		if (wlu_var_setbuf(irh, varname, vals + N * core, N * sizeof(int8)) < 0) {
			printf("wlmPhyRssiGainDelta2gSet: failed to set delta values.\n");
			return FALSE;
		}
	}
	return TRUE;
}

int
wlmPhyRssiGainDelta5gGet(char *varname, int *deltaValues, int core)
{
	int i, index;
	int8 vals[40];
	uint32 phytype;
	uint8 N = 13;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta5gGet: phy type %d is not suppported\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		if (wlu_iovar_get(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainDelta5gGet: failed to get delta value.\n");
			return FALSE;
		}

		memset(deltaValues, 0, N * sizeof(int));
		for (i = 0; i < N * 3; i++) {
			if (i == core * N) {
				core = (int)vals[i++];
				for (index = 0; index < N -1; index++)
					deltaValues[index] = (int)vals[i++];
				break;
			}
		}
	}

	return TRUE;
}

int
wlmPhyRssiGainDelta5gSet(char *varname, int *deltaValues, int core)
{
	int i, index;
	int8 vals[40];
	uint32 phytype;
	uint8 N = 13;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta5gSet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		for (i = 0; i < N * 3; i++) {
			if (i == core * N) {
				vals[i++] = core;
				for (index = 0; index < N -1; index++) {
					if (deltaValues[index] > 63 || deltaValues[index] < -64) {
						printf("wlmPhyRssiGainDelta5gSet:"
						       "input data out of range (-64,63)\n");
						return FALSE;
					}
					vals[i++] = (int8)deltaValues[index];
				}
				break;
			}
		}

		if (wlu_var_setbuf(irh, varname, vals + N * core, N * sizeof(int8)) < 0) {
			printf("wlmPhyRssiGainDelta5gSet: failed to set delta value.\n");
			return FALSE;
		}
	}

	return TRUE;
}

int
wlmVHTFeaturesSet(int val)
{
	val = htod32(val);

	if (wlu_iovar_setint(irh, "vht_features", val) < 0) {
		printf("wlmVHTFeaturesSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmTxBFEnable(void)
{
	if (wlu_iovar_setint(irh, "txbf", 1) < 0) {
		printf("wlmTxBFEnable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmTxBFDisable(void)
{
	if (wlu_iovar_setint(irh, "txbf", 0) < 0) {
		printf("wlmTxBFDisable: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int
wlmSpectModeSet(int val)
{
	int spect;

	spect = htod32(val);

	if ((spect < SPECT_MNGMT_OFF) || (spect > SPECT_MNGMT_LOOSE_11H_D)) {
		printf("wlmSpectEnable: spect %d out of range [0, 4] \n", spect);
		return FALSE;
	}

	if (wlu_set(irh, WLC_SET_SPECT_MANAGMENT, &spect, sizeof(spect)) < 0) {
		printf("wlmSpectEnable: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int
wlmIBSSGmodeEnable(void)
{
	/* check parameter -1 here */
	if (wlu_iovar_setint(irh, "ibss_gmode", -1) < 0) {
		printf("wlmIBSSGmodeEnable: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int
wlmIBSSGmodeDisable(void)
{
	/* check parameter 0 here */
	if (wlu_iovar_setint(irh, "ibss_gmode", 0) < 0) {
		printf("wlmIBSSGmodeDisable: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int
wlmBandWidthCapabilitySet(WLM_BAND band, int val)
{
	struct {
		uint32 band;
		uint32 bw_cap;
	} param = {0, 0};

	param.band = band;
	param.bw_cap = (uint32)val;

	if (wlu_var_setbuf(irh, "bw_cap", &param, sizeof(param)) < 0) {
		printf("wlmBandWidthCapabilitySet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmBandWidthCapabilityGet(WLM_BAND band, int *val)
{
	struct {
		uint32 band;
		uint32 bw_cap;
	} param = {0, 0};
	void *ptr = NULL;

	param.band = band;
	param.bw_cap = 0;

	if (wlu_var_getbuf_sm(irh, "bw_cap", &param, sizeof(param), &ptr) < 0) {
		printf("wlmBandWidthCapabilityGet: %s\n", wlmLastError());
		return FALSE;
	}
	*val = *((uint32 *)ptr);

	return TRUE;
}

int
wlmEnableLongPacket(int enable)
{
	if ((enable < 0) || (enable > 1)) {
		printf("wlmEnableLongPacket: invalid input select [0,1]\n");
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "longpkt", enable) < 0) {
		printf("wlmEnableLongPacket: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmSpatialPolicySet(int mode_2g, int mode_5g_low, int mode_5g_mid,
    int mode_5g_high, int mode_5g_upper)
{
	int mode[SPATIAL_MODE_MAX_IDX] = { -1, -1, -1, -1, -1};

	mode[SPATIAL_MODE_2G_IDX] = mode_2g;
	mode[SPATIAL_MODE_5G_LOW_IDX] = mode_5g_low;
	mode[SPATIAL_MODE_5G_MID_IDX] = mode_5g_mid;
	mode[SPATIAL_MODE_5G_HIGH_IDX] = mode_5g_high;
	mode[SPATIAL_MODE_5G_UPPER_IDX] = mode_5g_upper;

	if (wlu_var_setbuf(irh, "spatial_policy", &mode, sizeof(mode)) < 0) {
		printf("wlmSpatialPolicySet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPHYRPTableSet(void)
{
	if (wlu_var_setbuf(irh, "phy_setrptbl", NULL, 0) < 0) {
		printf("wlmPHYRPTableSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPHYForceImplicitBeamforming(void)
{
	if (wlu_var_setbuf(irh, "phy_forceimpbf", NULL, 0) < 0) {
		printf("wlmPHYForceImplicitBeamforming: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPHYForceSteer(int on)
{
	if ((on < 0) || (on > 1)) {
		printf("wlmPHYForceSteer: invalid input select [0,1]\n");
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "phy_forcesteer", on) < 0) {
		printf("wlmPHYForceSteer: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyRssiGainError2gGet(char *varname, int *deltaValues)
{
	int i;
	int8 vals[18];
	uint32 phytype;
	int coremask = 0;
	int N = 0;

	if (wlu_iovar_getint(irh, "hw_rxchain", &coremask) < 0) {
		printf("wlmPhyRssiGainError2gGet: failed to query hw_rxchain.\n");
		return FALSE;
	}

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainError2gGet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		if (wlu_iovar_get(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainError2gGet: failed to get error values.\n");
			return FALSE;
		}

		memset(deltaValues, 0, N * sizeof(int));
		for (i = 0; i < N; i++) {
			deltaValues[i] = (int)vals[i];
		}
	}

	return TRUE;
}

int
wlmPhyRssiGainError2gSet(char *varname, int *deltaValues)
{
	int i;
	int8 vals[18];
	uint32 phytype;
	int coremask = 0;
	int N = 0;

	if (wlu_iovar_getint(irh, "hw_rxchain", &coremask) < 0) {
		printf("wlmPhyRssiGainError2gGet: failed to query hw_rxchain.\n");
		return FALSE;
	}

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta2gSet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		for (i = 0; i < N; i++) {
			if (deltaValues[i] > 63 || deltaValues[i] < -64) {
				printf("wlmPhyRssiGainError2gSet:"
				       "deltaValues[%d] out of range (-64,63) \n", i);
				return FALSE;
			}
			vals[i] = (int8)htod32(deltaValues[i]);
		}

		if (wlu_var_setbuf(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainError2gSet: failed to set error values.\n");
			return FALSE;
		}
	}
	return TRUE;
}

int
wlmPhyRssiGainError5gGet(char *varname, int *deltaValues)
{
	int i;
	int8 vals[28];
	uint32 phytype;
	int coremask;
	int N = 0;

	if (wlu_iovar_getint(irh, "hw_rxchain", &coremask) < 0) {
		printf("wlmPhyRssiGainError2gGet: failed to query hw_rxchain.\n");
		return FALSE;
	}

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainError5gGet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		if (wlu_iovar_get(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainError5gGet: failed to get error value.\n");
			return FALSE;
		}

		memset(deltaValues, 0, N * sizeof(int));
		for (i = 0; i < N; i++) {
			deltaValues[i] = (int)vals[i];
		}
	}

	return TRUE;
}

int
wlmPhyRssiGainError5gSet(char *varname, int *deltaValues)
{
	int i;
	int8 vals[28];
	uint32 phytype;
	int coremask;
	int N = 0;

	if (wlu_iovar_getint(irh, "hw_rxchain", &coremask) < 0) {
		printf("wlmPhyRssiGainError2gGet: failed to query hw_rxchain.\n");
		return FALSE;
	}

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainError5gSet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		memset(vals, 0, sizeof(vals));
		for (i = 0; i < N;  i++) {
			if (deltaValues[i] > 63 || deltaValues[i] < -64) {
				printf("wlmPhyRssiGainError5gSet:"
				       "gainerror  %d out of range (-64,63) \n", deltaValues[i]);
				return FALSE;
			}
			vals[i] = (int8)htod32(deltaValues[i]);
		}

		if (wlu_var_setbuf(irh, varname, vals, sizeof(vals)) < 0) {
			printf("wlmPhyRssiGainError5gSet: failed to set error value.\n");
			return FALSE;
		}
	}

	return TRUE;
}

int
wlmPhyRawTempsenseGet(int *val)
{
	uint32 phytype;
	int8 rawtempsense;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRawTempsenseGet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		if (wlu_iovar_get(irh, "phy_sromtempsense",
			&rawtempsense, sizeof(rawtempsense)) < 0) {
			printf("wlmPhyRawTempsenseGet: failed phy_rawtempsense.\n");
			return FALSE;
		}
	}

	*val = (int)rawtempsense;

	return TRUE;
}

int
wlmPhyRawTempsenseSet(int val)
{
	uint32 phytype;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRawTempsenseSet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		if ((val > 63) || (val < -64)) {
			printf("wlmPhyRawTempsenseSet:"
			       "value %d out of  range (-64,63)\n", val);
			return FALSE;
		}

		if (wlu_var_setbuf(irh, "phy_sromtempsense",
			&val, sizeof(val)) < 0) {
			printf("wlmPhyRawTempsenseSet: failed to set value %s.\n", wlmLastError());
			return FALSE;
		}
	}

	return TRUE;
}

int
wlmPhyRPCalValsSet(int rpcal2g, int rpcal5gb0, int rpcal5gb1, int rpcal5gb2, int rpcal5gb3,
                   int rpcal2gcore3, int rpcal5gb0core3, int rpcal5gb1core3,
                   int rpcal5gb2core3, int rpcal5gb3core3)
{
	wl_rpcal_t rpcal[2*WL_NUM_RPCALVARS];

	rpcal[WL_CHAN_FREQ_RANGE_2G].value = (uint16) (rpcal2g & 0xffff);
	rpcal[WL_NUM_RPCALVARS + WL_CHAN_FREQ_RANGE_2G].value =
	(uint16) (rpcal2gcore3 & 0xff);
	rpcal[WL_CHAN_FREQ_RANGE_2G].update = 1;

	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND0].value = (uint16) (rpcal5gb0 & 0xffff);
	rpcal[WL_NUM_RPCALVARS + WL_CHAN_FREQ_RANGE_5G_BAND0].value =
	(uint16) (rpcal5gb0core3 & 0xff);
	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND0].update = 1;

	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND1].value = (uint16) (rpcal5gb1 & 0xffff);
	rpcal[WL_NUM_RPCALVARS + WL_CHAN_FREQ_RANGE_5G_BAND1].value =
	(uint16) (rpcal5gb1core3 & 0xff);
	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND1].update = 1;

	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND2].value = (uint16) (rpcal5gb2 & 0xffff);
	rpcal[WL_NUM_RPCALVARS + WL_CHAN_FREQ_RANGE_5G_BAND2].value =
	(uint16) (rpcal5gb2core3 & 0xff);
	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND2].update = 1;

	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND3].value = (uint16) (rpcal5gb3 & 0xffff);
	rpcal[WL_NUM_RPCALVARS + WL_CHAN_FREQ_RANGE_5G_BAND3].value =
	(uint16) (rpcal5gb3core3 & 0xff);
	rpcal[WL_CHAN_FREQ_RANGE_5G_BAND3].update = 1;

	if (wlu_var_setbuf(irh, "rpcalvars", &(rpcal[0]),
		sizeof(wl_rpcal_t) * 2 * WL_NUM_RPCALVARS) < 0) {
		printf("wlmPhyRPCalValsSet: failed to set rpcalvals %s\n", wlmGetLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyRPCalValsGet(int *rpcal2g, int *rpcal5gb0, int *rpcal5gb1, int *rpcal5gb2, int *rpcal5gb3,
                   int *rpcal2gcore3, int *rpcal5gb0core3, int *rpcal5gb1core3,
                   int *rpcal5gb2core3, int *rpcal5gb3core3)
{
	wl_rpcal_t rpcal[2*WL_NUM_RPCALVARS], *rpcal_out;
	void *ptr = NULL;

	if (wlu_var_getbuf_sm(irh, "rpcalvars", &(rpcal[0]),
	      sizeof(wl_rpcal_t) * 2 * WL_NUM_RPCALVARS, &ptr) < 0) {
		printf("wlmPhyRPCalValsGet: failed to get rpcalvals %s\n", wlmGetLastError());
		return FALSE;
	}

	rpcal_out = (wl_rpcal_t *) ptr;

	*rpcal2g = rpcal_out[WL_CHAN_FREQ_RANGE_2G].value;
	*rpcal5gb0 = rpcal_out[WL_CHAN_FREQ_RANGE_5G_BAND0].value;
	*rpcal5gb1 = rpcal_out[WL_CHAN_FREQ_RANGE_5G_BAND1].value;
	*rpcal5gb2 = rpcal_out[WL_CHAN_FREQ_RANGE_5G_BAND2].value;
	*rpcal5gb3 = rpcal_out[WL_CHAN_FREQ_RANGE_5G_BAND3].value;

	*rpcal2gcore3 = rpcal_out[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_2G].value & 0xff;
	*rpcal5gb0core3 = rpcal_out[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND0].value & 0xff;
	*rpcal5gb1core3 = rpcal_out[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND1].value & 0xff;
	*rpcal5gb2core3 = rpcal_out[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND2].value & 0xff;
	*rpcal5gb3core3 = rpcal_out[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND3].value & 0xff;

	return TRUE;
}

int
wlmPhyRegDump(char *reg_name, int *dump_vals, int *len)
{
	char *dump_buf;
	char	*pchar = NULL;
	int i = 0;

	dump_buf = CALLOC(WL_DUMP_BUF_LEN);

	if (dump_buf == NULL) {
		printf("wlmPhyRegDump: "
		       "failed to allocate %d bytes for dump_buf\n", WL_DUMP_BUF_LEN);
		return FALSE;
	}

	if (dump_buf[0] != '\0')
		strcat(dump_buf, " ");

	strcat(dump_buf, reg_name);
	strcat(dump_buf, " ");

	if (wlu_iovar_getbuf(irh, "dump",
		dump_buf, strlen(dump_buf), dump_buf, WL_DUMP_BUF_LEN) < 0) {
		printf("wlmPhyRegDump: failed dump %s.\n", reg_name);
		return FALSE;
	}

	*len = 0;

	pchar = strtok(dump_buf, "\n");
	while (pchar) {
		/* printf("%s\n", pchar); */
		dump_vals[i] = (int)strtol(pchar, NULL, 0);
		pchar = strtok(NULL, "\n");
		i++;
	}

	*len = i;

	return TRUE;
}

int
wlmPhyRssiCalibrationFrequencyGroup2gGet(int *values)
{
	int i;
	uint8 nvramValues[14];
	uint8 N = 0;
	uint32 phytype;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiCalibrationFrequencyGroup2gGet: "
		       "phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		/* Reading the NVRAM variable */
		if (wlu_iovar_get(irh, "rssi_cal_freq_grp_2g",
			nvramValues, sizeof(nvramValues)) < 0) {
			printf("wlmPhyRssiCalibrationFrequencyGroup2gGet: "
			       "failed to get group values %s.\n", wlmGetLastError());
			return FALSE;
		}

		N = 14; /* 14 corresponds to number of channels in 2g */

		for (i = 0; i < N-2; i++) {
			values[i] = (int)nvramValues[i];
			values[i+1] = (int)nvramValues[i+1];
			i++;
		}
		values[i] = (int)nvramValues[i];
		values[i+1] = (int)nvramValues[i+1];
	}

	return TRUE;
}

int
wlmPhyRssiCalibrationFrequencyGroup2gSet(int *values)
{
	int i;
	uint8 N = 0;
	uint8 nvramValues[14];
	uint32 phytype;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiCalibrationFrequencyGroup2gSet: "
		       "phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		N = 7;
		i = 0;
		while (i < 14) {
			nvramValues[i] = (uint8)(values[i] & 0xf);
			i++;
		}

		if (i != 14) {
			printf("wlmPhyRssicalibrationFrequencyGroup2gSet: "
			       "Insufficient arguments \n");
			return FALSE;
		}

		if (wlu_var_setbuf(irh, "rssi_cal_freq_grp_2g",
			nvramValues, N*2*sizeof(int8)) < 0) {
			printf("wlmPhyRssicalibrationFrequencyGroup2gSet: "
			       "failed to set group values %s\n", wlmGetLastError());
			return FALSE;
		}
	}
	return TRUE;
}

int
wlmPhyRssiGainDelta2gSubGet(char *varname, int *values, int coreid)
{
	int i;
	int8 deltaValues[28];
	uint32 phytype;
	uint8 N = 0;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta2GSubGet: phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		if (wlu_iovar_get(irh, varname,
			deltaValues, sizeof(deltaValues)) < 0) {
			printf("wlmPhyRssiGainDelta2GSubGet: "
			       "failed to get delta values %s\n", wlmGetLastError());
			return FALSE;
		}
		N = 9; /* 9 entries per core,43602WLCSP - 27 MAX entries,
				* 4350 - 18 MAX entries; 4345 9 MAX entries
				*/
		for (i = 0; i < N - 1; i++) {
			if (deltaValues[(9 * coreid) + i] ==  -1)
				break;
			values[i] = deltaValues[(9 * coreid) + i + 1];
			printf("wlmSubGet: %d\n", values[i]);
		}
	}
	return TRUE;
}

int
wlmPhyRssiGainDelta2gSubSet(char *varname, int *values, int coreid)
{
	int i;
	int8 deltaValues[28];
	uint32 phytype;
	uint8 N = 0;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyRssiGainDelta2gSubSet: "
		       "phy type %d is not suppprted\n", phytype);
		return FALSE;
	} else {
		/* ACPHY : 8/9 entries for a core; core 0 delta's can be
		 * given with or with out core_num as first element
		 */
		N = 9;
		/*
		  if (wlu_iovar_get(irh, varname, deltaValues, sizeof(deltaValues)) < 0) {
			printf("wlmPhyRssiGainDelta2GSubSet: "
			       "failed to get existing delta values %s\n", wlmGetLastError());
			return FALSE;
		}
		*/
		deltaValues[coreid * 9] = (int8)(coreid & 0xff);
		for (i = 0; i < N - 1; i++) {
			deltaValues[(coreid * 9) + i + 1] = (int8)(values[i] & 0xff);
			printf("wlmSubSet: %d\n", deltaValues[(coreid * 9) + i + 1]);
		}

		if (wlu_var_setbuf(irh, varname,
			(deltaValues + coreid * 9), 9*sizeof(int8)) < 0) {
			printf("wlmPhyRssiGainDelta2GSubSet: "
			       "failed to set values %s\n", wlmGetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

int
wlmAdjustedTssiGet(int core, int *adjTssi)
{
	int val;
	void *ptr = NULL;

	val = htod32(core);

	if (wlu_var_getbuf_sm(irh, "phy_adj_tssi", &val, sizeof(int), &ptr) < 0) {
		printf("wlmAdjTssiGet: failed to get adj_tssi value %s\n", wlmLastError());
		return FALSE;
	}

	*adjTssi = dtoh32(*(int*)ptr);

	return TRUE;
}

int
wlmPhyTestIdleTssiGet(int core, int *tssi)
{
	int val;
	char *buf = NULL;
	const char *iovar = "phy_test_idletssi";
	const uint buf_size = strlen(iovar) + 1 + sizeof(val);
	val = htod32(core);
	buf = CALLOC(buf_size);

	if (buf == NULL) {
		printf("wlmPhyTestIdleTssiGet: failed to allocate memory\n");
		return FALSE;
	}

	if (wlu_iovar_getbuf(irh, iovar, &val, sizeof(val), buf, buf_size) < 0) {
		printf("wlmPhyTestIdleTssiGet: failed to get idle tssi value %s\n", wlmLastError());
		return FALSE;
	}

	*tssi = dtoh32(*(int*)buf);

	return TRUE;
}

int
wlmPhyTestTssiGet(int core, int *tssi)
{
	int val;
	char *buf = NULL;
	const char *iovar = "phy_test_tssi";
	const uint buf_size = strlen(iovar) + 1 + sizeof(val);
	val = htod32(core);
	buf = CALLOC(buf_size);

	if (buf == NULL) {
		printf("wlmPhyTestTssiGet: failed to allocate memory\n");
		return FALSE;
	}

	if (wlu_iovar_getbuf(irh, iovar, &val, sizeof(val), buf, buf_size) < 0) {
		printf("wlmPhyTestTssiGet: failed to get tssi value %s\n", wlmLastError());
		return FALSE;
	}

	*tssi = dtoh32(*(int*)buf);

	return TRUE;
}

int
wlmTxCalGainSweepMeasGet(int count, int *tssi, int *txpower, int core)
{
	wl_txcal_meas_ncore_t * txcal_meas;
	wl_txcal_meas_percore_t * per_core;
	wl_txcal_meas_old_t *txcal_meas_legacy;
	void *ptr = NULL;
	uint8 i;
	void* buf = NULL;
	uint16 buf_size = OFFSETOF(wl_txcal_meas_ncore_t, txcal_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_meas_percore_t);
	int version_err = BCME_OK;

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	txcal_meas = (wl_txcal_meas_ncore_t *)buf;

	if (count > MAX_NUM_TXCAL_MEAS) {
		printf("wlmTxCalGainSweepMeasGet: Entries exceeded max allowed\n");
		goto fail;
	}

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "txcal_gainsweep_meas", NULL, 0, &ptr) < 0) {
			printf("wlmTxCalGainSweepMeasGet: failed to get"
					"txcal gain sweep measurement\n");
			goto fail;
		}
	    txcal_meas_legacy = ptr;

		for (i = 0; i < txcal_meas_legacy->valid_cnt; i++) {
			tssi[i] = txcal_meas_legacy->tssi[core][i];
			txpower[i] = txcal_meas_legacy->pwr[core][i];
			/* printf("tssi[%d] %d, txpower[%d] %d\n", i, tssi[i], i, txpower[i]); */
		}

		goto success;

	} else {
		txcal_meas->version = TXCAL_IOVAR_VERSION;
		if (wlu_var_getbuf_minimal(irh, "txcal_gainsweep_meas",
				txcal_meas, buf_size, &ptr) < 0) {
			printf("wlmTxCalGainSweepMeasGet: failed to get"
					"txcal gain sweep measurement\n");
			goto fail;
		}

		txcal_meas = (wl_txcal_meas_ncore_t*)ptr;
		/* Txcal version check */
		if (txcal_meas->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_meas->version);
			goto fail;
		}
		/* support for firmware with new txcal structures */
		/* Move to per core */
		per_core = txcal_meas->txcal_percore;
		per_core += core;

		for (i = 0; i < txcal_meas->valid_cnt; i++) {
			tssi[i] = per_core->tssi[i];
			txpower[i] = per_core->pwr[i];
			/* printf("tssi[%d] %d, txpower[%d] %d\n", i, tssi[i], i, txpower[i]); */
		}
	}
success:
	if (buf)
		free(buf);
	return TRUE;
fail:
	if (buf)
		free(buf);
	return FALSE;
}

int
wlmTxCalGainSweepMeasSet(int *measPower, int count, int core)
{
	wl_txcal_meas_ncore_t * txcal_meas, * txcal_meas_old;
	wl_txcal_meas_percore_t * per_core;
	wl_txcal_meas_old_t *txcal_meas_legacy;
	int16 pwr[WLC_TXCORE_MAX][MAX_NUM_TXCAL_MEAS];
	void *ptr = NULL;
	void* buf = NULL;
	uint16 i;
	int version_err = BCME_OK;

	uint16 buf_size = OFFSETOF(wl_txcal_meas_ncore_t, txcal_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_meas_percore_t);

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	txcal_meas = (wl_txcal_meas_ncore_t *)buf;
	memset(pwr, 0, WLC_TXCORE_MAX*MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0]));

	if (core > (WLC_TXCORE_MAX- 1)) {
		printf("wlmTxCalGainSweepMeasSet : invalid core number \n");
		goto fail;
	}
	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		  /* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "txcal_gainsweep_meas", NULL, 0, &ptr) < 0) {
			printf("wlmTxCalGainSweepMeasSet: failed to get current"
					"TxCalGainSweep entries.\n");
			goto fail;
		}

	   txcal_meas_legacy = ptr;

		memcpy(&pwr[0][0], &txcal_meas_legacy->pwr[0][0],
		       WLC_TXCORE_MAX_OLD*MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0]));

		i = 0;

		while (i < count) {
			if (i >= MAX_NUM_TXCAL_MEAS) {
				printf("wlmTxCalGainSweepMeasSet: Entries exceeded max allowed\n");
				goto fail;
			}

			pwr[core][i] = (int16)measPower[i];
			i++;
		}

		if (i != txcal_meas_legacy->valid_cnt) {
			printf("wlmTxCalGainSweepMeasSet: Incorrect Number of Entries."
			      " Expected %d, Entered %d\n", txcal_meas_legacy->valid_cnt, i);
			goto fail;
		}

		if (wlu_var_setbuf(irh, "txcal_gainsweep_meas",
			pwr, WLC_TXCORE_MAX_OLD*MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0])) < 0) {
			printf("wlmTxCalGainSweepMeasSet: failed to set TxCalGainSweep entries\n");
			goto fail;
		}

		goto success;

	} else {
		txcal_meas->version = TXCAL_IOVAR_VERSION;
		if (wlu_var_getbuf_minimal(irh, "txcal_gainsweep_meas",
				txcal_meas, buf_size, &ptr) < 0) {
			printf("wlmTxCalGainSweepMeasSet: failed to"
					"get current TxCalGainSweep entries.\n");
			goto fail;
		}

		/* Existing copy */
		txcal_meas_old = (wl_txcal_meas_ncore_t *)ptr;
		/* txcal version check */
		if (txcal_meas_old->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_meas_old->version);
			goto fail;
		}

		/* support for firmware with new txcal structures */
		/* check for max core support from dongle */
		if (core > txcal_meas_old->num_core - 1) {
			printf("wlmTxCalGainSweepMeasSet: Dongle supports only %d  cores \n"
				"Allowed range 0 - %d \n", txcal_meas_old->num_core,
				txcal_meas_old->num_core - 1);
			goto fail;
		}

		/* Initialize set structure with fw copy to start with */
		memcpy(txcal_meas, txcal_meas_old, buf_size);

		/* Get to per core info */
		per_core = txcal_meas->txcal_percore;
		per_core += core;
		i = 0;
		while (i < count) {
			if (i >= MAX_NUM_TXCAL_MEAS) {
				printf("wlmTxCalGainSweepMeasSet: Entries exceeded max allowed\n");
				goto fail;
			}
			per_core->pwr[i] = (int16)measPower[i];
			i++;
		}

		/* check for valid count */
		if (i != txcal_meas->valid_cnt) {
			printf("wlmTxCalGainSweepMeasSet: Incorrect Number of Entries."
			      " Expected %d, Entered %d\n", txcal_meas->valid_cnt, i);
			goto fail;
		}

		txcal_meas->version = TXCAL_IOVAR_VERSION;
		if (wlu_var_setbuf(irh, "txcal_gainsweep_meas",
			txcal_meas, buf_size) < 0) {
			printf("wlmTxCalGainSweepMeasSet: failed to set TxCalGainSweep entries\n");
			goto fail;
		}
	}
success:
	if (buf)
		free(buf);
	return TRUE;
fail:
	if (buf)
		free(buf);

	return FALSE;
}

int
wlmTxCalGainSweep(char *desMac, int delay, int length, int nFrames, int start, int stop, int step)
{
	wl_txcal_params_t txcal_params;
	uint8 gidx_start, gidx_stop;
	int8 gidx_step;

	memset(&txcal_params, 0, sizeof(txcal_params));

	if (!wl_ether_atoe(desMac, (struct ether_addr *)&txcal_params.pkteng.dest)) {
		printf("wlmTxCalGainSweep: destinatin Mac address incorrect %s\n", desMac);
		return FALSE;
	}

	txcal_params.pkteng.delay = delay;
	txcal_params.pkteng.length = length;
	txcal_params.pkteng.nframes = nFrames;

	if (txcal_params.pkteng.nframes == 0)
		txcal_params.pkteng.nframes = 4;

	txcal_params.pkteng.flags = WL_PKTENG_PER_TX_START;
	txcal_params.pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
	txcal_params.pkteng.flags = htod32(txcal_params.pkteng.flags);
	txcal_params.pkteng.delay = htod32(txcal_params.pkteng.delay);
	txcal_params.pkteng.nframes = htod32(txcal_params.pkteng.nframes);
	txcal_params.pkteng.length = htod32(txcal_params.pkteng.length);

	gidx_start = start;
	gidx_stop = stop;
	gidx_step = step;

	txcal_params.gidx_start = gidx_start;
	txcal_params.gidx_step = gidx_step;
	txcal_params.gidx_stop = gidx_stop;

	if (wlu_var_setbuf(irh, "txcal_gainsweep", &txcal_params, sizeof(txcal_params)) < 0) {
		printf("wlmTxCalGainSweep: failed to run TxCalGainSweep\n");
		return FALSE;
	}

	return TRUE;
}
int
wlmTxCalPowerTssiTableGet(int core, int *channel, int *startPower, int *num, int *tssiValues)
{
	wl_txcal_power_tssi_ncore_t * txcal_tssi;
	wl_txcal_power_tssi_percore_t * per_core;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;
	void *ptr = NULL;
	uint8 i;
	int version_err = BCME_OK;

	/* Total buffer size to be allocated */
	uint16 buf_size = OFFSETOF(wl_txcal_power_tssi_ncore_t, tssi_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_power_tssi_percore_t);
	void * buf = NULL;
	int8 mode = 1;

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	txcal_tssi = (wl_txcal_power_tssi_ncore_t *)buf;

	if (core > (WLC_TXCORE_MAX- 1)) {
		printf("wlmTxCalPowerTssiTableGet: Invalid core requested\n");
		goto fail;
	}

	TXCAL_DETECT_MODE

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "txcal_pwr_tssi_tbl",
			(uint8*)channel, sizeof(uint8), &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGet: failed to read the table\n");
			goto fail;
		}
		txcal_pwr_tssi_ptr = ptr;

		*channel = txcal_pwr_tssi_ptr->channel;
		*startPower = txcal_pwr_tssi_ptr->pwr_start[core];
		*num = txcal_pwr_tssi_ptr->num_entries[core];
		for (i = 0; i < txcal_pwr_tssi_ptr->num_entries[core]; i++)
			tssiValues[i] = txcal_pwr_tssi_ptr->tssi[core][i];
		goto success;
	} else {
		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		txcal_tssi->channel = *channel;
		txcal_tssi->ofdm = mode;
		txcal_tssi->set_core = core;

		if (wlu_var_getbuf_minimal(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size, &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGet: failed to read the table\n");
			goto fail;
		}

		txcal_tssi = (wl_txcal_power_tssi_ncore_t *)ptr;
		/* txcal version check */
		if (txcal_tssi->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_tssi->version);
			goto fail;
		}

		/* support for firmware with new txcal structures */
		/* check for max core support from dongle */
		if (core > txcal_tssi->num_core - 1) {
			printf("Dongle supports only %d cores \n"
				"Allowed range 0 - %d \n", txcal_tssi->num_core,
				txcal_tssi->num_core - 1);
			goto fail;
		}

		/* per core pointer */
		per_core = txcal_tssi->tssi_percore;

		/* Move to requested core */
		per_core += core;

		TXCAL_PRINT_MODE

		*channel = txcal_tssi->channel;
		*startPower = per_core->pwr_start;
		*num = per_core->num_entries;
		for (i = 0; i < per_core->num_entries; i++)
			tssiValues[i] = per_core->tssi[i];

	}
success:
	if (buf)
		free(buf);
	return TRUE;
fail:
	if (buf)
		free(buf);
	return FALSE;
}

int
wlmTxCalPowerTssiTableSet(int core, int channel, int startPower, int num, int *tssiValues)
{
	wl_txcal_power_tssi_ncore_t * txcal_tssi;
	wl_txcal_power_tssi_ncore_t * txcal_tssi_old;
	wl_txcal_power_tssi_percore_t * per_core;
	wl_txcal_power_tssi_old_t txcal_pwr_tssi;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;
	void *ptr = NULL;
	uint8 i;
	int version_err = BCME_OK;

	/* Total buffer size to be allocated */
	uint16 buf_size = OFFSETOF(wl_txcal_power_tssi_ncore_t, tssi_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_power_tssi_percore_t);
	void * buf = NULL;
	int8 mode = 1;

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	txcal_tssi = (wl_txcal_power_tssi_ncore_t *)buf;

	TXCAL_DETECT_MODE

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "txcal_pwr_tssi_tbl", (uint8*)&channel,
				sizeof(uint8), &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableSet: "
		       "could not set the consolidated power tssi table, %s\n", wlmLastError());
			goto fail;
		}

		txcal_pwr_tssi_ptr = ptr;

		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;
		txcal_pwr_tssi.set_core = core;
		txcal_pwr_tssi.channel = channel;
		txcal_pwr_tssi.pwr_start[core] = startPower;
		txcal_pwr_tssi.num_entries[core] = num;

		memset(txcal_pwr_tssi.tssi[core], 0,
				MAX_NUM_PWR_STEP*sizeof(txcal_pwr_tssi.tssi[0][0]));

		i = 0;
		for (i = 0; i < num; i++) {
			if (i >= MAX_NUM_PWR_STEP) {
				printf("wlmTxCalPowerTssiTableSet: Entries exceeded max allowed\n");
				goto fail;
			}
			txcal_pwr_tssi.tssi[core][i] = tssiValues[i];
		}

		if (i != txcal_pwr_tssi.num_entries[core]) {
			printf("wlmTxCalPowerTssiTableSet: Incorrect Number of Entries."
			       "Expected %d, Entered %d\n",
			       txcal_pwr_tssi.num_entries[core], i);
			goto fail;
		}

		txcal_pwr_tssi.gen_tbl = 0;
		if (wlu_var_setbuf(irh, "txcal_pwr_tssi_tbl",
			&txcal_pwr_tssi, sizeof(txcal_pwr_tssi)) < 0) {
			printf("wlmTxCalPowerTssiTableSet: failed to set TxCal TssiTable Set\n");
			goto fail;
		}

		goto success;

	} else {

		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		txcal_tssi->channel = channel;
		txcal_tssi->ofdm = mode;

		if (wlu_var_getbuf_minimal(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size, &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableSet: could not set the consolidated"
					"power tssi table, %s\n", wlmLastError());
			goto fail;
		}

		txcal_tssi_old = (wl_txcal_power_tssi_ncore_t *)ptr;
		/* txcal version check */
		if (txcal_tssi_old->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_tssi_old->version);
			goto fail;
		}

		/* support for firmware with new txcal structures */
		memcpy(txcal_tssi, txcal_tssi_old, buf_size);

		/* Update user input values */
		txcal_tssi->channel = channel;
		txcal_tssi->set_core = core;
		txcal_tssi->ofdm = mode;

		/* Move to requested core */
		per_core = txcal_tssi->tssi_percore;
		per_core += core;

		TXCAL_PRINT_MODE

		/* Update per core info */
		per_core->pwr_start = startPower;
		per_core->num_entries = num;

		memset(per_core->tssi, 0, MAX_NUM_PWR_STEP * sizeof(per_core->tssi[0]));

		i = 0;
		for (i = 0; i < num; i++) {
			if (i >= MAX_NUM_PWR_STEP) {
				printf("wlmTxCalPowerTssiTableSet: Entries exceeded max allowed\n");
				goto fail;
			}
			per_core->tssi[i] = tssiValues[i];
		}

		if (i != per_core->num_entries) {
			printf("wlmTxCalPowerTssiTableSet: Incorrect Number of Entries."
			       "Expected %d, Entered %d\n",
			       per_core->num_entries, i);
			goto fail;
		}

		txcal_tssi->gen_tbl = 0;

		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		if (wlu_var_setbuf(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size) < 0) {
			printf("wlmTxCalPowerTssiTableSet: failed to set TxCal TssiTable Set\n");
			goto fail;
		}
	}

success:
	if (buf)
		free(buf);
	return TRUE;
fail:
	if (buf)
		free(buf);
	return FALSE;
}

int
wlmTxCalPowerTssiTableGenerate(int core, int channel, int startPower, int num, int *tssi)
{
	wl_txcal_power_tssi_ncore_t * txcal_tssi;
	wl_txcal_power_tssi_ncore_t * txcal_tssi_old;
	wl_txcal_power_tssi_percore_t * per_core;
	wl_txcal_power_tssi_old_t txcal_pwr_tssi;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;

	void * buf = NULL;

	/* Total buffer size to be allocated */
	uint16 buf_size = OFFSETOF(wl_txcal_power_tssi_ncore_t, tssi_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_power_tssi_percore_t);

	void *ptr = NULL;
	uint8 i;
	int version_err = BCME_OK;
	int8 mode = 1;

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	txcal_tssi = (wl_txcal_power_tssi_ncore_t *)buf;

	if (num >= MAX_NUM_PWR_STEP) {
		printf("wlmTxCalPowerTssiTableGenerate: "
		       "Entries exceeded max allowed\n");
		goto fail;
	}

	TXCAL_DETECT_MODE

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "txcal_pwr_tssi_tbl",
			(uint8*)&channel, sizeof(uint8), &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
		       "could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}
		txcal_pwr_tssi_ptr = ptr;

		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;
		txcal_pwr_tssi.set_core = core;
		txcal_pwr_tssi.channel = channel;
		txcal_pwr_tssi.pwr_start[core] = startPower;
		txcal_pwr_tssi.num_entries[core] = num;

		txcal_pwr_tssi.gen_tbl = 1;

		if (wlu_var_setbuf(irh, "txcal_pwr_tssi_tbl",
			&txcal_pwr_tssi, sizeof(txcal_pwr_tssi)) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
			       "could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}

		if (wlu_var_getbuf(irh, "txcal_pwr_tssi_tbl",
			(uint8*)&channel, sizeof(uint8), &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
		       "could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}

		txcal_pwr_tssi_ptr = ptr;
		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;

		for (i = 0; i < txcal_pwr_tssi_ptr->num_entries[core]; i++)
			tssi[i] = txcal_pwr_tssi_ptr->tssi[core][i] & 0xff;

		goto success;

	} else {
		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		txcal_tssi->channel = channel;
		txcal_tssi->ofdm = mode;

		if (wlu_var_getbuf_minimal(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size, &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
			       "could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}

		/* Current copy from fw */
		txcal_tssi_old = (wl_txcal_power_tssi_ncore_t *)ptr;
		/* txcal version check */
		if (txcal_tssi_old->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_tssi_old->version);
			goto fail;
		}
		/* support for firmware with new txcal structures */
		memcpy(txcal_tssi, txcal_tssi_old, buf_size);

		/* Update user input values */
		txcal_tssi->channel = channel;
		txcal_tssi->set_core = core;
		txcal_tssi->ofdm = mode;

		/* Move to requested core */
		per_core = txcal_tssi->tssi_percore;
		per_core += core;

		/* Update per core info */
		per_core->pwr_start = startPower;
		per_core->num_entries = num;

		txcal_tssi->gen_tbl = 1;
		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		txcal_tssi->channel = channel;
		if (wlu_var_setbuf(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
			       "could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}

		if (wlu_var_getbuf_minimal(irh, "txcal_pwr_tssi_tbl",
			txcal_tssi, buf_size, &ptr) < 0) {
			printf("wlmTxCalPowerTssiTableGenerate: "
				"could not generate consolidate table, %s\n", wlmLastError());
			goto fail;
		}

		/* Move to requested core */
		txcal_tssi = (wl_txcal_power_tssi_ncore_t *)ptr;
		per_core = txcal_tssi->tssi_percore;
		per_core += core;

		TXCAL_PRINT_MODE

		for (i = 0; i < per_core->num_entries; i++)
			tssi[i] = per_core->tssi[i] & 0xff;

	}
success:
	if (buf)
		free(buf);
	return TRUE;
fail:
	if (buf)
		free(buf);
	return FALSE;
}

int
wlmTxCalEstimatePowerLookupTableRead(int core, int *estPwrLut)
{
	uint16 *estpwrlut;
	void *ptr = NULL;
	int i;
	int val;

	val = htod32(core);

	if (wlu_var_getbuf_med(irh, "phy_read_estpwrlut", &val, sizeof(val), &ptr) < 0) {
		printf("wlmTxCalEstimatePowerLookupTableRead: failed %s\n", wlmLastError());
		return FALSE;
	}

	estpwrlut = ptr;

	for (i = 0; i < 128; i++)
		estPwrLut[i] = (int)(estpwrlut[i] > 0x7f ?
			(int16) (estpwrlut[i] - 0x100) : estpwrlut[i]);

	return TRUE;
}

int
wlmTxCalApplyPowerTssiTable(int mode)
{
	if ((mode < 0) && (mode > 1)) {
		printf("wlmTxCalApplyPowerTssiTable: invalid argument. Must be 0 or 1.\n");
		return FALSE;
	}

	if (wlu_iovar_setint(irh, "txcal_apply_pwr_tssi_tbl", mode)) {
		printf("wlmTxCalApplyPowerTssiTable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyForceCRSMinSet(int *values, int count)
{
	int err = -1;
	int8 th[4] = { 0 };
	int32 value;
	uint8 i = 0;
	uint32 phytype;
	int N;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype != WLC_PHY_TYPE_AC) {
		printf("wlmPhyForceCRSMinSet: "
			"phy type %d is not suppprted\n", phytype);
		 return FALSE;
	} else {
		N = 3; /* IOVAR works only for up to 3 cores */
		if (count > N) {
			printf("wlmPhyForceCRSMinSet: "
			       "this iovara works only for up to 3 core\n");
			return FALSE;
		}

		for (i = 0; i < count; i++) {
			value = (int32)htod32(values[i]);

			if ((i == 0) && (value < -1)) {
				/* Offset values (2nd/3rd arguments) can be negative */
				printf("wlmPhyForceCRSMinSet: "
				       "first value core0_th can not be negative, except for -1\n");
				return FALSE;
			}
			th[i] = (int8)value;
		}

		if ((err = wlu_var_setbuf(irh, "phy_force_crsmin", th, 4*sizeof(int8))) < 0) {
			printf("wlmPhyForceCRSMinSet: "
			       "falied to set phy_force_crsmin values\n");
			return FALSE;
		}
	}

	return TRUE;
}

int
wlmOlpcAnchorIndexGet(int core, int channel, int *olpc_idx, int *tempsense)
{
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;
	wl_olpc_pwr_t *olpc_pwr_ptr, olpc_pwr;

	void *ptr = NULL;
	int version_err = BCME_OK;
	int8 mode = 1;

	olpc_pwr_ptr = &olpc_pwr;
	olpc_pwr_ptr->core = core;
	olpc_pwr_ptr->channel = channel;
	olpc_pwr_ptr->version = TXCAL_IOVAR_VERSION;

	mode = wl_txcal_mode(irh);
	if (mode < 0) {
		printf("BPHY or OFDM rate needs to be specified properly!\n");
		return FALSE;
	}
	olpc_pwr_ptr->ofdm = mode;

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "olpc_anchoridx", (uint8*)&channel,
				sizeof(uint8), &ptr) < 0) {
			printf("wlmOlpcAnchorIndexGet: failed to retrieve"
					"anchor point gain index\n");
			return FALSE;
		}

		txcal_pwr_tssi_ptr = ptr;
		*olpc_idx = (int) (txcal_pwr_tssi_ptr->pwr_start_idx[core]);
		*tempsense = (int) (txcal_pwr_tssi_ptr->tempsense[core]);
		return TRUE;
	} else {
		if (wlu_var_getbuf_med(irh, "olpc_anchoridx", olpc_pwr_ptr,
			sizeof(wl_olpc_pwr_t), &ptr) < 0) {
			printf("wlmOlpcAnchorIndexGet: failed to retrieve"
					"anchor point gain index\n");
			return FALSE;
		}
		olpc_pwr_ptr = ptr;
		/* txcal version check */
		if (olpc_pwr_ptr->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", olpc_pwr_ptr->version);
			return FALSE;
		}
	   /* support for firmware with new txcal structures */
		*olpc_idx = (int) (olpc_pwr_ptr->olpc_idx);
		*tempsense = (int) (olpc_pwr_ptr->tempsense);
		return TRUE;
	}
}

int
wlmOlpcAnchorIndexSet(int core, int channel, int olpc_idx, int tempsense)
{
	wl_txcal_power_tssi_old_t txcal_pwr_tssi;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;
	wl_olpc_pwr_t *olpc_pwr_ptr, olpc_pwr;
	void *ptr = NULL;
	int version_err = BCME_OK;
	int8 mode = 1;

	olpc_pwr_ptr = &olpc_pwr;
	olpc_pwr_ptr->core = core;
	olpc_pwr_ptr->channel = channel;
	olpc_pwr_ptr->version = TXCAL_IOVAR_VERSION;

	mode = wl_txcal_mode(irh);
	if (mode < 0) {
		printf("BPHY or OFDM rate needs to be specified properly!\n");
		return FALSE;
	}
	olpc_pwr_ptr->ofdm = mode;

	version_err = wlu_var_getbuf_sm(irh, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if (wlu_var_getbuf_med(irh, "olpc_anchoridx", (uint8*)&channel,
				sizeof(uint8), &ptr) < 0) {
			printf("wlmOlpcAnchorIndexSet: failed to retrieve"
					"anchor point gain index\n");
			return FALSE;
		}

		txcal_pwr_tssi_ptr = ptr;
		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;
		txcal_pwr_tssi.channel = channel;
		txcal_pwr_tssi.tempsense[core] = tempsense;
		txcal_pwr_tssi.set_core = core;
		txcal_pwr_tssi.pwr_start_idx[core] = olpc_idx;
		if (wlu_var_setbuf(irh, "olpc_anchoridx", &txcal_pwr_tssi,
				sizeof(txcal_pwr_tssi)) < 0) {
			printf("wlmOlpcAnchorIndexSet: failed to set anchor point gain index\n");
			return FALSE;
		}

		return TRUE;

	} else {
		if (wlu_var_getbuf_med(irh, "olpc_anchoridx", olpc_pwr_ptr,
				sizeof(wl_olpc_pwr_t), &ptr) < 0) {
			printf("wlmOlpcAnchorIndexSet: failed to"
					"retrieve anchor point gain index\n");
			return FALSE;
		}
		/* txcal version check */
		if (olpc_pwr_ptr->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", olpc_pwr_ptr->version);
			return FALSE;
		}

	   /* support for firmware with new txcal structures */
		olpc_pwr_ptr->channel = channel;
		olpc_pwr_ptr->tempsense = tempsense;
		olpc_pwr_ptr->core = core;
		olpc_pwr_ptr->olpc_idx = olpc_idx;
		olpc_pwr_ptr->ofdm = mode;
		if (wlu_var_setbuf_sm(irh, "olpc_anchoridx", olpc_pwr_ptr,
				sizeof(wl_olpc_pwr_t)) < 0) {
			printf("wlmOlpcAnchorIndexSet: failed to set anchor point gain index\n");
			return FALSE;
		}
		return TRUE;
	}

}
int
wlmOlpcAnchorPower2GGet(int *power)
{
	if (wlu_iovar_getint(irh, "olpc_anchor2g", power)) {
		printf("wlmOlpcAnchorPower2GGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcAnchorPower2GSet(int power)
{
	if (wlu_iovar_setint(irh, "olpc_anchor2g", power)) {
		printf("wlmOlpcAnchorPower2GSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcAnchorPower5GGet(int *power)
{
	if (wlu_iovar_getint(irh, "olpc_anchor5g", power)) {
		printf("wlmOlpcAnchorPower5GGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcAnchorPower5GSet(int power)
{
	if (wlu_iovar_setint(irh, "olpc_anchor5g", power)) {
		printf("wlmOlpcAnchorPower5GSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcDisable(void)
{
	if (wlu_iovar_setint(irh, "disable_olpc", 1)) {
		printf("wlmOlpcDisable: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

int
wlmOlpcEanble(void)
{
	if (wlu_iovar_setint(irh, "disable_olpc", 0)) {
		printf("wlmOlpcEnable: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcThresholdGet(int *val)
{
	if (wlu_iovar_getint(irh, "olpc_thresh", val)) {
		printf("wlmOlpcThresholdGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcThresholdSet(int val)
{
	if (wlu_iovar_setint(irh, "olpc_thresh", val)) {
		printf("wlmOlpcThresholdSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcIndexValidGet(int *val)
{
	if (wlu_iovar_getint(irh, "olpc_idx_valid", val)) {
		printf("wlmOlpcIndexValidGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmOlpcIndexValidSet(int val)
{
	if (wlu_iovar_setint(irh, "olpc_idx_valid", val)) {
		printf("wlmOlpcIndexValidSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyTxPowerCapGet(int *txpwrcap)
{
	if (wlu_iovar_getint(irh, "phy_txpwrcap", txpwrcap)) {
		printf("wlmPhyTxPowerCapGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyTxPowerCapTableGet(int core,  int *numAnt, int *cellOnCaps, int *cellOffCaps)
{
	wl_txpwrcap_tbl_t * txpwrcap_tbl_ptr;
	void *ptr = NULL;
	uint8 i;

	if (wlu_var_getbuf_med(irh, "phy_txpwrcap_tbl", NULL, 0, &ptr) < 0) {
		printf("wlmPhyTxPowerCapTableGet: %s\n", wlmLastError());
		return FALSE;
	}

	txpwrcap_tbl_ptr = ptr;
	*numAnt = txpwrcap_tbl_ptr->num_antennas_per_core[core];

	/*
	printf("wl_txpwrcap_tbl_ptr->num_antennas_per_core:\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
		printf("%d ", txpwrcap_tbl_ptr->num_antennas_per_core[i]);
	}

	printf("\n");
	printf("pwrcap_cell_on:\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_ANTENNAS; i++) {
		printf("%d ", txpwrcap_tbl_ptr->pwrcap_cell_on[i]);
	}
	printf("\n");

	printf("pwrcap_cell_off\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_ANTENNAS; i++) {
		printf("%d ", txpwrcap_tbl_ptr->pwrcap_cell_off[i]);
	}
	*/

	for (i = 0; i < *numAnt; i++) {
		cellOnCaps[i] = txpwrcap_tbl_ptr->pwrcap_cell_on[core*2+i];
		cellOffCaps[i] = txpwrcap_tbl_ptr->pwrcap_cell_off[core*2+i];
	}

	return TRUE;
}

int
wlmPhyTxPowerCapTableSet(
int numOfCores,	int *numAntPerCore, int *cellOnCaps, int *cellOffCaps, int numOfCaps)
{
	wl_txpwrcap_tbl_t txpwrcap_tbl;
	uint8 num_antennas = 0;
	uint8 i;

	memset(txpwrcap_tbl.pwrcap_cell_on, 127,
	               TXPWRCAP_MAX_NUM_ANTENNAS *
	       sizeof(txpwrcap_tbl.pwrcap_cell_on[0]));
	memset(txpwrcap_tbl.pwrcap_cell_off, 127,
	               TXPWRCAP_MAX_NUM_ANTENNAS *
	       sizeof(txpwrcap_tbl.pwrcap_cell_off[0]));
	memset(txpwrcap_tbl.num_antennas_per_core, 0,
	       TXPWRCAP_MAX_NUM_CORES);

	for (i = 0; i < numOfCores; i++) {
		if ((numAntPerCore[i] < 1) || (numAntPerCore[i] > 2)) {
			printf("wlmPhyTxPwerTableSet:"
			       "invalid antenna number %d for core %d."
			       "should be [1, 2]\n", numAntPerCore[i], i);
			return FALSE;
		}
		txpwrcap_tbl.num_antennas_per_core[i] = numAntPerCore[i];
		num_antennas += txpwrcap_tbl.num_antennas_per_core[i];
	}

	if (numOfCaps != num_antennas) {
			printf("wlmPhyTxPwerTableSet:"
			       "expect %d caps numbers but we only got %d\n",
			       num_antennas, numOfCaps);
			return FALSE;
	}

	for (i = 0; i < num_antennas; i++) {
		txpwrcap_tbl.pwrcap_cell_on[i] = cellOnCaps[i];
		txpwrcap_tbl.pwrcap_cell_off[i] = cellOffCaps[i];
	}

	/*
	printf("wl_txpwrcap_tbl.num_antennas_per_core:\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
		printf("%d ", txpwrcap_tbl.num_antennas_per_core[i]);
	}

	printf("\n");
	printf("pwrcap_cell_on:\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_ANTENNAS; i++) {
		printf("%d ", txpwrcap_tbl.pwrcap_cell_on[i]);
	}
	printf("\n");
	printf("pwrcap_cell_off\n");
	for (i = 0; i < TXPWRCAP_MAX_NUM_ANTENNAS; i++) {
		printf("%d ", txpwrcap_tbl.pwrcap_cell_off[i]);
	}
	*/

	if (wlu_var_setbuf(irh, "phy_txpwrcap_tbl",  &txpwrcap_tbl, sizeof(txpwrcap_tbl)) < 0) {
		printf("wlmPhyTxPwerTableSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

/* phy_cellstatus */
int
wlmPhyCellStatusGet(int *status)
{
	if (wlu_iovar_getint(irh, "phy_cellstatus", status)) {
		printf("wlmPhyCellStatusGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyCellStatusSet(int status)
{
	if (wlu_iovar_setint(irh, "phy_cellstatus", status)) {
		printf("wlmPhyCellStatusSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyTxPowerControlGet(int* val)
{
	if (wlu_iovar_getint(irh, "phy_txpwrctrl", val)) {
		printf("wlmPhyTxPowerControlGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyTxPowerControlSet(int val)
{
	if (wlu_iovar_setint(irh, "phy_txpwrctrl", val)) {
		printf("wlmPhyTxPowerControlSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmClmLoad(char *fileName)
{
	if (process_clm_data(irh, fileName, 0) < 0) {
		printf("wlmClmLoad: download clm file %s failed. %s\n",
		       fileName, wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmClmLoadStatusGet(int *status)
{
	if (wlu_iovar_getint(irh, "clmload_status", status)) {
		printf("wlmClmLoadStatusGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmClmVersionGet(char *clmverstr, int len)
{
	void *ptr = NULL;

	if (wlu_var_getbuf_sm(irh, "clmver", NULL, 0, &ptr) < 0) {
		printf("wlmClmVersionGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* printf("wlmClmVersionGet: %s\n", ptr); */
	strncpy(clmverstr, ptr, len);
	return TRUE;
}

int
wlmTxCapLoad(char *fileName)
{
	if (process_txcap_data(irh, fileName) < 0) {
		printf("wlmTxCapLoad: download txcap file %s failed. %s\n",
		       fileName, wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmTxCapVersionGet(char *txcapver, int len)
{
	void *ptr = NULL;

	if (wlu_var_getbuf_sm(irh, "txcapver", NULL, 0, &ptr) < 0) {
		printf("wlmTxCapVersionGet: %s\n", wlmLastError());
		return FALSE;
	}

	/* printf("wlmTxCapVersionGet: %s\n", ptr); */
	strncpy(txcapver, ptr, len);

	return TRUE;
}

int
wlmSWDiversityStatsGet(char *stats, int length)
{
	char *statsbuf;
	void *ptr;
	struct wlc_swdiv_stats_v1 *cnt_v1;
	struct wlc_swdiv_stats_v2 *cnt;

	if (wlc_ver_major(irh) <= 5) /* Check for WLC major_version */
	{
		if (length < (int)sizeof(struct wlc_swdiv_stats_v1)) {
			printf("wlmSWDiversityStatsGet: "
		       "requested length %d is less than expected %d\n",
		       length, (int)sizeof(struct wlc_swdiv_stats_v2));
			return FALSE;
		}
	} else {

		if (length < (int)sizeof(struct wlc_swdiv_stats_v2)) {
			printf("wlmSWDiversityStatsGet: "
		       "requested length %d is less than expected %d\n",
		       length, (int)sizeof(struct wlc_swdiv_stats_v2));
			return FALSE;
		}
	}

	if (wlu_var_getbuf_med (irh, "swdiv_stats", NULL, 0, &ptr) < 0) {
		printf("wlmSWDiversityStatsGet: %s\n", wlmLastError());
		return FALSE;
	}

	statsbuf = (char *)ptr;

	if (wlc_ver_major(irh) <= 5) {
		cnt_v1 = (struct wlc_swdiv_stats_v1 *)CALLOC(sizeof(struct wlc_swdiv_stats_v1));
		if (cnt_v1 == NULL) {
			printf("wlmSWDiversityStatsGet: "
		       "Can not allocate %d bytes for wl swdiv stats struct\n",
		       (int)sizeof(struct wlc_swdiv_stats_v1));
			return FALSE;
		}
		memcpy(cnt_v1, statsbuf, sizeof(struct wlc_swdiv_stats_v1));
		WLMPRVALV1(auto_en); WLMPRVALV1(active_ant); WLMPRVALV1(rxcount); WLMPRNL();
		WLMPRVALV1(avg_snr_per_ant0); WLMPRVALV1(avg_snr_per_ant1);
		WLMPRVALV1(avg_snr_per_ant2); WLMPRNL();
		WLMPRVALV1(swap_ge_rxcount0); WLMPRVALV1(swap_ge_rxcount1); WLMPRNL();
		WLMPRVALV1(swap_ge_snrthresh0); WLMPRVALV1(swap_ge_snrthresh1); WLMPRNL();
		WLMPRVALV1(swap_txfail0); WLMPRVALV1(swap_txfail1); WLMPRNL();
		WLMPRVALV1(swap_timer0); WLMPRVALV1(swap_timer1); WLMPRNL();
		WLMPRVALV1(swap_alivecheck0); WLMPRVALV1(swap_alivecheck1); WLMPRNL();

	} else {
		cnt = (struct wlc_swdiv_stats_v2 *)CALLOC(sizeof(struct wlc_swdiv_stats_v2));
		if (cnt == NULL) {
			printf("wlmSWDiversityStatsGet: "
		       "Can not allocate %d bytes for wl swdiv stats struct\n",
		       (int)sizeof(struct wlc_swdiv_stats_v2));
			return FALSE;
		}
		memcpy(cnt, statsbuf, sizeof(struct wlc_swdiv_stats_v2));

		WLMPRVAL(auto_en); WLMPRVAL(active_ant); WLMPRVAL(rxcount); WLMPRNL();
		WLMPRVAL(avg_snr_per_ant0); WLMPRVAL(avg_snr_per_ant1);
		WLMPRVAL(avg_snr_per_ant2); WLMPRNL();
		WLMPRVAL(swap_ge_rxcount0); WLMPRVAL(swap_ge_rxcount1); WLMPRNL();
		WLMPRVAL(swap_ge_snrthresh0); WLMPRVAL(swap_ge_snrthresh1); WLMPRNL();
		WLMPRVAL(swap_txfail0); WLMPRVAL(swap_txfail1); WLMPRNL();
		WLMPRVAL(swap_timer0); WLMPRVAL(swap_timer1); WLMPRNL();
		WLMPRVAL(swap_alivecheck0); WLMPRVAL(swap_alivecheck1); WLMPRNL();
	}
	stats += sprintf(stats, "\n");
	printf("stats: %s\n", stats);

	return TRUE;
}

int
wlmCalLoad(char *fileName)
{
	if (process_cal_data(irh, fileName) < 0) {
		printf("wlmCalLoad: download TxCal file %s failed. %s\n",
		       fileName, wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmCalLoadStatusGet(int *status)
{
	if (wlu_iovar_getint(irh, "calload_status", status)) {
		printf("wlmCalLoadStatusGet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmCalDump(char *fileName)
{
	if (process_cal_dump(irh, fileName, NULL) < 0) {
		printf("wlmCalDump: dump cal file %s failed. %s\n",
		       fileName, wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyVcoCalSet(void)
{
	if (wlu_iovar_setint(irh, "phy_vcocal", 0) < 0) {
		printf("wlmPhyVcoCalSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmPhyRegGet(int offset, WLM_BAND band, int *val)
{
	int reg;
	struct {
		int val;
		int band;
	} x;

	x.val = 0;
	x.band = htod32(band);
	reg = htod32(offset);

	x.val = (x.val << 16) | (reg & 0xffff);
	x.val = htod32(x.val);

	if (wlu_get(irh, WLC_GET_PHYREG, &x, (int)sizeof(x)) < 0) {
		printf("wlmPhyRegGet: %s\n", wlmLastError());
		return FALSE;
	}

	*val = (uint16)(dtoh32(x.val));
	return TRUE;
}

int
wlmPhyRegSet(int offset, WLM_BAND band, int val)
{
	int reg;
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = htod32(val);
	reg = htod32(offset);

	x.val = (x.val << 16) | (reg & 0xffff);
	x.val = htod32(x.val);

	if (wlu_set(irh, WLC_SET_PHYREG, &x, sizeof(x)) < 0) {
		printf("wlmPhyRegSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmRadioRegGet(int offset, WLM_BAND band, int id, int *val)
{
	int reg;
	struct {
		int val;
		int band;
	} x;

	x.val = 0;
	x.band = htod32(band);
	reg = htod32(offset);

	/* Support AC PHY only:
	 * RADIO_2069_CORE_CR0  (0x0 << 9)
	 * RADIO_2069_CORE_CR1  (0x1 << 9)
	 * RADIO_2069_CORE_CR2  (0x2 << 9)
	 * RADIO_2069_CORE_ALL  (0x3 << 9)
	 * RADIO_2069_CORE_PLL  (0x4 << 9)
	 * RADIO_2069_CORE_PLL0 (0x4 << 9)
	 * RADIO_2069_CORE_PLL1 (0x5 << 9)
	 */

	reg |= (id << 9);
	x.val = (x.val << 16) | (reg & 0xffff);
	x.val = htod32(x.val);

	if (wlu_get(irh, WLC_GET_RADIOREG, &x, sizeof(x)) < 0) {
		printf("wlmRadioRegGet: %s\n", wlmLastError());
		return FALSE;
	}

	*val = (uint16)(dtoh32(x.val));

	return TRUE;
}

int
wlmRadioRegSet(int offset, WLM_BAND band, int id, int val)
{
	int reg;
	struct {
		int val;
		int band;
	} x;

	x.band = htod32(band);
	x.val = htod32(val);
	reg = htod32(offset);

	/* Support AC PHY only:
	 * RADIO_2069_CORE_CR0  (0x0 << 9)
	 * RADIO_2069_CORE_CR1  (0x1 << 9)
	 * RADIO_2069_CORE_CR2  (0x2 << 9)
	 * RADIO_2069_CORE_ALL  (0x3 << 9)
	 * RADIO_2069_CORE_PLL  (0x4 << 9)
	 * RADIO_2069_CORE_PLL0 (0x4 << 9)
	 * RADIO_2069_CORE_PLL1 (0x5 << 9)
	 */

	reg |= (id << 9);
	x.val = (x.val << 16) | (reg & 0xffff);
	x.val = htod32(x.val);

	if (wlu_set(irh, WLC_SET_RADIOREG, &x, sizeof(x)) < 0) {
		printf("wlmRadioRegSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmLDOSetVcoCal(int val, WLM_BAND band)
{
	if (!wlmRadioRegSet(0x8c, band, 4, val)) {
		printf("wlmLDOSetVcoCal: failed to set the LDO Vco value\n");
		return FALSE;
	}

	if (!wlmPhyVcoCalSet()) {
		printf("wlmLDOSetVcoCal: failed to run LDO Vco cal\n");
		return FALSE;
	}

	if (cmd_batching_mode)
		wlmSequenceDelay(2);
	else
		rwl_sleep(2);

	return TRUE;
}

int
wlmOTALoadTestFlowFile(char *flowfile)
{
	int ret = -1;
	char *args[2] = {0};

	if (!flowfile) {
		printf("wlmOTALoadTestFlowFile: Missing ota test flow file %s\n", flowfile);
		return FALSE;
	}

	args[0] = CALLOC(sizeof(char*) * (strlen(" ")+1));
	args[1] = CALLOC(sizeof(char*) * (strlen(flowfile)+1));

	if (args[0] == NULL || args[1] == NULL) {
		ret = -116;
		printf("Malloc failures, aborting download ota flow file\n");
		goto cleanup;
	}

	strncpy(args[0], " ", strlen(" "));
	strncpy(args[1], flowfile, strlen(flowfile));

	ret = ota_loadtest(irh, "ota_loadtest", (char **) args);

	if (ret < 0) {
		printf("wlmOtaLoadTestFlowFile: failed download test flow. ret == %d\n", ret);
		ret = FALSE;
	}
	else
		ret = TRUE;

cleanup:
	    if (args[0] != NULL)
		    free(args[0]);
	    if (args[1] != NULL)
		    free(args[1]);

	    return ret;
}

int
wlmOTATriggerTest()
{
	if (wlu_iovar_setint(irh, "ota_trigger", 1)) {
		printf("wlmOTATriggerTest: %s\n", wlmLastError());
		return FALSE;
	}
	return TRUE;
}

/* Returns the matching config table entry from the config_iovar_list for the passed config
 * iovar. If no matches are found, then returns the default (last) entry from the list
 */
static struct config_iovar_s *get_config_for_iovar(const char *iovar_name)
{
	int i = 0;

	while (config_iovar_list[i].iovar_name) {
		if (!stricmp(iovar_name, config_iovar_list[i].iovar_name))
			break;
		i++;
	}

	return &(config_iovar_list[i]);
}

int
wlmConfigSet(const char *iovar, char *config)
{
	wl_config_t cfg;
	int j = 0;
	bool found = 0;
	struct config_iovar_s *config_iovar;
	config_iovar = get_config_for_iovar(iovar);
	if (!config_iovar)
		return FALSE;
	while (config_iovar->params[j].name) {
		if (!stricmp(config_iovar->params[j].name, config)) {
			cfg.config = config_iovar->params[j].value;
			found = 1;
			break;
		}
		j++;
	}
	if (!found) {
	/* Check if an integer value is passed as the param */
	char *endptr = NULL;
	cfg.config = (uint32) strtol(config, &endptr, 0);
	if (*endptr == '\0')
		found = 1;
	}
	if (!found) {
		printf("Unsupported parameter [%s]\n", config);
		return FALSE;
	}
	cfg.config = htod32(cfg.config);
	if (wlu_var_setbuf(irh, iovar, &cfg, sizeof(wl_config_t))) {
		printf("wlm_ConfigSet: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int
wlmConfigGet(const char *iovar, char *status_str, unsigned int *val)
{
	wl_config_t *cfg;
	char *status = NULL;
	void *ptr = NULL;
	int i = 0;
	struct config_iovar_s *config_iovar;
	config_iovar = get_config_for_iovar(iovar);
	if (!config_iovar)
		return FALSE;
	if ((wlu_var_getbuf_sm(irh, iovar, NULL, 0, &ptr)) < 0) {
		printf("wlmConfigGet: %s\n", wlmLastError());
		return FALSE;
	}

	cfg = (wl_config_t *) ptr;
	cfg->config = dtoh32(cfg->config);
	cfg->status = dtoh32(cfg->status);
	*val = cfg->status;
	status = "";
	while (config_iovar->params[i].name) {
		if (config_iovar->params[i].value == cfg->status) {
			status = config_iovar->params[i].name;
			break;
		}
		i++;
	}
	strncpy(status_str, status, NAME_SIZE);
	return TRUE;
}

int
wlmOtatestStatus(int cnt)
{
	wl_ota_test_status_t *test_status = NULL;
	wl_ota_test_vector_t *ota_test_vctr = NULL;

	test_status = (wl_ota_test_status_t *)CALLOC(sizeof(wl_ota_test_status_t));
	if (test_status == NULL) {
		printf("Malloc Failure\n");
		return FALSE;
	}
	if (cnt) {
		if ((cnt < 1) || ((uint16)cnt > ARRAYSIZE(ota_test_vctr->test_arg))) {
			printf("Error, Out of range \n");
			return FALSE;
		}
		/* read nth test arg details */
		if (wlu_iovar_getbuf(irh, "ota_teststatus", &cnt, sizeof(uint16),
			test_status, sizeof(*test_status)) < 0) {
				return FALSE;
		}
		if (cnt > (test_status->test_cnt)) {
			printf("Error : Number of test seq downloaded %d  \n",
				test_status->test_cnt);
			return FALSE;
		}

		/* Display Test init info */
		wl_ota_display_test_init_info(test_status);

		/* Dsiplay test arg info */
		wl_ota_display_test_option(&(test_status->test_arg), cnt);
	} else {
		/* read back current state */
		if (wlu_iovar_getbuf(irh, "ota_teststatus", NULL, 0,
			test_status, sizeof(*test_status)) < 0) {
			return FALSE;
		}
		cnt = test_status->cur_test_cnt;

		switch (test_status->test_stage) {
			case WL_OTA_TEST_IDLE:		/* Idle state */
				printf("Init state \n");
				break;
			case WL_OTA_TEST_ACTIVE:	/* Active test state */
				/* Read back details for current test arg */
				cnt++;
				if (wlu_iovar_getbuf(irh, "ota_teststatus", &cnt, sizeof(uint16),
					test_status, sizeof(*test_status)) < 0) {
					return FALSE;
				}
				if (test_status->sync_status == WL_OTA_SYNC_ACTIVE)
					printf("Waiting for sync \n");
				else
					wl_ota_display_test_option(&(test_status->test_arg), cnt);
				break;
			case WL_OTA_TEST_SUCCESS:	/* Test Finished */
				printf("Test completed \n");
				break;
			case WL_OTA_TEST_FAIL:		/* Test Failed to complete */
				wl_otatest_display_skip_test_reason(test_status->skip_test_reason);
				break;
			default:
				printf("Invalid test Phase \n");
				break;
		}
	}
	return TRUE;
}

int
wlOtatestRssi()
{
	int16 cnt = 0, rssi;
	wl_ota_test_rssi_t *test_rssi = NULL;
	wl_ota_rx_rssi_t *rx_rssi = NULL;

	test_rssi = (wl_ota_test_rssi_t *)CALLOC(sizeof(wl_ota_test_rssi_t));
	if (test_rssi == NULL) {
		printf("Malloc Failure\n");
		return FALSE;
	}
	if (wlu_iovar_getbuf(irh, "ota_rssi", NULL, 0,
		test_rssi, sizeof(*test_rssi)) < 0) {
			return FALSE;
	}

	if (test_rssi->version != WL_OTARSSI_T_VERSION)
		return FALSE;

	rx_rssi = test_rssi->rx_rssi;
	for (cnt = 0; cnt < test_rssi->testcnt; cnt++) {
		rssi = dtoh16(rx_rssi[cnt].rssi);
		if (rssi < 0)
			printf("-%d.%02d  ", ((-rssi) >> 2),
				((-rssi) & 0x3)*25);
		else
			printf("%d.%02d  ", (rssi >> 2),
				(rssi & 0x3)*25);
	}
	printf("\n");
	return TRUE;
}

int wlmTwPwrOvrInitBaseIdx(int val)
{
	if (wlu_iovar_setint(irh, "phy_txpwr_ovrinitbaseidx", val)) {
		printf("wlmOvrInitBaseIdx: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmInitBaseIdx2g(int val)
{
	uint8 initidx2g = (uint8)val;
	if (wlu_iovar_setint(irh, "initbaseidx2g", initidx2g)) {
		printf("wlmInitBaseIdx2g: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmInitBaseIdx5g(int val)
{
	uint8 initidx5g = (uint8)val;
	if (wlu_iovar_setint(irh, "initbaseidx5g", initidx5g)) {
		printf("wlmInitBaseIdx5g: %s\n", wlmLastError());
		return FALSE;
	}

	return TRUE;
}

int wlmPhyTpcAvSet(int core, int subband, int val)
{
	uint8 Av_buff[3];
	char *buf;
	const char *iovar = "phy_tpc_av";
	const uint inbuf_size = 3 * sizeof(uint8);
	const uint buf_size = strlen(iovar) + 1 + inbuf_size;
	buf = CALLOC(sizeof(int) * buf_size);
	if (buf == NULL) {
		printf("wlmPhyTpcAvSet: failed to allocate memory\n");
		return FALSE;
	}

	Av_buff[0] = (uint8)core;
	Av_buff[1] = (uint8)subband;
	Av_buff[2] = (uint8)val;

	if (wlu_iovar_setbuf(irh, iovar, Av_buff, inbuf_size,
		buf, buf_size) < 0) {
		printf("wlmPhyTpcAvSet: %s\n", wlmLastError());
		return FALSE;
	}
	free(buf);

	return TRUE;
}

int wlmPhyTpcAvGet(int core, int subband, int *val)
{
	uint8 Av_buff[2];
	char *buf;
	const char *iovar = "phy_tpc_av";
	const uint inbuf_size = 2 * sizeof(uint8);
	const uint buf_size = strlen(iovar) + 1 + inbuf_size;
	buf = CALLOC(sizeof(int) * buf_size);
	if (buf == NULL) {
		printf("wlmPhyTpcAvGet: failed to allocate memory\n");
		return FALSE;
	}

	Av_buff[0] = (uint8)core;
	Av_buff[1] = (uint8)subband;

	if (wlu_iovar_getbuf(irh, iovar, Av_buff, inbuf_size,
		buf, buf_size) < 0) {
		printf("wlmPhyTpcAvGet: %s\n", wlmLastError());
		return FALSE;
	}
	*val = *(uint*)buf;
	free(buf);

	return TRUE;
}

int wlmPhyTpcVmidSet(int core, int subband, int val)
{
	uint8 Vmid_buff[3];
	char *buf;
	const char *iovar = "phy_tpc_vmid";
	const uint inbuf_size = 3 * sizeof(uint8);
	const uint buf_size = strlen(iovar) + 1 + inbuf_size;
	buf = CALLOC(sizeof(int) * buf_size);
	if (buf == NULL) {
		printf("wlmPhyTpcVmidSet: failed to allocate memory\n");
		return FALSE;
	}

	Vmid_buff[0] = (uint8)core;
	Vmid_buff[1] = (uint8)subband;
	Vmid_buff[2] = (uint8)val;

	if (wlu_iovar_setbuf(irh, iovar, Vmid_buff, inbuf_size,
		buf, buf_size) < 0) {
		printf("wlmPhyTpcVmidSet: %s\n", wlmLastError());
		return FALSE;
	}
	free(buf);

	return TRUE;
}

int wlmPhyTpcVmidGet(int core, int subband, int *val)
{
	uint8 Vmid_buff[2];
	char *buf;
	const char *iovar = "phy_tpc_vmid";
	const uint inbuf_size = 2 * sizeof(uint8);
	const uint buf_size = strlen(iovar) + 1 + inbuf_size;
	buf = CALLOC(sizeof(int) * buf_size);
	if (buf == NULL) {
		printf("wlmPhyTpcVmidGet: failed to allocate memory\n");
		return FALSE;
	}

	Vmid_buff[0] = (uint8)core;
	Vmid_buff[1] = (uint8)subband;

	if (wlu_iovar_getbuf(irh, iovar, Vmid_buff, inbuf_size,
		buf, buf_size) < 0) {
		printf("wlmPhyTpcVmidGet: %s\n", wlmLastError());
		return FALSE;
	}
	*val = *(uint*)buf;
	free(buf);

	return TRUE;
}

int
wlmBtcoexDesenseRxgainGet(WLM_BAND *band, int *num_cores, int *desense_array)
{
	wl_desense_restage_gain_t *desense_restage_gain_ptr, desense_restage_gain;
	void *ptr = NULL;
	uint32 phytype;
	int16 cnt = 0;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype == WLC_PHY_TYPE_AC) {
		desense_restage_gain_ptr = &desense_restage_gain;
		desense_restage_gain_ptr->version =  0;
		desense_restage_gain_ptr->length = sizeof(desense_restage_gain);

		if (wlu_var_getbuf(irh, "phy_btcoex_desense_rxgain", desense_restage_gain_ptr,
				sizeof(wl_desense_restage_gain_t), &ptr) < 0) {
			printf("wlmBtcoexDesenseRxgainGet: failed to retrieve"
							"rxgain desense mode \n");
			return FALSE;
		}
		desense_restage_gain_ptr = ptr;

		*band = (uint) (desense_restage_gain_ptr->band);
		*num_cores = (uint8) (desense_restage_gain_ptr->num_cores);
		for (cnt = 0; cnt < desense_restage_gain_ptr->num_cores; cnt++) {
			desense_array[cnt] = (desense_restage_gain_ptr->desense_array[cnt]);
		}

		return TRUE;
	} else {
		printf("wlmBtcoexDesenseRxgainGet: "
			"phy type %d is not suppprted\n", phytype);
		return FALSE;
	}
}

int
wlmBtcoexDesenseRxgainSet(WLM_BAND band, int num_cores, int *desense_array)
{
	wl_desense_restage_gain_t *desense_restage_gain_ptr, desense_restage_gain;
	uint32 phytype;
	int16 cnt = 0;

	if (curPhyType == PHY_TYPE_NULL)
		phytype = wlmPhyTypeGet();
	else
		phytype = curPhyType;

	if (phytype == WLC_PHY_TYPE_AC) {
		desense_restage_gain_ptr = &desense_restage_gain;

		desense_restage_gain_ptr->version =  0;
		desense_restage_gain_ptr->length = sizeof(desense_restage_gain);
		desense_restage_gain_ptr->band = (uint) band;

		desense_restage_gain_ptr->num_cores = (uint8) num_cores;
		for (cnt = 0; cnt < num_cores; cnt++) {
			desense_restage_gain_ptr->desense_array[cnt] = (uint8) desense_array[cnt];
		}

		if (wlu_var_setbuf(irh, "phy_btcoex_desense_rxgain", desense_restage_gain_ptr,
				sizeof(desense_restage_gain_ptr)) < 0) {
			printf("wlmBtcoexDesenseRxgainSet: failed to set btcoex rxgain desense\n");
			return FALSE;
		}
		return TRUE;
	} else {
		printf("wlmBtcoexDesenseRxgainSet: "
			"phy type %d is not suppprted\n", phytype);
		return FALSE;
	}
}
