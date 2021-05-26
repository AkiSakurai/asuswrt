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
 * $Id: wldev_common.c,v 1.1.4.1.2.14 2011-02-09 01:40:07 $
 */

#include <osl.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>

#include <wldev_common.h>
#include <bcmutils.h>

#if defined(IL_BIGENDIAN)
#include <bcmendian.h>
#define htod32(i) (bcmswap32(i))
#define htod16(i) (bcmswap16(i))
#define dtoh32(i) (bcmswap32(i))
#define dtoh16(i) (bcmswap16(i))
#define htodchanspec(i) htod16(i)
#define dtohchanspec(i) dtoh16(i)
#else
#define htod32(i) i
#define htod16(i) i
#define dtoh32(i) i
#define dtoh16(i) i
#define htodchanspec(i) i
#define dtohchanspec(i) i
#endif // endif

#define WLDEV_DBG_NONE    (0)
#define WLDEV_DBG_ERROR   (1 << 0)
#define WLDEV_DBG_INFO    (1 << 1)
#define WLDEV_DBG_DEBUG   (1 << 2)
#define WLDEV_DBG_TRACE   (1 << 3)

#ifdef BCMDEBUG
u32 wldev_dbg_level = WLDEV_DBG_ERROR | WLDEV_DBG_INFO | WLDEV_DBG_DEBUG | WLDEV_DBG_TRACE;
#else
u32 wldev_dbg_level = WLDEV_DBG_ERROR | WLDEV_DBG_INFO;
#endif // endif

#define	WLDEV_ERROR(args) do { \
	if (wldev_dbg_level & WLDEV_DBG_ERROR) { \
		printk(KERN_ERR "WLDEV-ERROR) %s : ", __func__); \
		printk args; \
	} \
} while (0)

#define	WLDEV_INFO(args) do { \
	if (wldev_dbg_level & WLDEV_DBG_INFO) { \
		printk(KERN_INFO "WLDEV-INFO) %s : ", __func__); \
		printk args; \
	} \
} while (0)

#define	WLDEV_DEBUG(args) do { \
	if (wldev_dbg_level & WLDEV_DBG_DEBUG) { \
		printk(KERN_INFO "WLDEV-DEBUG) %s : ", __func__); \
		printk args; \
	} \
} while (0)

#define	WLDEV_TRACE(args) do { \
	if (wldev_dbg_level & WLDEV_DBG_TRACE) { \
		printk(KERN_INFO "WLDEV-TRACE) %s : ", __func__); \
		printk args; \
	} \
} while (0)

extern int dhd_ioctl_entry_local(struct net_device *net, wl_ioctl_t *ioc, int cmd);

s32 wldev_ioctl(
	struct net_device *dev, u32 cmd, void *arg, u32 len, u32 set)
{
	s32 ret = 0;
	struct wl_ioctl ioc;

	struct ifreq ifr;
	mm_segment_t fs;

	memset(&ioc, 0, sizeof(ioc));
	ioc.cmd = cmd;
	ioc.buf = arg;
	ioc.len = len;
	ioc.set = set;

	strcpy(ifr.ifr_name, dev->name);
	ifr.ifr_data = (caddr_t)&ioc;

	fs = get_fs();
	set_fs(get_ds());
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
	ret = dev->do_ioctl(dev, &ifr, SIOCDEVPRIVATE);
#else
	ret = dev->netdev_ops->ndo_do_ioctl(dev, &ifr, SIOCDEVPRIVATE);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31) */
	set_fs(fs);

	ret = 0;

	return ret;
}

/* Format a iovar buffer, not bsscfg indexed. The bsscfg index will be
 * taken care of in dhd_ioctl_entry. Internal use only, not exposed to
 * wl_iw, wl_cfg80211 and wl_cfgp2p
 */
static s32 wldev_mkiovar(
	s8 *iovar_name, s8 *param, s32 paramlen,
	s8 *iovar_buf, u32 buflen)
{
	s32 iolen = 0;

	iolen = bcm_mkiovar(iovar_name, param, paramlen, iovar_buf, buflen);
	return iolen;
}

