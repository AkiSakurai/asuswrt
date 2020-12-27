/*
 * Wireless Network Adapter Configuration Utility
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlconf.c 777961 2019-08-16 12:32:59Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmparams.h>
#include <bcmdevs.h>
#include <shutils.h>
#include <wlutils.h>
#include <wlioctl.h>
#include <802.1d.h>
#include <bcmconfig.h>
#include <bcmwifi_channels.h>
#include <netconf.h>
#include <nvparse.h>
#include <arpa/inet.h>
#ifdef BCA_CPEROUTER
#include <wlcsm_linux.h>
#include <wlcsm_lib_api.h>
#endif // endif

#if defined(linux)
#include <sys/utsname.h>
#endif // endif
#include <wlif_utils.h>

/* phy types */
#define	PHY_TYPE_A		0
#define	PHY_TYPE_B		1
#define	PHY_TYPE_G		2
#define	PHY_TYPE_N		4
#define	PHY_TYPE_LP		5
#define PHY_TYPE_SSN		6
#define	PHY_TYPE_HT		7
#define	PHY_TYPE_LCN		8
#define PHY_TYPE_AC		11
#define	PHY_TYPE_NULL		0xf

/* how many times to attempt to bring up a virtual i/f when
 * we are in APSTA mode and IOVAR set of "bss" "up" returns busy
 */
#define MAX_BSS_UP_RETRIES 5

/* notify the average dma xfer rate (in kbps) to the driver */
#define AVG_DMA_XFER_RATE 120000

/* parts of an idcode: */
#define	IDCODE_MFG_MASK		0x00000fff
#define	IDCODE_MFG_SHIFT	0
#define	IDCODE_ID_MASK		0x0ffff000
#define	IDCODE_ID_SHIFT		12
#define	IDCODE_REV_MASK		0xf0000000
#define	IDCODE_REV_SHIFT	28

/*
 * Debugging Macros
 */
#ifdef BCMDBG
#define WLCONF_DBG(fmt, arg...)	printf("%s: "fmt, __FUNCTION__ , ## arg)
#define WL_IOCTL(ifname, cmd, buf, len)					\
	if ((ret = wl_ioctl(ifname, cmd, buf, len)))			\
		fprintf(stderr, "%s:%d:(%s): %s failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, #cmd, ret);
#define WL_SETINT(ifname, cmd, val)								\
	if ((ret = wlconf_setint(ifname, cmd, val)))						\
		fprintf(stderr, "%s:%d:(%s): setting %s to %d (0x%x) failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, #cmd, (int)val, (unsigned int)val, ret);
#define WL_GETINT(ifname, cmd, pval)								\
	if ((ret = wlconf_getint(ifname, cmd, pval)))						\
		fprintf(stderr, "%s:%d:(%s): getting %s failed, err = %d\n",			\
		        __FUNCTION__, __LINE__, ifname, #cmd, ret);
#define WL_IOVAR_SET(ifname, iovar, param, paramlen)					\
	if ((ret = wl_iovar_set(ifname, iovar, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_IOVAR_GET(ifname, iovar, param, paramlen)					\
	if ((ret = wl_iovar_get(ifname, iovar, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): getting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_IOVAR_SETINT(ifname, iovar, val)							\
	if ((ret = wl_iovar_setint(ifname, iovar, val)))					\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s\" to 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, (unsigned int)val, ret);
#define WL_IOVAR_GETINT(ifname, iovar, val)							\
	if ((ret = wl_iovar_getint(ifname, iovar, val)))					\
		fprintf(stderr, "%s:%d:(%s): getting iovar \"%s\" failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, ret);
#define WL_BSSIOVAR_SETBUF(ifname, iovar, bssidx, param, paramlen, buf, buflen)			\
	if ((ret = wl_bssiovar_setbuf(ifname, iovar, bssidx, param, paramlen, buf, buflen)))	\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_SET(ifname, iovar, bssidx, param, paramlen)					\
	if ((ret = wl_bssiovar_set(ifname, iovar, bssidx, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_GET(ifname, iovar, bssidx, param, paramlen)					\
	if ((ret = wl_bssiovar_get(ifname, iovar, bssidx, param, paramlen)))			\
		fprintf(stderr, "%s:%d:(%s): getting bsscfg #%d iovar \"%s\" failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, ret);
#define WL_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)						\
	if ((ret = wl_bssiovar_setint(ifname, iovar, bssidx, val)))				\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" " \
				"to val 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, (unsigned int)val, ret);
#define WL_HEIOVAR_SETINT(ifname, iovar, subcmd, val)						\
	if ((ret = wl_heiovar_setint(ifname, iovar, subcmd, val)))				\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s %s\" to 0x%x failed, err = %d\n", \
		        __FUNCTION__, __LINE__, ifname, iovar, subcmd, val, ret);
#define WL_IOVAR_XTLV_SETINT(ifname, iovar, val, version, cmd_id, xtlv_id)			\
	if ((ret = wl_iovar_xtlv_setint(ifname, iovar, val, version, cmd_id, xtlv_id)))		\
		fprintf(stderr, "%s:%d:(%s): setting iovar \"%s\" to 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, iovar, (unsigned int)val, ret);
#ifdef __CONFIG_DHDAP__
#define DHD_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)						\
	if ((ret = dhd_bssiovar_setint(ifname, iovar, bssidx, val)))				\
		fprintf(stderr, "%s:%d:(%s): setting bsscfg #%d iovar \"%s\" " \
				"to val 0x%x failed, err = %d\n",	\
		        __FUNCTION__, __LINE__, ifname, bssidx, iovar, (unsigned int)val, ret);
#endif // endif
#else
#define WLCONF_DBG(fmt, arg...)
#define WL_IOCTL(name, cmd, buf, len)			(ret = wl_ioctl(name, cmd, buf, len))
#define WL_SETINT(name, cmd, val)			(ret = wlconf_setint(name, cmd, val))
#define WL_GETINT(name, cmd, pval)			(ret = wlconf_getint(name, cmd, pval))
#define WL_IOVAR_SET(ifname, iovar, param, paramlen)	(ret = wl_iovar_set(ifname, iovar, \
							param, paramlen))
#define WL_IOVAR_GET(ifname, iovar, param, paramlen)	(ret = wl_iovar_get(ifname, iovar, \
							param, paramlen))
#define WL_IOVAR_SETINT(ifname, iovar, val)		(ret = wl_iovar_setint(ifname, iovar, val))
#define WL_IOVAR_GETINT(ifname, iovar, val)		(ret = wl_iovar_getint(ifname, iovar, val))
#define WL_BSSIOVAR_SETBUF(ifname, iovar, bssidx, param, paramlen, buf, buflen) \
		(ret = wl_bssiovar_setbuf(ifname, iovar, bssidx, param, paramlen, buf, buflen))
#define WL_BSSIOVAR_SET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_set(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_GET(ifname, iovar, bssidx, param, paramlen) \
		(ret = wl_bssiovar_get(ifname, iovar, bssidx, param, paramlen))
#define WL_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)	(ret = wl_bssiovar_setint(ifname, iovar, \
			bssidx, val))
#define WL_HEIOVAR_SETINT(ifname, iovar, subcmd, val) \
		(ret = wl_heiovar_setint(ifname, iovar, subcmd, val))
#ifdef __CONFIG_DHDAP__
#define DHD_BSSIOVAR_SETINT(ifname, iovar, bssidx, val)	(ret = dhd_bssiovar_setint(ifname, iovar, \
			bssidx, val))
#endif // endif
#define WL_IOVAR_XTLV_SETINT(ifname, iovar, val, version, cmd_id, xtlv_id)	\
	(ret = wl_iovar_xtlv_setint(ifname, iovar, val, version, cmd_id, xtlv_id))
#endif /* BCMDBG */

#define CHECK_PSK(mode) ((mode) & (WPA_AUTH_PSK | WPA2_AUTH_PSK | WPA2_AUTH_FT))

#ifdef MFP
#define WL_MFP_DISABLE		0X00
#define WL_MFP_CAPABLE		0X01
#define WL_MFP_REQUIRED		0X02
#endif /* ifdef MFP */

#define WL_CHSPEC_DEF_2G	0x1001		/* Default 2G chanspec from Channel 1 */
#define WL_CHSPEC_DEF_5G_L	0xD024		/* Default 5GL chanspec from Channel 36 */
#define WL_CHSPEC_DEF_5G_H	0xD095		/* Default 5GH chanspec from Channel 149 */
#define WL_RADIO_CHAN_5GL	0x0018
#define WL_RADIO_CHAN_5GH	0x0007

#define IS_FIRST_WET_BSSIDX	0
#define IS_URE_GUEST_STA(idx, name)	(((idx) == IS_FIRST_WET_BSSIDX) ? 0 : \
		!strcmp(nvram_safe_get(strcat_r(name, "_mode", tmp)), "wet"))

#if defined(MULTIAP) || defined(BCM_WBD)
#define NVRAM_MAP_MODE	"multiap_mode"
#define NVRAM_MAP_UAP	"map_uap"

/* MulitAP Modes */
#define MAP_MODE_FLAG_DISABLED		0x0000	/* Disabled */
#define MAP_MODE_FLAG_CONTROLLER	0x0001	/* Controller */
#define MAP_MODE_FLAG_AGENT		0x0002	/* Agent */

#define MAP_IS_DISABLED(mode)	((mode) <= MAP_MODE_FLAG_DISABLED)	/* Is Disabled */
#define MAP_IS_CONTROLLER(mode)	((mode) & MAP_MODE_FLAG_CONTROLLER)	/* Is Controller */
#define MAP_IS_AGENT(mode)	((mode) & MAP_MODE_FLAG_AGENT)		/* Is Agent */
#endif /* defined(MULTIAP) || defined(BCM_WBD) */

#define PSPRETEND_DEFAULT_THRESHOLD 5

#define DPSTA_PRIMARY_AP_IDX (IS_FIRST_WET_BSSIDX + 1)

/* prototypes */
struct bsscfg_list *wlconf_get_bsscfgs(char* ifname, char* prefix);
int wlconf(char *name);
int wlconf_down(char *name);

static int
wlconf_getint(char* ifname, int cmd, int *pval)
{
	return wl_ioctl(ifname, cmd, pval, sizeof(int));
}

static int
wlconf_setint(char* ifname, int cmd, int val)
{
	return wl_ioctl(ifname, cmd, &val, sizeof(int));
}

static int
wlconf_wds_clear(char *name)
{
	struct maclist maclist;
	int    ret;

	maclist.count = 0;
	WL_IOCTL(name, WLC_SET_WDSLIST, &maclist, sizeof(maclist));

	return ret;
}

/* set WEP key */
static int
wlconf_set_wep_key(char *name, char *prefix, int bsscfg_idx, int i)
{
	wl_wsec_key_t key;
	char wl_key[] = "wlXXXXXXXXXX_keyXXXXXXXXXX";
	char *keystr, hex[] = "XX";
	unsigned char *data = key.data;
	int ret = 0;

	memset(&key, 0, sizeof(key));
	key.index = i - 1;
	sprintf(wl_key, "%skey%d", prefix, i);
	keystr = nvram_safe_get(wl_key);

	switch (strlen(keystr)) {
	case WEP1_KEY_SIZE:
	case WEP128_KEY_SIZE:
		key.len = strlen(keystr);
		strcpy((char *)key.data, keystr);
		break;
	case WEP1_KEY_HEX_SIZE:
	case WEP128_KEY_HEX_SIZE:
		key.len = strlen(keystr) / 2;
		while (*keystr) {
			strncpy(hex, keystr, 2);
			*data++ = (unsigned char) strtoul(hex, NULL, 16);
			keystr += 2;
		}
		break;
	default:
		key.len = 0;
		break;
	}

	/* Set current WEP key */
	if (key.len && i == atoi(nvram_safe_get(strcat_r(prefix, "key", wl_key))))
		key.flags = WL_PRIMARY_KEY;

	WL_BSSIOVAR_SET(name, "wsec_key", bsscfg_idx, &key, sizeof(key));

	return ret;
}

static int
wlconf_akm_options(char *prefix)
{
	char comb[32];
	char *wl_akm;
	int akm_ret_val = 0;
	char akm[32];
	char *next;

	wl_akm = nvram_safe_get(strcat_r(prefix, "akm", comb));
	foreach(akm, wl_akm, next) {
		if (!strcmp(akm, "wpa"))
			akm_ret_val |= WPA_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk"))
			akm_ret_val |= WPA_AUTH_PSK;
		if (!strcmp(akm, "wpa2"))
			akm_ret_val |= WPA2_AUTH_UNSPECIFIED;
		if (!strcmp(akm, "psk2"))
			akm_ret_val |= WPA2_AUTH_PSK;
		if (!strcmp(akm, "psk2ft"))
			akm_ret_val |= WPA2_AUTH_PSK | WPA2_AUTH_FT;
		if (!strcmp(akm, "brcm_psk"))
			akm_ret_val |= BRCM_AUTH_PSK;
	}
	return akm_ret_val;
}

/* Set up wsec */
static int
wlconf_set_wsec(char *ifname, char *prefix, int bsscfg_idx)
{
	char tmp[100];
	int val = 0;
	int akm_val;
	int ret;

	/* Set wsec bitvec */
	akm_val = wlconf_akm_options(prefix);
	if (akm_val != 0) {
		if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip"))
			val = TKIP_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "aes"))
			val = AES_ENABLED;
		else if (nvram_match(strcat_r(prefix, "crypto", tmp), "tkip+aes"))
			val = TKIP_ENABLED | AES_ENABLED;
	}
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled"))
		val |= WEP_ENABLED;
	WL_BSSIOVAR_SETINT(ifname, "wsec", bsscfg_idx, val);
	/* Set wsec restrict if WSEC_ENABLED */
	WL_BSSIOVAR_SETINT(ifname, "wsec_restrict", bsscfg_idx, val ? 1 : 0);

	return 0;
}

static int
wlconf_set_preauth(char *name, int bsscfg_idx, int preauth)
{
	uint cap;
	int ret;

	WL_BSSIOVAR_GET(name, "wpa_cap", bsscfg_idx, &cap, sizeof(uint));
	if (ret != 0) return -1;

	if (preauth)
		cap |= WPA_CAP_WPA2_PREAUTH;
	else
		cap &= ~WPA_CAP_WPA2_PREAUTH;

	WL_BSSIOVAR_SETINT(name, "wpa_cap", bsscfg_idx, cap);

	return ret;
}

static void
wlconf_dfs_pref_chan_options(char *name)
{
	char val[32], *next;
	wl_dfs_forced_t *dfs_frcd = NULL;
	uint ioctl_size;
	chanspec_t chanspec;
	int ret;
	wl_dfs_forced_t inp;

	dfs_frcd = (wl_dfs_forced_t *) malloc(WL_DFS_FORCED_PARAMS_MAX_SIZE);
	if (!dfs_frcd) {
		return;
	}

	memset(dfs_frcd, 0, WL_DFS_FORCED_PARAMS_MAX_SIZE);
	memset(&inp, 0, sizeof(wl_dfs_forced_t));

	inp.version = DFS_PREFCHANLIST_VER;
	wl_iovar_getbuf(name, "dfs_channel_forced", &inp, sizeof(wl_dfs_forced_t),
		dfs_frcd, WL_DFS_FORCED_PARAMS_MAX_SIZE);

	if (dfs_frcd->version != DFS_PREFCHANLIST_VER) {
		free(dfs_frcd);
		return;
	}

	dfs_frcd->chspec_list.num = 0;
	foreach(val, nvram_safe_get("wl_dfs_pref"), next) {
		if (atoi(val)) {
			chanspec = wf_chspec_aton(val);
			/* Maximum 6 entries supported in UI */
			if (dfs_frcd->chspec_list.num > 6)
				return;
			dfs_frcd->chspec_list.list[dfs_frcd->chspec_list.num++] = chanspec;
		}
	}

	ioctl_size = WL_DFS_FORCED_PARAMS_FIXED_SIZE +
		(dfs_frcd->chspec_list.num * sizeof(chanspec_t));
	dfs_frcd->version = DFS_PREFCHANLIST_VER;
	WL_IOVAR_SET(name, "dfs_channel_forced", dfs_frcd, ioctl_size);

	free(dfs_frcd);
	return;
}

static void
wlconf_set_radarthrs(char *name, char *prefix)
{
	wl_radar_thr_t  radar_thr;
	int  i, ret, len;
	char nv_buf[NVRAM_MAX_VALUE_LEN], *rargs, *v, *endptr;
	char buf[WLC_IOCTL_SMLEN];

	char *version = NULL;
	char *thr0_20_lo = NULL, *thr1_20_lo = NULL;
	char *thr0_40_lo = NULL, *thr1_40_lo = NULL;
	char *thr0_80_lo = NULL, *thr1_80_lo = NULL;
	char *thr0_20_hi = NULL, *thr1_20_hi = NULL;
	char *thr0_40_hi = NULL, *thr1_40_hi = NULL;
	char *thr0_80_hi = NULL, *thr1_80_hi = NULL;
	char *thr0_160_lo = NULL, *thr1_160_lo = NULL;
	char *thr0_160_hi = NULL, *thr1_160_hi = NULL;
	char **locals[] = { &version, &thr0_20_lo, &thr1_20_lo, &thr0_40_lo, &thr1_40_lo,
	&thr0_80_lo, &thr1_80_lo, &thr0_20_hi, &thr1_20_hi,
	&thr0_40_hi, &thr1_40_hi, &thr0_80_hi, &thr1_80_hi,
	&thr0_160_lo, &thr1_160_lo, &thr0_160_hi, &thr1_160_hi
	};

	rargs = nvram_safe_get(strcat_r(prefix, "radarthrs", nv_buf));
	if (!rargs)
		goto err;

	len = strlen(rargs);
	if ((len > NVRAM_MAX_VALUE_LEN) || (len == 0))
		goto err;

	memset(nv_buf, 0, sizeof(nv_buf));
	strncpy(nv_buf, rargs, len);
	v = nv_buf;
	for (i = 0; i < (sizeof(locals) / sizeof(locals[0])); i++) {
		*locals[i] = v;
		while (*v && *v != ' ') {
			v++;
		}
		if (*v) {
			*v = 0;
			v++;
		}
		if (v >= (nv_buf + len)) /* Check for complete list, if not caught later */
			break;
	}

	/* Start building request */
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "radarthrs");
	/* Retrieve radar thrs parameters */
	if (!version)
		goto err;
	radar_thr.version = atoi(version);
	if (radar_thr.version > WL_RADAR_THR_VERSION)
		goto err;

	/* Retrieve ver 0 params */
	if (!thr0_20_lo)
		goto err;
	radar_thr.thresh0_20_lo = (uint16)strtol(thr0_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_20_lo)
		goto err;
	radar_thr.thresh1_20_lo = (uint16)strtol(thr1_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_40_lo)
		goto err;
	radar_thr.thresh0_40_lo = (uint16)strtol(thr0_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_40_lo)
		goto err;
	radar_thr.thresh1_40_lo = (uint16)strtol(thr1_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_80_lo)
		goto err;
	radar_thr.thresh0_80_lo = (uint16)strtol(thr0_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_80_lo)
		goto err;
	radar_thr.thresh1_80_lo = (uint16)strtol(thr1_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;
	if (!thr0_160_lo)
		goto err;
	radar_thr.thresh0_160_lo = (uint16)strtol(thr0_160_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_160_lo)
		goto err;
	radar_thr.thresh1_160_lo = (uint16)strtol(thr1_160_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (radar_thr.version == 0) {
		/*
		 * Attempt a best effort update of ver 0 to ver 1 by updating
		 * the appropriate values with the specified defaults.  The defaults
		 * are from the reference design.
		 */
		radar_thr.version = WL_RADAR_THR_VERSION; /* avoid driver rejecting it */
		radar_thr.thresh0_20_hi = 0x6ac;
		radar_thr.thresh1_20_hi = 0x6cc;
		radar_thr.thresh0_40_hi = 0x6bc;
		radar_thr.thresh1_40_hi = 0x6e0;
		radar_thr.thresh0_80_hi = 0x6b0;
		radar_thr.thresh1_80_hi = 0x30;
	} else {
		/* Retrieve ver 1 params */
		if (!thr0_20_hi)
			goto err;
		radar_thr.thresh0_20_hi = (uint16)strtol(thr0_20_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_20_hi)
			goto err;
		radar_thr.thresh1_20_hi = (uint16)strtol(thr1_20_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr0_40_hi)
			goto err;
		radar_thr.thresh0_40_hi = (uint16)strtol(thr0_40_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_40_hi)
			goto err;
		radar_thr.thresh1_40_hi = (uint16)strtol(thr1_40_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr0_80_hi)
			goto err;
		radar_thr.thresh0_80_hi = (uint16)strtol(thr0_80_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_80_hi)
			goto err;
		radar_thr.thresh1_80_hi = (uint16)strtol(thr1_80_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;
		if (!thr0_160_hi)
			goto err;
		radar_thr.thresh0_160_hi = (uint16)strtol(thr0_160_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

		if (!thr1_160_hi)
			goto err;
		radar_thr.thresh1_160_hi = (uint16)strtol(thr1_160_hi, &endptr, 0);
		if (*endptr != '\0')
			goto err;

	}

	/* Copy radar parameters into buffer and plug them to the driver */
	memcpy((char*)(buf + strlen(buf) + 1), (char*)&radar_thr, sizeof(wl_radar_thr_t));
	WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));

	return;

err:
	WLCONF_DBG("Did not parse radar thrs params, using driver defaults\n");
	return;
}

static void
wlconf_set_radarthrs2(char *name, char *prefix)
{
	wl_radar_thr2_t  radar_thr2;
	int  i, ret, len;
	char nv_buf[NVRAM_MAX_VALUE_LEN], *rargs, *v, *endptr;
	char buf[WLC_IOCTL_SMLEN];

	char *version = NULL;
	char *thr0_sc_20_lo = NULL, *thr1_sc_20_lo = NULL;
	char *thr0_sc_40_lo = NULL, *thr1_sc_40_lo = NULL;
	char *thr0_sc_80_lo = NULL, *thr1_sc_80_lo = NULL;
	char *thr0_sc_20_hi = NULL, *thr1_sc_20_hi = NULL;
	char *thr0_sc_40_hi = NULL, *thr1_sc_40_hi = NULL;
	char *thr0_sc_80_hi = NULL, *thr1_sc_80_hi = NULL;
	char *thr0_sc_160_lo = NULL, *thr1_sc_160_lo = NULL;
	char *thr0_sc_160_hi = NULL, *thr1_sc_160_hi = NULL;
	char *fc_varth_sb = NULL, *fc_varth_bin5_sb = NULL;
	char *notradar_enb = NULL, *max_notradar_lp = NULL;
	char *max_notradar = NULL, *max_notradar_lp_sc = NULL;
	char *max_notradar_sc = NULL, *highpow_war_enb = NULL;
	char *highpow_sp_ratio = NULL, *fm_chk_opt = NULL;
	char *fm_chk_pw = NULL, *fm_var_chk_pw = NULL;
	char *fm_thresh_sp1 = NULL, *fm_thresh_sp2 = NULL;
	char *fm_thresh_sp3 = NULL, *fm_thresh_etsi4 = NULL;
	char *fm_thresh_p1c = NULL, *fm_tol_div = NULL;

	char **locals[] = { &version, &thr0_sc_20_lo, &thr1_sc_20_lo,
	&thr0_sc_40_lo, &thr1_sc_40_lo, &thr0_sc_80_lo, &thr1_sc_80_lo,
	&thr0_sc_20_hi, &thr1_sc_20_hi, &thr0_sc_40_hi, &thr1_sc_40_hi,
	&thr0_sc_80_hi, &thr1_sc_80_hi,	&thr0_sc_160_lo, &thr1_sc_160_lo,
	&thr0_sc_160_hi, &thr1_sc_160_hi, &fc_varth_sb, &fc_varth_bin5_sb,
	&notradar_enb, &max_notradar_lp, &max_notradar, &max_notradar_lp_sc,
	&max_notradar_sc, &highpow_war_enb,
	&highpow_sp_ratio, &fm_chk_opt, &fm_chk_pw, &fm_var_chk_pw,
	&fm_thresh_sp1, &fm_thresh_sp2,	&fm_thresh_sp3, &fm_thresh_etsi4,
	&fm_thresh_p1c, &fm_tol_div
	};

	rargs = nvram_safe_get(strcat_r(prefix, "radarthrs2", nv_buf));
	if (!rargs)
		goto err;

	len = strlen(rargs);

	if ((len > NVRAM_MAX_VALUE_LEN) || (len == 0))
		goto err;

	memset(nv_buf, 0, sizeof(nv_buf));
	strncpy(nv_buf, rargs, len);
	v = nv_buf;
	for (i = 0; i < (sizeof(locals) / sizeof(locals[0])); i++) {
		*locals[i] = v;
		while (*v && *v != ' ') {
			v++;
		}
		if (*v) {
			*v = 0;
			v++;
		}
		if (v >= (nv_buf + len)) /* Check for complete list, if not caught later */
			break;
	}

	/* Start building request */
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "radarthrs2");
	/* Retrieve radar thrs parameters */
	if (!version)
		goto err;
	radar_thr2.version = atoi(version);
	if (radar_thr2.version > WL_RADAR_THR_VERSION)
		goto err;

	if (!thr0_sc_20_lo)
		goto err;
	radar_thr2.thresh0_sc_20_lo = (uint16)strtol(thr0_sc_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_20_lo)
		goto err;
	radar_thr2.thresh1_sc_20_lo = (uint16)strtol(thr1_sc_20_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_40_lo)
		goto err;
	radar_thr2.thresh0_sc_40_lo = (uint16)strtol(thr0_sc_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_40_lo)
		goto err;
	radar_thr2.thresh1_sc_40_lo = (uint16)strtol(thr1_sc_40_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_80_lo)
		goto err;
	radar_thr2.thresh0_sc_80_lo = (uint16)strtol(thr0_sc_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_80_lo)
		goto err;
	radar_thr2.thresh1_sc_80_lo = (uint16)strtol(thr1_sc_80_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_160_lo)
		goto err;
	radar_thr2.thresh0_sc_160_lo = (uint16)strtol(thr0_sc_160_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_160_lo)
		goto err;
	radar_thr2.thresh1_sc_160_lo = (uint16)strtol(thr1_sc_160_lo, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_20_hi)
		goto err;
	radar_thr2.thresh0_sc_20_hi = (uint16)strtol(thr0_sc_20_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_20_hi)
		goto err;
	radar_thr2.thresh1_sc_20_hi = (uint16)strtol(thr1_sc_20_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_40_hi)
		goto err;
	radar_thr2.thresh0_sc_40_hi = (uint16)strtol(thr0_sc_40_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_40_hi)
		goto err;
	radar_thr2.thresh1_sc_40_hi = (uint16)strtol(thr1_sc_40_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_80_hi)
		goto err;
	radar_thr2.thresh0_sc_80_hi = (uint16)strtol(thr0_sc_80_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_80_hi)
		goto err;
	radar_thr2.thresh1_sc_80_hi = (uint16)strtol(thr1_sc_80_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr0_sc_160_hi)
		goto err;
	radar_thr2.thresh0_sc_160_hi = (uint16)strtol(thr0_sc_160_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!thr1_sc_160_hi)
		goto err;
	radar_thr2.thresh1_sc_160_hi = (uint16)strtol(thr1_sc_160_hi, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fc_varth_sb)
		goto err;
	radar_thr2.fc_varth_sb = (uint16)strtol(fc_varth_sb, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fc_varth_bin5_sb)
		goto err;
	radar_thr2.fc_varth_bin5_sb = (uint16)strtol(fc_varth_bin5_sb, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!notradar_enb)
		goto err;
	radar_thr2.notradar_enb = (uint16)strtol(notradar_enb, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!max_notradar_lp)
		goto err;
	radar_thr2.max_notradar_lp = (uint16)strtol(max_notradar_lp, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!max_notradar)
		goto err;
	radar_thr2.max_notradar = (uint16)strtol(max_notradar, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!max_notradar_lp_sc)
		goto err;
	radar_thr2.max_notradar_lp_sc = (uint16)strtol(max_notradar_lp_sc, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!max_notradar_sc)
		goto err;
	radar_thr2.max_notradar_sc = (uint16)strtol(max_notradar_sc, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!highpow_war_enb)
		goto err;
	radar_thr2.highpow_war_enb = (uint16)strtol(highpow_war_enb, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!highpow_sp_ratio)
		goto err;
	radar_thr2.highpow_sp_ratio = (uint16)strtol(highpow_sp_ratio, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_chk_opt)
		goto err;
	radar_thr2.fm_chk_opt = (uint16)strtol(fm_chk_opt, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_chk_pw)
		goto err;
	radar_thr2.fm_chk_pw = (uint16)strtol(fm_chk_pw, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_var_chk_pw)
		goto err;
	radar_thr2.fm_var_chk_pw = (uint16)strtol(fm_var_chk_pw, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_thresh_sp1)
		goto err;
	radar_thr2.fm_thresh_sp1 = (uint16)strtol(fm_thresh_sp1, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_thresh_sp2)
		goto err;
	radar_thr2.fm_thresh_sp2 = (uint16)strtol(fm_thresh_sp2, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_thresh_sp3)
		goto err;
	radar_thr2.fm_thresh_sp3 = (uint16)strtol(fm_thresh_sp3, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_thresh_etsi4)
		goto err;
	radar_thr2.fm_thresh_etsi4 = (uint16)strtol(fm_thresh_etsi4, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_thresh_p1c)
		goto err;
	radar_thr2.fm_thresh_p1c = (uint16)strtol(fm_thresh_p1c, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	if (!fm_tol_div)
		goto err;
	radar_thr2.fm_tol_div = (uint16)strtol(fm_tol_div, &endptr, 0);
	if (*endptr != '\0')
		goto err;

	/* Copy radar parameters into buffer and plug them to the driver */
	memcpy((char*)(buf + strlen(buf) + 1), (char*)&radar_thr2, sizeof(wl_radar_thr2_t));
	WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));

	return;

err:
	WLCONF_DBG("Did not parse radar thrs2 params, using driver defaults\n");
	return;
}

/*
 * This allows phy antenna selection to be retrieved from NVRAM
 */
static void
wlconf_set_antsel(char *name, char *prefix)
{
	int	i, j, len, argc, ret;
	char	buf[WLC_IOCTL_SMLEN];
	wlc_antselcfg_t val = { {0}, 0};
	char	*argv[ANT_SELCFG_MAX] = {};
	char	nv_buf[NVRAM_MAX_VALUE_LEN], *argstr, *v, *endptr;

	argstr = nvram_safe_get(strcat_r(prefix, "phy_antsel", nv_buf));
	if (!argstr) {
		return;
	}
	len = strlen(argstr);
	if ((len == 0) || (len > NVRAM_MAX_VALUE_LEN)) {
		return;
	}

	memset(nv_buf, 0, sizeof(nv_buf));
	strncpy(nv_buf, argstr, len);
	v = nv_buf;
	for (argc = 0; argc < ANT_SELCFG_MAX; ) {
		argv[argc++] = v;
		while (*v && *v != ' ') {
			v++;
		}
		if (*v) {
			*v = 0;
			v++;
		}
		if (v >= (nv_buf + len)) {
			break;
		}
	}
	if ((argc != 1) && (argc != ANT_SELCFG_MAX)) {
		WLCONF_DBG("phy_antsel requires 1 or %d arguments\n", ANT_SELCFG_MAX);
		return;
	}

	memset(buf, 0, sizeof(buf));
	strcpy(buf, "phy_antsel");
	for (i = 0, j = 0; i < ANT_SELCFG_MAX; i++) {
		val.ant_config[i] = (uint8)strtol(argv[j], &endptr, 0);
		if (*endptr != '\0') {
			WLCONF_DBG("Invalid antsel argument\n");
			return;
		}
		if (argc > 1) {
			/* ANT_SELCFG_MAX argument format */
			j++;
		}
	}

	/* Copy antsel parameters into buffer and plug them to the driver */
	memcpy((char*)(buf + strlen(buf) + 1), (char*)&val, sizeof(wlc_antselcfg_t));
	WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));

	return;
}

