/*
 * WPS main (platform dependent portion)
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
 * $Id: wps_linux_main.c 772297 2019-02-20 06:50:21Z $
 */
#include <stdio.h>
#include <linux/types.h>
#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <typedefs.h>
#include <linux/sockios.h>
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#include <linux/ethtool.h>

#include <bn.h>
#include <wps_dh.h>
#include <md5.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <bcmnvram.h>
#include <wpstypes.h>
#include <sminfo.h>
#include <wps_apapi.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <wlutils.h>
#include <tutrace.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <shutils.h>
#ifdef WPS_UPNP_DEVICE
#include <ap_upnp_sm.h>
#endif // endif
#include <wlif_utils.h>
#include <bcmparams.h>
#include <security_ipc.h>
#include <wps_ui.h>
#include <wps_wps.h>
#include <wps_wl.h>
#ifdef BCMWPSAPSTA
#include <wps_pb.h>
#endif // endif
#include <wps_aplockdown.h>
#include <wps_apputils.h>
#ifdef BCA_CPEROUTER
#ifdef BCA_SUPPORT_UNFWLCFG
/* In UNFWLCFG project, nothing to do for the lock functions temporarily.
 * We will implement it once encountering issues.
*/
#define brcm_get_lock(name, timeout)
#define brcm_release_lock(name);
#else
extern void brcm_get_lock(char *name, int timeout);
extern void brcm_release_lock(char *name);
#endif /* BCA_SUPPORT_UNFWLCFG */
extern void cperouter_restart_wlan(void);
#endif /* BCA_CPEROUTER */
#define WPSM_PID_FILE_PATH	"/tmp/wps_monitor.pid"

extern void RAND_linux_init();
void wps_osl_build_conf();

char wps_conf_buf[1024 * 4];
char *wps_conf_buf_ptr;
static int wps_conf_num;
static char *wps_conf_list[256];

void
wps_osl_restart_wl()
{
#ifdef __CONFIG_STBAP__
	char *wps_argv[] = {"rc", "restart", NULL};
	pid_t pid;
#endif /* __CONFIG_STBAP__ */
	/* Wait for M8/DONE packet completed */
	WpsSleep(1);
	TUTRACE((TUTRACE_ERR, "%s:rc restart -> \n", __FUNCTION__));
#ifdef __CONFIG_STBAP__
	/* STB maps 'kill -1 1' to /etc/inittab, AP maps it to 'rc restart' */
	_eval(wps_argv, NULL, 0, &pid);
#else /* __CONFIG_STBAP__ */
#ifdef BCA_CPEROUTER
	cperouter_restart_wlan();
#else
	/* restart all process */
	system("kill -1 1");
#endif /* BCA_CPEROUTER */
#endif /* __CONFIG_STBAP__ */
}

int
wps_getProcessStates()
{
	int value;

	value = atoi(nvram_safe_get("wps_proc_status"));
	switch (value) {
	case WPS_UI_INIT:		value = WPS_INIT;		break;
	case WPS_UI_ASSOCIATED:		value = WPS_ASSOCIATED;		break;
	case WPS_UI_OK:			value = WPS_OK;			break;
	case WPS_UI_MSG_ERR:		value = WPS_MSG_ERR;		break;
	case WPS_UI_TIMEOUT:		value = WPS_TIMEOUT;		break;
	case WPS_UI_SENDM2:		value = WPS_SENDM2;		break;
	case WPS_UI_SENDM7:		value = WPS_SENDM7;		break;
	case WPS_UI_MSGDONE:		value = WPS_MSGDONE;		break;
	case WPS_UI_PBCOVERLAP:		value = WPS_PBCOVERLAP;		break;
	case WPS_UI_FIND_PBC_AP:	value = WPS_FIND_PBC_AP;	break;
	case WPS_UI_FIND_SEL_AP:	value = WPS_FIND_SEL_AP;	break;
	case WPS_UI_ASSOCIATING:	value = WPS_ASSOCIATING;	break;
	case WPS_UI_NFC_WR_CFG:		value = WPS_NFC_WR_CFG;		break;
	case WPS_UI_NFC_WR_PW:		value = WPS_NFC_WR_PW;		break;
	case WPS_UI_NFC_WR_CPLT:	value = WPS_NFC_WR_CPLT;	break;
	case WPS_UI_NFC_RD_CFG:		value = WPS_NFC_RD_CFG;		break;
	case WPS_UI_NFC_RD_PW:		value = WPS_NFC_RD_PW;		break;
	case WPS_UI_NFC_RD_CPLT:	value = WPS_NFC_RD_CPLT;	break;
	case WPS_UI_NFC_HO_S:		value = WPS_NFC_HO_S;		break;
	case WPS_UI_NFC_HO_R:		value = WPS_NFC_HO_R;		break;
	case WPS_UI_NFC_HO_NDEF:	value = WPS_NFC_HO_NDEF;	break;
	case WPS_UI_NFC_HO_CPLT:	value = WPS_NFC_HO_CPLT;	break;
	case WPS_UI_NFC_OP_ERROR:	value = WPS_NFC_OP_ERROR;	break;
	case WPS_UI_NFC_OP_STOP:	value = WPS_NFC_OP_STOP;	break;
	case WPS_UI_NFC_OP_TO:		value = WPS_NFC_OP_TO;		break;
	case WPS_UI_NFC_FM:		value = WPS_NFC_FM;		break;
	case WPS_UI_NFC_FM_CPLT:	value = WPS_NFC_FM_CPLT;	break;
	case WPS_UI_MAP_TIMEOUT:	value = WPS_MAP_TIMEOUT;	break;
	case WPS_UI_NFC_HO_DPI_MISMATCH:	value = WPS_NFC_HO_DPI_MISMATCH;	break;
	case WPS_UI_NFC_HO_PKH_MISMATCH:	value = WPS_NFC_HO_PKH_MISMATCH;	break;

	default:
		TUTRACE((TUTRACE_ERR, "Unknow nv process state %d\n", value));
		value = WPS_INIT;
	}

	return value;
}

