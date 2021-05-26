/*
 * Common function shared by Linux WEXT, cfg80211 and p2p drivers
 *
 * Copyright (C) 2020, Broadcom. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wldev_common.h,v 1.1.4.1.2.14 2011-02-09 01:40:07 $
 */
#ifndef __WLDEV_COMMON_H__
#define __WLDEV_COMMON_H__

#include <wlioctl.h>

/* XXX: For BMAC drivers, define WLC_HIGH_ONLY here. Including wlc_cfg.h here
 *      breaks several other brands (especially -dhd- specific brands)
 */
#if !defined(WLC_HIGH_ONLY)
#if defined(WLC_HIGH) && !defined(WLC_LOW)
#define WLC_HIGH_ONLY
#endif // endif
#endif /* #if !defined(WLC_HIGH_ONLY) */

#if !defined(OEM_ANDROID)
/* Wake lock are used in Android only, which is dongle based as of now */
#define DHD_OS_WAKE_LOCK(pub)
#define DHD_OS_WAKE_UNLOCK(pub)
#define DHD_OS_WAKE_LOCK_TIMEOUT(pub)
#endif /* #if !defined(OEM_ANDROID) */

#if defined(WLC_HIGH_ONLY)
#define CMD_START				"START"
#define CMD_STOP				"STOP"
#define	CMD_SCAN_ACTIVE			"SCAN-ACTIVE"
#define	CMD_SCAN_PASSIVE		"SCAN-PASSIVE"
#define CMD_RSSI				"RSSI"
#define CMD_LINKSPEED			"LINKSPEED"
#define CMD_RXFILTER_START		"RXFILTER-START"
#define CMD_RXFILTER_STOP		"RXFILTER-STOP"
#define CMD_RXFILTER_ADD		"RXFILTER-ADD"
#define CMD_RXFILTER_REMOVE		"RXFILTER-REMOVE"
#define CMD_BTCOEXSCAN_START	"BTCOEXSCAN-START"
#define CMD_BTCOEXSCAN_STOP		"BTCOEXSCAN-STOP"
#define CMD_BTCOEXMODE			"BTCOEXMODE"
#define CMD_SETSUSPENDOPT		"SETSUSPENDOPT"
#define CMD_P2P_DEV_ADDR		"P2P_DEV_ADDR"
#define CMD_SETFWPATH			"SETFWPATH"
#define CMD_SETBAND				"SETBAND"
#define CMD_GETBAND				"GETBAND"
#define CMD_COUNTRY				"COUNTRY"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_SET_AP_WPS_P2P_IE	"SET_AP_WPS_P2P_IE"
#define CMD_COUNTRYREV_SET		"SETCOUNTRYREV"
#define CMD_COUNTRYREV_GET		"GETCOUNTRYREV"
#define CMD_MCHAN_SCHED_MODE_SET	"SET_MCHAN_SCHED_MODE"
#define CMD_MCHAN_SCHED_MODE_GET	"GET_MCHAN_SCHED_MODE"
#define CMD_WOWL_BCN_LOSS_SET		"SET_WOWL_BCN_LOSS"
#define CMD_WOWL_BCN_LOSS_GET		"GET_WOWL_BCN_LOSS"
#define CMD_WOWL_KEYROT_SET		"SET_WOWL_KEYROT"
#define CMD_WOWL_KEYROT_GET		"GET_WOWL_KEYROT"
#define CMD_WOWL_PATTERN_SET	"SET_WOWL_PATTERN"
#define CMD_WOWL_ACTIVATE		"WOWL_ACTIVATE"
#define CMD_WOWL_STATUS			"WOWL_STATUS"
#define CMD_WOWL_SET			"SET_WOWL"
#define CMD_WOWL_GET			"GET_WOWL"
#endif  /* #if defined(WLC_HIGH_ONLY) */

/* wl_dev_ioctl - get/set IOCTLs, will call net_device's do_ioctl (or
 *  netdev_ops->ndo_do_ioctl in new kernels)
 *  @dev: the net_device handle
 */
s32 wldev_ioctl(
	struct net_device *dev, u32 cmd, void *arg, u32 len, u32 set);

/** Retrieve named IOVARs, this function calls wl_dev_ioctl with
 *  WLC_GET_VAR IOCTL code
 */
s32 wldev_iovar_getbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync);