static void
wlconf_set_current_txparam_into_nvram(char *name, char *prefix)
{
	int ret, aci;
	wme_tx_params_t txparams[AC_COUNT];
	char *nv[] = {"wme_txp_be", "wme_txp_bk", "wme_txp_vi", "wme_txp_vo"};
	char data[50], tmp[50];

	/* get the WME tx parameters */
	WL_IOVAR_GET(name, "wme_tx_params", txparams, sizeof(txparams));

	/* Set nvram accordingly */
	for (aci = 0; aci < AC_COUNT; aci++) {
		sprintf(data, "%d %d %d %d %d", txparams[aci].short_retry,
			txparams[aci].short_fallback,
			txparams[aci].long_retry,
			txparams[aci].long_fallback,
			txparams[aci].max_rate);

		nvram_set(strcat_r(prefix, nv[aci], tmp), data);
	}
}

/* Set up WME */
static void
wlconf_set_wme(char *name, char *prefix)
{
	int i, j, k;
	int val, ret;
	int phytype, gmode, no_ack, apsd, dp[2];
	edcf_acparam_t *acparams;
	/* Pay attention to buffer length requirements when using this */
	char buf[WLC_IOCTL_SMLEN*2];
	char *v, *nv_value, nv[100];
	char nv_name[] = "%swme_%s_%s";
	char *ac[] = {"be", "bk", "vi", "vo"};
	char *cwmin, *cwmax, *aifsn, *txop_b, *txop_ag, *admin_forced, *oldest_first;
	char **locals[] = { &cwmin, &cwmax, &aifsn, &txop_b, &txop_ag, &admin_forced,
	                    &oldest_first };
	struct {char *req; char *str;} mode[] = {{"wme_ac_ap", "ap"}, {"wme_ac_sta", "sta"},
	                                         {"wme_tx_params", "txp"}};

	/* query the phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	/* get gmode */
	gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", nv)));

	/* WME sta setting first */
	for (i = 0; i < 2; i++) {
		/* build request block */
		memset(buf, 0, sizeof(buf));
		strncpy(buf, mode[i].req, sizeof(buf)-1);
		/* put push wmeac params after "wme-ac" in buf */
		acparams = (edcf_acparam_t *)(buf + strlen(buf) + 1);
		dp[i] = 0;
		for (j = 0; j < AC_COUNT; j++) {
			/* get packed nvram parameter */
			snprintf(nv, sizeof(nv), nv_name, prefix, mode[i].str, ac[j]);
			nv_value = nvram_safe_get(nv);
			strncpy(nv, nv_value, sizeof(nv)-1);
			nv[sizeof(nv)-1] = '\0';
			/* unpack it */
			v = nv;
			for (k = 0; k < (sizeof(locals) / sizeof(locals[0])); k++) {
				*locals[k] = v;
				while (*v && *v != ' ')
					v++;
				if (*v) {
					*v = 0;
					v++;
				}
			}

			/* update CWmin */
			acparams->ECW &= ~EDCF_ECWMIN_MASK;
			val = atoi(cwmin);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= (k ? k - 1 : 0) & EDCF_ECWMIN_MASK;
			/* update CWmax */
			acparams->ECW &= ~EDCF_ECWMAX_MASK;
			val = atoi(cwmax);
			for (val++, k = 0; val; val >>= 1, k++);
			acparams->ECW |= ((k ? k - 1 : 0) << EDCF_ECWMAX_SHIFT) & EDCF_ECWMAX_MASK;
			/* update AIFSN */
			acparams->ACI &= ~EDCF_AIFSN_MASK;
			acparams->ACI |= atoi(aifsn) & EDCF_AIFSN_MASK;
			/* update ac */
			acparams->ACI &= ~EDCF_ACI_MASK;
			acparams->ACI |= j << EDCF_ACI_SHIFT;
			/* update TXOP */
			if (phytype == PHY_TYPE_B || gmode == 0)
				val = atoi(txop_b);
			else
				val = atoi(txop_ag);
			acparams->TXOP = val / 32;
			/* update acm */
			acparams->ACI &= ~EDCF_ACM_MASK;
			val = strcmp(admin_forced, "on") ? 0 : 1;
			acparams->ACI |= val << 4;

			/* configure driver */
			WL_IOCTL(name, WLC_SET_VAR, buf, sizeof(buf));
		}
	}

	/* set no-ack */
	v = nvram_safe_get(strcat_r(prefix, "wme_no_ack", nv));
	no_ack = strcmp(v, "on") ? 0 : 1;
	WL_IOVAR_SETINT(name, "wme_noack", no_ack);

	/* set APSD */
	v = nvram_safe_get(strcat_r(prefix, "wme_apsd", nv));
	apsd = strcmp(v, "on") ? 0 : 1;
	WL_IOVAR_SETINT(name, "wme_apsd", apsd);

	/* set per-AC discard policy */
	strcpy(buf, "wme_dp");
	WL_IOVAR_SETINT(name, "wme_dp", dp[1]);

	/* WME Tx parameters setting */
	{
		wme_tx_params_t txparams[AC_COUNT];
		char *srl, *sfbl, *lrl, *lfbl, *maxrate;
		char **locals[] = { &srl, &sfbl, &lrl, &lfbl, &maxrate };

		/* build request block */
		memset(txparams, 0, sizeof(txparams));

		for (j = 0; j < AC_COUNT; j++) {
			/* get packed nvram parameter */
			snprintf(nv, sizeof(nv), nv_name, prefix, mode[2].str, ac[j]);
			nv_value = nvram_safe_get(nv);
			strncpy(nv, nv_value, sizeof(nv)-1);
			nv[sizeof(nv)-1] = '\0';
			/* unpack it */
			v = nv;
			for (k = 0; k < (sizeof(locals) / sizeof(locals[0])); k++) {
				*locals[k] = v;
				while (*v && *v != ' ')
					v++;
				if (*v) {
					*v = 0;
					v++;
				}
			}

			/* update short retry limit */
			txparams[j].short_retry = atoi(srl);

			/* update short fallback limit */
			txparams[j].short_fallback = atoi(sfbl);

			/* update long retry limit */
			txparams[j].long_retry = atoi(lrl);

			/* update long fallback limit */
			txparams[j].long_fallback = atoi(lfbl);

			/* update max rate */
			txparams[j].max_rate = atoi(maxrate);
		}

		/* set the WME tx parameters */
		WL_IOVAR_SET(name, mode[2].req, txparams, sizeof(txparams));
	}

	return;
}

/*
* XXX - should find these utilities a home where everybody can share
*/
#if defined(linux) || defined(__NetBSD__)
#include <unistd.h>
static void
sleep_ms(const unsigned int ms)
{
	usleep(1000*ms);
}
#elif defined(_RTE_)
#define sleep_ms(ms) hnd_delay(ms*1000)
#else
#error "sleep_ms() not defined for this OS!!!"
#endif /* defined(linux) */

/*
* The following condition(s) must be met when Auto Channel Selection
* is enabled.
*  - the I/F is up (change radio channel requires it is up?)
*  - the AP must not be associated (setting SSID to empty should
*    make sure it for us)
*/
static uint8
wlconf_auto_channel(char *name)
{
	int chosen = 0;
	wl_uint32_list_t request;
	int phytype;
	int ret;
	int i;

	/* query the phy type */
	WL_GETINT(name, WLC_GET_PHYTYPE, &phytype);

	request.count = 0;	/* let the ioctl decide */
	WL_IOCTL(name, WLC_START_CHANNEL_SEL, &request, sizeof(request));
	if (!ret) {
		sleep_ms(phytype == PHY_TYPE_A ? 1000 : 750);
		for (i = 0; i < 100; i++) {
			WL_GETINT(name, WLC_GET_CHANNEL_SEL, &chosen);
			if (!ret)
				break;
			sleep_ms(100);
		}
	}
	WLCONF_DBG("interface %s: channel selected %d\n", name, chosen);
	return chosen;
}

static chanspec_t
wlconf_auto_chanspec(char *name)
{
	chanspec_t chosen = 0;
	int temp = 0;
	wl_uint32_list_t request;
	int ret;
	int i;

	request.count = 0;	/* let the ioctl decide */
	WL_IOCTL(name, WLC_START_CHANNEL_SEL, &request, sizeof(request));
	if (!ret) {
		/* this time needs to be < 1000 to prevent mpc kicking in for 2nd radio */
		sleep_ms(500);
		for (i = 0; i < 100; i++) {
			WL_IOVAR_GETINT(name, "apcschspec", &temp);
			if (!ret)
				break;
			sleep_ms(100);
		}
	}

	chosen = (chanspec_t) temp;
	WLCONF_DBG("interface %s: chanspec selected %04x\n", name, chosen);
	return chosen;
}

/* PHY type/BAND conversion */
#define WLCONF_PHYTYPE2BAND(phy)	((phy) == PHY_TYPE_A ? WLC_BAND_5G : WLC_BAND_2G)
/* PHY type conversion */
#define WLCONF_PHYTYPE2STR(phy)	((phy) == PHY_TYPE_A ? "a" : \
				 (phy) == PHY_TYPE_B ? "b" : \
				 (phy) == PHY_TYPE_LP ? "l" : \
				 (phy) == PHY_TYPE_G ? "g" : \
				 (phy) == PHY_TYPE_SSN ? "s" : \
				 (phy) == PHY_TYPE_HT ? "h" : \
				 (phy) == PHY_TYPE_AC ? "v" : \
				 (phy) == PHY_TYPE_LCN ? "c" : "n")
#define WLCONF_STR2PHYTYPE(ch)	((ch) == 'a' ? PHY_TYPE_A : \
				 (ch) == 'b' ? PHY_TYPE_B : \
				 (ch) == 'l' ? PHY_TYPE_LP : \
				 (ch) == 'g' ? PHY_TYPE_G : \
				 (ch) == 's' ? PHY_TYPE_SSN : \
				 (ch) == 'h' ? PHY_TYPE_HT : \
				 (ch) == 'v' ? PHY_TYPE_AC : \
				 (ch) == 'c' ? PHY_TYPE_LCN : PHY_TYPE_N)

#define PREFIX_LEN 32			/* buffer size for wlXXX_ prefix */

#define WLCONF_PHYTYPE_11N(phy) ((phy) == PHY_TYPE_N 	|| (phy) == PHY_TYPE_SSN || \
				 (phy) == PHY_TYPE_LCN 	|| (phy) == PHY_TYPE_HT || \
				 (phy) == PHY_TYPE_AC)

struct bsscfg_info {
	int idx;			/* bsscfg index */
	char ifname[PREFIX_LEN];	/* OS name of interface (debug only) */
	char prefix[PREFIX_LEN];	/* prefix for nvram params (eg. "wl0.1_") */
};

struct bsscfg_list {
	int count;
	struct bsscfg_info bsscfgs[WL_MAXBSSCFG];
};

struct bsscfg_list *
wlconf_get_bsscfgs(char* ifname, char* prefix)
{
	char var[80];
	char tmp[100];
	char *next;

	struct bsscfg_list *bclist;
	struct bsscfg_info *bsscfg;

	bclist = (struct bsscfg_list*)malloc(sizeof(struct bsscfg_list));
	if (bclist == NULL)
		return NULL;
	memset(bclist, 0, sizeof(struct bsscfg_list));

	/* Set up Primary BSS Config information */
	bsscfg = &bclist->bsscfgs[0];
	bsscfg->idx = 0;
	strncpy(bsscfg->ifname, ifname, PREFIX_LEN-1);
	strncpy(bsscfg->prefix, prefix, sizeof(bsscfg->prefix) - 1);
	bclist->count = 1;

	/* additional virtual BSS Configs from wlX_vifs */
	foreach(var, nvram_safe_get(strcat_r(prefix, "vifs", tmp)), next) {
		if (bclist->count == WL_MAXBSSCFG) {
			WLCONF_DBG("wlconf(%s): exceeded max number of BSS Configs (%d)"
			           "in nvram %s\n"
			           "while configuring interface \"%s\"\n",
			           ifname, WL_MAXBSSCFG, strcat_r(prefix, "vifs", tmp), var);
			continue;
		}
		bsscfg = &bclist->bsscfgs[bclist->count];
		if (get_ifname_unit(var, NULL, &bsscfg->idx) != 0) {
			WLCONF_DBG("wlconfg(%s): unable to parse unit.subunit in interface "
			           "name \"%s\"\n",
			           ifname, var);
			continue;
		}
		strncpy(bsscfg->ifname, var, PREFIX_LEN-1);
		snprintf(bsscfg->prefix, PREFIX_LEN, "%s_", bsscfg->ifname);
		bclist->count++;
	}

	return bclist;
}

static void
wlconf_config_join_pref(char *name, int bsscfg_idx, int auth_val)
{
	int ret = 0, i = 0;

	if ((auth_val & (WPA_AUTH_UNSPECIFIED | WPA2_AUTH_UNSPECIFIED)) ||
	    CHECK_PSK(auth_val)) {
		uchar pref[] = {
		/* WPA pref, 14 tuples */
		0x02, 0xaa, 0x00, 0x0e,
		/* WPA2                 AES  (unicast)          AES (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
		/* WPA                  AES  (unicast)          AES (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x04,
		/* WPA2                 AES  (unicast)          TKIP (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x02,
		/* WPA                  AES  (unicast)          TKIP (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x02,
		/* WPA2                 AES  (unicast)          WEP-40 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x01,
		/* WPA                  AES  (unicast)          WEP-40 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x01,
		/* WPA2                 AES  (unicast)          WEP-128 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x05,
		/* WPA                  AES  (unicast)          WEP-128 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x04, 0x00, 0x50, 0xf2, 0x05,
		/* WPA2                 TKIP (unicast)          TKIP (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x02,
		/* WPA                  TKIP (unicast)          TKIP (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x02,
		/* WPA2                 TKIP (unicast)          WEP-40 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x01,
		/* WPA                  TKIP (unicast)          WEP-40 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x01,
		/* WPA2                 TKIP (unicast)          WEP-128 (multicast) */
		0x00, 0x0f, 0xac, 0x01, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x0f, 0xac, 0x05,
		/* WPA                  TKIP (unicast)          WEP-128 (multicast) */
		0x00, 0x50, 0xf2, 0x01, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x50, 0xf2, 0x05,
		/* RSSI pref */
		0x01, 0x02, 0x00, 0x00,
		};

		if (CHECK_PSK(auth_val)) {
			for (i = 0; i < pref[3]; i ++)
				pref[7 + i * 12] = 0x02;
		}

		WL_BSSIOVAR_SET(name, "join_pref", bsscfg_idx, pref, sizeof(pref));
	}

}

static void
wlconf_configure_mbo(char *name, char *prefix, int bsscfg_idx)
{
	int ret;
	int mbo_ap_enable = 0;
	char tmp[100];
#ifdef MFP
	int val;
	char var[8];
#endif /* MFP */

	mbo_ap_enable = atoi(nvram_safe_get(strcat_r(prefix, "mbo_enable", tmp)));

	WL_IOVAR_XTLV_SETINT(name, "mbo", mbo_ap_enable, WL_MBO_IOV_VERSION,
		WL_MBO_CMD_AP_ENAB, WL_MBO_XTLV_AP_ENAB);
	if (mbo_ap_enable != 1) {
		return;
	}

	/* Pre-requisite:
	 * Wi-Fi Agile Multiband AP shall support the following features:
	 * Enables Protected Management Frames (PMF) whenever WPA2 is enabled.
	 * A Wi-Fi Agile Multiband AP with WPA2 enabled shall require PMF to
	 * be negotiated for use in the association when a Wi-Fi Agile Multiband
	 * STA associates with it
	 */
#ifdef MFP
	/* Set MFP */
	val = WPA_AUTH_DISABLED;
	WL_BSSIOVAR_GET(name, "wpa_auth", bsscfg_idx, &val, sizeof(val));
	if (val & (WPA2_AUTH_PSK | WPA2_AUTH_UNSPECIFIED | WPA2_AUTH_FT)) {
		int phytype;

		val = atoi(nvram_safe_get(strcat_r(prefix, "mfp", tmp)));
		/* explictly enable mfp */
		if ((val == -1) || (val == 0)) {
			WL_GETINT(name, WLC_GET_PHYTYPE,  &phytype);
			/* Enable PMF to capable for 11AC dual-band and 11AX AP */
			if ((phytype == PHY_TYPE_AC) &&
				(nvram_match(strcat_r(prefix, "mode", tmp), "ap")))
				val = WL_MFP_CAPABLE;
			snprintf(var, sizeof(var), "%d", val);
			nvram_set(strcat_r(prefix, "mfp", tmp), var);
			WL_BSSIOVAR_SETINT(name, "mfp", bsscfg_idx, val);
		}
	}
#endif /* ifdef MFP */
}

static void
wlconf_security_options(char *name, char *prefix, int bsscfg_idx, bool id_supp,
                        bool check_join_pref)
{
	int i;
	int val;
	int ret;
	char tmp[100];
	bool need_join_pref = FALSE;
#define AUTOWPA(cfg) \
	(((cfg) & (WPA_AUTH_PSK | WPA2_AUTH_PSK)) == (WPA_AUTH_PSK | WPA2_AUTH_PSK))

	/* Set WSEC */
	/*
	* Need to check errors (card may have changed) and change to
	* defaults since the new chip may not support the requested
	* encryptions after the card has been changed.
	*/
	if (wlconf_set_wsec(name, prefix, bsscfg_idx)) {
		/* change nvram only, code below will pass them on */
		nvram_restore_var(prefix, "auth_mode");
		nvram_restore_var(prefix, "auth");
		/* reset wep to default */
		nvram_restore_var(prefix, "crypto");
		nvram_restore_var(prefix, "wep");
		wlconf_set_wsec(name, prefix, bsscfg_idx);
	}

	val = wlconf_akm_options(prefix);
	if (!nvram_match(strcat_r(prefix, "mode", tmp), "ap"))
		need_join_pref = (check_join_pref || id_supp) && AUTOWPA(val);

	if (need_join_pref)
		wlconf_config_join_pref(name, bsscfg_idx, val);

	/* enable in-driver wpa supplicant? */
	if (id_supp && (CHECK_PSK(val))) {
		wsec_pmk_t psk;
		char *key;

		if (((key = nvram_get(strcat_r(prefix, "wpa_psk", tmp))) != NULL) &&
		    (strlen(key) < WSEC_MAX_PSK_LEN)) {
			psk.key_len = (ushort) strlen(key);
			psk.flags = WSEC_PASSPHRASE;
			strcpy((char *)psk.key, key);
			WL_IOCTL(name, WLC_SET_WSEC_PMK, &psk, sizeof(psk));
		}
		if (wl_iovar_setint(name, "sup_wpa", 1)) {
			WLCONF_DBG("(%s):wl_iovar_setint(sup_wpa,1) failed\n", name);
		}
	}

	if (!need_join_pref)
		WL_BSSIOVAR_SETINT(name, "wpa_auth", bsscfg_idx, val);

	/* EAP Restrict if we have an AKM or radius authentication */
	val = ((val != 0) || (nvram_match(strcat_r(prefix, "auth_mode", tmp), "radius")));
	WL_BSSIOVAR_SETINT(name, "eap_restrict", bsscfg_idx, val);

	/* Set WEP keys */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled")) {
		for (i = 1; i <= DOT11_MAX_DEFAULT_KEYS; i++)
			wlconf_set_wep_key(name, prefix, bsscfg_idx, i);
	}

	/* Set 802.11 authentication mode - open/shared */
	val = atoi(nvram_safe_get(strcat_r(prefix, "auth", tmp)));
	WL_BSSIOVAR_SETINT(name, "auth", bsscfg_idx, val);
#ifdef MFP
	/* Set MFP */
	val = WPA_AUTH_DISABLED;
	WL_BSSIOVAR_GET(name, "wpa_auth", bsscfg_idx, &val, sizeof(val));
	if (val & (WPA2_AUTH_PSK | WPA2_AUTH_UNSPECIFIED | WPA2_AUTH_FT)) {
		int phytype;
		char var[8];
		char *ptr = NULL;

		ptr = nvram_safe_get(strcat_r(prefix, "mfp", tmp));

		if (ptr == NULL) {
			printf("ptr should not be NULL! return.\n");
			return;
		}

		val = atoi(ptr);

		/* MFP nvram default is -1  for primary ifce and empty
		 * for virtual BSS.Set to CAPABLE in driver and nvram
		 */
		if (val == -1 || (val == 0 && !strcmp(ptr, ""))) {
			val = WL_MFP_DISABLE;
			WL_GETINT(name, WLC_GET_PHYTYPE,  &phytype);
			/* Enable PMF to capable for 11AC dual-band and 11AX AP */
			if ((phytype == PHY_TYPE_AC) &&
					(nvram_match(strcat_r(prefix, "mode", tmp), "ap")))
				val = WL_MFP_CAPABLE;
			snprintf(var, sizeof(var), "%d", val);
			nvram_set(strcat_r(prefix, "mfp", tmp), var);
		}
		WL_BSSIOVAR_SETINT(name, "mfp", bsscfg_idx, val);
	}