void
wps_setProcessStates(int state)
{
	int value;
	char buf[32];
	static int prev_state = WPS_INIT;

	switch (state) {
	case WPS_INIT:		value = WPS_UI_INIT;		break;
	case WPS_ASSOCIATED:	value = WPS_UI_ASSOCIATED;	break;
	case WPS_OK:		value = WPS_UI_OK;		break;
	case WPS_MSG_ERR:	value = WPS_UI_MSG_ERR;		break;
	case WPS_TIMEOUT:	value = WPS_UI_TIMEOUT;		break;
	case WPS_SENDM2:	value = WPS_UI_SENDM2;		break;
	case WPS_SENDM7:	value = WPS_UI_SENDM7;		break;
	case WPS_MSGDONE:	value = WPS_UI_MSGDONE;		break;
	case WPS_PBCOVERLAP:	value = WPS_UI_PBCOVERLAP;	break;
	case WPS_FIND_PBC_AP:	value = WPS_UI_FIND_PBC_AP;	break;
	case WPS_FIND_SEL_AP:	value = WPS_UI_FIND_SEL_AP;	break;
	case WPS_ASSOCIATING:	value = WPS_UI_ASSOCIATING;	break;
	case WPS_NFC_WR_CFG:	value = WPS_UI_NFC_WR_CFG;	break;
	case WPS_NFC_WR_PW:	value = WPS_UI_NFC_WR_PW;	break;
	case WPS_NFC_WR_CPLT:	value = WPS_UI_NFC_WR_CPLT;	break;
	case WPS_NFC_RD_CFG:	value = WPS_UI_NFC_RD_CFG;	break;
	case WPS_NFC_RD_PW:	value = WPS_UI_NFC_RD_PW;	break;
	case WPS_NFC_RD_CPLT:	value = WPS_UI_NFC_RD_CPLT;	break;
	case WPS_NFC_HO_S:	value = WPS_UI_NFC_HO_S;	break;
	case WPS_NFC_HO_R:	value = WPS_UI_NFC_HO_R;	break;
	case WPS_NFC_HO_NDEF:	value = WPS_UI_NFC_HO_NDEF;	break;
	case WPS_NFC_HO_CPLT:	value = WPS_UI_NFC_HO_CPLT;	break;
	case WPS_NFC_OP_ERROR:	value = WPS_UI_NFC_OP_ERROR;	break;
	case WPS_NFC_OP_STOP:	value = WPS_UI_NFC_OP_STOP;	break;
	case WPS_NFC_OP_TO:	value = WPS_UI_NFC_OP_TO;	break;
	case WPS_NFC_FM:	value = WPS_UI_NFC_FM;		break;
	case WPS_NFC_FM_CPLT:	value = WPS_UI_NFC_FM_CPLT;	break;
	case WPS_MAP_TIMEOUT:	value = WPS_UI_MAP_TIMEOUT;	break;
	case WPS_NFC_HO_DPI_MISMATCH:	value = WPS_UI_NFC_HO_DPI_MISMATCH;	break;
	case WPS_NFC_HO_PKH_MISMATCH:	value = WPS_UI_NFC_HO_PKH_MISMATCH;	break;
	default:
		TUTRACE((TUTRACE_ERR, "Unknow process state %d\n", state));
		goto done;
	}

	sprintf(buf, "%d", value);
	wps_ui_set_env("wps_proc_status", buf);
	nvram_set("wps_proc_status", buf);

#ifdef WPS_NFC_DEVICE
	/* Add external NFC Token bit */
	if (state == WPS_NFC_WR_CPLT) {
		value = strtoul(nvram_safe_get("wps_config_method"), NULL, 16);

		if (!(value & WPS_CONFMET_EXT_NFC_TOK)) {
			value |= WPS_CONFMET_EXT_NFC_TOK;
			sprintf(buf, "0x%x", value);
			nvram_set("wps_config_method", buf);

			/* Update the wps config table from nvram config for PW token, because
			 * write PW token successfully doesn't restart the AP, we need to update
			 * the wps config table manually for wps_config_method variable.
			 */
			if (prev_state == WPS_NFC_WR_PW) {
				wps_osl_build_conf();
				wps_conf_upd(wps_conf_num, wps_conf_list);
			}
		}
	}
#endif /* WPS_NFC_DEVICE */

done:
	prev_state = state;
	return;
}

void
wps_setStaDevName(char *str)
{
	if (str) {
		wps_ui_set_env("wps_sta_devname", str);
		nvram_set("wps_sta_devname", str);
	}

	return;
}

void
wps_setPinFailInfo(uint8 *mac, char *name, char *state)
{
	char buf[32] = {0};

	if (mac)
		nvram_set("wps_pinfail_mac", ether_etoa(mac, buf));
	else
		nvram_set("wps_pinfail_mac", "");

	if (name)
		nvram_set("wps_pinfail_name", name);
	else
		nvram_set("wps_pinfail_name", "");

	if (state) {
		wps_ui_set_env("wps_pinfail_state", state);
		nvram_set("wps_pinfail_state", state);
	}
	else {
		wps_ui_set_env("wps_pinfail_state", "");
		nvram_set("wps_pinfail_state", "");
	}
}

void
wps_setWPSSuccessMode(int state)
{
	char buf[32];

	sprintf(buf, "%d", state);
	nvram_set("wps_success_mode", buf);
	return;
}

static int
findstr(char *s, char *t)
{
	int len_t = strlen(t);
	int len_s = strlen(s);
	int i;

	if (len_t > len_s)
		return 0;
	for (i = 0; i <= (len_s - len_t); i++) {
		if (*(s+i) == *t)
			if (strstr(s+i, t))
				return 1;
	}

	return 0;
}

static int
print_hex(char *str, const void *bytes, int len)
{
	int i;
	char *p = str;
	const uint8 *src = (const uint8*)bytes;

	for (i = 0; i < len; i++) {
		p += snprintf(p, 3, "%02X", *src);
		src++;
	}
	return (int)(p - str);
}

static int
set_wep_key(char *wl_keyi, char *key, int key_len)
{
	char keystr[WEP128_KEY_HEX_SIZE+1] = {0};

	switch (key_len) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		print_hex(keystr, key, key_len);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		memcpy(keystr, key, key_len);
		break;
	default:
		TUTRACE((TUTRACE_ERR, "Wrong Key length %d\n", key_len));
		return -1;
	}

	wps_osl_set_conf(wl_keyi, keystr);

	return 0;
}