s32 wldev_iovar_getbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync)
{
	s32 ret = 0;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}

	WLDEV_TRACE(("dev = 0x%lx, for \"%s\" \n", (unsigned long)dev, iovar_name));

	wldev_mkiovar(iovar_name, param, paramlen, buf, buflen);
	ret = wldev_ioctl(dev, WLC_GET_VAR, buf, buflen, FALSE);
	if (buf_sync)
		mutex_unlock(buf_sync);
	return ret;
}

s32 wldev_iovar_setbuf(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, struct mutex* buf_sync)
{
	s32 ret = 0;
	s32 iovar_len;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}

	WLDEV_TRACE(("dev = 0x%lx, for \"%s\" \n", (unsigned long)dev, iovar_name));

	iovar_len = wldev_mkiovar(iovar_name, param, paramlen, buf, buflen);
	if (iovar_len > 0)
		ret = wldev_ioctl(dev, WLC_SET_VAR, buf, iovar_len, TRUE);
	else
		ret = BCME_BUFTOOSHORT;

	if (buf_sync)
		mutex_unlock(buf_sync);
	return ret;
}

s32 wldev_iovar_setint(
	struct net_device *dev, s8 *iovar, s32 val)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];

	val = htod32(val);
	memset(iovar_buf, 0, sizeof(iovar_buf));
	return wldev_iovar_setbuf(dev, iovar, &val, sizeof(val), iovar_buf,
		sizeof(iovar_buf), NULL);
}

s32 wldev_iovar_getint(
	struct net_device *dev, s8 *iovar, s32 *pval)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];
	s32 err;

	memset(iovar_buf, 0, sizeof(iovar_buf));
	err = wldev_iovar_getbuf(dev, iovar, pval, sizeof(*pval), iovar_buf,
		sizeof(iovar_buf), NULL);
	if (err == 0)
	{
		memcpy(pval, iovar_buf, sizeof(*pval));
		*pval = dtoh32(*pval);
	}
	return err;
}

/** Format a bsscfg indexed iovar buffer. The bsscfg index will be
 *  taken care of in dhd_ioctl_entry. Internal use only, not exposed to
 *  wl_iw, wl_cfg80211 and wl_cfgp2p
 */
s32 wldev_mkiovar_bsscfg(
	const s8 *iovar_name, s8 *param, s32 paramlen,
	s8 *iovar_buf, s32 buflen, s32 bssidx)
{
	const s8 *prefix = "bsscfg:";
	s8 *p;
	u32 prefixlen;
	u32 namelen;
	u32 iolen;

	if (bssidx == 0) {
		return wldev_mkiovar((s8*)iovar_name, (s8 *)param, paramlen,
			(s8 *) iovar_buf, buflen);
	}

	prefixlen = (u32) strlen(prefix); /* lengh of bsscfg prefix */
	namelen = (u32) strlen(iovar_name) + 1; /* lengh of iovar  name + null */
	iolen = prefixlen + namelen + sizeof(u32) + paramlen;

	if (buflen < 0 || iolen > (u32)buflen)
	{
		WLDEV_ERROR(("%s: buffer is too short\n", __FUNCTION__));
		return BCME_BUFTOOSHORT;
	}

	p = (s8 *)iovar_buf;

	/* copy prefix, no null */
	memcpy(p, prefix, prefixlen);
	p += prefixlen;

	/* copy iovar name including null */
	memcpy(p, iovar_name, namelen);
	p += namelen;

	/* bss config index as first param */
	bssidx = htod32(bssidx);
	memcpy(p, &bssidx, sizeof(u32));
	p += sizeof(u32);

	/* parameter buffer follows */
	if (paramlen)
		memcpy(p, param, paramlen);

	return iolen;

}