#endif /* ifdef MFP */

	if (bsscfg_idx == 0) { /* for primary interface */
		val = atoi(nvram_safe_get(strcat_r(prefix, "cevent", tmp)));
		WL_IOVAR_SETINT(name, "cevent", val);
	}
}

static void
wlconf_set_ampdu_retry_limit(char *name, char *prefix)
{
	int i, j, ret, nv_len;
	struct ampdu_retry_tid retry_limit;
	char *nv_name[2] = {"ampdu_rtylimit_tid", "ampdu_rr_rtylimit_tid"};
	char *iov_name[2] = {"ampdu_retry_limit_tid", "ampdu_rr_retry_limit_tid"};
	char *retry, *v, *nv_value, nv[100], tmp[100];

	/* Get packed AMPDU (rr) retry limit per-tid from NVRAM if present */
	for (i = 0; i < 2; i++) {
		nv_value = nvram_safe_get(strcat_r(prefix, nv_name[i], tmp));
		nv_len = strlen(nv_value);
		strcpy(nv, nv_value);

		/* unpack it */
		v = nv;
		for (j = 0; nv_len >= 0 && j < NUMPRIO; j++) {
			retry = v;
			while (*v && *v != ' ') {
				v++;
				nv_len--;
			}
			if (*v) {
				*v = 0;
				v++;
				nv_len--;
			}
			/* set the AMPDU retry limit per-tid */
			retry_limit.tid = j;
			retry_limit.retry = atoi(retry);
			WL_IOVAR_SET(name, iov_name[i], &retry_limit, sizeof(retry_limit));
		}
	}

	return;
}

static void
wlconf_set_traffic_thresh(char *name, char *prefix, int bandtype)
{
	wl_traffic_thresh_t trfdata;
	char tmp[100];
	int i, ret;

	if (bandtype == WLC_BAND_2G) {
		return;
	}

	memset(&trfdata, 0, sizeof(wl_traffic_thresh_t));
	trfdata.mode = WL_TRF_MODE_WL_FEATURE;
	trfdata.enable = 1;
	WL_IOVAR_SET(name, "traffic_thresh", &trfdata, sizeof(wl_traffic_thresh_t));
	trfdata.mode = WL_TRF_MODE_CFG_FEATURE;
	trfdata.enable = 1;
	WL_IOVAR_SET(name, "traffic_thresh", &trfdata, sizeof(wl_traffic_thresh_t));
	trfdata.mode = WL_TRF_MODE_AP;
	trfdata.enable = 1;
	/* enabling interface configuration */
	for (i = 0; i< WL_TRF_MAX_QUEUE; i++) {
		trfdata.type = i;
		if (i == WL_TRF_TO) {
			trfdata.num_secs = atoi(nvram_safe_get(strcat_r(prefix,
					"acs_ap_inttrf_total_numsecs", tmp)));
			trfdata.thresh = atoi(nvram_safe_get(strcat_r(prefix,
					"acs_ap_inttrf_total_thresh", tmp)));
		} else {
			trfdata.num_secs = atoi(nvram_safe_get(strcat_r(prefix,
					"acs_ap_inttrf_numsecs", tmp)));
			trfdata.thresh = atoi(nvram_safe_get(strcat_r(prefix,
					"acs_ap_inttrf_thresh", tmp)));
		}
		WL_IOVAR_SET(name, "traffic_thresh", &trfdata, sizeof(wl_traffic_thresh_t));
	}
	trfdata.mode = WL_TRF_MODE_STA;
	trfdata.enable = 1;

	for (i = 0; i< WL_TRF_MAX_QUEUE; i++) {
		trfdata.type = i;
		trfdata.num_secs = atoi(nvram_safe_get(strcat_r(prefix,
				"acs_sta_inttrf_numsecs", tmp)));
		trfdata.thresh = atoi(nvram_safe_get(strcat_r(prefix,
				"acs_sta_inttrf_thresh", tmp)));
		WL_IOVAR_SET(name, "traffic_thresh", &trfdata, sizeof(wl_traffic_thresh_t));
	}

	return;
}

/* Function to set the Short and the Long retry limits
 */
static void
wlconf_set_retry_limit(char *name, char *prefix)
{
		char *srl, *lrl;
		int srl_val, lrl_val, ret;
		char tmp[100];

		srl = nvram_safe_get(strcat_r(prefix, "srl", tmp));

		if (srl && *srl != '\0') {
			srl_val = atoi(srl);
			if (srl_val)
				WL_IOCTL(name, WLC_SET_SRL, &srl_val, sizeof(srl_val));
		}

		lrl = nvram_safe_get(strcat_r(prefix, "lrl", tmp));

		if (lrl && *lrl != '\0') {
			lrl_val = atoi(lrl);
			if (lrl_val)
				WL_IOCTL(name, WLC_SET_LRL, &lrl_val, sizeof(lrl_val));
		}
		return;
}

/*
 * When N-mode is ON, AMPDU, AMSDU are enabled/disabled
 * based on the nvram setting. Only one of the AMPDU or AMSDU options is enabled any
 * time. When N-mode is OFF or the device is non N-phy, AMPDU and AMSDU are turned off.
 *
 * WME/WMM is also set in this procedure as it depends on N.
 *     N ==> WMM is on by default
 *
 * Returns WME setting.
 */
static int
wlconf_ampdu_amsdu_set(char *name, char prefix[PREFIX_LEN], int nmode, int btc_mode)
{
	bool ampdu_valid_option = FALSE;
	bool amsdu_valid_option = FALSE;
	int  val, ampdu_option_val = OFF, amsdu_option_val = OFF;
	int wme_option_val = ON;  /* On by default */
	char caps[WLC_IOCTL_MEDLEN], var[80], *next, *wme_val;
	char buf[WLC_IOCTL_SMLEN];
	int len = (sizeof("amsdu") - 1);
	int ret, phytype;
	wlc_rev_info_t rev;
#ifdef __CONFIG_DHDAP__
	int is_dhd = 0;
#endif // endif
#ifdef linux
	struct utsname unamebuf;

	uname(&unamebuf);
#endif // endif

#ifdef __CONFIG_DHDAP__
	is_dhd = !dhd_probe(name);
#endif // endif
	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	WL_GETINT(name, WLC_GET_PHYTYPE, &phytype);

	/* First, clear WMM settings to avoid conflicts */
	WL_IOVAR_SETINT(name, "wme", OFF);

	/* Get WME setting from NVRAM if present */
	wme_val = nvram_get(strcat_r(prefix, "wme", caps));
	if (wme_val && !strcmp(wme_val, "off")) {
		wme_option_val = OFF;
	}

	/* Set options based on capability */
	WL_IOVAR_GET(name, "cap", (void *)caps, sizeof(caps));
	if (ret == 0) {
		foreach(var, caps, next) {
			char *nvram_str;
			bool amsdu = 0;

			/* Check for the capabilitiy 'amsdutx' */
			if (strncmp(var, "amsdutx", sizeof(var)) == 0) {
				var[len] = '\0';
				amsdu = 1;
			}
			nvram_str = nvram_get(strcat_r(prefix, var, buf));
			if (!nvram_str)
				continue;

			if (!strcmp(nvram_str, "on"))
				val = ON;
			else if (!strcmp(nvram_str, "off"))
				val = OFF;
			else if (!strcmp(nvram_str, "auto"))
				val = AUTO;
			else
				continue;

			if (strncmp(var, "ampdu", sizeof(var)) == 0) {
				ampdu_valid_option = TRUE;
				ampdu_option_val = val;
			}

			if (amsdu) {
				amsdu_valid_option = TRUE;
				amsdu_option_val = val;
			}
		}
	}

	if (nmode != OFF) { /* N-mode is ON/AUTO */
		if (ampdu_valid_option) {
			if (ampdu_option_val != OFF) {
				WL_IOVAR_SETINT(name, "amsdu", OFF);
				WL_IOVAR_SETINT(name, "ampdu", ampdu_option_val);
			} else {
				WL_IOVAR_SETINT(name, "ampdu", OFF);
			}

			if (rev.corerev < 40)
				wlconf_set_ampdu_retry_limit(name, prefix);
		}

		if (amsdu_valid_option) {
			if (amsdu_option_val != OFF) { /* AMPDU (above) has priority over AMSDU */
				if (rev.corerev >= 40) {
					WL_IOVAR_SETINT(name, "amsdu", amsdu_option_val);
#ifdef __CONFIG_DHDAP__
					/* Only for 43602 dhdap, ampdu_mpdu=32 gives good tput
					 * with amsdu but on BCM4365_CHIP it gets worst 20~30Mbps.
					 */
					if (is_dhd && rev.chipnum == BCM43602_CHIP_ID)
						WL_IOVAR_SETINT(name, "ampdu_mpdu", 32);
#endif // endif
				} else if (ampdu_option_val == OFF) {
					WL_IOVAR_SETINT(name, "ampdu", OFF);
					WL_IOVAR_SETINT(name, "amsdu", amsdu_option_val);
				} else if (rev.chipnum == BCM43217_CHIP_ID) {
					/* In 43217, ampdu_mpdu=32 gives good tput with amsdu */
					WL_IOVAR_SETINT(name, "ampdu_mpdu", 32);
				}
			} else {
				WL_IOVAR_SETINT(name, "amsdu", OFF);
#ifdef __CONFIG_DHDAP__
				/* Use default ampdu_mpdu=-1 when amsdu is disabled */
				if (is_dhd)
					WL_IOVAR_SETINT(name, "ampdu_mpdu", -1);
#endif // endif
			}
		} else {
#ifdef __CONFIG_DHDAP__
			/* Use default ampdu_mpdu=-1 when amsdu is disabled */
			if (is_dhd)
				WL_IOVAR_SETINT(name, "ampdu_mpdu", -1);
#endif // endif
		}
	} else {
		/* When N-mode is off or for non N-phy device, turn off AMPDU, AMSDU;
		 */
		WL_IOVAR_SETINT(name, "amsdu", OFF);
		WL_IOVAR_SETINT(name, "ampdu", OFF);
	}

	if (wme_option_val) {
		WL_IOVAR_SETINT(name, "wme", wme_option_val);
		wlconf_set_wme(name, prefix);
	}
	/* Override SRL & LRL if nvram configuration is defined */
	wlconf_set_retry_limit(name, prefix);
	return wme_option_val;
}

/* Get configured bandwidth cap. */
static int
wlconf_bw_cap(char *prefix, int bandtype)
{
	char *str, tmp[100];
	int bw_cap = WLC_BW_CAP_20MHZ;
	char *q = NULL;

	if ((str = nvram_get(strcat_r(prefix, "bw_cap", tmp))) != NULL)
		bw_cap = strtol(str, &q, 0);
	else {
		/* Backward compatibility. Map to bandwidth cap bitmap values. */
		int val = strtol(nvram_safe_get(strcat_r(prefix, "nbw_cap", tmp)), &q, 0);

		if (((bandtype == WLC_BAND_2G) && (val == WLC_N_BW_40ALL)) ||
		    ((bandtype == WLC_BAND_5G) &&
		     (val == WLC_N_BW_40ALL || val == WLC_N_BW_20IN2G_40IN5G)))
			bw_cap = WLC_BW_CAP_40MHZ;
		else
			bw_cap = WLC_BW_CAP_20MHZ;
	}

	return bw_cap;
}

/* Unset TXBF. Called when i/f is down. */
static void wlconf_unset_txbf(char *name, char *prefix)
{
	char tmp[100];
	int ret = 0;

	/* unset nvram TxBF off */
	nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "0");
	nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "0");
	nvram_set(strcat_r(prefix, "txbf_imp", tmp), "0");

	/* turning TXBF off */
	WL_IOVAR_SETINT(name, "txbf", 0);
	WL_IOVAR_SETINT(name, "txbf_bfr_cap", 0);
	WL_IOVAR_SETINT(name, "txbf_bfe_cap", 0);
	WL_IOVAR_SETINT(name, "txbf_imp", 0);

	/* unset nvram mu_features off */
	nvram_set(strcat_r(prefix, "mu_features", tmp), "0");
	/* turning mu_features off */
	WL_IOVAR_SETINT(name, "mu_features", 0);
}

/* Set up TxBF. Called when i/f is down. */
static void wlconf_set_txbf(char *name, char *prefix, int bandtype)
{
	char *str, tmp[100];
	wlc_rev_info_t rev;
	uint32 txbf_bfe_cap = 0;
	uint32 txbf_bfr_cap = 0;
	uint32 txbf_imp = 0;
	uint32 mu_features = 0;
	int ret = 0;

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	if (rev.corerev < 40) return;	/* TxBF unsupported */

	if ((str = nvram_get(strcat_r(prefix, "txbf_bfr_cap", tmp))) != NULL) {
		txbf_bfr_cap = (uint32)strtoul(str, NULL, 0);

		if (txbf_bfr_cap) {
			/* Turning TxBF on (order matters) */
			WL_IOVAR_SETINT(name, "txbf_bfr_cap", txbf_bfr_cap);
			WL_IOVAR_SETINT(name, "txbf", 1);
		} else {
			/* Similarly, turning TxBF off in reverse order */
			WL_IOVAR_SETINT(name, "txbf", 0);
			WL_IOVAR_SETINT(name, "txbf_bfr_cap", 0);
		}
	}

	if ((str = nvram_get(strcat_r(prefix, "txbf_bfe_cap", tmp))) != NULL) {
		txbf_bfe_cap = (uint32)strtoul(str, NULL, 0);

		WL_IOVAR_SETINT(name, "txbf_bfe_cap", txbf_bfe_cap);
	}

	if ((str = nvram_get(strcat_r(prefix, "txbf_imp", tmp))) != NULL) {
		txbf_imp = atoi(str);

		WL_IOVAR_SETINT(name, "txbf_imp", txbf_imp);
	}

	if (bandtype == WLC_BAND_2G) {
		/* turning mu_features off not tested in 2G band */
		WL_IOVAR_SETINT(name, "mu_features", 0);
	} else {
		if ((str = nvram_get(strcat_r(prefix, "mu_features", tmp))) != NULL) {
			mu_features = (uint32)strtoul(str, NULL, 0);
			WL_IOVAR_SETINT(name, "mu_features", mu_features);
		}
	}
}

/* Set up TxBF timer. Called when i/f is up. */
static void wlconf_set_txbf_timer(char *name, char *prefix)
{
	char *str, tmp[100];
	wlc_rev_info_t rev;
	uint32 txbf_timer = 0;
	int ret = 0;

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	if (rev.corerev < 40) return;	/* TxBF unsupported */

	if ((str = nvram_get(strcat_r(prefix, "txbf_timer", tmp))) != NULL) {
		txbf_timer = (uint32) atoi(str);
		WL_IOVAR_SETINT(name, "txbf_timer", txbf_timer);
	}
}

/* Apply Traffic Management filter settings stored in NVRAM */
static void
trf_mgmt_settings(char *prefix, bool dwm_supported)
{
	char buffer[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_RULES+1)];
	char iobuff[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_RULES+1)+32];
	int i, filterlen, ret = 0;
	trf_mgmt_config_t trf_mgmt_config;
	trf_mgmt_filter_list_t *trf_mgmt_filter_list;
	netconf_trmgmt_t nettrm;
	trf_mgmt_filter_t *trfmgmt;
	char *wlifname;
	struct in_addr ipaddr, ipmask;
	char nvram_ifname[32];
	bool tm_filters_configured = FALSE;
	/* DWM variables */
	bool dwm_filters_configured = FALSE;
	char dscp_filter_buffer[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_DWM_RULES)];
	char dscp_filter_iobuff[sizeof(trf_mgmt_filter_t)*(MAX_NUM_TRF_MGMT_DWM_RULES)+
		sizeof("trf_mgmt_filters_add") + OFFSETOF(trf_mgmt_filter_list_t, filter)];
	int dscp_filterlen;
	trf_mgmt_filter_t *trf_mgmt_dwm_filter;
	trf_mgmt_filter_list_t *trf_mgmt_dwm_filter_list = NULL;
	netconf_trmgmt_t nettrm_dwm;
	bool is_dhd = 0;

	snprintf(nvram_ifname, sizeof(nvram_ifname), "%sifname", prefix);
	wlifname = nvram_get(nvram_ifname);
	if (!wlifname) {
		return;
	}

	trf_mgmt_filter_list = (trf_mgmt_filter_list_t *)buffer;
	trfmgmt = &trf_mgmt_filter_list->filter[0];

	/* Initialize the common parameters */
	memset(buffer, 0, sizeof(buffer));
	memset(&trf_mgmt_config, 0, sizeof(trf_mgmt_config_t));

	/* no-rx packets, local subnet, don't override priority and no-traffic shape */
	trf_mgmt_config.flags = (TRF_MGMT_FLAG_NO_RX |
		TRF_MGMT_FLAG_MANAGE_LOCAL_TRAFFIC |
		TRF_MGMT_FLAG_DISABLE_SHAPING);
	(void) inet_aton("192.168.1.1", &ipaddr);         /* Dummy value */
	(void) inet_aton("255.255.255.0", &ipmask);       /* Dummy value */
	trf_mgmt_config.host_ip_addr = ipaddr.s_addr;     /* Dummy value */
	trf_mgmt_config.host_subnet_mask = ipmask.s_addr; /* Dummy value */
	trf_mgmt_config.downlink_bandwidth = 1;           /* Dummy value */
	trf_mgmt_config.uplink_bandwidth = 1;             /* Dummy value */

	/* Read up to NUM_TFF_MGMT_FILTERS entries from NVRAM */
	for (i = 0; i < MAX_NUM_TRF_MGMT_RULES; i++) {
		if (get_trf_mgmt_port(prefix, i, &nettrm) == FALSE) {
			continue;
		}
		if (nettrm.match.flags == NETCONF_DISABLED) {
			continue;
		}
		trfmgmt->dst_port = ntohs(nettrm.match.dst.ports[0]);
		trfmgmt->src_port = ntohs(nettrm.match.src.ports[0]);
		trfmgmt->prot = nettrm.match.ipproto;
		if (nettrm.favored) {
			trfmgmt->flags |= TRF_FILTER_FAVORED;
		}
		trfmgmt->priority = nettrm.prio;
		memcpy(&trfmgmt->dst_ether_addr, &nettrm.match.mac, sizeof(struct ether_addr));
		if (nettrm.match.ipproto == IPPROTO_IP) {
			/* Enable MAC filter */
			trf_mgmt_config.flags |= TRF_MGMT_FLAG_FILTER_ON_MACADDR;
			trfmgmt->flags |= TRF_FILTER_MAC_ADDR;
		}
		trf_mgmt_filter_list->num_filters += 1;
		trfmgmt++;
	}
	if (trf_mgmt_filter_list->num_filters)
		tm_filters_configured = TRUE;

	if (dwm_supported) {
		trf_mgmt_dwm_filter_list = (trf_mgmt_filter_list_t *)dscp_filter_buffer;
		trf_mgmt_dwm_filter = &trf_mgmt_dwm_filter_list->filter[0];
		memset(dscp_filter_buffer, 0, sizeof(dscp_filter_buffer));

		/* Read up to NUM_TFF_MGMT_FILTERS entries from NVRAM */
		for (i = 0; i < MAX_NUM_TRF_MGMT_DWM_RULES; i++) {
			if (get_trf_mgmt_dwm(prefix, i, &nettrm_dwm) == FALSE) {
				continue;
			}
			if (nettrm_dwm.match.flags == NETCONF_DISABLED) {
				continue;
			}

			trf_mgmt_dwm_filter->dscp = nettrm_dwm.match.dscp;
			if (nettrm_dwm.favored)
				trf_mgmt_dwm_filter->flags |= TRF_FILTER_FAVORED;
			trf_mgmt_dwm_filter->flags |= TRF_FILTER_DWM;
			trf_mgmt_dwm_filter->priority = nettrm_dwm.prio;
			trf_mgmt_dwm_filter++;
			trf_mgmt_dwm_filter_list->num_filters += 1;
		}
		if (trf_mgmt_dwm_filter_list->num_filters)
			dwm_filters_configured = TRUE;
	}

#ifdef __CONFIG_DHDAP__
	is_dhd = !dhd_probe(wlifname);
#endif // endif
	if (!is_dhd) {
		/* Disable traffic management module to initial known state */
		trf_mgmt_config.trf_mgmt_enabled = 0;
		WL_IOVAR_SET(wlifname, "trf_mgmt_config", &trf_mgmt_config,
			sizeof(trf_mgmt_config_t));
		/* Add traffic management filter entries */
		if (tm_filters_configured || dwm_filters_configured) {
			/* Enable traffic management module  before adding filter mappings */
			trf_mgmt_config.trf_mgmt_enabled = 1;
			WL_IOVAR_SET(wlifname, "trf_mgmt_config", &trf_mgmt_config,
				sizeof(trf_mgmt_config_t));
			if (tm_filters_configured) {
				/* Configure TM module filters mappings */
				filterlen = trf_mgmt_filter_list->num_filters *
					sizeof(trf_mgmt_filter_t) +
					OFFSETOF(trf_mgmt_filter_list_t, filter);
				wl_iovar_setbuf(wlifname, "trf_mgmt_filters_add",
					trf_mgmt_filter_list,
					filterlen, iobuff, sizeof(iobuff));
			}

			if (dwm_filters_configured) {
				dscp_filterlen =  trf_mgmt_dwm_filter_list->num_filters *
					sizeof(trf_mgmt_filter_t) +
					OFFSETOF(trf_mgmt_filter_list_t, filter);
				wl_iovar_setbuf(wlifname, "trf_mgmt_filters_add",
					trf_mgmt_dwm_filter_list,
					dscp_filterlen, dscp_filter_iobuff,
					sizeof(dscp_filter_iobuff));
			}
		}
	} else {
#ifdef __CONFIG_DHDAP__
		/*
		 * for dhd interfaces we are supporting dwm filters only, dwm filters are for
		 * dscp to wmm prio mapping. The following traffic management functions are not
		 * supported, port and protocol based filters, traffic shaping, RSSI based filters.
		 */

		if (dwm_filters_configured) {
			dscp_filterlen =  trf_mgmt_dwm_filter_list->num_filters *
				sizeof(trf_mgmt_filter_t) +
				OFFSETOF(trf_mgmt_filter_list_t, filter);
			dhd_iovar_setbuf(wlifname, "trf_mgmt_filters_add", trf_mgmt_dwm_filter_list,
				dscp_filterlen, dscp_filter_iobuff, sizeof(dscp_filter_iobuff));
		}
#endif // endif
	}
}

#ifdef TRAFFIC_MGMT_RSSI_POLICY
static void
trf_mgmt_rssi_policy(char *prefix)
{
	char *wlifname;
	char nvram_ifname[32];
	char rssi_policy[64];
	uint32 rssi_policy_value, ret;

	/* Get wl interface name */
	snprintf(nvram_ifname, sizeof(nvram_ifname), "%sifname", prefix);
	if ((wlifname = nvram_get(nvram_ifname)) == NULL) {
		return;
	}

	/* Get RSSI policy from NVRAM variable wlx_trf_mgmt_rssi_policy */
	snprintf(rssi_policy, sizeof(rssi_policy), "%strf_mgmt_rssi_policy", prefix);
	if (!nvram_invmatch(rssi_policy, ""))
		return;
	rssi_policy_value = atoi(nvram_get(rssi_policy));

	/* Enable/Disable RSSI policy depending on value of  rssi_policy_value */
	WL_IOVAR_SETINT(wlifname, "trf_mgmt_rssi_policy", rssi_policy_value);
}
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

#ifdef WL_PROXDETECT
static wl_proxd_iov_t *
wlconf_ftm_alloc_getset_buf(wl_proxd_method_t method, wl_proxd_session_id_t session_id,
	wl_proxd_cmd_t cmdid, uint16 tlvs_bufsize, uint16 *p_out_bufsize)
{
	wl_proxd_iov_t *p_proxd_iov = (wl_proxd_iov_t *) NULL;
	*p_out_bufsize = 0;

	/* calculate the whole buffer size, including one reserve-tlv entry in the header */
	uint16 proxd_iovsize = sizeof(wl_proxd_iov_t) + tlvs_bufsize;

	p_proxd_iov = calloc(1, proxd_iovsize);
	if (p_proxd_iov == NULL) {
		printf("error: failed to allocate %d bytes of memory\n", proxd_iovsize);
		return NULL;
	}

	/* setup proxd-FTM-method iovar header */
	p_proxd_iov->version = (WL_PROXD_API_VERSION);
	p_proxd_iov->len = (proxd_iovsize); /* caller may adjust it based on #of TLVs */
	p_proxd_iov->cmd = (cmdid);
	p_proxd_iov->method = (method);
	p_proxd_iov->sid = (session_id);

	/* initialize the reserved/dummy-TLV in iovar header */
	wl_proxd_tlv_t *p_tlv = p_proxd_iov->tlvs;
	p_tlv->id = (WL_PROXD_TLV_ID_NONE);
	p_tlv->len = (0);

	*p_out_bufsize = proxd_iovsize;	/* for caller's reference */

	return p_proxd_iov;
}