static int
set_wsec(char *ifname, void *credential, int mode)
{
	char tmp[128];
	unsigned char psk_mode = 0;
	WpsEnrCred *cred = (WpsEnrCred *)credential;
	char prefix[] = "wlXXXXXXXXXX_";
	bool b_wps_version2 = false;
	char *value;

	value = nvram_get("wps_version2");
	if (value && !strcmp(value, "enabled"))
		b_wps_version2 = true;

	/* empty credential check */
	if (cred->ssidLen == 0) {
		TUTRACE((TUTRACE_INFO, "Ignore apply new credential because ssid is empty\n"));
		return 0;
	}

	TUTRACE((TUTRACE_INFO,
		"nvram set key = %s keyMgmt = %s ssid = %s(b_configured)\n",
		cred->nwKey, cred->keyMgmt, cred->ssid));

	/* convert os name to wl name */
	if (osifname_to_nvifname(ifname, prefix, sizeof(prefix)) != 0) {
		TUTRACE((TUTRACE_INFO, "Convert to nvname failed\n"));
		return 0;
	}
	strcat(prefix, "_");

	/* Check credential */
	if (findstr(cred->keyMgmt, "WPA-PSK"))
		psk_mode |= 1;
	if (findstr(cred->keyMgmt, "WPA2-PSK"))
		psk_mode |= 2;

	/* for version 2, force psk2 if psk1 is on */
	if (b_wps_version2 && (psk_mode & 1)) {
		psk_mode |= 2;
	}

	switch (psk_mode) {
	case 1:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk ");
		break;
	case 2:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk2 ");
		break;
	case 3:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "psk psk2 ");
		break;
	default:
		wps_osl_set_conf(strcat_r(prefix, "akm", tmp), "");
		break;
	}

	if (findstr(cred->keyMgmt, "SHARED"))
		wps_osl_set_conf(strcat_r(prefix, "auth", tmp), "1");
	else
		wps_osl_set_conf(strcat_r(prefix, "auth", tmp), "0");

	/* set SSID */
	wps_osl_set_conf(strcat_r(prefix, "ssid", tmp), cred->ssid);
	if (psk_mode)
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "disabled");

	/* for version 2, force aes if tkip is on */
	if (b_wps_version2 && (cred->encrType & WPS_ENCRTYPE_TKIP)) {
		cred->encrType |= WPS_ENCRTYPE_AES;
	}

	/* set Encr type */
	if (cred->encrType == WPS_ENCRTYPE_NONE)
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "disabled");
	else if (cred->encrType == WPS_ENCRTYPE_WEP)
		wps_osl_set_conf(strcat_r(prefix, "wep", tmp), "enabled");
	else if (cred->encrType == WPS_ENCRTYPE_TKIP)
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip");
	else if (cred->encrType == WPS_ENCRTYPE_AES)
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "aes");
	else if (cred->encrType == (WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES))
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip+aes");
	else
		wps_osl_set_conf(strcat_r(prefix, "crypto", tmp), "tkip");

	if (cred->encrType == WPS_ENCRTYPE_WEP) {
		char buf[16] = {0};
		sprintf(buf, "%d", cred->wepIndex);
		wps_osl_set_conf(strcat_r(prefix, "key", tmp), buf);
		sprintf(buf, "key%d", cred->wepIndex);
		set_wep_key(strcat_r(prefix, buf, tmp), cred->nwKey, cred->nwKeyLen);
	}
	else {
		/* set key */
		if (cred->nwKeyLen < 64) {
			wps_osl_set_conf(strcat_r(prefix, "wpa_psk", tmp), cred->nwKey);
		}
		else {
			char temp_key[65] = {0};
			memcpy(temp_key, cred->nwKey, 64);
			temp_key[64] = 0;
			wps_osl_set_conf(strcat_r(prefix, "wpa_psk", tmp), temp_key);
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	switch (cred->encrType) {
	case WPS_ENCRTYPE_WEP:
	case WPS_ENCRTYPE_TKIP:
		wps_osl_set_conf(strcat_r(prefix, "nmode", tmp), "0");
		break;
	default:
		wps_osl_set_conf(strcat_r(prefix, "nmode", tmp), "-1");
		break;
	}

	return 1;
}

int
wps_set_wsec(int ess_id, char *wps_ifname, void *credential, int mode)
{
	char prefix[] = "lanXXXXXXXXX_";
	int instance;
	char *value;
	char *oob = NULL;

	char tmp[100];
	char *wlnames = wps_ifname;
	char *next = NULL;
	char ifname[IFNAMSIZ];

	if (wps_ifname == NULL || credential == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_set_wsec: Invaild argument\n"));
		return -1;
	}
#ifdef BCA_CPEROUTER
	brcm_get_lock("wps", 200);
#endif /* BCA_CPEROUTER */
#ifdef BCMWPSAPSTA
	/* only support on URE WRE mode */
	snprintf(tmp, sizeof(tmp), "%s_ure_mode", wps_ifname);
	value = wps_safe_get_conf(tmp);
	if (strcmp(value, "wre") == 0 &&
		strcmp(wps_safe_get_conf("wps_pbc_apsta"), "enabled") == 0 &&
		strcmp(wps_ifname,
			wps_safe_get_conf(wps_pbc_sta_ifnames[0].name)) == 0) {

		char *ap_ifname = wps_safe_get_conf(wps_pbc_ap_ifnames[0].name);

		/* Set wsec to ap interface */

		value = wps_get_conf("wps_pbc_ap_index");
		if (value == NULL) {
#ifdef BCA_CPEROUTER
			brcm_release_lock("wps");
#endif /* BCA_CPEROUTER */
			TUTRACE((TUTRACE_ERR, "wps_set_wsec: Cannot get UPnP instance\n"));
			return -1;
		}

		instance = atoi(value);
		if (instance == 0)
			strcpy(prefix, "lan_");
		else
			sprintf(prefix, "lan%d_", instance);
		wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
			"disabled");

		wps_osl_set_conf(strcat_r(prefix, "wps_reg", tmp),
			"enabled");
		TUTRACE((TUTRACE_INFO, "built-in registrar enabled\n"));

		wlnames = ap_ifname;
		TUTRACE((TUTRACE_INFO, "WPS STA (%s) has finished WPS process "
			"and now is setting security to AP (%s).\n", wps_ifname, ap_ifname));
	}
	else
#endif /* BCMWPSAPSTA */
	if (!wps_is_wps_sta(wps_ifname)) {
		sprintf(tmp, "ess%d_ap_index", ess_id);
		value = wps_get_conf(tmp);
		if (value == NULL) {
#ifdef BCA_CPEROUTER
			brcm_release_lock("wps");
#endif /* BCA_CPEROUTER */
			TUTRACE((TUTRACE_ERR, "wps_set_wsec: Cannot get UPnP instance\n"));

			return -1;
		}

		instance = atoi(value);
		if (instance == 0)
			strcpy(prefix, "lan_");
		else
			sprintf(prefix, "lan%d_", instance);

		/* Default OOB set to enabled */
		oob = nvram_get(strcat_r(prefix, "wps_oob", tmp));
		if (!oob)
			oob = "enabled";

		/* Check wether OOB is true */
		if (!strcmp(oob, "enabled")) {
			/* OOB mode, apply to all wl if */
			sprintf(tmp, "ess%d_wlnames", ess_id);
			wlnames = wps_safe_get_conf(tmp);

			TUTRACE((TUTRACE_INFO,
				"wps_set_wsec: OOB set config\n"));
		}

		/* Set OOB and built-in reg (Per-ESS) */
		if (mode == SCMODE_AP_REGISTRAR) {
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
				"disabled");
			TUTRACE((TUTRACE_INFO, "OOB state configed\n"));

			wps_osl_set_conf(strcat_r(prefix, "wps_reg", tmp),
				"enabled");
			TUTRACE((TUTRACE_INFO, "built-in registrar enabled\n"));
		} else if (mode == SCMODE_AP_ENROLLEE) {
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp),
				"disabled");
			TUTRACE((TUTRACE_INFO, "OOB state configed\n"));
		}
	}
	else {
		/* STA ess interfaces */
		if (osifname_to_nvifname(wlnames, prefix, sizeof(prefix)) == 0) {
			strcat(prefix, "_");
			wps_osl_set_conf(strcat_r(prefix, "wps_oob", tmp), "disabled");
		}
		else {
			TUTRACE((TUTRACE_INFO, "Clear OOB: convert to nvname failed!\n"));
		}
	}

	/* Apply the wsec */
	foreach(ifname, wlnames, next) {
		set_wsec(ifname, credential, mode);
		TUTRACE((TUTRACE_INFO, "wps_set_wsec: Set config to %s\n", ifname));
	}

	/*
	 * Do commit
	 */
#ifdef BCA_CPEROUTER
	brcm_release_lock("wps");
#else
	/* for CPEROUTER, don't need commit */
	nvram_commit();
#endif /* BCA_CPEROUTER */

	TUTRACE((TUTRACE_INFO, "wps: wsec configuraiton set completed\n"));
	return 1;
}

int
wps_osl_set_conf(char *config_name, char *config_string)
{
	if (config_name && config_string) {
		TUTRACE((TUTRACE_INFO,	"wps_osl_set_conf: Set %s = %s\n",
			config_name, config_string));
		nvram_set(config_name, config_string);
	}

	return 0;
}

static char *
print_conf(char *os_ifname, char *nv_ifname, char *name, char *value)
{
	char os_prefix[] = "wlXXXXXXXXXX_";
	char nv_prefix[] = "wlXXXXXXXXXX_";
	char tmp[100];

	/* Set up os prefix */
	if (os_ifname)
		snprintf(os_prefix, sizeof(os_prefix), "%s_", os_ifname);
	else
		os_prefix[0] = 0;

	/* Set up nv prefix */
	if (nv_ifname)
		snprintf(nv_prefix, sizeof(nv_prefix), "%s_", nv_ifname);
	else
		nv_prefix[0] = 0;

	/* If value is null, get from NVRAM. */
	if (value == NULL) {
		value = nvram_get(strcat_r(nv_prefix, name, tmp));
		if (!value)
			return NULL;
	}

	wps_conf_buf_ptr += sprintf(wps_conf_buf_ptr, "%s%s=%s\n",
		os_prefix, name, value);
	TUTRACE((TUTRACE_INFO, "%s%s=%s\n", os_prefix, name, value));
	return value;
}