s32 wldev_iovar_getbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync)
{
	s32 ret = 0;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}

	WLDEV_TRACE(("bsscfg_idx = %d, for \"%s\" \n", bsscfg_idx, iovar_name));

	wldev_mkiovar_bsscfg(iovar_name, param, paramlen, buf, buflen, bsscfg_idx);
	ret = wldev_ioctl(dev, WLC_GET_VAR, buf, buflen, FALSE);
	if (buf_sync) {
		mutex_unlock(buf_sync);
	}
	return ret;

}

s32 wldev_iovar_setbuf_bsscfg(
	struct net_device *dev, s8 *iovar_name,
	void *param, s32 paramlen, void *buf, s32 buflen, s32 bsscfg_idx, struct mutex* buf_sync)
{
	s32 ret = 0;
	s32 iovar_len;
	if (buf_sync) {
		mutex_lock(buf_sync);
	}

	WLDEV_TRACE(("bsscfg_idx = %d, for \"%s\", param = 0x%lx, paramlen = %d \n",
		bsscfg_idx, iovar_name, (unsigned long)param, paramlen));

	iovar_len = wldev_mkiovar_bsscfg(iovar_name, param, paramlen, buf, buflen, bsscfg_idx);
	if (iovar_len > 0)
		ret = wldev_ioctl(dev, WLC_SET_VAR, buf, iovar_len, TRUE);
	else {
		ret = BCME_BUFTOOSHORT;
	}

	if (buf_sync) {
		mutex_unlock(buf_sync);
	}
	return ret;
}

s32 wldev_iovar_setint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 val, s32 bssidx)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];

	val = htod32(val);
	memset(iovar_buf, 0, sizeof(iovar_buf));
	return wldev_iovar_setbuf_bsscfg(dev, iovar, &val, sizeof(val), iovar_buf,
		sizeof(iovar_buf), bssidx, NULL);
}

s32 wldev_iovar_getint_bsscfg(
	struct net_device *dev, s8 *iovar, s32 *pval, s32 bssidx)
{
	s8 iovar_buf[WLC_IOCTL_SMLEN];
	s32 err;

	memset(iovar_buf, 0, sizeof(iovar_buf));
	err = wldev_iovar_getbuf_bsscfg(dev, iovar, pval, sizeof(*pval), iovar_buf,
		sizeof(iovar_buf), bssidx, NULL);
	if (err == 0)
	{
		memcpy(pval, iovar_buf, sizeof(*pval));
		*pval = dtoh32(*pval);
	}
	return err;
}

int wldev_get_link_speed(
	struct net_device *dev, int *plink_speed)
{
	int error;

	if (!plink_speed)
		return -ENOMEM;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_GET_RATE \n", (unsigned long)dev));
	error = wldev_ioctl(dev, WLC_GET_RATE, plink_speed, sizeof(int), 0);
	if (unlikely(error))
		return error;

	/* Convert internal 500Kbps to Kbps */
	*plink_speed *= 500;
	return error;
}

int wldev_get_rssi(
	struct net_device *dev, int *prssi)
{
	scb_val_t scb_val;
	int error;

	if (!prssi)
		return -ENOMEM;
	bzero(&scb_val, sizeof(scb_val_t));

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_GET_RSSI \n", (unsigned long)dev));
	error = wldev_ioctl(dev, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t), 0);
	if (unlikely(error))
		return error;

	*prssi = dtoh32(scb_val.val);
	return error;
}

int wldev_get_ssid(
	struct net_device *dev, wlc_ssid_t *pssid)
{
	int error;

	if (!pssid)
		return -ENOMEM;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_GET_SSID \n",
		(unsigned long)dev));
	error = wldev_ioctl(dev, WLC_GET_SSID, pssid, sizeof(wlc_ssid_t), 0);
	if (unlikely(error))
		return error;
	pssid->SSID_len = dtoh32(pssid->SSID_len);
	return error;
}