/* 'wl proxd ftm config' handler */
static int
wlconf_ftm_subcmd_config_tlv(char *name, struct bsscfg_info *bsscfg,
	uint16 cmdid, wl_proxd_method_t method,
	wl_proxd_session_id_t session_id, uint32 tlvdata, uint16 tlvid)
{
	uint16 proxd_iovsize = 0;
	wl_proxd_iov_t *p_proxd_iov = NULL;
	int ret = BCME_OK;
	uint16 bufsize;
	wl_proxd_tlv_t *p_tlv;
	uint16	buf_space_left;
	uint16 all_tlvsize;

	/* allocate a buffer for proxd-ftm config via 'set' iovar */
	p_proxd_iov = wlconf_ftm_alloc_getset_buf(method, session_id,
		cmdid, WLC_IOCTL_MEDLEN, &proxd_iovsize);
	if (p_proxd_iov == NULL || proxd_iovsize == 0) {
		ret = BCME_NOMEM;
		goto done;
	}

	/* setup TLVs */
	bufsize = proxd_iovsize - WL_PROXD_IOV_HDR_SIZE;	/* adjust available size for TLVs */
	p_tlv = &p_proxd_iov->tlvs[0];

	/* TLV buffer starts with a full size, will decrement for each packed TLV */
	buf_space_left = bufsize;

	if (tlvid == WL_PROXD_TLV_ID_FLAGS) {
		/* setup flags_mask TLV to go along with the flags,
		 * this will add flags instead of replacing the current flags
		 */
		ret = bcm_pack_xtlv_entry((uint8 **)&p_tlv, &buf_space_left,
			WL_PROXD_TLV_ID_FLAGS_MASK,
			sizeof(uint32), (uint8 *)&tlvdata, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			printf("%s: bcm_pack_xltv_entry() for flags_mask failed, status=%d\n",
				__FUNCTION__, ret);
			goto done;
		}
	}
	/* setup TLV */
	ret = bcm_pack_xtlv_entry((uint8 **)&p_tlv, &buf_space_left, tlvid,
		sizeof(uint32), (uint8 *)&tlvdata, BCM_XTLV_OPTION_ALIGN32);
	if (ret != BCME_OK) {
		printf("%s: bcm_pack_xltv_entry() for flags_mask failed, status=%d\n",
			__FUNCTION__, ret);
		goto done;
	}

	if (ret == BCME_OK) {
		/* update the iov header, set len to include all TLVs + header */
		all_tlvsize = (bufsize - buf_space_left);
		p_proxd_iov->len = (all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
		WL_BSSIOVAR_SET(name, "proxd", bsscfg->idx, p_proxd_iov,
			all_tlvsize + WL_PROXD_IOV_HDR_SIZE);
	}

done:
	/* clean up */
	if (p_proxd_iov != NULL) {
		free(p_proxd_iov);
	}
	return ret;
}

/* does not accept any parameters */
static int
wlconf_ftm_setiov_no_tlv(char *name, struct bsscfg_info *bsscfg,
	uint16 cmdid, wl_proxd_method_t method, wl_proxd_session_id_t session_id)
{
	/* allocate and initialize a temp buffer for 'set proxd' iovar */
	uint16 proxd_iovsize = 0;
	int ret = 0;
	wl_proxd_iov_t *p_proxd_iov = wlconf_ftm_alloc_getset_buf(method, session_id,
		cmdid, 0, &proxd_iovsize);
	if (p_proxd_iov == NULL || proxd_iovsize == 0) {
		ret = BCME_NOMEM;
		goto done;
	}

	/* no TLV to pack, simply issue a set-proxd iovar */
	WL_BSSIOVAR_SET(name, "proxd", bsscfg->idx, p_proxd_iov, proxd_iovsize);

done:
	/* clean up */
	if (p_proxd_iov != NULL) {
		free(p_proxd_iov);
	}
	return ret;
}

/*
* enable FTM method
*/
static int
wlconf_ftm_enable(char *name, struct bsscfg_info *bsscfg, int enable)
{
	/* issue an iovar call */
	return wlconf_ftm_setiov_no_tlv(name, bsscfg,
		(enable ? WL_PROXD_CMD_ENABLE : WL_PROXD_CMD_DISABLE),
		WL_PROXD_METHOD_FTM, WL_PROXD_SESSION_ID_GLOBAL);
}

/*
* config FTM LCI tx
*/
static int
wlconf_ftm_flags(char *name, struct bsscfg_info *bsscfg, uint32 flags)
{
	/* issue an iovar call */
	return wlconf_ftm_subcmd_config_tlv(name, bsscfg, WL_PROXD_CMD_CONFIG,
		WL_PROXD_METHOD_FTM, WL_PROXD_SESSION_ID_GLOBAL, flags, WL_PROXD_TLV_ID_FLAGS);
}

static int
wlconf_rrm_parse_location(char *loc_arg, char *bufptr, int buflen)
{
	int len = 0;
	char *ptr = loc_arg;
	char hex[] = "XX";

	if (!loc_arg) {
		printf("%s: Location data is missing\n", __FUNCTION__);
		len = -1;
		goto done;
	}

	len = strlen(loc_arg)/2;
	if ((len <= 0) || (len > buflen)) {
		len = -1;
		goto done;
	}

	if ((uint16)strlen(ptr) != len*2) {
		printf("%s: Invalid length. Even number of characters expected.\n",
			__FUNCTION__);
		len = -1;
		goto done;
	}

	while (*ptr) {
		strncpy(hex, ptr, 2);
		*bufptr++ = (char) strtoul(hex, NULL, 16);
		ptr += 2;
	}
done:
	return len;
}

static int
wlconf_rrm_self_lci_civic(char *name, struct bsscfg_info *bsscfg, int cmd_id, char *loc)
{
	int ret = BCME_OK;
	int len = 0;
	char *bufptr;
	wl_rrm_config_ioc_t *rrm_config_cmd = NULL;
	int malloc_len = sizeof(*rrm_config_cmd) + TLV_BODY_LEN_MAX -
		DOT11_MNG_IE_MREP_FIXED_LEN;

	rrm_config_cmd = (wl_rrm_config_ioc_t *) calloc(1, malloc_len);
	if (rrm_config_cmd == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		ret = BCME_NOMEM;
		goto done;
	}

	rrm_config_cmd->id = (uint16)cmd_id;
	bufptr = (char *)&rrm_config_cmd->data[0];
	len = wlconf_rrm_parse_location(loc, bufptr, TLV_BODY_LEN_MAX -
			DOT11_MNG_IE_MREP_FIXED_LEN);
	if (len <= 0) {
		printf("%s: parsing location arguments failed\n", __FUNCTION__);
		ret = BCME_USAGE_ERROR;
		goto done;
	}

	rrm_config_cmd->len = (uint16)len;
	WL_BSSIOVAR_SET(name, WL_RRM_CONFIG_NAME, bsscfg->idx, (void *)rrm_config_cmd,
		WL_RRM_CONFIG_MIN_LENGTH + len);
done:
	if (rrm_config_cmd != NULL) {
		free(rrm_config_cmd);
	}
	return ret;
}
#endif /* WL_PROXDETECT */

static int
wlconf_del_brcm_syscap_ie(char *name, int bsscfg_idx, char *oui)
{
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf = NULL;
	int iecount, i;

	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;
	int ret = 0;

	frametype = VNDR_IE_BEACON_FLAG;

	WL_BSSIOVAR_GET(name, "vndr_ie", bsscfg_idx, getbuf, 2048);
	iebuf = (vndr_ie_buf_t *)getbuf;

	bufaddr = (char*)iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t *)bufaddr;
		bcopy((char*)&ieinfo->pktflag, (char*)&pktflag, (int)sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, oui, DOT11_OUI_LEN)) {
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}
		bufaddr = (char *)(ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len);
	}

	if (!found)
		goto err;

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *)malloc(iebuf_len);
	if (!ie_setbuf) {
		WLCONF_DBG("memory alloc failure\n");
		ret = -1;
		goto err;
	}

	memset(ie_setbuf, 0, iebuf_len);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	WL_BSSIOVAR_SET(name, "vndr_ie", bsscfg_idx, ie_setbuf, iebuf_len);

err:
	if (ie_setbuf)
		free(ie_setbuf);

	return ret;
}

static int
wlconf_set_brcm_syscap_ie(char *name, int bsscfg_idx, char *oui, uchar *data, int datalen)
{
	vndr_ie_setbuf_t *ie_setbuf = NULL;
	unsigned int pktflag;
	int buflen, iecount;
	int ret = 0;

	pktflag = VNDR_IE_BEACON_FLAG;

	buflen = sizeof(vndr_ie_setbuf_t) + datalen - 1;
	ie_setbuf = (vndr_ie_setbuf_t *)malloc(buflen);
	if (!ie_setbuf) {
		WLCONF_DBG("memory alloc failure\n");
		ret = -1;
		goto err;
	}

	memset(ie_setbuf, 0, buflen);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* The packet flag bit field indicates the packets that will contain this IE */
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer, +1: one byte OUI_TYPE */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = DOT11_OUI_LEN + datalen;

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0], oui, DOT11_OUI_LEN);
	if (datalen > 0)
		memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0], data,
		       datalen);

	ret = wlconf_del_brcm_syscap_ie(name, bsscfg_idx, oui);
	if (ret)
		goto err;

	WL_BSSIOVAR_SET(name, "vndr_ie", (int)bsscfg_idx, ie_setbuf, buflen);

err:
	if (ie_setbuf)
		free(ie_setbuf);

	return (ret);
}

/*
 * wlconf_process_sta_config_entry() - process a single sta_config settings entry.
 *
 *	This function processes a single sta_config settings entry by parsing the entry and
 *	calling the appropriate IOVAR(s) to apply the settings in the driver.
 *
 * Inputs:
 *	params	- address of a nul terminated ascii string buffer containing a single sta_config
 *		  settings entry in the form "xx:xx:xx:xx:xx:xx,prio[,steerflag]".
 *
 *	At least the mac address of the station to which the settings are to be applied needs
 *	to be present, with one or more setting values, in a specific order. New settings must
 *	be added at the end. Alternatively, we could allow "prio=<value>,steerflag=<value>" and
 *	so on, but that would take some more parsing and use more nvram space.
 *
 * Outputs:
 *	Driver settings may be updated.
 *
 * Returns:
 *	 This function returns a BCME_xxx status indicating success (BCME_OK) or failure.
 *
 * Side effects: The input buffer is trashed by the strsep() function.
 *
 */
static int
wlconf_process_sta_config_entry(char *ifname, char *param_list)
{
	enum {	/* Parameter index in comma-separated list of settings. */
		PARAM_MACADDRESS = 0,
		PARAM_PRIO	 = 1,
		PARAM_STEERFLAG	 = 2,
		PARAM_COUNT
	} param_idx = PARAM_MACADDRESS;
	char *param;
	struct ether_addr ea;
	char *end;
	uint32 value;

	while ((param = strsep(&param_list, ","))) {
		switch (param_idx) {

		case PARAM_MACADDRESS: /* MAC Address - parse into ea */
			if (!param || !ether_atoe(param, &ea.octet[0])) {
				return BCME_BADADDR;
			}
			break;

		case PARAM_PRIO: /* prio value - parse and apply through "staprio" iovar */
			if (*param) { /* If no value is provided, do not configure the prio */
				wl_staprio_cfg_t staprio_arg;
				int ret;

				value = strtol(param, &end, 0);
				if (*end != '\0') {
					return BCME_BADARG;
				}
				memset(&staprio_arg, 0, sizeof(staprio_arg));
				memcpy(&staprio_arg.ea, &ea, sizeof(ea));
				staprio_arg.prio = value; /* prio is byte sized, no htod() needed */
				WL_IOVAR_SET(ifname, "staprio", &staprio_arg, sizeof(staprio_arg));
			}
			break;

		case PARAM_STEERFLAG:
			if (*param) {
				value = strtol(param, &end, 0);
				if (*end != '\0') {
					return BCME_BADARG;
				}
			}
			break;

		default:
			/* Future use parameter already set in nvram config - ignore. */
			break;
		}
		++param_idx;
	}

	if (param_idx <= PARAM_PRIO) { /* No mac address and/or no parameters at all, forget it. */
		return BCME_BADARG;
	}

	return BCME_OK;
}

#define VIFNAME_LEN 16

static void
wlconf_set_taf(char *name, bool enable)
{
#if defined(__CONFIG_TOAD__)
	static const char toad_ifnames[] = "toad_ifnames";
#endif // endif
	char iobuf[sizeof( "taf" )		/* Need some space for the iovar itself, + nul */
		+ sizeof(wl_taf_define_t)
		+ sizeof("enable")		/* "enable" string + terminating nul byte */
		+ sizeof("0")			/* "0" or "1" string + terminating nul byte */
		+ sizeof("")];			/* Final terminating nul byte */

	wl_taf_define_t *taf_def;
	char *ptr;
	int ret;				/* Variable name "ret" is required by WL_IOCTL() */

	memset(iobuf, 0, sizeof(iobuf));
	strcpy(iobuf, "taf");

	taf_def = (wl_taf_define_t *)&iobuf[ sizeof("taf") ];
	memset(&taf_def->ea, 0xff, sizeof(taf_def->ea));

	taf_def->version = 1;

	ptr = taf_def->text;
	ptr += sprintf(ptr, "enable");	/* Set up the command name argument */
	*ptr++ = '\0';			/* add the NUL separator */
	*ptr++ = (enable) ? '1' : '0';	/* enable or disable taf */
	*ptr++ = '\0';			/* add the NUL separator */
	*ptr = '\0';			/* and another NUL to terminate the argument list */

	WL_IOCTL(name, WLC_SET_VAR, iobuf, sizeof(iobuf));

#if defined(__CONFIG_TOAD__)

	/* Update the "toad_ifnames" nvram variable by adding or removing the interface name */

	ptr = nvram_get(toad_ifnames);

	if (!ptr && enable) {		/* Creating nvram with the first ifname */

		nvram_set(toad_ifnames, name);

	} else if (ptr && enable) {	/* Possibly need to add a new ifname */

		if (!strstr(ptr, name)) {
			char *tmp;

			tmp = malloc(strlen(ptr) + sizeof(" ") + strlen(name));

			if (tmp) {
				sprintf(tmp, "%s %s", ptr, name);
				nvram_set(toad_ifnames, tmp);
				free(tmp);
			}
		}

	} else if (ptr && !enable) {	/* Possibly need to remove an existing ifname */

		if (strstr(ptr, name)) {
			int tlen;

			tlen = strlen(ptr) - strlen(name);
			if (tlen == 0) {
				nvram_unset(toad_ifnames);
			} else {
				char *tmp;
				tmp = malloc(tlen + sizeof(""));
				if (tmp) {
					char ifname[VIFNAME_LEN];
					char *next;
					char *cp;

					memset(tmp, 0, tlen + sizeof(""));
					cp = tmp;

					foreach(ifname, ptr, next) {
						if (strcmp(ifname, name) != 0) {
							cp += sprintf(cp, "%s%s",
								(cp == tmp) ? "":" ", name);
						}
					}
					nvram_set(toad_ifnames, tmp);
					free(tmp);
				}
			}
		}
	} /* otherwise this must be (!ptr && !enable), no action needed here. */

#endif /* __CONFIG_TOAD__ */

}

/* If mode set to PSTA/PSR
 * unset all the mbss interfaces
 * to create the virtual intefaces in sequence
 */
static void
wlconf_mbss_unset(char* ifname, char* prefix)
{
	char var[80];
	char tmp[100], buf[100];
	char *next;
	int bsscfg_idx = 1, ret; /* leave primary bsscfg_idx = 0 */

	foreach(var, nvram_safe_get(strcat_r(prefix, "vifs", tmp)), next) {
		WL_BSSIOVAR_SET(var, "interface_remove", bsscfg_idx, NULL, 0);
		if (ret) {
			WLCONF_DBG("(%s): WL_BSSIOVAR_SET(interface_remove, %d)"
				"failed\n", var, bsscfg_idx);
		}
		nvram_unset(strcat_r(var, "_bss_enabled", buf));
		nvram_unset(strcat_r(var, "_ifname", buf));
		bsscfg_idx++;
	}
	nvram_unset(tmp);
}

#ifdef __CONFIG_RSDB__
static int
wlconf_setup_rsdb(char *name)
{
	int ret = 0;
	wl_config_t conf;

	/* If the chip is not RSDB capable, do not do anything */
	if (wl_wlif_get_chip_cap(name, "rsdb") != TRUE)
		return BCME_OK;

	/* get the current rsdb mode from nvram and set it */
	conf.config = wl_wlif_get_rsdb_mode();
	if (conf.config == WLIF_RSDB_MODE_AUTO) {
		conf.config = WLIF_RSDB_MODE_RSDB;
	}
	conf.status = 0;
	ret = wl_iovar_set(name, "rsdb_mode", &conf, sizeof(conf));

	/* Disable parallel scan */
	WL_IOVAR_SETINT(name, "scan_parallel", 0);

	return ret;
}
#endif /* __CONFIG_RSDB__ */

#define AMPDU_DENSITY_8USEC 6

#ifdef BCM_WBD

/* Read "wbd_ifnames" NVRAM and get actual ifnames */
extern int wbd_read_actual_ifnames(char *wbd_ifnames1, int len1, bool create);

/* Returns 1 If Device is Downstream AP and wbd is enabled in the current BSS */
static int
wlconf_get_wbd_slave_mode(char *ifname)
{
	char ifnames[64], name[IFNAMSIZ], *next = NULL;
	int map_mode = strtol(nvram_safe_get(NVRAM_MAP_MODE), NULL, 0);
	int map_uap = strtol(nvram_safe_get(NVRAM_MAP_UAP), NULL, 0); /* Upstream AP */

	/* If Upstream AP return 0 */
	if (!MAP_IS_DISABLED(map_mode) && map_uap) {
		WLCONF_DBG("MAP mode[%d] Upstream AP[%d]\n", map_mode, map_uap);
		return 0;
	}

	/* Read "wbd_ifnames" NVRAM and get actual ifnames */
	if (wbd_read_actual_ifnames(ifnames, sizeof(ifnames), FALSE) != 0)
		return 0;

	/* Traverse wbd_ifnames for each ifname */
	foreach(name, ifnames, next) {
		/* If the wbd_ifnames matches with the BSS's ifname return 1 */
		if (strncmp(name, ifname, sizeof(name)) == 0) {
			WLCONF_DBG("ifname[%s] found in wbd_ifnames[%s]\n", ifname, ifnames);
			return 1;
		}
	}
	WLCONF_DBG("ifname[%s] not found in wbd_ifnames[%s]\n", ifname, ifnames);

	return 0;
}

/* Check if a STA interface can use specific BSSID to join. If yes return 1 else return 0.
 * Also fill the bssid in the argument
 */
static int
wlconf_wbd_is_bssid_join_possible(char *ifname, char *prefix, struct ether_addr *bssid)
{
	char *nvval;
	char tmp[100];

	/* Get the BSSID */
	nvval = nvram_safe_get(strcat_r(prefix, "bssid", tmp));
	if (!nvval || strlen(nvval) < 17 || !ether_atoe(nvval, bssid)) {
		WLCONF_DBG("ifname[%s] NVRAM[%s=%s]. No valid BSSID\n", ifname, tmp, nvval);
		return 0;
	}

	return 1;
}

/* Join the STA to a specified BSSID */
static void
wlconf_wbd_join_sta(char *ifname, char *prefix, struct ether_addr *bssid)
{
	int ret;
	wl_join_params_t *join_params = NULL;
	uint join_params_size;
	char *nvval;
	char tmp[100];

	/* allocate the storage */
	join_params_size = WL_JOIN_PARAMS_FIXED_SIZE;
	if ((join_params = malloc(join_params_size)) == NULL) {
		WLCONF_DBG("ifname[%s] Error allocating %d bytes for assoc params\n",
			ifname, join_params_size);
		return;
	}

	/* Join SSID with assoc params */
	memset(join_params, 0, join_params_size);

	/* Get SSID from NVRAM */
	strcat_r(prefix, "ssid", tmp);
	nvval = nvram_safe_get(tmp);
	join_params->ssid.SSID_len = strlen(nvval);
	if (join_params->ssid.SSID_len > sizeof(join_params->ssid.SSID)) {
		join_params->ssid.SSID_len = sizeof(join_params->ssid.SSID);
	}
	strncpy((char *)join_params->ssid.SSID, nvval, join_params->ssid.SSID_len);

	memcpy(&join_params->params.bssid, bssid, ETHER_ADDR_LEN);
	join_params->params.bssid_cnt = 0;
	join_params->params.chanspec_num = 0;

	WLCONF_DBG("ifname[%s] Joining SSID: %s BSSID:["MACF"]\n", ifname, join_params->ssid.SSID,
		ETHER_TO_MACF(join_params->params.bssid));

	WL_IOCTL(ifname, WLC_SET_SSID, join_params, join_params_size);

	if (join_params) {
		free(join_params);
	}
}
#endif /* BCM_WBD */

#ifdef WLHOSTFBT

extern void fbt_restore_defaults(char *in_prefix, int max_nvparse);

/* WBD related NVRAMs which are required for FBT */
#define NVRAM_WBD_FBT		"wbd_fbt"

/* FBT related NVRAMs */
#define NVRAM_FBT_ENABLED	"fbt"
#define NVRAM_FBT_MDID		"fbt_mdid"
#define NVRAM_FBT_OVERDS	"fbtoverds"
#define NVRAM_FBT_REASSOC_TIME	"fbt_reassoc_time"
#define NVRAM_FBT_AP		"fbt_ap"
#define NVRAM_FBT_R0KH_ID	"r0kh_id"
#define NVRAM_FBT_R1KH_ID	"r1kh_id"

/* FBT Related IOVAR Names */
#define WLCONF_IOVAR_FBT		"fbt"
#define WLCONF_IOVAR_FBT_MDID		"fbt_mdid"
#define WLCONF_IOVAR_FBT_OVERDS		"fbtoverds"
#define WLCONF_IOVAR_FBT_R0KHID		"fbt_r0kh_id"
#define WLCONF_IOVAR_FBT_R1KHID		"fbt_r1kh_id"
#define WLCONF_IOVAR_FBT_REASSOC_TIME	"fbt_reassoc_time"
#define WLCONF_IOVAR_FBT_AP		"fbt_ap"

/* Configure the FBT based on NVRAMs */
static int
wlconf_set_fbt(struct bsscfg_list *bclist)
{
	int i, nvval, ret, map_mode = 0, wbd_fbt = 0, fbt = 0;
	struct bsscfg_info *bsscfg = NULL;
	char tmp[100];
	char *strnvval = NULL;
	struct ether_addr ea;

	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];

		/* FBT should be enabled only if the akm contains psk2ft type */
		strnvval = nvram_safe_get(strcat_r(bsscfg->prefix, "akm", tmp));
		if (find_in_list(strnvval, "psk2ft") == NULL) {
			WLCONF_DBG("i[%d] name[%s] prefix[%s] %sakm[%s]. akm is not psk2ft\n",
				i, bsscfg->ifname, bsscfg->prefix, bsscfg->prefix, strnvval);
			continue;
		}
#ifdef MULTIAP
		/* Get multiap_mode, wbd_fbt and FBT NVRAM */
		map_mode = (int)strtol(nvram_safe_get(NVRAM_MAP_MODE), NULL, 0);
#endif	/* MULTIAP */
		wbd_fbt = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_WBD_FBT, tmp)));
		fbt = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_ENABLED, tmp)));

		WLCONF_DBG("i[%d] name[%s] prefix[%s] map_mode[%d] wbd_fbt[%d] fbt[%d]\n",
			i, bsscfg->ifname, bsscfg->prefix, map_mode, wbd_fbt, fbt);

		/* If fbt is not enabled or wbd_mode is not enabled, then FBT is disabled */
		if (fbt <= 0) {
			/* If WBD is disabled, then check whether FBT is enabled by WBD or not */
			if (map_mode <= 0) {
				/* If wbd_fbt is 1 means FBT is enabled from WBD so, set it to 0 */
				if (wbd_fbt == 1) {
					nvram_set(strcat_r(bsscfg->prefix, NVRAM_WBD_FBT, tmp),
						0);
					/* Restore all the FBT NVRAMs */
					fbt_restore_defaults(bsscfg->prefix, MAX_NVPARSE);
					nvram_commit();
				}
				WLCONF_DBG("i[%d] name[%s] prefix[%s] FBT Disabled\n",
					i, bsscfg->ifname, bsscfg->prefix);
			} else if (wbd_fbt == 0) {
				WLCONF_DBG("i[%d] name[%s] prefix[%s] FBT Disabled\n",
					i, bsscfg->ifname, bsscfg->prefix);
			} else {
				/* FBT is enabled from WBD */
				fbt = 1;
			}
		}

		/* Set FBT wl command */
		WL_IOVAR_SETINT(bsscfg->ifname, WLCONF_IOVAR_FBT, fbt);
		if (ret != 0) {
			WLCONF_DBG("i[%d] name[%s] prefix[%s] FBT Set Failed. Skipping\n",
				i, bsscfg->ifname, bsscfg->prefix);
			continue;
		}

		/* If FBT is disabled continue */
		if (fbt <= 0) {
			continue;
		}

		/* Read fbtoverds NVRAM and set wl */
		nvval = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_OVERDS, tmp)));
		WL_IOVAR_SETINT(bsscfg->ifname, WLCONF_IOVAR_FBT_OVERDS, nvval);

		/* Read MDID NVRAM and set wl */
		nvval = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_MDID, tmp)));
		WL_IOVAR_SETINT(bsscfg->ifname, WLCONF_IOVAR_FBT_MDID, nvval);

		/* Read r0khid NVRAM and set wl */
		strnvval = nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_R0KH_ID, tmp));
		if (strlen(strnvval)) {
			WL_IOVAR_SET(bsscfg->ifname, WLCONF_IOVAR_FBT_R0KHID, strnvval,
				strlen(strnvval));
		}

		/* Set R1KHID as BSSID */
		strnvval = nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_R1KH_ID, tmp));
		if (strlen(strnvval)) {
			ether_atoe(strnvval, &ea.octet[0]);
			WL_IOVAR_SET(bsscfg->ifname, WLCONF_IOVAR_FBT_R1KHID, &ea, sizeof(ea));
		}

		/* Read fbt reassoc time NVRAM and set wl */
		nvval = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_REASSOC_TIME, tmp)));
		WL_IOVAR_SETINT(bsscfg->ifname, WLCONF_IOVAR_FBT_REASSOC_TIME, nvval);

		/* Read fbt ap NVRAM and set wl */
		nvval = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, NVRAM_FBT_AP, tmp)));
		WL_IOVAR_SETINT(bsscfg->ifname, WLCONF_IOVAR_FBT_AP, nvval);
	}

	return 1;
}
#endif /* WLHOSTFBT */

/* The following defines are copied from wlc_txbf.h */
#define TXBF_SU_BFR_CAP		0x01
#define TXBF_MU_BFR_CAP		0x02
#define TXBF_HE_SU_BFR_CAP	0x04
#define TXBF_HE_MU_BFR_CAP	0x08
#define TXBF_SU_MU_BFR_CAP	(TXBF_SU_BFR_CAP | TXBF_MU_BFR_CAP) /* 0x03 */
#define TXBF_HE_SU_MU_BFR_CAP	(TXBF_HE_SU_BFR_CAP | TXBF_HE_MU_BFR_CAP) /* 0x0c */

#define TXBF_SU_BFE_CAP		0x01
#define TXBF_MU_BFE_CAP		0x02
#define TXBF_HE_SU_BFE_CAP	0x04
#define TXBF_HE_MU_BFE_CAP	0x08
#define TXBF_SU_MU_BFE_CAP	(TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP) /* 0x03 */
#define TXBF_HE_SU_MU_BFE_CAP	(TXBF_HE_SU_BFE_CAP | TXBF_HE_MU_BFE_CAP) /* 0x0c */