static unsigned char*
create_device_uuid_string(unsigned char *uuid_string)
{
	unsigned char mac[6] = {0};
	char deviceType[] = "urn:schemas-wifialliance-org:device:WFADevice";
	char *lanmac_str;
	MD5_CTX mdContext;
	unsigned char uuid[16];

	lanmac_str = nvram_get("lan_hwaddr");
	if (lanmac_str)
		ether_atoe(lanmac_str, mac);

	/* Generate hash */
	MD5Init(&mdContext);
	MD5Update(&mdContext, mac, 6);
	MD5Update(&mdContext, (unsigned char *)deviceType, strlen(deviceType));
	MD5Final(uuid, &mdContext);

	sprintf((char *)uuid_string,
		"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

	return uuid_string;
}

static void
wps_osl_validate_default()
{
	int dirty = 0;
	unsigned short config_method, config_method_org, config_method_v1;
	char *value, tmp[128], devPwd[9];
	bool b_wps_version2 = false;

	/* Validate wps_device_pin existance,  embedded nvram can override it */
	value = nvram_get("wps_device_pin");
	if (!value || strcmp(value, "12345670") == 0) {
		/* Generate a random device's PIN */
		if (wps_gen_pin(devPwd, sizeof(devPwd)) == WPS_SUCCESS)
			nvram_set("wps_device_pin", devPwd);
		else {
			TUTRACE((TUTRACE_ERR, "Generate new random AP PIN failed\n"));
			nvram_set("wps_device_pin", "12345670");
		}
		dirty++;
	}

	/* WSC 2.0 support */
	value = nvram_get("wps_version2");
	if (value && !strcmp(value, "enabled"))
		b_wps_version2 = true;

	/* Set wps_config_method default value */
	value = nvram_get("wps_config_method");
	if (!value) {
		if (b_wps_version2) {
			config_method = (WPS_CONFMET_VIRT_PBC | WPS_CONFMET_PHY_PBC |
				WPS_CONFMET_VIRT_DISPLAY | WPS_CONFMET_PBC |
				WPS_CONFMET_DISPLAY);
		}
		else {
			/* For DTM 1.1 test */
			/* config_method = (WPS_CONFMET_PBC | WPS_CONFMET_DISPLAY); */
			config_method = (WPS_CONFMET_PBC | WPS_CONFMET_LABEL);
		}

		sprintf(tmp, "0x%x", config_method);
		nvram_set("wps_config_method", tmp);
		dirty++;
	}
	else {
		config_method = strtoul(value, NULL, 16);
		if (b_wps_version2) {
			config_method_org = config_method;

			/* We expect that add virtual PBC if PBC supported,
			 * add virtual DISPLAY if DISPLAY supported.
			 */
			if (config_method & WPS_CONFMET_PBC)
				config_method |= WPS_CONFMET_VIRT_PBC;
			if (config_method & WPS_CONFMET_DISPLAY)
				config_method |= WPS_CONFMET_VIRT_DISPLAY;

			if (config_method != config_method_org) {
				sprintf(tmp, "0x%x", config_method);
				nvram_set("wps_config_method", tmp);
				dirty++;
			}
		}
		else {
			/* Is it compatible with V1 */
			config_method_v1 = config_method;
			config_method_v1 &= ~(0x200 | 0x400 | 0x2000 | 0x4000);

			if (config_method != config_method_v1) {
				sprintf(tmp, "0x%x", config_method_v1);
				nvram_set("wps_config_method", tmp);
				dirty++;
			}
		}
	}

#ifdef WPS_NFC_DEVICE
	if (!(config_method & WPS_CONFMET_NFC_INTF)) {
		config_method |= WPS_CONFMET_NFC_INTF;
		sprintf(tmp, "0x%x", config_method);
		nvram_set("wps_config_method", tmp);
		dirty++;
	}

	if (config_method & WPS_CONFMET_EXT_NFC_TOK) {
		value = nvram_get("lan_wps_oob");
		if (!value || !strcmp(value, "enabled")) {
			/* Remove the external NFC Token bit when OOB. */
			config_method &= ~WPS_CONFMET_EXT_NFC_TOK;
			sprintf(tmp, "0x%x", config_method);
			nvram_set("wps_config_method", tmp);
			dirty++;
		}
	}
#endif /* WPS_NFC_DEVICE */

	/* Commit */
	if (dirty)
		nvram_commit();
}

static bool
is_wps_pbc_apsta_enabled(int ess_num, int ap_num, int sta_num)
{
	TUTRACE((TUTRACE_INFO, "ap_num = %d, sta_num = %d, ess_num = %d\n",
		ap_num, sta_num, ess_num));

	if (strcmp(nvram_safe_get("wps_pbc_apsta"), "enabled") != 0) {
		TUTRACE((TUTRACE_INFO, "nvram wps_pbc_apsta is disable ...\n"));
		return FALSE;
	}

	if ((ess_num >= 2) && (ap_num >= 1) && (sta_num >= 1)) {
		TUTRACE((TUTRACE_INFO, "PBC APSTA mode enabled .....\n"));
		return TRUE;
	}

	TUTRACE((TUTRACE_INFO, "PBC APSTA mdoe disable .....\n"));

	return FALSE;
}

void
wps_osl_build_conf()
{
	int i, unit;
	char *value;
	char *item, *p, *next;
#ifdef WPS_UPNP_DEVICE
	char *ifname;
	char lan_ifname[IFNAMSIZ];
#endif // endif
	char *ifnames;
	char wlnames[256];
	char stanames[256];
	char prefix[] = "wlXXXXXXXXXX_";
	char name[IFNAMSIZ], os_name[IFNAMSIZ], wl_name[IFNAMSIZ];
	char ess_name[IFNAMSIZ], lan_name[IFNAMSIZ];
	char tmp[100];
	int bandtype = 0;
	int band = 0;
	char key[8];
	int ess_id = 0;
	int ap_index;
	char *wl_mode;
	char *wl_radio, *wl_bss_enabled;
	unsigned char uuid_string[36];
	int wps_pbc_ap_index = 0;
#ifdef BCMWPSAPSTA
	int ap_idx = 0, sta_idx = 0;
#endif // endif
	/* Init print pointer */
	wps_conf_buf_ptr = wps_conf_buf;

	/* Start config print */
	print_conf(NULL, NULL, "wps_aplockdown_forceon", NULL);
	print_conf(NULL, NULL, "wps_lock_start_cnt", NULL);
	print_conf(NULL, NULL, "wps_lock_forever_cnt", NULL);
	print_conf(NULL, NULL, "wps_device_pin", NULL);
	print_conf(NULL, NULL, "wps_mixedmode", NULL);
	print_conf(NULL, NULL, "wps_random_ssid_prefix", NULL);
	print_conf(NULL, NULL, "wps_randomssid", NULL);
	print_conf(NULL, NULL, "wps_randomkey", NULL);
	print_conf(NULL, NULL, "wps_pbc_prefer_intf", NULL);
	print_conf(NULL, NULL, "wps_pbc_prefer_intf_count", NULL);
	print_conf(NULL, NULL, "wps_device_name", NULL);
	print_conf(NULL, NULL, "wps_mfstring", NULL);
	print_conf(NULL, NULL, "wps_modelname", NULL);
	print_conf(NULL, NULL, "wps_modelnum", NULL);
	print_conf(NULL, NULL, "boardnum", NULL);
	print_conf(NULL, NULL, "wps_config_method", NULL);
	print_conf(NULL, NULL, "wps_wer_mode", NULL);
	print_conf(NULL, NULL, "wps_wer_override", NULL);
	print_conf(NULL, NULL, "wps_proc_status", NULL);
	print_conf(NULL, NULL, "wps_oob_configured", NULL);

#ifdef WPS_UPNP_DEVICE
	print_conf(NULL, NULL, "wfa_port", NULL);
	print_conf(NULL, NULL, "wfa_adv_time", NULL);
#endif // endif
	/* Support WSC 2.0 or not */
	print_conf(NULL, NULL, "wps_version2", NULL);
	if (print_conf(NULL, NULL, "wps_version2_num", NULL) == NULL) {
		snprintf(tmp, sizeof(tmp), "0x%x", WPS_VERSION2);
		print_conf(NULL, NULL, "wps_version2_num", tmp);
	}

	/* For testing purpose */
#ifdef WFA_WPS_20_TESTBED
	/* WPS IE fragment threshold */
	print_conf(NULL, NULL, "wps_ie_frag", NULL);
	/* WPS EAP fragment threshold */
	print_conf(NULL, NULL, "wps_eap_frag", NULL);
	/* New attribute */
	print_conf(NULL, NULL, "wps_nattr", NULL);
	/* Disable Null-Termination */
	print_conf(NULL, NULL, "wps_zpadding", NULL);
	/* Multiple Credential Attribute */
	print_conf(NULL, NULL, "wps_mca", NULL);
	/*
	 * Update partial embedded WPS IE in beacon /probe req /probe resp /
	 * associate req and associate resp
	 */
	print_conf(NULL, NULL, "wps_beacon", NULL);
	print_conf(NULL, NULL, "wps_prbreq", NULL);
	print_conf(NULL, NULL, "wps_prbrsp", NULL);
	print_conf(NULL, NULL, "wps_assocreq", NULL);
	print_conf(NULL, NULL, "wps_assocrsp", NULL);
	/* Zero length in mandatory string attributes such as serial number */
	print_conf(NULL, NULL, "wps_zlength", NULL);
#endif /* WFA_WPS_20_TESTBED */

#ifdef WFA_WPS_20_NFC_TESTBED
	/* Fake public key hash */
	print_conf(NULL, NULL, "wps_fake_pkh", NULL);
	print_conf(NULL, NULL, "wps_nfc_fake_p2p", NULL);
#endif /* WFA_WPS_20_NFC_TESTBED */

	/* Set device UUID */
	print_conf(NULL, NULL, "wps_uuid", (char *)create_device_uuid_string(uuid_string));

	/* Set WPS M1 Pairing */
	print_conf(NULL, NULL, "wps_pairing_auth", NULL);
	print_conf(NULL, NULL, "wps_sta_device_name", NULL);

	/* set delay deauth time after EAP-FAIL */
	print_conf(NULL, NULL, "wps_delay_deauth_ms", NULL);

	print_conf(NULL, NULL, "wps_custom_ifnames", NULL);
	ess_id = 0;
	/* (LAN) for UPnP and push button */
	for (i = 0; i < 256; i ++) {
		/* Taking care of LAN interface names */
		if (i == 0) {
			strcpy(name, "lan_ifnames");
			strcpy(lan_name, "lan");
		}
		else {
			sprintf(name, "lan%d_ifnames", i);
			sprintf(lan_name, "lan%d", i);
		}

		ifnames = nvram_get(name);
		if (!ifnames)
			continue;

#ifdef WPS_UPNP_DEVICE
		if (i == 0)
			strcpy(lan_ifname, "lan_ifname");
		else
			sprintf(lan_ifname, "lan%d_ifname", i);

		ifname = nvram_get(lan_ifname);
		if (!ifname)
			continue;
#endif /* WPS_UPNP_DEVICE */

		/* Search for wl_name in ess */
		ap_index = -1;
		sprintf(ess_name, "ess%d", ess_id);

		memset(wlnames, 0, sizeof(wlnames));
		memset(stanames, 0, sizeof(stanames));
		band = 0;

		foreach(name, ifnames, next) {
			if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
				continue;
			if (wl_probe(os_name) ||
				wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
				continue;

			/* Convert eth name to wl name */
			if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
				continue;

			/* Ignore radio or bss is disabled */
			snprintf(tmp, sizeof(tmp), "wl%d_radio", unit);
			wl_radio = nvram_safe_get(tmp);
			if (strcmp(wl_radio, "1") != 0)
				continue;

			snprintf(tmp, sizeof(tmp), "%s_bss_enabled", wl_name);
			wl_bss_enabled = nvram_safe_get(tmp);
			if (strcmp(wl_bss_enabled, "1") != 0) {
				/* In URE WRE mode, the STA interface always join WPS */
				snprintf(tmp, sizeof(tmp), "%s_ure_mode", wl_name);
				value = nvram_safe_get(tmp);
				if (strcmp(value, "wre") != 0)
					continue;
			}
			print_conf(os_name, wl_name, "ure_mode", NULL);

			/* Get configured wireless address */
			snprintf(prefix, sizeof(prefix), "%s_", wl_name);

			value = nvram_get(strcat_r(prefix, "wps_mode", tmp));
			if (!value || strcmp(value, "disabled") == 0)
				continue;

			/* If the radio is disabled, skip it */
			if (!print_conf(os_name, wl_name, "hwaddr", NULL))
				continue;

			/* Print wl specific variables */
			wl_mode = nvram_get(strcat_r(prefix, "mode", tmp));
			print_conf(os_name, wl_name, "mode", wl_mode);

			print_conf(os_name, wl_name, "wep", NULL);
			print_conf(os_name, wl_name, "auth", NULL);
			print_conf(os_name, wl_name, "ssid", NULL);
			print_conf(os_name, wl_name, "akm", NULL);
			print_conf(os_name, wl_name, "crypto", NULL);
			print_conf(os_name, wl_name, "wpa_psk", NULL);
#if defined(MULTIAP)
			print_conf(os_name, wl_name, "map", NULL);
#endif	/* MULTIAP */
			value = print_conf(os_name, wl_name, "key", NULL);
			if (value) {
				sprintf(key, "key%s", value);
				print_conf(os_name, wl_name, key, NULL);
			}

			/* Set only for AP mode */
			if (strcmp(wl_mode, "ap") == 0) {
				int wlband;

				/* Get per lan RF bands */
				wl_ioctl(os_name, WLC_GET_BAND, &bandtype, sizeof(bandtype));
				if (bandtype == WLC_BAND_AUTO) {
					int list[3];
					int j;

					wl_ioctl(os_name, WLC_GET_BANDLIST, list, sizeof(list));
					if (list[0] > 2)
						list[0] = 2;

					bandtype = 0;
					for (j = 0; j < list[0]; j++) {
						switch (list[j]) {
						case WLC_BAND_5G:
						case WLC_BAND_2G:
							bandtype |= list[j];
							break;
						default:
							break;
						}
					}
				}

				/* Save band per wl interface as WPS RFBAND type */
				switch (bandtype) {
				case WLC_BAND_ALL:
					wlband = WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_5G:
					wlband = WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_2G:
				default:
					wlband = WPS_RFBAND_24GHZ;
					break;
				}
				sprintf(tmp, "%d", wlband);
				print_conf(os_name, wl_name, "band", tmp);

				/* Aggregage band type */
				band |= bandtype;

				TUTRACE((TUTRACE_INFO, "%s(%s) bandtype = %d\n",
					os_name, wl_name, bandtype));

#ifdef BCMWPSAPSTA
				/* Save apsta AP ifname */
				if (ap_idx < WPS_MAX_PBC_APSTA) {
					strncpy(wps_pbc_ap_ifnames[ap_idx].ifname,
						os_name, IFNAMSIZ);
					ap_idx++;
				}
				wps_pbc_ap_index = i;
#endif // endif

#ifdef __CONFIG_WFI__
				/* Get Wifi-Invite configuration for this (virtual) interface */
				print_conf(os_name, wl_name, "wfi_enable", NULL);
				print_conf(os_name, wl_name, "wfi_pinmode", NULL);
#endif	/* __CONFIG_WFI__ */
				/* We got an AP interface, need UPnP attached */
				ap_index = i;

				/* Save wlnames of this lan */
				if (strlen(wlnames))
					strcat(wlnames, " ");

				strcat(wlnames, os_name);
			}
			/* Set only for STA mode */
			else {
#ifdef BCMWPSAPSTA
				/* Save apsta STA ifname */
				if (sta_idx < WPS_MAX_PBC_APSTA) {
					strncpy(wps_pbc_sta_ifnames[sta_idx].ifname,
						os_name, IFNAMSIZ);
					sta_idx++;
				}
				wps_init_escan_bss_results(os_name, unit);
#endif // endif
				/* Save stanames of this lan */
				if (strlen(stanames))
					strcat(stanames, " ");

				strcat(stanames, os_name);
			}
		}

		/* Setup ESS for AP */
		if (strlen(wlnames)) {
			print_conf(ess_name, NULL, "wlnames", wlnames);
			print_conf(ess_name, lan_name, "wps_reg", NULL);
			print_conf(ess_name, lan_name, "wps_oob", NULL);

			/* LED id */
			sprintf(tmp, "%d", ess_id);
			print_conf(ess_name, NULL, "led_id", tmp);

			if (band) {
				/* ALL RF bands supported */
				switch (band) {
				case WLC_BAND_ALL:
					bandtype = WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_5G:
					bandtype = WPS_RFBAND_50GHZ;
					break;
				case WLC_BAND_2G:
				default:
					bandtype = WPS_RFBAND_24GHZ;
					break;
				}
				sprintf(tmp, "%d", bandtype);
				print_conf(ess_name, NULL, "band", tmp);
			}

			if (ap_index != -1) {
				sprintf(tmp, "%d", ap_index);
				print_conf(ess_name, NULL, "ap_index", tmp);
#ifdef WPS_UPNP_DEVICE
				print_conf(ess_name, NULL, "ifname", ifname);
#endif // endif
			}

			/* WSC 2.0, for built-in Registrar add AuthorizedMACs */
			print_conf(ess_name, lan_name, "ipaddr", NULL);

			ess_id++;
		}

		/* Setup ESS for URE STA */
		foreach(name, stanames, next) {
			/* Convert eth name to wl name */
			if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
				continue;

			sprintf(ess_name, "ess%d", ess_id);
			print_conf(ess_name, NULL, "wlnames", name);
			print_conf(ess_name, wl_name, "wps_oob", NULL);

			/* LED id */
			sprintf(tmp, "%d", ess_id);
			print_conf(ess_name, NULL, "led_id", tmp);

			/* WSC 2.0, for built-in Registrar add AuthorizedMACs */
			print_conf(ess_name, lan_name, "ipaddr", NULL);

			ess_id++;
		}
	}

	/* (WAN) for push button */
	if (!nvram_match("router_disable", "1")) {
		for (i = 0; i < 256; i ++) {
			/* Taking care of WAN interface names */
			if (i == 0)
				strcpy(name, "wan_ifnames");
			else
				sprintf(name, "wan%d_ifnames", i);

			ifnames = nvram_get(name);
			if (!ifnames)
				continue;

			/* Search for wl_name in it */
			memset(wlnames, 0, sizeof(wlnames));
			foreach(name, ifnames, next) {
				if (nvifname_to_osifname(name, os_name, sizeof(os_name)) < 0)
					continue;
				if (wl_probe(os_name) ||
					wl_ioctl(os_name, WLC_GET_INSTANCE, &unit, sizeof(unit)))
					continue;

				/* Convert eth name to wl name */
				if (osifname_to_nvifname(name, wl_name, sizeof(wl_name)) != 0)
					continue;

				/* Ignore radio or bss is disabled */
				snprintf(tmp, sizeof(tmp), "wl%d_radio", unit);
				wl_radio = nvram_safe_get(tmp);

				snprintf(tmp, sizeof(tmp), "%s_bss_enabled", wl_name);
				wl_bss_enabled = nvram_safe_get(tmp);
				if (strcmp(wl_radio, "1") != 0 || strcmp(wl_bss_enabled, "1") != 0)
					continue;

				/* Get configured wireless address */
				snprintf(prefix, sizeof(prefix), "%s_", wl_name);

				value = nvram_get(strcat_r(prefix, "wps_mode", tmp));
				if (!value || strcmp(value, "disabled") == 0)
					continue;

				if (!print_conf(os_name, wl_name, "hwaddr", NULL))
					continue;

				/* Print wl specific variables */
				print_conf(os_name, wl_name, "mode", NULL);

				print_conf(os_name, wl_name, "wep", NULL);
				print_conf(os_name, wl_name, "auth", NULL);
				print_conf(os_name, wl_name, "ssid", NULL);
				print_conf(os_name, wl_name, "akm", NULL);
				print_conf(os_name, wl_name, "crypto", NULL);
				print_conf(os_name, wl_name, "wpa_psk", NULL);

				value = print_conf(os_name, wl_name, "key", NULL);
				if (value) {
					sprintf(key, "key%s", value);
					print_conf(os_name, wl_name, key, NULL);
				}
#ifdef BCMWPSAPSTA
				/* Save apsta STA ifname */
				if (sta_idx < WPS_MAX_PBC_APSTA)
					strncpy(wps_pbc_sta_ifnames[sta_idx].ifname,
						os_name, IFNAMSIZ);
				sta_idx++;
#endif // endif

				/* Set ESS instance prefix */
				sprintf(ess_name, "ess%d", ess_id);
				print_conf(ess_name, NULL, "wlnames", os_name);

				/* LED id */
				sprintf(tmp, "%d", ess_id);
				print_conf(ess_name, NULL, "led_id", tmp);
				print_conf(ess_name, wl_name, "wps_oob", NULL);

				ess_id++;
			}
		}
	}

#ifdef BCMWPSAPSTA
	for (i = 0; i < WPS_MAX_PBC_APSTA; i++) {
		TUTRACE((TUTRACE_INFO, " ap%d ifname = %s\n", i, wps_pbc_ap_ifnames[i].ifname));
		TUTRACE((TUTRACE_INFO, " sta%d ifname = %s\n", i, wps_pbc_sta_ifnames[i].ifname));
	}

	if (is_wps_pbc_apsta_enabled(ess_id, ap_idx, sta_idx)) {
		print_conf(NULL, NULL, "wps_pbc_apsta", "enabled");
		for (i = 0; i < WPS_MAX_PBC_APSTA; i++) {
			if (strlen(wps_pbc_ap_ifnames[i].ifname))
				print_conf(NULL, NULL, wps_pbc_ap_ifnames[i].name,
						wps_pbc_ap_ifnames[i].ifname);
			if (strlen(wps_pbc_sta_ifnames[i].ifname))
				print_conf(NULL, NULL, wps_pbc_sta_ifnames[i].name,
						wps_pbc_sta_ifnames[i].ifname);
		}
		sprintf(tmp, "%d", wps_pbc_ap_index);
		print_conf(NULL, NULL, "wps_pbc_ap_index", tmp);
	}
#endif /* BCMWPSAPSTA */

	/* Save maximum instance */
	sprintf(tmp, "%d", ess_id);
	print_conf(NULL, NULL, "wps_ess_num", tmp);

	/* Seperate wps_conf_buf into wps_conf_list[] */
	wps_conf_num = 0;
	for (item = wps_conf_buf, p = item; item && item[0]; item = next, p = 0) {
		/* Get next token */
		strtok_r(p, "\n", &next);
		wps_conf_list[wps_conf_num++] = item;
	}
}

#if defined(MULTIAP)
/* Fill the prim interface prefix */
static bool
wps_map_get_prefix(char *ifname, char *out_prefix, int size)
{
	int unit = 0;

	if (!out_prefix)
		return FALSE;

	if (wl_probe(ifname) ||
		wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit))) {
		return FALSE;
	}

	snprintf(out_prefix, size, "wl%d_", unit);

	return TRUE;
}