/** Set named IOVARs, this function calls wl_dev_ioctl with
 *  WLC_SET_VAR IOCTL code
 */
s32 wldev_iovar_setbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync);

s32 wldev_iovar_setint(
	struct net_device *dev, s8 *iovar, s32 val);

s32 wldev_iovar_getint(
	struct net_device *dev, s8 *iovar, s32 *pval);

/** The following function can be implemented if there is a need for bsscfg
 *  indexed IOVARs
 */

s32 wldev_mkiovar_bsscfg(
	const s8 *iovar_name, s8 *param, s32 paramlen,
	s8 *iovar_buf, s32 buflen, s32 bssidx);

/** Retrieve named and bsscfg indexed IOVARs, this function calls wl_dev_ioctl with
 *  WLC_GET_VAR IOCTL code
 */
s32 wldev_iovar_getbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name, void *param, s32 paramlen,
	void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync);

/** Set named and bsscfg indexed IOVARs, this function calls wl_dev_ioctl with
 *  WLC_SET_VAR IOCTL code
 */
s32 wldev_iovar_setbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name, void *param, s32 paramlen,
	void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync);

s32 wldev_iovar_getint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 *pval, s32 bssidx);

s32 wldev_iovar_setint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 val, s32 bssidx);

extern int dhd_net_set_fw_path(struct net_device *dev, char *fw);
extern int dhd_net_bus_suspend(struct net_device *dev);
extern int dhd_net_bus_resume(struct net_device *dev, uint8 stage);
extern int dhd_net_wifi_platform_set_power(struct net_device *dev, bool on,
	unsigned long delay_msec);
extern void dhd_get_customized_country_code(struct net_device *dev, char *country_iso_code,
	wl_country_t *cspec);
extern void dhd_bus_country_set(struct net_device *dev, wl_country_t *cspec, bool notify);
extern void dhd_bus_band_set(struct net_device *dev, uint band);
extern int wldev_set_country(struct net_device *dev, char *country_code, bool notify,
	bool user_enforced);
extern int net_os_wake_lock(struct net_device *dev);
extern int net_os_wake_unlock(struct net_device *dev);
extern int net_os_wake_lock_timeout(struct net_device *dev);
extern int net_os_wake_lock_timeout_enable(struct net_device *dev, int val);
extern int net_os_set_dtim_skip(struct net_device *dev, int val);
extern int net_os_set_suspend_disable(struct net_device *dev, int val);
extern int net_os_set_suspend(struct net_device *dev, int val, int force);
extern int wl_iw_parse_ssid_list_tlv(char** list_str, wlc_ssid_t* ssid,
	int max, int *bytes_left);

/* Get the link speed from dongle, speed is in kpbs */
int wldev_get_link_speed(struct net_device *dev, int *plink_speed);

int wldev_get_rssi(struct net_device *dev, int *prssi);

int wldev_get_ssid(struct net_device *dev, wlc_ssid_t *pssid);

int wldev_get_band(struct net_device *dev, uint *pband);

int wldev_set_band(struct net_device *dev, uint band);

#if defined(WLC_HIGH_ONLY)
int wldev_set_country_rev(struct net_device *dev, char *command, int total_len);
int wldev_get_country_rev(struct net_device *dev, char *command, int total_len);
int wldev_set_mchan_sched_mode(struct net_device *dev, uint mode);
int wldev_get_mchan_sched_mode(struct net_device *dev, char *command, int total_len);
int wldev_set_wowl_bcn_loss(struct net_device *dev, int32 sec);
int wldev_get_wowl_bcn_loss(struct net_device *dev, char *command, int total_len);
int wldev_set_wowl_keyrot(struct net_device *dev, int32 val);
int wldev_get_wowl_keyrot(struct net_device *dev, char *command, int total_len);
int wldev_set_wowl_pattern(struct net_device *dev, char *command, int total_len);
int wldev_wowl_activate(struct net_device *dev, char *command, int total_len);
int wldev_wowl_status(struct net_device *dev, char *command, int total_len);
int wldev_set_wowl(struct net_device *dev, int32 mode);
int wldev_get_wowl(struct net_device *dev, char *command, int total_len);
#endif /* #if defined(WLC_HIGH_ONLY) */

#endif /* __WLDEV_COMMON_H__ */