/* The following define is copied from wlc_pub.h */
#define MU_FEATURES_OFF		0
#define MU_FEATURES_MUTX	(1 << 0)

static int
wlconf_overwrite_txbf_cap_nvram(char *name, char *prefix)
{
	int ret;
	wlc_rev_info_t rev;
	char tmp[100];
	char val[80];

	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));

	if (ret)
		return ret;

	if (BCM6710_CHIP(rev.chipnum)) {
		/* XXX temporarily overwrite nvram: txbf_bfr_cap, txbf_bfe_cap, and mu_features
		 * for 6710 early BU because 6710 VASIP F/W is not brought up yet (note it's
		 * needed for MU-MIMO but is not needed for SU TXBF).
		 */
		snprintf(val, sizeof(val), "%d", TXBF_SU_BFR_CAP | TXBF_HE_SU_BFR_CAP);
		nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), val);

		snprintf(val, sizeof(val), "%d", TXBF_SU_BFE_CAP | TXBF_HE_SU_BFE_CAP);
		nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), val);

		snprintf(val, sizeof(val), "%d", MU_FEATURES_OFF);
		nvram_set(strcat_r(prefix, "mu_features", tmp), val);
	} else {
		/* Overwrite nvram: txbf_bfr_cap and txbf_bfe_cap for 11ax device */
		snprintf(val, sizeof(val), "%d", TXBF_SU_MU_BFR_CAP | TXBF_HE_SU_MU_BFR_CAP);
		nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), val);

		snprintf(val, sizeof(val), "%d", TXBF_SU_MU_BFE_CAP | TXBF_HE_SU_MU_BFE_CAP);
		nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), val);

		if (EMBEDDED_2x2AX_CORE(rev.chipnum)) {
			/* Disable MUTX by default */
			snprintf(val, sizeof(val), "%d", MU_FEATURES_OFF);
		} else {
			/* Enable MUTX by default iso AUTO (=switch SU/MU) for other 11ax devices */
			snprintf(val, sizeof(val), "%d", MU_FEATURES_MUTX);
		}
		nvram_set(strcat_r(prefix, "mu_features", tmp), val);
	}

	return ret;
}

/* This function is used to configure bw_switch_160
 * It returns TRUE only if either configured fixed chanspec is 160Mhz
 * Or configured BWcap is 160Mhz. Returns FALSE otherwise.
 */
static bool
#if 1
wlconf_is_160(uint8 bw_cap, uint8 bw)
{
	if (bw_cap == WLC_BW_CAP_160MHZ && bw == 0)
		return TRUE;
	else
		return FALSE;
}
#else
wlconf_is_160(chanspec_t chanspec, uint8 bw_cap)
{
	if (chanspec) { /* Fixed channel case */
		return CHSPEC_IS160(chanspec);
	} else { /* Auto channel case */
		return (bw_cap == WLC_BW_CAP_160MHZ);
	}
	return FALSE;
}
#endif

/* This function is used to configure amsdu_aggsf
 * It returns TRUE only if either configured fixed chanspec is greater than 80Mhz
 * Or configured BWcap is greater than 80Mhz. Returns FALSE otherwise.
 */
static bool
wlconf_ge_bw80(chanspec_t chanspec, uint8 bw_cap)
{
	if (chanspec) { /* Fixed channel case */
		return (CHSPEC_IS160(chanspec) ||
			CHSPEC_IS80(chanspec));
	} else { /* Auto channel case */
		return ((bw_cap == WLC_BW_CAP_160MHZ) ||
			(bw_cap == WLC_BW_CAP_80MHZ));
	}
}

/* configure the specified wireless interface */
int
wlconf(char *name)
{
	int restore_defaults, val, unit, phytype, bandtype, gmode = 0, ret = 0;
	int bcmerr;
	int error_bg, error_a;
	struct bsscfg_list *bclist = NULL;
	struct bsscfg_info *bsscfg = NULL;
	char tmp[100], tmp1[100], tmp2[100], prefix[PREFIX_LEN];
	char var[80], *next, *str, *str1, *addr = NULL;
	/* Pay attention to buffer length requirements when using this */
	char buf[WLC_IOCTL_SMLEN*2] __attribute__ ((aligned(4)));
	char *country;
	char *country_rev;
	wlc_rev_info_t rev;
	channel_info_t ci;
	struct maclist *maclist;
	struct ether_addr *ea;
	wlc_ssid_t ssid;
	wl_rateset_t rs;
	unsigned int i;
	char eaddr[32];
	int ap, apsta, wds, sta = 0, wet = 0, mac_spoof = 0, wmf = 0, dwds = 0, monitor = 0;
	int rxchain_pwrsave = 0, radio_pwrsave = 0, map = 0, map_mode = 0;
	wl_country_t country_spec = {{0}, 0, {0}};
	char *ba;
	char *preauth;
	int set_preauth;
	int wlunit = -1;
	int wlsubunit = -1;
	int wl_ap_build = 0; /* wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_MEDLEN];
	int btc_mode;
	uint32 leddc;
	uint nbw = WL_CHANSPEC_BW_20;
	int nmode = OFF; /* 802.11n support */
	char vif_addr[WLC_IOCTL_SMLEN];
	int max_no_vifs = 0;
	int wme_global;
	int max_assoc = -1;
	bool ure_enab = FALSE;
	bool radar_enab = FALSE;
	bool obss_coex = FALSE, psta, psr, dpsta;
	uint8 nvr_bw_cap = WLC_BW_CAP_20MHZ;
	chanspec_t chanspec = 0, nvr_chanspec = 0;
	int wet_tunnel_cap = 0, wet_tunnel_enable = 0;
	bool cap_160 = FALSE, cap_dyn160 = FALSE;
	brcm_prop_ie_t brcm_syscap_ie;
	int prev_max_no_vifs;
#ifdef __CONFIG_DHDAP__
	int is_dhd;
	int cfg_max_assoc = -1;
#endif // endif
	bool ure_mbss = FALSE, mbss = FALSE, mbss_ign_mac_valid = FALSE;
	bool cap_bgdfs160 = FALSE;
	bool cap_11ax = FALSE;
#ifdef MULTIAP
	int map_reonboard = atoi(nvram_safe_get("map_reonboard"));
#endif // endif

	/* wlconf doesn't work for virtual i/f, so if we are given a
	 * virtual i/f return 0 if that interface is in it's parent's "vifs"
	 * list otherwise return -1
	 */
	if (get_ifname_unit(name, &wlunit, &wlsubunit) == 0) {
		if (wlsubunit >= 0) {
			/* we have been given a virtual i/f,
			 * is it in it's parent i/f's virtual i/f list?
			 */
			sprintf(tmp, "wl%d_vifs", wlunit);

			if (strstr(nvram_safe_get(tmp), name) == NULL)
				return -1; /* config error */
			else
				return 0; /* okay */
		}
	} else {
		return -1;
	}

	/* clean up tmp */
	memset(tmp, 0, sizeof(tmp));

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

#ifdef __CONFIG_DHDAP__
	/* Check if interface uses dhd adapter */
	is_dhd = !dhd_probe(name);
#endif /* __CONFIG_DHDAP__ */

	/* Bring up the interface temporarily before issuing iovars. */
	/* This will ensure all the cores are fully initialized */
	WL_IOCTL(name, WLC_UP, NULL, 0);

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		} else if (!strcmp(cap, "mbss16"))
			max_no_vifs = 16;
		else if (!strcmp(cap, "mbss8"))
			max_no_vifs = 8;
		else if (!strcmp(cap, "mbss4"))
			max_no_vifs = 4;
		else if (!strcmp(cap, "wmf"))
			wmf = 1;
		else if (!strcmp(cap, "rxchain_pwrsave"))
			rxchain_pwrsave = 1;
		else if (!strcmp(cap, "radio_pwrsave"))
			radio_pwrsave = 1;
		else if (!strcmp(cap, "wet_tunnel"))
			wet_tunnel_cap = 1;
		else if (!strcmp(cap, "160"))
			cap_160 = TRUE;
		else if (!strcmp(cap, "dyn160"))
			cap_dyn160 = TRUE;
		else if (!strcmp(cap, "bgdfs160"))
			cap_bgdfs160 = 1;
		else if (!strcmp(cap, "11ax"))
			cap_11ax = 1;
	}

#if defined(BCA_CPEROUTER)
	if (max_no_vifs > WL_MAX_NUM_SSID)
		max_no_vifs =  WL_MAX_NUM_SSID;
#endif // endif

	/* Get MAC address */
	(void) wl_hwaddr(name, (uchar *)buf);
	memcpy(vif_addr, buf, ETHER_ADDR_LEN);

	/* Get instance */
	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

#ifdef MULTIAP
	if (map_reonboard) {
		nvram_unset("map_onboarded");
		nvram_unset("wbd_ifnames");
		nvram_unset("bsd_ifnames");
		nvram_set(strcat_r(prefix, "map", tmp), "4");

		/* Reset back SSID and security credentials to default to ensure
		 * this STA interface does not get associated to the root AP.
		 */
		nvram_set(strcat_r(prefix, "ssid", tmp), "Broadcom");
		nvram_unset(strcat_r(prefix, "akm", tmp));
		nvram_unset(strcat_r(prefix, "wpa_psk", tmp));
		nvram_unset(strcat_r(prefix, "crypto", tmp));
		nvram_unset(strcat_r(prefix, "wbd_fbt", tmp));
	}
#endif // endif

	/* Restore defaults if per-interface parameters do not exist */
//	restore_defaults = !nvram_get(strcat_r(prefix, "ifname", tmp));
	restore_defaults = !strlen(nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
//	nvram_validate_all(prefix, restore_defaults);
	nvram_set(strcat_r(prefix, "ifname", tmp), name);
	nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa((uchar *)buf, eaddr));
	snprintf(buf, sizeof(buf), "%d", unit);
	nvram_set(strcat_r(prefix, "unit", tmp), buf);

	/* "_default_restored_" nvram is used by NON-HNDROUTER platforms. When The first system
	 * boot up from default setting, this nvram is not exist and the return value
	 * would be NULL pointer. Then the necessary nvram adjust would applied here.
	 * The special nvram is set and given value by other configuration system.
	 */
#ifdef BCA_CPEROUTER
	if (restore_defaults || !nvram_get("restore_defaults"))
#else
	if (restore_defaults)
#endif // endif
	{
		/* all necessary nvram adjustment beyond on defaults will be here */
		if (cap_11ax) {
			wlconf_overwrite_txbf_cap_nvram(name, prefix);
		}
		wlconf_set_current_txparam_into_nvram(name, prefix);

		/* To prevent that power-cycled causes the corresponding nvrams lost */
		nvram_commit();
	}
#ifdef BCMDBG
	/* Apply message level */
	if (nvram_invmatch("wl_msglevel", "")) {
		val = (int)strtoul(nvram_get("wl_msglevel"), NULL, 0);

		if (nvram_invmatch("wl_msglevel2", "")) {
			struct wl_msglevel2 msglevel64;
			msglevel64.low = val;
			val = (int)strtoul(nvram_get("wl_msglevel2"), NULL, 0);
			msglevel64.high = val;
			WL_IOVAR_SET(name, "msglevel", &msglevel64, sizeof(struct wl_msglevel2));
		}
		else
			WL_IOCTL(name, WLC_SET_MSGLEVEL, &val, sizeof(val));
	}

	/* Apply assert_type */
	str = nvram_safe_get("assert_type");
	if (strcmp(str, "")) {
		WL_IOVAR_SETINT(name, "assert_type", (int)strtoul(str, NULL, 0));
	}
#endif /* BCMDBG */

	dpsta = nvram_invmatch("dpsta_ifnames", "");

	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	str1 = nvram_safe_get(strcat_r(prefix, "prev_mode", tmp));
	if (!strcmp(str1, "")) {
		nvram_set(strcat_r(prefix, "prev_mode", tmp), str);
	}

	/* If mode set to PSTA/PSR
	 * unset all the MBSS interfaces
	 * to create virtual interfaces in sequence
	 */
	if (!strcmp(str, "psta") || !strcmp(str, "psr")) {
		wlconf_mbss_unset(name, prefix);
	}

	/* Bring the interface down */
	WL_IOCTL(name, WLC_DOWN, NULL, 0);

#ifdef __CONFIG_RSDB__
	/* Do rsdb specific setups for relevant chips */
	wlconf_setup_rsdb(name);
#endif /* __CONFIG_RSDB__ */

	/* Disable all BSS Configs */
	for (i = 0; i < WL_MAXBSSCFG; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;
		setbuf.bsscfg_idx = i;
		setbuf.enable = 0;

		ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
		if (ret) {
			(void) wl_iovar_getint(name, "bcmerror", &bcmerr);
			/* fail quietly on a range error since the driver may
			 * support fewer bsscfgs than we are prepared to configure
			 */
			if (bcmerr == BCME_RANGE)
				break;
		}
		if (ret) {
			WLCONF_DBG("%d:(%s): setting bsscfg #%d iovar \"bss\" to 0"
			           " (down) failed, ret = %d, bcmerr = %d\n",
			           __LINE__, name, i, ret, bcmerr);
		}
	}

	/* Get the list of BSS Configs */
	bclist = wlconf_get_bsscfgs(name, prefix);
	if (bclist == NULL) {
		ret = -1;
		goto exit;
	}

#ifdef BCMDBG
	strcat_r(prefix, "vifs", tmp);
	printf("BSS Config summary: primary -> \"%s\", %s -> \"%s\"\n", name, tmp,
	       nvram_safe_get(tmp));
	for (i = 0; i < bclist->count; i++) {
		printf("BSS Config \"%s\": index %d\n",
		       bclist->bsscfgs[i].ifname, bclist->bsscfgs[i].idx);
	}
#endif // endif

	/* create a wlX.Y_ifname nvram setting */
	for (i = 1; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
#if defined(linux) || defined(__NetBSD__)
		strcpy(var, bsscfg->ifname);
#endif // endif
		nvram_set(strcat_r(bsscfg->prefix, "ifname", tmp), var);
	}

	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));

	/* If ure_disable is not present or is 1, ure is not enabled;
	 * that is, if it is present and 0, ure is enabled.
	 */
	if (!strcmp(nvram_safe_get("ure_disable"), "0")) { /* URE is enabled */
		ure_enab = TRUE;
		if (!strcmp(nvram_safe_get(strcat_r(prefix, "ure_mbss", tmp)), "1")) {
			/* Multiple URE BSS is enabled */
			ure_mbss = TRUE;
		}
	}

	/* must config this value before we enable MBSS */
	if (dpsta || !strcmp(nvram_safe_get(strcat_r(prefix, "mbss_ign_mac_valid",
		tmp)), "1")) {
		WL_IOVAR_SETINT(name, "mbss_ign_mac_valid", 1);
		mbss_ign_mac_valid = TRUE;
	}

	/* If MBSS8 to MBSS16 (DHD to NIC)
	 * unset all the MBSS interfaces
	 * to create virtual interfaces in sequence
	 */
	prev_max_no_vifs = atoi(nvram_safe_get(strcat_r(prefix, "prev_mbss", tmp)));
	str1 = nvram_safe_get(strcat_r(prefix, "prev_mbss", tmp));

	if (prev_max_no_vifs || !(strcmp(str1, ""))) {
		snprintf(var, sizeof(var), "%d", max_no_vifs);
		nvram_set(tmp, var);
	}
	if (prev_max_no_vifs < max_no_vifs) {
		for (i = 1; i < max_no_vifs; i++) {
			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, i);
				nvram_unset(tmp);
		}
	} else if (prev_max_no_vifs > max_no_vifs) {
		for (i = max_no_vifs; i < prev_max_no_vifs; i++) {
			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, i);
				nvram_unset(tmp);
		}
	}
	if (wl_ap_build) {
		/* Enable MBSS mode if appropriate. */
		if ((ure_enab && !ure_mbss) || !strcmp(str, "psr")) {
			mbss = FALSE;
			WL_IOVAR_SETINT(name, "mbss", 0);
		} else {
			mbss = TRUE;
#ifndef __CONFIG_USBAP__
			WL_IOVAR_SETINT(name, "mbss", (bclist->count >= 1));
#else
			WL_IOVAR_SETINT(name, "mbss", (bclist->count >= 2));
#endif /* __CONFIG_USBAP__ */
		}
	}

	/* Create addresses for VIFs */
	if (mbss) {

#ifdef BCA_CPEROUTER
		vif_addr[0] = 96 + (vif_addr[5]%(max_no_vifs-1) * 8);
#endif // endif
		/* set local bit for our MBSS vif base */
		ETHER_SET_LOCALADDR(vif_addr);

		/* construct and set other wlX.Y_hwaddr */
		for (i = 1; i < max_no_vifs; i++) {
			/* if dpsta w/ mbss, wlX and wlX.1 must use the same hwaddr
			 * otherwise dpsta will fail to recevie EAPOL from upstream AP.
			 */
			if (i == DPSTA_PRIMARY_AP_IDX && dpsta) {
				snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit,
					DPSTA_PRIMARY_AP_IDX);
				addr = nvram_safe_get(strcat_r(prefix, "hwaddr", tmp2));
				printf("%s: overwrite dpsta wl%d.%d_hwaddr to %s\n",
					__FUNCTION__, unit, DPSTA_PRIMARY_AP_IDX, addr);
				nvram_set(tmp, addr);
				continue;
			}

			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, i);
			addr = nvram_safe_get(tmp);
			if ((ure_mbss && !mbss_ign_mac_valid) || !strcmp(addr, "")) {
				vif_addr[5] = (vif_addr[5] & ~(max_no_vifs-1))
				        | ((max_no_vifs-1) & (vif_addr[5]+1));

				nvram_set(tmp, ether_etoa((uchar *)vif_addr, eaddr));
			}
		}

		/* PR100961, set the cur_etheraddr for each MBSS in vifs no matter bss is enabled
		 * or not, otherwise the cur_etheraddr of primary bsscfg will be corrupted by
		 * wlc_bsscfg_macgen in mbss_bsscfg_up
		 */
		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
			/* Ignore primary */
			if (bsscfg->idx == 0)
				continue;

			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, bsscfg->idx);
			ether_atoe(nvram_safe_get(tmp), (unsigned char *)eaddr);
#ifndef CONFIG_HOSTAPD
			WL_BSSIOVAR_SET(name, "cur_etheraddr", bsscfg->idx, eaddr, ETHER_ADDR_LEN);
#endif /* CONFIG_HOSTAPD */
		}
	} else { /* One of URE or Proxy STA Repeater is enabled */
		/* URE/PSR is on, so set wlX.1 hwaddr is same as that of primary interface */
		snprintf(tmp, sizeof(tmp), "wl%d.1_hwaddr", unit);
		nvram_set(tmp, ether_etoa((uchar *)vif_addr, eaddr));
#ifndef CONFIG_HOSTAPD
		WL_BSSIOVAR_SET(name, "cur_etheraddr", 1, vif_addr,
		                ETHER_ADDR_LEN);
#endif /* CONFIG_HOSTAPD */
	}

	if (wl_ap_build) {
		/*
		 * Set SSID for each BSS Config
		 */
		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
#ifdef CONFIG_HOSTAPD
			/* Use iw cmd only for virtual interface.
			 * Non zero value of i confirms the bss being vifs.
			 */
			if (!nvram_match("hapd_enable", "0") && i) {
				strcat_r(bsscfg->prefix, "hwaddr", tmp1);
				WLCONF_DBG("Create interface %s using iw\n", bsscfg->ifname);
				str = nvram_get(strcat_r(bsscfg->prefix, "mode", var));
				snprintf(tmp, sizeof(tmp), "iw dev %s interface"
						" add %s type __%s addr %s", name, bsscfg->ifname,
						str ? str : "ap", /* Default mode:ap */
						nvram_safe_get(tmp1));
				WLCONF_DBG("iw command: %s\n", tmp);
				system(tmp);
			}
#endif /* CONFIG_HOSTAPD */
			strcat_r(bsscfg->prefix, "ssid", tmp);
			ssid.SSID_len = strlen(nvram_safe_get(tmp));
			if (ssid.SSID_len > sizeof(ssid.SSID))
				ssid.SSID_len = sizeof(ssid.SSID);
			strncpy((char *)ssid.SSID, nvram_safe_get(tmp), ssid.SSID_len);
			WLCONF_DBG("wlconfig(%s): configuring bsscfg #%d (%s) "
			           "with SSID \"%s\"\n", name, bsscfg->idx,
			           bsscfg->ifname, nvram_safe_get(tmp));
			/* enable guest BSS sta configs */
			if (ure_mbss) {
				if (IS_URE_GUEST_STA(bsscfg->idx, bsscfg->ifname))  {
					struct {int bsscfg_idx; int enable;} setbuf;
					setbuf.bsscfg_idx = bsscfg->idx;
					setbuf.enable = WLC_AP_IOV_OP_MANUAL_STA_BSSCFG_CREATE;

					/* Allocate guest sta BSS first */
					printf("%s try to create wet guest sta bsscfg \n",
						bsscfg->ifname);
					WL_IOVAR_SET(name, "bss", &setbuf, sizeof(setbuf));
				}
			}
			WL_BSSIOVAR_SET(name, "ssid", bsscfg->idx, &ssid,
			                sizeof(ssid));
		}
	}

	/* wlX_mode settings: AP, STA, WET, BSS/IBSS, APSTA */
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	ap = (!strcmp(str, "") || !strcmp(str, "ap"));
	apsta = (!strcmp(str, "apsta") ||
	         ((!strcmp(str, "sta") || !strcmp(str, "psr") || !strcmp(str, "wet")) &&
	          bclist->count > 1));
	sta = (!strcmp(str, "sta") && bclist->count == 1);
	wds = !strcmp(str, "wds");
	wet = !strcmp(str, "wet");
	mac_spoof = !strcmp(str, "mac_spoof");
	psta = !strcmp(str, "psta");
	psr = !strcmp(str, "psr");
	monitor = !strcmp(str, "monitor");
	dwds = atoi(nvram_safe_get(strcat_r(prefix, "dwds", tmp)));
	map = strtol(nvram_safe_get(strcat_r(prefix, "map", tmp)), NULL, 0);

#ifdef MULTIAP
	map_mode = strtol(nvram_safe_get(NVRAM_MAP_MODE), NULL, 0);

	/* For multiap agents wireless onboarding intial mode is sta to do wps */
	if (map == 4 && strtol(nvram_safe_get("map_onboarded"), NULL, 0) == 0) {
		ap = 0;
		sta = 1;
		nvram_set(strcat_r(prefix, "mode", tmp), "sta");
		nvram_set("wps_on_sta", "1");
	}

	/* If the map_uap (Upstream AP NVRAM) is not set, set it to 1 by default for controller */
	if (nvram_get(NVRAM_MAP_UAP) == NULL) {
		if (map_mode & MAP_MODE_FLAG_CONTROLLER) {
			nvram_set(NVRAM_MAP_UAP, "1");
		}
	}
#endif	/* MULTIAP */

	if (monitor) {
		WL_SETINT(name, WLC_SET_MONITOR, 3);
	} else {
		WL_SETINT(name, WLC_SET_MONITOR, 0);
	}
	/* set apsta var first, because APSTA mode takes precedence */
	WL_IOVAR_SETINT(name, "apsta", apsta);

	/* Set AP mode */
	val = (ap || apsta || wds) ? 1 : 0;
	WL_IOCTL(name, WLC_SET_AP, &val, sizeof(val));

#ifdef __CONFIG_DHDAP__
	if (is_dhd) {
		dhd_iovar_setint(name, "wet", wet);
	} else
#endif // endif
	/* Turn WET mode ON or OFF based on selected mode */
	WL_IOCTL(name, WLC_SET_WET, &wet, sizeof(wet));

	if (mac_spoof) {
		sta = 1;
		WL_IOVAR_SETINT(name, "mac_spoof", 1);
	}
	WL_IOVAR_SETINT(name, "wet_enab", wet);

	/* If we are using hostapd/wpa_supplicant, disable the auto-join from driver */
	if (wet && !nvram_match("hapd_enable", "0")) {
		WL_IOVAR_SETINT(name, "block_as_retry", 2);
	}

	/* For STA configurations, configure association retry time.
	 * Use specified time (capped), or mode-specific defaults.
	 */
	if (sta || wet || apsta || psta || psr) {
		char *sta_retry_time_name = "sta_retry_time";
		char *assoc_retry_max_name = "assoc_retry_max";
		struct {
			int val;
			int band;
		} roam;

		str = nvram_safe_get(strcat_r(prefix, sta_retry_time_name, tmp));
		WL_IOVAR_SETINT(name, sta_retry_time_name, atoi(str));

		/* Set the wlX_assoc_retry_max, but only if one was specified. */
		if ((str = nvram_get(strcat_r(prefix, assoc_retry_max_name, tmp)))) {
			WL_IOVAR_SETINT(name, assoc_retry_max_name, atoi(str));
		}

		roam.val = WLC_ROAM_NEVER_ROAM_TRIGGER;
		roam.band = WLC_BAND_ALL;
		WL_IOCTL(name, WLC_SET_ROAM_TRIGGER, &roam, sizeof(roam));
	}

	/* Retain remaining WET effects only if not APSTA */
	wet &= !apsta;

	/* Set infra: BSS/IBSS (IBSS only for WET or STA modes) */
	val = 1;
	if (wet || sta || psta || psr)
		val = atoi(nvram_safe_get(strcat_r(prefix, "infra", tmp)));
	WL_IOCTL(name, WLC_SET_INFRA, &val, sizeof(val));

	/* Set DWDS only for AP or STA modes */
	for (i = 0; i < bclist->count; i++) {
		int map_val = 0;
		val = 0;
		bsscfg = &bclist->bsscfgs[i];

		if (ap || sta || psta || psr || (apsta && !wet) || wet) {
			strcat_r(bsscfg->prefix, "dwds", tmp);
			val = atoi(nvram_safe_get(tmp));
			strcat_r(bsscfg->prefix, "map", tmp);
			map_val = strtol(nvram_safe_get(tmp), NULL, 0);
		}
		WL_BSSIOVAR_SETINT(name, "dwds", bsscfg->idx, val);
		WL_BSSIOVAR_SETINT(name, "map", bsscfg->idx, map_val);
#ifdef MULTIAP
		/* If multiap is enabled, and it is onboarded, for STA interface enable dwds_lb_filter */
		if ((map_val == 4) && (strtol(nvram_safe_get("map_onboarded"), NULL, 0) == 1) &&
				(map_mode > 0)) {
			WL_BSSIOVAR_SETINT(name, "dwds_lb_filter", bsscfg->idx, 1);
		}
#endif // endif
	}

	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];

		if (sta || psta || psr || apsta || wet) {
			strcat_r(bsscfg->prefix, "split_assoc_resp", tmp);
			val = atoi(nvram_safe_get(tmp));
			WL_BSSIOVAR_SETINT(name, "split_assoc_resp", bsscfg->idx, val);
		}

		if (ap || apsta) {
			strcat_r(bsscfg->prefix, "split_assoc_req", tmp);
			val = atoi(nvram_safe_get(tmp));
#ifdef BCM_WBD
			/* for wifi blanket repeaters, set split_assoc_req if it does not support
			 * multi hop network
			 */
			if (wlconf_get_wbd_slave_mode(bsscfg->ifname) &&
				strtol(nvram_safe_get("map_no_multihop"), NULL, 0)) {
				val = 1;
			}
#endif // endif
			WL_BSSIOVAR_SETINT(name, "split_assoc_req", bsscfg->idx, val);
		}
	}

	/* Set MAXSCB */
	if (ap || apsta) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "maxscb", tmp)));
		if (val > 0) {
			WL_IOVAR_SETINT(name, "maxscb", val);
		}
	}

	/* Set The AP MAX Associations Limit */
	if (ap || apsta) {
#ifdef __CONFIG_DHDAP__
		/* check if we have driver maxassoc tuneable value */
		cfg_max_assoc = atoi(nvram_safe_get(strcat_r(prefix, "cfg_maxassoc", tmp)));
		if (cfg_max_assoc <= 0) {
			WL_IOVAR_GETINT(name, "maxassoc", &cfg_max_assoc);
			/* save to nvram */
			snprintf(var, sizeof(var), "%d", cfg_max_assoc);
			nvram_set(tmp, var);
		}
#endif // endif

		max_assoc = val = atoi(nvram_safe_get(strcat_r(prefix, "maxassoc", tmp)));
		if (val > 0) {
#ifdef __CONFIG_DHDAP__
			/* fix for max_assoc value greater than 32 for DHD */
			if ((val > cfg_max_assoc) && is_dhd) {
			    val = cfg_max_assoc;
			    snprintf(var, sizeof(var), "%d", val);
			    nvram_set(tmp, var);
			}
#endif // endif
			WL_IOVAR_SETINT(name, "maxassoc", val);
		} else { /* Get value from driver if not in nvram */
			WL_IOVAR_GETINT(name, "maxassoc", &max_assoc);
		}
	}
	if (!wet && !sta)
		WL_IOVAR_SETINT(name, "mpc", OFF);

	/* Set the Proxy STA or Repeater mode */
	if (psta) {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_PROXY);