/* Check whether interface is virtual */
static bool
wps_map_is_vifs(char *ifname)
{
	int unit = -1, subunit = -1;
	bool ret = FALSE;

	if (get_ifname_unit(ifname, &unit, &subunit) < 0) {
		return FALSE;
	}

	if (unit < 0) {
		ret = FALSE;
	}

	if (subunit >= 0) {
		ret = TRUE;
	}

	return ret;

}

/* Get ifname corrosponding to matching band */
static void
wps_map_filter_ifnames(char *ifnames, int band, char *out_ifname, int size)
{
	char ifname[32] = {0}, *next;
	uint16 chanspec;
	int curr_band = 0;

	foreach(ifname, ifnames, next) {
		if (wl_iovar_get(ifname, "chanspec", &chanspec, sizeof(chanspec))) {
			continue;
		}
		if (!wf_chspec_valid(chanspec) || wps_map_is_vifs(ifname)) {
			continue;
		}

		if (CHSPEC_IS2G(chanspec)) {
			curr_band = 1;	/* 2G */
		} else if (CHSPEC_IS5G(chanspec)) {
			uint8 channel = CHSPEC_CHANNEL(chanspec);

			if (channel < 100) {
				curr_band = 2;	/* 5GL */
			} else {
				curr_band = 3;	/* 5GH */
			}
		}

		if (curr_band == band) {
			strncpy(out_ifname, ifname, size - 1);
			out_ifname[size -1] = '\0';
			TUTRACE((TUTRACE_INFO, "Found ifname %s for band %d\n", ifname, band));
			break;
		}
	}
}