int wldev_get_band(
	struct net_device *dev, uint *pband)
{
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_GET_BAND \n", (unsigned long)dev));
	error = wldev_ioctl(dev, WLC_GET_BAND, pband, sizeof(uint), 0);
	return error;
}

int wldev_set_band(
	struct net_device *dev, uint band)
{
	int error = -1;

	if ((band == WLC_BAND_AUTO) || (band == WLC_BAND_5G) || (band == WLC_BAND_2G)) {
		WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_SET_BAND \n",
			(unsigned long)dev));
		error = wldev_ioctl(dev, WLC_SET_BAND, &band, sizeof(band), true);
	}
	return error;
}

int wldev_set_country(
	struct net_device *dev, char *country_code, bool notify, bool user_enforced)
{
	return 0;
}

#if defined(WLC_HIGH_ONLY)
int wldev_set_country_rev(
	struct net_device *dev, char *command, int total_len)
{
	int error = -1;
	wl_country_t cspec = {{0}, 0, {0}};
	char country_code[WLC_CNTRY_BUF_SZ];
	char smbuf[WLC_IOCTL_SMLEN];
	int rev = 0;

	memset(country_code, 0, sizeof(country_code));
	sscanf(command+sizeof(CMD_COUNTRYREV_SET), "%10s %10d", country_code, &rev);
	WLDEV_TRACE(("country_code = %s, rev = %d\n", country_code, rev));

	memcpy(cspec.country_abbrev, country_code, sizeof(country_code));
	memcpy(cspec.ccode, country_code, sizeof(country_code));
	cspec.rev = rev;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_setbuf()' for \"country\" \n",
		(unsigned long)dev));
	error = wldev_iovar_setbuf(dev, "country", (char *)&cspec,
		sizeof(cspec), smbuf, sizeof(smbuf), NULL);

	if (error) {
		WLDEV_ERROR(("%s: set country '%s/%d' failed code %d\n",
			__FUNCTION__, cspec.ccode, cspec.rev, error));
	} else {
		/* dhd_bus_country_set(dev, &cspec, true); */
		WLDEV_INFO(("%s: set country '%s/%d'\n",
			__FUNCTION__, cspec.ccode, cspec.rev));
	}

	return error;
}

int wldev_get_country_rev(
	struct net_device *dev, char *command, int total_len)
{
	int error;
	int bytes_written;
	char smbuf[WLC_IOCTL_SMLEN];
	wl_country_t cspec;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getbuf()' for \"country\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getbuf(dev, "country", NULL, 0, smbuf,
		sizeof(smbuf), NULL);

	if (error) {
		WLDEV_ERROR(("%s: get country failed code %d\n",
			__FUNCTION__, error));
		return -1;
	} else {
		memcpy(&cspec, smbuf, sizeof(cspec));
		WLDEV_INFO(("%s: get country '%c%c %d'\n",
			__FUNCTION__, cspec.ccode[0], cspec.ccode[1], cspec.rev));
	}

	bytes_written = snprintf(command, total_len, "%s %c%c %d",
		CMD_COUNTRYREV_GET, cspec.ccode[0], cspec.ccode[1], cspec.rev);

	return bytes_written;
}

int wldev_set_mchan_sched_mode(
	struct net_device *dev, uint mode)
{
	int error = 0;

	if (mode >= 3) {
		WLDEV_ERROR(("%s: mode is invalid value\n", __FUNCTION__));
		return -1;
	}
	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_setint()' for \"country\" \n",
		(unsigned long)dev));
	error = wldev_iovar_setint(dev, "mchan_sched_mode", mode);
	if (error < 0) {
		WLDEV_ERROR(("%s: set mchan_shced_mode %d failed\n",
			__FUNCTION__, mode));
		return -1;
	}
	return error;
}