#ifdef __CONFIG_DHDAP__
		if (is_dhd)
			dhd_iovar_setint(name, "psta", PSTA_MODE_PROXY);
#endif /* __CONFIG_DHDAP__ */
		/* Set inactivity timer */
		str = nvram_get(strcat_r(prefix, "psta_inact", tmp));
		if (str) {
			val = atoi(str);
			WL_IOVAR_SETINT(name, "psta_inact", val);
		}
	} else if (psr) {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_REPEATER);
#ifdef __CONFIG_DHDAP__
		if (is_dhd)
			dhd_iovar_setint(name, "psta", PSTA_MODE_REPEATER);
#endif /* __CONFIG_DHDAP__ */
		val = atoi(nvram_safe_get(strcat_r(prefix, "psr_mrpt", tmp)));
		WL_IOVAR_SETINT(name, "psta_mrpt", val);
	} else {
		WL_IOVAR_SETINT(name, "psta", PSTA_MODE_DISABLED);
#ifdef __CONFIG_DHDAP__
		if (is_dhd)
			dhd_iovar_setint(name, "psta", PSTA_MODE_DISABLED);
#endif /* __CONFIG_DHDAP__ */
	}

#ifdef __CONFIG_GMAC3__
	if (psta || psr || wet || ((dwds || map) && (sta || apsta)))
		DHD_BSSIOVAR_SETINT(name, "dev_def", 0, 1);
#endif // endif

#if defined(__CONFIG_VISUALIZATION__) && defined(CONFIG_VISUALIZATION_ENABLED) || defined(RTCONFIG_BCN_RPT)
	str = nvram_safe_get(strcat_r(prefix, "rrm", tmp));
	if (str) {
		val = strtol(str, NULL, 0);
		WL_IOVAR_SETINT(name, "rrm", val);
	}
#endif /* __CONFIG_VISUALIZATION__ && CONFIG_VISUALIZATION_ENABLED */
	/* Turn WET tunnel mode ON or OFF */
	if ((ap || apsta) && (wet_tunnel_cap)) {
		if (atoi(nvram_safe_get(strcat_r(prefix, "wet_tunnel", tmp))) == 1) {
			WL_IOVAR_SETINT(name, "wet_tunnel", 1);
			wet_tunnel_enable = 1;
		} else {
			WL_IOVAR_SETINT(name, "wet_tunnel", 0);
		}
	}

	for (i = 0; i < bclist->count; i++) {
		char *subprefix;
		int wmf_bss_disab = 0;
		bsscfg = &bclist->bsscfgs[i];

		/* XXXMSSID: The note about setting preauth now does not seem right.
		 * NAS brings the BSS up if it runs, so setting the preauth value
		 * will make it in the bcn/prb. If that is right, we can move this
		 * chunk out of wlconf.
		 */
		/*
		 * Set The WPA2 Pre auth cap. only reason we are doing it here is the driver is down
		 * if we do it in the NAS we need to bring down the interface and up to make
		 * it affect in the  beacons
		 */
		if (ap || (apsta && bsscfg->idx != 0)) {
			set_preauth = 1;
			preauth = nvram_safe_get(strcat_r(bsscfg->prefix, "preauth", tmp));
			if (strlen (preauth) != 0) {
				set_preauth = atoi(preauth);
			}
			wlconf_set_preauth(name, bsscfg->idx, set_preauth);
		}

		/* Clear BRCM System level Capability IE */
		memset(&brcm_syscap_ie, 0, sizeof(brcm_prop_ie_t));
		brcm_syscap_ie.type = BRCM_SYSCAP_IE_TYPE;

		/* Add WET TUNNEL to IE */
		if (wet_tunnel_enable)
			brcm_syscap_ie.cap |= BRCM_SYSCAP_WET_TUNNEL;

		subprefix = apsta ? prefix : bsscfg->prefix;

		if (ap || (apsta && bsscfg->idx != 0)) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "bss_maxassoc", tmp)));
			if (val > 0) {
#ifdef __CONFIG_DHDAP__
				/* fix for val greater than 32 for DHD */
				if (is_dhd && (val > cfg_max_assoc)) {
					val = cfg_max_assoc;
					/* rewrite the nvram contents */
					snprintf(var, sizeof(var), "%d", val);
					nvram_set(tmp, var);
				}
#endif // endif
				WL_BSSIOVAR_SETINT(name, "bss_maxassoc", bsscfg->idx, val);
			} else if (max_assoc > 0) { /* Set maxassoc same as global if not set */
				snprintf(var, sizeof(var), "%d", max_assoc);
				nvram_set(tmp, var);
			}

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "wnm_no_btq_resp", tmp)));
			WL_BSSIOVAR_SETINT(name, "wnm_no_btq_resp", bsscfg->idx, val? 1 : 0);
		}

		/* Set network type */
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "closed", tmp)));
		WL_BSSIOVAR_SETINT(name, "closednet", bsscfg->idx, val);

		/* Set the ap isolate mode */
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "ap_isolate", tmp)));
#ifdef __CONFIG_DHDAP__
		/* For DHD-AP ap-isolate has to be 1 for intra-bss to work */
		if (is_dhd) {
			DHD_BSSIOVAR_SETINT(name, "ap_isolate", bsscfg->idx, val);
		} else
#endif /* __CONFIG_DHDAP__ */
		WL_BSSIOVAR_SETINT(name, "ap_isolate", bsscfg->idx, val);

		/* Set the MAC filter based probe response mode */
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "probresp_mf", tmp)));
		WL_BSSIOVAR_SETINT(name, "probresp_mac_filter", bsscfg->idx, val);

		/* WMF is supported only in AP mode */
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "mode", tmp));
		wmf_bss_disab = strcmp(str, "ap");

		/* Set the WMF enable mode */
		if (wmf ||
#ifdef __CONFIG_DHDAP__
			is_dhd ||
#endif // endif
			0) {
#ifdef __CONFIG_DHDAP__
			if (is_dhd) {
				if (wmf_bss_disab) {
					val = 0;
				} else {
					val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
							"wmf_bss_enable", tmp)));
				}
				DHD_BSSIOVAR_SETINT(name, "wmf_bss_enable", bsscfg->idx, val);

				val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
						"wmf_psta_disable", tmp)));
				DHD_BSSIOVAR_SETINT(name, "wmf_psta_disable", bsscfg->idx, val);

			} else {
#endif /* __CONFIG_DHDAP__ */
			if (wmf_bss_disab) {
				val = 0;
			} else {
				val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
						"wmf_bss_enable", tmp)));
			}
			WL_BSSIOVAR_SETINT(name, "wmf_bss_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"wmf_psta_disable", tmp)));
			WL_BSSIOVAR_SETINT(name, "wmf_psta_disable", bsscfg->idx, val);
#ifdef __CONFIG_DHDAP__
			}
#endif /* __CONFIG_DHDAP__ */
		}

		/* Set the Multicast Reverse Translation enable mode */
		if (wet || psta || psr) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
					"mcast_regen_bss_enable", tmp)));
#ifdef __CONFIG_DHDAP__
			if (is_dhd) {
				DHD_BSSIOVAR_SETINT(name, "mcast_regen_bss_enable",
					bsscfg->idx, val);
			} else {
#endif /* __CONFIG_DHDAP__ */
				WL_BSSIOVAR_SETINT(name, "mcast_regen_bss_enable",
					bsscfg->idx, val);
#ifdef __CONFIG_DHDAP__
			}
#endif /* __CONFIG_DHDAP__ */
		}

		if (rxchain_pwrsave) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "rxchain_pwrsave_enable",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"rxchain_pwrsave_quiet_time", tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_quiet_time", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "rxchain_pwrsave_pps",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_pps", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"rxchain_pwrsave_stas_assoc_check", tmp)));
			WL_BSSIOVAR_SETINT(name, "rxchain_pwrsave_stas_assoc_check", bsscfg->idx,
				val);
		}

		if (radio_pwrsave) {
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_enable",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_enable", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"radio_pwrsave_quiet_time", tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_quiet_time", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_pps",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_pps", bsscfg->idx, val);
			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "radio_pwrsave_level",
				tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_level", bsscfg->idx, val);

			val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
				"radio_pwrsave_stas_assoc_check", tmp)));
			WL_BSSIOVAR_SETINT(name, "radio_pwrsave_stas_assoc_check", bsscfg->idx,
				val);
		}
		val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix, "aspm", tmp)));
#ifdef __CONFIG_DHDAP__
		if (is_dhd) {
			dhd_iovar_setint(name, "aspm", val);
		} else
#endif /* __CONFIG_DHDAP__ */
		WL_BSSIOVAR_SETINT(name, "aspm", bsscfg->idx, val);

		/* Configure SYSCAP IE to driver */
		if (brcm_syscap_ie.cap)  {
			wlconf_set_brcm_syscap_ie(name, bsscfg->idx,
				BRCM_PROP_OUI, (uchar *)&(brcm_syscap_ie.type),
				sizeof(brcm_syscap_ie.type) +
				sizeof(brcm_syscap_ie.cap));
		}

#if (defined(__CONFIG_VISUALIZATION__) && defined(CONFIG_VISUALIZATION_ENABLED)) || \
	defined(BCM_WBD) || defined(RTCONFIG_BCN_RPT)
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "rrm", tmp));
		val = 0;
		if (str) {
			val = strtol(str, NULL, 0);
		}
		if (map_mode > 0) {
			val |= (1 << DOT11_RRM_CAP_BCN_PASSIVE) | (1 << DOT11_RRM_CAP_BCN_ACTIVE) |
				(1 << DOT11_RRM_CAP_NEIGHBOR_REPORT);
		}
		WL_BSSIOVAR_SETINT(name, "rrm", bsscfg->idx, val);
#endif /* (__CONFIG_VISUALIZATION__ && CONFIG_VISUALIZATION_ENABLED) || BCM_WBD */

	}

	/* Change LED Duty Cycle */
	leddc = (uint32)strtoul(nvram_safe_get(strcat_r(prefix, "leddc", tmp)), NULL, 16);
	if (leddc)
		WL_IOVAR_SETINT(name, "leddc", leddc);

	/* Enable or disable the radio */
	val = nvram_match(strcat_r(prefix, "radio", tmp), "0");
	val += WL_RADIO_SW_DISABLE << 16;
	WL_IOCTL(name, WLC_SET_RADIO, &val, sizeof(val));

	/* Get supported phy types */
	WL_IOCTL(name, WLC_GET_PHYLIST, var, sizeof(var));
	nvram_set(strcat_r(prefix, "phytypes", tmp), var);

	/* Get radio IDs */
	*(next = buf) = '\0';
	for (i = 0; i < strlen(var); i++) {
		/* Switch to band */
		val = WLCONF_STR2PHYTYPE(var[i]);
		if (WLCONF_PHYTYPE_11N(val)) {
			WL_GETINT(name, WLC_GET_BAND, &val);
		} else
			val = WLCONF_PHYTYPE2BAND(val);
		WL_IOCTL(name, WLC_SET_BAND, &val, sizeof(val));
		/* Get radio ID on this band */
		WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
		next += sprintf(next, "%sBCM%X", i ? " " : "",
		                (rev.radiorev & IDCODE_ID_MASK) >> IDCODE_ID_SHIFT);
	}
	nvram_set(strcat_r(prefix, "radioids", tmp), buf);

	/* Set band */
	str = nvram_get(strcat_r(prefix, "phytype", tmp));
	val = str ? WLCONF_STR2PHYTYPE(str[0]) : PHY_TYPE_G;
	/* For NPHY use band value from NVRAM */
	if (WLCONF_PHYTYPE_11N(val)) {
		str = nvram_get(strcat_r(prefix, "nband", tmp));
		if (str)
			val = atoi(str);
		else {
			WL_GETINT(name, WLC_GET_BAND, &val);
		}
	} else
		val = WLCONF_PHYTYPE2BAND(val);

	WL_SETINT(name, WLC_SET_BAND, val);

	/* Check errors (card may have changed) */
	if (ret) {
		/* default band to the first band in band list */
		val = WLCONF_STR2PHYTYPE(var[0]);
		val = WLCONF_PHYTYPE2BAND(val);
		WL_SETINT(name, WLC_SET_BAND, val);
	}

	/* Store the resolved bandtype */
	bandtype = val;

	/* Check errors again (will cover 5Ghz-only cards) */
	if (ret) {
		int list[3];

		/* default band to the first band in band list */
		wl_ioctl(name, WLC_GET_BANDLIST, list, sizeof(list));
		WL_SETINT(name, WLC_SET_BAND, list[1]);

		/* Read it back, and set bandtype accordingly */
		WL_GETINT(name, WLC_GET_BAND, &bandtype);
	}

	/* Set up the country code */
	(void) strcat_r(prefix, "country_code", tmp);
	country = nvram_get(tmp);
	(void) strcat_r(prefix, "country_rev", tmp2);
	country_rev = nvram_get(tmp2);
	if ((country && country[0] != '\0') && (country_rev && country_rev[0] != '\0')) {
		/* Initialize the wl country parameter */
		strncpy(country_spec.country_abbrev, country, WLC_CNTRY_BUF_SZ-1);
		country_spec.country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		strncpy(country_spec.ccode, country, WLC_CNTRY_BUF_SZ-1);
		country_spec.ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		country_spec.rev = atoi(country_rev);

		WL_IOVAR_SET(name, "country", &country_spec, sizeof(country_spec));
	} else {
		/* Get the default country code if undefined */
		wl_iovar_get(name, "country", &country_spec, sizeof(country_spec));

		/* Add the new NVRAM variable */
		nvram_set("wl_country_code", country_spec.ccode);
		(void) strcat_r(prefix, "country_code", tmp);
		nvram_set(tmp, country_spec.ccode);
		snprintf(buf, sizeof(buf),  "%d", country_spec.rev);
		nvram_set("wl_country_rev", buf);
		(void) strcat_r(prefix, "country_rev", tmp);
		nvram_set(tmp, buf);
	}

	/* Get current core revision */
	WL_IOCTL(name, WLC_GET_REVINFO, &rev, sizeof(rev));
	snprintf(buf, sizeof(buf), "%d", rev.corerev);
	nvram_set(strcat_r(prefix, "corerev", tmp), buf);

	/* XXX PR65742: Advertise mixed-mode only support for 4716/4717/4718/47162.
	 * It is observed that the Tx-TPUT drops while transmitting GF frames to
	 * specific non-BRCM vendor STAs.
	 */
	if ((rev.chipnum == BCM47162_CHIP_ID) || (rev.chipnum == BCM43236_CHIP_ID)) {
		int pam_mode = WLC_N_PREAMBLE_GF_BRCM; /* default GF-BRCM */

		strcat_r(prefix, "mimo_preamble", tmp);
		if (nvram_match(tmp, "mm"))
			pam_mode = WLC_N_PREAMBLE_MIXEDMODE;
		else if (nvram_match(tmp, "gf"))
			pam_mode = WLC_N_PREAMBLE_GF;
		else if (nvram_match(tmp, "auto"))
			pam_mode = -1;
		WL_IOVAR_SETINT(name, "mimo_preamble", pam_mode);
	}

	/* Making default ampdu_density to 8usec in order to improve throughput
	 * of very small packet sizes (64, 88, 128,..).
	 */
	if (rev.chipnum == BCM43217_CHIP_ID)
		WL_IOVAR_SETINT(name, "ampdu_rx_density", AMPDU_DENSITY_8USEC);

	/* Get current phy type */
	WL_IOCTL(name, WLC_GET_PHYTYPE, &phytype, sizeof(phytype));
	snprintf(buf, sizeof(buf), "%s", WLCONF_PHYTYPE2STR(phytype));
	nvram_set(strcat_r(prefix, "phytype", tmp), buf);

	/* Setup regulatory mode */
	strcat_r(prefix, "reg_mode", tmp);
	if (nvram_match(tmp, "off")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
	} else if (nvram_match(tmp, "h") || nvram_match(tmp, "strict_h")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		radar_enab = TRUE;
		if (nvram_match(tmp, "h"))
			val = 1;
		else
			val = 2;
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));

		/* Set the CAC parameters, if they exist in nvram. */
		if ((str = nvram_get(strcat_r(prefix, "dfs_preism", tmp)))) {
			val = atoi(str);
			WL_IOVAR_SETINT(name, "dfs_preism", val);
		}
		if ((str = nvram_get(strcat_r(prefix, "dfs_postism", tmp)))) {
			val = atoi(str);
			WL_IOVAR_SETINT(name, "dfs_postism", val);
		}
		val = atoi(nvram_safe_get(strcat_r(prefix, "tpc_db", tmp)));
		WL_IOCTL(name, WLC_SEND_PWR_CONSTRAINT, &val, sizeof(val));
		wlconf_dfs_pref_chan_options(name);
	} else if (nvram_match(tmp, "d")) {
		val = 0;
		WL_IOCTL(name, WLC_SET_RADAR, &val, sizeof(val));
		WL_IOCTL(name, WLC_SET_SPECT_MANAGMENT, &val, sizeof(val));
		val = 1;
		WL_IOCTL(name, WLC_SET_REGULATORY, &val, sizeof(val));
	}

	/* Set up number of Tx and Rx streams */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		int count;
		int streams;

		/* Get the number of tx chains supported by the hardware */
		WL_IOVAR_GETINT(name, "hw_txchain", &count);
		/* update NVRAM with capabilities */
		snprintf(var, sizeof(var), "%d", count);
		nvram_set(strcat_r(prefix, "hw_txchain", tmp), var);

		/* Verify that there is an NVRAM param for txstreams, if not create it and
		 * set it to hw_txchain
		 */
		streams = atoi(nvram_safe_get(strcat_r(prefix, "txchain", tmp)));
		if (streams == 0) {
			/* invalid - NVRAM needs to be fixed/initialized */
			nvram_set(strcat_r(prefix, "txchain", tmp), var);
			streams = count;
		}
		/* Apply user configured txstreams, use 1 if user disabled nmode */
		WL_IOVAR_SETINT(name, "txchain", streams);

		WL_IOVAR_GETINT(name, "hw_rxchain", &count);
		/* update NVRAM with capabilities */
		snprintf(var, sizeof(var), "%d", count);
		nvram_set(strcat_r(prefix, "hw_rxchain", tmp), var);

		/* Verify that there is an NVRAM param for rxstreams, if not create it and
		 * set it to hw_txchain
		 */
		streams = atoi(nvram_safe_get(strcat_r(prefix, "rxchain", tmp)));
		if (streams == 0) {
			/* invalid - NVRAM needs to be fixed/initialized */
			nvram_set(strcat_r(prefix, "rxchain", tmp), var);
			streams = count;
		}

		/* Apply user configured rxstreams, use 1 if user disabled nmode */
		WL_IOVAR_SETINT(name, "rxchain", streams);
	}

	/* set bandwidth capability for nphy and calculate nbw */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		struct {
			int bandtype;
			uint8 bw_cap;
		} param;

		/* Get the user nmode setting now */
		nmode = AUTO;	/* enable by default for NPHY */
		/* Set n mode */
		strcat_r(prefix, "nmode", tmp);
		if (nvram_match(tmp, "0"))
			nmode = OFF;

		WL_IOVAR_SETINT(name, "nmode", (uint32)nmode);

		if (nmode == OFF)
			val = WLC_BW_CAP_20MHZ;
		else
			val = wlconf_bw_cap(prefix, bandtype);

		val &= WLC_BW_CAP_UNRESTRICTED;

		/* record the bw here */
		if (val == WLC_BW_CAP_UNRESTRICTED) {
			if (phytype == PHY_TYPE_AC) {
				nbw = cap_160 ? WL_CHANSPEC_BW_160 : WL_CHANSPEC_BW_80;
			} else {
				nbw = WL_CHANSPEC_BW_40;
			}
		} else if (val == WLC_BW_CAP_160MHZ)
			nbw = WL_CHANSPEC_BW_160;
		else if (val == WLC_BW_CAP_80MHZ)
			nbw = WL_CHANSPEC_BW_80;
		else if (val == WLC_BW_CAP_40MHZ)
			nbw = WL_CHANSPEC_BW_40;

		param.bandtype = bandtype;
		param.bw_cap = (uint8) val;
		nvr_bw_cap = (uint8) (val & WLC_BW_CAP_160MHZ); /* backup for later comparisons */

		WL_IOVAR_SET(name, "bw_cap", &param, sizeof(param));
	} else {
		/* Save n mode to OFF */
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
	}

	/* on 160MHz effect dyn160 setting when capable */
	str = nvram_safe_get(strcat_r(prefix, "dyn160", tmp));
	/* validate before setting */
	if (cap_dyn160 && str != NULL && str[0] != '\0' && str[0] >= '0' && str[0] <= '1') {
		val = atoi(str);
		WL_IOVAR_SETINT(name, "dyn160", (uint32)val);
	}

	/* Use chanspec to set the channel */
	if ((str = nvram_get(strcat_r(prefix, "chanspec", tmp))) != NULL) {
		uint32 dis_ch_grp;
		chanspec = wf_chspec_aton(str);
		nvr_chanspec = chanspec;

		/* Get the dis_ch_grp to set the initial channel */
		WL_IOVAR_GETINT(name, "dis_ch_grp", &dis_ch_grp);

		/* For Auto-Channel selection, set an initial chanspec based on band
		 * for 2G, channel 1 and for 5G, channel 36 resptively. ACSD will later select
		 * the best channel based on maximum available BW.
		 */
		if (!strcmp(str, "0")) {
			if (bandtype == WLC_BAND_5G) {
				nvr_chanspec = 0;
				chanspec = WL_CHSPEC_DEF_5G_L;
				if (dis_ch_grp == WL_RADIO_CHAN_5GH) {
					chanspec = WL_CHSPEC_DEF_5G_H;
				}
			} else {
				chanspec = WL_CHSPEC_DEF_2G;
			}
		}

		if (chanspec) {
			WL_IOVAR_SETINT(name, "chanspec", (uint32)chanspec);
		}
	}

	/* set bw_switch_160 only for 160 4x4 capable chips like 43684 */
	if (cap_bgdfs160) {
		uint8 nvr_bw = 0;
		nvr_bw = atoi(nvram_safe_get(strcat_r(prefix, "bw", tmp)));
		/* read from nvram only for 5G-160 interfaces */
#if 1
		if (bandtype == WLC_BAND_5G && wlconf_is_160(nvr_bw_cap, nvr_bw)) {
#else
		if (bandtype == WLC_BAND_5G && wlconf_is_160(nvr_chanspec, nvr_bw_cap)) {
#endif
			str = nvram_safe_get(strcat_r(prefix, "bw_switch_160", tmp));
			/* if nvram is properly set, apply value from nvram to firmware */
			if (str && str[0] && str[0] >= '0' && str[0] <= '2') {
				val = atoi(str);
				WL_IOVAR_SETINT(name, "bw_switch_160", (uint32)val);
				WLCONF_DBG("interface %s set bw_switch_160 to %d\n", name, val);
			}
			/* if nvram is not (properly) set, leave 5G-160 to firmware defaults;
			 * do not override
			 */
		} else {
			/* override to 1(FW default) to for all other cases(non-160/2G) */
			val = 1; /* default value to set */
			WL_IOVAR_SETINT(name, "bw_switch_160", (uint32)val);
			WLCONF_DBG("interface %s set bw_switch_160 to %d\n", name, val);
		}
	}
	/* Legacy method of setting channels (for compatibility) */
	/* Set channel before setting gmode or rateset */
	/* Manual Channel Selection - when channel # is not 0 */
	val = atoi(nvram_safe_get(strcat_r(prefix, "channel", tmp)));
	if ((chanspec == 0) && val && !WLCONF_PHYTYPE_11N(phytype)) {
		WL_SETINT(name, WLC_SET_CHANNEL, val);
		if (ret) {
			/* Use current channel (card may have changed) */
			WL_IOCTL(name, WLC_GET_CHANNEL, &ci, sizeof(ci));
			snprintf(buf, sizeof(buf), "%d", ci.target_channel);
			nvram_set(strcat_r(prefix, "channel", tmp), buf);
		}
	} else if ((chanspec == 0) && val && WLCONF_PHYTYPE_11N(phytype)) {
		uint channel;
		uint nctrlsb = 0;
		uint cspecbw = (bandtype == WLC_BAND_2G) ?
			WL_CHANSPEC_BAND_2G:WL_CHANSPEC_BAND_5G;

		channel = val;

		if (nbw == WL_CHANSPEC_BW_160) {
			/* Get Ctrl SB for 160MHz channel */
			str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));

			/* Adjust the channel to be center channel */
			channel = channel + CH_80MHZ_APART - CH_10MHZ_APART;

			if (!strcmp(str, "lll")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LLL;
			} else if (!strcmp(str, "llu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LLU;
				channel -= CH_20MHZ_APART;
			} else if (!strcmp(str, "lul")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LUL;
				channel -= 2 * CH_20MHZ_APART;
			} else if (!strcmp(str, "luu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LUU;
				channel -= 3 * CH_20MHZ_APART;
			} else if (!strcmp(str, "ull")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_ULL;
				channel -= 4 * CH_20MHZ_APART;
			} else if (!strcmp(str, "ulu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_ULU;
				channel -= 5 * CH_20MHZ_APART;
			} else if (!strcmp(str, "uul")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UUL;
				channel -= 6 * CH_20MHZ_APART;
			} else if (!strcmp(str, "uuu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UUU;
				channel -= 7 * CH_20MHZ_APART;
			}

		} else if (nbw == WL_CHANSPEC_BW_80) {
			/* Get Ctrl SB for 80MHz channel */
			str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));

			/* Adjust the channel to be center channel */
			channel = channel + CH_40MHZ_APART - CH_10MHZ_APART;

			if (!strcmp(str, "ll")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LL;
			} else if (!strcmp(str, "lu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LU;
				channel -= CH_20MHZ_APART;
			} else if (!strcmp(str, "ul")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UL;
				channel -= 2 * CH_20MHZ_APART;
			} else if (!strcmp(str, "uu")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UU;
				channel -= 3 * CH_20MHZ_APART;
			}

		} else if (nbw == WL_CHANSPEC_BW_40) {
			/* Get Ctrl SB for 40MHz channel */
			str = nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp));

			/* Adjust the channel to be center channel */
			if (!strcmp(str, "lower")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_LOWER;
				channel = channel + 2;
			} else if (!strcmp(str, "upper")) {
				nctrlsb = WL_CHANSPEC_CTL_SB_UPPER;
				channel = channel - 2;
			}
		}

		/* band | BW | CTRL SB | Channel */
		chanspec |= (cspecbw | nbw | nctrlsb | channel);
		WL_IOVAR_SETINT(name, "chanspec", (uint32)chanspec);
	}