/* Get backhaul interface name if backhaul band nvram is present */
int
wps_map_get_sta_backhaul_ifname(char *ifname, int size)
{
	char *backhaul_ifname = nvram_safe_get("wbd_backhaul_band");
	char *ifnames = nvram_safe_get("lan_ifnames");
	int ret = 1;

	if (backhaul_ifname[0] == '\0' || !ifname || !size) {
		return 0;
	}

	wps_map_filter_ifnames(ifnames, atoi(backhaul_ifname), ifname, size);

	if (ifname[0] == '\0') {
		ret = 0;
	}

	return ret;
}

/* Update the settings of interfaces except skip_ifr */
void
wps_map_update_settings(char *skip_ifr)
{
	char ifname[32] = {0}, *next;
	char *ifnames = nvram_safe_get("lan_ifnames");

	foreach(ifname, ifnames, next) {
		char prefix[16] = {0}, tmp[64] = {0};

		if (skip_ifr && !strcmp(ifname, skip_ifr)) {
			continue;
		}
		if (!wps_map_is_vifs(ifname) &&
			wps_map_get_prefix(ifname, prefix, sizeof(prefix))) {
			nvram_unset(strcat_r(prefix, "map", tmp));
			nvram_set(strcat_r(prefix, "mode", tmp), "ap");
		}
	}

	nvram_unset("tmp_ifnames");
	nvram_set("map_onboarded", "1");
	nvram_commit();
}