int wldev_get_mchan_sched_mode(
	struct net_device *dev, char *command, int total_len)
{
	int mode;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' for \"mchan_sched_mode\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "mchan_sched_mode", &mode);
	if (error < 0) {
		WLDEV_ERROR(("%s: get mchan_shced_mode failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_MCHAN_SCHED_MODE_GET, mode);

	return bytes_written;
}

int wldev_set_wowl(
	struct net_device *dev, int32 mode)
{
	int error = 0;

	if ((mode & ~(WL_WOWL_MAGIC | WL_WOWL_NET | WL_WOWL_DIS |
		WL_WOWL_GTK_FAILURE | WL_WOWL_RETR | WL_WOWL_BCN | WL_WOWL_M1 |
		WL_WOWL_SCANOL | WL_WOWL_EAPID | WL_WOWL_ARPOFFLOAD |
		WL_WOWL_TCPKEEP | WL_WOWL_MDNS_CONFLICT |
		WL_WOWL_MDNS_SERVICE | WL_WOWL_FW_HALT)) != 0) {
		return -1;
	}
	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_setint()' for \"wowl\" \n",
		(unsigned long)dev));
	error = wldev_iovar_setint(dev, "wowl", mode);
	if (error < 0) {
		WLDEV_ERROR(("%s: set wowl %d failed\n",
			__FUNCTION__, mode));
		return -1;
	}
	return error;
}

int wldev_get_wowl(
	struct net_device *dev, char *command, int total_len)
{
	int32 mode;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' for \"wowl\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "wowl", &mode);
	if (error < 0) {
		WLDEV_ERROR(("%s: get wowl failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_WOWL_GET, mode);

	return bytes_written;
}

int wldev_set_wowl_bcn_loss(
	struct net_device *dev, int32 sec)
{
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_setint()' for \"wowl_bcn_loss\" \n",
		(unsigned long)dev));
	error = wldev_iovar_setint(dev, "wowl_bcn_loss", sec);
	if (error < 0) {
		WLDEV_ERROR(("%s: set wowl_bcn_loss %d failed\n",
			__FUNCTION__, sec));
		return -1;
	}
	return error;
}

int wldev_get_wowl_bcn_loss(
	struct net_device *dev, char *command, int total_len)
{
	int32 sec;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' for \"wowl_bcn_loss\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "wowl_bcn_loss", &sec);
	if (error < 0) {
		WLDEV_ERROR(("%s: get wowl_bcn_loss failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_WOWL_BCN_LOSS_GET, sec);

	return bytes_written;
}

int wldev_set_wowl_keyrot(
	struct net_device *dev, int32 val)
{
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_setint()' for \"wowl_keyrot\" \n",
		(unsigned long)dev));
	error = wldev_iovar_setint(dev, "wowl_keyrot", val);
	if (error < 0) {
		WLDEV_ERROR(("%s: set wowl_keyrot %d failed\n",
			__FUNCTION__, val));
		return -1;
	}
	return error;
}

int wldev_get_wowl_keyrot(
	struct net_device *dev, char *command, int total_len)
{
	int32 val;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' for \"wowl_keyrot\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "wowl_keyrot", &val);
	if (error < 0) {
		WLDEV_ERROR(("%s: get wowl_keyrot failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_WOWL_KEYROT_GET, val);

	return bytes_written;
}

/* Convert user's input in hex pattern to byte-size mask */
static int
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
		dst[i] = (uint8)bcm_strtoul(num, NULL, 16);
		src += 2;
	}
	return i;
}

int wldev_set_wowl_pattern(
	struct net_device *dev, char *command, int total_len)
{
	int error = -1;
	char *cmdbuf = NULL;
	int buf_len = 0;
	char *dst = NULL;
	const char *str;
	char *cmd_dst;
	wl_wowl_pattern_t *wl_pattern;
	char *next_sp;

	cmdbuf = kmalloc(WLC_IOCTL_MEDLEN, GFP_KERNEL);
	if (!cmdbuf) {
		WLDEV_ERROR(("%s: failed to allocate memory\n",
		__FUNCTION__));
		error = -ENOMEM;
		goto error;
	}
	bzero(cmdbuf, WLC_IOCTL_MEDLEN);
	dst = cmdbuf;
	cmd_dst = (char *)(command + sizeof(CMD_WOWL_PATTERN_SET));