#ifdef __CONFIG_DHDAP__
	/* Set the amsdu_aggsf value based on the chanspec and bandwidth.
	 * Current only apply it on FD dirver for 43684.
	 */
	if (is_dhd && BCM43684_CHIP(rev.chipnum)) {
		uint32 amsdu_aggsf = 2; /* default */
		if (wlconf_ge_bw80(nvr_chanspec, nvr_bw_cap)) {
			amsdu_aggsf = 4;
		}

		WL_IOVAR_SETINT(name, "amsdu_aggsf", amsdu_aggsf);
	}
#endif /* __CONFIG_DHDAP__ */

	/* Reset to hardware rateset (band may have changed) */
	WL_IOCTL(name, WLC_GET_RATESET, &rs, sizeof(wl_rateset_t));
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof(wl_rateset_t));

	/* Set gmode */
	if (bandtype == WLC_BAND_2G) {
		int override = WLC_PROTECTION_OFF;
		int control = WLC_PROTECTION_CTL_OFF;

		/* Set gmode */
		gmode = atoi(nvram_safe_get(strcat_r(prefix, "gmode", tmp)));
		WL_IOCTL(name, WLC_SET_GMODE, &gmode, sizeof(gmode));

		/* Set gmode protection override and control algorithm */
		strcat_r(prefix, "gmode_protection", tmp);
		if (nvram_match(tmp, "auto")) {
			override = WLC_PROTECTION_AUTO;
			control = WLC_PROTECTION_CTL_OVERLAP;
		}
		WL_IOCTL(name, WLC_SET_GMODE_PROTECTION_OVERRIDE, &override, sizeof(override));
		WL_IOCTL(name, WLC_SET_PROTECTION_CONTROL, &control, sizeof(control));
	}

	/* Set nmode_protection */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		int override = WLC_PROTECTION_OFF;
		int control = WLC_PROTECTION_CTL_OFF;

		/* Set n protection override and control algorithm */
		str = nvram_get(strcat_r(prefix, "nmode_protection", tmp));
		if (!str || !strcmp(str, "auto")) {
			override = WLC_PROTECTION_AUTO;
			control = WLC_PROTECTION_CTL_OVERLAP;
		}

		WL_IOVAR_SETINT(name, "nmode_protection_override",
		                (uint32)override);
		WL_IOCTL(name, WLC_SET_PROTECTION_CONTROL, &control, sizeof(control));
	}

	/* Set vlan_prio_mode */
	{
		uint32 mode = OFF; /* default */

		strcat_r(prefix, "vlan_prio_mode", tmp);

		if (nvram_match(tmp, "on"))
			mode = ON;

		WL_IOVAR_SETINT(name, "vlan_mode", mode);
	}

	/* Get bluetooth coexistance(BTC) mode */
	btc_mode = atoi(nvram_safe_get(strcat_r(prefix, "btc_mode", tmp)));

	/* Set the AMPDU and AMSDU options based on the N-mode */
	wme_global = wlconf_ampdu_amsdu_set(name, prefix, nmode, btc_mode);

	/* Now that wme_global is known, check per-BSS disable settings */
	for (i = 0; i < bclist->count; i++) {
		char *subprefix;
		bsscfg = &bclist->bsscfgs[i];

		subprefix = apsta ? prefix : bsscfg->prefix;

		/* For each BSS, check WME; make sure wme is set properly for this interface */
		strcat_r(subprefix, "wme", tmp);
		nvram_set(tmp, wme_global ? "on" : "off");

		str = nvram_safe_get(strcat_r(bsscfg->prefix, "wme_bss_disable", tmp));
		val = (str[0] == '1') ? 1 : 0;
		WL_BSSIOVAR_SETINT(name, "wme_bss_disable", bsscfg->idx, val);
	}

	/*
	* Set operational capabilities required for stations
	* to associate to the BSS. Per-BSS setting.
	*/
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "bss_opmode_cap_reqd", tmp));
		val = atoi(str);
		WL_BSSIOVAR_SETINT(name, "mode_reqd", bsscfg->idx, val);
	}

	/* Get current rateset (gmode may have changed) */
	WL_IOCTL(name, WLC_GET_CURR_RATESET, &rs, sizeof(wl_rateset_t));

	strcat_r(prefix, "rateset", tmp);
	if (nvram_match(tmp, "all")) {
		/* Make all rates basic */
		for (i = 0; i < rs.count; i++)
			rs.rates[i] |= 0x80;
	} else if (nvram_match(tmp, "12")) {
		/* Make 1 and 2 basic */
		for (i = 0; i < rs.count; i++) {
			if ((rs.rates[i] & 0x7f) == 2 || (rs.rates[i] & 0x7f) == 4)
				rs.rates[i] |= 0x80;
			else
				rs.rates[i] &= ~0x80;
		}
	} else if (nvram_match(tmp, "ofdmonly")) {
		/* ofdm basic rate are 6, 12 and 24Mpbs */
		wl_rateset_t ofdm_rates = {
			8,
			{ /*    6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
				0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
			}
		};
		memcpy((char*)&rs, (char*)&ofdm_rates, sizeof(ofdm_rates));
	}

	if (phytype != PHY_TYPE_SSN && phytype != PHY_TYPE_LCN) {
		/* Set BTC mode */
		if (!wl_iovar_setint(name, "btc_mode", btc_mode)) {
			if (btc_mode == WL_BTC_PREMPT) {
				wl_rateset_t rs_tmp = rs;
				/* remove 1Mbps and 2 Mbps from rateset */
				for (i = 0, rs.count = 0; i < rs_tmp.count; i++) {
					if ((rs_tmp.rates[i] & 0x7f) == 2 ||
					    (rs_tmp.rates[i] & 0x7f) == 4)
						continue;
					rs.rates[rs.count++] = rs_tmp.rates[i];
				}
			}
		}
	}

	/* Set rateset */
	WL_IOCTL(name, WLC_SET_RATESET, &rs, sizeof(wl_rateset_t));

	/* Set bss beacon rate (force_bcn_rspec) in 500Kbps unit */
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "force_bcn_rspec", tmp));
		val = atoi(str);
		if (val == 12 || val == 24 || val == 48 || val == 2 || val == 4 ||
				val == 11 || val == 22) {
			WL_BSSIOVAR_SETINT(name, "force_bcn_rspec", bsscfg->idx, val);
		}
	}

	/* Allow short preamble settings for the following:
	 * 11b - short/long
	 * 11g - short /long in GMODE_LEGACY_B and GMODE_AUTO gmodes
	 *	 GMODE_PERFORMANCE and GMODE_LRS will use short and long
	 *	 preambles respectively, by default
	 * 11n - short/long applicable in 2.4G band only
	 */
	if (phytype == PHY_TYPE_B ||
	    (WLCONF_PHYTYPE_11N(phytype) && (bandtype == WLC_BAND_2G)) ||
	    ((phytype == PHY_TYPE_G || phytype == PHY_TYPE_LP) &&
	     (gmode == GMODE_LEGACY_B || gmode == GMODE_AUTO))) {
		strcat_r(prefix, "plcphdr", tmp);
		if (nvram_match(tmp, "long"))
			val = WLC_PLCP_AUTO;
		else
			val = WLC_PLCP_SHORT;
		WL_IOCTL(name, WLC_SET_PLCPHDR, &val, sizeof(val));
	}

	/* Set rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rate", tmp))) / 500000;

	/* Convert Auto mcsidx to Auto rate */
	if (WLCONF_PHYTYPE_11N(phytype) && (nmode != OFF)) {
		int mcsidx = atoi(nvram_safe_get(strcat_r(prefix, "nmcsidx", tmp)));

		/* -1 mcsidx used to designate AUTO rate */
		if (mcsidx == -1)
			val = 0;
	}

	/* 1Mbps and 2 Mbps are not allowed in BTC pre-emptive mode */
	if (btc_mode == WL_BTC_PREMPT && (val == 2 || val == 4))
		/* Must b/g band.  Set to 5.5Mbps */
		val = 11;

	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "2g_rate", val);
	error_a = wl_iovar_setint(name, "5g_rate", val);

	if (error_bg && error_a) {
		/* both failed. Try default rate (card may have changed) */
		val = 0;

		error_bg = wl_iovar_setint(name, "2g_rate", val);
		error_a = wl_iovar_setint(name, "5g_rate", val);

		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "rate", tmp), buf);
	}

	/* check if nrate needs to be applied */
	if (nmode != OFF) {
		uint32 nrate = 0;
		int mcsidx = atoi(nvram_safe_get(strcat_r(prefix, "nmcsidx", tmp)));
		bool ismcs = (mcsidx >= 0);

		/* mcsidx of 32 is valid only for 40 Mhz */
		if (mcsidx == 32 && nbw == WL_CHANSPEC_BW_20) {
			mcsidx = -1;
			ismcs = FALSE;
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");
		}

		/* Use nrate iovar only for MCS rate. */
		if (ismcs) {
			nrate |= WL_RSPEC_ENCODE_HT;
			nrate |= mcsidx & WL_RSPEC_RATE_MASK;

			WL_IOVAR_SETINT(name, "nrate", nrate);
		}
	}

	/* Set multicast rate in 500 Kbps units */
	val = atoi(nvram_safe_get(strcat_r(prefix, "mrate", tmp))) / 500000;
	/* 1Mbps and 2 Mbps are not allowed in BTC pre-emptive mode */
	if (btc_mode == WL_BTC_PREMPT && (val == 2 || val == 4))
		/* Must b/g band.  Set to 5.5Mbps */
		val = 11;

	/* it is band-blind. try both band */
	error_bg = wl_iovar_setint(name, "2g_mrate", val);
	error_a = wl_iovar_setint(name, "5g_mrate", val);

	if (error_bg && error_a) {
		/* Try default rate (card may have changed) */
		val = 0;

		WL_IOVAR_SETINT(name, "2g_mrate", val);
		WL_IOVAR_SETINT(name, "5g_mrate", val);

		snprintf(buf, sizeof(buf), "%d", val);
		nvram_set(strcat_r(prefix, "mrate", tmp), buf);
	}

	/* set vht mcs */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		str = nvram_get(strcat_r(prefix, "vmcsidx", tmp));
		if (str != NULL) {
			int vmcsidx = atoi(str);
			/* -1 vmcsidx used to designate AUTO rate */
			if (vmcsidx >= 0 && vmcsidx <= 11) {
				int Nss = 0, streams;

				streams = atoi(nvram_safe_get(strcat_r(prefix, "txchain", tmp)));
				if (streams == 0) {
					Nss = 1;
				} else {
					/* count the number of bits set */
					for (i = 0; i < 4; i++) {
						if (streams & (1 << i)) {
							Nss += streams & 1;
						}
					}
				}
				val = WL_RSPEC_ENCODE_VHT;	/* 11ac VHT */
				val |= (Nss << WL_RSPEC_VHT_NSS_SHIFT) | vmcsidx;
				WL_IOVAR_SETINT(name, "2g_rate", (int)val);
				WL_IOVAR_SETINT(name, "5g_rate", (int)val);
			}
		}
	}

	/* Set fragmentation threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "frag", tmp)));
	WL_IOVAR_SETINT(name, "fragthresh", val);

	/* Set RTS threshold */
	val = atoi(nvram_safe_get(strcat_r(prefix, "rts", tmp)));
	WL_IOVAR_SETINT(name, "rtsthresh", val);

	/* Set DTIM period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "dtim", tmp)));
	WL_IOCTL(name, WLC_SET_DTIMPRD, &val, sizeof(val));

	/* Set beacon period */
	val = atoi(nvram_safe_get(strcat_r(prefix, "bcn", tmp)));
	WL_IOCTL(name, WLC_SET_BCNPRD, &val, sizeof(val));

	/* Set SW probe response */
	val = atoi(nvram_safe_get(strcat_r(prefix, "probresp_sw", tmp)));
	WL_IOVAR_SETINT(name, "probresp_sw", val);

	/* Update vht_features only if explicitly updated by NVRAM */
	if (phytype == PHY_TYPE_AC) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "vhtmode", tmp)));
		if (val != -1) {
			WL_IOVAR_SETINT(name, "vhtmode", val);
		}

		val = atoi(nvram_safe_get(strcat_r(prefix, "vht_features", tmp)));
		if (val != -1) {
			WL_IOVAR_SETINT(name, "vht_features", val);
		}
	}

	/* For 11ax device, update he features only if explicitly updated by NVRAM */
	if (cap_11ax) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "he_features", tmp)));
		if (val != -1) {
			WL_HEIOVAR_SETINT(name, "he", "features", val);
		}
	}

	/* Set obss dyn bw switch only for 5g */
	val = atoi(nvram_safe_get(strcat_r(prefix, "obss_dyn_bw", tmp)));
	if ((bandtype == WLC_BAND_5G) && (val == 1 || val == 2)) {
		/*  val = 1 means dynamic bwsw is based on rxcrs stats
		* val = 2 means dynamic bwsw is based on txop stats
		*/
		WL_IOVAR_SETINT(name, "obss_dyn_bw", val);
	}
	else {
		WL_IOVAR_SETINT(name, "obss_dyn_bw", 0);
	}

	/* Set obss_prot to default OFF */
	if (bandtype == WLC_BAND_5G) {
		wl_config_t cfg;

		cfg.config = 0;
		cfg.status = 0;

		printf("%s: disable obss_prot by default\n", __FUNCTION__);
		WL_IOVAR_SET(name, "obss_prot", &cfg, sizeof(cfg));
	}

	/* Set beacon rotation */
	str = nvram_get(strcat_r(prefix, "bcn_rotate", tmp));
	if (!str) {
		/* No nvram variable found, use the default */
		str = nvram_default_get(strcat_r(prefix, "bcn_rotate", tmp));
	}
	val = atoi(str);
	WL_IOVAR_SETINT(name, "bcn_rotate", val);

	/* Set framebursting mode */
	if (btc_mode == WL_BTC_PREMPT) {
		val = FALSE;
	} else {
		val = nvram_match(strcat_r(prefix, "frameburst", tmp), "on");
		WL_IOCTL(name, WLC_SET_FAKEFRAG, &val, sizeof(val));

		/* Dynamic framebusrting */
		val = nvram_match(strcat_r(prefix, "frameburst_override", tmp), "on");
		WL_IOVAR_SETINT(name, "frameburst_override", val);
	}

	/* Set STBC tx and rx mode */
	if (phytype == PHY_TYPE_N ||
		phytype == PHY_TYPE_HT ||
		phytype == PHY_TYPE_AC) {
		char *nvram_str = nvram_safe_get(strcat_r(prefix, "stbc_tx", tmp));

		if (!strcmp(nvram_str, "auto")) {
			WL_IOVAR_SETINT(name, "stbc_tx", AUTO);
		} else if (!strcmp(nvram_str, "on")) {
			WL_IOVAR_SETINT(name, "stbc_tx", ON);
		} else if (!strcmp(nvram_str, "off")) {
			WL_IOVAR_SETINT(name, "stbc_tx", OFF);
		}
		val = atoi(nvram_safe_get(strcat_r(prefix, "stbc_rx", tmp)));
		WL_IOVAR_SETINT(name, "stbc_rx", val);
	}

	/* Set RIFS mode based on framebursting */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		char *nvram_str = nvram_safe_get(strcat_r(prefix, "rifs", tmp));
		if (!strcmp(nvram_str, "on")) {
			WL_IOVAR_SETINT(name, "rifs", ON);
		} else if (!strcmp(nvram_str, "off")) {
			WL_IOVAR_SETINT(name, "rifs", OFF);
		}
		/* RIFS mode advertisement */
		nvram_str = nvram_safe_get(strcat_r(prefix, "rifs_advert", tmp));
		if (!strcmp(nvram_str, "auto")) {
			WL_IOVAR_SETINT(name, "rifs_advert", AUTO);
		} else if (!strcmp(nvram_str, "off")) {
			WL_IOVAR_SETINT(name, "rifs_advert", OFF);
		}
	}

	/* Set Guard Intervals */
	str = nvram_safe_get(strcat_r(prefix, "sgi_tx", tmp));
	if (!strcmp(str, "auto")) {
		WL_IOVAR_SETINT(name, "sgi_tx", AUTO);
	} else if (!strcmp(str, "on")) {
		WL_IOVAR_SETINT(name, "sgi_tx", ON);
	} else if (!strcmp(str, "off")) {
		WL_IOVAR_SETINT(name, "sgi_tx", OFF);
	}

	/* Override BA mode only if set to on/off */
	ba = nvram_safe_get(strcat_r(prefix, "ba", tmp));
	if (!strcmp(ba, "on")) {
		WL_IOVAR_SETINT(name, "ba", ON);
	} else if (!strcmp(ba, "off")) {
		WL_IOVAR_SETINT(name, "ba", OFF);
	}

	if (WLCONF_PHYTYPE_11N(phytype)) {
		val = AVG_DMA_XFER_RATE;
		wl_iovar_set(name, "avg_dma_xfer_rate", &val, sizeof(val));
	}

	/*
	 * If no nvram variable exists to force non-aggregated mpdu regulation ON,
	 * for both 2G and 5G interfaces.
	 */
	str = nvram_get(strcat_r(prefix, "nar", tmp));
	if (str) {
		val = atoi(str);
	} else {
		val = 1;
	}
	WLCONF_DBG("%sabling non-aggregated regulation on band %d\n", (val) ?
		"En":"Dis", bandtype);
	WL_IOVAR_SETINT(name, "nar", val);
	if (val) {
		/* nar is enabled on this interface, add tuneable parameters */
		str = nvram_get(strcat_r(prefix, "nar_handle_ampdu", tmp));
		if (str) {
			WL_IOVAR_SETINT(name, "nar_handle_ampdu", atoi(str));
		}
		str = nvram_get(strcat_r(prefix, "nar_transit_limit", tmp));
		if (str) {
			WL_IOVAR_SETINT(name, "nar_transit_limit", atoi(str));
		}
	}

	if (BCM53573_CHIP(rev.chipnum) && (rev.chiprev == 0x0)) {
		/* Unset TXBF cap */
		wlconf_unset_txbf(name, prefix);
	} else {
		/* Set up TxBF */
		wlconf_set_txbf(name, prefix, bandtype);
	}

	/* set airtime fairness */
	val = 0;
	str = nvram_get(strcat_r(prefix, "atf", tmp));
	if (str) {
		val = atoi(str);
	}
	WL_IOVAR_SETINT(name, "atf", val);

	str = nvram_get(strcat_r(prefix, "ampdu_atf_us", tmp));
	if (str) {
		val = atoi(str);
		if (val) {
			WL_IOVAR_SETINT(name, "ampdu_atf_us", val);
			WL_IOVAR_SETINT(name, "nar_atf_us", val);
		}
	}

	/* set TAF */
	val = 0;
	str = nvram_get(strcat_r(prefix, "taf_enable", tmp));
	if (str) {
		val = atoi(str);
	}
	wlconf_set_taf(name, val);

	/* Bring the interface back up */
	WL_IOCTL(name, WLC_UP, NULL, 0);

	/* set phy_percal_delay */
	val = atoi(nvram_safe_get(strcat_r(prefix, "percal_delay", tmp)));
	if (val) {
		wl_iovar_set(name, "phy_percal_delay", &val, sizeof(val));
	}

	/* Set phy periodic cal if nvram present. Otherwise, use driver defaults. */
	str = nvram_get(strcat_r(prefix, "cal_period", tmp));
	if (str) {
		/*
		 *  If cal_period is "-1 / Auto"
		 *     - For corerev >= 40, set cal_period to 0
		 *     - For corerev < 40, use driver defaults.
		 *  Else
		 *     - Use the value specified in the nvram.
		 */
		val = atoi(str);
		if (val == -1) {
			if (rev.corerev >= 40) {
				val = 0;
				WL_IOVAR_SET(name, "cal_period", &val, sizeof(val));
			}
		} else {
			WL_IOVAR_SET(name, "cal_period", &val, sizeof(val));
		}
	}

	/* Set antenna */
	val = atoi(nvram_safe_get(strcat_r(prefix, "antdiv", tmp)));
	WL_IOCTL(name, WLC_SET_ANTDIV, &val, sizeof(val));

	/* Set antenna selection */
	wlconf_set_antsel(name, prefix);

	/* Set radar parameters if it is enabled */
	if (radar_enab) {
		wlconf_set_radarthrs(name, prefix);
		wlconf_set_radarthrs2(name, prefix);
	}

	/* set pspretend */
	val = 0;
	if (ap) {
		bool set_psp;
		/* Set pspretend for multi-ssid bss */
		for (i = 0; i < bclist->count; i++) {
			set_psp = FALSE;
			bsscfg = &bclist->bsscfgs[i];
			str = nvram_safe_get(strcat_r(bsscfg->prefix,
				"pspretend_retry_limit", tmp));
			if (str && (*str != '\0')) {
				val = atoi(str);
				set_psp = TRUE;
			} else {
				val = PSPRETEND_DEFAULT_THRESHOLD;
				set_psp = TRUE;
			}

			if (set_psp) {
				WL_BSSIOVAR_SETINT(name, "pspretend_retry_limit", bsscfg->idx, val);
			}
		}

		/* now set it for primary bss */
		val = 0;
		str = nvram_get(strcat_r(prefix, "pspretend_retry_limit", tmp));
		if (str && (*str != '\0')) {
			val = atoi(str);
		} else {
			val = PSPRETEND_DEFAULT_THRESHOLD;
		}
	}
	WL_IOVAR_SETINT(name, "pspretend_retry_limit", val);

	/* Set channel interference threshold value if it is enabled */

	str = nvram_get(strcat_r(prefix, "glitchthres", tmp));

	if (str) {
		int glitch_thres = atoi(str);
		if (glitch_thres > 0)
			WL_IOVAR_SETINT(name, "chanim_glitchthres", glitch_thres);
	}

	str = nvram_get(strcat_r(prefix, "ccathres", tmp));

	if (str) {
		int cca_thres = atoi(str);
		if (cca_thres > 0)
			WL_IOVAR_SETINT(name, "chanim_ccathres", cca_thres);
	}

	str = nvram_get(strcat_r(prefix, "chanimmode", tmp));

	if (str) {
		int chanim_mode = atoi(str);
		if (chanim_mode >= 0)
			WL_IOVAR_SETINT(name, "chanim_mode", chanim_mode);
	}

	/* bcm_dcs (dynamic channel selection) settings */
	str = nvram_safe_get(strcat_r(prefix, "bcmdcs", tmp));
	if (!strcmp(str, "on")) {
		WL_IOVAR_SETINT(name, "bcm_dcs", ON);
	} else if (!strcmp(str, "off")) {
		WL_IOVAR_SETINT(name, "bcm_dcs", OFF);
	}

	/* Overlapping BSS Coexistence aka 20/40 Coex. aka OBSS Coex.
	 * For an AP - Only use if 2G band AND user wants a 40Mhz chanspec.
	 * For a STA - Always
	 */
	if (WLCONF_PHYTYPE_11N(phytype)) {
		if (sta ||
		    ((ap || apsta) && (nbw == WL_CHANSPEC_BW_40) && (bandtype == WLC_BAND_2G))) {
			str = nvram_safe_get(strcat_r(prefix, "obss_coex", tmp));
			if (!str) {
				/* No nvram variable found, use the default */
				str = nvram_default_get(strcat_r(prefix, "obss_coex", tmp));
			}
			obss_coex = atoi(str);
		} else {
			/* Need to disable obss coex in case of 20MHz and/or
			 * in case of 5G.
			 */
			obss_coex = 0;
		}
#ifdef WLTEST
		/* force coex off for msgtest build */
		obss_coex = 0;
#endif // endif
		WL_IOVAR_SETINT(name, "obss_coex", obss_coex);
	}

	/* Set nvram defaults for txfail thresholds */
	wlconf_set_traffic_thresh(name, prefix, bandtype);
	/* Set up TxBF timer */
	wlconf_set_txbf_timer(name, prefix);

	/* Set dynamic ED */
	strcat_r(prefix, "dy_ed_thresh", tmp);
	if (nvram_match(tmp, "1")) {
		if (phytype == PHY_TYPE_AC) {
			WL_IOVAR_SETINT(name, "dy_ed_thresh", 1);
			WL_IOVAR_SETINT(name, "dy_ed_thresh_acphy", 1);
		} else {
			printf("wl%d: phytype %d not support dy-ed", unit, phytype);
		}
	}

	/* Auto Channel Selection:
	 * 1. When channel # is 0 in AP mode, this determines our channel and 20Mhz vs. 40Mhz
	 * 2. If we're running OBSS Coex and the user specified a channel, Autochannel runs to
	 *    do an initial scan to help us make decisions about whether we can create a 40Mhz AP
	 */
	/* The following condition(s) must be met in order for Auto Channel Selection to work.
	 *  - the I/F must be up for the channel scan
	 *  - the AP must not be supporting a BSS (all BSS Configs must be disabled)
	 */
	if (ap || apsta) {
		int channel = chanspec ? wf_chspec_ctlchan(chanspec) : 0;
#if defined(BCM_ESCAND) || defined(EXT_ACS)
		char tmp[100];
		char *ptr;
#endif /* BCM_ESCAND || EXT_ACS */
#ifdef EXT_ACS
		char * str_val;
#endif /* EXT_ACS */
#ifdef BCM_ESCAND
		snprintf(tmp, sizeof(tmp), "escand_ifnames");
		ptr = nvram_get(tmp);
		/* only when escand_ifnames is not set OR
		 * it does not include current name to
		 * set escand_ifnames
		 */
		if (!ptr || (!strstr(ptr, name) && !strstr(ptr, "none"))) {
			if (ptr)
				snprintf(buf, sizeof(buf), "%s %s", ptr, name);
			else
				strncpy(buf, name, sizeof(buf));
			nvram_set(tmp, buf);
		}
#endif /* BCM_ESCAND */
#ifdef EXT_ACS
		str_val = nvram_safe_get("acs_mode");
		if (!strcmp(str_val, "legacy"))
			goto legacy_mode;

		snprintf(tmp, sizeof(tmp), "acs_ifnames");
		ptr = nvram_get(tmp);
		/* only when acs_ifnames is not set OR
		 * it does not include current name to
		 * set acs_ifnames
		 */
		if (!ptr || !strstr(ptr, name)) {
			if (ptr)
				snprintf(buf, sizeof(buf), "%s %s", ptr, name);
			else
				strncpy(buf, name, sizeof(buf));
			nvram_set(tmp, buf);
		}
		WL_IOVAR_SETINT(name, "chanim_mode", CHANIM_EXT);
		goto legacy_end;

legacy_mode:
#endif /* EXT_ACS */
		if (obss_coex || channel == 0) {
			if (WLCONF_PHYTYPE_11N(phytype)) {
				chanspec_t chanspec;
				int pref_chspec;

				if (channel != 0) {
					/* assumes that initial chanspec has been set earlier */
					/* Maybe we expand scope of chanspec from above so
					 * that we don't have to do the iovar_get here?
					 */

					/* We're not doing auto-channel, give the driver
					 * the preferred chanspec.
					 */
					wl_iovar_get(name, "chanspec", &pref_chspec, sizeof(chanspec_t));
					WL_IOVAR_SETINT(name, "pref_chanspec", pref_chspec);
				} else {
					WL_IOVAR_SETINT(name, "pref_chanspec", 0);
				}

				chanspec = wlconf_auto_chanspec(name);
				if (chanspec != 0)
					WL_IOVAR_SETINT(name, "chanspec", chanspec);
			} else {
				/* select a channel */
				val = wlconf_auto_channel(name);
				/* switch to the selected channel */
				if (val != 0)
					WL_IOCTL(name, WLC_SET_CHANNEL, &val, sizeof(val));
			}
			/* set the auto channel scan timer in the driver when in auto mode */
			if (channel == 0) {
				val = 15;	/* 15 minutes for now */
			} else {
				val = 0;
			}
		} else {
			/* reset the channel scan timer in the driver when not in auto mode */
			val = 0;
		}

		WL_IOCTL(name, WLC_SET_CS_SCAN_TIMER, &val, sizeof(val));
		WL_IOVAR_SETINT(name, "chanim_mode", CHANIM_ACT);
#ifdef EXT_ACS
legacy_end:
		;
#endif /* EXT_ACS */
		/* Apply sta_config configuration settings for this interface */
		foreach(var, nvram_safe_get("sta_config"), next) {
			wlconf_process_sta_config_entry(name, var);
		}

	} /* AP or APSTA */

	/* Security settings for each BSS Configuration */
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		wlconf_security_options(name, bsscfg->prefix, bsscfg->idx,
		                        mac_spoof, wet || sta || apsta || psta || psr);
	}

	/* configure MBO, MFP  */
	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		wlconf_configure_mbo(bsscfg->ifname, bsscfg->prefix, bsscfg->idx);
	}
	/*
	 * Finally enable BSS Configs or Join BSS
	 *
	 * AP: Enable BSS Config to bring AP up only when nas will not run
	 * STA: Join the BSS regardless.
	 */
	for (i = 0; i < bclist->count; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;
		char vifname[VIFNAME_LEN];
		char *name_ptr = name;

		setbuf.bsscfg_idx = bclist->bsscfgs[i].idx;
		setbuf.enable = 0;

		bsscfg = &bclist->bsscfgs[i];
		if (nvram_match(strcat_r(bsscfg->prefix, "bss_enabled", tmp), "1")) {
			setbuf.enable = 1;
		}

		/* Set the MAC list */
		maclist = (struct maclist *)buf;
		maclist->count = 0;
		if (!nvram_match(strcat_r(bsscfg->prefix, "macmode", tmp), "disabled")) {
			ea = maclist->ea;
			foreach(var, nvram_safe_get(strcat_r(bsscfg->prefix, "maclist", tmp)),
				next) {
				if (((char *)((&ea[1])->octet)) > ((char *)(&buf[sizeof(buf)])))
					break;
				if (ether_atoe(var, ea->octet)) {
					maclist->count++;
					ea++;
				}
			}
		}

		if (setbuf.bsscfg_idx == 0) {
			name_ptr = name;
		} else { /* Non-primary BSS; changes name syntax */
			char tmp[VIFNAME_LEN];
			int len;

			/* Remove trailing _ if present */
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, bsscfg->prefix, VIFNAME_LEN - 1);
			if (((len = strlen(tmp)) > 0) && (tmp[len - 1] == '_')) {
				tmp[len - 1] = 0;
			}
			nvifname_to_osifname(tmp, vifname, VIFNAME_LEN);
			name_ptr = vifname;
		}

		WL_IOCTL(name_ptr, WLC_SET_MACLIST, buf, sizeof(buf));

		/* Set macmode for each VIF */
		(void) strcat_r(bsscfg->prefix, "macmode", tmp);

		if (nvram_match(tmp, "deny"))
			val = WLC_MACMODE_DENY;
		else if (nvram_match(tmp, "allow"))
			val = WLC_MACMODE_ALLOW;
		else
			val = WLC_MACMODE_DISABLED;

		WL_IOCTL(name_ptr, WLC_SET_MACMODE, &val, sizeof(val));
	}

	ret = 0;