/* Get the multiap ifnames */
char*
wps_map_get_ifnames()
{
	char *ifnames = nvram_safe_get("tmp_ifnames");

	if (ifnames[0] == '\0') {
		ifnames = nvram_safe_get("wps_custom_ifnames");
		nvram_set("tmp_ifnames", ifnames);
	}

	return ifnames;
}

/* Update the multiap ifnames */
void
wps_map_update_ifnames(char *skip_ifr)
{
	char buf[128] = {0}, len = 0;
	char ifname[32] = {0}, *next;
	char *ifnames = nvram_safe_get("tmp_ifnames");

	foreach(ifname, ifnames, next) {
		if (skip_ifr && !strcmp(ifname, skip_ifr)) {
			continue;
		}
		if ((len + strlen(ifname) + 1) < sizeof(buf)) {
			len += snprintf(buf + len, sizeof(buf) - len, "%s%c", ifname, ' ');
		}
	}

	nvram_set("tmp_ifnames", buf);

	TUTRACE((TUTRACE_INFO, "Updated tmp ifnames %s \n", buf));
}

/* Checks whether its multiap onboarding or not */
bool
wps_map_is_onboarding(char *ifname)
{
	char tmp[32] = {0};
	bool ret = FALSE;

	if (ifname == NULL) {
		return FALSE;
	}

	snprintf(tmp, sizeof(tmp), "%s_map", ifname);
	if (atoi(wps_safe_get_conf(tmp)) == 4 &&
		atoi(nvram_safe_get("map_onboarded")) == 0) {
		ret = TRUE;
	}

	return ret;
}

/* Returns total count of wireless interfaces in sta mode present in wps_custom_ifnames nvram */
int
wps_map_sta_intf_count()
{
	char *ifnames = nvram_safe_get("wps_custom_ifnames"), *mode = NULL, tmp[64] = {0};
	char ifname[IFNAMSIZ] = {0}, *next, os_ifname[IFNAMSIZ] = {0};
	int total = 0, unit = 0;

	BCM_REFERENCE(unit);
	foreach(ifname, ifnames, next) {
		if (nvifname_to_osifname(ifname, os_ifname, sizeof(os_ifname)) < 0) {
			continue;
		}

		if (wl_probe(os_ifname) ||
			wl_ioctl(os_ifname, WLC_GET_INSTANCE, &unit, sizeof(unit))) {
			continue;
		}
		snprintf(tmp, sizeof(tmp), "%s_mode", ifname);
		mode = wps_safe_get_conf(tmp);
		if (strcmp(mode, "sta")) {
			continue;
		}

		total++;
	}

	return total;
}

/* calculates the multiap timeout as follow :
 * Fetch multiap timeout value from nvram.
 * If nvram contains Non-zero value than same will be used,
 * otherwise wps timeout value (120 secs) will be devided by the no of
 * wireless sta interfaces present in wps_custom_ifnames nvram.
 */
int
wps_map_get_timeout_val()
{
	int timeout = WPS_MAX_TIMEOUT, total = 0;
	int map_timeout = atoi(nvram_safe_get("wps_map_timeout"));

	if (map_timeout > 0) {
		timeout = map_timeout;
	} else {
		total = wps_map_sta_intf_count();
		if (total > 0) {
			timeout = timeout/total;
		}
	}

	TUTRACE((TUTRACE_INFO, "Multiap timeout val = %d\n", timeout));

	return timeout;
}