	str = "wowl_pattern";
	strncpy(dst, str, strlen(str));
	dst[strlen(str)] = '\0';
	dst += strlen(str) + 1;
	buf_len += strlen(str) + 1;

	if (strncmp(cmd_dst, "add", 3) != 0 && strncmp(cmd_dst, "del", 3) != 0 &&
		strncmp(cmd_dst, "clr", 3) != 0) {
		WLDEV_ERROR(("%s: cmd not specified\n", __func__));
		goto error;
	}

	str = cmd_dst;
	strncpy(dst, str, 3);
	dst[3] = '\0';
	buf_len += 3 + 1;

	if (strncmp(dst, "clr", 3) != 0) {
		/* parse the offset */
		cmd_dst += 3 + 1;

		wl_pattern = (wl_wowl_pattern_t *)(dst + 3 + 1);
		dst = (char *)wl_pattern + sizeof(wl_wowl_pattern_t);
		wl_pattern->offset = htod32(bcm_strtoul(cmd_dst, NULL, 0));

		/* parse the mask */
		while (!bcm_isspace(*cmd_dst))
			cmd_dst++;
		cmd_dst++;

		next_sp = cmd_dst;

		/* replace the space with null char */
		while (!bcm_isspace(*next_sp))
			next_sp++;
		*next_sp = '\0';
		next_sp++;

		wl_pattern->masksize = htod32(wl_pattern_atoh((char *)(uintptr)cmd_dst, dst));
		if (wl_pattern->masksize == (uint) -1)
			goto error;

		cmd_dst = next_sp;
		dst += wl_pattern->masksize;
		wl_pattern->patternoffset = htod32((sizeof(wl_wowl_pattern_t) +
			wl_pattern->masksize));

		/* parse the value */
		wl_pattern->patternsize =
			htod32(wl_pattern_atoh((char *)(uintptr)cmd_dst, dst));
		if (wl_pattern->patternsize == (uint)-1)
			goto error;
		buf_len += sizeof(wl_wowl_pattern_t) + wl_pattern->patternsize +
			wl_pattern->masksize;
	}

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_ioctl()' for WLC_SET_VAR -> \"wowl_pattern\" \n",
		(unsigned long)dev));
	error = wldev_ioctl(dev, WLC_SET_VAR, cmdbuf, buf_len, TRUE);

	if (error) {
		WLDEV_ERROR(("%s: set wowl_pattern failed code %d\n",
			__FUNCTION__, error));
	} else {
		/* dhd_bus_country_set(dev, &cspec, true); */
		WLDEV_INFO(("%s: set wowl_pattern\n", __FUNCTION__));
	}

error:
	if (cmdbuf)
		kfree(cmdbuf);

	return error;
}

int wldev_wowl_activate(
	struct net_device *dev, char *command, int total_len)
{
	int32 val;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' \"wowl_activate\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "wowl_activate", &val);
	if (error < 0) {
		WLDEV_ERROR(("%s: get wowl_activate failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_WOWL_ACTIVATE, val);

	return bytes_written;
}

int wldev_wowl_status(
	struct net_device *dev, char *command, int total_len)
{
	int32 val;
	int bytes_written;
	int error;

	WLDEV_TRACE(("dev = 0x%lx, Call 'wldev_iovar_getint()' \"wowl_status\" \n",
		(unsigned long)dev));
	error = wldev_iovar_getint(dev, "wowl_status", &val);
	if (error < 0) {
		WLDEV_ERROR(("%s: get wowl_status failed\n", __FUNCTION__));
		return -1;
	}

	bytes_written = snprintf(command, total_len, "%s %d",
		CMD_WOWL_STATUS, val);

	return bytes_written;
}
#endif /* #if defined(WLC_HIGH_ONLY) */