exit:
	if (bclist != NULL)
		free(bclist);

	return ret;
}

int
wlconf_down(char *name)
{
	int val, ret = 0;
	int i, unit;
	int wlsubunit;
	int bcmerr;
	struct {int bsscfg_idx; int enable;} setbuf;
	int wl_ap_build = 0; /* 1 = wl compiled with AP capabilities */
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_MEDLEN];
	char tmp[100], prefix[PREFIX_LEN], *str, *str1;
	int max_no_vifs = 0;
	char *next;
	wlc_ssid_t ssid;

	/* wlconf doesn't work for virtual i/f */
	if (get_ifname_unit(name, NULL, &wlsubunit) == 0 && wlsubunit >= 0) {
		WLCONF_DBG("wlconf: skipping virtual interface \"%s\"\n", name);
		return 0;
	}

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap")) {
			wl_ap_build = 1;
		} else if (!strcmp(cap, "mbss16")) {
			max_no_vifs = 16;
		} else if (!strcmp(cap, "mbss8")) {
			max_no_vifs = 8;
		} else if (!strcmp(cap, "mbss4")) {
			max_no_vifs = 4;
		}
	}

#if defined(BCA_CPEROUTER)
	if (max_no_vifs > WL_MAX_NUM_SSID)
		max_no_vifs =  WL_MAX_NUM_SSID;
#endif // endif

	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	str1 = nvram_safe_get(strcat_r(prefix, "prev_mode", tmp));
	if (strcmp(str, str1)) {
		/* Clear all the mbss interfaces */
		for (i = 1; i < max_no_vifs; i++) {
			snprintf(tmp, sizeof(tmp), "wl%d.%d_hwaddr", unit, i);
			nvram_set(tmp, "");
		}
		nvram_set(strcat_r(prefix, "prev_mode", tmp), str);
	}

	if (wl_ap_build) {
#ifdef CONFIG_HOSTAPD
		if (!nvram_match("hapd_enable", "0")) {
			/* For STA interface set NULL SSID */
			if (strcmp(str, "sta") == 0) {
				/* Nuke SSID */
				ssid.SSID_len = 0;
				ssid.SSID[0] = '\0';
				WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));
			}
		}
#endif	/* CONFIG_HOSTAPD */

		/* Bring down the interface */
		WL_IOCTL(name, WLC_DOWN, NULL, 0);

		/* Disable all BSS Configs */
		for (i = 0; i < WL_MAXBSSCFG; i++) {
			setbuf.bsscfg_idx = i;
			setbuf.enable = 0;

			ret = wl_iovar_set(name, "bss", &setbuf, sizeof(setbuf));
			if (ret) {
				(void) wl_iovar_getint(name, "bcmerror", &bcmerr);
				/* fail quietly on a range error since the driver may
				 * support fewer bsscfgs than we are prepared to configure
				 */
				if (bcmerr == BCME_RANGE)
					break;
			}
		}
	} else {
		WL_IOCTL(name, WLC_GET_UP, &val, sizeof(val));
		if (val) {
			/* Nuke SSID */
			ssid.SSID_len = 0;
			ssid.SSID[0] = '\0';
			WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));

			/* Bring down the interface */
			WL_IOCTL(name, WLC_DOWN, NULL, 0);
		}
	}

	/* Nuke the WDS list */
	wlconf_wds_clear(name);

	return 0;
}

int
wlconf_start(char *name)
{
	int i, ii, unit, val, ret = 0;
	int wlunit = -1;
	int wlsubunit = -1;
	int ap, apsta, wds, sta = 0, wet = 0;
	int wl_ap_build = 0; /* wl compiled with AP capabilities */
	char buf[WLC_IOCTL_SMLEN];
	struct maclist *maclist;
	struct ether_addr *ea;
	struct bsscfg_list *bclist = NULL;
	struct bsscfg_info *bsscfg = NULL;
	wlc_ssid_t ssid;
	char cap[WLC_IOCTL_SMLEN], caps[WLC_IOCTL_MEDLEN];
	char var[80], tmp[100], prefix[PREFIX_LEN], *str, *next;
	int trf_mgmt_cap = 0, trf_mgmt_dwm_cap = 0;
	bool dwm_supported = FALSE;
#ifdef __CONFIG_DHDAP__
	int is_dhd = 0;
#endif // endif
	int wmf_bss_disab = 0;
	bool ure_mbss_enab = FALSE;
#ifdef BCM_WBD
	int map_mode = 0, map = 0, is_bssid_join = 0;
	struct ether_addr bssid;
#endif /* BCM_WBD */

	/* Check interface (fail silently for non-wl interfaces) */
	if ((ret = wl_probe(name)))
		return ret;

	/* wlconf doesn't work for virtual i/f, so if we are given a
	 * virtual i/f return 0 if that interface is in it's parent's "vifs"
	 * list otherwise return -1
	 */
	memset(tmp, 0, sizeof(tmp));
	if (get_ifname_unit(name, &wlunit, &wlsubunit) == 0) {
		if (wlsubunit >= 0) {
			/* we have been given a virtual i/f,
			 * is it in it's parent i/f's virtual i/f list?
			 */
			sprintf(tmp, "wl%d_vifs", wlunit);

			if (strstr(nvram_safe_get(tmp), name) == NULL)
				return -1; /* config error */
			else
				return 0; /* okay */
		}
	}
	else {
		return -1;
	}

	/* because of ifdefs in wl driver,  when we don't have AP capabilities we
	 * can't use the same iovars to configure the wl.
	 * so we use "wl_ap_build" to help us know how to configure the driver
	 */
	if (wl_iovar_get(name, "cap", (void *)caps, sizeof(caps)))
		return -1;

	foreach(cap, caps, next) {
		if (!strcmp(cap, "ap"))
			wl_ap_build = 1;

		if (!strcmp(cap, "traffic-mgmt"))
			trf_mgmt_cap = 1;

		if (!strcmp(cap, "traffic-mgmt-dwm"))
			trf_mgmt_dwm_cap = 1;
	}

	/* Get instance */
	WL_IOCTL(name, WLC_GET_INSTANCE, &unit, sizeof(unit));
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	/* Get the list of BSS Configs */
	if (!(bclist = wlconf_get_bsscfgs(name, prefix)))
		return -1;

	/* wlX_mode settings: AP, STA, WET, BSS/IBSS, APSTA */
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	ap = (!strcmp(str, "") || !strcmp(str, "ap"));
	apsta = (!strcmp(str, "apsta") ||
	         ((!strcmp(str, "sta") || !strcmp(str, "psr") || !strcmp(str, "wet")) &&
	          bclist->count > 1));
	sta = (!strcmp(str, "sta") && bclist->count == 1);
	wds = !strcmp(str, "wds");
	wet = !strcmp(str, "wet");
	if (!strcmp(str, "mac_spoof") || !strcmp(str, "psta") || !strcmp(str, "psr"))
		sta = 1;
#ifdef BCM_WBD
	map_mode = strtol(nvram_safe_get(NVRAM_MAP_MODE), NULL, 0);
	map = strtol(nvram_safe_get(strcat_r(prefix, "map", tmp)), NULL, 0);
#endif	/* BCM_WBD */

#ifdef BCA_HNDROUTER
	/* If WET is enabled, we need to set flow-cache in L3 acceleration mode.
	 * L2+L3 acceleration mode is not compatible with WET.
	 */
	if (wet) {
		eval("fcctl", "disable");
		eval("fcctl", "flush");
		eval("fcctl", "config", "--accel-mode", "0"); /* 0:L3, 1:L2+L3 */
		eval("fcctl", "enable");
		eval("fcctl", "status");
	}
#endif /* BCA_HNDROUTER */

	/* Retain remaining WET effects only if not APSTA */
	wet &= !apsta;

	/* AP only config, code copied as-is from wlconf function */
	if (ap || apsta || wds) {
		/* Set lazy WDS mode */
		val = atoi(nvram_safe_get(strcat_r(prefix, "lazywds", tmp)));
		WL_IOCTL(name, WLC_SET_LAZYWDS, &val, sizeof(val));

		/* Set the WDS list */
		maclist = (struct maclist *) buf;
		maclist->count = 0;
		ea = maclist->ea;
		foreach(var, nvram_safe_get(strcat_r(prefix, "wds", tmp)), next) {
			if (((char *)(ea->octet)) > ((char *)(&buf[sizeof(buf)])))
				break;
			ether_atoe(var, ea->octet);
			maclist->count++;
			ea++;
		}
		WL_IOCTL(name, WLC_SET_WDSLIST, buf, sizeof(buf));

		/* Set WDS link detection timeout */
		val = atoi(nvram_safe_get(strcat_r(prefix, "wds_timeout", tmp)));
		WL_IOVAR_SETINT(name, "wdstimeout", val);
	}

	if (!strcmp(nvram_safe_get("ure_disable"), "0") &&
		!strcmp(nvram_safe_get(strcat_r(prefix, "ure_mbss", tmp)), "1")) {
		/* Multiple URE BSS is enabled */
		ure_mbss_enab = TRUE;
	}

	/*
	 * Finally enable BSS Configs or Join BSS
	 * code copied as-is from wlconf function
	 */
	for (i = 0; i < bclist->count; i++) {
		struct {int bsscfg_idx; int enable;} setbuf;

		setbuf.bsscfg_idx = bclist->bsscfgs[i].idx;
		setbuf.enable = 0;

		bsscfg = &bclist->bsscfgs[i];
		if (nvram_match(strcat_r(bsscfg->prefix, "bss_enabled", tmp), "1")) {
			setbuf.enable = 1;
		}

#ifdef BCM_WBD
		/* If multiap is enabled and if the interface is STA, try to join to the
		 * provided BSSID. STA interface will be primary, so check the bsscfg index
		 */
		if ((bsscfg->idx == 0) && (!MAP_IS_DISABLED(map_mode)) && (map & 0x04)) {
			is_bssid_join = wlconf_wbd_is_bssid_join_possible(name,
				bsscfg->prefix, &bssid);
		} else {
			is_bssid_join = 0;
		}
#endif /* BCM_WBD */

		/*  bring up BSS  */
		if (ap || apsta || sta || wet) {
			for (ii = 0; ii < MAX_BSS_UP_RETRIES; ii++) {
				if (wl_ap_build) {
#ifdef CONFIG_HOSTAPD
					if (!nvram_match("hapd_enable", "0")) {

						/* No need to enable interface here, hostapd
						 * should take care of enabling the intreface.
						 * Disable roaming in firmware.
						 */
						int val = 1;
						WL_IOVAR_SET(name, "roam_off", &val, sizeof(val));
					} else
#endif	/* CONFIG_HOSTAPD */
					{
#ifdef BCM_WBD
						/* is_bssid_join is true, then join the STA */
						if (is_bssid_join) {
							wlconf_wbd_join_sta(name, prefix, &bssid);
						} else
#endif /* BCM_WBD */
						{
							WL_IOVAR_SET(name, "bss", &setbuf,
								sizeof(setbuf));
							if (ure_mbss_enab &&
								(i == IS_FIRST_WET_BSSIDX)) {
								/* Make sure primary sta can join
								 * successful
								 */
								sleep_ms(2000);
							}
						}
					}
				}
				else {
					strcat_r(prefix, "ssid", tmp);
					ssid.SSID_len = strlen(nvram_safe_get(tmp));
					if (ssid.SSID_len > sizeof(ssid.SSID))
						ssid.SSID_len = sizeof(ssid.SSID);
					strncpy((char *)ssid.SSID, nvram_safe_get(tmp),
						ssid.SSID_len);
					WL_IOCTL(name, WLC_SET_SSID, &ssid, sizeof(ssid));
				}
				if (apsta && (ret != 0))
					sleep_ms(1000);
				else
					break;
			}
		}

	}
	if ((ap || apsta || sta) && (trf_mgmt_cap)) {
		if (trf_mgmt_dwm_cap && ap)
			dwm_supported = TRUE;
		trf_mgmt_settings(prefix, dwm_supported);
	}

	if (ap)
	{
		val = atoi(nvram_safe_get(strcat_r(prefix, "bcnprs_txpwr_offset", tmp)));
		WL_IOVAR_SETINT(name, "bcnprs_txpwr_offset", val);
	}

#ifdef TRAFFIC_MGMT_RSSI_POLICY
	if ((ap || apsta) && (trf_mgmt_cap)) {
		trf_mgmt_rssi_policy(prefix);
	}
#endif /* TRAFFIC_MGMT_RSSI_POLICY */

#ifdef WL_PROXDETECT
	/* proxd ftm configuration */
	if (ap || apsta) {
		uint32 flags = 0;
		char lci_str[WLC_IOCTL_SMLEN*2];
		char civic_str[WLC_IOCTL_SMLEN*2];
		char ftm_flags[WLC_IOCTL_SMLEN];
		char ftm_mode[WLC_IOCTL_SMLEN];

		char *ftm_flagsp = NULL;
		char *ftm_modep = NULL;
		char *lcip = NULL;
		char *civicp = NULL;

		memset(lci_str, 0, sizeof(lci_str));
		memset(civic_str, 0, sizeof(civic_str));
		memset(ftm_flags, 0, sizeof(ftm_flags));
		memset(ftm_mode, 0, sizeof(ftm_mode));

		ftm_flagsp = nvram_get(strcat_r(prefix, "ftm_flags", ftm_flags));
		ftm_modep = nvram_get(strcat_r(prefix, "ftm_mode", ftm_mode));
		lcip = nvram_get(strcat_r(prefix, "ftm_lci", lci_str));
		civicp = nvram_get(strcat_r(prefix, "ftm_civic", civic_str));

		/* set ftm enable/disable */
		if (ftm_modep) {
			val = atoi(ftm_modep);
		}
		else {
			val = 0;
		}

		/* set ftm config flags */
		if (ftm_flagsp) {
			flags = (uint32)strtoul(ftm_flagsp, NULL, 16);
		}
		else {
			flags = 0;
		}

		for (i = 0; i < bclist->count; i++) {
			bsscfg = &bclist->bsscfgs[i];
			if (wlconf_ftm_enable(name, bsscfg, val) == BCME_OK) {
				wlconf_ftm_flags(name, bsscfg, flags);
			}

			if (lcip != NULL) {
				wlconf_rrm_self_lci_civic(name, bsscfg,
					WL_RRM_CONFIG_SET_LCI, lcip);
			}
			if (civicp != NULL) {
				wlconf_rrm_self_lci_civic(name, bsscfg,
					WL_RRM_CONFIG_SET_CIVIC, civicp);
			}
		}
	}
#endif /* WL_PROXDETECT */

#ifdef __CONFIG_EMF__
#ifdef __CONFIG_DHDAP__
	/* WMF will be managed in DHD for FDAP */
	/* Check if interface uses dhd adapter */
	is_dhd = !dhd_probe(name);

	if (is_dhd) {
		/* WMF is supported only in AP mode */
		str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
		wmf_bss_disab = strcmp(str, "ap");

		if (!wmf_bss_disab &&
			(nvram_match(strcat_r(prefix, "wmf_bss_enable", tmp), "1"))) {
			val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_ucigmp_query", tmp)));
			(void)dhd_iovar_setint(name, "wmf_ucast_igmp_query", val);
			val = atoi(nvram_safe_get(strcat_r(prefix, "wmf_ucast_upnp", tmp)));
			(void)dhd_iovar_setint(name, "wmf_ucast_upnp", val);
		}
	}
#endif /* __CONFIG_DHDAP__ */

	for (i = 0; i < bclist->count; i++) {
		bsscfg = &bclist->bsscfgs[i];
		/* WMF is supported only in AP mode */
		str = nvram_safe_get(strcat_r(bsscfg->prefix, "mode", tmp));
		wmf_bss_disab = strcmp(str, "ap");

		if (!wmf_bss_disab &&
			nvram_match(strcat_r(bsscfg->prefix, "wmf_bss_enable", tmp), "1")) {
#ifdef __CONFIG_DHDAP__
			if (is_dhd) {
				val = atoi(nvram_safe_get(strcat_r(prefix,
						"wmf_mdata_sendup", tmp)));
				DHD_BSSIOVAR_SETINT(name, "wmf_mcast_data_sendup",
					bsscfg->idx, val);
			} else
#endif /* __CONFIG_DHDAP__ */
			{
				val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
						"wmf_ucigmp_query", tmp)));
				WL_BSSIOVAR_SETINT(name, "wmf_ucast_igmp_query", bsscfg->idx, val);
				val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
						"wmf_mdata_sendup", tmp)));
				WL_BSSIOVAR_SETINT(name, "wmf_mcast_data_sendup", bsscfg->idx, val);
				val = atoi(nvram_safe_get(strcat_r(bsscfg->prefix,
						"wmf_ucast_upnp", tmp)));
				WL_BSSIOVAR_SETINT(name, "wmf_ucast_upnp", bsscfg->idx, val);
			}
		}
	}
#endif /* __CONFIG_EMF__ */

#ifdef WLHOSTFBT
	/* Set FBT */
	wlconf_set_fbt(bclist);
#endif /* WLHOSTFBT */

#ifdef __CONFIG_DHDAP__
#ifdef __CONFIG_LBR_AGGR__
	/* Check if interface uses dhd adapter */
	is_dhd = !dhd_probe(name);

	if (is_dhd) {
		val = atoi(nvram_safe_get(strcat_r(prefix, "lbr_aggr_en_mask", tmp)));
		(void)dhd_iovar_setint(name, "lbr_aggr_en_mask", val);
		val = atoi(nvram_safe_get(strcat_r(prefix, "lbr_aggr_release_timeout", tmp)));
		if (val) {
			(void)dhd_iovar_setint(name, "lbr_aggr_release_timeout", val);
		}
		val = atoi(nvram_safe_get(strcat_r(prefix, "lbr_aggr_len", tmp)));
		if (val) {
			(void)dhd_iovar_setint(name, "lbr_aggr_len", val);
		}
	}
#endif /* __CONFIG_LBR_AGGR__ */
#endif /* __CONFIG_DHDAP__ */

	if (bclist != NULL)
		free(bclist);

	return ret;
}

#if defined(linux)
int
wlconf_security(char *name)
{
	int unit, bsscfg_idx;
	char prefix[PREFIX_LEN];
	char tmp[100], *str;

	/* Get the interface subunit */
	if (get_ifname_unit(name, &unit, &bsscfg_idx) != 0) {
		WLCONF_DBG("wlconfig(%s): unable to parse unit.subunit in interface "
		           "name \"%s\"\n", name, name);
		return -1;
	}

	if (bsscfg_idx == -1)
		bsscfg_idx = 0;

	/* Configure security parameters for the newly created interface */
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	str = nvram_safe_get(strcat_r(prefix, "mode", tmp));
	wlconf_security_options(name, prefix, bsscfg_idx, FALSE,
	                        !strcmp(str, "psta") || !strcmp(str, "psr"));

	return 0;
}

int
main(int argc, char *argv[])
{
	/* Check parameters and branch based on action */
	if (argc == 3 && !strcmp(argv[2], "up"))
		return wlconf(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "down"))
		return wlconf_down(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "start"))
	  return wlconf_start(argv[1]);
	else if (argc == 3 && !strcmp(argv[2], "security"))
	  return wlconf_security(argv[1]);
	else {
		fprintf(stderr, "Usage: wlconf <ifname> up|down\n");
		return -1;
	}
}
#endif /*  defined(linux) */