/* Get candidate backhaul interface names */
void
wps_map_get_candidate_backhaul_ifnames(char *out_ifname, int size)
{
	char ifname[IFNAMSIZ] = {0}, tmp[64] = {0};
	char *ifnames = nvram_safe_get("lan_ifnames");
	char *next, *mode, *map;
	char prefix[] = "wlxxxxxxxxxxxxxx_";
	uint16 chanspec;
	int len = 0;
#ifdef BCMWPSAPSTA
	int idx = 0;
#endif // endif

	if (!out_ifname || !size) {
		return;
	}

	foreach(ifname, ifnames, next) {
		if (wl_iovar_get(ifname, "chanspec", &chanspec, sizeof(chanspec))) {
			continue;
		}

		if (!wf_chspec_valid(chanspec) || wps_map_is_vifs(ifname)) {
			continue;
		}

		if (osifname_to_nvifname(ifname, prefix, sizeof(prefix)) != 0) {
			continue;
		}

		snprintf(tmp, sizeof(tmp), "%s_%s", prefix, "mode");
		mode = nvram_safe_get(tmp);
		snprintf(tmp, sizeof(tmp), "%s_%s", prefix, "map");
		map = nvram_safe_get(tmp);
		if (strcmp(mode, "sta") || (atoi(map) != 4)) {
			continue;
		}

		if (size > (len + strlen(ifname) + 1)) {
			len += snprintf(out_ifname + len, size - len, "%s ", ifname);
#ifdef BCMWPSAPSTA
			wps_init_escan_bss_results(ifname, idx);
			idx++;
#endif // endif
		}
	}
}

/* Issue the wl down followed by up.  */
void
wps_map_do_wl_up_down(char *ifname)
{
	if (!ifname) {
		return;
	}

	wl_ioctl(ifname, WLC_DOWN, NULL, 0);
	wl_ioctl(ifname, WLC_UP, NULL, 0);
}

/* Get AP backhaul interface name */
void
wps_map_get_ap_backhaul_ifname(char *out_ifname, int size)
{
	char tmp[64] = {0}, *mode = NULL;
	char *ifnames = nvram_safe_get("lan_ifnames"), *next, ifname[IFNAMSIZ] = {0};
	char *ptr;

	if (!out_ifname || !size) {
		return;
	}

	ptr = nvram_safe_get("wps_backhaul_ifname");
	if (ptr[0] != '\0') {
		wps_strncpy(out_ifname, ptr, size);
	} else {
		foreach(ifname, ifnames, next) {
			uint16 map = 0;

			snprintf(tmp, sizeof(tmp), "%s_map", ifname);
			ptr = wps_safe_get_conf(tmp);
			if (ptr[0] != '\0') {
				map = (uint16)strtoul(ptr, NULL, 0);
			}

			snprintf(tmp, sizeof(tmp), "%s_mode", ifname);
			mode = wps_safe_get_conf(tmp);
			TUTRACE((TUTRACE_INFO, "ifname[%s] map[%d] mode[%s] \n",
				ifname, map, mode));
			if (!strcmp(mode, "ap") && (map & WPS_NVVAL_MAP_BH_BSS)) {
				wps_strncpy(out_ifname, ifname, size);
				/* Dedicated bh interface found */
				if (map == WPS_NVVAL_MAP_BH_BSS) {
					break;
				}
			}
		}
	}
	TUTRACE((TUTRACE_INFO, "Selected backhaul ifname [%s] \n", out_ifname));
}

/* Check whether the interface is operable or not in given chanspec */
bool
wps_map_is_ifr_opr_on_chanspec(char *ifname, uint16 chanspec)
{
	wl_uint32_list_t *list = NULL;
	uint32 buf[WL_NUMCHANNELS + 1] = {0};
	int idx = 0;
	bool ret = FALSE;
	uint8 channel;

	if (!ifname || wf_chspec_malformed(chanspec)) {
		TUTRACE((TUTRACE_ERR, "Invalid ifname or Malformed chanspec\n"));
		goto end;
	}

	list = (wl_uint32_list_t *)buf;
	list->count = htod32(WL_NUMCHANNELS);
	if (wl_ioctl(ifname, WLC_GET_VALID_CHANNELS, buf, sizeof(buf)) < 0) {
		TUTRACE((TUTRACE_INFO, "Channels iovar is failed \n"));
		goto end;
	}

	channel = wf_chspec_ctlchan(chanspec);
	list->count = dtoh32(list->count);

	for (idx = 0; idx < list->count; idx++) {
		if (channel == list->element[idx]) {
			ret = TRUE;
			break;
		}
	}

#if defined(_TUDEBUGTRACE)
	TUTRACE((TUTRACE_INFO, "Channel of Backhaul AP %d \n", channel));
	TUTRACE((TUTRACE_INFO, "Output of channels iovar for %s\n", ifname));
	for (idx = 0; idx < list->count; idx++) {
		printf(" %d ", list->element[idx]);
	}
	TUTRACE((TUTRACE_INFO, "\n*** END **** \n"));
#endif	/* _TUDEBUGTRACE */

end:
	return ret;
}
#endif	/* MULTIAP */

void
wps_custom_init()
{
	wps_aplockdown_custom.init = NULL;
	wps_aplockdown_custom.wps_pinfail_added = NULL;
	wpsapp_update_custom_cred_callback = NULL;
	return;
}

void
wps_custom_deinit()
{
	wps_aplockdown_custom.init = NULL;
	wps_aplockdown_custom.wps_pinfail_added = NULL;
	wpsapp_update_custom_cred_callback = NULL;
	return;
}
/*
 * Name        : main
 * Description : Main entry point for the WPS monitor
 * Arguments   : int argc, char *argv[] - command line parameters
 * Return type : int
 */
int
main(int argc, char* argv[])
{
	FILE *pidfile;
	int flag;
	char *var;
	unsigned int wps_msglevel;

	/* Show usage */
	if (argc > 1) {
		fprintf(stderr, "Usage: wps_monitor\n");
		return -1;
	}

	/*
	 * Check whether this process is running
	 */
	if ((pidfile = fopen(WPSM_PID_FILE_PATH, "r"))) {
		fprintf(stderr, "%s: wps_monitor has been started\n", __FILE__);

		fclose(pidfile);
		return -1;
	}

	/* Cread pid file */
	if ((pidfile = fopen(WPSM_PID_FILE_PATH, "w"))) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	}
	else {
		perror("pidfile");
		exit(errno);
	}

	/* establish a handler to handle SIGTERM. */
	signal(SIGTERM, wps_stophandler);
	signal(SIGUSR1, wps_restarthandler);

	RAND_linux_init();

	/* Default settings validation */
	wps_osl_validate_default();

	wps_osl_build_conf();

	/* Set denug message level */
	var = nvram_get("wps_msglevel");
	if (var) {
		wps_msglevel = strtoul(var, NULL, 16);
		fprintf(stderr, "WPS: set msglevel to 0x%x\n", wps_msglevel);
		wps_tutrace_set_msglevel(wps_msglevel);
	}

	/* Enter main loop */
	flag = wps_mainloop(wps_conf_num, wps_conf_list);

#if defined(MULTIAP)
	/* Need to apply the settings to the sta interface before restart */
	if (wps_map_get_settings() && (flag & WPSM_WKSP_FLAG_SUCCESS_RESTART)) {
		wps_map_do_configure();
		TUTRACE((TUTRACE_INFO, "******* Multiap onboarding done ******\n"));
	}
#endif	/* MULTIAP */
	/* XXX,
	 * We need to back here to find what is that.
	 */
	if (flag & WPSM_WKSP_FLAG_SET_RESTART)
		nvram_set("wps_restart", "1");

	/* need restart wireless */
	if (flag & WPSM_WKSP_FLAG_RESTART_WL)
		wps_osl_restart_wl();

	/* Destroy pid file */
	unlink(WPSM_PID_FILE_PATH);

	return 0;
}
#ifdef BCA_CPEROUTER
void kill_wps_pid_file(void)
{
	/* Destroy pid file */
	unlink(WPSM_PID_FILE_PATH);
}
#endif /* BCA_CPEROUTER */
